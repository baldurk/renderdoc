/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include <wincrypt.h>
#include "core/settings.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"
#include "tinyfiledialogs/tinyfiledialogs.h"
#include "d3d12_common.h"

RDOC_CONFIG(rdcstr, D3D12_D3D12CoreDirPath, "",
            "The location of the D3D12Core library. This path should be the directory that "
            "contains the D3D12Core.dll that you want to use.");
RDOC_CONFIG(bool, D3D12_Debug_IgnoreSignatureCheck, false,
            "Whether to ignore digital signature check for dll's embedded in capture file");

// special hooking functions exposed on windows for hooking while in replay mode
void Win32_RegisterManualModuleHooking();
void Win32_InterceptLibraryLoads(std::function<HMODULE(const rdcstr &, HANDLE, DWORD)> callback);
void Win32_ManualHookModule(rdcstr modName, HMODULE module);

namespace
{
DWORD SystemCoreVersion;
rdcstr D3D12Core_Override_Path;
rdcstr D3D12Core_Temp_Path;
};    // namespace

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
HookedFunction<PFN_D3D12_GET_INTERFACE> D3D12GetInterface_Core_hook;
HookedFunction<PFN_D3D12_GET_INTERFACE> D3D12GetInterface_SDKLayers_hook;

HMODULE Hooked_D3D12LoadLibrary(const rdcstr &filename, HANDLE h, DWORD flags)
{
  if(!D3D12Core_Override_Path.empty())
  {
    // if we detect the intercepted call to a D3D12 dll and we have a redirect path, load that
    // redirect path instead
    for(const rdcstr &dll : {"d3d12core.dll", "d3d12sdklayers.dll"})
    {
      if(strlower(filename).contains(dll))
      {
        HMODULE ret = LoadLibraryExW(
            StringFormat::UTF82Wide(D3D12Core_Override_Path + "/" + dll).c_str(), NULL, flags);

        if(ret)
        {
          Win32_ManualHookModule(dll, ret);
        }
        else
        {
          RDCERR("Error loading %s from %s", dll.c_str(), D3D12Core_Override_Path.c_str());
        }

        return ret;
      }
    }
  }

  return NULL;
}

HRESULT WINAPI Hooked_Core_D3D12GetInterface(_In_ REFCLSID rclsid, _In_ REFIID riid,
                                             _COM_Outptr_opt_ void **ppvDebug)
{
  HRESULT ret = D3D12GetInterface_Core_hook()(rclsid, riid, ppvDebug);

  // intercept the interface with our own wrapper to ensure version checking is silenced
  if(SUCCEEDED(ret) && riid == __uuidof(ID3D12CoreModule))
    *ppvDebug = (ID3D12CoreModule *)(new WrappedCoreModule((ID3D12CoreModule *)*ppvDebug));

  return ret;
}

HRESULT WINAPI Hooked_SDKLayers_D3D12GetInterface(_In_ REFCLSID rclsid, _In_ REFIID riid,
                                                  _COM_Outptr_opt_ void **ppvDebug)
{
  HRESULT ret = D3D12GetInterface_SDKLayers_hook()(rclsid, riid, ppvDebug);

  // intercept the interface with our own wrapper to ensure version checking is silenced
  if(SUCCEEDED(ret) && riid == __uuidof(ID3D12CoreModule))
    *ppvDebug = (ID3D12CoreModule *)(new WrappedCoreModule((ID3D12CoreModule *)*ppvDebug));

  return ret;
}

// Can check signatures of .exe and .dll files
bool IsSignedByMicrosoft(const rdcstr &filename, rdcstr &signer)
{
  bool IsSignatureValid = false;

  typedef BOOL(WINAPI * PFN_CRYPTQUERYOBJECT)(DWORD, const void *, DWORD, DWORD, DWORD, DWORD *,
                                              DWORD *, DWORD *, HCERTSTORE *, HCRYPTMSG *,
                                              const void **);
  typedef BOOL(WINAPI * PFN_CRYPTMSGGETPARAM)(HCRYPTMSG, DWORD, DWORD, void *, DWORD *);
  typedef PCCERT_CONTEXT(WINAPI * PFN_CERTFINDCERTIFICATEINSTORE)(HCERTSTORE, DWORD, DWORD, DWORD,
                                                                  const void *, PCCERT_CONTEXT);
  typedef DWORD(WINAPI * PFN_CERTGETNAMESTRINGW)(PCCERT_CONTEXT, DWORD, DWORD, void *, LPWSTR, DWORD);
  typedef BOOL(WINAPI * PFN_CERTFREECERTIFICATECONTEXT)(PCCERT_CONTEXT);
  typedef BOOL(WINAPI * PFN_CERTCLOSESTORE)(HCERTSTORE, DWORD);
  typedef BOOL(WINAPI * PFN_CRYPTMSGCLOSE)(HCRYPTMSG);

  HMODULE crypt32 = NULL;
  PFN_CRYPTQUERYOBJECT CryptQueryObject = NULL;
  PFN_CRYPTMSGGETPARAM CryptMsgGetParam = NULL;
  PFN_CERTFINDCERTIFICATEINSTORE CertFindCertificateInStore = NULL;
  PFN_CERTGETNAMESTRINGW CertGetNameStringW = NULL;
  PFN_CERTFREECERTIFICATECONTEXT CertFreeCertificateContext = NULL;
  PFN_CERTCLOSESTORE CertCloseStore = NULL;
  PFN_CRYPTMSGCLOSE CryptMsgClose = NULL;

  HCERTSTORE store = NULL;
  HCRYPTMSG msg = NULL;
  PCMSG_SIGNER_INFO signer_info = NULL;
  DWORD signer_info_size = 0;
  PCCERT_CONTEXT cert_context = NULL;
  CERT_INFO cert_info = {};
  LPWSTR signer_name = NULL;
  DWORD signer_name_length = 0;

  do
  {
    crypt32 = LoadLibraryA("crypt32.dll");

    if(!crypt32)
    {
      break;
    }

    CryptQueryObject = (PFN_CRYPTQUERYOBJECT)GetProcAddress(crypt32, "CryptQueryObject");
    CryptMsgGetParam = (PFN_CRYPTMSGGETPARAM)GetProcAddress(crypt32, "CryptMsgGetParam");
    CertFindCertificateInStore =
        (PFN_CERTFINDCERTIFICATEINSTORE)GetProcAddress(crypt32, "CertFindCertificateInStore");
    CertGetNameStringW = (PFN_CERTGETNAMESTRINGW)GetProcAddress(crypt32, "CertGetNameStringW");
    CertFreeCertificateContext =
        (PFN_CERTFREECERTIFICATECONTEXT)GetProcAddress(crypt32, "CertFreeCertificateContext");
    CertCloseStore = (PFN_CERTCLOSESTORE)GetProcAddress(crypt32, "CertCloseStore");
    CryptMsgClose = (PFN_CRYPTMSGCLOSE)GetProcAddress(crypt32, "CryptMsgClose");

    if(!CryptQueryObject || !CryptMsgGetParam || !CertFindCertificateInStore ||
       !CertGetNameStringW || !CertFreeCertificateContext || !CertCloseStore || !CryptMsgClose)
    {
      break;
    }

    rdcwstr wideFilename = StringFormat::UTF82Wide(filename);

    // Get message handle and store handle from the signed file.
    BOOL res = CryptQueryObject(
        CERT_QUERY_OBJECT_FILE, wideFilename.c_str(), CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY, 0, NULL, NULL, NULL, &store, &msg, NULL);

    if(!res)
    {
      break;
    }

    // Get signer information size.
    res = CryptMsgGetParam(msg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &signer_info_size);
    if(!res)
    {
      break;
    }

    // Allocate memory for signer information.
    signer_info = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, signer_info_size);
    if(!signer_info)
    {
      break;
    }

    // Get Signer Information.
    res = CryptMsgGetParam(msg, CMSG_SIGNER_INFO_PARAM, 0, (PVOID)signer_info, &signer_info_size);
    if(!res)
    {
      break;
    }

    cert_info.Issuer = signer_info->Issuer;
    cert_info.SerialNumber = signer_info->SerialNumber;

    // Get Certificate handle
    cert_context = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                                              CERT_FIND_SUBJECT_CERT, (PVOID)&cert_info, NULL);
    if(!cert_context)
    {
      break;
    }

    // Get Subject name size.
    signer_name_length =
        CertGetNameStringW(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);
    if(!signer_name_length)
    {
      break;
    }

    // Allocate memory for subject name.
    signer_name = (LPWSTR)LocalAlloc(LPTR, signer_name_length * sizeof(WCHAR));
    if(!signer_name)
    {
      break;
    }

    // Get subject name.
    if(!CertGetNameStringW(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, signer_name,
                           signer_name_length))
    {
      break;
    }

    signer = StringFormat::Wide2UTF8(rdcwstr(signer_name));

    // Check whether signer is Microsoft
    // Since Microsoft uses multiple different signatures,
    // We just check whether "Microsoft" is a substring of signer simple name
    // Nobody except Microsoft should ever have such signature
    if(wcsstr(signer_name, L"Microsoft") == NULL)
    {
      break;
    }

    IsSignatureValid = true;

  } while(0);

  if(signer_info != NULL)
    LocalFree(signer_info);
  if(cert_context != NULL)
    CertFreeCertificateContext(cert_context);
  if(store != NULL)
    CertCloseStore(store, 0);
  if(msg != NULL)
    CryptMsgClose(msg);
  if(signer_name != NULL)
    LocalFree(signer_name);
  if(crypt32)
    FreeLibrary(crypt32);

  return IsSignatureValid;
}

D3D12DevConfiguration *D3D12_PrepareReplaySDKVersion(bool untrustedCapture, UINT SDKVersion,
                                                     bytebuf d3d12core_file,
                                                     bytebuf d3d12sdklayers_file, HMODULE d3d12lib)
{
  // D3D12Core shouldn't be loaded at this point, but it might be due to bugs. If it is, we don't do
  // anything to change it anymore so we have to just handle what we have.
  // In theory it might be possible to load multiple d3d12cores using the new dll selection API, but
  // that's probably not stable/reliable so we don't use it.
  HMODULE D3D12Core = GetModuleHandleA("D3D12Core.dll");
  if(D3D12Core != NULL)
  {
    DWORD *ver_ptr = (DWORD *)GetProcAddress(D3D12Core, "D3D12SDKVersion");

    // if the core that's loaded is sufficient, don't show any warnings
    if(ver_ptr && SDKVersion <= *ver_ptr)
      return NULL;

    RDCWARN(
        "D3D12Core.dll was already loaded before replay started. This may be caused by a D3D12 "
        "runtime bug if the validation layers are enabled, that means D3D12 is never unloaded.");

    if(ver_ptr)
      RDCWARN("The existing D3D12Core.dll is version %u but this capture requires version %u",
              *ver_ptr, SDKVersion);
    else
      RDCWARN("The existing D3D12Core.dll had an unknown version, this capture requires version %u",
              SDKVersion);

    return NULL;
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
    return NULL;

  // similarly, if the system version is enough then the user didn't use a new runtime (or they used
  // what was at the time a new runtime but is now available in the system...), so also abort.
  // That means we'll only do the interception & patching when we think it's really needed.
  // The only exception is if the user has configured a force override, in which case we always use
  // it.
  if(SDKVersion <= SystemCoreVersion && D3D12_D3D12CoreDirPath().empty())
    return NULL;

  // *always* use the user's path if it exists
  D3D12Core_Override_Path = D3D12_D3D12CoreDirPath();

  DWORD OverrideDllVersion = 0;

  if(D3D12Core_Override_Path.empty() || !FileIO::exists(D3D12Core_Override_Path.c_str()))
  {
    if(d3d12core_file.empty())
    {
      RDCERR(
          "No D3D12Core.dll embedded in capture but we need a newer one (version %u) to properly "
          "replay this capture",
          SDKVersion);
      return NULL;
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

        rdcstr signer;
        // trusted captures (i.e. not marked as downloaded from the internet by windows) will skip
        // this check entirely. Untrusted captures will verify the DLL signature is Microsoft signed
        if(untrustedCapture && !IsSignedByMicrosoft(filename, signer))
        {
          if(D3D12_Debug_IgnoreSignatureCheck())
          {
            RDCWARN(
                "Can't verify the digital signature of the D3D12Core.dll embedded in capture, it "
                "will be loaded since D3D12.Debug.IgnoreSignatureCheck is set to true");
          }
          else
          {
            RDCLOG("D3D12Core signed by '%s' instead of MS", signer.c_str());

            rdcstr msg =
                "Capture file contains an embedded D3D12 dll which is not correctly signed by "
                "Microsoft.\n\n";
            if(signer.empty())
              msg += "There is no signature at all.\n\n";
            else
              msg += "The file is signed by '" + signer + "'.\n\n";

            msg +=
                "If you want to load the capture anyway, click yes. To use the system version of "
                "D3D12 click no.";

            int res = tinyfd_messageBox("Unexpected DLL signature", msg.c_str(), "yesnocancel",
                                        "error", 2);
            // 1 == yes, either no or cancel will abort the load
            if(res != 1)
            {
              FileIO::Delete(filename);
              RDCERR(
                  "Can't verify the digital signature of the D3D12Core.dll embedded in capture, it "
                  "won't be loaded. If the capture came from a trusted source and you want to load "
                  "unsigned dll's, set D3D12.Debug.IgnoreSignatureCheck to true");
              break;
            }

            RDCLOG("User selected to continue with load.");
          }
        }

        rdcstr sdklayers_filename = get_dirname(filename) + "/d3d12sdklayers.dll";

        f = FileIO::fopen(sdklayers_filename.c_str(), FileIO::WriteBinary);

        // if we can write to this file, we have exclusive use of it so let's write it and use it
        if(f)
        {
          FileIO::fwrite(d3d12sdklayers_file.data(), 1, d3d12sdklayers_file.size(), f);
          FileIO::fclose(f);
        }

// d3d12sdklayers.dll is not always signed
#if 0
        if(!IsSignedByMicrosoft(sdklayers_filename))
        {
          if(D3D12_Debug_IgnoreSignatureCheck())
          {
            RDCWARN(
                "Can't verify digital signature of d3d12sdklayers.dll embedded in capture, it will "
                "be loaded since D3D12.Debug.IgnoreSignatureCheck is set to true");
          }
          else
          {
            RDCERR(
                "Can't verify digital signature of d3d12sdklayers.dll embedded in capture, it "
                "won't be loaded. If capture came from trusted source you want to load unsigned "
                "dll's set D3D12.Debug.IgnoreSignatureCheck to true");
            FileIO::Delete(filename);
            FileIO::Delete(sdklayers_filename);
            break;
          }
        }
#endif

        D3D12Core_Override_Path = get_dirname(filename);
        D3D12Core_Temp_Path = get_dirname(filename);

        break;
      }
    }

    if(D3D12Core_Override_Path.empty() || !FileIO::exists(D3D12Core_Override_Path.c_str()))
    {
      RDCERR("Couldn't write embedded D3D12Core.dll to disk! system dll will be used");
    }
    else
    {
      UINT prevErrorMode = GetErrorMode();
      SetErrorMode(prevErrorMode | SEM_FAILCRITICALERRORS);
      HMODULE ret =
          LoadLibraryW(StringFormat::UTF82Wide(D3D12Core_Override_Path + "/d3d12core.dll").c_str());

      SetErrorMode(prevErrorMode);

      if(ret == NULL)
      {
        RDCERR("Can't open DLL! Wrong architecture or incompatible? system dll will be used");
        D3D12Core_Override_Path.clear();
      }

      FreeLibrary(ret);
    }
  }

  if(FileIO::exists(D3D12Core_Override_Path.c_str()))
  {
    UINT prevErrorMode = GetErrorMode();
    SetErrorMode(prevErrorMode | SEM_FAILCRITICALERRORS);
    HMODULE ret =
        LoadLibraryW(StringFormat::UTF82Wide(D3D12Core_Override_Path + "/d3d12core.dll").c_str());

    SetErrorMode(prevErrorMode);

    DWORD *ver_ptr = (DWORD *)GetProcAddress(ret, "D3D12SDKVersion");

    if(ver_ptr)
      OverrideDllVersion = *ver_ptr;

    FreeLibrary(ret);
  }

  RDCLOG("Loading D3D12 runtime from %s which is version %u", D3D12Core_Override_Path.c_str(),
         OverrideDllVersion);

  // see if we can use the new proper D3D12 dll selection API
  {
    HMODULE d3d12Lib = GetModuleHandleA("d3d12.dll");
    PFN_D3D12_GET_INTERFACE getD3D12Interface = NULL;

    if(d3d12Lib)
      getD3D12Interface = (PFN_D3D12_GET_INTERFACE)GetProcAddress(d3d12Lib, "D3D12GetInterface");

    if(getD3D12Interface)
    {
      ID3D12SDKConfiguration *config = NULL;
      HRESULT hr = getD3D12Interface(CLSID_D3D12SDKConfiguration, __uuidof(ID3D12SDKConfiguration),
                                     (void **)&config);

      if(SUCCEEDED(hr) && config)
      {
        ID3D12SDKConfiguration1 *config1 = NULL;
        ID3D12DeviceFactory *devfactory = NULL;
        hr = config->QueryInterface(__uuidof(ID3D12SDKConfiguration1), (void **)&config1);
        if(SUCCEEDED(hr) && config1)
        {
          config1->CreateDeviceFactory(OverrideDllVersion, D3D12Core_Override_Path.c_str(),
                                       __uuidof(ID3D12DeviceFactory), (void **)&devfactory);
        }
        SAFE_RELEASE(config);

        if(devfactory)
        {
          ID3D12Debug *debug = NULL;
          hr = devfactory->GetConfigurationInterface(CLSID_D3D12Debug, __uuidof(ID3D12Debug),
                                                     (void **)&debug);
          if(FAILED(hr))
            SAFE_RELEASE(debug);
          ID3D12DeviceConfiguration *devConfig = NULL;
          hr = devfactory->QueryInterface(__uuidof(ID3D12DeviceConfiguration), (void **)&devConfig);
          if(FAILED(hr))
            SAFE_RELEASE(devConfig);

          // we got what we need, return the interfaces to use
          D3D12DevConfiguration *ret = new D3D12DevConfiguration;
          ret->devfactory = devfactory;
          ret->sdkconfig = config1;
          ret->debug = debug;
          ret->devconfig = devConfig;

          RDCLOG("Accessing D3D12 dll via SDK configuration API");

          return ret;
        }
        else
        {
          RDCLOG("Couldn't get device factory");
        }

        SAFE_RELEASE(config1);
      }
      else
      {
        RDCLOG("Couldn't get SDK configuration interface");
      }
      SAFE_RELEASE(config);
    }
    else
    {
      RDCLOG("Couldn't get D3D12 interface query");
    }
  }

  RDCLOG("Accessing D3D12 dll via hooks");

  // finally we're at a point where we will hook to force the library we want.

  if(!hooks_applied)
  {
    hooks_applied = true;

    Win32_RegisterManualModuleHooking();

    D3D12GetInterface_Core_hook.Register("d3d12core.dll", "D3D12GetInterface",
                                         Hooked_Core_D3D12GetInterface);
    D3D12GetInterface_SDKLayers_hook.Register("d3d12sdklayers.dll", "D3D12GetInterface",
                                              Hooked_SDKLayers_D3D12GetInterface);

    Win32_InterceptLibraryLoads(Hooked_D3D12LoadLibrary);
  }

  // we do this always, even if the hooks are already applied, because this module has possibly been
  // reloaded and needs to be re-hooked each time
  Win32_ManualHookModule("d3d12.dll", d3d12lib);

  return NULL;
}

void D3D12_CleanupReplaySDK()
{
  if(!D3D12Core_Temp_Path.empty() && FileIO::exists(D3D12Core_Temp_Path.c_str()))
  {
    FileIO::Delete((D3D12Core_Temp_Path + "/d3d12core.dll").c_str());
    FileIO::Delete((D3D12Core_Temp_Path + "/d3d12sdklayers.dll").c_str());
  }
}
