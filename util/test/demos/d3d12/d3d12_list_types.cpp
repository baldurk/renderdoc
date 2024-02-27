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

RD_TEST(D3D12_List_Types, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Uses command lists of different types to ensure all are captured and replayed properly.";

  std::string compute = R"EOSHADER(

struct A2V
{
  float3 pos;
  float4 col;
  float2 uv;
};

RWStructuredBuffer<A2V> verts : register(u0);

[numthreads(3,1,1)]
void main(uint3 tid : SV_GroupThreadID)
{
	verts[tid.x].uv = float2(1234.0f, 5678.0f);
	verts[tid.x].col.b += 1.0f;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    ID3D12ResourcePtr vb_src = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1, 0),
    });

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ID3DBlobPtr csblob = Compile(compute, "main", "cs_5_0");

    ID3D12PipelineStatePtr comppso = MakePSO().RootSig(sig).CS(csblob);

    ResourceBarrier(vb_src, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);

    ID3D12CommandAllocatorPtr computeAlloc;
    CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                         __uuidof(ID3D12CommandAllocator), (void **)&computeAlloc));
    ID3D12CommandAllocatorPtr copyAlloc;
    CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
                                         __uuidof(ID3D12CommandAllocator), (void **)&copyAlloc));

    ID3D12GraphicsCommandListPtr computeList = NULL;
    CHECK_HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, computeAlloc, NULL,
                                    __uuidof(ID3D12GraphicsCommandList), (void **)&computeList));
    computeList->Close();

    ID3D12GraphicsCommandListPtr copyList = NULL;

    // where supported, use the new way that doesn't need an extra close
    if(dev4)
    {
      CHECK_HR(dev4->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_LIST_FLAG_NONE,
                                        __uuidof(ID3D12GraphicsCommandList), (void **)&copyList));
    }
    else
    {
      CHECK_HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copyAlloc, NULL,
                                      __uuidof(ID3D12GraphicsCommandList), (void **)&copyList));
      copyList->Close();
    }

    ID3D12CommandQueuePtr computeQueue;
    {
      D3D12_COMMAND_QUEUE_DESC desc = {};
      desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
      dev->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void **)&computeQueue);
    }

    ID3D12CommandQueuePtr copyQueue;
    {
      D3D12_COMMAND_QUEUE_DESC desc = {};
      desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
      dev->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void **)&copyQueue);
    }

    while(Running())
    {
      ID3D12ResourcePtr vb = MakeBuffer().UAV().Size(sizeof(DefaultTri));

      MakeUAV(vb).NumElements(3).StructureStride(sizeof(DefaultA2V)).CreateGPU(0);

      // first copy the VB as-is
      {
        copyList->Reset(copyAlloc, NULL);

        copyList->CopyResource(vb, vb_src);

        copyList->Close();

        ID3D12CommandList *copySubmit = copyList.GetInterfacePtr();
        copyQueue->ExecuteCommandLists(1, &copySubmit);
        copyQueue->Signal(m_GPUSyncFence, ++m_GPUSyncCounter);
      }

      // then invoke compute shader to change color
      {
        computeList->Reset(computeAlloc, NULL);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = vb;
        barrier.Transition.Subresource = 0;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        computeList->ResourceBarrier(1, &barrier);

        computeList->SetComputeRootSignature(sig);
        computeList->SetPipelineState(comppso);
        computeList->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        computeList->SetComputeRootDescriptorTable(
            0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        computeList->Dispatch(1, 1, 1);

        computeList->Close();

        ID3D12CommandList *computeSubmit = computeList.GetInterfacePtr();
        computeQueue->Wait(m_GPUSyncFence, m_GPUSyncCounter);
        computeQueue->ExecuteCommandLists(1, &computeSubmit);
        computeQueue->Signal(m_GPUSyncFence, ++m_GPUSyncCounter);
      }

      // then draw
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = vb;
      barrier.Transition.Subresource = 0;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

      cmd->ResourceBarrier(1, &barrier);

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

      queue->Wait(m_GPUSyncFence, m_GPUSyncCounter);
      Submit({cmd});

      // sync so we can release the buffer
      GPUSync();

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
