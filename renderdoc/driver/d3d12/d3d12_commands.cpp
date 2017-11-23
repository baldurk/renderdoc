/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandQueue::AddRef()
{
  m_pQueue->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandQueue::Release()
{
  m_pQueue->Release();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandList::AddRef()
{
  m_pList->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE WrappedID3D12DebugCommandList::Release()
{
  m_pList->Release();
  return 1;
}

WrappedID3D12CommandQueue::WrappedID3D12CommandQueue(ID3D12CommandQueue *real,
                                                     WrappedID3D12Device *device, CaptureState &state)
    : RefCounter12(real), m_pDevice(device), m_State(state)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this,
                                                              sizeof(WrappedID3D12CommandQueue));

  if(m_pReal)
    m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandQueue), (void **)&m_WrappedDebug.m_pReal);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_ReplayList = new WrappedID3D12GraphicsCommandList(NULL, m_pDevice, state);

    m_ReplayList->SetCommandData(&m_Cmd);
  }

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_QueueRecord = NULL;

  m_Cmd.m_pDevice = m_pDevice;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_QueueRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_QueueRecord->type = Resource_CommandQueue;
    m_QueueRecord->DataInSerialiser = false;
    m_QueueRecord->SpecialResource = true;
    m_QueueRecord->Length = 0;
  }

  m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);

  m_pDevice->SoftRef();
}

WrappedID3D12CommandQueue::~WrappedID3D12CommandQueue()
{
  m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());

  for(size_t i = 0; i < m_Cmd.m_IndirectBuffers.size(); i++)
    SAFE_RELEASE(m_Cmd.m_IndirectBuffers[i]);

  SAFE_RELEASE(m_WrappedDebug.m_pReal);
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
  else
  {
    string guid = ToStr(riid);
    RDCWARN("Querying ID3D12CommandQueue for interface: %s", guid.c_str());
  }

  return RefCounter12::QueryInterface(riid, ppvObject);
}

void WrappedID3D12CommandQueue::ClearAfterCapture()
{
  // delete cmd buffers now - had to keep them alive until after serialiser flush.
  for(size_t i = 0; i < m_CmdListRecords.size(); i++)
    m_CmdListRecords[i]->Delete(GetResourceManager());

  m_CmdListRecords.clear();

  m_QueueRecord->DeleteChunks();
}

WriteSerialiser &WrappedID3D12CommandQueue::GetThreadSerialiser()
{
  return m_pDevice->GetThreadSerialiser();
}

std::string WrappedID3D12CommandQueue::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D12Chunk)idx);
}

const APIEvent &WrappedID3D12CommandQueue::GetEvent(uint32_t eventID)
{
  for(const APIEvent &e : m_Cmd.m_Events)
  {
    if(e.eventID >= eventID)
      return e;
  }

  return m_Cmd.m_Events.back();
}

bool WrappedID3D12CommandQueue::ProcessChunk(ReadSerialiser &ser, D3D12Chunk chunk)
{
  m_Cmd.m_AddedDrawcall = false;

  bool ret = false;

  switch(chunk)
  {
    case D3D12Chunk::Device_CreateConstantBufferView:
    case D3D12Chunk::Device_CreateShaderResourceView:
    case D3D12Chunk::Device_CreateUnorderedAccessView:
    case D3D12Chunk::Device_CreateRenderTargetView:
    case D3D12Chunk::Device_CreateDepthStencilView:
    case D3D12Chunk::Device_CreateSampler:
      ret = m_pDevice->Serialise_DynamicDescriptorWrite(ser, NULL);
      break;
    case D3D12Chunk::Device_CopyDescriptors:
    case D3D12Chunk::Device_CopyDescriptorsSimple:
      ret = m_pDevice->Serialise_DynamicDescriptorCopies(ser, std::vector<DynamicDescriptorCopy>());
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

    case D3D12Chunk::PushMarker: ret = m_ReplayList->Serialise_BeginEvent(ser, 0, NULL, 0); break;
    case D3D12Chunk::PopMarker: ret = m_ReplayList->Serialise_EndEvent(ser); break;
    case D3D12Chunk::SetMarker: ret = m_ReplayList->Serialise_SetMarker(ser, 0, NULL, 0); break;

    case D3D12Chunk::Resource_Unmap:
      ret = m_pDevice->Serialise_MapDataWrite(ser, NULL, 0, NULL, D3D12_RANGE());
      break;
    case D3D12Chunk::Resource_WriteToSubresource:
      ret = m_pDevice->Serialise_WriteToSubresource(ser, NULL, 0, NULL, NULL, 0, 0);
      break;

    case D3D12Chunk::CaptureEnd:
    {
      SERIALISE_ELEMENT_LOCAL(PresentedImage, ResourceId());

      SERIALISE_CHECK_READ_ERRORS();

      m_BackbufferID = PresentedImage;

      if(IsLoading(m_State))
      {
        m_Cmd.AddEvent();

        DrawcallDescription draw;
        draw.name = "Present()";
        draw.flags |= DrawFlags::Present;

        draw.copyDestination = m_BackbufferID;

        m_Cmd.AddDrawcall(draw, true);
      }

      ret = true;
    }
    default: RDCERR("Unrecognised Chunk type %s", ToStr(chunk).c_str()); break;
  }

  if(IsLoading(m_State))
  {
    if(chunk == D3D12Chunk::PushMarker)
    {
      // no push/pop necessary
    }
    else if(chunk == D3D12Chunk::SetMarker || chunk == D3D12Chunk::PopMarker)
    {
      // don't add these events - they will be handled when inserted in-line into queue submit
    }
    else
    {
      if(!m_Cmd.m_AddedDrawcall)
        m_Cmd.AddEvent();
    }
  }

  m_Cmd.m_AddedDrawcall = false;

  return ret;
}

ReplayStatus WrappedID3D12CommandQueue::ReplayLog(CaptureState readType, uint32_t startEventID,
                                                  uint32_t endEventID, bool partial)
{
  m_State = readType;

  if(!m_FrameReader)
  {
    RDCERR("Can't replay context capture without frame reader");
    return ReplayStatus::InternalError;
  }

  m_FrameReader->SetOffset(0);

  ReadSerialiser ser(m_FrameReader, Ownership::Nothing);

  ser.SetStringDatabase(&m_StringDB);
  ser.SetUserData(GetResourceManager());

  if(IsLoading(m_State) || IsStructuredExporting(m_State))
  {
    ser.ConfigureStructuredExport(&GetChunkName, IsStructuredExporting(m_State));

    ser.GetStructuredFile().swap(m_pDevice->GetStructuredFile());

    m_StructuredFile = &ser.GetStructuredFile();
  }
  else
  {
    m_StructuredFile = &m_pDevice->GetStructuredFile();
  }

  m_Cmd.m_StructuredFile = m_StructuredFile;

  D3D12Chunk header = ser.ReadChunk<D3D12Chunk>();
  RDCASSERTEQUAL(header, D3D12Chunk::CaptureBegin);

  m_pDevice->Serialise_BeginCaptureFrame(ser, !partial);

  ser.EndChunk();

  m_Cmd.m_RootEvents.clear();

  if(IsLoading(m_State))
  {
    m_pDevice->ApplyInitialContents();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }

  if(IsActiveReplaying(m_State))
  {
    APIEvent ev = GetEvent(startEventID);
    m_Cmd.m_RootEventID = ev.eventID;

    // if not partial, we need to be sure to replay
    // past the command list records, so can't
    // skip to the file offset of the first event
    if(partial)
    {
      ser.GetReader()->SetOffset(ev.fileOffset);

      D3D12CommandData::DrawcallUse use(ev.fileOffset, 0);
      auto it = std::lower_bound(m_Cmd.m_DrawcallUses.begin(), m_Cmd.m_DrawcallUses.end(), use);

      if(it != m_Cmd.m_DrawcallUses.end())
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
    m_Cmd.m_RootDrawcallID = 1;
    m_Cmd.m_FirstEventID = 0;
    m_Cmd.m_LastEventID = ~0U;
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
      return ReplayStatus::APIDataCorrupted;

    m_Cmd.m_ChunkMetadata = ser.ChunkMetadata();

    m_Cmd.m_LastCmdListID = ResourceId();

    bool success = ProcessChunk(ser, context);

    ser.EndChunk();

    if(ser.GetReader()->IsErrored())
      return ReplayStatus::APIDataCorrupted;

    // if there wasn't a serialisation error, but the chunk didn't succeed, then it's an API replay
    // failure.
    if(!success)
      return m_FailedReplayStatus;

    RenderDoc::Inst().SetProgress(FileInitialRead, float(m_Cmd.m_CurChunkOffset - startOffset) /
                                                       float(ser.GetReader()->GetSize()));

    // for now just abort after capture scope. Really we'd need to support multiple frames
    // but for now this will do.
    if(context == D3D12Chunk::CaptureEnd)
      break;

    // break out if we were only executing one event
    if(IsActiveReplaying(m_State) && startEventID == endEventID)
      break;

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
      m_Cmd.m_BakedCmdListInfo[m_Cmd.m_LastCmdListID].curEventID++;
    }
  }

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().swap(m_pDevice->GetStructuredFile());

  m_StructuredFile = NULL;

  if(IsLoading(m_State))
  {
    struct SortEID
    {
      bool operator()(const APIEvent &a, const APIEvent &b) { return a.eventID < b.eventID; }
    };

    std::sort(m_Cmd.m_Events.begin(), m_Cmd.m_Events.end(), SortEID());
  }

  for(int p = 0; p < D3D12CommandData::ePartialNum; p++)
    SAFE_RELEASE(m_Cmd.m_Partial[p].resultPartialCmdList);

  for(auto it = m_Cmd.m_RerecordCmds.begin(); it != m_Cmd.m_RerecordCmds.end(); ++it)
    SAFE_RELEASE(it->second);

  m_Cmd.m_RerecordCmds.clear();

  return ReplayStatus::Succeeded;
}

WrappedID3D12GraphicsCommandList::WrappedID3D12GraphicsCommandList(ID3D12GraphicsCommandList *real,
                                                                   WrappedID3D12Device *device,
                                                                   CaptureState &state)
    : RefCounter12(real), m_pDevice(device), m_State(state)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(
        this, sizeof(WrappedID3D12GraphicsCommandList));

  if(m_pReal)
    m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandList), (void **)&m_WrappedDebug.m_pReal);

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
    m_ListRecord->SpecialResource = true;
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
    m_CreationRecord->SpecialResource = true;

    m_ListRecord->AddParent(m_CreationRecord);
  }
  else
  {
    m_Cmd = m_pDevice->GetQueue()->GetCommandData();
  }

  if(m_pReal)
  {
    bool ret = m_pDevice->GetResourceManager()->AddWrapper(this, m_pReal);
    if(!ret)
      RDCERR("Error adding wrapper for ID3D12GraphicsCommandList");
  }

  m_pDevice->GetResourceManager()->AddCurrentResource(GetResourceID(), this);

  m_pDevice->SoftRef();
}

WrappedID3D12GraphicsCommandList::~WrappedID3D12GraphicsCommandList()
{
  SAFE_RELEASE(m_AMDMarkers);

  if(m_pReal)
    m_pDevice->GetResourceManager()->RemoveWrapper(m_pReal);

  if(m_CreationRecord)
    m_CreationRecord->Delete(m_pDevice->GetResourceManager());

  if(m_ListRecord && m_ListRecord->bakedCommands)
    m_ListRecord->bakedCommands->Delete(m_pDevice->GetResourceManager());

  m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());

  SAFE_RELEASE(m_WrappedDebug.m_pReal);
  SAFE_RELEASE(m_pReal);
}

bool WrappedID3D12GraphicsCommandList::ValidateRootGPUVA(D3D12_GPU_VIRTUAL_ADDRESS buffer)
{
  if(buffer == 0)
  {
    // abort, we don't have this buffer. Print errors while reading
    if(IsLoading(m_State))
    {
      m_pDevice->AddDebugMessage(MessageCategory::Resource_Manipulation, MessageSeverity::Medium,
                                 MessageSource::IncorrectAPIUse,
                                 "Binding 0 as a GPU Virtual Address in a root constant is "
                                 "invalid. This call will be dropped during replay.");
    }

    return true;
  }

  return false;
}

WriteSerialiser &WrappedID3D12GraphicsCommandList::GetThreadSerialiser()
{
  return m_pDevice->GetThreadSerialiser();
}

std::string WrappedID3D12GraphicsCommandList::GetChunkName(uint32_t idx)
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
  else
  {
    string guid = ToStr(riid);
    RDCWARN("Querying ID3D12GraphicsCommandList for interface: %s", guid.c_str());
  }

  return RefCounter12::QueryInterface(riid, ppvObject);
}

void BakedCmdListInfo::ShiftForRemoved(uint32_t shiftDrawID, uint32_t shiftEID, size_t idx)
{
  std::vector<D3D12DrawcallTreeNode> &draws = draw->children;

  drawCount -= shiftDrawID;
  eventCount -= shiftEID;

  if(idx < draws.size())
  {
    for(size_t i = idx; i < draws.size(); i++)
    {
      // should have no children as we don't push in for markers since they
      // can cross command list boundaries.
      RDCASSERT(draws[i].children.empty());

      draws[i].draw.eventID -= shiftEID;
      draws[i].draw.drawcallID -= shiftDrawID;

      for(APIEvent &ev : draws[i].draw.events)
        ev.eventID -= shiftEID;
    }

    uint32_t lastEID = draws[idx].draw.eventID;

    // shift any resource usage for drawcalls after the removed section
    for(size_t i = 0; i < draw->resourceUsage.size(); i++)
    {
      if(draw->resourceUsage[i].second.eventID >= lastEID)
        draw->resourceUsage[i].second.eventID -= shiftEID;
    }

    // patch any subsequent executes
    for(size_t i = 0; i < executeEvents.size(); i++)
    {
      if(executeEvents[i].baseEvent >= lastEID)
      {
        executeEvents[i].baseEvent -= shiftEID;

        if(executeEvents[i].lastEvent > 0)
          executeEvents[i].lastEvent -= shiftEID;
      }
    }
  }
}

D3D12CommandData::D3D12CommandData()
{
  m_CurChunkOffset = 0;

  m_IndirectOffset = 0;

  m_RootEventID = 1;
  m_RootDrawcallID = 1;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_StructuredFile = NULL;

  m_pDevice = NULL;

  m_DrawcallCallback = NULL;

  m_AddedDrawcall = false;

  m_RootDrawcallStack.push_back(&m_ParentDrawcall);
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
    indirectDesc.Width = m_IndirectSize;

    // create a custom heap that sits in CPU memory and is mappable, but we can
    // use for indirect args (unlike upload and readback).
    D3D12_HEAP_PROPERTIES heapProps;
    heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    ID3D12Resource *argbuf = NULL;

    HRESULT hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &indirectDesc,
                                                    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, NULL,
                                                    __uuidof(ID3D12Resource), (void **)&argbuf);

    SetObjName(argbuf, StringFormat::Fmt("Indirect Arg Buf (%llu bytes)", (uint64_t)size));

    if(FAILED(hr))
      RDCERR("Failed to create indirect buffer, HRESULT: %s", ToStr(hr).c_str());

    m_IndirectBuffers.push_back(argbuf);
    m_IndirectOffset = 0;
  }

  *buf = m_IndirectBuffers.back();
  *offs = m_IndirectOffset;

  m_IndirectOffset = AlignUp16(m_IndirectOffset + size);
}

uint32_t D3D12CommandData::HandlePreCallback(ID3D12GraphicsCommandList *list, bool dispatch,
                                             uint32_t multiDrawOffset)
{
  if(!m_DrawcallCallback)
    return 0;

  // look up the EID this drawcall came from
  DrawcallUse use(m_CurChunkOffset, 0);
  auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);

  if(it == m_DrawcallUses.end())
  {
    RDCERR("Couldn't find drawcall use entry for %llu", m_CurChunkOffset);
    return 0;
  }

  uint32_t eventID = it->eventID;

  RDCASSERT(eventID != 0);

  // handle all aliases of this drawcall as long as it's not a multidraw
  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventID);

  if(draw == NULL || !(draw->flags & DrawFlags::MultiDraw))
  {
    ++it;
    while(it != m_DrawcallUses.end() && it->fileOffset == m_CurChunkOffset)
    {
      m_DrawcallCallback->AliasEvent(eventID, it->eventID);
      ++it;
    }
  }

  eventID += multiDrawOffset;

  if(dispatch)
    m_DrawcallCallback->PreDispatch(eventID, list);
  else
    m_DrawcallCallback->PreDraw(eventID, list);

  return eventID;
}

bool D3D12CommandData::ShouldRerecordCmd(ResourceId cmdid)
{
  if(m_Partial[Primary].outsideCmdList != NULL)
    return true;

  if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
    return true;

  return cmdid == m_Partial[Primary].partialParent || cmdid == m_Partial[Secondary].partialParent;
}

bool D3D12CommandData::InRerecordRange(ResourceId cmdid)
{
  if(m_Partial[Primary].outsideCmdList != NULL)
    return true;

  if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
    return true;

  for(int p = 0; p < ePartialNum; p++)
  {
    if(cmdid == m_Partial[p].partialParent)
    {
      return m_BakedCmdListInfo[m_Partial[p].partialParent].curEventID <=
             m_LastEventID - m_Partial[p].baseEvent;
    }
  }

  return false;
}

ID3D12GraphicsCommandList *D3D12CommandData::RerecordCmdList(ResourceId cmdid,
                                                             PartialReplayIndex partialType)
{
  if(m_Partial[Primary].outsideCmdList != NULL)
    return m_Partial[Primary].outsideCmdList;

  if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
  {
    auto it = m_RerecordCmds.find(cmdid);

    if(it == m_RerecordCmds.end())
    {
      RDCERR("Didn't generate re-record command for %llu", cmdid);
      return NULL;
    }

    return it->second;
  }

  if(partialType != ePartialNum)
    return m_Partial[partialType].resultPartialCmdList;

  for(int p = 0; p < ePartialNum; p++)
    if(cmdid == m_Partial[p].partialParent)
      return m_Partial[p].resultPartialCmdList;

  RDCERR("Calling re-record for invalid command list id");

  return NULL;
}

void D3D12CommandData::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEventID
                                                     : m_RootEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  apievent.callstack = m_ChunkMetadata.callstack;

  for(size_t i = 0; i < m_EventMessages.size(); i++)
    m_EventMessages[i].eventID = apievent.eventID;

  if(m_LastCmdListID != ResourceId())
  {
    m_BakedCmdListInfo[m_LastCmdListID].curEvents.push_back(apievent);

    std::vector<DebugMessage> &msgs = m_BakedCmdListInfo[m_LastCmdListID].debugMessages;

    msgs.insert(msgs.end(), m_EventMessages.begin(), m_EventMessages.end());
  }
  else
  {
    m_RootEvents.push_back(apievent);
    m_Events.push_back(apievent);

    for(auto it = m_EventMessages.begin(); it != m_EventMessages.end(); ++it)
      m_pDevice->AddDebugMessage(*it);
  }

  m_EventMessages.clear();
}

void D3D12CommandData::AddUsage(D3D12DrawcallTreeNode &drawNode, ResourceId id, uint32_t EID,
                                ResourceUsage usage)
{
  if(id == ResourceId())
    return;

  drawNode.resourceUsage.push_back(std::make_pair(id, EventUsage(EID, usage)));
}

void D3D12CommandData::AddUsage(D3D12DrawcallTreeNode &drawNode)
{
  DrawcallDescription &d = drawNode.draw;

  const D3D12RenderState &state = m_BakedCmdListInfo[m_LastCmdListID].state;
  uint32_t e = d.eventID;

  DrawFlags DrawMask = DrawFlags::Drawcall | DrawFlags::Dispatch;
  if(!(d.flags & DrawMask))
    return;

  const D3D12RenderState::RootSignature *rootdata = NULL;

  if((d.flags & DrawFlags::Dispatch) && state.compute.rootsig != ResourceId())
  {
    rootdata = &state.compute;
  }
  else if(state.graphics.rootsig != ResourceId())
  {
    rootdata = &state.graphics;

    if(d.flags & DrawFlags::UseIBuffer && state.ibuffer.buf != ResourceId())
      drawNode.resourceUsage.push_back(
          std::make_pair(state.ibuffer.buf, EventUsage(e, ResourceUsage::IndexBuffer)));

    for(size_t i = 0; i < state.vbuffers.size(); i++)
    {
      if(state.vbuffers[i].buf != ResourceId())
        drawNode.resourceUsage.push_back(
            std::make_pair(state.vbuffers[i].buf, EventUsage(e, ResourceUsage::VertexBuffer)));
    }

    for(size_t i = 0; i < state.streamouts.size(); i++)
    {
      if(state.streamouts[i].buf != ResourceId())
        drawNode.resourceUsage.push_back(
            std::make_pair(state.streamouts[i].buf, EventUsage(e, ResourceUsage::StreamOut)));
      if(state.streamouts[i].countbuf != ResourceId())
        drawNode.resourceUsage.push_back(
            std::make_pair(state.streamouts[i].countbuf, EventUsage(e, ResourceUsage::StreamOut)));
    }

    vector<ResourceId> rts = state.GetRTVIDs();

    for(size_t i = 0; i < rts.size(); i++)
    {
      if(rts[i] != ResourceId())
        drawNode.resourceUsage.push_back(
            std::make_pair(rts[i], EventUsage(e, ResourceUsage::ColorTarget)));
    }

    ResourceId id = state.GetDSVID();
    if(id != ResourceId())
      drawNode.resourceUsage.push_back(
          std::make_pair(id, EventUsage(e, ResourceUsage::DepthStencilTarget)));
  }

  if(rootdata)
  {
    WrappedID3D12RootSignature *sig =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootdata->rootsig);

    for(size_t rootEl = 0; rootEl < sig->sig.params.size(); rootEl++)
    {
      if(rootEl >= rootdata->sigelems.size())
        break;

      const D3D12RootSignatureParameter &p = sig->sig.params[rootEl];
      const D3D12RenderState::SignatureElement &el = rootdata->sigelems[rootEl];

      ResourceUsage cb = ResourceUsage::CS_Constants;
      ResourceUsage ro = ResourceUsage::CS_Resource;
      ResourceUsage rw = ResourceUsage::CS_RWResource;

      if(rootdata == &state.graphics)
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

      if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && el.type == eRootCBV)
      {
        AddUsage(drawNode, el.id, e, cb);
      }
      else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && el.type == eRootSRV)
      {
        AddUsage(drawNode, el.id, e, ro);
      }
      else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && el.type == eRootUAV)
      {
        AddUsage(drawNode, el.id, e, rw);
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

          if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
          {
            EventUsage usage(e, cb);

            for(UINT i = 0; i < num; i++)
            {
              ResourceId id =
                  WrappedID3D12Resource::GetResIDFromAddr(desc->nonsamp.cbv.BufferLocation);

              AddUsage(drawNode, id, e, cb);

              desc++;
            }
          }
          else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ||
                  range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
          {
            ResourceUsage usage = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV ? ro : rw;

            for(UINT i = 0; i < num; i++)
            {
              AddUsage(drawNode, GetResID(desc->nonsamp.resource), e, usage);

              desc++;
            }
          }
        }
      }
    }
  }
}

void D3D12CommandData::AddDrawcall(const DrawcallDescription &d, bool hasEvents, bool addUsage)
{
  m_AddedDrawcall = true;

  DrawcallDescription draw = d;
  draw.eventID = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEventID
                                                 : m_RootEventID;
  draw.drawcallID = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].drawCount
                                                    : m_RootDrawcallID;

  for(int i = 0; i < 8; i++)
    draw.outputs[i] = ResourceId();

  draw.depthOut = ResourceId();

  draw.indexByteWidth = 0;
  draw.topology = Topology::Unknown;

  if(m_LastCmdListID != ResourceId())
  {
    draw.topology = MakePrimitiveTopology(m_BakedCmdListInfo[m_LastCmdListID].state.topo);
    draw.indexByteWidth = m_BakedCmdListInfo[m_LastCmdListID].state.ibuffer.bytewidth;

    vector<ResourceId> rts = m_BakedCmdListInfo[m_LastCmdListID].state.GetRTVIDs();

    for(size_t i = 0; i < ARRAY_COUNT(draw.outputs); i++)
    {
      if(i < rts.size())
        draw.outputs[i] = m_pDevice->GetResourceManager()->GetOriginalID(rts[i]);
      else
        draw.outputs[i] = ResourceId();
    }

    draw.depthOut = m_pDevice->GetResourceManager()->GetOriginalID(
        m_BakedCmdListInfo[m_LastCmdListID].state.GetDSVID());
  }

  if(m_LastCmdListID != ResourceId())
    m_BakedCmdListInfo[m_LastCmdListID].drawCount++;
  else
    m_RootDrawcallID++;

  if(hasEvents)
  {
    vector<APIEvent> &srcEvents = m_LastCmdListID != ResourceId()
                                      ? m_BakedCmdListInfo[m_LastCmdListID].curEvents
                                      : m_RootEvents;

    draw.events = srcEvents;
    srcEvents.clear();
  }

  // should have at least the root drawcall here, push this drawcall
  // onto the back's children list.
  if(!GetDrawcallStack().empty())
  {
    D3D12DrawcallTreeNode node(draw);

    node.resourceUsage.swap(m_BakedCmdListInfo[m_LastCmdListID].resourceUsage);

    if(m_LastCmdListID != ResourceId() && addUsage)
      AddUsage(node);

    node.children.insert(node.children.begin(), draw.children.begin(), draw.children.end());
    GetDrawcallStack().back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost drawcall stack!");
}

void D3D12CommandData::InsertDrawsAndRefreshIDs(ResourceId cmd,
                                                vector<D3D12DrawcallTreeNode> &cmdBufNodes)
{
  // assign new drawcall IDs
  for(size_t i = 0; i < cmdBufNodes.size(); i++)
  {
    if(cmdBufNodes[i].draw.flags & DrawFlags::PopMarker)
    {
      if(GetDrawcallStack().size() > 1)
        GetDrawcallStack().pop_back();

      // Skip - pop marker draws aren't processed otherwise, we just apply them to the drawcall
      // stack.
      continue;
    }

    D3D12DrawcallTreeNode n = cmdBufNodes[i];
    n.draw.eventID += m_RootEventID;
    n.draw.drawcallID += m_RootDrawcallID;

    for(APIEvent &ev : n.draw.events)
    {
      ev.eventID += m_RootEventID;
      m_Events.push_back(ev);
    }

    DrawcallUse use(m_Events.back().fileOffset, n.draw.eventID, cmd, cmdBufNodes[i].draw.eventID);

    // insert in sorted location
    auto drawit = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
    m_DrawcallUses.insert(drawit, use);

    RDCASSERT(n.children.empty());

    for(auto it = n.resourceUsage.begin(); it != n.resourceUsage.end(); ++it)
    {
      EventUsage u = it->second;
      u.eventID += m_RootEventID;
      m_ResourceUses[it->first].push_back(u);
    }

    GetDrawcallStack().back()->children.push_back(n);

    // if this is a push marker too, step down the drawcall stack
    if(cmdBufNodes[i].draw.flags & DrawFlags::PushMarker)
      GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());
  }
}
