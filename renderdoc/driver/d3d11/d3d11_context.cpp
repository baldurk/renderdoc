/******************************************************************************
 * The MIT License (MIT)
 * 
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


#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_manager.h"
#include "driver/d3d11/d3d11_resources.h"

#include "common/string_utils.h"

#include "replay/type_helpers.h"

WRAPPED_POOL_INST(WrappedID3D11DeviceContext);
WRAPPED_POOL_INST(WrappedID3D11CommandList);


#if defined(INCLUDE_D3D_11_1)
INT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::BeginEvent(LPCWSTR Name)
{
	if(m_Context)
		return m_Context->BeginEvent(0, Name);

	return -1;
}

INT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::EndEvent()
{
	if(m_Context)
		return m_Context->EndEvent();

	return -1;
}

void STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::SetMarker(LPCWSTR Name)
{
	if(m_Context)
		return m_Context->SetMarker(0, Name);
}

HRESULT STDMETHODCALLTYPE WrappedID3DUserDefinedAnnotation::QueryInterface(REFIID riid, void **ppvObject)
{
	//DEFINE_GUID(IID_ID3DUserDefinedAnnotation,0xb2daad8b,0x03d4,0x4dbf,0x95,0xeb,0x32,0xab,0x4b,0x63,0xd0,0xab);
	static const GUID ID3D11UserDefinedAnnotation_uuid = { 0xb2daad8b, 0x03d4, 0x4dbf, { 0x95, 0xeb, 0x32, 0xab, 0x4b, 0x63, 0xd0, 0xab } };

	if(riid == ID3D11UserDefinedAnnotation_uuid)
	{
		*ppvObject = (void *)this;
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}
#endif

extern uint32_t NullCBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
extern uint32_t NullCBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

WrappedID3D11DeviceContext::WrappedID3D11DeviceContext(WrappedID3D11Device* realDevice, ID3D11DeviceContext* context,
														Serialiser *ser, Serialiser *debugser)
	: RefCounter(context), m_pDevice(realDevice), m_pRealContext(context)
{
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedID3D11DeviceContext));

	for(int i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
	{
		NullCBOffsets[i] = 0;
		NullCBCounts[i] = 4096;
	}

#if defined(INCLUDE_D3D_11_1)
	D3D11_FEATURE_DATA_D3D11_OPTIONS features;
	RDCEraseEl(features);
	HRESULT hr = m_pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &features, sizeof(features));

	m_SetCBuffer1 = false;
	if(SUCCEEDED(hr))
		m_SetCBuffer1 = features.ConstantBufferOffsetting == TRUE;
	
	m_pRealContext1 = NULL;
	m_pRealContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&m_pRealContext1);
#endif

#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	if(RenderDoc::Inst().IsReplayApp())
	{
		m_State = READING;
		m_pSerialiser = ser;
	}
	else
	{
		m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
		m_pDebugSerialiser = debugser;
		m_State = WRITING_IDLE;
	}

	// create a temporary and grab its resource ID
	m_ResourceID = TrackedResource().GetResourceID();

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

	m_DrawcallStack.push_back(&m_ParentDrawcall);

	m_CurEventID = 1;
	m_CurDrawcallID = 1;

	m_MarkerIndentLevel = 0;
#if defined(INCLUDE_D3D_11_1)
	m_UserAnnotation.SetContext(this);
#endif

	m_CurrentPipelineState = new D3D11RenderState((Serialiser *)NULL);
	m_DoStateVerify = m_State >= WRITING;

	if(context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
	{
		m_CurrentPipelineState->SetImmediatePipeline(m_pDevice);
	}
	else
	{
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

	if(m_State >= WRITING)
	{
		SAFE_DELETE(m_pSerialiser);
	}

#if defined(INCLUDE_D3D_11_1)
	SAFE_RELEASE(m_pRealContext1);
#endif

	SAFE_DELETE(m_CurrentPipelineState);
	SAFE_RELEASE(m_pRealContext);

	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
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
		state = *m_CurrentPipelineState;

		state.SetSerialiser(m_pSerialiser);

		state.MarkReferenced(this, true);
	}

	state.Serialise(m_State, m_pDevice);

	if(m_State <= EXECUTING && applyInitialState)
	{
		m_DoStateVerify = false;
		{
			*m_CurrentPipelineState = state;
			state.ApplyState(this);
		}
		m_DoStateVerify = true;
		VerifyState();
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
		m_ContextRecord->MarkResourceFrameReferenced(id, refType);
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

	// deferred contexts are initially NOT successful unless empty. That's because we don't have the serialised
	// contents of whatever is in them up until now (could be anything).
	// Only after they have been through a Finish() and then in CAPFRAME mode are they considered
	// successful.
	if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
	{
		RDCDEBUG("Deferred Context %llu Attempting capture - initially %hs, %hs", GetResourceID(), m_SuccessfulCapture ? "successful" : "unsuccessful", m_EmptyCommandList ? "empty" : "non-empty");

		m_SuccessfulCapture |= m_EmptyCommandList;

		if(m_SuccessfulCapture)
			m_FailureReason = CaptureSucceeded;
		else
			m_FailureReason = CaptureFailed_UncappedCmdlist;
		
		RDCDEBUG("Deferred Context %llu Attempting capture - now %hs", GetResourceID(), m_SuccessfulCapture ? "successful" : "unsuccessful");
	}
	else
	{
		RDCDEBUG("Immediate Context %llu Attempting capture", GetResourceID());

		m_SuccessfulCapture = true;
		m_FailureReason = CaptureSucceeded;

		for(auto it=m_DeferredRecords.begin(); it != m_DeferredRecords.end(); ++it)
			(*it)->Delete(m_pDevice->GetResourceManager());
		m_DeferredRecords.clear();

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
	if(GetType() != D3D11_DEVICE_CONTEXT_DEFERRED || !RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists)
	{
		m_State = WRITING_IDLE;

		m_SuccessfulCapture = false;
		m_FailureReason = CaptureSucceeded;
	}

	for(auto it=m_DeferredRecords.begin(); it != m_DeferredRecords.end(); ++it)
	{
		m_ContextRecord->AddParent(*it);
		(*it)->Delete(m_pDevice->GetResourceManager());
	}
	m_DeferredRecords.clear();
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
		uint64_t *stack = call->GetAddrs();

		m_pSerialiser->Serialise("callstack", stack, numLevels);

		delete call;
	}

	m_ContextRecord->AddChunk(scope.Get());
}

void WrappedID3D11DeviceContext::FreeCaptureData()
{
	for(auto it = WrappedID3D11Buffer::m_BufferList.begin(); it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
	{
		D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(it->first);

		if(record == NULL) continue;

		bool inuse = false;
		for(auto mapit=m_OpenMaps.begin(); mapit != m_OpenMaps.end(); ++mapit)
		{
			if(mapit->first.resource == it->first)
			{
				inuse = true;
				break;
			}
		}

		if(inuse) continue;

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

		for(auto it=m_MapResourceRecordAllocs.begin(); it != m_MapResourceRecordAllocs.end(); ++it)
		{
			D3D11ResourceRecord *record = m_pDevice->GetResourceManager()->GetResourceRecord(it->first);
			if(record) record->FreeContextID(it->second);
		}

		if(RenderDoc::Inst().GetCaptureOptions().CaptureAllCmdLists)
			return;
	}
	else
	{
		m_SuccessfulCapture = true;
		m_FailureReason = CaptureSucceeded;
	}

	for(auto it=m_DeferredRecords.begin(); it != m_DeferredRecords.end(); ++it)
		(*it)->Delete(m_pDevice->GetResourceManager());
	m_DeferredRecords.clear();

	m_ContextRecord->LockChunks();
	while(m_ContextRecord->HasChunks())
	{
		Chunk *chunk = m_ContextRecord->GetLastChunk();

		SAFE_DELETE(chunk);
		m_ContextRecord->PopChunk();
	}
	m_ContextRecord->UnlockChunks();

	m_ContextRecord->FreeParents(m_pDevice->GetResourceManager());

	for(auto it=m_MissingTracks.begin(); it != m_MissingTracks.end(); ++it)
	{
		if(m_pDevice->GetResourceManager()->HasResourceRecord(*it))
			m_pDevice->GetResourceManager()->MarkDirtyResource(*it);
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
	m_pDevice->GetResourceManager()->FlushPendingDirty();
}

void WrappedID3D11DeviceContext::ProcessChunk(uint64_t offset, D3D11ChunkType chunk, bool forceExecute)
{
	if(chunk < FIRST_CONTEXT_CHUNK && !forceExecute)
	{
		if(m_State == READING)
		{
			m_pDevice->GetResourceManager()->MarkInFrame(false);

			m_pDevice->ProcessChunk(offset, chunk);
			m_pSerialiser->PopContext(NULL, chunk);

			m_pDevice->GetResourceManager()->MarkInFrame(true);
		}
		else if(m_State == EXECUTING)
		{
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(NULL, chunk);
		}
		return;
	}

	m_CurChunkOffset = offset;

	RDCASSERT(GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);

	uint64_t cOffs = m_pSerialiser->GetOffset();

	ResourceId ctxId;
	m_pSerialiser->Serialise("context", ctxId);

	WrappedID3D11DeviceContext *context = (WrappedID3D11DeviceContext *)m_pDevice->GetResourceManager()->GetLiveResource(ctxId);

	if(m_FakeContext != ResourceId())
	{
		if(m_FakeContext == ctxId)
			context = this;
		else
		{
			m_pSerialiser->SetOffset(cOffs);
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(NULL, chunk);
			return;
		}
	}
	
	RDCASSERT(WrappedID3D11DeviceContext::IsAlloc(context));
	
	LogState state = context->m_State;

	if(forceExecute)
		context->m_State = EXECUTING;
	else
		context->m_State = m_State;

	m_AddedDrawcall = false;

	switch(chunk)
	{
	case SET_INPUT_LAYOUT:
		context->Serialise_IASetInputLayout(0x0);
		break;
	case SET_VBUFFER:
		context->Serialise_IASetVertexBuffers(0, 0, 0x0, 0x0, 0x0);
		break;
	case SET_IBUFFER:
		context->Serialise_IASetIndexBuffer(0, DXGI_FORMAT_UNKNOWN, 0);
		break;
	case SET_TOPOLOGY:
		context->Serialise_IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
		break;

	case SET_VS_CBUFFERS:
		context->Serialise_VSSetConstantBuffers(0, 0, 0x0);
		break;
	case SET_VS_RESOURCES:
		context->Serialise_VSSetShaderResources(0, 0, 0x0);
		break;
	case SET_VS_SAMPLERS:
		context->Serialise_VSSetSamplers(0, 0, 0x0);
		break;
	case SET_VS:
		context->Serialise_VSSetShader(0x0, 0x0, 0);
		break;

	case SET_HS_CBUFFERS:
		context->Serialise_HSSetConstantBuffers(0, 0, 0x0);
		break;
	case SET_HS_RESOURCES:
		context->Serialise_HSSetShaderResources(0, 0, 0x0);
		break;
	case SET_HS_SAMPLERS:
		context->Serialise_HSSetSamplers(0, 0, 0x0);
		break;
	case SET_HS:
		context->Serialise_HSSetShader(0x0, 0x0, 0);
		break;

	case SET_DS_CBUFFERS:
		context->Serialise_DSSetConstantBuffers(0, 0, 0x0);
		break;
	case SET_DS_RESOURCES:
		context->Serialise_DSSetShaderResources(0, 0, 0x0);
		break;
	case SET_DS_SAMPLERS:
		context->Serialise_DSSetSamplers(0, 0, 0x0);
		break;
	case SET_DS:
		context->Serialise_DSSetShader(0x0, 0x0, 0);
		break;
		
	case SET_GS_CBUFFERS:
		context->Serialise_GSSetConstantBuffers(0, 0, 0x0);
		break;
	case SET_GS_RESOURCES:
		context->Serialise_GSSetShaderResources(0, 0, 0x0);
		break;
	case SET_GS_SAMPLERS:
		context->Serialise_GSSetSamplers(0, 0, 0x0);
		break;
	case SET_GS:
		context->Serialise_GSSetShader(0x0, 0x0, 0);
		break;

	case SET_SO_TARGETS:
		context->Serialise_SOSetTargets(0, 0x0, 0x0);
		break;
		
	case SET_PS_CBUFFERS:
		context->Serialise_PSSetConstantBuffers(0, 0, 0x0);
		break;
	case SET_PS_RESOURCES:
		context->Serialise_PSSetShaderResources(0, 0, 0x0);
		break;
	case SET_PS_SAMPLERS:
		context->Serialise_PSSetSamplers(0, 0, 0x0);
		break;
	case SET_PS:
		context->Serialise_PSSetShader(0x0, 0x0, 0);
		break;

	case SET_CS_CBUFFERS:
		context->Serialise_CSSetConstantBuffers(0, 0, 0x0);
		break;
	case SET_CS_RESOURCES:
		context->Serialise_CSSetShaderResources(0, 0, 0x0);
		break;
	case SET_CS_UAVS:
		context->Serialise_CSSetUnorderedAccessViews(0, 0, 0x0, 0x0);
		break;
	case SET_CS_SAMPLERS:
		context->Serialise_CSSetSamplers(0, 0, 0x0);
		break;
	case SET_CS:
		context->Serialise_CSSetShader(0x0, 0x0, 0);
		break;

	case SET_VIEWPORTS:
		context->Serialise_RSSetViewports(0, 0x0);
		break;
	case SET_SCISSORS:
		context->Serialise_RSSetScissorRects(0, 0x0);
		break;
	case SET_RASTER:
		context->Serialise_RSSetState(0x0);
		break;

	case SET_RTARGET:
		context->Serialise_OMSetRenderTargets(0, 0x0, 0x0);
		break;
	case SET_RTARGET_AND_UAVS:
		context->Serialise_OMSetRenderTargetsAndUnorderedAccessViews(0, 0x0, 0x0, 0, 0, 0x0, 0x0);
		break;
	case SET_BLEND:
		context->Serialise_OMSetBlendState(0x0, (FLOAT*)0x0, 0);
		break;
	case SET_DEPTHSTENCIL:
		context->Serialise_OMSetDepthStencilState(0x0, 0);
		break;
		
	case DRAW_INDEXED_INST:
		context->Serialise_DrawIndexedInstanced(0, 0, 0, 0, 0);
		break;
	case DRAW_INST:
		context->Serialise_DrawInstanced(0, 0, 0, 0);
		break;
	case DRAW_INDEXED:
		context->Serialise_DrawIndexed(0, 0, 0);
		break;
	case DRAW:
		context->Serialise_Draw(0, 0);
		break;
	case DRAW_AUTO:
		context->Serialise_DrawAuto();
		break;
	case DRAW_INDEXED_INST_INDIRECT:
		context->Serialise_DrawIndexedInstancedIndirect(0x0, 0);
		break;
	case DRAW_INST_INDIRECT:
		context->Serialise_DrawInstancedIndirect(0x0, 0);
		break;

	case MAP:
		context->Serialise_Map(0, 0, (D3D11_MAP)0, 0, 0);
		break;
	case UNMAP:
		context->Serialise_Unmap(0, 0);
		break;
		
	case COPY_SUBRESOURCE_REGION:
		context->Serialise_CopySubresourceRegion(0x0, 0, 0, 0, 0, 0x0, 0, 0x0);
		break;
	case COPY_RESOURCE:
		context->Serialise_CopyResource(0x0, 0x0);
		break;
	case UPDATE_SUBRESOURCE:
		context->Serialise_UpdateSubresource(0x0, 0, 0x0, 0x0, 0, 0);
		break;
	case COPY_STRUCTURE_COUNT:
		context->Serialise_CopyStructureCount(0x0, 0, 0x0);
		break;
	case RESOLVE_SUBRESOURCE:
		context->Serialise_ResolveSubresource(0x0, 0, 0x0, 0, DXGI_FORMAT_UNKNOWN);
		break;
	case GENERATE_MIPS:
		context->Serialise_GenerateMips(0x0);
		break;

	case CLEAR_DSV:
		context->Serialise_ClearDepthStencilView(0x0, 0, 0.0f, 0);
		break;
	case CLEAR_RTV:
		context->Serialise_ClearRenderTargetView(0x0, (FLOAT*)0x0);
		break;
	case CLEAR_UAV_INT:
		context->Serialise_ClearUnorderedAccessViewUint(0x0, (UINT*)0x0);
		break;
	case CLEAR_UAV_FLOAT:
		context->Serialise_ClearUnorderedAccessViewFloat(0x0, (FLOAT*)0x0);
		break;
	case CLEAR_STATE:
		context->Serialise_ClearState();
		break;
		
	case EXECUTE_CMD_LIST:
		context->Serialise_ExecuteCommandList(0x0, 0);
		break;
	case DISPATCH:
		context->Serialise_Dispatch(0, 0, 0);
		break;
	case DISPATCH_INDIRECT:
		context->Serialise_DispatchIndirect(0x0, 0);
		break;
	case FINISH_CMD_LIST:
		context->Serialise_FinishCommandList(0, 0x0);
		break;
	case FLUSH:
		context->Serialise_Flush();
		break;
		
	case SET_PREDICATION:
		context->Serialise_SetPredication(0x0, 0x0);
		break;
	case SET_RESOURCE_MINLOD:
		context->Serialise_SetResourceMinLOD(0x0, 0);
		break;

	case BEGIN:
		context->Serialise_Begin(0x0);
		break;
	case END:
		context->Serialise_End(0x0);
		break;
		
#if defined(INCLUDE_D3D_11_1)
	case COPY_SUBRESOURCE_REGION1:
		context->Serialise_CopySubresourceRegion1(0x0, 0, 0, 0, 0, 0x0, 0, 0x0, 0);
		break;
	case UPDATE_SUBRESOURCE1:
		context->Serialise_UpdateSubresource1(0x0, 0, 0x0, 0x0, 0, 0, 0);
		break;
	case CLEAR_VIEW:
		context->Serialise_ClearView(0x0, 0x0, 0x0, 0);
		break;

	case SET_VS_CBUFFERS1:
		context->Serialise_VSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0);
		break;
	case SET_HS_CBUFFERS1:
		context->Serialise_HSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0);
		break;
	case SET_DS_CBUFFERS1:
		context->Serialise_DSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0);
		break;
	case SET_GS_CBUFFERS1:
		context->Serialise_GSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0);
		break;
	case SET_PS_CBUFFERS1:
		context->Serialise_PSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0);
		break;
	case SET_CS_CBUFFERS1:
		context->Serialise_CSSetConstantBuffers1(0, 0, 0x0, 0x0, 0x0);
		break;
#else
	case COPY_SUBRESOURCE_REGION1:
	case UPDATE_SUBRESOURCE1:
	case CLEAR_VIEW:
	case SET_VS_CBUFFERS1:
	case SET_HS_CBUFFERS1:
	case SET_DS_CBUFFERS1:
	case SET_GS_CBUFFERS1:
	case SET_PS_CBUFFERS1:
	case SET_CS_CBUFFERS1:
		RDCERR("Replaying log with D3D11.1 events on a build without D3D11.1 support");
		break;
#endif

	case BEGIN_EVENT:
		context->Serialise_BeginEvent(0, L"");
		break;
	case SET_MARKER:
		context->Serialise_SetMarker(0, L"");
		break;
	case END_EVENT:
		context->Serialise_EndEvent();
		break;

	case CONTEXT_CAPTURE_FOOTER:
		{
			bool HasCallstack = false;
			m_pSerialiser->Serialise("HasCallstack", HasCallstack);	

			if(HasCallstack)
			{
				size_t numLevels = 0;
				uint64_t *stack = NULL;

				m_pSerialiser->Serialise("callstack", stack, numLevels);

				m_pSerialiser->SetCallstack(stack, numLevels);

				SAFE_DELETE_ARRAY(stack);
			}

			if(m_State == READING)
			{
				AddEvent(CONTEXT_CAPTURE_FOOTER, "IDXGISwapChain::Present()");

				FetchDrawcall draw;
				draw.name = L"Present()";
				draw.flags |= eDraw_Present;

				AddDrawcall(draw, true);
			}
		}
		break;
	default:
		RDCERR("Unrecognised Chunk type %d", chunk);
		break;
	}

	m_pSerialiser->PopContext(NULL, chunk);
	
	if(context->m_State == READING && chunk == SET_MARKER)
	{
		// no push/pop necessary
	}
	else if(context->m_State == READING && chunk == BEGIN_EVENT)
	{
		// push down the drawcallstack to the latest drawcall
		context->m_DrawcallStack.push_back(&context->m_DrawcallStack.back()->children.back());
	}
	else if(context->m_State == READING && chunk == END_EVENT)
	{
		// refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
		RDCASSERT(context->m_DrawcallStack.size() > 1);
		if(context->m_DrawcallStack.size() > 1)
			context->m_DrawcallStack.pop_back();
	}
	else if(context->m_State == READING)
	{
		if(!m_AddedDrawcall)
			context->AddEvent(chunk, m_pSerialiser->GetDebugStr());
	}

	m_AddedDrawcall = false;
	
	if(forceExecute)
		context->m_State = state;
}

void WrappedID3D11DeviceContext::AddUsage(FetchDrawcall d)
{
	const D3D11RenderState *pipe = m_CurrentPipelineState;
	uint32_t e = d.eventID;

	if((d.flags & (eDraw_Drawcall|eDraw_Dispatch|eDraw_CmdList)) == 0)
		return;

	//////////////////////////////
	// IA

	if(d.flags & eDraw_UseIBuffer && pipe->IA.IndexBuffer != NULL)
		m_ResourceUses[GetIDForResource(pipe->IA.IndexBuffer)].push_back(EventUsage(e, eUsage_IA_IB));

	for(int i=0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++)
		if(pipe->IA.Used_VB(m_pDevice, i))
			m_ResourceUses[GetIDForResource(pipe->IA.VBs[i])].push_back(EventUsage(e, eUsage_IA_VB));
	
	//////////////////////////////
	// Shaders

	const D3D11RenderState::shader *shArr = &pipe->VS;
	for(int s=0; s < 6; s++)
	{
		const D3D11RenderState::shader &sh = shArr[s];
		
		for(int i=0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
			if(sh.Used_CB(i))
				m_ResourceUses[GetIDForResource(sh.ConstantBuffers[i])].push_back(EventUsage(e, (ResourceUsage)(eUsage_VS_CB+s)));
	
		for(int i=0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
			if(sh.Used_SRV(i))
				m_ResourceUses[((WrappedID3D11ShaderResourceView *)sh.SRVs[i])->GetResourceResID()].push_back(EventUsage(e, (ResourceUsage)(eUsage_VS_SRV+s)));
	
		if(s == 5)
		{
			for(int i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
				if(pipe->CS.Used_UAV(i) && pipe->CS.UAVs[i])
					m_ResourceUses[((WrappedID3D11UnorderedAccessView *)pipe->CS.UAVs[i])->GetResourceResID()].push_back(EventUsage(e, eUsage_CS_UAV));
		}
	}
	
	//////////////////////////////
	// SO

	for(int i=0; i < D3D11_SO_BUFFER_SLOT_COUNT; i++)
		if(pipe->SO.Buffers[i]) // assuming for now that any SO target bound is used.
			m_ResourceUses[GetIDForResource(pipe->SO.Buffers[i])].push_back(EventUsage(e, eUsage_SO));
	
	//////////////////////////////
	// OM

	for(int i=0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++)
		if(pipe->PS.Used_UAV(i) && pipe->OM.UAVs[i])
			m_ResourceUses[((WrappedID3D11UnorderedAccessView *)pipe->OM.UAVs[i])->GetResourceResID()].push_back(EventUsage(e, eUsage_PS_UAV));
	
	if(pipe->OM.DepthView) // assuming for now that any DSV bound is used.
		m_ResourceUses[((WrappedID3D11DepthStencilView *)pipe->OM.DepthView)->GetResourceResID()].push_back(EventUsage(e, eUsage_OM_DSV));

	for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		if(pipe->OM.RenderTargets[i]) // assuming for now that any RTV bound is used.
			m_ResourceUses[((WrappedID3D11RenderTargetView *)pipe->OM.RenderTargets[i])->GetResourceResID()].push_back(EventUsage(e, eUsage_OM_RTV));
}

void WrappedID3D11DeviceContext::RefreshDrawcallIDs(DrawcallTreeNode &node)
{
	if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
	{
		m_pDevice->GetImmediateContext()->RefreshDrawcallIDs(node);
		return;
	}

	// assign new drawcall IDs
	for(size_t i=0; i < node.children.size(); i++)
	{
		node.children[i].draw.drawcallID = m_CurDrawcallID++;
		RefreshDrawcallIDs(node.children[i]);
	}
}

void WrappedID3D11DeviceContext::AddDrawcall(FetchDrawcall d, bool hasEvents)
{
	if(d.context == ResourceId()) d.context = m_pDevice->GetResourceManager()->GetOriginalID(m_ResourceID);

	if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
	{
		m_pDevice->GetImmediateContext()->AddDrawcall(d, hasEvents);
		return;
	}

	m_AddedDrawcall = true;

	WrappedID3D11DeviceContext *context = (WrappedID3D11DeviceContext *)m_pDevice->GetResourceManager()->GetLiveResource(d.context);

	RDCASSERT(context);

	FetchDrawcall draw = d;
	draw.eventID = m_CurEventID;
	draw.drawcallID = m_CurDrawcallID;

	for(int i=0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
	{
		draw.outputs[i] = ResourceId();
		if(m_CurrentPipelineState->OM.RenderTargets[i])
			draw.outputs[i] = ((WrappedID3D11RenderTargetView *)m_CurrentPipelineState->OM.RenderTargets[i])->GetResourceResID();
	}

	{
		draw.depthOut = ResourceId();
		if(m_CurrentPipelineState->OM.DepthView)
			draw.depthOut = ((WrappedID3D11DepthStencilView *)m_CurrentPipelineState->OM.DepthView)->GetResourceResID();
	}

	m_CurDrawcallID++;
	if(hasEvents)
	{
		vector<FetchAPIEvent> evs;
		evs.reserve(m_CurEvents.size());
		for(size_t i=0; i < m_CurEvents.size(); )
		{
			if(m_CurEvents[i].context == draw.context)
			{
				evs.push_back(m_CurEvents[i]);
				m_CurEvents.erase(m_CurEvents.begin()+i);
			}
			else
			{
				i++;
			}
		}

		draw.events = evs;
	}

	AddUsage(draw);

	// should have at least the root drawcall here, push this drawcall
	// onto the back's children list.
	if(!context->m_DrawcallStack.empty())
	{
		DrawcallTreeNode node(draw);
		node.children.insert(node.children.begin(), draw.children.elems, draw.children.elems+draw.children.count);
		context->m_DrawcallStack.back()->children.push_back(node);
	}
	else
		RDCERR("Somehow lost drawcall stack!");
}

void WrappedID3D11DeviceContext::AddEvent(D3D11ChunkType type, string description, ResourceId ctx)
{
	if(ctx == ResourceId()) ctx = m_pDevice->GetResourceManager()->GetOriginalID(m_ResourceID);

	if(GetType() == D3D11_DEVICE_CONTEXT_DEFERRED)
	{
		m_pDevice->GetImmediateContext()->AddEvent(type, description, ctx);
		return;
	}

	FetchAPIEvent apievent;

	apievent.context = ctx;
	apievent.fileOffset = m_CurChunkOffset;
	apievent.eventID = m_CurEventID;

	apievent.eventDesc = widen(description);

	Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
	if(stack)
	{
		create_array(apievent.callstack, stack->NumLevels());
		memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t)*stack->NumLevels());
	}

	m_CurEvents.push_back(apievent);

	if(m_State == READING)
		m_Events.push_back(apievent);
}

FetchAPIEvent WrappedID3D11DeviceContext::GetEvent(uint32_t eventID)
{
	for(size_t i=m_Events.size()-1; i > 0; i--)
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

void WrappedID3D11DeviceContext::ReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial)
{
	m_State = readType;

	m_DoStateVerify = true;

	D3D11ChunkType header = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, 1, false);
	RDCASSERT(header == CONTEXT_CAPTURE_HEADER);

	ResourceId id;
	m_pSerialiser->Serialise("context", id);

	WrappedID3D11DeviceContext *context = (WrappedID3D11DeviceContext *)m_pDevice->GetResourceManager()->GetLiveResource(id);
	
	RDCASSERT(WrappedID3D11DeviceContext::IsAlloc(context) && context == this);

	Serialise_BeginCaptureFrame(!partial);

	m_pSerialiser->PopContext(NULL, header);

	m_CurEvents.clear();
	
	if(m_State == EXECUTING)
	{
		FetchAPIEvent ev = GetEvent(startEventID);
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
		for(size_t i=0; i < m_pDevice->GetNumDeferredContexts(); i++)
		{
			WrappedID3D11DeviceContext *context = m_pDevice->GetDeferredContext(i);
			context->ClearMaps();
		}
	}

	m_pDevice->GetResourceManager()->MarkInFrame(true);

	while(1)
	{
		if(m_State == EXECUTING && m_CurEventID > endEventID)
		{
			// we can just break out if we've done all the events desired.
			break;
		}

		uint64_t offset = m_pSerialiser->GetOffset();

		D3D11ChunkType context = (D3D11ChunkType)m_pSerialiser->PushContext(NULL, 1, false);

		ProcessChunk(offset, context, false);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));
		
		// for now just abort after capture scope. Really we'd need to support multiple frames
		// but for now this will do.
		if(context == CONTEXT_CAPTURE_FOOTER)
			break;
		
		m_CurEventID++;
	}

	if(m_State == READING)
	{
		m_pDevice->GetFrameRecord().back().drawcallList = m_ParentDrawcall.Bake();

		m_ParentDrawcall.children.clear();

		int initialSkips = 0;

		for(auto it=WrappedID3D11Buffer::m_BufferList.begin(); it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
			m_ResourceUses[it->first];

		for(auto it=WrappedID3D11Texture1D::m_TextureList.begin(); it != WrappedID3D11Texture1D::m_TextureList.end(); ++it)
			m_ResourceUses[it->first];
		for(auto it=WrappedID3D11Texture2D::m_TextureList.begin(); it != WrappedID3D11Texture2D::m_TextureList.end(); ++it)
			m_ResourceUses[it->first];
		for(auto it=WrappedID3D11Texture3D::m_TextureList.begin(); it != WrappedID3D11Texture3D::m_TextureList.end(); ++it)
			m_ResourceUses[it->first];

		for(auto it = m_ResourceUses.begin(); it != m_ResourceUses.end(); ++it)
		{
			ResourceId id = m_pDevice->GetResourceManager()->GetOriginalID(it->first);

			if(m_pDevice->GetResourceManager()->GetInitialContents(id) == NULL)
				continue;

			RDCDEBUG("Resource %llu", id);
			if(it->second.empty())
			{
				RDCDEBUG("Never used!");
				initialSkips++;
			}
			else
			{
				bool written = false;

				for(auto usit = it->second.begin(); usit != it->second.end(); ++usit)
				{
					ResourceUsage u = usit->usage;

					if(u == eUsage_SO ||
						u == eUsage_CS_UAV || u == eUsage_PS_UAV ||
						u == eUsage_OM_DSV || u == eUsage_OM_RTV)
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
		}

		RDCDEBUG("Can skip %d initial states.", initialSkips);
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

		ID3D11Resource *res = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetLiveResource(it->first.resource);

		m_pRealContext->Unmap(m_pDevice->GetResourceManager()->UnwrapResource(res), it->first.subresource);
	}

	m_OpenMaps.clear();
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceContext::QueryInterface( REFIID riid, void **ppvObject )
{
	//DEFINE_GUID(IID_ID3D11DeviceContext2,0x420d5b32,0xb90c,0x4da4,0xbe,0xf0,0x35,0x9f,0x6a,0x24,0xa8,0x3a);
	static const GUID ID3D11DeviceContext2_uuid = { 0x420d5b32, 0xb90c, 0x4da4, { 0xbe, 0xf0, 0x35, 0x9f, 0x6a, 0x24, 0xa8, 0x3a } };

	if(riid == __uuidof(ID3D11DeviceContext))
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
#if defined(INCLUDE_D3D_11_1)
	else if(riid == __uuidof(ID3D11DeviceContext1))
	{
		*ppvObject = (ID3D11DeviceContext1 *)this;
		AddRef();
		return S_OK;
	}
	else if(riid == __uuidof(ID3DUserDefinedAnnotation))
	{
		*ppvObject = (ID3DUserDefinedAnnotation *)&m_UserAnnotation;
		m_UserAnnotation.AddRef();
		return S_OK;
	}
#endif
	else if(riid == ID3D11DeviceContext2_uuid)
	{
		RDCWARN("Trying to get ID3D11DeviceContext2. DX11.2 not supported at this time.");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	else
	{
		string guid = ToStr::Get(riid);
		RDCWARN("Querying ID3D11DeviceContext for interface: %hs", guid.c_str());
	}

	return RefCounter::QueryInterface(riid, ppvObject);
}
