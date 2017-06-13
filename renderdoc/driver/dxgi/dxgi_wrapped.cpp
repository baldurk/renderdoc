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

#include "driver/dxgi/dxgi_wrapped.h"
#include <stddef.h>
#include <stdio.h>
#include "core/core.h"
#include "serialise/serialiser.h"

string ToStrHelper<false, IID>::Get(const IID &el)
{
  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "GUID {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                         el.Data1, (unsigned int)el.Data2, (unsigned int)el.Data3, el.Data4[0],
                         el.Data4[1], el.Data4[2], el.Data4[3], el.Data4[4], el.Data4[5],
                         el.Data4[6], el.Data4[7]);

  return tostrBuf;
}

WRAPPED_POOL_INST(WrappedIDXGIDevice4);

std::vector<D3DDeviceCallback> WrappedIDXGISwapChain4::m_D3DCallbacks;

ID3DDevice *GetD3DDevice(IUnknown *pDevice)
{
  ID3DDevice *wrapDevice = NULL;

  if(WrappedIDXGIDevice4::IsAlloc(pDevice))
    wrapDevice = ((WrappedIDXGIDevice4 *)(IDXGIDevice3 *)pDevice)->GetD3DDevice();

  if(wrapDevice == NULL)
    wrapDevice = WrappedIDXGISwapChain4::GetD3DDevice(pDevice);

  return wrapDevice;
}

bool RefCountDXGIObject::HandleWrap(REFIID riid, void **ppvObject)
{
  if(ppvObject == NULL || *ppvObject == NULL)
  {
    RDCWARN("HandleWrap called with NULL ppvObject");
    return false;
  }

  // unknown GUID that we only want to print once to avoid log spam
  // {79D2046C-22EF-451B-9E74-2245D9C760EA}
  static const GUID Unknown_uuid = {
      0x79d2046c, 0x22ef, 0x451b, {0x9e, 0x74, 0x22, 0x45, 0xd9, 0xc7, 0x60, 0xea}};

  // ditto
  // {9B7E4C04-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10Texture2D_uuid = {
      0x9b7e4c04, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  if(riid == __uuidof(IDXGIDevice))
  {
    // should have been handled elsewhere, so we can properly create this device
    RDCERR("Unexpected uuid in RefCountDXGIObject::HandleWrap");
    return false;
  }
  else if(riid == __uuidof(IDXGIAdapter))
  {
    IDXGIAdapter *real = (IDXGIAdapter *)(*ppvObject);
    *ppvObject = (IDXGIAdapter *)(new WrappedIDXGIAdapter3(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory))
  {
    // yes I know PRECISELY how fucked up this is. Speak to microsoft - after KB2670838 the internal
    // D3D11 device creation function will pass in __uuidof(IDXGIFactory) then attempt to call
    // EnumDevices1 (which is in the IDXGIFactory1 vtable). Doing this *should* be safe as using a
    // IDXGIFactory1 like a IDXGIFactory should all just work by definition, but there's no way to
    // know now if someone trying to create a IDXGIFactory really means it or not.
    IDXGIFactory *real = (IDXGIFactory *)(*ppvObject);
    *ppvObject = (IDXGIFactory *)(new WrappedIDXGIFactory5(real));
    return true;
  }

  else if(riid == __uuidof(IDXGIDevice1))
  {
    // should have been handled elsewhere, so we can properly create this device
    RDCERR("Unexpected uuid in RefCountDXGIObject::HandleWrap");
    return false;
  }
  else if(riid == __uuidof(IDXGIAdapter1))
  {
    IDXGIAdapter1 *real = (IDXGIAdapter1 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter1 *)(new WrappedIDXGIAdapter3(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory1))
  {
    IDXGIFactory1 *real = (IDXGIFactory1 *)(*ppvObject);
    *ppvObject = (IDXGIFactory1 *)(new WrappedIDXGIFactory5(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIAdapter2))
  {
    IDXGIAdapter2 *real = (IDXGIAdapter2 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter2 *)(new WrappedIDXGIAdapter3(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIAdapter3))
  {
    IDXGIAdapter3 *real = (IDXGIAdapter3 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter3 *)(new WrappedIDXGIAdapter3(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory2))
  {
    IDXGIFactory2 *real = (IDXGIFactory2 *)(*ppvObject);
    *ppvObject = (IDXGIFactory2 *)(new WrappedIDXGIFactory5(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory3))
  {
    IDXGIFactory3 *real = (IDXGIFactory3 *)(*ppvObject);
    *ppvObject = (IDXGIFactory3 *)(new WrappedIDXGIFactory5(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory4))
  {
    IDXGIFactory4 *real = (IDXGIFactory4 *)(*ppvObject);
    *ppvObject = (IDXGIFactory4 *)(new WrappedIDXGIFactory5(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory5))
  {
    IDXGIFactory5 *real = (IDXGIFactory5 *)(*ppvObject);
    *ppvObject = (IDXGIFactory5 *)(new WrappedIDXGIFactory5(real));
    return true;
  }
  else if(riid == ID3D10Texture2D_uuid)
  {
    static bool printed = false;
    if(!printed)
    {
      printed = true;
      RDCWARN("Querying IDXGIObject for unsupported D3D10 interface: %s", ToStr::Get(riid).c_str());
    }
    return false;
  }
  else if(riid == Unknown_uuid)
  {
    static bool printed = false;
    if(!printed)
    {
      printed = true;
      RDCWARN("Querying IDXGIObject for unknown GUID: %s", ToStr::Get(riid).c_str());
    }
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying IDXGIObject for interface: %s", guid.c_str());
  }

  return false;
}

HRESULT STDMETHODCALLTYPE RefCountDXGIObject::GetParent(
    /* [in] */ REFIID riid,
    /* [retval][out] */ void **ppParent)
{
  HRESULT ret = m_pReal->GetParent(riid, ppParent);

  if(SUCCEEDED(ret))
    HandleWrap(riid, ppParent);

  return ret;
}

HRESULT RefCountDXGIObject::WrapQueryInterface(IUnknown *real, REFIID riid, void **ppvObject)
{
  HRESULT ret = real->QueryInterface(riid, ppvObject);

  if(SUCCEEDED(ret))
    HandleWrap(riid, ppvObject);

  return ret;
}

WrappedIDXGISwapChain4::WrappedIDXGISwapChain4(IDXGISwapChain *real, HWND wnd, ID3DDevice *device)
    : RefCountDXGIObject(real), m_pReal(real), m_pDevice(device), m_Wnd(wnd)
{
  DXGI_SWAP_CHAIN_DESC desc;
  real->GetDesc(&desc);

  m_pDevice->AddRef();

  m_pReal1 = NULL;
  real->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&m_pReal1);
  m_pReal2 = NULL;
  real->QueryInterface(__uuidof(IDXGISwapChain2), (void **)&m_pReal2);
  m_pReal3 = NULL;
  real->QueryInterface(__uuidof(IDXGISwapChain3), (void **)&m_pReal3);
  m_pReal4 = NULL;
  real->QueryInterface(__uuidof(IDXGISwapChain4), (void **)&m_pReal4);

  WrapBuffersAfterResize();

  // we do a 'fake' present right at the start, so that we can capture frame 1, by
  // going from this fake present to the first present.
  m_pDevice->FirstFrame(this);
}

WrappedIDXGISwapChain4::~WrappedIDXGISwapChain4()
{
  m_pDevice->ReleaseSwapchainResources(this, 0, NULL, NULL);

  SAFE_RELEASE(m_pDevice);

  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGISwapChain4::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IDXGISwapChain))
  {
    AddRef();
    *ppvObject = (IDXGISwapChain *)this;
    return S_OK;
  }
  else if(riid == __uuidof(IDXGISwapChain1))
  {
    if(m_pReal1)
    {
      AddRef();
      *ppvObject = (IDXGISwapChain1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGISwapChain2))
  {
    if(m_pReal2)
    {
      AddRef();
      *ppvObject = (IDXGISwapChain2 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGISwapChain3))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGISwapChain3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGISwapChain4))
  {
    if(m_pReal4)
    {
      AddRef();
      *ppvObject = (IDXGISwapChain4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying IDXGISwapChain for interface: %s", guid.c_str());
  }

  return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

void WrappedIDXGISwapChain4::ReleaseBuffersForResize(UINT QueueCount, IUnknown *const *ppPresentQueue,
                                                     IUnknown **unwrappedQueues)
{
  m_pDevice->ReleaseSwapchainResources(this, QueueCount, ppPresentQueue, unwrappedQueues);
}

void WrappedIDXGISwapChain4::WrapBuffersAfterResize()
{
  DXGI_SWAP_CHAIN_DESC desc;
  m_pReal->GetDesc(&desc);

  int bufCount = desc.BufferCount;

  if(desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD)
    bufCount = 1;

  RDCASSERT(bufCount < MAX_NUM_BACKBUFFERS);

  for(int i = 0; i < MAX_NUM_BACKBUFFERS; i++)
  {
    m_pBackBuffers[i] = NULL;

    if(i < bufCount)
    {
      GetBuffer(i, m_pDevice->GetBackbufferUUID(), (void **)&m_pBackBuffers[i]);
      m_pDevice->NewSwapchainBuffer(m_pBackBuffers[i]);
    }
  }
}

HRESULT WrappedIDXGISwapChain4::ResizeBuffers(
    /* [in] */ UINT BufferCount,
    /* [in] */ UINT Width,
    /* [in] */ UINT Height,
    /* [in] */ DXGI_FORMAT NewFormat,
    /* [in] */ UINT SwapChainFlags)
{
  ReleaseBuffersForResize(0, NULL, NULL);

  HRESULT ret = m_pReal->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

  WrapBuffersAfterResize();

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGISwapChain4::GetContainingOutput(IDXGIOutput **ppOutput)
{
  HRESULT ret = m_pReal->GetContainingOutput(ppOutput);

  if(SUCCEEDED(ret) && ppOutput && *ppOutput)
    *ppOutput = (IDXGIOutput *)(new WrappedIDXGIOutput5(this, *ppOutput));

  return ret;
}

HRESULT WrappedIDXGISwapChain4::ResizeBuffers1(_In_ UINT BufferCount, _In_ UINT Width,
                                               _In_ UINT Height, _In_ DXGI_FORMAT Format,
                                               _In_ UINT SwapChainFlags,
                                               _In_reads_(BufferCount) const UINT *pCreationNodeMask,
                                               _In_reads_(BufferCount)
                                                   IUnknown *const *ppPresentQueue)
{
  IUnknown **unwrappedQueues = NULL;

  if(ppPresentQueue)
    unwrappedQueues = new IUnknown *[BufferCount];

  ReleaseBuffersForResize(BufferCount, ppPresentQueue, unwrappedQueues);

  HRESULT ret = m_pReal3->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags,
                                         pCreationNodeMask, unwrappedQueues);

  SAFE_DELETE_ARRAY(unwrappedQueues);

  WrapBuffersAfterResize();

  return ret;
}

HRESULT WrappedIDXGISwapChain4::SetFullscreenState(
    /* [in] */ BOOL Fullscreen,
    /* [in] */ IDXGIOutput *pTarget)
{
  WrappedIDXGIOutput5 *wrappedOutput = (WrappedIDXGIOutput5 *)pTarget;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
    return m_pReal->SetFullscreenState(Fullscreen, unwrappedOutput);

  return S_OK;
}

HRESULT WrappedIDXGISwapChain4::GetFullscreenState(
    /* [out] */ BOOL *pFullscreen,
    /* [out] */ IDXGIOutput **ppTarget)
{
  HRESULT ret = m_pReal->GetFullscreenState(pFullscreen, ppTarget);

  if(SUCCEEDED(ret) && ppTarget && *ppTarget)
    *ppTarget = (IDXGIOutput *)(new WrappedIDXGIOutput5(this, *ppTarget));

  return ret;
}

HRESULT WrappedIDXGISwapChain4::GetBuffer(
    /* [in] */ UINT Buffer,
    /* [in] */ REFIID riid,
    /* [out][in] */ void **ppSurface)
{
  if(ppSurface == NULL)
    return E_INVALIDARG;

  // ID3D10Texture2D UUID {9B7E4C04-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10Texture2D_uuid = {
      0x9b7e4c04, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  // ID3D10Resource  UUID {9B7E4C01-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10Resource_uuid = {
      0x9b7e4c01, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  IID uuid = riid;

  if(uuid == ID3D10Texture2D_uuid || uuid == ID3D10Resource_uuid)
  {
    RDCERR("Querying swapchain buffers via D3D10 interface UUIDs is not supported");
    return E_NOINTERFACE;
  }
  else if(uuid == __uuidof(IDXGISurface))
  {
    RDCWARN("Querying swapchain buffer for IDXGISurface. This query is ambiguous.");

    // query as native format so that wrapping works as expected
    uuid = m_pDevice->GetBackbufferUUID();
  }
  else if(uuid != __uuidof(ID3D11Texture2D) && uuid != __uuidof(ID3D11Resource) &&
          uuid != __uuidof(ID3D12Resource))
  {
    RDCERR("Unsupported or unrecognised UUID passed to IDXGISwapChain::GetBuffer - %s",
           ToStr::Get(uuid).c_str());
    return E_NOINTERFACE;
  }

  RDCASSERT(uuid == __uuidof(ID3D11Texture2D) || uuid == __uuidof(ID3D11Resource) ||
            uuid == __uuidof(ID3D12Resource));

  HRESULT ret = m_pReal->GetBuffer(Buffer, uuid, ppSurface);

  {
    IUnknown *realSurface = (IUnknown *)*ppSurface;
    IUnknown *tex = realSurface;

    if(FAILED(ret))
    {
      RDCERR("Failed to get swapchain backbuffer %d: %08x", Buffer, ret);
      SAFE_RELEASE(realSurface);
      tex = NULL;
    }
    else
    {
      DXGI_SWAP_CHAIN_DESC desc;
      GetDesc(&desc);
      tex = m_pDevice->WrapSwapchainBuffer(this, &desc, Buffer, realSurface);
    }

    // if the original UUID was IDXGISurface, fixup for the expected interface being returned
    if(riid == __uuidof(IDXGISurface))
    {
      IDXGISurface *surf = NULL;
      HRESULT hr = tex->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
      RDCASSERTEQUAL(hr, S_OK);

      tex->Release();
      tex = surf;
    }

    *ppSurface = tex;
  }

  return ret;
}

HRESULT WrappedIDXGISwapChain4::GetDevice(
    /* [in] */ REFIID riid,
    /* [retval][out] */ void **ppDevice)
{
  HRESULT ret = m_pReal->GetDevice(riid, ppDevice);

  if(SUCCEEDED(ret))
  {
    // try one of the trivial wraps, we don't mind making a new one of those
    if(m_pDevice->IsDeviceUUID(riid))
    {
      // probably they're asking for the device device.
      *ppDevice = m_pDevice->GetDeviceInterface(riid);
      m_pDevice->AddRef();
    }
    else if(riid == __uuidof(IDXGISwapChain))
    {
      // don't think anyone would try this, but what the hell.
      *ppDevice = this;
      AddRef();
    }
    else if(!HandleWrap(riid, ppDevice))
    {
      // can probably get away with returning the real result here,
      // but it worries me a bit.
      RDCUNIMPLEMENTED("Not returning trivial type");
    }
  }

  return ret;
}

HRESULT WrappedIDXGISwapChain4::Present(
    /* [in] */ UINT SyncInterval,
    /* [in] */ UINT Flags)
{
  if(!RenderDoc::Inst().GetCaptureOptions().AllowVSync)
  {
    SyncInterval = 0;
  }

  m_pDevice->Present(this, SyncInterval, Flags);

  return m_pReal->Present(SyncInterval, Flags);
}

HRESULT WrappedIDXGISwapChain4::Present1(UINT SyncInterval, UINT Flags,
                                         const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
  if(!RenderDoc::Inst().GetCaptureOptions().AllowVSync)
  {
    SyncInterval = 0;
  }

  m_pDevice->Present(this, SyncInterval, Flags);

  return m_pReal1->Present1(SyncInterval, Flags, pPresentParameters);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGISwapChain4::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput)
{
  HRESULT ret = m_pReal2->GetRestrictToOutput(ppRestrictToOutput);

  if(SUCCEEDED(ret) && ppRestrictToOutput && *ppRestrictToOutput)
    *ppRestrictToOutput = (IDXGIOutput *)(new WrappedIDXGIOutput5(this, *ppRestrictToOutput));

  return ret;
}

WrappedIDXGIOutput5::WrappedIDXGIOutput5(RefCountDXGIObject *owner, IDXGIOutput *real)
    : RefCountDXGIObject(real), m_Owner(owner), m_pReal(real)
{
  SAFE_ADDREF(m_Owner);

  m_pReal1 = NULL;
  real->QueryInterface(__uuidof(IDXGIOutput1), (void **)&m_pReal1);
  m_pReal2 = NULL;
  real->QueryInterface(__uuidof(IDXGIOutput2), (void **)&m_pReal2);
  m_pReal3 = NULL;
  real->QueryInterface(__uuidof(IDXGIOutput3), (void **)&m_pReal3);
  m_pReal4 = NULL;
  real->QueryInterface(__uuidof(IDXGIOutput4), (void **)&m_pReal4);
  m_pReal5 = NULL;
  real->QueryInterface(__uuidof(IDXGIOutput5), (void **)&m_pReal5);
}

WrappedIDXGIOutput5::~WrappedIDXGIOutput5()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal5);
  SAFE_RELEASE(m_pReal);
  SAFE_RELEASE(m_Owner);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput5::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IDXGIOutput))
  {
    AddRef();
    *ppvObject = (IDXGIOutput *)this;
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIOutput1))
  {
    if(m_pReal1)
    {
      AddRef();
      *ppvObject = (IDXGIOutput1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIOutput2))
  {
    if(m_pReal2)
    {
      AddRef();
      *ppvObject = (IDXGIOutput2 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIOutput3))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIOutput3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIOutput4))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIOutput4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIOutput5))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIOutput5 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying IDXGIOutput for interface: %s", guid.c_str());
  }

  return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

WrappedIDXGIAdapter3::WrappedIDXGIAdapter3(IDXGIAdapter *real)
    : RefCountDXGIObject(real), m_pReal(real)
{
  m_pReal1 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter1), (void **)&m_pReal1);
  m_pReal2 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter2), (void **)&m_pReal2);
  m_pReal3 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter3), (void **)&m_pReal3);
}

WrappedIDXGIAdapter3::~WrappedIDXGIAdapter3()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIAdapter3::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IDXGIAdapter))
  {
    AddRef();
    *ppvObject = (IDXGIAdapter *)this;
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIAdapter1))
  {
    if(m_pReal1)
    {
      AddRef();
      *ppvObject = (IDXGIAdapter1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIAdapter2))
  {
    if(m_pReal2)
    {
      AddRef();
      *ppvObject = (IDXGIAdapter2 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIAdapter3))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIAdapter3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying IDXGIAdapter for interface: %s", guid.c_str());
  }

  return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

WrappedIDXGIDevice4::WrappedIDXGIDevice4(IDXGIDevice *real, ID3DDevice *d3d)
    : RefCountDXGIObject(real), m_pReal(real), m_pD3DDevice(d3d)
{
  m_pD3DDevice->AddRef();

  m_pReal1 = NULL;
  real->QueryInterface(__uuidof(IDXGIDevice1), (void **)&m_pReal1);
  m_pReal2 = NULL;
  real->QueryInterface(__uuidof(IDXGIDevice2), (void **)&m_pReal2);
  m_pReal3 = NULL;
  real->QueryInterface(__uuidof(IDXGIDevice3), (void **)&m_pReal3);
  m_pReal4 = NULL;
  real->QueryInterface(__uuidof(IDXGIDevice4), (void **)&m_pReal4);
}

WrappedIDXGIDevice4::~WrappedIDXGIDevice4()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal);
  SAFE_RELEASE(m_pD3DDevice);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice4::QueryInterface(REFIID riid, void **ppvObject)
{
  if(m_pD3DDevice->IsDeviceUUID(riid))
  {
    m_pD3DDevice->AddRef();
    *ppvObject = m_pD3DDevice->GetDeviceInterface(riid);
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIDevice))
  {
    AddRef();
    *ppvObject = (IDXGIDevice *)this;
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIDevice1))
  {
    if(m_pReal1)
    {
      AddRef();
      *ppvObject = (IDXGIDevice1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIDevice2))
  {
    if(m_pReal2)
    {
      AddRef();
      *ppvObject = (IDXGIDevice2 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIDevice3))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIDevice3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIDevice4))
  {
    if(m_pReal4)
    {
      AddRef();
      *ppvObject = (IDXGIDevice4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying IDXGIDevice for interface: %s", guid.c_str());
  }

  return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

WrappedIDXGIFactory5::WrappedIDXGIFactory5(IDXGIFactory *real)
    : RefCountDXGIObject(real), m_pReal(real)
{
  m_pReal1 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory1), (void **)&m_pReal1);
  m_pReal2 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory2), (void **)&m_pReal2);
  m_pReal3 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory3), (void **)&m_pReal3);
  m_pReal4 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory4), (void **)&m_pReal4);
  m_pReal5 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory5), (void **)&m_pReal5);
}

WrappedIDXGIFactory5::~WrappedIDXGIFactory5()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal5);
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIFactory5::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IDXGIFactory))
  {
    AddRef();
    *ppvObject = (IDXGIFactory *)this;
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIFactory1))
  {
    if(m_pReal1)
    {
      AddRef();
      *ppvObject = (IDXGIFactory1 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIFactory2))
  {
    if(m_pReal2)
    {
      AddRef();
      *ppvObject = (IDXGIFactory2 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIFactory3))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIFactory3 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIFactory4))
  {
    if(m_pReal4)
    {
      AddRef();
      *ppvObject = (IDXGIFactory4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIFactory5))
  {
    if(m_pReal5)
    {
      AddRef();
      *ppvObject = (IDXGIFactory5 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying IDXGIFactory for interface: %s", guid.c_str());
  }

  return RefCountDXGIObject::QueryInterface(riid, ppvObject);
}

HRESULT WrappedIDXGIFactory5::CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc,
                                              IDXGISwapChain **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  if(wrapDevice)
  {
    if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen && pDesc)
    {
      pDesc->Windowed = TRUE;
    }

    HRESULT ret = m_pReal->CreateSwapChain(wrapDevice->GetRealIUnknown(), pDesc, ppSwapChain);

    if(SUCCEEDED(ret))
    {
      *ppSwapChain =
          new WrappedIDXGISwapChain4(*ppSwapChain, pDesc ? pDesc->OutputWindow : NULL, wrapDevice);
    }

    return ret;
  }

  RDCERR("Creating swap chain with non-hooked device!");

  return m_pReal->CreateSwapChain(pDevice, pDesc, ppSwapChain);
}

HRESULT WrappedIDXGIFactory5::CreateSwapChainForHwnd(
    IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput,
    IDXGISwapChain1 **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  WrappedIDXGIOutput5 *wrappedOutput = (WrappedIDXGIOutput5 *)pRestrictToOutput;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(wrapDevice)
  {
    if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen && pFullscreenDesc)
    {
      pFullscreenDesc = NULL;
    }

    HRESULT ret = m_pReal2->CreateSwapChainForHwnd(wrapDevice->GetRealIUnknown(), hWnd, pDesc,
                                                   pFullscreenDesc, unwrappedOutput, ppSwapChain);

    if(SUCCEEDED(ret))
    {
      *ppSwapChain = new WrappedIDXGISwapChain4(*ppSwapChain, hWnd, wrapDevice);
    }

    return ret;
  }
  else
  {
    RDCERR("Creating swap chain with non-hooked device!");
  }

  return m_pReal2->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, unwrappedOutput,
                                          ppSwapChain);
}

HRESULT WrappedIDXGIFactory5::CreateSwapChainForCoreWindow(IUnknown *pDevice, IUnknown *pWindow,
                                                           const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                                                           IDXGIOutput *pRestrictToOutput,
                                                           IDXGISwapChain1 **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  WrappedIDXGIOutput5 *wrappedOutput = (WrappedIDXGIOutput5 *)pRestrictToOutput;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
  {
    RDCWARN("Impossible to disallow fullscreen on call to CreateSwapChainForCoreWindow");
  }

  if(wrapDevice)
  {
    HRESULT ret = m_pReal2->CreateSwapChainForCoreWindow(wrapDevice->GetRealIUnknown(), pWindow,
                                                         pDesc, unwrappedOutput, ppSwapChain);

    if(SUCCEEDED(ret))
    {
      HWND wnd = NULL;
      (*ppSwapChain)->GetHwnd(&wnd);
      if(wnd == NULL)
        wnd = (HWND)pWindow;
      *ppSwapChain = new WrappedIDXGISwapChain4(*ppSwapChain, wnd, wrapDevice);
    }

    return ret;
  }
  else
  {
    RDCERR("Creating swap chain with non-hooked device!");
  }

  return m_pReal2->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, unwrappedOutput,
                                                ppSwapChain);
}

HRESULT WrappedIDXGIFactory5::CreateSwapChainForComposition(IUnknown *pDevice,
                                                            const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                                                            IDXGIOutput *pRestrictToOutput,
                                                            IDXGISwapChain1 **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  WrappedIDXGIOutput5 *wrappedOutput = (WrappedIDXGIOutput5 *)pRestrictToOutput;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(!RenderDoc::Inst().GetCaptureOptions().AllowFullscreen)
  {
    RDCWARN("Impossible to disallow fullscreen on call to CreateSwapChainForComposition");
  }

  if(wrapDevice)
  {
    HRESULT ret = m_pReal2->CreateSwapChainForComposition(wrapDevice->GetRealIUnknown(), pDesc,
                                                          unwrappedOutput, ppSwapChain);

    if(SUCCEEDED(ret))
    {
      HWND wnd = NULL;
      (*ppSwapChain)->GetHwnd(&wnd);
      if(wnd == NULL)
        wnd = (HWND)0x1;
      *ppSwapChain = new WrappedIDXGISwapChain4(*ppSwapChain, wnd, wrapDevice);
    }

    return ret;
  }
  else
  {
    RDCERR("Creating swap chain with non-hooked device!");
  }

  return m_pReal2->CreateSwapChainForComposition(pDevice, pDesc, unwrappedOutput, ppSwapChain);
}
