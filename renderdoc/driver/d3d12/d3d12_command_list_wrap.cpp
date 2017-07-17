/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "d3d12_command_queue.h"

ID3D12GraphicsCommandList *WrappedID3D12GraphicsCommandList::GetList(ResourceId id)
{
  return GetResourceManager()->GetLiveAs<WrappedID3D12GraphicsCommandList>(id)->GetReal();
}

ID3D12GraphicsCommandList *WrappedID3D12GraphicsCommandList::GetCrackedList(ResourceId id)
{
  return Unwrap(m_Cmd->m_BakedCmdListInfo[id].crackedLists.back());
}

bool WrappedID3D12GraphicsCommandList::Serialise_Close()
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());

  ResourceId bakedCmdId;

  if(m_State >= WRITING)
  {
    D3D12ResourceRecord *record = GetResourceManager()->GetResourceRecord(CommandList);
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      bakedCmdId = record->bakedCommands->GetResourceID();
  }

  SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
      RDCDEBUG("Ending partial command list for %llu baked to %llu", CommandList, bakeId);
#endif

      list->Close();

      // erase the non-baked reference to this command list so that we don't have
      // duplicates when it comes time to clean up. See below in in Reset()
      m_Cmd->m_RerecordCmds.erase(CommandList);

      if(m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent == CommandList)
        m_Cmd->m_Partial[D3D12CommandData::Primary].partialParent = ResourceId();
    }

    m_Cmd->m_BakedCmdListInfo[CommandList].curEventID = 0;
  }
  else if(m_State == READING)
  {
    ID3D12GraphicsCommandList *list = GetList(CommandList);

    GetResourceManager()->RemoveReplacement(CommandList);

    list->Close();

    if(!m_Cmd->m_BakedCmdListInfo[CommandList].crackedLists.empty())
    {
      GetCrackedList(CommandList)->Close();
    }

    if(!m_Cmd->m_BakedCmdListInfo[CommandList].curEvents.empty())
    {
      DrawcallDescription draw;
      draw.name = "API Calls";
      draw.flags |= DrawFlags::SetMarker | DrawFlags::APICalls;

      m_Cmd->AddDrawcall(draw, true);

      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID++;
    }

    {
      if(m_Cmd->GetDrawcallStack().size() > 1)
        m_Cmd->GetDrawcallStack().pop_back();
    }

    m_Cmd->m_BakedCmdListInfo[bakeId].BakeFrom(CommandList, m_Cmd->m_BakedCmdListInfo[CommandList]);
  }

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Close()
{
  if(m_State >= WRITING)
  {
    {
      SCOPED_SERIALISE_CONTEXT(CLOSE_LIST);
      Serialise_Close();

      m_ListRecord->AddChunk(scope.Get());
    }

    m_ListRecord->Bake();
  }

  return m_pReal->Close();
}

bool WrappedID3D12GraphicsCommandList::Serialise_Reset(ID3D12CommandAllocator *pAllocator,
                                                       ID3D12PipelineState *pInitialState)
{
  // parameters to create the list with if needed
  SERIALISE_ELEMENT(IID, riid, m_Init.riid);
  SERIALISE_ELEMENT(UINT, nodeMask, m_Init.nodeMask);
  SERIALISE_ELEMENT(D3D12_COMMAND_LIST_TYPE, type, m_Init.type);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, Allocator, GetResID(pAllocator));
  SERIALISE_ELEMENT(ResourceId, State, GetResID(pInitialState));

  ResourceId bakedCmdId;

  if(m_State >= WRITING)
  {
    D3D12ResourceRecord *record = GetResourceManager()->GetResourceRecord(CommandList);
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      bakedCmdId = record->bakedCommands->GetResourceID();
  }

  SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    const uint32_t length = m_Cmd->m_BakedCmdListInfo[bakeId].eventCount;

    bool partial = false;
    int partialType = D3D12CommandData::ePartialNum;

    // check for partial execution of this command list
    for(int p = 0; p < D3D12CommandData::ePartialNum; p++)
    {
      const vector<uint32_t> &baseEvents = m_Cmd->m_Partial[p].cmdListExecs[bakeId];

      for(auto it = baseEvents.begin(); it != baseEvents.end(); ++it)
      {
        if(*it <= m_Cmd->m_LastEventID && m_Cmd->m_LastEventID < (*it + length))
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Reset - partial detected %u < %u < %u, %llu -> %llu", *it, m_Cmd->m_LastEventID,
                   *it + length, CommandList, bakeId);
#endif

          m_Cmd->m_Partial[p].partialParent = CommandList;
          m_Cmd->m_Partial[p].baseEvent = *it;

          partial = true;
          partialType = p;
        }
      }
    }

    if(partial || (m_Cmd->m_DrawcallCallback && m_Cmd->m_DrawcallCallback->RecordAllCmds()))
    {
      pAllocator = GetResourceManager()->GetLiveAs<ID3D12CommandAllocator>(Allocator);
      pInitialState =
          State == ResourceId() ? NULL : GetResourceManager()->GetLiveAs<ID3D12PipelineState>(State);

      ID3D12GraphicsCommandList *list = NULL;
      HRESULT hr = m_pDevice->CreateCommandList(nodeMask, type, pAllocator, pInitialState, riid,
                                                (void **)&list);

      if(FAILED(hr))
        RDCERR("Failed on resource serialise-creation, hr: 0x%08x", hr);

      if(partial)
      {
        m_Cmd->m_Partial[partialType].resultPartialCmdList = list;
      }
      else
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
        m_Cmd->m_RerecordCmds[bakeId] = list;
        m_Cmd->m_RerecordCmds[CommandList] = list;
      }

      m_Cmd->m_RenderState.pipe = GetResID(pInitialState);
    }

    m_Cmd->m_BakedCmdListInfo[CommandList].curEventID = 0;
    m_Cmd->m_BakedCmdListInfo[CommandList].executeEvents =
        m_Cmd->m_BakedCmdListInfo[bakeId].executeEvents;
  }
  else if(m_State == READING)
  {
    pAllocator = GetResourceManager()->GetLiveAs<ID3D12CommandAllocator>(Allocator);
    pInitialState =
        State == ResourceId() ? NULL : GetResourceManager()->GetLiveAs<ID3D12PipelineState>(State);

    if(!GetResourceManager()->HasLiveResource(bakeId))
    {
      ID3D12GraphicsCommandList *list = NULL;
      m_pDevice->CreateCommandList(nodeMask, type, pAllocator, pInitialState, riid, (void **)&list);

      GetResourceManager()->AddLiveResource(bakeId, list);

      // whenever a command-building chunk asks for the command list, it
      // will get our baked version.
      GetResourceManager()->ReplaceResource(CommandList, bakeId);
    }
    else
    {
      GetList(CommandList)->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
    }

    {
      D3D12DrawcallTreeNode *draw = new D3D12DrawcallTreeNode;
      m_Cmd->m_BakedCmdListInfo[CommandList].draw = draw;

      {
        if(m_Cmd->m_CrackedAllocators[Allocator] == NULL)
        {
          HRESULT hr =
              m_pDevice->CreateCommandAllocator(type, __uuidof(ID3D12CommandAllocator),
                                                (void **)&m_Cmd->m_CrackedAllocators[Allocator]);
          RDCASSERTEQUAL(hr, S_OK);
        }

        ID3D12GraphicsCommandList *list = NULL;
        m_pDevice->CreateCommandList(nodeMask, type, m_Cmd->m_CrackedAllocators[Allocator],
                                     pInitialState, riid, (void **)&list);

        RDCASSERT(m_Cmd->m_BakedCmdListInfo[CommandList].crackedLists.empty());
        m_Cmd->m_BakedCmdListInfo[CommandList].crackedLists.push_back(list);
      }

      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].type = type;
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].nodeMask = nodeMask;
      m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].allocator = Allocator;

      // On list execute we increment all child events/drawcalls by
      // m_RootEventID and insert them into the tree.
      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID = 0;
      m_Cmd->m_BakedCmdListInfo[CommandList].eventCount = 0;
      m_Cmd->m_BakedCmdListInfo[CommandList].drawCount = 0;

      m_Cmd->m_BakedCmdListInfo[CommandList].drawStack.push_back(draw);

      // reset state
      D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;
      state.m_ResourceManager = GetResourceManager();
      state.pipe = GetResID(pInitialState);
    }
  }

  return true;
}

HRESULT WrappedID3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator,
                                                ID3D12PipelineState *pInitialState)
{
  if(m_State >= WRITING)
  {
    bool firstTime = false;

    // reset for new recording
    m_ListRecord->DeleteChunks();
    m_ListRecord->ContainsExecuteIndirect = false;

    // free parents
    m_ListRecord->FreeParents(GetResourceManager());

    // free any baked commands. If we don't have any, this is the creation reset
    // so we return before actually doing the 'real' reset.
    if(m_ListRecord->bakedCommands)
      m_ListRecord->bakedCommands->Delete(GetResourceManager());
    else
      firstTime = true;

    m_ListRecord->bakedCommands =
        GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_ListRecord->bakedCommands->type = Resource_GraphicsCommandList;
    m_ListRecord->bakedCommands->SpecialResource = true;
    m_ListRecord->bakedCommands->cmdInfo = new CmdListRecordingInfo();

    {
      SCOPED_SERIALISE_CONTEXT(RESET_LIST);
      Serialise_Reset(pAllocator, pInitialState);

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

  return m_pReal->Reset(Unwrap(pAllocator), Unwrap(pInitialState));
}

void WrappedID3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
  m_pReal->ClearState(Unwrap(pPipelineState));
}

bool WrappedID3D12GraphicsCommandList::Serialise_ResourceBarrier(UINT NumBarriers,
                                                                 const D3D12_RESOURCE_BARRIER *pBarriers)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumBarriers);
  SERIALISE_ELEMENT_ARR(D3D12_RESOURCE_BARRIER, barriers, pBarriers, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  vector<D3D12_RESOURCE_BARRIER> filtered;
  if(m_State <= EXECUTING)
  {
    filtered.reserve(num);

    // non-transition barriers allow NULLs, but for transition barriers filter out any that
    // reference the NULL resource - this means the resource wasn't used elsewhere so was discarded
    // from the capture
    for(UINT i = 0; i < num; i++)
      if(barriers[i].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
         barriers[i].Transition.pResource)
        filtered.push_back(barriers[i]);
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      if(!filtered.empty())
        Unwrap(list)->ResourceBarrier((UINT)filtered.size(), &filtered[0]);

      ResourceId cmd = GetResID(list);

      // need to re-wrap the barriers
      for(UINT i = 0; i < num; i++)
      {
        D3D12_RESOURCE_BARRIER b = barriers[i];

        switch(b.Type)
        {
          case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
          {
            b.Transition.pResource =
                (ID3D12Resource *)GetResourceManager()->GetWrapper(b.Transition.pResource);
            break;
          }
          case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
          {
            b.Aliasing.pResourceBefore =
                (ID3D12Resource *)GetResourceManager()->GetWrapper(b.Aliasing.pResourceBefore);
            b.Aliasing.pResourceAfter =
                (ID3D12Resource *)GetResourceManager()->GetWrapper(b.Aliasing.pResourceAfter);
            break;
          }
          case D3D12_RESOURCE_BARRIER_TYPE_UAV:
          {
            b.UAV.pResource = (ID3D12Resource *)GetResourceManager()->GetWrapper(b.UAV.pResource);
            break;
          }
        }

        m_Cmd->m_BakedCmdListInfo[cmd].barriers.push_back(b);
      }
    }
  }
  else if(m_State == READING)
  {
    ID3D12GraphicsCommandList *list = GetList(CommandList);

    if(!filtered.empty())
    {
      list->ResourceBarrier((UINT)filtered.size(), &filtered[0]);
      GetCrackedList(CommandList)->ResourceBarrier((UINT)filtered.size(), &filtered[0]);
    }

    ResourceId cmd = GetResID(GetResourceManager()->GetWrapper(list));

    // need to re-wrap the barriers
    for(UINT i = 0; i < num; i++)
    {
      D3D12_RESOURCE_BARRIER b = barriers[i];

      switch(b.Type)
      {
        case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
        {
          b.Transition.pResource =
              (ID3D12Resource *)GetResourceManager()->GetWrapper(b.Transition.pResource);
          break;
        }
        case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
        {
          b.Aliasing.pResourceBefore =
              (ID3D12Resource *)GetResourceManager()->GetWrapper(b.Aliasing.pResourceBefore);
          b.Aliasing.pResourceAfter =
              (ID3D12Resource *)GetResourceManager()->GetWrapper(b.Aliasing.pResourceAfter);
          break;
        }
        case D3D12_RESOURCE_BARRIER_TYPE_UAV:
        {
          b.UAV.pResource = (ID3D12Resource *)GetResourceManager()->GetWrapper(b.UAV.pResource);
          break;
        }
      }

      m_Cmd->m_BakedCmdListInfo[cmd].barriers.push_back(b);
    }
  }

  SAFE_DELETE_ARRAY(barriers);

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

  m_pReal->ResourceBarrier(NumBarriers, barriers);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(RESOURCE_BARRIER);
    Serialise_ResourceBarrier(NumBarriers, pBarriers);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->cmdInfo->barriers.insert(m_ListRecord->cmdInfo->barriers.end(), pBarriers,
                                           pBarriers + NumBarriers);
  }
}

#pragma region State Setting

bool WrappedID3D12GraphicsCommandList::Serialise_IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(D3D12_PRIMITIVE_TOPOLOGY, topo, PrimitiveTopology);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->IASetPrimitiveTopology(topo);

      m_Cmd->m_RenderState.topo = topo;
    }
  }
  else if(m_State == READING)
  {
    m_Cmd->m_BakedCmdListInfo[CommandList].state.topo = topo;

    GetList(CommandList)->IASetPrimitiveTopology(topo);
    GetCrackedList(CommandList)->IASetPrimitiveTopology(topo);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
  m_pReal->IASetPrimitiveTopology(PrimitiveTopology);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_TOPOLOGY);
    Serialise_IASetPrimitiveTopology(PrimitiveTopology);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_RSSetViewports(UINT NumViewports,
                                                                const D3D12_VIEWPORT *pViewports)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumViewports);
  SERIALISE_ELEMENT_ARR(D3D12_VIEWPORT, views, pViewports, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->RSSetViewports(num, views);

      if(m_Cmd->m_RenderState.views.size() < num)
        m_Cmd->m_RenderState.views.resize(num);

      for(UINT i = 0; i < num; i++)
        m_Cmd->m_RenderState.views[i] = views[i];
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->RSSetViewports(num, views);
    GetCrackedList(CommandList)->RSSetViewports(num, views);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.views.size() < num)
      state.views.resize(num);

    for(UINT i = 0; i < num; i++)
      state.views[i] = views[i];
  }

  SAFE_DELETE_ARRAY(views);

  return true;
}

void WrappedID3D12GraphicsCommandList::RSSetViewports(UINT NumViewports,
                                                      const D3D12_VIEWPORT *pViewports)
{
  m_pReal->RSSetViewports(NumViewports, pViewports);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_VIEWPORTS);
    Serialise_RSSetViewports(NumViewports, pViewports);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_RSSetScissorRects(UINT NumRects,
                                                                   const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->RSSetScissorRects(num, rects);

      if(m_Cmd->m_RenderState.scissors.size() < num)
        m_Cmd->m_RenderState.scissors.resize(num);

      for(UINT i = 0; i < num; i++)
        m_Cmd->m_RenderState.scissors[i] = rects[i];
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->RSSetScissorRects(num, rects);
    GetCrackedList(CommandList)->RSSetScissorRects(num, rects);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.scissors.size() < num)
      state.scissors.resize(num);

    for(UINT i = 0; i < num; i++)
      state.scissors[i] = rects[i];
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->RSSetScissorRects(NumRects, pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_SCISSORS);
    Serialise_RSSetScissorRects(NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_OMSetBlendFactor(const FLOAT BlendFactor[4])
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());

  float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  if(m_State >= WRITING && BlendFactor)
    memcpy(factor, BlendFactor, sizeof(float) * 4);

  m_pSerialiser->SerialisePODArray<4>("BlendFactor", factor);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->OMSetBlendFactor(factor);

      memcpy(m_Cmd->m_RenderState.blendFactor, factor, sizeof(factor));
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->OMSetBlendFactor(factor);
    GetCrackedList(CommandList)->OMSetBlendFactor(factor);

    memcpy(m_Cmd->m_BakedCmdListInfo[CommandList].state.blendFactor, factor, sizeof(factor));
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
  m_pReal->OMSetBlendFactor(BlendFactor);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_BLENDFACTOR);
    Serialise_OMSetBlendFactor(BlendFactor);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_OMSetStencilRef(UINT StencilRef)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, ref, StencilRef);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->OMSetStencilRef(ref);

      m_Cmd->m_RenderState.stencilRef = ref;
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->OMSetStencilRef(ref);
    GetCrackedList(CommandList)->OMSetStencilRef(ref);

    m_Cmd->m_BakedCmdListInfo[CommandList].state.stencilRef = ref;
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
  m_pReal->OMSetStencilRef(StencilRef);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_STENCIL);
    Serialise_OMSetStencilRef(StencilRef);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetDescriptorHeaps(
    UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());

  std::vector<ResourceId> DescriptorHeaps;

  if(m_State >= WRITING)
  {
    DescriptorHeaps.resize(NumDescriptorHeaps);
    for(UINT i = 0; i < NumDescriptorHeaps; i++)
      DescriptorHeaps[i] = GetResID(ppDescriptorHeaps[i]);
  }

  m_pSerialiser->Serialise("DescriptorHeaps", DescriptorHeaps);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      std::vector<ID3D12DescriptorHeap *> heaps;
      heaps.resize(DescriptorHeaps.size());
      for(size_t i = 0; i < heaps.size(); i++)
        heaps[i] = Unwrap(GetResourceManager()->GetLiveAs<ID3D12DescriptorHeap>(DescriptorHeaps[i]));

      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetDescriptorHeaps((UINT)heaps.size(), &heaps[0]);

      m_Cmd->m_RenderState.heaps.resize(heaps.size());
      for(size_t i = 0; i < heaps.size(); i++)
        m_Cmd->m_RenderState.heaps[i] = GetResourceManager()->GetLiveID(DescriptorHeaps[i]);
    }
  }
  else if(m_State == READING)
  {
    std::vector<ID3D12DescriptorHeap *> heaps;
    heaps.resize(DescriptorHeaps.size());
    for(size_t i = 0; i < heaps.size(); i++)
      heaps[i] = Unwrap(GetResourceManager()->GetLiveAs<ID3D12DescriptorHeap>(DescriptorHeaps[i]));

    GetList(CommandList)->SetDescriptorHeaps((UINT)heaps.size(), &heaps[0]);
    GetCrackedList(CommandList)->SetDescriptorHeaps((UINT)heaps.size(), &heaps[0]);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    state.heaps.resize(heaps.size());
    for(size_t i = 0; i < heaps.size(); i++)
      state.heaps[i] = GetResourceManager()->GetLiveID(DescriptorHeaps[i]);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps,
                                                          ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  ID3D12DescriptorHeap **heaps = m_pDevice->GetTempArray<ID3D12DescriptorHeap *>(NumDescriptorHeaps);
  for(UINT i = 0; i < NumDescriptorHeaps; i++)
    heaps[i] = Unwrap(ppDescriptorHeaps[i]);

  m_pReal->SetDescriptorHeaps(NumDescriptorHeaps, heaps);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_DESC_HEAPS);
    Serialise_SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumDescriptorHeaps; i++)
      m_ListRecord->MarkResourceFrameReferenced(GetResID(ppDescriptorHeaps[i]), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(bool, HasView, pView != NULL);
  SERIALISE_ELEMENT_OPT(D3D12_INDEX_BUFFER_VIEW, view, *pView, HasView);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      if(HasView)
      {
        Unwrap(list)->IASetIndexBuffer(&view);

        WrappedID3D12Resource::GetResIDFromAddr(
            view.BufferLocation, m_Cmd->m_RenderState.ibuffer.buf, m_Cmd->m_RenderState.ibuffer.offs);
        m_Cmd->m_RenderState.ibuffer.bytewidth = (view.Format == DXGI_FORMAT_R32_UINT ? 4 : 2);
        m_Cmd->m_RenderState.ibuffer.size = view.SizeInBytes;
      }
      else
      {
        Unwrap(list)->IASetIndexBuffer(NULL);

        m_Cmd->m_RenderState.ibuffer.buf = ResourceId();
        m_Cmd->m_RenderState.ibuffer.offs = 0;
        m_Cmd->m_RenderState.ibuffer.bytewidth = 2;
      }
    }
  }
  else if(m_State == READING)
  {
    ID3D12GraphicsCommandList *list = GetList(CommandList);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(HasView)
    {
      list->IASetIndexBuffer(&view);
      GetCrackedList(CommandList)->IASetIndexBuffer(&view);

      WrappedID3D12Resource::GetResIDFromAddr(view.BufferLocation, state.ibuffer.buf,
                                              state.ibuffer.offs);
      state.ibuffer.bytewidth = (view.Format == DXGI_FORMAT_R32_UINT ? 4 : 2);
      state.ibuffer.size = view.SizeInBytes;
    }
    else
    {
      list->IASetIndexBuffer(NULL);
      GetCrackedList(CommandList)->IASetIndexBuffer(NULL);

      state.ibuffer.buf = ResourceId();
      state.ibuffer.offs = 0;
      state.ibuffer.bytewidth = 2;
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
  m_pReal->IASetIndexBuffer(pView);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_IBUFFER);
    Serialise_IASetIndexBuffer(pView);

    m_ListRecord->AddChunk(scope.Get());
    if(pView)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pView->BufferLocation), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_IASetVertexBuffers(
    UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, start, StartSlot);
  SERIALISE_ELEMENT(UINT, num, NumViews);
  SERIALISE_ELEMENT_ARR(D3D12_VERTEX_BUFFER_VIEW, views, pViews, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->IASetVertexBuffers(start, num, views);

      if(m_Cmd->m_RenderState.vbuffers.size() < start + num)
        m_Cmd->m_RenderState.vbuffers.resize(start + num);

      for(UINT i = 0; i < num; i++)
      {
        WrappedID3D12Resource::GetResIDFromAddr(views[i].BufferLocation,
                                                m_Cmd->m_RenderState.vbuffers[start + i].buf,
                                                m_Cmd->m_RenderState.vbuffers[start + i].offs);

        m_Cmd->m_RenderState.vbuffers[start + i].stride = views[i].StrideInBytes;
        m_Cmd->m_RenderState.vbuffers[start + i].size = views[i].SizeInBytes;
      }
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->IASetVertexBuffers(start, num, views);
    GetCrackedList(CommandList)->IASetVertexBuffers(start, num, views);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.vbuffers.size() < start + num)
      state.vbuffers.resize(start + num);

    for(UINT i = 0; i < num; i++)
    {
      WrappedID3D12Resource::GetResIDFromAddr(
          views[i].BufferLocation, state.vbuffers[start + i].buf, state.vbuffers[start + i].offs);

      state.vbuffers[start + i].stride = views[i].StrideInBytes;
      state.vbuffers[start + i].size = views[i].SizeInBytes;
    }
  }

  SAFE_DELETE_ARRAY(views);

  return true;
}

void WrappedID3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews,
                                                          const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
  m_pReal->IASetVertexBuffers(StartSlot, NumViews, pViews);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_VBUFFERS);
    Serialise_IASetVertexBuffers(StartSlot, NumViews, pViews);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SOSetTargets(
    UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, start, StartSlot);
  SERIALISE_ELEMENT(UINT, num, NumViews);
  SERIALISE_ELEMENT_ARR(D3D12_STREAM_OUTPUT_BUFFER_VIEW, views, pViews, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SOSetTargets(start, num, views);

      if(m_Cmd->m_RenderState.streamouts.size() < start + num)
        m_Cmd->m_RenderState.streamouts.resize(start + num);

      for(UINT i = 0; i < num; i++)
      {
        D3D12RenderState::StreamOut &so = m_Cmd->m_RenderState.streamouts[start + i];

        WrappedID3D12Resource::GetResIDFromAddr(views[i].BufferLocation, so.buf, so.offs);

        WrappedID3D12Resource::GetResIDFromAddr(views[i].BufferFilledSizeLocation, so.countbuf,
                                                so.countoffs);

        so.size = views[i].SizeInBytes;
      }
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SOSetTargets(start, num, views);
    GetCrackedList(CommandList)->SOSetTargets(start, num, views);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.streamouts.size() < start + num)
      state.streamouts.resize(start + num);

    for(UINT i = 0; i < num; i++)
    {
      D3D12RenderState::StreamOut &so = state.streamouts[start + i];

      WrappedID3D12Resource::GetResIDFromAddr(views[i].BufferLocation, so.buf, so.offs);

      WrappedID3D12Resource::GetResIDFromAddr(views[i].BufferFilledSizeLocation, so.countbuf,
                                              so.countoffs);

      so.size = views[i].SizeInBytes;
    }
  }

  SAFE_DELETE_ARRAY(views);

  return true;
}

void WrappedID3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews,
                                                    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  m_pReal->SOSetTargets(StartSlot, NumViews, pViews);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_SOTARGETS);
    Serialise_SOSetTargets(StartSlot, NumViews, pViews);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < NumViews; i++)
      m_ListRecord->MarkResourceFrameReferenced(
          WrappedID3D12Resource::GetResIDFromAddr(pViews[i].BufferLocation), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetPipelineState(ID3D12PipelineState *pPipelineState)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, pipe, GetResID(pPipelineState));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      pPipelineState = GetResourceManager()->GetLiveAs<ID3D12PipelineState>(pipe);
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetPipelineState(Unwrap(pPipelineState));

      m_Cmd->m_RenderState.pipe = GetResID(pPipelineState);
    }
  }
  else if(m_State == READING)
  {
    pPipelineState = GetResourceManager()->GetLiveAs<ID3D12PipelineState>(pipe);
    GetList(CommandList)->SetPipelineState(Unwrap(pPipelineState));
    GetCrackedList(CommandList)->SetPipelineState(Unwrap(pPipelineState));

    m_Cmd->m_BakedCmdListInfo[CommandList].state.pipe = GetResID(pPipelineState);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState *pPipelineState)
{
  m_pReal->SetPipelineState(Unwrap(pPipelineState));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_PIPE);
    Serialise_SetPipelineState(pPipelineState);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pPipelineState), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_OMSetRenderTargets(
    UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, num, NumRenderTargetDescriptors);
  SERIALISE_ELEMENT(bool, singlehandle, RTsSingleHandleToDescriptorRange != FALSE);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  UINT numHandles = singlehandle ? RDCMIN(1U, num) : num;

  std::vector<PortableHandle> rts;

  if(m_State >= WRITING)
  {
    rts.resize(numHandles);
    // indexing pRenderTargetDescriptors with [i] is fine since if single handle is true,
    // i will only ever be 0 (so we do equivalent of *pRenderTargetDescriptors)
    for(UINT i = 0; i < numHandles; i++)
      rts[i] = ToPortableHandle(pRenderTargetDescriptors[i]);
  }

  m_pSerialiser->Serialise("pRenderTargetDescriptors", rts);

  SERIALISE_ELEMENT(PortableHandle, dsv, pDepthStencilDescriptor
                                             ? ToPortableHandle(*pDepthStencilDescriptor)
                                             : PortableHandle(0));

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = CPUHandleFromPortableHandle(GetResourceManager(), dsv);

      D3D12_CPU_DESCRIPTOR_HANDLE *rtHandles = new D3D12_CPU_DESCRIPTOR_HANDLE[numHandles];

      for(UINT i = 0; i < numHandles; i++)
        rtHandles[i] = CPUHandleFromPortableHandle(GetResourceManager(), rts[i]);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->OMSetRenderTargets(num, rtHandles, singlehandle ? TRUE : FALSE,
                               dsv.heap != ResourceId() ? &dsvHandle : NULL);

      m_Cmd->m_RenderState.rts.resize(numHandles);

      for(UINT i = 0; i < numHandles; i++)
        m_Cmd->m_RenderState.rts[i] = rts[i];

      m_Cmd->m_RenderState.rtSingle = singlehandle;

      m_Cmd->m_RenderState.dsv = dsv;

      SAFE_DELETE_ARRAY(rtHandles);
    }
  }
  else if(m_State == READING)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = CPUHandleFromPortableHandle(GetResourceManager(), dsv);

    D3D12_CPU_DESCRIPTOR_HANDLE *rtHandles = new D3D12_CPU_DESCRIPTOR_HANDLE[numHandles];

    for(UINT i = 0; i < numHandles; i++)
      rtHandles[i] = CPUHandleFromPortableHandle(GetResourceManager(), rts[i]);

    GetList(CommandList)
        ->OMSetRenderTargets(num, rtHandles, singlehandle ? TRUE : FALSE,
                             dsv.heap != ResourceId() ? &dsvHandle : NULL);
    GetCrackedList(CommandList)
        ->OMSetRenderTargets(num, rtHandles, singlehandle ? TRUE : FALSE,
                             dsv.heap != ResourceId() ? &dsvHandle : NULL);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    state.rts.resize(numHandles);

    for(UINT i = 0; i < numHandles; i++)
      state.rts[i] = rts[i];

    state.rtSingle = singlehandle;

    state.dsv = dsv;

    SAFE_DELETE_ARRAY(rtHandles);
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

  m_pReal->OMSetRenderTargets(num, unwrapped, RTsSingleHandleToDescriptorRange,
                              pDepthStencilDescriptor ? &dsv : NULL);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_RTVS);
    Serialise_OMSetRenderTargets(num, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange,
                                 pDepthStencilDescriptor);

    m_ListRecord->AddChunk(scope.Get());
    for(UINT i = 0; i < numHandles; i++)
    {
      D3D12Descriptor *desc = GetWrapped(pRenderTargetDescriptors[i]);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Write);
    }

    if(pDepthStencilDescriptor)
    {
      D3D12Descriptor *desc = GetWrapped(*pDepthStencilDescriptor);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Write);
    }
  }
}

#pragma endregion State Setting

#pragma region Compute Root Signatures

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootSignature(
    ID3D12RootSignature *pRootSignature)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, sig, GetResID(pRootSignature));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      pRootSignature = GetResourceManager()->GetLiveAs<ID3D12RootSignature>(sig);
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetComputeRootSignature(Unwrap(pRootSignature));

      // From the docs
      // (https://msdn.microsoft.com/en-us/library/windows/desktop/dn903950(v=vs.85).aspx)
      // "If a root signature is changed on a command list, all previous root signature bindings
      // become stale and all newly expected bindings must be set before Draw/Dispatch; otherwise,
      // the behavior is undefined. If the root signature is redundantly set to the same one
      // currently set, existing root signature bindings do not become stale."
      if(m_Cmd->m_RenderState.compute.rootsig != GetResID(pRootSignature))
        m_Cmd->m_RenderState.compute.sigelems.clear();

      m_Cmd->m_RenderState.compute.rootsig = GetResID(pRootSignature);
    }
  }
  else if(m_State == READING)
  {
    pRootSignature = GetResourceManager()->GetLiveAs<ID3D12RootSignature>(sig);

    GetList(CommandList)->SetComputeRootSignature(Unwrap(pRootSignature));
    GetCrackedList(CommandList)->SetComputeRootSignature(Unwrap(pRootSignature));

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.rootsig != GetResID(pRootSignature))
      state.compute.sigelems.clear();
    state.compute.rootsig = GetResID(pRootSignature);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature *pRootSignature)
{
  m_pReal->SetComputeRootSignature(Unwrap(pRootSignature));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_SIG);
    Serialise_SetComputeRootSignature(pRootSignature);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pRootSignature), eFrameRef_Read);

    // store this so we can look up how many descriptors a given slot references, etc
    m_CurCompRootSig = GetWrapped(pRootSignature);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(PortableHandle, Descriptor, ToPortableHandle(BaseDescriptor));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetComputeRootDescriptorTable(
              idx, GPUHandleFromPortableHandle(GetResourceManager(), Descriptor));

      WrappedID3D12DescriptorHeap *heap =
          GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(Descriptor.heap);

      if(m_Cmd->m_RenderState.compute.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.compute.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.compute.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootTable, GetResID(heap), (UINT64)Descriptor.index);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)
        ->SetComputeRootDescriptorTable(
            idx, GPUHandleFromPortableHandle(GetResourceManager(), Descriptor));
    GetCrackedList(CommandList)
        ->SetComputeRootDescriptorTable(
            idx, GPUHandleFromPortableHandle(GetResourceManager(), Descriptor));

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.sigelems.size() < idx + 1)
      state.compute.sigelems.resize(idx + 1);

    WrappedID3D12DescriptorHeap *heap =
        GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(Descriptor.heap);

    state.compute.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootTable, GetResID(heap), (UINT64)Descriptor.index);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  m_pReal->SetComputeRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_TABLE);
    Serialise_SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(GetWrapped(BaseDescriptor)->nonsamp.heap),
                                              eFrameRef_Read);

    vector<D3D12_DESCRIPTOR_RANGE1> &ranges =
        GetWrapped(m_CurCompRootSig)->sig.params[RootParameterIndex].ranges;

    D3D12Descriptor *base = GetWrapped(BaseDescriptor);

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
        num = base->samp.heap->GetNumDescriptors() - offset;
      }

      if(!RenderDoc::Inst().GetCaptureOptions().RefAllResources)
      {
        std::vector<D3D12Descriptor *> &descs = m_ListRecord->cmdInfo->boundDescs;

        for(UINT d = 0; d < num; d++)
          descs.push_back(rangeStart + d);
      }

      prevTableOffset = offset + num;
    }
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRoot32BitConstant(
    UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(UINT, val, SrcData);
  SERIALISE_ELEMENT(UINT, offs, DestOffsetIn32BitValues);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetComputeRoot32BitConstant(idx, val, offs);

      if(m_Cmd->m_RenderState.compute.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.compute.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.compute.sigelems[idx].SetConstant(offs, val);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetComputeRoot32BitConstant(idx, val, offs);
    GetCrackedList(CommandList)->SetComputeRoot32BitConstant(idx, val, offs);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.sigelems.size() < idx + 1)
      state.compute.sigelems.resize(idx + 1);

    state.compute.sigelems[idx].SetConstant(offs, val);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex,
                                                                   UINT SrcData,
                                                                   UINT DestOffsetIn32BitValues)
{
  m_pReal->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_CONST);
    Serialise_SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRoot32BitConstants(
    UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData,
    UINT DestOffsetIn32BitValues)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(UINT, num, Num32BitValuesToSet);
  SERIALISE_ELEMENT(UINT, offs, DestOffsetIn32BitValues);
  SERIALISE_ELEMENT_ARR(UINT, data, (UINT *)pSrcData, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetComputeRoot32BitConstants(idx, num, data, offs);

      if(m_Cmd->m_RenderState.compute.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.compute.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.compute.sigelems[idx].SetConstants(num, data, offs);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetComputeRoot32BitConstants(idx, num, data, offs);
    GetCrackedList(CommandList)->SetComputeRoot32BitConstants(idx, num, data, offs);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.sigelems.size() < idx + 1)
      state.compute.sigelems.resize(idx + 1);

    state.compute.sigelems[idx].SetConstants(num, data, offs);
  }

  SAFE_DELETE_ARRAY(data);

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex,
                                                                    UINT Num32BitValuesToSet,
                                                                    const void *pSrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  m_pReal->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                        DestOffsetIn32BitValues);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_CONSTS);
    Serialise_SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                           DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ResourceId id;
  UINT64 offs = 0;

  if(m_State >= WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, id);
  SERIALISE_ELEMENT(UINT64, byteOffset, offs);

  if(m_State < WRITING)
  {
    m_Cmd->m_LastCmdListID = CommandList;

    if(ValidateRootGPUVA(buffer))
      return true;
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetComputeRootConstantBufferView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

      if(m_Cmd->m_RenderState.compute.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.compute.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.compute.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootCBV, GetResID(pRes), byteOffset);
    }
  }
  else if(m_State == READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)->SetComputeRootConstantBufferView(idx, pRes->GetGPUVirtualAddress() + byteOffset);
    GetCrackedList(CommandList)
        ->SetComputeRootConstantBufferView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.sigelems.size() < idx + 1)
      state.compute.sigelems.resize(idx + 1);

    state.compute.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootCBV, GetResID(pRes), byteOffset);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_CBV);
    Serialise_SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ResourceId id;
  UINT64 offs = 0;

  if(m_State >= WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, id);
  SERIALISE_ELEMENT(UINT64, byteOffset, offs);

  if(m_State < WRITING)
  {
    m_Cmd->m_LastCmdListID = CommandList;

    if(ValidateRootGPUVA(buffer))
      return true;
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetComputeRootShaderResourceView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

      if(m_Cmd->m_RenderState.compute.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.compute.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.compute.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootSRV, GetResID(pRes), byteOffset);
    }
  }
  else if(m_State == READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)->SetComputeRootShaderResourceView(idx, pRes->GetGPUVirtualAddress() + byteOffset);
    GetCrackedList(CommandList)
        ->SetComputeRootShaderResourceView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.sigelems.size() < idx + 1)
      state.compute.sigelems.resize(idx + 1);

    state.compute.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootSRV, GetResID(pRes), byteOffset);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_SRV);
    Serialise_SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ResourceId id;
  UINT64 offs = 0;

  if(m_State >= WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, id);
  SERIALISE_ELEMENT(UINT64, byteOffset, offs);

  if(m_State < WRITING)
  {
    m_Cmd->m_LastCmdListID = CommandList;

    if(ValidateRootGPUVA(buffer))
      return true;
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetComputeRootUnorderedAccessView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

      if(m_Cmd->m_RenderState.compute.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.compute.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.compute.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(pRes), byteOffset);
    }
  }
  else if(m_State == READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)
        ->SetComputeRootUnorderedAccessView(idx, pRes->GetGPUVirtualAddress() + byteOffset);
    GetCrackedList(CommandList)
        ->SetComputeRootUnorderedAccessView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.compute.sigelems.size() < idx + 1)
      state.compute.sigelems.resize(idx + 1);

    state.compute.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootUAV, GetResID(pRes), byteOffset);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_COMP_ROOT_UAV);
    Serialise_SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

#pragma endregion Compute Root Signatures

#pragma region Graphics Root Signatures

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootSignature(
    ID3D12RootSignature *pRootSignature)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, sig, GetResID(pRootSignature));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      pRootSignature = GetResourceManager()->GetLiveAs<ID3D12RootSignature>(sig);
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetGraphicsRootSignature(Unwrap(pRootSignature));

      // From the docs
      // (https://msdn.microsoft.com/en-us/library/windows/desktop/dn903950(v=vs.85).aspx)
      // "If a root signature is changed on a command list, all previous root signature bindings
      // become stale and all newly expected bindings must be set before Draw/Dispatch; otherwise,
      // the behavior is undefined. If the root signature is redundantly set to the same one
      // currently set, existing root signature bindings do not become stale."
      if(m_Cmd->m_RenderState.graphics.rootsig != GetResID(pRootSignature))
        m_Cmd->m_RenderState.graphics.sigelems.clear();

      m_Cmd->m_RenderState.graphics.rootsig = GetResID(pRootSignature);
    }
  }
  else if(m_State == READING)
  {
    pRootSignature = GetResourceManager()->GetLiveAs<ID3D12RootSignature>(sig);

    GetList(CommandList)->SetGraphicsRootSignature(Unwrap(pRootSignature));
    GetCrackedList(CommandList)->SetGraphicsRootSignature(Unwrap(pRootSignature));

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.rootsig != GetResID(pRootSignature))
      state.graphics.sigelems.clear();
    state.graphics.rootsig = GetResID(pRootSignature);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature)
{
  m_pReal->SetGraphicsRootSignature(Unwrap(pRootSignature));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_SIG);
    Serialise_SetGraphicsRootSignature(pRootSignature);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pRootSignature), eFrameRef_Read);

    // store this so we can look up how many descriptors a given slot references, etc
    m_CurGfxRootSig = GetWrapped(pRootSignature);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(PortableHandle, Descriptor, ToPortableHandle(BaseDescriptor));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetGraphicsRootDescriptorTable(
              idx, GPUHandleFromPortableHandle(GetResourceManager(), Descriptor));

      WrappedID3D12DescriptorHeap *heap =
          GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(Descriptor.heap);

      if(m_Cmd->m_RenderState.graphics.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.graphics.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootTable, GetResID(heap), (UINT64)Descriptor.index);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)
        ->SetGraphicsRootDescriptorTable(
            idx, GPUHandleFromPortableHandle(GetResourceManager(), Descriptor));
    GetCrackedList(CommandList)
        ->SetGraphicsRootDescriptorTable(
            idx, GPUHandleFromPortableHandle(GetResourceManager(), Descriptor));

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.sigelems.size() < idx + 1)
      state.graphics.sigelems.resize(idx + 1);

    WrappedID3D12DescriptorHeap *heap =
        GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(Descriptor.heap);

    state.graphics.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootTable, GetResID(heap), (UINT64)Descriptor.index);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
  m_pReal->SetGraphicsRootDescriptorTable(RootParameterIndex, Unwrap(BaseDescriptor));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_TABLE);
    Serialise_SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(GetWrapped(BaseDescriptor)->nonsamp.heap),
                                              eFrameRef_Read);

    vector<D3D12_DESCRIPTOR_RANGE1> &ranges =
        GetWrapped(m_CurGfxRootSig)->sig.params[RootParameterIndex].ranges;

    D3D12Descriptor *base = GetWrapped(BaseDescriptor);

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
        num = base->samp.heap->GetNumDescriptors() - offset;
      }

      if(!RenderDoc::Inst().GetCaptureOptions().RefAllResources)
      {
        std::vector<D3D12Descriptor *> &descs = m_ListRecord->cmdInfo->boundDescs;

        for(UINT d = 0; d < num; d++)
          descs.push_back(rangeStart + d);
      }

      prevTableOffset = offset + num;
    }
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRoot32BitConstant(
    UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(UINT, val, SrcData);
  SERIALISE_ELEMENT(UINT, offs, DestOffsetIn32BitValues);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetGraphicsRoot32BitConstant(idx, val, offs);

      if(m_Cmd->m_RenderState.graphics.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.graphics.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.graphics.sigelems[idx].SetConstant(offs, val);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetGraphicsRoot32BitConstant(idx, val, offs);
    GetCrackedList(CommandList)->SetGraphicsRoot32BitConstant(idx, val, offs);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.sigelems.size() < idx + 1)
      state.graphics.sigelems.resize(idx + 1);

    state.graphics.sigelems[idx].SetConstant(offs, val);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex,
                                                                    UINT SrcData,
                                                                    UINT DestOffsetIn32BitValues)
{
  m_pReal->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_CONST);
    Serialise_SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRoot32BitConstants(
    UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData,
    UINT DestOffsetIn32BitValues)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(UINT, num, Num32BitValuesToSet);
  SERIALISE_ELEMENT(UINT, offs, DestOffsetIn32BitValues);
  SERIALISE_ELEMENT_ARR(UINT, data, (UINT *)pSrcData, num);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->SetGraphicsRoot32BitConstants(idx, num, data, offs);

      if(m_Cmd->m_RenderState.graphics.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.graphics.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.graphics.sigelems[idx].SetConstants(num, data, offs);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetGraphicsRoot32BitConstants(idx, num, data, offs);
    GetCrackedList(CommandList)->SetGraphicsRoot32BitConstants(idx, num, data, offs);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.sigelems.size() < idx + 1)
      state.graphics.sigelems.resize(idx + 1);

    state.graphics.sigelems[idx].SetConstants(num, data, offs);
  }

  SAFE_DELETE_ARRAY(data);

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex,
                                                                     UINT Num32BitValuesToSet,
                                                                     const void *pSrcData,
                                                                     UINT DestOffsetIn32BitValues)
{
  m_pReal->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                         DestOffsetIn32BitValues);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_CONSTS);
    Serialise_SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData,
                                            DestOffsetIn32BitValues);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ResourceId id;
  UINT64 offs = 0;

  if(m_State >= WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, id);
  SERIALISE_ELEMENT(UINT64, byteOffset, offs);

  if(m_State < WRITING)
  {
    m_Cmd->m_LastCmdListID = CommandList;

    if(ValidateRootGPUVA(buffer))
      return true;
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetGraphicsRootConstantBufferView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

      if(m_Cmd->m_RenderState.graphics.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.graphics.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootCBV, GetResID(pRes), byteOffset);
    }
  }
  else if(m_State == READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)
        ->SetGraphicsRootConstantBufferView(idx, pRes->GetGPUVirtualAddress() + byteOffset);
    GetCrackedList(CommandList)
        ->SetGraphicsRootConstantBufferView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.sigelems.size() < idx + 1)
      state.graphics.sigelems.resize(idx + 1);

    state.graphics.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootCBV, GetResID(pRes), byteOffset);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_CBV);
    Serialise_SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ResourceId id;
  UINT64 offs = 0;

  if(m_State >= WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, id);
  SERIALISE_ELEMENT(UINT64, byteOffset, offs);

  if(m_State < WRITING)
  {
    m_Cmd->m_LastCmdListID = CommandList;

    if(ValidateRootGPUVA(buffer))
      return true;
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetGraphicsRootShaderResourceView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

      if(m_Cmd->m_RenderState.graphics.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.graphics.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootSRV, GetResID(pRes), byteOffset);
    }
  }
  else if(m_State == READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)
        ->SetGraphicsRootShaderResourceView(idx, pRes->GetGPUVirtualAddress() + byteOffset);
    GetCrackedList(CommandList)
        ->SetGraphicsRootShaderResourceView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.sigelems.size() < idx + 1)
      state.graphics.sigelems.resize(idx + 1);

    state.graphics.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootSRV, GetResID(pRes), byteOffset);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_SRV);
    Serialise_SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  ResourceId id;
  UINT64 offs = 0;

  if(m_State >= WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idx, RootParameterIndex);
  SERIALISE_ELEMENT(ResourceId, buffer, id);
  SERIALISE_ELEMENT(UINT64, byteOffset, offs);

  if(m_State < WRITING)
  {
    m_Cmd->m_LastCmdListID = CommandList;

    if(ValidateRootGPUVA(buffer))
      return true;
  }

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->SetGraphicsRootUnorderedAccessView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

      if(m_Cmd->m_RenderState.graphics.sigelems.size() < idx + 1)
        m_Cmd->m_RenderState.graphics.sigelems.resize(idx + 1);

      m_Cmd->m_RenderState.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(eRootUAV, GetResID(pRes), byteOffset);
    }
  }
  else if(m_State == READING)
  {
    WrappedID3D12Resource *pRes = GetResourceManager()->GetLiveAs<WrappedID3D12Resource>(buffer);

    GetList(CommandList)
        ->SetGraphicsRootUnorderedAccessView(idx, pRes->GetGPUVirtualAddress() + byteOffset);
    GetCrackedList(CommandList)
        ->SetGraphicsRootUnorderedAccessView(idx, pRes->GetGPUVirtualAddress() + byteOffset);

    D3D12RenderState &state = m_Cmd->m_BakedCmdListInfo[CommandList].state;

    if(state.graphics.sigelems.size() < idx + 1)
      state.graphics.sigelems.resize(idx + 1);

    state.graphics.sigelems[idx] =
        D3D12RenderState::SignatureElement(eRootUAV, GetResID(pRes), byteOffset);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
  m_pReal->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_GFX_ROOT_UAV);
    Serialise_SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);

    ResourceId id;
    UINT64 offs = 0;
    WrappedID3D12Resource::GetResIDFromAddr(BufferLocation, id, offs);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(id, eFrameRef_Read);
  }
}

#pragma endregion Graphics Root Signatures

#pragma region Queries / Events

bool WrappedID3D12GraphicsCommandList::Serialise_BeginQuery(ID3D12QueryHeap *pQueryHeap,
                                                            D3D12_QUERY_TYPE Type, UINT Index)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, heap, GetResID(pQueryHeap));
  SERIALISE_ELEMENT(D3D12_QUERY_TYPE, type, Type);
  SERIALISE_ELEMENT(UINT, idx, Index);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      pQueryHeap = GetResourceManager()->GetLiveAs<ID3D12QueryHeap>(heap);

      // Unwrap(m_Cmd->RerecordCmdList(CommandList))->BeginQuery(Unwrap(pQueryHeap), type, idx);
    }
  }
  else if(m_State == READING)
  {
    pQueryHeap = GetResourceManager()->GetLiveAs<ID3D12QueryHeap>(heap);

    // GetList(CommandList)->BeginQuery(Unwrap(pQueryHeap), type, idx);
    // GetCrackedList(CommandList)->BeginQuery(Unwrap(pQueryHeap), type, idx);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap *pQueryHeap,
                                                  D3D12_QUERY_TYPE Type, UINT Index)
{
  m_pReal->BeginQuery(Unwrap(pQueryHeap), Type, Index);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_QUERY);
    Serialise_BeginQuery(pQueryHeap, Type, Index);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_EndQuery(ID3D12QueryHeap *pQueryHeap,
                                                          D3D12_QUERY_TYPE Type, UINT Index)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, heap, GetResID(pQueryHeap));
  SERIALISE_ELEMENT(D3D12_QUERY_TYPE, type, Type);
  SERIALISE_ELEMENT(UINT, idx, Index);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      pQueryHeap = GetResourceManager()->GetLiveAs<ID3D12QueryHeap>(heap);

      // Unwrap(m_Cmd->RerecordCmdList(CommandList))->EndQuery(Unwrap(pQueryHeap), type, idx);
    }
  }
  else if(m_State == READING)
  {
    pQueryHeap = GetResourceManager()->GetLiveAs<ID3D12QueryHeap>(heap);

    // GetList(CommandList)->EndQuery(Unwrap(pQueryHeap), type, idx);
    // GetCrackedList(CommandList)->EndQuery(Unwrap(pQueryHeap), type, idx);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                                UINT Index)
{
  m_pReal->EndQuery(Unwrap(pQueryHeap), Type, Index);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(END_QUERY);
    Serialise_EndQuery(pQueryHeap, Type, Index);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ResolveQueryData(
    ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries,
    ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, heap, GetResID(pQueryHeap));
  SERIALISE_ELEMENT(D3D12_QUERY_TYPE, type, Type);
  SERIALISE_ELEMENT(UINT, start, StartIndex);
  SERIALISE_ELEMENT(UINT, num, NumQueries);
  SERIALISE_ELEMENT(ResourceId, buf, GetResID(pDestinationBuffer));
  SERIALISE_ELEMENT(UINT64, offs, AlignedDestinationBufferOffset);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      pQueryHeap = GetResourceManager()->GetLiveAs<ID3D12QueryHeap>(heap);
      pDestinationBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(buf);

      /*
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->ResolveQueryData(Unwrap(pQueryHeap), type, start, num, Unwrap(pDestinationBuffer),
      offs);*/
    }
  }
  else if(m_State == READING)
  {
    pQueryHeap = GetResourceManager()->GetLiveAs<ID3D12QueryHeap>(heap);
    pDestinationBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(buf);

    // GetList(CommandList)->ResolveQueryData(Unwrap(pQueryHeap), type, start, num,
    // Unwrap(pDestinationBuffer), offs);
    // GetCrackedList(CommandList)->ResolveQueryData(Unwrap(pQueryHeap), type, start, num,
    // Unwrap(pDestinationBuffer), offs);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap *pQueryHeap,
                                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                                        UINT NumQueries,
                                                        ID3D12Resource *pDestinationBuffer,
                                                        UINT64 AlignedDestinationBufferOffset)
{
  m_pReal->ResolveQueryData(Unwrap(pQueryHeap), Type, StartIndex, NumQueries,
                            Unwrap(pDestinationBuffer), AlignedDestinationBufferOffset);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(RESOLVE_QUERY);
    Serialise_ResolveQueryData(pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer,
                               AlignedDestinationBufferOffset);

    m_ListRecord->AddChunk(scope.Get());

    m_ListRecord->MarkResourceFrameReferenced(GetResID(pQueryHeap), eFrameRef_Read);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDestinationBuffer), eFrameRef_Write);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetPredication(ID3D12Resource *pBuffer,
                                                                UINT64 AlignedBufferOffset,
                                                                D3D12_PREDICATION_OP Operation)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, buffer, GetResID(pBuffer));
  SERIALISE_ELEMENT(UINT64, offs, AlignedBufferOffset);
  SERIALISE_ELEMENT(D3D12_PREDICATION_OP, op, Operation);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  // don't replay predication at all

  return true;
}

void WrappedID3D12GraphicsCommandList::SetPredication(ID3D12Resource *pBuffer,
                                                      UINT64 AlignedBufferOffset,
                                                      D3D12_PREDICATION_OP Operation)
{
  m_pReal->SetPredication(Unwrap(pBuffer), AlignedBufferOffset, Operation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_PREDICATION);
    Serialise_SetPredication(pBuffer, AlignedBufferOffset, Operation);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pBuffer), eFrameRef_Read);
  }
}

// from PIXEventsCommon.h of winpixeventruntime
enum PIXEventType
{
  ePIXEvent_EndEvent = 0x000,
  ePIXEvent_BeginEvent_VarArgs = 0x001,
  ePIXEvent_BeginEvent_NoArgs = 0x002,
  ePIXEvent_SetMarker_VarArgs = 0x007,
  ePIXEvent_SetMarker_NoArgs = 0x008,

  ePIXEvent_EndEvent_OnContext = 0x010,
  ePIXEvent_BeginEvent_OnContext_VarArgs = 0x011,
  ePIXEvent_BeginEvent_OnContext_NoArgs = 0x012,
  ePIXEvent_SetMarker_OnContext_VarArgs = 0x017,
  ePIXEvent_SetMarker_OnContext_NoArgs = 0x018,
};

static const UINT PIX_EVENT_UNICODE_VERSION = 0;
static const UINT PIX_EVENT_ANSI_VERSION = 1;
static const UINT PIX_EVENT_PIX3BLOB_VERSION = 2;

inline void PIX3DecodeEventInfo(const UINT64 BlobData, UINT64 &Timestamp, PIXEventType &EventType)
{
  static const UINT64 PIXEventsBlockEndMarker = 0x00000000000FFF80;

  static const UINT64 PIXEventsTypeReadMask = 0x00000000000FFC00;
  static const UINT64 PIXEventsTypeWriteMask = 0x00000000000003FF;
  static const UINT64 PIXEventsTypeBitShift = 10;

  static const UINT64 PIXEventsTimestampReadMask = 0xFFFFFFFFFFF00000;
  static const UINT64 PIXEventsTimestampWriteMask = 0x00000FFFFFFFFFFF;
  static const UINT64 PIXEventsTimestampBitShift = 20;

  Timestamp = (BlobData >> PIXEventsTimestampBitShift) & PIXEventsTimestampWriteMask;
  EventType = PIXEventType((BlobData >> PIXEventsTypeBitShift) & PIXEventsTypeWriteMask);
}

inline void PIX3DecodeStringInfo(const UINT64 BlobData, UINT64 &Alignment, UINT64 &CopyChunkSize,
                                 bool &IsANSI, bool &IsShortcut)
{
  static const UINT64 PIXEventsStringAlignmentWriteMask = 0x000000000000000F;
  static const UINT64 PIXEventsStringAlignmentReadMask = 0xF000000000000000;
  static const UINT64 PIXEventsStringAlignmentBitShift = 60;

  static const UINT64 PIXEventsStringCopyChunkSizeWriteMask = 0x000000000000001F;
  static const UINT64 PIXEventsStringCopyChunkSizeReadMask = 0x0F80000000000000;
  static const UINT64 PIXEventsStringCopyChunkSizeBitShift = 55;

  static const UINT64 PIXEventsStringIsANSIWriteMask = 0x0000000000000001;
  static const UINT64 PIXEventsStringIsANSIReadMask = 0x0040000000000000;
  static const UINT64 PIXEventsStringIsANSIBitShift = 54;

  static const UINT64 PIXEventsStringIsShortcutWriteMask = 0x0000000000000001;
  static const UINT64 PIXEventsStringIsShortcutReadMask = 0x0020000000000000;
  static const UINT64 PIXEventsStringIsShortcutBitShift = 53;

  Alignment = (BlobData >> PIXEventsStringAlignmentBitShift) & PIXEventsStringAlignmentWriteMask;
  CopyChunkSize =
      (BlobData >> PIXEventsStringCopyChunkSizeBitShift) & PIXEventsStringCopyChunkSizeWriteMask;
  IsANSI = (BlobData >> PIXEventsStringIsANSIBitShift) & PIXEventsStringIsANSIWriteMask;
  IsShortcut = (BlobData >> PIXEventsStringIsShortcutBitShift) & PIXEventsStringIsShortcutWriteMask;
}

const UINT64 *PIX3DecodeStringParam(const UINT64 *pData, string &DecodedString)
{
  UINT64 alignment;
  UINT64 copyChunkSize;
  bool isANSI;
  bool isShortcut;
  PIX3DecodeStringInfo(*pData, alignment, copyChunkSize, isANSI, isShortcut);
  ++pData;

  UINT totalStringBytes = 0;
  if(isANSI)
  {
    const char *c = (const char *)pData;
    UINT formatStringByteCount = UINT(strlen((const char *)pData));
    DecodedString = string(c, c + formatStringByteCount);
    totalStringBytes = formatStringByteCount + 1;
  }
  else
  {
    const wchar_t *w = (const wchar_t *)pData;
    UINT formatStringByteCount = UINT(wcslen((const wchar_t *)pData));
    DecodedString = StringFormat::Wide2UTF8(std::wstring(w, w + formatStringByteCount));
    totalStringBytes = (formatStringByteCount + 1) * sizeof(wchar_t);
  }

  UINT64 byteChunks = ((totalStringBytes + copyChunkSize - 1) / copyChunkSize) * copyChunkSize;
  UINT64 stringQWordCount = (byteChunks + 7) / 8;
  pData += stringQWordCount;

  return pData;
}

string PIX3SprintfParams(const string &Format, const UINT64 *pData)
{
  string finalString;
  string formatPart;
  size_t lastFind = 0;

  for(size_t found = Format.find_first_of("%"); found != string::npos;)
  {
    finalString += Format.substr(lastFind, found - lastFind);

    size_t endOfFormat = Format.find_first_of("%diufFeEgGxXoscpaAn", found + 1);
    if(endOfFormat == string::npos)
    {
      finalString += "<FORMAT_ERROR>";
      break;
    }

    formatPart = Format.substr(found, (endOfFormat - found) + 1);

    // strings
    if(formatPart.back() == 's')
    {
      string stringParam;
      pData = PIX3DecodeStringParam(pData, stringParam);
      finalString += stringParam;
    }
    // numerical values
    else
    {
      static const UINT MAX_CHARACTERS_FOR_VALUE = 32;
      char formattedValue[MAX_CHARACTERS_FOR_VALUE];
      StringFormat::snprintf(formattedValue, MAX_CHARACTERS_FOR_VALUE, formatPart.c_str(), *pData);
      finalString += formattedValue;
      ++pData;
    }

    lastFind = endOfFormat + 1;
    found = Format.find_first_of("%", lastFind);
  }

  finalString += Format.substr(lastFind);

  return finalString;
}

inline string PIX3DecodeEventString(const UINT64 *pData)
{
  // event header
  UINT64 timestamp;
  PIXEventType eventType;
  PIX3DecodeEventInfo(*pData, timestamp, eventType);
  ++pData;

  if(eventType != ePIXEvent_BeginEvent_NoArgs && eventType != ePIXEvent_BeginEvent_VarArgs)
  {
    RDCERR("Unexpected/unsupported PIX3Event %u type in PIXDecodeMarkerEventString", eventType);
    return "";
  }

  // color
  // UINT64 color = *pData;
  ++pData;

  // format string
  string formatString;
  pData = PIX3DecodeStringParam(pData, formatString);

  if(eventType == ePIXEvent_BeginEvent_NoArgs)
    return formatString;

  // sprintf remaining args
  formatString = PIX3SprintfParams(formatString, pData);
  return formatString;
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  string markerText = "";

  if(m_State >= WRITING && pData && Size)
  {
    if(Metadata == PIX_EVENT_UNICODE_VERSION)
    {
      const wchar_t *w = (const wchar_t *)pData;
      markerText = StringFormat::Wide2UTF8(std::wstring(w, w + Size));
    }
    else if(Metadata == PIX_EVENT_ANSI_VERSION)
    {
      const char *c = (const char *)pData;
      markerText = string(c, c + Size);
    }
    else if(Metadata == PIX_EVENT_PIX3BLOB_VERSION)
    {
      markerText = PIX3DecodeEventString((UINT64 *)pData);
    }
    else
    {
      RDCERR("Unexpected/unsupported Metadata value %u in SetMarker", Metadata);
    }
  }

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  m_pSerialiser->Serialise("MarkerText", markerText);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == READING)
  {
    DrawcallDescription draw;
    draw.name = markerText;
    draw.flags |= DrawFlags::SetMarker;

    m_Cmd->AddDrawcall(draw, false);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  m_pReal->SetMarker(Metadata, pData, Size);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_MARKER);
    Serialise_SetMarker(Metadata, pData, Size);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_BeginEvent(UINT Metadata, const void *pData,
                                                            UINT Size)
{
  string markerText = "";

  if(m_State >= WRITING && pData && Size)
  {
    if(Metadata == PIX_EVENT_UNICODE_VERSION)
    {
      const wchar_t *w = (const wchar_t *)pData;
      markerText = StringFormat::Wide2UTF8(std::wstring(w, w + Size));
    }
    else if(Metadata == PIX_EVENT_ANSI_VERSION)
    {
      const char *c = (const char *)pData;
      markerText = string(c, c + Size);
    }
    else if(Metadata == PIX_EVENT_PIX3BLOB_VERSION)
    {
      markerText = PIX3DecodeEventString((UINT64 *)pData);
    }
    else
    {
      RDCERR("Unexpected/unsupported Metadata value %u in BeginEvent", Metadata);
    }
  }

  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  m_pSerialiser->Serialise("MarkerText", markerText);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == READING)
  {
    DrawcallDescription draw;
    draw.name = markerText;
    draw.flags |= DrawFlags::PushMarker;

    m_Cmd->AddDrawcall(draw, false);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
  m_pReal->BeginEvent(Metadata, pData, Size);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
    Serialise_BeginEvent(Metadata, pData, Size);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_EndEvent()
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == READING && !m_Cmd->m_BakedCmdListInfo[CommandList].curEvents.empty())
  {
    DrawcallDescription draw;
    draw.name = "API Calls";
    draw.flags = DrawFlags::SetMarker | DrawFlags::APICalls;

    m_Cmd->AddDrawcall(draw, true);
  }

  if(m_State == READING)
  {
    // dummy draw that is consumed when this command buffer
    // is being in-lined into the call stream
    DrawcallDescription draw;
    draw.name = "Pop()";
    draw.flags = DrawFlags::PopMarker;

    m_Cmd->AddDrawcall(draw, false);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::EndEvent()
{
  m_pReal->EndEvent();

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(END_EVENT);
    Serialise_EndEvent();

    m_ListRecord->AddChunk(scope.Get());
  }
}

#pragma endregion Queries / Events

#pragma region Draws

bool WrappedID3D12GraphicsCommandList::Serialise_DrawInstanced(UINT VertexCountPerInstance,
                                                               UINT InstanceCount,
                                                               UINT StartVertexLocation,
                                                               UINT StartInstanceLocation)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, vtxCount, VertexCountPerInstance);
  SERIALISE_ELEMENT(UINT, instCount, InstanceCount);
  SERIALISE_ELEMENT(UINT, startVtx, StartVertexLocation);
  SERIALISE_ELEMENT(UINT, startInst, StartInstanceLocation);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      uint32_t eventID = m_Cmd->HandlePreCallback(list);

      Unwrap(list)->DrawInstanced(vtxCount, instCount, startVtx, startInst);

      if(eventID && m_Cmd->m_DrawcallCallback->PostDraw(eventID, list))
      {
        Unwrap(list)->DrawInstanced(vtxCount, instCount, startVtx, startInst);
        m_Cmd->m_DrawcallCallback->PostRedraw(eventID, list);
      }
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->DrawInstanced(vtxCount, instCount, startVtx, startInst);
    GetCrackedList(CommandList)->DrawInstanced(vtxCount, instCount, startVtx, startInst);

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(desc);
    string name = "DrawInstanced(" + ToStr::Get(vtxCount) + ", " + ToStr::Get(instCount) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = vtxCount;
    draw.numInstances = instCount;
    draw.indexOffset = 0;
    draw.vertexOffset = startVtx;
    draw.instanceOffset = startInst;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced;

    m_Cmd->AddDrawcall(draw, true);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance,
                                                     UINT InstanceCount, UINT StartVertexLocation,
                                                     UINT StartInstanceLocation)
{
  m_pReal->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                         StartInstanceLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_INST);
    Serialise_DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
                            StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_DrawIndexedInstanced(UINT IndexCountPerInstance,
                                                                      UINT InstanceCount,
                                                                      UINT StartIndexLocation,
                                                                      INT BaseVertexLocation,
                                                                      UINT StartInstanceLocation)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, idxCount, IndexCountPerInstance);
  SERIALISE_ELEMENT(UINT, instCount, InstanceCount);
  SERIALISE_ELEMENT(UINT, startIdx, StartIndexLocation);
  SERIALISE_ELEMENT(INT, startVtx, BaseVertexLocation);
  SERIALISE_ELEMENT(UINT, startInst, StartInstanceLocation);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      uint32_t eventID = m_Cmd->HandlePreCallback(list);

      Unwrap(list)->DrawIndexedInstanced(idxCount, instCount, startIdx, startVtx, startInst);

      if(eventID && m_Cmd->m_DrawcallCallback->PostDraw(eventID, list))
      {
        Unwrap(list)->DrawIndexedInstanced(idxCount, instCount, startIdx, startVtx, startInst);
        m_Cmd->m_DrawcallCallback->PostRedraw(eventID, list);
      }
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->DrawIndexedInstanced(idxCount, instCount, startIdx, startVtx, startInst);
    GetCrackedList(CommandList)->DrawIndexedInstanced(idxCount, instCount, startIdx, startVtx, startInst);

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(desc);
    string name =
        "DrawIndexedInstanced(" + ToStr::Get(idxCount) + ", " + ToStr::Get(instCount) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.numIndices = idxCount;
    draw.numInstances = instCount;
    draw.indexOffset = startIdx;
    draw.baseVertex = startVtx;
    draw.instanceOffset = startInst;

    draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer;

    m_Cmd->AddDrawcall(draw, true);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance,
                                                            UINT InstanceCount,
                                                            UINT StartIndexLocation,
                                                            INT BaseVertexLocation,
                                                            UINT StartInstanceLocation)
{
  m_pReal->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                BaseVertexLocation, StartInstanceLocation);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INST);
    Serialise_DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
                                   BaseVertexLocation, StartInstanceLocation);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_Dispatch(UINT ThreadGroupCountX,
                                                          UINT ThreadGroupCountY,
                                                          UINT ThreadGroupCountZ)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(UINT, x, ThreadGroupCountX);
  SERIALISE_ELEMENT(UINT, y, ThreadGroupCountY);
  SERIALISE_ELEMENT(UINT, z, ThreadGroupCountZ);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      uint32_t eventID = m_Cmd->HandlePreCallback(list, true);

      Unwrap(list)->Dispatch(x, y, z);

      if(eventID && m_Cmd->m_DrawcallCallback->PostDraw(eventID, list))
      {
        Unwrap(list)->Dispatch(x, y, z);
        m_Cmd->m_DrawcallCallback->PostRedraw(eventID, list);
      }
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->Dispatch(x, y, z);
    GetCrackedList(CommandList)->Dispatch(x, y, z);

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(desc);
    string name = "Dispatch(" + ToStr::Get(x) + ", " + ToStr::Get(y) + ", " + ToStr::Get(z) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.dispatchDimension[0] = x;
    draw.dispatchDimension[1] = y;
    draw.dispatchDimension[2] = z;

    draw.flags |= DrawFlags::Dispatch;

    m_Cmd->AddDrawcall(draw, true);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                                UINT ThreadGroupCountZ)
{
  m_pReal->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(DISPATCH);
    Serialise_Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    m_ListRecord->AddChunk(scope.Get());
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());

  ResourceId Bundle;

  if(m_State >= WRITING)
  {
    D3D12ResourceRecord *record = GetRecord(pCommandList);
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      Bundle = record->bakedCommands->GetResourceID();
  }

  m_pSerialiser->Serialise("Bundle", Bundle);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      uint32_t eventID = m_Cmd->HandlePreCallback(list, true);

      ID3D12GraphicsCommandList *bundle =
          GetResourceManager()->GetLiveAs<ID3D12GraphicsCommandList>(Bundle);

      Unwrap(list)->ExecuteBundle(Unwrap(bundle));

      if(eventID && m_Cmd->m_DrawcallCallback->PostDraw(eventID, list))
      {
        Unwrap(list)->ExecuteBundle(Unwrap(bundle));
        m_Cmd->m_DrawcallCallback->PostRedraw(eventID, list);
      }
    }
  }
  else if(m_State == READING)
  {
    ID3D12GraphicsCommandList *bundle =
        GetResourceManager()->GetLiveAs<ID3D12GraphicsCommandList>(Bundle);

    GetList(CommandList)->ExecuteBundle(Unwrap(bundle));
    GetCrackedList(CommandList)->ExecuteBundle(Unwrap(bundle));

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(desc);
    string name = "ExecuteBundle(" + ToStr::Get(Bundle) + ")";

    DrawcallDescription draw;
    draw.name = name;

    draw.flags |= DrawFlags::CmdList;

    m_Cmd->AddDrawcall(draw, true);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
  m_pReal->ExecuteBundle(Unwrap(pCommandList));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(EXEC_BUNDLE);
    Serialise_ExecuteBundle(pCommandList);

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
 * ExecuteIndirect needs special handling - whenever we encounter an ExecuteIndirect during READING
 * time we crack the list into two, and copy off the argument buffer in the first part and execute
 * with the copy destination in the second part.
 *
 * Then when we come to ExecuteCommandLists this list, we go step by step through the cracked lists,
 * executing the first, then syncing to the GPU and patching the argument buffer before continuing.
 *
 * At READING time we reserve a maxCount number of drawcalls and events, and later on when patching
 * the argument buffer we fill in the parameters/names and remove any excess draws that weren't
 * actually executed.
 *
 * During EXECUTING we read the patched argument buffer and execute any commands needed by hand on
 * the CPU.
 */

void WrappedID3D12GraphicsCommandList::ReserveExecuteIndirect(ID3D12GraphicsCommandList *list,
                                                              ResourceId sig, UINT maxCount)
{
  WrappedID3D12CommandSignature *comSig =
      GetResourceManager()->GetLiveAs<WrappedID3D12CommandSignature>(sig);

  const bool multidraw = (maxCount > 1 || comSig->sig.numDraws > 1);
  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();

  RDCASSERT(m_State == READING);

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
          m_Cmd->AddEvent("");
          m_Cmd->AddDrawcall(DrawcallDescription(), true);
          cmdInfo.curEventID++;
          break;
        case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
        case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
          // add dummy event
          m_Cmd->AddEvent("");
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

  WrappedID3D12CommandSignature *comSig =
      GetResourceManager()->GetLiveAs<WrappedID3D12CommandSignature>(exec.sig);

  uint32_t count = exec.maxCount;

  if(exec.countBuf)
  {
    vector<byte> data;
    m_pDevice->GetDebugManager()->GetBufferData(exec.countBuf, exec.countOffs, 4, data);
    count = RDCMIN(count, *(uint32_t *)&data[0]);
  }

  exec.realCount = count;

  const bool multidraw = (count > 1 || comSig->sig.numDraws > 1);
  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();
  const bool gfx = comSig->sig.graphics;
  const char *sigTypeString = gfx ? "Graphics" : "Compute";

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
    if(draws[idx].draw.eventID == eid)
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

  for(uint32_t i = 0; i < count; i++)
  {
    byte *data = mapPtr + exec.argOffs;
    mapPtr += comSig->sig.ByteStride;

    for(uint32_t a = 0; a < sigSize; a++)
    {
      const D3D12_INDIRECT_ARGUMENT_DESC &arg = comSig->sig.arguments[a];

      switch(arg.Type)
      {
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
        {
          D3D12_DRAW_ARGUMENTS *args = (D3D12_DRAW_ARGUMENTS *)data;
          data += sizeof(D3D12_DRAW_ARGUMENTS);

          DrawcallDescription &draw = draws[idx].draw;
          draw.numIndices = args->VertexCountPerInstance;
          draw.numInstances = args->InstanceCount;
          draw.vertexOffset = args->StartVertexLocation;
          draw.instanceOffset = args->StartInstanceLocation;
          draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::Indirect;
          draw.name = StringFormat::Fmt("[%u] arg%u: IndirectDraw(<%u, %u>)", i, a, draw.numIndices,
                                        draw.numInstances);

          // if this is the first draw of the indirect, we could have picked up previous
          // non-indirect events in this drawcall, so the EID will be higher than we expect. Just
          // assign the draw's EID
          eid = draw.eventID;

          string eventStr = draw.name;

          // a bit of a hack, but manually construct serialised event data for this command
          eventStr += "\n{\n";
          eventStr +=
              StringFormat::Fmt("\tVertexCountPerInstance: %u\n", args->VertexCountPerInstance);
          eventStr += StringFormat::Fmt("\tInstanceCount: %u\n", args->InstanceCount);
          eventStr += StringFormat::Fmt("\tStartVertexLocation: %u\n", args->StartVertexLocation);
          eventStr += StringFormat::Fmt("\tStartInstanceLocation: %u\n", args->StartInstanceLocation);
          eventStr += "}\n";

          APIEvent &ev = draw.events[draw.events.count - 1];
          ev.eventDesc = eventStr;

          RDCASSERT(ev.eventID == eid);

          m_Cmd->AddUsage(draws[idx]);

          // advance
          idx++;
          eid++;

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
        {
          D3D12_DRAW_INDEXED_ARGUMENTS *args = (D3D12_DRAW_INDEXED_ARGUMENTS *)data;
          data += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

          DrawcallDescription &draw = draws[idx].draw;
          draw.numIndices = args->IndexCountPerInstance;
          draw.numInstances = args->InstanceCount;
          draw.baseVertex = args->BaseVertexLocation;
          draw.vertexOffset = args->StartIndexLocation;
          draw.instanceOffset = args->StartInstanceLocation;
          draw.flags |= DrawFlags::Drawcall | DrawFlags::Instanced | DrawFlags::UseIBuffer |
                        DrawFlags::Indirect;
          draw.name = StringFormat::Fmt("[%u] arg%u: IndirectDrawIndexed(<%u, %u>)", i, a,
                                        draw.numIndices, draw.numInstances);

          // if this is the first draw of the indirect, we could have picked up previous
          // non-indirect events in this drawcall, so the EID will be higher than we expect. Just
          // assign the draw's EID
          eid = draw.eventID;

          string eventStr = draw.name;

          // a bit of a hack, but manually construct serialised event data for this command
          eventStr += "\n{\n";
          eventStr += StringFormat::Fmt("\tIndexCountPerInstance: %u\n", args->IndexCountPerInstance);
          eventStr += StringFormat::Fmt("\tInstanceCount: %u\n", args->InstanceCount);
          eventStr += StringFormat::Fmt("\tBaseVertexLocation: %u\n", args->BaseVertexLocation);
          eventStr += StringFormat::Fmt("\tStartIndexLocation: %u\n", args->StartIndexLocation);
          eventStr += StringFormat::Fmt("\tStartInstanceLocation: %u\n", args->StartInstanceLocation);
          eventStr += "}\n";

          APIEvent &ev = draw.events[draw.events.count - 1];
          ev.eventDesc = eventStr;

          RDCASSERT(ev.eventID == eid);

          m_Cmd->AddUsage(draws[idx]);

          // advance
          idx++;
          eid++;

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
        {
          D3D12_DISPATCH_ARGUMENTS *args = (D3D12_DISPATCH_ARGUMENTS *)data;
          data += sizeof(D3D12_DISPATCH_ARGUMENTS);

          DrawcallDescription &draw = draws[idx].draw;
          draw.dispatchDimension[0] = args->ThreadGroupCountX;
          draw.dispatchDimension[1] = args->ThreadGroupCountY;
          draw.dispatchDimension[2] = args->ThreadGroupCountZ;
          draw.flags |= DrawFlags::Dispatch | DrawFlags::Indirect;
          draw.name = StringFormat::Fmt("[%u] arg%u: IndirectDispatch(<%u, %u, %u>)", i, a,
                                        draw.dispatchDimension[0], draw.dispatchDimension[1],
                                        draw.dispatchDimension[2]);

          // if this is the first draw of the indirect, we could have picked up previous
          // non-indirect events in this drawcall, so the EID will be higher than we expect. Just
          // assign the draw's EID
          eid = draw.eventID;

          string eventStr = draw.name;

          // a bit of a hack, but manually construct serialised event data for this command
          eventStr += "\n{\n";
          eventStr += StringFormat::Fmt("\tThreadGroupCountX: %u\n", args->ThreadGroupCountX);
          eventStr += StringFormat::Fmt("\tThreadGroupCountY: %u\n", args->ThreadGroupCountY);
          eventStr += StringFormat::Fmt("\tThreadGroupCountZ: %u\n", args->ThreadGroupCountZ);
          eventStr += "}\n";

          APIEvent &ev = draw.events[draw.events.count - 1];
          ev.eventDesc = eventStr;

          RDCASSERT(ev.eventID == eid);

          m_Cmd->AddUsage(draws[idx]);

          // advance
          idx++;
          eid++;

          break;
        }
        case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
        {
          size_t argSize = sizeof(uint32_t) * arg.Constant.Num32BitValuesToSet;
          uint32_t *values = (uint32_t *)data;
          data += argSize;

          string name = StringFormat::Fmt("[%u] arg%u: IndirectSet%sRoot32bitConstants()\n", i, a,
                                          sigTypeString);

          // a bit of a hack, but manually construct serialised event data for this command
          name += "{\n";
          name += StringFormat::Fmt("\tConstant.RootParameterIndex: %u\n",
                                    arg.Constant.RootParameterIndex);
          name += StringFormat::Fmt("\tConstant.DestOffsetIn32BitValues: %u\n",
                                    arg.Constant.DestOffsetIn32BitValues);
          name += StringFormat::Fmt("\tConstant.Num32BitValuesToSet: %u\n",
                                    arg.Constant.Num32BitValuesToSet);
          for(uint32_t val = 0; val < arg.Constant.Num32BitValuesToSet; val++)
            name += StringFormat::Fmt("\tvalues[%u]: %u\n", val, values[val]);
          name += "}\n";

          DrawcallDescription &draw = draws[idx].draw;
          APIEvent *ev = NULL;

          for(int32_t e = 0; e < draw.events.count; e++)
          {
            if(draw.events[e].eventID == eid)
            {
              ev = &draw.events[e];
              break;
            }
          }

          if(ev)
            ev->eventDesc = name;

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

          string name = StringFormat::Fmt("[%u] arg%u: IndirectIASetVertexBuffers()\n", i, a);

          // a bit of a hack, but manually construct serialised event data for this command
          name += "{\n";
          name += StringFormat::Fmt("\tVertexBuffer.Slot: %u\n", arg.VertexBuffer.Slot);
          name += StringFormat::Fmt("\tView.BufferLocation: %llu\n", id);
          name += StringFormat::Fmt("\tView.BufferLocation: %llu\n", offs);
          name += StringFormat::Fmt("\tView.SizeInBytes: %u\n", vb->SizeInBytes);
          name += StringFormat::Fmt("\tView.StrideInBytes: %u\n", vb->StrideInBytes);
          name += "}\n";

          DrawcallDescription &draw = draws[idx].draw;
          APIEvent *ev = NULL;

          for(int32_t e = 0; e < draw.events.count; e++)
          {
            if(draw.events[e].eventID == eid)
            {
              ev = &draw.events[e];
              break;
            }
          }

          if(ev)
            ev->eventDesc = name;

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

          string name = StringFormat::Fmt("[%u] arg%u: IndirectIASetIndexBuffer()\n", i, a);

          // a bit of a hack, but manually construct serialised event data for this command
          name += "{\n";
          name += StringFormat::Fmt("\tView.BufferLocation: %llu\n", id);
          name += StringFormat::Fmt("\tView.BufferLocation: %llu\n", offs);
          name += StringFormat::Fmt("\tView.SizeInBytes: %u\n", ib->SizeInBytes);
          name += StringFormat::Fmt("\tView.Format: %s\n", ToStr::Get(ib->Format).c_str());
          name += "}\n";

          DrawcallDescription &draw = draws[idx].draw;
          APIEvent *ev = NULL;

          for(int32_t e = 0; e < draw.events.count; e++)
          {
            if(draw.events[e].eventID == eid)
            {
              ev = &draw.events[e];
              break;
            }
          }

          if(ev)
            ev->eventDesc = name;

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
          RDCASSERT(res);
          if(res)
            *addr = res->GetGPUVirtualAddress() + offs;

          const uint32_t rootIdx = arg.Constant.RootParameterIndex;
          const char *argTypeString = "Unknown";

          if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
            argTypeString = "ConstantBuffer";
          else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
            argTypeString = "ShaderResource";
          else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
            argTypeString = "UnorderedAccess";
          else
            RDCERR("Unexpected argument type! %d", arg.Type);

          string name = StringFormat::Fmt("[%u] arg%u: IndirectSet%sRoot%sView()\n", i, a,
                                          sigTypeString, argTypeString);

          // a bit of a hack, but manually construct serialised event data for this command
          name += "{\n";
          name += StringFormat::Fmt("\tView.RootParameterIndex: %u\n", rootIdx);
          name += StringFormat::Fmt("\tBufferLocation: %llu\n", id);
          name += StringFormat::Fmt("\tBufferLocation_Offset: %llu\n", offs);
          name += "}\n";

          DrawcallDescription &draw = draws[idx].draw;
          APIEvent *ev = NULL;

          for(int32_t e = 0; e < draw.events.count; e++)
          {
            if(draw.events[e].eventID == eid)
            {
              ev = &draw.events[e];
              break;
            }
          }

          if(ev)
            ev->eventDesc = name;

          // advance only the EID, since we're still in the same draw
          eid++;

          break;
        }
        default: RDCERR("Unexpected argument type! %d", arg.Type); break;
      }
    }
  }

  exec.argBuf->Unmap(0, &range);

  // remove excesss draws if count < maxCount
  if(count < exec.maxCount)
  {
    uint32_t shiftEID = (exec.maxCount - count) * sigSize;
    uint32_t lastEID = exec.baseEvent + 1 + sigSize * exec.maxCount;

    uint32_t shiftDrawID = 0;

    while(idx < draws.size() && draws[idx].draw.eventID < lastEID)
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

void WrappedID3D12GraphicsCommandList::ReplayExecuteIndirect(ID3D12GraphicsCommandList *list,
                                                             BakedCmdListInfo &info)
{
  BakedCmdListInfo &cmdInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

  size_t executeIndex = info.executeEvents.size();

  for(size_t i = 0; i < info.executeEvents.size(); i++)
  {
    if(info.executeEvents[i].baseEvent <= cmdInfo.curEventID &&
       cmdInfo.curEventID < info.executeEvents[i].lastEvent)
    {
      executeIndex = i;
      break;
    }
  }

  if(executeIndex >= info.executeEvents.size())
  {
    RDCERR("Couldn't find ExecuteIndirect to replay!");
    return;
  }

  BakedCmdListInfo::ExecuteData &exec = info.executeEvents[executeIndex];

  WrappedID3D12CommandSignature *comSig =
      GetResourceManager()->GetLiveAs<WrappedID3D12CommandSignature>(exec.sig);

  uint32_t count = exec.realCount;
  uint32_t origCount = exec.realCount;

  const bool multidraw = (count > 1 || comSig->sig.numDraws > 1);

  vector<byte> data;
  m_pDevice->GetDebugManager()->GetBufferData(exec.argBuf, exec.argOffs,
                                              count * comSig->sig.ByteStride, data);

  byte *dataPtr = &data[0];

  const bool gfx = comSig->sig.graphics;
  const uint32_t sigSize = (uint32_t)comSig->sig.arguments.size();

  vector<D3D12RenderState::SignatureElement> &sigelems =
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

    RDCASSERT(it != m_Cmd->m_DrawcallUses.end());

    uint32_t baseEventID = it->eventID;

    // TODO when re-recording all, we should submit every drawcall individually
    if(m_Cmd->m_DrawcallCallback && m_Cmd->m_DrawcallCallback->RecordAllCmds())
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
        // note we'll never be asked to do e.g. 3rd-7th commands of an execute. Only ever 0th-nth or
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
          WrappedID3D12Resource::GetResIDFromAddr(srcVB->BufferLocation, id, offs);
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
          WrappedID3D12Resource::GetResIDFromAddr(srcIB->BufferLocation, id, offs);
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
          WrappedID3D12Resource::GetResIDFromAddr(*srcAddr, id, offs);
          RDCASSERT(id != ResourceId());

          const uint32_t rootIdx = arg.Constant.RootParameterIndex;

          SignatureElementType elemType = eRootUnknown;

          if(gfx)
          {
            if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
            {
              elemType = eRootCBV;

              if(executing && id != ResourceId())
                list->SetGraphicsRootConstantBufferView(rootIdx, *srcAddr);
            }
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
            {
              elemType = eRootSRV;

              if(executing && id != ResourceId())
                list->SetGraphicsRootShaderResourceView(rootIdx, *srcAddr);
            }
            else if(arg.Type == D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
            {
              elemType = eRootUAV;

              if(executing && id != ResourceId())
                list->SetGraphicsRootUnorderedAccessView(rootIdx, *srcAddr);
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

bool WrappedID3D12GraphicsCommandList::Serialise_ExecuteIndirect(
    ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer,
    UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, sig, GetResID(pCommandSignature));
  SERIALISE_ELEMENT(UINT, maxCount, MaxCommandCount);
  SERIALISE_ELEMENT(ResourceId, arg, GetResID(pArgumentBuffer));
  SERIALISE_ELEMENT(UINT64, argOffs, ArgumentBufferOffset);
  SERIALISE_ELEMENT(ResourceId, countbuf, GetResID(pCountBuffer));
  SERIALISE_ELEMENT(UINT64, countOffs, CountBufferOffset);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      BakedCmdListInfo &bakeInfo = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID];

      ReplayExecuteIndirect(Unwrap(list), bakeInfo);
    }
  }
  else if(m_State == READING)
  {
    const string desc = m_pSerialiser->GetDebugStr();

    WrappedID3D12CommandSignature *comSig =
        GetResourceManager()->GetLiveAs<WrappedID3D12CommandSignature>(sig);

    ID3D12Resource *argBuf = GetResourceManager()->GetLiveAs<ID3D12Resource>(arg);
    ID3D12Resource *countBuf = GetResourceManager()->GetLiveAs<ID3D12Resource>(countbuf);

    m_Cmd->AddEvent(desc);

    DrawcallDescription draw;
    draw.name = "ExecuteIndirect";

    draw.flags |= DrawFlags::MultiDraw;

    if(maxCount > 1 || comSig->sig.numDraws > 1)
      draw.flags |= DrawFlags::PushMarker;
    else
      draw.flags |= DrawFlags::SetMarker;

    // this drawcall needs an event to anchor its file offset. This is a bit of a hack,
    // but a proper solution for handling 'fake' events that don't correspond to actual
    // events in the file, or duplicates, is overkill.
    create_array(draw.events, 1);
    draw.events[0] = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].curEvents.back();
    m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].curEvents.pop_back();

    m_Cmd->AddDrawcall(draw, false);

    D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

    drawNode.resourceUsage.push_back(
        std::make_pair(GetResourceManager()->GetLiveID(arg),
                       EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));
    drawNode.resourceUsage.push_back(
        std::make_pair(GetResourceManager()->GetLiveID(countbuf),
                       EventUsage(drawNode.draw.eventID, ResourceUsage::Indirect)));

    ID3D12GraphicsCommandList *cracked = GetCrackedList(CommandList);

    BakedCmdListInfo::ExecuteData exec;
    exec.baseEvent = m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].curEventID;
    exec.sig = sig;
    exec.maxCount = maxCount;
    exec.countBuf = countBuf;
    exec.countOffs = countOffs;

    // allocate space for patched indirect buffer
    m_Cmd->GetIndirectBuffer(comSig->sig.ByteStride * maxCount, &exec.argBuf, &exec.argOffs);

    // transition buffer to COPY_SOURCE/COPY_DEST, copy, and back to INDIRECT_ARG
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Transition.pResource = Unwrap(argBuf);
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[1].Transition.pResource = Unwrap(exec.argBuf);
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    cracked->ResourceBarrier(2, barriers);

    cracked->CopyBufferRegion(Unwrap(exec.argBuf), exec.argOffs, Unwrap(argBuf), argOffs,
                              comSig->sig.ByteStride * maxCount);

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

      ID3D12GraphicsCommandList *list = NULL;
      m_pDevice->CreateCommandList(nodeMask, type, allocator, NULL,
                                   __uuidof(ID3D12GraphicsCommandList), (void **)&list);

      m_Cmd->m_BakedCmdListInfo[CommandList].crackedLists.push_back(list);

      m_Cmd->m_BakedCmdListInfo[CommandList].state.ApplyState(list);
    }

    // perform indirect draw, but from patched buffer. It will be patched between the above list and
    // this list during the first execution of the command list
    GetList(CommandList)
        ->ExecuteIndirect(comSig->GetReal(), maxCount, Unwrap(exec.argBuf), exec.argOffs,
                          Unwrap(countBuf), countOffs);
    GetCrackedList(CommandList)
        ->ExecuteIndirect(comSig->GetReal(), maxCount, Unwrap(exec.argBuf), exec.argOffs,
                          Unwrap(countBuf), countOffs);

    m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].executeEvents.push_back(exec);

    m_Cmd->m_BakedCmdListInfo[m_Cmd->m_LastCmdListID].curEventID++;

    // reserve the right number of drawcalls and events, to later be patched up with the actual
    // details
    ReserveExecuteIndirect(GetList(CommandList), sig, maxCount);
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
  m_pReal->ExecuteIndirect(Unwrap(pCommandSignature), MaxCommandCount, Unwrap(pArgumentBuffer),
                           ArgumentBufferOffset, Unwrap(pCountBuffer), CountBufferOffset);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(EXEC_INDIRECT);
    Serialise_ExecuteIndirect(pCommandSignature, MaxCommandCount, pArgumentBuffer,
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

bool WrappedID3D12GraphicsCommandList::Serialise_ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(PortableHandle, dsv, ToPortableHandle(DepthStencilView));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  SERIALISE_ELEMENT(D3D12_CLEAR_FLAGS, f, ClearFlags);
  SERIALISE_ELEMENT(FLOAT, d, Depth);
  SERIALISE_ELEMENT(UINT8, s, Stencil);
  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State == EXECUTING)
  {
    DepthStencilView = CPUHandleFromPortableHandle(GetResourceManager(), dsv);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->ClearDepthStencilView(DepthStencilView, f, d, s, num, rects);
    }
  }
  else if(m_State == READING)
  {
    DepthStencilView = CPUHandleFromPortableHandle(GetResourceManager(), dsv);

    GetList(CommandList)->ClearDepthStencilView(DepthStencilView, f, d, s, num, rects);
    GetCrackedList(CommandList)->ClearDepthStencilView(DepthStencilView, f, d, s, num, rects);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);
      string name = "ClearDepthStencilView(" + ToStr::Get(d) + "," + ToStr::Get(s) + ")";

      D3D12Descriptor *descriptor = DescriptorFromPortableHandle(GetResourceManager(), dsv);

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearDepthStencil;
      draw.copyDestination =
          GetResourceManager()->GetOriginalID(GetResID(descriptor->nonsamp.resource));

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(
          std::make_pair(GetResID(descriptor->nonsamp.resource),
                         EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
    }
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
    UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->ClearDepthStencilView(Unwrap(DepthStencilView), ClearFlags, Depth, Stencil, NumRects,
                                 pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_DSV);
    Serialise_ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

    {
      D3D12Descriptor *desc = GetWrapped(DepthStencilView);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Read);
    }
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects,
    const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(PortableHandle, rtv, ToPortableHandle(RenderTargetView));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  float Color[4] = {0};

  if(m_State >= WRITING)
    memcpy(Color, ColorRGBA, sizeof(float) * 4);

  m_pSerialiser->SerialisePODArray<4>("ColorRGBA", Color);

  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State == EXECUTING)
  {
    RenderTargetView = CPUHandleFromPortableHandle(GetResourceManager(), rtv);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->ClearRenderTargetView(RenderTargetView, Color, num, rects);
    }
  }
  else if(m_State == READING)
  {
    RenderTargetView = CPUHandleFromPortableHandle(GetResourceManager(), rtv);

    GetList(CommandList)->ClearRenderTargetView(RenderTargetView, Color, num, rects);
    GetCrackedList(CommandList)->ClearRenderTargetView(RenderTargetView, Color, num, rects);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);
      string name = "ClearRenderTargetView(" + ToStr::Get(Color[0]) + "," + ToStr::Get(Color[1]) +
                    "," + ToStr::Get(Color[2]) + "," + ToStr::Get(Color[3]) + ")";

      D3D12Descriptor *descriptor = DescriptorFromPortableHandle(GetResourceManager(), rtv);

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear | DrawFlags::ClearColor;
      draw.copyDestination =
          GetResourceManager()->GetOriginalID(GetResID(descriptor->nonsamp.resource));

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(
          std::make_pair(GetResID(descriptor->nonsamp.resource),
                         EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
    }
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects,
    const D3D12_RECT *pRects)
{
  m_pReal->ClearRenderTargetView(Unwrap(RenderTargetView), ColorRGBA, NumRects, pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_RTV);
    Serialise_ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

    {
      D3D12Descriptor *desc = GetWrapped(RenderTargetView);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Read);
    }
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(PortableHandle, gpuhandle, ToPortableHandle(ViewGPUHandleInCurrentHeap));
  SERIALISE_ELEMENT(PortableHandle, cpuhandle, ToPortableHandle(ViewCPUHandle));
  SERIALISE_ELEMENT(ResourceId, res, GetResID(pResource));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  UINT vals[4] = {0};

  if(m_State >= WRITING)
    memcpy(vals, Values, sizeof(UINT) * 4);

  m_pSerialiser->SerialisePODArray<4>("Values", vals);

  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State == EXECUTING)
  {
    ViewGPUHandleInCurrentHeap = GPUHandleFromPortableHandle(GetResourceManager(), gpuhandle);
    ViewCPUHandle = CPUHandleFromPortableHandle(GetResourceManager(), cpuhandle);
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                         Unwrap(pResource), vals, num, rects);
    }
  }
  else if(m_State == READING)
  {
    ViewGPUHandleInCurrentHeap = GPUHandleFromPortableHandle(GetResourceManager(), gpuhandle);
    ViewCPUHandle = CPUHandleFromPortableHandle(GetResourceManager(), cpuhandle);
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    GetList(CommandList)
        ->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, Unwrap(pResource),
                                       vals, num, rects);
    GetCrackedList(CommandList)
        ->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, Unwrap(pResource),
                                       vals, num, rects);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);
      string name = "ClearUnorderedAccessViewUint(" + ToStr::Get(vals[0]) + "," +
                    ToStr::Get(vals[1]) + "," + ToStr::Get(vals[2]) + "," + ToStr::Get(vals[3]) +
                    ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear;
      draw.copyDestination = res;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(std::make_pair(
          GetResID(pResource), EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
    }
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->ClearUnorderedAccessViewUint(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                        Unwrap(pResource), Values, NumRects, pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_UAV_INT);
    Serialise_ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource,
                                           Values, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

    {
      D3D12Descriptor *desc = GetWrapped(ViewGPUHandleInCurrentHeap);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Write);

      desc = GetWrapped(ViewCPUHandle);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Write);

      m_ListRecord->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_Write);
    }
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(PortableHandle, gpuhandle, ToPortableHandle(ViewGPUHandleInCurrentHeap));
  SERIALISE_ELEMENT(PortableHandle, cpuhandle, ToPortableHandle(ViewCPUHandle));
  SERIALISE_ELEMENT(ResourceId, res, GetResID(pResource));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  FLOAT vals[4] = {0};

  if(m_State >= WRITING)
    memcpy(vals, Values, sizeof(FLOAT) * 4);

  m_pSerialiser->SerialisePODArray<4>("Values", vals);

  SERIALISE_ELEMENT(UINT, num, NumRects);
  SERIALISE_ELEMENT_ARR(D3D12_RECT, rects, pRects, num);

  if(m_State == EXECUTING)
  {
    ViewGPUHandleInCurrentHeap = GPUHandleFromPortableHandle(GetResourceManager(), gpuhandle);
    ViewCPUHandle = CPUHandleFromPortableHandle(GetResourceManager(), cpuhandle);
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                          Unwrap(pResource), vals, num, rects);
    }
  }
  else if(m_State == READING)
  {
    ViewGPUHandleInCurrentHeap = GPUHandleFromPortableHandle(GetResourceManager(), gpuhandle);
    ViewCPUHandle = CPUHandleFromPortableHandle(GetResourceManager(), cpuhandle);
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    GetList(CommandList)
        ->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                        Unwrap(pResource), vals, num, rects);
    GetCrackedList(CommandList)
        ->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle,
                                        Unwrap(pResource), vals, num, rects);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);
      string name = "ClearUnorderedAccessViewFloat(" + ToStr::Get(vals[0]) + "," +
                    ToStr::Get(vals[1]) + "," + ToStr::Get(vals[2]) + "," + ToStr::Get(vals[3]) +
                    ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Clear;
      draw.copyDestination = res;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(std::make_pair(
          GetResID(pResource), EventUsage(drawNode.draw.eventID, ResourceUsage::Clear)));
    }
  }

  SAFE_DELETE_ARRAY(rects);

  return true;
}

void WrappedID3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
    ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
  m_pReal->ClearUnorderedAccessViewFloat(Unwrap(ViewGPUHandleInCurrentHeap), Unwrap(ViewCPUHandle),
                                         Unwrap(pResource), Values, NumRects, pRects);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_UAV_FLOAT);
    Serialise_ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource,
                                            Values, NumRects, pRects);

    m_ListRecord->AddChunk(scope.Get());

    {
      D3D12Descriptor *desc = GetWrapped(ViewGPUHandleInCurrentHeap);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Write);

      desc = GetWrapped(ViewCPUHandle);
      m_ListRecord->MarkResourceFrameReferenced(desc->nonsamp.heap->GetResourceID(), eFrameRef_Read);
      m_ListRecord->MarkResourceFrameReferenced(GetResID(desc->nonsamp.resource), eFrameRef_Write);

      m_ListRecord->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_Write);
    }
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_DiscardResource(ID3D12Resource *pResource,
                                                                 const D3D12_DISCARD_REGION *pRegion)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, res, GetResID(pResource));
  SERIALISE_ELEMENT(BOOL, HasRegion, pRegion != NULL);
  SERIALISE_ELEMENT_OPT(D3D12_DISCARD_REGION, region, *pRegion, HasRegion);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      Unwrap(m_Cmd->RerecordCmdList(CommandList))
          ->DiscardResource(Unwrap(pResource), HasRegion ? &region : NULL);
    }
  }
  else if(m_State == READING)
  {
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    GetList(CommandList)->DiscardResource(Unwrap(pResource), HasRegion ? &region : NULL);
    GetCrackedList(CommandList)->DiscardResource(Unwrap(pResource), HasRegion ? &region : NULL);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::DiscardResource(ID3D12Resource *pResource,
                                                       const D3D12_DISCARD_REGION *pRegion)
{
  m_pReal->DiscardResource(Unwrap(pResource), pRegion);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(DISCARD_RESOURCE);
    Serialise_DiscardResource(pResource, pRegion);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_Write);
  }
}

#pragma endregion Clears

#pragma region Copies

bool WrappedID3D12GraphicsCommandList::Serialise_CopyBufferRegion(ID3D12Resource *pDstBuffer,
                                                                  UINT64 DstOffset,
                                                                  ID3D12Resource *pSrcBuffer,
                                                                  UINT64 SrcOffset, UINT64 NumBytes)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, dst, GetResID(pDstBuffer));
  SERIALISE_ELEMENT(UINT64, dstoffs, DstOffset);
  SERIALISE_ELEMENT(ResourceId, src, GetResID(pSrcBuffer));
  SERIALISE_ELEMENT(UINT64, srcoffs, SrcOffset);
  SERIALISE_ELEMENT(UINT64, num, NumBytes);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    pDstBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);
      Unwrap(list)->CopyBufferRegion(Unwrap(pDstBuffer), dstoffs, Unwrap(pSrcBuffer), srcoffs, num);
    }
  }
  else if(m_State == READING)
  {
    pDstBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    GetList(CommandList)->CopyBufferRegion(Unwrap(pDstBuffer), dstoffs, Unwrap(pSrcBuffer), srcoffs, num);
    GetCrackedList(CommandList)
        ->CopyBufferRegion(Unwrap(pDstBuffer), dstoffs, Unwrap(pSrcBuffer), srcoffs, num);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);
      string name = "CopyBufferRegion(" + ToStr::Get(src) + "," + ToStr::Get(dst) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Copy;

      draw.copySource = src;
      draw.copyDestination = dst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(src == dst)
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::Copy)));
      }
      else
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pDstBuffer), EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource *pDstBuffer,
                                                        UINT64 DstOffset, ID3D12Resource *pSrcBuffer,
                                                        UINT64 SrcOffset, UINT64 NumBytes)
{
  m_pReal->CopyBufferRegion(Unwrap(pDstBuffer), DstOffset, Unwrap(pSrcBuffer), SrcOffset, NumBytes);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(COPY_BUFFER);
    Serialise_CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstBuffer), eFrameRef_Write);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcBuffer), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(D3D12_TEXTURE_COPY_LOCATION, dst, *pDst);
  SERIALISE_ELEMENT(UINT, dstX, DstX);
  SERIALISE_ELEMENT(UINT, dstY, DstY);
  SERIALISE_ELEMENT(UINT, dstZ, DstZ);
  SERIALISE_ELEMENT(D3D12_TEXTURE_COPY_LOCATION, src, *pSrc);
  SERIALISE_ELEMENT(bool, HasSrcBox, pSrcBox != NULL);
  SERIALISE_ELEMENT_OPT(D3D12_BOX, box, *pSrcBox, HasSrcBox);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);
      Unwrap(list)->CopyTextureRegion(&dst, dstX, dstY, dstZ, &src, HasSrcBox ? &box : NULL);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->CopyTextureRegion(&dst, dstX, dstY, dstZ, &src, HasSrcBox ? &box : NULL);
    GetCrackedList(CommandList)->CopyTextureRegion(&dst, dstX, dstY, dstZ, &src, HasSrcBox ? &box : NULL);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);

      ResourceId liveSrc = GetResID(GetResourceManager()->GetWrapper(src.pResource));
      ResourceId liveDst = GetResID(GetResourceManager()->GetWrapper(dst.pResource));

      ResourceId origSrc = GetResourceManager()->GetOriginalID(liveSrc);
      ResourceId origDst = GetResourceManager()->GetOriginalID(liveDst);

      string name = "CopyTextureRegion(" + ToStr::Get(origSrc) + "," + ToStr::Get(origDst) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Copy;

      draw.copySource = origSrc;
      draw.copyDestination = origDst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(origSrc == origDst)
      {
        drawNode.resourceUsage.push_back(
            std::make_pair(liveSrc, EventUsage(drawNode.draw.eventID, ResourceUsage::Copy)));
      }
      else
      {
        drawNode.resourceUsage.push_back(
            std::make_pair(liveSrc, EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(
            std::make_pair(liveDst, EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
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

  m_pReal->CopyTextureRegion(&dst, DstX, DstY, DstZ, &src, pSrcBox);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(COPY_TEXTURE);
    Serialise_CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDst->pResource), eFrameRef_Write);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrc->pResource), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_CopyResource(ID3D12Resource *pDstResource,
                                                              ID3D12Resource *pSrcResource)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, dst, GetResID(pDstResource));
  SERIALISE_ELEMENT(ResourceId, src, GetResID(pSrcResource));

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    pDstResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);
      Unwrap(list)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
    }
  }
  else if(m_State == READING)
  {
    pDstResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    GetList(CommandList)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));
    GetCrackedList(CommandList)->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);

      string name = "CopyResource(" + ToStr::Get(src) + "," + ToStr::Get(dst) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Copy;

      draw.copySource = src;
      draw.copyDestination = dst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(pSrcResource == pDstResource)
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, ResourceUsage::Copy)));
      }
      else
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, ResourceUsage::CopySrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pDstResource), EventUsage(drawNode.draw.eventID, ResourceUsage::CopyDst)));
      }
    }
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource,
                                                    ID3D12Resource *pSrcResource)
{
  m_pReal->CopyResource(Unwrap(pDstResource), Unwrap(pSrcResource));

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(COPY_RESOURCE);
    Serialise_CopyResource(pDstResource, pSrcResource);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_Write);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_ResolveSubresource(ID3D12Resource *pDstResource,
                                                                    UINT DstSubresource,
                                                                    ID3D12Resource *pSrcResource,
                                                                    UINT SrcSubresource,
                                                                    DXGI_FORMAT Format)
{
  SERIALISE_ELEMENT(ResourceId, CommandList, GetResourceID());
  SERIALISE_ELEMENT(ResourceId, dst, GetResID(pDstResource));
  SERIALISE_ELEMENT(UINT, dstSub, DstSubresource);
  SERIALISE_ELEMENT(ResourceId, src, GetResID(pSrcResource));
  SERIALISE_ELEMENT(UINT, srcSub, SrcSubresource);
  SERIALISE_ELEMENT(DXGI_FORMAT, fmt, Format);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  if(m_State == EXECUTING)
  {
    pDstResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);
      Unwrap(list)->ResolveSubresource(Unwrap(pDstResource), dstSub, Unwrap(pSrcResource), srcSub,
                                       fmt);
    }
  }
  else if(m_State == READING)
  {
    pDstResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(dst);
    pSrcResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(src);

    GetList(CommandList)
        ->ResolveSubresource(Unwrap(pDstResource), dstSub, Unwrap(pSrcResource), srcSub, fmt);
    GetCrackedList(CommandList)
        ->ResolveSubresource(Unwrap(pDstResource), dstSub, Unwrap(pSrcResource), srcSub, fmt);

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(desc);

      string name = "ResolveSubresource(" + ToStr::Get(src) + "," + ToStr::Get(dst) + ")";

      DrawcallDescription draw;
      draw.name = name;
      draw.flags |= DrawFlags::Resolve;

      draw.copySource = src;
      draw.copyDestination = dst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(pSrcResource == pDstResource)
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, ResourceUsage::Resolve)));
      }
      else
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, ResourceUsage::ResolveSrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pDstResource), EventUsage(drawNode.draw.eventID, ResourceUsage::ResolveDst)));
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
  m_pReal->ResolveSubresource(Unwrap(pDstResource), DstSubresource, Unwrap(pSrcResource),
                              SrcSubresource, Format);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(RESOLVE_SUBRESOURCE);
    Serialise_ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);

    m_ListRecord->AddChunk(scope.Get());
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_Write);
    m_ListRecord->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }
}

bool WrappedID3D12GraphicsCommandList::Serialise_CopyTiles(
    ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
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
  m_pReal->CopyTiles(Unwrap(pTiledResource), pTileRegionStartCoordinate, pTileRegionSize,
                     Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);
}

#pragma endregion Copies
