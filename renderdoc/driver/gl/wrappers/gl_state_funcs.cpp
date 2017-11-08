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
#include "strings/string_utils.h"

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendFunc(SerialiserType &ser, GLenum sfactor, GLenum dfactor)
{
  SERIALISE_ELEMENT(sfactor);
  SERIALISE_ELEMENT(dfactor);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendFunc(sfactor, dfactor);
  }

  return true;
}

void WrappedOpenGL::glBlendFunc(GLenum sfactor, GLenum dfactor)
{
  m_Real.glBlendFunc(sfactor, dfactor);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFunc(ser, sfactor, dfactor);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendFunci(SerialiserType &ser, GLuint buf, GLenum src, GLenum dst)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(src);
  SERIALISE_ELEMENT(dst);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendFunci(buf, src, dst);
  }

  return true;
}

void WrappedOpenGL::glBlendFunci(GLuint buf, GLenum src, GLenum dst)
{
  m_Real.glBlendFunci(buf, src, dst);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFunci(ser, buf, src, dst);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendColor(SerialiserType &ser, GLfloat red, GLfloat green,
                                           GLfloat blue, GLfloat alpha)
{
  SERIALISE_ELEMENT(red);
  SERIALISE_ELEMENT(green);
  SERIALISE_ELEMENT(blue);
  SERIALISE_ELEMENT(alpha);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendColor(red, green, blue, alpha);
  }

  return true;
}

void WrappedOpenGL::glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  m_Real.glBlendColor(red, green, blue, alpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendColor(ser, red, green, blue, alpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendFuncSeparate(SerialiserType &ser, GLenum sfactorRGB,
                                                  GLenum dfactorRGB, GLenum sfactorAlpha,
                                                  GLenum dfactorAlpha)
{
  SERIALISE_ELEMENT(sfactorRGB);
  SERIALISE_ELEMENT(dfactorRGB);
  SERIALISE_ELEMENT(sfactorAlpha);
  SERIALISE_ELEMENT(dfactorAlpha);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha,
                                        GLenum dfactorAlpha)
{
  m_Real.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFuncSeparate(ser, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendFuncSeparatei(SerialiserType &ser, GLuint buf,
                                                   GLenum sfactorRGB, GLenum dfactorRGB,
                                                   GLenum sfactorAlpha, GLenum dfactorAlpha)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(sfactorRGB);
  SERIALISE_ELEMENT(dfactorRGB);
  SERIALISE_ELEMENT(sfactorAlpha);
  SERIALISE_ELEMENT(dfactorAlpha);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB,
                                         GLenum sfactorAlpha, GLenum dfactorAlpha)
{
  m_Real.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFuncSeparatei(ser, buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquation(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendEquation(mode);
  }

  return true;
}

void WrappedOpenGL::glBlendEquation(GLenum mode)
{
  m_Real.glBlendEquation(mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquation(ser, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquationi(SerialiserType &ser, GLuint buf, GLenum mode)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendEquationi(buf, mode);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationi(GLuint buf, GLenum mode)
{
  m_Real.glBlendEquationi(buf, mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquationi(ser, buf, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquationSeparate(SerialiserType &ser, GLenum modeRGB,
                                                      GLenum modeAlpha)
{
  SERIALISE_ELEMENT(modeRGB);
  SERIALISE_ELEMENT(modeAlpha);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendEquationSeparate(modeRGB, modeAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
  m_Real.glBlendEquationSeparate(modeRGB, modeAlpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquationSeparate(ser, modeRGB, modeAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquationSeparatei(SerialiserType &ser, GLuint buf,
                                                       GLenum modeRGB, GLenum modeAlpha)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(modeRGB);
  SERIALISE_ELEMENT(modeAlpha);

  if(IsReplayingAndReading())
  {
    m_Real.glBlendEquationSeparatei(buf, modeRGB, modeAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
  m_Real.glBlendEquationSeparatei(buf, modeRGB, modeAlpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquationSeparatei(ser, buf, modeRGB, modeAlpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendBarrierKHR(SerialiserType &ser)
{
  if(IsReplayingAndReading())
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

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendBarrierKHR(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glBlendBarrier()
{
  CoherentMapImplicitBarrier();

  m_Real.glBlendBarrier();

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendBarrierKHR(ser);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glLogicOp(SerialiserType &ser, GLenum opcode)
{
  SERIALISE_ELEMENT(opcode);

  if(IsReplayingAndReading())
  {
    m_Real.glLogicOp(opcode);
  }

  return true;
}

void WrappedOpenGL::glLogicOp(GLenum opcode)
{
  m_Real.glLogicOp(opcode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glLogicOp(ser, opcode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilFunc(SerialiserType &ser, GLenum func, GLint ref, GLuint mask)
{
  SERIALISE_ELEMENT(func);
  SERIALISE_ELEMENT(ref);
  SERIALISE_ELEMENT(mask);

  if(IsReplayingAndReading())
  {
    m_Real.glStencilFunc(func, ref, mask);
  }

  return true;
}

void WrappedOpenGL::glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
  m_Real.glStencilFunc(func, ref, mask);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilFunc(ser, func, ref, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilFuncSeparate(SerialiserType &ser, GLenum face, GLenum func,
                                                    GLint ref, GLuint mask)
{
  SERIALISE_ELEMENT(face);
  SERIALISE_ELEMENT(func);
  SERIALISE_ELEMENT(ref);
  SERIALISE_ELEMENT(mask);

  if(IsReplayingAndReading())
  {
    m_Real.glStencilFuncSeparate(face, func, ref, mask);
  }

  return true;
}

void WrappedOpenGL::glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
  m_Real.glStencilFuncSeparate(face, func, ref, mask);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilFuncSeparate(ser, face, func, ref, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilMask(SerialiserType &ser, GLuint mask)
{
  SERIALISE_ELEMENT(mask);

  if(IsReplayingAndReading())
  {
    m_Real.glStencilMask(mask);
  }

  return true;
}

void WrappedOpenGL::glStencilMask(GLuint mask)
{
  m_Real.glStencilMask(mask);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilMask(ser, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilMaskSeparate(SerialiserType &ser, GLenum face, GLuint mask)
{
  SERIALISE_ELEMENT(face);
  SERIALISE_ELEMENT(mask);

  if(IsReplayingAndReading())
  {
    m_Real.glStencilMaskSeparate(face, mask);
  }

  return true;
}

void WrappedOpenGL::glStencilMaskSeparate(GLenum face, GLuint mask)
{
  m_Real.glStencilMaskSeparate(face, mask);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilMaskSeparate(ser, face, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilOp(SerialiserType &ser, GLenum fail, GLenum zfail, GLenum zpass)
{
  SERIALISE_ELEMENT(fail);
  SERIALISE_ELEMENT(zfail);
  SERIALISE_ELEMENT(zpass);

  if(IsReplayingAndReading())
  {
    m_Real.glStencilOp(fail, zfail, zpass);
  }

  return true;
}

void WrappedOpenGL::glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
  m_Real.glStencilOp(fail, zfail, zpass);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilOp(ser, fail, zfail, zpass);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilOpSeparate(SerialiserType &ser, GLenum face, GLenum sfail,
                                                  GLenum dpfail, GLenum dppass)
{
  SERIALISE_ELEMENT(face);
  SERIALISE_ELEMENT(sfail);
  SERIALISE_ELEMENT(dpfail);
  SERIALISE_ELEMENT(dppass);

  if(IsReplayingAndReading())
  {
    m_Real.glStencilOpSeparate(face, sfail, dpfail, dppass);
  }

  return true;
}

void WrappedOpenGL::glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
  m_Real.glStencilOpSeparate(face, sfail, dpfail, dppass);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilOpSeparate(ser, face, sfail, dpfail, dppass);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearColor(SerialiserType &ser, GLclampf red, GLclampf green,
                                           GLclampf blue, GLclampf alpha)
{
  SERIALISE_ELEMENT(red);
  SERIALISE_ELEMENT(green);
  SERIALISE_ELEMENT(blue);
  SERIALISE_ELEMENT(alpha);

  if(IsReplayingAndReading())
  {
    m_Real.glClearColor(red, green, blue, alpha);
  }

  return true;
}

void WrappedOpenGL::glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  m_Real.glClearColor(red, green, blue, alpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearColor(ser, red, green, blue, alpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearStencil(SerialiserType &ser, GLint stencil)
{
  SERIALISE_ELEMENT_TYPED(int32_t, stencil);

  if(IsReplayingAndReading())
  {
    m_Real.glClearStencil(stencil);
  }

  return true;
}

void WrappedOpenGL::glClearStencil(GLint stencil)
{
  m_Real.glClearStencil(stencil);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearStencil(ser, stencil);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearDepth(SerialiserType &ser, GLdouble depth)
{
  SERIALISE_ELEMENT(depth);

  if(IsReplayingAndReading())
  {
    if(IsGLES)
      m_Real.glClearDepthf((float)depth);
    else
      m_Real.glClearDepth(depth);
  }

  return true;
}

void WrappedOpenGL::glClearDepth(GLdouble depth)
{
  m_Real.glClearDepth(depth);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearDepth(ser, depth);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glClearDepthf(GLfloat depth)
{
  m_Real.glClearDepthf(depth);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearDepth(ser, depth);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthFunc(SerialiserType &ser, GLenum func)
{
  SERIALISE_ELEMENT(func);

  if(IsReplayingAndReading())
  {
    m_Real.glDepthFunc(func);
  }

  return true;
}

void WrappedOpenGL::glDepthFunc(GLenum func)
{
  m_Real.glDepthFunc(func);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthFunc(ser, func);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthMask(SerialiserType &ser, GLboolean flag)
{
  SERIALISE_ELEMENT_TYPED(bool, flag);

  if(IsReplayingAndReading())
  {
    m_Real.glDepthMask(flag ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glDepthMask(GLboolean flag)
{
  m_Real.glDepthMask(flag);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthMask(ser, flag);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRange(SerialiserType &ser, GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  if(IsReplayingAndReading())
    m_Real.glDepthRange(nearVal, farVal);

  return true;
}

void WrappedOpenGL::glDepthRange(GLdouble nearVal, GLdouble farVal)
{
  m_Real.glDepthRange(nearVal, farVal);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRange(ser, nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRangef(SerialiserType &ser, GLfloat nearVal, GLfloat farVal)
{
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  if(IsReplayingAndReading())
    m_Real.glDepthRangef(nearVal, farVal);

  return true;
}

void WrappedOpenGL::glDepthRangef(GLfloat nearVal, GLfloat farVal)
{
  m_Real.glDepthRangef(nearVal, farVal);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangef(ser, nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRangeIndexed(SerialiserType &ser, GLuint index,
                                                  GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  if(IsReplayingAndReading())
  {
    if(IsGLES)
      m_Real.glDepthRangeIndexedfOES(index, (GLfloat)nearVal, (GLfloat)farVal);
    else
      m_Real.glDepthRangeIndexed(index, nearVal, farVal);
  }

  return true;
}

void WrappedOpenGL::glDepthRangeIndexed(GLuint index, GLdouble nearVal, GLdouble farVal)
{
  m_Real.glDepthRangeIndexed(index, nearVal, farVal);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeIndexed(ser, index, nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDepthRangeIndexedfOES(GLuint index, GLfloat nearVal, GLfloat farVal)
{
  m_Real.glDepthRangeIndexedfOES(index, nearVal, farVal);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeIndexed(ser, index, (GLdouble)nearVal, (GLdouble)farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRangeArrayv(SerialiserType &ser, GLuint first, GLsizei count,
                                                 const GLdouble *v)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  uint32_t numValues = count * 2;
  SERIALISE_ELEMENT_ARRAY(v, numValues);

  if(IsReplayingAndReading())
  {
    if(IsGLES)
    {
      GLfloat *fv = new GLfloat[numValues];
      for(uint32_t i = 0; i < numValues; ++i)
        fv[i] = (GLfloat)v[i];

      m_Real.glDepthRangeArrayfvOES(first, count, fv);

      delete[] fv;
    }
    else
    {
      m_Real.glDepthRangeArrayv(first, count, v);
    }
  }

  return true;
}

void WrappedOpenGL::glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v)
{
  m_Real.glDepthRangeArrayv(first, count, v);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeArrayv(ser, first, count, v);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDepthRangeArrayfvOES(GLuint first, GLsizei count, const GLfloat *v)
{
  m_Real.glDepthRangeArrayfvOES(first, count, v);

  if(IsActiveCapturing(m_State))
  {
    GLdouble *dv = new GLdouble[count * 2];
    for(GLsizei i = 0; i < count * 2; ++i)
      dv[i] = v[i];

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeArrayv(ser, first, count, dv);

    delete[] dv;

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthBoundsEXT(SerialiserType &ser, GLclampd nearVal, GLclampd farVal)
{
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  if(IsReplayingAndReading())
  {
    m_Real.glDepthBoundsEXT(nearVal, farVal);
  }

  return true;
}

void WrappedOpenGL::glDepthBoundsEXT(GLclampd nearVal, GLclampd farVal)
{
  m_Real.glDepthBoundsEXT(nearVal, farVal);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthBoundsEXT(ser, nearVal, farVal);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClipControl(SerialiserType &ser, GLenum origin, GLenum depth)
{
  SERIALISE_ELEMENT(origin);
  SERIALISE_ELEMENT(depth);

  if(IsReplayingAndReading())
  {
    m_Real.glClipControl(origin, depth);
  }

  return true;
}

void WrappedOpenGL::glClipControl(GLenum origin, GLenum depth)
{
  m_Real.glClipControl(origin, depth);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClipControl(ser, origin, depth);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glProvokingVertex(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glProvokingVertex(mode);
  }

  return true;
}

void WrappedOpenGL::glProvokingVertex(GLenum mode)
{
  m_Real.glProvokingVertex(mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glProvokingVertex(ser, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPrimitiveRestartIndex(SerialiserType &ser, GLuint index)
{
  SERIALISE_ELEMENT(index);

  if(IsReplayingAndReading())
  {
    m_Real.glPrimitiveRestartIndex(index);
  }

  return true;
}

void WrappedOpenGL::glPrimitiveRestartIndex(GLuint index)
{
  m_Real.glPrimitiveRestartIndex(index);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPrimitiveRestartIndex(ser, index);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDisable(SerialiserType &ser, GLenum cap)
{
  SERIALISE_ELEMENT(cap);

  if(IsReplayingAndReading())
  {
    m_Real.glDisable(cap);
  }

  return true;
}

void WrappedOpenGL::glDisable(GLenum cap)
{
  m_Real.glDisable(cap);

  if(IsActiveCapturing(m_State))
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

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDisable(ser, cap);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEnable(SerialiserType &ser, GLenum cap)
{
  SERIALISE_ELEMENT(cap);

  if(IsReplayingAndReading())
  {
    m_Real.glEnable(cap);
  }

  return true;
}

void WrappedOpenGL::glEnable(GLenum cap)
{
  m_Real.glEnable(cap);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEnable(ser, cap);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDisablei(SerialiserType &ser, GLenum cap, GLuint index)
{
  SERIALISE_ELEMENT(cap);
  SERIALISE_ELEMENT(index);

  if(IsReplayingAndReading())
  {
    m_Real.glDisablei(cap, index);
  }

  return true;
}

void WrappedOpenGL::glDisablei(GLenum cap, GLuint index)
{
  m_Real.glDisablei(cap, index);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDisablei(ser, cap, index);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEnablei(SerialiserType &ser, GLenum cap, GLuint index)
{
  SERIALISE_ELEMENT(cap);
  SERIALISE_ELEMENT(index);

  if(IsReplayingAndReading())
  {
    m_Real.glEnablei(cap, index);
  }

  return true;
}

void WrappedOpenGL::glEnablei(GLenum cap, GLuint index)
{
  m_Real.glEnablei(cap, index);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEnablei(ser, cap, index);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFrontFace(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glFrontFace(mode);
  }

  return true;
}

void WrappedOpenGL::glFrontFace(GLenum mode)
{
  m_Real.glFrontFace(mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFrontFace(ser, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCullFace(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glCullFace(mode);
  }

  return true;
}

void WrappedOpenGL::glCullFace(GLenum mode)
{
  m_Real.glCullFace(mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCullFace(ser, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glHint(SerialiserType &ser, GLenum target, GLenum mode)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glHint(target, mode);
  }

  return true;
}

void WrappedOpenGL::glHint(GLenum target, GLenum mode)
{
  m_Real.glHint(target, mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glHint(ser, target, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glColorMask(SerialiserType &ser, GLboolean red, GLboolean green,
                                          GLboolean blue, GLboolean alpha)
{
  SERIALISE_ELEMENT_TYPED(bool, red);
  SERIALISE_ELEMENT_TYPED(bool, green);
  SERIALISE_ELEMENT_TYPED(bool, blue);
  SERIALISE_ELEMENT_TYPED(bool, alpha);

  if(IsReplayingAndReading())
  {
    m_Real.glColorMask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
                       blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
  m_Real.glColorMask(red, green, blue, alpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glColorMask(ser, red, green, blue, alpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glColorMaski(SerialiserType &ser, GLuint buf, GLboolean red,
                                           GLboolean green, GLboolean blue, GLboolean alpha)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT_TYPED(bool, red);
  SERIALISE_ELEMENT_TYPED(bool, green);
  SERIALISE_ELEMENT_TYPED(bool, blue);
  SERIALISE_ELEMENT_TYPED(bool, alpha);

  if(IsReplayingAndReading())
  {
    m_Real.glColorMaski(buf, red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
                        blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue,
                                 GLboolean alpha)
{
  m_Real.glColorMaski(buf, red, green, blue, alpha);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glColorMaski(ser, buf, red, green, blue, alpha);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSampleMaski(SerialiserType &ser, GLuint maskNumber, GLbitfield mask)
{
  SERIALISE_ELEMENT(maskNumber);
  SERIALISE_ELEMENT(mask);

  if(IsReplayingAndReading())
  {
    m_Real.glSampleMaski(maskNumber, mask);
  }

  return true;
}

void WrappedOpenGL::glSampleMaski(GLuint maskNumber, GLbitfield mask)
{
  m_Real.glSampleMaski(maskNumber, mask);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSampleMaski(ser, maskNumber, mask);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSampleCoverage(SerialiserType &ser, GLfloat value, GLboolean invert)
{
  SERIALISE_ELEMENT(value);
  SERIALISE_ELEMENT_TYPED(bool, invert);

  if(IsReplayingAndReading())
  {
    m_Real.glSampleCoverage(value, invert ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glSampleCoverage(GLfloat value, GLboolean invert)
{
  m_Real.glSampleCoverage(value, invert);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSampleCoverage(ser, value, invert);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMinSampleShading(SerialiserType &ser, GLfloat value)
{
  SERIALISE_ELEMENT(value);

  if(IsReplayingAndReading())
  {
    m_Real.glMinSampleShading(value);
  }

  return true;
}

void WrappedOpenGL::glMinSampleShading(GLfloat value)
{
  m_Real.glMinSampleShading(value);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMinSampleShading(ser, value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glRasterSamplesEXT(SerialiserType &ser, GLuint samples,
                                                 GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT_TYPED(bool, fixedsamplelocations);

  if(IsReplayingAndReading())
  {
    m_Real.glRasterSamplesEXT(samples, fixedsamplelocations ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glRasterSamplesEXT(GLuint samples, GLboolean fixedsamplelocations)
{
  m_Real.glRasterSamplesEXT(samples, fixedsamplelocations);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glRasterSamplesEXT(ser, samples, fixedsamplelocations);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPatchParameteri(SerialiserType &ser, GLenum pname, GLint value)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(value);

  if(IsReplayingAndReading())
  {
    m_Real.glPatchParameteri(pname, value);
  }

  return true;
}

void WrappedOpenGL::glPatchParameteri(GLenum pname, GLint value)
{
  m_Real.glPatchParameteri(pname, value);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPatchParameteri(ser, pname, value);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPatchParameterfv(SerialiserType &ser, GLenum pname,
                                                 const GLfloat *values)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(values, FIXED_COUNT(pname == eGL_PATCH_DEFAULT_OUTER_LEVEL ? 4U : 2U));

  if(IsReplayingAndReading())
  {
    m_Real.glPatchParameterfv(pname, values);
  }

  return true;
}

void WrappedOpenGL::glPatchParameterfv(GLenum pname, const GLfloat *values)
{
  m_Real.glPatchParameterfv(pname, values);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPatchParameterfv(ser, pname, values);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glLineWidth(SerialiserType &ser, GLfloat width)
{
  SERIALISE_ELEMENT(width);

  if(IsReplayingAndReading())
  {
    m_Real.glLineWidth(width);
  }

  return true;
}

void WrappedOpenGL::glLineWidth(GLfloat width)
{
  m_Real.glLineWidth(width);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glLineWidth(ser, width);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointSize(SerialiserType &ser, GLfloat size)
{
  SERIALISE_ELEMENT(size);

  if(IsReplayingAndReading())
  {
    m_Real.glPointSize(size);
  }

  return true;
}

void WrappedOpenGL::glPointSize(GLfloat size)
{
  m_Real.glPointSize(size);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointSize(ser, size);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameteri(SerialiserType &ser, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(pname);

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(pname == GL_POINT_SPRITE_COORD_ORIGIN)
  {
    SERIALISE_ELEMENT_TYPED(GLenum, param);
  }
  else
  {
    SERIALISE_ELEMENT(param);
  }

  if(IsReplayingAndReading())
  {
    m_Real.glPointParameteri(pname, param);
  }

  return true;
}

void WrappedOpenGL::glPointParameteri(GLenum pname, GLint param)
{
  m_Real.glPointParameteri(pname, param);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameteri(ser, pname, param);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameteriv(SerialiserType &ser, GLenum pname,
                                                 const GLint *params)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_LOCAL(Param, *params);

  if(IsReplayingAndReading())
  {
    m_Real.glPointParameteriv(pname, &Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameteriv(GLenum pname, const GLint *params)
{
  m_Real.glPointParameteriv(pname, params);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameteriv(ser, pname, params);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameterf(SerialiserType &ser, GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  if(IsReplayingAndReading())
  {
    m_Real.glPointParameterf(pname, param);
  }

  return true;
}

void WrappedOpenGL::glPointParameterf(GLenum pname, GLfloat param)
{
  m_Real.glPointParameterf(pname, param);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameterf(ser, pname, param);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameterfv(SerialiserType &ser, GLenum pname,
                                                 const GLfloat *params)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_LOCAL(Param, *params);

  if(IsReplayingAndReading())
  {
    m_Real.glPointParameterfv(pname, &Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameterfv(GLenum pname, const GLfloat *params)
{
  m_Real.glPointParameterfv(pname, params);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameterfv(ser, pname, params);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glViewport(SerialiserType &ser, GLint x, GLint y, GLsizei width,
                                         GLsizei height)
{
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  if(IsReplayingAndReading())
  {
    m_Real.glViewport(x, y, width, height);
  }

  return true;
}

void WrappedOpenGL::glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
  m_Real.glViewport(x, y, width, height);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glViewport(ser, x, y, width, height);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glViewportArrayv(SerialiserType &ser, GLuint index, GLuint count,
                                               const GLfloat *v)
{
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(count);
  uint32_t numValues = count * 4;
  SERIALISE_ELEMENT_ARRAY(v, numValues);

  if(IsReplayingAndReading())
  {
    m_Real.glViewportArrayv(index, count, v);
  }

  return true;
}

void WrappedOpenGL::glViewportArrayv(GLuint index, GLuint count, const GLfloat *v)
{
  m_Real.glViewportArrayv(index, count, v);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glViewportArrayv(ser, index, count, v);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glScissor(SerialiserType &ser, GLint x, GLint y, GLsizei width,
                                        GLsizei height)
{
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  if(IsReplayingAndReading())
  {
    m_Real.glScissor(x, y, width, height);
  }

  return true;
}

void WrappedOpenGL::glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
  m_Real.glScissor(x, y, width, height);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glScissor(ser, x, y, width, height);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glScissorArrayv(SerialiserType &ser, GLuint first, GLsizei count,
                                              const GLint *v)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  uint32_t numValues = count * 4;
  SERIALISE_ELEMENT_ARRAY(v, numValues);

  if(IsReplayingAndReading())
  {
    m_Real.glScissorArrayv(first, count, v);
  }

  return true;
}

void WrappedOpenGL::glScissorArrayv(GLuint first, GLsizei count, const GLint *v)
{
  m_Real.glScissorArrayv(first, count, v);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glScissorArrayv(ser, first, count, v);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPolygonMode(SerialiserType &ser, GLenum face, GLenum mode)
{
  SERIALISE_ELEMENT(face);
  SERIALISE_ELEMENT(mode);

  if(IsReplayingAndReading())
  {
    m_Real.glPolygonMode(face, mode);
  }

  return true;
}

void WrappedOpenGL::glPolygonMode(GLenum face, GLenum mode)
{
  m_Real.glPolygonMode(face, mode);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPolygonMode(ser, face, mode);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPolygonOffset(SerialiserType &ser, GLfloat factor, GLfloat units)
{
  SERIALISE_ELEMENT(factor);
  SERIALISE_ELEMENT(units);

  if(IsReplayingAndReading())
  {
    m_Real.glPolygonOffset(factor, units);
  }

  return true;
}

void WrappedOpenGL::glPolygonOffset(GLfloat factor, GLfloat units)
{
  m_Real.glPolygonOffset(factor, units);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPolygonOffset(ser, factor, units);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPolygonOffsetClampEXT(SerialiserType &ser, GLfloat factor,
                                                      GLfloat units, GLfloat clamp)
{
  SERIALISE_ELEMENT(factor);
  SERIALISE_ELEMENT(units);
  SERIALISE_ELEMENT(clamp);

  if(IsReplayingAndReading())
  {
    m_Real.glPolygonOffsetClampEXT(factor, units, clamp);
  }

  return true;
}

void WrappedOpenGL::glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp)
{
  m_Real.glPolygonOffsetClampEXT(factor, units, clamp);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPolygonOffsetClampEXT(ser, factor, units, clamp);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPrimitiveBoundingBox(SerialiserType &ser, GLfloat minX, GLfloat minY,
                                                     GLfloat minZ, GLfloat minW, GLfloat maxX,
                                                     GLfloat maxY, GLfloat maxZ, GLfloat maxW)
{
  SERIALISE_ELEMENT(minX);
  SERIALISE_ELEMENT(minY);
  SERIALISE_ELEMENT(minZ);
  SERIALISE_ELEMENT(minW);
  SERIALISE_ELEMENT(maxX);
  SERIALISE_ELEMENT(maxY);
  SERIALISE_ELEMENT(maxZ);
  SERIALISE_ELEMENT(maxW);

  if(IsReplayingAndReading())
  {
    m_Real.glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
  }

  return true;
}

void WrappedOpenGL::glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                           GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW)
{
  m_Real.glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPrimitiveBoundingBox(ser, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    m_ContextRecord->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, glBlendFunc, GLenum sfactor, GLenum dfactor);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendFunci, GLuint buf, GLenum src, GLenum dst);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendColor, GLfloat red, GLfloat green, GLfloat blue,
                                GLfloat alpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendFuncSeparate, GLenum sfactorRGB, GLenum dfactorRGB,
                                GLenum sfactorAlpha, GLenum dfactorAlpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendFuncSeparatei, GLuint buf, GLenum sfactorRGB,
                                GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendEquation, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendEquationi, GLuint buf, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendEquationSeparate, GLenum modeRGB, GLenum modeAlpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendEquationSeparatei, GLuint buf, GLenum modeRGB,
                                GLenum modeAlpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlendBarrierKHR);
INSTANTIATE_FUNCTION_SERIALISED(void, glLogicOp, GLenum opcode);
INSTANTIATE_FUNCTION_SERIALISED(void, glStencilFunc, GLenum func, GLint ref, GLuint mask);
INSTANTIATE_FUNCTION_SERIALISED(void, glStencilFuncSeparate, GLenum face, GLenum func, GLint ref,
                                GLuint mask);
INSTANTIATE_FUNCTION_SERIALISED(void, glStencilMask, GLuint mask);
INSTANTIATE_FUNCTION_SERIALISED(void, glStencilMaskSeparate, GLenum face, GLuint mask);
INSTANTIATE_FUNCTION_SERIALISED(void, glStencilOp, GLenum fail, GLenum zfail, GLenum zpass);
INSTANTIATE_FUNCTION_SERIALISED(void, glStencilOpSeparate, GLenum face, GLenum sfail, GLenum dpfail,
                                GLenum dppass);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearColor, GLclampf red, GLclampf green, GLclampf blue,
                                GLclampf alpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearStencil, GLint stencil);
INSTANTIATE_FUNCTION_SERIALISED(void, glClearDepth, GLdouble depth);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthFunc, GLenum func);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthMask, GLboolean flag);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthRange, GLdouble nearVal, GLdouble farVal);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthRangef, GLfloat nearVal, GLfloat farVal);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthRangeIndexed, GLuint index, GLdouble nearVal,
                                GLdouble farVal);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthRangeArrayv, GLuint first, GLsizei count,
                                const GLdouble *v);
INSTANTIATE_FUNCTION_SERIALISED(void, glDepthBoundsEXT, GLclampd nearVal, GLclampd farVal);
INSTANTIATE_FUNCTION_SERIALISED(void, glClipControl, GLenum origin, GLenum depth);
INSTANTIATE_FUNCTION_SERIALISED(void, glProvokingVertex, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glPrimitiveRestartIndex, GLuint index);
INSTANTIATE_FUNCTION_SERIALISED(void, glDisable, GLenum cap);
INSTANTIATE_FUNCTION_SERIALISED(void, glEnable, GLenum cap);
INSTANTIATE_FUNCTION_SERIALISED(void, glDisablei, GLenum cap, GLuint index);
INSTANTIATE_FUNCTION_SERIALISED(void, glEnablei, GLenum cap, GLuint index);
INSTANTIATE_FUNCTION_SERIALISED(void, glFrontFace, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glCullFace, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glHint, GLenum target, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glColorMask, GLboolean red, GLboolean green, GLboolean blue,
                                GLboolean alpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glColorMaski, GLuint buf, GLboolean red, GLboolean green,
                                GLboolean blue, GLboolean alpha);
INSTANTIATE_FUNCTION_SERIALISED(void, glSampleMaski, GLuint maskNumber, GLbitfield mask);
INSTANTIATE_FUNCTION_SERIALISED(void, glSampleCoverage, GLfloat value, GLboolean invert);
INSTANTIATE_FUNCTION_SERIALISED(void, glMinSampleShading, GLfloat value);
INSTANTIATE_FUNCTION_SERIALISED(void, glRasterSamplesEXT, GLuint samples,
                                GLboolean fixedsamplelocations);
INSTANTIATE_FUNCTION_SERIALISED(void, glPatchParameteri, GLenum pname, GLint value);
INSTANTIATE_FUNCTION_SERIALISED(void, glPatchParameterfv, GLenum pname, const GLfloat *values);
INSTANTIATE_FUNCTION_SERIALISED(void, glLineWidth, GLfloat width);
INSTANTIATE_FUNCTION_SERIALISED(void, glPointSize, GLfloat size);
INSTANTIATE_FUNCTION_SERIALISED(void, glPointParameteri, GLenum pname, GLint param);
INSTANTIATE_FUNCTION_SERIALISED(void, glPointParameteriv, GLenum pname, const GLint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glPointParameterf, GLenum pname, GLfloat param);
INSTANTIATE_FUNCTION_SERIALISED(void, glPointParameterfv, GLenum pname, const GLfloat *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glViewportArrayv, GLuint index, GLuint count, const GLfloat *v);
INSTANTIATE_FUNCTION_SERIALISED(void, glScissor, GLint x, GLint y, GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glScissorArrayv, GLuint first, GLsizei count, const GLint *v);
INSTANTIATE_FUNCTION_SERIALISED(void, glPolygonMode, GLenum face, GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glPolygonOffset, GLfloat factor, GLfloat units);
INSTANTIATE_FUNCTION_SERIALISED(void, glPolygonOffsetClampEXT, GLfloat factor, GLfloat units,
                                GLfloat clamp);
INSTANTIATE_FUNCTION_SERIALISED(void, glPrimitiveBoundingBox, GLfloat minX, GLfloat minY,
                                GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY,
                                GLfloat maxZ, GLfloat maxW);
