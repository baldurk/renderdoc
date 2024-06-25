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

#include "d3d12_device.h"
#include <algorithm>
#include "core/core.h"
#include "core/settings.h"
#include "data/hlsl/hlsl_cbuffers.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/ihv/amd/amd_rgp.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3D.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_rendertext.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

RDOC_DEBUG_CONFIG(bool, D3D12_Debug_SingleSubmitFlushing, false,
                  "Every command buffer is submitted and fully flushed to the GPU, to narrow down "
                  "the source of problems.");

WRAPPED_POOL_INST(WrappedID3D12Device);

Threading::CriticalSection WrappedID3D12Device::m_DeviceWrappersLock;
std::map<ID3D12Device *, WrappedID3D12Device *> WrappedID3D12Device::m_DeviceWrappers;

void WrappedID3D12Device::RemoveQueue(WrappedID3D12CommandQueue *queue)
{
  m_Queues.removeOne(queue);
}

rdcstr WrappedID3D12Device::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D12Chunk)idx);
}

D3D12ShaderCache *WrappedID3D12Device::GetShaderCache()
{
  if(m_ShaderCache == NULL)
    m_ShaderCache = new D3D12ShaderCache(this);
  return m_ShaderCache;
}

D3D12DebugManager *WrappedID3D12Device::GetDebugManager()
{
  return m_Replay->GetDebugManager();
}

ULONG STDMETHODCALLTYPE DummyID3D12InfoQueue::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12InfoQueue::Release()
{
  m_pDevice->Release();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugDevice::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugDevice::Release()
{
  m_pDevice->Release();
  return 1;
}

HRESULT STDMETHODCALLTYPE DummyID3D12DebugDevice::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(ID3D12InfoQueue) || riid == __uuidof(ID3D12InfoQueue1) ||
     riid == __uuidof(ID3D12DebugDevice) || riid == __uuidof(ID3D12Device) ||
     riid == __uuidof(ID3D12Device1) || riid == __uuidof(ID3D12Device2) ||
     riid == __uuidof(ID3D12Device3) || riid == __uuidof(ID3D12Device4) ||
     riid == __uuidof(ID3D12Device5) || riid == __uuidof(ID3D12Device6) ||
     riid == __uuidof(ID3D12Device7) || riid == __uuidof(ID3D12Device8) ||
     riid == __uuidof(ID3D12Device9) || riid == __uuidof(ID3D12Device10) ||
     riid == __uuidof(ID3D12Device11) || riid == __uuidof(ID3D12Device12))
    return m_pDevice->QueryInterface(riid, ppvObject);

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12DebugDevice *)this;
    AddRef();
    return S_OK;
  }

  WarnUnknownGUID("ID3D12DebugDevice", riid);

  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugDevice::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugDevice::Release()
{
  m_pDevice->Release();
  return 1;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12DebugDevice::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(ID3D12InfoQueue) || riid == __uuidof(ID3D12InfoQueue1) ||
     riid == __uuidof(ID3D12DebugDevice) || riid == __uuidof(ID3D12Device) ||
     riid == __uuidof(ID3D12Device1) || riid == __uuidof(ID3D12Device2) ||
     riid == __uuidof(ID3D12Device3) || riid == __uuidof(ID3D12Device4) ||
     riid == __uuidof(ID3D12Device5) || riid == __uuidof(ID3D12Device6) ||
     riid == __uuidof(ID3D12Device7) || riid == __uuidof(ID3D12Device8) ||
     riid == __uuidof(ID3D12Device9) || riid == __uuidof(ID3D12Device10) ||
     riid == __uuidof(ID3D12Device11) || riid == __uuidof(ID3D12Device12))
    return m_pDevice->QueryInterface(riid, ppvObject);

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12DebugDevice *)this;
    AddRef();
    return S_OK;
  }

  WarnUnknownGUID("ID3D12DebugDevice", riid);

  return m_pDebug->QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE WrappedDownlevelDevice::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pDevice.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedDownlevelDevice::AddRef()
{
  return m_pDevice.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedDownlevelDevice::Release()
{
  return m_pDevice.Release();
}

HRESULT STDMETHODCALLTYPE WrappedDownlevelDevice::QueryVideoMemoryInfo(
    UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
    _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
{
  return m_pDevice.QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12SharingContract::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pDevice.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedID3D12SharingContract::AddRef()
{
  return m_pDevice.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedID3D12SharingContract::Release()
{
  return m_pDevice.Release();
}

ID3DDevice *WrappedID3D12SharingContract::GetD3DDevice()
{
  return &m_pDevice;
}

UINT WrappedID3D12SharingContract::GetWidth()
{
  D3D12_RESOURCE_DESC desc = m_pPresentSource->GetDesc();
  return (UINT)desc.Width;
}

UINT WrappedID3D12SharingContract::GetHeight()
{
  D3D12_RESOURCE_DESC desc = m_pPresentSource->GetDesc();
  return desc.Height;
}

DXGI_FORMAT WrappedID3D12SharingContract::GetFormat()
{
  D3D12_RESOURCE_DESC desc = m_pPresentSource->GetDesc();
  return desc.Format;
}

HWND WrappedID3D12SharingContract::GetHWND()
{
  return m_pPresentHWND;
}

void STDMETHODCALLTYPE WrappedID3D12SharingContract::Present(_In_ ID3D12Resource *pResource,
                                                             UINT Subresource, _In_ HWND window)
{
  // this is similar to the downlevel queue present

  if(m_pPresentHWND != NULL)
  {
    // don't let the device actually release any refs on the resource, just make it release internal
    // resources
    m_pPresentSource->AddRef();
    m_pDevice.ReleaseSwapchainResources(this, 0, NULL, NULL);
  }

  if(m_pPresentHWND != window)
  {
    if(m_pPresentHWND != NULL)
    {
      Keyboard::RemoveInputWindow(WindowingSystem::Win32, m_pPresentHWND);
      RenderDoc::Inst().RemoveFrameCapturer(
          DeviceOwnedWindow(m_pDevice.GetFrameCapturerDevice(), m_pPresentHWND));
    }

    Keyboard::AddInputWindow(WindowingSystem::Win32, window);

    RenderDoc::Inst().AddFrameCapturer(
        DeviceOwnedWindow(m_pDevice.GetFrameCapturerDevice(), window), m_pDevice.GetFrameCapturer());
  }

  m_pPresentSource = pResource;
  m_pPresentHWND = window;

  m_pDevice.WrapSwapchainBuffer(this, GetFormat(), 0, m_pPresentSource);

  m_pDevice.Present(NULL, this, 0, 0);

  m_pReal->Present(Unwrap(pResource), Subresource, window);
}

void STDMETHODCALLTYPE WrappedID3D12SharingContract::SharedFenceSignal(_In_ ID3D12Fence *pFence,
                                                                       UINT64 FenceValue)
{
  m_pReal->SharedFenceSignal(Unwrap(pFence), FenceValue);
}

void STDMETHODCALLTYPE WrappedID3D12SharingContract::BeginCapturableWork(_In_ REFGUID guid)
{
  // undocumented what this does
  m_pReal->BeginCapturableWork(guid);
}

void STDMETHODCALLTYPE WrappedID3D12SharingContract::EndCapturableWork(_In_ REFGUID guid)
{
  // undocumented what this does
  m_pReal->EndCapturableWork(guid);
}

HRESULT STDMETHODCALLTYPE WrappedDRED::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pDevice.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedDRED::AddRef()
{
  return m_pDevice.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedDRED::Release()
{
  return m_pDevice.Release();
}

HRESULT STDMETHODCALLTYPE WrappedDREDSettings::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pDevice.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedDREDSettings::AddRef()
{
  return m_pDevice.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedDREDSettings::Release()
{
  return m_pDevice.Release();
}

HRESULT STDMETHODCALLTYPE WrappedCompatibilityDevice::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pDevice.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedCompatibilityDevice::AddRef()
{
  return m_pDevice.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedCompatibilityDevice::Release()
{
  return m_pDevice.Release();
}

WriteSerialiser &WrappedCompatibilityDevice::GetThreadSerialiser()
{
  return m_pDevice.GetThreadSerialiser();
}

HRESULT STDMETHODCALLTYPE WrappedCompatibilityDevice::CreateSharedResource(
    _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ const D3D11_RESOURCE_FLAGS *pFlags11,
    D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
    _In_opt_ ID3D12LifetimeTracker *pLifetimeTracker,
    _In_opt_ ID3D12SwapChainAssistant *pOwningSwapchain, REFIID riid,
    _COM_Outptr_opt_ void **ppResource)
{
  if(ppResource == NULL)
    return E_INVALIDARG;

  HRESULT hr;

  // not exactly sure what to do with these...
  RDCASSERT(pLifetimeTracker == NULL);
  RDCASSERT(pOwningSwapchain == NULL);

  SERIALISE_TIME_CALL(
      hr = m_pReal->CreateSharedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                                         pOptimizedClearValue, pFlags11, CompatibilityFlags,
                                         pLifetimeTracker, pOwningSwapchain, riid, ppResource));

  if(FAILED(hr))
  {
    IUnknown *unk = (IUnknown *)*ppResource;
    SAFE_RELEASE(unk);
    return hr;
  }

  return m_pDevice.OpenSharedHandleInternal(D3D12Chunk::CompatDevice_CreateSharedResource,
                                            HeapFlags, riid, ppResource);
}

HRESULT STDMETHODCALLTYPE WrappedCompatibilityDevice::CreateSharedHeap(
    _In_ const D3D12_HEAP_DESC *pHeapDesc, D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
    REFIID riid, _COM_Outptr_opt_ void **ppHeap)
{
  if(ppHeap == NULL)
    return E_INVALIDARG;

  HRESULT hr;

  SERIALISE_TIME_CALL(hr = m_pReal->CreateSharedHeap(pHeapDesc, CompatibilityFlags, riid, ppHeap));

  if(FAILED(hr))
  {
    IUnknown *unk = (IUnknown *)*ppHeap;
    SAFE_RELEASE(unk);
    return hr;
  }

  return m_pDevice.OpenSharedHandleInternal(D3D12Chunk::CompatDevice_CreateSharedHeap,
                                            pHeapDesc->Flags, riid, ppHeap);
}

HRESULT STDMETHODCALLTYPE WrappedCompatibilityDevice::ReflectSharedProperties(
    _In_ ID3D12Object *pHeapOrResource, D3D12_REFLECT_SHARED_PROPERTY ReflectType,
    _Out_writes_bytes_(DataSize) void *pData, UINT DataSize)
{
  return m_pReal->ReflectSharedProperties(Unwrap(pHeapOrResource), ReflectType, pData, DataSize);
}

HRESULT STDMETHODCALLTYPE WrappedNVAPI12::QueryInterface(REFIID riid, void **ppvObject)
{
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedNVAPI12::AddRef()
{
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedNVAPI12::Release()
{
  return 1;
}

BOOL STDMETHODCALLTYPE WrappedNVAPI12::SetReal(IUnknown *)
{
  // shouldn't be called on capture, do nothing
  return FALSE;
}

IUnknown *STDMETHODCALLTYPE WrappedNVAPI12::GetReal()
{
  return m_pDevice.GetReal();
}

BOOL STDMETHODCALLTYPE WrappedNVAPI12::SetShaderExtUAV(DWORD space, DWORD reg, BOOL global)
{
  m_pDevice.SetShaderExtUAV(GPUVendor::nVidia, reg, space, global ? true : false);
  return TRUE;
}

void WrappedNVAPI12::UnwrapDesc(D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc)
{
  pDesc->pRootSignature = Unwrap(pDesc->pRootSignature);
}

void WrappedNVAPI12::UnwrapDesc(D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc)
{
  pDesc->pRootSignature = Unwrap(pDesc->pRootSignature);
}

ID3D12PipelineState *WrappedNVAPI12::ProcessCreatedGraphicsPipelineState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, uint32_t reg, uint32_t space,
    ID3D12PipelineState *realPSO)
{
  ID3D12PipelineState *ret = NULL;
  m_pDevice.SetShaderExt(GPUVendor::nVidia);
  m_pDevice.ProcessCreatedGraphicsPSO(realPSO, reg, space, pDesc, __uuidof(ID3D12PipelineState),
                                      (void **)&ret);
  return ret;
}

ID3D12PipelineState *WrappedNVAPI12::ProcessCreatedComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, uint32_t reg, uint32_t space,
    ID3D12PipelineState *realPSO)
{
  ID3D12PipelineState *ret = NULL;
  m_pDevice.SetShaderExt(GPUVendor::nVidia);
  m_pDevice.ProcessCreatedComputePSO(realPSO, reg, space, pDesc, __uuidof(ID3D12PipelineState),
                                     (void **)&ret);
  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedAGS12::QueryInterface(REFIID riid, void **ppvObject)
{
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedAGS12::AddRef()
{
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedAGS12::Release()
{
  return 1;
}

IUnknown *STDMETHODCALLTYPE WrappedAGS12::GetReal()
{
  return m_pDevice.GetReal();
}

BOOL STDMETHODCALLTYPE WrappedAGS12::SetShaderExtUAV(DWORD space, DWORD reg)
{
  m_pDevice.SetShaderExtUAV(GPUVendor::AMD, reg, space, true);
  return TRUE;
}

HRESULT STDMETHODCALLTYPE WrappedAGS12::CreateD3D11(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
                                                    CONST D3D_FEATURE_LEVEL *, UINT FeatureLevels,
                                                    UINT, CONST DXGI_SWAP_CHAIN_DESC *,
                                                    IDXGISwapChain **, ID3D11Device **,
                                                    D3D_FEATURE_LEVEL *, ID3D11DeviceContext **)
{
  // shouldn't be called on capture
  return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WrappedAGS12::CreateD3D12(IUnknown *pAdapter,
                                                    D3D_FEATURE_LEVEL MinimumFeatureLevel,
                                                    REFIID riid, void **ppDevice)
{
  // shouldn't be called on capture
  return E_NOTIMPL;
}

BOOL STDMETHODCALLTYPE WrappedAGS12::ExtensionsSupported()
{
  return FALSE;
}

WrappedID3D12Device::WrappedID3D12Device(ID3D12Device *realDevice, D3D12InitParams params,
                                         bool enabledDebugLayer)
    : m_RefCounter(realDevice, false),
      m_DevConfig(realDevice, this),
      m_SoftRefCounter(NULL, false),
      m_pDevice(realDevice),
      m_debugLayerEnabled(enabledDebugLayer),
      m_WrappedDownlevel(*this),
      m_DRED(*this),
      m_DREDSettings(*this),
      m_SharingContract(*this),
      m_CompatDevice(*this),
      m_WrappedNVAPI(*this),
      m_WrappedAGS(*this)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(WrappedID3D12Device));

  m_SectionVersion = D3D12InitParams::CurrentVersion;

  m_Replay = new D3D12Replay(this);

  m_StructuredFile = m_StoredStructuredData = new SDFile;

  RDCEraseEl(m_D3D12Opts);
  RDCEraseEl(m_D3D12Opts1);
  RDCEraseEl(m_D3D12Opts2);
  RDCEraseEl(m_D3D12Opts3);
  RDCEraseEl(m_D3D12Opts5);
  RDCEraseEl(m_D3D12Opts6);
  RDCEraseEl(m_D3D12Opts7);
  RDCEraseEl(m_D3D12Opts9);
  RDCEraseEl(m_D3D12Opts12);
  RDCEraseEl(m_D3D12Opts14);
  RDCEraseEl(m_D3D12Opts15);
  RDCEraseEl(m_D3D12Opts16);

  m_pDevice1 = NULL;
  m_pDevice2 = NULL;
  m_pDevice3 = NULL;
  m_pDevice4 = NULL;
  m_pDevice5 = NULL;
  m_pDevice6 = NULL;
  m_pDevice7 = NULL;
  m_pDevice8 = NULL;
  m_pDevice9 = NULL;
  m_pDevice10 = NULL;
  m_pDevice11 = NULL;
  m_pDevice12 = NULL;
  m_pDownlevel = NULL;
  if(m_pDevice)
  {
    m_pDevice->QueryInterface(__uuidof(ID3D12Device1), (void **)&m_pDevice1);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device2), (void **)&m_pDevice2);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device3), (void **)&m_pDevice3);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device4), (void **)&m_pDevice4);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device5), (void **)&m_pDevice5);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device6), (void **)&m_pDevice6);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device7), (void **)&m_pDevice7);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device8), (void **)&m_pDevice8);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device9), (void **)&m_pDevice9);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device10), (void **)&m_pDevice10);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device11), (void **)&m_pDevice11);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device12), (void **)&m_pDevice12);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedData), (void **)&m_DRED.m_pReal);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedData1), (void **)&m_DRED.m_pReal1);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings),
                              (void **)&m_DREDSettings.m_pReal);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings1),
                              (void **)&m_DREDSettings.m_pReal1);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings2),
                              (void **)&m_DREDSettings.m_pReal2);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceDownlevel), (void **)&m_pDownlevel);
    m_pDevice->QueryInterface(__uuidof(ID3D12CompatibilityDevice), (void **)&m_CompatDevice.m_pReal);
    m_pDevice->QueryInterface(__uuidof(ID3D12SharingContract), (void **)&m_SharingContract.m_pReal);

    for(size_t i = 0; i < ARRAY_COUNT(m_DescriptorIncrements); i++)
      m_DescriptorIncrements[i] =
          m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

    HRESULT hr = S_OK;

    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &m_D3D12Opts,
                                        sizeof(m_D3D12Opts));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &m_D3D12Opts1,
                                        sizeof(m_D3D12Opts1));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts1);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &m_D3D12Opts2,
                                        sizeof(m_D3D12Opts2));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts2);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &m_D3D12Opts3,
                                        sizeof(m_D3D12Opts3));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts3);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &m_D3D12Opts5,
                                        sizeof(m_D3D12Opts5));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts5);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &m_D3D12Opts6,
                                        sizeof(m_D3D12Opts6));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts6);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &m_D3D12Opts7,
                                        sizeof(m_D3D12Opts7));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts7);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &m_D3D12Opts9,
                                        sizeof(m_D3D12Opts9));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts9);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &m_D3D12Opts12,
                                        sizeof(m_D3D12Opts12));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts12);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS14, &m_D3D12Opts14,
                                        sizeof(m_D3D12Opts14));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts14);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS14, &m_D3D12Opts15,
                                        sizeof(m_D3D12Opts15));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts15);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS14, &m_D3D12Opts16,
                                        sizeof(m_D3D12Opts16));
    if(hr != S_OK)
      RDCEraseEl(m_D3D12Opts16);
  }

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

  m_DummyInfoQueue.m_pDevice = this;
  m_DummyDebug.m_pDevice = this;
  m_WrappedDebug.m_pDevice = this;

  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();
  tempMemoryTLSSlot = Threading::AllocateTLSSlot();

  m_HeaderChunk = NULL;

  m_Alloc = m_DataUploadAlloc = NULL;
  m_GPUSyncFence = NULL;
  m_GPUSyncHandle = NULL;
  m_GPUSyncCounter = 0;

  initStateCurBatch = 0;
  initStateCurList = NULL;

  m_InitParams = params;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::LoadingReplaying;

    if(realDevice)
    {
      m_ResourceList = new std::map<ResourceId, WrappedID3D12Resource *>();
      m_PipelineList = new rdcarray<WrappedID3D12PipelineState *>();
    }

    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();

    HMODULE D3D12Core = GetModuleHandleA("D3D12Core.dll");
    if(D3D12Core != NULL)
    {
      DWORD *ver_ptr = (DWORD *)GetProcAddress(D3D12Core, "D3D12SDKVersion");

      if(ver_ptr && *ver_ptr >= 700 && *ver_ptr <= 706)
      {
        RDCLOG("Disabling new barrier support on older beta SDK");
        m_D3D12Opts12.EnhancedBarriersSupported = FALSE;
      }
    }
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;

    if(m_pDevice)
    {
      typedef HRESULT(WINAPI * PFN_CREATE_DXGI_FACTORY)(REFIID, void **);

      PFN_CREATE_DXGI_FACTORY createFunc = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(
          GetModuleHandleA("dxgi.dll"), "CreateDXGIFactory1");

      IDXGIFactory1 *tmpFactory = NULL;
      HRESULT hr = createFunc(__uuidof(IDXGIFactory1), (void **)&tmpFactory);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create DXGI factory! HRESULT: %s", ToStr(hr).c_str());
      }

      if(tmpFactory)
      {
        IDXGIAdapter *pDXGIAdapter = NULL;
        hr = EnumAdapterByLuid(tmpFactory, m_pDevice->GetAdapterLuid(), &pDXGIAdapter);

        if(FAILED(hr))
        {
          RDCERR("Couldn't get DXGI adapter by LUID from D3D12 device");
        }
        else
        {
          DXGI_ADAPTER_DESC desc = {};
          pDXGIAdapter->GetDesc(&desc);

          m_InitParams.AdapterDesc = desc;

          GPUVendor vendor = GPUVendorFromPCIVendor(desc.VendorId);
          rdcstr descString = GetDriverVersion(desc);

          RDCLOG("New D3D12 device created: %s / %s", ToStr(vendor).c_str(), descString.c_str());

          SAFE_RELEASE(pDXGIAdapter);
        }
      }

      SAFE_RELEASE(tmpFactory);
    }
  }

  m_ResourceManager = new D3D12ResourceManager(m_State, this);

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_DeviceRecord = NULL;

  m_Queue = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_DeviceRecord->type = Resource_Device;
    m_DeviceRecord->DataInSerialiser = false;
    m_DeviceRecord->InternalResource = true;
    m_DeviceRecord->Length = 0;

    m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_FrameCaptureRecord->DataInSerialiser = false;
    m_FrameCaptureRecord->InternalResource = true;
    m_FrameCaptureRecord->Length = 0;

    RenderDoc::Inst().AddDeviceFrameCapturer((ID3D12Device *)this, this);
  }

  m_pInfoQueue = NULL;
  m_WrappedDebug.m_pDebug = NULL;
  m_WrappedDebug.m_pDebug1 = NULL;
  m_WrappedDebug.m_pDebug2 = NULL;
  if(m_pDevice)
  {
    m_pDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void **)&m_pInfoQueue);
    m_pDevice->QueryInterface(__uuidof(ID3D12DebugDevice), (void **)&m_WrappedDebug.m_pDebug);
    m_pDevice->QueryInterface(__uuidof(ID3D12DebugDevice1), (void **)&m_WrappedDebug.m_pDebug1);
    m_pDevice->QueryInterface(__uuidof(ID3D12DebugDevice2), (void **)&m_WrappedDebug.m_pDebug2);
  }

  if(m_pInfoQueue)
  {
    if(RenderDoc::Inst().GetCaptureOptions().debugOutputMute)
      m_pInfoQueue->SetMuteDebugOutput(true);

    UINT size = m_pInfoQueue->GetStorageFilterStackSize();

    while(size > 1)
    {
      m_pInfoQueue->ClearStorageFilter();
      size = m_pInfoQueue->GetStorageFilterStackSize();
    }

    size = m_pInfoQueue->GetRetrievalFilterStackSize();

    while(size > 1)
    {
      m_pInfoQueue->ClearRetrievalFilter();
      size = m_pInfoQueue->GetRetrievalFilterStackSize();
    }

    m_pInfoQueue->ClearStoredMessages();

    if(RenderDoc::Inst().IsReplayApp())
    {
      m_pInfoQueue->SetMuteDebugOutput(false);

      D3D12_MESSAGE_ID mute[] = {
          // the runtime cries foul when you use normal APIs in expected ways (for simple markers)
          D3D12_MESSAGE_ID_CORRUPTED_PARAMETER2,

          // super spammy, mostly just perf warning, and impossible to fix for our cases
          D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
          D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,

          // caused often by an over-declaration in the root signature to match between
          // different shaders, and in some descriptors are entirely skipped. We rely on
          // the user to get this right - if the error is non-fatal, any real problems
          // will be potentially highlighted in the pipeline view
          D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
          D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,

          // message about a NULL range to map for reading/writing the whole resource
          // which is "inefficient" but for our use cases it's almost always what we mean.
          D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,

          // D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH
          // message about mismatched SRV dimensions, which it seems to get wrong with the
          // dummy NULL descriptors on the texture sampling code
          D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH,
      };

      D3D12_INFO_QUEUE_FILTER filter = {};
      filter.DenyList.NumIDs = ARRAY_COUNT(mute);
      filter.DenyList.pIDList = mute;

      m_pInfoQueue->AddStorageFilterEntries(&filter);
    }
  }
  else
  {
    RDCDEBUG("Couldn't get ID3D12InfoQueue.");
  }

  {
    SCOPED_LOCK(m_DeviceWrappersLock);
    m_DeviceWrappers[m_pDevice] = this;
  }
}

WrappedID3D12Device::~WrappedID3D12Device()
{
  {
    SCOPED_LOCK(m_DeviceWrappersLock);
    m_DeviceWrappers.erase(m_pDevice);
  }

  SAFE_DELETE(m_StoredStructuredData);

  RenderDoc::Inst().RemoveDeviceFrameCapturer((ID3D12Device *)this);

  if(!m_InternalCmds.pendingcmds.empty())
    ExecuteLists(m_Queue);

  if(!m_InternalCmds.submittedcmds.empty())
    FlushLists(true);

  for(size_t i = 0; i < m_InternalCmds.freecmds.size(); i++)
    SAFE_RELEASE(m_InternalCmds.freecmds[i]);

  for(size_t i = 0; i < m_QueueFences.size(); i++)
  {
    GPUSync(m_Queues[i], m_QueueFences[i]);

    SAFE_RELEASE(m_QueueFences[i]);
  }

  for(auto it = m_UploadBuffers.begin(); it != m_UploadBuffers.end(); ++it)
  {
    SAFE_RELEASE(it->second);
  }

  SAFE_RELEASE(m_blasAddressBufferResource);
  SAFE_RELEASE(m_blasAddressBufferUploadResource);

  m_Replay->DestroyResources();

  DestroyInternalResources();

  SAFE_RELEASE(m_Queue);

  if(m_DeviceRecord)
  {
    RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
    m_DeviceRecord->Delete(GetResourceManager());
  }

  if(m_FrameCaptureRecord)
  {
    RDCASSERT(m_FrameCaptureRecord->GetRefCount() == 1);
    m_FrameCaptureRecord->Delete(GetResourceManager());
  }

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  SAFE_RELEASE(m_DRED.m_pReal);
  SAFE_RELEASE(m_DRED.m_pReal1);
  SAFE_RELEASE(m_DREDSettings.m_pReal);
  SAFE_RELEASE(m_DREDSettings.m_pReal1);
  SAFE_RELEASE(m_DREDSettings.m_pReal2);
  SAFE_RELEASE(m_CompatDevice.m_pReal);
  SAFE_RELEASE(m_SharingContract.m_pReal);
  SAFE_RELEASE(m_pDownlevel);
  SAFE_RELEASE(m_pDevice12);
  SAFE_RELEASE(m_pDevice11);
  SAFE_RELEASE(m_pDevice10);
  SAFE_RELEASE(m_pDevice9);
  SAFE_RELEASE(m_pDevice8);
  SAFE_RELEASE(m_pDevice7);
  SAFE_RELEASE(m_pDevice6);
  SAFE_RELEASE(m_pDevice5);
  SAFE_RELEASE(m_pDevice4);
  SAFE_RELEASE(m_pDevice3);
  SAFE_RELEASE(m_pDevice2);
  SAFE_RELEASE(m_pDevice1);

  SAFE_RELEASE(m_pInfoQueue);
  SAFE_RELEASE(m_WrappedDebug.m_pDebug);
  SAFE_RELEASE(m_WrappedDebug.m_pDebug1);
  SAFE_RELEASE(m_WrappedDebug.m_pDebug2);
  SAFE_RELEASE(m_pDevice);

  for(size_t i = 0; i < m_ThreadSerialisers.size(); i++)
    delete m_ThreadSerialisers[i];

  for(size_t i = 0; i < m_ThreadTempMem.size(); i++)
  {
    delete[] m_ThreadTempMem[i]->memory;
    delete m_ThreadTempMem[i];
  }

  SAFE_DELETE(m_ResourceList);
  SAFE_DELETE(m_PipelineList);

  delete m_Replay;

  SAFE_RELEASE(m_ReplayNVAPI);
  SAFE_RELEASE(m_ReplayAGS);

  RenderDoc::Inst().UnregisterMemoryRegion(this);
}

WrappedID3D12Device *WrappedID3D12Device::Create(ID3D12Device *realDevice, D3D12InitParams params,
                                                 bool enabledDebugLayer)
{
  {
    SCOPED_LOCK(m_DeviceWrappersLock);

    auto it = m_DeviceWrappers.find(realDevice);
    if(it != m_DeviceWrappers.end())
    {
      it->second->AddRef();
      return it->second;
    }
  }

  return new WrappedID3D12Device(realDevice, params, enabledDebugLayer);
}

HRESULT WrappedID3D12Device::QueryInterface(REFIID riid, void **ppvObject)
{
  // RenderDoc UUID {A7AA6116-9C8D-4BBA-9083-B4D816B71B78}
  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

  static const GUID ID3D12CompatibilityDevice_uuid = {
      0x8f1c0e3c, 0xfae3, 0x4a82, {0xb0, 0x98, 0xbf, 0xe1, 0x70, 0x82, 0x07, 0xff}};

  // unknown/undocumented internal interface
  // {de18ef3a-0x2089-0x4936-0xa3 0xf3- 0xec 0x78 0x7a 0xc6 0xa4 0x0d}
  static const GUID ID3D12DeviceDriverDetails_RS5_uuid = {
      0xde18ef3a, 0x2089, 0x4936, {0xa3, 0xf3, 0xec, 0x78, 0x7a, 0xc6, 0xa4, 0x0d}};

  HRESULT hr = S_OK;

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12Device *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIDevice))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice *real = (IDXGIDevice *)(*ppvObject);
      *ppvObject = (IDXGIDevice *)(new WrappedIDXGIDevice4(real, this));
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice1))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice1 *real = (IDXGIDevice1 *)(*ppvObject);
      *ppvObject = (IDXGIDevice1 *)(new WrappedIDXGIDevice4(real, this));
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice2))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice2 *real = (IDXGIDevice2 *)(*ppvObject);
      *ppvObject = (IDXGIDevice2 *)(new WrappedIDXGIDevice4(real, this));
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice3))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice3 *real = (IDXGIDevice3 *)(*ppvObject);
      *ppvObject = (IDXGIDevice3 *)(new WrappedIDXGIDevice4(real, this));
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice4))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice4 *real = (IDXGIDevice4 *)(*ppvObject);
      *ppvObject = (IDXGIDevice4 *)(new WrappedIDXGIDevice4(real, this));
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(ID3D12Device))
  {
    AddRef();
    *ppvObject = (ID3D12Device *)this;
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Device1))
  {
    if(m_pDevice1)
    {
      AddRef();
      *ppvObject = (ID3D12Device1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device2))
  {
    if(m_pDevice2)
    {
      AddRef();
      *ppvObject = (ID3D12Device2 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device3))
  {
    if(m_pDevice3)
    {
      AddRef();
      *ppvObject = (ID3D12Device3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device4))
  {
    if(m_pDevice4)
    {
      AddRef();
      *ppvObject = (ID3D12Device4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device5))
  {
    if(m_pDevice5)
    {
      AddRef();
      *ppvObject = (ID3D12Device5 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device6))
  {
    if(m_pDevice6)
    {
      AddRef();
      *ppvObject = (ID3D12Device6 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device7))
  {
    if(m_pDevice7)
    {
      AddRef();
      *ppvObject = (ID3D12Device7 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device8))
  {
    if(m_pDevice8)
    {
      AddRef();
      *ppvObject = (ID3D12Device8 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device9))
  {
    if(m_pDevice9)
    {
      AddRef();
      *ppvObject = (ID3D12Device9 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device10))
  {
    if(m_pDevice10)
    {
      AddRef();
      *ppvObject = (ID3D12Device10 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device11))
  {
    if(m_pDevice11)
    {
      AddRef();
      *ppvObject = (ID3D12Device11 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Device12))
  {
    if(m_pDevice12)
    {
      AddRef();
      *ppvObject = (ID3D12Device12 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DeviceConfiguration))
  {
    if(m_DevConfig.IsValid())
    {
      *ppvObject = (ID3D12DeviceConfiguration *)&m_DevConfig;
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }
  else if(riid == __uuidof(ID3D12DeviceDownlevel))
  {
    if(m_pDownlevel)
    {
      *ppvObject = &m_WrappedDownlevel;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12InfoQueue))
  {
    RDCWARN(
        "Returning a dummy ID3D12InfoQueue that does nothing. This ID3D12InfoQueue will not work!");
    *ppvObject = (ID3D12InfoQueue *)&m_DummyInfoQueue;
    m_DummyInfoQueue.AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12InfoQueue1))
  {
    RDCWARN(
        "Returning a dummy ID3D12InfoQueue1 that does nothing. This ID3D12InfoQueue1 will not "
        "work!");
    *ppvObject = (ID3D12InfoQueue1 *)&m_DummyInfoQueue;
    m_DummyInfoQueue.AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(INVAPID3DDevice))
  {
    // don't addref, this is an internal interface so we just don't addref at all
    *ppvObject = (INVAPID3DDevice *)&m_WrappedNVAPI;
    return S_OK;
  }
  else if(riid == __uuidof(IAGSD3DDevice))
  {
    // don't addref, this is an internal interface so we just don't addref at all
    *ppvObject = (IAGSD3DDevice *)&m_WrappedAGS;
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DebugDevice))
  {
    // we queryinterface for this at startup, so if it's present we can
    // return our wrapper
    if(m_WrappedDebug.m_pDebug)
    {
      AddRef();
      *ppvObject = (ID3D12DebugDevice *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      RDCWARN(
          "Returning a dummy ID3D12DebugDevice that does nothing. This ID3D12DebugDevice will not "
          "work!");
      *ppvObject = (ID3D12DebugDevice *)&m_DummyDebug;
      m_DummyDebug.AddRef();
      return S_OK;
    }
  }
  else if(riid == __uuidof(ID3D12DebugDevice1))
  {
    // we queryinterface for this at startup, so if it's present we can
    // return our wrapper
    if(m_WrappedDebug.m_pDebug1)
    {
      AddRef();
      *ppvObject = (ID3D12DebugDevice1 *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      RDCWARN(
          "Returning a dummy ID3D12DebugDevice1 that does nothing. This ID3D12DebugDevice1 will "
          "not "
          "work!");
      *ppvObject = (ID3D12DebugDevice1 *)&m_DummyDebug;
      m_DummyDebug.AddRef();
      return S_OK;
    }
  }
  else if(riid == __uuidof(ID3D12DebugDevice2))
  {
    // we queryinterface for this at startup, so if it's present we can
    // return our wrapper
    if(m_WrappedDebug.m_pDebug1)
    {
      AddRef();
      *ppvObject = (ID3D12DebugDevice2 *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      RDCWARN(
          "Returning a dummy ID3D12DebugDevice2 that does nothing. This ID3D12DebugDevice2 will "
          "not "
          "work!");
      *ppvObject = (ID3D12DebugDevice2 *)&m_DummyDebug;
      m_DummyDebug.AddRef();
      return S_OK;
    }
  }
  else if(riid == __uuidof(ID3D12DeviceRemovedExtendedData))
  {
    if(m_DRED.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12DeviceRemovedExtendedData *)&m_DRED;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DeviceRemovedExtendedData1))
  {
    if(m_DRED.m_pReal1)
    {
      AddRef();
      *ppvObject = (ID3D12DeviceRemovedExtendedData1 *)&m_DRED;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DeviceRemovedExtendedDataSettings))
  {
    if(m_DREDSettings.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12DeviceRemovedExtendedDataSettings *)&m_DREDSettings;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DeviceRemovedExtendedDataSettings1))
  {
    if(m_DREDSettings.m_pReal1)
    {
      AddRef();
      *ppvObject = (ID3D12DeviceRemovedExtendedDataSettings1 *)&m_DREDSettings;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DeviceRemovedExtendedDataSettings2))
  {
    if(m_DREDSettings.m_pReal2)
    {
      AddRef();
      *ppvObject = (ID3D12DeviceRemovedExtendedDataSettings2 *)&m_DREDSettings;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == IRenderDoc_uuid)
  {
    AddRef();
    *ppvObject = (IUnknown *)this;
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12CompatibilityDevice))
  {
    if(m_CompatDevice.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12CompatibilityDevice *)&m_CompatDevice;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12SharingContract))
  {
    if(m_SharingContract.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12SharingContract *)&m_SharingContract;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == ID3D12DeviceDriverDetails_RS5_uuid)
  {
    static bool printed = false;
    if(!printed)
    {
      RDCWARN(
          "Querying ID3D12Device for unsupported/undocumented interface: "
          "ID3D12DeviceDriverDetails_RS5");
      printed = true;
    }
    // return the real thing unwrapped and hope this is OK
    return m_pDevice->QueryInterface(riid, ppvObject);
  }

  return m_RefCounter.QueryInterface("ID3D12Device", riid, ppvObject);
}

HRESULT WrappedID3D12Device::CreateInitialStateBuffer(const D3D12_RESOURCE_DESC &desc,
                                                      ID3D12Resource **buf)
{
  const UINT64 InitialStateHeapSize = 128 * 1024 * 1024;

  HRESULT ret = S_OK;

  // if desc.Width is greater than InitialStateHeapSize this will forcibly be true, regardless of
  // m_LastInitialStateHeapOffset
  // it will create a dedicated heap for that resource, and then when m_LastInitialStateHeapOffset
  // is updated the next resource (regardless of size) will go in a new heap because
  // m_LastInitialStateHeapOffset >= InitialStateHeapSize will be true
  if(m_InitialStateHeaps.empty() || m_LastInitialStateHeapOffset >= InitialStateHeapSize ||
     desc.Width > InitialStateHeapSize - m_LastInitialStateHeapOffset)
  {
    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    heapDesc.SizeInBytes = RDCMAX(InitialStateHeapSize, desc.Width);
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

    heapDesc.Properties.Type = D3D12_HEAP_TYPE_READBACK;
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Properties.CreationNodeMask = 1;
    heapDesc.Properties.VisibleNodeMask = 1;

    ID3D12Heap *heap = NULL;

    ret = GetReal()->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), (void **)&heap);

    if(FAILED(ret))
    {
      CheckHRESULT(ret);
      RDCERR("Couldn't create new initial state heap #%zu: %s", m_InitialStateHeaps.size(),
             ToStr(ret).c_str());
      SAFE_RELEASE(heap);
      return ret;
    }

    m_InitialStateHeaps.push_back(heap);
    m_LastInitialStateHeapOffset = 0;
  }

  ret = GetReal()->CreatePlacedResource(m_InitialStateHeaps.back(), m_LastInitialStateHeapOffset,
                                        &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                        __uuidof(ID3D12Resource), (void **)buf);

  // all D3D12 buffers are 64kb aligned
  m_LastInitialStateHeapOffset = AlignUp(m_LastInitialStateHeapOffset + desc.Width, 64 * 1024LLU);

  return ret;
}

ID3D12Resource *WrappedID3D12Device::GetUploadBuffer(uint64_t chunkOffset, uint64_t byteSize)
{
  ID3D12Resource *buf = m_UploadBuffers[chunkOffset];

  if(buf != NULL)
    return buf;

  D3D12_RESOURCE_DESC soBufDesc;
  soBufDesc.Alignment = 0;
  soBufDesc.DepthOrArraySize = 1;
  soBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  soBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  soBufDesc.Format = DXGI_FORMAT_UNKNOWN;
  soBufDesc.Height = 1;
  soBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  soBufDesc.MipLevels = 1;
  soBufDesc.SampleDesc.Count = 1;
  soBufDesc.SampleDesc.Quality = 0;
  soBufDesc.Width = byteSize;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &soBufDesc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                       __uuidof(ID3D12Resource), (void **)&buf);

  m_UploadBuffers[chunkOffset] = buf;

  RDCASSERT(hr == S_OK, hr, S_OK, byteSize);
  return buf;
}

void WrappedID3D12Device::ApplyInitialContents()
{
  RENDERDOC_PROFILEFUNCTION();

  initStateCurBatch = 0;
  initStateCurList = NULL;

  GetResourceManager()->ApplyInitialContents();

  // Upload all buffer addresses as all of the referenced buffer resources would have been
  // created and addresses had been tracked
  UploadBLASBufferAddresses();

  // close the final list
  if(initStateCurList)
  {
    D3D12MarkerRegion::End(initStateCurList);
    initStateCurList->Close();
  }

  initStateCurBatch = 0;
  initStateCurList = NULL;
}

void WrappedID3D12Device::AddCaptureSubmission()
{
  if(IsActiveCapturing(m_State))
  {
    // 15 is quite a lot of submissions.
    const int expectedMaxSubmissions = 15;

    RenderDoc::Inst().SetProgress(CaptureProgress::FrameCapture,
                                  FakeProgress(m_SubmitCounter, expectedMaxSubmissions));
    m_SubmitCounter++;
  }
}

void WrappedID3D12Device::CheckForDeath()
{
  if(!m_Alive)
    return;

  if(m_RefCounter.GetRefCount() == 0)
  {
    RDCASSERT(m_SoftRefCounter.GetRefCount() >= m_InternalRefcount);

    // MEGA HACK
    if(m_SoftRefCounter.GetRefCount() <= m_InternalRefcount || IsReplayMode(m_State))
    {
      m_Alive = false;
      delete this;
    }
  }
}

void WrappedID3D12Device::FirstFrame(IDXGISwapper *swapper)
{
  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture(
        DeviceOwnedWindow((ID3D12Device *)this, swapper ? swapper->GetHWND() : NULL));

    m_FirstFrameCapture = true;
    m_FirstFrameCaptureWindow = swapper ? swapper->GetHWND() : NULL;
    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

void WrappedID3D12Device::ApplyBarriers(BarrierSet &barriers)
{
  SCOPED_LOCK(m_ResourceStatesLock);
  GetResourceManager()->ApplyBarriers(barriers, m_ResourceStates);
}

void WrappedID3D12Device::ReleaseSwapchainResources(IDXGISwapper *swapper, UINT QueueCount,
                                                    IUnknown *const *ppPresentQueue,
                                                    IUnknown **unwrappedQueues)
{
  if(ppPresentQueue)
  {
    for(UINT i = 0; i < QueueCount; i++)
    {
      WrappedID3D12CommandQueue *wrappedQ = (WrappedID3D12CommandQueue *)ppPresentQueue[i];
      RDCASSERT(WrappedID3D12CommandQueue::IsAlloc(wrappedQ));

      unwrappedQueues[i] = wrappedQ->GetReal();
    }
  }

  for(int i = 0; i < swapper->GetNumBackbuffers(); i++)
  {
    ID3D12Resource *res = (ID3D12Resource *)swapper->GetBackbuffers()[i];

    if(!res)
      continue;

    WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)res;
    wrapped->ReleaseInternalRef();
    SAFE_RELEASE(wrapped);
  }

  auto it = m_SwapChains.find(swapper);
  if(it != m_SwapChains.end())
  {
    for(int i = 0; i < swapper->GetNumBackbuffers(); i++)
      FreeRTV(it->second.rtvs[i]);

    m_SwapChains.erase(it);
  }
}

void WrappedID3D12Device::NewSwapchainBuffer(IUnknown *backbuffer)
{
  ID3D12Resource *pRes = (ID3D12Resource *)backbuffer;

  if(pRes)
  {
    WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)pRes;
    wrapped->AddInternalRef();
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_WrapSwapchainBuffer(SerialiserType &ser, IDXGISwapper *swapper,
                                                        DXGI_FORMAT bufferFormat, UINT Buffer,
                                                        IUnknown *realSurface)
{
  WrappedID3D12Resource *pRes = (WrappedID3D12Resource *)realSurface;

  SERIALISE_ELEMENT(Buffer);
  SERIALISE_ELEMENT_LOCAL(SwapbufferID, GetResID(pRes)).TypedAs("ID3D12Resource *"_lit);
  SERIALISE_ELEMENT_LOCAL(BackbufferDescriptor, pRes->GetDesc()).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12Resource *fakeBB = NULL;

    DXGI_FORMAT SwapbufferFormat = BackbufferDescriptor.Format;

    // DXGI swap chain back buffers can be freely cast as a special-case.
    // translate the format to a typeless format to allow for this.
    // the original type is stored separately below
    BackbufferDescriptor.Format = GetTypelessFormat(BackbufferDescriptor.Format);

    HRESULT hr = S_OK;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // create in common, which is the same as present
    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &BackbufferDescriptor,
                                            D3D12_RESOURCE_STATE_COMMON, NULL,
                                            __uuidof(ID3D12Resource), (void **)&fakeBB);

    AddResource(SwapbufferID, ResourceType::SwapchainImage, "Swapchain Image");

    if(FAILED(hr))
    {
      RDCERR("Failed to create fake back buffer, HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(fakeBB, NULL, 0, this);
      fakeBB = wrapped;

      fakeBB->SetName(L"Swap Chain Buffer");

      GetResourceManager()->AddLiveResource(SwapbufferID, fakeBB);

      m_BackbufferFormat[wrapped->GetResourceID()] = SwapbufferFormat;

      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states = {D3D12ResourceLayout::FromStates(D3D12_RESOURCE_STATE_PRESENT)};
    }
  }

  return true;
}

IUnknown *WrappedID3D12Device::WrapSwapchainBuffer(IDXGISwapper *swapper, DXGI_FORMAT bufferFormat,
                                                   UINT buffer, IUnknown *realSurface)
{
  ID3D12Resource *pRes = NULL;

  ID3D12Resource *query = NULL;
  realSurface->QueryInterface(__uuidof(ID3D12Resource), (void **)&query);
  if(query)
    query->Release();

  if(GetResourceManager()->HasWrapper(query))
  {
    pRes = (ID3D12Resource *)GetResourceManager()->GetWrapper(query);
    pRes->AddRef();

    realSurface->Release();
  }
  else if(WrappedID3D12Resource::IsAlloc(query))
  {
    // this could be possible if we're doing downlevel presenting
    pRes = query;
  }
  else
  {
    pRes = new WrappedID3D12Resource((ID3D12Resource *)realSurface, NULL, 0, this);

    ResourceId id = GetResID(pRes);

    // there shouldn't be a resource record for this texture as it wasn't created via
    // Create*Resource
    RDCASSERT(id != ResourceId() && !GetResourceManager()->HasResourceRecord(id));

    if(IsCaptureMode(m_State))
    {
      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      record->type = Resource_Resource;
      record->DataInSerialiser = false;
      record->Length = 0;

      WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)pRes;

      wrapped->SetResourceRecord(record);

      WriteSerialiser &ser = GetThreadSerialiser();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::CreateSwapBuffer);

      Serialise_WrapSwapchainBuffer(ser, swapper, bufferFormat, buffer, pRes);

      record->AddChunk(scope.Get());

      {
        SCOPED_LOCK(m_ResourceStatesLock);
        SubresourceStateVector &states = m_ResourceStates[id];

        states = {D3D12ResourceLayout::FromStates(D3D12_RESOURCE_STATE_PRESENT)};
      }
    }
    else
    {
      WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)pRes;

      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }
  }

  if(IsCaptureMode(m_State))
  {
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = GetSRGBFormat(bufferFormat);
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = AllocRTV();

    if(rtv.ptr != 0)
      CreateRenderTargetView(pRes, NULL, rtv);

    FreeRTV(m_SwapChains[swapper].rtvs[buffer]);
    m_SwapChains[swapper].rtvs[buffer] = rtv;

    ID3DDevice *swapQ = swapper->GetD3DDevice();
    RDCASSERT(WrappedID3D12CommandQueue::IsAlloc(swapQ));
    m_SwapChains[swapper].queue = (WrappedID3D12CommandQueue *)swapQ;
  }

  return pRes;
}

IDXGIResource *WrappedID3D12Device::WrapExternalDXGIResource(IDXGIResource *res)
{
  SCOPED_LOCK(m_WrapDeduplicateLock);

  ID3D12Resource *d3d12res;
  res->QueryInterface(__uuidof(ID3D12Resource), (void **)&d3d12res);
  if(GetResourceManager()->HasWrapper(d3d12res))
  {
    ID3D12DeviceChild *wrapper = GetResourceManager()->GetWrapper(d3d12res);
    IDXGIResource *ret = NULL;
    wrapper->QueryInterface(__uuidof(IDXGIResource), (void **)&ret);
    res->Release();
    return ret;
  }

  void *voidRes = (void *)res;
  OpenSharedHandleInternal(D3D12Chunk::Device_ExternalDXGIResource, D3D12_HEAP_FLAG_NONE,
                           __uuidof(IDXGIResource), &voidRes);
  return (IDXGIResource *)voidRes;
}

void WrappedID3D12Device::Map(ID3D12Resource *Resource, UINT Subresource)
{
  MapState map;
  map.res = Resource;
  map.subres = Subresource;

  D3D12_RESOURCE_DESC desc = Resource->GetDesc();

  D3D12_HEAP_PROPERTIES heapProps;
  Resource->GetHeapProperties(&heapProps, NULL);

  // ignore maps of readback resources, these cannot ever reach the GPU because the resource is
  // stuck in COPY_DEST state.
  if(heapProps.Type == D3D12_HEAP_TYPE_READBACK)
    return;

  m_pDevice->GetCopyableFootprints(&desc, Subresource, 1, 0, NULL, NULL, NULL, &map.totalSize);

  {
    SCOPED_LOCK(m_MapsLock);
    m_Maps.push_back(map);
  }
}

void WrappedID3D12Device::Unmap(ID3D12Resource *Resource, UINT Subresource, byte *mapPtr,
                                const D3D12_RANGE *pWrittenRange)
{
  MapState map = {};

  D3D12_HEAP_PROPERTIES heapProps;
  Resource->GetHeapProperties(&heapProps, NULL);

  // ignore maps of readback resources, these cannot ever reach the GPU because the resource is
  // stuck in COPY_DEST state.
  if(heapProps.Type == D3D12_HEAP_TYPE_READBACK)
    return;

  map.res = Resource;
  map.subres = Subresource;

  {
    SCOPED_LOCK(m_MapsLock);
    int32_t idx = m_Maps.indexOf(map);

    if(idx < 0)
      return;

    map = m_Maps.takeAt(idx);
  }

  bool capframe = false;
  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  D3D12_RANGE range = {0, (SIZE_T)map.totalSize};

  if(pWrittenRange)
  {
    range = *pWrittenRange;

    // clamp end if it's lower than begin
    if(range.End < range.Begin)
      range.End = range.Begin;
  }

  if(capframe)
    MapDataWrite(Resource, Subresource, mapPtr, range, false);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_MapDataWrite(SerialiserType &ser, ID3D12Resource *Resource,
                                                 UINT Subresource, byte *MappedData,
                                                 D3D12_RANGE range, bool coherentFlush)
{
  SERIALISE_ELEMENT(Resource).Important();
  SERIALISE_ELEMENT(Subresource);

  // tracks if we're uploading the data to a persistent buffer and don't do the upload from CPU each
  // time. If this is true, during load we init the buffer and upload, then on replay each time we
  // just do a GPU-side copy. If this is false, during init we save the range (since currently it's
  // serialised after the data), then during replay we map the resource and serialise directly into
  // it.
  bool gpuUpload = false;

  ResourceId origid;
  if(IsReplayingAndReading() && Resource)
  {
    origid = GetResourceManager()->GetOriginalID(GetResID(Resource));
    if(m_UploadResourceIds.find(origid) != m_UploadResourceIds.end())
      gpuUpload = true;
  }

  MappedData += range.Begin;

  const D3D12_RANGE nopRange = {0, 0};

  // we serialise MappedData manually, so that when we don't need it we just skip instead of
  // actually allocating and memcpy'ing the buffer.
  SerialiserFlags flags = SerialiserFlags::AllocateMemory;
  if(IsActiveReplaying(m_State))
  {
    if(gpuUpload)
    {
      // set the array to explicitly NULL (so it doesn't crash on deserialise) and don't allocate.
      // This will cause it to be skipped
      MappedData = NULL;
      flags = SerialiserFlags::NoFlags;
    }
    else
    {
      HRESULT hr = Resource->Map(Subresource, &nopRange, (void **)&MappedData);
      CheckHRESULT(hr);
      if(FAILED(hr))
      {
        RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
      }

      // write into the appropriate offset, don't allocate
      if(MappedData != NULL)
        MappedData += m_UploadRanges[m_Queue->GetCommandData()->m_CurChunkOffset].Begin;
      flags = SerialiserFlags::NoFlags;
    }
  }

  ser.Serialise("MappedData"_lit, MappedData, range.End - range.Begin, flags).Important();

  size_t dataOffset = 0;
  if(ser.IsWriting())
    dataOffset = size_t(ser.GetWriter()->GetOffset() - (range.End - range.Begin));

  SERIALISE_ELEMENT(range);

  uint64_t rangeSize = range.End - range.Begin;

  if(!gpuUpload && ser.IsReading())
  {
    if(IsLoading(m_State))
    {
      byte *writePtr = NULL;
      HRESULT hr = Resource->Map(Subresource, &nopRange, (void **)&writePtr);
      CheckHRESULT(hr);
      if(FAILED(hr))
      {
        RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
      }

      if(writePtr)
      {
        writePtr += range.Begin;
        memcpy(writePtr, MappedData, (size_t)rangeSize);
        Resource->Unmap(Subresource, &range);
      }
    }
    else if(IsActiveReplaying(m_State))
    {
      // if we mapped the resource for a cpu upload, we can unmap now
      // set the pointer to NULL so that it doesn't get freed later
      MappedData = NULL;
      Resource->Unmap(Subresource, &range);
    }
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(ser.IsWriting() && coherentFlush)
  {
    byte *ref = GetWrapped(Resource)->GetShadow(Subresource);
    const byte *serialisedData = ser.GetWriter()->GetData() + dataOffset;

    if(ref)
    {
      memcpy(ref + range.Begin, serialisedData, size_t(range.End - range.Begin));
    }
    else
    {
      RDCERR("Shadow data not allocated when coherent map flush is processed!");
    }
  }

  // don't do anything if end <= begin because the range is empty.
  if(IsReplayingAndReading() && Resource && range.End > range.Begin)
  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    if(IsLoading(m_State))
    {
      cmd.AddCPUUsage(GetResID(Resource), ResourceUsage::CPUWrite);

      // when CPU uploading we just save the range (wouldn't be necessary if the range was
      // serialised before the blob)
      if(!gpuUpload)
      {
        m_UploadRanges[cmd.m_CurChunkOffset] = range;
      }
      else
      {
        ID3D12Resource *uploadBuf = GetUploadBuffer(cmd.m_CurChunkOffset, rangeSize);

        if(!uploadBuf)
        {
          RDCERR("Couldn't get upload buffer");
          return false;
        }

        SetObjName(uploadBuf,
                   StringFormat::Fmt("Map data write, %llu bytes for %s/%u @ %llu", rangeSize,
                                     ToStr(origid).c_str(), Subresource, cmd.m_CurChunkOffset));

        D3D12_RANGE maprange = {0, 0};
        void *dst = NULL;
        HRESULT hr = uploadBuf->Map(Subresource, &maprange, &dst);
        CheckHRESULT(hr);

        if(SUCCEEDED(hr))
        {
          maprange.Begin = 0;
          maprange.End = SIZE_T(rangeSize);

          memcpy(dst, MappedData, maprange.End);

          uploadBuf->Unmap(Subresource, &maprange);
        }
        else
        {
          RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
        }
      }
    }

    // now whether we are loading or replaying, if we're GPU uploading do the actual copy
    if(gpuUpload)
    {
      ID3D12Resource *uploadBuf = GetUploadBuffer(cmd.m_CurChunkOffset, rangeSize);

      if(!uploadBuf)
      {
        RDCERR("Couldn't get upload buffer");
        return false;
      }

      // then afterwards just execute a list to copy the result
      m_DataUploadList[m_CurDataUpload]->Reset(m_DataUploadAlloc, NULL);
      m_DataUploadList[m_CurDataUpload]->CopyBufferRegion(Resource, range.Begin, uploadBuf, 0,
                                                          range.End - range.Begin);
      m_DataUploadList[m_CurDataUpload]->Close();

      ID3D12CommandList *l = m_DataUploadList[m_CurDataUpload];
      GetQueue()->ExecuteCommandLists(1, &l);

      m_CurDataUpload++;
      if(m_CurDataUpload == ARRAY_COUNT(m_DataUploadList))
      {
        GPUSync();
        m_CurDataUpload = 0;
      }
    }
  }

  // on loading we always alloc'd for this, whether gpu upload or not
  if(IsLoading(m_State))
    FreeAlignedBuffer(MappedData);

  return true;
}

template bool WrappedID3D12Device::Serialise_MapDataWrite(ReadSerialiser &ser,
                                                          ID3D12Resource *Resource,
                                                          UINT Subresource, byte *MappedData,
                                                          D3D12_RANGE range, bool coherentFlush);
template bool WrappedID3D12Device::Serialise_MapDataWrite(WriteSerialiser &ser,
                                                          ID3D12Resource *Resource,
                                                          UINT Subresource, byte *MappedData,
                                                          D3D12_RANGE range, bool coherentFlush);

void WrappedID3D12Device::MapDataWrite(ID3D12Resource *Resource, UINT Subresource, byte *mapPtr,
                                       D3D12_RANGE range, bool coherentFlush)
{
  CACHE_THREAD_SERIALISER();

  SCOPED_SERIALISE_CHUNK(coherentFlush ? D3D12Chunk::CoherentMapWrite : D3D12Chunk::Resource_Unmap);
  Serialise_MapDataWrite(ser, Resource, Subresource, mapPtr, range, coherentFlush);

  m_FrameCaptureRecord->AddChunk(scope.Get());

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(Resource), eFrameRef_PartialWrite);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_WriteToSubresource(SerialiserType &ser, ID3D12Resource *Resource,
                                                       UINT Subresource, const D3D12_BOX *pDstBox,
                                                       const void *pSrcData, UINT SrcRowPitch,
                                                       UINT SrcDepthPitch)
{
  SERIALISE_ELEMENT(Resource).Important();
  SERIALISE_ELEMENT(Subresource);
  SERIALISE_ELEMENT_OPT(pDstBox);

  uint64_t dataSize = 0;

  if(ser.IsWriting())
  {
    D3D12_RESOURCE_DESC desc = Resource->GetDesc();

    // for buffers the data size is just the width of the box/resource
    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      if(pDstBox)
        dataSize = RDCMIN(desc.Width, UINT64(pDstBox->right - pDstBox->left));
      else
        dataSize = desc.Width;
    }
    else
    {
      UINT width = UINT(desc.Width);
      UINT height = desc.Height;
      // only 3D textures have a depth, array slices are separate subresources.
      UINT depth = (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1);

      // if we have a box, use its dimensions
      if(pDstBox)
      {
        width = RDCMIN(width, pDstBox->right - pDstBox->left);
        height = RDCMIN(height, pDstBox->bottom - pDstBox->top);
        depth = RDCMIN(depth, pDstBox->back - pDstBox->front);
      }

      if(IsBlockFormat(desc.Format))
        height = RDCMAX(1U, AlignUp4(height) / 4);
      else if(IsYUVPlanarFormat(desc.Format))
        height = GetYUVNumRows(desc.Format, height);

      dataSize = 0;

      // if we're copying multiple slices, all but the last one consume SrcDepthPitch bytes
      if(depth > 1)
        dataSize += SrcDepthPitch * (depth - 1);

      // similarly if we're copying multiple rows (possibly block-sized rows) in the final slice
      if(height > 1)
        dataSize += SrcRowPitch * (height - 1);

      // lastly, the final row (or block row) consumes just a tightly packed amount of data
      dataSize += GetRowPitch(width, desc.Format, 0);
    }
  }

  SERIALISE_ELEMENT_ARRAY(pSrcData, dataSize).Important();
  SERIALISE_ELEMENT(dataSize).Hidden();

  SERIALISE_ELEMENT(SrcRowPitch);
  SERIALISE_ELEMENT(SrcDepthPitch);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && Resource)
  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    if(IsLoading(m_State))
      cmd.AddCPUUsage(GetResID(Resource), ResourceUsage::CPUWrite);

    ResourceId origid = GetResourceManager()->GetOriginalID(GetResID(Resource));
    if(m_UploadResourceIds.find(origid) != m_UploadResourceIds.end())
    {
      ID3D12Resource *uploadBuf = GetUploadBuffer(cmd.m_CurChunkOffset, dataSize);

      // during reading, fill out the buffer itself
      if(IsLoading(m_State))
      {
        D3D12_RANGE range = {0, 0};
        void *dst = NULL;
        HRESULT hr = uploadBuf->Map(Subresource, &range, &dst);
        CheckHRESULT(hr);

        if(SUCCEEDED(hr))
        {
          memcpy(dst, pSrcData, (size_t)dataSize);

          range.Begin = 0;
          range.End = (size_t)dataSize;

          uploadBuf->Unmap(Subresource, &range);
        }
        else
        {
          RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
        }
      }

      // then afterwards just execute a list to copy the result
      m_DataUploadList[m_CurDataUpload]->Reset(m_DataUploadAlloc, NULL);
      UINT64 copySize = dataSize;
      if(pDstBox)
        copySize = RDCMIN(copySize, UINT64(pDstBox->right - pDstBox->left));
      m_DataUploadList[m_CurDataUpload]->CopyBufferRegion(Resource, pDstBox ? pDstBox->left : 0,
                                                          uploadBuf, 0, copySize);
      m_DataUploadList[m_CurDataUpload]->Close();
      ID3D12CommandList *l = m_DataUploadList[m_CurDataUpload];
      GetQueue()->ExecuteCommandLists(1, &l);

      m_CurDataUpload++;
      if(m_CurDataUpload == ARRAY_COUNT(m_DataUploadList))
      {
        GPUSync();
        m_CurDataUpload = 0;
      }
    }
    else
    {
      HRESULT hr = Resource->Map(Subresource, NULL, NULL);
      CheckHRESULT(hr);

      if(SUCCEEDED(hr))
      {
        Resource->WriteToSubresource(Subresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

        Resource->Unmap(Subresource, NULL);
      }
      else
      {
        RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
      }
    }
  }

  return true;
}

template bool WrappedID3D12Device::Serialise_WriteToSubresource(
    ReadSerialiser &ser, ID3D12Resource *Resource, UINT Subresource, const D3D12_BOX *pDstBox,
    const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
template bool WrappedID3D12Device::Serialise_WriteToSubresource(
    WriteSerialiser &ser, ID3D12Resource *Resource, UINT Subresource, const D3D12_BOX *pDstBox,
    const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);

void WrappedID3D12Device::WriteToSubresource(ID3D12Resource *Resource, UINT Subresource,
                                             const D3D12_BOX *pDstBox, const void *pSrcData,
                                             UINT SrcRowPitch, UINT SrcDepthPitch)
{
  bool capframe = false;
  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  if(capframe)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Resource_WriteToSubresource);
    Serialise_WriteToSubresource(ser, Resource, Subresource, pDstBox, pSrcData, SrcRowPitch,
                                 SrcDepthPitch);

    m_FrameCaptureRecord->AddChunk(scope.Get());

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(Resource), eFrameRef_PartialWrite);
  }
}

HRESULT WrappedID3D12Device::Present(ID3D12GraphicsCommandList *pOverlayCommandList,
                                     IDXGISwapper *swapper, UINT SyncInterval, UINT Flags)
{
  if((Flags & DXGI_PRESENT_TEST) != 0)
    return S_OK;

  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  DeviceOwnedWindow devWnd((ID3D12Device *)this, swapper->GetHWND());

  bool activeWindow = RenderDoc::Inst().IsActiveWindow(devWnd);

  m_LastSwap = swapper;

  if(IsBackgroundCapturing(m_State))
  {
    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      SwapPresentInfo &swapInfo = m_SwapChains[swapper];
      D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapInfo.rtvs[swapper->GetLastPresentedBuffer()];

      if(rtv.ptr)
      {
        m_TextRenderer->SetOutputDimensions(swapper->GetWidth(), swapper->GetHeight(),
                                            swapper->GetFormat());

        ID3D12GraphicsCommandList *list = pOverlayCommandList;
        bool submitlist = false;

        if(!list)
        {
          list = GetNewList();
          submitlist = true;
        }

        // buffer will be in common for presentation, transition to render target
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Transition.pResource =
            (ID3D12Resource *)swapper->GetBackbuffers()[swapper->GetLastPresentedBuffer()];
        barrier.Transition.Subresource = 0;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        list->ResourceBarrier(1, &barrier);

        list->OMSetRenderTargets(1, &rtv, FALSE, NULL);

        rdcstr overlayText =
            RenderDoc::Inst().GetOverlayText(RDCDriver::D3D12, devWnd, m_FrameCounter, 0);

        m_TextRenderer->RenderText(list, 0.0f, 0.0f, overlayText);

        // transition backbuffer back again
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        list->ResourceBarrier(1, &barrier);

        if(submitlist)
        {
          list->Close();

          ExecuteLists(swapInfo.queue);
          FlushLists(false, swapInfo.queue);
        }
      }
    }
  }

  RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D12, true);

  // serialise the present call, even for inactive windows
  if(IsActiveCapturing(m_State))
  {
    SERIALISE_TIME_CALL();

    ID3D12Resource *backbuffer = NULL;

    if(swapper != NULL)
      backbuffer = (ID3D12Resource *)swapper->GetBackbuffers()[swapper->GetLastPresentedBuffer()];

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Swapchain_Present);
    Serialise_Present(ser, backbuffer, SyncInterval, Flags);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  if(!activeWindow)
  {
    // first present to *any* window, even inactive, terminates frame 0
    if(m_FirstFrameCapture && IsActiveCapturing(m_State))
    {
      RenderDoc::Inst().EndFrameCapture(
          DeviceOwnedWindow((ID3D12Device *)this, m_FirstFrameCaptureWindow));
      m_FirstFrameCaptureWindow = NULL;
      m_FirstFrameCapture = false;
    }

    return S_OK;
  }

  // kill any current capture that isn't application defined
  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture(devWnd);

  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter))
  {
    RenderDoc::Inst().StartFrameCapture(devWnd);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = m_FrameCounter;
  }

  return S_OK;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_Present(SerialiserType &ser, ID3D12Resource *PresentedImage,
                                            UINT SyncInterval, UINT Flags)
{
  SERIALISE_ELEMENT_LOCAL(PresentedBackbuffer, GetResID(PresentedImage))
      .TypedAs("ID3D12Resource *"_lit)
      .Important();

  // we don't do anything with these parameters, they're just here to store
  // them for user benefits
  SERIALISE_ELEMENT(SyncInterval);
  SERIALISE_ELEMENT(Flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && IsLoading(m_State))
  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();
    cmd.AddEvent();

    ActionDescription action;

    action.customName = StringFormat::Fmt("Present(%s)", ToStr(PresentedBackbuffer).c_str());
    action.flags |= ActionFlags::Present;

    cmd.m_LastPresentedImage = PresentedBackbuffer;
    action.copyDestination = PresentedBackbuffer;

    cmd.AddAction(action);
  }

  return true;
}

template bool WrappedID3D12Device::Serialise_Present(ReadSerialiser &ser,
                                                     ID3D12Resource *PresentedImage,
                                                     UINT SyncInterval, UINT Flags);
template bool WrappedID3D12Device::Serialise_Present(WriteSerialiser &ser,
                                                     ID3D12Resource *PresentedImage,
                                                     UINT SyncInterval, UINT Flags);

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT_LOCAL(frameNumber, m_CapturedFrames.back().frameNumber);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayMode(m_State))
  {
    GetReplay()->WriteFrameRecord().frameInfo.frameNumber = frameNumber;
    RDCEraseEl(GetReplay()->WriteFrameRecord().frameInfo.stats);
  }

  return true;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_BeginCaptureFrame(SerialiserType &ser)
{
  BarrierSet barriers;

  if(IsReplayingAndReading() && IsLoading(m_State))
  {
    m_InitialResourceStates = m_ResourceStates;

    GetDebugManager()->PrepareExecuteIndirectPatching(m_OrigGPUAddresses);
    GetResourceManager()->GetRaytracingResourceAndUtilHandler()->PrepareRayDispatchBuffer(
        &m_OrigGPUAddresses);
  }

  std::map<ResourceId, SubresourceStateVector> initialStates;

  {
    SCOPED_LOCK(m_ResourceStatesLock);    // not needed on replay, but harmless also
    GetResourceManager()->SerialiseResourceStates(ser, barriers, m_ResourceStates,
                                                  m_InitialResourceStates);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && !barriers.empty())
  {
    // apply initial resource states
    ID3D12GraphicsCommandListX *list = GetNewList();
    if(!list)
      return false;

    barriers.Apply(list);

    list->Close();

    ExecuteLists();
    FlushLists();
  }

  return true;
}

template bool WrappedID3D12Device::Serialise_BeginCaptureFrame(ReadSerialiser &ser);
template bool WrappedID3D12Device::Serialise_BeginCaptureFrame(WriteSerialiser &ser);

void WrappedID3D12Device::EndCaptureFrame()
{
  WriteSerialiser &ser = GetThreadSerialiser();
  ser.SetActionChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  // here for compatibility reasons, this used to store the presented Resource.
  SERIALISE_ELEMENT_LOCAL(PresentedBackbuffer, ResourceId()).Hidden();

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedID3D12Device::StartFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");

  if(m_Queue == NULL)
  {
    RDCLOG("Creating direct queue as none was found in the application");

    // pretend this is the application's call and just release - everything else will be handled the
    // same (the queue will be kept alive internally)
    ID3D12CommandQueue *q = NULL;
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void **)&q);
    q->Release();
  }

  WrappedID3D12CommandAllocator::PauseResets();

  m_CaptureTimer.Restart();

  m_AppControlledCapture = true;

  m_SubmitCounter = 0;

  FrameDescription frame;
  frame.frameNumber = ~0U;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  GetDebugMessages();
  m_DebugMessages.clear();

  GetResourceManager()->ClearReferencedResources();

  // need to do all this atomically so that no other commands
  // will check to see if they need to markdirty or markpendingdirty
  // and go into the frame record.
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    initStateCurBatch = 0;
    initStateCurList = NULL;

    GPUSyncAllQueues();

    // wait until we've synced all queues to check for these
    GetResourceManager()->GetRaytracingResourceAndUtilHandler()->CheckPendingASBuilds();

    GetResourceManager()->PrepareInitialContents();

    if(initStateCurList)
    {
      // close the final list
      initStateCurList->Close();
    }

    initStateCurBatch = 0;
    initStateCurList = NULL;

    ExecuteLists(NULL, true);
    FlushLists();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();

    // fetch and discard debug messages so we don't serialise any messages of our own.
    (void)GetDebugMessages();

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureBegin);

      Serialise_BeginCaptureFrame(ser);

      // need to hold onto this as it must come right after the capture chunk,
      // before any command lists
      m_HeaderChunk = scope.Get();
    }

    m_State = CaptureState::ActiveCapturing;

    // keep all queues alive during the capture, by adding a refcount. Also reference the creation
    // record so that it's pulled in as initialisation chunks.
    for(auto it = m_Queues.begin(); it != m_Queues.end(); ++it)
    {
      (*it)->AddRef();
      GetResourceManager()->MarkResourceFrameReferenced((*it)->GetCreationRecord()->GetResourceID(),
                                                        eFrameRef_Read);
    }

    if(m_BindlessResourceUseActive)
    {
      SCOPED_LOCK(m_ResourceStatesLock);

      for(auto it = m_BindlessFrameRefs.begin(); it != m_BindlessFrameRefs.end(); ++it)
        GetResourceManager()->MarkResourceFrameReferenced(it->first, it->second);
    }

    m_RefQueues = m_Queues;
    m_RefBuffers = WrappedID3D12Resource::AddRefBuffersBeforeCapture(GetResourceManager());
  }

  GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Read);

  rdcarray<D3D12ResourceRecord *> forced = GetForcedReferences();

  for(auto it = forced.begin(); it != forced.end(); ++it)
    GetResourceManager()->MarkResourceFrameReferenced((*it)->GetResourceID(), eFrameRef_Read);
}

bool WrappedID3D12Device::EndFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  IDXGISwapper *swapper = NULL;
  SwapPresentInfo swapInfo = {};

  if(devWnd.windowHandle)
  {
    for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    {
      if(it->first->GetHWND() == devWnd.windowHandle)
      {
        swapper = it->first;
        swapInfo = it->second;
        break;
      }
    }

    if(swapper == NULL)
    {
      RDCERR("Output window %p provided for frame capture corresponds with no known swap chain",
             devWnd.windowHandle);
      return false;
    }
  }

  RDCLOG("Finished capture, Frame %u", m_CapturedFrames.back().frameNumber);

  ID3D12Resource *backbuffer = NULL;

  if(swapper == NULL)
  {
    swapper = m_LastSwap;
    swapInfo = m_SwapChains[swapper];
  }

  if(swapper != NULL)
    backbuffer = (ID3D12Resource *)swapper->GetBackbuffers()[swapper->GetLastPresentedBuffer()];

  rdcarray<WrappedID3D12CommandQueue *> queues;

  // transition back to IDLE and readback initial states atomically
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);
    EndCaptureFrame();

    queues = m_Queues;

    bool ContainsExecuteIndirect = false;

    for(auto it = queues.begin(); it != queues.end(); ++it)
      ContainsExecuteIndirect |= (*it)->GetResourceRecord()->ContainsExecuteIndirect;

    // There is no easy way to mark resource referenced in the AS input and the dispatch tables.
    // We could be more selective and only force-reference acceleration structure buffers -
    // resources in the AS state - but this would not include scratch buffers (which could be done
    // by hand) and build input buffers (which can't). we don't do this with the forced reference
    // system as we want this to be retroactive - only after seeing an AS build do we mark all
    // buffers referenced but buffers could be created before an AS is built.
    if(ContainsExecuteIndirect || m_HaveSeenASBuild)
      WrappedID3D12Resource::RefBuffers(GetResourceManager());

    m_State = CaptureState::BackgroundCapturing;

    GPUSync();
  }

  rdcarray<MapState> maps = GetMaps();
  for(auto it = maps.begin(); it != maps.end(); ++it)
    GetWrapped(it->res)->FreeShadow();

  const uint32_t maxSize = 2048;
  RenderDoc::FramePixels fp;

  // gather backbuffer screenshot
  if(backbuffer != NULL)
  {
    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufDesc;
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = 1;

    D3D12_RESOURCE_DESC desc = backbuffer->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};

    m_pDevice->GetCopyableFootprints(&desc, 0, 1, 0, &layout, NULL, NULL, &bufDesc.Width);

    ID3D12Resource *copyDst = NULL;
    HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                    D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&copyDst);

    if(SUCCEEDED(hr))
    {
      ID3D12GraphicsCommandList *list = Unwrap(GetNewList());

      D3D12_RESOURCE_BARRIER barrier = {};

      // we know there's only one subresource, and it will be in PRESENT state
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.Transition.pResource = Unwrap(backbuffer);
      barrier.Transition.Subresource = 0;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

      list->ResourceBarrier(1, &barrier);

      // copy to readback buffer
      D3D12_TEXTURE_COPY_LOCATION dst, src;

      src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src.pResource = Unwrap(backbuffer);
      src.SubresourceIndex = 0;

      dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dst.pResource = copyDst;
      dst.PlacedFootprint = layout;

      list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

      // transition back
      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
      list->ResourceBarrier(1, &barrier);

      list->Close();

      ExecuteLists(NULL, true);
      FlushLists();

      byte *data = NULL;
      hr = copyDst->Map(0, NULL, (void **)&data);

      if(SUCCEEDED(hr) && data)
      {
        fp.len = (uint32_t)bufDesc.Width;
        fp.data = new uint8_t[fp.len];
        memcpy(fp.data, data, fp.len);

        ResourceFormat fmt = MakeResourceFormat(desc.Format);
        fp.width = (uint32_t)desc.Width;
        fp.height = (uint32_t)desc.Height;
        fp.pitch = layout.Footprint.RowPitch;
        fp.stride = fmt.compByteWidth * fmt.compCount;
        fp.bpc = fmt.compByteWidth;
        fp.bgra = fmt.BGRAOrder();
        fp.max_width = maxSize;
        fp.pitch_requirement = 8;
        switch(fmt.type)
        {
          case ResourceFormatType::R10G10B10A2:
            fp.stride = 4;
            fp.buf1010102 = true;
            break;
          case ResourceFormatType::R5G6B5:
            fp.stride = 2;
            fp.buf565 = true;
            break;
          case ResourceFormatType::R5G5B5A1:
            fp.stride = 2;
            fp.buf5551 = true;
            break;
          default: break;
        }
        copyDst->Unmap(0, NULL);
      }
      else
      {
        RDCERR("Couldn't map readback buffer: HRESULT: %s", ToStr(hr).c_str());
      }

      SAFE_RELEASE(copyDst);
    }
    else
    {
      RDCERR("Couldn't create readback buffer: HRESULT: %s", ToStr(hr).c_str());
    }
  }

  RDCFile *rdc =
      RenderDoc::Inst().CreateRDC(RDCDriver::D3D12, m_CapturedFrames.back().frameNumber, fp);

  StreamWriter *captureWriter = NULL;

  HMODULE D3D12Core = GetModuleHandleA("D3D12Core.dll");
  if(D3D12Core)
  {
    m_InitParams.SDKVersion = *(DWORD *)GetProcAddress(D3D12Core, "D3D12SDKVersion");
  }

  if(rdc)
  {
    SectionProperties props;

    // Compress with LZ4 so that it's fast
    props.flags = SectionFlags::LZ4Compressed;
    props.version = m_SectionVersion;
    props.type = SectionType::FrameCapture;

    captureWriter = rdc->WriteSection(props);
  }
  else
  {
    captureWriter = new StreamWriter(StreamWriter::InvalidStream);
  }

  uint64_t captureSectionSize = 0;

  {
    WriteSerialiser ser(captureWriter, Ownership::Stream);

    ser.SetChunkMetadataRecording(GetThreadSerialiser().GetChunkMetadataRecording());

    ser.SetUserData(GetResourceManager());

    m_InitParams.usedDXIL = m_UsedDXIL;

    if(m_UsedDXIL)
    {
      RDCLOG("Capture used DXIL");
    }

    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, sizeof(D3D12InitParams));

      SERIALISE_ELEMENT(m_InitParams);
    }

    RDCDEBUG("Inserting Resource Serialisers");

    GetResourceManager()->InsertReferencedChunks(ser);

    GetResourceManager()->InsertInitialContentsChunks(ser);

    RDCDEBUG("Creating Capture Scope");

    GetResourceManager()->Serialise_InitialContentsNeeded(ser);

    {
      SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);

      Serialise_CaptureScope(ser);
    }

    m_HeaderChunk->Write(ser);

    // don't need to lock access to m_CmdListRecords as we are no longer
    // in capframe (the transition is thread-protected) so nothing will be
    // pushed to the vector

    std::map<int64_t, Chunk *> recordlist;

    for(auto it = queues.begin(); it != queues.end(); ++it)
    {
      WrappedID3D12CommandQueue *q = *it;

      const rdcarray<D3D12ResourceRecord *> &cmdListRecords = q->GetCmdLists();

      RDCDEBUG("Flushing %u command list records from queue %s", (uint32_t)cmdListRecords.size(),
               ToStr(q->GetResourceID()).c_str());

      for(size_t i = 0; i < cmdListRecords.size(); i++)
      {
        uint32_t prevSize = (uint32_t)recordlist.size();
        cmdListRecords[i]->Insert(recordlist);

        // prevent complaints in release that prevSize is unused
        (void)prevSize;

        RDCDEBUG("Adding %u chunks to file serialiser from command list %s",
                 (uint32_t)recordlist.size() - prevSize,
                 ToStr(cmdListRecords[i]->GetResourceID()).c_str());
      }

      q->GetResourceRecord()->Insert(recordlist);
    }

    m_FrameCaptureRecord->Insert(recordlist);

    RDCDEBUG("Flushing %u chunks to file serialiser from context record",
             (uint32_t)recordlist.size());

    float num = float(recordlist.size());
    float idx = 0.0f;

    for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
    {
      RenderDoc::Inst().SetProgress(CaptureProgress::SerialiseFrameContents, idx / num);
      idx += 1.0f;
      it->second->Write(ser);
    }

    RDCDEBUG("Done");

    captureSectionSize = captureWriter->GetOffset();
  }

  RDCLOG("Captured D3D12 frame with %f MB capture section in %f seconds",
         double(captureSectionSize) / (1024.0 * 1024.0), m_CaptureTimer.GetMilliseconds() / 1000.0);

  if(D3D12Core)
  {
    if(rdc)
    {
      wchar_t wide_core_filename[MAX_PATH + 1] = {};
      GetModuleFileNameW(D3D12Core, wide_core_filename, MAX_PATH);

      rdcstr core_filename = StringFormat::Wide2UTF8(wide_core_filename);

      bytebuf buf;
      FileIO::ReadAll(core_filename, buf);

      {
        SectionProperties props;

        props.flags = SectionFlags::ZstdCompressed;
        props.version = 1;
        props.type = SectionType::D3D12Core;

        captureWriter = rdc->WriteSection(props);

        captureWriter->Write(buf.data(), buf.size());

        captureWriter->Finish();
        SAFE_DELETE(captureWriter);
      }

      buf.clear();

      // try to grab the SDK layers which will be needed for debug on replay

      rdcstr sdklayers_filename = get_dirname(core_filename) + "/d3d12sdklayers.dll";

      FileIO::ReadAll(sdklayers_filename, buf);

      if(!buf.empty())
      {
        SectionProperties props;

        props.flags = SectionFlags::ZstdCompressed;
        props.version = 1;
        props.type = SectionType::D3D12SDKLayers;

        captureWriter = rdc->WriteSection(props);

        captureWriter->Write(buf.data(), buf.size());

        captureWriter->Finish();
        SAFE_DELETE(captureWriter);
      }
    }
  }

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  for(auto it = queues.begin(); it != queues.end(); ++it)
    (*it)->ClearAfterCapture();

  // remove the references held during capture, potentially releasing the queue/buffer.
  for(WrappedID3D12CommandQueue *q : m_RefQueues)
    q->Release();

  for(ID3D12Resource *r : m_RefBuffers)
    r->Release();

  for(ID3D12Heap *h : m_InitialStateHeaps)
    h->Release();
  m_InitialStateHeaps.clear();

  WrappedID3D12CommandAllocator::ResumeResets();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  return true;
}

bool WrappedID3D12Device::DiscardFrameCapture(DeviceOwnedWindow devWnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  RDCLOG("Discarding frame capture.");

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_CapturedFrames.pop_back();

  rdcarray<WrappedID3D12CommandQueue *> queues;

  // transition back to IDLE and readback initial states atomically
  {
    SCOPED_WRITELOCK(m_CapTransitionLock);

    m_State = CaptureState::BackgroundCapturing;

    GPUSync();

    queues = m_Queues;
  }

  rdcarray<MapState> maps = GetMaps();
  for(auto it = maps.begin(); it != maps.end(); ++it)
    GetWrapped(it->res)->FreeShadow();

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  for(auto it = queues.begin(); it != queues.end(); ++it)
    (*it)->ClearAfterCapture();

  // remove the reference held during capture, potentially releasing the queue.
  for(WrappedID3D12CommandQueue *q : m_RefQueues)
    q->Release();

  for(ID3D12Resource *r : m_RefBuffers)
    r->Release();

  for(ID3D12Heap *h : m_InitialStateHeaps)
    h->Release();
  m_InitialStateHeaps.clear();

  WrappedID3D12CommandAllocator::ResumeResets();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  return true;
}

void WrappedID3D12Device::UploadBLASBufferAddresses()
{
  if(m_addressBufferUploaded)
    return;

  rdcarray<BlasAddressPair> blasAddressPair;
  D3D12ResourceManager *resManager = GetResourceManager();

  for(size_t i = 0; i < m_OrigGPUAddresses.addresses.size(); i++)
  {
    GPUAddressRange addressRange = m_OrigGPUAddresses.addresses[i];
    ResourceId resId = addressRange.id;
    if(resManager->HasLiveResource(resId))
    {
      WrappedID3D12Resource *wrappedRes = (WrappedID3D12Resource *)resManager->GetLiveResource(resId);
      if(wrappedRes->IsAccelerationStructureResource())
      {
        BlasAddressPair addressPair;
        addressPair.oldAddress.start = addressRange.start;
        addressPair.oldAddress.end = addressRange.realEnd;

        addressPair.newAddress.start = wrappedRes->GetGPUVirtualAddress();
        addressPair.newAddress.end = addressPair.newAddress.start + wrappedRes->GetDesc().Width;
        blasAddressPair.push_back(addressPair);
      }
    }
  }

  m_blasAddressCount = (uint32_t)blasAddressPair.size();

  uint64_t requiredSize = blasAddressPair.size() * sizeof(BlasAddressPair);

  D3D12_RESOURCE_DESC addressBufferResDesc;
  addressBufferResDesc.Alignment = 0;
  addressBufferResDesc.DepthOrArraySize = 1;
  addressBufferResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  addressBufferResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  addressBufferResDesc.Format = DXGI_FORMAT_UNKNOWN;
  addressBufferResDesc.Height = 1;
  addressBufferResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  addressBufferResDesc.MipLevels = 1;
  addressBufferResDesc.SampleDesc.Count = 1;
  addressBufferResDesc.SampleDesc.Quality = 0;
  addressBufferResDesc.Width = RDCMAX(8ULL, requiredSize);

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  HRESULT hr = CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &addressBufferResDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
      NULL, __uuidof(ID3D12Resource), (void **)&m_blasAddressBufferUploadResource);

  if(!SUCCEEDED(hr))
  {
    RDCERR("Unable to create upload buffer for BLAS address");
  }
  else
  {
    D3D12_RANGE readRange = {0, 0};
    void *ptr = NULL;
    HRESULT result = m_blasAddressBufferUploadResource->Map(0, &readRange, &ptr);

    if(!SUCCEEDED(result))
    {
      RDCERR("Unable to map the resource for uploading old addresses");
    }
    else
    {
      memcpy((byte *)ptr, blasAddressPair.data(), (size_t)requiredSize);
      m_blasAddressBufferUploadResource->Unmap(0, NULL);
    }
  }

  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  hr = CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &addressBufferResDesc,
                               D3D12_RESOURCE_STATE_COPY_DEST, NULL, __uuidof(ID3D12Resource),
                               (void **)&m_blasAddressBufferResource);

  if(!SUCCEEDED(hr))
  {
    RDCERR("Unable to create upload buffer for BLAS address");
  }
  else
  {
    ID3D12GraphicsCommandList *initList = GetInitialStateList();
    initList->CopyBufferRegion(m_blasAddressBufferResource, 0, m_blasAddressBufferUploadResource, 0,
                               requiredSize);

    D3D12_RESOURCE_BARRIER resBarrier;
    resBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    resBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    resBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    resBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    resBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    resBarrier.Transition.pResource = m_blasAddressBufferResource;
    initList->ResourceBarrier(1, &resBarrier);
  }

  m_addressBufferUploaded = true;
}

void WrappedID3D12Device::ReleaseResource(ID3D12DeviceChild *res)
{
  ResourceId id = GetResID(res);

  {
    SCOPED_LOCK(m_ResourceStatesLock);
    m_ResourceStates.erase(id);
  }

  {
    SCOPED_LOCK(m_ForcedReferencesLock);
    m_ForcedReferences.removeOne(GetRecord(res));
  }

  {
    SCOPED_LOCK(m_SparseLock);
    m_SparseResources.erase(id);
    m_SparseHeaps.erase(id);
  }

  D3D12ResourceRecord *record = GetRecord(res);

  if(record)
    record->Delete(GetResourceManager());

  // wrapped resources get released all the time, we don't want to
  // try and slerp in a resource release. Just the explicit ones
  if(IsReplayMode(m_State))
  {
    if(GetResourceManager()->HasLiveResource(id))
      GetResourceManager()->EraseLiveResource(id);
    return;
  }
}

HRESULT WrappedID3D12Device::CreatePipeState(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &desc,
                                             ID3D12PipelineState **state)
{
  if(m_pDevice3)
  {
    D3D12_PACKED_PIPELINE_STATE_STREAM_DESC packedDesc = desc;
    return CreatePipelineState(packedDesc.AsDescStream(), __uuidof(ID3D12PipelineState),
                               (void **)state);
  }

  if(desc.CS.BytecodeLength > 0)
  {
    D3D12_COMPUTE_PIPELINE_STATE_DESC compDesc;
    compDesc.pRootSignature = desc.pRootSignature;
    compDesc.CS = desc.CS;
    compDesc.NodeMask = desc.NodeMask;
    compDesc.CachedPSO = desc.CachedPSO;
    compDesc.Flags = desc.Flags;
    return CreateComputePipelineState(&compDesc, __uuidof(ID3D12PipelineState), (void **)state);
  }
  else
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc;
    graphicsDesc.pRootSignature = desc.pRootSignature;
    graphicsDesc.VS = desc.VS;
    graphicsDesc.PS = desc.PS;
    graphicsDesc.DS = desc.DS;
    graphicsDesc.HS = desc.HS;
    graphicsDesc.GS = desc.GS;
    graphicsDesc.StreamOutput = desc.StreamOutput;
    graphicsDesc.BlendState = desc.BlendState;
    graphicsDesc.SampleMask = desc.SampleMask;

    // can't create mesh shaders with old function, so we should not be trying
    RDCASSERT(desc.AS.BytecodeLength == 0);
    RDCASSERT(desc.MS.BytecodeLength == 0);

    // graphicsDesc.RasterizerState = desc.RasterizerState;
    {
      graphicsDesc.RasterizerState.FillMode = desc.RasterizerState.FillMode;
      graphicsDesc.RasterizerState.CullMode = desc.RasterizerState.CullMode;
      graphicsDesc.RasterizerState.FrontCounterClockwise = desc.RasterizerState.FrontCounterClockwise;
      graphicsDesc.RasterizerState.DepthBias = INT(desc.RasterizerState.DepthBias);
      graphicsDesc.RasterizerState.DepthBiasClamp = desc.RasterizerState.DepthBiasClamp;
      graphicsDesc.RasterizerState.SlopeScaledDepthBias = desc.RasterizerState.SlopeScaledDepthBias;
      graphicsDesc.RasterizerState.DepthClipEnable = desc.RasterizerState.DepthClipEnable;
      graphicsDesc.RasterizerState.ForcedSampleCount = desc.RasterizerState.ForcedSampleCount;
      graphicsDesc.RasterizerState.ConservativeRaster = desc.RasterizerState.ConservativeRaster;

      switch(desc.RasterizerState.LineRasterizationMode)
      {
        case D3D12_LINE_RASTERIZATION_MODE_ALIASED:
          graphicsDesc.RasterizerState.MultisampleEnable = FALSE;
          graphicsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
          break;
        case D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED:
          graphicsDesc.RasterizerState.MultisampleEnable = FALSE;
          graphicsDesc.RasterizerState.AntialiasedLineEnable = TRUE;
          break;
        case D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE:
        case D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_NARROW:
          graphicsDesc.RasterizerState.MultisampleEnable = TRUE;
          graphicsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
          break;
        default:
          graphicsDesc.RasterizerState.MultisampleEnable = FALSE;
          graphicsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
          break;
      }
    }

    // graphicsDesc.DepthStencilState = desc.DepthStencilState;
    {
      graphicsDesc.DepthStencilState.DepthEnable = desc.DepthStencilState.DepthEnable;
      graphicsDesc.DepthStencilState.DepthWriteMask = desc.DepthStencilState.DepthWriteMask;
      graphicsDesc.DepthStencilState.DepthFunc = desc.DepthStencilState.DepthFunc;
      graphicsDesc.DepthStencilState.StencilEnable = desc.DepthStencilState.StencilEnable;
      graphicsDesc.DepthStencilState.StencilReadMask =
          desc.DepthStencilState.FrontFace.StencilReadMask;
      graphicsDesc.DepthStencilState.StencilWriteMask =
          desc.DepthStencilState.FrontFace.StencilWriteMask;
      graphicsDesc.DepthStencilState.FrontFace.StencilFunc =
          desc.DepthStencilState.FrontFace.StencilFunc;
      graphicsDesc.DepthStencilState.FrontFace.StencilPassOp =
          desc.DepthStencilState.FrontFace.StencilPassOp;
      graphicsDesc.DepthStencilState.FrontFace.StencilFailOp =
          desc.DepthStencilState.FrontFace.StencilFailOp;
      graphicsDesc.DepthStencilState.FrontFace.StencilDepthFailOp =
          desc.DepthStencilState.FrontFace.StencilDepthFailOp;
      graphicsDesc.DepthStencilState.BackFace.StencilFunc =
          desc.DepthStencilState.BackFace.StencilFunc;
      graphicsDesc.DepthStencilState.BackFace.StencilPassOp =
          desc.DepthStencilState.BackFace.StencilPassOp;
      graphicsDesc.DepthStencilState.BackFace.StencilFailOp =
          desc.DepthStencilState.BackFace.StencilFailOp;
      graphicsDesc.DepthStencilState.BackFace.StencilDepthFailOp =
          desc.DepthStencilState.BackFace.StencilDepthFailOp;
      // no DepthBoundsTestEnable
    }
    graphicsDesc.InputLayout = desc.InputLayout;
    graphicsDesc.IBStripCutValue = desc.IBStripCutValue;
    graphicsDesc.PrimitiveTopologyType = desc.PrimitiveTopologyType;
    graphicsDesc.NumRenderTargets = desc.RTVFormats.NumRenderTargets;
    memcpy(graphicsDesc.RTVFormats, desc.RTVFormats.RTFormats, 8 * sizeof(DXGI_FORMAT));
    graphicsDesc.DSVFormat = desc.DSVFormat;
    graphicsDesc.SampleDesc = desc.SampleDesc;
    graphicsDesc.NodeMask = desc.NodeMask;
    graphicsDesc.CachedPSO = desc.CachedPSO;
    graphicsDesc.Flags = desc.Flags;
    return CreateGraphicsPipelineState(&graphicsDesc, __uuidof(ID3D12PipelineState), (void **)state);
  }
}

void WrappedID3D12Device::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                          rdcstr d)
{
  D3D12CommandData &cmd = *m_Queue->GetCommandData();

  DebugMessage msg;
  msg.eventId = 0;
  msg.messageID = 0;
  msg.source = src;
  msg.category = c;
  msg.severity = sv;
  msg.description = d;
  if(IsActiveReplaying(m_State))
  {
    // look up the EID this action came from
    D3D12CommandData::ActionUse use(cmd.m_CurChunkOffset, 0);
    auto it = std::lower_bound(cmd.m_ActionUses.begin(), cmd.m_ActionUses.end(), use);
    RDCASSERT(it != cmd.m_ActionUses.end());

    if(it != cmd.m_ActionUses.end())
      msg.eventId = it->eventId;
    else
      RDCERR("Couldn't locate action use for current chunk offset %llu", cmd.m_CurChunkOffset);

    AddDebugMessage(msg);
  }
  else
  {
    cmd.m_EventMessages.push_back(msg);
  }
}

void WrappedID3D12Device::AddDebugMessage(const DebugMessage &msg)
{
  m_DebugMessages.push_back(msg);
}

rdcarray<DebugMessage> WrappedID3D12Device::GetDebugMessages()
{
  rdcarray<DebugMessage> ret;

  if(IsActiveReplaying(m_State))
  {
    // once we're active replaying, m_DebugMessages will contain all the messages from loading
    // (either from the captured serialised messages, or from ourselves during replay).
    ret.swap(m_DebugMessages);

    return ret;
  }

  if(IsCaptureMode(m_State))
  {
    // add any manually-added messages before fetching those from the API
    ret.swap(m_DebugMessages);
  }

  // during loading only try and fetch messages if we're doing that deliberately during replay
  if(IsLoading(m_State) && !m_ReplayOptions.apiValidation)
    return ret;

  // during capture, and during loading if the option is enabled, we fetch messages from the API

  if(!m_pInfoQueue)
    return ret;

  UINT64 numMessages = m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

  for(UINT64 i = 0; i < m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter(); i++)
  {
    SIZE_T len = 0;
    m_pInfoQueue->GetMessage(i, NULL, &len);

    char *msgbuf = new char[len];
    D3D12_MESSAGE *message = (D3D12_MESSAGE *)msgbuf;

    m_pInfoQueue->GetMessage(i, message, &len);

    DebugMessage msg;
    msg.eventId = 0;
    msg.source = MessageSource::API;
    msg.category = MessageCategory::Miscellaneous;
    msg.severity = MessageSeverity::Medium;

    switch(message->Category)
    {
      case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:
        msg.category = MessageCategory::Application_Defined;
        break;
      case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:
        msg.category = MessageCategory::Miscellaneous;
        break;
      case D3D12_MESSAGE_CATEGORY_INITIALIZATION:
        msg.category = MessageCategory::Initialization;
        break;
      case D3D12_MESSAGE_CATEGORY_CLEANUP: msg.category = MessageCategory::Cleanup; break;
      case D3D12_MESSAGE_CATEGORY_COMPILATION: msg.category = MessageCategory::Compilation; break;
      case D3D12_MESSAGE_CATEGORY_STATE_CREATION:
        msg.category = MessageCategory::State_Creation;
        break;
      case D3D12_MESSAGE_CATEGORY_STATE_SETTING:
        msg.category = MessageCategory::State_Setting;
        break;
      case D3D12_MESSAGE_CATEGORY_STATE_GETTING:
        msg.category = MessageCategory::State_Getting;
        break;
      case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
        msg.category = MessageCategory::Resource_Manipulation;
        break;
      case D3D12_MESSAGE_CATEGORY_EXECUTION: msg.category = MessageCategory::Execution; break;
      case D3D12_MESSAGE_CATEGORY_SHADER: msg.category = MessageCategory::Shaders; break;
      default: RDCWARN("Unexpected message category: %d", message->Category); break;
    }

    switch(message->Severity)
    {
      case D3D12_MESSAGE_SEVERITY_CORRUPTION: msg.severity = MessageSeverity::High; break;
      case D3D12_MESSAGE_SEVERITY_ERROR: msg.severity = MessageSeverity::High; break;
      case D3D12_MESSAGE_SEVERITY_WARNING: msg.severity = MessageSeverity::Medium; break;
      case D3D12_MESSAGE_SEVERITY_INFO: msg.severity = MessageSeverity::Low; break;
      case D3D12_MESSAGE_SEVERITY_MESSAGE: msg.severity = MessageSeverity::Info; break;
      default: RDCWARN("Unexpected message severity: %d", message->Severity); break;
    }

    msg.messageID = (uint32_t)message->ID;
    msg.description = rdcstr(message->pDescription);

    // during capture add all messages. Otherwise only add this message if it's different to the
    // last one - we can sometimes get duplicated messages
    if(!IsLoading(m_State) || ret.empty() || !(ret.back() == msg))
      ret.push_back(msg);

    SAFE_DELETE_ARRAY(msgbuf);
  }

  // Docs are fuzzy on the thread safety of the info queue, but I'm going to assume it should only
  // ever be accessed on one thread since it's tied to the device & immediate context.
  // There doesn't seem to be a way to lock it for access and without that there's no way to know
  // that a new message won't be added between the time you retrieve the last one and clearing the
  // queue. There is also no way to pop a message that I can see, which would presumably be the
  // best way if its member functions are thread safe themselves (if the queue is protected
  // internally).
  RDCASSERT(numMessages == m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter());

  m_pInfoQueue->ClearStoredMessages();

  return ret;
}

void WrappedID3D12Device::CheckHRESULT(HRESULT hr)
{
  if(SUCCEEDED(hr) || HasFatalError())
    return;

  if(hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
     hr == DXGI_ERROR_DEVICE_HUNG || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
  {
    SET_ERROR_RESULT(m_FatalError, ResultCode::DeviceLost, "Logging device lost fatal error for %s",
                     ToStr(hr).c_str());
  }
  else if(hr == E_OUTOFMEMORY)
  {
    if(m_OOMHandler)
    {
      RDCLOG("Ignoring out of memory error that will be handled");
    }
    else
    {
      RDCLOG("Logging out of memory fatal error for %s", ToStr(hr).c_str());
      m_FatalError = ResultCode::OutOfMemory;
    }
  }
  else
  {
    RDCLOG("Ignoring return code %s", ToStr(hr).c_str());
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_SetShaderDebugPath(SerialiserType &ser,
                                                       ID3D12DeviceChild *pResource, const char *Path)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Path);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    RDCERR("SetDebugInfoPath doesn't work as-is because it can't specify a shader specific path");
  }

  return true;
}

template bool WrappedID3D12Device::Serialise_SetShaderDebugPath(ReadSerialiser &ser,
                                                                ID3D12DeviceChild *pResource,
                                                                const char *Path);
template bool WrappedID3D12Device::Serialise_SetShaderDebugPath(WriteSerialiser &ser,
                                                                ID3D12DeviceChild *pResource,
                                                                const char *Path);

HRESULT WrappedID3D12Device::SetShaderDebugPath(ID3D12DeviceChild *pResource, const char *Path)
{
  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = GetRecord(pResource);

    if(record == NULL)
    {
      RDCERR("Setting shader debug path on object %p of type %d that has no resource record.",
             pResource, IdentifyTypeByPtr(pResource));
      return E_INVALIDARG;
    }

    {
      WriteSerialiser &ser = GetThreadSerialiser();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetShaderDebugPath);
      Serialise_SetShaderDebugPath(ser, pResource, Path);
      record->AddChunk(scope.Get());
    }
  }

  return S_OK;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_SetName(SerialiserType &ser, ID3D12DeviceChild *pResource,
                                            const char *Name)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Name);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ResourceId origId = GetResourceManager()->GetOriginalID(GetResID(pResource));

    ResourceDescription &descr = GetReplay()->GetResourceDesc(origId);
    if(Name && Name[0])
    {
      descr.SetCustomName(Name);
      pResource->SetName(StringFormat::UTF82Wide(Name).c_str());
    }
    AddResourceCurChunk(descr);
  }

  return true;
}

void WrappedID3D12Device::SetName(ID3D12DeviceChild *pResource, const char *Name)
{
  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = GetRecord(pResource);

    if(WrappedID3D12CommandQueue::IsAlloc(pResource))
      record = ((WrappedID3D12CommandQueue *)pResource)->GetCreationRecord();
    if(WrappedID3D12GraphicsCommandList::IsAlloc(pResource))
      record = ((WrappedID3D12GraphicsCommandList *)pResource)->GetCreationRecord();
    if(record == NULL)
      record = m_DeviceRecord;

    {
      WriteSerialiser &ser = GetThreadSerialiser();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetName);

      Serialise_SetName(ser, pResource, Name);

      // don't serialise many SetName chunks to the object record, but we can't afford to drop any.
      if(record != m_DeviceRecord)
      {
        record->LockChunks();
        while(record->HasChunks())
        {
          Chunk *end = record->GetLastChunk();

          if(end->GetChunkType<D3D12Chunk>() == D3D12Chunk::SetName)
          {
            end->Delete();
            record->PopChunk();
            continue;
          }

          break;
        }
        record->UnlockChunks();
      }

      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateAS(SerialiserType &ser, ID3D12Resource *pResource,
                                             UINT64 resourceOffset, UINT64 byteSize,
                                             D3D12AccelerationStructure *as)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(resourceOffset);
  SERIALISE_ELEMENT(byteSize);
  SERIALISE_ELEMENT_LOCAL(asId, as->GetResourceID());

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    WrappedID3D12Resource *asbWrappedResource = (WrappedID3D12Resource *)pResource;
    D3D12AccelerationStructure *accStructAtOffset = NULL;
    if(asbWrappedResource->CreateAccStruct(resourceOffset, byteSize, &accStructAtOffset))
    {
      GetResourceManager()->AddLiveResource(asId, accStructAtOffset);

      AddResource(asId, ResourceType::AccelerationStructure, "Acceleration Structure");
      // ignored if there's no heap
      DerivedResource(pResource, asId);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Couldn't recreate acceleration structure object");
      return false;
    }
  }

  return true;
}

template bool WrappedID3D12Device::Serialise_CreateAS(ReadSerialiser &ser, ID3D12Resource *pResource,
                                                      UINT64 resourceOffset, UINT64 byteSize,
                                                      D3D12AccelerationStructure *as);
template bool WrappedID3D12Device::Serialise_CreateAS(WriteSerialiser &ser, ID3D12Resource *pResource,
                                                      UINT64 resourceOffset, UINT64 byteSize,
                                                      D3D12AccelerationStructure *as);

void WrappedID3D12Device::CreateAS(ID3D12Resource *pResource, UINT64 resourceOffset,
                                   UINT64 byteSize, D3D12AccelerationStructure *as)
{
  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = as->GetResourceRecord();

    m_HaveSeenASBuild = true;

    {
      WriteSerialiser &ser = GetThreadSerialiser();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::CreateAS);
      Serialise_CreateAS(ser, pResource, resourceOffset, byteSize, as);
      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_SetShaderExtUAV(SerialiserType &ser, GPUVendor vendor,
                                                    uint32_t reg, uint32_t space, bool global)
{
  SERIALISE_ELEMENT(vendor);
  SERIALISE_ELEMENT(reg);
  SERIALISE_ELEMENT(space);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_VendorEXT = vendor;
    m_GlobalEXTUAV = reg;
    if(vendor == GPUVendor::nVidia)
    {
      if(!m_ReplayNVAPI)
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIUnsupported,
                         "This capture uses nvapi extensions but it failed to initialise.");
        return false;
      }
      m_ReplayNVAPI->SetShaderExtUAV(space, reg, true);
    }
    else if(vendor == GPUVendor::AMD || vendor == GPUVendor::Samsung)
    {
      m_GlobalEXTUAVSpace = space;
      // do nothing, it was configured at device create time. This is purely informational
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIUnsupported,
                       "This capture uses %s extensions which are not supported.",
                       ToStr(vendor).c_str());
      return false;
    }
  }

  return true;
}

void WrappedID3D12Device::SetShaderExtUAV(GPUVendor vendor, uint32_t reg, uint32_t space, bool global)
{
  // just overwrite, we don't expect to switch back and forth on a given device.
  m_VendorEXT = vendor;
  if(global)
  {
    SCOPED_LOCK(m_EXTUAVLock);
    m_GlobalEXTUAV = reg;
    m_GlobalEXTUAVSpace = space;
    m_InitParams.VendorUAV = reg;
    m_InitParams.VendorUAVSpace = space;
  }
  else
  {
    if(m_ThreadLocalEXTUAVSlot == ~0ULL)
      m_ThreadLocalEXTUAVSlot = Threading::AllocateTLSSlot();

    uint64_t packedVal = 0U;

// on 64-bit we can store both values in the pointer fine.
#if ENABLED(RDOC_X64)
    packedVal |= space;
    packedVal <<= 32U;
    packedVal |= reg;
#else
    // no-one should be writing D3D12 applications in 32-bit, but if they are we assume that the
    // space and register are less than 16-bit. We error if this would truncate.

    // AMD's space is too big, so we steal 0xFFFF for that space
    if(reg > 0xFFFFU || space >= 0xFFFFU)
    {
      RDCERR("Register %u space %u is too large! Capture this as an 64-bit application", reg, space);

      space = RDCMIN(0xFFFEU, space);
      reg = RDCMIN(0xFFFFU, space);
    }

    if(space == 2147420894U)
      space = 0xFFFFU;

    packedVal |= space;
    packedVal <<= 16U;
    packedVal |= reg;
#endif

    Threading::SetTLSValue(m_ThreadLocalEXTUAVSlot, (void *)(uintptr_t)packedVal);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, SetShaderExtUAV, GPUVendor vendor,
                                uint32_t reg, uint32_t space, bool global);

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_SetPipelineStackSize(SerialiserType &ser,
                                                         ID3D12StateObject *pStateObject,
                                                         UINT64 StackSize)
{
  SERIALISE_ELEMENT(pStateObject);
  SERIALISE_ELEMENT(StackSize);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pStateObject)
  {
    ID3D12StateObjectProperties *properties = NULL;
    pStateObject->QueryInterface(__uuidof(ID3D12StateObjectProperties), (void **)&properties);

    properties->SetPipelineStackSize(StackSize);

    SAFE_RELEASE(properties);
  }

  return true;
}

void WrappedID3D12Device::SetPipelineStackSize(ID3D12StateObject *pStateObject, UINT64 StackSize)
{
  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = GetRecord(pStateObject);

    {
      WriteSerialiser &ser = GetThreadSerialiser();
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::StateObject_SetPipelineStackSize);
      Serialise_SetPipelineStackSize(ser, pStateObject, StackSize);
      record->AddChunk(scope.Get());
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, SetPipelineStackSize,
                                ID3D12StateObject *pStateObject, UINT64 StackSize);

void WrappedID3D12Device::SetShaderExt(GPUVendor vendor)
{
  // just overwrite, we don't expect to switch back and forth on a given device.
  m_VendorEXT = vendor;
}

void WrappedID3D12Device::GetShaderExtUAV(uint32_t &reg, uint32_t &space)
{
  if(m_ThreadLocalEXTUAVSlot != ~0ULL)
  {
    // see just above in SetShaderExtUAV for the packing
    uint64_t threadVal = (uint64_t)(uintptr_t)Threading::GetTLSValue(m_ThreadLocalEXTUAVSlot);

#if ENABLED(RDOC_X64)
    uint32_t threadReg = (threadVal & 0xFFFFFFFFU);
    uint32_t threadSpace = ((threadVal >> 32U) & 0xFFFFFFFFU);
#else
    uint32_t threadReg = (threadVal & 0xFFFFU);
    uint32_t threadSpace = ((threadVal >> 16U) & 0xFFFFU);

    if(threadSpace == 0xFFFFU)
      threadSpace = 2147420894U;
#endif

    if(threadReg != ~0U)
    {
      reg = threadReg;
      space = threadSpace;
      return;
    }
  }

  SCOPED_LOCK(m_EXTUAVLock);
  reg = m_GlobalEXTUAV;
  space = m_GlobalEXTUAVSpace;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Device::QueryVideoMemoryInfo(
    UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
    _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
{
  return m_pDownlevel->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}

FrameRefType WrappedID3D12Device::BindlessRefTypeForRes(ID3D12Resource *wrapped)
{
  // if the resource could be used in any mutable way, assume it's written. Otherwise assume it's
  // read-only (if it's modified some other way like with a copy etc, this will naturally become a
  // read-before-write)
  return (wrapped->GetDesc().Flags &
          (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
           D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) != 0
             ? eFrameRef_ReadBeforeWrite
             : eFrameRef_Read;
}

byte *WrappedID3D12Device::GetTempMemory(size_t s)
{
  TempMem *mem = (TempMem *)Threading::GetTLSValue(tempMemoryTLSSlot);
  if(mem && mem->size >= s)
    return mem->memory;

  // alloc or grow alloc
  TempMem *newmem = mem;

  if(!newmem)
    newmem = new TempMem();

  // free old memory, don't need to keep contents
  if(newmem->memory)
    delete[] newmem->memory;

  // alloc new memory
  newmem->size = s;
  newmem->memory = new byte[s];

  Threading::SetTLSValue(tempMemoryTLSSlot, (void *)newmem);

  // if this is entirely new, save it for deletion on shutdown
  if(!mem)
  {
    SCOPED_LOCK(m_ThreadTempMemLock);
    m_ThreadTempMem.push_back(newmem);
  }

  return newmem->memory;
}

WriteSerialiser &WrappedID3D12Device::GetThreadSerialiser()
{
  WriteSerialiser *ser = (WriteSerialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
  if(ser)
    return *ser;

  // slow path, but rare

  ser = new WriteSerialiser(new StreamWriter(1024), Ownership::Stream);

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());
  ser->SetVersion(D3D12InitParams::CurrentVersion);

  Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

  {
    SCOPED_LOCK(m_ThreadSerialisersLock);
    m_ThreadSerialisers.push_back(ser);
  }

  return *ser;
}

D3D12_CPU_DESCRIPTOR_HANDLE WrappedID3D12Device::AllocRTV()
{
  if(m_FreeRTVs.empty())
  {
    RDCERR("Not enough free RTVs for swapchain overlays!");
    return D3D12_CPU_DESCRIPTOR_HANDLE();
  }

  D3D12_CPU_DESCRIPTOR_HANDLE ret = m_FreeRTVs.back();
  m_FreeRTVs.pop_back();

  m_UsedRTVs.push_back(ret);

  return ret;
}

void WrappedID3D12Device::FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if(handle.ptr == 0)
    return;

  int32_t idx = m_UsedRTVs.indexOf(handle);

  if(idx >= 0)
  {
    m_UsedRTVs.erase(idx);
    m_FreeRTVs.push_back(handle);
  }
  else
  {
    RDCERR("Unknown RTV %zu being freed", handle.ptr);
  }
}

void QueueReadbackData::Resize(uint64_t size)
{
  size = AlignUp(size, 4096ULL);

  if(readbackSize >= size && size != 0)
    return;

  if(readbackBuf)
  {
    Unwrap(readbackBuf)->Unmap(0, NULL);
    SAFE_RELEASE(readbackBuf);
    readbackMapped = NULL;
  }

  readbackSize = size;

  if(size == 0)
    return;

  RDCLOG("Resizing GPU readback window to %llu", size);

  D3D12_RESOURCE_DESC readbackDesc;
  readbackDesc.Alignment = 0;
  readbackDesc.DepthOrArraySize = 1;
  readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
  readbackDesc.Height = 1;
  readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  readbackDesc.MipLevels = 1;
  readbackDesc.SampleDesc.Count = 1;
  readbackDesc.SampleDesc.Quality = 0;
  readbackDesc.Width = size;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                  D3D12_RESOURCE_STATE_COPY_DEST, NULL, __uuidof(ID3D12Resource),
                                  (void **)&readbackBuf);
  // don't intercept the map
  Unwrap(readbackBuf)->Map(0, NULL, (void **)&readbackMapped);
}

void WrappedID3D12Device::CreateInternalResources()
{
  if(IsReplayMode(m_State) && m_DriverInfo.vendor == GPUVendor::AMD)
  {
    // Initialise AMD extension, if possible
    HMODULE mod = GetModuleHandleA("amdxc64.dll");

    m_pAMDExtObject = NULL;

    if(mod)
    {
      PFNAmdExtD3DCreateInterface pAmdExtD3dCreateFunc =
          (PFNAmdExtD3DCreateInterface)GetProcAddress(mod, "AmdExtD3DCreateInterface");

      if(pAmdExtD3dCreateFunc != NULL)
      {
        // Initialize extension object
        pAmdExtD3dCreateFunc(m_pDevice, __uuidof(IAmdExtD3DFactory), (void **)&m_pAMDExtObject);
      }
    }
  }

  GetResourceManager()->GetRaytracingResourceAndUtilHandler()->CreateInternalResources();

  // we don't want replay-only shaders added in WrappedID3D12Shader to pollute the list of resources
  WrappedID3D12Shader::InternalResources(true);

  {
    m_QueueReadbackData.device = this;
    m_QueueReadbackData.Resize(4 * 1024 * 1024);
    InternalRef();

    {
      D3D12_COMMAND_QUEUE_DESC desc = {};
      desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
      // make this queue as unwrapped so that it doesn't get included in captures
      GetReal()->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue),
                                    (void **)&m_QueueReadbackData.unwrappedQueue);
      InternalRef();
      m_QueueReadbackData.unwrappedQueue->SetName(L"m_QueueReadbackData.queue");
      CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, __uuidof(ID3D12CommandAllocator),
                             (void **)&m_QueueReadbackData.alloc);
      m_QueueReadbackData.alloc->SetName(L"m_QueueReadbackData.alloc");
      InternalRef();
      CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_QueueReadbackData.alloc, NULL,
                        __uuidof(ID3D12GraphicsCommandList), (void **)&m_QueueReadbackData.list);
      InternalRef();
      m_QueueReadbackData.list->Close();
      CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                  (void **)&m_QueueReadbackData.fence);
      m_QueueReadbackData.fence->SetName(L"m_QueueReadbackData.fence");
      InternalRef();
    }
  }

  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_Alloc);
  InternalRef();
  CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_GPUSyncFence);
  m_GPUSyncFence->SetName(L"m_GPUSyncFence");
  InternalRef();
  m_GPUSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  GetResourceManager()->SetInternalResource(m_Alloc);
  GetResourceManager()->SetInternalResource(m_GPUSyncFence);

  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_DataUploadAlloc);
  InternalRef();

  GetResourceManager()->SetInternalResource(m_DataUploadAlloc);

  for(size_t i = 0; i < ARRAY_COUNT(m_DataUploadList); i++)
  {
    CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DataUploadAlloc, NULL,
                      __uuidof(ID3D12GraphicsCommandList), (void **)&m_DataUploadList[i]);
    InternalRef();

    m_DataUploadList[i]->Close();
  }
  m_CurDataUpload = 0;

  D3D12_DESCRIPTOR_HEAP_DESC desc;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  desc.NodeMask = 1;
  desc.NumDescriptors = 1024;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

  CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), (void **)&m_RTVHeap);
  InternalRef();

  GetResourceManager()->SetInternalResource(m_RTVHeap);

  D3D12_CPU_DESCRIPTOR_HANDLE handle;

  if(m_RTVHeap)
  {
    handle = m_RTVHeap->GetCPUDescriptorHandleForHeapStart();

    UINT step = GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for(size_t i = 0; i < 1024; i++)
    {
      m_FreeRTVs.push_back(handle);

      handle.ptr += step;
    }
  }
  else
  {
    RDCERR("Failed to create RTV heap");
  }

  m_GPUSyncCounter = 0;

  if(IsReplayMode(m_State))
    GetShaderCache()->SetDevConfiguration(m_Replay->GetDevConfiguration());

  if(m_TextRenderer == NULL)
    m_TextRenderer = new D3D12TextRenderer(this);

  m_Replay->CreateResources();

  WrappedID3D12Shader::InternalResources(false);
  GetResourceManager()->GetRaytracingResourceAndUtilHandler()->InitInternalResources();
}

void WrappedID3D12Device::DestroyInternalResources()
{
  if(m_GPUSyncHandle == NULL)
    return;

  SAFE_RELEASE(m_pAMDExtObject);

  ExecuteLists();
  FlushLists(true);

  SAFE_RELEASE(m_RTVHeap);

  SAFE_DELETE(m_TextRenderer);
  SAFE_DELETE(m_ShaderCache);

  for(size_t i = 0; i < ARRAY_COUNT(m_DataUploadList); i++)
    SAFE_RELEASE(m_DataUploadList[i]);
  SAFE_RELEASE(m_DataUploadAlloc);

  SAFE_RELEASE(m_QueueReadbackData.unwrappedQueue);
  SAFE_RELEASE(m_QueueReadbackData.alloc);
  SAFE_RELEASE(m_QueueReadbackData.list);
  SAFE_RELEASE(m_QueueReadbackData.fence);
  m_QueueReadbackData.Resize(0);

  SAFE_RELEASE(m_Alloc);
  SAFE_RELEASE(m_GPUSyncFence);
  CloseHandle(m_GPUSyncHandle);
}

void WrappedID3D12Device::DataUploadSync()
{
  if(m_CurDataUpload >= 0)
  {
    GPUSync();
    m_CurDataUpload = 0;
  }
}

void WrappedID3D12Device::GPUSync(ID3D12CommandQueue *queue, ID3D12Fence *fence)
{
  m_GPUSyncCounter++;

  if(HasFatalError())
    return;

  if(queue == NULL)
    queue = GetQueue();

  if(fence == NULL)
    fence = m_GPUSyncFence;

  HRESULT hr = queue->Signal(fence, m_GPUSyncCounter);
  CheckHRESULT(hr);
  RDCASSERTEQUAL(hr, S_OK);

  fence->SetEventOnCompletion(m_GPUSyncCounter, m_GPUSyncHandle);
  WaitForSingleObject(m_GPUSyncHandle, 10000);

  hr = m_pDevice->GetDeviceRemovedReason();
  CheckHRESULT(hr);
  RDCASSERTEQUAL(hr, S_OK);
}

void WrappedID3D12Device::GPUSyncAllQueues()
{
  if(m_GPUSynced)
    return;

  for(size_t i = 0; i < m_QueueFences.size(); i++)
    GPUSync(m_Queues[i], m_QueueFences[i]);

  m_GPUSynced = true;
}

ID3D12GraphicsCommandListX *WrappedID3D12Device::GetNewList()
{
  ID3D12GraphicsCommandListX *ret = NULL;

  m_GPUSynced = false;

  if(!m_InternalCmds.freecmds.empty())
  {
    ret = m_InternalCmds.freecmds.back();
    m_InternalCmds.freecmds.pop_back();

    ret->Reset(m_Alloc, NULL);
  }
  else
  {
    ID3D12GraphicsCommandList *list = NULL;
    HRESULT hr = CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Alloc, NULL,
                                   __uuidof(ID3D12GraphicsCommandList), (void **)&list);
    InternalRef();

    // safe to upcast because this is a wrapped object.
    ret = (ID3D12GraphicsCommandListX *)list;

    RDCASSERTEQUAL(hr, S_OK);
    CheckHRESULT(hr);

    if(ret == NULL)
      return NULL;

    if(IsReplayMode(m_State))
    {
      GetResourceManager()->AddLiveResource(GetResID(ret), ret);
      // add a reference here so that when we release our internal resources on destruction we don't
      // free this too soon before the resource manager can. We still want to have it tracked as a
      // resource in the manager though.
      ret->AddRef();
    }
  }

  m_InternalCmds.pendingcmds.push_back(ret);

  return ret;
}

ID3D12GraphicsCommandListX *WrappedID3D12Device::GetInitialStateList()
{
  if(initStateCurBatch >= initialStateMaxBatch)
  {
    CloseInitialStateList();
  }

  if(initStateCurList == NULL)
  {
    initStateCurList = GetNewList();

    if(IsReplayMode(m_State))
    {
      D3D12MarkerRegion::Begin(initStateCurList,
                               "!!!!RenderDoc Internal: ApplyInitialContents batched list");
    }
  }

  initStateCurBatch++;

  return initStateCurList;
}

void WrappedID3D12Device::CloseInitialStateList()
{
  D3D12MarkerRegion::End(initStateCurList);
  initStateCurList->Close();
  initStateCurList = NULL;
  initStateCurBatch = 0;
}

void WrappedID3D12Device::ExecuteList(ID3D12GraphicsCommandListX *list,
                                      WrappedID3D12CommandQueue *queue, bool InFrameCaptureBoundary)
{
  if(HasFatalError())
    return;

  if(queue == NULL)
    queue = GetQueue();

  ID3D12CommandList *l = list;
  queue->ExecuteCommandListsInternal(1, &l, InFrameCaptureBoundary, false);

  MarkListExecuted(list);
}

void WrappedID3D12Device::MarkListExecuted(ID3D12GraphicsCommandListX *list)
{
  m_InternalCmds.pendingcmds.removeOne(list);
  m_InternalCmds.submittedcmds.push_back(list);
}

void WrappedID3D12Device::ExecuteLists(WrappedID3D12CommandQueue *queue, bool InFrameCaptureBoundary)
{
  if(HasFatalError())
    return;

  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  rdcarray<ID3D12CommandList *> cmds;
  cmds.resize(m_InternalCmds.pendingcmds.size());
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = m_InternalCmds.pendingcmds[i];

  if(queue == NULL)
    queue = GetQueue();

  for(size_t i = 0; i < cmds.size(); i += executeListsMaxSize)
  {
    UINT cmdCount = RDCMIN(executeListsMaxSize, (UINT)(cmds.size() - i));
    queue->ExecuteCommandListsInternal(cmdCount, &cmds[i], InFrameCaptureBoundary, false);
  }

  m_InternalCmds.submittedcmds.append(m_InternalCmds.pendingcmds);
  m_InternalCmds.pendingcmds.clear();
}

void WrappedID3D12Device::FlushLists(bool forceSync, ID3D12CommandQueue *queue)
{
  if(HasFatalError())
    return;

  if(!m_InternalCmds.submittedcmds.empty() || forceSync)
  {
    GPUSync(queue);

    if(!m_InternalCmds.submittedcmds.empty())
      m_InternalCmds.freecmds.append(m_InternalCmds.submittedcmds);
    m_InternalCmds.submittedcmds.clear();

    if(m_InternalCmds.pendingcmds.empty())
      m_Alloc->Reset();
  }
}

const ActionDescription *WrappedID3D12Device::GetAction(uint32_t eventId)
{
  if(eventId >= m_Actions.size())
    return NULL;

  return m_Actions[eventId];
}

bool WrappedID3D12Device::ProcessChunk(ReadSerialiser &ser, D3D12Chunk context)
{
  switch(context)
  {
    case D3D12Chunk::Device_CreateCommandQueue:
      return Serialise_CreateCommandQueue(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateCommandAllocator:
      return Serialise_CreateCommandAllocator(ser, D3D12_COMMAND_LIST_TYPE_DIRECT, IID(), NULL);
    case D3D12Chunk::Device_CreateCommandList:
      return Serialise_CreateCommandList(ser, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, NULL, NULL, IID(),
                                         NULL);

    case D3D12Chunk::Device_CreateGraphicsPipeline:
      return Serialise_CreateGraphicsPipelineState(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateComputePipeline:
      return Serialise_CreateComputePipelineState(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateDescriptorHeap:
      return Serialise_CreateDescriptorHeap(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateRootSignature:
      return Serialise_CreateRootSignature(ser, 0, NULL, 0, IID(), NULL);
    case D3D12Chunk::Device_CreateCommandSignature:
      return Serialise_CreateCommandSignature(ser, NULL, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateHeap: return Serialise_CreateHeap(ser, NULL, IID(), NULL); break;
    case D3D12Chunk::Device_CreateCommittedResource:
      return Serialise_CreateCommittedResource(ser, NULL, D3D12_HEAP_FLAG_NONE, NULL,
                                               D3D12_RESOURCE_STATE_COMMON, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreatePlacedResource:
      return Serialise_CreatePlacedResource(ser, NULL, 0, NULL, D3D12_RESOURCE_STATE_COMMON, NULL,
                                            IID(), NULL);
    case D3D12Chunk::Device_CreateReservedResource:
      return Serialise_CreateReservedResource(ser, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, IID(),
                                              NULL);

    case D3D12Chunk::Device_CreateQueryHeap:
      return Serialise_CreateQueryHeap(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateFence:
      return Serialise_CreateFence(ser, 0, D3D12_FENCE_FLAG_NONE, IID(), NULL);
    case D3D12Chunk::SetName: return Serialise_SetName(ser, 0x0, "");
    case D3D12Chunk::SetShaderDebugPath: return Serialise_SetShaderDebugPath(ser, NULL, NULL);
    case D3D12Chunk::CreateSwapBuffer:
      return Serialise_WrapSwapchainBuffer(ser, NULL, DXGI_FORMAT_UNKNOWN, 0, NULL);
    case D3D12Chunk::Device_CreatePipelineState:
      return Serialise_CreatePipelineState(ser, NULL, IID(), NULL);
    // these functions are serialised as-if they are a real heap.
    case D3D12Chunk::Device_CreateHeapFromAddress:
    case D3D12Chunk::Device_CreateHeapFromFileMapping:
      return Serialise_CreateHeap(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_OpenSharedHandle:
      return Serialise_OpenSharedHandle(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateCommandList1:
      return Serialise_CreateCommandList1(ser, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          D3D12_COMMAND_LIST_FLAG_NONE, IID(), NULL);
    case D3D12Chunk::Device_CreateCommittedResource1:
      return Serialise_CreateCommittedResource1(ser, NULL, D3D12_HEAP_FLAG_NONE, NULL,
                                                D3D12_RESOURCE_STATE_COMMON, NULL, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateHeap1: return Serialise_CreateHeap1(ser, NULL, NULL, IID(), NULL);
    case D3D12Chunk::Device_ExternalDXGIResource:
      return Serialise_OpenSharedHandle(ser, NULL, IID(), NULL);
    case D3D12Chunk::CompatDevice_CreateSharedResource:
    case D3D12Chunk::CompatDevice_CreateSharedHeap:
      return Serialise_OpenSharedHandle(ser, NULL, IID(), NULL);
    case D3D12Chunk::SetShaderExtUAV:
      return Serialise_SetShaderExtUAV(ser, GPUVendor::Unknown, 0, 0, true);
    case D3D12Chunk::Device_CreateCommittedResource2:
      return Serialise_CreateCommittedResource2(ser, NULL, D3D12_HEAP_FLAG_NONE, NULL,
                                                D3D12_RESOURCE_STATE_COMMON, NULL, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreatePlacedResource1:
      return Serialise_CreatePlacedResource1(ser, NULL, 0, NULL, D3D12_RESOURCE_STATE_COMMON, NULL,
                                             IID(), NULL);
    case D3D12Chunk::Device_CreateCommandQueue1:
      return Serialise_CreateCommandQueue1(ser, NULL, IID(), IID(), NULL);
    case D3D12Chunk::Device_CreateCommittedResource3:
      return Serialise_CreateCommittedResource3(ser, NULL, D3D12_HEAP_FLAG_NONE, NULL,
                                                D3D12_BARRIER_LAYOUT_COMMON, NULL, NULL, 0, NULL,
                                                IID(), NULL);
    case D3D12Chunk::Device_CreatePlacedResource2:
      return Serialise_CreatePlacedResource2(ser, NULL, 0, NULL, D3D12_BARRIER_LAYOUT_COMMON, NULL,
                                             0, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateReservedResource1:
      return Serialise_CreateReservedResource1(ser, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, NULL,
                                               IID(), NULL);
    case D3D12Chunk::Device_CreateReservedResource2:
      return Serialise_CreateReservedResource2(ser, NULL, D3D12_BARRIER_LAYOUT_COMMON, NULL, NULL,
                                               0, NULL, IID(), NULL);
    case D3D12Chunk::Device_CreateStateObject:
      return Serialise_CreateStateObject(ser, NULL, IID(), NULL);
    case D3D12Chunk::Device_AddToStateObject:
      return Serialise_AddToStateObject(ser, NULL, NULL, IID(), NULL);
    case D3D12Chunk::CreateAS: return Serialise_CreateAS(ser, NULL, 0, {}, NULL);
    case D3D12Chunk::StateObject_SetPipelineStackSize:
      return Serialise_SetPipelineStackSize(ser, NULL, 0);

    // in order to get a warning if we miss a case, we explicitly handle the list/queue chunks here.
    // If we actually encounter one it's an error (we should hit CaptureBegin first and switch to
    // D3D12CommandData::ProcessChunk)
    case D3D12Chunk::Device_CreateConstantBufferView:
    case D3D12Chunk::Device_CreateShaderResourceView:
    case D3D12Chunk::Device_CreateUnorderedAccessView:
    case D3D12Chunk::Device_CreateRenderTargetView:
    case D3D12Chunk::Device_CreateDepthStencilView:
    case D3D12Chunk::Device_CreateSampler:
    case D3D12Chunk::Device_CopyDescriptors:
    case D3D12Chunk::Device_CopyDescriptorsSimple:
    case D3D12Chunk::Queue_ExecuteCommandLists:
    case D3D12Chunk::Queue_Signal:
    case D3D12Chunk::Queue_Wait:
    case D3D12Chunk::Queue_UpdateTileMappings:
    case D3D12Chunk::Queue_CopyTileMappings:
    case D3D12Chunk::Queue_BeginEvent:
    case D3D12Chunk::Queue_SetMarker:
    case D3D12Chunk::Queue_EndEvent:
    case D3D12Chunk::List_Close:
    case D3D12Chunk::List_Reset:
    case D3D12Chunk::List_ResourceBarrier:
    case D3D12Chunk::List_BeginQuery:
    case D3D12Chunk::List_EndQuery:
    case D3D12Chunk::List_ResolveQueryData:
    case D3D12Chunk::List_SetPredication:
    case D3D12Chunk::List_DrawIndexedInstanced:
    case D3D12Chunk::List_DrawInstanced:
    case D3D12Chunk::List_Dispatch:
    case D3D12Chunk::List_ExecuteIndirect:
    case D3D12Chunk::List_ExecuteBundle:
    case D3D12Chunk::List_CopyBufferRegion:
    case D3D12Chunk::List_CopyTextureRegion:
    case D3D12Chunk::List_CopyResource:
    case D3D12Chunk::List_ResolveSubresource:
    case D3D12Chunk::List_ClearRenderTargetView:
    case D3D12Chunk::List_ClearDepthStencilView:
    case D3D12Chunk::List_ClearUnorderedAccessViewUint:
    case D3D12Chunk::List_ClearUnorderedAccessViewFloat:
    case D3D12Chunk::List_DiscardResource:
    case D3D12Chunk::List_IASetPrimitiveTopology:
    case D3D12Chunk::List_IASetIndexBuffer:
    case D3D12Chunk::List_IASetVertexBuffers:
    case D3D12Chunk::List_SOSetTargets:
    case D3D12Chunk::List_RSSetViewports:
    case D3D12Chunk::List_RSSetScissorRects:
    case D3D12Chunk::List_SetPipelineState:
    case D3D12Chunk::List_SetDescriptorHeaps:
    case D3D12Chunk::List_OMSetRenderTargets:
    case D3D12Chunk::List_OMSetStencilRef:
    case D3D12Chunk::List_OMSetBlendFactor:
    case D3D12Chunk::List_SetGraphicsRootDescriptorTable:
    case D3D12Chunk::List_SetGraphicsRootSignature:
    case D3D12Chunk::List_SetGraphicsRoot32BitConstant:
    case D3D12Chunk::List_SetGraphicsRoot32BitConstants:
    case D3D12Chunk::List_SetGraphicsRootConstantBufferView:
    case D3D12Chunk::List_SetGraphicsRootShaderResourceView:
    case D3D12Chunk::List_SetGraphicsRootUnorderedAccessView:
    case D3D12Chunk::List_SetComputeRootDescriptorTable:
    case D3D12Chunk::List_SetComputeRootSignature:
    case D3D12Chunk::List_SetComputeRoot32BitConstant:
    case D3D12Chunk::List_SetComputeRoot32BitConstants:
    case D3D12Chunk::List_SetComputeRootConstantBufferView:
    case D3D12Chunk::List_SetComputeRootShaderResourceView:
    case D3D12Chunk::List_SetComputeRootUnorderedAccessView:
    case D3D12Chunk::List_CopyTiles:
    case D3D12Chunk::List_AtomicCopyBufferUINT:
    case D3D12Chunk::List_AtomicCopyBufferUINT64:
    case D3D12Chunk::List_OMSetDepthBounds:
    case D3D12Chunk::List_ResolveSubresourceRegion:
    case D3D12Chunk::List_SetSamplePositions:
    case D3D12Chunk::List_SetViewInstanceMask:
    case D3D12Chunk::List_WriteBufferImmediate:
    case D3D12Chunk::List_BeginRenderPass:
    case D3D12Chunk::List_EndRenderPass:
    case D3D12Chunk::List_RSSetShadingRate:
    case D3D12Chunk::List_RSSetShadingRateImage:
    case D3D12Chunk::PushMarker:
    case D3D12Chunk::PopMarker:
    case D3D12Chunk::SetMarker:
    case D3D12Chunk::Resource_Unmap:
    case D3D12Chunk::Resource_WriteToSubresource:
    case D3D12Chunk::List_IndirectSubCommand:
    case D3D12Chunk::Swapchain_Present:
    case D3D12Chunk::List_ClearState:
    case D3D12Chunk::CoherentMapWrite:
    case D3D12Chunk::Device_CreateSampler2:
    case D3D12Chunk::List_OMSetFrontAndBackStencilRef:
    case D3D12Chunk::List_RSSetDepthBias:
    case D3D12Chunk::List_IASetIndexBufferStripCutValue:
    case D3D12Chunk::List_Barrier:
    case D3D12Chunk::List_DispatchMesh:
    case D3D12Chunk::List_BuildRaytracingAccelerationStructure:
    case D3D12Chunk::List_CopyRaytracingAccelerationStructure:
    case D3D12Chunk::List_EmitRaytracingAccelerationStructurePostbuildInfo:
    case D3D12Chunk::List_DispatchRays:
    case D3D12Chunk::List_SetPipelineState1:
      RDCERR("Unexpected chunk while processing initialisation: %s", ToStr(context).c_str());
      return false;

    // no explicit default so that we have compiler warnings if a chunk isn't explicitly handled.
    case D3D12Chunk::Max: break;
  }

  {
    SystemChunk system = (SystemChunk)context;
    if(system == SystemChunk::DriverInit)
    {
      D3D12InitParams InitParams;
      SERIALISE_ELEMENT(InitParams);

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContentsList)
    {
      GetResourceManager()->CreateInitialContents(ser);

      if(initStateCurList)
      {
        CloseInitialStateList();

        ExecuteLists(NULL, true);
        FlushLists();
      }

      SERIALISE_CHECK_READ_ERRORS();
    }
    else if(system == SystemChunk::InitialContents)
    {
      return GetResourceManager()->Serialise_InitialState(ser, ResourceId(), NULL, NULL);
    }
    else if(system == SystemChunk::CaptureScope)
    {
      return Serialise_CaptureScope(ser);
    }
    else if(system < SystemChunk::FirstDriverChunk)
    {
      RDCERR("Unexpected system chunk in capture data: %u", system);
      ser.SkipCurrentChunk();

      SERIALISE_CHECK_READ_ERRORS();
    }
    else
    {
      RDCERR("Unexpected chunk %s", ToStr(context).c_str());
      return false;
    }
  }

  return true;
}

ResourceDescription &WrappedID3D12Device::GetResourceDesc(ResourceId id)
{
  return GetReplay()->GetResourceDesc(id);
}

void WrappedID3D12Device::AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix)
{
  ResourceDescription &descr = GetReplay()->GetResourceDesc(id);

  uint64_t num;
  memcpy(&num, &id, sizeof(uint64_t));
  descr.name = defaultNamePrefix + (" " + ToStr(num));
  descr.autogeneratedName = true;
  descr.type = type;
  AddResourceCurChunk(descr);
}

void WrappedID3D12Device::DerivedResource(ID3D12DeviceChild *parent, ResourceId child)
{
  if(!parent)
    return;

  ResourceId parentId = GetResourceManager()->GetOriginalID(GetResID(parent));

  DerivedResource(parentId, child);
}

void WrappedID3D12Device::DerivedResource(ResourceId parent, ResourceId child)
{
  if(GetReplay()->GetResourceDesc(parent).derivedResources.contains(child))
    return;

  GetReplay()->GetResourceDesc(parent).derivedResources.push_back(child);
  GetReplay()->GetResourceDesc(child).parentResources.push_back(parent);
}

void WrappedID3D12Device::AddResourceCurChunk(ResourceDescription &descr)
{
  descr.initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 1);
}

void WrappedID3D12Device::AddResourceCurChunk(ResourceId id)
{
  AddResourceCurChunk(GetReplay()->GetResourceDesc(id));
}

RDResult WrappedID3D12Device::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(IsStructuredExporting(m_State))
  {
    // when structured exporting don't do any timebase conversion
    m_TimeBase = 0;
    m_TimeFrequency = 1.0;
  }
  else
  {
    m_TimeBase = rdc->GetTimestampBase();
    m_TimeFrequency = rdc->GetTimestampFrequency();
  }

  if(reader->IsErrored())
  {
    RDResult result = reader->GetError();
    delete reader;
    return result;
  }

  ReadSerialiser ser(reader, Ownership::Stream);

  APIProps.DXILShaders = m_UsedDXIL = m_InitParams.usedDXIL;

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers, m_TimeBase, m_TimeFrequency);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData->version = m_StructuredFile->version = m_SectionVersion;

  ser.SetVersion(m_SectionVersion);

  int chunkIdx = 0;

  struct chunkinfo
  {
    chunkinfo() : count(0), totalsize(0), total(0.0) {}
    int count;
    uint64_t totalsize;
    double total;
  };

  std::map<D3D12Chunk, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  uint64_t frameDataSize = 0;

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offsetStart = reader->GetOffset();

    D3D12Chunk context = ser.ReadChunk<D3D12Chunk>();

    chunkIdx++;

    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    UINT64 startMessage = 0;

    // store how many messages existed before this chunk. If it fails we'll only print messages
    // related to/generated by it
    if(m_pInfoQueue)
      startMessage = m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(reader->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
    {
      rdcstr extra;

      if(m_pInfoQueue)
      {
        extra += "\n";

        for(UINT64 i = startMessage;
            i < m_pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter(); i++)
        {
          SIZE_T len = 0;
          m_pInfoQueue->GetMessage(i, NULL, &len);

          char *msgbuf = new char[len];
          D3D12_MESSAGE *message = (D3D12_MESSAGE *)msgbuf;

          m_pInfoQueue->GetMessage(i, message, &len);

          extra += "\n";
          extra += message->pDescription;

          delete[] msgbuf;
        }
      }
      else
      {
        extra +=
            "\n\nMore debugging information may be available by enabling API validation on replay "
            "via `File` -> `Open Capture with Options`";
      }

      if(HasFatalError())
      {
        m_FatalError.message = rdcstr(m_FatalError.message) + extra;
        return m_FatalError;
      }

      m_FailedReplayResult.message = rdcstr(m_FailedReplayResult.message) + extra;
      return m_FailedReplayResult;
    }

    if(m_FatalError != ResultCode::Succeeded)
      return m_FatalError;

    uint64_t offsetEnd = reader->GetOffset();

    RenderDoc::Inst().SetProgress(LoadProgress::FileInitialRead,
                                  float(offsetEnd) / float(reader->GetSize()));

    if((SystemChunk)context == SystemChunk::CaptureScope)
    {
      GetReplay()->WriteFrameRecord().frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      if(IsStructuredExporting(m_State))
      {
        m_Queue = new WrappedID3D12CommandQueue(NULL, this, m_State);
        m_Queues.push_back(m_Queue);
      }

      m_Queue->SetFrameReader(new StreamReader(reader, frameDataSize));

      if(!IsStructuredExporting(m_State))
      {
        rdcarray<DebugMessage> savedDebugMessages;

        // save any debug messages we built up
        savedDebugMessages.swap(m_DebugMessages);

        ApplyInitialContents();

        // restore saved messages - which implicitly discards any generated while applying initial
        // contents
        savedDebugMessages.swap(m_DebugMessages);
      }

      RDResult result = m_Queue->ReplayLog(m_State, 0, 0, false);

      if(result != ResultCode::Succeeded)
        return result;
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if((SystemChunk)context == SystemChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
      break;
  }

  // steal the structured data for ourselves
  m_StructuredFile->Swap(*m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = m_StoredStructuredData;

  if(!IsStructuredExporting(m_State))
  {
    GetReplay()->WriteFrameRecord().actionList = m_Queue->GetParentAction().Bake();

    m_Queue->GetParentAction().children.clear();

    SetupActionPointers(m_Actions, GetReplay()->WriteFrameRecord().actionList);
  }

  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    for(auto it = cmd.m_ResourceUses.begin(); it != cmd.m_ResourceUses.end(); ++it)
    {
      if(m_ModResources.find(it->first) != m_ModResources.end())
        continue;

      for(const EventUsage &use : it->second)
      {
        if(use.usage == ResourceUsage::CopyDst || use.usage == ResourceUsage::Copy ||
           use.usage == ResourceUsage::ResolveDst || use.usage == ResourceUsage::Resolve ||
           use.usage == ResourceUsage::GenMips || use.usage == ResourceUsage::Clear ||
           use.usage == ResourceUsage::Discard || use.usage == ResourceUsage::CPUWrite ||
           use.usage == ResourceUsage::ColorTarget ||
           use.usage == ResourceUsage::DepthStencilTarget || use.usage == ResourceUsage::StreamOut)
        {
          m_ModResources.insert(it->first);
          break;
        }
      }
    }
  }

#if ENABLED(RDOC_DEVEL)
  for(auto it = chunkInfos.begin(); it != chunkInfos.end(); ++it)
  {
    double dcount = double(it->second.count);

    RDCDEBUG(
        "% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
        it->second.count, it->second.total, it->second.total / dcount,
        double(it->second.totalsize) / (1024.0 * 1024.0),
        double(it->second.totalsize) / (dcount * 1024.0 * 1024.0),
        GetChunkName((uint32_t)it->first).c_str(), uint32_t(it->first));
  }
#endif

  GetReplay()->WriteFrameRecord().frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.compressedFileSize =
      rdc->GetSectionProperties(sectionIdx).compressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.persistentSize = frameDataSize;
  GetReplay()->WriteFrameRecord().frameInfo.initDataSize =
      chunkInfos[(D3D12Chunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           GetReplay()->WriteFrameRecord().frameInfo.persistentSize);

  if(m_FatalError != ResultCode::Succeeded)
    return m_FatalError;

  if(m_pDevice && m_pDevice->GetDeviceRemovedReason() != S_OK)
    RETURN_ERROR_RESULT(ResultCode::DeviceLost, "Device lost during load: %s",
                        ToStr(m_pDevice->GetDeviceRemovedReason()).c_str());

  return ResultCode::Succeeded;
}

void WrappedID3D12Device::ReplayLog(uint32_t startEventID, uint32_t endEventID,
                                    ReplayLogType replayType)
{
  bool partial = true;

  m_GPUSynced = false;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;

    m_GPUSyncCounter++;

    // I'm not sure the reason for this, but the debug layer warns about being unable to resubmit
    // command lists due to the 'previous queue fence' not being ready yet, even if no fences are
    // signalled or waited. So instead we just signal a dummy fence each new 'frame'
    for(size_t i = 0; i < m_Queues.size(); i++)
      CheckHRESULT(m_Queues[i]->Signal(m_QueueFences[i], m_GPUSyncCounter));

    FlushLists(true);
    m_CurDataUpload = 0;

    // take this opportunity to reset command allocators to ensure we don't steadily leak over time.
    if(m_DataUploadAlloc)
      CheckHRESULT(m_DataUploadAlloc->Reset());

    for(ID3D12CommandAllocator *alloc : m_CommandAllocators)
      CheckHRESULT(alloc->Reset());

    if(HasFatalError())
      return;
  }

  if(!partial)
  {
    {
      D3D12MarkerRegion apply(GetQueue(), "!!!!RenderDoc Internal: ApplyInitialContents");
      ApplyInitialContents();
    }

    ExecuteLists();
    FlushLists(true);

    // clear any previous ray dispatch references
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    for(PatchedRayDispatch::Resources &r : cmd.m_RayDispatches)
    {
      SAFE_RELEASE(r.lookupBuffer);
      SAFE_RELEASE(r.patchScratchBuffer);
      SAFE_RELEASE(r.argumentBuffer);
    }
    cmd.m_RayDispatches.clear();

    if(HasFatalError())
      return;
  }

  m_State = CaptureState::ActiveReplaying;

  D3D12MarkerRegion::Set(
      GetQueue(), StringFormat::Fmt("!!!!RenderDoc Internal: RenderDoc Replay %d (%d): %u->%u",
                                    (int)replayType, (int)partial, startEventID, endEventID));

  if(!partial)
  {
    ID3D12GraphicsCommandList *beginList = GetNewList();
    if(!beginList)
      return;

    {
      rdcwstr text = StringFormat::UTF82Wide(AMDRGPControl::GetBeginMarker());
      UINT size = UINT(text.length() * sizeof(wchar_t));
      beginList->SetMarker(0, text.c_str(), size);
    }

    CheckHRESULT(beginList->Close());
    ExecuteLists();

    if(HasFatalError())
      return;
  }

  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    if(!partial)
    {
      cmd.m_Partial[D3D12CommandData::Primary].Reset();
      cmd.m_Partial[D3D12CommandData::Secondary].Reset();
      cmd.m_RenderState = D3D12RenderState();
      cmd.m_RenderState.m_ResourceManager = GetResourceManager();
      cmd.m_RenderState.m_DebugManager = m_Replay->GetDebugManager();
    }
    else
    {
      // Copy the state in case m_RenderState was modified externally for the partial replay.
      cmd.m_BakedCmdListInfo[cmd.m_Partial[D3D12CommandData::Primary].partialParent].state =
          cmd.m_RenderState;
    }

    // we'll need our own command list if we're replaying just a subsection
    // of events within a single command list record - always if it's only
    // one action, or if start event ID is > 0 we assume the outside code
    // has chosen a subsection that lies within a command list
    if(partial)
    {
      ID3D12GraphicsCommandListX *list = cmd.m_OutsideCmdList = GetNewList();

      cmd.m_BakedCmdListInfo[GetResID(cmd.m_OutsideCmdList)].barriers.clear();

      if(!list)
        return;

      cmd.m_RenderState.ApplyState(this, list);
    }

    RDResult result = ResultCode::Succeeded;

    if(replayType == eReplay_Full)
      result = m_Queue->ReplayLog(m_State, startEventID, endEventID, partial);
    else if(replayType == eReplay_WithoutDraw)
      result = m_Queue->ReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    else if(replayType == eReplay_OnlyDraw)
      result = m_Queue->ReplayLog(m_State, endEventID, endEventID, partial);
    else
      RDCFATAL("Unexpected replay type");

    RDCASSERTEQUAL(result.code, ResultCode::Succeeded);

    if(cmd.m_OutsideCmdList != NULL)
    {
      if(replayType == eReplay_OnlyDraw)
        ApplyBarriers(cmd.m_BakedCmdListInfo[GetResID(cmd.m_OutsideCmdList)].barriers);

      ID3D12GraphicsCommandList *list = cmd.m_OutsideCmdList;

      CheckHRESULT(list->Close());

      ExecuteLists();

      cmd.m_OutsideCmdList = NULL;
    }

    if(HasFatalError())
      return;

    cmd.m_RenderState =
        cmd.m_BakedCmdListInfo[cmd.m_Partial[D3D12CommandData::Primary].partialParent].state;
    cmd.m_RenderState.ResolvePendingIndirectState(this);

    if(D3D12_Debug_SingleSubmitFlushing())
    {
      FlushLists(true);

      if(HasFatalError())
        return;
    }
  }

  D3D12MarkerRegion::Set(GetQueue(), "!!!!RenderDoc Internal: Done replay");

  // ensure all UAV writes have finished before subsequent work
  ID3D12GraphicsCommandList *list = GetNewList();

  if(list)
  {
    {
      rdcwstr text = StringFormat::UTF82Wide(AMDRGPControl::GetEndMarker());
      UINT size = UINT(text.length() * sizeof(wchar_t));
      list->SetMarker(0, text.c_str(), size);
    }

    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    list->ResourceBarrier(1, &uavBarrier);

    CheckHRESULT(list->Close());

    ExecuteLists();
  }
}

void WrappedID3D12Device::ReplayDraw(ID3D12GraphicsCommandListX *cmd, const ActionDescription &action)
{
  if(action.drawIndex == 0)
  {
    if(action.flags & ActionFlags::MeshDispatch)
    {
      cmd->DispatchMesh(action.dispatchDimension[0], action.dispatchDimension[1],
                        action.dispatchDimension[2]);
    }
    else if(action.flags & ActionFlags::Indexed)
    {
      RDCASSERT(action.flags & ActionFlags::Drawcall);
      cmd->DrawIndexedInstanced(action.numIndices, action.numInstances, action.indexOffset,
                                action.baseVertex, action.instanceOffset);
    }
    else
    {
      RDCASSERT(action.flags & ActionFlags::Drawcall);
      cmd->DrawInstanced(action.numIndices, action.numInstances, action.vertexOffset,
                         action.instanceOffset);
    }
  }
  else
  {
    // TODO: support replay of draws not in callback
    D3D12CommandData *cmdData = m_Queue->GetCommandData();
    RDCASSERT(cmdData->m_IndirectData.commandSig != NULL);
    cmd->ExecuteIndirect(cmdData->m_IndirectData.commandSig, 1, cmdData->m_IndirectData.argsBuffer,
                         cmdData->m_IndirectData.argsOffset, NULL, 0);
  }
}
