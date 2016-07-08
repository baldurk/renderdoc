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

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandQueue::AddRef()
{
  m_pQueue->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandQueue::Release()
{
  m_pQueue->Release();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandList::AddRef()
{
  m_pList->AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE DummyID3D12DebugCommandList::Release()
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

  m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandQueue), (void **)&m_DummyDebug.m_pReal);

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_pSerialiser = serialiser;

    m_ReplayList = new WrappedID3D12GraphicsCommandList(NULL, m_pDevice, m_pSerialiser, state);
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

  m_RootEventID = 1;
  m_RootDrawcallID = 1;
  m_FirstEventID = 0;
  m_LastEventID = ~0U;

  m_LastCmdListID = ResourceId();

  m_PartialReplayData.resultPartialCmdList = NULL;
  m_PartialReplayData.outsideCmdList = NULL;
  m_PartialReplayData.partialParent = ResourceId();
  m_PartialReplayData.baseEvent = 0;

  m_DrawcallStack.push_back(&m_ParentDrawcall);

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
}

FetchAPIEvent WrappedID3D12CommandQueue::GetEvent(uint32_t eventID)
{
  for(size_t i = m_Events.size() - 1; i > 0; i--)
  {
    if(m_Events[i].eventID <= eventID)
      return m_Events[i];
  }

  return m_Events[0];
}

void WrappedID3D12CommandQueue::ProcessChunk(uint64_t offset, D3D12ChunkType chunk)
{
  m_CurChunkOffset = offset;

  m_AddedDrawcall = false;

  switch(chunk)
  {
    case CLOSE_LIST: m_ReplayList->Serialise_Close(); break;
    case RESET_LIST: m_ReplayList->Serialise_Reset(NULL, NULL); break;

    case RESOURCE_BARRIER: m_ReplayList->Serialise_ResourceBarrier(0, NULL); break;

    case DRAW_INDEXED_INST: m_ReplayList->Serialise_DrawIndexedInstanced(0, 0, 0, 0, 0); break;
    case COPY_BUFFER: m_ReplayList->Serialise_CopyBufferRegion(NULL, 0, NULL, 0, 0); break;

    case CLEAR_RTV:
      m_ReplayList->Serialise_ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE(), (FLOAT *)NULL, 0,
                                                    NULL);
      break;

    case SET_TOPOLOGY:
      m_ReplayList->Serialise_IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
      break;
    case SET_IBUFFER: m_ReplayList->Serialise_IASetIndexBuffer(NULL); break;
    case SET_VBUFFERS: m_ReplayList->Serialise_IASetVertexBuffers(0, 0, NULL); break;
    case SET_VIEWPORTS: m_ReplayList->Serialise_RSSetViewports(0, NULL); break;
    case SET_SCISSORS: m_ReplayList->Serialise_RSSetScissorRects(0, NULL); break;
    case SET_PIPE: m_ReplayList->Serialise_SetPipelineState(NULL); break;
    case SET_RTVS: m_ReplayList->Serialise_OMSetRenderTargets(0, NULL, FALSE, NULL); break;
    case SET_GFX_ROOT_SIG: m_ReplayList->Serialise_SetGraphicsRootSignature(NULL); break;
    case SET_GFX_ROOT_CBV:
      m_ReplayList->Serialise_SetGraphicsRootConstantBufferView(0, D3D12_GPU_VIRTUAL_ADDRESS());
      break;

    case EXECUTE_CMD_LISTS: Serialise_ExecuteCommandLists(0, NULL); break;
    case SIGNAL: Serialise_Signal(NULL, 0); break;
    case CONTEXT_CAPTURE_FOOTER:
    {
      SERIALISE_ELEMENT(ResourceId, bbid, ResourceId());

      bool HasCallstack = false;
      m_pSerialiser->Serialise("HasCallstack", HasCallstack);

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
        AddEvent(CONTEXT_CAPTURE_FOOTER, "Present()");

        FetchDrawcall draw;
        draw.name = "Present()";
        draw.flags |= eDraw_Present;

        draw.copyDestination = bbid;

        AddDrawcall(draw, true);
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
  else if(m_State == READING && (chunk == PUSH_EVENT || chunk == POP_EVENT))
  {
    // don't add these events - they will be handled when inserted in-line into queue submit
  }
  else if(m_State == READING)
  {
    if(!m_AddedDrawcall)
      AddEvent(chunk, m_pSerialiser->GetDebugStr());
  }

  m_AddedDrawcall = false;
}

void WrappedID3D12CommandQueue::ReplayLog(LogState readType, uint32_t startEventID,
                                          uint32_t endEventID, bool partial)
{
  m_State = readType;

  D3D12ChunkType header = (D3D12ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
  RDCASSERTEQUAL(header, CONTEXT_CAPTURE_HEADER);

  m_pDevice->Serialise_BeginCaptureFrame(!partial);

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists();

  m_pSerialiser->PopContext(header);

  m_RootEvents.clear();

  if(m_State == EXECUTING)
  {
    FetchAPIEvent ev = GetEvent(startEventID);
    m_RootEventID = ev.eventID;

    // if not partial, we need to be sure to replay
    // past the command buffer records, so can't
    // skip to the file offset of the first event
    if(partial)
      m_pSerialiser->SetOffset(ev.fileOffset);

    m_FirstEventID = startEventID;
    m_LastEventID = endEventID;
  }
  else if(m_State == READING)
  {
    m_RootEventID = 1;
    m_RootDrawcallID = 1;
    m_FirstEventID = 0;
    m_LastEventID = ~0U;
  }

  for(;;)
  {
    if(m_State == EXECUTING && m_RootEventID > endEventID)
    {
      // we can just break out if we've done all the events desired.
      // note that the command buffer events aren't 'real' and we just blaze through them
      break;
    }

    uint64_t offset = m_pSerialiser->GetOffset();

    D3D12ChunkType context = (D3D12ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

    m_LastCmdListID = ResourceId();

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
    // replay inside a command buffer (if we crossed command buffer
    // boundaries, the event IDs would no longer match up).
    if(m_LastCmdListID == ResourceId() || startEventID > 1)
    {
      m_RootEventID++;

      if(startEventID > 1)
        m_pSerialiser->SetOffset(GetEvent(m_RootEventID).fileOffset);
    }
    else
    {
      m_BakedCmdListInfo[m_LastCmdListID].curEventID++;
    }
  }

  m_pDevice->ExecuteLists();
  m_pDevice->FlushLists(true);

  if(m_State == READING)
  {
    struct SortEID
    {
      bool operator()(const FetchAPIEvent &a, const FetchAPIEvent &b)
      {
        return a.eventID < b.eventID;
      }
    };

    std::sort(m_Events.begin(), m_Events.end(), SortEID());
  }

  SAFE_RELEASE(m_PartialReplayData.resultPartialCmdList);

  for(auto it = m_RerecordCmds.begin(); it != m_RerecordCmds.end(); ++it)
    SAFE_RELEASE(it->second);

  m_RerecordCmds.clear();

  m_State = READING;
}

void WrappedID3D12CommandQueue::AddDrawcall(const FetchDrawcall &d, bool hasEvents)
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
    // TODO fill from m_BakedCmdListInfo[m_LastCmdListID].state
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

    // TODO add usage

    node.children.insert(node.children.begin(), draw.children.elems,
                         draw.children.elems + draw.children.count);
    GetDrawcallStack().back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost drawcall stack!");
}

void WrappedID3D12CommandQueue::AddEvent(D3D12ChunkType type, string description)
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

  // TODO have real m_EventMessages
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

    // TODO m_DebugMessages.insert(m_DebugMessages.end(), m_EventMessages.begin(),
    // m_EventMessages.end());
  }

  m_EventMessages.clear();
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
    m_pReal->QueryInterface(__uuidof(ID3D12DebugCommandList), (void **)&m_DummyDebug.m_pReal);

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

  m_ListRecord = NULL;

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

  m_pDevice->SoftRef();
}

WrappedID3D12GraphicsCommandList::~WrappedID3D12GraphicsCommandList()
{
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

void WrappedID3D12GraphicsCommandList::AddDrawcall(const FetchDrawcall &d, bool hasEvents)
{
  m_pDevice->GetQueue()->AddDrawcall(d, hasEvents);
}

void WrappedID3D12GraphicsCommandList::AddEvent(D3D12ChunkType type, string description)
{
  m_pDevice->GetQueue()->AddEvent(type, description);
}
