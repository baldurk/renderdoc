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

#include "common/string_utils.h"
#include "maths/matrix.h"

ReplayOutput::ReplayOutput(ReplayRenderer *parent, void *w)
{
	m_pRenderer = parent;

	m_MainOutput.dirty = true;

	m_OverlayDirty = true;

	m_pDevice = parent->GetDevice();

	m_OverlayResourceId = ResourceId();
	
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
		m_OverlayDirty = true;
	m_RenderData.texDisplay = o;
	m_MainOutput.dirty = true;
	return true;
}

bool ReplayOutput::SetMeshDisplay(const MeshDisplay &o)
{
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
	
	if(m_Config.m_Type == eOutputType_TexDisplay && m_RenderData.texDisplay.overlay != eTexOverlay_None)
	{
		if(draw && m_pDevice->IsRenderOutput(m_RenderData.texDisplay.texid))
		{
			m_OverlayResourceId = m_pDevice->RenderOverlay(m_pDevice->GetLiveID(m_RenderData.texDisplay.texid), m_RenderData.texDisplay.overlay, m_FrameID, m_EventID);
			m_OverlayDirty = false;
		}
	}

	if(m_Config.m_Type == eOutputType_MeshDisplay && m_OverlayDirty)
	{
		m_OverlayDirty = false;

		if(draw == NULL || (draw->flags & eDraw_Drawcall) == 0)
			return;

		if(m_RenderData.meshDisplay.thisDrawOnly)
		{
			m_pDevice->InitPostVSBuffers(m_FrameID, draw->eventID);
		}
		else
		{
			FetchDrawcall *start = draw;
			while(start->previous != 0 && (m_pRenderer->GetDrawcallByDrawID((uint32_t)start->previous)->flags & eDraw_Clear) == 0)
				start = m_pRenderer->GetDrawcallByDrawID((uint32_t)start->previous);
			
			m_pDevice->ReplayLog(m_FrameID, 0, start->eventID, eReplay_WithoutDraw);

			uint32_t prev = start->eventID;

			while(start)
			{
				if(start->flags & eDraw_Drawcall)
				{
					if(prev != start->eventID)
					{
						m_pDevice->ReplayLog(m_FrameID, prev, start->eventID, eReplay_WithoutDraw);

						prev = start->eventID;
					}

					m_pDevice->InitPostVSBuffers(m_FrameID, start->eventID);
				}

				if(start == draw)
					break;

				start = m_pRenderer->GetDrawcallByDrawID((uint32_t)start->next);
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

bool ReplayOutput::PickPixel(ResourceId tex, bool customShader, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, PixelValue *ret)
{
	if(ret == NULL || tex == ResourceId()) return false;

	RDCEraseEl(ret->value_f);

	if(customShader && m_RenderData.texDisplay.CustomShader != ResourceId() && m_CustomShaderResourceId != ResourceId())
	{
		tex = m_CustomShaderResourceId;
	}

	m_pDevice->PickPixel(m_pDevice->GetLiveID(tex), x, y, sliceFace, mip, ret->value_f);

	return true;
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
		disp.mip = 0;
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
		
	if(m_RenderData.texDisplay.CustomShader != ResourceId())
	{
		m_CustomShaderResourceId = m_pDevice->ApplyCustomShader(m_RenderData.texDisplay.CustomShader, texDisplay.texid, texDisplay.mip);

		texDisplay.texid = m_pDevice->GetLiveID(m_CustomShaderResourceId);
		texDisplay.CustomShader = ResourceId();
		texDisplay.sliceFace = 0;
		texDisplay.mip = 0;
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

	vector<int> events;

	if(draw && m_OverlayDirty)
	{
		m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_WithoutDraw);
		RefreshOverlay();
		m_pDevice->ReplayLog(m_FrameID, 0, m_EventID, eReplay_OnlyDraw);
	}
	
	if(m_RenderData.meshDisplay.type != eMeshDataStage_VSIn)
	{
		if(m_RenderData.meshDisplay.thisDrawOnly)
		{
			events.push_back(draw->eventID);
		}
		else
		{
			FetchDrawcall *start = draw;
			while(start->previous != 0 && (m_pRenderer->GetDrawcallByDrawID((uint32_t)start->previous)->flags & eDraw_Clear) == 0)
				start = m_pRenderer->GetDrawcallByDrawID((uint32_t)start->previous);

			while(start)
			{
				if(start->flags & eDraw_Drawcall)
				{
					events.push_back(start->eventID);
				}

				if(start == draw)
					break;

				start = m_pRenderer->GetDrawcallByDrawID((uint32_t)start->next);
			}
		}
	}
  
	m_pDevice->BindOutputWindow(m_MainOutput.outputID, true);
	m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

	m_pDevice->RenderCheckerboard(Vec3f(0.666f, 0.666f, 0.666f), Vec3f(0.333f, 0.333f, 0.333f));

	if(m_RenderData.meshDisplay.type == eMeshDataStage_VSIn)
	{
		events.clear();
		events.push_back(draw->eventID);
	}

	RDCASSERT(!events.empty());
	
	m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

	m_pDevice->RenderMesh(m_FrameID, events, m_RenderData.meshDisplay);
}


extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_SetOutputConfig(ReplayOutput *output, const OutputConfig &o)
{ return output->SetOutputConfig(o); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_SetTextureDisplay(ReplayOutput *output, const TextureDisplay &o)
{ return output->SetTextureDisplay(o); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_SetMeshDisplay(ReplayOutput *output, const MeshDisplay &o)
{ return output->SetMeshDisplay(o); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_ClearThumbnails(ReplayOutput *output)
{ return output->ClearThumbnails(); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_AddThumbnail(ReplayOutput *output, void *wnd, ResourceId texID)
{ return output->AddThumbnail(wnd, texID); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_Display(ReplayOutput *output)
{ return output->Display(); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_SetPixelContext(ReplayOutput *output, void *wnd)
{ return output->SetPixelContext(wnd); }
extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_SetPixelContextLocation(ReplayOutput *output, uint32_t x, uint32_t y)
{ return output->SetPixelContextLocation(x, y); }
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_DisablePixelContext(ReplayOutput *output)
{ output->DisablePixelContext(); }

extern "C" RENDERDOC_API bool RENDERDOC_CC ReplayOutput_PickPixel(ReplayOutput *output, ResourceId texID, bool customShader,
														uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, PixelValue *val)
{ return output->PickPixel(texID, customShader, x, y, sliceFace, mip, val); }
