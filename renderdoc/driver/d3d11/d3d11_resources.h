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

#include <algorithm>
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_manager.h"
#include "driver/shaders/dxbc/dxbc_inspect.h"

D3D11ResourceType IdentifyTypeByPtr(IUnknown *ptr);
ResourceId GetIDForDeviceChild(ID3D11DeviceChild *ptr);
template <typename T>
inline ResourceId GetIDForResource(T *ptr);
template <typename T>
inline ResourceId GetViewResourceResID(T *);

UINT GetByteSize(ID3D11Texture1D *tex, int SubResource);
UINT GetByteSize(ID3D11Texture2D *tex, int SubResource);
UINT GetByteSize(ID3D11Texture3D *tex, int SubResource);

UINT GetMipForSubresource(ID3D11Resource *res, int Subresource);

struct ResourcePitch
{
  UINT m_RowPitch;
  UINT m_DepthPitch;
};

ResourcePitch GetResourcePitchForSubresource(ID3D11DeviceContext *ctx, ID3D11Resource *res,
                                             int Subresource);

template <typename derived, typename base>
bool CanQuery(base *b)
{
  derived *d = NULL;
  HRESULT check = b->QueryInterface(__uuidof(derived), (void **)&d);

  if(d)
    d->Release();

  return SUCCEEDED(check) && d != NULL;
}

class TrackedResource
{
public:
  TrackedResource() { m_ID = ResourceIDGen::GetNewUniqueID(); }
  ResourceId GetResourceID() { return m_ID; }
private:
  TrackedResource(const TrackedResource &);
  TrackedResource &operator=(const TrackedResource &);

  ResourceId m_ID;
};

extern const GUID RENDERDOC_ID3D11ShaderGUID_ShaderDebugMagicValue;

template <typename NestedType, typename NestedType1 = NestedType, typename NestedType2 = NestedType1>
class WrappedDeviceChild11 : public RefCounter, public NestedType2, public TrackedResource
{
protected:
  WrappedID3D11Device *m_pDevice;
  NestedType *m_pReal;
  unsigned int m_PipelineRefs;

  WrappedDeviceChild11(NestedType *real, WrappedID3D11Device *device)
      : RefCounter(real), m_pDevice(device), m_pReal(real), m_PipelineRefs(0)
  {
    m_pDevice->SoftRef();

    bool ret = m_pDevice->GetResourceManager()->AddWrapper(this, real);
    if(!ret)
      RDCERR("Error adding wrapper for type %s", ToStr(__uuidof(NestedType)).c_str());

    m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);
  }

  virtual void Shutdown()
  {
    m_pDevice->GetResourceManager()->RemoveWrapper(m_pReal);
    m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());
    m_pDevice->ReleaseResource((NestedType *)this);
    SAFE_RELEASE(m_pReal);
    m_pDevice = NULL;
  }

  virtual ~WrappedDeviceChild11()
  {
    // should have already called shutdown (needs to be called from child class to ensure
    // vtables are still in place when we call ReleaseResource)
    RDCASSERT(m_pDevice == NULL && m_pReal == NULL);
  }

public:
  typedef NestedType InnerType;
  typedef NestedType1 InnerType1;
  typedef NestedType2 InnerType2;

  NestedType *GetReal() { return m_pReal; }
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::SoftRef(m_pDevice) - m_PipelineRefs; }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int piperefs = m_PipelineRefs;
    return RefCounter::SoftRelease(m_pDevice) - piperefs;
  }

  void PipelineAddRef() { InterlockedIncrement(&m_PipelineRefs); }
  void PipelineRelease() { InterlockedDecrement(&m_PipelineRefs); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(IUnknown))
    {
      *ppvObject = (IUnknown *)(NestedType *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(NestedType))
    {
      *ppvObject = (NestedType *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(NestedType1))
    {
      // check that the real interface supports this
      NestedType1 *dummy = NULL;
      HRESULT check = m_pReal->QueryInterface(riid, (void **)&dummy);

      SAFE_RELEASE(dummy);

      if(FAILED(check))
        return check;

      *ppvObject = (NestedType1 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(NestedType2))
    {
      // check that the real interface supports this
      NestedType2 *dummy = NULL;
      HRESULT check = m_pReal->QueryInterface(riid, (void **)&dummy);

      SAFE_RELEASE(dummy);

      if(FAILED(check))
        return check;

      *ppvObject = (NestedType2 *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D11DeviceChild))
    {
      *ppvObject = (ID3D11DeviceChild *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D11Multithread))
    {
      // forward to the device as the lock is shared amongst all things
      return m_pDevice->QueryInterface(riid, ppvObject);
    }

    // for DXGI object queries, just make a new throw-away WrappedDXGIObject
    // and return.
    if(riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) ||
       riid == __uuidof(IDXGIResource) || riid == __uuidof(IDXGIKeyedMutex) ||
       riid == __uuidof(IDXGISurface) || riid == __uuidof(IDXGISurface1) ||
       riid == __uuidof(IDXGIResource1) || riid == __uuidof(IDXGISurface2))
    {
      // ensure the real object has this interface
      void *outObj;
      HRESULT hr = m_pReal->QueryInterface(riid, &outObj);

      IUnknown *unk = (IUnknown *)outObj;
      SAFE_RELEASE(unk);

      if(FAILED(hr))
      {
        return hr;
      }

      auto dxgiWrapper = new WrappedDXGIInterface<WrappedDeviceChild11>(this, m_pDevice);

      // anything could happen outside of our wrapped ecosystem, so immediately mark dirty
      m_pDevice->GetResourceManager()->MarkDirtyResource(GetResourceID());

      if(riid == __uuidof(IDXGIObject))
      {
        *ppvObject = (IDXGIObject *)(IDXGIKeyedMutex *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGIDeviceSubObject))
      {
        *ppvObject = (IDXGIDeviceSubObject *)(IDXGIKeyedMutex *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGIResource))
      {
        *ppvObject = (IDXGIResource *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGIKeyedMutex))
      {
        *ppvObject = (IDXGIKeyedMutex *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGISurface))
      {
        *ppvObject = (IDXGISurface *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGISurface1))
      {
        *ppvObject = (IDXGISurface1 *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGIResource1))
      {
        *ppvObject = (IDXGIResource1 *)dxgiWrapper;
      }
      else if(riid == __uuidof(IDXGISurface2))
      {
        *ppvObject = (IDXGISurface2 *)dxgiWrapper;
      }
      else
      {
        RDCWARN("Unexpected guid %s", ToStr(riid).c_str());
        SAFE_DELETE(dxgiWrapper);
      }

      return S_OK;
    }

    return RefCounter::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11DeviceChild

  void STDMETHODCALLTYPE GetDevice(
      /* [annotation] */
      __out ID3D11Device **ppDevice)
  {
    if(ppDevice)
    {
      *ppDevice = m_pDevice;
      m_pDevice->AddRef();
    }
  }

  HRESULT STDMETHODCALLTYPE GetPrivateData(
      /* [annotation] */
      __in REFGUID guid,
      /* [annotation] */
      __inout UINT *pDataSize,
      /* [annotation] */
      __out_bcount_opt(*pDataSize) void *pData)
  {
    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(
      /* [annotation] */
      __in REFGUID guid,
      /* [annotation] */
      __in UINT DataSize,
      /* [annotation] */
      __in_bcount_opt(DataSize) const void *pData)
  {
    if(guid == RENDERDOC_ID3D11ShaderGUID_ShaderDebugMagicValue)
      return m_pDevice->SetShaderDebugPath(this, (const char *)pData);

    if(guid == WKPDID_D3DDebugObjectName)
    {
      const char *pStrData = (const char *)pData;
      if(DataSize != 0 && pStrData[DataSize - 1] != '\0')
      {
        std::string sName(pStrData, DataSize);
        m_pDevice->SetResourceName(this, sName.c_str());
      }
      else
      {
        m_pDevice->SetResourceName(this, pStrData);
      }
    }

    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      /* [annotation] */
      __in REFGUID guid,
      /* [annotation] */
      __in_opt const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }
};

template <typename NestedType, typename DescType, typename NestedType1 = NestedType>
class WrappedResource11 : public WrappedDeviceChild11<NestedType, NestedType1>
{
private:
  unsigned int m_ViewRefcount;    // refcount from views (invisible to the end-user)

protected:
#if ENABLED(RDOC_DEVEL)
  DescType m_Desc;
#endif

  WrappedResource11(NestedType *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device), m_ViewRefcount(0)
  {
#if ENABLED(RDOC_DEVEL)
    real->GetDesc(&m_Desc);
#endif

    // we'll handle deleting on release, so we can check against m_ViewRefcount
    RefCounter::SetSelfDeleting(false);
  }

  virtual void Shutdown() { WrappedDeviceChild11::Shutdown(); }
  virtual ~WrappedResource11() {}
public:
  void ViewAddRef()
  {
    InterlockedIncrement(&m_ViewRefcount);

    RefCounter::AddDeviceSoftref(m_pDevice);
  }

  void ViewRelease()
  {
    InterlockedDecrement(&m_ViewRefcount);
    unsigned int extRefCount = RefCounter::GetRefCount();

    WrappedID3D11Device *dev = m_pDevice;

    if(extRefCount == 0 && m_ViewRefcount == 0)
      delete this;

    RefCounter::ReleaseDeviceSoftref(dev);
  }

  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::SoftRef(m_pDevice) - m_PipelineRefs; }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int extRefCount = RefCounter::Release();
    unsigned int pipeRefs = m_PipelineRefs;

    WrappedID3D11Device *dev = m_pDevice;

    if(extRefCount == 0 && m_ViewRefcount == 0)
      delete this;

    RefCounter::ReleaseDeviceSoftref(dev);

    return extRefCount - pipeRefs;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D11Resource))
    {
      *ppvObject = (ID3D11Resource *)this;
      AddRef();
      return S_OK;
    }

    return WrappedDeviceChild11::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11Resource

  virtual void STDMETHODCALLTYPE GetType(
      /* [annotation] */
      __out D3D11_RESOURCE_DIMENSION *pResourceDimension)
  {
    m_pReal->GetType(pResourceDimension);
  }

  virtual void STDMETHODCALLTYPE SetEvictionPriority(
      /* [annotation] */
      __in UINT EvictionPriority)
  {
    m_pReal->SetEvictionPriority(EvictionPriority);
  }

  virtual UINT STDMETHODCALLTYPE GetEvictionPriority(void)
  {
    return m_pReal->GetEvictionPriority();
  }

  //////////////////////////////
  // implement NestedType

  virtual void STDMETHODCALLTYPE GetDesc(
      /* [annotation] */
      __out DescType *pDesc)
  {
    m_pReal->GetDesc(pDesc);
  }
};

class WrappedID3D11Buffer : public WrappedResource11<ID3D11Buffer, D3D11_BUFFER_DESC>
{
public:
  struct BufferEntry
  {
    BufferEntry(WrappedID3D11Buffer *b = NULL, uint32_t l = 0) : m_Buffer(b), length(l) {}
    WrappedID3D11Buffer *m_Buffer;
    uint32_t length;
  };

  static std::map<ResourceId, BufferEntry> m_BufferList;

  static const int AllocPoolCount = 128 * 1024;
  static const int AllocPoolMaxByteSize = 13 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Buffer, AllocPoolCount, AllocPoolMaxByteSize);

  WrappedID3D11Buffer(ID3D11Buffer *real, uint32_t byteLength, WrappedID3D11Device *device)
      : WrappedResource11(real, device)
  {
    SCOPED_LOCK(m_pDevice->D3DLock());

    RDCASSERT(m_BufferList.find(GetResourceID()) == m_BufferList.end());
    m_BufferList[GetResourceID()] = BufferEntry(this, byteLength);
  }

  virtual ~WrappedID3D11Buffer()
  {
    SCOPED_LOCK(m_pDevice->D3DLock());

    if(m_BufferList.find(GetResourceID()) != m_BufferList.end())
      m_BufferList.erase(GetResourceID());

    Shutdown();
  }

  virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
  {
    if(pResourceDimension)
      *pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
  }
};

template <typename NestedType, typename DescType, typename NestedType1>
class WrappedTexture : public WrappedResource11<NestedType, DescType, NestedType1>
{
public:
  struct TextureEntry
  {
    TextureEntry(NestedType *t = NULL, TextureDisplayType ty = TEXDISPLAY_UNKNOWN)
        : m_Texture(t), m_Type(ty)
    {
    }
    NestedType *m_Texture;
    TextureDisplayType m_Type;
  };

  static std::map<ResourceId, TextureEntry> m_TextureList;

  WrappedTexture(NestedType *real, WrappedID3D11Device *device, TextureDisplayType type)
      : WrappedResource11(real, device)
  {
    if(type != TEXDISPLAY_UNKNOWN)
    {
      SCOPED_LOCK(m_pDevice->D3DLock());

      RDCASSERT(m_TextureList.find(GetResourceID()) == m_TextureList.end());
      m_TextureList[GetResourceID()] = TextureEntry(this, type);
    }
  }

  virtual ~WrappedTexture()
  {
    SCOPED_LOCK(m_pDevice->D3DLock());

    if(m_TextureList.find(GetResourceID()) != m_TextureList.end())
      m_TextureList.erase(GetResourceID());

    Shutdown();
  }
};

class WrappedID3D11Texture1D
    : public WrappedTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, ID3D11Texture1D>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Texture1D);

  WrappedID3D11Texture1D(ID3D11Texture1D *real, WrappedID3D11Device *device,
                         TextureDisplayType type = TEXDISPLAY_SRV_COMPATIBLE)
      : WrappedTexture(real, device, type)
  {
  }
  virtual ~WrappedID3D11Texture1D() {}
  virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
  {
    if(pResourceDimension)
      *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  }
};

class WrappedID3D11Texture2D1
    : public WrappedTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, ID3D11Texture2D1>
{
public:
  static const int AllocPoolCount = 32768;
  static const int AllocPoolMaxByteSize = 4 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Texture2D1, AllocPoolCount, AllocPoolMaxByteSize);

  WrappedID3D11Texture2D1(ID3D11Texture2D *real, WrappedID3D11Device *device,
                          TextureDisplayType type = TEXDISPLAY_SRV_COMPATIBLE)
      : WrappedTexture(real, device, type)
  {
    m_RealDescriptor = NULL;
  }
  virtual ~WrappedID3D11Texture2D1() { SAFE_DELETE(m_RealDescriptor); }
  // for backbuffer textures they behave a little differently from every other texture in D3D11
  // as they can be cast from one type to another, whereas normally you need to declare as typeless
  // and then cast to a type. To simulate this on our fake backbuffer textures I create them as
  // typeless, HOWEVER this means if we try to create a view with a NULL descriptor then we need
  // the real original type.
  D3D11_TEXTURE2D_DESC *m_RealDescriptor;

  virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
  {
    if(pResourceDimension)
      *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  }

  //////////////////////////////
  // implement ID3D11Texture2D1

  virtual void STDMETHODCALLTYPE GetDesc1(
      /* [annotation] */
      __out D3D11_TEXTURE2D_DESC1 *pDesc)
  {
    ID3D11Texture2D1 *tex1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11Texture2D1), (void **)&tex1);

    if(SUCCEEDED(check) && tex1)
      tex1->GetDesc1(pDesc);

    SAFE_RELEASE(tex1);
  }
};

class WrappedID3D11Texture3D1
    : public WrappedTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, ID3D11Texture3D1>
{
public:
  static const int AllocPoolCount = 16384;
  static const int AllocPoolMaxByteSize = 2 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Texture3D1, AllocPoolCount, AllocPoolMaxByteSize);

  WrappedID3D11Texture3D1(ID3D11Texture3D *real, WrappedID3D11Device *device,
                          TextureDisplayType type = TEXDISPLAY_SRV_COMPATIBLE)
      : WrappedTexture(real, device, type)
  {
  }
  virtual ~WrappedID3D11Texture3D1() {}
  virtual void STDMETHODCALLTYPE GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension)
  {
    if(pResourceDimension)
      *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  }

  //////////////////////////////
  // implement ID3D11Texture3D1

  virtual void STDMETHODCALLTYPE GetDesc1(
      /* [annotation] */
      __out D3D11_TEXTURE3D_DESC1 *pDesc)
  {
    ID3D11Texture3D1 *tex1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11Texture3D1), (void **)&tex1);

    if(SUCCEEDED(check) && tex1)
      tex1->GetDesc1(pDesc);

    SAFE_RELEASE(tex1);
  }
};

class WrappedID3D11InputLayout : public WrappedDeviceChild11<ID3D11InputLayout>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11InputLayout);

  WrappedID3D11InputLayout(ID3D11InputLayout *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11<ID3D11InputLayout>(real, device)
  {
  }
  virtual ~WrappedID3D11InputLayout() { Shutdown(); }
};

class WrappedID3D11RasterizerState2
    : public WrappedDeviceChild11<ID3D11RasterizerState, ID3D11RasterizerState1, ID3D11RasterizerState2>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11RasterizerState2);

  WrappedID3D11RasterizerState2(ID3D11RasterizerState *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11RasterizerState2() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11RasterizerState

  virtual void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC *pDesc) { m_pReal->GetDesc(pDesc); }
  //////////////////////////////
  // implement ID3D11RasterizerState1
  virtual void STDMETHODCALLTYPE GetDesc1(D3D11_RASTERIZER_DESC1 *pDesc1)
  {
    ID3D11RasterizerState1 *state1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11RasterizerState1), (void **)&state1);

    if(SUCCEEDED(check) && state1)
      state1->GetDesc1(pDesc1);

    SAFE_RELEASE(state1);
  }

  //////////////////////////////
  // implement ID3D11RasterizerState2
  virtual void STDMETHODCALLTYPE GetDesc2(D3D11_RASTERIZER_DESC2 *pDesc2)
  {
    ID3D11RasterizerState2 *state2 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11RasterizerState2), (void **)&state2);

    if(SUCCEEDED(check) && state2)
      state2->GetDesc2(pDesc2);

    SAFE_RELEASE(state2);
  }
};

class WrappedID3D11BlendState1 : public WrappedDeviceChild11<ID3D11BlendState, ID3D11BlendState1>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11BlendState1);

  WrappedID3D11BlendState1(ID3D11BlendState *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11BlendState1() { Shutdown(); }
  static bool IsState1(ID3D11BlendState *state) { return CanQuery<ID3D11BlendState1>(state); }
  //////////////////////////////
  // implement ID3D11BlendState

  virtual void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC *pDesc) { m_pReal->GetDesc(pDesc); }
  //////////////////////////////
  // implement ID3D11BlendState1
  virtual void STDMETHODCALLTYPE GetDesc1(D3D11_BLEND_DESC1 *pDesc1)
  {
    ID3D11BlendState1 *state1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11BlendState1), (void **)&state1);

    if(SUCCEEDED(check) && state1)
      state1->GetDesc1(pDesc1);

    SAFE_RELEASE(state1);
  }
};

class WrappedID3D11DepthStencilState : public WrappedDeviceChild11<ID3D11DepthStencilState>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DepthStencilState);

  WrappedID3D11DepthStencilState(ID3D11DepthStencilState *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11<ID3D11DepthStencilState>(real, device)
  {
  }
  virtual ~WrappedID3D11DepthStencilState() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11DepthStencilState

  virtual void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC *pDesc)
  {
    m_pReal->GetDesc(pDesc);
  }
};

class WrappedID3D11SamplerState : public WrappedDeviceChild11<ID3D11SamplerState>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11SamplerState);

  WrappedID3D11SamplerState(ID3D11SamplerState *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11<ID3D11SamplerState>(real, device)
  {
  }
  virtual ~WrappedID3D11SamplerState() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11SamplerState

  virtual void STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC *pDesc) { m_pReal->GetDesc(pDesc); }
};

template <typename NestedType, typename DescType, typename NestedType1>
class WrappedView1 : public WrappedDeviceChild11<NestedType, NestedType1>
{
protected:
  ID3D11Resource *m_pResource;
  ResourceId m_ResourceResID;
  ResourceRange m_ResourceRange;

  WrappedView1(NestedType *real, WrappedID3D11Device *device, ID3D11Resource *res)
      : WrappedDeviceChild11(real, device), m_pResource(res), m_ResourceRange(this)
  {
    m_ResourceResID = GetIDForResource(m_pResource);
    // cast is potentially invalid but functions in WrappedResource will be identical across each
    ((WrappedID3D11Buffer *)m_pResource)->ViewAddRef();
  }

  virtual void Shutdown()
  {
    WrappedDeviceChild11::Shutdown();
    // cast is potentially invalid but functions in WrappedResource will be identical across each
    ((WrappedID3D11Buffer *)m_pResource)->ViewRelease();
    m_pResource = NULL;
  }

  virtual ~WrappedView1() {}
public:
  ResourceId GetResourceResID() { return m_ResourceResID; }
  const ResourceRange &GetResourceRange() { return m_ResourceRange; }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D11View))
    {
      *ppvObject = (ID3D11View *)this;
      AddRef();
      return S_OK;
    }

    return WrappedDeviceChild11::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11View

  void STDMETHODCALLTYPE GetResource(
      /* [annotation] */
      __out ID3D11Resource **pResource)
  {
    RDCASSERT(m_pResource);
    if(pResource)
      *pResource = m_pResource;
    m_pResource->AddRef();
  }

  //////////////////////////////
  // implement NestedType

  void STDMETHODCALLTYPE GetDesc(
      /* [annotation] */
      __out DescType *pDesc)
  {
    m_pReal->GetDesc(pDesc);
  }
};

class WrappedID3D11RenderTargetView1
    : public WrappedView1<ID3D11RenderTargetView, D3D11_RENDER_TARGET_VIEW_DESC, ID3D11RenderTargetView1>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11RenderTargetView1);

  WrappedID3D11RenderTargetView1(ID3D11RenderTargetView *real, ID3D11Resource *res,
                                 WrappedID3D11Device *device)
      : WrappedView1(real, device, res)
  {
  }
  virtual ~WrappedID3D11RenderTargetView1() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11RenderTargetView1
  virtual void STDMETHODCALLTYPE GetDesc1(D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1)
  {
    ID3D11RenderTargetView1 *view1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11RenderTargetView1), (void **)&view1);

    if(SUCCEEDED(check) && view1)
      view1->GetDesc1(pDesc1);

    SAFE_RELEASE(view1);
  }
};

class WrappedID3D11ShaderResourceView1
    : public WrappedView1<ID3D11ShaderResourceView, D3D11_SHADER_RESOURCE_VIEW_DESC, ID3D11ShaderResourceView1>
{
public:
  static const int AllocPoolCount = 65535;
  static const int AllocPoolMaxByteSize = 6 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11ShaderResourceView1, AllocPoolCount, AllocPoolMaxByteSize);

  WrappedID3D11ShaderResourceView1(ID3D11ShaderResourceView *real, ID3D11Resource *res,
                                   WrappedID3D11Device *device)
      : WrappedView1(real, device, res)
  {
  }
  virtual ~WrappedID3D11ShaderResourceView1() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11ShaderResourceView1
  virtual void STDMETHODCALLTYPE GetDesc1(D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1)
  {
    ID3D11ShaderResourceView1 *view1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11ShaderResourceView1), (void **)&view1);

    if(SUCCEEDED(check) && view1)
      view1->GetDesc1(pDesc1);

    SAFE_RELEASE(view1);
  }
};

class WrappedID3D11DepthStencilView
    : public WrappedView1<ID3D11DepthStencilView, D3D11_DEPTH_STENCIL_VIEW_DESC, ID3D11DepthStencilView>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DepthStencilView);

  WrappedID3D11DepthStencilView(ID3D11DepthStencilView *real, ID3D11Resource *res,
                                WrappedID3D11Device *device)
      : WrappedView1(real, device, res)
  {
  }
  virtual ~WrappedID3D11DepthStencilView() { Shutdown(); }
};

class WrappedID3D11UnorderedAccessView1
    : public WrappedView1<ID3D11UnorderedAccessView, D3D11_UNORDERED_ACCESS_VIEW_DESC,
                          ID3D11UnorderedAccessView1>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11UnorderedAccessView1);

  WrappedID3D11UnorderedAccessView1(ID3D11UnorderedAccessView *real, ID3D11Resource *res,
                                    WrappedID3D11Device *device)
      : WrappedView1(real, device, res)
  {
  }
  virtual ~WrappedID3D11UnorderedAccessView1() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11UnorderedAccessView1
  virtual void STDMETHODCALLTYPE GetDesc1(D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1)
  {
    ID3D11UnorderedAccessView1 *view1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11UnorderedAccessView1), (void **)&view1);

    if(SUCCEEDED(check) && view1)
      view1->GetDesc1(pDesc1);

    SAFE_RELEASE(view1);
  }
};

class WrappedShader
{
public:
  class ShaderEntry
  {
  public:
    ShaderEntry() : m_DebugInfoSearchPaths(NULL), m_DXBCFile(NULL) {}
    ShaderEntry(WrappedID3D11Device *device, ResourceId id, const byte *code, size_t codeLen)
    {
      m_ID = id;
      m_Bytecode.assign(code, code + codeLen);
      m_DebugInfoSearchPaths = device->GetShaderDebugInfoSearchPaths();
      m_DXBCFile = NULL;
    }
    ~ShaderEntry()
    {
      m_Bytecode.clear();
      SAFE_DELETE(m_DXBCFile);
    }

    void SetDebugInfoPath(const std::string &path) { m_DebugInfoPath = path; }
    DXBC::DXBCFile *GetDXBC()
    {
      if(m_DXBCFile == NULL && !m_Bytecode.empty())
      {
        TryReplaceOriginalByteCode();
        m_DXBCFile = new DXBC::DXBCFile((const void *)&m_Bytecode[0], m_Bytecode.size());
      }
      return m_DXBCFile;
    }

    ShaderReflection &GetDetails()
    {
      if(!m_Built && GetDXBC() != NULL)
        BuildReflection();
      m_Built = true;
      return m_Details;
    }

    const ShaderBindpointMapping &GetMapping()
    {
      if(!m_Built && GetDXBC() != NULL)
        BuildReflection();
      m_Built = true;
      return m_Mapping;
    }

  private:
    ShaderEntry(const ShaderEntry &e);
    void TryReplaceOriginalByteCode();
    ShaderEntry &operator=(const ShaderEntry &e);

    void BuildReflection();

    ResourceId m_ID;

    std::string m_DebugInfoPath;
    std::vector<std::string> *m_DebugInfoSearchPaths;

    std::vector<byte> m_Bytecode;

    bool m_Built = false;
    DXBC::DXBCFile *m_DXBCFile;
    ShaderReflection m_Details;
    ShaderBindpointMapping m_Mapping;
  };

  static std::map<ResourceId, ShaderEntry *> m_ShaderList;
  static Threading::CriticalSection m_ShaderListLock;

  WrappedShader(WrappedID3D11Device *device, ResourceId origId, ResourceId liveId, const byte *code,
                size_t codeLen)
      : m_ID(liveId)
  {
    SCOPED_LOCK(m_ShaderListLock);

    RDCASSERT(m_ShaderList.find(m_ID) == m_ShaderList.end());
    m_ShaderList[m_ID] =
        new ShaderEntry(device, origId != ResourceId() ? origId : liveId, code, codeLen);
  }
  virtual ~WrappedShader()
  {
    SCOPED_LOCK(m_ShaderListLock);

    auto it = m_ShaderList.find(m_ID);
    if(it != m_ShaderList.end())
    {
      delete it->second;
      m_ShaderList.erase(it);
    }
  }

  DXBC::DXBCFile *GetDXBC()
  {
    SCOPED_LOCK(m_ShaderListLock);
    return m_ShaderList[m_ID]->GetDXBC();
  }
  ShaderReflection &GetDetails()
  {
    SCOPED_LOCK(m_ShaderListLock);
    return m_ShaderList[m_ID]->GetDetails();
  }
  const ShaderBindpointMapping &GetMapping()
  {
    SCOPED_LOCK(m_ShaderListLock);
    return m_ShaderList[m_ID]->GetMapping();
  }

private:
  ResourceId m_ID;
};

template <class RealShaderType>
class WrappedID3D11Shader : public WrappedDeviceChild11<RealShaderType>, public WrappedShader
{
public:
  static const int AllocPoolCount = 32 * 1024;
  static const int AllocPoolMaxByteSize = 3 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Shader<RealShaderType>, AllocPoolCount,
                             AllocPoolMaxByteSize);

  WrappedID3D11Shader(RealShaderType *real, ResourceId origId, const byte *code, size_t codeLen,
                      WrappedID3D11Device *device)
      : WrappedDeviceChild11<RealShaderType>(real, device),
        WrappedShader(device, origId, GetResourceID(), code, codeLen)
  {
  }
  virtual ~WrappedID3D11Shader() { Shutdown(); }
};

class WrappedID3D11Counter : public WrappedDeviceChild11<ID3D11Counter>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Counter);

  WrappedID3D11Counter(ID3D11Counter *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11Counter() { Shutdown(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D11Asynchronous))
    {
      *ppvObject = (ID3D11Asynchronous *)this;
      AddRef();
      return S_OK;
    }

    return WrappedDeviceChild11<ID3D11Counter>::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11Asynchronous

  UINT STDMETHODCALLTYPE GetDataSize(void) { return m_pReal->GetDataSize(); }
  //////////////////////////////
  // implement ID3D11Counter

  void STDMETHODCALLTYPE GetDesc(__out D3D11_COUNTER_DESC *pDesc) { m_pReal->GetDesc(pDesc); }
};

class WrappedID3D11Query1 : public WrappedDeviceChild11<ID3D11Query, ID3D11Query1>
{
public:
  static const int AllocPoolCount = 16 * 1024;
  static const int AllocPoolMaxByteSize = 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Query1, AllocPoolCount, AllocPoolMaxByteSize);

  WrappedID3D11Query1(ID3D11Query *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11Query1() { Shutdown(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D11Asynchronous))
    {
      *ppvObject = (ID3D11Asynchronous *)this;
      AddRef();
      return S_OK;
    }

    return WrappedDeviceChild11::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11Asynchronous

  UINT STDMETHODCALLTYPE GetDataSize(void) { return m_pReal->GetDataSize(); }
  //////////////////////////////
  // implement ID3D11Query

  void STDMETHODCALLTYPE GetDesc(__out D3D11_QUERY_DESC *pDesc) { m_pReal->GetDesc(pDesc); }
  //////////////////////////////
  // implement ID3D11Query1
  virtual void STDMETHODCALLTYPE GetDesc1(D3D11_QUERY_DESC1 *pDesc1)
  {
    ID3D11Query1 *query1 = NULL;
    HRESULT check = m_pReal->QueryInterface(__uuidof(ID3D11Query1), (void **)&query1);

    if(SUCCEEDED(check) && query1)
      query1->GetDesc1(pDesc1);

    SAFE_RELEASE(query1);
  }
};

class WrappedID3D11Predicate : public WrappedDeviceChild11<ID3D11Predicate>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Predicate);

  WrappedID3D11Predicate(ID3D11Predicate *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11Predicate() { Shutdown(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D11Asynchronous))
    {
      *ppvObject = (ID3D11Asynchronous *)this;
      AddRef();
      return S_OK;
    }

    return WrappedDeviceChild11<ID3D11Predicate>::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11Asynchronous

  UINT STDMETHODCALLTYPE GetDataSize(void) { return m_pReal->GetDataSize(); }
  //////////////////////////////
  // implement ID3D11Query

  void STDMETHODCALLTYPE GetDesc(__out D3D11_QUERY_DESC *pDesc) { m_pReal->GetDesc(pDesc); }
};

class WrappedID3D11ClassInstance : public WrappedDeviceChild11<ID3D11ClassInstance>
{
private:
  ID3D11ClassLinkage *m_pLinkage;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11ClassInstance);

  WrappedID3D11ClassInstance(ID3D11ClassInstance *real, ID3D11ClassLinkage *linkage,
                             WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device), m_pLinkage(linkage)
  {
    SAFE_ADDREF(m_pLinkage);
  }
  virtual ~WrappedID3D11ClassInstance()
  {
    SAFE_RELEASE(m_pLinkage);
    Shutdown();
  }

  //////////////////////////////
  // implement ID3D11ClassInstance

  virtual void STDMETHODCALLTYPE GetClassLinkage(
      /* [annotation] */
      __out ID3D11ClassLinkage **ppLinkage)
  {
    if(ppLinkage)
    {
      SAFE_ADDREF(m_pLinkage);
      *ppLinkage = m_pLinkage;
    }
  }

  virtual void STDMETHODCALLTYPE GetDesc(
      /* [annotation] */
      __out D3D11_CLASS_INSTANCE_DESC *pDesc)
  {
    m_pReal->GetDesc(pDesc);
  }

  virtual void STDMETHODCALLTYPE GetInstanceName(
      /* [annotation] */
      __out_ecount_opt(*pBufferLength) LPSTR pInstanceName,
      /* [annotation] */
      __inout SIZE_T *pBufferLength)
  {
    m_pReal->GetInstanceName(pInstanceName, pBufferLength);
  }

  virtual void STDMETHODCALLTYPE GetTypeName(
      /* [annotation] */
      __out_ecount_opt(*pBufferLength) LPSTR pTypeName,
      /* [annotation] */
      __inout SIZE_T *pBufferLength)
  {
    m_pReal->GetTypeName(pTypeName, pBufferLength);
  }
};

class WrappedID3D11ClassLinkage : public WrappedDeviceChild11<ID3D11ClassLinkage>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11ClassLinkage);

  WrappedID3D11ClassLinkage(ID3D11ClassLinkage *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11ClassLinkage() { Shutdown(); }
  //////////////////////////////
  // implement ID3D11ClassLinkage

  virtual HRESULT STDMETHODCALLTYPE GetClassInstance(
      /* [annotation] */
      __in LPCSTR pClassInstanceName,
      /* [annotation] */
      __in UINT InstanceIndex,
      /* [annotation] */
      __out ID3D11ClassInstance **ppInstance)
  {
    if(ppInstance == NULL)
      return E_INVALIDARG;

    ID3D11ClassInstance *real = NULL;
    HRESULT hr = m_pReal->GetClassInstance(pClassInstanceName, InstanceIndex, &real);

    if(SUCCEEDED(hr) && real)
    {
      *ppInstance = m_pDevice->GetClassInstance(pClassInstanceName, InstanceIndex, this, &real);
    }
    else
    {
      SAFE_RELEASE(real);
    }

    return hr;
  }

  virtual HRESULT STDMETHODCALLTYPE CreateClassInstance(
      /* [annotation] */
      __in LPCSTR pClassTypeName,
      /* [annotation] */
      __in UINT ConstantBufferOffset,
      /* [annotation] */
      __in UINT ConstantVectorOffset,
      /* [annotation] */
      __in UINT TextureOffset,
      /* [annotation] */
      __in UINT SamplerOffset,
      /* [annotation] */
      __out ID3D11ClassInstance **ppInstance)
  {
    if(ppInstance == NULL)
      return E_INVALIDARG;

    ID3D11ClassInstance *real = NULL;
    HRESULT hr =
        m_pReal->CreateClassInstance(pClassTypeName, ConstantBufferOffset, ConstantVectorOffset,
                                     TextureOffset, SamplerOffset, &real);

    if(SUCCEEDED(hr) && real)
    {
      *ppInstance =
          m_pDevice->CreateClassInstance(pClassTypeName, ConstantBufferOffset, ConstantVectorOffset,
                                         TextureOffset, SamplerOffset, this, &real);
    }
    else
    {
      SAFE_RELEASE(real);
    }

    return hr;
  }
};

class WrappedID3D11DeviceContext;

class WrappedID3D11CommandList : public WrappedDeviceChild11<ID3D11CommandList>
{
  WrappedID3D11DeviceContext *m_pContext;
  bool m_Successful;    // indicates whether we have all of the commands serialised for this command
                        // list

  std::set<ResourceId> m_Dirty;
  std::set<ResourceId> m_References;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11CommandList);

  WrappedID3D11CommandList(ID3D11CommandList *real, WrappedID3D11Device *device,
                           WrappedID3D11DeviceContext *context, bool success)
      : WrappedDeviceChild11(real, device), m_pContext(context), m_Successful(success)
  {
    // context isn't defined type at this point.
  }
  virtual ~WrappedID3D11CommandList()
  {
    D3D11ResourceManager *manager = m_pDevice->GetResourceManager();
    // release the references we were holding
    for(ResourceId id : m_References)
    {
      D3D11ResourceRecord *record = manager->GetResourceRecord(id);
      if(record)
        record->Delete(manager);
    }

    // context isn't defined type at this point.
    Shutdown();
  }

  WrappedID3D11DeviceContext *GetContext() { return m_pContext; }
  bool IsCaptured() { return m_Successful; }
  void SwapReferences(std::set<ResourceId> &refs) { m_References.swap(refs); }
  void SwapDirtyResources(std::set<ResourceId> &dirty) { m_Dirty.swap(dirty); }
  void MarkDirtyResources(D3D11ResourceManager *manager)
  {
    for(auto it = m_Dirty.begin(); it != m_Dirty.end(); ++it)
      manager->MarkDirtyResource(*it);
  }
  void MarkDirtyResources(std::set<ResourceId> &missingTracks)
  {
    for(auto it = m_Dirty.begin(); it != m_Dirty.end(); ++it)
      missingTracks.insert(*it);
  }

  //////////////////////////////
  // implement ID3D11CommandList

  virtual UINT STDMETHODCALLTYPE GetContextFlags(void) { return m_pReal->GetContextFlags(); }
};

class WrappedID3DDeviceContextState : public WrappedDeviceChild11<ID3DDeviceContextState>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3DDeviceContextState);

  static std::vector<WrappedID3DDeviceContextState *> m_List;
  static Threading::CriticalSection m_Lock;
  D3D11RenderState *state;

  WrappedID3DDeviceContextState(ID3DDeviceContextState *real, WrappedID3D11Device *device);
  virtual ~WrappedID3DDeviceContextState();
};

class WrappedID3D11Fence : public WrappedDeviceChild11<ID3D11Fence>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11Fence);

  WrappedID3D11Fence(ID3D11Fence *real, WrappedID3D11Device *device)
      : WrappedDeviceChild11(real, device)
  {
  }
  virtual ~WrappedID3D11Fence() { Shutdown(); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    return WrappedDeviceChild11<ID3D11Fence>::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D11Fence
  virtual HRESULT STDMETHODCALLTYPE CreateSharedHandle(const SECURITY_ATTRIBUTES *pAttributes,
                                                       DWORD dwAccess, LPCWSTR lpName,
                                                       HANDLE *pHandle)
  {
    return m_pReal->CreateSharedHandle(pAttributes, dwAccess, lpName, pHandle);
  }

  virtual UINT64 STDMETHODCALLTYPE GetCompletedValue() { return m_pReal->GetCompletedValue(); }
  virtual HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value, HANDLE hEvent)
  {
    return m_pReal->SetEventOnCompletion(Value, hEvent);
  }
};

#define GET_RANGE(wrapped, unwrapped)                                    \
  template <>                                                            \
  inline const ResourceRange &GetResourceRange(unwrapped *v)             \
  {                                                                      \
    return v ? ((wrapped *)v)->GetResourceRange() : ResourceRange::Null; \
  }
GET_RANGE(WrappedID3D11RenderTargetView1, ID3D11RenderTargetView);
GET_RANGE(WrappedID3D11RenderTargetView1, ID3D11RenderTargetView1);
GET_RANGE(WrappedID3D11UnorderedAccessView1, ID3D11UnorderedAccessView);
GET_RANGE(WrappedID3D11UnorderedAccessView1, ID3D11UnorderedAccessView1);
GET_RANGE(WrappedID3D11ShaderResourceView1, ID3D11ShaderResourceView);
GET_RANGE(WrappedID3D11ShaderResourceView1, ID3D11ShaderResourceView1);
GET_RANGE(WrappedID3D11DepthStencilView, ID3D11DepthStencilView);

#define GET_VIEW_RESOURCE_RES_ID(wrapped, unwrapped)              \
  template <>                                                     \
  inline ResourceId GetViewResourceResID(unwrapped *v)            \
  {                                                               \
    return v ? ((wrapped *)v)->GetResourceResID() : ResourceId(); \
  }
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11RenderTargetView1, ID3D11RenderTargetView);
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11RenderTargetView1, ID3D11RenderTargetView1);
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11UnorderedAccessView1, ID3D11UnorderedAccessView);
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11UnorderedAccessView1, ID3D11UnorderedAccessView1);
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11ShaderResourceView1, ID3D11ShaderResourceView);
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11ShaderResourceView1, ID3D11ShaderResourceView1);
GET_VIEW_RESOURCE_RES_ID(WrappedID3D11DepthStencilView, ID3D11DepthStencilView);

// macro that only handles non-revisioned interfaces, 1:1 with its parent
#define GET_RES_ID(wrapped)                                    \
  template <>                                                  \
  inline ResourceId GetIDForResource(wrapped::InnerType *v)    \
  {                                                            \
    return v ? ((wrapped *)v)->GetResourceID() : ResourceId(); \
  }

// macro for interfaces with a '1' version that has two parents
#define GET_RES_ID1(wrapped)                                   \
  template <>                                                  \
  inline ResourceId GetIDForResource(wrapped::InnerType *v)    \
  {                                                            \
    return v ? ((wrapped *)v)->GetResourceID() : ResourceId(); \
  }                                                            \
  template <>                                                  \
  inline ResourceId GetIDForResource(wrapped::InnerType1 *v)   \
  {                                                            \
    return v ? ((wrapped *)v)->GetResourceID() : ResourceId(); \
  }

// macro for '2' interfaces with three parents
#define GET_RES_ID2(wrapped)                                   \
  template <>                                                  \
  inline ResourceId GetIDForResource(wrapped::InnerType *v)    \
  {                                                            \
    return v ? ((wrapped *)v)->GetResourceID() : ResourceId(); \
  }                                                            \
  template <>                                                  \
  inline ResourceId GetIDForResource(wrapped::InnerType1 *v)   \
  {                                                            \
    return v ? ((wrapped *)v)->GetResourceID() : ResourceId(); \
  }                                                            \
  template <>                                                  \
  inline ResourceId GetIDForResource(wrapped::InnerType2 *v)   \
  {                                                            \
    return v ? ((wrapped *)v)->GetResourceID() : ResourceId(); \
  }
GET_RES_ID(WrappedID3D11Buffer);
GET_RES_ID(WrappedID3D11Texture1D);
GET_RES_ID1(WrappedID3D11Texture2D1);
GET_RES_ID1(WrappedID3D11Texture3D1);
GET_RES_ID(WrappedID3D11InputLayout);
GET_RES_ID(WrappedID3D11SamplerState);
GET_RES_ID2(WrappedID3D11RasterizerState2);
GET_RES_ID(WrappedID3D11DepthStencilState);
GET_RES_ID1(WrappedID3D11BlendState1);
GET_RES_ID1(WrappedID3D11ShaderResourceView1);
GET_RES_ID1(WrappedID3D11UnorderedAccessView1);
GET_RES_ID1(WrappedID3D11RenderTargetView1);
GET_RES_ID(WrappedID3D11DepthStencilView);
GET_RES_ID(WrappedID3D11Shader<ID3D11VertexShader>);
GET_RES_ID(WrappedID3D11Shader<ID3D11HullShader>);
GET_RES_ID(WrappedID3D11Shader<ID3D11DomainShader>);
GET_RES_ID(WrappedID3D11Shader<ID3D11GeometryShader>);
GET_RES_ID(WrappedID3D11Shader<ID3D11PixelShader>);
GET_RES_ID(WrappedID3D11Shader<ID3D11ComputeShader>);
GET_RES_ID(WrappedID3D11Counter);
GET_RES_ID1(WrappedID3D11Query1);
GET_RES_ID(WrappedID3D11Predicate);
GET_RES_ID(WrappedID3D11ClassInstance);
GET_RES_ID(WrappedID3D11ClassLinkage);
GET_RES_ID(WrappedID3DDeviceContextState);
GET_RES_ID(WrappedID3D11CommandList);
GET_RES_ID(WrappedID3D11Fence);

// generic version that checks all the wrapped pools. We can use this for resource since it checks
// buffer and textures first, and also for purely virtual interfaces like asynchronous even though
// it's a little less efficient as we know we could narrow the set of types to search.
template <>
inline ResourceId GetIDForResource(ID3D11DeviceChild *v)
{
  return GetIDForDeviceChild(v);
}

template <>
inline ResourceId GetIDForResource(ID3D11Resource *v)
{
  return GetIDForDeviceChild(v);
}

template <>
inline ResourceId GetIDForResource(ID3D11Asynchronous *v)
{
  return GetIDForDeviceChild(v);
}

template <>
inline ResourceId GetIDForResource(ID3D11View *v)
{
  return GetIDForDeviceChild(v);
}