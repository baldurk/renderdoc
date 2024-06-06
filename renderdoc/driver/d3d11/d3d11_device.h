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

#pragma once

#include <stdint.h>
#include <map>
#include "common/threading.h"
#include "common/timing.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "driver/ihv/amd/ags_wrapper.h"
#include "driver/ihv/nv/nvapi_wrapper.h"
#include "d3d11_common.h"
#include "d3d11_manager.h"
#include "d3d11_video.h"

class D3D11DebugManager;
class D3D11TextRenderer;
class D3D11ShaderCache;
class D3D11Replay;

#ifndef D3D11_1_UAV_SLOT_COUNT
#define D3D11_1_UAV_SLOT_COUNT 64
#endif

struct StreamOutData
{
  ID3D11Query *query = NULL;
  bool running = false;
  uint64_t numPrims = 0;
  uint32_t stride = 0;
};

struct SOShaderData
{
  uint32_t rastStream = 0;
  uint32_t strides[4] = {};
};

enum TextureDisplayType
{
  TEXDISPLAY_UNKNOWN = 0,
  TEXDISPLAY_SRV_COMPATIBLE,
  TEXDISPLAY_DEPTH_TARGET,
  TEXDISPLAY_INDIRECT_VIEW,
};

struct D3D11InitParams
{
  D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;
  UINT Flags = 0;
  UINT SDKVersion = D3D11_SDK_VERSION;
  UINT NumFeatureLevels = 0;
  D3D_FEATURE_LEVEL FeatureLevels[16] = {};

  DXGI_ADAPTER_DESC AdapterDesc = {};

  GPUVendor VendorExtensions = GPUVendor::Unknown;
  uint32_t VendorUAV = ~0U;

  // check if a frame capture section version is supported
  static const uint64_t CurrentVersion = 0x13;
  static bool IsSupportedVersion(uint64_t ver);
};

DECLARE_REFLECTION_STRUCT(D3D11InitParams);

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;
class WrappedShader;

struct WrappedD3D11Multithread : public ID3D11Multithread
{
  WrappedID3D11Device *m_pDevice = NULL;
  ID3D11Multithread *m_pReal = NULL;

  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11Multithread
  virtual void STDMETHODCALLTYPE Enter();
  virtual void STDMETHODCALLTYPE Leave();
  virtual BOOL STDMETHODCALLTYPE SetMultithreadProtected(BOOL bMTProtect);
  virtual BOOL STDMETHODCALLTYPE GetMultithreadProtected();
};

// We can pass through all calls to ID3D11Debug without intercepting, this
// struct isonly here so that we can intercept QueryInterface calls to return
// ID3D11InfoQueue
struct WrappedID3D11Debug : public ID3D11Debug
{
  WrappedID3D11Device *m_pDevice;
  ID3D11Debug *m_pDebug;

  WrappedID3D11Debug() : m_pDevice(NULL), m_pDebug(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11Debug
  virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(UINT Mask)
  {
    return m_pDebug->SetFeatureMask(Mask);
  }
  virtual UINT STDMETHODCALLTYPE GetFeatureMask() { return m_pDebug->GetFeatureMask(); }
  virtual HRESULT STDMETHODCALLTYPE SetPresentPerRenderOpDelay(UINT Milliseconds)
  {
    return m_pDebug->SetPresentPerRenderOpDelay(Milliseconds);
  }
  virtual UINT STDMETHODCALLTYPE GetPresentPerRenderOpDelay()
  {
    return m_pDebug->GetPresentPerRenderOpDelay();
  }
  virtual HRESULT STDMETHODCALLTYPE SetSwapChain(IDXGISwapChain *pSwapChain)
  {
    return m_pDebug->SetSwapChain(pSwapChain);
  }
  virtual HRESULT STDMETHODCALLTYPE GetSwapChain(IDXGISwapChain **ppSwapChain)
  {
    return m_pDebug->GetSwapChain(ppSwapChain);
  }
  virtual HRESULT STDMETHODCALLTYPE ValidateContext(ID3D11DeviceContext *pContext)
  {
    return m_pDebug->ValidateContext(pContext);
  }
  virtual HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(D3D11_RLDO_FLAGS Flags)
  {
    return m_pDebug->ReportLiveDeviceObjects(Flags);
  }
  virtual HRESULT STDMETHODCALLTYPE ValidateContextForDispatch(ID3D11DeviceContext *pContext)
  {
    return m_pDebug->ValidateContextForDispatch(pContext);
  }
};

struct WrappedNVAPI11 : public INVAPID3DDevice
{
private:
  WrappedID3D11Device &m_pDevice;

public:
  WrappedNVAPI11(WrappedID3D11Device &dev) : m_pDevice(dev) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  virtual BOOL STDMETHODCALLTYPE SetReal(IUnknown *);
  virtual IUnknown *STDMETHODCALLTYPE GetReal();
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD space, DWORD reg, BOOL global);
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
};

struct WrappedAGS11 : public IAGSD3DDevice
{
private:
  WrappedID3D11Device &m_pDevice;

public:
  WrappedAGS11(WrappedID3D11Device &dev) : m_pDevice(dev) {}
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

// give every impression of working but do nothing.
// Just allow the user to call functions so that they don't
// have to check for E_NOINTERFACE when they expect an infoqueue to be there
struct DummyID3D11InfoQueue : public ID3D11InfoQueue
{
  WrappedID3D11Device *m_pDevice;

  DummyID3D11InfoQueue() : m_pDevice(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11InfoQueue
  virtual HRESULT STDMETHODCALLTYPE SetMessageCountLimit(UINT64 MessageCountLimit) { return S_OK; }
  virtual void STDMETHODCALLTYPE ClearStoredMessages() {}
  virtual HRESULT STDMETHODCALLTYPE GetMessage(UINT64 MessageIndex, D3D11_MESSAGE *pMessage,
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
  virtual HRESULT STDMETHODCALLTYPE AddStorageFilterEntries(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE GetStorageFilter(D3D11_INFO_QUEUE_FILTER *pFilter,
                                                     SIZE_T *pFilterByteLength)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE ClearStorageFilter() {}
  virtual HRESULT STDMETHODCALLTYPE PushEmptyStorageFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushCopyOfStorageFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushStorageFilter(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE PopStorageFilter() {}
  virtual UINT STDMETHODCALLTYPE GetStorageFilterStackSize() { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddRetrievalFilterEntries(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE GetRetrievalFilter(D3D11_INFO_QUEUE_FILTER *pFilter,
                                                       SIZE_T *pFilterByteLength)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE ClearRetrievalFilter() {}
  virtual HRESULT STDMETHODCALLTYPE PushEmptyRetrievalFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushCopyOfRetrievalFilter() { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE PushRetrievalFilter(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return S_OK;
  }
  virtual void STDMETHODCALLTYPE PopRetrievalFilter() {}
  virtual UINT STDMETHODCALLTYPE GetRetrievalFilterStackSize() { return 0; }
  virtual HRESULT STDMETHODCALLTYPE AddMessage(D3D11_MESSAGE_CATEGORY Category,
                                               D3D11_MESSAGE_SEVERITY Severity, D3D11_MESSAGE_ID ID,
                                               LPCSTR pDescription)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE AddApplicationMessage(D3D11_MESSAGE_SEVERITY Severity,
                                                          LPCSTR pDescription)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnCategory(D3D11_MESSAGE_CATEGORY Category, BOOL bEnable)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY Severity, BOOL bEnable)
  {
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnID(D3D11_MESSAGE_ID ID, BOOL bEnable) { return S_OK; }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnCategory(D3D11_MESSAGE_CATEGORY Category)
  {
    return FALSE;
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnSeverity(D3D11_MESSAGE_SEVERITY Severity)
  {
    return FALSE;
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnID(D3D11_MESSAGE_ID ID) { return FALSE; }
  virtual void STDMETHODCALLTYPE SetMuteDebugOutput(BOOL bMute) {}
  virtual BOOL STDMETHODCALLTYPE GetMuteDebugOutput() { return TRUE; }
};

// This one actually works and requires a special GUID to access. We only wrap it
// so we can keep the refcounting on our own device
struct WrappedID3D11InfoQueue : public ID3D11InfoQueue
{
  WrappedID3D11Device *m_pDevice = NULL;
  ID3D11InfoQueue *m_pReal = NULL;

  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11InfoQueue
  virtual HRESULT STDMETHODCALLTYPE SetMessageCountLimit(UINT64 MessageCountLimit)
  {
    return m_pReal->SetMessageCountLimit(MessageCountLimit);
  }
  virtual void STDMETHODCALLTYPE ClearStoredMessages() { return m_pReal->ClearStoredMessages(); }
  virtual HRESULT STDMETHODCALLTYPE GetMessage(UINT64 MessageIndex, D3D11_MESSAGE *pMessage,
                                               SIZE_T *pMessageByteLength)
  {
    return m_pReal->GetMessage(MessageIndex, pMessage, pMessageByteLength);
  }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesAllowedByStorageFilter()
  {
    return m_pReal->GetNumMessagesAllowedByStorageFilter();
  }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDeniedByStorageFilter()
  {
    return m_pReal->GetNumMessagesDeniedByStorageFilter();
  }
  virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessages()
  {
    return m_pReal->GetNumStoredMessages();
  }
  virtual UINT64 STDMETHODCALLTYPE GetNumStoredMessagesAllowedByRetrievalFilter()
  {
    return m_pReal->GetNumStoredMessagesAllowedByRetrievalFilter();
  }
  virtual UINT64 STDMETHODCALLTYPE GetNumMessagesDiscardedByMessageCountLimit()
  {
    return m_pReal->GetNumMessagesDiscardedByMessageCountLimit();
  }
  virtual UINT64 STDMETHODCALLTYPE GetMessageCountLimit()
  {
    return m_pReal->GetMessageCountLimit();
  }
  virtual HRESULT STDMETHODCALLTYPE AddStorageFilterEntries(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return m_pReal->AddStorageFilterEntries(pFilter);
  }
  virtual HRESULT STDMETHODCALLTYPE GetStorageFilter(D3D11_INFO_QUEUE_FILTER *pFilter,
                                                     SIZE_T *pFilterByteLength)
  {
    return m_pReal->GetStorageFilter(pFilter, pFilterByteLength);
  }
  virtual void STDMETHODCALLTYPE ClearStorageFilter() { return m_pReal->ClearStorageFilter(); }
  virtual HRESULT STDMETHODCALLTYPE PushEmptyStorageFilter()
  {
    return m_pReal->PushEmptyStorageFilter();
  }
  virtual HRESULT STDMETHODCALLTYPE PushCopyOfStorageFilter()
  {
    return m_pReal->PushCopyOfStorageFilter();
  }
  virtual HRESULT STDMETHODCALLTYPE PushStorageFilter(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return m_pReal->PushStorageFilter(pFilter);
  }
  virtual void STDMETHODCALLTYPE PopStorageFilter() { return m_pReal->PopStorageFilter(); }
  virtual UINT STDMETHODCALLTYPE GetStorageFilterStackSize()
  {
    return m_pReal->GetStorageFilterStackSize();
  }
  virtual HRESULT STDMETHODCALLTYPE AddRetrievalFilterEntries(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return m_pReal->AddRetrievalFilterEntries(pFilter);
  }
  virtual HRESULT STDMETHODCALLTYPE GetRetrievalFilter(D3D11_INFO_QUEUE_FILTER *pFilter,
                                                       SIZE_T *pFilterByteLength)
  {
    return m_pReal->GetRetrievalFilter(pFilter, pFilterByteLength);
  }
  virtual void STDMETHODCALLTYPE ClearRetrievalFilter() { return m_pReal->ClearRetrievalFilter(); }
  virtual HRESULT STDMETHODCALLTYPE PushEmptyRetrievalFilter()
  {
    return m_pReal->PushEmptyRetrievalFilter();
  }
  virtual HRESULT STDMETHODCALLTYPE PushCopyOfRetrievalFilter()
  {
    return m_pReal->PushCopyOfRetrievalFilter();
  }
  virtual HRESULT STDMETHODCALLTYPE PushRetrievalFilter(D3D11_INFO_QUEUE_FILTER *pFilter)
  {
    return m_pReal->PushRetrievalFilter(pFilter);
  }
  virtual void STDMETHODCALLTYPE PopRetrievalFilter() { return m_pReal->PopRetrievalFilter(); }
  virtual UINT STDMETHODCALLTYPE GetRetrievalFilterStackSize()
  {
    return m_pReal->GetRetrievalFilterStackSize();
  }
  virtual HRESULT STDMETHODCALLTYPE AddMessage(D3D11_MESSAGE_CATEGORY Category,
                                               D3D11_MESSAGE_SEVERITY Severity, D3D11_MESSAGE_ID ID,
                                               LPCSTR pDescription)
  {
    return m_pReal->AddMessage(Category, Severity, ID, pDescription);
  }
  virtual HRESULT STDMETHODCALLTYPE AddApplicationMessage(D3D11_MESSAGE_SEVERITY Severity,
                                                          LPCSTR pDescription)
  {
    return m_pReal->AddApplicationMessage(Severity, pDescription);
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnCategory(D3D11_MESSAGE_CATEGORY Category, BOOL bEnable)
  {
    return m_pReal->SetBreakOnCategory(Category, bEnable);
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY Severity, BOOL bEnable)
  {
    return m_pReal->SetBreakOnSeverity(Severity, bEnable);
  }
  virtual HRESULT STDMETHODCALLTYPE SetBreakOnID(D3D11_MESSAGE_ID ID, BOOL bEnable)
  {
    return m_pReal->SetBreakOnID(ID, bEnable);
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnCategory(D3D11_MESSAGE_CATEGORY Category)
  {
    return m_pReal->GetBreakOnCategory(Category);
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnSeverity(D3D11_MESSAGE_SEVERITY Severity)
  {
    return m_pReal->GetBreakOnSeverity(Severity);
  }
  virtual BOOL STDMETHODCALLTYPE GetBreakOnID(D3D11_MESSAGE_ID ID)
  {
    return m_pReal->GetBreakOnID(ID);
  }
  virtual void STDMETHODCALLTYPE SetMuteDebugOutput(BOOL bMute)
  {
    return m_pReal->SetMuteDebugOutput(bMute);
  }
  virtual BOOL STDMETHODCALLTYPE GetMuteDebugOutput() { return m_pReal->GetMuteDebugOutput(); }
};

// give every impression of working but do nothing.
// Same idea as DummyID3D11InfoQueue above, a dummy interface so that users
// expecting a ID3D11Debug don't get confused if we have turned off the debug
// layer and can't return the real one.
struct DummyID3D11Debug : public ID3D11Debug
{
  WrappedID3D11Device *m_pDevice;

  DummyID3D11Debug() : m_pDevice(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11Debug
  virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(UINT Mask) { return S_OK; }
  virtual UINT STDMETHODCALLTYPE GetFeatureMask() { return 0; }
  virtual HRESULT STDMETHODCALLTYPE SetPresentPerRenderOpDelay(UINT Milliseconds) { return S_OK; }
  virtual UINT STDMETHODCALLTYPE GetPresentPerRenderOpDelay(void) { return 0; }
  virtual HRESULT STDMETHODCALLTYPE SetSwapChain(IDXGISwapChain *pSwapChain) { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE GetSwapChain(IDXGISwapChain **ppSwapChain)
  {
    if(ppSwapChain)
      *ppSwapChain = NULL;
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE ValidateContext(ID3D11DeviceContext *pContext) { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(D3D11_RLDO_FLAGS Flags) { return S_OK; }
  virtual HRESULT STDMETHODCALLTYPE ValidateContextForDispatch(ID3D11DeviceContext *pContext)
  {
    return S_OK;
  }
};

class WrappedID3D11ClassLinkage;
enum CaptureFailReason;

class WrappedID3D11Device : public IFrameCapturer, public ID3DDevice, public ID3D11Device5
{
private:
  enum
  {
    eInitialContents_Copy = 0,
    eInitialContents_ClearRTV = 1,
    eInitialContents_ClearDSV = 2,
  };

  D3D11Replay *m_Replay;

  WrappedD3D11Multithread m_WrappedMultithread;
  DummyID3D11InfoQueue m_DummyInfoQueue;
  WrappedID3D11InfoQueue m_WrappedInfoQueue;
  DummyID3D11Debug m_DummyDebug;
  WrappedID3D11Debug m_WrappedDebug;
  WrappedID3D11VideoDevice2 m_WrappedVideo;
  WrappedNVAPI11 m_WrappedNVAPI;
  WrappedAGS11 m_WrappedAGS;

  ID3DUserDefinedAnnotation *m_RealAnnotations;
  int m_ReplayEventCount;

  // the device only has one refcount, all device childs take precisely one when they have external
  // references (if they lose their external references they release it) and when it reaches 0 the
  // device is deleted.
  int32_t m_RefCount;

  rdcarray<ID3D11DeviceChild *> m_DeadObjects;

  int32_t m_ChunkAtomic;

  D3D11DebugManager *m_DebugManager = NULL;
  D3D11TextRenderer *m_TextRenderer = NULL;
  D3D11ShaderCache *m_ShaderCache = NULL;
  D3D11ResourceManager *m_ResourceManager = NULL;

  D3D11InitParams m_InitParams;
  uint64_t m_SectionVersion;
  ReplayOptions m_ReplayOptions;

  ResourceId m_BBID;

  ID3D11Device *m_pDevice;
  ID3D11Device1 *m_pDevice1;
  ID3D11Device2 *m_pDevice2;
  ID3D11Device3 *m_pDevice3;
  ID3D11Device4 *m_pDevice4;
  ID3D11Device5 *m_pDevice5;
  ID3D11InfoQueue *m_pInfoQueue;
  WrappedID3D11DeviceContext *m_pImmediateContext;

  uint32_t m_GlobalEXTUAV = ~0U;
  uint64_t m_ThreadLocalEXTUAVSlot = ~0ULL;
  GPUVendor m_VendorEXT = GPUVendor::Unknown;
  INVAPID3DDevice *m_ReplayNVAPI = NULL;
  IAGSD3DDevice *m_ReplayAGS = NULL;

  // ensure all calls in via the D3D wrapped interface are thread safe
  // protects wrapped resource creation and serialiser access
  Threading::CriticalSection m_D3DLock;

  // behaviour that can be enabled by ID3D11Multithread, to auto-lock all context functions
  bool m_D3DThreadSafe = false;

  WriteSerialiser m_ScratchSerialiser;
  std::set<rdcstr> m_StringDB;

  ResourceId m_ResourceID;
  D3D11ResourceRecord *m_DeviceRecord;

  CaptureState m_State;
  void *m_FirstFrameCaptureWindow = NULL;
  bool m_AppControlledCapture = false;

  PerformanceTimer m_CaptureTimer;

  RDResult m_FailedReplayResult = ResultCode::APIReplayFailed;

  std::set<ID3D11DeviceChild *> m_CachedStateObjects;

  // This function will check if m_CachedStateObjects is growing too large, and if so
  // go through m_CachedStateObjects and release any state objects that are purely
  // cached (refcount == 1). This prevents us from aggressively caching and running
  // out of state objects (D3D11 has a max of 4096).
  //
  // This isn't the ideal solution as it means some Create calls will be slightly
  // more expensive while they run this garbage collect, but it is the simplest.
  //
  // For cases where cached objects are repeatedly created and released this will
  // rarely kick in - only in the case where a lot of unique state objects are
  // created then released and never re-used.
  //
  // Internally locks m_D3DLock
  void CachedObjectsGarbageCollect();

  std::set<WrappedID3D11DeviceContext *> m_DeferredContexts;
  std::map<ID3D11InputLayout *, rdcarray<D3D11_INPUT_ELEMENT_DESC> > m_LayoutDescs;
  std::map<ID3D11InputLayout *, WrappedShader *> m_LayoutShaders;

  std::map<ResourceId, StreamOutData> m_StreamOutCounters;
  std::map<ResourceId, SOShaderData> m_SOShaders;

  static WrappedID3D11Device *m_pCurrentWrappedDevice;

  std::map<IDXGISwapper *, ID3D11RenderTargetView *> m_SwapChains;

  IDXGISwapper *m_LastSwap = NULL;

  uint32_t m_FrameCounter = 0;
  uint32_t m_FailedFrame = 0;
  CaptureFailReason m_FailedReason;
  uint32_t m_Failures = 0;

  uint64_t m_TimeBase = 0;
  double m_TimeFrequency = 1.0f;
  SDFile *m_StructuredFile = NULL;
  SDFile *m_StoredStructuredData;

  int m_OOMHandler = 0;
  rdcarray<DebugMessage> m_DebugMessages;
  RDResult m_FatalError = ResultCode::Succeeded;

  rdcarray<FrameDescription> m_CapturedFrames;
  rdcarray<ActionDescription *> m_Actions;

  void MaskResourceMiscFlags(UINT &MiscFlags);

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Device);

  WrappedID3D11Device(ID3D11Device *realDevice, D3D11InitParams params);
  void SetInitParams(const D3D11InitParams &params, uint64_t sectionVersion,
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
  virtual ~WrappedID3D11Device();

  ////////////////////////////////////////////////////////////////
  // non wrapping interface

  APIProperties APIProps;

  void AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix);
  void DerivedResource(ID3D11DeviceChild *parent, ResourceId child);
  void AddResourceCurChunk(ResourceDescription &descr);
  void AddResourceCurChunk(ResourceId id);

  ID3D11Device *GetReal() { return m_pDevice; }
  static rdcstr GetChunkName(uint32_t idx);
  D3D11ShaderCache *GetShaderCache() { return m_ShaderCache; }
  D3D11ResourceManager *GetResourceManager() { return m_ResourceManager; }
  D3D11DebugManager *GetDebugManager() { return m_DebugManager; }
  D3D11Replay *GetReplay() { return m_Replay; }
  Threading::CriticalSection &D3DLock() { return m_D3DLock; }
  bool D3DThreadSafe() const { return m_D3DThreadSafe; }
  void SetD3DThreadSafe(bool safe) { m_D3DThreadSafe = safe; }
  WrappedID3D11DeviceContext *GetImmediateContext() { return m_pImmediateContext; }
  size_t GetNumDeferredContexts() { return m_DeferredContexts.size(); }
  void AddDeferredContext(WrappedID3D11DeviceContext *defctx);
  void RemoveDeferredContext(WrappedID3D11DeviceContext *defctx);
  WrappedID3D11DeviceContext *GetDeferredContext(size_t idx);

  const std::map<ResourceId, StreamOutData> &GetSOHiddenCounters() { return m_StreamOutCounters; }
  StreamOutData &GetSOHiddenCounterForBuffer(ResourceId id) { return m_StreamOutCounters[id]; }
  const SOShaderData &GetSOShaderData(ResourceId id) { return m_SOShaders[id]; }
  ResourceId GetResourceID() { return m_ResourceID; }
  const ActionDescription *GetAction(uint32_t eventId);
  ResourceDescription &GetResourceDesc(ResourceId id);
  FrameStatistics &GetFrameStats();

  void ReplayPushEvent() { m_ReplayEventCount++; }
  void ReplayPopEvent() { m_ReplayEventCount = RDCMAX(0, m_ReplayEventCount - 1); }
  void LockForChunkFlushing();
  void UnlockForChunkFlushing();
  void LockForChunkRemoval();
  void UnlockForChunkRemoval();

  SDFile *GetStructuredFile() { return m_StructuredFile; }
  SDFile *DetachStructuredFile()
  {
    SDFile *ret = m_StoredStructuredData;
    m_StoredStructuredData = m_StructuredFile = NULL;
    return ret;
  }
  uint64_t GetTimeBase() { return m_TimeBase; }
  double GetTimeFrequency() { return m_TimeFrequency; }
  void FirstFrame(IDXGISwapper *swapper);

  void HandleOOM(bool handle)
  {
    if(handle)
      m_OOMHandler++;
    else
      m_OOMHandler--;
  }
  void CheckHRESULT(HRESULT hr);
  void ReportFatalError(RDResult error) { m_FatalError = error; }
  RDResult FatalErrorCheck() { return m_FatalError; }
  bool HasFatalError() { return m_FatalError != ResultCode::Succeeded; }
  rdcarray<DebugMessage> GetDebugMessages();
  void AddDebugMessage(DebugMessage msg);
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d);
  const rdcarray<D3D11_INPUT_ELEMENT_DESC> &GetLayoutDesc(ID3D11InputLayout *layout)
  {
    return m_LayoutDescs[layout];
  }

  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);

  RDCDriver GetFrameCaptureDriver() { return RDCDriver::D3D11; }
  void StartFrameCapture(DeviceOwnedWindow devWnd);
  bool EndFrameCapture(DeviceOwnedWindow devWnd);
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd);

  ID3DUserDefinedAnnotation *GetAnnotations() { return m_RealAnnotations; }
  ID3D11InfoQueue *GetInfoQueue() { return m_pInfoQueue; }
  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  void *GetFrameCapturerDevice() { return (ID3D11Device *)this; }
  virtual IFrameCapturer *GetFrameCapturer() { return this; }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D11Texture2D); }
  virtual bool IsDeviceUUID(REFIID iid)
  {
    if(iid == __uuidof(ID3D11Device) || iid == __uuidof(ID3D11Device1) ||
       iid == __uuidof(ID3D11Device2) || iid == __uuidof(ID3D11Device3) ||
       iid == __uuidof(ID3D11Device4) || iid == __uuidof(ID3D11Device5))
      return true;

    return false;
  }
  virtual IUnknown *GetDeviceInterface(REFIID iid)
  {
    if(iid == __uuidof(ID3D11Device))
      return (ID3D11Device *)this;
    else if(iid == __uuidof(ID3D11Device1))
      return (ID3D11Device1 *)this;
    else if(iid == __uuidof(ID3D11Device2))
      return (ID3D11Device2 *)this;
    else if(iid == __uuidof(ID3D11Device3))
      return (ID3D11Device3 *)this;
    else if(iid == __uuidof(ID3D11Device4))
      return (ID3D11Device4 *)this;
    else if(iid == __uuidof(ID3D11Device5))
      return (ID3D11Device5 *)this;

    RDCERR("Requested unknown device interface %s", ToStr(iid).c_str());

    return NULL;
  }
  ////////////////////////////////////////////////////////////////
  // log replaying

  bool Prepare_InitialState(ID3D11DeviceChild *res);
  uint64_t GetSize_InitialState(ResourceId id, const D3D11InitialContents &initial);
  template <typename SerialiserType>
  bool Serialise_InitialState(SerialiserType &ser, ResourceId resid, D3D11ResourceRecord *record,
                              const D3D11InitialContents *initial);

  void Create_InitialState(ResourceId id, ID3D11DeviceChild *live, bool hasData);
  void Apply_InitialState(ID3D11DeviceChild *live, const D3D11InitialContents &initial);

  void SetStructuredExport(uint64_t sectionVersion)
  {
    m_SectionVersion = sectionVersion;
    m_State = CaptureState::StructuredExport;
  }
  RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  bool ProcessChunk(ReadSerialiser &ser, D3D11Chunk context);
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);

  ////////////////////////////////////////////////////////////////
  // 'fake' interfaces

  // Resource
  IMPLEMENT_FUNCTION_SERIALISED(void, SetResourceName, ID3D11DeviceChild *pResource,
                                const char *Name);
  IMPLEMENT_FUNCTION_SERIALISED(HRESULT, SetShaderDebugPath, ID3D11DeviceChild *pResource,
                                const char *Path);

  // IHV APIs
  IMPLEMENT_FUNCTION_SERIALISED(void, SetShaderExtUAV, GPUVendor vendor, uint32_t reg, bool global);
  uint32_t GetShaderExtUAV();

  // Swap Chain
  IMPLEMENT_FUNCTION_SERIALISED(IUnknown *, WrapSwapchainBuffer, IDXGISwapper *swapper,
                                DXGI_FORMAT bufferFormat, UINT buffer, IUnknown *realSurface);

// this is defined as a macro so that we can re-use it to explicitly instantiate these functions as
// templates in the wrapper definition file.
#define SERIALISED_ID3D11DEVICE_FAKE_FUNCTIONS()                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      ID3D11ClassInstance *, CreateClassInstance, LPCSTR pClassTypeName,                            \
      UINT ConstantBufferOffset, UINT ConstantVectorOffset, UINT TextureOffset,                     \
      UINT SamplerOffset, ID3D11ClassLinkage *pClassLinkage, ID3D11ClassInstance **ppInstance);     \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(ID3D11ClassInstance *, GetClassInstance, LPCSTR pClassInstanceName, \
                                UINT InstanceIndex, ID3D11ClassLinkage *pClassLinkage,              \
                                ID3D11ClassInstance **ppInstance);

  SERIALISED_ID3D11DEVICE_FAKE_FUNCTIONS();

  HRESULT Present(IDXGISwapper *swapper, UINT SyncInterval, UINT Flags);

  void NewSwapchainBuffer(IUnknown *backbuffer);

  void ReleaseSwapchainResources(IDXGISwapper *swapper, UINT QueueCount,
                                 IUnknown *const *ppPresentQueue, IUnknown **unwrappedQueues);

  virtual IDXGIResource *WrapExternalDXGIResource(IDXGIResource *res);

  ResourceId GetBackbufferResourceID() { return m_BBID; }
  void ReportDeath(ID3D11DeviceChild *obj);
  void FlushPendingDead();
  void DestroyDeadObject(ID3D11DeviceChild *child);
  void Resurrect(ID3D11DeviceChild *obj);

  ////////////////////////////////////////////////////////////////
  // Functions for D3D9 hooks to call into (D3DPERF api)

  static void SetMarker(uint32_t col, const wchar_t *name);
  static int BeginEvent(uint32_t col, const wchar_t *name);
  static int EndEvent();

  //////////////////////////////
  // implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef()
  {
    Atomic::Inc32(&m_RefCount);
    return (ULONG)m_RefCount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    Atomic::Dec32(&m_RefCount);
    ASSERT_REFCOUNT(m_RefCount);
    if(m_RefCount == 0)
    {
      FlushPendingDead();
      delete this;
      return 0;
    }
    return (ULONG)m_RefCount;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D11Device

  template <typename TexDesc>
  TextureDisplayType DispTypeForTexture(TexDesc &Descriptor)
  {
    TextureDisplayType dispType = TEXDISPLAY_SRV_COMPATIBLE;

    if(Descriptor.Usage == D3D11_USAGE_STAGING)
    {
      dispType = TEXDISPLAY_INDIRECT_VIEW;
    }
    else if(IsDepthFormat(Descriptor.Format) || (Descriptor.BindFlags & D3D11_BIND_DEPTH_STENCIL))
    {
      dispType = TEXDISPLAY_DEPTH_TARGET;
    }
    else
    {
      // diverging from perfect reproduction here
      Descriptor.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    return dispType;
  }

  template <typename SerialiserType>
  rdcarray<D3D11_SUBRESOURCE_DATA> Serialise_CreateTextureData(
      SerialiserType &ser, ID3D11Resource *tex, ResourceId id, const D3D11_SUBRESOURCE_DATA *data,
      UINT w, UINT h, UINT d, DXGI_FORMAT fmt, UINT mips, UINT arr, bool HasData);

  HRESULT STDMETHODCALLTYPE OpenSharedResourceInternal(D3D11Chunk chunkType,
                                                       REFIID ReturnedInterface, void **ppResource);

// this is defined as a macro so that we can re-use it to explicitly instantiate these functions as
// templates in the wrapper definition file.
#define SERIALISED_ID3D11DEVICE_FUNCTIONS()                                                         \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateBuffer, const D3D11_BUFFER_DESC *pDesc,              \
      const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer);                         \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateTexture1D, const D3D11_TEXTURE1D_DESC *pDesc,        \
      const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D);                   \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateTexture2D, const D3D11_TEXTURE2D_DESC *pDesc,        \
      const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D);                   \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateTexture3D, const D3D11_TEXTURE3D_DESC *pDesc,        \
      const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D);                   \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateShaderResourceView, ID3D11Resource *pResource,       \
      const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppSRView);           \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateUnorderedAccessView, ID3D11Resource *pResource,      \
      const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc, ID3D11UnorderedAccessView **ppUAView);         \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateRenderTargetView, ID3D11Resource *pResource,         \
      const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRTView);               \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateDepthStencilView, ID3D11Resource *pResource,         \
      const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView);     \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateInputLayout,               \
                                const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,                 \
                                UINT NumElements, const void *pShaderBytecodeWithInputSignature,    \
                                SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout);          \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateVertexShader,              \
                                const void *pShaderBytecode, SIZE_T BytecodeLength,                 \
                                ID3D11ClassLinkage *pClassLinkage,                                  \
                                ID3D11VertexShader **ppVertexShader);                               \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateGeometryShader,            \
                                const void *pShaderBytecode, SIZE_T BytecodeLength,                 \
                                ID3D11ClassLinkage *pClassLinkage,                                  \
                                ID3D11GeometryShader **ppGeometryShader);                           \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateGeometryShaderWithStreamOutput,                      \
      const void *pShaderBytecode, SIZE_T BytecodeLength,                                           \
      const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries,                            \
      const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream,                           \
      ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader);                  \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreatePixelShader, const void *pShaderBytecode,            \
      SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader); \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CreateHullShader, const void *pShaderBytecode,             \
      SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader);   \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDomainShader,              \
                                const void *pShaderBytecode, SIZE_T BytecodeLength,                 \
                                ID3D11ClassLinkage *pClassLinkage,                                  \
                                ID3D11DomainShader **ppDomainShader);                               \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateComputeShader,             \
                                const void *pShaderBytecode, SIZE_T BytecodeLength,                 \
                                ID3D11ClassLinkage *pClassLinkage,                                  \
                                ID3D11ComputeShader **ppComputeShader);                             \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateClassLinkage,              \
                                ID3D11ClassLinkage **ppLinkage);                                    \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateBlendState,                \
                                const D3D11_BLEND_DESC *pBlendStateDesc,                            \
                                ID3D11BlendState **ppBlendState);                                   \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDepthStencilState,         \
                                const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,                  \
                                ID3D11DepthStencilState **ppDepthStencilState);                     \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRasterizerState,           \
                                const D3D11_RASTERIZER_DESC *pRasterizerDesc,                       \
                                ID3D11RasterizerState **ppRasterizerState);                         \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateSamplerState,              \
                                const D3D11_SAMPLER_DESC *pSamplerDesc,                             \
                                ID3D11SamplerState **ppSamplerState);                               \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateQuery,                     \
                                const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery);         \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreatePredicate,                 \
                                const D3D11_QUERY_DESC *pPredicateDesc,                             \
                                ID3D11Predicate **ppPredicate);                                     \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateCounter,                   \
                                const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter); \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDeferredContext,           \
                                UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext);        \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedResource,              \
                                HANDLE hResource, REFIID ReturnedInterface, void **ppResource);     \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckFormatSupport,              \
                                DXGI_FORMAT Format, UINT *pFormatSupport);                          \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckMultisampleQualityLevels,   \
                                DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels);     \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CheckCounterInfo,                   \
                                D3D11_COUNTER_INFO *pCounterInfo);                                  \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                    \
      virtual HRESULT STDMETHODCALLTYPE, CheckCounter, const D3D11_COUNTER_DESC *pDesc,             \
      D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength,            \
      LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength);            \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CheckFeatureSupport,             \
                                D3D11_FEATURE Feature, void *pFeatureSupportData,                   \
                                UINT FeatureSupportDataSize);                                       \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetPrivateData, REFGUID guid,    \
                                UINT *pDataSize, void *pData);                                      \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetPrivateData, REFGUID guid,    \
                                UINT DataSize, const void *pData);                                  \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetPrivateDataInterface,         \
                                REFGUID guid, const IUnknown *pData);                               \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual D3D_FEATURE_LEVEL STDMETHODCALLTYPE, GetFeatureLevel);      \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetCreationFlags);                  \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetDeviceRemovedReason);         \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GetImmediateContext,                \
                                ID3D11DeviceContext **ppImmediateContext);                          \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetExceptionMode,                \
                                UINT RaiseFlags);                                                   \
                                                                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetExceptionMode);

  SERIALISED_ID3D11DEVICE_FUNCTIONS();

//////////////////////////////
// implement ID3D11Device1

// this is defined as a macro so that we can re-use it to explicitly instantiate these functions as
// templates in the wrapper definition file.
#define SERIALISED_ID3D11DEVICE1_FUNCTIONS()                                                     \
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, GetImmediateContext1,            \
                                ID3D11DeviceContext1 **ppImmediateContext);                      \
                                                                                                 \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDeferredContext1,       \
                                UINT ContextFlags, ID3D11DeviceContext1 **ppDeferredContext);    \
                                                                                                 \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateBlendState1,            \
                                const D3D11_BLEND_DESC1 *pBlendStateDesc,                        \
                                ID3D11BlendState1 **ppBlendState);                               \
                                                                                                 \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRasterizerState1,       \
                                const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,                   \
                                ID3D11RasterizerState1 **ppRasterizerState);                     \
                                                                                                 \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateDeviceContextState,     \
                                UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels,             \
                                UINT FeatureLevels, UINT SDKVersion, REFIID EmulatedInterface,   \
                                D3D_FEATURE_LEVEL *pChosenFeatureLevel,                          \
                                ID3DDeviceContextState **ppContextState);                        \
                                                                                                 \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedResource1,          \
                                HANDLE hResource, REFIID returnedInterface, void **ppResource);  \
                                                                                                 \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, OpenSharedResourceByName,     \
                                LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface, \
                                void **ppResource);

  SERIALISED_ID3D11DEVICE1_FUNCTIONS();

  //////////////////////////////
  // implement ID3D11Device2

  virtual void STDMETHODCALLTYPE GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext);

  virtual HRESULT STDMETHODCALLTYPE CreateDeferredContext2(UINT ContextFlags,
                                                           ID3D11DeviceContext2 **ppDeferredContext);

  virtual void STDMETHODCALLTYPE GetResourceTiling(
      ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource,
      D3D11_PACKED_MIP_DESC *pPackedMipDesc, D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
      UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
      D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips);

  virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels1(DXGI_FORMAT Format,
                                                                   UINT SampleCount, UINT Flags,
                                                                   UINT *pNumQualityLevels);

//////////////////////////////
// implement ID3D11Device3

// this is defined as a macro so that we can re-use it to explicitly instantiate these functions as
// templates in the wrapper definition file.
#define SERIALISED_ID3D11DEVICE3_FUNCTIONS()                                                    \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                \
      virtual HRESULT STDMETHODCALLTYPE, CreateTexture2D1, const D3D11_TEXTURE2D_DESC1 *pDesc1, \
      const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D1 **ppTexture2D);              \
                                                                                                \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                \
      virtual HRESULT STDMETHODCALLTYPE, CreateTexture3D1, const D3D11_TEXTURE3D_DESC1 *pDesc1, \
      const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D1 **ppTexture3D);              \
                                                                                                \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateRasterizerState2,      \
                                const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,                  \
                                ID3D11RasterizerState2 **ppRasterizerState);                    \
                                                                                                \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                \
      virtual HRESULT STDMETHODCALLTYPE, CreateShaderResourceView1, ID3D11Resource *pResource,  \
      const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1, ID3D11ShaderResourceView1 **ppSRView1);   \
                                                                                                \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                \
      virtual HRESULT STDMETHODCALLTYPE, CreateUnorderedAccessView1, ID3D11Resource *pResource, \
      const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1, ID3D11UnorderedAccessView1 **ppUAView1); \
                                                                                                \
  IMPLEMENT_FUNCTION_SERIALISED(                                                                \
      virtual HRESULT STDMETHODCALLTYPE, CreateRenderTargetView1, ID3D11Resource *pResource,    \
      const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1, ID3D11RenderTargetView1 **ppRTView1);       \
                                                                                                \
  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, CreateQuery1,                \
                                const D3D11_QUERY_DESC1 *pQueryDesc1, ID3D11Query1 **ppQuery1);

  SERIALISED_ID3D11DEVICE3_FUNCTIONS();

  virtual void STDMETHODCALLTYPE GetImmediateContext3(ID3D11DeviceContext3 **ppImmediateContext);

  virtual HRESULT STDMETHODCALLTYPE CreateDeferredContext3(UINT ContextFlags,
                                                           ID3D11DeviceContext3 **ppDeferredContext);

  virtual void STDMETHODCALLTYPE WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                                                    const D3D11_BOX *pDstBox, const void *pSrcData,
                                                    UINT SrcRowPitch, UINT SrcDepthPitch);

  virtual void STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch,
                                                     UINT DstDepthPitch, ID3D11Resource *pSrcResource,
                                                     UINT SrcSubresource, const D3D11_BOX *pSrcBox);

  //////////////////////////////
  // implement ID3D11Device4

  virtual HRESULT STDMETHODCALLTYPE RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie);

  virtual void STDMETHODCALLTYPE UnregisterDeviceRemoved(DWORD dwCookie);

  //////////////////////////////
  // implement ID3D11Device5

  virtual HRESULT STDMETHODCALLTYPE CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags,
                                                REFIID riid, void **ppFence);

  virtual HRESULT STDMETHODCALLTYPE OpenSharedFence(HANDLE hFence, REFIID riid, void **ppFence);
};
