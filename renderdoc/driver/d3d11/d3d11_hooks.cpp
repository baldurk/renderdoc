/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#if ENABLED(RDOC_X64)

#define BIT_SPECIFIC_DLL(dll32, dll64) dll64

#else

#define BIT_SPECIFIC_DLL(dll32, dll64) dll32

#endif

ID3D11Resource *UnwrapDXResource(void *dxObject);

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

// this is the type of the lambda we use to route the call out to the 'real' function inside our
// generic wrapper.
// Could be any of D3D11CreateDevice, D3D11CreateDeviceAndSwapChain, or the nvapi equivalents
typedef std::function<HRESULT(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL *,
                              UINT FeatureLevels, UINT, CONST DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **,
                              ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **)>
    RealFunction;

#define NVENCAPI __stdcall
enum NVENCSTATUS
{
  NV_ENC_SUCCESS = 0,
  NV_ENC_ERR_INVALID_PTR = 6,
};

enum NV_ENC_INPUT_RESOURCE_TYPE
{
  NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX = 0x0,
  NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR = 0x1,
  NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY = 0x2,
  NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX = 0x3
};

struct NV_ENC_REGISTER_RESOURCE
{
  uint32_t version;
  NV_ENC_INPUT_RESOURCE_TYPE resourceType;
  uint32_t dummy[4];
  void *resourceToRegister;

  // there is more data here but we don't allocate this structure only patch the above pointer, so
  // we don't need it
};

typedef NVENCSTATUS(NVENCAPI *PNVENCREGISTERRESOURCE)(void *, NV_ENC_REGISTER_RESOURCE *params);

struct NV_ENCODE_API_FUNCTION_LIST
{
  uint32_t version;
  uint32_t reserved;
  void *otherFunctions[30];    // other functions in the dispatch table
  PNVENCREGISTERRESOURCE nvEncRegisterResource;

  // there is more data here but we don't allocate this structure only patch the above pointer, so
  // we don't need it
};

typedef NVENCSTATUS(NVENCAPI *PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST *);

class D3D11Hook : LibraryHook
{
public:
  D3D11Hook() { m_InsideCreate = false; }
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

    // these are hooked to prevent AMD extensions from activating and causing later crashes when not
    // replayed correctly
    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("atidxx32.dll", "atidxx64.dll"), NULL);
    AmdCreate11.Register(BIT_SPECIFIC_DLL("atidxx32.dll", "atidxx64.dll"), "AmdDxExtCreate11",
                         AmdCreate11_hook);

    // nvapi has some seriously awful ""feature"" where it can wrap CreateDevice with some other
    // extra parameter. Since we don't want to hook nvapi when it calls out to d3d11.dll, we have to
    // intercept it here.
    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("nvapi.dll", "nvapi64.dll"), NULL);
    nvapi_QueryInterface.Register(BIT_SPECIFIC_DLL("nvapi.dll", "nvapi64.dll"),
                                  "nvapi_QueryInterface", nvapi_QueryInterface_hook);

    // we need to wrap nvcodec to handle unwrapping D3D11 pointers passed to it
    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("nvEncodeAPI.dll", "nvEncodeAPI64.dll"), NULL);
    NvEncodeCreate.Register(BIT_SPECIFIC_DLL("nvEncodeAPI.dll", "nvEncodeAPI64.dll"),
                            "NvEncodeAPICreateInstance", NvEncodeAPICreateInstance_hook);
  }

private:
  static D3D11Hook d3d11hooks;

  HookedFunction<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN> CreateDeviceAndSwapChain;
  HookedFunction<PFN_D3D11_CREATE_DEVICE> CreateDevice;

  // optional extension hooks
  HookedFunction<PFNAmdDxExtCreate11> AmdCreate11;
  HookedFunction<PFNNVQueryInterface> nvapi_QueryInterface;
  HookedFunction<PFN_NvEncodeAPICreateInstance> NvEncodeCreate;

  // re-entrancy detection (can happen in rare cases with e.g. fraps)
  bool m_InsideCreate;

  HRESULT Create_Internal(RealFunction real, __in_opt IDXGIAdapter *pAdapter,
                          D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                          __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                          UINT FeatureLevels, UINT SDKVersion,
                          __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                          __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
                          __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                          __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    // if we're already inside a wrapped create, then DON'T do anything special. Just call onwards
    if(m_InsideCreate)
    {
      return real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                  pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    }

    m_InsideCreate = true;

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

    HRESULT ret =
        real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
             pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

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

        if(ppImmediateContext)
        {
          if(*ppImmediateContext)
            (*ppImmediateContext)->Release();
          wrap->GetImmediateContext(ppImmediateContext);
        }

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

    m_InsideCreate = false;

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

    HRESULT ret = d3d11hooks.Create_Internal(
        createFunc, pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
        SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

    return ret;
  }

  static HRESULT __cdecl AmdCreate11_hook(ID3D11Device *pDevice, IAmdDxExt **ppExt)
  {
    RDCLOG("Attempt to create AMD extension interface via AmdDxExtCreate11 was blocked.");

    if(ppExt)
      *ppExt = NULL;

    return E_FAIL;
  }

  PFNNVCreateDevice nvapi_CreateDevice_real = NULL;
  PFNNVCreateDeviceAndSwapChain nvapi_CreateDeviceAndSwapChain_real = NULL;

  static HRESULT WINAPI nvapi_CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType,
                                           HMODULE Software, UINT Flags,
                                           CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                                           UINT FeatureLevels, UINT SDKVersion,
                                           ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
                                           ID3D11DeviceContext **ppImmediateContext,
                                           NVAPI_DEVICE_FEATURE_LEVEL *outNVLevel)
  {
    return d3d11hooks.Create_Internal(
        [=](IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
            CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
            CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
            ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
            ID3D11DeviceContext **ppImmediateContext) {
          // we know that when we come back in here the swapchain parameters will be NULL because
          // that's what we pass below
          RDCASSERT(!pSwapChainDesc && !ppSwapChain);
          return d3d11hooks.nvapi_CreateDevice_real(
              pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
              ppDevice, pFeatureLevel, ppImmediateContext, outNVLevel);
        },
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, NULL,
        NULL, ppDevice, pFeatureLevel, ppImmediateContext);
  }

  static HRESULT WINAPI nvapi_CreateDeviceAndSwapChain(
      IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
      CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
      ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
      ID3D11DeviceContext **ppImmediateContext, NVAPI_DEVICE_FEATURE_LEVEL *outNVLevel)
  {
    return d3d11hooks.Create_Internal(
        [=](IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
            CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
            CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
            ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
            ID3D11DeviceContext **ppImmediateContext) {
          return d3d11hooks.nvapi_CreateDeviceAndSwapChain_real(
              pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
              pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext, outNVLevel);
        },
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
        pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
  }

  static void *nvapi_QueryInterface_hook(uint32_t ID)
  {
    void *real = d3d11hooks.nvapi_QueryInterface()(ID);

    if(real == NULL)
      return real;

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
    else if(ID == 0xAD298D3F || ID == 0x0150E828 || ID == 0x33C7358C || ID == 0x593E8644)
    {
      // these seem to be fetched inside NvAPI_Initialize so we allow them through to avoid
      // causing problems
      return real;
    }
    else
    {
      if(RenderDoc::Inst().IsVendorExtensionEnabled(VendorExtensions::NvAPI))
      {
        RDCDEBUG("NvAPI allowed: Returning %p for nvapi_QueryInterface(%x)", real, ID);
        return real;
      }
      else
      {
        static int count = 0;
        if(count < 10)
          RDCWARN("NvAPI disabled: Returning NULL for nvapi_QueryInterface(%x)", ID);
        count++;
        return NULL;
      }
    }
  }

  PNVENCREGISTERRESOURCE real_nvEncRegisterResource = NULL;

  static NVENCSTATUS NVENCAPI NvEncodeAPIRegisterResource_hook(void *encoder,
                                                               NV_ENC_REGISTER_RESOURCE *params)
  {
    if(!d3d11hooks.real_nvEncRegisterResource)
    {
      RDCERR("nvEncRegisterResource called without hooking NvEncodeAPICreateInstance!");
      return NV_ENC_ERR_INVALID_PTR;
    }

    // only directx textures need to be unwrapped
    if(!encoder || !params || params->resourceType != NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX)
      return d3d11hooks.real_nvEncRegisterResource(encoder, params);

    // attempt to unwrap the handle in place
    void *origHandle = params->resourceToRegister;
    params->resourceToRegister = UnwrapDXResource(origHandle);
    if(params->resourceToRegister == NULL)
    {
      RDCERR("Failed to unwrap DX handle %p, falling back to pass-through", origHandle);
      params->resourceToRegister = origHandle;
    }

    // call out to the actual function
    NVENCSTATUS ret = d3d11hooks.real_nvEncRegisterResource(encoder, params);

    // restore the handle to the original value
    params->resourceToRegister = origHandle;

    return ret;
  }

  static NVENCSTATUS NVENCAPI NvEncodeAPICreateInstance_hook(NV_ENCODE_API_FUNCTION_LIST *functions)
  {
    NVENCSTATUS ret = d3d11hooks.NvEncodeCreate()(functions);

    if(ret == NV_ENC_SUCCESS && functions && functions->nvEncRegisterResource)
    {
      // this is an encoded struct version, 7 is a magic value, 8.1 is the major.minor of nvcodec
      // and 2 is the struct version
      const uint32_t expectedVersion = 7 << 28 | 8 << 1 | 1 << 24 | 2 << 16;
      if(functions->version != expectedVersion)
        RDCWARN("Call to NvEncodeAPICreateInstance with version %x, expected %x",
                functions->version, expectedVersion);

      // we don't handle multiple different pointers coming back, but that seems unlikely.
      RDCASSERT(d3d11hooks.real_nvEncRegisterResource == NULL ||
                d3d11hooks.real_nvEncRegisterResource == functions->nvEncRegisterResource);
      d3d11hooks.real_nvEncRegisterResource = functions->nvEncRegisterResource;

      functions->nvEncRegisterResource = &NvEncodeAPIRegisterResource_hook;
    }

    return ret;
  }
};

D3D11Hook D3D11Hook::d3d11hooks;
