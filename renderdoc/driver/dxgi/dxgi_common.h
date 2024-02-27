/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "api/replay/data_types.h"
#include "api/replay/rdcflatmap.h"
#include "api/replay/replay_enums.h"
#include "api/replay/stringise.h"
#include "driver/dx/official/d3dcommon.h"
#include "driver/dx/official/dxgi1_5.h"
#include "maths/vec.h"

ResourceFormat MakeResourceFormat(DXGI_FORMAT fmt);
DXGI_FORMAT MakeDXGIFormat(ResourceFormat fmt);

UINT GetByteSize(int Width, int Height, int Depth, DXGI_FORMAT Format, int mip);
UINT GetRowPitch(int Width, DXGI_FORMAT Format, int mip);

DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT f);
DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetTypedFormat(DXGI_FORMAT f, CompType hint);
DXGI_FORMAT GetDepthTypedFormat(DXGI_FORMAT f);
DXGI_FORMAT GetDepthSRVFormat(DXGI_FORMAT f, UINT planeSlice);
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

DXGI_FORMAT GetYUVViewPlane0Format(DXGI_FORMAT f);
DXGI_FORMAT GetYUVViewPlane1Format(DXGI_FORMAT f);
void GetYUVShaderParameters(DXGI_FORMAT f, Vec4u &YUVDownsampleRate, Vec4u &YUVAChannels);

bool IsUIntFormat(DXGI_FORMAT f);
bool IsTypelessFormat(DXGI_FORMAT f);
bool IsIntFormat(DXGI_FORMAT f);
bool IsSRGBFormat(DXGI_FORMAT f);
bool IsYUVFormat(DXGI_FORMAT f);
bool IsYUVPlanarFormat(DXGI_FORMAT f);
UINT GetYUVNumRows(DXGI_FORMAT f, UINT height);

// not technically DXGI, but makes more sense to have it here common between D3D versions
Topology MakePrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY Topo);
D3D_PRIMITIVE_TOPOLOGY MakeD3DPrimitiveTopology(Topology Topo);

void WarnUnknownGUID(const char *name, REFIID riid);

struct ReplayOptions;

rdcstr GetDriverVersion(DXGI_ADAPTER_DESC &desc);
void ChooseBestMatchingAdapter(GraphicsAPI api, IDXGIFactory *factory,
                               const DXGI_ADAPTER_DESC &AdapterDesc, const ReplayOptions &opts,
                               bool *useWarp, IDXGIAdapter **adapter);

struct EmbeddedD3DIncluder : public ID3DInclude
{
private:
  rdcarray<rdcpair<rdcstr, rdcstr>> m_FixedFiles;
  rdcarray<rdcstr> m_IncludeDirs;

  rdcarray<rdcstr *> m_FileStrings;
  // use flatmap here to avoid pulling in the <map> header in such a high-profile place
  rdcflatmap<const void *, rdcstr> m_StringPaths;

public:
  EmbeddedD3DIncluder(const rdcarray<rdcstr> &includeDirs,
                      const rdcarray<rdcpair<rdcstr, rdcstr>> &fixed_files)
      : m_IncludeDirs(includeDirs), m_FixedFiles(fixed_files)
  {
  }
  ~EmbeddedD3DIncluder()
  {
    for(rdcstr *s : m_FileStrings)
      delete s;
  }
  virtual HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName,
                                         LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override;
  // we just 'leak' all handles we don't track open/close at fine-grained detail.
  virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData) override { return S_OK; }
};

DECLARE_REFLECTION_STRUCT(DXGI_SAMPLE_DESC);
DECLARE_REFLECTION_STRUCT(DXGI_ADAPTER_DESC);
DECLARE_REFLECTION_STRUCT(IID);
DECLARE_REFLECTION_STRUCT(LUID);
DECLARE_REFLECTION_ENUM(DXGI_FORMAT);
DECLARE_REFLECTION_ENUM(D3D_FEATURE_LEVEL);
DECLARE_REFLECTION_ENUM(D3D_DRIVER_TYPE);
