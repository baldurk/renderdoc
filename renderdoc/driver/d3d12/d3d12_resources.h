/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "driver/shaders/dxbc/dxbc_container.h"
#include "serialise/serialiser.h"
#include "d3d12_device.h"
#include "d3d12_manager.h"

UINT GetPlaneForSubresource(ID3D12Resource *res, int Subresource);
UINT GetMipForSubresource(ID3D12Resource *res, int Subresource);
UINT GetSliceForSubresource(ID3D12Resource *res, int Subresource);
UINT GetMipForDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC &dsv);
UINT GetSliceForDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC &dsv);
UINT GetMipForRtv(const D3D12_RENDER_TARGET_VIEW_DESC &rtv);
UINT GetSliceForRtv(const D3D12_RENDER_TARGET_VIEW_DESC &rtv);

D3D12_SHADER_RESOURCE_VIEW_DESC MakeSRVDesc(const D3D12_RESOURCE_DESC &desc);
D3D12_UNORDERED_ACCESS_VIEW_DESC MakeUAVDesc(const D3D12_RESOURCE_DESC &desc);

class TrackedResource12
{
public:
  TrackedResource12()
  {
    m_ID = ResourceIDGen::GetNewUniqueID();
    m_pRecord = NULL;
  }
  ResourceId GetResourceID() { return m_ID; }
  D3D12ResourceRecord *GetResourceRecord() { return m_pRecord; }
  void SetResourceRecord(D3D12ResourceRecord *record) { m_pRecord = record; }
protected:
  TrackedResource12(const TrackedResource12 &);
  TrackedResource12 &operator=(const TrackedResource12 &);

  ResourceId m_ID;
  D3D12ResourceRecord *m_pRecord;
};

extern const GUID RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue;

template <typename NestedType, typename NestedType1 = NestedType, typename NestedType2 = NestedType1>
class WrappedDeviceChild12 : public RefCounter12<NestedType>,
                             public NestedType2,
                             public TrackedResource12
{
protected:
  WrappedID3D12Device *m_pDevice;

  WrappedDeviceChild12(NestedType *real, WrappedID3D12Device *device)
      : RefCounter12(real), m_pDevice(device)
  {
    m_pDevice->SoftRef();

    if(real)
    {
      bool ret = m_pDevice->GetResourceManager()->AddWrapper(this, real);
      if(!ret)
        RDCERR("Error adding wrapper for type %s", ToStr(__uuidof(NestedType)).c_str());
    }

    m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);
  }

  void Shutdown()
  {
    if(m_pReal)
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
    else if(riid == __uuidof(NestedType))
    {
      *ppvObject = (NestedType *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(NestedType1))
    {
      if(!m_pReal)
        return E_NOINTERFACE;

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
    else if(riid == __uuidof(NestedType2))
    {
      if(!m_pReal)
        return E_NOINTERFACE;

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
    else if(riid == __uuidof(ID3D12Pageable))
    {
      // not all child classes support this, so check it on the real interface
      if(!m_pReal)
        return E_NOINTERFACE;

      // check that the real interface supports this
      ID3D12Pageable *dummy = NULL;
      HRESULT check = m_pReal->QueryInterface(riid, (void **)&dummy);

      SAFE_RELEASE(dummy);

      if(FAILED(check))
        return check;

      *ppvObject = (ID3D12Pageable *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12Object))
    {
      *ppvObject = (ID3D12DeviceChild *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12DeviceChild))
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
      if(m_pReal == NULL)
        return E_NOINTERFACE;

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
        RDCWARN("Unexpected guid %s", ToStr(riid).c_str());
        SAFE_DELETE(dxgiWrapper);
      }

      return S_OK;
    }

    return RefCounter12::QueryInterface("ID3D12DeviceChild", riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D12Object

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
  {
    if(!m_pReal)
    {
      if(pDataSize)
        *pDataSize = 0;
      return S_OK;
    }

    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
  {
    if(guid == RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue)
      return m_pDevice->SetShaderDebugPath(this, (const char *)pData);

    if(guid == WKPDID_D3DDebugObjectName)
    {
      m_pDevice->SetName(this, (const char *)pData);
    }
    else if(guid == WKPDID_D3DDebugObjectNameW)
    {
      rdcwstr wName((const wchar_t *)pData, DataSize / 2);
      rdcstr sName = StringFormat::Wide2UTF8(wName);
      m_pDevice->SetName(this, sName.c_str());
    }

    if(!m_pReal)
      return S_OK;

    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    if(!m_pReal)
      return S_OK;

    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    rdcstr utf8 = Name ? StringFormat::Wide2UTF8(Name) : "";
    m_pDevice->SetName(this, utf8.c_str());

    if(!m_pReal)
      return S_OK;

    return m_pReal->SetName(Name);
  }

  //////////////////////////////
  // implement ID3D12DeviceChild

  virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    return m_pDevice->GetDevice(riid, ppvDevice);
  }
};

class WrappedID3D12CommandAllocator : public WrappedDeviceChild12<ID3D12CommandAllocator>
{
  static int32_t m_ResetEnabled;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandAllocator);

  ChunkAllocator *alloc = NULL;
  bool m_Internal = false;

  enum
  {
    TypeEnum = Resource_CommandAllocator,
  };

  WrappedID3D12CommandAllocator(ID3D12CommandAllocator *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12CommandAllocator() { Shutdown(); }
  static void PauseResets() { Atomic::Dec32(&m_ResetEnabled); }
  static void ResumeResets() { Atomic::Inc32(&m_ResetEnabled); }
  //////////////////////////////
  // implement ID3D12CommandAllocator

  virtual HRESULT STDMETHODCALLTYPE Reset()
  {
    // reset the allocator. D3D12 munges the pool and the allocator together, so the allocator
    // becomes redundant as the only pool client and the pool is reset together.
    if(Atomic::CmpExch32(&m_ResetEnabled, 1, 1) == 1 && alloc)
      alloc->Reset();
    return m_pReal->Reset();
  }
};

class WrappedID3D12CommandSignature : public WrappedDeviceChild12<ID3D12CommandSignature>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandSignature);

  D3D12CommandSignature sig;

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

struct D3D12Descriptor;

class WrappedID3D12DescriptorHeap : public WrappedDeviceChild12<ID3D12DescriptorHeap>
{
  D3D12_CPU_DESCRIPTOR_HANDLE realCPUBase;
  D3D12_GPU_DESCRIPTOR_HANDLE realGPUBase;

  UINT increment;
  UINT numDescriptors;

  D3D12Descriptor *descriptors;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12DescriptorHeap);

  enum
  {
    TypeEnum = Resource_DescriptorHeap,
  };

  WrappedID3D12DescriptorHeap(ID3D12DescriptorHeap *real, WrappedID3D12Device *device,
                              const D3D12_DESCRIPTOR_HEAP_DESC &desc, UINT UnpatchedNumDescriptors);
  virtual ~WrappedID3D12DescriptorHeap();

  D3D12Descriptor *GetDescriptors() { return descriptors; }
  UINT GetNumDescriptors() { return numDescriptors; }
  //////////////////////////////
  // implement ID3D12DescriptorHeap

  virtual D3D12_DESCRIPTOR_HEAP_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
  virtual D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart()
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = (SIZE_T)descriptors;
    return handle;
  }

  virtual D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart()
  {
    D3D12_GPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = (UINT64)descriptors;
    return handle;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE GetCPU(uint32_t idx)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = realCPUBase;
    handle.ptr += idx * increment;
    return handle;
  }

  D3D12_GPU_DESCRIPTOR_HANDLE GetGPU(uint32_t idx)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = realGPUBase;
    handle.ptr += idx * increment;
    return handle;
  }
};

class WrappedID3D12Fence : public WrappedDeviceChild12<ID3D12Fence, ID3D12Fence1>
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
  //////////////////////////////
  // implement ID3D12Fence1
  virtual D3D12_FENCE_FLAGS STDMETHODCALLTYPE GetCreationFlags()
  {
    ID3D12Fence1 *real1 = NULL;
    m_pReal->QueryInterface(__uuidof(ID3D12Fence1), (void **)&real1);

    if(!real1)
      return D3D12_FENCE_FLAG_NONE;

    D3D12_FENCE_FLAGS ret = real1->GetCreationFlags();

    SAFE_RELEASE(real1);
    return ret;
  }
};

class WrappedID3D12ProtectedResourceSession
    : public WrappedDeviceChild12<ID3D12ProtectedResourceSession, ID3D12ProtectedResourceSession1>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12ProtectedResourceSession);

  enum
  {
    TypeEnum = Resource_ProtectedResourceSession,
  };

  WrappedID3D12ProtectedResourceSession(ID3D12ProtectedResourceSession *real,
                                        WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12ProtectedResourceSession() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12ProtectedSession
  virtual HRESULT STDMETHODCALLTYPE GetStatusFence(REFIID riid, _COM_Outptr_opt_ void **ppFence)
  {
    if(riid != __uuidof(ID3D12Fence) && riid != __uuidof(ID3D12Fence1))
    {
      RDCERR("Unsupported fence interface %s", ToStr(riid).c_str());
      return E_NOINTERFACE;
    }

    void *iface = NULL;
    HRESULT ret = m_pReal->GetStatusFence(riid, &iface);

    if(ret != S_OK)
      return ret;

    ID3D12Fence *fence = NULL;

    if(riid == __uuidof(ID3D12Fence))
      fence = (ID3D12Fence *)iface;
    else if(riid == __uuidof(ID3D12Fence1))
      fence = (ID3D12Fence *)(ID3D12Fence1 *)iface;

    *ppFence = m_pDevice->CreateProtectedSessionFence(fence);
    return S_OK;
  }

  virtual D3D12_PROTECTED_SESSION_STATUS STDMETHODCALLTYPE GetSessionStatus(void)
  {
    return m_pReal->GetSessionStatus();
  }

  //////////////////////////////
  // implement ID3D12ProtectedResourceSession
  virtual D3D12_PROTECTED_RESOURCE_SESSION_DESC STDMETHODCALLTYPE GetDesc(void)
  {
    return m_pReal->GetDesc();
  }

  //////////////////////////////
  // implement ID3D12ProtectedResourceSession1
  virtual D3D12_PROTECTED_RESOURCE_SESSION_DESC1 STDMETHODCALLTYPE GetDesc1()
  {
    ID3D12ProtectedResourceSession1 *real1 = NULL;
    m_pReal->QueryInterface(__uuidof(ID3D12ProtectedResourceSession1), (void **)&real1);

    if(!real1)
      return {};

    D3D12_PROTECTED_RESOURCE_SESSION_DESC1 ret = real1->GetDesc1();

    SAFE_RELEASE(real1);
    return ret;
  }
};

class WrappedID3D12Heap : public WrappedDeviceChild12<ID3D12Heap, ID3D12Heap1>
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
  //////////////////////////////
  // implement ID3D12Heap1
  virtual HRESULT STDMETHODCALLTYPE
  GetProtectedResourceSession(REFIID riid, _COM_Outptr_opt_ void **ppProtectedSession)
  {
    ID3D12Heap1 *real1 = NULL;
    m_pReal->QueryInterface(__uuidof(ID3D12Heap1), (void **)&real1);

    if(!real1)
      return E_NOINTERFACE;

    void *iface = NULL;
    HRESULT ret = real1->GetProtectedResourceSession(riid, &iface);

    SAFE_RELEASE(real1);

    if(ret != S_OK)
      return ret;

    if(riid == __uuidof(ID3D12ProtectedResourceSession))
    {
      *ppProtectedSession = new WrappedID3D12ProtectedResourceSession(
          (ID3D12ProtectedResourceSession *)iface, m_pDevice);
    }
    else
    {
      RDCERR("Unsupported interface %s", ToStr(riid).c_str());
      return E_NOINTERFACE;
    }

    return S_OK;
  }
};

class WrappedID3D12PipelineState : public WrappedDeviceChild12<ID3D12PipelineState>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12PipelineState);

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC *graphics = NULL;
  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC *compute = NULL;

  void Fill(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &desc)
  {
    if(graphics)
    {
      desc = *graphics;
      if(VS())
        desc.VS = VS()->GetDesc();
      if(HS())
        desc.HS = HS()->GetDesc();
      if(DS())
        desc.DS = DS()->GetDesc();
      if(GS())
        desc.GS = GS()->GetDesc();
      if(PS())
        desc.PS = PS()->GetDesc();
    }
    else
    {
      desc = *compute;
      desc.CS = CS()->GetDesc();
    }
  }

  bool IsGraphics() { return graphics != NULL; }
  bool IsCompute() { return compute != NULL; }
  struct DXBCKey
  {
    DXBCKey(const D3D12_SHADER_BYTECODE &byteCode)
    {
      byteLen = (uint32_t)byteCode.BytecodeLength;
      DXBC::DXBCContainer::GetHash(hash, byteCode.pShaderBytecode, byteCode.BytecodeLength);
    }

    // assume that byte length + hash is enough to uniquely identify a shader bytecode
    uint32_t byteLen;
    uint32_t hash[4];

    bool operator<(const DXBCKey &o) const
    {
      if(byteLen != o.byteLen)
        return byteLen < o.byteLen;

      for(size_t i = 0; i < 4; i++)
        if(hash[i] != o.hash[i])
          return hash[i] < o.hash[i];

      return false;
    }

    bool operator==(const DXBCKey &o) const
    {
      return byteLen == o.byteLen && hash[0] == o.hash[0] && hash[1] == o.hash[1] &&
             hash[2] == o.hash[2] && hash[3] == o.hash[3];
    }
  };

  class ShaderEntry : public WrappedDeviceChild12<ID3D12DeviceChild>
  {
  public:
    static bool m_InternalResources;

    static void InternalResources(bool internalResources)
    {
      m_InternalResources = internalResources;
    }

    ShaderEntry(const D3D12_SHADER_BYTECODE &byteCode, WrappedID3D12Device *device)
        : WrappedDeviceChild12(NULL, device), m_Key(byteCode)
    {
      m_Bytecode.assign((const byte *)byteCode.pShaderBytecode, byteCode.BytecodeLength);
      m_DXBCFile = NULL;

      device->GetResourceManager()->AddLiveResource(GetResourceID(), this);

      if(!m_InternalResources)
      {
        device->AddResource(GetResourceID(), ResourceType::Shader, "Shader");

        ResourceDescription &desc = device->GetResourceDesc(GetResourceID());
        // this will be appended to in the function above.
        desc.initialisationChunks.clear();

        // since these don't have live IDs, let's use the first uint of the hash as the name. Slight
        // chance of collision but not that bad.
        desc.name = StringFormat::Fmt("Shader {%08x}", m_Key.hash[0]);
      }

      m_Built = false;
    }

    virtual ~ShaderEntry()
    {
      m_Shaders.erase(m_Key);
      m_Bytecode.clear();
      SAFE_DELETE(m_DXBCFile);
      Shutdown();
    }

    static ShaderEntry *AddShader(const D3D12_SHADER_BYTECODE &byteCode, WrappedID3D12Device *device)
    {
      DXBCKey key(byteCode);
      ShaderEntry *shader = m_Shaders[key];

      if(shader == NULL)
        shader = m_Shaders[key] = new ShaderEntry(byteCode, device);

      return shader;
    }

    static void ReleaseShader(ShaderEntry *shader)
    {
      if(shader == NULL)
        return;

      shader->Release();
    }

    static bool IsShader(ResourceId id)
    {
      for(auto it = m_Shaders.begin(); it != m_Shaders.end(); ++it)
        if(it->second->GetResourceID() == id)
          return true;

      return false;
    }

    void GetShaderExtSlot(uint32_t &slot, uint32_t &space)
    {
      slot = m_ShaderExtSlot;
      space = m_ShaderExtSpace;
    }

    void SetShaderExtSlot(uint32_t slot, uint32_t space)
    {
      // it doesn't make sense to build the same DXBC with different slots/spaces since it's baked
      // in.

      if(slot != m_ShaderExtSlot && m_ShaderExtSlot != ~0U)
        RDCERR(
            "Unexpected case - different valid slot %u being set for same shader already "
            "configured with slot %u",
            slot, m_ShaderExtSlot);
      if(space != m_ShaderExtSpace && m_ShaderExtSpace != ~0U)
        RDCERR(
            "Unexpected case - different valid space %u being set for same shader already "
            "configured with space %u",
            space, m_ShaderExtSpace);

      m_ShaderExtSlot = slot;
      m_ShaderExtSpace = space;
    }

    DXBCKey GetKey() { return m_Key; }
    D3D12_SHADER_BYTECODE GetDesc()
    {
      D3D12_SHADER_BYTECODE ret;
      ret.BytecodeLength = m_Bytecode.size();
      ret.pShaderBytecode = (const void *)&m_Bytecode[0];
      return ret;
    }

    DXBC::DXBCContainer *GetDXBC()
    {
      if(m_DXBCFile == NULL && !m_Bytecode.empty())
      {
        m_DXBCFile = new DXBC::DXBCContainer(m_Bytecode, rdcstr(), GraphicsAPI::D3D12,
                                             m_ShaderExtSlot, m_ShaderExtSpace);
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

    DXBCKey m_Key;

    bytebuf m_Bytecode;
    uint32_t m_ShaderExtSlot = ~0U, m_ShaderExtSpace = ~0U;

    bool m_Built;
    DXBC::DXBCContainer *m_DXBCFile;
    ShaderReflection m_Details;
    ShaderBindpointMapping m_Mapping;

    static std::map<DXBCKey, ShaderEntry *> m_Shaders;
  };

  enum
  {
    TypeEnum = Resource_PipelineState,
  };

  ShaderEntry *VS() { return (ShaderEntry *)graphics->VS.pShaderBytecode; }
  ShaderEntry *HS() { return (ShaderEntry *)graphics->HS.pShaderBytecode; }
  ShaderEntry *DS() { return (ShaderEntry *)graphics->DS.pShaderBytecode; }
  ShaderEntry *GS() { return (ShaderEntry *)graphics->GS.pShaderBytecode; }
  ShaderEntry *PS() { return (ShaderEntry *)graphics->PS.pShaderBytecode; }
  ShaderEntry *CS() { return (ShaderEntry *)compute->CS.pShaderBytecode; }
  WrappedID3D12PipelineState(ID3D12PipelineState *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
    if(IsReplayMode(m_pDevice->GetState()))
      m_pDevice->GetPipelineList().push_back(this);
  }
  virtual ~WrappedID3D12PipelineState()
  {
    if(IsReplayMode(m_pDevice->GetState()))
      m_pDevice->GetPipelineList().removeOne(this);

    Shutdown();

    if(graphics)
    {
      ShaderEntry::ReleaseShader(VS());
      ShaderEntry::ReleaseShader(HS());
      ShaderEntry::ReleaseShader(DS());
      ShaderEntry::ReleaseShader(GS());
      ShaderEntry::ReleaseShader(PS());

      SAFE_DELETE_ARRAY(graphics->InputLayout.pInputElementDescs);
      SAFE_DELETE_ARRAY(graphics->StreamOutput.pSODeclaration);
      SAFE_DELETE_ARRAY(graphics->StreamOutput.pBufferStrides);
      SAFE_DELETE_ARRAY(graphics->ViewInstancing.pViewInstanceLocations);

      SAFE_DELETE(graphics);
    }

    if(compute)
    {
      ShaderEntry::ReleaseShader(CS());

      SAFE_DELETE(compute);
    }
  }

  //////////////////////////////
  // implement ID3D12PipelineState

  virtual HRESULT STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **ppBlob)
  {
    return m_pReal->GetCachedBlob(ppBlob);
  }
};

typedef WrappedID3D12PipelineState::ShaderEntry WrappedID3D12Shader;

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

class WrappedID3D12Resource
    : public WrappedDeviceChild12<ID3D12Resource, ID3D12Resource1, ID3D12Resource2>
{
  static GPUAddressRangeTracker m_Addresses;

  WriteSerialiser &GetThreadSerialiser();

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Resource, false);

  static void RefBuffers(D3D12ResourceManager *rm);

  static rdcarray<ID3D12Resource *> AddRefBuffersBeforeCapture(D3D12ResourceManager *rm);

  static void GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr, ResourceId &id, UINT64 &offs)
  {
    m_Addresses.GetResIDFromAddr(addr, id, offs);
  }

  // overload to just return the id in case the offset isn't needed
  static ResourceId GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr)
  {
    ResourceId id;
    UINT64 offs;

    m_Addresses.GetResIDFromAddr(addr, id, offs);

    return id;
  }

  enum
  {
    TypeEnum = Resource_Resource,
  };

  WrappedID3D12Resource(ID3D12Resource *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
    if(IsReplayMode(device->GetState()))
      device->GetResourceList()[GetResourceID()] = this;

    // assuming only valid for buffers
    if(m_pReal->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      D3D12_GPU_VIRTUAL_ADDRESS addr = m_pReal->GetGPUVirtualAddress();

      GPUAddressRange range;
      range.start = addr;
      range.end = addr + m_pReal->GetDesc().Width;
      range.id = GetResourceID();

      m_Addresses.AddTo(range);
    }
  }
  virtual ~WrappedID3D12Resource();

  byte *GetMap(UINT Subresource);
  byte *GetShadow(UINT Subresource);
  void AllocShadow(UINT Subresource, size_t size);
  void FreeShadow();
  void LockMaps();
  void UnlockMaps();

  virtual uint64_t GetGPUVirtualAddressIfBuffer()
  {
    if(m_pReal->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      return m_pReal->GetGPUVirtualAddress();
    return 0;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D12Resource2))
    {
      return E_NOINTERFACE;
    }
    return WrappedDeviceChild12::QueryInterface(riid, ppvObject);
  }

  //////////////////////////////
  // implement ID3D12Resource

  virtual D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
  virtual D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress()
  {
    return m_pReal->GetGPUVirtualAddress();
  }

  virtual HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *pHeapProperties,
                                                      D3D12_HEAP_FLAGS *pHeapFlags)
  {
    return m_pReal->GetHeapProperties(pHeapProperties, pHeapFlags);
  }

  virtual HRESULT STDMETHODCALLTYPE Map(UINT Subresource, const D3D12_RANGE *pReadRange,
                                        void **ppData);
  virtual void STDMETHODCALLTYPE Unmap(UINT Subresource, const D3D12_RANGE *pWrittenRange);
  virtual HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource, const D3D12_BOX *pDstBox,
                                                       const void *pSrcData, UINT SrcRowPitch,
                                                       UINT SrcDepthPitch);
  virtual HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch,
                                                        UINT DstDepthPitch, UINT SrcSubresource,
                                                        const D3D12_BOX *pSrcBox)
  {
    // don't have to do anything here
    return m_pReal->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource,
                                        pSrcBox);
  }

  //////////////////////////////
  // implement ID3D12Resource1
  virtual HRESULT STDMETHODCALLTYPE
  GetProtectedResourceSession(REFIID riid, _COM_Outptr_opt_ void **ppProtectedSession)
  {
    ID3D12Resource1 *real1 = NULL;
    m_pReal->QueryInterface(__uuidof(ID3D12Resource1), (void **)&real1);

    if(!real1)
      return E_NOINTERFACE;

    void *iface = NULL;
    HRESULT ret = real1->GetProtectedResourceSession(riid, &iface);

    SAFE_RELEASE(real1);

    if(ret != S_OK)
      return ret;

    if(riid == __uuidof(ID3D12ProtectedResourceSession))
    {
      *ppProtectedSession = new WrappedID3D12ProtectedResourceSession(
          (ID3D12ProtectedResourceSession *)iface, m_pDevice);
    }
    else
    {
      RDCERR("Unsupported interface %s", ToStr(riid).c_str());
      return E_NOINTERFACE;
    }

    return S_OK;
  }

  //////////////////////////////
  // implement ID3D12Resource2
  virtual D3D12_RESOURCE_DESC1 STDMETHODCALLTYPE GetDesc1(void)
  {
    ID3D12Resource2 *real2 = NULL;
    m_pReal->QueryInterface(__uuidof(ID3D12Resource2), (void **)&real2);

    if(!real2)
      return {};

    D3D12_RESOURCE_DESC1 ret = real2->GetDesc1();

    SAFE_RELEASE(real2);
    return ret;
  }
};

class WrappedID3D12RootSignature : public WrappedDeviceChild12<ID3D12RootSignature>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12RootSignature);

  D3D12RootSignature sig;

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

class WrappedID3D12PipelineLibrary : public WrappedDeviceChild12<ID3D12PipelineLibrary1>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12PipelineLibrary);

  enum
  {
    TypeEnum = Resource_PipelineLibrary,
  };

  WrappedID3D12PipelineLibrary(WrappedID3D12Device *device) : WrappedDeviceChild12(NULL, device) {}
  virtual ~WrappedID3D12PipelineLibrary() { Shutdown(); }
  virtual HRESULT STDMETHODCALLTYPE StorePipeline(_In_opt_ LPCWSTR pName,
                                                  _In_ ID3D12PipelineState *pPipeline)
  {
    // do nothing
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE
  LoadGraphicsPipeline(_In_ LPCWSTR pName, _In_ const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
                       REFIID riid, _COM_Outptr_ void **ppPipelineState)
  {
    // pretend we don't have it - assume that the application won't store then
    // load in the same run, or will handle that if it happens
    return E_INVALIDARG;
  }

  virtual HRESULT STDMETHODCALLTYPE
  LoadComputePipeline(_In_ LPCWSTR pName, _In_ const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
                      REFIID riid, _COM_Outptr_ void **ppPipelineState)
  {
    // pretend we don't have it - assume that the application won't store then
    // load in the same run, or will handle that if it happens
    return E_INVALIDARG;
  }

  static const SIZE_T DummyBytes = 32;

  virtual SIZE_T STDMETHODCALLTYPE GetSerializedSize(void)
  {
    // simple dummy serialisation since applications might not expect 0 bytes
    return DummyBytes;
  }

  virtual HRESULT STDMETHODCALLTYPE Serialize(_Out_writes_(DataSizeInBytes) void *pData,
                                              SIZE_T DataSizeInBytes)
  {
    if(DataSizeInBytes < DummyBytes)
      return E_INVALIDARG;

    memset(pData, 0, DummyBytes);
    return S_OK;
  }

  //////////////////////////////
  // implement ID3D12PipelineLibrary1

  virtual HRESULT STDMETHODCALLTYPE LoadPipeline(LPCWSTR pName,
                                                 const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc,
                                                 REFIID riid, void **ppPipelineState)
  {
    // pretend we don't have it - assume that the application won't store then
    // load in the same run, or will handle that if it happens
    return E_INVALIDARG;
  }
};

class WrappedID3D12ShaderCacheSession : public WrappedDeviceChild12<ID3D12ShaderCacheSession>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12ShaderCacheSession);

  enum
  {
    TypeEnum = Resource_ShaderCacheSession,
  };

  WrappedID3D12ShaderCacheSession(ID3D12ShaderCacheSession *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
  }
  virtual ~WrappedID3D12ShaderCacheSession() { Shutdown(); }
  //////////////////////////////
  // implement ID3D12ShaderCacheSession
  virtual HRESULT STDMETHODCALLTYPE FindValue(
      /* [annotation][in] */
      _In_reads_bytes_(KeySize) const void *pKey, UINT KeySize,
      /* [annotation][out] */
      _Out_writes_bytes_(*pValueSize) void *pValue, _Inout_ UINT *pValueSize)
  {
    return m_pReal->FindValue(pKey, KeySize, pValue, pValueSize);
  }

  virtual HRESULT STDMETHODCALLTYPE StoreValue(
      /* [annotation][in] */
      _In_reads_bytes_(KeySize) const void *pKey, UINT KeySize,
      /* [annotation][in] */
      _In_reads_bytes_(ValueSize) const void *pValue, UINT ValueSize)
  {
    return m_pReal->StoreValue(pKey, KeySize, pValue, ValueSize);
  }

  virtual void STDMETHODCALLTYPE SetDeleteOnDestroy(void) { m_pReal->SetDeleteOnDestroy(); }
  virtual D3D12_SHADER_CACHE_SESSION_DESC STDMETHODCALLTYPE GetDesc(void)
  {
    return m_pReal->GetDesc();
  }
};

#define ALL_D3D12_TYPES                             \
  D3D12_TYPE_MACRO(ID3D12CommandAllocator);         \
  D3D12_TYPE_MACRO(ID3D12CommandSignature);         \
  D3D12_TYPE_MACRO(ID3D12DescriptorHeap);           \
  D3D12_TYPE_MACRO(ID3D12Fence);                    \
  D3D12_TYPE_MACRO(ID3D12Heap);                     \
  D3D12_TYPE_MACRO(ID3D12PipelineState);            \
  D3D12_TYPE_MACRO(ID3D12QueryHeap);                \
  D3D12_TYPE_MACRO(ID3D12Resource);                 \
  D3D12_TYPE_MACRO(ID3D12RootSignature);            \
  D3D12_TYPE_MACRO(ID3D12PipelineLibrary);          \
  D3D12_TYPE_MACRO(ID3D12ProtectedResourceSession); \
  D3D12_TYPE_MACRO(ID3D12ShaderCacheSession);

// template magic voodoo to unwrap types
template <typename inner>
struct UnwrapHelper
{
};

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)                                                           \
  template <>                                                                             \
  struct UnwrapHelper<iface>                                                              \
  {                                                                                       \
    typedef CONCAT(Wrapped, iface) Outer;                                                 \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }                        \
    static D3D12ResourceType GetTypeEnum() { return (D3D12ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(iface *wrapped) { return (Outer *)wrapped; }                 \
  };                                                                                      \
  template <>                                                                             \
  struct UnwrapHelper<CONCAT(Wrapped, iface)>                                             \
  {                                                                                       \
    typedef CONCAT(Wrapped, iface) Outer;                                                 \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }                        \
    static D3D12ResourceType GetTypeEnum() { return (D3D12ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(iface *wrapped) { return (Outer *)wrapped; }                 \
  };

ALL_D3D12_TYPES;

// extra helpers here for '1' or '2' extended interfaces
#define D3D12_UNWRAP_EXTENDED(iface, ifaceX)                                              \
  template <>                                                                             \
  struct UnwrapHelper<ifaceX>                                                             \
  {                                                                                       \
    typedef CONCAT(Wrapped, iface) Outer;                                                 \
    static bool IsAlloc(void *ptr) { return Outer::IsAlloc(ptr); }                        \
    static D3D12ResourceType GetTypeEnum() { return (D3D12ResourceType)Outer::TypeEnum; } \
    static Outer *FromHandle(ifaceX *wrapped) { return (Outer *)wrapped; }                \
  };

D3D12_UNWRAP_EXTENDED(ID3D12Fence, ID3D12Fence1);
D3D12_UNWRAP_EXTENDED(ID3D12PipelineLibrary, ID3D12PipelineLibrary1);
D3D12_UNWRAP_EXTENDED(ID3D12Heap, ID3D12Heap1);
D3D12_UNWRAP_EXTENDED(ID3D12Resource, ID3D12Resource1);
D3D12_UNWRAP_EXTENDED(ID3D12Resource, ID3D12Resource2);
D3D12_UNWRAP_EXTENDED(ID3D12ProtectedResourceSession, ID3D12ProtectedResourceSession1);

D3D12ResourceType IdentifyTypeByPtr(ID3D12Object *ptr);

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

class WrappedID3D12GraphicsCommandList;

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
D3D12ResourceRecord *GetRecord(ifaceptr obj)
{
  if(obj == NULL)
    return NULL;

  return GetWrapped(obj)->GetResourceRecord();
}

// specialisations that use the IsAlloc() function to identify the real type
template <>
ResourceId GetResID(ID3D12Object *ptr);
template <>
ID3D12Object *Unwrap(ID3D12Object *ptr);
template <>
D3D12ResourceRecord *GetRecord(ID3D12Object *ptr);

template <>
ResourceId GetResID(ID3D12DeviceChild *ptr);
template <>
ResourceId GetResID(ID3D12Pageable *ptr);
template <>
ResourceId GetResID(ID3D12CommandList *ptr);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList *ptr);
template <>
ResourceId GetResID(ID3D12CommandQueue *ptr);
template <>
ID3D12DeviceChild *Unwrap(ID3D12DeviceChild *ptr);

template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr);
