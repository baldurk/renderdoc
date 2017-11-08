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
#include "strings/string_utils.h"

WRAPPED_POOL_INST(WrappedID3D11DeviceContext);
WRAPPED_POOL_INST(WrappedID3D11CommandList);

INT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::BeginEvent(LPCWSTR Name)
{
  if(m_Context)
    return m_Context->PushMarker(0, Name);

  return -1;
}

INT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::EndEvent()
{
  if(m_Context)
    return m_Context->PopMarker();

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
                                                       ID3D11DeviceContext *context)
    : RefCounter(context),
      m_pDevice(realDevice),
      m_pRealContext(context),
      m_ScratchSerialiser(new StreamWriter(1024), Ownership::Stream)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this,
                                                              sizeof(WrappedID3D11DeviceContext));

  uint32_t flags = 0;

  if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks)
    flags |= WriteSerialiser::ChunkCallstack;

  m_ScratchSerialiser.SetChunkMetadataRecording(flags);

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
  {
    NullCBOffsets[i] = 0;
    NullCBCounts[i] = 4096;
  }

  D3D11_FEATURE_DATA_D3D11_OPTIONS features;
  RDCEraseEl(features);
  HRESULT hr = S_OK;

  if(m_pRealContext)
    hr = m_pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &features, sizeof(features));

  m_SetCBuffer1 = false;
  if(SUCCEEDED(hr))
    m_SetCBuffer1 = features.ConstantBufferOffsetting == TRUE;

  m_pRealContext1 = NULL;
  m_pRealContext2 = NULL;
  m_pRealContext3 = NULL;
  if(m_pRealContext)
  {
    m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&m_pRealContext1);
    m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext2), (void **)&m_pRealContext2);
    m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext3), (void **)&m_pRealContext3);
  }

  m_NeedUpdateSubWorkaround = false;
  if(m_pRealContext)
  {
    D3D11_FEATURE_DATA_THREADING caps = {FALSE, FALSE};

    hr = m_pDevice->CheckFeatureSupport(D3D11_FEATURE_THREADING, &caps, sizeof(caps));
    if(SUCCEEDED(hr) && !caps.DriverCommandLists)
      m_NeedUpdateSubWorkaround = true;
  }

  if(RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::LoadingReplaying;
  }
  else
  {
    m_State = CaptureState::BackgroundCapturing;
  }

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

  m_ScratchSerialiser.SetUserData(GetResourceManager());

  m_SuccessfulCapture = true;
  m_FailureReason = CaptureSucceeded;
  m_EmptyCommandList = true;

  m_PresentChunk = false;

  m_DrawcallStack.push_back(&m_ParentDrawcall);

  m_CurEventID = 0;
  m_CurDrawcallID = 1;

  m_MarkerIndentLevel = 0;
  m_UserAnnotation.SetContext(this);

  m_CurrentPipelineState = new D3D11RenderState(D3D11RenderState::Empty);
  m_DeferredSavedState = NULL;
  m_DoStateVerify = IsCaptureMode(m_State);

  if(!context || context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
  {
    m_CurrentPipelineState->SetImmediatePipeline(m_pDevice);
  }
  else
  {
    m_CurrentPipelineState->SetDevice(m_pDevice);
    m_pDevice->SoftRef();

    if(IsCaptureMode(m_State) && RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists)
      m_State = CaptureState::ActiveCapturing;
  }

  ReplayFakeContext(ResourceId());
}

WrappedID3D11DeviceContext::~WrappedID3D11DeviceContext()
{
  if(m_ContextRecord)
    m_ContextRecord->Delete(m_pDevice->GetResourceManager());

  if(m_pRealContext && m_pRealContext->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
    m_pDevice->RemoveDeferredContext(this);

  for(auto it = m_StreamOutCounters.begin(); it != m_StreamOutCounters.end(); ++it)
  {
    SAFE_RELEASE(it->second.query);
  }

  SAFE_DELETE(m_FrameReader);

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

D3D11ResourceManager *WrappedID3D11DeviceContext::GetResourceManager()
{
  return m_pDevice->GetResourceManager();
}

std::string WrappedID3D11DeviceContext::GetChunkName(uint32_t idx)
{
  if((SystemChunk)idx < SystemChunk::FirstDriverChunk)
    return ToStr((SystemChunk)idx);

  return ToStr((D3D11Chunk)idx);
}

struct HiddenCounter
{
  ResourceId id;
  uint64_t counterValue;
};

DECLARE_REFLECTION_STRUCT(HiddenCounter);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, HiddenCounter &el)
{
  SERIALISE_MEMBER(id);
  SERIALISE_MEMBER(counterValue);
}

template <typename SerialiserType>
bool WrappedID3D11DeviceContext::Serialise_BeginCaptureFrame(SerialiserType &ser,
                                                             bool applyInitialState)
{
  D3D11RenderState state(D3D11RenderState::Empty);

  if(ser.IsWriting())
  {
    state.CopyState(*m_CurrentPipelineState);

    state.MarkReferenced(this, true);
  }

  SERIALISE_ELEMENT(state);

  if(IsReplayingAndReading() && applyInitialState)
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
  std::vector<HiddenCounter> HiddenStreamOutCounters;

  if(ser.IsWriting())
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
    for(auto it = m_StreamOutCounters.begin(); it != m_StreamOutCounters.end(); ++it)
    {
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
        RDCERR("Couldn't retrieve hidden buffer counter for streamout on buffer %llu", it->first);
      }

      HiddenStreamOutCounters.push_back({it->first, (uint64_t)numPrims.NumPrimitivesWritten});
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

  SERIALISE_ELEMENT(HiddenStreamOutCounters);

  // read in the known stream-out counters at the start of the frame.
  // any stream-out that happens in the captured frame will be replayed
  // and those counters will override this value when it comes to a
  // DrawAuto()
  if(IsReplayingAndReading())
  {
    for(const HiddenCounter &c : HiddenStreamOutCounters)
    {
      if(m_pDevice->GetResourceManager()->HasLiveResource(c.id))
        m_StreamOutCounters[m_pDevice->GetResourceManager()->GetLiveID(c.id)].numPrims =
            c.counterValue;
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
  WriteSerialiser &ser = m_ScratchSerialiser;
  SCOPED_SERIALISE_CHUNK(D3D11Chunk::CaptureBegin);

  Serialise_BeginCaptureFrame(ser, false);

  {
    SCOPED_LOCK(m_AnnotLock);

    m_AnnotationQueue.clear();
  }

  m_ContextRecord->AddChunk(scope.Get(), 1);
}

void WrappedID3D11DeviceContext::AttemptCapture()
{
  m_State = CaptureState::ActiveCapturing;

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
    m_State = CaptureState::BackgroundCapturing;

    m_SuccessfulCapture = false;
    m_FailureReason = CaptureSucceeded;
  }
}

void WrappedID3D11DeviceContext::EndCaptureFrame()
{
  WriteSerialiser &ser = m_ScratchSerialiser;
  ser.SetDrawChunk();
  SCOPED_SERIALISE_CHUNK(D3D11Chunk::CaptureEnd);

  SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");

  m_ContextRecord->AddChunk(scope.Get());
}

void WrappedID3D11DeviceContext::Present(UINT SyncInterval, UINT Flags)
{
  WriteSerialiser &ser = m_ScratchSerialiser;
  SCOPED_SERIALISE_CHUNK(D3D11Chunk::SwapchainPresent);
  SERIALISE_ELEMENT(m_ResourceID).Named("Context ID");
  SERIALISE_ELEMENT(SyncInterval);
  SERIALISE_ELEMENT(Flags);

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

  if(IsBackgroundCapturing(m_State))
    m_pDevice->GetResourceManager()->FlushPendingDirty();
}

bool WrappedID3D11DeviceContext::IsFL11_1()
{
  return m_pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1;
}

void WrappedID3D11DeviceContext::ProcessChunk(ReadSerialiser &ser, D3D11Chunk chunk)
{
  ResourceId ctxId;
  SERIALISE_ELEMENT(ctxId).Named("Context ID");

  m_AddedDrawcall = false;

  switch(chunk)
  {
    case D3D11Chunk::IASetInputLayout: Serialise_IASetInputLayout(ser, 0x0); break;
    case D3D11Chunk::IASetVertexBuffers:
      Serialise_IASetVertexBuffers(ser, 0, 0, 0x0, 0x0, 0x0);
      break;
    case D3D11Chunk::IASetIndexBuffer:
      Serialise_IASetIndexBuffer(ser, 0, DXGI_FORMAT_UNKNOWN, 0);
      break;
    case D3D11Chunk::IASetPrimitiveTopology:
      Serialise_IASetPrimitiveTopology(ser, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
      break;

    case D3D11Chunk::VSSetConstantBuffers: Serialise_VSSetConstantBuffers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::VSSetShaderResources: Serialise_VSSetShaderResources(ser, 0, 0, 0x0); break;
    case D3D11Chunk::VSSetSamplers: Serialise_VSSetSamplers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::VSSetShader: Serialise_VSSetShader(ser, 0x0, 0x0, 0); break;

    case D3D11Chunk::HSSetConstantBuffers: Serialise_HSSetConstantBuffers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::HSSetShaderResources: Serialise_HSSetShaderResources(ser, 0, 0, 0x0); break;
    case D3D11Chunk::HSSetSamplers: Serialise_HSSetSamplers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::HSSetShader: Serialise_HSSetShader(ser, 0x0, 0x0, 0); break;

    case D3D11Chunk::DSSetConstantBuffers: Serialise_DSSetConstantBuffers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::DSSetShaderResources: Serialise_DSSetShaderResources(ser, 0, 0, 0x0); break;
    case D3D11Chunk::DSSetSamplers: Serialise_DSSetSamplers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::DSSetShader: Serialise_DSSetShader(ser, 0x0, 0x0, 0); break;

    case D3D11Chunk::GSSetConstantBuffers: Serialise_GSSetConstantBuffers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::GSSetShaderResources: Serialise_GSSetShaderResources(ser, 0, 0, 0x0); break;
    case D3D11Chunk::GSSetSamplers: Serialise_GSSetSamplers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::GSSetShader: Serialise_GSSetShader(ser, 0x0, 0x0, 0); break;

    case D3D11Chunk::SOSetTargets: Serialise_SOSetTargets(ser, 0, 0x0, 0x0); break;

    case D3D11Chunk::PSSetConstantBuffers: Serialise_PSSetConstantBuffers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::PSSetShaderResources: Serialise_PSSetShaderResources(ser, 0, 0, 0x0); break;
    case D3D11Chunk::PSSetSamplers: Serialise_PSSetSamplers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::PSSetShader: Serialise_PSSetShader(ser, 0x0, 0x0, 0); break;

    case D3D11Chunk::CSSetConstantBuffers: Serialise_CSSetConstantBuffers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::CSSetShaderResources: Serialise_CSSetShaderResources(ser, 0, 0, 0x0); break;
    case D3D11Chunk::CSSetUnorderedAccessViews:
      Serialise_CSSetUnorderedAccessViews(ser, 0, 0, 0x0, 0x0);
      break;
    case D3D11Chunk::CSSetSamplers: Serialise_CSSetSamplers(ser, 0, 0, 0x0); break;
    case D3D11Chunk::CSSetShader: Serialise_CSSetShader(ser, 0x0, 0x0, 0); break;

    case D3D11Chunk::RSSetViewports: Serialise_RSSetViewports(ser, 0, 0x0); break;
    case D3D11Chunk::RSSetScissorRects: Serialise_RSSetScissorRects(ser, 0, 0x0); break;
    case D3D11Chunk::RSSetState: Serialise_RSSetState(ser, 0x0); break;

    case D3D11Chunk::OMSetRenderTargets: Serialise_OMSetRenderTargets(ser, 0, 0x0, 0x0); break;
    case D3D11Chunk::OMSetRenderTargetsAndUnorderedAccessViews:
      Serialise_OMSetRenderTargetsAndUnorderedAccessViews(ser, 0, 0x0, 0x0, 0, 0, 0x0, 0x0);
      break;
    case D3D11Chunk::OMSetBlendState: Serialise_OMSetBlendState(ser, 0x0, (FLOAT *)0x0, 0); break;
    case D3D11Chunk::OMSetDepthStencilState: Serialise_OMSetDepthStencilState(ser, 0x0, 0); break;

    case D3D11Chunk::DrawIndexedInstanced:
      Serialise_DrawIndexedInstanced(ser, 0, 0, 0, 0, 0);
      break;
    case D3D11Chunk::DrawInstanced: Serialise_DrawInstanced(ser, 0, 0, 0, 0); break;
    case D3D11Chunk::DrawIndexed: Serialise_DrawIndexed(ser, 0, 0, 0); break;
    case D3D11Chunk::Draw: Serialise_Draw(ser, 0, 0); break;
    case D3D11Chunk::DrawAuto: Serialise_DrawAuto(ser); break;
    case D3D11Chunk::DrawIndexedInstancedIndirect:
      Serialise_DrawIndexedInstancedIndirect(ser, 0x0, 0);
      break;
    case D3D11Chunk::DrawInstancedIndirect: Serialise_DrawInstancedIndirect(ser, 0x0, 0); break;

    case D3D11Chunk::Map: Serialise_Map(ser, 0, 0, (D3D11_MAP)0, 0, 0); break;
    case D3D11Chunk::Unmap: Serialise_Unmap(ser, 0, 0); break;

    case D3D11Chunk::CopySubresourceRegion:
      Serialise_CopySubresourceRegion(ser, 0x0, 0, 0, 0, 0, 0x0, 0, 0x0);
      break;
    case D3D11Chunk::CopyResource: Serialise_CopyResource(ser, 0x0, 0x0); break;
    case D3D11Chunk::UpdateSubresource:
      Serialise_UpdateSubresource(ser, 0x0, 0, 0x0, 0x0, 0, 0);
      break;
    case D3D11Chunk::CopyStructureCount: Serialise_CopyStructureCount(ser, 0x0, 0, 0x0); break;
    case D3D11Chunk::ResolveSubresource:
      Serialise_ResolveSubresource(ser, 0x0, 0, 0x0, 0, DXGI_FORMAT_UNKNOWN);
      break;
    case D3D11Chunk::GenerateMips: Serialise_GenerateMips(ser, 0x0); break;

    case D3D11Chunk::ClearDepthStencilView:
      Serialise_ClearDepthStencilView(ser, 0x0, 0, 0.0f, 0);
      break;
    case D3D11Chunk::ClearRenderTargetView:
      Serialise_ClearRenderTargetView(ser, 0x0, (FLOAT *)0x0);
      break;
    case D3D11Chunk::ClearUnorderedAccessViewUint:
      Serialise_ClearUnorderedAccessViewUint(ser, 0x0, (UINT *)0x0);
      break;
    case D3D11Chunk::ClearUnorderedAccessViewFloat:
      Serialise_ClearUnorderedAccessViewFloat(ser, 0x0, (FLOAT *)0x0);
      break;
    case D3D11Chunk::ClearState: Serialise_ClearState(ser); break;

    case D3D11Chunk::ExecuteCommandList: Serialise_ExecuteCommandList(ser, 0x0, 0); break;
    case D3D11Chunk::Dispatch: Serialise_Dispatch(ser, 0, 0, 0); break;
    case D3D11Chunk::DispatchIndirect: Serialise_DispatchIndirect(ser, 0x0, 0); break;
    case D3D11Chunk::FinishCommandList: Serialise_FinishCommandList(ser, 0, 0x0); break;
    case D3D11Chunk::Flush: Serialise_Flush(ser); break;

    case D3D11Chunk::SetPredication: Serialise_SetPredication(ser, 0x0, 0x0); break;
    case D3D11Chunk::SetResourceMinLOD: Serialise_SetResourceMinLOD(ser, 0x0, 0); break;

    case D3D11Chunk::Begin: Serialise_Begin(ser, 0x0); break;
    case D3D11Chunk::End: Serialise_End(ser, 0x0); break;

    case D3D11Chunk::CopySubresourceRegion1:
      Serialise_CopySubresourceRegion1(ser, 0x0, 0, 0, 0, 0, 0x0, 0, 0x0, 0);
      break;
    case D3D11Chunk::UpdateSubresource1:
      Serialise_UpdateSubresource1(ser, 0x0, 0, 0x0, 0x0, 0, 0, 0);
      break;
    case D3D11Chunk::ClearView: Serialise_ClearView(ser, 0x0, 0x0, 0x0, 0); break;

    case D3D11Chunk::VSSetConstantBuffers1:
      Serialise_VSSetConstantBuffers1(ser, 0, 0, 0x0, 0x0, 0x0);
      break;
    case D3D11Chunk::HSSetConstantBuffers1:
      Serialise_HSSetConstantBuffers1(ser, 0, 0, 0x0, 0x0, 0x0);
      break;
    case D3D11Chunk::DSSetConstantBuffers1:
      Serialise_DSSetConstantBuffers1(ser, 0, 0, 0x0, 0x0, 0x0);
      break;
    case D3D11Chunk::GSSetConstantBuffers1:
      Serialise_GSSetConstantBuffers1(ser, 0, 0, 0x0, 0x0, 0x0);
      break;
    case D3D11Chunk::PSSetConstantBuffers1:
      Serialise_PSSetConstantBuffers1(ser, 0, 0, 0x0, 0x0, 0x0);
      break;
    case D3D11Chunk::CSSetConstantBuffers1:
      Serialise_CSSetConstantBuffers1(ser, 0, 0, 0x0, 0x0, 0x0);
      break;

    case D3D11Chunk::PushMarker: Serialise_PushMarker(ser, 0, L""); break;
    case D3D11Chunk::SetMarker: Serialise_SetMarker(ser, 0, L""); break;
    case D3D11Chunk::PopMarker: Serialise_PopMarker(ser); break;

    case D3D11Chunk::DiscardResource: Serialise_DiscardResource(ser, NULL); break;
    case D3D11Chunk::DiscardView: Serialise_DiscardView(ser, NULL); break;
    case D3D11Chunk::DiscardView1: Serialise_DiscardView1(ser, NULL, NULL, 0); break;

    case D3D11Chunk::PostExecuteCommandListRestore:
      Serialise_PostExecuteCommandListRestore(ser);
      break;

    case D3D11Chunk::PostFinishCommandListSet: Serialise_PostFinishCommandListSet(ser); break;

    case D3D11Chunk::SwapDeviceContextState:
      Serialise_SwapDeviceContextState(ser, NULL, NULL);
      break;

    case D3D11Chunk::SwapchainPresent:
    {
      // we don't do anything with these parameters, they're just here to store
      // them for user benefits
      UINT SyncInterval = 0, Flags = 0;
      SERIALISE_ELEMENT(SyncInterval);
      SERIALISE_ELEMENT(Flags);
      m_PresentChunk = true;
      break;
    }

    case D3D11Chunk::CaptureEnd:
    {
      if(IsLoading(m_State))
      {
        if(!m_PresentChunk)
          AddEvent();

        DrawcallDescription draw;
        draw.name = "End of Frame";
        draw.flags |= DrawFlags::Present;

        draw.copyDestination = m_pDevice->GetBackbufferResourceID();

        AddDrawcall(draw, true);
      }
    }
    break;
    default: RDCERR("Unrecognised Chunk type %d", chunk); break;
  }

  if(IsLoading(m_State) && m_CurEventID > 0)
  {
    if(chunk == D3D11Chunk::SetMarker)
    {
      // no push/pop necessary
    }
    else if(chunk == D3D11Chunk::PushMarker)
    {
      // push down the drawcallstack to the latest drawcall
      m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());
    }
    else if(chunk == D3D11Chunk::PopMarker)
    {
      // refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
      if(m_DrawcallStack.size() > 1)
        m_DrawcallStack.pop_back();
    }
    else
    {
      if(!m_AddedDrawcall)
        AddEvent();
    }
  }

  m_AddedDrawcall = false;
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

  const D3D11RenderState::Shader *shArr[6] = {
      &pipe->VS, &pipe->HS, &pipe->DS, &pipe->GS, &pipe->PS, &pipe->CS,
  };
  for(int s = 0; s < 6; s++)
  {
    const D3D11RenderState::Shader &sh = *shArr[s];

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
    m_DrawcallStack.back()->children.push_back(draw);
  else
    RDCERR("Somehow lost drawcall stack!");
}

void WrappedID3D11DeviceContext::AddEvent()
{
  APIEvent apievent;

  apievent.fileOffset = m_CurChunkOffset;
  apievent.eventID = m_CurEventID;

  apievent.chunkIndex = uint32_t(m_StructuredFile->chunks.size() - 1);

  apievent.callstack = m_ChunkMetadata.callstack;

  m_CurEvents.push_back(apievent);

  if(IsLoading(m_State))
    m_Events.push_back(apievent);
}

const APIEvent &WrappedID3D11DeviceContext::GetEvent(uint32_t eventID)
{
  for(const APIEvent &e : m_Events)
  {
    if(e.eventID >= eventID)
      return e;
  }

  return m_Events.back();
}

void WrappedID3D11DeviceContext::ReplayFakeContext(ResourceId id)
{
  m_FakeContext = id;
}

void WrappedID3D11DeviceContext::ReplayLog(CaptureState readType, uint32_t startEventID,
                                           uint32_t endEventID, bool partial)
{
  m_State = readType;

  if(!m_FrameReader)
  {
    RDCERR("Can't replay context capture without frame reader");
    return;
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

  m_DoStateVerify = true;

  D3D11Chunk header = ser.ReadChunk<D3D11Chunk>();
  RDCASSERTEQUAL(header, D3D11Chunk::CaptureBegin);

  Serialise_BeginCaptureFrame(ser, !partial);

  ser.EndChunk();

  m_CurEvents.clear();

  if(IsActiveReplaying(m_State))
  {
    APIEvent ev = GetEvent(startEventID);
    m_CurEventID = ev.eventID;
    ser.GetReader()->SetOffset(ev.fileOffset);

    ClearMaps();
    for(size_t i = 0; i < m_pDevice->GetNumDeferredContexts(); i++)
    {
      WrappedID3D11DeviceContext *defcontext = m_pDevice->GetDeferredContext(i);
      defcontext->ClearMaps();
    }
  }
  else
  {
    m_CurEventID = 1;
  }

  m_pDevice->GetResourceManager()->MarkInFrame(true);

  uint64_t startOffset = ser.GetReader()->GetOffset();

  for(;;)
  {
    if(IsActiveReplaying(m_State) && m_CurEventID > endEventID)
    {
      // set event ID correctly as we haven't actually replayed the next one.
      m_CurEventID = endEventID;
      // we can just break out if we've done all the events desired.
      break;
    }

    m_CurChunkOffset = ser.GetReader()->GetOffset();

    D3D11Chunk chunktype = ser.ReadChunk<D3D11Chunk>();

    m_ChunkMetadata = ser.ChunkMetadata();

    ProcessChunk(ser, chunktype);

    ser.EndChunk();

    RenderDoc::Inst().SetProgress(
        FrameEventsRead, float(m_CurChunkOffset - startOffset) / float(ser.GetReader()->GetSize()));

    if(chunktype == D3D11Chunk::CaptureEnd)
      break;

    m_CurEventID++;
  }

  if(IsLoading(m_State))
  {
    m_pDevice->GetFrameRecord().drawcallList = m_ParentDrawcall.children;
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

  // swap the structure back now that we've accumulated the frame as well.
  if(IsLoading(m_State) || IsStructuredExporting(m_State))
    ser.GetStructuredFile().swap(m_pDevice->GetStructuredFile());

  m_StructuredFile = NULL;

  m_pDevice->GetResourceManager()->MarkInFrame(false);

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
    RDCWARN("Querying ID3D11DeviceContext for interface: %s", ToStr(riid).c_str());
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

void WrappedID3D11DeviceContext::RecordVertexBindStats(UINT NumBuffers, ID3D11Buffer *const Buffers[])
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
                                                     ID3D11Buffer *const Buffers[])
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
                                                     ID3D11ShaderResourceView *const Resources[])
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
      // rdcarray so I'm being paranoid
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
                                                    ID3D11SamplerState *const Samplers[])
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

void WrappedID3D11DeviceContext::RecordBlendStats(ID3D11BlendState *Blend,
                                                  const FLOAT BlendFactor[4], UINT SampleMask)
{
  FrameStatistics &stats = m_pDevice->GetFrameStats();
  BlendStats &blends = stats.blends;

  blends.calls += 1;
  blends.sets += (Blend != NULL);
  blends.nulls += (Blend == NULL);
  const D3D11RenderState::OutputMerger *Current = &m_CurrentPipelineState->OM;
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
  const D3D11RenderState::OutputMerger *Current = &m_CurrentPipelineState->OM;
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
  const D3D11RenderState::Rasterizer *Current = &m_CurrentPipelineState->RS;
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
  const D3D11RenderState::Rasterizer *Current = &m_CurrentPipelineState->RS;
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
  const D3D11RenderState::Rasterizer *Current = &m_CurrentPipelineState->RS;
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

  if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
  {
    if(RTVs != NULL)
    {
      for(UINT index = 0; index < NumRTVs; index++)
      {
        outputs.sets += (RTVs[index] != NULL);
        outputs.nulls += (RTVs[index] == NULL);
      }
    }
    else
    {
      outputs.nulls += NumRTVs;
    }
  }

  outputs.sets += (DSV != NULL);
  outputs.nulls += (DSV == NULL);

  if(NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
  {
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
  }

  UINT NumSlots = 0;

  if(NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
    NumSlots += NumRTVs;
  if(NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS)
    NumSlots += NumUAVs;

  RDCASSERT(NumSlots < outputs.bindslots.size());
  outputs.bindslots[NumSlots] += 1;
}

#pragma endregion
