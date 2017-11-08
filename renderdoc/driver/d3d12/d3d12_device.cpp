/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "driver/ihv/amd/official/DXExt/AmdExtD3D.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"

// must be at the start of any function that serialises
#define CACHE_THREAD_SERIALISER() WriteSerialiser &ser = GetThreadSerialiser();

WRAPPED_POOL_INST(WrappedID3D12Device);

std::string WrappedID3D12Device::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D12Chunk)idx);
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

  RDCWARN("Querying ID3D12DebugDevice for interface: %s", ToStr(riid).c_str());

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

  string guid = ToStr(riid);
  RDCWARN("Querying ID3D12DebugDevice for interface: %s", guid.c_str());

  return m_pDebug->QueryInterface(riid, ppvObject);
}

WrappedID3D12Device::WrappedID3D12Device(ID3D12Device *realDevice, D3D12InitParams *params)
    : m_RefCounter(realDevice, false),
      m_SoftRefCounter(NULL, false),
      m_pDevice(realDevice),
      m_ScratchSerialiser(new StreamWriter(1024), Ownership::Stream)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D12Device));

  m_SectionVersion = D3D12InitParams::CurrentVersion;

  uint32_t flags = 0;

  if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  m_ScratchSerialiser.SetChunkMetadataRecording(flags);

  m_StructuredFile = &m_StoredStructuredData;

  RDCEraseEl(m_D3D12Opts);

  m_pDevice1 = NULL;
  if(m_pDevice)
  {
    m_pDevice->QueryInterface(__uuidof(ID3D12Device1), (void **)&m_pDevice1);

    for(size_t i = 0; i < ARRAY_COUNT(m_DescriptorIncrements); i++)
      m_DescriptorIncrements[i] =
          m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

    m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &m_D3D12Opts, sizeof(m_D3D12Opts));
  }

  WrappedID3D12Resource::m_List = NULL;

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

  threadSerialiserTLSSlot = Threading::AllocateTLSSlot();
  tempMemoryTLSSlot = Threading::AllocateTLSSlot();

  m_FrameCounter = 0;

  m_HeaderChunk = NULL;

  m_Alloc = NULL;
  m_List = NULL;
  m_GPUSyncFence = NULL;
  m_GPUSyncHandle = NULL;
  m_GPUSyncCounter = 0;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::LoadingReplaying;

    WrappedID3D12Resource::m_List = new std::map<ResourceId, WrappedID3D12Resource *>();

    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

  m_DebugManager = NULL;
  m_ResourceManager = new D3D12ResourceManager(m_State, this);

  m_ScratchSerialiser.SetUserData(GetResourceManager());

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

  m_pInfoQueue = NULL;
  if(m_pDevice)
  {
    m_pDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void **)&m_pInfoQueue);
    m_pDevice->QueryInterface(__uuidof(ID3D12DebugDevice), (void **)&m_WrappedDebug.m_pDebug);
  }

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

          // message about a NULL range to map for reading/writing the whole resource
          // which is "inefficient" but for our use cases it's almost always what we mean.
          D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,

          // D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH
          // message about mismatched SRV dimensions, which it seems to get wrong with the
          // dummy NULL descriptors on the texture sampling code
          (D3D12_MESSAGE_ID)1023,
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
}

WrappedID3D12Device::~WrappedID3D12Device()
{
  RenderDoc::Inst().RemoveDeviceFrameCapturer((ID3D12Device *)this);

  SAFE_DELETE(WrappedID3D12Resource::m_List);

  for(size_t i = 0; i < m_QueueFences.size(); i++)
  {
    GPUSync(m_Queues[i], m_QueueFences[i]);

    SAFE_RELEASE(m_QueueFences[i]);
  }

  for(auto it = m_UploadBuffers.begin(); it != m_UploadBuffers.end(); ++it)
  {
    SAFE_RELEASE(it->second);
  }

  DestroyInternalResources();

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

  SAFE_RELEASE(m_pDevice1);

  SAFE_RELEASE(m_pInfoQueue);
  SAFE_RELEASE(m_WrappedDebug.m_pDebug);
  SAFE_RELEASE(m_pDevice);

  for(size_t i = 0; i < m_ThreadSerialisers.size(); i++)
    delete m_ThreadSerialisers[i];

  for(size_t i = 0; i < m_ThreadTempMem.size(); i++)
  {
    delete[] m_ThreadTempMem[i]->memory;
    delete m_ThreadTempMem[i];
  }

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
    string guid = ToStr(riid);
    RDCWARN("Querying ID3D12Device for interface: %s", guid.c_str());
  }

  return m_RefCounter.QueryInterface(riid, ppvObject);
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
  initStateCurBatch = 0;
  initStateCurList = NULL;

  GetResourceManager()->ApplyInitialContents();

  // close the final list
  if(initStateCurList)
  {
    D3D12MarkerRegion::End(initStateCurList);
    initStateCurList->Close();
  }

  initStateCurBatch = 0;
  initStateCurList = NULL;
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

void WrappedID3D12Device::FirstFrame(WrappedIDXGISwapChain4 *swap)
{
  DXGI_SWAP_CHAIN_DESC swapdesc = swap->GetDescWithHWND();

  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
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

void WrappedID3D12Device::ReleaseSwapchainResources(WrappedIDXGISwapChain4 *swap, UINT QueueCount,
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

  for(int i = 0; i < swap->GetNumBackbuffers(); i++)
  {
    ID3D12Resource *res = (ID3D12Resource *)swap->GetBackbuffers()[i];

    if(!res)
      continue;

    WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)res;
    wrapped->ReleaseInternalRef();
    SAFE_RELEASE(wrapped);
  }

  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC desc = swap->GetDescWithHWND();

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
bool WrappedID3D12Device::Serialise_WrapSwapchainBuffer(SerialiserType &ser,
                                                        WrappedIDXGISwapChain4 *swap,
                                                        DXGI_SWAP_CHAIN_DESC *swapDesc, UINT Buffer,
                                                        IUnknown *realSurface)
{
  WrappedID3D12Resource *pRes = (WrappedID3D12Resource *)realSurface;

  SERIALISE_ELEMENT(Buffer);
  SERIALISE_ELEMENT_LOCAL(SwapbufferID, GetResID(pRes));
  SERIALISE_ELEMENT_LOCAL(BackbufferDescriptor, pRes->GetDesc());

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

    if(FAILED(hr))
    {
      RDCERR("Failed to create fake back buffer, HRESULT: 0x%08x", hr);
    }
    else
    {
      WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(fakeBB, this);
      fakeBB = wrapped;

      m_ResourceNames[SwapbufferID] = "Swap Chain Buffer";
      fakeBB->SetName(L"Swap Chain Buffer");

      GetResourceManager()->AddLiveResource(SwapbufferID, fakeBB);

      m_BackbufferFormat[wrapped->GetResourceID()] = SwapbufferFormat;

      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states.resize(1, D3D12_RESOURCE_STATE_PRESENT);
    }
  }

  return true;
}

IUnknown *WrappedID3D12Device::WrapSwapchainBuffer(WrappedIDXGISwapChain4 *swap,
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

  if(IsCaptureMode(m_State))
  {
    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    record->type = Resource_Resource;
    record->DataInSerialiser = false;
    record->SpecialResource = true;
    record->Length = 0;

    WrappedID3D12Resource *wrapped = (WrappedID3D12Resource *)pRes;

    wrapped->SetResourceRecord(record);

    WriteSerialiser &ser = m_ScratchSerialiser;

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::CreateSwapBuffer);

    Serialise_WrapSwapchainBuffer(ser, swap, swapDesc, buffer, pRes);

    record->AddChunk(scope.Get());

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[id];

      states.resize(1, D3D12_RESOURCE_STATE_PRESENT);
    }
  }

  if(IsCaptureMode(m_State))
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

    ID3DDevice *swapQ = swap->GetD3DDevice();
    RDCASSERT(WrappedID3D12CommandQueue::IsAlloc(swapQ));
    m_SwapChains[swap].queue = (WrappedID3D12CommandQueue *)swapQ;

    // start at -1 so that we know we've never presented before
    m_SwapChains[swap].lastPresentedBuffer = -1;
  }

  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC sdesc = swap->GetDescWithHWND();

    Keyboard::AddInputWindow(sdesc.OutputWindow);

    RenderDoc::Inst().AddFrameCapturer((ID3D12Device *)this, sdesc.OutputWindow, this);
  }

  return pRes;
}

void WrappedID3D12Device::Map(ID3D12Resource *Resource, UINT Subresource)
{
  MapState map;
  map.res = Resource;
  map.subres = Subresource;

  D3D12_RESOURCE_DESC desc = Resource->GetDesc();

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
  {
    SCOPED_LOCK(m_MapsLock);
    for(auto it = m_Maps.begin(); it != m_Maps.end(); ++it)
    {
      if(it->res == Resource && it->subres == Subresource)
      {
        map = *it;
        m_Maps.erase(it);
        break;
      }
    }
  }

  if(map.res == NULL)
    return;

  bool capframe = false;
  {
    SCOPED_LOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  D3D12_RANGE range = {0, (SIZE_T)map.totalSize};

  if(pWrittenRange)
    range = *pWrittenRange;

  if(capframe)
    MapDataWrite(Resource, Subresource, mapPtr, range);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_MapDataWrite(SerialiserType &ser, ID3D12Resource *Resource,
                                                 UINT Subresource, byte *MappedData,
                                                 D3D12_RANGE range)
{
  SERIALISE_ELEMENT(Resource);
  SERIALISE_ELEMENT(Subresource);
  SERIALISE_ELEMENT(range);

  MappedData += range.Begin;
  uint64_t rangeSize = range.End - range.Begin;

  SERIALISE_ELEMENT_ARRAY(MappedData, rangeSize);

  // don't do anything if end <= begin because the range is empty.
  if(IsReplayingAndReading() && Resource && range.End > range.Begin)
  {
    ResourceId origid = GetResourceManager()->GetOriginalID(GetResID(Resource));
    if(m_UploadResourceIds.find(origid) != m_UploadResourceIds.end())
    {
      D3D12CommandData &cmd = *m_Queue->GetCommandData();

      ID3D12Resource *uploadBuf = GetUploadBuffer(cmd.m_CurChunkOffset, rangeSize);

      SetObjName(uploadBuf, StringFormat::Fmt("Map data write, %llu bytes for %llu/%u @ %llu",
                                              rangeSize, origid, Subresource, cmd.m_CurChunkOffset));

      // during loading, fill out the buffer itself
      if(IsLoading(m_State))
      {
        D3D12_RANGE maprange = {0, 0};
        void *dst = NULL;
        HRESULT hr = uploadBuf->Map(Subresource, &maprange, &dst);

        if(SUCCEEDED(hr))
        {
          maprange.Begin = 0;
          maprange.End = SIZE_T(rangeSize);

          memcpy(dst, MappedData, maprange.End);

          uploadBuf->Unmap(Subresource, &maprange);
        }
        else
        {
          RDCERR("Failed to map resource on replay %08x", hr);
        }
      }

      // then afterwards just execute a list to copy the result
      m_DataUploadList->Reset(m_DataUploadAlloc, NULL);
      m_DataUploadList->CopyBufferRegion(Resource, range.Begin, uploadBuf, 0,
                                         range.End - range.Begin);
      m_DataUploadList->Close();
      ID3D12CommandList *l = m_DataUploadList;
      GetQueue()->ExecuteCommandLists(1, &l);
      GPUSync();
    }
    else
    {
      byte *dst = NULL;

      D3D12_RANGE nopRange = {0, 0};

      HRESULT hr = Resource->Map(Subresource, &nopRange, (void **)&dst);

      if(SUCCEEDED(hr))
      {
        memcpy(dst + range.Begin, MappedData, size_t(range.End - range.Begin));

        Resource->Unmap(Subresource, &range);
      }
      else
      {
        RDCERR("Failed to map resource on replay %08x", hr);
      }
    }
  }

  return true;
}

template bool WrappedID3D12Device::Serialise_MapDataWrite(ReadSerialiser &ser,
                                                          ID3D12Resource *Resource, UINT Subresource,
                                                          byte *MappedData, D3D12_RANGE range);
template bool WrappedID3D12Device::Serialise_MapDataWrite(WriteSerialiser &ser,
                                                          ID3D12Resource *Resource, UINT Subresource,
                                                          byte *MappedData, D3D12_RANGE range);

void WrappedID3D12Device::MapDataWrite(ID3D12Resource *Resource, UINT Subresource, byte *mapPtr,
                                       D3D12_RANGE range)
{
  CACHE_THREAD_SERIALISER();

  SCOPED_SERIALISE_CHUNK(D3D12Chunk::Resource_Unmap);
  Serialise_MapDataWrite(ser, Resource, Subresource, mapPtr, range);

  m_FrameCaptureRecord->AddChunk(scope.Get());

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(Resource), eFrameRef_Write);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_WriteToSubresource(SerialiserType &ser, ID3D12Resource *Resource,
                                                       UINT Subresource, const D3D12_BOX *pDstBox,
                                                       const void *pSrcData, UINT SrcRowPitch,
                                                       UINT SrcDepthPitch)
{
  SERIALISE_ELEMENT(Resource);
  SERIALISE_ELEMENT(Subresource);
  SERIALISE_ELEMENT_OPT(pDstBox);
  SERIALISE_ELEMENT(SrcRowPitch);
  SERIALISE_ELEMENT(SrcDepthPitch);

  uint64_t dataSize = 0;

  if(ser.IsWriting())
  {
    // if we have a box, calculate how much data from user's pitch
    if(pDstBox)
    {
      UINT numSlicesBeforeLast = pDstBox->back - pDstBox->front - 1;
      UINT numRows = pDstBox->bottom - pDstBox->top;

      dataSize = SrcDepthPitch * numSlicesBeforeLast + SrcRowPitch * numRows;
    }
    else
    {
      D3D12_RESOURCE_DESC desc = Resource->GetDesc();

      // otherwise fetch the whole resource size
      m_pDevice->GetCopyableFootprints(&desc, Subresource, 1, 0, NULL, NULL, NULL, &dataSize);
    }
  }

  SERIALISE_ELEMENT_ARRAY(pSrcData, dataSize);

  if(IsReplayingAndReading() && Resource)
  {
    ResourceId origid = GetResourceManager()->GetOriginalID(GetResID(Resource));
    if(m_UploadResourceIds.find(origid) != m_UploadResourceIds.end())
    {
      D3D12CommandData &cmd = *m_Queue->GetCommandData();

      ID3D12Resource *uploadBuf = GetUploadBuffer(cmd.m_CurChunkOffset, dataSize);

      // during reading, fill out the buffer itself
      if(IsLoading(m_State))
      {
        D3D12_RANGE range = {0, 0};
        void *dst = NULL;
        HRESULT hr = uploadBuf->Map(Subresource, &range, &dst);

        if(SUCCEEDED(hr))
        {
          memcpy(dst, pSrcData, (size_t)dataSize);

          range.Begin = 0;
          range.End = (size_t)dataSize;

          uploadBuf->Unmap(Subresource, &range);
        }
        else
        {
          RDCERR("Failed to map resource on replay %08x", hr);
        }
      }

      // then afterwards just execute a list to copy the result
      m_DataUploadList->Reset(m_DataUploadAlloc, NULL);
      UINT64 copySize = dataSize;
      if(pDstBox)
        copySize = RDCMIN(copySize, UINT64(pDstBox->right - pDstBox->left));
      m_DataUploadList->CopyBufferRegion(Resource, pDstBox ? pDstBox->left : 0, uploadBuf, 0,
                                         copySize);
      m_DataUploadList->Close();
      ID3D12CommandList *l = m_DataUploadList;
      GetQueue()->ExecuteCommandLists(1, &l);
      GPUSync();
    }
    else
    {
      HRESULT hr = Resource->Map(Subresource, NULL, NULL);

      if(SUCCEEDED(hr))
      {
        Resource->WriteToSubresource(Subresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

        Resource->Unmap(Subresource, NULL);
      }
      else
      {
        RDCERR("Failed to map resource on replay %08x", hr);
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
    SCOPED_LOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  if(capframe)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Resource_WriteToSubresource);
    Serialise_WriteToSubresource(ser, Resource, Subresource, pDstBox, pSrcData, SrcRowPitch,
                                 SrcDepthPitch);

    m_FrameCaptureRecord->AddChunk(scope.Get());

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(Resource), eFrameRef_Write);
  }
}

HRESULT WrappedID3D12Device::Present(WrappedIDXGISwapChain4 *swap, UINT SyncInterval, UINT Flags)
{
  if((Flags & DXGI_PRESENT_TEST) != 0)
    return S_OK;

  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  DXGI_SWAP_CHAIN_DESC swapdesc = swap->GetDescWithHWND();
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

  if(IsBackgroundCapturing(m_State))
  {
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

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      string overlayText = RenderDoc::Inst().GetOverlayText(RDC_D3D12, m_FrameCounter, flags);

      if(!overlayText.empty())
        GetDebugManager()->RenderText(list, 0.0f, 0.0f, overlayText.c_str());

      // transition backbuffer back again
      std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
      list->ResourceBarrier(1, &barrier);

      list->Close();

      ExecuteLists(swapInfo.queue);
      FlushLists(false, swapInfo.queue);
    }
  }

  if(!activeWindow)
    return S_OK;

  RenderDoc::Inst().SetCurrentDriver(RDC_D3D12);

  // kill any current capture that isn't application defined
  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture((ID3D12Device *)this, swapdesc.OutputWindow);

  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter))
  {
    RenderDoc::Inst().StartFrameCapture((ID3D12Device *)this, swapdesc.OutputWindow);

    m_AppControlledCapture = false;
  }

  return S_OK;
}

template <typename SerialiserType>
void WrappedID3D12Device::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT(m_FrameCounter);

  if(IsReplayMode(m_State))
  {
    m_FrameRecord.frameInfo.frameNumber = m_FrameCounter;
    RDCEraseEl(m_FrameRecord.frameInfo.stats);
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_BeginCaptureFrame(SerialiserType &ser, bool applyInitialState)
{
  std::vector<D3D12_RESOURCE_BARRIER> barriers;

  {
    SCOPED_LOCK(m_ResourceStatesLock);    // not needed on replay, but harmless also
    GetResourceManager()->SerialiseResourceStates(ser, barriers, m_ResourceStates);
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

template bool WrappedID3D12Device::Serialise_BeginCaptureFrame(ReadSerialiser &ser,
                                                               bool applyInitialState);
template bool WrappedID3D12Device::Serialise_BeginCaptureFrame(WriteSerialiser &ser,
                                                               bool applyInitialState);

void WrappedID3D12Device::EndCaptureFrame(ID3D12Resource *presentImage)
{
  WriteSerialiser &ser = m_ScratchSerialiser;
  ser.SetDrawChunk();
  SCOPED_SERIALISE_CHUNK(D3D12Chunk::CaptureEnd);

  SERIALISE_ELEMENT_LOCAL(PresentedBackbuffer, GetResID(presentImage));

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedID3D12Device::StartFrameCapture(void *dev, void *wnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RenderDoc::Inst().SetCurrentDriver(RDC_D3D12);

  m_AppControlledCapture = true;

  m_FrameCounter = RDCMAX(1 + (uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FrameDescription frame;
  frame.frameNumber = m_FrameCounter + 1;
  frame.captureTime = Timing::GetUnixTimestamp();
  RDCEraseEl(frame.stats);
  m_CapturedFrames.push_back(frame);

  GetDebugMessages();
  m_DebugMessages.clear();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Read);

  // need to do all this atomically so that no other commands
  // will check to see if they need to markdirty or markpendingdirty
  // and go into the frame record.
  {
    SCOPED_LOCK(m_CapTransitionLock);

    initStateCurBatch = 0;
    initStateCurList = NULL;

    GetResourceManager()->PrepareInitialContents();

    // close the final list
    initStateCurList->Close();

    initStateCurBatch = 0;
    initStateCurList = NULL;

    ExecuteLists();
    FlushLists();

    RDCDEBUG("Attempting capture");
    m_FrameCaptureRecord->DeleteChunks();

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::CaptureBegin);

      Serialise_BeginCaptureFrame(ser, false);

      // need to hold onto this as it must come right after the capture chunk,
      // before any command lists
      m_HeaderChunk = scope.Get();
    }

    m_State = CaptureState::ActiveCapturing;
  }

  RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedID3D12Device::EndFrameCapture(void *dev, void *wnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  WrappedIDXGISwapChain4 *swap = NULL;
  SwapPresentInfo swapInfo = {};

  if(wnd)
  {
    for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    {
      DXGI_SWAP_CHAIN_DESC swapDesc = it->first->GetDescWithHWND();

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

  std::vector<WrappedID3D12CommandQueue *> queues;
  RDCFile *rdc = NULL;

  // transition back to IDLE and readback initial states atomically
  {
    SCOPED_LOCK(m_CapTransitionLock);
    EndCaptureFrame(backbuffer);

    m_State = CaptureState::BackgroundCapturing;

    GPUSync();

    {
      SCOPED_LOCK(m_MapsLock);
      for(auto it = m_Maps.begin(); it != m_Maps.end(); ++it)
        GetWrapped(it->res)->FreeShadow();
    }

    byte *thpixels = NULL;
    uint16_t thwidth = 0;
    uint16_t thheight = 0;

    const uint32_t maxSize = 2048;

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

          thwidth = (uint16_t)RDCMIN(maxSize, (uint32_t)desc.Width);
          thwidth &= ~0x7;    // align down to multiple of 8
          thheight = uint16_t(float(thwidth) / aspect);

          thpixels = new byte[3 * thwidth * thheight];

          float widthf = float(desc.Width);
          float heightf = float(desc.Height);

          uint32_t stride = fmt.compByteWidth * fmt.compCount;

          bool buf1010102 = false;
          bool bufBGRA = (fmt.bgraOrder != false);

          if(fmt.type == ResourceFormatType::R10G10B10A2)
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
      p.m_quality = 80;

      bool success = jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3,
                                                                 thpixels, p);

      if(!success)
      {
        RDCERR("Failed to compress to jpg");
        SAFE_DELETE_ARRAY(jpgbuf);
        thwidth = 0;
        thheight = 0;
      }
    }

    queues = m_Queues;

    for(auto it = queues.begin(); it != queues.end(); ++it)
      if((*it)->GetResourceRecord()->ContainsExecuteIndirect)
        WrappedID3D12Resource::RefBuffers(GetResourceManager());

    rdc = RenderDoc::Inst().CreateRDC(m_FrameCounter, jpgbuf, len, thwidth, thheight);

    SAFE_DELETE_ARRAY(jpgbuf);
    SAFE_DELETE_ARRAY(thpixels);
  }

  StreamWriter *captureWriter = NULL;

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

  {
    WriteSerialiser ser(captureWriter, Ownership::Stream);

    ser.SetUserData(GetResourceManager());

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
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::CaptureScope, 16);

      Serialise_CaptureScope(ser);
    }

    m_HeaderChunk->Write(ser);

    // don't need to lock access to m_CmdListRecords as we are no longer
    // in capframe (the transition is thread-protected) so nothing will be
    // pushed to the vector

    map<int32_t, Chunk *> recordlist;

    for(auto it = queues.begin(); it != queues.end(); ++it)
    {
      WrappedID3D12CommandQueue *q = *it;

      const std::vector<D3D12ResourceRecord *> &cmdListRecords = q->GetCmdLists();

      RDCDEBUG("Flushing %u command list records from queue %llu", (uint32_t)cmdListRecords.size(),
               q->GetResourceID());

      for(size_t i = 0; i < cmdListRecords.size(); i++)
      {
        uint32_t prevSize = (uint32_t)recordlist.size();
        cmdListRecords[i]->Insert(recordlist);

        // prevent complaints in release that prevSize is unused
        (void)prevSize;

        RDCDEBUG("Adding %u chunks to file serialiser from command list %llu",
                 (uint32_t)recordlist.size() - prevSize, cmdListRecords[i]->GetResourceID());
      }

      q->GetResourceRecord()->Insert(recordlist);
    }

    m_FrameCaptureRecord->Insert(recordlist);

    RDCDEBUG("Flushing %u chunks to file serialiser from context record",
             (uint32_t)recordlist.size());

    for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
      it->second->Write(ser);

    RDCDEBUG("Done");
  }

  RenderDoc::Inst().FinishCaptureWriting(rdc, m_FrameCounter);

  SAFE_DELETE(m_HeaderChunk);

  m_State = CaptureState::BackgroundCapturing;

  for(auto it = queues.begin(); it != queues.end(); ++it)
    (*it)->ClearAfterCapture();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  GetResourceManager()->FlushPendingDirty();

  FlushPendingDescriptorWrites();

  return true;
}

void WrappedID3D12Device::ReleaseResource(ID3D12DeviceChild *res)
{
  ResourceId id = GetResID(res);

  {
    SCOPED_LOCK(m_ResourceStatesLock);
    m_ResourceStates.erase(id);
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

void WrappedID3D12Device::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                          std::string d)
{
  D3D12CommandData &cmd = *m_Queue->GetCommandData();

  DebugMessage msg;
  msg.eventID = 0;
  msg.messageID = 0;
  msg.source = src;
  msg.category = c;
  msg.severity = sv;
  msg.description = d;
  if(IsActiveReplaying(m_State))
  {
    // look up the EID this drawcall came from
    D3D12CommandData::DrawcallUse use(cmd.m_CurChunkOffset, 0);
    auto it = std::lower_bound(cmd.m_DrawcallUses.begin(), cmd.m_DrawcallUses.end(), use);
    RDCASSERT(it != cmd.m_DrawcallUses.end());

    msg.eventID = it->eventID;
  }

  AddDebugMessage(msg);
}

void WrappedID3D12Device::AddDebugMessage(const DebugMessage &msg)
{
  D3D12CommandData &cmd = *m_Queue->GetCommandData();

  if(IsLoading(m_State))
    cmd.m_EventMessages.push_back(msg);
  else
    m_DebugMessages.push_back(msg);
}

std::vector<DebugMessage> WrappedID3D12Device::GetDebugMessages()
{
  std::vector<DebugMessage> ret;

  // if reading, m_DebugMessages will contain all the messages (we
  // don't try and fetch anything from the API). If writing,
  // m_DebugMessages will contain any manually-added messages.
  ret.swap(m_DebugMessages);

  if(IsReplayMode(m_State))
    return ret;

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
    msg.eventID = 0;
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
      case D3D12_MESSAGE_SEVERITY_ERROR: msg.severity = MessageSeverity::Medium; break;
      case D3D12_MESSAGE_SEVERITY_WARNING: msg.severity = MessageSeverity::Low; break;
      case D3D12_MESSAGE_SEVERITY_INFO: msg.severity = MessageSeverity::Info; break;
      case D3D12_MESSAGE_SEVERITY_MESSAGE: msg.severity = MessageSeverity::Info; break;
      default: RDCWARN("Unexpected message severity: %d", message->Severity); break;
    }

    msg.messageID = (uint32_t)message->ID;
    msg.description = string(message->pDescription);

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

void WrappedID3D12Device::FlushPendingDescriptorWrites()
{
  std::vector<DynamicDescriptorWrite> writes;
  std::vector<DynamicDescriptorCopy> copies;

  {
    SCOPED_LOCK(m_DynDescLock);
    writes.swap(m_DynamicDescriptorWrites);
    copies.swap(m_DynamicDescriptorCopies);
    m_DynamicDescriptorRefs.clear();
  }

  for(size_t i = 0; i < writes.size(); i++)
    writes[i].dest->CopyFrom(writes[i].desc);

  for(size_t i = 0; i < copies.size(); i++)
    copies[i].dst->CopyFrom(*copies[i].src);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_SetShaderDebugPath(SerialiserType &ser,
                                                       ID3D12DeviceChild *pResource, const char *Path)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Path);

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
      WriteSerialiser &ser = m_ScratchSerialiser;
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

  if(IsReplayingAndReading() && pResource)
  {
    m_ResourceNames[GetResourceManager()->GetOriginalID(GetResID(pResource))] = Name;

    pResource->SetName(StringFormat::UTF82Wide(Name).c_str());
  }

  return true;
}

void WrappedID3D12Device::SetName(ID3D12DeviceChild *pResource, const char *Name)
{
  // don't allow naming device contexts or command lists so we know this chunk
  // is always on a pre-capture chunk.
  if(IsCaptureMode(m_State) && !WrappedID3D12GraphicsCommandList::IsAlloc(pResource) &&
     !WrappedID3D12CommandQueue::IsAlloc(pResource))
  {
    D3D12ResourceRecord *record = GetRecord(pResource);

    if(record == NULL)
      record = m_DeviceRecord;

    {
      WriteSerialiser &ser = m_ScratchSerialiser;
      SCOPED_SERIALISE_CHUNK(D3D12Chunk::SetName);

      Serialise_SetName(ser, pResource, Name);

      // don't serialise many SetName chunks to the
      // object record, but we can't afford to drop any.
      record->LockChunks();
      while(record->HasChunks())
      {
        Chunk *end = record->GetLastChunk();

        if(end->GetChunkType<D3D12Chunk>() == D3D12Chunk::SetName)
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

  uint32_t flags = 0;

  if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  ser->SetChunkMetadataRecording(flags);
  ser->SetUserData(GetResourceManager());

  Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

  {
    SCOPED_LOCK(m_ThreadSerialisersLock);
    m_ThreadSerialisers.push_back(ser);
  }

  return *ser;
}

void WrappedID3D12Device::CreateInternalResources()
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

  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_Alloc);
  CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_GPUSyncFence);
  m_GPUSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_DataUploadAlloc);

  CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DataUploadAlloc, NULL,
                    __uuidof(ID3D12GraphicsCommandList), (void **)&m_DataUploadList);

  m_DataUploadList->Close();

  m_GPUSyncCounter = 0;

  RDCASSERT(m_DebugManager == NULL);

  if(m_DebugManager == NULL)
    m_DebugManager = new D3D12DebugManager(this);
}

void WrappedID3D12Device::DestroyInternalResources()
{
  if(m_GPUSyncHandle == NULL)
    return;

  SAFE_RELEASE(m_pAMDExtObject);

  ExecuteLists();
  FlushLists(true);

  for(size_t i = 0; i < m_InternalCmds.pendingcmds.size(); i++)
    SAFE_RELEASE(m_InternalCmds.pendingcmds[i]);

  delete m_DebugManager;
  m_DebugManager = NULL;

  SAFE_RELEASE(m_DataUploadList);
  SAFE_RELEASE(m_DataUploadAlloc);

  SAFE_RELEASE(m_Alloc);
  SAFE_RELEASE(m_GPUSyncFence);
  CloseHandle(m_GPUSyncHandle);
}

void WrappedID3D12Device::GPUSync(ID3D12CommandQueue *queue, ID3D12Fence *fence)
{
  m_GPUSyncCounter++;

  if(queue == NULL)
    queue = GetQueue();

  if(fence == NULL)
    fence = m_GPUSyncFence;

  HRESULT hr = queue->Signal(fence, m_GPUSyncCounter);
  fence->SetEventOnCompletion(m_GPUSyncCounter, m_GPUSyncHandle);
  WaitForSingleObject(m_GPUSyncHandle, 10000);

  RDCASSERTEQUAL(hr, S_OK);
  hr = m_pDevice->GetDeviceRemovedReason();
  RDCASSERTEQUAL(hr, S_OK);
}

void WrappedID3D12Device::GPUSyncAllQueues()
{
  for(size_t i = 0; i < m_QueueFences.size(); i++)
    GPUSync(m_Queues[i], m_QueueFences[i]);
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

    if(IsReplayMode(m_State))
    {
      GetResourceManager()->AddLiveResource(GetResID(ret), ret);
    }
  }

  m_InternalCmds.pendingcmds.push_back(ret);

  return ret;
}

ID3D12GraphicsCommandList *WrappedID3D12Device::GetInitialStateList()
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

  return initStateCurList;
}

void WrappedID3D12Device::CloseInitialStateList()
{
  D3D12MarkerRegion::End(initStateCurList);
  initStateCurList->Close();
  initStateCurList = NULL;
  initStateCurBatch = 0;
}

void WrappedID3D12Device::ExecuteList(ID3D12GraphicsCommandList *list, ID3D12CommandQueue *queue)
{
  if(queue == NULL)
    queue = GetQueue();

  ID3D12CommandList *l = list;
  queue->ExecuteCommandLists(1, &l);

  for(auto it = m_InternalCmds.pendingcmds.begin(); it != m_InternalCmds.pendingcmds.end(); ++it)
  {
    if(list == *it)
    {
      m_InternalCmds.pendingcmds.erase(it);
      break;
    }
  }

  m_InternalCmds.submittedcmds.push_back(list);
}

void WrappedID3D12Device::ExecuteLists(ID3D12CommandQueue *queue)
{
  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  vector<ID3D12CommandList *> cmds;
  cmds.resize(m_InternalCmds.pendingcmds.size());
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = m_InternalCmds.pendingcmds[i];

  if(queue == NULL)
    queue = GetQueue();

  queue->ExecuteCommandLists((UINT)cmds.size(), &cmds[0]);

  m_InternalCmds.submittedcmds.insert(m_InternalCmds.submittedcmds.end(),
                                      m_InternalCmds.pendingcmds.begin(),
                                      m_InternalCmds.pendingcmds.end());
  m_InternalCmds.pendingcmds.clear();
}

void WrappedID3D12Device::FlushLists(bool forceSync, ID3D12CommandQueue *queue)
{
  if(!m_InternalCmds.submittedcmds.empty() || forceSync)
  {
    GPUSync(queue);

    if(!m_InternalCmds.submittedcmds.empty())
      m_InternalCmds.freecmds.insert(m_InternalCmds.freecmds.end(),
                                     m_InternalCmds.submittedcmds.begin(),
                                     m_InternalCmds.submittedcmds.end());
    m_InternalCmds.submittedcmds.clear();

    if(m_InternalCmds.pendingcmds.empty())
      m_Alloc->Reset();
  }
}

const DrawcallDescription *WrappedID3D12Device::GetDrawcall(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}

void WrappedID3D12Device::ProcessChunk(ReadSerialiser &ser, D3D12Chunk context)
{
  switch(context)
  {
    case D3D12Chunk::Device_CreateCommandQueue:
      Serialise_CreateCommandQueue(ser, NULL, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreateCommandAllocator:
      Serialise_CreateCommandAllocator(ser, D3D12_COMMAND_LIST_TYPE_DIRECT, IID(), NULL);
      break;

    case D3D12Chunk::Device_CreateGraphicsPipeline:
      Serialise_CreateGraphicsPipelineState(ser, NULL, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreateComputePipeline:
      Serialise_CreateComputePipelineState(ser, NULL, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreateDescriptorHeap:
      Serialise_CreateDescriptorHeap(ser, NULL, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreateRootSignature:
      Serialise_CreateRootSignature(ser, 0, NULL, 0, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreateCommandSignature:
      Serialise_CreateCommandSignature(ser, NULL, NULL, IID(), NULL);
      break;

    case D3D12Chunk::Device_CreateHeap: Serialise_CreateHeap(ser, NULL, IID(), NULL); break;
    case D3D12Chunk::Device_CreateCommittedResource:
      Serialise_CreateCommittedResource(ser, NULL, D3D12_HEAP_FLAG_NONE, NULL,
                                        D3D12_RESOURCE_STATE_COMMON, NULL, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreatePlacedResource:
      Serialise_CreatePlacedResource(ser, NULL, 0, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, IID(),
                                     NULL);
      break;
    case D3D12Chunk::Device_CreateReservedResource:
      Serialise_CreateReservedResource(ser, NULL, D3D12_RESOURCE_STATE_COMMON, NULL, IID(), NULL);
      break;

    case D3D12Chunk::Device_CreateQueryHeap:
      Serialise_CreateQueryHeap(ser, NULL, IID(), NULL);
      break;
    case D3D12Chunk::Device_CreateFence:
      Serialise_CreateFence(ser, 0, D3D12_FENCE_FLAG_NONE, IID(), NULL);
      break;
    case D3D12Chunk::SetName: Serialise_SetName(ser, 0x0, ""); break;
    case D3D12Chunk::SetShaderDebugPath: Serialise_SetShaderDebugPath(ser, NULL, NULL); break;
    case D3D12Chunk::CreateSwapBuffer:
      Serialise_WrapSwapchainBuffer(ser, NULL, NULL, 0, NULL);
      break;
    case D3D12Chunk::CaptureScope: Serialise_CaptureScope(ser); break;
    default:
    {
      SystemChunk system = (SystemChunk)context;
      if(system == SystemChunk::DriverInit)
      {
        D3D12InitParams InitParams;
        SERIALISE_ELEMENT(InitParams);
      }
      else if(system == SystemChunk::InitialContentsList)
      {
        GetResourceManager()->CreateInitialContents(ser);
      }
      else if(system == SystemChunk::InitialContents)
      {
        GetResourceManager()->Serialise_InitialState(ser, ResourceId(), NULL);
      }
      else if(system < SystemChunk::FirstDriverChunk)
      {
        RDCERR("Unexpected system chunk in capture data: %u", system);
        ser.SkipCurrentChunk();
      }
      else
      {
        RDCERR("Unexpected chunk %s", ToStr(context).c_str());
      }
      break;
    }
  }
}

void WrappedID3D12Device::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(reader->IsErrored())
    return;

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData.version = m_StructuredFile->version = m_SectionVersion;

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

    ProcessChunk(ser, context);

    ser.EndChunk();

    uint64_t offsetEnd = reader->GetOffset();

    RenderDoc::Inst().SetProgress(FileInitialRead, float(offsetEnd) / float(reader->GetSize()));

    if(context == D3D12Chunk::CaptureScope)
    {
      m_FrameRecord.frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      m_Queue->SetFrameReader(new StreamReader(reader, frameDataSize));

      if(!IsStructuredExporting(m_State))
        ApplyInitialContents();

      m_Queue->ReplayLog(m_State, 0, 0, false);
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if(context == D3D12Chunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
      break;
  }

  // steal the structured data for ourselves
  m_StructuredFile->swap(m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = &m_StoredStructuredData;

  if(!IsStructuredExporting(m_State))
  {
    GetFrameRecord().drawcallList = m_Queue->GetParentDrawcall().Bake();

    m_Queue->GetParentDrawcall().children.clear();

    DrawcallDescription *previous = NULL;
    SetupDrawcallPointers(&m_Drawcalls, GetFrameRecord().drawcallList, NULL, previous);

    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    for(auto it = cmd.m_BakedCmdListInfo.begin(); it != cmd.m_BakedCmdListInfo.end(); it++)
    {
      for(size_t i = 0; i < it->second.crackedLists.size(); i++)
        it->second.crackedLists[i]->Release();
      it->second.crackedLists.clear();
    }

    for(auto it = cmd.m_CrackedAllocators.begin(); it != cmd.m_CrackedAllocators.end(); it++)
      SAFE_RELEASE(it->second);
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

  m_FrameRecord.frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  m_FrameRecord.frameInfo.compressedFileSize = rdc->GetSectionProperties(sectionIdx).compressedSize;
  m_FrameRecord.frameInfo.persistentSize = frameDataSize;
  m_FrameRecord.frameInfo.initDataSize =
      chunkInfos[(D3D12Chunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_FrameRecord.frameInfo.persistentSize);
}

void WrappedID3D12Device::ReplayLog(uint32_t startEventID, uint32_t endEventID,
                                    ReplayLogType replayType)
{
  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;

    m_GPUSyncCounter++;

    // I'm not sure the reason for this, but the debug layer warns about being unable to resubmit
    // command lists due to the 'previous queue fence' not being ready yet, even if no fences are
    // signalled or waited. So instead we just signal a dummy fence each new 'frame'
    for(size_t i = 0; i < m_Queues.size(); i++)
      m_Queues[i]->Signal(m_QueueFences[i], m_GPUSyncCounter);
  }

  if(!partial)
  {
    {
      D3D12MarkerRegion apply(GetQueue(), "!!!!RenderDoc Internal: ApplyInitialContents");
      ApplyInitialContents();
      GetResourceManager()->ReleaseInFrameResources();
    }

    ExecuteLists();
    FlushLists(true);
  }

  m_State = CaptureState::ActiveReplaying;

  D3D12MarkerRegion::Set(
      GetQueue(), StringFormat::Fmt("!!!!RenderDoc Internal: RenderDoc Replay %d (%d): %u->%u",
                                    (int)replayType, (int)partial, startEventID, endEventID));

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
      m_Queue->ReplayLog(m_State, startEventID, endEventID, partial);
    else if(replayType == eReplay_WithoutDraw)
      m_Queue->ReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    else if(replayType == eReplay_OnlyDraw)
      m_Queue->ReplayLog(m_State, endEventID, endEventID, partial);
    else
      RDCFATAL("Unexpected replay type");

    if(cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList != NULL)
    {
      ID3D12GraphicsCommandList *list = cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList;

      list->Close();

      ExecuteLists();

      cmd.m_Partial[D3D12CommandData::Primary].outsideCmdList = NULL;
    }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    FlushLists(true);
#endif
  }

  D3D12MarkerRegion::Set(GetQueue(), "!!!!RenderDoc Internal: Done replay");

  // ensure all UAV writes have finished before subsequent work
  ID3D12GraphicsCommandList *list = GetNewList();

  D3D12_RESOURCE_BARRIER uavBarrier = {};
  uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  list->ResourceBarrier(1, &uavBarrier);

  list->Close();

  ExecuteLists();
}
