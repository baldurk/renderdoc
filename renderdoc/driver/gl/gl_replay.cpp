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


#include "gl_replay.h"
#include "gl_driver.h"
#include "gl_resources.h"

#include "common/string_utils.h"

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
	CloseReplayContext();

	delete m_pDriver;
}

#pragma region Implemented

void GLReplay::ReadLogInitialisation()
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->ReadLogInitialisation();
}

void GLReplay::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	MakeCurrentReplayContext(&m_ReplayCtx);
	m_pDriver->ReplayLog(frameID, startEventID, endEventID, replayType);
}

vector<FetchFrameRecord> GLReplay::GetFrameRecord()
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
	
	for(auto it=m_pDriver->m_Textures.begin(); it != m_pDriver->m_Textures.end(); ++it)
		ret.push_back(it->first);

	return ret;
}

void GLReplay::SetReplayData(GLWindowingData data)
{
	m_ReplayCtx = data;
	
	InitDebugData();
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

void GLReplay::CreateOutputWindowBackbuffer(OutputWindow &outwin)
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
	
	gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_RGB8, outwin.width, outwin.height); 
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, outwin.BlitData.backbuffer, 0);

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
	
		gl.glDeleteTextures(1, &outw.BlitData.backbuffer);
		gl.glDeleteFramebuffers(1, &outw.BlitData.windowFBO);

		CreateOutputWindowBackbuffer(outw);

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

	DebugData.outWidth = float(outw.width); DebugData.outHeight = float(outw.height);
}

void GLReplay::ClearOutputWindowColour(uint64_t id, float col[4])
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	MakeCurrentReplayContext(m_DebugCtx);

	m_pDriver->glClearBufferfv(eGL_COLOR, 0, col);
}

void GLReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	MakeCurrentReplayContext(&outw);

	m_pDriver->glClearBufferfv(eGL_DEPTH, 0, &depth);
}

void GLReplay::FlipOutputWindow(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	OutputWindow &outw = m_OutputWindows[id];
	
	MakeCurrentReplayContext(&outw);

	WrappedOpenGL &gl = *m_pDriver;

	gl.glBindFramebuffer(eGL_FRAMEBUFFER, 0);
	gl.glViewport(0, 0, outw.width, outw.height);
	
	gl.glUseProgram(DebugData.blitProg);

	gl.glActiveTexture(eGL_TEXTURE0);
	gl.glBindTexture(eGL_TEXTURE_2D, outw.BlitData.backbuffer);
	
	gl.glBindVertexArray(outw.BlitData.emptyVAO);
	gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

	SwapBuffers(&outw);
}

vector<byte> GLReplay::GetBufferData(ResourceId buff, uint32_t offset, uint32_t len)
{
	vector<byte> ret;

	if(m_pDriver->m_Buffers.find(buff) == m_pDriver->m_Buffers.end())
	{
		RDCWARN("Requesting data for non-existant buffer %llu", buff);
		return ret;
	}

	auto &buf = m_pDriver->m_Buffers[buff];
	
	if(len > 0 && offset+len > buf.size)
	{
		RDCWARN("Attempting to read off the end of the array. Will be clamped");
		len = RDCMIN(len, uint32_t(buf.size-offset));
	}
	else if(len == 0)
	{
		len = (uint32_t)buf.size;
	}
	
	ret.resize(len);
	
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(m_DebugCtx);

	gl.glBindBuffer(eGL_COPY_READ_BUFFER, buf.resource.name);

	gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)len, &ret[0]);

	return ret;
}

bool GLReplay::IsRenderOutput(ResourceId id)
{
	for(int32_t i=0; i < m_CurPipelineState.m_FB.Color.count; i++)
	{
		if(m_CurPipelineState.m_FB.Color[i] == id)
				return true;
	}
	
	if(m_CurPipelineState.m_FB.Depth == id ||
		 m_CurPipelineState.m_FB.Stencil == id)
			return true;

	return false;
}

FetchTexture GLReplay::GetTexture(ResourceId id)
{
	FetchTexture tex;
	
	MakeCurrentReplayContext(&m_ReplayCtx);
	
	auto &res = m_pDriver->m_Textures[id];

	if(res.resource.Namespace == eResUnknown)
	{
		RDCERR("Details for invalid texture id %llu requested", id);
		RDCEraseEl(tex);
		return tex;
	}
	
	WrappedOpenGL &gl = *m_pDriver;
	
	tex.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);

	gl.glBindTexture(res.curType, res.resource.name);

	GLenum levelQueryType = res.curType;
	if(levelQueryType == eGL_TEXTURE_CUBE_MAP)
		levelQueryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

	// TODO if I call this for levels 1, 2, .. etc. Can I get sizes that aren't mip dimensions?
	GLint width = 1, height = 1, depth = 1, samples=1;
	gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_WIDTH, &width);
	gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_HEIGHT, &height);
	gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_DEPTH, &depth);
	gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_SAMPLES, &samples);

	if(res.width == 0)
	{
		RDCWARN("TextureData::width didn't get filled out, setting at last minute");
		res.width = width;
	}
	if(res.height == 0)
	{
		RDCWARN("TextureData::height didn't get filled out, setting at last minute");
		res.height = height;
	}
	if(res.depth == 0)
	{
		RDCWARN("TextureData::depth didn't get filled out, setting at last minute");
		res.depth = depth;
	}

	// reasonably common defaults
	tex.msQual = 0;
	tex.msSamp = 1;
	tex.width = tex.height = tex.depth = tex.arraysize = 1;
	tex.cubemap = false;

	switch(res.curType)
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
			tex.depth = (res.curType == eGL_TEXTURE_CUBE_MAP ? 6 : 1);
			tex.cubemap = (res.curType == eGL_TEXTURE_CUBE_MAP);
			tex.msSamp = (res.curType == eGL_TEXTURE_2D_MULTISAMPLE ? samples : 1);
			break;
		case eGL_TEXTURE_2D_ARRAY:
		case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			tex.dimension = 2;
			tex.width = (uint32_t)width;
			tex.height = (uint32_t)height;
			tex.depth = (res.curType == eGL_TEXTURE_CUBE_MAP ? 6 : 1);
			tex.arraysize = depth;
			tex.cubemap = (res.curType == eGL_TEXTURE_CUBE_MAP_ARRAY);
			tex.msSamp = (res.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ? samples : 1);
			break;
		case eGL_TEXTURE_3D:
			tex.dimension = 3;
			tex.width = (uint32_t)width;
			tex.height = (uint32_t)height;
			tex.depth = (uint32_t)depth;
			break;

		default:
			tex.dimension = 2;
			RDCERR("Unexpected texture enum %hs", ToStr::Get(res.curType).c_str());
	}
	
	GLint immut = 0;
	gl.glGetTexParameteriv(res.curType, eGL_TEXTURE_IMMUTABLE_FORMAT, &immut);
	
	if(immut)
	{
		gl.glGetTexParameteriv(res.curType, eGL_TEXTURE_IMMUTABLE_LEVELS, &immut);
		tex.mips = (uint32_t)immut;
	}
	else
	{
		// assuming complete texture
		GLint mips = 1;
		gl.glGetTexParameteriv(res.curType, eGL_TEXTURE_MAX_LEVEL, &mips);
		tex.mips = (uint32_t)mips;
	}

	tex.numSubresources = tex.mips*tex.arraysize;
	
	// surely this will be the same for each level... right? that would be insane if it wasn't
	GLint fmt = 0;
	gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, &fmt);

	tex.format = MakeResourceFormat(gl, res.curType, (GLenum)fmt);
	
	string str = "";
	char name[128] = {0};
	gl.glGetObjectLabel(eGL_TEXTURE, res.resource.name, 127, NULL, name);
	str = name;
	tex.customName = true;

	if(str == "")
	{
		tex.customName = false;
		str = StringFormat::Fmt("Texture%dD %llu", tex.dimension, tex.ID);
	}

	tex.name = widen(str);

	tex.creationFlags = res.creationFlags;
	if(tex.format.compType == eCompType_Depth)
		tex.creationFlags |= eTextureCreate_DSV;
	if(res.resource.name == gl.m_FakeBB_Color || res.resource.name == gl.m_FakeBB_DepthStencil)
		tex.creationFlags |= eTextureCreate_SwapBuffer;

	GLint compressed;
	gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_COMPRESSED, &compressed);
	tex.byteSize = 0;
	for(uint32_t a=0; a < tex.arraysize; a++)
	{
		for(uint32_t m=0; m < tex.mips; m++)
		{
			if(compressed)
			{
				gl.glGetTexLevelParameteriv(levelQueryType, m, eGL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compressed);
				tex.byteSize += compressed;
			}
			else if(tex.format.special)
			{
				tex.byteSize += GetByteSize(RDCMAX(1U, tex.width>>m), RDCMAX(1U, tex.height>>m), RDCMAX(1U, tex.depth>>m), 
																		(GLenum)fmt, (GLenum)fmt, 1);
			}
			else
			{
				tex.byteSize += RDCMAX(1U, tex.width>>m)*RDCMAX(1U, tex.height>>m)*RDCMAX(1U, tex.depth>>m)*
													tex.format.compByteWidth*tex.format.compCount;
			}
		}
	}

	return tex;
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
		ret.customName = false;
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
		case eGL_PIXEL_PACK_BUFFER:
		case eGL_COPY_WRITE_BUFFER:
			break;
		default:
			RDCERR("Unexpected buffer type %hs", ToStr::Get(res.curType).c_str());
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

	ret.name = widen(str);

	return ret;
}

ShaderReflection *GLReplay::GetShader(ResourceId id)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(&m_ReplayCtx);
	
	void *ctx = m_ReplayCtx.ctx;
	
	auto &shaderDetails = m_pDriver->m_Shaders[id];
	
	if(shaderDetails.prog == 0)
	{
		RDCERR("Can't get shader details without separable program");
		return NULL;
	}

	return &shaderDetails.reflection;
}

#pragma endregion

#pragma region Mostly Implemented

void GLReplay::GetMapping(WrappedOpenGL &gl, GLuint curProg, int shadIdx, ShaderReflection *refl, ShaderBindpointMapping &mapping)
{
	// in case of bugs, we readback into this array instead of
	GLint dummyReadback[32];

#if !defined(RELEASE)
	for(size_t i=1; i < ARRAY_COUNT(dummyReadback); i++)
		dummyReadback[i] = 0x6c7b8a9d;
#endif

	GLenum refEnum[] = {
		eGL_REFERENCED_BY_VERTEX_SHADER,
		eGL_REFERENCED_BY_TESS_CONTROL_SHADER,
		eGL_REFERENCED_BY_TESS_EVALUATION_SHADER,
		eGL_REFERENCED_BY_GEOMETRY_SHADER,
		eGL_REFERENCED_BY_FRAGMENT_SHADER,
		eGL_REFERENCED_BY_COMPUTE_SHADER,
	};
	
	create_array_uninit(mapping.Resources, refl->Resources.count);
	for(int32_t i=0; i < refl->Resources.count; i++)
	{
		if(refl->Resources.elems[i].IsSRV && refl->Resources.elems[i].IsTexture)
		{
			GLint loc = gl.glGetUniformLocation(curProg, refl->Resources.elems[i].name.elems);
			if(loc >= 0)
			{
				gl.glGetUniformiv(curProg, loc, dummyReadback);
				mapping.Resources[i].bind = dummyReadback[0];
			}
		}
		else
		{
			mapping.Resources[i].bind = -1;
		}

		GLuint idx = gl.glGetProgramResourceIndex(curProg, eGL_UNIFORM, refl->Resources.elems[i].name.elems);
		if(idx == GL_INVALID_INDEX)
		{
			mapping.Resources[i].used = false;
		}
		else
		{
			GLint used = 0;
			gl.glGetProgramResourceiv(curProg, eGL_UNIFORM, idx, 1, &refEnum[shadIdx], 1, NULL, &used);
			mapping.Resources[i].used = (used != 0);
		}
	}
	
	create_array_uninit(mapping.ConstantBlocks, refl->ConstantBlocks.count);
	for(int32_t i=0; i < refl->ConstantBlocks.count; i++)
	{
		if(refl->ConstantBlocks.elems[i].bufferBacked)
		{
			GLint loc = gl.glGetUniformBlockIndex(curProg, refl->ConstantBlocks.elems[i].name.elems);
			if(loc >= 0)
			{
				gl.glGetActiveUniformBlockiv(curProg, loc, eGL_UNIFORM_BLOCK_BINDING, dummyReadback);
				mapping.ConstantBlocks[i].bind = dummyReadback[0];
			}
		}
		else
		{
			mapping.ConstantBlocks[i].bind = -1;
		}

		if(!refl->ConstantBlocks.elems[i].bufferBacked)
		{
			mapping.ConstantBlocks[i].used = true;
		}
		else
		{
			GLuint idx = gl.glGetProgramResourceIndex(curProg, eGL_UNIFORM_BLOCK, refl->ConstantBlocks.elems[i].name.elems);
			if(idx == GL_INVALID_INDEX)
			{
				mapping.ConstantBlocks[i].used = false;
			}
			else
			{
				GLint used = 0;
				gl.glGetProgramResourceiv(curProg, eGL_UNIFORM_BLOCK, idx, 1, &refEnum[shadIdx], 1, NULL, &used);
				mapping.ConstantBlocks[i].used = (used != 0);
			}
		}
	}

#if !defined(RELEASE)
	for(size_t i=1; i < ARRAY_COUNT(dummyReadback); i++)
		if(dummyReadback[i] != 0x6c7b8a9d)
			RDCERR("Invalid uniform readback - data beyond first element modified!");
#endif
}

void GLReplay::SavePipelineState()
{
	GLPipelineState &pipe = m_CurPipelineState;
	WrappedOpenGL &gl = *m_pDriver;
	GLResourceManager *rm = m_pDriver->GetResourceManager();

	GLRenderState rs(&gl.GetHookset(), NULL);
	rs.FetchState();

	MakeCurrentReplayContext(&m_ReplayCtx);

	// Index buffer

	pipe.m_VtxIn.ibuffer.Offset = m_pDriver->m_LastIndexOffset;
	
	pipe.m_VtxIn.ibuffer.Format = ResourceFormat();
	pipe.m_VtxIn.ibuffer.Format.special = false;
	pipe.m_VtxIn.ibuffer.Format.compCount = 1;
	pipe.m_VtxIn.ibuffer.Format.compType = eCompType_UInt;
	switch(m_pDriver->m_LastIndexSize)
	{
		default:
			break;
		case eGL_UNSIGNED_BYTE:
			pipe.m_VtxIn.ibuffer.Format.compByteWidth = 1;
			pipe.m_VtxIn.ibuffer.Format.strname = L"GL_UNSIGNED_BYTE";
			break;
		case eGL_UNSIGNED_SHORT:
			pipe.m_VtxIn.ibuffer.Format.compByteWidth = 2;
			pipe.m_VtxIn.ibuffer.Format.strname = L"GL_UNSIGNED_SHORT";
			break;
		case eGL_UNSIGNED_INT:
			pipe.m_VtxIn.ibuffer.Format.compByteWidth = 4;
			pipe.m_VtxIn.ibuffer.Format.strname = L"GL_UNSIGNED_INT";
			break;
	}

	void *ctx = m_ReplayCtx.ctx;

	pipe.m_VtxIn.ibuffer.Buffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.BufferBindings[GLRenderState::eBufIdx_Element_Array])));

	// Vertex buffers and attributes
	GLint numVBufferBindings = 16;
	gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);
	
	GLint numVAttribBindings = 16;
	gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numVAttribBindings);

	create_array_uninit(pipe.m_VtxIn.vbuffers, numVBufferBindings);
	create_array_uninit(pipe.m_VtxIn.attributes, numVAttribBindings);

	for(GLuint i=0; i < (GLuint)numVBufferBindings; i++)
	{
		GLint vb = 0;
		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_BUFFER, i, &vb);
		pipe.m_VtxIn.vbuffers[i].Buffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, vb)));

		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Stride);
		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Offset);
		gl.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Divisor);
		pipe.m_VtxIn.vbuffers[i].PerInstance = (pipe.m_VtxIn.vbuffers[i].Divisor != 0);
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

		// TODO should check eGL_VERTEX_ATTRIB_ARRAY_INTEGER

		ResourceFormat fmt;

		fmt.special = false;
		fmt.compCount = 4;
		gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&fmt.compCount);
		
		switch(type)
		{
			default:
			case eGL_BYTE:
				fmt.compByteWidth = 1;
				fmt.compType = normalized ? eCompType_SInt : eCompType_SNorm;
				fmt.strname = StringFormat::WFmt(L"GL_BYTE%d", fmt.compCount) + (normalized ? L"" : L"_SNORM");
				break;
			case eGL_UNSIGNED_BYTE:
				fmt.compByteWidth = 1;
				fmt.compType = normalized ? eCompType_UInt : eCompType_UNorm;
				fmt.strname = StringFormat::WFmt(L"GL_UNSIGNED_BYTE%d", fmt.compCount) + (normalized ? L"" : L"_UNORM");
				break;
			case eGL_SHORT:
				fmt.compByteWidth = 2;
				fmt.compType = normalized ? eCompType_SInt : eCompType_SNorm;
				fmt.strname = StringFormat::WFmt(L"GL_SHORT%d", fmt.compCount) + (normalized ? L"" : L"_SNORM");
				break;
			case eGL_UNSIGNED_SHORT:
				fmt.compByteWidth = 2;
				fmt.compType = normalized ? eCompType_UInt : eCompType_UNorm;
				fmt.strname = StringFormat::WFmt(L"GL_UNSIGNED_SHORT%d", fmt.compCount) + (normalized ? L"" : L"_UNORM");
				break;
			case eGL_INT:
				fmt.compByteWidth = 4;
				fmt.compType = normalized ? eCompType_SInt : eCompType_SNorm;
				fmt.strname = StringFormat::WFmt(L"GL_INT%d", fmt.compCount) + (normalized ? L"" : L"_SNORM");
				break;
			case eGL_UNSIGNED_INT:
				fmt.compByteWidth = 4;
				fmt.compType = normalized ? eCompType_UInt : eCompType_UNorm;
				fmt.strname = StringFormat::WFmt(L"GL_UNSIGNED_INT%d", fmt.compCount) + (normalized ? L"" : L"_UNORM");
				break;
			case eGL_FLOAT:
				fmt.compByteWidth = 4;
				fmt.compType = eCompType_Float;
				fmt.strname = StringFormat::WFmt(L"GL_FLOAT%d", fmt.compCount);
				break;
			case eGL_DOUBLE:
				fmt.compByteWidth = 8;
				fmt.compType = eCompType_Double;
				fmt.strname = StringFormat::WFmt(L"GL_DOUBLE%d", fmt.compCount);
				break;
			case eGL_HALF_FLOAT:
				fmt.compByteWidth = 2;
				fmt.compType = eCompType_Float;
				fmt.strname = StringFormat::WFmt(L"GL_HALF_FLOAT%d", fmt.compCount);
				break;
			case eGL_INT_2_10_10_10_REV:
				fmt.special = true;
				fmt.specialFormat = eSpecial_R10G10B10A2;
				fmt.compCount = 4;
				fmt.compType = eCompType_UInt;
				fmt.strname = L"GL_INT_2_10_10_10_REV";
				break;
			case eGL_UNSIGNED_INT_2_10_10_10_REV:
				fmt.special = true;
				fmt.specialFormat = eSpecial_R10G10B10A2;
				fmt.compCount = 4;
				fmt.compType = eCompType_SInt;
				fmt.strname = L"eGL_UNSIGNED_INT_2_10_10_10_REV";
				break;
			case eGL_UNSIGNED_INT_10F_11F_11F_REV:
				fmt.special = true;
				fmt.specialFormat = eSpecial_R11G11B10;
				fmt.compCount = 3;
				fmt.compType = eCompType_SInt;
				fmt.strname = L"eGL_UNSIGNED_INT_10F_11F_11F_REV";
				break;
		}

		pipe.m_VtxIn.attributes[i].Format = fmt;
	}

	switch(m_pDriver->m_LastDrawMode)
	{
		default:
			pipe.m_VtxIn.Topology = eTopology_Unknown;
			break;
		case GL_POINTS:
			pipe.m_VtxIn.Topology = eTopology_PointList;
			break;
		case GL_LINE_STRIP:
			pipe.m_VtxIn.Topology = eTopology_LineStrip;
			break;
		case GL_LINE_LOOP:
			pipe.m_VtxIn.Topology = eTopology_LineLoop;
			break;
		case GL_LINES:
			pipe.m_VtxIn.Topology = eTopology_LineList;
			break;
		case GL_LINE_STRIP_ADJACENCY:
			pipe.m_VtxIn.Topology = eTopology_LineStrip_Adj;
			break;
		case GL_LINES_ADJACENCY:
			pipe.m_VtxIn.Topology = eTopology_LineList_Adj;
			break;
		case GL_TRIANGLE_STRIP:
			pipe.m_VtxIn.Topology = eTopology_TriangleStrip;
			break;
		case GL_TRIANGLE_FAN:
			pipe.m_VtxIn.Topology = eTopology_TriangleFan;
			break;
		case GL_TRIANGLES:
			pipe.m_VtxIn.Topology = eTopology_TriangleList;
			break;
		case GL_TRIANGLE_STRIP_ADJACENCY:
			pipe.m_VtxIn.Topology = eTopology_TriangleStrip_Adj;
			break;
		case GL_TRIANGLES_ADJACENCY:
			pipe.m_VtxIn.Topology = eTopology_TriangleList_Adj;
			break;
		case GL_PATCHES:
		{
			GLint patchCount = 3;
			gl.glGetIntegerv(eGL_PATCH_VERTICES, &patchCount);
			pipe.m_VtxIn.Topology = PrimitiveTopology(eTopology_PatchList_1CPs+patchCount);
			break;
		}
	}
	
	// Shader stages & Textures
	
	GLint numTexUnits = 8;
	gl.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);
	create_array_uninit(pipe.Textures, numTexUnits);

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
		
	if(curProg == 0)
	{
		gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint*)&curProg);
	
		if(curProg == 0)
		{
			pipe.m_VS.Shader = ResourceId();
			pipe.m_FS.Shader = ResourceId();

			for(GLint unit=0; unit < numTexUnits; unit++)
			{
				pipe.Textures[unit].FirstSlice = 0;
				pipe.Textures[unit].Resource = ResourceId();
			}
		}
		else
		{
			ResourceId id = rm->GetID(ProgramPipeRes(ctx, curProg));
			auto &pipeDetails = m_pDriver->m_Pipelines[id];

			for(size_t i=0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
			{
				if(pipeDetails.stageShaders[i] != ResourceId())
				{
					curProg = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
					stages[i]->Shader = rm->GetOriginalID(pipeDetails.stageShaders[i]);
					refls[i] = GetShader(pipeDetails.stageShaders[i]);
					GetMapping(gl, curProg, (int)i, refls[i], stages[i]->BindpointMapping);
					mappings[i] = &stages[i]->BindpointMapping;
				}
			}
		}
	}
	else
	{
		auto &progDetails = m_pDriver->m_Programs[rm->GetID(ProgramRes(ctx, curProg))];
		
		for(size_t i=0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
		{
			if(progDetails.stageShaders[i] != ResourceId())
			{
				stages[i]->Shader = rm->GetOriginalID(progDetails.stageShaders[i]);
				refls[i] = GetShader(progDetails.stageShaders[i]);
				GetMapping(gl, curProg, (int)i, refls[i], stages[i]->BindpointMapping);
				mappings[i] = &stages[i]->BindpointMapping;
			}
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

		for(size_t s=0; s < ARRAY_COUNT(refls); s++)
		{
			if(refls[s] == NULL) continue;

			for(int32_t r=0; r < refls[s]->Resources.count; r++)
			{
				// bindPoint is the uniform value for this sampler
				if(mappings[s]->Resources[ refls[s]->Resources[r].bindPoint ].bind == unit)
				{
					GLenum t = eGL_NONE;

					switch(refls[s]->Resources[r].resType)
					{
					case eResType_None:
						t = eGL_NONE;
						break;
					case eResType_Buffer:
						t = eGL_TEXTURE_BINDING_BUFFER;
						break;
					case eResType_Texture1D:
						t = eGL_TEXTURE_BINDING_1D;
						target = eGL_TEXTURE_1D;
						break;
					case eResType_Texture1DArray:
						t = eGL_TEXTURE_BINDING_1D_ARRAY;
						target = eGL_TEXTURE_1D_ARRAY;
						break;
					case eResType_Texture2D:
						t = eGL_TEXTURE_BINDING_2D;
						target = eGL_TEXTURE_2D;
						break;
					case eResType_Texture2DArray:
						t = eGL_TEXTURE_BINDING_2D_ARRAY;
						target = eGL_TEXTURE_2D_ARRAY;
						break;
					case eResType_Texture2DMS:
						t = eGL_TEXTURE_BINDING_2D_MULTISAMPLE;
						target = eGL_TEXTURE_2D_MULTISAMPLE;
						break;
					case eResType_Texture2DMSArray:
						t = eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
						target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY;
						break;
					case eResType_Texture3D:
						t = eGL_TEXTURE_BINDING_3D;
						target = eGL_TEXTURE_3D;
						break;
					case eResType_TextureCube:
						t = eGL_TEXTURE_BINDING_CUBE_MAP;
						target = eGL_TEXTURE_CUBE_MAP;
						break;
					case eResType_TextureCubeArray:
						t = eGL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
						target = eGL_TEXTURE_CUBE_MAP_ARRAY;
						break;
					}

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
						RDCWARN("Two uniforms pointing to texture unit %d with types %hs and %hs", unit, ToStr::Get(binding).c_str(), ToStr::Get(t).c_str());
					}
				}
			}
		}

		if(binding != eGL_NONE)
		{
			gl.glActiveTexture(GLenum(eGL_TEXTURE0+unit));

			GLuint tex;
			gl.glGetIntegerv(binding, (GLint *)&tex);

			// very bespoke/specific
			GLint firstSlice = 0;
			gl.glGetTexParameteriv(target, eGL_TEXTURE_VIEW_MIN_LEVEL, &firstSlice);

			pipe.Textures[unit].Resource = rm->GetOriginalID(rm->GetID(TextureRes(ctx, tex)));
			pipe.Textures[unit].FirstSlice = (uint32_t)firstSlice;
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

	GLuint curFBO = 0;
	gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curFBO);
	
	GLint numCols = 8;
	gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

	GLuint curCol[32] = { 0 };
	GLuint curDepth = 0;
	GLuint curStencil = 0;

	RDCASSERT(numCols <= 32);

	// we should never bind the true default framebuffer - if the app did, we will have our fake bound
	RDCASSERT(curFBO != 0);

	{
		for(GLint i=0; i < numCols; i++)
			gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curCol[i]);
		gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curDepth);
		gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curStencil);
	}

	pipe.m_FB.FBO = rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curFBO)));
	create_array_uninit(pipe.m_FB.Color, numCols);
	for(GLint i=0; i < numCols; i++)
		pipe.m_FB.Color[i] = rm->GetOriginalID(rm->GetID(TextureRes(ctx, curCol[i])));

	pipe.m_FB.Depth = rm->GetOriginalID(rm->GetID(TextureRes(ctx, curDepth)));
	pipe.m_FB.Stencil = rm->GetOriginalID(rm->GetID(TextureRes(ctx, curStencil)));
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
				RDCUNIMPLEMENTED("Double uniform variables");
				break;
		}
	}

	if(!rowMajor)
	{
		uint32_t uv[16];
		memcpy(&uv[0], &outVar.value.uv[0], sizeof(uv));
		
		for(uint32_t r=0; r < outVar.rows; r++)
			for(uint32_t c=0; c < outVar.columns; c++)
				outVar.value.uv[r*outVar.columns+c] = uv[c*outVar.rows+r];
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
			if(desc.elements == 1)
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
					arrEl.name = StringFormat::Fmt("%hs[%u]", var.name.elems, a);
					
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
				RDCERR("Can't find program resource index for %hs", fullname.c_str());
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

				if(desc.elements == 1)
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
						el.name = StringFormat::Fmt("%hs[%u]", var.name.elems, a);

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

void GLReplay::FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
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

#pragma endregion

bool GLReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	RDCUNIMPLEMENTED("GetMinMax");
	return false;
}

bool GLReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
{
	RDCUNIMPLEMENTED("GetHistogram");
	return false;
}

void GLReplay::InitPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	GLNOTIMP("GLReplay::InitPostVSBuffers");
}

vector<EventUsage> GLReplay::GetUsage(ResourceId id)
{
	GLNOTIMP("GetUsage");
	return vector<EventUsage>();
}

void GLReplay::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	RDCUNIMPLEMENTED("SetContextFilter");
}

void GLReplay::FreeTargetResource(ResourceId id)
{
	RDCUNIMPLEMENTED("FreeTargetResource");
}

void GLReplay::FreeCustomShader(ResourceId id)
{
	RDCUNIMPLEMENTED("FreeCustomShader");
}

PostVSMeshData GLReplay::GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage)
{
	PostVSMeshData ret;
	RDCEraseEl(ret);

	GLNOTIMP("GLReplay::GetPostVSBuffers");

	return ret;
}

byte *GLReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize)
{
	RDCUNIMPLEMENTED("GetTextureData");
	return NULL;
}

void GLReplay::ReplaceResource(ResourceId from, ResourceId to)
{
	RDCUNIMPLEMENTED("ReplaceResource");
}

void GLReplay::RemoveReplacement(ResourceId id)
{
	RDCUNIMPLEMENTED("RemoveReplacement");
}

void GLReplay::TimeDrawcalls(rdctype::array<FetchDrawcall> &arr)
{
	RDCUNIMPLEMENTED("TimeDrawcalls");
}

void GLReplay::BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	RDCUNIMPLEMENTED("BuildTargetShader");
}

void GLReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	RDCUNIMPLEMENTED("BuildCustomShader");
}

vector<PixelModification> GLReplay::PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y)
{
	RDCUNIMPLEMENTED("GLReplay::PixelHistory");
	return vector<PixelModification>();
}

ShaderDebugTrace GLReplay::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	RDCUNIMPLEMENTED("DebugVertex");
	return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	RDCUNIMPLEMENTED("DebugPixel");
	return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	RDCUNIMPLEMENTED("DebugThread");
	return ShaderDebugTrace();
}

ResourceId GLReplay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
{
	RDCUNIMPLEMENTED("ApplyCustomShader");
	return ResourceId();
}

ResourceId GLReplay::CreateProxyTexture( FetchTexture templateTex )
{
	RDCUNIMPLEMENTED("CreateProxyTexture");
	return ResourceId();
}

void GLReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
{
	RDCUNIMPLEMENTED("SetProxyTextureData");
}

const GLHookSet &GetRealFunctions();

// defined in gl_replay_<platform>.cpp
ReplayCreateStatus GL_CreateReplayDevice(const wchar_t *logfile, IReplayDriver **driver);

static DriverRegistration GLDriverRegistration(RDC_OpenGL, L"OpenGL", &GL_CreateReplayDevice);
