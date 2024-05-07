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

#pragma once

#include "common/wrapped_pool.h"
#include "d3d12_commands.h"
#include "d3d12_common.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

class WrappedID3D12CommandQueue;

// this aren't documented, they're defined in D3D12TranslationLayer in the d3d11on12 codebase
MIDL_INTERFACE("7974c836-9520-4cda-8d43-d996622e8926")
ID3D12CompatibilityQueue : public IUnknown
{
public:
  virtual HRESULT STDMETHODCALLTYPE AcquireKeyedMutex(
      _In_ ID3D12Object * pHeapOrResourceWithKeyedMutex, UINT64 Key, DWORD dwTimeout,
      _Reserved_ void *pReserved, _In_range_(0, 0) UINT Reserved) = 0;

  virtual HRESULT STDMETHODCALLTYPE ReleaseKeyedMutex(
      _In_ ID3D12Object * pHeapOrResourceWithKeyedMutex, UINT64 Key, _Reserved_ void *pReserved,
      _In_range_(0, 0) UINT Reserved) = 0;
};

struct WrappedID3D12CompatibilityQueue : public ID3D12CompatibilityQueue
{
  WrappedID3D12CommandQueue &m_pQueue;
  ID3D12CompatibilityQueue *m_pReal = NULL;

  WrappedID3D12CompatibilityQueue(WrappedID3D12CommandQueue &dev) : m_pQueue(dev) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12CompatibilityQueue
  virtual HRESULT STDMETHODCALLTYPE AcquireKeyedMutex(_In_ ID3D12Object *pHeapOrResourceWithKeyedMutex,
                                                      UINT64 Key, DWORD dwTimeout,
                                                      _Reserved_ void *pReserved,
                                                      _In_range_(0, 0) UINT Reserved);

  virtual HRESULT STDMETHODCALLTYPE ReleaseKeyedMutex(_In_ ID3D12Object *pHeapOrResourceWithKeyedMutex,
                                                      UINT64 Key, _Reserved_ void *pReserved,
                                                      _In_range_(0, 0) UINT Reserved);
};

struct WrappedID3D12DebugCommandQueue : public ID3D12DebugCommandQueue1
{
  WrappedID3D12CommandQueue *m_pQueue = NULL;
  ID3D12DebugCommandQueue *m_pReal = NULL;
  ID3D12DebugCommandQueue1 *m_pReal1 = NULL;

  WrappedID3D12DebugCommandQueue() {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D12DebugCommandQueue))
    {
      *ppvObject = (ID3D12DebugCommandQueue *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12DebugCommandQueue1))
    {
      *ppvObject = (ID3D12DebugCommandQueue1 *)this;
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DebugCommandQueue

  virtual BOOL STDMETHODCALLTYPE AssertResourceState(ID3D12Resource *pResource, UINT Subresource,
                                                     UINT State)
  {
    if(m_pReal)
      return m_pReal->AssertResourceState(Unwrap(pResource), Subresource, State);
    return TRUE;
  }

  //////////////////////////////
  // implement ID3D12DebugCommandQueue1

  virtual void STDMETHODCALLTYPE AssertResourceAccess(ID3D12Resource *pResource, UINT Subresource,
                                                      D3D12_BARRIER_ACCESS Access)
  {
    if(m_pReal1)
      m_pReal1->AssertResourceAccess(Unwrap(pResource), Subresource, Access);
  }

  virtual void STDMETHODCALLTYPE AssertTextureLayout(ID3D12Resource *pResource, UINT Subresource,
                                                     D3D12_BARRIER_LAYOUT Layout)
  {
    if(m_pReal1)
      m_pReal1->AssertTextureLayout(Unwrap(pResource), Subresource, Layout);
  }
};

struct WrappedDownlevelQueue : public ID3D12CommandQueueDownlevel
{
  WrappedID3D12CommandQueue &m_pQueue;

  WrappedDownlevelQueue(WrappedID3D12CommandQueue &q) : m_pQueue(q) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  //////////////////////////////
  // implement ID3D12CommandQueueDownlevel
  virtual HRESULT STDMETHODCALLTYPE Present(ID3D12GraphicsCommandList *pOpenCommandList,
                                            ID3D12Resource *pSourceTex2D, HWND hWindow,
                                            D3D12_DOWNLEVEL_PRESENT_FLAGS Flags);
};

class WrappedID3D12GraphicsCommandList;

class WrappedID3D12CommandQueue : public ID3D12CommandQueue,
                                  public RefCounter12<ID3D12CommandQueue>,
                                  public ID3DDevice,
                                  public IDXGISwapper
{
  friend class WrappedID3D12GraphicsCommandList;

  ID3D12CommandQueueDownlevel *m_pDownlevel;

  WrappedDownlevelQueue m_WrappedDownlevel;

  WrappedID3D12Device *m_pDevice;

  WrappedID3D12GraphicsCommandList *m_ReplayList;

  ID3D12Resource *m_pPresentSource = NULL;
  HWND m_pPresentHWND = NULL;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_QueueRecord, *m_CreationRecord;

  CaptureState &m_State;

  // tracking ray dispatches that are pending during capture, to free them once the execution is finished
  ID3D12Fence *m_RayFence = NULL;
  UINT64 m_RayFenceValue = 1;
  rdcarray<PatchedRayDispatch::Resources> m_RayDispatchesPending;

  ID3D12Fence *GetRayFence();

  bool m_MarkedActive = false;

  WrappedID3D12DebugCommandQueue m_WrappedDebug;
  WrappedID3D12CompatibilityQueue m_WrappedCompat;

  rdcarray<D3D12ResourceRecord *> m_CmdListRecords;
  rdcarray<D3D12ResourceRecord *> m_CmdListAllocators;

  std::unordered_set<ResourceId> m_SparseBindResources;

  // D3D12 guarantees that queues are thread-safe
  Threading::CriticalSection m_Lock;

  std::set<rdcstr> m_StringDB;

  WriteSerialiser &GetThreadSerialiser();

  StreamReader *m_FrameReader = NULL;

  uint64_t m_TimeBase = 0;
  double m_TimeFrequency = 1.0f;
  SDFile *m_StructuredFile = NULL;

  // command recording/replay data shared between queues and lists
  D3D12CommandData m_Cmd;

  ResourceId m_PrevQueueId;

  bool ProcessChunk(ReadSerialiser &ser, D3D12Chunk context);

  static rdcstr GetChunkName(uint32_t idx);
  D3D12ResourceManager *GetResourceManager() { return m_pDevice->GetResourceManager(); }
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandQueue);

  WrappedID3D12CommandQueue(ID3D12CommandQueue *real, WrappedID3D12Device *device,
                            CaptureState &state);
  virtual ~WrappedID3D12CommandQueue();

  ResourceId GetResourceID() { return m_ResourceID; }
  ID3D12CommandQueue *GetReal() { return m_pReal; }
  D3D12ResourceRecord *GetResourceRecord() { return m_QueueRecord; }
  D3D12ResourceRecord *GetCreationRecord() { return m_CreationRecord; }
  WrappedID3D12Device *GetWrappedDevice() { return m_pDevice; }
  const rdcarray<D3D12ResourceRecord *> &GetCmdLists() { return m_CmdListRecords; }
  D3D12ActionTreeNode &GetParentAction() { return m_Cmd.m_ParentAction; }
  const APIEvent &GetEvent(uint32_t eventId);
  uint32_t GetMaxEID() { return m_Cmd.m_Events.back().eventId; }
  void ClearAfterCapture();

  bool IsSparseUpdatedResource(ResourceId id) const
  {
    return m_SparseBindResources.find(id) != m_SparseBindResources.end();
  }

  void CheckAndFreeRayDispatches();

  RDResult ReplayLog(CaptureState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
  void SetFrameReader(StreamReader *reader) { m_FrameReader = reader; }
  D3D12CommandData *GetCommandData() { return &m_Cmd; }
  const rdcarray<EventUsage> &GetUsage(ResourceId id) { return m_Cmd.m_ResourceUses[id]; }
  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D12Resource); }
  virtual bool IsDeviceUUID(REFIID iid)
  {
    return iid == __uuidof(ID3D12CommandQueue) ? true : false;
  }
  virtual IUnknown *GetDeviceInterface(REFIID iid)
  {
    if(iid == __uuidof(ID3D12CommandQueue))
      return (ID3D12CommandQueue *)this;

    RDCERR("Requested unknown device interface %s", ToStr(iid).c_str());

    return NULL;
  }
  // the rest forward to the device
  virtual void *GetFrameCapturerDevice() { return m_pDevice->GetFrameCapturerDevice(); }
  virtual IFrameCapturer *GetFrameCapturer() { return m_pDevice->GetFrameCapturer(); }
  virtual void FirstFrame(IDXGISwapper *swapper) { m_pDevice->FirstFrame(swapper); }
  virtual void NewSwapchainBuffer(IUnknown *backbuffer)
  {
    m_pDevice->NewSwapchainBuffer(backbuffer);
  }
  virtual void ReleaseSwapchainResources(IDXGISwapper *swapper, UINT QueueCount,
                                         IUnknown *const *ppPresentQueue, IUnknown **unwrappedQueues)
  {
    m_pDevice->ReleaseSwapchainResources(swapper, QueueCount, ppPresentQueue, unwrappedQueues);
  }
  virtual IUnknown *WrapSwapchainBuffer(IDXGISwapper *swapper, DXGI_FORMAT bufferFormat,
                                        UINT buffer, IUnknown *realSurface)
  {
    return m_pDevice->WrapSwapchainBuffer(swapper, bufferFormat, buffer, realSurface);
  }
  virtual IDXGIResource *WrapExternalDXGIResource(IDXGIResource *res)
  {
    return m_pDevice->WrapExternalDXGIResource(res);
  }

  virtual HRESULT Present(IDXGISwapper *swapper, UINT SyncInterval, UINT Flags)
  {
    return m_pDevice->Present(swapper, SyncInterval, Flags);
  }

  // fake pretending to be a swapchain for when we're doing downlevel mega-hacky presents
  virtual ID3DDevice *GetD3DDevice() { return this; }
  virtual int GetNumBackbuffers() { return 1; }
  virtual IUnknown **GetBackbuffers() { return (IUnknown **)&m_pPresentSource; }
  virtual int GetLastPresentedBuffer() { return 0; }
  virtual UINT GetWidth()
  {
    D3D12_RESOURCE_DESC desc = m_pPresentSource->GetDesc();
    return (UINT)desc.Width;
  }
  virtual UINT GetHeight()
  {
    D3D12_RESOURCE_DESC desc = m_pPresentSource->GetDesc();
    return desc.Height;
  }
  virtual DXGI_FORMAT GetFormat()
  {
    D3D12_RESOURCE_DESC desc = m_pPresentSource->GetDesc();
    return desc.Format;
  }
  virtual HWND GetHWND() { return m_pPresentHWND; }
  //////////////////////////////
  // implement IUnknown

  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::SoftRef(m_pDevice); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::SoftRelease(m_pDevice); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D12Object

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
  {
    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
  {
    if(guid == WKPDID_D3DDebugObjectName)
    {
      m_pDevice->SetName(this, (const char *)pData);
    }
    else if(guid == WKPDID_D3DDebugObjectNameW)
    {
      rdcwstr wName((const wchar_t *)pData, DataSize / 2);
      rdcstr sName = StringFormat::Wide2UTF8(wName);
      m_pDevice->SetName(this, sName.c_str());
    }

    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    rdcstr utf8 = StringFormat::Wide2UTF8(Name);
    m_pDevice->SetName(this, utf8.c_str());

    return m_pReal->SetName(Name);
  }

  //////////////////////////////
  // implement ID3D12DeviceChild

  virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    return m_pDevice->GetDevice(riid, ppvDevice);
  }

  //////////////////////////////
  // implement ID3D12CommandQueue

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, UpdateTileMappings,
                                ID3D12Resource *pResource, UINT NumResourceRegions,
                                const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
                                const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap,
                                UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags,
                                const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts,
                                D3D12_TILE_MAPPING_FLAGS Flags);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyTileMappings,
                                ID3D12Resource *pDstResource,
                                const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
                                ID3D12Resource *pSrcResource,
                                const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
                                const D3D12_TILE_REGION_SIZE *pRegionSize,
                                D3D12_TILE_MAPPING_FLAGS Flags);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ExecuteCommandLists,
                                UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists);

  virtual void ExecuteCommandListsInternal(UINT NumCommandLists,
                                           ID3D12CommandList *const *ppCommandLists,
                                           bool InFrameCaptureBoundary, bool SkipRealExecute);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetMarker, UINT Metadata,
                                const void *pData, UINT Size);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, BeginEvent, UINT Metadata,
                                const void *pData, UINT Size);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndEvent, );

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Signal, ID3D12Fence *pFence,
                                UINT64 Value);

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Wait, ID3D12Fence *pFence,
                                UINT64 Value);

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetTimestampFrequency,
                                UINT64 *pFrequency);

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, GetClockCalibration,
                                UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp);

  virtual D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
  //////////////////////////////
  // implement ID3D12CommandQueueDownlevel
  virtual HRESULT STDMETHODCALLTYPE Present(ID3D12GraphicsCommandList *pOpenCommandList,
                                            ID3D12Resource *pSourceTex2D, HWND hWindow,
                                            D3D12_DOWNLEVEL_PRESENT_FLAGS Flags);
};

template <>
ID3D12CommandQueue *Unwrap(ID3D12CommandQueue *obj);

template <>
ResourceId GetResID(ID3D12CommandQueue *obj);

template <>
D3D12ResourceRecord *GetRecord(ID3D12CommandQueue *obj);
