/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "api/replay/renderdoc_replay.h"
#include "common/wrapped_pool.h"
#include "core/core.h"
#include "core/resource_manager.h"
#include "driver/d3d12/d3d12_common.h"
#include "serialise/serialiser.h"

enum D3D12ResourceType
{
  Resource_Unknown = 0,
  Resource_Device,
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
  Resource_PipelineLibrary,
  Resource_ProtectedResourceSession,
};

DECLARE_REFLECTION_ENUM(D3D12ResourceType);

class WrappedID3D12DescriptorHeap;

// squeeze the descriptor a bit so that the D3D12Descriptor struct fits in 64 bytes
struct D3D12_UNORDERED_ACCESS_VIEW_DESC_SQUEEZED
{
  // pull up and compress down to 1 byte the enums/flags that don't have any larger values
  uint8_t Format;
  uint8_t ViewDimension;
  uint8_t BufferFlags;

  // 5 more bytes here - below union is 8-byte aligned

  union
  {
    struct D3D12_BUFFER_UAV_SQUEEZED
    {
      UINT64 FirstElement;
      UINT NumElements;
      UINT StructureByteStride;
      UINT64 CounterOffsetInBytes;
    } Buffer;
    D3D12_TEX1D_UAV Texture1D;
    D3D12_TEX1D_ARRAY_UAV Texture1DArray;
    D3D12_TEX2D_UAV Texture2D;
    D3D12_TEX2D_ARRAY_UAV Texture2DArray;
    D3D12_TEX3D_UAV Texture3D;
  };

  void Init(const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc)
  {
    Format = (uint8_t)desc.Format;
    ViewDimension = (uint8_t)desc.ViewDimension;

    // all but buffer elements should fit in 4 UINTs, so we can copy the Buffer (minus the flags we
    // moved) and still cover them.
    RDCCOMPILE_ASSERT(sizeof(Texture1D) <= 4 * sizeof(UINT), "Buffer isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture1DArray) <= 4 * sizeof(UINT),
                      "Buffer isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture2D) <= 4 * sizeof(UINT), "Buffer isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture2DArray) <= 4 * sizeof(UINT),
                      "Buffer isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture3D) <= 4 * sizeof(UINT), "Buffer isn't largest union member!");

    Buffer.FirstElement = desc.Buffer.FirstElement;
    Buffer.NumElements = desc.Buffer.NumElements;
    Buffer.StructureByteStride = desc.Buffer.StructureByteStride;
    Buffer.CounterOffsetInBytes = desc.Buffer.CounterOffsetInBytes;
    BufferFlags = (uint8_t)desc.Buffer.Flags;
  }

  D3D12_UNORDERED_ACCESS_VIEW_DESC AsDesc() const
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};

    desc.Format = (DXGI_FORMAT)Format;
    desc.ViewDimension = (D3D12_UAV_DIMENSION)ViewDimension;

    desc.Buffer.FirstElement = Buffer.FirstElement;
    desc.Buffer.NumElements = Buffer.NumElements;
    desc.Buffer.StructureByteStride = Buffer.StructureByteStride;
    desc.Buffer.CounterOffsetInBytes = Buffer.CounterOffsetInBytes;
    desc.Buffer.Flags = (D3D12_BUFFER_UAV_FLAGS)BufferFlags;

    return desc;
  }
};

struct D3D12_SHADER_RESOURCE_VIEW_DESC_SQUEEZED
{
  // pull up and compress down to 1 byte the enums that don't have any larger values.
  // note Shader4ComponentMapping only uses the lower 2 bytes - 3 bits per component = 12 bits.
  // Could even be bitpacked with ViewDimension if you wanted to get extreme.
  UINT Shader4ComponentMapping;
  uint8_t Format;
  uint8_t ViewDimension;

  // 2 more bytes here - below union is 8-byte aligned

  union
  {
    D3D12_BUFFER_SRV Buffer;
    D3D12_TEX1D_SRV Texture1D;
    D3D12_TEX1D_ARRAY_SRV Texture1DArray;
    D3D12_TEX2D_SRV Texture2D;
    D3D12_TEX2D_ARRAY_SRV Texture2DArray;
    D3D12_TEX2DMS_SRV Texture2DMS;
    D3D12_TEX2DMS_ARRAY_SRV Texture2DMSArray;
    D3D12_TEX3D_SRV Texture3D;
    D3D12_TEXCUBE_SRV TextureCube;
    D3D12_TEXCUBE_ARRAY_SRV TextureCubeArray;
  };

  void Init(const D3D12_SHADER_RESOURCE_VIEW_DESC &desc)
  {
    Format = (uint8_t)desc.Format;
    ViewDimension = (uint8_t)desc.ViewDimension;
    Shader4ComponentMapping = desc.Shader4ComponentMapping;

    // D3D12_TEX2D_ARRAY_SRV should be the largest component, so we can copy it and ensure we've
    // copied the rest.
    RDCCOMPILE_ASSERT(sizeof(Buffer) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture1D) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture1DArray) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture2D) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture2DMS) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture2DMSArray) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(Texture3D) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(TextureCube) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");
    RDCCOMPILE_ASSERT(sizeof(TextureCubeArray) <= sizeof(Texture2DArray),
                      "Texture2DArray isn't largest union member!");

    Texture2DArray = desc.Texture2DArray;
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC AsDesc() const
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

    desc.Format = (DXGI_FORMAT)Format;
    desc.ViewDimension = (D3D12_SRV_DIMENSION)ViewDimension;
    desc.Shader4ComponentMapping = Shader4ComponentMapping;

    desc.Texture2DArray = Texture2DArray;

    return desc;
  }
};

enum class D3D12DescriptorType
{
  // we start at 0x1000 since this element will alias with the filter
  // in the sampler, to save space
  Sampler,
  CBV = 0x1000,
  SRV,
  UAV,
  RTV,
  DSV,
  Undefined,
};

DECLARE_REFLECTION_ENUM(D3D12DescriptorType);

struct PortableHandle
{
  PortableHandle() : index(0) {}
  PortableHandle(ResourceId id, uint32_t i) : heap(id), index(i) {}
  PortableHandle(uint32_t i) : index(i) {}
  ResourceId heap;
  uint32_t index;
};

DECLARE_REFLECTION_STRUCT(PortableHandle);

// the heap pointer & index are inside the data structs, because in the sampler case we don't need
// to pad up for any alignment, and in the non-sampler case we declare the type before uint64/ptr
// aligned elements come, so we don't get any padding waste.
struct SamplerDescriptorData
{
  // same location in both structs
  WrappedID3D12DescriptorHeap *heap;
  uint32_t idx;

  D3D12_SAMPLER_DESC desc;
};

struct NonSamplerDescriptorData
{
  // same location in both structs
  WrappedID3D12DescriptorHeap *heap;
  uint32_t idx;

  // this element overlaps with the D3D12_FILTER in D3D12_SAMPLER_DESC,
  // with values that are invalid for filter
  D3D12DescriptorType type;

  // we store the ResourceId instead of a pointer here so we can check for invalidation,
  // in case the resource is freed and a different one is allocated in its place.
  // This can happen if e.g. a descriptor is initialised with ResourceId(1234), then the
  // resource is deleted and the descriptor is unused after that, but ResourceId(5678) is
  // allocated with the same ID3D12Resource*. We'd serialise the descriptor pointing to
  // ResourceId(5678) and it may well be completely invalid to create with the other parameters
  // we have stored.
  // We don't need anything but the ResourceId in high-traffic situations
  ResourceId resource;

  // this needs to be out here because we can't have the ResourceId with a constructor in the
  // anonymous union
  ResourceId counterResource;

  union
  {
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv;
    D3D12_SHADER_RESOURCE_VIEW_DESC_SQUEEZED srv;
    D3D12_UNORDERED_ACCESS_VIEW_DESC_SQUEEZED uav;
    D3D12_RENDER_TARGET_VIEW_DESC rtv;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv;
  };
};

union DescriptorData
{
  DescriptorData()
  {
    nonsamp.resource = ResourceId();
    nonsamp.counterResource = ResourceId();
  }
  SamplerDescriptorData samp;
  NonSamplerDescriptorData nonsamp;
};

struct D3D12Descriptor
{
public:
  D3D12Descriptor()
  {
    data.samp.heap = NULL;
    data.samp.idx = 0;
  }

  void Setup(WrappedID3D12DescriptorHeap *heap, UINT idx)
  {
    // only need to set this once, it's aliased between samp and nonsamp
    data.samp.heap = heap;
    data.samp.idx = idx;

    // initially descriptors are undefined. This way we just fill them with
    // some null SRV descriptor so it's safe to copy around etc but is no
    // less undefined for the application to use
    data.nonsamp.type = D3D12DescriptorType::Undefined;
  }

  D3D12DescriptorType GetType() const
  {
    RDCCOMPILE_ASSERT(sizeof(D3D12Descriptor) <= 64, "D3D12Descriptor has gotten larger");

    if(data.nonsamp.type < D3D12DescriptorType::CBV)
      return D3D12DescriptorType::Sampler;

    return data.nonsamp.type;
  }

  operator D3D12_CPU_DESCRIPTOR_HANDLE() const
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = (SIZE_T) this;
    return handle;
  }

  operator D3D12_GPU_DESCRIPTOR_HANDLE() const
  {
    D3D12_GPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = (SIZE_T) this;
    return handle;
  }

  void Init(const D3D12_SAMPLER_DESC *pDesc);
  void Init(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc);
  void Init(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc);
  void Init(ID3D12Resource *pResource, ID3D12Resource *pCounterResource,
            const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc);
  void Init(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc);
  void Init(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc);

  void Create(D3D12_DESCRIPTOR_HEAP_TYPE heapType, WrappedID3D12Device *dev,
              D3D12_CPU_DESCRIPTOR_HANDLE handle);
  void CopyFrom(const D3D12Descriptor &src);
  void GetRefIDs(ResourceId &id, ResourceId &id2, FrameRefType &ref);

  WrappedID3D12DescriptorHeap *GetHeap() const { return data.samp.heap; }
  uint32_t GetHeapIndex() const { return data.samp.idx; }
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPU() const;
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPU() const;
  PortableHandle GetPortableHandle() const;

  // these IDs are the live IDs during replay, not the original IDs. Treat them as if you called
  // GetResID(resource).
  //
  // descriptor heap itself
  ResourceId GetHeapResourceId() const;
  //
  // a resource - this covers RTV/DSV/SRV resource, UAV main resource (not counter - see below).
  // It does NOT cover the CBV's address - fetch that via GetCBV().BufferLocation
  ResourceId GetResResourceId() const;
  //
  // the counter resource for UAVs
  ResourceId GetCounterResourceId() const;

  // Accessors for descriptor structs. The squeezed structs return only by value, others have const
  // reference returns
  const D3D12_SAMPLER_DESC &GetSampler() const { return data.samp.desc; }
  const D3D12_RENDER_TARGET_VIEW_DESC &GetRTV() const { return data.nonsamp.rtv; }
  const D3D12_DEPTH_STENCIL_VIEW_DESC &GetDSV() const { return data.nonsamp.dsv; }
  const D3D12_CONSTANT_BUFFER_VIEW_DESC &GetCBV() const { return data.nonsamp.cbv; }
  // squeezed descriptors
  D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAV() const { return data.nonsamp.uav.AsDesc(); }
  D3D12_SHADER_RESOURCE_VIEW_DESC GetSRV() const { return data.nonsamp.srv.AsDesc(); }
private:
  DescriptorData data;

  // allow serialisation function access to the data
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, D3D12Descriptor &el);
};

DECLARE_REFLECTION_STRUCT(D3D12Descriptor);

inline D3D12Descriptor *GetWrapped(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  return (D3D12Descriptor *)handle.ptr;
}

inline D3D12Descriptor *GetWrapped(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
  return (D3D12Descriptor *)handle.ptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE Unwrap(D3D12_CPU_DESCRIPTOR_HANDLE handle);
D3D12_GPU_DESCRIPTOR_HANDLE Unwrap(D3D12_GPU_DESCRIPTOR_HANDLE handle);
D3D12_CPU_DESCRIPTOR_HANDLE UnwrapCPU(D3D12Descriptor *handle);
D3D12_GPU_DESCRIPTOR_HANDLE UnwrapGPU(D3D12Descriptor *handle);

class D3D12ResourceManager;

PortableHandle ToPortableHandle(D3D12Descriptor *handle);
PortableHandle ToPortableHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);
PortableHandle ToPortableHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle);
D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromPortableHandle(D3D12ResourceManager *manager,
                                                        PortableHandle handle);
D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromPortableHandle(D3D12ResourceManager *manager,
                                                        PortableHandle handle);
D3D12Descriptor *DescriptorFromPortableHandle(D3D12ResourceManager *manager, PortableHandle handle);

struct DynamicDescriptorWrite
{
  D3D12Descriptor desc;

  D3D12Descriptor *dest;
};

struct DynamicDescriptorCopy
{
  DynamicDescriptorCopy() : dst(NULL), src(NULL), type(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {}
  DynamicDescriptorCopy(D3D12Descriptor *d, D3D12Descriptor *s, D3D12_DESCRIPTOR_HEAP_TYPE t)
      : dst(d), src(s), type(t)
  {
  }

  D3D12Descriptor *dst;
  D3D12Descriptor *src;
  D3D12_DESCRIPTOR_HEAP_TYPE type;
};

DECLARE_REFLECTION_STRUCT(DynamicDescriptorCopy);

struct D3D12ResourceRecord;

struct CmdListRecordingInfo
{
  std::vector<D3D12_RESOURCE_BARRIER> barriers;

  // a list of all resources dirtied by this command list
  std::set<ResourceId> dirtied;

  // a list of descriptors that are bound at any point in this command list
  // used to look up all the frame refs per-descriptor and apply them on queue
  // submit with latest binding refs.
  // We allow duplicates in here since it's a better tradeoff to let the vector
  // expand a bit more to contain duplicates and then deal with it during frame
  // capture, than to constantly be deduplicating during record (e.g. with a
  // set or sorted vector).
  std::vector<D3D12Descriptor *> boundDescs;

  // bundles executed
  std::vector<D3D12ResourceRecord *> bundles;
};

class WrappedID3D12Resource1;

struct GPUAddressRange
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

struct GPUAddressRangeTracker
{
  GPUAddressRangeTracker() {}
  // no copying
  GPUAddressRangeTracker(const GPUAddressRangeTracker &);
  GPUAddressRangeTracker &operator=(const GPUAddressRangeTracker &);

  std::vector<GPUAddressRange> addresses;
  Threading::RWLock addressLock;

  void AddTo(const GPUAddressRange &range)
  {
    SCOPED_WRITELOCK(addressLock);
    auto it = std::lower_bound(addresses.begin(), addresses.end(), range.start);

    addresses.insert(it, range);
  }

  void RemoveFrom(const GPUAddressRange &range)
  {
    {
      SCOPED_WRITELOCK(addressLock);
      auto it = std::lower_bound(addresses.begin(), addresses.end(), range.start);

      // there might be multiple buffers with the same range start, find the exact range for this
      // buffer
      while(it != addresses.end() && it->start == range.start)
      {
        if(it->id == range.id)
        {
          addresses.erase(it);
          return;
        }

        ++it;
      }
    }

    RDCERR("Couldn't find matching range to remove for %s", ToStr(range.id).c_str());
  }

  void GetResIDFromAddr(D3D12_GPU_VIRTUAL_ADDRESS addr, ResourceId &id, UINT64 &offs)
  {
    id = ResourceId();
    offs = 0;

    if(addr == 0)
      return;

    GPUAddressRange range;

    // this should really be a read-write lock
    {
      SCOPED_READLOCK(addressLock);

      auto it = std::lower_bound(addresses.begin(), addresses.end(), addr);
      if(it == addresses.end())
        return;

      range = *it;
    }

    if(addr < range.start || addr >= range.end)
      return;

    id = range.id;
    offs = addr - range.start;
  }
};

struct MapState
{
  ID3D12Resource *res;
  UINT subres;
  UINT64 totalSize;
};

struct D3D12ResourceRecord : public ResourceRecord
{
  enum
  {
    NullResource = NULL
  };

  D3D12ResourceRecord(ResourceId id)
      : ResourceRecord(id, true),
        type(Resource_Unknown),
        ContainsExecuteIndirect(false),
        cmdInfo(NULL),
        m_Maps(NULL),
        m_MapsCount(0),
        bakedCommands(NULL)
  {
  }
  ~D3D12ResourceRecord()
  {
    SAFE_DELETE(cmdInfo);
    SAFE_DELETE_ARRAY(m_Maps);
  }
  void Bake()
  {
    RDCASSERT(cmdInfo);
    SwapChunks(bakedCommands);
    cmdInfo->barriers.swap(bakedCommands->cmdInfo->barriers);
    cmdInfo->dirtied.swap(bakedCommands->cmdInfo->dirtied);
    cmdInfo->boundDescs.swap(bakedCommands->cmdInfo->boundDescs);
    cmdInfo->bundles.swap(bakedCommands->cmdInfo->bundles);
  }

  D3D12ResourceType type;
  bool ContainsExecuteIndirect;
  D3D12ResourceRecord *bakedCommands;
  CmdListRecordingInfo *cmdInfo;

  struct MapData
  {
    MapData() : refcount(0), realPtr(NULL), shadowPtr(NULL) {}
    int32_t refcount;
    byte *realPtr;
    byte *shadowPtr;
  };

  MapData *m_Maps;
  size_t m_MapsCount;
  Threading::CriticalSection m_MapLock;
};

typedef std::vector<D3D12_RESOURCE_STATES> SubresourceStateVector;

struct D3D12InitialContents
{
  enum Tag
  {
    Copy,
    // this is only valid during capture - it indicates we didn't create a staging texture, we're
    // going to read directly from the resource (only valid for resources that are already READBACK)
    MapDirect,
  };
  D3D12InitialContents(D3D12Descriptor *d, uint32_t n)
      : tag(Copy),
        resourceType(Resource_DescriptorHeap),
        descriptors(d),
        numDescriptors(n),
        resource(NULL),
        srcData(NULL),
        dataSize(0)
  {
  }
  D3D12InitialContents(ID3D12DescriptorHeap *r)
      : tag(Copy),
        resourceType(Resource_DescriptorHeap),
        descriptors(NULL),
        numDescriptors(0),
        resource(r),
        srcData(NULL),
        dataSize(0)
  {
  }
  D3D12InitialContents(ID3D12Resource *r)
      : tag(Copy),
        resourceType(Resource_Resource),
        descriptors(NULL),
        numDescriptors(0),
        resource(r),
        srcData(NULL),
        dataSize(0)
  {
  }
  D3D12InitialContents(byte *data, size_t size)
      : tag(MapDirect),
        resourceType(Resource_Resource),
        descriptors(NULL),
        numDescriptors(0),
        resource(NULL),
        srcData(data),
        dataSize(size)
  {
  }
  D3D12InitialContents(Tag tg, D3D12ResourceType type)
      : tag(tg),
        resourceType(type),
        descriptors(NULL),
        numDescriptors(0),
        resource(NULL),
        srcData(NULL),
        dataSize(0)
  {
  }
  D3D12InitialContents()
      : tag(Copy),
        resourceType(Resource_Unknown),
        descriptors(NULL),
        numDescriptors(0),
        resource(NULL),
        srcData(NULL),
        dataSize(0)
  {
  }
  template <typename Configuration>
  void Free(ResourceManager<Configuration> *rm)
  {
    SAFE_RELEASE(resource);
    FreeAlignedBuffer(srcData);
  }

  Tag tag;
  D3D12ResourceType resourceType;
  D3D12Descriptor *descriptors;
  uint32_t numDescriptors;
  ID3D12DeviceChild *resource;
  byte *srcData;
  size_t dataSize;
};

struct D3D12ResourceManagerConfiguration
{
  typedef ID3D12DeviceChild *WrappedResourceType;
  typedef ID3D12DeviceChild *RealResourceType;
  typedef D3D12ResourceRecord RecordType;
  typedef D3D12InitialContents InitialContentData;
};

class D3D12ResourceManager : public ResourceManager<D3D12ResourceManagerConfiguration>
{
public:
  D3D12ResourceManager(CaptureState state, WrappedID3D12Device *dev)
      : ResourceManager(), m_State(state), m_Device(dev)
  {
  }
  template <class T>
  T *GetLiveAs(ResourceId id)
  {
    return (T *)GetLiveResource(id);
  }

  template <class T>
  T *GetCurrentAs(ResourceId id)
  {
    return (T *)GetCurrentResource(id);
  }

  void ApplyBarriers(std::vector<D3D12_RESOURCE_BARRIER> &barriers,
                     std::map<ResourceId, SubresourceStateVector> &states);

  template <typename SerialiserType>
  void SerialiseResourceStates(SerialiserType &ser, std::vector<D3D12_RESOURCE_BARRIER> &barriers,
                               std::map<ResourceId, SubresourceStateVector> &states);

  template <typename SerialiserType>
  bool Serialise_InitialState(SerialiserType &ser, ResourceId id, D3D12ResourceRecord *record,
                              const D3D12InitialContents *initial);

  void SetInternalResource(ID3D12DeviceChild *res);

private:
  ResourceId GetID(ID3D12DeviceChild *res);

  bool ResourceTypeRelease(ID3D12DeviceChild *res);

  bool Prepare_InitialState(ID3D12DeviceChild *res);
  uint64_t GetSize_InitialState(ResourceId id, const D3D12InitialContents &data);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, D3D12ResourceRecord *record,
                              const D3D12InitialContents *initial)
  {
    return Serialise_InitialState<WriteSerialiser>(ser, id, record, initial);
  }
  void Create_InitialState(ResourceId id, ID3D12DeviceChild *live, bool hasData);
  void Apply_InitialState(ID3D12DeviceChild *live, const D3D12InitialContents &data);

  CaptureState m_State;
  WrappedID3D12Device *m_Device;
};
