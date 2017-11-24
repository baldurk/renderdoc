/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "core/core.h"
#include "strings/string_utils.h"
#include "gl_common.h"
#include "gl_driver.h"

// bit of a hack, to work around C4127: conditional expression is constant
// on template parameters
template <typename T>
T CheckConstParam(T t);
template <>
bool CheckConstParam(bool t)
{
  return t;
}

template <const bool CopyUniforms, const bool SerialiseUniforms, typename SerialiserType>
static void ForAllProgramUniforms(SerialiserType *ser, CaptureState state, const GLHookSet &gl,
                                  GLuint progSrc, GLuint progDst, map<GLint, GLint> *locTranslate)
{
  const bool ReadSourceProgram = CopyUniforms || (SerialiseUniforms && ser && ser->IsWriting());
  const bool WriteDestProgram = CopyUniforms || (SerialiseUniforms && ser && ser->IsReading());

  RDCCOMPILE_ASSERT((CopyUniforms && !SerialiseUniforms) || (!CopyUniforms && SerialiseUniforms),
                    "Invalid call to ForAllProgramUniforms");

  GLint NumUniforms = 0;
  if(CheckConstParam(ReadSourceProgram))
    gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &NumUniforms);

  if(CheckConstParam(SerialiseUniforms) && ser)
  {
    // get accurate count of uniforms not in UBOs
    GLint numSerialised = 0;

    if(ser->IsWriting())
    {
      for(GLint i = 0; i < NumUniforms; i++)
      {
        GLenum prop = eGL_BLOCK_INDEX;
        GLint blockIdx;
        gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, 1, &prop, 1, NULL, (GLint *)&blockIdx);

        if(blockIdx >= 0)
          continue;

        numSerialised++;
      }
    }

    ser->Serialise("NumUniforms", numSerialised);

    if(ser->IsReading())
      NumUniforms = numSerialised;
  }

  const size_t numProps = 5;
  GLenum resProps[numProps] = {
      eGL_BLOCK_INDEX, eGL_TYPE, eGL_NAME_LENGTH, eGL_ARRAY_SIZE, eGL_LOCATION,
  };

  for(GLint i = 0; i < NumUniforms; i++)
  {
    GLenum type = eGL_NONE;
    int32_t arraySize = 0;
    int32_t srcLocation = 0;
    string basename;
    bool isArray = false;

    if(CheckConstParam(ReadSourceProgram))
    {
      GLint values[numProps];
      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, numProps, resProps, numProps, NULL, values);

      // we don't need to consider uniforms within UBOs
      if(values[0] >= 0)
        continue;

      type = (GLenum)values[1];
      arraySize = values[3];
      srcLocation = values[4];

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_UNIFORM, i, values[2], NULL, n);

      if(arraySize > 1)
      {
        isArray = true;

        size_t len = strlen(n);

        if(n[len - 3] == '[' && n[len - 2] == '0' && n[len - 1] == ']')
          n[len - 3] = 0;
      }
      else
      {
        arraySize = 1;
      }

      basename = n;
    }

    if(CheckConstParam(SerialiseUniforms) && ser)
    {
      ser->Serialise("type", type);
      ser->Serialise("arraySize", arraySize);
      ser->Serialise("basename", basename);
      ser->Serialise("isArray", isArray);
    }

    double dv[16] = {};
    float *fv = (float *)dv;
    int32_t *iv = (int32_t *)dv;
    uint32_t *uiv = (uint32_t *)dv;

    for(GLint arr = 0; arr < arraySize; arr++)
    {
      string name = basename;

      if(isArray)
      {
        name += StringFormat::Fmt("[%d]", arr);

        if(CheckConstParam(ReadSourceProgram))
          srcLocation = gl.glGetUniformLocation(progSrc, name.c_str());
      }

      if(CheckConstParam(SerialiseUniforms) && ser)
        ser->Serialise("srcLocation", srcLocation);

      GLint newloc = 0;
      if(CheckConstParam(WriteDestProgram) && IsReplayMode(state))
      {
        newloc = gl.glGetUniformLocation(progDst, name.c_str());
        if(locTranslate)
          (*locTranslate)[srcLocation] = newloc;
      }

      if(CheckConstParam(CopyUniforms) && newloc == -1)
        continue;

      if(CheckConstParam(ReadSourceProgram))
      {
        switch(type)
        {
          case eGL_FLOAT_MAT4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT4x3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT4x2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT3x4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT3x2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT2x4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT2x3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_DOUBLE_MAT4: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT4x3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT4x2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT3x4: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT3x2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT2x4: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT2x3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_FLOAT: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_DOUBLE: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_VEC2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_VEC3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_VEC4:
            gl.glGetUniformdv(progSrc, srcLocation, dv);
            break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_1D:
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_1D_SHADOW:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_1D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_1D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_SAMPLER_2D_RECT:
          case eGL_SAMPLER_2D_RECT_SHADOW:
          case eGL_INT_SAMPLER_1D:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_1D_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D_RECT:
          case eGL_UNSIGNED_INT_SAMPLER_1D:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_IMAGE_1D:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_2D_RECT:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_1D_ARRAY:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_IMAGE_2D_MULTISAMPLE:
          case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_IMAGE_1D:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_2D_RECT:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_1D_ARRAY:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_1D:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_ATOMIC_COUNTER:
          case eGL_INT: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_INT_VEC2: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_INT_VEC3: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_INT_VEC4: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(type).c_str());
        }
      }

      if(CheckConstParam(SerialiseUniforms) && ser)
        ser->Serialise("data", dv);

      if(CheckConstParam(WriteDestProgram) && IsReplayMode(state))
      {
        switch(type)
        {
          case eGL_FLOAT_MAT4: gl.glProgramUniformMatrix4fv(progDst, newloc, 1, false, fv); break;
          case eGL_FLOAT_MAT4x3:
            gl.glProgramUniformMatrix4x3fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT4x2:
            gl.glProgramUniformMatrix4x2fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3: gl.glProgramUniformMatrix3fv(progDst, newloc, 1, false, fv); break;
          case eGL_FLOAT_MAT3x4:
            gl.glProgramUniformMatrix3x4fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3x2:
            gl.glProgramUniformMatrix3x2fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2: gl.glProgramUniformMatrix2fv(progDst, newloc, 1, false, fv); break;
          case eGL_FLOAT_MAT2x4:
            gl.glProgramUniformMatrix2x4fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2x3:
            gl.glProgramUniformMatrix2x3fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_DOUBLE_MAT4: gl.glProgramUniformMatrix4dv(progDst, newloc, 1, false, dv); break;
          case eGL_DOUBLE_MAT4x3:
            gl.glProgramUniformMatrix4x3dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT4x2:
            gl.glProgramUniformMatrix4x2dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3: gl.glProgramUniformMatrix3dv(progDst, newloc, 1, false, dv); break;
          case eGL_DOUBLE_MAT3x4:
            gl.glProgramUniformMatrix3x4dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3x2:
            gl.glProgramUniformMatrix3x2dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2: gl.glProgramUniformMatrix2dv(progDst, newloc, 1, false, dv); break;
          case eGL_DOUBLE_MAT2x4:
            gl.glProgramUniformMatrix2x4dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2x3:
            gl.glProgramUniformMatrix2x3dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_FLOAT: gl.glProgramUniform1fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC2: gl.glProgramUniform2fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC3: gl.glProgramUniform3fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC4: gl.glProgramUniform4fv(progDst, newloc, 1, fv); break;
          case eGL_DOUBLE: gl.glProgramUniform1dv(progDst, newloc, 1, dv); break;
          case eGL_DOUBLE_VEC2: gl.glProgramUniform2dv(progDst, newloc, 1, dv); break;
          case eGL_DOUBLE_VEC3: gl.glProgramUniform3dv(progDst, newloc, 1, dv); break;
          case eGL_DOUBLE_VEC4:
            gl.glProgramUniform4dv(progDst, newloc, 1, dv);
            break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_1D:
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_1D_SHADOW:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_1D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_1D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_SAMPLER_2D_RECT:
          case eGL_SAMPLER_2D_RECT_SHADOW:
          case eGL_INT_SAMPLER_1D:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_1D_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D_RECT:
          case eGL_UNSIGNED_INT_SAMPLER_1D:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_IMAGE_1D:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_2D_RECT:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_1D_ARRAY:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_IMAGE_2D_MULTISAMPLE:
          case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_IMAGE_1D:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_2D_RECT:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_1D_ARRAY:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_1D:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_ATOMIC_COUNTER:
          case eGL_INT: gl.glProgramUniform1iv(progDst, newloc, 1, iv); break;
          case eGL_INT_VEC2: gl.glProgramUniform2iv(progDst, newloc, 1, iv); break;
          case eGL_INT_VEC3: gl.glProgramUniform3iv(progDst, newloc, 1, iv); break;
          case eGL_INT_VEC4: gl.glProgramUniform4iv(progDst, newloc, 1, iv); break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL: gl.glProgramUniform1uiv(progDst, newloc, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2: gl.glProgramUniform2uiv(progDst, newloc, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3: gl.glProgramUniform3uiv(progDst, newloc, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: gl.glProgramUniform4uiv(progDst, newloc, 1, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(type).c_str());
        }
      }
    }
  }

  GLint numUBOs = 0;
  if(CheckConstParam(ReadSourceProgram))
    gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

  if(CheckConstParam(SerialiseUniforms) && ser)
    ser->Serialise("numUBOs", numUBOs);

  for(GLint i = 0; i < numUBOs; i++)
  {
    GLenum prop = eGL_BUFFER_BINDING;
    uint32_t bind = 0;
    string name;

    if(CheckConstParam(ReadSourceProgram))
    {
      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM_BLOCK, i, 1, &prop, 1, NULL, (GLint *)&bind);

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_UNIFORM_BLOCK, i, 1023, NULL, n);

      name = n;
    }

    if(CheckConstParam(SerialiseUniforms) && ser)
    {
      ser->Serialise("bind", bind);
      ser->Serialise("name", name);
    }

    if(CheckConstParam(WriteDestProgram) && IsReplayMode(state))
    {
      GLuint idx = gl.glGetUniformBlockIndex(progDst, name.c_str());
      if(idx != GL_INVALID_INDEX)
        gl.glUniformBlockBinding(progDst, idx, bind);
    }
  }

  GLint numSSBOs = 0;
  if(CheckConstParam(ReadSourceProgram) && HasExt[ARB_shader_storage_buffer_object])
    gl.glGetProgramInterfaceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);

  if(CheckConstParam(SerialiseUniforms) && ser)
    ser->Serialise("numSSBOs", numSSBOs);

  for(GLint i = 0; i < numSSBOs; i++)
  {
    GLenum prop = eGL_BUFFER_BINDING;
    uint32_t bind = 0;
    string name;

    if(CheckConstParam(ReadSourceProgram))
    {
      gl.glGetProgramResourceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1, &prop, 1, NULL,
                                (GLint *)&bind);

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1023, NULL, n);

      name = n;
    }

    if(CheckConstParam(SerialiseUniforms) && ser)
    {
      ser->Serialise("bind", bind);
      ser->Serialise("name", name);
    }

    if(CheckConstParam(WriteDestProgram) && IsReplayMode(state))
    {
      GLuint idx = gl.glGetProgramResourceIndex(progDst, eGL_SHADER_STORAGE_BLOCK, name.c_str());
      if(idx != GL_INVALID_INDEX)
      {
        if(gl.glShaderStorageBlockBinding)
        {
          gl.glShaderStorageBlockBinding(progDst, i, bind);
        }
        else
        {
          // TODO glShaderStorageBlockBinding is not core GLES
          RDCERR("glShaderStorageBlockBinding is not supported!");
        }
      }
    }
  }
}

void CopyProgramUniforms(const GLHookSet &gl, GLuint progSrc, GLuint progDst)
{
  const bool CopyUniforms = true;
  const bool SerialiseUniforms = false;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms, ReadSerialiser>(
      NULL, CaptureState::ActiveReplaying, gl, progSrc, progDst, NULL);
}

template <typename SerialiserType>
void SerialiseProgramUniforms(SerialiserType &ser, CaptureState state, const GLHookSet &gl,
                              GLuint prog, map<GLint, GLint> *locTranslate)
{
  const bool CopyUniforms = false;
  const bool SerialiseUniforms = true;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(&ser, state, gl, prog, prog, locTranslate);
}

template void SerialiseProgramUniforms(ReadSerialiser &ser, CaptureState state, const GLHookSet &gl,
                                       GLuint prog, map<GLint, GLint> *locTranslate);
template void SerialiseProgramUniforms(WriteSerialiser &ser, CaptureState state, const GLHookSet &gl,
                                       GLuint prog, map<GLint, GLint> *locTranslate);

void CopyProgramAttribBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                               ShaderReflection *refl)
{
  // copy over attrib bindings
  for(const SigParameter &sig : refl->InputSig)
  {
    // skip built-ins
    if(sig.systemValue != ShaderBuiltin::Undefined)
      continue;

    GLint idx = gl.glGetAttribLocation(progsrc, sig.varName.c_str());
    if(idx >= 0)
      gl.glBindAttribLocation(progdst, (GLuint)idx, sig.varName.c_str());
  }
}

void CopyProgramFragDataBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                                 ShaderReflection *refl)
{
  uint64_t used = 0;

  // copy over fragdata bindings
  for(size_t i = 0; i < refl->OutputSig.size(); i++)
  {
    // only look at colour outputs (should be the only outputs from fs)
    if(refl->OutputSig[i].systemValue != ShaderBuiltin::ColorOutput)
      continue;

    if(!strncmp("gl_", refl->OutputSig[i].varName.c_str(), 3))
      continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix

    GLint idx = gl.glGetFragDataLocation(progsrc, refl->OutputSig[i].varName.c_str());
    if(idx >= 0)
    {
      uint64_t mask = 1ULL << idx;

      if(used & mask)
      {
        RDCWARN("Multiple signatures bound to output %zu, ignoring %s", i,
                refl->OutputSig[i].varName.c_str());
        continue;
      }

      used |= mask;

      if(gl.glBindFragDataLocation)
      {
        gl.glBindFragDataLocation(progdst, (GLuint)idx, refl->OutputSig[i].varName.c_str());
      }
      else
      {
        // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
        // TODO what to do if that extension is not supported
        RDCERR("glBindFragDataLocation is not supported!");
      }
    }
  }
}

template <typename SerialiserType>
void SerialiseProgramBindings(SerialiserType &ser, CaptureState state, const GLHookSet &gl,
                              GLuint prog)
{
  char Name[128] = {0};

  for(int sigType = 0; sigType < 2; sigType++)
  {
    GLenum sigEnum = (sigType == 0 ? eGL_PROGRAM_INPUT : eGL_PROGRAM_OUTPUT);

    uint64_t used = 0;

    int32_t NumAttributes = 0;

    if(ser.IsWriting())
      gl.glGetProgramInterfaceiv(prog, sigEnum, eGL_ACTIVE_RESOURCES, (GLint *)&NumAttributes);

    SERIALISE_ELEMENT(NumAttributes);

    for(GLint i = 0; i < NumAttributes; i++)
    {
      int32_t Location = -1;

      if(ser.IsWriting())
      {
        gl.glGetProgramResourceName(prog, sigEnum, i, 128, NULL, Name);

        if(sigType == 0)
          Location = gl.glGetAttribLocation(prog, Name);
        else
          Location = gl.glGetFragDataLocation(prog, Name);
      }

      SERIALISE_ELEMENT(Name);
      SERIALISE_ELEMENT(Location);

      if(ser.IsReading() && IsReplayMode(state) && Location >= 0)
      {
        uint64_t mask = 1ULL << Location;

        if(used & mask)
        {
          RDCWARN("Multiple %s items bound to location %d, ignoring %s",
                  sigType == 0 ? "attrib" : "fragdata", Location, Name);
          continue;
        }

        used |= mask;

        if(!strncmp("gl_", Name, 3))
          continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix (for both
                       // glBindAttribLocation and glBindFragDataLocation)

        if(sigType == 0)
        {
          gl.glBindAttribLocation(prog, (GLuint)Location, Name);
        }
        else
        {
          if(gl.glBindFragDataLocation)
          {
            gl.glBindFragDataLocation(prog, (GLuint)Location, Name);
          }
          else
          {
            // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
            // TODO what to do if that extension is not supported
            RDCERR("glBindFragDataLocation is not supported!");
          }
        }
      }
    }
  }
}

template void SerialiseProgramBindings(ReadSerialiser &ser, CaptureState state, const GLHookSet &gl,
                                       GLuint prog);
template void SerialiseProgramBindings(WriteSerialiser &ser, CaptureState state,
                                       const GLHookSet &gl, GLuint prog);
