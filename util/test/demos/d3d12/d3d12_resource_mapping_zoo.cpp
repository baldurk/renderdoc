/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

RD_TEST(D3D12_Resource_Mapping_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests various resource types and mappings with both Shader Model 5 and 5.1 to ensure"
      "correct parsing and debugging behavior.";

  std::string pixel_5_0 = R"EOSHADER(

Texture2D res1 : register(t0);
Texture2D res2 : register(t2);

// TODO: Add UAV writes and test gaps in those mappings

cbuffer consts : register(b3)
{
  uint4 test;
};

float4 main() : SV_Target0
{
  float4 color = (float4)test + float4(0.1f, 0.0f, 0.0f, 0.0f);
	return color + res1[uint2(0, 0)] + res2[uint2(0, 0)];
}

)EOSHADER";

  std::string pixel_5_1 = R"EOSHADER(

Texture2D res1 : register(t6);
Texture2D res2 : register(t7);

RWTexture2D<float> res1uav : register(u0);

// TODO: Add UAV writes and test gaps in those mappings

cbuffer consts : register(b3)
{
  uint4 test;
};

struct Foo
{
  float4 col;
};
ConstantBuffer<Foo> bar[4][3] : register(b4);

float4 main() : SV_Target0
{
  float4 color = bar[1][2].col;
  color += (float4)test + float4(0.1f, 0.0f, 0.0f, 0.0f);
  float4 uavVal = res1uav[uint2(1, 1)];
  return color + res1[uint2(0, 0)] + res2[uint2(0, 0)] + uavVal;
}

)EOSHADER";

  std::string pixel_resArray = R"EOSHADER(

Texture2DArray<float> resArray[4] : register(t10, space1);

cbuffer consts : register(b3)
{
  uint4 test;
};

float4 main(float4 pos : SV_Position) : SV_Target0
{
  // Test resource array access with a constant, uniform, and non-uniform
  uint2 indices = ((uint2)pos.xy) % uint2(4, 4);
  float arrayVal1 = resArray[1].Load(uint4(0, 0, indices.y, 0));
  float arrayVal2 = resArray[test.x].Load(uint4(0, 0, indices.y, 0));
  float arrayVal3 = resArray[NonUniformResourceIndex(indices.x)].Load(uint4(0, 0, indices.y, 0));
  return float4(arrayVal1, arrayVal2, arrayVal3, 1.0f);
}

)EOSHADER";

  std::string pixel_bindless = R"EOSHADER(

Texture2DArray<float> resArray[] : register(t0);

cbuffer consts : register(b3)
{
  uint4 test;
};

float4 main(float4 pos : SV_Position) : SV_Target0
{
  // Test resource array access with a constant, uniform, and non-uniform
  uint2 indices = ((uint2)pos.xy) % uint2(4, 4);
  float arrayVal1 = resArray[1].Load(uint4(0, 0, indices.y, 0));
  float arrayVal2 = resArray[test.x].Load(uint4(0, 0, indices.y, 0));
  float arrayVal3 = resArray[NonUniformResourceIndex(indices.x)].Load(uint4(0, 0, indices.y, 0));
  return float4(arrayVal1, arrayVal2, arrayVal3, 1.0f);
}

)EOSHADER";

  std::string pixel_resourceAccess = R"EOSHADER(

// SRVs
Texture2D<float> srvAccessed : register(t0);
Texture2D<float> srvNotAccessed : register(t1);
Texture2D<float> srvArray[] : register(t0, space1);

// UAVs
RWTexture2D<float> uavAccessed : register(u0);
RWTexture2D<float> uavNotAccessed : register(u1);

struct Data
{
  uint4 data;
};
ConstantBuffer<Data> cbvAccessed : register(b0);
ConstantBuffer<Data> cbvNotAccessed : register(b1);
ConstantBuffer<Data> cbvArray[8] : register(b0, space1);

float4 main(float4 pos : SV_Position) : SV_Target0
{
  float srvVal = srvAccessed.Load(uint3(0, 0, 0));
  uint cbvVal = cbvAccessed.data.x;
  float srvArrayVal = srvArray[cbvVal].Load(uint3(0, 0, 0));
  float3 cbvArrayVal = (float3)cbvArray[cbvVal].data.yzw;
  float3 ret = cbvArrayVal;
  uavAccessed[uint2(0, 0)] = srvArrayVal / 100.0f;
  ret.x += srvVal;
  ret.y += srvArrayVal;
  return float4(ret, 1.0f);
}

)EOSHADER";

  void UploadTexture(ID3D12ResourcePtr & uploadBuf, ID3D12ResourcePtr & dstTexture, byte * data,
                     uint32_t dataStride)
  {
    // dstTexture is assumed to be in the D3D12_RESOURCE_STATE_COPY_DEST state

    D3D12_RESOURCE_DESC desc = dstTexture->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts =
        new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[desc.DepthOrArraySize];
    dev->GetCopyableFootprints(&desc, 0, desc.DepthOrArraySize, 0, pLayouts, NULL, NULL, NULL);
    ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
    Reset(cmd);

    D3D12_TEXTURE_COPY_LOCATION dst, src;
    src.Type = src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.pResource = uploadBuf;

    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = dstTexture;

    byte *dataptr = data;
    for(UINT i = 0; i < desc.DepthOrArraySize; ++i)
    {
      src.PlacedFootprint = pLayouts[i];
      uint32_t copyStride =
          pLayouts[i].Footprint.RowPitch < dataStride ? pLayouts[i].Footprint.RowPitch : dataStride;
      byte *mapptr = Map(uploadBuf, 0) + pLayouts[i].Offset;
      for(UINT y = 0; y < pLayouts[i].Footprint.Height; ++y)
      {
        memcpy(mapptr, dataptr, copyStride);
        mapptr += pLayouts[i].Footprint.RowPitch;
        dataptr += dataStride;
      }

      dst.SubresourceIndex = i;
      cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
      uploadBuf->Unmap(0, NULL);

      D3D12_RESOURCE_BARRIER b = {};
      b.Transition.pResource = dstTexture;
      b.Transition.Subresource = i;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
      cmd->ResourceBarrier(1, &b);
    }

    cmd->Close();
    Submit({cmd});
    GPUSync();

    delete[] pLayouts;
  }

  // Constant buffer locations must be 256 byte aligned, so that's the smallest size that
  // an entry of a CB array can be.
  struct AlignedCB
  {
    Vec4f col;
    Vec4f padding[15];
  };
  static_assert(sizeof(AlignedCB) == 256, "Invalid alignment for CB data");

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob_5_0 = Compile(pixel_5_0, "main", "ps_5_0");
    ID3DBlobPtr psblob_5_1 = Compile(pixel_5_1, "main", "ps_5_1");
    ID3DBlobPtr psblob_resArray = Compile(pixel_resArray, "main", "ps_5_1");
    ID3DBlobPtr psblob_bindless = Compile(pixel_bindless, "main", "ps_5_1");
    ID3DBlobPtr psblob_resourceAccess = Compile(pixel_resourceAccess, "main", "ps_5_1");

    uint32_t cbufferdata[4] = {3, 50, 75, 100};

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);
    ID3D12ResourcePtr cb = MakeBuffer().Data(cbufferdata);

    // Descriptor table entries:
    // 0-12: CB array
    // 30-33: SRV array
    // 56-57: SRVs containing stepped unorm data

    AlignedCB cbufferarray[4][3];
    for(uint32_t x = 0; x < 4; ++x)
      for(uint32_t y = 0; y < 3; ++y)
        cbufferarray[x][y].col = Vec4f(x / 1.0f, y / 1.0f, 0.5f, 0.5f);
    ID3D12ResourcePtr cbArray = MakeBuffer().Data(cbufferarray).Size(sizeof(AlignedCB) * 12);
    for(uint32_t i = 0; i < 12; ++i)
      MakeCBV(cbArray).SizeBytes(256).Offset(i * sizeof(AlignedCB)).CreateGPU(i);

    ID3D12ResourcePtr res1 = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2)
                                 .Mips(1)
                                 .InitialState(D3D12_RESOURCE_STATE_COPY_DEST)
                                 .UAV();
    MakeSRV(res1).CreateGPU(56);
    ID3D12ResourcePtr res2 =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2).Mips(1).InitialState(D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12ViewCreator srvRes2 = MakeSRV(res2);
    srvRes2.CreateGPU(57);
    MakeUAV(res1).CreateGPU(20);

    // Create a few unused SRVs so that a bindless descriptor table has a lot of things to report
    srvRes2.CreateGPU(500);
    srvRes2.CreateGPU(501);
    srvRes2.CreateGPU(510);
    srvRes2.CreateGPU(625);

    ID3D12ResourcePtr uploadBuf = MakeBuffer().Size(1024 * 1024).Upload();

    // Create texture arrays
    ID3D12ResourcePtr resArray[4] = {NULL};
    for(int i = 0; i < 4; ++i)
    {
      resArray[i] =
          MakeTexture(DXGI_FORMAT_R32_FLOAT, 2, 2).Array(4).InitialState(D3D12_RESOURCE_STATE_COPY_DEST);
      MakeSRV(resArray[i]).NumSlices(4).CreateGPU(30 + i);

      float arrayData[16];
      for(int j = 0; j < 16; ++j)
        arrayData[j] = (float)(i + j);
      UploadTexture(uploadBuf, resArray[i], (byte *)arrayData, 2 * sizeof(float));
    }

    // In UNORM, 1/10, 2/10, 3/10, 4/10 for the first row, then reverse for the second row
    byte res1Data[16] = {26, 51, 77, 102, 26, 51, 77, 102, 102, 77, 51, 26, 102, 77, 51, 26};
    UploadTexture(uploadBuf, res1, res1Data, 8);

    // In UNORM, 5/10, 6/10, 7/10, 8/10
    byte res2Data[16] = {128, 153, 179, 204, 128, 153, 179, 204,
                         128, 153, 179, 204, 128, 153, 179, 204};
    UploadTexture(uploadBuf, res2, res2Data, 8);

    // Test the same resource mappings both with explicitly specified resources,
    // and a bindless style table param
    ID3D12RootSignaturePtr sig_5_0 = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 3),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 56),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, 1, 57),
    });
    ID3D12RootSignaturePtr sig_5_1 = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 3),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 4, 12, 0),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1, 20),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, UINT_MAX, 50),
    });
    ID3D12RootSignaturePtr sig_resArray = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 3),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10, 4, 30),
    });
    ID3D12RootSignaturePtr sig_bindless = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 3),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, UINT_MAX, 30),
    });
    ID3D12RootSignaturePtr sig_resourceAccess = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 56),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, 1, 57),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, UINT_MAX, 30),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1, 56),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 1, 57),
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 1),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 8, 0),
    });

    ID3D12PipelineStatePtr pso_5_0 = MakePSO()
                                         .RootSig(sig_5_0)
                                         .InputLayout()
                                         .VS(vsblob)
                                         .PS(psblob_5_0)
                                         .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    ID3D12PipelineStatePtr pso_5_1 = MakePSO()
                                         .RootSig(sig_5_1)
                                         .InputLayout()
                                         .VS(vsblob)
                                         .PS(psblob_5_1)
                                         .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    ID3D12PipelineStatePtr pso_resArray = MakePSO()
                                              .RootSig(sig_resArray)
                                              .InputLayout()
                                              .VS(vsblob)
                                              .PS(psblob_resArray)
                                              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    ID3D12PipelineStatePtr pso_bindless = MakePSO()
                                              .RootSig(sig_bindless)
                                              .InputLayout()
                                              .VS(vsblob)
                                              .PS(psblob_bindless)
                                              .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});
    ID3D12PipelineStatePtr pso_resourceAccess = MakePSO()
                                                    .RootSig(sig_resourceAccess)
                                                    .InputLayout()
                                                    .VS(vsblob)
                                                    .PS(psblob_resourceAccess)
                                                    .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(cb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(cbArray, D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      D3D12_CPU_DESCRIPTOR_HANDLE offrtv = MakeRTV(rtvtex).CreateCPU(1);

      OMSetRenderTargets(cmd, {offrtv}, {});
      ClearRenderTargetView(cmd, bbrtv, {0.4f, 0.5f, 0.6f, 1.0f});
      ClearRenderTargetView(cmd, offrtv, {0.4f, 0.5f, 0.6f, 1.0f});

      setMarker(cmd, "sm_5_0");
      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso_5_0);
      cmd->SetGraphicsRootSignature(sig_5_0);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
      cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      cmd->DrawInstanced(3, 1, 0, 0);

      setMarker(cmd, "sm_5_1");
      cmd->SetPipelineState(pso_5_1);
      cmd->SetGraphicsRootSignature(sig_5_1);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
      cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(3, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->DrawInstanced(3, 1, 0, 0);

      setMarker(cmd, "ResArray");
      cmd->SetPipelineState(pso_resArray);
      cmd->SetGraphicsRootSignature(sig_resArray);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
      cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->DrawInstanced(3, 1, 0, 0);

      setMarker(cmd, "Bindless");
      cmd->SetPipelineState(pso_bindless);
      cmd->SetGraphicsRootSignature(sig_bindless);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
      cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->DrawInstanced(3, 1, 0, 0);

      setMarker(cmd, "ResourceAccess");
      cmd->SetPipelineState(pso_resourceAccess);
      cmd->SetGraphicsRootSignature(sig_resourceAccess);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(3, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(4, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootConstantBufferView(5, cb->GetGPUVirtualAddress());
      cmd->SetGraphicsRootConstantBufferView(6, cb->GetGPUVirtualAddress() + sizeof(AlignedCB));
      cmd->SetGraphicsRootDescriptorTable(7, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
