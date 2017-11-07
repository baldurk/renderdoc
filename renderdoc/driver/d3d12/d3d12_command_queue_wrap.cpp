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

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_UpdateTileMappings(
    SerialiserType &ser, ID3D12Resource *pResource, UINT NumResourceRegions,
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

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_CopyTileMappings(
    SerialiserType &ser, ID3D12Resource *pDstResource,
    const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource,
    const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
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

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists(SerialiserType &ser,
                                                              UINT NumCommandLists,
                                                              ID3D12CommandList *const *ppCommandLists)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT_ARRAY(ppCommandLists, NumCommandLists);

  {
    std::vector<DebugMessage> DebugMessages;

    if(ser.IsWriting())
      DebugMessages = m_pDevice->GetDebugMessages();

    SERIALISE_ELEMENT(DebugMessages);

    if(ser.IsReading() && IsLoading(m_State))
    {
      for(const DebugMessage &msg : DebugMessages)
        m_Cmd.m_EventMessages.push_back(msg);
    }
  }

  if(IsReplayingAndReading())
  {
    ID3D12CommandQueue *real = Unwrap(pQueue);

    if(m_PrevQueueId != GetResID(pQueue))
    {
      RDCDEBUG("Previous queue execution was on queue %llu, now executing %llu, syncing GPU",
               m_PrevQueueId, GetResID(pQueue));
      if(m_PrevQueueId != ResourceId())
        m_pDevice->GPUSync(GetResourceManager()->GetCurrentAs<ID3D12CommandQueue>(m_PrevQueueId));

      m_PrevQueueId = GetResID(pQueue);
    }

    if(IsLoading(m_State))
    {
      for(uint32_t i = 0; i < NumCommandLists; i++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[i]));

        if(m_Cmd.m_BakedCmdListInfo[cmd].executeEvents.empty() ||
           m_Cmd.m_BakedCmdListInfo[cmd].executeEvents[0].patched)
        {
          ID3D12CommandList *list = Unwrap(ppCommandLists[i]);
          real->ExecuteCommandLists(1, &list);
#if ENABLED(SINGLE_FLUSH_VALIDATE)
          m_pDevice->GPUSync();
#endif
        }
        else
        {
          BakedCmdListInfo &info = m_Cmd.m_BakedCmdListInfo[cmd];

          // execute the first half of the cracked list
          ID3D12CommandList *list = Unwrap(info.crackedLists[0]);
          real->ExecuteCommandLists(1, &list);

          for(size_t c = 1; c < info.crackedLists.size(); c++)
          {
            // ensure all work on all queues has finished
            m_pDevice->GPUSyncAllQueues();

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

      for(uint32_t i = 0; i < NumCommandLists; i++)
      {
        ResourceId cmd = GetResID(ppCommandLists[i]);
        m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[cmd].barriers);
      }

      m_Cmd.AddEvent();

      // we're adding multiple events, need to increment ourselves
      m_Cmd.m_RootEventID++;

      std::string basename = StringFormat::Fmt("ExecuteCommandLists(%u)", NumCommandLists);

      for(uint32_t c = 0; c < NumCommandLists; c++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[c]));

        // add a fake marker
        DrawcallDescription draw;
        draw.name = StringFormat::Fmt("=> %s[%u]: ID3D12CommandList(%s)", basename.c_str(), c,
                                      ToStr(cmd).c_str());
        draw.flags = DrawFlags::PassBoundary | DrawFlags::BeginPass;
        m_Cmd.AddEvent();
        m_Cmd.AddDrawcall(draw, true);
        m_Cmd.m_RootEventID++;

        BakedCmdListInfo &cmdBufInfo = m_Cmd.m_BakedCmdListInfo[cmd];

        // insert the baked command list in-line into this list of notes, assigning new event and
        // drawIDs
        m_Cmd.InsertDrawsAndRefreshIDs(cmd, cmdBufInfo.draw->children);

        for(size_t e = 0; e < cmdBufInfo.draw->executedCmds.size(); e++)
        {
          vector<uint32_t> &submits = m_Cmd.m_Partial[D3D12CommandData::Secondary]
                                          .cmdListExecs[cmdBufInfo.draw->executedCmds[e]];

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
        m_Cmd.m_Partial[D3D12CommandData::Primary].cmdListExecs[cmd].push_back(m_Cmd.m_RootEventID);

        m_Cmd.m_RootEventID += cmdBufInfo.eventCount;
        m_Cmd.m_RootDrawcallID += cmdBufInfo.drawCount;

        draw.name =
            StringFormat::Fmt("=> %s[%u]: Close(%s)", basename.c_str(), c, ToStr(cmd).c_str());
        draw.flags = DrawFlags::PassBoundary | DrawFlags::EndPass;
        m_Cmd.AddEvent();
        m_Cmd.AddDrawcall(draw, true);
        m_Cmd.m_RootEventID++;
      }

      // account for the outer loop thinking we've added one event and incrementing,
      // since we've done all the handling ourselves this will be off by one.
      m_Cmd.m_RootEventID--;
    }
    else
    {
      // account for the queue submit event
      m_Cmd.m_RootEventID++;

      uint32_t startEID = m_Cmd.m_RootEventID;

      // advance m_CurEventID to match the events added when reading
      for(uint32_t c = 0; c < NumCommandLists; c++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[c]));

        // 2 extra for the virtual labels around the command list
        m_Cmd.m_RootEventID += 2 + m_Cmd.m_BakedCmdListInfo[cmd].eventCount;
        m_Cmd.m_RootDrawcallID += 2 + m_Cmd.m_BakedCmdListInfo[cmd].drawCount;
      }

      // same accounting for the outer loop as above
      m_Cmd.m_RootEventID--;

      if(NumCommandLists == 0)
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

        for(uint32_t c = 0; c < NumCommandLists; c++)
        {
          ResourceId cmdId = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[c]));

          ID3D12CommandList *cmd = m_Cmd.RerecordCmdList(cmdId);
          ResourceId rerecord = GetResID(cmd);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit fully re-recorded replay of %llu, using %llu", cmdId, rerecord);
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

        for(uint32_t c = 0; c < NumCommandLists; c++)
        {
          ResourceId cmdId = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[c]));

          // account for the virtual label at the start of the events here
          // so it matches up to baseEvent
          eid++;

          uint32_t end = eid + m_Cmd.m_BakedCmdListInfo[cmdId].eventCount;

          if(eid == m_Cmd.m_Partial[D3D12CommandData::Primary].baseEvent)
          {
            ID3D12GraphicsCommandList *list = m_Cmd.RerecordCmdList(cmdId, D3D12CommandData::Primary);
            ResourceId partial = GetResID(list);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Queue Submit partial replay of %llu at %u, using %llu", cmdId, eid, partial);
#endif
            trimmedCmdIds.push_back(partial);
            trimmedCmds.push_back(Unwrap(list));
          }
          else if(m_Cmd.m_LastEventID >= end)
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Queue Submit full replay %llu", cmdId);
#endif
            trimmedCmdIds.push_back(cmdId);
            trimmedCmds.push_back(Unwrap(ppCommandLists[c]));
          }
          else
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Queue not submitting %llu", cmdId);
#endif
          }

          // 1 extra to account for the virtual end command list label (begin is accounted for
          // above)
          eid += 1 + m_Cmd.m_BakedCmdListInfo[cmdId].eventCount;
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

        ID3D12CommandList **unwrapped = new ID3D12CommandList *[NumCommandLists];
        for(uint32_t i = 0; i < NumCommandLists; i++)
          unwrapped[i] = Unwrap(ppCommandLists[i]);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        for(UINT i = 0; i < NumCommandLists; i++)
        {
          real->ExecuteCommandLists(1, &unwrapped[i]);
          m_pDevice->GPUSync();
        }
#else
        real->ExecuteCommandLists(NumCommandLists, unwrapped);
#endif

        SAFE_DELETE_ARRAY(unwrapped);

        for(uint32_t i = 0; i < NumCommandLists; i++)
        {
          ResourceId cmd = GetResID(ppCommandLists[i]);
          m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[cmd].barriers);
        }
      }
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
  ID3D12CommandList **unwrapped = m_pDevice->GetTempArray<ID3D12CommandList *>(NumCommandLists);
  for(UINT i = 0; i < NumCommandLists; i++)
    unwrapped[i] = Unwrap(ppCommandLists[i]);

  m_pReal->ExecuteCommandLists(NumCommandLists, unwrapped);

  if(IsCaptureMode(m_State))
  {
    SCOPED_LOCK(m_Lock);
    SCOPED_LOCK(m_pDevice->GetCapTransitionLock());

    bool capframe = IsActiveCapturing(m_State);
    std::set<ResourceId> refdIDs;

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
      std::vector<MapState> maps = m_pDevice->GetMaps();

      for(auto it = maps.begin(); it != maps.end(); ++it)
      {
        WrappedID3D12Resource *res = GetWrapped(it->res);
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

      {
        WriteSerialiser &ser = m_ScratchSerialiser;
        ser.SetDrawChunk();
        SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_ExecuteCommandLists);
        Serialise_ExecuteCommandLists(ser, NumCommandLists, ppCommandLists);

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

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_Signal(SerialiserType &ser, ID3D12Fence *pFence,
                                                 UINT64 Value)
{
  SERIALISE_ELEMENT(pFence);
  SERIALISE_ELEMENT(Value);

  if(IsReplayingAndReading() && pFence)
  {
    m_pReal->Signal(Unwrap(pFence), Value);
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
  if(IsActiveCapturing(m_State))
  {
    SCOPED_LOCK(m_Lock);

    WriteSerialiser &ser = m_ScratchSerialiser;
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_Signal);
    Serialise_Signal(ser, pFence, Value);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFence), eFrameRef_Read);
  }

  return m_pReal->Signal(Unwrap(pFence), Value);
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_Wait(SerialiserType &ser, ID3D12Fence *pFence, UINT64 Value)
{
  SERIALISE_ELEMENT(pFence);
  SERIALISE_ELEMENT(Value);

  if(IsReplayingAndReading() && pFence)
  {
    m_pDevice->GPUSync();
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
  if(IsActiveCapturing(m_State))
  {
    SCOPED_LOCK(m_Lock);

    WriteSerialiser &ser = m_ScratchSerialiser;
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_Wait);
    Serialise_Wait(ser, pFence, Value);

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

INSTANTIATE_FUNCTION_SERIALISED(
    void, WrappedID3D12CommandQueue, UpdateTileMappings, ID3D12Resource *pResource,
    UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges,
    const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets,
    const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, CopyTileMappings,
                                ID3D12Resource *pDstResource,
                                const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
                                ID3D12Resource *pSrcResource,
                                const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
                                const D3D12_TILE_REGION_SIZE *pRegionSize,
                                D3D12_TILE_MAPPING_FLAGS Flags);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, ExecuteCommandLists,
                                UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, Signal, ID3D12Fence *pFence,
                                UINT64 Value);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, Wait, ID3D12Fence *pFence,
                                UINT64 Value);