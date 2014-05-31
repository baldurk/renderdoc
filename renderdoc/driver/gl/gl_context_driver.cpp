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
#include "gl_driver.h"

#pragma region State functions

bool WrappedOpenGL::Serialise_glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	SERIALISE_ELEMENT(GLenum, s, sfactor);
	SERIALISE_ELEMENT(GLenum, d, dfactor);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendFunc(s, d);
	}

	return true;
}

void WrappedOpenGL::glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	m_Real.glBlendFunc(sfactor, dfactor);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_FUNC);
		Serialise_glBlendFunc(sfactor, dfactor);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBlendFunci(GLuint buf, GLenum src, GLenum dst)
{
	SERIALISE_ELEMENT(GLuint, b, buf);
	SERIALISE_ELEMENT(GLenum, s, src);
	SERIALISE_ELEMENT(GLenum, d, dst);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendFunci(b, s, d);
	}

	return true;
}

void WrappedOpenGL::glBlendFunci(GLuint buf, GLenum src, GLenum dst)
{
	m_Real.glBlendFunci(buf, src, dst);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_FUNCI);
		Serialise_glBlendFunci(buf, src, dst);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	SERIALISE_ELEMENT(float, r, red);
	SERIALISE_ELEMENT(float, g, green);
	SERIALISE_ELEMENT(float, b, blue);
	SERIALISE_ELEMENT(float, a, alpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendColor(r, g, b, a);
	}

	return true;
}

void WrappedOpenGL::glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	m_Real.glBlendColor(red, green, blue, alpha);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_COLOR);
		Serialise_glBlendColor(red, green, blue, alpha);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
	SERIALISE_ELEMENT(GLenum, s1, sfactorRGB);
	SERIALISE_ELEMENT(GLenum, d1, dfactorRGB);
	SERIALISE_ELEMENT(GLenum, s2, sfactorAlpha);
	SERIALISE_ELEMENT(GLenum, d2, dfactorAlpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendFuncSeparate(s1, d1, s2, d2);
	}

	return true;
}

void WrappedOpenGL::glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
	m_Real.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_FUNC_SEP);
		Serialise_glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
	SERIALISE_ELEMENT(uint32_t, b, buf);
	SERIALISE_ELEMENT(GLenum, s1, sfactorRGB);
	SERIALISE_ELEMENT(GLenum, d1, dfactorRGB);
	SERIALISE_ELEMENT(GLenum, s2, sfactorAlpha);
	SERIALISE_ELEMENT(GLenum, d2, dfactorAlpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendFuncSeparatei(b, s1, d1, s2, d2);
	}

	return true;
}

void WrappedOpenGL::glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
	m_Real.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_FUNC_SEPI);
		Serialise_glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
	SERIALISE_ELEMENT(GLenum, m1, modeRGB);
	SERIALISE_ELEMENT(GLenum, m2, modeAlpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendEquationSeparate(m1, m2);
	}

	return true;
}

void WrappedOpenGL::glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
	m_Real.glBlendEquationSeparate(modeRGB, modeAlpha);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_EQ_SEP);
		Serialise_glBlendEquationSeparate(modeRGB, modeAlpha);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
	SERIALISE_ELEMENT(uint32_t, b, buf);
	SERIALISE_ELEMENT(GLenum, m1, modeRGB);
	SERIALISE_ELEMENT(GLenum, m2, modeAlpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glBlendEquationSeparatei(b, m1, m2);
	}

	return true;
}

void WrappedOpenGL::glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
	m_Real.glBlendEquationSeparatei(buf, modeRGB, modeAlpha);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(BLEND_EQ_SEPI);
		Serialise_glBlendEquationSeparatei(buf, modeRGB, modeAlpha);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	SERIALISE_ELEMENT(float, r, red);
	SERIALISE_ELEMENT(float, g, green);
	SERIALISE_ELEMENT(float, b, blue);
	SERIALISE_ELEMENT(float, a, alpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glClearColor(r, g, b, a);
	}

	return true;
}

void WrappedOpenGL::glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	m_Real.glClearColor(red, green, blue, alpha);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
		Serialise_glClearColor(red, green, blue, alpha);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearDepth(GLclampd depth)
{
	SERIALISE_ELEMENT(double, d, depth);

	if(m_State <= EXECUTING)
	{
		m_Real.glClearDepth(d);
	}

	return true;
}

void WrappedOpenGL::glClearDepth(GLclampd depth)
{
	m_Real.glClearDepth(depth);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTH);
		Serialise_glClearDepth(depth);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDepthFunc(GLenum func)
{
	SERIALISE_ELEMENT(GLenum, f, func);

	if(m_State <= EXECUTING)
	{
		m_Real.glDepthFunc(func);
	}

	return true;
}

void WrappedOpenGL::glDepthFunc(GLenum func)
{
	m_Real.glDepthFunc(func);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DEPTH_FUNC);
		Serialise_glDepthFunc(func);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDepthMask(GLboolean flag)
{
	SERIALISE_ELEMENT(uint8_t, f, flag);

	if(m_State <= EXECUTING)
	{
		m_Real.glDepthMask(f);
	}

	return true;
}

void WrappedOpenGL::glDepthMask(GLboolean flag)
{
	m_Real.glDepthMask(flag);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DEPTH_MASK);
		Serialise_glDepthMask(flag);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v)
{
	SERIALISE_ELEMENT(uint32_t, idx, first);
	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT_ARR(GLdouble, ranges, v, cnt*2);

	if(m_State <= EXECUTING)
	{
		m_Real.glDepthRangeArrayv(idx, cnt, ranges);
	}

	delete[] ranges;

	return true;
}

void WrappedOpenGL::glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v)
{
	m_Real.glDepthRangeArrayv(first, count, v);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DEPTH_RANGEARRAY);
		Serialise_glDepthRangeArrayv(first, count, v);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDisable(GLenum cap)
{
	SERIALISE_ELEMENT(GLenum, c, cap);

	if(m_State <= EXECUTING)
	{
		m_Real.glDisable(c);
	}

	return true;
}

void WrappedOpenGL::glDisable(GLenum cap)
{
	m_Real.glDisable(cap);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISABLE);
		Serialise_glDisable(cap);

		m_ContextRecord->AddChunk(scope.Get());
	}
	// TODO replace this with glIsEnabled() for the relevant states
	if(m_State == WRITING_IDLE)
	{
		SCOPED_SERIALISE_CONTEXT(DISABLE);
		Serialise_glDisable(cap);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEnable(GLenum cap)
{
	SERIALISE_ELEMENT(GLenum, c, cap);

	if(m_State <= EXECUTING)
	{
		m_Real.glEnable(c);
	}

	return true;
}

void WrappedOpenGL::glEnable(GLenum cap)
{
	m_Real.glEnable(cap);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(ENABLE);
		Serialise_glEnable(cap);

		m_ContextRecord->AddChunk(scope.Get());
	}
	// TODO replace this with glIsEnabled() for the relevant states
	if(m_State == WRITING_IDLE)
	{
		SCOPED_SERIALISE_CONTEXT(ENABLE);
		Serialise_glEnable(cap);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDisablei(GLenum cap, GLuint index)
{
	SERIALISE_ELEMENT(GLenum, c, cap);
	SERIALISE_ELEMENT(uint32_t, i, index);

	if(m_State <= EXECUTING)
	{
		m_Real.glDisablei(c, i);
	}

	return true;
}

void WrappedOpenGL::glDisablei(GLenum cap, GLuint index)
{
	m_Real.glDisablei(cap, index);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DISABLEI);
		Serialise_glDisablei(cap, index);

		m_ContextRecord->AddChunk(scope.Get());
	}
	// TODO replace this with glIsEnabled() for the relevant states
	if(m_State == WRITING_IDLE)
	{
		SCOPED_SERIALISE_CONTEXT(DISABLEI);
		Serialise_glDisablei(cap, index);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEnablei(GLenum cap, GLuint index)
{
	SERIALISE_ELEMENT(GLenum, c, cap);
	SERIALISE_ELEMENT(uint32_t, i, index);

	if(m_State <= EXECUTING)
	{
		m_Real.glEnablei(c, i);
	}

	return true;
}

void WrappedOpenGL::glEnablei(GLenum cap, GLuint index)
{
	m_Real.glEnablei(cap, index);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(ENABLEI);
		Serialise_glEnablei(cap, index);

		m_ContextRecord->AddChunk(scope.Get());
	}
	// TODO replace this with glIsEnabled() for the relevant states
	if(m_State == WRITING_IDLE)
	{
		SCOPED_SERIALISE_CONTEXT(ENABLEI);
		Serialise_glEnablei(cap, index);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glFrontFace(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, m, mode);

	if(m_State <= EXECUTING)
	{
		m_Real.glFrontFace(m);
	}

	return true;
}

void WrappedOpenGL::glFrontFace(GLenum mode)
{
	m_Real.glFrontFace(mode);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(FRONT_FACE);
		Serialise_glFrontFace(mode);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glCullFace(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, m, mode);

	if(m_State <= EXECUTING)
	{
		m_Real.glCullFace(m);
	}

	return true;
}

void WrappedOpenGL::glCullFace(GLenum mode)
{
	m_Real.glCullFace(mode);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CULL_FACE);
		Serialise_glCullFace(mode);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glHint(GLenum target, GLenum mode)
{
	m_Real.glHint(target, mode);

	RDCUNIMPLEMENTED();
}

bool WrappedOpenGL::Serialise_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	SERIALISE_ELEMENT(uint8_t, r, red);
	SERIALISE_ELEMENT(uint8_t, g, green);
	SERIALISE_ELEMENT(uint8_t, b, blue);
	SERIALISE_ELEMENT(uint8_t, a, alpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glColorMask(r, g, b, a);
	}

	return true;
}

void WrappedOpenGL::glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	m_Real.glColorMask(red, green, blue, alpha);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(COLOR_MASK);
		Serialise_glColorMask(red, green, blue, alpha);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	SERIALISE_ELEMENT(uint32_t, buffer, buf);
	SERIALISE_ELEMENT(uint8_t, r, red);
	SERIALISE_ELEMENT(uint8_t, g, green);
	SERIALISE_ELEMENT(uint8_t, b, blue);
	SERIALISE_ELEMENT(uint8_t, a, alpha);

	if(m_State <= EXECUTING)
	{
		m_Real.glColorMaski(buffer, r, g, b, a);
	}

	return true;
}

void WrappedOpenGL::glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	m_Real.glColorMaski(buf, red, green, blue, alpha);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(COLOR_MASKI);
		Serialise_glColorMaski(buf, red, green, blue, alpha);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(int32_t, X, x);
	SERIALISE_ELEMENT(int32_t, Y, y);
	SERIALISE_ELEMENT(uint32_t, W, width);
	SERIALISE_ELEMENT(uint32_t, H, height);

	if(m_State <= EXECUTING)
	{
		m_Real.glViewport(X, Y, W, H);
	}

	return true;
}

void WrappedOpenGL::glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	m_Real.glViewport(x, y, width, height);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(VIEWPORT);
		Serialise_glViewport(x, y, width, height);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glViewportArrayv(GLuint index, GLuint count, const GLfloat *v)
{
	SERIALISE_ELEMENT(uint32_t, idx, index);
	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT_ARR(GLfloat, views, v, cnt*4);

	if(m_State <= EXECUTING)
	{
		m_Real.glViewportArrayv(idx, cnt, views);
	}

	delete[] views;

	return true;
}

void WrappedOpenGL::glViewportArrayv(GLuint index, GLuint count, const GLfloat *v)
{
	m_Real.glViewportArrayv(index, count, v);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(VIEWPORT_ARRAY);
		Serialise_glViewportArrayv(index, count, v);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
	const float v[4] = { x, y, w, h };
	glViewportArrayv(index, 1, v);
}

void WrappedOpenGL::glViewportIndexedfv(GLuint index, const GLfloat *v)
{
	glViewportArrayv(index, 1, v);
}

bool WrappedOpenGL::Serialise_glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	SERIALISE_ELEMENT(int32_t, X, x);
	SERIALISE_ELEMENT(int32_t, Y, y);
	SERIALISE_ELEMENT(uint32_t, W, width);
	SERIALISE_ELEMENT(uint32_t, H, height);

	if(m_State <= EXECUTING)
	{
		m_Real.glScissor(X, Y, W, H);
	}

	return true;
}

void WrappedOpenGL::glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	m_Real.glScissor(x, y, width, height);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SCISSOR);
		Serialise_glScissor(x, y, width, height);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glScissorArrayv(GLuint index, GLsizei count, const GLint *v)
{
	SERIALISE_ELEMENT(uint32_t, idx, index);
	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT_ARR(GLint, scissors, v, cnt*4);

	if(m_State <= EXECUTING)
	{
		m_Real.glScissorArrayv(idx, cnt, scissors);
	}

	delete[] scissors;

	return true;
}

void WrappedOpenGL::glScissorArrayv(GLuint first, GLsizei count, const GLint *v)
{
	m_Real.glScissorArrayv(first, count, v);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(SCISSOR_ARRAY);
		Serialise_glScissorArrayv(first, count, v);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

void WrappedOpenGL::glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
	const GLint v[4] = { left, bottom, width, height };
	glScissorArrayv(index, 1, v);
}

void WrappedOpenGL::glScissorIndexedv(GLuint index, const GLint *v)
{
	glScissorArrayv(index, 1, v);
}

bool WrappedOpenGL::Serialise_glPolygonMode(GLenum face, GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, f, face);
	SERIALISE_ELEMENT(GLenum, m, mode);

	if(m_State <= EXECUTING)
	{
		m_Real.glPolygonMode(f, m);
	}

	return true;
}

void WrappedOpenGL::glPolygonMode(GLenum face, GLenum mode)
{
	m_Real.glPolygonMode(face, mode);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(POLYGON_MODE);
		Serialise_glPolygonMode(face, mode);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glPolygonOffset(GLfloat factor, GLfloat units)
{
	SERIALISE_ELEMENT(float, f, factor);
	SERIALISE_ELEMENT(float, u, units);

	if(m_State <= EXECUTING)
	{
		m_Real.glPolygonOffset(f, u);
	}

	return true;
}

void WrappedOpenGL::glPolygonOffset(GLfloat factor, GLfloat units)
{
	m_Real.glPolygonOffset(factor, units);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(POLYGON_OFFSET);
		Serialise_glPolygonOffset(factor, units);
		
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

	if(framebuffer == 0 && (m_State == READING || m_State == EXECUTING))
		framebuffer = m_FakeBB_FBO;

	m_Real.glBindFramebuffer(target, framebuffer);
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

#pragma endregion

#pragma region Debugging annotation

void WrappedOpenGL::glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label)
{
	m_Real.glGetObjectLabel(identifier, name, bufSize, length, label);
}

bool WrappedOpenGL::Serialise_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
	ResourceId liveid;

	string Label;
	if(m_State >= WRITING)
	{
		if(length == 0)
			Label = "";
		else
			Label = string(label, label + (length > 0 ? length : strlen(label)));

		switch(identifier)
		{
			case eGL_TEXTURE:
				liveid = GetResourceManager()->GetID(TextureRes(name));
				break;
			case eGL_BUFFER:
				liveid = GetResourceManager()->GetID(BufferRes(name));
				break;
			default:
				RDCERR("Unhandled namespace in glObjectLabel");
		}
	}

	SERIALISE_ELEMENT(GLenum, Identifier, identifier);
	SERIALISE_ELEMENT(ResourceId, id, liveid);
	SERIALISE_ELEMENT(uint32_t, Length, length);
	SERIALISE_ELEMENT(bool, HasLabel, label != NULL);

	m_pSerialiser->SerialiseString("label", Label);
	
	if(m_State == READING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(id);
		m_Real.glObjectLabel(Identifier, res.name, Length, HasLabel ? Label.c_str() : NULL);
	}

	return true;
}

void WrappedOpenGL::glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
	m_Real.glObjectLabel(identifier, name, length, label);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(OBJECT_LABEL);
		Serialise_glObjectLabel(identifier, name, length, label);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

#pragma endregion

#pragma region Drawcalls

bool WrappedOpenGL::Serialise_glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(int32_t, First, first);
	SERIALISE_ELEMENT(uint32_t, Count, count);
	SERIALISE_ELEMENT(uint32_t, InstanceCount, instancecount);
	SERIALISE_ELEMENT(uint32_t, BaseInstance, baseinstance);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawArraysInstancedBaseInstance(Mode, First, Count, InstanceCount, BaseInstance);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS_INSTANCEDBASEDINSTANCE, desc);
		string name = "glDrawArraysInstancedBaseInstance(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ", " +
						ToStr::Get(InstanceCount) + ", " +
						ToStr::Get(BaseInstance) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = InstanceCount;
		draw.indexOffset = First;
		draw.vertexOffset = 0;
		draw.instanceOffset = BaseInstance;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		m_LastDrawMode = Mode;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
	m_Real.glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS_INSTANCEDBASEDINSTANCE);
		Serialise_glDrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);
	SERIALISE_ELEMENT(int32_t, First, first);
	SERIALISE_ELEMENT(uint32_t, Count, count);

	if(m_State <= EXECUTING)
	{
		m_Real.glDrawArrays(Mode, First, Count);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(DRAWARRAYS, desc);
		string name = "glDrawArrays(" +
						ToStr::Get(Mode) + ", " +
						ToStr::Get(First) + ", " +
						ToStr::Get(Count) + ", " + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.numIndices = Count;
		draw.numInstances = 1;
		draw.indexOffset = First;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall|eDraw_Instanced;

		m_LastDrawMode = Mode;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	m_Real.glDrawArrays(mode, first, count);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DRAWARRAYS);
		Serialise_glDrawArrays(mode, first, count);

		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	
	if(buf != eGL_DEPTH)
	{
		Vec4f v;
		if(value) v = *((Vec4f *)value);

		m_pSerialiser->Serialise<4>("value", (float *)&v.x);
		
		if(m_State <= EXECUTING)
			m_Real.glClearBufferfv(buf, draw, &v.x);
	}
	else
	{
		SERIALISE_ELEMENT(float, val, *value);

		if(m_State <= EXECUTING)
			m_Real.glClearBufferfv(buf, draw, &val);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERF, desc);
		string name = "glClearBufferfv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}


	return true;
}

void WrappedOpenGL::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
	m_Real.glClearBufferfv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERF);
		Serialise_glClearBufferfv(buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	
	if(buf != eGL_STENCIL)
	{
		int32_t v[4];
		if(value) memcpy(v, value, sizeof(v));

		m_pSerialiser->Serialise<4>("value", v);
		
		if(m_State <= EXECUTING)
			m_Real.glClearBufferiv(buf, draw, v);
	}
	else
	{
		SERIALISE_ELEMENT(int32_t, val, *value);

		if(m_State <= EXECUTING)
			m_Real.glClearBufferiv(buf, draw, &val);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERI, desc);
		string name = "glClearBufferiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}


	return true;
}

void WrappedOpenGL::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
	m_Real.glClearBufferiv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERI);
		Serialise_glClearBufferiv(buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	
	{
		uint32_t v[4];
		if(value) memcpy(v, value, sizeof(v));

		m_pSerialiser->Serialise<4>("value", v);
		
		if(m_State <= EXECUTING)
			m_Real.glClearBufferuiv(buf, draw, v);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERUI, desc);
		string name = "glClearBufferuiv(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
	m_Real.glClearBufferuiv(buffer, drawbuffer, value);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERUI);
		Serialise_glClearBufferuiv(buffer, drawbuffer, value);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}
		
bool WrappedOpenGL::Serialise_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
	SERIALISE_ELEMENT(GLenum, buf, buffer);
	SERIALISE_ELEMENT(int32_t, draw, drawbuffer);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(int32_t, s, stencil);
	
	if(m_State <= EXECUTING)
		m_Real.glClearBufferfi(buf, draw, d, s);
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERFI, desc);
		string name = "glClearBufferfi(" +
						ToStr::Get(buf) + ", " +
						ToStr::Get(draw) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
	m_Real.glClearBufferfi(buffer, drawbuffer, depth, stencil);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEARBUFFERFI);
		Serialise_glClearBufferfi(buffer, drawbuffer, depth, stencil);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glClear(GLbitfield mask)
{
	SERIALISE_ELEMENT(uint32_t, Mask, mask);

	if(m_State <= EXECUTING)
		m_Real.glClear(Mask);
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(CLEARBUFFERF, desc);
		string name = "glClear(";
		if(Mask & GL_DEPTH_BUFFER_BIT)
			name += "GL_DEPTH_BUFFER_BIT | ";
		if(Mask & GL_COLOR_BUFFER_BIT)
			name += "GL_COLOR_BUFFER_BIT | ";
		if(Mask & GL_STENCIL_BUFFER_BIT)
			name += "GL_STENCIL_BUFFER_BIT | ";

		if(Mask & (eGL_DEPTH_BUFFER_BIT|eGL_COLOR_BUFFER_BIT|eGL_STENCIL_BUFFER_BIT))
		{
			name.pop_back(); // ' '
			name.pop_back(); // '|'
			name.pop_back(); // ' '
		}

		name += ")";

		FetchDrawcall draw;
		draw.name = widen(name);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glClear(GLbitfield mask)
{
	m_Real.glClear(mask);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CLEAR);
		Serialise_glClear(mask);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

#pragma endregion

// most of this is just hacks to get glxgears working :)

#pragma region Legacy/Immediate mode

bool WrappedOpenGL::Serialise_glGenLists(GLsizei range)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(DisplayListRes((GLuint)range)));

	if(m_State == READING)
	{
		GLuint real = m_Real.glGenLists(1);
		
		GLResource res = DisplayListRes(real);

		ResourceId live = m_ResourceManager->RegisterResource(res);
		GetResourceManager()->AddLiveResource(id, res);
	}

	return true;
}

GLuint WrappedOpenGL::glGenLists(GLsizei range)
{
	GLuint listret = m_Real.glGenLists(range);

	RDCASSERT(range == 1); // assumption from glxgears.
	
	for(GLsizei i=0; i < range; i++)
	{
		GLResource res = DisplayListRes(listret+i);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GENLISTS);
				Serialise_glGenLists((GLsizei)(listret+i));

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

	return listret;
}

bool WrappedOpenGL::Serialise_glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	SERIALISE_ELEMENT(GLenum, Light, light);
	SERIALISE_ELEMENT(GLenum, Name, pname);

	Vec4f v;
	if(params) v = *((Vec4f *)params);

	m_pSerialiser->Serialise<4>("params", (float *)&v.x);
		
	if(m_State <= EXECUTING)
		m_Real.glLightfv(Light, Name, &v.x);

	return true;
}

void WrappedOpenGL::glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	RDCASSERT(pname == (GLenum)0x1203 /*eGL_POSITION*/); // assumption from glxgears.

	m_Real.glLightfv(light, pname, params);
	
	{
		SCOPED_SERIALISE_CONTEXT(LIGHTFV);
		Serialise_glLightfv(light, pname, params);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	SERIALISE_ELEMENT(GLenum, Face, face);
	SERIALISE_ELEMENT(GLenum, Name, pname);

	Vec4f v;
	if(params) v = *((Vec4f *)params);

	m_pSerialiser->Serialise<4>("params", (float *)&v.x);
		
	if(m_State <= EXECUTING)
		m_Real.glMaterialfv(Face, Name, &v.x);

	return true;
}

void WrappedOpenGL::glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	RDCASSERT(pname == (GLenum)0x1602 /*eGL_AMBIENT_AND_DIFFUSE*/); // assumption from glxgears.

	m_Real.glMaterialfv(face, pname, params);
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(MATERIALFV);
		Serialise_glMaterialfv(face, pname, params);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glNewList(GLuint list, GLenum mode)
{
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(DisplayListRes(list)));
	SERIALISE_ELEMENT(GLenum, Mode, mode);

	if(m_State <= EXECUTING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(Id);
		m_Real.glNewList(res.name, Mode);
	}

	return true;
}

void WrappedOpenGL::glNewList(GLuint list, GLenum mode)
{
	m_Real.glNewList(list, mode);
	
	RDCASSERT(m_DisplayListRecord == NULL);
	m_DisplayListRecord = m_ResourceManager->GetResourceRecord(DisplayListRes(list));
	{
		SCOPED_SERIALISE_CONTEXT(NEWLIST);
		Serialise_glNewList(list, mode);
		
		m_DisplayListRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEndList()
{
	if(m_State <= EXECUTING)
		m_Real.glEndList();

	return true;
}

void WrappedOpenGL::glEndList()
{
	m_Real.glEndList();

	RDCASSERT(m_DisplayListRecord);
	{
		SCOPED_SERIALISE_CONTEXT(ENDLIST);
		Serialise_glEndList();
		
		m_DisplayListRecord->AddChunk(scope.Get());
	}
	m_DisplayListRecord = NULL;
}

bool WrappedOpenGL::Serialise_glCallList(GLuint list)
{
	SERIALISE_ELEMENT(ResourceId, Id, GetResourceManager()->GetID(DisplayListRes(list)));

	if(m_State <= EXECUTING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(Id);
		m_Real.glCallList(res.name);
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		GLResource res = GetResourceManager()->GetLiveResource(Id);
		AddEvent(CALLLIST, desc);
		string name = "glCallList(" + ToStr::Get(res.name) + ")";

		FetchDrawcall draw;
		draw.name = widen(name);

		draw.flags |= eDraw_CmdList;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedOpenGL::glCallList(GLuint list)
{
	m_Real.glCallList(list);
	
	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(CALLLIST);
		Serialise_glCallList(list);
		
		m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glShadeModel(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);

	if(m_State <= EXECUTING)
		m_Real.glShadeModel(Mode);

	return true;
}

void WrappedOpenGL::glShadeModel(GLenum mode)
{
	m_Real.glShadeModel(mode);
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(SHADEMODEL);
		Serialise_glShadeModel(mode);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glBegin(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);

	if(m_State <= EXECUTING)
		m_Real.glBegin(Mode);

	return true;
}

void WrappedOpenGL::glBegin(GLenum mode)
{
	m_Real.glBegin(mode);
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(BEGIN);
		Serialise_glBegin(mode);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glEnd()
{
	if(m_State <= EXECUTING)
		m_Real.glEnd();

	return true;
}

void WrappedOpenGL::glEnd()
{
	m_Real.glEnd();
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(END);
		Serialise_glEnd();

		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	SERIALISE_ELEMENT(float, X, x);
	SERIALISE_ELEMENT(float, Y, y);
	SERIALISE_ELEMENT(float, Z, z);

	if(m_State <= EXECUTING)
		m_Real.glVertex3f(X, Y, Z);

	return true;
}

void WrappedOpenGL::glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	m_Real.glVertex3f(x, y, z);
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(VERTEX3F);
		Serialise_glVertex3f(x, y, z);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	SERIALISE_ELEMENT(float, NX, nx);
	SERIALISE_ELEMENT(float, NY, ny);
	SERIALISE_ELEMENT(float, NZ, nz);

	if(m_State <= EXECUTING)
		m_Real.glNormal3f(NX, NY, NZ);

	return true;
}

void WrappedOpenGL::glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	m_Real.glNormal3f(nx, ny, nz);
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(NORMAL3F);
		Serialise_glNormal3f(nx, ny, nz);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glPushMatrix()
{
	if(m_State <= EXECUTING)
		m_Real.glPushMatrix();

	return true;
}

void WrappedOpenGL::glPushMatrix()
{
	m_Real.glPushMatrix();
	
	{
		SCOPED_SERIALISE_CONTEXT(PUSHMATRIX);
		Serialise_glPushMatrix();
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glPopMatrix()
{
	if(m_State <= EXECUTING)
		m_Real.glPopMatrix();

	return true;
}

void WrappedOpenGL::glPopMatrix()
{
	m_Real.glPopMatrix();
	
	{
		SCOPED_SERIALISE_CONTEXT(POPMATRIX);
		Serialise_glPopMatrix();
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glMatrixMode(GLenum mode)
{
	SERIALISE_ELEMENT(GLenum, Mode, mode);

	if(m_State <= EXECUTING)
		m_Real.glMatrixMode(Mode);

	return true;
}

void WrappedOpenGL::glMatrixMode(GLenum mode)
{
	m_Real.glMatrixMode(mode);
	
	{
		SCOPED_SERIALISE_CONTEXT(MATRIXMODE);
		Serialise_glMatrixMode(mode);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glLoadIdentity()
{
	if(m_State <= EXECUTING)
		m_Real.glLoadIdentity();

	return true;
}

void WrappedOpenGL::glLoadIdentity()
{
	m_Real.glLoadIdentity();
	
	{
		SCOPED_SERIALISE_CONTEXT(LOADIDENTITY);
		Serialise_glLoadIdentity();
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	SERIALISE_ELEMENT(double, L, left);
	SERIALISE_ELEMENT(double, R, right);
	SERIALISE_ELEMENT(double, B, bottom);
	SERIALISE_ELEMENT(double, T, top);
	SERIALISE_ELEMENT(double, N, zNear);
	SERIALISE_ELEMENT(double, F, zFar);

	if(m_State <= EXECUTING)
		m_Real.glFrustum(L, R, B, T, N, F);

	return true;
}

void WrappedOpenGL::glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	m_Real.glFrustum(left, right, bottom, top, zNear, zFar);
	
	{
		SCOPED_SERIALISE_CONTEXT(FRUSTUM);
		Serialise_glFrustum(left, right, bottom, top, zNear, zFar);

		m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	SERIALISE_ELEMENT(float, X, x);
	SERIALISE_ELEMENT(float, Y, y);
	SERIALISE_ELEMENT(float, Z, z);

	if(m_State <= EXECUTING)
		m_Real.glTranslatef(X, Y, Z);

	return true;
}

void WrappedOpenGL::glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	m_Real.glTranslatef(x, y, z);
	
	{
		SCOPED_SERIALISE_CONTEXT(TRANSLATEF);
		Serialise_glTranslatef(x, y, z);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else if(m_State == WRITING_CAPFRAME)
			m_ContextRecord->AddChunk(scope.Get());
		else
			m_DeviceRecord->AddChunk(scope.Get());
	}
}

bool WrappedOpenGL::Serialise_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	SERIALISE_ELEMENT(float, ang, angle);
	SERIALISE_ELEMENT(float, X, x);
	SERIALISE_ELEMENT(float, Y, y);
	SERIALISE_ELEMENT(float, Z, z);

	if(m_State <= EXECUTING)
		m_Real.glRotatef(ang, X, Y, Z);

	return true;
}

void WrappedOpenGL::glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	m_Real.glRotatef(angle, x, y, z);
	
	if(m_State == WRITING_CAPFRAME || m_DisplayListRecord)
	{
		SCOPED_SERIALISE_CONTEXT(ROTATEF);
		Serialise_glRotatef(angle, x, y, z);
		
		if(m_DisplayListRecord)
			m_DisplayListRecord->AddChunk(scope.Get());
		else
			m_ContextRecord->AddChunk(scope.Get());
	}
}

#pragma endregion


