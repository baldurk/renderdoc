/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "driver/d3d11/d3d11_device.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/ihv/amd/official/DXExt/AmdDxExtApi.h"
#include "hooks/hooks.h"

#define DLL_NAME "d3d11.dll"

ID3DDevice *GetD3D11DeviceIfAlloc(IUnknown *dev)
{
  if(WrappedID3D11Device::IsAlloc(dev))
    return (WrappedID3D11Device *)dev;

  return NULL;
}

enum NVAPI_DEVICE_FEATURE_LEVEL
{
  dummy = 0,
};

typedef void *(__cdecl *PFNNVQueryInterface)(uint32_t ID);
typedef HRESULT (*PFNNVCreateDevice)(_In_opt_ IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
                                     _In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL *,
                                     UINT FeatureLevels, UINT, _COM_Outptr_opt_ ID3D11Device **,
                                     _Out_opt_ D3D_FEATURE_LEVEL *,
                                     _COM_Outptr_opt_ ID3D11DeviceContext **,
                                     NVAPI_DEVICE_FEATURE_LEVEL *);
typedef HRESULT (*PFNNVCreateDeviceAndSwapChain)(
    _In_opt_ IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
    _In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL *, UINT FeatureLevels, UINT,
    _In_opt_ CONST DXGI_SWAP_CHAIN_DESC *, _COM_Outptr_opt_ IDXGISwapChain **,
    _COM_Outptr_opt_ ID3D11Device **, _Out_opt_ D3D_FEATURE_LEVEL *,
    _COM_Outptr_opt_ ID3D11DeviceContext **, NVAPI_DEVICE_FEATURE_LEVEL *);

class D3D11Hook : LibraryHook
{
public:
  D3D11Hook()
  {
    LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
    m_HasHooks = false;
    m_EnabledHooks = true;
    m_InsideCreate = false;
  }

  bool CreateHooks(const char *libName)
  {
    bool success = true;

    WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

    // also require d3dcompiler_??.dll
    if(GetD3DCompiler() == NULL)
    {
      RDCERR("Failed to load d3dcompiler_??.dll - not inserting D3D11 hooks.");
      return false;
    }

    success &= CreateDevice.Initialize("D3D11CreateDevice", DLL_NAME, D3D11CreateDevice_hook);
    success &= CreateDeviceAndSwapChain.Initialize("D3D11CreateDeviceAndSwapChain", DLL_NAME,
                                                   D3D11CreateDeviceAndSwapChain_hook);

// these are not required for success, but opportunistic to prevent AMD extensions from
// activating and causing later crashes when not replayed correctly
#if ENABLED(RDOC_X64)
    AmdCreate11.Initialize("AmdDxExtCreate11", "atidxx64.dll", AmdCreate11_hook);
#else
    AmdCreate11.Initialize("AmdDxExtCreate11", "atidxx32.dll", AmdCreate11_hook);
#endif

// nvapi has some seriously awful ""feature"" where it can wrap CreateDevice with some other
// extra parameter. Since we don't want to hook nvapi when it calls out to d3d11.dll, we have to
// intercept it here.
#if ENABLED(RDOC_X64)
    nvapi_QueryInterface.Initialize("nvapi_QueryInterface", "nvapi64.dll", nvapi_QueryInterface_hook);
#else
    nvapi_QueryInterface.Initialize("nvapi_QueryInterface", "nvapi.dll", nvapi_QueryInterface_hook);
#endif

    if(!success)
      return false;

    m_HasHooks = true;
    m_EnabledHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
  bool UseHooks() { return (d3d11hooks.m_HasHooks && d3d11hooks.m_EnabledHooks); }
  static HRESULT CreateWrappedDeviceAndSwapChain(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
      __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    return d3d11hooks.Create_Internal(pAdapter, DriverType, Software, Flags, pFeatureLevels,
                                      FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain,
                                      ppDevice, pFeatureLevel, ppImmediateContext);
  }

private:
  static D3D11Hook d3d11hooks;

  bool m_HasHooks;
  bool m_EnabledHooks;

  Hook<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN> CreateDeviceAndSwapChain;
  Hook<PFN_D3D11_CREATE_DEVICE> CreateDevice;

  // optional extension hooks
  Hook<PFNAmdDxExtCreate11> AmdCreate11;
  Hook<PFNNVQueryInterface> nvapi_QueryInterface;

  // re-entrancy detection (can happen in rare cases with e.g. fraps)
  bool m_InsideCreate;

  HRESULT Create_Internal(__in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType,
                          HMODULE Software, UINT Flags,
                          __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                          UINT FeatureLevels, UINT SDKVersion,
                          __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                          __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
                          __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                          __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    // if we're already inside a wrapped create i.e. this function, then DON'T do anything
    // special. Just grab the trampolined function and call it.
    if(m_InsideCreate)
    {
      PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createFunc = NULL;

      // shouldn't ever get in here if we're in the case without hooks but let's be safe.
      if(m_HasHooks)
      {
        createFunc = CreateDeviceAndSwapChain();
      }
      else
      {
        HMODULE d3d11 = GetModuleHandleA("d3d11.dll");

        if(d3d11)
        {
          createFunc = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
              d3d11, "D3D11CreateDeviceAndSwapChain");
        }
        else
        {
          RDCERR("Something went seriously wrong, d3d11.dll couldn't be loaded!");
          return E_UNEXPECTED;
        }
      }

      return createFunc(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                        SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel,
                        ppImmediateContext);
    }

    m_InsideCreate = true;

    RDCDEBUG("Call to Create_Internal Flags %x", Flags);

    bool reading = RenderDoc::Inst().IsReplayApp();

    if(reading)
    {
      RDCDEBUG("In replay app");
    }

    if(m_EnabledHooks)
    {
      if(!reading && RenderDoc::Inst().GetCaptureOptions().APIValidation)
      {
        Flags |= D3D11_CREATE_DEVICE_DEBUG;
      }
      else
      {
        Flags &= ~D3D11_CREATE_DEVICE_DEBUG;
      }
    }

    DXGI_SWAP_CHAIN_DESC swapDesc;
    DXGI_SWAP_CHAIN_DESC *pUsedSwapDesc = NULL;

    if(pSwapChainDesc)
    {
      swapDesc = *pSwapChainDesc;
      pUsedSwapDesc = &swapDesc;
    }

    if(pUsedSwapDesc && m_EnabledHooks && !RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
    {
      pUsedSwapDesc->Windowed = TRUE;
    }

    RDCDEBUG("Calling real createdevice...");

    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createFunc =
        (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(GetModuleHandleA("d3d11.dll"),
                                                               "D3D11CreateDeviceAndSwapChain");

    // shouldn't ever get here, we should either have it from procaddress or the trampoline, but
    // let's be
    // safe.
    if(createFunc == NULL)
    {
      RDCERR("Something went seriously wrong with the hooks!");

      m_InsideCreate = false;

      return E_UNEXPECTED;
    }

    // Hack for D3DGear which crashes if ppDevice is NULL
    ID3D11Device *dummydev = NULL;
    bool dummyUsed = false;
    if(ppDevice == NULL)
    {
      ppDevice = &dummydev;
      dummyUsed = true;
    }

    HRESULT ret =
        createFunc(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                   pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

    SAFE_RELEASE(dummydev);
    if(dummyUsed)
      ppDevice = NULL;

    RDCDEBUG("Called real createdevice...");

    bool suppress = false;

    suppress = (Flags & D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY) != 0;

    if(suppress && !reading)
    {
      RDCDEBUG("Application requested not to be hooked.");
    }
    else if(SUCCEEDED(ret) && m_EnabledHooks && ppDevice)
    {
      WrapDevice(ppDevice, DriverType, Flags, SDKVersion, FeatureLevels, pFeatureLevels,
                 ppImmediateContext, ppSwapChain, pSwapChainDesc);
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D11 device.");
    }
    else
    {
      RDCDEBUG("failed. 0x%08x", ret);
    }

    m_InsideCreate = false;

    return ret;
  }

  void WrapDevice(ID3D11Device **ppDevice, D3D_DRIVER_TYPE DriverType, UINT Flags, UINT SDKVersion,
                  UINT FeatureLevels, CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                  ID3D11DeviceContext **ppImmediateContext, IDXGISwapChain **ppSwapChain,
                  CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc)
  {
    RDCDEBUG("succeeded and hooking.");

    if(!WrappedID3D11Device::IsAlloc(*ppDevice))
    {
      D3D11InitParams params;
      params.DriverType = DriverType;
      params.Flags = Flags;
      params.SDKVersion = SDKVersion;
      params.NumFeatureLevels = FeatureLevels;
      if(FeatureLevels > 0)
        memcpy(params.FeatureLevels, pFeatureLevels, sizeof(D3D_FEATURE_LEVEL) * FeatureLevels);

      WrappedID3D11Device *wrap = new WrappedID3D11Device(*ppDevice, &params);

      RDCDEBUG("created wrapped device.");

      *ppDevice = wrap;
      wrap->GetImmediateContext(ppImmediateContext);

      if(ppSwapChain && *ppSwapChain)
        *ppSwapChain = new WrappedIDXGISwapChain4(
            *ppSwapChain, pSwapChainDesc ? pSwapChainDesc->OutputWindow : NULL, wrap);
    }
  }

  static HRESULT WINAPI D3D11CreateDevice_hook(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    HRESULT ret = d3d11hooks.Create_Internal(pAdapter, DriverType, Software, Flags, pFeatureLevels,
                                             FeatureLevels, SDKVersion, NULL, NULL, ppDevice,
                                             pFeatureLevel, ppImmediateContext);

    return ret;
  }

  static HRESULT WINAPI D3D11CreateDeviceAndSwapChain_hook(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
      __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    HRESULT ret = d3d11hooks.Create_Internal(pAdapter, DriverType, Software, Flags, pFeatureLevels,
                                             FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain,
                                             ppDevice, pFeatureLevel, ppImmediateContext);

    return ret;
  }

  static HRESULT __cdecl AmdCreate11_hook(ID3D11Device *pDevice, IAmdDxExt **ppExt)
  {
    RDCLOG("Attempt to create AMD extension interface via AmdDxExtCreate11 was blocked.");

    if(ppExt)
      *ppExt = NULL;

    return E_FAIL;
  }

  PFNNVCreateDevice nvapi_CreateDevice_real;
  PFNNVCreateDeviceAndSwapChain nvapi_CreateDeviceAndSwapChain_real;

  static HRESULT WINAPI nvapi_CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType,
                                           HMODULE Software, UINT Flags,
                                           CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                                           UINT FeatureLevels, UINT SDKVersion,
                                           ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
                                           ID3D11DeviceContext **ppImmediateContext,
                                           NVAPI_DEVICE_FEATURE_LEVEL *outNVLevel)
  {
    RDCDEBUG("Call to nvapi_CreateDevice");

    bool reading = RenderDoc::Inst().IsReplayApp();

    if(reading)
    {
      RDCDEBUG("In replay app");
    }

    if(d3d11hooks.m_EnabledHooks)
    {
      if(!reading && RenderDoc::Inst().GetCaptureOptions().APIValidation)
      {
        Flags |= D3D11_CREATE_DEVICE_DEBUG;
      }
      else
      {
        Flags &= ~D3D11_CREATE_DEVICE_DEBUG;
      }
    }

    RDCDEBUG("Calling real createdevice...");

    HRESULT ret = d3d11hooks.nvapi_CreateDevice_real(
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice,
        pFeatureLevel, ppImmediateContext, outNVLevel);

    RDCDEBUG("Called real createdevice...");

    bool suppress = false;

    suppress = (Flags & D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY) != 0;

    if(suppress && !reading)
    {
      RDCDEBUG("Application requested not to be hooked.");
    }
    else if(SUCCEEDED(ret) && d3d11hooks.m_EnabledHooks && ppDevice)
    {
      d3d11hooks.WrapDevice(ppDevice, DriverType, Flags, SDKVersion, FeatureLevels, pFeatureLevels,
                            ppImmediateContext, NULL, NULL);
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D11 device.");
    }
    else
    {
      RDCDEBUG("failed. 0x%08x", ret);
    }

    return ret;
  }

  static HRESULT WINAPI nvapi_CreateDeviceAndSwapChain(
      IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
      CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
      ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
      ID3D11DeviceContext **ppImmediateContext, NVAPI_DEVICE_FEATURE_LEVEL *outNVLevel)
  {
    RDCDEBUG("Call to nvapi_CreateDeviceAndSwapChain");

    bool reading = RenderDoc::Inst().IsReplayApp();

    if(reading)
    {
      RDCDEBUG("In replay app");
    }

    if(d3d11hooks.m_EnabledHooks)
    {
      if(!reading && RenderDoc::Inst().GetCaptureOptions().APIValidation)
      {
        Flags |= D3D11_CREATE_DEVICE_DEBUG;
      }
      else
      {
        Flags &= ~D3D11_CREATE_DEVICE_DEBUG;
      }
    }

    DXGI_SWAP_CHAIN_DESC swapDesc;
    DXGI_SWAP_CHAIN_DESC *pUsedSwapDesc = NULL;

    if(pSwapChainDesc)
    {
      swapDesc = *pSwapChainDesc;
      pUsedSwapDesc = &swapDesc;
    }

    if(pUsedSwapDesc && d3d11hooks.m_EnabledHooks &&
       !RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
    {
      pUsedSwapDesc->Windowed = TRUE;
    }

    RDCDEBUG("Calling real createdevice...");

    HRESULT ret = d3d11hooks.nvapi_CreateDeviceAndSwapChain_real(
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
        pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext, outNVLevel);

    RDCDEBUG("Called real createdevice...");

    bool suppress = false;

    suppress = (Flags & D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY) != 0;

    if(suppress && !reading)
    {
      RDCDEBUG("Application requested not to be hooked.");
    }
    else if(SUCCEEDED(ret) && d3d11hooks.m_EnabledHooks && ppDevice)
    {
      d3d11hooks.WrapDevice(ppDevice, DriverType, Flags, SDKVersion, FeatureLevels, pFeatureLevels,
                            ppImmediateContext, ppSwapChain, pSwapChainDesc);
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D11 device.");
    }
    else
    {
      RDCDEBUG("failed. 0x%08x", ret);
    }

    return ret;
  }

  static void *nvapi_QueryInterface_hook(uint32_t ID)
  {
    void *real = d3d11hooks.nvapi_QueryInterface()(ID);

    if(ID == 0x6A16D3A0)
    {
      d3d11hooks.nvapi_CreateDevice_real = (PFNNVCreateDevice)real;
      return &nvapi_CreateDevice;
    }
    else if(ID == 0xBB939EE5)
    {
      d3d11hooks.nvapi_CreateDeviceAndSwapChain_real = (PFNNVCreateDeviceAndSwapChain)real;
      return &nvapi_CreateDeviceAndSwapChain;
    }

    return real;
  }
};

D3D11Hook D3D11Hook::d3d11hooks;

extern "C" __declspec(dllexport) HRESULT __cdecl RENDERDOC_CreateWrappedD3D11DeviceAndSwapChain(
    __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
    __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
    __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
{
  return D3D11Hook::CreateWrappedDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
      pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}

extern "C" __declspec(dllexport) HRESULT __cdecl RENDERDOC_CreateWrappedD3D11Device(
    __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, __out_opt ID3D11Device **ppDevice, __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
    __out_opt ID3D11DeviceContext **ppImmediateContext)
{
  return D3D11Hook::CreateWrappedDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, NULL, NULL,
      ppDevice, pFeatureLevel, ppImmediateContext);
}
