/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#ifdef VERBOSE_PARTIAL_REPLAY
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

    if(m_State == READING && !m_Cmd->m_BakedCmdListInfo[CommandList].curEvents.empty())
    {
      FetchDrawcall draw;
      draw.name = "API Calls";
      draw.flags |= eDraw_SetMarker;

      m_Cmd->AddDrawcall(draw, true);

      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID++;
    }

    {
      if(m_Cmd->GetDrawcallStack().size() > 1)
        m_Cmd->GetDrawcallStack().pop_back();
    }

    m_Cmd->m_BakedCmdListInfo[bakeId].BakeFrom(m_Cmd->m_BakedCmdListInfo[CommandList]);
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
#ifdef VERBOSE_PARTIAL_REPLAY
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

      // pull all re-recorded commands from our own device and command pool for easier cleanup
      if(!partial)
        pAllocator = Unwrap(m_pDevice->GetAlloc());

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

      // On list execute we increment all child events/drawcalls by
      // m_RootEventID and insert them into the tree.
      m_Cmd->m_BakedCmdListInfo[CommandList].curEventID = 0;
      m_Cmd->m_BakedCmdListInfo[CommandList].eventCount = 0;
      m_Cmd->m_BakedCmdListInfo[CommandList].drawCount = 0;

      m_Cmd->m_BakedCmdListInfo[CommandList].drawStack.push_back(draw);
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
      list->ResourceBarrier((UINT)filtered.size(), &filtered[0]);

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
  D3D12_RESOURCE_BARRIER *barriers = new D3D12_RESOURCE_BARRIER[NumBarriers];

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

  SAFE_DELETE_ARRAY(barriers);

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
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps,
                                                          ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
  ID3D12DescriptorHeap **heaps = new ID3D12DescriptorHeap *[NumDescriptorHeaps];
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

  SAFE_DELETE_ARRAY(heaps);
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

    if(HasView)
    {
      list->IASetIndexBuffer(&view);

      m_Cmd->m_BakedCmdListInfo[CommandList].state.idxWidth =
          (view.Format == DXGI_FORMAT_R32_UINT ? 4 : 2);
    }
    else
    {
      list->IASetIndexBuffer(NULL);

      m_Cmd->m_BakedCmdListInfo[CommandList].state.idxWidth = 2;
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

void WrappedID3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews,
                                                    const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->SOSetTargets(StartSlot, NumViews, pViews);
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

  UINT numHandles = singlehandle ? 1U : num;

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

    RDCEraseEl(m_Cmd->m_BakedCmdListInfo[CommandList].state.dsv);
    RDCEraseEl(m_Cmd->m_BakedCmdListInfo[CommandList].state.rts);

    if(singlehandle)
    {
      WrappedID3D12DescriptorHeap *heap =
          GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(rts[0].heap);

      const D3D12Descriptor *descs = heap->GetDescriptors() + rts[0].index;

      for(UINT i = 0; i < num; i++)
      {
        RDCASSERT(descs[i].GetType() == D3D12Descriptor::TypeRTV);
        m_Cmd->m_BakedCmdListInfo[CommandList].state.rts[i] = GetResID(descs[i].nonsamp.resource);
      }
    }
    else
    {
      for(UINT i = 0; i < num; i++)
      {
        WrappedID3D12DescriptorHeap *heap =
            GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(rts[0].heap);

        const D3D12Descriptor &desc = heap->GetDescriptors()[rts[i].index];

        RDCASSERT(desc.GetType() == D3D12Descriptor::TypeRTV);
        m_Cmd->m_BakedCmdListInfo[CommandList].state.rts[i] = GetResID(desc.nonsamp.resource);
      }
    }

    if(dsv.heap != ResourceId())
    {
      WrappedID3D12DescriptorHeap *heap =
          GetResourceManager()->GetLiveAs<WrappedID3D12DescriptorHeap>(dsv.heap);

      const D3D12Descriptor &desc = heap->GetDescriptors()[dsv.index];

      RDCASSERT(desc.GetType() == D3D12Descriptor::TypeDSV);

      m_Cmd->m_BakedCmdListInfo[CommandList].state.dsv = GetResID(desc.nonsamp.resource);
    }

    SAFE_DELETE_ARRAY(rtHandles);
  }

  return true;
}

void WrappedID3D12GraphicsCommandList::OMSetRenderTargets(
    UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
  UINT numHandles = RTsSingleHandleToDescriptorRange ? 1U : NumRenderTargetDescriptors;
  D3D12_CPU_DESCRIPTOR_HANDLE *unwrapped = new D3D12_CPU_DESCRIPTOR_HANDLE[numHandles];
  for(UINT i = 0; i < numHandles; i++)
    unwrapped[i] = Unwrap(pRenderTargetDescriptors[i]);

  D3D12_CPU_DESCRIPTOR_HANDLE dsv =
      pDepthStencilDescriptor ? Unwrap(*pDepthStencilDescriptor) : D3D12_CPU_DESCRIPTOR_HANDLE();

  m_pReal->OMSetRenderTargets(NumRenderTargetDescriptors, unwrapped, RTsSingleHandleToDescriptorRange,
                              pDepthStencilDescriptor ? &dsv : NULL);

  SAFE_DELETE_ARRAY(unwrapped);

  if(m_State >= WRITING)
  {
    SCOPED_SERIALISE_CONTEXT(SET_RTVS);
    Serialise_OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors,
                                 RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);

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

    vector<D3D12_DESCRIPTOR_RANGE> &ranges =
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

      for(UINT d = 0; d < num; d++)
        m_ListRecord->cmdInfo->boundDescs.insert(rangeStart + d);

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

      m_Cmd->m_RenderState.compute.sigelems[idx] = D3D12RenderState::SignatureElement(offs, val);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetComputeRoot32BitConstant(idx, val, offs);
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

      m_Cmd->m_RenderState.compute.sigelems[idx] =
          D3D12RenderState::SignatureElement(num, data, offs);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetComputeRoot32BitConstants(idx, num, data, offs);
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
    m_Cmd->m_LastCmdListID = CommandList;

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
    m_Cmd->m_LastCmdListID = CommandList;

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
    m_Cmd->m_LastCmdListID = CommandList;

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

    vector<D3D12_DESCRIPTOR_RANGE> &ranges =
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

      for(UINT d = 0; d < num; d++)
        m_ListRecord->cmdInfo->boundDescs.insert(rangeStart + d);

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

      m_Cmd->m_RenderState.graphics.sigelems[idx] = D3D12RenderState::SignatureElement(offs, val);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetGraphicsRoot32BitConstant(idx, val, offs);
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

      m_Cmd->m_RenderState.graphics.sigelems[idx] =
          D3D12RenderState::SignatureElement(num, data, offs);
    }
  }
  else if(m_State == READING)
  {
    GetList(CommandList)->SetGraphicsRoot32BitConstants(idx, num, data, offs);
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
    m_Cmd->m_LastCmdListID = CommandList;

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
    m_Cmd->m_LastCmdListID = CommandList;

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
    m_Cmd->m_LastCmdListID = CommandList;

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

void WrappedID3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap *pQueryHeap,
                                                  D3D12_QUERY_TYPE Type, UINT Index)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->BeginQuery(Unwrap(pQueryHeap), Type, Index);
}

void WrappedID3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                                UINT Index)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->EndQuery(Unwrap(pQueryHeap), Type, Index);
}

void WrappedID3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap *pQueryHeap,
                                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                                        UINT NumQueries,
                                                        ID3D12Resource *pDestinationBuffer,
                                                        UINT64 AlignedDestinationBufferOffset)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->ResolveQueryData(Unwrap(pQueryHeap), Type, StartIndex, NumQueries,
                            Unwrap(pDestinationBuffer), AlignedDestinationBufferOffset);
}

void WrappedID3D12GraphicsCommandList::SetPredication(ID3D12Resource *pBuffer,
                                                      UINT64 AlignedBufferOffset,
                                                      D3D12_PREDICATION_OP Operation)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->SetPredication(Unwrap(pBuffer), AlignedBufferOffset, Operation);
}

bool WrappedID3D12GraphicsCommandList::Serialise_SetMarker(UINT Metadata, const void *pData, UINT Size)
{
  string markerText = "";

  if(m_State >= WRITING && pData && Size)
  {
    static const UINT PIX_EVENT_UNICODE_VERSION = 0;
    static const UINT PIX_EVENT_ANSI_VERSION = 1;

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
    FetchDrawcall draw;
    draw.name = markerText;
    draw.flags |= eDraw_SetMarker;

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
    static const UINT PIX_EVENT_UNICODE_VERSION = 0;
    static const UINT PIX_EVENT_ANSI_VERSION = 1;

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
    FetchDrawcall draw;
    draw.name = markerText;
    draw.flags |= eDraw_PushMarker;

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
    FetchDrawcall draw;
    draw.name = "API Calls";
    draw.flags = eDraw_SetMarker;

    m_Cmd->AddDrawcall(draw, true);
  }

  if(m_State == READING)
  {
    // dummy draw that is consumed when this command buffer
    // is being in-lined into the call stream
    FetchDrawcall draw;
    draw.name = "Pop()";
    draw.flags = eDraw_PopMarker;

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

  D3D12NOTIMP("Serialise_DebugMessages");

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

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(DRAW_INST, desc);
    string name = "DrawInstanced(" + ToStr::Get(vtxCount) + ", " + ToStr::Get(instCount) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = vtxCount;
    draw.numInstances = instCount;
    draw.indexOffset = 0;
    draw.baseVertex = startVtx;
    draw.instanceOffset = startInst;

    draw.flags |= eDraw_Drawcall | eDraw_Instanced;

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

  D3D12NOTIMP("Serialise_DebugMessages");

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

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(DRAW_INDEXED_INST, desc);
    string name =
        "DrawIndexedInstanced(" + ToStr::Get(idxCount) + ", " + ToStr::Get(instCount) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.numIndices = idxCount;
    draw.numInstances = instCount;
    draw.indexOffset = startIdx;
    draw.baseVertex = startVtx;
    draw.instanceOffset = startInst;

    draw.flags |= eDraw_Drawcall | eDraw_Instanced | eDraw_UseIBuffer;

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

  D3D12NOTIMP("Serialise_DebugMessages");

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

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(DISPATCH, desc);
    string name = "Dispatch(" + ToStr::Get(x) + ", " + ToStr::Get(y) + ", " + ToStr::Get(z) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.dispatchDimension[0] = x;
    draw.dispatchDimension[1] = y;
    draw.dispatchDimension[2] = z;

    draw.flags |= eDraw_Dispatch;

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

void WrappedID3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->ExecuteBundle(Unwrap(pCommandList));
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
  SERIALISE_ELEMENT(ResourceId, count, GetResID(pCountBuffer));
  SERIALISE_ELEMENT(UINT64, countOffs, CountBufferOffset);

  if(m_State < WRITING)
    m_Cmd->m_LastCmdListID = CommandList;

  D3D12NOTIMP("Serialise_DebugMessages");

  if(m_State == EXECUTING)
  {
    pCommandSignature = GetResourceManager()->GetLiveAs<ID3D12CommandSignature>(sig);
    pArgumentBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(arg);
    pCountBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(count);

    if(m_Cmd->ShouldRerecordCmd(CommandList) && m_Cmd->InRerecordRange(CommandList))
    {
      ID3D12GraphicsCommandList *list = m_Cmd->RerecordCmdList(CommandList);

      Unwrap(list)->ExecuteIndirect(Unwrap(pCommandSignature), maxCount, Unwrap(pArgumentBuffer),
                                    argOffs, Unwrap(pCountBuffer), countOffs);
    }
  }
  else if(m_State == READING)
  {
    pCommandSignature = GetResourceManager()->GetLiveAs<ID3D12CommandSignature>(sig);
    pArgumentBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(arg);
    pCountBuffer = GetResourceManager()->GetLiveAs<ID3D12Resource>(count);

    GetList(CommandList)
        ->ExecuteIndirect(Unwrap(pCommandSignature), maxCount, Unwrap(pArgumentBuffer), argOffs,
                          Unwrap(pCountBuffer), countOffs);

    const string desc = m_pSerialiser->GetDebugStr();

    m_Cmd->AddEvent(EXEC_INDIRECT, desc);
    string name = "ExecuteIndirect(...)";

    FetchDrawcall draw;
    draw.name = name;

    draw.flags |= eDraw_CmdList;

    m_Cmd->AddDrawcall(draw, true);
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(CLEAR_DSV, desc);
      string name = "ClearDepthStencilView(" + ToStr::Get(d) + "," + ToStr::Get(s) + ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Clear | eDraw_ClearDepthStencil;

      m_Cmd->AddDrawcall(draw, true);

      D3D12NOTIMP("Getting image for DSV to mark usage");

      // D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      // drawNode.resourceUsage.push_back(
      //    std::make_pair(GetResID(image), EventUsage(drawNode.draw.eventID, eUsage_Clear)));
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(CLEAR_RTV, desc);
      string name = "ClearRenderTargetView(" + ToStr::Get(Color[0]) + "," + ToStr::Get(Color[1]) +
                    "," + ToStr::Get(Color[2]) + "," + ToStr::Get(Color[3]) + ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Clear | eDraw_ClearColour;

      m_Cmd->AddDrawcall(draw, true);

      D3D12NOTIMP("Getting image for RTV to mark usage");

      // D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      // drawNode.resourceUsage.push_back(
      //    std::make_pair(GetResID(image), EventUsage(drawNode.draw.eventID, eUsage_Clear)));
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(CLEAR_UAV_INT, desc);
      string name = "ClearUnorderedAccessViewUint(" + ToStr::Get(vals[0]) + "," +
                    ToStr::Get(vals[1]) + "," + ToStr::Get(vals[2]) + "," + ToStr::Get(vals[3]) +
                    ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Clear;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(
          std::make_pair(GetResID(pResource), EventUsage(drawNode.draw.eventID, eUsage_Clear)));
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(CLEAR_UAV_INT, desc);
      string name = "ClearUnorderedAccessViewFloat(" + ToStr::Get(vals[0]) + "," +
                    ToStr::Get(vals[1]) + "," + ToStr::Get(vals[2]) + "," + ToStr::Get(vals[3]) +
                    ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Clear;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      drawNode.resourceUsage.push_back(
          std::make_pair(GetResID(pResource), EventUsage(drawNode.draw.eventID, eUsage_Clear)));
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
      Unwrap(m_Cmd->RerecordCmdList(CommandList))->DiscardResource(pResource, HasRegion ? &region : NULL);
    }
  }
  else if(m_State == READING)
  {
    pResource = GetResourceManager()->GetLiveAs<ID3D12Resource>(res);

    GetList(CommandList)->DiscardResource(pResource, HasRegion ? &region : NULL);
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(COPY_BUFFER, desc);
      string name = "CopyBufferRegion(" + ToStr::Get(src) + "," + ToStr::Get(dst) + ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Copy;

      draw.copySource = src;
      draw.copyDestination = dst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(src == dst)
      {
        drawNode.resourceUsage.push_back(
            std::make_pair(GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventID, eUsage_Copy)));
      }
      else
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcBuffer), EventUsage(drawNode.draw.eventID, eUsage_CopySrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pDstBuffer), EventUsage(drawNode.draw.eventID, eUsage_CopyDst)));
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(COPY_TEXTURE, desc);

      ResourceId liveSrc = GetResID(GetResourceManager()->GetWrapper(src.pResource));
      ResourceId liveDst = GetResID(GetResourceManager()->GetWrapper(dst.pResource));

      ResourceId origSrc = GetResourceManager()->GetOriginalID(liveSrc);
      ResourceId origDst = GetResourceManager()->GetOriginalID(liveDst);

      string name = "CopyTextureRegion(" + ToStr::Get(origSrc) + "," + ToStr::Get(origDst) + ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Copy;

      draw.copySource = origSrc;
      draw.copyDestination = origDst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(origSrc == origDst)
      {
        drawNode.resourceUsage.push_back(
            std::make_pair(liveSrc, EventUsage(drawNode.draw.eventID, eUsage_Copy)));
      }
      else
      {
        drawNode.resourceUsage.push_back(
            std::make_pair(liveSrc, EventUsage(drawNode.draw.eventID, eUsage_CopySrc)));
        drawNode.resourceUsage.push_back(
            std::make_pair(liveDst, EventUsage(drawNode.draw.eventID, eUsage_CopyDst)));
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(COPY_RESOURCE, desc);

      string name = "CopyResource(" + ToStr::Get(src) + "," + ToStr::Get(dst) + ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eDraw_Copy;

      draw.copySource = src;
      draw.copyDestination = dst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(pSrcResource == pDstResource)
      {
        drawNode.resourceUsage.push_back(
            std::make_pair(GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, eUsage_Copy)));
      }
      else
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, eUsage_CopySrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pDstResource), EventUsage(drawNode.draw.eventID, eUsage_CopyDst)));
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

    const string desc = m_pSerialiser->GetDebugStr();

    {
      m_Cmd->AddEvent(RESOLVE_SUBRESOURCE, desc);

      string name = "ResolveSubresource(" + ToStr::Get(src) + "," + ToStr::Get(dst) + ")";

      FetchDrawcall draw;
      draw.name = name;
      draw.flags |= eUsage_Resolve;

      draw.copySource = src;
      draw.copyDestination = dst;

      m_Cmd->AddDrawcall(draw, true);

      D3D12DrawcallTreeNode &drawNode = m_Cmd->GetDrawcallStack().back()->children.back();

      if(pSrcResource == pDstResource)
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, eUsage_Resolve)));
      }
      else
      {
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pSrcResource), EventUsage(drawNode.draw.eventID, eUsage_ResolveSrc)));
        drawNode.resourceUsage.push_back(std::make_pair(
            GetResID(pDstResource), EventUsage(drawNode.draw.eventID, eUsage_ResolveDst)));
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

void WrappedID3D12GraphicsCommandList::CopyTiles(
    ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
  D3D12NOTIMP(__PRETTY_FUNCTION_SIGNATURE__);
  m_pReal->CopyTiles(Unwrap(pTiledResource), pTileRegionStartCoordinate, pTileRegionSize,
                     Unwrap(pBuffer), BufferStartOffsetInBytes, Flags);
}

#pragma endregion Copies
