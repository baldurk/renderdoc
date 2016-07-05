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

#pragma once

#include "driver/shaders/dxbc/dxbc_inspect.h"
#include "d3d12_device.h"
#include "d3d12_manager.h"

enum ResourceType
{
  Resource_Unknown = 0,
  Resource_CommandAllocator,
  Resource_CommandQueue,
  Resource_CommandSignature,
  Resource_DescriptorHeap,
  Resource_Fence,
  Resource_Heap,
  Resource_PipelineState,
  Resource_QueryHeap,
  Resource_Resource,
  Resource_GraphicsCommandList,
  Resource_RootSignature,
};

class TrackedResource
{
public:
  TrackedResource() { m_ID = ResourceIDGen::GetNewUniqueID(); }
  ResourceId GetResourceID() { return m_ID; }
  D3D12ResourceRecord *GetResourceRecord() { return m_pRecord; }
private:
  TrackedResource(const TrackedResource &);
  TrackedResource &operator=(const TrackedResource &);

  ResourceId m_ID;
  D3D12ResourceRecord *m_pRecord;
};

extern const GUID RENDERDOC_ID3D11ShaderGUID_ShaderDebugMagicValue;

template <typename NestedType, typename NestedType1 = NestedType, typename NestedType2 = NestedType1>
class WrappedDeviceChild12 : public RefCounter12<NestedType>, public NestedType2, public TrackedResource
{
protected:
  WrappedID3D12Device *m_pDevice;

  WrappedDeviceChild12(NestedType *real, WrappedID3D12Device *device)
      : RefCounter12(real), m_pDevice(device)
  {
    m_pDevice->SoftRef();

    bool ret = m_pDevice->GetResourceManager()->AddWrapper(this, real);
    if(!ret)
      RDCERR("Error adding wrapper for type %s", ToStr::Get(__uuidof(NestedType)).c_str());

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

  virtual ~WrappedDeviceChild12()
  {
    // should have already called shutdown (needs to be called from child class to ensure
    // vtables are still in place when we call ReleaseResource)
    RDCASSERT(m_pDevice == NULL && m_pReal == NULL);
  }

public:
  typedef NestedType InnerType;

  NestedType *GetReal() { return m_pReal; }
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::SoftRef(m_pDevice); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::SoftRelease(m_pDevice); }
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
    if(riid == __uuidof(ID3D12Object))
    {
      *ppvObject = (ID3D12DeviceChild *)this;
      AddRef();
      return S_OK;
    }
    if(riid == __uuidof(ID3D12DeviceChild))
    {
      *ppvObject = (ID3D12DeviceChild *)this;
      AddRef();
      return S_OK;
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

      auto dxgiWrapper = new WrappedDXGIInterface<WrappedDeviceChild12>(this, m_pDevice);

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
        RDCWARN("Unexpected guid %s", ToStr::Get(riid).c_str());
        SAFE_DELETE(dxgiWrapper);
      }

      return S_OK;
    }

    return RefCounter12::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D12Object

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
  {
    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
  {
    if(guid == RENDERDOC_ID3D11ShaderGUID_ShaderDebugMagicValue)
      return m_pDevice->SetShaderDebugPath(this, (const char *)pData);

    if(guid == WKPDID_D3DDebugObjectName)
      m_pDevice->SetResourceName(this, (const char *)pData);

    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    string utf8 = StringFormat::Wide2UTF8(Name);
    m_pDevice->SetResourceName(this, utf8.c_str());

    return m_pReal->SetName(Name);
  }

  //////////////////////////////
  // implement ID3D12DeviceChild

  virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    if(riid == __uuidof(ID3D12Device) && ppvDevice)
    {
      *ppvDevice = (ID3D12Device *)m_pDevice;
      m_pDevice->AddRef();
    }
    else if(riid != __uuidof(ID3D12Device))
    {
      return E_NOINTERFACE;
    }

    return E_INVALIDARG;
  }
};

class WrappedID3D12CommandAllocator : public WrappedDeviceChild12<ID3D12CommandAllocator>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandAllocator);

  enum
  {
    TypeEnum = Resource_CommandAllocator,
  };

  WrappedID3D12CommandAllocator(ID3D12CommandAllocator *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12CommandAllocator() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12CommandAllocator

  virtual HRESULT STDMETHODCALLTYPE Reset() { return m_pReal->Reset(); }
};

class WrappedID3D12CommandSignature : public WrappedDeviceChild12<ID3D12CommandSignature>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandSignature);

  enum
  {
    TypeEnum = Resource_CommandSignature,
  };

  WrappedID3D12CommandSignature(ID3D12CommandSignature *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12CommandSignature() { Shutdown(); }
};

class WrappedID3D12DescriptorHeap : public WrappedDeviceChild12<ID3D12DescriptorHeap>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12DescriptorHeap);

  enum
  {
    TypeEnum = Resource_DescriptorHeap,
  };

  WrappedID3D12DescriptorHeap(ID3D12DescriptorHeap *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12DescriptorHeap() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12DescriptorHeap

  virtual D3D12_DESCRIPTOR_HEAP_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
  virtual D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart()
  {
    return m_pReal->GetCPUDescriptorHandleForHeapStart();
  }

  virtual D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart()
  {
    return m_pReal->GetGPUDescriptorHandleForHeapStart();
  }
};

class WrappedID3D12Fence : public WrappedDeviceChild12<ID3D12Fence>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Fence);

  enum
  {
    TypeEnum = Resource_Fence,
  };

  WrappedID3D12Fence(ID3D12Fence *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12Fence() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12Fence

  virtual UINT64 STDMETHODCALLTYPE GetCompletedValue() { return m_pReal->GetCompletedValue(); }
  virtual HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value, HANDLE hEvent)
  {
    return m_pReal->SetEventOnCompletion(Value, hEvent);
  }

  virtual HRESULT STDMETHODCALLTYPE Signal(UINT64 Value) { return m_pReal->Signal(Value); }
};

class WrappedID3D12Heap : public WrappedDeviceChild12<ID3D12Heap>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Heap);

  enum
  {
    TypeEnum = Resource_Heap,
  };

  WrappedID3D12Heap(ID3D12Heap *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12Heap() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12Heap

  virtual D3D12_HEAP_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
};

class WrappedID3D12PipelineState : public WrappedDeviceChild12<ID3D12PipelineState>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12PipelineState);

  enum
  {
    TypeEnum = Resource_PipelineState,
  };

  WrappedID3D12PipelineState(ID3D12PipelineState *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12PipelineState() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12PipelineState

  virtual HRESULT STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **ppBlob)
  {
    return m_pReal->GetCachedBlob(ppBlob);
  }
};

class WrappedID3D12QueryHeap : public WrappedDeviceChild12<ID3D12QueryHeap>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12QueryHeap);

  enum
  {
    TypeEnum = Resource_QueryHeap,
  };

  WrappedID3D12QueryHeap(ID3D12QueryHeap *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12QueryHeap() { Shutdown(); }
};

class WrappedID3D12Resource : public WrappedDeviceChild12<ID3D12Resource>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Resource);

  enum
  {
    TypeEnum = Resource_Resource,
  };

  WrappedID3D12Resource(ID3D12Resource *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12Resource() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12Resource

  virtual HRESULT STDMETHODCALLTYPE Map(UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData)
  {
    return m_pReal->Map(Subresource, pReadRange, ppData);
  }

  virtual void STDMETHODCALLTYPE Unmap(UINT Subresource, const D3D12_RANGE *pWrittenRange)
  {
    return m_pReal->Unmap(Subresource, pWrittenRange);
  }

  virtual D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
  virtual D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress()
  {
    return m_pReal->GetGPUVirtualAddress();
  }

  virtual HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource, const D3D12_BOX *pDstBox,
                                                       const void *pSrcData, UINT SrcRowPitch,
                                                       UINT SrcDepthPitch)
  {
    return m_pReal->WriteToSubresource(DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
  }

  virtual HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch,
                                                        UINT DstDepthPitch, UINT SrcSubresource,
                                                        const D3D12_BOX *pSrcBox)
  {
    return m_pReal->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource,
                                        pSrcBox);
  }

  virtual HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *pHeapProperties,
                                                      D3D12_HEAP_FLAGS *pHeapFlags)
  {
    return m_pReal->GetHeapProperties(pHeapProperties, pHeapFlags);
  }
};

class WrappedID3D12RootSignature : public WrappedDeviceChild12<ID3D12RootSignature>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12RootSignature);

  enum
  {
    TypeEnum = Resource_RootSignature,
  };

  WrappedID3D12RootSignature(ID3D12RootSignature *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12RootSignature() { Shutdown(); }
};

#define ALL_D3D12_TYPES                     \
  D3D12_TYPE_MACRO(ID3D12CommandAllocator); \
  D3D12_TYPE_MACRO(ID3D12CommandSignature); \
  D3D12_TYPE_MACRO(ID3D12DescriptorHeap);   \
  D3D12_TYPE_MACRO(ID3D12Fence);            \
  D3D12_TYPE_MACRO(ID3D12Heap);             \
  D3D12_TYPE_MACRO(ID3D12PipelineState);    \
  D3D12_TYPE_MACRO(ID3D12QueryHeap);        \
  D3D12_TYPE_MACRO(ID3D12Resource);         \
  D3D12_TYPE_MACRO(ID3D12RootSignature);

// template magic voodoo to unwrap types
template <typename inner>
struct UnwrapHelper
{
};

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)                                                 \
  template <>                                                                   \
  struct UnwrapHelper<iface>                                                    \
  {                                                                             \
    typedef CONCAT(Wrapped, iface) Outer;                                       \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }              \
    static ResourceType GetTypeEnum() { return (ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(iface *wrapped) { return (Outer *)wrapped; }       \
  };                                                                            \
  template <>                                                                   \
  struct UnwrapHelper<CONCAT(Wrapped, iface)>                                   \
  {                                                                             \
    typedef CONCAT(Wrapped, iface) Outer;                                       \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }              \
    static ResourceType GetTypeEnum() { return (ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(iface *wrapped) { return (Outer *)wrapped; }       \
  };

ALL_D3D12_TYPES;

ResourceType IdentifyTypeByPtr(ID3D12DeviceChild *ptr);

#define WRAPPING_DEBUG 1

template <typename iface>
typename UnwrapHelper<iface>::Outer *GetWrapped(iface *obj)
{
  if(obj == NULL)
    return NULL;

  typename UnwrapHelper<iface>::Outer *wrapped = UnwrapHelper<iface>::FromHandle(obj);

#if WRAPPING_DEBUG
  if(obj != NULL && !wrapped->IsAlloc(wrapped))
  {
    RDCWARN("Trying to unwrap invalid type");
    return NULL;
  }
#endif

  return wrapped;
}

// this function identifies the type by IsAlloc() and returns
// the pointer to the TrackedResource interface for fetching ID
// or record
TrackedResource *GetTracked(ID3D12DeviceChild *ptr);

template <typename iface>
iface *Unwrap(iface *obj)
{
  if(obj == NULL)
    return NULL;

  auto wrapped = GetWrapped(obj);

  if(wrapped)
    return wrapped->GetReal();

  return NULL;
}

template <typename iface>
ResourceId GetResID(iface *obj)
{
  if(obj == NULL)
    return ResourceId();

  auto wrapped = GetWrapped(obj);

  if(wrapped)
    return wrapped->GetResourceID();

  return ResourceId();
}

template <typename iface>
D3D12ResourceRecord *GetRecord(iface *obj)
{
  if(obj == NULL)
    return NULL;

  auto wrapped = GetWrapped(obj);

  if(wrapped)
    return wrapped->GetResourceRecord();

  return NULL;
}

// specialisations that use the GetTracked() function to fetch the ID
template <>
ResourceId GetResID(ID3D12DeviceChild *ptr);
template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr);
