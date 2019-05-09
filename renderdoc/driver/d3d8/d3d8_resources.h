/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d8_device.h"
#include "d3d8_manager.h"

class TrackedResource8
{
public:
  TrackedResource8()
  {
    m_ID = ResourceIDGen::GetNewUniqueID();
    m_pRecord = NULL;
  }
  ResourceId GetResourceID() { return m_ID; }
  D3D8ResourceRecord *GetResourceRecord() { return m_pRecord; }
  void SetResourceRecord(D3D8ResourceRecord *record) { m_pRecord = record; }
private:
  TrackedResource8(const TrackedResource8 &);
  TrackedResource8 &operator=(const TrackedResource8 &);

  ResourceId m_ID;
  D3D8ResourceRecord *m_pRecord;
};

template <typename NestedType>
class WrappedIDirect3DResource8 : public RefCounter8, public NestedType, public TrackedResource8
{
protected:
  WrappedD3DDevice8 *m_pDevice;
  NestedType *m_pReal;
  unsigned int m_PipelineRefs;

  WrappedIDirect3DResource8(NestedType *real, WrappedD3DDevice8 *device)
      : RefCounter8(real), m_pDevice(device), m_pReal(real), m_PipelineRefs(0)
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

  virtual ~WrappedIDirect3DResource8()
  {
    // should have already called shutdown (needs to be called from child class to ensure
    // vtables are still in place when we call ReleaseResource)
    RDCASSERT(m_pDevice == NULL && m_pReal == NULL);
  }

public:
  typedef NestedType InnerType;

  NestedType *GetReal() { return m_pReal; }
  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter8::SoftRef(m_pDevice) - m_PipelineRefs; }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int piperefs = m_PipelineRefs;
    return RefCounter8::SoftRelease(m_pDevice) - piperefs;
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
    if(riid == __uuidof(IDirect3DResource8))
    {
      *ppvObject = (IDirect3DResource8 *)this;
      AddRef();
      return S_OK;
    }

    return RefCounter8::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement IDirect3DResource8

  HRESULT STDMETHODCALLTYPE GetDevice(__out IDirect3DDevice8 **ppDevice)
  {
    if(ppDevice)
    {
      *ppDevice = m_pDevice;
      m_pDevice->AddRef();
      return S_OK;
    }
    return E_INVALIDARG;
  }

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, void *pData, DWORD *pSizeOfData)
  {
    return m_pReal->GetPrivateData(guid, pData, pSizeOfData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, const void *pData, DWORD SizeOfData,
                                           DWORD Flags)
  {
    return m_pReal->SetPrivateData(guid, pData, SizeOfData, Flags);
  }

  HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID guid) { return m_pReal->FreePrivateData(guid); }
  DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew)
  {
    return m_pReal->SetPriority(PriorityNew);
  }

  DWORD STDMETHODCALLTYPE GetPriority() { return m_pReal->GetPriority(); }
  void STDMETHODCALLTYPE PreLoad() { return m_pReal->PreLoad(); }
  D3DRESOURCETYPE STDMETHODCALLTYPE GetType() { return m_pReal->GetType(); }
};

template <typename NestedType, typename DescType>
class WrappedD3DBuffer8 : public WrappedIDirect3DResource8<NestedType>
{
protected:
#if ENABLED(RDOC_DEVEL)
  DescType m_Desc;
#endif

public:
  struct BufferEntry
  {
    BufferEntry(WrappedD3DBuffer8 *b = NULL, uint32_t l = 0) : m_Buffer(b), length(l) {}
    WrappedD3DBuffer8 *m_Buffer;
    uint32_t length;
  };

  static std::map<ResourceId, BufferEntry> m_BufferList;

  WrappedD3DBuffer8(NestedType *real, uint32_t byteLength, WrappedD3DDevice8 *device)
      : WrappedIDirect3DResource8(real, device)
  {
#if ENABLED(RDOC_DEVEL)
    real->GetDesc(&m_Desc);
#endif
    SCOPED_LOCK(m_pDevice->D3DLock());

    RDCASSERT(m_BufferList.find(GetResourceID()) == m_BufferList.end());
    m_BufferList[GetResourceID()] = BufferEntry(this, byteLength);
  }

  virtual ~WrappedD3DBuffer8()
  {
    SCOPED_LOCK(m_pDevice->D3DLock());

    if(m_BufferList.find(GetResourceID()) != m_BufferList.end())
      m_BufferList.erase(GetResourceID());

    Shutdown();
  }

  HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags)
  {
    // TODO
    return m_pReal->Lock(OffsetToLock, SizeToLock, ppbData, Flags);
  }

  HRESULT STDMETHODCALLTYPE Unlock()
  {
    // TODO
    return m_pReal->Unlock();
  }

  HRESULT STDMETHODCALLTYPE GetDesc(DescType *pDesc) { return m_pReal->GetDesc(pDesc); }
};

class WrappedIDirect3DVertexBuffer8
    : public WrappedD3DBuffer8<IDirect3DVertexBuffer8, D3DVERTEXBUFFER_DESC>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedIDirect3DVertexBuffer8);

  enum
  {
    TypeEnum = Resource_VertexBuffer,
  };

  WrappedIDirect3DVertexBuffer8(IDirect3DVertexBuffer8 *real, uint32_t byteLength,
                                WrappedD3DDevice8 *device)
      : WrappedD3DBuffer8(real, byteLength, device)
  {
  }
};

class WrappedIDirect3DIndexBuffer8
    : public WrappedD3DBuffer8<IDirect3DIndexBuffer8, D3DINDEXBUFFER_DESC>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedIDirect3DIndexBuffer8);

  enum
  {
    TypeEnum = Resource_IndexBuffer,
  };

  WrappedIDirect3DIndexBuffer8(IDirect3DIndexBuffer8 *real, uint32_t byteLength,
                               WrappedD3DDevice8 *device)
      : WrappedD3DBuffer8(real, byteLength, device)
  {
  }
};

#define ALL_D3D8_TYPES                     \
  D3D8_TYPE_MACRO(IDirect3DVertexBuffer8); \
  D3D8_TYPE_MACRO(IDirect3DIndexBuffer8);

// template magic voodoo to unwrap types
template <typename inner>
struct UnwrapHelper
{
};

#undef D3D8_TYPE_MACRO
#define D3D8_TYPE_MACRO(iface)                                                          \
  template <>                                                                           \
  struct UnwrapHelper<iface>                                                            \
  {                                                                                     \
    typedef CONCAT(Wrapped, iface) Outer;                                               \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }                      \
    static D3D8ResourceType GetTypeEnum() { return (D3D8ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(iface *wrapped) { return (Outer *)wrapped; }               \
  };                                                                                    \
  template <>                                                                           \
  struct UnwrapHelper<CONCAT(Wrapped, iface)>                                           \
  {                                                                                     \
    typedef CONCAT(Wrapped, iface) Outer;                                               \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }                      \
    static D3D8ResourceType GetTypeEnum() { return (D3D8ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(iface *wrapped) { return (Outer *)wrapped; }               \
  };

ALL_D3D8_TYPES;

D3D8ResourceType IdentifyTypeByPtr(IUnknown *ptr);

#define WRAPPING_DEBUG 0

template <typename iface>
typename UnwrapHelper<iface>::Outer *GetWrapped(iface *obj)
{
  if(obj == NULL)
    return NULL;

  typename UnwrapHelper<iface>::Outer *wrapped = UnwrapHelper<iface>::FromHandle(obj);

#if WRAPPING_DEBUG
  if(obj != NULL && !wrapped->IsAlloc(wrapped))
  {
    RDCERR("Trying to unwrap invalid type");
    return NULL;
  }
#endif

  return wrapped;
}

template <typename ifaceptr>
ifaceptr Unwrap(ifaceptr obj)
{
  if(obj == NULL)
    return NULL;

  return GetWrapped(obj)->GetReal();
}

template <typename ifaceptr>
ResourceId GetResID(ifaceptr obj)
{
  if(obj == NULL)
    return ResourceId();

  return GetWrapped(obj)->GetResourceID();
}

template <typename ifaceptr>
D3D8ResourceRecord *GetRecord(ifaceptr obj)
{
  if(obj == NULL)
    return NULL;

  return GetWrapped(obj)->GetResourceRecord();
}

// specialisations that use the IsAlloc() function to identify the real type
template <>
ResourceId GetResID(IUnknown *ptr);
template <>
IUnknown *Unwrap(IUnknown *ptr);
template <>
D3D8ResourceRecord *GetRecord(IUnknown *ptr);