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

#include <list>
#include "common/wrapped_pool.h"
#include "d3d12_common.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

class WrappedID3D12CommandQueue;

struct DummyID3D12DebugCommandQueue : public ID3D12DebugCommandQueue
{
  WrappedID3D12CommandQueue *m_pQueue;
  ID3D12DebugCommandQueue *m_pReal;

  DummyID3D12DebugCommandQueue() {}
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
      m_pReal->AssertResourceState(pResource, Subresource, State);
    return TRUE;
  }
};

struct D3D12DrawcallTreeNode
{
  D3D12DrawcallTreeNode() {}
  explicit D3D12DrawcallTreeNode(const FetchDrawcall &d) : draw(d) {}
  FetchDrawcall draw;
  vector<D3D12DrawcallTreeNode> children;

  vector<pair<ResourceId, EventUsage> > resourceUsage;

  D3D12DrawcallTreeNode &operator=(const FetchDrawcall &d)
  {
    *this = D3D12DrawcallTreeNode(d);
    return *this;
  }

  vector<FetchDrawcall> Bake()
  {
    vector<FetchDrawcall> ret;
    if(children.empty())
      return ret;

    ret.resize(children.size());
    for(size_t i = 0; i < children.size(); i++)
    {
      ret[i] = children[i].draw;
      ret[i].children = children[i].Bake();
    }

    return ret;
  }
};

class WrappedID3D12GraphicsCommandList;

class WrappedID3D12CommandQueue : public ID3D12CommandQueue,
                                  public RefCounter12<ID3D12CommandQueue>,
                                  public ID3DDevice
{
  WrappedID3D12Device *m_pDevice;

  WrappedID3D12GraphicsCommandList *m_ReplayList;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_QueueRecord;

  Serialiser *m_pSerialiser;
  LogState &m_State;

  DummyID3D12DebugCommandQueue m_DummyDebug;

  vector<D3D12ResourceRecord *> m_CmdListRecords;

  vector<FetchAPIEvent> m_RootEvents, m_Events;
  bool m_AddedDrawcall;

  uint64_t m_CurChunkOffset;
  uint32_t m_RootEventID, m_RootDrawcallID;
  uint32_t m_FirstEventID, m_LastEventID;

  D3D12DrawcallTreeNode m_ParentDrawcall;

  ResourceId m_BackbufferID;

  void InsertDrawsAndRefreshIDs(vector<D3D12DrawcallTreeNode> &cmdBufNodes, uint32_t baseEventID,
                                uint32_t baseDrawID);

  struct BakedCmdListInfo
  {
    vector<FetchAPIEvent> curEvents;
    vector<DebugMessage> debugMessages;
    std::list<D3D12DrawcallTreeNode *> drawStack;

    vector<pair<ResourceId, EventUsage> > resourceUsage;

    struct CmdListState
    {
      ResourceId pipeline;

      uint32_t idxWidth;
      ResourceId ibuffer;
      vector<ResourceId> vbuffers;

      ResourceId rts[8];
      ResourceId dsv;
    } state;

    vector<D3D12_RESOURCE_BARRIER> barriers;

    D3D12DrawcallTreeNode *draw;    // the root draw to copy from when submitting
    uint32_t eventCount;            // how many events are in this cmd list, for quick skipping
    uint32_t curEventID;            // current event ID while reading or executing
    uint32_t drawCount;             // similar to above
  };
  map<ResourceId, BakedCmdListInfo> m_BakedCmdListInfo;

  // on replay, the current command list for the last chunk we
  // handled.
  ResourceId m_LastCmdListID;
  int m_CmdListsInProgress;

  // this is a list of uint64_t file offset -> uint32_t EIDs of where each
  // drawcall is used. E.g. the drawcall at offset 873954 is EID 50. If a
  // command list is executed more than once, there may be more than
  // one entry here - the drawcall will be aliased among several EIDs, with
  // the first one being the 'primary'
  struct DrawcallUse
  {
    DrawcallUse(uint64_t offs, uint32_t eid) : fileOffset(offs), eventID(eid) {}
    uint64_t fileOffset;
    uint32_t eventID;
    bool operator<(const DrawcallUse &o) const
    {
      if(fileOffset != o.fileOffset)
        return fileOffset < o.fileOffset;
      return eventID < o.eventID;
    }
  };
  vector<DrawcallUse> m_DrawcallUses;

  struct PartialReplayData
  {
    // if we're doing a partial replay, by definition only one command
    // list will be partial at any one time. While replaying through
    // the command list chunks, the partial command list will be
    // created as a temporary new command list and when it comes to
    // the queue that should execute it, it can execute this instead.
    ID3D12CommandAllocator *resultPartialCmdAllocator;
    ID3D12GraphicsCommandList *resultPartialCmdList;

    // if we're replaying just a single draw or a particular command
    // list subsection of command events, we don't go through the
    // whole original command lists to set up the partial replay,
    // so we just set this command list
    ID3D12GraphicsCommandList *outsideCmdList;

    // this records where in the frame a command list was executed,
    // so that we know if our replay range ends in one of these ranges
    // we need to construct a partial command list for future
    // replaying. Note that we always have the complete command list
    // around - it's the bakeID itself.
    // Since we only ever record a bakeID once the key is unique - note
    // that the same command list could be reset multiple times
    // a frame, so the parent command list ID (the one recorded in
    // CmdList chunks) is NOT unique.
    // However, a single baked command list can be executed multiple
    // times - so we have to have a list of base events
    // Map from bakeID -> vector<baseEventID>
    map<ResourceId, vector<uint32_t> > cmdListExecs;

    // This is just the ResourceId of the original parent command list
    // and it's baked id.
    // If we are in the middle of a partial replay - allows fast checking
    // in all CmdList chunks, with the iteration through the above list
    // only in Reset.
    // partialParent gets reset to ResourceId() in the Close so that
    // other baked command lists from the same parent don't pick it up
    // Also reset each overall replay
    ResourceId partialParent;

    // If a partial replay is detected, this records the base of the
    // range. This both allows easily and uniquely identifying it in the
    // executecmdlists, but also allows the recording to 'rebase' the
    // last event ID by subtracting this, to know how far to record
    uint32_t baseEvent;
  } m_PartialReplayData;

  map<ResourceId, ID3D12GraphicsCommandList *> m_RerecordCmds;

  std::list<D3D12DrawcallTreeNode *> m_DrawcallStack;

  std::list<D3D12DrawcallTreeNode *> &GetDrawcallStack()
  {
    if(m_LastCmdListID != ResourceId())
      return m_BakedCmdListInfo[m_LastCmdListID].drawStack;

    return m_DrawcallStack;
  }

  bool ShouldRerecordCmd(ResourceId cmdid);
  bool InRerecordRange(ResourceId cmdid);
  ID3D12GraphicsCommandList *RerecordCmdList(ResourceId cmdid);

  void ProcessChunk(uint64_t offset, D3D12ChunkType context);

  const char *GetChunkName(uint32_t idx) { return m_pDevice->GetChunkName(idx); }
  D3D12ResourceManager *GetResourceManager() { return m_pDevice->GetResourceManager(); }
public:
  static const int AllocPoolCount = 16;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandQueue, AllocPoolCount);

  WrappedID3D12CommandQueue(ID3D12CommandQueue *real, WrappedID3D12Device *device,
                            Serialiser *serialiser, LogState &state);
  virtual ~WrappedID3D12CommandQueue();

  Serialiser *GetSerialiser() { return m_pSerialiser; }
  ResourceId GetResourceID() { return m_ResourceID; }
  ID3D12CommandQueue *GetReal() { return m_pReal; }
  D3D12ResourceRecord *GetResourceRecord() { return m_QueueRecord; }
  WrappedID3D12Device *GetWrappedDevice() { return m_pDevice; }
  const vector<D3D12ResourceRecord *> &GetCmdLists() { return m_CmdListRecords; }
  D3D12DrawcallTreeNode &GetParentDrawcall() { return m_ParentDrawcall; }
  FetchAPIEvent GetEvent(uint32_t eventID);
  uint32_t GetMaxEID() { return m_Events.back().eventID; }
  ResourceId GetBackbufferResourceID() { return m_BackbufferID; }
  void ClearAfterCapture();

  void AddDrawcall(const FetchDrawcall &d, bool hasEvents);
  void AddEvent(D3D12ChunkType type, string description);

  void ReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);

  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D12Resource); }
  virtual IID GetDeviceUUID() { return __uuidof(ID3D12CommandQueue); }
  virtual IUnknown *GetDeviceInterface() { return (ID3D12CommandQueue *)this; }
  // the rest forward to the device
  virtual void FirstFrame(WrappedIDXGISwapChain3 *swapChain) { m_pDevice->FirstFrame(swapChain); }
  virtual void NewSwapchainBuffer(IUnknown *backbuffer)
  {
    m_pDevice->NewSwapchainBuffer(backbuffer);
  }
  virtual void ReleaseSwapchainResources(WrappedIDXGISwapChain3 *swapChain)
  {
    m_pDevice->ReleaseSwapchainResources(swapChain);
  }
  virtual IUnknown *WrapSwapchainBuffer(WrappedIDXGISwapChain3 *swap, DXGI_SWAP_CHAIN_DESC *swapDesc,
                                        UINT buffer, IUnknown *realSurface)
  {
    return m_pDevice->WrapSwapchainBuffer(swap, swapDesc, buffer, realSurface);
  }

  virtual HRESULT Present(WrappedIDXGISwapChain3 *swapChain, UINT SyncInterval, UINT Flags)
  {
    return m_pDevice->Present(swapChain, SyncInterval, Flags);
  }

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
      m_pDevice->SetResourceName(this, (const char *)pData);

    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    string utf8 = StringFormat::Wide2UTF8(Name);
    m_pDevice->SetResourceName(this, utf8.c_str());

    return m_pReal->SetName(Name);
  }

  //////////////////////////////
  // implement ID3D12DeviceChild

  virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    if(riid == __uuidof(ID3D12Device) && ppvDevice)
    {
      *ppvDevice = (ID3D12Device *)m_pDevice;
      m_pDevice->AddRef();
    }
    else if(riid != __uuidof(ID3D12Device))
    {
      return E_NOINTERFACE;
    }

    return E_INVALIDARG;
  }

  //////////////////////////////
  // implement ID3D12CommandQueue

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions,
                         const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
                         const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap,
                         UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags,
                         const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts,
                         D3D12_TILE_MAPPING_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyTileMappings(ID3D12Resource *pDstResource,
                       const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
                       ID3D12Resource *pSrcResource,
                       const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
                       const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ExecuteCommandLists(UINT NumCommandLists,
                                                    ID3D12CommandList *const *ppCommandLists));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetMarker(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                BeginEvent(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndEvent());

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Signal(ID3D12Fence *pFence, UINT64 Value));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Wait(ID3D12Fence *pFence, UINT64 Value));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetTimestampFrequency(UINT64 *pFrequency));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp));

  virtual D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
};

template <>
ID3D12CommandQueue *Unwrap(ID3D12CommandQueue *obj);

template <>
ResourceId GetResID(ID3D12CommandQueue *obj);

template <>
D3D12ResourceRecord *GetRecord(ID3D12CommandQueue *obj);