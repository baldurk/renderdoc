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


#include "driver/gl/gl_manager.h"
#include "driver/gl/gl_driver.h"

struct VertexAttribInitialData
{
	uint32_t enabled;
	uint32_t vbslot;
	uint32_t offset;
	GLenum   type;
	int32_t  normalized;
	uint32_t integer;
	uint32_t size;
};

struct VertexBufferInitialData
{
	ResourceId Buffer;
	uint64_t Stride;
	uint64_t Offset;
	uint32_t Divisor;
};

struct VAOInitialData
{
	VertexAttribInitialData VertexAttribs[16];
	VertexBufferInitialData VertexBuffers[16];
	ResourceId ElementArrayBuffer;
};

struct FeedbackInitialData
{
	ResourceId Buffer[4];
	uint64_t Offset[4];
	uint64_t Size[4];
};

struct FramebufferInitialData
{
	GLenum DrawBuffers[8];
	GLenum ReadBuffer;
};

template<>
void Serialiser::Serialise(const char *name, VertexAttribInitialData &el)
{
	ScopedContext scope(this, this, name, "VertexArrayInitialData", 0, true);
	Serialise("enabled", el.enabled);
	Serialise("vbslot", el.vbslot);
	Serialise("offset", el.offset);
	Serialise("type", el.type);
	Serialise("normalized", el.normalized);
	Serialise("integer", el.integer);
	Serialise("size", el.size);
}

template<>
void Serialiser::Serialise(const char *name, VertexBufferInitialData &el)
{
	ScopedContext scope(this, this, name, "VertexBufferInitialData", 0, true);
	Serialise("Buffer", el.Buffer);
	Serialise("Stride", el.Stride);
	Serialise("Offset", el.Offset);
	Serialise("Divisor", el.Divisor);
}

template<>
void Serialiser::Serialise(const char *name, FeedbackInitialData &el)
{
	ScopedContext scope(this, this, name, "FeedbackInitialData", 0, true);
	Serialise<4>("Buffer", el.Buffer);
	Serialise<4>("Offset", el.Offset);
	Serialise<4>("Size", el.Size);
}

template<>
void Serialiser::Serialise(const char *name, FramebufferInitialData &el)
{
	ScopedContext scope(this, this, name, "FramebufferInitialData", 0, true);
	Serialise<8>("DrawBuffers", el.DrawBuffers);
	Serialise("ReadBuffer", el.ReadBuffer);
}

struct TextureStateInitialData
{
	int32_t baseLevel, maxLevel;
	float minLod, maxLod;
	GLenum srgbDecode;
	GLenum depthMode;
	GLenum compareFunc, compareMode;
	GLenum minFilter, magFilter;
	GLenum swizzle[4];
	GLenum wrap[3];
	float border[4];
	float lodBias;
};

template<>
void Serialiser::Serialise(const char *name, TextureStateInitialData &el)
{
	ScopedContext scope(this, this, name, "TextureStateInitialData", 0, true);
	Serialise("baseLevel", el.baseLevel);
	Serialise("maxLevel", el.maxLevel);
	Serialise("minLod", el.minLod);
	Serialise("maxLod", el.maxLod);
	Serialise("srgbDecode", el.srgbDecode);
	Serialise("depthMode", el.depthMode);
	Serialise("compareFunc", el.compareFunc);
	Serialise("compareMode", el.compareMode);
	Serialise("minFilter", el.minFilter);
	Serialise("magFilter", el.magFilter);
	Serialise<4>("swizzle", el.swizzle);
	Serialise<3>("wrap", el.wrap);
	Serialise<4>("border", el.border);
	Serialise("lodBias", el.lodBias);
}

bool GLResourceManager::SerialisableResource(ResourceId id, GLResourceRecord *record)
{
	if(id == m_GL->GetContextResourceID())
		return false;
	return true;
}

bool GLResourceManager::Need_InitialStateChunk(GLResource res)
{
	return res.Namespace != eResBuffer;
}

bool GLResourceManager::Prepare_InitialState(GLResource res)
{
	ResourceId Id = GetID(res);
	
	const GLHookSet &gl = m_GL->m_Real;

	if(res.Namespace == eResBuffer)
	{
		GLResourceRecord *record = GetResourceRecord(res);

		// TODO copy this to an immutable buffer elsewhere and SetInitialContents() it.
		// then only do the readback in Serialise_InitialState
		
		GLint length;
		gl.glGetNamedBufferParameterivEXT(res.name, eGL_BUFFER_SIZE, &length);
	
		gl.glGetNamedBufferSubDataEXT(res.name, 0, length, record->GetDataPtr());
	}
	else if(res.Namespace == eResProgram)
	{
		ScopedContext scope(m_pSerialiser, NULL, "Initial Contents", "Initial Contents", INITIAL_CONTENTS, false);

		m_pSerialiser->Serialise("Id", Id);

		SerialiseProgramUniforms(gl, m_pSerialiser, res.name, NULL, true);

		SetInitialChunk(Id, scope.Get());
	}
	else if(res.Namespace == eResTexture)
	{
		WrappedOpenGL::TextureData &details = m_GL->m_Textures[Id];
		GLenum binding = TextureBinding(details.curType);
		
		GLuint tex = 0;

		{
			GLuint oldtex = 0;
			gl.glGetIntegerv(binding, (GLint *)&oldtex);

			gl.glGenTextures(1, &tex);
			gl.glBindTexture(details.curType, tex);

			gl.glBindTexture(details.curType, oldtex);
		}

		int depth = details.depth;
		if(details.curType != eGL_TEXTURE_3D) depth = 1;

		GLint isComp = 0;

		GLenum queryType = details.curType;
		if(queryType == eGL_TEXTURE_CUBE_MAP)
			queryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;
		gl.glGetTextureLevelParameterivEXT(res.name, queryType, 0, eGL_TEXTURE_COMPRESSED, &isComp);
		
		int mips = GetNumMips(gl, details.curType, res.name, details.width, details.height, details.depth);

		// create texture of identical format/size to store initial contents
		if(details.curType == eGL_TEXTURE_2D_MULTISAMPLE)
		{
			gl.glTextureStorage2DMultisampleEXT(tex, details.curType, details.samples, details.internalFormat, details.width, details.height, GL_TRUE);
			mips = 1;
		}
		else if(details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
		{
			gl.glTextureStorage3DMultisampleEXT(tex, details.curType, details.samples, details.internalFormat, details.width, details.height, details.depth, GL_TRUE);
			mips = 1;
		}
		else if(details.dimension == 1)
		{
			gl.glTextureStorage1DEXT(tex, details.curType, mips, details.internalFormat, details.width);
		}
		else if(details.dimension == 2)
		{
			gl.glTextureStorage2DEXT(tex, details.curType, mips, details.internalFormat, details.width, details.height);
		}
		else if(details.dimension == 3)
		{
			gl.glTextureStorage3DEXT(tex, details.curType, mips, details.internalFormat, details.width, details.height, details.depth);
		}
		
		TextureStateInitialData *state = (TextureStateInitialData *)Serialiser::AllocAlignedBuffer(sizeof(TextureStateInitialData));
		RDCEraseMem(state, sizeof(TextureStateInitialData));
		
		{
			bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE || details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

			gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE, (GLint *)&state->depthMode);
			gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_BASE_LEVEL, (GLint *)&state->baseLevel);
			gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL, (GLint *)&state->maxLevel);
			gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_SWIZZLE_RGBA, (GLint *)&state->swizzle[0]);

			// only non-ms textures have sampler state
			if(!ms)
			{
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_SRGB_DECODE_EXT, (GLint *)&state->srgbDecode);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_COMPARE_FUNC, (GLint *)&state->compareFunc);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_COMPARE_MODE, (GLint *)&state->compareMode);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MIN_FILTER, (GLint *)&state->minFilter);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAG_FILTER, (GLint *)&state->magFilter);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_R, (GLint *)&state->wrap[0]);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_S, (GLint *)&state->wrap[1]);
				gl.glGetTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_WRAP_T, (GLint *)&state->wrap[2]);
				gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_MIN_LOD, &state->minLod);
				gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_MAX_LOD, &state->maxLod);
				gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_BORDER_COLOR, &state->border[0]);
				gl.glGetTextureParameterfvEXT(res.name, details.curType, eGL_TEXTURE_LOD_BIAS, &state->lodBias);
			}
		}
		
		// we need to set maxlevel appropriately for number of mips to force the texture to be complete.
		// This can happen if e.g. a texture is initialised just by default with glTexImage for level 0 and
		// used as a framebuffer attachment, then the implementation is fine with it. Unfortunately glCopyImageSubData
		// requires completeness across all mips, a stricter requirement :(.
		// We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then restore it to the 'real' value we fetched above
		int maxlevel = mips-1;
		gl.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

		// copy over mips
		for(int i=0; i < mips; i++)
		{
			int w = RDCMAX(details.width>>i, 1);
			int h = RDCMAX(details.height>>i, 1);
			int d = RDCMAX(details.depth>>i, 1);

			if(details.curType == eGL_TEXTURE_CUBE_MAP)
				d *= 6;

			// it seems like everything explodes if I do glCopyImageSubData on a D32F_S8 texture - in-program the overlay
			// gets corrupted as one UBO seems to not provide data anymore until it's "refreshed". It seems like a driver bug,
			// nvidia specific.
			// In most cases a program isn't going to rely on the contents of a depth-stencil buffer (shadow maps that it might
			// require would be depth-only formatted).
			if(details.internalFormat == eGL_DEPTH32F_STENCIL8 && VendorCheck(VendorCheck_NV_avoid_D32S8_copy))
				RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
			else
				gl.glCopyImageSubData(res.name, details.curType, i, 0, 0, 0, tex, details.curType, i, 0, 0, 0, w, h, d);
		}

		gl.glTextureParameterivEXT(res.name, details.curType, eGL_TEXTURE_MAX_LEVEL, (GLint *)&state->maxLevel);
		
		SetInitialContents(Id, InitialContentData(TextureRes(res.Context, tex), 0, (byte *)state));
	}
	else if(res.Namespace == eResFramebuffer)
	{
		// need to be on the right context, as feedback objects are never shared
		void *oldctx = m_GL->SwitchToContext(res.Context);
		
		GLuint prevread = 0, prevdraw = 0;
		gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
		gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);
		
		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, res.name);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, res.name);

		FramebufferInitialData *data = (FramebufferInitialData *)Serialiser::AllocAlignedBuffer(sizeof(FramebufferInitialData));
		RDCEraseMem(data, sizeof(FramebufferInitialData));

		for(int i=0; i < (int)ARRAY_COUNT(data->DrawBuffers); i++)
			gl.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&data->DrawBuffers[i]);

		gl.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&data->ReadBuffer);
		
		SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, (byte *)data));

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);

		// restore the previous context
		m_GL->SwitchToContext(oldctx);
	}
	else if(res.Namespace == eResFeedback)
	{
		// need to be on the right context, as feedback objects are never shared
		void *oldctx = m_GL->SwitchToContext(res.Context);

		GLuint prevfeedback = 0;
		gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK, (GLint *)&prevfeedback);

		gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, res.name);
		
		FeedbackInitialData *data = (FeedbackInitialData *)Serialiser::AllocAlignedBuffer(sizeof(FeedbackInitialData));
		RDCEraseMem(data, sizeof(FeedbackInitialData));

		GLint maxCount = 0;
		gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

		for(int i=0; i < (int)ARRAY_COUNT(data->Buffer) && i < maxCount; i++)
		{
			GLuint buffer = 0;
			gl.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint*)&buffer);data->Buffer[i] = GetID(BufferRes(res.Context, buffer));
			gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i, (GLint64*)&data->Offset[i]);
			gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE,  i, (GLint64*)&data->Size[i]);
		}
		
		SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, (byte *)data));

		gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);

		// restore the previous context
		m_GL->SwitchToContext(oldctx);
	}
	else if(res.Namespace == eResVertexArray)
	{
		// need to be on the right context, as VAOs are never shared
		void *oldctx = m_GL->SwitchToContext(res.Context);

		GLuint prevVAO = 0;
		gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

		gl.glBindVertexArray(res.name);

		VAOInitialData *data = (VAOInitialData *)Serialiser::AllocAlignedBuffer(sizeof(VAOInitialData));
		RDCEraseMem(data, sizeof(VAOInitialData));
		
		for(GLuint i=0; i < 16; i++)
		{
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED, (GLint *)&data->VertexAttribs[i].enabled);
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING, (GLint *)&data->VertexAttribs[i].vbslot);
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET, (GLint*)&data->VertexAttribs[i].offset);
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&data->VertexAttribs[i].type);
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint *)&data->VertexAttribs[i].normalized);
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER, (GLint *)&data->VertexAttribs[i].integer);
			gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&data->VertexAttribs[i].size);

			GLuint buffer = GetBoundVertexBuffer(gl, i);
			
			data->VertexBuffers[i].Buffer = GetID(BufferRes(res.Context, buffer));

			gl.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&data->VertexBuffers[i].Stride);
			gl.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&data->VertexBuffers[i].Offset);
			gl.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&data->VertexBuffers[i].Divisor);
		}

		GLuint buffer = 0;
		gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint*)&buffer);
		data->ElementArrayBuffer = GetID(BufferRes(res.Context, buffer));

		SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, (byte *)data));

		gl.glBindVertexArray(prevVAO);

		// restore the previous context
		m_GL->SwitchToContext(oldctx);
	}
	else
	{
		RDCERR("Unexpected type of resource requiring initial state");
	}

	return true;
}

bool GLResourceManager::Force_InitialState(GLResource res)
{
	return false;
}

bool GLResourceManager::Serialise_InitialState(GLResource res)
{
	ResourceId Id = ResourceId();

	if(m_State >= WRITING)
	{
		Id = GetID(res);

		if(res.Namespace != eResBuffer)
			m_pSerialiser->Serialise("Id", Id);
	}
	else
	{
		m_pSerialiser->Serialise("Id", Id);
	}
	
	if(m_State < WRITING)
	{
		if(HasLiveResource(Id))
			res = GetLiveResource(Id);
		else
			res = GLResource(MakeNullResource);
	}
	
	const GLHookSet &gl = m_GL->m_Real;
	
	if(res.Namespace == eResBuffer)
	{
		// Nothing to serialize
	}
	else if(res.Namespace == eResProgram)
	{
		// Prepare_InitialState sets the serialise chunk directly on write,
		// so we should never come in here except for when reading
		RDCASSERT(m_State < WRITING);
		
		WrappedOpenGL::ProgramData &details = m_GL->m_Programs[GetLiveID(Id)];
		
		GLuint initProg = gl.glCreateProgram();

		for(size_t i=0; i < details.shaders.size(); i++)
		{
			const auto &shadDetails = m_GL->m_Shaders[details.shaders[i]];

			GLuint shad = gl.glCreateShader(shadDetails.type);

			char **srcs = new char *[shadDetails.sources.size()];
			for(size_t s=0; s < shadDetails.sources.size(); s++)
				srcs[s] = (char *)shadDetails.sources[s].c_str();
			gl.glShaderSource(shad, (GLsizei)shadDetails.sources.size(), srcs, NULL);

			SAFE_DELETE_ARRAY(srcs);
			gl.glCompileShader(shad);
			gl.glAttachShader(initProg, shad);
			gl.glDeleteShader(shad);
		}

		gl.glLinkProgram(initProg);
		
		GLint status = 0;
		gl.glGetProgramiv(initProg, eGL_LINK_STATUS, &status);
		if(status == 0)
		{
			char buffer[1025] = {0};
			gl.glGetProgramInfoLog(initProg, 1024, NULL, buffer);
			RDCERR("Link error: %s", buffer);
		}

		SerialiseProgramUniforms(gl, m_pSerialiser, initProg, &details.locationTranslate, false);
		
		SetInitialContents(Id, InitialContentData(ProgramRes(m_GL->GetCtx(), initProg), 0, NULL));
	}
	else if(res.Namespace == eResTexture)
	{
		if(m_State >= WRITING)
		{
			WrappedOpenGL::TextureData &details = m_GL->m_Textures[Id];

			GLuint tex = GetInitialContents(Id).resource.name;

			GLuint ppb = 0;
			gl.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
			gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);

			GLint packParams[8];
			gl.glGetIntegerv(eGL_PACK_SWAP_BYTES, &packParams[0]);
			gl.glGetIntegerv(eGL_PACK_LSB_FIRST, &packParams[1]);
			gl.glGetIntegerv(eGL_PACK_ROW_LENGTH, &packParams[2]);
			gl.glGetIntegerv(eGL_PACK_IMAGE_HEIGHT, &packParams[3]);
			gl.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &packParams[4]);
			gl.glGetIntegerv(eGL_PACK_SKIP_ROWS, &packParams[5]);
			gl.glGetIntegerv(eGL_PACK_SKIP_IMAGES, &packParams[6]);
			gl.glGetIntegerv(eGL_PACK_ALIGNMENT, &packParams[7]);

			gl.glPixelStorei(eGL_PACK_SWAP_BYTES, 0);
			gl.glPixelStorei(eGL_PACK_LSB_FIRST, 0);
			gl.glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
			gl.glPixelStorei(eGL_PACK_IMAGE_HEIGHT, 0);
			gl.glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
			gl.glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
			gl.glPixelStorei(eGL_PACK_SKIP_IMAGES, 0);
			gl.glPixelStorei(eGL_PACK_ALIGNMENT, 1);
			
			GLint isComp = 0;
			
			GLenum queryType = details.curType;
			if(queryType == eGL_TEXTURE_CUBE_MAP)
				queryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

			gl.glGetTextureLevelParameterivEXT(res.name, queryType, 0, eGL_TEXTURE_COMPRESSED, &isComp);
			
			int imgmips = GetNumMips(gl, details.curType, tex, details.width, details.height, details.depth);
			
			TextureStateInitialData *state = (TextureStateInitialData *)GetInitialContents(Id).blob;

			SERIALISE_ELEMENT(TextureStateInitialData, stateData, *state);

			SERIALISE_ELEMENT(uint32_t, width, details.width);
			SERIALISE_ELEMENT(uint32_t, height, details.height);
			SERIALISE_ELEMENT(uint32_t, depth, details.depth);
			SERIALISE_ELEMENT(uint32_t, samples, details.samples);
			SERIALISE_ELEMENT(uint32_t, dim, details.dimension);
			SERIALISE_ELEMENT(GLenum, t, details.curType);
			SERIALISE_ELEMENT(GLenum, f, details.internalFormat);
			SERIALISE_ELEMENT(int, mips, imgmips);
			
			SERIALISE_ELEMENT(bool, isCompressed, isComp != 0);

			if(isCompressed)
			{
				for(int i=0; i < mips; i++)
				{
					int w = RDCMAX(details.width>>i, 1);
					int h = RDCMAX(details.height>>i, 1);
					int d = RDCMAX(details.depth>>i, 1);
					
					GLenum targets[] = {
						eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
					};

					int count = ARRAY_COUNT(targets);
						
					if(t != eGL_TEXTURE_CUBE_MAP)
					{
						targets[0] = details.curType;
						count = 1;
					}

					for(int trg=0; trg < count; trg++)
					{
						GLint compSize;
						gl.glGetTextureLevelParameterivEXT(tex, targets[trg], i, eGL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compSize);

						size_t size = compSize;

						// sometimes cubemaps return the compressed image size for the whole texture, but we read it
						// face by face
						if(VendorCheck(VendorCheck_EXT_compressed_cube_size) && t == eGL_TEXTURE_CUBE_MAP)
							size /= 6;

						byte *buf = new byte[size];

						gl.glGetCompressedTextureImageEXT(tex, targets[trg], i, buf);

						m_pSerialiser->SerialiseBuffer("image", buf, size);

						delete[] buf;
					}
				}
			}
			else if(samples > 1)
			{
				GLNOTIMP("Not implemented - initial states of multisampled textures");
			}
			else
			{
				GLenum fmt = GetBaseFormat(details.internalFormat);
				GLenum type = GetDataType(details.internalFormat);
					
				size_t size = GetByteSize(details.width, details.height, details.depth, fmt, type, 0);

				byte *buf = new byte[size];

				GLenum binding = TextureBinding(t);

				GLuint prevtex = 0;
				gl.glGetIntegerv(binding, (GLint *)&prevtex);

				gl.glBindTexture(t, tex);

				for(int i=0; i < mips; i++)
				{
					int w = RDCMAX(details.width>>i, 1);
					int h = RDCMAX(details.height>>i, 1);
					int d = RDCMAX(details.depth>>i, 1);
					
					size = GetByteSize(w, h, d, fmt, type, 0);
					
					GLenum targets[] = {
						eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
					};

					int count = ARRAY_COUNT(targets);
						
					if(t != eGL_TEXTURE_CUBE_MAP)
					{
						targets[0] = t;
						count = 1;
					}

					for(int trg=0; trg < count; trg++)
					{
						// we avoid glGetTextureImageEXT as it seems buggy for cubemap faces
						gl.glGetTexImage(targets[trg], i, fmt, type, buf);

						m_pSerialiser->SerialiseBuffer("image", buf, size);
					}
				}
				
				gl.glBindTexture(t, prevtex);

				SAFE_DELETE_ARRAY(buf);
			}

			gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);

			gl.glPixelStorei(eGL_PACK_SWAP_BYTES, packParams[0]);
			gl.glPixelStorei(eGL_PACK_LSB_FIRST, packParams[1]);
			gl.glPixelStorei(eGL_PACK_ROW_LENGTH, packParams[2]);
			gl.glPixelStorei(eGL_PACK_IMAGE_HEIGHT, packParams[3]);
			gl.glPixelStorei(eGL_PACK_SKIP_PIXELS, packParams[4]);
			gl.glPixelStorei(eGL_PACK_SKIP_ROWS, packParams[5]);
			gl.glPixelStorei(eGL_PACK_SKIP_IMAGES, packParams[6]);
			gl.glPixelStorei(eGL_PACK_ALIGNMENT, packParams[7]);
		}
		else
		{
			GLuint pub = 0;
			gl.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
			gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

			GLint unpackParams[8];
			gl.glGetIntegerv(eGL_UNPACK_SWAP_BYTES, &unpackParams[0]);
			gl.glGetIntegerv(eGL_UNPACK_LSB_FIRST, &unpackParams[1]);
			gl.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &unpackParams[2]);
			gl.glGetIntegerv(eGL_UNPACK_IMAGE_HEIGHT, &unpackParams[3]);
			gl.glGetIntegerv(eGL_UNPACK_SKIP_PIXELS, &unpackParams[4]);
			gl.glGetIntegerv(eGL_UNPACK_SKIP_ROWS, &unpackParams[5]);
			gl.glGetIntegerv(eGL_UNPACK_SKIP_IMAGES, &unpackParams[6]);
			gl.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &unpackParams[7]);

			gl.glPixelStorei(eGL_UNPACK_SWAP_BYTES, 0);
			gl.glPixelStorei(eGL_UNPACK_LSB_FIRST, 0);
			gl.glPixelStorei(eGL_UNPACK_ROW_LENGTH, 0);
			gl.glPixelStorei(eGL_UNPACK_IMAGE_HEIGHT, 0);
			gl.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, 0);
			gl.glPixelStorei(eGL_UNPACK_SKIP_ROWS, 0);
			gl.glPixelStorei(eGL_UNPACK_SKIP_IMAGES, 0);
			gl.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

			TextureStateInitialData *state = (TextureStateInitialData *)Serialiser::AllocAlignedBuffer(sizeof(TextureStateInitialData));
			RDCEraseMem(state, sizeof(TextureStateInitialData));

			m_pSerialiser->Serialise("state", *state);

			SERIALISE_ELEMENT(uint32_t, width, 0);
			SERIALISE_ELEMENT(uint32_t, height, 0);
			SERIALISE_ELEMENT(uint32_t, depth, 0);
			SERIALISE_ELEMENT(uint32_t, samples, 0);
			SERIALISE_ELEMENT(uint32_t, dim, 0);
			SERIALISE_ELEMENT(GLenum, textype, eGL_NONE);
			SERIALISE_ELEMENT(GLenum, internalformat, eGL_NONE);
			SERIALISE_ELEMENT(int, mips, 0);
			SERIALISE_ELEMENT(bool, isCompressed, false);
			
			GLuint tex = 0;
			
			{
				GLuint prevtex = 0;
				gl.glGetIntegerv(TextureBinding(textype), (GLint *)&prevtex);
				
				gl.glGenTextures(1, &tex);
				gl.glBindTexture(textype, tex);
				
				gl.glBindTexture(textype, prevtex);
			}

			// create texture of identical format/size to store initial contents
			if(textype == eGL_TEXTURE_2D_MULTISAMPLE)
			{
				gl.glTextureStorage2DMultisampleEXT(tex, textype, samples, internalformat, width, height, GL_TRUE);
				mips = 1;
			}
			else if(textype == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
			{
				gl.glTextureStorage3DMultisampleEXT(tex, textype, samples, internalformat, width, height, depth, GL_TRUE);
				mips = 1;
			}
			else if(dim == 1)
			{
				gl.glTextureStorage1DEXT(tex, textype, mips, internalformat, width);
			}
			else if(dim == 2)
			{
				gl.glTextureStorage2DEXT(tex, textype, mips, internalformat, width, height);
			}
			else if(dim == 3)
			{
				gl.glTextureStorage3DEXT(tex, textype, mips, internalformat, width, height, depth);
			}

			if(isCompressed)
			{
				for(int i=0; i < mips; i++)
				{
					uint32_t w = RDCMAX(width>>i, 1U);
					uint32_t h = RDCMAX(height>>i, 1U);
					uint32_t d = RDCMAX(depth>>i, 1U);
						
					GLenum targets[] = {
						eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
					};

					int count = ARRAY_COUNT(targets);
						
					if(textype != eGL_TEXTURE_CUBE_MAP)
					{
						targets[0] = textype;
						count = 1;
					}

					for(int trg=0; trg < count; trg++)
					{
						size_t size = 0;
						byte *buf = NULL;

						m_pSerialiser->SerialiseBuffer("image", buf, size);

						if(dim == 1)
							gl.glCompressedTextureSubImage1DEXT(tex, targets[trg], i, 0, w, internalformat, (GLsizei)size, buf);
						else if(dim == 2)
							gl.glCompressedTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, internalformat, (GLsizei)size, buf);
						else if(dim == 3)
							gl.glCompressedTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, internalformat, (GLsizei)size, buf);

						delete[] buf;
					}
				}
			}
			else if(samples > 1)
			{
				GLNOTIMP("Not implemented - initial states of multisampled textures");
			}
			else
			{
				GLenum fmt = GetBaseFormat(internalformat);
				GLenum type = GetDataType(internalformat);

				for(int i=0; i < mips; i++)
				{
					uint32_t w = RDCMAX(width>>i, 1U);
					uint32_t h = RDCMAX(height>>i, 1U);
					uint32_t d = RDCMAX(depth>>i, 1U);
					
					GLenum targets[] = {
						eGL_TEXTURE_CUBE_MAP_POSITIVE_X,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Y,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
						eGL_TEXTURE_CUBE_MAP_POSITIVE_Z,
						eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
					};

					int count = ARRAY_COUNT(targets);
						
					if(textype != eGL_TEXTURE_CUBE_MAP)
					{
						targets[0] = textype;
						count = 1;
					}
					
					for(int trg=0; trg < count; trg++)
					{
						size_t size = 0;
						byte *buf = NULL;
						m_pSerialiser->SerialiseBuffer("image", buf, size);

						if(dim == 1)
							gl.glTextureSubImage1DEXT(tex, targets[trg], i, 0, w, fmt, type, buf);
						else if(dim == 2)
							gl.glTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, fmt, type, buf);
						else if(dim == 3)
							gl.glTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, fmt, type, buf);

						delete[] buf;
					}
				}
			}
			
			SetInitialContents(Id, InitialContentData(TextureRes(m_GL->GetCtx(), tex), 0, (byte *)state));

			gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);

			gl.glPixelStorei(eGL_UNPACK_SWAP_BYTES, unpackParams[0]);
			gl.glPixelStorei(eGL_UNPACK_LSB_FIRST, unpackParams[1]);
			gl.glPixelStorei(eGL_UNPACK_ROW_LENGTH, unpackParams[2]);
			gl.glPixelStorei(eGL_UNPACK_IMAGE_HEIGHT, unpackParams[3]);
			gl.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, unpackParams[4]);
			gl.glPixelStorei(eGL_UNPACK_SKIP_ROWS, unpackParams[5]);
			gl.glPixelStorei(eGL_UNPACK_SKIP_IMAGES, unpackParams[6]);
			gl.glPixelStorei(eGL_UNPACK_ALIGNMENT, unpackParams[7]);
		}
	}
	else if(res.Namespace == eResFramebuffer)
	{
		FramebufferInitialData data;

		if(m_State >= WRITING)
		{
			FramebufferInitialData *initialdata = (FramebufferInitialData *)GetInitialContents(Id).blob;
			memcpy(&data, initialdata, sizeof(data));
		}

		m_pSerialiser->Serialise("Framebuffer object Buffers", data);
		
		if(m_State < WRITING)
		{
			byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
			memcpy(blob, &data, sizeof(data));

			SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
		}
	}
	else if(res.Namespace == eResFeedback)
	{
		FeedbackInitialData data;

		if(m_State >= WRITING)
		{
			FeedbackInitialData *initialdata = (FeedbackInitialData *)GetInitialContents(Id).blob;
			memcpy(&data, initialdata, sizeof(data));
		}

		m_pSerialiser->Serialise("Transform Feedback Buffers", data);
		
		if(m_State < WRITING)
		{
			byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
			memcpy(blob, &data, sizeof(data));

			SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
		}
	}
	else if(res.Namespace == eResVertexArray)
	{
		VAOInitialData data;

		if(m_State >= WRITING)
		{
			VAOInitialData *initialdata = (VAOInitialData *)GetInitialContents(Id).blob;
			memcpy(&data, initialdata, sizeof(data));
		}

		for(GLuint i=0; i < 16; i++)
		{
			m_pSerialiser->Serialise("VertexAttrib[]", data.VertexAttribs[i]);
			m_pSerialiser->Serialise("VertexBuffer[]", data.VertexBuffers[i]);
		}
		m_pSerialiser->Serialise("ElementArrayBuffer", data.ElementArrayBuffer);

		if(m_State < WRITING)
		{
			byte *blob = Serialiser::AllocAlignedBuffer(sizeof(data));
			memcpy(blob, &data, sizeof(data));

			SetInitialContents(Id, InitialContentData(GLResource(MakeNullResource), 0, blob));
		}
	}
	else
	{
		RDCERR("Unexpected type of resource requiring initial state");
	}

	return true;
}

void GLResourceManager::Create_InitialState(ResourceId id, GLResource live, bool hasData)
{
	if(live.Namespace == eResTexture)
	{
		GLNOTIMP("Need to set initial clear state for textures without an initial state");
	}
	else if(live.Namespace != eResBuffer)
	{
		RDCUNIMPLEMENTED("Expect all initial states to be created & not skipped, presently");
	}
}

void GLResourceManager::Apply_InitialState(GLResource live, InitialContentData initial)
{
	const GLHookSet &gl = m_GL->m_Real;
	
	if(live.Namespace == eResTexture)
	{
		ResourceId Id = GetID(live);
		WrappedOpenGL::TextureData &details = m_GL->m_Textures[Id];

		GLuint tex = initial.resource.name;
		
		int mips = GetNumMips(gl, details.curType, tex, details.width, details.height, details.depth);
		
		// we need to set maxlevel appropriately for number of mips to force the texture to be complete.
		// This can happen if e.g. a texture is initialised just by default with glTexImage for level 0 and
		// used as a framebuffer attachment, then the implementation is fine with it. Unfortunately glCopyImageSubData
		// requires completeness across all mips, a stricter requirement :(.
		// We set max_level to mips - 1 (so mips=1 means MAX_LEVEL=0). Then below where we set the texture state, the
		// correct MAX_LEVEL is set to whatever the program had.
		int maxlevel = mips-1;
		gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

		// copy over mips
		for(int i=0; i < mips; i++)
		{
			int w = RDCMAX(details.width>>i, 1);
			int h = RDCMAX(details.height>>i, 1);
			int d = RDCMAX(details.depth>>i, 1);
			
			if(details.curType == eGL_TEXTURE_CUBE_MAP)
				d *= 6;
			
			// it seems like everything explodes if I do glCopyImageSubData on a D32F_S8 texture - on replay loads of things
			// get heavily corrupted - probably the same as the problems we get in-program, but magnified. It seems like a driver bug,
			// nvidia specific.
			// In most cases a program isn't going to rely on the contents of a depth-stencil buffer (shadow maps that it might
			// require would be depth-only formatted).
			if(details.internalFormat == eGL_DEPTH32F_STENCIL8 && VendorCheck(VendorCheck_NV_avoid_D32S8_copy))
				RDCDEBUG("Not fetching initial contents of D32F_S8 texture");
			else
				gl.glCopyImageSubData(tex, details.curType, i, 0, 0, 0, live.name, details.curType, i, 0, 0, 0, w, h, d);
		}

		TextureStateInitialData *state = (TextureStateInitialData *)initial.blob;

		{
			bool ms = (details.curType == eGL_TEXTURE_2D_MULTISAMPLE || details.curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

			if(state->depthMode == eGL_DEPTH_COMPONENT || state->depthMode == eGL_STENCIL_INDEX)
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE, (GLint *)&state->depthMode);

			gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_BASE_LEVEL, (GLint *)&state->baseLevel);
			gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAX_LEVEL, (GLint *)&state->maxLevel);
			gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_SWIZZLE_RGBA, (GLint *)state->swizzle);

			if(!ms)
			{
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_SRGB_DECODE_EXT, (GLint *)&state->srgbDecode);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_COMPARE_FUNC, (GLint *)&state->compareFunc);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_COMPARE_MODE, (GLint *)&state->compareMode);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MIN_FILTER, (GLint *)&state->minFilter);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_MAG_FILTER, (GLint *)&state->magFilter);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_R, (GLint *)&state->wrap[0]);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_S, (GLint *)&state->wrap[1]);
				gl.glTextureParameterivEXT(live.name, details.curType, eGL_TEXTURE_WRAP_T, (GLint *)&state->wrap[2]);
				gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_MIN_LOD, &state->minLod);
				gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_MAX_LOD, &state->maxLod);
				gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_BORDER_COLOR, state->border);
				gl.glTextureParameterfvEXT(live.name, details.curType, eGL_TEXTURE_LOD_BIAS, &state->lodBias);
			}
		}
	}
	else if(live.Namespace == eResProgram)
	{
		CopyProgramUniforms(gl, initial.resource.name, live.name);
	}
	else if(live.Namespace == eResFramebuffer)
	{
		GLuint prevread = 0, prevdraw = 0;
		gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
		gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, live.name);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, live.name);

		FramebufferInitialData *data = (FramebufferInitialData *)initial.blob;

		for(int i=0; i < (int)ARRAY_COUNT(data->DrawBuffers); i++)
			gl.glDrawBuffers(ARRAY_COUNT(data->DrawBuffers), data->DrawBuffers);

		gl.glReadBuffer(data->ReadBuffer);

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);
	}
	else if(live.Namespace == eResFeedback)
	{
		GLuint prevfeedback = 0;
		gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK, (GLint *)&prevfeedback);

		gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, live.name);
		
		FeedbackInitialData *data = (FeedbackInitialData *)initial.blob;

		GLint maxCount = 0;
		gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

		for(int i=0; i < (int)ARRAY_COUNT(data->Buffer) && i < maxCount; i++)
		{
			GLuint buffer = data->Buffer[i] == ResourceId() ? 0 : GetLiveResource(data->Buffer[i]).name;
			gl.glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, i, buffer, (GLintptr)data->Offset[i], (GLsizei)data->Size[i]);
		}

		gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, prevfeedback);
	}
	else if(live.Namespace == eResVertexArray)
	{
		GLuint VAO = 0;
		gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);

		gl.glBindVertexArray(live.name);
	
		VAOInitialData *initialdata = (VAOInitialData *)initial.blob;	

		for(GLuint i=0; i < 16; i++)
		{
			VertexAttribInitialData &attrib = initialdata->VertexAttribs[i];

			if(attrib.enabled)
				gl.glEnableVertexAttribArray(i);
			else
				gl.glDisableVertexAttribArray(i);

			gl.glVertexAttribBinding(i, attrib.vbslot);

			if(initialdata->VertexAttribs[i].integer == 0)
				gl.glVertexAttribFormat(i, attrib.size, attrib.type, (GLboolean)attrib.normalized, attrib.offset);
			else
				gl.glVertexAttribIFormat(i, attrib.size, attrib.type, attrib.offset);

			VertexBufferInitialData &buf = initialdata->VertexBuffers[i];

			GLuint buffer = buf.Buffer == ResourceId() ? 0 : GetLiveResource(buf.Buffer).name;
			
			gl.glBindVertexBuffer(i, buffer, (GLintptr)buf.Offset, (GLsizei)buf.Stride);
			gl.glVertexBindingDivisor(i, buf.Divisor);
		}
		
		GLuint buffer = initialdata->ElementArrayBuffer == ResourceId() ? 0 : GetLiveResource(initialdata->ElementArrayBuffer).name;
		gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);

		gl.glBindVertexArray(VAO);
	}
	else
	{
		RDCERR("Unexpected type of resource requiring initial state");
	}
}
