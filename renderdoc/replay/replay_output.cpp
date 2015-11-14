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


#include "common/common.h"

#include "replay_renderer.h"

#include "serialise/string_utils.h"
#include "maths/matrix.h"

ReplayOutput::ReplayOutput(ReplayRenderer *parent, void *w)
{
	m_pRenderer = parent;

	m_MainOutput.dirty = true;

	m_OverlayDirty = true;
	m_ForceOverlayRefresh = false;

	m_pDevice = parent->GetDevice();

	m_OverlayResourceId = ResourceId();

	RDCEraseEl(m_RenderData);
	
	m_PixelContext.wndHandle = 0;
	m_PixelContext.outputID = 0;
	m_PixelContext.texture = ResourceId();
	m_PixelContext.depthMode = false;

	m_ContextX = -1.0f;
	m_ContextY = -1.0f;

	m_Config.m_Type = eOutputType_None;

	if(w) m_MainOutput.outputID = m_pDevice->MakeOutputWindow(w, true);
	else m_MainOutput.outputID = 0;
	m_MainOutput.texture = ResourceId();
	
	m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);

	m_FirstDeferredEvent = 0;
	m_LastDeferredEvent = 0;

	m_CustomShaderResourceId = ResourceId();
}

ReplayOutput::~ReplayOutput()
{
	m_pDevice->DestroyOutputWindow(m_MainOutput.outputID);
	m_pDevice->DestroyOutputWindow(m_PixelContext.outputID);

	m_CustomShaderResourceId = ResourceId();
	
	ClearThumbnails();
}

bool ReplayOutput::SetOutputConfig( const OutputConfig &o )
{
	m_OverlayDirty = true;
	m_Config = o;
	m_MainOutput.dirty = true;
	return true;
}

bool ReplayOutput::SetTextureDisplay(const TextureDisplay &o)
{
	if(o.overlay != m_RenderData.texDisplay.overlay)
	{
		if(m_RenderData.texDisplay.overlay == eTexOverlay_ClearBeforeDraw ||
			m_RenderData.texDisplay.overlay == eTexOverlay_ClearBeforePass)
		{
			// by necessity these overlays modify the actual texture, not an
			// independent overlay texture. So if we disable them, we must
			// refresh the log.
			m_ForceOverlayRefresh = true;
		}
		m_OverlayDirty = true;
	}
	m_RenderData.texDisplay = o;
	m_MainOutput.dirty = true;
	return true;
}

bool ReplayOutput::SetMeshDisplay(const MeshDisplay &o)
{
	if(o.thisDrawOnly!= m_RenderData.meshDisplay.thisDrawOnly)
		m_OverlayDirty = true;
	m_RenderData.meshDisplay = o;
	m_MainOutput.dirty = true;
	return true;
}

void ReplayOutput::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	m_FirstDeferredEvent = firstDefEv;
	m_LastDeferredEvent = lastDefEv;
}

void ReplayOutput::SetFrameEvent(int frameID, int eventID)
{
	m_FrameID = frameID;
	m_EventID = eventID;

	m_OverlayDirty = true;
	m_MainOutput.dirty = true;
	
	for(size_t i=0; i < m_Thumbnails.size(); i++)
		m_Thumbnails[i].dirty = true;

	RefreshOverlay();
}

void ReplayOutput::RefreshOverlay()
{
	FetchDrawcall *draw = m_pRenderer->GetDrawcallByEID(m_EventID, m_LastDeferredEvent);
	
	{
		passEvents.clear();

		FetchDrawcall *start = draw;
		while(start && start->previous != 0 && (m_pRenderer->GetDrawcallByEID((uint32_t)start->previous, 0)->flags & eDraw_Clear) == 0)
		{
			FetchDrawcall *prev = m_pRenderer->GetDrawcallByEID((uint32_t)start->previous, 0);

			if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) || start->depthOut != prev->depthOut)
				break;

			start = prev;
		}

		while(start)
		{
			if(start == draw)
				break;

			if(start->flags & eDraw_Drawcall)
			{
				passEvents.push_back(start->eventID);
			}

			start = m_pRenderer->GetDrawcallByEID((uint32_t)start->next, 0);
		}
	}

	if(m_Config.m_Type == eOutputType_TexDisplay && m_RenderData.texDisplay.overlay != eTexOverlay_None)
	{
		if(draw && m_pDevice->IsRenderOutput(m_RenderData.texDisplay.texid))
		{
			m_OverlayResourceId = m_pDevice->RenderOverlay(m_pDevice->GetLiveID(m_RenderData.texDisplay.texid), m_RenderData.texDisplay.overlay,
			                                               m_FrameID, m_EventID, passEvents);
			m_OverlayDirty = false;
		}
	}

	if(m_Config.m_Type == eOutputType_MeshDisplay && m_OverlayDirty)
	{
		m_OverlayDirty = false;

		if(draw == NULL || (draw->flags & eDraw_Drawcall) == 0)
			return;
		
		m_pDevice->InitPostVSBuffers(m_FrameID, draw->eventID);

		if(!m_RenderData.meshDisplay.thisDrawOnly && !passEvents.empty())
		{
			uint32_t prev = 0;

			for(size_t i=0; i < passEvents.size(); i++)
			{
				if(prev != passEvents[i])
				{
					m_pDevice->ReplayLog(m_FrameID, prev, passEvents[i], eReplay_WithoutDraw);

					prev = passEvents[i];
				}

				FetchDrawcall *d = m_pRenderer->GetDrawcallByEID(m_EventID, m_LastDeferredEvent);

				if(d)
				{
					m_pDevice->InitPostVSBuffers(m_FrameID, passEvents[i]);
				}
			}

			m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_WithoutDraw);
		}
	}
}

bool ReplayOutput::ClearThumbnails()
{
	for(size_t i=0; i < m_Thumbnails.size(); i++)
		m_pDevice->DestroyOutputWindow(m_Thumbnails[i].outputID);

	m_Thumbnails.clear();

	return true;
}
	
bool ReplayOutput::SetPixelContext(void *wnd)
{
	m_PixelContext.wndHandle = wnd;
	m_PixelContext.outputID = m_pDevice->MakeOutputWindow(m_PixelContext.wndHandle, false);
	m_PixelContext.texture = ResourceId();
	m_PixelContext.depthMode = false;

	RDCASSERT(m_PixelContext.outputID > 0);

	return true;
}

bool ReplayOutput::AddThumbnail(void *wnd, ResourceId texID)
{
	OutputPair p;

	RDCASSERT(wnd);
	
	bool depthMode = false;

	for(size_t t=0; t < m_pRenderer->m_Textures.size(); t++)
	{
		if(m_pRenderer->m_Textures[t].ID == texID)
		{
			depthMode = (m_pRenderer->m_Textures[t].creationFlags & eTextureCreate_DSV) > 0;
			break;
		}
	}

	for(size_t i=0; i < m_Thumbnails.size(); i++)
	{
		if(m_Thumbnails[i].wndHandle == wnd)
		{
			m_Thumbnails[i].texture = texID;

			m_Thumbnails[i].depthMode = depthMode;

			m_Thumbnails[i].dirty = true;

			return true;
		}
	}

	p.wndHandle = wnd;
	p.outputID = m_pDevice->MakeOutputWindow(p.wndHandle, false);
	p.texture = texID;
	p.depthMode = depthMode;
	p.dirty = true;

	RDCASSERT(p.outputID > 0);

	m_Thumbnails.push_back(p);

	return true;
}

bool ReplayOutput::PickPixel(ResourceId tex, bool customShader, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *ret)
{
	if(ret == NULL || tex == ResourceId()) return false;

	RDCEraseEl(ret->value_f);

	bool decodeRamp = false;

	if(customShader && m_RenderData.texDisplay.CustomShader != ResourceId() && m_CustomShaderResourceId != ResourceId())
	{
		tex = m_CustomShaderResourceId;
	}
	if((m_RenderData.texDisplay.overlay == eTexOverlay_QuadOverdrawDraw || 
		  m_RenderData.texDisplay.overlay == eTexOverlay_QuadOverdrawPass) &&
			m_OverlayResourceId != ResourceId())
	{
		decodeRamp = true;
		tex = m_OverlayResourceId;
	}

	m_pDevice->PickPixel(m_pDevice->GetLiveID(tex), x, y, sliceFace, mip, sample, ret->value_f);
	
	if(decodeRamp)
	{
		for(size_t c=0; c < ARRAY_COUNT(overdrawRamp); c++)
		{
			if(fabs(ret->value_f[0] - overdrawRamp[c].x) < 0.00004f &&
				 fabs(ret->value_f[1] - overdrawRamp[c].y) < 0.00004f &&
				 fabs(ret->value_f[2] - overdrawRamp[c].z) < 0.00004f)
			{
				ret->value_i[0] = (int32_t)c;
				ret->value_i[1] = 0;
				ret->value_i[2] = 0;
				ret->value_i[3] = 0;
				break;
			}
		}
	}

	return true;
}

uint32_t ReplayOutput::PickVertex(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y)
{
	FetchDrawcall *draw = m_pRenderer->GetDrawcallByEID(eventID, 0);

	if(!draw) return ~0U;
	if(m_RenderData.meshDisplay.type == eMeshDataStage_Unknown) return ~0U;
	if((draw->flags & eDraw_Drawcall) == 0) return ~0U;

	MeshDisplay cfg = m_RenderData.meshDisplay;
	cfg.position.buf = m_pDevice->GetLiveID(cfg.position.buf);
	cfg.position.idxbuf = m_pDevice->GetLiveID(cfg.position.idxbuf);
	cfg.second.buf = m_pDevice->GetLiveID(cfg.second.buf);
	cfg.second.idxbuf = m_pDevice->GetLiveID(cfg.second.idxbuf);

	return m_pDevice->PickVertex(m_FrameID, m_EventID, cfg, x, y);
}

bool ReplayOutput::SetPixelContextLocation(uint32_t x, uint32_t y)
{
	m_ContextX = RDCMAX((float)x, 0.0f);
	m_ContextY = RDCMAX((float)y, 0.0f);

	DisplayContext();

	return true;
}

void ReplayOutput::DisablePixelContext()
{
	m_ContextX = -1.0f;
	m_ContextY = -1.0f;

	DisplayContext();
}

void ReplayOutput::DisplayContext()
{
	if(m_PixelContext.outputID == 0) return;
	float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);
	m_pDevice->ClearOutputWindowColour(m_PixelContext.outputID, color);
	
	if(m_Config.m_Type != eOutputType_TexDisplay) return;
	if(m_ContextX < 0.0f && m_ContextY < 0.0f) return;
	if(m_RenderData.texDisplay.texid == ResourceId()) return;

	TextureDisplay disp = m_RenderData.texDisplay;
	disp.rawoutput = false;
	disp.CustomShader = ResourceId();

	if(m_RenderData.texDisplay.CustomShader != ResourceId())
		disp.texid = m_CustomShaderResourceId;
	
	if((m_RenderData.texDisplay.overlay == eTexOverlay_QuadOverdrawDraw || 
		  m_RenderData.texDisplay.overlay == eTexOverlay_QuadOverdrawPass) &&
			m_OverlayResourceId != ResourceId())
		disp.texid = m_OverlayResourceId;

	disp.scale = 8.0f;

	int32_t width = 0, height = 0;
	m_pDevice->GetOutputWindowDimensions(m_PixelContext.outputID, width, height);

	float w = (float)width;
	float h = (float)height;

	disp.offx = -m_ContextX*disp.scale;
	disp.offy = -m_ContextY*disp.scale;

	disp.offx += w/2.0f;
	disp.offy += h/2.0f;

	disp.texid = m_pDevice->GetLiveID(disp.texid);

	m_pDevice->RenderTexture(disp);

	m_pDevice->RenderHighlightBox(w, h, disp.scale);

	m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
}

bool ReplayOutput::Display()
{
	if(m_pDevice->CheckResizeOutputWindow(m_MainOutput.outputID))
	{
		m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);
		m_MainOutput.dirty = true;
	}
	
	for(size_t i=0; i < m_Thumbnails.size(); i++)
		if(m_pDevice->CheckResizeOutputWindow(m_Thumbnails[i].outputID))
			m_Thumbnails[i].dirty = true;

	for(size_t i=0; i < m_Thumbnails.size(); i++)
	{
		if(!m_Thumbnails[i].dirty)
		{
			m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
			continue;
		}
		if(!m_pDevice->IsOutputWindowVisible(m_Thumbnails[i].outputID))
			continue;
		
		float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		if(m_Thumbnails[i].texture == ResourceId())
		{
			m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);

			color[0] = 0.4f;
			m_pDevice->ClearOutputWindowColour(m_Thumbnails[i].outputID, color);

			m_pDevice->RenderCheckerboard(Vec3f(0.6f, 0.6f, 0.7f), Vec3f(0.5f, 0.5f, 0.6f));
		
			m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
			continue;
		}
		
		m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);
		m_pDevice->ClearOutputWindowColour(m_Thumbnails[i].outputID, color);

		TextureDisplay disp;

		disp.Red = disp.Green = disp.Blue = true;
		disp.Alpha = false;
		disp.HDRMul = -1.0f;
		disp.linearDisplayAsGamma = true;
		disp.FlipY = false;
		disp.mip = 0;
		disp.sampleIdx = ~0U;
		disp.CustomShader = ResourceId();
		disp.texid = m_pDevice->GetLiveID(m_Thumbnails[i].texture);
		disp.scale = -1.0f;
		disp.rangemin = 0.0f; disp.rangemax = 1.0f;
		disp.sliceFace = 0;
		disp.rawoutput = false;

		if(m_Thumbnails[i].depthMode)
			disp.Green = disp.Blue = false;

		m_pDevice->RenderTexture(disp);

		m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);

		m_Thumbnails[i].dirty = false;
	}
	
	if(m_pDevice->CheckResizeOutputWindow(m_PixelContext.outputID))
		m_MainOutput.dirty = true;

	if(!m_MainOutput.dirty)
	{
		m_pDevice->FlipOutputWindow(m_MainOutput.outputID);
		m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
		return true;
	}

	m_MainOutput.dirty = false;
	
	switch(m_Config.m_Type)
	{
		case eOutputType_MeshDisplay:
			DisplayMesh();
			break;
		case eOutputType_TexDisplay:
			DisplayTex();
			break;
		default:
			RDCERR("Unexpected display type! %d", m_Config.m_Type);
			break;
	}
	
	m_pDevice->FlipOutputWindow(m_MainOutput.outputID);

	DisplayContext();

	return true;
}

void ReplayOutput::DisplayTex()
{
	FetchDrawcall *draw = m_pRenderer->GetDrawcallByEID(m_EventID, m_LastDeferredEvent);
	
	if(m_MainOutput.outputID == 0) return;
	if(m_RenderData.texDisplay.texid == ResourceId())
	{
		float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
		m_pDevice->ClearOutputWindowColour(m_MainOutput.outputID, color);
		return;
	}
	if(m_Width <= 0 || m_Height <= 0) return;
	
	TextureDisplay texDisplay = m_RenderData.texDisplay;
	texDisplay.rawoutput = false;
	texDisplay.texid = m_pDevice->GetLiveID(texDisplay.texid);

	if(m_RenderData.texDisplay.overlay != eTexOverlay_None && draw)
	{
		if(m_OverlayDirty)
		{
			m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_WithoutDraw);
			RefreshOverlay();
			m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_OnlyDraw);
		}
	}
	else if(m_ForceOverlayRefresh)
	{
		m_ForceOverlayRefresh = false;
		m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_Full);
	}
		
	if(m_RenderData.texDisplay.CustomShader != ResourceId())
	{
		m_CustomShaderResourceId = m_pDevice->ApplyCustomShader(m_RenderData.texDisplay.CustomShader, texDisplay.texid, texDisplay.mip);

		texDisplay.texid = m_pDevice->GetLiveID(m_CustomShaderResourceId);
		texDisplay.CustomShader = ResourceId();
		texDisplay.sliceFace = 0;
		texDisplay.mip = 0;
		texDisplay.linearDisplayAsGamma = false;
	}
	
	float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	
	m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
	m_pDevice->ClearOutputWindowColour(m_MainOutput.outputID, color);

	m_pDevice->RenderCheckerboard(Vec3f(texDisplay.lightBackgroundColour.x, texDisplay.lightBackgroundColour.y, texDisplay.lightBackgroundColour.z),
	                              Vec3f(texDisplay.darkBackgroundColour.x,  texDisplay.darkBackgroundColour.y,  texDisplay.darkBackgroundColour.z));

	m_pDevice->RenderTexture(texDisplay);
	
	if(m_RenderData.texDisplay.overlay != eTexOverlay_None && draw && m_pDevice->IsRenderOutput(m_RenderData.texDisplay.texid) &&
		m_RenderData.texDisplay.overlay != eTexOverlay_NaN &&
		m_RenderData.texDisplay.overlay != eTexOverlay_Clipping)
	{
		RDCASSERT(m_OverlayResourceId != ResourceId());
		texDisplay.texid = m_pDevice->GetLiveID(m_OverlayResourceId);
		texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
		texDisplay.rawoutput = false;
		texDisplay.CustomShader = ResourceId();
		texDisplay.scale = m_RenderData.texDisplay.scale;
		texDisplay.HDRMul = -1.0f;
		texDisplay.FlipY = m_RenderData.texDisplay.FlipY;
		texDisplay.rangemin = 0.0f;
		texDisplay.rangemax = 1.0f;

		m_pDevice->RenderTexture(texDisplay);
	}
}

void ReplayOutput::DisplayMesh()
{
	FetchDrawcall *draw = m_pRenderer->GetDrawcallByEID(m_EventID, m_LastDeferredEvent);

	if(!draw) return;
	if(m_MainOutput.outputID == 0) return;
	if(m_Width <= 0 || m_Height <= 0) return;
	if(m_RenderData.meshDisplay.type == eMeshDataStage_Unknown) return;
	if((draw->flags & eDraw_Drawcall) == 0) return;

	if(draw && m_OverlayDirty)
	{
		m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_WithoutDraw);
		RefreshOverlay();
		m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_OnlyDraw);
	}
	
	m_pDevice->BindOutputWindow(m_MainOutput.outputID, true);
	m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

	m_pDevice->RenderCheckerboard(Vec3f(0.666f, 0.666f, 0.666f), Vec3f(0.333f, 0.333f, 0.333f));
	
	m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

	MeshDisplay mesh = m_RenderData.meshDisplay;
	mesh.position.buf = m_pDevice->GetLiveID(mesh.position.buf);
	mesh.position.idxbuf = m_pDevice->GetLiveID(mesh.position.idxbuf);
	mesh.second.buf = m_pDevice->GetLiveID(mesh.second.buf);
	mesh.second.idxbuf = m_pDevice->GetLiveID(mesh.second.idxbuf);
	
	vector<MeshFormat> secondaryDraws;
	
	if(m_RenderData.meshDisplay.type != eMeshDataStage_VSIn &&
		 !m_RenderData.meshDisplay.thisDrawOnly)
	{
		mesh.position.unproject = true;
		mesh.second.unproject = true;

		for(size_t i=0; i < passEvents.size(); i++)
		{
			FetchDrawcall *d = m_pRenderer->GetDrawcallByEID(passEvents[i], m_LastDeferredEvent);

			if(d)
			{
				for(uint32_t inst=0; inst < RDCMAX(1U, draw->numInstances); inst++)
				{
					// get the 'most final' stage
					MeshFormat fmt = m_pDevice->GetPostVSBuffers(m_FrameID, passEvents[i], inst, eMeshDataStage_GSOut);
					if(fmt.buf == ResourceId()) fmt = m_pDevice->GetPostVSBuffers(m_FrameID, passEvents[i], inst, eMeshDataStage_VSOut);

					// if unproject is marked, this output had a 'real' system position output
					if(fmt.unproject)
						secondaryDraws.push_back(fmt);
				}
			}
		}

		// draw previous instances in the current drawcall
		if(draw->flags & eDraw_Instanced)
		{
			for(uint32_t inst=0; inst < RDCMAX(1U, draw->numInstances) && inst < m_RenderData.meshDisplay.curInstance; inst++)
			{
				// get the 'most final' stage
				MeshFormat fmt = m_pDevice->GetPostVSBuffers(m_FrameID, draw->eventID, inst, eMeshDataStage_GSOut);
				if(fmt.buf == ResourceId()) fmt = m_pDevice->GetPostVSBuffers(m_FrameID, draw->eventID, inst, eMeshDataStage_VSOut);

				// if unproject is marked, this output had a 'real' system position output
				if(fmt.unproject)
					secondaryDraws.push_back(fmt);
			}
		}
	}

	m_pDevice->RenderMesh(m_FrameID, m_EventID, secondaryDraws, mesh);
}


extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetOutputConfig(ReplayOutput *output, const OutputConfig &o)
{ return output->SetOutputConfig(o); }
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetTextureDisplay(ReplayOutput *output, const TextureDisplay &o)
{ return output->SetTextureDisplay(o); }
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetMeshDisplay(ReplayOutput *output, const MeshDisplay &o)
{ return output->SetMeshDisplay(o); }

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_ClearThumbnails(ReplayOutput *output)
{ return output->ClearThumbnails(); }
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_AddThumbnail(ReplayOutput *output, void *wnd, ResourceId texID)
{ return output->AddThumbnail(wnd, texID); }

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_Display(ReplayOutput *output)
{ return output->Display(); }

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetPixelContext(ReplayOutput *output, void *wnd)
{ return output->SetPixelContext(wnd); }
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetPixelContextLocation(ReplayOutput *output, uint32_t x, uint32_t y)
{ return output->SetPixelContextLocation(x, y); }
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_DisablePixelContext(ReplayOutput *output)
{ output->DisablePixelContext(); }

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_GetCustomShaderTexID(ReplayOutput *output, ResourceId *id)
{ if(id) *id = output->GetCustomShaderTexID(); }

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_PickPixel(ReplayOutput *output, ResourceId texID, bool32 customShader,
														uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *val)
{ return output->PickPixel(texID, customShader != 0, x, y, sliceFace, mip, sample, val); }

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC ReplayOutput_PickVertex(ReplayOutput *output, uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y)
{ return output->PickVertex(frameID, eventID, x, y); }
