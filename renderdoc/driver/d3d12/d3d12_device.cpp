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
    "ID3D11Device::Initialisation",
    "ID3D12Object::SetName",
    "ID3D12Resource::Release",
    "IDXGISwapChain::GetBuffer",

    "Capture",

    "ID3D12CommandQueue::BeginEvent",
    "ID3D12CommandQueue::SetMarker",
    "ID3D12CommandQueue::EndEvent",

    "DebugMessageList",

    "ContextBegin",
    "ContextEnd",

    "SetShaderDebugPath",
};

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

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

  m_DummyInfoQueue.m_pDevice = this;
  m_DummyDebug.m_pDevice = this;
  m_WrappedDebug.m_pDevice = this;

  m_AppControlledCapture = false;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    m_pSerialiser = NULL;

    ResourceIDGen::SetReplayResourceIDs();
  }
  else
  {
    m_State = WRITING_IDLE;
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, true);
  }

  m_ResourceManager = new D3D12ResourceManager(m_State, m_pSerialiser, this);

  if(m_pSerialiser)
    m_pSerialiser->SetChunkNameLookup(&GetChunkName);

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_DeviceRecord = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_DeviceRecord->DataInSerialiser = false;
    m_DeviceRecord->SpecialResource = true;
    m_DeviceRecord->Length = 0;
    m_DeviceRecord->NumSubResources = 0;
    m_DeviceRecord->SubResources = NULL;

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
      m_pInfoQueue->SetMuteDebugOutput(false);
  }
  else
  {
    RDCDEBUG("Couldn't get ID3D12InfoQueue.");
  }

  m_InitParams = *params;

  //////////////////////////////////////////////////////////////////////////
  // Compile time asserts

  RDCCOMPILE_ASSERT(ARRAY_COUNT(D3D12ChunkNames) == NUM_D3D12_CHUNKS - FIRST_CHUNK_ID,
                    "Not right number of chunk names");
}

WrappedID3D12Device::~WrappedID3D12Device()
{
  RenderDoc::Inst().RemoveDeviceFrameCapturer((ID3D12Device *)this);

  if(m_DeviceRecord)
  {
    RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
    m_DeviceRecord->Delete(GetResourceManager());
  }

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  SAFE_RELEASE(m_pInfoQueue);
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
      *ppvObject = new WrappedIDXGIDevice(real, this);
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
      *ppvObject = new WrappedIDXGIDevice1(real, this);
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
      *ppvObject = new WrappedIDXGIDevice2(real, this);
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
      *ppvObject = new WrappedIDXGIDevice3(real, this);
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

void WrappedID3D12Device::ReleaseSwapchainResources(WrappedIDXGISwapChain3 *swap)
{
  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC desc;
    swap->GetDesc(&desc);

    Keyboard::RemoveInputWindow(desc.OutputWindow);

    RenderDoc::Inst().RemoveFrameCapturer((ID3D12Device *)this, desc.OutputWindow);
  }

  auto it = m_SwapChains.find(swap);
  if(it != m_SwapChains.end())
    m_SwapChains.erase(it);
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

    RDCUNIMPLEMENTED("Creating fake backbuffer texture");

    // DXGI swap chain back buffers can be freely cast as a special-case.
    // translate the format to a typeless format to allow for this.
    // the original type will be stored in the texture below
    Descriptor.Format = GetTypelessFormat(Descriptor.Format);

    // create fake texture
    HRESULT hr = E_INVALIDARG;

    if(FAILED(hr))
    {
      RDCERR("Failed to create fake back buffer, HRESULT: 0x%08x", hr);
    }
    else
    {
      WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(fakeBB, this);
      fakeBB = wrapped;

      fakeBB->SetName(L"Serialised Swap Chain Buffer");

      GetResourceManager()->AddLiveResource(TexID, fakeBB);
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

  pRes->SetName(L"Swap Chain Backbuffer");

  ResourceId id = GetResID(pRes);

  // there shouldn't be a resource record for this texture as it wasn't created via
  // Create*Resource
  RDCASSERT(id != ResourceId() && !GetResourceManager()->HasResourceRecord(id));

  if(m_State >= WRITING)
  {
    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    record->DataInSerialiser = false;
    record->SpecialResource = true;
    record->Length = 0;
    record->NumSubResources = 0;
    record->SubResources = NULL;

    SCOPED_LOCK(m_D3DLock);

    SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);

    Serialise_WrapSwapchainBuffer(swap, swapDesc, buffer, pRes);

    record->AddChunk(scope.Get());
  }

  if(buffer == 0 && m_State >= WRITING)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = GetSRGBFormat(swapDesc->BufferDesc.Format);
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    D3D12NOTIMP("Creating RTV for rendering to back buffer");
    // m_pDevice->CreateRenderTargetView(Unwrap(pRes), &rtvDesc, rtv);

    m_SwapChains[swap] = rtv;
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

  return S_OK;
}

void WrappedID3D12Device::StartFrameCapture(void *dev, void *wnd)
{
  if(m_State != WRITING_IDLE)
    return;
}

bool WrappedID3D12Device::EndFrameCapture(void *dev, void *wnd)
{
  if(m_State != WRITING_CAPFRAME)
    return true;

  return false;
}
bool WrappedID3D12Device::Serialise_ReleaseResource(ID3D12DeviceChild *res)
{
  return true;
}

void WrappedID3D12Device::ReleaseResource(ID3D12DeviceChild *res)
{
  D3D12NOTIMP("ReleaseResource");
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
