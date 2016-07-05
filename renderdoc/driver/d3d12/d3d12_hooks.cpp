/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "driver/d3d12/d3d12_device.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks/hooks.h"
#include "serialise/serialiser.h"

#define DLL_NAME "d3d12.dll"

ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev)
{
  if(WrappedID3D12Device::IsAlloc(dev))
    return (ID3DDevice *)dev;

  return NULL;
}

// dummy class to present to the user, while we maintain control
class WrappedID3D12Debug : public RefCounter12<ID3D12Debug>, public ID3D12Debug
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

    return RefCounter12::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // Implement ID3D12Debug
  virtual void STDMETHODCALLTYPE EnableDebugLayer() {}
};

class D3D12Hook : LibraryHook
{
public:
  D3D12Hook()
  {
    LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
    m_EnabledHooks = true;
    m_InsideCreate = false;

    WrappedIDXGISwapChain3::RegisterD3DDeviceCallback(GetD3D12DeviceIfAlloc);
  }

  bool CreateHooks(const char *libName)
  {
    bool success = true;

    // also require d3dcompiler_??.dll
    if(GetD3DCompiler() == NULL)
    {
      RDCERR("Failed to load d3dcompiler_??.dll - not inserting D3D12 hooks.");
      return false;
    }

    success &= CreateDevice.Initialize("D3D12CreateDevice", DLL_NAME, D3D12CreateDevice_hook);
    success &= GetDebugInterface.Initialize("D3D12GetDebugInterface", DLL_NAME,
                                            D3D12GetDebugInterface_hook);

    if(!success)
      return false;

    m_HasHooks = true;
    m_EnabledHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
  bool UseHooks() { return (d3d12hooks.m_HasHooks && d3d12hooks.m_EnabledHooks); }
  static HRESULT CreateWrappedDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel,
                                     REFIID riid, void **ppDevice)
  {
    return d3d12hooks.Create_Internal(pAdapter, MinimumFeatureLevel, riid, ppDevice);
  }

private:
  static D3D12Hook d3d12hooks;

  bool m_HasHooks;
  bool m_EnabledHooks;

  Hook<PFN_D3D12_GET_DEBUG_INTERFACE> GetDebugInterface;
  Hook<PFN_D3D12_CREATE_DEVICE> CreateDevice;

  // re-entrancy detection (can happen in rare cases with e.g. fraps)
  bool m_InsideCreate;

  HRESULT Create_Internal(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                          void **ppDevice)
  {
    // if we're already inside a wrapped create i.e. this function, then DON'T do anything
    // special. Just grab the trampolined function and call it.
    if(m_InsideCreate)
    {
      PFN_D3D12_CREATE_DEVICE createFunc = NULL;

      // shouldn't ever get in here if we're in the case without hooks but let's be safe.
      if(m_HasHooks)
      {
        createFunc = CreateDevice();
      }
      else
      {
        HMODULE d3d12 = GetModuleHandleA("d3d12.dll");

        if(d3d12)
        {
          createFunc = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice");
        }
        else
        {
          RDCERR("Something went seriously wrong, d3d12.dll couldn't be loaded!");
          return E_UNEXPECTED;
        }
      }

      return createFunc(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    }

    m_InsideCreate = true;

    if(riid != __uuidof(ID3D12Device))
    {
      RDCERR("Unsupported UUID %s for D3D12CreateDevice", ToStr::Get(riid).c_str());
      return E_NOINTERFACE;
    }

    RDCDEBUG("Call to Create_Internal Feature Level %x", MinimumFeatureLevel,
             ToStr::Get(riid).c_str());

    bool reading = RenderDoc::Inst().IsReplayApp();

    if(reading)
    {
      RDCDEBUG("In replay app");
    }

    const bool EnableDebugLayer =
        (m_EnabledHooks && !reading && RenderDoc::Inst().GetCaptureOptions().APIValidation);

    if(EnableDebugLayer)
    {
      ID3D12Debug *debug = NULL;
      HRESULT hr = GetDebugInterface()(__uuidof(ID3D12Debug), (void **)&debug);

      if(SUCCEEDED(hr) && debug)
        debug->EnableDebugLayer();
      else
        RDCERR("Couldn't enable debug layer: %x", hr);

      SAFE_RELEASE(debug);
    }

    RDCDEBUG("Calling real createdevice...");

    PFN_D3D12_CREATE_DEVICE createFunc =
        (PFN_D3D12_CREATE_DEVICE)GetProcAddress(GetModuleHandleA("d3d12.dll"), "D3D12CreateDevice");

    if(createFunc == NULL)
      createFunc = CreateDevice();

    // shouldn't ever get here, we should either have it from procaddress or the trampoline, but
    // let's be safe.
    if(createFunc == NULL)
    {
      RDCERR("Something went seriously wrong with the hooks!");

      m_InsideCreate = false;

      return E_UNEXPECTED;
    }

    HRESULT ret = createFunc(pAdapter, MinimumFeatureLevel, riid, ppDevice);

    RDCDEBUG("Called real createdevice... 0x%08x", ret);

    if(SUCCEEDED(ret) && m_EnabledHooks && ppDevice)
    {
      RDCDEBUG("succeeded and hooking.");

      if(!WrappedID3D12Device::IsAlloc(*ppDevice))
      {
        D3D12InitParams params;
        params.MinimumFeatureLevel = MinimumFeatureLevel;

        ID3D12Device *dev = (ID3D12Device *)*ppDevice;

        WrappedID3D12Device *wrap = new WrappedID3D12Device(dev, &params);

        RDCDEBUG("created wrapped device.");

        *ppDevice = (ID3D12Device *)wrap;
      }
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D12 device.");
    }
    else
    {
      RDCDEBUG("failed. 0x%08x", ret);
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

  static HRESULT WINAPI D3D12GetDebugInterface_hook(REFIID riid, void **ppvDebug)
  {
    if(riid != __uuidof(ID3D12Debug))
    {
      IUnknown *releaseme = NULL;
      HRESULT real = d3d12hooks.GetDebugInterface()(riid, (void **)&releaseme);

      if(releaseme)
        releaseme->Release();

      RDCWARN("Unknown UUID passed to D3D12GetDebugInterface: %s. Real call %s succeed (%x).",
              ToStr::Get(riid).c_str(), SUCCEEDED(real) ? "did" : "did not", real);

      return E_NOINTERFACE;
    }

    *ppvDebug = (ID3D12Debug *)(new WrappedID3D12Debug());
    return S_OK;
  }
};

D3D12Hook D3D12Hook::d3d12hooks;

extern "C" __declspec(dllexport) HRESULT
    __cdecl RENDERDOC_CreateWrappedD3D12Device(IUnknown *pAdapter,
                                               D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                               void **ppDevice)
{
  return D3D12Hook::CreateWrappedDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);
}