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

#include "d3d11_shader_cache.h"
#include "common/shader_cache.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "strings/string_utils.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);

struct D3DBlobShaderCallbacks
{
  pD3DCreateBlob GetCreateBlob() const
  {
    static pD3DCreateBlob blobCreate = NULL;

    if(!blobCreate)
    {
      HMODULE d3dcompiler = GetD3DCompiler();

      if(d3dcompiler == NULL)
        RDCFATAL("Can't get handle to d3dcompiler_??.dll");

      blobCreate = (pD3DCreateBlob)GetProcAddress(d3dcompiler, "D3DCreateBlob");

      if(blobCreate == NULL)
        RDCFATAL("d3dcompiler.dll doesn't contain D3DCreateBlob");
    }

    return blobCreate;
  }

  bool Create(uint32_t size, byte *data, ID3DBlob **ret) const
  {
    RDCASSERT(ret);

    pD3DCreateBlob blobCreate = GetCreateBlob();

    *ret = NULL;
    HRESULT hr = blobCreate((SIZE_T)size, ret);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create blob of size %u from shadercache: %s", size, ToStr(ret).c_str());
      return false;
    }

    memcpy((*ret)->GetBufferPointer(), data, size);

    return true;
  }

  void Destroy(ID3DBlob *blob) const { blob->Release(); }
  uint32_t GetSize(ID3DBlob *blob) const { return (uint32_t)blob->GetBufferSize(); }
  const byte *GetData(ID3DBlob *blob) const { return (const byte *)blob->GetBufferPointer(); }
  pD3DCreateBlob m_BlobCreate = NULL;
} D3D11ShaderCacheCallbacks;

D3D11ShaderCache::D3D11ShaderCache(WrappedID3D11Device *wrapper)
{
  m_pDevice = wrapper;

  bool success = LoadShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, D3D11ShaderCacheCallbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  m_CompileFlags = D3DCOMPILE_WARNINGS_ARE_ERRORS;

  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

  // if we're being self-captured, the 'real' device will respond to renderdoc's UUID. Enable debug
  // shaders
  IUnknown *dummy = NULL;
  if(wrapper->GetReal())
    wrapper->GetReal()->QueryInterface(IRenderDoc_uuid, (void **)&dummy);

  if(dummy)
  {
    m_CompileFlags |=
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_OPTIMIZATION_LEVEL0;
    SAFE_RELEASE(dummy);
  }
}

D3D11ShaderCache::~D3D11ShaderCache()
{
  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    D3D11ShaderCacheCallbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      D3D11ShaderCacheCallbacks.Destroy(it->second);
  }
}

rdcstr D3D11ShaderCache::GetShaderBlob(const char *source, const char *entry,
                                       const uint32_t compileFlags,
                                       const rdcarray<rdcstr> &includeDirs, const char *profile,
                                       ID3DBlob **srcblob)
{
  rdcstr cbuffers = GetEmbeddedResource(hlsl_cbuffers_h);
  rdcstr texsample = GetEmbeddedResource(hlsl_texsample_h);

  EmbeddedD3DIncluder includer(includeDirs, {
                                                {"hlsl_texsample.h", texsample},
                                                {"hlsl_cbuffers.h", cbuffers},
                                            });

  uint32_t hash = strhash(source);
  hash = strhash(entry, hash);
  hash = strhash(profile, hash);
  hash = strhash(cbuffers.c_str(), hash);
  hash = strhash(texsample.c_str(), hash);
  hash ^= compileFlags;

  if(m_ShaderCache.find(hash) != m_ShaderCache.end())
  {
    *srcblob = m_ShaderCache[hash];
    (*srcblob)->AddRef();
    return "";
  }

  HRESULT hr = S_OK;

  ID3DBlob *byteBlob = NULL;
  ID3DBlob *errBlob = NULL;

  HMODULE d3dcompiler = GetD3DCompiler();

  if(d3dcompiler == NULL)
  {
    RDCFATAL("Can't get handle to d3dcompiler_??.dll");
  }

  pD3DCompile compileFunc = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");

  if(compileFunc == NULL)
  {
    RDCFATAL("Can't get D3DCompile from d3dcompiler_??.dll");
  }

  uint32_t flags = compileFlags & ~D3DCOMPILE_NO_PRESHADER;

  hr = compileFunc(source, strlen(source), entry, NULL, &includer, entry, profile, flags, 0,
                   &byteBlob, &errBlob);

  rdcstr errors = "";

  if(errBlob)
  {
    errors = (char *)errBlob->GetBufferPointer();

    rdcstr logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCWARN("Shader compile error in '%s':\n%s", entry, logerror.c_str());

    SAFE_RELEASE(errBlob);

    if(FAILED(hr))
    {
      SAFE_RELEASE(byteBlob);
      return errors;
    }
  }

  if(m_CacheShaders)
  {
    m_ShaderCache[hash] = byteBlob;
    byteBlob->AddRef();
    m_ShaderCacheDirty = true;
  }

  SAFE_RELEASE(errBlob);

  *srcblob = byteBlob;
  return errors;
}

ID3D11VertexShader *D3D11ShaderCache::MakeVShader(const char *source, const char *entry,
                                                  const char *profile, int numInputDescs,
                                                  D3D11_INPUT_ELEMENT_DESC *inputs,
                                                  ID3D11InputLayout **ret, rdcarray<byte> *blob)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, m_CompileFlags, {}, profile, &byteBlob) != "")
  {
    RDCERR("Couldn't get shader blob for %s", entry);
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11VertexShader *ps = NULL;

  HRESULT hr = m_pDevice->CreateVertexShader(bytecode, bytecodeLen, NULL, &ps);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create vertex shader for %s %s", entry, ToStr(ret).c_str());

    SAFE_RELEASE(byteBlob);

    return NULL;
  }

  if(numInputDescs)
  {
    hr = m_pDevice->CreateInputLayout(inputs, numInputDescs, bytecode, bytecodeLen, ret);

    if(FAILED(hr))
    {
      RDCERR("Couldn't create input layout for %s %s", entry, ToStr(ret).c_str());
    }
  }

  if(blob)
  {
    blob->resize(bytecodeLen);
    memcpy(&(*blob)[0], bytecode, bytecodeLen);
  }

  SAFE_RELEASE(byteBlob);

  return ps;
}

ID3D11GeometryShader *D3D11ShaderCache::MakeGShader(const char *source, const char *entry,
                                                    const char *profile)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, m_CompileFlags, {}, profile, &byteBlob) != "")
  {
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11GeometryShader *gs = NULL;

  HRESULT hr = m_pDevice->CreateGeometryShader(bytecode, bytecodeLen, NULL, &gs);

  SAFE_RELEASE(byteBlob);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create geometry shader for %s %s", entry, ToStr(hr).c_str());
    return NULL;
  }

  return gs;
}

ID3D11PixelShader *D3D11ShaderCache::MakePShader(const char *source, const char *entry,
                                                 const char *profile)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, m_CompileFlags, {}, profile, &byteBlob) != "")
  {
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11PixelShader *ps = NULL;

  HRESULT hr = m_pDevice->CreatePixelShader(bytecode, bytecodeLen, NULL, &ps);

  SAFE_RELEASE(byteBlob);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create pixel shader for %s %s", entry, ToStr(hr).c_str());
    return NULL;
  }

  return ps;
}

ID3D11ComputeShader *D3D11ShaderCache::MakeCShader(const char *source, const char *entry,
                                                   const char *profile)
{
  ID3DBlob *byteBlob = NULL;

  if(GetShaderBlob(source, entry, m_CompileFlags, {}, profile, &byteBlob) != "")
  {
    return NULL;
  }

  void *bytecode = byteBlob->GetBufferPointer();
  size_t bytecodeLen = byteBlob->GetBufferSize();

  ID3D11ComputeShader *cs = NULL;

  HRESULT hr = m_pDevice->CreateComputeShader(bytecode, bytecodeLen, NULL, &cs);

  SAFE_RELEASE(byteBlob);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create compute shader for %s %s", entry, ToStr(hr).c_str());
    return NULL;
  }

  return cs;
}
