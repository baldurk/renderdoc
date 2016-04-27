/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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


#include "gl_replay.h"
#include "gl_driver.h"
#include "gl_resources.h"

#include "data/glsl/debuguniforms.h"

#include "serialise/string_utils.h"

GLReplay::GLReplay()
{
	m_pDriver = NULL;
	m_Proxy = false;

	RDCEraseEl(m_ReplayCtx);
	m_DebugCtx = NULL;

	m_OutputWindowID = 1;
}

void GLReplay::Shutdown()
{
	PreContextShutdownCounters();

	DeleteDebugData();

	DestroyOutputWindow(m_DebugID);

	CloseReplayContext();

	delete m_pDriver;

	GLReplay::PostContextShutdownCounters();
}

#pragma region Implemented

void GLReplay::ReadLogInitialisation()
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->ReadLogInitialisation();
}

void GLReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->ReplayLog(0, endEventID, replayType);
}

vector<uint32_t> GLReplay::GetPassEvents(uint32_t eventID)
{
	vector<uint32_t> passEvents;
	
	const FetchDrawcall *draw = m_pDriver->GetDrawcall(eventID);

	const FetchDrawcall *start = draw;
	while(start && start->previous != 0 && (m_pDriver->GetDrawcall((uint32_t)start->previous)->flags & eDraw_Clear) == 0)
	{
		const FetchDrawcall *prev = m_pDriver->GetDrawcall((uint32_t)start->previous);

		if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) || start->depthOut != prev->depthOut)
			break;

		start = prev;
	}

	while(start)
	{
		if(start == draw)
			break;

		if(start->flags & eDraw_Drawcall)
			passEvents.push_back(start->eventID);

		start = m_pDriver->GetDrawcall((uint32_t)start->next);
	}

	return passEvents;
}

FetchFrameRecord GLReplay::GetFrameRecord()
{
	return m_pDriver->GetFrameRecord();
}

ResourceId GLReplay::GetLiveID(ResourceId id)
{
	return m_pDriver->GetResourceManager()->GetLiveID(id);
}

APIProperties GLReplay::GetAPIProperties()
{
	APIProperties ret;

	ret.pipelineType = ePipelineState_OpenGL;
	ret.degraded = false;

	return ret;
}

vector<ResourceId> GLReplay::GetBuffers()
{
	vector<ResourceId> ret;
	
	for(auto it=m_pDriver->m_Buffers.begin(); it != m_pDriver->m_Buffers.end(); ++it)
		ret.push_back(it->first);

	return ret;
}

vector<ResourceId> GLReplay::GetTextures()
{
	vector<ResourceId> ret;
	ret.reserve(m_pDriver->m_Textures.size());
	
	for(auto it=m_pDriver->m_Textures.begin(); it != m_pDriver->m_Textures.end(); ++it)
	{
		auto &res = m_pDriver->m_Textures[it->first];

		// skip textures that aren't from the log (except the 'default backbuffer' textures)
		if(res.resource.name != m_pDriver->m_FakeBB_Color &&
		   res.resource.name != m_pDriver->m_FakeBB_DepthStencil &&
		   m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first) continue;

		ret.push_back(it->first);
		CacheTexture(it->first);
	}

	return ret;
}

void GLReplay::SetReplayData(GLWindowingData data)
{
	m_ReplayCtx = data;
	if (m_pDriver != NULL)
		m_pDriver->RegisterContext(m_ReplayCtx, NULL, true, true);
	
	InitDebugData();

	PostContextInitCounters();
}

void GLReplay::InitCallstackResolver()
{
	m_pDriver->GetSerialiser()->InitCallstackResolver();
}

bool GLReplay::HasCallstacks()
{
	return m_pDriver->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *GLReplay::GetCallstackResolver()
{
	return m_pDriver->GetSerialiser()->GetCallstackResolver();
}

void GLReplay::CreateOutputWindowBackbuffer(OutputWindow &outwin, bool depth)
{
	if(m_pDriver == NULL) return;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	WrappedOpenGL &gl = *m_pDriver;
	
	// create fake backbuffer for this output window.
	// We'll make an FBO for this backbuffer on the replay context, so we can
	// use the replay context to do the hard work of rendering to it, then just
	// blit across to the real default framebuffer on the output window context
	gl.glGenFramebuffers(1, &outwin.BlitData.windowFBO);
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, outwin.BlitData.windowFBO);

	gl.glGenTextures(1, &outwin.BlitData.backbuffer);
	gl.glBindTexture(eGL_TEXTURE_2D, outwin.BlitData.backbuffer);
	
	gl.glTextureStorage2DEXT(outwin.BlitData.backbuffer, eGL_TEXTURE_2D, 1, eGL_SRGB8, outwin.width, outwin.height); 
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, outwin.BlitData.backbuffer, 0);

	if(depth)
	{
		gl.glGenTextures(1, &outwin.BlitData.depthstencil);
		gl.glBindTexture(eGL_TEXTURE_2D, outwin.BlitData.depthstencil);

		gl.glTextureStorage2DEXT(outwin.BlitData.depthstencil, eGL_TEXTURE_2D, 1, eGL_DEPTH_COMPONENT24, outwin.width, outwin.height); 
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	}
	else
	{
		outwin.BlitData.depthstencil = 0;
	}

	outwin.BlitData.replayFBO = 0;
}

void GLReplay::InitOutputWindow(OutputWindow &outwin)
{
	if(m_pDriver == NULL) return;
	
	MakeCurrentReplayContext(&outwin);
	
	WrappedOpenGL &gl = *m_pDriver;

	gl.glGenVertexArrays(1, &outwin.BlitData.emptyVAO);
	gl.glBindVertexArray(outwin.BlitData.emptyVAO);
}

bool GLReplay::CheckResizeOutputWindow(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;
	
	OutputWindow &outw = m_OutputWindows[id];

	if(outw.wnd == 0)
		return false;

	int32_t w, h;
	GetOutputWindowDimensions(id, w, h);

	if(w != outw.width || h != outw.height)
	{
		outw.width = w;
		outw.height = h;
		
		MakeCurrentReplayContext(m_DebugCtx);
		
		WrappedOpenGL &gl = *m_pDriver;

		bool haddepth = false;
	
		gl.glDeleteTextures(1, &outw.BlitData.backbuffer);
		if(outw.BlitData.depthstencil)
		{
			haddepth = true;
			gl.glDeleteTextures(1, &outw.BlitData.depthstencil);
		}
		gl.glDeleteFramebuffers(1, &outw.BlitData.windowFBO);

		CreateOutputWindowBackbuffer(outw, haddepth);

		return true;
	}

	return false;
}

void GLReplay::BindOutputWindow(uint64_t id, bool depth)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	MakeCurrentReplayContext(m_DebugCtx);

	m_pDriver->glBindFramebuffer(eGL_FRAMEBUFFER, outw.BlitData.windowFBO);
	m_pDriver->glViewport(0, 0, outw.width, outw.height);

	m_pDriver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depth && outw.BlitData.depthstencil ? outw.BlitData.depthstencil : 0, 0);

	DebugData.outWidth = float(outw.width); DebugData.outHeight = float(outw.height);
}

void GLReplay::ClearOutputWindowColour(uint64_t id, float col[4])
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	MakeCurrentReplayContext(m_DebugCtx);

	m_pDriver->glClearBufferfv(eGL_COLOR, 0, col);
}

void GLReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	MakeCurrentReplayContext(m_DebugCtx);

	m_pDriver->glClearBufferfi(eGL_DEPTH_STENCIL, 0, depth, (GLint)stencil);
}

void GLReplay::FlipOutputWindow(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	MakeCurrentReplayContext(&outw);

	WrappedOpenGL &gl = *m_pDriver;

	// go directly to real function so we don't try to bind the 'fake' backbuffer FBO.
	gl.m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, 0);
	gl.glViewport(0, 0, outw.width, outw.height);
	
	gl.glUseProgram(DebugData.blitProg);

	gl.glActiveTexture(eGL_TEXTURE0);
	gl.glBindTexture(eGL_TEXTURE_2D, outw.BlitData.backbuffer);
	gl.glEnable(eGL_FRAMEBUFFER_SRGB);
	
	gl.glBindVertexArray(outw.BlitData.emptyVAO);
	gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

	SwapBuffers(&outw);
}

void GLReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret)
{
	if(m_pDriver->m_Buffers.find(buff) == m_pDriver->m_Buffers.end())
	{
		RDCWARN("Requesting data for non-existant buffer %llu", buff);
		return;
	}

	auto &buf = m_pDriver->m_Buffers[buff];

	uint64_t bufsize = buf.size;
	
	if(len > 0 && offset+len > buf.size)
	{
		RDCWARN("Attempting to read off the end of the array. Will be clamped");

		if(offset < buf.size)
			len = ~0ULL; // min below will clamp to max size size
		else
			return; // offset past buffer size, return empty array
	}
	else if(len == 0)
	{
		len = bufsize;
	}
	
	// need to ensure len+offset doesn't overrun buffer or the glGetBufferSubData call
	// will fail.
	len = RDCMIN(len, bufsize-offset);

	if(len == 0) return;
	
	ret.resize((size_t)len);
	
	WrappedOpenGL &gl = *m_pDriver;

	GLuint oldbuf = 0;
	gl.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf);

	gl.glBindBuffer(eGL_COPY_READ_BUFFER, buf.resource.name);

	gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)len, &ret[0]);

	gl.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf);
}

bool GLReplay::IsRenderOutput(ResourceId id)
{
	for(int32_t i=0; i < m_CurPipelineState.m_FB.m_DrawFBO.Color.count; i++)
	{
		if(m_CurPipelineState.m_FB.m_DrawFBO.Color[i].Obj == id)
				return true;
	}
	
	if(m_CurPipelineState.m_FB.m_DrawFBO.Depth.Obj == id ||
		 m_CurPipelineState.m_FB.m_DrawFBO.Stencil.Obj == id)
			return true;

	return false;
}

void GLReplay::CacheTexture(ResourceId id)
{
	FetchTexture tex;
	
	MakeCurrentReplayContext(&m_ReplayCtx);
	
	auto &res = m_pDriver->m_Textures[id];
	WrappedOpenGL &gl = *m_pDriver;
	
	tex.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);
	
	if(res.resource.Namespace == eResUnknown || res.curType == eGL_NONE)
	{
		if(res.resource.Namespace == eResUnknown)
			RDCERR("Details for invalid texture id %llu requested", id);

		tex.name = "<Uninitialised Texture>";
		tex.customName = true;
		tex.format = ResourceFormat();
		tex.dimension = 1;
		tex.resType = eResType_None;
		tex.width = tex.height = tex.depth = 1;
		tex.cubemap = false;
		tex.mips = 1;
		tex.arraysize = 1;
		tex.numSubresources = 1;
		tex.creationFlags = 0;
		tex.msQual = 0;
		tex.msSamp = 1;
		tex.byteSize = 1;

		m_CachedTextures[id] = tex;
		return;
	}
	
	if(res.resource.Namespace == eResRenderbuffer || res.curType == eGL_RENDERBUFFER)
	{
		tex.dimension = 2;
		tex.resType = eResType_Texture2D;
		tex.width = res.width;
		tex.height = res.height;
		tex.depth = 1;
		tex.cubemap = false;
		tex.mips = 1;
		tex.arraysize = 1;
		tex.numSubresources = 1;
		tex.creationFlags = eTextureCreate_RTV;
		tex.msQual = 0;
		tex.msSamp = res.samples;

		tex.format = MakeResourceFormat(gl, eGL_TEXTURE_2D, res.internalFormat);

		if(IsDepthStencilFormat(res.internalFormat))
			tex.creationFlags |= eTextureCreate_DSV;
		
		tex.byteSize = (tex.width*tex.height)*(tex.format.compByteWidth*tex.format.compCount);

		string str = "";
		char name[128] = {0};
		gl.glGetObjectLabel(eGL_RENDERBUFFER, res.resource.name, 127, NULL, name);
		str = name;
		tex.customName = true;

		if(str == "")
		{
			const char *suffix = "";
			const char *ms = "";

			if(tex.msSamp > 1)
				ms = "MS";

			if(tex.creationFlags & eTextureCreate_RTV)
				suffix = " RTV";
			if(tex.creationFlags & eTextureCreate_DSV)
				suffix = " DSV";

			tex.customName = false;

			str = StringFormat::Fmt("Renderbuffer%s%s %llu", ms, suffix, tex.ID);
		}

		tex.name = str;

		m_CachedTextures[id] = tex;
		return;
	}
	
	GLenum target = TextureTarget(res.curType);

	GLenum levelQueryType = target;
	if(levelQueryType == eGL_TEXTURE_CUBE_MAP)
		levelQueryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

	GLint width = 1, height = 1, depth = 1, samples=1;
	gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_WIDTH, &width);
	gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_HEIGHT, &height);
	gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_DEPTH, &depth);
	gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_SAMPLES, &samples);

	// the above queries sometimes come back 0, if we have dimensions from creation functions, use those
	if(width == 0 && res.width > 0)
		width = res.width;
	if(height == 0 && res.height > 0)
		height = res.height;
	if(depth == 0 && res.depth > 0)
		depth = res.depth;

	if(res.width == 0 && width > 0)
	{
		RDCWARN("TextureData::width didn't get filled out, setting at last minute");
		res.width = width;
	}
	if(res.height == 0 && height > 0)
	{
		RDCWARN("TextureData::height didn't get filled out, setting at last minute");
		res.height = height;
	}
	if(res.depth == 0 && depth > 0)
	{
		RDCWARN("TextureData::depth didn't get filled out, setting at last minute");
		res.depth = depth;
	}

	// reasonably common defaults
	tex.msQual = 0;
	tex.msSamp = 1;
	tex.width = tex.height = tex.depth = tex.arraysize = 1;
	tex.cubemap = false;
	
	switch(target)
	{
		case eGL_TEXTURE_BUFFER:
			tex.resType = eResType_Buffer;
			break;
		case eGL_TEXTURE_1D:
			tex.resType = eResType_Texture1D;
			break;
		case eGL_TEXTURE_2D:
			tex.resType = eResType_Texture2D;
			break;
		case eGL_TEXTURE_3D:
			tex.resType = eResType_Texture3D;
			break;
		case eGL_TEXTURE_1D_ARRAY:
			tex.resType = eResType_Texture1DArray;
			break;
		case eGL_TEXTURE_2D_ARRAY:
			tex.resType = eResType_Texture2DArray;
			break;
		case eGL_TEXTURE_RECTANGLE:
			tex.resType = eResType_TextureRect;
			break;
		case eGL_TEXTURE_2D_MULTISAMPLE:
			tex.resType = eResType_Texture2DMS;
			break;
		case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
			tex.resType = eResType_Texture2DMSArray;
			break;
		case eGL_TEXTURE_CUBE_MAP:
			tex.resType = eResType_TextureCube;
			break;
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			tex.resType = eResType_TextureCubeArray;
			break;

		default:
			tex.resType = eResType_None;
			RDCERR("Unexpected texture enum %s", ToStr::Get(target).c_str());
	}
	
	switch(target)
	{
		case eGL_TEXTURE_1D:
		case eGL_TEXTURE_BUFFER:
			tex.dimension = 1;
			tex.width = (uint32_t)width;
			break;
		case eGL_TEXTURE_1D_ARRAY:
			tex.dimension = 1;
			tex.width = (uint32_t)width;
			tex.arraysize = depth;
			break;
		case eGL_TEXTURE_2D:
		case eGL_TEXTURE_RECTANGLE:
		case eGL_TEXTURE_2D_MULTISAMPLE:
		case eGL_TEXTURE_CUBE_MAP:
			tex.dimension = 2;
			tex.width = (uint32_t)width;
			tex.height = (uint32_t)height;
			tex.depth = 1;
			tex.arraysize = (target == eGL_TEXTURE_CUBE_MAP ? 6 : 1);
			tex.cubemap = (target == eGL_TEXTURE_CUBE_MAP);
			tex.msSamp = (target == eGL_TEXTURE_2D_MULTISAMPLE ? samples : 1);
			break;
		case eGL_TEXTURE_2D_ARRAY:
		case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			tex.dimension = 2;
			tex.width = (uint32_t)width;
			tex.height = (uint32_t)height;
			tex.depth = 1;
			tex.arraysize = depth;
			tex.cubemap = (target == eGL_TEXTURE_CUBE_MAP_ARRAY);
			tex.msSamp = (target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ? samples : 1);
			break;
		case eGL_TEXTURE_3D:
			tex.dimension = 3;
			tex.width = (uint32_t)width;
			tex.height = (uint32_t)height;
			tex.depth = (uint32_t)depth;
			break;

		default:
			tex.dimension = 2;
			RDCERR("Unexpected texture enum %s", ToStr::Get(target).c_str());
	}
	
	tex.creationFlags = res.creationFlags;
	if(res.resource.name == gl.m_FakeBB_Color || res.resource.name == gl.m_FakeBB_DepthStencil)
		tex.creationFlags |= eTextureCreate_SwapBuffer;

	// surely this will be the same for each level... right? that would be insane if it wasn't
	GLint fmt = 0;
	gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, &fmt);

	tex.format = MakeResourceFormat(gl, target, (GLenum)fmt);
	
	if(tex.format.compType == eCompType_Depth)
		tex.creationFlags |= eTextureCreate_DSV;

	string str = "";
	char name[128] = {0};
	gl.glGetObjectLabel(eGL_TEXTURE, res.resource.name, 127, NULL, name);
	str = name;
	tex.customName = true;

	if(str == "")
	{
		const char *suffix = "";
		const char *ms = "";

		if(tex.msSamp > 1)
			ms = "MS";

		if(tex.creationFlags & eTextureCreate_RTV)
			suffix = " RTV";
		if(tex.creationFlags & eTextureCreate_DSV)
			suffix = " DSV";

		tex.customName = false;

		if(tex.cubemap)
		{
			if(tex.arraysize > 6)
				str = StringFormat::Fmt("TextureCube%sArray%s %llu", ms, suffix, tex.ID);
			else
				str = StringFormat::Fmt("TextureCube%s%s %llu", ms, suffix, tex.ID);
		}
		else
		{
			if(tex.arraysize > 1)
				str = StringFormat::Fmt("Texture%dD%sArray%s %llu", tex.dimension, ms, suffix, tex.ID);
			else
				str = StringFormat::Fmt("Texture%dD%s%s %llu", tex.dimension, ms, suffix, tex.ID);
		}
	}

	tex.name = str;

	if(target == eGL_TEXTURE_BUFFER)
	{
		tex.dimension = 1;
		tex.width = tex.height = tex.depth = 1;
		tex.cubemap = false;
		tex.mips = 1;
		tex.arraysize = 1;
		tex.numSubresources = 1;
		tex.creationFlags = eTextureCreate_SRV;
		tex.msQual = tex.msSamp = 0;
		tex.byteSize = 0;

		gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_BUFFER_SIZE, (GLint *)&tex.byteSize);
		tex.width = uint32_t(tex.byteSize/(tex.format.compByteWidth*tex.format.compCount));
		
		m_CachedTextures[id] = tex;
		return;
	}

	tex.mips = GetNumMips(gl.m_Real, target, res.resource.name, tex.width, tex.height, tex.depth);

	tex.numSubresources = tex.mips*tex.arraysize;
	
	GLint compressed;
	gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_COMPRESSED, &compressed);
	tex.byteSize = 0;
	for(uint32_t a=0; a < tex.arraysize; a++)
	{
		for(uint32_t m=0; m < tex.mips; m++)
		{
			if(compressed)
			{
				gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, m, eGL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressed);
				tex.byteSize += compressed;
			}
			else if(tex.format.special)
			{
				tex.byteSize += GetByteSize(RDCMAX(1U, tex.width>>m), RDCMAX(1U, tex.height>>m), RDCMAX(1U, tex.depth>>m), 
																		GetBaseFormat((GLenum)fmt), GetDataType((GLenum)fmt));
			}
			else
			{
				tex.byteSize += RDCMAX(1U, tex.width>>m)*RDCMAX(1U, tex.height>>m)*RDCMAX(1U, tex.depth>>m)*
													tex.format.compByteWidth*tex.format.compCount;
			}
		}
	}

	m_CachedTextures[id] = tex;
}

FetchBuffer GLReplay::GetBuffer(ResourceId id)
{
	FetchBuffer ret;
	
	MakeCurrentReplayContext(&m_ReplayCtx);
	
	auto &res = m_pDriver->m_Buffers[id];

	if(res.resource.Namespace == eResUnknown)
	{
		RDCERR("Details for invalid buffer id %llu requested", id);
		RDCEraseEl(ret);
		return ret;
	}
	
	WrappedOpenGL &gl = *m_pDriver;
	
	ret.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);

	if(res.curType == eGL_NONE)
	{
		ret.byteSize = 0;
		ret.creationFlags = 0;
		ret.customName = true;
		ret.name = "<Uninitialised Buffer>";
		ret.length = 0;
		ret.structureSize = 0;
		return ret;
	}

	gl.glBindBuffer(res.curType, res.resource.name);

	ret.structureSize = 0;

	ret.creationFlags = 0;
	switch(res.curType)
	{
		case eGL_ARRAY_BUFFER:
			ret.creationFlags = eBufferCreate_VB;
			break;
		case eGL_ELEMENT_ARRAY_BUFFER:
			ret.creationFlags = eBufferCreate_IB;
			break;
		case eGL_UNIFORM_BUFFER:
			ret.creationFlags = eBufferCreate_CB;
			break;
		case eGL_SHADER_STORAGE_BUFFER:
			ret.creationFlags = eBufferCreate_UAV;
			break;
		case eGL_DRAW_INDIRECT_BUFFER:
		case eGL_DISPATCH_INDIRECT_BUFFER:
		case eGL_PARAMETER_BUFFER_ARB:
			ret.creationFlags = eBufferCreate_Indirect;
			break;
		case eGL_PIXEL_PACK_BUFFER:
		case eGL_PIXEL_UNPACK_BUFFER:
		case eGL_COPY_WRITE_BUFFER:
		case eGL_COPY_READ_BUFFER:
		case eGL_QUERY_BUFFER:
		case eGL_TEXTURE_BUFFER:
		case eGL_TRANSFORM_FEEDBACK_BUFFER:
		case eGL_ATOMIC_COUNTER_BUFFER:
			break;
		default:
			RDCERR("Unexpected buffer type %s", ToStr::Get(res.curType).c_str());
	}

	GLint size;
	gl.glGetBufferParameteriv(res.curType, eGL_BUFFER_SIZE, &size);

	ret.byteSize = ret.length = (uint32_t)size;
	
	if(res.size == 0)
	{
		RDCWARN("BufferData::size didn't get filled out, setting at last minute");
		res.size = ret.byteSize;
	}

	string str = "";
	char name[128] = {0};
	gl.glGetObjectLabel(eGL_BUFFER, res.resource.name, 127, NULL, name);
	str = name;
	ret.customName = true;

	if(str == "")
	{
		ret.customName = false;
		str = StringFormat::Fmt("Buffer %llu", ret.ID);
	}

	ret.name = str;

	return ret;
}

vector<DebugMessage> GLReplay::GetDebugMessages()
{
	return m_pDriver->GetDebugMessages();
}

ShaderReflection *GLReplay::GetShader(ResourceId shader, string entryPoint)
{
	auto &shaderDetails = m_pDriver->m_Shaders[shader];
	
	if(shaderDetails.prog == 0)
	{
		RDCERR("Can't get shader details without separable program");
		return NULL;
	}

	return &shaderDetails.reflection;
}

void GLReplay::SavePipelineState()
{
	GLPipelineState &pipe = m_CurPipelineState;
	WrappedOpenGL &gl = *m_pDriver;
	GLResourceManager *rm = m_pDriver->GetResourceManager();

	MakeCurrentReplayContext(&m_ReplayCtx);
	
	GLRenderState rs(&gl.GetHookset(), NULL, READING);
	rs.FetchState(m_ReplayCtx.ctx, &gl);

	// Index buffer

	void *ctx = m_ReplayCtx.ctx;

	GLuint ibuffer = 0;
	gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint*)&ibuffer);
	pipe.m_VtxIn.ibuffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, ibuffer)));

	pipe.m_VtxIn.primitiveRestart = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart];
	pipe.m_VtxIn.restartIndex = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex] ? ~0U : rs.PrimitiveRestartIndex;

	// Vertex buffers and attributes
	GLint numVBufferBindings = 16;
	gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);
	
	GLint numVAttribBindings = 16;
	gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numVAttribBindings);

	create_array_uninit(pipe.m_VtxIn.vbuffers, numVBufferBindings);
	create_array_uninit(pipe.m_VtxIn.attributes, numVAttribBindings);

	for(GLuint i=0; i < (GLuint)numVBufferBindings; i++)
	{
		GLuint buffer = GetBoundVertexBuffer(gl.m_Real, i);

		pipe.m_VtxIn.vbuffers[i].Buffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, buffer)));

		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Stride);
		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Offset);
		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Divisor);
	}
	
	for(GLuint i=0; i < (GLuint)numVAttribBindings; i++)
	{
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED, (GLint *)&pipe.m_VtxIn.attributes[i].Enabled);
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING, (GLint *)&pipe.m_VtxIn.attributes[i].BufferSlot);
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET, (GLint*)&pipe.m_VtxIn.attributes[i].RelativeOffset);

		GLenum type = eGL_FLOAT;
		GLint normalized = 0;
		
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&type);
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);

		GLint integer = 0;
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);
		
		RDCEraseEl(pipe.m_VtxIn.attributes[i].GenericValue);
		gl.glGetVertexAttribfv(i, eGL_CURRENT_VERTEX_ATTRIB, pipe.m_VtxIn.attributes[i].GenericValue.f);

		ResourceFormat fmt;

		fmt.special = false;
		fmt.compCount = 4;
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&fmt.compCount);

		bool intComponent = !normalized || integer;
		
		switch(type)
		{
			default:
			case eGL_BYTE:
				fmt.compByteWidth = 1;
				fmt.compType = intComponent ? eCompType_SInt : eCompType_SNorm;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_BYTE%d", fmt.compCount) : string("GL_BYTE")) + (intComponent ? "" : "_SNORM");
				break;
			case eGL_UNSIGNED_BYTE:
				fmt.compByteWidth = 1;
				fmt.compType = intComponent ? eCompType_UInt : eCompType_UNorm;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_UNSIGNED_BYTE%d", fmt.compCount) : string("GL_UNSIGNED_BYTE")) + (intComponent ? "" : "_UNORM");
				break;
			case eGL_SHORT:
				fmt.compByteWidth = 2;
				fmt.compType = intComponent ? eCompType_SInt : eCompType_SNorm;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_SHORT%d", fmt.compCount) : string("GL_SHORT")) + (intComponent ? "" : "_SNORM");
				break;
			case eGL_UNSIGNED_SHORT:
				fmt.compByteWidth = 2;
				fmt.compType = intComponent ? eCompType_UInt : eCompType_UNorm;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_UNSIGNED_SHORT%d", fmt.compCount) : string("GL_UNSIGNED_SHORT")) + (intComponent ? "" : "_UNORM");
				break;
			case eGL_INT:
				fmt.compByteWidth = 4;
				fmt.compType = intComponent ? eCompType_SInt : eCompType_SNorm;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_INT%d", fmt.compCount) : string("GL_INT")) + (intComponent ? "" : "_SNORM");
				break;
			case eGL_UNSIGNED_INT:
				fmt.compByteWidth = 4;
				fmt.compType = intComponent ? eCompType_UInt : eCompType_UNorm;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_UNSIGNED_INT%d", fmt.compCount) : string("GL_UNSIGNED_INT")) + (intComponent ? "" : "_UNORM");
				break;
			case eGL_FLOAT:
				fmt.compByteWidth = 4;
				fmt.compType = eCompType_Float;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_FLOAT%d", fmt.compCount) : string("GL_FLOAT"));
				break;
			case eGL_DOUBLE:
				fmt.compByteWidth = 8;
				fmt.compType = eCompType_Double;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_DOUBLE%d", fmt.compCount) : string("GL_DOUBLE"));
				break;
			case eGL_HALF_FLOAT:
				fmt.compByteWidth = 2;
				fmt.compType = eCompType_Float;
				fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_HALF_FLOAT%d", fmt.compCount) : string("GL_HALF_FLOAT"));
				break;
			case eGL_INT_2_10_10_10_REV:
				fmt.special = true;
				fmt.specialFormat = eSpecial_R10G10B10A2;
				fmt.compCount = 4;
				fmt.compType = eCompType_UInt;
				fmt.strname = "GL_INT_2_10_10_10_REV";
				break;
			case eGL_UNSIGNED_INT_2_10_10_10_REV:
				fmt.special = true;
				fmt.specialFormat = eSpecial_R10G10B10A2;
				fmt.compCount = 4;
				fmt.compType = eCompType_SInt;
				fmt.strname = "GL_UNSIGNED_INT_2_10_10_10_REV";
				break;
			case eGL_UNSIGNED_INT_10F_11F_11F_REV:
				fmt.special = true;
				fmt.specialFormat = eSpecial_R11G11B10;
				fmt.compCount = 3;
				fmt.compType = eCompType_Float;
				fmt.strname = "GL_UNSIGNED_INT_10F_11F_11F_REV";
				break;
		}
		
		if(fmt.compCount == eGL_BGRA)
		{
			fmt.compByteWidth = 1;
			fmt.compCount = 4;
			fmt.bgraOrder = true;
			fmt.compType = eCompType_UNorm;

			if(type == eGL_UNSIGNED_BYTE)
			{
				fmt.strname = "GL_BGRA8";
			}
			else if(type == eGL_UNSIGNED_INT_2_10_10_10_REV || type == eGL_INT_2_10_10_10_REV)
			{
				fmt.specialFormat = eSpecial_R10G10B10A2;
				fmt.compType = type == eGL_UNSIGNED_INT_2_10_10_10_REV ? eCompType_UInt : eCompType_SInt;
				fmt.strname = type == eGL_UNSIGNED_INT_2_10_10_10_REV ? "GL_UNSIGNED_INT_2_10_10_10_REV" : "GL_INT_2_10_10_10_REV";
			}
			else
			{
				RDCERR("Unexpected BGRA type");
			}

			// haven't checked the other cases work properly
			RDCASSERT(type == eGL_UNSIGNED_BYTE);
		}

		pipe.m_VtxIn.attributes[i].Format = fmt;
	}

	pipe.m_VtxIn.provokingVertexLast = (rs.ProvokingVertex != eGL_FIRST_VERTEX_CONVENTION);
	
	memcpy(pipe.m_VtxProcess.defaultInnerLevel, rs.PatchParams.defaultInnerLevel, sizeof(rs.PatchParams.defaultInnerLevel));
	memcpy(pipe.m_VtxProcess.defaultOuterLevel, rs.PatchParams.defaultOuterLevel, sizeof(rs.PatchParams.defaultOuterLevel));

	pipe.m_VtxProcess.discard = rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard];
	pipe.m_VtxProcess.clipOriginLowerLeft = (rs.ClipOrigin != eGL_UPPER_LEFT);
	pipe.m_VtxProcess.clipNegativeOneToOne = (rs.ClipDepth != eGL_ZERO_TO_ONE);
	for(int i=0; i < 8; i++)
		pipe.m_VtxProcess.clipPlanes[i] = rs.Enabled[GLRenderState::eEnabled_ClipDistance0+i];
	
	// Shader stages & Textures
	
	GLint numTexUnits = 8;
	gl.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);
	create_array_uninit(pipe.Textures, numTexUnits);
	create_array_uninit(pipe.Samplers, numTexUnits);

	GLenum activeTexture = eGL_TEXTURE0;
	gl.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint*)&activeTexture);

	pipe.m_VS.stage = eShaderStage_Vertex;
	pipe.m_TCS.stage = eShaderStage_Tess_Control;
	pipe.m_TES.stage = eShaderStage_Tess_Eval;
	pipe.m_GS.stage = eShaderStage_Geometry;
	pipe.m_FS.stage = eShaderStage_Fragment;
	pipe.m_CS.stage = eShaderStage_Compute;

	GLuint curProg = 0;
	gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint*)&curProg);
	
	GLPipelineState::ShaderStage *stages[6] = {
		&pipe.m_VS,
		&pipe.m_TCS,
		&pipe.m_TES,
		&pipe.m_GS,
		&pipe.m_FS,
		&pipe.m_CS,
	};
	ShaderReflection *refls[6] = { NULL };
	ShaderBindpointMapping *mappings[6] = { NULL };

	for(int i=0; i < 6; i++)
	{
		stages[i]->Shader = ResourceId();
		stages[i]->ShaderDetails = NULL;
		stages[i]->BindpointMapping.ConstantBlocks.Delete();
		stages[i]->BindpointMapping.ReadOnlyResources.Delete();
		stages[i]->BindpointMapping.ReadWriteResources.Delete();
	}

	if(curProg == 0)
	{
		gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint*)&curProg);
	
		if(curProg == 0)
		{
			for(GLint unit=0; unit < numTexUnits; unit++)
			{
				RDCEraseEl(pipe.Textures[unit]);
				RDCEraseEl(pipe.Samplers[unit]);
			}
		}
		else
		{
			ResourceId id = rm->GetID(ProgramPipeRes(ctx, curProg));
			auto &pipeDetails = m_pDriver->m_Pipelines[id];
			
			string pipelineName;
			{
				char name[128] = {0};
				gl.glGetObjectLabel(eGL_PROGRAM_PIPELINE, curProg, 127, NULL, name);
				pipelineName = name;
			}

			for(size_t i=0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
			{
				stages[i]->PipelineActive = true;
				stages[i]->PipelineName = pipelineName;
				stages[i]->customPipelineName = (pipelineName != "");

				if(pipeDetails.stageShaders[i] != ResourceId())
				{
					curProg = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
					stages[i]->Shader = rm->GetOriginalID(pipeDetails.stageShaders[i]);
					refls[i] = GetShader(pipeDetails.stageShaders[i], "");
					GetBindpointMapping(gl.GetHookset(), curProg, (int)i, refls[i], stages[i]->BindpointMapping);
					mappings[i] = &stages[i]->BindpointMapping;

					{
						char name[128] = {0};
						gl.glGetObjectLabel(eGL_PROGRAM, curProg, 127, NULL, name);
						stages[i]->ProgramName = name;
						stages[i]->customProgramName = (name[0] != 0);
					}

					{
						char name[128] = {0};
						gl.glGetObjectLabel(eGL_SHADER, rm->GetCurrentResource(pipeDetails.stageShaders[i]).name, 127, NULL, name);
						stages[i]->ShaderName = name;
						stages[i]->customShaderName = (name[0] != 0);
					}
				}
				else
				{
					stages[i]->Shader = ResourceId();
				}
			}
		}
	}
	else
	{
		auto &progDetails = m_pDriver->m_Programs[rm->GetID(ProgramRes(ctx, curProg))];
		
		string programName;
		{
			char name[128] = {0};
			gl.glGetObjectLabel(eGL_PROGRAM, curProg, 127, NULL, name);
			programName = name;
		}

		for(size_t i=0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
		{
			if(progDetails.stageShaders[i] != ResourceId())
			{
				stages[i]->ProgramName = programName;
				stages[i]->customProgramName = (programName != "");

				stages[i]->Shader = rm->GetOriginalID(progDetails.stageShaders[i]);
				refls[i] = GetShader(progDetails.stageShaders[i], "");
				GetBindpointMapping(gl.GetHookset(), curProg, (int)i, refls[i], stages[i]->BindpointMapping);
				mappings[i] = &stages[i]->BindpointMapping;

				{
					char name[128] = {0};
					gl.glGetObjectLabel(eGL_SHADER, rm->GetCurrentResource(progDetails.stageShaders[i]).name, 127, NULL, name);
					stages[i]->ShaderName = name;
					stages[i]->customShaderName = (name[0] != 0);
				}
			}
		}
	}

	RDCEraseEl(pipe.m_Feedback);
	
	GLuint feedback = 0;
	gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint*)&feedback);

	if(feedback != 0)
		pipe.m_Feedback.Obj = rm->GetOriginalID(rm->GetID(FeedbackRes(ctx, feedback)));

	GLint maxCount = 0;
	gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

	for(int i=0; i < (int)ARRAY_COUNT(pipe.m_Feedback.BufferBinding) && i < maxCount; i++)
	{
		GLuint buffer = 0;
		gl.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint*)&buffer);
		pipe.m_Feedback.BufferBinding[i] = rm->GetOriginalID(rm->GetID(BufferRes(ctx, buffer)));
		gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i, (GLint64*)&pipe.m_Feedback.Offset[i]);
		gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE,  i, (GLint64*)&pipe.m_Feedback.Size[i]);
	}

	GLint p=0;
	gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BUFFER_PAUSED, &p);
	pipe.m_Feedback.Paused = (p != 0);

	gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE, &p);
	pipe.m_Feedback.Active = (p != 0);

	for(int i=0; i < 6; i++)
	{
		size_t num = RDCMIN(128, rs.Subroutines[i].numSubroutines);
		if(num == 0)
		{
			RDCEraseEl(stages[i]->Subroutines);
		}
		else
		{
			create_array_uninit(stages[i]->Subroutines, num);
			memcpy(stages[i]->Subroutines.elems, rs.Subroutines[i].Values, num);
		}
	}

	// GL is ass-backwards in its handling of texture units. When a shader is active
	// the types in the glsl samplers inform which targets are used from which texture units
	//
	// So texture unit 5 can have a 2D bound (texture 52) and a Cube bound (texture 77).
	// * if a uniform sampler2D has value 5 then the 2D texture is used, and we sample from 52
	// * if a uniform samplerCube has value 5 then the Cube texture is used, and we sample from 77
	// It's illegal for both a sampler2D and samplerCube to both have the same value (or any two
	// different types). It makes it all rather pointless and needlessly complex.
	//
	// What we have to do then, is consider the program, look at the values of the uniforms, and
	// then get the appropriate current binding based on the uniform type. We can warn/alert the
	// user if we hit the illegal case of two uniforms with different types but the same value
	//
	// Handling is different if no shaders are active, but we don't consider that case.

	for(GLint unit=0; unit < numTexUnits; unit++)
	{
		GLenum binding = eGL_NONE;
		GLenum target = eGL_NONE;
		ShaderResourceType resType = eResType_None;

		bool shadow = false;

		for(size_t s=0; s < ARRAY_COUNT(refls); s++)
		{
			if(refls[s] == NULL) continue;

			for(int32_t r=0; r < refls[s]->ReadOnlyResources.count; r++)
			{
				// bindPoint is the uniform value for this sampler
				if(mappings[s]->ReadOnlyResources[ refls[s]->ReadOnlyResources[r].bindPoint ].bind == unit)
				{
					GLenum t = eGL_NONE;

					if(strstr(refls[s]->ReadOnlyResources[r].variableType.descriptor.name.elems, "Shadow"))
						shadow = true;

					switch(refls[s]->ReadOnlyResources[r].resType)
					{
						case eResType_None:
							target = eGL_NONE;
							break;
						case eResType_Buffer:
							target = eGL_TEXTURE_BUFFER;
							break;
						case eResType_Texture1D:
							target = eGL_TEXTURE_1D;
							break;
						case eResType_Texture1DArray:
							target = eGL_TEXTURE_1D_ARRAY;
							break;
						case eResType_Texture2D:
							target = eGL_TEXTURE_2D;
							break;
						case eResType_TextureRect:
							target = eGL_TEXTURE_RECTANGLE;
							break;
						case eResType_Texture2DArray:
							target = eGL_TEXTURE_2D_ARRAY;
							break;
						case eResType_Texture2DMS:
							target = eGL_TEXTURE_2D_MULTISAMPLE;
							break;
						case eResType_Texture2DMSArray:
							target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY;
							break;
						case eResType_Texture3D:
							target = eGL_TEXTURE_3D;
							break;
						case eResType_TextureCube:
							target = eGL_TEXTURE_CUBE_MAP;
							break;
						case eResType_TextureCubeArray:
							target = eGL_TEXTURE_CUBE_MAP_ARRAY;
							break;
						case eResType_Count:
							RDCERR("Invalid shader resource type");
							break;
					}
					
					if(target != eGL_NONE)
						t = TextureBinding(target);

					resType = refls[s]->ReadOnlyResources[r].resType;

					if(binding == eGL_NONE)
					{
						binding = t;
					}
					else if(binding == t)
					{
						// two uniforms with the same type pointing to the same slot is fine
						binding = t;
					}
					else if(binding != t)
					{
						RDCWARN("Two uniforms pointing to texture unit %d with types %s and %s", unit, ToStr::Get(binding).c_str(), ToStr::Get(t).c_str());
					}
				}
			}
		}

		if(binding != eGL_NONE)
		{
			gl.glActiveTexture(GLenum(eGL_TEXTURE0+unit));

			GLuint tex;
			gl.glGetIntegerv(binding, (GLint *)&tex);

			if(tex == 0)
			{
				pipe.Textures[unit].Resource = ResourceId();
				pipe.Textures[unit].FirstSlice = 0;
				pipe.Textures[unit].ResType = eResType_None;
				pipe.Textures[unit].DepthReadChannel = -1;
				pipe.Textures[unit].Swizzle[0] = eSwizzle_Red;
				pipe.Textures[unit].Swizzle[1] = eSwizzle_Green;
				pipe.Textures[unit].Swizzle[2] = eSwizzle_Blue;
				pipe.Textures[unit].Swizzle[3] = eSwizzle_Alpha;

				RDCEraseEl(pipe.Samplers[unit].BorderColor);
				pipe.Samplers[unit].AddressS = "";
				pipe.Samplers[unit].AddressT = "";
				pipe.Samplers[unit].AddressR = "";
				pipe.Samplers[unit].Comparison = "";
				pipe.Samplers[unit].MinFilter = "";
				pipe.Samplers[unit].MagFilter = "";
				pipe.Samplers[unit].UseBorder = false;
				pipe.Samplers[unit].UseComparison = false;
				pipe.Samplers[unit].SeamlessCube = false;
				pipe.Samplers[unit].MaxAniso = 0.0f;
				pipe.Samplers[unit].MaxLOD = 0.0f;
				pipe.Samplers[unit].MinLOD = 0.0f;
				pipe.Samplers[unit].MipLODBias = 0.0f;
			}
			else
			{
				// very bespoke/specific
				GLint firstSlice = 0, firstMip = 0;

				if(target != eGL_TEXTURE_BUFFER)
				{
					gl.glGetTexParameteriv(target, eGL_TEXTURE_VIEW_MIN_LEVEL, &firstMip);
					gl.glGetTexParameteriv(target, eGL_TEXTURE_VIEW_MIN_LAYER, &firstSlice);
				}

				pipe.Textures[unit].Resource = rm->GetOriginalID(rm->GetID(TextureRes(ctx, tex)));
				pipe.Textures[unit].HighestMip = (uint32_t)firstMip;
				pipe.Textures[unit].FirstSlice = (uint32_t)firstSlice;
				pipe.Textures[unit].ResType = resType;

				pipe.Textures[unit].DepthReadChannel = -1;

				GLenum levelQueryType = target == eGL_TEXTURE_CUBE_MAP ? eGL_TEXTURE_CUBE_MAP_POSITIVE_X : target;
				GLenum fmt = eGL_NONE;
				gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
				fmt = GetSizedFormat(gl.GetHookset(), target, fmt);
				if(IsDepthStencilFormat(fmt))
				{
					GLint depthMode;
					gl.glGetTexParameteriv(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &depthMode);

					if(depthMode == eGL_DEPTH_COMPONENT)
						pipe.Textures[unit].DepthReadChannel = 0;
					else if(depthMode == eGL_STENCIL_INDEX)
						pipe.Textures[unit].DepthReadChannel = 1;
				}

				GLint swizzles[4] = { eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA };
				if(target != eGL_TEXTURE_BUFFER)
					gl.glGetTexParameteriv(target, eGL_TEXTURE_SWIZZLE_RGBA, swizzles);

				for(int i=0; i < 4; i++)
				{
					switch(swizzles[i])
					{
					default:
					case GL_ZERO:
						pipe.Textures[unit].Swizzle[i] = eSwizzle_Zero;
						break;
					case GL_ONE:
						pipe.Textures[unit].Swizzle[i] = eSwizzle_One;
						break;
					case eGL_RED:
						pipe.Textures[unit].Swizzle[i] = eSwizzle_Red;
						break;
					case eGL_GREEN:
						pipe.Textures[unit].Swizzle[i] = eSwizzle_Green;
						break;
					case eGL_BLUE:
						pipe.Textures[unit].Swizzle[i] = eSwizzle_Blue;
						break;
					case eGL_ALPHA:
						pipe.Textures[unit].Swizzle[i] = eSwizzle_Alpha;
						break;
					}
				}

				GLuint samp;
				gl.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&samp);

				pipe.Samplers[unit].Samp = rm->GetOriginalID(rm->GetID(SamplerRes(ctx, samp)));

				if(target != eGL_TEXTURE_BUFFER)
				{
					if(samp != 0)
						gl.glGetSamplerParameterfv(samp, eGL_TEXTURE_BORDER_COLOR, &pipe.Samplers[unit].BorderColor[0]);
					else
						gl.glGetTexParameterfv(target, eGL_TEXTURE_BORDER_COLOR, &pipe.Samplers[unit].BorderColor[0]);

					pipe.Samplers[unit].UseBorder = false;
					pipe.Samplers[unit].UseComparison = shadow;

					GLint v;
					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_S, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_S, &v);
					pipe.Samplers[unit].AddressS = SamplerString((GLenum)v);
					pipe.Samplers[unit].UseBorder |= (v == eGL_CLAMP_TO_BORDER);

					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_T, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_T, &v);
					pipe.Samplers[unit].AddressT = SamplerString((GLenum)v);
					pipe.Samplers[unit].UseBorder |= (v == eGL_CLAMP_TO_BORDER);

					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_R, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_R, &v);
					pipe.Samplers[unit].AddressR = SamplerString((GLenum)v);
					pipe.Samplers[unit].UseBorder |= (v == eGL_CLAMP_TO_BORDER);

					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_CUBE_MAP_SEAMLESS, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_CUBE_MAP_SEAMLESS, &v);
					pipe.Samplers[unit].SeamlessCube = (v != 0 || rs.Enabled[GLRenderState::eEnabled_TexCubeSeamless]);

					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_COMPARE_FUNC, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_COMPARE_FUNC, &v);
					pipe.Samplers[unit].Comparison = ToStr::Get((GLenum)v).substr(3).c_str();

					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_MIN_FILTER, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_MIN_FILTER, &v);
					pipe.Samplers[unit].MinFilter = SamplerString((GLenum)v);

					v=0;
					if(samp != 0)
						gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_MAG_FILTER, &v);
					else
						gl.glGetTexParameteriv(target, eGL_TEXTURE_MAG_FILTER, &v);
					pipe.Samplers[unit].MagFilter = SamplerString((GLenum)v);

					if(samp != 0)
						gl.glGetSamplerParameterfv(samp, eGL_TEXTURE_MAX_ANISOTROPY_EXT, &pipe.Samplers[unit].MaxAniso);
					else
						gl.glGetTexParameterfv(target, eGL_TEXTURE_MAX_ANISOTROPY_EXT, &pipe.Samplers[unit].MaxAniso);

					gl.glGetTexParameterfv(target, eGL_TEXTURE_MAX_LOD, &pipe.Samplers[unit].MaxLOD);
					gl.glGetTexParameterfv(target, eGL_TEXTURE_MIN_LOD, &pipe.Samplers[unit].MinLOD);
					gl.glGetTexParameterfv(target, eGL_TEXTURE_LOD_BIAS, &pipe.Samplers[unit].MipLODBias);
				}
				else
				{
					// texture buffers don't support sampling
					RDCEraseEl(pipe.Samplers[unit].BorderColor);
					pipe.Samplers[unit].AddressS = "";
					pipe.Samplers[unit].AddressT = "";
					pipe.Samplers[unit].AddressR = "";
					pipe.Samplers[unit].Comparison = "";
					pipe.Samplers[unit].MinFilter = "";
					pipe.Samplers[unit].MagFilter = "";
					pipe.Samplers[unit].UseBorder = false;
					pipe.Samplers[unit].UseComparison = false;
					pipe.Samplers[unit].SeamlessCube = false;
					pipe.Samplers[unit].MaxAniso = 0.0f;
					pipe.Samplers[unit].MaxLOD = 0.0f;
					pipe.Samplers[unit].MinLOD = 0.0f;
					pipe.Samplers[unit].MipLODBias = 0.0f;
				}
			}
		}
		else
		{
			// what should we do in this case? there could be something bound just not used,
			// it'd be nice to return that
		}
	}

	gl.glActiveTexture(activeTexture);
	
	create_array_uninit(pipe.UniformBuffers, ARRAY_COUNT(rs.UniformBinding));
	for(int32_t b=0; b < pipe.UniformBuffers.count; b++)
	{
		if(rs.UniformBinding[b].name == 0)
		{
			pipe.UniformBuffers[b].Resource = ResourceId();
			pipe.UniformBuffers[b].Offset = pipe.UniformBuffers[b].Size = 0;
		}
		else
		{
			pipe.UniformBuffers[b].Resource = rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.UniformBinding[b].name)));
			pipe.UniformBuffers[b].Offset = rs.UniformBinding[b].start;
			pipe.UniformBuffers[b].Size = rs.UniformBinding[b].size;
		}
	}
	
	create_array_uninit(pipe.AtomicBuffers, ARRAY_COUNT(rs.AtomicCounter));
	for(int32_t b=0; b < pipe.AtomicBuffers.count; b++)
	{
		if(rs.AtomicCounter[b].name == 0)
		{
			pipe.AtomicBuffers[b].Resource = ResourceId();
			pipe.AtomicBuffers[b].Offset = pipe.AtomicBuffers[b].Size = 0;
		}
		else
		{
			pipe.AtomicBuffers[b].Resource = rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.AtomicCounter[b].name)));
			pipe.AtomicBuffers[b].Offset = rs.AtomicCounter[b].start;
			pipe.AtomicBuffers[b].Size = rs.AtomicCounter[b].size;
		}
	}
	
	create_array_uninit(pipe.ShaderStorageBuffers, ARRAY_COUNT(rs.ShaderStorage));
	for(int32_t b=0; b < pipe.ShaderStorageBuffers.count; b++)
	{
		if(rs.ShaderStorage[b].name == 0)
		{
			pipe.ShaderStorageBuffers[b].Resource = ResourceId();
			pipe.ShaderStorageBuffers[b].Offset = pipe.ShaderStorageBuffers[b].Size = 0;
		}
		else
		{
			pipe.ShaderStorageBuffers[b].Resource = rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.ShaderStorage[b].name)));
			pipe.ShaderStorageBuffers[b].Offset = rs.ShaderStorage[b].start;
			pipe.ShaderStorageBuffers[b].Size = rs.ShaderStorage[b].size;
		}
	}
	
	create_array_uninit(pipe.Images, ARRAY_COUNT(rs.Images));
	for(int32_t i=0; i < pipe.Images.count; i++)
	{
		if(rs.Images[i].name == 0)
		{
			RDCEraseEl(pipe.Images[i]);
		}
		else
		{
			ResourceId id = rm->GetID(TextureRes(ctx, rs.Images[i].name));
			pipe.Images[i].Resource = rm->GetOriginalID(id);
			pipe.Images[i].Level = rs.Images[i].level;
			pipe.Images[i].Layered = rs.Images[i].layered;
			pipe.Images[i].Layer = rs.Images[i].layer;
			if(rs.Images[i].access == eGL_READ_ONLY)
			{
				pipe.Images[i].readAllowed = true;
				pipe.Images[i].writeAllowed = false;
			}
			else if(rs.Images[i].access == eGL_WRITE_ONLY)
			{
				pipe.Images[i].readAllowed = false;
				pipe.Images[i].writeAllowed = true;
			}
			else
			{
				pipe.Images[i].readAllowed = true;
				pipe.Images[i].writeAllowed = true;
			}
			pipe.Images[i].Format = MakeResourceFormat(gl, eGL_TEXTURE_2D, rs.Images[i].format);

			pipe.Images[i].ResType = m_CachedTextures[id].resType;
		}
	}

	// Vertex post processing and rasterization

	RDCCOMPILE_ASSERT(ARRAY_COUNT(rs.Viewports) == ARRAY_COUNT(rs.DepthRanges), "GL Viewport count does not match depth ranges count");
	create_array_uninit(pipe.m_Rasterizer.Viewports, ARRAY_COUNT(rs.Viewports));
	for (int32_t v = 0; v < pipe.m_Rasterizer.Viewports.count; ++v)
	{
		pipe.m_Rasterizer.Viewports[v].Left = rs.Viewports[v].x;
		pipe.m_Rasterizer.Viewports[v].Bottom = rs.Viewports[v].y;
		pipe.m_Rasterizer.Viewports[v].Width = rs.Viewports[v].width;
		pipe.m_Rasterizer.Viewports[v].Height = rs.Viewports[v].height;
		pipe.m_Rasterizer.Viewports[v].MinDepth = rs.DepthRanges[v].nearZ;
		pipe.m_Rasterizer.Viewports[v].MaxDepth = rs.DepthRanges[v].farZ;
	}

	create_array_uninit(pipe.m_Rasterizer.Scissors, ARRAY_COUNT(rs.Scissors));
	for (int32_t s = 0; s < pipe.m_Rasterizer.Scissors.count; ++s)
	{
		pipe.m_Rasterizer.Scissors[s].Left = rs.Scissors[s].x;
		pipe.m_Rasterizer.Scissors[s].Bottom = rs.Scissors[s].y;
		pipe.m_Rasterizer.Scissors[s].Width = rs.Scissors[s].width;
		pipe.m_Rasterizer.Scissors[s].Height = rs.Scissors[s].height;
		pipe.m_Rasterizer.Scissors[s].Enabled = rs.Scissors[s].enabled;
	}

	int polygonOffsetEnableEnum;
	switch (rs.PolygonMode)
	{
		default:
			RDCWARN("Unexpected value for POLYGON_MODE %x", rs.PolygonMode);
		case eGL_FILL:
			pipe.m_Rasterizer.m_State.FillMode = eFill_Solid;
			polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetFill;
			break;
		case eGL_LINE:
			pipe.m_Rasterizer.m_State.FillMode = eFill_Wireframe;
			polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetLine;
			break;
		case eGL_POINT:
			pipe.m_Rasterizer.m_State.FillMode = eFill_Point;
			polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetPoint;
			break;
	}
	if (rs.Enabled[polygonOffsetEnableEnum])
	{
		pipe.m_Rasterizer.m_State.DepthBias = rs.PolygonOffset[1];
		pipe.m_Rasterizer.m_State.SlopeScaledDepthBias = rs.PolygonOffset[0];
		pipe.m_Rasterizer.m_State.OffsetClamp = rs.PolygonOffset[2];
	}
	else
	{
		pipe.m_Rasterizer.m_State.DepthBias = 0.0f;
		pipe.m_Rasterizer.m_State.SlopeScaledDepthBias = 0.0f;
		pipe.m_Rasterizer.m_State.OffsetClamp = 0.0f;
	}

	if (rs.Enabled[GLRenderState::eEnabled_CullFace])
	{
		switch (rs.CullFace)
		{
		default:
			RDCWARN("Unexpected value for CULL_FACE");
		case eGL_BACK:
			pipe.m_Rasterizer.m_State.CullMode = eCull_Back;
			break;
		case eGL_FRONT:
			pipe.m_Rasterizer.m_State.CullMode = eCull_Front;
			break;
		case eGL_FRONT_AND_BACK:
			pipe.m_Rasterizer.m_State.CullMode = eCull_FrontAndBack;
			break;
		}
	}
	else
	{
		pipe.m_Rasterizer.m_State.CullMode = eCull_None;
	}
	
	RDCASSERT(rs.FrontFace == eGL_CCW || rs.FrontFace == eGL_CW);
	pipe.m_Rasterizer.m_State.FrontCCW = rs.FrontFace == eGL_CCW;
	pipe.m_Rasterizer.m_State.DepthClamp = rs.Enabled[GLRenderState::eEnabled_DepthClamp];

	pipe.m_Rasterizer.m_State.MultisampleEnable = rs.Enabled[GLRenderState::eEnabled_Multisample];
	pipe.m_Rasterizer.m_State.SampleShading = rs.Enabled[GLRenderState::eEnabled_SampleShading];
	pipe.m_Rasterizer.m_State.SampleMask = rs.Enabled[GLRenderState::eEnabled_SampleMask];
	pipe.m_Rasterizer.m_State.SampleMaskValue = rs.SampleMask[0]; // assume number of samples is less than 32
	pipe.m_Rasterizer.m_State.SampleCoverage = rs.Enabled[GLRenderState::eEnabled_SampleCoverage];
	pipe.m_Rasterizer.m_State.SampleCoverageInvert = rs.SampleCoverageInvert;
	pipe.m_Rasterizer.m_State.SampleCoverageValue = rs.SampleCoverage;
	pipe.m_Rasterizer.m_State.SampleAlphaToCoverage = rs.Enabled[GLRenderState::eEnabled_SampleAlphaToCoverage];
	pipe.m_Rasterizer.m_State.SampleAlphaToOne = rs.Enabled[GLRenderState::eEnabled_SampleAlphaToOne];
	pipe.m_Rasterizer.m_State.MinSampleShadingRate = rs.MinSampleShading;

	pipe.m_Rasterizer.m_State.ProgrammablePointSize = rs.Enabled[rs.eEnabled_ProgramPointSize];
	pipe.m_Rasterizer.m_State.PointSize = rs.PointSize;
	pipe.m_Rasterizer.m_State.LineWidth = rs.LineWidth;
	pipe.m_Rasterizer.m_State.PointFadeThreshold = rs.PointFadeThresholdSize;
	pipe.m_Rasterizer.m_State.PointOriginUpperLeft = (rs.PointSpriteOrigin != eGL_LOWER_LEFT);

	// depth and stencil states

	pipe.m_DepthState.DepthEnable = rs.Enabled[GLRenderState::eEnabled_DepthTest];
	pipe.m_DepthState.DepthWrites = rs.DepthWriteMask != 0;
	pipe.m_DepthState.DepthFunc = ToStr::Get(rs.DepthFunc).substr(3);

	pipe.m_DepthState.DepthBounds = rs.Enabled[GLRenderState::eEnabled_DepthBoundsEXT];
	pipe.m_DepthState.NearBound = rs.DepthBounds.nearZ;
	pipe.m_DepthState.FarBound = rs.DepthBounds.farZ;

	pipe.m_StencilState.StencilEnable = rs.Enabled[GLRenderState::eEnabled_StencilTest];
	pipe.m_StencilState.m_FrontFace.ValueMask = rs.StencilFront.valuemask;
	pipe.m_StencilState.m_FrontFace.WriteMask = rs.StencilFront.writemask;
	pipe.m_StencilState.m_FrontFace.Ref = rs.StencilFront.ref;
	pipe.m_StencilState.m_FrontFace.Func = ToStr::Get(rs.StencilFront.func).substr(3);
	pipe.m_StencilState.m_FrontFace.PassOp = ToStr::Get(rs.StencilFront.pass).substr(3);
	pipe.m_StencilState.m_FrontFace.FailOp = ToStr::Get(rs.StencilFront.stencilFail).substr(3);
	pipe.m_StencilState.m_FrontFace.DepthFailOp = ToStr::Get(rs.StencilFront.depthFail).substr(3);
	pipe.m_StencilState.m_BackFace.ValueMask = rs.StencilBack.valuemask;
	pipe.m_StencilState.m_BackFace.WriteMask = rs.StencilBack.writemask;
	pipe.m_StencilState.m_BackFace.Ref = rs.StencilBack.ref;
	pipe.m_StencilState.m_BackFace.Func = ToStr::Get(rs.StencilBack.func).substr(3);
	pipe.m_StencilState.m_BackFace.PassOp = ToStr::Get(rs.StencilBack.pass).substr(3);
	pipe.m_StencilState.m_BackFace.FailOp = ToStr::Get(rs.StencilBack.stencilFail).substr(3);
	pipe.m_StencilState.m_BackFace.DepthFailOp = ToStr::Get(rs.StencilBack.depthFail).substr(3);

	// Frame buffer

	GLuint curDrawFBO = 0;
	gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curDrawFBO);
	GLuint curReadFBO = 0;
	gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint*)&curReadFBO);
	
	GLint numCols = 8;
	gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

	bool rbCol[32] = { false };
	bool rbDepth = false;
	bool rbStencil = false;
	GLuint curCol[32] = { 0 };
	GLuint curDepth = 0;
	GLuint curStencil = 0;

	RDCASSERT(numCols <= 32);

	// we should never bind the true default framebuffer - if the app did, we will have our fake bound
	RDCASSERT(curDrawFBO != 0);
	RDCASSERT(curReadFBO != 0);

	{
		GLenum type = eGL_TEXTURE;
		for(GLint i=0; i < numCols; i++)
		{
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curCol[i]);
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);
			if(type == eGL_RENDERBUFFER) rbCol[i] = true;
		}

		gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curDepth);
		gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);
		if(type == eGL_RENDERBUFFER) rbDepth = true;
		gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curStencil);
		gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);
		if(type == eGL_RENDERBUFFER) rbStencil = true;

		pipe.m_FB.m_DrawFBO.Obj = rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curDrawFBO)));
		create_array_uninit(pipe.m_FB.m_DrawFBO.Color, numCols);
		for(GLint i=0; i < numCols; i++)
		{
			pipe.m_FB.m_DrawFBO.Color[i].Obj = rm->GetOriginalID(rm->GetID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i])));

			if(pipe.m_FB.m_DrawFBO.Color[i].Obj != ResourceId() && !rbCol[i])
			{
				gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, (GLint*)&pipe.m_FB.m_DrawFBO.Color[i].Mip);
				gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint*)&pipe.m_FB.m_DrawFBO.Color[i].Layer);
				if(pipe.m_FB.m_DrawFBO.Color[i].Layer == 0)
					gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, (GLint*)&pipe.m_FB.m_DrawFBO.Color[i].Layer);
			}
		}

		pipe.m_FB.m_DrawFBO.Depth.Obj = rm->GetOriginalID(rm->GetID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
		pipe.m_FB.m_DrawFBO.Stencil.Obj = rm->GetOriginalID(rm->GetID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

		if(pipe.m_FB.m_DrawFBO.Depth.Obj != ResourceId() && !rbDepth)
		{
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, (GLint*)&pipe.m_FB.m_DrawFBO.Depth.Mip);
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint*)&pipe.m_FB.m_DrawFBO.Depth.Layer);
			if(pipe.m_FB.m_DrawFBO.Depth.Layer == 0)
				gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, (GLint*)&pipe.m_FB.m_DrawFBO.Depth.Layer);
		}

		if(pipe.m_FB.m_DrawFBO.Stencil.Obj != ResourceId() && !rbStencil)
		{
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, (GLint*)&pipe.m_FB.m_DrawFBO.Stencil.Mip);
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint*)&pipe.m_FB.m_DrawFBO.Stencil.Layer);
			if(pipe.m_FB.m_DrawFBO.Stencil.Layer == 0)
				gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, (GLint*)&pipe.m_FB.m_DrawFBO.Stencil.Layer);
		}

		create_array_uninit(pipe.m_FB.m_DrawFBO.DrawBuffers, numCols);
		for(GLint i=0; i < numCols; i++)
		{
			GLenum b = eGL_NONE;
			gl.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&b);
			if(b >= eGL_COLOR_ATTACHMENT0 && b <= GLenum(eGL_COLOR_ATTACHMENT0+numCols))
				pipe.m_FB.m_DrawFBO.DrawBuffers[i] = b-eGL_COLOR_ATTACHMENT0;
			else
				pipe.m_FB.m_DrawFBO.DrawBuffers[i] = -1;
		}

		pipe.m_FB.m_DrawFBO.ReadBuffer = -1;
	}

	{
		GLenum type = eGL_TEXTURE;
		for(GLint i=0; i < numCols; i++)
		{
			gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curCol[i]);
			gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);
			if(type == eGL_RENDERBUFFER) rbCol[i] = true;
		}

		gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curDepth);
		gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);
		if(type == eGL_RENDERBUFFER) rbDepth = true;
		gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curStencil);
		gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint*)&type);
		if(type == eGL_RENDERBUFFER) rbStencil = true;

		pipe.m_FB.m_ReadFBO.Obj = rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curReadFBO)));
		create_array_uninit(pipe.m_FB.m_ReadFBO.Color, numCols);
		for(GLint i=0; i < numCols; i++)
		{
			pipe.m_FB.m_ReadFBO.Color[i].Obj = rm->GetOriginalID(rm->GetID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i])));

			if(pipe.m_FB.m_ReadFBO.Color[i].Obj != ResourceId() && !rbCol[i])
			{
				gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, (GLint*)&pipe.m_FB.m_ReadFBO.Color[i].Mip);
				gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint*)&pipe.m_FB.m_ReadFBO.Color[i].Layer);
				if(pipe.m_FB.m_ReadFBO.Color[i].Layer == 0)
					gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, (GLint*)&pipe.m_FB.m_ReadFBO.Color[i].Layer);
			}
		}

		pipe.m_FB.m_ReadFBO.Depth.Obj = rm->GetOriginalID(rm->GetID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
		pipe.m_FB.m_ReadFBO.Stencil.Obj = rm->GetOriginalID(rm->GetID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));
		
		if(pipe.m_FB.m_ReadFBO.Depth.Obj != ResourceId() && !rbDepth)
		{
			gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, (GLint*)&pipe.m_FB.m_ReadFBO.Depth.Mip);
			gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint*)&pipe.m_FB.m_ReadFBO.Depth.Layer);
			if(pipe.m_FB.m_ReadFBO.Depth.Layer == 0)
				gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, (GLint*)&pipe.m_FB.m_ReadFBO.Depth.Layer);
		}

		if(pipe.m_FB.m_ReadFBO.Stencil.Obj != ResourceId() && !rbStencil)
		{
			gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, (GLint*)&pipe.m_FB.m_ReadFBO.Stencil.Mip);
			gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint*)&pipe.m_FB.m_ReadFBO.Stencil.Layer);
			if(pipe.m_FB.m_ReadFBO.Stencil.Layer == 0)
				gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, (GLint*)&pipe.m_FB.m_ReadFBO.Stencil.Layer);
		}

		create_array_uninit(pipe.m_FB.m_ReadFBO.DrawBuffers, numCols);
		for(GLint i=0; i < numCols; i++)
			pipe.m_FB.m_ReadFBO.DrawBuffers[i] = -1;
		
		GLenum b = eGL_NONE;
		gl.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&b);
		if(b >= eGL_COLOR_ATTACHMENT0 && b <= GLenum(eGL_COLOR_ATTACHMENT0+numCols))
			pipe.m_FB.m_DrawFBO.ReadBuffer = b-eGL_COLOR_ATTACHMENT0;
		else
			pipe.m_FB.m_DrawFBO.ReadBuffer = -1;
	}

	memcpy(pipe.m_FB.m_Blending.BlendFactor, rs.BlendColor, sizeof(rs.BlendColor));

	pipe.m_FB.FramebufferSRGB = rs.Enabled[GLRenderState::eEnabled_FramebufferSRGB];
	pipe.m_FB.Dither = rs.Enabled[GLRenderState::eEnabled_Dither];

	RDCCOMPILE_ASSERT(ARRAY_COUNT(rs.Blends) == ARRAY_COUNT(rs.ColorMasks), "Color masks and blends mismatched");
	create_array_uninit(pipe.m_FB.m_Blending.Blends, ARRAY_COUNT(rs.Blends));
	for(size_t i=0; i < ARRAY_COUNT(rs.Blends); i++)
	{
		pipe.m_FB.m_Blending.Blends[i].Enabled = rs.Blends[i].Enabled;
		pipe.m_FB.m_Blending.Blends[i].LogicOp = "";
		if(rs.LogicOp != eGL_NONE && rs.LogicOp != eGL_COPY && rs.Enabled[GLRenderState::eEnabled_ColorLogicOp])
			pipe.m_FB.m_Blending.Blends[i].LogicOp = ToStr::Get(rs.LogicOp).substr(3); // 3 == strlen("GL_")

		pipe.m_FB.m_Blending.Blends[i].m_Blend.Source = BlendString(rs.Blends[i].SourceRGB);
		pipe.m_FB.m_Blending.Blends[i].m_Blend.Destination = BlendString(rs.Blends[i].DestinationRGB);
		pipe.m_FB.m_Blending.Blends[i].m_Blend.Operation = BlendString(rs.Blends[i].EquationRGB);

		pipe.m_FB.m_Blending.Blends[i].m_AlphaBlend.Source = BlendString(rs.Blends[i].SourceAlpha);
		pipe.m_FB.m_Blending.Blends[i].m_AlphaBlend.Destination = BlendString(rs.Blends[i].DestinationAlpha);
		pipe.m_FB.m_Blending.Blends[i].m_AlphaBlend.Operation = BlendString(rs.Blends[i].EquationAlpha);

		pipe.m_FB.m_Blending.Blends[i].WriteMask = 0;
		if(rs.ColorMasks[i].red)   pipe.m_FB.m_Blending.Blends[i].WriteMask |= 1;
		if(rs.ColorMasks[i].green) pipe.m_FB.m_Blending.Blends[i].WriteMask |= 2;
		if(rs.ColorMasks[i].blue)  pipe.m_FB.m_Blending.Blends[i].WriteMask |= 4;
		if(rs.ColorMasks[i].alpha) pipe.m_FB.m_Blending.Blends[i].WriteMask |= 8;
	}

	switch(rs.Hints.Derivatives)
	{
		default:
	  case eGL_DONT_CARE: pipe.m_Hints.Derivatives = eQuality_DontCare; break;
	  case eGL_NICEST:    pipe.m_Hints.Derivatives = eQuality_Nicest; break;
	  case eGL_FASTEST:   pipe.m_Hints.Derivatives = eQuality_Fastest; break;
	}

	switch(rs.Hints.LineSmooth)
	{
		default:
	  case eGL_DONT_CARE: pipe.m_Hints.LineSmooth = eQuality_DontCare; break;
	  case eGL_NICEST:    pipe.m_Hints.LineSmooth = eQuality_Nicest; break;
	  case eGL_FASTEST:   pipe.m_Hints.LineSmooth = eQuality_Fastest; break;
	}

	switch(rs.Hints.PolySmooth)
	{
		default:
	  case eGL_DONT_CARE: pipe.m_Hints.PolySmooth = eQuality_DontCare; break;
	  case eGL_NICEST:    pipe.m_Hints.PolySmooth = eQuality_Nicest; break;
	  case eGL_FASTEST:   pipe.m_Hints.PolySmooth = eQuality_Fastest; break;
	}

	switch(rs.Hints.TexCompression)
	{
		default:
	  case eGL_DONT_CARE: pipe.m_Hints.TexCompression = eQuality_DontCare; break;
	  case eGL_NICEST:    pipe.m_Hints.TexCompression = eQuality_Nicest; break;
	  case eGL_FASTEST:   pipe.m_Hints.TexCompression = eQuality_Fastest; break;
	}

	pipe.m_Hints.LineSmoothEnabled = rs.Enabled[GLRenderState::eEnabled_LineSmooth];
	pipe.m_Hints.PolySmoothEnabled = rs.Enabled[GLRenderState::eEnabled_PolySmooth];
}

void GLReplay::FillCBufferValue(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, bool rowMajor,
								uint32_t offs, uint32_t matStride, const vector<byte> &data, ShaderVariable &outVar)
{
	const byte *bufdata = data.empty() ? NULL : &data[offs];
	size_t datasize = data.size() - offs;
	if(offs > data.size()) datasize = 0;

	if(bufferBacked)
	{
		size_t rangelen = outVar.rows*outVar.columns*sizeof(float);

		if(outVar.rows > 1 && outVar.columns > 1)
		{
			uint32_t *dest = &outVar.value.uv[0];

			uint32_t majorsize = outVar.columns;
			uint32_t minorsize = outVar.rows;

			if(rowMajor)
			{
				majorsize = outVar.rows;
				minorsize = outVar.columns;
			}

			for(uint32_t c=0; c < majorsize; c++)
			{
				if(datasize > 0)
					memcpy((byte *)dest, bufdata, RDCMIN(rangelen, minorsize*sizeof(float)));

				datasize -= RDCMIN(datasize, (size_t)matStride);
				bufdata += matStride;
				dest += minorsize;
			}
		}
		else
		{
			if(datasize > 0)
				memcpy(&outVar.value.uv[0], bufdata, RDCMIN(rangelen, datasize));
		}
	}
	else
	{
		switch(outVar.type)
		{
			case eVar_Float:
				gl.glGetUniformfv(prog, offs, outVar.value.fv);
				break;
			case eVar_Int:
				gl.glGetUniformiv(prog, offs, outVar.value.iv);
				break;
			case eVar_UInt:
				gl.glGetUniformuiv(prog, offs, outVar.value.uv);
				break;
			case eVar_Double:
				gl.glGetUniformdv(prog, offs, outVar.value.dv);
				break;
		}
	}

	if(!rowMajor)
	{
		if(outVar.type != eVar_Double)
		{
			uint32_t uv[16];
			memcpy(&uv[0], &outVar.value.uv[0], sizeof(uv));

			for(uint32_t r=0; r < outVar.rows; r++)
				for(uint32_t c=0; c < outVar.columns; c++)
					outVar.value.uv[r*outVar.columns+c] = uv[c*outVar.rows+r];
		}
		else
		{
			double dv[16];
			memcpy(&dv[0], &outVar.value.dv[0], sizeof(dv));

			for(uint32_t r=0; r < outVar.rows; r++)
				for(uint32_t c=0; c < outVar.columns; c++)
					outVar.value.dv[r*outVar.columns+c] = dv[c*outVar.rows+r];
		}
	}
}

void GLReplay::FillCBufferVariables(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, string prefix,
                                    const rdctype::array<ShaderConstant> &variables, vector<ShaderVariable> &outvars,
                                    const vector<byte> &data)
{
	for(int32_t i=0; i < variables.count; i++)
	{
		auto desc = variables[i].type.descriptor;

		ShaderVariable var;
		var.name = variables[i].name.elems;
		var.rows = desc.rows;
		var.columns = desc.cols;
		var.type = desc.type;

		if(variables[i].type.members.count > 0)
		{
			if(desc.elements == 0)
			{
				vector<ShaderVariable> ov;
				FillCBufferVariables(gl, prog, bufferBacked, prefix + var.name.elems + ".", variables[i].type.members, ov, data);
				var.isStruct = true;
				var.members = ov;
			}
			else
			{
				vector<ShaderVariable> arrelems;
				for(uint32_t a=0; a < desc.elements; a++)
				{
					ShaderVariable arrEl = var;
					arrEl.name = StringFormat::Fmt("%s[%u]", var.name.elems, a);
					
					vector<ShaderVariable> ov;
					FillCBufferVariables(gl, prog, bufferBacked, prefix + arrEl.name.elems + ".", variables[i].type.members, ov, data);
					arrEl.members = ov;

					arrEl.isStruct = true;
					
					arrelems.push_back(arrEl);
				}
				var.members = arrelems;
				var.isStruct = false;
				var.rows = var.columns = 0;
			}
		}
		else
		{
			RDCEraseEl(var.value);
			
			// need to query offset and strides as there's no way to know what layout was used
			// (and if it's not an std layout it's implementation defined :( )
			string fullname = prefix + var.name.elems;

			GLuint idx = gl.glGetProgramResourceIndex(prog, eGL_UNIFORM, fullname.c_str());

			if(idx == GL_INVALID_INDEX)
			{
				RDCERR("Can't find program resource index for %s", fullname.c_str());
			}
			else
			{
				GLenum props[] = { eGL_OFFSET, eGL_MATRIX_STRIDE, eGL_ARRAY_STRIDE, eGL_LOCATION };
				GLint values[] = { 0, 0, 0, 0 };

				gl.glGetProgramResourceiv(prog, eGL_UNIFORM, idx, ARRAY_COUNT(props), props, ARRAY_COUNT(props), NULL, values);

				if(!bufferBacked)
				{
					values[0] = values[3];
					values[2] = 1;
				}

				if(desc.elements == 0)
				{
					FillCBufferValue(gl, prog, bufferBacked, desc.rowMajorStorage ? true : false,
						values[0], values[1], data, var);
				}
				else
				{
					vector<ShaderVariable> elems;
					for(uint32_t a=0; a < desc.elements; a++)
					{
						ShaderVariable el = var;
						el.name = StringFormat::Fmt("%s[%u]", var.name.elems, a);

						FillCBufferValue(gl, prog, bufferBacked, desc.rowMajorStorage ? true : false,
							values[0] + values[2] * a, values[1], data, el);

						el.isStruct = false;

						elems.push_back(el);
					}

					var.members = elems;
					var.isStruct = false;
					var.rows = var.columns = 0;
				}
			}
		}

		outvars.push_back(var);
	}
}

void GLReplay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(&m_ReplayCtx);

	auto &shaderDetails = m_pDriver->m_Shaders[shader];

	if((int32_t)cbufSlot >= shaderDetails.reflection.ConstantBlocks.count)
	{
		RDCERR("Requesting invalid constant block");
		return;
	}
	
	GLuint curProg = 0;
	gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint*)&curProg);

	if(curProg == 0)
	{
		gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint*)&curProg);
	
		if(curProg == 0)
		{
			RDCERR("No program or pipeline bound");
			return;
		}
		else
		{
			ResourceId id = m_pDriver->GetResourceManager()->GetID(ProgramPipeRes(m_ReplayCtx.ctx, curProg));
			auto &pipeDetails = m_pDriver->m_Pipelines[id];

			size_t s = ShaderIdx(shaderDetails.type);

			curProg = m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[s]).name;
		}
	}

	auto cblock = shaderDetails.reflection.ConstantBlocks.elems[cbufSlot];
	
	FillCBufferVariables(gl, curProg, cblock.bufferBacked ? true : false, "", cblock.variables, outvars, data);
}

byte *GLReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	auto &texDetails = m_pDriver->m_Textures[tex];

	byte *ret = NULL;

	GLuint tempTex = 0;

	GLenum texType = texDetails.curType;
	GLuint texname = texDetails.resource.name;
	GLenum intFormat = texDetails.internalFormat;
	GLsizei width = RDCMAX(1, texDetails.width>>mip);
	GLsizei height = RDCMAX(1, texDetails.height>>mip);
	GLsizei depth = RDCMAX(1, texDetails.depth>>mip);
	GLsizei arraysize = 1;
	GLint samples = texDetails.samples;

	if(texType == eGL_TEXTURE_BUFFER)
	{
		GLuint bufName = 0;
		gl.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_DATA_STORE_BINDING, (GLint *)&bufName);
		ResourceId id = m_pDriver->GetResourceManager()->GetID(BufferRes(m_pDriver->GetCtx(), bufName));

		GLuint offs = 0, size = 0;
		gl.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_OFFSET, (GLint *)&offs);
		gl.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_SIZE, (GLint *)&size);

		vector<byte> data;
		GetBufferData(id, offs, size, data);

		dataSize = data.size();
		ret = new byte[dataSize];
		memcpy(ret, &data[0], dataSize);

		return ret;
	}

	if(texType == eGL_TEXTURE_2D_ARRAY ||
		texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
		texType == eGL_TEXTURE_1D_ARRAY ||
		texType == eGL_TEXTURE_CUBE_MAP ||
		texType == eGL_TEXTURE_CUBE_MAP_ARRAY)
	{
		// array size doesn't get mip'd down
		depth = texDetails.depth;
		arraysize = texDetails.depth;
	}

	if(forceRGBA8unorm && intFormat != eGL_RGBA8 && intFormat != eGL_SRGB8_ALPHA8)
	{
		MakeCurrentReplayContext(m_DebugCtx);

		GLenum finalFormat = IsSRGBFormat(intFormat) ? eGL_SRGB8_ALPHA8 : eGL_RGBA8;
		GLenum newtarget = (texType == eGL_TEXTURE_3D ? eGL_TEXTURE_3D : eGL_TEXTURE_2D);

		// create temporary texture of width/height in RGBA8 format to render to
		gl.glGenTextures(1, &tempTex);
		gl.glBindTexture(newtarget, tempTex);
		if(newtarget == eGL_TEXTURE_3D)
			gl.glTextureStorage3DEXT(tempTex, newtarget, 1, finalFormat, width, height, depth);
		else
			gl.glTextureStorage2DEXT(tempTex, newtarget, 1, finalFormat, width, height);

		// create temp framebuffer
		GLuint fbo = 0;
		gl.glGenFramebuffers(1, &fbo);
		gl.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);
		
		gl.glTexParameteri(newtarget, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
		gl.glTexParameteri(newtarget, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
		gl.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
		gl.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
		gl.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_R, eGL_CLAMP_TO_EDGE);
		if(newtarget == eGL_TEXTURE_3D)
			gl.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D, tempTex, 0, 0);
		else
			gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, tempTex, 0);

		float col[] = { 0.3f, 0.6f, 0.9f, 1.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, col);

		// render to the temp texture to do the downcast
		float oldW = DebugData.outWidth;
		float oldH = DebugData.outHeight;

		DebugData.outWidth = float(width); DebugData.outHeight = float(height);
		
		for(GLsizei d=0; d < (newtarget == eGL_TEXTURE_3D ? depth : 1); d++)
		{
			TextureDisplay texDisplay;
			
			texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
			texDisplay.HDRMul = -1.0f;
			texDisplay.linearDisplayAsGamma = false;
			texDisplay.overlay = eTexOverlay_None;
			texDisplay.FlipY = false;
			texDisplay.mip = mip;
			texDisplay.sampleIdx = ~0U;
			texDisplay.CustomShader = ResourceId();
			texDisplay.sliceFace = arrayIdx;
			texDisplay.rangemin = blackPoint;
			texDisplay.rangemax = whitePoint;
			texDisplay.scale = 1.0f;
			texDisplay.texid = tex;
			texDisplay.rawoutput = false;
			texDisplay.offx = 0;
			texDisplay.offy = 0;
			
			if(newtarget == eGL_TEXTURE_3D)
			{
				gl.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D, tempTex, 0, (GLint)d);
				texDisplay.sliceFace = (uint32_t)d;
			}

			gl.glViewport(0, 0, width, height);

			RenderTextureInternal(texDisplay, false);
		}

		DebugData.outWidth = oldW; DebugData.outHeight = oldH;
		
		// rewrite the variables to temporary texture
		texType = newtarget;
		texname = tempTex;
		intFormat = finalFormat;
		if(newtarget == eGL_TEXTURE_2D) depth = 1;
		arraysize = 1;
		samples = 1;

		gl.glDeleteFramebuffers(1, &fbo);
	}
	else if(resolve && samples > 1)
	{
		MakeCurrentReplayContext(m_DebugCtx);
		
		GLuint curDrawFBO = 0;
		GLuint curReadFBO = 0;
		gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curDrawFBO);
		gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint*)&curReadFBO);
		
		// create temporary texture of width/height in same format to render to
		gl.glGenTextures(1, &tempTex);
		gl.glBindTexture(eGL_TEXTURE_2D, tempTex);
		gl.glTextureStorage2DEXT(tempTex, eGL_TEXTURE_2D, 1, intFormat, width, height);

		// create temp framebuffers
		GLuint fbos[2] = { 0 };
		gl.glGenFramebuffers(2, fbos);

		gl.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[0]);
		gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, tempTex, 0);

		gl.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[1]);
		if(texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
			gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texname, 0, arrayIdx);
		else
			gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texname, 0);
		
		// do default resolve (framebuffer blit)
		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbos[0]);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, fbos[1]);

		float col[] = { 0.3f, 0.4f, 0.5f, 1.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, col);

		gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, eGL_NEAREST);
		
		// rewrite the variables to temporary texture
		texType = eGL_TEXTURE_2D;
		texname = tempTex;
		depth = 1;
		arraysize = 1;
		samples = 1;

		gl.glDeleteFramebuffers(2, fbos);

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);
	}
	else if(samples > 1)
	{
		MakeCurrentReplayContext(m_DebugCtx);

		// create temporary texture array of width/height in same format to render to,
		// with the same number of array slices as multi samples.
		gl.glGenTextures(1, &tempTex);
		gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, tempTex);
		gl.glTextureStorage3DEXT(tempTex, eGL_TEXTURE_2D_ARRAY, 1, intFormat, width, height, arraysize*samples);

		// copy multisampled texture to an array
		CopyTex2DMSToArray(tempTex, texname, width, height, arraysize, samples, intFormat);
		
		// rewrite the variables to temporary texture
		texType = eGL_TEXTURE_2D_ARRAY;
		texname = tempTex;
		depth = 1;
		depth = samples;
		arraysize = samples;
		samples = 1;
	}

	// fetch and return data now
	{
		PixelUnpackState unpack;

		unpack.Fetch(&gl.GetHookset(), true);

		PixelUnpackState identity = {0};
		identity.alignment = 1;

		identity.Apply(&gl.GetHookset(), true);

		GLenum binding = TextureBinding(texType);
		
		GLuint prevtex = 0;
		gl.glGetIntegerv(binding, (GLint *)&prevtex);
		
		gl.glBindTexture(texType, texname);
		
		GLenum target = texType;
		if(texType == eGL_TEXTURE_CUBE_MAP)
		{
			GLenum targets[] = {
				eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
				eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
				eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
			};
			
			RDCASSERT(arrayIdx < ARRAY_COUNT(targets));
			target = targets[arrayIdx];
		}

		if(IsCompressedFormat(intFormat))
		{
			GLuint compSize;
			gl.glGetTexLevelParameteriv(target, mip, eGL_TEXTURE_COMPRESSED_IMAGE_SIZE, (GLint *)&compSize);

			dataSize = compSize;

			ret = new byte[dataSize];

			gl.glGetCompressedTexImage(target, mip, ret);
		}
		else
		{
			GLenum fmt = GetBaseFormat(intFormat);
			GLenum type = GetDataType(intFormat);

			dataSize = GetByteSize(width, height, depth, fmt, type);
			ret = new byte[dataSize];

			m_pDriver->glGetTexImage(target, (GLint)mip, fmt, type, ret);

			// need to vertically flip the image now to get conventional row ordering
			// we either do this when copying out the slice of interest, or just
			// on its own
			size_t rowSize = GetByteSize(width, 1, 1, fmt, type);
			byte *src, *dst;

			// for arrays just extract the slice we're interested in.
			if(texType == eGL_TEXTURE_2D_ARRAY ||
				texType == eGL_TEXTURE_1D_ARRAY ||
				texType == eGL_TEXTURE_CUBE_MAP_ARRAY)
			{
				dataSize = GetByteSize(width, height, 1, fmt, type);
				byte *slice = new byte[dataSize];

				// src points to the last row in the array slice image
				src = (ret + dataSize*arrayIdx) + (height-1)*rowSize;
				dst = slice;

				// we do memcpy + vertical flip
				//memcpy(slice, ret + dataSize*arrayIdx, dataSize);

				for(GLsizei i=0; i < height; i++)
				{
					memcpy(dst, src, rowSize);

					dst += rowSize;
					src -= rowSize;
				}

				delete[] ret;

				ret = slice;
			}
			else
			{
				byte *row = new byte[rowSize];
				
				size_t sliceSize = GetByteSize(width, height, 1, fmt, type);

				// invert all slices in a 3D texture
				for(GLsizei d=0; d < depth; d++)
				{
					dst = ret + d*sliceSize;
					src = dst + (height-1)*rowSize;

					for(GLsizei i=0; i < height>>1; i++)
					{
						memcpy(row, src, rowSize);
						memcpy(src, dst, rowSize);
						memcpy(dst, row, rowSize);

						dst += rowSize;
						src -= rowSize;
					}
				}

				delete[] row;
			}
		}
		
		unpack.Apply(&gl.GetHookset(), true);

		gl.glBindTexture(texType, prevtex);
	}

	if(tempTex)
		gl.glDeleteTextures(1, &tempTex);

	return ret;
}

void GLReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	if(id == NULL || errors == NULL)
	{
		if(id) *id = ResourceId();
		return;
	}

	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(m_DebugCtx);

	GLenum shtype = eGL_VERTEX_SHADER;
	switch(type)
	{
		default: RDCWARN("Unknown shader type %u", type);
		case eShaderStage_Vertex:       shtype = eGL_VERTEX_SHADER; break;
		case eShaderStage_Tess_Control: shtype = eGL_TESS_CONTROL_SHADER; break;
		case eShaderStage_Tess_Eval:    shtype = eGL_TESS_EVALUATION_SHADER; break;
		case eShaderStage_Geometry:     shtype = eGL_GEOMETRY_SHADER; break;
		case eShaderStage_Fragment:     shtype = eGL_FRAGMENT_SHADER; break;
		case eShaderStage_Compute:      shtype = eGL_COMPUTE_SHADER; break;
	}
	
	const char *src = source.c_str();
	GLuint shaderprog = gl.glCreateShaderProgramv(shtype, 1, &src);
	
	GLint status = 0;
	gl.glGetProgramiv(shaderprog, eGL_LINK_STATUS, &status);

	if(errors)
	{
		GLint len = 1024;
		gl.glGetProgramiv(shaderprog, eGL_INFO_LOG_LENGTH, &len);
		char *buffer = new char[len+1];
		gl.glGetProgramInfoLog(shaderprog, len, NULL, buffer); buffer[len] = 0;
		*errors = buffer;
		delete[] buffer;
	}

	if(status == 0)
		*id = ResourceId();
	else
		*id = m_pDriver->GetResourceManager()->GetID(ProgramRes(m_pDriver->GetCtx(), shaderprog));
}

ResourceId GLReplay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
{
	if(shader == ResourceId() || texid == ResourceId()) return ResourceId();

	auto &texDetails = m_pDriver->m_Textures[texid];

	MakeCurrentReplayContext(m_DebugCtx);
	
	CreateCustomShaderTex(texDetails.width, texDetails.height);

	m_pDriver->glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.customFBO);
	m_pDriver->glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D, DebugData.customTex, 0);

	m_pDriver->glViewport(0, 0, texDetails.width, texDetails.height);

	DebugData.outWidth = float(texDetails.width); DebugData.outHeight = float(texDetails.height);
	
	float clr[] = { 0.0f, 0.8f, 0.0f, 0.0f };
	m_pDriver->glClearBufferfv(eGL_COLOR, 0, clr);

	TextureDisplay disp;
	disp.Red = disp.Green = disp.Blue = disp.Alpha = true;
	disp.FlipY = false;
	disp.offx = 0.0f;
	disp.offy = 0.0f;
	disp.CustomShader = shader;
	disp.texid = texid;
	disp.lightBackgroundColour = disp.darkBackgroundColour = FloatVector(0,0,0,0);
	disp.HDRMul = -1.0f;
	disp.linearDisplayAsGamma = true;
	disp.mip = mip;
	disp.sampleIdx = 0;
	disp.overlay = eTexOverlay_None;
	disp.rangemin = 0.0f;
	disp.rangemax = 1.0f;
	disp.rawoutput = false;
	disp.scale = 1.0f;
	disp.sliceFace = 0;

	RenderTextureInternal(disp, false);

	return DebugData.CustomShaderTexID;
}

void GLReplay::CreateCustomShaderTex(uint32_t w, uint32_t h)
{
	if(DebugData.customTex)
	{
		uint32_t oldw = 0, oldh = 0;
		m_pDriver->glGetTextureLevelParameterivEXT(DebugData.customTex, eGL_TEXTURE_2D, 0, eGL_TEXTURE_WIDTH, (GLint *)&oldw);
		m_pDriver->glGetTextureLevelParameterivEXT(DebugData.customTex, eGL_TEXTURE_2D, 0, eGL_TEXTURE_HEIGHT, (GLint *)&oldh);

		if(oldw == w && oldh == h)
			return;

		m_pDriver->glDeleteTextures(1, &DebugData.customTex);
		DebugData.customTex = 0;
	}
	
	m_pDriver->glGenTextures(1, &DebugData.customTex);
	m_pDriver->glBindTexture(eGL_TEXTURE_2D, DebugData.customTex);
	m_pDriver->glTextureStorage2DEXT(DebugData.customTex, eGL_TEXTURE_2D, 1, eGL_RGBA16F, (GLsizei)w, (GLsizei)h);
	m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_BASE_LEVEL, 0);
	m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 1);
	m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

	DebugData.CustomShaderTexID = m_pDriver->GetResourceManager()->GetID(TextureRes(m_pDriver->GetCtx(), DebugData.customTex));
}

void GLReplay::FreeCustomShader(ResourceId id)
{
	if(id == ResourceId()) return;

	m_pDriver->glDeleteProgram(m_pDriver->GetResourceManager()->GetCurrentResource(id).name);
}

void GLReplay::BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	if(id == NULL || errors == NULL)
	{
		if(id) *id = ResourceId();
		return;
	}

	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(m_DebugCtx);

	GLenum shtype = eGL_VERTEX_SHADER;
	switch(type)
	{
		default: RDCWARN("Unknown shader type %u", type);
		case eShaderStage_Vertex:       shtype = eGL_VERTEX_SHADER; break;
		case eShaderStage_Tess_Control: shtype = eGL_TESS_CONTROL_SHADER; break;
		case eShaderStage_Tess_Eval:    shtype = eGL_TESS_EVALUATION_SHADER; break;
		case eShaderStage_Geometry:     shtype = eGL_GEOMETRY_SHADER; break;
		case eShaderStage_Fragment:     shtype = eGL_FRAGMENT_SHADER; break;
		case eShaderStage_Compute:      shtype = eGL_COMPUTE_SHADER; break;
	}
	
	const char *src = source.c_str();
	GLuint shader = gl.glCreateShader(shtype);
	gl.glShaderSource(shader, 1, &src, NULL);
	gl.glCompileShader(shader);
	
	GLint status = 0;
	gl.glGetShaderiv(shader, eGL_COMPILE_STATUS, &status);

	if(errors)
	{
		GLint len = 1024;
		gl.glGetShaderiv(shader, eGL_INFO_LOG_LENGTH, &len);
		char *buffer = new char[len+1];
		gl.glGetShaderInfoLog(shader, len, NULL, buffer); buffer[len] = 0;
		*errors = buffer;
		delete[] buffer;
	}

	if(status == 0)
		*id = ResourceId();
	else
		*id = m_pDriver->GetResourceManager()->GetID(ShaderRes(m_pDriver->GetCtx(), shader));
}

void GLReplay::ReplaceResource(ResourceId from, ResourceId to)
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->ReplaceResource(from, to);
}

void GLReplay::RemoveReplacement(ResourceId id)
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->RemoveReplacement(id);
}

void GLReplay::FreeTargetResource(ResourceId id)
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->FreeTargetResource(id);
}

ResourceId GLReplay::CreateProxyTexture(FetchTexture templateTex)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	GLuint tex = 0;
	gl.glGenTextures(1, &tex);

	GLenum intFormat = MakeGLFormat(gl, templateTex.format);
	
	switch(templateTex.resType)
	{
		case eResType_None:
			break;
		case eResType_Buffer:
		case eResType_Texture1D:
		{
			gl.glBindTexture(eGL_TEXTURE_1D, tex);
			gl.glTextureStorage1DEXT(tex, eGL_TEXTURE_1D, templateTex.mips, intFormat, templateTex.width);
			break;
		}
		case eResType_Texture1DArray:
		{
			gl.glBindTexture(eGL_TEXTURE_1D_ARRAY, tex);
			gl.glTextureStorage2DEXT(tex, eGL_TEXTURE_1D_ARRAY, templateTex.mips, intFormat, templateTex.width, templateTex.arraysize);
			break;
		}
		case eResType_TextureRect:
		case eResType_Texture2D:
		{
			gl.glBindTexture(eGL_TEXTURE_2D, tex);
			gl.glTextureStorage2DEXT(tex, eGL_TEXTURE_2D, templateTex.mips, intFormat, templateTex.width, templateTex.height);
			break;
		}
		case eResType_Texture2DArray:
		{
			gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, tex);
			gl.glTextureStorage3DEXT(tex, eGL_TEXTURE_2D_ARRAY, templateTex.mips, intFormat, templateTex.width, templateTex.height, templateTex.arraysize);
			break;
		}
		case eResType_Texture2DMS:
		{
			gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, tex);
			gl.glTextureStorage2DMultisampleEXT(tex, eGL_TEXTURE_2D_MULTISAMPLE, templateTex.msSamp, intFormat, templateTex.width, templateTex.height, GL_TRUE);
			break;
		}
		case eResType_Texture2DMSArray:
		{
			gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex);
			gl.glTextureStorage3DMultisampleEXT(tex, eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, templateTex.msSamp, intFormat, templateTex.width, templateTex.height, templateTex.arraysize, GL_TRUE);
			break;
		}
		case eResType_Texture3D:
		{
			gl.glBindTexture(eGL_TEXTURE_3D, tex);
			gl.glTextureStorage3DEXT(tex, eGL_TEXTURE_3D, templateTex.mips, intFormat, templateTex.width, templateTex.height, templateTex.depth);
			break;
		}
		case eResType_TextureCube:
		{
			gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, tex);
			gl.glTextureStorage2DEXT(tex, eGL_TEXTURE_CUBE_MAP, templateTex.mips, intFormat, templateTex.width, templateTex.height);
			break;
		}
		case eResType_TextureCubeArray:
		{
			gl.glBindTexture(eGL_TEXTURE_CUBE_MAP_ARRAY, tex);
			gl.glTextureStorage3DEXT(tex, eGL_TEXTURE_CUBE_MAP_ARRAY, templateTex.mips, intFormat, templateTex.width, templateTex.height, templateTex.arraysize);
			break;
		}
		case eResType_Count:
		{
			RDCERR("Invalid shader resource type");
			break;
		}
	}

	if(templateTex.customName)
		gl.glObjectLabel(eGL_TEXTURE, tex, -1, templateTex.name.elems);

	return m_pDriver->GetResourceManager()->GetID(TextureRes(m_pDriver->GetCtx(), tex));
}

void GLReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	GLuint tex = m_pDriver->GetResourceManager()->GetCurrentResource(texid).name;

	auto &texdetails = m_pDriver->m_Textures[texid];
	
	GLenum fmt = texdetails.internalFormat;
	GLenum target = texdetails.curType;

	if(IsCompressedFormat(target))
	{
		if(target == eGL_TEXTURE_1D)
		{
			gl.glCompressedTextureSubImage1DEXT(tex, target, (GLint)mip, 0, texdetails.width, fmt, (GLsizei)dataSize, data);
		}
		else if(target == eGL_TEXTURE_1D_ARRAY)
		{
			gl.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, (GLint)arrayIdx, texdetails.width, 1, fmt, (GLsizei)dataSize, data);
		}
		else if(target == eGL_TEXTURE_2D)
		{
			gl.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width, texdetails.height, fmt, (GLsizei)dataSize, data);
		}
		else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
		{
			gl.glCompressedTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, (GLint)arrayIdx, texdetails.width, texdetails.height, 1, fmt, (GLsizei)dataSize, data);
		}
		else if(target == eGL_TEXTURE_3D)
		{
			gl.glCompressedTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, 0, texdetails.width, texdetails.height, texdetails.depth, fmt, (GLsizei)dataSize, data);
		}
		else if(target == eGL_TEXTURE_CUBE_MAP)
		{
			GLenum targets[] = {
				eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
				eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
				eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
			};
			
			RDCASSERT(arrayIdx < ARRAY_COUNT(targets));
			target = targets[arrayIdx];

			gl.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width, texdetails.height, fmt, (GLsizei)dataSize, data);
		}
		else if(target == eGL_TEXTURE_2D_MULTISAMPLE)
		{
			RDCUNIMPLEMENTED("multisampled proxy textures");
		}
		else if(target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
		{
			RDCUNIMPLEMENTED("multisampled proxy textures");
		}
	}
	else
	{
		GLenum baseformat = GetBaseFormat(fmt);
		GLenum datatype = GetDataType(fmt);

		GLint depth = 1;
		if(target == eGL_TEXTURE_3D) depth = texdetails.depth;

		if(dataSize < GetByteSize(texdetails.width, texdetails.height, depth, baseformat, datatype))
		{
			RDCERR("Insufficient data provided to SetProxyTextureData");
			return;
		}

		if(target == eGL_TEXTURE_1D)
		{
			gl.glTextureSubImage1DEXT(tex, target, (GLint)mip, 0, texdetails.width, baseformat, datatype, data);
		}
		else if(target == eGL_TEXTURE_1D_ARRAY)
		{
			gl.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, (GLint)arrayIdx, texdetails.width, 1, baseformat, datatype, data);
		}
		else if(target == eGL_TEXTURE_2D)
		{
			gl.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width, texdetails.height, baseformat, datatype, data);
		}
		else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
		{
			gl.glTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, (GLint)arrayIdx, texdetails.width, texdetails.height, 1, baseformat, datatype, data);
		}
		else if(target == eGL_TEXTURE_3D)
		{
			gl.glTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, 0, texdetails.width, texdetails.height, texdetails.depth, baseformat, datatype, data);
		}
		else if(target == eGL_TEXTURE_CUBE_MAP)
		{
			GLenum targets[] = {
				eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
				eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
				eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
				eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
			};
			
			RDCASSERT(arrayIdx < ARRAY_COUNT(targets));
			target = targets[arrayIdx];

			gl.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width, texdetails.height, baseformat, datatype, data);
		}
		else if(target == eGL_TEXTURE_2D_MULTISAMPLE)
		{
			RDCUNIMPLEMENTED("multisampled proxy textures");
		}
		else if(target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
		{
			RDCUNIMPLEMENTED("multisampled proxy textures");
		}
	}

}

ResourceId GLReplay::CreateProxyBuffer(FetchBuffer templateBuf)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(m_DebugCtx);

	GLenum target = eGL_ARRAY_BUFFER;
	
	if(templateBuf.creationFlags & eBufferCreate_Indirect)
		target = eGL_DRAW_INDIRECT_BUFFER;
	if(templateBuf.creationFlags & eBufferCreate_IB)
		target = eGL_ELEMENT_ARRAY_BUFFER;
	if(templateBuf.creationFlags & eBufferCreate_CB)
		target = eGL_UNIFORM_BUFFER;
	if(templateBuf.creationFlags & eBufferCreate_UAV)
		target = eGL_SHADER_STORAGE_BUFFER;

	GLuint buf = 0;
	gl.glGenBuffers(1, &buf);
	gl.glBindBuffer(target, buf);
	gl.glNamedBufferStorageEXT(buf, (GLsizeiptr)templateBuf.byteSize, NULL, GL_DYNAMIC_STORAGE_BIT|GL_MAP_READ_BIT);

	if(templateBuf.customName)
		gl.glObjectLabel(eGL_BUFFER, buf, -1, templateBuf.name.elems);

	return m_pDriver->GetResourceManager()->GetID(BufferRes(m_pDriver->GetCtx(), buf));
}

void GLReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
	GLuint buf = m_pDriver->GetResourceManager()->GetCurrentResource(bufid).name;

	m_pDriver->glNamedBufferSubDataEXT(buf, 0, dataSize, data);
}

vector<EventUsage> GLReplay::GetUsage(ResourceId id)
{
	return m_pDriver->GetUsage(id);
}

#pragma endregion






void GLReplay::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	GLNOTIMP("SetContextFilter");
}



vector<PixelModification> GLReplay::PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx)
{
	GLNOTIMP("GLReplay::PixelHistory");
	return vector<PixelModification>();
}




ShaderDebugTrace GLReplay::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	GLNOTIMP("DebugVertex");
	return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	GLNOTIMP("DebugPixel");
	return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	GLNOTIMP("DebugThread");
	return ShaderDebugTrace();
}

const GLHookSet &GetRealGLFunctions();

// defined in gl_replay_<platform>.cpp
ReplayCreateStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver);

static DriverRegistration GLDriverRegistration(RDC_OpenGL, "OpenGL", &GL_CreateReplayDevice);
