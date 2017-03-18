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

GLenum WrappedOpenGL::glGetError()
{
  return m_Real.glGetError();
}

GLenum WrappedOpenGL::glGetGraphicsResetStatus()
{
  return m_Real.glGetGraphicsResetStatus();
}

GLuint WrappedOpenGL::glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources,
                                           GLenum *types, GLuint *ids, GLenum *severities,
                                           GLsizei *lengths, GLchar *messageLog)
{
  return m_Real.glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths,
                                     messageLog);
}

void WrappedOpenGL::glFlush()
{
  CoherentMapImplicitBarrier();

  m_Real.glFlush();
}

void WrappedOpenGL::glFinish()
{
  CoherentMapImplicitBarrier();

  m_Real.glFinish();
}

GLboolean WrappedOpenGL::glIsEnabled(GLenum cap)
{
  if(cap == eGL_DEBUG_TOOL_EXT)
    return true;

  return m_Real.glIsEnabled(cap);
}

GLboolean WrappedOpenGL::glIsTexture(GLuint texture)
{
  return m_Real.glIsTexture(texture);
}

GLboolean WrappedOpenGL::glIsEnabledi(GLenum target, GLuint index)
{
  if(target == eGL_DEBUG_TOOL_EXT)
    return true;

  return m_Real.glIsEnabledi(target, index);
}

GLboolean WrappedOpenGL::glIsBuffer(GLuint buffer)
{
  return m_Real.glIsBuffer(buffer);
}

GLboolean WrappedOpenGL::glIsFramebuffer(GLuint framebuffer)
{
  return m_Real.glIsFramebuffer(framebuffer);
}

GLboolean WrappedOpenGL::glIsProgram(GLuint program)
{
  return m_Real.glIsProgram(program);
}

GLboolean WrappedOpenGL::glIsProgramPipeline(GLuint pipeline)
{
  return m_Real.glIsProgramPipeline(pipeline);
}

GLboolean WrappedOpenGL::glIsQuery(GLuint id)
{
  return m_Real.glIsQuery(id);
}

GLboolean WrappedOpenGL::glIsRenderbuffer(GLuint renderbuffer)
{
  return m_Real.glIsRenderbuffer(renderbuffer);
}

GLboolean WrappedOpenGL::glIsSampler(GLuint sampler)
{
  return m_Real.glIsSampler(sampler);
}

GLboolean WrappedOpenGL::glIsShader(GLuint shader)
{
  return m_Real.glIsShader(shader);
}

GLboolean WrappedOpenGL::glIsSync(GLsync sync)
{
  return m_Real.glIsSync(sync);
}

GLboolean WrappedOpenGL::glIsTransformFeedback(GLuint id)
{
  return m_Real.glIsTransformFeedback(id);
}

GLboolean WrappedOpenGL::glIsVertexArray(GLuint array)
{
  return m_Real.glIsVertexArray(array);
}

GLboolean WrappedOpenGL::glIsNamedStringARB(GLint namelen, const GLchar *name)
{
  return m_Real.glIsNamedStringARB(namelen, name);
}

void WrappedOpenGL::glGetFloatv(GLenum pname, GLfloat *params)
{
  m_Real.glGetFloatv(pname, params);
}

void WrappedOpenGL::glGetDoublev(GLenum pname, GLdouble *params)
{
  m_Real.glGetDoublev(pname, params);
}

void WrappedOpenGL::glGetPointerv(GLenum pname, void **params)
{
  if(pname == eGL_DEBUG_CALLBACK_FUNCTION)
    *params = (void *)m_RealDebugFunc;
  else if(pname == eGL_DEBUG_CALLBACK_USER_PARAM)
    *params = (void *)m_RealDebugFuncParam;
  else
    m_Real.glGetPointerv(pname, params);
}

void WrappedOpenGL::glGetIntegerv(GLenum pname, GLint *params)
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

void WrappedOpenGL::glGetBooleanv(GLenum pname, GLboolean *data)
{
  m_Real.glGetBooleanv(pname, data);
}

void WrappedOpenGL::glGetInteger64v(GLenum pname, GLint64 *data)
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

void WrappedOpenGL::glGetBooleani_v(GLenum pname, GLuint index, GLboolean *data)
{
  m_Real.glGetBooleani_v(pname, index, data);
}

void WrappedOpenGL::glGetIntegeri_v(GLenum pname, GLuint index, GLint *data)
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

void WrappedOpenGL::glGetFloati_v(GLenum pname, GLuint index, GLfloat *data)
{
  m_Real.glGetFloati_v(pname, index, data);
}

void WrappedOpenGL::glGetDoublei_v(GLenum pname, GLuint index, GLdouble *data)
{
  m_Real.glGetDoublei_v(pname, index, data);
}

void WrappedOpenGL::glGetInteger64i_v(GLenum pname, GLuint index, GLint64 *data)
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

void WrappedOpenGL::glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
  m_Real.glGetTexLevelParameteriv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
  m_Real.glGetTexLevelParameterfv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
  m_Real.glGetTexParameterfv(target, pname, params);
}

void WrappedOpenGL::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetTexParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname,
                                                 GLfloat *params)
{
  m_Real.glGetTextureLevelParameterfv(texture, level, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname,
                                                 GLint *params)
{
  m_Real.glGetTextureLevelParameteriv(texture, level, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params)
{
  m_Real.glGetTextureParameterIiv(texture, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params)
{
  m_Real.glGetTextureParameterIuiv(texture, pname, params);
}

void WrappedOpenGL::glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params)
{
  m_Real.glGetTextureParameterfv(texture, pname, params);
}

void WrappedOpenGL::glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params)
{
  m_Real.glGetTextureParameteriv(texture, pname, params);
}

void WrappedOpenGL::glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetTexParameterIiv(target, pname, params);
}

void WrappedOpenGL::glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params)
{
  m_Real.glGetTexParameterIuiv(target, pname, params);
}

void WrappedOpenGL::glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTexImage(target, level, format, type, pixels);
}

void WrappedOpenGL::glGetCompressedTexImage(GLenum target, GLint level, void *img)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTexImage(target, level, img);
}

void WrappedOpenGL::glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetnCompressedTexImage(target, lod, bufSize, pixels);
}

void WrappedOpenGL::glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize,
                                                void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTextureImage(texture, level, bufSize, pixels);
}

void WrappedOpenGL::glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset,
                                                   GLint yoffset, GLint zoffset, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLsizei bufSize,
                                                   void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height,
                                        depth, bufSize, pixels);
}

void WrappedOpenGL::glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type,
                                   GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetnTexImage(target, level, format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                      GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTextureImage(texture, level, format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                         GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                         GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth,
                              format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                          GLsizei bufSize, GLint *params)
{
  m_Real.glGetInternalformativ(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname,
                                            GLsizei bufSize, GLint64 *params)
{
  m_Real.glGetInternalformati64v(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params)
{
  m_Real.glGetSamplerParameterIiv(sampler, pname, params);
}

void WrappedOpenGL::glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params)
{
  m_Real.glGetSamplerParameterIuiv(sampler, pname, params);
}

void WrappedOpenGL::glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
  m_Real.glGetSamplerParameterfv(sampler, pname, params);
}

void WrappedOpenGL::glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
  m_Real.glGetSamplerParameteriv(sampler, pname, params);
}

void WrappedOpenGL::glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params)
{
  m_Real.glGetBufferParameteri64v(target, pname, params);
}

void WrappedOpenGL::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetBufferParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetBufferPointerv(GLenum target, GLenum pname, void **params)
{
  CoherentMapImplicitBarrier();

  // intercept GL_BUFFER_MAP_POINTER queries
  if(pname == eGL_BUFFER_MAP_POINTER)
  {
    GLResourceRecord *record = GetCtxData().m_BufferRecord[BufferIdx(target)];
    RDCASSERTMSG("Couldn't identify implicit object at binding. Mismatched or bad GLuint?", record,
                 target);

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

void WrappedOpenGL::glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetBufferSubData(target, offset, size, data);
}

void WrappedOpenGL::glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
  m_Real.glGetQueryObjectuiv(id, pname, params);
}

void WrappedOpenGL::glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
  m_Real.glGetQueryObjectui64v(id, pname, params);
}

void WrappedOpenGL::glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetQueryIndexediv(target, index, pname, params);
}

void WrappedOpenGL::glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
  m_Real.glGetQueryObjecti64v(id, pname, params);
}

void WrappedOpenGL::glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
  m_Real.glGetQueryObjectiv(id, pname, params);
}

void WrappedOpenGL::glGetQueryiv(GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetQueryiv(target, pname, params);
}

void WrappedOpenGL::glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname,
                                                GLintptr offset)
{
  m_Real.glGetQueryBufferObjectui64v(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  m_Real.glGetQueryBufferObjectuiv(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  m_Real.glGetQueryBufferObjecti64v(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  m_Real.glGetQueryBufferObjectiv(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length,
                                GLint *values)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetSynciv(sync, pname, bufSize, length, values);
}

const GLubyte *WrappedOpenGL::glGetString(GLenum name)
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

const GLubyte *WrappedOpenGL::glGetStringi(GLenum name, GLuint i)
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

void WrappedOpenGL::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
                                                          GLenum pname, GLint *params)
{
  m_Real.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

GLenum WrappedOpenGL::glCheckFramebufferStatus(GLenum target)
{
  return m_Real.glCheckFramebufferStatus(target);
}

void WrappedOpenGL::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetVertexAttribiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
  m_Real.glGetVertexAttribPointerv(index, pname, pointer);
}

GLint WrappedOpenGL::glGetFragDataIndex(GLuint program, const GLchar *name)
{
  return m_Real.glGetFragDataIndex(program, name);
}

GLint WrappedOpenGL::glGetFragDataLocation(GLuint program, const GLchar *name)
{
  return m_Real.glGetFragDataLocation(program, name);
}

void WrappedOpenGL::glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
  m_Real.glGetMultisamplefv(pname, index, val);
}

void WrappedOpenGL::glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize,
                                     GLsizei *length, GLchar *label)
{
  m_Real.glGetObjectLabel(identifier, name, bufSize, length, label);
}

void WrappedOpenGL::glGetObjectLabelEXT(GLenum identifier, GLuint name, GLsizei bufSize,
                                        GLsizei *length, GLchar *label)
{
  m_Real.glGetObjectLabelEXT(identifier, name, bufSize, length, label);
}

void WrappedOpenGL::glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length,
                                        GLchar *label)
{
  m_Real.glGetObjectPtrLabel(ptr, bufSize, length, label);
}

void WrappedOpenGL::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
  m_Real.glGetShaderiv(shader, pname, params);
}

void WrappedOpenGL::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length,
                                       GLchar *infoLog)
{
  m_Real.glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype,
                                               GLint *range, GLint *precision)
{
  m_Real.glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void WrappedOpenGL::glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
  m_Real.glGetShaderSource(shader, bufSize, length, source);
}

void WrappedOpenGL::glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count,
                                         GLuint *shaders)
{
  m_Real.glGetAttachedShaders(program, maxCount, count, shaders);
}

void WrappedOpenGL::glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
  m_Real.glGetProgramiv(program, pname, params);
}

void WrappedOpenGL::glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint *values)
{
  m_Real.glGetProgramStageiv(program, shadertype, pname, values);
}

void WrappedOpenGL::glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                                       GLenum *binaryFormat, void *binary)
{
  m_Real.glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void WrappedOpenGL::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length,
                                        GLchar *infoLog)
{
  m_Real.glGetProgramInfoLog(program, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
  m_Real.glGetProgramPipelineiv(pipeline, pname, params);
}

void WrappedOpenGL::glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length,
                                                GLchar *infoLog)
{
  m_Real.glGetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname,
                                            GLint *params)
{
  m_Real.glGetProgramInterfaceiv(program, programInterface, pname, params);
}

GLuint WrappedOpenGL::glGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                                const GLchar *name)
{
  return m_Real.glGetProgramResourceIndex(program, programInterface, name);
}

void WrappedOpenGL::glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index,
                                           GLsizei propCount, const GLenum *props, GLsizei bufSize,
                                           GLsizei *length, GLint *params)
{
  m_Real.glGetProgramResourceiv(program, programInterface, index, propCount, props, bufSize, length,
                                params);
}

void WrappedOpenGL::glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index,
                                             GLsizei bufSize, GLsizei *length, GLchar *name)
{
  m_Real.glGetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

GLint WrappedOpenGL::glGetProgramResourceLocation(GLuint program, GLenum programInterface,
                                                  const GLchar *name)
{
  return m_Real.glGetProgramResourceLocation(program, programInterface, name);
}

GLint WrappedOpenGL::glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface,
                                                       const GLchar *name)
{
  return m_Real.glGetProgramResourceLocationIndex(program, programInterface, name);
}

void WrappedOpenGL::glGetNamedStringARB(GLint namelen, const GLchar *name, GLsizei bufSize,
                                        GLint *stringlen, GLchar *string)
{
  return m_Real.glGetNamedStringARB(namelen, name, bufSize, stringlen, string);
}

void WrappedOpenGL::glGetNamedStringivARB(GLint namelen, const GLchar *name, GLenum pname,
                                          GLint *params)
{
  return m_Real.glGetNamedStringivARB(namelen, name, pname, params);
}

GLint WrappedOpenGL::glGetUniformLocation(GLuint program, const GLchar *name)
{
  return m_Real.glGetUniformLocation(program, name);
}

void WrappedOpenGL::glGetUniformIndices(GLuint program, GLsizei uniformCount,
                                        const GLchar *const *uniformNames, GLuint *uniformIndices)
{
  m_Real.glGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

GLuint WrappedOpenGL::glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
  return m_Real.glGetUniformBlockIndex(program, uniformBlockName);
}

GLint WrappedOpenGL::glGetAttribLocation(GLuint program, const GLchar *name)
{
  return m_Real.glGetAttribLocation(program, name);
}

GLuint WrappedOpenGL::glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name)
{
  return m_Real.glGetSubroutineIndex(program, shadertype, name);
}

GLint WrappedOpenGL::glGetSubroutineUniformLocation(GLuint program, GLenum shadertype,
                                                    const GLchar *name)
{
  return m_Real.glGetSubroutineUniformLocation(program, shadertype, name);
}

void WrappedOpenGL::glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params)
{
  m_Real.glGetUniformSubroutineuiv(shadertype, location, params);
}

void WrappedOpenGL::glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index,
                                              GLsizei bufsize, GLsizei *length, GLchar *name)
{
  m_Real.glGetActiveSubroutineName(program, shadertype, index, bufsize, length, name);
}

void WrappedOpenGL::glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index,
                                                     GLsizei bufsize, GLsizei *length, GLchar *name)
{
  m_Real.glGetActiveSubroutineUniformName(program, shadertype, index, bufsize, length, name);
}

void WrappedOpenGL::glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index,
                                                   GLenum pname, GLint *values)
{
  m_Real.glGetActiveSubroutineUniformiv(program, shadertype, index, pname, values);
}

void WrappedOpenGL::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                                       GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  m_Real.glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                                          const GLuint *uniformIndices, GLenum pname, GLint *params)
{
  m_Real.glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

void WrappedOpenGL::glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize,
                                           GLsizei *length, GLchar *uniformName)
{
  m_Real.glGetActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
}

void WrappedOpenGL::glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex,
                                              GLenum pname, GLint *params)
{
  m_Real.glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void WrappedOpenGL::glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex,
                                                GLsizei bufSize, GLsizei *length,
                                                GLchar *uniformBlockName)
{
  m_Real.glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void WrappedOpenGL::glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize,
                                      GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  m_Real.glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex,
                                                     GLenum pname, GLint *params)
{
  m_Real.glGetActiveAtomicCounterBufferiv(program, bufferIndex, pname, params);
}

void WrappedOpenGL::glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
  m_Real.glGetUniformfv(program, location, params);
}

void WrappedOpenGL::glGetUniformiv(GLuint program, GLint location, GLint *params)
{
  m_Real.glGetUniformiv(program, location, params);
}

void WrappedOpenGL::glGetUniformuiv(GLuint program, GLint location, GLuint *params)
{
  m_Real.glGetUniformuiv(program, location, params);
}

void WrappedOpenGL::glGetUniformdv(GLuint program, GLint location, GLdouble *params)
{
  m_Real.glGetUniformdv(program, location, params);
}

void WrappedOpenGL::glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params)
{
  m_Real.glGetnUniformdv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
  m_Real.glGetnUniformfv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
  m_Real.glGetnUniformiv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params)
{
  m_Real.glGetnUniformuiv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param)
{
  m_Real.glGetVertexArrayiv(vaobj, pname, param);
}

void WrappedOpenGL::glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname,
                                                GLint64 *param)
{
  m_Real.glGetVertexArrayIndexed64iv(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
  m_Real.glGetVertexArrayIndexediv(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
  m_Real.glGetVertexAttribIiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
  m_Real.glGetVertexAttribIuiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params)
{
  m_Real.glGetVertexAttribLdv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params)
{
  m_Real.glGetVertexAttribdv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
  m_Real.glGetVertexAttribfv(index, pname, params);
}

void WrappedOpenGL::glClampColor(GLenum target, GLenum clamp)
{
  m_Real.glClampColor(target, clamp);
}

void WrappedOpenGL::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                 GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glReadPixels(x, y, width, height, format, type, pixels);
}

void WrappedOpenGL::glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                  GLenum type, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glReadnPixels(x, y, width, height, format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                                  GLsizei *length, GLsizei *size, GLenum *type,
                                                  GLchar *name)
{
  m_Real.glGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param)
{
  m_Real.glGetTransformFeedbacki64_v(xfb, pname, index, param);
}

void WrappedOpenGL::glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param)
{
  m_Real.glGetTransformFeedbacki_v(xfb, pname, index, param);
}

void WrappedOpenGL::glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param)
{
  m_Real.glGetTransformFeedbackiv(xfb, pname, param);
}

void WrappedOpenGL::glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  m_Real.glGetFramebufferParameteriv(target, pname, param);
}

void WrappedOpenGL::glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  m_Real.glGetRenderbufferParameteriv(target, pname, param);
}

void WrappedOpenGL::glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params)
{
  m_Real.glGetNamedBufferParameteri64v(buffer, pname, params);
}

void WrappedOpenGL::glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint *param)
{
  m_Real.glGetNamedFramebufferParameterivEXT(framebuffer, pname, param);
}

void WrappedOpenGL::glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer,
                                                                  GLenum attachment, GLenum pname,
                                                                  GLint *params)
{
  m_Real.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment, pname, params);
}

void WrappedOpenGL::glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname,
                                                         GLint *params)
{
  m_Real.glGetNamedRenderbufferParameterivEXT(renderbuffer, pname, params);
}

void WrappedOpenGL::glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format,
                                         GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetTextureImageEXT(texture, target, level, format, type, pixels);
}

void WrappedOpenGL::glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint level,
                                                   void *img)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedTextureImageEXT(texture, target, level, img);
}

GLenum WrappedOpenGL::glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target)
{
  return m_Real.glCheckNamedFramebufferStatusEXT(framebuffer, target);
}

void WrappedOpenGL::glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params)
{
  m_Real.glGetNamedBufferParameterivEXT(buffer, pname, params);
}

void WrappedOpenGL::glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                               void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetNamedBufferSubDataEXT(buffer, offset, size, data);
}

void WrappedOpenGL::glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                            void *data)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetNamedBufferSubData(buffer, offset, size, data);
}

void WrappedOpenGL::glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                               GLint *params)
{
  m_Real.glGetTextureParameterivEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                               GLfloat *params)
{
  m_Real.glGetTextureParameterfvEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                                GLint *params)
{
  m_Real.glGetTextureParameterIivEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                                 GLuint *params)
{
  m_Real.glGetTextureParameterIuivEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level,
                                                    GLenum pname, GLint *params)
{
  m_Real.glGetTextureLevelParameterivEXT(texture, target, level, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level,
                                                    GLenum pname, GLfloat *params)
{
  m_Real.glGetTextureLevelParameterfvEXT(texture, target, level, pname, params);
}

void WrappedOpenGL::glGetPointeri_vEXT(GLenum pname, GLuint index, void **params)
{
  if(pname == eGL_DEBUG_CALLBACK_FUNCTION)
    *params = (void *)m_RealDebugFunc;
  else if(pname == eGL_DEBUG_CALLBACK_USER_PARAM)
    *params = (void *)m_RealDebugFuncParam;
  else
    m_Real.glGetPointeri_vEXT(pname, index, params);
}

void WrappedOpenGL::glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble *data)
{
  m_Real.glGetDoubleIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetPointerIndexedvEXT(GLenum target, GLuint index, void **data)
{
  m_Real.glGetPointerIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data)
{
  m_Real.glGetIntegerIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data)
{
  m_Real.glGetBooleanIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat *data)
{
  m_Real.glGetFloatIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format,
                                          GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetMultiTexImageEXT(texunit, target, level, format, type, pixels);
}

void WrappedOpenGL::glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                                GLfloat *params)
{
  m_Real.glGetMultiTexParameterfvEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                GLint *params)
{
  m_Real.glGetMultiTexParameterivEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                 GLint *params)
{
  m_Real.glGetMultiTexParameterIivEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                  GLuint *params)
{
  m_Real.glGetMultiTexParameterIuivEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level,
                                                     GLenum pname, GLfloat *params)
{
  m_Real.glGetMultiTexLevelParameterfvEXT(texunit, target, level, pname, params);
}

void WrappedOpenGL::glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level,
                                                     GLenum pname, GLint *params)
{
  m_Real.glGetMultiTexLevelParameterivEXT(texunit, target, level, pname, params);
}

void WrappedOpenGL::glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod,
                                                    void *img)
{
  CoherentMapImplicitBarrier();

  m_Real.glGetCompressedMultiTexImageEXT(texunit, target, lod, img);
}

void WrappedOpenGL::glGetNamedBufferPointervEXT(GLuint buffer, GLenum pname, void **params)
{
  CoherentMapImplicitBarrier();

  // intercept GL_BUFFER_MAP_POINTER queries
  if(pname == eGL_BUFFER_MAP_POINTER)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 buffer);

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

void WrappedOpenGL::glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint *params)
{
  m_Real.glGetNamedProgramivEXT(program, target, pname, params);
}

void WrappedOpenGL::glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint *param)
{
  m_Real.glGetVertexArrayIntegervEXT(vaobj, pname, param);
}

void WrappedOpenGL::glGetVertexArrayPointervEXT(GLuint vaobj, GLenum pname, void **param)
{
  m_Real.glGetVertexArrayPointervEXT(vaobj, pname, param);
}

void WrappedOpenGL::glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname,
                                                  GLint *param)
{
  m_Real.glGetVertexArrayIntegeri_vEXT(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetVertexArrayPointeri_vEXT(GLuint vaobj, GLuint index, GLenum pname,
                                                  void **param)
{
  m_Real.glGetVertexArrayPointeri_vEXT(vaobj, index, pname, param);
}
