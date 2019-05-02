/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendFunc(sfactor, dfactor);
  }

  return true;
}

void WrappedOpenGL::glBlendFunc(GLenum sfactor, GLenum dfactor)
{
  SERIALISE_TIME_CALL(GL.glBlendFunc(sfactor, dfactor));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFunc(ser, sfactor, dfactor);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendFunci(SerialiserType &ser, GLuint buf, GLenum src, GLenum dst)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(src);
  SERIALISE_ELEMENT(dst);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendFunci(buf, src, dst);
  }

  return true;
}

void WrappedOpenGL::glBlendFunci(GLuint buf, GLenum src, GLenum dst)
{
  SERIALISE_TIME_CALL(GL.glBlendFunci(buf, src, dst));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFunci(ser, buf, src, dst);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendColor(red, green, blue, alpha);
  }

  return true;
}

void WrappedOpenGL::glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  SERIALISE_TIME_CALL(GL.glBlendColor(red, green, blue, alpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendColor(ser, red, green, blue, alpha);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha,
                                        GLenum dfactorAlpha)
{
  SERIALISE_TIME_CALL(GL.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFuncSeparate(ser, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB,
                                         GLenum sfactorAlpha, GLenum dfactorAlpha)
{
  SERIALISE_TIME_CALL(
      GL.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendFuncSeparatei(ser, buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquation(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendEquation(mode);
  }

  return true;
}

void WrappedOpenGL::glBlendEquation(GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glBlendEquation(mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquation(ser, mode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquationi(SerialiserType &ser, GLuint buf, GLenum mode)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendEquationi(buf, mode);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationi(GLuint buf, GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glBlendEquationi(buf, mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquationi(ser, buf, mode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquationSeparate(SerialiserType &ser, GLenum modeRGB,
                                                      GLenum modeAlpha)
{
  SERIALISE_ELEMENT(modeRGB);
  SERIALISE_ELEMENT(modeAlpha);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendEquationSeparate(modeRGB, modeAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
  SERIALISE_TIME_CALL(GL.glBlendEquationSeparate(modeRGB, modeAlpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquationSeparate(ser, modeRGB, modeAlpha);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendEquationSeparatei(SerialiserType &ser, GLuint buf,
                                                       GLenum modeRGB, GLenum modeAlpha)
{
  SERIALISE_ELEMENT(buf);
  SERIALISE_ELEMENT(modeRGB);
  SERIALISE_ELEMENT(modeAlpha);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBlendEquationSeparatei(buf, modeRGB, modeAlpha);
  }

  return true;
}

void WrappedOpenGL::glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
  SERIALISE_TIME_CALL(GL.glBlendEquationSeparatei(buf, modeRGB, modeAlpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendEquationSeparatei(ser, buf, modeRGB, modeAlpha);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlendBarrierKHR(SerialiserType &ser)
{
  if(IsReplayingAndReading())
  {
    if(IsGLES && GL.glBlendBarrier)
      GL.glBlendBarrier();
    else
      GL.glBlendBarrierKHR();
  }

  return true;
}

void WrappedOpenGL::glBlendBarrierKHR()
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(GL.glBlendBarrierKHR());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendBarrierKHR(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glBlendBarrier()
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(GL.glBlendBarrier());

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlendBarrierKHR(ser);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glLogicOp(SerialiserType &ser, GLenum opcode)
{
  SERIALISE_ELEMENT(opcode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glLogicOp(opcode);
  }

  return true;
}

void WrappedOpenGL::glLogicOp(GLenum opcode)
{
  SERIALISE_TIME_CALL(GL.glLogicOp(opcode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glLogicOp(ser, opcode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilFunc(SerialiserType &ser, GLenum func, GLint ref, GLuint mask)
{
  SERIALISE_ELEMENT(func);
  SERIALISE_ELEMENT(ref);
  SERIALISE_ELEMENT(mask);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glStencilFunc(func, ref, mask);
  }

  return true;
}

void WrappedOpenGL::glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
  SERIALISE_TIME_CALL(GL.glStencilFunc(func, ref, mask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilFunc(ser, func, ref, mask);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glStencilFuncSeparate(face, func, ref, mask);
  }

  return true;
}

void WrappedOpenGL::glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
  SERIALISE_TIME_CALL(GL.glStencilFuncSeparate(face, func, ref, mask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilFuncSeparate(ser, face, func, ref, mask);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilMask(SerialiserType &ser, GLuint mask)
{
  SERIALISE_ELEMENT(mask);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glStencilMask(mask);
  }

  return true;
}

void WrappedOpenGL::glStencilMask(GLuint mask)
{
  SERIALISE_TIME_CALL(GL.glStencilMask(mask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilMask(ser, mask);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilMaskSeparate(SerialiserType &ser, GLenum face, GLuint mask)
{
  SERIALISE_ELEMENT(face);
  SERIALISE_ELEMENT(mask);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glStencilMaskSeparate(face, mask);
  }

  return true;
}

void WrappedOpenGL::glStencilMaskSeparate(GLenum face, GLuint mask)
{
  SERIALISE_TIME_CALL(GL.glStencilMaskSeparate(face, mask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilMaskSeparate(ser, face, mask);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glStencilOp(SerialiserType &ser, GLenum fail, GLenum zfail, GLenum zpass)
{
  SERIALISE_ELEMENT(fail);
  SERIALISE_ELEMENT(zfail);
  SERIALISE_ELEMENT(zpass);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glStencilOp(fail, zfail, zpass);
  }

  return true;
}

void WrappedOpenGL::glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
  SERIALISE_TIME_CALL(GL.glStencilOp(fail, zfail, zpass));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilOp(ser, fail, zfail, zpass);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glStencilOpSeparate(face, sfail, dpfail, dppass);
  }

  return true;
}

void WrappedOpenGL::glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
  SERIALISE_TIME_CALL(GL.glStencilOpSeparate(face, sfail, dpfail, dppass));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glStencilOpSeparate(ser, face, sfail, dpfail, dppass);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glClearColor(red, green, blue, alpha);
  }

  return true;
}

void WrappedOpenGL::glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  SERIALISE_TIME_CALL(GL.glClearColor(red, green, blue, alpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearColor(ser, red, green, blue, alpha);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearStencil(SerialiserType &ser, GLint stencil)
{
  SERIALISE_ELEMENT_TYPED(int32_t, stencil);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glClearStencil(stencil);
  }

  return true;
}

void WrappedOpenGL::glClearStencil(GLint stencil)
{
  SERIALISE_TIME_CALL(GL.glClearStencil(stencil));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearStencil(ser, stencil);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClearDepth(SerialiserType &ser, GLdouble depth)
{
  SERIALISE_ELEMENT(depth);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsGLES)
      GL.glClearDepthf((float)depth);
    else
      GL.glClearDepth(depth);
  }

  return true;
}

void WrappedOpenGL::glClearDepth(GLdouble depth)
{
  SERIALISE_TIME_CALL(GL.glClearDepth(depth));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearDepth(ser, depth);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glClearDepthf(GLfloat depth)
{
  SERIALISE_TIME_CALL(GL.glClearDepthf(depth));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClearDepth(ser, depth);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthFunc(SerialiserType &ser, GLenum func)
{
  SERIALISE_ELEMENT(func);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glDepthFunc(func);
  }

  return true;
}

void WrappedOpenGL::glDepthFunc(GLenum func)
{
  SERIALISE_TIME_CALL(GL.glDepthFunc(func));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthFunc(ser, func);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthMask(SerialiserType &ser, GLboolean flag)
{
  SERIALISE_ELEMENT_TYPED(bool, flag);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glDepthMask(flag ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glDepthMask(GLboolean flag)
{
  SERIALISE_TIME_CALL(GL.glDepthMask(flag));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthMask(ser, flag);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRange(SerialiserType &ser, GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    GL.glDepthRange(nearVal, farVal);

  return true;
}

void WrappedOpenGL::glDepthRange(GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_TIME_CALL(GL.glDepthRange(nearVal, farVal));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRange(ser, nearVal, farVal);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRangef(SerialiserType &ser, GLfloat nearVal, GLfloat farVal)
{
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    GL.glDepthRangef(nearVal, farVal);

  return true;
}

void WrappedOpenGL::glDepthRangef(GLfloat nearVal, GLfloat farVal)
{
  SERIALISE_TIME_CALL(GL.glDepthRangef(nearVal, farVal));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangef(ser, nearVal, farVal);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRangeIndexed(SerialiserType &ser, GLuint index,
                                                  GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsGLES)
      GL.glDepthRangeIndexedfOES(index, (GLfloat)nearVal, (GLfloat)farVal);
    else
      GL.glDepthRangeIndexed(index, nearVal, farVal);
  }

  return true;
}

void WrappedOpenGL::glDepthRangeIndexed(GLuint index, GLdouble nearVal, GLdouble farVal)
{
  SERIALISE_TIME_CALL(GL.glDepthRangeIndexed(index, nearVal, farVal));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeIndexed(ser, index, nearVal, farVal);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDepthRangeIndexedfOES(GLuint index, GLfloat nearVal, GLfloat farVal)
{
  SERIALISE_TIME_CALL(GL.glDepthRangeIndexedfOES(index, nearVal, farVal));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeIndexed(ser, index, (GLdouble)nearVal, (GLdouble)farVal);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthRangeArrayv(SerialiserType &ser, GLuint first, GLsizei count,
                                                 const GLdouble *v)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(v, count * 2);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsGLES)
    {
      GLfloat *fv = new GLfloat[count * 2];
      for(GLsizei i = 0; i < count * 2; ++i)
        fv[i] = (GLfloat)v[i];

      GL.glDepthRangeArrayfvOES(first, count, fv);

      delete[] fv;
    }
    else
    {
      GL.glDepthRangeArrayv(first, count, v);
    }
  }

  return true;
}

void WrappedOpenGL::glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v)
{
  SERIALISE_TIME_CALL(GL.glDepthRangeArrayv(first, count, v));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeArrayv(ser, first, count, v);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDepthRangeArrayfvOES(GLuint first, GLsizei count, const GLfloat *v)
{
  SERIALISE_TIME_CALL(GL.glDepthRangeArrayfvOES(first, count, v));

  if(IsActiveCapturing(m_State))
  {
    GLdouble *dv = new GLdouble[count * 2];
    for(GLsizei i = 0; i < count * 2; ++i)
      dv[i] = v[i];

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthRangeArrayv(ser, first, count, dv);

    delete[] dv;

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDepthBoundsEXT(SerialiserType &ser, GLclampd nearVal, GLclampd farVal)
{
  SERIALISE_ELEMENT(nearVal);
  SERIALISE_ELEMENT(farVal);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glDepthBoundsEXT);

    GL.glDepthBoundsEXT(nearVal, farVal);
  }

  return true;
}

void WrappedOpenGL::glDepthBoundsEXT(GLclampd nearVal, GLclampd farVal)
{
  SERIALISE_TIME_CALL(GL.glDepthBoundsEXT(nearVal, farVal));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDepthBoundsEXT(ser, nearVal, farVal);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glClipControl(SerialiserType &ser, GLenum origin, GLenum depth)
{
  SERIALISE_ELEMENT(origin);
  SERIALISE_ELEMENT(depth);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glClipControl(origin, depth);
  }

  return true;
}

void WrappedOpenGL::glClipControl(GLenum origin, GLenum depth)
{
  SERIALISE_TIME_CALL(GL.glClipControl(origin, depth));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glClipControl(ser, origin, depth);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glProvokingVertex(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glProvokingVertex(mode);
  }

  return true;
}

void WrappedOpenGL::glProvokingVertex(GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glProvokingVertex(mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glProvokingVertex(ser, mode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPrimitiveRestartIndex(SerialiserType &ser, GLuint index)
{
  SERIALISE_ELEMENT(index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPrimitiveRestartIndex(index);
  }

  return true;
}

void WrappedOpenGL::glPrimitiveRestartIndex(GLuint index)
{
  SERIALISE_TIME_CALL(GL.glPrimitiveRestartIndex(index));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPrimitiveRestartIndex(ser, index);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDisable(SerialiserType &ser, GLenum cap)
{
  SERIALISE_ELEMENT(cap);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glDisable(cap);
  }

  return true;
}

void WrappedOpenGL::glDisable(GLenum cap)
{
  // if we're emulating KHR_debug, skip its caps here
  if(!HasExt[KHR_debug] && (cap == eGL_DEBUG_OUTPUT || cap == eGL_DEBUG_OUTPUT_SYNCHRONOUS))
    return;

  SERIALISE_TIME_CALL(GL.glDisable(cap));

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

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEnable(SerialiserType &ser, GLenum cap)
{
  SERIALISE_ELEMENT(cap);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glEnable(cap);
  }

  return true;
}

void WrappedOpenGL::glEnable(GLenum cap)
{
  // if we're emulating KHR_debug, skip its caps here
  if(!HasExt[KHR_debug] && (cap == eGL_DEBUG_OUTPUT || cap == eGL_DEBUG_OUTPUT_SYNCHRONOUS))
    return;

  SERIALISE_TIME_CALL(GL.glEnable(cap));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEnable(ser, cap);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glDisablei(SerialiserType &ser, GLenum cap, GLuint index)
{
  SERIALISE_ELEMENT(cap);
  SERIALISE_ELEMENT(index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glDisablei(cap, index);
  }

  return true;
}

void WrappedOpenGL::glDisablei(GLenum cap, GLuint index)
{
  SERIALISE_TIME_CALL(GL.glDisablei(cap, index));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glDisablei(ser, cap, index);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glEnablei(SerialiserType &ser, GLenum cap, GLuint index)
{
  SERIALISE_ELEMENT(cap);
  SERIALISE_ELEMENT(index);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glEnablei(cap, index);
  }

  return true;
}

void WrappedOpenGL::glEnablei(GLenum cap, GLuint index)
{
  SERIALISE_TIME_CALL(GL.glEnablei(cap, index));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glEnablei(ser, cap, index);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFrontFace(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glFrontFace(mode);
  }

  return true;
}

void WrappedOpenGL::glFrontFace(GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glFrontFace(mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFrontFace(ser, mode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCullFace(SerialiserType &ser, GLenum mode)
{
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glCullFace(mode);
  }

  return true;
}

void WrappedOpenGL::glCullFace(GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glCullFace(mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCullFace(ser, mode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glHint(SerialiserType &ser, GLenum target, GLenum mode)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glHint(target, mode);
  }

  return true;
}

void WrappedOpenGL::glHint(GLenum target, GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glHint(target, mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glHint(ser, target, mode);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glColorMask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE, blue ? GL_TRUE : GL_FALSE,
                   alpha ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
  SERIALISE_TIME_CALL(GL.glColorMask(red, green, blue, alpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glColorMask(ser, red, green, blue, alpha);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glColorMaski(buf, red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
                    blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue,
                                 GLboolean alpha)
{
  SERIALISE_TIME_CALL(GL.glColorMaski(buf, red, green, blue, alpha));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glColorMaski(ser, buf, red, green, blue, alpha);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSampleMaski(SerialiserType &ser, GLuint maskNumber, GLbitfield mask)
{
  SERIALISE_ELEMENT(maskNumber);
  SERIALISE_ELEMENT(mask);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSampleMaski(maskNumber, mask);
  }

  return true;
}

void WrappedOpenGL::glSampleMaski(GLuint maskNumber, GLbitfield mask)
{
  SERIALISE_TIME_CALL(GL.glSampleMaski(maskNumber, mask));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSampleMaski(ser, maskNumber, mask);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSampleCoverage(SerialiserType &ser, GLfloat value, GLboolean invert)
{
  SERIALISE_ELEMENT(value);
  SERIALISE_ELEMENT_TYPED(bool, invert);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glSampleCoverage(value, invert ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glSampleCoverage(GLfloat value, GLboolean invert)
{
  SERIALISE_TIME_CALL(GL.glSampleCoverage(value, invert));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSampleCoverage(ser, value, invert);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMinSampleShading(SerialiserType &ser, GLfloat value)
{
  SERIALISE_ELEMENT(value);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glMinSampleShading(value);
  }

  return true;
}

void WrappedOpenGL::glMinSampleShading(GLfloat value)
{
  SERIALISE_TIME_CALL(GL.glMinSampleShading(value));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMinSampleShading(ser, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glRasterSamplesEXT(SerialiserType &ser, GLuint samples,
                                                 GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT_TYPED(bool, fixedsamplelocations);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glRasterSamplesEXT);

    GL.glRasterSamplesEXT(samples, fixedsamplelocations ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedOpenGL::glRasterSamplesEXT(GLuint samples, GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glRasterSamplesEXT(samples, fixedsamplelocations));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glRasterSamplesEXT(ser, samples, fixedsamplelocations);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPatchParameteri(SerialiserType &ser, GLenum pname, GLint value)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(value);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPatchParameteri(pname, value);
  }

  return true;
}

void WrappedOpenGL::glPatchParameteri(GLenum pname, GLint value)
{
  SERIALISE_TIME_CALL(GL.glPatchParameteri(pname, value));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPatchParameteri(ser, pname, value);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPatchParameterfv(SerialiserType &ser, GLenum pname,
                                                 const GLfloat *values)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(values, pname == eGL_PATCH_DEFAULT_OUTER_LEVEL ? 4U : 2U);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPatchParameterfv(pname, values);
  }

  return true;
}

void WrappedOpenGL::glPatchParameterfv(GLenum pname, const GLfloat *values)
{
  SERIALISE_TIME_CALL(GL.glPatchParameterfv(pname, values));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPatchParameterfv(ser, pname, values);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glLineWidth(SerialiserType &ser, GLfloat width)
{
  SERIALISE_ELEMENT(width);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glLineWidth(width);
  }

  return true;
}

void WrappedOpenGL::glLineWidth(GLfloat width)
{
  SERIALISE_TIME_CALL(GL.glLineWidth(width));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glLineWidth(ser, width);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointSize(SerialiserType &ser, GLfloat size)
{
  SERIALISE_ELEMENT(size);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPointSize(size);
  }

  return true;
}

void WrappedOpenGL::glPointSize(GLfloat size)
{
  SERIALISE_TIME_CALL(GL.glPointSize(size));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointSize(ser, size);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPointParameteri(pname, param);
  }

  return true;
}

void WrappedOpenGL::glPointParameteri(GLenum pname, GLint param)
{
  SERIALISE_TIME_CALL(GL.glPointParameteri(pname, param));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameteri(ser, pname, param);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameteriv(SerialiserType &ser, GLenum pname,
                                                 const GLint *params)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_LOCAL(Param, *params);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPointParameteriv(pname, &Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameteriv(GLenum pname, const GLint *params)
{
  SERIALISE_TIME_CALL(GL.glPointParameteriv(pname, params));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameteriv(ser, pname, params);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameterf(SerialiserType &ser, GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPointParameterf(pname, param);
  }

  return true;
}

void WrappedOpenGL::glPointParameterf(GLenum pname, GLfloat param)
{
  SERIALISE_TIME_CALL(GL.glPointParameterf(pname, param));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameterf(ser, pname, param);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPointParameterfv(SerialiserType &ser, GLenum pname,
                                                 const GLfloat *params)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_LOCAL(Param, *params);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPointParameterfv(pname, &Param);
  }

  return true;
}

void WrappedOpenGL::glPointParameterfv(GLenum pname, const GLfloat *params)
{
  SERIALISE_TIME_CALL(GL.glPointParameterfv(pname, params));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPointParameterfv(ser, pname, params);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glViewport(x, y, width, height);
  }

  return true;
}

void WrappedOpenGL::glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glViewport(x, y, width, height));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glViewport(ser, x, y, width, height);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glViewportArrayv(SerialiserType &ser, GLuint index, GLuint count,
                                               const GLfloat *v)
{
  SERIALISE_ELEMENT(index);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(v, count * 4);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glViewportArrayv(index, count, v);
  }

  return true;
}

void WrappedOpenGL::glViewportArrayv(GLuint index, GLuint count, const GLfloat *v)
{
  SERIALISE_TIME_CALL(GL.glViewportArrayv(index, count, v));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glViewportArrayv(ser, index, count, v);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glScissor(x, y, width, height);
  }

  return true;
}

void WrappedOpenGL::glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glScissor(x, y, width, height));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glScissor(ser, x, y, width, height);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glScissorArrayv(SerialiserType &ser, GLuint first, GLsizei count,
                                              const GLint *v)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_ARRAY(v, count * 4);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glScissorArrayv(first, count, v);
  }

  return true;
}

void WrappedOpenGL::glScissorArrayv(GLuint first, GLsizei count, const GLint *v)
{
  SERIALISE_TIME_CALL(GL.glScissorArrayv(first, count, v));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glScissorArrayv(ser, first, count, v);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPolygonMode(face, mode);
  }

  return true;
}

void WrappedOpenGL::glPolygonMode(GLenum face, GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glPolygonMode(face, mode));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPolygonMode(ser, face, mode);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPolygonOffset(SerialiserType &ser, GLfloat factor, GLfloat units)
{
  SERIALISE_ELEMENT(factor);
  SERIALISE_ELEMENT(units);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPolygonOffset(factor, units);
  }

  return true;
}

void WrappedOpenGL::glPolygonOffset(GLfloat factor, GLfloat units)
{
  SERIALISE_TIME_CALL(GL.glPolygonOffset(factor, units));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPolygonOffset(ser, factor, units);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPolygonOffsetClamp(SerialiserType &ser, GLfloat factor,
                                                   GLfloat units, GLfloat clamp)
{
  SERIALISE_ELEMENT(factor);
  SERIALISE_ELEMENT(units);
  SERIALISE_ELEMENT(clamp);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glPolygonOffsetClamp);

    GL.glPolygonOffsetClamp(factor, units, clamp);
  }

  return true;
}

void WrappedOpenGL::glPolygonOffsetClamp(GLfloat factor, GLfloat units, GLfloat clamp)
{
  SERIALISE_TIME_CALL(GL.glPolygonOffsetClamp(factor, units, clamp));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPolygonOffsetClamp(ser, factor, units, clamp);

    GetContextRecord()->AddChunk(scope.Get());
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

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glPrimitiveBoundingBox);

    GL.glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
  }

  return true;
}

void WrappedOpenGL::glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW,
                                           GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW)
{
  SERIALISE_TIME_CALL(GL.glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPrimitiveBoundingBox(ser, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    GetContextRecord()->AddChunk(scope.Get());
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
INSTANTIATE_FUNCTION_SERIALISED(void, glPolygonOffsetClamp, GLfloat factor, GLfloat units,
                                GLfloat clamp);
INSTANTIATE_FUNCTION_SERIALISED(void, glPrimitiveBoundingBox, GLfloat minX, GLfloat minY,
                                GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY,
                                GLfloat maxZ, GLfloat maxW);
