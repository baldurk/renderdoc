/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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
#include "d3d12_manager.h"
#include "d3d12_replay.h"

struct IAmdExtD3DFactory;

struct D3D12InitParams
{
  D3D12InitParams();

  D3D_FEATURE_LEVEL MinimumFeatureLevel;

  // check if a frame capture section version is supported
  static const uint64_t CurrentVersion = 0x4;
  static bool IsSupportedVersion(uint64_t ver);
};

DECLARE_REFLECTION_STRUCT(D3D12InitParams);

class WrappedID3D12Device;
class WrappedID3D12Resource;

class D3D12TextRenderer;
class D3D12ShaderCache;

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

#define IMPLEMENT_FUNCTION_THREAD_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                                     \
  template <typename SerialiserType>                         \
  bool CONCAT(Serialise_, func(SerialiserType &ser, __VA_ARGS__));

class WrappedID3D12Device : public IFrameCapturer, public ID3DDevice, public ID3D12Device1
{
private:
  ID3D12Device *m_pDevice;
  ID3D12Device1 *m_pDevice1;

  // list of all queues being captured
  std::vector<WrappedID3D12CommandQueue *> m_Queues;
  std::vector<ID3D12Fence *> m_QueueFences;

  // the queue we use for all internal work, the first DIRECT queue
  WrappedID3D12CommandQueue *m_Queue;

  ID3D12CommandAllocator *m_Alloc = NULL, *m_DataUploadAlloc = NULL;
  ID3D12GraphicsCommandList *m_List = NULL, *m_DataUploadList = NULL;
  ID3D12DescriptorHeap *m_RTVHeap = NULL;
  ID3D12Fence *m_GPUSyncFence;
  HANDLE m_GPUSyncHandle;
  UINT64 m_GPUSyncCounter;

  D3D12_CPU_DESCRIPTOR_HANDLE AllocRTV();
  void FreeRTV(D3D12_CPU_DESCRIPTOR_HANDLE handle);

  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_FreeRTVs;
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_UsedRTVs;

  void CreateInternalResources();
  void DestroyInternalResources();

  IAmdExtD3DFactory *m_pAMDExtObject = NULL;

  D3D12ResourceManager *m_ResourceManager;
  DummyID3D12InfoQueue m_DummyInfoQueue;
  DummyID3D12DebugDevice m_DummyDebug;
  WrappedID3D12DebugDevice m_WrappedDebug;

  D3D12Replay m_Replay;
  D3D12ShaderCache *m_ShaderCache = NULL;
  D3D12TextRenderer *m_TextRenderer = NULL;

  set<ResourceId> m_UploadResourceIds;
  map<uint64_t, ID3D12Resource *> m_UploadBuffers;

  Threading::CriticalSection m_MapsLock;
  vector<MapState> m_Maps;

  bool ProcessChunk(ReadSerialiser &ser, D3D12Chunk context);

  unsigned int m_InternalRefcount;
  RefCounter12<ID3D12Device> m_RefCounter;
  RefCounter12<ID3D12Device> m_SoftRefCounter;
  bool m_Alive;

  uint64_t threadSerialiserTLSSlot;

  Threading::CriticalSection m_ThreadSerialisersLock;
  std::vector<WriteSerialiser *> m_ThreadSerialisers;

  uint64_t tempMemoryTLSSlot;
  struct TempMem
  {
    TempMem() : memory(NULL), size(0) {}
    byte *memory;
    size_t size;
  };
  Threading::CriticalSection m_ThreadTempMemLock;
  vector<TempMem *> m_ThreadTempMem;

  vector<DebugMessage> m_DebugMessages;

  SDFile *m_StructuredFile = NULL;
  SDFile m_StoredStructuredData;

  uint32_t m_FrameCounter;
  vector<FrameDescription> m_CapturedFrames;
  FrameRecord m_FrameRecord;
  vector<DrawcallDescription *> m_Drawcalls;

  ReplayStatus m_FailedReplayStatus = ReplayStatus::APIReplayFailed;

  bool m_AppControlledCapture;

  Threading::CriticalSection m_CapTransitionLock;
  CaptureState m_State;

  uint32_t m_SubmitCounter = 0;

  D3D12InitParams m_InitParams;
  uint64_t m_SectionVersion;
  ID3D12InfoQueue *m_pInfoQueue;

  D3D12ResourceRecord *m_FrameCaptureRecord;
  Chunk *m_HeaderChunk;

  std::set<std::string> m_StringDB;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_DeviceRecord;

  Threading::CriticalSection m_DynDescLock;
  std::vector<DynamicDescriptorCopy> m_DynamicDescriptorCopies;
  std::vector<DynamicDescriptorWrite> m_DynamicDescriptorWrites;
  std::vector<D3D12Descriptor> m_DynamicDescriptorRefs;

  GPUAddressRangeTracker m_GPUAddresses;

  void FlushPendingDescriptorWrites();

  // used both on capture and replay side to track resource states. Only locked
  // in capture
  map<ResourceId, SubresourceStateVector> m_ResourceStates;
  Threading::CriticalSection m_ResourceStatesLock;

  set<ResourceId> m_Cubemaps;

  map<ResourceId, string> m_ResourceNames;

  struct SwapPresentInfo
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8];

    ID3D12CommandQueue *queue;

    int32_t lastPresentedBuffer;
  };

  map<WrappedIDXGISwapChain4 *, SwapPresentInfo> m_SwapChains;
  map<ResourceId, DXGI_FORMAT> m_BackbufferFormat;

  WrappedIDXGISwapChain4 *m_LastSwap;

  D3D12_FEATURE_DATA_D3D12_OPTIONS m_D3D12Opts;
  UINT m_DescriptorIncrements[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

  template <typename SerialiserType>
  bool Serialise_CaptureScope(SerialiserType &ser);
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

  void RemoveQueue(WrappedID3D12CommandQueue *queue);

  ////////////////////////////////////////////////////////////////
  // non wrapping interface

  APIProperties APIProps;

  WriteSerialiser &GetThreadSerialiser();

  ID3D12Device *GetReal() { return m_pDevice; }
  static std::string GetChunkName(uint32_t idx);
  D3D12ResourceManager *GetResourceManager() { return m_ResourceManager; }
  D3D12ShaderCache *GetShaderCache() { return m_ShaderCache; }
  ResourceId GetResourceID() { return m_ResourceID; }
  Threading::CriticalSection &GetCapTransitionLock() { return m_CapTransitionLock; }
  void ReleaseSwapchainResources(IDXGISwapChain *swap, IUnknown **backbuffers, int numBackbuffers);
  void FirstFrame(WrappedIDXGISwapChain4 *swap);
  FrameRecord &GetFrameRecord() { return m_FrameRecord; }
  const DrawcallDescription *GetDrawcall(uint32_t eventId);

  ResourceId GetFrameCaptureResourceId() { return m_FrameCaptureRecord->GetResourceID(); }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, std::string d);
  void AddDebugMessage(const DebugMessage &msg);
  vector<DebugMessage> GetDebugMessages();

  void AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix);
  void DerivedResource(ResourceId parent, ResourceId child);
  void DerivedResource(ID3D12DeviceChild *parent, ResourceId child);
  void AddResourceCurChunk(ResourceDescription &descr);
  void AddResourceCurChunk(ResourceId id);

  const string &GetResourceName(ResourceId id) { return m_ResourceNames[id]; }
  vector<D3D12_RESOURCE_STATES> &GetSubresourceStates(ResourceId id)
  {
    return m_ResourceStates[id];
  }
  const map<ResourceId, SubresourceStateVector> &GetSubresourceStates() { return m_ResourceStates; }
  const map<ResourceId, DXGI_FORMAT> &GetBackbufferFormats() { return m_BackbufferFormat; }
  void SetLogFile(const char *logfile);
  void SetInitParams(const D3D12InitParams &params, uint64_t sectionVersion)
  {
    m_InitParams = params;
    m_SectionVersion = sectionVersion;
  }
  CaptureState GetState() { return m_State; }
  D3D12Replay *GetReplay() { return &m_Replay; }
  WrappedID3D12CommandQueue *GetQueue() { return m_Queue; }
  ID3D12CommandAllocator *GetAlloc() { return m_Alloc; }
  void ApplyBarriers(vector<D3D12_RESOURCE_BARRIER> &barriers);

  void GetDynamicDescriptorReferences(std::vector<D3D12Descriptor> &refs)
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

    vector<ID3D12GraphicsCommandList *> freecmds;
    // -> GetNextCmd() ->
    vector<ID3D12GraphicsCommandList *> pendingcmds;
    // -> ExecuteLists() ->
    vector<ID3D12GraphicsCommandList *> submittedcmds;
    // -> FlushLists()--------back to freecmds--------^
  } m_InternalCmds;

  // batch this many initial state lists together. Balance between
  // creating fewer temporary lists and making too bloated lists
  static const int initialStateMaxBatch = 100;
  int initStateCurBatch;
  ID3D12GraphicsCommandList *initStateCurList;

  ID3D12GraphicsCommandList *GetNewList();
  ID3D12GraphicsCommandList *GetInitialStateList();
  void CloseInitialStateList();
  ID3D12Resource *GetUploadBuffer(uint64_t chunkOffset, uint64_t byteSize);
  void ApplyInitialContents();

  void AddCaptureSubmission();

  void ExecuteList(ID3D12GraphicsCommandList *list, ID3D12CommandQueue *queue = NULL);
  void ExecuteLists(ID3D12CommandQueue *queue = NULL);
  void FlushLists(bool forceSync = false, ID3D12CommandQueue *queue = NULL);

  void GPUSync(ID3D12CommandQueue *queue = NULL, ID3D12Fence *fence = NULL);
  void GPUSyncAllQueues();

  void StartFrameCapture(void *dev, void *wnd);
  bool EndFrameCapture(void *dev, void *wnd);

  template <typename SerialiserType>
  bool Serialise_BeginCaptureFrame(SerialiserType &ser);

  template <typename SerialiserType>
  bool Serialise_DynamicDescriptorWrite(SerialiserType &ser, const DynamicDescriptorWrite *write);
  template <typename SerialiserType>
  bool Serialise_DynamicDescriptorCopies(SerialiserType &ser,
                                         const std::vector<DynamicDescriptorCopy> &DescriptorCopies);

  ReplayStatus ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  void ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);

  void SetStructuredExport(uint64_t sectionVersion)
  {
    m_SectionVersion = sectionVersion;
    m_State = CaptureState::StructuredExport;
  }
  SDFile &GetStructuredFile() { return *m_StructuredFile; }
  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D12Resource); }
  virtual bool IsDeviceUUID(REFIID iid) { return iid == __uuidof(ID3D12Device) ? true : false; }
  virtual IUnknown *GetDeviceInterface(REFIID iid)
  {
    if(iid == __uuidof(ID3D12Device))
      return (ID3D12Device *)this;

    RDCERR("Requested unknown device interface %s", ToStr(iid).c_str());

    return NULL;
  }
  // Swap Chain
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(IUnknown *, WrapSwapchainBuffer,
                                       WrappedIDXGISwapChain4 *swap, DXGI_SWAP_CHAIN_DESC *desc,
                                       UINT buffer, IUnknown *realSurface);
  HRESULT Present(WrappedIDXGISwapChain4 *swap, UINT SyncInterval, UINT Flags);

  void NewSwapchainBuffer(IUnknown *backbuffer);
  void ReleaseSwapchainResources(WrappedIDXGISwapChain4 *swap, UINT QueueCount,
                                 IUnknown *const *ppPresentQueue, IUnknown **unwrappedQueues);

  void Map(ID3D12Resource *Resource, UINT Subresource);
  void Unmap(ID3D12Resource *Resource, UINT Subresource, byte *mapPtr,
             const D3D12_RANGE *pWrittenRange);

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(void, MapDataWrite, ID3D12Resource *Resource,
                                       UINT Subresource, byte *mapPtr, D3D12_RANGE range);
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(void, WriteToSubresource, ID3D12Resource *Resource,
                                       UINT Subresource, const D3D12_BOX *pDstBox,
                                       const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);

  vector<MapState> GetMaps()
  {
    vector<MapState> ret;
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

  // Resource
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(void, SetName, ID3D12DeviceChild *pResource, const char *Name);
  IMPLEMENT_FUNCTION_THREAD_SERIALISED(HRESULT, SetShaderDebugPath, ID3D12DeviceChild *pResource,
                                       const char *Path);

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

  IMPLEMENT_FUNCTION_THREAD_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetDeviceRemovedReason, );

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
};
