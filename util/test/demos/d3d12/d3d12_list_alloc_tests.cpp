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

#include <Psapi.h>

RD_TEST(D3D12_List_Alloc_Tests, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests different edge cases of pooled list allocators to ensure we don't have use-after-free "
      "problems.";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12CommandAllocatorPtr alloc1;
    CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         __uuidof(ID3D12CommandAllocator), (void **)&alloc1));
    ID3D12CommandAllocatorPtr alloc2;
    CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         __uuidof(ID3D12CommandAllocator), (void **)&alloc2));

    ID3D12GraphicsCommandListPtr list1 = GetCommandBuffer();
    ID3D12GraphicsCommandListPtr list2 = GetCommandBuffer();

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.resize(16 * 1024);
    for(D3D12_RESOURCE_BARRIER &b : barriers)
      b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

    while(Running())
    {
      PROCESS_MEMORY_COUNTERS memInfo = {};
      if(GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo)))
      {
        if(memInfo.WorkingSetSize > 800 * 1024 * 1024)
        {
          TEST_ERROR("Too much memory allocated - leak detected");
          break;
        }
      }

      {
        // start from scratch
        alloc1->Reset();

        list1->Reset(alloc1, NULL);

        // record commands that are too large to be pooled
        for(int i = 0; i < 100; i++)
          list1->ResourceBarrier((UINT)barriers.size(), barriers.data());

        list1->Close();

        // reset the allocator
        alloc1->Reset();

        // use it for another list
        list2->Reset(alloc1, NULL);

        // record some dummy commands, overwriting the chunk from above with a non-external alloc
        for(int i = 0; i < 100; i++)
          list2->SetPipelineState(pso);

        list2->Close();

        // reset list1 with a different allocator to force it to free any stored chunks. Since we
        // trashed the chunks above after allocator reset, this won't correctly free the external
        // chunk
        list1->Reset(alloc2, NULL);
        list1->Close();

        // re-associate list2
        list2->Reset(alloc2, NULL);
        list2->Close();
      }

      {
        ID3D12CommandAllocatorPtr allocTmp;
        CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             __uuidof(ID3D12CommandAllocator), (void **)&allocTmp));

        list1->Reset(allocTmp, NULL);

        // record some simple dummy commands

        IASetVertexBuffer(list1, vb, sizeof(DefaultA2V), 0);
        list1->SetPipelineState(pso);
        list1->SetGraphicsRootSignature(sig);

        RSSetViewport(list1, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(list1, {0, 0, screenWidth, screenHeight});
        list1->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        list1->Close();

        // destroy the allocator now
        allocTmp = NULL;

        // the chunk memory is freed but may not be trashed yet. Serialise a big chunk to try and
        // overwrite it.
        list2->Reset(alloc2, NULL);

        // record commands that are too large to be pooled
        for(int i = 0; i < 10; i++)
          list2->ResourceBarrier((UINT)barriers.size(), barriers.data());

        list2->Close();

        alloc2->Reset();

        // check that resetting list1 works fine even after the backing for its stored chunks has
        // been released
        list1->Reset(alloc2, NULL);
        list1->Close();

        // re-associate list2
        list2->Reset(alloc2, NULL);
        list2->Close();
      }

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

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
