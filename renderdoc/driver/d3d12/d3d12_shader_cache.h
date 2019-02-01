/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
#include <string>
#include <vector>
#include "api/replay/renderdoc_replay.h"
#include "driver/dx/official/d3d11_4.h"

class WrappedID3D11Device;

class D3D12ShaderCache
{
public:
  D3D12ShaderCache();
  ~D3D12ShaderCache();

  std::string GetShaderBlob(const char *source, const char *entry, const uint32_t compileFlags,
                            const char *profile, ID3DBlob **srcblob);

  D3D12RootSignature GetRootSig(const void *data, size_t dataSize);
  ID3DBlob *MakeRootSig(const std::vector<D3D12_ROOT_PARAMETER1> &params,
                        D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
                        UINT NumStaticSamplers = 0,
                        const D3D12_STATIC_SAMPLER_DESC *StaticSamplers = NULL);
  ID3DBlob *MakeRootSig(const D3D12RootSignature &rootsig);
  ID3DBlob *MakeFixedColShader(float overlayConsts[4]);

  void SetCaching(bool enabled) { m_CacheShaders = enabled; }
private:
  static const uint32_t m_ShaderCacheMagic = 0xf000baba;
  static const uint32_t m_ShaderCacheVersion = 3;

  bool m_ShaderCacheDirty = false, m_CacheShaders = false;
  std::map<uint32_t, ID3DBlob *> m_ShaderCache;
};