/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2019-2021 Baldur Karlsson
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

#include <comdef.h>
#include <string>
#include <vector>
#include "dx/official/dxgi1_4.h"

extern std::string D3DFullscreenQuadVertex;
extern std::string D3DDefaultVertex;
extern std::string D3DDefaultPixel;

#define COM_SMARTPTR(classname) _COM_SMARTPTR_TYPEDEF(classname, __uuidof(classname))

COM_SMARTPTR(IDXGISwapChain);
COM_SMARTPTR(IDXGISwapChain1);
COM_SMARTPTR(IDXGISwapChain2);
COM_SMARTPTR(IDXGISwapChain3);
COM_SMARTPTR(IDXGIFactory);
COM_SMARTPTR(IDXGIFactory1);
COM_SMARTPTR(IDXGIFactory4);
COM_SMARTPTR(IDXGIAdapter);
COM_SMARTPTR(IDXGISurface);
COM_SMARTPTR(IDXGIResource);

template <typename T>
ULONG GetRefcount(T *ptr)
{
  ptr->AddRef();
  return ptr->Release();
}

std::vector<IDXGIAdapterPtr> FindD3DAdapters(IDXGIFactoryPtr factory, int argc, char **argv,
                                             bool &warp);

enum class ResourceType
{
  Buffer,
  Texture1D,
  Texture1DArray,
  Texture2D,
  Texture2DArray,
  Texture2DMS,
  Texture2DMSArray,
  Texture3D,
};

enum class ViewType
{
  SRV,
  RTV,
  DSV,
  UAV,
  CBV,
};
