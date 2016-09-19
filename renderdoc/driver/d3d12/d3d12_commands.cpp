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

#include <algorithm>
#include "driver/dxgi/dxgi_common.h"
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
                                                     WrappedID3D12Device *device,
                                                     Serialiser *serialiser, LogState &state)
    : RefCounter12(real), m_pDevice(device), m_State(state)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this,
                                                              sizeof(WrappedID3D12CommandQueue));

  m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandQueue), (void **)&m_WrappedDebug.m_pReal);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_pSerialiser = serialiser;

    m_ReplayList = new WrappedID3D12GraphicsCommandList(NULL, m_pDevice, m_pSerialiser, state);

    m_ReplayList->SetCommandData(&m_Cmd);
  }
  else
  {
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, true);

    m_pSerialiser->SetDebugText(true);
  }

  m_pSerialiser->SetUserData(m_pDevice->GetResourceManager());

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_QueueRecord = NULL;

  m_Cmd.m_pSerialiser = m_pSerialiser;
  m_Cmd.m_pDevice = m_pDevice;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_QueueRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_QueueRecord->type = Resource_CommandQueue;
    m_QueueRecord->DataInSerialiser = false;
    m_QueueRecord->SpecialResource = true;
    m_QueueRecord->Length = 0;
  }

  m_pDevice->SoftRef();
}

WrappedID3D12CommandQueue::~WrappedID3D12CommandQueue()
{
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
    string guid = ToStr::Get(riid);
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

FetchAPIEvent WrappedID3D12CommandQueue::GetEvent(uint32_t eventID)
{
  for(size_t i = m_Cmd.m_Events.size() - 1; i > 0; i--)
  {
    if(m_Cmd.m_Events[i].eventID <= eventID)
      return m_Cmd.m_Events[i];
  }

  return m_Cmd.m_Events[0];
}

void WrappedID3D12CommandQueue::ProcessChunk(uint64_t offset, D3D12ChunkType chunk)
{
  m_Cmd.m_CurChunkOffset = offset;
  m_Cmd.m_AddedDrawcall = false;

  switch(chunk)
  {
    case CLOSE_LIST: m_ReplayList->Serialise_Close(); break;
    case RESET_LIST: m_ReplayList->Serialise_Reset(NULL, NULL); break;

    case RESOURCE_BARRIER: m_ReplayList->Serialise_ResourceBarrier(0, NULL); break;

    case BEGIN_EVENT: m_ReplayList->Serialise_BeginEvent(0, NULL, 0); break;
    case SET_MARKER: m_ReplayList->Serialise_SetMarker(0, NULL, 0); break;
    case END_EVENT: m_ReplayList->Serialise_EndEvent(); break;

    case DRAW_INST: m_ReplayList->Serialise_DrawInstanced(0, 0, 0, 0); break;
    case DRAW_INDEXED_INST: m_ReplayList->Serialise_DrawIndexedInstanced(0, 0, 0, 0, 0); break;
    case DISPATCH: m_ReplayList->Serialise_Dispatch(0, 0, 0); break;
    case EXEC_INDIRECT: m_ReplayList->Serialise_ExecuteIndirect(NULL, 0, NULL, 0, NULL, 0); break;

    case COPY_BUFFER: m_ReplayList->Serialise_CopyBufferRegion(NULL, 0, NULL, 0, 0); break;
    case COPY_TEXTURE: m_ReplayList->Serialise_CopyTextureRegion(NULL, 0, 0, 0, NULL, NULL); break;
    case COPY_RESOURCE: m_ReplayList->Serialise_CopyResource(NULL, NULL); break;
    case RESOLVE_SUBRESOURCE:
      m_ReplayList->Serialise_ResolveSubresource(NULL, 0, NULL, 0, DXGI_FORMAT_UNKNOWN);
      break;

    case CLEAR_RTV:
      m_ReplayList->Serialise_ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE(), (FLOAT *)NULL, 0,
                                                    NULL);
      break;
    case CLEAR_DSV:
      m_ReplayList->Serialise_ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE(),
                                                    D3D12_CLEAR_FLAGS(0), 0.0f, 0, 0, NULL);
      break;
    case CLEAR_UAV_INT:
      m_ReplayList->Serialise_ClearUnorderedAccessViewUint(
          D3D12_GPU_DESCRIPTOR_HANDLE(), D3D12_CPU_DESCRIPTOR_HANDLE(), NULL, NULL, 0, NULL);
      break;
    case CLEAR_UAV_FLOAT:
      m_ReplayList->Serialise_ClearUnorderedAccessViewFloat(
          D3D12_GPU_DESCRIPTOR_HANDLE(), D3D12_CPU_DESCRIPTOR_HANDLE(), NULL, NULL, 0, NULL);
      break;
    case DISCARD_RESOURCE: m_ReplayList->Serialise_DiscardResource(NULL, NULL); break;

    case SET_TOPOLOGY:
      m_ReplayList->Serialise_IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
      break;
    case SET_IBUFFER: m_ReplayList->Serialise_IASetIndexBuffer(NULL); break;
    case SET_VBUFFERS: m_ReplayList->Serialise_IASetVertexBuffers(0, 0, NULL); break;
    case SET_VIEWPORTS: m_ReplayList->Serialise_RSSetViewports(0, NULL); break;
    case SET_SCISSORS: m_ReplayList->Serialise_RSSetScissorRects(0, NULL); break;
    case SET_STENCIL: m_ReplayList->Serialise_OMSetStencilRef(0); break;
    case SET_BLENDFACTOR: m_ReplayList->Serialise_OMSetBlendFactor(NULL); break;
    case SET_PIPE: m_ReplayList->Serialise_SetPipelineState(NULL); break;
    case SET_RTVS: m_ReplayList->Serialise_OMSetRenderTargets(0, NULL, FALSE, NULL); break;
    case SET_DESC_HEAPS: m_ReplayList->Serialise_SetDescriptorHeaps(0, NULL); break;

    case SET_GFX_ROOT_SIG: m_ReplayList->Serialise_SetGraphicsRootSignature(NULL); break;
    case SET_GFX_ROOT_TABLE:
      m_ReplayList->Serialise_SetGraphicsRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE());
      break;
    case SET_GFX_ROOT_CONST: m_ReplayList->Serialise_SetGraphicsRoot32BitConstant(0, 0, 0); break;
    case SET_GFX_ROOT_CONSTS:
      m_ReplayList->Serialise_SetGraphicsRoot32BitConstants(0, 0, NULL, 0);
      break;
    case SET_GFX_ROOT_CBV:
      m_ReplayList->Serialise_SetGraphicsRootConstantBufferView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case SET_GFX_ROOT_SRV:
      m_ReplayList->Serialise_SetGraphicsRootShaderResourceView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case SET_GFX_ROOT_UAV:
      m_ReplayList->Serialise_SetGraphicsRootUnorderedAccessView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;

    case SET_COMP_ROOT_SIG: m_ReplayList->Serialise_SetComputeRootSignature(NULL); break;
    case SET_COMP_ROOT_TABLE:
      m_ReplayList->Serialise_SetComputeRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE());
      break;
    case SET_COMP_ROOT_CONST: m_ReplayList->Serialise_SetComputeRoot32BitConstant(0, 0, 0); break;
    case SET_COMP_ROOT_CONSTS:
      m_ReplayList->Serialise_SetComputeRoot32BitConstants(0, 0, NULL, 0);
      break;
    case SET_COMP_ROOT_CBV:
      m_ReplayList->Serialise_SetComputeRootConstantBufferView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case SET_COMP_ROOT_SRV:
      m_ReplayList->Serialise_SetComputeRootShaderResourceView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;
    case SET_COMP_ROOT_UAV:
      m_ReplayList->Serialise_SetComputeRootUnorderedAccessView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;

    case EXECUTE_CMD_LISTS: Serialise_ExecuteCommandLists(0, NULL); break;
    case SIGNAL: Serialise_Signal(NULL, 0); break;
    case CONTEXT_CAPTURE_FOOTER:
    {
      SERIALISE_ELEMENT(ResourceId, bbid, ResourceId());

      bool HasCallstack = false;
      m_pSerialiser->Serialise("HasCallstack", HasCallstack);

      m_BackbufferID = bbid;

      if(HasCallstack)
      {
        size_t numLevels = 0;
        uint64_t *stack = NULL;

        m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

        m_pSerialiser->SetCallstack(stack, numLevels);

        SAFE_DELETE_ARRAY(stack);
      }

      if(m_State == READING)
      {
        m_Cmd.AddEvent(CONTEXT_CAPTURE_FOOTER, "Present()");

        FetchDrawcall draw;
        draw.name = "Present()";
        draw.flags |= eDraw_Present;

        draw.copyDestination = bbid;

        m_Cmd.AddDrawcall(draw, true);
      }
      break;
    }
    default:
      // ignore system chunks
      if(chunk == INITIAL_CONTENTS)
        GetResourceManager()->Serialise_InitialState(ResourceId(), NULL);
      else if(chunk < FIRST_CHUNK_ID)
        m_pSerialiser->SkipCurrentChunk();
      else
        RDCERR("Unexpected non-device chunk %d at offset %llu", chunk, offset);
      break;
  }

  m_pSerialiser->PopContext(chunk);

  if(m_State == READING && chunk == SET_MARKER)
  {
    // no push/pop necessary
  }
  else if(m_State == READING && (chunk == BEGIN_EVENT || chunk == END_EVENT))
  {
    // don't add these events - they will be handled when inserted in-line into queue submit
  }
  else if(m_State == READING)
  {
    if(!m_Cmd.m_AddedDrawcall)
      m_Cmd.AddEvent(chunk, m_pSerialiser->GetDebugStr());
  }

  m_Cmd.m_AddedDrawcall = false;
}

void WrappedID3D12CommandQueue::ReplayLog(LogState readType, uint32_t startEventID,
                                          uint32_t endEventID, bool partial)
{
  m_State = readType;

  D3D12ChunkType header = (D3D12ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
  RDCASSERTEQUAL(header, CONTEXT_CAPTURE_HEADER);

  m_pDevice->Serialise_BeginCaptureFrame(!partial);

  if(readType == READING)
  {
    GetResourceManager()->ApplyInitialContents();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }

  m_pSerialiser->PopContext(header);

  m_Cmd.m_RootEvents.clear();

  if(m_State == EXECUTING)
  {
    FetchAPIEvent ev = GetEvent(startEventID);
    m_Cmd.m_RootEventID = ev.eventID;

    // if not partial, we need to be sure to replay
    // past the command list records, so can't
    // skip to the file offset of the first event
    if(partial)
      m_pSerialiser->SetOffset(ev.fileOffset);

    m_Cmd.m_FirstEventID = startEventID;
    m_Cmd.m_LastEventID = endEventID;
  }
  else if(m_State == READING)
  {
    m_Cmd.m_RootEventID = 1;
    m_Cmd.m_RootDrawcallID = 1;
    m_Cmd.m_FirstEventID = 0;
    m_Cmd.m_LastEventID = ~0U;
  }

  for(;;)
  {
    if(m_State == EXECUTING && m_Cmd.m_RootEventID > endEventID)
    {
      // we can just break out if we've done all the events desired.
      // note that the command list events aren't 'real' and we just blaze through them
      break;
    }

    uint64_t offset = m_pSerialiser->GetOffset();

    D3D12ChunkType context = (D3D12ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

    m_Cmd.m_LastCmdListID = ResourceId();

    ProcessChunk(offset, context);

    RenderDoc::Inst().SetProgress(FileInitialRead, float(offset) / float(m_pSerialiser->GetSize()));

    // for now just abort after capture scope. Really we'd need to support multiple frames
    // but for now this will do.
    if(context == CONTEXT_CAPTURE_FOOTER)
      break;

    // break out if we were only executing one event
    if(m_State == EXECUTING && startEventID == endEventID)
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
        m_pSerialiser->SetOffset(GetEvent(m_Cmd.m_RootEventID).fileOffset);
    }
    else
    {
      m_Cmd.m_BakedCmdListInfo[m_Cmd.m_LastCmdListID].curEventID++;
    }
  }

  if(m_State == READING)
  {
    struct SortEID
    {
      bool operator()(const FetchAPIEvent &a, const FetchAPIEvent &b)
      {
        return a.eventID < b.eventID;
      }
    };

    std::sort(m_Cmd.m_Events.begin(), m_Cmd.m_Events.end(), SortEID());
  }

  for(int p = 0; p < D3D12CommandData::ePartialNum; p++)
    SAFE_RELEASE(m_Cmd.m_Partial[p].resultPartialCmdList);

  for(auto it = m_Cmd.m_RerecordCmds.begin(); it != m_Cmd.m_RerecordCmds.end(); ++it)
    SAFE_RELEASE(it->second);

  m_Cmd.m_RerecordCmds.clear();

  m_State = READING;
}

WrappedID3D12GraphicsCommandList::WrappedID3D12GraphicsCommandList(ID3D12GraphicsCommandList *real,
                                                                   WrappedID3D12Device *device,
                                                                   Serialiser *serialiser,
                                                                   LogState &state)
    : RefCounter12(real), m_pDevice(device), m_State(state)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(
        this, sizeof(WrappedID3D12GraphicsCommandList));

  if(m_pReal)
    m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandList), (void **)&m_WrappedDebug.m_pReal);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_pSerialiser = serialiser;
  }
  else
  {
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, true);

    m_pSerialiser->SetDebugText(true);
  }

  m_pSerialiser->SetUserData(m_pDevice->GetResourceManager());

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  RDCEraseEl(m_Init);

  m_ListRecord = NULL;
  m_Cmd = NULL;

  m_CurGfxRootSig = NULL;

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
  if(m_pReal)
    m_pDevice->GetResourceManager()->RemoveWrapper(m_pReal);

  m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());

  SAFE_RELEASE(m_WrappedDebug.m_pReal);
  SAFE_RELEASE(m_pReal);
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
    string guid = ToStr::Get(riid);
    RDCWARN("Querying ID3D12GraphicsCommandList for interface: %s", guid.c_str());
  }

  return RefCounter12::QueryInterface(riid, ppvObject);
}

D3D12CommandData::D3D12CommandData()
{
  m_CurChunkOffset = 0;

  m_RootEventID = 1;
  m_RootDrawcallID = 1;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_pDevice = NULL;
  m_pSerialiser = NULL;

  m_DrawcallCallback = NULL;

  m_AddedDrawcall = false;

  m_RootDrawcallStack.push_back(&m_ParentDrawcall);
}

uint32_t D3D12CommandData::HandlePreCallback(ID3D12GraphicsCommandList *list, bool dispatch,
                                             uint32_t multiDrawOffset)
{
  if(!m_DrawcallCallback)
    return 0;

  // look up the EID this drawcall came from
  DrawcallUse use(m_CurChunkOffset, 0);
  auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
  RDCASSERT(it != m_DrawcallUses.end());

  uint32_t eventID = it->eventID;

  RDCASSERT(eventID != 0);

  // handle all aliases of this drawcall as long as it's not a multidraw
  const FetchDrawcall *draw = m_pDevice->GetDrawcall(eventID);

  if(draw == NULL || (draw->flags & eDraw_MultiDraw) == 0)
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

    RDCASSERT(it != m_RerecordCmds.end());

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

void D3D12CommandData::AddEvent(D3D12ChunkType type, string description)
{
  FetchAPIEvent apievent;

  apievent.context = ResourceId();
  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEventID
                                                     : m_RootEventID;

  apievent.eventDesc = description;

  Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
  if(stack)
  {
    create_array(apievent.callstack, stack->NumLevels());
    memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t) * stack->NumLevels());
  }

  D3D12NOTIMP("debug messages");
  vector<DebugMessage> m_EventMessages;

  for(size_t i = 0; i < m_EventMessages.size(); i++)
    m_EventMessages[i].eventID = apievent.eventID;

  if(m_LastCmdListID != ResourceId())
  {
    m_BakedCmdListInfo[m_LastCmdListID].curEvents.push_back(apievent);

    vector<DebugMessage> &msgs = m_BakedCmdListInfo[m_LastCmdListID].debugMessages;

    msgs.insert(msgs.end(), m_EventMessages.begin(), m_EventMessages.end());
  }
  else
  {
    m_RootEvents.push_back(apievent);
    m_Events.push_back(apievent);

    D3D12NOTIMP("debug messages");
    // m_DebugMessages.insert(m_DebugMessages.end(), m_EventMessages.begin(),
    // m_EventMessages.end());
  }

  m_EventMessages.clear();
}

void D3D12CommandData::AddDrawcall(const FetchDrawcall &d, bool hasEvents)
{
  m_AddedDrawcall = true;

  FetchDrawcall draw = d;
  draw.eventID = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].curEventID
                                                 : m_RootEventID;
  draw.drawcallID = m_LastCmdListID != ResourceId() ? m_BakedCmdListInfo[m_LastCmdListID].drawCount
                                                    : m_RootDrawcallID;

  for(int i = 0; i < 8; i++)
    draw.outputs[i] = ResourceId();

  draw.depthOut = ResourceId();

  draw.indexByteWidth = 0;
  draw.topology = eTopology_Unknown;

  if(m_LastCmdListID != ResourceId())
  {
    draw.topology = MakePrimitiveTopology(m_BakedCmdListInfo[m_LastCmdListID].state.topo);
    draw.indexByteWidth = m_BakedCmdListInfo[m_LastCmdListID].state.idxWidth;

    memcpy(draw.outputs, m_BakedCmdListInfo[m_LastCmdListID].state.rts, sizeof(draw.outputs));
    draw.depthOut = m_BakedCmdListInfo[m_LastCmdListID].state.dsv;
  }

  if(m_LastCmdListID != ResourceId())
    m_BakedCmdListInfo[m_LastCmdListID].drawCount++;
  else
    m_RootDrawcallID++;

  if(hasEvents)
  {
    vector<FetchAPIEvent> &srcEvents = m_LastCmdListID != ResourceId()
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

    D3D12NOTIMP("event usage");

    node.children.insert(node.children.begin(), draw.children.elems,
                         draw.children.elems + draw.children.count);
    GetDrawcallStack().back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost drawcall stack!");
}

void D3D12CommandData::InsertDrawsAndRefreshIDs(vector<D3D12DrawcallTreeNode> &cmdBufNodes)
{
  // assign new drawcall IDs
  for(size_t i = 0; i < cmdBufNodes.size(); i++)
  {
    if(cmdBufNodes[i].draw.flags & eDraw_PopMarker)
    {
      RDCASSERT(GetDrawcallStack().size() > 1);
      if(GetDrawcallStack().size() > 1)
        GetDrawcallStack().pop_back();

      // Skip - pop marker draws aren't processed otherwise, we just apply them to the drawcall
      // stack.
      continue;
    }

    D3D12DrawcallTreeNode n = cmdBufNodes[i];
    n.draw.eventID += m_RootEventID;
    n.draw.drawcallID += m_RootDrawcallID;

    for(int32_t e = 0; e < n.draw.events.count; e++)
    {
      n.draw.events[e].eventID += m_RootEventID;
      m_Events.push_back(n.draw.events[e]);
    }

    DrawcallUse use(m_Events.back().fileOffset, n.draw.eventID);

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
    if(cmdBufNodes[i].draw.flags & eDraw_PushMarker)
      GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());
  }
}
