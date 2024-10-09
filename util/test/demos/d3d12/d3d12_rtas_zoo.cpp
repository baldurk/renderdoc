/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "d3d12_test.h"

RD_TEST(D3D12_RTAS_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Test of different AS edge-cases and formats.";

  std::string rtshaders = R"EOSHADER(

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> RenderTarget : register(u1);

struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void gen()
{
    float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

    {
        RayDesc ray;
        ray.Origin = float3(0, 0, 5);
        ray.Direction = float3(lerp(-1.0f, 1.0f, lerpValues.x),
                               lerp(-1.0f, 1.0f, lerpValues.y),
                               -1.0f);
        ray.TMin = 0.001;
        ray.TMax = 10000.0;
        RayPayload payload = { float4(0, 0, 1, 1) };
        TraceRay(Scene, RAY_FLAG_NONE, ~0, 0, 0, 0, ray, payload);

        // Write the raytraced color to the output texture.
        RenderTarget[DispatchRaysIndex().xy] = payload.color;
    }
}

[shader("closesthit")]
void chit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attrs)
{
    payload.color = float4(0, 1, 0, 1);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.color = float4(1, 0, 0, 1);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(opts5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
      Avail = "RT is not supported";

    if(!m_DXILSupport)
      Avail = "DXIL can't be compiled";
  }

  static const uint64_t scratchSpace = 1 * 1024 * 1024;

  static const uint64_t blasOffset = 0;
  static const uint64_t blasSize = 1 * 1024 * 1024;
  static const uint64_t tlasOffset = blasOffset + blasSize;
  static const uint64_t tlasSize = 1 * 1024 * 1024;
  static const uint64_t asbSize = tlasOffset + tlasSize;

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D12ResourcePtr asb = MakeBuffer().ASB().Size(asbSize);

    asb->SetName(L"asb");

    ID3D12ResourcePtr uav = MakeBuffer().UAV().Size(scratchSpace);

    {
      ID3D12GraphicsCommandList4Ptr cmd = GetCommandBuffer();

      Reset(cmd);

      D3D12_RAYTRACING_GEOMETRY_DESC geom = {};
      geom.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
      geom.Triangles.VertexBuffer.StartAddress = DefaultTriVB->GetGPUVirtualAddress();
      geom.Triangles.VertexBuffer.StrideInBytes = sizeof(DefaultA2V);
      geom.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
      geom.Triangles.VertexCount = 3;
      geom.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

      D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
      desc.DestAccelerationStructureData = asb->GetGPUVirtualAddress();
      desc.ScratchAccelerationStructureData = uav->GetGPUVirtualAddress();
      desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
      desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
      desc.Inputs.NumDescs = 1;
      desc.Inputs.pGeometryDescs = &geom;

      cmd->BuildRaytracingAccelerationStructure(&desc, 0, NULL);

      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
      dev5->GetRaytracingAccelerationStructurePrebuildInfo(&desc.Inputs, &prebuild);

      TEST_ASSERT(prebuild.ScratchDataSizeInBytes < scratchSpace, "Insufficient scratch space");
      TEST_ASSERT(prebuild.ResultDataMaxSizeInBytes < tlasOffset, "BLAS too large");

      cmd->Close();

      Submit({cmd});
    }

    D3D12_RAYTRACING_INSTANCE_DESC instances[8] = {};
    for(int i = 0; i < 8; i++)
    {
      instances[i].AccelerationStructure = asb->GetGPUVirtualAddress();
      instances[i].InstanceMask = 1;
      instances[i].Transform[0][0] = 1.0f;
      instances[i].Transform[1][1] = 1.0f;
      instances[i].Transform[2][2] = 1.0f;

      instances[i].Transform[0][3] = -4.0f + i * 1.0f;
    }

    ID3D12ResourcePtr instData = MakeBuffer().Size(sizeof(instances)).Upload();

    byte *instUpload = Map(instData, 0);

    instData->SetName(L"instData");

    D3D12_GPU_VIRTUAL_ADDRESS instancesIndirect[8] = {};
    for(int i = 0; i < 8; i++)
      instancesIndirect[i] =
          instData->GetGPUVirtualAddress() + i * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

    ID3D12ResourcePtr instIndirectData = MakeBuffer().Data(instancesIndirect);

    instIndirectData->SetName(L"instIndirectData");

    ID3D12StateObjectPtr rtpso;

    std::vector<D3D12_STATE_SUBOBJECT> subObjs;

    ID3DBlobPtr lib = Compile(rtshaders, "", "lib_6_3");
    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    libDesc.DXILLibrary.BytecodeLength = lib->GetBufferSize();
    libDesc.DXILLibrary.pShaderBytecode = lib->GetBufferPointer();

    ID3D12RootSignaturePtr rootsig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 100, 0),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 100, 0),
    });
    D3D12_GLOBAL_ROOT_SIGNATURE rootSigDesc = {rootsig};

    D3D12_RAYTRACING_SHADER_CONFIG shadDesc = {};
    shadDesc.MaxPayloadSizeInBytes = 16;
    shadDesc.MaxAttributeSizeInBytes = 8;
    D3D12_RAYTRACING_PIPELINE_CONFIG pipeDesc = {};
    pipeDesc.MaxTraceRecursionDepth = 1;
    D3D12_HIT_GROUP_DESC hitgroupDesc = {};
    hitgroupDesc.ClosestHitShaderImport = L"chit";
    hitgroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitgroupDesc.HitGroupExport = L"hitgroup";

    subObjs.push_back({D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &rootSigDesc});
    subObjs.push_back({D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &libDesc});
    subObjs.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shadDesc});
    subObjs.push_back({D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeDesc});
    subObjs.push_back({D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitgroupDesc});

    D3D12_STATE_OBJECT_DESC stateDesc = {};
    stateDesc.NumSubobjects = (UINT)subObjs.size();
    stateDesc.pSubobjects = subObjs.data();
    stateDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

    dev5->CreateStateObject(&stateDesc, __uuidof(ID3D12StateObject), (void **)&rtpso);
    ID3D12StateObjectPropertiesPtr rtpso_props = rtpso;

    std::vector<byte> tablesData;

    tablesData.resize(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT * 3);

    memcpy(tablesData.data(), rtpso_props->GetShaderIdentifier(L"gen"),
           D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    memcpy(tablesData.data() + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT,
           rtpso_props->GetShaderIdentifier(L"miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    memcpy(tablesData.data() + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT * 2,
           rtpso_props->GetShaderIdentifier(L"hitgroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    ID3D12ResourcePtr tables = MakeBuffer().Data(tablesData);

    ID3D12ResourcePtr uavtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .UAV()
                                   .InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    MakeUAV(uavtex).CreateCPU(1);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      ID3D12GraphicsCommandList4Ptr cmd4 = cmd;

      Reset(cmd);

      for(int i = 0; i < 8; i++)
      {
        instances[i].InstanceID = curFrame;
        instances[i].Transform[1][3] = 2.5f * cosf(i * 0.29f + 0.05f * curFrame);
      }
      memcpy(instUpload, instances, sizeof(instances));

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      cmd->SetComputeRootSignature(rootsig);
      cmd4->SetPipelineState1(rtpso);
      cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

      D3D12_DISPATCH_RAYS_DESC rayDispatch = {};
      rayDispatch.Width = screenWidth;
      rayDispatch.Height = screenHeight;
      rayDispatch.Depth = 1;
      rayDispatch.RayGenerationShaderRecord.StartAddress = tables->GetGPUVirtualAddress();
      rayDispatch.RayGenerationShaderRecord.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
      rayDispatch.MissShaderTable.StartAddress =
          tables->GetGPUVirtualAddress() + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
      rayDispatch.MissShaderTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
      rayDispatch.MissShaderTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
      rayDispatch.HitGroupTable.StartAddress =
          tables->GetGPUVirtualAddress() + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT * 2;
      rayDispatch.HitGroupTable.StrideInBytes = 0;
      rayDispatch.HitGroupTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
      cmd4->DispatchRays(&rayDispatch);

      ResourceBarrier(cmd);

      cmd->Close();

      Submit({cmd});

      cmd = GetCommandBuffer();
      cmd4 = cmd;

      Reset(cmd);

      D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
      desc.DestAccelerationStructureData = asb->GetGPUVirtualAddress() + tlasOffset;
      desc.ScratchAccelerationStructureData = uav->GetGPUVirtualAddress();
      desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS;
      desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
      desc.Inputs.NumDescs = 8;
      desc.Inputs.InstanceDescs = instIndirectData->GetGPUVirtualAddress();

      cmd4->BuildRaytracingAccelerationStructure(&desc, 0, NULL);
      MakeAS(asb).Offset(tlasOffset).CreateCPU(0);

      ResourceBarrier(cmd);

      cmd->Close();

      Submit({cmd});

      cmd = GetCommandBuffer();
      cmd4 = cmd;

      Reset(cmd);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      cmd->SetComputeRootSignature(rootsig);
      cmd4->SetPipelineState1(rtpso);
      cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

      cmd4->DispatchRays(&rayDispatch);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      ResourceBarrier(cmd, uavtex, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

      blitToSwap(cmd, uavtex, bb);

      ResourceBarrier(cmd, uavtex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
