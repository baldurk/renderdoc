/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2026 Baldur Karlsson
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

#include "d3d12_command_list.h"
#include <algorithm>
#include "core/settings.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3DCommandListMarkerApi.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_replay.h"

RDOC_EXTERN_CONFIG(bool, D3D12_Debug_RT_Auditing);

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_Close(SerialiserType &ser)
{
  ResourceId BakedCommandList;

  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = m_ListRecord;
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      BakedCommandList = record->bakedCommands->GetResourceID();
  }

  SERIALISE_ELEMENT_LOCAL(CommandList, GetResourceID())
      .TypedAs("ID3D12GraphicsCommandList *"_lit)
      .Important();
  SERIALISE_ELEMENT(BakedCommandList).TypedAs("ID3D12GraphicsCommandList *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = BakedCommandList;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->HasRerecordCmdList(BakedCommandList))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(BakedCommandList);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
        RDCDEBUG("Ending re-recorded command list for %s baked to %s", ToStr(CommandList).c_str(),
                 ToStr(BakedCommandList).c_str());
#endif

        BakedCmdListInfo &bakedInfo = m_Cmd->m_BakedCmdListInfo[BakedCommandList];

        for(int i = 0; i < bakedInfo.markerCount; i++)
          D3D12MarkerRegion::End(list);

        for(BakedCmdListInfo::OutstandingQuery &q : bakedInfo.m_OutstandingQueries)
          Unwrap(list)->EndQuery(Unwrap(q.heap), q.Type, q.Index);

        bakedInfo.m_OutstandingQueries.clear();

        if(m_Cmd->m_ActionCallback)
          m_Cmd->m_ActionCallback->PreCloseCommandList(list);

        // if(m_Cmd->m_Partial[D3D12CommandData::Primary].renderPassActive)
        // list->EndRenderPass();

        list->Close();

        if(m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent == CommandList)
          m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent = ResourceId();
      }

      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID = 0;
    }
    else
    {
      GetResourceManager()->GetResAs<WrappedID3D12GraphicsCommandList>(CommandList)->Close();

      BakedCmdListInfo &baked = m_Cmd->m_BakedCmdListInfo[BakedCommandList];
      BakedCmdListInfo &parent = m_Cmd->m_BakedCmdListInfo[CommandList];

      baked.eventCount = baked.curEventID;
      baked.curEventID = 0;
      baked.parentList = CommandList;

      baked.endChunk = uint32_t(m_Cmd->m_StructuredFile->chunks.size() - 1);

      parent.curEventID = 0;
      parent.eventCount = 0;
    }
  }

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Close()
{
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pList->Close());

  if(IsCaptureMode(m_State))
  {
    {
      CACHE_THREAD_SERIALISER();
      ser.SetActionChunk();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Close);
      Serialise_Close(ser);

      m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    }

    m_ListRecord->Bake();
  }

  CHECK_HR(m_pDevice, ret);

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_Reset(SerialiserType &ser,
                                                       ID3D12CommandAllocator *pAllocator,
                                                       ID3D12PipelineState *pInitialState)
{
  // parameters to create the list with if needed
  SERIALISE_ELEMENT_LOCAL(riid, m_Init.riid).Hidden();
  SERIALISE_ELEMENT_LOCAL(nodeMask, m_Init.nodeMask).Hidden();
  SERIALISE_ELEMENT_LOCAL(type, m_Init.type).Hidden();

  ResourceId BakedCommandList;

  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = m_ListRecord;
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      BakedCommandList = record->bakedCommands->GetResourceID();
  }

  SERIALISE_ELEMENT(BakedCommandList).TypedAs("ID3D12GraphicsCommandList *"_lit);
  SERIALISE_ELEMENT_LOCAL(CommandList, GetResourceID())
      .TypedAs("ID3D12GraphicsCommandList *"_lit)
      .Important();
  SERIALISE_ELEMENT(pAllocator);
  SERIALISE_ELEMENT(pInitialState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    nodeMask = 0;

    m_Cmd->m_LastCmdListID = BakedCommandList;

    if(IsActiveReplaying(m_State))
    {
      const uint32_t length = m_Cmd->m_BakedCmdListInfo[BakedCommandList].eventCount;

      bool rerecord = false;
      bool partial = false;
      int partialType = D3D12CommandData::ePartialNum;

      // check for partial execution of this command list
      for(int p = 0; p < D3D12CommandData::ePartialNum; p++)
      {
        const rdcarray<uint32_t> &baseEvents = m_Cmd->m_Partial[p].cmdListExecs[BakedCommandList];

        for(auto it = baseEvents.begin(); it != baseEvents.end(); ++it)
        {
          if(*it <= m_Cmd->m_LastEventID && m_Cmd->m_LastEventID < (*it + length))
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Reset - partial detected %u < %u < %u, %s -> %s", *it, m_Cmd->m_LastEventID,
                     *it + length, ToStr(CommandList).c_str(), ToStr(BakedCommandList).c_str());
#endif

            m_Cmd->m_Partial[p].partialParent = BakedCommandList;
            m_Cmd->m_Partial[p].baseEvent = *it;

            rerecord = true;
            partial = true;
            partialType = p;
          }
          else if(*it <= m_Cmd->m_LastEventID)
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Reset() - full re-record detected %u < %u <= %u, %s -> %s", *it, *it + length,
                     m_Cmd->m_LastEventID, ToStr(m_Cmd->m_LastCmdListID).c_str(),
                     ToStr(BakedCommandList).c_str());
#endif

            // this submission is completely within the range, so it should still be re-recorded
            rerecord = true;
          }
        }
      }

      {
        ID3D12GraphicsCommandList *listptr = NULL;
        HRESULT hr =
            m_pDevice->CreateCommandList(BakedCommandList, nodeMask, type, pAllocator, pInitialState,
                                         __uuidof(ID3D12GraphicsCommandList), (void **)&listptr);

        if(FAILED(hr))
        {
          SET_ERROR_RESULT(m_Cmd->m_FailedReplayResult, ResultCode::APIReplayFailed,
                           "Failed creating command list, HRESULT: %s", ToStr(hr).c_str());
          return false;
        }

        // this is a safe upcast because it's a wrapped object
        ID3D12GraphicsCommandListX *list = (ID3D12GraphicsCommandListX *)listptr;

        if(rerecord)
        {
          // we store under both baked and non baked ID.
          // The baked ID is the 'real' entry, the non baked is simply so it
          // can be found in the subsequent serialised commands that ref the
          // non-baked ID. The baked ID is referenced by the submit itself.
          //
          // In Close() we erase the non-baked reference, and since
          // we know you can only be recording a command list once at a time
          // (even if it's baked to several command listsin the frame)
          // there's no issue with clashes here.
          m_Cmd->m_RerecordCmds[BakedCommandList] = list;
          m_Cmd->m_RerecordCmds[CommandList] = list;
        }
        else
        {
          list->Close();
        }

        // always create a version of this list even if we're not re-recording, so the serialisation
        // has an object to find
        m_Cmd->m_RerecordCmdList.push_back(list);
      }

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state = D3D12RenderState();
      state.m_ResourceManager = GetResourceManager();
      state.m_DebugManager = m_pDevice->GetDebugManager();
      state.pipe = GetResID(pInitialState);

      if(state.pipe != ResourceId())
      {
        WrappedID3D12PipelineState *pipe = (WrappedID3D12PipelineState *)pInitialState;
        if(pipe->IsGraphics())
        {
          state.depthBias = pipe->graphics->RasterizerState.DepthBias;
          state.depthBiasClamp = pipe->graphics->RasterizerState.DepthBiasClamp;
          state.slopeScaledDepthBias = pipe->graphics->RasterizerState.SlopeScaledDepthBias;
          state.cutValue = pipe->graphics->IBStripCutValue;
        }
      }

      // whenever a command-building chunk asks for the command list, it
      // will get our baked version.
      if(GetResourceManager()->HasReplacement(CommandList))
        GetResourceManager()->RemoveReplacement(CommandList);

      GetResourceManager()->ReplaceResource(CommandList, BakedCommandList);

      m_Cmd->m_BakedCmdListInfo[CommandList].markerCount =
          m_Cmd->m_BakedCmdListInfo[BakedCommandList].markerCount = 0;
      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID =
          m_Cmd->m_BakedCmdListInfo[BakedCommandList].curEventID = 0;
      m_Cmd->m_BakedCmdListInfo[CommandList].barriers.clear();
      m_Cmd->m_BakedCmdListInfo[BakedCommandList].barriers.clear();
    }
    else
    {
      if(!GetResourceManager()->HasResource(BakedCommandList))
      {
        ID3D12GraphicsCommandList *list = NULL;
        HRESULT hr =
            m_pDevice->CreateCommandList(BakedCommandList, nodeMask, type, pAllocator, pInitialState,
                                         __uuidof(ID3D12GraphicsCommandList), (void **)&list);
        RDCASSERTEQUAL(hr, S_OK);

        m_pDevice->AddResource(BakedCommandList, ResourceType::CommandBuffer, "Baked Command List");
        m_pDevice->GetResourceDesc(BakedCommandList).initialisationChunks.clear();
        m_pDevice->DerivedResource(CommandList, BakedCommandList);
        m_pDevice->DerivedResource(pAllocator, BakedCommandList);
        if(pInitialState)
          m_pDevice->DerivedResource(pInitialState, BakedCommandList);

        ResourceDescription &descr = m_pDevice->GetResourceDesc(CommandList);
        if(!descr.autogeneratedName)
        {
          m_pDevice->GetResourceDesc(BakedCommandList).SetCustomName(descr.name + " (Baked)");
        }

        // whenever a command-building chunk asks for the command list, it
        // will get our baked version.
        if(GetResourceManager()->HasReplacement(CommandList))
          GetResourceManager()->RemoveReplacement(CommandList);

        GetResourceManager()->ReplaceResource(CommandList, BakedCommandList);

        // this is a safe upcast because it's a wrapped object
        ID3D12GraphicsCommandListX *listX = (ID3D12GraphicsCommandListX *)list;
        m_Cmd->m_RerecordCmdList.push_back(listX);
      }
      else
      {
        ID3D12GraphicsCommandList *list =
            GetResourceManager()->GetResAs<WrappedID3D12GraphicsCommandList>(BakedCommandList)->GetReal();
        list->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
      }

      {
        m_Cmd->m_BakedCmdListInfo[CommandList].type =
            m_Cmd->m_BakedCmdListInfo[BakedCommandList].type = type;
        m_Cmd->m_BakedCmdListInfo[CommandList].nodeMask =
            m_Cmd->m_BakedCmdListInfo[BakedCommandList].nodeMask = nodeMask;
        m_Cmd->m_BakedCmdListInfo[CommandList].allocator =
            m_Cmd->m_BakedCmdListInfo[BakedCommandList].allocator = GetResID(pAllocator);
        m_Cmd->m_BakedCmdListInfo[CommandList].barriers.clear();
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].barriers.clear();

        // On list execute we increment all child events/actions by
        // m_RootEventID and insert them into the tree.
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].curEventID = 0;
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].eventCount = 0;

        m_Cmd->m_BakedCmdListInfo[BakedCommandList].beginChunk =
            uint32_t(m_Cmd->m_StructuredFile->chunks.size() - 1);

        // reset state
        D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[BakedCommandList].state;
        state = D3D12RenderState();
        state.m_ResourceManager = GetResourceManager();
        state.m_DebugManager = m_pDevice->GetDebugManager();
        state.pipe = GetResID(pInitialState);

        if(state.pipe != ResourceId())
        {
          WrappedID3D12PipelineState *pipe = (WrappedID3D12PipelineState *)pInitialState;
          if(pipe->IsGraphics())
          {
            state.depthBias = pipe->graphics->RasterizerState.DepthBias;
            state.depthBiasClamp = pipe->graphics->RasterizerState.DepthBiasClamp;
            state.slopeScaledDepthBias = pipe->graphics->RasterizerState.SlopeScaledDepthBias;
            state.cutValue = pipe->graphics->IBStripCutValue;
          }
        }
      }
    }
  }

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator,
                                                ID3D12PipelineState *pInitialState)
{
  return ResetInternal(pAllocator, pInitialState, false);
}

HRESULT WrappedID3D12GraphicsCommandList::ResetInternal(ID3D12CommandAllocator *pAllocator,
                                                        ID3D12PipelineState *pInitialState,
                                                        bool fakeCreationReset)
{
  HRESULT ret = S_OK;

  if(IsCaptureMode(m_State))
  {
    m_ListRecord->DisableChunkLocking();

    // reset for new recording
    m_ListRecord->DeleteChunks();
    m_ListRecord->ContainsExecuteIndirect = false;

    // release the 'persistent' reference on all these buffers immediately. If this list was never
    // submitted, this immediately frees the buffer. If it was submitted those submissions will be
    // holding references until their fences are appropriately signalled.
    for(PatchedRayDispatch::Resources &r : m_RayDispatches)
    {
      r.Release();
    }
    m_RayDispatches.clear();

    m_ImmediateCallbacks.clear();
    m_PendingCallbacks.clear();

    for(std::function<void()> &func : m_UnusedCleanupCallbacks)
      func();
    m_UnusedCleanupCallbacks.clear();

    m_CaptureComputeState = D3D12RenderState();
    m_CaptureComputeState.m_ResourceManager = GetResourceManager();

    // free any baked commands.
    if(m_ListRecord->bakedCommands)
      m_ListRecord->bakedCommands->Delete(GetResourceManager());

    // If this reset is 'fake' to record the initial allocator and state. Don't actually call
    // Reset(), just pretend it was so that we can pretend D3D12 doesn't have weird behaviour.
    if(!fakeCreationReset)
    {
      SERIALISE_TIME_CALL(ret = m_pList->Reset(Unwrap(pAllocator), Unwrap(pInitialState)));
    }

    m_ListRecord->bakedCommands =
        GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_ListRecord->bakedCommands->DisableChunkLocking();
    m_ListRecord->bakedCommands->type = Resource_GraphicsCommandList;
    m_ListRecord->bakedCommands->InternalResource = true;
    m_ListRecord->bakedCommands->cmdInfo = new CmdListRecordingInfo();

    m_ListRecord->cmdInfo->allocRecord = GetRecord(pAllocator);
    m_ListRecord->cmdInfo->alloc = m_ListRecord->cmdInfo->allocRecord->cmdInfo->alloc;

    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Reset);
      Serialise_Reset(ser, pAllocator, pInitialState);

      m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    }

    // add allocator and initial state (if there is one) as frame refs. We can't add
    // them as parents of the list record because it won't get directly referenced
    // (just the baked commands), and we can't parent them onto the baked commands
    // because that would pull them into the capture section.
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pAllocator), eFrameRef_Read);
    if(pInitialState)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(pInitialState), eFrameRef_Read);
  }
  else
  {
    ret = m_pList->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
    CHECK_HR(m_pDevice, ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ResourceBarrier(
    SerialiserType &ser, UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumBarriers);
  SERIALISE_ELEMENT_ARRAY(pBarriers, NumBarriers).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    rdcarray<D3D12_RESOURCE_BARRIER> filtered;
    {
      filtered.reserve(NumBarriers);

      // non-transition barriers allow NULLs, but for transition barriers filter out any that
      // reference the NULL resource - this means the resource wasn't used elsewhere so was
      // discarded from the capture
      for(UINT i = 0; i < NumBarriers; i++)
      {
        if(pBarriers[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
           pBarriers[i].Transition.pResource)
        {
          filtered.push_back(pBarriers[i]);

          // unwrap it
          D3D12_RESOURCE_BARRIER &b = filtered.back();

          ID3D12Resource *res1 = NULL, *res2 = NULL;

          if(b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
          {
            res1 = b.Transition.pResource;
            b.Transition.pResource = Unwrap(b.Transition.pResource);
          }
          else if(b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
          {
            res1 = b.Aliasing.pResourceBefore;
            res2 = b.Aliasing.pResourceAfter;
            b.Aliasing.pResourceBefore = Unwrap(b.Aliasing.pResourceBefore);
            b.Aliasing.pResourceAfter = Unwrap(b.Aliasing.pResourceAfter);
          }
          else if(b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
          {
            res1 = b.UAV.pResource;
            b.UAV.pResource = Unwrap(b.UAV.pResource);
          }

          if(IsLoading(m_State) && (res1 || res2))
          {
            D3D12EventNode &eventNode = m_Cmd->m_LoadingEventNode;

            if(res1)
            {
              eventNode.resourceUsage.push_back(make_rdcpair(GetResID(res1), ResourceUsage::Barrier));
            }
            if(res2)
            {
              eventNode.resourceUsage.push_back(make_rdcpair(GetResID(res2), ResourceUsage::Barrier));
            }
          }
        }
      }
    }

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        pCommandList = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        if(!filtered.empty())
          Unwrap(pCommandList)->ResourceBarrier((UINT)filtered.size(), &filtered[0]);
      }
      else
      {
        pCommandList = NULL;
      }
    }
    else
    {
      if(!filtered.empty())
        Unwrap(pCommandList)->ResourceBarrier((UINT)filtered.size(), &filtered[0]);
    }

    if(pCommandList)
    {
      ResourceId cmd = GetResID(pCommandList);

      for(UINT i = 0; i < NumBarriers; i++)
      {
        if(pBarriers[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
           pBarriers[i].Transition.pResource)
        {
          m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].barriers.barriers.push_back(pBarriers[i]);
          m_Cmd->m_BakedCmdListInfo[cmd].barriers.barriers.push_back(pBarriers[i]);
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ResourceBarrier(UINT NumBarriers,
                                                       const D3D12_RESOURCE_BARRIER *pBarriers)
{
  D3D12_RESOURCE_BARRIER *barriers = m_pDevice->GetTempArray<D3D12_RESOURCE_BARRIER>(NumBarriers);

  for(UINT i = 0; i < NumBarriers; i++)
  {
    barriers[i] = pBarriers[i];

    if(barriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
    {
      barriers[i].Transition.pResource = Unwrap(barriers[i].Transition.pResource);
    }
    else if(barriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
    {
      barriers[i].Aliasing.pResourceBefore = Unwrap(barriers[i].Aliasing.pResourceBefore);
      barriers[i].Aliasing.pResourceAfter = Unwrap(barriers[i].Aliasing.pResourceAfter);
    }
    else if(barriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
    {
      barriers[i].UAV.pResource = Unwrap(barriers[i].UAV.pResource);
    }
  }

  SERIALISE_TIME_CALL(m_pList->ResourceBarrier(NumBarriers, barriers));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ResourceBarrier);
    Serialise_ResourceBarrier(ser, NumBarriers, pBarriers);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    m_ListRecord->cmdInfo->barriers.barriers.append(pBarriers, NumBarriers);
  }
}

#pragma region State Setting

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ClearState(SerialiserType &ser,
                                                            ID3D12PipelineState *pPipelineState)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pPipelineState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->ClearState(Unwrap(pPipelineState));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->ClearState(Unwrap(pPipelineState));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state = D3D12RenderState();
      state.m_DebugManager = m_pDevice->GetDebugManager();
      state.m_ResourceManager = m_pDevice->GetResourceManager();
      state.pipe = GetResID(pPipelineState);

      if(state.pipe != ResourceId())
      {
        WrappedID3D12PipelineState *pipe = (WrappedID3D12PipelineState *)pPipelineState;
        if(pipe->IsGraphics())
        {
          state.depthBias = pipe->graphics->RasterizerState.DepthBias;
          state.depthBiasClamp = pipe->graphics->RasterizerState.DepthBiasClamp;
          state.slopeScaledDepthBias = pipe->graphics->RasterizerState.SlopeScaledDepthBias;
          state.cutValue = pipe->graphics->IBStripCutValue;
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
  SERIALISE_TIME_CALL(m_pList->ClearState(Unwrap(pPipelineState)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearState);
    Serialise_ClearState(ser, pPipelineState);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pPipelineState), eFrameRef_Read);

    m_CaptureComputeState = D3D12RenderState();
    m_CaptureComputeState.m_ResourceManager = GetResourceManager();
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetPrimitiveTopology(
    SerialiserType &ser, D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(PrimitiveTopology).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->IASetPrimitiveTopology(PrimitiveTopology);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->IASetPrimitiveTopology(PrimitiveTopology);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.topo = PrimitiveTopology;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  SERIALISE_TIME_CALL(m_pList->IASetPrimitiveTopology(PrimitiveTopology));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_IASetPrimitiveTopology);
    Serialise_IASetPrimitiveTopology(ser, PrimitiveTopology);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_RSSetViewports(SerialiserType &ser,
                                                                UINT NumViewports,
                                                                const D3D12_VIEWPORT *pViewports)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumViewports);
  SERIALISE_ELEMENT_ARRAY(pViewports, NumViewports).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->RSSetViewports(NumViewports, pViewports);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->RSSetViewports(NumViewports, pViewports);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.views.size() < NumViewports)
        state.views.resize(NumViewports);

      for(UINT i = 0; i < NumViewports; i++)
        state.views[i] = pViewports[i];
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::RSSetViewports(UINT NumViewports,
                                                      const D3D12_VIEWPORT *pViewports)
{
  SERIALISE_TIME_CALL(m_pList->RSSetViewports(NumViewports, pViewports));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_RSSetViewports);
    Serialise_RSSetViewports(ser, NumViewports, pViewports);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_RSSetScissorRects(SerialiserType &ser, UINT NumRects,
                                                                   const D3D12_RECT *pRects)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->RSSetScissorRects(NumRects, pRects);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->RSSetScissorRects(NumRects, pRects);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.scissors.size() < NumRects)
        state.scissors.resize(NumRects);

      for(UINT i = 0; i < NumRects; i++)
        state.scissors[i] = pRects[i];
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_TIME_CALL(m_pList->RSSetScissorRects(NumRects, pRects));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_RSSetScissorRects);
    Serialise_RSSetScissorRects(ser, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetBlendFactor(SerialiserType &ser,
                                                                  const FLOAT BlendFactor[4])
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_ARRAY(BlendFactor, 4).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->OMSetBlendFactor(BlendFactor);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->OMSetBlendFactor(BlendFactor);

      stateUpdate = true;
    }

    if(stateUpdate)
      memcpy(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.blendFactor, BlendFactor,
             sizeof(float) * 4);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
  SERIALISE_TIME_CALL(m_pList->OMSetBlendFactor(BlendFactor));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_OMSetBlendFactor);
    Serialise_OMSetBlendFactor(ser, BlendFactor);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetStencilRef(SerialiserType &ser, UINT StencilRef)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(StencilRef).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->OMSetStencilRef(StencilRef);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->OMSetStencilRef(StencilRef);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &rs = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;
      rs.stencilRefFront = rs.stencilRefBack = StencilRef;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
  SERIALISE_TIME_CALL(m_pList->OMSetStencilRef(StencilRef));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_OMSetStencilRef);
    Serialise_OMSetStencilRef(ser, StencilRef);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetDescriptorHeaps(
    SerialiserType &ser, UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumDescriptorHeaps);
  SERIALISE_ELEMENT_ARRAY(ppDescriptorHeaps, NumDescriptorHeaps).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    rdcarray<ResourceId> heapIDs;
    rdcarray<ID3D12DescriptorHeap *> heaps;
    heaps.resize(NumDescriptorHeaps);
    heapIDs.resize(heaps.size());
    for(size_t i = 0; i < heaps.size(); i++)
    {
      heapIDs[i] = GetResID(ppDescriptorHeaps[i]);
      heaps[i] = Unwrap(ppDescriptorHeaps[i]);
    }

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetDescriptorHeaps(NumDescriptorHeaps, heaps.data());

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetDescriptorHeaps(NumDescriptorHeaps, heaps.data());

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.heaps = heapIDs;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps,
                                                          ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  ID3D12DescriptorHeap **heaps = m_pDevice->GetTempArray<ID3D12DescriptorHeap *>(NumDescriptorHeaps);
  for(UINT i = 0; i < NumDescriptorHeaps; i++)
    heaps[i] = Unwrap(ppDescriptorHeaps[i]);

  SERIALISE_TIME_CALL(m_pList->SetDescriptorHeaps(NumDescriptorHeaps, heaps));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetDescriptorHeaps);
    Serialise_SetDescriptorHeaps(ser, NumDescriptorHeaps, ppDescriptorHeaps);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    for(UINT i = 0; i < NumDescriptorHeaps; i++)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(ppDescriptorHeaps[i]), eFrameRef_Read);

    m_CaptureComputeState.heaps.resize(NumDescriptorHeaps);
    for(size_t i = 0; i < m_CaptureComputeState.heaps.size(); i++)
      m_CaptureComputeState.heaps[i] = GetResID(ppDescriptorHeaps[i]);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetIndexBuffer(SerialiserType &ser,
                                                                  const D3D12_INDEX_BUFFER_VIEW *pView)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_OPT(pView).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->IASetIndexBuffer(pView);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      ID3D12GraphicsCommandList *list = pCommandList;

      list->IASetIndexBuffer(pView);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(pView)
      {
        WrappedID3D12Resource::GetResIDFromAddr(pView->BufferLocation, state.ibuffer.buf,
                                                state.ibuffer.offs);
        state.ibuffer.bytewidth = (pView->Format == DXGI_FORMAT_R32_UINT ? 4 : 2);
        state.ibuffer.size = pView->SizeInBytes;
      }
      else
      {
        state.ibuffer.buf = ResourceId();
        state.ibuffer.offs = 0;
        state.ibuffer.bytewidth = 2;
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
  SERIALISE_TIME_CALL(m_pList->IASetIndexBuffer(pView));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_IASetIndexBuffer);
    Serialise_IASetIndexBuffer(ser, pView);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    if(pView)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pView->BufferLocation), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetVertexBuffers(
    SerialiserType &ser, UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(StartSlot).Important();
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(pViews, NumViews).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->IASetVertexBuffers(StartSlot, NumViews, pViews);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->IASetVertexBuffers(StartSlot, NumViews, pViews);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.vbuffers.size() < StartSlot + NumViews)
        state.vbuffers.resize(StartSlot + NumViews);

      for(UINT i = 0; i < NumViews; i++)
      {
        WrappedID3D12Resource::GetResIDFromAddr(pViews ? pViews[i].BufferLocation : 0,
                                                state.vbuffers[StartSlot + i].buf,
                                                state.vbuffers[StartSlot + i].offs);

        state.vbuffers[StartSlot + i].stride = pViews ? pViews[i].StrideInBytes : 0;
        state.vbuffers[StartSlot + i].size = pViews ? pViews[i].SizeInBytes : 0;
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews,
                                                          const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  SERIALISE_TIME_CALL(m_pList->IASetVertexBuffers(StartSlot, NumViews, pViews));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_IASetVertexBuffers);
    Serialise_IASetVertexBuffers(ser, StartSlot, NumViews, pViews);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    for(UINT i = 0; pViews && i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SOSetTargets(
    SerialiserType &ser, UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(StartSlot).Important();
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(pViews, NumViews).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SOSetTargets(StartSlot, NumViews, pViews);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SOSetTargets(StartSlot, NumViews, pViews);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.streamouts.size() < StartSlot + NumViews)
        state.streamouts.resize(StartSlot + NumViews);

      for(UINT i = 0; i < NumViews; i++)
      {
        D3D12RenderState::StreamOut &so = state.streamouts[StartSlot + i];

        WrappedID3D12Resource::GetResIDFromAddr(pViews ? pViews[i].BufferLocation : 0, so.buf,
                                                so.offs);

        WrappedID3D12Resource::GetResIDFromAddr(pViews ? pViews[i].BufferFilledSizeLocation : 0,
                                                so.countbuf, so.countoffs);

        so.size = pViews ? pViews[i].SizeInBytes : 0;
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews,
                                                    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  SERIALISE_TIME_CALL(m_pList->SOSetTargets(StartSlot, NumViews, pViews));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SOSetTargets);
    Serialise_SOSetTargets(ser, StartSlot, NumViews, pViews);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    for(UINT i = 0; pViews && i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetPipelineState(SerialiserType &ser,
                                                                  ID3D12PipelineState *pPipelineState)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pPipelineState).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SetPipelineState(Unwrap(pPipelineState));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetPipelineState(Unwrap(pPipelineState));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;
      state.pipe = GetResID(pPipelineState);
      state.stateobj = ResourceId();

      if(pPipelineState)
      {
        WrappedID3D12PipelineState *pipe = (WrappedID3D12PipelineState *)pPipelineState;
        if(pipe->IsGraphics())
        {
          state.depthBias = pipe->graphics->RasterizerState.DepthBias;
          state.depthBiasClamp = pipe->graphics->RasterizerState.DepthBiasClamp;
          state.slopeScaledDepthBias = pipe->graphics->RasterizerState.SlopeScaledDepthBias;
          state.cutValue = pipe->graphics->IBStripCutValue;
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState *pPipelineState)
{
  SERIALISE_TIME_CALL(m_pList->SetPipelineState(Unwrap(pPipelineState)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetPipelineState);
    Serialise_SetPipelineState(ser, pPipelineState);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pPipelineState), eFrameRef_Read);

    m_CaptureComputeState.pipe = GetResID(pPipelineState);
    m_CaptureComputeState.stateobj = ResourceId();
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetRenderTargets(
    SerialiserType &ser, UINT NumRenderTargetDescriptors,
    const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumRenderTargetDescriptors);

  rdcarray<D3D12Descriptor> RTVs;

  if(ser.VersionAtLeast(0x5))
  {
    if(ser.IsWriting())
    {
      if(RTsSingleHandleToDescriptorRange)
      {
        if(pRenderTargetDescriptors && NumRenderTargetDescriptors > 0)
        {
          const D3D12Descriptor *descs = GetWrapped(pRenderTargetDescriptors[0]);

          RTVs.assign(descs, NumRenderTargetDescriptors);
        }
      }
      else
      {
        for(UINT i = 0; i < NumRenderTargetDescriptors; i++)
          RTVs.push_back(*GetWrapped(pRenderTargetDescriptors[i]));
      }
    }

    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately
    SERIALISE_ELEMENT(RTVs).Named("pRenderTargetDescriptors"_lit).Important();
  }
  else
  {
    // in this case just make the number of descriptors important
    ser.Important();

    // this path is only used during reading, since during writing we're implicitly on the newest
    // version above. We start with numHandles initialised to 0, as the array count is not used on
    // reading (it's filled in), then we calculate it below after having serialised
    // RTsSingleHandleToDescriptorRange
    UINT numHandles = 0;
    SERIALISE_ELEMENT_ARRAY(pRenderTargetDescriptors, numHandles);
    SERIALISE_ELEMENT_TYPED(bool, RTsSingleHandleToDescriptorRange);

    numHandles = RTsSingleHandleToDescriptorRange ? RDCMIN(1U, NumRenderTargetDescriptors)
                                                  : NumRenderTargetDescriptors;

    if(IsReplayingAndReading())
    {
      if(RTsSingleHandleToDescriptorRange)
      {
        if(pRenderTargetDescriptors && NumRenderTargetDescriptors > 0)
        {
          const D3D12Descriptor *descs = GetWrapped(pRenderTargetDescriptors[0]);

          RTVs.assign(descs, NumRenderTargetDescriptors);
        }
      }
      else
      {
        for(UINT h = 0; h < numHandles; h++)
        {
          RTVs.push_back(*GetWrapped(pRenderTargetDescriptors[h]));
        }
      }
    }
  }

  D3D12Descriptor DSV;

  if(ser.VersionAtLeast(0x5))
  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately.
    const D3D12Descriptor *pDSV = NULL;

    if(ser.IsWriting())
      pDSV = pDepthStencilDescriptor ? GetWrapped(*pDepthStencilDescriptor) : NULL;

    SERIALISE_ELEMENT_OPT(pDSV).Named("pDepthStencilDescriptor"_lit);

    if(pDSV)
      DSV = *pDSV;
  }
  else
  {
    SERIALISE_ELEMENT_OPT(pDepthStencilDescriptor);

    if(IsReplayingAndReading())
    {
      if(pDepthStencilDescriptor)
        DSV = *GetWrapped(*pDepthStencilDescriptor);
    }
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    rdcarray<D3D12_CPU_DESCRIPTOR_HANDLE> unwrappedRTs;
    unwrappedRTs.resize(RTVs.size());
    for(size_t i = 0; i < RTVs.size(); i++)
    {
      unwrappedRTs[i] = Unwrap(m_pDevice->GetDebugManager()->GetTempDescriptor(RTVs[i], i));
    }

    D3D12_CPU_DESCRIPTOR_HANDLE unwrappedDSV = {};
    if(DSV.GetResResourceId() != ResourceId())
    {
      unwrappedDSV = Unwrap(m_pDevice->GetDebugManager()->GetTempDescriptor(DSV));
    }

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->OMSetRenderTargets((UINT)unwrappedRTs.size(), unwrappedRTs.data(), FALSE,
                                 unwrappedDSV.ptr ? &unwrappedDSV : NULL);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->OMSetRenderTargets((UINT)unwrappedRTs.size(), unwrappedRTs.data(), FALSE,
                               unwrappedDSV.ptr ? &unwrappedDSV : NULL);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.rts = RTVs;
      state.dsv = DSV;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetRenderTargets(
    UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  UINT num = NumRenderTargetDescriptors;
  UINT numHandles = RTsSingleHandleToDescriptorRange ? RDCMIN(1U, num) : num;
  D3D12_CPU_DESCRIPTOR_HANDLE *unwrapped =
      m_pDevice->GetTempArray<D3D12_CPU_DESCRIPTOR_HANDLE>(numHandles);
  for(UINT i = 0; i < numHandles; i++)
    unwrapped[i] = Unwrap(pRenderTargetDescriptors[i]);

  D3D12_CPU_DESCRIPTOR_HANDLE dsv =
      pDepthStencilDescriptor ? Unwrap(*pDepthStencilDescriptor) : D3D12_CPU_DESCRIPTOR_HANDLE();

  SERIALISE_TIME_CALL(m_pList->OMSetRenderTargets(num, unwrapped, RTsSingleHandleToDescriptorRange,
                                                  pDepthStencilDescriptor ? &dsv : NULL));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_OMSetRenderTargets);
    Serialise_OMSetRenderTargets(ser, num, pRenderTargetDescriptors,
                                 RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    if(RTsSingleHandleToDescriptorRange)
    {
      D3D12Descriptor *desc =
          (NumRenderTargetDescriptors == 0) ? NULL : GetWrapped(pRenderTargetDescriptors[0]);
      for(UINT i = 0; i < NumRenderTargetDescriptors; i++)
      {
        m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
        m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);
        desc++;
      }
    }
    else
    {
      for(UINT i = 0; i < NumRenderTargetDescriptors; i++)
      {
        D3D12Descriptor *desc = GetWrapped(pRenderTargetDescriptors[i]);
        m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
        m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);
      }
    }

    if(pDepthStencilDescriptor)
    {
      D3D12Descriptor *desc = GetWrapped(*pDepthStencilDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);
    }
  }
}

#pragma endregion State Setting

#pragma region Compute Root Signatures

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootSignature(
    SerialiserType &ser, ID3D12RootSignature *pRootSignature)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pRootSignature).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootSignature(Unwrap(pRootSignature));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootSignature(Unwrap(pRootSignature));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      // From the docs
      // (https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#command-list-semantics)
      // "If a root signature is changed on a command list, all previous root arguments become stale
      // and all newly expected arguments must be set before Draw/Dispatch otherwise behavior is
      // undefined. If the root signature is redundantly set to the same one currently set, existing
      // root signature bindings do not become stale."
      if(Unwrap(GetResourceManager()->GetResAs<ID3D12RootSignature>(state.compute.rootsig)) !=
         Unwrap(pRootSignature))
        state.compute.sigelems.clear();
      state.compute.rootsig = GetResID(pRootSignature);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature *pRootSignature)
{
  SERIALISE_TIME_CALL(m_pList->SetComputeRootSignature(Unwrap(pRootSignature)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRootSignature);
    Serialise_SetComputeRootSignature(ser, pRootSignature);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pRootSignature), eFrameRef_Read);

    // store this so we can look up how many descriptors a given slot references, etc
    m_CurCompRootSig = GetWrapped(pRootSignature);

    // From the docs
    // (https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#command-list-semantics)
    // "If a root signature is changed on a command list, all previous root arguments become stale
    // and all newly expected arguments must be set before Draw/Dispatch otherwise behavior is
    // undefined. If the root signature is redundantly set to the same one currently set, existing
    // root signature bindings do not become stale."
    if(Unwrap(GetResourceManager()->GetResAs<ID3D12RootSignature>(
           m_CaptureComputeState.compute.rootsig)) != Unwrap(pRootSignature))
      m_CaptureComputeState.compute.sigelems.clear();
    m_CaptureComputeState.compute.rootsig = GetResID(pRootSignature);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootDescriptorTable(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT(BaseDescriptor).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.compute.sigelems.resize_for_index(RootParameterIndex);
      state.compute.sigelems[RootParameterIndex] = D3D12RenderState::SignatureElement(
          eRootTable, GetWrapped(BaseDescriptor)->GetHeapResourceId(),
          (UINT64)GetWrapped(BaseDescriptor)->GetHeapIndex());
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  SERIALISE_TIME_CALL(
      m_pList->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRootDescriptorTable);
    Serialise_SetComputeRootDescriptorTable(ser, RootParameterIndex, BaseDescriptor);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                              eFrameRef_Read);

    {
      m_CaptureComputeState.compute.sigelems.resize_for_index(RootParameterIndex);
      m_CaptureComputeState.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootTable,
                                             GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                             (UINT64)GetWrapped(BaseDescriptor)->GetHeapIndex());
    }

    rdcarray<D3D12_DESCRIPTOR_RANGE1> &ranges =
        GetWrapped(m_CurCompRootSig)->sig.Parameters[RootParameterIndex].ranges;

    D3D12Descriptor *base = GetWrapped(BaseDescriptor);
    UINT HeapNumDescriptors = base->GetHeap()->GetNumDescriptors();

    UINT prevTableOffset = 0;

    for(size_t i = 0; i < ranges.size(); i++)
    {
      D3D12Descriptor *rangeStart = base;

      UINT offset = ranges[i].OffsetInDescriptorsFromTableStart;

      if(ranges[i].OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
        offset = prevTableOffset;

      rangeStart += offset;

      UINT num = ranges[i].NumDescriptors;

      if(num == UINT_MAX)
      {
        // find out how many descriptors are left after rangeStart
        num = HeapNumDescriptors - rangeStart->GetHeapIndex();
      }

      if(!m_pDevice->IsBindlessResourceUseActive())
      {
        rdcarray<rdcpair<D3D12Descriptor *, UINT>> &descs = m_ListRecord->cmdInfo->boundDescs;
        descs.push_back(make_rdcpair(rangeStart, num));
      }

      prevTableOffset = offset + num;
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRoot32BitConstant(
    SerialiserType &ser, UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT(SrcData).Important();
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.compute.sigelems.resize_for_index(RootParameterIndex);
      state.compute.sigelems[RootParameterIndex].SetConstant(DestOffsetIn32BitValues, SrcData);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex,
                                                                   UINT SrcData,
                                                                   UINT DestOffsetIn32BitValues)
{
  SERIALISE_TIME_CALL(
      m_pList->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRoot32BitConstant);
    Serialise_SetComputeRoot32BitConstant(ser, RootParameterIndex, SrcData, DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    {
      m_CaptureComputeState.compute.sigelems.resize_for_index(RootParameterIndex);
      m_CaptureComputeState.compute.sigelems[RootParameterIndex].SetConstant(
          DestOffsetIn32BitValues, SrcData);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRoot32BitConstants(
    SerialiserType &ser, UINT RootParameterIndex, UINT Num32BitValuesToSet,
    const void *pSrcVoidData, UINT DestOffsetIn32BitValues)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT(Num32BitValuesToSet);
  const UINT *pSrcData = (const UINT *)pSrcVoidData;
  SERIALISE_ELEMENT_ARRAY(pSrcData, Num32BitValuesToSet).Important();
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    UINT dummyData;
    // nVidia driver crashes if pSrcData is NULL even with Num32BitValuesToSet = 0
    const UINT *pValidSrcData = (Num32BitValuesToSet > 0) ? pSrcData : &dummyData;
    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pValidSrcData,
                                           DestOffsetIn32BitValues);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pValidSrcData,
                                         DestOffsetIn32BitValues);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.compute.sigelems.resize_for_index(RootParameterIndex);
      state.compute.sigelems[RootParameterIndex].SetConstants(Num32BitValuesToSet, pValidSrcData,
                                                              DestOffsetIn32BitValues);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex,
                                                                    UINT Num32BitValuesToSet,
                                                                    const void *pSrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  // nVidia driver crashes if pSrcData is NULL even with Num32BitValuesToSet = 0
  UINT dummyData;
  const void *pValidSrcData = Num32BitValuesToSet > 0 ? pSrcData : &dummyData;
  SERIALISE_TIME_CALL(m_pList->SetComputeRoot32BitConstants(
      RootParameterIndex, Num32BitValuesToSet, pValidSrcData, DestOffsetIn32BitValues));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRoot32BitConstants);
    Serialise_SetComputeRoot32BitConstants(ser, RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                           DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    {
      m_CaptureComputeState.compute.sigelems.resize_for_index(RootParameterIndex);
      m_CaptureComputeState.compute.sigelems[RootParameterIndex].SetConstants(
          Num32BitValuesToSet, pValidSrcData, DestOffsetIn32BitValues);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootConstantBufferView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.compute.sigelems.resize_for_index(RootParameterIndex);
      state.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootCBV, id, offs);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_TIME_CALL(m_pList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRootConstantBufferView);
    Serialise_SetComputeRootConstantBufferView(ser, RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);

    {
      m_CaptureComputeState.compute.sigelems.resize_for_index(RootParameterIndex);
      m_CaptureComputeState.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootCBV, id, offs);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootShaderResourceView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.compute.sigelems.resize_for_index(RootParameterIndex + 1);
      state.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootSRV, id, offs);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_TIME_CALL(m_pList->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRootShaderResourceView);
    Serialise_SetComputeRootShaderResourceView(ser, RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);

    {
      m_CaptureComputeState.compute.sigelems.resize_for_index(RootParameterIndex);
      m_CaptureComputeState.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootSRV, id, offs);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootUnorderedAccessView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.compute.sigelems.resize_for_index(RootParameterIndex);
      state.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootUAV, id, offs);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_TIME_CALL(m_pList->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRootUnorderedAccessView);
    Serialise_SetComputeRootUnorderedAccessView(ser, RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);

    {
      m_CaptureComputeState.compute.sigelems.resize_for_index(RootParameterIndex);
      m_CaptureComputeState.compute.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootUAV, id, offs);
    }
  }
}

#pragma endregion Compute Root Signatures

#pragma region Graphics Root Signatures

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootSignature(
    SerialiserType &ser, ID3D12RootSignature *pRootSignature)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pRootSignature).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootSignature(Unwrap(pRootSignature));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootSignature(Unwrap(pRootSignature));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      // From the docs
      // (https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#command-list-semantics)
      // "If a root signature is changed on a command list, all previous root arguments become stale
      // and all newly expected arguments must be set before Draw/Dispatch otherwise behavior is
      // undefined. If the root signature is redundantly set to the same one currently set, existing
      // root signature bindings do not become stale."
      if(Unwrap(GetResourceManager()->GetResAs<ID3D12RootSignature>(state.graphics.rootsig)) !=
         Unwrap(pRootSignature))
        state.graphics.sigelems.clear();
      state.graphics.rootsig = GetResID(pRootSignature);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature)
{
  SERIALISE_TIME_CALL(m_pList->SetGraphicsRootSignature(Unwrap(pRootSignature)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRootSignature);
    Serialise_SetGraphicsRootSignature(ser, pRootSignature);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pRootSignature), eFrameRef_Read);

    // store this so we can look up how many descriptors a given slot references, etc
    m_CurGfxRootSig = GetWrapped(pRootSignature);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootDescriptorTable(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT(BaseDescriptor).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.graphics.sigelems.resize_for_index(RootParameterIndex);
      state.graphics.sigelems[RootParameterIndex] = D3D12RenderState::SignatureElement(
          eRootTable, GetWrapped(BaseDescriptor)->GetHeapResourceId(),
          (UINT64)GetWrapped(BaseDescriptor)->GetHeapIndex());
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  SERIALISE_TIME_CALL(
      m_pList->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRootDescriptorTable);
    Serialise_SetGraphicsRootDescriptorTable(ser, RootParameterIndex, BaseDescriptor);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                              eFrameRef_Read);

    rdcarray<D3D12_DESCRIPTOR_RANGE1> &ranges =
        GetWrapped(m_CurGfxRootSig)->sig.Parameters[RootParameterIndex].ranges;

    D3D12Descriptor *base = GetWrapped(BaseDescriptor);
    UINT HeapNumDescriptors = base->GetHeap()->GetNumDescriptors();

    UINT prevTableOffset = 0;

    for(size_t i = 0; i < ranges.size(); i++)
    {
      D3D12Descriptor *rangeStart = base;

      UINT offset = ranges[i].OffsetInDescriptorsFromTableStart;

      if(ranges[i].OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
        offset = prevTableOffset;

      rangeStart += offset;

      UINT num = ranges[i].NumDescriptors;

      if(num == UINT_MAX)
      {
        // find out how many descriptors are left after rangeStart
        num = HeapNumDescriptors - rangeStart->GetHeapIndex();
      }

      if(!m_pDevice->IsBindlessResourceUseActive())
      {
        rdcarray<rdcpair<D3D12Descriptor *, UINT>> &descs = m_ListRecord->cmdInfo->boundDescs;
        descs.push_back(make_rdcpair(rangeStart, num));
      }

      prevTableOffset = offset + num;
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRoot32BitConstant(
    SerialiserType &ser, UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT(SrcData).Important();
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.graphics.sigelems.resize_for_index(RootParameterIndex);
      state.graphics.sigelems[RootParameterIndex].SetConstant(DestOffsetIn32BitValues, SrcData);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex,
                                                                    UINT SrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  SERIALISE_TIME_CALL(
      m_pList->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRoot32BitConstant);
    Serialise_SetGraphicsRoot32BitConstant(ser, RootParameterIndex, SrcData, DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRoot32BitConstants(
    SerialiserType &ser, UINT RootParameterIndex, UINT Num32BitValuesToSet,
    const void *pSrcVoidData, UINT DestOffsetIn32BitValues)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT(Num32BitValuesToSet);
  const UINT *pSrcData = (const UINT *)pSrcVoidData;
  SERIALISE_ELEMENT_ARRAY(pSrcData, Num32BitValuesToSet).Important();
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    UINT dummyData;
    // nVidia driver crashes if pSrcData is NULL even with Num32BitValuesToSet = 0
    const UINT *pValidSrcData = (Num32BitValuesToSet > 0) ? pSrcData : &dummyData;
    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pValidSrcData,
                                            DestOffsetIn32BitValues);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pValidSrcData,
                                          DestOffsetIn32BitValues);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.graphics.sigelems.resize_for_index(RootParameterIndex);
      state.graphics.sigelems[RootParameterIndex].SetConstants(Num32BitValuesToSet, pValidSrcData,
                                                               DestOffsetIn32BitValues);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex,
                                                                     UINT Num32BitValuesToSet,
                                                                     const void *pSrcData,
                                                                     UINT DestOffsetIn32BitValues)
{
  // nVidia driver crashes if pSrcData is NULL even with Num32BitValuesToSet = 0
  UINT dummyData;
  const void *pValidSrcData = Num32BitValuesToSet > 0 ? pSrcData : &dummyData;
  SERIALISE_TIME_CALL(m_pList->SetGraphicsRoot32BitConstants(
      RootParameterIndex, Num32BitValuesToSet, pValidSrcData, DestOffsetIn32BitValues));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRoot32BitConstants);
    Serialise_SetGraphicsRoot32BitConstants(ser, RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                            DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootConstantBufferView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.graphics.sigelems.resize_for_index(RootParameterIndex);
      state.graphics.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootCBV, id, offs);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_TIME_CALL(m_pList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRootConstantBufferView);
    Serialise_SetGraphicsRootConstantBufferView(ser, RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootShaderResourceView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.graphics.sigelems.resize_for_index(RootParameterIndex);
      state.graphics.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootSRV, id, offs);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_TIME_CALL(m_pList->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRootShaderResourceView);
    Serialise_SetGraphicsRootShaderResourceView(ser, RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootUnorderedAccessView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex).Important();
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.graphics.sigelems.resize_for_index(RootParameterIndex);
      state.graphics.sigelems[RootParameterIndex] =
          D3D12RenderState::SignatureElement(eRootUAV, id, offs);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  SERIALISE_TIME_CALL(m_pList->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRootUnorderedAccessView);
    Serialise_SetGraphicsRootUnorderedAccessView(ser, RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

#pragma endregion Graphics Root Signatures

#pragma region Queries / Events

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_BeginQuery(SerialiserType &ser,
                                                            ID3D12QueryHeap *pQueryHeap,
                                                            D3D12_QUERY_TYPE Type, UINT Index)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pQueryHeap).Important();
  SERIALISE_ELEMENT(Type).Important();
  SERIALISE_ELEMENT(Index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        // don't replay query calls if we're just doing one event, it doesn't do anything
        if(m_Cmd->m_FirstEventID == 1)
        {
          Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
              ->BeginQuery(Unwrap(pQueryHeap), Type, Index);

          m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].m_OutstandingQueries.push_back(
              {pQueryHeap, Type, Index});
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->BeginQuery(Unwrap(pQueryHeap), Type, Index);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap *pQueryHeap,
                                                  D3D12_QUERY_TYPE Type, UINT Index)
{
  SERIALISE_TIME_CALL(m_pList->BeginQuery(Unwrap(pQueryHeap), Type, Index));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_BeginQuery);
    Serialise_BeginQuery(ser, pQueryHeap, Type, Index);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_EndQuery(SerialiserType &ser,
                                                          ID3D12QueryHeap *pQueryHeap,
                                                          D3D12_QUERY_TYPE Type, UINT Index)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pQueryHeap).Important();
  SERIALISE_ELEMENT(Type).Important();
  SERIALISE_ELEMENT(Index).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    WrappedID3D12QueryHeap *queryHeap = (WrappedID3D12QueryHeap *)pQueryHeap;

    // D3D12 requires queries to remain within a command buffer so we don't have to worry about not
    // having seen the corresponding begin
    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        // don't replay query calls if we're doing partial replays, it doesn't do anything
        if(m_Cmd->m_FirstEventID == 1)
        {
          Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
              ->EndQuery(Unwrap(pQueryHeap), Type, Index);

          m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].m_OutstandingQueries.removeOne(
              {pQueryHeap, Type, Index});
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->EndQuery(Unwrap(pQueryHeap), Type, Index);

      // during replay store which queries are issued in the capture itself, so we know which ones
      // we can do a 'real' resolve of and which ones must be faked from initial contents if they
      // refer to queries from previous frames.
      // see the comment in ResolveQueryData for more information
      queryHeap->SetQueryValid(Index, Type);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                                UINT Index)
{
  SERIALISE_TIME_CALL(m_pList->EndQuery(Unwrap(pQueryHeap), Type, Index));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_EndQuery);
    Serialise_EndQuery(ser, pQueryHeap, Type, Index);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);

    // during capture store which queries have been issued so we know which ones we can resolve for initial contents
    WrappedID3D12QueryHeap *queryHeap = (WrappedID3D12QueryHeap *)pQueryHeap;
    AddSubmissionASBuildCallback(
        false,
        [queryHeap, Index, Type]() {
          queryHeap->SetQueryValid(Index, Type);
          return true;
        },
        NULL);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ResolveQueryData(
    SerialiserType &ser, ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex,
    UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pQueryHeap).Important();
  SERIALISE_ELEMENT(Type).Important();
  SERIALISE_ELEMENT(StartIndex);
  SERIALISE_ELEMENT(NumQueries);
  SERIALISE_ELEMENT(pDestinationBuffer).Important();
  SERIALISE_ELEMENT(AlignedDestinationBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    WrappedID3D12QueryHeap *queryHeap = (WrappedID3D12QueryHeap *)pQueryHeap;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        // let the query heap decide which indices to resolve normally and which to fake from the
        // stored buffer
        queryHeap->ResolveValidQueryData(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID), Type,
                                         StartIndex, NumQueries, pDestinationBuffer,
                                         AlignedDestinationBufferOffset);
      }
    }
    else
    {
      // don't resolve queries during load, since we can't know for certain at record time whether
      // or not a query will be valid (it could have been queried in a previous frame so not valid
      // to resolve right now, and without knowing the submission order ahead of time we can't
      // always know if a re-record in this capture will happen before this resolve).
      //
      // there are cases we can know this is safe, but we can't detect all cases where it's unsafe, e.g:
      //
      // [previous frame during capture]:
      //   EndEvent(Index)
      //
      // [on replay during captured frame]:
      //   listA->EndEvent(Index)
      //   listB->ResolveQueryData(Index)
      //
      // if listA is submitted first we can resolve normally on listB, but if listB were submitted first
      // we'd need to fake or skip the resolve.
      //
      // What we do is skip resolving during load, then all queries that are ever resolved will have
      // some kind of data. This case above would still return the 'wrong' data as we'd do a normal
      // resolve, when in fact we should fake the resolve to get last frame's data, but at least we
      // won't hit a device lost. In future we could detect this after load once we know the submission order.
      // queryHeap->ResolveQueryData(pCommandList, Type, StartIndex, NumQueries, pDestinationBuffer,
      //                             AlignedDestinationBufferOffset);

      {
        m_Cmd->AddEvent();

        ActionDescription action;

        action.copyDestination = GetResID(pDestinationBuffer);
        action.copyDestinationSubresource = 0;

        action.flags |= ActionFlags::Resolve;

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(
            make_rdcpair(GetResID(pDestinationBuffer), ResourceUsage::ResolveDst));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap *pQueryHeap,
                                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                                        UINT NumQueries,
                                                        ID3D12Resource *pDestinationBuffer,
                                                        UINT64 AlignedDestinationBufferOffset)
{
  SERIALISE_TIME_CALL(m_pList->ResolveQueryData(Unwrap(pQueryHeap), Type, StartIndex, NumQueries,
                                                Unwrap(pDestinationBuffer),
                                                AlignedDestinationBufferOffset));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ResolveQueryData);
    Serialise_ResolveQueryData(ser, pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer,
                               AlignedDestinationBufferOffset);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDestinationBuffer), eFrameRef_PartialWrite);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetPredication(SerialiserType &ser,
                                                                ID3D12Resource *pBuffer,
                                                                UINT64 AlignedBufferOffset,
                                                                D3D12_PREDICATION_OP Operation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pBuffer).Important();
  SERIALISE_ELEMENT(AlignedBufferOffset);
  SERIALISE_ELEMENT(Operation).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    bool stateUpdate = false;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetPredication(Unwrap(pBuffer), AlignedBufferOffset, Operation);

        stateUpdate = true;
      }
      else if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
      {
        stateUpdate = true;
      }
    }
    else
    {
      Unwrap(pCommandList)->SetPredication(Unwrap(pBuffer), AlignedBufferOffset, Operation);

      stateUpdate = true;
    }

    if(stateUpdate)
    {
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.predication.buffer = GetResID(pBuffer);
      state.predication.offset = AlignedBufferOffset;
      state.predication.op = Operation;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetPredication(ID3D12Resource *pBuffer,
                                                      UINT64 AlignedBufferOffset,
                                                      D3D12_PREDICATION_OP Operation)
{
  SERIALISE_TIME_CALL(m_pList->SetPredication(Unwrap(pBuffer), AlignedBufferOffset, Operation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetPredication);
    Serialise_SetPredication(ser, pBuffer, AlignedBufferOffset, Operation);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetMarker(SerialiserType &ser, UINT Metadata,
                                                           const void *pData, UINT Size)
{
  rdcstr MarkerText = "";
  uint64_t Color = 0;

  if(ser.IsWriting() && pData && Size)
    MarkerText = DecodeMarkerString(Metadata, pData, Size, Color);

  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(MarkerText).Important();
  if(ser.VersionAtLeast(0xD))
  {
    SERIALISE_ELEMENT(Color);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        D3D12MarkerRegion::Set(list, MarkerText);
      }
    }
    else
    {
      D3D12MarkerRegion::Set(pCommandList, MarkerText);

      ActionDescription action;
      action.customName = MarkerText.empty() ? "<empty>" : MarkerText;
      if(Color != 0)
      {
        action.markerColor = DecodePIXColor(Color);
      }
      action.flags |= ActionFlags::SetMarker;

      m_Cmd->AddEvent();
      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  SERIALISE_TIME_CALL(m_pList->SetMarker(Metadata, pData, Size));

  if(m_AMDMarkers && Metadata == PIX_EVENT_UNICODE_VERSION)
    m_AMDMarkers->SetMarker(
        StringFormat::Wide2UTF8(rdcwstr((const wchar_t *)pData, Size / sizeof(wchar_t))).c_str());

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetMarker);
    Serialise_SetMarker(ser, Metadata, pData, Size);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_BeginEvent(SerialiserType &ser, UINT Metadata,
                                                            const void *pData, UINT Size)
{
  rdcstr MarkerText = "";
  uint64_t Color = 0;

  if(ser.IsWriting() && pData && Size)
    MarkerText = DecodeMarkerString(Metadata, pData, Size, Color);

  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(MarkerText).Important();
  if(ser.VersionAtLeast(0xD))
  {
    SERIALISE_ELEMENT(Color);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].markerCount++;

        D3D12MarkerRegion::Begin(list, MarkerText);
      }
    }
    else
    {
      D3D12MarkerRegion::Begin(pCommandList, MarkerText);

      ActionDescription action;
      action.customName = MarkerText.empty() ? "<empty>" : MarkerText;
      if(Color != 0)
      {
        action.markerColor = DecodePIXColor(Color);
      }
      action.flags |= ActionFlags::PushMarker;

      m_Cmd->AddEvent();
      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
  SERIALISE_TIME_CALL(m_pList->BeginEvent(Metadata, pData, Size));

  if(m_AMDMarkers && Metadata == PIX_EVENT_UNICODE_VERSION)
    m_AMDMarkers->PushMarker(
        StringFormat::Wide2UTF8(rdcwstr((const wchar_t *)pData, Size / sizeof(wchar_t))).c_str());

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::PushMarker);
    Serialise_BeginEvent(ser, Metadata, pData, Size);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_EndEvent(SerialiserType &ser)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList).Unimportant();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        int &markerCount = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].markerCount;
        markerCount = RDCMAX(0, markerCount - 1);

        D3D12MarkerRegion::End(list);
      }
    }
    else
    {
      D3D12MarkerRegion::End(pCommandList);

      ActionDescription action;
      action.flags = ActionFlags::PopMarker;

      m_Cmd->AddEvent();
      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EndEvent()
{
  SERIALISE_TIME_CALL(m_pList->EndEvent());

  if(m_AMDMarkers)
    m_AMDMarkers->PopMarker();

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::PopMarker);
    Serialise_EndEvent(ser);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetCommandAnnotation(
    SerialiserType &ser, rdcstr key, RENDERDOC_AnnotationType valueType, uint32_t valueVectorWidth,
    RENDERDOC_AnnotationValue value)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList).Unimportant();
  SERIALISE_ELEMENT(key);
  SERIALISE_ELEMENT(valueType);
  ser.SetStructArg(valueType);
  SERIALISE_ELEMENT(valueVectorWidth);
  SERIALISE_ELEMENT(value);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsLoading(m_State))
    {
      if(!m_Cmd->m_RootAnnotation)
        m_Cmd->m_RootAnnotation = new SDObject("Event Annotations"_lit, "Event Annotations"_lit);

      ResourceId cmdId = GetResID(pCommandList);

      PendingAnnotation annot = {0, key, valueType, valueVectorWidth, value};
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].pendingAnnotations.push_back(annot);

      m_pDevice->GetReplay()->WriteFrameRecord().frameInfo.containsAnnotations = true;
    }
  }

  return true;
}

#pragma endregion Queries / Events

#pragma region Draws

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_DrawInstanced(SerialiserType &ser,
                                                               UINT VertexCountPerInstance,
                                                               UINT InstanceCount,
                                                               UINT StartVertexLocation,
                                                               UINT StartInstanceLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(VertexCountPerInstance).Important();
  SERIALISE_ELEMENT(InstanceCount).Important();
  SERIALISE_ELEMENT(StartVertexLocation);
  SERIALISE_ELEMENT(StartInstanceLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Drawcall);
        Unwrap(list)->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                    StartInstanceLocation);
        if(eventId && m_Cmd->m_ActionCallback->PostDraw(eventId, list))
        {
          Unwrap(list)->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                      StartInstanceLocation);
          m_Cmd->m_ActionCallback->PostRedraw(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                          StartInstanceLocation);

      m_Cmd->AddEvent();

      ActionDescription action;
      action.numIndices = VertexCountPerInstance;
      action.numInstances = InstanceCount;
      action.indexOffset = 0;
      action.vertexOffset = StartVertexLocation;
      action.instanceOffset = StartInstanceLocation;

      action.flags |= ActionFlags::Drawcall | ActionFlags::Instanced;

      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance,
                                                     UINT InstanceCount, UINT StartVertexLocation,
                                                     UINT StartInstanceLocation)
{
  SERIALISE_TIME_CALL(m_pList->DrawInstanced(VertexCountPerInstance, InstanceCount,
                                             StartVertexLocation, StartInstanceLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_DrawInstanced);
    Serialise_DrawInstanced(ser, VertexCountPerInstance, InstanceCount, StartVertexLocation,
                            StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_DrawIndexedInstanced(
    SerialiserType &ser, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(IndexCountPerInstance).Important();
  SERIALISE_ELEMENT(InstanceCount).Important();
  SERIALISE_ELEMENT(StartIndexLocation);
  SERIALISE_ELEMENT(BaseVertexLocation);
  SERIALISE_ELEMENT(StartInstanceLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Drawcall);
        Unwrap(list)->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                           BaseVertexLocation, StartInstanceLocation);
        if(eventId && m_Cmd->m_ActionCallback->PostDraw(eventId, list))
        {
          Unwrap(list)->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                             BaseVertexLocation, StartInstanceLocation);
          m_Cmd->m_ActionCallback->PostRedraw(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                 BaseVertexLocation, StartInstanceLocation);

      m_Cmd->AddEvent();

      ActionDescription action;
      action.numIndices = IndexCountPerInstance;
      action.numInstances = InstanceCount;
      action.indexOffset = StartIndexLocation;
      action.baseVertex = BaseVertexLocation;
      action.instanceOffset = StartInstanceLocation;

      action.flags |= ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indexed;

      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance,
                                                            UINT InstanceCount,
                                                            UINT StartIndexLocation,
                                                            INT BaseVertexLocation,
                                                            UINT StartInstanceLocation)
{
  SERIALISE_TIME_CALL(m_pList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount,
                                                    StartIndexLocation, BaseVertexLocation,
                                                    StartInstanceLocation));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_DrawIndexedInstanced);
    Serialise_DrawIndexedInstanced(ser, IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                   BaseVertexLocation, StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_Dispatch(SerialiserType &ser, UINT ThreadGroupCountX,
                                                          UINT ThreadGroupCountY,
                                                          UINT ThreadGroupCountZ)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(ThreadGroupCountX).Important();
  SERIALISE_ELEMENT(ThreadGroupCountY).Important();
  SERIALISE_ELEMENT(ThreadGroupCountZ).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Dispatch);
        Unwrap(list)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
        if(eventId && m_Cmd->m_ActionCallback->PostDispatch(eventId, list))
        {
          Unwrap(list)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
          m_Cmd->m_ActionCallback->PostRedispatch(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

      m_Cmd->AddEvent();

      ActionDescription action;
      action.dispatchDimension[0] = ThreadGroupCountX;
      action.dispatchDimension[1] = ThreadGroupCountY;
      action.dispatchDimension[2] = ThreadGroupCountZ;

      action.flags |= ActionFlags::Dispatch;

      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                                UINT ThreadGroupCountZ)
{
  SERIALISE_TIME_CALL(m_pList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Dispatch);
    Serialise_Dispatch(ser, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ExecuteBundle(SerialiserType &ser,
                                                               ID3D12GraphicsCommandList *pBundle)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pBundle).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pDevice->APIProps.D3D12Bundle = true;

    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::CmdList);
        Unwrap(list)->ExecuteBundle(Unwrap(pBundle));
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::CmdList, list))
        {
          Unwrap(list)->ExecuteBundle(Unwrap(pBundle));
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::CmdList, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->ExecuteBundle(Unwrap(pBundle));

      m_Cmd->AddEvent();

      ActionDescription action;
      action.flags |= ActionFlags::CmdList;

      m_Cmd->AddAction(action);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
  SERIALISE_TIME_CALL(m_pList->ExecuteBundle(Unwrap(pCommandList)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ExecuteBundle);
    Serialise_ExecuteBundle(ser, pCommandList);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    D3D12ResourceRecord *record = GetRecord(pCommandList);

    CmdListRecordingInfo *dst = m_ListRecord->cmdInfo;
    CmdListRecordingInfo *src = record->bakedCommands->cmdInfo;
    dst->boundDescs.append(src->boundDescs);
    dst->dirtied.insert(src->dirtied.begin(), src->dirtied.end());

    dst->bundles.push_back(record);
  }
}

D3D12ExecuteData WrappedID3D12GraphicsCommandList::SaveExecuteIndirectParameters(
    ID3D12GraphicsCommandListX *list, ID3D12CommandSignature *pCommandSignature,
    UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset,
    ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset)
{
  WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)pCommandSignature;

  const size_t argsSize =
      comSig->sig.ByteStride * (RDCMAX(1U, MaxCommandCount) - 1) + comSig->sig.PackedByteSize;
  const size_t countSize = 16;

  // at most we need to copy two executes. The last may be partial and so contain some state set
  // in the previous execute
  ID3D12Resource *buf = NULL;
  uint64_t offs = 0;
  m_Cmd->GetIndirectBuffer(argsSize + countSize, &buf, &offs);

  if(pCountBuffer)
    Unwrap(list)->CopyBufferRegion(Unwrap(buf), offs, Unwrap(pCountBuffer), CountBufferOffset, 4);
  Unwrap(list)->CopyBufferRegion(Unwrap(buf), offs + countSize, Unwrap(pArgumentBuffer),
                                 ArgumentBufferOffset, argsSize);

  D3D12ExecuteData exec = {};
  exec.sig = comSig;
  exec.maxCount = MaxCommandCount;
  // For variable count indirects only allocate up to one indirect command to avoid pessimistic
  // allocation if MaxCommandCount is very high but the actual action count is low.
  exec.reservedCount = pCountBuffer ? RDCMIN(1U, MaxCommandCount) : MaxCommandCount;
  if(pCountBuffer)
  {
    exec.countBuf = buf;
    exec.countOffs = offs;
  }
  exec.argBuf = buf;
  exec.argOffs = offs + 16;

  return exec;
}

void WrappedID3D12GraphicsCommandList::ResetAndRecordExecuteIndirectStates(
    ID3D12GraphicsCommandListX *list, uint32_t baseEventID, uint32_t execCount,
    ID3D12CommandSignature *pCommandSignature, ID3D12Resource *pArgumentBuffer,
    UINT64 ArgumentBufferOffset, uint32_t argumentsReplayed)
{
  WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)pCommandSignature;

  BakedCmdListInfo &cmdListInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];
  D3D12RenderState &state = cmdListInfo.state;

  const uint32_t numArgsPerExec = (uint32_t)comSig->sig.arguments.size();

  if(m_Cmd->m_LastEventID > baseEventID + execCount * comSig->sig.arguments.size() + 1)
  {
    // reset states to 0, we've replayed past this EI
    for(const D3D12_INDIRECT_ARGUMENT_DESC &arg : comSig->sig.arguments)
    {
      switch(arg.Type)
      {
        case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
          if(arg.VertexBuffer.Slot < state.vbuffers.size())
            state.vbuffers[arg.VertexBuffer.Slot] = {};
          break;
        case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW: state.ibuffer = {}; break;
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
          if(arg.Constant.RootParameterIndex < state.graphics.sigelems.size())
          {
            for(uint32_t j = 0; j < arg.Constant.Num32BitValuesToSet; j++)
            {
              size_t index = j + arg.Constant.DestOffsetIn32BitValues;
              state.graphics.sigelems[arg.Constant.RootParameterIndex].constants.resize_for_index(
                  index);
              state.graphics.sigelems[arg.Constant.RootParameterIndex].constants[index] = 0;
            }
          }

          if(arg.Constant.RootParameterIndex < state.compute.sigelems.size())
          {
            for(uint32_t j = 0; j < arg.Constant.Num32BitValuesToSet; j++)
            {
              size_t index = j + arg.Constant.DestOffsetIn32BitValues;
              state.compute.sigelems[arg.Constant.RootParameterIndex].constants.resize_for_index(
                  index);
              state.compute.sigelems[arg.Constant.RootParameterIndex].constants[index] = 0;
            }
          }
          break;
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
          // ConstantBufferView, ShaderResourceView and UnorderedAccessView all have one member -
          // RootParameterIndex
          if(arg.ConstantBufferView.RootParameterIndex < state.graphics.sigelems.size())
          {
            state.graphics.sigelems[arg.ConstantBufferView.RootParameterIndex].id = ResourceId();
            state.graphics.sigelems[arg.ConstantBufferView.RootParameterIndex].offset = 0;
          }

          if(arg.ConstantBufferView.RootParameterIndex < state.compute.sigelems.size())
          {
            state.compute.sigelems[arg.ConstantBufferView.RootParameterIndex].id = ResourceId();
            state.compute.sigelems[arg.ConstantBufferView.RootParameterIndex].offset = 0;
          }
          break;
        default: break;
      }
    }

    return;
  }

  if(m_Cmd->m_LastEventID > baseEventID)
  {
    // at most we need to copy two executes. The last may be partial and so contain some state set
    // in the previous execute
    ID3D12Resource *buf = NULL;
    uint64_t offs = 0;
    m_Cmd->GetIndirectBuffer(comSig->sig.ByteStride + comSig->sig.PackedByteSize, &buf, &offs);

    state.indirectState.argsBuf = buf;
    state.indirectState.argsOffs = offs;
    state.indirectState.comSig = comSig;

    UINT64 BytesToRead = comSig->sig.PackedByteSize;

    if(argumentsReplayed <= numArgsPerExec)
    {
      state.indirectState.argsToProcess = argumentsReplayed;
    }
    else
    {
      state.indirectState.argsToProcess = argumentsReplayed % numArgsPerExec + numArgsPerExec;
      if(argumentsReplayed % numArgsPerExec != 0)
        BytesToRead += comSig->sig.ByteStride;

      // skip all but the last executes we care about
      while(argumentsReplayed > state.indirectState.argsToProcess)
      {
        ArgumentBufferOffset += comSig->sig.ByteStride;
        argumentsReplayed -= numArgsPerExec;
      }
    }

    Unwrap(list)->CopyBufferRegion(Unwrap(buf), offs, Unwrap(pArgumentBuffer), ArgumentBufferOffset,
                                   BytesToRead);

    // this is processed in D3D12RenderState::ResolvePendingIndirectState()
  }
}

void WrappedID3D12GraphicsCommandList::FinaliseExecuteIndirectEvents(BakedCmdListInfo &info,
                                                                     size_t nodeIdx)
{
  rdcarray<D3D12EventNode> &eventNodes = info.eventNodes;
  // Take a copy because eventNodes is likely to be resized
  const D3D12ExecuteData exec = eventNodes[nodeIdx].executeData;

  WrappedID3D12CommandSignature *comSig = exec.sig;

  uint32_t count = exec.maxCount;

  if(exec.countBuf)
  {
    bytebuf data;
    m_pDevice->GetDebugManager()->GetBufferData(exec.countBuf, exec.countOffs, 4, data);

    if(data.size() < sizeof(uint32_t))
      count = 0;
    else
      count = RDCMIN(count, *(uint32_t *)&data[0]);
  }

  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();

  D3D12_RANGE range = {0, D3D12CommandData::m_IndirectSize};
  byte *mapPtr = NULL;
  CHECK_HR(m_pDevice, exec.argBuf->Map(0, &range, (void **)&mapPtr));

  if(m_pDevice->HasFatalError())
    return;

  // patch the name for the base action
  eventNodes[nodeIdx].action.customName =
      StringFormat::Fmt("ExecuteIndirect(maxCount %u, count <%u>)", exec.maxCount, count);

  // move to the first actual event of the commands
  size_t idx = nodeIdx;
  idx++;

  D3D12RenderState state;

  SDChunk *baseChunk = NULL;
  SDFile *sdFile = m_Cmd->m_StructuredFile;

  if(count > 0)
  {
    size_t firstAction = idx;
    // Find the first action (which should have state)
    for(uint32_t a = 0; a < sigSize; ++a)
    {
      if(eventNodes[firstAction].action.actionId == UINT32_MAX)
        break;
      firstAction++;
    }
    RDCASSERT(eventNodes[firstAction].state);
    state = *eventNodes[firstAction].state;

    baseChunk = m_Cmd->m_StructuredFile->chunks[eventNodes[idx].event.chunkIndex];
  }
  // this can be negative if count is 0
  int32_t countExtraActions = count - exec.reservedCount;
  // exec.reservedCount copies of the signature were reserved for the indirect count actions
  // if we ended up with a different number countExtraNodes will be non-zero,
  // we need to adjust and either remove the nodes we allocated (if no actions happened)
  // or clone the nodes to create more that we can then patch.
  if(countExtraActions != 0)
  {
    if(count == 0)
    {
      // idx is the start of the signature nodes
      eventNodes.erase(idx, sigSize);
    }
    else if(countExtraActions > 0)
    {
      size_t baseCommandStart = idx;
      size_t baseCommandEnd = baseCommandStart + sigSize;
      size_t countExtraNodes = countExtraActions * sigSize;

      // We need to clone the signature nodes countExtraNode times
      sdFile->chunks.reserve(sdFile->chunks.size() + countExtraNodes);
      // Insert space for the new nodes
      eventNodes.resize(eventNodes.size() + countExtraNodes);
      // Shift the nodes after the placeholder forwards by countExtraNodes
      for(size_t e = eventNodes.size() - 1; e >= baseCommandEnd + countExtraNodes; e--)
        eventNodes[e] = std::move(eventNodes[e - countExtraNodes]);

      size_t newIdx = baseCommandEnd;
      for(int32_t extra = 0; extra < countExtraActions; ++extra)
      {
        for(uint32_t a = 0; a < sigSize; ++a)
        {
          // duplicate the base node
          eventNodes[newIdx++] = eventNodes[baseCommandStart + a];
        }
      }
    }
  }

  size_t countNodes = eventNodes.size();
  for(uint32_t i = 0; i < count; i++)
  {
    byte *data = mapPtr + exec.argOffs;
    mapPtr += comSig->sig.ByteStride;

    for(uint32_t a = 0; a < sigSize; a++)
    {
      const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[a];

      if(idx >= countNodes)
      {
        RDCERR("Couldn't find child event %u in current action while patching ExecuteIndirect", idx);
        break;
      }

      D3D12EventNode &eventNode = eventNodes[idx];
      ActionDescription &curAction = eventNodes[idx].action;

      SDChunk *fakeChunk = new SDChunk(""_lit);
      fakeChunk->metadata = baseChunk->metadata;
      fakeChunk->metadata.chunkID = (uint32_t)D3D12Chunk::List_IndirectSubCommand;

      {
        StructuredSerialiser structuriser(fakeChunk, &GetChunkName);
        structuriser.SetUserData(GetResourceManager());

        structuriser.Serialise("CommandIndex"_lit, i);
        structuriser.Serialise("ArgumentIndex"_lit, a);
        structuriser.Serialise("ArgumentSignature"_lit, arg);

        switch(arg.Type)
        {
          case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
          {
            const D3D12_DRAW_ARGUMENTS *args = (D3D12_DRAW_ARGUMENTS *)data;
            data += sizeof(D3D12_DRAW_ARGUMENTS);

            curAction.drawIndex = a;
            curAction.numIndices = args->VertexCountPerInstance;
            curAction.numInstances = args->InstanceCount;
            curAction.vertexOffset = args->StartVertexLocation;
            curAction.instanceOffset = args->StartInstanceLocation;
            curAction.flags |= ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indirect;

            curAction.customName = StringFormat::Fmt("[%u] arg%u: IndirectDraw(<%u, %u>)", i, a,
                                                     curAction.numIndices, curAction.numInstances);

            fakeChunk->name = curAction.customName;

            structuriser.Serialise("ArgumentData"_lit, *args).Important();

            m_Cmd->AddUsage(state, eventNode);
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
          {
            const D3D12_DRAW_INDEXED_ARGUMENTS *args = (D3D12_DRAW_INDEXED_ARGUMENTS *)data;
            data += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

            curAction.drawIndex = a;
            curAction.numIndices = args->IndexCountPerInstance;
            curAction.numInstances = args->InstanceCount;
            curAction.baseVertex = args->BaseVertexLocation;
            curAction.indexOffset = args->StartIndexLocation;
            curAction.instanceOffset = args->StartInstanceLocation;
            curAction.flags |= ActionFlags::Drawcall | ActionFlags::Instanced |
                               ActionFlags::Indexed | ActionFlags::Indirect;
            curAction.customName = StringFormat::Fmt("[%u] arg%u: IndirectDrawIndexed(<%u, %u>)", i,
                                                     a, curAction.numIndices, curAction.numInstances);

            fakeChunk->name = curAction.customName;

            structuriser.Serialise("ArgumentData"_lit, *args).Important();

            m_Cmd->AddUsage(state, eventNode);
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
          {
            const D3D12_DISPATCH_ARGUMENTS *args = (D3D12_DISPATCH_ARGUMENTS *)data;
            data += sizeof(D3D12_DISPATCH_ARGUMENTS);

            curAction.dispatchDimension[0] = args->ThreadGroupCountX;
            curAction.dispatchDimension[1] = args->ThreadGroupCountY;
            curAction.dispatchDimension[2] = args->ThreadGroupCountZ;
            curAction.flags |= ActionFlags::Dispatch | ActionFlags::Indirect;
            curAction.customName = StringFormat::Fmt(
                "[%u] arg%u: IndirectDispatch(<%u, %u, %u>)", i, a, curAction.dispatchDimension[0],
                curAction.dispatchDimension[1], curAction.dispatchDimension[2]);

            fakeChunk->name = curAction.customName;

            structuriser.Serialise("ArgumentData"_lit, *args).Important();

            m_Cmd->AddUsage(state, eventNode);
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
          {
            const D3D12_DISPATCH_MESH_ARGUMENTS *args = (D3D12_DISPATCH_MESH_ARGUMENTS *)data;
            data += sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);

            curAction.dispatchDimension[0] = args->ThreadGroupCountX;
            curAction.dispatchDimension[1] = args->ThreadGroupCountY;
            curAction.dispatchDimension[2] = args->ThreadGroupCountZ;
            curAction.flags |= ActionFlags::MeshDispatch | ActionFlags::Indirect;
            curAction.customName =
                StringFormat::Fmt("[%u] arg%u: IndirectDispatchMesh(<%u, %u, %u>)", i, a,
                                  curAction.dispatchDimension[0], curAction.dispatchDimension[1],
                                  curAction.dispatchDimension[2]);

            fakeChunk->name = curAction.customName;

            structuriser.Serialise("ArgumentData"_lit, *args).Important();

            m_Cmd->AddUsage(state, eventNode);
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:
          {
            // This modifies the mapped data via args
            D3D12_DISPATCH_RAYS_DESC *args = (D3D12_DISPATCH_RAYS_DESC *)data;
            data += sizeof(D3D12_DISPATCH_RAYS_DESC);

            for(D3D12_GPU_VIRTUAL_ADDRESS *addr :
                {&args->RayGenerationShaderRecord.StartAddress, &args->MissShaderTable.StartAddress,
                 &args->HitGroupTable.StartAddress, &args->CallableShaderTable.StartAddress})
            {
              if(*addr == 0)
                continue;

              ResourceId id;
              uint64_t offs = 0;
              m_pDevice->GetResIDFromOrigAddr(*addr, id, offs);

              ID3D12Resource *res = GetResourceManager()->GetResAs<ID3D12Resource>(id);
              RDCASSERT(res);
              if(res)
                *addr = res->GetGPUVirtualAddress() + offs;
            }

            curAction.dispatchDimension[0] = args->Width;
            curAction.dispatchDimension[1] = args->Height;
            curAction.dispatchDimension[2] = args->Depth;
            curAction.flags |= ActionFlags::DispatchRay | ActionFlags::Indirect;
            curAction.customName =
                StringFormat::Fmt("[%u] arg%u: IndirectDispatchRays(<%u, %u, %u>)", i, a,
                                  curAction.dispatchDimension[0], curAction.dispatchDimension[1],
                                  curAction.dispatchDimension[2]);

            fakeChunk->name = curAction.customName;

            structuriser.Serialise("ArgumentData"_lit, *args).Important();

            m_Cmd->AddUsage(state, eventNode);
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
          {
            size_t argSize = sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
            const uint32_t *data32 = (uint32_t *)data;
            data += argSize;

            fakeChunk->name = StringFormat::Fmt("[%u] arg%u: IndirectSetRoot32BitConstants", i, a);

            structuriser.Serialise("Values"_lit, data32, arg.Constant.Num32BitValuesToSet).Important();

            if(arg.Constant.RootParameterIndex < state.graphics.sigelems.size())
            {
              state.graphics.sigelems[arg.Constant.RootParameterIndex].SetConstants(
                  arg.Constant.Num32BitValuesToSet, data32, arg.Constant.DestOffsetIn32BitValues);
            }

            if(arg.Constant.RootParameterIndex < state.compute.sigelems.size())
            {
              state.compute.sigelems[arg.Constant.RootParameterIndex].SetConstants(
                  arg.Constant.Num32BitValuesToSet, data32, arg.Constant.DestOffsetIn32BitValues);
            }
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
          {
            // This modifies the mapped data via vb
            D3D12_VERTEX_BUFFER_VIEW *vb = (D3D12_VERTEX_BUFFER_VIEW *)data;
            data += sizeof(D3D12_VERTEX_BUFFER_VIEW);

            ResourceId id;
            uint64_t offs = 0;
            m_pDevice->GetResIDFromOrigAddr(vb->BufferLocation, id, offs);

            ID3D12Resource *res = GetResourceManager()->GetResAs<ID3D12Resource>(id);
            RDCASSERT(res);
            if(res)
              vb->BufferLocation = res->GetGPUVirtualAddress() + offs;

            if(arg.VertexBuffer.Slot >= state.vbuffers.size())
              state.vbuffers.resize(arg.VertexBuffer.Slot + 1);

            state.vbuffers[arg.VertexBuffer.Slot].buf = id;
            state.vbuffers[arg.VertexBuffer.Slot].offs = offs;
            state.vbuffers[arg.VertexBuffer.Slot].size = vb->SizeInBytes;
            state.vbuffers[arg.VertexBuffer.Slot].stride = vb->StrideInBytes;

            fakeChunk->name = StringFormat::Fmt("[%u] arg%u: IndirectIASetVertexBuffer", i, a);

            structuriser.Serialise("ArgumentData"_lit, *vb).Important();
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
          {
            // This modifies the mapped data via ib
            D3D12_INDEX_BUFFER_VIEW *ib = (D3D12_INDEX_BUFFER_VIEW *)data;
            data += sizeof(D3D12_INDEX_BUFFER_VIEW);

            ResourceId id;
            uint64_t offs = 0;
            m_pDevice->GetResIDFromOrigAddr(ib->BufferLocation, id, offs);

            ID3D12Resource *res = GetResourceManager()->GetResAs<ID3D12Resource>(id);
            RDCASSERT(res);
            if(res)
              ib->BufferLocation = res->GetGPUVirtualAddress() + offs;

            state.ibuffer.buf = id;
            state.ibuffer.offs = offs;
            state.ibuffer.size = ib->SizeInBytes;
            state.ibuffer.bytewidth = ib->Format == DXGI_FORMAT_R32_UINT ? 4 : 2;

            fakeChunk->name = StringFormat::Fmt("[%u] arg%u: IndirectIASetIndexBuffer", i, a);

            structuriser.Serialise("ArgumentData"_lit, *ib).Important();
            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
          case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
          case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
          {
            // This modifies the mapped data via addr
            D3D12_GPU_VIRTUAL_ADDRESS *addr = (D3D12_GPU_VIRTUAL_ADDRESS *)data;
            data += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);

            ResourceId id;
            uint64_t offs = 0;
            m_pDevice->GetResIDFromOrigAddr(*addr, id, offs);

            ID3D12Resource *res = GetResourceManager()->GetResAs<ID3D12Resource>(id);
            if(res)
              *addr = res->GetGPUVirtualAddress() + offs;

            // ConstantBufferView, ShaderResourceView and UnorderedAccessView all have one member -
            // RootParameterIndex
            if(arg.ConstantBufferView.RootParameterIndex < state.graphics.sigelems.size())
            {
              state.graphics.sigelems[arg.ConstantBufferView.RootParameterIndex].id = id;
              state.graphics.sigelems[arg.ConstantBufferView.RootParameterIndex].offset = offs;
            }

            if(arg.ConstantBufferView.RootParameterIndex < state.compute.sigelems.size())
            {
              state.compute.sigelems[arg.ConstantBufferView.RootParameterIndex].id = id;
              state.compute.sigelems[arg.ConstantBufferView.RootParameterIndex].offset = offs;
            }

            const char *viewTypeStr = "?";

            if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
              viewTypeStr = "ConstantBuffer";
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
              viewTypeStr = "ShaderResource";
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
              viewTypeStr = "UnorderedAccess";

            fakeChunk->name =
                StringFormat::Fmt("[%u] arg%u: IndirectSetRoot%sView", i, a, viewTypeStr);

            D3D12BufferLocation buf = *addr;

            structuriser.Serialise("ArgumentData"_lit, buf).Important();
            break;
          }
          default: RDCERR("Unexpected argument type! %d", arg.Type); break;
        }
      }
      // Set the chunk data
      sdFile->chunks.push_back(fakeChunk);

      eventNode.event.chunkIndex = uint32_t(sdFile->chunks.size() - 1);

      ++idx;
    }
  }

  range.End = range.Begin = 0;
  exec.argBuf->Unmap(0, &range);
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ExecuteIndirect(
    SerialiserType &ser, ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount,
    ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer,
    UINT64 CountBufferOffset)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pCommandSignature).Important();
  SERIALISE_ELEMENT(MaxCommandCount).Important();
  SERIALISE_ELEMENT(pArgumentBuffer).Important();
  SERIALISE_ELEMENT(ArgumentBufferOffset).OffsetOrSize();
  SERIALISE_ELEMENT(pCountBuffer);
  SERIALISE_ELEMENT(CountBufferOffset).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    BakedCmdListInfo &cmdInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

    const D3D12RenderState &state = cmdInfo.state;

    WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)pCommandSignature;

    if(IsActiveReplaying(m_State))
    {
      uint32_t actualCount = MaxCommandCount;
      uint32_t countEventsReplayed = actualCount * comSig->sig.arguments.count();

      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t curEID = m_Cmd->m_RootEventID;

        if(m_Cmd->m_FirstEventID <= 1)
        {
          curEID = cmdInfo.curEventID;

          if(m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent == m_Cmd->m_LastCmdListID)
            curEID += m_Cmd->m_Partial[D3D12CommandData::Primary].baseEvent;
          else if(m_Cmd->m_Partial[D3D12CommandData::Secondary].partialParent ==
                  m_Cmd->m_LastCmdListID)
            curEID += m_Cmd->m_Partial[D3D12CommandData::Secondary].baseEvent;
        }

        D3D12CommandData::ActionUse use(m_Cmd->m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_Cmd->m_ActionUses.begin(), m_Cmd->m_ActionUses.end(), use);

        // baseEventID is the EI action EID
        uint32_t baseEventID = it->eventId;

        {
          // get the number of draws by looking at how many children the parent action has.
          const rdcarray<ActionDescription> &children = m_pDevice->GetAction(it->eventId)->children;
          actualCount = (uint32_t)children.size();

          // don't count the popmarker child
          if(!children.empty() && children.back().flags & ActionFlags::PopMarker)
            actualCount--;
        }

        uint32_t argumentsReplayed =
            RDCMIN(m_Cmd->m_LastEventID - baseEventID, actualCount * comSig->sig.arguments.count());
        uint32_t executesReplayed = argumentsReplayed / comSig->sig.arguments.count();

        // executesReplayed is relative to baseEventID
        // compute the number of events to skip relative to the curEID
        countEventsReplayed = (baseEventID + argumentsReplayed) - curEID;

        BarrierSet barriers;

        barriers.Configure(pArgumentBuffer, cmdInfo.GetState(m_pDevice, GetResID(pArgumentBuffer)),
                           BarrierSet::CopySourceAccess);
        if(pCountBuffer)
          barriers.Configure(pCountBuffer, cmdInfo.GetState(m_pDevice, GetResID(pCountBuffer)),
                             BarrierSet::CopySourceAccess);

        barriers.Apply(list);

        // the spec says that any root arguments of VB/IBs set are reset to 0. We reset the ones
        // replayed here (accounting for selecting within the first few events), then record the
        // arguments so that if the last event ends mid-way through this execute we can later
        // set the state with the correct arguments
        ResetAndRecordExecuteIndirectStates(list, baseEventID, actualCount, pCommandSignature,
                                            pArgumentBuffer, ArgumentBufferOffset, argumentsReplayed);

        barriers.Unapply(list);

        // when we have a callback, submit every action individually to the callback
        if(m_Cmd->m_ActionCallback)
        {
          uint32_t countToReplay = RDCMIN(actualCount, executesReplayed);

          D3D12MarkerRegion::Begin(
              list,
              StringFormat::Fmt("ExecuteIndirect callback replay (drawCount=%u)", countToReplay));

          rdcpair<ID3D12Resource *, UINT64> patched =
              m_pDevice->GetDebugManager()->PatchExecuteIndirect(
                  list, m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state, pCommandSignature,
                  pArgumentBuffer, ArgumentBufferOffset,
                  (pCountBuffer ? pCountBuffer->GetGPUVirtualAddress() : 0) + CountBufferOffset,
                  MaxCommandCount);

          m_Cmd->m_IndirectData.commandSig = pCommandSignature;
          m_Cmd->m_IndirectData.argsBuffer = patched.first;
          m_Cmd->m_IndirectData.argsOffset = patched.second;

          ID3D12Resource *argBuffer = Unwrap(patched.first);
          uint64_t argOffset = patched.second;
          uint32_t maxCommands = MaxCommandCount;

          if(comSig->sig.raytraced)
          {
            PatchedRayDispatch patchedDispatch = {};
            patchedDispatch = GetResourceManager()->GetRTManager()->PatchIndirectRayDispatch(
                Unwrap(list), state.heaps, comSig, maxCommands, patched.first, patched.second,
                pCountBuffer, CountBufferOffset);

            argBuffer = patchedDispatch.resources.argumentBuffer->Resource();
            argOffset = patchedDispatch.resources.argumentBuffer->Offset();

            // restore state that would have been mutated by the patching process
            Unwrap(list)->SetComputeRootSignature(
                Unwrap(GetResourceManager()->GetResAs<ID3D12RootSignature>(state.compute.rootsig)));
            Unwrap4((ID3D12GraphicsCommandList4 *)list)
                ->SetPipelineState1(
                    Unwrap(GetResourceManager()->GetResAs<ID3D12StateObject>(state.stateobj)));
            state.ApplyComputeRootElementsUnwrapped(Unwrap(list));
            m_Cmd->m_RayDispatches.push_back(patchedDispatch);
          }

          countToReplay = RDCMIN(countToReplay, maxCommands);

          uint32_t firstDraw = 0;
          if(m_Cmd->m_FirstEventID > 1)
          {
            const uint32_t argidx = (curEID > baseEventID) ? (curEID - baseEventID - 1) : 0;
            const uint32_t execidx = argidx / comSig->sig.arguments.count();
            firstDraw = execidx;

            argOffset += comSig->sig.ByteStride * execidx;

            // Don't replay anything when selecting the pop marker
            if(execidx == maxCommands)
              countToReplay = 0;
          }

          for(uint32_t i = firstDraw; i < countToReplay; i++)
          {
            ActionFlags drawType =
                comSig->sig.graphics ? ActionFlags::Drawcall : ActionFlags::Dispatch;

            uint32_t eventId =
                m_Cmd->HandlePreCallback(list, drawType, (i + 1) * comSig->sig.arguments.count());

            // Allow the callback to recreate the command signature i.e. to match the root signature
            pCommandSignature = m_Cmd->m_IndirectData.commandSig;

            // single action starting at specific arg offset
            Unwrap(list)->ExecuteIndirect(Unwrap(pCommandSignature), 1, argBuffer, argOffset, NULL,
                                          0);

            if(drawType == ActionFlags::Drawcall)
            {
              if(eventId && m_Cmd->m_ActionCallback->PostDraw(eventId, list))
              {
                // Allow the callback to recreate the command signature i.e. to match the root signature
                pCommandSignature = m_Cmd->m_IndirectData.commandSig;

                Unwrap(list)->ExecuteIndirect(Unwrap(pCommandSignature), 1, argBuffer, argOffset,
                                              NULL, 0);
                m_Cmd->m_ActionCallback->PostRedraw(eventId, list);
              }
            }
            else
            {
              if(eventId && m_Cmd->m_ActionCallback->PostDispatch(eventId, list))
              {
                Unwrap(list)->ExecuteIndirect(Unwrap(pCommandSignature), 1, argBuffer, argOffset,
                                              NULL, 0);
                m_Cmd->m_ActionCallback->PostRedispatch(eventId, list);
              }
            }

            argOffset += comSig->sig.ByteStride;
            m_Cmd->m_IndirectData.argsOffset += comSig->sig.ByteStride;
          }
          m_Cmd->m_IndirectData.commandSig = NULL;
          m_Cmd->m_IndirectData.argsBuffer = NULL;
          m_Cmd->m_IndirectData.argsOffset = 0;

          D3D12MarkerRegion::End(list);
        }
        else if(m_Cmd->m_LastEventID > baseEventID)
        {
          rdcpair<ID3D12Resource *, UINT64> patched =
              m_pDevice->GetDebugManager()->PatchExecuteIndirect(
                  list, m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state, pCommandSignature,
                  pArgumentBuffer, ArgumentBufferOffset,
                  (pCountBuffer ? pCountBuffer->GetGPUVirtualAddress() : 0) + CountBufferOffset,
                  MaxCommandCount);

          ID3D12Resource *argBuffer = Unwrap(patched.first);
          UINT64 argOffset = patched.second;
          uint32_t maxCommands = MaxCommandCount;

          if(comSig->sig.raytraced)
          {
            PatchedRayDispatch patchedDispatch = {};
            patchedDispatch = GetResourceManager()->GetRTManager()->PatchIndirectRayDispatch(
                Unwrap(list), state.heaps, comSig, maxCommands, patched.first, patched.second,
                pCountBuffer, CountBufferOffset);

            argBuffer = patchedDispatch.resources.argumentBuffer->Resource();
            argOffset = patchedDispatch.resources.argumentBuffer->Offset();

            // restore state that would have been mutated by the patching process
            Unwrap(list)->SetComputeRootSignature(
                Unwrap(GetResourceManager()->GetResAs<ID3D12RootSignature>(state.compute.rootsig)));
            Unwrap4((ID3D12GraphicsCommandList4 *)list)
                ->SetPipelineState1(
                    Unwrap(GetResourceManager()->GetResAs<ID3D12StateObject>(state.stateobj)));
            state.ApplyComputeRootElementsUnwrapped(Unwrap(list));
            m_Cmd->m_RayDispatches.push_back(patchedDispatch);
          }

          const ActionDescription *action = m_pDevice->GetAction(curEID);
          uint32_t countToReplay = RDCMIN(actualCount, maxCommands);

          if(m_Cmd->m_FirstEventID <= 1)
          {
            // if we're replaying part-way into a multidraw we just clamp the count
            // ExecuteIndirect requires that there is precisely one dispatch/draw, and it comes
            // last. So after accounting for state setting above in
            // ResetAndRecordExecuteIndirectStates we can 'round down' to the nearest whole number
            // of executes, as if we select e.g. partway but not to the end of the second execute
            // there's no need to replay anything more than the first execute.
            countToReplay = RDCMIN(countToReplay, executesReplayed);
          }
          else if(action && action->flags & ActionFlags::PopMarker)
          {
            // don't do anything when selecting the final popmarker as well - everything will have
            // been done in previous replays so this is a no-op.
            countToReplay = 0;
          }
          else
          {
            const uint32_t argidx = (curEID > baseEventID) ? (curEID - baseEventID - 1) : 0;

            // we also know that only the last argument actually does anything - previous are just
            // state setting. So if argIdx isn't the last one, we can skip this
            if((argidx + 1) % comSig->sig.arguments.count() != 0)
            {
              countToReplay = 0;
            }
            else
            {
              // slightly more complex, we're replaying only one execute later on as a single draw
              // fortunately ExecuteIndirect has no 'draw' builtin, so we can just offset the
              // argument buffer and set count to 1
              const uint32_t execidx = argidx / comSig->sig.arguments.count();
              countToReplay = 1;
              argOffset += comSig->sig.ByteStride * execidx;
            }
          }

          if(countToReplay > 0)
            Unwrap(list)->ExecuteIndirect(Unwrap(pCommandSignature), countToReplay, argBuffer,
                                          argOffset, NULL, 0);
        }
      }

      // executes skip the event ID past the whole thing
      ++countEventsReplayed;
      if(m_Cmd->m_FirstEventID > 1)
        m_Cmd->m_RootEventID += countEventsReplayed;
      else
        cmdInfo.curEventID += countEventsReplayed;
    }
    else
    {
      BarrierSet barriers;

      barriers.Configure(pArgumentBuffer, cmdInfo.GetState(m_pDevice, GetResID(pArgumentBuffer)),
                         BarrierSet::CopySourceAccess);
      if(pCountBuffer)
        barriers.Configure(pCountBuffer, cmdInfo.GetState(m_pDevice, GetResID(pCountBuffer)),
                           BarrierSet::CopySourceAccess);

      ID3D12GraphicsCommandListX *list = ((ID3D12GraphicsCommandListX *)pCommandList);

      barriers.Apply(list);

      D3D12ExecuteData execData =
          SaveExecuteIndirectParameters(list, pCommandSignature, MaxCommandCount, pArgumentBuffer,
                                        ArgumentBufferOffset, pCountBuffer, CountBufferOffset);

      barriers.Unapply(list);

      rdcpair<ID3D12Resource *, UINT64> patched = m_pDevice->GetDebugManager()->PatchExecuteIndirect(
          list, cmdInfo.state, pCommandSignature, pArgumentBuffer, ArgumentBufferOffset,
          (pCountBuffer ? pCountBuffer->GetGPUVirtualAddress() : 0) + CountBufferOffset,
          MaxCommandCount);

      ID3D12Resource *argBuffer = Unwrap(patched.first);
      UINT64 argOffset = patched.second;
      uint32_t maxCommands = MaxCommandCount;

      if(comSig->sig.raytraced)
      {
        PatchedRayDispatch patchedDispatch =
            GetResourceManager()->GetRTManager()->PatchIndirectRayDispatch(
                Unwrap(list), state.heaps, comSig, maxCommands, patched.first, patched.second,
                pCountBuffer, CountBufferOffset);

        argBuffer = patchedDispatch.resources.argumentBuffer->Resource();
        argOffset = patchedDispatch.resources.argumentBuffer->Offset();

        // restore state that would have been mutated by the patching process
        Unwrap(pCommandList)
            ->SetComputeRootSignature(
                Unwrap(GetResourceManager()->GetResAs<ID3D12RootSignature>(state.compute.rootsig)));
        Unwrap4((ID3D12GraphicsCommandList4 *)pCommandList)
            ->SetPipelineState1(
                Unwrap(GetResourceManager()->GetResAs<ID3D12StateObject>(state.stateobj)));
        state.ApplyComputeRootElementsUnwrapped(Unwrap(pCommandList));
        m_Cmd->m_RayDispatches.push_back(std::move(patchedDispatch));
      }

      Unwrap(list)->ExecuteIndirect(comSig->GetReal(), maxCommands, argBuffer, argOffset,
                                    Unwrap(pCountBuffer), CountBufferOffset);

      const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();

      // add base PushMarker. We always push for even single-event indirects, for consistency
      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.customName = "ExecuteIndirect";

        action.flags |= ActionFlags::MultiAction | ActionFlags::PushMarker;

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.executeData = execData;
        eventNode.hasExecuteData = true;
        cmdInfo.hasExecuteDatas = true;

        eventNode.resourceUsage.push_back(
            make_rdcpair(GetResID(pArgumentBuffer), ResourceUsage::Indirect));
        if(pCountBuffer)
        {
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pCountBuffer), ResourceUsage::Indirect));
        }
      }

      // For variable count indirects only allocate up to one indirect command to avoid pessimistic
      // allocation if MaxCommandCount is very high but the actual action count is low.
      uint32_t maxCountCmds = execData.reservedCount;
      for(uint32_t i = 0; i < maxCountCmds; i++)
      {
        for(uint32_t a = 0; a < sigSize; a++)
        {
          const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[a];

          switch(arg.Type)
          {
            case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
            case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
            case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:
            case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
            case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
            {
              // add dummy event and action
              m_Cmd->AddEvent();
              ActionDescription action;
              action.customName = "ExecuteIndirect";
              m_Cmd->AddAction(action);
              D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
              eventNode.state = new D3D12RenderState(cmdInfo.state);
              break;
            }
            case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
            case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
              // add dummy event
              m_Cmd->AddEvent();
              break;
            default: RDCERR("Unexpected argument type! %d", arg.Type); break;
          }
        }
      }

      {
        m_Cmd->AddEvent();
        ActionDescription action;
        action.customName = "ExecuteIndirect()";
        action.flags = ActionFlags::PopMarker;
        m_Cmd->AddAction(action);
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ExecuteIndirect(ID3D12CommandSignature *pCommandSignature,
                                                       UINT MaxCommandCount,
                                                       ID3D12Resource *pArgumentBuffer,
                                                       UINT64 ArgumentBufferOffset,
                                                       ID3D12Resource *pCountBuffer,
                                                       UINT64 CountBufferOffset)
{
  ID3D12Resource *argBuffer = Unwrap(pArgumentBuffer);
  UINT64 argOffset = ArgumentBufferOffset;

  PatchedRayDispatch patchedDispatch = {};
  const D3D12CommandSignature &sigData = ((WrappedID3D12CommandSignature *)pCommandSignature)->sig;
  if(sigData.raytraced)
  {
    patchedDispatch = GetResourceManager()->GetRTManager()->PatchIndirectRayDispatch(
        m_pList, m_CaptureComputeState.heaps, pCommandSignature, MaxCommandCount, pArgumentBuffer,
        ArgumentBufferOffset, pCountBuffer, CountBufferOffset);

    argBuffer = patchedDispatch.resources.argumentBuffer->Resource();
    argOffset = patchedDispatch.resources.argumentBuffer->Offset();

    // restore state that would have been mutated by the patching process
    m_pList->SetComputeRootSignature(Unwrap(
        GetResourceManager()->GetResAs<ID3D12RootSignature>(m_CaptureComputeState.compute.rootsig)));
    m_pList4->SetPipelineState1(
        Unwrap(GetResourceManager()->GetResAs<ID3D12StateObject>(m_CaptureComputeState.stateobj)));
    m_CaptureComputeState.ApplyComputeRootElementsUnwrapped(m_pList);
  }

  SERIALISE_TIME_CALL(m_pList->ExecuteIndirect(Unwrap(pCommandSignature), MaxCommandCount, argBuffer,
                                               argOffset, Unwrap(pCountBuffer), CountBufferOffset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ExecuteIndirect);
    Serialise_ExecuteIndirect(ser, pCommandSignature, MaxCommandCount, pArgumentBuffer,
                              ArgumentBufferOffset, pCountBuffer, CountBufferOffset);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    m_ListRecord->ContainsExecuteIndirect = true;

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pCommandSignature), eFrameRef_Read);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pArgumentBuffer), eFrameRef_Read);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pCountBuffer), eFrameRef_Read);

    // during capture track the ray dispatches so the memory can be freed dynamically. On replay we
    if(patchedDispatch.resources.lookupBuffer)
    {
      // free all the memory at the end of each replay
      m_RayDispatches.push_back(patchedDispatch.resources);
    }

    // an ExecuteIndirect could reference any buffer at all without us knowing if it has GPU VAs
    // anywhere in its arguments
    for(const D3D12_INDIRECT_ARGUMENT_DESC &arg : sigData.arguments)
    {
      if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW ||
         arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW ||
         arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW ||
         arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW ||
         arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW)
        m_ListRecord->cmdInfo->forceMapsListEvent = true;
    }
  }
}

#pragma endregion Draws

#pragma region Clears

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ClearDepthStencilView(
    SerialiserType &ser, D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags,
    FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  if(ser.VersionAtLeast(0x5))
  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately
    SERIALISE_ELEMENT_LOCAL(DSV, *GetWrapped(DepthStencilView)).Named("DepthStencilView"_lit);

    if(IsReplayingAndReading())
      DepthStencilView = m_pDevice->GetDebugManager()->GetTempDescriptor(DSV);
  }
  else
  {
    SERIALISE_ELEMENT(DepthStencilView);
  }
  SERIALISE_ELEMENT(ClearFlags);
  SERIALISE_ELEMENT(Depth).Important();
  SERIALISE_ELEMENT(Stencil).Important();
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId =
            m_Cmd->HandlePreCallback(list, ActionFlags::Clear | ActionFlags::ClearDepthStencil);
        Unwrap(list)->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil,
                                            NumRects, pRects);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(
                          eventId, ActionFlags::Clear | ActionFlags::ClearDepthStencil, list))
        {
          Unwrap(list)->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil,
                                              NumRects, pRects);
          m_Cmd->m_ActionCallback->PostRemisc(
              eventId, ActionFlags::Clear | ActionFlags::ClearDepthStencil, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil, NumRects,
                                  pRects);

      {
        m_Cmd->AddEvent();

        D3D12Descriptor *descriptor = GetWrapped(DepthStencilView);

        ActionDescription action;
        action.flags |= ActionFlags::Clear | ActionFlags::ClearDepthStencil;
        action.copyDestination = descriptor->GetResResourceId();
        action.copyDestinationSubresource =
            Subresource(GetMipForDsv(descriptor->GetDSV()), GetSliceForDsv(descriptor->GetDSV()));
        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(
            make_rdcpair(descriptor->GetResResourceId(), ResourceUsage::Clear));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_TIME_CALL(m_pList->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth,
                                                     Stencil, NumRects, pRects));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearDepthStencilView);
    Serialise_ClearDepthStencilView(ser, DepthStencilView, ClearFlags, Depth, Stencil, NumRects,
                                    pRects);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    {
      D3D12Descriptor *desc = GetWrapped(DepthStencilView);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ClearRenderTargetView(
    SerialiserType &ser, D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4],
    UINT NumRects, const D3D12_RECT *pRects)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  if(ser.VersionAtLeast(0x5))
  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately
    SERIALISE_ELEMENT_LOCAL(RTV, *GetWrapped(RenderTargetView)).Named("RenderTargetView"_lit);

    if(IsReplayingAndReading())
      RenderTargetView = m_pDevice->GetDebugManager()->GetTempDescriptor(RTV);
  }
  else
  {
    SERIALISE_ELEMENT(RenderTargetView);
  }
  SERIALISE_ELEMENT_ARRAY(ColorRGBA, 4).Important();
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId =
            m_Cmd->HandlePreCallback(list, ActionFlags::Clear | ActionFlags::ClearColor);
        Unwrap(list)->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(
                          eventId, ActionFlags::Clear | ActionFlags::ClearColor, list))
        {
          Unwrap(list)->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Clear | ActionFlags::ClearColor,
                                              list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);

      {
        m_Cmd->AddEvent();

        D3D12Descriptor *descriptor = GetWrapped(RenderTargetView);

        ActionDescription action;
        action.flags |= ActionFlags::Clear | ActionFlags::ClearColor;
        action.copyDestination = descriptor->GetResResourceId();
        action.copyDestinationSubresource =
            Subresource(GetMipForRtv(descriptor->GetRTV()), GetSliceForRtv(descriptor->GetRTV()));
        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(
            make_rdcpair(descriptor->GetResResourceId(), ResourceUsage::Clear));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects,
    const D3D12_RECT *pRects)
{
  SERIALISE_TIME_CALL(
      m_pList->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearRenderTargetView);
    Serialise_ClearRenderTargetView(ser, RenderTargetView, ColorRGBA, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    {
      D3D12Descriptor *desc = GetWrapped(RenderTargetView);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ClearUnorderedAccessViewUint(
    SerialiserType &ser, D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4],
    UINT NumRects, const D3D12_RECT *pRects)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(ViewGPUHandleInCurrentHeap);
  if(ser.VersionAtLeast(0x5))
  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately. This is only true for the CPU-side handle
    SERIALISE_ELEMENT_LOCAL(UAV, *GetWrapped(ViewCPUHandle)).Named("ViewCPUHandle"_lit);

    if(IsReplayingAndReading())
      ViewCPUHandle = m_pDevice->GetDebugManager()->GetTempDescriptor(UAV);
  }
  else
  {
    SERIALISE_ELEMENT(ViewCPUHandle);
  }
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_ARRAY(Values, 4).Important();
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Clear);
        Unwrap(list)->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap),
                                                   Unwrap(ViewCPUHandle), Unwrap(pResource), Values,
                                                   NumRects, pRects);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Clear, list))
        {
          Unwrap(list)->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap),
                                                     Unwrap(ViewCPUHandle), Unwrap(pResource),
                                                     Values, NumRects, pRects);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Clear, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                         Unwrap(pResource), Values, NumRects, pRects);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.flags |= ActionFlags::Clear;
        action.copyDestination = GetResID(pResource);
        action.copyDestinationSubresource = Subresource();

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(make_rdcpair(GetResID(pResource), ResourceUsage::Clear));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_TIME_CALL(m_pList->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap),
                                                            Unwrap(ViewCPUHandle), Unwrap(pResource),
                                                            Values, NumRects, pRects));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearUnorderedAccessViewUint);
    Serialise_ClearUnorderedAccessViewUint(ser, ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                           pResource, Values, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    {
      D3D12Descriptor *desc = GetWrapped(ViewGPUHandleInCurrentHeap);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      desc = GetWrapped(ViewCPUHandle);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      m_ListRecord->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_PartialWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ClearUnorderedAccessViewFloat(
    SerialiserType &ser, D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4],
    UINT NumRects, const D3D12_RECT *pRects)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(ViewGPUHandleInCurrentHeap);
  if(ser.VersionAtLeast(0x5))
  {
    // read and serialise the D3D12Descriptor contents directly, as the call has semantics of
    // consuming the descriptor immediately. This is only true for the CPU-side handle
    SERIALISE_ELEMENT_LOCAL(UAV, *GetWrapped(ViewCPUHandle)).Named("ViewCPUHandle"_lit);

    if(IsReplayingAndReading())
      ViewCPUHandle = m_pDevice->GetDebugManager()->GetTempDescriptor(UAV);
  }
  else
  {
    SERIALISE_ELEMENT(ViewCPUHandle);
  }
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_ARRAY(Values, 4).Important();
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Clear);
        Unwrap(list)->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap),
                                                    Unwrap(ViewCPUHandle), Unwrap(pResource),
                                                    Values, NumRects, pRects);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Clear, list))
        {
          Unwrap(list)->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap),
                                                      Unwrap(ViewCPUHandle), Unwrap(pResource),
                                                      Values, NumRects, pRects);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Clear, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                          Unwrap(pResource), Values, NumRects, pRects);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.flags |= ActionFlags::Clear;
        action.copyDestination = GetResID(pResource);
        action.copyDestinationSubresource = Subresource();

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(make_rdcpair(GetResID(pResource), ResourceUsage::Clear));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_TIME_CALL(m_pList->ClearUnorderedAccessViewFloat(
      Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle), Unwrap(pResource), Values,
      NumRects, pRects));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearUnorderedAccessViewFloat);
    Serialise_ClearUnorderedAccessViewFloat(ser, ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                            pResource, Values, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));

    {
      D3D12Descriptor *desc = GetWrapped(ViewGPUHandleInCurrentHeap);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      desc = GetWrapped(ViewCPUHandle);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);

      m_ListRecord->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_PartialWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_DiscardResource(SerialiserType &ser,
                                                                 ID3D12Resource *pResource,
                                                                 const D3D12_DISCARD_REGION *pRegion)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pResource).Important();
  SERIALISE_ELEMENT_OPT(pRegion);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->DiscardResource(Unwrap(pResource), pRegion);

        if(m_pDevice->GetReplayOptions().optimisation != ReplayOptimisationLevel::Fastest)
        {
          m_pDevice->GetDebugManager()->FillWithDiscardPattern(
              m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID),
              m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state, DiscardType::DiscardCall,
              pResource, pRegion, D3D12_BARRIER_LAYOUT_UNDEFINED);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->DiscardResource(Unwrap(pResource), pRegion);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.flags |= ActionFlags::Clear;
        action.copyDestination = GetResID(pResource);
        action.copyDestinationSubresource = Subresource();

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(make_rdcpair(GetResID(pResource), ResourceUsage::Discard));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DiscardResource(ID3D12Resource *pResource,
                                                       const D3D12_DISCARD_REGION *pRegion)
{
  SERIALISE_TIME_CALL(m_pList->DiscardResource(Unwrap(pResource), pRegion));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_DiscardResource);
    Serialise_DiscardResource(ser, pResource, pRegion);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_PartialWrite);
  }
}

#pragma endregion Clears

#pragma region Copies

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_CopyBufferRegion(SerialiserType &ser,
                                                                  ID3D12Resource *pDstBuffer,
                                                                  UINT64 DstOffset,
                                                                  ID3D12Resource *pSrcBuffer,
                                                                  UINT64 SrcOffset, UINT64 NumBytes)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pDstBuffer).Important();
  SERIALISE_ELEMENT(DstOffset).OffsetOrSize();
  SERIALISE_ELEMENT(pSrcBuffer).Important();
  SERIALISE_ELEMENT(SrcOffset).OffsetOrSize();
  SERIALISE_ELEMENT(NumBytes).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Copy);
        Unwrap(list)->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset,
                                       NumBytes);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, list))
        {
          Unwrap(list)->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                         SrcOffset, NumBytes);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset, NumBytes);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.copySource = GetResID(pSrcBuffer);
        action.copySourceSubresource = Subresource();
        action.copyDestination = GetResID(pDstBuffer);
        action.copyDestinationSubresource = Subresource();

        action.flags |= ActionFlags::Copy;

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();

        if(pSrcBuffer == pDstBuffer)
        {
          eventNode.resourceUsage.push_back(make_rdcpair(GetResID(pSrcBuffer), ResourceUsage::Copy));
        }
        else
        {
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pSrcBuffer), ResourceUsage::CopySrc));
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pDstBuffer), ResourceUsage::CopyDst));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource *pDstBuffer,
                                                        UINT64 DstOffset, ID3D12Resource *pSrcBuffer,
                                                        UINT64 SrcOffset, UINT64 NumBytes)
{
  SERIALISE_TIME_CALL(m_pList->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                                SrcOffset, NumBytes));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyBufferRegion);
    Serialise_CopyBufferRegion(ser, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstBuffer), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_CopyTextureRegion(
    SerialiserType &ser, const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_LOCAL(dst, *pDst).Important();
  SERIALISE_ELEMENT(DstX);
  SERIALISE_ELEMENT(DstY);
  SERIALISE_ELEMENT(DstZ);
  SERIALISE_ELEMENT_LOCAL(src, *pSrc).Important();
  SERIALISE_ELEMENT_OPT(pSrcBox);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    D3D12_TEXTURE_COPY_LOCATION unwrappedDst = dst;
    unwrappedDst.pResource = Unwrap(unwrappedDst.pResource);
    D3D12_TEXTURE_COPY_LOCATION unwrappedSrc = src;
    unwrappedSrc.pResource = Unwrap(unwrappedSrc.pResource);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Copy);
        Unwrap(list)->CopyTextureRegion(&unwrappedDst, DstX, DstY, DstZ, &unwrappedSrc, pSrcBox);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, list))
        {
          Unwrap(list)->CopyTextureRegion(&unwrappedDst, DstX, DstY, DstZ, &unwrappedSrc, pSrcBox);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->CopyTextureRegion(&unwrappedDst, DstX, DstY, DstZ, &unwrappedSrc, pSrcBox);

      {
        m_Cmd->AddEvent();

        ResourceId liveSrc = GetResID(src.pResource);
        ResourceId liveDst = GetResID(dst.pResource);

        ResourceId origSrc = liveSrc;
        ResourceId origDst = liveDst;

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = origSrc;
        action.copySourceSubresource = Subresource();
        if(unwrappedSrc.Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
        {
          action.copySourceSubresource = Subresource(
              GetMipForSubresource(unwrappedSrc.pResource, unwrappedSrc.SubresourceIndex),
              GetSliceForSubresource(unwrappedSrc.pResource, unwrappedSrc.SubresourceIndex));
        }

        action.copyDestination = origDst;
        action.copyDestinationSubresource = Subresource();
        if(unwrappedDst.Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
        {
          action.copyDestinationSubresource = Subresource(
              GetMipForSubresource(unwrappedDst.pResource, unwrappedDst.SubresourceIndex),
              GetSliceForSubresource(unwrappedDst.pResource, unwrappedDst.SubresourceIndex));
        }

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();

        if(origSrc == origDst)
        {
          eventNode.resourceUsage.push_back(make_rdcpair(liveSrc, ResourceUsage::Copy));
        }
        else
        {
          eventNode.resourceUsage.push_back(make_rdcpair(liveSrc, ResourceUsage::CopySrc));
          eventNode.resourceUsage.push_back(make_rdcpair(liveDst, ResourceUsage::CopyDst));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst,
                                                         UINT DstX, UINT DstY, UINT DstZ,
                                                         const D3D12_TEXTURE_COPY_LOCATION *pSrc,
                                                         const D3D12_BOX *pSrcBox)
{
  D3D12_TEXTURE_COPY_LOCATION dst = *pDst;
  dst.pResource = Unwrap(dst.pResource);

  D3D12_TEXTURE_COPY_LOCATION src = *pSrc;
  src.pResource = Unwrap(src.pResource);

  SERIALISE_TIME_CALL(m_pList->CopyTextureRegion(&dst, DstX, DstY, DstZ, &src, pSrcBox));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyTextureRegion);
    Serialise_CopyTextureRegion(ser, pDst, DstX, DstY, DstZ, pSrc, pSrcBox);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDst->pResource), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrc->pResource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_CopyResource(SerialiserType &ser,
                                                              ID3D12Resource *pDstResource,
                                                              ID3D12Resource *pSrcResource)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pDstResource).Important();
  SERIALISE_ELEMENT(pSrcResource).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Copy);
        Unwrap(list)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, list))
        {
          Unwrap(list)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.copySource = GetResID(pSrcResource);
        action.copySourceSubresource = Subresource();
        action.copyDestination = GetResID(pDstResource);
        action.copyDestinationSubresource = Subresource();

        action.flags |= ActionFlags::Copy;

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();

        if(pSrcResource == pDstResource)
        {
          eventNode.resourceUsage.push_back(make_rdcpair(GetResID(pSrcResource), ResourceUsage::Copy));
        }
        else
        {
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pSrcResource), ResourceUsage::CopySrc));
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pDstResource), ResourceUsage::CopyDst));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource,
                                                    ID3D12Resource *pSrcResource)
{
  SERIALISE_TIME_CALL(m_pList->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource)));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyResource);
    Serialise_CopyResource(ser, pDstResource, pSrcResource);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ResolveSubresource(
    SerialiserType &ser, ID3D12Resource *pDstResource, UINT DstSubresource,
    ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pDstResource).Important();
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(pSrcResource).Important();
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT(Format);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Resolve);
        Unwrap(list)->ResolveSubresource(Unwrap(pDstResource), DstSubresource, Unwrap(pSrcResource),
                                         SrcSubresource, Format);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Resolve, list))
        {
          Unwrap(list)->ResolveSubresource(Unwrap(pDstResource), DstSubresource,
                                           Unwrap(pSrcResource), SrcSubresource, Format);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Resolve, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ResolveSubresource(Unwrap(pDstResource), DstSubresource, Unwrap(pSrcResource),
                               SrcSubresource, Format);

      {
        m_Cmd->AddEvent();

        ActionDescription action;
        action.copySource = GetResID(pSrcResource);
        action.copySourceSubresource =
            Subresource(GetMipForSubresource(pSrcResource, SrcSubresource),
                        GetSliceForSubresource(pSrcResource, SrcSubresource));

        action.copyDestination = GetResID(pDstResource);
        action.copyDestinationSubresource =
            Subresource(GetMipForSubresource(pDstResource, DstSubresource),
                        GetSliceForSubresource(pDstResource, DstSubresource));

        action.flags |= ActionFlags::Resolve;

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();

        if(pSrcResource == pDstResource)
        {
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pSrcResource), ResourceUsage::Resolve));
        }
        else
        {
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pSrcResource), ResourceUsage::ResolveSrc));
          eventNode.resourceUsage.push_back(
              make_rdcpair(GetResID(pDstResource), ResourceUsage::ResolveDst));
        }
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ResolveSubresource(ID3D12Resource *pDstResource,
                                                          UINT DstSubresource,
                                                          ID3D12Resource *pSrcResource,
                                                          UINT SrcSubresource, DXGI_FORMAT Format)
{
  SERIALISE_TIME_CALL(m_pList->ResolveSubresource(Unwrap(pDstResource), DstSubresource,
                                                  Unwrap(pSrcResource), SrcSubresource, Format));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ResolveSubresource);
    Serialise_ResolveSubresource(ser, pDstResource, DstSubresource, pSrcResource, SrcSubresource,
                                 Format);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_CopyTiles(
    SerialiserType &ser, ID3D12Resource *pTiledResource,
    const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pTiledResource).Important();
  SERIALISE_ELEMENT_LOCAL(TileRegionStartCoordinate, *pTileRegionStartCoordinate);
  SERIALISE_ELEMENT_LOCAL(TileRegionSize, *pTileRegionSize);
  SERIALISE_ELEMENT(pBuffer).Important();
  SERIALISE_ELEMENT(BufferStartOffsetInBytes).OffsetOrSize();
  SERIALISE_ELEMENT(Flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResID(pCommandList);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandListX *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, ActionFlags::Copy);
        Unwrap(list)->CopyTiles(Unwrap(pTiledResource), &TileRegionStartCoordinate, &TileRegionSize,
                                Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);
        if(eventId && m_Cmd->m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, list))
        {
          Unwrap(list)->CopyTiles(Unwrap(pTiledResource), &TileRegionStartCoordinate,
                                  &TileRegionSize, Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);
          m_Cmd->m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->CopyTiles(Unwrap(pTiledResource), &TileRegionStartCoordinate, &TileRegionSize,
                      Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);

      {
        m_Cmd->AddEvent();

        ResourceId liveSrc = GetResID(pBuffer);
        ResourceId liveDst = GetResID(pTiledResource);

        if(Flags & D3D12_TILE_COPY_FLAG_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER)
          std::swap(liveSrc, liveDst);

        ResourceId origSrc = liveSrc;
        ResourceId origDst = liveDst;

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = origSrc;
        action.copyDestination = origDst;

        Subresource tileSub = Subresource(
            GetMipForSubresource(pTiledResource, TileRegionStartCoordinate.Subresource),
            GetSliceForSubresource(pTiledResource, TileRegionStartCoordinate.Subresource));

        if(Flags & D3D12_TILE_COPY_FLAG_SWIZZLED_TILED_RESOURCE_TO_LINEAR_BUFFER)
          action.copySourceSubresource = tileSub;
        else
          action.copyDestinationSubresource = tileSub;

        m_Cmd->AddAction(action);

        D3D12EventNode &eventNode = m_Cmd->GetLastEventNode();
        eventNode.resourceUsage.push_back(make_rdcpair(liveSrc, ResourceUsage::CopySrc));
        eventNode.resourceUsage.push_back(make_rdcpair(liveDst, ResourceUsage::CopyDst));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
  SERIALISE_TIME_CALL(m_pList->CopyTiles(Unwrap(pTiledResource), pTileRegionStartCoordinate,
                                         pTileRegionSize, Unwrap(pBuffer), BufferStartOffsetInBytes,
                                         Flags));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyTiles);
    Serialise_CopyTiles(ser, pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer,
                        BufferStartOffsetInBytes, Flags);

    m_ListRecord->AddChunk(scope.Get(m_ListRecord->cmdInfo->alloc));
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pTiledResource), eFrameRef_PartialWrite);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pBuffer), eFrameRef_Read);
  }
}

#pragma endregion Copies

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, Close);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, Reset,
                                ID3D12CommandAllocator *pAllocator,
                                ID3D12PipelineState *pInitialState);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ResourceBarrier,
                                UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ClearState,
                                ID3D12PipelineState *pPipelineState);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, IASetPrimitiveTopology,
                                D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, RSSetViewports,
                                UINT NumViewports, const D3D12_VIEWPORT *pViewports);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, RSSetScissorRects,
                                UINT NumRects, const D3D12_RECT *pRects);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, OMSetBlendFactor,
                                const FLOAT BlendFactor[4]);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, OMSetStencilRef,
                                UINT StencilRef);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetDescriptorHeaps,
                                UINT NumDescriptorHeaps,
                                ID3D12DescriptorHeap *const *ppDescriptorHeaps);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, IASetIndexBuffer,
                                const D3D12_INDEX_BUFFER_VIEW *pView);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, IASetVertexBuffers,
                                UINT StartSlot, UINT NumViews,
                                const D3D12_VERTEX_BUFFER_VIEW *pViews);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SOSetTargets, UINT StartSlot,
                                UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetPipelineState,
                                ID3D12PipelineState *pPipelineState);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, OMSetRenderTargets,
                                UINT NumRenderTargetDescriptors,
                                const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
                                BOOL RTsSingleHandleToDescriptorRange,
                                const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetComputeRootSignature,
                                ID3D12RootSignature *pRootSignature);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetComputeRootDescriptorTable,
                                UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetComputeRoot32BitConstant,
                                UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetComputeRoot32BitConstants,
                                UINT RootParameterIndex, UINT Num32BitValuesToSet,
                                const void *pSrcData, UINT DestOffsetIn32BitValues);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetComputeRootConstantBufferView, UINT RootParameterIndex,
                                D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetComputeRootShaderResourceView, UINT RootParameterIndex,
                                D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetComputeRootUnorderedAccessView, UINT RootParameterIndex,
                                D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetGraphicsRootSignature,
                                ID3D12RootSignature *pRootSignature);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetGraphicsRootDescriptorTable, UINT RootParameterIndex,
                                D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetGraphicsRoot32BitConstant,
                                UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetGraphicsRoot32BitConstants,
                                UINT RootParameterIndex, UINT Num32BitValuesToSet,
                                const void *pSrcData, UINT DestOffsetIn32BitValues);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetGraphicsRootConstantBufferView, UINT RootParameterIndex,
                                D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetGraphicsRootShaderResourceView, UINT RootParameterIndex,
                                D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList,
                                SetGraphicsRootUnorderedAccessView, UINT RootParameterIndex,
                                D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, BeginQuery,
                                ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, EndQuery,
                                ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ResolveQueryData,
                                ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex,
                                UINT NumQueries, ID3D12Resource *pDestinationBuffer,
                                UINT64 AlignedDestinationBufferOffset);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetPredication,
                                ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset,
                                D3D12_PREDICATION_OP Operation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetMarker, UINT Metadata,
                                const void *pData, UINT Size);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, BeginEvent, UINT Metadata,
                                const void *pData, UINT Size);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, EndEvent);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, SetCommandAnnotation,
                                rdcstr key, RENDERDOC_AnnotationType valueType,
                                uint32_t valueVectorWidth, RENDERDOC_AnnotationValue value);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, DrawInstanced,
                                UINT VertexCountPerInstance, UINT InstanceCount,
                                UINT StartVertexLocation, UINT StartInstanceLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, DrawIndexedInstanced,
                                UINT IndexCountPerInstance, UINT InstanceCount,
                                UINT StartIndexLocation, INT BaseVertexLocation,
                                UINT StartInstanceLocation);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, Dispatch,
                                UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                UINT ThreadGroupCountZ);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ExecuteBundle,
                                ID3D12GraphicsCommandList *pCommandList);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ExecuteIndirect,
                                ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount,
                                ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset,
                                ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ClearDepthStencilView,
                                D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
                                D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil,
                                UINT NumRects, const D3D12_RECT *pRects);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ClearRenderTargetView,
                                D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
                                const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ClearUnorderedAccessViewUint,
                                D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
                                D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource,
                                const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ClearUnorderedAccessViewFloat,
                                D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
                                D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource,
                                const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, DiscardResource,
                                ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, CopyBufferRegion,
                                ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, CopyTextureRegion,
                                const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY,
                                UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc,
                                const D3D12_BOX *pSrcBox);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, CopyResource,
                                ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ResolveSubresource,
                                ID3D12Resource *pDstResource, UINT DstSubresource,
                                ID3D12Resource *pSrcResource, UINT SrcSubresource,
                                DXGI_FORMAT Format);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, CopyTiles,
                                ID3D12Resource *pTiledResource,
                                const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
                                const D3D12_TILE_REGION_SIZE *pTileRegionSize,
                                ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes,
                                D3D12_TILE_COPY_FLAGS Flags);
