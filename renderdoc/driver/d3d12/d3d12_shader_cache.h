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

#include <map>
#include "driver/dx/official/d3d11_4.h"
#include "d3d12_common.h"

class WrappedID3D12Device;

class D3D12ShaderCache
{
public:
  D3D12ShaderCache(WrappedID3D12Device *device);
  ~D3D12ShaderCache();

  rdcstr GetShaderBlob(const char *source, const char *entry, uint32_t compileFlags,
                       const rdcarray<rdcstr> &includeDirs, const char *profile, ID3DBlob **srcblob);
  rdcstr GetShaderBlob(const char *source, const char *entry, const ShaderCompileFlags &compileFlags,
                       const rdcarray<rdcstr> &includeDirs, const char *profile, ID3DBlob **srcblob);

  D3D12RootSignature GetRootSig(const void *data, size_t dataSize);
  ID3DBlob *MakeRootSig(const rdcarray<D3D12_ROOT_PARAMETER1> &params,
                        D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
                        UINT NumStaticSamplers = 0,
                        const D3D12_STATIC_SAMPLER_DESC1 *StaticSamplers = NULL);
  ID3DBlob *MakeRootSig(const D3D12RootSignature &rootsig);

  // must match the values in fixedcol.hlsl
  enum FixedColVariant
  {
    RED = 0,
    GREEN = 1,
    HIGHLIGHT = 2,
    WIREFRAME = 3,
  };
  ID3DBlob *MakeFixedColShader(FixedColVariant variant, bool dxil = false);
  ID3DBlob *GetQuadShaderDXILBlob();
  ID3DBlob *GetPrimitiveIDShaderDXILBlob();
  ID3DBlob *GetFixedColorShaderDXILBlob(uint32_t variant);

  void LoadDXC();

  void SetDevConfiguration(D3D12DevConfiguration *config) { m_DevConfig = config; }
  void SetCaching(bool enabled) { m_CacheShaders = enabled; }
private:
  static const uint32_t m_ShaderCacheMagic = 0xf000baba;
  static const uint32_t m_ShaderCacheVersion = 3;

  uint32_t m_CompileFlags = 0;

  bool m_ShaderCacheDirty = false, m_CacheShaders = false;
  std::map<uint32_t, ID3DBlob *> m_ShaderCache;

  D3D12DevConfiguration *m_DevConfig = NULL;

  D3D12_STATIC_SAMPLER_DESC1 Upconvert(const D3D12_STATIC_SAMPLER_DESC &StaticSampler);
  D3D12_STATIC_SAMPLER_DESC Downconvert(const D3D12_STATIC_SAMPLER_DESC1 &StaticSampler);
};
