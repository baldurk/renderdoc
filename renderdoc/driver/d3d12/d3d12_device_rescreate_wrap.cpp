/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "d3d12_device.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_resources.h"

bool WrappedID3D12Device::Serialise_CreateResource(
    D3D12Chunk chunkType, ID3D12Heap *pHeap, UINT64 HeapOffset, D3D12_HEAP_PROPERTIES &props,
    D3D12_HEAP_FLAGS HeapFlags, D3D12_RESOURCE_DESC1 &desc, D3D12ResourceLayout InitialLayout,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, UINT NumCastableFormats,
    const DXGI_FORMAT *pCastableFormats, ResourceId pResource, uint64_t gpuAddress)
{
  rdcarray<DXGI_FORMAT> CastableFormats;
  CastableFormats.assign(pCastableFormats, NumCastableFormats);

  // if we're creating a placed resource
  if(pHeap)
  {
    D3D12_HEAP_DESC heapDesc = pHeap->GetDesc();

    // if the heap was from OpenExistingHeap* then we will have removed the shared flags from it
    // as it's CPU-visible and impossible to share.
    // That means any resources placed to it would have had this flag that we then need to remove
    // as well.
    if((heapDesc.Flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER) == 0)
      desc.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
  }

  // if we're creating a committed resource (only place where heap properties is set)
  if(props.Type == D3D12_HEAP_TYPE_UPLOAD && desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    // place large resources in local memory so that initial contents and maps can
    // be cached and copied on the GPU instead of memcpy'd from the CPU every time.
    // smaller resources it's better to just leave them as upload and map into them
    if(desc.Width >= 1024 * 1024)
    {
      RDCLOG("Remapping committed resource %s from upload to default for efficient replay",
             ToStr(pResource).c_str());
      props.Type = D3D12_HEAP_TYPE_DEFAULT;
      m_UploadResourceIds.insert(pResource);
    }
  }

  APIProps.YUVTextures |= IsYUVFormat(desc.Format);

  // always allow SRVs on replay so we can inspect resources
  desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

  // don't create resources non-resident
  HeapFlags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;

  // don't create displayable heaps (?!)
  HeapFlags &= ~D3D12_HEAP_FLAG_ALLOW_DISPLAY;

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && gpuAddress != 0)
  {
    GPUAddressRange range;
    range.start = gpuAddress;
    range.realEnd = gpuAddress + desc.Width;

    // if this is placed, the OOB end is all the way to the end of the heap, from where we're
    // placed, allowing accesses past the buffer but still in bounds of the heap.
    if(pHeap)
    {
      const UINT64 heapSize = pHeap->GetDesc().SizeInBytes;
      range.oobEnd = gpuAddress + (heapSize - HeapOffset);
    }
    else
    {
      range.oobEnd = range.realEnd;
    }

    range.id = pResource;

    m_OrigGPUAddresses.AddTo(range);
  }

  // check for device requirement
  switch(chunkType)
  {
    case D3D12Chunk::Device_CreateCommittedResource1:
    case D3D12Chunk::Device_CreateReservedResource1:
      if(!m_pDevice4)
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                         "Capture requires ID3D12Device4 which isn't available");
        return false;
      }
      break;
    case D3D12Chunk::Device_CreateCommittedResource2:
    case D3D12Chunk::Device_CreatePlacedResource1:
      if(!m_pDevice8)
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                         "Capture requires ID3D12Device8 which isn't available");
        return false;
      }
      break;
    case D3D12Chunk::Device_CreateCommittedResource3:
    case D3D12Chunk::Device_CreatePlacedResource2:
    case D3D12Chunk::Device_CreateReservedResource2:
      if(!m_pDevice10)
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                         "Capture requires ID3D12Device10 which isn't available");
        return false;
      }
      break;
    default: break;
  }

  D3D12_RESOURCE_DESC desc0 = {};
  memcpy(&desc0, &desc, sizeof(desc0));

  ID3D12Resource *ret = NULL;
  HRESULT hr = S_OK;

  // dispatch actual creation
  switch(chunkType)
  {
    case D3D12Chunk::Device_OpenSharedHandle:
    case D3D12Chunk::Device_CreateCommittedResource:
    {
      hr = m_pDevice->CreateCommittedResource(&props, HeapFlags, &desc0, InitialLayout.ToStates(),
                                              pOptimizedClearValue, __uuidof(ID3D12Resource),
                                              (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource1:
    {
      hr = m_pDevice4->CreateCommittedResource1(&props, HeapFlags, &desc0, InitialLayout.ToStates(),
                                                pOptimizedClearValue, NULL,
                                                __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource2:
    {
      hr = m_pDevice8->CreateCommittedResource2(&props, HeapFlags, &desc, InitialLayout.ToStates(),
                                                pOptimizedClearValue, NULL,
                                                __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource3:
    {
      hr = m_pDevice10->CreateCommittedResource3(
          &props, HeapFlags, &desc, InitialLayout.ToLayout(), pOptimizedClearValue, NULL,
          (UINT)CastableFormats.size(), CastableFormats.data(), __uuidof(ID3D12Resource),
          (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreatePlacedResource:
    {
      hr = m_pDevice->CreatePlacedResource(Unwrap(pHeap), HeapOffset, &desc0,
                                           InitialLayout.ToStates(), pOptimizedClearValue,
                                           __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreatePlacedResource1:
    {
      hr = m_pDevice8->CreatePlacedResource1(Unwrap(pHeap), HeapOffset, &desc,
                                             InitialLayout.ToStates(), pOptimizedClearValue,
                                             __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreatePlacedResource2:
    {
      hr = m_pDevice10->CreatePlacedResource2(Unwrap(pHeap), HeapOffset, &desc,
                                              InitialLayout.ToLayout(), pOptimizedClearValue,
                                              (UINT)CastableFormats.size(), CastableFormats.data(),
                                              __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreateReservedResource:
    {
      hr = m_pDevice->CreateReservedResource(&desc0, InitialLayout.ToStates(), pOptimizedClearValue,
                                             __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreateReservedResource1:
    {
      hr = m_pDevice4->CreateReservedResource1(&desc0, InitialLayout.ToStates(), pOptimizedClearValue,
                                               NULL, __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    case D3D12Chunk::Device_CreateReservedResource2:
    {
      hr = m_pDevice10->CreateReservedResource2(
          &desc0, InitialLayout.ToLayout(), pOptimizedClearValue, NULL, (UINT)CastableFormats.size(),
          CastableFormats.data(), __uuidof(ID3D12Resource), (void **)&ret);
      break;
    }
    default: break;
  }

  if(FAILED(hr))
  {
    SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                     "Failed recreating %s, HRESULT: %s", ToStr(chunkType).c_str(),
                     ToStr(hr).c_str());
    return false;
  }

  const char *ResourceTypeName = "?";

  switch(chunkType)
  {
    case D3D12Chunk::Device_OpenSharedHandle: ResourceTypeName = "Shared"; break;
    case D3D12Chunk::Device_CreateCommittedResource:
    case D3D12Chunk::Device_CreateCommittedResource1:
    case D3D12Chunk::Device_CreateCommittedResource2:
    case D3D12Chunk::Device_CreateCommittedResource3: ResourceTypeName = "Committed"; break;
    case D3D12Chunk::Device_CreatePlacedResource:
    case D3D12Chunk::Device_CreatePlacedResource1:
    case D3D12Chunk::Device_CreatePlacedResource2: ResourceTypeName = "Placed"; break;
    case D3D12Chunk::Device_CreateReservedResource:
    case D3D12Chunk::Device_CreateReservedResource1:
    case D3D12Chunk::Device_CreateReservedResource2: ResourceTypeName = "Reserved"; break;
    default:
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::InternalError,
                       "Invalid call to Serialise_CreateResource");
      return false;
    }
  }

  SetObjName(ret, StringFormat::Fmt("%s Resource %s %s", ResourceTypeName,
                                    ToStr(desc.Dimension).c_str(), ToStr(pResource).c_str()));

  ret = new WrappedID3D12Resource(ret, pHeap, HeapOffset, this, gpuAddress);

  switch(chunkType)
  {
    case D3D12Chunk::Device_CreateReservedResource:
    case D3D12Chunk::Device_CreateReservedResource1:
    case D3D12Chunk::Device_CreateReservedResource2:
      APIProps.SparseResources = true;
      m_SparseResources.insert(GetResID(ret));
    default: break;
  }

  GetResourceManager()->AddLiveResource(pResource, ret);

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    m_ModResources.insert(GetResID(ret));

  SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
  states.fill(GetNumSubresources(m_pDevice, &desc), InitialLayout);

  ResourceType type = ResourceType::Texture;
  const char *prefix = "Texture";

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    type = ResourceType::Buffer;
    if(InitialLayout.ToStates() == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
    {
      prefix = "AS Buffer";
      ((WrappedID3D12Resource *)ret)->MarkAsAccelerationStructureResource();
    }
    else
    {
      prefix = "Buffer";
    }
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    prefix = desc.DepthOrArraySize > 1 ? "1D TextureArray" : "1D Texture";

    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
      prefix = "1D Render Target";
    else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
      prefix = "1D Depth Target";
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
  {
    prefix = desc.DepthOrArraySize > 1 ? "2D TextureArray" : "2D Texture";

    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
      prefix = "2D Render Target";
    else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
      prefix = "2D Depth Target";
  }
  else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    prefix = "3D Texture";

    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
      prefix = "3D Render Target";
    else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
      prefix = "3D Depth Target";
  }

  AddResource(pResource, type, prefix);
  // ignored if there's no heap
  DerivedResource(pHeap, pResource);

  return true;
}

HRESULT WrappedID3D12Device::CreateResource(
    D3D12Chunk chunkType, ID3D12Heap *pHeap, UINT64 HeapOffset,
    const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    D3D12_RESOURCE_DESC1 desc, D3D12ResourceLayout InitialLayout,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, UINT NumCastableFormats,
    const DXGI_FORMAT *pCastableFormats, REFIID riidResource, void **ppvResource)
{
  if(riidResource != __uuidof(ID3D12Resource) && riidResource != __uuidof(ID3D12Resource1) &&
     riidResource != __uuidof(ID3D12Resource2))
    return E_NOINTERFACE;

  if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.SampleDesc.Count > 1)
  {
    // need to be able to create SRVs of MSAA textures to copy out their contents
    desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
  }

  D3D12_RESOURCE_DESC desc0 = {};
  memcpy(&desc0, &desc, sizeof(desc0));

  ID3D12Resource *realRes = NULL;
  HRESULT ret = E_NOINTERFACE;

  void **outPtr = (void **)&realRes;

  if(ppvResource == NULL)
    outPtr = NULL;

  switch(chunkType)
  {
    case D3D12Chunk::Device_OpenSharedHandle:
    {
      // already created externally
      realRes = (ID3D12Resource *)*ppvResource;
      ret = S_OK;
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice->CreateCommittedResource(
                              pHeapProperties, HeapFlags, &desc0, InitialLayout.ToStates(),
                              pOptimizedClearValue, __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource1:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice4->CreateCommittedResource1(
                              pHeapProperties, HeapFlags, &desc0, InitialLayout.ToStates(),
                              pOptimizedClearValue, pProtectedSession, __uuidof(ID3D12Resource),
                              outPtr));
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource2:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice8->CreateCommittedResource2(
                              pHeapProperties, HeapFlags, &desc, InitialLayout.ToStates(),
                              pOptimizedClearValue, pProtectedSession, __uuidof(ID3D12Resource),
                              outPtr));
      break;
    }
    case D3D12Chunk::Device_CreateCommittedResource3:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice10->CreateCommittedResource3(
                              pHeapProperties, HeapFlags, &desc, InitialLayout.ToLayout(),
                              pOptimizedClearValue, pProtectedSession, NumCastableFormats,
                              pCastableFormats, __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreatePlacedResource:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice->CreatePlacedResource(
                              Unwrap(pHeap), HeapOffset, &desc0, InitialLayout.ToStates(),
                              pOptimizedClearValue, __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreatePlacedResource1:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice8->CreatePlacedResource1(
                              Unwrap(pHeap), HeapOffset, &desc, InitialLayout.ToStates(),
                              pOptimizedClearValue, __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreatePlacedResource2:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice10->CreatePlacedResource2(
                              Unwrap(pHeap), HeapOffset, &desc, InitialLayout.ToLayout(),
                              pOptimizedClearValue, NumCastableFormats, pCastableFormats,
                              __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreateReservedResource:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice->CreateReservedResource(&desc0, InitialLayout.ToStates(),
                                                                  pOptimizedClearValue,
                                                                  __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreateReservedResource1:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice4->CreateReservedResource1(
                              &desc0, InitialLayout.ToStates(), pOptimizedClearValue,
                              pProtectedSession, __uuidof(ID3D12Resource), outPtr));
      break;
    }
    case D3D12Chunk::Device_CreateReservedResource2:
    {
      SERIALISE_TIME_CALL(ret = m_pDevice10->CreateReservedResource2(
                              &desc0, InitialLayout.ToLayout(), pOptimizedClearValue,
                              pProtectedSession, NumCastableFormats, pCastableFormats,
                              __uuidof(ID3D12Resource), outPtr));
      break;
    }
    default: break;
  }

  if(FAILED(ret))
  {
    CheckHRESULT(ret);
    return ret;
  }

  if(ppvResource == NULL)
    return ret;

  UINT NumSubresources = GetNumSubresources(m_pDevice, &desc);

  WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(realRes, pHeap, HeapOffset, this);

  if(IsCaptureMode(m_State))
  {
    if(HeapFlags & D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
      wrapped->Evict();

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(chunkType);

    switch(chunkType)
    {
      case D3D12Chunk::Device_OpenSharedHandle:
      {
        ID3D12DeviceChild *wrappedDeviceChild = wrapped;
        Serialise_OpenSharedHandle(ser, 0, riidResource, (void **)&wrappedDeviceChild);
        break;
      }
      case D3D12Chunk::Device_CreateCommittedResource:
      {
        Serialise_CreateCommittedResource(ser, pHeapProperties, HeapFlags, &desc0,
                                          InitialLayout.ToStates(), pOptimizedClearValue,
                                          riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreateCommittedResource1:
      {
        Serialise_CreateCommittedResource1(ser, pHeapProperties, HeapFlags, &desc0,
                                           InitialLayout.ToStates(), pOptimizedClearValue,
                                           pProtectedSession, riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreateCommittedResource2:
      {
        Serialise_CreateCommittedResource2(ser, pHeapProperties, HeapFlags, &desc,
                                           InitialLayout.ToStates(), pOptimizedClearValue,
                                           pProtectedSession, riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreateCommittedResource3:
      {
        Serialise_CreateCommittedResource3(ser, pHeapProperties, HeapFlags, &desc,
                                           InitialLayout.ToLayout(), pOptimizedClearValue,
                                           pProtectedSession, NumCastableFormats, pCastableFormats,
                                           riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreatePlacedResource:
      {
        Serialise_CreatePlacedResource(ser, pHeap, HeapOffset, &desc0, InitialLayout.ToStates(),
                                       pOptimizedClearValue, riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreatePlacedResource1:
      {
        Serialise_CreatePlacedResource1(ser, pHeap, HeapOffset, &desc, InitialLayout.ToStates(),
                                        pOptimizedClearValue, riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreatePlacedResource2:
      {
        Serialise_CreatePlacedResource2(ser, pHeap, HeapOffset, &desc, InitialLayout.ToLayout(),
                                        pOptimizedClearValue, NumCastableFormats, pCastableFormats,
                                        riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreateReservedResource:
      {
        Serialise_CreateReservedResource(ser, &desc0, InitialLayout.ToStates(),
                                         pOptimizedClearValue, riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreateReservedResource1:
      {
        Serialise_CreateReservedResource1(ser, &desc0, InitialLayout.ToStates(), pOptimizedClearValue,
                                          pProtectedSession, riidResource, (void **)&wrapped);
        break;
      }
      case D3D12Chunk::Device_CreateReservedResource2:
      {
        Serialise_CreateReservedResource2(
            ser, &desc0, InitialLayout.ToLayout(), pOptimizedClearValue, pProtectedSession,
            NumCastableFormats, pCastableFormats, riidResource, (void **)&wrapped);
        break;
      }
      default: break;
    }

    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
    record->type = Resource_Resource;
    record->Length = 0;
    wrapped->SetResourceRecord(record);

    if(desc0.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
       InitialLayout.ToStates() == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
    {
      wrapped->MarkAsAccelerationStructureResource();
    }
    else
    {
      record->m_MapsCount = NumSubresources;
      record->m_Maps = new D3D12ResourceRecord::MapData[record->m_MapsCount];
    }

    if(chunkType == D3D12Chunk::Device_CreateReservedResource ||
       chunkType == D3D12Chunk::Device_CreateReservedResource1 ||
       chunkType == D3D12Chunk::Device_CreateReservedResource2)
    {
      const UINT pageSize = 64 * 1024;

      if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      {
        record->sparseTable = new Sparse::PageTable;
        record->sparseTable->Initialise(desc.Width, pageSize);
      }
      else
      {
        D3D12_PACKED_MIP_INFO mipTail = {};
        D3D12_TILE_SHAPE tileShape = {};

        m_pDevice->GetResourceTiling(wrapped->GetReal(), NULL, &mipTail, &tileShape, NULL, 0, NULL);

        UINT texDepth = 1;
        UINT texSlices = desc.DepthOrArraySize;
        if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        {
          texDepth = desc.DepthOrArraySize;
          texSlices = 1;
        }

        RDCASSERT(mipTail.NumStandardMips + mipTail.NumPackedMips == desc.MipLevels,
                  mipTail.NumStandardMips, mipTail.NumPackedMips, desc.MipLevels);
        record->sparseTable = new Sparse::PageTable;
        record->sparseTable->Initialise(
            {(uint32_t)desc.Width, desc.Height, texDepth}, desc.MipLevels, texSlices, pageSize,
            {tileShape.WidthInTexels, tileShape.HeightInTexels, tileShape.DepthInTexels},
            mipTail.NumStandardMips, mipTail.StartTileIndexInOverallResource * pageSize,
            (mipTail.StartTileIndexInOverallResource + mipTail.NumTilesForPackedMips) * pageSize,
            mipTail.NumTilesForPackedMips * pageSize * texSlices);
      }

      {
        SCOPED_LOCK(m_SparseLock);
        m_SparseResources.insert(wrapped->GetResourceID());
      }
    }

    record->AddChunk(scope.Get());
    if(pHeap)
      record->AddParent(GetRecord(pHeap));

    GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
  }
  else
  {
    GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
  }

  {
    SCOPED_LOCK(m_ResourceStatesLock);
    SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

    states.fill(NumSubresources, InitialLayout);

    m_BindlessFrameRefs[wrapped->GetResourceID()] = BindlessRefTypeForRes(wrapped);
  }

  // while actively capturing we keep all buffers around to prevent the address lookup from
  // losing addresses we might need (or the manageable but annoying problem of an address being
  // re-used)
  {
    SCOPED_READLOCK(m_CapTransitionLock);
    if(IsActiveCapturing(m_State))
    {
      wrapped->AddRef();
      m_RefBuffers.push_back(wrapped);
      if(m_BindlessResourceUseActive)
        GetResourceManager()->MarkResourceFrameReferenced(wrapped->GetResourceID(),
                                                          BindlessRefTypeForRes(wrapped));
    }
  }

  *ppvResource = (ID3D12Resource *)wrapped;

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommittedResource(
    SerialiserType &ser, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(props, *pHeapProperties).Named("pHeapProperties"_lit);
  SERIALISE_ELEMENT(HeapFlags);
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialResourceState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT_LOCAL(guid, riidResource).Named("riidResource"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    return Serialise_CreateResource(D3D12Chunk::Device_CreateCommittedResource, NULL, 0, props,
                                    HeapFlags, desc,
                                    D3D12ResourceLayout::FromStates(InitialResourceState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                                     D3D12_HEAP_FLAGS HeapFlags,
                                                     const D3D12_RESOURCE_DESC *pDesc,
                                                     D3D12_RESOURCE_STATES InitialResourceState,
                                                     const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                     REFIID riidResource, void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateCommittedResource, NULL, 0, pHeapProperties,
                        HeapFlags, *pDesc, D3D12ResourceLayout::FromStates(InitialResourceState),
                        pOptimizedClearValue, NULL, 0, NULL, riidResource, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreatePlacedResource(
    SerialiserType &ser, ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc,
    D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
    void **ppvResource)
{
  SERIALISE_ELEMENT(pHeap).Important();
  SERIALISE_ELEMENT(HeapOffset).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_HEAP_PROPERTIES props = {};
    return Serialise_CreateResource(D3D12Chunk::Device_CreatePlacedResource, pHeap, HeapOffset,
                                    props, D3D12_HEAP_FLAG_NONE, desc,
                                    D3D12ResourceLayout::FromStates(InitialState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                  const D3D12_RESOURCE_DESC *pDesc,
                                                  D3D12_RESOURCE_STATES InitialState,
                                                  const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                  REFIID riid, void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreatePlacedResource, pHeap, HeapOffset, NULL,
                        D3D12_HEAP_FLAG_NONE, *pDesc, D3D12ResourceLayout::FromStates(InitialState),
                        pOptimizedClearValue, NULL, 0, NULL, riid, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateReservedResource(
    SerialiserType &ser, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_HEAP_PROPERTIES props = {};
    return Serialise_CreateResource(D3D12Chunk::Device_CreateReservedResource, NULL, 0, props,
                                    D3D12_HEAP_FLAG_NONE, desc,
                                    D3D12ResourceLayout::FromStates(InitialState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc,
                                                    D3D12_RESOURCE_STATES InitialState,
                                                    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                    REFIID riid, void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateReservedResource, NULL, 0, NULL,
                        D3D12_HEAP_FLAG_NONE, *pDesc, D3D12ResourceLayout::FromStates(InitialState),
                        pOptimizedClearValue, NULL, 0, NULL, riid, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_OpenSharedHandle(SerialiserType &ser, HANDLE, REFIID riid,
                                                     void **ppvObj)
{
  SERIALISE_ELEMENT_LOCAL(ResourceRIID, riid).Important();

  SERIALISE_CHECK_READ_ERRORS();

  bool isRes = ResourceRIID == __uuidof(ID3D12Resource) ||
               ResourceRIID == __uuidof(ID3D12Resource1) ||
               ResourceRIID == __uuidof(ID3D12Resource2);
  bool isFence = ResourceRIID == __uuidof(ID3D12Fence) || ResourceRIID == __uuidof(ID3D12Fence1);
  bool isHeap = ResourceRIID == __uuidof(ID3D12Heap) || ResourceRIID == __uuidof(ID3D12Heap1);
  if(isFence)
  {
    ID3D12Fence *fence = NULL;
    if(ser.IsWriting())
    {
      fence = (ID3D12Fence *)(ID3D12DeviceChild *)*ppvObj;
      if(ResourceRIID == __uuidof(ID3D12Fence1))
        fence = (ID3D12Fence1 *)(ID3D12DeviceChild *)*ppvObj;
    }

    SERIALISE_ELEMENT_LOCAL(resourceId, GetResID(fence));

    UINT64 fakeInitialValue = 0;
    D3D12_FENCE_FLAGS fakeFlags = D3D12_FENCE_FLAG_NONE;

    // maybe in future this can be determined?
    SERIALISE_ELEMENT_LOCAL(initialValue, fakeInitialValue);
    SERIALISE_ELEMENT_LOCAL(flags, fakeFlags);

    if(IsReplayingAndReading())
    {
      HRESULT hr;
      ID3D12Fence *ret;
      hr = m_pDevice->CreateFence(initialValue, flags, __uuidof(ID3D12Fence), (void **)&ret);
      if(FAILED(hr))
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                         "Failed creating shared fence, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        ret = new WrappedID3D12Fence(ret, this);

        GetResourceManager()->AddLiveResource(resourceId, ret);
      }

      AddResource(resourceId, ResourceType::Sync, "Fence");
    }
  }
  else if(isRes)
  {
    D3D12_RESOURCE_DESC desc;
    D3D12_HEAP_PROPERTIES heapProperties;
    D3D12_HEAP_FLAGS heapFlags;

    ID3D12Resource *res = NULL;
    if(ser.IsWriting())
    {
      res = (ID3D12Resource *)(ID3D12DeviceChild *)*ppvObj;
      if(ResourceRIID == __uuidof(ID3D12Resource1))
        res = (ID3D12Resource1 *)(ID3D12DeviceChild *)*ppvObj;
      else if(ResourceRIID == __uuidof(ID3D12Resource2))
        res = (ID3D12Resource2 *)(ID3D12DeviceChild *)*ppvObj;
      desc = res->GetDesc();
      res->GetHeapProperties(&heapProperties, &heapFlags);
    }

    SERIALISE_ELEMENT_LOCAL(resourceId, GetResID(res));
    SERIALISE_ELEMENT(desc);
    SERIALISE_ELEMENT(heapProperties);
    SERIALISE_ELEMENT(heapFlags);

    if(IsReplayingAndReading())
    {
      // the runtime doesn't like us telling it what DENY heap flags will be set, remove them
      heapFlags &= ~(D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES |
                     D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES);

      Serialise_CreateResource(D3D12Chunk::Device_OpenSharedHandle, NULL, 0, heapProperties,
                               heapFlags, desc,
                               D3D12ResourceLayout::FromStates(D3D12_RESOURCE_STATE_COMMON), NULL,
                               0, NULL, resourceId, 0);
    }
  }
  else if(isHeap)
  {
    D3D12_HEAP_DESC desc;

    ID3D12Heap *heap = NULL;
    if(ser.IsWriting())
    {
      heap = (ID3D12Heap *)(ID3D12DeviceChild *)*ppvObj;
      if(ResourceRIID == __uuidof(ID3D12Heap1))
        heap = (ID3D12Heap1 *)(ID3D12DeviceChild *)*ppvObj;
      desc = heap->GetDesc();
    }

    SERIALISE_ELEMENT_LOCAL(resourceId, GetResID(heap));
    SERIALISE_ELEMENT(desc);

    if(IsReplayingAndReading())
    {
      HRESULT hr;
      ID3D12Heap *ret;
      hr = m_pDevice->CreateHeap(&desc, __uuidof(ID3D12Heap), (void **)&ret);
      if(FAILED(hr))
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                         "Failed creating shared heap, HRESULT: %s", ToStr(hr).c_str());
        return false;
      }
      else
      {
        ret = new WrappedID3D12Heap(ret, this);

        GetResourceManager()->AddLiveResource(resourceId, ret);
      }

      AddResource(resourceId, ResourceType::Memory, "Heap");
    }
  }
  else
  {
    RDCERR("Unknown type of resource being shared");
  }

  return true;
}

HRESULT WrappedID3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
  if(ppvObj == NULL)
    return E_INVALIDARG;

  HRESULT hr;

  SERIALISE_TIME_CALL(hr = m_pDevice->OpenSharedHandle(NTHandle, riid, ppvObj));

  if(FAILED(hr))
  {
    IUnknown *unk = (IUnknown *)*ppvObj;
    SAFE_RELEASE(unk);
    return hr;
  }

  return OpenSharedHandleInternal(D3D12Chunk::Device_OpenSharedHandle, D3D12_HEAP_FLAG_NONE, riid,
                                  ppvObj);
}

HRESULT WrappedID3D12Device::OpenSharedHandleInternal(D3D12Chunk chunkType,
                                                      D3D12_HEAP_FLAGS HeapFlags, REFIID riid,
                                                      void **ppvObj)
{
  if(IsReplayMode(m_State))
  {
    RDCERR("Don't support opening shared handle during replay.");
    return E_NOTIMPL;
  }

  bool isDXGIRes = riid == __uuidof(IDXGIResource) || riid == __uuidof(IDXGIResource1);
  bool isRes = riid == __uuidof(ID3D12Resource) || riid == __uuidof(ID3D12Resource1) ||
               riid == __uuidof(ID3D12Resource2);
  bool isFence = riid == __uuidof(ID3D12Fence) || riid == __uuidof(ID3D12Fence1);
  bool isHeap = riid == __uuidof(ID3D12Heap) || riid == __uuidof(ID3D12Heap1);
  bool isDeviceChild = riid == __uuidof(ID3D12DeviceChild);
  bool isIUnknown = riid == __uuidof(IUnknown);

  IID riid_internal = riid;
  void *ret = *ppvObj;

  if(isIUnknown)
  {
    // same as device child but we're even more in the dark. Hope against hope it's a
    // ID3D12DeviceChild
    IUnknown *real = (IUnknown *)ret;

    ID3D12DeviceChild *d3d12child = NULL;
    isDeviceChild =
        SUCCEEDED(real->QueryInterface(__uuidof(ID3D12DeviceChild), (void **)&d3d12child)) &&
        d3d12child;
    SAFE_RELEASE(real);

    if(isDeviceChild)
    {
      riid_internal = __uuidof(ID3D12DeviceChild);
      ret = (void *)d3d12child;
    }
    else
    {
      SAFE_RELEASE(d3d12child);
      return E_NOINTERFACE;
    }
  }

  if(isDeviceChild)
  {
    // In this case we need to find out what the actual underlying type is
    // Should be one of ID3D12Heap, ID3D12Resource, ID3D12Fence
    ID3D12DeviceChild *real = (ID3D12DeviceChild *)ret;

    ID3D12Resource *d3d12Res = NULL;
    ID3D12Fence *d3d12Fence = NULL;
    ID3D12Heap *d3d12Heap = NULL;
    isRes = SUCCEEDED(real->QueryInterface(__uuidof(ID3D12Resource), (void **)&d3d12Res)) && d3d12Res;
    isFence =
        SUCCEEDED(real->QueryInterface(__uuidof(ID3D12Fence), (void **)&d3d12Fence)) && d3d12Fence;
    isHeap = SUCCEEDED(real->QueryInterface(__uuidof(ID3D12Heap), (void **)&d3d12Heap)) && d3d12Heap;
    SAFE_RELEASE(real);

    if(isRes)
    {
      riid_internal = __uuidof(ID3D12Resource);
      ret = (void *)d3d12Res;
    }
    else if(isFence)
    {
      riid_internal = __uuidof(ID3D12Fence);
      ret = (void *)d3d12Fence;
    }
    else if(isHeap)
    {
      riid_internal = __uuidof(ID3D12Heap);
      ret = (void *)d3d12Heap;
    }
    else
    {
      SAFE_RELEASE(d3d12Res);
      SAFE_RELEASE(d3d12Fence);
      SAFE_RELEASE(d3d12Heap);
      return E_NOINTERFACE;
    }
  }

  if(isDXGIRes || isRes || isFence || isHeap)
  {
    HRESULT hr = S_OK;

    if(isDXGIRes)
    {
      IDXGIResource *dxgiRes = (IDXGIResource *)ret;
      if(riid_internal == __uuidof(IDXGIResource1))
        dxgiRes = (IDXGIResource1 *)ret;

      ID3D12Resource *d3d12Res = NULL;
      hr = dxgiRes->QueryInterface(__uuidof(ID3D12Resource), (void **)&d3d12Res);

      // if we can't get a d3d11Res then we can't properly wrap this resource,
      // whatever it is.
      if(FAILED(hr) || d3d12Res == NULL)
      {
        SAFE_RELEASE(d3d12Res);
        SAFE_RELEASE(dxgiRes);
        return E_NOINTERFACE;
      }

      // release this interface
      SAFE_RELEASE(dxgiRes);

      // and use this one, so it'll be casted back below
      ret = (void *)d3d12Res;
      isRes = true;
    }

    ID3D12DeviceChild *wrappedDeviceChild = NULL;
    D3D12ResourceRecord *record = NULL;

    if(isFence)
    {
      ID3D12Fence *real = (ID3D12Fence *)ret;
      if(riid_internal == __uuidof(ID3D12Fence1))
        real = (ID3D12Fence1 *)ret;

      WrappedID3D12Fence *wrapped = new WrappedID3D12Fence(real, this);

      wrappedDeviceChild = wrapped;

      record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Fence;
      record->Length = 0;
      wrapped->SetResourceRecord(record);
    }
    else if(isRes)
    {
      ID3D12Resource *real = (ID3D12Resource *)ret;
      hr = CreateResource(D3D12Chunk::Device_OpenSharedHandle, NULL, 0, NULL, D3D12_HEAP_FLAG_NONE,
                          real->GetDesc(),
                          D3D12ResourceLayout::FromStates(D3D12_RESOURCE_STATE_COMMON), NULL, NULL,
                          0, NULL, riid_internal, (void **)&real);

      // use queryinterface to get the right interface into ppvObj, then release the reference
      real->QueryInterface(riid, ppvObj);
      real->Release();

      return hr;
    }
    else if(isHeap)
    {
      WrappedID3D12Heap *wrapped = new WrappedID3D12Heap((ID3D12Heap *)ret, this);

      if(HeapFlags & D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
        wrapped->Evict();

      wrappedDeviceChild = wrapped;

      record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Heap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);
    }

    // use queryinterface to get the right interface into ppvObj, then release the reference
    wrappedDeviceChild->QueryInterface(riid, ppvObj);
    wrappedDeviceChild->Release();

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(chunkType);
    Serialise_OpenSharedHandle(ser, 0, riid_internal, (void **)&wrappedDeviceChild);

    record->AddChunk(scope.Get());

    return S_OK;
  }

  RDCERR("Unknown OpenSharedResourceInternal GUID: %s", ToStr(riid).c_str());

  IUnknown *unk = (IUnknown *)*ppvObj;
  SAFE_RELEASE(unk);

  return E_NOINTERFACE;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommittedResource1(
    SerialiserType &ser, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(props, *pHeapProperties).Named("pHeapProperties"_lit);
  SERIALISE_ELEMENT(HeapFlags);
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialResourceState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riidResource).Named("riidResource"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    return Serialise_CreateResource(D3D12Chunk::Device_CreateCommittedResource1, NULL, 0, props,
                                    HeapFlags, desc,
                                    D3D12ResourceLayout::FromStates(InitialResourceState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommittedResource1(
    const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateCommittedResource1, NULL, 0, pHeapProperties,
                        HeapFlags, *pDesc, D3D12ResourceLayout::FromStates(InitialResourceState),
                        pOptimizedClearValue, pProtectedSession, 0, NULL, riidResource, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateReservedResource1(
    SerialiserType &ser, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_HEAP_PROPERTIES props = {};
    return Serialise_CreateResource(D3D12Chunk::Device_CreateReservedResource1, NULL, 0, props,
                                    D3D12_HEAP_FLAG_NONE, desc,
                                    D3D12ResourceLayout::FromStates(InitialState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateReservedResource1(
    _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid,
    _COM_Outptr_opt_ void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateReservedResource1, NULL, 0, NULL,
                        D3D12_HEAP_FLAG_NONE, *pDesc, D3D12ResourceLayout::FromStates(InitialState),
                        pOptimizedClearValue, pProtectedSession, 0, NULL, riid, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommittedResource2(
    SerialiserType &ser, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(props, *pHeapProperties).Named("pHeapProperties"_lit);
  SERIALISE_ELEMENT(HeapFlags);
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialResourceState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riidResource).Named("riidResource"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    return Serialise_CreateResource(D3D12Chunk::Device_CreateCommittedResource2, NULL, 0, props,
                                    HeapFlags, desc,
                                    D3D12ResourceLayout::FromStates(InitialResourceState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommittedResource2(
    const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateCommittedResource2, NULL, 0, pHeapProperties,
                        HeapFlags, *pDesc, D3D12ResourceLayout::FromStates(InitialResourceState),
                        pOptimizedClearValue, pProtectedSession, 0, NULL, riidResource, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreatePlacedResource1(
    SerialiserType &ser, ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc,
    D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
    void **ppvResource)
{
  SERIALISE_ELEMENT(pHeap).Important();
  SERIALISE_ELEMENT(HeapOffset).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_HEAP_PROPERTIES props = {};
    return Serialise_CreateResource(D3D12Chunk::Device_CreatePlacedResource1, pHeap, HeapOffset,
                                    props, D3D12_HEAP_FLAG_NONE, desc,
                                    D3D12ResourceLayout::FromStates(InitialState),
                                    pOptimizedClearValue, 0, NULL, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreatePlacedResource1(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                   const D3D12_RESOURCE_DESC1 *pDesc,
                                                   D3D12_RESOURCE_STATES InitialState,
                                                   const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                   REFIID riid, void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreatePlacedResource1, pHeap, HeapOffset, NULL,
                        D3D12_HEAP_FLAG_NONE, *pDesc, D3D12ResourceLayout::FromStates(InitialState),
                        pOptimizedClearValue, NULL, 0, NULL, riid, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommittedResource3(
    SerialiserType &ser, const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC1 *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats,
    const DXGI_FORMAT *pCastableFormats, REFIID riidResource, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(props, *pHeapProperties).Named("pHeapProperties"_lit);
  SERIALISE_ELEMENT(HeapFlags);
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialLayout);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT(NumCastableFormats);
  SERIALISE_ELEMENT_ARRAY(pCastableFormats, NumCastableFormats);
  SERIALISE_ELEMENT_LOCAL(guid, riidResource).Named("riidResource"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    return Serialise_CreateResource(D3D12Chunk::Device_CreateCommittedResource3, NULL, 0, props,
                                    HeapFlags, desc, D3D12ResourceLayout::FromLayout(InitialLayout),
                                    pOptimizedClearValue, NumCastableFormats, pCastableFormats,
                                    pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommittedResource3(
    _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    _In_ const D3D12_RESOURCE_DESC1 *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats,
    _In_opt_count_(NumCastableFormats) const DXGI_FORMAT *pCastableFormats, REFIID riidResource,
    _COM_Outptr_opt_ void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateCommittedResource3, NULL, 0, pHeapProperties,
                        HeapFlags, *pDesc, D3D12ResourceLayout::FromLayout(InitialLayout),
                        pOptimizedClearValue, pProtectedSession, NumCastableFormats,
                        pCastableFormats, riidResource, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreatePlacedResource2(
    SerialiserType &ser, ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc,
    D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    UINT32 NumCastableFormats, const DXGI_FORMAT *pCastableFormats, REFIID riid, void **ppvResource)
{
  SERIALISE_ELEMENT(pHeap).Important();
  SERIALISE_ELEMENT(HeapOffset).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialLayout);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  SERIALISE_ELEMENT(NumCastableFormats);
  SERIALISE_ELEMENT_ARRAY(pCastableFormats, NumCastableFormats);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_HEAP_PROPERTIES props = {};
    return Serialise_CreateResource(
        D3D12Chunk::Device_CreatePlacedResource2, pHeap, HeapOffset, props, D3D12_HEAP_FLAG_NONE,
        desc, D3D12ResourceLayout::FromLayout(InitialLayout), pOptimizedClearValue,
        NumCastableFormats, pCastableFormats, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreatePlacedResource2(
    _In_ ID3D12Heap *pHeap, UINT64 HeapOffset, _In_ const D3D12_RESOURCE_DESC1 *pDesc,
    D3D12_BARRIER_LAYOUT InitialLayout, _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    UINT32 NumCastableFormats, _In_opt_count_(NumCastableFormats) const DXGI_FORMAT *pCastableFormats,
    REFIID riid, _COM_Outptr_opt_ void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreatePlacedResource2, pHeap, HeapOffset, NULL,
                        D3D12_HEAP_FLAG_NONE, *pDesc,
                        D3D12ResourceLayout::FromLayout(InitialLayout), pOptimizedClearValue, NULL,
                        NumCastableFormats, pCastableFormats, riid, ppvResource);
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateReservedResource2(
    SerialiserType &ser, const D3D12_RESOURCE_DESC *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession,
    UINT32 NumCastableFormats, const DXGI_FORMAT *pCastableFormats, REFIID riid, void **ppvResource)
{
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit).Important();
  SERIALISE_ELEMENT(InitialLayout);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT(NumCastableFormats);
  SERIALISE_ELEMENT_ARRAY(pCastableFormats, NumCastableFormats);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
      .Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    D3D12_HEAP_PROPERTIES props = {};
    return Serialise_CreateResource(
        D3D12Chunk::Device_CreateReservedResource2, NULL, 0, props, D3D12_HEAP_FLAG_NONE, desc,
        D3D12ResourceLayout::FromLayout(InitialLayout), pOptimizedClearValue, NumCastableFormats,
        pCastableFormats, pResource, gpuAddress);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateReservedResource2(
    _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats,
    _In_opt_count_(NumCastableFormats) const DXGI_FORMAT *pCastableFormats, REFIID riid,
    _COM_Outptr_opt_ void **ppvResource)
{
  return CreateResource(D3D12Chunk::Device_CreateReservedResource2, NULL, 0, NULL,
                        D3D12_HEAP_FLAG_NONE, *pDesc,
                        D3D12ResourceLayout::FromLayout(InitialLayout), pOptimizedClearValue,
                        pProtectedSession, NumCastableFormats, pCastableFormats, riid, ppvResource);
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommittedResource,
                                const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc,
                                D3D12_RESOURCE_STATES InitialResourceState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreatePlacedResource, ID3D12Heap *pHeap,
                                UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc,
                                D3D12_RESOURCE_STATES InitialState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateReservedResource,
                                const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, OpenSharedHandle, HANDLE NTHandle,
                                REFIID riid, void **ppvObj);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommittedResource1,
                                const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc,
                                D3D12_RESOURCE_STATES InitialResourceState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                ID3D12ProtectedResourceSession *pProtectedSession,
                                REFIID riidResource, void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateReservedResource1,
                                const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommittedResource2,
                                const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *pDesc,
                                D3D12_RESOURCE_STATES InitialResourceState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                ID3D12ProtectedResourceSession *pProtectedSession,
                                REFIID riidResource, void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreatePlacedResource1, ID3D12Heap *pHeap,
                                UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc,
                                D3D12_RESOURCE_STATES InitialState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
                                void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommittedResource3,
                                const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *pDesc,
                                D3D12_BARRIER_LAYOUT InitialLayout,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                ID3D12ProtectedResourceSession *pProtectedSession,
                                UINT NumCastableFormats, const DXGI_FORMAT *pCastableFormats,
                                REFIID riidResource, void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreatePlacedResource2, ID3D12Heap *pHeap,
                                UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc,
                                D3D12_BARRIER_LAYOUT InitialLayout,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                UINT NumCastableFormats, const DXGI_FORMAT *pCastableFormats,
                                REFIID riid, void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateReservedResource2,
                                const D3D12_RESOURCE_DESC *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                ID3D12ProtectedResourceSession *pProtectedSession,
                                UINT NumCastableFormats, const DXGI_FORMAT *pCastableFormats,
                                REFIID riid, void **ppvResource);
