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
  SERIALISE_ELEMENT(type).Important();
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
    nodeMask = 0;

    ID3D12GraphicsCommandList *list = NULL;
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice4)
    {
      hr = CreateCommandList1(nodeMask, type, flags, __uuidof(ID3D12GraphicsCommandList),
                              (void **)&list);
    }
    else
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Capture requires ID3D12Device4 which isn't available");
      return false;
    }

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating command list, HRESULT: %s", ToStr(hr).c_str());
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
     riid != __uuidof(ID3D12GraphicsCommandList3) && riid != __uuidof(ID3D12GraphicsCommandList4) &&
     riid != __uuidof(ID3D12GraphicsCommandList5) && riid != __uuidof(ID3D12GraphicsCommandList6) &&
     riid != __uuidof(ID3D12GraphicsCommandList7) && riid != __uuidof(ID3D12GraphicsCommandList8) &&
     riid != __uuidof(ID3D12GraphicsCommandList9))
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
  else if(riid == __uuidof(ID3D12GraphicsCommandList5))
    real = (ID3D12GraphicsCommandList5 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList6))
    real = (ID3D12GraphicsCommandList6 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList7))
    real = (ID3D12GraphicsCommandList7 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList8))
    real = (ID3D12GraphicsCommandList8 *)realptr;
  else if(riid == __uuidof(ID3D12GraphicsCommandList9))
    real = (ID3D12GraphicsCommandList9 *)realptr;

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
    else if(riid == __uuidof(ID3D12GraphicsCommandList5))
      *ppCommandList = (ID3D12GraphicsCommandList5 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList6))
      *ppCommandList = (ID3D12GraphicsCommandList6 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList7))
      *ppCommandList = (ID3D12GraphicsCommandList7 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList8))
      *ppCommandList = (ID3D12GraphicsCommandList8 *)wrapped;
    else if(riid == __uuidof(ID3D12GraphicsCommandList9))
      *ppCommandList = (ID3D12GraphicsCommandList9 *)wrapped;
    else if(riid == __uuidof(ID3D12CommandList))
      *ppCommandList = (ID3D12CommandList *)wrapped;
    else
      RDCERR("Unexpected riid! %s", ToStr(riid).c_str());
  }
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
}

HRESULT WrappedID3D12Device::CreateProtectedResourceSession(
    _In_ const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc, _In_ REFIID riid,
    _COM_Outptr_ void **ppSession)
{
  if(ppSession == NULL)
    return m_pDevice4->CreateProtectedResourceSession(pDesc, riid, NULL);

  if(riid != __uuidof(ID3D12ProtectedResourceSession) &&
     riid != __uuidof(ID3D12ProtectedResourceSession1) && riid != __uuidof(ID3D12ProtectedSession))
    return E_NOINTERFACE;

  ID3D12ProtectedResourceSession *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice4->CreateProtectedResourceSession(
                          pDesc, __uuidof(ID3D12ProtectedResourceSession), (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12ProtectedResourceSession *wrapped =
        new WrappedID3D12ProtectedResourceSession(real, this);

    if(riid == __uuidof(ID3D12ProtectedResourceSession))
      *ppSession = (ID3D12ProtectedResourceSession *)wrapped;
    else if(riid == __uuidof(ID3D12ProtectedResourceSession1))
      *ppSession = (ID3D12ProtectedResourceSession1 *)wrapped;
    else if(riid == __uuidof(ID3D12ProtectedSession))
      *ppSession = (ID3D12ProtectedSession *)wrapped;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedID3D12Device::Serialise_CreateHeap1(SerialiserType &ser, const D3D12_HEAP_DESC *pDesc,
                                                ID3D12ProtectedResourceSession *pProtectedSession,
                                                REFIID riid, void **ppvHeap)
{
  SERIALISE_ELEMENT_LOCAL(Descriptor, *pDesc).Named("pDesc"_lit).Important();
  // placeholder for future use if we properly capture & replay protected sessions
  SERIALISE_ELEMENT_LOCAL(ProtectedSession, ResourceId()).Named("pProtectedSession"_lit);
  SERIALISE_ELEMENT_LOCAL(guid, riid).Named("riid"_lit);
  SERIALISE_ELEMENT_LOCAL(pHeap, ((WrappedID3D12Heap *)*ppvHeap)->GetResourceID())
      .TypedAs("ID3D12Heap *"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    void *realptr = NULL;

    // don't create resources non-resident
    Descriptor.Flags &= ~D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;

    // don't create displayable heaps (?!)
    Descriptor.Flags &= ~D3D12_HEAP_FLAG_ALLOW_DISPLAY;

    // don't replay with a protected session
    HRESULT hr = E_NOINTERFACE;
    if(m_pDevice4)
      hr = m_pDevice4->CreateHeap1(&Descriptor, NULL, guid, &realptr);
    else
      RDCERR("Replaying a without D3D12.4 available");

    ID3D12Heap *ret = NULL;
    if(guid == __uuidof(ID3D12Heap))
      ret = (ID3D12Heap *)realptr;
    else if(guid == __uuidof(ID3D12Heap1))
      ret = (ID3D12Heap1 *)realptr;

    if(FAILED(hr))
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating heap, HRESULT: %s", ToStr(hr).c_str());
      return false;
    }
    else
    {
      ret = new WrappedID3D12Heap(ret, this);

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
    WrappedID3D12Heap *wrapped = new WrappedID3D12Heap(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeap1);
      Serialise_CreateHeap1(ser, pDesc, pProtectedSession, riid, (void **)&wrapped);

      if(pDesc->Flags & D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT)
        wrapped->Evict();

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
  else
  {
    CheckHRESULT(ret);
  }

  return ret;
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
  WrappedID3D12Fence *wrapped = NULL;

  {
    SCOPED_LOCK(m_WrapDeduplicateLock);

    // if we already have this fence wrapped, return the existing wrapper
    if(GetResourceManager()->HasWrapper(real))
    {
      return (ID3D12Fence *)GetResourceManager()->GetWrapper((ID3D12DeviceChild *)real);
    }

    // we basically treat this kind of like CreateFence and serialise it as such, and guess at the
    // parameters to CreateFence.
    wrapped = new WrappedID3D12Fence(real, this);
  }

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

INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateHeap1, const D3D12_HEAP_DESC *pDesc,
                                ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid,
                                void **ppvHeap);
INSTANTIATE_FUNCTION_SERIALISED(void, WrappedID3D12Device, CreateCommandList1, UINT nodeMask,
                                D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags,
                                REFIID riid, void **ppCommandList);
