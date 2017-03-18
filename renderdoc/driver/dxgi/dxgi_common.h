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

#pragma once

#include "api/replay/renderdoc_replay.h"
#include "driver/dx/official/d3dcommon.h"
#include "driver/dx/official/dxgi1_5.h"

ResourceFormat MakeResourceFormat(DXGI_FORMAT fmt);
DXGI_FORMAT MakeDXGIFormat(ResourceFormat fmt);

UINT GetByteSize(int Width, int Height, int Depth, DXGI_FORMAT Format, int mip);

// returns block size for block-compressed formats
UINT GetFormatBPP(DXGI_FORMAT f);

DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT f);
DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f, FormatComponentType hint);
DXGI_FORMAT GetDepthTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetFloatTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetUnormTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetSnormTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetUIntTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetSIntTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT f);
DXGI_FORMAT GetNonSRGBFormat(DXGI_FORMAT f);
bool IsBlockFormat(DXGI_FORMAT f);
bool IsDepthFormat(DXGI_FORMAT f);
bool IsDepthAndStencilFormat(DXGI_FORMAT f);

bool IsUIntFormat(DXGI_FORMAT f);
bool IsTypelessFormat(DXGI_FORMAT f);
bool IsIntFormat(DXGI_FORMAT f);
bool IsSRGBFormat(DXGI_FORMAT f);

// not technically DXGI, but makes more sense to have it here common between D3D versions
PrimitiveTopology MakePrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY Topo);
D3D_PRIMITIVE_TOPOLOGY MakeD3DPrimitiveTopology(PrimitiveTopology Topo);
