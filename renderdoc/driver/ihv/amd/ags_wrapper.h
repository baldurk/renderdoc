/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include <stdint.h>
#include <windows.h>
#include "driver/dx/official/d3dcommon.h"

struct IDXGIAdapter;
struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct DXGI_SWAP_CHAIN_DESC;

MIDL_INTERFACE("947EBFA0-EF82-451F-8E5C-27269A21B8D4")
IAGSD3DDevice : public IUnknown
{
  virtual IUnknown *STDMETHODCALLTYPE GetReal() = 0;
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD space, DWORD reg) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateD3D11(
      IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL *, UINT FeatureLevels,
      UINT, CONST DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *,
      ID3D11DeviceContext **) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateD3D12(
      IUnknown * pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice) = 0;
  virtual BOOL STDMETHODCALLTYPE ExtensionsSupported() = 0;
};

IAGSD3DDevice *InitialiseAGSReplay(uint32_t space, uint32_t reg);
