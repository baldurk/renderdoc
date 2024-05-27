/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "dxgi_wrapped.h"
#include "core/core.h"
#include "serialise/serialiser.h"
#include "dxgi_common.h"

ID3D11Resource *UnwrapDXResource(void *dxObject);
IDXGIResource *UnwrapDXGIResource(void *dxgiObject);

WRAPPED_POOL_INST(WrappedIDXGIDevice4);

rdcarray<D3DDeviceCallback> WrappedIDXGISwapChain4::m_D3DCallbacks;

ID3DDevice *GetD3DDevice(IUnknown *pDevice)
{
  ID3DDevice *wrapDevice = NULL;

  if(WrappedIDXGIDevice4::IsAlloc(pDevice))
    wrapDevice = ((WrappedIDXGIDevice4 *)(IDXGIDevice3 *)pDevice)->GetD3DDevice();

  if(wrapDevice == NULL)
    wrapDevice = WrappedIDXGISwapChain4::GetD3DDevice(pDevice);

  return wrapDevice;
}

bool RefCountDXGIObject::HandleWrap(const char *ifaceName, REFIID riid, void **ppvObject)
{
  if(ppvObject == NULL || *ppvObject == NULL)
  {
    RDCWARN("HandleWrap called with NULL ppvObject querying %s", ifaceName);
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
    RDCERR("Unexpected uuid in RefCountDXGIObject::HandleWrap querying %s", ifaceName);
    return false;
  }
  else if(riid == __uuidof(IDXGIAdapter))
  {
    IDXGIAdapter *real = (IDXGIAdapter *)(*ppvObject);
    *ppvObject = (IDXGIAdapter *)(new WrappedIDXGIAdapter4(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIAdapter1))
  {
    IDXGIAdapter1 *real = (IDXGIAdapter1 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter1 *)(new WrappedIDXGIAdapter4(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIAdapter2))
  {
    IDXGIAdapter2 *real = (IDXGIAdapter2 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter2 *)(new WrappedIDXGIAdapter4(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIAdapter3))
  {
    IDXGIAdapter3 *real = (IDXGIAdapter3 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter3 *)(new WrappedIDXGIAdapter4(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIAdapter4))
  {
    IDXGIAdapter4 *real = (IDXGIAdapter4 *)(*ppvObject);
    *ppvObject = (IDXGIAdapter4 *)(new WrappedIDXGIAdapter4(real));
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
    *ppvObject = (IDXGIFactory *)(new WrappedIDXGIFactory(real));
    return true;
  }

  else if(riid == __uuidof(IDXGIDevice1))
  {
    // should have been handled elsewhere, so we can properly create this device
    RDCERR("Unexpected uuid in RefCountDXGIObject::HandleWrap querying %s", ifaceName);
    return false;
  }
  else if(riid == __uuidof(IDXGIFactory1))
  {
    IDXGIFactory1 *real = (IDXGIFactory1 *)(*ppvObject);
    *ppvObject = (IDXGIFactory1 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory2))
  {
    IDXGIFactory2 *real = (IDXGIFactory2 *)(*ppvObject);
    *ppvObject = (IDXGIFactory2 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory3))
  {
    IDXGIFactory3 *real = (IDXGIFactory3 *)(*ppvObject);
    *ppvObject = (IDXGIFactory3 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory4))
  {
    IDXGIFactory4 *real = (IDXGIFactory4 *)(*ppvObject);
    *ppvObject = (IDXGIFactory4 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory5))
  {
    IDXGIFactory5 *real = (IDXGIFactory5 *)(*ppvObject);
    *ppvObject = (IDXGIFactory5 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory6))
  {
    IDXGIFactory6 *real = (IDXGIFactory6 *)(*ppvObject);
    *ppvObject = (IDXGIFactory6 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == __uuidof(IDXGIFactory7))
  {
    IDXGIFactory7 *real = (IDXGIFactory7 *)(*ppvObject);
    *ppvObject = (IDXGIFactory7 *)(new WrappedIDXGIFactory(real));
    return true;
  }
  else if(riid == ID3D10Texture2D_uuid)
  {
    static bool printed = false;
    if(!printed)
    {
      printed = true;
      RDCWARN("Querying %s for unsupported D3D10 interface: %s", ifaceName, ToStr(riid).c_str());
    }
    return false;
  }
  else if(riid == Unknown_uuid)
  {
    static bool printed = false;
    if(!printed)
    {
      printed = true;
      RDCWARN("Querying %s for unknown GUID: %s", ifaceName, ToStr(riid).c_str());
    }
  }
  else
  {
    WarnUnknownGUID(ifaceName, riid);
  }

  return false;
}

HRESULT STDMETHODCALLTYPE RefCountDXGIObject::GetParent(
    /* [in] */ REFIID riid,
    /* [retval][out] */ void **ppParent)
{
  HRESULT ret = m_pReal->GetParent(riid, ppParent);

  if(SUCCEEDED(ret))
    HandleWrap("GetParent", riid, ppParent);

  return ret;
}

HRESULT RefCountDXGIObject::WrapQueryInterface(IUnknown *real, const char *ifaceName, REFIID riid,
                                               void **ppvObject)
{
  HRESULT ret = real->QueryInterface(riid, ppvObject);

  if(SUCCEEDED(ret))
    HandleWrap(ifaceName, riid, ppvObject);

  return ret;
}

WrappedIDXGISwapChain4::WrappedIDXGISwapChain4(IDXGISwapChain *real, HWND w, ID3DDevice *device)
    : RefCountDXGIObject(real), m_pReal(real), m_pDevice(device), m_Wnd(w)
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

  HWND wnd = GetHWND();

  if(wnd)
  {
    Keyboard::AddInputWindow(WindowingSystem::Win32, wnd);

    RenderDoc::Inst().AddFrameCapturer(DeviceOwnedWindow(m_pDevice->GetFrameCapturerDevice(), wnd),
                                       m_pDevice->GetFrameCapturer());
  }

  // we do a 'fake' present right at the start, so that we can capture frame 1, by
  // going from this fake present to the first present.
  m_pDevice->FirstFrame(this);
}

WrappedIDXGISwapChain4::~WrappedIDXGISwapChain4()
{
  HWND wnd = GetHWND();

  if(wnd)
  {
    Keyboard::RemoveInputWindow(WindowingSystem::Win32, wnd);

    RenderDoc::Inst().RemoveFrameCapturer(DeviceOwnedWindow(m_pDevice->GetFrameCapturerDevice(), wnd));
  }

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

  return RefCountDXGIObject::QueryInterface("IDXGISwapChain", riid, ppvObject);
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

  // discard swap effects only allow querying buffer 0, but only on D3D11 - on D3D12 they can (and
  // must) still query the full set of buffers.
  if(m_pDevice->GetFrameCapturer()->GetFrameCaptureDriver() == RDCDriver::D3D11 &&
     (desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD || desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD))
  {
    bufCount = 1;
  }

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

  m_LastPresentedBuffer = -1;

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGISwapChain4::GetContainingOutput(IDXGIOutput **ppOutput)
{
  HRESULT ret = m_pReal->GetContainingOutput(ppOutput);

  if(SUCCEEDED(ret) && ppOutput && *ppOutput)
    *ppOutput = (IDXGIOutput *)(new WrappedIDXGIOutput6(this, *ppOutput));

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

  m_LastPresentedBuffer = -1;

  return ret;
}

HRESULT WrappedIDXGISwapChain4::SetFullscreenState(
    /* [in] */ BOOL Fullscreen,
    /* [in] */ IDXGIOutput *pTarget)
{
  WrappedIDXGIOutput6 *wrappedOutput = (WrappedIDXGIOutput6 *)pTarget;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    return m_pReal->SetFullscreenState(Fullscreen, unwrappedOutput);

  return S_OK;
}

HRESULT WrappedIDXGISwapChain4::GetFullscreenState(
    /* [out] */ BOOL *pFullscreen,
    /* [out] */ IDXGIOutput **ppTarget)
{
  HRESULT ret = m_pReal->GetFullscreenState(pFullscreen, ppTarget);

  if(SUCCEEDED(ret) && ppTarget && *ppTarget)
    *ppTarget = (IDXGIOutput *)(new WrappedIDXGIOutput6(this, *ppTarget));

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

  if(riid == ID3D10Texture2D_uuid || riid == ID3D10Resource_uuid)
  {
    RDCERR("Querying swapchain buffers via D3D10 interface UUIDs is not supported");
    return E_NOINTERFACE;
  }

  IUnknown *unwrappedBackbuffer = NULL;

  // query as the device's native backbuffer format
  HRESULT ret =
      m_pReal->GetBuffer(Buffer, m_pDevice->GetBackbufferUUID(), (void **)&unwrappedBackbuffer);

  // if this fails we can't continue, the wrapping below assumes it's a native resource
  if(FAILED(ret))
  {
    RDCERR("Failed to get swapchain backbuffer %d as %s: HRESULT: %s", Buffer,
           ToStr(m_pDevice->GetBackbufferUUID()).c_str(), ToStr(ret).c_str());
    SAFE_RELEASE(unwrappedBackbuffer);
    return ret;
  }

  DXGI_SWAP_CHAIN_DESC desc;
  GetDesc(&desc);

  // this wraps and takes the reference, so unwrappedBackbuffer no longer holds a reference
  IUnknown *wrappedBackbuffer =
      m_pDevice->WrapSwapchainBuffer(this, desc.BufferDesc.Format, Buffer, unwrappedBackbuffer);
  unwrappedBackbuffer = NULL;

  // now query the original UUID the user wanted
  ret = wrappedBackbuffer->QueryInterface(riid, ppSurface);

  if(FAILED(ret) || *ppSurface == NULL)
  {
    RDCERR("Couldn't convert wrapped swapchain backbuffer %d to %s: HRESULT: %s", Buffer,
           ToStr(riid).c_str(), ToStr(ret).c_str());
    SAFE_RELEASE(wrappedBackbuffer);
    return ret;
  }

  // now the reference is in ppSurface
  SAFE_RELEASE(wrappedBackbuffer);

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
    else if(riid == __uuidof(IDXGIDevice) || riid == __uuidof(IDXGIDevice1) ||
            riid == __uuidof(IDXGIDevice2) || riid == __uuidof(IDXGIDevice3) ||
            riid == __uuidof(IDXGIDevice4))
    {
      return m_pDevice->QueryInterface(riid, ppDevice);
    }
    else if(!HandleWrap("GetDevice", riid, ppDevice))
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
  if(!RenderDoc::Inst().GetCaptureOptions().allowVSync)
  {
    SyncInterval = 0;
  }

  if((Flags & DXGI_PRESENT_TEST) == 0)
  {
    TickLastPresentedBuffer();
    m_pDevice->Present(this, SyncInterval, Flags);
  }

  return m_pReal->Present(SyncInterval, Flags);
}

HRESULT WrappedIDXGISwapChain4::Present1(UINT SyncInterval, UINT Flags,
                                         const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
  if(!RenderDoc::Inst().GetCaptureOptions().allowVSync)
  {
    SyncInterval = 0;
  }

  if((Flags & DXGI_PRESENT_TEST) == 0)
  {
    TickLastPresentedBuffer();
    m_pDevice->Present(this, SyncInterval, Flags);
  }

  return m_pReal1->Present1(SyncInterval, Flags, pPresentParameters);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGISwapChain4::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput)
{
  HRESULT ret = m_pReal1->GetRestrictToOutput(ppRestrictToOutput);

  if(SUCCEEDED(ret) && ppRestrictToOutput && *ppRestrictToOutput)
    *ppRestrictToOutput = (IDXGIOutput *)(new WrappedIDXGIOutput6(this, *ppRestrictToOutput));

  return ret;
}

WrappedIDXGIOutput6::WrappedIDXGIOutput6(RefCountDXGIObject *owner, IDXGIOutput *real)
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
  m_pReal6 = NULL;
  real->QueryInterface(__uuidof(IDXGIOutput6), (void **)&m_pReal6);
}

WrappedIDXGIOutput6::~WrappedIDXGIOutput6()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal5);
  SAFE_RELEASE(m_pReal6);
  SAFE_RELEASE(m_pReal);
  SAFE_RELEASE(m_Owner);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::FindClosestMatchingMode(
    const DXGI_MODE_DESC *pModeToMatch, DXGI_MODE_DESC *pClosestMatch, IUnknown *pConcernedDevice)
{
  if(pConcernedDevice)
  {
    ID3DDevice *wrapDevice = GetD3DDevice(pConcernedDevice);

    if(wrapDevice)
      return m_pReal->FindClosestMatchingMode(pModeToMatch, pClosestMatch,
                                              wrapDevice->GetRealIUnknown());

    RDCERR("Unrecognised device in FindClosestMatchingMode()");

    return E_INVALIDARG;
  }

  return m_pReal->FindClosestMatchingMode(pModeToMatch, pClosestMatch, NULL);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::TakeOwnership(IUnknown *pDevice, BOOL Exclusive)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  if(wrapDevice)
    return m_pReal->TakeOwnership(wrapDevice->GetRealIUnknown(), Exclusive);

  // since this is supposed to be an internal function, allow passing through the pointer directly
  // just in case
  return m_pReal->TakeOwnership(pDevice, Exclusive);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::FindClosestMatchingMode1(
    const DXGI_MODE_DESC1 *pModeToMatch, DXGI_MODE_DESC1 *pClosestMatch, IUnknown *pConcernedDevice)
{
  if(pConcernedDevice)
  {
    ID3DDevice *wrapDevice = GetD3DDevice(pConcernedDevice);

    if(wrapDevice)
      return m_pReal1->FindClosestMatchingMode1(pModeToMatch, pClosestMatch,
                                                wrapDevice->GetRealIUnknown());

    RDCERR("Unrecognised device in FindClosestMatchingMode1()");

    return E_INVALIDARG;
  }

  return m_pReal1->FindClosestMatchingMode1(pModeToMatch, pClosestMatch, NULL);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::GetDisplaySurfaceData1(IDXGIResource *pDestination)
{
  return m_pReal1->GetDisplaySurfaceData1(UnwrapDXGIResource(pDestination));
}

HRESULT STDMETHODCALLTYPE
WrappedIDXGIOutput6::DuplicateOutput(IUnknown *pDevice, IDXGIOutputDuplication **ppOutputDuplication)
{
  if(!ppOutputDuplication)
    return E_INVALIDARG;

  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  if(wrapDevice)
  {
    IDXGIOutputDuplication *dup = NULL;
    HRESULT ret = m_pReal1->DuplicateOutput(wrapDevice->GetRealIUnknown(), &dup);

    if(SUCCEEDED(ret) && dup)
      dup = new WrappedIDXGIOutputDuplication(wrapDevice, dup);

    *ppOutputDuplication = dup;

    return ret;
  }

  if(pDevice)
    RDCERR("Unrecognised device in DuplicateOutput()");

  return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::CheckOverlaySupport(DXGI_FORMAT EnumFormat,
                                                                   IUnknown *pConcernedDevice,
                                                                   UINT *pFlags)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pConcernedDevice);

  if(wrapDevice)
    return m_pReal3->CheckOverlaySupport(EnumFormat, wrapDevice->GetRealIUnknown(), pFlags);

  if(pConcernedDevice)
    RDCERR("Unrecognised device in CheckOverlaySupport()");

  return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::DuplicateOutput1(
    IUnknown *pDevice, UINT Flags, UINT SupportedFormatsCount, const DXGI_FORMAT *pSupportedFormats,
    IDXGIOutputDuplication **ppOutputDuplication)
{
  if(!ppOutputDuplication)
    return E_INVALIDARG;

  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  if(wrapDevice)
  {
    IDXGIOutputDuplication *dup = NULL;
    HRESULT ret = m_pReal5->DuplicateOutput1(wrapDevice->GetRealIUnknown(), Flags,
                                             SupportedFormatsCount, pSupportedFormats, &dup);

    if(SUCCEEDED(ret) && dup)
      dup = new WrappedIDXGIOutputDuplication(wrapDevice, dup);

    *ppOutputDuplication = dup;

    return ret;
  }

  if(pDevice)
    RDCERR("Unrecognised device in DuplicateOutput()");

  return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutput6::QueryInterface(REFIID riid, void **ppvObject)
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
    if(m_pReal4)
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
    if(m_pReal5)
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
  else if(riid == __uuidof(IDXGIOutput6))
  {
    if(m_pReal6)
    {
      AddRef();
      *ppvObject = (IDXGIOutput6 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }

  return RefCountDXGIObject::QueryInterface("IDXGIOutput", riid, ppvObject);
}

WrappedIDXGIAdapter4::WrappedIDXGIAdapter4(IDXGIAdapter *real)
    : RefCountDXGIObject(real), m_pReal(real)
{
  m_pReal1 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter1), (void **)&m_pReal1);
  m_pReal2 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter2), (void **)&m_pReal2);
  m_pReal3 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter3), (void **)&m_pReal3);
  m_pReal4 = NULL;
  real->QueryInterface(__uuidof(IDXGIAdapter4), (void **)&m_pReal4);
}

WrappedIDXGIAdapter4::~WrappedIDXGIAdapter4()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIAdapter4::QueryInterface(REFIID riid, void **ppvObject)
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
  else if(riid == __uuidof(IDXGIAdapter4))
  {
    if(m_pReal3)
    {
      AddRef();
      *ppvObject = (IDXGIAdapter4 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }

  return RefCountDXGIObject::QueryInterface("IDXGIAdapter", riid, ppvObject);
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
  else if(riid == __uuidof(ID3D11Multithread))
  {
    // forward to the device as the lock is shared amongst all things
    return m_pD3DDevice->QueryInterface(riid, ppvObject);
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

  return RefCountDXGIObject::QueryInterface("IDXGIDevice", riid, ppvObject);
}

rdcarray<IDXGIResource *> UnwrapResourceSet(UINT NumResources, IDXGIResource *const *ppResources)
{
  rdcarray<IDXGIResource *> resources;
  resources.resize(NumResources);
  for(UINT i = 0; i < NumResources; i++)
  {
    WrappedDXGIInterface<WrappedIDXGIOutput6> *wrapped =
        (WrappedDXGIInterface<WrappedIDXGIOutput6> *)ppResources[i];
    resources[i] = UnwrapDXGIResource(wrapped->GetWrapped());
    if(resources[i] == NULL)
    {
      RDCERR("Unrecognised resource %p!", ppResources[i]);
      resources[i] = ppResources[i];
    }
  }
  return resources;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice4::OfferResources(UINT NumResources,
                                                              IDXGIResource *const *ppResources,
                                                              DXGI_OFFER_RESOURCE_PRIORITY Priority)
{
  rdcarray<IDXGIResource *> resources = UnwrapResourceSet(NumResources, ppResources);
  return m_pReal2->OfferResources(NumResources, resources.data(), Priority);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice4::ReclaimResources(UINT NumResources,
                                                                IDXGIResource *const *ppResources,
                                                                BOOL *pDiscarded)
{
  rdcarray<IDXGIResource *> resources = UnwrapResourceSet(NumResources, ppResources);
  return m_pReal2->ReclaimResources(NumResources, resources.data(), pDiscarded);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice4::OfferResources1(UINT NumResources,
                                                               IDXGIResource *const *ppResources,
                                                               DXGI_OFFER_RESOURCE_PRIORITY Priority,
                                                               UINT Flags)
{
  rdcarray<IDXGIResource *> resources = UnwrapResourceSet(NumResources, ppResources);
  return m_pReal4->OfferResources1(NumResources, resources.data(), Priority, Flags);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIDevice4::ReclaimResources1(UINT NumResources,
                                                                 IDXGIResource *const *ppResources,

                                                                 DXGI_RECLAIM_RESOURCE_RESULTS *pResults)
{
  rdcarray<IDXGIResource *> resources = UnwrapResourceSet(NumResources, ppResources);
  return m_pReal4->ReclaimResources1(NumResources, resources.data(), pResults);
}

WrappedIDXGIFactory::WrappedIDXGIFactory(IDXGIFactory *real)
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
  m_pReal6 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory6), (void **)&m_pReal6);
  m_pReal7 = NULL;
  real->QueryInterface(__uuidof(IDXGIFactory7), (void **)&m_pReal7);
}

WrappedIDXGIFactory::~WrappedIDXGIFactory()
{
  SAFE_RELEASE(m_pReal1);
  SAFE_RELEASE(m_pReal2);
  SAFE_RELEASE(m_pReal3);
  SAFE_RELEASE(m_pReal4);
  SAFE_RELEASE(m_pReal5);
  SAFE_RELEASE(m_pReal6);
  SAFE_RELEASE(m_pReal7);
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIFactory::QueryInterface(REFIID riid, void **ppvObject)
{
  // {713f394e-92ca-47e7-ab81-1159c2791e54}
  static const GUID IDXGIFactoryDWM_uuid = {
      0x713f394e, 0x92ca, 0x47e7, {0xab, 0x81, 0x11, 0x59, 0xc2, 0x79, 0x1e, 0x54}};

  // {1ddd77aa-9a4a-4cc8-9e55-98c196bafc8f}
  static const GUID IDXGIFactoryDWM8_uuid = {
      0x1ddd77aa, 0x9a4a, 0x4cc8, {0x9e, 0x55, 0x98, 0xc1, 0x96, 0xba, 0xfc, 0x8f}};

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
  else if(riid == __uuidof(IDXGIFactory6))
  {
    if(m_pReal6)
    {
      AddRef();
      *ppvObject = (IDXGIFactory6 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(IDXGIFactory7))
  {
    if(m_pReal7)
    {
      AddRef();
      *ppvObject = (IDXGIFactory7 *)this;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == IDXGIFactoryDWM_uuid)
  {
    RDCWARN("Blocking QueryInterface for IDXGIFactoryDWM");
    return E_NOINTERFACE;
  }
  else if(riid == IDXGIFactoryDWM8_uuid)
  {
    RDCWARN("Blocking QueryInterface for IDXGIFactoryDWM8");
    return E_NOINTERFACE;
  }

  return RefCountDXGIObject::QueryInterface("IDXGIFactory", riid, ppvObject);
}

HRESULT WrappedIDXGIFactory::CreateSwapChain(IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc,
                                             IDXGISwapChain **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  if(wrapDevice)
  {
    DXGI_SWAP_CHAIN_DESC local = {};
    DXGI_SWAP_CHAIN_DESC *desc = NULL;

    if(pDesc)
    {
      local = *pDesc;
      desc = &local;
    }

    local.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;

    if(!RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
      local.Windowed = TRUE;

    HRESULT ret = m_pReal->CreateSwapChain(wrapDevice->GetRealIUnknown(), desc, ppSwapChain);

    if(SUCCEEDED(ret))
    {
      *ppSwapChain =
          new WrappedIDXGISwapChain4(*ppSwapChain, desc ? desc->OutputWindow : NULL, wrapDevice);
    }

    return ret;
  }

  RDCERR("Creating swap chain with non-hooked device!");

  return m_pReal->CreateSwapChain(pDevice, pDesc, ppSwapChain);
}

HRESULT WrappedIDXGIFactory::CreateSwapChainForHwnd(
    IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput,
    IDXGISwapChain1 **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  WrappedIDXGIOutput6 *wrappedOutput = (WrappedIDXGIOutput6 *)pRestrictToOutput;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(wrapDevice)
  {
    DXGI_SWAP_CHAIN_DESC1 local;
    DXGI_SWAP_CHAIN_DESC1 *desc = NULL;

    if(pDesc)
    {
      local = *pDesc;
      desc = &local;
    }

    local.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;

    if(!RenderDoc::Inst().GetCaptureOptions().allowFullscreen && pFullscreenDesc)
    {
      pFullscreenDesc = NULL;
    }

    HRESULT ret = m_pReal2->CreateSwapChainForHwnd(wrapDevice->GetRealIUnknown(), hWnd, desc,
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

HRESULT WrappedIDXGIFactory::CreateSwapChainForCoreWindow(IUnknown *pDevice, IUnknown *pWindow,
                                                          const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                                                          IDXGIOutput *pRestrictToOutput,
                                                          IDXGISwapChain1 **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  WrappedIDXGIOutput6 *wrappedOutput = (WrappedIDXGIOutput6 *)pRestrictToOutput;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(!RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
  {
    RDCWARN("Impossible to disallow fullscreen on call to CreateSwapChainForCoreWindow");
  }

  if(wrapDevice)
  {
    DXGI_SWAP_CHAIN_DESC1 local = {};
    DXGI_SWAP_CHAIN_DESC1 *desc = NULL;

    if(pDesc)
    {
      local = *pDesc;
      desc = &local;
    }

    local.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;

    HRESULT ret = m_pReal2->CreateSwapChainForCoreWindow(wrapDevice->GetRealIUnknown(), pWindow,
                                                         desc, unwrappedOutput, ppSwapChain);

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

HRESULT WrappedIDXGIFactory::CreateSwapChainForComposition(IUnknown *pDevice,
                                                           const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                                                           IDXGIOutput *pRestrictToOutput,
                                                           IDXGISwapChain1 **ppSwapChain)
{
  ID3DDevice *wrapDevice = GetD3DDevice(pDevice);

  WrappedIDXGIOutput6 *wrappedOutput = (WrappedIDXGIOutput6 *)pRestrictToOutput;
  IDXGIOutput *unwrappedOutput = wrappedOutput ? wrappedOutput->GetReal() : NULL;

  if(!RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
  {
    RDCWARN("Impossible to disallow fullscreen on call to CreateSwapChainForComposition");
  }

  if(wrapDevice)
  {
    DXGI_SWAP_CHAIN_DESC1 local = {};
    DXGI_SWAP_CHAIN_DESC1 *desc = NULL;

    if(pDesc)
    {
      local = *pDesc;
      desc = &local;
    }

    local.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;

    HRESULT ret = m_pReal2->CreateSwapChainForComposition(wrapDevice->GetRealIUnknown(), desc,
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

WrappedIDXGIOutputDuplication::WrappedIDXGIOutputDuplication(ID3DDevice *device,
                                                             IDXGIOutputDuplication *real)
    : RefCountDXGIObject(real), m_Device(device), m_pReal(real)
{
}

WrappedIDXGIOutputDuplication::~WrappedIDXGIOutputDuplication()
{
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutputDuplication::AcquireNextFrame(
    UINT TimeoutInMilliseconds, DXGI_OUTDUPL_FRAME_INFO *pFrameInfo, IDXGIResource **ppDesktopResource)
{
  if(!ppDesktopResource)
    return E_INVALIDARG;

  IDXGIResource *desktop = NULL;
  HRESULT ret = m_pReal->AcquireNextFrame(TimeoutInMilliseconds, pFrameInfo, &desktop);

  if(SUCCEEDED(ret) && desktop)
    desktop = m_Device->WrapExternalDXGIResource(desktop);

  *ppDesktopResource = desktop;

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedIDXGIOutputDuplication::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IDXGIOutputDuplication))
  {
    AddRef();
    *ppvObject = (IDXGIOutputDuplication *)this;
    return S_OK;
  }

  return RefCountDXGIObject::QueryInterface("IDXGIOutputDuplication", riid, ppvObject);
}
