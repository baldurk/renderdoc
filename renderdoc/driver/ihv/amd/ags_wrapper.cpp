/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "ags_wrapper.h"
#include "api/replay/version.h"
#include "common/common.h"
#include "core/plugins.h"

#include "driver/dx/official/d3d11.h"
#include "driver/dx/official/d3d12.h"

#include "official/ags/amd_ags.h"

struct ReplayAGSD3DDevice : IAGSD3DDevice
{
public:
  ReplayAGSD3DDevice(uint32_t space, uint32_t reg) : space(space), reg(reg) {}
  ~ReplayAGSD3DDevice()
  {
    if(device11)
    {
      unsigned int dummy = 0;
      agsDriverExtensionsDX11_DestroyDevice(ags, device11, &dummy, context, &dummy);
    }
    else if(device12)
    {
      unsigned int dummy = 0;
      agsDriverExtensionsDX12_DestroyDevice(ags, device12, &dummy);
    }

    device11 = NULL;
    context = NULL;
    device12 = NULL;

    if(ags)
    {
      agsDeInitialize(ags);
    }

    ags = NULL;
  }

  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(ppvObject)
      *ppvObject = NULL;
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef()
  {
    ref++;
    return ref;
  }

  virtual ULONG STDMETHODCALLTYPE Release()
  {
    ref--;

    if(ref == 0)
    {
      delete this;
      return 0;
    }

    return ref;
  }

  virtual HRESULT STDMETHODCALLTYPE
  CreateD3D11(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
              CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
              CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
              ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
              ID3D11DeviceContext **ppImmediateContext)
  {
    AGSDX11DeviceCreationParams creationParams;
    AGSDX11ExtensionParams extensionParams;
    AGSDX11ReturnedParams returnedParams;

    extensionParams.numBreadcrumbMarkers = 0;
    extensionParams.pAppName = L"RenderDoc";
    extensionParams.pEngineName = L"RenderDoc";
    extensionParams.appVersion = RENDERDOC_VERSION_MAJOR << 8 | RENDERDOC_VERSION_MINOR;
    extensionParams.engineVersion = RENDERDOC_VERSION_MAJOR << 8 | RENDERDOC_VERSION_MINOR;
    extensionParams.crossfireMode = AGS_CROSSFIRE_MODE_DISABLE;
    extensionParams.uavSlot = reg;

    creationParams.pAdapter = pAdapter;
    creationParams.DriverType = DriverType;
    creationParams.Software = Software;
    creationParams.Flags = Flags;
    creationParams.pFeatureLevels = pFeatureLevels;
    creationParams.FeatureLevels = FeatureLevels;
    creationParams.SDKVersion = SDKVersion;
    creationParams.pSwapChainDesc = pSwapChainDesc;

    AGSReturnCode ret = agsDriverExtensionsDX11_CreateDevice(ags, &creationParams, &extensionParams,
                                                             &returnedParams);

    if(ret == AGS_SUCCESS)
    {
      device11 = returnedParams.pDevice;
      device11->AddRef();

      context = returnedParams.pImmediateContext;
      context->AddRef();

      if(ppSwapChain)
        *ppSwapChain = returnedParams.pSwapChain;
      if(ppDevice)
        *ppDevice = returnedParams.pDevice;
      if(ppImmediateContext)
        *ppImmediateContext = returnedParams.pImmediateContext;
      if(pFeatureLevel)
        *pFeatureLevel = returnedParams.featureLevel;

      extensionsSupported11 = returnedParams.extensionsSupported;

      return S_OK;
    }

    return E_FAIL;
  }

  virtual HRESULT STDMETHODCALLTYPE CreateD3D12(IUnknown *pAdapter,
                                                D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                                void **ppDevice)
  {
    AGSDX12DeviceCreationParams creationParams;
    AGSDX12ExtensionParams extensionParams;
    AGSDX12ReturnedParams returnedParams;

    extensionParams.pAppName = L"RenderDoc";
    extensionParams.pEngineName = L"RenderDoc";
    extensionParams.appVersion = RENDERDOC_VERSION_MAJOR << 8 | RENDERDOC_VERSION_MINOR;
    extensionParams.engineVersion = RENDERDOC_VERSION_MAJOR << 8 | RENDERDOC_VERSION_MINOR;
    extensionParams.uavSlot = reg;

    if(reg == 0)
      RDCASSERT(space == AGS_DX12_SHADER_INSTRINSICS_SPACE_ID);
    else
      RDCASSERT(space == 0);

    creationParams.pAdapter = (IDXGIAdapter *)pAdapter;
    creationParams.FeatureLevel = MinimumFeatureLevel;
    creationParams.iid = riid;

    AGSReturnCode ret = agsDriverExtensionsDX12_CreateDevice(ags, &creationParams, &extensionParams,
                                                             &returnedParams);

    if(ret == AGS_SUCCESS)
    {
      device12 = returnedParams.pDevice;
      device12->AddRef();

      if(ppDevice)
        *ppDevice = returnedParams.pDevice;

      extensionsSupported12 = returnedParams.extensionsSupported;

      return S_OK;
    }

    return E_FAIL;
  }

  virtual BOOL STDMETHODCALLTYPE ExtensionsSupported()
  {
    // check that the oldest extension we might need is supported. If this is a different GPU (like
    // nv) the device creation might have succeeded but this extension won't be listed.
    // this doesn't catch the case where some intrinsics are used on replay that are newer. We don't
    // store that fine-grained information about which intrinsics are used.
    if(device12)
      return extensionsSupported12.intrinsics16;
    else if(device11)
      return extensionsSupported11.intrinsics16;
    else
      return FALSE;
  }

  // this should only be used on capture
  virtual IUnknown *STDMETHODCALLTYPE GetReal() { return NULL; }
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD, DWORD) { return FALSE; }
private:
  friend IAGSD3DDevice *InitialiseAGSReplay(uint32_t space, uint32_t reg);

  uint32_t space, reg;

  ULONG ref = 1;

  AGSContext *ags = NULL;

  ID3D11Device *device11 = NULL;
  ID3D11DeviceContext *context = NULL;

  ID3D12Device *device12 = NULL;

  AGSDX11ReturnedParams::ExtensionsSupported extensionsSupported11 = {};
  AGSDX12ReturnedParams::ExtensionsSupported extensionsSupported12 = {};

#define AGS_FUNC(func) decltype(&::func) func = NULL;

#define AGS_FUNCS()                                \
  AGS_FUNC(agsDriverExtensionsDX12_CreateDevice);  \
  AGS_FUNC(agsDriverExtensionsDX12_DestroyDevice); \
  AGS_FUNC(agsDriverExtensionsDX11_CreateDevice);  \
  AGS_FUNC(agsDriverExtensionsDX11_DestroyDevice); \
  AGS_FUNC(agsInitialize);                         \
  AGS_FUNC(agsDeInitialize);

  AGS_FUNCS()
};

IAGSD3DDevice *InitialiseAGSReplay(uint32_t space, uint32_t reg)
{
#if ENABLED(RDOC_X64)
  const char *dll = "amd_ags_x64.dll";
#else
  const char *dll = "amd_ags_x86.dll";
#endif
  HMODULE ags = LoadLibraryA(LocatePluginFile("amd/ags", dll).c_str());

  if(ags == NULL)
  {
    RDCERR("Couldn't load ags DLL.");
    return NULL;
  }

  ReplayAGSD3DDevice *ret = new ReplayAGSD3DDevice(space, reg);

#undef AGS_FUNC
#define AGS_FUNC(func)                                                 \
  ret->func = (decltype(&::func))GetProcAddress(ags, STRINGIZE(func)); \
  if(!ret->func)                                                       \
  {                                                                    \
    RDCERR("Couldn't obtain %s from %s", STRINGIZE(func), dll);        \
    delete ret;                                                        \
    return NULL;                                                       \
  }

  AGS_FUNCS();

  int version = AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH);

  AGSGPUInfo info = {};
  AGSReturnCode agsRet = ret->agsInitialize(version, NULL, &ret->ags, &info);

  if(agsRet != AGS_SUCCESS)
  {
    RDCERR("AGS failed to initialise: %d", agsRet);
    delete ret;
    return NULL;
  }

  RDCLOG("Initialised AGS on replay: %d.%d.%d (%s / %s)", AMD_AGS_VERSION_MAJOR,
         AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH, info.driverVersion,
         info.radeonSoftwareVersion);

  return ret;
}
