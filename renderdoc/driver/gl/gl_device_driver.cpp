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
		GLResource res = GetResourceManager()->GetLiveResource(Id);
		m_Real.glBindTexture(Target, res.name);

		m_Textures[GetResourceManager()->GetLiveID(Id)].curType = Target;
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

bool WrappedOpenGL::Serialise_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLsizei, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(GLsizei, Width, width);
	SERIALISE_ELEMENT(GLsizei, Height, height);
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

bool WrappedOpenGL::Serialise_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLint, Level, level);
	SERIALISE_ELEMENT(GLint, xoff, xoffset);
	SERIALISE_ELEMENT(GLint, yoff, yoffset);
	SERIALISE_ELEMENT(GLsizei, Width, width);
	SERIALISE_ELEMENT(GLsizei, Height, height);
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
	
	RDCASSERT(m_TextureRecord[m_TextureUnit]);
	{
		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D);
		Serialise_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);

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
	
	if(m_State == READING)
	{
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

		m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
	}
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
			RDCUNIMPLEMENTED();
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

void WrappedOpenGL::glBindSampler(GLuint unit, GLuint sampler)
{
	m_Real.glBindSampler(unit, sampler);
	
	if(m_State >= WRITING)
	{
		RDCUNIMPLEMENTED();
	}
}

void WrappedOpenGL::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
	m_Real.glSamplerParameteri(sampler, pname, param);
	
	if(m_State >= WRITING)
	{
		RDCUNIMPLEMENTED();
	}
}

bool WrappedOpenGL::Serialise_glPixelStorei(GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(GLint, Param, param);

	if(m_State == READING)
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
}

void WrappedOpenGL::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
	m_Real.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);

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

void WrappedOpenGL::glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	m_Real.glGenFramebuffers(n, framebuffers);

	for(GLsizei i=0; i < n; i++)
	{
		GLResource res = FramebufferRes(framebuffers[i]);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			RDCUNIMPLEMENTED();
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}
}

void WrappedOpenGL::glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture(target, attachment, texture, level);
	
	if(m_State >= WRITING)
		RDCUNIMPLEMENTED();
}

void WrappedOpenGL::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	m_Real.glDeleteFramebuffers(n, framebuffers);
	
	if(m_State >= WRITING)
		RDCUNIMPLEMENTED();

	for(GLsizei i=0; i < n; i++)
		GetResourceManager()->UnregisterResource(FramebufferRes(framebuffers[i]));
}

void WrappedOpenGL::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
	m_Real.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

#pragma endregion

#pragma region Shaders / Programs

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
	SERIALISE_ELEMENT(GLsizei, Count, count);

	vector<string> srcs;

	for(GLsizei i=0; i < Count; i++)
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

		for(GLsizei i=0; i < Count; i++)
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

#pragma region Uniforms

bool WrappedOpenGL::Serialise_glUniformMatrix(GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type)
{
	SERIALISE_ELEMENT(UniformType, Type, type);
	SERIALISE_ELEMENT(GLint, Loc, location);
	SERIALISE_ELEMENT(GLsizei, Count, count);
	SERIALISE_ELEMENT(GLboolean, Transpose, transpose);

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
	SERIALISE_ELEMENT(GLint, Loc, location);
	SERIALISE_ELEMENT(GLsizei, Count, count);
	
	size_t elemsPerVec = 0;

	switch(Type)
	{
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

#pragma endregion

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
	SERIALISE_ELEMENT_BUF(byte *, bytes, data, (size_t)Bytesize);
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

		m_BufferRecord[idx]->AddChunk(scope.Get());
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
		RDCUNIMPLEMENTED();
	}

	return m_Real.glMapBufferRange(target, offset, length, access);
}

GLboolean WrappedOpenGL::glUnmapBuffer(GLenum target)
{
	if(m_State >= WRITING)
	{
		RDCUNIMPLEMENTED();
	}

	return m_Real.glUnmapBuffer(target);
}

bool WrappedOpenGL::Serialise_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	SERIALISE_ELEMENT(GLuint, Index, index);
	SERIALISE_ELEMENT(GLint, Size, size);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(GLboolean, Norm, normalized);
	SERIALISE_ELEMENT(GLsizei, Stride, stride);
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
	RDCASSERT(r);
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(VERTEXATTRIBPOINTER);
		Serialise_glVertexAttribPointer(index, size, type, normalized, stride, pointer);

		r->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEnableVertexAttribArray(GLuint index)
{
	SERIALISE_ELEMENT(GLuint, Index, index);
	
	if(m_State < WRITING)
	{
		m_Real.glEnableVertexAttribArray(Index);
	}
	return true;
}

void WrappedOpenGL::glEnableVertexAttribArray(GLuint index)
{
	m_Real.glEnableVertexAttribArray(index);
	
	GLResourceRecord *r = m_VertexArrayRecord ? m_VertexArrayRecord : m_DeviceRecord;
	RDCASSERT(r);
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(ENABLEVERTEXATTRIBARRAY);
		Serialise_glEnableVertexAttribArray(index);

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
