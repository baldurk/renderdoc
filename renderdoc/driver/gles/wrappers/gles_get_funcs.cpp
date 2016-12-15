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

GLenum WrappedGLES::glGetError()
{
  return m_Real.glGetError();
}

GLenum WrappedGLES::glGetGraphicsResetStatus()
{
  return m_Real.glGetGraphicsResetStatus();
}

GLuint WrappedGLES::glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources,
                                           GLenum *types, GLuint *ids, GLenum *severities,
                                           GLsizei *lengths, GLchar *messageLog)
{
  return m_Real.glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths,
                                     messageLog);
}

void WrappedGLES::glFlush()
{
  CoherentMapImplicitBarrier();

  m_Real.glFlush();
}

void WrappedGLES::glFinish()
{
  CoherentMapImplicitBarrier();

  m_Real.glFinish();
}

GLboolean WrappedGLES::glIsEnabled(GLenum cap)
{
  if(cap == eGL_DEBUG_TOOL_EXT)
    return true;

  return m_Real.glIsEnabled(cap);
}

GLboolean WrappedGLES::glIsTexture(GLuint texture)
{
  return m_Real.glIsTexture(texture);
}

GLboolean WrappedGLES::glIsEnabledi(GLenum target, GLuint index)
{
  if(target == eGL_DEBUG_TOOL_EXT)
    return true;

  return m_Real.glIsEnabledi(target, index);
}

GLboolean WrappedGLES::glIsBuffer(GLuint buffer)
{
  return m_Real.glIsBuffer(buffer);
}

GLboolean WrappedGLES::glIsFramebuffer(GLuint framebuffer)
{
  return m_Real.glIsFramebuffer(framebuffer);
}

GLboolean WrappedGLES::glIsProgram(GLuint program)
{
  return m_Real.glIsProgram(program);
}

GLboolean WrappedGLES::glIsProgramPipeline(GLuint pipeline)
{
  return m_Real.glIsProgramPipeline(pipeline);
}

GLboolean WrappedGLES::glIsQuery(GLuint id)
{
  return m_Real.glIsQuery(id);
}

GLboolean WrappedGLES::glIsRenderbuffer(GLuint renderbuffer)
{
  return m_Real.glIsRenderbuffer(renderbuffer);
}

GLboolean WrappedGLES::glIsSampler(GLuint sampler)
{
  return m_Real.glIsSampler(sampler);
}

GLboolean WrappedGLES::glIsShader(GLuint shader)
{
  return m_Real.glIsShader(shader);
}

GLboolean WrappedGLES::glIsSync(GLsync sync)
{
  return m_Real.glIsSync(sync);
}

GLboolean WrappedGLES::glIsTransformFeedback(GLuint id)
{
  return m_Real.glIsTransformFeedback(id);
}

GLboolean WrappedGLES::glIsVertexArray(GLuint array)
{
  return m_Real.glIsVertexArray(array);
}

void WrappedGLES::glGetFloatv(GLenum pname, GLfloat *params)
{
  m_Real.glGetFloatv(pname, params);
}

void WrappedGLES::glGetPointerv(GLenum pname, void **params)
{
  if(pname == eGL_DEBUG_CALLBACK_FUNCTION)
    *params = (void *)m_RealDebugFunc;
  else if(pname == eGL_DEBUG_CALLBACK_USER_PARAM)
    *params = (void *)m_RealDebugFuncParam;
  else
    m_Real.glGetPointerv(pname, params);
}

void WrappedGLES::glGetIntegerv(GLenum pname, GLint *params)
{
  if(pname == eGL_NUM_EXTENSIONS)
  {
    if(params)
      *params = (GLint)GetCtxData().glExts.size();
    return;
  }
  else if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
  {
    if(params)
      *params = GLint(eGL_DEBUG_TOOL_FRAME_CAPTURE_BIT_EXT);
    return;
  }

  m_Real.glGetIntegerv(pname, params);
}

void WrappedGLES::glGetBooleanv(GLenum pname, GLboolean *data)
{
  m_Real.glGetBooleanv(pname, data);
}

void WrappedGLES::glGetInteger64v(GLenum pname, GLint64 *data)
{
  if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
  {
    if(data)
      *data = GLint64(eGL_DEBUG_TOOL_FRAME_CAPTURE_BIT_EXT);
    return;
  }
  m_Real.glGetInteger64v(pname, data);
}

void WrappedGLES::glGetBooleani_v(GLenum pname, GLuint index, GLboolean *data)
{
  m_Real.glGetBooleani_v(pname, index, data);
}

void WrappedGLES::glGetIntegeri_v(GLenum pname, GLuint index, GLint *data)
{
  if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
  {
    if(data)
      *data = GLint(eGL_DEBUG_TOOL_FRAME_CAPTURE_BIT_EXT);
    return;
  }
  m_Real.glGetIntegeri_v(pname, index, data);
}

void WrappedGLES::glGetFloati_vOES(GLenum pname, GLuint index, GLfloat *data)
{
  m_Real.glGetFloati_vOES(pname, index, data);
}

void WrappedGLES::glGetFloati_vNV(GLenum pname, GLuint index, GLfloat *data)
{
  m_Real.glGetFloati_vNV(pname, index, data);
}

void WrappedGLES::glGetInteger64i_v(GLenum pname, GLuint index, GLint64 *data)
{
  if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
  {
    if(data)
      *data = GLint64(eGL_DEBUG_TOOL_FRAME_CAPTURE_BIT_EXT);
    return;
  }
  m_Real.glGetInteger64i_v(pname, index, data);
}

void WrappedGLES::glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
  m_Real.glGetTexLevelParameteriv(target, level, pname, params);
}

void WrappedGLES::glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
  m_Real.glGetTexLevelParameterfv(target, level, pname, params);
}

void WrappedGLES::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
  m_Real.glGetTexParameterfv(target, pname, params);
}

void WrappedGLES::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetTexParameteriv(target, pname, params);
}

void WrappedGLES::glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetTexParameterIiv(target, pname, params);
}

void WrappedGLES::glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params)
{
  m_Real.glGetTexParameterIuiv(target, pname, params);
}

void WrappedGLES::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                          GLsizei bufSize, GLint *params)
{
  m_Real.glGetInternalformativ(target, internalformat, pname, bufSize, params);
}

void WrappedGLES::glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params)
{
  m_Real.glGetSamplerParameterIiv(sampler, pname, params);
}

void WrappedGLES::glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params)
{
  m_Real.glGetSamplerParameterIuiv(sampler, pname, params);
}

void WrappedGLES::glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
  m_Real.glGetSamplerParameterfv(sampler, pname, params);
}

void WrappedGLES::glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
  m_Real.glGetSamplerParameteriv(sampler, pname, params);
}

void WrappedGLES::glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params)
{
  m_Real.glGetBufferParameteri64v(target, pname, params);
}

void WrappedGLES::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetBufferParameteriv(target, pname, params);
}

void WrappedGLES::glGetBufferPointerv(GLenum target, GLenum pname, void **params)
{
  CoherentMapImplicitBarrier();

  // intercept GL_BUFFER_MAP_POINTER queries
  if(pname == eGL_BUFFER_MAP_POINTER)
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERT(record);

    if(record)
    {
      if(record->Map.status == GLResourceRecord::Unmapped)
        *params = NULL;
      else
        *params = (void *)record->Map.ptr;
    }
    else
    {
      *params = NULL;
    }
  }
  else
  {
    m_Real.glGetBufferPointerv(target, pname, params);
  }
}

void WrappedGLES::glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
  m_Real.glGetQueryObjectuiv(id, pname, params);
}

void WrappedGLES::glGetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 *params)
{
  m_Real.glGetQueryObjectui64vEXT(id, pname, params);
}

void WrappedGLES::glGetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 *params)
{
  m_Real.glGetQueryObjecti64vEXT(id, pname, params);
}

void WrappedGLES::glGetQueryObjectivEXT(GLuint id, GLenum pname, GLint *params)
{
  m_Real.glGetQueryObjectivEXT(id, pname, params);
}

void WrappedGLES::glGetQueryiv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetQueryiv(target, pname, params);
}

void WrappedGLES::glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length,
                                GLint *values)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetSynciv(sync, pname, bufSize, length, values);
}

const GLubyte *WrappedGLES::glGetString(GLenum name)
{
  if(name == eGL_EXTENSIONS)
  {
    return (const GLubyte *)GetCtxData().glExtsString.c_str();
  }
  else if(name == eGL_DEBUG_TOOL_NAME_EXT)
  {
    return (const GLubyte *)"RenderDoc";
  }
  return m_Real.glGetString(name);
}

const GLubyte *WrappedGLES::glGetStringi(GLenum name, GLuint i)
{
  if(name == eGL_EXTENSIONS)
  {
    if((size_t)i < GetCtxData().glExts.size())
      return (const GLubyte *)GetCtxData().glExts[i].c_str();

    return (const GLubyte *)"";
  }
  else if(name == eGL_DEBUG_TOOL_NAME_EXT)
  {
    return (const GLubyte *)"RenderDoc";
  }
  return m_Real.glGetStringi(name, i);
}

void WrappedGLES::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
                                                          GLenum pname, GLint *params)
{
  m_Real.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

GLenum WrappedGLES::glCheckFramebufferStatus(GLenum target)
{
  return m_Real.glCheckFramebufferStatus(target);
}

void WrappedGLES::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetVertexAttribiv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
  m_Real.glGetVertexAttribPointerv(index, pname, pointer);
}

GLint WrappedGLES::glGetFragDataIndexEXT(GLuint program, const GLchar *name)
{
  return m_Real.glGetFragDataIndexEXT(program, name);
}

GLint WrappedGLES::glGetFragDataLocation(GLuint program, const GLchar *name)
{
  return m_Real.glGetFragDataLocation(program, name);
}

void WrappedGLES::glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
  m_Real.glGetMultisamplefv(pname, index, val);
}

void WrappedGLES::glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize,
                                     GLsizei *length, GLchar *label)
{
  m_Real.glGetObjectLabel(identifier, name, bufSize, length, label);
}

void WrappedGLES::glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length,
                                        GLchar *label)
{
  m_Real.glGetObjectPtrLabel(ptr, bufSize, length, label);
}

void WrappedGLES::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
  m_Real.glGetShaderiv(shader, pname, params);
}

void WrappedGLES::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length,
                                       GLchar *infoLog)
{
  m_Real.glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

void WrappedGLES::glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype,
                                               GLint *range, GLint *precision)
{
  m_Real.glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void WrappedGLES::glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
  m_Real.glGetShaderSource(shader, bufSize, length, source);
}

void WrappedGLES::glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count,
                                         GLuint *shaders)
{
  m_Real.glGetAttachedShaders(program, maxCount, count, shaders);
}

void WrappedGLES::glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
  m_Real.glGetProgramiv(program, pname, params);
}

void WrappedGLES::glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                                       GLenum *binaryFormat, void *binary)
{
  m_Real.glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void WrappedGLES::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length,
                                        GLchar *infoLog)
{
  m_Real.glGetProgramInfoLog(program, bufSize, length, infoLog);
}

void WrappedGLES::glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
  m_Real.glGetProgramPipelineiv(pipeline, pname, params);
}

void WrappedGLES::glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length,
                                                GLchar *infoLog)
{
  m_Real.glGetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void WrappedGLES::glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname,
                                            GLint *params)
{
  m_Real.glGetProgramInterfaceiv(program, programInterface, pname, params);
}

GLuint WrappedGLES::glGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                                const GLchar *name)
{
  return m_Real.glGetProgramResourceIndex(program, programInterface, name);
}

void WrappedGLES::glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index,
                                           GLsizei propCount, const GLenum *props, GLsizei bufSize,
                                           GLsizei *length, GLint *params)
{
  m_Real.glGetProgramResourceiv(program, programInterface, index, propCount, props, bufSize, length,
                                params);
}

void WrappedGLES::glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index,
                                             GLsizei bufSize, GLsizei *length, GLchar *name)
{
  m_Real.glGetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

GLint WrappedGLES::glGetProgramResourceLocation(GLuint program, GLenum programInterface,
                                                  const GLchar *name)
{
  return m_Real.glGetProgramResourceLocation(program, programInterface, name);
}

GLint WrappedGLES::glGetProgramResourceLocationIndexEXT(GLuint program, GLenum programInterface,
                                                       const GLchar *name)
{
  return m_Real.glGetProgramResourceLocationIndexEXT(program, programInterface, name);
}

GLint WrappedGLES::glGetUniformLocation(GLuint program, const GLchar *name)
{
  return m_Real.glGetUniformLocation(program, name);
}

void WrappedGLES::glGetUniformIndices(GLuint program, GLsizei uniformCount,
                                      const GLchar *const *uniformNames, GLuint *uniformIndices)
{
  m_Real.glGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

GLuint WrappedGLES::glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
  return m_Real.glGetUniformBlockIndex(program, uniformBlockName);
}

GLint WrappedGLES::glGetAttribLocation(GLuint program, const GLchar *name)
{
  return m_Real.glGetAttribLocation(program, name);
}

void WrappedGLES::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                                       GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  m_Real.glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void WrappedGLES::glGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                                          const GLuint *uniformIndices, GLenum pname, GLint *params)
{
  m_Real.glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

void WrappedGLES::glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex,
                                              GLenum pname, GLint *params)
{
  m_Real.glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void WrappedGLES::glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex,
                                                GLsizei bufSize, GLsizei *length,
                                                GLchar *uniformBlockName)
{
  m_Real.glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void WrappedGLES::glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize,
                                      GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  m_Real.glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void WrappedGLES::glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
  m_Real.glGetUniformfv(program, location, params);
}

void WrappedGLES::glGetUniformiv(GLuint program, GLint location, GLint *params)
{
  m_Real.glGetUniformiv(program, location, params);
}

void WrappedGLES::glGetUniformuiv(GLuint program, GLint location, GLuint *params)
{
  m_Real.glGetUniformuiv(program, location, params);
}

void WrappedGLES::glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
  m_Real.glGetnUniformfv(program, location, bufSize, params);
}

void WrappedGLES::glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
  m_Real.glGetnUniformiv(program, location, bufSize, params);
}

void WrappedGLES::glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params)
{
  m_Real.glGetnUniformuiv(program, location, bufSize, params);
}

void WrappedGLES::glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetVertexAttribIiv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
  m_Real.glGetVertexAttribIuiv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
  m_Real.glGetVertexAttribfv(index, pname, params);
}

void WrappedGLES::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                 GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glReadPixels(x, y, width, height, format, type, pixels);
}

void WrappedGLES::glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                  GLenum type, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glReadnPixels(x, y, width, height, format, type, bufSize, pixels);
}

void WrappedGLES::glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                                  GLsizei *length, GLsizei *size, GLenum *type,
                                                  GLchar *name)
{
  m_Real.glGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void WrappedGLES::glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  m_Real.glGetFramebufferParameteriv(target, pname, param);
}

void WrappedGLES::glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  m_Real.glGetRenderbufferParameteriv(target, pname, param);
}
