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

#include "d3d12_shader_cache.h"
#include "common/shader_cache.h"
#include "core/plugins.h"
#include "core/settings.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dx/official/dxcapi.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "strings/string_utils.h"
#include "d3d12_device.h"

RDOC_CONFIG(rdcstr, D3D12_DXCPath, "", "The location of the dxcompiler.dll library to use.");

static void *SearchForDXC(rdcstr &path)
{
  void *ret = NULL;

  path = D3D12_DXCPath();

  if(strlower(path).contains("dxcompiler.dll") && FileIO::exists(path))
    return Process::LoadModule(path);

  path = LocatePluginFile("d3d12", "dxcompiler.dll");

  // don't do a plain load yet, only load if we got an actual file
  if(path != "dxcompiler.dll")
    return Process::LoadModule(path);

  // Search windows SDK folders, using the registry to locate the SDK
  for(int wow64Pass = 0; wow64Pass < 2; wow64Pass++)
  {
    rdcstr regpath = "SOFTWARE\\";
    if(wow64Pass == 1)
      regpath += "Wow6432Node\\";
    regpath += "Microsoft\\Microsoft SDKs\\Windows\\v10.0";

    HKEY key = NULL;
    LSTATUS regret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, regpath.c_str(), 0, KEY_READ, &key);

    if(regret != ERROR_SUCCESS)
    {
      if(key)
        RegCloseKey(key);
      continue;
    }

    DWORD dataSize = 0;
    regret = RegGetValueW(key, NULL, L"InstallationFolder", RRF_RT_ANY, NULL, NULL, &dataSize);

    if(regret == ERROR_SUCCESS)
    {
      // this is the size in bytes
      wchar_t *data = new wchar_t[dataSize / sizeof(wchar_t)];
      RegGetValueW(key, NULL, L"InstallationFolder", RRF_RT_ANY, NULL, data, &dataSize);

      rdcstr sdkpath = StringFormat::Wide2UTF8(data);

      RegCloseKey(key);

      delete[] data;

      if(sdkpath.back() != '\\')
        sdkpath.push_back('\\');

      // next search the versioned bin folders, from newest to oldest
      sdkpath += "bin\\";

      // sort by name
      rdcarray<PathEntry> entries;
      FileIO::GetFilesInDirectory(sdkpath, entries);
      std::sort(entries.begin(), entries.end());

      // do a reverse iteration so we get the latest SDK first
      for(size_t i = 0; i < entries.size(); i++)
      {
        const PathEntry &e = entries[entries.size() - 1 - i];

        // skip any files
        if(!(e.flags & PathProperty::Directory))
          continue;

        // we've found an SDK! check to see if it contains dxcompiler.dll
        if(e.filename.beginsWith("10.0."))
        {
          path = sdkpath + e.filename + "\\x64\\dxcompiler.dll";

          if(FileIO::exists(path))
            return Process::LoadModule(path);
        }
      }

      // try in the Redist folder
      path = sdkpath + "..\\Redist\\D3D\\x64\\dxcompiler.dll";

      if(FileIO::exists(path))
        return Process::LoadModule(path);

      // if we've gotten here and haven't returned anything, then try just the base x64 folder
      path = sdkpath + "x64\\dxcompiler.dll";

      if(FileIO::exists(path))
        return Process::LoadModule(path);
    }
    else
    {
      RegCloseKey(key);
    }
  }

  // if we got here without finding any specific path'd dxc, just try loading it from anywhere
  ret = Process::LoadModule("dxcompiler.dll");
  path = "PATH";

  return ret;
}

static HMODULE GetDXC()
{
  static bool searched = false;
  static HMODULE ret = NULL;
  if(searched)
    return ret;

  searched = true;

  // we actively do not try to load dxil.dll as we rely on our own hashing and the validator is not
  // useful otherwise. This simplifies the search as we only need to find dxcompiler.dll, preferring
  // one specified with a config variable, then our shipped version, then one in a windows SDK
  // before finally searching PATH.
  rdcstr path;
  ret = (HMODULE)SearchForDXC(path);

  if(ret)
    RDCLOG("Loaded dxcompiler.dll from %s", path.c_str());
  else
    RDCERR("Couldn't load dxcompiler.dll");

  return ret;
}

typedef HRESULT(WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob **ppBlob);
typedef DXC_API_IMPORT HRESULT(__stdcall *pDxcCreateInstance)(REFCLSID rclsid, REFIID riid,
                                                              LPVOID *ppv);

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

  bool Create(uint32_t size, const void *data, ID3DBlob **ret) const
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

class EmbeddedID3DIncludeHandler : public IDxcIncludeHandler
{
public:
  EmbeddedID3DIncludeHandler(IDxcLibrary *dxcLib, const rdcarray<rdcstr> &includeDirs,
                             const rdcarray<rdcpair<rdcstr, IDxcBlob *>> fixedFileBlobs)
      : m_dxcLibrary(dxcLib)
  {
    m_includeDirs = includeDirs;
    m_fixedFileBlobs = fixedFileBlobs;

    if(m_dxcLibrary)
    {
      IDxcIncludeHandler *includeHandler = NULL;
      HRESULT res = m_dxcLibrary->CreateIncludeHandler(&includeHandler);

      if(SUCCEEDED(res))
        m_defaultHandler = includeHandler;
    }
  }

  virtual ~EmbeddedID3DIncludeHandler() { SAFE_RELEASE(m_defaultHandler); }
  HRESULT STDMETHODCALLTYPE LoadSource(
      _In_z_ LPCWSTR pFilename,    // Candidate filename.
      _COM_Outptr_result_maybenull_ IDxcBlob *
          *ppIncludeSource    // Resultant source object for included file, nullptr if not found.
  )
  {
    IDxcBlob *dxcBlob = NULL;

    rdcstr filename = StringFormat::Wide2UTF8(pFilename);
    rdcstr fileNameWithoutRelSep = filename;

    if(FileIO::IsRelativePath(filename))
    {
      size_t index = filename.find_first_of("./");
      fileNameWithoutRelSep = filename.substr(index + 2);
    }

    for(const rdcpair<rdcstr, IDxcBlob *> &f : m_fixedFileBlobs)
    {
      if(filename == f.first || fileNameWithoutRelSep == f.first)
      {
        dxcBlob = f.second;
        break;
      }
    }

    if(!dxcBlob && FileIO::IsRelativePath(filename))
    {
      rdcstr absFilePath = fileNameWithoutRelSep;
      for(const rdcstr &dir : m_includeDirs)
      {
        absFilePath = dir + absFilePath;
        rdcstr source;
        if(FileIO::exists(absFilePath) && FileIO::ReadAll(absFilePath, source) && m_dxcLibrary)
        {
          IDxcBlobEncoding *encodedBlob = NULL;
          HRESULT res = m_dxcLibrary->CreateBlobWithEncodingFromPinned(
              source.c_str(), (UINT32)source.size(), CP_UTF8, &encodedBlob);

          if(!SUCCEEDED(res))
          {
            RDCERR("Unable to creata Blob");
          }
          else
          {
            dxcBlob = encodedBlob;
          }

          break;
        }
      }
    }

    if(dxcBlob)
    {
      *ppIncludeSource = dxcBlob;
      return S_OK;
    }

    if(!dxcBlob && m_defaultHandler)
    {
      return m_defaultHandler->LoadSource(pFilename, ppIncludeSource);
    }

    return E_FAIL;
  }

  virtual HRESULT STDMETHODCALLTYPE QueryInterface(
      /* [in] */ REFIID riid,
      /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
  {
    if(m_defaultHandler)
    {
      return m_defaultHandler->QueryInterface(riid, ppvObject);
    }

    return E_FAIL;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef(void)
  {
    if(m_defaultHandler)
      return m_defaultHandler->AddRef();

    return 0;
  }

  virtual ULONG STDMETHODCALLTYPE Release(void)
  {
    if(m_defaultHandler)
      return m_defaultHandler->Release();

    return 0;
  }

private:
  rdcarray<rdcstr> m_includeDirs;
  rdcarray<rdcpair<rdcstr, IDxcBlob *>> m_fixedFileBlobs;
  IDxcLibrary *m_dxcLibrary;
  IDxcIncludeHandler *m_defaultHandler;
};

D3D12ShaderCache::D3D12ShaderCache(WrappedID3D12Device *device)
{
  bool success = LoadShaderCache("d3dshaders.cache", m_ShaderCacheMagic, m_ShaderCacheVersion,
                                 m_ShaderCache, D3D12ShaderCacheCallbacks);

  // if we failed to load from the cache
  m_ShaderCacheDirty = !success;

  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

  // if we're being self-captured, the 'real' device will respond to renderdoc's UUID. Enable debug
  // shaders
  IUnknown *dummy = NULL;
  if(device->GetReal())
    device->GetReal()->QueryInterface(IRenderDoc_uuid, (void **)&dummy);

  if(dummy)
  {
    m_CompileFlags |=
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_OPTIMIZATION_LEVEL0;
    SAFE_RELEASE(dummy);
  }
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

rdcstr D3D12ShaderCache::GetShaderBlob(const char *source, const char *entry,
                                       const ShaderCompileFlags &compileFlags,
                                       const rdcarray<rdcstr> &includeDirs, const char *profile,
                                       ID3DBlob **srcblob)
{
  rdcstr cbuffers = GetEmbeddedResource(hlsl_cbuffers_h);
  rdcstr texsample = GetEmbeddedResource(hlsl_texsample_h);

  uint32_t hash = strhash(source);
  hash = strhash(entry, hash);
  hash = strhash(profile, hash);
  hash = strhash(cbuffers.c_str(), hash);
  hash = strhash(texsample.c_str(), hash);
  for(const ShaderCompileFlag &f : compileFlags.flags)
  {
    hash = strhash(f.name.c_str(), hash);
    hash = strhash(f.value.c_str(), hash);
  }

  if(m_ShaderCache.find(hash) != m_ShaderCache.end())
  {
    *srcblob = m_ShaderCache[hash];
    (*srcblob)->AddRef();
    return "";
  }

  HRESULT hr = S_OK;

  ID3DBlob *byteBlob = NULL;
  ID3DBlob *errBlob = NULL;

  if(profile[3] >= '6')
  {
    // compile as DXIL

    UINT prevErrorMode = GetErrorMode();
    SetErrorMode(prevErrorMode | SEM_FAILCRITICALERRORS);

    HMODULE dxc = GetDXC();

    SetErrorMode(prevErrorMode);

    if(dxc == NULL)
    {
      return "Couldn't locate dxcompiler.dll. Ensure you have a Windows 10 SDK installed or place "
             "dxcompiler.dll in RenderDoc's plugins/d3d12 folder.";
    }
    else
    {
      pDxcCreateInstance dxcCreate = (pDxcCreateInstance)GetProcAddress(dxc, "DxcCreateInstance");

      IDxcLibrary *library = NULL;
      hr = dxcCreate(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void **)&library);

      if(FAILED(hr))
      {
        SAFE_RELEASE(library);
        return "Couldn't create DXC library";
      }

      IDxcCompiler *compiler = NULL;
      hr = dxcCreate(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void **)&compiler);

      if(FAILED(hr))
      {
        SAFE_RELEASE(library);
        SAFE_RELEASE(compiler);
        return "Couldn't create DXC compiler";
      }

      IDxcBlobEncoding *sourceBlob = NULL;
      hr = library->CreateBlobWithEncodingFromPinned(source, (UINT)strlen(source), CP_UTF8,
                                                     &sourceBlob);

      if(FAILED(hr))
      {
        SAFE_RELEASE(library);
        SAFE_RELEASE(compiler);
        SAFE_RELEASE(sourceBlob);
        return "Couldn't create DXC blob";
      }

      IDxcBlobEncoding *texSampleBlob = NULL;
      hr = library->CreateBlobWithEncodingFromPinned(texsample.c_str(), (UINT)texsample.size(),
                                                     CP_UTF8, &texSampleBlob);

      if(FAILED(hr))
      {
        SAFE_RELEASE(library);
        SAFE_RELEASE(compiler);
        SAFE_RELEASE(sourceBlob);
        SAFE_RELEASE(texSampleBlob);
        return "Couldn't create DXC blob";
      }

      IDxcBlobEncoding *cBufferBlob = NULL;
      hr = library->CreateBlobWithEncodingFromPinned(cbuffers.c_str(), (UINT)cbuffers.size(),
                                                     CP_UTF8, &cBufferBlob);

      if(FAILED(hr))
      {
        SAFE_RELEASE(library);
        SAFE_RELEASE(compiler);
        SAFE_RELEASE(sourceBlob);
        SAFE_RELEASE(texSampleBlob);
        SAFE_RELEASE(cBufferBlob);
        return "Couldn't create DXC blob";
      }

      EmbeddedID3DIncludeHandler includeHandler(
          library, includeDirs,
          {{"hlsl_texsample.h", texSampleBlob}, {"hlsl_cbuffers.h", cBufferBlob}});

      IDxcOperationResult *result = NULL;
      uint32_t flags = DXBC::DecodeFlags(compileFlags) & ~D3DCOMPILE_NO_PRESHADER;
      rdcarray<rdcwstr> argsData;
      DXBC::EncodeDXCFlags(flags, argsData);
      rdcarray<LPCWSTR> arguments;
      for(const rdcwstr &arg : argsData)
        arguments.push_back(arg.c_str());

      hr = compiler->Compile(sourceBlob, NULL, StringFormat::UTF82Wide(entry).c_str(),
                             StringFormat::UTF82Wide(profile).c_str(), arguments.data(),
                             arguments.count(), NULL, 0, &includeHandler, &result);

      SAFE_RELEASE(sourceBlob);

      if(SUCCEEDED(hr) && result)
        result->GetStatus(&hr);

      if(SUCCEEDED(hr))
      {
        IDxcBlob *code = NULL;
        result->GetResult(&code);

        D3D12ShaderCacheCallbacks.Create((uint32_t)code->GetBufferSize(), code->GetBufferPointer(),
                                         &byteBlob);

        if(!DXBC::DXBCContainer::IsHashedContainer(byteBlob->GetBufferPointer(),
                                                   byteBlob->GetBufferSize()))
          DXBC::DXBCContainer::HashContainer(byteBlob->GetBufferPointer(), byteBlob->GetBufferSize());

        SAFE_RELEASE(code);
      }
      else
      {
        if(result)
        {
          IDxcBlobEncoding *dxcErrors = NULL;
          hr = result->GetErrorBuffer(&dxcErrors);
          if(SUCCEEDED(hr) && dxcErrors)
          {
            D3D12ShaderCacheCallbacks.Create((uint32_t)dxcErrors->GetBufferSize(),
                                             dxcErrors->GetBufferPointer(), &errBlob);
          }

          SAFE_RELEASE(dxcErrors);
        }

        if(!errBlob)
        {
          rdcstr err = "No compilation result found from DXC compile";
          D3D12ShaderCacheCallbacks.Create((uint32_t)err.size(), err.c_str(), &errBlob);
        }
      }

      SAFE_RELEASE(library);
      SAFE_RELEASE(compiler);
      SAFE_RELEASE(result);
    }
  }
  else
  {
    EmbeddedD3DIncluder includer(includeDirs, {
                                                  {"hlsl_texsample.h", texsample},
                                                  {"hlsl_cbuffers.h", cbuffers},
                                              });

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

    uint32_t flags = DXBC::DecodeFlags(compileFlags) & ~D3DCOMPILE_NO_PRESHADER;

    hr = compileFunc(source, strlen(source), entry, NULL, &includer, entry, profile, flags, 0,
                     &byteBlob, &errBlob);
  }

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

  if(m_CacheShaders && byteBlob)
  {
    m_ShaderCache[hash] = byteBlob;
    byteBlob->AddRef();
    m_ShaderCacheDirty = true;
  }

  SAFE_RELEASE(errBlob);

  *srcblob = byteBlob;
  return errors;
}

rdcstr D3D12ShaderCache::GetShaderBlob(const char *source, const char *entry, uint32_t compileFlags,
                                       const rdcarray<rdcstr> &includeDirs, const char *profile,
                                       ID3DBlob **srcblob)
{
  return GetShaderBlob(source, entry, DXBC::EncodeFlags(compileFlags | m_CompileFlags, profile),
                       includeDirs, profile, srcblob);
}

ID3DBlob *D3D12ShaderCache::MakeFixedColShader(FixedColVariant variant, bool dxil)
{
  ID3DBlob *ret = NULL;
  rdcstr hlsl =
      StringFormat::Fmt("#define VARIANT %u\n\n", variant) + GetEmbeddedResource(fixedcol_hlsl);
  bool wasCaching = m_CacheShaders;
  m_CacheShaders = true;
  GetShaderBlob(hlsl.c_str(), "main", ShaderCompileFlags(), {}, dxil ? "ps_6_0" : "ps_5_0", &ret);
  m_CacheShaders = wasCaching;

  if(!ret)
  {
    const rdcstr embedded[] = {
        GetEmbeddedResource(fixedcol_0_dxbc),
        GetEmbeddedResource(fixedcol_1_dxbc),
        GetEmbeddedResource(fixedcol_2_dxbc),
        GetEmbeddedResource(fixedcol_3_dxbc),
    };

    D3D12ShaderCacheCallbacks.Create((uint32_t)embedded[variant].size(), embedded[variant].data(),
                                     &ret);
  }

  return ret;
}

ID3DBlob *D3D12ShaderCache::GetQuadShaderDXILBlob()
{
  rdcstr embedded = GetEmbeddedResource(quadwrite_dxbc);
  if(embedded.empty() || !embedded.beginsWith("DXBC"))
    return NULL;

  ID3DBlob *ret = NULL;
  D3D12ShaderCacheCallbacks.Create((uint32_t)embedded.size(), embedded.data(), &ret);
  return ret;
}

ID3DBlob *D3D12ShaderCache::GetPrimitiveIDShaderDXILBlob()
{
  rdcstr embedded = GetEmbeddedResource(pixelhistory_primitiveid_dxbc);
  if(embedded.empty() || !embedded.beginsWith("DXBC"))
    return NULL;

  ID3DBlob *ret = NULL;
  D3D12ShaderCacheCallbacks.Create((uint32_t)embedded.size(), embedded.data(), &ret);
  return ret;
}

ID3DBlob *D3D12ShaderCache::GetFixedColorShaderDXILBlob(uint32_t variant)
{
  const rdcstr variants[] = {
      GetEmbeddedResource(pixelhistory_fixedcol_0_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_1_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_2_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_3_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_4_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_5_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_6_dxbc),
      GetEmbeddedResource(pixelhistory_fixedcol_7_dxbc),
  };

  const rdcstr embedded = variants[variant];
  if(embedded.empty() || !embedded.beginsWith("DXBC"))
    return NULL;

  ID3DBlob *ret = NULL;
  D3D12ShaderCacheCallbacks.Create((uint32_t)embedded.size(), embedded.data(), &ret);
  return ret;
}

void D3D12ShaderCache::LoadDXC()
{
  GetDXC();
}
