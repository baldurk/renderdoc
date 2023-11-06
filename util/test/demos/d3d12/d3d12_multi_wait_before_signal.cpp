/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

template <UINT numQs>
class ComputeQueues
{
  ID3D12DevicePtr m_dev;
  ID3D12CommandQueuePtr m_computeQs[numQs];
  ID3D12FencePtr m_gpuQEndSyncFences[numQs];
  HANDLE m_gpuQSyncHandles[numQs];
  UINT64 m_gpuQSyncCounters[numQs];
  ID3D12CommandAllocatorPtr m_commandAllocators[numQs];
  std::vector<ID3D12GraphicsCommandListPtr> m_freeCommandBuffers[numQs];
  std::vector<ID3D12GraphicsCommandListPtr> m_pendingCommandBuffers[numQs];

public:
  ComputeQueues() = delete;
  ComputeQueues(const ComputeQueues &) = delete;
  void operator=(const ComputeQueues &) = delete;

  ComputeQueues(ID3D12DevicePtr dev) : m_dev(dev)
  {
    D3D12_COMMAND_QUEUE_DESC qDesc = {};
    qDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    wchar_t nameBuf[32];

    for(UINT i = 0; i < numQs; ++i)
    {
      dev->CreateCommandQueue(&qDesc, __uuidof(ID3D12CommandQueue), (void **)&m_computeQs[i]);
      dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence),
                       (void **)&m_gpuQEndSyncFences[i]);
      wsprintf(nameBuf, L"Compute %u GPU end sync fence", i);
      m_gpuQEndSyncFences[i]->SetName(nameBuf);
      m_gpuQSyncHandles[i] = ::CreateEvent(NULL, FALSE, FALSE, NULL);
      CHECK_HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                           __uuidof(ID3D12CommandAllocator),
                                           (void **)&m_commandAllocators[i]));
      wsprintf(nameBuf, L"Compute %u command allocator", i);
      m_commandAllocators[i]->SetName(nameBuf);
      m_gpuQSyncCounters[i] = 1u;
      m_freeCommandBuffers[i].reserve(4);
      m_pendingCommandBuffers[i].reserve(4);

      ID3D12GraphicsCommandListPtr list = NULL;
      CHECK_HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_commandAllocators[i],
                                      NULL, __uuidof(ID3D12GraphicsCommandList), (void **)&list));
      list->Close();    // list starts opened, close it
      m_freeCommandBuffers[i].push_back(list);
    }
  }

  ~ComputeQueues()
  {
    for(UINT i = 0; i < numQs; ++i)
    {
      m_gpuQSyncCounters[i]++;
      CHECK_HR(m_computeQs[i]->Signal(m_gpuQEndSyncFences[i], m_gpuQSyncCounters[i]));
      CHECK_HR(m_gpuQEndSyncFences[i]->SetEventOnCompletion(m_gpuQSyncCounters[i],
                                                            m_gpuQSyncHandles[i]));
    }
    WaitForMultipleObjects(numQs, &m_gpuQSyncHandles[0], TRUE /*waitAll*/, 10000);

    for(UINT i = 0; i < numQs; ++i)
    {
      m_pendingCommandBuffers[i].clear();
      m_freeCommandBuffers[i].clear();
      m_commandAllocators[i] = NULL;
      CloseHandle(m_gpuQSyncHandles[i]);
      m_gpuQEndSyncFences[i] = NULL;
      m_computeQs[i] = NULL;
    }

    m_dev = NULL;
  }

  ID3D12GraphicsCommandListPtr GetResetCommandBuffer(UINT queueIdx)
  {
    if(m_freeCommandBuffers[queueIdx].empty())
    {
      ID3D12GraphicsCommandListPtr list = NULL;
      CHECK_HR(m_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                        m_commandAllocators[queueIdx], NULL,
                                        __uuidof(ID3D12GraphicsCommandList), (void **)&list));
      list->Close();    // list starts opened, close it
      m_freeCommandBuffers[queueIdx].push_back(list);
    }

    ID3D12GraphicsCommandListPtr ret = m_freeCommandBuffers[queueIdx].back();
    m_freeCommandBuffers[queueIdx].pop_back();

    ret->Reset(m_commandAllocators[queueIdx], NULL);
    return ret;
  }

  void Wait(UINT queueIdx, ID3D12Fence *fence, UINT64 val)
  {
    TEST_ASSERT(queueIdx < numQs, "Out of bounds queueIdx");
    m_computeQs[queueIdx]->Wait(fence, val);
  }

  void Signal(UINT queueIdx, ID3D12Fence *fence, UINT64 val)
  {
    TEST_ASSERT(queueIdx < numQs, "Out of bounds queueIdx");
    m_computeQs[queueIdx]->Signal(fence, val);
  }

  void Submit(UINT queueIdx, ID3D12GraphicsCommandListPtr cmdList)
  {
    ID3D12CommandList *rawList = cmdList;
    m_computeQs[queueIdx]->ExecuteCommandLists(1, &rawList);
    m_pendingCommandBuffers[queueIdx].push_back(cmdList);
  }

  void EndSyncFenceSignal(UINT queueIdx)
  {
    m_gpuQSyncCounters[queueIdx]++;
    m_computeQs[queueIdx]->Signal(m_gpuQEndSyncFences[queueIdx], m_gpuQSyncCounters[queueIdx]);
  }

  void PostPresentSyncAndReset()
  {
    for(UINT i = 0; i < numQs; ++i)
    {
      CHECK_HR(m_gpuQEndSyncFences[i]->SetEventOnCompletion(m_gpuQSyncCounters[i],
                                                            m_gpuQSyncHandles[i]));
    }
    WaitForMultipleObjects(numQs, &m_gpuQSyncHandles[0], TRUE /*waitAll*/, 10000);

    for(UINT i = 0; i < numQs; ++i)
    {
      m_freeCommandBuffers[i].insert(m_freeCommandBuffers[i].end(),
                                     m_pendingCommandBuffers[i].begin(),
                                     m_pendingCommandBuffers[i].end());
      m_pendingCommandBuffers[i].clear();
      m_commandAllocators[i]->Reset();
    }
  }
};

RD_TEST(D3D12_Multi_Wait_Before_Signal, D3D12GraphicsTest)
{
  // Overview of work -
  //
  // Comp0: Wait GQ-------------------| Modify col0, Signal GQ!
  // Comp1: Wait.GQ-------------------------------------------------------| Modify col1, Signal GQ!
  // GfxQ:  Init RTs/buff,  Signal CQ0!  Wait CQ0-------------| Signal CQ1!  Wait CQ1-------------| Draw Tris

  static constexpr const char *Description =
      "Draws two triangles that read their colours from a buffer that is "
      "populated by two different compute queue dispatches.  This tests "
      "that any walking of queue command lists is able to deserialise multiple "
      "queues that may initially appear blocked, waiting for a fence signal "
      "from another queue.";

  enum CBVUAVSRVDescriptorHeapIdx : uint32_t
  {
    eBufferUAV = 0,
    eBufferSRV,
  };

  std::string sources = R"EOSHADERS(

cbuffer RootConstants : register(b0)
{
  uint rootConstant0;
  uint rootConstant1;
};
RWStructuredBuffer<float4> bufferRW : register(u0);
StructuredBuffer<float4> buffer : register(t0);

[numthreads(2,1,1)]
void resetBufferCS(uint dispatchTID : SV_DispatchThreadID)
{
  bufferRW[dispatchTID] = float4(0.0f, 0.0f, 1.0f, 0.0f);
}

[numthreads(1,1,1)]
void mainCS()
{
  uint myBufferIdx = rootConstant0;
  float4 oldCol = bufferRW[myBufferIdx];
  float4 addCol = float4((float)(rootConstant1 & 0xffu) / 255.0f, (float)((rootConstant1>>8u) & 0xffu) / 255.0f, (float)((rootConstant1>>16u) & 0xffu) / 255.0f, (float)((rootConstant1>>24u) & 0xffu) / 255.0f);
  bufferRW[myBufferIdx] = oldCol + addCol;
}

float4 mainVS(in float3 pos : POSITION) : SV_POSITION
{
	return float4(pos, 1);
}

float4 mainPS(in float4 pos : SV_POSITION) : SV_Target0
{
  uint myBufferIdx = rootConstant0;
  return buffer[myBufferIdx];
}

)EOSHADERS";

  enum class CBVUAVSRVHeapIdx : uint32_t
  {
    eBufferUAV = 0,
    eBufferSRV
  };

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ComputeQueues<2> computeQs(dev);

    ID3DBlobPtr resetBuffCSBlob = Compile(sources, "resetBufferCS", "cs_5_0");
    ID3DBlobPtr csBlob = Compile(sources, "mainCS", "cs_5_0");
    ID3DBlobPtr vsBlob = Compile(sources, "mainVS", "vs_4_0");
    ID3DBlobPtr psBlob = Compile(sources, "mainPS", "ps_4_0");

    const Vec3f twoTrisVBDat[6] = {
        Vec3f(-0.5f, -0.25f, 0.9f), Vec3f(0.25f, 0.5f, 0.9f),   Vec3f(1.0f, -0.25f, 0.9f),
        Vec3f(-0.75f, 0.75f, 0.5f), Vec3f(0.75f, -0.75f, 0.5f), Vec3f(-0.75f, -0.75f, 0.5f),
    };
    ID3D12ResourcePtr vb = MakeBuffer().Data(twoTrisVBDat);

    uint32_t rootConstants[2];
    ID3D12RootSignaturePtr sig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0, sizeof(rootConstants) / sizeof(uint32_t)),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1,
                   (uint32_t)CBVUAVSRVHeapIdx::eBufferUAV, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1,
                   (uint32_t)CBVUAVSRVHeapIdx::eBufferSRV, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE),
    });

    static const std::vector<D3D12_INPUT_ELEMENT_DESC> vtxLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    ID3D12PipelineStatePtr reset_buff_cs_pso = MakePSO().RootSig(sig).CS(resetBuffCSBlob);
    reset_buff_cs_pso->SetName(L"reset_buff_cs_pso");
    ID3D12PipelineStatePtr cs_pso = MakePSO().RootSig(sig).CS(csBlob);
    cs_pso->SetName(L"cs_pso");
    D3D12PSOCreator gfx_pso_creator =
        MakePSO().RootSig(sig).InputLayout(vtxLayout).VS(vsBlob).PS(psBlob).DSV(DXGI_FORMAT_D32_FLOAT);
    gfx_pso_creator.GraphicsDesc.DepthStencilState.DepthEnable = TRUE;
    gfx_pso_creator.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    gfx_pso_creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
    gfx_pso_creator.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    ID3D12PipelineStatePtr gfx_pso = gfx_pso_creator;
    gfx_pso->SetName(L"gfx_pso");

    D3D12_CPU_DESCRIPTOR_HANDLE hBackBufferRTVDescs[2] = {
        MakeRTV(bbTex[0]).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0),
        MakeRTV(bbTex[1]).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(1),
    };

    D3D12_CLEAR_VALUE clearVal;
    clearVal.Format = DXGI_FORMAT_D32_FLOAT;
    clearVal.DepthStencil.Depth = 0.0f;
    clearVal.DepthStencil.Stencil = 0u;
    ID3D12ResourcePtr dsvTex = MakeTexture(DXGI_FORMAT_D32_FLOAT, screenWidth, screenHeight)
                                   .DSV()
                                   .NoSRV()
                                   .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE)
                                   .ClearVal(clearVal);
    D3D12_CPU_DESCRIPTOR_HANDLE hDsvCpuDesc = MakeDSV(dsvTex).CreateCPU(0);

    ID3D12ResourcePtr buffer =
        MakeBuffer().Size(32).UAV().InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    buffer->SetName(L"buffer");
    D3D12_GPU_DESCRIPTOR_HANDLE hBuffUavGpuDesc =
        MakeUAV(buffer).StructureStride(16).CreateGPU((uint32_t)CBVUAVSRVHeapIdx::eBufferUAV);
    D3D12_GPU_DESCRIPTOR_HANDLE hBuffSrvGpuDesc =
        MakeSRV(buffer).StructureStride(16).CreateGPU((uint32_t)CBVUAVSRVHeapIdx::eBufferSRV);

    ID3D12FencePtr gfxToCompute0Fence, gfxToCompute1Fence, compute0ToGfxFence, compute1ToGfxFence;
    dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void **)&gfxToCompute0Fence);
    gfxToCompute0Fence->SetName(L"gfxToCompute0");
    dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void **)&gfxToCompute1Fence);
    gfxToCompute1Fence->SetName(L"gfxToCompute1");
    dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void **)&compute0ToGfxFence);
    compute0ToGfxFence->SetName(L"compute0ToGfx");
    dev->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void **)&compute1ToGfxFence);
    compute1ToGfxFence->SetName(L"compute1ToGfx");

    UINT64 sharedGfxComputeSyncCounter = 1;

    // Transition resources from initial states to states they'll be after completion of each frame
    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    while(Running())
    {
      sharedGfxComputeSyncCounter++;

      {    // ComputeQ0's work for the entire frame
        computeQs.Wait(0, gfxToCompute0Fence, sharedGfxComputeSyncCounter);

        ID3D12GraphicsCommandListPtr cmd = computeQs.GetResetCommandBuffer(0);
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        cmd->SetComputeRootSignature(sig);
        cmd->SetPipelineState(cs_pso);

        rootConstants[0] = 0;    // myBufferIdx
        rootConstants[1] = 0x80u | (0x40u << 8) | (0x00u << 16) |
                           (0xffu << 24);    // myCol (packed):  Brown (purple, added to blue)
        cmd->SetComputeRoot32BitConstants(0, 2, &rootConstants[0], 0);
        cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetComputeRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->Dispatch(1, 1, 1);
        cmd->Close();

        computeQs.Submit(0, cmd);

        computeQs.Signal(0, compute0ToGfxFence, sharedGfxComputeSyncCounter);

        computeQs.EndSyncFenceSignal(0);
      }

      {    // ComputeQ1's work for the entire frame
        computeQs.Wait(1, gfxToCompute1Fence, sharedGfxComputeSyncCounter);

        ID3D12GraphicsCommandListPtr cmd = computeQs.GetResetCommandBuffer(1);
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        cmd->SetComputeRootSignature(sig);
        cmd->SetPipelineState(cs_pso);

        rootConstants[0] = 1;    // myBufferIdx
        rootConstants[1] = 0x40u | (0xc0u << 8) | (0x00u << 16) |
                           (0xffu << 24);    // myCol (packed):  Green (light blue, added to blue)
        cmd->SetComputeRoot32BitConstants(0, 2, &rootConstants[0], 0);
        cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetComputeRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->Dispatch(1, 1, 1);
        cmd->Close();

        computeQs.Submit(1, cmd);

        computeQs.Signal(1, compute1ToGfxFence, sharedGfxComputeSyncCounter);

        computeQs.EndSyncFenceSignal(1);
      }

      {    // Gfx work
        // Clear RTs
        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
        Reset(cmd);

        ResourceBarrier(cmd, bbTex[texIdx], D3D12_RESOURCE_STATE_PRESENT,
                        D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ClearDepthStencilView(hDsvCpuDesc, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, NULL);
        const Vec4f clearCol(0.2f, 0.2f, 0.2f, 0.0f);
        cmd->ClearRenderTargetView(hBackBufferRTVDescs[texIdx], &clearCol.x, 0, NULL);

        // Reset 'buffer' with initial colour (blue)
        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
        cmd->SetComputeRootSignature(sig);
        cmd->SetPipelineState(reset_buff_cs_pso);
        cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetComputeRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->Dispatch(1, 1, 1);

        D3D12_RESOURCE_BARRIER buffer_uav_barrier;
        buffer_uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        buffer_uav_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        buffer_uav_barrier.UAV.pResource = buffer;
        cmd->ResourceBarrier(1, &buffer_uav_barrier);

        cmd->Close();

        ID3D12CommandList *rawCmdListPtr = cmd;
        queue->ExecuteCommandLists(1, &rawCmdListPtr);

        // Signal CQ0
        TEST_ASSERT(gfxToCompute0Fence->GetCompletedValue() < sharedGfxComputeSyncCounter,
                    "Compute0 hasn't waited for gfx signal!");
        queue->Signal(gfxToCompute0Fence, sharedGfxComputeSyncCounter);

        // Wait on CQ0
        queue->Wait(compute0ToGfxFence, sharedGfxComputeSyncCounter);

        // Signal CQ1
        TEST_ASSERT(gfxToCompute1Fence->GetCompletedValue() < sharedGfxComputeSyncCounter,
                    "Compute1 hasn't waited for gfx signal");
        queue->Signal(gfxToCompute1Fence, sharedGfxComputeSyncCounter);

        // Wait on CQ1
        queue->Wait(compute1ToGfxFence, sharedGfxComputeSyncCounter);

        // Draw Tris
        cmd = GetCommandBuffer();
        Reset(cmd);

        ResourceBarrier(cmd, buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        cmd->SetDescriptorHeaps(
            1, &m_CBVUAVSRV.GetInterfacePtr());    // Must be done BEFORE setting root signatures
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        IASetVertexBuffer(cmd, vb, sizeof(Vec3f), 0);
        cmd->SetPipelineState(gfx_pso);
        cmd->SetGraphicsRootSignature(sig);
        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});
        OMSetRenderTargets(cmd, {hBackBufferRTVDescs[texIdx]}, hDsvCpuDesc);
        rootConstants[0] = 0;    // myBufferIdx
        cmd->SetGraphicsRoot32BitConstants(0, 1, &rootConstants[0], 0);
        cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->DrawInstanced(3, 1, 0, 0);

        rootConstants[0] = 1;    // myBufferIdx
        cmd->SetGraphicsRoot32BitConstants(0, 1, &rootConstants[0], 0);
        cmd->DrawInstanced(3, 1, 3, 0);
        setMarker(cmd, "Last draw");    // Help locate this draw through 'find_action' in python test

        ResourceBarrier(cmd, bbTex[texIdx], D3D12_RESOURCE_STATE_RENDER_TARGET,
                        D3D12_RESOURCE_STATE_PRESENT);
        texIdx = 1u - texIdx;

        // Transition resources back to their expected states at the start of the next frame
        ResourceBarrier(cmd, buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        cmd->Close();

        Submit({cmd});
      }

      Present();    // Just deals with the gfx set of objects, so we'll do the equivalent
      // sync & resetting for our compute work -
      computeQs.PostPresentSyncAndReset();
    }

    return 0;
  }
};

REGISTER_TEST();
