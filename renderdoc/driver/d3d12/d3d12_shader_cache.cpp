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
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dx/official/dxcapi.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "strings/string_utils.h"
#include "d3d12_device.h"

static HMODULE GetDXC()
{
  static bool searched = false;
  static HMODULE ret = NULL;
  if(searched)
    return ret;

  searched = true;

  // we need to load dxil.dll first before dxcompiler.dll, because if dxil.dll can't be loaded when
  // dxcompiler.dll is loaded then validation is disabled and we might not be able to run
  // non-validated shaders (blehchh)

  // do two passes. First we try and find dxil.dll and dxcompiler.dll both together.
  // In the second pass we just look for dxcompiler.dll. Hence a higher priority dxcompiler.dll
  // without a dxil.dll will be less preferred than a lower priority dxcompiler.dll that does have a
  // dxil.dll
  for(int sdkPass = 0; sdkPass < 2; sdkPass++)
  {
    HMODULE dxilHandle = NULL;

    // first try normal plugin search path. This will prioritise any one placed locally with
    // RenderDoc, otherwise it will try just the unadorned dll in case it's in the PATH somewhere.
    {
      dxilHandle = (HMODULE)Process::LoadModule(LocatePluginFile("d3d12", "dxil.dll"));

      // dxc is very particular/brittle, so if we get dxil try to locate a dxcompiler right next to
      // it. Loading a different dxcompiler might produce a non-working compiler setup. If we can't,
      // we'll fall back to finding the next best dxcompiler we can
      if(dxilHandle)
      {
        wchar_t dxilPath[MAX_PATH + 1] = {};
        GetModuleFileNameW(dxilHandle, dxilPath, MAX_PATH);

        rdcstr path = StringFormat::Wide2UTF8(dxilPath);
        HMODULE dxcompiler = (HMODULE)Process::LoadModule(get_dirname(path) + "/dxcompiler.dll");
        if(dxcompiler)
        {
          ret = dxcompiler;
          return ret;
        }
      }

      // don't try to load dxcompiler.dll until we've got dxil.dll successfully, or if we're not
      // trying to get dxil. Otherwise we could load dxcompiler (to check for its existence) and
      // then find we can't get dxil and be stuck on pass 0.
      if(dxilHandle || sdkPass == 1)
      {
        HMODULE dxcompiler =
            (HMODULE)Process::LoadModule(LocatePluginFile("d3d12", "dxcompiler.dll"));
        if(dxcompiler)
        {
          ret = dxcompiler;
          return ret;
        }
      }

      // if we didn't find dxcompiler but did find dxil, somehow, then unload it
      if(dxilHandle)
        FreeLibrary(dxilHandle);
      dxilHandle = NULL;
    }

    // otherwise search windows SDK folders.
    // First use the registry to locate the SDK
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

        rdcstr path = StringFormat::Wide2UTF8(data);

        RegCloseKey(key);

        delete[] data;

        if(path.back() != '\\')
          path.push_back('\\');

        // next search the versioned bin folders, from newest to oldest
        path += "bin\\";

        // sort by name
        rdcarray<PathEntry> entries;
        FileIO::GetFilesInDirectory(path, entries);
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
            rdcstr dxilPath = path + e.filename + "\\x64\\dxil.dll";
            rdcstr dxcompilerPath = path + e.filename + "\\x64\\dxcompiler.dll";

            bool dxil = FileIO::exists(dxilPath);
            bool dxcompiler = FileIO::exists(dxcompilerPath);

            // if we have both, or we're on the second pass (given up on dxil.dll) and have
            // dxcompiler, then load this.
            if((dxil && dxcompiler) || (sdkPass == 1 && dxcompiler))
            {
              if(dxil)
                dxilHandle = (HMODULE)Process::LoadModule(dxilPath);
              ret = (HMODULE)Process::LoadModule(dxcompilerPath);
            }

            if(ret)
              return ret;

            // if we didn't find dxcompiler but did find dxil, somehow, then unload it
            if(dxilHandle)
              FreeLibrary(dxilHandle);
            dxilHandle = NULL;
          }
        }

        // try in the Redist folder
        {
          rdcstr dxilPath = path + "..\\Redist\\D3D\\x64\\dxil.dll";
          rdcstr dxcompilerPath = path + "..\\Redist\\D3D\\x64\\dxcompiler.dll";

          bool dxil = FileIO::exists(dxilPath);
          bool dxcompiler = FileIO::exists(dxcompilerPath);

          // if we have both, or we're on the second pass (given up on dxil.dll) and have
          // dxcompiler, then load this.
          if((dxil && dxcompiler) || (sdkPass == 1 && dxcompiler))
          {
            if(dxil)
              dxilHandle = (HMODULE)Process::LoadModule(dxilPath);
            ret = (HMODULE)Process::LoadModule(dxcompilerPath);
          }

          if(ret)
            return ret;

          // if we didn't find dxcompiler but did find dxil, somehow, then unload it
          if(dxilHandle)
            FreeLibrary(dxilHandle);
          dxilHandle = NULL;
        }

        // if we've gotten here and haven't returned anything, then try just the base x64 folder
        {
          rdcstr dxilPath = path + "x64\\dxil.dll";
          rdcstr dxcompilerPath = path + "x64\\dxcompiler.dll";

          bool dxil = FileIO::exists(dxilPath);
          bool dxcompiler = FileIO::exists(dxcompilerPath);

          // if we have both, or we're on the second pass (given up on dxil.dll) and have
          // dxcompiler, then load this.
          if((dxil && dxcompiler) || (sdkPass == 1 && dxcompiler))
          {
            if(dxil)
              dxilHandle = (HMODULE)Process::LoadModule(dxilPath);
            ret = (HMODULE)Process::LoadModule(dxcompilerPath);
          }

          if(ret)
            return ret;

          // if we didn't find dxcompiler but did find dxil, somehow, then unload it
          if(dxilHandle)
            FreeLibrary(dxilHandle);
          dxilHandle = NULL;
        }

        continue;
      }

      RegCloseKey(key);
    }
  }

  RDCERR("Couldn't find dxcompiler.dll in any path.");

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

    ret.Parameters.resize(desc->NumParameters);

    ret.dwordLength = 0;

    for(size_t i = 0; i < ret.Parameters.size(); i++)
    {
      ret.Parameters[i].MakeFrom(desc->pParameters[i], ret.maxSpaceIndex);

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
      ret.StaticSamplers.resize(desc->NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
      {
        ret.StaticSamplers[i] = Upconvert(desc->pStaticSamplers[i]);
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
      }
    }

    SAFE_RELEASE(deser);

    return ret;
  }

  ID3D12VersionedRootSignatureDeserializer *deser = NULL;
  HRESULT hr;

  if(m_DevConfig)
    hr = m_DevConfig->devconfig->CreateVersionedRootSignatureDeserializer(
        data, dataSize, __uuidof(ID3D12VersionedRootSignatureDeserializer), (void **)&deser);
  else
    hr = deserializeRootSig(data, dataSize, __uuidof(ID3D12VersionedRootSignatureDeserializer),
                            (void **)&deser);

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get deserializer");
    return D3D12RootSignature();
  }

  D3D12RootSignature ret;

  uint32_t version = 12;
  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *verdesc = NULL;
  hr = deser->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_2, &verdesc);
  if(FAILED(hr))
  {
    version = 11;
    hr = deser->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &verdesc);
  }

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get descriptor");
    return D3D12RootSignature();
  }

  const D3D12_ROOT_SIGNATURE_DESC1 *desc = &verdesc->Desc_1_1;

  ret.Flags = desc->Flags;

  ret.Parameters.resize(desc->NumParameters);

  ret.dwordLength = 0;

  for(size_t i = 0; i < ret.Parameters.size(); i++)
  {
    ret.Parameters[i].MakeFrom(desc->pParameters[i], ret.maxSpaceIndex);

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
    if(version >= 12)
    {
      ret.StaticSamplers.assign(verdesc->Desc_1_2.pStaticSamplers,
                                verdesc->Desc_1_2.NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
    }
    else
    {
      ret.StaticSamplers.resize(desc->NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
      {
        ret.StaticSamplers[i] = Upconvert(desc->pStaticSamplers[i]);
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
      }
    }
  }

  SAFE_RELEASE(deser);

  return ret;
}

ID3DBlob *D3D12ShaderCache::MakeRootSig(const rdcarray<D3D12_ROOT_PARAMETER1> &params,
                                        D3D12_ROOT_SIGNATURE_FLAGS Flags, UINT NumStaticSamplers,
                                        const D3D12_STATIC_SAMPLER_DESC1 *StaticSamplers)
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

    rdcarray<D3D12_STATIC_SAMPLER_DESC> oldSamplers;
    oldSamplers.resize(NumStaticSamplers);
    for(size_t i = 0; i < oldSamplers.size(); i++)
      oldSamplers[i] = Downconvert(StaticSamplers[i]);

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.Flags = Flags;
    desc.NumStaticSamplers = NumStaticSamplers;
    desc.pStaticSamplers = oldSamplers.data();
    desc.NumParameters = (UINT)params.size();

    rdcarray<D3D12_ROOT_PARAMETER> params_1_0;
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
      rdcstr errors = (char *)errBlob->GetBufferPointer();

      rdcstr logerror = errors;
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
  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_2;

  D3D12_ROOT_SIGNATURE_DESC2 &desc12 = verdesc.Desc_1_2;
  desc12.Flags = Flags;
  desc12.NumStaticSamplers = NumStaticSamplers;
  desc12.pStaticSamplers = StaticSamplers;
  desc12.NumParameters = (UINT)params.size();
  desc12.pParameters = &params[0];

  ID3DBlob *ret = NULL;
  ID3DBlob *errBlob = NULL;
  HRESULT hr;

  if(m_DevConfig && m_DevConfig->devconfig)
    hr = m_DevConfig->devconfig->SerializeVersionedRootSignature(&verdesc, &ret, &errBlob);
  else
    hr = serializeRootSig(&verdesc, &ret, &errBlob);
  SAFE_RELEASE(errBlob);

  if(SUCCEEDED(hr))
    return ret;

  // if it failed, try again at version 1.1
  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
  D3D12_ROOT_SIGNATURE_DESC1 &desc11 = verdesc.Desc_1_1;
  rdcarray<D3D12_STATIC_SAMPLER_DESC> oldSamplers;
  oldSamplers.resize(NumStaticSamplers);
  for(size_t i = 0; i < oldSamplers.size(); i++)
    oldSamplers[i] = Downconvert(StaticSamplers[i]);
  desc11.pStaticSamplers = oldSamplers.data();

  if(m_DevConfig && m_DevConfig->devconfig)
    hr = m_DevConfig->devconfig->SerializeVersionedRootSignature(&verdesc, &ret, &errBlob);
  else
    hr = serializeRootSig(&verdesc, &ret, &errBlob);

  if(FAILED(hr))
  {
    rdcstr errors = (char *)errBlob->GetBufferPointer();

    rdcstr logerror = errors;
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
  rdcarray<D3D12_ROOT_PARAMETER1> params;
  params.resize(rootsig.Parameters.size());
  for(size_t i = 0; i < params.size(); i++)
    params[i] = rootsig.Parameters[i];

  return MakeRootSig(params, rootsig.Flags, (UINT)rootsig.StaticSamplers.size(),
                     rootsig.StaticSamplers.empty() ? NULL : &rootsig.StaticSamplers[0]);
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

D3D12_STATIC_SAMPLER_DESC1 D3D12ShaderCache::Upconvert(const D3D12_STATIC_SAMPLER_DESC &StaticSampler)
{
  D3D12_STATIC_SAMPLER_DESC1 ret;
  memcpy(&ret, &StaticSampler, sizeof(StaticSampler));
  ret.Flags = D3D12_SAMPLER_FLAG_NONE;
  return ret;
}

D3D12_STATIC_SAMPLER_DESC D3D12ShaderCache::Downconvert(const D3D12_STATIC_SAMPLER_DESC1 &StaticSampler)
{
  D3D12_STATIC_SAMPLER_DESC ret;
  memcpy(&ret, &StaticSampler, sizeof(ret));
  if(StaticSampler.Flags != 0)
    RDCWARN("Downconverting sampler with advanced features set");
  if(ret.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT)
    ret.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT;
  else if(ret.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT)
    ret.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT;
  return ret;
}
