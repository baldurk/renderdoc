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

RD_TEST(D3D12_Parameter_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "General tests of parameters known to cause problems - e.g. optional values that should be "
      "ignored, edge cases, special values, etc.";

  std::string pixel = R"EOSHADER(

Texture2D<float> empty : register(t50);

float4 main() : SV_Target0
{
	return float4(0, 1, 0, 1) + empty.Load(int3(0,0,0));
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    LUID luid = dev->GetAdapterLuid();
    IDXGIAdapterPtr pDXGIAdapter;
    ID3D12DevicePtr devB;
    {
      HRESULT hr = EnumAdapterByLuid(dev->GetAdapterLuid(), pDXGIAdapter);
      if(FAILED(hr))
        return 2;

      devB = CreateDevice({pDXGIAdapter}, D3D_FEATURE_LEVEL_11_0);
      if(!devB)
        return 2;
    }

    // create a buffer on another unrelated device
    ID3D12ResourcePtr bufferB = D3D12BufferCreator(devB, this).Data(DefaultTri);

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    uint32_t indices[1024 / 4] = {0, 1, 2};

    dev->CreateConstantBufferView(NULL, m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart());

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    D3D12_HEAP_DESC heapDesc;
    heapDesc.SizeInBytes = 4096;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    heapDesc.Alignment = 0;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Properties.CreationNodeMask = 1;
    heapDesc.Properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resDesc;
    resDesc.Alignment = 0;
    resDesc.DepthOrArraySize = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.Height = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Width = sizeof(indices);
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;

    ID3D12HeapPtr ibHeap;
    CHECK_HR(dev->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), (void **)&ibHeap));

    ID3D12Pageable *ibHeapPageable = (ID3D12Pageable *)ibHeap.GetInterfacePtr();

    ID3D12ResourcePtr ib;
    CHECK_HR(dev->CreatePlacedResource(ibHeap, 0, &resDesc, D3D12_RESOURCE_STATE_COMMON, NULL,
                                       __uuidof(ID3D12Resource), (void **)&ib));

    SetBufferData(ib, D3D12_RESOURCE_STATE_COMMON, (const byte *)indices, sizeof(indices));

    ID3D12ResourcePtr vb2;

    {
      ID3D12HeapPtr vbReleasedHeap;
      CHECK_HR(dev->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), (void **)&vbReleasedHeap));

      CHECK_HR(dev->CreatePlacedResource(vbReleasedHeap, 0, &resDesc, D3D12_RESOURCE_STATE_COMMON,
                                         NULL, __uuidof(ID3D12Resource), (void **)&vb2));
    }

    SetBufferData(vb2, D3D12_RESOURCE_STATE_COMMON, (const byte *)DefaultTri, sizeof(DefaultTri));

    // test residency refcounting
    ID3D12Pageable *vbPageable = (ID3D12Pageable *)vb.GetInterfacePtr();
    dev->MakeResident(1, &vbPageable);
    dev->MakeResident(1, &vbPageable);
    dev->MakeResident(1, &vbPageable);
    dev->Evict(1, &vbPageable);
    dev->Evict(1, &vbPageable);
    dev->Evict(1, &vbPageable);

    ID3D12RootSignaturePtr sig = MakeSig({
        // table that's larger than the descriptor heap we'll bind
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 50, 999, 0),
    });

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12PSOCreator psoCreator = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);
    psoCreator.GraphicsDesc.StreamOutput.NumEntries = 0;
    psoCreator.GraphicsDesc.StreamOutput.pSODeclaration = (D3D12_SO_DECLARATION_ENTRY *)0x3456;
    psoCreator.GraphicsDesc.StreamOutput.NumStrides = 0xcccccccc;
    psoCreator.GraphicsDesc.StreamOutput.pBufferStrides = (UINT *)0x1234;

    ID3D12RootSignaturePtr duplicateSig = MakeSig(
        {
            cbvParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
            constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 1, 1),
        },
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ID3D12DescriptorHeapPtr descHeap;

    {
      D3D12_DESCRIPTOR_HEAP_DESC desc;
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      desc.NodeMask = 1;
      desc.NumDescriptors = 4;
      desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&descHeap));
    }

    ID3D12DescriptorHeapPtr sampHeap;

    {
      D3D12_DESCRIPTOR_HEAP_DESC desc;
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      desc.NodeMask = 1;
      desc.NumDescriptors = 2000;
      desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
      CHECK_HR(dev->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&sampHeap));
    }

    sampHeap->SetName(L"Sampler Heap");

    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerDesc.MaxAnisotropy = 4;
    samplerDesc.MinLOD = 1.5f;
    UINT increment = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    D3D12_CPU_DESCRIPTOR_HANDLE samplerStart = sampHeap->GetCPUDescriptorHandleForHeapStart();
    dev->CreateSampler(&samplerDesc, {samplerStart.ptr + increment * 1234});

    D3D12_GPU_DESCRIPTOR_HANDLE descGPUHandle = descHeap->GetGPUDescriptorHandleForHeapStart();

    ID3D12DescriptorHeap *heaps[] = {
        descHeap.GetInterfacePtr(),
        sampHeap.GetInterfacePtr(),
    };

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    dev->CreateShaderResourceView(NULL, &srvDesc, descHeap->GetCPUDescriptorHandleForHeapStart());

    ID3D12PipelineStatePtr pso = psoCreator;
    ID3D12PipelineStatePtr pso2;

    if(dev2)
    {
      struct StreamStructBase
      {
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE rootsig_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
        UINT padding0;
        ID3D12RootSignature *pRootSignature;

        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE vs_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
        UINT padding1;
        D3D12_SHADER_BYTECODE vs = {};
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE ps_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
        UINT padding2;
        D3D12_SHADER_BYTECODE ps = {};

        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE input_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
        D3D12_INPUT_LAYOUT_DESC InputLayout;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE mask_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK;
        UINT SampleMask;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE dsv_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
        DXGI_FORMAT DSVFormat;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE blend_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND;
        D3D12_BLEND_DESC BlendState;
        UINT padding3;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE rast_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
        D3D12_RASTERIZER_DESC RasterizerState;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE depth_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
        D3D12_DEPTH_STENCIL_DESC DepthStencilState;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE prim_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE rtv_type =
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
        D3D12_RT_FORMAT_ARRAY RTVFormats;
      };

      struct StreamStructMesh : StreamStructBase
      {
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE as_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS;
        UINT padding3;
        D3D12_SHADER_BYTECODE as = {};
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE ms_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS;
        UINT padding4;
        D3D12_SHADER_BYTECODE ms = {};
      } streamStruct;

      streamStruct.InputLayout = psoCreator.GraphicsDesc.InputLayout;
      streamStruct.pRootSignature = psoCreator.GraphicsDesc.pRootSignature;
      streamStruct.vs = psoCreator.GraphicsDesc.VS;
      streamStruct.ps = psoCreator.GraphicsDesc.PS;
      streamStruct.SampleMask = psoCreator.GraphicsDesc.SampleMask;
      streamStruct.DSVFormat = psoCreator.GraphicsDesc.DSVFormat;
      streamStruct.BlendState = psoCreator.GraphicsDesc.BlendState;
      streamStruct.RasterizerState = psoCreator.GraphicsDesc.RasterizerState;
      streamStruct.DepthStencilState = psoCreator.GraphicsDesc.DepthStencilState;
      streamStruct.PrimitiveTopologyType = psoCreator.GraphicsDesc.PrimitiveTopologyType;
      memcpy(&streamStruct.RTVFormats.RTFormats, &psoCreator.GraphicsDesc.RTVFormats,
             sizeof(D3D12_RT_FORMAT_ARRAY));
      streamStruct.RTVFormats.NumRenderTargets = 1;

      D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
      streamDesc.pPipelineStateSubobjectStream = &streamStruct;
      streamDesc.SizeInBytes = sizeof(StreamStructMesh);

      // if the device understands the options7 query we assume it can handle mesh shader structs?
      HRESULT hr = dev2->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &opts7, sizeof(opts7));
      if(hr != S_OK)
      {
        streamDesc.SizeInBytes = sizeof(StreamStructBase);
      }

      hr = dev2->CreatePipelineState(&streamDesc, __uuidof(ID3D12PipelineState), (void **)&pso2);

      TEST_ASSERT(hr == S_OK, "Pipe created");
    }

    // if D3D12.4 (??) is available, use different interfaces
    if(dev4)
    {
      GPUSync();

      D3D12_RESOURCE_DESC desc;
      desc.Alignment = 0;
      desc.DepthOrArraySize = 1;
      desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Flags = D3D12_RESOURCE_FLAG_NONE;
      desc.Format = DXGI_FORMAT_UNKNOWN;
      desc.Height = 1;
      desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      desc.Width = sizeof(DefaultTri);
      desc.MipLevels = 1;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;

      CHECK_HR(dev4->CreateCommittedResource1(&heapDesc.Properties, D3D12_HEAP_FLAG_NONE, &desc,
                                              D3D12_RESOURCE_STATE_COMMON, NULL, NULL,
                                              __uuidof(ID3D12Resource), (void **)&vb));

      SetBufferData(vb, D3D12_RESOURCE_STATE_COMMON, (const byte *)DefaultTri, sizeof(DefaultTri));

      ID3D12Heap1Ptr heap;
      CHECK_HR(dev4->CreateHeap1(&heapDesc, NULL, __uuidof(ID3D12Heap1), (void **)&heap));

      desc.Width = sizeof(indices);

      CHECK_HR(dev4->CreatePlacedResource(heap, 0, &desc, D3D12_RESOURCE_STATE_COMMON, NULL,
                                          __uuidof(ID3D12Resource), (void **)&ib));

      SetBufferData(ib, D3D12_RESOURCE_STATE_COMMON, (const byte *)indices, sizeof(indices));
    }

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 4)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12CommandSignaturePtr cmdsig = MakeCommandSig(NULL, {vbArg(0), drawArg()});
    ID3D12ResourcePtr argBuf = MakeBuffer().Upload().Size(1024);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12DebugCommandListPtr debug = cmd;

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      // force duplicate signature to be used
      cmd->SetGraphicsRootSignature(duplicateSig);

      if(debug)
        debug->AssertResourceState(bb, D3D12_RESOURCE_STATE_RENDER_TARGET, 0);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {1.0f, 0.0f, 1.0f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      cmd->SetDescriptorHeaps(2, heaps);

      cmd->SetComputeRootSignature(duplicateSig);
      cmd->SetComputeRoot32BitConstants(1, 0, &debug, 0);
      cmd->SetGraphicsRoot32BitConstants(1, 0, &debug, 0);

      IASetVertexBuffer(cmd, vb2, sizeof(DefaultA2V), 0);
      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      D3D12_INDEX_BUFFER_VIEW view;
      view.BufferLocation = ib->GetGPUVirtualAddress();
      view.Format = DXGI_FORMAT_R32_UINT;
      view.SizeInBytes = 1024;
      cmd->IASetIndexBuffer(&view);
      if(pso2)
        cmd->SetPipelineState(pso2);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);
      cmd->SetGraphicsRootDescriptorTable(0, descGPUHandle);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      // trash slots 3 and 4
      D3D12_CPU_DESCRIPTOR_HANDLE rtv3 = MakeRTV(rtvtex).CreateCPU(3);
      D3D12_CPU_DESCRIPTOR_HANDLE rtv4 = MakeRTV(rtvtex).CreateCPU(4);

      // write the proper RTV to slot 3
      MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(3);

      // copy to slot 4
      dev->CopyDescriptorsSimple(1, rtv4, rtv3, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

      // bind from slot 4
      cmd->OMSetRenderTargets(1, &rtv4, FALSE, NULL);

      // trash RTV slots 3 and 4 again
      MakeRTV(rtvtex).CreateCPU(3);
      MakeRTV(rtvtex).CreateCPU(4);

      setMarker(cmd, "Color Draw");

      cmd->DrawIndexedInstanced(3, 1, 0, 0, 0);

      setMarker(cmd, "Empty indirect execute");

      cmd->ExecuteIndirect(cmdsig, 0, argBuf, 0, NULL, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      if(debug)
      {
        BOOL wrong = debug->AssertResourceState(bb, D3D12_RESOURCE_STATE_COPY_DEST, 0);
        BOOL right = debug->AssertResourceState(bb, D3D12_RESOURCE_STATE_PRESENT, 0);

        if(wrong == TRUE)
          TEST_WARN("Didn't get the expected return value from AssertResourceState(COPY_DEST)");
        if(right == FALSE)
          TEST_WARN("Didn't get the expected return value from AssertResourceState(PRESENT)");
      }

      cmd->Close();

      Submit({cmd});

      {
        ID3D12CommandAllocatorPtr tempAlloc;
        CHECK_HR(dev->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void **)&tempAlloc));

        ID3D12GraphicsCommandListPtr tempCmd = NULL;
        CHECK_HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAlloc, NULL,
                                        __uuidof(ID3D12GraphicsCommandList), (void **)&tempCmd));

        // record a lot of commands just to ensure that if they get corrupted we'll notice
        for(int i = 0; i < 1000; i++)
        {
          tempCmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
          tempCmd->SetDescriptorHeaps(1, &descHeap.GetInterfacePtr());
          tempCmd->SetPipelineState(pso);
          tempCmd->SetGraphicsRootSignature(sig);
          tempCmd->SetGraphicsRootDescriptorTable(0, descGPUHandle);
        }

        tempCmd->Close();

        ID3D12CommandList *list = tempCmd.GetInterfacePtr();

        queue->ExecuteCommandLists(1, &list);

        GPUSync();
      }

      // keep vertex/index buffer evicted across presents
      dev->Evict(1, &vbPageable);
      dev->Evict(1, &ibHeapPageable);

      Present();

      dev->MakeResident(1, &vbPageable);
      dev->MakeResident(1, &ibHeapPageable);
    }

    return 0;
  }
};

REGISTER_TEST();
