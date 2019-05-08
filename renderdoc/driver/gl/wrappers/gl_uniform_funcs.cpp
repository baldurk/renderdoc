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
bool WrappedOpenGL::Serialise_glProgramUniformVector(SerialiserType &ser, GLuint program,
                                                     GLint location, GLsizei count,
                                                     const void *value, UniformType type)
{
  SERIALISE_ELEMENT_LOCAL(Program, ProgramRes(GetCtx(), program));
  SERIALISE_ELEMENT(location);

  // this is used to share serialisation code amongst the brazillion variations
  SERIALISE_ELEMENT(type).Hidden();

  // not all variants technically have a count, so this will come through as a fixed value of 1.
  // It showing up even for those functions is a concession to sanity...
  SERIALISE_ELEMENT(count);

  size_t elemsPerVec = 0;
  SDBasic elemBaseType = SDBasic::Float;
  size_t elemSize = sizeof(float);

  switch(type)
  {
    case VEC1iv:
    case VEC1uiv:
    case VEC1fv:
    case VEC1dv: elemsPerVec = 1; break;
    case VEC2iv:
    case VEC2uiv:
    case VEC2fv:
    case VEC2dv: elemsPerVec = 2; break;
    case VEC3iv:
    case VEC3uiv:
    case VEC3fv:
    case VEC3dv: elemsPerVec = 3; break;
    case VEC4iv:
    case VEC4uiv:
    case VEC4fv:
    case VEC4dv: elemsPerVec = 4; break;
    default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", type);
  }

  switch(type)
  {
    case VEC1iv:
    case VEC2iv:
    case VEC3iv:
    case VEC4iv: elemBaseType = SDBasic::SignedInteger; break;
    case VEC1uiv:
    case VEC2uiv:
    case VEC3uiv:
    case VEC4uiv: elemBaseType = SDBasic::UnsignedInteger; break;
    case VEC1dv:
    case VEC2dv:
    case VEC3dv:
    case VEC4dv: elemSize = sizeof(double); break;
    default: break;
  }

  union
  {
    byte *alloc;
    float *f;
    int32_t *i;
    uint32_t *u;
    double *d;
  } v;

  if(ser.IsReading())
    v.alloc = new byte[elemSize * elemsPerVec * count];
  else
    v.alloc = (byte *)value;

  uint32_t arrayLength = uint32_t(elemsPerVec * count);

  // we don't want to allocate since we've already handled that
  if(elemBaseType == SDBasic::Float && elemSize == sizeof(float))
    ser.Serialise("values"_lit, v.f, arrayLength, SerialiserFlags::NoFlags);
  else if(elemBaseType == SDBasic::Float)
    ser.Serialise("values"_lit, v.d, arrayLength, SerialiserFlags::NoFlags);
  else if(elemBaseType == SDBasic::SignedInteger)
    ser.Serialise("values"_lit, v.i, arrayLength, SerialiserFlags::NoFlags);
  else if(elemBaseType == SDBasic::UnsignedInteger)
    ser.Serialise("values"_lit, v.u, arrayLength, SerialiserFlags::NoFlags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && Program.name)
  {
    ResourceId liveProgId = GetResourceManager()->GetID(Program);
    GLuint live = Program.name;

    std::map<GLint, GLint> &translate = m_Programs[liveProgId].locationTranslate;
    if(translate.find(location) != translate.end())
      location = translate[location];
    else
      location = -1;

    if(location >= 0)
    {
      switch(type)
      {
        case VEC1iv: GL.glProgramUniform1iv(live, location, count, v.i); break;
        case VEC1uiv: GL.glProgramUniform1uiv(live, location, count, v.u); break;
        case VEC1fv: GL.glProgramUniform1fv(live, location, count, v.f); break;
        case VEC1dv: GL.glProgramUniform1dv(live, location, count, v.d); break;
        case VEC2iv: GL.glProgramUniform2iv(live, location, count, v.i); break;
        case VEC2uiv: GL.glProgramUniform2uiv(live, location, count, v.u); break;
        case VEC2fv: GL.glProgramUniform2fv(live, location, count, v.f); break;
        case VEC2dv: GL.glProgramUniform2dv(live, location, count, v.d); break;
        case VEC3iv: GL.glProgramUniform3iv(live, location, count, v.i); break;
        case VEC3uiv: GL.glProgramUniform3uiv(live, location, count, v.u); break;
        case VEC3fv: GL.glProgramUniform3fv(live, location, count, v.f); break;
        case VEC3dv: GL.glProgramUniform3dv(live, location, count, v.d); break;
        case VEC4iv: GL.glProgramUniform4iv(live, location, count, v.i); break;
        case VEC4uiv: GL.glProgramUniform4uiv(live, location, count, v.u); break;
        case VEC4fv: GL.glProgramUniform4fv(live, location, count, v.f); break;
        case VEC4dv: GL.glProgramUniform4dv(live, location, count, v.d); break;
        default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", type);
      }
    }
  }

  if(ser.IsReading())
    delete[] v.alloc;

  return true;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glProgramUniformMatrix(SerialiserType &ser, GLuint program,
                                                     GLint location, GLsizei count,
                                                     GLboolean transpose, const void *value,
                                                     UniformType type)
{
  SERIALISE_ELEMENT_LOCAL(Program, ProgramRes(GetCtx(), program));
  SERIALISE_ELEMENT(location);
  SERIALISE_ELEMENT_TYPED(bool, transpose);

  // this is used to share serialisation code amongst the brazillion variations
  SERIALISE_ELEMENT(type).Hidden();

  // not all variants technically have a count, so this will come through as a fixed value of 1.
  // It showing up even for those functions is a concession to sanity...
  SERIALISE_ELEMENT(count);

  size_t elemsPerMat = 0;
  size_t elemSize = sizeof(float);

  switch(type)
  {
    case MAT2fv:
    case MAT2dv: elemsPerMat = 2 * 2; break;
    case MAT2x3fv:
    case MAT2x3dv:
    case MAT3x2fv:
    case MAT3x2dv: elemsPerMat = 2 * 3; break;
    case MAT2x4fv:
    case MAT2x4dv:
    case MAT4x2fv:
    case MAT4x2dv: elemsPerMat = 2 * 4; break;
    case MAT3fv:
    case MAT3dv: elemsPerMat = 3 * 3; break;
    case MAT3x4fv:
    case MAT3x4dv:
    case MAT4x3fv:
    case MAT4x3dv: elemsPerMat = 3 * 4; break;
    case MAT4fv:
    case MAT4dv: elemsPerMat = 4 * 4; break;
    default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformMatrix: %d", type);
  }

  switch(type)
  {
    case MAT2dv:
    case MAT2x3dv:
    case MAT2x4dv:
    case MAT3dv:
    case MAT3x2dv:
    case MAT3x4dv:
    case MAT4dv:
    case MAT4x2dv:
    case MAT4x3dv: elemSize = sizeof(double); break;
    default: break;
  }

  union
  {
    byte *alloc;
    float *f;
    double *d;
  } v;

  if(ser.IsReading())
    v.alloc = new byte[elemSize * elemsPerMat * count];
  else
    v.alloc = (byte *)value;

  uint32_t arrayLength = uint32_t(elemsPerMat * count);

  // we don't want to allocate since we've already handled that
  if(elemSize == sizeof(float))
    ser.Serialise("values"_lit, v.f, arrayLength, SerialiserFlags::NoFlags);
  else
    ser.Serialise("values"_lit, v.d, arrayLength, SerialiserFlags::NoFlags);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading() && Program.name)
  {
    ResourceId liveProgId = GetResourceManager()->GetID(Program);
    GLuint live = Program.name;

    std::map<GLint, GLint> &translate = m_Programs[liveProgId].locationTranslate;
    if(translate.find(location) != translate.end())
      location = translate[location];
    else
      location = -1;

    if(location >= 0)
    {
      switch(type)
      {
        case MAT2fv:
          GL.glProgramUniformMatrix2fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT2x3fv:
          GL.glProgramUniformMatrix2x3fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT2x4fv:
          GL.glProgramUniformMatrix2x4fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT3fv:
          GL.glProgramUniformMatrix3fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT3x2fv:
          GL.glProgramUniformMatrix3x2fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT3x4fv:
          GL.glProgramUniformMatrix3x4fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT4fv:
          GL.glProgramUniformMatrix4fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT4x2fv:
          GL.glProgramUniformMatrix4x2fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT4x3fv:
          GL.glProgramUniformMatrix4x3fv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.f);
          break;
        case MAT2dv:
          GL.glProgramUniformMatrix2dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT2x3dv:
          GL.glProgramUniformMatrix2x3dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT2x4dv:
          GL.glProgramUniformMatrix2x4dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT3dv:
          GL.glProgramUniformMatrix3dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT3x2dv:
          GL.glProgramUniformMatrix3x2dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT3x4dv:
          GL.glProgramUniformMatrix3x4dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT4dv:
          GL.glProgramUniformMatrix4dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT4x2dv:
          GL.glProgramUniformMatrix4x2dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        case MAT4x3dv:
          GL.glProgramUniformMatrix4x3dv(live, location, count, transpose ? GL_TRUE : GL_FALSE, v.d);
          break;
        default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformMatrix: %d", type);
      }
    }
  }

  if(ser.IsReading())
    delete[] v.alloc;

  return true;
}

#define UNIFORM_FUNC(count, suffix, paramtype, ...)                                          \
                                                                                             \
  void WrappedOpenGL::CONCAT(CONCAT(FUNCNAME, count), suffix)(FUNCPARAMS, __VA_ARGS__)       \
                                                                                             \
  {                                                                                          \
    SERIALISE_TIME_CALL(GL.CONCAT(CONCAT(FUNCNAME, count), suffix)(FUNCARGPASS, ARRAYLIST)); \
                                                                                             \
    if(IsActiveCapturing(m_State))                                                           \
    {                                                                                        \
      USE_SCRATCH_SERIALISER();                                                              \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                                   \
      const paramtype vals[] = {ARRAYLIST};                                                  \
      Serialise_glProgramUniformVector(ser, PROGRAM, location, 1, vals,                      \
                                       CONCAT(CONCAT(VEC, count), CONCAT(suffix, v)));       \
      GetContextRecord()->AddChunk(scope.Get());                                             \
    }                                                                                        \
    else if(IsBackgroundCapturing(m_State))                                                  \
    {                                                                                        \
      GetResourceManager()->MarkDirtyResource(ProgramRes(GetCtx(), PROGRAM));                \
    }                                                                                        \
  }

#define FUNCNAME glUniform
#define FUNCPARAMS GLint location
#define FUNCARGPASS location
#define PROGRAM GetUniformProgram()

#define ARRAYLIST v0

UNIFORM_FUNC(1, f, GLfloat, GLfloat v0)
UNIFORM_FUNC(1, i, GLint, GLint v0)
UNIFORM_FUNC(1, ui, GLuint, GLuint v0)
UNIFORM_FUNC(1, d, GLdouble, GLdouble v0)

#undef ARRAYLIST
#define ARRAYLIST v0, v1

UNIFORM_FUNC(2, f, GLfloat, GLfloat v0, GLfloat v1)
UNIFORM_FUNC(2, i, GLint, GLint v0, GLint v1)
UNIFORM_FUNC(2, ui, GLuint, GLuint v0, GLuint v1)
UNIFORM_FUNC(2, d, GLdouble, GLdouble v0, GLdouble v1)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2

UNIFORM_FUNC(3, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2)
UNIFORM_FUNC(3, i, GLint, GLint v0, GLint v1, GLint v2)
UNIFORM_FUNC(3, ui, GLuint, GLuint v0, GLuint v1, GLuint v2)
UNIFORM_FUNC(3, d, GLdouble, GLdouble v0, GLdouble v1, GLdouble v2)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2, v3

UNIFORM_FUNC(4, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
UNIFORM_FUNC(4, i, GLint, GLint v0, GLint v1, GLint v2, GLint v3)
UNIFORM_FUNC(4, ui, GLuint, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
UNIFORM_FUNC(4, d, GLdouble, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3)

#undef FUNCNAME
#undef FUNCPARAMS
#undef FUNCARGPASS
#undef PROGRAM
#define FUNCNAME glProgramUniform
#define FUNCPARAMS GLuint program, GLint location
#define FUNCARGPASS program, location
#define PROGRAM program

#undef ARRAYLIST
#define ARRAYLIST v0

UNIFORM_FUNC(1, f, GLfloat, GLfloat v0)
UNIFORM_FUNC(1, i, GLint, GLint v0)
UNIFORM_FUNC(1, ui, GLuint, GLuint v0)
UNIFORM_FUNC(1, d, GLdouble, GLdouble v0)

#undef ARRAYLIST
#define ARRAYLIST v0, v1

UNIFORM_FUNC(2, f, GLfloat, GLfloat v0, GLfloat v1)
UNIFORM_FUNC(2, i, GLint, GLint v0, GLint v1)
UNIFORM_FUNC(2, ui, GLuint, GLuint v0, GLuint v1)
UNIFORM_FUNC(2, d, GLdouble, GLdouble v0, GLdouble v1)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2

UNIFORM_FUNC(3, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2)
UNIFORM_FUNC(3, i, GLint, GLint v0, GLint v1, GLint v2)
UNIFORM_FUNC(3, ui, GLuint, GLuint v0, GLuint v1, GLuint v2)
UNIFORM_FUNC(3, d, GLdouble, GLdouble v0, GLdouble v1, GLdouble v2)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2, v3

UNIFORM_FUNC(4, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
UNIFORM_FUNC(4, i, GLint, GLint v0, GLint v1, GLint v2, GLint v3)
UNIFORM_FUNC(4, ui, GLuint, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
UNIFORM_FUNC(4, d, GLdouble, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3)

#undef ARRAYLIST

#undef UNIFORM_FUNC
#define UNIFORM_FUNC(unicount, suffix, paramtype)                                             \
                                                                                              \
  void WrappedOpenGL::CONCAT(CONCAT(FUNCNAME, unicount), CONCAT(suffix, v))(                  \
      FUNCPARAMS, GLsizei count, const paramtype *value)                                      \
                                                                                              \
  {                                                                                           \
    SERIALISE_TIME_CALL(                                                                      \
        GL.CONCAT(CONCAT(FUNCNAME, unicount), CONCAT(suffix, v))(FUNCARGPASS, count, value)); \
                                                                                              \
    if(IsActiveCapturing(m_State))                                                            \
    {                                                                                         \
      USE_SCRATCH_SERIALISER();                                                               \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                                    \
      Serialise_glProgramUniformVector(ser, PROGRAM, location, count, value,                  \
                                       CONCAT(CONCAT(VEC, unicount), CONCAT(suffix, v)));     \
      GetContextRecord()->AddChunk(scope.Get());                                              \
    }                                                                                         \
    else if(IsBackgroundCapturing(m_State))                                                   \
    {                                                                                         \
      GetResourceManager()->MarkDirtyResource(ProgramRes(GetCtx(), PROGRAM));                 \
    }                                                                                         \
  }

#undef FUNCNAME
#undef FUNCPARAMS
#undef FUNCARGPASS
#undef PROGRAM
#define FUNCNAME glUniform
#define FUNCPARAMS GLint location
#define FUNCARGPASS location
#define PROGRAM GetUniformProgram()

UNIFORM_FUNC(1, f, GLfloat)
UNIFORM_FUNC(1, i, GLint)
UNIFORM_FUNC(1, ui, GLuint)
UNIFORM_FUNC(1, d, GLdouble)

UNIFORM_FUNC(2, f, GLfloat)
UNIFORM_FUNC(2, i, GLint)
UNIFORM_FUNC(2, ui, GLuint)
UNIFORM_FUNC(2, d, GLdouble)

UNIFORM_FUNC(3, f, GLfloat)
UNIFORM_FUNC(3, i, GLint)
UNIFORM_FUNC(3, ui, GLuint)
UNIFORM_FUNC(3, d, GLdouble)

UNIFORM_FUNC(4, f, GLfloat)
UNIFORM_FUNC(4, i, GLint)
UNIFORM_FUNC(4, ui, GLuint)
UNIFORM_FUNC(4, d, GLdouble)

#undef FUNCNAME
#undef FUNCPARAMS
#undef FUNCARGPASS
#undef PROGRAM
#define FUNCNAME glProgramUniform
#define FUNCPARAMS GLuint program, GLint location
#define FUNCARGPASS program, location
#define PROGRAM program

UNIFORM_FUNC(1, f, GLfloat)
UNIFORM_FUNC(1, i, GLint)
UNIFORM_FUNC(1, ui, GLuint)
UNIFORM_FUNC(1, d, GLdouble)

UNIFORM_FUNC(2, f, GLfloat)
UNIFORM_FUNC(2, i, GLint)
UNIFORM_FUNC(2, ui, GLuint)
UNIFORM_FUNC(2, d, GLdouble)

UNIFORM_FUNC(3, f, GLfloat)
UNIFORM_FUNC(3, i, GLint)
UNIFORM_FUNC(3, ui, GLuint)
UNIFORM_FUNC(3, d, GLdouble)

UNIFORM_FUNC(4, f, GLfloat)
UNIFORM_FUNC(4, i, GLint)
UNIFORM_FUNC(4, ui, GLuint)
UNIFORM_FUNC(4, d, GLdouble)

#undef UNIFORM_FUNC
#define UNIFORM_FUNC(dim, suffix, paramtype)                                             \
                                                                                         \
  void WrappedOpenGL::CONCAT(CONCAT(FUNCNAME, dim), suffix)(                             \
      FUNCPARAMS, GLsizei count, GLboolean transpose, const paramtype *value)            \
                                                                                         \
  {                                                                                      \
    SERIALISE_TIME_CALL(                                                                 \
        GL.CONCAT(CONCAT(FUNCNAME, dim), suffix)(FUNCARGPASS, count, transpose, value)); \
                                                                                         \
    if(IsActiveCapturing(m_State))                                                       \
    {                                                                                    \
      USE_SCRATCH_SERIALISER();                                                          \
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);                                               \
      Serialise_glProgramUniformMatrix(ser, PROGRAM, location, count, transpose, value,  \
                                       CONCAT(CONCAT(MAT, dim), suffix));                \
      GetContextRecord()->AddChunk(scope.Get());                                         \
    }                                                                                    \
    else if(IsBackgroundCapturing(m_State))                                              \
    {                                                                                    \
      GetResourceManager()->MarkDirtyResource(ProgramRes(GetCtx(), PROGRAM));            \
    }                                                                                    \
  }

#undef FUNCNAME
#undef FUNCPARAMS
#undef FUNCARGPASS
#undef PROGRAM
#define FUNCNAME glUniformMatrix
#define FUNCPARAMS GLint location
#define FUNCARGPASS location
#define PROGRAM GetUniformProgram()

UNIFORM_FUNC(2, fv, GLfloat)
UNIFORM_FUNC(2x3, fv, GLfloat)
UNIFORM_FUNC(2x4, fv, GLfloat)
UNIFORM_FUNC(3, fv, GLfloat)
UNIFORM_FUNC(3x2, fv, GLfloat)
UNIFORM_FUNC(3x4, fv, GLfloat)
UNIFORM_FUNC(4, fv, GLfloat)
UNIFORM_FUNC(4x2, fv, GLfloat)
UNIFORM_FUNC(4x3, fv, GLfloat)

UNIFORM_FUNC(2, dv, GLdouble)
UNIFORM_FUNC(2x3, dv, GLdouble)
UNIFORM_FUNC(2x4, dv, GLdouble)
UNIFORM_FUNC(3, dv, GLdouble)
UNIFORM_FUNC(3x2, dv, GLdouble)
UNIFORM_FUNC(3x4, dv, GLdouble)
UNIFORM_FUNC(4, dv, GLdouble)
UNIFORM_FUNC(4x2, dv, GLdouble)
UNIFORM_FUNC(4x3, dv, GLdouble)

#undef FUNCNAME
#undef FUNCPARAMS
#undef FUNCARGPASS
#undef PROGRAM
#define FUNCNAME glProgramUniformMatrix
#define FUNCPARAMS GLuint program, GLint location
#define FUNCARGPASS program, location
#define PROGRAM program

UNIFORM_FUNC(2, fv, GLfloat)
UNIFORM_FUNC(2x3, fv, GLfloat)
UNIFORM_FUNC(2x4, fv, GLfloat)
UNIFORM_FUNC(3, fv, GLfloat)
UNIFORM_FUNC(3x2, fv, GLfloat)
UNIFORM_FUNC(3x4, fv, GLfloat)
UNIFORM_FUNC(4, fv, GLfloat)
UNIFORM_FUNC(4x2, fv, GLfloat)
UNIFORM_FUNC(4x3, fv, GLfloat)

UNIFORM_FUNC(2, dv, GLdouble)
UNIFORM_FUNC(2x3, dv, GLdouble)
UNIFORM_FUNC(2x4, dv, GLdouble)
UNIFORM_FUNC(3, dv, GLdouble)
UNIFORM_FUNC(3x2, dv, GLdouble)
UNIFORM_FUNC(3x4, dv, GLdouble)
UNIFORM_FUNC(4, dv, GLdouble)
UNIFORM_FUNC(4x2, dv, GLdouble)
UNIFORM_FUNC(4x3, dv, GLdouble)

INSTANTIATE_FUNCTION_SERIALISED(void, glProgramUniformVector, GLuint program, GLint location,
                                GLsizei count, const void *value, UniformType type);
INSTANTIATE_FUNCTION_SERIALISED(void, glProgramUniformMatrix, GLuint program, GLint location,
                                GLsizei count, GLboolean transpose, const void *value,
                                UniformType type);
