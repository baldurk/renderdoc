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

#include "d3d12_device.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"

WRAPPED_POOL_INST(WrappedID3D12Device);

const char *D3D12ChunkNames[] = {
#undef D3D12_CHUNK_MACRO
#define D3D12_CHUNK_MACRO(enum, string) string,

    D3D12_CHUNKS};

D3D12InitParams::D3D12InitParams()
{
  SerialiseVersion = D3D12_SERIALISE_VERSION;
  MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;
}

ReplayCreateStatus D3D12InitParams::Serialise()
{
  SERIALISE_ELEMENT(uint32_t, ver, D3D12_SERIALISE_VERSION);
  SerialiseVersion = ver;

  if(ver != D3D12_SERIALISE_VERSION)
  {
    RDCERR("Incompatible D3D12 serialise version, expected %d got %d", D3D12_SERIALISE_VERSION, ver);
    return eReplayCreate_APIIncompatibleVersion;
  }

  m_pSerialiser->Serialise("MinimumFeatureLevel", MinimumFeatureLevel);

  return eReplayCreate_Success;
}

const char *WrappedID3D12Device::GetChunkName(uint32_t idx)
{
  if(idx == CREATE_PARAMS)
    return "Create Params";
  if(idx == THUMBNAIL_DATA)
    return "Thumbnail Data";
  if(idx == DRIVER_INIT_PARAMS)
    return "Driver Init Params";
  if(idx == INITIAL_CONTENTS)
    return "Initial Contents";
  if(idx < FIRST_CHUNK_ID || idx >= NUM_D3D12_CHUNKS)
    return "<unknown>";
  return D3D12ChunkNames[idx - FIRST_CHUNK_ID];
}

template <>
string ToStrHelper<false, D3D12ChunkType>::Get(const D3D12ChunkType &el)
{
  return WrappedID3D12Device::GetChunkName(el);
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
  if(riid == __uuidof(ID3D12InfoQueue) || riid == __uuidof(ID3D12DebugDevice) ||
     riid == __uuidof(ID3D12Device))
    return m_pDevice->QueryInterface(riid, ppvObject);

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12DebugDevice *)this;
    AddRef();
    return S_OK;
  }

  RDCWARN("Querying ID3D12DebugDevice for interface: %s", ToStr::Get(riid).c_str());

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
  if(riid == __uuidof(ID3D12InfoQueue) || riid == __uuidof(ID3D12DebugDevice) ||
     riid == __uuidof(ID3D12Device))
    return m_pDevice->QueryInterface(riid, ppvObject);

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12DebugDevice *)this;
    AddRef();
    return S_OK;
  }

  string guid = ToStr::Get(riid);
  RDCWARN("Querying ID3D12DebugDevice for interface: %s", guid.c_str());

  return m_pDebug->QueryInterface(riid, ppvObject);
}

WrappedID3D12Device::WrappedID3D12Device(ID3D12Device *realDevice, D3D12InitParams *params)
    : m_RefCounter(realDevice, false), m_SoftRefCounter(NULL, false), m_pDevice(realDevice)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D12Device));

  for(size_t i = 0; i < ARRAY_COUNT(m_DescriptorIncrements); i++)
    m_DescriptorIncrements[i] =
        realDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

  m_DummyInfoQueue.m_pDevice = this;
  m_DummyDebug.m_pDevice = this;
  m_WrappedDebug.m_pDevice = this;

  m_Replay.SetDevice(this);

  m_AppControlledCapture = false;

  m_FrameCounter = 0;

  m_FrameTimer.Restart();

  m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;

  m_HeaderChunk = NULL;

  m_Alloc = NULL;
  m_List = NULL;
  m_GPUSyncFence = NULL;
  m_GPUSyncHandle = NULL;
  m_GPUSyncCounter = 0;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    m_pSerialiser = NULL;

    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();
  }
  else
  {
    m_State = WRITING_IDLE;
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, true);

    m_pSerialiser->SetDebugText(true);
  }

  m_DebugManager = NULL;
  m_ResourceManager = new D3D12ResourceManager(m_State, m_pSerialiser, this);

  if(m_pSerialiser)
  {
    m_pSerialiser->SetUserData(m_ResourceManager);
    m_pSerialiser->SetChunkNameLookup(&GetChunkName);
  }

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_DeviceRecord = NULL;

  m_Queue = NULL;
  m_LastSwap = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_DeviceRecord->type = Resource_Device;
    m_DeviceRecord->DataInSerialiser = false;
    m_DeviceRecord->SpecialResource = true;
    m_DeviceRecord->Length = 0;

    m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_FrameCaptureRecord->DataInSerialiser = false;
    m_FrameCaptureRecord->SpecialResource = true;
    m_FrameCaptureRecord->Length = 0;

    RenderDoc::Inst().AddDeviceFrameCapturer((ID3D12Device *)this, this);
  }

  realDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void **)&m_pInfoQueue);
  realDevice->QueryInterface(__uuidof(ID3D12DebugDevice), (void **)&m_WrappedDebug.m_pDebug);

  if(m_pInfoQueue)
  {
    if(RenderDoc::Inst().GetCaptureOptions().DebugOutputMute)
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
          // super spammy, mostly just perf warning, and impossible to fix for our cases
          D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
          D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,

          // caused often by an over-declaration in the root signature to match between
          // different shaders, and in some descriptors are entirely skipped. We rely on
          // the user to get this right - if the error is non-fatal, any real problems
          // will be potentially highlighted in the pipeline view
          D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
          D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,
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

  m_InitParams = *params;

  //////////////////////////////////////////////////////////////////////////
  // Compile time asserts

  RDCCOMPILE_ASSERT(ARRAY_COUNT(D3D12ChunkNames) == NUM_D3D12_CHUNKS - FIRST_CHUNK_ID + 1,
                    "Not right number of chunk names");
}

WrappedID3D12Device::~WrappedID3D12Device()
{
  RenderDoc::Inst().RemoveDeviceFrameCapturer((ID3D12Device *)this);

  DestroyInternalResources();

  if(m_DeviceRecord)
  {
    RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
    m_DeviceRecord->Delete(GetResourceManager());
  }

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  SAFE_RELEASE(m_pInfoQueue);
  SAFE_RELEASE(m_WrappedDebug.m_pDebug);
  SAFE_RELEASE(m_pDevice);

  SAFE_DELETE(m_pSerialiser);

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

HRESULT WrappedID3D12Device::QueryInterface(REFIID riid, void **ppvObject)
{
  // DEFINE_GUID(IID_IDirect3DDevice9, 0xd0223b96, 0xbf7a, 0x43fd, 0x92, 0xbd, 0xa4, 0x3b, 0xd,
  // 0x82, 0xb9, 0xeb);
  static const GUID IDirect3DDevice9_uuid = {
      0xd0223b96, 0xbf7a, 0x43fd, {0x92, 0xbd, 0xa4, 0x3b, 0xd, 0x82, 0xb9, 0xeb}};

  // ID3D10Device UUID {9B7E4C0F-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10Device_uuid = {
      0x9b7e4c0f, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  // RenderDoc UUID {A7AA6116-9C8D-4BBA-9083-B4D816B71B78}
  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

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
      *ppvObject = (IDXGIDevice *)(new WrappedIDXGIDevice(real, this));
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
      *ppvObject = (IDXGIDevice1 *)(new WrappedIDXGIDevice1(real, this));
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
      *ppvObject = (IDXGIDevice2 *)(new WrappedIDXGIDevice2(real, this));
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
      *ppvObject = (IDXGIDevice3 *)(new WrappedIDXGIDevice3(real, this));
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
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
  else if(riid == IRenderDoc_uuid)
  {
    AddRef();
    *ppvObject = (IUnknown *)this;
    return S_OK;
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying ID3D12Device for interface: %s", guid.c_str());
  }

  return m_RefCounter.QueryInterface(riid, ppvObject);
}

void WrappedID3D12Device::CheckForDeath()
{
  if(!m_Alive)
    return;

  if(m_RefCounter.GetRefCount() == 0)
  {
    RDCASSERT(m_SoftRefCounter.GetRefCount() >= m_InternalRefcount);

    if(m_SoftRefCounter.GetRefCount() <= m_InternalRefcount || m_State < WRITING)    // MEGA HACK
    {
      m_Alive = false;
      delete this;
    }
  }
}

void WrappedID3D12Device::FirstFrame(WrappedIDXGISwapChain3 *swap)
{
  DXGI_SWAP_CHAIN_DESC swapdesc;
  swap->GetDesc(&swapdesc);

  // if we have to capture the first frame, begin capturing immediately
  if(m_State == WRITING_IDLE && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture((ID3D12Device *)this, swapdesc.OutputWindow);

    m_AppControlledCapture = false;
  }
}

void WrappedID3D12Device::ApplyBarriers(vector<D3D12_RESOURCE_BARRIER> &barriers)
{
  SCOPED_LOCK(m_ResourceStatesLock);
  GetResourceManager()->ApplyBarriers(barriers, m_ResourceStates);
}

void WrappedID3D12Device::ReleaseSwapchainResources(WrappedIDXGISwapChain3 *swap)
{
  for(int i = 0; i < swap->GetNumBackbuffers(); i++)
  {
    WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)swap->GetBackbuffers()[i];
    SAFE_RELEASE(wrapped);
  }

  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC desc;
    swap->GetDesc(&desc);

    Keyboard::RemoveInputWindow(desc.OutputWindow);

    RenderDoc::Inst().RemoveFrameCapturer((ID3D12Device *)this, desc.OutputWindow);
  }

  auto it = m_SwapChains.find(swap);
  if(it != m_SwapChains.end())
  {
    for(int i = 0; i < swap->GetNumBackbuffers(); i++)
      GetDebugManager()->FreeRTV(it->second.rtvs[i]);

    m_SwapChains.erase(it);
  }
}

bool WrappedID3D12Device::Serialise_WrapSwapchainBuffer(WrappedIDXGISwapChain3 *swap,
                                                        DXGI_SWAP_CHAIN_DESC *swapDesc, UINT buffer,
                                                        IUnknown *realSurface)
{
  WrappedID3D12Resource *pRes = (WrappedID3D12Resource *)realSurface;

  SERIALISE_ELEMENT(DXGI_FORMAT, swapFormat, swapDesc->BufferDesc.Format);
  SERIALISE_ELEMENT(uint32_t, BuffNum, buffer);
  SERIALISE_ELEMENT(ResourceId, TexID, GetResID(pRes));

  SERIALISE_ELEMENT(D3D12_RESOURCE_DESC, Descriptor, pRes->GetDesc());

  if(m_State < WRITING)
  {
    ID3D12Resource *fakeBB = NULL;

    // DXGI swap chain back buffers can be freely cast as a special-case.
    // translate the format to a typeless format to allow for this.
    // the original type is stored separately below
    Descriptor.Format = GetTypelessFormat(Descriptor.Format);

    HRESULT hr = S_OK;

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // create in common, which is the same as present
    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &Descriptor,
                                            D3D12_RESOURCE_STATE_COMMON, NULL,
                                            __uuidof(ID3D12Resource), (void **)&fakeBB);

    if(FAILED(hr))
    {
      RDCERR("Failed to create fake back buffer, HRESULT: 0x%08x", hr);
    }
    else
    {
      WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(fakeBB, this);
      fakeBB = wrapped;

      m_ResourceNames[TexID] = "Swap Chain Buffer";
      fakeBB->SetName(L"Swap Chain Buffer");

      GetResourceManager()->AddLiveResource(TexID, fakeBB);

      m_BackbufferFormat = std::make_pair(wrapped->GetResourceID(), swapFormat);

      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states.resize(1, D3D12_RESOURCE_STATE_PRESENT);
    }
  }

  return true;
}

IUnknown *WrappedID3D12Device::WrapSwapchainBuffer(WrappedIDXGISwapChain3 *swap,
                                                   DXGI_SWAP_CHAIN_DESC *swapDesc, UINT buffer,
                                                   IUnknown *realSurface)
{
  if(GetResourceManager()->HasWrapper((ID3D12DeviceChild *)realSurface))
  {
    ID3D12Resource *tex =
        (ID3D12Resource *)GetResourceManager()->GetWrapper((ID3D12DeviceChild *)realSurface);
    tex->AddRef();

    realSurface->Release();

    return tex;
  }

  ID3D12Resource *pRes = new WrappedID3D12Resource((ID3D12Resource *)realSurface, this);

  ResourceId id = GetResID(pRes);

  // there shouldn't be a resource record for this texture as it wasn't created via
  // Create*Resource
  RDCASSERT(id != ResourceId() && !GetResourceManager()->HasResourceRecord(id));

  if(m_State >= WRITING)
  {
    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    record->type = Resource_Resource;
    record->DataInSerialiser = false;
    record->SpecialResource = true;
    record->Length = 0;

    WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)pRes;

    wrapped->SetResourceRecord(record);

    SCOPED_LOCK(m_D3DLock);

    SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);

    Serialise_WrapSwapchainBuffer(swap, swapDesc, buffer, pRes);

    record->AddChunk(scope.Get());

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[id];

      states.resize(1, D3D12_RESOURCE_STATE_PRESENT);
    }
  }

  if(m_State >= WRITING)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = GetSRGBFormat(swapDesc->BufferDesc.Format);
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    rtv = GetDebugManager()->AllocRTV();

    CreateRenderTargetView(pRes, NULL, rtv);

    m_SwapChains[swap].rtvs[buffer] = rtv;

    // start at -1 so that we know we've never presented before
    m_SwapChains[swap].lastPresentedBuffer = -1;
  }

  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC sdesc;
    swap->GetDesc(&sdesc);

    Keyboard::AddInputWindow(sdesc.OutputWindow);

    RenderDoc::Inst().AddFrameCapturer((ID3D12Device *)this, sdesc.OutputWindow, this);
  }

  return pRes;
}

HRESULT WrappedID3D12Device::Present(WrappedIDXGISwapChain3 *swap, UINT SyncInterval, UINT Flags)
{
  if((Flags & DXGI_PRESENT_TEST) != 0)
    return S_OK;

  if(m_State == WRITING_IDLE)
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  DXGI_SWAP_CHAIN_DESC swapdesc;
  swap->GetDesc(&swapdesc);
  bool activeWindow = RenderDoc::Inst().IsActiveWindow((ID3D12Device *)this, swapdesc.OutputWindow);

  m_LastSwap = swap;

  if(swapdesc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD)
  {
    // discard always presents from 0
    m_SwapChains[swap].lastPresentedBuffer = 0;
  }
  else
  {
    // other modes use each buffer in turn
    m_SwapChains[swap].lastPresentedBuffer++;
    m_SwapChains[swap].lastPresentedBuffer %= swapdesc.BufferCount;
  }

  if(m_State == WRITING_IDLE)
  {
    m_FrameTimes.push_back(m_FrameTimer.GetMilliseconds());
    m_TotalTime += m_FrameTimes.back();
    m_FrameTimer.Restart();

    // update every second
    if(m_TotalTime > 1000.0)
    {
      m_MinFrametime = 10000.0;
      m_MaxFrametime = 0.0;
      m_AvgFrametime = 0.0;

      m_TotalTime = 0.0;

      for(size_t i = 0; i < m_FrameTimes.size(); i++)
      {
        m_AvgFrametime += m_FrameTimes[i];
        if(m_FrameTimes[i] < m_MinFrametime)
          m_MinFrametime = m_FrameTimes[i];
        if(m_FrameTimes[i] > m_MaxFrametime)
          m_MaxFrametime = m_FrameTimes[i];
      }

      m_AvgFrametime /= double(m_FrameTimes.size());

      m_FrameTimes.clear();
    }

    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      SwapPresentInfo &swapInfo = m_SwapChains[swap];
      D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapInfo.rtvs[swapInfo.lastPresentedBuffer];

      GetDebugManager()->SetOutputDimensions(swapdesc.BufferDesc.Width, swapdesc.BufferDesc.Height,
                                             swapdesc.BufferDesc.Format);

      ID3D12GraphicsCommandList *list = GetNewList();

      // buffer will be in common for presentation, transition to render target
      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Transition.pResource =
          (ID3D12Resource *)swap->GetBackbuffers()[swapInfo.lastPresentedBuffer];
      barrier.Transition.Subresource = 0;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

      list->ResourceBarrier(1, &barrier);

      list->OMSetRenderTargets(1, &rtv, FALSE, NULL);

      if(activeWindow)
      {
        vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetCaptureKeys();

        string overlayText = "D3D12. ";

        if(Keyboard::PlatformHasKeyInput())
        {
          for(size_t i = 0; i < keys.size(); i++)
          {
            if(i > 0)
              overlayText += ", ";

            overlayText += ToStr::Get(keys[i]);
          }

          if(!keys.empty())
            overlayText += " to capture.";
        }
        else
        {
          if(RenderDoc::Inst().IsTargetControlConnected())
            overlayText += "Connected by " + RenderDoc::Inst().GetTargetControlUsername() + ".";
          else
            overlayText += "No remote access connection.";
        }

        if(overlay & eRENDERDOC_Overlay_FrameNumber)
        {
          overlayText += StringFormat::Fmt(" Frame: %d.", m_FrameCounter);
        }
        if(overlay & eRENDERDOC_Overlay_FrameRate)
        {
          overlayText += StringFormat::Fmt(" %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)", m_AvgFrametime,
                                           m_MinFrametime, m_MaxFrametime,
                                           // max with 0.01ms so that we don't divide by zero
                                           1000.0f / RDCMAX(0.01, m_AvgFrametime));
        }

        float y = 0.0f;

        if(!overlayText.empty())
        {
          GetDebugManager()->RenderText(list, 0.0f, y, overlayText.c_str());
          y += 1.0f;
        }

        if(overlay & eRENDERDOC_Overlay_CaptureList)
        {
          GetDebugManager()->RenderText(list, 0.0f, y, "%d Captures saved.",
                                        (uint32_t)m_CapturedFrames.size());
          y += 1.0f;

          uint64_t now = Timing::GetUnixTimestamp();
          for(size_t i = 0; i < m_CapturedFrames.size(); i++)
          {
            if(now - m_CapturedFrames[i].captureTime < 20)
            {
              GetDebugManager()->RenderText(list, 0.0f, y, "Captured frame %d.",
                                            m_CapturedFrames[i].frameNumber);
              y += 1.0f;
            }
          }
        }

#if !defined(RELEASE)
        GetDebugManager()->RenderText(list, 0.0f, y, "%llu chunks - %.2f MB", Chunk::NumLiveChunks(),
                                      float(Chunk::TotalMem()) / 1024.0f / 1024.0f);
        y += 1.0f;
#endif
      }
      else
      {
        vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetFocusKeys();

        string str = "D3D12. Inactive swapchain.";

        for(size_t i = 0; i < keys.size(); i++)
        {
          if(i == 0)
            str += " ";
          else
            str += ", ";

          str += ToStr::Get(keys[i]);
        }

        if(!keys.empty())
          str += " to cycle between swapchains";

        GetDebugManager()->RenderText(list, 0.0f, 0.0f, str.c_str());
      }

      // transition backbuffer back again
      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
      list->ResourceBarrier(1, &barrier);

      list->Close();

      ExecuteLists();
      FlushLists();
    }
  }

  if(!activeWindow)
    return S_OK;

  RenderDoc::Inst().SetCurrentDriver(RDC_D3D12);

  // kill any current capture that isn't application defined
  if(m_State == WRITING_CAPFRAME && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture((ID3D12Device *)this, swapdesc.OutputWindow);

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE)
  {
    RenderDoc::Inst().StartFrameCapture((ID3D12Device *)this, swapdesc.OutputWindow);

    m_AppControlledCapture = false;
  }

  return S_OK;
}

void WrappedID3D12Device::Serialise_CaptureScope(uint64_t offset)
{
  uint32_t FrameNumber = m_FrameCounter;
  m_pSerialiser->Serialise("FrameNumber", FrameNumber);

  if(m_State >= WRITING)
  {
    GetResourceManager()->Serialise_InitialContentsNeeded();
  }
  else
  {
    m_FrameRecord.frameInfo.fileOffset = offset;
    m_FrameRecord.frameInfo.firstEvent = 1;
    m_FrameRecord.frameInfo.frameNumber = FrameNumber;
    RDCEraseEl(m_FrameRecord.frameInfo.stats);

    GetResourceManager()->CreateInitialContents();
  }
}

bool WrappedID3D12Device::Serialise_BeginCaptureFrame(bool applyInitialState)
{
  if(m_State < WRITING && !applyInitialState)
  {
    m_pSerialiser->SkipCurrentChunk();
    return true;
  }

  vector<D3D12_RESOURCE_BARRIER> barriers;

  {
    SCOPED_LOCK(m_ResourceStatesLock);    // not needed on replay, but harmless also
    GetResourceManager()->SerialiseResourceStates(barriers, m_ResourceStates);
  }

  if(applyInitialState && !barriers.empty())
  {
    // apply initial resource states
    ID3D12GraphicsCommandList *list = GetNewList();

    list->ResourceBarrier((UINT)barriers.size(), &barriers[0]);

    list->Close();

    ExecuteLists();
    FlushLists();
  }

  return true;
}

void WrappedID3D12Device::EndCaptureFrame(ID3D12Resource *presentImage)
{
  SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);

  SERIALISE_ELEMENT(ResourceId, bbid, GetResID(presentImage));

  bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
  m_pSerialiser->Serialise("HasCallstack", HasCallstack);

  if(HasCallstack)
  {
    Callstack::Stackwalk *call = Callstack::Collect();

    RDCASSERT(call->NumLevels() < 0xff);

    size_t numLevels = call->NumLevels();
    uint64_t *stack = (uint64_t *)call->GetAddrs();

    m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

    delete call;
  }

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedID3D12Device::StartFrameCapture(void *dev, void *wnd)
{
  if(m_State != WRITING_IDLE)
    return;

  RenderDoc::Inst().SetCurrentDriver(RDC_D3D12);

  m_AppControlledCapture = true;

  m_FrameCounter = RDCMAX(1 + (uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FetchFrameInfo frame;
  frame.frameNumber = m_FrameCounter + 1;
  frame.captureTime = Timing::GetUnixTimestamp();
  RDCEraseEl(frame.stats);
  m_CapturedFrames.push_back(frame);

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Read);

  // need to do all this atomically so that no other commands
  // will check to see if they need to markdirty or markpendingdirty
  // and go into the frame record.
  {
    SCOPED_LOCK(m_CapTransitionLock);
    GetResourceManager()->PrepareInitialContents();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();

    {
      SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);

      Serialise_BeginCaptureFrame(false);

      // need to hold onto this as it must come right after the capture chunk,
      // before any command lists
      m_HeaderChunk = scope.Get();
    }

    m_State = WRITING_CAPFRAME;
  }

  RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedID3D12Device::EndFrameCapture(void *dev, void *wnd)
{
  if(m_State != WRITING_CAPFRAME)
    return true;

  WrappedIDXGISwapChain3 *swap = NULL;
  SwapPresentInfo swapInfo = {};

  if(wnd)
  {
    for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    {
      DXGI_SWAP_CHAIN_DESC swapDesc;
      it->first->GetDesc(&swapDesc);

      if(swapDesc.OutputWindow == wnd)
      {
        swap = it->first;
        swapInfo = it->second;
        break;
      }
    }

    if(swap == NULL)
    {
      RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
      return false;
    }
  }

  RDCLOG("Finished capture, Frame %u", m_FrameCounter);

  ID3D12Resource *backbuffer = NULL;

  if(swap == NULL)
  {
    swap = m_LastSwap;
    swapInfo = m_SwapChains[swap];
  }

  if(swap != NULL)
    backbuffer = (ID3D12Resource *)swap->GetBackbuffers()[swapInfo.lastPresentedBuffer];

  // transition back to IDLE atomically
  {
    SCOPED_LOCK(m_CapTransitionLock);
    EndCaptureFrame(backbuffer);

    m_State = WRITING_IDLE;

    GPUSync();
  }

  byte *thpixels = NULL;
  uint32_t thwidth = 0;
  uint32_t thheight = 0;

  const uint32_t maxSize = 1024;

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

      ExecuteLists();
      FlushLists();

      byte *data = NULL;
      hr = copyDst->Map(0, NULL, (void **)&data);

      if(SUCCEEDED(hr) && data)
      {
        ResourceFormat fmt = MakeResourceFormat(desc.Format);

        float aspect = float(desc.Width) / float(desc.Height);

        thwidth = RDCMIN(maxSize, (uint32_t)desc.Width);
        thwidth &= ~0x7;    // align down to multiple of 8
        thheight = uint32_t(float(thwidth) / aspect);

        thpixels = new byte[3 * thwidth * thheight];

        float widthf = float(desc.Width);
        float heightf = float(desc.Height);

        uint32_t stride = fmt.compByteWidth * fmt.compCount;

        bool buf1010102 = false;
        bool bufBGRA = (fmt.bgraOrder != false);

        if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
        {
          stride = 4;
          buf1010102 = true;
        }

        byte *dstPixels = thpixels;

        for(uint32_t y = 0; y < thheight; y++)
        {
          for(uint32_t x = 0; x < thwidth; x++)
          {
            float xf = float(x) / float(thwidth);
            float yf = float(y) / float(thheight);

            byte *srcPixels = &data[stride * uint32_t(xf * widthf) +
                                    layout.Footprint.RowPitch * uint32_t(yf * heightf)];

            if(buf1010102)
            {
              uint32_t *src1010102 = (uint32_t *)srcPixels;
              Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
              dstPixels[0] = (byte)(unorm.x * 255.0f);
              dstPixels[1] = (byte)(unorm.y * 255.0f);
              dstPixels[2] = (byte)(unorm.z * 255.0f);
            }
            else if(bufBGRA)
            {
              dstPixels[0] = srcPixels[2];
              dstPixels[1] = srcPixels[1];
              dstPixels[2] = srcPixels[0];
            }
            else if(fmt.compByteWidth == 2)    // R16G16B16A16 backbuffer
            {
              uint16_t *src16 = (uint16_t *)srcPixels;

              float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
              float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
              float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

              if(linearR < 0.0031308f)
                dstPixels[0] = byte(255.0f * (12.92f * linearR));
              else
                dstPixels[0] = byte(255.0f * (1.055f * powf(linearR, 1.0f / 2.4f) - 0.055f));

              if(linearG < 0.0031308f)
                dstPixels[1] = byte(255.0f * (12.92f * linearG));
              else
                dstPixels[1] = byte(255.0f * (1.055f * powf(linearG, 1.0f / 2.4f) - 0.055f));

              if(linearB < 0.0031308f)
                dstPixels[2] = byte(255.0f * (12.92f * linearB));
              else
                dstPixels[2] = byte(255.0f * (1.055f * powf(linearB, 1.0f / 2.4f) - 0.055f));
            }
            else
            {
              dstPixels[0] = srcPixels[0];
              dstPixels[1] = srcPixels[1];
              dstPixels[2] = srcPixels[2];
            }

            dstPixels += 3;
          }
        }

        copyDst->Unmap(0, NULL);
      }
      else
      {
        RDCERR("Couldn't map readback buffer: 0x%08x", hr);
      }

      SAFE_RELEASE(copyDst);
    }
    else
    {
      RDCERR("Couldn't create readback buffer: 0x%08x", hr);
    }
  }

  byte *jpgbuf = NULL;
  int len = thwidth * thheight;

  if(wnd && thpixels)
  {
    jpgbuf = new byte[len];

    jpge::params p;

    p.m_quality = 40;

    bool success =
        jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

    if(!success)
    {
      RDCERR("Failed to compress to jpg");
      SAFE_DELETE_ARRAY(jpgbuf);
      thwidth = 0;
      thheight = 0;
    }
  }

  Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(
      m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

  {
    SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

    m_pFileSerialiser->Insert(scope.Get(true));
  }

  RDCDEBUG("Inserting Resource Serialisers");

  GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);

  GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

  RDCDEBUG("Creating Capture Scope");

  {
    SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

    Serialise_CaptureScope(0);

    m_pFileSerialiser->Insert(scope.Get(true));

    m_pFileSerialiser->Insert(m_HeaderChunk);
  }

  // don't need to lock access to m_CmdListRecords as we are no longer
  // in capframe (the transition is thread-protected) so nothing will be
  // pushed to the vector

  map<int32_t, Chunk *> recordlist;

  {
    const vector<D3D12ResourceRecord *> &cmdListRecords = m_Queue->GetCmdLists();

    RDCDEBUG("Flushing %u command list records to file serialiser", (uint32_t)cmdListRecords.size());

    for(size_t i = 0; i < cmdListRecords.size(); i++)
    {
      cmdListRecords[i]->Insert(recordlist);

      RDCDEBUG("Adding %u chunks to file serialiser from command list %llu",
               (uint32_t)recordlist.size(), cmdListRecords[i]->GetResourceID());
    }

    m_Queue->GetResourceRecord()->Insert(recordlist);
  }

  {
    m_FrameCaptureRecord->Insert(recordlist);

    RDCDEBUG("Flushing %u chunks to file serialiser from context record",
             (uint32_t)recordlist.size());

    for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      m_pFileSerialiser->Insert(it->second);

    RDCDEBUG("Done");
  }

  m_pFileSerialiser->FlushToDisk();

  RenderDoc::Inst().SuccessfullyWrittenLog();

  SAFE_DELETE(m_pFileSerialiser);
  SAFE_DELETE(m_HeaderChunk);

  m_State = WRITING_IDLE;

  m_Queue->ClearAfterCapture();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  GetResourceManager()->FlushPendingDirty();

  return true;
}

bool WrappedID3D12Device::Serialise_ReleaseResource(ID3D12DeviceChild *res)
{
  return true;
}

void WrappedID3D12Device::ReleaseResource(ID3D12DeviceChild *res)
{
  D3D12NOTIMP("ReleaseResource");

  ResourceId id = GetResID(res);

  {
    SCOPED_LOCK(m_ResourceStatesLock);
    m_ResourceStates.erase(id);
  }

  // wrapped resources get released all the time, we don't want to
  // try and slerp in a resource release. Just the explicit ones
  if(m_State < WRITING)
  {
    if(GetResourceManager()->HasLiveResource(id))
      GetResourceManager()->EraseLiveResource(id);
    return;
  }
}

bool WrappedID3D12Device::Serialise_SetShaderDebugPath(ID3D12DeviceChild *res, const char *p)
{
  SERIALISE_ELEMENT(ResourceId, resource, GetResID(res));
  string debugPath = p ? p : "";
  m_pSerialiser->Serialise("debugPath", debugPath);

  if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
  {
    RDCUNIMPLEMENTED("SetDebugInfoPath");
  }

  return true;
}

HRESULT WrappedID3D12Device::SetShaderDebugPath(ID3D12DeviceChild *res, const char *path)
{
  if(m_State >= WRITING)
  {
    D3D12ResourceRecord *record = GetRecord(res);

    if(record == NULL)
    {
      RDCERR("Setting shader debug path on object %p of type %d that has no resource record.", res,
             IdentifyTypeByPtr(res));
      return E_INVALIDARG;
    }

    {
      SCOPED_SERIALISE_CONTEXT(SET_SHADER_DEBUG_PATH);
      Serialise_SetShaderDebugPath(res, path);
      record->AddChunk(scope.Get());
    }

    return S_OK;
  }

  return S_OK;
}

bool WrappedID3D12Device::Serialise_SetResourceName(ID3D12DeviceChild *res, const char *nm)
{
  SERIALISE_ELEMENT(ResourceId, resource, GetResID(res));
  string name = nm ? nm : "";
  m_pSerialiser->Serialise("name", name);

  if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
  {
    ID3D12DeviceChild *r = GetResourceManager()->GetLiveResource(resource);

    m_ResourceNames[resource] = name;

    r->SetName(StringFormat::UTF82Wide(name).c_str());
  }

  return true;
}

void WrappedID3D12Device::SetResourceName(ID3D12DeviceChild *res, const char *name)
{
  // don't allow naming device contexts or command lists so we know this chunk
  // is always on a pre-capture chunk.
  if(m_State >= WRITING && !WrappedID3D12GraphicsCommandList::IsAlloc(res) &&
     !WrappedID3D12CommandQueue::IsAlloc(res))
  {
    D3D12ResourceRecord *record = GetRecord(res);

    if(record == NULL)
      record = m_DeviceRecord;

    SCOPED_LOCK(m_D3DLock);
    {
      SCOPED_SERIALISE_CONTEXT(SET_RESOURCE_NAME);

      Serialise_SetResourceName(res, name);

      // don't serialise many SetResourceName chunks to the
      // object record, but we can't afford to drop any.
      record->LockChunks();
      while(record->HasChunks())
      {
        Chunk *end = record->GetLastChunk();

        if(end->GetChunkType() == SET_RESOURCE_NAME)
        {
          SAFE_DELETE(end);
          record->PopChunk();
          continue;
        }

        break;
      }
      record->UnlockChunks();

      record->AddChunk(scope.Get());
    }
  }
}

void WrappedID3D12Device::CreateInternalResources()
{
  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_Alloc);
  CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_GPUSyncFence);
  m_GPUSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  m_GPUSyncCounter = 0;

  RDCASSERT(m_DebugManager == NULL);

  if(m_DebugManager == NULL)
    m_DebugManager = new D3D12DebugManager(this);
}

void WrappedID3D12Device::DestroyInternalResources()
{
  if(m_GPUSyncHandle == NULL)
    return;

  ExecuteLists();
  FlushLists(true);

  for(size_t i = 0; i < m_InternalCmds.pendingcmds.size(); i++)
    SAFE_RELEASE(m_InternalCmds.pendingcmds[i]);

  SAFE_RELEASE(m_Alloc);
  SAFE_RELEASE(m_GPUSyncFence);
  CloseHandle(m_GPUSyncHandle);
}

void WrappedID3D12Device::GPUSync()
{
  m_GPUSyncCounter++;

  m_Queue->Signal(m_GPUSyncFence, m_GPUSyncCounter);
  m_GPUSyncFence->SetEventOnCompletion(m_GPUSyncCounter, m_GPUSyncHandle);
  WaitForSingleObject(m_GPUSyncHandle, 2000);
}

ID3D12GraphicsCommandList *WrappedID3D12Device::GetNewList()
{
  ID3D12GraphicsCommandList *ret = NULL;

  if(!m_InternalCmds.freecmds.empty())
  {
    ret = m_InternalCmds.freecmds.back();
    m_InternalCmds.freecmds.pop_back();

    ret->Reset(m_Alloc, NULL);
  }
  else
  {
    HRESULT hr = CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Alloc, NULL,
                                   __uuidof(ID3D12GraphicsCommandList), (void **)&ret);

    RDCASSERTEQUAL(hr, S_OK);

    if(ret == NULL)
      return NULL;

    if(m_State < WRITING)
    {
      GetResourceManager()->AddLiveResource(GetResID(ret), ret);
    }
  }

  m_InternalCmds.pendingcmds.push_back(ret);

  return ret;
}

void WrappedID3D12Device::ExecuteLists()
{
  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  vector<ID3D12CommandList *> cmds;
  cmds.resize(m_InternalCmds.pendingcmds.size());
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = m_InternalCmds.pendingcmds[i];

  GetQueue()->ExecuteCommandLists((UINT)cmds.size(), &cmds[0]);

  m_InternalCmds.submittedcmds.insert(m_InternalCmds.submittedcmds.end(),
                                      m_InternalCmds.pendingcmds.begin(),
                                      m_InternalCmds.pendingcmds.end());
  m_InternalCmds.pendingcmds.clear();
}

void WrappedID3D12Device::FlushLists(bool forceSync)
{
  if(!m_InternalCmds.submittedcmds.empty() || forceSync)
  {
    GPUSync();

    if(!m_InternalCmds.submittedcmds.empty())
      m_InternalCmds.freecmds.insert(m_InternalCmds.freecmds.end(),
                                     m_InternalCmds.submittedcmds.begin(),
                                     m_InternalCmds.submittedcmds.end());
    m_InternalCmds.submittedcmds.clear();

    m_Alloc->Reset();
  }
}

void WrappedID3D12Device::SetLogFile(const char *logfile)
{
  m_pSerialiser = new Serialiser(logfile, Serialiser::READING, false);
  m_pSerialiser->SetChunkNameLookup(&GetChunkName);

  SAFE_DELETE(m_ResourceManager);
  m_ResourceManager = new D3D12ResourceManager(m_State, m_pSerialiser, this);
  m_pSerialiser->SetUserData(m_ResourceManager);
}

const FetchDrawcall *WrappedID3D12Device::GetDrawcall(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}

void WrappedID3D12Device::ProcessChunk(uint64_t offset, D3D12ChunkType context)
{
  switch(context)
  {
    case DEVICE_INIT: { break;
    }

    case CREATE_COMMAND_QUEUE: Serialise_CreateCommandQueue(NULL, IID(), NULL); break;
    case CREATE_COMMAND_ALLOCATOR:
      Serialise_CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID(), NULL);
      break;

    case CREATE_GRAPHICS_PIPE: Serialise_CreateGraphicsPipelineState(NULL, IID(), NULL); break;
    case CREATE_COMPUTE_PIPE: Serialise_CreateComputePipelineState(NULL, IID(), NULL); break;
    case CREATE_DESCRIPTOR_HEAP: Serialise_CreateDescriptorHeap(NULL, IID(), NULL); break;
    case CREATE_ROOT_SIG: Serialise_CreateRootSignature(0, NULL, 0, IID(), NULL); break;
    case CREATE_COMMAND_SIG: Serialise_CreateCommandSignature(NULL, NULL, IID(), NULL); break;

    case CREATE_HEAP: Serialise_CreateHeap(NULL, IID(), NULL); break;
    case CREATE_COMMITTED_RESOURCE:
      Serialise_CreateCommittedResource(NULL, D3D12_HEAP_FLAG_NONE, NULL,
                                        D3D12_RESOURCE_STATE_COMMON, NULL, IID(), NULL);
      break;
    case CREATE_PLACED_RESOURCE:
      Serialise_CreatePlacedResource(NULL, 0, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, IID(), NULL);
      break;

    case CREATE_QUERY_HEAP: Serialise_CreateQueryHeap(NULL, IID(), NULL); break;
    case CREATE_FENCE: Serialise_CreateFence(0, D3D12_FENCE_FLAG_NONE, IID(), NULL); break;

    case SET_RESOURCE_NAME: Serialise_SetResourceName(0x0, ""); break;
    case SET_SHADER_DEBUG_PATH: Serialise_SetShaderDebugPath(NULL, NULL); break;
    case RELEASE_RESOURCE: Serialise_ReleaseResource(0x0); break;
    case CREATE_SWAP_BUFFER: Serialise_WrapSwapchainBuffer(NULL, NULL, 0, NULL); break;
    case CAPTURE_SCOPE: Serialise_CaptureScope(offset); break;
    default:
      // ignore system chunks
      if(context == INITIAL_CONTENTS)
        GetResourceManager()->Serialise_InitialState(ResourceId(), NULL);
      else if(context < FIRST_CHUNK_ID)
        m_pSerialiser->SkipCurrentChunk();
      else
        RDCERR("Unexpected non-device chunk %d at offset %llu", context, offset);
      break;
  }
}

void WrappedID3D12Device::ReadLogInitialisation()
{
  uint64_t frameOffset = 0;

  m_pSerialiser->SetDebugText(true);

  m_pSerialiser->Rewind();

  int chunkIdx = 0;

  struct chunkinfo
  {
    chunkinfo() : count(0), totalsize(0), total(0.0) {}
    int count;
    uint64_t totalsize;
    double total;
  };

  map<D3D12ChunkType, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offset = m_pSerialiser->GetOffset();

    D3D12ChunkType context = (D3D12ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

    if(context == CAPTURE_SCOPE)
    {
      // immediately read rest of log into memory
      m_pSerialiser->SetPersistentBlock(offset);
    }

    chunkIdx++;

    ProcessChunk(offset, context);

    m_pSerialiser->PopContext(context);

    RenderDoc::Inst().SetProgress(FileInitialRead, float(offset) / float(m_pSerialiser->GetSize()));

    if(context == CAPTURE_SCOPE)
    {
      frameOffset = offset;

      GetResourceManager()->ApplyInitialContents();

      m_Queue->ReplayLog(READING, 0, 0, false);
    }

    uint64_t offset2 = m_pSerialiser->GetOffset();

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offset2 - offset;
    chunkInfos[context].count++;

    if(context == CAPTURE_SCOPE)
      break;

    if(m_pSerialiser->AtEnd())
      break;
  }

  if(m_State == READING)
  {
    GetFrameRecord().drawcallList = m_Queue->GetParentDrawcall().Bake();

    m_Queue->GetParentDrawcall().children.clear();

    SetupDrawcallPointers(&m_Drawcalls, m_FrameRecord.drawcallList, NULL, NULL);
  }

#if !defined(RELEASE)
  for(auto it = chunkInfos.begin(); it != chunkInfos.end(); ++it)
  {
    double dcount = double(it->second.count);

    RDCDEBUG(
        "% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
        it->second.count, it->second.total, it->second.total / dcount,
        double(it->second.totalsize) / (1024.0 * 1024.0),
        double(it->second.totalsize) / (dcount * 1024.0 * 1024.0), GetChunkName(it->first),
        uint32_t(it->first));
  }
#endif

  m_FrameRecord.frameInfo.fileSize = m_pSerialiser->GetSize();
  m_FrameRecord.frameInfo.persistentSize = m_pSerialiser->GetSize() - frameOffset;
  m_FrameRecord.frameInfo.initDataSize = chunkInfos[(D3D12ChunkType)INITIAL_CONTENTS].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_pSerialiser->GetSize() - frameOffset);

  m_pSerialiser->SetDebugText(false);
}

void WrappedID3D12Device::ReplayLog(uint32_t startEventID, uint32_t endEventID,
                                    ReplayLogType replayType)
{
  uint64_t offs = m_FrameRecord.frameInfo.fileOffset;

  m_pSerialiser->SetOffset(offs);

  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = m_FrameRecord.frameInfo.firstEvent;
    partial = false;
  }

  D3D12ChunkType header = (D3D12ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

  RDCASSERTEQUAL(header, CAPTURE_SCOPE);

  m_pSerialiser->SkipCurrentChunk();

  m_pSerialiser->PopContext(header);

  if(!partial)
  {
    GetResourceManager()->ApplyInitialContents();
    GetResourceManager()->ReleaseInFrameResources();

    ExecuteLists();
    FlushLists(true);
  }

  m_State = EXECUTING;

  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    if(!partial)
    {
      RDCASSERT(cmd.m_Partial[D3D12CommandData::Primary].resultPartialCmdList == NULL);
      RDCASSERT(cmd.m_Partial[D3D12CommandData::Secondary].resultPartialCmdList == NULL);
      cmd.m_Partial[D3D12CommandData::Primary].Reset();
      cmd.m_Partial[D3D12CommandData::Secondary].Reset();
      cmd.m_RenderState = D3D12RenderState();
      cmd.m_RenderState.m_ResourceManager = GetResourceManager();
    }

    // we'll need our own command list if we're replaying just a subsection
    // of events within a single command list record - always if it's only
    // one drawcall, or if start event ID is > 0 we assume the outside code
    // has chosen a subsection that lies within a command list
    if(partial)
    {
      ID3D12GraphicsCommandList *list = cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList =
          GetNewList();

      cmd.m_RenderState.ApplyState(list);
    }

    if(replayType == eReplay_Full)
      m_Queue->ReplayLog(EXECUTING, startEventID, endEventID, partial);
    else if(replayType == eReplay_WithoutDraw)
      m_Queue->ReplayLog(EXECUTING, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    else if(replayType == eReplay_OnlyDraw)
      m_Queue->ReplayLog(EXECUTING, endEventID, endEventID, partial);
    else
      RDCFATAL("Unexpected replay type");

    if(cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList != NULL)
    {
      ID3D12GraphicsCommandList *list = cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList;

      list->Close();

      ExecuteLists();

      cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList = NULL;
    }

#if defined(SINGLE_FLUSH_VALIDATE)
    FlushLists(true);
#endif
  }
}
