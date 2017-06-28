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

#include "d3d12_command_queue.h"
#include "d3d12_command_list.h"
#include "d3d12_resources.h"

bool WrappedID3D12CommandQueue::Serialise_UpdateTileMappings(
    ID3D12Resource *pResource, UINT NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges,
    const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets,
    const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
  D3D12NOTIMP("Tiled Resources");
  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::UpdateTileMappings(
    ID3D12Resource *pResource, UINT NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges,
    const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets,
    const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
  D3D12NOTIMP("Tiled Resources");
  m_pReal->UpdateTileMappings(Unwrap(pResource), NumResourceRegions, pResourceRegionStartCoordinates,
                              pResourceRegionSizes, Unwrap(pHeap), NumRanges, pRangeFlags,
                              pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}

bool WrappedID3D12CommandQueue::Serialise_CopyTileMappings(
    ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
    ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
  D3D12NOTIMP("Tiled Resources");
  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::CopyTileMappings(
    ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
    ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
  D3D12NOTIMP("Tiled Resources");
  m_pReal->CopyTileMappings(Unwrap(pDstResource), pDstRegionStartCoordinate, Unwrap(pSrcResource),
                            pSrcRegionStartCoordinate, pRegionSize, Flags);
}

bool WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists(UINT NumCommandLists,
                                                              ID3D12CommandList *const *ppCommandLists)
{
  SERIALISE_ELEMENT(ResourceId, queueId, GetResourceID());
  SERIALISE_ELEMENT(UINT, numCmds, NumCommandLists);

  vector<ResourceId> cmdIds;
  ID3D12GraphicsCommandList **cmds =
      m_State >= WRITING ? NULL : new ID3D12GraphicsCommandList *[numCmds];

  if(m_State >= WRITING)
  {
    for(UINT i = 0; i < numCmds; i++)
    {
      D3D12ResourceRecord *record = GetRecord(ppCommandLists[i]);
      RDCASSERT(record->bakedCommands);
      if(record->bakedCommands)
        cmdIds.push_back(record->bakedCommands->GetResourceID());
    }
  }

  m_pSerialiser->Serialise("ppCommandLists", cmdIds);

  if(m_State < WRITING)
  {
    for(UINT i = 0; i < numCmds; i++)
    {
      cmds[i] = cmdIds[i] != ResourceId()
                    ? GetResourceManager()->GetLiveAs<ID3D12GraphicsCommandList>(cmdIds[i])
                    : NULL;
    }
  }

  const string desc = m_pSerialiser->GetDebugStr();

  // debug messages
  {
    vector<DebugMessage> debugMessages;

    if(m_State == WRITING_CAPFRAME)
      debugMessages = m_pDevice->GetDebugMessages();

    SERIALISE_ELEMENT(uint32_t, NumMessages, (uint32_t)debugMessages.size());

    for(uint32_t i = 0; i < NumMessages; i++)
    {
      ScopedContext msgscope(m_pSerialiser, "DebugMessage", "DebugMessage", 0, false);

      string msgDesc;
      if(m_State >= WRITING)
        msgDesc = debugMessages[i].description.elems;

      SERIALISE_ELEMENT(MessageCategory, Category, debugMessages[i].category);
      SERIALISE_ELEMENT(MessageSeverity, Severity, debugMessages[i].severity);
      SERIALISE_ELEMENT(uint32_t, ID, debugMessages[i].messageID);
      SERIALISE_ELEMENT(string, Description, msgDesc);

      if(m_State == READING)
      {
        DebugMessage msg;
        msg.source = MessageSource::API;
        msg.category = Category;
        msg.severity = Severity;
        msg.messageID = ID;
        msg.description = Description;

        m_Cmd.m_EventMessages.push_back(msg);
      }
    }
  }

  ID3D12CommandQueue *real = NULL;

  if(m_State <= EXECUTING)
  {
    real = Unwrap(GetResourceManager()->GetLiveAs<ID3D12CommandQueue>(queueId));

    if(m_PrevQueueId != queueId)
    {
      RDCDEBUG("Previous queue execution was on queue %llu, now executing %llu, syncing GPU",
               m_PrevQueueId, queueId);
      if(m_PrevQueueId != ResourceId())
        m_pDevice->GPUSync(GetResourceManager()->GetLiveAs<ID3D12CommandQueue>(m_PrevQueueId));

      m_PrevQueueId = queueId;
    }
  }

  if(m_State == READING)
  {
    for(uint32_t i = 0; i < numCmds; i++)
    {
      if(m_Cmd.m_BakedCmdListInfo[cmdIds[i]].executeEvents.empty() ||
         m_Cmd.m_BakedCmdListInfo[cmdIds[i]].executeEvents[0].patched)
      {
        ID3D12CommandList *list = Unwrap(cmds[i]);
        real->ExecuteCommandLists(1, &list);
#if ENABLED(SINGLE_FLUSH_VALIDATE)
        m_pDevice->GPUSync();
#endif
      }
      else
      {
        BakedCmdListInfo &info = m_Cmd.m_BakedCmdListInfo[cmdIds[i]];

        // execute the first half of the cracked list
        ID3D12CommandList *list = Unwrap(info.crackedLists[0]);
        real->ExecuteCommandLists(1, &list);

        for(size_t c = 1; c < info.crackedLists.size(); c++)
        {
          m_pDevice->GPUSync();

          // readback the patch buffer and perform patching
          m_ReplayList->PatchExecuteIndirect(info, uint32_t(c - 1));

          // execute next list with this indirect.
          list = Unwrap(info.crackedLists[c]);
          real->ExecuteCommandLists(1, &list);
        }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        m_pDevice->GPUSync();
#endif
      }
    }

    for(uint32_t i = 0; i < numCmds; i++)
    {
      ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
      m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[cmd].barriers);
    }

    m_Cmd.AddEvent(desc);

    // we're adding multiple events, need to increment ourselves
    m_Cmd.m_RootEventID++;

    string basename = "ExecuteCommandLists(" + ToStr::Get(numCmds) + ")";

    for(uint32_t c = 0; c < numCmds; c++)
    {
      string name = StringFormat::Fmt("=> %s[%u]: ID3D12CommandList(%s)", basename.c_str(), c,
                                      ToStr::Get(cmdIds[c]).c_str());

      // add a fake marker
      DrawcallDescription draw;
      draw.name = name;
      draw.flags = DrawFlags::PassBoundary | DrawFlags::BeginPass;
      m_Cmd.AddEvent(name);
      m_Cmd.AddDrawcall(draw, true);
      m_Cmd.m_RootEventID++;

      BakedCmdListInfo &cmdBufInfo = m_Cmd.m_BakedCmdListInfo[cmdIds[c]];

      // insert the baked command list in-line into this list of notes, assigning new event and
      // drawIDs
      m_Cmd.InsertDrawsAndRefreshIDs(cmdIds[c], cmdBufInfo.draw->children);

      for(size_t e = 0; e < cmdBufInfo.draw->executedCmds.size(); e++)
      {
        vector<uint32_t> &submits =
            m_Cmd.m_Partial[D3D12CommandData::Secondary].cmdListExecs[cmdBufInfo.draw->executedCmds[e]];

        for(size_t s = 0; s < submits.size(); s++)
          submits[s] += m_Cmd.m_RootEventID;
      }

      for(size_t i = 0; i < cmdBufInfo.debugMessages.size(); i++)
      {
        DebugMessage msg = cmdBufInfo.debugMessages[i];
        msg.eventID += m_Cmd.m_RootEventID;
        m_pDevice->AddDebugMessage(msg);
      }

      // only primary command lists can be submitted
      m_Cmd.m_Partial[D3D12CommandData::Primary].cmdListExecs[cmdIds[c]].push_back(
          m_Cmd.m_RootEventID);

      m_Cmd.m_RootEventID += cmdBufInfo.eventCount;
      m_Cmd.m_RootDrawcallID += cmdBufInfo.drawCount;

      name = StringFormat::Fmt("=> %s[%u]: Close(%s)", basename.c_str(), c,
                               ToStr::Get(cmdIds[c]).c_str());
      draw.name = name;
      draw.flags = DrawFlags::PassBoundary | DrawFlags::EndPass;
      m_Cmd.AddEvent(name);
      m_Cmd.AddDrawcall(draw, true);
      m_Cmd.m_RootEventID++;
    }

    // account for the outer loop thinking we've added one event and incrementing,
    // since we've done all the handling ourselves this will be off by one.
    m_Cmd.m_RootEventID--;
  }
  else if(m_State == EXECUTING)
  {
    // account for the queue submit event
    m_Cmd.m_RootEventID++;

    uint32_t startEID = m_Cmd.m_RootEventID;

    // advance m_CurEventID to match the events added when reading
    for(uint32_t c = 0; c < numCmds; c++)
    {
      // 2 extra for the virtual labels around the command list
      m_Cmd.m_RootEventID += 2 + m_Cmd.m_BakedCmdListInfo[cmdIds[c]].eventCount;
      m_Cmd.m_RootDrawcallID += 2 + m_Cmd.m_BakedCmdListInfo[cmdIds[c]].drawCount;
    }

    // same accounting for the outer loop as above
    m_Cmd.m_RootEventID--;

    if(numCmds == 0)
    {
      // do nothing, don't bother with the logic below
    }
    else if(m_Cmd.m_LastEventID <= startEID)
    {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
      RDCDEBUG("Queue Submit no replay %u == %u", m_Cmd.m_LastEventID, startEID);
#endif
    }
    else if(m_Cmd.m_DrawcallCallback && m_Cmd.m_DrawcallCallback->RecordAllCmds())
    {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
      RDCDEBUG("Queue Submit re-recording from %u", m_Cmd.m_RootEventID);
#endif

      vector<ID3D12CommandList *> rerecordedCmds;

      for(uint32_t c = 0; c < numCmds; c++)
      {
        ID3D12CommandList *cmd = m_Cmd.RerecordCmdList(cmdIds[c]);
        ResourceId rerecord = GetResID(cmd);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
        RDCDEBUG("Queue Submit fully re-recorded replay of %llu, using %llu", cmdIds[c], rerecord);
#endif
        rerecordedCmds.push_back(Unwrap(cmd));

        m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[rerecord].barriers);
      }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      for(size_t i = 0; i < rerecordedCmds.size(); i++)
      {
        real->ExecuteCommandLists(1, &rerecordedCmds[i]);
        m_pDevice->GPUSync();
      }
#else
      real->ExecuteCommandLists((UINT)rerecordedCmds.size(), &rerecordedCmds[0]);
#endif
    }
    else if(m_Cmd.m_LastEventID > startEID && m_Cmd.m_LastEventID < m_Cmd.m_RootEventID)
    {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
      RDCDEBUG("Queue Submit partial replay %u < %u", m_Cmd.m_LastEventID, m_Cmd.m_RootEventID);
#endif

      uint32_t eid = startEID;

      vector<ResourceId> trimmedCmdIds;
      vector<ID3D12CommandList *> trimmedCmds;

      for(uint32_t c = 0; c < numCmds; c++)
      {
        // account for the virtual label at the start of the events here
        // so it matches up to baseEvent
        eid++;

        uint32_t end = eid + m_Cmd.m_BakedCmdListInfo[cmdIds[c]].eventCount;

        if(eid == m_Cmd.m_Partial[D3D12CommandData::Primary].baseEvent)
        {
          ID3D12GraphicsCommandList *list =
              m_Cmd.RerecordCmdList(cmdIds[c], D3D12CommandData::Primary);
          ResourceId partial = GetResID(list);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit partial replay of %llu at %u, using %llu", cmdIds[c], eid, partial);
#endif
          trimmedCmdIds.push_back(partial);
          trimmedCmds.push_back(Unwrap(list));
        }
        else if(m_Cmd.m_LastEventID >= end)
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit full replay %llu", cmdIds[c]);
#endif
          trimmedCmdIds.push_back(cmdIds[c]);
          trimmedCmds.push_back(Unwrap(GetResourceManager()->GetLiveAs<ID3D12CommandList>(cmdIds[c])));
        }
        else
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue not submitting %llu", cmdIds[c]);
#endif
        }

        // 1 extra to account for the virtual end command list label (begin is accounted for
        // above)
        eid += 1 + m_Cmd.m_BakedCmdListInfo[cmdIds[c]].eventCount;
      }

      RDCASSERT(trimmedCmds.size() > 0);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      for(size_t i = 0; i < trimmedCmds.size(); i++)
      {
        real->ExecuteCommandLists(1, &trimmedCmds[i]);
        m_pDevice->GPUSync();
      }
#else
      real->ExecuteCommandLists((UINT)trimmedCmds.size(), &trimmedCmds[0]);
#endif

      for(uint32_t i = 0; i < trimmedCmdIds.size(); i++)
      {
        ResourceId cmd = trimmedCmdIds[i];
        m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[cmd].barriers);
      }
    }
    else
    {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
      RDCDEBUG("Queue Submit full replay %u >= %u", m_Cmd.m_LastEventID, m_Cmd.m_RootEventID);
#endif

      ID3D12CommandList **unwrapped = new ID3D12CommandList *[numCmds];
      for(uint32_t i = 0; i < numCmds; i++)
        unwrapped[i] = Unwrap(cmds[i]);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      for(UINT i = 0; i < numCmds; i++)
      {
        real->ExecuteCommandLists(1, &unwrapped[i]);
        m_pDevice->GPUSync();
      }
#else
      real->ExecuteCommandLists(numCmds, unwrapped);
#endif

      SAFE_DELETE_ARRAY(unwrapped);

      for(uint32_t i = 0; i < numCmds; i++)
      {
        ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
        m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[cmd].barriers);
      }
    }
  }

  SAFE_DELETE_ARRAY(cmds);

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
  ID3D12CommandList **unwrapped = m_pDevice->GetTempArray<ID3D12CommandList *>(NumCommandLists);
  for(UINT i = 0; i < NumCommandLists; i++)
    unwrapped[i] = Unwrap(ppCommandLists[i]);

  m_pReal->ExecuteCommandLists(NumCommandLists, unwrapped);

  if(m_State >= WRITING)
  {
    SCOPED_LOCK(m_Lock);
    SCOPED_LOCK(m_pDevice->GetCapTransitionLock());

    bool capframe = (m_State == WRITING_CAPFRAME);
    set<ResourceId> refdIDs;

    for(UINT i = 0; i < NumCommandLists; i++)
    {
      D3D12ResourceRecord *record = GetRecord(ppCommandLists[i]);

      if(record->ContainsExecuteIndirect)
        m_QueueRecord->ContainsExecuteIndirect = true;

      m_pDevice->ApplyBarriers(record->bakedCommands->cmdInfo->barriers);

      // need to lock the whole section of code, not just the check on
      // m_State, as we also need to make sure we don't check the state,
      // start marking dirty resources then while we're doing so the
      // state becomes capframe.
      // the next sections where we mark resources referenced and add
      // the submit chunk to the frame record don't have to be protected.
      // Only the decision of whether we're inframe or not, and marking
      // dirty.
      if(capframe)
      {
        for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
            it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
          GetResourceManager()->MarkPendingDirty(*it);
      }
      else
      {
        for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
            it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
          GetResourceManager()->MarkDirtyResource(*it);
      }

      if(capframe)
      {
        // any descriptor copies or writes could reference new resources not in the
        // bound descs list yet. So we take all of those referenced descriptors and
        // include them to see if we need to flush
        std::vector<D3D12Descriptor> dynDescRefs;
        m_pDevice->GetDynamicDescriptorReferences(dynDescRefs);

        for(size_t d = 0; d < dynDescRefs.size(); d++)
        {
          ResourceId id, id2;
          FrameRefType ref = eFrameRef_Read;

          dynDescRefs[d].GetRefIDs(id, id2, ref);

          if(id != ResourceId())
          {
            refdIDs.insert(id);
            GetResourceManager()->MarkResourceFrameReferenced(id, ref);
          }

          if(id2 != ResourceId())
          {
            refdIDs.insert(id2);
            GetResourceManager()->MarkResourceFrameReferenced(id2, ref);
          }
        }

        // for each bound descriptor table, mark it referenced as well as all resources currently
        // bound to it
        for(auto it = record->bakedCommands->cmdInfo->boundDescs.begin();
            it != record->bakedCommands->cmdInfo->boundDescs.end(); ++it)
        {
          D3D12Descriptor *desc = *it;

          ResourceId id, id2;
          FrameRefType ref = eFrameRef_Read;

          desc->GetRefIDs(id, id2, ref);

          if(id != ResourceId())
          {
            refdIDs.insert(id);
            GetResourceManager()->MarkResourceFrameReferenced(id, ref);
          }

          if(id2 != ResourceId())
          {
            refdIDs.insert(id2);
            GetResourceManager()->MarkResourceFrameReferenced(id2, ref);
          }
        }

        // pull in frame refs from this baked command list
        record->bakedCommands->AddResourceReferences(GetResourceManager());
        record->bakedCommands->AddReferencedIDs(refdIDs);

        // reference all executed bundles as well
        for(size_t b = 0; b < record->bakedCommands->cmdInfo->bundles.size(); b++)
        {
          record->bakedCommands->cmdInfo->bundles[b]->bakedCommands->AddResourceReferences(
              GetResourceManager());
          record->bakedCommands->cmdInfo->bundles[b]->bakedCommands->AddReferencedIDs(refdIDs);
          GetResourceManager()->MarkResourceFrameReferenced(
              record->bakedCommands->cmdInfo->bundles[b]->GetResourceID(), eFrameRef_Read);

          record->bakedCommands->cmdInfo->bundles[b]->bakedCommands->AddRef();
        }

        {
          m_CmdListRecords.push_back(record->bakedCommands);
          for(size_t sub = 0; sub < record->bakedCommands->cmdInfo->bundles.size(); sub++)
            m_CmdListRecords.push_back(record->bakedCommands->cmdInfo->bundles[sub]->bakedCommands);
        }

        record->bakedCommands->AddRef();
      }

      record->cmdInfo->dirtied.clear();
    }

    if(capframe)
    {
      vector<MapState> maps = m_pDevice->GetMaps();

      for(auto it = maps.begin(); it != maps.end(); ++it)
      {
        WrappedID3D12Resource *res = it->res;
        UINT subres = it->subres;
        size_t size = (size_t)it->totalSize;

        // only need to flush memory that could affect this submitted batch of work
        if(refdIDs.find(res->GetResourceID()) == refdIDs.end())
        {
          RDCDEBUG("Map of memory %llu not referenced in this queue - not flushing",
                   res->GetResourceID());
          continue;
        }

        size_t diffStart = 0, diffEnd = 0;
        bool found = true;

        byte *ref = res->GetShadow(subres);
        byte *data = res->GetMap(subres);

        if(ref)
          found = FindDiffRange(data, ref, size, diffStart, diffEnd);
        else
          diffEnd = size;

        if(found)
        {
          RDCLOG("Persistent map flush forced for %llu (%llu -> %llu)", res->GetResourceID(),
                 (uint64_t)diffStart, (uint64_t)diffEnd);

          D3D12_RANGE range = {diffStart, diffEnd};

          m_pDevice->MapDataWrite(res, subres, data, range);

          if(ref == NULL)
          {
            res->AllocShadow(subres, size);

            ref = res->GetShadow(subres);
          }

          // update comparison shadow for next time
          memcpy(ref, res->GetMap(subres), size);

          GetResourceManager()->MarkPendingDirty(res->GetResourceID());
        }
        else
        {
          RDCDEBUG("Persistent map flush not needed for %llu", res->GetResourceID());
        }
      }

      for(UINT i = 0; i < NumCommandLists; i++)
      {
        SCOPED_SERIALISE_CONTEXT(EXECUTE_CMD_LISTS);
        Serialise_ExecuteCommandLists(1, ppCommandLists + i);

        m_QueueRecord->AddChunk(scope.Get());
      }
    }
  }
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::SetMarker(UINT Metadata, const void *pData,
                                                            UINT Size)
{
  m_pReal->SetMarker(Metadata, pData, Size);
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::BeginEvent(UINT Metadata, const void *pData,
                                                             UINT Size)
{
  m_pReal->BeginEvent(Metadata, pData, Size);
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::EndEvent()
{
  m_pReal->EndEvent();
}

bool WrappedID3D12CommandQueue::Serialise_Signal(ID3D12Fence *pFence, UINT64 Value)
{
  SERIALISE_ELEMENT(ResourceId, Fence, GetResID(pFence));
  SERIALISE_ELEMENT(UINT64, val, Value);

  if(m_State <= EXECUTING && GetResourceManager()->HasLiveResource(Fence))
  {
    pFence = GetResourceManager()->GetLiveAs<ID3D12Fence>(Fence);

    m_pReal->Signal(Unwrap(pFence), val);
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_LOCK(m_Lock);

    SCOPED_SERIALISE_CONTEXT(SIGNAL);
    Serialise_Signal(pFence, Value);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFence), eFrameRef_Read);
  }

  return m_pReal->Signal(Unwrap(pFence), Value);
}

bool WrappedID3D12CommandQueue::Serialise_Wait(ID3D12Fence *pFence, UINT64 Value)
{
  SERIALISE_ELEMENT(ResourceId, Fence, GetResID(pFence));
  SERIALISE_ELEMENT(UINT64, val, Value);

  if(m_State <= EXECUTING && GetResourceManager()->HasLiveResource(Fence))
  {
    // pFence = GetResourceManager()->GetLiveAs<ID3D12Fence>(Fence);

    m_pDevice->GPUSync();
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_LOCK(m_Lock);

    SCOPED_SERIALISE_CONTEXT(WAIT);
    Serialise_Wait(pFence, Value);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFence), eFrameRef_Read);
  }

  return m_pReal->Wait(Unwrap(pFence), Value);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::GetTimestampFrequency(UINT64 *pFrequency)
{
  return m_pReal->GetTimestampFrequency(pFrequency);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::GetClockCalibration(UINT64 *pGpuTimestamp,
                                                                         UINT64 *pCpuTimestamp)
{
  return m_pReal->GetClockCalibration(pGpuTimestamp, pCpuTimestamp);
}