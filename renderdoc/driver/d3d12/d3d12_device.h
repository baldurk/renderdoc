/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#pragma once

#include <stdint.h>
#include <map>
#include "common/threading.h"
#include "common/timing.h"
#include "common/wrapped_pool.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/ihv/amd/ags_wrapper.h"
#include "driver/ihv/nv/nvapi_wrapper.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"
#include "d3d12_manager.h"

struct IAmdExtD3DFactory;

struct D3D12InitParams
{
  D3D_FEATURE_LEVEL MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  DXGI_ADAPTER_DESC AdapterDesc = {};

  bool usedDXIL = false;

  GPUVendor VendorExtensions = GPUVendor::Unknown;
  uint32_t VendorUAV = ~0U;
  uint32_t VendorUAVSpace = ~0U;

  // check if a frame capture section version is supported
  static const uint64_t CurrentVersion = 0xA;

  static bool IsSupportedVersion(uint64_t ver);
};

DECLARE_REFLECTION_STRUCT(D3D12InitParams);

class WrappedID3D12Device;
class WrappedID3D12Resource;
class WrappedID3D12PipelineState;

class D3D12Replay;
class D3D12TextRenderer;
class D3D12ShaderCache;
class D3D12DebugManager;

// give every impression of working but do nothing.
// Just allow the user to call functions so that they don't
// have to check for E_NOINTERFACE when they expect an infoqueue to be there
struct DummyID3D12InfoQueue : public ID3D12InfoQueue
{
  WrappedID3D12Device *m_pDevice;

  DummyID3D12InfoQueue() : m_pDevice(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12InfoQueue
  virtual HRESULT STDMETHODCALLTYPE SetMessageCountLimit(UINT64 MessageCountLimit) { return S_OK; }
  virtual void STDMETHODCALLTYPE ClearStoredMessages() {}
  virtual HRESULT STDMETHODCALLTYPE GetMessage(UINT64 MessageIndex, D3D12_MESSAGE *pMessage,
                                               SIZE_T *pMessageByteLength)
  {
    return S_OK;
  }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesAllowedByStorageFilter() { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDeniedByStorageFilter() { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessages() { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessagesAllowedByRetrievalFilter() { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDiscardedByMessageCountLimit() { return 0; }
  virtual UINT64 STDMETHODCALLTYPE GetMessageCountLimit() { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddStorageFilterEntries(D3D12_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE GetStorageFilter(D3D12_INFO_QUEUE_FILTER *pFilter,
                                                     SIZE_T *pFilterByteLength)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE ClearStorageFilter() {}
  virtual HRESULT STDMETHODCALLTYPE PushEmptyStorageFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushCopyOfStorageFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushStorageFilter(D3D12_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE PopStorageFilter() {}
  virtual UINT STDMETHODCALLTYPE GetStorageFilterStackSize() { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddRetrievalFilterEntries(D3D12_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE GetRetrievalFilter(D3D12_INFO_QUEUE_FILTER *pFilter,
                                                       SIZE_T *pFilterByteLength)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE ClearRetrievalFilter() {}
  virtual HRESULT STDMETHODCALLTYPE PushEmptyRetrievalFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushCopyOfRetrievalFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushRetrievalFilter(D3D12_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE PopRetrievalFilter() {}
  virtual UINT STDMETHODCALLTYPE GetRetrievalFilterStackSize() { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddMessage(D3D12_MESSAGE_CATEGORY Category,
                                               D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID,
                                               LPCSTR pDescription)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE AddApplicationMessage(D3D12_MESSAGE_SEVERITY Severity,
                                                          LPCSTR pDescription)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnCategory(D3D12_MESSAGE_CATEGORY Category, BOOL bEnable)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY Severity, BOOL bEnable)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnID(D3D12_MESSAGE_ID ID, BOOL bEnable) { return S_OK; }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnCategory(D3D12_MESSAGE_CATEGORY Category)
  {
    return FALSE;
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnSeverity(D3D12_MESSAGE_SEVERITY Severity)
  {
    return FALSE;
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnID(D3D12_MESSAGE_ID ID) { return FALSE; }
  virtual void STDMETHODCALLTYPE SetMuteDebugOutput(BOOL bMute) {}
  virtual BOOL STDMETHODCALLTYPE GetMuteDebugOutput() { return TRUE; }
};

class WrappedID3D12Device;

// We can pass through all calls to ID3D12DebugDevice without intercepting, this
// struct isonly here so that we can intercept QueryInterface calls to return
// ID3D11InfoQueue
//
// The inheritance is awful for these classes. ID3D12DebugDevice2 inherits from ID3D12DebugDevice
// but ID3D12DebugDevice1 is separate entirely, although its functions overlap with
// ID3D12DebugDevice2 and ID3D12DebugDevice
struct WrappedID3D12DebugDevice : public ID3D12DebugDevice2, public ID3D12DebugDevice1
{
  WrappedID3D12Device *m_pDevice;
  ID3D12DebugDevice *m_pDebug;
  ID3D12DebugDevice1 *m_pDebug1;
  ID3D12DebugDevice2 *m_pDebug2;

  WrappedID3D12DebugDevice() : m_pDevice(NULL), m_pDebug(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DebugDevice
  virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(D3D12_DEBUG_FEATURE Mask)
  {
    return m_pDebug->SetFeatureMask(Mask);
  }

  virtual D3D12_DEBUG_FEATURE STDMETHODCALLTYPE GetFeatureMask()
  {
    return m_pDebug->GetFeatureMask();
  }

  virtual HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(D3D12_RLDO_FLAGS Flags)
  {
    if(m_pDebug)
      return m_pDebug->ReportLiveDeviceObjects(Flags);
    else
      return m_pDebug1->ReportLiveDeviceObjects(Flags);
  }

  //////////////////////////////
  // implement ID3D12DebugDevice1 / ID3D12DebugDevice2
  virtual HRESULT STDMETHODCALLTYPE SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_TYPE Type,
                                                      _In_reads_bytes_(DataSize) const void *pData,
                                                      UINT DataSize)
  {
    if(m_pDebug1)
      return m_pDebug1->SetDebugParameter(Type, pData, DataSize);
    else
      return m_pDebug2->SetDebugParameter(Type, pData, DataSize);
  }

  virtual HRESULT STDMETHODCALLTYPE GetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_TYPE Type,
                                                      _Out_writes_bytes_(DataSize) void *pData,
                                                      UINT DataSize)
  {
    if(m_pDebug1)
      return m_pDebug1->GetDebugParameter(Type, pData, DataSize);
    else
      return m_pDebug2->GetDebugParameter(Type, pData, DataSize);
  }
};

// give every impression of working but do nothing.
// Same idea as DummyID3D12InfoQueue above, a dummy interface so that users
// expecting a ID3D12DebugDevice don't get confused if we have turned off the debug
// layer and can't return the real one.
//
// The inheritance is awful for these. See WrappedID3D12DebugDevice for why there are multiple
// parent classes
struct DummyID3D12DebugDevice : public ID3D12DebugDevice2, public ID3D12DebugDevice1
{
  WrappedID3D12Device *m_pDevice;

  DummyID3D12DebugDevice() : m_pDevice(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DebugDevice
  virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(D3D12_DEBUG_FEATURE Mask) { return S_OK; }
  virtual D3D12_DEBUG_FEATURE STDMETHODCALLTYPE GetFeatureMask()
  {
    return D3D12_DEBUG_FEATURE_NONE;
  }

  virtual HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(D3D12_RLDO_FLAGS Flags) { return S_OK; }
  //////////////////////////////
  // implement ID3D12DebugDevice1 / ID3D12DebugDevice2
  virtual HRESULT STDMETHODCALLTYPE SetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_TYPE Type,
                                                      _In_reads_bytes_(DataSize) const void *pData,
                                                      UINT DataSize)
  {
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetDebugParameter(D3D12_DEBUG_DEVICE_PARAMETER_TYPE Type,
                                                      _Out_writes_bytes_(DataSize) void *pData,
                                                      UINT DataSize)
  {
    return S_OK;
  }
};

struct WrappedDownlevelDevice : public ID3D12DeviceDownlevel
{
  WrappedID3D12Device &m_pDevice;

  WrappedDownlevelDevice(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  //////////////////////////////
  // implement ID3D12DeviceDownlevel
  virtual HRESULT STDMETHODCALLTYPE
  QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
                       _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo);
};

struct WrappedDRED : public ID3D12DeviceRemovedExtendedData1
{
  WrappedID3D12Device &m_pDevice;
  ID3D12DeviceRemovedExtendedData *m_pReal = NULL;
  ID3D12DeviceRemovedExtendedData1 *m_pReal1 = NULL;

  WrappedDRED(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DeviceRemovedExtendedDataSettings
  virtual HRESULT STDMETHODCALLTYPE GetAutoBreadcrumbsOutput(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT *pOutput)
  {
    return m_pReal->GetAutoBreadcrumbsOutput(pOutput);
  }

  virtual HRESULT STDMETHODCALLTYPE GetPageFaultAllocationOutput(D3D12_DRED_PAGE_FAULT_OUTPUT *pOutput)
  {
    return m_pReal->GetPageFaultAllocationOutput(pOutput);
  }

  //////////////////////////////
  // implement ID3D12DeviceRemovedExtendedData1
  virtual HRESULT STDMETHODCALLTYPE GetAutoBreadcrumbsOutput1(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 *pOutput)
  {
    return m_pReal1->GetAutoBreadcrumbsOutput1(pOutput);
  }

  virtual HRESULT STDMETHODCALLTYPE GetPageFaultAllocationOutput1(D3D12_DRED_PAGE_FAULT_OUTPUT1 *pOutput)
  {
    return m_pReal1->GetPageFaultAllocationOutput1(pOutput);
  }
};

struct WrappedDREDSettings : public ID3D12DeviceRemovedExtendedDataSettings1
{
  WrappedID3D12Device &m_pDevice;
  ID3D12DeviceRemovedExtendedDataSettings *m_pReal = NULL;
  ID3D12DeviceRemovedExtendedDataSettings1 *m_pReal1 = NULL;

  WrappedDREDSettings(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DeviceRemovedExtendedDataSettings
  virtual void STDMETHODCALLTYPE SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT setting)
  {
    m_pReal->SetAutoBreadcrumbsEnablement(setting);
  }
  virtual void STDMETHODCALLTYPE SetPageFaultEnablement(D3D12_DRED_ENABLEMENT setting)
  {
    m_pReal->SetPageFaultEnablement(setting);
  }
  virtual void STDMETHODCALLTYPE SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT setting)
  {
    m_pReal->SetWatsonDumpEnablement(setting);
  }

  //////////////////////////////
  // implement ID3D12DeviceRemovedExtendedDataSettings1
  virtual void STDMETHODCALLTYPE SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT Enablement)
  {
    m_pReal1->SetBreadcrumbContextEnablement(Enablement);
  }
};

struct WrappedID3D12SharingContract : public ID3D12SharingContract, public IDXGISwapper
{
private:
  ID3D12Resource *m_pPresentSource = NULL;
  HWND m_pPresentHWND = NULL;

public:
  WrappedID3D12Device &m_pDevice;
  ID3D12SharingContract *m_pReal = NULL;

  WrappedID3D12SharingContract(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  //////////////////////////////
  // implement IDXGISwapper
  virtual ID3DDevice *GetD3DDevice();
  virtual int GetNumBackbuffers() { return 1; }
  virtual IUnknown **GetBackbuffers() { return (IUnknown **)&m_pPresentSource; }
  virtual int GetLastPresentedBuffer() { return 0; }
  virtual UINT GetWidth();
  virtual UINT GetHeight();
  virtual DXGI_FORMAT GetFormat();
  virtual HWND GetHWND();

  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12SharingContract
  virtual void STDMETHODCALLTYPE Present(_In_ ID3D12Resource *pResource, UINT Subresource,
                                         _In_ HWND window);
  virtual void STDMETHODCALLTYPE SharedFenceSignal(_In_ ID3D12Fence *pFence, UINT64 FenceValue);
  virtual void STDMETHODCALLTYPE BeginCapturableWork(_In_ REFGUID guid);
  virtual void STDMETHODCALLTYPE EndCapturableWork(_In_ REFGUID guid);
};

// these aren't documented, they're defined in D3D12TranslationLayer in the d3d11on12 codebase
typedef enum D3D12_COMPATIBILITY_SHARED_FLAGS {
  D3D12_COMPATIBILITY_SHARED_FLAG_NONE = 0,
  D3D12_COMPATIBILITY_SHARED_FLAG_NON_NT_HANDLE = 0x1,
  D3D12_COMPATIBILITY_SHARED_FLAG_KEYED_MUTEX = 0x2,
  D3D12_COMPATIBILITY_SHARED_FLAG_9_ON_12 = 0x4
} D3D12_COMPATIBILITY_SHARED_FLAGS;

typedef enum D3D12_REFLECT_SHARED_PROPERTY {
  D3D12_REFLECT_SHARED_PROPERTY_D3D11_RESOURCE_FLAGS = 0,
  D3D12_REFELCT_SHARED_PROPERTY_COMPATIBILITY_SHARED_FLAGS =
      (D3D12_REFLECT_SHARED_PROPERTY_D3D11_RESOURCE_FLAGS + 1),
  D3D12_REFLECT_SHARED_PROPERTY_NON_NT_SHARED_HANDLE =
      (D3D12_REFELCT_SHARED_PROPERTY_COMPATIBILITY_SHARED_FLAGS + 1)
} D3D12_REFLECT_SHARED_PROPERTY;

typedef struct D3D11_RESOURCE_FLAGS
{
  UINT BindFlags;
  UINT MiscFlags;
  UINT CPUAccessFlags;
  UINT StructureByteStride;
} D3D11_RESOURCE_FLAGS;

MIDL_INTERFACE("8f1c0e3c-fae3-4a82-b098-bfe1708207ff")
ID3D12CompatibilityDevice : public IUnknown
{
public:
  virtual HRESULT STDMETHODCALLTYPE CreateSharedResource(
      _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
      _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
      _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
      _In_opt_ const D3D11_RESOURCE_FLAGS *pFlags11,
      D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
      _In_opt_ ID3D12LifetimeTracker *pLifetimeTracker,
      _In_opt_ ID3D12SwapChainAssistant *pOwningSwapchain, REFIID riid,
      _COM_Outptr_opt_ void **ppResource) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateSharedHeap(
      _In_ const D3D12_HEAP_DESC *pHeapDesc, D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
      REFIID riid, _COM_Outptr_opt_ void **ppHeap) = 0;

  virtual HRESULT STDMETHODCALLTYPE ReflectSharedProperties(
      _In_ ID3D12Object * pHeapOrResource, D3D12_REFLECT_SHARED_PROPERTY ReflectType,
      _Out_writes_bytes_(DataSize) void *pData, UINT DataSize) = 0;
};

struct WrappedCompatibilityDevice : public ID3D12CompatibilityDevice
{
  WrappedID3D12Device &m_pDevice;
  ID3D12CompatibilityDevice *m_pReal = NULL;

  WrappedCompatibilityDevice(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  WriteSerialiser &GetThreadSerialiser();

  //////////////////////////////
  // implement ID3D12CompatibilityDevice
  virtual HRESULT STDMETHODCALLTYPE CreateSharedResource(
      _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
      _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
      _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
      _In_opt_ const D3D11_RESOURCE_FLAGS *pFlags11,
      D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
      _In_opt_ ID3D12LifetimeTracker *pLifetimeTracker,
      _In_opt_ ID3D12SwapChainAssistant *pOwningSwapchain, REFIID riid,
      _COM_Outptr_opt_ void **ppResource);

  virtual HRESULT STDMETHODCALLTYPE CreateSharedHeap(_In_ const D3D12_HEAP_DESC *pHeapDesc,
                                                     D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
                                                     REFIID riid, _COM_Outptr_opt_ void **ppHeap);

  virtual HRESULT STDMETHODCALLTYPE ReflectSharedProperties(_In_ ID3D12Object *pHeapOrResource,
                                                            D3D12_REFLECT_SHARED_PROPERTY ReflectType,
                                                            _Out_writes_bytes_(DataSize) void *pData,
                                                            UINT DataSize);
};

struct WrappedNVAPI12 : public INVAPID3DDevice
{
private:
  WrappedID3D12Device &m_pDevice;

public:
  WrappedNVAPI12(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  virtual BOOL STDMETHODCALLTYPE SetReal(IUnknown *);
  virtual IUnknown *STDMETHODCALLTYPE GetReal();
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD space, DWORD reg, BOOL global);
};

struct WrappedAGS12 : public IAGSD3DDevice
{
private:
  WrappedID3D12Device &m_pDevice;

public:
  WrappedAGS12(WrappedID3D12Device &dev) : m_pDevice(dev) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  virtual IUnknown *STDMETHODCALLTYPE GetReal();
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD space, DWORD reg);

  virtual HRESULT STDMETHODCALLTYPE CreateD3D11(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
                                                CONST D3D_FEATURE_LEVEL *, UINT FeatureLevels, UINT,
                                                CONST DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **,
                                                ID3D11Device **, D3D_FEATURE_LEVEL *,
                                                ID3D11DeviceContext **);
  virtual HRESULT STDMETHODCALLTYPE CreateD3D12(IUnknown *pAdapter,
                                                D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                                void **ppDevice);
  virtual BOOL STDMETHODCALLTYPE ExtensionsSupported();
};

class WrappedID3D12CommandQueue;

#define IMPLEMENT_FUNCTION_THREAD_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                                     \
  template <typename SerialiserType>                         \
  bool CONCAT(Serialise_, func(SerialiserType &ser, __VA_ARGS__));

class WrappedID3D12Device : public IFrameCapturer, public ID3DDevice, public ID3D12Device8
{
private:
  ID3D12Device *m_pDevice;
  ID3D12Device1 *m_pDevice1;
  ID3D12Device2 *m_pDevice2;
  ID3D12Device3 *m_pDevice3;
  ID3D12Device4 *m_pDevice4;
  ID3D12Device5 *m_pDevice5;
  ID3D12Device6 *m_pDevice6;
  ID3D12Device7 *m_pDevice7;
  ID3D12Device8 *m_pDevice8;
  ID3D12DeviceDownlevel *m_pDownlevel;

  // list of all queues being captured
  rdcarray<WrappedID3D12CommandQueue *> m_Queues;
  rdcarray<ID3D12Fence *> m_QueueFences;

  // list of queues and buffers kept alive during capture artificially even if the user destroys
  // them, so we can use them in the capture. Storing this separately prevents races where a
  // queue/buffer is added between us transitioning away from active capturing (so we don't addref
  // it) and us releasing our reference on them.
  rdcarray<WrappedID3D12CommandQueue *> m_RefQueues;
  rdcarray<ID3D12Resource *> m_RefBuffers;

  // the queue we use for all internal work, the first DIRECT queue
  WrappedID3D12CommandQueue *m_Queue;

  ID3D12CommandAllocator *m_Alloc = NULL, *m_DataUploadAlloc = NULL;
  ID3D12GraphicsCommandList *m_DataUploadList = NULL;
  ID3D12DescriptorHeap *m_RTVHeap = NULL;
  ID3D12Fence *m_GPUSyncFence;
  HANDLE m_GPUSyncHandle;
  UINT64 m_GPUSyncCounter;

  WrappedDownlevelDevice m_WrappedDownlevel;
  WrappedDRED m_DRED;
  WrappedDREDSettings m_DREDSettings;
  WrappedCompatibilityDevice m_CompatDevice;
  WrappedNVAPI12 m_WrappedNVAPI;
  WrappedAGS12 m_WrappedAGS;

  rdcarray<ID3D12CommandAllocator *> m_CommandAllocators;

  D3D12_CPU_DESCRIPTOR_HANDLE AllocRTV();
  void FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle);

  rdcarray<D3D12_CPU_DESCRIPTOR_HANDLE> m_FreeRTVs;
  rdcarray<D3D12_CPU_DESCRIPTOR_HANDLE> m_UsedRTVs;

  void CreateInternalResources();
  void DestroyInternalResources();

  IAmdExtD3DFactory *m_pAMDExtObject = NULL;

  Threading::CriticalSection m_EXTUAVLock;
  uint32_t m_GlobalEXTUAV = ~0U;
  uint32_t m_GlobalEXTUAVSpace = ~0U;
  uint64_t m_ThreadLocalEXTUAVSlot = ~0ULL;
  GPUVendor m_VendorEXT = GPUVendor::Unknown;
  INVAPID3DDevice *m_ReplayNVAPI = NULL;
  IAGSD3DDevice *m_ReplayAGS = NULL;

  D3D12ResourceManager *m_ResourceManager;
  DummyID3D12InfoQueue m_DummyInfoQueue;
  DummyID3D12DebugDevice m_DummyDebug;
  WrappedID3D12DebugDevice m_WrappedDebug;
  WrappedID3D12SharingContract m_SharingContract;

  D3D12Replay *m_Replay;
  D3D12ShaderCache *m_ShaderCache = NULL;
  D3D12TextRenderer *m_TextRenderer = NULL;

  std::set<ResourceId> m_UploadResourceIds;
  std::map<uint64_t, ID3D12Resource *> m_UploadBuffers;

  Threading::CriticalSection m_MapsLock;
  rdcarray<MapState> m_Maps;

  Threading::CriticalSection m_WrapDeduplicateLock;

  bool ProcessChunk(ReadSerialiser &ser, D3D12Chunk context);

  unsigned int m_InternalRefcount;
  RefCounter12<ID3D12Device> m_RefCounter;
  RefCounter12<ID3D12Device> m_SoftRefCounter;
  bool m_Alive;

  uint64_t threadSerialiserTLSSlot;

  Threading::CriticalSection m_ThreadSerialisersLock;
  rdcarray<WriteSerialiser *> m_ThreadSerialisers;

  uint64_t tempMemoryTLSSlot;
  struct TempMem
  {
    TempMem() : memory(NULL), size(0) {}
    byte *memory;
    size_t size;
  };
  Threading::CriticalSection m_ThreadTempMemLock;
  rdcarray<TempMem *> m_ThreadTempMem;

  rdcarray<DebugMessage> m_DebugMessages;

  uint64_t m_TimeBase = 0;
  double m_TimeFrequency = 1.0f;
  SDFile *m_StructuredFile = NULL;
  SDFile m_StoredStructuredData;

  uint32_t m_FrameCounter = 0;
  rdcarray<FrameDescription> m_CapturedFrames;
  rdcarray<DrawcallDescription *> m_Drawcalls;

  ReplayStatus m_FailedReplayStatus = ReplayStatus::APIReplayFailed;

  bool m_AppControlledCapture = false;

  Threading::RWLock m_CapTransitionLock;
  CaptureState m_State;

  PerformanceTimer m_CaptureTimer;

  uint32_t m_SubmitCounter = 0;

  bool m_UsedDXIL = false;

  D3D12InitParams m_InitParams;
  uint64_t m_SectionVersion;
  ReplayOptions m_ReplayOptions;
  ID3D12InfoQueue *m_pInfoQueue;

  D3D12ResourceRecord *m_FrameCaptureRecord;
  Chunk *m_HeaderChunk;

  std::set<rdcstr> m_StringDB;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_DeviceRecord;

  Threading::CriticalSection m_DynDescLock;
  rdcarray<D3D12Descriptor> m_DynamicDescriptorRefs;

  GPUAddressRangeTracker m_GPUAddresses;

  // used both on capture and replay side to track resource states. Only locked
  // in capture
  std::map<ResourceId, SubresourceStateVector> m_ResourceStates;
  Threading::CriticalSection m_ResourceStatesLock;

  // used on replay only. Contains the initial resource states before any barriers - this allows us
  // to reset any resources to their proper initial state if they were created mid-frame, since for
  // those we won't have recorded their state at the start of the frame capture.
  std::map<ResourceId, SubresourceStateVector> m_InitialResourceStates;

  std::set<ResourceId> m_Cubemaps;

  // only valid on replay
  std::map<ResourceId, WrappedID3D12Resource *> *m_ResourceList = NULL;
  rdcarray<WrappedID3D12PipelineState *> *m_PipelineList = NULL;

  struct SwapPresentInfo
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8] = {};

    WrappedID3D12CommandQueue *queue;
  };

  std::map<IDXGISwapper *, SwapPresentInfo> m_SwapChains;
  std::map<ResourceId, DXGI_FORMAT> m_BackbufferFormat;

  IDXGISwapper *m_LastSwap = NULL;

  D3D12_FEATURE_DATA_D3D12_OPTIONS m_D3D12Opts;
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 m_D3D12Opts1;
  D3D12_FEATURE_DATA_D3D12_OPTIONS2 m_D3D12Opts2;
  D3D12_FEATURE_DATA_D3D12_OPTIONS3 m_D3D12Opts3;
  D3D12_FEATURE_DATA_D3D12_OPTIONS6 m_D3D12Opts6;
  UINT m_DescriptorIncrements[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);
  void EndCaptureFrame();

  bool m_debugLayerEnabled;

  static Threading::CriticalSection m_DeviceWrappersLock;
  static std::map<ID3D12Device *, WrappedID3D12Device *> m_DeviceWrappers;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Device);

  WrappedID3D12Device(ID3D12Device *realDevice, D3D12InitParams params, bool enabledDebugLayer);
  bool IsDebugLayerEnabled() const { return m_debugLayerEnabled; }
  virtual ~WrappedID3D12Device();

  static WrappedID3D12Device *Create(ID3D12Device *realDevice, D3D12InitParams params,
                                     bool enabledDebugLayer);

  UINT GetUnwrappedDescriptorIncrement(D3D12_DESCRIPTOR_HEAP_TYPE type)
  {
    return m_DescriptorIncrements[type];
  }

  const D3D12_FEATURE_DATA_D3D12_OPTIONS &GetOpts() { return m_D3D12Opts; }
  const D3D12_FEATURE_DATA_D3D12_OPTIONS1 &GetOpts1() { return m_D3D12Opts1; }
  const D3D12_FEATURE_DATA_D3D12_OPTIONS2 &GetOpts2() { return m_D3D12Opts2; }
  const D3D12_FEATURE_DATA_D3D12_OPTIONS3 &GetOpts3() { return m_D3D12Opts3; }
  const D3D12_FEATURE_DATA_D3D12_OPTIONS6 &GetOpts6() { return m_D3D12Opts6; }
  void RemoveQueue(WrappedID3D12CommandQueue *queue);

  // only valid on replay
  std::map<ResourceId, WrappedID3D12Resource *> &GetResourceList() { return *m_ResourceList; }
  rdcarray<WrappedID3D12PipelineState *> &GetPipelineList() { return *m_PipelineList; }
  ////////////////////////////////////////////////////////////////
  // non wrapping interface

  APIProperties APIProps;

  WriteSerialiser &GetThreadSerialiser();

  ID3D12Device *GetReal() { return m_pDevice; }
  static rdcstr GetChunkName(uint32_t idx);
  D3D12ResourceManager *GetResourceManager() { return m_ResourceManager; }
  D3D12ShaderCache *GetShaderCache() { return m_ShaderCache; }
  D3D12DebugManager *GetDebugManager();
  ResourceId GetResourceID() { return m_ResourceID; }
  Threading::RWLock &GetCapTransitionLock() { return m_CapTransitionLock; }
  void ReleaseSwapchainResources(IDXGISwapChain *swap, IUnknown **backbuffers, int numBackbuffers);
  void FirstFrame(IDXGISwapper *swapper);
  const DrawcallDescription *GetDrawcall(uint32_t eventId);

  ResourceId GetFrameCaptureResourceId() { return m_FrameCaptureRecord->GetResourceID(); }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d);
  void AddDebugMessage(const DebugMessage &msg);
  rdcarray<DebugMessage> GetDebugMessages();

  ResourceDescription &GetResourceDesc(ResourceId id);
  void AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix);
  void DerivedResource(ResourceId parent, ResourceId child);
  void DerivedResource(ID3D12DeviceChild *parent, ResourceId child);
  void AddResourceCurChunk(ResourceDescription &descr);
  void AddResourceCurChunk(ResourceId id);

  bool UsedDXIL() { return m_UsedDXIL; }
  rdcarray<D3D12_RESOURCE_STATES> &GetSubresourceStates(ResourceId id)
  {
    return m_ResourceStates[id];
  }
  const std::map<ResourceId, SubresourceStateVector> &GetSubresourceStates()
  {
    return m_ResourceStates;
  }
  const std::map<ResourceId, DXGI_FORMAT> &GetBackbufferFormats() { return m_BackbufferFormat; }
  void SetLogFile(const char *logfile);
  void SetInitParams(const D3D12InitParams &params, uint64_t sectionVersion,
                     const ReplayOptions &opts, INVAPID3DDevice *nvapi, IAGSD3DDevice *ags)
  {
    m_InitParams = params;
    m_SectionVersion = sectionVersion;
    m_ReplayOptions = opts;
    m_ReplayNVAPI = nvapi;
    m_ReplayAGS = ags;
  }
  const ReplayOptions &GetReplayOptions() { return m_ReplayOptions; }
  uint64_t GetLogVersion() { return m_SectionVersion; }
  CaptureState GetState() { return m_State; }
  D3D12Replay *GetReplay() { return m_Replay; }
  WrappedID3D12CommandQueue *GetQueue() { return m_Queue; }
  ID3D12CommandAllocator *GetAlloc() { return m_Alloc; }
  void ApplyBarriers(rdcarray<D3D12_RESOURCE_BARRIER> &barriers);

  void GetDynamicDescriptorReferences(rdcarray<D3D12Descriptor> &refs)
  {
    SCOPED_LOCK(m_DynDescLock);
    m_DynamicDescriptorRefs.swap(refs);
  }

  void GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr, ResourceId &id, UINT64 &offs)
  {
    m_GPUAddresses.GetResIDFromAddr(addr, id, offs);
  }

  bool IsCubemap(ResourceId id) { return m_Cubemaps.find(id) != m_Cubemaps.end(); }
  // returns thread-local temporary memory
  byte *GetTempMemory(size_t s);
  template <class T>
  T *GetTempArray(uint32_t arraycount)
  {
    return (T *)GetTempMemory(sizeof(T) * arraycount);
  }

  struct
  {
    void Reset()
    {
      freecmds.clear();
      pendingcmds.clear();
      submittedcmds.clear();
    }

    rdcarray<ID3D12GraphicsCommandListX *> freecmds;
    // -> GetNextCmd() ->
    rdcarray<ID3D12GraphicsCommandListX *> pendingcmds;
    // -> ExecuteLists() ->
    rdcarray<ID3D12GraphicsCommandListX *> submittedcmds;
    // -> FlushLists()--------back to freecmds--------^
  } m_InternalCmds;

  // batch this many initial state lists together. Balance between
  // creating fewer temporary lists and making too bloated lists
  static const int initialStateMaxBatch = 100;
  int initStateCurBatch;
  ID3D12GraphicsCommandListX *initStateCurList;

  // ExecuteLists may issue this many command lists together. If more than this are
  // pending at once then they will be split up and issued in multiple calls.
  static const UINT executeListsMaxSize = 50;

  ID3D12GraphicsCommandListX *GetNewList();
  ID3D12GraphicsCommandListX *GetInitialStateList();
  void CloseInitialStateList();
  ID3D12Resource *GetUploadBuffer(uint64_t chunkOffset, uint64_t byteSize);
  void ApplyInitialContents();

  void AddCaptureSubmission();

  void ExecuteList(ID3D12GraphicsCommandListX *list, WrappedID3D12CommandQueue *queue = NULL,
                   bool InFrameCaptureBoundary = false);
  void MarkListExecuted(ID3D12GraphicsCommandListX *list);
  void ExecuteLists(WrappedID3D12CommandQueue *queue = NULL, bool InFrameCaptureBoundary = false);
  void FlushLists(bool forceSync = false, ID3D12CommandQueue *queue = NULL);

  void GPUSync(ID3D12CommandQueue *queue = NULL, ID3D12Fence *fence = NULL);
  void GPUSyncAllQueues();

  RDCDriver GetFrameCaptureDriver() { return RDCDriver::D3D12; }
  void StartFrameCapture(void *dev, void *wnd);
  bool EndFrameCapture(void *dev, void *wnd);
  bool DiscardFrameCapture(void *dev, void *wnd);

  template <typename SerialiserType>
  bool Serialise_Present(SerialiserType &ser, ID3D12Resource *PresentedImage, UINT SyncInterval,
                         UINT Flags);

  template <typename SerialiserType>
  bool Serialise_BeginCaptureFrame(SerialiserType &ser);

  template <typename SerialiserType>
  bool Serialise_DynamicDescriptorWrite(SerialiserType &ser, const DynamicDescriptorWrite *write);
  template <typename SerialiserType>
  bool Serialise_DynamicDescriptorCopies(SerialiserType &ser,
                                         const rdcarray<DynamicDescriptorCopy> &DescriptorCopies);

  ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);

  void SetStructuredExport(uint64_t sectionVersion)
  {
    m_SectionVersion = sectionVersion;
    m_State = CaptureState::StructuredExport;
  }
  SDFile &GetStructuredFile() { return *m_StructuredFile; }
  uint64_t GetTimeBase() { return m_TimeBase; }
  double GetTimeFrequency() { return m_TimeFrequency; }
  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  void *GetFrameCapturerDevice() { return (ID3D12Device *)this; }
  virtual IFrameCapturer *GetFrameCapturer() { return this; }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D12Resource); }
  virtual bool IsDeviceUUID(REFIID iid)
  {
    if(iid == __uuidof(ID3D12Device) || iid == __uuidof(ID3D12Device1) ||
       iid == __uuidof(ID3D12Device2) || iid == __uuidof(ID3D12Device3) ||
       iid == __uuidof(ID3D12Device4) || iid == __uuidof(ID3D12Device5) ||
       iid == __uuidof(ID3D12Device6) || iid == __uuidof(ID3D12Device7) ||
       iid == __uuidof(ID3D12Device8))
      return true;

    return false;
  }
  virtual IUnknown *GetDeviceInterface(REFIID iid)
  {
    if(iid == __uuidof(ID3D12Device))
      return (ID3D12Device *)this;
    else if(iid == __uuidof(ID3D12Device1))
      return (ID3D12Device1 *)this;
    else if(iid == __uuidof(ID3D12Device2))
      return (ID3D12Device2 *)this;
    else if(iid == __uuidof(ID3D12Device3))
      return (ID3D12Device3 *)this;
    else if(iid == __uuidof(ID3D12Device4))
      return (ID3D12Device4 *)this;
    else if(iid == __uuidof(ID3D12Device5))
      return (ID3D12Device5 *)this;
    else if(iid == __uuidof(ID3D12Device6))
      return (ID3D12Device6 *)this;
    else if(iid == __uuidof(ID3D12Device7))
      return (ID3D12Device7 *)this;
    else if(iid == __uuidof(ID3D12Device8))
      return (ID3D12Device8 *)this;

    RDCERR("Requested unknown device interface %s", ToStr(iid).c_str());

    return NULL;
  }
  // Swap Chain
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(IUnknown *, WrapSwapchainBuffer, IDXGISwapper *swapper,
                                       DXGI_FORMAT bufferFormat, UINT buffer, IUnknown *realSurface);
  HRESULT Present(ID3D12GraphicsCommandList *pOverlayCommandList, IDXGISwapper *swapper,
                  UINT SyncInterval, UINT Flags);
  HRESULT Present(IDXGISwapper *swapper, UINT SyncInterval, UINT Flags)
  {
    return Present(NULL, swapper, SyncInterval, Flags);
  }

  void NewSwapchainBuffer(IUnknown *backbuffer);
  void ReleaseSwapchainResources(IDXGISwapper *swapper, UINT QueueCount,
                                 IUnknown *const *ppPresentQueue, IUnknown **unwrappedQueues);

  IDXGIResource *WrapExternalDXGIResource(IDXGIResource *res);

  void Map(ID3D12Resource *Resource, UINT Subresource);
  void Unmap(ID3D12Resource *Resource, UINT Subresource, byte *mapPtr,
             const D3D12_RANGE *pWrittenRange);

  // helper for ID3D12DeviceChild implementations to use
  HRESULT GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    if(!ppvDevice)
      return E_INVALIDARG;

    if(riid == __uuidof(ID3D12Device))
    {
      *ppvDevice = (ID3D12Device *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device1))
    {
      *ppvDevice = (ID3D12Device1 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device2))
    {
      *ppvDevice = (ID3D12Device2 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device3))
    {
      *ppvDevice = (ID3D12Device3 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device4))
    {
      *ppvDevice = (ID3D12Device4 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device5))
    {
      *ppvDevice = (ID3D12Device5 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device6))
    {
      *ppvDevice = (ID3D12Device6 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device7))
    {
      *ppvDevice = (ID3D12Device7 *)this;
      this->AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Device8))
    {
      *ppvDevice = (ID3D12Device8 *)this;
      this->AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(void, MapDataWrite, ID3D12Resource *Resource,
                                       UINT Subresource, byte *mapPtr, D3D12_RANGE range);
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(void, WriteToSubresource, ID3D12Resource *Resource,
                                       UINT Subresource, const D3D12_BOX *pDstBox,
                                       const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);

  rdcarray<MapState> GetMaps()
  {
    rdcarray<MapState> ret;
    {
      SCOPED_LOCK(m_MapsLock);
      ret = m_Maps;
    }
    return ret;
  }

  void InternalRef() { InterlockedIncrement(&m_InternalRefcount); }
  void InternalRelease() { InterlockedDecrement(&m_InternalRefcount); }
  void SoftRef() { m_SoftRefCounter.AddRef(); }
  void SoftRelease()
  {
    m_SoftRefCounter.Release();
    CheckForDeath();
  }
  void CheckForDeath();

  void ReleaseResource(ID3D12DeviceChild *pResource);

  // helper function that takes an expanded descriptor, but downcasts it to the regular descriptor
  // if the new creation method isn't available.
  HRESULT CreatePipeState(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &desc,
                          ID3D12PipelineState **state);

  // Resource
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(void, SetName, ID3D12DeviceChild *pResource, const char *Name);
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(HRESULT, SetShaderDebugPath, ID3D12DeviceChild *pResource,
                                       const char *Path);

  // IHV APIs
  IMPLEMENT_FUNCTION_SERIALISED(void, SetShaderExtUAV, GPUVendor vendor, uint32_t reg,
                                uint32_t space, bool global);
  void GetShaderExtUAV(uint32_t &reg, uint32_t &space);

  // Protected session
  ID3D12Fence *CreateProtectedSessionFence(ID3D12Fence *real);

  //////////////////////////////
  // implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return m_RefCounter.AddRef(); }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = m_RefCounter.Release();
    CheckForDeath();
    return ret;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D12Object
  virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData);

  virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData);

  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData);

  virtual HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name);

  //////////////////////////////
  // implement ID3D12Device
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetNodeCount, );

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommandQueue,
                                       const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid,
                                       void **ppCommandQueue);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommandAllocator,
                                       D3D12_COMMAND_LIST_TYPE type, REFIID riid,
                                       void **ppCommandAllocator);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateGraphicsPipelineState,
                                       const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid,
                                       void **ppPipelineState);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateComputePipelineState,
                                       const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid,
                                       void **ppPipelineState);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommandList,
                                       UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                                       ID3D12CommandAllocator *pCommandAllocator,
                                       ID3D12PipelineState *pInitialState, REFIID riid,
                                       void **ppCommandList);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckFeatureSupport,
                                       D3D12_FEATURE Feature, void *pFeatureSupportData,
                                       UINT FeatureSupportDataSize);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDescriptorHeap,
                                       const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc,
                                       REFIID riid, void **ppvHeap);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual UINT STDMETHODCALLTYPE,
                                       GetDescriptorHandleIncrementSize,
                                       D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRootSignature,
                                       UINT nodeMask, const void *pBlobWithRootSignature,
                                       SIZE_T blobLengthInBytes, REFIID riid,
                                       void **ppvRootSignature);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CreateConstantBufferView,
                                       const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CreateShaderResourceView,
                                       ID3D12Resource *pResource,
                                       const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CreateUnorderedAccessView,
                                       ID3D12Resource *pResource, ID3D12Resource *pCounterResource,
                                       const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CreateRenderTargetView,
                                       ID3D12Resource *pResource,
                                       const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CreateDepthStencilView,
                                       ID3D12Resource *pResource,
                                       const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CreateSampler,
                                       const D3D12_SAMPLER_DESC *pDesc,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CopyDescriptors,
                                       UINT NumDestDescriptorRanges,
                                       const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts,
                                       const UINT *pDestDescriptorRangeSizes,
                                       UINT NumSrcDescriptorRanges,
                                       const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts,
                                       const UINT *pSrcDescriptorRangeSizes,
                                       D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, CopyDescriptorsSimple,
                                       UINT NumDescriptors,
                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
                                       D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
                                       D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE,
                                       GetResourceAllocationInfo, UINT visibleMask,
                                       UINT numResourceDescs,
                                       const D3D12_RESOURCE_DESC *pResourceDescs);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE,
                                       GetCustomHeapProperties, UINT nodeMask,
                                       D3D12_HEAP_TYPE heapType);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommittedResource,
                                       const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                       D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc,
                                       D3D12_RESOURCE_STATES InitialResourceState,
                                       const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                       REFIID riidResource, void **ppvResource);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateHeap,
                                       const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePlacedResource,
                                       ID3D12Heap *pHeap, UINT64 HeapOffset,
                                       const D3D12_RESOURCE_DESC *pDesc,
                                       D3D12_RESOURCE_STATES InitialState,
                                       const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                       void **ppvResource);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateReservedResource,
                                       const D3D12_RESOURCE_DESC *pDesc,
                                       D3D12_RESOURCE_STATES InitialState,
                                       const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                       void **ppvResource);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateSharedHandle,
                                       ID3D12DeviceChild *pObject,
                                       const SECURITY_ATTRIBUTES *pAttributes, DWORD Access,
                                       LPCWSTR Name, HANDLE *pHandle);

  HRESULT STDMETHODCALLTYPE OpenSharedHandleInternal(D3D12Chunk chunkType, D3D12_HEAP_FLAGS HeapFlags,
                                                     REFIID riid, void **ppvObj);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedHandle,
                                       HANDLE NTHandle, REFIID riid, void **ppvObj);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedHandleByName,
                                       LPCWSTR Name, DWORD Access, HANDLE *pNTHandle);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, MakeResident,
                                       UINT NumObjects, ID3D12Pageable *const *ppObjects);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Evict, UINT NumObjects,
                                       ID3D12Pageable *const *ppObjects);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateFence,
                                       UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid,
                                       void **ppFence);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetDeviceRemovedReason);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, GetCopyableFootprints,
                                       const D3D12_RESOURCE_DESC *pResourceDesc,
                                       UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset,
                                       D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts, UINT *pNumRows,
                                       UINT64 *pRowSizeInBytes, UINT64 *pTotalBytes);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateQueryHeap,
                                       const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                       void **ppvHeap);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetStablePowerState,
                                       BOOL Enable);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommandSignature,
                                       const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                       ID3D12RootSignature *pRootSignature, REFIID riid,
                                       void **ppvCommandSignature);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual void STDMETHODCALLTYPE, GetResourceTiling,
                                       ID3D12Resource *pTiledResource,
                                       UINT *pNumTilesForEntireResource,
                                       D3D12_PACKED_MIP_INFO *pPackedMipDesc,
                                       D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
                                       UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
                                       D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual LUID STDMETHODCALLTYPE, GetAdapterLuid);

  //////////////////////////////
  // implement ID3D12Device1
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePipelineLibrary,
                                       _In_reads_(BlobLength) const void *pLibraryBlob,
                                       SIZE_T BlobLength, REFIID riid,
                                       _COM_Outptr_ void **ppPipelineLibrary);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                       SetEventOnMultipleFenceCompletion,
                                       _In_reads_(NumFences) ID3D12Fence *const *ppFences,
                                       _In_reads_(NumFences) const UINT64 *pFenceValues,
                                       UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags,
                                       HANDLE hEvent);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetResidencyPriority,
                                       UINT NumObjects,
                                       _In_reads_(NumObjects) ID3D12Pageable *const *ppObjects,
                                       _In_reads_(NumObjects)
                                           const D3D12_RESIDENCY_PRIORITY *pPriorities);

  //////////////////////////////
  // implement ID3D12Device2
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePipelineState,
                                       const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid,
                                       _COM_Outptr_ void **ppPipelineState);

  //////////////////////////////
  // implement ID3D12Device3
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                       OpenExistingHeapFromAddress, _In_ const void *pAddress,
                                       REFIID riid, _COM_Outptr_ void **ppvHeap);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                       OpenExistingHeapFromFileMapping, _In_ HANDLE hFileMapping,
                                       REFIID riid, _COM_Outptr_ void **ppvHeap);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, EnqueueMakeResident,
                                       D3D12_RESIDENCY_FLAGS Flags, UINT NumObjects,
                                       _In_reads_(NumObjects) ID3D12Pageable *const *ppObjects,
                                       _In_ ID3D12Fence *pFenceToSignal, UINT64 FenceValueToSignal);

  //////////////////////////////
  // implement ID3D12Device4
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommandList1,
                                       _In_ UINT nodeMask, _In_ D3D12_COMMAND_LIST_TYPE type,
                                       _In_ D3D12_COMMAND_LIST_FLAGS flags, REFIID riid,
                                       _COM_Outptr_ void **ppCommandList);

  virtual HRESULT STDMETHODCALLTYPE
  CreateProtectedResourceSession(_In_ const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc,
                                 _In_ REFIID riid, _COM_Outptr_ void **ppSession);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommittedResource1,
                                       _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                       D3D12_HEAP_FLAGS HeapFlags,
                                       _In_ const D3D12_RESOURCE_DESC *pDesc,
                                       D3D12_RESOURCE_STATES InitialResourceState,
                                       _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                       _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession,
                                       REFIID riidResource, _COM_Outptr_opt_ void **ppvResource);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateHeap1,
                                       _In_ const D3D12_HEAP_DESC *pDesc,
                                       _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession,
                                       REFIID riid, _COM_Outptr_opt_ void **ppvHeap);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateReservedResource1,
                                       _In_ const D3D12_RESOURCE_DESC *pDesc,
                                       D3D12_RESOURCE_STATES InitialState,
                                       _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                       _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession,
                                       REFIID riid, _COM_Outptr_opt_ void **ppvResource);

  virtual D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE GetResourceAllocationInfo1(
      UINT visibleMask, UINT numResourceDescs,
      _In_reads_(numResourceDescs) const D3D12_RESOURCE_DESC *pResourceDescs,
      _Out_writes_opt_(numResourceDescs) D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1);

  //////////////////////////////
  // implement ID3D12Device5

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateLifetimeTracker,
                                       _In_ ID3D12LifetimeOwner *pOwner, REFIID riid,
                                       _COM_Outptr_ void **ppvTracker);

  virtual void STDMETHODCALLTYPE RemoveDevice();

  virtual HRESULT STDMETHODCALLTYPE EnumerateMetaCommands(_Inout_ UINT *pNumMetaCommands,
                                                          _Out_writes_opt_(*pNumMetaCommands)
                                                              D3D12_META_COMMAND_DESC *pDescs);

  virtual HRESULT STDMETHODCALLTYPE EnumerateMetaCommandParameters(
      _In_ REFGUID CommandId, _In_ D3D12_META_COMMAND_PARAMETER_STAGE Stage,
      _Out_opt_ UINT *pTotalStructureSizeInBytes, _Inout_ UINT *pParameterCount,
      _Out_writes_opt_(*pParameterCount) D3D12_META_COMMAND_PARAMETER_DESC *pParameterDescs);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateMetaCommand,
                                       _In_ REFGUID CommandId, _In_ UINT NodeMask,
                                       _In_reads_bytes_opt_(CreationParametersDataSizeInBytes)
                                           const void *pCreationParametersData,
                                       _In_ SIZE_T CreationParametersDataSizeInBytes, REFIID riid,
                                       _COM_Outptr_ void **ppMetaCommand);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateStateObject,
                                       const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid,
                                       _COM_Outptr_ void **ppStateObject);

  virtual void STDMETHODCALLTYPE GetRaytracingAccelerationStructurePrebuildInfo(
      _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS *pDesc,
      _Out_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO *pInfo);

  virtual D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS STDMETHODCALLTYPE CheckDriverMatchingIdentifier(
      _In_ D3D12_SERIALIZED_DATA_TYPE SerializedDataType,
      _In_ const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER *pIdentifierToCheck);

  //////////////////////////////
  // implement ID3D12DeviceDownlevel
  virtual HRESULT STDMETHODCALLTYPE
  QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
                       _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo);

  //////////////////////////////
  // implement ID3D12Device6
  virtual HRESULT STDMETHODCALLTYPE SetBackgroundProcessingMode(
      D3D12_BACKGROUND_PROCESSING_MODE Mode, D3D12_MEASUREMENTS_ACTION MeasurementsAction,
      _In_opt_ HANDLE hEventToSignalUponCompletion, _Out_opt_ BOOL *pbFurtherMeasurementsDesired);

  //////////////////////////////
  // implement ID3D12Device7
  virtual HRESULT STDMETHODCALLTYPE AddToStateObject(const D3D12_STATE_OBJECT_DESC *pAddition,
                                                     ID3D12StateObject *pStateObjectToGrowFrom,
                                                     REFIID riid,
                                                     _COM_Outptr_ void **ppNewStateObject);

  virtual HRESULT STDMETHODCALLTYPE
  CreateProtectedResourceSession1(_In_ const D3D12_PROTECTED_RESOURCE_SESSION_DESC1 *pDesc,
                                  _In_ REFIID riid, _COM_Outptr_ void **ppSession);

  //////////////////////////////
  // implement ID3D12Device8
  virtual D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE GetResourceAllocationInfo2(
      UINT visibleMask, UINT numResourceDescs,
      _In_reads_(numResourceDescs) const D3D12_RESOURCE_DESC1 *pResourceDescs,
      _Out_writes_opt_(numResourceDescs) D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCommittedResource2,
                                       _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                       D3D12_HEAP_FLAGS HeapFlags,
                                       _In_ const D3D12_RESOURCE_DESC1 *pDesc,
                                       D3D12_RESOURCE_STATES InitialResourceState,
                                       _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                       _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession,
                                       REFIID riidResource, _COM_Outptr_opt_ void **ppvResource);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePlacedResource1,
                                       _In_ ID3D12Heap *pHeap, UINT64 HeapOffset,
                                       _In_ const D3D12_RESOURCE_DESC1 *pDesc,
                                       D3D12_RESOURCE_STATES InitialState,
                                       _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                       REFIID riid, _COM_Outptr_opt_ void **ppvResource);

  virtual void STDMETHODCALLTYPE CreateSamplerFeedbackUnorderedAccessView(
      _In_opt_ ID3D12Resource *pTargetedResource, _In_opt_ ID3D12Resource *pFeedbackResource,
      _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor);

  virtual void STDMETHODCALLTYPE GetCopyableFootprints1(
      _In_ const D3D12_RESOURCE_DESC1 *pResourceDesc,
      _In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
      _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources,
      UINT64 BaseOffset,
      _Out_writes_opt_(NumSubresources) D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
      _Out_writes_opt_(NumSubresources) UINT *pNumRows,
      _Out_writes_opt_(NumSubresources) UINT64 *pRowSizeInBytes, _Out_opt_ UINT64 *pTotalBytes);
};
