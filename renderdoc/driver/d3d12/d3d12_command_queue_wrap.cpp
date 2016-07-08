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

#include "d3d12_command_queue.h"
#include "d3d12_command_list.h"
#include "d3d12_resources.h"

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::UpdateTileMappings(
    ID3D12Resource *pResource, UINT NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges,
    const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets,
    const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
  m_pReal->UpdateTileMappings(Unwrap(pResource), NumResourceRegions, pResourceRegionStartCoordinates,
                              pResourceRegionSizes, Unwrap(pHeap), NumRanges, pRangeFlags,
                              pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::CopyTileMappings(
    ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
    ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
  m_pReal->CopyTileMappings(Unwrap(pDstResource), pDstRegionStartCoordinate, Unwrap(pSrcResource),
                            pSrcRegionStartCoordinate, pRegionSize, Flags);
}

bool WrappedID3D12CommandQueue::Serialise_ExecuteCommandLists(UINT NumCommandLists,
                                                              ID3D12CommandList *const *ppCommandLists)
{
  SERIALISE_ELEMENT(UINT, num, NumCommandLists);

  std::vector<ResourceId> ids;
  ids.reserve(num);
  for(UINT i = 0; i < num; i++)
    ids.push_back(GetResID(ppCommandLists[i]));

  m_pSerialiser->Serialise("ppCommandLists", ids);

  if(m_State <= EXECUTING)
  {
    std::vector<ID3D12CommandList *> unwrappedLists;
    unwrappedLists.reserve(num);
    for(UINT i = 0; i < num; i++)
      unwrappedLists.push_back(Unwrap(GetResourceManager()->GetLiveAs<ID3D12CommandList>(ids[i])));

    m_pReal->ExecuteCommandLists(num, &unwrappedLists[0]);
  }

  return true;
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
  ID3D12CommandList **unwrapped = new ID3D12CommandList *[NumCommandLists];
  for(UINT i = 0; i < NumCommandLists; i++)
    unwrapped[i] = Unwrap(ppCommandLists[i]);

  m_pReal->ExecuteCommandLists(NumCommandLists, unwrapped);

  SAFE_DELETE_ARRAY(unwrapped);

  bool capframe = false;
  set<ResourceId> refdIDs;

  for(UINT i = 0; i < NumCommandLists; i++)
  {
    D3D12ResourceRecord *record = GetRecord(ppCommandLists[i]);

    m_pDevice->ApplyBarriers(record->bakedCommands->cmdInfo->barriers);

    // need to lock the whole section of code, not just the check on
    // m_State, as we also need to make sure we don't check the state,
    // start marking dirty resources then while we're doing so the
    // state becomes capframe.
    // the next sections where we mark resources referenced and add
    // the submit chunk to the frame record don't have to be protected.
    // Only the decision of whether we're inframe or not, and marking
    // dirty.
    {
      SCOPED_LOCK(m_pDevice->GetCapTransitionLock());
      if(m_State == WRITING_CAPFRAME)
      {
        for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
            it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
          GetResourceManager()->MarkPendingDirty(*it);

        capframe = true;
      }
      else
      {
        for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
            it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
          GetResourceManager()->MarkDirtyResource(*it);
      }
    }

    if(capframe)
    {
      // pull in frame refs from this baked command buffer
      record->bakedCommands->AddResourceReferences(GetResourceManager());
      record->bakedCommands->AddReferencedIDs(refdIDs);

      // ref the parent command buffer by itself, this will pull in the cmd buffer pool
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

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
    // flush coherent maps
    for(UINT i = 0; i < NumCommandLists; i++)
    {
      SCOPED_SERIALISE_CONTEXT(EXECUTE_CMD_LISTS);
      Serialise_ExecuteCommandLists(1, ppCommandLists + i);

      m_QueueRecord->AddChunk(scope.Get());
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

  if(m_State <= EXECUTING)
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
    SCOPED_SERIALISE_CONTEXT(SIGNAL);
    Serialise_Signal(pFence, Value);

    m_QueueRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFence), eFrameRef_Read);
  }

  return m_pReal->Signal(Unwrap(pFence), Value);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
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