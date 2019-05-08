/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include "driver/ihv/amd/official/DXExt/AmdExtD3D.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3DCommandListMarkerApi.h"
#include "d3d12_command_list.h"
#include "d3d12_resources.h"

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateCommandList1(SerialiserType &ser, UINT nodeMask,
                                                       D3D12_COMMAND_LIST_TYPE type,
                                                       D3D12_COMMAND_LIST_FLAGS flags, REFIID riid,
                                                       void **ppCommandList)
{
  SERIALISE_ELEMENT(nodeMask);
  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(flags);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pCommandList,
                          ((WrappedID3D12GraphicsCommandList *)*ppCommandList)->GetResourceID())
      .TypedAs("ID3D12GraphicsCommandList *"_lit);

  // this chunk is purely for user information and consistency, the command buffer we allocate is
  // a dummy and is not used for anything.

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ID3D12GraphicsCommandList *list = NULL;
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice4)
      hr = CreateCommandList1(nodeMask, type, flags, __uuidof(ID3D12GraphicsCommandList),
                              (void **)&list);
    else
      RDCERR("Replaying a without D3D12.4 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else if(list)
    {
      // don't have to close it, as there's no implicit reset
      GetResourceManager()->AddLiveResource(pCommandList, list);
    }

    AddResource(pCommandList, ResourceType::CommandBuffer, "Command List");
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateCommandList1(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                                                D3D12_COMMAND_LIST_FLAGS flags, REFIID riid,
                                                void **ppCommandList)
{
  if(ppCommandList == NULL)
    return m_pDevice4->CreateCommandList1(nodeMask, type, flags, riid, NULL);

  if(riid != __uuidof(ID3D12GraphicsCommandList) && riid != __uuidof(ID3D12CommandList) &&
     riid != __uuidof(ID3D12GraphicsCommandList1) && riid != __uuidof(ID3D12GraphicsCommandList2) &&
     riid != __uuidof(ID3D12GraphicsCommandList3) && riid != __uuidof(ID3D12GraphicsCommandList4))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice4->CreateCommandList1(
                          nodeMask, type, flags, __uuidof(ID3D12GraphicsCommandList), &realptr));

  ID3D12GraphicsCommandList *real = NULL;

  if(riid == __uuidof(ID3D12CommandList))
    real = (ID3D12GraphicsCommandList *)(ID3D12CommandList *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList))
    real = (ID3D12GraphicsCommandList *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList1))
    real = (ID3D12GraphicsCommandList1 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList2))
    real = (ID3D12GraphicsCommandList2 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList3))
    real = (ID3D12GraphicsCommandList3 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList4))
    real = (ID3D12GraphicsCommandList4 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12GraphicsCommandList *wrapped =
        new WrappedID3D12GraphicsCommandList(real, this, m_State);

    if(m_pAMDExtObject)
    {
      IAmdExtD3DCommandListMarker *markers = NULL;
      m_pAMDExtObject->CreateInterface(real, __uuidof(IAmdExtD3DCommandListMarker),
                                       (void **)&markers);
      wrapped->SetAMDMarkerInterface(markers);
    }

    if(IsCaptureMode(m_State))
    {
      // we just serialise out command allocator creation as a reset, since it's equivalent.
      wrapped->SetInitParams(riid, nodeMask, type);
      // no flags currently
      RDCASSERT(flags == D3D12_COMMAND_LIST_FLAG_NONE);

      // we don't call Reset() - it's not implicit in this version

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommandList1);
        Serialise_CreateCommandList1(ser, nodeMask, type, flags, riid, (void **)&wrapped);

        wrapped->GetCreationRecord()->AddChunk(scope.Get());
      }
    }

    // during replay, the caller is responsible for calling AddLiveResource as this function
    // can be called from ID3D12GraphicsCommandList::Reset serialising

    if(riid == __uuidof(ID3D12GraphicsCommandList))
      *ppCommandList = (ID3D12GraphicsCommandList *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList1))
      *ppCommandList = (ID3D12GraphicsCommandList1 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList2))
      *ppCommandList = (ID3D12GraphicsCommandList2 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList3))
      *ppCommandList = (ID3D12GraphicsCommandList3 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList4))
      *ppCommandList = (ID3D12GraphicsCommandList4 *)wrapped;
    else if(riid == __uuidof(ID3D12CommandList))
      *ppCommandList = (ID3D12CommandList *)wrapped;
    else
      RDCERR("Unexpected riid! %s", ToStr(riid).c_str());
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateProtectedResourceSession(
    _In_ const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc, _In_ REFIID riid,
    _COM_Outptr_ void **ppSession)
{
  if(ppSession == NULL)
    return m_pDevice4->CreateProtectedResourceSession(pDesc, riid, NULL);

  // this is the only UUID in the headers that we can support
  if(riid != __uuidof(ID3D12ProtectedResourceSession))
    return E_NOINTERFACE;

  ID3D12ProtectedResourceSession *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice4->CreateProtectedResourceSession(pDesc, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12ProtectedResourceSession *wrapped =
        new WrappedID3D12ProtectedResourceSession(real, this);

    *ppSession = (ID3D12ProtectedResourceSession *)wrapped;
  }

  return ret;
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
  SERIALISE_ELEMENT_LOCAL(desc, *pDesc).Named("pDesc"_lit);
  SERIALISE_ELEMENT(InitialResourceState);
  SERIALISE_ELEMENT_OPT(pOptimizedClearValue);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riidResource).Named("riidResource"_lit);
  SERIALISE_ELEMENT_LOCAL(pResource, ((WrappedID3D12Resource1 *)*ppvResource)->GetResourceID())
      .TypedAs("ID3D12Resource *"_lit);

  SERIALISE_ELEMENT_LOCAL(gpuAddress,
                          ((WrappedID3D12Resource1 *)*ppvResource)->GetGPUVirtualAddressIfBuffer())
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
        RDCLOG("Remapping committed resource %llu from upload to default for efficient replay",
               pResource);
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
    if(m_pDevice4)
      m_pDevice4->CreateCommittedResource1(&props, HeapFlags, &desc, InitialResourceState,
                                           pOptimizedClearValue, NULL, guid, (void **)&ret);
    else
      RDCERR("Replaying a without D3D12.4 available");

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      SetObjName(ret, StringFormat::Fmt("Committed Resource %s ID %llu",
                                        ToStr(desc.Dimension).c_str(), pResource));

      ret = new WrappedID3D12Resource1(ret, this);

      GetResourceManager()->AddLiveResource(pResource, ret);

      SubresourceStateVector &states = m_ResourceStates[GetResID(ret)];
      states.resize(GetNumSubresources(m_pDevice, &desc), InitialResourceState);

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

HRESULT WrappedID3D12Device::CreateCommittedResource1(
    const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
  if(ppvResource == NULL)
    return m_pDevice4->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc,
                                                InitialResourceState, pOptimizedClearValue,
                                                Unwrap(pProtectedSession), riidResource, NULL);

  if(riidResource != __uuidof(ID3D12Resource) && riidResource != __uuidof(ID3D12Resource1))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice4->CreateCommittedResource1(
                          pHeapProperties, HeapFlags, pDesc, InitialResourceState,
                          pOptimizedClearValue, Unwrap(pProtectedSession), riidResource, &realptr));

  ID3D12Resource *real = NULL;
  if(riidResource == __uuidof(ID3D12Resource))
    real = (ID3D12Resource *)realptr;
  else if(riidResource == __uuidof(ID3D12Resource1))
    real = (ID3D12Resource1 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Resource1 *wrapped = new WrappedID3D12Resource1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateCommittedResource1);
      Serialise_CreateCommittedResource1(ser, pHeapProperties, HeapFlags, pDesc,
                                         InitialResourceState, pOptimizedClearValue,
                                         pProtectedSession, riidResource, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Resource;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->m_MapsCount = GetNumSubresources(this, pDesc);
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

      states.resize(GetNumSubresources(m_pDevice, pDesc), InitialResourceState);
    }

    *ppvResource = (ID3D12Resource *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateHeap1(SerialiserType &ser, const D3D12_HEAP_DESC *pDesc,
                                                ID3D12ProtectedResourceSession *pProtectedSession,
                                                REFIID riid, void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit);
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pHeap, ((WrappedID3D12Heap1 *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12Heap *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    void *realptr = NULL;
    // don't replay with a protected session
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice4)
      m_pDevice4->CreateHeap1(&Descriptor, NULL, guid, &realptr);
    else
      RDCERR("Replaying a without D3D12.4 available");

    ID3D12Heap *ret = NULL;
    if(guid == __uuidof(ID3D12Heap))
      ret = (ID3D12Heap *)realptr;
    else if(guid == __uuidof(ID3D12Heap1))
      ret = (ID3D12Heap1 *)realptr;

    if(FAILED(hr))
    {
      RDCERR("Failed on resource serialise-creation, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12Heap1(ret, this);

      GetResourceManager()->AddLiveResource(pHeap, ret);
    }

    AddResource(pHeap, ResourceType::Memory, "Heap");
  }

  return true;
}

HRESULT WrappedID3D12Device::CreateHeap1(const D3D12_HEAP_DESC *pDesc,
                                         ID3D12ProtectedResourceSession *pProtectedSession,
                                         REFIID riid, void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice4->CreateHeap1(pDesc, Unwrap(pProtectedSession), riid, ppvHeap);

  if(riid != __uuidof(ID3D12Heap) && riid != __uuidof(ID3D12Heap1))
    return E_NOINTERFACE;

  void *realptr = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice4->CreateHeap1(pDesc, Unwrap(pProtectedSession), riid, (void **)&realptr));

  ID3D12Heap *real = NULL;

  if(riid == __uuidof(ID3D12Heap))
    real = (ID3D12Heap *)realptr;
  else if(riid == __uuidof(ID3D12Heap1))
    real = (ID3D12Heap1 *)realptr;

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Heap1 *wrapped = new WrappedID3D12Heap1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeap1);
      Serialise_CreateHeap1(ser, pDesc, pProtectedSession, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Heap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12Heap *)wrapped;
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateReservedResource1(
    _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid,
    _COM_Outptr_opt_ void **ppvResource)
{
  RDCERR("Tiled Resources are not currently implemented on D3D12");
  return E_NOINTERFACE;
}

D3D12_RESOURCE_ALLOCATION_INFO WrappedID3D12Device::GetResourceAllocationInfo1(
    UINT visibleMask, UINT numResourceDescs,
    _In_reads_(numResourceDescs) const D3D12_RESOURCE_DESC *pResourceDescs,
    _Out_writes_opt_(numResourceDescs) D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1)
{
  return m_pDevice4->GetResourceAllocationInfo1(visibleMask, numResourceDescs, pResourceDescs,
                                                pResourceAllocationInfo1);
}

ID3D12Fence *WrappedID3D12Device::CreateProtectedSessionFence(ID3D12Fence *real)
{
  // we basically treat this kind of like CreateFence and serialise it as such, and guess at the
  // parameters to CreateFence.
  WrappedID3D12Fence1 *wrapped = new WrappedID3D12Fence1(real, this);

  if(IsCaptureMode(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateFence);
    Serialise_CreateFence(ser, 0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&wrapped);

    D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
    record->type = Resource_Resource;
    record->Length = 0;
    wrapped->SetResourceRecord(record);

    record->AddChunk(scope.Get());
  }
  else
  {
    RDCERR("Shouldn't be calling CreateProtectedSessionFence during replay!");
  }

  return wrapped;
}

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommittedResource1,
                                const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc,
                                D3D12_RESOURCE_STATES InitialResourceState,
                                const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                ID3D12ProtectedResourceSession *pProtectedSession,
                                REFIID riidResource, void **ppvResource);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateHeap1, const D3D12_HEAP_DESC *pDesc,
                                ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid,
                                void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandList1, UINT nodeMask,
                                D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags,
                                REFIID riid, void **ppCommandList);