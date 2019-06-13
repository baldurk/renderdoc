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

#pragma once

#include "common/common.h"
#include "common/wrapped_pool.h"
#include "driver/dx/official/dxgi1_6.h"
#include "driver/dx/official/dxgidebug.h"

MIDL_INTERFACE("6f15aaf2-d208-4e89-9ab4-489535d34f9c") ID3D11Texture2D;
MIDL_INTERFACE("51218251-1E33-4617-9CCB-4D3A4367E7BB") ID3D11Texture2D1;
MIDL_INTERFACE("dc8e63f3-d12b-4952-b47b-5e45026a862d") ID3D11Resource;
MIDL_INTERFACE("db6f6ddb-ac77-4e88-8253-819df9bbf140") ID3D11Device;
MIDL_INTERFACE("696442be-a72e-4059-bc79-5b5c98040fad") ID3D12Resource;
MIDL_INTERFACE("9D5E227A-4430-4161-88B3-3ECA6BB16E19") ID3D12Resource1;
MIDL_INTERFACE("189819f1-1db6-4b57-be54-1821339b85f7") ID3D12Device;
MIDL_INTERFACE("9B7E4E00-342C-4106-A19F-4F2704F689F0") ID3D11Multithread;

class RefCountDXGIObject : public IDXGIObject
{
  IDXGIObject *m_pReal;
  unsigned int m_iRefcount;

public:
  RefCountDXGIObject(IDXGIObject *real) : m_pReal(real), m_iRefcount(1) {}
  virtual ~RefCountDXGIObject() {}
  static bool HandleWrap(REFIID riid, void **ppvObject);
  static HRESULT WrapQueryInterface(IUnknown *real, REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(
      /* [in] */ REFIID riid,
      /* [annotation][iid_is][out] */
      __RPC__deref_out void **ppvObject)
  {
    if(riid == __uuidof(IUnknown))
    {
      AddRef();
      *ppvObject = (IUnknown *)(IDXGIObject *)this;
      return S_OK;
    }

    return WrapQueryInterface(m_pReal, riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef()
  {
    InterlockedIncrement(&m_iRefcount);
    return m_iRefcount;
  }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = InterlockedDecrement(&m_iRefcount);
    if(ret == 0)
      delete this;
    return ret;
  }

  //////////////////////////////
  // implement IDXGIObject

  virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
      /* [in] */ REFGUID Name,
      /* [in] */ UINT DataSize,
      /* [in] */ const void *pData)
  {
    return m_pReal->SetPrivateData(Name, DataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      /* [in] */ REFGUID Name,
      /* [in] */ const IUnknown *pUnknown)
  {
    return m_pReal->SetPrivateDataInterface(Name, pUnknown);
  }

  virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
      /* [in] */ REFGUID Name,
      /* [out][in] */ UINT *pDataSize,
      /* [out] */ void *pData)
  {
    return m_pReal->GetPrivateData(Name, pDataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE GetParent(
      /* [in] */ REFIID riid,
      /* [retval][out] */ void **ppParent);
};

#define IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT                                      \
  ULONG STDMETHODCALLTYPE AddRef() { return RefCountDXGIObject::AddRef(); }                \
  ULONG STDMETHODCALLTYPE Release() { return RefCountDXGIObject::Release(); }              \
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)                  \
  {                                                                                        \
    return RefCountDXGIObject::QueryInterface(riid, ppvObject);                            \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFIID Name, UINT DataSize, const void *pData)  \
  {                                                                                        \
    return RefCountDXGIObject::SetPrivateData(Name, DataSize, pData);                      \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFIID Name, const IUnknown *pUnknown) \
  {                                                                                        \
    return RefCountDXGIObject::SetPrivateDataInterface(Name, pUnknown);                    \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFIID Name, UINT *pDataSize, void *pData)      \
  {                                                                                        \
    return RefCountDXGIObject::GetPrivateData(Name, pDataSize, pData);                     \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppvObject)                       \
  {                                                                                        \
    return RefCountDXGIObject::GetParent(riid, ppvObject);                                 \
  }

#define IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY                          \
  ULONG STDMETHODCALLTYPE AddRef() { return RefCountDXGIObject::AddRef(); }                \
  ULONG STDMETHODCALLTYPE Release() { return RefCountDXGIObject::Release(); }              \
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFIID Name, UINT DataSize, const void *pData)  \
  {                                                                                        \
    return RefCountDXGIObject::SetPrivateData(Name, DataSize, pData);                      \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFIID Name, const IUnknown *pUnknown) \
  {                                                                                        \
    return RefCountDXGIObject::SetPrivateDataInterface(Name, pUnknown);                    \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFIID Name, UINT *pDataSize, void *pData)      \
  {                                                                                        \
    return RefCountDXGIObject::GetPrivateData(Name, pDataSize, pData);                     \
  }                                                                                        \
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppvObject)                       \
  {                                                                                        \
    return RefCountDXGIObject::GetParent(riid, ppvObject);                                 \
  }

class WrappedIDXGISwapChain4;

struct ID3DDevice
{
  // re-use IUnknown
  virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
  virtual ULONG STDMETHODCALLTYPE Release() = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) = 0;

  virtual IUnknown *GetRealIUnknown() = 0;

  virtual IID GetBackbufferUUID() = 0;

  virtual bool IsDeviceUUID(REFIID guid) = 0;
  virtual IUnknown *GetDeviceInterface(REFIID guid) = 0;

  virtual void FirstFrame(WrappedIDXGISwapChain4 *swapChain) = 0;

  virtual void NewSwapchainBuffer(IUnknown *backbuffer) = 0;
  virtual void ReleaseSwapchainResources(WrappedIDXGISwapChain4 *swapChain, UINT QueueCount,
                                         IUnknown *const *ppPresentQueue,
                                         IUnknown **unwrappedQueues) = 0;
  virtual IUnknown *WrapSwapchainBuffer(WrappedIDXGISwapChain4 *swap, DXGI_SWAP_CHAIN_DESC *swapDesc,
                                        UINT buffer, IUnknown *realSurface) = 0;

  virtual HRESULT Present(WrappedIDXGISwapChain4 *swapChain, UINT SyncInterval, UINT Flags) = 0;
};

typedef ID3DDevice *(*D3DDeviceCallback)(IUnknown *dev);

template <typename NestedType>
class WrappedDXGIInterface : public RefCountDXGIObject,
                             public IDXGIKeyedMutex,
                             public IDXGISurface2,
                             public IDXGIResource1
{
public:
  ID3DDevice *m_pDevice;
  NestedType *m_pWrapped;

  WrappedDXGIInterface(NestedType *wrapped, ID3DDevice *device)
      : RefCountDXGIObject(NULL), m_pDevice(device), m_pWrapped(wrapped)
  {
    m_pWrapped->AddRef();
    m_pDevice->AddRef();
  }

  virtual ~WrappedDXGIInterface()
  {
    m_pWrapped->Release();
    m_pDevice->Release();
  }

  NestedType *GetWrapped() { return m_pWrapped; }
  //////////////////////////////
  // Implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return RefCountDXGIObject::AddRef(); }
  ULONG STDMETHODCALLTYPE Release() { return RefCountDXGIObject::Release(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    // ensure the real object has this interface
    void *outObj;
    HRESULT hr = m_pWrapped->QueryInterface(riid, &outObj);

    IUnknown *unk = (IUnknown *)outObj;
    SAFE_RELEASE(unk);

    if(FAILED(hr))
    {
      return hr;
    }

    if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (IUnknown *)(IDXGIKeyedMutex *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGIObject))
    {
      *ppvObject = (IDXGIObject *)(IDXGIKeyedMutex *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGIDeviceSubObject))
    {
      *ppvObject = (IDXGIDeviceSubObject *)(IDXGIKeyedMutex *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGIResource))
    {
      *ppvObject = (IDXGIResource *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGIKeyedMutex))
    {
      *ppvObject = (IDXGIKeyedMutex *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGISurface))
    {
      *ppvObject = (IDXGISurface *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGISurface1))
    {
      *ppvObject = (IDXGISurface1 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGIResource1))
    {
      *ppvObject = (IDXGIResource1 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(IDXGISurface2))
    {
      *ppvObject = (IDXGISurface2 *)this;
      AddRef();
      return S_OK;
    }

    return m_pWrapped->QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // Implement IDXGIObject
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
  {
    return m_pWrapped->SetPrivateData(Name, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
  {
    return m_pWrapped->SetPrivateDataInterface(Name, pUnknown);
  }

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
  {
    return m_pWrapped->GetPrivateData(Name, pDataSize, pData);
  }

  // this should only be called for adapters, devices, factories etc
  // so we pass it onto the device
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent)
  {
    return m_pDevice->QueryInterface(riid, ppParent);
  }

  //////////////////////////////
  // Implement IDXGIDeviceSubObject

  // same as GetParent
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice)
  {
    return m_pDevice->QueryInterface(riid, ppDevice);
  }

  //////////////////////////////
  // Implement IDXGIKeyedMutex
  HRESULT STDMETHODCALLTYPE AcquireSync(UINT64 Key, DWORD dwMilliseconds)
  {
    // temporarily get the real interface
    IDXGIKeyedMutex *mutex = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&mutex);
    if(FAILED(hr))
    {
      SAFE_RELEASE(mutex);
      return hr;
    }

    hr = mutex->AcquireSync(Key, dwMilliseconds);
    SAFE_RELEASE(mutex);
    return hr;
  }

  HRESULT STDMETHODCALLTYPE ReleaseSync(UINT64 Key)
  {
    // temporarily get the real interface
    IDXGIKeyedMutex *mutex = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&mutex);
    if(FAILED(hr))
    {
      SAFE_RELEASE(mutex);
      return hr;
    }

    hr = mutex->ReleaseSync(Key);
    SAFE_RELEASE(mutex);
    return hr;
  }

  //////////////////////////////
  // Implement IDXGIResource
  virtual HRESULT STDMETHODCALLTYPE GetSharedHandle(HANDLE *pSharedHandle)
  {
    // temporarily get the real interface
    IDXGIResource *res = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
    if(FAILED(hr))
    {
      SAFE_RELEASE(res);
      return hr;
    }

    hr = res->GetSharedHandle(pSharedHandle);
    SAFE_RELEASE(res);
    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE GetUsage(DXGI_USAGE *pUsage)
  {
    // temporarily get the real interface
    IDXGIResource *res = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
    if(FAILED(hr))
    {
      SAFE_RELEASE(res);
      return hr;
    }

    hr = res->GetUsage(pUsage);
    SAFE_RELEASE(res);
    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority)
  {
    // temporarily get the real interface
    IDXGIResource *res = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
    if(FAILED(hr))
    {
      SAFE_RELEASE(res);
      return hr;
    }

    hr = res->SetEvictionPriority(EvictionPriority);
    SAFE_RELEASE(res);
    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE GetEvictionPriority(UINT *pEvictionPriority)
  {
    // temporarily get the real interface
    IDXGIResource *res = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource), (void **)&res);
    if(FAILED(hr))
    {
      SAFE_RELEASE(res);
      return hr;
    }

    hr = res->GetEvictionPriority(pEvictionPriority);
    SAFE_RELEASE(res);
    return hr;
  }

  //////////////////////////////
  // Implement IDXGISurface
  virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SURFACE_DESC *pDesc)
  {
    // temporarily get the real interface
    IDXGISurface *surf = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
    if(FAILED(hr))
    {
      SAFE_RELEASE(surf);
      return hr;
    }

    hr = surf->GetDesc(pDesc);
    SAFE_RELEASE(surf);
    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE Map(DXGI_MAPPED_RECT *pLockedRect, UINT MapFlags)
  {
    // temporarily get the real interface
    IDXGISurface *surf = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
    if(FAILED(hr))
    {
      SAFE_RELEASE(surf);
      return hr;
    }

    hr = surf->Map(pLockedRect, MapFlags);
    SAFE_RELEASE(surf);
    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE Unmap(void)
  {
    // temporarily get the real interface
    IDXGISurface *surf = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface), (void **)&surf);
    if(FAILED(hr))
    {
      SAFE_RELEASE(surf);
      return hr;
    }

    hr = surf->Unmap();
    SAFE_RELEASE(surf);
    return hr;
  }

  //////////////////////////////
  // Implement IDXGISurface1
  virtual HRESULT STDMETHODCALLTYPE GetDC(BOOL Discard, HDC *phdc)
  {
    // temporarily get the real interface
    IDXGISurface1 *surf = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface1), (void **)&surf);
    if(FAILED(hr))
    {
      SAFE_RELEASE(surf);
      return hr;
    }

    hr = surf->GetDC(Discard, phdc);
    SAFE_RELEASE(surf);
    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE ReleaseDC(RECT *pDirtyRect)
  {
    // temporarily get the real interface
    IDXGISurface1 *surf = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGISurface1), (void **)&surf);
    if(FAILED(hr))
    {
      SAFE_RELEASE(surf);
      return hr;
    }

    hr = surf->ReleaseDC(pDirtyRect);
    SAFE_RELEASE(surf);
    return hr;
  }

  //////////////////////////////
  // Implement IDXGIResource1
  virtual HRESULT STDMETHODCALLTYPE CreateSubresourceSurface(UINT index, IDXGISurface2 **ppSurface)
  {
    if(ppSurface == NULL)
      return E_INVALIDARG;

    // maybe this will work?!?
    AddRef();
    *ppSurface = (IDXGISurface2 *)this;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *pAttributes,
                                                       DWORD dwAccess, LPCWSTR lpName,
                                                       HANDLE *pHandle)
  {
    // temporarily get the real interface
    IDXGIResource1 *res = NULL;
    HRESULT hr = m_pWrapped->GetReal()->QueryInterface(__uuidof(IDXGIResource1), (void **)&res);
    if(FAILED(hr))
    {
      SAFE_RELEASE(res);
      return hr;
    }

    hr = res->CreateSharedHandle(pAttributes, dwAccess, lpName, pHandle);
    SAFE_RELEASE(res);
    return hr;
  }

  //////////////////////////////
  // Implement IDXGISurface2
  virtual HRESULT STDMETHODCALLTYPE GetResource(REFIID riid, void **ppParentResource,
                                                UINT *pSubresourceIndex)
  {
    // not really sure how to implement this :(.
    if(pSubresourceIndex)
      pSubresourceIndex = 0;
    return QueryInterface(riid, ppParentResource);
  }
};

class WrappedIDXGISwapChain4 : public IDXGISwapChain4, public RefCountDXGIObject
{
  IDXGISwapChain *m_pReal;
  IDXGISwapChain1 *m_pReal1;
  IDXGISwapChain2 *m_pReal2;
  IDXGISwapChain3 *m_pReal3;
  IDXGISwapChain4 *m_pReal4;
  ID3DDevice *m_pDevice;

  static std::vector<D3DDeviceCallback> m_D3DCallbacks;

  HWND m_Wnd;

  static const int MAX_NUM_BACKBUFFERS = 8;

  IUnknown *m_pBackBuffers[MAX_NUM_BACKBUFFERS];

  void ReleaseBuffersForResize(UINT QueueCount, IUnknown *const *ppPresentQueue,
                               IUnknown **unwrappedQueues);
  void WrapBuffersAfterResize();

public:
  WrappedIDXGISwapChain4(IDXGISwapChain *real, HWND wnd, ID3DDevice *device);
  virtual ~WrappedIDXGISwapChain4();

  static void RegisterD3DDeviceCallback(D3DDeviceCallback callback)
  {
    if(std::find(m_D3DCallbacks.begin(), m_D3DCallbacks.end(), callback) == m_D3DCallbacks.end())
      m_D3DCallbacks.push_back(callback);
  }

  static ID3DDevice *GetD3DDevice(IUnknown *dev)
  {
    for(size_t i = 0; i < m_D3DCallbacks.size(); i++)
    {
      ID3DDevice *d3d = m_D3DCallbacks[i](dev);
      if(d3d)
        return d3d;
    }

    return NULL;
  }

  ID3DDevice *GetD3DDevice() { return m_pDevice; }
  int GetNumBackbuffers() { return MAX_NUM_BACKBUFFERS; }
  IUnknown **GetBackbuffers() { return m_pBackBuffers; }
  IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  DXGI_SWAP_CHAIN_DESC GetDescWithHWND()
  {
    DXGI_SWAP_CHAIN_DESC ret = {};

    m_pReal->GetDesc(&ret);

    if(ret.OutputWindow == NULL)
      ret.OutputWindow = m_Wnd;

    return ret;
  }

  //////////////////////////////
  // implement IDXGIDeviceSubObject

  virtual HRESULT STDMETHODCALLTYPE GetDevice(
      /* [in] */ REFIID riid,
      /* [retval][out] */ void **ppDevice);

  //////////////////////////////
  // implement IDXGISwapChain

  virtual HRESULT STDMETHODCALLTYPE Present(
      /* [in] */ UINT SyncInterval,
      /* [in] */ UINT Flags);

  virtual HRESULT STDMETHODCALLTYPE GetBuffer(
      /* [in] */ UINT Buffer,
      /* [in] */ REFIID riid,
      /* [out][in] */ void **ppSurface);

  virtual HRESULT STDMETHODCALLTYPE SetFullscreenState(
      /* [in] */ BOOL Fullscreen,
      /* [in] */ IDXGIOutput *pTarget);

  virtual HRESULT STDMETHODCALLTYPE GetFullscreenState(
      /* [out] */ BOOL *pFullscreen,
      /* [out] */ IDXGIOutput **ppTarget);

  virtual HRESULT STDMETHODCALLTYPE GetDesc(
      /* [out] */ DXGI_SWAP_CHAIN_DESC *pDesc)
  {
    return m_pReal->GetDesc(pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE ResizeBuffers(
      /* [in] */ UINT BufferCount,
      /* [in] */ UINT Width,
      /* [in] */ UINT Height,
      /* [in] */ DXGI_FORMAT NewFormat,
      /* [in] */ UINT SwapChainFlags);

  virtual HRESULT STDMETHODCALLTYPE ResizeTarget(
      /* [in] */ const DXGI_MODE_DESC *pNewTargetParameters)
  {
    return m_pReal->ResizeTarget(pNewTargetParameters);
  }

  virtual HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **ppOutput);

  virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(
      /* [out] */ DXGI_FRAME_STATISTICS *pStats)
  {
    return m_pReal->GetFrameStatistics(pStats);
  }

  virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount(
      /* [out] */ UINT *pLastPresentCount)
  {
    return m_pReal->GetLastPresentCount(pLastPresentCount);
  }

  //////////////////////////////
  // implement IDXGISwapChain1

  virtual HRESULT STDMETHODCALLTYPE GetDesc1(
      /* [annotation][out] */
      _Out_ DXGI_SWAP_CHAIN_DESC1 *pDesc)
  {
    return m_pReal1->GetDesc1(pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
      /* [annotation][out] */
      _Out_ DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
  {
    return m_pReal1->GetFullscreenDesc(pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE GetHwnd(
      /* [annotation][out] */
      _Out_ HWND *pHwnd)
  {
    return m_pReal1->GetHwnd(pHwnd);
  }

  virtual HRESULT STDMETHODCALLTYPE GetCoreWindow(
      /* [annotation][in] */
      _In_ REFIID refiid,
      /* [annotation][out] */
      _Out_ void **ppUnk)
  {
    return m_pReal1->GetCoreWindow(refiid, ppUnk);
  }

  virtual HRESULT STDMETHODCALLTYPE Present1(
      /* [in] */ UINT SyncInterval,
      /* [in] */ UINT PresentFlags,
      /* [annotation][in] */
      _In_ const DXGI_PRESENT_PARAMETERS *pPresentParameters);

  virtual BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported(void)
  {
    return m_pReal1->IsTemporaryMonoSupported();
  }

  virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
      /* [annotation][out] */
      _Out_ IDXGIOutput **ppRestrictToOutput);

  virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(
      /* [annotation][in] */
      _In_ const DXGI_RGBA *pColor)
  {
    return m_pReal1->SetBackgroundColor(pColor);
  }

  virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor(
      /* [annotation][out] */
      _Out_ DXGI_RGBA *pColor)
  {
    return m_pReal1->GetBackgroundColor(pColor);
  }

  virtual HRESULT STDMETHODCALLTYPE SetRotation(
      /* [annotation][in] */
      _In_ DXGI_MODE_ROTATION Rotation)
  {
    return m_pReal1->SetRotation(Rotation);
  }

  virtual HRESULT STDMETHODCALLTYPE GetRotation(
      /* [annotation][out] */
      _Out_ DXGI_MODE_ROTATION *pRotation)
  {
    return m_pReal1->GetRotation(pRotation);
  }

  //////////////////////////////
  // implement IDXGISwapChain2

  virtual HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height)
  {
    return m_pReal2->SetSourceSize(Width, Height);
  }

  virtual HRESULT STDMETHODCALLTYPE GetSourceSize(
      /* [annotation][out] */
      _Out_ UINT *pWidth,
      /* [annotation][out] */
      _Out_ UINT *pHeight)
  {
    return m_pReal2->GetSourceSize(pWidth, pHeight);
  }

  virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency)
  {
    return m_pReal2->SetMaximumFrameLatency(MaxLatency);
  }

  virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
      /* [annotation][out] */
      _Out_ UINT *pMaxLatency)
  {
    return m_pReal2->GetMaximumFrameLatency(pMaxLatency);
  }

  virtual HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject(void)
  {
    return m_pReal2->GetFrameLatencyWaitableObject();
  }

  virtual HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix)
  {
    return m_pReal2->SetMatrixTransform(pMatrix);
  }

  virtual HRESULT STDMETHODCALLTYPE GetMatrixTransform(
      /* [annotation][out] */
      _Out_ DXGI_MATRIX_3X2_F *pMatrix)
  {
    return m_pReal2->GetMatrixTransform(pMatrix);
  }

  //////////////////////////////
  // implement IDXGISwapChain3

  virtual UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex(void)
  {
    return m_pReal3->GetCurrentBackBufferIndex();
  }

  virtual HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(
      /* [annotation][in] */
      _In_ DXGI_COLOR_SPACE_TYPE ColorSpace,
      /* [annotation][out] */
      _Out_ UINT *pColorSpaceSupport)
  {
    return m_pReal3->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
  }

  virtual HRESULT STDMETHODCALLTYPE SetColorSpace1(
      /* [annotation][in] */
      _In_ DXGI_COLOR_SPACE_TYPE ColorSpace)
  {
    return m_pReal3->SetColorSpace1(ColorSpace);
  }

  virtual HRESULT STDMETHODCALLTYPE ResizeBuffers1(
      /* [annotation][in] */
      _In_ UINT BufferCount,
      /* [annotation][in] */
      _In_ UINT Width,
      /* [annotation][in] */
      _In_ UINT Height,
      /* [annotation][in] */
      _In_ DXGI_FORMAT Format,
      /* [annotation][in] */
      _In_ UINT SwapChainFlags,
      /* [annotation][in] */
      _In_reads_(BufferCount) const UINT *pCreationNodeMask,
      /* [annotation][in] */
      _In_reads_(BufferCount) IUnknown *const *ppPresentQueue);

  //////////////////////////////
  // implement IDXGISwapChain4

  virtual HRESULT STDMETHODCALLTYPE SetHDRMetaData(
      /* [annotation][in] */
      _In_ DXGI_HDR_METADATA_TYPE Type,
      /* [annotation][in] */
      _In_ UINT Size,
      /* [annotation][size_is][in] */
      _In_reads_opt_(Size) void *pMetaData)
  {
    return m_pReal4->SetHDRMetaData(Type, Size, pMetaData);
  }
};

class WrappedIDXGIOutput6 : public IDXGIOutput6, public RefCountDXGIObject
{
  RefCountDXGIObject *m_Owner;
  IDXGIOutput *m_pReal;
  IDXGIOutput1 *m_pReal1;
  IDXGIOutput2 *m_pReal2;
  IDXGIOutput3 *m_pReal3;
  IDXGIOutput4 *m_pReal4;
  IDXGIOutput5 *m_pReal5;
  IDXGIOutput6 *m_pReal6;

public:
  IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  WrappedIDXGIOutput6(RefCountDXGIObject *owner, IDXGIOutput *real);
  ~WrappedIDXGIOutput6();

  IDXGIOutput *GetReal() { return m_pReal; }
  //////////////////////////////
  // implement IDXGIOutput

  virtual HRESULT STDMETHODCALLTYPE GetDesc(
      /* [annotation][out] */
      _Out_ DXGI_OUTPUT_DESC *pDesc)
  {
    return m_pReal->GetDesc(pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList(
      /* [in] */ DXGI_FORMAT EnumFormat,
      /* [in] */ UINT Flags,
      /* [annotation][out][in] */
      _Inout_ UINT *pNumModes,
      /* [annotation][out] */
      _Out_writes_to_opt_(*pNumModes, *pNumModes) DXGI_MODE_DESC *pDesc)
  {
    return m_pReal->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
      /* [annotation][in] */
      _In_ const DXGI_MODE_DESC *pModeToMatch,
      /* [annotation][out] */
      _Out_ DXGI_MODE_DESC *pClosestMatch,
      /* [annotation][in] */
      _In_opt_ IUnknown *pConcernedDevice)
  {
    return m_pReal->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice);
  }

  virtual HRESULT STDMETHODCALLTYPE WaitForVBlank(void) { return m_pReal->WaitForVBlank(); }
  virtual HRESULT STDMETHODCALLTYPE TakeOwnership(
      /* [annotation][in] */
      _In_ IUnknown *pDevice, BOOL Exclusive)
  {
    return m_pReal->TakeOwnership(pDevice, Exclusive);
  }

  virtual void STDMETHODCALLTYPE ReleaseOwnership(void) { return m_pReal->ReleaseOwnership(); }
  virtual HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
      /* [annotation][out] */
      _Out_ DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps)
  {
    return m_pReal->GetGammaControlCapabilities(pGammaCaps);
  }

  virtual HRESULT STDMETHODCALLTYPE SetGammaControl(
      /* [annotation][in] */
      _In_ const DXGI_GAMMA_CONTROL *pArray)
  {
    return m_pReal->SetGammaControl(pArray);
  }

  virtual HRESULT STDMETHODCALLTYPE GetGammaControl(
      /* [annotation][out] */
      _Out_ DXGI_GAMMA_CONTROL *pArray)
  {
    return m_pReal->GetGammaControl(pArray);
  }

  virtual HRESULT STDMETHODCALLTYPE SetDisplaySurface(
      /* [annotation][in] */
      _In_ IDXGISurface *pScanoutSurface)
  {
    return m_pReal->SetDisplaySurface(pScanoutSurface);
  }

  virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(
      /* [annotation][in] */
      _In_ IDXGISurface *pDestination)
  {
    return m_pReal->GetDisplaySurfaceData(pDestination);
  }

  virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(
      /* [annotation][out] */
      _Out_ DXGI_FRAME_STATISTICS *pStats)
  {
    return m_pReal->GetFrameStatistics(pStats);
  }

  //////////////////////////////
  // implement IDXGIOutput1

  virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList1(
      /* [in] */ DXGI_FORMAT EnumFormat,
      /* [in] */ UINT Flags,
      /* [annotation][out][in] */
      _Inout_ UINT *pNumModes,
      /* [annotation][out] */
      _Out_writes_to_opt_(*pNumModes, *pNumModes) DXGI_MODE_DESC1 *pDesc)
  {
    return m_pReal1->GetDisplayModeList1(EnumFormat, Flags, pNumModes, pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(
      /* [annotation][in] */
      _In_ const DXGI_MODE_DESC1 *pModeToMatch,
      /* [annotation][out] */
      _Out_ DXGI_MODE_DESC1 *pClosestMatch,
      /* [annotation][in] */
      _In_opt_ IUnknown *pConcernedDevice)
  {
    return m_pReal1->FindClosestMatchingMode1(pModeToMatch, pClosestMatch, pConcernedDevice);
  }

  virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData1(
      /* [annotation][in] */
      _In_ IDXGIResource *pDestination)
  {
    return m_pReal1->GetDisplaySurfaceData1(pDestination);
  }

  virtual HRESULT STDMETHODCALLTYPE DuplicateOutput(
      /* [annotation][in] */
      _In_ IUnknown *pDevice,
      /* [annotation][out] */
      _COM_Outptr_ IDXGIOutputDuplication **ppOutputDuplication)
  {
    return m_pReal1->DuplicateOutput(pDevice, ppOutputDuplication);
  }

  //////////////////////////////
  // implement IDXGIOutput2

  virtual BOOL STDMETHODCALLTYPE SupportsOverlays(void) { return m_pReal2->SupportsOverlays(); }
  //////////////////////////////
  // implement IDXGIOutput3

  virtual HRESULT STDMETHODCALLTYPE CheckOverlaySupport(
      /* [annotation][in] */
      _In_ DXGI_FORMAT EnumFormat,
      /* [annotation][out] */
      _In_ IUnknown *pConcernedDevice,
      /* [annotation][out] */
      _Out_ UINT *pFlags)
  {
    return m_pReal3->CheckOverlaySupport(EnumFormat, pConcernedDevice, pFlags);
  }

  //////////////////////////////
  // implement IDXGIOutput4

  virtual HRESULT STDMETHODCALLTYPE CheckOverlayColorSpaceSupport(
      /* [annotation][in] */
      _In_ DXGI_FORMAT Format,
      /* [annotation][in] */
      _In_ DXGI_COLOR_SPACE_TYPE ColorSpace,
      /* [annotation][in] */
      _In_ IUnknown *pConcernedDevice,
      /* [annotation][out] */
      _Out_ UINT *pFlags)
  {
    return m_pReal4->CheckOverlayColorSpaceSupport(Format, ColorSpace, pConcernedDevice, pFlags);
  }

  //////////////////////////////
  // implement IDXGIOutput5

  virtual HRESULT STDMETHODCALLTYPE DuplicateOutput1(
      /* [annotation][in] */
      _In_ IUnknown *pDevice,
      /* [in] */ UINT Flags,
      /* [annotation][in] */
      _In_ UINT SupportedFormatsCount,
      /* [annotation][in] */
      _In_reads_(SupportedFormatsCount) const DXGI_FORMAT *pSupportedFormats,
      /* [annotation][out] */
      _COM_Outptr_ IDXGIOutputDuplication **ppOutputDuplication)
  {
    return m_pReal5->DuplicateOutput1(pDevice, Flags, SupportedFormatsCount, pSupportedFormats,
                                      ppOutputDuplication);
  }

  //////////////////////////////
  // implement IDXGIOutput6

  virtual HRESULT STDMETHODCALLTYPE GetDesc1(
      /* [annotation][out] */
      _Out_ DXGI_OUTPUT_DESC1 *pDesc)
  {
    return m_pReal6->GetDesc1(pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE CheckHardwareCompositionSupport(
      /* [annotation][out] */
      _Out_ UINT *pFlags)
  {
    return m_pReal6->CheckHardwareCompositionSupport(pFlags);
  }
};

class WrappedIDXGIAdapter4 : public IDXGIAdapter4, public RefCountDXGIObject
{
  IDXGIAdapter *m_pReal;
  IDXGIAdapter1 *m_pReal1;
  IDXGIAdapter2 *m_pReal2;
  IDXGIAdapter3 *m_pReal3;
  IDXGIAdapter4 *m_pReal4;

public:
  WrappedIDXGIAdapter4(IDXGIAdapter *real);
  virtual ~WrappedIDXGIAdapter4();

  IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement IDXGIAdapter

  virtual HRESULT STDMETHODCALLTYPE EnumOutputs(
      /* [in] */ UINT Output,
      /* [annotation][out][in] */
      __out IDXGIOutput **ppOutput)
  {
    HRESULT ret = m_pReal->EnumOutputs(Output, ppOutput);

    if(SUCCEEDED(ret) && ppOutput && *ppOutput)
      *ppOutput = (IDXGIOutput *)(new WrappedIDXGIOutput6(this, *ppOutput));

    return ret;
  }

  virtual HRESULT STDMETHODCALLTYPE GetDesc(
      /* [annotation][out] */
      __out DXGI_ADAPTER_DESC *pDesc)
  {
    return m_pReal->GetDesc(pDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(
      /* [annotation][in] */
      __in REFGUID InterfaceName,
      /* [annotation][out] */
      __out LARGE_INTEGER *pUMDVersion)
  {
    return m_pReal->CheckInterfaceSupport(InterfaceName, pUMDVersion);
  }

  //////////////////////////////
  // implement IDXGIAdapter1

  virtual HRESULT STDMETHODCALLTYPE GetDesc1(
      /* [out] */ DXGI_ADAPTER_DESC1 *pDesc)
  {
    return m_pReal1->GetDesc1(pDesc);
  }

  //////////////////////////////
  // implement IDXGIAdapter2

  virtual HRESULT STDMETHODCALLTYPE GetDesc2(
      /* [annotation][out] */
      _Out_ DXGI_ADAPTER_DESC2 *pDesc)
  {
    return m_pReal2->GetDesc2(pDesc);
  }

  //////////////////////////////
  // implement IDXGIAdapter3

  virtual HRESULT STDMETHODCALLTYPE RegisterHardwareContentProtectionTeardownStatusEvent(
      /* [annotation][in] */
      _In_ HANDLE hEvent,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal3->RegisterHardwareContentProtectionTeardownStatusEvent(hEvent, pdwCookie);
  }

  virtual void STDMETHODCALLTYPE UnregisterHardwareContentProtectionTeardownStatus(
      /* [annotation][in] */
      _In_ DWORD dwCookie)
  {
    return m_pReal3->UnregisterHardwareContentProtectionTeardownStatus(dwCookie);
  }

  virtual HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(
      /* [annotation][in] */
      _In_ UINT NodeIndex,
      /* [annotation][in] */
      _In_ DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
      /* [annotation][out] */
      _Out_ DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
  {
    return m_pReal3->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
  }

  virtual HRESULT STDMETHODCALLTYPE SetVideoMemoryReservation(
      /* [annotation][in] */
      _In_ UINT NodeIndex,
      /* [annotation][in] */
      _In_ DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup,
      /* [annotation][in] */
      _In_ UINT64 Reservation)
  {
    return m_pReal3->SetVideoMemoryReservation(NodeIndex, MemorySegmentGroup, Reservation);
  }

  virtual HRESULT STDMETHODCALLTYPE RegisterVideoMemoryBudgetChangeNotificationEvent(
      /* [annotation][in] */
      _In_ HANDLE hEvent,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal3->RegisterVideoMemoryBudgetChangeNotificationEvent(hEvent, pdwCookie);
  }

  virtual void STDMETHODCALLTYPE UnregisterVideoMemoryBudgetChangeNotification(
      /* [annotation][in] */
      _In_ DWORD dwCookie)
  {
    return m_pReal3->UnregisterVideoMemoryBudgetChangeNotification(dwCookie);
  }

  //////////////////////////////
  // implement IDXGIAdapter4

  virtual HRESULT STDMETHODCALLTYPE GetDesc3(
      /* [annotation][out] */
      _Out_ DXGI_ADAPTER_DESC3 *pDesc)
  {
    return m_pReal4->GetDesc3(pDesc);
  }
};

class WrappedIDXGIDevice4 : public IDXGIDevice4, public RefCountDXGIObject
{
  IDXGIDevice *m_pReal;
  IDXGIDevice1 *m_pReal1;
  IDXGIDevice2 *m_pReal2;
  IDXGIDevice3 *m_pReal3;
  IDXGIDevice4 *m_pReal4;
  ID3DDevice *m_pD3DDevice;

public:
  WrappedIDXGIDevice4(IDXGIDevice *real, ID3DDevice *d3d);
  virtual ~WrappedIDXGIDevice4();

  static const int AllocPoolCount = 4;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedIDXGIDevice4, AllocPoolCount);

  IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  ID3DDevice *GetD3DDevice() { return m_pD3DDevice; }
  //////////////////////////////
  // implement IDXGIDevice

  virtual HRESULT STDMETHODCALLTYPE GetAdapter(
      /* [annotation][out] */
      __out IDXGIAdapter **pAdapter)
  {
    HRESULT ret = m_pReal->GetAdapter(pAdapter);
    if(SUCCEEDED(ret))
      *pAdapter = (IDXGIAdapter *)(new WrappedIDXGIAdapter4(*pAdapter));
    return ret;
  }

  virtual HRESULT STDMETHODCALLTYPE CreateSurface(
      /* [annotation][in] */
      __in const DXGI_SURFACE_DESC *pDesc,
      /* [in] */ UINT NumSurfaces,
      /* [in] */ DXGI_USAGE Usage,
      /* [annotation][in] */
      __in_opt const DXGI_SHARED_RESOURCE *pSharedResource,
      /* [annotation][out] */
      __out IDXGISurface **ppSurface)
  {
    return m_pReal->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
  }

  virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency(
      /* [annotation][size_is][in] */
      __in_ecount(NumResources) IUnknown *const *ppResources,
      /* [annotation][size_is][out] */
      __out_ecount(NumResources) DXGI_RESIDENCY *pResidencyStatus,
      /* [in] */ UINT NumResources)
  {
    return m_pReal->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
  }

  virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(
      /* [in] */ INT Priority)
  {
    return m_pReal->SetGPUThreadPriority(Priority);
  }

  virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(
      /* [annotation][retval][out] */
      __out INT *pPriority)
  {
    return m_pReal->GetGPUThreadPriority(pPriority);
  }

  //////////////////////////////
  // implement IDXGIDevice1

  virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
      /* [in] */ UINT MaxLatency)
  {
    return m_pReal1->SetMaximumFrameLatency(MaxLatency);
  }

  virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
      /* [annotation][out] */
      __out UINT *pMaxLatency)
  {
    return m_pReal1->GetMaximumFrameLatency(pMaxLatency);
  }

  //////////////////////////////
  // implement IDXGIDevice2

  virtual HRESULT STDMETHODCALLTYPE OfferResources(
      /* [annotation][in] */
      _In_ UINT NumResources,
      /* [annotation][size_is][in] */
      _In_reads_(NumResources) IDXGIResource *const *ppResources,
      /* [annotation][in] */
      _In_ DXGI_OFFER_RESOURCE_PRIORITY Priority);

  virtual HRESULT STDMETHODCALLTYPE ReclaimResources(
      /* [annotation][in] */
      _In_ UINT NumResources,
      /* [annotation][size_is][in] */
      _In_reads_(NumResources) IDXGIResource *const *ppResources,
      /* [annotation][size_is][out] */
      _Out_writes_all_opt_(NumResources) BOOL *pDiscarded);

  virtual HRESULT STDMETHODCALLTYPE EnqueueSetEvent(
      /* [annotation][in] */
      _In_ HANDLE hEvent)
  {
    return m_pReal2->EnqueueSetEvent(hEvent);
  }

  //////////////////////////////
  // implement IDXGIDevice3

  virtual void STDMETHODCALLTYPE Trim() { m_pReal3->Trim(); }
  //////////////////////////////
  // implement IDXGIDevice4

  virtual HRESULT STDMETHODCALLTYPE OfferResources1(
      /* [annotation][in] */
      _In_ UINT NumResources,
      /* [annotation][size_is][in] */
      _In_reads_(NumResources) IDXGIResource *const *ppResources,
      /* [annotation][in] */
      _In_ DXGI_OFFER_RESOURCE_PRIORITY Priority,
      /* [annotation][in] */
      _In_ UINT Flags);

  virtual HRESULT STDMETHODCALLTYPE ReclaimResources1(
      /* [annotation][in] */
      _In_ UINT NumResources,
      /* [annotation][size_is][in] */
      _In_reads_(NumResources) IDXGIResource *const *ppResources,
      /* [annotation][size_is][out] */
      _Out_writes_all_(NumResources) DXGI_RECLAIM_RESOURCE_RESULTS *pResults);
};

class WrappedIDXGIFactory : public IDXGIFactory7, public RefCountDXGIObject
{
  IDXGIFactory *m_pReal;
  IDXGIFactory1 *m_pReal1;
  IDXGIFactory2 *m_pReal2;
  IDXGIFactory3 *m_pReal3;
  IDXGIFactory4 *m_pReal4;
  IDXGIFactory5 *m_pReal5;
  IDXGIFactory6 *m_pReal6;
  IDXGIFactory7 *m_pReal7;

public:
  WrappedIDXGIFactory(IDXGIFactory *real);
  virtual ~WrappedIDXGIFactory();

  IMPLEMENT_IDXGIOBJECT_WITH_REFCOUNTDXGIOBJECT_CUSTOMQUERY;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement IDXGIFactory

  virtual HRESULT STDMETHODCALLTYPE EnumAdapters(
      /* [in] */ UINT Adapter,
      /* [annotation][out] */
      __out IDXGIAdapter **ppAdapter)
  {
    HRESULT ret = m_pReal->EnumAdapters(Adapter, ppAdapter);
    if(SUCCEEDED(ret))
      *ppAdapter = (IDXGIAdapter *)(new WrappedIDXGIAdapter4(*ppAdapter));
    return ret;
  }

  virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags)
  {
    return m_pReal->MakeWindowAssociation(WindowHandle, Flags);
  }

  virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation(
      /* [annotation][out] */
      __out HWND *pWindowHandle)
  {
    return m_pReal->GetWindowAssociation(pWindowHandle);
  }

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(
      /* [annotation][in] */
      __in IUnknown *pDevice,
      /* [annotation][in] */
      __in DXGI_SWAP_CHAIN_DESC *pDesc,
      /* [annotation][out] */
      __out IDXGISwapChain **ppSwapChain);

  virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(
      /* [in] */ HMODULE Module,
      /* [annotation][out] */
      __out IDXGIAdapter **ppAdapter)
  {
    HRESULT ret = m_pReal->CreateSoftwareAdapter(Module, ppAdapter);
    if(SUCCEEDED(ret))
      *ppAdapter = (IDXGIAdapter *)(new WrappedIDXGIAdapter4(*ppAdapter));
    return ret;
  }

  //////////////////////////////
  // implement IDXGIFactory1

  virtual HRESULT STDMETHODCALLTYPE EnumAdapters1(
      /* [in] */ UINT Adapter,
      /* [annotation][out] */
      __out IDXGIAdapter1 **ppAdapter)
  {
    IDXGIFactory1 *factory = m_pReal1;
    if(m_pReal1 == NULL)
    {
      // see comment in RefCountDXGIObject::HandleWrap for IDXGIFactory
      RDCWARN("Calling EnumAdapters1 with no IDXGIFactory1 - assuming weird internal call");
      factory = (IDXGIFactory1 *)m_pReal;
    }

    HRESULT ret = factory->EnumAdapters1(Adapter, ppAdapter);

    if(SUCCEEDED(ret))
      *ppAdapter = (IDXGIAdapter1 *)(new WrappedIDXGIAdapter4(*ppAdapter));
    return ret;
  }

  virtual BOOL STDMETHODCALLTYPE IsCurrent(void) { return m_pReal1->IsCurrent(); }
  //////////////////////////////
  // implement IDXGIFactory2

  virtual BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled(void)
  {
    return m_pReal2->IsWindowedStereoEnabled();
  }

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(
      /* [annotation][in] */
      _In_ IUnknown *pDevice,
      /* [annotation][in] */
      _In_ HWND hWnd,
      /* [annotation][in] */
      _In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      /* [annotation][in] */
      _In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      /* [annotation][in] */
      _In_opt_ IDXGIOutput *pRestrictToOutput,
      /* [annotation][out] */
      _Out_ IDXGISwapChain1 **ppSwapChain);

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(
      /* [annotation][in] */
      _In_ IUnknown *pDevice,
      /* [annotation][in] */
      _In_ IUnknown *pWindow,
      /* [annotation][in] */
      _In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      /* [annotation][in] */
      _In_opt_ IDXGIOutput *pRestrictToOutput,
      /* [annotation][out] */
      _Out_ IDXGISwapChain1 **ppSwapChain);

  virtual HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(
      /* [annotation] */
      _In_ HANDLE hResource,
      /* [annotation] */
      _Out_ LUID *pLuid)
  {
    return m_pReal2->GetSharedResourceAdapterLuid(hResource, pLuid);
  }

  virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(
      /* [annotation][in] */
      _In_ HWND WindowHandle,
      /* [annotation][in] */
      _In_ UINT wMsg,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal2->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
  }

  virtual HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(
      /* [annotation][in] */
      _In_ HANDLE hEvent,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal2->RegisterStereoStatusEvent(hEvent, pdwCookie);
  }

  virtual void STDMETHODCALLTYPE UnregisterStereoStatus(
      /* [annotation][in] */
      _In_ DWORD dwCookie)
  {
    return m_pReal2->UnregisterStereoStatus(dwCookie);
  }

  virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(
      /* [annotation][in] */
      _In_ HWND WindowHandle,
      /* [annotation][in] */
      _In_ UINT wMsg,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal2->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
  }

  virtual HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(
      /* [annotation][in] */
      _In_ HANDLE hEvent,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal2->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
  }

  virtual void STDMETHODCALLTYPE UnregisterOcclusionStatus(
      /* [annotation][in] */
      _In_ DWORD dwCookie)
  {
    return m_pReal2->UnregisterOcclusionStatus(dwCookie);
  }

  virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(
      /* [annotation][in] */
      _In_ IUnknown *pDevice,
      /* [annotation][in] */
      _In_ const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      /* [annotation][in] */
      _In_opt_ IDXGIOutput *pRestrictToOutput,
      /* [annotation][out] */
      _Outptr_ IDXGISwapChain1 **ppSwapChain);

  //////////////////////////////
  // implement IDXGIFactory3

  virtual UINT STDMETHODCALLTYPE GetCreationFlags(void) { return m_pReal3->GetCreationFlags(); }
  //////////////////////////////
  // implement IDXGIFactory4

  void WrapAdapter(REFIID riid, void **ppvAdapter)
  {
    if(ppvAdapter == NULL || *ppvAdapter == NULL)
      return;

    if(riid == __uuidof(IDXGIAdapter4))
    {
      IDXGIAdapter4 *adapter = (IDXGIAdapter4 *)*ppvAdapter;
      *ppvAdapter = (IDXGIAdapter4 *)(new WrappedIDXGIAdapter4(adapter));
    }
    else if(riid == __uuidof(IDXGIAdapter3))
    {
      IDXGIAdapter3 *adapter = (IDXGIAdapter3 *)*ppvAdapter;
      *ppvAdapter = (IDXGIAdapter3 *)(new WrappedIDXGIAdapter4(adapter));
    }
    else if(riid == __uuidof(IDXGIAdapter2))
    {
      IDXGIAdapter2 *adapter = (IDXGIAdapter2 *)*ppvAdapter;
      *ppvAdapter = (IDXGIAdapter2 *)(new WrappedIDXGIAdapter4(adapter));
    }
    else if(riid == __uuidof(IDXGIAdapter1))
    {
      IDXGIAdapter1 *adapter = (IDXGIAdapter1 *)*ppvAdapter;
      *ppvAdapter = (IDXGIAdapter1 *)(new WrappedIDXGIAdapter4(adapter));
    }
    else if(riid == __uuidof(IDXGIAdapter))
    {
      IDXGIAdapter *adapter = (IDXGIAdapter *)*ppvAdapter;
      *ppvAdapter = (IDXGIAdapter *)(new WrappedIDXGIAdapter4(adapter));
    }
    else
    {
      RDCWARN("Wrapping unknown adapter GUID %s", ToStr(riid).c_str());
      RefCountDXGIObject::HandleWrap(riid, ppvAdapter);
    }
  }

  virtual HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(
      /* [annotation] */
      _In_ LUID AdapterLuid,
      /* [annotation] */
      _In_ REFIID riid,
      /* [annotation] */
      _COM_Outptr_ void **ppvAdapter)
  {
    HRESULT ret = m_pReal4->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter);
    if(SUCCEEDED(ret))
      WrapAdapter(riid, ppvAdapter);
    return ret;
  }

  virtual HRESULT STDMETHODCALLTYPE EnumWarpAdapter(
      /* [annotation] */
      _In_ REFIID riid,
      /* [annotation] */
      _COM_Outptr_ void **ppvAdapter)
  {
    HRESULT ret = m_pReal4->EnumWarpAdapter(riid, ppvAdapter);
    if(SUCCEEDED(ret))
      WrapAdapter(riid, ppvAdapter);
    return ret;
  }

  //////////////////////////////
  // implement IDXGIFactory5

  virtual HRESULT STDMETHODCALLTYPE
  CheckFeatureSupport(DXGI_FEATURE Feature,
                      /* [annotation] */
                      _Inout_updates_bytes_(FeatureSupportDataSize) void *pFeatureSupportData,
                      UINT FeatureSupportDataSize)
  {
    return m_pReal5->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
  }

  //////////////////////////////
  // implement IDXGIFactory6

  virtual HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(
      /* [annotation] */
      _In_ UINT Adapter,
      /* [annotation] */
      _In_ DXGI_GPU_PREFERENCE GpuPreference,
      /* [annotation] */
      _In_ REFIID riid,
      /* [annotation] */
      _COM_Outptr_ void **ppvAdapter)
  {
    HRESULT ret = m_pReal6->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter);
    if(SUCCEEDED(ret))
      WrapAdapter(riid, ppvAdapter);
    return ret;
  }

  //////////////////////////////
  // implement IDXGIFactory7

  virtual HRESULT STDMETHODCALLTYPE RegisterAdaptersChangedEvent(
      /* [annotation][in] */
      _In_ HANDLE hEvent,
      /* [annotation][out] */
      _Out_ DWORD *pdwCookie)
  {
    return m_pReal7->RegisterAdaptersChangedEvent(hEvent, pdwCookie);
  }

  virtual HRESULT STDMETHODCALLTYPE UnregisterAdaptersChangedEvent(
      /* [annotation][in] */
      _In_ DWORD dwCookie)
  {
    return m_pReal7->UnregisterAdaptersChangedEvent(dwCookie);
  }
};
