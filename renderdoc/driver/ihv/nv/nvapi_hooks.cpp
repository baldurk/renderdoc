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

#include <unordered_map>
#include "common/formatting.h"
#include "core/core.h"
#include "driver/d3d11/d3d11_hooks.h"
#include "hooks/hooks.h"
#include "nvapi_wrapper.h"

#include "driver/dx/official/d3d11.h"
#include "driver/dx/official/d3d12.h"

#include "official/nvapi/nvapi.h"

namespace
{
#include "official/nvapi/nvapi_interface.h"
};

#if ENABLED(RDOC_X64)

#define BIT_SPECIFIC_DLL(dll32, dll64) dll64

#else

#define BIT_SPECIFIC_DLL(dll32, dll64) dll32

#endif

ID3D11Resource *UnwrapDXResource(void *dxObject);

typedef void *(__cdecl *PFNNVQueryInterface)(uint32_t ID);

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
    RealD3D11CreateFunction;

#define NVENCAPI __stdcall
enum NVENCSTATUS
{
  NV_ENC_SUCCESS = 0,
  NV_ENC_ERR_INVALID_PTR = 6,
};

enum NV_ENC_INPUT_RESOURCE_TYPE : uint32_t
{
  NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX = 0x0,
  NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR = 0x1,
  NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY = 0x2,
  NV_ENC_INPUT_RESOURCE_TYPE_OPENGL_TEX = 0x3
};

enum NV_ENC_DEVICE_TYPE : uint32_t
{
  NV_ENC_DEVICE_TYPE_DIRECTX = 0x0,
  NV_ENC_DEVICE_TYPE_CUDA = 0x1,
  NV_ENC_DEVICE_TYPE_OPENGL = 0x2,
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

struct NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS
{
  uint32_t version;
  NV_ENC_DEVICE_TYPE deviceType;
  void *device;

  // there is more data here but we don't allocate this structure only patch the above pointer, so
  // we don't need it
};

typedef NVENCSTATUS(NVENCAPI *PNVENCREGISTERRESOURCE)(void *encoder,
                                                      NV_ENC_REGISTER_RESOURCE *params);
typedef NVENCSTATUS(NVENCAPI *PNVENCOPENENCODESESSION)(void *device, uint32_t devType,
                                                       void **encoder);
typedef NVENCSTATUS(NVENCAPI *PNVENCOPENENCODESESSIONEX)(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS *params,
                                                         void **encoder);

struct NV_ENCODE_API_FUNCTION_LIST
{
  uint32_t version;
  uint32_t reserved;
  PNVENCOPENENCODESESSION nvEncOpenEncodeSession;
  void *otherFunctions[28];    // other functions in the dispatch table
  PNVENCOPENENCODESESSIONEX nvEncOpenEncodeSessionEx;
  PNVENCREGISTERRESOURCE nvEncRegisterResource;

  // there is more data here but we don't allocate this structure only patch the above pointers, so
  // we don't need it
};

typedef NVENCSTATUS(NVENCAPI *PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST *);

class NVHook : LibraryHook
{
public:
  void RegisterHooks()
  {
    RDCLOG("Registering nvidia hooks");

    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("nvapi.dll", "nvapi64.dll"), NULL);
    nvapi_QueryInterface.Register(BIT_SPECIFIC_DLL("nvapi.dll", "nvapi64.dll"),
                                  "nvapi_QueryInterface", nvapi_QueryInterface_hook);

    // we need to wrap nvcodec to handle unwrapping D3D11 pointers passed to it
    LibraryHooks::RegisterLibraryHook(BIT_SPECIFIC_DLL("nvEncodeAPI.dll", "nvEncodeAPI64.dll"), NULL);
    NvEncodeCreate.Register(BIT_SPECIFIC_DLL("nvEncodeAPI.dll", "nvEncodeAPI64.dll"),
                            "NvEncodeAPICreateInstance", NvEncodeAPICreateInstance_hook);

    // we need ID -> function lookup, not function -> ID as the table gives us. We also want fairly
    // quick lookup since some programs call nvapi_QueryInterface at *high* frequency.
    for(const NVAPI_INTERFACE_TABLE &iface : nvapi_interface_table)
      nvapi_lookup[iface.id] = iface.func;
  }

private:
  static NVHook nvhooks;

  std::unordered_map<uint32_t, const char *> nvapi_lookup;

  HookedFunction<PFNNVQueryInterface> nvapi_QueryInterface;
  HookedFunction<PFN_NvEncodeAPICreateInstance> NvEncodeCreate;

#define HOOK_NVAPI(fname, ID) HookedFunction<decltype(&::fname)> fname;
#define WHITELIST_NVAPI(fname, ID)

#define NVAPI_FUNCS()                                                      \
  HOOK_NVAPI(NvAPI_Initialize, 0x0150e828);                                \
  HOOK_NVAPI(NvAPI_D3D11_CreateDevice, 0x6a16d3a0);                        \
  HOOK_NVAPI(NvAPI_D3D11_CreateDeviceAndSwapChain, 0xbb939ee5);            \
  HOOK_NVAPI(NvAPI_D3D11_IsNvShaderExtnOpCodeSupported, 0x5f68da40);       \
  HOOK_NVAPI(NvAPI_D3D11_SetNvShaderExtnSlot, 0x8e90bb9f);                 \
  HOOK_NVAPI(NvAPI_D3D11_SetNvShaderExtnSlotLocalThread, 0x0e6482a0);      \
  HOOK_NVAPI(NvAPI_D3D12_IsNvShaderExtnOpCodeSupported, 0x3dfacec8);       \
  HOOK_NVAPI(NvAPI_D3D12_SetNvShaderExtnSlotSpace, 0xac2dfeb5);            \
  HOOK_NVAPI(NvAPI_D3D12_SetNvShaderExtnSlotSpaceLocalThread, 0x43d867c0); \
  HOOK_NVAPI(NvAPI_D3D12_CreateGraphicsPipelineState, 0x2fc28856);         \
  HOOK_NVAPI(NvAPI_D3D12_CreateComputePipelineState, 0x2762deac);          \
  WHITELIST_NVAPI(NvAPI_Unload, 0xd22bdd7e);                               \
  WHITELIST_NVAPI(NvAPI_GetErrorMessage, 0x6c2d048c);                      \
  WHITELIST_NVAPI(NvAPI_GetInterfaceVersionString, 0x01053fa5);

  NVAPI_FUNCS();

  static NvAPI_Status __cdecl NvAPI_D3D11_IsNvShaderExtnOpCodeSupported_hook(__in IUnknown *pDev,
                                                                             __in NvU32 opCode,
                                                                             __out bool *pSupported)
  {
    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDev->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      return nvhooks.NvAPI_D3D11_IsNvShaderExtnOpCodeSupported()(nvapiDev->GetReal(), opCode,
                                                                 pSupported);
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D12_IsNvShaderExtnOpCodeSupported_hook(__in IUnknown *pDev,
                                                                             __in NvU32 opCode,
                                                                             __out bool *pSupported)
  {
    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDev->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      IUnknown *real = nvapiDev->GetReal();

      ID3D12Device *dev = NULL;
      hr = real->QueryInterface(__uuidof(ID3D12Device), (void **)&dev);

      if(SUCCEEDED(hr))
      {
        NvAPI_Status ret =
            nvhooks.NvAPI_D3D12_IsNvShaderExtnOpCodeSupported()(dev, opCode, pSupported);

        dev->Release();

        return ret;
      }
      else
      {
        RDCERR("Couldn't retrieve ID3D12Device from RenderDoc-wrapped device");
        return NVAPI_INVALID_POINTER;
      }
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D11_SetNvShaderExtnSlot_hook(__in IUnknown *pDev,
                                                                   __in NvU32 uavSlot)
  {
    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDev->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      NvAPI_Status ret = nvhooks.NvAPI_D3D11_SetNvShaderExtnSlot()(nvapiDev->GetReal(), uavSlot);

      nvapiDev->SetShaderExtUAV(~0U, uavSlot, TRUE);

      return ret;
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D11_SetNvShaderExtnSlotLocalThread_hook(__in IUnknown *pDev,
                                                                              __in NvU32 uavSlot)
  {
    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDev->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      NvAPI_Status ret =
          nvhooks.NvAPI_D3D11_SetNvShaderExtnSlotLocalThread()(nvapiDev->GetReal(), uavSlot);

      nvapiDev->SetShaderExtUAV(~0U, uavSlot, FALSE);

      return ret;
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D12_SetNvShaderExtnSlotSpace_hook(__in IUnknown *pDev,
                                                                        __in NvU32 uavSlot,
                                                                        __in NvU32 uavSpace)
  {
    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDev->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      NvAPI_Status ret =
          nvhooks.NvAPI_D3D12_SetNvShaderExtnSlotSpace()(nvapiDev->GetReal(), uavSlot, uavSpace);

      nvapiDev->SetShaderExtUAV(uavSpace, uavSlot, TRUE);

      return ret;
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D12_SetNvShaderExtnSlotSpaceLocalThread_hook(
      __in IUnknown *pDev, __in NvU32 uavSlot, __in NvU32 uavSpace)
  {
    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDev->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      NvAPI_Status ret = nvhooks.NvAPI_D3D12_SetNvShaderExtnSlotSpaceLocalThread()(
          nvapiDev->GetReal(), uavSlot, uavSpace);

      nvapiDev->SetShaderExtUAV(uavSpace, uavSlot, FALSE);

      return ret;
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D12_CreateGraphicsPipelineState_hook(
      __in ID3D12Device *pDevice, __in const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pPSODesc,
      NvU32 numExtensions, __in const NVAPI_D3D12_PSO_EXTENSION_DESC **ppExtensions,
      __out ID3D12PipelineState **ppPSO)
  {
    // check that there's only supported extensions first, and extract the info we want.
    uint32_t reg = 0, space = 0;
    for(NvU32 i = 0; i < numExtensions; i++)
    {
      if(ppExtensions[i]->psoExtension != NV_PSO_SET_SHADER_EXTNENSION_SLOT_AND_SPACE)
      {
        RDCWARN("Unsupported D3D12 PSO extension: %d", ppExtensions[i]->psoExtension);
        return NVAPI_NOT_SUPPORTED;
      }

      // the versions don't look to be backwards compatible so we have to require an exact version
      if(ppExtensions[i]->baseVersion != NV_PSO_EXTENSION_DESC_VER)
      {
        RDCERR("Unsupported PSO extension version %x, expected %x", ppExtensions[i]->baseVersion,
               NV_PSO_EXTENSION_DESC_VER);
        return NVAPI_NOT_SUPPORTED;
      }

      const NVAPI_D3D12_PSO_SET_SHADER_EXTENSION_SLOT_DESC_V1 *psoExt =
          (const NVAPI_D3D12_PSO_SET_SHADER_EXTENSION_SLOT_DESC_V1 *)ppExtensions[i];

      if(psoExt->version != NV_SET_SHADER_EXTENSION_SLOT_DESC_VER)
      {
        RDCERR("Unsupported set-slot extension version %x, expected %x", psoExt->version,
               NV_SET_SHADER_EXTENSION_SLOT_DESC_VER);
        return NVAPI_NOT_SUPPORTED;
      }

      reg = psoExt->uavSlot;
      space = psoExt->registerSpace;
    }

    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDevice->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      IUnknown *real = nvapiDev->GetReal();

      ID3D12Device *dev = NULL;
      hr = real->QueryInterface(__uuidof(ID3D12Device), (void **)&dev);

      if(SUCCEEDED(hr))
      {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = *pPSODesc;
        nvapiDev->UnwrapDesc(&desc);

        ID3D12PipelineState *realPSO = NULL;
        NvAPI_Status ret = nvhooks.NvAPI_D3D12_CreateGraphicsPipelineState()(
            dev, &desc, numExtensions, ppExtensions, &realPSO);

        dev->Release();

        if(ret == NVAPI_OK)
        {
          *ppPSO = nvapiDev->ProcessCreatedGraphicsPipelineState(pPSODesc, reg, space, realPSO);
          return NVAPI_OK;
        }
        else
        {
          SAFE_RELEASE(realPSO);
        }

        return ret;
      }
      else
      {
        RDCERR("Couldn't retrieve ID3D12Device from RenderDoc-wrapped device");
        return NVAPI_INVALID_POINTER;
      }
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_D3D12_CreateComputePipelineState_hook(
      __in ID3D12Device *pDevice, __in const D3D12_COMPUTE_PIPELINE_STATE_DESC *pPSODesc,
      NvU32 numExtensions, __in const NVAPI_D3D12_PSO_EXTENSION_DESC **ppExtensions,
      __out ID3D12PipelineState **ppPSO)
  {
    // check that there's only supported extensions first, and extract the info we want.
    uint32_t reg = 0, space = 0;
    for(NvU32 i = 0; i < numExtensions; i++)
    {
      if(ppExtensions[i]->psoExtension != NV_PSO_SET_SHADER_EXTNENSION_SLOT_AND_SPACE)
      {
        RDCWARN("Unsupported D3D12 PSO extension: %d", ppExtensions[i]->psoExtension);
        return NVAPI_NOT_SUPPORTED;
      }

      // the versions don't look to be backwards compatible so we have to require an exact version
      if(ppExtensions[i]->baseVersion != NV_PSO_EXTENSION_DESC_VER)
      {
        RDCERR("Unsupported PSO extension version %x, expected %x", ppExtensions[i]->baseVersion,
               NV_PSO_EXTENSION_DESC_VER);
        return NVAPI_NOT_SUPPORTED;
      }

      const NVAPI_D3D12_PSO_SET_SHADER_EXTENSION_SLOT_DESC_V1 *psoExt =
          (const NVAPI_D3D12_PSO_SET_SHADER_EXTENSION_SLOT_DESC_V1 *)ppExtensions[i];

      if(psoExt->version != NV_SET_SHADER_EXTENSION_SLOT_DESC_VER)
      {
        RDCERR("Unsupported set-slot extension version %x, expected %x", psoExt->version,
               NV_SET_SHADER_EXTENSION_SLOT_DESC_VER);
        return NVAPI_NOT_SUPPORTED;
      }

      reg = psoExt->uavSlot;
      space = psoExt->registerSpace;
    }

    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = pDevice->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);
    // this will only succeed if it's our own wrapped device. It doesn't change the refcount, this
    // is a COM-breaking backdoor
    if(SUCCEEDED(hr))
    {
      IUnknown *real = nvapiDev->GetReal();

      ID3D12Device *dev = NULL;
      hr = real->QueryInterface(__uuidof(ID3D12Device), (void **)&dev);

      if(SUCCEEDED(hr))
      {
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = *pPSODesc;
        nvapiDev->UnwrapDesc(&desc);

        ID3D12PipelineState *realPSO = NULL;
        NvAPI_Status ret = nvhooks.NvAPI_D3D12_CreateComputePipelineState()(
            dev, &desc, numExtensions, ppExtensions, &realPSO);

        dev->Release();

        if(ret == NVAPI_OK)
        {
          *ppPSO = nvapiDev->ProcessCreatedComputePipelineState(pPSODesc, reg, space, realPSO);
          return NVAPI_OK;
        }
        else
        {
          SAFE_RELEASE(realPSO);
        }

        return ret;
      }
      else
      {
        RDCERR("Couldn't retrieve ID3D12Device from RenderDoc-wrapped device");
        return NVAPI_INVALID_POINTER;
      }
    }
    else
    {
      RDCERR("Didn't pass RenderDoc-wrapped device to nvapi function");
      return NVAPI_INVALID_POINTER;
    }
  }

  static NvAPI_Status __cdecl NvAPI_Initialize_hook()
  {
    NvAPI_Status ret = nvhooks.NvAPI_Initialize()();

    if(ret == NVAPI_OK)
    {
      using PFN_NvAPI_GetInterfaceVersionString = decltype(&::NvAPI_GetInterfaceVersionString);
      PFN_NvAPI_GetInterfaceVersionString getver =
          (PFN_NvAPI_GetInterfaceVersionString)nvhooks.nvapi_QueryInterface()(0x01053fa5);

      if(getver)
      {
        NvAPI_ShortString ver = {};
        getver(ver);
        if(ver[0])
          RDCLOG("Initialised nvapi, version %s", ver);
        else
          RDCLOG("Initialised nvapi, unknown version");
      }
      else
      {
        RDCLOG("Initialised nvapi, unknown version");
      }
    }
    else
    {
      RDCERR("Error in NvAPI_Initialize: %d", ret);
    }

    return ret;
  }

  static HRESULT __cdecl NvAPI_D3D11_CreateDevice_hook(
      IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
      ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
      ID3D11DeviceContext **ppImmediateContext, NVAPI_DEVICE_FEATURE_LEVEL *outNVLevel)
  {
    return CreateD3D11_Internal(
        [=](IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
            CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
            CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
            ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
            ID3D11DeviceContext **ppImmediateContext) {
          // we know that when we come back in here the swapchain parameters will be NULL because
          // that's what we pass below
          RDCASSERT(!pSwapChainDesc && !ppSwapChain);
          return nvhooks.NvAPI_D3D11_CreateDevice()(
              pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
              ppDevice, pFeatureLevel, ppImmediateContext, outNVLevel);
        },
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, NULL,
        NULL, ppDevice, pFeatureLevel, ppImmediateContext);
  }

  static HRESULT __cdecl NvAPI_D3D11_CreateDeviceAndSwapChain_hook(
      IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
      CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
      ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
      ID3D11DeviceContext **ppImmediateContext, NVAPI_DEVICE_FEATURE_LEVEL *outNVLevel)
  {
    return CreateD3D11_Internal(
        [=](IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
            CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
            CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain,
            ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel,
            ID3D11DeviceContext **ppImmediateContext) {
          return nvhooks.NvAPI_D3D11_CreateDeviceAndSwapChain()(
              pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
              pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext, outNVLevel);
        },
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
        pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
  }

  static void *nvapi_QueryInterface_hook(uint32_t ID)
  {
    void *real = nvhooks.nvapi_QueryInterface()(ID);

    if(real == NULL)
      return real;

#undef HOOK_NVAPI
#define HOOK_NVAPI(fname, ID)       \
  case ID:                          \
  {                                 \
    nvhooks.fname.SetFuncPtr(real); \
    return &CONCAT(fname, _hook);   \
  }
#undef WHITELIST_NVAPI
#define WHITELIST_NVAPI(fname, ID) \
  case ID: return real;

    switch(ID)
    {
      NVAPI_FUNCS();
      // unknown, but these are fetched inside NvAPI_Initialize so allow them through to avoid
      // causing problems.
      WHITELIST_NVAPI(unknown_func, 0xad298d3f);
      WHITELIST_NVAPI(unknown_func, 0x33c7358c);
      WHITELIST_NVAPI(unknown_func, 0x593e8644);
      default: break;
    }

    const char *cname = nvhooks.nvapi_lookup[ID];
    rdcstr name = cname ? cname : StringFormat::Fmt("0x%x", ID);

    if(RenderDoc::Inst().IsVendorExtensionEnabled(VendorExtensions::NvAPI))
    {
      RDCDEBUG("NvAPI allowed: Returning %p for nvapi_QueryInterface(%s)", real, name.c_str());
      return real;
    }
    else
    {
      static int count = 0;
      if(count < 10)
        RDCWARN("NvAPI disabled: Returning NULL for nvapi_QueryInterface(%s)", name.c_str());
      count++;
      return NULL;
    }
  }

  PNVENCOPENENCODESESSION real_nvEncOpenEncodeSession = NULL;
  PNVENCOPENENCODESESSIONEX real_nvEncOpenEncodeSessionEx = NULL;
  PNVENCREGISTERRESOURCE real_nvEncRegisterResource = NULL;

  static NVENCSTATUS NVENCAPI NvEncodeAPIOpenEncodeSession_hook(void *device, uint32_t devType,
                                                                void **encoder)
  {
    if(!nvhooks.real_nvEncOpenEncodeSession)
    {
      RDCERR("nvEncOpenEncodeSession called without hooking NvEncodeAPICreateInstance!");
      return NV_ENC_ERR_INVALID_PTR;
    }

    if(devType != NV_ENC_DEVICE_TYPE_DIRECTX)
    {
      RDCWARN("Unsupported device type %d in encode session, passing through but this may break!",
              devType);
      return nvhooks.real_nvEncOpenEncodeSession(device, devType, encoder);
    }

    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr = ((IUnknown *)device)->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);

    if(FAILED(hr))
    {
      RDCERR("nvEncOpenEncodeSession called with invalid non-wrapped device!");
      return NV_ENC_ERR_INVALID_PTR;
    }

    return nvhooks.real_nvEncOpenEncodeSession(nvapiDev->GetReal(), devType, encoder);
  }

  static NVENCSTATUS NVENCAPI
  NvEncodeAPIOpenEncodeSessionEx_hook(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS *params, void **encoder)
  {
    if(!nvhooks.real_nvEncOpenEncodeSessionEx)
    {
      RDCERR("nvEncOpenEncodeSessionEx called without hooking NvEncodeAPICreateInstance!");
      return NV_ENC_ERR_INVALID_PTR;
    }

    if(params->deviceType != NV_ENC_DEVICE_TYPE_DIRECTX)
    {
      RDCWARN("Unsupported device type %d in encode session, passing through but this may break!",
              params->deviceType);
      return nvhooks.real_nvEncOpenEncodeSessionEx(params, encoder);
    }

    // attempt to unwrap the handle in place
    void *origDevice = params->device;

    INVAPID3DDevice *nvapiDev = NULL;
    HRESULT hr =
        ((IUnknown *)origDevice)->QueryInterface(__uuidof(INVAPID3DDevice), (void **)&nvapiDev);

    if(FAILED(hr))
    {
      RDCERR("nvEncOpenEncodeSessionEx called with invalid non-wrapped device!");
      return NV_ENC_ERR_INVALID_PTR;
    }

    params->device = nvapiDev->GetReal();

    // call out to the actual function
    NVENCSTATUS ret = nvhooks.real_nvEncOpenEncodeSessionEx(params, encoder);

    // restore the handle to the original value
    params->device = origDevice;

    return ret;
  }

  static NVENCSTATUS NVENCAPI NvEncodeAPIRegisterResource_hook(void *encoder,
                                                               NV_ENC_REGISTER_RESOURCE *params)
  {
    if(!nvhooks.real_nvEncRegisterResource)
    {
      RDCERR("nvEncRegisterResource called without hooking NvEncodeAPICreateInstance!");
      return NV_ENC_ERR_INVALID_PTR;
    }

    // only directx textures need to be unwrapped
    if(!encoder || !params || params->resourceType != NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX)
      return nvhooks.real_nvEncRegisterResource(encoder, params);

    // attempt to unwrap the handle in place
    void *origHandle = params->resourceToRegister;
    params->resourceToRegister = UnwrapDXResource(origHandle);
    if(params->resourceToRegister == NULL)
    {
      RDCERR("Failed to unwrap DX handle %p, falling back to pass-through", origHandle);
      params->resourceToRegister = origHandle;
    }

    // call out to the actual function
    NVENCSTATUS ret = nvhooks.real_nvEncRegisterResource(encoder, params);

    // restore the handle to the original value
    params->resourceToRegister = origHandle;

    return ret;
  }

  static NVENCSTATUS NVENCAPI NvEncodeAPICreateInstance_hook(NV_ENCODE_API_FUNCTION_LIST *functions)
  {
    NVENCSTATUS ret = nvhooks.NvEncodeCreate()(functions);

    if(ret == NV_ENC_SUCCESS && functions && functions->nvEncRegisterResource)
    {
      // this is an encoded struct version, 7 is a magic value, 8.1 is the major.minor of nvcodec
      // and 2 is the struct version
      const uint32_t expectedVersion8_1 = 7 << 28 | 8 << 0 | 1 << 24 | 2 << 16;
      const uint32_t expectedVersion11_0 = 7 << 28 | 11 << 0 | 0 << 24 | 2 << 16;
      if(functions->version != expectedVersion8_1 && functions->version != expectedVersion11_0)
        RDCWARN("Call to NvEncodeAPICreateInstance with untested version %x", functions->version);

      // we don't handle multiple different pointers coming back, but that seems unlikely.
      RDCASSERT(nvhooks.real_nvEncRegisterResource == NULL ||
                nvhooks.real_nvEncRegisterResource == functions->nvEncRegisterResource);
      nvhooks.real_nvEncRegisterResource = functions->nvEncRegisterResource;

      // we don't handle multiple different pointers coming back, but that seems unlikely.
      RDCASSERT(nvhooks.real_nvEncOpenEncodeSession == NULL ||
                nvhooks.real_nvEncOpenEncodeSession == functions->nvEncOpenEncodeSession);
      nvhooks.real_nvEncOpenEncodeSession = functions->nvEncOpenEncodeSession;

      // we don't handle multiple different pointers coming back, but that seems unlikely.
      RDCASSERT(nvhooks.real_nvEncOpenEncodeSessionEx == NULL ||
                nvhooks.real_nvEncOpenEncodeSessionEx == functions->nvEncOpenEncodeSessionEx);
      nvhooks.real_nvEncOpenEncodeSessionEx = functions->nvEncOpenEncodeSessionEx;

      functions->nvEncRegisterResource = &NvEncodeAPIRegisterResource_hook;
      functions->nvEncOpenEncodeSession = &NvEncodeAPIOpenEncodeSession_hook;
      functions->nvEncOpenEncodeSessionEx = &NvEncodeAPIOpenEncodeSessionEx_hook;
    }

    return ret;
  }
};

NVHook NVHook::nvhooks;
