/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2023 Baldur Karlsson
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

D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE WrappedID3D12Device::GetResourceAllocationInfo2(
    UINT visibleMask, UINT numResourceDescs,
    _In_reads_(numResourceDescs) const D3D12_RESOURCE_DESC1 *pResourceDescs,
    _Out_writes_opt_(numResourceDescs) D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1)
{
  return m_pDevice8->GetResourceAllocationInfo2(visibleMask, numResourceDescs, pResourceDescs,
                                                pResourceAllocationInfo1);
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

    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      GPUAddressRange range;
      range.start = gpuAddress;
      range.end = gpuAddress + desc.Width;
      range.id = pResource;

      m_GPUAddresses.AddTo(range);
    }

    ID3D12Resource *ret = NULL;
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice8)
    {
      hr = m_pDevice8->CreateCommittedResource2(&props, HeapFlags, &desc, InitialResourceState,
                                                pOptimizedClearValue, NULL, guid, (void **)&ret);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device8 which isn't available");
      return false;
    }

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating committed resource, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Committed Resource %s ID %s",
                                        ToStr(desc.Dimension).c_str(), ToStr(pResource).c_str()));

      ret = new WrappedID3D12Resource(ret, this);

      GetResourceManager()->AddLiveResource(pResource, ret);

      if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        m_ModResources.insert(GetResID(ret));

      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
      D3D12_RESOURCE_DESC desc0;
      memcpy(&desc0, &desc, sizeof(desc0));
      states.fill(GetNumSubresources(m_pDevice, &desc0), InitialResourceState);

      ResourceType type = ResourceType::Texture;
      const char *prefix = "Texture";

      if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      {
        type = ResourceType::Buffer;
        prefix = "Buffer";
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
    }
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommittedResource2(
    const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
  if(ppvResource == NULL)
    return m_pDevice8->CreateCommittedResource2(pHeapProperties, HeapFlags, pDesc,
                                                InitialResourceState, pOptimizedClearValue,
                                                Unwrap(pProtectedSession), riidResource, NULL);

  if(riidResource != __uuidof(ID3D12Resource) && riidResource != __uuidof(ID3D12Resource1) &&
     riidResource != __uuidof(ID3D12Resource2))
    return E_NOINTERFACE;

  const D3D12_RESOURCE_DESC1 *pCreateDesc = pDesc;
  D3D12_RESOURCE_DESC1 localDesc;

  if(pDesc && pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && pDesc->SampleDesc.Count > 1)
  {
    localDesc = *pDesc;
    // need to be able to create SRVs of MSAA textures to copy out their contents
    localDesc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    pCreateDesc = &localDesc;
  }

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice8->CreateCommittedResource2(
                          pHeapProperties, HeapFlags, pCreateDesc, InitialResourceState,
                          pOptimizedClearValue, Unwrap(pProtectedSession), riidResource, &realptr));

  ID3D12Resource *real = NULL;
  if(riidResource == __uuidof(ID3D12Resource))
    real = (ID3D12Resource *)realptr;
  else if(riidResource == __uuidof(ID3D12Resource1))
    real = (ID3D12Resource1 *)realptr;
  else if(riidResource == __uuidof(ID3D12Resource2))
    real = (ID3D12Resource2 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommittedResource2);
      Serialise_CreateCommittedResource2(ser, pHeapProperties, HeapFlags, pDesc,
                                         InitialResourceState, pOptimizedClearValue,
                                         pProtectedSession, riidResource, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
      D3D12_RESOURCE_DESC desc0;
      memcpy(&desc0, pDesc, sizeof(desc0));
      record->m_MapsCount = GetNumSubresources(this, &desc0);
      record->m_Maps = new D3D12ResourceRecord::MapData[record->m_MapsCount];

      record->AddChunk(scope.Get());

      GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
      D3D12_RESOURCE_DESC desc0;
      memcpy(&desc0, pDesc, sizeof(desc0));
      states.fill(GetNumSubresources(m_pDevice, &desc0), InitialResourceState);

      m_BindlessFrameRefs[wrapped->GetResourceID()] = BindlessRefTypeForRes(wrapped);
    }

    if(riidResource == __uuidof(ID3D12Resource))
      *ppvResource = (ID3D12Resource *)wrapped;
    else if(riidResource == __uuidof(ID3D12Resource1))
      *ppvResource = (ID3D12Resource1 *)wrapped;
    else if(riidResource == __uuidof(ID3D12Resource2))
      *ppvResource = (ID3D12Resource2 *)wrapped;

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
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreatePlacedResource1(
    SerialiserType &ser, ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc,
    D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid,
    void **ppvResource)
{
  SERIALISE_ELEMENT(pHeap).Important();
  SERIALISE_ELEMENT(HeapOffset);
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
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
    if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      GPUAddressRange range;
      range.start = gpuAddress;
      range.end = gpuAddress + Descriptor.Width;
      range.id = pResource;

      m_GPUAddresses.AddTo(range);
    }

    APIProps.YUVTextures |= IsYUVFormat(Descriptor.Format);

    // always allow SRVs on replay so we can inspect resources
    Descriptor.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    D3D12_HEAP_DESC heapDesc = pHeap->GetDesc();

    // if the heap was from OpenExistingHeap* then we will have removed the shared flags from it as
    // it's CPU-visible and impossible to share.
    // That means any resources placed to it would have had this flag that we then need to remove as
    // well.
    if((heapDesc.Flags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER) == 0)
      Descriptor.Flags &= ~D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;

    ID3D12Resource *ret = NULL;
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice8)
    {
      hr = m_pDevice8->CreatePlacedResource1(Unwrap(pHeap), HeapOffset, &Descriptor, InitialState,
                                             pOptimizedClearValue, guid, (void **)&ret);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device8 which isn't available");
      return false;
    }

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating placed resource, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Placed Resource %s %s", ToStr(Descriptor.Dimension).c_str(),
                                        ToStr(pResource).c_str()));

      ret = new WrappedID3D12Resource(ret, this);

      GetResourceManager()->AddLiveResource(pResource, ret);

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        m_ModResources.insert(GetResID(ret));

      // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
      D3D12_RESOURCE_DESC desc0;
      memcpy(&desc0, &Descriptor, sizeof(desc0));
      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      states.fill(GetNumSubresources(m_pDevice, &desc0), InitialState);
    }

    ResourceType type = ResourceType::Texture;
    const char *prefix = "Texture";

    if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
      type = ResourceType::Buffer;
      prefix = "Buffer";
    }
    else if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    {
      prefix = Descriptor.DepthOrArraySize > 1 ? "1D TextureArray" : "1D Texture";

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        prefix = "1D Render Target";
      else if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        prefix = "1D Depth Target";
    }
    else if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
      prefix = Descriptor.DepthOrArraySize > 1 ? "2D TextureArray" : "2D Texture";

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        prefix = "2D Render Target";
      else if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        prefix = "2D Depth Target";
    }
    else if(Descriptor.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
      prefix = "3D Texture";

      if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        prefix = "3D Render Target";
      else if(Descriptor.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        prefix = "3D Depth Target";
    }

    AddResource(pResource, type, prefix);
    DerivedResource(pHeap, pResource);
  }

  return true;
}

HRESULT WrappedID3D12Device::CreatePlacedResource1(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                   const D3D12_RESOURCE_DESC1 *pDesc,
                                                   D3D12_RESOURCE_STATES InitialState,
                                                   const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                   REFIID riid, void **ppvResource)
{
  if(ppvResource == NULL)
    return m_pDevice8->CreatePlacedResource1(Unwrap(pHeap), HeapOffset, pDesc, InitialState,
                                             pOptimizedClearValue, riid, NULL);

  if(riid != __uuidof(ID3D12Resource))
    return E_NOINTERFACE;

  const D3D12_RESOURCE_DESC1 *pCreateDesc = pDesc;
  D3D12_RESOURCE_DESC1 localDesc;

  if(pDesc && pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && pDesc->SampleDesc.Count > 1)
  {
    localDesc = *pDesc;
    // need to be able to create SRVs of MSAA textures to copy out their contents
    localDesc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    pCreateDesc = &localDesc;
  }

  ID3D12Resource *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice8->CreatePlacedResource1(Unwrap(pHeap), HeapOffset, pCreateDesc,
                                                              InitialState, pOptimizedClearValue,
                                                              riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Resource *wrapped = new WrappedID3D12Resource(real, this);

    wrapped->SetHeap(pHeap);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreatePlacedResource1);
      Serialise_CreatePlacedResource1(ser, pHeap, HeapOffset, pDesc, InitialState,
                                      pOptimizedClearValue, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
      D3D12_RESOURCE_DESC desc0;
      memcpy(&desc0, pDesc, sizeof(desc0));
      record->m_MapsCount = GetNumSubresources(this, &desc0);
      record->m_Maps = new D3D12ResourceRecord::MapData[record->m_MapsCount];

      RDCASSERT(pHeap);

      record->AddParent(GetRecord(pHeap));
      record->AddChunk(scope.Get());

      GetResourceManager()->MarkDirtyResource(wrapped->GetResourceID());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    {
      SCOPED_LOCK(m_ResourceStatesLock);
      SubresourceStateVector &states = m_ResourceStates[wrapped->GetResourceID()];

      // D3D12_RESOURCE_DESC is the same as the start of D3D12_RESOURCE_DESC1
      D3D12_RESOURCE_DESC desc0;
      memcpy(&desc0, pDesc, sizeof(desc0));
      states.fill(GetNumSubresources(m_pDevice, &desc0), InitialState);

      m_BindlessFrameRefs[wrapped->GetResourceID()] = BindlessRefTypeForRes(wrapped);
    }

    if(riid == __uuidof(ID3D12Resource))
      *ppvResource = (ID3D12Resource *)wrapped;
    else if(riid == __uuidof(ID3D12Resource1))
      *ppvResource = (ID3D12Resource1 *)wrapped;
    else if(riid == __uuidof(ID3D12Resource2))
      *ppvResource = (ID3D12Resource2 *)wrapped;

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
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

void STDMETHODCALLTYPE WrappedID3D12Device::CreateSamplerFeedbackUnorderedAccessView(
    _In_opt_ ID3D12Resource *pTargetedResource, _In_opt_ ID3D12Resource *pFeedbackResource,
    _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  RDCERR("CreateSamplerFeedbackUnorderedAccessView called but sampler feedback is not supported!");
}

void STDMETHODCALLTYPE WrappedID3D12Device::GetCopyableFootprints1(
    _In_ const D3D12_RESOURCE_DESC1 *pResourceDesc,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources, UINT64 BaseOffset,
    _Out_writes_opt_(NumSubresources) D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
    _Out_writes_opt_(NumSubresources) UINT *pNumRows,
    _Out_writes_opt_(NumSubresources) UINT64 *pRowSizeInBytes, _Out_opt_ UINT64 *pTotalBytes)
{
  return m_pDevice8->GetCopyableFootprints1(pResourceDesc, FirstSubresource, NumSubresources,
                                            BaseOffset, pLayouts, pNumRows, pRowSizeInBytes,
                                            pTotalBytes);
}

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
