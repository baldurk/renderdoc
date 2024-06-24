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

#include <algorithm>
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/official/DXExt/AmdExtD3DCommandListMarkerApi.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"

WRAPPED_POOL_INST(WrappedID3D12CommandQueue);
WRAPPED_POOL_INST(WrappedID3D12GraphicsCommandList);

template <>
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList1 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList2 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList3 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList4 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList5 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList6 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList7 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList8 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList9 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

ID3D12GraphicsCommandList1 *Unwrap1(ID3D12GraphicsCommandList1 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal1();
}

ID3D12GraphicsCommandList2 *Unwrap2(ID3D12GraphicsCommandList2 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal2();
}

ID3D12GraphicsCommandList3 *Unwrap3(ID3D12GraphicsCommandList3 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal3();
}

ID3D12GraphicsCommandList4 *Unwrap4(ID3D12GraphicsCommandList4 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal4();
}

ID3D12GraphicsCommandList5 *Unwrap5(ID3D12GraphicsCommandList5 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal5();
}

ID3D12GraphicsCommandList6 *Unwrap6(ID3D12GraphicsCommandList6 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal6();
}

ID3D12GraphicsCommandList7 *Unwrap7(ID3D12GraphicsCommandList7 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal7();
}

ID3D12GraphicsCommandList8 *Unwrap8(ID3D12GraphicsCommandList8 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal8();
}

ID3D12GraphicsCommandList9 *Unwrap9(ID3D12GraphicsCommandList9 *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal9();
}

template <>
ID3D12CommandList *Unwrap(ID3D12CommandList *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetReal();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList1 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList2 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList3 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList4 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList5 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList6 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList7 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList8 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12GraphicsCommandList9 *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
ResourceId GetResID(ID3D12CommandList *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceID();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12GraphicsCommandList *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceRecord();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12CommandList *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12GraphicsCommandList *)obj)->GetResourceRecord();
}

template <>
ID3D12CommandQueue *Unwrap(ID3D12CommandQueue *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12CommandQueue *)obj)->GetReal();
}

template <>
ResourceId GetResID(ID3D12CommandQueue *obj)
{
  if(obj == NULL)
    return ResourceId();

  return ((WrappedID3D12CommandQueue *)obj)->GetResourceID();
}

template <>
D3D12ResourceRecord *GetRecord(ID3D12CommandQueue *obj)
{
  if(obj == NULL)
    return NULL;

  return ((WrappedID3D12CommandQueue *)obj)->GetResourceRecord();
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList1 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList2 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList3 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList4 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList5 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList6 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList7 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList8 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList9 *obj)
{
  return ((WrappedID3D12GraphicsCommandList *)obj);
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandQueue::AddRef()
{
  if(m_pQueue)
    m_pQueue->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandQueue::Release()
{
  if(m_pQueue)
    m_pQueue->Release();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandList::AddRef()
{
  if(m_pList)
    m_pList->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandList::Release()
{
  if(m_pList)
    m_pList->Release();
  return 1;
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CompatibilityQueue::QueryInterface(REFIID riid,
                                                                          void **ppvObject)
{
  return m_pQueue.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedID3D12CompatibilityQueue::AddRef()
{
  return m_pQueue.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedID3D12CompatibilityQueue::Release()
{
  return m_pQueue.Release();
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CompatibilityQueue::AcquireKeyedMutex(
    _In_ ID3D12Object *pHeapOrResourceWithKeyedMutex, UINT64 Key, DWORD dwTimeout,
    _Reserved_ void *pReserved, _In_range_(0, 0) UINT Reserved)
{
  return m_pReal->AcquireKeyedMutex(Unwrap(pHeapOrResourceWithKeyedMutex), Key, dwTimeout,
                                    pReserved, Reserved);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CompatibilityQueue::ReleaseKeyedMutex(
    _In_ ID3D12Object *pHeapOrResourceWithKeyedMutex, UINT64 Key, _Reserved_ void *pReserved,
    _In_range_(0, 0) UINT Reserved)
{
  return m_pReal->ReleaseKeyedMutex(Unwrap(pHeapOrResourceWithKeyedMutex), Key, pReserved, Reserved);
}

HRESULT STDMETHODCALLTYPE WrappedDownlevelQueue::QueryInterface(REFIID riid, void **ppvObject)
{
  return m_pQueue.QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE WrappedDownlevelQueue::AddRef()
{
  return m_pQueue.AddRef();
}

ULONG STDMETHODCALLTYPE WrappedDownlevelQueue::Release()
{
  return m_pQueue.Release();
}

HRESULT STDMETHODCALLTYPE WrappedDownlevelQueue::Present(ID3D12GraphicsCommandList *pOpenCommandList,
                                                         ID3D12Resource *pSourceTex2D, HWND hWindow,
                                                         D3D12_DOWNLEVEL_PRESENT_FLAGS Flags)
{
  return m_pQueue.Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}

WrappedID3D12CommandQueue::WrappedID3D12CommandQueue(ID3D12CommandQueue *real,
                                                     WrappedID3D12Device *device, CaptureState &state)
    : RefCounter12(real),
      m_pDevice(device),
      m_State(state),
      m_WrappedDownlevel(*this),
      m_WrappedCompat(*this)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(WrappedID3D12CommandQueue));

  m_WrappedDebug.m_pQueue = this;
  m_pDownlevel = NULL;
  if(m_pReal)
  {
    m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandQueue), (void **)&m_WrappedDebug.m_pReal);
    m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandQueue1), (void **)&m_WrappedDebug.m_pReal1);
    m_pReal->QueryInterface(__uuidof(ID3D12CommandQueueDownlevel), (void **)&m_pDownlevel);
    m_pReal->QueryInterface(__uuidof(ID3D12CompatibilityQueue), (void **)&m_WrappedCompat.m_pReal);
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_ReplayList = new WrappedID3D12GraphicsCommandList(NULL, m_pDevice, state);

    m_ReplayList->SetCommandData(&m_Cmd);
  }

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_QueueRecord = NULL;
  m_CreationRecord = NULL;

  m_Cmd.m_pDevice = m_pDevice;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_QueueRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_QueueRecord->type = Resource_CommandQueue;
    m_QueueRecord->DataInSerialiser = false;
    m_QueueRecord->InternalResource = true;
    m_QueueRecord->Length = 0;

    // a bit of a hack, we make a parallel resource record with the same lifetime as the command
    // queue. It will hold onto our create chunk and not get thrown away as we clear and re-fill
    // submissions into the queue record itself. We'll pull it into the capture by marking the
    // queues as referenced.
    m_CreationRecord =
        m_pDevice->GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_CreationRecord->type = Resource_CommandQueue;
    m_CreationRecord->InternalResource = true;
  }

  m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);

  m_pDevice->SoftRef();
}

WrappedID3D12CommandQueue::~WrappedID3D12CommandQueue()
{
  SAFE_DELETE(m_FrameReader);

  SAFE_RELEASE(m_RayFence);

  if(m_CreationRecord)
    m_CreationRecord->Delete(m_pDevice->GetResourceManager());

  if(m_QueueRecord)
    m_QueueRecord->Delete(m_pDevice->GetResourceManager());
  m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());
  m_pDevice->RemoveQueue(this);

  SAFE_RELEASE(m_pDownlevel);

  SAFE_RELEASE(m_WrappedCompat.m_pReal);
  SAFE_RELEASE(m_WrappedDebug.m_pReal);
  SAFE_RELEASE(m_WrappedDebug.m_pReal1);
  SAFE_RELEASE(m_pReal);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12CommandQueue::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12CommandQueue *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12CommandQueue))
  {
    *ppvObject = (ID3D12CommandQueue *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DebugCommandQueue))
  {
    if(m_WrappedDebug.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12DebugCommandQueue *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DebugCommandQueue1))
  {
    if(m_WrappedDebug.m_pReal1)
    {
      AddRef();
      *ppvObject = (ID3D12DebugCommandQueue1 *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12CompatibilityQueue))
  {
    if(m_WrappedCompat.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12CompatibilityQueue *)&m_WrappedCompat;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12Pageable))
  {
    *ppvObject = (ID3D12Pageable *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DeviceChild))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Object))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12CommandQueueDownlevel))
  {
    if(m_pDownlevel)
    {
      *ppvObject = &m_WrappedDownlevel;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }

  return RefCounter12::QueryInterface("ID3D12CommandQueue", riid, ppvObject);
}

void WrappedID3D12CommandQueue::CheckAndFreeRayDispatches()
{
  UINT64 signalled = 0;
  if(m_RayFence)
    signalled = m_RayFence->GetCompletedValue();

  for(PatchedRayDispatch::Resources &ray : m_RayDispatchesPending)
  {
    if(signalled >= ray.fenceValue)
    {
      SAFE_RELEASE(ray.patchScratchBuffer);
      SAFE_RELEASE(ray.lookupBuffer);
      SAFE_RELEASE(ray.argumentBuffer);
    }
  }

  m_RayDispatchesPending.removeIf(
      [](const PatchedRayDispatch::Resources &ray) { return ray.lookupBuffer == NULL; });
}

void WrappedID3D12CommandQueue::ClearAfterCapture()
{
  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdListRecords.size(); i++)
    m_CmdListRecords[i]->Delete(GetResourceManager());

  for(size_t i = 0; i < m_CmdListAllocators.size(); i++)
    m_CmdListAllocators[i]->Delete(GetResourceManager());

  m_CmdListRecords.clear();
  m_CmdListAllocators.clear();

  m_QueueRecord->DeleteChunks();
}

WriteSerialiser &WrappedID3D12CommandQueue::GetThreadSerialiser()
{
  return m_pDevice->GetThreadSerialiser();
}

rdcstr WrappedID3D12CommandQueue::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D12Chunk)idx);
}

const APIEvent &WrappedID3D12CommandQueue::GetEvent(uint32_t eventId)
{
  // start at where the requested eventId would be
  size_t idx = eventId;

  // find the next valid event (some may be skipped)
  while(idx < m_Cmd.m_Events.size() - 1 && m_Cmd.m_Events[idx].eventId == 0)
    idx++;

  return m_Cmd.m_Events[RDCMIN(idx, m_Cmd.m_Events.size() - 1)];
}

bool WrappedID3D12CommandQueue::ProcessChunk(ReadSerialiser &ser, D3D12Chunk chunk)
{
  m_Cmd.m_AddedAction = false;

  bool ret = false;

  switch(chunk)
  {
    case D3D12Chunk::Device_CreateConstantBufferView:
    case D3D12Chunk::Device_CreateShaderResourceView:
    case D3D12Chunk::Device_CreateUnorderedAccessView:
    case D3D12Chunk::Device_CreateRenderTargetView:
    case D3D12Chunk::Device_CreateDepthStencilView:
    case D3D12Chunk::Device_CreateSampler:
    case D3D12Chunk::Device_CreateSampler2:
      ret = m_pDevice->Serialise_DynamicDescriptorWrite(ser, NULL);
      break;
    case D3D12Chunk::Device_CopyDescriptors:
    case D3D12Chunk::Device_CopyDescriptorsSimple:
      ret = m_pDevice->Serialise_DynamicDescriptorCopies(ser, rdcarray<DynamicDescriptorCopy>());
      break;

    case D3D12Chunk::Queue_ExecuteCommandLists:
      ret = Serialise_ExecuteCommandLists(ser, 0, NULL);
      break;
    case D3D12Chunk::Queue_Signal: ret = Serialise_Signal(ser, NULL, 0); break;
    case D3D12Chunk::Queue_Wait: ret = Serialise_Wait(ser, NULL, 0); break;
    case D3D12Chunk::Queue_UpdateTileMappings:
      ret = Serialise_UpdateTileMappings(ser, NULL, 0, NULL, NULL, NULL, 0, NULL, NULL, NULL,
                                         D3D12_TILE_MAPPING_FLAGS(0));
      break;
    case D3D12Chunk::Queue_CopyTileMappings:
      ret =
          Serialise_CopyTileMappings(ser, NULL, NULL, NULL, NULL, NULL, D3D12_TILE_MAPPING_FLAGS(0));
      break;
    case D3D12Chunk::Queue_BeginEvent: ret = Serialise_BeginEvent(ser, 0, NULL, 0); break;
    case D3D12Chunk::Queue_SetMarker: ret = Serialise_SetMarker(ser, 0, NULL, 0); break;
    case D3D12Chunk::Queue_EndEvent: ret = Serialise_EndEvent(ser); break;

    case D3D12Chunk::List_Close: ret = m_ReplayList->Serialise_Close(ser); break;
    case D3D12Chunk::List_Reset: ret = m_ReplayList->Serialise_Reset(ser, NULL, NULL); break;
    case D3D12Chunk::List_ResourceBarrier:
      ret = m_ReplayList->Serialise_ResourceBarrier(ser, 0, NULL);
      break;
    case D3D12Chunk::List_BeginQuery:
      ret = m_ReplayList->Serialise_BeginQuery(ser, NULL, D3D12_QUERY_TYPE(0), 0);
      break;
    case D3D12Chunk::List_EndQuery:
      ret = m_ReplayList->Serialise_EndQuery(ser, NULL, D3D12_QUERY_TYPE(0), 0);
      break;
    case D3D12Chunk::List_ResolveQueryData:
      ret = m_ReplayList->Serialise_ResolveQueryData(ser, NULL, D3D12_QUERY_TYPE(0), 0, 0, NULL, 0);
      break;
    case D3D12Chunk::List_SetPredication:
      ret = m_ReplayList->Serialise_SetPredication(ser, NULL, 0, D3D12_PREDICATION_OP(0));
      break;
    case D3D12Chunk::List_DrawIndexedInstanced:
      ret = m_ReplayList->Serialise_DrawIndexedInstanced(ser, 0, 0, 0, 0, 0);
      break;
    case D3D12Chunk::List_DrawInstanced:
      ret = m_ReplayList->Serialise_DrawInstanced(ser, 0, 0, 0, 0);
      break;
    case D3D12Chunk::List_Dispatch: ret = m_ReplayList->Serialise_Dispatch(ser, 0, 0, 0); break;
    case D3D12Chunk::List_ExecuteIndirect:
      ret = m_ReplayList->Serialise_ExecuteIndirect(ser, NULL, 0, NULL, 0, NULL, 0);
      break;
    case D3D12Chunk::List_ExecuteBundle:
      ret = m_ReplayList->Serialise_ExecuteBundle(ser, NULL);
      break;
    case D3D12Chunk::List_CopyBufferRegion:
      ret = m_ReplayList->Serialise_CopyBufferRegion(ser, NULL, 0, NULL, 0, 0);
      break;
    case D3D12Chunk::List_CopyTextureRegion:
      ret = m_ReplayList->Serialise_CopyTextureRegion(ser, NULL, 0, 0, 0, NULL, NULL);
      break;
    case D3D12Chunk::List_CopyResource:
      ret = m_ReplayList->Serialise_CopyResource(ser, NULL, NULL);
      break;
    case D3D12Chunk::List_ResolveSubresource:
      ret = m_ReplayList->Serialise_ResolveSubresource(ser, NULL, 0, NULL, 0, DXGI_FORMAT_UNKNOWN);
      break;
    case D3D12Chunk::List_ClearRenderTargetView:
      ret = m_ReplayList->Serialise_ClearRenderTargetView(ser, D3D12_CPU_DESCRIPTOR_HANDLE(),
                                                          (FLOAT *)NULL, 0, NULL);
      break;
    case D3D12Chunk::List_ClearDepthStencilView:
      ret = m_ReplayList->Serialise_ClearDepthStencilView(ser, D3D12_CPU_DESCRIPTOR_HANDLE(),
                                                          D3D12_CLEAR_FLAGS(0), 0.0f, 0, 0, NULL);
      break;
    case D3D12Chunk::List_ClearUnorderedAccessViewUint:
      ret = m_ReplayList->Serialise_ClearUnorderedAccessViewUint(
          ser, D3D12_GPU_DESCRIPTOR_HANDLE(), D3D12_CPU_DESCRIPTOR_HANDLE(), NULL, NULL, 0, NULL);
      break;
    case D3D12Chunk::List_ClearUnorderedAccessViewFloat:
      ret = m_ReplayList->Serialise_ClearUnorderedAccessViewFloat(
          ser, D3D12_GPU_DESCRIPTOR_HANDLE(), D3D12_CPU_DESCRIPTOR_HANDLE(), NULL, NULL, 0, NULL);
      break;
    case D3D12Chunk::List_DiscardResource:
      ret = m_ReplayList->Serialise_DiscardResource(ser, NULL, NULL);
      break;
    case D3D12Chunk::List_IASetPrimitiveTopology:
      ret = m_ReplayList->Serialise_IASetPrimitiveTopology(ser, D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
      break;
    case D3D12Chunk::List_IASetIndexBuffer:
      ret = m_ReplayList->Serialise_IASetIndexBuffer(ser, NULL);
      break;
    case D3D12Chunk::List_IASetVertexBuffers:
      ret = m_ReplayList->Serialise_IASetVertexBuffers(ser, 0, 0, NULL);
      break;
    case D3D12Chunk::List_SOSetTargets:
      ret = m_ReplayList->Serialise_SOSetTargets(ser, 0, 0, NULL);
      break;
    case D3D12Chunk::List_RSSetViewports:
      ret = m_ReplayList->Serialise_RSSetViewports(ser, 0, NULL);
      break;
    case D3D12Chunk::List_RSSetScissorRects:
      ret = m_ReplayList->Serialise_RSSetScissorRects(ser, 0, NULL);
      break;
    case D3D12Chunk::List_SetPipelineState:
      ret = m_ReplayList->Serialise_SetPipelineState(ser, NULL);
      break;
    case D3D12Chunk::List_SetDescriptorHeaps:
      ret = m_ReplayList->Serialise_SetDescriptorHeaps(ser, 0, NULL);
      break;
    case D3D12Chunk::List_OMSetRenderTargets:
      ret = m_ReplayList->Serialise_OMSetRenderTargets(ser, 0, NULL, FALSE, NULL);
      break;
    case D3D12Chunk::List_OMSetStencilRef:
      ret = m_ReplayList->Serialise_OMSetStencilRef(ser, 0);
      break;
    case D3D12Chunk::List_OMSetBlendFactor:
      ret = m_ReplayList->Serialise_OMSetBlendFactor(ser, NULL);
      break;
    case D3D12Chunk::List_SetGraphicsRootDescriptorTable:
      ret = m_ReplayList->Serialise_SetGraphicsRootDescriptorTable(ser, 0,
                                                                   D3D12_GPU_DESCRIPTOR_HANDLE());
      break;
    case D3D12Chunk::List_SetGraphicsRootSignature:
      ret = m_ReplayList->Serialise_SetGraphicsRootSignature(ser, NULL);
      break;
    case D3D12Chunk::List_SetGraphicsRoot32BitConstant:
      ret = m_ReplayList->Serialise_SetGraphicsRoot32BitConstant(ser, 0, 0, 0);
      break;
    case D3D12Chunk::List_SetGraphicsRoot32BitConstants:
      ret = m_ReplayList->Serialise_SetGraphicsRoot32BitConstants(ser, 0, 0, NULL, 0);
      break;
    case D3D12Chunk::List_SetGraphicsRootConstantBufferView:
      ret = m_ReplayList->Serialise_SetGraphicsRootConstantBufferView(ser, 0,
                                                                      D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case D3D12Chunk::List_SetGraphicsRootShaderResourceView:
      ret = m_ReplayList->Serialise_SetGraphicsRootShaderResourceView(ser, 0,
                                                                      D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case D3D12Chunk::List_SetGraphicsRootUnorderedAccessView:
      ret = m_ReplayList->Serialise_SetGraphicsRootUnorderedAccessView(ser, 0,
                                                                       D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case D3D12Chunk::List_SetComputeRootDescriptorTable:
      ret = m_ReplayList->Serialise_SetComputeRootDescriptorTable(ser, 0,
                                                                  D3D12_GPU_DESCRIPTOR_HANDLE());
      break;
    case D3D12Chunk::List_SetComputeRootSignature:
      ret = m_ReplayList->Serialise_SetComputeRootSignature(ser, NULL);
      break;
    case D3D12Chunk::List_SetComputeRoot32BitConstant:
      ret = m_ReplayList->Serialise_SetComputeRoot32BitConstant(ser, 0, 0, 0);
      break;
    case D3D12Chunk::List_SetComputeRoot32BitConstants:
      ret = m_ReplayList->Serialise_SetComputeRoot32BitConstants(ser, 0, 0, NULL, 0);
      break;
    case D3D12Chunk::List_SetComputeRootConstantBufferView:
      ret = m_ReplayList->Serialise_SetComputeRootConstantBufferView(ser, 0,
                                                                     D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case D3D12Chunk::List_SetComputeRootShaderResourceView:
      ret = m_ReplayList->Serialise_SetComputeRootShaderResourceView(ser, 0,
                                                                     D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case D3D12Chunk::List_SetComputeRootUnorderedAccessView:
      ret = m_ReplayList->Serialise_SetComputeRootUnorderedAccessView(ser, 0,
                                                                      D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case D3D12Chunk::List_CopyTiles:
      ret = m_ReplayList->Serialise_CopyTiles(ser, NULL, NULL, NULL, NULL, 0,
                                              D3D12_TILE_COPY_FLAGS(0));
      break;
    case D3D12Chunk::List_AtomicCopyBufferUINT:
      ret = m_ReplayList->Serialise_AtomicCopyBufferUINT(ser, NULL, 0, NULL, 0, 0, NULL, NULL);
      break;
    case D3D12Chunk::List_AtomicCopyBufferUINT64:
      ret = m_ReplayList->Serialise_AtomicCopyBufferUINT64(ser, NULL, 0, NULL, 0, 0, NULL, NULL);
      break;
    case D3D12Chunk::List_OMSetDepthBounds:
      ret = m_ReplayList->Serialise_OMSetDepthBounds(ser, 0.0f, 0.0f);
      break;
    case D3D12Chunk::List_ResolveSubresourceRegion:
      ret = m_ReplayList->Serialise_ResolveSubresourceRegion(
          ser, NULL, 0, 0, 0, NULL, 0, NULL, DXGI_FORMAT_UNKNOWN, D3D12_RESOLVE_MODE_DECOMPRESS);
      break;
    case D3D12Chunk::List_SetSamplePositions:
      ret = m_ReplayList->Serialise_SetSamplePositions(ser, 0, NULL, NULL);
      break;
    case D3D12Chunk::List_SetViewInstanceMask:
      ret = m_ReplayList->Serialise_SetViewInstanceMask(ser, 0);
      break;
    case D3D12Chunk::List_WriteBufferImmediate:
      ret = m_ReplayList->Serialise_WriteBufferImmediate(ser, 0, NULL, NULL);
      break;
    case D3D12Chunk::List_BeginRenderPass:
      ret = m_ReplayList->Serialise_BeginRenderPass(ser, 0, NULL, NULL, D3D12_RENDER_PASS_FLAG_NONE);
      break;
    case D3D12Chunk::List_EndRenderPass: ret = m_ReplayList->Serialise_EndRenderPass(ser); break;
    case D3D12Chunk::List_RSSetShadingRate:
      ret = m_ReplayList->Serialise_RSSetShadingRate(ser, D3D12_SHADING_RATE_1X1, NULL);
      break;
    case D3D12Chunk::List_RSSetShadingRateImage:
      ret = m_ReplayList->Serialise_RSSetShadingRateImage(ser, NULL);
      break;
    case D3D12Chunk::List_OMSetFrontAndBackStencilRef:
      ret = m_ReplayList->Serialise_OMSetFrontAndBackStencilRef(ser, 0, 0);
      break;
    case D3D12Chunk::List_RSSetDepthBias:
      ret = m_ReplayList->Serialise_RSSetDepthBias(ser, 0.0f, 0.0f, 0.0f);
      break;
    case D3D12Chunk::List_IASetIndexBufferStripCutValue:
      ret = m_ReplayList->Serialise_IASetIndexBufferStripCutValue(
          ser, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED);
      break;
    case D3D12Chunk::List_Barrier: ret = m_ReplayList->Serialise_Barrier(ser, 0, NULL); break;
    case D3D12Chunk::List_DispatchMesh:
      ret = m_ReplayList->Serialise_DispatchMesh(ser, 0, 0, 0);
      break;

    case D3D12Chunk::PushMarker: ret = m_ReplayList->Serialise_BeginEvent(ser, 0, NULL, 0); break;
    case D3D12Chunk::PopMarker: ret = m_ReplayList->Serialise_EndEvent(ser); break;
    case D3D12Chunk::SetMarker: ret = m_ReplayList->Serialise_SetMarker(ser, 0, NULL, 0); break;

    case D3D12Chunk::CoherentMapWrite:
    case D3D12Chunk::Resource_Unmap:
      ret = m_pDevice->Serialise_MapDataWrite(ser, NULL, 0, NULL, D3D12_RANGE(), false);
      break;
    case D3D12Chunk::Resource_WriteToSubresource:
      ret = m_pDevice->Serialise_WriteToSubresource(ser, NULL, 0, NULL, NULL, 0, 0);
      break;
    case D3D12Chunk::List_IndirectSubCommand:
      // this is a fake chunk generated at runtime as part of indirect draws.
      // Just in case it gets exported and imported, completely ignore it.
      return true;

    case D3D12Chunk::Swapchain_Present: ret = m_pDevice->Serialise_Present(ser, NULL, 0, 0); break;

    case D3D12Chunk::List_ClearState: ret = m_ReplayList->Serialise_ClearState(ser, NULL); break;

    case D3D12Chunk::List_BuildRaytracingAccelerationStructure:
      ret = m_ReplayList->Serialise_BuildRaytracingAccelerationStructure(ser, NULL, 0, NULL);
      break;
    case D3D12Chunk::List_CopyRaytracingAccelerationStructure:
      ret = m_ReplayList->Serialise_CopyRaytracingAccelerationStructure(
          ser, D3D12_GPU_VIRTUAL_ADDRESS(), D3D12_GPU_VIRTUAL_ADDRESS(),
          D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_CLONE);
      break;
    case D3D12Chunk::List_EmitRaytracingAccelerationStructurePostbuildInfo:
      ret = m_ReplayList->Serialise_EmitRaytracingAccelerationStructurePostbuildInfo(ser, NULL, 0,
                                                                                     NULL);
      break;
    case D3D12Chunk::List_DispatchRays:
      ret = m_ReplayList->Serialise_DispatchRays(ser, NULL);
      break;
    case D3D12Chunk::List_SetPipelineState1:
      ret = m_ReplayList->Serialise_SetPipelineState1(ser, NULL);
      break;

    // in order to get a warning if we miss a case, we explicitly handle the device creation chunks
    // here. If we actually encounter one it's an error (we shouldn't see these inside the captured
    // frame itself)
    case D3D12Chunk::Device_CreateCommandQueue:
    case D3D12Chunk::Device_CreateCommandAllocator:
    case D3D12Chunk::Device_CreateCommandList:
    case D3D12Chunk::Device_CreateGraphicsPipeline:
    case D3D12Chunk::Device_CreateComputePipeline:
    case D3D12Chunk::Device_CreateDescriptorHeap:
    case D3D12Chunk::Device_CreateRootSignature:
    case D3D12Chunk::Device_CreateCommandSignature:
    case D3D12Chunk::Device_CreateHeap:
    case D3D12Chunk::Device_CreateCommittedResource:
    case D3D12Chunk::Device_CreatePlacedResource:
    case D3D12Chunk::Device_CreateReservedResource:
    case D3D12Chunk::Device_CreateQueryHeap:
    case D3D12Chunk::Device_CreateFence:
    case D3D12Chunk::SetName:
    case D3D12Chunk::SetShaderDebugPath:
    case D3D12Chunk::CreateSwapBuffer:
    case D3D12Chunk::Device_CreatePipelineState:
    case D3D12Chunk::Device_CreateHeapFromAddress:
    case D3D12Chunk::Device_CreateHeapFromFileMapping:
    case D3D12Chunk::Device_OpenSharedHandle:
    case D3D12Chunk::Device_CreateCommandList1:
    case D3D12Chunk::Device_CreateCommittedResource1:
    case D3D12Chunk::Device_CreateHeap1:
    case D3D12Chunk::Device_ExternalDXGIResource:
    case D3D12Chunk::CompatDevice_CreateSharedResource:
    case D3D12Chunk::CompatDevice_CreateSharedHeap:
    case D3D12Chunk::SetShaderExtUAV:
    case D3D12Chunk::Device_CreateCommittedResource2:
    case D3D12Chunk::Device_CreatePlacedResource1:
    case D3D12Chunk::Device_CreateCommandQueue1:
    case D3D12Chunk::Device_CreateCommittedResource3:
    case D3D12Chunk::Device_CreatePlacedResource2:
    case D3D12Chunk::Device_CreateReservedResource1:
    case D3D12Chunk::Device_CreateReservedResource2:
    case D3D12Chunk::Device_CreateStateObject:
    case D3D12Chunk::Device_AddToStateObject:
    case D3D12Chunk::CreateAS:
    case D3D12Chunk::StateObject_SetPipelineStackSize:
      RDCERR("Unexpected chunk while processing frame: %s", ToStr(chunk).c_str());
      return false;

    // no explicit default so that we have compiler warnings if a chunk isn't explicitly handled.
    case D3D12Chunk::Max: break;
  }

  {
    SystemChunk system = (SystemChunk)chunk;

    if(system == SystemChunk::CaptureEnd)
    {
      SERIALISE_ELEMENT_LOCAL(PresentedImage, ResourceId()).TypedAs("ID3D12Resource *"_lit);

      SERIALISE_CHECK_READ_ERRORS();

      if(PresentedImage != ResourceId())
        m_Cmd.m_LastPresentedImage = PresentedImage;

      if(IsLoading(m_State) && m_Cmd.m_LastChunk != D3D12Chunk::Swapchain_Present)
      {
        m_Cmd.AddEvent();

        ActionDescription action;
        action.customName = "End of Capture";
        action.flags |= ActionFlags::Present;

        action.copyDestination = m_Cmd.m_LastPresentedImage;

        m_Cmd.AddAction(action);
      }

      ret = true;
    }
    else if(!ret)
    {
      RDCERR("Chunk failed to serialise: %s", ToStr(chunk).c_str());
      return false;
    }
  }

  if(IsLoading(m_State))
  {
    if(chunk == D3D12Chunk::List_Reset || chunk == D3D12Chunk::List_Close)
    {
      // don't add these events - they will be handled when inserted in-line into queue submit
    }
    else if(chunk == D3D12Chunk::Queue_EndEvent)
    {
      // also ignore, this just pops the action stack
    }
    else
    {
      if(!m_Cmd.m_AddedAction)
        m_Cmd.AddEvent();
    }
  }

  m_Cmd.m_AddedAction = false;

  return ret;
}

RDResult WrappedID3D12CommandQueue::ReplayLog(CaptureState readType, uint32_t startEventID,
                                              uint32_t endEventID, bool partial)
{
  m_State = readType;

  if(!m_FrameReader)
  {
    RETURN_ERROR_RESULT(ResultCode::InvalidParameter,
                        "Can't replay context capture without frame reader");
  }

  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());
  ser.SetVersion(m_pDevice->GetCaptureVersion());

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State),
                                  m_pDevice->GetTimeBase(), m_pDevice->GetTimeFrequency());

    ser.GetStructuredFile().Swap(*m_pDevice->GetStructuredFile());

    m_StructuredFile = &ser.GetStructuredFile();
  }
  else
  {
    m_StructuredFile = m_pDevice->GetStructuredFile();
  }

  m_Cmd.m_StructuredFile = m_StructuredFile;

  SystemChunk header = ser.ReadChunk<SystemChunk>();
  RDCASSERTEQUAL(header, SystemChunk::CaptureBegin);

  if(partial)
    ser.SkipCurrentChunk();
  else
    m_pDevice->Serialise_BeginCaptureFrame(ser);

  ser.EndChunk();

  m_Cmd.m_RootEvents.clear();

  if(IsLoading(m_State))
  {
    m_pDevice->ApplyInitialContents();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();

    if(m_pDevice->HasFatalError())
      return m_pDevice->FatalErrorCheck();
  }

  if(IsActiveReplaying(m_State))
  {
    APIEvent ev = GetEvent(startEventID);
    m_Cmd.m_RootEventID = ev.eventId;

    // if not partial, we need to be sure to replay
    // past the command list records, so can't
    // skip to the file offset of the first event
    if(partial)
    {
      ser.GetReader()->SetOffset(ev.fileOffset);

      D3D12CommandData::ActionUse use(ev.fileOffset, 0);
      auto it = std::lower_bound(m_Cmd.m_ActionUses.begin(), m_Cmd.m_ActionUses.end(), use);

      if(it != m_Cmd.m_ActionUses.end())
      {
        BakedCmdListInfo &cmdInfo = m_Cmd.m_BakedCmdListInfo[it->cmdList];
        cmdInfo.curEventID = it->relativeEID;
      }
    }

    m_Cmd.m_FirstEventID = startEventID;
    m_Cmd.m_LastEventID = endEventID;
  }
  else
  {
    m_Cmd.m_RootEventID = 1;
    m_Cmd.m_RootActionID = 1;
    m_Cmd.m_FirstEventID = 0;
    m_Cmd.m_LastEventID = ~0U;
  }

  if(IsReplayMode(m_State))
  {
    for(size_t i = 0; i < m_Cmd.m_IndirectBuffers.size(); i++)
      SAFE_RELEASE(m_Cmd.m_IndirectBuffers[i]);
    m_Cmd.m_IndirectBuffers.clear();
  }

  uint64_t startOffset = ser.GetReader()->GetOffset();

  for(;;)
  {
    if(IsActiveReplaying(m_State) && m_Cmd.m_RootEventID > endEventID)
    {
      // we can just break out if we've done all the events desired.
      // note that the command list events aren't 'real' and we just blaze through them
      break;
    }

    m_Cmd.m_CurChunkOffset = ser.GetReader()->GetOffset();

    D3D12Chunk context = ser.ReadChunk<D3D12Chunk>();

    if(ser.GetReader()->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    m_Cmd.m_ChunkMetadata = ser.ChunkMetadata();

    m_Cmd.m_LastCmdListID = ResourceId();

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(ser.GetReader()->IsErrored())
      return RDResult(ResultCode::APIDataCorrupted, ser.GetError().message);

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
    {
      rdcstr extra;

      if(m_pDevice->GetInfoQueue())
      {
        extra += "\n";

        for(UINT64 i = 0;
            i < m_pDevice->GetInfoQueue()->GetNumStoredMessagesAllowedByRetrievalFilter(); i++)
        {
          SIZE_T len = 0;
          m_pDevice->GetInfoQueue()->GetMessage(i, NULL, &len);

          char *msgbuf = new char[len];
          D3D12_MESSAGE *message = (D3D12_MESSAGE *)msgbuf;

          m_pDevice->GetInfoQueue()->GetMessage(i, message, &len);

          extra += "\n";
          extra += message->pDescription;

          delete[] msgbuf;
        }
      }
      else
      {
        extra +=
            "\n\nMore debugging information may be available by enabling API validation on "
            "replay via `File` -> `Open Capture with Options`";
      }

      if(m_pDevice->HasFatalError())
      {
        RDResult result = m_pDevice->FatalErrorCheck();
        result.message = rdcstr(result.message) + extra;
        return result;
      }

      m_Cmd.m_FailedReplayResult.message = rdcstr(m_Cmd.m_FailedReplayResult.message) + extra;
      return m_Cmd.m_FailedReplayResult;
    }

    if(m_pDevice->HasFatalError())
      return ResultCode::Succeeded;

    RenderDoc::Inst().SetProgress(
        LoadProgress::FrameEventsRead,
        float(m_Cmd.m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if((SystemChunk)context == SystemChunk::CaptureEnd || ser.GetReader()->AtEnd())
      break;

    // break out if we were only executing one event
    if(IsActiveReplaying(m_State) && startEventID == endEventID)
      break;

    m_Cmd.m_LastChunk = context;

    // increment root event ID either if we didn't just replay a cmd
    // buffer event, OR if we are doing a frame sub-section replay,
    // in which case it's up to the calling code to make sure we only
    // replay inside a command list (if we crossed command list
    // boundaries, the event IDs would no longer match up).
    if(m_Cmd.m_LastCmdListID == ResourceId() || startEventID > 1)
    {
      m_Cmd.m_RootEventID++;

      if(startEventID > 1)
        ser.GetReader()->SetOffset(GetEvent(m_Cmd.m_RootEventID).fileOffset);
    }
    else
    {
      // these events are completely omitted, so don't increment the curEventID
      if(context != D3D12Chunk::List_Reset && context != D3D12Chunk::List_Close)
        m_Cmd.m_BakedCmdListInfo[m_Cmd.m_LastCmdListID].curEventID++;
    }
  }

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().Swap(*m_pDevice->GetStructuredFile());

  m_StructuredFile = NULL;

  for(size_t i = 0; i < m_Cmd.m_RerecordCmdList.size(); i++)
    SAFE_RELEASE(m_Cmd.m_RerecordCmdList[i]);

  m_Cmd.m_RerecordCmds.clear();
  m_Cmd.m_RerecordCmdList.clear();

  return ResultCode::Succeeded;
}

WrappedID3D12GraphicsCommandList::WrappedID3D12GraphicsCommandList(ID3D12GraphicsCommandList *real,
                                                                   WrappedID3D12Device *device,
                                                                   CaptureState &state)
    : m_RefCounter(real, false), m_pList(real), m_pDevice(device), m_State(state)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(WrappedID3D12GraphicsCommandList));

  m_pList1 = NULL;
  m_pList2 = NULL;
  m_pList3 = NULL;
  m_pList4 = NULL;
  m_pList5 = NULL;

  m_WrappedDebug.m_pList = this;
  if(m_pList)
  {
    m_pList->QueryInterface(__uuidof(ID3D12DebugCommandList), (void **)&m_WrappedDebug.m_pReal);
    m_pList->QueryInterface(__uuidof(ID3D12DebugCommandList1), (void **)&m_WrappedDebug.m_pReal1);
    m_pList->QueryInterface(__uuidof(ID3D12DebugCommandList2), (void **)&m_WrappedDebug.m_pReal2);
    m_pList->QueryInterface(__uuidof(ID3D12DebugCommandList3), (void **)&m_WrappedDebug.m_pReal3);

    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList1), (void **)&m_pList1);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList2), (void **)&m_pList2);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList3), (void **)&m_pList3);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList4), (void **)&m_pList4);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList5), (void **)&m_pList5);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList6), (void **)&m_pList6);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList7), (void **)&m_pList7);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList8), (void **)&m_pList8);
    m_pList->QueryInterface(__uuidof(ID3D12GraphicsCommandList9), (void **)&m_pList9);
  }

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  RDCEraseEl(m_Init);

  m_ListRecord = NULL;
  m_CreationRecord = NULL;
  m_Cmd = NULL;

  m_CurGfxRootSig = NULL;
  m_CurCompRootSig = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_ListRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_ListRecord->type = Resource_GraphicsCommandList;
    m_ListRecord->DataInSerialiser = false;
    m_ListRecord->InternalResource = true;
    m_ListRecord->Length = 0;

    m_ListRecord->cmdInfo = new CmdListRecordingInfo();

    // this is set up in the implicit Reset() right after creation
    m_ListRecord->bakedCommands = NULL;

    // a bit of a hack, we make a parallel resource record with the same lifetime as the command
    // list and make it a parent, so it will hold onto our create chunk and not try to
    // record it (and throw it away with baked commands that are unused), then it'll be pulled
    // into the capture.
    m_CreationRecord =
        m_pDevice->GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    m_CreationRecord->type = Resource_GraphicsCommandList;
    m_CreationRecord->InternalResource = true;

    m_ListRecord->AddParent(m_CreationRecord);
  }
  else
  {
    m_Cmd = m_pDevice->GetQueue()->GetCommandData();
  }

  if(m_pList)
  {
    bool ret = m_pDevice->GetResourceManager()->AddWrapper(this, m_pList);
    if(!ret)
      RDCERR("Error adding wrapper for ID3D12GraphicsCommandList");
  }

  m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);

  m_pDevice->SoftRef();
}

WrappedID3D12GraphicsCommandList::~WrappedID3D12GraphicsCommandList()
{
  SAFE_RELEASE(m_AMDMarkers);

  if(m_pList)
    m_pDevice->GetResourceManager()->RemoveWrapper(m_pList);

  if(m_CreationRecord)
    m_CreationRecord->Delete(m_pDevice->GetResourceManager());

  if(m_ListRecord && m_ListRecord->bakedCommands)
    m_ListRecord->bakedCommands->Delete(m_pDevice->GetResourceManager());

  if(m_ListRecord)
    m_ListRecord->Delete(m_pDevice->GetResourceManager());

  m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());

  SAFE_RELEASE(m_WrappedDebug.m_pReal);
  SAFE_RELEASE(m_WrappedDebug.m_pReal1);
  SAFE_RELEASE(m_WrappedDebug.m_pReal2);
  SAFE_RELEASE(m_WrappedDebug.m_pReal3);
  SAFE_RELEASE(m_pList9);
  SAFE_RELEASE(m_pList8);
  SAFE_RELEASE(m_pList7);
  SAFE_RELEASE(m_pList6);
  SAFE_RELEASE(m_pList5);
  SAFE_RELEASE(m_pList4);
  SAFE_RELEASE(m_pList3);
  SAFE_RELEASE(m_pList2);
  SAFE_RELEASE(m_pList1);
  SAFE_RELEASE(m_pList);
}

bool WrappedID3D12GraphicsCommandList::ValidateRootGPUVA(D3D12_GPU_VIRTUAL_ADDRESS buffer)
{
  if(buffer == 0)
  {
    // silently drop binds of 0 since this sometimes crashes drivers, and is not legal if the
    // address is ever actually accessed (if it is accessed, whatever was bound or never bound here
    // is just as illegal)

    return true;
  }

  return false;
}

WriteSerialiser &WrappedID3D12GraphicsCommandList::GetThreadSerialiser()
{
  return m_pDevice->GetThreadSerialiser();
}

void WrappedID3D12GraphicsCommandList::AddRayDispatches(rdcarray<PatchedRayDispatch::Resources> &dispatches)
{
  dispatches.reserve(dispatches.size() + m_RayDispatches.size());
  for(const PatchedRayDispatch::Resources &r : m_RayDispatches)
  {
    dispatches.push_back(r);
    SAFE_ADDREF(r.lookupBuffer);
    SAFE_ADDREF(r.patchScratchBuffer);
    SAFE_ADDREF(r.argumentBuffer);
  }
}

rdcstr WrappedID3D12GraphicsCommandList::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D12Chunk)idx);
}

HRESULT STDMETHODCALLTYPE WrappedID3D12GraphicsCommandList::QueryInterface(REFIID riid,
                                                                           void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D12GraphicsCommandList *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList))
  {
    *ppvObject = (ID3D12GraphicsCommandList *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DebugCommandList))
  {
    if(m_WrappedDebug.m_pReal)
    {
      AddRef();
      *ppvObject = (ID3D12DebugCommandList *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DebugCommandList1))
  {
    if(m_WrappedDebug.m_pReal1)
    {
      AddRef();
      *ppvObject = (ID3D12DebugCommandList1 *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DebugCommandList2))
  {
    if(m_WrappedDebug.m_pReal2)
    {
      AddRef();
      *ppvObject = (ID3D12DebugCommandList2 *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12DebugCommandList3))
  {
    if(m_WrappedDebug.m_pReal3)
    {
      AddRef();
      *ppvObject = (ID3D12DebugCommandList3 *)&m_WrappedDebug;
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList1))
  {
    if(m_pList1)
    {
      *ppvObject = (ID3D12GraphicsCommandList1 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList2))
  {
    if(m_pList2)
    {
      *ppvObject = (ID3D12GraphicsCommandList2 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList3))
  {
    if(m_pList3)
    {
      *ppvObject = (ID3D12GraphicsCommandList3 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList4))
  {
    if(m_pList4)
    {
      *ppvObject = (ID3D12GraphicsCommandList4 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList5))
  {
    if(m_pList5)
    {
      *ppvObject = (ID3D12GraphicsCommandList5 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList6))
  {
    if(m_pList6)
    {
      *ppvObject = (ID3D12GraphicsCommandList6 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList7))
  {
    if(m_pList7)
    {
      *ppvObject = (ID3D12GraphicsCommandList7 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList8))
  {
    if(m_pList8)
    {
      *ppvObject = (ID3D12GraphicsCommandList8 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12GraphicsCommandList9))
  {
    if(m_pList9)
    {
      *ppvObject = (ID3D12GraphicsCommandList9 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D12CommandList))
  {
    *ppvObject = (ID3D12CommandList *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Pageable))
  {
    *ppvObject = (ID3D12Pageable *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12DeviceChild))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D12Object))
  {
    *ppvObject = (ID3D12DeviceChild *)this;
    AddRef();
    return S_OK;
  }

  return m_RefCounter.QueryInterface("ID3D12GraphicsCommandList", riid, ppvObject);
}

void BakedCmdListInfo::ShiftForRemoved(uint32_t shiftActionID, uint32_t shiftEID, size_t idx)
{
  rdcarray<D3D12ActionTreeNode> &actions = action->children;

  actionCount -= shiftActionID;
  eventCount -= shiftEID;

  if(idx < actions.size())
  {
    for(size_t i = idx; i < actions.size(); i++)
    {
      // should have no children as we don't push in for markers since they
      // can cross command list boundaries.
      RDCASSERT(actions[i].children.empty());

      actions[i].action.eventId -= shiftEID;
      actions[i].action.actionId -= shiftActionID;

      for(APIEvent &ev : actions[i].action.events)
        ev.eventId -= shiftEID;

      for(size_t u = 0; u < actions[i].resourceUsage.size(); u++)
        actions[i].resourceUsage[u].second.eventId -= shiftEID;
    }

    uint32_t lastEID = actions[idx].action.eventId;

    // shift any resource usage for actions after the removed section

    // patch any subsequent executes
    for(size_t i = 0; i < executeEvents.size(); i++)
    {
      if(executeEvents[i].baseEvent >= lastEID)
        executeEvents[i].baseEvent -= shiftEID;
    }
  }

  for(size_t i = 0; i < curEvents.size(); i++)
  {
    curEvents[i].eventId -= shiftEID;
  }
}

SubresourceStateVector BakedCmdListInfo::GetState(WrappedID3D12Device *device, ResourceId id)
{
  std::map<ResourceId, SubresourceStateVector> data;

  data[id] = device->GetSubresourceStates(id);

  device->GetResourceManager()->ApplyBarriers(barriers, data);

  return data[id];
}

D3D12CommandData::D3D12CommandData()
{
  m_CurChunkOffset = 0;

  m_IndirectOffset = 0;

  m_RootEventID = 1;
  m_RootActionID = 1;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_StructuredFile = NULL;

  m_pDevice = NULL;

  m_ActionCallback = NULL;

  m_AddedAction = false;

  m_RootActionStack.push_back(&m_ParentAction);
}

void D3D12CommandData::GetIndirectBuffer(size_t size, ID3D12Resource **buf, uint64_t *offs)
{
  // check if we need to allocate a new buffer
  if(m_IndirectBuffers.empty() || m_IndirectOffset + size > m_IndirectSize)
  {
    D3D12_RESOURCE_DESC indirectDesc;
    indirectDesc.Alignment = 0;
    indirectDesc.DepthOrArraySize = 1;
    indirectDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    indirectDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    indirectDesc.Format = DXGI_FORMAT_UNKNOWN;
    indirectDesc.Height = 1;
    indirectDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    indirectDesc.MipLevels = 1;
    indirectDesc.SampleDesc.Count = 1;
    indirectDesc.SampleDesc.Quality = 0;
    indirectDesc.Width = RDCMAX(AlignUp((uint64_t)size, 64ULL), m_IndirectSize);

    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    ID3D12Resource *argbuf = NULL;

    HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &indirectDesc,
                                                    D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&argbuf);

    SetObjName(argbuf, StringFormat::Fmt("Indirect Readback Buf (%llu bytes)", (uint64_t)size));

    if(FAILED(hr))
      RDCERR("Failed to create indirect buffer, HRESULT: %s", ToStr(hr).c_str());

    m_IndirectBuffers.push_back(argbuf);
    m_IndirectOffset = 0;
  }

  *buf = m_IndirectBuffers.back();
  *offs = m_IndirectOffset;

  m_IndirectOffset = AlignUp16(m_IndirectOffset + size);
}

uint32_t D3D12CommandData::HandlePreCallback(ID3D12GraphicsCommandListX *list, ActionFlags type,
                                             uint32_t multiDrawOffset)
{
  if(!m_ActionCallback)
    return 0;

  // look up the EID this action came from
  ActionUse use(m_CurChunkOffset, 0);
  auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

  if(it == m_ActionUses.end())
  {
    RDCERR("Couldn't find action use entry for %llu", m_CurChunkOffset);
    return 0;
  }

  uint32_t eventId = it->eventId;

  RDCASSERT(eventId != 0);

  // handle all aliases of this action as long as it's not a multidraw
  const ActionDescription *action = m_pDevice->GetAction(eventId);

  if(action == NULL || !(action->flags & ActionFlags::MultiAction))
  {
    ++it;
    while(it != m_ActionUses.end() && it->fileOffset == m_CurChunkOffset)
    {
      m_ActionCallback->AliasEvent(eventId, it->eventId);
      ++it;
    }
  }

  eventId += multiDrawOffset;

  switch(type)
  {
    case ActionFlags::MeshDispatch:
    case ActionFlags::Drawcall:
    {
      m_ActionCallback->PreDraw(eventId, list);
      break;
    }
    case ActionFlags::Dispatch:
    case ActionFlags::DispatchRay:
    {
      m_ActionCallback->PreDispatch(eventId, list);
      break;
    }
    default:
    {
      m_ActionCallback->PreMisc(eventId, type, list);
      break;
    }
  }

  return eventId;
}

bool D3D12CommandData::InRerecordRange(ResourceId cmdid)
{
  // if we have an outside command list, assume the range is valid and we're replaying all events
  // onto it.
  if(m_OutsideCmdList != NULL)
    return true;

  // if not, check if we're one of the actual partial command buffers and check to see if we're in
  // the range for their partial replay.
  for(int p = 0; p < ePartialNum; p++)
  {
    if(cmdid == m_Partial[p].partialParent)
    {
      return m_BakedCmdListInfo[m_Partial[p].partialParent].curEventID <=
             m_LastEventID - m_Partial[p].baseEvent;
    }
  }

  // otherwise just check if we have a re-record command list for this, as then we're doing a full
  // re-record and replay
  return m_RerecordCmds.find(cmdid) != m_RerecordCmds.end();
}

bool D3D12CommandData::HasRerecordCmdList(ResourceId cmdid)
{
  if(m_OutsideCmdList != NULL)
    return true;

  return m_RerecordCmds.find(cmdid) != m_RerecordCmds.end();
}

bool D3D12CommandData::IsPartialCmdList(ResourceId cmdid)
{
  if(m_OutsideCmdList != NULL)
    return true;

  for(int p = 0; p < ePartialNum; p++)
    if(cmdid == m_Partial[p].partialParent)
      return true;

  return false;
}

ID3D12GraphicsCommandListX *D3D12CommandData::RerecordCmdList(ResourceId cmdid,
                                                              PartialReplayIndex partialType)
{
  if(m_OutsideCmdList != NULL)
    return m_OutsideCmdList;

  auto it = m_RerecordCmds.find(cmdid);

  if(it == m_RerecordCmds.end())
  {
    RDCERR("Didn't generate re-record command for %s", ToStr(cmdid).c_str());
    return NULL;
  }

  return it->second;
}

void D3D12CommandData::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventId = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEventID
                                                     : m_RootEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  // if we're using replay-time debug messages, fetch them now since we can do better to correlate
  // to events on replay
  if(m_pDevice->GetReplayOptions().apiValidation)
    m_EventMessages = m_pDevice->GetDebugMessages();

  for(size_t i = 0; i < m_EventMessages.size(); i++)
    m_EventMessages[i].eventId = apievent.eventId;

  if(m_LastCmdListID != ResourceId())
  {
    m_BakedCmdListInfo[m_LastCmdListID].curEvents.push_back(apievent);

    rdcarray<DebugMessage> &msgs = m_BakedCmdListInfo[m_LastCmdListID].debugMessages;

    msgs.append(m_EventMessages);
  }
  else
  {
    m_RootEvents.push_back(apievent);
    m_Events.resize_for_index(apievent.eventId);
    m_Events[apievent.eventId] = apievent;

    for(auto it = m_EventMessages.begin(); it != m_EventMessages.end(); ++it)
      m_pDevice->AddDebugMessage(*it);
  }

  m_EventMessages.clear();
}

void D3D12CommandData::AddResourceUsage(D3D12ActionTreeNode &actionNode, ResourceId id,
                                        uint32_t EID, ResourceUsage usage)
{
  if(id == ResourceId())
    return;

  actionNode.resourceUsage.push_back(make_rdcpair(id, EventUsage(EID, usage)));
}

void D3D12CommandData::AddCPUUsage(ResourceId id, ResourceUsage usage)
{
  m_ResourceUses[id].push_back(EventUsage(m_RootEventID, usage));
}

void D3D12CommandData::AddUsageForBindInRootSig(const D3D12RenderState &state,
                                                D3D12ActionTreeNode &actionNode,
                                                const D3D12RenderState::RootSignature *rootsig,
                                                D3D12_DESCRIPTOR_RANGE_TYPE type, uint32_t space,
                                                uint32_t bind, uint32_t rangeSize)
{
  static bool hugeRangeWarned = false;

  ActionDescription &a = actionNode.action;
  uint32_t eid = a.eventId;

  // use a 'clamped' range size to avoid annoying overflow issues
  rangeSize = RDCMIN(rangeSize, 0x10000000U);

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12RootSignature *sig = rm->GetCurrentAs<WrappedID3D12RootSignature>(rootsig->rootsig);

  for(size_t rootEl = 0; rootEl < sig->sig.Parameters.size(); rootEl++)
  {
    if(rootEl >= rootsig->sigelems.size())
      break;

    const D3D12RootSignatureParameter &p = sig->sig.Parameters[rootEl];
    const D3D12RenderState::SignatureElement &el = rootsig->sigelems[rootEl];

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      continue;

    ResourceUsage cb = ResourceUsage::CS_Constants;
    ResourceUsage ro = ResourceUsage::CS_Resource;
    ResourceUsage rw = ResourceUsage::CS_RWResource;

    if(rootsig == &state.graphics)
    {
      if(p.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
      {
        cb = ResourceUsage::All_Constants;
        ro = ResourceUsage::All_Resource;
        rw = ResourceUsage::All_RWResource;
      }
      else
      {
        cb = CBUsage(p.ShaderVisibility - D3D12_SHADER_VISIBILITY_VERTEX);
        ro = ResUsage(p.ShaderVisibility - D3D12_SHADER_VISIBILITY_VERTEX);
        rw = RWResUsage(p.ShaderVisibility - D3D12_SHADER_VISIBILITY_VERTEX);
      }
    }

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && el.type == eRootCBV &&
       type == D3D12_DESCRIPTOR_RANGE_TYPE_CBV && p.Descriptor.RegisterSpace == space &&
       p.Descriptor.ShaderRegister >= bind && p.Descriptor.ShaderRegister < bind + rangeSize)
    {
      AddResourceUsage(actionNode, el.id, eid, cb);

      // common case - root element matches 1:1 with a non-array shader bind, if so we can exit. If
      // not we might have to continue since other parts of it might be mapped to a table, or
      // another root element
      if(rangeSize == 1)
        return;
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && el.type == eRootSRV &&
            type == D3D12_DESCRIPTOR_RANGE_TYPE_SRV && p.Descriptor.RegisterSpace == space &&
            p.Descriptor.ShaderRegister >= bind && p.Descriptor.ShaderRegister < bind + rangeSize)
    {
      AddResourceUsage(actionNode, el.id, eid, ro);

      // common case - root element matches 1:1 with a non-array shader bind, if so we can exit. If
      // not we might have to continue since other parts of it might be mapped to a table, or
      // another root element
      if(rangeSize == 1)
        return;
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && el.type == eRootUAV &&
            type == D3D12_DESCRIPTOR_RANGE_TYPE_UAV && p.Descriptor.RegisterSpace == space &&
            p.Descriptor.ShaderRegister >= bind && p.Descriptor.ShaderRegister < bind + rangeSize)
    {
      AddResourceUsage(actionNode, el.id, eid, rw);

      // common case - root element matches 1:1 with a non-array shader bind, if so we can exit. If
      // not we might have to continue since other parts of it might be mapped to a table, or
      // another root element
      if(rangeSize == 1)
        return;
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && el.type == eRootTable)
    {
      WrappedID3D12DescriptorHeap *heap =
          m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(el.id);

      if(heap == NULL)
        continue;

      UINT prevTableOffset = 0;

      for(size_t r = 0; r < p.ranges.size(); r++)
      {
        const D3D12_DESCRIPTOR_RANGE1 &range = p.ranges[r];

        UINT offset = range.OffsetInDescriptorsFromTableStart;

        if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
          offset = prevTableOffset;

        D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
        desc += el.offset;
        desc += offset;

        UINT num = range.NumDescriptors;

        if(num == UINT_MAX)
        {
          // find out how many descriptors are left after
          num = heap->GetNumDescriptors() - offset - UINT(el.offset);
        }

        prevTableOffset = offset + num;

        // skip ranges that aren't the type or register space we want
        if(range.RangeType != type || range.RegisterSpace != space)
          continue;

        // skip ranges that don't overlap with the registers we are looking for at all
        if(range.BaseShaderRegister + num <= bind || range.BaseShaderRegister >= bind + rangeSize)
          continue;

        if(num > 1000)
        {
          if(!hugeRangeWarned)
            RDCWARN("Skipping large, most likely 'bindless', descriptor range");
          hugeRangeWarned = true;

          continue;
        }

        bool allInRange = (bind >= range.BaseShaderRegister && rangeSize <= range.NumDescriptors);

        // move to the first descriptor in the range which is in the binding we want, if the binding
        // is later on in the range.
        //
        // It's also possible that the range is later on in the binding (e.g. if the binding is at
        // base register 5 and is 1000000 in length, the range could start at register 10. In that
        // case we just consume as much of the range as still fits in the bind
        if(bind > range.BaseShaderRegister)
          desc += (bind - range.BaseShaderRegister);
        if(range.BaseShaderRegister > bind)
          rangeSize -= (range.BaseShaderRegister - bind);

        if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
        {
          EventUsage usage(eid, cb);

          for(UINT i = 0; i < num && i < rangeSize; i++)
          {
            ResourceId id = WrappedID3D12Resource::GetResIDFromAddr(desc->GetCBV().BufferLocation);

            AddResourceUsage(actionNode, id, eid, cb);

            desc++;
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ||
                range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
        {
          ResourceUsage usage = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? ro : rw;

          for(UINT i = 0; i < num && i < rangeSize; i++)
          {
            AddResourceUsage(actionNode, desc->GetResResourceId(), eid, usage);

            desc++;
          }
        }

        // if this descriptor range fully covered the binding (which may be quite common) we can
        // return now, other ranges/root elements won't overlap so don't bother looking at them
        if(allInRange)
          return;
      }
    }
  }
}

void D3D12CommandData::AddUsage(const D3D12RenderState &state, D3D12ActionTreeNode &actionNode)
{
  ActionDescription &a = actionNode.action;

  uint32_t eid = a.eventId;

  ActionFlags DrawMask = ActionFlags::Drawcall | ActionFlags::MeshDispatch | ActionFlags::Dispatch;
  if(!(a.flags & DrawMask))
    return;

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  const D3D12RenderState::RootSignature *rootsig = NULL;

  WrappedID3D12PipelineState *pipe = NULL;

  if(state.pipe != ResourceId())
    pipe = rm->GetCurrentAs<WrappedID3D12PipelineState>(state.pipe);

  const ShaderReflection *refls[NumShaderStages] = {};

  if((a.flags & ActionFlags::Dispatch) && state.compute.rootsig != ResourceId())
  {
    rootsig = &state.compute;

    if(pipe && pipe->IsCompute())
    {
      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)pipe->compute->CS.pShaderBytecode;

      refls[uint32_t(ShaderStage::Compute)] = &sh->GetDetails();
    }
  }
  else if(state.graphics.rootsig != ResourceId())
  {
    rootsig = &state.graphics;

    if(pipe && pipe->IsGraphics())
    {
      D3D12_SHADER_BYTECODE *srcArr[] = {
          &pipe->graphics->VS,
          &pipe->graphics->HS,
          &pipe->graphics->DS,
          &pipe->graphics->GS,
          &pipe->graphics->PS,
          // compute
          NULL,
          &pipe->graphics->AS,
          &pipe->graphics->MS,
      };
      for(size_t stage = 0; stage < ARRAY_COUNT(srcArr); stage++)
      {
        if(!srcArr[stage])
          continue;

        WrappedID3D12Shader *sh = (WrappedID3D12Shader *)srcArr[stage]->pShaderBytecode;

        if(sh)
          refls[stage] = &sh->GetDetails();
      }
    }

    if(a.flags & ActionFlags::Indexed && state.ibuffer.buf != ResourceId())
      actionNode.resourceUsage.push_back(
          make_rdcpair(state.ibuffer.buf, EventUsage(eid, ResourceUsage::IndexBuffer)));

    if(a.flags & ActionFlags::Drawcall)
    {
      for(size_t i = 0; i < state.vbuffers.size(); i++)
      {
        if(state.vbuffers[i].buf != ResourceId())
          actionNode.resourceUsage.push_back(
              make_rdcpair(state.vbuffers[i].buf, EventUsage(eid, ResourceUsage::VertexBuffer)));
      }

      for(size_t i = 0; i < state.streamouts.size(); i++)
      {
        if(state.streamouts[i].buf != ResourceId())
          actionNode.resourceUsage.push_back(
              make_rdcpair(state.streamouts[i].buf, EventUsage(eid, ResourceUsage::StreamOut)));
        if(state.streamouts[i].countbuf != ResourceId())
          actionNode.resourceUsage.push_back(make_rdcpair(
              state.streamouts[i].countbuf, EventUsage(eid, ResourceUsage::StreamOut)));
      }
    }

    rdcarray<ResourceId> rts = state.GetRTVIDs();

    for(size_t i = 0; i < rts.size(); i++)
    {
      if(rts[i] != ResourceId())
        actionNode.resourceUsage.push_back(
            make_rdcpair(rts[i], EventUsage(eid, ResourceUsage::ColorTarget)));
    }

    ResourceId id = state.GetDSVID();
    if(id != ResourceId())
      actionNode.resourceUsage.push_back(
          make_rdcpair(id, EventUsage(eid, ResourceUsage::DepthStencilTarget)));
  }

  if(rootsig)
  {
    // iterate over each stage, looking at its used binds, then for each bind find it in the root
    // signature. We have to do this kind of N:N lookup because of D3D12's bad design, but this
    // should be a better way around to do it than iterating over the root signature and finding a
    // bind for each element
    for(size_t sh = 0; sh < ARRAY_COUNT(refls); sh++)
    {
      if(!refls[sh])
        continue;

      for(const ConstantBlock &b : refls[sh]->constantBlocks)
      {
        AddUsageForBindInRootSig(state, actionNode, rootsig, D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                                 b.fixedBindSetOrSpace, b.fixedBindNumber, b.bindArraySize);
      }

      for(const ShaderResource &r : refls[sh]->readOnlyResources)
      {
        AddUsageForBindInRootSig(state, actionNode, rootsig, D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                 r.fixedBindSetOrSpace, r.fixedBindNumber, r.bindArraySize);
      }

      for(const ShaderResource &r : refls[sh]->readWriteResources)
      {
        AddUsageForBindInRootSig(state, actionNode, rootsig, D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                 r.fixedBindSetOrSpace, r.fixedBindNumber, r.bindArraySize);
      }
    }
  }
}

void D3D12CommandData::AddAction(const ActionDescription &a)
{
  m_AddedAction = true;

  ActionDescription action = a;
  action.eventId = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEventID
                                                   : m_RootEventID;
  action.actionId = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].actionCount
                                                    : m_RootActionID;

  for(int i = 0; i < 8; i++)
    action.outputs[i] = ResourceId();

  action.depthOut = ResourceId();

  if(m_LastCmdListID != ResourceId())
  {
    rdcarray<ResourceId> rts = m_BakedCmdListInfo[m_LastCmdListID].state.GetRTVIDs();

    for(size_t i = 0; i < ARRAY_COUNT(action.outputs); i++)
    {
      if(i < rts.size())
        action.outputs[i] = m_pDevice->GetResourceManager()->GetOriginalID(rts[i]);
      else
        action.outputs[i] = ResourceId();
    }

    action.depthOut = m_pDevice->GetResourceManager()->GetOriginalID(
        m_BakedCmdListInfo[m_LastCmdListID].state.GetDSVID());
  }

  // markers don't increment action ID
  ActionFlags MarkerMask = ActionFlags::SetMarker | ActionFlags::PushMarker |
                           ActionFlags::PopMarker | ActionFlags::PassBoundary;
  if(!(action.flags & MarkerMask))
  {
    if(m_LastCmdListID != ResourceId())
      m_BakedCmdListInfo[m_LastCmdListID].actionCount++;
    else
      m_RootActionID++;
  }

  action.events.swap(m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEvents
                                                     : m_RootEvents);

  // should have at least the root action here, push this action
  // onto the back's children list.
  if(!GetActionStack().empty())
  {
    D3D12ActionTreeNode node(action);

    node.resourceUsage.swap(m_BakedCmdListInfo[m_LastCmdListID].resourceUsage);

    if(m_LastCmdListID != ResourceId())
      AddUsage(m_BakedCmdListInfo[m_LastCmdListID].state, node);

    for(const ActionDescription &child : action.children)
      node.children.push_back(D3D12ActionTreeNode(child));
    GetActionStack().back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost action stack!");
}

void D3D12CommandData::InsertActionsAndRefreshIDs(ResourceId cmd,
                                                  rdcarray<D3D12ActionTreeNode> &cmdBufNodes)
{
  // assign new action IDs
  for(size_t i = 0; i < cmdBufNodes.size(); i++)
  {
    D3D12ActionTreeNode n = cmdBufNodes[i];
    n.action.eventId += m_RootEventID;
    n.action.actionId += m_RootActionID;

    for(APIEvent &ev : n.action.events)
    {
      ev.eventId += m_RootEventID;
      m_Events.resize(ev.eventId + 1);
      m_Events[ev.eventId] = ev;
    }

    ActionUse use(m_Events.back().fileOffset, n.action.eventId, cmd, cmdBufNodes[i].action.eventId);

    // insert in sorted location
    auto drawit = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);
    m_ActionUses.insert(drawit - m_ActionUses.begin(), use);

    RDCASSERT(n.children.empty());

    for(auto it = n.resourceUsage.begin(); it != n.resourceUsage.end(); ++it)
    {
      EventUsage u = it->second;
      u.eventId += m_RootEventID;
      m_ResourceUses[it->first].push_back(u);
    }

    GetActionStack().back()->children.push_back(n);

    // if this is a push marker too, step down the action stack
    if(cmdBufNodes[i].action.flags & ActionFlags::PushMarker)
      GetActionStack().push_back(&GetActionStack().back()->children.back());

    // similarly for a pop, but don't pop off the root
    if((cmdBufNodes[i].action.flags & ActionFlags::PopMarker) && GetActionStack().size() > 1)
      GetActionStack().pop_back();
  }
}
