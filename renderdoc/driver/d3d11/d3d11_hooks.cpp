/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "d3d11_hooks.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks/hooks.h"
#include "d3d11_device.h"

ID3DDevice *GetD3D11DeviceIfAlloc(IUnknown *dev)
{
  if(WrappedID3D11Device::IsAlloc(dev))
    return (WrappedID3D11Device *)dev;

  return NULL;
}

class D3D11Hook : LibraryHook
{
public:
  void RegisterHooks()
  {
    RDCLOG("Registering D3D11 hooks");

    WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

    // also require d3dcompiler_??.dll
    if(GetD3DCompiler() == NULL)
    {
      RDCERR("Failed to load d3dcompiler_??.dll - not inserting D3D11 hooks.");
      return;
    }

    LibraryHooks::RegisterLibraryHook("d3d11.dll", NULL);

    CreateDevice.Register("d3d11.dll", "D3D11CreateDevice", D3D11CreateDevice_hook);
    CreateDeviceAndSwapChain.Register("d3d11.dll", "D3D11CreateDeviceAndSwapChain",
                                      D3D11CreateDeviceAndSwapChain_hook);

    m_RecurseSlot = Threading::AllocateTLSSlot();
    Threading::SetTLSValue(m_RecurseSlot, NULL);
  }

private:
  static D3D11Hook d3d11hooks;

  HookedFunction<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN> CreateDeviceAndSwapChain;
  HookedFunction<PFN_D3D11_CREATE_DEVICE> CreateDevice;

  // re-entrancy detection (can happen in rare cases with e.g. fraps)
  uint64_t m_RecurseSlot = 0;

  void EndRecurse() { Threading::SetTLSValue(m_RecurseSlot, NULL); }
  bool CheckRecurse()
  {
    if(Threading::GetTLSValue(m_RecurseSlot) == NULL)
    {
      Threading::SetTLSValue(m_RecurseSlot, (void *)1);
      return false;
    }

    return true;
  }

  friend HRESULT CreateD3D11_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                                      D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                                      __in_ecount_opt(FeatureLevels)
                                          CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                                      UINT FeatureLevels, UINT SDKVersion,
                                      __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                                      __out_opt IDXGISwapChain **ppSwapChain,
                                      __out_opt ID3D11Device **ppDevice,
                                      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                                      __out_opt ID3D11DeviceContext **ppImmediateContext);

  HRESULT Create_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                          D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                          __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                          UINT FeatureLevels, UINT SDKVersion,
                          __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                          __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
                          __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                          __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    // if we're already inside a wrapped create, then DON'T do anything special. Just call onwards
    if(CheckRecurse())
    {
      return real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                  pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    }

    RDCDEBUG("Call to Create_Internal Flags %x", Flags);

    // we should no longer go through here in the replay application
    RDCASSERT(!RenderDoc::Inst().IsReplayApp());

    if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
      Flags |= D3D11_CREATE_DEVICE_DEBUG;
    else
      Flags &= ~D3D11_CREATE_DEVICE_DEBUG;

    DXGI_SWAP_CHAIN_DESC swapDesc;
    DXGI_SWAP_CHAIN_DESC *pUsedSwapDesc = NULL;

    if(pSwapChainDesc)
    {
      swapDesc = *pSwapChainDesc;
      pUsedSwapDesc = &swapDesc;
    }

    if(pUsedSwapDesc && !RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    {
      pUsedSwapDesc->Windowed = TRUE;
    }

    RDCDEBUG("Calling real createdevice...");

    // Hack for D3DGear which crashes if ppDevice is NULL
    ID3D11Device *dummydev = NULL;
    bool dummyUsed = false;
    if(ppDevice == NULL)
    {
      ppDevice = &dummydev;
      dummyUsed = true;
    }

    HRESULT ret = real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                       SDKVersion, pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, NULL);

    SAFE_RELEASE(dummydev);
    if(dummyUsed)
      ppDevice = NULL;

    RDCDEBUG("Called real createdevice...");

    bool suppress = false;

    suppress = (Flags & D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY) != 0;

    if(suppress)
    {
      RDCLOG("Application requested not to be hooked.");
    }
    else if(SUCCEEDED(ret) && ppDevice)
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

        WrappedID3D11Device *wrap = new WrappedID3D11Device(*ppDevice, params);

        RDCDEBUG("created wrapped device.");

        *ppDevice = wrap;

        wrap->GetImmediateContext(ppImmediateContext);

        if(ppSwapChain && *ppSwapChain)
          *ppSwapChain = new WrappedIDXGISwapChain4(
              *ppSwapChain, pSwapChainDesc ? pSwapChainDesc->OutputWindow : NULL, wrap);
      }
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D11 device.");
    }
    else
    {
      RDCDEBUG("failed. HRESULT: %s", ToStr(ret).c_str());
    }

    EndRecurse();

    return ret;
  }

  static HRESULT WINAPI D3D11CreateDevice_hook(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    // just forward the call with NULL swapchain parameters
    return D3D11CreateDeviceAndSwapChain_hook(pAdapter, DriverType, Software, Flags, pFeatureLevels,
                                              FeatureLevels, SDKVersion, NULL, NULL, ppDevice,
                                              pFeatureLevel, ppImmediateContext);
  }

  static HRESULT WINAPI D3D11CreateDeviceAndSwapChain_hook(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
      __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createFunc = d3d11hooks.CreateDeviceAndSwapChain();

    if(createFunc == NULL)
    {
      RDCWARN("Call to D3D11CreateDeviceAndSwapChain_hook without onward function pointer");
      createFunc = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
          GetModuleHandleA("d3d11.dll"), "D3D11CreateDeviceAndSwapChain");
    }

    // shouldn't ever get here, we should either have it from procaddress or the hook function, but
    // let's be safe.
    if(createFunc == NULL)
    {
      RDCERR("Something went seriously wrong with the hooks!");
      return E_UNEXPECTED;
    }

    return d3d11hooks.Create_Internal(createFunc, pAdapter, DriverType, Software, Flags,
                                      pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc,
                                      ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
  }
};

D3D11Hook D3D11Hook::d3d11hooks;

HRESULT CreateD3D11_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                             D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                             __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                             UINT FeatureLevels, UINT SDKVersion,
                             __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                             __out_opt IDXGISwapChain **ppSwapChain,
                             __out_opt ID3D11Device **ppDevice,
                             __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                             __out_opt ID3D11DeviceContext **ppImmediateContext)
{
  return D3D11Hook::d3d11hooks.Create_Internal(
      real, pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
      pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}
