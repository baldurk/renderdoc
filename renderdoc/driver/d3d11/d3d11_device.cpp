/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "d3d11_device.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_renderstate.h"
#include "d3d11_rendertext.h"
#include "d3d11_resources.h"
#include "d3d11_shader_cache.h"

WRAPPED_POOL_INST(WrappedID3D11Device);

WrappedID3D11Device *WrappedID3D11Device::m_pCurrentWrappedDevice = NULL;

void WrappedID3D11Device::NewSwapchainBuffer(IUnknown *backbuffer)
{
  WrappedID3D11Texture2D1 *wrapped = (WrappedID3D11Texture2D1 *)backbuffer;

  if(wrapped)
  {
    // keep ref as a 'view' (invisible to user)
    wrapped->ViewAddRef();
    wrapped->Release();
  }
}

WrappedID3D11Device::WrappedID3D11Device(ID3D11Device *realDevice, D3D11InitParams params)
    : m_RefCounter(realDevice, false),
      m_SoftRefCounter(NULL, false),
      m_pDevice(realDevice),
      m_ScratchSerialiser(new StreamWriter(1024), Ownership::Stream)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D11Device));

  m_SectionVersion = D3D11InitParams::CurrentVersion;

  uint32_t flags = WriteSerialiser::ChunkDuration | WriteSerialiser::ChunkTimestamp |
                   WriteSerialiser::ChunkThreadID;

  if(RenderDoc::Inst().GetCaptureOptions().captureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  m_ScratchSerialiser.SetChunkMetadataRecording(flags);
  m_ScratchSerialiser.SetVersion(D3D11InitParams::CurrentVersion);

  m_StructuredFile = &m_StoredStructuredData;

  m_pDevice1 = NULL;
  m_pDevice2 = NULL;
  m_pDevice3 = NULL;
  m_pDevice4 = NULL;
  m_pDevice5 = NULL;
  if(m_pDevice)
  {
    m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&m_pDevice1);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&m_pDevice2);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&m_pDevice3);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device4), (void **)&m_pDevice4);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device5), (void **)&m_pDevice5);
  }

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

  m_DummyInfoQueue.m_pDevice = this;
  m_DummyDebug.m_pDevice = this;
  m_WrappedDebug.m_pDevice = this;
  m_WrappedMultithread.m_pDevice = this;
  m_WrappedVideo.m_pDevice = this;

  m_FrameCounter = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;
  m_Failures = 0;

  m_ChunkAtomic = 0;

  m_AppControlledCapture = false;

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::LoadingReplaying;

    D3D11MarkerRegion::device = this;

    std::string shaderSearchPathString =
        RenderDoc::Inst().GetConfigSetting("shader.debug.searchPaths");
    split(shaderSearchPathString, m_ShaderSearchPaths, ';');

    ResourceIDGen::SetReplayResourceIDs();
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

  m_ResourceManager = new D3D11ResourceManager(this);

  m_ShaderCache = new D3D11ShaderCache(this);

  m_ScratchSerialiser.SetUserData(GetResourceManager());

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_DeviceRecord = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_DeviceRecord->ResType = Resource_Unknown;
    m_DeviceRecord->DataInSerialiser = false;
    m_DeviceRecord->InternalResource = true;
    m_DeviceRecord->Length = 0;
    m_DeviceRecord->NumSubResources = 0;
    m_DeviceRecord->SubResources = NULL;

    RenderDoc::Inst().AddDeviceFrameCapturer((ID3D11Device *)this, this);

    {
      IDXGIDevice *pDXGIDevice = NULL;
      HRESULT hr = m_pDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);

      if(FAILED(hr))
      {
        RDCERR("Couldn't get DXGI device from D3D device");
      }
      else
      {
        IDXGIAdapter *pDXGIAdapter = NULL;
        hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

        if(SUCCEEDED(hr))
        {
          DXGI_ADAPTER_DESC desc = {};
          pDXGIAdapter->GetDesc(&desc);

          GPUVendor vendor = GPUVendorFromPCIVendor(desc.VendorId);
          std::string descString = GetDriverVersion(desc);

          RDCLOG("New D3D11 device created: %s / %s", ToStr(vendor).c_str(), descString.c_str());

          SAFE_RELEASE(pDXGIAdapter);
        }
      }

      SAFE_RELEASE(pDXGIDevice);
    }
  }

  ID3D11DeviceContext *context = NULL;
  if(realDevice)
    realDevice->GetImmediateContext(&context);

  m_pImmediateContext = new WrappedID3D11DeviceContext(this, context);

  m_pImmediateContext->GetScratchSerialiser().SetChunkMetadataRecording(
      m_ScratchSerialiser.GetChunkMetadataRecording());

  m_pInfoQueue = NULL;
  if(realDevice)
  {
    realDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&m_pInfoQueue);
    realDevice->QueryInterface(__uuidof(ID3D11Debug), (void **)&m_WrappedDebug.m_pDebug);
    realDevice->QueryInterface(__uuidof(ID3D11Multithread), (void **)&m_WrappedMultithread.m_pReal);
    realDevice->QueryInterface(__uuidof(ID3D11VideoDevice), (void **)&m_WrappedVideo.m_pReal);
    realDevice->QueryInterface(__uuidof(ID3D11VideoDevice1), (void **)&m_WrappedVideo.m_pReal1);
    realDevice->QueryInterface(__uuidof(ID3D11VideoDevice2), (void **)&m_WrappedVideo.m_pReal2);
  }

  // useful for marking regions during replay for self-captures
  m_RealAnnotations = NULL;
  if(context)
    context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void **)&m_RealAnnotations);

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
      m_pInfoQueue->SetMuteDebugOutput(false);
  }
  else if(m_pDevice)
  {
    RDCDEBUG("Couldn't get ID3D11InfoQueue.");
  }

  m_Replay.SetDevice(this);

  m_InitParams = params;

  if(realDevice)
    m_DebugManager = new D3D11DebugManager(this);

  // ATI workaround - these dlls can get unloaded and cause a crash.

  if(GetModuleHandleA("aticfx32.dll"))
    LoadLibraryA("aticfx32.dll");
  if(GetModuleHandleA("atiuxpag.dll"))
    LoadLibraryA("atiuxpag.dll");
  if(GetModuleHandleA("atidxx32.dll"))
    LoadLibraryA("atidxx32.dll");

  if(GetModuleHandleA("aticfx64.dll"))
    LoadLibraryA("aticfx64.dll");
  if(GetModuleHandleA("atiuxp64.dll"))
    LoadLibraryA("atiuxp64.dll");
  if(GetModuleHandleA("atidxx64.dll"))
    LoadLibraryA("atidxx64.dll");

  // NVIDIA workaround - same as above!

  if(GetModuleHandleA("nvwgf2umx.dll"))
    LoadLibraryA("nvwgf2umx.dll");
}

WrappedID3D11Device::~WrappedID3D11Device()
{
  if(m_pCurrentWrappedDevice == this)
    m_pCurrentWrappedDevice = NULL;

  D3D11MarkerRegion::device = NULL;

  RenderDoc::Inst().RemoveDeviceFrameCapturer((ID3D11Device *)this);

  for(auto it = m_CachedStateObjects.begin(); it != m_CachedStateObjects.end(); ++it)
    if(*it)
      (*it)->Release();

  m_CachedStateObjects.clear();

  GetResourceManager()->ClearReferencedResources();

  SAFE_RELEASE(m_pDevice1);
  SAFE_RELEASE(m_pDevice2);
  SAFE_RELEASE(m_pDevice3);
  SAFE_RELEASE(m_pDevice4);
  SAFE_RELEASE(m_pDevice5);

  SAFE_RELEASE(m_RealAnnotations);

  SAFE_RELEASE(m_pImmediateContext);

  for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    SAFE_RELEASE(it->second);

  m_Replay.DestroyResources();

  SAFE_DELETE(m_DebugManager);
  SAFE_DELETE(m_TextRenderer);
  SAFE_DELETE(m_ShaderCache);

  if(m_DeviceRecord)
  {
    RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
    m_DeviceRecord->Delete(GetResourceManager());
  }

  for(auto it = m_LayoutShaders.begin(); it != m_LayoutShaders.end(); ++it)
    SAFE_DELETE(it->second);
  m_LayoutShaders.clear();
  m_LayoutDescs.clear();

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  SAFE_RELEASE(m_pInfoQueue);
  SAFE_RELEASE(m_WrappedMultithread.m_pReal);
  SAFE_RELEASE(m_WrappedVideo.m_pReal);
  SAFE_RELEASE(m_WrappedVideo.m_pReal1);
  SAFE_RELEASE(m_WrappedVideo.m_pReal2);
  SAFE_RELEASE(m_WrappedDebug.m_pDebug);
  SAFE_RELEASE(m_pDevice);

  if(!IsStructuredExporting(m_State))
  {
    RDCASSERT(WrappedID3D11Buffer::m_BufferList.empty());
    RDCASSERT(WrappedID3D11Texture1D::m_TextureList.empty());
    RDCASSERT(WrappedID3D11Texture2D1::m_TextureList.empty());
    RDCASSERT(WrappedID3D11Texture3D1::m_TextureList.empty());
  }

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void WrappedID3D11Device::CheckForDeath()
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

ULONG STDMETHODCALLTYPE DummyID3D11InfoQueue::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D11InfoQueue::Release()
{
  m_pDevice->Release();
  return 1;
}

HRESULT STDMETHODCALLTYPE DummyID3D11Debug::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pDevice->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE DummyID3D11Debug::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D11Debug::Release()
{
  m_pDevice->Release();
  return 1;
}

HRESULT STDMETHODCALLTYPE WrappedD3D11Multithread::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)this;
    AddRef();
    return S_OK;
  }
  if(riid == __uuidof(ID3D11Multithread))
  {
    *ppvObject = (ID3D11Multithread *)this;
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedD3D11Multithread::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedD3D11Multithread::Release()
{
  m_pDevice->Release();
  return 1;
}

void STDMETHODCALLTYPE WrappedD3D11Multithread::Enter()
{
  m_pDevice->D3DLock().Lock();
  if(m_pReal)
    m_pReal->Enter();
}

void STDMETHODCALLTYPE WrappedD3D11Multithread::Leave()
{
  if(m_pReal)
    m_pReal->Leave();
  m_pDevice->D3DLock().Unlock();
}

BOOL STDMETHODCALLTYPE WrappedD3D11Multithread::SetMultithreadProtected(BOOL bMTProtect)
{
  bool old = m_pDevice->D3DThreadSafe();
  m_pDevice->SetD3DThreadSafe(bMTProtect == TRUE);
  if(m_pReal)
    m_pReal->SetMultithreadProtected(bMTProtect);
  // TODO - unclear if m_MultithreadProtected just enables Enter/Leave to work, or if it enables
  // auto-thread safety on all D3D interfaces
  return old ? TRUE : FALSE;
}

BOOL STDMETHODCALLTYPE WrappedD3D11Multithread::GetMultithreadProtected()
{
  return m_pDevice->D3DThreadSafe() ? TRUE : FALSE;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11Debug::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(ID3D11InfoQueue) || riid == __uuidof(ID3D11Debug) ||
     riid == __uuidof(ID3D11Device) || riid == __uuidof(ID3D11Device1) ||
     riid == __uuidof(ID3D11Device2) || riid == __uuidof(ID3D11Device3) ||
     riid == __uuidof(ID3D11Device4))
    return m_pDevice->QueryInterface(riid, ppvObject);

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D11Debug *)this;
    AddRef();
    return S_OK;
  }

  WarnUnknownGUID("ID3D11Debug", riid);

  return m_pDebug->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedID3D11Debug::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D11Debug::Release()
{
  m_pDevice->Release();
  return 1;
}

HRESULT WrappedID3D11Device::QueryInterface(REFIID riid, void **ppvObject)
{
  // DEFINE_GUID(IID_IDirect3DDevice9, 0xd0223b96, 0xbf7a, 0x43fd, 0x92, 0xbd, 0xa4, 0x3b, 0xd,
  // 0x82, 0xb9, 0xeb);
  static const GUID IDirect3DDevice9_uuid = {
      0xd0223b96, 0xbf7a, 0x43fd, {0x92, 0xbd, 0xa4, 0x3b, 0xd, 0x82, 0xb9, 0xeb}};

  // ID3D10Device UUID {9B7E4C0F-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10Device_uuid = {
      0x9b7e4c0f, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  // ID3D12Device UUID {189819f1-1db6-4b57-be54-1821339b85f7}
  static const GUID ID3D12Device_uuid = {
      0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};

  // ID3D11ShaderTraceFactory UUID {1fbad429-66ab-41cc-9617-667ac10e4459}
  static const GUID ID3D11ShaderTraceFactory_uuid = {
      0x1fbad429, 0x66ab, 0x41cc, {0x96, 0x17, 0x66, 0x7a, 0xc1, 0x0e, 0x44, 0x59}};

  // RenderDoc UUID {A7AA6116-9C8D-4BBA-9083-B4D816B71B78}
  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

  // UUID for returning unwrapped ID3D11InfoQueue {3FC4E618-3F70-452A-8B8F-A73ACCB58E3D}
  static const GUID unwrappedID3D11InfoQueue__uuid = {
      0x3fc4e618, 0x3f70, 0x452a, {0x8b, 0x8f, 0xa7, 0x3a, 0xcc, 0xb5, 0x8e, 0x3d}};

  HRESULT hr = S_OK;

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D11Device4 *)this;
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
  else if(riid == __uuidof(ID3D11Device))
  {
    AddRef();
    *ppvObject = (ID3D11Device *)this;
    return S_OK;
  }
  else if(riid == ID3D10Device_uuid)
  {
    RDCWARN("Trying to get ID3D10Device - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == ID3D12Device_uuid)
  {
    RDCWARN("Trying to get ID3D12Device - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == IDirect3DDevice9_uuid)
  {
    RDCWARN("Trying to get IDirect3DDevice9 - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == __uuidof(ID3D11Device1))
  {
    if(m_pDevice1)
    {
      AddRef();
      *ppvObject = (ID3D11Device1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Device2))
  {
    if(m_pDevice2)
    {
      AddRef();
      *ppvObject = (ID3D11Device2 *)this;
      RDCWARN(
          "Trying to get ID3D11Device2. DX11.2 tiled resources are not supported at this time.");
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Device3))
  {
    if(m_pDevice3)
    {
      AddRef();
      *ppvObject = (ID3D11Device3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Device4))
  {
    if(m_pDevice4)
    {
      AddRef();
      *ppvObject = (ID3D11Device4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Device5))
  {
    if(m_pDevice5)
    {
      AddRef();
      *ppvObject = (ID3D11Device5 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Multithread))
  {
    AddRef();
    *ppvObject = (ID3D11Multithread *)&m_WrappedMultithread;
    return S_OK;
  }
  else if(riid == ID3D11ShaderTraceFactory_uuid)
  {
    RDCWARN("Trying to get ID3D11ShaderTraceFactory. Not supported at this time.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == __uuidof(ID3D11InfoQueue))
  {
    RDCWARN(
        "Returning a dummy ID3D11InfoQueue that does nothing. RenderDoc takes control of the debug "
        "layer.");
    RDCWARN(
        "If you want direct access, enable API validation and query for %s. This will return the "
        "real ID3D11InfoQueue - be careful as it is unwrapped so you should not call "
        "QueryInterface on it.",
        ToStr(unwrappedID3D11InfoQueue__uuid).c_str());
    *ppvObject = (ID3D11InfoQueue *)&m_DummyInfoQueue;
    m_DummyInfoQueue.AddRef();
    return S_OK;
  }
  else if(riid == unwrappedID3D11InfoQueue__uuid)
  {
    if(m_pInfoQueue)
    {
      *ppvObject = m_pInfoQueue;
      m_pInfoQueue->AddRef();
      return S_OK;
    }
    else
    {
      if(!RenderDoc::Inst().GetCaptureOptions().apiValidation)
      {
        RDCWARN("API Validation is not enabled, RenderDoc disabled the debug layer.");
        RDCWARN(
            "Enable this either in the capture options, or using the RenderDoc API before device "
            "creation.");
      }
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Debug))
  {
    // we queryinterface for this at startup, so if it's present we can
    // return our wrapper
    if(m_WrappedDebug.m_pDebug)
    {
      AddRef();
      *ppvObject = (ID3D11Debug *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      RDCWARN("Returning a dummy ID3D11Debug that does nothing. This ID3D11Debug will not work!");
      *ppvObject = (ID3D11Debug *)&m_DummyDebug;
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
  else if(riid == __uuidof(ID3D11VideoDevice) || riid == __uuidof(ID3D11VideoDevice1) ||
          riid == __uuidof(ID3D11VideoDevice2))
  {
    return m_WrappedVideo.QueryInterface(riid, ppvObject);
  }

  WarnUnknownGUID("ID3D11Device", riid);

  return m_RefCounter.QueryInterface(riid, ppvObject);
}

std::string WrappedID3D11Device::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D11Chunk)idx);
}

void WrappedID3D11Device::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                          std::string d)
{
  // Only add runtime warnings while executing.
  // While reading, add the messages from the log, and while writing add messages
  // we add (on top of the API debug messages)
  if(!IsActiveReplaying(m_State) || src == MessageSource::RuntimeWarning)
  {
    DebugMessage msg;
    msg.eventId = IsCaptureMode(m_State) ? 0 : m_pImmediateContext->GetEventID();
    msg.messageID = 0;
    msg.source = src;
    msg.category = c;
    msg.severity = sv;
    msg.description = d;

    m_DebugMessages.push_back(msg);
  }
}

void WrappedID3D11Device::AddDebugMessage(DebugMessage msg)
{
  if(!IsActiveReplaying(m_State) || msg.source == MessageSource::RuntimeWarning)
    m_DebugMessages.push_back(msg);
}

std::vector<DebugMessage> WrappedID3D11Device::GetDebugMessages()
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
    D3D11_MESSAGE *message = (D3D11_MESSAGE *)msgbuf;

    m_pInfoQueue->GetMessage(i, message, &len);

    DebugMessage msg;
    msg.eventId = 0;
    msg.source = MessageSource::API;
    msg.category = MessageCategory::Miscellaneous;
    msg.severity = MessageSeverity::Medium;

    switch(message->Category)
    {
      case D3D11_MESSAGE_CATEGORY_APPLICATION_DEFINED:
        msg.category = MessageCategory::Application_Defined;
        break;
      case D3D11_MESSAGE_CATEGORY_MISCELLANEOUS:
        msg.category = MessageCategory::Miscellaneous;
        break;
      case D3D11_MESSAGE_CATEGORY_INITIALIZATION:
        msg.category = MessageCategory::Initialization;
        break;
      case D3D11_MESSAGE_CATEGORY_CLEANUP: msg.category = MessageCategory::Cleanup; break;
      case D3D11_MESSAGE_CATEGORY_COMPILATION: msg.category = MessageCategory::Compilation; break;
      case D3D11_MESSAGE_CATEGORY_STATE_CREATION:
        msg.category = MessageCategory::State_Creation;
        break;
      case D3D11_MESSAGE_CATEGORY_STATE_SETTING:
        msg.category = MessageCategory::State_Setting;
        break;
      case D3D11_MESSAGE_CATEGORY_STATE_GETTING:
        msg.category = MessageCategory::State_Getting;
        break;
      case D3D11_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
        msg.category = MessageCategory::Resource_Manipulation;
        break;
      case D3D11_MESSAGE_CATEGORY_EXECUTION: msg.category = MessageCategory::Execution; break;
      case D3D11_MESSAGE_CATEGORY_SHADER: msg.category = MessageCategory::Shaders; break;
      default: RDCWARN("Unexpected message category: %d", message->Category); break;
    }

    switch(message->Severity)
    {
      case D3D11_MESSAGE_SEVERITY_CORRUPTION: msg.severity = MessageSeverity::High; break;
      case D3D11_MESSAGE_SEVERITY_ERROR: msg.severity = MessageSeverity::Medium; break;
      case D3D11_MESSAGE_SEVERITY_WARNING: msg.severity = MessageSeverity::Low; break;
      case D3D11_MESSAGE_SEVERITY_INFO: msg.severity = MessageSeverity::Info; break;
      case D3D11_MESSAGE_SEVERITY_MESSAGE: msg.severity = MessageSeverity::Info; break;
      default: RDCWARN("Unexpected message severity: %d", message->Severity); break;
    }

    msg.messageID = (uint32_t)message->ID;
    msg.description = std::string(message->pDescription);

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

bool WrappedID3D11Device::ProcessChunk(ReadSerialiser &ser, D3D11Chunk context)
{
  switch(context)
  {
    case D3D11Chunk::DeviceInitialisation:
    {
      SERIALISE_ELEMENT_LOCAL(ImmediateContext, ResourceId()).TypedAs("ID3D11DeviceContext *"_lit);

      SERIALISE_CHECK_READ_ERRORS();

      // add a reference for the resource manager - normally it takes ownership of the resource on
      // creation and releases it
      // to destruction, but we want to control our immediate context ourselves.
      if(IsReplayingAndReading())
      {
        m_pImmediateContext->AddRef();
        m_ResourceManager->AddLiveResource(ImmediateContext, m_pImmediateContext);

        AddResource(ImmediateContext, ResourceType::Queue, "");
        ResourceDescription &desc = GetReplay()->GetResourceDesc(ImmediateContext);
        desc.autogeneratedName = false;
        desc.name = "Immediate Context";
        desc.initialisationChunks.clear();
      }

      return true;
    }
    case D3D11Chunk::SetResourceName: return Serialise_SetResourceName(ser, 0x0, "");
    case D3D11Chunk::CreateSwapBuffer: return Serialise_WrapSwapchainBuffer(ser, 0x0, 0x0, 0, 0x0);

    case D3D11Chunk::CreateTexture1D: return Serialise_CreateTexture1D(ser, 0x0, 0x0, 0x0);
    case D3D11Chunk::CreateTexture2D: return Serialise_CreateTexture2D(ser, 0x0, 0x0, 0x0);
    case D3D11Chunk::CreateTexture2D1: return Serialise_CreateTexture2D1(ser, 0x0, 0x0, 0x0);
    case D3D11Chunk::CreateTexture3D: return Serialise_CreateTexture3D(ser, 0x0, 0x0, 0x0);
    case D3D11Chunk::CreateTexture3D1: return Serialise_CreateTexture3D1(ser, 0x0, 0x0, 0x0);
    case D3D11Chunk::CreateBuffer: return Serialise_CreateBuffer(ser, 0x0, 0x0, 0x0);
    case D3D11Chunk::CreateVertexShader: return Serialise_CreateVertexShader(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateHullShader: return Serialise_CreateHullShader(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateDomainShader: return Serialise_CreateDomainShader(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateGeometryShader:
      return Serialise_CreateGeometryShader(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateGeometryShaderWithStreamOutput:
      return Serialise_CreateGeometryShaderWithStreamOutput(ser, 0x0, 0, 0x0, 0, 0x0, 0, 0, 0x0, 0x0);

    case D3D11Chunk::CreatePixelShader: return Serialise_CreatePixelShader(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateComputeShader:
      return Serialise_CreateComputeShader(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::GetClassInstance: return Serialise_GetClassInstance(ser, 0x0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateClassInstance:
      return Serialise_CreateClassInstance(ser, 0x0, 0, 0, 0, 0, 0x0, 0x0);

    case D3D11Chunk::CreateClassLinkage: return Serialise_CreateClassLinkage(ser, 0x0);
    case D3D11Chunk::CreateShaderResourceView:
      return Serialise_CreateShaderResourceView(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateShaderResourceView1:
      return Serialise_CreateShaderResourceView1(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateRenderTargetView:
      return Serialise_CreateRenderTargetView(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateRenderTargetView1:
      return Serialise_CreateRenderTargetView1(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateDepthStencilView:
      return Serialise_CreateDepthStencilView(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateUnorderedAccessView:
      return Serialise_CreateUnorderedAccessView(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateUnorderedAccessView1:
      return Serialise_CreateUnorderedAccessView1(ser, 0x0, 0x0, 0x0);

    case D3D11Chunk::CreateInputLayout:
      return Serialise_CreateInputLayout(ser, 0x0, 0, 0x0, 0, 0x0);

    case D3D11Chunk::CreateBlendState: return Serialise_CreateBlendState(ser, 0x0, 0x0);
    case D3D11Chunk::CreateBlendState1: return Serialise_CreateBlendState1(ser, 0x0, 0x0);
    case D3D11Chunk::CreateDepthStencilState:
      return Serialise_CreateDepthStencilState(ser, 0x0, 0x0);

    case D3D11Chunk::CreateRasterizerState: return Serialise_CreateRasterizerState(ser, 0x0, 0x0);

    case D3D11Chunk::CreateRasterizerState1: return Serialise_CreateRasterizerState1(ser, 0x0, 0x0);

    case D3D11Chunk::CreateRasterizerState2: return Serialise_CreateRasterizerState2(ser, 0x0, 0x0);

    case D3D11Chunk::CreateSamplerState: return Serialise_CreateSamplerState(ser, 0x0, 0x0);
    case D3D11Chunk::CreateQuery: return Serialise_CreateQuery(ser, 0x0, 0x0);
    case D3D11Chunk::CreateQuery1: return Serialise_CreateQuery1(ser, 0x0, 0x0);
    case D3D11Chunk::CreatePredicate: return Serialise_CreatePredicate(ser, 0x0, 0x0);
    case D3D11Chunk::CreateCounter: return Serialise_CreateCounter(ser, 0x0, 0x0);
    case D3D11Chunk::CreateDeferredContext: return Serialise_CreateDeferredContext(ser, 0, 0x0);

    case D3D11Chunk::SetExceptionMode: return Serialise_SetExceptionMode(ser, 0);
    case D3D11Chunk::OpenSharedResource:
    {
      IID nul;
      return Serialise_OpenSharedResource(ser, 0, nul, NULL);
    }
    case D3D11Chunk::SetShaderDebugPath: return Serialise_SetShaderDebugPath(ser, NULL, NULL);
    default:
    {
      SystemChunk system = (SystemChunk)context;
      if(system == SystemChunk::DriverInit)
      {
        D3D11InitParams InitParams;
        SERIALISE_ELEMENT(InitParams);

        SERIALISE_CHECK_READ_ERRORS();
      }
      else if(system == SystemChunk::InitialContentsList)
      {
        GetResourceManager()->CreateInitialContents(ser);

        SERIALISE_CHECK_READ_ERRORS();
      }
      else if(system == SystemChunk::InitialContents)
      {
        return Serialise_InitialState(ser, ResourceId(), NULL, NULL);
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
        return m_pImmediateContext->ProcessChunk(ser, context);
      }

      break;
    }
  }

  return true;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_CaptureScope(SerialiserType &ser)
{
  SERIALISE_ELEMENT(m_FrameCounter);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayMode(m_State))
  {
    m_FrameRecord.frameInfo.frameNumber = m_FrameCounter;

    FrameStatistics &stats = m_FrameRecord.frameInfo.stats;
    RDCEraseEl(stats);

    // #mivance GL/Vulkan don't set this so don't get stats in window
    stats.recorded = true;

    for(uint32_t stage = uint32_t(ShaderStage::First); stage < uint32_t(ShaderStage::Count); stage++)
    {
      stats.constants[stage].bindslots.resize(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT + 1);
      stats.constants[stage].sizes.resize(ConstantBindStats::BucketCount);

      stats.samplers[stage].bindslots.resize(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT + 1);

      stats.resources[stage].types.resize(uint32_t(TextureType::Count));
      stats.resources[stage].bindslots.resize(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT + 1);
    }

    stats.updates.types.resize(uint32_t(TextureType::Count));
    stats.updates.sizes.resize(ResourceUpdateStats::BucketCount);

    stats.draws.counts.resize(DrawcallStats::BucketCount);

    stats.vertices.bindslots.resize(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT + 1);

    stats.rasters.viewports.resize(D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 2);
    stats.rasters.rects.resize(D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 2);

    stats.outputs.bindslots.resize(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + D3D11_1_UAV_SLOT_COUNT +
                                   1);
  }

  return true;
}

ReplayStatus WrappedID3D11Device::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return ReplayStatus::FileCorrupted;

  StreamReader *reader = rdc->ReadSection(sectionIdx);

  if(reader->IsErrored())
  {
    delete reader;
    return ReplayStatus::FileIOFailed;
  }

  ReadSerialiser ser(reader, Ownership::Stream);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  ser.ConfigureStructuredExport(&GetChunkName, storeStructuredBuffers);

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

  std::map<D3D11Chunk, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  uint64_t frameDataSize = 0;

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offsetStart = reader->GetOffset();

    D3D11Chunk context = ser.ReadChunk<D3D11Chunk>();

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
      m_FrameRecord.frameInfo.fileOffset = offsetStart;

      // read the remaining data into memory and pass to immediate context
      frameDataSize = reader->GetSize() - reader->GetOffset();

      m_pImmediateContext->SetFrameReader(new StreamReader(reader, frameDataSize));

      if(!IsStructuredExporting(m_State))
        GetResourceManager()->ApplyInitialContents();

      ReplayStatus status = m_pImmediateContext->ReplayLog(m_State, 0, 0, false);

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
    SetupDrawcallPointers(m_Drawcalls, GetFrameRecord().drawcallList);
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
      chunkInfos[(D3D11Chunk)SystemChunk::InitialContents].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_FrameRecord.frameInfo.persistentSize);

  return ReplayStatus::Succeeded;
}

void WrappedID3D11Device::ReplayLog(uint32_t startEventID, uint32_t endEventID,
                                    ReplayLogType replayType)
{
  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;
  }

  if(!partial)
  {
    D3D11MarkerRegion apply("!!!!RenderDoc Internal: ApplyInitialContents");
    GetResourceManager()->ApplyInitialContents();
  }

  m_State = CaptureState::ActiveReplaying;

  D3D11MarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal: Replay %d (%d): %u->%u",
                                           (int)replayType, (int)partial, startEventID, endEventID));

  m_ReplayEventCount = 0;

  ReplayStatus status = ReplayStatus::Succeeded;

  if(replayType == eReplay_Full)
    status = m_pImmediateContext->ReplayLog(m_State, startEventID, endEventID, partial);
  else if(replayType == eReplay_WithoutDraw)
    status =
        m_pImmediateContext->ReplayLog(m_State, startEventID, RDCMAX(1U, endEventID) - 1, partial);
  else if(replayType == eReplay_OnlyDraw)
    status = m_pImmediateContext->ReplayLog(m_State, endEventID, endEventID, partial);
  else
    RDCFATAL("Unexpected replay type");

  RDCASSERTEQUAL(status, ReplayStatus::Succeeded);

  // make sure to end any unbalanced replay events if we stopped in the middle of a frame
  for(int i = 0; i < m_ReplayEventCount; i++)
    D3D11MarkerRegion::End();

  D3D11MarkerRegion::Set("!!!!RenderDoc Internal: Done replay");
}

void WrappedID3D11Device::ReleaseSwapchainResources(WrappedIDXGISwapChain4 *swap, UINT QueueCount,
                                                    IUnknown *const *ppPresentQueue,
                                                    IUnknown **unwrappedQueues)
{
  RDCASSERT(ppPresentQueue == NULL);

  if(ppPresentQueue)
  {
    RDCERR("D3D11 doesn't support present queues - passing through unmodified");
    for(UINT i = 0; i < QueueCount; i++)
      unwrappedQueues[i] = ppPresentQueue[i];
  }

  for(int i = 0; i < swap->GetNumBackbuffers(); i++)
  {
    WrappedID3D11Texture2D1 *wrapped11 = (WrappedID3D11Texture2D1 *)swap->GetBackbuffers()[i];
    if(wrapped11)
    {
      ResourceRange range(wrapped11);

      GetImmediateContext()->GetCurrentPipelineState()->UnbindRangeForWrite(range);
      GetImmediateContext()->GetCurrentPipelineState()->UnbindRangeForRead(range);

      {
        SCOPED_LOCK(WrappedID3DDeviceContextState::m_Lock);
        for(size_t s = 0; s < WrappedID3DDeviceContextState::m_List.size(); s++)
        {
          WrappedID3DDeviceContextState::m_List[s]->state->UnbindRangeForWrite(range);
          WrappedID3DDeviceContextState::m_List[s]->state->UnbindRangeForRead(range);
        }
      }

      wrapped11->ViewRelease();
    }

    wrapped11 = NULL;
  }

  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC desc = swap->GetDescWithHWND();

    Keyboard::RemoveInputWindow(desc.OutputWindow);

    RenderDoc::Inst().RemoveFrameCapturer((ID3D11Device *)this, desc.OutputWindow);
  }

  auto it = m_SwapChains.find(swap);
  if(it != m_SwapChains.end())
  {
    SAFE_RELEASE(it->second);
    m_SwapChains.erase(it);
  }
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_WrapSwapchainBuffer(SerialiserType &ser,
                                                        WrappedIDXGISwapChain4 *swap,
                                                        DXGI_SWAP_CHAIN_DESC *swapDesc, UINT Buffer,
                                                        IUnknown *realSurface)
{
  WrappedID3D11Texture2D1 *pTex = (WrappedID3D11Texture2D1 *)realSurface;

  SERIALISE_ELEMENT(Buffer);
  SERIALISE_ELEMENT_LOCAL(SwapbufferID, pTex->GetResourceID()).TypedAs("IDXGISwapChain *"_lit);

  m_BBID = SwapbufferID;

  D3D11_TEXTURE2D_DESC BackbufferDescriptor;

  if(ser.IsWriting() && pTex)
    pTex->GetDesc(&BackbufferDescriptor);

  SERIALISE_ELEMENT(BackbufferDescriptor);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D11Texture2D *fakeBB;

    D3D11_TEXTURE2D_DESC realDescriptor = BackbufferDescriptor;

    // DXGI swap chain back buffers can be freely cast as a special-case.
    // translate the format to a typeless format to allow for this.
    // the original type will be stored in the texture below
    BackbufferDescriptor.Format = GetTypelessFormat(BackbufferDescriptor.Format);

    HRESULT hr = m_pDevice->CreateTexture2D(&BackbufferDescriptor, NULL, &fakeBB);

    AddResource(SwapbufferID, ResourceType::SwapchainImage, "Swapchain Image");

    if(FAILED(hr))
    {
      RDCERR("Failed to create fake back buffer, HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      WrappedID3D11Texture2D1 *wrapped =
          new WrappedID3D11Texture2D1(fakeBB, this, TEXDISPLAY_INDIRECT_VIEW);
      fakeBB = wrapped;

      wrapped->m_RealDescriptor = new D3D11_TEXTURE2D_DESC(realDescriptor);

      SetDebugName(fakeBB, "Serialised Swap Chain Buffer");

      GetResourceManager()->AddLiveResource(SwapbufferID, fakeBB);
    }
  }

  return true;
}

IUnknown *WrappedID3D11Device::WrapSwapchainBuffer(WrappedIDXGISwapChain4 *swap,
                                                   DXGI_SWAP_CHAIN_DESC *swapDesc, UINT buffer,
                                                   IUnknown *realSurface)
{
  if(GetResourceManager()->HasWrapper((ID3D11DeviceChild *)realSurface))
  {
    ID3D11Texture2D *tex =
        (ID3D11Texture2D *)GetResourceManager()->GetWrapper((ID3D11DeviceChild *)realSurface);
    tex->AddRef();

    realSurface->Release();

    return tex;
  }

  WrappedID3D11Texture2D1 *pTex =
      new WrappedID3D11Texture2D1((ID3D11Texture2D *)realSurface, this, TEXDISPLAY_UNKNOWN);

  SetDebugName(pTex, "Swap Chain Backbuffer");

  D3D11_TEXTURE2D_DESC desc;
  pTex->GetDesc(&desc);

  ResourceId id = pTex->GetResourceID();

  // init the text renderer
  if(m_TextRenderer == NULL)
    m_TextRenderer = new D3D11TextRenderer(this);

  // there shouldn't be a resource record for this texture as it wasn't created via
  // CreateTexture2D
  RDCASSERT(id != ResourceId() && !GetResourceManager()->HasResourceRecord(id));

  if(IsCaptureMode(m_State))
  {
    D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    record->ResType = Resource_Texture2D;
    record->DataInSerialiser = false;
    record->Length = 0;
    record->NumSubResources = 0;
    record->SubResources = NULL;

    SCOPED_LOCK(m_D3DLock);

    WriteSerialiser &ser = m_ScratchSerialiser;

    SCOPED_SERIALISE_CHUNK(D3D11Chunk::CreateSwapBuffer);

    Serialise_WrapSwapchainBuffer(ser, swap, swapDesc, buffer, pTex);

    record->AddChunk(scope.Get());
  }

  if(buffer == 0 && IsCaptureMode(m_State))
  {
    ID3D11RenderTargetView *rtv = NULL;
    HRESULT hr = m_pDevice->CreateRenderTargetView(UNWRAP(WrappedID3D11Texture2D1, pTex), NULL, &rtv);

    if(FAILED(hr))
      RDCERR("Couldn't create RTV for swapchain tex HRESULT: %s", ToStr(hr).c_str());

    m_SwapChains[swap] = rtv;
  }

  if(swap)
  {
    DXGI_SWAP_CHAIN_DESC sdesc = swap->GetDescWithHWND();

    Keyboard::AddInputWindow(sdesc.OutputWindow);

    RenderDoc::Inst().AddFrameCapturer((ID3D11Device *)this, sdesc.OutputWindow, this);
  }

  return pTex;
}

void WrappedID3D11Device::SetMarker(uint32_t col, const wchar_t *name)
{
  if(m_pCurrentWrappedDevice == NULL)
    return;

  m_pCurrentWrappedDevice->m_pImmediateContext->ThreadSafe_SetMarker(col, name);
}

int WrappedID3D11Device::BeginEvent(uint32_t col, const wchar_t *name)
{
  if(m_pCurrentWrappedDevice == NULL)
    return 0;

  return m_pCurrentWrappedDevice->m_pImmediateContext->ThreadSafe_BeginEvent(col, name);
}

int WrappedID3D11Device::EndEvent()
{
  if(m_pCurrentWrappedDevice == NULL)
    return 0;

  return m_pCurrentWrappedDevice->m_pImmediateContext->ThreadSafe_EndEvent();
}

void WrappedID3D11Device::StartFrameCapture(void *dev, void *wnd)
{
  SCOPED_LOCK(m_D3DLock);

  if(!IsBackgroundCapturing(m_State))
    return;

  m_State = CaptureState::ActiveCapturing;

  m_AppControlledCapture = true;

  m_Failures = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;

  m_FrameCounter = RDCMAX((uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FrameDescription frame;
  frame.frameNumber = m_FrameCounter;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  m_DebugMessages.clear();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_PartialWrite);

  m_pImmediateContext->FreeCaptureData();

  m_pImmediateContext->AttemptCapture();
  m_pImmediateContext->BeginCaptureFrame();

  for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
  {
    WrappedID3D11DeviceContext *context = *it;

    if(context)
    {
      context->AttemptCapture();
    }
    else
    {
      RDCERR("NULL deferred context in resource record!");
    }
  }

  GetResourceManager()->PrepareInitialContents();

  if(m_pInfoQueue)
    m_pInfoQueue->ClearStoredMessages();

  RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedID3D11Device::EndFrameCapture(void *dev, void *wnd)
{
  SCOPED_LOCK(m_D3DLock);

  if(!IsActiveCapturing(m_State))
    return true;

  CaptureFailReason reason;

  WrappedIDXGISwapChain4 *swap = NULL;

  if(wnd)
  {
    for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    {
      DXGI_SWAP_CHAIN_DESC swapDesc = it->first->GetDescWithHWND();

      if(swapDesc.OutputWindow == wnd)
      {
        swap = it->first;
        break;
      }
    }

    if(swap == NULL)
    {
      RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
      return false;
    }
  }

  if(m_pImmediateContext->HasSuccessfulCapture(reason))
  {
    RDCLOG("Finished capture, Frame %u", m_FrameCounter);

    m_Failures = 0;
    m_FailedFrame = 0;
    m_FailedReason = CaptureSucceeded;

    m_pImmediateContext->EndCaptureFrame();
    m_pImmediateContext->FinishCapture();

    for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
    {
      WrappedID3D11DeviceContext *context = *it;

      if(context)
      {
        context->FinishCapture();
      }
      else
      {
        RDCERR("NULL deferred context in resource record!");
      }
    }

    const uint32_t maxSize = 2048;
    RenderDoc::FramePixels fp;

    if(swap != NULL)
    {
      ID3D11RenderTargetView *rtv = m_SwapChains[swap];

      ID3D11Resource *res = NULL;

      rtv->GetResource(&res);
      res->Release();

      ID3D11Texture2D *tex = (ID3D11Texture2D *)res;

      D3D11_TEXTURE2D_DESC desc;
      tex->GetDesc(&desc);

      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;
      desc.Usage = D3D11_USAGE_STAGING;

      bool msaa = (desc.SampleDesc.Count > 1) || (desc.SampleDesc.Quality > 0);

      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;

      ID3D11Texture2D *stagingTex = NULL;

      HRESULT hr = S_OK;

      hr = m_pDevice->CreateTexture2D(&desc, NULL, &stagingTex);

      if(FAILED(hr))
      {
        RDCERR("Couldn't create staging texture to create thumbnail. HRESULT: %s", ToStr(hr).c_str());
      }
      else
      {
        if(msaa)
        {
          desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
          desc.CPUAccessFlags = 0;
          desc.Usage = D3D11_USAGE_DEFAULT;

          ID3D11Texture2D *resolveTex = NULL;

          hr = m_pDevice->CreateTexture2D(&desc, NULL, &resolveTex);

          if(FAILED(hr))
          {
            RDCERR("Couldn't create resolve texture to create thumbnail. HRESULT: %s",
                   ToStr(hr).c_str());
            tex = NULL;
          }
          else
          {
            m_pImmediateContext->GetReal()->ResolveSubresource(resolveTex, 0, tex, 0, desc.Format);
            m_pImmediateContext->GetReal()->CopyResource(stagingTex, resolveTex);
            resolveTex->Release();
          }
        }
        else
        {
          m_pImmediateContext->GetReal()->CopyResource(stagingTex, tex);
        }

        if(tex)
        {
          ResourceFormat fmt = MakeResourceFormat(desc.Format);

          D3D11_MAPPED_SUBRESOURCE mapped;
          hr = m_pImmediateContext->GetReal()->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mapped);

          if(FAILED(hr))
          {
            RDCERR("Couldn't map staging texture to create thumbnail. HRESULT: %s",
                   ToStr(hr).c_str());
          }
          else
          {
            fp.len = (uint32_t)mapped.RowPitch * desc.Height;
            fp.data = new uint8_t[fp.len];
            memcpy(fp.data, mapped.pData, fp.len);

            m_pImmediateContext->GetReal()->Unmap(stagingTex, 0);

            fp.width = (uint32_t)desc.Width;
            fp.height = (uint32_t)desc.Height;
            fp.pitch = mapped.RowPitch;
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
          }
        }

        stagingTex->Release();
      }
    }

    RDCFile *rdc =
        RenderDoc::Inst().CreateRDC(RDCDriver::D3D11, m_CapturedFrames.back().frameNumber, fp);

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

      ser.SetChunkMetadataRecording(m_ScratchSerialiser.GetChunkMetadataRecording());

      ser.SetUserData(GetResourceManager());

      {
        // remember to update this estimated chunk length if you add more parameters
        SCOPED_SERIALISE_CHUNK(SystemChunk::DriverInit, sizeof(D3D11InitParams) + 16);

        SERIALISE_ELEMENT(m_InitParams);
      }

      {
        // remember to update this estimated chunk length if you add more parameters
        SCOPED_SERIALISE_CHUNK(D3D11Chunk::DeviceInitialisation, 16);

        SERIALISE_ELEMENT_LOCAL(ImmediateContext, m_pImmediateContext->GetResourceID())
            .TypedAs("ID3D11DeviceContext *"_lit);
      }

      RDCDEBUG("Inserting Resource Serialisers");

      LockForChunkFlushing();

      GetResourceManager()->ApplyInitialContentsNonChunks(ser);

      GetResourceManager()->InsertReferencedChunks(ser);

      GetResourceManager()->InsertInitialContentsChunks(ser);

      RDCDEBUG("Creating Capture Scope");

      GetResourceManager()->Serialise_InitialContentsNeeded(ser);

      {
        // remember to update this estimated chunk length if you add more parameters
        SCOPED_SERIALISE_CHUNK(SystemChunk::CaptureScope, 16);

        Serialise_CaptureScope(ser);
      }

      {
        RDCDEBUG("Getting Resource Record");

        D3D11ResourceRecord *record = m_pImmediateContext->GetResourceRecord();

        RDCDEBUG("Accumulating context resource list");

        std::map<int32_t, Chunk *> recordlist;
        record->Insert(recordlist);

        RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());

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

      UnlockForChunkFlushing();
    }

    RenderDoc::Inst().FinishCaptureWriting(rdc, m_CapturedFrames.back().frameNumber);

    m_State = CaptureState::BackgroundCapturing;

    m_pImmediateContext->CleanupCapture();

    m_pImmediateContext->FreeCaptureData();

    for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
    {
      WrappedID3D11DeviceContext *context = *it;

      if(context)
        context->CleanupCapture();
      else
        RDCERR("NULL deferred context in resource record!");
    }

    GetResourceManager()->MarkUnwrittenResources();

    GetResourceManager()->ClearReferencedResources();

    GetResourceManager()->FreeInitialContents();

    return true;
  }
  else
  {
    const char *reasonString = "Unknown reason";
    switch(reason)
    {
      case CaptureFailed_UncappedCmdlist: reasonString = "Uncapped command list"; break;
      case CaptureFailed_UncappedUnmap: reasonString = "Uncapped Map()/Unmap()"; break;
      default: break;
    }

    RDCLOG("Failed to capture, frame %u: %s", m_FrameCounter, reasonString);

    m_Failures++;

    if((RenderDoc::Inst().GetOverlayBits() & eRENDERDOC_Overlay_Enabled) && swap != NULL)
    {
      D3D11RenderState old = *m_pImmediateContext->GetCurrentPipelineState();

      ID3D11RenderTargetView *rtv = m_SwapChains[swap];

      if(rtv)
      {
        m_pImmediateContext->GetReal()->OMSetRenderTargets(1, &rtv, NULL);

        DXGI_SWAP_CHAIN_DESC swapDesc = swap->GetDescWithHWND();
        m_TextRenderer->SetOutputDimensions(swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height);
        m_TextRenderer->SetOutputWindow(swapDesc.OutputWindow);

        m_TextRenderer->RenderText(0.0f, 0.0f, "Failed to capture frame %u: %s", m_FrameCounter,
                                   reasonString);
      }

      old.ApplyState(m_pImmediateContext);
    }

    m_CapturedFrames.back().frameNumber = m_FrameCounter;

    m_pImmediateContext->CleanupCapture();

    for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
    {
      WrappedID3D11DeviceContext *context = *it;

      if(context)
        context->CleanupCapture();
      else
        RDCERR("NULL deferred context in resource record!");
    }

    GetResourceManager()->ClearReferencedResources();

    GetResourceManager()->FreeInitialContents();

    // if it's a capture triggered from application code, immediately
    // give up as it's not reasonable to expect applications to detect and retry.
    // otherwise we can retry in case the next frame works.
    if(m_Failures > 5 || m_AppControlledCapture)
    {
      m_pImmediateContext->FinishCapture();

      m_CapturedFrames.pop_back();

      for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
      {
        WrappedID3D11DeviceContext *context = *it;

        if(context)
        {
          context->FinishCapture();
        }
        else
        {
          RDCERR("NULL deferred context in resource record!");
        }
      }

      m_pImmediateContext->FreeCaptureData();

      m_FailedFrame = m_FrameCounter;
      m_FailedReason = reason;

      m_State = CaptureState::BackgroundCapturing;

      for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
      {
        WrappedID3D11DeviceContext *context = *it;

        if(context)
          context->CleanupCapture();
        else
          RDCERR("NULL deferred context in resource record!");
      }

      GetResourceManager()->MarkUnwrittenResources();
    }
    else
    {
      GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_PartialWrite);
      GetResourceManager()->PrepareInitialContents();

      m_pImmediateContext->AttemptCapture();
      m_pImmediateContext->BeginCaptureFrame();

      for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
      {
        WrappedID3D11DeviceContext *context = *it;

        if(context)
        {
          context->AttemptCapture();
        }
        else
        {
          RDCERR("NULL deferred context in resource record!");
        }
      }
    }

    if(m_pInfoQueue)
      m_pInfoQueue->ClearStoredMessages();

    return false;
  }
}

bool WrappedID3D11Device::DiscardFrameCapture(void *dev, void *wnd)
{
  SCOPED_LOCK(m_D3DLock);

  if(!IsActiveCapturing(m_State))
    return true;

  RenderDoc::Inst().FinishCaptureWriting(NULL, m_CapturedFrames.back().frameNumber);

  m_pImmediateContext->CleanupCapture();

  for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
  {
    WrappedID3D11DeviceContext *context = *it;

    if(context)
      context->CleanupCapture();
    else
      RDCERR("NULL deferred context in resource record!");
  }

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->FreeInitialContents();

  m_pImmediateContext->FinishCapture();

  m_CapturedFrames.pop_back();

  for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
  {
    WrappedID3D11DeviceContext *context = *it;

    if(context)
    {
      context->FinishCapture();
    }
    else
    {
      RDCERR("NULL deferred context in resource record!");
    }
  }

  m_pImmediateContext->FreeCaptureData();

  m_State = CaptureState::BackgroundCapturing;

  for(auto it = m_DeferredContexts.begin(); it != m_DeferredContexts.end(); ++it)
  {
    WrappedID3D11DeviceContext *context = *it;

    if(context)
      context->CleanupCapture();
    else
      RDCERR("NULL deferred context in resource record!");
  }

  GetResourceManager()->MarkUnwrittenResources();

  if(m_pInfoQueue)
    m_pInfoQueue->ClearStoredMessages();

  return true;
}

void WrappedID3D11Device::LockForChunkFlushing()
{
  // wait for the value to be 0 (no-one messing with chunks right now) and set to -1
  // to indicate that we're writing chunks and so no-one should try messing.
  for(;;)
  {
    int32_t val = Atomic::CmpExch32(&m_ChunkAtomic, 0, -1);

    // val was 0, so we replaced it, so we can stop
    if(val == 0)
      break;

    // we don't support recursive locking, so negative value is invalid
    if(val < 0)
    {
      RDCERR("Something went wrong! m_ChunkAtomic was %d before!", val);

      // try and recover by just setting to -1 anyway and hope for the best
      val = -1;
      break;
    }

    // spin while val is positive
  }
}

void WrappedID3D11Device::UnlockForChunkFlushing()
{
  // set value back to 0
  int32_t val = Atomic::CmpExch32(&m_ChunkAtomic, -1, 0);

  // should only come in here if we successfully grabbed the lock before. We don't
  // support multiple flushing locks.
  if(val != -1)
  {
    RDCERR("Something went wrong! m_ChunkAtomic was %d before, expected -1", val);

    // try and recover by just setting to 0 anyway and hope for the best
    val = 0;
  }
}

void WrappedID3D11Device::LockForChunkRemoval()
{
  // wait for value to be non-negative (indicating that we're not using the chunks)
  // and then increment it. Spin until we have incremented it.
  for(;;)
  {
    int32_t prev = m_ChunkAtomic;

    // spin while val is negative
    if(prev < 0)
      continue;

    // try to increment the value
    int32_t val = Atomic::CmpExch32(&m_ChunkAtomic, prev, prev + 1);

    // val was prev. That means we incremented it so we can stop
    if(val == prev)
      break;
  }
}

void WrappedID3D11Device::UnlockForChunkRemoval()
{
  // spin until we've decremented the value
  for(;;)
  {
    int32_t prev = m_ChunkAtomic;

    // val should always be positive because we locked it. Bail out if not
    if(prev <= 0)
    {
      RDCERR("Something went wrong! m_ChunkAtomic was %d before, expected positive", prev);
      // do nothing, hope it all goes OK
      break;
    }

    // try to decrement the value
    int32_t val = Atomic::CmpExch32(&m_ChunkAtomic, prev, prev - 1);

    // val was prev. That means we decremented it so we can stop
    if(val == prev)
      break;
  }
}

void WrappedID3D11Device::FirstFrame(WrappedIDXGISwapChain4 *swapChain)
{
  DXGI_SWAP_CHAIN_DESC swapdesc = swapChain->GetDescWithHWND();

  // if we have to capture the first frame, begin capturing immediately
  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(0))
  {
    RenderDoc::Inst().StartFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);

    m_AppControlledCapture = false;
  }
}

HRESULT WrappedID3D11Device::Present(WrappedIDXGISwapChain4 *swap, UINT SyncInterval, UINT Flags)
{
  if((Flags & DXGI_PRESENT_TEST) != 0)
    return S_OK;

  m_pCurrentWrappedDevice = this;

  if(IsBackgroundCapturing(m_State))
    RenderDoc::Inst().Tick();

  m_pImmediateContext->EndFrame();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  m_pImmediateContext->BeginFrame();

  DXGI_SWAP_CHAIN_DESC swapdesc = swap->GetDescWithHWND();
  bool activeWindow = RenderDoc::Inst().IsActiveWindow((ID3D11Device *)this, swapdesc.OutputWindow);

  if(IsBackgroundCapturing(m_State))
  {
    D3D11RenderState old = *m_pImmediateContext->GetCurrentPipelineState();

    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      ID3D11RenderTargetView *rtv = m_SwapChains[swap];

      m_pImmediateContext->GetReal()->OMSetRenderTargets(1, &rtv, NULL);

      DXGI_SWAP_CHAIN_DESC swapDesc = {0};
      swap->GetDesc(&swapDesc);
      m_TextRenderer->SetOutputDimensions(swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height);
      m_TextRenderer->SetOutputWindow(swapDesc.OutputWindow);

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      std::string overlayText =
          RenderDoc::Inst().GetOverlayText(RDCDriver::D3D11, m_FrameCounter, flags);

      if(activeWindow && m_FailedFrame > 0)
      {
        const char *reasonString = "Unknown reason";
        switch(m_FailedReason)
        {
          case CaptureFailed_UncappedCmdlist: reasonString = "Uncapped command list"; break;
          case CaptureFailed_UncappedUnmap: reasonString = "Uncapped Map()/Unmap()"; break;
          default: break;
        }

        overlayText += StringFormat::Fmt("Failed capture at frame %d:\n", m_FailedFrame);
        overlayText += StringFormat::Fmt("    %s\n", reasonString);
      }

      if(!overlayText.empty())
        m_TextRenderer->RenderText(0.0f, 0.0f, overlayText.c_str());

      old.ApplyState(m_pImmediateContext);
    }
  }

  RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D11, true);

  if(!activeWindow)
    return S_OK;

  // kill any current capture that isn't application defined
  if(IsActiveCapturing(m_State) && !m_AppControlledCapture)
  {
    m_pImmediateContext->Present(SyncInterval, Flags);

    RenderDoc::Inst().EndFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);
  }

  if(IsBackgroundCapturing(m_State) && RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter))
  {
    RenderDoc::Inst().StartFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);

    m_AppControlledCapture = false;
  }

  return S_OK;
}

void WrappedID3D11Device::CachedObjectsGarbageCollect()
{
  // 4000 is a fairly arbitrary number, chosen to make sure this garbage
  // collection kicks in as rarely as possible (4000 is a *lot* of unique
  // state objects to have), while still meaning that we'll never
  // accidentally cause a state object to fail to create because the app
  // expects only N to be alive but we're caching M more causing M+N>4096
  if(m_CachedStateObjects.size() < 4000)
    return;

  // Now release all purely cached objects that have no external refcounts.
  // This will thrash if we have e.g. 2000 rasterizer state objects, all
  // referenced, and 2000 sampler state objects, all referenced.

  for(auto it = m_CachedStateObjects.begin(); it != m_CachedStateObjects.end();)
  {
    ID3D11DeviceChild *o = *it;

    o->AddRef();
    if(o->Release() == 1)
    {
      auto eraseit = it;
      ++it;
      o->Release();
      InternalRelease();
      m_CachedStateObjects.erase(eraseit);
    }
    else
    {
      ++it;
    }
  }
}

void WrappedID3D11Device::AddDeferredContext(WrappedID3D11DeviceContext *defctx)
{
  RDCASSERT(m_DeferredContexts.find(defctx) == m_DeferredContexts.end());
  m_DeferredContexts.insert(defctx);
}

void WrappedID3D11Device::RemoveDeferredContext(WrappedID3D11DeviceContext *defctx)
{
  RDCASSERT(m_DeferredContexts.find(defctx) != m_DeferredContexts.end());
  m_DeferredContexts.erase(defctx);
}

void WrappedID3D11Device::AddResource(ResourceId id, ResourceType type, const char *defaultNamePrefix)
{
  ResourceDescription &descr = GetReplay()->GetResourceDesc(id);

  uint64_t num;
  memcpy(&num, &id, sizeof(uint64_t));
  descr.name = defaultNamePrefix + (" " + ToStr(num));
  descr.autogeneratedName = true;
  descr.type = type;
  AddResourceCurChunk(descr);
}

void WrappedID3D11Device::AddResourceCurChunk(ResourceDescription &descr)
{
  descr.initialisationChunks.push_back((uint32_t)m_StructuredFile->chunks.size() - 1);
}

void WrappedID3D11Device::AddResourceCurChunk(ResourceId id)
{
  if(GetResourceManager()->HasLiveResource(id))
    AddResourceCurChunk(GetReplay()->GetResourceDesc(id));
}

void WrappedID3D11Device::DerivedResource(ID3D11DeviceChild *parent, ResourceId child)
{
  ResourceId parentId = GetResourceManager()->GetOriginalID(GetIDForResource(parent));

  GetReplay()->GetResourceDesc(parentId).derivedResources.push_back(child);
  GetReplay()->GetResourceDesc(child).parentResources.push_back(parentId);
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_SetShaderDebugPath(SerialiserType &ser,
                                                       ID3D11DeviceChild *pResource, const char *Path)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Path);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ResourceId resId = GetResourceManager()->GetOriginalID(GetIDForResource(pResource));

    AddResourceCurChunk(resId);

    auto it = WrappedShader::m_ShaderList.find(GetIDForResource(pResource));

    if(it != WrappedShader::m_ShaderList.end())
      it->second->SetDebugInfoPath(Path);
  }

  return true;
}

HRESULT WrappedID3D11Device::SetShaderDebugPath(ID3D11DeviceChild *pResource, const char *Path)
{
  if(IsCaptureMode(m_State))
  {
    ResourceId idx = GetIDForResource(pResource);
    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);

    if(record == NULL)
    {
      RDCERR("Setting shader debug path on object %p of type %d that has no resource record.",
             pResource, IdentifyTypeByPtr(pResource));
      return E_INVALIDARG;
    }

    RDCASSERT(idx != ResourceId());

    {
      WriteSerialiser &ser = m_ScratchSerialiser;
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::SetShaderDebugPath);
      Serialise_SetShaderDebugPath(ser, pResource, Path);
      record->AddChunk(scope.Get());
    }

    return S_OK;
  }

  return S_OK;
}

template <typename SerialiserType>
bool WrappedID3D11Device::Serialise_SetResourceName(SerialiserType &ser,
                                                    ID3D11DeviceChild *pResource, const char *Name)
{
  SERIALISE_ELEMENT(pResource);
  SERIALISE_ELEMENT(Name);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && pResource)
  {
    ResourceDescription &descr = GetReplay()->GetResourceDesc(
        GetResourceManager()->GetOriginalID(GetIDForResource(pResource)));
    descr.SetCustomName(Name ? Name : "");
    AddResourceCurChunk(descr);

    SetDebugName(pResource, Name);
  }

  return true;
}

void WrappedID3D11Device::SetResourceName(ID3D11DeviceChild *pResource, const char *Name)
{
  // don't allow naming device contexts or command lists so we know this chunk
  // is always on a pre-capture chunk.
  if(IsCaptureMode(m_State) && !WrappedID3D11DeviceContext::IsAlloc(pResource) &&
     !WrappedID3D11CommandList::IsAlloc(pResource))
  {
    ResourceId idx = GetIDForResource(pResource);
    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);

    if(record == NULL)
      record = m_DeviceRecord;

    RDCASSERT(idx != ResourceId());

    SCOPED_LOCK(m_D3DLock);
    {
      WriteSerialiser &ser = m_ScratchSerialiser;
      SCOPED_SERIALISE_CHUNK(D3D11Chunk::SetResourceName);

      Serialise_SetResourceName(ser, pResource, Name);

      LockForChunkRemoval();

      // don't serialise many SetResourceName chunks to the
      // object record, but we can't afford to drop any.
      record->LockChunks();
      while(record->HasChunks())
      {
        Chunk *end = record->GetLastChunk();

        if(end->GetChunkType<D3D11Chunk>() == D3D11Chunk::SetResourceName)
        {
          SAFE_DELETE(end);
          record->PopChunk();
          continue;
        }

        break;
      }
      record->UnlockChunks();

      UnlockForChunkRemoval();

      record->AddChunk(scope.Get());
    }
  }
}

void WrappedID3D11Device::ReleaseResource(ID3D11DeviceChild *res)
{
  ResourceId idx = GetIDForResource(res);

  // wrapped resources get released all the time, we don't want to
  // try and slerp in a resource release. Just the explicit ones
  if(!IsCaptureMode(m_State))
  {
    if(GetResourceManager()->HasLiveResource(idx))
      GetResourceManager()->EraseLiveResource(idx);
    return;
  }

  SCOPED_LOCK(m_D3DLock);

  if(WrappedID3D11DeviceContext::IsAlloc(res))
    RemoveDeferredContext((WrappedID3D11DeviceContext *)res);

  D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);
  if(record)
    record->Delete(GetResourceManager());
}

WrappedID3D11DeviceContext *WrappedID3D11Device::GetDeferredContext(size_t idx)
{
  auto it = m_DeferredContexts.begin();

  for(size_t i = 0; i < idx; i++)
  {
    ++it;
    if(it == m_DeferredContexts.end())
      return NULL;
  }

  return *it;
}

const DrawcallDescription *WrappedID3D11Device::GetDrawcall(uint32_t eventId)
{
  if(eventId >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventId];
}
