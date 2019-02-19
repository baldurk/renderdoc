/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks/hooks.h"
#include "serialise/serialiser.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"

#if ENABLED(RDOC_X64)

#define BIT_SPECIFIC_DLL(dll32, dll64) dll64

#else

#define BIT_SPECIFIC_DLL(dll32, dll64) dll32

#endif

typedef HRESULT(__cdecl *PFN_AmdExtD3DCreateInterface)(IUnknown *, REFIID, void **);

typedef HRESULT(WINAPI *PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES)(UINT NumFeatures, const IID *pIIDs,
                                                                void *pConfigurationStructs,
                                                                UINT *pConfigurationStructSizes);

ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev)
{
  if(WrappedID3D12CommandQueue::IsAlloc(dev))
    return (WrappedID3D12CommandQueue *)dev;

  return NULL;
}

// dummy class to present to the user, while we maintain control
//
// The inheritance is awful for these. See WrappedID3D12DebugDevice for why there are multiple
// parent classes
class WrappedID3D12Debug : public RefCounter12<ID3D12Debug>,
                           public ID3D12Debug3,
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

    // these are hooked to prevent AMD extensions from activating and causing later crashes when not
    // replayed correctly
    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("amdxc32.dll", "amdxc64.dll"), NULL);
    AmdExtD3DCreateInterface.Register(BIT_SPECIFIC_DLL("amdxc32.dll", "amdxc64.dll"),
                                      "AmdExtD3DCreateInterface", AmdExtD3DCreateInterface_hook);

    CreateDevice.Register("d3d12.dll", "D3D12CreateDevice", D3D12CreateDevice_hook);
    GetDebugInterface.Register("d3d12.dll", "D3D12GetDebugInterface", D3D12GetDebugInterface_hook);
    EnableExperimentalFeatures.Register("d3d12.dll", "D3D12EnableExperimentalFeatures",
                                        D3D12EnableExperimentalFeatures_hook);
  }

private:
  static D3D12Hook d3d12hooks;

  HookedFunction<PFN_AmdExtD3DCreateInterface> AmdExtD3DCreateInterface;

  static HRESULT __cdecl AmdExtD3DCreateInterface_hook(IUnknown *, REFIID, void **ppvObject)
  {
    RDCLOG("Attempt to create AMD extension interface via AmdExtD3DCreateInterface was blocked.");

    if(ppvObject)
      *ppvObject = NULL;

    return E_FAIL;
  }

  HookedFunction<PFN_D3D12_GET_DEBUG_INTERFACE> GetDebugInterface;
  HookedFunction<PFN_D3D12_CREATE_DEVICE> CreateDevice;
  HookedFunction<PFN_D3D12_ENABLE_EXPERIMENTAL_FEATURES> EnableExperimentalFeatures;

  // re-entrancy detection (can happen in rare cases with e.g. fraps)
  bool m_InsideCreate = false;

  HRESULT Create_Internal(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                          void **ppDevice)
  {
    // if we're already inside a wrapped create i.e. this function, then DON'T do anything
    // special. Just grab the trampolined function and call it.
    if(m_InsideCreate)
    {
      PFN_D3D12_CREATE_DEVICE createFunc = CreateDevice();

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

      return createFunc(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    }

    m_InsideCreate = true;

    if(riid != __uuidof(ID3D12Device) && riid != __uuidof(ID3D12Device1) &&
       riid != __uuidof(ID3D12Device2) && riid != __uuidof(ID3D12Device3) &&
       riid != __uuidof(ID3D12Device4) && riid != __uuidof(ID3D12Device5))
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

    PFN_D3D12_CREATE_DEVICE createFunc = CreateDevice();

    if(createFunc == NULL)
      createFunc = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(GetModuleHandleA("d3d12.dll"),
                                                           "D3D12CreateDevice");

    // shouldn't ever get here, we should either have it from procaddress or the trampoline, but
    // let's be safe.
    if(createFunc == NULL)
    {
      RDCERR("Something went seriously wrong with the hooks!");

      m_InsideCreate = false;

      return E_UNEXPECTED;
    }

    HRESULT ret = createFunc(pAdapter, MinimumFeatureLevel, riid, ppDevice);

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

        WrappedID3D12Device *wrap = new WrappedID3D12Device(dev, params, EnableDebugLayer);

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

    m_InsideCreate = false;

    return ret;
  }

  static HRESULT WINAPI D3D12CreateDevice_hook(IUnknown *pAdapter,
                                               D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                               void **ppDevice)
  {
    return d3d12hooks.Create_Internal(pAdapter, MinimumFeatureLevel, riid, ppDevice);
  }

  static HRESULT WINAPI D3D12EnableExperimentalFeatures_hook(UINT NumFeatures, const IID *pIIDs,
                                                             void *pConfigurationStructs,
                                                             UINT *pConfigurationStructSizes)
  {
    // in future in theory we could whitelist some features. For now we don't allow any.

    // header says "The call returns E_NOINTERFACE if an unrecognized feature is passed in or
    // Windows Developer mode is not on." so this is the most appropriate error.
    return E_NOINTERFACE;
  }

  static HRESULT WINAPI D3D12GetDebugInterface_hook(REFIID riid, void **ppvDebug)
  {
    if(riid != __uuidof(ID3D12Debug))
    {
      *ppvDebug = (ID3D12Debug *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid != __uuidof(ID3D12Debug1))
    {
      *ppvDebug = (ID3D12Debug1 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid != __uuidof(ID3D12Debug2))
    {
      *ppvDebug = (ID3D12Debug2 *)(new WrappedID3D12Debug());
      return S_OK;
    }
    else if(riid != __uuidof(ID3D12Debug3))
    {
      *ppvDebug = (ID3D12Debug3 *)(new WrappedID3D12Debug());
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
