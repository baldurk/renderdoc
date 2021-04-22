/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Baldur Karlsson
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

#include "core/settings.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"
#include "d3d12_common.h"

RDOC_CONFIG(rdcstr, D3D12_D3D12CorePath, "",
            "The location of the D3D12Core library. This path should be the directory that "
            "contains the D3D12Core.dll that you want to use.");

// special hooking functions exposed on windows for hooking while in replay mode
void Win32_RegisterManualModuleHooking();
void Win32_InterceptLibraryLoads(std::function<HMODULE(const rdcstr &, HANDLE, DWORD)> callback);
void Win32_ManualHookModule(rdcstr modName, HMODULE module);

namespace
{
DWORD SystemCoreVersion;
rdcstr D3D12Core_Override_Path;
rdcstr D3D12Core_Temp_Path;
};

MIDL_INTERFACE("DFAFDD2C-355F-4CB3-A8B2-EA7F9260148B")
ID3D12CoreModule : public IUnknown
{
public:
  virtual DWORD STDMETHODCALLTYPE LOEnter(void) = 0;
  virtual DWORD STDMETHODCALLTYPE LOLeave(void) = 0;
  virtual DWORD STDMETHODCALLTYPE LOTryEnter(void) = 0;
  virtual HRESULT STDMETHODCALLTYPE Initialize(DWORD, LPCSTR) = 0;
  virtual DWORD STDMETHODCALLTYPE GetSDKVersion(void) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetDllExports(void *) = 0;
};

struct WrappedCoreModule : public ID3D12CoreModule
{
  unsigned int m_iRefcount = 0;
  ID3D12CoreModule *m_pReal = NULL;

  WrappedCoreModule(ID3D12CoreModule *real) : m_iRefcount(1), m_pReal(real) {}
  ~WrappedCoreModule() { SAFE_RELEASE(m_pReal); }
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0)
      delete this;
    return ret;
  }
  //////////////////////////////
  // implement ID3D12CoreModule
  virtual DWORD STDMETHODCALLTYPE LOEnter(void) { return m_pReal->LOEnter(); }
  virtual DWORD STDMETHODCALLTYPE LOLeave(void) { return m_pReal->LOLeave(); }
  virtual DWORD STDMETHODCALLTYPE LOTryEnter(void) { return m_pReal->LOTryEnter(); }
  virtual HRESULT STDMETHODCALLTYPE Initialize(DWORD version, LPCSTR unknown)
  {
    // D3D12 should always think it's loading the system core version
    RDCASSERT(version == SystemCoreVersion);

    // initialize the actual core (which might in some cases be the system version) with its own
    // version so it doesn't complain
    return m_pReal->Initialize(m_pReal->GetSDKVersion(), unknown);
  }

  virtual DWORD STDMETHODCALLTYPE GetSDKVersion(void) { return SystemCoreVersion; }
  virtual HRESULT STDMETHODCALLTYPE GetDllExports(void *a) { return m_pReal->GetDllExports(a); }
};

using PFN_D3D12_GET_INTERFACE = decltype(&D3D12GetInterface);
HookedFunction<PFN_D3D12_GET_INTERFACE> D3D12GetInterface_hook;

HMODULE Hooked_D3D12LoadLibrary(const rdcstr &filename, HANDLE h, DWORD flags)
{
  // if we detect the intercepted call to D3D12Core.dll and we have a redirect path, load that
  // redirect path instead
  if(strlower(filename).contains("d3d12core.dll") && !D3D12Core_Override_Path.empty())
    return LoadLibraryExW(StringFormat::UTF82Wide(D3D12Core_Override_Path).c_str(), NULL, flags);

  return NULL;
}

HRESULT WINAPI Hooked_D3D12GetInterface(_In_ REFCLSID rclsid, _In_ REFIID riid,
                                        _COM_Outptr_opt_ void **ppvDebug)
{
  HRESULT ret = D3D12GetInterface_hook()(rclsid, riid, ppvDebug);

  // intercept the interface with our own wrapper to ensure version checking is silenced
  if(SUCCEEDED(ret) && riid == __uuidof(ID3D12CoreModule))
    *ppvDebug = (ID3D12CoreModule *)(new WrappedCoreModule((ID3D12CoreModule *)*ppvDebug));

  return ret;
}

void D3D12_PrepareReplaySDKVersion(UINT SDKVersion, bytebuf d3d12core_file, HMODULE d3d12lib)
{
  // D3D12Core shouldn't be loaded at this point, but it might be due to bugs. If it is, we can't do
  // anything to change it anymore so we have to just handle what we have
  HMODULE D3D12Core = GetModuleHandleA("D3D12Core.dll");
  if(D3D12Core != NULL)
  {
    DWORD *ver_ptr = (DWORD *)GetProcAddress(D3D12Core, "D3D12SDKVersion");

    // if the core that's loaded is sufficient, don't show any warnings
    if(ver_ptr && SDKVersion <= *ver_ptr)
      return;

    RDCWARN(
        "D3D12Core.dll was already loaded before replay started. This may be caused by a D3D12 "
        "runtime bug if the validation layers are enabled, that means D3D12 is never unloaded.");

    if(ver_ptr)
      RDCWARN("The existing D3D12Core.dll is version %u but this capture requires version %u",
              *ver_ptr, SDKVersion);
    else
      RDCWARN("The existing D3D12Core.dll had an unknown version, this capture requires version %u",
              SDKVersion);

    return;
  }

  static bool core_fetched = false, hooks_applied = false;

  // if we don't have the system core version yet
  if(!core_fetched)
  {
    core_fetched = true;

    // default to 0
    SystemCoreVersion = 0;

    // get the system path to load it explicitly
    wchar_t sys_path[MAX_PATH + 16] = {};
    GetSystemDirectoryW(sys_path, MAX_PATH);

    wcscat_s(sys_path, L"\\D3D12Core.dll");

    HMODULE real_sys = LoadLibraryW(sys_path);
    if(real_sys)
    {
      DWORD *ver_ptr = (DWORD *)GetProcAddress(real_sys, "D3D12SDKVersion");

      if(ver_ptr)
        SystemCoreVersion = *ver_ptr;
      else
        RDCERR("D3D12Core.dll loaded from %ls doesn't have D3D12SDKVersion export!", sys_path);

      FreeLibrary(real_sys);

      RDCLOG("System D3D12 runtime is version %u", SystemCoreVersion);
    }
    else
    {
      RDCLOG("No system D3D12 runtime found at %ls.", sys_path);

      // if the captured SDK version was greater than 1 then most likely this capture will fail to
      // replay, but we can still try to replay (there's no guarantee a user actually used features
      // exclusive to the new runtime).
      if(SDKVersion > 1)
        RDCWARN(
            "Capture was made with runtime version %u but this system does not support D3D12 "
            "runtimes, possible incompatibility");
    }
  }

  // if the system doesn't have a core DLL we can't intercept and point to our own runtime, so just
  // abort here before doing anything potentially dangerous below.
  if(SystemCoreVersion == 0)
    return;

  // similarly, if the system version is enough then the user didn't use a new runtime (or they used
  // what was at the time a new runtime but is now available in the system...), so also abort.
  // That means we'll only do the interception & patching when we think it's really needed.
  if(SDKVersion <= SystemCoreVersion)
    return;

  // finally we're at a point where we will hook to force the library we want.

  if(!hooks_applied)
  {
    Win32_RegisterManualModuleHooking();

    D3D12GetInterface_hook.Register("d3d12core.dll", "D3D12GetInterface", Hooked_D3D12GetInterface);

    Win32_InterceptLibraryLoads(Hooked_D3D12LoadLibrary);
  }

  // we do this always, even if the hooks are already applied, because this module has possibly been
  // reloaded and needs to be re-hooked each time
  Win32_ManualHookModule("d3d12.dll", d3d12lib);

  // *always* use the user's path if it exists
  D3D12Core_Override_Path = D3D12_D3D12CorePath();

  if(D3D12Core_Override_Path.empty() || !FileIO::exists(D3D12Core_Override_Path.c_str()))
  {
    if(d3d12core_file.empty())
    {
      RDCERR(
          "No D3D12Core.dll embedded in capture but we need a newer one (version %u) to properly "
          "replay this capture",
          SDKVersion);
      return;
    }

    // find an appropriate spot to write this file. Other instances of RenderDoc might be running so
    // we try a few different  variants
    for(uint32_t i = 0; i < 32; i++)
    {
      rdcstr filename = StringFormat::Fmt("%s/RenderDoc/D3D12Core/%u.ver%u/D3D12Core.dll",
                                          FileIO::GetTempFolderFilename().c_str(), i, SDKVersion);

      FileIO::CreateParentDirectory(filename);

      FILE *f = FileIO::fopen(filename.c_str(), FileIO::WriteBinary);

      // if we can write to this file, we have exclusive use of it so let's write it and use it
      if(f)
      {
        FileIO::fwrite(d3d12core_file.data(), 1, d3d12core_file.size(), f);
        FileIO::fclose(f);

        D3D12Core_Override_Path = filename;
        D3D12Core_Temp_Path = filename;
        break;
      }
    }

    if(D3D12Core_Override_Path.empty() || !FileIO::exists(D3D12Core_Override_Path.c_str()))
    {
      RDCERR("Couldn't write embedded D3D12Core.dll to disk! system dll will be used");
    }
  }
}

void D3D12_CleanupReplaySDK()
{
  if(!D3D12Core_Temp_Path.empty() && FileIO::exists(D3D12Core_Temp_Path.c_str()))
  {
    FileIO::Delete(D3D12Core_Temp_Path.c_str());
  }
}
