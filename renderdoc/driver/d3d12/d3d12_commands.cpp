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

#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_pSerialiser = serialiser;

    m_ReplayList = new WrappedID3D12GraphicsCommandList(NULL, m_pDevice, m_pSerialiser, state);

    m_ReplayList->SetCommandData(&m_Cmd);
  }
  else
  {
    // make serialisers smaller by default since we create a lot of these internally for small
    // commands.
    // User lists will quickly grow to a steady-state over time.
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser, 256);

    m_pSerialiser->SetDebugText(debugSerialiser);
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

APIEvent WrappedID3D12CommandQueue::GetEvent(uint32_t eventID)
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

    case MAP_DATA_WRITE:
      m_pDevice->Serialise_MapDataWrite(m_pSerialiser, NULL, 0, NULL, D3D12_RANGE());
      break;
    case WRITE_TO_SUB:
      m_pDevice->Serialise_WriteToSubresource(m_pSerialiser, NULL, 0, NULL, NULL, 0, 0);
      break;

    case BEGIN_QUERY:
      m_ReplayList->Serialise_BeginQuery(NULL, D3D12_QUERY_TYPE_OCCLUSION, 0);
      break;
    case END_QUERY: m_ReplayList->Serialise_EndQuery(NULL, D3D12_QUERY_TYPE_OCCLUSION, 0); break;
    case RESOLVE_QUERY:
      m_ReplayList->Serialise_ResolveQueryData(NULL, D3D12_QUERY_TYPE_OCCLUSION, 0, 0, NULL, 0);
      break;
    case SET_PREDICATION:
      m_ReplayList->Serialise_SetPredication(NULL, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
      break;

    case BEGIN_EVENT: m_ReplayList->Serialise_BeginEvent(0, NULL, 0); break;
    case SET_MARKER: m_ReplayList->Serialise_SetMarker(0, NULL, 0); break;
    case END_EVENT: m_ReplayList->Serialise_EndEvent(); break;

    case DRAW_INST: m_ReplayList->Serialise_DrawInstanced(0, 0, 0, 0); break;
    case DRAW_INDEXED_INST: m_ReplayList->Serialise_DrawIndexedInstanced(0, 0, 0, 0, 0); break;
    case DISPATCH: m_ReplayList->Serialise_Dispatch(0, 0, 0); break;
    case EXEC_INDIRECT: m_ReplayList->Serialise_ExecuteIndirect(NULL, 0, NULL, 0, NULL, 0); break;
    case EXEC_BUNDLE: m_ReplayList->Serialise_ExecuteBundle(NULL); break;

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
    case SET_SOTARGETS: m_ReplayList->Serialise_SOSetTargets(0, 0, NULL); break;
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

    case DYN_DESC_WRITE:
      m_pDevice->Serialise_DynamicDescriptorWrite(m_pDevice->GetMainSerialiser(), NULL);
      break;
    case DYN_DESC_COPIES:
      m_pDevice->Serialise_DynamicDescriptorCopies(m_pDevice->GetMainSerialiser(), NULL);
      break;

    case UPDATE_TILE_MAPPINGS:
      Serialise_UpdateTileMappings(NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL,
                                   D3D12_TILE_MAPPING_FLAG_NONE);
      break;
    case COPY_TILE_MAPPINGS:
      Serialise_CopyTileMappings(NULL, NULL, NULL, NULL, NULL, D3D12_TILE_MAPPING_FLAG_NONE);
      break;
    case EXECUTE_CMD_LISTS: Serialise_ExecuteCommandLists(0, NULL); break;
    case SIGNAL: Serialise_Signal(NULL, 0); break;
    case WAIT: Serialise_Wait(NULL, 0); break;
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
        m_Cmd.AddEvent("Present()");

        DrawcallDescription draw;
        draw.name = "Present()";
        draw.flags |= DrawFlags::Present;

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
      m_Cmd.AddEvent(m_pSerialiser->GetDebugStr());
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
    m_pDevice->ApplyInitialContents();

    m_pDevice->ExecuteLists();
    m_pDevice->FlushLists();
  }

  m_pSerialiser->PopContext(header);

  m_Cmd.m_RootEvents.clear();

  if(m_State == EXECUTING)
  {
    APIEvent ev = GetEvent(startEventID);
    m_Cmd.m_RootEventID = ev.eventID;

    // if not partial, we need to be sure to replay
    // past the command list records, so can't
    // skip to the file offset of the first event
    if(partial)
    {
      m_pSerialiser->SetOffset(ev.fileOffset);

      D3D12CommandData::DrawcallUse use(ev.fileOffset, 0);
      auto it = std::lower_bound(m_Cmd.m_DrawcallUses.begin(), m_Cmd.m_DrawcallUses.end(), use);

      if(it != m_Cmd.m_DrawcallUses.end())
      {
        BakedCmdListInfo &cmdInfo =
            m_Cmd.m_BakedCmdListInfo[m_Cmd.m_BakedCmdListInfo[it->cmdList].parentList];
        cmdInfo.curEventID = it->relativeEID;
      }
    }

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
      bool operator()(const APIEvent &a, const APIEvent &b) { return a.eventID < b.eventID; }
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

#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_pSerialiser = serialiser;
  }
  else
  {
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);

    m_pSerialiser->SetDebugText(debugSerialiser);
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

  if(m_ListRecord && m_ListRecord->bakedCommands)
    m_ListRecord->bakedCommands->Delete(m_pDevice->GetResourceManager());

  m_pDevice->GetResourceManager()->ReleaseCurrentResource(GetResourceID());

  SAFE_RELEASE(m_WrappedDebug.m_pReal);
  SAFE_RELEASE(m_pReal);
}

bool WrappedID3D12GraphicsCommandList::ValidateRootGPUVA(ResourceId buffer)
{
  if(!GetResourceManager()->HasLiveResource(buffer))
  {
    // abort, we don't have this buffer. Print errors while reading
    if(m_State == READING)
    {
      if(buffer != ResourceId())
      {
        RDCERR("Don't have live buffer for %llu", buffer);
      }
      else
      {
        m_pDevice->AddDebugMessage(MessageCategory::Resource_Manipulation, MessageSeverity::Medium,
                                   MessageSource::IncorrectAPIUse,
                                   "Binding 0 as a GPU Virtual Address in a root constant is "
                                   "invalid. This call will be dropped during replay.");
      }
    }

    return true;
  }

  return false;
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

      for(int32_t e = 0; e < draws[i].draw.events.count; e++)
        draws[i].draw.events[e].eventID -= shiftEID;
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
      RDCERR("Failed to create indirect buffer, HRESULT: 0x%08x", hr);

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
  RDCASSERT(it != m_DrawcallUses.end());

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

void D3D12CommandData::AddEvent(string description)
{
  APIEvent apievent;

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

    node.children.insert(node.children.begin(), draw.children.elems,
                         draw.children.elems + draw.children.count);
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

    for(int32_t e = 0; e < n.draw.events.count; e++)
    {
      n.draw.events[e].eventID += m_RootEventID;
      m_Events.push_back(n.draw.events[e]);
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
