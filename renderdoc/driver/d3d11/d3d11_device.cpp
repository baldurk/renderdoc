/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "driver/d3d11/d3d11_device.h"
#include "core/core.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "strings/string_utils.h"

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

void WrappedID3D11Device::SetLogFile(const char *logfile)
{
#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  m_pSerialiser = new Serialiser(logfile, Serialiser::READING, debugSerialiser);
  m_pSerialiser->SetChunkNameLookup(&GetChunkName);
  m_pImmediateContext->SetSerialiser(m_pSerialiser);

  SAFE_DELETE(m_ResourceManager);
  m_ResourceManager = new D3D11ResourceManager(m_State, m_pSerialiser, this);
}

WrappedID3D11Device::WrappedID3D11Device(ID3D11Device *realDevice, D3D11InitParams *params)
    : m_RefCounter(realDevice, false), m_SoftRefCounter(NULL, false), m_pDevice(realDevice)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D11Device));

  m_SectionVersion = D3D11InitParams::CurrentVersion;

  m_pDevice1 = NULL;
  m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&m_pDevice1);

  m_pDevice2 = NULL;
  m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&m_pDevice2);

  m_pDevice3 = NULL;
  m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&m_pDevice3);

  m_pDevice4 = NULL;
  m_pDevice->QueryInterface(__uuidof(ID3D11Device4), (void **)&m_pDevice4);

  m_Replay.SetDevice(this);

  m_DebugManager = NULL;

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

  m_DummyInfoQueue.m_pDevice = this;
  m_DummyD3D10Multithread.m_pDevice = this;
  m_DummyDebug.m_pDevice = this;
  m_WrappedDebug.m_pDevice = this;

  m_FrameCounter = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;
  m_Failures = 0;

  m_ChunkAtomic = 0;

  m_AppControlledCapture = false;

#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    m_pSerialiser = NULL;

    D3D11MarkerRegion::device = this;

    string shaderSearchPathString = RenderDoc::Inst().GetConfigSetting("shader.debug.searchPaths");
    split(shaderSearchPathString, m_ShaderSearchPaths, ';');

    ResourceIDGen::SetReplayResourceIDs();
  }
  else
  {
    m_State = WRITING_IDLE;
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);

    m_pSerialiser->SetDebugText(true);
  }

  m_ResourceManager = new D3D11ResourceManager(m_State, m_pSerialiser, this);

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

    RenderDoc::Inst().AddDeviceFrameCapturer((ID3D11Device *)this, this);
  }

  ID3D11DeviceContext *context = NULL;
  realDevice->GetImmediateContext(&context);

  m_pImmediateContext = new WrappedID3D11DeviceContext(this, context, m_pSerialiser);

  realDevice->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&m_pInfoQueue);
  realDevice->QueryInterface(__uuidof(ID3D11Debug), (void **)&m_WrappedDebug.m_pDebug);

  // useful for marking regions during replay for self-captures
  m_RealAnnotations = NULL;
  m_pImmediateContext->GetReal()->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                                 (void **)&m_RealAnnotations);

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
    RDCDEBUG("Couldn't get ID3D11InfoQueue.");
  }

  m_InitParams = *params;

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

  SAFE_RELEASE(m_pDevice1);
  SAFE_RELEASE(m_pDevice2);
  SAFE_RELEASE(m_pDevice3);
  SAFE_RELEASE(m_pDevice4);

  SAFE_RELEASE(m_RealAnnotations);

  SAFE_RELEASE(m_pImmediateContext);

  for(auto it = m_SwapChains.begin(); it != m_SwapChains.end(); ++it)
    SAFE_RELEASE(it->second);

  SAFE_DELETE(m_DebugManager);

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
  SAFE_RELEASE(m_WrappedDebug.m_pDebug);
  SAFE_RELEASE(m_pDevice);

  SAFE_DELETE(m_pSerialiser);

  RDCASSERT(WrappedID3D11Buffer::m_BufferList.empty());
  RDCASSERT(WrappedID3D11Texture1D::m_TextureList.empty());
  RDCASSERT(WrappedID3D11Texture2D1::m_TextureList.empty());
  RDCASSERT(WrappedID3D11Texture3D1::m_TextureList.empty());

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

    if(m_SoftRefCounter.GetRefCount() <= m_InternalRefcount || m_State < WRITING)    // MEGA HACK
    {
      m_Alive = false;
      delete this;
    }
  }
}

ULONG STDMETHODCALLTYPE DummyID3D10Multithread::AddRef()
{
  m_pDevice->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D10Multithread::Release()
{
  m_pDevice->Release();
  return 1;
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

  string guid = ToStr(riid);
  RDCWARN("Querying ID3D11Debug for interface: %s", guid.c_str());

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
  else if(riid == __uuidof(ID3D10Multithread))
  {
    RDCWARN(
        "Returning a dummy ID3D10Multithread that does nothing. This ID3D10Multithread will not "
        "work!");
    *ppvObject = (ID3D10Multithread *)&m_DummyD3D10Multithread;
    m_DummyD3D10Multithread.AddRef();
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
      if(!RenderDoc::Inst().GetCaptureOptions().APIValidation)
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
  else
  {
    string guid = ToStr(riid);
    RDCWARN("Querying ID3D11Device for interface: %s", guid.c_str());
  }

  return m_RefCounter.QueryInterface(riid, ppvObject);
}

const char *WrappedID3D11Device::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ::GetChunkName((SystemChunk)idx);

  return ::GetChunkName((D3D11Chunk)idx);
}

void WrappedID3D11Device::LazyInit()
{
  if(m_DebugManager == NULL)
    m_DebugManager = new D3D11DebugManager(this);
}

void WrappedID3D11Device::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                          std::string d)
{
  // Only add runtime warnings while executing.
  // While reading, add the messages from the log, and while writing add messages
  // we add (on top of the API debug messages)
  if(m_State != EXECUTING || src == MessageSource::RuntimeWarning)
  {
    DebugMessage msg;
    msg.eventID = m_State >= WRITING ? 0 : m_pImmediateContext->GetEventID();
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
  if(m_State != EXECUTING || msg.source == MessageSource::RuntimeWarning)
    m_DebugMessages.push_back(msg);
}

vector<DebugMessage> WrappedID3D11Device::GetDebugMessages()
{
  vector<DebugMessage> ret;

  // if reading, m_DebugMessages will contain all the messages (we
  // don't try and fetch anything from the API). If writing,
  // m_DebugMessages will contain any manually-added messages.
  ret.swap(m_DebugMessages);

  if(m_State < WRITING)
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
    msg.eventID = 0;
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

void WrappedID3D11Device::ProcessChunk(uint64_t offset, D3D11ChunkType context)
{
  switch(context)
  {
    case DEVICE_INIT:
    {
      SERIALISE_ELEMENT(ResourceId, immContextId, ResourceId());

      // add a reference for the resource manager - normally it takes ownership of the resource on
      // creation and releases it
      // to destruction, but we want to control our immediate context ourselves.
      m_pImmediateContext->AddRef();
      m_ResourceManager->AddLiveResource(immContextId, m_pImmediateContext);
      break;
    }
    case SET_RESOURCE_NAME: Serialise_SetResourceName(0x0, ""); break;
    case RELEASE_RESOURCE: Serialise_ReleaseResource(0x0); break;
    case CREATE_SWAP_BUFFER: Serialise_WrapSwapchainBuffer(0x0, 0x0, 0, 0x0); break;
    case CREATE_TEXTURE_1D: Serialise_CreateTexture1D(0x0, 0x0, 0x0); break;
    case CREATE_TEXTURE_2D: Serialise_CreateTexture2D(0x0, 0x0, 0x0); break;
    case CREATE_TEXTURE_2D1: Serialise_CreateTexture2D1(0x0, 0x0, 0x0); break;
    case CREATE_TEXTURE_3D: Serialise_CreateTexture3D(0x0, 0x0, 0x0); break;
    case CREATE_TEXTURE_3D1: Serialise_CreateTexture3D1(0x0, 0x0, 0x0); break;
    case CREATE_BUFFER: Serialise_CreateBuffer(0x0, 0x0, 0x0); break;
    case CREATE_VERTEX_SHADER: Serialise_CreateVertexShader(0x0, 0, 0x0, 0x0); break;
    case CREATE_HULL_SHADER: Serialise_CreateHullShader(0x0, 0, 0x0, 0x0); break;
    case CREATE_DOMAIN_SHADER: Serialise_CreateDomainShader(0x0, 0, 0x0, 0x0); break;
    case CREATE_GEOMETRY_SHADER: Serialise_CreateGeometryShader(0x0, 0, 0x0, 0x0); break;
    case CREATE_GEOMETRY_SHADER_WITH_SO:
      Serialise_CreateGeometryShaderWithStreamOutput(0x0, 0, 0x0, 0, 0x0, 0, 0, 0x0, 0x0);
      break;
    case CREATE_PIXEL_SHADER: Serialise_CreatePixelShader(0x0, 0, 0x0, 0x0); break;
    case CREATE_COMPUTE_SHADER: Serialise_CreateComputeShader(0x0, 0, 0x0, 0x0); break;
    case GET_CLASS_INSTANCE: Serialise_GetClassInstance(0x0, 0, 0x0, 0x0); break;
    case CREATE_CLASS_INSTANCE: Serialise_CreateClassInstance(0x0, 0, 0, 0, 0, 0x0, 0x0); break;
    case CREATE_CLASS_LINKAGE: Serialise_CreateClassLinkage(0x0); break;
    case CREATE_SRV: Serialise_CreateShaderResourceView(0x0, 0x0, 0x0); break;
    case CREATE_SRV1: Serialise_CreateShaderResourceView1(0x0, 0x0, 0x0); break;
    case CREATE_RTV: Serialise_CreateRenderTargetView(0x0, 0x0, 0x0); break;
    case CREATE_RTV1: Serialise_CreateRenderTargetView1(0x0, 0x0, 0x0); break;
    case CREATE_DSV: Serialise_CreateDepthStencilView(0x0, 0x0, 0x0); break;
    case CREATE_UAV: Serialise_CreateUnorderedAccessView(0x0, 0x0, 0x0); break;
    case CREATE_UAV1: Serialise_CreateUnorderedAccessView1(0x0, 0x0, 0x0); break;
    case CREATE_INPUT_LAYOUT: Serialise_CreateInputLayout(0x0, 0, 0x0, 0, 0x0); break;
    case CREATE_BLEND_STATE: Serialise_CreateBlendState(0x0, 0x0); break;
    case CREATE_BLEND_STATE1: Serialise_CreateBlendState1(0x0, 0x0); break;
    case CREATE_DEPTHSTENCIL_STATE: Serialise_CreateDepthStencilState(0x0, 0x0); break;
    case CREATE_RASTER_STATE: Serialise_CreateRasterizerState(0x0, 0x0); break;
    case CREATE_RASTER_STATE1: Serialise_CreateRasterizerState1(0x0, 0x0); break;
    case CREATE_RASTER_STATE2: Serialise_CreateRasterizerState2(0x0, 0x0); break;
    case CREATE_SAMPLER_STATE: Serialise_CreateSamplerState(0x0, 0x0); break;
    case CREATE_QUERY: Serialise_CreateQuery(0x0, 0x0); break;
    case CREATE_QUERY1: Serialise_CreateQuery1(0x0, 0x0); break;
    case CREATE_PREDICATE: Serialise_CreatePredicate(0x0, 0x0); break;
    case CREATE_COUNTER: Serialise_CreateCounter(0x0, 0x0); break;
    case CREATE_DEFERRED_CONTEXT: Serialise_CreateDeferredContext(0, 0x0); break;
    case SET_EXCEPTION_MODE: Serialise_SetExceptionMode(0); break;
    case OPEN_SHARED_RESOURCE:
    {
      IID nul;
      Serialise_OpenSharedResource(0, nul, NULL);
      break;
    }
    case CAPTURE_SCOPE: Serialise_CaptureScope(offset); break;
    case SET_SHADER_DEBUG_PATH: Serialise_SetShaderDebugPath(NULL, NULL); break;
    default:
      // ignore system chunks
      if(context == INITIAL_CONTENTS)
        Serialise_InitialState(ResourceId(), NULL);
      else if(context < FIRST_CHUNK_ID)
        m_pSerialiser->SkipCurrentChunk();
      else
        m_pImmediateContext->ProcessChunk(offset, context, true);
      break;
  }
}

void WrappedID3D11Device::Serialise_CaptureScope(uint64_t offset)
{
  SERIALISE_ELEMENT(uint32_t, FrameNumber, m_FrameCounter);

  if(m_State >= WRITING)
  {
    GetResourceManager()->Serialise_InitialContentsNeeded();
  }
  else
  {
    m_FrameRecord.frameInfo.fileOffset = offset;
    m_FrameRecord.frameInfo.frameNumber = FrameNumber;

    FrameStatistics &stats = m_FrameRecord.frameInfo.stats;
    RDCEraseEl(stats);

    // #mivance GL/Vulkan don't set this so don't get stats in window
    stats.recorded = true;

    for(uint32_t stage = uint32_t(ShaderStage::First); stage < uint32_t(ShaderStage::Count); stage++)
    {
      stats.constants[stage].bindslots.resize(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT + 1);
      stats.constants[stage].sizes.resize(ConstantBindStats::BucketCount);

      stats.samplers[stage].bindslots.resize(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT + 1);

      stats.resources[stage].types.resize(uint32_t(TextureDim::Count));
      stats.resources[stage].bindslots.resize(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT + 1);
    }

    stats.updates.types.resize(uint32_t(TextureDim::Count));
    stats.updates.sizes.resize(ResourceUpdateStats::BucketCount);

    stats.draws.counts.resize(DrawcallStats::BucketCount);

    stats.vertices.bindslots.resize(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT + 1);

    stats.rasters.viewports.resize(D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 2);
    stats.rasters.rects.resize(D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX + 2);

    stats.outputs.bindslots.resize(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + D3D11_1_UAV_SLOT_COUNT +
                                   1);

    GetResourceManager()->CreateInitialContents();
  }
}

void WrappedID3D11Device::ReadLogInitialisation(RDCFile *rdc)
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

  map<D3D11ChunkType, chunkinfo> chunkInfos;

  SCOPED_TIMER("chunk initialisation");

  for(;;)
  {
    PerformanceTimer timer;

    uint64_t offset = m_pSerialiser->GetOffset();

    D3D11ChunkType context = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

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

      m_pImmediateContext->ReplayLog(READING, 0, 0, false);
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

  DrawcallDescription *previous = NULL;
  SetupDrawcallPointers(&m_Drawcalls, GetFrameRecord().drawcallList, NULL, previous);

#if ENABLED(RDOC_DEVEL)
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

  m_FrameRecord.frameInfo.uncompressedFileSize = m_pSerialiser->GetSize();
  m_FrameRecord.frameInfo.compressedFileSize = m_pSerialiser->GetFileSize();
  m_FrameRecord.frameInfo.persistentSize = m_pSerialiser->GetSize() - frameOffset;
  m_FrameRecord.frameInfo.initDataSize = chunkInfos[(D3D11ChunkType)INITIAL_CONTENTS].totalsize;

  RDCDEBUG("Allocating %llu persistant bytes of memory for the log.",
           m_pSerialiser->GetSize() - frameOffset);

  m_pSerialiser->SetDebugText(false);
}

void WrappedID3D11Device::ReplayLog(uint32_t startEventID, uint32_t endEventID,
                                    ReplayLogType replayType)
{
  uint64_t offs = m_FrameRecord.frameInfo.fileOffset;

  m_pSerialiser->SetOffset(offs);

  bool partial = true;

  if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
  {
    startEventID = 1;
    partial = false;
  }

  D3D11ChunkType header = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

  RDCASSERTEQUAL(header, CAPTURE_SCOPE);

  m_pSerialiser->SkipCurrentChunk();

  m_pSerialiser->PopContext(header);

  if(!partial)
  {
    D3D11MarkerRegion apply("!!!!RenderDoc Internal: ApplyInitialContents");
    GetResourceManager()->ApplyInitialContents();
    GetResourceManager()->ReleaseInFrameResources();
  }

  m_State = EXECUTING;

  D3D11MarkerRegion::Set(StringFormat::Fmt("!!!!RenderDoc Internal: Replay %d (%d): %u->%u",
                                           (int)replayType, (int)partial, startEventID, endEventID));

  m_ReplayEventCount = 0;

  if(replayType == eReplay_Full)
    m_pImmediateContext->ReplayLog(EXECUTING, startEventID, endEventID, partial);
  else if(replayType == eReplay_WithoutDraw)
    m_pImmediateContext->ReplayLog(EXECUTING, startEventID, RDCMAX(1U, endEventID) - 1, partial);
  else if(replayType == eReplay_OnlyDraw)
    m_pImmediateContext->ReplayLog(EXECUTING, endEventID, endEventID, partial);
  else
    RDCFATAL("Unexpected replay type");

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

bool WrappedID3D11Device::Serialise_WrapSwapchainBuffer(WrappedIDXGISwapChain4 *swap,
                                                        DXGI_SWAP_CHAIN_DESC *swapDesc, UINT buffer,
                                                        IUnknown *realSurface)
{
  WrappedID3D11Texture2D1 *pTex = (WrappedID3D11Texture2D1 *)realSurface;

  SERIALISE_ELEMENT(DXGI_FORMAT, swapFormat, swapDesc->BufferDesc.Format);
  SERIALISE_ELEMENT(uint32_t, BuffNum, buffer);
  SERIALISE_ELEMENT(ResourceId, pTexture, pTex->GetResourceID());

  m_BBID = pTexture;

  if(m_State >= WRITING)
  {
    D3D11_TEXTURE2D_DESC desc;

    pTex->GetDesc(&desc);

    SERIALISE_ELEMENT(D3D11_TEXTURE2D_DESC, Descriptor, desc);
  }
  else
  {
    ID3D11Texture2D *fakeBB;

    SERIALISE_ELEMENT(D3D11_TEXTURE2D_DESC, Descriptor, D3D11_TEXTURE2D_DESC());

    D3D11_TEXTURE2D_DESC realDescriptor = Descriptor;

    // DXGI swap chain back buffers can be freely cast as a special-case.
    // translate the format to a typeless format to allow for this.
    // the original type will be stored in the texture below
    Descriptor.Format = GetTypelessFormat(Descriptor.Format);

    HRESULT hr = m_pDevice->CreateTexture2D(&Descriptor, NULL, &fakeBB);

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

      GetResourceManager()->AddLiveResource(pTexture, fakeBB);
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

  LazyInit();

  // there shouldn't be a resource record for this texture as it wasn't created via
  // CreateTexture2D
  RDCASSERT(id != ResourceId() && !GetResourceManager()->HasResourceRecord(id));

  if(m_State >= WRITING)
  {
    D3D11ResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
    record->DataInSerialiser = false;
    record->SpecialResource = true;
    record->Length = 0;
    record->NumSubResources = 0;
    record->SubResources = NULL;

    SCOPED_LOCK(m_D3DLock);

    SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);

    Serialise_WrapSwapchainBuffer(swap, swapDesc, buffer, pTex);

    record->AddChunk(scope.Get());
  }

  if(buffer == 0 && m_State >= WRITING)
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
  if(m_State != WRITING_IDLE)
    return;

  SCOPED_LOCK(m_D3DLock);

  RenderDoc::Inst().SetCurrentDriver(RDC_D3D11);

  m_State = WRITING_CAPFRAME;

  m_AppControlledCapture = true;

  m_Failures = 0;
  m_FailedFrame = 0;
  m_FailedReason = CaptureSucceeded;

  m_FrameCounter = RDCMAX(1 + (uint32_t)m_CapturedFrames.size(), m_FrameCounter);

  FrameDescription frame;
  frame.frameNumber = m_FrameCounter + 1;
  frame.captureTime = Timing::GetUnixTimestamp();
  m_CapturedFrames.push_back(frame);

  m_DebugMessages.clear();

  GetResourceManager()->ClearReferencedResources();

  GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Write);

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
  if(m_State != WRITING_CAPFRAME)
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
    SCOPED_LOCK(m_D3DLock);

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

    byte *thpixels = NULL;
    uint32_t thwidth = 0;
    uint32_t thheight = 0;

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
            byte *data = (byte *)mapped.pData;

            float aspect = float(desc.Width) / float(desc.Height);

            thwidth = RDCMIN(maxSize, desc.Width);
            thwidth &= ~0x7;    // align down to multiple of 8
            thheight = uint32_t(float(thwidth) / aspect);

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

            byte *dst = thpixels;

            for(uint32_t y = 0; y < thheight; y++)
            {
              for(uint32_t x = 0; x < thwidth; x++)
              {
                float xf = float(x) / float(thwidth);
                float yf = float(y) / float(thheight);

                byte *src =
                    &data[stride * uint32_t(xf * widthf) + mapped.RowPitch * uint32_t(yf * heightf)];

                if(buf1010102)
                {
                  uint32_t *src1010102 = (uint32_t *)src;
                  Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
                  dst[0] = (byte)(unorm.x * 255.0f);
                  dst[1] = (byte)(unorm.y * 255.0f);
                  dst[2] = (byte)(unorm.z * 255.0f);
                }
                else if(bufBGRA)
                {
                  dst[0] = src[2];
                  dst[1] = src[1];
                  dst[2] = src[0];
                }
                else if(fmt.compByteWidth == 2)    // R16G16B16A16 backbuffer
                {
                  uint16_t *src16 = (uint16_t *)src;

                  float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
                  float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
                  float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

                  if(linearR < 0.0031308f)
                    dst[0] = byte(255.0f * (12.92f * linearR));
                  else
                    dst[0] = byte(255.0f * (1.055f * powf(linearR, 1.0f / 2.4f) - 0.055f));

                  if(linearG < 0.0031308f)
                    dst[1] = byte(255.0f * (12.92f * linearG));
                  else
                    dst[1] = byte(255.0f * (1.055f * powf(linearG, 1.0f / 2.4f) - 0.055f));

                  if(linearB < 0.0031308f)
                    dst[2] = byte(255.0f * (12.92f * linearB));
                  else
                    dst[2] = byte(255.0f * (1.055f * powf(linearB, 1.0f / 2.4f) - 0.055f));
                }
                else
                {
                  dst[0] = src[0];
                  dst[1] = src[1];
                  dst[2] = src[2];
                }

                dst += 3;
              }
            }

            m_pImmediateContext->GetReal()->Unmap(stagingTex, 0);
          }
        }

        stagingTex->Release();
      }
    }

    byte *jpgbuf = NULL;
    int len = thwidth * thheight;

    if(wnd)
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

    Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(
        m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

    SAFE_DELETE_ARRAY(jpgbuf);
    SAFE_DELETE(thpixels);

    {
      SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

      SERIALISE_ELEMENT(ResourceId, immContextId, m_pImmediateContext->GetResourceID());

      m_pFileSerialiser->Insert(scope.Get(true));
    }

    RDCDEBUG("Inserting Resource Serialisers");

    LockForChunkFlushing();

    GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);

    GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

    RDCDEBUG("Creating Capture Scope");

    {
      SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

      Serialise_CaptureScope(0);

      m_pFileSerialiser->Insert(scope.Get(true));
    }

    {
      RDCDEBUG("Getting Resource Record");

      D3D11ResourceRecord *record =
          m_ResourceManager->GetResourceRecord(m_pImmediateContext->GetResourceID());

      RDCDEBUG("Accumulating context resource list");

      map<int32_t, Chunk *> recordlist;
      record->Insert(recordlist);

      RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());

      for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
        m_pFileSerialiser->Insert(it->second);

      RDCDEBUG("Done");
    }

    m_pFileSerialiser->FlushToDisk();

    UnlockForChunkFlushing();

    SAFE_DELETE(m_pFileSerialiser);

    RenderDoc::Inst().SuccessfullyWrittenLog(m_FrameCounter);

    m_State = WRITING_IDLE;

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
        GetDebugManager()->SetOutputDimensions(swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height);
        GetDebugManager()->SetOutputWindow(swapDesc.OutputWindow);

        GetDebugManager()->RenderText(0.0f, 0.0f, "Failed to capture frame %u: %s", m_FrameCounter,
                                      reasonString);
      }

      old.ApplyState(m_pImmediateContext);
    }

    m_CapturedFrames.back().frameNumber = m_FrameCounter + 1;

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

      m_State = WRITING_IDLE;

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
      GetResourceManager()->MarkResourceFrameReferenced(m_ResourceID, eFrameRef_Write);
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
  if(m_State == WRITING_IDLE && RenderDoc::Inst().ShouldTriggerCapture(0))
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

  if(m_State == WRITING_IDLE)
    RenderDoc::Inst().Tick();

  m_pImmediateContext->EndFrame();

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  m_pImmediateContext->BeginFrame();

  DXGI_SWAP_CHAIN_DESC swapdesc = swap->GetDescWithHWND();
  bool activeWindow = RenderDoc::Inst().IsActiveWindow((ID3D11Device *)this, swapdesc.OutputWindow);

  if(m_State == WRITING_IDLE)
  {
    D3D11RenderState old = *m_pImmediateContext->GetCurrentPipelineState();

    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      ID3D11RenderTargetView *rtv = m_SwapChains[swap];

      m_pImmediateContext->GetReal()->OMSetRenderTargets(1, &rtv, NULL);

      DXGI_SWAP_CHAIN_DESC swapDesc = {0};
      swap->GetDesc(&swapDesc);
      GetDebugManager()->SetOutputDimensions(swapDesc.BufferDesc.Width, swapDesc.BufferDesc.Height);
      GetDebugManager()->SetOutputWindow(swapDesc.OutputWindow);

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      string overlayText = RenderDoc::Inst().GetOverlayText(RDC_D3D11, m_FrameCounter, flags);

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
        GetDebugManager()->RenderText(0.0f, 0.0f, overlayText.c_str());

      old.ApplyState(m_pImmediateContext);
    }
  }

  if(!activeWindow)
    return S_OK;

  RenderDoc::Inst().SetCurrentDriver(RDC_D3D11);

  // kill any current capture that isn't application defined
  if(m_State == WRITING_CAPFRAME && !m_AppControlledCapture)
  {
    m_pImmediateContext->Present(SyncInterval, Flags);

    RenderDoc::Inst().EndFrameCapture((ID3D11Device *)this, swapdesc.OutputWindow);
  }

  if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE)
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

bool WrappedID3D11Device::Serialise_SetShaderDebugPath(ID3D11DeviceChild *res, const char *p)
{
  SERIALISE_ELEMENT(ResourceId, resource, GetIDForResource(res));
  string debugPath = p ? p : "";
  m_pSerialiser->Serialise("debugPath", debugPath);

  if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
  {
    auto it = WrappedShader::m_ShaderList.find(GetResourceManager()->GetLiveID(resource));

    if(it != WrappedShader::m_ShaderList.end())
      it->second->SetDebugInfoPath(debugPath);
  }

  return true;
}

HRESULT WrappedID3D11Device::SetShaderDebugPath(ID3D11DeviceChild *res, const char *path)
{
  if(m_State >= WRITING)
  {
    ResourceId idx = GetIDForResource(res);
    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);

    if(record == NULL)
    {
      RDCERR("Setting shader debug path on object %p of type %d that has no resource record.", res,
             IdentifyTypeByPtr(res));
      return E_INVALIDARG;
    }

    RDCASSERT(idx != ResourceId());

    {
      SCOPED_SERIALISE_CONTEXT(SET_SHADER_DEBUG_PATH);
      Serialise_SetShaderDebugPath(res, path);
      record->AddChunk(scope.Get());
    }

    return S_OK;
  }

  return S_OK;
}

bool WrappedID3D11Device::Serialise_SetResourceName(ID3D11DeviceChild *res, const char *nm)
{
  SERIALISE_ELEMENT(ResourceId, resource, GetIDForResource(res));
  string name = nm ? nm : "";
  m_pSerialiser->Serialise("name", name);

  if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
  {
    ID3D11DeviceChild *r = GetResourceManager()->GetLiveResource(resource);

    SetDebugName(r, name.c_str());
  }

  return true;
}

void WrappedID3D11Device::SetResourceName(ID3D11DeviceChild *res, const char *name)
{
  // don't allow naming device contexts or command lists so we know this chunk
  // is always on a pre-capture chunk.
  if(m_State >= WRITING && !WrappedID3D11DeviceContext::IsAlloc(res) &&
     !WrappedID3D11CommandList::IsAlloc(res))
  {
    ResourceId idx = GetIDForResource(res);
    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(idx);

    if(record == NULL)
      record = m_DeviceRecord;

    RDCASSERT(idx != ResourceId());

    SCOPED_LOCK(m_D3DLock);
    {
      SCOPED_SERIALISE_CONTEXT(SET_RESOURCE_NAME);

      Serialise_SetResourceName(res, name);

      LockForChunkRemoval();

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

      UnlockForChunkRemoval();

      record->AddChunk(scope.Get());
    }
  }
}

bool WrappedID3D11Device::Serialise_ReleaseResource(ID3D11DeviceChild *res)
{
  ResourceType resourceType = Resource_Unknown;
  ResourceId resource = GetIDForResource(res);

  if(m_State >= WRITING)
  {
    resourceType = IdentifyTypeByPtr(res);
  }

  if(m_State == WRITING_IDLE || m_State < WRITING)
  {
    SERIALISE_ELEMENT(ResourceId, serRes, GetIDForResource(res));
    SERIALISE_ELEMENT(ResourceType, serType, resourceType);

    resourceType = serType;
    resource = serRes;
  }

  if(m_State >= WRITING)
  {
    D3D11ResourceRecord *record = GetResourceManager()->GetResourceRecord(resource);
    if(record)
      record->Delete(m_ResourceManager);
  }
  if(m_State < WRITING && GetResourceManager()->HasLiveResource(resource))
  {
    res = GetResourceManager()->GetLiveResource(resource);
    GetResourceManager()->EraseLiveResource(resource);
    SAFE_RELEASE(res);
  }

  return true;
}

void WrappedID3D11Device::ReleaseResource(ID3D11DeviceChild *res)
{
  ResourceId idx = GetIDForResource(res);

  // wrapped resources get released all the time, we don't want to
  // try and slerp in a resource release. Just the explicit ones
  if(m_State < WRITING)
  {
    if(GetResourceManager()->HasLiveResource(idx))
      GetResourceManager()->EraseLiveResource(idx);
    return;
  }

  SCOPED_LOCK(m_D3DLock);

  ResourceType type = IdentifyTypeByPtr(res);

  D3D11ResourceRecord *record = m_DeviceRecord;

  if(m_State == WRITING_IDLE)
  {
    if(type == Resource_ShaderResourceView || type == Resource_DepthStencilView ||
       type == Resource_UnorderedAccessView || type == Resource_RenderTargetView ||
       type == Resource_Buffer || type == Resource_Texture1D || type == Resource_Texture2D ||
       type == Resource_Texture3D || type == Resource_CommandList)
    {
      record = GetResourceManager()->GetResourceRecord(idx);
      RDCASSERT(record);

      if(record->SpecialResource)
      {
        record = m_DeviceRecord;
      }
      else if(record->GetRefCount() == 1)
      {
        // we're about to decrement this chunk out of existance!
        // don't hold onto the record to add the chunk.
        record = NULL;
      }
    }
  }

  GetResourceManager()->MarkCleanResource(idx);

  if(type == Resource_DeviceContext)
  {
    RemoveDeferredContext((WrappedID3D11DeviceContext *)res);
  }

  bool serialiseRelease = true;

  WrappedID3D11CommandList *cmdList = (WrappedID3D11CommandList *)res;

  // don't serialise releases of counters or queries since we ignore them.
  // Also don't serialise releases of command lists that weren't captured,
  // since their creation won't be in the log either.
  if(type == Resource_Counter || type == Resource_Query ||
     (type == Resource_CommandList && !cmdList->IsCaptured()))
    serialiseRelease = false;

  if(type == Resource_DeviceState)
    serialiseRelease = false;

  if(type == Resource_CommandList && !cmdList->IsCaptured())
  {
    record = GetResourceManager()->GetResourceRecord(idx);
    if(record)
      record->Delete(GetResourceManager());
  }

  if(serialiseRelease)
  {
    if(m_State == WRITING_CAPFRAME)
    {
      Serialise_ReleaseResource(res);
    }
    else
    {
      SCOPED_SERIALISE_CONTEXT(RELEASE_RESOURCE);
      Serialise_ReleaseResource(res);

      if(record)
      {
        record->AddChunk(scope.Get());
      }
    }

    if(record == NULL)
    {
      // if record is NULL then we just deleted a reference-less resource.
      // That means it is not used and can be safely discarded, so just
      // throw away the serialiser contents
      m_pSerialiser->Rewind();
    }
  }
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

const DrawcallDescription *WrappedID3D11Device::GetDrawcall(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}
