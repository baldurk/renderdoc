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

#include "d3d12_shader_cache.h"
#include "common/shader_cache.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/shaders/dxbc/dxbc_inspect.h"
#include "strings/string_utils.h"

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);

struct D3D12BlobShaderCallbacks
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
      RDCERR("Couldn't create blob of size %u from shadercache: HRESULT: %s", size,
             ToStr(hr).c_str());
      return false;
    }

    memcpy((*ret)->GetBufferPointer(), data, size);

    return true;
  }

  void Destroy(ID3DBlob *blob) const { blob->Release(); }
  uint32_t GetSize(ID3DBlob *blob) const { return (uint32_t)blob->GetBufferSize(); }
  const byte *GetData(ID3DBlob *blob) const { return (const byte *)blob->GetBufferPointer(); }
} D3D12ShaderCacheCallbacks;

struct EmbeddedD3D12Includer : public ID3DInclude
{
  std::string texsample = GetEmbeddedResource(hlsl_texsample_h);
  std::string cbuffers = GetEmbeddedResource(hlsl_cbuffers_h);

  virtual HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName,
                                         LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override
  {
    std::string *str;

    if(!strcmp(pFileName, "hlsl_texsample.h"))
      str = &texsample;
    else if(!strcmp(pFileName, "hlsl_cbuffers.h"))
      str = &cbuffers;
    else
      return E_FAIL;

    if(ppData)
      *ppData = str->c_str();
    if(pBytes)
      *pBytes = (uint32_t)str->size();

    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData) override { return S_OK; }
};

D3D12ShaderCache::D3D12ShaderCache()
{
  bool success = LoadShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, D3D12ShaderCacheCallbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;
}

D3D12ShaderCache::~D3D12ShaderCache()
{
  if(m_ShaderCacheDirty)
  {
    SaveShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion, m_ShaderCache,
                    D3D12ShaderCacheCallbacks);
  }
  else
  {
    for(auto it = m_ShaderCache.begin(); it != m_ShaderCache.end(); ++it)
      D3D12ShaderCacheCallbacks.Destroy(it->second);
  }
}

std::string D3D12ShaderCache::GetShaderBlob(const char *source, const char *entry,
                                            const uint32_t compileFlags, const char *profile,
                                            ID3DBlob **srcblob)
{
  EmbeddedD3D12Includer includer;

  uint32_t hash = strhash(source);
  hash = strhash(entry, hash);
  hash = strhash(profile, hash);
  hash = strhash(includer.cbuffers.c_str(), hash);
  hash = strhash(includer.texsample.c_str(), hash);
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

  std::string errors = "";

  if(errBlob)
  {
    errors = (char *)errBlob->GetBufferPointer();

    std::string logerror = errors;
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

D3D12RootSignature D3D12ShaderCache::GetRootSig(const void *data, size_t dataSize)
{
  PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER deserializeRootSig =
      (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12CreateVersionedRootSignatureDeserializer");

  PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER deserializeRootSigOld =
      (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12CreateRootSignatureDeserializer");

  if(deserializeRootSig == NULL)
  {
    RDCWARN("Can't get D3D12CreateVersionedRootSignatureDeserializer - old version of windows?");

    if(deserializeRootSigOld == NULL)
    {
      RDCERR("Can't get D3D12CreateRootSignatureDeserializer!");
      return D3D12RootSignature();
    }

    ID3D12RootSignatureDeserializer *deser = NULL;
    HRESULT hr = deserializeRootSigOld(data, dataSize, __uuidof(ID3D12RootSignatureDeserializer),
                                       (void **)&deser);

    if(FAILED(hr))
    {
      SAFE_RELEASE(deser);
      RDCERR("Can't get deserializer");
      return D3D12RootSignature();
    }

    D3D12RootSignature ret;

    const D3D12_ROOT_SIGNATURE_DESC *desc = deser->GetRootSignatureDesc();
    if(FAILED(hr))
    {
      SAFE_RELEASE(deser);
      RDCERR("Can't get descriptor");
      return D3D12RootSignature();
    }

    ret.Flags = desc->Flags;

    ret.params.resize(desc->NumParameters);

    ret.dwordLength = 0;

    for(size_t i = 0; i < ret.params.size(); i++)
    {
      ret.params[i].MakeFrom(desc->pParameters[i], ret.maxSpaceIndex);

      // Descriptor tables cost 1 DWORD each.
      // Root constants cost 1 DWORD each, since they are 32-bit values.
      // Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
      if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        ret.dwordLength++;
      else if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
        ret.dwordLength += desc->pParameters[i].Constants.Num32BitValues;
      else
        ret.dwordLength += 2;
    }

    if(desc->NumStaticSamplers > 0)
    {
      ret.samplers.assign(desc->pStaticSamplers, desc->pStaticSamplers + desc->NumStaticSamplers);

      for(size_t i = 0; i < ret.samplers.size(); i++)
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.samplers[i].RegisterSpace + 1);
    }

    SAFE_RELEASE(deser);

    return ret;
  }

  ID3D12VersionedRootSignatureDeserializer *deser = NULL;
  HRESULT hr = deserializeRootSig(
      data, dataSize, __uuidof(ID3D12VersionedRootSignatureDeserializer), (void **)&deser);

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get deserializer");
    return D3D12RootSignature();
  }

  D3D12RootSignature ret;

  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *verdesc = NULL;
  hr = deser->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &verdesc);
  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get descriptor");
    return D3D12RootSignature();
  }

  const D3D12_ROOT_SIGNATURE_DESC1 *desc = &verdesc->Desc_1_1;

  ret.Flags = desc->Flags;

  ret.params.resize(desc->NumParameters);

  ret.dwordLength = 0;

  for(size_t i = 0; i < ret.params.size(); i++)
  {
    ret.params[i].MakeFrom(desc->pParameters[i], ret.maxSpaceIndex);

    // Descriptor tables cost 1 DWORD each.
    // Root constants cost 1 DWORD each, since they are 32-bit values.
    // Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
    if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      ret.dwordLength++;
    else if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      ret.dwordLength += desc->pParameters[i].Constants.Num32BitValues;
    else
      ret.dwordLength += 2;
  }

  if(desc->NumStaticSamplers > 0)
  {
    ret.samplers.assign(desc->pStaticSamplers, desc->pStaticSamplers + desc->NumStaticSamplers);

    for(size_t i = 0; i < ret.samplers.size(); i++)
      ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.samplers[i].RegisterSpace + 1);
  }

  SAFE_RELEASE(deser);

  return ret;
}

ID3DBlob *D3D12ShaderCache::MakeRootSig(const std::vector<D3D12_ROOT_PARAMETER1> &params,
                                        D3D12_ROOT_SIGNATURE_FLAGS Flags, UINT NumStaticSamplers,
                                        const D3D12_STATIC_SAMPLER_DESC *StaticSamplers)
{
  PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE serializeRootSig =
      (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(
          GetModuleHandleA("d3d12.dll"), "D3D12SerializeVersionedRootSignature");

  PFN_D3D12_SERIALIZE_ROOT_SIGNATURE serializeRootSigOld =
      (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(GetModuleHandleA("d3d12.dll"),
                                                         "D3D12SerializeRootSignature");

  if(serializeRootSig == NULL)
  {
    RDCWARN("Can't get D3D12SerializeVersionedRootSignature - old version of windows?");

    if(serializeRootSigOld == NULL)
    {
      RDCERR("Can't get D3D12SerializeRootSignature!");
      return NULL;
    }

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.Flags = Flags;
    desc.NumStaticSamplers = NumStaticSamplers;
    desc.pStaticSamplers = StaticSamplers;
    desc.NumParameters = (UINT)params.size();

    std::vector<D3D12_ROOT_PARAMETER> params_1_0;
    params_1_0.resize(params.size());
    for(size_t i = 0; i < params.size(); i++)
    {
      params_1_0[i].ShaderVisibility = params[i].ShaderVisibility;
      params_1_0[i].ParameterType = params[i].ParameterType;

      if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      {
        params_1_0[i].Constants = params[i].Constants;
      }
      else if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        params_1_0[i].DescriptorTable.NumDescriptorRanges =
            params[i].DescriptorTable.NumDescriptorRanges;

        D3D12_DESCRIPTOR_RANGE *dst =
            new D3D12_DESCRIPTOR_RANGE[params[i].DescriptorTable.NumDescriptorRanges];
        params_1_0[i].DescriptorTable.pDescriptorRanges = dst;

        for(UINT r = 0; r < params[i].DescriptorTable.NumDescriptorRanges; r++)
        {
          dst[r].BaseShaderRegister =
              params[i].DescriptorTable.pDescriptorRanges[r].BaseShaderRegister;
          dst[r].NumDescriptors = params[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
          dst[r].OffsetInDescriptorsFromTableStart =
              params[i].DescriptorTable.pDescriptorRanges[r].OffsetInDescriptorsFromTableStart;
          dst[r].RangeType = params[i].DescriptorTable.pDescriptorRanges[r].RangeType;
          dst[r].RegisterSpace = params[i].DescriptorTable.pDescriptorRanges[r].RegisterSpace;

          if(params[i].DescriptorTable.pDescriptorRanges[r].Flags !=
             (D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
              D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE))
            RDCWARN("Losing information when reducing down to 1.0 root signature");
        }
      }
      else
      {
        params_1_0[i].Descriptor.RegisterSpace = params[i].Descriptor.RegisterSpace;
        params_1_0[i].Descriptor.ShaderRegister = params[i].Descriptor.ShaderRegister;

        if(params[i].Descriptor.Flags != D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE)
          RDCWARN("Losing information when reducing down to 1.0 root signature");
      }
    }

    desc.pParameters = &params_1_0[0];

    ID3DBlob *ret = NULL;
    ID3DBlob *errBlob = NULL;
    HRESULT hr = serializeRootSigOld(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &ret, &errBlob);

    for(size_t i = 0; i < params_1_0.size(); i++)
      if(params_1_0[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        delete[] params_1_0[i].DescriptorTable.pDescriptorRanges;

    if(FAILED(hr))
    {
      std::string errors = (char *)errBlob->GetBufferPointer();

      std::string logerror = errors;
      if(logerror.length() > 1024)
        logerror = logerror.substr(0, 1024) + "...";

      RDCERR("Root signature serialize error:\n%s", logerror.c_str());

      SAFE_RELEASE(errBlob);
      SAFE_RELEASE(ret);
      return NULL;
    }

    SAFE_RELEASE(errBlob);

    return ret;
  }

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC verdesc;
  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

  D3D12_ROOT_SIGNATURE_DESC1 &desc = verdesc.Desc_1_1;
  desc.Flags = Flags;
  desc.NumStaticSamplers = NumStaticSamplers;
  desc.pStaticSamplers = StaticSamplers;
  desc.NumParameters = (UINT)params.size();
  desc.pParameters = &params[0];

  ID3DBlob *ret = NULL;
  ID3DBlob *errBlob = NULL;
  HRESULT hr = serializeRootSig(&verdesc, &ret, &errBlob);

  if(FAILED(hr))
  {
    std::string errors = (char *)errBlob->GetBufferPointer();

    std::string logerror = errors;
    if(logerror.length() > 1024)
      logerror = logerror.substr(0, 1024) + "...";

    RDCERR("Root signature serialize error:\n%s", logerror.c_str());

    SAFE_RELEASE(errBlob);
    SAFE_RELEASE(ret);
    return NULL;
  }

  SAFE_RELEASE(errBlob);

  return ret;
}

ID3DBlob *D3D12ShaderCache::MakeRootSig(const D3D12RootSignature &rootsig)
{
  std::vector<D3D12_ROOT_PARAMETER1> params;
  params.resize(rootsig.params.size());
  for(size_t i = 0; i < params.size(); i++)
    params[i] = rootsig.params[i];

  return MakeRootSig(params, rootsig.Flags, (UINT)rootsig.samplers.size(),
                     rootsig.samplers.empty() ? NULL : &rootsig.samplers[0]);
}

ID3DBlob *D3D12ShaderCache::MakeFixedColShader(float overlayConsts[4])
{
  ID3DBlob *ret = NULL;
  std::string hlsl =
      StringFormat::Fmt("float4 main() : SV_Target0 { return float4(%f, %f, %f, %f); }\n",
                        overlayConsts[0], overlayConsts[1], overlayConsts[2], overlayConsts[3]);
  GetShaderBlob(hlsl.c_str(), "main", D3DCOMPILE_WARNINGS_ARE_ERRORS, "ps_5_0", &ret);
  return ret;
}
