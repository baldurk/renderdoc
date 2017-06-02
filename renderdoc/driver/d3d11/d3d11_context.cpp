/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_manager.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"
#include "replay/type_helpers.h"
#include "serialise/string_utils.h"

WRAPPED_POOL_INST(WrappedID3D11DeviceContext);
WRAPPED_POOL_INST(WrappedID3D11CommandList);

INT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::BeginEvent(LPCWSTR Name)
{
  if(m_Context)
    return m_Context->PushEvent(0, Name);

  return -1;
}

INT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::EndEvent()
{
  if(m_Context)
    return m_Context->PopEvent();

  return -1;
}

void STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::SetMarker(LPCWSTR Name)
{
  if(m_Context)
    return m_Context->SetMarker(0, Name);
}

HRESULT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::QueryInterface(REFIID riid,
                                                                           void **ppvObject)
{
  if(riid == __uuidof(ID3DUserDefinedAnnotation))
  {
    *ppvObject = (void *)(ID3DUserDefinedAnnotation *)this;
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

extern uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
extern uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

WrappedID3D11DeviceContext::WrappedID3D11DeviceContext(WrappedID3D11Device *realDevice,
                                                       ID3D11DeviceContext *context, Serialiser *ser)
    : RefCounter(context), m_pDevice(realDevice), m_pRealContext(context)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this,
                                                              sizeof(WrappedID3D11DeviceContext));

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
  {
    NullCBOffsets[i] = 0;
    NullCBCounts[i] = 4096;
  }

  D3D11_FEATURE_DATA_D3D11_OPTIONS features;
  RDCEraseEl(features);
  HRESULT hr =
      m_pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &features, sizeof(features));

  m_SetCBuffer1 = false;
  if(SUCCEEDED(hr))
    m_SetCBuffer1 = features.ConstantBufferOffsetting == TRUE;

  m_pRealContext1 = NULL;
  m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&m_pRealContext1);

  m_pRealContext2 = NULL;
  m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext2), (void **)&m_pRealContext2);

  m_pRealContext3 = NULL;
  m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext3), (void **)&m_pRealContext3);

#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  m_NeedUpdateSubWorkaround = false;
  {
    D3D11_FEATURE_DATA_THREADING caps = {FALSE, FALSE};

    hr = m_pDevice->CheckFeatureSupport(D3D11_FEATURE_THREADING, &caps, sizeof(caps));
    if(SUCCEEDED(hr) && !caps.DriverCommandLists)
      m_NeedUpdateSubWorkaround = true;
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = READING;
    m_pSerialiser = ser;
  }
  else
  {
    m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
    m_State = WRITING_IDLE;

    m_pSerialiser->SetDebugText(true);
  }

  m_OwnSerialiser = false;

  // create a temporary and grab its resource ID
  m_ResourceID = ResourceIDGen::GetNewUniqueID();

  m_ContextRecord = NULL;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_ContextRecord = m_pDevice->GetResourceManager()->AddResourceRecord(m_ResourceID);
    m_ContextRecord->DataInSerialiser = false;
    m_ContextRecord->SpecialResource = true;
    m_ContextRecord->Length = 0;
    m_ContextRecord->NumSubResources = 0;
    m_ContextRecord->SubResources = NULL;
    m_ContextRecord->ignoreSerialise = true;
  }

  m_SuccessfulCapture = true;
  m_FailureReason = CaptureSucceeded;
  m_EmptyCommandList = true;

  m_PresentChunk = false;

  m_DrawcallStack.push_back(&m_ParentDrawcall);

  m_CurEventID = 1;
  m_CurDrawcallID = 1;

  m_MarkerIndentLevel = 0;
  m_UserAnnotation.SetContext(this);

  m_CurrentPipelineState = new D3D11RenderState((Serialiser *)NULL);
  m_DeferredSavedState = NULL;
  m_DoStateVerify = m_State >= WRITING;

  if(context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
  {
    m_CurrentPipelineState->SetImmediatePipeline(m_pDevice);
  }
  else
  {
    m_CurrentPipelineState->SetDevice(m_pDevice);
    m_pDevice->SoftRef();

    if(m_State >= WRITING && RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists)
      m_State = WRITING_CAPFRAME;
  }

  ReplayFakeContext(ResourceId());
}

WrappedID3D11DeviceContext::~WrappedID3D11DeviceContext()
{
  if(m_ContextRecord)
    m_ContextRecord->Delete(m_pDevice->GetResourceManager());

  if(m_pRealContext->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
    m_pDevice->RemoveDeferredContext(this);

  for(auto it = m_StreamOutCounters.begin(); it != m_StreamOutCounters.end(); ++it)
  {
    SAFE_RELEASE(it->second.query);
  }

  if(m_State >= WRITING || m_OwnSerialiser)
  {
    SAFE_DELETE(m_pSerialiser);
  }

  SAFE_RELEASE(m_pRealContext1);
  SAFE_RELEASE(m_pRealContext2);
  SAFE_RELEASE(m_pRealContext3);

  SAFE_DELETE(m_DeferredSavedState);

  SAFE_DELETE(m_CurrentPipelineState);
  SAFE_RELEASE(m_pRealContext);

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void WrappedID3D11DeviceContext::GetDevice(ID3D11Device **ppDevice)
{
  *ppDevice = (ID3D11Device *)m_pDevice;
  (*ppDevice)->AddRef();
}

const char *WrappedID3D11DeviceContext::GetChunkName(D3D11ChunkType idx)
{
  return m_pDevice->GetChunkName(idx);
}

bool WrappedID3D11DeviceContext::Serialise_BeginCaptureFrame(bool applyInitialState)
{
  D3D11RenderState state(m_pSerialiser);

  if(m_State >= WRITING)
  {
    state.CopyState(*m_CurrentPipelineState);

    state.SetSerialiser(m_pSerialiser);

    state.MarkReferenced(this, true);
  }

  state.Serialise(m_State, m_pDevice);

  if(m_State <= EXECUTING && applyInitialState)
  {
    m_DoStateVerify = false;
    {
      m_CurrentPipelineState->CopyState(state);
      m_CurrentPipelineState->SetDevice(m_pDevice);
      state.ApplyState(this);
    }
    m_DoStateVerify = true;
    VerifyState();
  }

  // stream-out hidden counters need to be saved, in case their results are used
  // for a DrawAuto() somewhere. Each buffer used as a stream-out target has a hidden
  // counter saved with it that stores the number of primitives written, which is then
  // used for a DrawAuto(). If the stream-out happens in frame we don't need to worry,
  // but if it references a buffer from before we need to have that counter available
  // on replay to 'fake' the DrawAuto() just as a Draw() with known values
  if(m_State >= WRITING)
  {
    // this may break API guarantees, but we need to fetch the hidden counters
    // so we need to restart any queries for currently set SO targets.
    // Potentially to be more correct we could defer fetching the results of queries
    // that are still running until they get detached (as they must be detached
    // before being used for any DrawAuto calls - if we're in CAPFRAME we could
    // serialise the data then. If they're never detached, we don't need the results)

    bool restart[4] = {false};

    for(UINT b = 0; b < 4; b++)
    {
      ID3D11Buffer *buf = m_CurrentPipelineState->SO.Buffers[b];

      if(buf)
      {
        ResourceId id = GetIDForResource(buf);

        m_pRealContext->End(m_StreamOutCounters[id].query);
        m_StreamOutCounters[id].running = false;

        restart[b] = true;
      }
    }

    D3D11_QUERY_DATA_SO_STATISTICS numPrims;

    // readback all known counters
    SERIALISE_ELEMENT(uint32_t, numStreamOutCounters, (uint32_t)m_StreamOutCounters.size());
    for(auto it = m_StreamOutCounters.begin(); it != m_StreamOutCounters.end(); ++it)
    {
      SERIALISE_ELEMENT(ResourceId, id, it->first);

      RDCEraseEl(numPrims);

      HRESULT hr = S_FALSE;

      do
      {
        hr = m_pRealContext->GetData(it->second.query, &numPrims,
                                     sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0);
      } while(hr == S_FALSE);

      if(hr != S_OK)
      {
        numPrims.NumPrimitivesWritten = 0;
        RDCERR("Couldn't retrieve hidden buffer counter for streamout on buffer %llu", id);
      }

      SERIALISE_ELEMENT(uint64_t, hiddenCounter, (uint64_t)numPrims.NumPrimitivesWritten);
    }

    // restart any counters we were forced to stop
    for(UINT b = 0; b < 4; b++)
    {
      ID3D11Buffer *buf = m_CurrentPipelineState->SO.Buffers[b];

      if(buf && restart[b])
      {
        ResourceId id = GetIDForResource(buf);

        // release any previous query as the hidden counter is overwritten
        SAFE_RELEASE(m_StreamOutCounters[id].query);

        D3D11_QUERY queryTypes[] = {
            D3D11_QUERY_SO_STATISTICS_STREAM0, D3D11_QUERY_SO_STATISTICS_STREAM1,
            D3D11_QUERY_SO_STATISTICS_STREAM2, D3D11_QUERY_SO_STATISTICS_STREAM3,
        };

        D3D11_QUERY_DESC qdesc;
        qdesc.MiscFlags = 0;
        qdesc.Query = queryTypes[b];

        m_pDevice->GetReal()->CreateQuery(&qdesc, &m_StreamOutCounters[id].query);

        m_pRealContext->Begin(m_StreamOutCounters[id].query);
        m_StreamOutCounters[id].running = true;
      }
    }
  }
  // version 5 added this set of data, we can assume for older logs there's just no counters
  else if(m_pDevice->GetLogVersion() >= 0x000005)
  {
    // read in the known stream-out counters at the start of the frame.
    // any stream-out that happens in the captured frame will be replayed
    // and those counters will override this value when it comes to a
    // DrawAuto()
    SERIALISE_ELEMENT(uint32_t, numStreamOutCounters, 0);
    for(uint32_t i = 0; i < numStreamOutCounters; i++)
    {
      SERIALISE_ELEMENT(ResourceId, id, ResourceId());
      SERIALISE_ELEMENT(uint64_t, hiddenCounter, 0);

      if(m_pDevice->GetResourceManager()->HasLiveResource(id))
        m_StreamOutCounters[m_pDevice->GetResourceManager()->GetLiveID(id)].numPrims = hiddenCounter;
    }
  }

  return true;
}

void WrappedID3D11DeviceContext::MarkResourceReferenced(ResourceId id, FrameRefType refType)
{
  if(m_pRealContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
  {
    m_pDevice->GetResourceManager()->MarkResourceFrameReferenced(id, refType);
  }
  else
  {
    bool newRef = m_ContextRecord->MarkResourceFrameReferenced(id, refType);

    // we need to keep this resource alive so that we can insert its record on capture
    // if this command list gets executed.
    if(newRef)
    {
      D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(id);
      if(record)
      {
        record->AddRef();
        m_DeferredReferences.insert(id);
      }
    }
  }
}

void WrappedID3D11DeviceContext::MarkDirtyResource(ResourceId id)
{
  if(m_pRealContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
  {
    m_pDevice->GetResourceManager()->MarkDirtyResource(id);
  }
  else
  {
    m_DeferredDirty.insert(id);
  }
}

void WrappedID3D11DeviceContext::VerifyState()
{
#if 0
	if(m_DoStateVerify)
	{
		D3D11RenderState state(this);
	}
#endif
}

void WrappedID3D11DeviceContext::BeginCaptureFrame()
{
  SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);
  m_pSerialiser->Serialise("context", m_ResourceID);

  Serialise_BeginCaptureFrame(false);

  {
    SCOPED_LOCK(m_AnnotLock);

    m_AnnotationQueue.clear();
  }

  m_ContextRecord->AddChunk(scope.Get(), 1);
}

void WrappedID3D11DeviceContext::AttemptCapture()
{
  m_State = WRITING_CAPFRAME;

  m_FailureReason = CaptureSucceeded;

  // deferred contexts are initially NOT successful unless empty. That's because we don't have the
  // serialised
  // contents of whatever is in them up until now (could be anything).
  // Only after they have been through a Finish() and then in CAPFRAME mode are they considered
  // successful.
  if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
  {
    RDCDEBUG("Deferred Context %llu Attempting capture - initially %s, %s", GetResourceID(),
             m_SuccessfulCapture ? "successful" : "unsuccessful",
             m_EmptyCommandList ? "empty" : "non-empty");

    m_SuccessfulCapture |= m_EmptyCommandList;

    if(m_SuccessfulCapture)
      m_FailureReason = CaptureSucceeded;
    else
      m_FailureReason = CaptureFailed_UncappedCmdlist;

    RDCDEBUG("Deferred Context %llu Attempting capture - now %s", GetResourceID(),
             m_SuccessfulCapture ? "successful" : "unsuccessful");
  }
  else
  {
    RDCDEBUG("Immediate Context %llu Attempting capture", GetResourceID());

    m_SuccessfulCapture = true;
    m_FailureReason = CaptureSucceeded;

    m_ContextRecord->LockChunks();
    while(m_ContextRecord->HasChunks())
    {
      Chunk *chunk = m_ContextRecord->GetLastChunk();

      SAFE_DELETE(chunk);
      m_ContextRecord->PopChunk();
    }
    m_ContextRecord->UnlockChunks();

    m_ContextRecord->FreeParents(m_pDevice->GetResourceManager());
  }
}

void WrappedID3D11DeviceContext::FinishCapture()
{
  if(GetType() != D3D11_DEVICE_CONTEXT_DEFERRED ||
     !RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists)
  {
    m_State = WRITING_IDLE;

    m_SuccessfulCapture = false;
    m_FailureReason = CaptureSucceeded;
  }
}

void WrappedID3D11DeviceContext::EndCaptureFrame()
{
  SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);
  m_pSerialiser->Serialise("context", m_ResourceID);

  bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
  m_pSerialiser->Serialise("HasCallstack", HasCallstack);

  if(HasCallstack)
  {
    Callstack::Stackwalk *call = Callstack::Collect();

    RDCASSERT(call->NumLevels() < 0xff);

    size_t numLevels = call->NumLevels();
    uint64_t *stack = (uint64_t *)call->GetAddrs();

    m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

    delete call;
  }

  m_ContextRecord->AddChunk(scope.Get());
}

void WrappedID3D11DeviceContext::Present(UINT SyncInterval, UINT Flags)
{
  SCOPED_SERIALISE_CONTEXT(SWAP_PRESENT);
  m_pSerialiser->Serialise("context", m_ResourceID);
  m_pSerialiser->Serialise("SyncInterval", SyncInterval);
  m_pSerialiser->Serialise("Flags", Flags);

  m_ContextRecord->AddChunk(scope.Get());
}

void WrappedID3D11DeviceContext::FreeCaptureData()
{
  SCOPED_LOCK(m_pDevice->D3DLock());

  for(auto it = WrappedID3D11Buffer::m_BufferList.begin();
      it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
  {
    D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(it->first);

    if(record == NULL)
      continue;

    bool inuse = false;
    for(auto mapit = m_OpenMaps.begin(); mapit != m_OpenMaps.end(); ++mapit)
    {
      if(mapit->first.resource == it->first)
      {
        inuse = true;
        break;
      }
    }

    if(inuse)
      continue;

    record->FreeShadowStorage();
  }
}

void WrappedID3D11DeviceContext::CleanupCapture()
{
  if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
  {
    m_SuccessfulCapture |= m_EmptyCommandList;

    if(m_SuccessfulCapture)
      m_FailureReason = CaptureSucceeded;
    else
      m_FailureReason = CaptureFailed_UncappedCmdlist;

    for(auto it = m_MapResourceRecordAllocs.begin(); it != m_MapResourceRecordAllocs.end(); ++it)
    {
      D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(it->first);
      if(record)
        record->FreeContextID(it->second);
    }

    if(RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists)
      return;
  }
  else
  {
    m_SuccessfulCapture = true;
    m_FailureReason = CaptureSucceeded;
  }

  m_ContextRecord->LockChunks();
  while(m_ContextRecord->HasChunks())
  {
    Chunk *chunk = m_ContextRecord->GetLastChunk();

    SAFE_DELETE(chunk);
    m_ContextRecord->PopChunk();
  }
  m_ContextRecord->UnlockChunks();

  m_ContextRecord->FreeParents(m_pDevice->GetResourceManager());

  for(auto it = m_MissingTracks.begin(); it != m_MissingTracks.end(); ++it)
  {
    if(m_pDevice->GetResourceManager()->HasResourceRecord(*it))
      MarkDirtyResource(*it);
  }

  m_MissingTracks.clear();
}

void WrappedID3D11DeviceContext::BeginFrame()
{
  {
    SCOPED_LOCK(m_AnnotLock);
    m_AnnotationQueue.clear();
  }
}

void WrappedID3D11DeviceContext::EndFrame()
{
  DrainAnnotationQueue();

  if(m_State == WRITING_IDLE)
    m_pDevice->GetResourceManager()->FlushPendingDirty();
}

bool WrappedID3D11DeviceContext::IsFL11_1()
{
  return m_pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1;
}

void WrappedID3D11DeviceContext::ProcessChunk(uint64_t offset, D3D11ChunkType chunk, bool forceExecute)
{
  m_CurChunkOffset = offset;

  ResourceId ctxId;
  m_pSerialiser->Serialise("context", ctxId);

  // ctxId is now ignored
  WrappedID3D11DeviceContext *context = this;

  LogState state = context->m_State;

  if(forceExecute)
    context->m_State = EXECUTING;
  else
    context->m_State = m_State;

  m_AddedDrawcall = false;

  switch(chunk)
  {
    case SET_INPUT_LAYOUT: context->Serialise_IASetInputLayout(0x0); break;
    case SET_VBUFFER: context->Serialise_IASetVertexBuffers(0, 0, 0x0, 0x0, 0x0); break;
    case SET_IBUFFER: context->Serialise_IASetIndexBuffer(0, DXGI_FORMAT_UNKNOWN, 0); break;
    case SET_TOPOLOGY:
      context->Serialise_IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
      break;

    case SET_VS_CBUFFERS: context->Serialise_VSSetConstantBuffers(0, 0, 0x0); break;
    case SET_VS_RESOURCES: context->Serialise_VSSetShaderResources(0, 0, 0x0); break;
    case SET_VS_SAMPLERS: context->Serialise_VSSetSamplers(0, 0, 0x0); break;
    case SET_VS: context->Serialise_VSSetShader(0x0, 0x0, 0); break;

    case SET_HS_CBUFFERS: context->Serialise_HSSetConstantBuffers(0, 0, 0x0); break;
    case SET_HS_RESOURCES: context->Serialise_HSSetShaderResources(0, 0, 0x0); break;
    case SET_HS_SAMPLERS: context->Serialise_HSSetSamplers(0, 0, 0x0); break;
    case SET_HS: context->Serialise_HSSetShader(0x0, 0x0, 0); break;

    case SET_DS_CBUFFERS: context->Serialise_DSSetConstantBuffers(0, 0, 0x0); break;
    case SET_DS_RESOURCES: context->Serialise_DSSetShaderResources(0, 0, 0x0); break;
    case SET_DS_SAMPLERS: context->Serialise_DSSetSamplers(0, 0, 0x0); break;
    case SET_DS: context->Serialise_DSSetShader(0x0, 0x0, 0); break;

    case SET_GS_CBUFFERS: context->Serialise_GSSetConstantBuffers(0, 0, 0x0); break;
    case SET_GS_RESOURCES: context->Serialise_GSSetShaderResources(0, 0, 0x0); break;
    case SET_GS_SAMPLERS: context->Serialise_GSSetSamplers(0, 0, 0x0); break;
    case SET_GS: context->Serialise_GSSetShader(0x0, 0x0, 0); break;

    case SET_SO_TARGETS: context->Serialise_SOSetTargets(0, 0x0, 0x0); break;

    case SET_PS_CBUFFERS: context->Serialise_PSSetConstantBuffers(0, 0, 0x0); break;
    case SET_PS_RESOURCES: context->Serialise_PSSetShaderResources(0, 0, 0x0); break;
    case SET_PS_SAMPLERS: context->Serialise_PSSetSamplers(0, 0, 0x0); break;
    case SET_PS: context->Serialise_PSSetShader(0x0, 0x0, 0); break;

    case SET_CS_CBUFFERS: context->Serialise_CSSetConstantBuffers(0, 0, 0x0); break;
    case SET_CS_RESOURCES: context->Serialise_CSSetShaderResources(0, 0, 0x0); break;
    case SET_CS_UAVS: context->Serialise_CSSetUnorderedAccessViews(0, 0, 0x0, 0x0); break;
    case SET_CS_SAMPLERS: context->Serialise_CSSetSamplers(0, 0, 0x0); break;
    case SET_CS: context->Serialise_CSSetShader(0x0, 0x0, 0); break;

    case SET_VIEWPORTS: context->Serialise_RSSetViewports(0, 0x0); break;
    case SET_SCISSORS: context->Serialise_RSSetScissorRects(0, 0x0); break;
    case SET_RASTER: context->Serialise_RSSetState(0x0); break;

    case SET_RTARGET: context->Serialise_OMSetRenderTargets(0, 0x0, 0x0); break;
    case SET_RTARGET_AND_UAVS:
      context->Serialise_OMSetRenderTargetsAndUnorderedAccessViews(0, 0x0, 0x0, 0, 0, 0x0, 0x0);
      break;
    case SET_BLEND: context->Serialise_OMSetBlendState(0x0, (FLOAT *)0x0, 0); break;
    case SET_DEPTHSTENCIL: context->Serialise_OMSetDepthStencilState(0x0, 0); break;

    case DRAW_INDEXED_INST: context->Serialise_DrawIndexedInstanced(0, 0, 0, 0, 0); break;
    case DRAW_INST: context->Serialise_DrawInstanced(0, 0, 0, 0); break;
    case DRAW_INDEXED: context->Serialise_DrawIndexed(0, 0, 0); break;
    case DRAW: context->Serialise_Draw(0, 0); break;
    case DRAW_AUTO: context->Serialise_DrawAuto(); break;
    case DRAW_INDEXED_INST_INDIRECT: context->Serialise_DrawIndexedInstancedIndirect(0x0, 0); break;
    case DRAW_INST_INDIRECT: context->Serialise_DrawInstancedIndirect(0x0, 0); break;

    case MAP: context->Serialise_Map(0, 0, (D3D11_MAP)0, 0, 0); break;
    case UNMAP: context->Serialise_Unmap(0, 0); break;

    case COPY_SUBRESOURCE_REGION:
      context->Serialise_CopySubresourceRegion(0x0, 0, 0, 0, 0, 0x0, 0, 0x0);
      break;
    case COPY_RESOURCE: context->Serialise_CopyResource(0x0, 0x0); break;
    case UPDATE_SUBRESOURCE: context->Serialise_UpdateSubresource(0x0, 0, 0x0, 0x0, 0, 0); break;
    case COPY_STRUCTURE_COUNT: context->Serialise_CopyStructureCount(0x0, 0, 0x0); break;
    case RESOLVE_SUBRESOURCE:
      context->Serialise_ResolveSubresource(0x0, 0, 0x0, 0, DXGI_FORMAT_UNKNOWN);
      break;
    case GENERATE_MIPS: context->Serialise_GenerateMips(0x0); break;

    case CLEAR_DSV: context->Serialise_ClearDepthStencilView(0x0, 0, 0.0f, 0); break;
    case CLEAR_RTV: context->Serialise_ClearRenderTargetView(0x0, (FLOAT *)0x0); break;
    case CLEAR_UAV_INT: context->Serialise_ClearUnorderedAccessViewUint(0x0, (UINT *)0x0); break;
    case CLEAR_UAV_FLOAT:
      context->Serialise_ClearUnorderedAccessViewFloat(0x0, (FLOAT *)0x0);
      break;
    case CLEAR_STATE: context->Serialise_ClearState(); break;

    case EXECUTE_CMD_LIST: context->Serialise_ExecuteCommandList(0x0, 0); break;
    case DISPATCH: context->Serialise_Dispatch(0, 0, 0); break;
    case DISPATCH_INDIRECT: context->Serialise_DispatchIndirect(0x0, 0); break;
    case FINISH_CMD_LIST: context->Serialise_FinishCommandList(0, 0x0); break;
    case FLUSH: context->Serialise_Flush(); break;

    case SET_PREDICATION: context->Serialise_SetPredication(0x0, 0x0); break;
    case SET_RESOURCE_MINLOD: context->Serialise_SetResourceMinLOD(0x0, 0); break;

    case BEGIN: context->Serialise_Begin(0x0); break;
    case END: context->Serialise_End(0x0); break;

    case COPY_SUBRESOURCE_REGION1:
      context->Serialise_CopySubresourceRegion1(0x0, 0, 0, 0, 0, 0x0, 0, 0x0, 0);
      break;
    case UPDATE_SUBRESOURCE1:
      context->Serialise_UpdateSubresource1(0x0, 0, 0x0, 0x0, 0, 0, 0);
      break;
    case CLEAR_VIEW: context->Serialise_ClearView(0x0, 0x0, 0x0, 0); break;

    case SET_VS_CBUFFERS1: context->Serialise_VSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0); break;
    case SET_HS_CBUFFERS1: context->Serialise_HSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0); break;
    case SET_DS_CBUFFERS1: context->Serialise_DSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0); break;
    case SET_GS_CBUFFERS1: context->Serialise_GSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0); break;
    case SET_PS_CBUFFERS1: context->Serialise_PSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0); break;
    case SET_CS_CBUFFERS1: context->Serialise_CSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0); break;

    case PUSH_EVENT: context->Serialise_PushEvent(0, L""); break;
    case SET_MARKER: context->Serialise_SetMarker(0, L""); break;
    case POP_EVENT: context->Serialise_PopEvent(); break;

    case DISCARD_RESOURCE: context->Serialise_DiscardResource(NULL); break;
    case DISCARD_VIEW: context->Serialise_DiscardView(NULL); break;
    case DISCARD_VIEW1: context->Serialise_DiscardView1(NULL, NULL, 0); break;

    case RESTORE_STATE_AFTER_EXEC:
    {
      // apply saved state.
      if(m_DeferredSavedState)
      {
        m_DeferredSavedState->ApplyState(this);
        SAFE_DELETE(m_DeferredSavedState);
      }
      break;
    }

    case RESTORE_STATE_AFTER_FINISH:
    {
      D3D11RenderState rs(m_pSerialiser);
      rs.Serialise(m_State, m_pDevice);
      rs.ApplyState(this);
      break;
    }

    case SWAP_DEVICE_STATE: Serialise_SwapDeviceContextState(NULL, NULL); break;

    case SWAP_PRESENT:
    {
      // we don't do anything with these parameters, they're just here to store
      // them for user benefits
      UINT SyncInterval = 0, Flags = 0;
      m_pSerialiser->Serialise("SyncInterval", SyncInterval);
      m_pSerialiser->Serialise("Flags", Flags);
      m_PresentChunk = true;
      break;
    }

    case CONTEXT_CAPTURE_FOOTER:
    {
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
        if(!m_PresentChunk)
          AddEvent("IDXGISwapChain::Present()");

        DrawcallDescription draw;
        draw.name = "Present()";
        draw.flags |= DrawFlags::Present;

        draw.copyDestination = m_pDevice->GetBackbufferResourceID();

        AddDrawcall(draw, true);
      }
    }
    break;
    default: RDCERR("Unrecognised Chunk type %d", chunk); break;
  }

  m_pSerialiser->PopContext(chunk);

  if(context->m_State == READING && chunk == SET_MARKER)
  {
    // no push/pop necessary
  }
  else if(context->m_State == READING && chunk == PUSH_EVENT)
  {
    // push down the drawcallstack to the latest drawcall
    context->m_DrawcallStack.push_back(&context->m_DrawcallStack.back()->children.back());
  }
  else if(context->m_State == READING && chunk == POP_EVENT)
  {
    // refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
    if(context->m_DrawcallStack.size() > 1)
      context->m_DrawcallStack.pop_back();
  }
  else if(context->m_State == READING)
  {
    if(!m_AddedDrawcall)
      context->AddEvent(m_pSerialiser->GetDebugStr());
  }

  m_AddedDrawcall = false;

  if(forceExecute)
    context->m_State = state;
}

void WrappedID3D11DeviceContext::AddUsage(const DrawcallDescription &d)
{
  const D3D11RenderState *pipe = m_CurrentPipelineState;
  uint32_t e = d.eventID;

  DrawFlags DrawMask = DrawFlags::Drawcall | DrawFlags::Dispatch | DrawFlags::CmdList;
  if(!(d.flags & DrawMask))
    return;

  //////////////////////////////
  // IA

  if(d.flags & DrawFlags::UseIBuffer && pipe->IA.IndexBuffer != NULL)
    m_ResourceUses[GetIDForResource(pipe->IA.IndexBuffer)].push_back(
        EventUsage(e, ResourceUsage::IndexBuffer));

  for(int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
    if(pipe->IA.Used_VB(m_pDevice, i))
      m_ResourceUses[GetIDForResource(pipe->IA.VBs[i])].push_back(
          EventUsage(e, ResourceUsage::VertexBuffer));

  //////////////////////////////
  // Shaders

  const D3D11RenderState::shader *shArr[6] = {
      &pipe->VS, &pipe->HS, &pipe->DS, &pipe->GS, &pipe->PS, &pipe->CS,
  };
  for(int s = 0; s < 6; s++)
  {
    const D3D11RenderState::shader &sh = *shArr[s];

    for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
      if(sh.Used_CB(i))
        m_ResourceUses[GetIDForResource(sh.ConstantBuffers[i])].push_back(EventUsage(e, CBUsage(s)));

    for(int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
    {
      if(sh.Used_SRV(i))
      {
        WrappedID3D11ShaderResourceView1 *view = (WrappedID3D11ShaderResourceView1 *)sh.SRVs[i];
        m_ResourceUses[view->GetResourceResID()].push_back(
            EventUsage(e, ResUsage(s), view->GetResourceID()));
      }
    }

    if(s == 5)
    {
      for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
      {
        if(pipe->CS.Used_UAV(i) && pipe->CSUAVs[i])
        {
          WrappedID3D11UnorderedAccessView1 *view =
              (WrappedID3D11UnorderedAccessView1 *)pipe->CSUAVs[i];
          m_ResourceUses[view->GetResourceResID()].push_back(
              EventUsage(e, ResourceUsage::CS_RWResource, view->GetResourceID()));
        }
      }
    }
  }

  //////////////////////////////
  // SO

  for(int i = 0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
    if(pipe->SO.Buffers[i])    // assuming for now that any SO target bound is used.
      m_ResourceUses[GetIDForResource(pipe->SO.Buffers[i])].push_back(
          EventUsage(e, ResourceUsage::StreamOut));

  //////////////////////////////
  // OM

  for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    if(pipe->PS.Used_UAV(i) && pipe->OM.UAVs[i])
    {
      WrappedID3D11UnorderedAccessView1 *view = (WrappedID3D11UnorderedAccessView1 *)pipe->OM.UAVs[i];
      m_ResourceUses[view->GetResourceResID()].push_back(
          EventUsage(e, ResourceUsage::PS_RWResource, view->GetResourceID()));
    }
  }

  if(pipe->OM.DepthView)    // assuming for now that any DSV bound is used.
  {
    WrappedID3D11DepthStencilView *view = (WrappedID3D11DepthStencilView *)pipe->OM.DepthView;
    m_ResourceUses[view->GetResourceResID()].push_back(
        EventUsage(e, ResourceUsage::DepthStencilTarget, view->GetResourceID()));
  }

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(pipe->OM.RenderTargets[i])    // assuming for now that any RTV bound is used.
    {
      WrappedID3D11RenderTargetView1 *view =
          (WrappedID3D11RenderTargetView1 *)pipe->OM.RenderTargets[i];
      m_ResourceUses[view->GetResourceResID()].push_back(
          EventUsage(e, ResourceUsage::ColorTarget, view->GetResourceID()));
    }
  }
}

void WrappedID3D11DeviceContext::AddDrawcall(const DrawcallDescription &d, bool hasEvents)
{
  DrawcallDescription draw = d;

  m_AddedDrawcall = true;

  draw.eventID = m_CurEventID;
  draw.drawcallID = m_CurDrawcallID;

  draw.indexByteWidth = 0;
  if(m_CurrentPipelineState->IA.IndexFormat == DXGI_FORMAT_R16_UINT)
    draw.indexByteWidth = 2;
  if(m_CurrentPipelineState->IA.IndexFormat == DXGI_FORMAT_R32_UINT)
    draw.indexByteWidth = 4;

  draw.topology = MakePrimitiveTopology(m_CurrentPipelineState->IA.Topo);

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    draw.outputs[i] = ResourceId();
    if(m_CurrentPipelineState->OM.RenderTargets[i])
      draw.outputs[i] = m_pDevice->GetResourceManager()->GetOriginalID(
          ((WrappedID3D11RenderTargetView1 *)m_CurrentPipelineState->OM.RenderTargets[i])
              ->GetResourceResID());
  }

  {
    draw.depthOut = ResourceId();
    if(m_CurrentPipelineState->OM.DepthView)
      draw.depthOut = m_pDevice->GetResourceManager()->GetOriginalID(
          ((WrappedID3D11DepthStencilView *)m_CurrentPipelineState->OM.DepthView)->GetResourceResID());
  }

  // markers don't increment drawcall ID
  DrawFlags MarkerMask = DrawFlags::SetMarker | DrawFlags::PushMarker;
  if(!(draw.flags & MarkerMask))
    m_CurDrawcallID++;

  if(hasEvents)
  {
    draw.events = m_CurEvents;
    m_CurEvents.clear();
  }

  AddUsage(draw);

  // should have at least the root drawcall here, push this drawcall
  // onto the back's children list.
  if(!m_DrawcallStack.empty())
  {
    DrawcallTreeNode node(draw);
    node.children.insert(node.children.begin(), draw.children.elems,
                         draw.children.elems + draw.children.count);
    m_DrawcallStack.back()->children.push_back(node);
  }
  else
    RDCERR("Somehow lost drawcall stack!");
}

void WrappedID3D11DeviceContext::AddEvent(string description)
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_CurEventID;

  apievent.eventDesc = description;

  Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
  if(stack)
  {
    create_array(apievent.callstack, stack->NumLevels());
    memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t) * stack->NumLevels());
  }

  m_CurEvents.push_back(apievent);

  if(m_State == READING)
    m_Events.push_back(apievent);
}

APIEvent WrappedID3D11DeviceContext::GetEvent(uint32_t eventID)
{
  for(size_t i = m_Events.size() - 1; i > 0; i--)
  {
    if(m_Events[i].eventID <= eventID)
      return m_Events[i];
  }

  return m_Events[0];
}

void WrappedID3D11DeviceContext::ReplayFakeContext(ResourceId id)
{
  m_FakeContext = id;
}

static void PadToAligned(Serialiser *dst, uint64_t alignment)
{
  uint64_t offs = dst->GetOffset();
  uint64_t alignedoffs = AlignUp(offs, alignment);

  // nothing to do
  if(offs == alignedoffs)
    return;

  uint16_t chunkIdx = 0;
  dst->Serialise("", chunkIdx);
  offs += sizeof(chunkIdx);

  uint8_t controlByte = 0;    // control byte 0 indicates padding
  dst->Serialise("", controlByte);
  offs += sizeof(controlByte);

  offs++;    // we will have to write out a byte indicating how much padding exists, so add 1
  alignedoffs = AlignUp(offs, alignment);

  uint8_t padLength = (alignedoffs - offs) & 0xff;
  dst->Serialise("", padLength);

  // we might have padded with the control bytes, so only write some bytes if we need to
  if(padLength > 0)
  {
    const byte zeroes[256] = {};
    dst->RawWriteBytes(zeroes, (size_t)padLength);
  }
}

static void CopyChunk(Serialiser *src, Serialiser *dst, uint64_t offsBegin)
{
  uint64_t offsEnd = src->GetOffset();

  // this whole function is quite an abuse of the serialiser interface :(.
  // It will all go away when I can break backwards compatibility and remove
  // this code and greatly tidy up the interface.

  src->SetOffset(offsBegin);

  dst->RawWriteBytes(src->RawReadBytes(size_t(offsEnd - offsBegin)), size_t(offsEnd - offsBegin));
}

static void CopyUnmap(uint32_t d3dLogVersion, Serialiser *src, Serialiser *dst, Serialiser *tmp)
{
  ResourceId Resource;
  uint32_t Subresource = 0;
  D3D11_MAP MapType = D3D11_MAP_WRITE_DISCARD;
  uint32_t MapFlags = 0;
  uint32_t DiffStart = 0;
  uint32_t DiffEnd = 0;
  byte *buf = NULL;
  size_t len = 0;

  tmp->Rewind();
  tmp->PushContext("", "", UNMAP, false);

  // context id - ignored but needed to match expected format
  tmp->Serialise("", Resource);

  src->Serialise("", Resource);
  tmp->Serialise("", Resource);

  src->Serialise("", Subresource);
  tmp->Serialise("", Subresource);

  src->Serialise("", MapType);
  tmp->Serialise("", MapType);

  src->Serialise("", MapFlags);
  tmp->Serialise("", MapFlags);

  src->Serialise("", DiffStart);
  tmp->Serialise("", DiffStart);

  src->Serialise("", DiffEnd);
  tmp->Serialise("", DiffEnd);

  if(d3dLogVersion >= 0x000007)
    src->AlignNextBuffer(32);

  src->SerialiseBuffer("", buf, len);
  tmp->SerialiseBuffer("", buf, len);

  src->PopContext(UNMAP);
  tmp->PopContext(UNMAP);

  SAFE_DELETE_ARRAY(buf);

  dst->RawWriteBytes(tmp->GetRawPtr(0), size_t(tmp->GetOffset()));
}

static void CopyUpdateSubresource(uint32_t d3dLogVersion, Serialiser *src, Serialiser *dst,
                                  Serialiser *tmp)
{
  ResourceId idx;
  uint32_t flags = 0;
  uint32_t DestSubresource = 0;
  uint8_t isUpdate = true;

  uint8_t HasDestBox = 0;
  D3D11_BOX box = {};
  uint32_t SourceRowPitch = 0;
  uint32_t SourceDepthPitch = 0;

  uint32_t ResourceBufLen = 0;

  byte *buf = NULL;
  size_t len = 0;

  tmp->Rewind();
  tmp->PushContext("", "", UPDATE_SUBRESOURCE1, false);

  // context id - ignored but needed to match expected format
  tmp->Serialise("", idx);

  src->Serialise("", idx);
  tmp->Serialise("", idx);

  src->Serialise("", flags);
  tmp->Serialise("", flags);

  src->Serialise("", DestSubresource);
  tmp->Serialise("", DestSubresource);

  src->Serialise("", isUpdate);
  tmp->Serialise("", isUpdate);

  if(isUpdate)
  {
    src->Serialise("", HasDestBox);
    tmp->Serialise("", HasDestBox);

    if(HasDestBox)
    {
      src->Serialise("", box);
      tmp->Serialise("", box);
    }

    src->Serialise("", SourceRowPitch);
    tmp->Serialise("", SourceRowPitch);

    src->Serialise("", SourceDepthPitch);
    tmp->Serialise("", SourceDepthPitch);

    src->Serialise("", ResourceBufLen);
    tmp->Serialise("", ResourceBufLen);

    src->SerialiseBuffer("", buf, len);
    tmp->SerialiseBuffer("", buf, len);
  }
  else
  {
    // shouldn't get in here, any chunks we're looking at are context chunks
    // so they should be recorded as updates

    src->Serialise("", ResourceBufLen);
    tmp->Serialise("", ResourceBufLen);

    if(d3dLogVersion >= 0x000007)
      src->AlignNextBuffer(32);

    src->SerialiseBuffer("", buf, len);
    tmp->SerialiseBuffer("", buf, len);
  }

  src->PopContext(UPDATE_SUBRESOURCE1);
  tmp->PopContext(UPDATE_SUBRESOURCE1);

  SAFE_DELETE_ARRAY(buf);

  dst->RawWriteBytes(tmp->GetRawPtr(0), size_t(tmp->GetOffset()));
}

void WrappedID3D11DeviceContext::FlattenLog()
{
  Serialiser *src = m_pSerialiser;
  Serialiser *dst = new Serialiser(NULL, Serialiser::WRITING, false);

  Serialiser *tmp = new Serialiser(NULL, Serialiser::WRITING, false);

  map<ResourceId, Serialiser *> deferred;

  uint64_t offsBegin = src->GetOffset();

  D3D11ChunkType chunk = (D3D11ChunkType)src->PushContext(NULL, NULL, 1, false);
  RDCASSERTEQUAL(chunk, CONTEXT_CAPTURE_HEADER);
  src->SkipCurrentChunk();
  src->PopContext(chunk);

  CopyChunk(src, dst, offsBegin);

  for(;;)
  {
    offsBegin = src->GetOffset();

    chunk = (D3D11ChunkType)src->PushContext(NULL, NULL, 1, false);

    ResourceId ctx;
    src->Serialise("ctx", ctx);

    WrappedID3D11DeviceContext *context =
        (WrappedID3D11DeviceContext *)m_pDevice->GetResourceManager()->GetLiveResource(ctx);

    // if it's a local chunk just copy it
    if(context == this)
    {
      ResourceId cmdList;
      uint8_t restore = 0;

      if(chunk == EXECUTE_CMD_LIST)
      {
        m_pSerialiser->Serialise("RestoreContextState", restore);
        m_pSerialiser->Serialise("cmdList", cmdList);
      }

      // these chunks need to be reserialised individually to ensure padding bytes in
      // between all remain the same
      if(chunk == UNMAP)
      {
        PadToAligned(dst, 64);
        CopyUnmap(m_pDevice->GetLogVersion(), src, dst, tmp);
      }
      else if(chunk == UPDATE_SUBRESOURCE || chunk == UPDATE_SUBRESOURCE1)
      {
        PadToAligned(dst, 64);
        CopyUpdateSubresource(m_pDevice->GetLogVersion(), src, dst, tmp);
      }
      else
      {
        src->SetOffset(offsBegin);
        src->PushContext(NULL, NULL, 1, false);
        src->SkipCurrentChunk();
        src->PopContext(chunk);

        CopyChunk(src, dst, offsBegin);
      }

      if(chunk == EXECUTE_CMD_LIST)
      {
        Serialiser *def = deferred[cmdList];

        if(def == NULL)
        {
          RDCERR("ExecuteCommandList found for command list we didn't Finish!");
        }
        else
        {
          // when inserting, ensure the chunks stay aligned (this is harmless if they don't need
          // alignment). Do this by inserting padding bytes, ugly but it works.
          PadToAligned(dst, 64);

          // now insert the chunks to replay
          dst->RawWriteBytes(deferred[cmdList]->GetRawPtr(0), (size_t)deferred[cmdList]->GetSize());

          // if we have to restore the state, the above Execute.. will have
          // saved it, so we just add a little chunk to let it pop it again
          if(restore)
          {
            tmp->Rewind();
            tmp->PushContext("", "", RESTORE_STATE_AFTER_EXEC, false);
            tmp->PopContext(RESTORE_STATE_AFTER_EXEC);

            dst->RawWriteBytes(tmp->GetRawPtr(0), size_t(tmp->GetOffset()));
          }

          // again since we inserted chunks we need to align again
          PadToAligned(dst, 64);
        }
      }
    }
    else
    {
      if(deferred[ctx] == NULL)
        deferred[ctx] = new Serialiser(NULL, Serialiser::WRITING, false);

      ResourceId cmdList;
      uint8_t restore = 0;

      if(chunk == FINISH_CMD_LIST)
      {
        m_pSerialiser->Serialise("RestoreDeferredContextState", restore);
        m_pSerialiser->Serialise("cmdList", cmdList);

        if(restore != 0)
        {
          RDCWARN(
              "RestoreDeferredContextState == TRUE was set for command list %llu. "
              "On old captures this wasn't properly replayed and can't be fixed as-is, "
              "re-capture with latest RenderDoc to solve this issue.");
        }
      }

      // these chunks need to be reserialised individually to ensure padding bytes in
      // between all remain the same
      if(chunk == UNMAP)
      {
        PadToAligned(deferred[ctx], 64);
        CopyUnmap(m_pDevice->GetLogVersion(), src, deferred[ctx], tmp);
      }
      else if(chunk == UPDATE_SUBRESOURCE || chunk == UPDATE_SUBRESOURCE1)
      {
        PadToAligned(deferred[ctx], 64);
        CopyUpdateSubresource(m_pDevice->GetLogVersion(), src, deferred[ctx], tmp);
      }
      else
      {
        src->SetOffset(offsBegin);
        src->PushContext(NULL, NULL, 1, false);
        src->SkipCurrentChunk();
        src->PopContext(chunk);

        CopyChunk(src, deferred[ctx], offsBegin);
      }

      if(chunk == FINISH_CMD_LIST)
      {
        // here is where RestoreDeferredContextState is broken, this doesn't
        // take into account the inherited state from previous recordings.
        deferred[cmdList] = deferred[ctx];
        deferred[ctx] = new Serialiser(NULL, Serialiser::WRITING, false);
      }
    }

    if(chunk == CONTEXT_CAPTURE_FOOTER)
    {
      break;
    }
  }

  m_pSerialiser = new Serialiser((size_t)dst->GetSize(), dst->GetRawPtr(0), false);
  m_OwnSerialiser = true;

  m_pSerialiser->SetDebugText(true);
  m_pSerialiser->SetChunkNameLookup(&WrappedID3D11Device::GetChunkName);

  // tidy up the temporary serialisers
  SAFE_DELETE(dst);
  SAFE_DELETE(tmp);

  for(auto it = deferred.begin(); it != deferred.end(); ++it)
    SAFE_DELETE(it->second);
}

void WrappedID3D11DeviceContext::ReplayLog(LogState readType, uint32_t startEventID,
                                           uint32_t endEventID, bool partial)
{
  m_State = readType;

  if(readType == READING && m_pDevice->GetNumDeferredContexts() &&
     m_pDevice->GetLogVersion() < 0x00000A)
  {
    RDCLOG("Flattening log file");

    // flatten the log
    FlattenLog();

    RDCLOG("Flattened");
  }

  m_DoStateVerify = true;

  if(readType == EXECUTING && m_pDevice->GetNumDeferredContexts() &&
     m_pDevice->GetLogVersion() < 0x00000A)
  {
    m_pSerialiser->SetOffset(0);
  }

  D3D11ChunkType header = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
  RDCASSERTEQUAL(header, CONTEXT_CAPTURE_HEADER);

  ResourceId id;
  m_pSerialiser->Serialise("context", id);

  // id is now ignored

  Serialise_BeginCaptureFrame(!partial);

  m_pSerialiser->PopContext(header);

  m_CurEvents.clear();

  if(m_State == EXECUTING)
  {
    APIEvent ev = GetEvent(startEventID);
    m_CurEventID = ev.eventID;
    m_pSerialiser->SetOffset(ev.fileOffset);
  }
  else if(m_State == READING)
  {
    m_CurEventID = 1;
  }

  if(m_State == EXECUTING)
  {
    ClearMaps();
    for(size_t i = 0; i < m_pDevice->GetNumDeferredContexts(); i++)
    {
      WrappedID3D11DeviceContext *defcontext = m_pDevice->GetDeferredContext(i);
      defcontext->ClearMaps();
    }
  }

  m_pDevice->GetResourceManager()->MarkInFrame(true);

  uint64_t startOffset = m_pSerialiser->GetOffset();

  for(;;)
  {
    if(m_State == EXECUTING && m_CurEventID > endEventID)
    {
      // set event ID correctly as we haven't actually replayed the next one.
      m_CurEventID = endEventID;
      // we can just break out if we've done all the events desired.
      break;
    }

    uint64_t offset = m_pSerialiser->GetOffset();

    D3D11ChunkType chunktype = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

    ProcessChunk(offset, chunktype, false);

    RenderDoc::Inst().SetProgress(FrameEventsRead,
                                  float(offset - startOffset) / float(m_pSerialiser->GetSize()));

    // for now just abort after capture scope. Really we'd need to support multiple frames
    // but for now this will do.
    if(chunktype == CONTEXT_CAPTURE_FOOTER)
      break;

    m_CurEventID++;
  }

  if(m_State == READING)
  {
    m_pDevice->GetFrameRecord().drawcallList = m_ParentDrawcall.Bake();
    m_pDevice->GetFrameRecord().frameInfo.debugMessages = m_pDevice->GetDebugMessages();

    for(auto it = WrappedID3D11Buffer::m_BufferList.begin();
        it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
      m_ResourceUses[it->first];

    for(auto it = WrappedID3D11Texture1D::m_TextureList.begin();
        it != WrappedID3D11Texture1D::m_TextureList.end(); ++it)
      m_ResourceUses[it->first];
    for(auto it = WrappedID3D11Texture2D1::m_TextureList.begin();
        it != WrappedID3D11Texture2D1::m_TextureList.end(); ++it)
      m_ResourceUses[it->first];
    for(auto it = WrappedID3D11Texture3D1::m_TextureList.begin();
        it != WrappedID3D11Texture3D1::m_TextureList.end(); ++it)
      m_ResourceUses[it->first];

#define CHECK_UNUSED_INITIAL_STATES 0

#if CHECK_UNUSED_INITIAL_STATES
    int initialSkips = 0;
#endif

    // it's easier to remove duplicate usages here than check it as we go.
    // this means if textures are bound in multiple places in the same draw
    // we don't have duplicate uses
    for(auto it = m_ResourceUses.begin(); it != m_ResourceUses.end(); ++it)
    {
      vector<EventUsage> &v = it->second;
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()), v.end());

#if CHECK_UNUSED_INITIAL_STATES
      ResourceId resid = m_pDevice->GetResourceManager()->GetOriginalID(it->first);

      if(m_pDevice->GetResourceManager()->GetInitialContents(resid).resource == NULL)
        continue;

      // code disabled for now as skipping these initial states
      // doesn't seem to produce any measurable improvement in any case
      // I've checked
      RDCDEBUG("Resource %llu", resid);
      if(v.empty())
      {
        RDCDEBUG("Never used!");
        initialSkips++;
      }
      else
      {
        bool written = false;

        for(auto usit = v.begin(); usit != v.end(); ++usit)
        {
          ResourceUsage u = usit->usage;

          if(u == ResourceUsage::StreamOut ||
             (u >= ResourceUsage::VS_RWResource && u <= ResourceUsage::CS_RWResource) ||
             u == ResourceUsage::DepthStencilTarget || u == ResourceUsage::ColorTarget)
          {
            written = true;
            break;
          }
        }

        if(written)
        {
          RDCDEBUG("Written in frame - needs initial state");
        }
        else
        {
          RDCDEBUG("Never written to in the frame");
          initialSkips++;
        }
      }
#endif
    }

    // RDCDEBUG("Can skip %d initial states.", initialSkips);
  }

  m_pDevice->GetResourceManager()->MarkInFrame(false);

  m_State = READING;

  m_DoStateVerify = false;
}

void WrappedID3D11DeviceContext::ClearMaps()
{
  auto it = m_OpenMaps.begin();

  for(; it != m_OpenMaps.end(); ++it)
  {
    RDCASSERT(m_pDevice->GetResourceManager()->HasLiveResource(it->first.resource));

    ID3D11Resource *res =
        (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(it->first.resource);

    m_pRealContext->Unmap(m_pDevice->GetResourceManager()->UnwrapResource(res),
                          it->first.subresource);
  }

  m_OpenMaps.clear();
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceContext::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D11DeviceContext *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11DeviceContext))
  {
    *ppvObject = (ID3D11DeviceContext *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11DeviceChild))
  {
    *ppvObject = (ID3D11DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11DeviceContext1))
  {
    if(m_pRealContext1)
    {
      *ppvObject = (ID3D11DeviceContext1 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11DeviceContext2))
  {
    if(m_pRealContext2)
    {
      *ppvObject = (ID3D11DeviceContext2 *)this;
      AddRef();
      RDCWARN(
          "Trying to get ID3D11DeviceContext2. DX11.2 tiled resources are not supported at this "
          "time.");
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11DeviceContext3))
  {
    if(m_pRealContext3)
    {
      *ppvObject = (ID3D11DeviceContext3 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11Multithread))
  {
    RDCWARN("ID3D11Multithread is not supported");
    return E_NOINTERFACE;
  }
  else if(riid == __uuidof(ID3DUserDefinedAnnotation))
  {
    *ppvObject = (ID3DUserDefinedAnnotation *)&m_UserAnnotation;
    m_UserAnnotation.AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11InfoQueue))
  {
    // forward to device
    return m_pDevice->QueryInterface(riid, ppvObject);
  }
  else
  {
    string guid = ToStr::Get(riid);
    RDCWARN("Querying ID3D11DeviceContext for interface: %s", guid.c_str());
  }

  return RefCounter::QueryInterface(riid, ppvObject);
}

#pragma region Record Statistics

void WrappedID3D11DeviceContext::RecordIndexBindStats(ID3D11Buffer *Buffer)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  IndexBindStats &indices = stats.indices;
  indices.calls += 1;
  indices.sets += (Buffer != NULL);
  indices.nulls += (Buffer == NULL);
}

void WrappedID3D11DeviceContext::RecordVertexBindStats(UINT NumBuffers, ID3D11Buffer *Buffers[])
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  VertexBindStats &vertices = stats.vertices;
  vertices.calls += 1;
  RDCASSERT(NumBuffers < vertices.bindslots.size());
  vertices.bindslots[NumBuffers] += 1;

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(Buffers[i])
      vertices.sets += 1;
    else
      vertices.nulls += 1;
  }
}

void WrappedID3D11DeviceContext::RecordLayoutBindStats(ID3D11InputLayout *Layout)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  LayoutBindStats &layouts = stats.layouts;
  layouts.calls += 1;
  layouts.sets += (Layout != NULL);
  layouts.nulls += (Layout == NULL);
}

void WrappedID3D11DeviceContext::RecordConstantStats(ShaderStage stage, UINT NumBuffers,
                                                     ID3D11Buffer *Buffers[])
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RDCASSERT(size_t(stage) < ARRAY_COUNT(stats.constants));
  ConstantBindStats &constants = stats.constants[uint32_t(stage)];
  constants.calls += 1;
  RDCASSERT(NumBuffers < constants.bindslots.size());
  constants.bindslots[NumBuffers] += 1;

  for(UINT i = 0; i < NumBuffers; i++)
  {
    if(Buffers[i])
    {
      constants.sets += 1;

      D3D11_BUFFER_DESC desc;
      Buffers[i]->GetDesc(&desc);
      uint32_t bufferSize = desc.ByteWidth;
      size_t bucket = BucketForRecord<ConstantBindStats>::Get(bufferSize);
      RDCASSERT(bucket < constants.sizes.size());
      constants.sizes[bucket] += 1;
    }
    else
    {
      constants.nulls += 1;
    }
  }
}

void WrappedID3D11DeviceContext::RecordResourceStats(ShaderStage stage, UINT NumResources,
                                                     ID3D11ShaderResourceView *Resources[])
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RDCASSERT(size_t(stage) < ARRAY_COUNT(stats.resources));
  ResourceBindStats &resources = stats.resources[(uint32_t)stage];
  resources.calls += 1;
  RDCASSERT(NumResources < resources.bindslots.size());
  resources.bindslots[NumResources] += 1;

  const TextureDim mapping[] = {
      TextureDim::Unknown,        TextureDim::Buffer,           TextureDim::Texture1D,
      TextureDim::Texture1DArray, TextureDim::Texture2D,        TextureDim::Texture2DArray,
      TextureDim::Texture2DMS,    TextureDim::Texture2DMSArray, TextureDim::Texture3D,
      TextureDim::TextureCube,    TextureDim::TextureCubeArray, TextureDim::Buffer,
  };
  RDCCOMPILE_ASSERT(ARRAY_COUNT(mapping) == D3D_SRV_DIMENSION_BUFFEREX + 1,
                    "Update mapping table.");

  for(UINT i = 0; i < NumResources; i++)
  {
    if(Resources[i])
    {
      resources.sets += 1;

      D3D11_SHADER_RESOURCE_VIEW_DESC desc;
      Resources[i]->GetDesc(&desc);
      RDCASSERT(desc.ViewDimension < ARRAY_COUNT(mapping));
      TextureDim type = mapping[desc.ViewDimension];
      // #mivance surprisingly this is not asserted in operator[] for
      // rdctype::array so I'm being paranoid
      RDCASSERT((int)type < (int)resources.types.size());
      resources.types[(int)type] += 1;
    }
    else
    {
      resources.nulls += 1;
    }
  }
}

void WrappedID3D11DeviceContext::RecordSamplerStats(ShaderStage stage, UINT NumSamplers,
                                                    ID3D11SamplerState *Samplers[])
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RDCASSERT(size_t(stage) < ARRAY_COUNT(stats.samplers));
  SamplerBindStats &samplers = stats.samplers[uint32_t(stage)];
  samplers.calls += 1;
  RDCASSERT(NumSamplers < samplers.bindslots.size());
  samplers.bindslots[NumSamplers] += 1;

  for(UINT i = 0; i < NumSamplers; i++)
  {
    if(Samplers[i])
      samplers.sets += 1;
    else
      samplers.nulls += 1;
  }
}

void WrappedID3D11DeviceContext::RecordUpdateStats(ID3D11Resource *res, uint32_t Size, bool Server)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  ResourceUpdateStats &updates = stats.updates;

  if(res == NULL)
    return;

  updates.calls += 1;
  updates.clients += (Server == false);
  updates.servers += (Server == true);

  const TextureDim mapping[] = {
      TextureDim::Unknown,      // D3D11_RESOURCE_DIMENSION_UNKNOWN	= 0,
      TextureDim::Buffer,       // D3D11_RESOURCE_DIMENSION_BUFFER	= 1,
      TextureDim::Texture1D,    // D3D11_RESOURCE_DIMENSION_TEXTURE1D	= 2,
      TextureDim::Texture2D,    // D3D11_RESOURCE_DIMENSION_TEXTURE2D	= 3,
      TextureDim::Texture3D,    // D3D11_RESOURCE_DIMENSION_TEXTURE3D	= 4
  };

  D3D11_RESOURCE_DIMENSION dim;
  res->GetType(&dim);
  RDCASSERT(dim < ARRAY_COUNT(mapping));
  TextureDim type = mapping[dim];
  RDCASSERT((int)type < (int)updates.types.size());
  updates.types[(int)type] += 1;

  // #mivance it might be nice to query the buffer to differentiate
  // between bindings for constant buffers

  size_t bucket = BucketForRecord<ResourceUpdateStats>::Get(Size);
  updates.sizes[bucket] += 1;
}

void WrappedID3D11DeviceContext::RecordDrawStats(bool instanced, bool indirect, UINT InstanceCount)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  DrawcallStats &draws = stats.draws;

  draws.calls += 1;
  draws.instanced += (uint32_t)instanced;
  draws.indirect += (uint32_t)indirect;

  if(instanced)
  {
    size_t bucket = BucketForRecord<DrawcallStats>::Get(InstanceCount);
    RDCASSERT(bucket < draws.counts.size());
    draws.counts[bucket] += 1;
  }
}

void WrappedID3D11DeviceContext::RecordDispatchStats(bool indirect)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  DispatchStats &dispatches = stats.dispatches;

  dispatches.calls += 1;
  dispatches.indirect += (uint32_t)indirect;
}

void WrappedID3D11DeviceContext::RecordShaderStats(ShaderStage stage, ID3D11DeviceChild *Current,
                                                   ID3D11DeviceChild *Shader)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RDCASSERT(size_t(stage) <= ARRAY_COUNT(stats.shaders));
  ShaderChangeStats &shaders = stats.shaders[uint32_t(stage)];

  shaders.calls += 1;
  shaders.sets += (Shader != NULL);
  shaders.nulls += (Shader == NULL);
  shaders.redundants += (Current == Shader);
}

void WrappedID3D11DeviceContext::RecordBlendStats(ID3D11BlendState *Blend, FLOAT BlendFactor[4],
                                                  UINT SampleMask)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  BlendStats &blends = stats.blends;

  blends.calls += 1;
  blends.sets += (Blend != NULL);
  blends.nulls += (Blend == NULL);
  const D3D11RenderState::outmerger *Current = &m_CurrentPipelineState->OM;
  bool same = (Current->BlendState == Blend) &&
              (memcmp(Current->BlendFactor, BlendFactor, sizeof(Current->BlendFactor)) == 0) &&
              (Current->SampleMask == SampleMask);
  blends.redundants += (uint32_t)same;
}

void WrappedID3D11DeviceContext::RecordDepthStencilStats(ID3D11DepthStencilState *DepthStencil,
                                                         UINT StencilRef)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  DepthStencilStats &depths = stats.depths;

  depths.calls += 1;
  depths.sets += (DepthStencil != NULL);
  depths.nulls += (DepthStencil == NULL);
  const D3D11RenderState::outmerger *Current = &m_CurrentPipelineState->OM;
  bool same = (Current->DepthStencilState == DepthStencil) && (Current->StencRef == StencilRef);
  depths.redundants += (uint32_t)same;
}

void WrappedID3D11DeviceContext::RecordRasterizationStats(ID3D11RasterizerState *Rasterizer)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RasterizationStats &rasters = stats.rasters;

  rasters.calls += 1;
  rasters.sets += (Rasterizer != NULL);
  rasters.nulls += (Rasterizer == NULL);
  const D3D11RenderState::rasterizer *Current = &m_CurrentPipelineState->RS;
  bool same = (Current->State == Rasterizer);
  rasters.redundants += (uint32_t)same;
}

void WrappedID3D11DeviceContext::RecordViewportStats(UINT NumViewports,
                                                     const D3D11_VIEWPORT *viewports)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RasterizationStats &rasters = stats.rasters;

  rasters.calls += 1;
  rasters.sets += 1;
  // #mivance fairly sure setting 0 viewports/null viewports is illegal?
  const D3D11RenderState::rasterizer *Current = &m_CurrentPipelineState->RS;
  bool same = (Current->NumViews == NumViewports);
  for(UINT index = 0; index < NumViewports; index++)
  {
    same = (same && (Current->Viewports[index] == viewports[index]));
  }
  rasters.redundants += (uint32_t)same;
  RDCASSERT(NumViewports < rasters.viewports.size());
  rasters.viewports[NumViewports] += 1;
}

void WrappedID3D11DeviceContext::RecordScissorStats(UINT NumRects, const D3D11_RECT *rects)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  RasterizationStats &rasters = stats.rasters;

  rasters.calls += 1;
  rasters.sets += 1;
  // #mivance see above
  const D3D11RenderState::rasterizer *Current = &m_CurrentPipelineState->RS;
  bool same = (Current->NumScissors == NumRects);
  for(UINT index = 0; index < NumRects; index++)
  {
    same = (same && (Current->Scissors[index] == rects[index]));
  }
  rasters.redundants += (uint32_t)same;
  RDCASSERT(NumRects < rasters.rects.size());
  rasters.rects[NumRects] += 1;
}

void WrappedID3D11DeviceContext::RecordOutputMergerStats(UINT NumRTVs, ID3D11RenderTargetView *RTVs[],
                                                         ID3D11DepthStencilView *DSV,
                                                         UINT UAVStartSlot, UINT NumUAVs,
                                                         ID3D11UnorderedAccessView *UAVs[])
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  OutputTargetStats &outputs = stats.outputs;

  outputs.calls += 1;
  // #mivance is an elaborate redundancy here even useful?
  // const D3D11RenderState::outmerger* Current = &m_CurrentPipelineState->OM;

  if(RTVs != NULL)
  {
    for(UINT index = 0; index < NumRTVs; index++)
    {
      outputs.sets += (RTVs[index] != NULL);
      outputs.nulls += (RTVs[index] == NULL);
    }
  }
  else if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
  {
    outputs.nulls += NumRTVs;
  }

  outputs.sets += (DSV != NULL);
  outputs.nulls += (DSV == NULL);

  if(UAVs != NULL)
  {
    for(UINT index = 0; index < NumUAVs; index++)
    {
      outputs.sets += (UAVs[index] != NULL);
      outputs.nulls += (UAVs[index] == NULL);
    }
  }
  else
  {
    outputs.nulls += NumUAVs;
  }

  UINT NumSlots = NumRTVs + NumUAVs;
  if(NumRTVs == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
    NumSlots = NumUAVs;
  RDCASSERT(NumSlots < outputs.bindslots.size());
  outputs.bindslots[NumSlots] += 1;
}

#pragma endregion
