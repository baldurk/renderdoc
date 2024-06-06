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
#include "d3d12_shader_cache.h"

GPUAddressRangeTracker WrappedID3D12Resource::m_Addresses;
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
  if(D3D12AccelerationStructure::IsAlloc(ptr))
    return Resource_AccelerationStructure;

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

WRAPPED_POOL_INST(D3D12AccelerationStructure);

D3D12AccelerationStructure::D3D12AccelerationStructure(WrappedID3D12Device *wrappedDevice,
                                                       WrappedID3D12Resource *bufferRes,
                                                       D3D12BufferOffset bufferOffset,
                                                       UINT64 byteSize)
    : WrappedDeviceChild12(NULL, wrappedDevice),
      m_asbWrappedResource(bufferRes),
      m_asbWrappedResourceBufferOffset(bufferOffset),
      byteSize(byteSize)
{
}

D3D12AccelerationStructure::~D3D12AccelerationStructure()
{
  Shutdown();
}

bool WrappedID3D12Resource::CreateAccStruct(D3D12BufferOffset bufferOffset, UINT64 byteSize,
                                            D3D12AccelerationStructure **accStruct)
{
  SCOPED_LOCK(m_accStructResourcesCS);
  if(m_accelerationStructMap.find(bufferOffset) == m_accelerationStructMap.end())
  {
    m_accelerationStructMap[bufferOffset] =
        new D3D12AccelerationStructure(m_pDevice, this, bufferOffset, byteSize);

    if(accStruct)
    {
      *accStruct = m_accelerationStructMap[bufferOffset];

      if(IsCaptureMode(m_pDevice->GetState()))
      {
        DeleteOverlappingAccStructsInRangeAtOffset(bufferOffset);
      }
    }

    return true;
  }

  return false;
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

  // release all ASs during capture. During replay these will be destroyed themselves
  if(IsCaptureMode(m_pDevice->GetState()))
  {
    for(auto it = m_accelerationStructMap.begin(); it != m_accelerationStructMap.end(); ++it)
      SAFE_RELEASE(it->second);
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
    ID3D12Resource *resource = (ID3D12Resource *)rm->GetCurrentResource(addresses[i].id);
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

  m_OriginalWrappedGPUBase = (uint64_t)descriptors;

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

  MakeShaderReflection(m_DXBCFile, {}, m_Details);
  m_Details->resourceId = GetResourceID();
}

rdcpair<uint32_t, uint32_t> FindMatchingRootParameter(const D3D12RootSignature &sig,
                                                      D3D12_SHADER_VISIBILITY visibility,
                                                      D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
                                                      uint32_t space, uint32_t bind)
{
  // search the root signature to find the matching entry and figure out the offset from the root binding
  for(uint32_t root = 0; root < sig.Parameters.size(); root++)
  {
    const D3D12RootSignatureParameter &param = sig.Parameters[root];

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
    uint32_t numRoots = (uint32_t)sig.Parameters.size();
    for(uint32_t samp = 0; samp < sig.StaticSamplers.size(); samp++)
    {
      if(sig.StaticSamplers[samp].RegisterSpace == space &&
         sig.StaticSamplers[samp].ShaderRegister == bind)
      {
        return {numRoots, samp};
      }
    }
  }

  return {~0U, 0};
}

void WrappedID3D12PipelineState::FetchRootSig(D3D12ShaderCache *shaderCache)
{
  if(compute)
  {
    if(compute->pRootSignature)
    {
      usedSig = ((WrappedID3D12RootSignature *)compute->pRootSignature)->sig;
    }
    else
    {
      D3D12_SHADER_BYTECODE desc = CS()->GetDesc();
      if(DXBC::DXBCContainer::CheckForRootSig(desc.pShaderBytecode, desc.BytecodeLength))
      {
        usedSig = shaderCache->GetRootSig(desc.pShaderBytecode, desc.BytecodeLength);
      }
      else
      {
        RDCWARN("Couldn't find root signature in either desc or compute shader");
      }
    }
  }
  else if(graphics)
  {
    if(graphics->pRootSignature)
    {
      usedSig = ((WrappedID3D12RootSignature *)graphics->pRootSignature)->sig;
    }
    else
    {
      // if there is any root signature it must match in all shaders, so we just have to find the first one.
      for(ShaderEntry *shad : {PS(), VS(), HS(), DS(), GS(), AS(), MS()})
      {
        if(shad)
        {
          D3D12_SHADER_BYTECODE desc = shad->GetDesc();

          if(DXBC::DXBCContainer::CheckForRootSig(desc.pShaderBytecode, desc.BytecodeLength))
          {
            usedSig = shaderCache->GetRootSig(desc.pShaderBytecode, desc.BytecodeLength);
            return;
          }
        }
      }

      RDCWARN("Couldn't find root signature in either desc or any bound shader");
    }
  }
}

void WrappedID3D12PipelineState::ProcessDescriptorAccess()
{
  if(m_AccessProcessed)
    return;
  m_AccessProcessed = true;

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
          FindMatchingRootParameter(usedSig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
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
          FindMatchingRootParameter(usedSig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
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
          FindMatchingRootParameter(usedSig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
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
          FindMatchingRootParameter(usedSig, visibility, D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                    bind.fixedBindSetOrSpace, bind.fixedBindNumber);

      if(access.byteSize != ~0U)
        staticDescriptorAccess.push_back(access);
    }
  }
}

D3D12ShaderExportDatabase::D3D12ShaderExportDatabase(ResourceId id,
                                                     D3D12RaytracingResourceAndUtilHandler *rayManager,
                                                     ID3D12StateObjectProperties *obj)
    : RefCounter12(NULL), objectOriginalId(id), m_RayManager(rayManager), m_StateObjectProps(obj)
{
  m_RayManager->RegisterExportDatabase(this);
}

D3D12ShaderExportDatabase::~D3D12ShaderExportDatabase()
{
  for(D3D12ShaderExportDatabase *parent : parents)
  {
    SAFE_RELEASE(parent);
  }

  m_RayManager->UnregisterExportDatabase(this);
}

void D3D12ShaderExportDatabase::PopulateDatabase(size_t NumSubobjects,
                                                 const D3D12_STATE_SUBOBJECT *subobjects)
{
  // store the default local root signature - if we only find one in the whole state object then it becomes default
  ID3D12RootSignature *defaultRoot = NULL;
  bool unassocDefaultValid = false;
  bool explicitDefault = false;
  bool unassocDXILDefaultValid = false;
  uint32_t dxilDefaultRoot = ~0U;

  rdcarray<rdcpair<rdcstr, uint32_t>> explicitRootSigAssocs;
  rdcarray<rdcstr> explicitDefaultDxilAssocs;
  rdcarray<rdcpair<rdcstr, rdcstr>> explicitDxilAssocs;
  rdcflatmap<rdcstr, uint32_t> dxilLocalRootSigs;

  rdcarray<rdcpair<rdcstr, uint32_t>> inheritedRootSigAssocs;
  rdcarray<rdcpair<rdcstr, rdcstr>> inheritedDXILRootSigAssocs;
  rdcflatmap<rdcstr, uint32_t> inheritedDXILLocalRootSigs;

  // fill shader exports list as well as local root signature lookups.
  // shader exports that can be queried come from two sources:
  // - hit groups
  // - exports from a DXIL library
  // - exports from a collection
  for(size_t i = 0; i < NumSubobjects; i++)
  {
    if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP)
    {
      D3D12_HIT_GROUP_DESC *desc = (D3D12_HIT_GROUP_DESC *)subobjects[i].pDesc;
      AddExport(StringFormat::Wide2UTF8(desc->HitGroupExport));

      rdcarray<rdcstr> shaders;
      if(desc->IntersectionShaderImport)
        shaders.push_back(StringFormat::Wide2UTF8(desc->IntersectionShaderImport));
      if(desc->AnyHitShaderImport)
        shaders.push_back(StringFormat::Wide2UTF8(desc->AnyHitShaderImport));
      if(desc->ClosestHitShaderImport)
        shaders.push_back(StringFormat::Wide2UTF8(desc->ClosestHitShaderImport));

      // register the hit group so that if we get associations with the individual shaders we
      // can apply that up to the hit group
      AddLastHitGroupShaders(std::move(shaders));
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY)
    {
      D3D12_DXIL_LIBRARY_DESC *dxil = (D3D12_DXIL_LIBRARY_DESC *)subobjects[i].pDesc;

      if(dxil->NumExports > 0)
      {
        for(UINT e = 0; e < dxil->NumExports; e++)
        {
          // Name is always the name used for exports - if renaming then the renamed-from name
          // is only used to lookup in the dxil library and not for any associations-by-name
          AddExport(StringFormat::Wide2UTF8(dxil->pExports[e].Name));
        }
      }
      else
      {
        // hard part, we need to parse the DXIL to get the entry points
        DXBC::DXBCContainer container(
            bytebuf((byte *)dxil->DXILLibrary.pShaderBytecode, dxil->DXILLibrary.BytecodeLength),
            rdcstr(), GraphicsAPI::D3D12, ~0U, ~0U);

        rdcarray<ShaderEntryPoint> entries = container.GetEntryPoints();

        for(const ShaderEntryPoint &e : entries)
          AddExport(e.name);
      }

      // TODO: register local root signature subobjects into dxilLocalRootSigs. Override
      // anything in there, unlike the import from a collection below.
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION)
    {
      D3D12_EXISTING_COLLECTION_DESC *coll = (D3D12_EXISTING_COLLECTION_DESC *)subobjects[i].pDesc;

      WrappedID3D12StateObject *stateObj = (WrappedID3D12StateObject *)coll->pExistingCollection;

      if(coll->NumExports > 0)
      {
        for(UINT e = 0; e < coll->NumExports; e++)
          InheritCollectionExport(stateObj->exports, StringFormat::Wide2UTF8(coll->pExports[e].Name),
                                  StringFormat::Wide2UTF8(coll->pExports[e].ExportToRename
                                                              ? coll->pExports[e].ExportToRename
                                                              : coll->pExports[e].Name));
      }
      else
      {
        InheritAllCollectionExports(stateObj->exports);
      }

      // inherit explicit associations from the collection as lowest priority
      inheritedRootSigAssocs.append(stateObj->exports->danglingRootSigAssocs);
      inheritedDXILRootSigAssocs.append(stateObj->exports->danglingDXILRootSigAssocs);

      for(auto it = stateObj->exports->danglingDXILLocalRootSigs.begin();
          it != stateObj->exports->danglingDXILLocalRootSigs.end(); ++it)
      {
        // don't override any local root signatures with the same name we already have. Not sure
        // how this conflict should be resolved properly?
        if(dxilLocalRootSigs.find(it->first) == dxilLocalRootSigs.end())
          dxilLocalRootSigs[it->first] = it->second;
      }
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
    {
      // ignore these if an explicit default association has been made
      if(!explicitDefault)
      {
        // if multiple root signatures are defined, then there can't be an unspecified default
        unassocDefaultValid = defaultRoot == NULL;
        defaultRoot = ((D3D12_LOCAL_ROOT_SIGNATURE *)subobjects[i].pDesc)->pLocalRootSignature;
      }
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
    {
      D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION *assoc =
          (D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION *)subobjects[i].pDesc;

      const D3D12_STATE_SUBOBJECT *other = assoc->pSubobjectToAssociate;

      // only care about associating local root signatures
      if(other->Type == D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE)
      {
        ID3D12RootSignature *root = ((D3D12_LOCAL_ROOT_SIGNATURE *)other->pDesc)->pLocalRootSignature;

        WrappedID3D12RootSignature *wrappedRoot = (WrappedID3D12RootSignature *)root;

        // if there are no exports this is an explicit default association. We assume this
        // matches and doesn't conflict
        if(assoc->NumExports == NULL)
        {
          explicitDefault = true;
          defaultRoot = root;
        }
        else
        {
          // otherwise record the explicit associations - these may refer to exports that
          // haven't been seen yet so we record them locally
          for(UINT e = 0; e < assoc->NumExports; e++)
            explicitRootSigAssocs.push_back(
                {StringFormat::Wide2UTF8(assoc->pExports[e]), wrappedRoot->localRootSigIdx});
        }
      }
    }
    else if(subobjects[i].Type == D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
    {
      D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION *assoc =
          (D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION *)subobjects[i].pDesc;

      rdcstr other = StringFormat::Wide2UTF8(assoc->SubobjectToAssociate);

      // we can't tell yet if this is a local root signature or not so we have to store it regardless
      {
        // if there are no exports this is an explicit default association, but we don't know if
        // it's for a local root signature...
        if(assoc->NumExports == NULL)
        {
          explicitDefaultDxilAssocs.push_back(other);
        }
        else
        {
          // otherwise record the explicit associations - these may refer to exports that
          // haven't been seen yet so we record them locally
          for(UINT e = 0; e < assoc->NumExports; e++)
            explicitDxilAssocs.push_back({StringFormat::Wide2UTF8(assoc->pExports[e]), other});
        }
      }
    }
  }

  // now that we have all exports registered, apply all associations we have in order of
  // priority to get the right

  for(size_t i = 0; i < explicitRootSigAssocs.size(); i++)
  {
    ApplyRoot(SubObjectPriority::CodeExplicitAssociation, explicitRootSigAssocs[i].first,
              explicitRootSigAssocs[i].second);
  }

  if(explicitDefault)
  {
    WrappedID3D12RootSignature *wrappedRoot = (WrappedID3D12RootSignature *)defaultRoot;

    ApplyDefaultRoot(SubObjectPriority::CodeExplicitDefault, wrappedRoot->localRootSigIdx);
  }
  // shouldn't be possible to have both explicit and implicit defaults?
  else if(unassocDefaultValid)
  {
    WrappedID3D12RootSignature *wrappedRoot = (WrappedID3D12RootSignature *)defaultRoot;

    ApplyDefaultRoot(SubObjectPriority::CodeImplicitDefault, wrappedRoot->localRootSigIdx);
  }

  for(size_t i = 0; i < explicitDxilAssocs.size(); i++)
  {
    auto it = dxilLocalRootSigs.find(explicitDxilAssocs[i].second);

    if(it == dxilLocalRootSigs.end())
      continue;

    uint32_t localRootSigIdx = it->second;

    ApplyRoot(SubObjectPriority::DXILExplicitAssociation, explicitDxilAssocs[i].first,
              localRootSigIdx);
  }

  for(size_t i = 0; i < explicitDefaultDxilAssocs.size(); i++)
  {
    auto it = dxilLocalRootSigs.find(explicitDefaultDxilAssocs[i]);

    if(it == dxilLocalRootSigs.end())
      continue;

    uint32_t localRootSigIdx = it->second;

    ApplyDefaultRoot(SubObjectPriority::DXILExplicitDefault, localRootSigIdx);

    // only expect one local root signature - the array is because we can't tell the type of the
    // default subobject when we encounter it
    break;
  }

  if(unassocDXILDefaultValid)
  {
    ApplyDefaultRoot(SubObjectPriority::DXILImplicitDefault, dxilDefaultRoot);
  }

  // we assume it's not possible to inherit two different explicit associations for a single export

  for(size_t i = 0; i < inheritedRootSigAssocs.size(); i++)
  {
    ApplyRoot(SubObjectPriority::CollectionExplicitAssociation, inheritedRootSigAssocs[i].first,
              inheritedRootSigAssocs[i].second);
  }
  for(size_t i = 0; i < inheritedDXILRootSigAssocs.size(); i++)
  {
    auto it = dxilLocalRootSigs.find(inheritedDXILRootSigAssocs[i].second);

    if(it == dxilLocalRootSigs.end())
      continue;

    uint32_t localRootSigIdx = it->second;

    ApplyRoot(SubObjectPriority::CollectionExplicitAssociation, inheritedDXILRootSigAssocs[i].first,
              localRootSigIdx);
  }

  danglingRootSigAssocs.swap(inheritedRootSigAssocs);
  danglingDXILRootSigAssocs.swap(inheritedDXILRootSigAssocs);
  danglingDXILLocalRootSigs.swap(dxilLocalRootSigs);

  UpdateHitGroupAssociations();
}

void D3D12ShaderExportDatabase::AddExport(const rdcstr &exportName)
{
  bool mangled = false;
  rdcstr unmangledName;

  if(exportName.size() > 2 && exportName[0] == '\x1' && exportName[1] == '?')
  {
    int idx = exportName.indexOf('@');
    if(idx > 2)
    {
      unmangledName = exportName.substr(2, idx - 2);
      mangled = true;
    }
  }

  void *identifier = NULL;
  // shader identifiers seem to be only accessible via unmangled names
  if(m_StateObjectProps)
    identifier = m_StateObjectProps->GetShaderIdentifier(
        StringFormat::UTF82Wide(mangled ? unmangledName : exportName).c_str());
  const bool complete = identifier != NULL;

  {
    // store the wrapped identifier here in this database, ready to return to the application in
    // this object or any child objects.
    wrappedIdentifiers.push_back({objectOriginalId, (uint32_t)ownExports.size()});

    // store the unwrapping information to go into the giant lookup table
    ownExports.push_back({});
    // if there's a real identifier then store it. But we track this regardless so that we can
    // know the root signature for hitgroup-component shaders. If this export is inherited then it
    // will be detected as incomplete and copied and patched in the child
    if(identifier)
      memcpy(ownExports.back().real, identifier, sizeof(ShaderIdentifier));
    // a local root signature may never get specified, so default to none
    ownExports.back().rootSigPrio = SubObjectPriority::NotYetDefined;
    ownExports.back().localRootSigIndex = 0xffff;
  }

  exportLookups.emplace_back(exportName, unmangledName, complete);
}

void D3D12ShaderExportDatabase::InheritCollectionExport(D3D12ShaderExportDatabase *existing,
                                                        const rdcstr &nameToExport,
                                                        const rdcstr &nameInExisting)
{
  if(!parents.contains(existing))
  {
    parents.push_back(existing);
    existing->AddRef();
  }

  for(size_t i = 0; i < existing->exportLookups.size(); i++)
  {
    if(existing->exportLookups[i].name == nameInExisting ||
       existing->exportLookups[i].altName == nameInExisting)
    {
      InheritExport(nameInExisting, existing, i);

      // if we renamed, now that we found the right export in the existing collection use the
      // desired name going forward. This may still find the existing identifier as that hasn't
      // necessarily changed
      if(nameToExport != nameInExisting)
      {
        exportLookups.back().name = nameToExport;
        exportLookups.back().altName.clear();

        if(exportLookups.back().hitgroup)
          hitGroups.back().first = nameToExport;
      }
    }
  }
}

void D3D12ShaderExportDatabase::InheritExport(const rdcstr &exportName,
                                              D3D12ShaderExportDatabase *existing, size_t i)
{
  void *identifier = NULL;
  if(m_StateObjectProps)
    identifier = m_StateObjectProps->GetShaderIdentifier(StringFormat::UTF82Wide(exportName).c_str());

  wrappedIdentifiers.push_back(existing->wrappedIdentifiers[i]);
  exportLookups.push_back(existing->exportLookups[i]);

  // if this export wasn't previously complete, consider it exported in this object
  // note that identifier may be NULL if this is a shader that can't be used on its own like any
  // hit, but we want to keep it in our export list so we can track its root signature to update
  // the hit group's root signature. Since there is only one level of collection => RT PSO this
  // won't cause too much wasted exports
  // we don't inherit non-complete identifiers when doing AddToStateObject so this doesn't apply
  if(!exportLookups.back().complete)
  {
    ownExports.push_back({});

    // we expect this identifier to have come from the object we're inheriting
    RDCASSERTEQUAL(wrappedIdentifiers.back().id, existing->objectOriginalId);
    // which means we can copy any root signature it had associated even if it wasn't complete
    ownExports.back() = existing->ownExports[wrappedIdentifiers.back().index];

    // now set the identifier, if we got one
    if(identifier)
      memcpy(ownExports.back().real, identifier, sizeof(ShaderIdentifier));

    // and re-point this to point to ourselves when queried as we have the best data for it.
    wrappedIdentifiers.back() = {objectOriginalId, (uint32_t)ownExports.size()};

    // if this is an incomplete hitgroup, also grab the hitgroup component data
    if(exportLookups.back().hitgroup)
    {
      for(size_t h = 0; h < existing->hitGroups.size(); h++)
      {
        if(existing->hitGroups[h].first == exportName)
        {
          hitGroups.push_back(existing->hitGroups[h]);
          break;
        }
      }
    }
  }
}

void D3D12ShaderExportDatabase::ApplyRoot(SubObjectPriority priority, const rdcstr &exportName,
                                          uint32_t localRootSigIndex)
{
  for(size_t i = 0; i < exportLookups.size(); i++)
  {
    if(exportLookups[i].name == exportName || exportLookups[i].altName == exportName)
    {
      ApplyRoot(wrappedIdentifiers[i], priority, localRootSigIndex);
      break;
    }
  }
}

void D3D12ShaderExportDatabase::ApplyRoot(const ShaderIdentifier &identifier,
                                          SubObjectPriority priority, uint32_t localRootSigIndex)
{
  if(identifier.id == objectOriginalId)
  {
    // set this anywhere we have a looser/lower priority association already (including the most
    // common case presumably where one isn't set at all)
    ExportedIdentifier &exported = ownExports[identifier.index];
    if(exported.rootSigPrio < priority)
    {
      exported.rootSigPrio = priority;
      exported.localRootSigIndex = (uint16_t)localRootSigIndex;
    }
  }
}

void D3D12ShaderExportDatabase::AddLastHitGroupShaders(rdcarray<rdcstr> &&shaders)
{
  exportLookups.back().hitgroup = true;
  hitGroups.emplace_back(exportLookups.back().name, shaders);
}

void D3D12ShaderExportDatabase::UpdateHitGroupAssociations()
{
  // for each hit group
  for(size_t h = 0; h < hitGroups.size(); h++)
  {
    // find it in the exports, as it could have been dangling before
    for(size_t e = 0; e < exportLookups.size(); e++)
    {
      if(hitGroups[h].first == exportLookups[e].name)
      {
        // if the export is our own (ie. not complete and finished in a parent), we might need to
        // update its root sig
        if(wrappedIdentifiers[e].id == objectOriginalId)
        {
          // if the hit group got a code association already we assume it must match, but a DXIL
          // association or a default association could be overridden since it's unclear if a
          // hitgroup is a 'candidate' for default
          if(ownExports[wrappedIdentifiers[e].index].rootSigPrio !=
             SubObjectPriority::CodeExplicitAssociation)
          {
            // for each export, find it and try to update the root signature
            for(const rdcstr &shaderExport : hitGroups[h].second)
            {
              for(size_t e2 = 0; e2 < exportLookups.size(); e2++)
              {
                if(shaderExport == exportLookups[e2].name || shaderExport == exportLookups[e2].altName)
                {
                  RDCASSERTEQUAL(wrappedIdentifiers[e2].id, objectOriginalId);
                  uint32_t idx = wrappedIdentifiers[e2].index;
                  ApplyRoot(wrappedIdentifiers[e], ownExports[idx].rootSigPrio,
                            ownExports[idx].localRootSigIndex);

                  // don't keep looking at exports, we found this shader
                  break;
                }
              }

              // if we've inherited an explicit code association from a component shader, that
              // also must match so we can stop looking. Otherwise we keep looking to try and find
              // a 'better' association that can't be overridden
              if(ownExports[wrappedIdentifiers[e].index].rootSigPrio ==
                 SubObjectPriority::CodeExplicitAssociation)
                break;
            }
          }
        }

        // found this hit group, don't keep looking
        break;
      }
    }
  }
}

void D3D12ShaderExportDatabase::InheritAllCollectionExports(D3D12ShaderExportDatabase *existing)
{
  if(!parents.contains(existing))
  {
    parents.push_back(existing);
    existing->AddRef();
  }

  wrappedIdentifiers.reserve(wrappedIdentifiers.size() + existing->wrappedIdentifiers.size());
  exportLookups.reserve(exportLookups.size() + existing->exportLookups.size());
  for(size_t i = 0; i < existing->exportLookups.size(); i++)
  {
    InheritExport(existing->exportLookups[i].name, existing, i);
  }
}

void D3D12ShaderExportDatabase::ApplyDefaultRoot(SubObjectPriority priority,
                                                 uint32_t localRootSigIndex)
{
  for(size_t i = 0; i < wrappedIdentifiers.size(); i++)
    ApplyRoot(wrappedIdentifiers[i], priority, localRootSigIndex);
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
