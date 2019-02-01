/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2019 Baldur Karlsson
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

#include "d3d11_context.h"
#include "d3d11_device.h"

/////////////////////////////////
// implement ID3D11DeviceContext2

HRESULT WrappedID3D11DeviceContext::UpdateTileMappings(
    ID3D11Resource *pTiledResource, UINT NumTiledResourceRegions,
    const D3D11_TILED_RESOURCE_COORDINATE *pTiledResourceRegionStartCoordinates,
    const D3D11_TILE_REGION_SIZE *pTiledResourceRegionSizes, ID3D11Buffer *pTilePool,
    UINT NumRanges, const UINT *pRangeFlags, const UINT *pTilePoolStartOffsets,
    const UINT *pRangeTileCounts, UINT Flags)
{
  RDCUNIMPLEMENTED(
      "Tiled resources are not yet supported. Please contact me if you have a working example!");

  if(m_pRealContext2 == NULL)
    return E_NOINTERFACE;

  return m_pRealContext2->UpdateTileMappings(
      pTiledResource, NumTiledResourceRegions, pTiledResourceRegionStartCoordinates,
      pTiledResourceRegionSizes, pTilePool, NumRanges, pRangeFlags, pTilePoolStartOffsets,
      pRangeTileCounts, Flags);
}

HRESULT WrappedID3D11DeviceContext::CopyTileMappings(
    ID3D11Resource *pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE *pDestRegionStartCoordinate,
    ID3D11Resource *pSourceTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE *pSourceRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE *pTileRegionSize, UINT Flags)
{
  RDCUNIMPLEMENTED(
      "Tiled resources are not yet supported. Please contact me if you have a working example!");

  if(m_pRealContext2 == NULL)
    return E_NOINTERFACE;

  return m_pRealContext2->CopyTileMappings(pDestTiledResource, pDestRegionStartCoordinate,
                                           pSourceTiledResource, pSourceRegionStartCoordinate,
                                           pTileRegionSize, Flags);
}

void WrappedID3D11DeviceContext::CopyTiles(
    ID3D11Resource *pTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE *pTileRegionSize, ID3D11Buffer *pBuffer,
    UINT64 BufferStartOffsetInBytes, UINT Flags)
{
  RDCUNIMPLEMENTED(
      "Tiled resources are not yet supported. Please contact me if you have a working example!");

  if(m_pRealContext2 == NULL)
    return;

  return m_pRealContext2->CopyTiles(pTiledResource, pTileRegionStartCoordinate, pTileRegionSize,
                                    pBuffer, BufferStartOffsetInBytes, Flags);
}

void WrappedID3D11DeviceContext::UpdateTiles(
    ID3D11Resource *pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE *pDestTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE *pDestTileRegionSize, const void *pSourceTileData, UINT Flags)
{
  RDCUNIMPLEMENTED(
      "Tiled resources are not yet supported. Please contact me if you have a working example!");

  if(m_pRealContext2 == NULL)
    return;

  return m_pRealContext2->UpdateTiles(pDestTiledResource, pDestTileRegionStartCoordinate,
                                      pDestTileRegionSize, pSourceTileData, Flags);
}

HRESULT WrappedID3D11DeviceContext::ResizeTilePool(ID3D11Buffer *pTilePool, UINT64 NewSizeInBytes)
{
  RDCUNIMPLEMENTED(
      "Tiled resources are not yet supported. Please contact me if you have a working example!");

  if(m_pRealContext2 == NULL)
    return E_NOINTERFACE;

  return m_pRealContext2->ResizeTilePool(pTilePool, NewSizeInBytes);
}

void WrappedID3D11DeviceContext::TiledResourceBarrier(
    ID3D11DeviceChild *pTiledResourceOrViewAccessBeforeBarrier,
    ID3D11DeviceChild *pTiledResourceOrViewAccessAfterBarrier)
{
  RDCUNIMPLEMENTED(
      "Tiled resources are not yet supported. Please contact me if you have a working example!");

  if(m_pRealContext2 == NULL)
    return;

  return m_pRealContext2->TiledResourceBarrier(pTiledResourceOrViewAccessBeforeBarrier,
                                               pTiledResourceOrViewAccessAfterBarrier);
}

BOOL WrappedID3D11DeviceContext::IsAnnotationEnabled()
{
  return TRUE;
}

void WrappedID3D11DeviceContext::SetMarkerInt(LPCWSTR pLabel, INT Data)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  SetMarker(0, pLabel);
}

void WrappedID3D11DeviceContext::BeginEventInt(LPCWSTR pLabel, INT Data)
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  PushMarker(0, pLabel);
}

void WrappedID3D11DeviceContext::EndEvent()
{
  SCOPED_LOCK_OPTIONAL(m_pDevice->D3DLock(), m_pDevice->D3DThreadSafe());

  PopMarker();
}
