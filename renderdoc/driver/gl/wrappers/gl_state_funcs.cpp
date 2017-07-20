/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "../gl_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"

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

  if(m_State == WRITING_CAPFRAME)
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

  if(m_State == WRITING_CAPFRAME)
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_COLOR);
    Serialise_glBlendColor(red, green, blue, alpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB,
                                                  GLenum sfactorAlpha, GLenum dfactorAlpha)
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

void WrappedOpenGL::glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha,
                                        GLenum dfactorAlpha)
{
  m_Real.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_FUNC_SEP);
    Serialise_glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB,
                                                   GLenum sfactorAlpha, GLenum dfactorAlpha)
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

void WrappedOpenGL::glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB,
                                         GLenum sfactorAlpha, GLenum dfactorAlpha)
{
  m_Real.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_FUNC_SEPI);
    Serialise_glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBlendEquation(GLenum mode)
{
  SERIALISE_ELEMENT(GLenum, m, mode);

  if(m_State <= EXECUTING)
  {
    m_Real.glBlendEquation(m);
  }

  return true;
}

void WrappedOpenGL::glBlendEquation(GLenum mode)
{
  m_Real.glBlendEquation(mode);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_EQ);
    Serialise_glBlendEquation(mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBlendEquationi(GLuint buf, GLenum mode)
{
  SERIALISE_ELEMENT(uint32_t, b, buf);
  SERIALISE_ELEMENT(GLenum, m, mode);

  if(m_State <= EXECUTING)
  {
    m_Real.glBlendEquationi(b, m);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationi(GLuint buf, GLenum mode)
{
  m_Real.glBlendEquationi(buf, mode);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_EQI);
    Serialise_glBlendEquationi(buf, mode);

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

  if(m_State == WRITING_CAPFRAME)
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

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_EQ_SEPI);
    Serialise_glBlendEquationSeparatei(buf, modeRGB, modeAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glBlendBarrierKHR()
{
  if(m_State <= EXECUTING)
  {
    if(IsGLES && m_Real.glBlendBarrier)
      m_Real.glBlendBarrier();
    else
      m_Real.glBlendBarrierKHR();
  }

  return true;
}

void WrappedOpenGL::glBlendBarrierKHR()
{
  CoherentMapImplicitBarrier();

  m_Real.glBlendBarrierKHR();

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_BARRIER);
    Serialise_glBlendBarrierKHR();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glBlendBarrier()
{
  CoherentMapImplicitBarrier();

  m_Real.glBlendBarrier();

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLEND_BARRIER);
    Serialise_glBlendBarrierKHR();

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glLogicOp(GLenum opcode)
{
  SERIALISE_ELEMENT(GLenum, Op, opcode);

  if(m_State <= EXECUTING)
  {
    m_Real.glLogicOp(Op);
  }

  return true;
}

void WrappedOpenGL::glLogicOp(GLenum opcode)
{
  m_Real.glLogicOp(opcode);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(LOGIC_OP);
    Serialise_glLogicOp(opcode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
  SERIALISE_ELEMENT(GLenum, f, func);
  SERIALISE_ELEMENT(int32_t, Ref, ref);
  SERIALISE_ELEMENT(uint32_t, Mask, mask);

  if(m_State <= EXECUTING)
  {
    m_Real.glStencilFunc(f, Ref, Mask);
  }

  return true;
}

void WrappedOpenGL::glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
  m_Real.glStencilFunc(func, ref, mask);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(STENCIL_FUNC);
    Serialise_glStencilFunc(func, ref, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
  SERIALISE_ELEMENT(GLenum, Face, face);
  SERIALISE_ELEMENT(GLenum, f, func);
  SERIALISE_ELEMENT(int32_t, Ref, ref);
  SERIALISE_ELEMENT(uint32_t, Mask, mask);

  if(m_State <= EXECUTING)
  {
    m_Real.glStencilFuncSeparate(Face, f, Ref, Mask);
  }

  return true;
}

void WrappedOpenGL::glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
  m_Real.glStencilFuncSeparate(face, func, ref, mask);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(STENCIL_FUNC_SEP);
    Serialise_glStencilFuncSeparate(face, func, ref, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glStencilMask(GLuint mask)
{
  SERIALISE_ELEMENT(uint32_t, Mask, mask);

  if(m_State <= EXECUTING)
  {
    m_Real.glStencilMask(Mask);
  }

  return true;
}

void WrappedOpenGL::glStencilMask(GLuint mask)
{
  m_Real.glStencilMask(mask);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(STENCIL_MASK);
    Serialise_glStencilMask(mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glStencilMaskSeparate(GLenum face, GLuint mask)
{
  SERIALISE_ELEMENT(GLenum, Face, face);
  SERIALISE_ELEMENT(uint32_t, Mask, mask);

  if(m_State <= EXECUTING)
  {
    m_Real.glStencilMaskSeparate(Face, Mask);
  }

  return true;
}

void WrappedOpenGL::glStencilMaskSeparate(GLenum face, GLuint mask)
{
  m_Real.glStencilMaskSeparate(face, mask);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(STENCIL_MASK_SEP);
    Serialise_glStencilMaskSeparate(face, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
  SERIALISE_ELEMENT(GLenum, f, fail);
  SERIALISE_ELEMENT(GLenum, zf, zfail);
  SERIALISE_ELEMENT(GLenum, p, zpass);

  if(m_State <= EXECUTING)
  {
    m_Real.glStencilOp(f, zf, p);
  }

  return true;
}

void WrappedOpenGL::glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
  m_Real.glStencilOp(fail, zfail, zpass);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(STENCIL_OP);
    Serialise_glStencilOp(fail, zfail, zpass);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail,
                                                  GLenum dppass)
{
  SERIALISE_ELEMENT(GLenum, Face, face);
  SERIALISE_ELEMENT(GLenum, sf, sfail);
  SERIALISE_ELEMENT(GLenum, zf, dpfail);
  SERIALISE_ELEMENT(GLenum, p, dppass);

  if(m_State <= EXECUTING)
  {
    m_Real.glStencilOpSeparate(Face, sf, zf, p);
  }

  return true;
}

void WrappedOpenGL::glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
  m_Real.glStencilOpSeparate(face, sfail, dpfail, dppass);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(STENCIL_OP_SEP);
    Serialise_glStencilOpSeparate(face, sfail, dpfail, dppass);

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

bool WrappedOpenGL::Serialise_glClearStencil(GLint stencil)
{
  SERIALISE_ELEMENT(uint32_t, s, (uint32_t)stencil);

  if(m_State <= EXECUTING)
  {
    m_Real.glClearStencil((GLint)s);
  }

  return true;
}

void WrappedOpenGL::glClearStencil(GLint stencil)
{
  m_Real.glClearStencil(stencil);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_STENCIL);
    Serialise_glClearStencil(stencil);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glClearDepth(GLdouble depth)
{
  SERIALISE_ELEMENT(double, d, depth);

  if(m_State <= EXECUTING)
  {
    if(IsGLES)
      m_Real.glClearDepthf((float)d);
    else
      m_Real.glClearDepth(d);
  }

  return true;
}

void WrappedOpenGL::glClearDepth(GLdouble depth)
{
  m_Real.glClearDepth(depth);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTH);
    Serialise_glClearDepth(depth);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glClearDepthf(GLfloat depth)
{
  m_Real.glClearDepthf(depth);

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
    m_Real.glDepthFunc(f);
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

bool WrappedOpenGL::Serialise_glDepthRange(GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_ELEMENT(GLdouble, n, nearVal);
  SERIALISE_ELEMENT(GLdouble, f, farVal);

  if(m_State <= EXECUTING)
    m_Real.glDepthRange(n, f);

  return true;
}

void WrappedOpenGL::glDepthRange(GLdouble nearVal, GLdouble farVal)
{
  m_Real.glDepthRange(nearVal, farVal);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DEPTH_RANGE);
    Serialise_glDepthRange(nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glDepthRangef(GLfloat nearVal, GLfloat farVal)
{
  SERIALISE_ELEMENT(GLfloat, n, nearVal);
  SERIALISE_ELEMENT(GLfloat, f, farVal);

  if(m_State <= EXECUTING)
    m_Real.glDepthRangef(n, f);

  return true;
}

void WrappedOpenGL::glDepthRangef(GLfloat nearVal, GLfloat farVal)
{
  m_Real.glDepthRangef(nearVal, farVal);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DEPTH_RANGEF);
    Serialise_glDepthRangef(nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glDepthRangeIndexed(GLuint index, GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_ELEMENT(GLuint, i, index);
  SERIALISE_ELEMENT(GLdouble, n, nearVal);
  SERIALISE_ELEMENT(GLdouble, f, farVal);

  if(m_State <= EXECUTING)
    m_Real.glDepthRangeIndexed(i, n, f);

  return true;
}

void WrappedOpenGL::glDepthRangeIndexed(GLuint index, GLdouble nearVal, GLdouble farVal)
{
  m_Real.glDepthRangeIndexed(index, nearVal, farVal);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DEPTH_RANGE_IDX);
    Serialise_glDepthRangeIndexed(index, nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v)
{
  SERIALISE_ELEMENT(uint32_t, idx, first);
  SERIALISE_ELEMENT(uint32_t, cnt, count);
  SERIALISE_ELEMENT_ARR(GLdouble, ranges, v, cnt * 2);

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

bool WrappedOpenGL::Serialise_glDepthBoundsEXT(GLclampd nearVal, GLclampd farVal)
{
  SERIALISE_ELEMENT(GLdouble, n, nearVal);
  SERIALISE_ELEMENT(GLdouble, f, farVal);

  if(m_State <= EXECUTING)
  {
    m_Real.glDepthBoundsEXT(n, f);
  }

  return true;
}

void WrappedOpenGL::glDepthBoundsEXT(GLclampd nearVal, GLclampd farVal)
{
  m_Real.glDepthBoundsEXT(nearVal, farVal);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DEPTH_BOUNDS);
    Serialise_glDepthBoundsEXT(nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glClipControl(GLenum origin, GLenum depth)
{
  SERIALISE_ELEMENT(GLenum, o, origin);
  SERIALISE_ELEMENT(GLenum, d, depth);

  if(m_State <= EXECUTING)
  {
    m_Real.glClipControl(o, d);
  }

  return true;
}

void WrappedOpenGL::glClipControl(GLenum origin, GLenum depth)
{
  m_Real.glClipControl(origin, depth);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(CLIP_CONTROL);
    Serialise_glClipControl(origin, depth);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glProvokingVertex(GLenum mode)
{
  SERIALISE_ELEMENT(GLenum, m, mode);

  if(m_State <= EXECUTING)
  {
    m_Real.glProvokingVertex(m);
  }

  return true;
}

void WrappedOpenGL::glProvokingVertex(GLenum mode)
{
  m_Real.glProvokingVertex(mode);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PROVOKING_VERTEX);
    Serialise_glProvokingVertex(mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPrimitiveRestartIndex(GLuint index)
{
  SERIALISE_ELEMENT(GLuint, i, index);

  if(m_State <= EXECUTING)
  {
    m_Real.glPrimitiveRestartIndex(i);
  }

  return true;
}

void WrappedOpenGL::glPrimitiveRestartIndex(GLuint index)
{
  m_Real.glPrimitiveRestartIndex(index);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PRIMITIVE_RESTART);
    Serialise_glPrimitiveRestartIndex(index);

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
    // Skip some compatibility caps purely for the sake of avoiding debug message spam.
    // We don't explicitly support compatibility, but where it's trivial we try and support it.
    // If these are enabled anywhere in the program/capture then the replay will probably be
    // wrong, but some legacy codebases running compatibility might still disable these.
    // So we don't skip these on glEnable (they will be serialised, and fire an error as
    // appropriate).
    if(cap == 0x0B50)
      return;    // GL_LIGHTING
    if(cap == 0x0BC0)
      return;    // GL_ALPHA_TEST

    SCOPED_SERIALISE_CONTEXT(DISABLE);
    Serialise_glDisable(cap);

    m_ContextRecord->AddChunk(scope.Get());
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

bool WrappedOpenGL::Serialise_glHint(GLenum target, GLenum mode)
{
  SERIALISE_ELEMENT(GLenum, t, target);
  SERIALISE_ELEMENT(GLenum, m, mode);

  if(m_State <= EXECUTING)
  {
    m_Real.glHint(t, m);
  }

  return true;
}

void WrappedOpenGL::glHint(GLenum target, GLenum mode)
{
  m_Real.glHint(target, mode);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(HINT);
    Serialise_glHint(target, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glColorMask(GLboolean red, GLboolean green, GLboolean blue,
                                          GLboolean alpha)
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

bool WrappedOpenGL::Serialise_glColorMaski(GLuint buf, GLboolean red, GLboolean green,
                                           GLboolean blue, GLboolean alpha)
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

void WrappedOpenGL::glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue,
                                 GLboolean alpha)
{
  m_Real.glColorMaski(buf, red, green, blue, alpha);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(COLOR_MASKI);
    Serialise_glColorMaski(buf, red, green, blue, alpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glSampleMaski(GLuint maskNumber, GLbitfield mask)
{
  SERIALISE_ELEMENT(uint32_t, num, maskNumber);
  SERIALISE_ELEMENT(uint32_t, Mask, mask);

  if(m_State <= EXECUTING)
  {
    m_Real.glSampleMaski(num, Mask);
  }

  return true;
}

void WrappedOpenGL::glSampleMaski(GLuint maskNumber, GLbitfield mask)
{
  m_Real.glSampleMaski(maskNumber, mask);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLE_MASK);
    Serialise_glSampleMaski(maskNumber, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glSampleCoverage(GLfloat value, GLboolean invert)
{
  SERIALISE_ELEMENT(float, Value, value);
  SERIALISE_ELEMENT(bool, Invert, invert != 0);

  if(m_State <= EXECUTING)
  {
    m_Real.glSampleCoverage(Value, Invert ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glSampleCoverage(GLfloat value, GLboolean invert)
{
  m_Real.glSampleCoverage(value, invert);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(SAMPLE_COVERAGE);
    Serialise_glSampleCoverage(value, invert);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glMinSampleShading(GLfloat value)
{
  SERIALISE_ELEMENT(float, Value, value);

  if(m_State <= EXECUTING)
  {
    m_Real.glMinSampleShading(Value);
  }

  return true;
}

void WrappedOpenGL::glMinSampleShading(GLfloat value)
{
  m_Real.glMinSampleShading(value);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(MIN_SAMPLE_SHADING);
    Serialise_glMinSampleShading(value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glRasterSamplesEXT(GLuint samples, GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT(uint32_t, s, samples);
  SERIALISE_ELEMENT(bool, f, fixedsamplelocations != 0);

  if(m_State <= EXECUTING)
  {
    m_Real.glRasterSamplesEXT(s, f);
  }

  return true;
}

void WrappedOpenGL::glRasterSamplesEXT(GLuint samples, GLboolean fixedsamplelocations)
{
  m_Real.glRasterSamplesEXT(samples, fixedsamplelocations);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(RASTER_SAMPLES);
    Serialise_glRasterSamplesEXT(samples, fixedsamplelocations);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPatchParameteri(GLenum pname, GLint value)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(int32_t, Value, value);

  if(m_State <= EXECUTING)
  {
    m_Real.glPatchParameteri(PName, Value);
  }

  return true;
}

void WrappedOpenGL::glPatchParameteri(GLenum pname, GLint value)
{
  m_Real.glPatchParameteri(pname, value);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PATCH_PARAMI);
    Serialise_glPatchParameteri(pname, value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPatchParameterfv(GLenum pname, const GLfloat *values)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  const size_t nParams = (PName == eGL_PATCH_DEFAULT_OUTER_LEVEL ? 4U : 2U);
  SERIALISE_ELEMENT_ARR(float, Values, values, nParams);

  if(m_State <= EXECUTING)
  {
    m_Real.glPatchParameterfv(PName, Values);
  }

  delete[] Values;

  return true;
}

void WrappedOpenGL::glPatchParameterfv(GLenum pname, const GLfloat *values)
{
  m_Real.glPatchParameterfv(pname, values);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PATCH_PARAMFV);
    Serialise_glPatchParameterfv(pname, values);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glLineWidth(GLfloat width)
{
  SERIALISE_ELEMENT(GLfloat, w, width);

  if(m_State <= EXECUTING)
  {
    m_Real.glLineWidth(w);
  }

  return true;
}

void WrappedOpenGL::glLineWidth(GLfloat width)
{
  m_Real.glLineWidth(width);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(LINE_WIDTH);
    Serialise_glLineWidth(width);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPointSize(GLfloat size)
{
  SERIALISE_ELEMENT(GLfloat, s, size);

  if(m_State <= EXECUTING)
  {
    m_Real.glPointSize(s);
  }

  return true;
}

void WrappedOpenGL::glPointSize(GLfloat size)
{
  m_Real.glPointSize(size);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(POINT_SIZE);
    Serialise_glPointSize(size);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPointParameteri(GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);

  int32_t ParamValue = 0;

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(PName == GL_POINT_SPRITE_COORD_ORIGIN)
  {
    SERIALISE_ELEMENT(GLenum, Param, (GLenum)param);

    ParamValue = (int32_t)Param;
  }
  else
  {
    SERIALISE_ELEMENT(int32_t, Param, param);

    ParamValue = Param;
  }

  if(m_State <= EXECUTING)
  {
    m_Real.glPointParameteri(PName, ParamValue);
  }

  return true;
}

void WrappedOpenGL::glPointParameteri(GLenum pname, GLint param)
{
  m_Real.glPointParameteri(pname, param);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(POINT_PARAMI);
    Serialise_glPointParameteri(pname, param);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPointParameteriv(GLenum pname, const GLint *params)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(int32_t, Param, *params);

  if(m_State <= EXECUTING)
  {
    m_Real.glPointParameteriv(PName, &Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameteriv(GLenum pname, const GLint *params)
{
  m_Real.glPointParameteriv(pname, params);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(POINT_PARAMIV);
    Serialise_glPointParameteriv(pname, params);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPointParameterf(GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(float, Param, param);

  if(m_State <= EXECUTING)
  {
    m_Real.glPointParameterf(PName, Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameterf(GLenum pname, GLfloat param)
{
  m_Real.glPointParameterf(pname, param);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(POINT_PARAMF);
    Serialise_glPointParameterf(pname, param);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glPointParameterfv(GLenum pname, const GLfloat *params)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(float, Param, *params);

  if(m_State <= EXECUTING)
  {
    m_Real.glPointParameterfv(PName, &Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameterfv(GLenum pname, const GLfloat *params)
{
  m_Real.glPointParameterfv(pname, params);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(POINT_PARAMFV);
    Serialise_glPointParameterfv(pname, params);

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
  SERIALISE_ELEMENT_ARR(GLfloat, views, v, cnt * 4);

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
  const float v[4] = {x, y, w, h};
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
  SERIALISE_ELEMENT_ARR(GLint, scissors, v, cnt * 4);

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

void WrappedOpenGL::glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width,
                                     GLsizei height)
{
  const GLint v[4] = {left, bottom, width, height};
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

bool WrappedOpenGL::Serialise_glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp)
{
  SERIALISE_ELEMENT(float, f, factor);
  SERIALISE_ELEMENT(float, u, units);
  SERIALISE_ELEMENT(float, c, clamp);

  if(m_State <= EXECUTING)
  {
    m_Real.glPolygonOffsetClampEXT(f, u, c);
  }

  return true;
}

void WrappedOpenGL::glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp)
{
  m_Real.glPolygonOffsetClampEXT(factor, units, clamp);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(POLYGON_OFFSET_CLAMP);
    Serialise_glPolygonOffsetClampEXT(factor, units, clamp);

    m_ContextRecord->AddChunk(scope.Get());
  }
}
