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

#include <algorithm>
#include "driver/shaders/dxbc/dxbc_inspect.h"
#include "d3d12_device.h"
#include "d3d12_manager.h"

class TrackedResource
{
public:
  TrackedResource()
  {
    m_ID = ResourceIDGen::GetNewUniqueID();
    m_pRecord = NULL;
  }
  ResourceId GetResourceID() { return m_ID; }
  D3D12ResourceRecord *GetResourceRecord() { return m_pRecord; }
  void SetResourceRecord(D3D12ResourceRecord *record) { m_pRecord = record; }
private:
  TrackedResource(const TrackedResource &);
  TrackedResource &operator=(const TrackedResource &);

  ResourceId m_ID;
  D3D12ResourceRecord *m_pRecord;
};

extern const GUID RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue;

template <typename NestedType, typename NestedType1 = NestedType, typename NestedType2 = NestedType1>
class WrappedDeviceChild12 : public RefCounter12<NestedType>, public NestedType2, public TrackedResource
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
        RDCERR("Error adding wrapper for type %s", ToStr::Get(__uuidof(NestedType)).c_str());
    }

    m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);
  }

  virtual void Shutdown()
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
    if(guid == RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue)
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
                              const D3D12_DESCRIPTOR_HEAP_DESC &desc);
  virtual ~WrappedID3D12DescriptorHeap();

  const D3D12Descriptor *GetDescriptors() { return descriptors; }
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

  D3D12_GRAPHICS_PIPELINE_STATE_DESC *graphics;
  D3D12_COMPUTE_PIPELINE_STATE_DESC *compute;

  bool IsGraphics() { return graphics != NULL; }
  bool IsCompute() { return compute != NULL; }
  struct DXBCKey
  {
    DXBCKey(const D3D12_SHADER_BYTECODE &byteCode)
    {
      byteLen = (uint32_t)byteCode.BytecodeLength;
      DXBC::DXBCFile::GetHash(hash, byteCode.pShaderBytecode, byteCode.BytecodeLength);
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
    static const int AllocPoolCount = 16384;
    static const int AllocMaxByteSize = 8 * 1024 * 1024;
    ALLOCATE_WITH_WRAPPED_POOL(ShaderEntry, AllocPoolCount, AllocMaxByteSize);

    ShaderEntry(const D3D12_SHADER_BYTECODE &byteCode, WrappedID3D12Device *device)
        : WrappedDeviceChild12(NULL, device), m_Key(byteCode)
    {
      const byte *code = (const byte *)byteCode.pShaderBytecode;
      m_Bytecode.assign(code, code + byteCode.BytecodeLength);
      m_DebugInfoSearchPaths = NULL;
      m_DXBCFile = NULL;

      device->GetResourceManager()->AddLiveResource(GetResourceID(), this);

      m_Built = false;
    }

    virtual ~ShaderEntry()
    {
      m_Bytecode.clear();
      SAFE_DELETE(m_DXBCFile);
      Shutdown();
    }

    DXBCKey GetKey() { return m_Key; }
    void SetDebugInfoPath(vector<std::string> *searchPaths, const std::string &path)
    {
      m_DebugInfoSearchPaths = searchPaths;
      m_DebugInfoPath = path;
    }

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
        MakeShaderReflection(m_DXBCFile, &m_Details, &m_Mapping);
      m_Built = true;
      return m_Details;
    }
    const ShaderBindpointMapping &GetMapping()
    {
      if(!m_Built && GetDXBC() != NULL)
        MakeShaderReflection(m_DXBCFile, &m_Details, &m_Mapping);
      m_Built = true;
      return m_Mapping;
    }

  private:
    ShaderEntry(const ShaderEntry &e);
    void TryReplaceOriginalByteCode();
    ShaderEntry &operator=(const ShaderEntry &e);

    DXBCKey m_Key;

    std::string m_DebugInfoPath;
    vector<std::string> *m_DebugInfoSearchPaths;

    vector<byte> m_Bytecode;

    bool m_Built;
    DXBC::DXBCFile *m_DXBCFile;
    ShaderReflection m_Details;
    ShaderBindpointMapping m_Mapping;
  };

  enum
  {
    TypeEnum = Resource_PipelineState,
  };

  static ShaderEntry *AddShader(const D3D12_SHADER_BYTECODE &byteCode, WrappedID3D12Device *device)
  {
    DXBCKey key(byteCode);
    ShaderEntry *shader = m_Shaders[key];

    if(shader == NULL)
      shader = m_Shaders[key] = new ShaderEntry(byteCode, device);
    else
      shader->AddRef();

    return shader;
  }

  static void ReleaseShader(ShaderEntry *shader)
  {
    if(shader == NULL)
      return;

    DXBCKey key = shader->GetKey();

    if(shader->Release() == 0)
      m_Shaders.erase(key);
  }

  WrappedID3D12PipelineState(ID3D12PipelineState *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
    graphics = NULL;
    compute = NULL;
  }
  virtual ~WrappedID3D12PipelineState()
  {
    Shutdown();

    if(graphics)
    {
      ShaderEntry *vs = (ShaderEntry *)graphics->VS.pShaderBytecode;
      ShaderEntry *hs = (ShaderEntry *)graphics->HS.pShaderBytecode;
      ShaderEntry *ds = (ShaderEntry *)graphics->DS.pShaderBytecode;
      ShaderEntry *gs = (ShaderEntry *)graphics->GS.pShaderBytecode;
      ShaderEntry *ps = (ShaderEntry *)graphics->PS.pShaderBytecode;

      ReleaseShader(vs);
      ReleaseShader(hs);
      ReleaseShader(ds);
      ReleaseShader(gs);
      ReleaseShader(ps);

      SAFE_DELETE(graphics);
    }

    if(compute)
    {
      ShaderEntry *cs = (ShaderEntry *)compute->CS.pShaderBytecode;

      ReleaseShader(cs);

      SAFE_DELETE(compute);
    }
  }

  //////////////////////////////
  // implement ID3D12PipelineState

  virtual HRESULT STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **ppBlob)
  {
    return m_pReal->GetCachedBlob(ppBlob);
  }

private:
  static map<DXBCKey, ShaderEntry *> m_Shaders;
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
  struct AddressRange
  {
    D3D12_GPU_VIRTUAL_ADDRESS start, end;
    ResourceId id;

    bool operator<(const D3D12_GPU_VIRTUAL_ADDRESS &o) const
    {
      if(o < start)
        return true;

      return false;
    }
  };

  static std::vector<AddressRange> m_Addresses;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12Resource);

  static std::map<ResourceId, WrappedID3D12Resource *> m_List;

  static void GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr, ResourceId &id, UINT64 &offs)
  {
    id = ResourceId();
    offs = 0;

    if(m_Addresses.empty())
      return;

    auto it = std::lower_bound(m_Addresses.begin(), m_Addresses.end(), addr);
    if(it == m_Addresses.end())
      return;

    if(addr < it->start || addr >= it->end)
      return;

    id = it->id;
    offs = addr - it->start;
  }

  // overload to just return the id in case the offset isn't needed
  static ResourceId GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr)
  {
    ResourceId id;
    UINT64 offs;

    GetResIDFromAddr(addr, id, offs);

    return id;
  }

  enum
  {
    TypeEnum = Resource_Resource,
  };

  WrappedID3D12Resource(ID3D12Resource *real, WrappedID3D12Device *device)
      : WrappedDeviceChild12(real, device)
  {
    m_List[GetResourceID()] = this;

    // assuming only valid for buffers
    if(m_pReal->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      D3D12_GPU_VIRTUAL_ADDRESS addr = m_pReal->GetGPUVirtualAddress();

      auto it = std::lower_bound(m_Addresses.begin(), m_Addresses.end(), addr);
      RDCASSERT(it == m_Addresses.begin() || it == m_Addresses.end() || addr < it->start ||
                addr >= it->end);

      AddressRange range;
      range.start = addr;
      range.end = addr + m_pReal->GetDesc().Width;
      range.id = GetResourceID();

      m_Addresses.insert(it, range);
    }
  }
  virtual ~WrappedID3D12Resource()
  {
    m_List.erase(GetResourceID());

    // assuming only valid for buffers
    if(m_pReal->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      D3D12_GPU_VIRTUAL_ADDRESS addr = m_pReal->GetGPUVirtualAddress();

      auto it = std::lower_bound(m_Addresses.begin(), m_Addresses.end(), addr);
      RDCASSERT(it != m_Addresses.end() && addr >= it->start && addr < it->end);

      m_Addresses.erase(it);
    }

    Shutdown();
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

  virtual HRESULT STDMETHODCALLTYPE Map(UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData)
  {
    D3D12NOTIMP("Resource mapping");
    return m_pReal->Map(Subresource, pReadRange, ppData);
  }

  virtual void STDMETHODCALLTYPE Unmap(UINT Subresource, const D3D12_RANGE *pWrittenRange)
  {
    D3D12NOTIMP("Resource mapping");
    return m_pReal->Unmap(Subresource, pWrittenRange);
  }

  virtual HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource, const D3D12_BOX *pDstBox,
                                                       const void *pSrcData, UINT SrcRowPitch,
                                                       UINT SrcDepthPitch)
  {
    D3D12NOTIMP("Resource mapping");
    return m_pReal->WriteToSubresource(DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
  }

  virtual HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch,
                                                        UINT DstDepthPitch, UINT SrcSubresource,
                                                        const D3D12_BOX *pSrcBox)
  {
    D3D12NOTIMP("Resource mapping");
    return m_pReal->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource,
                                        pSrcBox);
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

D3D12ResourceType IdentifyTypeByPtr(ID3D12DeviceChild *ptr);

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
D3D12ResourceRecord *GetRecord(ifaceptr obj)
{
  if(obj == NULL)
    return NULL;

  return GetWrapped(obj)->GetResourceRecord();
}

// specialisations that use the IsAlloc() function to identify the real type
template <>
ResourceId GetResID(ID3D12DeviceChild *ptr);
template <>
ID3D12DeviceChild *Unwrap(ID3D12DeviceChild *ptr);
template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr);
