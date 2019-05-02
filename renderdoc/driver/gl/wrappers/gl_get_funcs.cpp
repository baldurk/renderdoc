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

GLenum WrappedOpenGL::glGetError()
{
  return GL.glGetError();
}

GLenum WrappedOpenGL::glGetGraphicsResetStatus()
{
  return GL.glGetGraphicsResetStatus();
}

GLboolean WrappedOpenGL::glIsEnabled(GLenum cap)
{
  if(cap == eGL_DEBUG_TOOL_EXT)
    return true;

  if(!HasExt[KHR_debug])
  {
    if(cap == eGL_DEBUG_OUTPUT)
      return false;
    if(cap == eGL_DEBUG_OUTPUT_SYNCHRONOUS)
      return false;
  }

  return GL.glIsEnabled(cap);
}

GLboolean WrappedOpenGL::glIsTexture(GLuint texture)
{
  return GL.glIsTexture(texture);
}

GLboolean WrappedOpenGL::glIsEnabledi(GLenum target, GLuint index)
{
  if(target == eGL_DEBUG_TOOL_EXT)
    return true;

  return GL.glIsEnabledi(target, index);
}

GLboolean WrappedOpenGL::glIsBuffer(GLuint buffer)
{
  return GL.glIsBuffer(buffer);
}

GLboolean WrappedOpenGL::glIsFramebuffer(GLuint framebuffer)
{
  return GL.glIsFramebuffer(framebuffer);
}

GLboolean WrappedOpenGL::glIsProgram(GLuint program)
{
  return GL.glIsProgram(program);
}

GLboolean WrappedOpenGL::glIsProgramPipeline(GLuint pipeline)
{
  return GL.glIsProgramPipeline(pipeline);
}

GLboolean WrappedOpenGL::glIsQuery(GLuint id)
{
  return GL.glIsQuery(id);
}

GLboolean WrappedOpenGL::glIsRenderbuffer(GLuint renderbuffer)
{
  return GL.glIsRenderbuffer(renderbuffer);
}

GLboolean WrappedOpenGL::glIsSampler(GLuint sampler)
{
  return GL.glIsSampler(sampler);
}

GLboolean WrappedOpenGL::glIsShader(GLuint shader)
{
  return GL.glIsShader(shader);
}

GLboolean WrappedOpenGL::glIsSync(GLsync sync)
{
  return GL.glIsSync(sync);
}

GLboolean WrappedOpenGL::glIsTransformFeedback(GLuint id)
{
  return GL.glIsTransformFeedback(id);
}

GLboolean WrappedOpenGL::glIsVertexArray(GLuint array)
{
  return GL.glIsVertexArray(array);
}

GLboolean WrappedOpenGL::glIsNamedStringARB(GLint namelen, const GLchar *name)
{
  return GL.glIsNamedStringARB(namelen, name);
}

GLboolean WrappedOpenGL::glIsMemoryObjectEXT(GLuint memoryObject)
{
  return GL.glIsMemoryObjectEXT(memoryObject);
}

GLboolean WrappedOpenGL::glIsSemaphoreEXT(GLuint semaphore)
{
  return GL.glIsSemaphoreEXT(semaphore);
}

void WrappedOpenGL::glGetFloatv(GLenum pname, GLfloat *params)
{
  if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(params)
          *params = GLfloat(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(params)
          *params = GLfloat(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(params)
          *params = GLfloat(1);
        return;
      default: break;
    }
  }

  GL.glGetFloatv(pname, params);
}

void WrappedOpenGL::glGetDoublev(GLenum pname, GLdouble *params)
{
  if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(params)
          *params = GLdouble(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(params)
          *params = GLdouble(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(params)
          *params = GLdouble(1);
        return;
      default: break;
    }
  }

  GL.glGetDoublev(pname, params);
}

void WrappedOpenGL::glGetPointerv(GLenum pname, void **params)
{
  if(pname == eGL_DEBUG_CALLBACK_FUNCTION)
    *params = (void *)GetCtxData().m_RealDebugFunc;
  else if(pname == eGL_DEBUG_CALLBACK_USER_PARAM)
    *params = (void *)GetCtxData().m_RealDebugFuncParam;
  else
    GL.glGetPointerv(pname, params);
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
  else if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(params)
          *params = GLint(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(params)
          *params = GLint(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(params)
          *params = GLint(1);
        return;
      default: break;
    }
  }

  GL.glGetIntegerv(pname, params);
}

void WrappedOpenGL::glGetBooleanv(GLenum pname, GLboolean *data)
{
  if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLboolean(1);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLboolean(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLboolean(1);
        return;
      default: break;
    }
  }

  GL.glGetBooleanv(pname, data);
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
  else if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLint64(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLint64(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLint64(1);
        return;
      default: break;
    }
  }

  GL.glGetInteger64v(pname, data);
}

void WrappedOpenGL::glGetBooleani_v(GLenum pname, GLuint index, GLboolean *data)
{
  if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLboolean(1);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLboolean(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLboolean(1);
        return;
      default: break;
    }
  }

  GL.glGetBooleani_v(pname, index, data);
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
  else if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLint(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLint(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLint(1);
        return;
      default: break;
    }
  }

  GL.glGetIntegeri_v(pname, index, data);
}

void WrappedOpenGL::glGetFloati_v(GLenum pname, GLuint index, GLfloat *data)
{
  if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLfloat(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLfloat(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLfloat(1);
        return;
      default: break;
    }
  }

  GL.glGetFloati_v(pname, index, data);
}

void WrappedOpenGL::glGetDoublei_v(GLenum pname, GLuint index, GLdouble *data)
{
  if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLdouble(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLdouble(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLdouble(1);
        return;
      default: break;
    }
  }

  GL.glGetDoublei_v(pname, index, data);
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
  else if(!HasExt[KHR_debug])
  {
    switch(pname)
    {
      case eGL_MAX_LABEL_LENGTH:
      case eGL_MAX_DEBUG_MESSAGE_LENGTH:
      case eGL_MAX_DEBUG_LOGGED_MESSAGES:
      case eGL_MAX_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLint64(1024);
        return;
      case eGL_DEBUG_LOGGED_MESSAGES:
      case eGL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
        if(data)
          *data = GLint64(0);
        return;
      case eGL_DEBUG_GROUP_STACK_DEPTH:
        if(data)
          *data = GLint64(1);
        return;
      default: break;
    }
  }

  GL.glGetInteger64i_v(pname, index, data);
}

void WrappedOpenGL::glGetUnsignedBytevEXT(GLenum pname, GLubyte *data)
{
  GL.glGetUnsignedBytevEXT(pname, data);
}

void WrappedOpenGL::glGetUnsignedBytei_vEXT(GLenum target, GLuint index, GLubyte *data)
{
  GL.glGetUnsignedBytei_vEXT(target, index, data);
}

void WrappedOpenGL::glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
  GL.glGetTexLevelParameteriv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
  GL.glGetTexLevelParameterfv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
  GL.glGetTexParameterfv(target, pname, params);
}

void WrappedOpenGL::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
  GL.glGetTexParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname,
                                                 GLfloat *params)
{
  GL.glGetTextureLevelParameterfv(texture, level, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname,
                                                 GLint *params)
{
  GL.glGetTextureLevelParameteriv(texture, level, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params)
{
  GL.glGetTextureParameterIiv(texture, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params)
{
  GL.glGetTextureParameterIuiv(texture, pname, params);
}

void WrappedOpenGL::glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params)
{
  GL.glGetTextureParameterfv(texture, pname, params);
}

void WrappedOpenGL::glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params)
{
  GL.glGetTextureParameteriv(texture, pname, params);
}

void WrappedOpenGL::glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params)
{
  GL.glGetTexParameterIiv(target, pname, params);
}

void WrappedOpenGL::glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params)
{
  GL.glGetTexParameterIuiv(target, pname, params);
}

void WrappedOpenGL::glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetTexImage(target, level, format, type, pixels);
}

void WrappedOpenGL::glGetCompressedTexImage(GLenum target, GLint level, void *img)
{
  CoherentMapImplicitBarrier();

  GL.glGetCompressedTexImage(target, level, img);
}

void WrappedOpenGL::glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetnCompressedTexImage(target, lod, bufSize, pixels);
}

void WrappedOpenGL::glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize,
                                                void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetCompressedTextureImage(texture, level, bufSize, pixels);
}

void WrappedOpenGL::glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset,
                                                   GLint yoffset, GLint zoffset, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLsizei bufSize,
                                                   void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetCompressedTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth,
                                    bufSize, pixels);
}

void WrappedOpenGL::glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type,
                                   GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetnTexImage(target, level, format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type,
                                      GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetTextureImage(texture, level, format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                         GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                         GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetTextureSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth, format,
                          type, bufSize, pixels);
}

void WrappedOpenGL::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                          GLsizei bufSize, GLint *params)
{
  GL.glGetInternalformativ(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname,
                                            GLsizei bufSize, GLint64 *params)
{
  GL.glGetInternalformati64v(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params)
{
  GL.glGetSamplerParameterIiv(sampler, pname, params);
}

void WrappedOpenGL::glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params)
{
  GL.glGetSamplerParameterIuiv(sampler, pname, params);
}

void WrappedOpenGL::glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
  GL.glGetSamplerParameterfv(sampler, pname, params);
}

void WrappedOpenGL::glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
  GL.glGetSamplerParameteriv(sampler, pname, params);
}

void WrappedOpenGL::glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params)
{
  GL.glGetBufferParameteri64v(target, pname, params);
}

void WrappedOpenGL::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
  GL.glGetBufferParameteriv(target, pname, params);
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
    GL.glGetBufferPointerv(target, pname, params);
  }
}

void WrappedOpenGL::glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  CoherentMapImplicitBarrier();

  GL.glGetBufferSubData(target, offset, size, data);
}

void WrappedOpenGL::glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
  GL.glGetQueryObjectuiv(id, pname, params);
}

void WrappedOpenGL::glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
  GL.glGetQueryObjectui64v(id, pname, params);
}

void WrappedOpenGL::glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params)
{
  GL.glGetQueryIndexediv(target, index, pname, params);
}

void WrappedOpenGL::glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
  GL.glGetQueryObjecti64v(id, pname, params);
}

void WrappedOpenGL::glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
  GL.glGetQueryObjectiv(id, pname, params);
}

void WrappedOpenGL::glGetQueryiv(GLenum target, GLenum pname, GLint *params)
{
  GL.glGetQueryiv(target, pname, params);
}

void WrappedOpenGL::glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname,
                                                GLintptr offset)
{
  GL.glGetQueryBufferObjectui64v(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  GL.glGetQueryBufferObjectuiv(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  GL.glGetQueryBufferObjecti64v(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
  GL.glGetQueryBufferObjectiv(id, buffer, pname, offset);
}

void WrappedOpenGL::glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length,
                                GLint *values)
{
  CoherentMapImplicitBarrier();

  GL.glGetSynciv(sync, pname, bufSize, length, values);
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
  return GL.glGetString(name);
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
  return GL.glGetStringi(name, i);
}

void WrappedOpenGL::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
                                                          GLenum pname, GLint *params)
{
  GL.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

GLenum WrappedOpenGL::glCheckFramebufferStatus(GLenum target)
{
  return GL.glCheckFramebufferStatus(target);
}

void WrappedOpenGL::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
  GL.glGetVertexAttribiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
  GL.glGetVertexAttribPointerv(index, pname, pointer);
}

GLint WrappedOpenGL::glGetFragDataIndex(GLuint program, const GLchar *name)
{
  return GL.glGetFragDataIndex(program, name);
}

GLint WrappedOpenGL::glGetFragDataLocation(GLuint program, const GLchar *name)
{
  return GL.glGetFragDataLocation(program, name);
}

void WrappedOpenGL::glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
  GL.glGetMultisamplefv(pname, index, val);
}

void WrappedOpenGL::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
  GL.glGetShaderiv(shader, pname, params);
}

void WrappedOpenGL::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length,
                                       GLchar *infoLog)
{
  GL.glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype,
                                               GLint *range, GLint *precision)
{
  GL.glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void WrappedOpenGL::glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
  GL.glGetShaderSource(shader, bufSize, length, source);
}

void WrappedOpenGL::glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count,
                                         GLuint *shaders)
{
  GL.glGetAttachedShaders(program, maxCount, count, shaders);
}

void WrappedOpenGL::glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
  GL.glGetProgramiv(program, pname, params);
}

void WrappedOpenGL::glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint *values)
{
  GL.glGetProgramStageiv(program, shadertype, pname, values);
}

void WrappedOpenGL::glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                                       GLenum *binaryFormat, void *binary)
{
  GL.glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void WrappedOpenGL::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length,
                                        GLchar *infoLog)
{
  GL.glGetProgramInfoLog(program, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
  GL.glGetProgramPipelineiv(pipeline, pname, params);
}

void WrappedOpenGL::glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length,
                                                GLchar *infoLog)
{
  GL.glGetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname,
                                            GLint *params)
{
  GL.glGetProgramInterfaceiv(program, programInterface, pname, params);
}

GLuint WrappedOpenGL::glGetProgramResourceIndex(GLuint program, GLenum programInterface,
                                                const GLchar *name)
{
  return GL.glGetProgramResourceIndex(program, programInterface, name);
}

void WrappedOpenGL::glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index,
                                           GLsizei propCount, const GLenum *props, GLsizei bufSize,
                                           GLsizei *length, GLint *params)
{
  GL.glGetProgramResourceiv(program, programInterface, index, propCount, props, bufSize, length,
                            params);
}

void WrappedOpenGL::glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index,
                                             GLsizei bufSize, GLsizei *length, GLchar *name)
{
  GL.glGetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

GLint WrappedOpenGL::glGetProgramResourceLocation(GLuint program, GLenum programInterface,
                                                  const GLchar *name)
{
  return GL.glGetProgramResourceLocation(program, programInterface, name);
}

GLint WrappedOpenGL::glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface,
                                                       const GLchar *name)
{
  return GL.glGetProgramResourceLocationIndex(program, programInterface, name);
}

void WrappedOpenGL::glGetNamedStringARB(GLint namelen, const GLchar *name, GLsizei bufSize,
                                        GLint *stringlen, GLchar *string)
{
  return GL.glGetNamedStringARB(namelen, name, bufSize, stringlen, string);
}

void WrappedOpenGL::glGetNamedStringivARB(GLint namelen, const GLchar *name, GLenum pname,
                                          GLint *params)
{
  return GL.glGetNamedStringivARB(namelen, name, pname, params);
}

GLint WrappedOpenGL::glGetUniformLocation(GLuint program, const GLchar *name)
{
  return GL.glGetUniformLocation(program, name);
}

void WrappedOpenGL::glGetUniformIndices(GLuint program, GLsizei uniformCount,
                                        const GLchar *const *uniformNames, GLuint *uniformIndices)
{
  GL.glGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

GLuint WrappedOpenGL::glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
  return GL.glGetUniformBlockIndex(program, uniformBlockName);
}

GLint WrappedOpenGL::glGetAttribLocation(GLuint program, const GLchar *name)
{
  return GL.glGetAttribLocation(program, name);
}

GLuint WrappedOpenGL::glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name)
{
  return GL.glGetSubroutineIndex(program, shadertype, name);
}

GLint WrappedOpenGL::glGetSubroutineUniformLocation(GLuint program, GLenum shadertype,
                                                    const GLchar *name)
{
  return GL.glGetSubroutineUniformLocation(program, shadertype, name);
}

void WrappedOpenGL::glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params)
{
  GL.glGetUniformSubroutineuiv(shadertype, location, params);
}

void WrappedOpenGL::glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index,
                                              GLsizei bufsize, GLsizei *length, GLchar *name)
{
  GL.glGetActiveSubroutineName(program, shadertype, index, bufsize, length, name);
}

void WrappedOpenGL::glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index,
                                                     GLsizei bufsize, GLsizei *length, GLchar *name)
{
  GL.glGetActiveSubroutineUniformName(program, shadertype, index, bufsize, length, name);
}

void WrappedOpenGL::glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index,
                                                   GLenum pname, GLint *values)
{
  GL.glGetActiveSubroutineUniformiv(program, shadertype, index, pname, values);
}

void WrappedOpenGL::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                                       GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  GL.glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                                          const GLuint *uniformIndices, GLenum pname, GLint *params)
{
  GL.glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

void WrappedOpenGL::glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize,
                                           GLsizei *length, GLchar *uniformName)
{
  GL.glGetActiveUniformName(program, uniformIndex, bufSize, length, uniformName);
}

void WrappedOpenGL::glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex,
                                              GLenum pname, GLint *params)
{
  GL.glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void WrappedOpenGL::glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex,
                                                GLsizei bufSize, GLsizei *length,
                                                GLchar *uniformBlockName)
{
  GL.glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void WrappedOpenGL::glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize,
                                      GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  GL.glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex,
                                                     GLenum pname, GLint *params)
{
  GL.glGetActiveAtomicCounterBufferiv(program, bufferIndex, pname, params);
}

void WrappedOpenGL::glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
  GL.glGetUniformfv(program, location, params);
}

void WrappedOpenGL::glGetUniformiv(GLuint program, GLint location, GLint *params)
{
  GL.glGetUniformiv(program, location, params);
}

void WrappedOpenGL::glGetUniformuiv(GLuint program, GLint location, GLuint *params)
{
  GL.glGetUniformuiv(program, location, params);
}

void WrappedOpenGL::glGetUniformdv(GLuint program, GLint location, GLdouble *params)
{
  GL.glGetUniformdv(program, location, params);
}

void WrappedOpenGL::glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params)
{
  GL.glGetnUniformdv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
  GL.glGetnUniformfv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
  GL.glGetnUniformiv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params)
{
  GL.glGetnUniformuiv(program, location, bufSize, params);
}

void WrappedOpenGL::glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param)
{
  GL.glGetVertexArrayiv(vaobj, pname, param);
}

void WrappedOpenGL::glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname,
                                                GLint64 *param)
{
  GL.glGetVertexArrayIndexed64iv(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
  GL.glGetVertexArrayIndexediv(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
  GL.glGetVertexAttribIiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
  GL.glGetVertexAttribIuiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params)
{
  GL.glGetVertexAttribLdv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params)
{
  GL.glGetVertexAttribdv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
  GL.glGetVertexAttribfv(index, pname, params);
}

void WrappedOpenGL::glClampColor(GLenum target, GLenum clamp)
{
  GL.glClampColor(target, clamp);
}

void WrappedOpenGL::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                 GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glReadPixels(x, y, width, height, format, type, pixels);
}

void WrappedOpenGL::glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                                  GLenum type, GLsizei bufSize, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glReadnPixels(x, y, width, height, format, type, bufSize, pixels);
}

void WrappedOpenGL::glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                                  GLsizei *length, GLsizei *size, GLenum *type,
                                                  GLchar *name)
{
  GL.glGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param)
{
  GL.glGetTransformFeedbacki64_v(xfb, pname, index, param);
}

void WrappedOpenGL::glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param)
{
  GL.glGetTransformFeedbacki_v(xfb, pname, index, param);
}

void WrappedOpenGL::glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param)
{
  GL.glGetTransformFeedbackiv(xfb, pname, param);
}

void WrappedOpenGL::glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  GL.glGetFramebufferParameteriv(target, pname, param);
}

void WrappedOpenGL::glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *param)
{
  GL.glGetRenderbufferParameteriv(target, pname, param);
}

void WrappedOpenGL::glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params)
{
  GL.glGetNamedBufferParameteri64v(buffer, pname, params);
}

void WrappedOpenGL::glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint *param)
{
  GL.glGetNamedFramebufferParameterivEXT(framebuffer, pname, param);
}

void WrappedOpenGL::glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer,
                                                                  GLenum attachment, GLenum pname,
                                                                  GLint *params)
{
  GL.glGetNamedFramebufferAttachmentParameterivEXT(framebuffer, attachment, pname, params);
}

void WrappedOpenGL::glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname,
                                                         GLint *params)
{
  GL.glGetNamedRenderbufferParameterivEXT(renderbuffer, pname, params);
}

void WrappedOpenGL::glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format,
                                         GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetTextureImageEXT(texture, target, level, format, type, pixels);
}

void WrappedOpenGL::glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint level,
                                                   void *img)
{
  CoherentMapImplicitBarrier();

  GL.glGetCompressedTextureImageEXT(texture, target, level, img);
}

GLenum WrappedOpenGL::glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target)
{
  return GL.glCheckNamedFramebufferStatusEXT(framebuffer, target);
}

void WrappedOpenGL::glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params)
{
  GL.glGetNamedBufferParameterivEXT(buffer, pname, params);
}

void WrappedOpenGL::glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                               void *data)
{
  CoherentMapImplicitBarrier();

  GL.glGetNamedBufferSubDataEXT(buffer, offset, size, data);
}

void WrappedOpenGL::glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                            void *data)
{
  CoherentMapImplicitBarrier();

  GL.glGetNamedBufferSubData(buffer, offset, size, data);
}

void WrappedOpenGL::glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                               GLint *params)
{
  GL.glGetTextureParameterivEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                               GLfloat *params)
{
  GL.glGetTextureParameterfvEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                                GLint *params)
{
  GL.glGetTextureParameterIivEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                                 GLuint *params)
{
  GL.glGetTextureParameterIuivEXT(texture, target, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level,
                                                    GLenum pname, GLint *params)
{
  GL.glGetTextureLevelParameterivEXT(texture, target, level, pname, params);
}

void WrappedOpenGL::glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level,
                                                    GLenum pname, GLfloat *params)
{
  GL.glGetTextureLevelParameterfvEXT(texture, target, level, pname, params);
}

void WrappedOpenGL::glGetPointeri_vEXT(GLenum pname, GLuint index, void **params)
{
  if(pname == eGL_DEBUG_CALLBACK_FUNCTION)
    *params = (void *)GetCtxData().m_RealDebugFunc;
  else if(pname == eGL_DEBUG_CALLBACK_USER_PARAM)
    *params = (void *)GetCtxData().m_RealDebugFuncParam;
  else
    GL.glGetPointeri_vEXT(pname, index, params);
}

void WrappedOpenGL::glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble *data)
{
  GL.glGetDoubleIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetPointerIndexedvEXT(GLenum target, GLuint index, void **data)
{
  GL.glGetPointerIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data)
{
  GL.glGetIntegerIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data)
{
  GL.glGetBooleanIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat *data)
{
  GL.glGetFloatIndexedvEXT(target, index, data);
}

void WrappedOpenGL::glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format,
                                          GLenum type, void *pixels)
{
  CoherentMapImplicitBarrier();

  GL.glGetMultiTexImageEXT(texunit, target, level, format, type, pixels);
}

void WrappedOpenGL::glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                                GLfloat *params)
{
  GL.glGetMultiTexParameterfvEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                GLint *params)
{
  GL.glGetMultiTexParameterivEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                 GLint *params)
{
  GL.glGetMultiTexParameterIivEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                                  GLuint *params)
{
  GL.glGetMultiTexParameterIuivEXT(texunit, target, pname, params);
}

void WrappedOpenGL::glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level,
                                                     GLenum pname, GLfloat *params)
{
  GL.glGetMultiTexLevelParameterfvEXT(texunit, target, level, pname, params);
}

void WrappedOpenGL::glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level,
                                                     GLenum pname, GLint *params)
{
  GL.glGetMultiTexLevelParameterivEXT(texunit, target, level, pname, params);
}

void WrappedOpenGL::glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod,
                                                    void *img)
{
  CoherentMapImplicitBarrier();

  GL.glGetCompressedMultiTexImageEXT(texunit, target, lod, img);
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
    GL.glGetNamedBufferPointervEXT(buffer, pname, params);
  }
}

void WrappedOpenGL::glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint *params)
{
  GL.glGetNamedProgramivEXT(program, target, pname, params);
}

void WrappedOpenGL::glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint *param)
{
  GL.glGetVertexArrayIntegervEXT(vaobj, pname, param);
}

void WrappedOpenGL::glGetVertexArrayPointervEXT(GLuint vaobj, GLenum pname, void **param)
{
  GL.glGetVertexArrayPointervEXT(vaobj, pname, param);
}

void WrappedOpenGL::glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname,
                                                  GLint *param)
{
  GL.glGetVertexArrayIntegeri_vEXT(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetVertexArrayPointeri_vEXT(GLuint vaobj, GLuint index, GLenum pname,
                                                  void **param)
{
  GL.glGetVertexArrayPointeri_vEXT(vaobj, index, pname, param);
}

void WrappedOpenGL::glGetMemoryObjectParameterivEXT(GLuint memoryObject, GLenum pname, GLint *params)
{
  GL.glGetMemoryObjectParameterivEXT(memoryObject, pname, params);
}

void WrappedOpenGL::glGetSemaphoreParameterui64vEXT(GLuint semaphore, GLenum pname, GLuint64 *params)
{
  GL.glGetSemaphoreParameterui64vEXT(semaphore, pname, params);
}