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
#include "gl_driver.h"

#pragma region Textures

bool WrappedOpenGL::Serialise_glGenTextures(GLsizei n, GLuint* textures)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(*textures)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenTextures(1, &real);
		
		GLResource res = TextureRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);

		m_Textures[live].resource = res;
		m_Textures[live].curType = eGL_UNKNOWN_ENUM;
	}

	return true;
}

void WrappedOpenGL::glGenTextures(GLsizei n, GLuint* textures)
{
	m_Real.glGenTextures(n, textures);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = TextureRes(textures[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_TEXTURE);
				Serialise_glGenTextures(1, textures+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
			m_Textures[id].resource = res;
			m_Textures[id].curType = eGL_UNKNOWN_ENUM;
		}
	}
}

void WrappedOpenGL::glDeleteTextures(GLsizei n, const GLuint *textures)
{
	m_Real.glDeleteTextures(n, textures);

	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(TextureRes(textures[i]));
}

bool WrappedOpenGL::Serialise_glBindTexture(GLenum target, GLuint texture)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(TextureRes(texture)));
	
	if(m_State == WRITING_IDLE)
	{
		m_TextureRecord[m_TextureUnit]->datatype = Target;
	}
	else if(m_State < WRITING)
	{
		if(Id == ResourceId())
		{
			m_Real.glBindTexture(Target, 0);
		}
		else
		{
			GLResource res = GetResourceManager()->GetLiveResource(Id);
			m_Real.glBindTexture(Target, res.name);

			m_Textures[GetResourceManager()->GetLiveID(Id)].curType = Target;
		}
	}

	return true;
}

void WrappedOpenGL::glBindTexture(GLenum target, GLuint texture)
{
	m_Real.glBindTexture(target, texture);
	
	if(m_State == WRITING_CAPFRAME)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_TEXTURE);
			Serialise_glBindTexture(target, texture);

			chunk = scope.Get();
		}
		
		m_ContextRecord->AddChunk(chunk);
	}
	else if(m_State < WRITING)
	{
		m_Textures[GetResourceManager()->GetID(TextureRes(texture))].curType = target;
	}

	if(texture == 0)
	{
		m_TextureRecord[m_TextureUnit] = NULL;
		return;
	}

	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_TextureRecord[m_TextureUnit] = GetResourceManager()->GetResourceRecord(TextureRes(texture));

		if(r->datatype)
		{
			// it's illegal to retype a texture
			RDCASSERT(r->datatype == target);
		}
		else
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(BIND_TEXTURE);
				Serialise_glBindTexture(target, texture);

				chunk = scope.Get();
			}

			r->AddChunk(chunk);
		}
	}
}

bool WrappedOpenGL::Serialise_glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		m_Textures[liveId].width = Width;
		m_Textures[liveId].height = 1;
		m_Textures[liveId].depth = 1;

		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glTexStorage1D(Target, Levels, Format, Width);
	}

	return true;
}

void WrappedOpenGL::glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	m_Real.glTexStorage1D(target, levels, internalformat, width);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE1D);
		Serialise_glTexStorage1D(target, levels, internalformat, width);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		m_Textures[liveId].width = Width;
		m_Textures[liveId].height = Height;
		m_Textures[liveId].depth = 1;

		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glTexStorage2D(Target, Levels, Format, Width, Height);
	}

	return true;
}

void WrappedOpenGL::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	m_Real.glTexStorage2D(target, levels, internalformat, width, height);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE2D);
		Serialise_glTexStorage2D(target, levels, internalformat, width, height);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(uint32_t, Depth, depth);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		m_Textures[liveId].width = Width;
		m_Textures[liveId].height = Height;
		m_Textures[liveId].depth = Depth;

		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glTexStorage3D(Target, Levels, Format, Width, Height, Depth);
	}

	return true;
}

void WrappedOpenGL::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	m_Real.glTexStorage3D(target, levels, internalformat, width, height, depth);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE3D);
		Serialise_glTexStorage3D(target, levels, internalformat, width, height, depth);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	GLint align = 1;
	m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

	size_t subimageSize = GetByteSize(Width, 1, 1, Format, Type, Level, align);

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, subimageSize);
	
	if(m_State == READING)
	{
		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glTexSubImage1D(Target, Level, xoff, Width, Format, Type, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE1D);
		Serialise_glTexSubImage1D(target, level, xoffset, width, format, type, pixels);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(int32_t, yoff, yoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	GLint align = 1;
	m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

	size_t subimageSize = GetByteSize(Width, Height, 1, Format, Type, Level, align);

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, subimageSize);
	
	if(m_State == READING)
	{
		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glTexSubImage2D(Target, Level, xoff, yoff, Width, Height, Format, Type, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D);
		Serialise_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(int32_t, yoff, yoffset);
	SERIALISE_ELEMENT(int32_t, zoff, zoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(uint32_t, Depth, depth);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	GLint align = 1;
	m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

	size_t subimageSize = GetByteSize(Width, Height, Depth, Format, Type, Level, align);

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, subimageSize);
	
	if(m_State == READING)
	{
		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glTexSubImage3D(Target, Level, xoff, yoff, zoff, Width, Height, Depth, Format, Type, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D);
		Serialise_glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glGenerateMipmap(GLenum target)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());

	if(m_State == READING)
	{
		m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		m_Real.glGenerateMipmap(Target);
	}

	return true;
}

void WrappedOpenGL::glGenerateMipmap(GLenum target)
{
	m_Real.glGenerateMipmap(target);
	
	RDCASSERT(m_TextureRecord[m_TextureUnit]);
	{
		SCOPED_SERIALISE_CONTEXT(GENERATE_MIPMAP);
		Serialise_glGenerateMipmap(target);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());
	
	if(m_State < WRITING)
	{
		if(m_State == READING)
			m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		glTexParameteri(Target, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	m_Real.glTexParameteri(target, pname, param);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERI);
		Serialise_glTexParameteri(target, pname, param);

		if(m_State == WRITING_IDLE)
			m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		if(m_State == READING)
			m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		glTexParameteriv(Target, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	m_Real.glTexParameteriv(target, pname, params);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERIV);
		Serialise_glTexParameteriv(target, pname, params);

		if(m_State == WRITING_IDLE)
			m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(float, Param, param);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());
	
	if(m_State < WRITING)
	{
		if(m_State == READING)
			m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		glTexParameterf(Target, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	m_Real.glTexParameterf(target, pname, param);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERF);
		Serialise_glTexParameterf(target, pname, param);

		if(m_State == WRITING_IDLE)
			m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(ResourceId, id, m_TextureRecord[m_TextureUnit]->GetResourceID());
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(float, Params, params, nParams);

	if(m_State < WRITING)
	{
		if(m_State == READING)
			m_Real.glBindTexture(Target, GetResourceManager()->GetLiveResource(id).name);
		glTexParameterfv(Target, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	m_Real.glTexParameterfv(target, pname, params);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_TextureRecord[m_TextureUnit]);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERFV);
		Serialise_glTexParameterfv(target, pname, params);

		if(m_State == WRITING_IDLE)
			m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glGenSamplers(GLsizei n, GLuint* samplers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(*samplers)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenSamplers(1, &real);
		
		GLResource res = SamplerRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenSamplers(GLsizei count, GLuint *samplers)
{
	m_Real.glGenSamplers(count, samplers);

	for(GLsizei i=0; i < count; i++)
	{
		GLResource res = SamplerRes(samplers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_SAMPLERS);
				Serialise_glGenSamplers(1, samplers+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

bool WrappedOpenGL::Serialise_glBindSampler(GLuint unit, GLuint sampler)
{
	SERIALISE_ELEMENT(uint32_t, Unit, unit);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glBindSampler(Unit, res.name);
	}

	return true;
}

void WrappedOpenGL::glBindSampler(GLuint unit, GLuint sampler)
{
	m_Real.glBindSampler(unit, sampler);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_SAMPLER);
		Serialise_glBindSampler(unit, sampler);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameteri(res.name, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
	m_Real.glSamplerParameteri(sampler, pname, param);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERI);
		Serialise_glSamplerParameteri(sampler, pname, param);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(float, Param, param);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterf(res.name, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
	m_Real.glSamplerParameterf(sampler, pname, param);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERF);
		Serialise_glSamplerParameterf(sampler, pname, param);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameteriv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params)
{
	m_Real.glSamplerParameteriv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIV);
		Serialise_glSamplerParameteriv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(float, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterfv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params)
{
	m_Real.glSamplerParameterfv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERFV);
		Serialise_glSamplerParameterfv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterIiv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params)
{
	m_Real.glSamplerParameterIiv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIIV);
		Serialise_glSamplerParameterIiv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(SamplerRes(sampler)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(uint32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		glSamplerParameterIuiv(res.name, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params)
{
	m_Real.glSamplerParameterIuiv(sampler, pname, params);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(SAMPLER_PARAMETERIUIV);
		Serialise_glSamplerParameterIuiv(sampler, pname, params);

		if(m_State == WRITING_IDLE)
			GetResourceManager()->GetResourceRecord(SamplerRes(sampler))->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glPixelStorei(GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);

	if(m_State < WRITING)
		m_Real.glPixelStorei(PName, Param);

	return true;
}

void WrappedOpenGL::glPixelStorei(GLenum pname, GLint param)
{
	m_Real.glPixelStorei(pname, param);

	RDCASSERT(m_TextureRecord[m_TextureUnit]);
	{
		SCOPED_SERIALISE_CONTEXT(PIXELSTORE);
		Serialise_glPixelStorei(pname, param);

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glPixelStoref(GLenum pname, GLfloat param)
{
	glPixelStorei(pname, (GLint)param);
}

void WrappedOpenGL::glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	m_Real.glTexImage1D(target, level, internalformat, width, border, format, type, pixels);

	RDCUNIMPLEMENTED();
}

void WrappedOpenGL::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
	m_Real.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);

	RDCUNIMPLEMENTED();
}

void WrappedOpenGL::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
	m_Real.glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);

	RDCUNIMPLEMENTED();
}

bool WrappedOpenGL::Serialise_glActiveTexture(GLenum texture)
{
	SERIALISE_ELEMENT(GLenum, Texture, texture);

	if(m_State < WRITING)
		m_Real.glActiveTexture(Texture);

	return true;
}

void WrappedOpenGL::glActiveTexture(GLenum texture)
{
	m_Real.glActiveTexture(texture);

	m_TextureUnit = texture-eGL_TEXTURE0;
	
	if(m_State == WRITING_CAPFRAME)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(ACTIVE_TEXTURE);
			Serialise_glActiveTexture(texture);

			chunk = scope.Get();
		}
		
		m_ContextRecord->AddChunk(chunk);
	}
}

#pragma endregion

#pragma region Framebuffers

bool WrappedOpenGL::Serialise_glGenFramebuffers(GLsizei n, GLuint* framebuffers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(FramebufferRes(*framebuffers)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenFramebuffers(1, &real);
		
		GLResource res = FramebufferRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	m_Real.glGenFramebuffers(n, framebuffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = FramebufferRes(framebuffers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_FRAMEBUFFERS);
				Serialise_glGenFramebuffers(1, framebuffers+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

bool WrappedOpenGL::Serialise_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);

	ResourceId curFrameBuffer;

	if(m_State == WRITING_IDLE)
	{
		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(m_DrawFramebufferRecord)
				curFrameBuffer = m_DrawFramebufferRecord->GetResourceID();
		}
		else
		{
			if(m_ReadFramebufferRecord)
				curFrameBuffer = m_ReadFramebufferRecord->GetResourceID();
		}
	}

	SERIALISE_ELEMENT(ResourceId, fbid, curFrameBuffer);
	
	if(m_State < WRITING)
	{
		if(m_State == READING)
		{
			if(fbid != ResourceId())
			{
				GLResource res = GetResourceManager()->GetLiveResource(fbid);
				m_Real.glBindFramebuffer(Target, res.name);
			}
			else
			{
				m_Real.glBindFramebuffer(Target, 0);
			}
		}

		GLResource res = GetResourceManager()->GetLiveResource(id);
		glFramebufferTexture(Target, Attach, res.name, Level);
	}

	return true;
}

void WrappedOpenGL::glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture(target, attachment, texture, level);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
		Serialise_glFramebufferTexture(target, attachment, texture, level);

		if(m_State == WRITING_IDLE)
		{
			if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
			{
				if(m_DrawFramebufferRecord)
					m_DrawFramebufferRecord->AddChunk(scope.Get());
				else
					m_DeviceRecord->AddChunk(scope.Get());
			}
			else
			{
				if(m_ReadFramebufferRecord)
					m_ReadFramebufferRecord->AddChunk(scope.Get());
				else
					m_DeviceRecord->AddChunk(scope.Get());
			}
		}
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glReadBuffer(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, m, mode);
	SERIALISE_ELEMENT(ResourceId, id, m_ReadFramebufferRecord ? m_ReadFramebufferRecord->GetResourceID() : ResourceId());

	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, res.name);
		}
		else
		{
			m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, 0);
		}

		m_Real.glReadBuffer(m);
	}
	
	return true;
}

void WrappedOpenGL::glReadBuffer(GLenum mode)
{
	m_Real.glReadBuffer(mode);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(READ_BUFFER);
		Serialise_glReadBuffer(mode);

		if(m_State == WRITING_IDLE)
		{
			if(m_ReadFramebufferRecord)
				m_ReadFramebufferRecord->AddChunk(scope.Get());
			else
				m_DeviceRecord->AddChunk(scope.Get());
		}
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(FramebufferRes(framebuffer)));

	if(m_State <= EXECUTING)
	{
		if(Id == ResourceId())
		{
			m_Real.glBindFramebuffer(Target, m_FakeBB_FBO);
		}
		else
		{
			GLResource res = GetResourceManager()->GetLiveResource(Id);
			m_Real.glBindFramebuffer(Target, res.name);
		}
	}

	return true;
}

void WrappedOpenGL::glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_FRAMEBUFFER);
		Serialise_glBindFramebuffer(target, framebuffer);
		
		m_ContextRecord->AddChunk(scope.Get());
	}

	if(framebuffer == 0 && m_State < WRITING)
		framebuffer = m_FakeBB_FBO;

	if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		m_DrawFramebufferRecord = GetResourceManager()->GetResourceRecord(FramebufferRes(framebuffer));
	else
		m_ReadFramebufferRecord = GetResourceManager()->GetResourceRecord(FramebufferRes(framebuffer));

	m_Real.glBindFramebuffer(target, framebuffer);
}

bool WrappedOpenGL::Serialise_glDrawBuffer(GLenum buf)
{
	SERIALISE_ELEMENT(GLenum, b, buf);

	if(m_State < WRITING)
	{
		m_Real.glDrawBuffer(b);
	}

	return true;
}

void WrappedOpenGL::glDrawBuffer(GLenum buf)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFER);
		Serialise_glDrawBuffer(buf);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glDrawBuffer(buf);
}

bool WrappedOpenGL::Serialise_glDrawBuffers(GLsizei n, const GLenum *bufs)
{
	SERIALISE_ELEMENT(uint32_t, num, n);
	SERIALISE_ELEMENT_ARR(GLenum, buffers, bufs, num);

	if(m_State < WRITING)
	{
		m_Real.glDrawBuffers(num, buffers);
	}

	delete[] buffers;

	return true;
}

void WrappedOpenGL::glDrawBuffers(GLsizei n, const GLenum *bufs)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		Serialise_glDrawBuffers(n, bufs);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glDrawBuffers(n, bufs);
}

bool WrappedOpenGL::Serialise_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
	SERIALISE_ELEMENT(int32_t, sX0, srcX0);
	SERIALISE_ELEMENT(int32_t, sY0, srcY0);
	SERIALISE_ELEMENT(int32_t, sX1, srcX1);
	SERIALISE_ELEMENT(int32_t, sY1, srcY1);
	SERIALISE_ELEMENT(int32_t, dX0, dstX0);
	SERIALISE_ELEMENT(int32_t, dY0, dstY0);
	SERIALISE_ELEMENT(int32_t, dX1, dstX1);
	SERIALISE_ELEMENT(int32_t, dY1, dstY1);
	SERIALISE_ELEMENT(uint32_t, msk, mask);
	SERIALISE_ELEMENT(GLenum, flt, filter);
	
	if(m_State <= EXECUTING)
	{
		m_Real.glBlitFramebuffer(sX0, sY0, sX1, sY1, dX0, dY0, dX1, dY1, msk, flt);
	}

	return true;
}

void WrappedOpenGL::glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BLIT_FRAMEBUFFER);
		Serialise_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	
	m_Real.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void WrappedOpenGL::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	m_Real.glDeleteFramebuffers(n, framebuffers);

	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(FramebufferRes(framebuffers[i]));
}

void WrappedOpenGL::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
	m_Real.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

GLenum WrappedOpenGL::glCheckFramebufferStatus(GLenum target)
{
	return m_Real.glCheckFramebufferStatus(target);
}

#pragma endregion

#pragma region Shaders

bool WrappedOpenGL::Serialise_glCreateShader(GLuint shader, GLenum type)
{
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(shader)));

	if(m_State == READING)
	{
		GLuint real = m_Real.glCreateShader(Type);
		
		GLResource res = ShaderRes(real);

		ResourceId liveId = GetResourceManager()->RegisterResource(res);

		m_Shaders[liveId].type = Type;

		if(m_State >= WRITING)
		{
			RDCUNIMPLEMENTED();
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return true;
}

GLuint WrappedOpenGL::glCreateShader(GLenum type)
{
	GLuint real = m_Real.glCreateShader(type);

	GLResource res = ShaderRes(real);
	ResourceId id = GetResourceManager()->RegisterResource(res);

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(CREATE_SHADER);
			Serialise_glCreateShader(real, type);

			chunk = scope.Get();
		}

		GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
		RDCASSERT(record);

		record->AddChunk(chunk);
	}
	else
	{
		GetResourceManager()->AddLiveResource(id, res);
	}

	return real;
}

bool WrappedOpenGL::Serialise_glShaderSource(GLuint shader, GLsizei count, const GLchar* const *source, const GLint *length)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(shader)));
	SERIALISE_ELEMENT(uint32_t, Count, count);

	vector<string> srcs;

	for(uint32_t i=0; i < Count; i++)
	{
		string s;
		if(source)
			s = length ? string(source[i], source[i] + length[i]) : string(source[i]);
		
		m_pSerialiser->SerialiseString("source", s);

		if(m_State == READING)
			srcs.push_back(s);
	}
	
	if(m_State == READING)
	{
		const char **strings = new const char*[srcs.size()];
		for(size_t i=0; i < srcs.size(); i++)
			strings[i] = srcs[i].c_str();

		ResourceId liveId = GetResourceManager()->GetLiveID(id);

		for(uint32_t i=0; i < Count; i++)
			m_Shaders[liveId].sources.push_back(strings[i]);

		m_Real.glShaderSource(GetResourceManager()->GetLiveResource(id).name, Count, strings, NULL);
	}

	return true;
}

void WrappedOpenGL::glShaderSource(GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length)
{
	m_Real.glShaderSource(shader, count, string, length);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(shader));
		RDCASSERT(record);
		{
			SCOPED_SERIALISE_CONTEXT(SHADERSOURCE);
			Serialise_glShaderSource(shader, count, string, length);

			record->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glCompileShader(GLuint shader)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ShaderRes(shader)));
	
	if(m_State == READING)
	{
		m_Real.glCompileShader(GetResourceManager()->GetLiveResource(id).name);
	}

	return true;
}

void WrappedOpenGL::glCompileShader(GLuint shader)
{
	m_Real.glCompileShader(shader);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ShaderRes(shader));
		RDCASSERT(record);
		{
			SCOPED_SERIALISE_CONTEXT(COMPILESHADER);
			Serialise_glCompileShader(shader);

			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glReleaseShaderCompiler()
{
	m_Real.glReleaseShaderCompiler();
}

void WrappedOpenGL::glDeleteShader(GLuint shader)
{
	m_Real.glDeleteShader(shader);
	
	GetResourceManager()->UnregisterResource(ShaderRes(shader));
}

bool WrappedOpenGL::Serialise_glAttachShader(GLuint program, GLuint shader)
{
	SERIALISE_ELEMENT(ResourceId, progid, GetResourceManager()->GetID(ProgramRes(program)));
	SERIALISE_ELEMENT(ResourceId, shadid, GetResourceManager()->GetID(ShaderRes(shader)));

	if(m_State == READING)
	{
		ResourceId liveProgId = GetResourceManager()->GetLiveID(progid);
		ResourceId liveShadId = GetResourceManager()->GetLiveID(shadid);

		m_Programs[liveProgId].shaders.push_back(liveShadId);
		
		m_Real.glAttachShader(GetResourceManager()->GetLiveResource(progid).name,
								GetResourceManager()->GetLiveResource(shadid).name);
	}

	return true;
}

void WrappedOpenGL::glAttachShader(GLuint program, GLuint shader)
{
	m_Real.glAttachShader(program, shader);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *progRecord = GetResourceManager()->GetResourceRecord(ProgramRes(program));
		GLResourceRecord *shadRecord = GetResourceManager()->GetResourceRecord(ShaderRes(shader));
		RDCASSERT(progRecord && shadRecord);
		{
			SCOPED_SERIALISE_CONTEXT(ATTACHSHADER);
			Serialise_glAttachShader(program, shader);

			progRecord->AddParent(shadRecord);
			progRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glDetachShader(GLuint program, GLuint shader)
{
	SERIALISE_ELEMENT(ResourceId, progid, GetResourceManager()->GetID(ProgramRes(program)));
	SERIALISE_ELEMENT(ResourceId, shadid, GetResourceManager()->GetID(ShaderRes(shader)));

	if(m_State == READING)
	{
		ResourceId liveProgId = GetResourceManager()->GetLiveID(progid);
		ResourceId liveShadId = GetResourceManager()->GetLiveID(shadid);

		if(!m_Programs[liveProgId].linked)
			m_Programs[liveProgId].shaders.push_back(liveShadId);
		
		m_Real.glDetachShader(GetResourceManager()->GetLiveResource(progid).name,
								GetResourceManager()->GetLiveResource(shadid).name);
	}

	return true;
}

void WrappedOpenGL::glDetachShader(GLuint program, GLuint shader)
{
	m_Real.glDetachShader(program, shader);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *progRecord = GetResourceManager()->GetResourceRecord(ProgramRes(program));
		RDCASSERT(progRecord);
		{
			SCOPED_SERIALISE_CONTEXT(DETACHSHADER);
			Serialise_glDetachShader(program, shader);

			progRecord->AddChunk(scope.Get());
		}
	}
}

#pragma endregion

#pragma region Programs

bool WrappedOpenGL::Serialise_glCreateShaderProgramv(GLuint program, GLenum type, GLsizei count, const GLchar *const*strings)
{
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(int32_t, Count, count);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));

	vector<string> src;

	for(int32_t i=0; i < Count; i++)
	{
		string s;
		if(m_State >= WRITING) s = strings[i];
		m_pSerialiser->SerialiseString("Source", s);
		if(m_State < WRITING) src.push_back(s);
	}

	if(m_State == READING)
	{
		char **sources = new char*[Count];
		
		for(int32_t i=0; i < Count; i++)
			sources[i] = &src[i][0];

		GLuint real = m_Real.glCreateShaderProgramv(Type, Count, sources);

		delete[] sources;
		
		GLResource res = ProgramRes(program);

		m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

GLuint WrappedOpenGL::glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const*strings)
{
	GLuint real = m_Real.glCreateShaderProgramv(type, count, strings);
	
	GLResource res = ProgramRes(real);
	ResourceId id = GetResourceManager()->RegisterResource(res);
		
	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(CREATE_SHADERPROGRAM);
			Serialise_glCreateShaderProgramv(real, type, count, strings);

			chunk = scope.Get();
		}

		GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
		RDCASSERT(record);

		record->AddChunk(chunk);
	}
	else
	{
		GetResourceManager()->AddLiveResource(id, res);
	}

	return real;
}

bool WrappedOpenGL::Serialise_glCreateProgram(GLuint program)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));

	if(m_State == READING)
	{
		GLuint real = m_Real.glCreateProgram();
		
		GLResource res = ProgramRes(real);

		m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

GLuint WrappedOpenGL::glCreateProgram()
{
	GLuint real = m_Real.glCreateProgram();
	
	GLResource res = ProgramRes(real);
	ResourceId id = GetResourceManager()->RegisterResource(res);
		
	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(CREATE_PROGRAM);
			Serialise_glCreateProgram(real);

			chunk = scope.Get();
		}

		GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
		RDCASSERT(record);

		record->AddChunk(chunk);
	}
	else
	{
		GetResourceManager()->AddLiveResource(id, res);
	}

	return real;
}

bool WrappedOpenGL::Serialise_glLinkProgram(GLuint program)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));

	if(m_State == READING)
	{
		ResourceId progid = GetResourceManager()->GetLiveID(id);

		m_Programs[progid].linked = true;
		
		m_Real.glLinkProgram(GetResourceManager()->GetLiveResource(id).name);
	}

	return true;
}

void WrappedOpenGL::glLinkProgram(GLuint program)
{
	m_Real.glLinkProgram(program);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
		RDCASSERT(record);
		{
			SCOPED_SERIALISE_CONTEXT(LINKPROGRAM);
			Serialise_glLinkProgram(program);

			record->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Value, value);

	if(m_State == READING)
	{
		m_Real.glProgramParameteri(GetResourceManager()->GetLiveResource(id).name, PName, Value);
	}

	return true;
}

void WrappedOpenGL::glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
	m_Real.glProgramParameteri(program, pname, value);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
		RDCASSERT(record);
		{
			SCOPED_SERIALISE_CONTEXT(PROGRAMPARAMETER);
			Serialise_glProgramParameteri(program, pname, value);

			record->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glDeleteProgram(GLuint program)
{
	m_Real.glDeleteProgram(program);
	
	GetResourceManager()->UnregisterResource(ProgramRes(program));
}

bool WrappedOpenGL::Serialise_glUseProgram(GLuint program)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));

	if(m_State <= EXECUTING)
	{
		m_Real.glUseProgram(GetResourceManager()->GetLiveResource(id).name);
	}

	return true;
}

void WrappedOpenGL::glUseProgram(GLuint program)
{
	m_Real.glUseProgram(program);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(USEPROGRAM);
		Serialise_glUseProgram(program);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glValidateProgram(GLuint program)
{
	m_Real.glValidateProgram(program);
}

void WrappedOpenGL::glValidateProgramPipeline(GLuint pipeline)
{
	m_Real.glValidateProgramPipeline(pipeline);
}

#pragma endregion

#pragma region Program Pipelines

bool WrappedOpenGL::Serialise_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
	SERIALISE_ELEMENT(ResourceId, pipe, GetResourceManager()->GetID(ProgramPipeRes(pipeline)));
	SERIALISE_ELEMENT(uint32_t, Stages, stages);
	SERIALISE_ELEMENT(ResourceId, prog, GetResourceManager()->GetID(ProgramRes(program)));

	if(m_State < WRITING)
	{
		m_Real.glUseProgramStages(GetResourceManager()->GetLiveResource(pipe).name,
															stages,
															GetResourceManager()->GetLiveResource(prog).name);
	}

	return true;
}

void WrappedOpenGL::glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
	m_Real.glUseProgramStages(pipeline, stages, program);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(USE_PROGRAMSTAGES);
		Serialise_glUseProgramStages(pipeline, stages, program);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramPipeRes(pipeline));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
			
			GLResourceRecord *progrecord = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			record->AddParent(progrecord);
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

bool WrappedOpenGL::Serialise_glGenProgramPipelines(GLsizei n, GLuint* pipelines)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramPipeRes(*pipelines)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenProgramPipelines(1, &real);
		
		GLResource res = ProgramPipeRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
	m_Real.glGenProgramPipelines(n, pipelines);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = ProgramPipeRes(pipelines[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_PROGRAMPIPE);
				Serialise_glGenProgramPipelines(1, pipelines+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

bool WrappedOpenGL::Serialise_glBindProgramPipeline(GLuint pipeline)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramPipeRes(pipeline)));

	if(m_State <= EXECUTING)
	{
		m_Real.glBindProgramPipeline(GetResourceManager()->GetLiveResource(id).name);
	}

	return true;
}

void WrappedOpenGL::glBindProgramPipeline(GLuint pipeline)
{
	m_Real.glBindProgramPipeline(pipeline);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_PROGRAMPIPE);
		Serialise_glBindProgramPipeline(pipeline);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
	m_Real.glDeleteProgramPipelines(n, pipelines);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(ProgramPipeRes(pipelines[i]));
}

#pragma endregion

#pragma region Uniforms

bool WrappedOpenGL::Serialise_glUniformMatrix(GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(int32_t, Loc, location);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(uint8_t, Transpose, transpose);

	size_t elemsPerMat = 0;

	switch(Type)
	{
		case MAT4FV: elemsPerMat = 16; break;
		default:
			RDCERR("Unexpected uniform type to Serialise_glUniformMatrix: %d", Type);
	}

	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, sizeof(float)*elemsPerMat*Count);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(sizeof(float)*elemsPerMat*Count);

		switch(Type)
		{
			case MAT4FV: m_Real.glUniformMatrix4fv(Loc, Count, Transpose, (const GLfloat *)value); break;
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformMatrix: %d", Type);
		}
	}

	if(m_pSerialiser->GetDebugText())
	{
		switch(Type)
		{
			case MAT4FV:
			{
				float *f = (float *)value;
				if(Transpose)
				{
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0], f[4], f[8],  f[12]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[1], f[5], f[9],  f[13]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[2], f[6], f[10], f[14]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[3], f[7], f[11], f[15]);
				}
				else
				{
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0],  f[1],  f[2],  f[3]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[4],  f[5],  f[6],  f[7]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[8],  f[9],  f[10], f[11]);
					m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[12], f[13], f[14], f[15]);
				}
				break;
			}
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
		}
	}
	
	return true;
}

void WrappedOpenGL::glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	m_Real.glUniformMatrix4fv(location, count, transpose, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_MATRIX);
		Serialise_glUniformMatrix(location, count, transpose, value, MAT4FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glUniformVector(GLint location, GLsizei count, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(int32_t, Loc, location);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	
	size_t elemsPerVec = 0;

	switch(Type)
	{
		case VEC1IV:
		case VEC1UIV:
		case VEC1FV: elemsPerVec = 1; break;
		case VEC2FV: elemsPerVec = 2; break;
		case VEC3FV: elemsPerVec = 3; break;
		case VEC4FV: elemsPerVec = 4; break;
		default:
			RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
	}

	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, sizeof(float)*elemsPerVec*Count);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(sizeof(float)*elemsPerVec*Count);

		switch(Type)
		{
			case VEC1FV: m_Real.glUniform1fv(Loc, Count, (const GLfloat *)value); break;
			case VEC1IV: m_Real.glUniform1iv(Loc, Count, (const GLint *)value); break;
			case VEC1UIV: m_Real.glUniform1uiv(Loc, Count, (const GLuint *)value); break;
			case VEC2FV: m_Real.glUniform2fv(Loc, Count, (const GLfloat *)value); break;
			case VEC3FV: m_Real.glUniform3fv(Loc, Count, (const GLfloat *)value); break;
			case VEC4FV: m_Real.glUniform4fv(Loc, Count, (const GLfloat *)value); break;
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
		}
	}

	if(m_pSerialiser->GetDebugText())
	{
		switch(Type)
		{
			case VEC1FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f}\n", f[0]);
				break;
			}
			case VEC1IV:
			{
				int32_t *i = (int32_t *)value;
				m_pSerialiser->DebugPrint("value: {%d}\n", i[0]);
				break;
			}
			case VEC1UIV:
			{
				uint32_t *u = (uint32_t *)value;
				m_pSerialiser->DebugPrint("value: {%u}\n", u[0]);
				break;
			}
			case VEC2FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f}\n", f[0], f[1]);
				break;
			}
			case VEC3FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f}\n", f[0], f[1], f[2]);
				break;
			}
			case VEC4FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0], f[1], f[2], f[3]);
				break;
			}
			default:
				RDCERR("Unexpected uniform type to Serialise_glUniformVector: %d", Type);
		}
	}

	return true;
}

void WrappedOpenGL::glUniform1f(GLint location, GLfloat value)
{
	m_Real.glUniform1f(location, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, 1, &value, VEC1FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1i(GLint location, GLint value)
{
	m_Real.glUniform1i(location, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, 1, &value, VEC1IV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1ui(GLint location, GLuint value)
{
	m_Real.glUniform1ui(location, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, 1, &value, VEC1UIV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform1fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC1FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
	m_Real.glUniform1iv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC1IV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
	m_Real.glUniform1uiv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC1UIV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform2fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC2FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform3fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC3FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glUniform4fv(location, count, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(UNIFORM_VECTOR);
		Serialise_glUniformVector(location, count, value, VEC4FV);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glProgramUniformVector(GLuint program, GLint location, GLsizei count, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(program)));
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(int32_t, Loc, location);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	
	size_t elemsPerVec = 0;

	switch(Type)
	{
		case VEC1IV:
		case VEC1UIV:
		case VEC1FV: elemsPerVec = 1; break;
		case VEC2FV: elemsPerVec = 2; break;
		case VEC3FV: elemsPerVec = 3; break;
		case VEC4FV: elemsPerVec = 4; break;
		default:
			RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
	}

	if(m_State >= WRITING)
	{
		m_pSerialiser->RawWriteBytes(value, sizeof(float)*elemsPerVec*Count);
	}
	else if(m_State <= EXECUTING)
	{
		value = m_pSerialiser->RawReadBytes(sizeof(float)*elemsPerVec*Count);

		GLuint live = GetResourceManager()->GetLiveResource(id).name;

		switch(Type)
		{
			case VEC1IV: m_Real.glProgramUniform1iv(live, Loc, Count, (const GLint *)value); break;
			case VEC1UIV: m_Real.glProgramUniform1uiv(live, Loc, Count, (const GLuint *)value); break;
			case VEC1FV: m_Real.glProgramUniform1fv(live, Loc, Count, (const GLfloat *)value); break;
			case VEC2FV: m_Real.glProgramUniform2fv(live, Loc, Count, (const GLfloat *)value); break;
			case VEC3FV: m_Real.glProgramUniform3fv(live, Loc, Count, (const GLfloat *)value); break;
			case VEC4FV: m_Real.glProgramUniform4fv(live, Loc, Count, (const GLfloat *)value); break;
			default:
				RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
		}
	}

	if(m_pSerialiser->GetDebugText())
	{
		switch(Type)
		{
			case VEC1FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f}\n", f[0]);
				break;
			}
			case VEC1IV:
			{
				int32_t *i = (int32_t *)value;
				m_pSerialiser->DebugPrint("value: {%d}\n", i[0]);
				break;
			}
			case VEC1UIV:
			{
				uint32_t *u = (uint32_t *)value;
				m_pSerialiser->DebugPrint("value: {%u}\n", u[0]);
				break;
			}
			case VEC2FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f}\n", f[0], f[1]);
				break;
			}
			case VEC3FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f}\n", f[0], f[1], f[2]);
				break;
			}
			case VEC4FV:
			{
				float *f = (float *)value;
				m_pSerialiser->DebugPrint("value: {%f %f %f %f}\n", f[0], f[1], f[2], f[3]);
				break;
			}
			default:
				RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
		}
	}

	return true;
}

void WrappedOpenGL::glProgramUniform1i(GLuint program, GLint location, GLint v0)
{
	m_Real.glProgramUniform1i(program, location, v0);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, 1, &v0, VEC1IV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
	m_Real.glProgramUniform1iv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC1IV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform1fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC1FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
	m_Real.glProgramUniform1uiv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC1UIV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform2fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC2FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform3fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC3FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

void WrappedOpenGL::glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
	m_Real.glProgramUniform4fv(program, location, count, value);

	if(m_State > WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);
		Serialise_glProgramUniformVector(program, location, count, value, VEC4FV);
		
		if(m_State == WRITING_CAPFRAME)
		{
			GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ProgramRes(program));
			RDCASSERT(record);
			record->AddChunk(scope.Get());
		}
		else
		{
			m_ContextRecord->AddChunk(scope.Get());
		}
	}
}

#pragma endregion

#pragma region Buffers

bool WrappedOpenGL::Serialise_glGenBuffers(GLsizei n, GLuint* textures)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(BufferRes(*textures)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenBuffers(1, &real);
		
		GLResource res = BufferRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);

		m_Buffers[live].resource = res;
		m_Buffers[live].curType = eGL_UNKNOWN_ENUM;
	}

	return true;
}

void WrappedOpenGL::glGenBuffers(GLsizei n, GLuint *buffers)
{
	m_Real.glGenBuffers(n, buffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = BufferRes(buffers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_BUFFER);
				Serialise_glGenBuffers(1, buffers+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

size_t WrappedOpenGL::BufferIdx(GLenum buf)
{
	switch(buf)
	{
		case eGL_ARRAY_BUFFER:              return 0;
		case eGL_ATOMIC_COUNTER_BUFFER:     return 1;
		case eGL_COPY_READ_BUFFER:          return 2;
		case eGL_COPY_WRITE_BUFFER:         return 3;
		case eGL_DRAW_INDIRECT_BUFFER:      return 4;
		case eGL_DISPATCH_INDIRECT_BUFFER:  return 5;
		case eGL_ELEMENT_ARRAY_BUFFER:      return 6;
		case eGL_PIXEL_PACK_BUFFER:         return 7;
		case eGL_PIXEL_UNPACK_BUFFER:       return 8;
		case eGL_QUERY_BUFFER:              return 9;
		case eGL_SHADER_STORAGE_BUFFER:     return 10;
		case eGL_TEXTURE_BUFFER:            return 11;
		case eGL_TRANSFORM_FEEDBACK_BUFFER: return 12;
		case eGL_UNIFORM_BUFFER:            return 13;
		default:
			RDCERR("Unexpected enum as buffer target: %hs", ToStr::Get(buf).c_str());
	}

	return 0;
}

bool WrappedOpenGL::Serialise_glBindBuffer(GLenum target, GLuint buffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(BufferRes(buffer)));
	
	if(m_State >= WRITING)
	{
		size_t idx = BufferIdx(target);

		m_BufferRecord[idx]->datatype = Target;
	}
	else if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(Id);
		m_Real.glBindBuffer(Target, res.name);

		m_Buffers[GetResourceManager()->GetLiveID(Id)].curType = Target;
	}

	return true;
}

void WrappedOpenGL::glBindBuffer(GLenum target, GLuint buffer)
{
	m_Real.glBindBuffer(target, buffer);
	
	if(m_State == WRITING_CAPFRAME)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
			Serialise_glBindBuffer(target, buffer);

			chunk = scope.Get();
		}
		
		m_ContextRecord->AddChunk(chunk);
	}

	size_t idx = BufferIdx(target);

	if(buffer == 0)
	{
		m_BufferRecord[idx] = NULL;
		return;
	}

	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_BufferRecord[idx] = GetResourceManager()->GetResourceRecord(BufferRes(buffer));

		// it's legal to re-type buffers, generate another BindBuffer chunk to rename
		if(r->datatype != target)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(BIND_BUFFER);
				Serialise_glBindBuffer(target, buffer);

				chunk = scope.Get();
			}

			r->AddChunk(chunk);
		}
	}
}

bool WrappedOpenGL::Serialise_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint64_t, Bytesize, (uint64_t)size);

	byte *dummy = NULL;

	if(m_State >= WRITING && data == NULL)
	{
		dummy = new byte[size];
		data = dummy;
	}

	SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);

	uint64_t offs = m_pSerialiser->GetOffset();

	SERIALISE_ELEMENT(GLenum, Usage, usage);
	SERIALISE_ELEMENT(ResourceId, id, m_BufferRecord[BufferIdx(target)]->GetResourceID());

	if(m_State == READING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glBindBuffer(Target, res.name);
		m_Real.glBufferData(Target, (GLsizeiptr)Bytesize, bytes, Usage);
		
		m_Buffers[GetResourceManager()->GetLiveID(id)].size = Bytesize;

		SAFE_DELETE_ARRAY(bytes);
	}
	else if(m_State >= WRITING)
	{
		m_BufferRecord[BufferIdx(target)]->SetDataOffset(offs - Bytesize);
	}

	if(dummy)
		delete[] dummy;

	return true;
}

void WrappedOpenGL::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	m_Real.glBufferData(target, size, data, usage);
	
	size_t idx = BufferIdx(target);
	
	if(m_State >= WRITING)
	{
		RDCASSERT(m_BufferRecord[idx]);

		SCOPED_SERIALISE_CONTEXT(BUFFERDATA);
		Serialise_glBufferData(target, size, data, usage);

		Chunk *chunk = scope.Get();

		m_BufferRecord[idx]->AddChunk(chunk);
		m_BufferRecord[idx]->SetDataPtr(chunk->GetData());
		m_BufferRecord[idx]->Length = (int32_t)size;
	}
}

bool WrappedOpenGL::Serialise_glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_BufferRecord[BufferIdx(target)]->GetResourceID());

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glBindBufferBase(Target, Index, res.name);
	}

	return true;
}

void WrappedOpenGL::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_BASE);
		Serialise_glBindBufferBase(target, index, buffer);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glBindBufferBase(target, index, buffer);
}

bool WrappedOpenGL::Serialise_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_BufferRecord[BufferIdx(target)]->GetResourceID());
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);

	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glBindBufferRange(Target, Index, res.name, (GLintptr)Offset, (GLsizeiptr)Size);
	}

	return true;
}

void WrappedOpenGL::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_RANGE);
		Serialise_glBindBufferRange(target, index, buffer, offset, size);

		m_ContextRecord->AddChunk(scope.Get());
	}

	m_Real.glBindBufferRange(target, index, buffer, offset, size);
}

void *WrappedOpenGL::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
	if(m_State >= WRITING)
	{
		// haven't implemented non-invalidating write maps
		if((access & (GL_MAP_INVALIDATE_BUFFER_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_READ_BIT)) == 0)
			RDCUNIMPLEMENTED();
		
		// haven't implemented coherent/persistent bits
		if((access & (GL_MAP_COHERENT_BIT|GL_MAP_PERSISTENT_BIT)) != 0)
			RDCUNIMPLEMENTED();

		m_BufferRecord[BufferIdx(target)]->Map.offset = offset;
		m_BufferRecord[BufferIdx(target)]->Map.length = length;
		m_BufferRecord[BufferIdx(target)]->Map.access = access;

		if((access & GL_MAP_READ_BIT) != 0)
		{
			byte *ptr = m_BufferRecord[BufferIdx(target)]->GetDataPtr();

			if(ptr == NULL)
			{
				RDCWARN("Mapping buffer that hasn't been allocated");

				m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Read_Real;
				return m_Real.glMapBufferRange(target, offset, length, access);
			}

			ptr += offset;

			m_Real.glGetBufferSubData(target, offset, length, ptr);
			
			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Read;

			return ptr;
		}
		
		byte *ptr = m_BufferRecord[BufferIdx(target)]->GetDataPtr();

		if(ptr == NULL)
		{
			RDCWARN("Mapping buffer that hasn't been allocated");
			
			ptr = (byte *)m_Real.glMapBufferRange(target, offset, length, access);

			m_BufferRecord[BufferIdx(target)]->Map.ptr = ptr;
			m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write_Real;
		}
		else
		{
			if(m_State == WRITING_CAPFRAME)
			{
				ptr = new byte[length];
				
				m_BufferRecord[BufferIdx(target)]->Map.ptr = ptr;
				m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write_Alloc;
			}
			else
			{
				ptr += offset;
				
				m_BufferRecord[BufferIdx(target)]->Map.ptr = ptr;
				m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Mapped_Write;
			}
		}

		return ptr;
	}

	return m_Real.glMapBufferRange(target, offset, length, access);
}

bool WrappedOpenGL::Serialise_glUnmapBuffer(GLenum target)
{
	GLResourceRecord *record = NULL;

	if(m_State >= WRITING)
		record = m_BufferRecord[BufferIdx(target)];

	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, bufID, record->GetResourceID());
	SERIALISE_ELEMENT(uint64_t, offs, record->Map.offset);
	SERIALISE_ELEMENT(uint64_t, len, record->Map.length);

	uint64_t bufBindStart = 0;

	if(m_State >= WRITING)
	{
		if(Target == eGL_ATOMIC_COUNTER_BUFFER)
			m_Real.glGetInteger64i_v(eGL_ATOMIC_COUNTER_BUFFER_START, 0, (GLint64 *)&bufBindStart);
		if(Target == eGL_SHADER_STORAGE_BUFFER)
			m_Real.glGetInteger64i_v(eGL_SHADER_STORAGE_BUFFER_START, 0, (GLint64 *)&bufBindStart);
		if(Target == eGL_TRANSFORM_FEEDBACK_BUFFER)
			m_Real.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, 0, (GLint64 *)&bufBindStart);
		if(Target == eGL_UNIFORM_BUFFER)
			m_Real.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, 0, (GLint64 *)&bufBindStart);
	}

	SERIALISE_ELEMENT(uint64_t, bufOffs, bufBindStart);

	SERIALISE_ELEMENT_BUF(byte *, data, record->Map.ptr, (size_t)len);
	
	if(m_State < WRITING ||
			(m_State >= WRITING &&
				(record->Map.status == GLResourceRecord::Mapped_Write || record->Map.status == GLResourceRecord::Mapped_Write_Alloc)
			)
		)
	{
		GLuint oldBuf = 0;
		GLuint64 oldBufBase = 0;
		GLuint64 oldBufSize = 0;

		if(m_State == READING)
		{
			GLResource res = GetResourceManager()->GetLiveResource(bufID);
			m_Real.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 0, (GLint *)&oldBuf);
			m_Real.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, 0, (GLint64 *)&oldBufBase);
			m_Real.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, 0, (GLint64 *)&oldBufSize);
			m_Real.glBindBufferRange(eGL_UNIFORM_BUFFER, 0, res.name, (GLintptr)bufOffs, (GLsizeiptr)len);
		}

		void *ptr = m_Real.glMapBufferRange(Target, (GLintptr)offs, (GLsizeiptr)len, GL_MAP_WRITE_BIT);
		memcpy(ptr, data, (size_t)len);
		m_Real.glUnmapBuffer(Target);
		
		if(m_State == READING)
		{
			if(oldBufBase == 0 && oldBufSize == 0)
				m_Real.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, oldBuf);
			else
				m_Real.glBindBufferRange(eGL_UNIFORM_BUFFER, 0, oldBuf, (GLintptr)oldBufBase, (GLsizeiptr)oldBufSize);
		}
	}

	if(m_State < WRITING)
		delete[] data;

	return true;
}

GLboolean WrappedOpenGL::glUnmapBuffer(GLenum target)
{
	if(m_State >= WRITING)
	{
		RDCASSERT(m_BufferRecord[BufferIdx(target)]);

		auto status = m_BufferRecord[BufferIdx(target)]->Map.status;
		
		GLboolean ret = GL_TRUE;

		switch(status)
		{
			case GLResourceRecord::Unmapped:
				RDCERR("Unmapped buffer being passed to glUnmapBuffer");
				break;
			case GLResourceRecord::Mapped_Read:
				// can ignore
				break;
			case GLResourceRecord::Mapped_Read_Real:
				// need to do real unmap
				ret = m_Real.glUnmapBuffer(target);
				break;
			case GLResourceRecord::Mapped_Write:
			{
				if(m_State == WRITING_CAPFRAME)
					RDCWARN("Failed to cap frame - uncapped Map/Unmap");
				
				SCOPED_SERIALISE_CONTEXT(UNMAP);
				Serialise_glUnmapBuffer(target);
				
				if(m_State == WRITING_CAPFRAME)
					m_ContextRecord->AddChunk(scope.Get());
				else
					m_BufferRecord[BufferIdx(target)]->AddChunk(scope.Get());
				
				break;
			}
			case GLResourceRecord::Mapped_Write_Alloc:
			{
				SCOPED_SERIALISE_CONTEXT(UNMAP);
				Serialise_glUnmapBuffer(target);
				
				if(m_State == WRITING_CAPFRAME)
					m_ContextRecord->AddChunk(scope.Get());

				delete[] m_BufferRecord[BufferIdx(target)]->Map.ptr;

				break;
			}
			case GLResourceRecord::Mapped_Write_Real:
				RDCWARN("Throwing away map contents as we don't have datastore allocated");
				RDCWARN("Could init chunk here using known data (although maybe it's only partial)");
				ret = m_Real.glUnmapBuffer(target);
				break;
		}

		m_BufferRecord[BufferIdx(target)]->Map.status = GLResourceRecord::Unmapped;

		return ret;
	}

	return m_Real.glUnmapBuffer(target);
}

bool WrappedOpenGL::Serialise_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(int32_t, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(uint8_t, Norm, normalized);
	SERIALISE_ELEMENT(uint32_t, Stride, stride);
	SERIALISE_ELEMENT(uint64_t, Offset, (uint64_t)pointer);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glVertexAttribPointer(Index, Size, Type, Norm, Stride, (const void *)Offset);
	}

	return true;
}

void WrappedOpenGL::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	m_Real.glVertexAttribPointer(index, size, type, normalized, stride, pointer);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	if(m_State >= WRITING)
	{
		RDCASSERT(r);

		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
		Serialise_glVertexAttribPointer(index, size, type, normalized, stride, pointer);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEnableVertexAttribArray(GLuint index)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glEnableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glEnableVertexAttribArray(GLuint index)
{
	m_Real.glEnableVertexAttribArray(index);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	if(m_State >= WRITING)
	{
		RDCASSERT(r);

		SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
		Serialise_glEnableVertexAttribArray(index);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDisableVertexAttribArray(GLuint index)
{
	SERIALISE_ELEMENT(uint32_t, Index, index);
	SERIALISE_ELEMENT(ResourceId, id, m_VertexArrayRecord ? m_VertexArrayRecord->GetResourceID() : ResourceId());
	
	if(m_State < WRITING)
	{
		if(id != ResourceId())
		{
			GLResource res = GetResourceManager()->GetLiveResource(id);
			m_Real.glBindVertexArray(res.name);
		}
		else
		{
			m_Real.glBindVertexArray(0);
		}

		m_Real.glDisableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glDisableVertexAttribArray(GLuint index)
{
	m_Real.glDisableVertexAttribArray(index);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	RDCASSERT(r);
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(DISABLEVERTEXATTRIBARRAY);
		Serialise_glDisableVertexAttribArray(index);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glGenVertexArrays(GLsizei n, GLuint* arrays)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(VertexArrayRes(*arrays)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenVertexArrays(1, &real);
		
		GLResource res = VertexArrayRes(real);

		m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

void WrappedOpenGL::glGenVertexArrays(GLsizei n, GLuint *arrays)
{
	m_Real.glGenVertexArrays(n, arrays);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = VertexArrayRes(arrays[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GEN_VERTEXARRAY);
				Serialise_glGenVertexArrays(1, arrays+i);

				chunk = scope.Get();
			}

			GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

bool WrappedOpenGL::Serialise_glBindVertexArray(GLuint array)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(VertexArrayRes(array)));

	if(m_State <= EXECUTING)
	{
		m_Real.glBindVertexArray(GetResourceManager()->GetLiveResource(id).name);
	}

	return true;
}

void WrappedOpenGL::glBindVertexArray(GLuint array)
{
	m_Real.glBindVertexArray(array);

	if(m_State >= WRITING)
	{
		if(array == 0)
		{
			m_VertexArrayRecord = NULL;
		}
		else
		{
			m_VertexArrayRecord = GetResourceManager()->GetResourceRecord(VertexArrayRes(array));
		}
	}

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(BINDVERTEXARRAY);
		Serialise_glBindVertexArray(array);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	m_Real.glDeleteBuffers(n, buffers);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(BufferRes(buffers[i]));
}

void WrappedOpenGL::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
	m_Real.glDeleteVertexArrays(n, arrays);
	
	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(VertexArrayRes(arrays[i]));
}

#pragma endregion
