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

#pragma once

#include "common/wrapped_pool.h"
#include "d3d12_common.h"

class WrappedID3D12CommandQueue : public RefCounter12<ID3D12CommandQueue>, public ID3D12CommandQueue
{
  ID3D12CommandQueue *m_pReal;
  WrappedID3D12Device *m_pDevice;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandQueue);

  WrappedID3D12CommandQueue(ID3D12CommandQueue *real, WrappedID3D12Device *device,
                            Serialiser *serialiser);
  virtual ~WrappedID3D12CommandQueue();

  WrappedID3D12Device *GetWrappedDevice() { return m_pDevice; }
  //////////////////////////////
  // implement ID3D12CommandQueue

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions,
                         const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
                         const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap,
                         UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags,
                         const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts,
                         D3D12_TILE_MAPPING_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyTileMappings(ID3D12Resource *pDstResource,
                       const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
                       ID3D12Resource *pSrcResource,
                       const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
                       const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ExecuteCommandLists(UINT NumCommandLists,
                                                    ID3D12CommandList *const *ppCommandLists));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetMarker(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                BeginEvent(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndEvent());

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Signal(ID3D12Fence *pFence, UINT64 Value));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Wait(ID3D12Fence *pFence, UINT64 Value));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetTimestampFrequency(UINT64 *pFrequency));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp));

  virtual D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
};