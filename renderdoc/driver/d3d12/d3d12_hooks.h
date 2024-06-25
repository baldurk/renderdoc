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
#include "driver/dx/official/d3d12.h"
#include "driver/dx/official/dxgi.h"

struct D3D12DevConfiguration;

// this is the type of the lambda we use to route the call out to the 'real' function inside our
// generic wrapper.
// Could be any of D3D12CreateDevice or the AMD wrapper
typedef std::function<HRESULT(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel,
                              REFIID riid, void **ppDevice)>
    RealD3D12CreateFunction;

HRESULT CreateD3D12_Internal(RealD3D12CreateFunction real, D3D12DevConfiguration *devConfig,
                             IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                             void **ppDevice);

struct ID3DDevice;

ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev);
