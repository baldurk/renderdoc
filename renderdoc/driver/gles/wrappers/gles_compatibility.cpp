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

void WrappedGLES::Compat_glGetTexImage(GLenum target, GLenum texType, GLuint texname, GLint mip, GLenum fmt,
                                       GLenum type,GLint width, GLint height, GLint depth, void *ret)
{
  GLenum attachmentTarget = eGL_COLOR_ATTACHMENT0;
  if(fmt == eGL_DEPTH_COMPONENT)
    attachmentTarget = eGL_DEPTH_ATTACHMENT;
  else if(fmt == eGL_STENCIL)
    attachmentTarget = eGL_STENCIL_ATTACHMENT;
  else if(fmt == eGL_DEPTH_STENCIL)
    attachmentTarget = eGL_DEPTH_STENCIL_ATTACHMENT;

  if((fmt == eGL_DEPTH_COMPONENT && !ExtensionSupported[ExtensionSupported_NV_read_depth]) ||
     (fmt == eGL_STENCIL && !ExtensionSupported[ExtensionSupported_NV_read_stencil]) ||
     (fmt == eGL_DEPTH_STENCIL && !ExtensionSupported[ExtensionSupported_NV_read_depth_stencil]))
  {
    // return silently, check was made during startup
    return;
  }

  GLuint fbo = 0;
  m_Real.glGenFramebuffers(1, &fbo);

  SafeFramebufferBinder safeFramebufferBinder(m_Real, eGL_FRAMEBUFFER, fbo);

  size_t sliceSize = GetByteSize(width, height, 1, fmt, type);

  for(GLint d = 0; d < depth; ++d)
  {
    switch(texType)
    {
      case eGL_TEXTURE_3D:
      case eGL_TEXTURE_2D_ARRAY:
      case eGL_TEXTURE_CUBE_MAP_ARRAY:
      case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        m_Real.glFramebufferTextureLayer(eGL_FRAMEBUFFER, attachmentTarget, texname, mip, d);
        break;

      case eGL_TEXTURE_CUBE_MAP:
      case eGL_TEXTURE_2D:
      case eGL_TEXTURE_2D_MULTISAMPLE:
      default:
        m_Real.glFramebufferTexture2D(eGL_FRAMEBUFFER, attachmentTarget, target, texname, mip);
        break;
    }

    dumpFBOState(m_Real);

    byte *dst = (byte *)ret + d * sliceSize;
    m_Real.glReadPixels(0, 0, width, height, fmt, type, (void *)dst);
  }

  m_Real.glDeleteFramebuffers(1, &fbo);
}

void WrappedGLES::Compat_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  void* mappedData = m_Real.glMapBufferRange(target, offset, size, eGL_MAP_READ_BIT);
  if (mappedData != NULL)
  {
    memcpy(data, mappedData, size);
    m_Real.glUnmapBuffer(target);
  }
}

void WrappedGLES::Compat_glGetNamedBufferSubDataEXT(GLuint buffer, GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  SafeBufferBinder safeBufferBinder(m_Real, target, buffer);
  Compat_glGetBufferSubData(target, offset, size, data);
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

void* WrappedGLES::Compat_glMapNamedBufferRangeEXT(GLuint buffer, GLenum target, GLintptr offset, GLsizeiptr length, GLenum access)
{
  SafeBufferBinder safeBufferBinder(m_Real, target, buffer);
  return m_Real.glMapBufferRange(target, offset, length, access);
}

void WrappedGLES::Compat_glUnmapNamedBufferEXT(GLuint buffer, GLenum target)
{
  SafeBufferBinder safeBufferBinder(m_Real, target, buffer);
  m_Real.glUnmapBuffer(target);
}

void WrappedGLES::Compat_glTextureStorage2DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
  if(ExtensionSupported[ExtensionSupported_EXT_texture_storage] && false) // TODO(elecro): on android it is force disabled for now
  {
    m_Real.glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
  }
  else
  {
    SafeTextureBinder safeTextureBinder(m_Real, texture, target);
    m_Real.glTexStorage2D(target, levels, internalformat, width, height);
  }
}

void WrappedGLES::Compat_glTextureStorage3DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
  if(ExtensionSupported[ExtensionSupported_EXT_texture_storage] && false) // TODO(elecro): on android it is force disabled for now
    m_Real.glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
  else
  {
    SafeTextureBinder safeTextureBinder(m_Real, texture, target);
    m_Real.glTexStorage3D(target, levels, internalformat, width, height, depth);
  }
}

void WrappedGLES::Compat_glDrawArraysInstancedBaseInstanceEXT(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
  if(ExtensionSupported[ExtensionSupported_EXT_base_instance])
    m_Real.glDrawArraysInstancedBaseInstanceEXT(mode, first, count, instancecount, baseinstance);
  else
  {
    RDCASSERT(baseinstance == 0);
    m_Real.glDrawArraysInstanced(mode, first, count, instancecount);
  }
}

void WrappedGLES::Compat_glDrawElementsInstancedBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
  if(ExtensionSupported[ExtensionSupported_EXT_base_instance])
    m_Real.glDrawElementsInstancedBaseInstanceEXT(mode, count, type, indices, instancecount, baseinstance);
  else
  {
    RDCASSERT(baseinstance == 0);
    m_Real.glDrawElementsInstanced(mode, count, type, indices, instancecount);
  }
}

void WrappedGLES::Compat_glDrawElementsInstancedBaseVertexBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
  if(ExtensionSupported[ExtensionSupported_EXT_base_instance])
    m_Real.glDrawElementsInstancedBaseVertexBaseInstanceEXT(mode, count, type, indices, instancecount, basevertex, baseinstance);
  else
  {
    RDCASSERT(baseinstance == 0);
    m_Real.glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
  }
}

void WrappedGLES::Compat_glDepthRangeArrayfv(VendorType vendor, GLuint first, GLsizei count, const GLfloat *v)
{
  if(vendor == Vendor_OES && ExtensionSupported[ExtensionSupported_OES_viewport_array])
    m_Real.glDepthRangeArrayfvOES(first, count, v);
  else if(vendor == Vendor_NV && ExtensionSupported[ExtensionSupported_NV_viewport_array])
    m_Real.glDepthRangeArrayfvNV(first, count, v);
  else
    RDCERR("Unsupported function: glDepthRangeArrayfv (%s)", ToStr::Get(vendor).c_str());
}

void WrappedGLES::Compat_glDepthRangeIndexedf(VendorType vendor, GLuint index, GLfloat nearVal, GLfloat farVal)
{
  if(vendor == Vendor_OES && ExtensionSupported[ExtensionSupported_OES_viewport_array])
    m_Real.glDepthRangeIndexedfOES(index, nearVal, farVal);
  else if(vendor == Vendor_NV && ExtensionSupported[ExtensionSupported_NV_viewport_array])
    m_Real.glDepthRangeIndexedfNV(index, nearVal, farVal);
  else
    RDCERR("Unsupported function: glDepthRangeIndexedf (%s)", ToStr::Get(vendor).c_str());
}

void WrappedGLES::Compat_glScissorArrayv(VendorType vendor, GLuint first, GLsizei count, const GLint *v)
{
  if(vendor == Vendor_OES && ExtensionSupported[ExtensionSupported_OES_viewport_array])
    m_Real.glScissorArrayvOES(first, count, v);
  else if(vendor == Vendor_NV && ExtensionSupported[ExtensionSupported_NV_viewport_array])
    m_Real.glScissorArrayvNV(first, count, v);
  else
    RDCERR("Unsupported function: glScissorArrayv (%s)", ToStr::Get(vendor).c_str());
}

void WrappedGLES::Compat_glViewportArrayv(VendorType vendor, GLuint first, GLsizei count, const GLfloat *v)
{
  if(vendor == Vendor_OES && ExtensionSupported[ExtensionSupported_OES_viewport_array])
    m_Real.glViewportArrayvOES(first, count, v);
  else if(vendor == Vendor_NV && ExtensionSupported[ExtensionSupported_NV_viewport_array])
    m_Real.glViewportArrayvNV(first, count, v);
  else
    RDCERR("Unsupported function: glViewportArrayv (%s)", ToStr::Get(vendor).c_str());
}

void WrappedGLES::Compat_glFramebufferTexture2DMultisample(VendorType vendor, GLenum target, GLenum attachment,
                                                           GLenum textarget, GLuint texture, GLint level, GLsizei samples)
{
  if(vendor == Vendor_EXT && ExtensionSupported[ExtensionSupported_EXT_multisampled_render_to_texture])
    m_Real.glFramebufferTexture2DMultisampleEXT(target, attachment, textarget, texture, level, samples);
  else if(vendor == Vendor_IMG && ExtensionSupported[ExtensionSupported_IMG_multisampled_render_to_texture])
    m_Real.glFramebufferTexture2DMultisampleIMG(target, attachment, textarget, texture, level, samples);
  else
    RDCERR("Unsupported function: glFramebufferTexture2DMultisample (%s)", ToStr::Get(vendor).c_str());
}

void WrappedGLES::Compat_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                  const void *indices, GLint basevertex)
{
  if (m_Real.glDrawElementsBaseVertex != NULL)
    m_Real.glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
  else if (m_Real.glDrawElementsBaseVertex == NULL && basevertex == 0 && m_Real.glDrawElements != NULL)
    m_Real.glDrawElements(mode, count, type, indices);
  else
    RDCERR("glDrawElementsBaseVertex is not supported! No draw will be called!");
}

