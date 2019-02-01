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

#include "d3d11_device.h"
#include "d3d11_context.h"
#include "d3d11_resources.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// ID3D11Device2 interface

void WrappedID3D11Device::GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext)
{
  if(m_pDevice2 == NULL)
    return;

  if(ppImmediateContext)
  {
    m_pImmediateContext->AddRef();
    *ppImmediateContext = (ID3D11DeviceContext2 *)m_pImmediateContext;
  }
}

HRESULT WrappedID3D11Device::CreateDeferredContext2(UINT ContextFlags,
                                                    ID3D11DeviceContext2 **ppDeferredContext)
{
  if(m_pDevice2 == NULL)
    return E_NOINTERFACE;
  if(ppDeferredContext == NULL)
    return m_pDevice2->CreateDeferredContext2(ContextFlags, NULL);

  ID3D11DeviceContext *defCtx = NULL;
  HRESULT ret = CreateDeferredContext(ContextFlags, &defCtx);

  if(SUCCEEDED(ret))
  {
    WrappedID3D11DeviceContext *wrapped = (WrappedID3D11DeviceContext *)defCtx;
    *ppDeferredContext = (ID3D11DeviceContext2 *)wrapped;
  }
  else
  {
    SAFE_RELEASE(defCtx);
  }

  return ret;
}

void WrappedID3D11Device::GetResourceTiling(
    ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource,
    D3D11_PACKED_MIP_DESC *pPackedMipDesc, D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
    UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
  if(m_pDevice2 == NULL)
    return;

  m_pDevice2->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc,
                                pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
                                FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}

HRESULT WrappedID3D11Device::CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount,
                                                            UINT Flags, UINT *pNumQualityLevels)
{
  if(m_pDevice2 == NULL)
    return E_NOINTERFACE;

  return m_pDevice2->CheckMultisampleQualityLevels1(Format, SampleCount, Flags, pNumQualityLevels);
}
