/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include "../gles_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"

bool WrappedGLES::Serialise_glProgramUniformVector(GLuint program, GLint location, GLsizei count,
                                                     const void *value, UniformType type)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(UniformType, Type, type);
  SERIALISE_ELEMENT(int32_t, Loc, location);
  SERIALISE_ELEMENT(uint32_t, Count, count);

  size_t elemsPerVec = 0;
  size_t elemSize = sizeof(float);

  switch(Type)
  {
    case VEC1iv:
    case VEC1uiv:
    case VEC1fv: elemsPerVec = 1; break;
    case VEC2iv:
    case VEC2uiv:
    case VEC2fv: elemsPerVec = 2; break;
    case VEC3iv:
    case VEC3uiv:
    case VEC3fv: elemsPerVec = 3; break;
    case VEC4iv:
    case VEC4uiv:
    case VEC4fv: elemsPerVec = 4; break;
    default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
  }

  if(m_State >= WRITING)
  {
    m_pSerialiser->RawWriteBytes(value, elemSize * elemsPerVec * Count);
  }
  else if(m_State <= EXECUTING)
  {
    value = m_pSerialiser->RawReadBytes(elemSize * elemsPerVec * Count);

    ResourceId liveProgId = GetResourceManager()->GetLiveID(id);
    GLuint live = GetResourceManager()->GetLiveResource(id).name;

    map<GLint, GLint> &translate = m_Programs[liveProgId].locationTranslate;
    if(translate.find(Loc) != translate.end())
      Loc = translate[Loc];
    else
      Loc = -1;

    if(Loc >= 0)
    {
      switch(Type)
      {
        case VEC1iv: m_Real.glProgramUniform1iv(live, Loc, Count, (const GLint *)value); break;
        case VEC1uiv: m_Real.glProgramUniform1uiv(live, Loc, Count, (const GLuint *)value); break;
        case VEC1fv: m_Real.glProgramUniform1fv(live, Loc, Count, (const GLfloat *)value); break;
        case VEC2iv: m_Real.glProgramUniform2iv(live, Loc, Count, (const GLint *)value); break;
        case VEC2uiv: m_Real.glProgramUniform2uiv(live, Loc, Count, (const GLuint *)value); break;
        case VEC2fv: m_Real.glProgramUniform2fv(live, Loc, Count, (const GLfloat *)value); break;
        case VEC3iv: m_Real.glProgramUniform3iv(live, Loc, Count, (const GLint *)value); break;
        case VEC3uiv: m_Real.glProgramUniform3uiv(live, Loc, Count, (const GLuint *)value); break;
        case VEC3fv: m_Real.glProgramUniform3fv(live, Loc, Count, (const GLfloat *)value); break;
        case VEC4iv: m_Real.glProgramUniform4iv(live, Loc, Count, (const GLint *)value); break;
        case VEC4uiv: m_Real.glProgramUniform4uiv(live, Loc, Count, (const GLuint *)value); break;
        case VEC4fv: m_Real.glProgramUniform4fv(live, Loc, Count, (const GLfloat *)value); break;
        default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
      }
    }
  }

  if(m_pSerialiser->GetDebugText())
  {
    union
    {
      float *f;
      int32_t *i;
      uint32_t *u;
      double *d;
    } v;

    v.f = (float *)value;

    switch(Type)
    {
      case VEC1fv: m_pSerialiser->DebugPrint("value: {%f}\n", v.f[0]); break;
      case VEC1iv: m_pSerialiser->DebugPrint("value: {%d}\n", v.i[0]); break;
      case VEC1uiv: m_pSerialiser->DebugPrint("value: {%u}\n", v.u[0]); break;

      case VEC2fv: m_pSerialiser->DebugPrint("value: {%f, %f}\n", v.f[0], v.f[1]); break;
      case VEC2iv: m_pSerialiser->DebugPrint("value: {%d, %d}\n", v.i[0], v.i[1]); break;
      case VEC2uiv: m_pSerialiser->DebugPrint("value: {%u, %u}\n", v.u[0], v.u[1]); break;

      case VEC3fv:
        m_pSerialiser->DebugPrint("value: {%f, %f, %f}\n", v.f[0], v.f[1], v.f[2]);
        break;
      case VEC3iv:
        m_pSerialiser->DebugPrint("value: {%d, %d, %d}\n", v.i[0], v.i[1], v.i[2]);
        break;
      case VEC3uiv:
        m_pSerialiser->DebugPrint("value: {%u, %u, %u}\n", v.u[0], v.u[1], v.u[2]);
        break;

      case VEC4fv:
        m_pSerialiser->DebugPrint("value: {%f, %f, %f, %f}\n", v.f[0], v.f[1], v.f[2], v.f[3]);
        break;
      case VEC4iv:
        m_pSerialiser->DebugPrint("value: {%d, %d, %d, %d}\n", v.i[0], v.i[1], v.i[2], v.i[3]);
        break;
      case VEC4uiv:
        m_pSerialiser->DebugPrint("value: {%u, %u, %u, %u}\n", v.u[0], v.u[1], v.u[2], v.u[3]);
        break;

      default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformVector: %d", Type);
    }
  }

  return true;
}

bool WrappedGLES::Serialise_glProgramUniformMatrix(GLuint program, GLint location, GLsizei count,
                                                     GLboolean transpose, const void *value,
                                                     UniformType type)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(ProgramRes(GetCtx(), program)));
  SERIALISE_ELEMENT(UniformType, Type, type);
  SERIALISE_ELEMENT(int32_t, Loc, location);
  SERIALISE_ELEMENT(uint32_t, Count, count);
  SERIALISE_ELEMENT(uint8_t, Transpose, transpose);

  size_t elemsPerMat = 0;
  size_t elemSize = sizeof(float);

  switch(Type)
  {
    case MAT2fv: elemsPerMat = 2 * 2; break;
    case MAT2x3fv:
    case MAT3x2fv: elemsPerMat = 2 * 3; break;
    case MAT2x4fv:
    case MAT4x2fv: elemsPerMat = 2 * 4; break;
    case MAT3fv: elemsPerMat = 3 * 3; break;
    case MAT3x4fv:
    case MAT4x3fv: elemsPerMat = 3 * 4; break;
    case MAT4fv: elemsPerMat = 4 * 4; break;
    default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformMatrix: %d", Type);
  }

  if(m_State >= WRITING)
  {
    m_pSerialiser->RawWriteBytes(value, elemSize * elemsPerMat * Count);
  }
  else if(m_State <= EXECUTING)
  {
    value = m_pSerialiser->RawReadBytes(elemSize * elemsPerMat * Count);

    ResourceId liveProgId = GetResourceManager()->GetLiveID(id);
    GLuint live = GetResourceManager()->GetLiveResource(id).name;

    map<GLint, GLint> &translate = m_Programs[liveProgId].locationTranslate;
    if(translate.find(Loc) != translate.end())
      Loc = translate[Loc];
    else
      Loc = -1;

    if(Loc >= 0)
    {
      switch(Type)
      {
        case MAT2fv:
          m_Real.glProgramUniformMatrix2fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT2x3fv:
          m_Real.glProgramUniformMatrix2x3fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT2x4fv:
          m_Real.glProgramUniformMatrix2x4fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT3fv:
          m_Real.glProgramUniformMatrix3fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT3x2fv:
          m_Real.glProgramUniformMatrix3x2fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT3x4fv:
          m_Real.glProgramUniformMatrix3x4fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT4fv:
          m_Real.glProgramUniformMatrix4fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT4x2fv:
          m_Real.glProgramUniformMatrix4x2fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        case MAT4x3fv:
          m_Real.glProgramUniformMatrix4x3fv(live, Loc, Count, Transpose, (const GLfloat *)value);
          break;
        default: RDCERR("Unexpected uniform type to Serialise_glProgramUniformMatrix: %d", Type);
      }
    }
  }

  if(m_pSerialiser->GetDebugText())
  {
    float *fv = (float *)value;

    m_pSerialiser->DebugPrint("value: {");
    for(size_t i = 0; i < elemsPerMat; i++)
    {
      if(i == 0)
        m_pSerialiser->DebugPrint("%f", fv[i]);
      else
        m_pSerialiser->DebugPrint(", %f", fv[i]);
    }
    m_pSerialiser->DebugPrint("}\n");
  }

  return true;
}

#define UNIFORM_FUNC(count, suffix, paramtype, ...)                                    \
                                                                                       \
  void WrappedGLES::CONCAT(CONCAT(FUNCNAME, count), suffix)(FUNCPARAMS, __VA_ARGS__) \
                                                                                       \
  {                                                                                    \
    m_Real.CONCAT(CONCAT(FUNCNAME, count), suffix)(FUNCARGPASS, ARRAYLIST);            \
                                                                                       \
    if(m_State == WRITING_CAPFRAME)                                                    \
    {                                                                                  \
      SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);                                 \
      const paramtype vals[] = {ARRAYLIST};                                            \
      Serialise_glProgramUniformVector(PROGRAM, location, 1, vals,                     \
                                       CONCAT(CONCAT(VEC, count), CONCAT(suffix, v))); \
      m_ContextRecord->AddChunk(scope.Get());                                          \
    }                                                                                  \
    else if(m_State == WRITING_IDLE)                                                   \
    {                                                                                  \
      GetResourceManager()->MarkDirtyResource(ProgramRes(GetCtx(), PROGRAM));          \
    }                                                                                  \
  }

#define FUNCNAME glUniform
#define FUNCPARAMS GLint location
#define FUNCARGPASS location
#define PROGRAM GetUniformProgram()

#define ARRAYLIST v0

UNIFORM_FUNC(1, f, GLfloat, GLfloat v0)
UNIFORM_FUNC(1, i, GLint, GLint v0)
UNIFORM_FUNC(1, ui, GLuint, GLuint v0)

#undef ARRAYLIST
#define ARRAYLIST v0, v1

UNIFORM_FUNC(2, f, GLfloat, GLfloat v0, GLfloat v1)
UNIFORM_FUNC(2, i, GLint, GLint v0, GLint v1)
UNIFORM_FUNC(2, ui, GLuint, GLuint v0, GLuint v1)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2

UNIFORM_FUNC(3, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2)
UNIFORM_FUNC(3, i, GLint, GLint v0, GLint v1, GLint v2)
UNIFORM_FUNC(3, ui, GLuint, GLuint v0, GLuint v1, GLuint v2)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2, v3

UNIFORM_FUNC(4, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
UNIFORM_FUNC(4, i, GLint, GLint v0, GLint v1, GLint v2, GLint v3)
UNIFORM_FUNC(4, ui, GLuint, GLuint v0, GLuint v1, GLuint v2, GLuint v3)

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

#undef ARRAYLIST
#define ARRAYLIST v0, v1

UNIFORM_FUNC(2, f, GLfloat, GLfloat v0, GLfloat v1)
UNIFORM_FUNC(2, i, GLint, GLint v0, GLint v1)
UNIFORM_FUNC(2, ui, GLuint, GLuint v0, GLuint v1)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2

UNIFORM_FUNC(3, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2)
UNIFORM_FUNC(3, i, GLint, GLint v0, GLint v1, GLint v2)
UNIFORM_FUNC(3, ui, GLuint, GLuint v0, GLuint v1, GLuint v2)

#undef ARRAYLIST
#define ARRAYLIST v0, v1, v2, v3

UNIFORM_FUNC(4, f, GLfloat, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
UNIFORM_FUNC(4, i, GLint, GLint v0, GLint v1, GLint v2, GLint v3)
UNIFORM_FUNC(4, ui, GLuint, GLuint v0, GLuint v1, GLuint v2, GLuint v3)

#undef ARRAYLIST

#undef UNIFORM_FUNC
#define UNIFORM_FUNC(unicount, suffix, paramtype)                                            \
                                                                                             \
  void WrappedGLES::CONCAT(CONCAT(FUNCNAME, unicount), CONCAT(suffix, v))(                 \
      FUNCPARAMS, GLsizei count, const paramtype *value)                                     \
                                                                                             \
  {                                                                                          \
    m_Real.CONCAT(CONCAT(FUNCNAME, unicount), CONCAT(suffix, v))(FUNCARGPASS, count, value); \
                                                                                             \
    if(m_State == WRITING_CAPFRAME)                                                          \
    {                                                                                        \
      SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_VECTOR);                                       \
      Serialise_glProgramUniformVector(PROGRAM, location, count, value,                      \
                                       CONCAT(CONCAT(VEC, unicount), CONCAT(suffix, v)));    \
      m_ContextRecord->AddChunk(scope.Get());                                                \
    }                                                                                        \
    else if(m_State == WRITING_IDLE)                                                         \
    {                                                                                        \
      GetResourceManager()->MarkDirtyResource(ProgramRes(GetCtx(), PROGRAM));                \
    }                                                                                        \
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

UNIFORM_FUNC(2, f, GLfloat)
UNIFORM_FUNC(2, i, GLint)
UNIFORM_FUNC(2, ui, GLuint)

UNIFORM_FUNC(3, f, GLfloat)
UNIFORM_FUNC(3, i, GLint)
UNIFORM_FUNC(3, ui, GLuint)

UNIFORM_FUNC(4, f, GLfloat)
UNIFORM_FUNC(4, i, GLint)
UNIFORM_FUNC(4, ui, GLuint)

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

UNIFORM_FUNC(2, f, GLfloat)
UNIFORM_FUNC(2, i, GLint)
UNIFORM_FUNC(2, ui, GLuint)

UNIFORM_FUNC(3, f, GLfloat)
UNIFORM_FUNC(3, i, GLint)
UNIFORM_FUNC(3, ui, GLuint)

UNIFORM_FUNC(4, f, GLfloat)
UNIFORM_FUNC(4, i, GLint)
UNIFORM_FUNC(4, ui, GLuint)

#undef UNIFORM_FUNC
#define UNIFORM_FUNC(dim, suffix, paramtype)                                            \
                                                                                        \
  void WrappedGLES::CONCAT(CONCAT(FUNCNAME, dim), suffix)(                            \
      FUNCPARAMS, GLsizei count, GLboolean transpose, const paramtype *value)           \
                                                                                        \
  {                                                                                     \
    m_Real.CONCAT(CONCAT(FUNCNAME, dim), suffix)(FUNCARGPASS, count, transpose, value); \
                                                                                        \
    if(m_State == WRITING_CAPFRAME)                                                     \
    {                                                                                   \
      SCOPED_SERIALISE_CONTEXT(PROGRAMUNIFORM_MATRIX);                                  \
      Serialise_glProgramUniformMatrix(PROGRAM, location, count, transpose, value,      \
                                       CONCAT(CONCAT(MAT, dim), suffix));               \
      m_ContextRecord->AddChunk(scope.Get());                                           \
    }                                                                                   \
    else if(m_State == WRITING_IDLE)                                                    \
    {                                                                                   \
      GetResourceManager()->MarkDirtyResource(ProgramRes(GetCtx(), PROGRAM));           \
    }                                                                                   \
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
