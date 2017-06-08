/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

// in some cases we might need some functions (notably ARB_direct_state_access)
// emulated where possible, so we can simplify most codepaths by just assuming they're
// present elsewhere and using them unconditionally.

#include "driver/gl/gl_common.h"
#include "driver/gl/gl_hookset.h"
#include "driver/gl/gl_resources.h"

namespace glEmulate
{
const GLHookSet *hookset = NULL;
PFNGLGETINTERNALFORMATIVPROC glGetInternalformativ_real = NULL;

typedef GLenum (*BindingLookupFunc)(GLenum target);

struct PushPop
{
  // we can use PFNGLBINDTEXTUREPROC since most bind functions are identical - taking GLenum and
  // GLuint.
  PushPop(GLenum target, PFNGLBINDTEXTUREPROC bindFunc, BindingLookupFunc bindingLookup)
  {
    other = bindFunc;
    vao = NULL;
    t = target;
    hookset->glGetIntegerv(bindingLookup(target), (GLint *)&o);
  }

  PushPop(GLenum target, PFNGLBINDTEXTUREPROC bindFunc, GLenum binding)
  {
    other = bindFunc;
    vao = NULL;
    t = target;
    hookset->glGetIntegerv(binding, (GLint *)&o);
  }

  PushPop(PFNGLBINDVERTEXARRAYPROC bindFunc)
  {
    vao = bindFunc;
    other = NULL;
    hookset->glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&o);
  }

  ~PushPop()
  {
    if(vao)
      vao(o);
    else
      other(t, o);
  }

  PFNGLBINDVERTEXARRAYPROC vao;
  PFNGLBINDTEXTUREPROC other;

  GLenum t;
  GLuint o;
};

// if specifying the image or etc for a cubemap face, we must bind the cubemap itself.
GLenum TexBindTarget(GLenum target)
{
  switch(target)
  {
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return eGL_TEXTURE_CUBE_MAP;
    default: break;
  }

  return target;
}

#define PushPopTexture(target, obj)                                                    \
  GLenum bindtarget = TexBindTarget(target);                                           \
  PushPop CONCAT(prev, __LINE__)(bindtarget, hookset->glBindTexture, &TextureBinding); \
  hookset->glBindTexture(bindtarget, obj);

#define PushPopBuffer(target, obj)                                               \
  PushPop CONCAT(prev, __LINE__)(target, hookset->glBindBuffer, &BufferBinding); \
  hookset->glBindBuffer(target, obj);

#define PushPopXFB(obj)                                                                    \
  PushPop CONCAT(prev, __LINE__)(eGL_TRANSFORM_FEEDBACK, hookset->glBindTransformFeedback, \
                                 eGL_TRANSFORM_FEEDBACK_BINDING);                          \
  hookset->glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, obj);

#define PushPopFramebuffer(target, obj)                                                    \
  PushPop CONCAT(prev, __LINE__)(target, hookset->glBindFramebuffer, &FramebufferBinding); \
  hookset->glBindFramebuffer(target, obj);

#define PushPopRenderbuffer(obj)                                                \
  PushPop CONCAT(prev, __LINE__)(eGL_RENDERBUFFER, hookset->glBindRenderbuffer, \
                                 eGL_RENDERBUFFER_BINDING);                     \
  hookset->glBindRenderbuffer(eGL_RENDERBUFFER, obj);

#define PushPopVertexArray(obj)                               \
  PushPop CONCAT(prev, __LINE__)(hookset->glBindVertexArray); \
  hookset->glBindVertexArray(obj);

void APIENTRY _glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
  PushPopXFB(xfb);
  hookset->glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
}

void APIENTRY _glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                              GLintptr offset, GLsizeiptr size)
{
  PushPopXFB(xfb);
  hookset->glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, index, buffer, offset, size);
}

void APIENTRY _glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                         const GLint *value)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferiv(buffer, drawbuffer, value);
}

void APIENTRY _glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          const GLuint *value)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferuiv(buffer, drawbuffer, value);
}

void APIENTRY _glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                         const GLfloat *value)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferfv(buffer, drawbuffer, value);
}

void APIENTRY _glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, const GLfloat depth,
                                         GLint stencil)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferfi(buffer, 0, depth, stencil);
}

void APIENTRY _glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0,
                                      GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                                      GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
  PushPopFramebuffer(eGL_READ_FRAMEBUFFER, readFramebuffer);
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFramebuffer);
  hookset->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY _glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
  PushPopVertexArray(vaobj);
  hookset->glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);
}

void APIENTRY _glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                                          const GLuint *buffers, const GLintptr *offsets,
                                          const GLsizei *strides)
{
  PushPopVertexArray(vaobj);
  hookset->glBindVertexBuffers(first, count, buffers, offsets, strides);
}

void APIENTRY _glClearDepthf(GLfloat d)
{
  hookset->glClearDepth(d);
}

void EmulateUnsupportedFunctions(GLHookSet *hooks)
{
  hookset = hooks;

#define EMULATE_UNSUPPORTED(func) \
  if(!hooks->func)                \
    hooks->func = &CONCAT(_, func);

  EMULATE_UNSUPPORTED(glTransformFeedbackBufferBase)
  EMULATE_UNSUPPORTED(glTransformFeedbackBufferRange)
  EMULATE_UNSUPPORTED(glClearNamedFramebufferiv)
  EMULATE_UNSUPPORTED(glClearNamedFramebufferuiv)
  EMULATE_UNSUPPORTED(glClearNamedFramebufferfv)
  EMULATE_UNSUPPORTED(glClearNamedFramebufferfi)
  EMULATE_UNSUPPORTED(glBlitNamedFramebuffer)
  EMULATE_UNSUPPORTED(glVertexArrayElementBuffer);
  EMULATE_UNSUPPORTED(glVertexArrayVertexBuffers)

  // internally glClearDepthf is used instead of glClearDepth (because OpenGL ES does support the
  // non-f version), however glClearDepthf is not available before OpenGL 4.1
  EMULATE_UNSUPPORTED(glClearDepthf)

  // workaround for nvidia bug, which complains that GL_DEPTH_STENCIL is an invalid draw buffer.
  // also some issues with 32-bit implementation of this entry point.
  //
  // NOTE: Vendor Checks aren't initialised by this point, so we have to do this unconditionally
  // We include it just for searching: VendorCheck[VendorCheck_NV_ClearNamedFramebufferfiBugs]
  hooks->glClearNamedFramebufferfi = &_glClearNamedFramebufferfi;

  // workaround for AMD bug or weird behaviour. glVertexArrayElementBuffer doesn't update the
  // GL_ELEMENT_ARRAY_BUFFER_BINDING global query, when binding the VAO subsequently *will*.
  // I'm not sure if that's correct (weird) behaviour or buggy, but we can work around it just
  // by avoiding use of the DSA function and always doing our emulated version.
  //
  // VendorCheck[VendorCheck_AMD_vertex_array_elem_buffer_query]
  hooks->glVertexArrayElementBuffer = &_glVertexArrayElementBuffer;
}

#pragma region EXT_direct_state_access

#pragma region Framebuffers

void APIENTRY _glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment,
                                                             GLenum pname, GLint *params)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachment, pname, params);
}

GLenum APIENTRY _glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target)
{
  PushPopFramebuffer(target, framebuffer);
  return hookset->glCheckFramebufferStatus(target);
}

void APIENTRY _glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint *params)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glGetFramebufferParameteriv(eGL_DRAW_FRAMEBUFFER, pname, params);
}

void APIENTRY _glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment,
                                              GLenum textarget, GLuint texture, GLint level)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferTexture1D(eGL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level);
}

void APIENTRY _glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment,
                                              GLenum textarget, GLuint texture, GLint level)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level);
}

void APIENTRY _glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget,
                                              GLuint texture, GLint level, GLint zoffset)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferTexture3D(eGL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level,
                                  zoffset);
}

void APIENTRY _glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture,
                                            GLint level)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, attachment, texture, level);
}

void APIENTRY _glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment,
                                                 GLuint texture, GLint level, GLint layer)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attachment, texture, level, layer);
}

void APIENTRY _glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferParameteri(eGL_DRAW_FRAMEBUFFER, pname, param);
}

void APIENTRY _glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum mode)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glDrawBuffer(mode);
}

void APIENTRY _glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glDrawBuffers(n, bufs);
}

void APIENTRY _glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode)
{
  PushPopFramebuffer(eGL_READ_FRAMEBUFFER, framebuffer);
  hookset->glReadBuffer(mode);
}

void APIENTRY _glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment,
                                                 GLenum renderbuffertarget, GLuint renderbuffer)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glFramebufferRenderbuffer(eGL_DRAW_FRAMEBUFFER, attachment, renderbuffertarget,
                                     renderbuffer);
}

#pragma endregion

#pragma region Renderbuffers

void APIENTRY _glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat,
                                             GLsizei width, GLsizei height)
{
  PushPopRenderbuffer(renderbuffer);
  hookset->glRenderbufferStorage(eGL_RENDERBUFFER, internalformat, width, height);
}

void APIENTRY _glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height)
{
  PushPopRenderbuffer(renderbuffer);
  hookset->glRenderbufferStorageMultisample(eGL_RENDERBUFFER, samples, internalformat, width, height);
}

void APIENTRY _glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname, GLint *params)
{
  PushPopRenderbuffer(renderbuffer);
  hookset->glGetRenderbufferParameteriv(eGL_RENDERBUFFER, pname, params);
}

#pragma endregion

#pragma region Buffers

void APIENTRY _glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glGetBufferParameteriv(eGL_COPY_READ_BUFFER, pname, params);
}

void APIENTRY _glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  void *bufData = hookset->glMapBufferRange(target, offset, size, eGL_MAP_READ_BIT);
  if(!bufData)
  {
    RDCERR("glMapBufferRange failed to map buffer.");
    return;
  }
  memcpy(data, bufData, (size_t)size);
  hookset->glUnmapBuffer(target);
}

void APIENTRY _glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  _glGetBufferSubData(eGL_COPY_READ_BUFFER, offset, size, data);
}

void *APIENTRY _glMapNamedBufferEXT(GLuint buffer, GLenum access)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GLint size;
  hookset->glGetBufferParameteriv(eGL_COPY_READ_BUFFER, eGL_BUFFER_SIZE, &size);
  return hookset->glMapBufferRange(eGL_COPY_READ_BUFFER, 0, size, eGL_MAP_READ_BIT);
}

void *APIENTRY _glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                         GLbitfield access)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  return hookset->glMapBufferRange(eGL_COPY_READ_BUFFER, offset, length, access);
}

void APIENTRY _glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glFlushMappedBufferRange(eGL_COPY_READ_BUFFER, offset, length);
}

GLboolean APIENTRY _glUnmapNamedBufferEXT(GLuint buffer)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  return hookset->glUnmapBuffer(eGL_COPY_READ_BUFFER);
}

void APIENTRY _glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format,
                                         GLenum type, const void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glClearBufferData(eGL_COPY_READ_BUFFER, internalformat, format, type, data);
}

void APIENTRY _glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLsizeiptr offset,
                                            GLsizeiptr size, GLenum format, GLenum type,
                                            const void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glClearBufferSubData(eGL_COPY_READ_BUFFER, internalformat, offset, size, format, type,
                                data);
}

void APIENTRY _glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glBufferData(eGL_COPY_READ_BUFFER, size, data, usage);
}

void APIENTRY _glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data,
                                       GLbitfield flags)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glBufferStorage(eGL_COPY_READ_BUFFER, size, data, flags);
}

void APIENTRY _glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                       const void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  hookset->glBufferSubData(eGL_COPY_READ_BUFFER, offset, size, data);
}

void APIENTRY _glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer,
                                           GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, readBuffer);
  PushPopBuffer(eGL_COPY_WRITE_BUFFER, writeBuffer);
  hookset->glCopyBufferSubData(eGL_COPY_READ_BUFFER, eGL_COPY_WRITE_BUFFER, readOffset, writeOffset,
                               size);
}

#pragma endregion

#pragma region Textures

void APIENTRY _glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                             GLenum internalformat, GLsizei width, GLint border,
                                             GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  hookset->glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, bits);
}

void APIENTRY _glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                             GLenum internalformat, GLsizei width, GLsizei height,
                                             GLint border, GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  hookset->glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize,
                                  bits);
}

void APIENTRY _glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                             GLenum internalformat, GLsizei width, GLsizei height,
                                             GLsizei depth, GLint border, GLsizei imageSize,
                                             const void *bits)
{
  PushPopTexture(target, texture);
  hookset->glCompressedTexImage3D(target, level, internalformat, width, height, depth, border,
                                  imageSize, bits);
}

void APIENTRY _glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level,
                                                GLint xoffset, GLsizei width, GLenum format,
                                                GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  hookset->glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, bits);
}

void APIENTRY _glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLsizei width,
                                                GLsizei height, GLenum format, GLsizei imageSize,
                                                const void *bits)
{
  PushPopTexture(target, texture);
  hookset->glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                     imageSize, bits);
}

void APIENTRY _glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint zoffset,
                                                GLsizei width, GLsizei height, GLsizei depth,
                                                GLenum format, GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  hookset->glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                                     format, imageSize, bits);
}

void APIENTRY _glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void *img)
{
  PushPopTexture(target, texture);
  hookset->glGetCompressedTexImage(target, lod, img);
}

void APIENTRY _glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format,
                                    GLenum type, void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glGetTexImage(target, level, format, type, pixels);
}

void APIENTRY _glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat *params)
{
  PushPopTexture(target, texture);
  hookset->glGetTexParameterfv(target, pname, params);
}

void APIENTRY _glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params)
{
  PushPopTexture(target, texture);
  hookset->glGetTexParameteriv(target, pname, params);
}

void APIENTRY _glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params)
{
  PushPopTexture(target, texture);
  hookset->glGetTexParameterIiv(target, pname, params);
}

void APIENTRY _glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                            GLuint *params)
{
  PushPopTexture(target, texture);
  hookset->glGetTexParameterIuiv(target, pname, params);
}

void APIENTRY _glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level,
                                               GLenum pname, GLfloat *params)
{
  PushPopTexture(target, texture);
  hookset->glGetTexLevelParameterfv(target, level, pname, params);
}

void APIENTRY _glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level,
                                               GLenum pname, GLint *params)
{
  PushPopTexture(target, texture);
  hookset->glGetTexLevelParameteriv(target, level, pname, params);
}

void APIENTRY _glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLint border, GLenum format, GLenum type,
                                   const void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glTexImage1D(target, level, internalformat, width, border, format, type, pixels);
}

void APIENTRY _glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLsizei height, GLint border, GLenum format,
                                   GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void APIENTRY _glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                   GLenum format, GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glTexImage3D(target, level, internalformat, width, height, depth, border, format, type,
                        pixels);
}

// these two functions are a big hack. Internally we want to be using DSA functions for everything
// since then we don't have to track current bindings during replay for non-serialised calls.
// However, there are *no* DSA equivalents of the non-storage multisampled allocation functions.
// To get around this, we will emulate these functions whether or not ARB_texture_storage is
// present, and if it isn't just forward to the non-storage version which is equivalent

void APIENTRY _glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                GLenum internalformat, GLsizei width,
                                                GLsizei height, GLboolean fixedsamplelocations)
{
  PushPopTexture(target, texture);
  if(((IsGLES && GLCoreVersion >= 31) ||
      (!IsGLES && HasExt[ARB_texture_storage] && HasExt[ARB_texture_storage_multisample])) &&
     hookset->glTexStorage2DMultisample)
  {
    hookset->glTexStorage2DMultisample(target, samples, internalformat, width, height,
                                       fixedsamplelocations);
  }
  else
  {
    hookset->glTexImage2DMultisample(target, samples, internalformat, width, height,
                                     fixedsamplelocations);
  }
}

void APIENTRY _glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                GLenum internalformat, GLsizei width, GLsizei height,
                                                GLsizei depth, GLboolean fixedsamplelocations)
{
  PushPopTexture(target, texture);
  if(((IsGLES && HasExt[OES_texture_storage_multisample_2d_array]) ||
      (!IsGLES && HasExt[ARB_texture_storage] && HasExt[ARB_texture_storage_multisample])) &&
     hookset->glTexStorage3DMultisample)
  {
    hookset->glTexStorage3DMultisample(target, samples, internalformat, width, height, depth,
                                       fixedsamplelocations);
  }
  else
  {
    hookset->glTexImage3DMultisample(target, samples, internalformat, width, height, depth,
                                     fixedsamplelocations);
  }
}

void APIENTRY _glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
  PushPopTexture(target, texture);
  hookset->glTexParameterf(target, pname, param);
}

void APIENTRY _glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                       const GLfloat *params)
{
  PushPopTexture(target, texture);
  hookset->glTexParameterfv(target, pname, params);
}

void APIENTRY _glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
  PushPopTexture(target, texture);
  hookset->glTexParameteri(target, pname, param);
}

void APIENTRY _glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                       const GLint *params)
{
  PushPopTexture(target, texture);
  hookset->glTexParameteriv(target, pname, params);
}

void APIENTRY _glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                        const GLint *params)
{
  PushPopTexture(target, texture);
  hookset->glTexParameterIiv(target, pname, params);
}

void APIENTRY _glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                         const GLuint *params)
{
  PushPopTexture(target, texture);
  hookset->glTexParameterIuiv(target, pname, params);
}

void APIENTRY _glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLsizei width, GLenum format, GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

void APIENTRY _glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                      GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void APIENTRY _glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                      GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  hookset->glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                           type, pixels);
}

void APIENTRY _glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,
                                     GLenum internalformat, GLsizei width)
{
  PushPopTexture(target, texture);
  hookset->glTexStorage1D(target, levels, internalformat, width);
}

void APIENTRY _glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,
                                     GLenum internalformat, GLsizei width, GLsizei height)
{
  PushPopTexture(target, texture);
  hookset->glTexStorage2D(target, levels, internalformat, width, height);
}

void APIENTRY _glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,
                                     GLenum internalformat, GLsizei width, GLsizei height,
                                     GLsizei depth)
{
  PushPopTexture(target, texture);
  hookset->glTexStorage3D(target, levels, internalformat, width, height, depth);
}

void APIENTRY _glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                       GLenum internalformat, GLint x, GLint y, GLsizei width,
                                       GLint border)
{
  PushPopTexture(target, texture);
  hookset->glCopyTexImage1D(target, level, internalformat, x, y, width, border);
}

void APIENTRY _glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                       GLenum internalformat, GLint x, GLint y, GLsizei width,
                                       GLsizei height, GLint border)
{
  PushPopTexture(target, texture);
  hookset->glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void APIENTRY _glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                          GLint x, GLint y, GLsizei width)
{
  PushPopTexture(target, texture);
  hookset->glCopyTexSubImage1D(target, level, xoffset, x, y, width);
}

void APIENTRY _glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                          GLint yoffset, GLint x, GLint y, GLsizei width,
                                          GLsizei height)
{
  PushPopTexture(target, texture);
  hookset->glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY _glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                          GLint yoffset, GLint zoffset, GLint x, GLint y,
                                          GLsizei width, GLsizei height)
{
  PushPopTexture(target, texture);
  hookset->glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY _glGenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
  PushPopTexture(target, texture);
  hookset->glGenerateMipmap(target);
}

void APIENTRY _glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer)
{
  PushPopTexture(target, texture);
  hookset->glTexBuffer(target, internalformat, buffer);
}

void APIENTRY _glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat,
                                       GLuint buffer, GLintptr offset, GLsizeiptr size)
{
  PushPopTexture(target, texture);
  hookset->glTexBufferRange(target, internalformat, buffer, offset, size);
}

#pragma endregion

#pragma region Vertex Arrays

void APIENTRY _glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint *param)
{
  PushPopVertexArray(vaobj);
  hookset->glGetIntegerv(pname, param);
}

void APIENTRY _glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
  PushPopVertexArray(vaobj);
  hookset->glGetIntegeri_v(pname, index, param);
}

void APIENTRY _glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                  GLint size, GLenum type, GLboolean normalized,
                                                  GLsizei stride, GLintptr offset)
{
  PushPopVertexArray(vaobj);
  PushPopBuffer(eGL_ARRAY_BUFFER, buffer);
  hookset->glVertexAttribPointer(index, size, type, normalized, stride, (const void *)offset);
}

void APIENTRY _glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                   GLint size, GLenum type, GLsizei stride,
                                                   GLintptr offset)
{
  PushPopVertexArray(vaobj);
  PushPopBuffer(eGL_ARRAY_BUFFER, buffer);
  hookset->glVertexAttribIPointer(index, size, type, stride, (const void *)offset);
}

void APIENTRY _glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                   GLint size, GLenum type, GLsizei stride,
                                                   GLintptr offset)
{
  PushPopVertexArray(vaobj);
  PushPopBuffer(eGL_ARRAY_BUFFER, buffer);
  // this depends on an extension that might not be present, GL_EXT/ARB_vertex_attrib_64bit.
  // However we only use this internally if the capture used an equivalent function which would
  // rely on that extension anyway. ie. when we promote it to EXT_dsa form, we were already
  // assuming that the additional extension was available.
  hookset->glVertexAttribLPointer(index, size, type, stride, (const void *)offset);
}

void APIENTRY _glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  PushPopVertexArray(vaobj);
  hookset->glEnableVertexAttribArray(index);
}

void APIENTRY _glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  PushPopVertexArray(vaobj);
  hookset->glDisableVertexAttribArray(index);
}

void APIENTRY _glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer,
                                                GLintptr offset, GLsizei stride)
{
  PushPopVertexArray(vaobj);
  hookset->glBindVertexBuffer(bindingindex, buffer, offset, stride);
}

void APIENTRY _glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                  GLenum type, GLboolean normalized,
                                                  GLuint relativeoffset)
{
  PushPopVertexArray(vaobj);
  hookset->glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY _glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                   GLenum type, GLuint relativeoffset)
{
  PushPopVertexArray(vaobj);
  hookset->glVertexAttribIFormat(attribindex, size, type, relativeoffset);
}

void APIENTRY _glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                   GLenum type, GLuint relativeoffset)
{
  PushPopVertexArray(vaobj);
  hookset->glVertexAttribLFormat(attribindex, size, type, relativeoffset);
}

void APIENTRY _glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex,
                                                   GLuint bindingindex)
{
  PushPopVertexArray(vaobj);
  hookset->glVertexAttribBinding(attribindex, bindingindex);
}

void APIENTRY _glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
  PushPopVertexArray(vaobj);
  hookset->glVertexBindingDivisor(bindingindex, divisor);
}

void APIENTRY _glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor)
{
  PushPopVertexArray(vaobj);
  hookset->glVertexAttribDivisor(index, divisor);
}

#pragma endregion

#pragma endregion

#pragma region ARB_internalformat_query2

struct format_data
{
  GLenum fmt;
  GLenum type;
  GLint numColComp, colCompBits, depthBits, stencilBits;
};

static const format_data formats[] = {
    // colour formats
    {eGL_R8, eGL_UNSIGNED_NORMALIZED, 1, 8, 0, 0},
    {eGL_R8_SNORM, eGL_SIGNED_NORMALIZED, 1, 8, 0, 0},
    {eGL_R16, eGL_UNSIGNED_NORMALIZED, 1, 16, 0, 0},
    {eGL_R16_SNORM, eGL_SIGNED_NORMALIZED, 1, 16, 0, 0},
    {eGL_RG8, eGL_UNSIGNED_NORMALIZED, 2, 8, 0, 0},
    {eGL_RG8_SNORM, eGL_SIGNED_NORMALIZED, 2, 8, 0, 0},
    {eGL_RG16, eGL_UNSIGNED_NORMALIZED, 2, 16, 0, 0},
    {eGL_RG16_SNORM, eGL_SIGNED_NORMALIZED, 2, 16, 0, 0},
    {eGL_RGB4, eGL_UNSIGNED_NORMALIZED, 3, 4, 0, 0},
    {eGL_RGB5, eGL_UNSIGNED_NORMALIZED, 3, 5, 0, 0},
    {eGL_RGB8, eGL_UNSIGNED_NORMALIZED, 3, 8, 0, 0},
    {eGL_RGB8_SNORM, eGL_SIGNED_NORMALIZED, 3, 8, 0, 0},
    {eGL_RGB10, eGL_UNSIGNED_NORMALIZED, 3, 10, 0, 0},
    {eGL_RGB12, eGL_UNSIGNED_NORMALIZED, 3, 12, 0, 0},
    {eGL_RGB16_SNORM, eGL_SIGNED_NORMALIZED, 3, 16, 0, 0},
    {eGL_RGBA2, eGL_UNSIGNED_NORMALIZED, 4, 2, 0, 0},
    {eGL_RGBA4, eGL_UNSIGNED_NORMALIZED, 4, 4, 0, 0},
    {eGL_RGBA8, eGL_UNSIGNED_NORMALIZED, 4, 8, 0, 0},
    {eGL_RGBA8_SNORM, eGL_SIGNED_NORMALIZED, 4, 8, 0, 0},
    {eGL_RGBA12, eGL_UNSIGNED_NORMALIZED, 4, 12, 0, 0},
    {eGL_RGBA16, eGL_UNSIGNED_NORMALIZED, 4, 16, 0, 0},
    {eGL_SRGB8, eGL_UNSIGNED_NORMALIZED, 3, 8, 0, 0},
    {eGL_SRGB8_ALPHA8, eGL_UNSIGNED_NORMALIZED, 4, 8, 0, 0},
    {eGL_R16F, eGL_FLOAT, 1, 16, 0, 0},
    {eGL_RG16F, eGL_FLOAT, 2, 16, 0, 0},
    {eGL_RGB16F, eGL_FLOAT, 3, 16, 0, 0},
    {eGL_RGBA16F, eGL_FLOAT, 4, 16, 0, 0},
    {eGL_R32F, eGL_FLOAT, 1, 32, 0, 0},
    {eGL_RG32F, eGL_FLOAT, 2, 32, 0, 0},
    {eGL_RGB32F, eGL_FLOAT, 3, 32, 0, 0},
    {eGL_RGBA32F, eGL_FLOAT, 4, 32, 0, 0},
    {eGL_R8I, eGL_INT, 1, 8, 0, 0},
    {eGL_R8UI, eGL_UNSIGNED_INT, 1, 8, 0, 0},
    {eGL_R16I, eGL_INT, 1, 16, 0, 0},
    {eGL_R16UI, eGL_UNSIGNED_INT, 1, 16, 0, 0},
    {eGL_R32I, eGL_INT, 1, 32, 0, 0},
    {eGL_R32UI, eGL_UNSIGNED_INT, 1, 32, 0, 0},
    {eGL_RG8I, eGL_INT, 2, 8, 0, 0},
    {eGL_RG8UI, eGL_UNSIGNED_INT, 2, 8, 0, 0},
    {eGL_RG16I, eGL_INT, 2, 16, 0, 0},
    {eGL_RG16UI, eGL_UNSIGNED_INT, 2, 16, 0, 0},
    {eGL_RG32I, eGL_INT, 2, 32, 0, 0},
    {eGL_RG32UI, eGL_UNSIGNED_INT, 2, 32, 0, 0},
    {eGL_RGB8I, eGL_INT, 3, 8, 0, 0},
    {eGL_RGB8UI, eGL_UNSIGNED_INT, 3, 8, 0, 0},
    {eGL_RGB16I, eGL_INT, 3, 16, 0, 0},
    {eGL_RGB16UI, eGL_UNSIGNED_INT, 3, 16, 0, 0},
    {eGL_RGB32I, eGL_INT, 3, 32, 0, 0},
    {eGL_RGB32UI, eGL_UNSIGNED_INT, 3, 32, 0, 0},
    {eGL_RGBA8I, eGL_INT, 4, 8, 0, 0},
    {eGL_RGBA8UI, eGL_UNSIGNED_INT, 4, 8, 0, 0},
    {eGL_RGBA16I, eGL_INT, 4, 16, 0, 0},
    {eGL_RGBA16UI, eGL_UNSIGNED_INT, 4, 16, 0, 0},
    {eGL_RGBA32I, eGL_INT, 4, 32, 0, 0},
    {eGL_RGBA32UI, eGL_UNSIGNED_INT, 4, 32, 0, 0},

    {eGL_BGRA8_EXT, eGL_UNSIGNED_BYTE, 4, 8, 0, 0},

    // unsized formats
    {eGL_RED, eGL_UNSIGNED_BYTE, 1, 8, 0, 0},
    {eGL_RG, eGL_UNSIGNED_BYTE, 2, 8, 0, 0},
    {eGL_RGB, eGL_UNSIGNED_BYTE, 3, 8, 0, 0},
    {eGL_RGBA, eGL_UNSIGNED_BYTE, 4, 8, 0, 0},
    {eGL_BGRA_EXT, eGL_UNSIGNED_BYTE, 4, 8, 0, 0},

    // depth and stencil formats
    {eGL_DEPTH_COMPONENT16, eGL_NONE, 0, 0, 16, 0},
    {eGL_DEPTH_COMPONENT24, eGL_NONE, 0, 0, 24, 0},
    {eGL_DEPTH_COMPONENT32, eGL_NONE, 0, 0, 32, 0},
    {eGL_DEPTH_COMPONENT32F, eGL_NONE, 0, 0, 32, 0},
    {eGL_DEPTH24_STENCIL8, eGL_NONE, 0, 0, 24, 8},
    {eGL_DEPTH32F_STENCIL8, eGL_NONE, 0, 0, 32, 8},
    {eGL_STENCIL_INDEX1, eGL_NONE, 0, 0, 0, 1},
    {eGL_STENCIL_INDEX4, eGL_NONE, 0, 0, 0, 4},
    {eGL_STENCIL_INDEX8, eGL_NONE, 0, 0, 0, 8},
    {eGL_STENCIL_INDEX16, eGL_NONE, 0, 0, 0, 16},
};

static const GLenum viewClasses[] = {
    eGL_VIEW_CLASS_8_BITS,
    eGL_VIEW_CLASS_16_BITS,
    eGL_VIEW_CLASS_24_BITS,
    eGL_VIEW_CLASS_32_BITS,
    eGL_NONE,    // 40 bits
    eGL_VIEW_CLASS_48_BITS,
    eGL_NONE,    // 56 bits
    eGL_VIEW_CLASS_64_BITS,
    eGL_NONE,    // 72 bits
    eGL_NONE,    // 80 bits
    eGL_NONE,    // 88 bits
    eGL_VIEW_CLASS_96_BITS,
    eGL_NONE,    // 104 bits
    eGL_NONE,    // 112 bits
    eGL_NONE,    // 120 bits
    eGL_VIEW_CLASS_128_BITS,
};

void APIENTRY _glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                     GLsizei bufSize, GLint *params)
{
  // We only implement the subset of the queries that we care about ourselves.
  // We also skip special formats, and only return values for

  if(glGetInternalformativ_real)
  {
    switch(pname)
    {
      // these params are available for both GLES and GL
      case eGL_NUM_SAMPLE_COUNTS:
      case eGL_SAMPLES:
        glGetInternalformativ_real(target, internalformat, pname, bufSize, params);
        return;

      default: break;
    }
  }

  if(IsCompressedFormat(internalformat))
  {
    RDCERR("Compressed formats not supported by internal glGetInternalformativ");
    return;
  }

  if(pname == eGL_COLOR_ENCODING)
  {
    if(internalformat == eGL_SRGB8 || internalformat == eGL_SRGB8_ALPHA8)
      *params = eGL_SRGB;
    else
      *params = eGL_LINEAR;

    return;
  }

  const format_data *data = NULL;

  for(size_t i = 0; i < ARRAY_COUNT(formats); i++)
  {
    if(formats[i].fmt == internalformat)
    {
      data = &formats[i];
      break;
    }
  }

  if(data == NULL)
  {
    RDCERR("Format %s not supported by internal glGetInternalformativ, update database",
           ToStr::Get(internalformat).c_str());
    return;
  }

  switch(pname)
  {
    case eGL_VIEW_COMPATIBILITY_CLASS:
      if(data->numColComp > 0)
        *params = (GLint)viewClasses[data->numColComp * data->colCompBits / 8];
      else
        *params = (GLint)viewClasses[(data->depthBits + data->stencilBits) / 8];
      break;
    case eGL_COLOR_COMPONENTS: *params = (data->numColComp > 0 ? GL_TRUE : GL_FALSE); break;
    case eGL_DEPTH_COMPONENTS: *params = (data->depthBits > 0 ? GL_TRUE : GL_FALSE); break;
    case eGL_STENCIL_COMPONENTS: *params = (data->stencilBits > 0 ? GL_TRUE : GL_FALSE); break;
    case eGL_INTERNALFORMAT_RED_SIZE:
      *params = (data->numColComp > 0 ? data->colCompBits : 0);
      break;
    case eGL_INTERNALFORMAT_GREEN_SIZE:
      *params = (data->numColComp > 1 ? data->colCompBits : 0);
      break;
    case eGL_INTERNALFORMAT_BLUE_SIZE:
      *params = (data->numColComp > 2 ? data->colCompBits : 0);
      break;
    case eGL_INTERNALFORMAT_ALPHA_SIZE:
      *params = (data->numColComp > 3 ? data->colCompBits : 0);
      break;
    case eGL_INTERNALFORMAT_RED_TYPE:
      *params = GLint(data->numColComp > 0 ? data->type : eGL_NONE);
      break;
    case eGL_INTERNALFORMAT_GREEN_TYPE:
      *params = GLint(data->numColComp > 1 ? data->type : eGL_NONE);
      break;
    case eGL_INTERNALFORMAT_BLUE_TYPE:
      *params = GLint(data->numColComp > 2 ? data->type : eGL_NONE);
      break;
    case eGL_INTERNALFORMAT_ALPHA_TYPE:
      *params = GLint(data->numColComp > 3 ? data->type : eGL_NONE);
      break;
    case eGL_INTERNALFORMAT_DEPTH_SIZE: *params = data->depthBits; break;
    case eGL_INTERNALFORMAT_STENCIL_SIZE: *params = data->stencilBits; break;
    default:
      RDCERR("pname %s not supported by internal glGetInternalformativ", ToStr::Get(pname).c_str());
      break;
  }
}

#pragma endregion

#pragma region ARB_copy_image

void APIENTRY _glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX,
                                  GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget,
                                  GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                                  GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
  GLuint fbos[2] = {};
  GLuint &readFBO = fbos[0];
  GLuint &drawFBO = fbos[1];

  hookset->glGenFramebuffers(2, fbos);

  RDCASSERTEQUAL(srcTarget, dstTarget);

  {
    PushPopFramebuffer(eGL_READ_FRAMEBUFFER, readFBO);
    PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);

    GLbitfield mask = eGL_COLOR_BUFFER_BIT;
    GLenum attach = eGL_COLOR_ATTACHMENT0;

    bool layered = false;
    bool compressed = false;

    if(srcTarget == eGL_TEXTURE_CUBE_MAP || srcTarget == eGL_TEXTURE_CUBE_MAP_ARRAY ||
       srcTarget == eGL_TEXTURE_1D_ARRAY || srcTarget == eGL_TEXTURE_2D_ARRAY ||
       srcTarget == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY || srcTarget == eGL_TEXTURE_3D)
    {
      layered = true;
    }

    {
      PushPopTexture(srcTarget, srcName);

      GLenum levelQueryType = srcTarget;
      if(levelQueryType == eGL_TEXTURE_CUBE_MAP)
        levelQueryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

      GLenum fmt = eGL_NONE;
      hookset->glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT,
                                        (GLint *)&fmt);

      if(IsCompressedFormat(fmt))
      {
        // have to do this via CPU readback, there's no alternative for GPU copies
        compressed = true;

        GLenum targets[] = {
            eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        };

        int count = ARRAY_COUNT(targets);

        if(srcTarget != eGL_TEXTURE_CUBE_MAP)
        {
          targets[0] = srcTarget;
          count = 1;
        }

        size_t size = GetCompressedByteSize(srcWidth, srcHeight, srcDepth, fmt);

        if(srcTarget == eGL_TEXTURE_CUBE_MAP)
          size /= 6;

        byte *buf = new byte[size];

        for(int trg = 0; trg < count; trg++)
        {
          // read to CPU
          if(IsGLES)
          {
            RDCERR("Can't emulate glCopyImageSubData without glGetCompressedTexImage on GLES");
            memset(buf, 0, size);
          }
          else
          {
            hookset->glGetCompressedTextureImageEXT(srcName, targets[trg], srcLevel, buf);
          }

          // write to GPU
          if(srcTarget == eGL_TEXTURE_1D || srcTarget == eGL_TEXTURE_1D_ARRAY)
            hookset->glCompressedTextureSubImage1DEXT(dstName, targets[trg], dstLevel, 0, srcWidth,
                                                      fmt, (GLsizei)size, buf);
          else if(srcTarget == eGL_TEXTURE_3D)
            hookset->glCompressedTextureSubImage3DEXT(dstName, targets[trg], dstLevel, 0, 0, 0,
                                                      srcWidth, srcHeight, srcDepth, fmt,
                                                      (GLsizei)size, buf);
          else
            hookset->glCompressedTextureSubImage2DEXT(dstName, targets[trg], dstLevel, 0, 0,
                                                      srcWidth, srcHeight, fmt, (GLsizei)size, buf);
        }

        delete[] buf;
      }
      else
      {
        ResourceFormat format = MakeResourceFormat(*hookset, srcTarget, fmt);

        GLenum baseFormat = GetBaseFormat(fmt);

        if(baseFormat == eGL_DEPTH_COMPONENT)
        {
          mask = eGL_DEPTH_BUFFER_BIT;
          attach = eGL_DEPTH_ATTACHMENT;
        }
        else if(baseFormat == eGL_STENCIL_INDEX)
        {
          mask = eGL_STENCIL_BUFFER_BIT;
          attach = eGL_STENCIL_ATTACHMENT;
        }
        else if(baseFormat == eGL_DEPTH_STENCIL)
        {
          mask = eGL_DEPTH_BUFFER_BIT | eGL_STENCIL_BUFFER_BIT;
          attach = eGL_DEPTH_STENCIL_ATTACHMENT;
        }

        // simple case, non-layered. If we have a layered copy we need to loop and rebind
        if(!layered)
        {
          // we assume the destination texture is the same format, and we asserted that it's the
          // same
          // target.
          if(srcTarget == eGL_TEXTURE_2D || srcTarget == eGL_TEXTURE_2D_MULTISAMPLE)
          {
            hookset->glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, attach, srcTarget, srcName,
                                            srcLevel);
            hookset->glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attach, dstTarget, dstName,
                                            dstLevel);
          }
          else
          {
            hookset->glFramebufferTexture(eGL_READ_FRAMEBUFFER, attach, srcName, srcLevel);
            hookset->glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, attach, dstName, dstLevel);
          }
        }
      }
    }

    if(compressed)
    {
      // nothing to do!
    }
    else if(!layered)
    {
      hookset->glBlitFramebuffer(srcX, srcY, srcX + srcWidth, srcY + srcHeight, dstX, dstY,
                                 dstX + srcWidth, dstY + srcHeight, mask, eGL_NEAREST);
    }
    else if(srcTarget == eGL_TEXTURE_CUBE_MAP)
    {
      // cubemap non-array must be treated differently to use glFramebufferTexture2D

      GLenum textargets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      for(GLsizei slice = 0; slice < srcDepth; slice++)
      {
        hookset->glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, attach, textargets[srcZ + slice],
                                        srcName, srcLevel);
        hookset->glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attach, textargets[dstZ + slice],
                                        dstName, dstLevel);

        hookset->glBlitFramebuffer(srcX, srcY, srcX + srcWidth, srcY + srcHeight, dstX, dstY,
                                   dstX + srcWidth, dstY + srcHeight, mask, eGL_NEAREST);
      }
    }
    else
    {
      for(GLsizei slice = 0; slice < srcDepth; slice++)
      {
        hookset->glFramebufferTextureLayer(eGL_READ_FRAMEBUFFER, attach, srcName, srcLevel,
                                           srcZ + slice);
        hookset->glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, dstName, dstLevel,
                                           dstZ + slice);

        hookset->glBlitFramebuffer(srcX, srcY, srcX + srcWidth, srcY + srcHeight, dstX, dstY,
                                   dstX + srcWidth, dstY + srcHeight, mask, eGL_NEAREST);
      }
    }
  }

  hookset->glDeleteFramebuffers(2, fbos);
}

#pragma endregion

#pragma region ARB_clear_buffer_object

void APIENTRY _glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset,
                                    GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
  byte *dst = (byte *)hookset->glMapBufferRange(target, offset, size,
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);

  if(data == NULL)
  {
    memset(dst, 0, size);
  }
  else
  {
    // this is not a full implementation, just enough for internal use

    uint32_t compCount = 1;

    switch(format)
    {
      case GL_RED:
      case GL_RED_INTEGER: compCount = 1; break;
      case GL_RG:
      case GL_RG_INTEGER: compCount = 2; break;
      case GL_RGB:
      case GL_RGB_INTEGER: compCount = 3; break;
      case GL_RGBA:
      case GL_RGBA_INTEGER: compCount = 4; break;
      default:
        RDCERR("Unexpected format %s, not doing conversion. Update _glClearBufferSubData emulation",
               ToStr::Get(format).c_str());
    }

    uint32_t compByteWidth = 1;
    switch(type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_BYTE: compByteWidth = 1; break;
      case eGL_UNSIGNED_SHORT:
      case eGL_SHORT: compByteWidth = 2; break;
      case eGL_UNSIGNED_INT:
      case eGL_INT:
      case eGL_FLOAT: compByteWidth = 4; break;
      default:
        RDCERR("Unexpected type %s, not doing conversion. Update _glClearBufferSubData emulation",
               ToStr::Get(type).c_str());
    }

    CompType compType = CompType::UInt;

    switch(type)
    {
      case eGL_UNSIGNED_BYTE:
      case eGL_UNSIGNED_SHORT:
      case eGL_UNSIGNED_INT: compType = CompType::UInt; break;
      case eGL_BYTE:
      case eGL_SHORT:
      case eGL_INT: compType = CompType::SInt; break;
      case eGL_FLOAT: compType = CompType::Float; break;
      default: break;
    }

    ResourceFormat fmt = MakeResourceFormat(*hookset, eGL_TEXTURE_2D, internalformat);

    // ensure we don't need any conversion
    if(compByteWidth != fmt.compByteWidth)
      RDCERR(
          "Unexpected mismatch between app-data (%u bytes) and internal format (%u bytes). Update "
          "_glClearBufferSubData emulation",
          compByteWidth, fmt.compByteWidth);

    if(compCount != fmt.compCount)
      RDCERR(
          "Unexpected mismatch between app-data (%u components) and internal format (%u "
          "components). Update _glClearBufferSubData emulation",
          compCount, fmt.compCount);

    if(compType != fmt.compType)
      RDCERR(
          "Unexpected mismatch between app-data (%d type) and internal format (%d type). Update "
          "_glClearBufferSubData emulation",
          compType, fmt.compType);

    size_t stride = compCount * compByteWidth;

    RDCASSERT(size % stride == 0, uint64_t(size), uint64_t(stride));

    // copy without conversion
    for(GLsizeiptr i = 0; i < size; i += stride)
      memcpy(dst + i, data, stride);
  }

  hookset->glUnmapBuffer(target);
}

void APIENTRY _glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type,
                                 const void *data)
{
  GLint size = 0;
  hookset->glGetBufferParameteriv(target, eGL_BUFFER_SIZE, &size);

  _glClearBufferSubData(target, internalformat, 0, (GLsizeiptr)size, format, type, data);
}

#pragma endregion

#pragma region GLES Compatibility

void APIENTRY _glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
  if((format == eGL_DEPTH_COMPONENT && !HasExt[NV_read_depth]) ||
     (format == eGL_STENCIL && !HasExt[NV_read_stencil]) ||
     (format == eGL_DEPTH_STENCIL && !HasExt[NV_read_depth_stencil]))
  {
    // TODO create a workaround for this
    // return silently, check was made during startup
    return;
  }

  switch(target)
  {
    case eGL_TEXTURE_1D:
    case eGL_TEXTURE_1D_ARRAY:
      RDCWARN("1d and 1d array textures are not supported by GLES");
      return;

    case eGL_TEXTURE_BUFFER:
      // TODO implement this
      GLNOTIMP("Reading pixels from texture buffer");
      return;

    default: break;
  }

  GLint width = 0, height = 0, depth = 0;
  hookset->glGetTexLevelParameteriv(target, level, eGL_TEXTURE_WIDTH, &width);
  hookset->glGetTexLevelParameteriv(target, level, eGL_TEXTURE_HEIGHT, &height);
  hookset->glGetTexLevelParameteriv(target, level, eGL_TEXTURE_DEPTH, &depth);

  GLint texture = 0;
  hookset->glGetIntegerv(TextureBinding(target), (GLint *)&texture);

  GLenum attachment = eGL_COLOR_ATTACHMENT0;
  if(format == eGL_DEPTH_COMPONENT)
    attachment = eGL_DEPTH_ATTACHMENT;
  else if(format == eGL_STENCIL)
    attachment = eGL_STENCIL_ATTACHMENT;
  else if(format == eGL_DEPTH_STENCIL)
    attachment = eGL_DEPTH_STENCIL_ATTACHMENT;

  GLuint fbo = 0;
  hookset->glGenFramebuffers(1, &fbo);

  PushPopFramebuffer(eGL_FRAMEBUFFER, fbo);

  size_t sliceSize = GetByteSize(width, height, 1, format, type);

  for(GLint d = 0; d < depth; ++d)
  {
    switch(target)
    {
      case eGL_TEXTURE_3D:
      case eGL_TEXTURE_2D_ARRAY:
      case eGL_TEXTURE_CUBE_MAP_ARRAY:
      case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        hookset->glFramebufferTextureLayer(eGL_FRAMEBUFFER, attachment, texture, level, d);
        break;

      case eGL_TEXTURE_CUBE_MAP:
      case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      case eGL_TEXTURE_2D:
      case eGL_TEXTURE_2D_MULTISAMPLE:
      default:
        hookset->glFramebufferTexture2D(eGL_FRAMEBUFFER, attachment, target, texture, level);
        break;
    }

    byte *dst = (byte *)pixels + d * sliceSize;
    hookset->glReadPixels(0, 0, width, height, format, type, (void *)dst);
  }

  hookset->glDeleteFramebuffers(1, &fbo);
}

void APIENTRY _glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                        const void *indices, GLint basevertex)
{
  if(basevertex == 0)
    hookset->glDrawElements(mode, count, type, indices);
  else
    RDCERR("glDrawElementsBaseVertex is not supported! No draw will be called!");
}

void APIENTRY _glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                 const void *indices, GLsizei instancecount,
                                                 GLint basevertex)
{
  if(basevertex == 0)
    hookset->glDrawElementsInstanced(mode, count, type, indices, instancecount);
  else
    RDCERR("glDrawElementsInstancedBaseVertex is not supported! No draw will be called!");
}

void APIENTRY _glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                             GLenum type, const void *indices, GLint basevertex)
{
  if(basevertex == 0)
    hookset->glDrawRangeElements(mode, start, end, count, type, indices);
  else
    RDCERR("glDrawRangeElementsBaseVertex is not supported! No draw will be called!");
}

#pragma endregion

void EmulateRequiredExtensions(GLHookSet *hooks)
{
#define EMULATE_FUNC(func) hooks->func = &CONCAT(_, func);

  hookset = hooks;

  // this one is more complex as we need to implement the copy ourselves by creating FBOs and doing
  // a blit. Obviously this is going to be slower, but if we're emulating the extension then
  // performance is not the top priority.
  if(!HasExt[ARB_copy_image])
  {
    RDCLOG("Emulating ARB_copy_image");
    EMULATE_FUNC(glCopyImageSubData);
  }

  // this we implement as Map + memset + Unmap
  if(!HasExt[ARB_clear_buffer_object])
  {
    RDCLOG("Emulating ARB_clear_buffer_object");
    EMULATE_FUNC(glClearBufferData);
    EMULATE_FUNC(glClearBufferSubData);
  }

  // we manually implement these queries
  if(!HasExt[ARB_internalformat_query2])
  {
    RDCLOG("Emulating ARB_internalformat_query2");
    // GLES supports glGetInternalformativ from core 3.0 but it only accepts GL_NUM_SAMPLE_COUNTS
    // and GL_SAMPLES params
    if(!glGetInternalformativ_real && hooks->glGetInternalformativ != _glGetInternalformativ)
      glGetInternalformativ_real = hooks->glGetInternalformativ;
    EMULATE_FUNC(glGetInternalformativ);
  }

  // APIs that are not available at all in GLES.
  if(IsGLES)
  {
    EMULATE_FUNC(glGetBufferSubData);
    EMULATE_FUNC(glGetTexImage);

    if(GLCoreVersion < 32)
    {
      EMULATE_FUNC(glDrawElementsBaseVertex);
      EMULATE_FUNC(glDrawElementsInstancedBaseVertex);
      EMULATE_FUNC(glDrawRangeElementsBaseVertex);
    }
  }

  // Emulate the EXT_dsa functions that we'll need.
  //
  // We need to emulate EXT_dsa functions even if we don't use them directly. During capture, if the
  // application calls glBufferStorage or glGenerateMipmap then we call the corresponding function -
  // so if EXT_dsa is unused and the app doesn't call it then we won't call it either during
  // capture.
  //
  // However we serialise all glGenerateMipmap variants - glGenerateMultiTexMipmapEXT,
  // glGenerateTextureMipmap, glGenerateTextureMipmapEXT the same way, using the EXT_dsa function.
  //
  // So the end result is that we need to emulate any EXT_dsa functions that we call ourselves
  // independently during replay (e.g. for analysis work or initial state handling) and ALSO any
  // functions that we might promote to for ease of serialising.
  //
  // We don't have to emulate functions that we neither call directly, nor promote to, so e.g.
  // MultiTex functions.
  if(!HasExt[EXT_direct_state_access])
  {
    RDCLOG("Emulating EXT_direct_state_access");
    EMULATE_FUNC(glCheckNamedFramebufferStatusEXT);
    EMULATE_FUNC(glClearNamedBufferDataEXT);
    EMULATE_FUNC(glClearNamedBufferSubDataEXT);
    EMULATE_FUNC(glCompressedTextureImage1DEXT);
    EMULATE_FUNC(glCompressedTextureImage2DEXT);
    EMULATE_FUNC(glCompressedTextureImage3DEXT);
    EMULATE_FUNC(glCompressedTextureSubImage1DEXT);
    EMULATE_FUNC(glCompressedTextureSubImage2DEXT);
    EMULATE_FUNC(glCompressedTextureSubImage3DEXT);
    EMULATE_FUNC(glCopyTextureImage1DEXT);
    EMULATE_FUNC(glCopyTextureImage2DEXT);
    EMULATE_FUNC(glCopyTextureSubImage1DEXT);
    EMULATE_FUNC(glCopyTextureSubImage2DEXT);
    EMULATE_FUNC(glCopyTextureSubImage3DEXT);
    EMULATE_FUNC(glDisableVertexArrayAttribEXT);
    EMULATE_FUNC(glEnableVertexArrayAttribEXT);
    EMULATE_FUNC(glFlushMappedNamedBufferRangeEXT);
    EMULATE_FUNC(glFramebufferDrawBufferEXT);
    EMULATE_FUNC(glFramebufferDrawBuffersEXT);
    EMULATE_FUNC(glFramebufferReadBufferEXT);
    EMULATE_FUNC(glGenerateTextureMipmapEXT);
    EMULATE_FUNC(glGetCompressedTextureImageEXT);
    EMULATE_FUNC(glGetNamedBufferParameterivEXT);
    EMULATE_FUNC(glGetNamedBufferSubDataEXT);
    EMULATE_FUNC(glGetNamedFramebufferAttachmentParameterivEXT);
    EMULATE_FUNC(glGetNamedFramebufferParameterivEXT);
    EMULATE_FUNC(glGetNamedRenderbufferParameterivEXT);
    EMULATE_FUNC(glGetTextureImageEXT);
    EMULATE_FUNC(glGetTextureLevelParameterfvEXT);
    EMULATE_FUNC(glGetTextureLevelParameterivEXT);
    EMULATE_FUNC(glGetTextureParameterfvEXT);
    EMULATE_FUNC(glGetTextureParameterIivEXT);
    EMULATE_FUNC(glGetTextureParameterIuivEXT);
    EMULATE_FUNC(glGetTextureParameterivEXT);
    EMULATE_FUNC(glGetVertexArrayIntegeri_vEXT);
    EMULATE_FUNC(glGetVertexArrayIntegervEXT);
    EMULATE_FUNC(glMapNamedBufferEXT);
    EMULATE_FUNC(glMapNamedBufferRangeEXT);
    EMULATE_FUNC(glNamedBufferDataEXT);
    EMULATE_FUNC(glNamedBufferStorageEXT);
    EMULATE_FUNC(glNamedBufferSubDataEXT);
    EMULATE_FUNC(glNamedCopyBufferSubDataEXT);
    EMULATE_FUNC(glNamedFramebufferParameteriEXT);
    EMULATE_FUNC(glNamedFramebufferRenderbufferEXT);
    EMULATE_FUNC(glNamedFramebufferTexture1DEXT);
    EMULATE_FUNC(glNamedFramebufferTexture2DEXT);
    EMULATE_FUNC(glNamedFramebufferTexture3DEXT);
    EMULATE_FUNC(glNamedFramebufferTextureEXT);
    EMULATE_FUNC(glNamedFramebufferTextureLayerEXT);
    EMULATE_FUNC(glNamedRenderbufferStorageEXT);
    EMULATE_FUNC(glNamedRenderbufferStorageMultisampleEXT);
    EMULATE_FUNC(glTextureBufferEXT);
    EMULATE_FUNC(glTextureBufferRangeEXT);
    EMULATE_FUNC(glTextureImage1DEXT);
    EMULATE_FUNC(glTextureImage2DEXT);
    EMULATE_FUNC(glTextureImage3DEXT);
    EMULATE_FUNC(glTextureParameterfEXT);
    EMULATE_FUNC(glTextureParameterfvEXT);
    EMULATE_FUNC(glTextureParameteriEXT);
    EMULATE_FUNC(glTextureParameterIivEXT);
    EMULATE_FUNC(glTextureParameterIuivEXT);
    EMULATE_FUNC(glTextureParameterivEXT);
    EMULATE_FUNC(glTextureStorage1DEXT);
    EMULATE_FUNC(glTextureStorage2DEXT);
    EMULATE_FUNC(glTextureStorage2DMultisampleEXT);
    EMULATE_FUNC(glTextureStorage3DEXT);
    EMULATE_FUNC(glTextureStorage3DMultisampleEXT);
    EMULATE_FUNC(glTextureSubImage1DEXT);
    EMULATE_FUNC(glTextureSubImage2DEXT);
    EMULATE_FUNC(glTextureSubImage3DEXT);
    EMULATE_FUNC(glUnmapNamedBufferEXT);
    EMULATE_FUNC(glVertexArrayBindVertexBufferEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribBindingEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribDivisorEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribFormatEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribIFormatEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribIOffsetEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribLFormatEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribLOffsetEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribOffsetEXT);
    EMULATE_FUNC(glVertexArrayVertexBindingDivisorEXT);
  }
}

};    // namespace glEmulate
