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

bool WrappedOpenGL::Serialise_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
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
		glFramebufferTexture2D(Target, Attach, TexTarget, res.name, Level);
	}

	return true;
}

void WrappedOpenGL::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	m_Real.glFramebufferTexture2D(target, attachment, textarget, texture, level);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
		Serialise_glFramebufferTexture2D(target, attachment, textarget, texture, level);

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

bool WrappedOpenGL::Serialise_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	SERIALISE_ELEMENT(GLenum, Target, target);
	SERIALISE_ELEMENT(GLenum, Attach, attachment);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(texture)));
	SERIALISE_ELEMENT(int32_t, Level, level);
	SERIALISE_ELEMENT(int32_t, Layer, layer);

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
		glFramebufferTextureLayer(Target, Attach, res.name, Level, Layer);
	}

	return true;
}

void WrappedOpenGL::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	m_Real.glFramebufferTextureLayer(target, attachment, texture, level, layer);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
		Serialise_glFramebufferTextureLayer(target, attachment, texture, level, layer);

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
