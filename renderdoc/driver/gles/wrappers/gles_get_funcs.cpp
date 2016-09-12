/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

GLboolean WrappedGLES::glIsNamedStringARB(GLint namelen, const GLchar *name)
{
  return m_Real.glIsNamedStringARB(namelen, name);
}

void WrappedGLES::glGetFloatv(GLenum pname, GLfloat *params)
{
  m_Real.glGetFloatv(pname, params);
}

void WrappedGLES::glGetDoublev(GLenum pname, GLdouble *params)
{
  m_Real.glGetDoublev(pname, params);
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
  if(pname == eGL_MIN_MAP_BUFFER_ALIGNMENT)
  {
    if(params)
      *params = (GLint)64;
    return;
  }
  else if(pname == eGL_NUM_EXTENSIONS)
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
  if(pname == eGL_MIN_MAP_BUFFER_ALIGNMENT)
  {
    if(data)
      *data = (GLint64)64;
    return;
  }
  else if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
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
  if(pname == eGL_MIN_MAP_BUFFER_ALIGNMENT)
  {
    if(data)
      *data = (GLint)64;
    return;
  }
  else if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
  {
    if(data)
      *data = GLint(eGL_DEBUG_TOOL_FRAME_CAPTURE_BIT_EXT);
    return;
  }
  m_Real.glGetIntegeri_v(pname, index, data);
}

void WrappedGLES::glGetFloati_v(GLenum pname, GLuint index, GLfloat *data)
{
  m_Real.glGetFloati_v(pname, index, data);
}

void WrappedGLES::glGetDoublei_v(GLenum pname, GLuint index, GLdouble *data)
{
  m_Real.glGetDoublei_v(pname, index, data);
}

void WrappedGLES::glGetInteger64i_v(GLenum pname, GLuint index, GLint64 *data)
{
  if(pname == eGL_MIN_MAP_BUFFER_ALIGNMENT)
  {
    if(data)
      *data = (GLint64)64;
    return;
  }
  else if(pname == eGL_DEBUG_TOOL_PURPOSE_EXT)
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

void WrappedGLES::glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname,
                                                 GLfloat *params)
{
  m_Real.glGetTextureLevelParameterfv(texture, level, pname, params);
}

void WrappedGLES::glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname,
                                                 GLint *params)
{
  m_Real.glGetTextureLevelParameteriv(texture, level, pname, params);
}

void WrappedGLES::glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params)
{
  m_Real.glGetTextureParameterIiv(texture, pname, params);
}

void WrappedGLES::glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params)
{
  m_Real.glGetTextureParameterIuiv(texture, pname, params);
}

void WrappedGLES::glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params)
{
  m_Real.glGetTextureParameterfv(texture, pname, params);
}

void WrappedGLES::glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params)
{
  m_Real.glGetTextureParameteriv(texture, pname, params);
}

void WrappedGLES::glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetTexParameterIiv(target, pname, params);
}

void WrappedGLES::glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params)
{
  m_Real.glGetTexParameterIuiv(target, pname, params);
}

void WrappedGLES::glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTexImage(target, level, format, type, pixels);
}

void WrappedGLES::glGetCompressedTexImage(GLenum target, GLint level, void *img)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTexImage(target, level, img);
}

void WrappedGLES::glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetnCompressedTexImage(target, lod, bufSize, pixels);
}

void WrappedGLES::glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize,
                                                void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTextureImage(texture, level, bufSize, pixels);
}

void WrappedGLES::glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset,
                                                   GLint yoffset, GLint zoffset, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLsizei bufSize,
                                                   void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height,
                                        depth, bufSize, pixels);
}

void WrappedGLES::glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type,
                                   GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetnTexImage(target, level, format, type, bufSize, pixels);
}

void WrappedGLES::glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                      GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTextureImage(texture, level, format, type, bufSize, pixels);
}

void WrappedGLES::glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                         GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                         GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth,
                              format, type, bufSize, pixels);
}

void WrappedGLES::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                          GLsizei bufSize, GLint *params)
{
  m_Real.glGetInternalformativ(target, internalformat, pname, bufSize, params);
}

void WrappedGLES::glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname,
                                            GLsizei bufSize, GLint64 *params)
{
  m_Real.glGetInternalformati64v(target, internalformat, pname, bufSize, params);
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

void WrappedGLES::glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetBufferSubData(target, offset, size, data);
}

void WrappedGLES::glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
  m_Real.glGetQueryObjectuiv(id, pname, params);
}

void WrappedGLES::glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
  m_Real.glGetQueryObjectui64v(id, pname, params);
}

void WrappedGLES::glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetQueryIndexediv(target, index, pname, params);
}

void WrappedGLES::glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
  m_Real.glGetQueryObjecti64v(id, pname, params);
}

void WrappedGLES::glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
  m_Real.glGetQueryObjectiv(id, pname, params);
}

void WrappedGLES::glGetQueryiv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetQueryiv(target, pname, params);
}

void WrappedGLES::glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname,
                                                GLintptr offset)
{
  m_Real.glGetQueryBufferObjectui64v(id, buffer, pname, offset);
}

void WrappedGLES::glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  m_Real.glGetQueryBufferObjectuiv(id, buffer, pname, offset);
}

void WrappedGLES::glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  m_Real.glGetQueryBufferObjecti64v(id, buffer, pname, offset);
}

void WrappedGLES::glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  m_Real.glGetQueryBufferObjectiv(id, buffer, pname, offset);
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

GLint WrappedGLES::glGetFragDataIndex(GLuint program, const GLchar *name)
{
  return m_Real.glGetFragDataIndex(program, name);
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

void WrappedGLES::glGetObjectLabelEXT(GLenum identifier, GLuint name, GLsizei bufSize,
                                        GLsizei *length, GLchar *label)
{
  m_Real.glGetObjectLabelEXT(identifier, name, bufSize, length, label);
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

void WrappedGLES::glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint *values)
{
  m_Real.glGetProgramStageiv(program, shadertype, pname, values);
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

GLint WrappedGLES::glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface,
                                                       const GLchar *name)
{
  return m_Real.glGetProgramResourceLocationIndex(program, programInterface, name);
}

void WrappedGLES::glGetNamedStringARB(GLint namelen, const GLchar *name, GLsizei bufSize,
                                        GLint *stringlen, GLchar *string)
{
  return m_Real.glGetNamedStringARB(namelen, name, bufSize, stringlen, string);
}

void WrappedGLES::glGetNamedStringivARB(GLint namelen, const GLchar *name, GLenum pname,
                                          GLint *params)
{
  return m_Real.glGetNamedStringivARB(namelen, name, pname, params);
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

GLuint WrappedGLES::glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name)
{
  return m_Real.glGetSubroutineIndex(program, shadertype, name);
}

GLint WrappedGLES::glGetSubroutineUniformLocation(GLuint program, GLenum shadertype,
                                                    const GLchar *name)
{
  return m_Real.glGetSubroutineUniformLocation(program, shadertype, name);
}

void WrappedGLES::glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params)
{
  m_Real.glGetUniformSubroutineuiv(shadertype, location, params);
}

void WrappedGLES::glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index,
                                              GLsizei bufsize, GLsizei *length, GLchar *name)
{
  m_Real.glGetActiveSubroutineName(program, shadertype, index, bufsize, length, name);
}

void WrappedGLES::glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index,
                                                     GLsizei bufsize, GLsizei *length, GLchar *name)
{
  m_Real.glGetActiveSubroutineUniformName(program, shadertype, index, bufsize, length, name);
}

void WrappedGLES::glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index,
                                                   GLenum pname, GLint *values)
{
  m_Real.glGetActiveSubroutineUniformiv(program, shadertype, index, pname, values);
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

void WrappedGLES::glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize,
                                           GLsizei *length, GLchar *uniformName)
{
  m_Real.glGetActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
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

void WrappedGLES::glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex,
                                                     GLenum pname, GLint *params)
{
  m_Real.glGetActiveAtomicCounterBufferiv(program, bufferIndex, pname, params);
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

void WrappedGLES::glGetUniformdv(GLuint program, GLint location, GLdouble *params)
{
  m_Real.glGetUniformdv(program, location, params);
}

void WrappedGLES::glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params)
{
  m_Real.glGetnUniformdv(program, location, bufSize, params);
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

void WrappedGLES::glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param)
{
  m_Real.glGetVertexArrayiv(vaobj, pname, param);
}

void WrappedGLES::glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname,
                                                GLint64 *param)
{
  m_Real.glGetVertexArrayIndexed64iv(vaobj, index, pname, param);
}

void WrappedGLES::glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
  m_Real.glGetVertexArrayIndexediv(vaobj, index, pname, param);
}

void WrappedGLES::glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetVertexAttribIiv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
  m_Real.glGetVertexAttribIuiv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params)
{
  m_Real.glGetVertexAttribLdv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params)
{
  m_Real.glGetVertexAttribdv(index, pname, params);
}

void WrappedGLES::glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
  m_Real.glGetVertexAttribfv(index, pname, params);
}

void WrappedGLES::glClampColor(GLenum target, GLenum clamp)
{
  m_Real.glClampColor(target, clamp);
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

void WrappedGLES::glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param)
{
  m_Real.glGetTransformFeedbacki64_v(xfb, pname, index, param);
}

void WrappedGLES::glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param)
{
  m_Real.glGetTransformFeedbacki_v(xfb, pname, index, param);
}

void WrappedGLES::glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param)
{
  m_Real.glGetTransformFeedbackiv(xfb, pname, param);
}

void WrappedGLES::glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  m_Real.glGetFramebufferParameteriv(target, pname, param);
}

void WrappedGLES::glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  m_Real.glGetRenderbufferParameteriv(target, pname, param);
}

void WrappedGLES::glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params)
{
  m_Real.glGetNamedBufferParameteri64v(buffer, pname, params);
}

void WrappedGLES::glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint *param)
{
  m_Real.glGetNamedFramebufferParameterivEXT(framebuffer, pname, param);
}

void WrappedGLES::glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer,
                                                                  GLenum attachment, GLenum pname,
                                                                  GLint *params)
{
  m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment, pname, params);
}

void WrappedGLES::glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname,
                                                         GLint *params)
{
  m_Real.glGetNamedRenderbufferParameterivEXT(renderbuffer, pname, params);
}

void WrappedGLES::glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format,
                                         GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTextureImageEXT(texture, target, level, format, type, pixels);
}

void WrappedGLES::glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint level,
                                                   void *img)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTextureImageEXT(texture, target, level, img);
}

GLenum WrappedGLES::glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target)
{
  return m_Real.glCheckNamedFramebufferStatusEXT(framebuffer, target);
}

void WrappedGLES::glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params)
{
  m_Real.glGetNamedBufferParameterivEXT(buffer, pname, params);
}

void WrappedGLES::glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                               void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetNamedBufferSubDataEXT(buffer, offset, size, data);
}

void WrappedGLES::glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                            void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetNamedBufferSubData(buffer, offset, size, data);
}

void WrappedGLES::glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                               GLint *params)
{
  m_Real.glGetTextureParameterivEXT(texture, target, pname, params);
}

void WrappedGLES::glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                               GLfloat *params)
{
  m_Real.glGetTextureParameterfvEXT(texture, target, pname, params);
}

void WrappedGLES::glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                                GLint *params)
{
  m_Real.glGetTextureParameterIivEXT(texture, target, pname, params);
}

void WrappedGLES::glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                                 GLuint *params)
{
  m_Real.glGetTextureParameterIuivEXT(texture, target, pname, params);
}

void WrappedGLES::glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level,
                                                    GLenum pname, GLint *params)
{
  m_Real.glGetTextureLevelParameterivEXT(texture, target, level, pname, params);
}

void WrappedGLES::glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level,
                                                    GLenum pname, GLfloat *params)
{
  m_Real.glGetTextureLevelParameterfvEXT(texture, target, level, pname, params);
}

void WrappedGLES::glGetPointeri_vEXT(GLenum pname, GLuint index, void **params)
{
  if(pname == eGL_DEBUG_CALLBACK_FUNCTION)
    *params = (void *)m_RealDebugFunc;
  else if(pname == eGL_DEBUG_CALLBACK_USER_PARAM)
    *params = (void *)m_RealDebugFuncParam;
  else
    m_Real.glGetPointeri_vEXT(pname, index, params);
}

void WrappedGLES::glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble *data)
{
  m_Real.glGetDoubleIndexedvEXT(target, index, data);
}

void WrappedGLES::glGetPointerIndexedvEXT(GLenum target, GLuint index, void **data)
{
  m_Real.glGetPointerIndexedvEXT(target, index, data);
}

void WrappedGLES::glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data)
{
  m_Real.glGetIntegerIndexedvEXT(target, index, data);
}

void WrappedGLES::glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data)
{
  m_Real.glGetBooleanIndexedvEXT(target, index, data);
}

void WrappedGLES::glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat *data)
{
  m_Real.glGetFloatIndexedvEXT(target, index, data);
}

void WrappedGLES::glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format,
                                          GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetMultiTexImageEXT(texunit, target, level, format, type, pixels);
}

void WrappedGLES::glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                                GLfloat *params)
{
  m_Real.glGetMultiTexParameterfvEXT(texunit, target, pname, params);
}

void WrappedGLES::glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                GLint *params)
{
  m_Real.glGetMultiTexParameterivEXT(texunit, target, pname, params);
}

void WrappedGLES::glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                 GLint *params)
{
  m_Real.glGetMultiTexParameterIivEXT(texunit, target, pname, params);
}

void WrappedGLES::glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                  GLuint *params)
{
  m_Real.glGetMultiTexParameterIuivEXT(texunit, target, pname, params);
}

void WrappedGLES::glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level,
                                                     GLenum pname, GLfloat *params)
{
  m_Real.glGetMultiTexLevelParameterfvEXT(texunit, target, level, pname, params);
}

void WrappedGLES::glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level,
                                                     GLenum pname, GLint *params)
{
  m_Real.glGetMultiTexLevelParameterivEXT(texunit, target, level, pname, params);
}

void WrappedGLES::glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod,
                                                    void *img)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedMultiTexImageEXT(texunit, target, lod, img);
}

void WrappedGLES::glGetNamedBufferPointervEXT(GLuint buffer, GLenum pname, void **params)
{
  CoherentMapImplicitBarrier();

  // intercept GL_BUFFER_MAP_POINTER queries
  if(pname == eGL_BUFFER_MAP_POINTER)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
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
    m_Real.glGetNamedBufferPointervEXT(buffer, pname, params);
  }
}

void WrappedGLES::glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetNamedProgramivEXT(program, target, pname, params);
}

void WrappedGLES::glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint *param)
{
  m_Real.glGetVertexArrayIntegervEXT(vaobj, pname, param);
}

void WrappedGLES::glGetVertexArrayPointervEXT(GLuint vaobj, GLenum pname, void **param)
{
  m_Real.glGetVertexArrayPointervEXT(vaobj, pname, param);
}

void WrappedGLES::glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname,
                                                  GLint *param)
{
  m_Real.glGetVertexArrayIntegeri_vEXT(vaobj, index, pname, param);
}

void WrappedGLES::glGetVertexArrayPointeri_vEXT(GLuint vaobj, GLuint index, GLenum pname,
                                                  void **param)
{
  m_Real.glGetVertexArrayPointeri_vEXT(vaobj, index, pname, param);
}
