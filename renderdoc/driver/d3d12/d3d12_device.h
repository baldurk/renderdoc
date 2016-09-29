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

#pragma once

#include <stdint.h>
#include <map>
#include "api/replay/renderdoc_replay.h"
#include "common/threading.h"
#include "common/timing.h"
#include "common/wrapped_pool.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "replay/replay_driver.h"
#include "d3d12_common.h"
#include "d3d12_debug.h"
#include "d3d12_manager.h"
#include "d3d12_replay.h"

struct D3D12InitParams : public RDCInitParams
{
  D3D12InitParams();
  ReplayCreateStatus Serialise();

  D3D_FEATURE_LEVEL MinimumFeatureLevel;

  static const uint32_t D3D12_SERIALISE_VERSION = 0x0000001;

  // version number internal to d3d12 stream
  uint32_t SerialiseVersion;
};

class WrappedID3D12Device;

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
struct WrappedID3D12DebugDevice : public ID3D12DebugDevice
{
  WrappedID3D12Device *m_pDevice;
  ID3D12DebugDevice *m_pDebug;

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
    return m_pDebug->ReportLiveDeviceObjects(Flags);
  }
};

// give every impression of working but do nothing.
// Same idea as DummyID3D12InfoQueue above, a dummy interface so that users
// expecting a ID3D12DebugDevice don't get confused if we have turned off the debug
// layer and can't return the real one.
struct DummyID3D12DebugDevice : public ID3D12DebugDevice
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
};

class WrappedID3D12CommandQueue;

class WrappedID3D12Device : public IFrameCapturer, public ID3DDevice, public ID3D12Device
{
private:
  ID3D12Device *m_pDevice;

  WrappedID3D12CommandQueue *m_Queue;

  ID3D12CommandAllocator *m_Alloc;
  ID3D12GraphicsCommandList *m_List;
  ID3D12Fence *m_GPUSyncFence;
  HANDLE m_GPUSyncHandle;
  UINT64 m_GPUSyncCounter;

  void CreateInternalResources();
  void DestroyInternalResources();

  D3D12ResourceManager *m_ResourceManager;
  DummyID3D12InfoQueue m_DummyInfoQueue;
  DummyID3D12DebugDevice m_DummyDebug;
  WrappedID3D12DebugDevice m_WrappedDebug;

  D3D12Replay m_Replay;
  D3D12DebugManager *m_DebugManager;

  void ProcessChunk(uint64_t offset, D3D12ChunkType context);

  unsigned int m_InternalRefcount;
  RefCounter12<ID3D12Device> m_RefCounter;
  RefCounter12<ID3D12Device> m_SoftRefCounter;
  bool m_Alive;

  uint32_t m_FrameCounter;
  vector<FetchFrameInfo> m_CapturedFrames;
  FetchFrameRecord m_FrameRecord;
  vector<FetchDrawcall *> m_Drawcalls;

  PerformanceTimer m_FrameTimer;
  vector<double> m_FrameTimes;
  double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;

  Serialiser *m_pSerialiser;
  bool m_AppControlledCapture;

  Threading::CriticalSection m_CapTransitionLock;
  LogState m_State;

  D3D12InitParams m_InitParams;
  ID3D12InfoQueue *m_pInfoQueue;

  // ensure all calls in via the D3D wrapped interface are thread safe
  // protects wrapped resource creation and serialiser access
  Threading::CriticalSection m_D3DLock;

  D3D12ResourceRecord *m_FrameCaptureRecord;
  Chunk *m_HeaderChunk;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_DeviceRecord;

  // used both on capture and replay side to track resource states. Only locked
  // in capture
  map<ResourceId, SubresourceStateVector> m_ResourceStates;
  Threading::CriticalSection m_ResourceStatesLock;

  map<ResourceId, string> m_ResourceNames;

  struct SwapPresentInfo
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8];

    int32_t lastPresentedBuffer;
  };

  map<WrappedIDXGISwapChain3 *, SwapPresentInfo> m_SwapChains;
  pair<ResourceId, DXGI_FORMAT> m_BackbufferFormat;

  WrappedIDXGISwapChain3 *m_LastSwap;

  UINT m_DescriptorIncrements[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

  void Serialise_CaptureScope(uint64_t offset);
  void EndCaptureFrame(ID3D12Resource *presentImage);

public:
  static const int AllocPoolCount = 4;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Device, AllocPoolCount);

  WrappedID3D12Device(ID3D12Device *realDevice, D3D12InitParams *params);
  virtual ~WrappedID3D12Device();

  UINT GetUnwrappedDescriptorIncrement(D3D12_DESCRIPTOR_HEAP_TYPE type)
  {
    return m_DescriptorIncrements[type];
  }

  ////////////////////////////////////////////////////////////////
  // non wrapping interface

  ID3D12Device *GetReal() { return m_pDevice; }
  static const char *GetChunkName(uint32_t idx);
  D3D12ResourceManager *GetResourceManager() { return m_ResourceManager; }
  D3D12DebugManager *GetDebugManager() { return m_DebugManager; }
  Serialiser *GetSerialiser() { return m_pSerialiser; }
  ResourceId GetResourceID() { return m_ResourceID; }
  Threading::CriticalSection &GetCapTransitionLock() { return m_CapTransitionLock; }
  void ReleaseSwapchainResources(IDXGISwapChain *swap, IUnknown **backbuffers, int numBackbuffers);
  void FirstFrame(WrappedIDXGISwapChain3 *swap);
  FetchFrameRecord &GetFrameRecord() { return m_FrameRecord; }
  const FetchDrawcall *GetDrawcall(uint32_t eventID);

  const string &GetResourceName(ResourceId id) { return m_ResourceNames[id]; }
  vector<D3D12_RESOURCE_STATES> &GetSubresourceStates(ResourceId id)
  {
    return m_ResourceStates[id];
  }
  const map<ResourceId, SubresourceStateVector> &GetSubresourceStates() { return m_ResourceStates; }
  const pair<ResourceId, DXGI_FORMAT> GetBackbufferFormat() { return m_BackbufferFormat; }
  void SetLogFile(const char *logfile);
  void SetLogVersion(uint32_t fileversion) { m_InitParams.SerialiseVersion = fileversion; }
  D3D12Replay *GetReplay() { return &m_Replay; }
  WrappedID3D12CommandQueue *GetQueue() { return m_Queue; }
  ID3D12CommandAllocator *GetAlloc() { return m_Alloc; }
  void ApplyBarriers(vector<D3D12_RESOURCE_BARRIER> &barriers);

  struct
  {
    void Reset()
    {
      freecmds.clear();
      pendingcmds.clear();
      submittedcmds.clear();
    }

    vector<ID3D12GraphicsCommandList *> freecmds;
    // -> GetNextCmd() ->
    vector<ID3D12GraphicsCommandList *> pendingcmds;
    // -> ExecuteLists() ->
    vector<ID3D12GraphicsCommandList *> submittedcmds;
    // -> FlushLists()--------back to freecmds--------^
  } m_InternalCmds;

  ID3D12GraphicsCommandList *GetNewList();
  void ExecuteLists();
  void FlushLists(bool forceSync = false);

  void GPUSync();

  void StartFrameCapture(void *dev, void *wnd);
  bool EndFrameCapture(void *dev, void *wnd);

  bool Serialise_BeginCaptureFrame(bool applyInitialState);

  void ReadLogInitialisation();
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);

  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D12Resource); }
  virtual IID GetDeviceUUID() { return __uuidof(ID3D12Device); }
  virtual IUnknown *GetDeviceInterface() { return (ID3D12Device *)this; }
  // Swap Chain
  IMPLEMENT_FUNCTION_SERIALISED(IUnknown *, WrapSwapchainBuffer(WrappedIDXGISwapChain3 *swap,
                                                                DXGI_SWAP_CHAIN_DESC *desc,
                                                                UINT buffer, IUnknown *realSurface));
  HRESULT Present(WrappedIDXGISwapChain3 *swap, UINT SyncInterval, UINT Flags);

  void NewSwapchainBuffer(IUnknown *backbuffer) {}
  void ReleaseSwapchainResources(WrappedIDXGISwapChain3 *swap);

  void InternalRef() { InterlockedIncrement(&m_InternalRefcount); }
  void InternalRelease() { InterlockedDecrement(&m_InternalRefcount); }
  void SoftRef() { m_SoftRefCounter.AddRef(); }
  void SoftRelease()
  {
    m_SoftRefCounter.Release();
    CheckForDeath();
  }
  void CheckForDeath();

  // Resource
  IMPLEMENT_FUNCTION_SERIALISED(void, SetResourceName(ID3D12DeviceChild *res, const char *name));
  IMPLEMENT_FUNCTION_SERIALISED(HRESULT,
                                SetShaderDebugPath(ID3D12DeviceChild *res, const char *name));
  IMPLEMENT_FUNCTION_SERIALISED(void, ReleaseResource(ID3D12DeviceChild *res));

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
  IMPLEMENT_FUNCTION_SERIALISED(virtual UINT STDMETHODCALLTYPE, GetNodeCount());

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc,
                                                   REFIID riid, void **ppCommandQueue));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid,
                                                       void **ppCommandAllocator));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual HRESULT STDMETHODCALLTYPE,
      CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid,
                                  void **ppPipelineState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
                                                           REFIID riid, void **ppPipelineState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                                                  ID3D12CommandAllocator *pCommandAllocator,
                                                  ID3D12PipelineState *pInitialState, REFIID riid,
                                                  void **ppCommandList));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData,
                                                    UINT FeatureSupportDataSize));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc,
                                                     REFIID riid, void **ppvHeap));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual UINT STDMETHODCALLTYPE,
      GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature,
                                                    SIZE_T blobLengthInBytes, REFIID riid,
                                                    void **ppvRootSignature));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
                                                         D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CreateShaderResourceView(ID3D12Resource *pResource,
                                                         const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                         D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource,
                                const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CreateRenderTargetView(ID3D12Resource *pResource,
                                                       const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,
                                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CreateDepthStencilView(ID3D12Resource *pResource,
                                                       const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                       D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CreateSampler(const D3D12_SAMPLER_DESC *pDesc,
                                              D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyDescriptors(UINT NumDestDescriptorRanges,
                      const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts,
                      const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
                      const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts,
                      const UINT *pSrcDescriptorRangeSizes,
                      D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyDescriptorsSimple(UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
                            D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
                            D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType));

  IMPLEMENT_FUNCTION_SERIALISED(virtual D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE,
                                GetResourceAllocationInfo(UINT visibleMask, UINT numResourceDescs,
                                                          const D3D12_RESOURCE_DESC *pResourceDescs));

  IMPLEMENT_FUNCTION_SERIALISED(virtual D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE,
                                GetCustomHeapProperties(UINT nodeMask, D3D12_HEAP_TYPE heapType));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                                        D3D12_HEAP_FLAGS HeapFlags,
                                                        const D3D12_RESOURCE_DESC *pDesc,
                                                        D3D12_RESOURCE_STATES InitialResourceState,
                                                        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                        REFIID riidResource, void **ppvResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                     const D3D12_RESOURCE_DESC *pDesc,
                                                     D3D12_RESOURCE_STATES InitialState,
                                                     const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                     REFIID riid, void **ppvResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc,
                                                       D3D12_RESOURCE_STATES InitialState,
                                                       const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                       REFIID riid, void **ppvResource));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateSharedHandle(ID3D12DeviceChild *pObject,
                                                   const SECURITY_ATTRIBUTES *pAttributes,
                                                   DWORD Access, LPCWSTR Name, HANDLE *pHandle));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags,
                                            REFIID riid, void **ppFence));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetDeviceRemovedReason());

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                GetCopyableFootprints(const D3D12_RESOURCE_DESC *pResourceDesc,
                                                      UINT FirstSubresource, UINT NumSubresources,
                                                      UINT64 BaseOffset,
                                                      D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
                                                      UINT *pNumRows, UINT64 *pRowSizeInBytes,
                                                      UINT64 *pTotalBytes));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                                void **ppvHeap));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, SetStablePowerState(BOOL Enable));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                                       ID3D12RootSignature *pRootSignature,
                                                       REFIID riid, void **ppvCommandSignature));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      GetResourceTiling(ID3D12Resource *pTiledResource, UINT *pNumTilesForEntireResource,
                        D3D12_PACKED_MIP_INFO *pPackedMipDesc,
                        D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
                        UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
                        D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips));

  IMPLEMENT_FUNCTION_SERIALISED(virtual LUID STDMETHODCALLTYPE, GetAdapterLuid());
};