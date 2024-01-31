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

RD_TEST(D3D12_Map_PlacedAlias, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Check that mapped data is still saved even if the mapped resource is not the one used in "
      "rendering.";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    UINT heapSize = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 3;

    D3D12_HEAP_DESC heapDesc;
    heapDesc.SizeInBytes = heapSize;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    heapDesc.Alignment = 0;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
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
    resDesc.Width = sizeof(DefaultTri);
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.SampleDesc.Quality = 0;

    ID3D12HeapPtr vbHeap;
    CHECK_HR(dev->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), (void **)&vbHeap));

    ID3D12ResourcePtr vb;
    CHECK_HR(dev->CreatePlacedResource(vbHeap, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, &resDesc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                       __uuidof(ID3D12Resource), (void **)&vb));

    ID3D12ResourcePtr vb2;
    CHECK_HR(dev->CreatePlacedResource(vbHeap, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 2,
                                       &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                       __uuidof(ID3D12Resource), (void **)&vb2));

    resDesc.Width = heapSize;
    ID3D12ResourcePtr mapBuffer;
    CHECK_HR(dev->CreatePlacedResource(vbHeap, 0, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                       __uuidof(ID3D12Resource), (void **)&mapBuffer));

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    byte *mapptr = NULL;
    mapBuffer->Map(0, NULL, (void **)&mapptr);

    // clear the buffer before capturing, and we'll do so at the end after submitting, to ensure
    // data can only come from detected map writes.
    memset(mapptr, 0xfe, heapSize);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      memcpy(mapptr + D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, DefaultTri, sizeof(DefaultTri));

      Submit({cmd});

      GPUSync();

      memset(mapptr, 0xfe, heapSize);

      Present();
    }

    mapBuffer->Unmap(0, NULL);

    return 0;
  }
};

REGISTER_TEST();
