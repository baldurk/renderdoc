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


#include "replay_renderer.h"

#include <string.h>
#include <time.h>

#include "common/string_utils.h"
#include "os/os_specific.h"

#include "serialise/serialiser.h"

#include "3rdparty/jpeg-compressor/jpgd.h"
#include "3rdparty/jpeg-compressor/jpge.h"
#include "3rdparty/stb/stb_image.h"
#include "3rdparty/stb/stb_image_write.h"
#include "common/dds_readwrite.h"

ReplayRenderer::ReplayRenderer()
{
	m_pDevice = NULL;

	m_FrameID = 0;
	m_EventID = 100000;

	m_DeferredCtx = ResourceId();
	m_FirstDeferredEvent = 0;
	m_LastDeferredEvent = 0;
}

ReplayRenderer::~ReplayRenderer()
{
	for(size_t i=0; i < m_Outputs.size(); i++)
		SAFE_DELETE(m_Outputs[i]);

	m_Outputs.clear();

	for(auto it=m_CustomShaders.begin(); it != m_CustomShaders.end(); ++it)
		m_pDevice->FreeCustomShader(*it);

	m_CustomShaders.clear();

	for(auto it=m_TargetResources.begin(); it != m_TargetResources.end(); ++it)
		m_pDevice->FreeTargetResource(*it);

	m_TargetResources.clear();
	
	if(m_pDevice)
		m_pDevice->Shutdown();
	m_pDevice = NULL;
}

bool ReplayRenderer::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	if(m_DeferredCtx == ResourceId() && id == ResourceId())
		return true;

	m_pDevice->SetContextFilter(id, firstDefEv, lastDefEv);
	
	m_DeferredCtx = id;
	m_FirstDeferredEvent = firstDefEv;
	m_LastDeferredEvent = lastDefEv;

	for(size_t i=0; i < m_Outputs.size(); i++)
		m_Outputs[i]->SetContextFilter(id, firstDefEv, lastDefEv);
	
	SetFrameEvent(m_FrameID, m_EventID, true);

	return true;
}

bool ReplayRenderer::SetFrameEvent(uint32_t frameID, uint32_t eventID)
{
	return SetFrameEvent(frameID, eventID, false);
}

bool ReplayRenderer::SetFrameEvent(uint32_t frameID, uint32_t eventID, bool force)
{
	if(m_FrameID != frameID || eventID != m_EventID || force)
	{
		m_FrameID = frameID;
		m_EventID = eventID;

		m_pDevice->ReplayLog(frameID, 0, eventID, eReplay_WithoutDraw);

		FetchPipelineState();

		for(size_t i=0; i < m_Outputs.size(); i++)
			m_Outputs[i]->SetFrameEvent(frameID, eventID);
		
		m_pDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
	}

	return true;
}

bool ReplayRenderer::GetD3D11PipelineState(D3D11PipelineState *state)
{
	if(state)
	{
		*state = m_D3D11PipelineState;
		return true;
	}
	
	return false;
}

bool ReplayRenderer::GetGLPipelineState(GLPipelineState *state)
{
	if(state)
	{
		*state = m_GLPipelineState;
		return true;
	}
	
	return false;
}

bool ReplayRenderer::GetFrameInfo(rdctype::array<FetchFrameInfo> *arr)
{
	if(arr == NULL) return false;

	create_array_uninit(*arr, m_FrameRecord.size());
	for(size_t i=0; i < m_FrameRecord.size(); i++)
		arr->elems[i] = m_FrameRecord[i].frameInfo;

	return true;
}

FetchDrawcall *ReplayRenderer::GetDrawcallByEID(uint32_t eventID, uint32_t defEventID)
{
	uint32_t ev = defEventID > 0 ? defEventID : eventID;

	if(ev >= m_Drawcalls.size())
		return NULL;

	return m_Drawcalls[ev];
}

bool ReplayRenderer::GetDrawcalls(uint32_t frameID, bool includeTimes, rdctype::array<FetchDrawcall> *draws)
{
	if(frameID >= (uint32_t)m_FrameRecord.size() || draws == NULL)
		return false;

	if(includeTimes)
	{
		RDCDEBUG("Timing drawcalls...");

		m_pDevice->TimeDrawcalls(m_FrameRecord[frameID].m_DrawCallList);
	}

	*draws = m_FrameRecord[frameID].m_DrawCallList;
	return true;
}

bool ReplayRenderer::GetBuffers(rdctype::array<FetchBuffer> *out)
{
	if(m_Buffers.empty())
	{
		vector<ResourceId> ids = m_pDevice->GetBuffers();

		m_Buffers.resize(ids.size());

		for(size_t i=0; i < ids.size(); i++)
			m_Buffers[i] = m_pDevice->GetBuffer(ids[i]);
	}

	if(out)
	{
		*out = m_Buffers;
		return true;
	}

	return false;
}

bool ReplayRenderer::GetTextures(rdctype::array<FetchTexture> *out)
{
	if(m_Textures.empty())
	{
		vector<ResourceId> ids = m_pDevice->GetTextures();

		m_Textures.resize(ids.size());

		for(size_t i=0; i < ids.size(); i++)
			m_Textures[i] = m_pDevice->GetTexture(ids[i]);
	}
	
	if(out)
	{
		*out = m_Textures;
		return true;
	}

	return false;
}

bool ReplayRenderer::GetResolve(uint64_t *callstack, uint32_t callstackLen, rdctype::array<rdctype::wstr> *arr)
{
	if(arr == NULL || callstack == NULL || callstackLen == 0) return false;

	Callstack::StackResolver *resolv = m_pDevice->GetCallstackResolver();

	if(resolv == NULL)
	{
		create_array_uninit(*arr, 1);
		arr->elems[0] = L"";
		return true;
	}

	create_array_uninit(*arr, callstackLen);
	for(size_t i=0; i < callstackLen; i++)
	{
		Callstack::AddressDetails info = resolv->GetAddr(callstack[i]);
		arr->elems[i] = info.formattedString();
	}

	return true;
}

bool ReplayRenderer::GetUsage(ResourceId id, rdctype::array<EventUsage> *usage)
{
	if(usage)
	{
		*usage = m_pDevice->GetUsage(m_pDevice->GetLiveID(id));
		return true;
	}

	return false;
}

bool ReplayRenderer::GetPostVSData(MeshDataStage stage, PostVSMeshData *data)
{
	if(data == NULL) return false;

	FetchDrawcall *draw = GetDrawcallByEID(m_EventID, m_LastDeferredEvent);
	
	PostVSMeshData ret;
	ret.numVerts = 0;
	ret.topo = eTopology_Unknown;

	if(draw == NULL || (draw->flags & eDraw_Drawcall) == 0) return false;

	*data = m_pDevice->GetPostVSBuffers(m_FrameID, draw->eventID, stage);

	return true;
}

bool ReplayRenderer::GetMinMax(ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *minval, PixelValue *maxval)
{
	PixelValue *a = minval;
	PixelValue *b = maxval;

	PixelValue dummy;

	if(a == NULL) a = &dummy;
	if(b == NULL) b = &dummy;

	return m_pDevice->GetMinMax(m_pDevice->GetLiveID(tex), sliceFace, mip, sample, &a->value_f[0], &b->value_f[0]);
}

bool ReplayRenderer::GetHistogram(ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], rdctype::array<uint32_t> *histogram)
{
	if(histogram == NULL) return false;

	vector<uint32_t> hist;

	bool ret = m_pDevice->GetHistogram(m_pDevice->GetLiveID(tex), sliceFace, mip, sample, minval, maxval, channels, hist);

	if(ret)
		*histogram = hist;

	return ret;
}

bool ReplayRenderer::GetBufferData(ResourceId buff, uint32_t offset, uint32_t len, rdctype::array<byte> *data)
{
	if(data == NULL) return false;

	*data = m_pDevice->GetBufferData(m_pDevice->GetLiveID(buff), offset, len);

	return true;
}

bool ReplayRenderer::SaveTexture(ResourceId tex, uint32_t saveMip, const wchar_t *path)
{
	return m_pDevice->SaveTexture(m_pDevice->GetLiveID(tex), saveMip, path);
}

bool ReplayRenderer::PixelHistory(ResourceId target, uint32_t x, uint32_t y, rdctype::array<PixelModification> *history)
{
	bool outofbounds = false;
	
	for(size_t t=0; t < m_Textures.size(); t++)
	{
		if(m_Textures[t].ID == target)
		{
			if(x >= m_Textures[t].width || y >= m_Textures[t].height)
			{
				RDCDEBUG("PixelHistory out of bounds on %llx (%u,%u) vs (%u,%u)", target, x, y, m_Textures[t].width, m_Textures[t].height);
				history->count = 0;
				history->elems = NULL;
				return false;
			}

			break;
		}
	}

	auto usage = m_pDevice->GetUsage(m_pDevice->GetLiveID(target));

	vector<EventUsage> events;

	for(size_t i=0; i < usage.size(); i++)
	{
		if(usage[i].eventID > m_EventID)
			continue;

		switch(usage[i].usage)
		{
			case eUsage_IA_VB:
			case eUsage_IA_IB:
			case eUsage_VS_CB:
			case eUsage_HS_CB:
			case eUsage_DS_CB:
			case eUsage_GS_CB:
			case eUsage_PS_CB:
			case eUsage_CS_CB:
			case eUsage_VS_SRV:
			case eUsage_HS_SRV:
			case eUsage_DS_SRV:
			case eUsage_GS_SRV:
			case eUsage_PS_SRV:
			case eUsage_CS_SRV:
				// read-only, not a valid pixel history event
				continue;
			
			case eUsage_None:
			case eUsage_SO:
			case eUsage_CS_UAV:
			case eUsage_PS_UAV:
			case eUsage_OM_RTV:
			case eUsage_OM_DSV:
			case eUsage_Clear:
				// writing - include in pixel history events
				break;
		}

		events.push_back(usage[i]);
	}
	
	if(events.empty())
	{
		RDCDEBUG("Target %llx not written to before %u", target, m_EventID);
		history->count = 0;
		history->elems = NULL;
		return false;
	}

	*history = m_pDevice->PixelHistory(m_FrameID, events, m_pDevice->GetLiveID(target), x, y);
	
	SetFrameEvent(m_FrameID, m_EventID, true);

	return true;
}

bool ReplayRenderer::VSGetDebugStates(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace)
{
	if(trace == NULL) return false;

	*trace = m_pDevice->DebugVertex(m_FrameID, m_EventID, vertid, instid, idx, instOffset, vertOffset);

	SetFrameEvent(m_FrameID, m_EventID, true);

	return true;
}

bool ReplayRenderer::PSGetDebugStates(uint32_t x, uint32_t y, ShaderDebugTrace *trace)
{
	if(trace == NULL) return false;

	*trace = m_pDevice->DebugPixel(m_FrameID, m_EventID, x, y);
	
	SetFrameEvent(m_FrameID, m_EventID, true);

	return true;
}

bool ReplayRenderer::CSGetDebugStates(uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace)
{
	if(trace == NULL) return false;

	*trace = m_pDevice->DebugThread(m_FrameID, m_EventID, groupid, threadid);
	
	SetFrameEvent(m_FrameID, m_EventID, true);

	return true;
}

bool ReplayRenderer::GetCBufferVariableContents(ResourceId shader, uint32_t cbufslot, ResourceId buffer, uint32_t offs, rdctype::array<ShaderVariable> *vars)
{
	if(vars == NULL) return false;

	vector<byte> data;
	if(buffer != ResourceId())
		data = m_pDevice->GetBufferData(m_pDevice->GetLiveID(buffer), offs, 0);

	vector<ShaderVariable> v;

	m_pDevice->FillCBufferVariables(m_pDevice->GetLiveID(shader), cbufslot, v, data);

	*vars = v;

	return true;
}

ShaderReflection *ReplayRenderer::GetShaderDetails(ResourceId shader)
{
	return m_pDevice->GetShader(m_pDevice->GetLiveID(shader));
}

ReplayOutput *ReplayRenderer::CreateOutput(void *wndhandle)
{
	ReplayOutput *out = new ReplayOutput(this, wndhandle);

	m_Outputs.push_back(out);

	m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_WithoutDraw);
	
	out->SetFrameEvent(m_FrameID, m_EventID);

	m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_OnlyDraw);

	return out;
}

ResourceId ReplayRenderer::BuildTargetShader(const wchar_t *entry, const wchar_t *source, const uint32_t compileFlags, ShaderStageType type, rdctype::wstr *errors)
{
	ResourceId id;
	string errs;
	
	switch(type)
	{
		case eShaderStage_Vertex:
		case eShaderStage_Hull:
		case eShaderStage_Domain:
		case eShaderStage_Geometry:
		case eShaderStage_Pixel:
		case eShaderStage_Compute:
			break;
		default:
			RDCERR("Unexpected type in BuildShader!");
			return ResourceId();
	}

	m_pDevice->BuildTargetShader(narrow(source), narrow(entry), compileFlags, type, &id, &errs);

	if(id != ResourceId())
		m_TargetResources.insert(id);
	
	if(errors) *errors = widen(errs);

	return id;
}

ResourceId ReplayRenderer::BuildCustomShader(const wchar_t *entry, const wchar_t *source, const uint32_t compileFlags, ShaderStageType type, rdctype::wstr *errors)
{
	ResourceId id;
	string errs;
	
	switch(type)
	{
		case eShaderStage_Vertex:
		case eShaderStage_Hull:
		case eShaderStage_Domain:
		case eShaderStage_Geometry:
		case eShaderStage_Pixel:
		case eShaderStage_Compute:
			break;
		default:
			RDCERR("Unexpected type in BuildShader!");
			return ResourceId();
	}

	m_pDevice->BuildCustomShader(narrow(source), narrow(entry), compileFlags, type, &id, &errs);

	if(id != ResourceId())
		m_CustomShaders.insert(id);
	
	if(errors) *errors = widen(errs);

	return id;
}

bool ReplayRenderer::FreeTargetResource(ResourceId id)
{
	m_TargetResources.erase(id);
	m_pDevice->FreeTargetResource(id);

	return true;
}

bool ReplayRenderer::FreeCustomShader(ResourceId id)
{
	m_CustomShaders.erase(id);
	m_pDevice->FreeCustomShader(id);

	return true;
}

bool ReplayRenderer::ReplaceResource(ResourceId from, ResourceId to)
{
	m_pDevice->ReplaceResource(from, to);

	SetFrameEvent(m_FrameID, m_EventID, true);
	
	for(size_t i=0; i < m_Outputs.size(); i++)
		if(m_Outputs[i]->GetType() != eOutputType_None)
			m_Outputs[i]->Display();

	return true;
}

bool ReplayRenderer::RemoveReplacement(ResourceId id)
{
	m_pDevice->RemoveReplacement(id);

	SetFrameEvent(m_FrameID, m_EventID, true);
	
	for(size_t i=0; i < m_Outputs.size(); i++)
		if(m_Outputs[i]->GetType() != eOutputType_None)
			m_Outputs[i]->Display();

	return true;
}

ReplayCreateStatus ReplayRenderer::CreateDevice(const wchar_t *logfile)
{
	RDCLOG("Creating replay device for %ls", logfile);

	RDCDriver driverType = RDC_Unknown;
	wstring driverName = L"";
	auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, NULL);

	if(driverType == RDC_Unknown || driverName == L"" || status != eReplayCreate_Success)
	{
		RDCERR("Couldn't get device type from log");
		return status;
	}

	IReplayDriver *driver = NULL;
	status = RenderDoc::Inst().CreateReplayDriver(driverType, logfile, &driver);

	if(driver && status == eReplayCreate_Success)
	{
		RDCLOG("Created replay driver.");
		return PostCreateInit(driver);
	}
	
	RDCERR("Couldn't create a replay device :(.");
	return status;
}

ReplayCreateStatus ReplayRenderer::SetDevice(IReplayDriver *device)
{
	if(device)
	{
		RDCLOG("Got replay driver.");
		return PostCreateInit(device);
	}
	
	RDCERR("Given invalid replay driver.");
	return eReplayCreate_InternalError;
}

ReplayCreateStatus ReplayRenderer::PostCreateInit(IReplayDriver *device)
{
	m_pDevice = device;

	m_pDevice->ReadLogInitialisation();
	
	FetchPipelineState();

	vector<FetchFrameRecord> fr = m_pDevice->GetFrameRecord();

	m_FrameRecord.reserve(fr.size());
	for(size_t i=0; i < fr.size(); i++)
	{
		m_FrameRecord.push_back(FrameRecord());
		m_FrameRecord.back().frameInfo = fr[i].frameInfo;
		m_FrameRecord.back().m_DrawCallList = fr[i].drawcallList;
		
		SetupDrawcallPointers(fr[i].frameInfo, m_FrameRecord.back().m_DrawCallList, NULL, NULL);
	}

	return eReplayCreate_Success;
}

FetchDrawcall *ReplayRenderer::SetupDrawcallPointers(FetchFrameInfo frame, rdctype::array<FetchDrawcall> &draws, FetchDrawcall *parent, FetchDrawcall *previous)
{
	FetchDrawcall *ret = NULL;

	for(int32_t i=0; i < draws.count; i++)
	{
		FetchDrawcall *draw = &draws[i];

		draw->parent = parent ? parent->eventID : 0;

		if(draw->children.count > 0)
		{
			ret = previous = SetupDrawcallPointers(frame, draw->children, draw, previous);
		}
		else if(draw->flags & (eDraw_PushMarker|eDraw_SetMarker|eDraw_Present))
		{
			// don't want to set up previous/next links for markers
		}
		else
		{
			if(previous != NULL)
				previous->next = draw->eventID;
			draw->previous = previous ? previous->eventID : 0;

			RDCASSERT(m_Drawcalls.empty() || draw->eventID > m_Drawcalls.back()->eventID || draw->context != frame.immContextId);
			m_Drawcalls.resize(RDCMAX(m_Drawcalls.size(), size_t(draw->eventID+1)));
			m_Drawcalls[draw->eventID] = draw;

			ret = previous = draw;
		}
	}

	return ret;
}

bool ReplayRenderer::HasCallstacks()
{
	return m_pDevice->HasCallstacks();
}

APIProperties ReplayRenderer::GetAPIProperties()
{
	return m_pDevice->GetAPIProperties();
}

bool ReplayRenderer::InitResolver()
{
	m_pDevice->InitCallstackResolver();
	return m_pDevice->GetCallstackResolver() != NULL;
}

void ReplayRenderer::FetchPipelineState()
{
	m_pDevice->SavePipelineState();

	m_D3D11PipelineState = m_pDevice->GetD3D11PipelineState();
	m_GLPipelineState = m_pDevice->GetGLPipelineState();
	
	{
		D3D11PipelineState::ShaderStage *stage = &m_D3D11PipelineState.m_VS;
		for(int i=0; i < 6; i++)
			if(stage[i].Shader != ResourceId())
				stage[i].ShaderDetails = m_pDevice->GetShader(m_pDevice->GetLiveID(stage[i].Shader));
	}
	
	{
		GLPipelineState::ShaderStage *stage = &m_GLPipelineState.m_VS;
		for(int i=0; i < 6; i++)
			if(stage[i].Shader != ResourceId())
				stage[i].ShaderDetails = m_pDevice->GetShader(m_pDevice->GetLiveID(stage[i].Shader));
	}
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_GetAPIProperties(ReplayRenderer *rend, APIProperties *props)
{ if(props) *props = rend->GetAPIProperties(); }

extern "C" RENDERDOC_API ReplayOutput* RENDERDOC_CC ReplayRenderer_CreateOutput(ReplayRenderer *rend, void *handle)
{ return rend->CreateOutput(handle); }
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_Shutdown(ReplayRenderer *rend)
{ delete rend; }
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_ShutdownOutput(ReplayRenderer *rend, ReplayOutput *output)
{ RDCUNIMPLEMENTED("destroying individual outputs"); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_HasCallstacks(ReplayRenderer *rend)
{ return rend->HasCallstacks(); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_InitResolver(ReplayRenderer *rend)
{ return rend->InitResolver(); }
 
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_SetContextFilter(ReplayRenderer *rend, ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{ return rend->SetContextFilter(id, firstDefEv, lastDefEv); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_SetFrameEvent(ReplayRenderer *rend, uint32_t frameID, uint32_t eventID)
{ return rend->SetFrameEvent(frameID, eventID); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetD3D11PipelineState(ReplayRenderer *rend, D3D11PipelineState *state)
{ return rend->GetD3D11PipelineState(state); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetGLPipelineState(ReplayRenderer *rend, GLPipelineState *state)
{ return rend->GetGLPipelineState(state); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_BuildCustomShader(ReplayRenderer *rend, const wchar_t *entry, const wchar_t *source, const uint32_t compileFlags, ShaderStageType type, ResourceId *shaderID, rdctype::wstr *errors)
{
	if(shaderID == NULL) return false;

	*shaderID = rend->BuildCustomShader(entry, source, compileFlags, type, errors);

	return (*shaderID != ResourceId());
}
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_FreeCustomShader(ReplayRenderer *rend, ResourceId id)
{ return rend->FreeCustomShader(id); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_BuildTargetShader(ReplayRenderer *rend, const wchar_t *entry, const wchar_t *source, const uint32_t compileFlags, ShaderStageType type, ResourceId *shaderID, rdctype::wstr *errors)
{
	if(shaderID == NULL) return false;

	*shaderID = rend->BuildTargetShader(entry, source, compileFlags, type, errors);

	return (*shaderID != ResourceId());
}
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_ReplaceResource(ReplayRenderer *rend, ResourceId from, ResourceId to)
{ return rend->ReplaceResource(from, to); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_RemoveReplacement(ReplayRenderer *rend, ResourceId id)
{ return rend->RemoveReplacement(id); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_FreeTargetResource(ReplayRenderer *rend, ResourceId id)
{ return rend->FreeTargetResource(id); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetFrameInfo(ReplayRenderer *rend, rdctype::array<FetchFrameInfo> *frame)
{ return rend->GetFrameInfo(frame); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetDrawcalls(ReplayRenderer *rend, uint32_t frameID, bool includeTimes, rdctype::array<FetchDrawcall> *draws)
{ return rend->GetDrawcalls(frameID, includeTimes, draws); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetTextures(ReplayRenderer *rend, rdctype::array<FetchTexture> *texs)
{ return rend->GetTextures(texs); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetBuffers(ReplayRenderer *rend, rdctype::array<FetchBuffer> *bufs)
{ return rend->GetBuffers(bufs); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetResolve(ReplayRenderer *rend, uint64_t *callstack, uint32_t callstackLen, rdctype::array<rdctype::wstr> *trace)
{ return rend->GetResolve(callstack, callstackLen, trace); }
extern "C" RENDERDOC_API ShaderReflection* RENDERDOC_CC ReplayRenderer_GetShaderDetails(ReplayRenderer *rend, ResourceId shader)
{ return rend->GetShaderDetails(shader); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_PixelHistory(ReplayRenderer *rend, ResourceId target, uint32_t x, uint32_t y, rdctype::array<PixelModification> *history)
{ return rend->PixelHistory(target, x, y, history); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_VSGetDebugStates(ReplayRenderer *rend, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace)
{ return rend->VSGetDebugStates(vertid, instid, idx, instOffset, vertOffset, trace); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_PSGetDebugStates(ReplayRenderer *rend, uint32_t x, uint32_t y, ShaderDebugTrace *trace)
{ return rend->PSGetDebugStates(x, y, trace); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_CSGetDebugStates(ReplayRenderer *rend, uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace)
{ return rend->CSGetDebugStates(groupid, threadid, trace); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetUsage(ReplayRenderer *rend, ResourceId id, rdctype::array<EventUsage> *usage)
{ return rend->GetUsage(id, usage); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetCBufferVariableContents(ReplayRenderer *rend, ResourceId shader, uint32_t cbufslot, ResourceId buffer, uint32_t offs, rdctype::array<ShaderVariable> *vars)
{ return rend->GetCBufferVariableContents(shader, cbufslot, buffer, offs, vars); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_SaveTexture(ReplayRenderer *rend, ResourceId texID, uint32_t mip, const wchar_t *path)
{ return rend->SaveTexture(texID, mip, path); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetPostVSData(ReplayRenderer *rend, MeshDataStage stage, PostVSMeshData *data)
{ return rend->GetPostVSData(stage, data); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetMinMax(ReplayRenderer *rend, ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *minval, PixelValue *maxval)
{ return rend->GetMinMax(tex, sliceFace, mip, sample, minval, maxval); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetHistogram(ReplayRenderer *rend, ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], rdctype::array<uint32_t> *histogram)
{ return rend->GetHistogram(tex, sliceFace, mip, sample, minval, maxval, channels, histogram); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayRenderer_GetBufferData(ReplayRenderer *rend, ResourceId buff, uint32_t offset, uint32_t len, rdctype::array<byte> *data)
{ return rend->GetBufferData(buff, offset, len, data); }
