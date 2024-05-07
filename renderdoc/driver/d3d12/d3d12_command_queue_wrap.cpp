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

#include "d3d12_command_queue.h"
#include "core/settings.h"
#include "d3d12_command_list.h"
#include "d3d12_resources.h"

RDOC_EXTERN_CONFIG(bool, D3D12_Debug_SingleSubmitFlushing);

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_UpdateTileMappings(
    SerialiserType &ser, ID3D12Resource *pResource, UINT NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges,
    const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets,
    const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(pResource).Important();
  SERIALISE_ELEMENT(NumResourceRegions);
  SERIALISE_ELEMENT_ARRAY(pResourceRegionStartCoordinates, NumResourceRegions);
  SERIALISE_ELEMENT_ARRAY(pResourceRegionSizes, NumResourceRegions);
  SERIALISE_ELEMENT(pHeap).Important();
  SERIALISE_ELEMENT(NumRanges);
  SERIALISE_ELEMENT_ARRAY(pRangeFlags, NumRanges);
  SERIALISE_ELEMENT_ARRAY(pHeapRangeStartOffsets, NumRanges);
  SERIALISE_ELEMENT_ARRAY(pRangeTileCounts, NumRanges);
  SERIALISE_ELEMENT(Flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      m_SparseBindResources.insert(GetResID(pResource));

    // don't replay with NO_HAZARD
    m_pReal->UpdateTileMappings(Unwrap(pResource), NumResourceRegions,
                                pResourceRegionStartCoordinates, pResourceRegionSizes,
                                Unwrap(pHeap), NumRanges, pRangeFlags, pHeapRangeStartOffsets,
                                pRangeTileCounts, Flags & ~D3D12_TILE_MAPPING_FLAG_NO_HAZARD);
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::UpdateTileMappings(
    ID3D12Resource *pResource, UINT NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges,
    const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets,
    const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
  SERIALISE_TIME_CALL(m_pReal->UpdateTileMappings(
      Unwrap(pResource), NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes,
      Unwrap(pHeap), NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_UpdateTileMappings);
    Serialise_UpdateTileMappings(ser, pResource, NumResourceRegions, pResourceRegionStartCoordinates,
                                 pResourceRegionSizes, pHeap, NumRanges, pRangeFlags,
                                 pHeapRangeStartOffsets, pRangeTileCounts, Flags);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pResource), eFrameRef_Read);
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pHeap), eFrameRef_Read);
  }

  // update our internal page tables
  if(IsCaptureMode(m_State))
  {
    Sparse::PageTable &pageTable = *GetRecord(pResource)->sparseTable;
    ResourceId memoryId = GetResID(pHeap);

    // register this heap as having been used for sparse binding
    m_pDevice->AddSparseHeap(GetResID(pHeap));

// define macros to help provide the defaults for NULL arrays
#define REGION_START(i)                                                 \
  (pResourceRegionStartCoordinates ? pResourceRegionStartCoordinates[i] \
                                   : D3D12_TILED_RESOURCE_COORDINATE({0, 0, 0, 0}))
// The default for size depends on whether a co-ordinate is set (ughhhh). If we do have co-ordinates
// then the sizes are all 1 tile. If we don't, then the size is the whole resource. Ideally we'd
// provide the exact number of tiles, but instead we just set ~0U and the sparse table interprets
// this as 'unbounded tiles'
#define REGION_SIZE(i)                                                                  \
  (pResourceRegionSizes                                                                 \
       ? pResourceRegionSizes[i]                                                        \
       : (pResourceRegionStartCoordinates ? D3D12_TILE_REGION_SIZE({1, FALSE, 1, 1, 1}) \
                                          : D3D12_TILE_REGION_SIZE({~0U, FALSE, 1, 1, 1})))
#define RANGE_FLAGS(i) (pRangeFlags ? pRangeFlags[i] : D3D12_TILE_RANGE_FLAG_NONE)
// don't think there is any default for this one, but we just return 0 for consistency and safety
// since the array CAN be NULL when it's ignored
#define RANGE_OFFSET(i) (pHeapRangeStartOffsets ? pHeapRangeStartOffsets[i] : 0)
#define RANGE_SIZE(i) (pRangeTileCounts ? pRangeTileCounts[i] : ~0U)

    const UINT pageSize = 64 * 1024;
    const Sparse::Coord texelShape = pageTable.getPageTexelSize();

    // this persists from loop to loop. The effective offset is rangeBaseOffset +
    // curRelativeRangeOffset. That allows us to partially use a range in one region then another.
    // This goes from 0 to whatever rangeSize is
    UINT curRelativeRangeOffset = 0;

    // iterate region at a time
    UINT curRange = 0;
    for(UINT curRegion = 0; curRegion < NumResourceRegions && curRange < NumRanges; curRegion++)
    {
      D3D12_TILED_RESOURCE_COORDINATE regionStart = REGION_START(curRegion);
      D3D12_TILE_REGION_SIZE regionSize = REGION_SIZE(curRegion);

      // sanitise the region size according to the dimensions of the texture
      // clamp inputs that may be invalid for buffers or 2D to sensible values
      regionSize.Width = RDCCLAMP(1U, regionSize.Width, pageTable.getResourceSize().x);
      regionSize.Height =
          (uint16_t)RDCCLAMP(1U, (uint32_t)regionSize.Height, pageTable.getResourceSize().y);
      regionSize.Depth =
          (uint16_t)RDCCLAMP(1U, (uint32_t)regionSize.Depth, pageTable.getResourceSize().z);

      UINT rangeBaseOffset = RANGE_OFFSET(curRange);
      UINT rangeSize = RANGE_SIZE(curRange);
      D3D12_TILE_RANGE_FLAGS rangeFlags = RANGE_FLAGS(curRange);

      // get the memory ID, respecting the NULL flag
      ResourceId memId = memoryId;
      if(rangeFlags & D3D12_TILE_RANGE_FLAG_NULL)
        memId = ResourceId();

      // store if we're skipping for this range
      bool skip = false;
      if(rangeFlags & D3D12_TILE_RANGE_FLAG_SKIP)
        skip = true;

      // take the current range offset (which might be partway into the current range even at
      // the start of a region). Unless we're re-using a single tile in which case it's always the
      // start of the region
      bool singlePage = false;
      uint32_t memoryOffsetInTiles = rangeBaseOffset + curRelativeRangeOffset;
      if(rangeFlags & D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE)
      {
        memoryOffsetInTiles = RANGE_OFFSET(curRange);
        singlePage = true;
      }

      // if the region is a box region, contained within a subresource
      if(regionSize.UseBox)
      {
        // if this region is entirely within the current range, set it as one
        if(regionSize.NumTiles <= rangeSize)
        {
          if(skip)
          {
            // do no binding if we're skipping, because this whole range covers the region
          }
          else
          {
            pageTable.setImageBoxRange(
                regionStart.Subresource,
                {regionStart.X * texelShape.x, regionStart.Y * texelShape.y,
                 regionStart.Z * texelShape.z},
                {regionSize.Width * texelShape.x, regionSize.Height * texelShape.y,
                 regionSize.Depth * texelShape.z},
                memId, memoryOffsetInTiles * pageSize, singlePage);
          }

          // consume the number of tiles in the range, which might not be all of them
          curRelativeRangeOffset += regionSize.NumTiles;

          // however if it is, then move to the next range. We don't need to reset most range
          // parameters because they'll be refreshed on the next region, however the exception is
          // the range offset which is persistent region-to-region because we might use only part of
          // a range on one region.
          if(curRelativeRangeOffset >= rangeSize)
          {
            curRange++;

            curRelativeRangeOffset = 0;
            if(curRange < NumRanges)
              rangeBaseOffset = RANGE_OFFSET(curRange);
          }

          // we're done with this region, we'll loop around now
        }
        // if the region isn't contained within a single range, iterate tile-by-tile
        else
        {
          // the region spans multiple ranges. Fall back to tile-by-tile setting
          for(UINT z = 0; z < regionSize.Depth; z++)
          {
            for(UINT y = 0; y < regionSize.Height; y++)
            {
              for(UINT x = 0; x < regionSize.Width; x++)
              {
                if(skip)
                {
                  // do nothing
                }
                else
                {
                  pageTable.setImageBoxRange(
                      regionStart.Subresource,
                      {(regionStart.X + x) * texelShape.x, (regionStart.Y + y) * texelShape.y,
                       (regionStart.Z + z) * texelShape.z},
                      texelShape, memId, memoryOffsetInTiles * pageSize, singlePage);
                }

                // consume one tile, and also advance the memory offset if we're not in single page
                // mode
                curRelativeRangeOffset += 1;
                if(!singlePage)
                  memoryOffsetInTiles += 1;

                // if we've consumed everything in the current range, move to the next one
                if(curRelativeRangeOffset >= rangeSize)
                {
                  curRange++;
                  curRelativeRangeOffset = 0;

                  if(curRange < NumRanges)
                  {
                    rangeBaseOffset = RANGE_OFFSET(curRange);
                    rangeFlags = RANGE_FLAGS(curRange);
                    rangeSize = RANGE_SIZE(curRange);

                    skip = false;
                    if(rangeFlags & D3D12_TILE_RANGE_FLAG_SKIP)
                      skip = true;

                    memId = memoryId;
                    if(rangeFlags & D3D12_TILE_RANGE_FLAG_NULL)
                      memId = ResourceId();

                    memoryOffsetInTiles = rangeBaseOffset + curRelativeRangeOffset;
                    singlePage = false;
                    if(rangeFlags & D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE)
                      singlePage = true;
                  }
                }
              }
            }
          }

          // done with the x,y,z loop. Continue to the next region. We handled any range wrapping in
          // the innermost loop so we don't have to do anything here
        }
      }
      // the region isn't a box region, so it can wrap
      else
      {
        // set up the starting co-ord. setImageWrappedRange will help us iterate from here
        rdcpair<uint32_t, Sparse::Coord> curCoord = {
            regionStart.Subresource,
            {regionStart.X * texelShape.x, regionStart.Y * texelShape.y,
             regionStart.Z * texelShape.z}};

        // consume a region at a time setting it. The page table will handle detecting any
        // whole-subresource sets
        for(UINT i = 0; i < regionSize.NumTiles;)
        {
          // we consume either the rest of the range or the rest of the region, whichever is least
          UINT tilesToConsume = RDCMIN(regionSize.NumTiles - i, rangeSize - curRelativeRangeOffset);

          RDCASSERT(tilesToConsume > 0);

          curCoord = pageTable.setImageWrappedRange(
              curCoord.first, curCoord.second, tilesToConsume * pageSize, memId,
              memoryOffsetInTiles * pageSize, singlePage, !skip);

          // consume the number of tiles from the region and range
          i += tilesToConsume;
          curRelativeRangeOffset += tilesToConsume;

          // if we've consumed everything in the current range, move to the next one
          if(curRelativeRangeOffset >= rangeSize)
          {
            curRange++;
            curRelativeRangeOffset = 0;

            if(curRange < NumRanges)
            {
              rangeBaseOffset = RANGE_OFFSET(curRange);
              rangeFlags = RANGE_FLAGS(curRange);
              rangeSize = RANGE_SIZE(curRange);

              skip = false;
              if(rangeFlags & D3D12_TILE_RANGE_FLAG_SKIP)
                skip = true;

              memId = memoryId;
              if(rangeFlags & D3D12_TILE_RANGE_FLAG_NULL)
                memId = ResourceId();

              memoryOffsetInTiles = rangeBaseOffset + curRelativeRangeOffset;
              singlePage = false;
              if(rangeFlags & D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE)
                singlePage = true;
            }
          }
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_CopyTileMappings(
    SerialiserType &ser, ID3D12Resource *pDstResource,
    const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource,
    const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(pDstResource).Important();
  SERIALISE_ELEMENT_LOCAL(DstRegionStartCoordinate, *pDstRegionStartCoordinate);
  SERIALISE_ELEMENT(pSrcResource).Important();
  SERIALISE_ELEMENT_LOCAL(SrcRegionStartCoordinate, *pSrcRegionStartCoordinate);
  SERIALISE_ELEMENT_LOCAL(RegionSize, *pRegionSize);
  SERIALISE_ELEMENT(Flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State))
      m_SparseBindResources.insert(GetResID(pDstResource));

    // don't replay with NO_HAZARD
    m_pReal->CopyTileMappings(Unwrap(pDstResource), &DstRegionStartCoordinate, Unwrap(pSrcResource),
                              &SrcRegionStartCoordinate, &RegionSize,
                              Flags & ~D3D12_TILE_MAPPING_FLAG_NO_HAZARD);
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::CopyTileMappings(
    ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
    ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
  SERIALISE_TIME_CALL(m_pReal->CopyTileMappings(Unwrap(pDstResource), pDstRegionStartCoordinate,
                                                Unwrap(pSrcResource), pSrcRegionStartCoordinate,
                                                pRegionSize, Flags));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_CopyTileMappings);
    Serialise_CopyTileMappings(ser, pDstResource, pDstRegionStartCoordinate, pSrcResource,
                               pSrcRegionStartCoordinate, pRegionSize, Flags);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDstResource), eFrameRef_Read);
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pSrcResource), eFrameRef_Read);
  }

  // update our internal page tables
  if(IsCaptureMode(m_State))
  {
    Sparse::PageTable tmp;

    // if we're moving within a subresource the regions can overlap. Take a temporary copy for the
    // source
    if(pSrcResource == pDstResource)
      tmp = *GetRecord(pSrcResource)->sparseTable;

    const Sparse::PageTable &srcPageTable =
        (pSrcResource == pDstResource) ? tmp : *GetRecord(pSrcResource)->sparseTable;
    Sparse::PageTable &dstPageTable = *GetRecord(pDstResource)->sparseTable;

    const UINT srcSub = pSrcRegionStartCoordinate->Subresource;
    const UINT dstSub = pDstRegionStartCoordinate->Subresource;

    if(pRegionSize->UseBox)
    {
      D3D12_TILE_REGION_SIZE size = *pRegionSize;

      if(pRegionSize->Width == 0)
        return;

      // clamp inputs that may be invalid for buffers or 2D to sensible values
      size.Width = RDCCLAMP(1U, pRegionSize->Width, dstPageTable.getResourceSize().x);
      size.Height =
          (uint16_t)RDCCLAMP(1U, (uint32_t)pRegionSize->Height, dstPageTable.getResourceSize().y);
      size.Depth =
          (uint16_t)RDCCLAMP(1U, (uint32_t)pRegionSize->Depth, dstPageTable.getResourceSize().z);

      dstPageTable.copyImageBoxRange(
          dstSub,
          {pDstRegionStartCoordinate->X, pDstRegionStartCoordinate->Y, pDstRegionStartCoordinate->Z},
          {size.Width, size.Height, size.Depth}, srcPageTable, srcSub,
          {pSrcRegionStartCoordinate->X, pSrcRegionStartCoordinate->Y, pSrcRegionStartCoordinate->Z});
    }
    else
    {
      dstPageTable.copyImageWrappedRange(
          dstSub,
          {pDstRegionStartCoordinate->X, pDstRegionStartCoordinate->Y, pDstRegionStartCoordinate->Z},
          pRegionSize->NumTiles * 64 * 1024, srcPageTable, srcSub,
          {pSrcRegionStartCoordinate->X, pSrcRegionStartCoordinate->Y, pSrcRegionStartCoordinate->Z});
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists(SerialiserType &ser,
                                                              UINT NumCommandLists,
                                                              ID3D12CommandList *const *ppCommandLists)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(NumCommandLists).Important();
  SERIALISE_ELEMENT_ARRAY(ppCommandLists, NumCommandLists);

  {
    rdcarray<DebugMessage> DebugMessages;

    if(ser.IsWriting())
      DebugMessages = m_pDevice->GetDebugMessages();

    SERIALISE_ELEMENT(DebugMessages);

    if(ser.IsReading() && IsLoading(m_State))
    {
      // if we're using replay-time API validation, ignore messages from capture time
      if(m_pDevice->GetReplayOptions().apiValidation)
        DebugMessages.clear();

      for(const DebugMessage &msg : DebugMessages)
        m_Cmd.m_EventMessages.push_back(msg);
    }
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12CommandQueue *real = Unwrap(pQueue);

    m_pDevice->DataUploadSync();

    if(m_PrevQueueId != GetResID(pQueue))
    {
      RDCDEBUG("Previous queue execution was on queue %s, now executing %s, syncing GPU",
               ToStr(GetResourceManager()->GetOriginalID(m_PrevQueueId)).c_str(),
               ToStr(GetResourceManager()->GetOriginalID(GetResID(pQueue))).c_str());
      if(m_PrevQueueId != ResourceId())
        m_pDevice->GPUSync(GetResourceManager()->GetCurrentAs<ID3D12CommandQueue>(m_PrevQueueId));

      m_PrevQueueId = GetResID(pQueue);
    }

    if(IsLoading(m_State))
    {
      m_Cmd.AddEvent();

      // we're adding multiple events, need to increment ourselves
      m_Cmd.m_RootEventID++;

      for(uint32_t i = 0; i < NumCommandLists; i++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[i]));

        ID3D12CommandList *list = Unwrap(ppCommandLists[i]);
        real->ExecuteCommandLists(1, &list);
        if(D3D12_Debug_SingleSubmitFlushing())
          m_pDevice->GPUSync();

        BakedCmdListInfo &info = m_Cmd.m_BakedCmdListInfo[cmd];

        if(!info.executeEvents.empty())
        {
          // ensure all GPU work has finished for readback of arguments
          m_pDevice->GPUSync();

          if(m_pDevice->HasFatalError())
            return false;

          // readback the patch buffer and update recorded events
          for(size_t c = 0; c < info.executeEvents.size(); c++)
            m_ReplayList->FinaliseExecuteIndirectEvents(info, info.executeEvents[c]);
        }
      }

      for(uint32_t i = 0; i < NumCommandLists; i++)
      {
        ResourceId cmd = GetResID(ppCommandLists[i]);
        m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[cmd].barriers);
      }

      rdcstr basename = StringFormat::Fmt("ExecuteCommandLists(%u)", NumCommandLists);

      for(uint32_t c = 0; c < NumCommandLists; c++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[c]));

        BakedCmdListInfo &cmdListInfo = m_Cmd.m_BakedCmdListInfo[cmd];

        // add a fake marker
        ActionDescription action;
        {
          action.customName =
              StringFormat::Fmt("=> %s[%u]: Reset(%s)", basename.c_str(), c, ToStr(cmd).c_str());
          action.flags = ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary |
                         ActionFlags::BeginPass;
          m_Cmd.AddEvent();

          m_Cmd.m_RootEvents.back().chunkIndex = cmdListInfo.beginChunk;
          m_Cmd.m_Events.back().chunkIndex = cmdListInfo.beginChunk;

          m_Cmd.AddAction(action);
          m_Cmd.m_RootEventID++;
        }

        // insert the baked command list in-line into this list of notes, assigning new event and
        // drawIDs
        m_Cmd.InsertActionsAndRefreshIDs(cmd, cmdListInfo.action->children);

        for(size_t e = 0; e < cmdListInfo.action->executedCmds.size(); e++)
        {
          rdcarray<uint32_t> &submits = m_Cmd.m_Partial[D3D12CommandData::Secondary]
                                            .cmdListExecs[cmdListInfo.action->executedCmds[e]];

          for(size_t s = 0; s < submits.size(); s++)
            submits[s] += m_Cmd.m_RootEventID;
        }

        for(size_t i = 0; i < cmdListInfo.debugMessages.size(); i++)
        {
          DebugMessage msg = cmdListInfo.debugMessages[i];
          msg.eventId += m_Cmd.m_RootEventID;
          m_pDevice->AddDebugMessage(msg);
        }

        // only primary command lists can be submitted
        m_Cmd.m_Partial[D3D12CommandData::Primary].cmdListExecs[cmd].push_back(m_Cmd.m_RootEventID);

        // pull in any remaining events on the command buffer that weren't added to an action
        for(size_t e = 0; e < cmdListInfo.curEvents.size(); e++)
        {
          APIEvent apievent = cmdListInfo.curEvents[e];
          apievent.eventId += m_Cmd.m_RootEventID;

          m_Cmd.m_RootEvents.push_back(apievent);
          m_Cmd.m_Events.resize_for_index(apievent.eventId);
          m_Cmd.m_Events[apievent.eventId] = apievent;
        }

        for(auto it = cmdListInfo.resourceUsage.begin(); it != cmdListInfo.resourceUsage.end(); ++it)
        {
          EventUsage u = it->second;
          u.eventId += m_Cmd.m_RootEventID;
          m_Cmd.m_ResourceUses[it->first].push_back(u);
        }

        m_Cmd.m_RootEventID += cmdListInfo.eventCount;
        m_Cmd.m_RootActionID += cmdListInfo.actionCount;

        {
          action.customName =
              StringFormat::Fmt("=> %s[%u]: Close(%s)", basename.c_str(), c, ToStr(cmd).c_str());
          action.flags =
              ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary | ActionFlags::EndPass;
          m_Cmd.AddEvent();

          m_Cmd.m_RootEvents.back().chunkIndex = cmdListInfo.endChunk;
          m_Cmd.m_Events.back().chunkIndex = cmdListInfo.endChunk;

          m_Cmd.AddAction(action);
          m_Cmd.m_RootEventID++;
        }
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

        m_Cmd.m_RootEventID += m_Cmd.m_BakedCmdListInfo[cmd].eventCount;
        m_Cmd.m_RootActionID += m_Cmd.m_BakedCmdListInfo[cmd].actionCount;

        // 2 extra for the virtual labels around the command list
        {
          m_Cmd.m_RootEventID += 2;
          m_Cmd.m_RootActionID += 2;
        }
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
      else
      {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
        RDCDEBUG("Queue Submit re-recording from %u", m_Cmd.m_RootEventID);
#endif

        uint32_t eid = startEID;

        rdcarray<ID3D12CommandList *> rerecordedCmds;

        for(uint32_t c = 0; c < NumCommandLists; c++)
        {
          ResourceId cmdId = GetResourceManager()->GetOriginalID(GetResID(ppCommandLists[c]));

          // account for the virtual label at the start of the events here
          // so it matches up to baseEvent
          {
            eid++;
          }

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          uint32_t end = eid + m_Cmd.m_BakedCmdListInfo[cmdId].eventCount;
#endif

          if(eid <= m_Cmd.m_LastEventID)
          {
            ID3D12CommandList *cmd = m_Cmd.RerecordCmdList(cmdId);
            ResourceId rerecord = GetResID(cmd);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Queue submit re-recorded replay of %s, using %s (%u -> %u <= %u)",
                     ToStr(cmdId).c_str(), ToStr(rerecord).c_str(), eid, end, m_Cmd.m_LastEventID);
#endif
            rerecordedCmds.push_back(Unwrap(cmd));

            m_pDevice->ApplyBarriers(m_Cmd.m_BakedCmdListInfo[rerecord].barriers);
          }
          else
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Queue not submitting %s", ToStr(cmdId).c_str());
#endif
          }

          eid += m_Cmd.m_BakedCmdListInfo[cmdId].eventCount;

          // 1 extra to account for the virtual end command list label (begin is accounted for
          // above)
          {
            eid++;
          }
        }

        if(D3D12_Debug_SingleSubmitFlushing())
        {
          for(size_t i = 0; i < rerecordedCmds.size(); i++)
          {
            real->ExecuteCommandLists(1, &rerecordedCmds[i]);
            m_pDevice->GPUSync();
          }
        }
        else
        {
          real->ExecuteCommandLists((UINT)rerecordedCmds.size(), &rerecordedCmds[0]);
        }
      }
    }
  }

  return true;
}

ID3D12Fence *WrappedID3D12CommandQueue::GetRayFence()
{
  // if we don't have a fence for this queue tracking, create it now
  if(!m_RayFence)
  {
    // create this unwrapped so that it doesn't get recorded into captures
    m_pDevice->GetReal()->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                      (void **)&m_RayFence);
    m_RayFence->SetName(L"Queue Ray Fence");
  }

  return m_RayFence;
}

void WrappedID3D12CommandQueue::ExecuteCommandLists(UINT NumCommandLists,
                                                    ID3D12CommandList *const *ppCommandLists)
{
  if(m_pDevice->HasFatalError())
    return;
  ExecuteCommandListsInternal(NumCommandLists, ppCommandLists, false, false);
}

void WrappedID3D12CommandQueue::ExecuteCommandListsInternal(UINT NumCommandLists,
                                                            ID3D12CommandList *const *ppCommandLists,
                                                            bool InFrameCaptureBoundary,
                                                            bool SkipRealExecute)
{
  ID3D12CommandList **unwrapped = m_pDevice->GetTempArray<ID3D12CommandList *>(NumCommandLists);
  for(UINT i = 0; i < NumCommandLists; i++)
    unwrapped[i] = Unwrap(ppCommandLists[i]);

  if(!m_MarkedActive)
  {
    m_MarkedActive = true;
    RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D12, false);
  }

  if(IsActiveCapturing(m_State))
    m_pDevice->AddCaptureSubmission();

  if(!SkipRealExecute)
  {
    SERIALISE_TIME_CALL(m_pReal->ExecuteCommandLists(NumCommandLists, unwrapped));

    rdcarray<std::function<bool()>> pendingASBuildCallbacks;

    for(UINT i = 0; i < NumCommandLists; i++)
    {
      WrappedID3D12GraphicsCommandList *wrapped =
          (WrappedID3D12GraphicsCommandList *)(ppCommandLists[i]);

      if(!wrapped->ExecuteImmediateASBuildCallbacks())
      {
        RDCERR("Unable to execute post build for acc struct");
      }

      wrapped->TakeWaitingASBuildCallbacks(pendingASBuildCallbacks);
    }

    if(!pendingASBuildCallbacks.empty())
    {
      ID3D12Fence *fence = GetRayFence();

      // these callbacks need to be synchronised at every submission to process them as soon as the
      // results are available, since we could submit a build on one queue and then a dependent
      // build on another queue later once it's finished without any intermediate submissions on the
      // first queue. For that reason we pass these to the RT handler to hold onto, and tick it
      GetResourceManager()->GetRaytracingResourceAndUtilHandler()->AddPendingASBuilds(
          fence, m_RayFenceValue, pendingASBuildCallbacks);

      // add the signal for those callbacks to wait on
      HRESULT hr = m_pReal->Signal(fence, m_RayFenceValue++);
      m_pDevice->CheckHRESULT(hr);
      RDCASSERTEQUAL(hr, S_OK);
    }

    // check AS builds now
    GetResourceManager()->GetRaytracingResourceAndUtilHandler()->CheckPendingASBuilds();
  }

  if(IsCaptureMode(m_State))
  {
    CheckAndFreeRayDispatches();

    rdcarray<PatchedRayDispatch::Resources> rayDispatches;

    if(!InFrameCaptureBoundary)
      m_pDevice->GetCapTransitionLock().ReadLock();

    m_Lock.Lock();

    bool capframe = IsActiveCapturing(m_State);
    std::unordered_set<ResourceId> refdIDs;

    for(UINT i = 0; i < NumCommandLists; i++)
    {
      WrappedID3D12GraphicsCommandList *wrapped =
          (WrappedID3D12GraphicsCommandList *)ppCommandLists[i];
      D3D12ResourceRecord *record = GetRecord(ppCommandLists[i]);

      if(record->ContainsExecuteIndirect)
        m_QueueRecord->ContainsExecuteIndirect = true;

      m_pDevice->ApplyBarriers(record->bakedCommands->cmdInfo->barriers);

      wrapped->AddRayDispatches(rayDispatches);

      // need to lock the whole section of code, not just the check on
      // m_State, as we also need to make sure we don't check the state,
      // start marking dirty resources then while we're doing so the
      // state becomes capframe.
      // the next sections where we mark resources referenced and add
      // the submit chunk to the frame record don't have to be protected.
      // Only the decision of whether we're inframe or not, and marking
      // dirty.

      for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
          it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
        GetResourceManager()->MarkDirtyResource(*it);

      if(capframe)
      {
        // any descriptor copies or writes could reference new resources not in the
        // bound descs list yet. So we take all of those referenced descriptors and
        // include them to see if we need to flush
        rdcarray<D3D12Descriptor> dynDescRefs;
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
          rdcpair<D3D12Descriptor *, UINT> &descRange = *it;
          WrappedID3D12DescriptorHeap *heap = descRange.first->GetHeap();
          D3D12Descriptor *end = heap->GetDescriptors() + heap->GetNumDescriptors();
          for(UINT d = 0; d < descRange.second; ++d)
          {
            D3D12Descriptor *desc = descRange.first + d;

            if(desc >= end)
              break;

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
        }

        // pull in frame refs from this baked command list
        record->bakedCommands->AddResourceReferences(GetResourceManager());
        record->bakedCommands->AddReferencedIDs(refdIDs);

        // mark the creation record as referenced so it gets pulled in.
        GetResourceManager()->MarkResourceFrameReferenced(
            wrapped->GetCreationRecord()->GetResourceID(), eFrameRef_Read);

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

        m_CmdListAllocators.push_back(record->bakedCommands->cmdInfo->allocRecord);
        record->bakedCommands->cmdInfo->allocRecord->AddRef();

        record->bakedCommands->AddRef();
      }

      record->cmdInfo->dirtied.clear();
    }

    if(!rayDispatches.empty())
    {
      for(PatchedRayDispatch::Resources &ray : rayDispatches)
        ray.fenceValue = m_RayFenceValue;

      m_RayDispatchesPending.append(rayDispatches);

      HRESULT hr = m_pReal->Signal(GetRayFence(), m_RayFenceValue++);
      m_pDevice->CheckHRESULT(hr);
      RDCASSERTEQUAL(hr, S_OK);
    }

    if(capframe)
    {
      rdcarray<MapState> maps = m_pDevice->GetMaps();

      // get the Mappable referenced IDs. With the case of placed resources the resource that's
      // mapped may not be the one that was bound but they may overlap, so we use the heap as
      // reference for non-committed resource.
      std::unordered_set<ResourceId> mappableIDs;
      WrappedID3D12Resource::GetMappableIDs(GetResourceManager(), refdIDs, mappableIDs);

      for(auto it = maps.begin(); it != maps.end(); ++it)
      {
        WrappedID3D12Resource *res = GetWrapped(it->res);
        UINT subres = it->subres;
        size_t size = (size_t)it->totalSize;

        // only need to flush memory that could affect this submitted batch of work
        if(mappableIDs.find(res->GetMappableID()) == mappableIDs.end())
        {
          RDCDEBUG("Map of memory %s (mappable ID %s) not referenced in this queue - not flushing",
                   ToStr(res->GetResourceID()).c_str(), ToStr(res->GetMappableID()).c_str());
          continue;
        }

        // prevent this resource from being mapped or unmapped on another thread while we're
        // checking it. If it's unmapped subsequently we'll maybe redundantly detect the changes
        // here AND serialise them there, but we'll play it safe.
        res->LockMaps();

        size_t diffStart = 0, diffEnd = 0;
        bool found = true;

        byte *ref = res->GetShadow(subres);
        byte *data = res->GetMap(subres);

        // check we actually have map data. It's possible that over the course of the loop
        // iteration
        // the resource has been unmapped on another thread before we got here.
        if(data)
        {
          QueueReadbackData &queueReadback = m_pDevice->GetQueueReadbackData();

          D3D12_HEAP_PROPERTIES heapProps;
          res->GetHeapProperties(&heapProps, NULL);

          if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD ||
             heapProps.CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE)
          {
            RDCLOG("Doing GPU readback of mapped memory");

            queueReadback.lock.Lock();

            queueReadback.Resize(size);

            queueReadback.list->Reset(queueReadback.alloc, NULL);
            queueReadback.list->CopyBufferRegion(queueReadback.readbackBuf, 0, res, 0, size);
            queueReadback.list->Close();
            ID3D12CommandList *listptr = Unwrap(queueReadback.list);
            queueReadback.unwrappedQueue->ExecuteCommandLists(1, &listptr);
            m_pDevice->GPUSync(queueReadback.unwrappedQueue, Unwrap(queueReadback.fence));

            data = queueReadback.readbackMapped;
          }

          if(ref)
            found = FindDiffRange(data, ref, size, diffStart, diffEnd);
          else
            diffEnd = size;

          if(found)
          {
            RDCLOG("Persistent map flush forced for %s (%llu -> %llu)",
                   ToStr(res->GetResourceID()).c_str(), (uint64_t)diffStart, (uint64_t)diffEnd);

            D3D12_RANGE range = {diffStart, diffEnd};

            if(ref == NULL)
            {
              res->AllocShadow(subres, size);

              ref = res->GetShadow(subres);
            }

            // passing true here asks the serialisation function to update the shadow pointer for
            // this resource
            m_pDevice->MapDataWrite(res, subres, data, range, true);

            GetResourceManager()->MarkDirtyResource(res->GetResourceID());
          }
          else
          {
            RDCDEBUG("Persistent map flush not needed for %s", ToStr(res->GetResourceID()).c_str());
          }

          if(data == queueReadback.readbackMapped)
            queueReadback.lock.Unlock();
        }

        res->UnlockMaps();
      }

      std::unordered_set<ResourceId> sparsePageHeaps;
      std::unordered_set<ResourceId> sparseResources;

      // this returns the list of current live sparse resources, and the list of heaps *that have
      // ever been used for sparse binding*. The latter list may be way too big, in which case we
      // look at the referenced sparse resources and pull in the heaps they are currently using.
      // However many applications may use only a few large heaps for sparse binding so if the
      // list
      // is small enough then we just use it directly even if technically some heaps may not be
      // used
      // by any resources we are referencing.
      m_pDevice->GetSparseResources(sparseResources, sparsePageHeaps);

      if(sparsePageHeaps.size() > refdIDs.size() || sparsePageHeaps.size() > sparseResources.size())
      {
        // intersect sparse resources with ref'd IDs, and pull in the referenced heaps from its
        // current page table
        const std::unordered_set<ResourceId> &smaller =
            sparseResources.size() < refdIDs.size() ? sparseResources : refdIDs;
        const std::unordered_set<ResourceId> &larger =
            sparseResources.size() >= refdIDs.size() ? sparseResources : refdIDs;
        for(const ResourceId id : smaller)
        {
          if(larger.find(id) != larger.end())
          {
            D3D12ResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
            RDCASSERT(record->sparseTable);

            const Sparse::PageTable &table = *record->sparseTable;

            for(uint32_t sub = 0; sub < RDCMAX(1U, table.getNumSubresources());)
            {
              const Sparse::PageRangeMapping &mapping = table.isSubresourceInMipTail(sub)
                                                            ? table.getMipTailMapping(sub)
                                                            : table.getSubresource(sub);

              if(mapping.hasSingleMapping())
              {
                if(mapping.singleMapping.memory != ResourceId())
                  sparsePageHeaps.insert(mapping.singleMapping.memory);
              }
              else
              {
                // this is a huge perf cliff as we've lost any batching and we perform as badly as
                // if every page was mapped to a different resource, so we hope applications don't
                // hit this often.
                for(const Sparse::Page &page : mapping.pages)
                {
                  sparsePageHeaps.insert(page.memory);
                }
              }

              if(table.isSubresourceInMipTail(sub))
              {
                // move to the next subresource after the miptail, since we handle the miptail all
                // at once
                sub = ((sub / table.getMipCount()) + 1) * table.getMipCount();
              }
              else
              {
                sub++;
              }
            }
          }
        }
      }

      for(const ResourceId id : sparsePageHeaps)
        GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);

      {
        WriteSerialiser &ser = GetThreadSerialiser();
        ser.SetActionChunk();
        SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_ExecuteCommandLists);
        Serialise_ExecuteCommandLists(ser, NumCommandLists, ppCommandLists);

        m_QueueRecord->AddChunk(scope.Get());
      }
    }

    m_Lock.Unlock();

    if(!InFrameCaptureBoundary)
      m_pDevice->GetCapTransitionLock().ReadUnlock();
  }
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_SetMarker(SerialiserType &ser, UINT Metadata,
                                                    const void *pData, UINT Size)
{
  rdcstr MarkerText = "";
  uint64_t Color = 0;

  if(ser.IsWriting() && pData && Size)
    MarkerText = DecodeMarkerString(Metadata, pData, Size, Color);

  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(MarkerText).Important();
  if(ser.VersionAtLeast(0xD))
  {
    SERIALISE_ELEMENT(Color);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12MarkerRegion::Set(m_pReal, MarkerText);

    if(IsLoading(m_State))
    {
      ActionDescription action;
      action.customName = MarkerText;
      if(Color != 0)
      {
        action.markerColor = DecodePIXColor(Color);
      }
      action.flags |= ActionFlags::SetMarker;

      m_Cmd.AddEvent();
      m_Cmd.AddAction(action);
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::SetMarker(UINT Metadata, const void *pData,
                                                            UINT Size)
{
  SERIALISE_TIME_CALL(m_pReal->SetMarker(Metadata, pData, Size));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_SetMarker);
    Serialise_SetMarker(ser, Metadata, pData, Size);

    m_QueueRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_BeginEvent(SerialiserType &ser, UINT Metadata,
                                                     const void *pData, UINT Size)
{
  rdcstr MarkerText = "";
  uint64_t Color = 0;

  if(ser.IsWriting() && pData && Size)
    MarkerText = DecodeMarkerString(Metadata, pData, Size, Color);

  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(MarkerText).Important();
  if(ser.VersionAtLeast(0xD))
  {
    SERIALISE_ELEMENT(Color);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12MarkerRegion::Begin(m_pReal, MarkerText);

    if(IsLoading(m_State))
    {
      ActionDescription action;
      action.customName = MarkerText;
      if(Color != 0)
      {
        action.markerColor = DecodePIXColor(Color);
      }
      action.flags |= ActionFlags::PushMarker;

      m_Cmd.AddEvent();
      m_Cmd.AddAction(action);

      // now push the action stack
      m_Cmd.GetActionStack().push_back(&m_Cmd.GetActionStack().back()->children.back());
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::BeginEvent(UINT Metadata, const void *pData,
                                                             UINT Size)
{
  SERIALISE_TIME_CALL(m_pReal->BeginEvent(Metadata, pData, Size));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_BeginEvent);
    Serialise_BeginEvent(ser, Metadata, pData, Size);

    m_QueueRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_EndEvent(SerialiserType &ser)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12MarkerRegion::End(m_pReal);

    if(IsLoading(m_State))
    {
      ActionDescription action;
      action.flags |= ActionFlags::PopMarker;

      m_Cmd.AddEvent();
      m_Cmd.AddAction(action);

      if(m_Cmd.GetActionStack().size() > 1)
        m_Cmd.GetActionStack().pop_back();
    }
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::EndEvent()
{
  SERIALISE_TIME_CALL(m_pReal->EndEvent());

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_EndEvent);
    Serialise_EndEvent(ser);

    m_QueueRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_Signal(SerialiserType &ser, ID3D12Fence *pFence,
                                                 UINT64 Value)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(pFence).Important();
  SERIALISE_ELEMENT(Value).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pFence)
  {
    m_pReal->Signal(Unwrap(pFence), Value);
    m_pDevice->GPUSync(pQueue);
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pReal->Signal(Unwrap(pFence), Value));

  if(IsActiveCapturing(m_State))
  {
    SCOPED_LOCK(m_Lock);

    WriteSerialiser &ser = GetThreadSerialiser();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_Signal);
    Serialise_Signal(ser, pFence, Value);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFence), eFrameRef_Read);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12CommandQueue::Serialise_Wait(SerialiserType &ser, ID3D12Fence *pFence, UINT64 Value)
{
  ID3D12CommandQueue *pQueue = this;
  SERIALISE_ELEMENT(pQueue);
  SERIALISE_ELEMENT(pFence).Important();
  SERIALISE_ELEMENT(Value).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pFence)
  {
    m_pDevice->GPUSync(pQueue);
  }

  return true;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pReal->Wait(Unwrap(pFence), Value));

  if(IsActiveCapturing(m_State))
  {
    SCOPED_LOCK(m_Lock);

    WriteSerialiser &ser = GetThreadSerialiser();
    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Queue_Wait);
    Serialise_Wait(ser, pFence, Value);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFence), eFrameRef_Read);
  }

  return ret;
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

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Present(
    _In_ ID3D12GraphicsCommandList *pOpenCommandList, _In_ ID3D12Resource *pSourceTex2D,
    _In_ HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags)
{
  // D3D12 on windows 7
  if(!RenderDoc::Inst().GetCaptureOptions().allowVSync)
  {
    Flags = D3D12_DOWNLEVEL_PRESENT_FLAG_NONE;
  }

  // store the timestamp, thread ID etc. Don't store the duration
  SERIALISE_TIME_CALL();

  if(IsCaptureMode(m_State))
  {
    WrappedID3D12GraphicsCommandList *list = (WrappedID3D12GraphicsCommandList *)pOpenCommandList;

    // add a marker
    D3D12MarkerRegion::Set(list, "ID3D12CommandQueueDownlevel::Present()");

    // the list is implicitly closed, serialise that
    D3D12ResourceRecord *listRecord = GetRecord(list);

    {
      CACHE_THREAD_SERIALISER();
      ser.SetActionChunk();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::List_Close);
      list->Serialise_Close(ser);

      listRecord->AddChunk(scope.Get());
    }

    listRecord->Bake();

    // this queue implicitly submits the list, serialise that
    ID3D12CommandList *submitlist = list;
    ExecuteCommandListsInternal(1, &submitlist, false, true);
  }

  if(m_pPresentHWND != NULL)
  {
    // don't let the device actually release any refs on the resource, just make it release
    // internal
    // resources
    m_pPresentSource->AddRef();
    m_pDevice->ReleaseSwapchainResources(this, 0, NULL, NULL);
  }

  if(m_pPresentHWND != hWindow)
  {
    if(m_pPresentHWND != NULL)
    {
      Keyboard::RemoveInputWindow(WindowingSystem::Win32, m_pPresentHWND);
      RenderDoc::Inst().RemoveFrameCapturer(
          DeviceOwnedWindow(m_pDevice->GetFrameCapturerDevice(), m_pPresentHWND));
    }

    Keyboard::AddInputWindow(WindowingSystem::Win32, hWindow);

    RenderDoc::Inst().AddFrameCapturer(
        DeviceOwnedWindow(m_pDevice->GetFrameCapturerDevice(), hWindow),
        m_pDevice->GetFrameCapturer());
  }

  m_pPresentSource = pSourceTex2D;
  m_pPresentHWND = hWindow;

  m_pDevice->WrapSwapchainBuffer(this, GetFormat(), 0, m_pPresentSource);

  m_pDevice->Present(pOpenCommandList, this,
                     Flags == D3D12_DOWNLEVEL_PRESENT_FLAG_WAIT_FOR_VBLANK ? 1 : 0, 0);

  return m_pDownlevel->Present(Unwrap(pOpenCommandList), Unwrap(pSourceTex2D), hWindow, Flags);
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
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, SetMarker, UINT Metadata,
                                const void *pData, UINT Size);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, BeginEvent, UINT Metadata,
                                const void *pData, UINT Size);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, EndEvent);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, Wait, ID3D12Fence *pFence,
                                UINT64 Value);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, Signal, ID3D12Fence *pFence,
                                UINT64 Value);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12CommandQueue, Wait, ID3D12Fence *pFence,
                                UINT64 Value);
