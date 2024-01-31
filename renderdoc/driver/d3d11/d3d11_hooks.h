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

#include <functional>
#include "driver/dx/official/d3d11.h"
#include "driver/dx/official/dxgi.h"

// this is the type of the lambda we use to route the call out to the 'real' function inside our
// generic wrapper.
// Could be any of D3D11CreateDevice, D3D11CreateDeviceAndSwapChain, or the nvapi equivalents
typedef std::function<HRESULT(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL *,
                              UINT FeatureLevels, UINT, CONST DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **,
                              ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **)>
    RealD3D11CreateFunction;

HRESULT CreateD3D11_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                             D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                             __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                             UINT FeatureLevels, UINT SDKVersion,
                             __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                             __out_opt IDXGISwapChain **ppSwapChain,
                             __out_opt ID3D11Device **ppDevice,
                             __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                             __out_opt ID3D11DeviceContext **ppImmediateContext);

struct ID3DDevice;

ID3DDevice *GetD3D11DeviceIfAlloc(IUnknown *dev);
