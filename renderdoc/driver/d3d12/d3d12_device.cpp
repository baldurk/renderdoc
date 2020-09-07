/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "d3d12_rendertext.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

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
  if(riid == __uuidof(ID3D12InfoQueue) || riid == __uuidof(ID3D12DebugDevice) ||
     riid == __uuidof(ID3D12Device) || riid == __uuidof(ID3D12Device1) ||
     riid == __uuidof(ID3D12Device2) || riid == __uuidof(ID3D12Device3) ||
     riid == __uuidof(ID3D12Device4) || riid == __uuidof(ID3D12Device5) ||
     riid == __uuidof(ID3D12Device6) || riid == __uuidof(ID3D12Device7) ||
     riid == __uuidof(ID3D12Device8))
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
  if(riid == __uuidof(ID3D12InfoQueue) || riid == __uuidof(ID3D12DebugDevice) ||
     riid == __uuidof(ID3D12Device) || riid == __uuidof(ID3D12Device1) ||
     riid == __uuidof(ID3D12Device2) || riid == __uuidof(ID3D12Device3) ||
     riid == __uuidof(ID3D12Device4) || riid == __uuidof(ID3D12Device5) ||
     riid == __uuidof(ID3D12Device6) || riid == __uuidof(ID3D12Device7) ||
     riid == __uuidof(ID3D12Device8))
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

  return m_pDevice.OpenSharedHandleInternal(D3D12Chunk::CompatDevice_CreateSharedResource, riid,
                                            ppResource);
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

  return m_pDevice.OpenSharedHandleInternal(D3D12Chunk::CompatDevice_CreateSharedHeap, riid, ppHeap);
}

HRESULT STDMETHODCALLTYPE WrappedCompatibilityDevice::ReflectSharedProperties(
    _In_ ID3D12Object *pHeapOrResource, D3D12_REFLECT_SHARED_PROPERTY ReflectType,
    _Out_writes_bytes_(DataSize) void *pData, UINT DataSize)
{
  return m_pReal->ReflectSharedProperties(Unwrap(pHeapOrResource), ReflectType, pData, DataSize);
}

WrappedID3D12Device::WrappedID3D12Device(ID3D12Device *realDevice, D3D12InitParams params,
                                         bool enabledDebugLayer)
    : m_RefCounter(realDevice, false),
      m_SoftRefCounter(NULL, false),
      m_pDevice(realDevice),
      m_debugLayerEnabled(enabledDebugLayer),
      m_WrappedDownlevel(*this),
      m_DRED(*this),
      m_DREDSettings(*this),
      m_CompatDevice(*this)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D12Device));

  m_SectionVersion = D3D12InitParams::CurrentVersion;

  m_Replay = new D3D12Replay(this);

  m_StructuredFile = &m_StoredStructuredData;

  RDCEraseEl(m_D3D12Opts);
  RDCEraseEl(m_D3D12Opts1);
  RDCEraseEl(m_D3D12Opts2);
  RDCEraseEl(m_D3D12Opts3);

  m_pDevice1 = NULL;
  m_pDevice2 = NULL;
  m_pDevice3 = NULL;
  m_pDevice4 = NULL;
  m_pDevice5 = NULL;
  m_pDevice6 = NULL;
  m_pDownlevel = NULL;
  if(m_pDevice)
  {
    m_pDevice->QueryInterface(__uuidof(ID3D12Device1), (void **)&m_pDevice1);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device2), (void **)&m_pDevice2);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device3), (void **)&m_pDevice3);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device4), (void **)&m_pDevice4);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device5), (void **)&m_pDevice5);
    m_pDevice->QueryInterface(__uuidof(ID3D12Device6), (void **)&m_pDevice6);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedData), (void **)&m_DRED.m_pReal);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceRemovedExtendedDataSettings),
                              (void **)&m_DREDSettings.m_pReal);
    m_pDevice->QueryInterface(__uuidof(ID3D12DeviceDownlevel), (void **)&m_pDownlevel);
    m_pDevice->QueryInterface(__uuidof(ID3D12CompatibilityDevice), (void **)&m_CompatDevice.m_pReal);

    for(size_t i = 0; i < ARRAY_COUNT(m_DescriptorIncrements); i++)
      m_DescriptorIncrements[i] =
          m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));

    HRESULT hr = S_OK;

    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &m_D3D12Opts,
                                        sizeof(m_D3D12Opts));
    RDCASSERTEQUAL(hr, S_OK);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &m_D3D12Opts1,
                                        sizeof(m_D3D12Opts1));
    RDCASSERTEQUAL(hr, S_OK);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &m_D3D12Opts2,
                                        sizeof(m_D3D12Opts2));
    RDCASSERTEQUAL(hr, S_OK);
    hr = m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &m_D3D12Opts3,
                                        sizeof(m_D3D12Opts3));
    RDCASSERTEQUAL(hr, S_OK);
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
  m_DataUploadList = NULL;
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
      m_ResourceList = new std::map<ResourceId, WrappedID3D12Resource1 *>();
      m_PipelineList = new rdcarray<WrappedID3D12PipelineState *>();
    }

    m_FrameCaptureRecord = NULL;

    ResourceIDGen::SetReplayResourceIDs();
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

  if(!RenderDoc::Inst().IsReplayApp())
  {
    FirstFrame(NULL);
  }
}

WrappedID3D12Device::~WrappedID3D12Device()
{
  {
    SCOPED_LOCK(m_DeviceWrappersLock);
    m_DeviceWrappers.erase(m_pDevice);
  }

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
  SAFE_RELEASE(m_DREDSettings.m_pReal);
  SAFE_RELEASE(m_CompatDevice.m_pReal);
  SAFE_RELEASE(m_pDownlevel);
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

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
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
    return E_NOINTERFACE;
  }
  else if(riid == __uuidof(ID3D12Device8))
  {
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
  else
  {
    WarnUnknownGUID("ID3D12Device", riid);
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

void WrappedID3D12Device::AddCaptureSubmission()
{
  if(IsActiveCapturing(m_State))
  {
    // 15 is quite a lot of submissions.
    const int expectedMaxSubmissions = 15;

    RenderDoc::Inst().SetProgress(CaptureProgress::FrameCapture, FakeProgress(m_SubmitCounter, 15));
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
    RenderDoc::Inst().StartFrameCapture((ID3D12Device *)this, swapper ? swapper->GetHWND() : NULL);

    m_AppControlledCapture = false;
    m_CapturedFrames.back().frameNumber = 0;
  }
}

void WrappedID3D12Device::ApplyBarriers(rdcarray<D3D12_RESOURCE_BARRIER> &barriers)
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

    WrappedID3D12Resource1 *wrapped = (WrappedID3D12Resource1 *)res;
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
    WrappedID3D12Resource1 *wrapped = (WrappedID3D12Resource1 *)pRes;
    wrapped->AddInternalRef();
  }
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_WrapSwapchainBuffer(SerialiserType &ser, IDXGISwapper *swapper,
                                                        DXGI_FORMAT bufferFormat, UINT Buffer,
                                                        IUnknown *realSurface)
{
  WrappedID3D12Resource1 *pRes = (WrappedID3D12Resource1 *)realSurface;

  SERIALISE_ELEMENT(Buffer);
  SERIALISE_ELEMENT_LOCAL(SwapbufferID, GetResID(pRes)).TypedAs("ID3D12Resource *"_lit);
  SERIALISE_ELEMENT_LOCAL(BackbufferDescriptor, pRes->GetDesc());

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
      WrappedID3D12Resource1 *wrapped = new WrappedID3D12Resource1(fakeBB, this);
      fakeBB = wrapped;

      fakeBB->SetName(L"Swap Chain Buffer");

      GetResourceManager()->AddLiveResource(SwapbufferID, fakeBB);

      m_BackbufferFormat[wrapped->GetResourceID()] = SwapbufferFormat;

      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      states = {D3D12_RESOURCE_STATE_PRESENT};
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
  else if(WrappedID3D12Resource1::IsAlloc(query))
  {
    // this could be possible if we're doing downlevel presenting
    pRes = query;
  }
  else
  {
    pRes = new WrappedID3D12Resource1((ID3D12Resource *)realSurface, this);

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

      WrappedID3D12Resource1 *wrapped = (WrappedID3D12Resource1 *)pRes;

      wrapped->SetResourceRecord(record);

      WriteSerialiser &ser = GetThreadSerialiser();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::CreateSwapBuffer);

      Serialise_WrapSwapchainBuffer(ser, swapper, bufferFormat, buffer, pRes);

      record->AddChunk(scope.Get());

      {
        SCOPED_LOCK(m_ResourceStatesLock);
        SubresourceStateVector &states = m_ResourceStates[id];

        states = {D3D12_RESOURCE_STATE_PRESENT};
      }
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
  OpenSharedHandleInternal(D3D12Chunk::Device_ExternalDXGIResource, __uuidof(IDXGIResource),
                           &voidRes);
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

  {
    SCOPED_LOCK(m_MapsLock);
    map.res = Resource;
    map.subres = Subresource;

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
    range = *pWrittenRange;

  if(capframe && range.End > range.Begin)
    MapDataWrite(Resource, Subresource, mapPtr, range);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_MapDataWrite(SerialiserType &ser, ID3D12Resource *Resource,
                                                 UINT Subresource, byte *MappedData,
                                                 D3D12_RANGE range)
{
  SERIALISE_ELEMENT(Resource);
  SERIALISE_ELEMENT(Subresource);

  // tracks if we've already uploaded the data to a persistent buffer and don't need to re-serialise
  // pointlessly
  bool alreadyuploaded = false;

  if(IsReplayingAndReading() && Resource)
  {
    ResourceId origid = GetResourceManager()->GetOriginalID(GetResID(Resource));
    if(m_UploadResourceIds.find(origid) != m_UploadResourceIds.end())
    {
      // while loading, we still have yet to create the buffer so it's not already uploaded. Once
      // we've loaded, it is.
      alreadyuploaded = IsActiveReplaying(m_State);
    }
  }

  MappedData += range.Begin;

  // we serialise MappedData manually, so that when we don't need it we just skip instead of
  // actually allocating and memcpy'ing the buffer.
  ScopedDeserialiseArray<SerialiserType, byte *> deserialise_map(ser, &MappedData,
                                                                 range.End - range.Begin);
  SerialiserFlags flags = SerialiserFlags::AllocateMemory;
  if(alreadyuploaded)
  {
    // set the array to explicitly NULL (so it doesn't crash on deserialise) and don't allocate.
    // This will cause it to be skipped
    MappedData = NULL;
    flags = SerialiserFlags::NoFlags;
  }

  ser.Serialise("MappedData"_lit, MappedData, range.End - range.Begin, flags);

  SERIALISE_ELEMENT(range);

  uint64_t rangeSize = range.End - range.Begin;

  SERIALISE_CHECK_READ_ERRORS();

  // don't do anything if end <= begin because the range is empty.
  if(IsReplayingAndReading() && Resource && range.End > range.Begin)
  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    if(IsLoading(m_State))
      cmd.AddUsage(GetResID(Resource), ResourceUsage::CPUWrite);

    ResourceId origid = GetResourceManager()->GetOriginalID(GetResID(Resource));
    if(m_UploadResourceIds.find(origid) != m_UploadResourceIds.end())
    {
      ID3D12Resource *uploadBuf = GetUploadBuffer(cmd.m_CurChunkOffset, rangeSize);

      SetObjName(uploadBuf,
                 StringFormat::Fmt("Map data write, %llu bytes for %s/%u @ %llu", rangeSize,
                                   ToStr(origid).c_str(), Subresource, cmd.m_CurChunkOffset));

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
          RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
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
        RDCERR("Failed to map resource on replay HRESULT: %s", ToStr(hr).c_str());
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

  GetResourceManager()->MarkResourceFrameReferenced(GetResID(Resource), eFrameRef_PartialWrite);
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

  SERIALISE_ELEMENT_ARRAY(pSrcData, dataSize);
  SERIALISE_ELEMENT(dataSize).Hidden();

  SERIALISE_ELEMENT(SrcRowPitch);
  SERIALISE_ELEMENT(SrcDepthPitch);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && Resource)
  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();

    if(IsLoading(m_State))
      cmd.AddUsage(GetResID(Resource), ResourceUsage::CPUWrite);

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

  bool activeWindow = RenderDoc::Inst().IsActiveWindow((ID3D12Device *)this, swapper->GetHWND());

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

        int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
        rdcstr overlayText =
            RenderDoc::Inst().GetOverlayText(RDCDriver::D3D12, m_FrameCounter, flags);

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
    return S_OK;

  // kill any current capture that isn't application defined
  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
    RenderDoc::Inst().EndFrameCapture((ID3D12Device *)this, swapper->GetHWND());

  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter))
  {
    RenderDoc::Inst().StartFrameCapture((ID3D12Device *)this, swapper->GetHWND());

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
      .TypedAs("ID3D12Resource *"_lit);

  // we don't do anything with these parameters, they're just here to store
  // them for user benefits
  SERIALISE_ELEMENT(SyncInterval);
  SERIALISE_ELEMENT(Flags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && IsLoading(m_State))
  {
    D3D12CommandData &cmd = *m_Queue->GetCommandData();
    cmd.AddEvent();

    DrawcallDescription draw;

    draw.name = StringFormat::Fmt("Present(%s)", ToStr(PresentedBackbuffer).c_str());
    draw.flags |= DrawFlags::Present;

    cmd.m_LastPresentedImage = PresentedBackbuffer;
    draw.copyDestination = PresentedBackbuffer;

    cmd.AddDrawcall(draw, true);
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
  rdcarray<D3D12_RESOURCE_BARRIER> barriers;

  {
    SCOPED_LOCK(m_ResourceStatesLock);    // not needed on replay, but harmless also
    GetResourceManager()->SerialiseResourceStates(ser, barriers, m_ResourceStates);
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && !barriers.empty())
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

template bool WrappedID3D12Device::Serialise_BeginCaptureFrame(ReadSerialiser &ser);
template bool WrappedID3D12Device::Serialise_BeginCaptureFrame(WriteSerialiser &ser);

void WrappedID3D12Device::EndCaptureFrame()
{
  WriteSerialiser &ser = GetThreadSerialiser();
  ser.SetDrawChunk();
  SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureEnd);

  // here for compatibility reasons, this used to store the presented Resource.
  SERIALISE_ELEMENT_LOCAL(PresentedBackbuffer, ResourceId()).Hidden();

  m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedID3D12Device::StartFrameCapture(void *dev, void *wnd)
{
  if(!IsBackgroundCapturing(m_State))
    return;

  RDCLOG("Starting capture");

  m_CaptureTimer.Restart();

  m_AppControlledCapture = true;

  m_SubmitCounter = 0;

  FrameDescription frame;
  frame.frameNumber = ~0U;
  frame.captureTime = Timing::GetUnixTimestamp();
  RDCEraseEl(frame.stats);
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

    m_RefQueues = m_Queues;
    m_RefBuffers = WrappedID3D12Resource1::AddRefBuffersBeforeCapture(GetResourceManager());
  }

  GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Read);
}

bool WrappedID3D12Device::EndFrameCapture(void *dev, void *wnd)
{
  if(!IsActiveCapturing(m_State))
    return true;

  IDXGISwapper *swapper = NULL;
  SwapPresentInfo swapInfo = {};

  if(wnd)
  {
    for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    {
      if(it->first->GetHWND() == wnd)
      {
        swapper = it->first;
        swapInfo = it->second;
        break;
      }
    }

    if(swapper == NULL)
    {
      RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
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

    if(ContainsExecuteIndirect)
      WrappedID3D12Resource1::RefBuffers(GetResourceManager());

    m_State = CaptureState::BackgroundCapturing;

    GPUSync();

    {
      SCOPED_LOCK(m_MapsLock);
      for(auto it = m_Maps.begin(); it != m_Maps.end(); ++it)
        GetWrapped(it->res)->FreeShadow();
    }
  }

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
  }

  RDCLOG("Captured D3D12 frame with %f MB capture section in %f seconds",
         double(captureWriter->GetOffset()) / (1024.0 * 1024.0),
         m_CaptureTimer.GetMilliseconds() / 1000.0);

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

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  return true;
}

bool WrappedID3D12Device::DiscardFrameCapture(void *dev, void *wnd)
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

    {
      SCOPED_LOCK(m_MapsLock);
      for(auto it = m_Maps.begin(); it != m_Maps.end(); ++it)
        GetWrapped(it->res)->FreeShadow();
    }

    queues = m_Queues;
  }

  m_HeaderChunk->Delete();
  m_HeaderChunk = NULL;

  for(auto it = queues.begin(); it != queues.end(); ++it)
    (*it)->ClearAfterCapture();

  // remove the reference held during capture, potentially releasing the queue.
  for(WrappedID3D12CommandQueue *q : m_RefQueues)
    q->Release();

  for(ID3D12Resource *r : m_RefBuffers)
    r->Release();

  GetResourceManager()->MarkUnwrittenResources();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

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
    graphicsDesc.RasterizerState = desc.RasterizerState;
    // graphicsDesc.DepthStencilState = desc.DepthStencilState;
    {
      graphicsDesc.DepthStencilState.DepthEnable = desc.DepthStencilState.DepthEnable;
      graphicsDesc.DepthStencilState.DepthWriteMask = desc.DepthStencilState.DepthWriteMask;
      graphicsDesc.DepthStencilState.DepthFunc = desc.DepthStencilState.DepthFunc;
      graphicsDesc.DepthStencilState.StencilEnable = desc.DepthStencilState.StencilEnable;
      graphicsDesc.DepthStencilState.StencilReadMask = desc.DepthStencilState.StencilReadMask;
      graphicsDesc.DepthStencilState.StencilWriteMask = desc.DepthStencilState.StencilWriteMask;
      graphicsDesc.DepthStencilState.FrontFace = desc.DepthStencilState.FrontFace;
      graphicsDesc.DepthStencilState.BackFace = desc.DepthStencilState.BackFace;
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
    // look up the EID this drawcall came from
    D3D12CommandData::DrawcallUse use(cmd.m_CurChunkOffset, 0);
    auto it = std::lower_bound(cmd.m_DrawcallUses.begin(), cmd.m_DrawcallUses.end(), use);
    RDCASSERT(it != cmd.m_DrawcallUses.end());

    if(it != cmd.m_DrawcallUses.end())
      msg.eventId = it->eventId;
    else
      RDCERR("Couldn't locate drawcall use for current chunk offset %llu", cmd.m_CurChunkOffset);

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
    // last one - due to our replay with real and cracked lists we get many duplicated messages
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
  // don't allow naming device contexts or command lists so we know this chunk
  // is always on a pre-capture chunk.
  if(IsCaptureMode(m_State) && !WrappedID3D12GraphicsCommandList::IsAlloc(pResource) &&
     !WrappedID3D12CommandQueue::IsAlloc(pResource))
  {
    D3D12ResourceRecord *record = GetRecord(pResource);

    if(record == NULL)
      record = m_DeviceRecord;

    {
      WriteSerialiser &ser = GetThreadSerialiser();
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
          end->Delete();
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

HRESULT STDMETHODCALLTYPE WrappedID3D12Device::QueryVideoMemoryInfo(
    UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
    _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
{
  return m_pDownlevel->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Device::SetBackgroundProcessingMode(
    D3D12_BACKGROUND_PROCESSING_MODE Mode, D3D12_MEASUREMENTS_ACTION MeasurementsAction,
    _In_opt_ HANDLE hEventToSignalUponCompletion, _Out_opt_ BOOL *pbFurtherMeasurementsDesired)
{
  return m_pDevice6->SetBackgroundProcessingMode(
      Mode, MeasurementsAction, hEventToSignalUponCompletion, pbFurtherMeasurementsDesired);
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

void WrappedID3D12Device::CreateInternalResources()
{
  if(IsReplayMode(m_State))
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

  // we don't want replay-only shaders added in WrappedID3D12Shader to pollute the list of resources
  WrappedID3D12Shader::InternalResources(true);

  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_Alloc);
  ((WrappedID3D12CommandAllocator *)m_Alloc)->SetInternal(true);
  InternalRef();
  CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_GPUSyncFence);
  InternalRef();
  m_GPUSyncHandle = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  GetResourceManager()->SetInternalResource(m_Alloc);
  GetResourceManager()->SetInternalResource(m_GPUSyncFence);

  CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                         (void **)&m_DataUploadAlloc);
  ((WrappedID3D12CommandAllocator *)m_DataUploadAlloc)->SetInternal(true);
  InternalRef();

  GetResourceManager()->SetInternalResource(m_DataUploadAlloc);

  CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_DataUploadAlloc, NULL,
                    __uuidof(ID3D12GraphicsCommandList), (void **)&m_DataUploadList);
  InternalRef();

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

  m_DataUploadList->Close();

  m_GPUSyncCounter = 0;

  if(m_ShaderCache == NULL)
    m_ShaderCache = new D3D12ShaderCache();

  if(m_TextRenderer == NULL)
    m_TextRenderer = new D3D12TextRenderer(this);

  m_Replay->CreateResources();

  WrappedID3D12Shader::InternalResources(false);
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

ID3D12GraphicsCommandListX *WrappedID3D12Device::GetNewList()
{
  ID3D12GraphicsCommandListX *ret = NULL;

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
  // nothing to do
  if(m_InternalCmds.pendingcmds.empty())
    return;

  rdcarray<ID3D12CommandList *> cmds;
  cmds.resize(m_InternalCmds.pendingcmds.size());
  for(size_t i = 0; i < cmds.size(); i++)
    cmds[i] = m_InternalCmds.pendingcmds[i];

  if(queue == NULL)
    queue = GetQueue();

  queue->ExecuteCommandListsInternal((UINT)cmds.size(), &cmds[0], InFrameCaptureBoundary, false);

  m_InternalCmds.submittedcmds.append(m_InternalCmds.pendingcmds);
  m_InternalCmds.pendingcmds.clear();
}

void WrappedID3D12Device::FlushLists(bool forceSync, ID3D12CommandQueue *queue)
{
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

const DrawcallDescription *WrappedID3D12Device::GetDrawcall(uint32_t eventId)
{
  if(eventId >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventId];
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
  ResourceId parentId = GetResourceManager()->GetOriginalID(GetResID(parent));

  DerivedResource(parentId, child);
}

void WrappedID3D12Device::DerivedResource(ResourceId parent, ResourceId child)
{
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

ReplayStatus WrappedID3D12Device::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return ReplayStatus::FileCorrupted;

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
    delete reader;
    return ReplayStatus::FileIOFailed;
  }

  ReadSerialiser ser(reader, Ownership::Stream);

  APIProps.DXILShaders = m_UsedDXIL = m_InitParams.usedDXIL;

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers, m_TimeBase, m_TimeFrequency);

  m_StructuredFile = &ser.GetStructuredFile();

  m_StoredStructuredData.version = m_StructuredFile->version = m_SectionVersion;

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
      return ReplayStatus::APIDataCorrupted;

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(reader->IsErrored())
      return ReplayStatus::APIDataCorrupted;

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
      return m_FailedReplayStatus;

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

      ReplayStatus status = m_Queue->ReplayLog(m_State, 0, 0, false);

      if(status != ReplayStatus::Succeeded)
        return status;
    }

    chunkInfos[context].total += timer.GetMilliseconds();
    chunkInfos[context].totalsize += offsetEnd - offsetStart;
    chunkInfos[context].count++;

    if((SystemChunk)context == SystemChunk::CaptureScope || reader->IsErrored() || reader->AtEnd())
      break;
  }

  // steal the structured data for ourselves
  m_StructuredFile->Swap(m_StoredStructuredData);

  // and in future use this file.
  m_StructuredFile = &m_StoredStructuredData;

  if(!IsStructuredExporting(m_State))
  {
    GetReplay()->WriteFrameRecord().drawcallList = m_Queue->GetParentDrawcall().Bake();

    m_Queue->GetParentDrawcall().children.clear();

    SetupDrawcallPointers(m_Drawcalls, GetReplay()->WriteFrameRecord().drawcallList);

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

  GetReplay()->WriteFrameRecord().frameInfo.uncompressedFileSize =
      rdc->GetSectionProperties(sectionIdx).uncompressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.compressedFileSize =
      rdc->GetSectionProperties(sectionIdx).compressedSize;
  GetReplay()->WriteFrameRecord().frameInfo.persistentSize = frameDataSize;
  GetReplay()->WriteFrameRecord().frameInfo.initDataSize =
      chunkInfos[(D3D12Chunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           GetReplay()->WriteFrameRecord().frameInfo.persistentSize);

  return ReplayStatus::Succeeded;
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

    FlushLists(true);

    // take this opportunity to reset command allocators to ensure we don't steadily leak over time.
    if(m_DataUploadAlloc)
      m_DataUploadAlloc->Reset();

    for(ID3D12CommandAllocator *alloc : m_CommandAllocators)
      alloc->Reset();
  }

  if(!partial)
  {
    {
      D3D12MarkerRegion apply(GetQueue(), "!!!!RenderDoc Internal: ApplyInitialContents");
      ApplyInitialContents();
    }

    ExecuteLists();
    FlushLists(true);
  }

  m_State = CaptureState::ActiveReplaying;

  D3D12MarkerRegion::Set(
      GetQueue(), StringFormat::Fmt("!!!!RenderDoc Internal: RenderDoc Replay %d (%d): %u->%u",
                                    (int)replayType, (int)partial, startEventID, endEventID));

  if(!partial)
  {
    ID3D12GraphicsCommandList *beginList = GetNewList();
    {
      rdcwstr text = StringFormat::UTF82Wide(AMDRGPControl::GetBeginMarker());
      UINT size = UINT(text.length() * sizeof(wchar_t));
      beginList->SetMarker(0, text.c_str(), size);
    }
    beginList->Close();
    ExecuteLists();
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
    // one drawcall, or if start event ID is > 0 we assume the outside code
    // has chosen a subsection that lies within a command list
    if(partial)
    {
      ID3D12GraphicsCommandListX *list = cmd.m_OutsideCmdList = GetNewList();

      cmd.m_RenderState.ApplyState(this, list);
    }

    ReplayStatus status = ReplayStatus::Succeeded;

    if(replayType == eReplay_Full)
      status = m_Queue->ReplayLog(m_State, startEventID, endEventID, partial);
    else if(replayType == eReplay_WithoutDraw)
      status = m_Queue->ReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
    else if(replayType == eReplay_OnlyDraw)
      status = m_Queue->ReplayLog(m_State, endEventID, endEventID, partial);
    else
      RDCFATAL("Unexpected replay type");

    RDCASSERTEQUAL(status, ReplayStatus::Succeeded);

    if(cmd.m_OutsideCmdList != NULL)
    {
      ID3D12GraphicsCommandList *list = cmd.m_OutsideCmdList;

      list->Close();

      ExecuteLists();

      cmd.m_OutsideCmdList = NULL;
    }

    cmd.m_RenderState =
        cmd.m_BakedCmdListInfo[cmd.m_Partial[D3D12CommandData::Primary].partialParent].state;

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    FlushLists(true);
#endif
  }

  D3D12MarkerRegion::Set(GetQueue(), "!!!!RenderDoc Internal: Done replay");

  // ensure all UAV writes have finished before subsequent work
  ID3D12GraphicsCommandList *list = GetNewList();
  {
    rdcwstr text = StringFormat::UTF82Wide(AMDRGPControl::GetEndMarker());
    UINT size = UINT(text.length() * sizeof(wchar_t));
    list->SetMarker(0, text.c_str(), size);
  }

  D3D12_RESOURCE_BARRIER uavBarrier = {};
  uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  list->ResourceBarrier(1, &uavBarrier);

  list->Close();

  ExecuteLists();
}
