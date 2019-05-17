/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3DCommandListMarkerApi.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"

ID3D12GraphicsCommandList *WrappedID3D12GraphicsCommandList::GetCrackedList()
{
  return Unwrap(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.back());
}

ID3D12GraphicsCommandList1 *WrappedID3D12GraphicsCommandList::GetCrackedList1()
{
  return Unwrap1(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.back());
}

ID3D12GraphicsCommandList2 *WrappedID3D12GraphicsCommandList::GetCrackedList2()
{
  return Unwrap2(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.back());
}

ID3D12GraphicsCommandList3 *WrappedID3D12GraphicsCommandList::GetCrackedList3()
{
  return Unwrap3(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.back());
}

ID3D12GraphicsCommandList4 *WrappedID3D12GraphicsCommandList::GetCrackedList4()
{
  return Unwrap4(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.back());
}

ID3D12GraphicsCommandList4 *WrappedID3D12GraphicsCommandList::GetWrappedCrackedList()
{
  return m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.back();
}

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

  SERIALISE_ELEMENT_LOCAL(CommandList, GetResourceID()).TypedAs("ID3D12GraphicsCommandList *"_lit);
  SERIALISE_ELEMENT(BakedCommandList).TypedAs("ID3D12GraphicsCommandList *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = BakedCommandList;

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->HasRerecordCmdList(BakedCommandList))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(BakedCommandList);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
        RDCDEBUG("Ending re-recorded command list for %llu baked to %llu", CommandList,
                 BakedCommandList);
#endif

        int &markerCount = m_Cmd->m_BakedCmdListInfo[BakedCommandList].markerCount;

        for(int i = 0; i < markerCount; i++)
          D3D12MarkerRegion::End(list);

        if(m_Cmd->m_DrawcallCallback)
          m_Cmd->m_DrawcallCallback->PreCloseCommandList(list);

        if(m_Cmd->m_BakedCmdListInfo[BakedCommandList].renderPassActive)
          list->EndRenderPass();

        list->Close();

        if(m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent == CommandList)
          m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent = ResourceId();
      }

      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID = 0;
    }
    else
    {
      GetResourceManager()->GetLiveAs<WrappedID3D12GraphicsCommandList>(CommandList)->Close();

      if(!m_Cmd->m_BakedCmdListInfo[BakedCommandList].crackedLists.empty())
      {
        GetCrackedList()->Close();
      }

      if(!m_Cmd->m_BakedCmdListInfo[BakedCommandList].curEvents.empty())
      {
        DrawcallDescription draw;
        draw.name = "API Calls";
        draw.flags |= DrawFlags::APICalls;

        m_Cmd->AddDrawcall(draw, true);

        m_Cmd->m_BakedCmdListInfo[BakedCommandList].curEventID++;
      }

      {
        if(m_Cmd->GetDrawcallStack().size() > 1)
          m_Cmd->GetDrawcallStack().pop_back();
      }

      BakedCmdListInfo &baked = m_Cmd->m_BakedCmdListInfo[BakedCommandList];
      BakedCmdListInfo &parent = m_Cmd->m_BakedCmdListInfo[CommandList];

      baked.eventCount = baked.curEventID;
      baked.curEventID = 0;
      baked.parentList = CommandList;

      baked.endChunk = uint32_t(m_Cmd->m_StructuredFile->chunks.size() - 1);

      parent.curEventID = 0;
      parent.eventCount = 0;
      parent.drawCount = 0;
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
      ser.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Close);
      Serialise_Close(ser);

      m_ListRecord->AddChunk(scope.Get());
    }

    m_ListRecord->Bake();
  }

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
  SERIALISE_ELEMENT_LOCAL(CommandList, GetResourceID()).TypedAs("ID3D12GraphicsCommandList *"_lit);
  SERIALISE_ELEMENT(pAllocator);
  SERIALISE_ELEMENT(pInitialState);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
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
        const std::vector<uint32_t> &baseEvents = m_Cmd->m_Partial[p].cmdListExecs[BakedCommandList];

        for(auto it = baseEvents.begin(); it != baseEvents.end(); ++it)
        {
          if(*it <= m_Cmd->m_LastEventID && m_Cmd->m_LastEventID < (*it + length))
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Reset - partial detected %u < %u < %u, %llu -> %llu", *it,
                     m_Cmd->m_LastEventID, *it + length, CommandList, BakedCommandList);
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
            RDCDEBUG("Reset() - full re-record detected %u < %u <= %u, %llu -> %llu", *it,
                     *it + length, m_Cmd->m_LastEventID, m_Cmd->m_LastCmdListID, BakedCommandList);
#endif

            // this submission is completely within the range, so it should still be re-recorded
            rerecord = true;
          }
        }
      }

      if(rerecord)
      {
        ID3D12GraphicsCommandList *listptr = NULL;
        HRESULT hr =
            m_pDevice->CreateCommandList(nodeMask, type, pAllocator, pInitialState,
                                         __uuidof(ID3D12GraphicsCommandList), (void **)&listptr);

        if(FAILED(hr))
        {
          RDCERR("Failed on resource serialise-creation, hr: %s", ToStr(hr).c_str());
          return false;
        }

        // this is a safe upcast because it's a wrapped object
        ID3D12GraphicsCommandList4 *list = (ID3D12GraphicsCommandList4 *)listptr;

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

        m_Cmd->m_RerecordCmdList.push_back(list);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
          m_Cmd->m_RenderState.pipe = GetResID(pInitialState);
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
      m_Cmd->m_BakedCmdListInfo[CommandList].executeEvents =
          m_Cmd->m_BakedCmdListInfo[BakedCommandList].executeEvents;
    }
    else
    {
      if(!GetResourceManager()->HasLiveResource(BakedCommandList))
      {
        ID3D12GraphicsCommandList *list = NULL;
        HRESULT hr =
            m_pDevice->CreateCommandList(nodeMask, type, pAllocator, pInitialState,
                                         __uuidof(ID3D12GraphicsCommandList), (void **)&list);
        RDCASSERTEQUAL(hr, S_OK);

        m_pDevice->AddResource(BakedCommandList, ResourceType::CommandBuffer, "Baked Command List");
        m_pDevice->GetReplay()->GetResourceDesc(BakedCommandList).initialisationChunks.clear();
        m_pDevice->DerivedResource(CommandList, BakedCommandList);
        m_pDevice->DerivedResource(pAllocator, BakedCommandList);
        if(pInitialState)
          m_pDevice->DerivedResource(pInitialState, BakedCommandList);

        GetResourceManager()->AddLiveResource(BakedCommandList, list);

        // whenever a command-building chunk asks for the command list, it
        // will get our baked version.
        if(GetResourceManager()->HasReplacement(CommandList))
          GetResourceManager()->RemoveReplacement(CommandList);

        GetResourceManager()->ReplaceResource(CommandList, BakedCommandList);
      }
      else
      {
        ID3D12GraphicsCommandList *list =
            GetResourceManager()->GetLiveAs<WrappedID3D12GraphicsCommandList>(BakedCommandList)->GetReal();
        list->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
      }

      {
        D3D12DrawcallTreeNode *draw = new D3D12DrawcallTreeNode;
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].draw = draw;

        {
          if(m_Cmd->m_CrackedAllocators[GetResID(pAllocator)] == NULL)
          {
            HRESULT hr = m_pDevice->CreateCommandAllocator(
                type, __uuidof(ID3D12CommandAllocator),
                (void **)&m_Cmd->m_CrackedAllocators[GetResID(pAllocator)]);
            RDCASSERTEQUAL(hr, S_OK);
          }

          ID3D12GraphicsCommandList *listptr = NULL;
          m_pDevice->CreateCommandList(
              nodeMask, type, m_Cmd->m_CrackedAllocators[GetResID(pAllocator)], pInitialState,
              __uuidof(ID3D12GraphicsCommandList), (void **)&listptr);

          // this is a safe upcast because it's a wrapped object
          ID3D12GraphicsCommandList4 *list = (ID3D12GraphicsCommandList4 *)listptr;

          RDCASSERT(m_Cmd->m_BakedCmdListInfo[BakedCommandList].crackedLists.empty());
          m_Cmd->m_BakedCmdListInfo[BakedCommandList].crackedLists.push_back(list);
        }

        m_Cmd->m_BakedCmdListInfo[CommandList].type =
            m_Cmd->m_BakedCmdListInfo[BakedCommandList].type = type;
        m_Cmd->m_BakedCmdListInfo[CommandList].nodeMask =
            m_Cmd->m_BakedCmdListInfo[BakedCommandList].nodeMask = nodeMask;
        m_Cmd->m_BakedCmdListInfo[CommandList].allocator =
            m_Cmd->m_BakedCmdListInfo[BakedCommandList].allocator = GetResID(pAllocator);

        // On list execute we increment all child events/drawcalls by
        // m_RootEventID and insert them into the tree.
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].curEventID = 0;
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].eventCount = 0;
        m_Cmd->m_BakedCmdListInfo[BakedCommandList].drawCount = 0;

        m_Cmd->m_BakedCmdListInfo[BakedCommandList].drawStack.push_back(draw);

        m_Cmd->m_BakedCmdListInfo[BakedCommandList].beginChunk =
            uint32_t(m_Cmd->m_StructuredFile->chunks.size() - 1);

        // reset state
        D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[BakedCommandList].state;
        state.m_ResourceManager = GetResourceManager();
        state.m_DebugManager = m_pDevice->GetReplay()->GetDebugManager();
        state.pipe = GetResID(pInitialState);
      }
    }
  }

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator,
                                                ID3D12PipelineState *pInitialState)
{
  HRESULT ret = S_OK;

  if(IsCaptureMode(m_State))
  {
    bool firstTime = false;

    // reset for new recording
    m_ListRecord->DeleteChunks();
    m_ListRecord->ContainsExecuteIndirect = false;

    // free any baked commands. If we don't have any, this is the creation reset
    // so we don't actually do the 'real' reset.
    if(m_ListRecord->bakedCommands)
      m_ListRecord->bakedCommands->Delete(GetResourceManager());
    else
      firstTime = true;

    if(!firstTime)
    {
      SERIALISE_TIME_CALL(ret = m_pList->Reset(Unwrap(pAllocator), Unwrap(pInitialState)));
    }

    m_ListRecord->bakedCommands =
        GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_ListRecord->bakedCommands->type = Resource_GraphicsCommandList;
    m_ListRecord->bakedCommands->InternalResource = true;
    m_ListRecord->bakedCommands->cmdInfo = new CmdListRecordingInfo();

    {
      CACHE_THREAD_SERIALISER();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Reset);
      Serialise_Reset(ser, pAllocator, pInitialState);

      m_ListRecord->AddChunk(scope.Get());
    }

    // add allocator and initial state (if there is one) as frame refs. We can't add
    // them as parents of the list record because it won't get directly referenced
    // (just the baked commands), and we can't parent them onto the baked commands
    // because that would pull them into the capture section.
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pAllocator), eFrameRef_Read);
    if(pInitialState)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(pInitialState), eFrameRef_Read);

    if(firstTime)
      return S_OK;
  }
  else
  {
    ret = m_pList->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
  }

  return ret;
}

void WrappedID3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
  m_pList->ClearState(Unwrap(pPipelineState));
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ResourceBarrier(
    SerialiserType &ser, UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumBarriers);
  SERIALISE_ELEMENT_ARRAY(pBarriers, NumBarriers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    std::vector<D3D12_RESOURCE_BARRIER> filtered;
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
            BakedCmdListInfo &cmdinfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

            if(res1)
            {
              cmdinfo.resourceUsage.push_back(make_rdcpair(
                  GetResID(res1), EventUsage(cmdinfo.curEventID, ResourceUsage::Barrier)));
            }
            if(res2)
            {
              cmdinfo.resourceUsage.push_back(make_rdcpair(
                  GetResID(res2), EventUsage(cmdinfo.curEventID, ResourceUsage::Barrier)));
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
      {
        Unwrap(pCommandList)->ResourceBarrier((UINT)filtered.size(), &filtered[0]);
        GetCrackedList()->ResourceBarrier((UINT)filtered.size(), &filtered[0]);
      }
    }

    if(pCommandList)
    {
      ResourceId cmd = GetResID(pCommandList);

      for(UINT i = 0; i < NumBarriers; i++)
      {
        if(pBarriers[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
           pBarriers[i].Transition.pResource)
        {
          m_Cmd->m_BakedCmdListInfo[cmd].barriers.push_back(pBarriers[i]);
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

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->cmdInfo->barriers.insert(m_ListRecord->cmdInfo->barriers.end(), pBarriers,
                                           pBarriers + NumBarriers);
  }
}

#pragma region State Setting

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetPrimitiveTopology(
    SerialiserType &ser, D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(PrimitiveTopology);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->IASetPrimitiveTopology(PrimitiveTopology);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
          m_Cmd->m_RenderState.topo = PrimitiveTopology;
      }
    }
    else
    {
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.topo = PrimitiveTopology;

      Unwrap(pCommandList)->IASetPrimitiveTopology(PrimitiveTopology);
      GetCrackedList()->IASetPrimitiveTopology(PrimitiveTopology);
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

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT_ARRAY(pViewports, NumViewports);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->RSSetViewports(NumViewports, pViewports);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.views.size() < NumViewports)
            m_Cmd->m_RenderState.views.resize(NumViewports);

          for(UINT i = 0; i < NumViewports; i++)
            m_Cmd->m_RenderState.views[i] = pViewports[i];
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->RSSetViewports(NumViewports, pViewports);
      GetCrackedList()->RSSetViewports(NumViewports, pViewports);

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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_RSSetScissorRects(SerialiserType &ser, UINT NumRects,
                                                                   const D3D12_RECT *pRects)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->RSSetScissorRects(NumRects, pRects);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.scissors.size() < NumRects)
            m_Cmd->m_RenderState.scissors.resize(NumRects);

          for(UINT i = 0; i < NumRects; i++)
            m_Cmd->m_RenderState.scissors[i] = pRects[i];
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->RSSetScissorRects(NumRects, pRects);
      GetCrackedList()->RSSetScissorRects(NumRects, pRects);

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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetBlendFactor(SerialiserType &ser,
                                                                  const FLOAT BlendFactor[4])
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_ARRAY(BlendFactor, 4);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->OMSetBlendFactor(BlendFactor);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
          memcpy(m_Cmd->m_RenderState.blendFactor, BlendFactor, sizeof(float) * 4);
      }
    }
    else
    {
      Unwrap(pCommandList)->OMSetBlendFactor(BlendFactor);
      GetCrackedList()->OMSetBlendFactor(BlendFactor);

      memcpy(m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.blendFactor, BlendFactor,
             sizeof(float) * 4);
    }
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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_OMSetStencilRef(SerialiserType &ser, UINT StencilRef)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(StencilRef);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->OMSetStencilRef(StencilRef);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
          m_Cmd->m_RenderState.stencilRef = StencilRef;
      }
    }
    else
    {
      Unwrap(pCommandList)->OMSetStencilRef(StencilRef);
      GetCrackedList()->OMSetStencilRef(StencilRef);

      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.stencilRef = StencilRef;
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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetDescriptorHeaps(
    SerialiserType &ser, UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(NumDescriptorHeaps);
  SERIALISE_ELEMENT_ARRAY(ppDescriptorHeaps, NumDescriptorHeaps);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        std::vector<ID3D12DescriptorHeap *> heaps;
        heaps.resize(NumDescriptorHeaps);
        for(size_t i = 0; i < heaps.size(); i++)
          heaps[i] = Unwrap(ppDescriptorHeaps[i]);

        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetDescriptorHeaps(NumDescriptorHeaps, heaps.data());

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_RenderState.heaps.resize(heaps.size());
          for(size_t i = 0; i < heaps.size(); i++)
            m_Cmd->m_RenderState.heaps[i] = GetResID(ppDescriptorHeaps[i]);
        }
      }
    }
    else
    {
      std::vector<ID3D12DescriptorHeap *> heaps;
      heaps.resize(NumDescriptorHeaps);
      for(size_t i = 0; i < heaps.size(); i++)
        heaps[i] = Unwrap(ppDescriptorHeaps[i]);

      Unwrap(pCommandList)->SetDescriptorHeaps(NumDescriptorHeaps, heaps.data());
      GetCrackedList()->SetDescriptorHeaps(NumDescriptorHeaps, heaps.data());

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      state.heaps.resize(heaps.size());
      for(size_t i = 0; i < heaps.size(); i++)
        state.heaps[i] = GetResID(ppDescriptorHeaps[i]);
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

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumDescriptorHeaps; i++)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(ppDescriptorHeaps[i]), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetIndexBuffer(SerialiserType &ser,
                                                                  const D3D12_INDEX_BUFFER_VIEW *pView)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT_OPT(pView);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        Unwrap(list)->IASetIndexBuffer(pView);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(pView)
          {
            WrappedID3D12Resource1::GetResIDFromAddr(pView->BufferLocation,
                                                     m_Cmd->m_RenderState.ibuffer.buf,
                                                     m_Cmd->m_RenderState.ibuffer.offs);
            m_Cmd->m_RenderState.ibuffer.bytewidth = (pView->Format == DXGI_FORMAT_R32_UINT ? 4 : 2);
            m_Cmd->m_RenderState.ibuffer.size = pView->SizeInBytes;
          }
          else
          {
            m_Cmd->m_RenderState.ibuffer.buf = ResourceId();
            m_Cmd->m_RenderState.ibuffer.offs = 0;
            m_Cmd->m_RenderState.ibuffer.bytewidth = 2;
          }
        }
      }
    }
    else
    {
      ID3D12GraphicsCommandList *list = pCommandList;

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      list->IASetIndexBuffer(pView);
      GetCrackedList()->IASetIndexBuffer(pView);

      if(pView)
      {
        WrappedID3D12Resource1::GetResIDFromAddr(pView->BufferLocation, state.ibuffer.buf,
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

    m_ListRecord->AddChunk(scope.Get());
    if(pView)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource1::GetResIDFromAddr(pView->BufferLocation), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_IASetVertexBuffers(
    SerialiserType &ser, UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(pViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->IASetVertexBuffers(StartSlot, NumViews, pViews);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.vbuffers.size() < StartSlot + NumViews)
            m_Cmd->m_RenderState.vbuffers.resize(StartSlot + NumViews);

          for(UINT i = 0; i < NumViews; i++)
          {
            WrappedID3D12Resource1::GetResIDFromAddr(
                pViews[i].BufferLocation, m_Cmd->m_RenderState.vbuffers[StartSlot + i].buf,
                m_Cmd->m_RenderState.vbuffers[StartSlot + i].offs);

            m_Cmd->m_RenderState.vbuffers[StartSlot + i].stride = pViews[i].StrideInBytes;
            m_Cmd->m_RenderState.vbuffers[StartSlot + i].size = pViews[i].SizeInBytes;
          }
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->IASetVertexBuffers(StartSlot, NumViews, pViews);
      GetCrackedList()->IASetVertexBuffers(StartSlot, NumViews, pViews);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.vbuffers.size() < StartSlot + NumViews)
        state.vbuffers.resize(StartSlot + NumViews);

      for(UINT i = 0; i < NumViews; i++)
      {
        WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferLocation,
                                                 state.vbuffers[StartSlot + i].buf,
                                                 state.vbuffers[StartSlot + i].offs);

        state.vbuffers[StartSlot + i].stride = pViews[i].StrideInBytes;
        state.vbuffers[StartSlot + i].size = pViews[i].SizeInBytes;
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

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SOSetTargets(
    SerialiserType &ser, UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(StartSlot);
  SERIALISE_ELEMENT(NumViews);
  SERIALISE_ELEMENT_ARRAY(pViews, NumViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SOSetTargets(StartSlot, NumViews, pViews);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.streamouts.size() < StartSlot + NumViews)
            m_Cmd->m_RenderState.streamouts.resize(StartSlot + NumViews);

          for(UINT i = 0; i < NumViews; i++)
          {
            D3D12RenderState::StreamOut &so = m_Cmd->m_RenderState.streamouts[StartSlot + i];

            WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferLocation, so.buf, so.offs);

            WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferFilledSizeLocation,
                                                     so.countbuf, so.countoffs);

            so.size = pViews[i].SizeInBytes;
          }
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SOSetTargets(StartSlot, NumViews, pViews);
      GetCrackedList()->SOSetTargets(StartSlot, NumViews, pViews);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.streamouts.size() < StartSlot + NumViews)
        state.streamouts.resize(StartSlot + NumViews);

      for(UINT i = 0; i < NumViews; i++)
      {
        D3D12RenderState::StreamOut &so = state.streamouts[StartSlot + i];

        WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferLocation, so.buf, so.offs);

        WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferFilledSizeLocation, so.countbuf,
                                                 so.countoffs);

        so.size = pViews[i].SizeInBytes;
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

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource1::GetResIDFromAddr(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetPipelineState(SerialiserType &ser,
                                                                  ID3D12PipelineState *pPipelineState)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pPipelineState);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))->SetPipelineState(Unwrap(pPipelineState));

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
          m_Cmd->m_RenderState.pipe = GetResID(pPipelineState);
      }
    }
    else
    {
      Unwrap(pCommandList)->SetPipelineState(Unwrap(pPipelineState));
      GetCrackedList()->SetPipelineState(Unwrap(pPipelineState));

      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.pipe = GetResID(pPipelineState);
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

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pPipelineState), eFrameRef_Read);
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

  std::vector<D3D12Descriptor> RTVs;

  if(ser.VersionAtLeast(0x5))
  {
    if(ser.IsWriting())
    {
      if(RTsSingleHandleToDescriptorRange)
      {
        if(pRenderTargetDescriptors && NumRenderTargetDescriptors > 0)
        {
          const D3D12Descriptor *descs = GetWrapped(pRenderTargetDescriptors[0]);

          RTVs.insert(RTVs.begin(), descs, descs + NumRenderTargetDescriptors);
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
    SERIALISE_ELEMENT(RTVs).Named("pRenderTargetDescriptors"_lit);
  }
  else
  {
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

          RTVs.insert(RTVs.begin(), descs, descs + NumRenderTargetDescriptors);
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
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> unwrappedRTs;
    unwrappedRTs.resize(RTVs.size());
    for(size_t i = 0; i < RTVs.size(); i++)
    {
      unwrappedRTs[i] =
          Unwrap(m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(RTVs[i], i));
    }

    D3D12_CPU_DESCRIPTOR_HANDLE unwrappedDSV = {};
    if(DSV.GetResResourceId() != ResourceId())
    {
      unwrappedDSV = Unwrap(m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(DSV));
    }

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->OMSetRenderTargets((UINT)unwrappedRTs.size(), unwrappedRTs.data(), FALSE,
                                 unwrappedDSV.ptr ? &unwrappedDSV : NULL);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          m_Cmd->m_RenderState.rts = RTVs;
          m_Cmd->m_RenderState.dsv = DSV;
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->OMSetRenderTargets((UINT)unwrappedRTs.size(), unwrappedRTs.data(), FALSE,
                               unwrappedDSV.ptr ? &unwrappedDSV : NULL);
      GetCrackedList()->OMSetRenderTargets((UINT)unwrappedRTs.size(), unwrappedRTs.data(), FALSE,
                                           unwrappedDSV.ptr ? &unwrappedDSV : NULL);

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

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < numHandles; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pRenderTargetDescriptors[i]);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetHeapResourceId(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(desc->GetResResourceId(), eFrameRef_PartialWrite);
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
  SERIALISE_ELEMENT(pRootSignature);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootSignature(Unwrap(pRootSignature));

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          // From the docs
          // (https://msdn.microsoft.com/en-us/library/windows/desktop/dn903950(v=vs.85).aspx)
          // "If a root signature is changed on a command list, all previous root signature bindings
          // become stale and all newly expected bindings must be set before Draw/Dispatch;
          // otherwise, the behavior is undefined. If the root signature is redundantly set to the
          // same one currently set, existing root signature bindings do not become stale."
          if(m_Cmd->m_RenderState.compute.rootsig != GetResID(pRootSignature))
            m_Cmd->m_RenderState.compute.sigelems.clear();

          m_Cmd->m_RenderState.compute.rootsig = GetResID(pRootSignature);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootSignature(Unwrap(pRootSignature));
      GetCrackedList()->SetComputeRootSignature(Unwrap(pRootSignature));

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.rootsig != GetResID(pRootSignature))
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

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pRootSignature), eFrameRef_Read);

    // store this so we can look up how many descriptors a given slot references, etc
    m_CurCompRootSig = GetWrapped(pRootSignature);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootDescriptorTable(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT(BaseDescriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.compute.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.compute.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.compute.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootTable,
                                                 GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                                 (UINT64)GetWrapped(BaseDescriptor)->GetHeapIndex());
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));
      GetCrackedList()->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.sigelems.size() < RootParameterIndex + 1)
        state.compute.sigelems.resize(RootParameterIndex + 1);

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

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                              eFrameRef_Read);

    std::vector<D3D12_DESCRIPTOR_RANGE1> &ranges =
        GetWrapped(m_CurCompRootSig)->sig.params[RootParameterIndex].ranges;

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

      if(!RenderDoc::Inst().GetCaptureOptions().refAllResources)
      {
        std::vector<D3D12Descriptor *> &descs = m_ListRecord->cmdInfo->boundDescs;

        for(UINT d = 0; d < num; d++)
          descs.push_back(rangeStart + d);
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
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT(SrcData);
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.compute.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.compute.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.compute.sigelems[RootParameterIndex].SetConstant(
              DestOffsetIn32BitValues, SrcData);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
      GetCrackedList()->SetComputeRoot32BitConstant(RootParameterIndex, SrcData,
                                                    DestOffsetIn32BitValues);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.sigelems.size() < RootParameterIndex + 1)
        state.compute.sigelems.resize(RootParameterIndex + 1);

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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRoot32BitConstants(
    SerialiserType &ser, UINT RootParameterIndex, UINT Num32BitValuesToSet,
    const void *pSrcVoidData, UINT DestOffsetIn32BitValues)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT(Num32BitValuesToSet);
  const UINT *pSrcData = (const UINT *)pSrcVoidData;
  SERIALISE_ELEMENT_ARRAY(pSrcData, Num32BitValuesToSet);
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                           DestOffsetIn32BitValues);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.compute.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.compute.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.compute.sigelems[RootParameterIndex].SetConstants(
              Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                         DestOffsetIn32BitValues);
      GetCrackedList()->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet,
                                                     pSrcData, DestOffsetIn32BitValues);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.sigelems.size() < RootParameterIndex + 1)
        state.compute.sigelems.resize(RootParameterIndex + 1);

      state.compute.sigelems[RootParameterIndex].SetConstants(Num32BitValuesToSet, pSrcData,
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
  SERIALISE_TIME_CALL(m_pList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet,
                                                            pSrcData, DestOffsetIn32BitValues));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetComputeRoot32BitConstants);
    Serialise_SetComputeRoot32BitConstants(ser, RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                           DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootConstantBufferView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.compute.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.compute.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.compute.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootCBV, id, offs);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
      GetCrackedList()->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.sigelems.size() < RootParameterIndex + 1)
        state.compute.sigelems.resize(RootParameterIndex + 1);

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
    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootShaderResourceView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.compute.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.compute.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.compute.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootSRV, id, offs);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
      GetCrackedList()->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.sigelems.size() < RootParameterIndex + 1)
        state.compute.sigelems.resize(RootParameterIndex + 1);

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
    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootUnorderedAccessView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.compute.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.compute.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.compute.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootUAV, id, offs);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
      GetCrackedList()->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.compute.sigelems.size() < RootParameterIndex + 1)
        state.compute.sigelems.resize(RootParameterIndex + 1);

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
    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
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
  SERIALISE_ELEMENT(pRootSignature);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootSignature(Unwrap(pRootSignature));

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          // From the docs
          // (https://msdn.microsoft.com/en-us/library/windows/desktop/dn903950(v=vs.85).aspx)
          // "If a root signature is changed on a command list, all previous root signature bindings
          // become stale and all newly expected bindings must be set before Draw/Dispatch;
          // otherwise, the behavior is undefined. If the root signature is redundantly set to the
          // same one currently set, existing root signature bindings do not become stale."
          if(m_Cmd->m_RenderState.graphics.rootsig != GetResID(pRootSignature))
            m_Cmd->m_RenderState.graphics.sigelems.clear();

          m_Cmd->m_RenderState.graphics.rootsig = GetResID(pRootSignature);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootSignature(Unwrap(pRootSignature));
      GetCrackedList()->SetGraphicsRootSignature(Unwrap(pRootSignature));

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.rootsig != GetResID(pRootSignature))
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

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT(BaseDescriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.graphics.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.graphics.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.graphics.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootTable,
                                                 GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                                 (UINT64)GetWrapped(BaseDescriptor)->GetHeapIndex());
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));
      GetCrackedList()->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.sigelems.size() < RootParameterIndex + 1)
        state.graphics.sigelems.resize(RootParameterIndex + 1);

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

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetWrapped(BaseDescriptor)->GetHeapResourceId(),
                                              eFrameRef_Read);

    std::vector<D3D12_DESCRIPTOR_RANGE1> &ranges =
        GetWrapped(m_CurGfxRootSig)->sig.params[RootParameterIndex].ranges;

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

      if(!RenderDoc::Inst().GetCaptureOptions().refAllResources)
      {
        std::vector<D3D12Descriptor *> &descs = m_ListRecord->cmdInfo->boundDescs;

        for(UINT d = 0; d < num; d++)
          descs.push_back(rangeStart + d);
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
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT(SrcData);
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.graphics.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.graphics.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.graphics.sigelems[RootParameterIndex].SetConstant(
              DestOffsetIn32BitValues, SrcData);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
      GetCrackedList()->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData,
                                                     DestOffsetIn32BitValues);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.sigelems.size() < RootParameterIndex + 1)
        state.graphics.sigelems.resize(RootParameterIndex + 1);

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

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRoot32BitConstants(
    SerialiserType &ser, UINT RootParameterIndex, UINT Num32BitValuesToSet,
    const void *pSrcVoidData, UINT DestOffsetIn32BitValues)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT(Num32BitValuesToSet);
  const UINT *pSrcData = (const UINT *)pSrcVoidData;
  SERIALISE_ELEMENT_ARRAY(pSrcData, Num32BitValuesToSet);
  SERIALISE_ELEMENT(DestOffsetIn32BitValues);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                            DestOffsetIn32BitValues);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.graphics.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.graphics.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.graphics.sigelems[RootParameterIndex].SetConstants(
              Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                          DestOffsetIn32BitValues);
      GetCrackedList()->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet,
                                                      pSrcData, DestOffsetIn32BitValues);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.sigelems.size() < RootParameterIndex + 1)
        state.graphics.sigelems.resize(RootParameterIndex + 1);

      state.graphics.sigelems[RootParameterIndex].SetConstants(Num32BitValuesToSet, pSrcData,
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
  SERIALISE_TIME_CALL(m_pList->SetGraphicsRoot32BitConstants(
      RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues));

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_SetGraphicsRoot32BitConstants);
    Serialise_SetGraphicsRoot32BitConstants(ser, RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                            DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootConstantBufferView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.graphics.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.graphics.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.graphics.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootCBV, id, offs);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
      GetCrackedList()->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.sigelems.size() < RootParameterIndex + 1)
        state.graphics.sigelems.resize(RootParameterIndex + 1);

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
    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootShaderResourceView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.graphics.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.graphics.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.graphics.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootSRV, id, offs);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
      GetCrackedList()->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.sigelems.size() < RootParameterIndex + 1)
        state.graphics.sigelems.resize(RootParameterIndex + 1);

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
    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootUnorderedAccessView(
    SerialiserType &ser, UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(RootParameterIndex);
  SERIALISE_ELEMENT_TYPED(D3D12BufferLocation, BufferLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(ValidateRootGPUVA(BufferLocation))
      return true;

    ResourceId id;
    uint64_t offs;

    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);

        if(m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
        {
          if(m_Cmd->m_RenderState.graphics.sigelems.size() < RootParameterIndex + 1)
            m_Cmd->m_RenderState.graphics.sigelems.resize(RootParameterIndex + 1);

          m_Cmd->m_RenderState.graphics.sigelems[RootParameterIndex] =
              D3D12RenderState::SignatureElement(eRootUAV, id, offs);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
      GetCrackedList()->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);

      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state;

      if(state.graphics.sigelems.size() < RootParameterIndex + 1)
        state.graphics.sigelems.resize(RootParameterIndex + 1);

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
    WrappedID3D12Resource1::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(pQueryHeap);
  SERIALISE_ELEMENT(Type);
  SERIALISE_ELEMENT(Index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
      }
    }
    else
    {
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

    m_ListRecord->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(pQueryHeap);
  SERIALISE_ELEMENT(Type);
  SERIALISE_ELEMENT(Index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
      }
    }
    else
    {
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_EndQuery);
    Serialise_EndQuery(ser, pQueryHeap, Type, Index);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ResolveQueryData(
    SerialiserType &ser, ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex,
    UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pQueryHeap);
  SERIALISE_ELEMENT(Type);
  SERIALISE_ELEMENT(StartIndex);
  SERIALISE_ELEMENT(NumQueries);
  SERIALISE_ELEMENT(pDestinationBuffer);
  SERIALISE_ELEMENT(AlignedDestinationBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
      }
    }
    else
    {
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

    m_ListRecord->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(pBuffer);
  SERIALISE_ELEMENT(AlignedBufferOffset);
  SERIALISE_ELEMENT(Operation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    // don't replay predication at all
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

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pBuffer), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_SetMarker(SerialiserType &ser, UINT Metadata,
                                                           const void *pData, UINT Size)
{
  std::string MarkerText = "";

  if(ser.IsWriting() && pData && Size)
    MarkerText = DecodeMarkerString(Metadata, pData, Size);

  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(MarkerText);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        D3D12MarkerRegion::Set(list, MarkerText);
      }
    }
    else
    {
      D3D12MarkerRegion::Set(pCommandList, MarkerText);
      D3D12MarkerRegion::Set(GetWrappedCrackedList(), MarkerText);

      DrawcallDescription draw;
      draw.name = MarkerText;
      draw.flags |= DrawFlags::SetMarker;

      m_Cmd->AddEvent();
      m_Cmd->AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  SERIALISE_TIME_CALL(m_pList->SetMarker(Metadata, pData, Size));

  if(m_AMDMarkers && Metadata == PIX_EVENT_UNICODE_VERSION)
    m_AMDMarkers->SetMarker(StringFormat::Wide2UTF8((const wchar_t *)pData).c_str());

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetMarker);
    Serialise_SetMarker(ser, Metadata, pData, Size);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_BeginEvent(SerialiserType &ser, UINT Metadata,
                                                            const void *pData, UINT Size)
{
  std::string MarkerText = "";

  if(ser.IsWriting() && pData && Size)
    MarkerText = DecodeMarkerString(Metadata, pData, Size);

  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(MarkerText);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].markerCount++;

        D3D12MarkerRegion::Begin(list, MarkerText);
      }
    }
    else
    {
      D3D12MarkerRegion::Begin(pCommandList, MarkerText);
      D3D12MarkerRegion::Begin(GetWrappedCrackedList(), MarkerText);

      DrawcallDescription draw;
      draw.name = MarkerText;
      draw.flags |= DrawFlags::PushMarker;

      m_Cmd->AddEvent();
      m_Cmd->AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
  SERIALISE_TIME_CALL(m_pList->BeginEvent(Metadata, pData, Size));

  if(m_AMDMarkers && Metadata == PIX_EVENT_UNICODE_VERSION)
    m_AMDMarkers->PushMarker(StringFormat::Wide2UTF8((const wchar_t *)pData).c_str());

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::PushMarker);
    Serialise_BeginEvent(ser, Metadata, pData, Size);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_EndEvent(SerialiserType &ser)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        int &markerCount = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].markerCount;
        markerCount = RDCMAX(0, markerCount - 1);

        D3D12MarkerRegion::End(list);
      }
    }
    else
    {
      if(m_Cmd->HasNonMarkerEvents(m_Cmd->m_LastCmdListID))
      {
        DrawcallDescription draw;
        draw.name = "API Calls";
        draw.flags = DrawFlags::APICalls;

        m_Cmd->AddDrawcall(draw, true);
      }

      D3D12MarkerRegion::End(pCommandList);
      D3D12MarkerRegion::End(GetWrappedCrackedList());

      // dummy draw that is consumed when this command buffer
      // is being in-lined into the call stream
      DrawcallDescription draw;
      draw.name = "Pop()";
      draw.flags = DrawFlags::PopMarker;

      m_Cmd->AddEvent();
      m_Cmd->AddDrawcall(draw, false);
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::PopMarker);
    Serialise_EndEvent(ser);

    m_ListRecord->AddChunk(scope.Get());
  }
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
  SERIALISE_ELEMENT(VertexCountPerInstance);
  SERIALISE_ELEMENT(InstanceCount);
  SERIALISE_ELEMENT(StartVertexLocation);
  SERIALISE_ELEMENT(StartInstanceLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list);

        Unwrap(list)->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                    StartInstanceLocation);

        if(eventId && m_Cmd->m_DrawcallCallback->PostDraw(eventId, list))
        {
          Unwrap(list)->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                      StartInstanceLocation);
          m_Cmd->m_DrawcallCallback->PostRedraw(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                          StartInstanceLocation);
      GetCrackedList()->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                                      StartInstanceLocation);

      m_Cmd->AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("DrawInstanced(%u, %u)", VertexCountPerInstance, InstanceCount);
      draw.numIndices = VertexCountPerInstance;
      draw.numInstances = InstanceCount;
      draw.indexOffset = 0;
      draw.vertexOffset = StartVertexLocation;
      draw.instanceOffset = StartInstanceLocation;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

      m_Cmd->AddDrawcall(draw, true);
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_DrawInstanced);
    Serialise_DrawInstanced(ser, VertexCountPerInstance, InstanceCount, StartVertexLocation,
                            StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_DrawIndexedInstanced(
    SerialiserType &ser, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(IndexCountPerInstance);
  SERIALISE_ELEMENT(InstanceCount);
  SERIALISE_ELEMENT(StartIndexLocation);
  SERIALISE_ELEMENT(BaseVertexLocation);
  SERIALISE_ELEMENT(StartInstanceLocation);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list);

        Unwrap(list)->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                           BaseVertexLocation, StartInstanceLocation);

        if(eventId && m_Cmd->m_DrawcallCallback->PostDraw(eventId, list))
        {
          Unwrap(list)->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                             BaseVertexLocation, StartInstanceLocation);
          m_Cmd->m_DrawcallCallback->PostRedraw(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                 BaseVertexLocation, StartInstanceLocation);
      GetCrackedList()->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                             BaseVertexLocation, StartInstanceLocation);

      m_Cmd->AddEvent();

      DrawcallDescription draw;
      draw.name =
          StringFormat::Fmt("DrawIndexedInstanced(%u, %u)", IndexCountPerInstance, InstanceCount);
      draw.numIndices = IndexCountPerInstance;
      draw.numInstances = InstanceCount;
      draw.indexOffset = StartIndexLocation;
      draw.baseVertex = BaseVertexLocation;
      draw.instanceOffset = StartInstanceLocation;

      draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed;

      m_Cmd->AddDrawcall(draw, true);
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_DrawIndexedInstanced);
    Serialise_DrawIndexedInstanced(ser, IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                   BaseVertexLocation, StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_Dispatch(SerialiserType &ser, UINT ThreadGroupCountX,
                                                          UINT ThreadGroupCountY,
                                                          UINT ThreadGroupCountZ)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(ThreadGroupCountX);
  SERIALISE_ELEMENT(ThreadGroupCountY);
  SERIALISE_ELEMENT(ThreadGroupCountZ);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, true);

        Unwrap(list)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

        if(eventId && m_Cmd->m_DrawcallCallback->PostDraw(eventId, list))
        {
          Unwrap(list)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
          m_Cmd->m_DrawcallCallback->PostRedraw(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
      GetCrackedList()->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

      m_Cmd->AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("Dispatch(%u, %u, %u)", ThreadGroupCountX, ThreadGroupCountY,
                                    ThreadGroupCountZ);
      draw.dispatchDimension[0] = ThreadGroupCountX;
      draw.dispatchDimension[1] = ThreadGroupCountY;
      draw.dispatchDimension[2] = ThreadGroupCountZ;

      draw.flags |= DrawFlags::Dispatch;

      m_Cmd->AddDrawcall(draw, true);
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Dispatch);
    Serialise_Dispatch(ser, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    m_ListRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ExecuteBundle(SerialiserType &ser,
                                                               ID3D12GraphicsCommandList *pBundle)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pBundle);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_pDevice->APIProps.D3D12Bundle = true;

    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        uint32_t eventId = m_Cmd->HandlePreCallback(list, true);

        Unwrap(list)->ExecuteBundle(Unwrap(pBundle));

        if(eventId && m_Cmd->m_DrawcallCallback->PostDraw(eventId, list))
        {
          Unwrap(list)->ExecuteBundle(Unwrap(pBundle));
          m_Cmd->m_DrawcallCallback->PostRedraw(eventId, list);
        }
      }
    }
    else
    {
      Unwrap(pCommandList)->ExecuteBundle(Unwrap(pBundle));
      GetCrackedList()->ExecuteBundle(Unwrap(pBundle));

      m_Cmd->AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt(
          "ExecuteBundle(%s)", ToStr(GetResourceManager()->GetOriginalID(GetResID(pBundle))).c_str());

      draw.flags |= DrawFlags::CmdList;

      m_Cmd->AddDrawcall(draw, true);
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ExecuteBundle);
    Serialise_ExecuteBundle(ser, pCommandList);

    m_ListRecord->AddChunk(scope.Get());

    D3D12ResourceRecord *record = GetRecord(pCommandList);

    CmdListRecordingInfo *dst = m_ListRecord->cmdInfo;
    CmdListRecordingInfo *src = record->bakedCommands->cmdInfo;
    dst->boundDescs.insert(dst->boundDescs.end(), src->boundDescs.begin(), src->boundDescs.end());
    dst->dirtied.insert(src->dirtied.begin(), src->dirtied.end());

    dst->bundles.push_back(record);
  }
}

/*
 * ExecuteIndirect needs special handling - whenever we encounter an ExecuteIndirect during loading
 * time we crack the list into two, and copy off the argument buffer in the first part and execute
 * with the copy destination in the second part.
 *
 * Then when we come to ExecuteCommandLists this list, we go step by step through the cracked lists,
 * executing the first, then syncing to the GPU and patching the argument buffer before continuing.
 *
 * At loading time we reserve a maxCount number of drawcalls and events, and later on when patching
 * the argument buffer we fill in the parameters/names and remove any excess draws that weren't
 * actually executed.
 *
 * During active replaying we read the patched argument buffer and execute any commands needed by
 * hand on the CPU.
 */

void WrappedID3D12GraphicsCommandList::ReserveExecuteIndirect(ID3D12GraphicsCommandList *list,
                                                              WrappedID3D12CommandSignature *comSig,
                                                              UINT maxCount)
{
  const bool multidraw = (maxCount > 1 || comSig->sig.numDraws > 1);
  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();

  RDCASSERT(IsLoading(m_State));

  BakedCmdListInfo &cmdInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

  for(uint32_t i = 0; i < maxCount; i++)
  {
    for(uint32_t a = 0; a < sigSize; a++)
    {
      const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[a];

      switch(arg.Type)
      {
        case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
          // add dummy event and drawcall
          m_Cmd->AddEvent();
          m_Cmd->AddDrawcall(DrawcallDescription(), true);
          m_Cmd->GetDrawcallStack().back()->children.back().state =
              new D3D12RenderState(cmdInfo.state);
          cmdInfo.curEventID++;
          break;
        case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
          // add dummy event
          m_Cmd->AddEvent();
          cmdInfo.curEventID++;
          break;
        default: RDCERR("Unexpected argument type! %d", arg.Type); break;
      }
    }
  }

  if(multidraw)
  {
    DrawcallDescription draw;
    draw.name = "ExecuteIndirect()";
    draw.flags = DrawFlags::PopMarker;
    m_Cmd->AddDrawcall(draw, false);
  }
  else
  {
    cmdInfo.curEventID--;
  }
}

void WrappedID3D12GraphicsCommandList::PatchExecuteIndirect(BakedCmdListInfo &info,
                                                            uint32_t executeIndex)
{
  BakedCmdListInfo::ExecuteData &exec = info.executeEvents[executeIndex];

  exec.patched = true;

  WrappedID3D12CommandSignature *comSig = exec.sig;

  uint32_t count = exec.maxCount;

  if(exec.countBuf)
  {
    bytebuf data;
    m_pDevice->GetReplay()->GetDebugManager()->GetBufferData(exec.countBuf, exec.countOffs, 4, data);
    count = RDCMIN(count, *(uint32_t *)&data[0]);
  }

  exec.realCount = count;

  const bool multidraw = (count > 1 || comSig->sig.numDraws > 1);
  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();
  const bool gfx = comSig->sig.graphics;

  // + 1 is because baseEvent refers to the marker before the commands
  exec.lastEvent = exec.baseEvent + 1 + sigSize * count;

  D3D12_RANGE range = {0, D3D12CommandData::m_IndirectSize};
  byte *mapPtr = NULL;
  exec.argBuf->Map(0, &range, (void **)&mapPtr);

  std::vector<D3D12DrawcallTreeNode> &draws = info.draw->children;

  size_t idx = 0;
  uint32_t eid = exec.baseEvent;

  // find the draw where our execute begins
  for(; idx < draws.size(); idx++)
    if(draws[idx].draw.eventId == eid)
      break;

  RDCASSERTMSG("Couldn't find base event draw!", idx < draws.size(), idx, draws.size());

  // patch the name for the base drawcall
  draws[idx].draw.name =
      StringFormat::Fmt("ExecuteIndirect(maxCount %u, count <%u>)", exec.maxCount, count);
  // if there's only one command running, remove its pushmarker flag
  if(!multidraw)
    draws[idx].draw.flags = (draws[idx].draw.flags & ~DrawFlags::PushMarker) | DrawFlags::SetMarker;

  // move to the first actual draw of the commands
  idx++;
  eid++;

  RDCASSERT(draws[idx].state);

  D3D12RenderState state = *draws[idx].state;

  SDChunk *baseChunk = m_Cmd->m_StructuredFile->chunks[draws[idx].draw.events[0].chunkIndex];

  for(uint32_t i = 0; i < count; i++)
  {
    byte *data = mapPtr + exec.argOffs;
    mapPtr += comSig->sig.ByteStride;

    for(uint32_t a = 0; a < sigSize; a++)
    {
      const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[a];

      DrawcallDescription &curDraw = draws[idx].draw;

      APIEvent *curEvent = NULL;

      for(APIEvent &ev : curDraw.events)
      {
        if(ev.eventId == eid)
        {
          curEvent = &ev;
          break;
        }
      }

      APIEvent dummy;
      if(!curEvent)
      {
        RDCWARN("Couldn't find EID %u in current draw while patching ExecuteIndirect", eid);
        // assign a dummy so we don't have to NULL-check below
        curEvent = &dummy;
      }

      SDChunk *fakeChunk = new SDChunk("");
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
            D3D12_DRAW_ARGUMENTS *args = (D3D12_DRAW_ARGUMENTS *)data;
            data += sizeof(D3D12_DRAW_ARGUMENTS);

            curDraw.drawIndex = a;
            curDraw.numIndices = args->VertexCountPerInstance;
            curDraw.numInstances = args->InstanceCount;
            curDraw.vertexOffset = args->StartVertexLocation;
            curDraw.instanceOffset = args->StartInstanceLocation;
            curDraw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;
            curDraw.name = StringFormat::Fmt("[%u] arg%u: IndirectDraw(<%u, %u>)", i, a,
                                             curDraw.numIndices, curDraw.numInstances);

            fakeChunk->name = curDraw.name;

            structuriser.Serialise("ArgumentData"_lit, *args);

            // if this is the first draw of the indirect, we could have picked up previous
            // non-indirect events in this drawcall, so the EID will be higher than we expect. Just
            // assign the draw's EID
            eid = curDraw.eventId;

            m_Cmd->AddUsage(state, draws[idx]);

            // advance
            idx++;
            eid++;

            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
          {
            D3D12_DRAW_INDEXED_ARGUMENTS *args = (D3D12_DRAW_INDEXED_ARGUMENTS *)data;
            data += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

            curDraw.drawIndex = a;
            curDraw.numIndices = args->IndexCountPerInstance;
            curDraw.numInstances = args->InstanceCount;
            curDraw.baseVertex = args->BaseVertexLocation;
            curDraw.indexOffset = args->StartIndexLocation;
            curDraw.instanceOffset = args->StartInstanceLocation;
            curDraw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indexed |
                             DrawFlags::Indirect;
            curDraw.name = StringFormat::Fmt("[%u] arg%u: IndirectDrawIndexed(<%u, %u>)", i, a,
                                             curDraw.numIndices, curDraw.numInstances);

            fakeChunk->name = curDraw.name;

            structuriser.Serialise("ArgumentData"_lit, *args);

            // if this is the first draw of the indirect, we could have picked up previous
            // non-indirect events in this drawcall, so the EID will be higher than we expect. Just
            // assign the draw's EID
            eid = curDraw.eventId;

            m_Cmd->AddUsage(state, draws[idx]);

            // advance
            idx++;
            eid++;

            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
          {
            D3D12_DISPATCH_ARGUMENTS *args = (D3D12_DISPATCH_ARGUMENTS *)data;
            data += sizeof(D3D12_DISPATCH_ARGUMENTS);

            curDraw.dispatchDimension[0] = args->ThreadGroupCountX;
            curDraw.dispatchDimension[1] = args->ThreadGroupCountY;
            curDraw.dispatchDimension[2] = args->ThreadGroupCountZ;
            curDraw.flags |= DrawFlags::Dispatch | DrawFlags::Indirect;
            curDraw.name = StringFormat::Fmt(
                "[%u] arg%u: IndirectDispatch(<%u, %u, %u>)", i, a, curDraw.dispatchDimension[0],
                curDraw.dispatchDimension[1], curDraw.dispatchDimension[2]);

            fakeChunk->name = curDraw.name;

            structuriser.Serialise("ArgumentData"_lit, *args);

            // if this is the first draw of the indirect, we could have picked up previous
            // non-indirect events in this drawcall, so the EID will be higher than we expect. Just
            // assign the draw's EID
            eid = curDraw.eventId;

            m_Cmd->AddUsage(state, draws[idx]);

            // advance
            idx++;
            eid++;

            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
          {
            size_t argSize = sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
            uint32_t *data32 = (uint32_t *)data;
            data += argSize;

            fakeChunk->name = StringFormat::Fmt("[%u] arg%u: IndirectSetRoot32BitConstants()", i, a);

            structuriser.Serialise("Values"_lit, data32, arg.Constant.Num32BitValuesToSet);

            if(arg.Constant.RootParameterIndex < state.graphics.sigelems.size())
              state.graphics.sigelems[arg.Constant.RootParameterIndex].constants.assign(
                  data32, data32 + arg.Constant.Num32BitValuesToSet);

            if(arg.Constant.RootParameterIndex < state.compute.sigelems.size())
              state.compute.sigelems[arg.Constant.RootParameterIndex].constants.assign(
                  data32, data32 + arg.Constant.Num32BitValuesToSet);

            // advance only the EID, since we're still in the same draw
            eid++;

            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
          {
            D3D12_VERTEX_BUFFER_VIEW *vb = (D3D12_VERTEX_BUFFER_VIEW *)data;
            data += sizeof(D3D12_VERTEX_BUFFER_VIEW);

            ResourceId id;
            uint64_t offs = 0;
            m_pDevice->GetResIDFromAddr(vb->BufferLocation, id, offs);

            ID3D12Resource *res = GetResourceManager()->GetLiveAs<ID3D12Resource>(id);
            RDCASSERT(res);
            if(res)
              vb->BufferLocation = res->GetGPUVirtualAddress() + offs;

            if(arg.VertexBuffer.Slot >= state.vbuffers.size())
              state.vbuffers.resize(arg.VertexBuffer.Slot + 1);

            state.vbuffers[arg.VertexBuffer.Slot].buf = id;
            state.vbuffers[arg.VertexBuffer.Slot].offs = offs;
            state.vbuffers[arg.VertexBuffer.Slot].size = vb->SizeInBytes;
            state.vbuffers[arg.VertexBuffer.Slot].stride = vb->StrideInBytes;

            fakeChunk->name = StringFormat::Fmt("[%u] arg%u: IndirectIASetVertexBuffer()", i, a);

            structuriser.Serialise("ArgumentData"_lit, *vb);

            // advance only the EID, since we're still in the same draw
            eid++;

            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
          {
            D3D12_INDEX_BUFFER_VIEW *ib = (D3D12_INDEX_BUFFER_VIEW *)data;
            data += sizeof(D3D12_INDEX_BUFFER_VIEW);

            ResourceId id;
            uint64_t offs = 0;
            m_pDevice->GetResIDFromAddr(ib->BufferLocation, id, offs);

            ID3D12Resource *res = GetResourceManager()->GetLiveAs<ID3D12Resource>(id);
            RDCASSERT(res);
            if(res)
              ib->BufferLocation = res->GetGPUVirtualAddress() + offs;

            state.ibuffer.buf = id;
            state.ibuffer.offs = offs;
            state.ibuffer.size = ib->SizeInBytes;
            state.ibuffer.bytewidth = ib->Format == DXGI_FORMAT_R32_UINT ? 4 : 2;

            fakeChunk->name = StringFormat::Fmt("[%u] arg%u: IndirectIASetIndexBuffer()", i, a);

            structuriser.Serialise("ArgumentData"_lit, *ib);

            // advance only the EID, since we're still in the same draw
            eid++;

            break;
          }
          case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
          case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
          case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
          {
            D3D12_GPU_VIRTUAL_ADDRESS *addr = (D3D12_GPU_VIRTUAL_ADDRESS *)data;
            data += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);

            ResourceId id;
            uint64_t offs = 0;
            m_pDevice->GetResIDFromAddr(*addr, id, offs);

            ID3D12Resource *res = GetResourceManager()->GetLiveAs<ID3D12Resource>(id);
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
                StringFormat::Fmt("[%u] arg%u: IndirectSetRoot%sView()", i, a, viewTypeStr);

            D3D12BufferLocation buf = *addr;

            structuriser.Serialise("ArgumentData"_lit, buf);

            // advance only the EID, since we're still in the same draw
            eid++;

            break;
          }
          default: RDCERR("Unexpected argument type! %d", arg.Type); break;
        }
      }

      m_Cmd->m_StructuredFile->chunks.push_back(fakeChunk);

      curEvent->chunkIndex = uint32_t(m_Cmd->m_StructuredFile->chunks.size() - 1);
    }
  }

  exec.argBuf->Unmap(0, &range);

  // remove excesss draws if count < maxCount
  if(count < exec.maxCount)
  {
    uint32_t shiftEID = (exec.maxCount - count) * sigSize;
    uint32_t lastEID = exec.baseEvent + 1 + sigSize * exec.maxCount;

    uint32_t shiftDrawID = 0;

    while(idx < draws.size() && draws[idx].draw.eventId < lastEID)
    {
      draws.erase(draws.begin() + idx);
      shiftDrawID++;
    }

    // shift all subsequent EIDs and drawcall IDs so they're contiguous
    info.ShiftForRemoved(shiftDrawID, shiftEID, idx);
  }

  if(!multidraw && exec.maxCount > 1)
  {
    // remove pop event
    draws.erase(draws.begin() + idx);

    info.ShiftForRemoved(1, 1, idx);
  }
}

void WrappedID3D12GraphicsCommandList::ReplayExecuteIndirect(ID3D12GraphicsCommandList *list)
{
  BakedCmdListInfo &cmdInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

  size_t executeIndex = cmdInfo.executeEvents.size();

  for(size_t i = 0; i < cmdInfo.executeEvents.size(); i++)
  {
    if(cmdInfo.executeEvents[i].baseEvent <= cmdInfo.curEventID &&
       cmdInfo.curEventID < cmdInfo.executeEvents[i].lastEvent)
    {
      executeIndex = i;
      break;
    }
  }

  if(executeIndex >= cmdInfo.executeEvents.size())
  {
    RDCERR("Couldn't find ExecuteIndirect to replay!");
    return;
  }

  BakedCmdListInfo::ExecuteData &exec = cmdInfo.executeEvents[executeIndex];

  WrappedID3D12CommandSignature *comSig = exec.sig;

  uint32_t count = exec.realCount;
  uint32_t origCount = exec.realCount;

  const bool multidraw = (count > 1 || comSig->sig.numDraws > 1);

  const bool gfx = comSig->sig.graphics;
  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();

  // if we're partial then continue to emulate & replay, otherwise use the patched buffer
  if(!m_Cmd->IsPartialCmdList(m_Cmd->m_LastCmdListID))
  {
    list->ExecuteIndirect(comSig->GetReal(), exec.maxCount, Unwrap(exec.argBuf), exec.argOffs,
                          Unwrap(exec.countBuf), exec.countOffs);

    // skip past all the events
    cmdInfo.curEventID += origCount * sigSize;

    // skip past the pop event
    if(multidraw)
      cmdInfo.curEventID++;

    return;
  }

  bytebuf data;
  m_pDevice->GetReplay()->GetDebugManager()->GetBufferData(exec.argBuf, exec.argOffs,
                                                           count * comSig->sig.ByteStride, data);

  byte *dataPtr = &data[0];

  std::vector<D3D12RenderState::SignatureElement> &sigelems =
      gfx ? m_Cmd->m_RenderState.graphics.sigelems : m_Cmd->m_RenderState.compute.sigelems;

  // while executing, decide where to start and stop. We do this by modifying the max count and
  // noting which arg we should start working, and which arg in the last execute we should get up
  // to. Since we don't actually replay as indirect executes to save on having to allocate and
  // manage indirect buffers across the command list, we can just skip commands we don't want to
  // execute.

  uint32_t firstCommand = 0;
  uint32_t firstArg = 0;
  uint32_t lastArg = ~0U;

  {
    uint32_t curEID = m_Cmd->m_RootEventID;

    if(m_Cmd->m_FirstEventID <= 1)
    {
      curEID = cmdInfo.curEventID;

      if(m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent == m_Cmd->m_LastCmdListID)
        curEID += m_Cmd->m_Partial[D3D12CommandData::Primary].baseEvent;
      else if(m_Cmd->m_Partial[D3D12CommandData::Secondary].partialParent == m_Cmd->m_LastCmdListID)
        curEID += m_Cmd->m_Partial[D3D12CommandData::Secondary].baseEvent;
    }

    D3D12CommandData::DrawcallUse use(m_Cmd->m_CurChunkOffset, 0);
    auto it = std::lower_bound(m_Cmd->m_DrawcallUses.begin(), m_Cmd->m_DrawcallUses.end(), use);

    if(it == m_Cmd->m_DrawcallUses.end())
    {
      RDCERR("Unexpected drawcall not found in uses vector, offset %llu", m_Cmd->m_CurChunkOffset);
    }
    else
    {
      uint32_t baseEventID = it->eventId;

      // TODO when using a drawcall callback, we should submit every drawcall individually
      if(m_Cmd->m_DrawcallCallback)
      {
        firstCommand = 0;
        firstArg = 0;
        lastArg = ~0U;
      }
      // To add the execute, we made an event N that is the 'parent' marker, then
      // N+1, N+2, N+3, ... for each of the arguments. If the first sub-argument is selected
      // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
      // the first sub-draw in that range.
      else if(m_Cmd->m_LastEventID > baseEventID)
      {
        if(m_Cmd->m_FirstEventID <= 1)
        {
          // one event per arg, and N args per command
          uint32_t numArgs = m_Cmd->m_LastEventID - baseEventID;

          // play all commands up to the one we want
          firstCommand = 0;

          // how many commands?
          uint32_t numCmds = numArgs / sigSize + 1;
          count = RDCMIN(count, numCmds);

          // play all args in the fnial commmad up to the one we want
          firstArg = 0;

          // how many args in the final command
          if(numCmds > count)
            lastArg = ~0U;
          else
            lastArg = numArgs % sigSize;
        }
        else
        {
          // note we'll never be asked to do e.g. 3rd-7th commands of an execute. Only ever 0th-nth
          // or
          // a single argument.
          uint32_t argIdx = (curEID - baseEventID - 1);

          firstCommand = argIdx / sigSize;
          count = RDCMIN(count, firstCommand + 1);

          firstArg = argIdx % sigSize;
          lastArg = firstArg + 1;
        }
      }
      else
      {
        // don't do anything, we've selected the base event
        count = 0;
      }
    }
  }

  bool executing = true;

  for(uint32_t i = 0; i < count; i++)
  {
    byte *src = dataPtr;
    dataPtr += comSig->sig.ByteStride;

    // don't have to do an upper bound on commands, count was modified
    if(i < firstCommand)
      continue;

    for(uint32_t a = 0; a < sigSize; a++)
    {
      const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[a];

      // only execute things while we're in the range we want
      // on the last command count, stop executing once we're past the last arg.
      if(i == count - 1)
        executing = (a >= firstArg && a < lastArg);

      switch(arg.Type)
      {
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
        {
          D3D12_DRAW_ARGUMENTS *args = (D3D12_DRAW_ARGUMENTS *)src;
          src += sizeof(D3D12_DRAW_ARGUMENTS);

          if(executing)
            list->DrawInstanced(args->VertexCountPerInstance, args->InstanceCount,
                                args->StartVertexLocation, args->StartInstanceLocation);
          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
        {
          D3D12_DRAW_INDEXED_ARGUMENTS *args = (D3D12_DRAW_INDEXED_ARGUMENTS *)src;
          src += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

          if(executing)
            list->DrawIndexedInstanced(args->IndexCountPerInstance, args->InstanceCount,
                                       args->StartIndexLocation, args->BaseVertexLocation,
                                       args->StartInstanceLocation);
          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
        {
          D3D12_DISPATCH_ARGUMENTS *args = (D3D12_DISPATCH_ARGUMENTS *)src;
          src += sizeof(D3D12_DISPATCH_ARGUMENTS);

          if(executing)
            list->Dispatch(args->ThreadGroupCountX, args->ThreadGroupCountY, args->ThreadGroupCountZ);

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        {
          size_t argSize = sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
          uint32_t *values = (uint32_t *)src;
          src += argSize;

          if(executing)
          {
            if(sigelems.size() < arg.Constant.RootParameterIndex + 1)
              sigelems.resize(arg.Constant.RootParameterIndex + 1);

            sigelems[arg.Constant.RootParameterIndex].SetConstants(
                arg.Constant.Num32BitValuesToSet, values, arg.Constant.DestOffsetIn32BitValues);

            if(gfx)
              list->SetGraphicsRoot32BitConstants(arg.Constant.RootParameterIndex,
                                                  arg.Constant.Num32BitValuesToSet, values,
                                                  arg.Constant.DestOffsetIn32BitValues);
            else
              list->SetComputeRoot32BitConstants(arg.Constant.RootParameterIndex,
                                                 arg.Constant.Num32BitValuesToSet, values,
                                                 arg.Constant.DestOffsetIn32BitValues);
          }

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
        {
          D3D12_VERTEX_BUFFER_VIEW *srcVB = (D3D12_VERTEX_BUFFER_VIEW *)src;
          src += sizeof(D3D12_VERTEX_BUFFER_VIEW);

          ResourceId id;
          uint64_t offs = 0;
          WrappedID3D12Resource1::GetResIDFromAddr(srcVB->BufferLocation, id, offs);
          RDCASSERT(id != ResourceId());

          if(executing)
          {
            if(m_Cmd->m_RenderState.vbuffers.size() < arg.VertexBuffer.Slot + 1)
              m_Cmd->m_RenderState.vbuffers.resize(arg.VertexBuffer.Slot + 1);

            m_Cmd->m_RenderState.vbuffers[arg.VertexBuffer.Slot].buf = id;
            m_Cmd->m_RenderState.vbuffers[arg.VertexBuffer.Slot].offs = offs;
            m_Cmd->m_RenderState.vbuffers[arg.VertexBuffer.Slot].size = srcVB->SizeInBytes;
            m_Cmd->m_RenderState.vbuffers[arg.VertexBuffer.Slot].stride = srcVB->StrideInBytes;
          }

          if(executing && id != ResourceId())
            list->IASetVertexBuffers(arg.VertexBuffer.Slot, 1, srcVB);

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
        {
          D3D12_INDEX_BUFFER_VIEW *srcIB = (D3D12_INDEX_BUFFER_VIEW *)src;
          src += sizeof(D3D12_INDEX_BUFFER_VIEW);

          ResourceId id;
          uint64_t offs = 0;
          WrappedID3D12Resource1::GetResIDFromAddr(srcIB->BufferLocation, id, offs);
          RDCASSERT(id != ResourceId());

          if(executing)
          {
            m_Cmd->m_RenderState.ibuffer.buf = id;
            m_Cmd->m_RenderState.ibuffer.offs = offs;
            m_Cmd->m_RenderState.ibuffer.size = srcIB->SizeInBytes;
            m_Cmd->m_RenderState.ibuffer.bytewidth = (srcIB->Format == DXGI_FORMAT_R32_UINT ? 4 : 2);
          }

          if(executing && id != ResourceId())
            list->IASetIndexBuffer(srcIB);

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
        {
          D3D12_GPU_VIRTUAL_ADDRESS *srcAddr = (D3D12_GPU_VIRTUAL_ADDRESS *)src;
          src += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);

          ResourceId id;
          uint64_t offs = 0;
          WrappedID3D12Resource1::GetResIDFromAddr(*srcAddr, id, offs);
          RDCASSERT(*srcAddr == 0 || id != ResourceId());

          const uint32_t rootIdx = arg.Constant.RootParameterIndex;

          SignatureElementType elemType = eRootUnknown;

          if(gfx)
          {
            if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
            {
              elemType = eRootCBV;

              if(executing)
              {
                if(id != ResourceId())
                  list->SetGraphicsRootConstantBufferView(rootIdx, *srcAddr);
                else
                  list->SetGraphicsRootConstantBufferView(rootIdx, 0);
              }
            }
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
            {
              elemType = eRootSRV;

              if(executing)
              {
                if(id != ResourceId())
                  list->SetGraphicsRootShaderResourceView(rootIdx, *srcAddr);
                else
                  list->SetGraphicsRootShaderResourceView(rootIdx, 0);
              }
            }
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
            {
              elemType = eRootUAV;

              if(executing)
              {
                if(id != ResourceId())
                  list->SetGraphicsRootUnorderedAccessView(rootIdx, *srcAddr);
                else
                  list->SetGraphicsRootUnorderedAccessView(rootIdx, 0);
              }
            }
            else
            {
              RDCERR("Unexpected argument type! %d", arg.Type);
            }
          }
          else
          {
            if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
            {
              elemType = eRootCBV;

              if(executing && id != ResourceId())
                list->SetComputeRootConstantBufferView(rootIdx, *srcAddr);
            }
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
            {
              elemType = eRootSRV;

              if(executing && id != ResourceId())
                list->SetComputeRootShaderResourceView(rootIdx, *srcAddr);
            }
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
            {
              elemType = eRootUAV;

              if(executing && id != ResourceId())
                list->SetComputeRootUnorderedAccessView(rootIdx, *srcAddr);
            }
            else
            {
              RDCERR("Unexpected argument type! %d", arg.Type);
            }
          }

          if(executing)
          {
            if(sigelems.size() < rootIdx + 1)
              sigelems.resize(rootIdx + 1);

            sigelems[rootIdx] = D3D12RenderState::SignatureElement(elemType, id, offs);
          }

          break;
        }
        default: RDCERR("Unexpected argument type! %d", arg.Type); break;
      }
    }
  }

  // skip past all the events
  cmdInfo.curEventID += origCount * sigSize;

  // skip past the pop event
  if(multidraw)
    cmdInfo.curEventID++;
}

template <typename SerialiserType>
bool WrappedID3D12GraphicsCommandList::Serialise_ExecuteIndirect(
    SerialiserType &ser, ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount,
    ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer,
    UINT64 CountBufferOffset)
{
  ID3D12GraphicsCommandList *pCommandList = this;
  SERIALISE_ELEMENT(pCommandList);
  SERIALISE_ELEMENT(pCommandSignature);
  SERIALISE_ELEMENT(MaxCommandCount);
  SERIALISE_ELEMENT(pArgumentBuffer);
  SERIALISE_ELEMENT(ArgumentBufferOffset);
  SERIALISE_ELEMENT(pCountBuffer);
  SERIALISE_ELEMENT(CountBufferOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);

        ReplayExecuteIndirect(Unwrap(list));
      }
    }
    else
    {
      WrappedID3D12CommandSignature *comSig = (WrappedID3D12CommandSignature *)pCommandSignature;

      m_Cmd->AddEvent();

      DrawcallDescription draw;
      draw.name = "ExecuteIndirect";

      draw.flags |= DrawFlags::MultiDraw;

      if(MaxCommandCount > 1 || comSig->sig.numDraws > 1)
        draw.flags |= DrawFlags::PushMarker;
      else
        draw.flags |= DrawFlags::SetMarker;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(pArgumentBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));
      drawNode.resourceUsage.push_back(make_rdcpair(
          GetResID(pCountBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Indirect)));

      ID3D12GraphicsCommandList *cracked = GetCrackedList();

      BakedCmdListInfo::ExecuteData exec;
      exec.baseEvent = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].curEventID;
      exec.sig = comSig;
      exec.maxCount = MaxCommandCount;
      exec.countBuf = pCountBuffer;
      exec.countOffs = CountBufferOffset;

      // allocate space for patched indirect buffer
      m_Cmd->GetIndirectBuffer(comSig->sig.ByteStride * MaxCommandCount, &exec.argBuf, &exec.argOffs);

      // transition buffer to COPY_SOURCE/COPY_DEST, copy, and back to INDIRECT_ARG
      D3D12_RESOURCE_BARRIER barriers[2] = {};
      barriers[0].Transition.pResource = Unwrap(pArgumentBuffer);
      barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
      barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
      barriers[1].Transition.pResource = Unwrap(exec.argBuf);
      barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
      barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
      cracked->ResourceBarrier(2, barriers);

      cracked->CopyBufferRegion(Unwrap(exec.argBuf), exec.argOffs, Unwrap(pArgumentBuffer),
                                ArgumentBufferOffset, comSig->sig.ByteStride * MaxCommandCount);

      std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
      std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
      cracked->ResourceBarrier(2, barriers);

      cracked->Close();

      // open new cracked list and re-apply the current state
      {
        D3D12_COMMAND_LIST_TYPE type = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].type;
        UINT nodeMask = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].nodeMask;

        ResourceId allocid = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].allocator;
        ID3D12CommandAllocator *allocator = m_Cmd->m_CrackedAllocators[allocid];

        ID3D12GraphicsCommandList *listptr = NULL;
        m_pDevice->CreateCommandList(nodeMask, type, allocator, NULL,
                                     __uuidof(ID3D12GraphicsCommandList), (void **)&listptr);

        // this is a safe upcast because it's a wrapped object
        ID3D12GraphicsCommandList4 *list = (ID3D12GraphicsCommandList4 *)listptr;

        m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].crackedLists.push_back(list);

        m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].state.ApplyState(m_pDevice, list);
      }

      // perform indirect draw, but from patched buffer. It will be patched between the above list
      // and this list during the first execution of the command list
      Unwrap(pCommandList)
          ->ExecuteIndirect(comSig->GetReal(), MaxCommandCount, Unwrap(exec.argBuf), exec.argOffs,
                            Unwrap(pCountBuffer), CountBufferOffset);
      GetCrackedList()->ExecuteIndirect(comSig->GetReal(), MaxCommandCount, Unwrap(exec.argBuf),
                                        exec.argOffs, Unwrap(pCountBuffer), CountBufferOffset);

      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].executeEvents.push_back(exec);

      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].curEventID++;

      // reserve the right number of drawcalls and events, to later be patched up with the actual
      // details
      ReserveExecuteIndirect(pCommandList, comSig, MaxCommandCount);
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
  SERIALISE_TIME_CALL(m_pList->ExecuteIndirect(Unwrap(pCommandSignature), MaxCommandCount,
                                               Unwrap(pArgumentBuffer), ArgumentBufferOffset,
                                               Unwrap(pCountBuffer), CountBufferOffset));
  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ExecuteIndirect);
    Serialise_ExecuteIndirect(ser, pCommandSignature, MaxCommandCount, pArgumentBuffer,
                              ArgumentBufferOffset, pCountBuffer, CountBufferOffset);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->ContainsExecuteIndirect = true;

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pCommandSignature), eFrameRef_Read);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pArgumentBuffer), eFrameRef_Read);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pCountBuffer), eFrameRef_Read);
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
      DepthStencilView = m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(DSV);
  }
  else
  {
    SERIALISE_ELEMENT(DepthStencilView);
  }
  SERIALISE_ELEMENT(ClearFlags);
  SERIALISE_ELEMENT(Depth);
  SERIALISE_ELEMENT(Stencil);
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil, NumRects,
                                    pRects);
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil, NumRects,
                                  pRects);
      GetCrackedList()->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil,
                                              NumRects, pRects);

      {
        m_Cmd->AddEvent();

        D3D12Descriptor *descriptor = GetWrapped(DepthStencilView);

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("ClearDepthStencilView(%f, %hhu)", Depth, Stencil);
        draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;
        draw.copyDestination = GetResourceManager()->GetOriginalID(descriptor->GetResResourceId());

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(make_rdcpair(
            descriptor->GetResResourceId(), EventUsage(drawNode.draw.eventId, ResourceUsage::Clear)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearDepthStencilView);
    Serialise_ClearDepthStencilView(ser, DepthStencilView, ClearFlags, Depth, Stencil, NumRects,
                                    pRects);

    m_ListRecord->AddChunk(scope.Get());

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
      RenderTargetView = m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(RTV);
  }
  else
  {
    SERIALISE_ELEMENT(RenderTargetView);
  }
  SERIALISE_ELEMENT_ARRAY(ColorRGBA, 4);
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);
      }
    }
    else
    {
      Unwrap(pCommandList)->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);
      GetCrackedList()->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);

      {
        m_Cmd->AddEvent();

        D3D12Descriptor *descriptor = GetWrapped(RenderTargetView);

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("ClearRenderTargetView(%f, %f, %f, %f)", ColorRGBA[0],
                                      ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
        draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;
        draw.copyDestination = GetResourceManager()->GetOriginalID(descriptor->GetResResourceId());

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(make_rdcpair(
            descriptor->GetResResourceId(), EventUsage(drawNode.draw.eventId, ResourceUsage::Clear)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearRenderTargetView);
    Serialise_ClearRenderTargetView(ser, RenderTargetView, ColorRGBA, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

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
      ViewCPUHandle = m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(UAV);
  }
  else
  {
    SERIALISE_ELEMENT(ViewCPUHandle);
  }
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_ARRAY(Values, 4);
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                           Unwrap(pResource), Values, NumRects, pRects);
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                         Unwrap(pResource), Values, NumRects, pRects);
      GetCrackedList()->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap),
                                                     Unwrap(ViewCPUHandle), Unwrap(pResource),
                                                     Values, NumRects, pRects);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("ClearUnorderedAccessViewUint(%u, %u, %u, %u)", Values[0],
                                      Values[1], Values[2], Values[3]);
        draw.flags |= DrawFlags::Clear;
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pResource));

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(pResource), EventUsage(drawNode.draw.eventId, ResourceUsage::Clear)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearUnorderedAccessViewUint);
    Serialise_ClearUnorderedAccessViewUint(ser, ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                           pResource, Values, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

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
      ViewCPUHandle = m_pDevice->GetReplay()->GetDebugManager()->GetTempDescriptor(UAV);
  }
  else
  {
    SERIALISE_ELEMENT(ViewCPUHandle);
  }
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_ARRAY(Values, 4);
  SERIALISE_ELEMENT(NumRects);
  SERIALISE_ELEMENT_ARRAY(pRects, NumRects);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                            Unwrap(pResource), Values, NumRects, pRects);
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                          Unwrap(pResource), Values, NumRects, pRects);
      GetCrackedList()->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap),
                                                      Unwrap(ViewCPUHandle), Unwrap(pResource),
                                                      Values, NumRects, pRects);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("ClearUnorderedAccessViewFloat(%f, %f, %f, %f)", Values[0],
                                      Values[1], Values[2], Values[3]);
        draw.flags |= DrawFlags::Clear;
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pResource));

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        drawNode.resourceUsage.push_back(make_rdcpair(
            GetResID(pResource), EventUsage(drawNode.draw.eventId, ResourceUsage::Clear)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ClearUnorderedAccessViewFloat);
    Serialise_ClearUnorderedAccessViewFloat(ser, ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                            pResource, Values, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

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
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT_OPT(pRegion);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        Unwrap(m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID))
            ->DiscardResource(Unwrap(pResource), pRegion);
      }
    }
    else
    {
      Unwrap(pCommandList)->DiscardResource(Unwrap(pResource), pRegion);
      GetCrackedList()->DiscardResource(Unwrap(pResource), pRegion);
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

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(pDstBuffer);
  SERIALISE_ELEMENT(DstOffset);
  SERIALISE_ELEMENT(pSrcBuffer);
  SERIALISE_ELEMENT(SrcOffset);
  SERIALISE_ELEMENT(NumBytes);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap(list)->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset,
                                       NumBytes);
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset, NumBytes);
      GetCrackedList()->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer),
                                         SrcOffset, NumBytes);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcBuffer));
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstBuffer));

        draw.name = StringFormat::Fmt("CopyBufferRegion(%s, %s)", ToStr(draw.copyDestination).c_str(),
                                      ToStr(draw.copySource).c_str());
        draw.flags |= DrawFlags::Copy;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(pSrcBuffer == pDstBuffer)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstBuffer), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyBufferRegion);
    Serialise_CopyBufferRegion(ser, pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT_LOCAL(dst, *pDst);
  SERIALISE_ELEMENT(DstX);
  SERIALISE_ELEMENT(DstY);
  SERIALISE_ELEMENT(DstZ);
  SERIALISE_ELEMENT_LOCAL(src, *pSrc);
  SERIALISE_ELEMENT_OPT(pSrcBox);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    D3D12_TEXTURE_COPY_LOCATION unwrappedDst = dst;
    unwrappedDst.pResource = Unwrap(unwrappedDst.pResource);
    D3D12_TEXTURE_COPY_LOCATION unwrappedSrc = src;
    unwrappedSrc.pResource = Unwrap(unwrappedSrc.pResource);

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap(list)->CopyTextureRegion(&unwrappedDst, DstX, DstY, DstZ, &unwrappedSrc, pSrcBox);
      }
    }
    else
    {
      Unwrap(pCommandList)->CopyTextureRegion(&unwrappedDst, DstX, DstY, DstZ, &unwrappedSrc, pSrcBox);
      GetCrackedList()->CopyTextureRegion(&unwrappedDst, DstX, DstY, DstZ, &unwrappedSrc, pSrcBox);

      {
        m_Cmd->AddEvent();

        ResourceId liveSrc = GetResID(src.pResource);
        ResourceId liveDst = GetResID(dst.pResource);

        ResourceId origSrc = GetResourceManager()->GetOriginalID(liveSrc);
        ResourceId origDst = GetResourceManager()->GetOriginalID(liveDst);

        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("CopyTextureRegion(%s, %s)", ToStr(origSrc).c_str(),
                                      ToStr(origDst).c_str());
        draw.flags |= DrawFlags::Copy;

        draw.copySource = origSrc;
        draw.copyDestination = origDst;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(origSrc == origDst)
        {
          drawNode.resourceUsage.push_back(
              make_rdcpair(liveSrc, EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(
              make_rdcpair(liveSrc, EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(
              make_rdcpair(liveDst, EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyTextureRegion);
    Serialise_CopyTextureRegion(ser, pDst, DstX, DstY, DstZ, pSrc, pSrcBox);

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(pSrcResource);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap(list)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
      }
    }
    else
    {
      Unwrap(pCommandList)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
      GetCrackedList()->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcResource));
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstResource));

        draw.name = StringFormat::Fmt("CopyResource(%s, %s)", ToStr(draw.copyDestination).c_str(),
                                      ToStr(draw.copySource).c_str());
        draw.flags |= DrawFlags::Copy;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(pSrcResource == pDstResource)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(drawNode.draw.eventId, ResourceUsage::Copy)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(drawNode.draw.eventId, ResourceUsage::CopySrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstResource), EventUsage(drawNode.draw.eventId, ResourceUsage::CopyDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_CopyResource);
    Serialise_CopyResource(ser, pDstResource, pSrcResource);

    m_ListRecord->AddChunk(scope.Get());
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
  SERIALISE_ELEMENT(pDstResource);
  SERIALISE_ELEMENT(DstSubresource);
  SERIALISE_ELEMENT(pSrcResource);
  SERIALISE_ELEMENT(SrcSubresource);
  SERIALISE_ELEMENT(Format);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_Cmd->m_LastCmdListID = GetResourceManager()->GetOriginalID(GetResID(pCommandList));

    if(IsActiveReplaying(m_State))
    {
      if(m_Cmd->InRerecordRange(m_Cmd->m_LastCmdListID))
      {
        ID3D12GraphicsCommandList4 *list = m_Cmd->RerecordCmdList(m_Cmd->m_LastCmdListID);
        Unwrap(list)->ResolveSubresource(Unwrap(pDstResource), DstSubresource, Unwrap(pSrcResource),
                                         SrcSubresource, Format);
      }
    }
    else
    {
      Unwrap(pCommandList)
          ->ResolveSubresource(Unwrap(pDstResource), DstSubresource, Unwrap(pSrcResource),
                               SrcSubresource, Format);
      GetCrackedList()->ResolveSubresource(Unwrap(pDstResource), DstSubresource,
                                           Unwrap(pSrcResource), SrcSubresource, Format);

      {
        m_Cmd->AddEvent();

        DrawcallDescription draw;
        draw.copySource = GetResourceManager()->GetOriginalID(GetResID(pSrcResource));
        draw.copyDestination = GetResourceManager()->GetOriginalID(GetResID(pDstResource));

        draw.name =
            StringFormat::Fmt("ResolveSubresource(%s, %s)", ToStr(draw.copyDestination).c_str(),
                              ToStr(draw.copySource).c_str());
        draw.flags |= DrawFlags::Resolve;

        m_Cmd->AddDrawcall(draw, true);

        D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

        if(pSrcResource == pDstResource)
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(drawNode.draw.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pSrcResource), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveSrc)));
          drawNode.resourceUsage.push_back(make_rdcpair(
              GetResID(pDstResource), EventUsage(drawNode.draw.eventId, ResourceUsage::ResolveDst)));
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_ResolveSubresource);
    Serialise_ResolveSubresource(ser, pDstResource, DstSubresource, pSrcResource, SrcSubresource,
                                 Format);

    m_ListRecord->AddChunk(scope.Get());
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
  D3D12NOTIMP("Tiled Resources");
  return true;
}

void WrappedID3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
  D3D12NOTIMP("Tiled Resources");
  m_pList->CopyTiles(Unwrap(pTiledResource), pTileRegionStartCoordinate, pTileRegionSize,
                     Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);
}

#pragma endregion Copies

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, Close);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, Reset,
                                ID3D12CommandAllocator *pAllocator,
                                ID3D12PipelineState *pInitialState);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12GraphicsCommandList, ResourceBarrier,
                                UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers);
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
