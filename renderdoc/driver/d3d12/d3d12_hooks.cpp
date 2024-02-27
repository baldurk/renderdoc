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

#include "d3d12_hooks.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks/hooks.h"
#include "serialise/serialiser.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"

#include "driver/dx/official/D3D11On12On7.h"

typedef HRESULT(WINAPI *PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES)(UINT NumFeatures, const IID *pIIDs,
                                                                void *pConfigurationStructs,
                                                                UINT *pConfigurationStructSizes);

ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev)
{
  if(WrappedID3D12CommandQueue::IsAlloc(dev))
    return (WrappedID3D12CommandQueue *)dev;

  if(WrappedID3D12Device::IsAlloc(dev))
    return (WrappedID3D12Device *)dev;

  return NULL;
}

class WrappedD3D11On12On7 : public RefCounter12<ID3D11On12On7>
{
public:
  WrappedD3D11On12On7(ID3D11On12On7 *real) : RefCounter12(real) {}
  virtual ~WrappedD3D11On12On7() {}
  //////////////////////////////
  // Implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::AddRef(); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::Release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (IUnknown *)this;
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  //////////////////////////////
  // Implement ID3D11On12On7

  // Enables usage similar to D3D11On12CreateDevice.
  void STDMETHODCALLTYPE SetThreadDeviceCreationParams(ID3D12Device *pDevice,
                                                       ID3D12CommandQueue *pGraphicsQueue)
  {
    RDCASSERT(WrappedID3D12Device::IsAlloc(pDevice));
    m_pReal->SetThreadDeviceCreationParams(((WrappedID3D12Device *)pDevice)->GetReal(),
                                           Unwrap(pGraphicsQueue));
  }

  // Enables usage similar to ID3D11On12Device::CreateWrappedResource.
  // Note that the D3D11 resource creation parameters should be similar to the D3D12 resource,
  // or else unexpected/undefined behavior may occur.
  void STDMETHODCALLTYPE SetThreadResourceCreationParams(ID3D12Resource *pResource)
  {
    m_pReal->SetThreadResourceCreationParams(Unwrap(pResource));
  }

  ID3D11On12On7Device *STDMETHODCALLTYPE GetThreadLastCreatedDevice()
  {
    // don't need to wrap/unwrap, it only deals with ID3D11On12On7Resource
    return m_pReal->GetThreadLastCreatedDevice();
  }

  ID3D11On12On7Resource *STDMETHODCALLTYPE GetThreadLastCreatedResource()
  {
    // don't need to wrap/unwrap
    return m_pReal->GetThreadLastCreatedResource();
  }
};

// dummy class to present to the user, while we maintain control
//
// The inheritance is awful for these. See WrappedID3D12DebugDevice for why there are multiple
// parent classes
class WrappedID3D12Debug : public RefCounter12<ID3D12Debug>,
                           public ID3D12Debug6,
                           public ID3D12Debug1,
                           public ID3D12Debug2
{
public:
  WrappedID3D12Debug() : RefCounter12(NULL) {}
  virtual ~WrappedID3D12Debug() {}
  //////////////////////////////
  // Implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::AddRef(); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::Release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (IUnknown *)(ID3D12Debug *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug))
    {
      *ppvObject = (ID3D12Debug *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug1))
    {
      *ppvObject = (ID3D12Debug1 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug2))
    {
      *ppvObject = (ID3D12Debug2 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug3))
    {
      *ppvObject = (ID3D12Debug3 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug4))
    {
      *ppvObject = (ID3D12Debug4 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug5))
    {
      *ppvObject = (ID3D12Debug5 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12Debug6))
    {
      *ppvObject = (ID3D12Debug6 *)this;
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  //////////////////////////////
  // Implement ID3D12Debug / ID3D12Debug1
  virtual void STDMETHODCALLTYPE EnableDebugLayer() {}
  //////////////////////////////
  // Implement ID3D12Debug1 / ID3D12Debug3
  virtual void STDMETHODCALLTYPE SetEnableGPUBasedValidation(BOOL Enable) {}
  virtual void STDMETHODCALLTYPE SetEnableSynchronizedCommandQueueValidation(BOOL Enable) {}
  // Implement ID3D12Debug2 / ID3D12Debug3
  virtual void STDMETHODCALLTYPE SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS Flags)
  {
  }
  //////////////////////////////
  // Implement ID3D12Debug4
  virtual void STDMETHODCALLTYPE DisableDebugLayer(void) {}
  //////////////////////////////
  // Implement ID3D12Debug5
  virtual void STDMETHODCALLTYPE SetEnableAutoName(BOOL Enable) {}
  //////////////////////////////
  // Implement ID3D12Debug6
  virtual void STDMETHODCALLTYPE SetForceLegacyBarrierValidation(BOOL Enable) {}
};

class D3D12Hook : LibraryHook
{
public:
  void RegisterHooks()
  {
    RDCLOG("Registering D3D12 hooks");

    WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D12DeviceIfAlloc);

    // also require d3dcompiler_??.dll
    if(GetD3DCompiler() == NULL)
    {
      RDCERR("Failed to load d3dcompiler_??.dll - not inserting D3D12 hooks.");
      return;
    }

    LibraryHooks::RegisterLibraryHook("d3d12.dll", NULL);

    CreateDevice.Register("d3d12.dll", "D3D12CreateDevice", D3D12CreateDevice_hook);
    GetDebugInterface.Register("d3d12.dll", "D3D12GetDebugInterface", D3D12GetDebugInterface_hook);
    EnableExperimentalFeatures.Register("d3d12.dll", "D3D12EnableExperimentalFeatures",
                                        D3D12EnableExperimentalFeatures_hook);
    GetD3D11On12On7.Register("d3d11on12.dll", "GetD3D11On12On7Interface",
                             GetD3D11On12On7Interface_hook);

    m_RecurseSlot = Threading::AllocateTLSSlot();
    Threading::SetTLSValue(m_RecurseSlot, NULL);
  }

private:
  static D3D12Hook d3d12hooks;

  HookedFunction<PFN_D3D12_GET_DEBUG_INTERFACE> GetDebugInterface;
  HookedFunction<PFN_D3D12_CREATE_DEVICE> CreateDevice;
  HookedFunction<PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES> EnableExperimentalFeatures;
  HookedFunction<PFNGetD3D11On12On7Interface> GetD3D11On12On7;

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

  friend HRESULT CreateD3D12_Internal(RealD3D12CreateFunction real, IUnknown *pAdapter,
                                      D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                      void **ppDevice);

  HRESULT Create_Internal(RealD3D12CreateFunction real, IUnknown *pAdapter,
                          D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
  {
    // if we're already inside a wrapped create i.e. this function, then DON'T do anything
    // special. Just grab the trampolined function and call it.
    if(CheckRecurse())
      return real(pAdapter, MinimumFeatureLevel, riid, ppDevice);

    if(riid != __uuidof(ID3D12Device) && riid != __uuidof(ID3D12Device1) &&
       riid != __uuidof(ID3D12Device2) && riid != __uuidof(ID3D12Device3) &&
       riid != __uuidof(ID3D12Device4) && riid != __uuidof(ID3D12Device5) &&
       riid != __uuidof(ID3D12Device6) && riid != __uuidof(ID3D12Device7) &&
       riid != __uuidof(ID3D12Device8) && riid != __uuidof(ID3D12Device9) &&
       riid != __uuidof(ID3D12Device10) && riid != __uuidof(ID3D12Device11) &&
       riid != __uuidof(ID3D12Device12))
    {
      RDCERR("Unsupported UUID %s for D3D12CreateDevice", ToStr(riid).c_str());
      return E_NOINTERFACE;
    }

    RDCDEBUG("Call to Create_Internal Feature Level %x", MinimumFeatureLevel, ToStr(riid).c_str());

    // we should no longer go through here in the replay application
    RDCASSERT(!RenderDoc::Inst().IsReplayApp());

    bool EnableDebugLayer = false;

    if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
      EnableDebugLayer = EnableD3D12DebugLayer(GetDebugInterface());

    RDCDEBUG("Calling real createdevice...");

    HRESULT ret = real(pAdapter, MinimumFeatureLevel, riid, ppDevice);

    RDCDEBUG("Called real createdevice... HRESULT: %s", ToStr(ret).c_str());

    if(SUCCEEDED(ret) && ppDevice)
    {
      RDCDEBUG("succeeded and hooking.");

      if(!WrappedID3D12Device::IsAlloc(*ppDevice))
      {
        D3D12InitParams params;
        params.MinimumFeatureLevel = MinimumFeatureLevel;

        ID3D12Device *dev = (ID3D12Device *)*ppDevice;

        if(riid == __uuidof(ID3D12Device1))
        {
          ID3D12Device1 *dev1 = (ID3D12Device1 *)*ppDevice;
          dev = (ID3D12Device *)dev1;
        }
        else if(riid == __uuidof(ID3D12Device2))
        {
          ID3D12Device2 *dev2 = (ID3D12Device2 *)*ppDevice;
          dev = (ID3D12Device *)dev2;
        }
        else if(riid == __uuidof(ID3D12Device3))
        {
          ID3D12Device3 *dev3 = (ID3D12Device3 *)*ppDevice;
          dev = (ID3D12Device *)dev3;
        }
        else if(riid == __uuidof(ID3D12Device4))
        {
          ID3D12Device4 *dev4 = (ID3D12Device4 *)*ppDevice;
          dev = (ID3D12Device *)dev4;
        }
        else if(riid == __uuidof(ID3D12Device5))
        {
          ID3D12Device5 *dev5 = (ID3D12Device5 *)*ppDevice;
          dev = (ID3D12Device *)dev5;
        }
        else if(riid == __uuidof(ID3D12Device6))
        {
          ID3D12Device6 *dev6 = (ID3D12Device6 *)*ppDevice;
          dev = (ID3D12Device *)dev6;
        }
        else if(riid == __uuidof(ID3D12Device7))
        {
          ID3D12Device7 *dev7 = (ID3D12Device7 *)*ppDevice;
          dev = (ID3D12Device *)dev7;
        }
        else if(riid == __uuidof(ID3D12Device8))
        {
          ID3D12Device8 *dev8 = (ID3D12Device8 *)*ppDevice;
          dev = (ID3D12Device *)dev8;
        }
        else if(riid == __uuidof(ID3D12Device9))
        {
          ID3D12Device9 *dev9 = (ID3D12Device9 *)*ppDevice;
          dev = (ID3D12Device *)dev9;
        }
        else if(riid == __uuidof(ID3D12Device10))
        {
          ID3D12Device10 *dev9 = (ID3D12Device10 *)*ppDevice;
          dev = (ID3D12Device *)dev9;
        }
        else if(riid == __uuidof(ID3D12Device11))
        {
          ID3D12Device11 *dev9 = (ID3D12Device11 *)*ppDevice;
          dev = (ID3D12Device *)dev9;
        }
        else if(riid == __uuidof(ID3D12Device12))
        {
          ID3D12Device12 *dev9 = (ID3D12Device12 *)*ppDevice;
          dev = (ID3D12Device *)dev9;
        }

        WrappedID3D12Device *wrap = WrappedID3D12Device::Create(dev, params, EnableDebugLayer);

        RDCDEBUG("created wrapped device.");

        *ppDevice = (ID3D12Device *)wrap;

        if(riid == __uuidof(ID3D12Device1))
          *ppDevice = (ID3D12Device1 *)wrap;
        else if(riid == __uuidof(ID3D12Device2))
          *ppDevice = (ID3D12Device2 *)wrap;
        else if(riid == __uuidof(ID3D12Device3))
          *ppDevice = (ID3D12Device3 *)wrap;
        else if(riid == __uuidof(ID3D12Device4))
          *ppDevice = (ID3D12Device4 *)wrap;
        else if(riid == __uuidof(ID3D12Device5))
          *ppDevice = (ID3D12Device5 *)wrap;
        else if(riid == __uuidof(ID3D12Device6))
          *ppDevice = (ID3D12Device6 *)wrap;
        else if(riid == __uuidof(ID3D12Device7))
          *ppDevice = (ID3D12Device7 *)wrap;
        else if(riid == __uuidof(ID3D12Device8))
          *ppDevice = (ID3D12Device8 *)wrap;
        else if(riid == __uuidof(ID3D12Device9))
          *ppDevice = (ID3D12Device9 *)wrap;
        else if(riid == __uuidof(ID3D12Device10))
          *ppDevice = (ID3D12Device10 *)wrap;
        else if(riid == __uuidof(ID3D12Device11))
          *ppDevice = (ID3D12Device11 *)wrap;
        else if(riid == __uuidof(ID3D12Device12))
          *ppDevice = (ID3D12Device12 *)wrap;
      }
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D12 device.");
    }
    else
    {
      RDCDEBUG("failed. HRESULT: %s", ToStr(ret).c_str());
    }

    EndRecurse();

    return ret;
  }

  static HRESULT WINAPI D3D12CreateDevice_hook(IUnknown *pAdapter,
                                               D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                               void **ppDevice)
  {
    PFN_D3D12_CREATE_DEVICE createFunc = d3d12hooks.CreateDevice();

    if(!createFunc)
    {
      HMODULE d3d12 = GetModuleHandleA("d3d12.dll");

      if(d3d12)
        createFunc = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice");

      if(!createFunc)
      {
        RDCERR("Something went seriously wrong, d3d12.dll couldn't be loaded!");
        return E_UNEXPECTED;
      }
    }

    return d3d12hooks.Create_Internal(createFunc, pAdapter, MinimumFeatureLevel, riid, ppDevice);
  }

  static HRESULT WINAPI D3D12EnableExperimentalFeatures_hook(UINT NumFeatures, const IID *pIIDs,
                                                             void *pConfigurationStructs,
                                                             UINT *pConfigurationStructSizes)
  {
    rdcarray<IID> allowedIIDs;

    // allow enabling unsigned DXIL.
    for(UINT i = 0; i < NumFeatures; i++)
    {
      if(pIIDs[i] == D3D12ExperimentalShaderModels)
        allowedIIDs.push_back(D3D12ExperimentalShaderModels);
    }

    // there's no "partially successful" error code, so we just lie to the application and pretend
    // that any filtered IIDs also succeeded
    if(!allowedIIDs.empty())
      return d3d12hooks.EnableExperimentalFeatures()((UINT)allowedIIDs.size(), allowedIIDs.data(),
                                                     NULL, NULL);

    // header says "The call returns E_NOINTERFACE if an unrecognized feature is passed in or
    // Windows Developer mode is not on." so this is the most appropriate error for if no IIDs are
    // allowed.
    return E_NOINTERFACE;
  }

  static HRESULT WINAPI GetD3D11On12On7Interface_hook(ID3D11On12On7 **ppIface)
  {
    ID3D11On12On7 *real = NULL;
    d3d12hooks.GetD3D11On12On7()(&real);
    *ppIface = (ID3D11On12On7 *)(new WrappedD3D11On12On7(real));
    return S_OK;
  }

  static HRESULT WINAPI D3D12GetDebugInterface_hook(REFIID riid, void **ppvDebug)
  {
    if(riid == __uuidof(ID3D12Debug))
    {
      *ppvDebug = (ID3D12Debug *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Debug1))
    {
      *ppvDebug = (ID3D12Debug1 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Debug2))
    {
      *ppvDebug = (ID3D12Debug2 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Debug3))
    {
      *ppvDebug = (ID3D12Debug3 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Debug4))
    {
      *ppvDebug = (ID3D12Debug4 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Debug5))
    {
      *ppvDebug = (ID3D12Debug5 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Debug6))
    {
      *ppvDebug = (ID3D12Debug6 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else
    {
      IUnknown *releaseme = NULL;
      HRESULT real = d3d12hooks.GetDebugInterface()(riid, (void **)&releaseme);

      if(releaseme)
        releaseme->Release();

      RDCWARN("Unknown UUID passed to D3D12GetDebugInterface: %s. Real call %s succeed (%x).",
              ToStr(riid).c_str(), SUCCEEDED(real) ? "did" : "did not", real);

      return E_NOINTERFACE;
    }
  }
};

D3D12Hook D3D12Hook::d3d12hooks;

HRESULT CreateD3D12_Internal(RealD3D12CreateFunction real, IUnknown *pAdapter,
                             D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
  return D3D12Hook::d3d12hooks.Create_Internal(real, pAdapter, MinimumFeatureLevel, riid, ppDevice);
}
