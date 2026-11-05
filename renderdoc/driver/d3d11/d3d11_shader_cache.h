/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2026 Baldur Karlsson
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

#include <map>
#include "api/replay/version.h"
#include "driver/dx/official/d3d11_4.h"

class WrappedID3D11Device;

class D3D11ShaderCache
{
public:
  D3D11ShaderCache(WrappedID3D11Device *wrapper);
  ~D3D11ShaderCache();

  rdcstr GetShaderBlob(const rdcstr &source, const rdcstr &entry, const uint32_t compileFlags,
                       const rdcarray<rdcstr> &includeDirs, const rdcstr &profile,
                       ID3DBlob **srcblob);
  ID3D11VertexShader *MakeVShader(const rdcstr &source, const rdcstr &entry, const rdcstr &profile,
                                  int numInputDescs = 0, D3D11_INPUT_ELEMENT_DESC *inputs = NULL,
                                  ID3D11InputLayout **ret = NULL, rdcarray<byte> *blob = NULL);
  ID3D11GeometryShader *MakeGShader(const rdcstr &source, const rdcstr &entry, const rdcstr &profile);
  ID3D11PixelShader *MakePShader(const rdcstr &source, const rdcstr &entry, const rdcstr &profile);
  ID3D11ComputeShader *MakeCShader(const rdcstr &source, const rdcstr &entry, const rdcstr &profile);

  void SetCaching(bool enabled) { m_CacheShaders = enabled; }
private:
  static const uint32_t m_ShaderCacheMagic = 0xf000baba;
  static const uint32_t m_ShaderCacheVersion =
      RENDERDOC_VERSION_MAJOR * 1000 + RENDERDOC_VERSION_MINOR;

  ID3D11Device *m_pDevice = NULL;

  uint32_t m_CompileFlags = 0;

  bool m_ShaderCacheDirty = false, m_CacheShaders = false;
  std::map<uint64_t, ID3DBlob *> m_ShaderCache;
};
