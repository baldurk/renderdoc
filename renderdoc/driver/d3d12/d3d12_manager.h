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
};

class WrappedID3D12DescriptorHeap;

// squeeze the descriptor a bit so that the below struct fits in 64 bytes
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

struct D3D12Descriptor
{
  enum DescriptorType
  {
    // we start at 0x1000 since this element will alias with the filter
    // in the sampler, to save space
    TypeSampler,
    TypeCBV = 0x1000,
    TypeSRV,
    TypeUAV,
    TypeRTV,
    TypeDSV,
    TypeUndefined,
  };

  DescriptorType GetType() const
  {
    RDCCOMPILE_ASSERT(sizeof(D3D12Descriptor) <= 64, "D3D12Descriptor has gotten larger");

    if(nonsamp.type < TypeCBV)
      return TypeSampler;

    return nonsamp.type;
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

  union
  {
    // keep the sampler outside as it's the largest descriptor
    struct
    {
      // same location in both structs
      WrappedID3D12DescriptorHeap *heap;
      uint32_t idx;

      D3D12_SAMPLER_DESC desc;
    } samp;

    struct
    {
      // same location in both structs
      WrappedID3D12DescriptorHeap *heap;
      uint32_t idx;

      // this element overlaps with the D3D12_FILTER in D3D12_SAMPLER_DESC,
      // with values that are invalid for filter
      DescriptorType type;

      ID3D12Resource *resource;

      union
      {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv;
        struct
        {
          ID3D12Resource *counterResource;
          D3D12_UNORDERED_ACCESS_VIEW_DESC_SQUEEZED desc;
        } uav;
        D3D12_RENDER_TARGET_VIEW_DESC rtv;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv;
      };
    } nonsamp;
  };
};

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

struct PortableHandle
{
  PortableHandle() : index(0) {}
  PortableHandle(ResourceId id, uint32_t i) : heap(id), index(i) {}
  PortableHandle(uint32_t i) : index(i) {}
  ResourceId heap;
  uint32_t index;
};

class D3D12ResourceManager;

PortableHandle ToPortableHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);
PortableHandle ToPortableHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle);
D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromPortableHandle(D3D12ResourceManager *manager,
                                                        PortableHandle handle);
D3D12_GPU_DESCRIPTOR_HANDLE GPUHandleFromPortableHandle(D3D12ResourceManager *manager,
                                                        PortableHandle handle);

struct D3D12ResourceRecord;

struct CmdListRecordingInfo
{
  vector<D3D12_RESOURCE_BARRIER> barriers;

  // a list of all resources dirtied by this command list
  set<ResourceId> dirtied;

  // a list of descriptors that are bound at any point in this command list
  // used to look up all the frame refs per-descriptor and apply them on queue
  // submit with latest binding refs.
  set<D3D12Descriptor *> boundDescs;

  // bundles executed
  vector<D3D12ResourceRecord *> bundles;
};

struct D3D12ResourceRecord : public ResourceRecord
{
  enum
  {
    NullResource = NULL
  };

  D3D12ResourceRecord(ResourceId id)
      : ResourceRecord(id, true), type(Resource_Unknown), cmdInfo(NULL), bakedCommands(NULL)
  {
  }
  ~D3D12ResourceRecord() {}
  void Bake()
  {
    RDCASSERT(cmdInfo);
    SwapChunks(bakedCommands);
    cmdInfo->barriers.swap(bakedCommands->cmdInfo->barriers);
    cmdInfo->dirtied.swap(bakedCommands->cmdInfo->dirtied);
    cmdInfo->boundDescs.swap(bakedCommands->cmdInfo->boundDescs);
    cmdInfo->bundles.swap(bakedCommands->cmdInfo->bundles);
  }

  void Insert(map<int32_t, Chunk *> &recordlist)
  {
    bool dataWritten = DataWritten;

    DataWritten = true;

    for(auto it = Parents.begin(); it != Parents.end(); ++it)
    {
      if(!(*it)->DataWritten)
      {
        (*it)->Insert(recordlist);
      }
    }

    if(!dataWritten)
      recordlist.insert(m_Chunks.begin(), m_Chunks.end());
  }

  D3D12ResourceType type;
  D3D12ResourceRecord *bakedCommands;
  CmdListRecordingInfo *cmdInfo;
};

typedef vector<D3D12_RESOURCE_STATES> SubresourceStateVector;

class D3D12ResourceManager
    : public ResourceManager<ID3D12DeviceChild *, ID3D12DeviceChild *, D3D12ResourceRecord>
{
public:
  D3D12ResourceManager(LogState state, Serialiser *ser, WrappedID3D12Device *dev)
      : ResourceManager(state, ser), m_Device(dev)
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

  void ApplyBarriers(vector<D3D12_RESOURCE_BARRIER> &barriers,
                     map<ResourceId, SubresourceStateVector> &states);

  void SerialiseResourceStates(vector<D3D12_RESOURCE_BARRIER> &barriers,
                               map<ResourceId, SubresourceStateVector> &states);

  bool Serialise_InitialState(ResourceId resid, ID3D12DeviceChild *res);

private:
  bool SerialisableResource(ResourceId id, D3D12ResourceRecord *record);
  ResourceId GetID(ID3D12DeviceChild *res);

  bool ResourceTypeRelease(ID3D12DeviceChild *res);

  bool Force_InitialState(ID3D12DeviceChild *res, bool prepare);
  bool Need_InitialStateChunk(ID3D12DeviceChild *res);
  bool Prepare_InitialState(ID3D12DeviceChild *res);
  void Create_InitialState(ResourceId id, ID3D12DeviceChild *live, bool hasData);
  void Apply_InitialState(ID3D12DeviceChild *live, InitialContentData data);

  WrappedID3D12Device *m_Device;
};
