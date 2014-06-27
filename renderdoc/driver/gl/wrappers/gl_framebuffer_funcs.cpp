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

bool WrappedOpenGL::Serialise_glGenFramebuffers(GLsizei n, GLuint* framebuffers)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(FramebufferRes(GetCtx(), *framebuffers)));

	if(m_State == READING)
	{
		GLuint real = 0;
		m_Real.glGenFramebuffers(1, &real);
		
		GLResource res = FramebufferRes(GetCtx(), real);

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
		GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
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

bool WrappedOpenGL::Serialise_glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			glNamedFramebufferTextureEXT(0, Attach, res.name, Level);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			glNamedFramebufferTextureEXT(fbres.name, Attach, res.name, Level);
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	m_Real.glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
		Serialise_glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);
		
		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture(target, attachment, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;

		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(m_DrawFramebufferRecord) record = m_DrawFramebufferRecord;
		}
		else
		{
			if(m_ReadFramebufferRecord) record = m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
		Serialise_glNamedFramebufferTextureEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, texture, level);
		
		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			glNamedFramebufferTexture2DEXT(0, Attach, TexTarget, res.name, Level);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			glNamedFramebufferTexture2DEXT(fbres.name, Attach, TexTarget, res.name, Level);
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
		Serialise_glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);
		
		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture2D(target, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;

		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(m_DrawFramebufferRecord) record = m_DrawFramebufferRecord;
		}
		else
		{
			if(m_ReadFramebufferRecord) record = m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
		Serialise_glNamedFramebufferTexture2DEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, textarget, texture, level);
		
		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, Layer, layer);
	SERIALISE_ELEMENT(ResourceId, fbid, (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))));
	
	if(m_State < WRITING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		if(fbid == ResourceId())
		{
			glNamedFramebufferTextureLayerEXT(0, Attach, res.name, Level, Layer);
		}
		else
		{
			GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
			glNamedFramebufferTextureLayerEXT(fbres.name, Attach, res.name, Level, Layer);
		}
	}

	return true;
}

void WrappedOpenGL::glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	m_Real.glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
		Serialise_glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);
		
		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	m_Real.glFramebufferTextureLayer(target, attachment, texture, level, layer);
	
	if(m_State >= WRITING)
	{
		GLResourceRecord *record = m_DeviceRecord;

		if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
		{
			if(m_DrawFramebufferRecord) record = m_DrawFramebufferRecord;
		}
		else
		{
			if(m_ReadFramebufferRecord) record = m_ReadFramebufferRecord;
		}

		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
		Serialise_glNamedFramebufferTextureLayerEXT(GetResourceManager()->GetCurrentResource(record->GetResourceID()).name,
																					 attachment, texture, level, layer);
		
		if(m_State == WRITING_IDLE)
			record->AddChunk(scope.Get());
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
			{
				Chunk *last = m_ReadFramebufferRecord->GetLastChunk();
				if(last->GetChunkType() == READ_BUFFER)
				{
					delete last;
					m_ReadFramebufferRecord->PopChunk();
				}
				m_ReadFramebufferRecord->AddChunk(scope.Get());
			}
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
	SERIALISE_ELEMENT(ResourceId, Id, (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)) : ResourceId()));

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
		m_DrawFramebufferRecord = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
	else
		m_ReadFramebufferRecord = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

	m_Real.glBindFramebuffer(target, framebuffer);
}

bool WrappedOpenGL::Serialise_glDrawBuffer(GLenum buf)
{
	SERIALISE_ELEMENT(GLenum, b, buf);

	if(m_State < WRITING)
	{
		// since we are faking the default framebuffer with our own
		// to see the results, replace back/front/left/right with color attachment 0
		if(b == eGL_BACK_LEFT || b == eGL_BACK_RIGHT || b == eGL_BACK ||
				b == eGL_FRONT_LEFT || b == eGL_FRONT_RIGHT || b == eGL_FRONT)
				b = eGL_COLOR_ATTACHMENT0;

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

bool WrappedOpenGL::Serialise_glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)));
	SERIALISE_ELEMENT(uint32_t, num, n);
	SERIALISE_ELEMENT_ARR(GLenum, buffers, bufs, num);

	if(m_State < WRITING)
	{
		for(uint32_t i=0; i < num; i++)
		{
			// since we are faking the default framebuffer with our own
			// to see the results, replace back/front/left/right with color attachment 0
			if(buffers[i] == eGL_BACK_LEFT || buffers[i] == eGL_BACK_RIGHT || buffers[i] == eGL_BACK ||
					buffers[i] == eGL_FRONT_LEFT || buffers[i] == eGL_FRONT_RIGHT || buffers[i] == eGL_FRONT)
					buffers[i] = eGL_COLOR_ATTACHMENT0;
		}

		m_Real.glFramebufferDrawBuffersEXT(GetResourceManager()->GetLiveResource(Id).name, num, buffers);
	}

	delete[] buffers;

	return true;
}

void WrappedOpenGL::glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
	m_Real.glFramebufferDrawBuffersEXT(framebuffer, n, bufs);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		Serialise_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
	else if(m_State == WRITING_IDLE && framebuffer != 0)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		Serialise_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);

		ResourceRecord *record = GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
		record->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glDrawBuffers(GLsizei n, const GLenum *bufs)
{
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
		if(m_DrawFramebufferRecord)
			Serialise_glFramebufferDrawBuffersEXT(GetResourceManager()->GetCurrentResource(m_DrawFramebufferRecord->GetResourceID()).name, n, bufs);
		else
			Serialise_glFramebufferDrawBuffersEXT(0, n, bufs);
		
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
		GetResourceManager()->UnregisterResource(FramebufferRes(GetCtx(), framebuffers[i]));
}
