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

void WrappedGLES::Compat_glGetTexImage(GLenum target, GLenum texType, GLuint texname, GLint mip, GLenum fmt, GLenum type, GLint width, GLint height, void *ret)
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

  dumpFBOState(m_Real);

  m_Real.glReadPixels(0, 0, width, height, fmt, type, (void *)ret);

  m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, prevfbo);
  m_Real.glDeleteFramebuffers(1, &fbo);
}

void WrappedGLES::Compat_glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  void* mappedData = m_Real.glMapBufferRange(target, offset, size, eGL_MAP_READ_BIT);
  if (mappedData != NULL)
    memcpy(data, mappedData, size);

  m_Real.glUnmapBuffer(target);
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
  if(ExtensionSupported[ExtensionSupported_EXT_texture_storage])
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

