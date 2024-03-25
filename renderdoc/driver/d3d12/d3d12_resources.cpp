/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "d3d12_resources.h"
#include "driver/shaders/dxbc/dxbc_reflect.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"

GPUAddressRangeTracker WrappedID3D12Resource::m_Addresses;
rdcarray<ResourceId> WrappedID3D12Resource::m_bufferResources;
std::map<WrappedID3D12PipelineState::DXBCKey, WrappedID3D12Shader *> WrappedID3D12Shader::m_Shaders;
bool WrappedID3D12Shader::m_InternalResources = false;
int32_t WrappedID3D12CommandAllocator::m_ResetEnabled = 1;

const GUID RENDERDOC_ID3D12ShaderGUID_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface) WRAPPED_POOL_INST(CONCAT(Wrapped, iface));

ALL_D3D12_TYPES;

D3D12ResourceType IdentifyTypeByPtr(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return Resource_Unknown;

#undef D3D12_TYPE_MACRO
#define D3D12_TYPE_MACRO(iface)         \
  if(UnwrapHelper<iface>::IsAlloc(ptr)) \
    return UnwrapHelper<iface>::GetTypeEnum();

  ALL_D3D12_TYPES;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return Resource_GraphicsCommandList;
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return Resource_CommandQueue;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return Resource_Unknown;
}

TrackedResource12 *GetTracked(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return NULL;

  return (TrackedResource12 *)(WrappedDeviceChild12<ID3D12DeviceChild> *)ptr;
}

template <>
ID3D12Object *Unwrap(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return NULL;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return (ID3D12Object *)(((WrappedID3D12GraphicsCommandList *)ptr)->GetReal());
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return (ID3D12Object *)(((WrappedID3D12CommandQueue *)ptr)->GetReal());

  return ((WrappedDeviceChild12<ID3D12DeviceChild> *)ptr)->GetReal();
}

template <>
ResourceId GetResID(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return ResourceId();

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return ((WrappedID3D12GraphicsCommandList *)ptr)->GetResourceID();
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return ((WrappedID3D12CommandQueue *)ptr)->GetResourceID();

  return GetTracked(ptr)->GetResourceID();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12Object *ptr)
{
  if(ptr == NULL)
    return NULL;

  if(WrappedID3D12GraphicsCommandList::IsAlloc(ptr))
    return ((WrappedID3D12GraphicsCommandList *)ptr)->GetResourceRecord();
  if(WrappedID3D12CommandQueue::IsAlloc(ptr))
    return ((WrappedID3D12CommandQueue *)ptr)->GetResourceRecord();

  return GetTracked(ptr)->GetResourceRecord();
}

template <>
ResourceId GetResID(ID3D12DeviceChild *ptr)
{
  return GetResID((ID3D12Object *)ptr);
}

template <>
ResourceId GetResID(ID3D12Pageable *ptr)
{
  return GetResID((ID3D12Object *)ptr);
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12DeviceChild *ptr)
{
  return GetRecord((ID3D12Object *)ptr);
}
template <>
ID3D12DeviceChild *Unwrap(ID3D12DeviceChild *ptr)
{
  return (ID3D12DeviceChild *)Unwrap((ID3D12Object *)ptr);
}

WrappedID3D12Resource::~WrappedID3D12Resource()
{
  SAFE_RELEASE(m_Heap);

  // perform an implicit unmap on release
  if(GetResourceRecord())
  {
    D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;
    size_t mapcount = GetResourceRecord()->m_MapsCount;

    // may not have a map if e.g. no pointer was requested
    for(size_t i = 0; i < mapcount; i++)
    {
      if(map[i].refcount > 0)
      {
        m_pDevice->Unmap(this, (UINT)i, map[i].realPtr, NULL);

        FreeAlignedBuffer(map[i].shadowPtr);
        map[i].realPtr = NULL;
        map[i].shadowPtr = NULL;
      }
    }
  }

  if(IsReplayMode(m_pDevice->GetState()))
    m_pDevice->RemoveReplayResource(GetResourceID());

  // assuming only valid for buffers
  if(m_pReal->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    GPUAddressRange range;
    range.start = m_pReal->GetGPUVirtualAddress();
    // realEnd and oobEnd are not used for removing, just start + id
    range.id = GetResourceID();

    m_Addresses.RemoveFrom(range);

    m_bufferResources.removeOne(GetResourceID());
  }

  Shutdown();

  m_ID = ResourceId();
}

byte *WrappedID3D12Resource::GetMap(UINT Subresource)
{
  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;
  size_t mapcount = GetResourceRecord()->m_MapsCount;

  if(Subresource < mapcount)
    return map[Subresource].realPtr;

  return NULL;
}

byte *WrappedID3D12Resource::GetShadow(UINT Subresource)
{
  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

  return map[Subresource].shadowPtr;
}

void WrappedID3D12Resource::AllocShadow(UINT Subresource, size_t size)
{
  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

  if(map[Subresource].shadowPtr == NULL)
    map[Subresource].shadowPtr = AllocAlignedBuffer(size);
}

void WrappedID3D12Resource::FreeShadow()
{
  SCOPED_LOCK(GetResourceRecord()->m_MapLock);

  D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;
  size_t mapcount = GetResourceRecord()->m_MapsCount;

  for(size_t i = 0; i < mapcount; i++)
  {
    FreeAlignedBuffer(map[i].shadowPtr);
    map[i].shadowPtr = NULL;
  }
}

void WrappedID3D12Resource::LockMaps()
{
  GetResourceRecord()->m_MapLock.Lock();
}

void WrappedID3D12Resource::UnlockMaps()
{
  GetResourceRecord()->m_MapLock.Unlock();
}

WriteSerialiser &WrappedID3D12Resource::GetThreadSerialiser()
{
  return m_pDevice->GetThreadSerialiser();
}

size_t WrappedID3D12Resource::DeleteOverlappingAccStructsInRangeAtOffset(D3D12BufferOffset bufferOffset)
{
  SCOPED_LOCK(m_accStructResourcesCS);

  if(!m_accelerationStructMap.empty())
  {
    rdcarray<D3D12BufferOffset> toBeDeleted;
    uint64_t accStructAtOffsetSize = m_accelerationStructMap[bufferOffset]->Size();

    for(const rdcpair<D3D12BufferOffset, D3D12AccelerationStructure *> &accStructAtOffset :
        m_accelerationStructMap)
    {
      if(accStructAtOffset.first == bufferOffset)
      {
        continue;
      }

      if(accStructAtOffset.first < bufferOffset &&
         (accStructAtOffset.first + accStructAtOffset.second->Size()) > bufferOffset)
      {
        toBeDeleted.push_back(accStructAtOffset.first);
      }

      if(accStructAtOffset.first > bufferOffset &&
         (bufferOffset + accStructAtOffsetSize) > accStructAtOffset.first)
      {
        toBeDeleted.push_back(accStructAtOffset.first);
      }
    }

    for(D3D12BufferOffset deleting : toBeDeleted)
    {
      DeleteAccStructAtOffset(deleting);
    }

    return toBeDeleted.size();
  }

  return 0;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Resource::Map(UINT Subresource,
                                                     const D3D12_RANGE *pReadRange, void **ppData)
{
  // don't care about maps without returned pointers - we'll just intercept the WriteToSubresource
  // calls
  if(ppData == NULL)
    return m_pReal->Map(Subresource, pReadRange, ppData);

  void *mapPtr = NULL;

  // pass a NULL range as we might want to read from the whole range
  HRESULT hr = m_pReal->Map(Subresource, NULL, &mapPtr);

  *ppData = mapPtr;

  if(SUCCEEDED(hr) && GetResourceRecord())
  {
    SCOPED_LOCK(GetResourceRecord()->m_MapLock);

    D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

    map[Subresource].realPtr = (byte *)mapPtr;
    map[Subresource].refcount++;

    // on the first map, register this so we can flush any updates in case it's left persistant
    if(map[Subresource].refcount == 1)
      m_pDevice->Map(this, Subresource);
  }

  return hr;
}

void STDMETHODCALLTYPE WrappedID3D12Resource::Unmap(UINT Subresource, const D3D12_RANGE *pWrittenRange)
{
  if(GetResourceRecord())
  {
    D3D12ResourceRecord::MapData *map = GetResourceRecord()->m_Maps;

    {
      SCOPED_LOCK(GetResourceRecord()->m_MapLock);

      // may not have a ref at all if e.g. no pointer was requested
      if(map[Subresource].refcount >= 1)
      {
        map[Subresource].refcount--;

        if(map[Subresource].refcount == 0)
        {
          m_pDevice->Unmap(this, Subresource, map[Subresource].realPtr, pWrittenRange);

          FreeAlignedBuffer(map[Subresource].shadowPtr);
          map[Subresource].realPtr = NULL;
          map[Subresource].shadowPtr = NULL;
        }
      }
    }
  }

  return m_pReal->Unmap(Subresource, pWrittenRange);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12Resource::WriteToSubresource(UINT DstSubresource,
                                                                    const D3D12_BOX *pDstBox,
                                                                    const void *pSrcData,
                                                                    UINT SrcRowPitch,
                                                                    UINT SrcDepthPitch)
{
  HRESULT ret;

  SERIALISE_TIME_CALL(ret = m_pReal->WriteToSubresource(DstSubresource, pDstBox, pSrcData,
                                                        SrcRowPitch, SrcDepthPitch));

  if(GetResourceRecord())
  {
    m_pDevice->WriteToSubresource(this, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
                                  SrcDepthPitch);
  }

  return ret;
}

WRAPPED_POOL_INST(D3D12AccelerationStructure);

D3D12AccelerationStructure::D3D12AccelerationStructure(
    WrappedID3D12Device *wrappedDevice, WrappedID3D12Resource *bufferRes,
    D3D12BufferOffset bufferOffset,
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO &preBldInfo)
    : WrappedDeviceChild12(NULL, wrappedDevice),
      m_asbWrappedResource(bufferRes),
      m_asbWrappedResourceBufferOffset(bufferOffset),
      m_preBldInfo(preBldInfo)
{
}

D3D12AccelerationStructure::~D3D12AccelerationStructure()
{
  Shutdown();
}

bool WrappedID3D12Resource::CreateAccStruct(
    D3D12BufferOffset bufferOffset,
    const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO &preBldInfo,
    D3D12AccelerationStructure **accStruct)
{
  SCOPED_LOCK(m_accStructResourcesCS);
  if(m_accelerationStructMap.find(bufferOffset) == m_accelerationStructMap.end())
  {
    m_accelerationStructMap[bufferOffset] =
        new D3D12AccelerationStructure(m_pDevice, this, bufferOffset, preBldInfo);

    if(accStruct)
    {
      *accStruct = m_accelerationStructMap[bufferOffset];

      if(IsCaptureMode(m_pDevice->GetState()))
      {
        size_t deletedAccStructCount = DeleteOverlappingAccStructsInRangeAtOffset(bufferOffset);
        RDCDEBUG("Acc structure created after deleting %u overlapping acc structure(s)",
                 deletedAccStructCount);
        deletedAccStructCount;
      }
    }

    return true;
  }

  return false;
}

bool WrappedID3D12Resource::GetAccStructIfExist(D3D12BufferOffset bufferOffset,
                                                D3D12AccelerationStructure **accStruct)
{
  SCOPED_LOCK(m_accStructResourcesCS);
  bool found = false;

  if(m_accelerationStructMap.find(bufferOffset) != m_accelerationStructMap.end())
  {
    found = true;
    if(accStruct)
    {
      *accStruct = m_accelerationStructMap[bufferOffset];
    }
  }

  return found;
}

bool WrappedID3D12Resource::DeleteAccStructAtOffset(D3D12BufferOffset bufferOffset)
{
  SCOPED_LOCK(m_accStructResourcesCS);
  D3D12AccelerationStructure *accStruct = NULL;
  if(GetAccStructIfExist(bufferOffset, &accStruct))
  {
    if(m_accelerationStructMap[bufferOffset]->Release() == 0)
    {
      m_accelerationStructMap.erase(bufferOffset);
    }

    return true;
  }

  return false;
}

void WrappedID3D12Resource::RefBuffers(D3D12ResourceManager *rm)
{
  // only buffers go into m_Addresses
  SCOPED_READLOCK(m_Addresses.addressLock);
  for(size_t i = 0; i < m_Addresses.addresses.size(); i++)
    rm->MarkResourceFrameReferenced(m_Addresses.addresses[i].id, eFrameRef_Read);
}

void WrappedID3D12Resource::GetMappableIDs(D3D12ResourceManager *rm,
                                           const std::unordered_set<ResourceId> &refdIDs,
                                           std::unordered_set<ResourceId> &mappableIDs)
{
  SCOPED_READLOCK(m_Addresses.addressLock);
  for(size_t i = 0; i < m_Addresses.addresses.size(); i++)
  {
    if(refdIDs.find(m_Addresses.addresses[i].id) != refdIDs.end())
    {
      WrappedID3D12Resource *resource =
          (WrappedID3D12Resource *)rm->GetCurrentResource(m_Addresses.addresses[i].id);
      mappableIDs.insert(resource->GetMappableID());
    }
  }
}

rdcarray<ID3D12Resource *> WrappedID3D12Resource::AddRefBuffersBeforeCapture(D3D12ResourceManager *rm)
{
  rdcarray<ID3D12Resource *> ret;

  rdcarray<GPUAddressRange> addresses;
  {
    SCOPED_READLOCK(m_Addresses.addressLock);
    addresses = m_Addresses.addresses;
  }

  for(size_t i = 0; i < addresses.size(); i++)
  {
    ID3D12Resource *resource = (ID3D12Resource *)rm->GetCurrentResource(m_Addresses.addresses[i].id);
    if(resource)
    {
      resource->AddRef();
      ret.push_back(resource);
    }
  }

  return ret;
}

void WrappedID3D12DescriptorHeap::MarkMutableIndex(uint32_t index)
{
  if(!mutableDescriptorBitmask)
    return;

  mutableDescriptorBitmask[index / 64] |= (1ULL << (index % 64));
}

bool WrappedID3D12DescriptorHeap::HasValidDescriptorCache(uint32_t index)
{
  if(!mutableDescriptorBitmask)
    return false;

  // don't cache mutable views. In theory we could but we'd need to know which ones were modified
  // mid-frame, to mark the cache as stale when initial contents are re-applied. This optimisation
  // is aimed at the assumption of a huge number of descriptors that don't change so we just don't
  // cache ones that change mid-frame
  if((mutableDescriptorBitmask[index / 64] & (1ULL << (index % 64))) != 0)
    return false;

  EnsureDescriptorCache();

  // anything that's not mutable is valid once it's been set at least once. Since we
  // zero-initialise, we use bind as a flag (it isn't retrieved from the cache since it depends on
  // the binding)
  return cachedDescriptors[index].type != DescriptorType::Unknown;
}

void WrappedID3D12DescriptorHeap::GetFromDescriptorCache(uint32_t index, Descriptor &view)
{
  if(!mutableDescriptorBitmask)
    return;

  EnsureDescriptorCache();

  view = cachedDescriptors[index];
}

void WrappedID3D12DescriptorHeap::EnsureDescriptorCache()
{
  if(!cachedDescriptors)
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = GetDesc();
    cachedDescriptors = new Descriptor[desc.NumDescriptors];
    RDCEraseMem(cachedDescriptors, sizeof(Descriptor) * desc.NumDescriptors);
  }
}

void WrappedID3D12DescriptorHeap::SetToDescriptorCache(uint32_t index, const Descriptor &view)
{
  if(!mutableDescriptorBitmask)
    return;

  EnsureDescriptorCache();

  cachedDescriptors[index] = view;
}

WrappedID3D12DescriptorHeap::WrappedID3D12DescriptorHeap(ID3D12DescriptorHeap *real,
                                                         WrappedID3D12Device *device,
                                                         const D3D12_DESCRIPTOR_HEAP_DESC &desc,
                                                         UINT UnpatchedNumDescriptors)
    : WrappedDeviceChild12(real, device)
{
  realCPUBase = real->GetCPUDescriptorHandleForHeapStart();
  if(desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    realGPUBase = real->GetGPUDescriptorHandleForHeapStart();

  increment = device->GetUnwrappedDescriptorIncrement(desc.Type);
  numDescriptors = UnpatchedNumDescriptors;

  descriptors = new D3D12Descriptor[desc.NumDescriptors];

  RDCEraseMem(descriptors, sizeof(D3D12Descriptor) * desc.NumDescriptors);
  for(UINT i = 0; i < desc.NumDescriptors; i++)
    descriptors[i].Setup(this, i);

  // only cache views for "large" descriptor heaps where we expect few will actually change
  // mid-frame
  if(IsReplayMode(device->GetState()) && desc.NumDescriptors > 1024)
  {
    size_t bitmaskSize = AlignUp(desc.NumDescriptors, 64U) / 64;

    cachedDescriptors = NULL;

    mutableDescriptorBitmask = new uint64_t[bitmaskSize];
    RDCEraseMem(mutableDescriptorBitmask, sizeof(uint64_t) * bitmaskSize);
  }
  else
  {
    cachedDescriptors = NULL;
    mutableDescriptorBitmask = NULL;
  }
}

WrappedID3D12DescriptorHeap::~WrappedID3D12DescriptorHeap()
{
  Shutdown();
  SAFE_DELETE_ARRAY(descriptors);
  SAFE_DELETE_ARRAY(cachedDescriptors);
  SAFE_DELETE_ARRAY(mutableDescriptorBitmask);
}

void WrappedID3D12PipelineState::ShaderEntry::BuildReflection()
{
  RDCCOMPILE_ASSERT(
      D3Dx_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT == D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
      "Mismatched vertex input count");

  MakeShaderReflection(m_DXBCFile, m_Details);
  m_Details->resourceId = GetResourceID();
}

rdcpair<uint32_t, uint32_t> FindMatchingRootParameter(const D3D12RootSignature *sig,
                                                      D3D12_SHADER_VISIBILITY visibility,
                                                      D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
                                                      uint32_t space, uint32_t bind)
{
  // search the root signature to find the matching entry and figure out the offset from the root binding
  for(uint32_t root = 0; root < sig->Parameters.size(); root++)
  {
    const D3D12RootSignatureParameter &param = sig->Parameters[root];

    if(param.ShaderVisibility != visibility && param.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL)
      continue;

    // identify root parameters
    if((
           // root constants
           (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
            rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV) ||
           // root CBV
           (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV &&
            rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV) ||
           // root SRV
           (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV &&
            rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV) ||
           // root UAV
           (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV &&
            rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)) &&
       // and matching space/binding
       param.Descriptor.RegisterSpace == space && param.Descriptor.ShaderRegister == bind)
    {
      // offset is unused since it's just the root parameter, so we indicate that with the offset
      return {root, ~0U};
    }

    uint32_t descOffset = 0;
    for(const D3D12_DESCRIPTOR_RANGE1 &range : param.ranges)
    {
      uint32_t rangeOffset = range.OffsetInDescriptorsFromTableStart;
      if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
        rangeOffset = descOffset;

      if(range.RangeType == rangeType && range.RegisterSpace == space &&
         range.BaseShaderRegister <= bind &&
         (range.NumDescriptors == ~0U || bind < range.BaseShaderRegister + range.NumDescriptors))
      {
        return {root, rangeOffset + (bind - range.BaseShaderRegister)};
      }

      descOffset = rangeOffset + range.NumDescriptors;
    }
  }

  // if not found above, and looking for samplers, look at static samplers next
  if(rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
  {
    // indicate that we're looking up static samplers
    uint32_t numRoots = (uint32_t)sig->Parameters.size();
    for(uint32_t samp = 0; samp < sig->StaticSamplers.size(); samp++)
    {
      if(sig->StaticSamplers[samp].RegisterSpace == space &&
         sig->StaticSamplers[samp].ShaderRegister == bind)
      {
        return {numRoots, samp};
      }
    }
  }

  return {~0U, 0};
}

void WrappedID3D12PipelineState::ProcessDescriptorAccess()
{
  if(m_AccessProcessed)
    return;
  m_AccessProcessed = true;

  const D3D12RootSignature *sig = NULL;
  if(graphics)
    sig = &((WrappedID3D12RootSignature *)graphics->pRootSignature)->sig;
  else if(compute)
    sig = &((WrappedID3D12RootSignature *)compute->pRootSignature)->sig;

  for(ShaderEntry *shad : {VS(), HS(), DS(), GS(), PS(), AS(), MS(), CS()})
  {
    if(!shad)
      continue;

    const ShaderReflection &refl = shad->GetDetails();

    D3D12_SHADER_VISIBILITY visibility;
    switch(refl.stage)
    {
      case ShaderStage::Vertex: visibility = D3D12_SHADER_VISIBILITY_VERTEX; break;
      case ShaderStage::Hull: visibility = D3D12_SHADER_VISIBILITY_HULL; break;
      case ShaderStage::Domain: visibility = D3D12_SHADER_VISIBILITY_DOMAIN; break;
      case ShaderStage::Geometry: visibility = D3D12_SHADER_VISIBILITY_GEOMETRY; break;
      case ShaderStage::Pixel: visibility = D3D12_SHADER_VISIBILITY_PIXEL; break;
      case ShaderStage::Amplification: visibility = D3D12_SHADER_VISIBILITY_AMPLIFICATION; break;
      case ShaderStage::Mesh: visibility = D3D12_SHADER_VISIBILITY_MESH; break;
      default: visibility = D3D12_SHADER_VISIBILITY_ALL; break;
    }

    DescriptorAccess access;
    access.stage = refl.stage;

    // we will store the root signature element in byteSize to be decoded into descriptorStore later
    access.byteSize = 0;

    staticDescriptorAccess.reserve(staticDescriptorAccess.size() + refl.constantBlocks.size() +
                                   refl.samplers.size() + refl.readOnlyResources.size() +
                                   refl.readWriteResources.size());

    RDCASSERT(refl.constantBlocks.size() < 0xffff, refl.constantBlocks.size());
    for(uint16_t i = 0; i < refl.constantBlocks.size(); i++)
    {
      const ConstantBlock &bind = refl.constantBlocks[i];

      // arrayed descriptors will be handled with bindless feedback
      if(bind.bindArraySize > 1)
        continue;

      access.type = DescriptorType::ConstantBuffer;
      access.index = i;
      rdctie(access.byteSize, access.byteOffset) =
          FindMatchingRootParameter(sig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                                    bind.fixedBindSetOrSpace, bind.fixedBindNumber);

      if(access.byteSize != ~0U)
        staticDescriptorAccess.push_back(access);
    }

    RDCASSERT(refl.samplers.size() < 0xffff, refl.samplers.size());
    for(uint16_t i = 0; i < refl.samplers.size(); i++)
    {
      const ShaderSampler &bind = refl.samplers[i];

      // arrayed descriptors will be handled with bindless feedback
      if(bind.bindArraySize > 1)
        continue;

      access.type = DescriptorType::Sampler;
      access.index = i;
      rdctie(access.byteSize, access.byteOffset) =
          FindMatchingRootParameter(sig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                    bind.fixedBindSetOrSpace, bind.fixedBindNumber);

      if(access.byteSize != ~0U)
        staticDescriptorAccess.push_back(access);
    }

    RDCASSERT(refl.readOnlyResources.size() < 0xffff, refl.readOnlyResources.size());
    for(uint16_t i = 0; i < refl.readOnlyResources.size(); i++)
    {
      const ShaderResource &bind = refl.readOnlyResources[i];

      // arrayed descriptors will be handled with bindless feedback
      if(bind.bindArraySize > 1)
        continue;

      access.type = refl.readOnlyResources[i].descriptorType;
      access.index = i;
      rdctie(access.byteSize, access.byteOffset) =
          FindMatchingRootParameter(sig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                    bind.fixedBindSetOrSpace, bind.fixedBindNumber);

      if(access.byteSize != ~0U)
        staticDescriptorAccess.push_back(access);
    }

    RDCASSERT(refl.readWriteResources.size() < 0xffff, refl.readWriteResources.size());
    for(uint16_t i = 0; i < refl.readWriteResources.size(); i++)
    {
      const ShaderResource &bind = refl.readWriteResources[i];

      // arrayed descriptors will be handled with bindless feedback
      if(bind.bindArraySize > 1)
        continue;

      access.type = refl.readWriteResources[i].descriptorType;
      access.index = i;
      rdctie(access.byteSize, access.byteOffset) =
          FindMatchingRootParameter(sig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                    bind.fixedBindSetOrSpace, bind.fixedBindNumber);

      if(access.byteSize != ~0U)
        staticDescriptorAccess.push_back(access);
    }
  }
}

UINT GetPlaneForSubresource(ID3D12Resource *res, int Subresource)
{
  D3D12_RESOURCE_DESC desc = res->GetDesc();

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    return 0;

  int mipLevels = desc.MipLevels;

  if(mipLevels == 0)
    mipLevels = CalcNumMips((int)desc.Width, 1, 1);

  UINT arraySlices = desc.DepthOrArraySize;
  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    arraySlices = 1;

  return Subresource / (mipLevels * arraySlices);
}

UINT GetMipForSubresource(ID3D12Resource *res, int Subresource)
{
  D3D12_RESOURCE_DESC desc = res->GetDesc();

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    return Subresource;

  int mipLevels = desc.MipLevels;

  if(mipLevels == 0)
    mipLevels = CalcNumMips((int)desc.Width, 1, 1);

  return (Subresource % mipLevels);
}

UINT GetSliceForSubresource(ID3D12Resource *res, int Subresource)
{
  D3D12_RESOURCE_DESC desc = res->GetDesc();

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    return Subresource;

  int mipLevels = desc.MipLevels;

  if(mipLevels == 0)
    mipLevels = CalcNumMips((int)desc.Width, 1, 1);

  return (Subresource / mipLevels) % desc.DepthOrArraySize;
}

UINT GetMipForDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D12_DSV_DIMENSION_TEXTURE1D: return view.Texture1D.MipSlice;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.MipSlice;
    case D3D12_DSV_DIMENSION_TEXTURE2D: return view.Texture2D.MipSlice;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.MipSlice;
    default: return 0;
  }
}

UINT GetSliceForDsv(const D3D12_DEPTH_STENCIL_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.FirstArraySlice;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.FirstArraySlice;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.FirstArraySlice;
    default: return 0;
  }
}

UINT GetMipForRtv(const D3D12_RENDER_TARGET_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D12_RTV_DIMENSION_TEXTURE1D: return view.Texture1D.MipSlice;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.MipSlice;
    case D3D12_RTV_DIMENSION_TEXTURE2D: return view.Texture2D.MipSlice;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.MipSlice;
    case D3D12_RTV_DIMENSION_TEXTURE3D: return view.Texture3D.MipSlice;
    default: return 0;
  }
}
UINT GetSliceForRtv(const D3D12_RENDER_TARGET_VIEW_DESC &view)
{
  switch(view.ViewDimension)
  {
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: return view.Texture1DArray.FirstArraySlice;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: return view.Texture2DArray.FirstArraySlice;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: return view.Texture2DMSArray.FirstArraySlice;
    default: return 0;
  }
}

D3D12_SHADER_RESOURCE_VIEW_DESC MakeSRVDesc(const D3D12_RESOURCE_DESC &desc)
{
  D3D12_SHADER_RESOURCE_VIEW_DESC ret = {};

  ret.Format = desc.Format;
  ret.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  bool arrayed = desc.DepthOrArraySize > 1;

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    // I don't think it's possible to create a SRV/SRV of a buffer with a NULL desc, but the docs
    // and debug layer are quite hard to be sure. Put in something sensible.

    ret.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    ret.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    ret.Buffer.StructureByteStride = 0;
    ret.Buffer.FirstElement = 0;
    ret.Buffer.NumElements = (UINT)desc.Width;
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    ret.ViewDimension = arrayed ? D3D12_SRV_DIMENSION_TEXTURE1DARRAY : D3D12_SRV_DIMENSION_TEXTURE1D;

    // shared between arrayed and not
    ret.Texture1D.MipLevels = desc.MipLevels;

    if(arrayed)
      ret.Texture1DArray.ArraySize = desc.DepthOrArraySize;
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
  {
    if(desc.SampleDesc.Count > 1)
    {
      ret.ViewDimension =
          arrayed ? D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D12_SRV_DIMENSION_TEXTURE2DMS;

      if(arrayed)
        ret.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
    }
    else
    {
      ret.ViewDimension =
          arrayed ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D;

      // shared between arrayed and not
      ret.Texture2D.MipLevels = desc.MipLevels;

      if(arrayed)
        ret.Texture2DArray.ArraySize = desc.DepthOrArraySize;
    }
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    ret.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;

    ret.Texture3D.MipLevels = desc.MipLevels;
  }

  return ret;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC MakeUAVDesc(const D3D12_RESOURCE_DESC &desc)
{
  D3D12_UNORDERED_ACCESS_VIEW_DESC ret = {};

  ret.Format = desc.Format;

  bool arrayed = desc.DepthOrArraySize > 1;

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    // I don't think it's possible to create a UAV/SRV of a buffer with a NULL desc, but the docs
    // and debug layer are quite hard to be sure. Put in something sensible.

    ret.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    ret.Buffer.NumElements = (UINT)desc.Width;
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    ret.ViewDimension = arrayed ? D3D12_UAV_DIMENSION_TEXTURE1DARRAY : D3D12_UAV_DIMENSION_TEXTURE1D;

    if(arrayed)
      ret.Texture1DArray.ArraySize = desc.DepthOrArraySize;
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
  {
    if(desc.SampleDesc.Count > 1)
    {
      ret.ViewDimension =
          arrayed ? D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY : D3D12_UAV_DIMENSION_TEXTURE2DMS;

      if(arrayed)
        ret.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
    }
    else
    {
      ret.ViewDimension =
          arrayed ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY : D3D12_UAV_DIMENSION_TEXTURE2D;

      if(arrayed)
        ret.Texture2DArray.ArraySize = desc.DepthOrArraySize;
    }
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    ret.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;

    ret.Texture3D.WSize = desc.DepthOrArraySize;
  }

  return ret;
}
