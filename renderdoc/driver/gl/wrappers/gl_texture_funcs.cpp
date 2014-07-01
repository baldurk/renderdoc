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
#include "common/string_utils.h"
#include "../gl_driver.h"

bool WrappedOpenGL::Serialise_glGenTextures(GLsizei n, GLuint* textures)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), *textures)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenTextures(1, &real);
		
		GLResource res = TextureRes(GetCtx(), real);

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
		GLResource res = TextureRes(GetCtx(), textures[i]);
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
		GetResourceManager()->UnregisterResource(TextureRes(GetCtx(), textures[i]));
}

bool WrappedOpenGL::Serialise_glBindTexture(GLenum target, GLuint texture)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, Id, (texture ? GetResourceManager()->GetID(TextureRes(GetCtx(), texture)) : ResourceId()));
	
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
		m_Textures[GetResourceManager()->GetID(TextureRes(GetCtx(), texture))].curType = target;
	}

	if(texture == 0)
	{
		m_TextureRecord[m_TextureUnit] = NULL;
		return;
	}

	if(m_State >= WRITING)
	{
		GLResourceRecord *r = m_TextureRecord[m_TextureUnit] = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

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

bool WrappedOpenGL::Serialise_glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, InternalFormat, internalformat);
	SERIALISE_ELEMENT(uint32_t, MinLevel, minlevel);
	SERIALISE_ELEMENT(uint32_t, NumLevels, numlevels);
	SERIALISE_ELEMENT(uint32_t, MinLayer, minlayer);
	SERIALISE_ELEMENT(uint32_t, NumLayers, numlayers);
	SERIALISE_ELEMENT(ResourceId, texid, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(ResourceId, origid, GetResourceManager()->GetID(TextureRes(GetCtx(), origtexture)));

	if(m_State == READING)
	{
		GLResource tex = GetResourceManager()->GetLiveResource(texid);
		GLResource origtex = GetResourceManager()->GetLiveResource(origid);
		m_Real.glTextureView(tex.name, Target, origtex.name, InternalFormat, MinLevel, NumLevels, MinLayer, NumLayers);

		m_Textures[GetResourceManager()->GetLiveID(texid)].curType = Target;
	}

	return true;
}

void WrappedOpenGL::glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
	m_Real.glTextureView(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		GLResourceRecord *origrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), origtexture));
		RDCASSERT(record && origrecord);

		SCOPED_SERIALISE_CONTEXT(TEXTURE_VIEW);
		Serialise_glTextureView(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);

		record->AddChunk(scope.Get());
		record->AddParent(origrecord);

		// illegal to re-type textures
		if(record->datatype == eGL_UNKNOWN_ENUM)
			record->datatype = target;
		else
			RDCASSERT(record->datatype == target);
	}
}
		
bool WrappedOpenGL::Serialise_glGenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	if(m_State == READING)
	{
		m_Real.glGenerateTextureMipmapEXT(GetResourceManager()->GetLiveResource(id).name, Target);
	}

	return true;
}

void WrappedOpenGL::glGenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
	m_Real.glGenerateTextureMipmapEXT(texture, target);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(GENERATE_MIPMAP);
		Serialise_glGenerateTextureMipmapEXT(texture, target);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State == WRITING_IDLE)
	{
		SCOPED_SERIALISE_CONTEXT(GENERATE_MIPMAP);
		Serialise_glGenerateTextureMipmapEXT(texture, target);

		ResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);
		if(record)
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glGenerateMipmap(GLenum target)
{
	m_Real.glGenerateMipmap(target);

	ResourceRecord *record = m_TextureRecord[m_TextureUnit];
	
	RDCASSERT(record);
	if(!record) return;

	GLuint texture = GetResourceManager()->GetCurrentResource(record->GetResourceID()).name;

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(GENERATE_MIPMAP);
		Serialise_glGenerateTextureMipmapEXT(texture, target);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State == WRITING_IDLE)
	{
		SCOPED_SERIALISE_CONTEXT(GENERATE_MIPMAP);
		Serialise_glGenerateTextureMipmapEXT(texture, target);

		RDCASSERT(record);
		if(record)
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
												                         GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
												                         GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
	SERIALISE_ELEMENT(ResourceId, srcid, GetResourceManager()->GetID(TextureRes(GetCtx(), srcName)));
	SERIALISE_ELEMENT(ResourceId, dstid, GetResourceManager()->GetID(TextureRes(GetCtx(), dstName)));
	SERIALISE_ELEMENT(GLenum, SourceTarget, srcTarget);
	SERIALISE_ELEMENT(GLenum, DestTarget, dstTarget);
	SERIALISE_ELEMENT(uint32_t, SourceLevel, srcLevel);
	SERIALISE_ELEMENT(uint32_t, SourceX, srcX);
	SERIALISE_ELEMENT(uint32_t, SourceY, srcY);
	SERIALISE_ELEMENT(uint32_t, SourceZ, srcZ);
	SERIALISE_ELEMENT(uint32_t, SourceWidth, srcWidth);
	SERIALISE_ELEMENT(uint32_t, SourceHeight, srcHeight);
	SERIALISE_ELEMENT(uint32_t, SourceDepth, srcDepth);
	SERIALISE_ELEMENT(uint32_t, DestLevel, dstLevel);
	SERIALISE_ELEMENT(uint32_t, DestX, dstX);
	SERIALISE_ELEMENT(uint32_t, DestY, dstY);
	SERIALISE_ELEMENT(uint32_t, DestZ, dstZ);
	
	if(m_State < WRITING)
	{
		GLResource srcres = GetResourceManager()->GetLiveResource(srcid);
		GLResource dstres = GetResourceManager()->GetLiveResource(dstid);
		m_Real.glCopyImageSubData(srcres.name, SourceTarget, SourceLevel, SourceX, SourceY, SourceZ,
															dstres.name, DestTarget, DestLevel, DestX, DestY, DestZ,
															SourceWidth, SourceHeight, SourceDepth);
	}
	return true;
}

void WrappedOpenGL::glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
												               GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
												               GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
	m_Real.glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ,
												    dstName, dstTarget, dstLevel, dstX, dstY, dstZ,
												    srcWidth, srcHeight, srcDepth);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *srcrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), srcName));
		GLResourceRecord *dstrecord = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), dstName));
		RDCASSERT(srcrecord && dstrecord);

		SCOPED_SERIALISE_CONTEXT(COPY_SUBIMAGE);
		Serialise_glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ,
												         dstName, dstTarget, dstLevel, dstX, dstY, dstZ,
												         srcWidth, srcHeight, srcDepth);

		Chunk *chunk = scope.Get();

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);
		}
		else
		{
			dstrecord->AddChunk(chunk);
			dstrecord->AddParent(srcrecord);
		}
	}
}

bool WrappedOpenGL::Serialise_glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	
	if(m_State < WRITING)
	{
		m_Real.glTextureParameteriEXT(GetResourceManager()->GetLiveResource(id).name, Target, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
	m_Real.glTextureParameteriEXT(texture, target, pname, param);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERI);
		Serialise_glTextureParameteriEXT(texture, target, pname, param);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	m_Real.glTexParameteri(target, pname, param);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERI);
		Serialise_glTextureParameteriEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		 target, pname, param);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR || PName == eGL_TEXTURE_SWIZZLE_RGBA ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(int32_t, Params, params, nParams);

	if(m_State < WRITING)
	{
		m_Real.glTextureParameterivEXT(GetResourceManager()->GetLiveResource(id).name, Target, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params)
{
	m_Real.glTextureParameterivEXT(texture, target, pname, params);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERIV);
		Serialise_glTextureParameterivEXT(texture, target, pname, params);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	m_Real.glTexParameteriv(target, pname, params);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERIV);
		Serialise_glTextureParameterivEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		  target, pname, params);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(float, Param, param);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	
	if(m_State < WRITING)
	{
		m_Real.glTextureParameterfEXT(GetResourceManager()->GetLiveResource(id).name, Target, PName, Param);
	}

	return true;
}

void WrappedOpenGL::glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
	m_Real.glTextureParameterfEXT(texture, target, pname, param);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERF);
		Serialise_glTextureParameterfEXT(texture, target, pname, param);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	m_Real.glTexParameterf(target, pname, param);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERF);
		Serialise_glTextureParameterfEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		 target, pname, param);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat *params)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR || PName == eGL_TEXTURE_SWIZZLE_RGBA ? 4U : 1U);
	SERIALISE_ELEMENT_ARR(float, Params, params, nParams);

	if(m_State < WRITING)
	{
		m_Real.glTextureParameterfvEXT(GetResourceManager()->GetLiveResource(id).name, Target, PName, Params);
	}

	delete[] Params;

	return true;
}

void WrappedOpenGL::glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat *params)
{
	m_Real.glTextureParameterfvEXT(texture, target, pname, params);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERFV);
		Serialise_glTextureParameterfvEXT(texture, target, pname, params);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	m_Real.glTexParameterfv(target, pname, params);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXPARAMETERFV);
		Serialise_glTextureParameterfvEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		  target, pname, params);

		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glPixelStorei(GLenum pname, GLint param)
{
	SERIALISE_ELEMENT(GLenum, PName, pname);
	SERIALISE_ELEMENT(int32_t, Param, param);

	if(m_State < WRITING)
	{
		m_Real.glPixelStorei(PName, Param);
	}

	return true;
}

void WrappedOpenGL::glPixelStorei(GLenum pname, GLint param)
{
	m_Real.glPixelStorei(pname, param);

	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(PIXELSTORE);
		Serialise_glPixelStorei(pname, param);

		if(m_TextureRecord[m_TextureUnit])
			m_TextureRecord[m_TextureUnit]->AddChunk(scope.Get());
		else if(m_State == WRITING_IDLE)
			m_DeviceRecord->AddChunk(scope.Get());
		else if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glPixelStoref(GLenum pname, GLfloat param)
{
	glPixelStorei(pname, (GLint)param);
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

#pragma region Texture Storage/Upload

void WrappedOpenGL::glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	m_Real.glTexImage1D(target, level, internalformat, width, border, format, type, pixels);

	RDCUNIMPLEMENTED("Old glTexImage1D API not implemented");
}

void WrappedOpenGL::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
	m_Real.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);

	RDCUNIMPLEMENTED("Old glTexImage2D API not implemented");
}

void WrappedOpenGL::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
	m_Real.glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);

	RDCUNIMPLEMENTED("Old glTexImage3D API not implemented");
}

void WrappedOpenGL::glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTextureImage1DEXT(texture, target, level, internalformat, width, border, format, type, pixels);

	RDCUNIMPLEMENTED("Old glTexImage1D API not implemented");
}

void WrappedOpenGL::glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTextureImage2DEXT(texture, target, level, internalformat, width, height, border, format, type, pixels);

	RDCUNIMPLEMENTED("Old glTexImage2D API not implemented");
}

void WrappedOpenGL::glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, format, type, pixels);

	RDCUNIMPLEMENTED("Old glTexImage3D API not implemented");
}

void WrappedOpenGL::glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *pixels)
{
	m_Real.glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, pixels);

	RDCUNIMPLEMENTED("Old glCompressedTexImage1D API not implemented");
}

void WrappedOpenGL::glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * pixels)
{
	m_Real.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, pixels);

	RDCUNIMPLEMENTED("Old glCompressedTexImage2D API not implemented");
}

void WrappedOpenGL::glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * pixels)
{
	m_Real.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, pixels);

	RDCUNIMPLEMENTED("Old glCompressedTexImage3D API not implemented");
}

void WrappedOpenGL::glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *pixels)
{
	m_Real.glCompressedTextureImage1DEXT(texture, target, level, internalformat, width, border, imageSize, pixels);

	RDCUNIMPLEMENTED("Old glCompressedTexImage1D API not implemented");
}

void WrappedOpenGL::glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * pixels)
{
	m_Real.glCompressedTextureImage2DEXT(texture, target, level, internalformat, width, height, border, imageSize, pixels);

	RDCUNIMPLEMENTED("Old glCompressedTexImage2D API not implemented");
}

void WrappedOpenGL::glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * pixels)
{
	m_Real.glCompressedTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border, imageSize, pixels);

	RDCUNIMPLEMENTED("Old glCompressedTexImage3D API not implemented");
}

bool WrappedOpenGL::Serialise_glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		m_Textures[liveId].width = Width;
		m_Textures[liveId].height = 1;
		m_Textures[liveId].depth = 1;
		m_Textures[liveId].curType = Target;

		m_Real.glTextureStorage1DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Levels, Format, Width);
	}

	return true;
}

void WrappedOpenGL::glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	m_Real.glTextureStorage1DEXT(texture, target, levels, internalformat, width);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE1D);
		Serialise_glTextureStorage1DEXT(texture, target, levels, internalformat, width);

		record->AddChunk(scope.Get());

		// illegal to re-type textures
		if(record->datatype == eGL_UNKNOWN_ENUM)
			record->datatype = target;
		else
			RDCASSERT(record->datatype == target);
	}
}

void WrappedOpenGL::glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	m_Real.glTexStorage1D(target, levels, internalformat, width);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE1D);
		Serialise_glTextureStorage1DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		target, levels, internalformat, width);

		record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		m_Textures[liveId].width = Width;
		m_Textures[liveId].height = Height;
		m_Textures[liveId].depth = 1;
		m_Textures[liveId].curType = Target;

		m_Real.glTextureStorage2DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Levels, Format, Width, Height);
	}

	return true;
}

void WrappedOpenGL::glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	m_Real.glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE2D);
		Serialise_glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);

		record->AddChunk(scope.Get());

		// illegal to re-type textures
		if(record->datatype == eGL_UNKNOWN_ENUM)
			record->datatype = target;
		else
			RDCASSERT(record->datatype == target);
	}
}

void WrappedOpenGL::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	m_Real.glTexStorage2D(target, levels, internalformat, width, height);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE2D);
		Serialise_glTextureStorage2DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		target, levels, internalformat, width, height);

		record->AddChunk(scope.Get());

		// illegal to re-type textures
		if(record->datatype == eGL_UNKNOWN_ENUM)
			record->datatype = target;
		else
			RDCASSERT(record->datatype == target);
	}
}

bool WrappedOpenGL::Serialise_glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint32_t, Levels, levels);
	SERIALISE_ELEMENT(GLenum, Format, internalformat);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(uint32_t, Depth, depth);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	if(m_State == READING)
	{
		ResourceId liveId = GetResourceManager()->GetLiveID(id);
		m_Textures[liveId].width = Width;
		m_Textures[liveId].height = Height;
		m_Textures[liveId].depth = Depth;
		m_Textures[liveId].curType = Target;

		m_Real.glTextureStorage3DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Levels, Format, Width, Height, Depth);
	}

	return true;
}

void WrappedOpenGL::glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	m_Real.glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE3D);
		Serialise_glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);

		record->AddChunk(scope.Get());

		// illegal to re-type textures
		if(record->datatype == eGL_UNKNOWN_ENUM)
			record->datatype = target;
		else
			RDCASSERT(record->datatype == target);
	}
}

void WrappedOpenGL::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	m_Real.glTexStorage3D(target, levels, internalformat, width, height, depth);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSTORAGE3D);
		Serialise_glTextureStorage3DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		target, levels, internalformat, width, height, depth);

		record->AddChunk(scope.Get());

		// illegal to re-type textures
		if(record->datatype == eGL_UNKNOWN_ENUM)
			record->datatype = target;
		else
			RDCASSERT(record->datatype == target);
	}
}

bool WrappedOpenGL::Serialise_glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	
	GLint align = 1;
	m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
	
	GLint rowlen = 0;
	m_Real.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &rowlen);

	size_t subimageSize = GetByteSize(rowlen > 0 ? rowlen : Width, 1, 1, Format, Type, Level, align);

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, subimageSize);
	
	if(m_State == READING)
	{
		m_Real.glTextureSubImage1DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Level, xoff, Width, Format, Type, buf);

		delete[] buf;
	}

	return true;
}
		
void WrappedOpenGL::glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTextureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE1D);
		Serialise_glTextureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE1D);
		Serialise_glTextureSubImage1DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		 target, level, xoffset, width, format, type, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(int32_t, yoff, yoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(GLenum, Format, format);
	SERIALISE_ELEMENT(GLenum, Type, type);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	GLint align = 1;
	m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
	
	GLint rowlen = 0;
	m_Real.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &rowlen);
	
	GLint imgheight = 0;
	m_Real.glGetIntegerv(eGL_UNPACK_IMAGE_HEIGHT, &imgheight);

	size_t subimageSize = GetByteSize(rowlen > 0 ? rowlen : Width, imgheight > 0 ? imgheight : Height, 1, Format, Type, Level, align);

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, subimageSize);
	
	if(m_State == READING)
	{
		m_Real.glTextureSubImage2DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Level, xoff, yoff, Width, Height, Format, Type, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D);
		Serialise_glTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, type, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D);
		Serialise_glTextureSubImage2DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		 target, level, xoffset, yoffset, width, height, format, type, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
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
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	GLint align = 1;
	m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
	
	GLint rowlen = 0;
	m_Real.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &rowlen);
	
	GLint imgheight = 0;
	m_Real.glGetIntegerv(eGL_UNPACK_IMAGE_HEIGHT, &imgheight);

	size_t subimageSize = GetByteSize(rowlen > 0 ? rowlen : Width, imgheight > 0 ? imgheight : Height, Depth, Format, Type, Level, align);

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, subimageSize);
	
	if(m_State == READING)
	{
		m_Real.glTextureSubImage3DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Level, xoff, yoff, zoff, Width, Height, Depth, Format, Type, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D);
		Serialise_glTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	m_Real.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D);
		Serialise_glTextureSubImage3DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		 target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(GLenum, fmt, format);
	SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, byteSize);
	
	if(m_State == READING)
	{
		m_Real.glCompressedTextureSubImage1DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Level, xoff, Width, fmt, byteSize, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *pixels)
{
	m_Real.glCompressedTextureSubImage1DEXT(texture, target, level, xoffset, width, format, imageSize, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE1D_COMPRESSED);
		Serialise_glCompressedTextureSubImage1DEXT(texture, target, level, xoffset, width, format, imageSize, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *pixels)
{
	m_Real.glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE1D_COMPRESSED);
		Serialise_glCompressedTextureSubImage1DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		           target, level, xoffset, width, format, imageSize, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(int32_t, yoff, yoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(GLenum, fmt, format);
	SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, byteSize);
	
	if(m_State == READING)
	{
		m_Real.glCompressedTextureSubImage2DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Level, xoff, yoff, Width, Height, fmt, byteSize, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels)
{
	m_Real.glCompressedTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, imageSize, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D_COMPRESSED);
		Serialise_glCompressedTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format, imageSize, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *pixels)
{
	m_Real.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D_COMPRESSED);
		Serialise_glCompressedTextureSubImage2DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		           target, level, xoffset, yoffset, width, height, format, imageSize, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, xoff, xoffset);
	SERIALISE_ELEMENT(int32_t, yoff, yoffset);
	SERIALISE_ELEMENT(int32_t, zoff, zoffset);
	SERIALISE_ELEMENT(uint32_t, Width, width);
	SERIALISE_ELEMENT(uint32_t, Height, height);
	SERIALISE_ELEMENT(uint32_t, Depth, depth);
	SERIALISE_ELEMENT(GLenum, fmt, format);
	SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

	SERIALISE_ELEMENT_BUF(byte *, buf, pixels, byteSize);
	
	if(m_State == READING)
	{
		m_Real.glCompressedTextureSubImage3DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Level, xoff, yoff, zoff, Width, Height, Depth, fmt, byteSize, buf);

		delete[] buf;
	}

	return true;
}

void WrappedOpenGL::glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels)
{
	m_Real.glCompressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D_COMPRESSED);
		Serialise_glCompressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *pixels)
{
	m_Real.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D_COMPRESSED);
		Serialise_glCompressedTextureSubImage3DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		           target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels);

		if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			record->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(uint64_t, offs, (uint64_t)offset);
	SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);
	SERIALISE_ELEMENT(GLenum, fmt, internalformat);
	SERIALISE_ELEMENT(ResourceId, texid, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(TextureRes(GetCtx(), buffer)));
	
	if(m_State == READING)
	{
		m_Real.glTextureBufferRangeEXT(GetResourceManager()->GetLiveResource(texid).name,
																	 Target, fmt,
																	 GetResourceManager()->GetLiveResource(bufid).name,
																	 (GLintptr)offs, (GLsizeiptr)Size);
	}

	return true;
}

void WrappedOpenGL::glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	m_Real.glTextureBufferRangeEXT(texture, target, internalformat, buffer, offset, size);
		
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXBUFFER_RANGE);
		Serialise_glTextureBufferRangeEXT(texture, target, internalformat, buffer, offset, size);

		record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	m_Real.glTexBufferRange(target, internalformat, buffer, offset, size);
		
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_TextureRecord[m_TextureUnit];
		RDCASSERT(record);

		SCOPED_SERIALISE_CONTEXT(TEXBUFFER_RANGE);
		Serialise_glTextureBufferRangeEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																		  target, internalformat, buffer, offset, size);

		record->AddChunk(scope.Get());
	}
}

#pragma endregion
