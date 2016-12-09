/******************************************************************************
 * The MIT License (MIT)
 *
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

void WrappedGLES::glGetTexImage(GLenum target, GLenum texType, GLuint texname, GLint mip, GLenum fmt, GLenum type, GLint width, GLint height, void *ret)
{
  GLuint prevfbo = 0;
  GLuint fbo = 0;

  m_Real.glGetIntegerv(eGL_FRAMEBUFFER_BINDING, (GLint *)&prevfbo);
  m_Real.glGenFramebuffers(1, &fbo);
  m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);

  GLenum attachmentTarget = (fmt == eGL_DEPTH_COMPONENT) ? eGL_DEPTH_ATTACHMENT : eGL_COLOR_ATTACHMENT0;

  // TODO pantos 3d, arrays?
  if (texType == eGL_TEXTURE_CUBE_MAP) {
    m_Real.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, target, texname, mip);
  } else {
    m_Real.glFramebufferTexture(eGL_FRAMEBUFFER, attachmentTarget, texname, mip);
  }

  // TODO(elecro): remove this debug code
  GLenum status = m_Real.glCheckFramebufferStatus(eGL_FRAMEBUFFER);
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE: break;
#define DUMP(STATUS) case STATUS: printf(#STATUS "\n"); break

    DUMP(eGL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
    DUMP(eGL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS);
    DUMP(eGL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
    DUMP(eGL_FRAMEBUFFER_UNSUPPORTED);
#undef DUMP
    default: printf("Unkown status: %d\n", status);
  }

  m_Real.glReadPixels(0, 0, width, height, fmt, type, (void *)ret);

  m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, prevfbo);
  m_Real.glDeleteFramebuffers(1, &fbo);
}

void WrappedGLES::glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  void* mappedData = m_Real.glMapBufferRange(target, offset, size, eGL_MAP_READ_BIT);
  if (mappedData != NULL)
    memcpy(data, mappedData, size);

  m_Real.glUnmapBuffer(target);
}

void WrappedGLES::glGetNamedBufferSubDataEXT(GLuint buffer, GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  SafeBufferBinder safeBufferBinder(m_Real, target, buffer);
  glGetBufferSubData(target, offset, size, data);
}

void WrappedGLES::Compat_glBufferStorageEXT (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
  if(ExtensionSupported[ExtensionSupported_EXT_buffer_storage])
    m_Real.glBufferStorageEXT(target, size, data, flags);
  else
  {
    if ((flags & eGL_DYNAMIC_STORAGE_BIT_EXT) == flags)
      m_Real.glBufferData(target, size, data, eGL_DYNAMIC_DRAW);
    else if ((flags & (eGL_DYNAMIC_STORAGE_BIT_EXT | eGL_MAP_READ_BIT)) == flags)
      m_Real.glBufferData(target, size, data, eGL_DYNAMIC_READ);
    else
    {
      RDCWARN("Unhandled glBufferStorageEXT() flags! Default usage (GL_DYNAMIC_DRAW) is used.");
      m_Real.glBufferData(target, size, data, eGL_DYNAMIC_DRAW);
    }
  }
}

void WrappedGLES::Compat_glTextureStorage2DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
  if(ExtensionSupported[ExtensionSupported_EXT_texture_storage])
    m_Real.glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
  else
  {
    SafeTextureBinder safeTextureBinder(m_Real, texture, target);
    m_Real.glTexStorage2D(target, levels, internalformat, width, height);
  }
}

void WrappedGLES::Compat_glTextureStorage3DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
  if(ExtensionSupported[ExtensionSupported_EXT_texture_storage])
    m_Real.glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
  else
  {
    SafeTextureBinder safeTextureBinder(m_Real, texture, target);
    m_Real.glTexStorage3D(target, levels, internalformat, width, height, depth);
  }
}

void * WrappedGLES::Compat_glMapBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
  if (ExtensionSupported[ExtensionSupported_EXT_map_buffer_range])
    return m_Real.glMapBufferRangeEXT(target, offset, length, access);
  else
    return m_Real.glMapBufferRange(target, offset, length, access);
}

void WrappedGLES::Compat_glFlushMappedBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length)
{
  if (ExtensionSupported[ExtensionSupported_EXT_map_buffer_range])
    m_Real.glFlushMappedBufferRangeEXT(target, offset, length);
  else
    m_Real.glFlushMappedBufferRange(target, offset, length);
}
