/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

float4 main() : SV_Target0
{
	return float4(0, 1, 0, 1);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    uint32_t indices[1024 / 4] = {0, 1, 2};

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);
    ID3D12ResourcePtr ib = MakeBuffer().Data(indices);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12PSOCreator psoCreator = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);
    psoCreator.GraphicsDesc.StreamOutput.NumEntries = 0;
    psoCreator.GraphicsDesc.StreamOutput.pSODeclaration = (D3D12_SO_DECLARATION_ENTRY *)0x3456;
    psoCreator.GraphicsDesc.StreamOutput.NumStrides = 0xcccccccc;
    psoCreator.GraphicsDesc.StreamOutput.pBufferStrides = (UINT *)0x1234;

    ID3D12PipelineStatePtr pso = psoCreator;

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

      D3D12_HEAP_DESC heapDesc;
      heapDesc.SizeInBytes = 4096;
      heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
      heapDesc.Alignment = 0;
      heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
      heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
      heapDesc.Properties.CreationNodeMask = 1;
      heapDesc.Properties.VisibleNodeMask = 1;

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

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12DebugCommandListPtr debug = cmd;

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      if(debug)
        debug->AssertResourceState(bb, D3D12_RESOURCE_STATE_RENDER_TARGET, 0);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {1.0f, 0.0f, 1.0f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      D3D12_INDEX_BUFFER_VIEW view;
      view.BufferLocation = ib->GetGPUVirtualAddress();
      view.Format = DXGI_FORMAT_R32_UINT;
      view.SizeInBytes = 1024;
      cmd->IASetIndexBuffer(&view);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      // trash slots 3 and 4
      D3D12_CPU_DESCRIPTOR_HANDLE rtv3 = MakeRTV(rtvtex).CreateCPU(3);
      D3D12_CPU_DESCRIPTOR_HANDLE rtv4 = MakeRTV(rtvtex).CreateCPU(4);

      // write the proper RTV to slot 3
      MakeRTV(bb).CreateCPU(3);

      // copy to slot 4
      dev->CopyDescriptorsSimple(1, rtv4, rtv3, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

      // bind from slot 4
      cmd->OMSetRenderTargets(1, &rtv4, FALSE, NULL);

      // trash RTV slots 3 and 4 again
      MakeRTV(rtvtex).CreateCPU(3);
      MakeRTV(rtvtex).CreateCPU(4);

      setMarker(cmd, "Color Draw");

      cmd->DrawIndexedInstanced(3, 1, 0, 0, 0);

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

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
