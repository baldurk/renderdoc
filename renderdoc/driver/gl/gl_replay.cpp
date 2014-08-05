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
	delete m_pDriver;

	CloseReplayContext();
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

	if(res.curType == eGL_UNKNOWN_ENUM)
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

#pragma endregion

#pragma region Mostly Implemented

ShaderReflection *GLReplay::GetShader(ResourceId id)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(&m_ReplayCtx);
	
	// TODO this shouldn't be tied to the current program
	// This is only really needed to fill the latest sampler uniform values,
	// which could potentially be done instead in SavePipelineState?
	GLuint curProg = 0;
	gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint*)&curProg);
	
	void *ctx = m_ReplayCtx.ctx;
	
	auto &shaderDetails = m_pDriver->m_Shaders[id];

	auto &refl = shaderDetails.reflection;

	// initialise reflection data
	// TODO: do this earlier. In glLinkProgram?
	if(refl.DebugInfo.files.count == 0)
	{
		refl.DebugInfo.entryFunc = "main";
		refl.DebugInfo.compileFlags = 0;
		create_array_uninit(refl.DebugInfo.files, shaderDetails.sources.size());
		for(size_t i=0; i < shaderDetails.sources.size(); i++)
		{
			refl.DebugInfo.files[i].first = StringFormat::Fmt("source%u.glsl", (uint32_t)i);
			refl.DebugInfo.files[i].second = shaderDetails.sources[i];
		}

		refl.Disassembly = "";

		vector<ShaderResource> resources;

		GLint numUniforms = 0;
		gl.glGetProgramInterfaceiv(curProg, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &numUniforms);

		const size_t numProps = 8;

		GLenum resProps[numProps] = {
			eGL_REFERENCED_BY_VERTEX_SHADER,
			
			eGL_TYPE, eGL_NAME_LENGTH, eGL_LOCATION, eGL_BLOCK_INDEX, eGL_ARRAY_SIZE, eGL_OFFSET, eGL_IS_ROW_MAJOR,
		};

		if(shaderDetails.type == eGL_VERTEX_SHADER)          resProps[0] = eGL_REFERENCED_BY_VERTEX_SHADER;
		if(shaderDetails.type == eGL_TESS_CONTROL_SHADER)    resProps[0] = eGL_REFERENCED_BY_TESS_CONTROL_SHADER;
		if(shaderDetails.type == eGL_TESS_EVALUATION_SHADER) resProps[0] = eGL_REFERENCED_BY_TESS_EVALUATION_SHADER;
		if(shaderDetails.type == eGL_GEOMETRY_SHADER)        resProps[0] = eGL_REFERENCED_BY_GEOMETRY_SHADER;
		if(shaderDetails.type == eGL_FRAGMENT_SHADER)        resProps[0] = eGL_REFERENCED_BY_FRAGMENT_SHADER;
		if(shaderDetails.type == eGL_COMPUTE_SHADER)         resProps[0] = eGL_REFERENCED_BY_COMPUTE_SHADER;
		
		for(GLint u=0; u < numUniforms; u++)
		{
			GLint values[numProps];
			gl.glGetProgramResourceiv(curProg, eGL_UNIFORM, u, numProps, resProps, numProps, NULL, values);

			// skip if unused by this stage
			if(values[0] == GL_FALSE)
				continue;
			
			ShaderResource res;
			res.IsSampler = false; // no separate sampler objects in GL
			res.IsSRV = true;
			res.IsTexture = true;
			res.IsUAV = false;
			res.variableType.descriptor.rows = 1;
			res.variableType.descriptor.cols = 4;
			res.variableType.descriptor.elements = 1;

			// float samplers
			if(values[1] == GL_SAMPLER_BUFFER)
			{
				res.resType = eResType_Buffer;
				res.variableType.descriptor.name = "samplerBuffer";
			}
			else if(values[1] == GL_SAMPLER_1D)
			{
				res.resType = eResType_Texture1D;
				res.variableType.descriptor.name = "sampler1D";
			}
			else if(values[1] == GL_SAMPLER_1D_ARRAY)
			{
				res.resType = eResType_Texture1DArray;
				res.variableType.descriptor.name = "sampler1DArray";
			}
			else if(values[1] == GL_SAMPLER_1D_SHADOW)
			{
				res.resType = eResType_Texture1D;
				res.variableType.descriptor.name = "sampler1DShadow";
			}
			else if(values[1] == GL_SAMPLER_1D_ARRAY_SHADOW)
			{
				res.resType = eResType_Texture1DArray;
				res.variableType.descriptor.name = "sampler1DArrayShadow";
			}
			else if(values[1] == GL_SAMPLER_2D)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2D";
			}
			else if(values[1] == GL_SAMPLER_2D_ARRAY)
			{
				res.resType = eResType_Texture2DArray;
				res.variableType.descriptor.name = "sampler2DArray";
			}
			else if(values[1] == GL_SAMPLER_2D_SHADOW)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2DShadow";
			}
			else if(values[1] == GL_SAMPLER_2D_ARRAY_SHADOW)
			{
				res.resType = eResType_Texture2DArray;
				res.variableType.descriptor.name = "sampler2DArrayShadow";
			}
			else if(values[1] == GL_SAMPLER_2D_RECT)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2DRect";
			}
			else if(values[1] == GL_SAMPLER_2D_RECT_SHADOW)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2DRectShadow";
			}
			else if(values[1] == GL_SAMPLER_3D)
			{
				res.resType = eResType_Texture3D;
				res.variableType.descriptor.name = "sampler3D";
			}
			else if(values[1] == GL_SAMPLER_CUBE)
			{
				res.resType = eResType_TextureCube;
				res.variableType.descriptor.name = "samplerCube";
			}
			else if(values[1] == GL_SAMPLER_CUBE_SHADOW)
			{
				res.resType = eResType_TextureCube;
				res.variableType.descriptor.name = "samplerCubeShadow";
			}
			else if(values[1] == GL_SAMPLER_CUBE_MAP_ARRAY)
			{
				res.resType = eResType_TextureCubeArray;
				res.variableType.descriptor.name = "samplerCubeArray";
			}
			else if(values[1] == GL_SAMPLER_2D_MULTISAMPLE)
			{
				res.resType = eResType_Texture2DMS;
				res.variableType.descriptor.name = "sampler2DMS";
			}
			else if(values[1] == GL_SAMPLER_2D_MULTISAMPLE_ARRAY)
			{
				res.resType = eResType_Texture2DMSArray;
				res.variableType.descriptor.name = "sampler2DMSArray";
			}
			// int samplers
			else if(values[1] == GL_INT_SAMPLER_BUFFER)
			{
				res.resType = eResType_Buffer;
				res.variableType.descriptor.name = "samplerBuffer";
			}
			else if(values[1] == GL_INT_SAMPLER_1D)
			{
				res.resType = eResType_Texture1D;
				res.variableType.descriptor.name = "sampler1D";
			}
			else if(values[1] == GL_INT_SAMPLER_1D_ARRAY)
			{
				res.resType = eResType_Texture1DArray;
				res.variableType.descriptor.name = "sampler1DArray";
			}
			else if(values[1] == GL_INT_SAMPLER_2D)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2D";
			}
			else if(values[1] == GL_INT_SAMPLER_2D_ARRAY)
			{
				res.resType = eResType_Texture2DArray;
				res.variableType.descriptor.name = "sampler2DArray";
			}
			else if(values[1] == GL_INT_SAMPLER_2D_RECT)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2DRect";
			}
			else if(values[1] == GL_INT_SAMPLER_3D)
			{
				res.resType = eResType_Texture3D;
				res.variableType.descriptor.name = "sampler3D";
			}
			else if(values[1] == GL_INT_SAMPLER_CUBE)
			{
				res.resType = eResType_TextureCube;
				res.variableType.descriptor.name = "samplerCube";
			}
			else if(values[1] == GL_INT_SAMPLER_CUBE_MAP_ARRAY)
			{
				res.resType = eResType_TextureCubeArray;
				res.variableType.descriptor.name = "samplerCubeArray";
			}
			else if(values[1] == GL_INT_SAMPLER_2D_MULTISAMPLE)
			{
				res.resType = eResType_Texture2DMS;
				res.variableType.descriptor.name = "sampler2DMS";
			}
			else if(values[1] == GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
			{
				res.resType = eResType_Texture2DMSArray;
				res.variableType.descriptor.name = "sampler2DMSArray";
			}
			// unsigned int samplers
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_BUFFER)
			{
				res.resType = eResType_Buffer;
				res.variableType.descriptor.name = "samplerBuffer";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_1D)
			{
				res.resType = eResType_Texture1D;
				res.variableType.descriptor.name = "sampler1D";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_1D_ARRAY)
			{
				res.resType = eResType_Texture1DArray;
				res.variableType.descriptor.name = "sampler1DArray";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_2D)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2D";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_2D_ARRAY)
			{
				res.resType = eResType_Texture2DArray;
				res.variableType.descriptor.name = "sampler2DArray";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_2D_RECT)
			{
				res.resType = eResType_Texture2D;
				res.variableType.descriptor.name = "sampler2DRect";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_3D)
			{
				res.resType = eResType_Texture3D;
				res.variableType.descriptor.name = "sampler3D";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_CUBE)
			{
				res.resType = eResType_TextureCube;
				res.variableType.descriptor.name = "samplerCube";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY)
			{
				res.resType = eResType_TextureCubeArray;
				res.variableType.descriptor.name = "samplerCubeArray";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE)
			{
				res.resType = eResType_Texture2DMS;
				res.variableType.descriptor.name = "sampler2DMS";
			}
			else if(values[1] == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
			{
				res.resType = eResType_Texture2DMSArray;
				res.variableType.descriptor.name = "sampler2DMSArray";
			}
			else
			{
				// not a sampler
				continue;
			}

			res.variableAddress = values[3];

			create_array_uninit(res.name, values[2]+1);
			gl.glGetProgramResourceName(curProg, eGL_UNIFORM, u, values[2]+1, NULL, res.name.elems);
			res.name.count--; // trim off trailing null

			resources.push_back(res);
		}

		refl.Resources = resources;

		vector<ShaderConstant> globalUniforms;
		
		GLint numUBOs = 0;
		vector<string> uboNames;
		vector<bool> uboUsed;
		vector<ShaderConstant> *ubos = NULL;
		
		{
			gl.glGetProgramInterfaceiv(curProg, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

			ubos = new vector<ShaderConstant>[numUBOs];
			uboNames.resize(numUBOs);
			uboUsed.resize(numUBOs);

			for(GLint u=0; u < numUBOs; u++)
			{
				const size_t propnum = 2;
				GLenum UBOResProps[propnum] = {
					resProps[0], eGL_NAME_LENGTH,
				};
				GLint UBOValues[propnum];
				gl.glGetProgramResourceiv(curProg, eGL_UNIFORM_BLOCK, u, propnum, UBOResProps, propnum, NULL, UBOValues);

				// skip if unused by this stage
				if(UBOValues[0] == GL_FALSE)
					continue;

				char *nm = new char[UBOValues[1]+1];
				gl.glGetProgramResourceName(curProg, eGL_UNIFORM_BLOCK, u, UBOValues[1]+1, NULL, nm);
				uboNames[u] = nm;
				delete[] nm;

				uboUsed[u] = true;
			}
		}

		for(GLint u=0; u < numUniforms; u++)
		{
			GLint values[numProps];
			gl.glGetProgramResourceiv(curProg, eGL_UNIFORM, u, numProps, resProps, numProps, NULL, values);
			
			// skip if global and unused by this stage, or if a UBO that is unused by this stage
			if(values[0] == GL_FALSE && (values[4] == -1 || !uboUsed[ values[4] ]))
				continue;

			ShaderConstant var;
			
			if(values[1] == GL_FLOAT_VEC4)
			{
				var.type.descriptor.name = "vec4";
				var.type.descriptor.type = eVar_Float;
				var.type.descriptor.rows = 1;
				var.type.descriptor.cols = 4;
				var.type.descriptor.elements = RDCMAX(1, values[5]);
			}
			else if(values[1] == GL_FLOAT_VEC3)
			{
				var.type.descriptor.name = "vec3";
				var.type.descriptor.type = eVar_Float;
				var.type.descriptor.rows = 1;
				var.type.descriptor.cols = 3;
				var.type.descriptor.elements = RDCMAX(1, values[5]);
			}
			else if(values[1] == GL_FLOAT_MAT4)
			{
				var.type.descriptor.name = "mat4";
				var.type.descriptor.type = eVar_Float;
				var.type.descriptor.rows = 4;
				var.type.descriptor.cols = 4;
				var.type.descriptor.elements = RDCMAX(1, values[5]);
			}
			else if(values[1] == GL_UNSIGNED_INT_VEC4)
			{
				var.type.descriptor.name = "uvec4";
				var.type.descriptor.type = eVar_UInt;
				var.type.descriptor.rows = 1;
				var.type.descriptor.cols = 4;
				var.type.descriptor.elements = RDCMAX(1, values[5]);
			}
			else
			{
				// fill in more uniform types
				continue;
			}

			if(values[6] == -1 && values[3] >= 0)
			{
				var.reg.vec = values[3];
				var.reg.comp = 0;
			}
			else if(values[6] >= 0)
			{
				var.reg.vec = values[6] / 16;
				var.reg.comp = (values[6] / 4) % 4;

				RDCASSERT((values[6] % 4) == 0);
			}
			else
			{
				var.reg.vec = var.reg.comp = ~0U;
			}

			var.type.descriptor.rowMajorStorage = (values[7] >= 0);

			create_array_uninit(var.name, values[2]+1);
			gl.glGetProgramResourceName(curProg, eGL_UNIFORM, u, values[2]+1, NULL, var.name.elems);
			var.name.count--; // trim off trailing null

			int32_t c = var.name.count;

			// trim off trailing [0] if it's an array
			if(values[5] > 1 && var.name[c-3] == '[' && var.name[c-2] == '0' && var.name[c-1] == ']')
				var.name.count -= 3;

			if(strchr(var.name.elems, '.'))
			{
				GLNOTIMP("Variable contains . - structure not reconstructed");
			}
			
			vector<ShaderConstant> *UBO = &globalUniforms;

			// don't look at block uniforms just yet
			if(values[4] != -1)
			{
				RDCASSERT(values[4] < numUBOs);
				UBO = &ubos[ values[4] ];
			}
			
			UBO->push_back(var);
		}

		vector<ConstantBlock> cbuffers;
		
		if(ubos)
		{
			cbuffers.reserve(numUBOs + (globalUniforms.empty() ? 0 : 1));

			for(int i=0; i < numUBOs; i++)
			{
				if(!ubos[i].empty())
				{
					struct ubo_offset_sort
					{
						bool operator() (const ShaderConstant &a, const ShaderConstant &b)
						{ if(a.reg.vec == b.reg.vec) return a.reg.comp < b.reg.comp; else return a.reg.vec < b.reg.vec; }
					};

					ConstantBlock cblock;
					cblock.name = uboNames[i];
					cblock.bufferAddress = i;
					cblock.bindPoint = -1;
					std::sort(ubos[i].begin(), ubos[i].end(), ubo_offset_sort());

					cblock.variables = ubos[i];

					cbuffers.push_back(cblock);
				}
			}
		}

		if(!globalUniforms.empty())
		{
			ConstantBlock globals;
			globals.name = "$Globals";
			globals.bufferAddress = -1;
			globals.bindPoint = -1;
			globals.variables = globalUniforms;

			cbuffers.push_back(globals);
		}

		delete[] ubos;
		
		// TODO: fill in Interfaces with shader subroutines?
		// TODO: find a way of generating input/output signature.
		//       The only way I can think of doing this is to generate separable programs for each
		//       shader stage, but that requires modifying the glsl to redeclare built-in blocks if necessary.

		refl.ConstantBlocks = cbuffers;
	}

	// update with latest uniform values
	for(int32_t i=0; i < refl.Resources.count; i++)
	{
		if(refl.Resources.elems[i].IsSRV && refl.Resources.elems[i].IsTexture)
			gl.glGetUniformiv(curProg, refl.Resources.elems[i].variableAddress, (GLint *)&refl.Resources.elems[i].bindPoint);
	}
	for(int32_t i=0; i < refl.ConstantBlocks.count; i++)
	{
		if(refl.ConstantBlocks.elems[i].bufferAddress >= 0)
			gl.glGetActiveUniformBlockiv(curProg, refl.ConstantBlocks.elems[i].bufferAddress,
																		eGL_UNIFORM_BLOCK_BINDING, (GLint *)&refl.ConstantBlocks.elems[i].bindPoint);
	}

	return &refl;
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
		auto &progDetails = m_pDriver->m_Programs[rm->GetID(ProgramRes(ctx, curProg))];

		RDCASSERT(progDetails.shaders.size());

		for(size_t i=0; i < progDetails.shaders.size(); i++)
		{
			if(m_pDriver->m_Shaders[ progDetails.shaders[i] ].type == eGL_VERTEX_SHADER)
				pipe.m_VS.Shader = rm->GetOriginalID(progDetails.shaders[i]);
			else if(m_pDriver->m_Shaders[ progDetails.shaders[i] ].type == eGL_FRAGMENT_SHADER)
				pipe.m_FS.Shader = rm->GetOriginalID(progDetails.shaders[i]);
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


		// prefetch uniform values in GetShader()
		ShaderReflection *refls[6];
		for(size_t s=0; s < progDetails.shaders.size(); s++)
			refls[s] = GetShader(progDetails.shaders[s]);

		for(GLint unit=0; unit < numTexUnits; unit++)
		{
			GLenum binding = eGL_UNKNOWN_ENUM;
			GLenum target = eGL_UNKNOWN_ENUM;

			for(size_t s=0; s < progDetails.shaders.size(); s++)
			{
				if(refls[s] == NULL) continue;

				for(int32_t r=0; r < refls[s]->Resources.count; r++)
				{
					// bindPoint is the uniform value for this sampler
					if(refls[s]->Resources[r].bindPoint == unit)
					{
						GLenum t = eGL_UNKNOWN_ENUM;

						switch(refls[s]->Resources[r].resType)
						{
							case eResType_None:
								t = eGL_UNKNOWN_ENUM;
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

						if(binding == eGL_UNKNOWN_ENUM)
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

			if(binding != eGL_UNKNOWN_ENUM)
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

void GLReplay::FillCBufferValue(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, bool rowMajor, uint32_t locA, uint32_t locB,
											          const vector<byte> &data, ShaderVariable &outVar)
{
	const byte *bufdata = data.empty() ? NULL : &data[0];

	if(bufferBacked)
	{
		size_t rangelen = outVar.rows*outVar.columns*4;
		size_t offs = locA*4*sizeof(float) + (locB&0xf)*sizeof(float);

		if(offs < data.size())
		{
			rangelen = RDCMIN(rangelen, data.size()-offs);
			memcpy(&outVar.value.uv[0], bufdata + offs, rangelen);
		}
	}
	else
	{
		switch(outVar.type)
		{
		case eVar_Float:
			gl.glGetUniformfv(prog, locA, outVar.value.fv);
			break;
		case eVar_Int:
			gl.glGetUniformiv(prog, locA, outVar.value.iv);
			break;
		case eVar_UInt:
			gl.glGetUniformuiv(prog, locA, outVar.value.uv);
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

		for(uint32_t x=0; x < outVar.columns; x++)
			for(uint32_t y=0; y < outVar.rows; y++)
				outVar.value.uv[y + x*outVar.rows] = uv[x + y*outVar.columns];
	}
}

void GLReplay::FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(&m_ReplayCtx);

	GLuint curProg = 0;
	gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint*)&curProg);

	auto &shaderDetails = m_pDriver->m_Shaders[shader];

	if((int32_t)cbufSlot >= shaderDetails.reflection.ConstantBlocks.count)
	{
		RDCERR("Requesting invalid constant block");
		return;
	}

	auto cblock = shaderDetails.reflection.ConstantBlocks.elems[cbufSlot];

	for(int32_t i=0; i < cblock.variables.count; i++)
	{
		auto desc = cblock.variables[i].type.descriptor;

		ShaderVariable var;
		var.name = cblock.variables[i].name;
		var.rows = desc.rows;
		var.columns = desc.cols;
		var.type = desc.type;

		if(cblock.variables[i].type.members.count > 0)
			RDCUNIMPLEMENTED("Variables with members");

		RDCEraseEl(var.value);

		bool bufferBacked = (cblock.bindPoint >= 0 && cblock.bufferAddress >= 0 && !data.empty());
		bool hasValue = bufferBacked || (cblock.bindPoint < 0 && cblock.bufferAddress < 0); // buffer backed (with data), or global uniforms

		if(desc.elements == 1)
		{
			if(hasValue)
				FillCBufferValue(gl, curProg, bufferBacked, desc.rowMajorStorage ? true : false,
												 cblock.variables[i].reg.vec, cblock.variables[i].reg.comp, data, var);
		}
		else
		{
			vector<ShaderVariable> elems;
			for(uint32_t a=0; a < desc.elements; a++)
			{
				ShaderVariable el = var;
				el.name = StringFormat::Fmt("%s[%u]", var.name.elems, a);

				uint32_t arrayoffs = bufferBacked ? a : a*sizeof(float)*4;

				if(hasValue)
					FillCBufferValue(gl, curProg, bufferBacked, desc.rowMajorStorage ? true : false,
												   cblock.variables[i].reg.vec + arrayoffs, 0, data, el);

				elems.push_back(el);
			}

			var.members = elems;
		}

		outvars.push_back(var);
	}
}

#pragma endregion

bool GLReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, float *minval, float *maxval)
{
	RDCUNIMPLEMENTED("GetMinMax");
	return false;
}

bool GLReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
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

byte *GLReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, size_t &dataSize)
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

bool GLReplay::SaveTexture(ResourceId tex, uint32_t saveMip, wstring path)
{
	RDCUNIMPLEMENTED("SaveTexture");
	return false;
}

void GLReplay::BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	RDCUNIMPLEMENTED("BuildTargetShader");
}

void GLReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	RDCUNIMPLEMENTED("BuildCustomShader");
}

vector<PixelModification> GLReplay::PixelHistory(uint32_t frameID, vector<uint32_t> events, ResourceId target, uint32_t x, uint32_t y)
{
	RDCUNIMPLEMENTED("GLReplay::PixelHistory");
	return vector<PixelModification>();
}

ShaderDebugTrace GLReplay::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	RDCUNIMPLEMENTED("DebugVertex");
	return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y)
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
