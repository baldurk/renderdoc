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

#include "nvapi_wrapper.h"
#include "common/common.h"
#include "core/settings.h"

#include "driver/dx/official/d3d11.h"
#include "driver/dx/official/d3d12.h"

#include "official/nvapi/nvapi.h"

namespace
{
#include "official/nvapi/nvapi_interface.h"
};

typedef void *(*PFN_nvapi_QueryInterface)(NvU32 id);

struct ReplayNVAPID3DDevice : INVAPID3DDevice
{
public:
  virtual BOOL STDMETHODCALLTYPE SetReal(IUnknown *d)
  {
    if(device)
      device->Release();
    if(d3d12)
      d3d12->Release();

    device = d;
    device->AddRef();
    d3d12 = NULL;
    device->QueryInterface(__uuidof(ID3D12Device), (void **)&d3d12);

    // check that the nvapi can be used on this device
    BOOL ret = SetShaderExtUAV(0, 7, TRUE);
    SetShaderExtUAV(~0U, ~0U, TRUE);
    return ret;
  }

  ~ReplayNVAPID3DDevice()
  {
    if(device)
      device->Release();
    if(d3d12)
      d3d12->Release();
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

  // this should only be used on capture
  virtual IUnknown *STDMETHODCALLTYPE GetReal() { return NULL; }
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD space, DWORD reg, BOOL global)
  {
    if(d3d12)
    {
      if(NvAPI_D3D12_SetNvShaderExtnSlotSpace)
      {
        NvAPI_Status ret = NvAPI_D3D12_SetNvShaderExtnSlotSpace(d3d12, reg, space);
        return ret == NVAPI_OK ? TRUE : FALSE;
      }
    }
    else
    {
      if(NvAPI_D3D11_SetNvShaderExtnSlot)
      {
        NvAPI_Status ret = NvAPI_D3D11_SetNvShaderExtnSlot(device, reg);
        return ret == NVAPI_OK ? TRUE : FALSE;
      }
    }
    return FALSE;
  }

  // only used on capture
  virtual void STDMETHODCALLTYPE UnwrapDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc) {}
  virtual void STDMETHODCALLTYPE UnwrapDesc(D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc) {}
  virtual ID3D12PipelineState *STDMETHODCALLTYPE
  ProcessCreatedGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, uint32_t reg,
                                      uint32_t space, ID3D12PipelineState *realPSO)
  {
    return NULL;
  }
  virtual ID3D12PipelineState *STDMETHODCALLTYPE
  ProcessCreatedComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, uint32_t reg,
                                     uint32_t space, ID3D12PipelineState *realPSO)
  {
    return NULL;
  }

private:
  friend INVAPID3DDevice *InitialiseNVAPIReplay();

  ULONG ref = 1;

  IUnknown *device = NULL;
  ID3D12Device *d3d12 = NULL;

#define NVAPI_FUNC(func) decltype(&::func) func = NULL;

#define NVAPI_FUNCS()                               \
  NVAPI_FUNC(NvAPI_D3D11_SetNvShaderExtnSlot);      \
  NVAPI_FUNC(NvAPI_D3D12_SetNvShaderExtnSlotSpace); \
  NVAPI_FUNC(NvAPI_GetInterfaceVersionString);

  NVAPI_FUNCS()
};

uint32_t getId(const char *name)
{
  // slow lookup, we only check a couple of functions
  for(NVAPI_INTERFACE_TABLE &table : nvapi_interface_table)
    if(!strcmp(table.func, name))
      return table.id;

  RDCERR("Couldn't get function ID for %s", name);

  return 0;
}

// try to initialise nvapi for replay
INVAPID3DDevice *InitialiseNVAPIReplay()
{
#if ENABLED(RDOC_X64)
  HMODULE nvapi = LoadLibraryA("nvapi64.dll");
#else
  HMODULE nvapi = LoadLibraryA("nvapi.dll");
#endif

  if(nvapi == NULL)
  {
    RDCERR("Couldn't load nvapi DLL.");
    return NULL;
  }

  PFN_nvapi_QueryInterface nvapi_QueryInterface =
      (PFN_nvapi_QueryInterface)GetProcAddress(nvapi, "nvapi_QueryInterface");

  using PFN_NvAPI_Initialize = decltype(&::NvAPI_Initialize);

  PFN_NvAPI_Initialize NvAPI_Initialize =
      (PFN_NvAPI_Initialize)nvapi_QueryInterface(getId("NvAPI_Initialize"));

  NvAPI_Status nvResult = NvAPI_Initialize();

  if(nvResult != NVAPI_OK)
  {
    RDCERR("NvAPI_Initialize returned %d", nvResult);
    return NULL;
  }

  ReplayNVAPID3DDevice *ret = new ReplayNVAPID3DDevice();

#undef NVAPI_FUNC
#define NVAPI_FUNC(func) \
  ret->func = (decltype(&::func))nvapi_QueryInterface(getId(STRINGIZE(func)));

  NVAPI_FUNCS();

  NvAPI_ShortString nvapiVer;

  ret->NvAPI_GetInterfaceVersionString(nvapiVer);

  RDCLOG("Initialised nvapi on replay: %s", nvapiVer);

  return ret;
}
