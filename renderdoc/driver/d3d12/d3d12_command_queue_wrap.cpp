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
  m_pReal->UpdateTileMappings(pResource, NumResourceRegions, pResourceRegionStartCoordinates,
                              pResourceRegionSizes, pHeap, NumRanges, pRangeFlags,
                              pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::CopyTileMappings(
    ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
    ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
  m_pReal->CopyTileMappings(pDstResource, pDstRegionStartCoordinate, pSrcResource,
                            pSrcRegionStartCoordinate, pRegionSize, Flags);
}

void STDMETHODCALLTYPE WrappedID3D12CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
  ID3D12CommandList **unwrapped = new ID3D12CommandList *[NumCommandLists];
  for(UINT i = 0; i < NumCommandLists; i++)
    unwrapped[i] = Unwrap(ppCommandLists[i]);

  m_pReal->ExecuteCommandLists(NumCommandLists, unwrapped);

  SAFE_DELETE_ARRAY(unwrapped);
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

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
  return m_pReal->Signal(pFence, Value);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
  return m_pReal->Wait(pFence, Value);
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