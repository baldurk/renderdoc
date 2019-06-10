/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "driver/gl/gl_dispatch_table.h"
#include "driver/gl/gl_driver.h"
#include "driver/gl/gl_resources.h"
#include "driver/shaders/spirv/glslang_compile.h"

namespace glEmulate
{
PFNGLGETINTERNALFORMATIVPROC glGetInternalformativ_real = NULL;

PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_real = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer_real = NULL;
PFNGLVERTEXATTRIBLPOINTERPROC glVertexAttribLPointer_real = NULL;
PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor_real = NULL;

PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv_real = NULL;
PFNGLGETINTEGERVPROC glGetIntegerv_real = NULL;
PFNGLGETINTEGERI_VPROC glGetIntegeri_v_real = NULL;
PFNGLGETINTEGER64I_VPROC glGetInteger64i_v_real = NULL;

PFNGLGETPROGRAMIVPROC glGetProgramiv_real = NULL;

WrappedOpenGL *driver = NULL;

typedef GLenum (*BindingLookupFunc)(GLenum target);

struct PushPop
{
  enum VAOMode
  {
    VAO
  };
  enum ProgramMode
  {
    Program
  };

  // we can use PFNGLBINDTEXTUREPROC since most bind functions are identical - taking GLenum and
  // GLuint.
  PushPop(GLenum target, PFNGLBINDTEXTUREPROC bindFunc, BindingLookupFunc bindingLookup)
  {
    other = bindFunc;
    t = target;
    GL.glGetIntegerv(bindingLookup(target), (GLint *)&o);
  }

  PushPop(GLenum target, PFNGLBINDTEXTUREPROC bindFunc, GLenum binding)
  {
    other = bindFunc;
    t = target;
    GL.glGetIntegerv(binding, (GLint *)&o);
  }

  PushPop(VAOMode, PFNGLBINDVERTEXARRAYPROC bindFunc)
  {
    vao = bindFunc;
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&o);
  }

  PushPop(ProgramMode, PFNGLUSEPROGRAMPROC bindFunc)
  {
    prog = bindFunc;
    GL.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&o);
  }

  ~PushPop()
  {
    if(vao)
      vao(o);
    else if(prog)
      prog(o);
    else if(other)
      other(t, o);
  }

  PFNGLUSEPROGRAMPROC prog = NULL;
  PFNGLBINDVERTEXARRAYPROC vao = NULL;
  PFNGLBINDTEXTUREPROC other = NULL;

  GLenum t = eGL_NONE;
  GLuint o = 0;
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

#define PushPopTexture(target, obj)                                              \
  GLenum bindtarget = TexBindTarget(target);                                     \
  PushPop CONCAT(prev, __LINE__)(bindtarget, GL.glBindTexture, &TextureBinding); \
  GL.glBindTexture(bindtarget, obj);

#define PushPopBuffer(target, obj)                                         \
  PushPop CONCAT(prev, __LINE__)(target, GL.glBindBuffer, &BufferBinding); \
  GL.glBindBuffer(target, obj);

#define PushPopXFB(obj)                                                              \
  PushPop CONCAT(prev, __LINE__)(eGL_TRANSFORM_FEEDBACK, GL.glBindTransformFeedback, \
                                 eGL_TRANSFORM_FEEDBACK_BINDING);                    \
  GL.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, obj);

#define PushPopFramebuffer(target, obj)                                              \
  PushPop CONCAT(prev, __LINE__)(target, GL.glBindFramebuffer, &FramebufferBinding); \
  GL.glBindFramebuffer(target, obj);

#define PushPopRenderbuffer(obj)                                                                     \
  PushPop CONCAT(prev, __LINE__)(eGL_RENDERBUFFER, GL.glBindRenderbuffer, eGL_RENDERBUFFER_BINDING); \
  GL.glBindRenderbuffer(eGL_RENDERBUFFER, obj);

#define PushPopVertexArray(obj)                                       \
  PushPop CONCAT(prev, __LINE__)(PushPop::VAO, GL.glBindVertexArray); \
  GL.glBindVertexArray(obj);

#define PushPopProgram(obj)                                          \
  PushPop CONCAT(prev, __LINE__)(PushPop::Program, GL.glUseProgram); \
  GL.glUseProgram(obj);

void APIENTRY _glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
  PushPopXFB(xfb);
  GL.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
}

void APIENTRY _glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                              GLintptr offset, GLsizeiptr size)
{
  PushPopXFB(xfb);
  GL.glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, index, buffer, offset, size);
}

void APIENTRY _glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                         const GLint *value)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glClearBufferiv(buffer, drawbuffer, value);
}

void APIENTRY _glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          const GLuint *value)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glClearBufferuiv(buffer, drawbuffer, value);
}

void APIENTRY _glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                         const GLfloat *value)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glClearBufferfv(buffer, drawbuffer, value);
}

void APIENTRY _glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, int drawbuffer,
                                         const GLfloat depth, GLint stencil)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glClearBufferfi(buffer, drawbuffer, depth, stencil);
}

void APIENTRY _glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0,
                                      GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                                      GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
  PushPopFramebuffer(eGL_READ_FRAMEBUFFER, readFramebuffer);
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFramebuffer);
  GL.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY _glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
  PushPopVertexArray(vaobj);
  GL.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);
}

void APIENTRY _glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                                          const GLuint *buffers, const GLintptr *offsets,
                                          const GLsizei *strides)
{
  PushPopVertexArray(vaobj);
  GL.glBindVertexBuffers(first, count, buffers, offsets, strides);
}

void APIENTRY _glClearDepthf(GLfloat d)
{
  GL.glClearDepth(d);
}

#pragma region EXT_direct_state_access

#pragma region Framebuffers

void APIENTRY _glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment,
                                                             GLenum pname, GLint *params)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachment, pname, params);
}

GLenum APIENTRY _glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target)
{
  PushPopFramebuffer(target, framebuffer);
  return GL.glCheckFramebufferStatus(target);
}

void APIENTRY _glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint *params)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glGetFramebufferParameteriv(eGL_DRAW_FRAMEBUFFER, pname, params);
}

void APIENTRY _glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment,
                                              GLenum textarget, GLuint texture, GLint level)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferTexture1D(eGL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level);
}

void APIENTRY _glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment,
                                              GLenum textarget, GLuint texture, GLint level)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level);
}

void APIENTRY _glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget,
                                              GLuint texture, GLint level, GLint zoffset)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferTexture3D(eGL_DRAW_FRAMEBUFFER, attachment, textarget, texture, level, zoffset);
}

void APIENTRY _glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture,
                                            GLint level)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, attachment, texture, level);
}

void APIENTRY _glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment,
                                                 GLuint texture, GLint level, GLint layer)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attachment, texture, level, layer);
}

void APIENTRY _glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferParameteri(eGL_DRAW_FRAMEBUFFER, pname, param);
}

void APIENTRY _glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum mode)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glDrawBuffer(mode);
}

void APIENTRY _glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glDrawBuffers(n, bufs);
}

void APIENTRY _glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode)
{
  PushPopFramebuffer(eGL_READ_FRAMEBUFFER, framebuffer);
  GL.glReadBuffer(mode);
}

void APIENTRY _glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment,
                                                 GLenum renderbuffertarget, GLuint renderbuffer)
{
  PushPopFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  GL.glFramebufferRenderbuffer(eGL_DRAW_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
}

#pragma endregion

#pragma region Renderbuffers

void APIENTRY _glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat,
                                             GLsizei width, GLsizei height)
{
  PushPopRenderbuffer(renderbuffer);
  GL.glRenderbufferStorage(eGL_RENDERBUFFER, internalformat, width, height);
}

void APIENTRY _glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height)
{
  PushPopRenderbuffer(renderbuffer);
  GL.glRenderbufferStorageMultisample(eGL_RENDERBUFFER, samples, internalformat, width, height);
}

void APIENTRY _glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname, GLint *params)
{
  PushPopRenderbuffer(renderbuffer);
  GL.glGetRenderbufferParameteriv(eGL_RENDERBUFFER, pname, params);
}

#pragma endregion

#pragma region Buffers

void APIENTRY _glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glGetBufferParameteriv(eGL_COPY_READ_BUFFER, pname, params);
}

void APIENTRY _glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
  void *bufData = GL.glMapBufferRange(target, offset, size, eGL_MAP_READ_BIT);
  if(!bufData)
  {
    RDCERR("glMapBufferRange failed to map buffer.");
    return;
  }
  memcpy(data, bufData, (size_t)size);
  GL.glUnmapBuffer(target);
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
  GL.glGetBufferParameteriv(eGL_COPY_READ_BUFFER, eGL_BUFFER_SIZE, &size);

  GLbitfield accessBits = eGL_MAP_READ_BIT | eGL_MAP_WRITE_BIT;

  if(access == eGL_READ_ONLY)
    accessBits = eGL_MAP_READ_BIT;
  else if(access == eGL_WRITE_ONLY)
    accessBits = eGL_MAP_WRITE_BIT;

  return GL.glMapBufferRange(eGL_COPY_READ_BUFFER, 0, size, accessBits);
}

void *APIENTRY _glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length,
                                         GLbitfield access)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  return GL.glMapBufferRange(eGL_COPY_READ_BUFFER, offset, length, access);
}

void APIENTRY _glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glFlushMappedBufferRange(eGL_COPY_READ_BUFFER, offset, length);
}

GLboolean APIENTRY _glUnmapNamedBufferEXT(GLuint buffer)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  return GL.glUnmapBuffer(eGL_COPY_READ_BUFFER);
}

void APIENTRY _glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format,
                                         GLenum type, const void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glClearBufferData(eGL_COPY_READ_BUFFER, internalformat, format, type, data);
}

void APIENTRY _glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLsizeiptr offset,
                                            GLsizeiptr size, GLenum format, GLenum type,
                                            const void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glClearBufferSubData(eGL_COPY_READ_BUFFER, internalformat, offset, size, format, type, data);
}

void APIENTRY _glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glBufferData(eGL_COPY_READ_BUFFER, size, data, usage);
}

void APIENTRY _glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data,
                                       GLbitfield flags)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glBufferStorage(eGL_COPY_READ_BUFFER, size, data, flags);
}

void APIENTRY _glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size,
                                       const void *data)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, buffer);
  GL.glBufferSubData(eGL_COPY_READ_BUFFER, offset, size, data);
}

void APIENTRY _glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer,
                                           GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
  PushPopBuffer(eGL_COPY_READ_BUFFER, readBuffer);
  PushPopBuffer(eGL_COPY_WRITE_BUFFER, writeBuffer);
  GL.glCopyBufferSubData(eGL_COPY_READ_BUFFER, eGL_COPY_WRITE_BUFFER, readOffset, writeOffset, size);
}

#pragma endregion

#pragma region Textures

void APIENTRY _glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                             GLenum internalformat, GLsizei width, GLint border,
                                             GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  GL.glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, bits);
}

void APIENTRY _glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                             GLenum internalformat, GLsizei width, GLsizei height,
                                             GLint border, GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  GL.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, bits);
}

void APIENTRY _glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                             GLenum internalformat, GLsizei width, GLsizei height,
                                             GLsizei depth, GLint border, GLsizei imageSize,
                                             const void *bits)
{
  PushPopTexture(target, texture);
  GL.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize,
                            bits);
}

void APIENTRY _glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level,
                                                GLint xoffset, GLsizei width, GLenum format,
                                                GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  GL.glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, bits);
}

void APIENTRY _glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLsizei width,
                                                GLsizei height, GLenum format, GLsizei imageSize,
                                                const void *bits)
{
  PushPopTexture(target, texture);
  GL.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize,
                               bits);
}

void APIENTRY _glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint zoffset,
                                                GLsizei width, GLsizei height, GLsizei depth,
                                                GLenum format, GLsizei imageSize, const void *bits)
{
  PushPopTexture(target, texture);
  GL.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                               format, imageSize, bits);
}

void APIENTRY _glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void *img)
{
  PushPopTexture(target, texture);
  GL.glGetCompressedTexImage(target, lod, img);
}

void APIENTRY _glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format,
                                    GLenum type, void *pixels)
{
  PushPopTexture(target, texture);
  GL.glGetTexImage(target, level, format, type, pixels);
}

void APIENTRY _glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat *params)
{
  PushPopTexture(target, texture);
  GL.glGetTexParameterfv(target, pname, params);
}

void APIENTRY _glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params)
{
  PushPopTexture(target, texture);
  GL.glGetTexParameteriv(target, pname, params);
}

void APIENTRY _glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params)
{
  PushPopTexture(target, texture);
  GL.glGetTexParameterIiv(target, pname, params);
}

void APIENTRY _glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                            GLuint *params)
{
  PushPopTexture(target, texture);
  GL.glGetTexParameterIuiv(target, pname, params);
}

void APIENTRY _glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level,
                                               GLenum pname, GLfloat *params)
{
  PushPopTexture(target, texture);
  GL.glGetTexLevelParameterfv(target, level, pname, params);
}

void APIENTRY _glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level,
                                               GLenum pname, GLint *params)
{
  PushPopTexture(target, texture);
  GL.glGetTexLevelParameteriv(target, level, pname, params);
}

void APIENTRY _glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLint border, GLenum format, GLenum type,
                                   const void *pixels)
{
  PushPopTexture(target, texture);
  GL.glTexImage1D(target, level, internalformat, width, border, format, type, pixels);
}

void APIENTRY _glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLsizei height, GLint border, GLenum format,
                                   GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  GL.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void APIENTRY _glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                   GLenum format, GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  GL.glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
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
     GL.glTexStorage2DMultisample)
  {
    GL.glTexStorage2DMultisample(target, samples, internalformat, width, height,
                                 fixedsamplelocations);
  }
  else
  {
    GL.glTexImage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
  }
}

void APIENTRY _glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                GLenum internalformat, GLsizei width, GLsizei height,
                                                GLsizei depth, GLboolean fixedsamplelocations)
{
  PushPopTexture(target, texture);
  if(((IsGLES && HasExt[OES_texture_storage_multisample_2d_array]) ||
      (!IsGLES && HasExt[ARB_texture_storage] && HasExt[ARB_texture_storage_multisample])) &&
     GL.glTexStorage3DMultisample)
  {
    GL.glTexStorage3DMultisample(target, samples, internalformat, width, height, depth,
                                 fixedsamplelocations);
  }
  else
  {
    GL.glTexImage3DMultisample(target, samples, internalformat, width, height, depth,
                               fixedsamplelocations);
  }
}

void APIENTRY _glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
  PushPopTexture(target, texture);
  GL.glTexParameterf(target, pname, param);
}

void APIENTRY _glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                       const GLfloat *params)
{
  PushPopTexture(target, texture);
  GL.glTexParameterfv(target, pname, params);
}

void APIENTRY _glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
  PushPopTexture(target, texture);
  GL.glTexParameteri(target, pname, param);
}

void APIENTRY _glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                       const GLint *params)
{
  PushPopTexture(target, texture);
  GL.glTexParameteriv(target, pname, params);
}

void APIENTRY _glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                        const GLint *params)
{
  PushPopTexture(target, texture);
  GL.glTexParameterIiv(target, pname, params);
}

void APIENTRY _glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                         const GLuint *params)
{
  PushPopTexture(target, texture);
  GL.glTexParameterIuiv(target, pname, params);
}

void APIENTRY _glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLsizei width, GLenum format, GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  GL.glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

void APIENTRY _glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                      GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  GL.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void APIENTRY _glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                      GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
  PushPopTexture(target, texture);
  GL.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type,
                     pixels);
}

void APIENTRY _glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,
                                     GLenum internalformat, GLsizei width)
{
  PushPopTexture(target, texture);
  GL.glTexStorage1D(target, levels, internalformat, width);
}

void APIENTRY _glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,
                                     GLenum internalformat, GLsizei width, GLsizei height)
{
  PushPopTexture(target, texture);
  GL.glTexStorage2D(target, levels, internalformat, width, height);
}

void APIENTRY _glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,
                                     GLenum internalformat, GLsizei width, GLsizei height,
                                     GLsizei depth)
{
  PushPopTexture(target, texture);
  GL.glTexStorage3D(target, levels, internalformat, width, height, depth);
}

void APIENTRY _glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                       GLenum internalformat, GLint x, GLint y, GLsizei width,
                                       GLint border)
{
  PushPopTexture(target, texture);
  GL.glCopyTexImage1D(target, level, internalformat, x, y, width, border);
}

void APIENTRY _glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                       GLenum internalformat, GLint x, GLint y, GLsizei width,
                                       GLsizei height, GLint border)
{
  PushPopTexture(target, texture);
  GL.glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void APIENTRY _glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                          GLint x, GLint y, GLsizei width)
{
  PushPopTexture(target, texture);
  GL.glCopyTexSubImage1D(target, level, xoffset, x, y, width);
}

void APIENTRY _glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                          GLint yoffset, GLint x, GLint y, GLsizei width,
                                          GLsizei height)
{
  PushPopTexture(target, texture);
  GL.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void APIENTRY _glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                          GLint yoffset, GLint zoffset, GLint x, GLint y,
                                          GLsizei width, GLsizei height)
{
  PushPopTexture(target, texture);
  GL.glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY _glGenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
  PushPopTexture(target, texture);
  GL.glGenerateMipmap(target);
}

void APIENTRY _glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer)
{
  PushPopTexture(target, texture);
  GL.glTexBuffer(target, internalformat, buffer);
}

void APIENTRY _glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat,
                                       GLuint buffer, GLintptr offset, GLsizeiptr size)
{
  PushPopTexture(target, texture);
  GL.glTexBufferRange(target, internalformat, buffer, offset, size);
}

#pragma endregion

#pragma region Vertex Arrays

void APIENTRY _glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint *param)
{
  PushPopVertexArray(vaobj);
  GL.glGetIntegerv(pname, param);
}

void APIENTRY _glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
  PushPopVertexArray(vaobj);
  GL.glGetIntegeri_v(pname, index, param);
}

void APIENTRY _glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                  GLint size, GLenum type, GLboolean normalized,
                                                  GLsizei stride, GLintptr offset)
{
  PushPopVertexArray(vaobj);
  PushPopBuffer(eGL_ARRAY_BUFFER, buffer);
  GL.glVertexAttribPointer(index, size, type, normalized, stride, (const void *)offset);
}

void APIENTRY _glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index,
                                                   GLint size, GLenum type, GLsizei stride,
                                                   GLintptr offset)
{
  PushPopVertexArray(vaobj);
  PushPopBuffer(eGL_ARRAY_BUFFER, buffer);
  GL.glVertexAttribIPointer(index, size, type, stride, (const void *)offset);
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
  GL.glVertexAttribLPointer(index, size, type, stride, (const void *)offset);
}

void APIENTRY _glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  PushPopVertexArray(vaobj);
  GL.glEnableVertexAttribArray(index);
}

void APIENTRY _glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index)
{
  PushPopVertexArray(vaobj);
  GL.glDisableVertexAttribArray(index);
}

void APIENTRY _glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer,
                                                GLintptr offset, GLsizei stride)
{
  PushPopVertexArray(vaobj);
  GL.glBindVertexBuffer(bindingindex, buffer, offset, stride);
}

void APIENTRY _glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                  GLenum type, GLboolean normalized,
                                                  GLuint relativeoffset)
{
  PushPopVertexArray(vaobj);
  GL.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

void APIENTRY _glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                   GLenum type, GLuint relativeoffset)
{
  PushPopVertexArray(vaobj);
  GL.glVertexAttribIFormat(attribindex, size, type, relativeoffset);
}

void APIENTRY _glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size,
                                                   GLenum type, GLuint relativeoffset)
{
  PushPopVertexArray(vaobj);
  GL.glVertexAttribLFormat(attribindex, size, type, relativeoffset);
}

void APIENTRY _glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex,
                                                   GLuint bindingindex)
{
  PushPopVertexArray(vaobj);
  GL.glVertexAttribBinding(attribindex, bindingindex);
}

void APIENTRY _glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
  PushPopVertexArray(vaobj);
  GL.glVertexBindingDivisor(bindingindex, divisor);
}

void APIENTRY _glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor)
{
  PushPopVertexArray(vaobj);
  GL.glVertexAttribDivisor(index, divisor);
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
    {eGL_RGB16, eGL_UNSIGNED_NORMALIZED, 3, 16, 0, 0},
    {eGL_RGB16_SNORM, eGL_SIGNED_NORMALIZED, 3, 16, 0, 0},
    {eGL_RGBA2, eGL_UNSIGNED_NORMALIZED, 4, 2, 0, 0},
    {eGL_RGBA4, eGL_UNSIGNED_NORMALIZED, 4, 4, 0, 0},
    {eGL_RGBA8, eGL_UNSIGNED_NORMALIZED, 4, 8, 0, 0},
    {eGL_RGBA8_SNORM, eGL_SIGNED_NORMALIZED, 4, 8, 0, 0},
    {eGL_RGBA12, eGL_UNSIGNED_NORMALIZED, 4, 12, 0, 0},
    {eGL_RGBA16, eGL_UNSIGNED_NORMALIZED, 4, 16, 0, 0},
    {eGL_RGBA16_SNORM, eGL_SIGNED_NORMALIZED, 4, 16, 0, 0},
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
    {eGL_ALPHA, eGL_UNSIGNED_BYTE, 1, 8, 0, 0},
    {eGL_LUMINANCE, eGL_UNSIGNED_BYTE, 1, 8, 0, 0},
    {eGL_RG, eGL_UNSIGNED_BYTE, 2, 8, 0, 0},
    {eGL_LUMINANCE_ALPHA, eGL_UNSIGNED_BYTE, 2, 8, 0, 0},
    {eGL_RGB, eGL_UNSIGNED_BYTE, 3, 8, 0, 0},
    {eGL_RGBA, eGL_UNSIGNED_BYTE, 4, 8, 0, 0},
    {eGL_BGRA_EXT, eGL_UNSIGNED_BYTE, 4, 8, 0, 0},
    {eGL_DEPTH_COMPONENT, eGL_NONE, 0, 0, 24, 0},
    {eGL_STENCIL_INDEX, eGL_NONE, 0, 0, 0, 8},
    {eGL_DEPTH_STENCIL, eGL_NONE, 0, 0, 24, 8},

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
           ToStr(internalformat).c_str());
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
      RDCERR("pname %s not supported by internal glGetInternalformativ", ToStr(pname).c_str());
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

  GL.glGenFramebuffers(2, fbos);

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
      GL.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);

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
            GL.glGetCompressedTextureImageEXT(srcName, targets[trg], srcLevel, buf);
          }

          // write to GPU
          if(srcTarget == eGL_TEXTURE_1D || srcTarget == eGL_TEXTURE_1D_ARRAY)
            GL.glCompressedTextureSubImage1DEXT(dstName, targets[trg], dstLevel, 0, srcWidth, fmt,
                                                (GLsizei)size, buf);
          else if(srcTarget == eGL_TEXTURE_3D)
            GL.glCompressedTextureSubImage3DEXT(dstName, targets[trg], dstLevel, 0, 0, 0, srcWidth,
                                                srcHeight, srcDepth, fmt, (GLsizei)size, buf);
          else
            GL.glCompressedTextureSubImage2DEXT(dstName, targets[trg], dstLevel, 0, 0, srcWidth,
                                                srcHeight, fmt, (GLsizei)size, buf);
        }

        delete[] buf;
      }
      else
      {
        ResourceFormat format = MakeResourceFormat(srcTarget, fmt);

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
            GL.glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, attach, srcTarget, srcName, srcLevel);
            GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attach, dstTarget, dstName, dstLevel);
          }
          else
          {
            GL.glFramebufferTexture(eGL_READ_FRAMEBUFFER, attach, srcName, srcLevel);
            GL.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, attach, dstName, dstLevel);
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
      GLenum status = GL.glCheckFramebufferStatus(eGL_DRAW_FRAMEBUFFER);

      if(status != eGL_FRAMEBUFFER_COMPLETE)
        RDCERR("glCopyImageSubData emulation draw FBO is %s", ToStr(status).c_str());

      status = GL.glCheckFramebufferStatus(eGL_READ_FRAMEBUFFER);

      if(status != eGL_FRAMEBUFFER_COMPLETE)
        RDCERR("glCopyImageSubData emulation read FBO is %s", ToStr(status).c_str());

      SafeBlitFramebuffer(srcX, srcY, srcX + srcWidth, srcY + srcHeight, dstX, dstY,
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
        GL.glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, attach, textargets[srcZ + slice], srcName,
                                  srcLevel);
        GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attach, textargets[dstZ + slice], dstName,
                                  dstLevel);

        if(slice == 0)
        {
          GLenum status = GL.glCheckFramebufferStatus(eGL_DRAW_FRAMEBUFFER);

          if(status != eGL_FRAMEBUFFER_COMPLETE)
            RDCERR("glCopyImageSubData emulation draw FBO is %s for slice 0", ToStr(status).c_str());

          status = GL.glCheckFramebufferStatus(eGL_READ_FRAMEBUFFER);

          if(status != eGL_FRAMEBUFFER_COMPLETE)
            RDCERR("glCopyImageSubData emulation read FBO is %s for slice 0", ToStr(status).c_str());
        }

        SafeBlitFramebuffer(srcX, srcY, srcX + srcWidth, srcY + srcHeight, dstX, dstY,
                            dstX + srcWidth, dstY + srcHeight, mask, eGL_NEAREST);
      }
    }
    else
    {
      for(GLsizei slice = 0; slice < srcDepth; slice++)
      {
        GL.glFramebufferTextureLayer(eGL_READ_FRAMEBUFFER, attach, srcName, srcLevel, srcZ + slice);
        GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, dstName, dstLevel, dstZ + slice);

        if(slice == 0)
        {
          GLenum status = GL.glCheckFramebufferStatus(eGL_DRAW_FRAMEBUFFER);

          if(status != eGL_FRAMEBUFFER_COMPLETE)
            RDCERR("glCopyImageSubData emulation draw FBO is %s for slice 0", ToStr(status).c_str());

          status = GL.glCheckFramebufferStatus(eGL_READ_FRAMEBUFFER);

          if(status != eGL_FRAMEBUFFER_COMPLETE)
            RDCERR("glCopyImageSubData emulation read FBO is %s for slice 0", ToStr(status).c_str());
        }

        SafeBlitFramebuffer(srcX, srcY, srcX + srcWidth, srcY + srcHeight, dstX, dstY,
                            dstX + srcWidth, dstY + srcHeight, mask, eGL_NEAREST);
      }
    }
  }

  GL.glDeleteFramebuffers(2, fbos);
}

#pragma endregion

#pragma region ARB_vertex_attrib_binding

struct EmulatedVertexBuffer
{
  bool dirty = false;    // set by anything that changes this struct, used to determine which
                         // bindings/attribs we need to flush

  GLuint divisor = 0;     // set by _glVertexBindingDivisor
  GLuint buffer = 0;      // set by _glBindVertexBuffer
  GLintptr offset = 0;    // set by _glBindVertexBuffer
  GLsizei stride = 0;     // set by _glBindVertexBuffer
};

struct EmulatedVertexAttrib
{
  // start off with a binding index
  EmulatedVertexAttrib(GLuint b) : bindingindex(b) {}
  bool dirty = false;    // set by anything that changes this struct, used to determine which
                         // bindings/attribs we need to flush

  GLboolean Long = GL_FALSE;       // enabled by _glVertexAttribLFormat
  GLboolean Integer = GL_FALSE;    // enabled by _glVertexAttribIFormat

  GLint size = 4;                     // set by _glVertexAttrib?Format
  GLenum type = eGL_FLOAT;            // set by _glVertexAttrib?Format
  GLboolean normalized = GL_FALSE;    // set by _glVertexAttrib?Format
  GLuint relativeoffset = 0;          // set by _glVertexAttrib?Format

  GLuint bindingindex;    // index into vertex buffers, set by _glVertexAttribBinding
};

static const uint32_t NumEmulatedBinds = 16;

struct EmulatedVAO
{
  EmulatedVertexBuffer vbs[NumEmulatedBinds];

  // each attrib starts off pointing at its corresponding binding
  EmulatedVertexAttrib attribs[NumEmulatedBinds] = {
      EmulatedVertexAttrib(0),  EmulatedVertexAttrib(1),  EmulatedVertexAttrib(2),
      EmulatedVertexAttrib(3),  EmulatedVertexAttrib(4),  EmulatedVertexAttrib(5),
      EmulatedVertexAttrib(6),  EmulatedVertexAttrib(7),  EmulatedVertexAttrib(8),
      EmulatedVertexAttrib(9),  EmulatedVertexAttrib(10), EmulatedVertexAttrib(11),
      EmulatedVertexAttrib(12), EmulatedVertexAttrib(13), EmulatedVertexAttrib(14),
      EmulatedVertexAttrib(15),
  };
};

static std::map<GLResource, EmulatedVAO> _emulatedBindings;

static EmulatedVAO &_GetVAOData()
{
  GLuint o = 0;
  glGetIntegerv_real(eGL_VERTEX_ARRAY_BINDING, (GLint *)&o);

  return _emulatedBindings[VertexArrayRes(driver->GetCtx(), o)];
}

static void _ResetVertexAttribBinding()
{
  _emulatedBindings.clear();
}

static void _FlushVertexAttribBinding()
{
  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;
  EmulatedVertexBuffer *vbs = vao.vbs;

  // flush all dirty vertex attribs out using old-style binding.
  for(uint32_t a = 0; a < NumEmulatedBinds; a++)
  {
    EmulatedVertexAttrib &attrib = attribs[a];
    EmulatedVertexBuffer &bind = vbs[attrib.bindingindex];

    // if the attrib and its bind isn't dirty, just continue
    if(!attrib.dirty && !bind.dirty)
      continue;

    // otherwise, flush it out using VertexAttrib?Pointer

    {
      PushPopBuffer(eGL_ARRAY_BUFFER, bind.buffer);

      if(attrib.Long)
        glVertexAttribLPointer_real(a, attrib.size, attrib.type, bind.stride,
                                    (const void *)(bind.offset + attrib.relativeoffset));
      else if(attrib.Integer)
        glVertexAttribIPointer_real(a, attrib.size, attrib.type, bind.stride,
                                    (const void *)(bind.offset + attrib.relativeoffset));
      else
        glVertexAttribPointer_real(a, attrib.size, attrib.type, attrib.normalized, bind.stride,
                                   (const void *)(bind.offset + attrib.relativeoffset));

      if(glVertexAttribDivisor_real)
        glVertexAttribDivisor_real(a, bind.divisor);
    }

    // attrib is no longer dirty
    attrib.dirty = false;

    // we can't un-dirty the VB because multiple attribs may point to them and we need to update
    // them all
  }

  // unset VB dirty flags now
  for(uint32_t vb = 0; vb < NumEmulatedBinds; vb++)
    vbs[vb].dirty = false;
}

void APIENTRY _glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type,
                                    GLboolean normalized, GLuint relativeoffset)
{
  if(attribindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribFormat", attribindex);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;

  attribs[attribindex].Long = GL_FALSE;
  attribs[attribindex].Integer = GL_FALSE;
  attribs[attribindex].size = size;
  attribs[attribindex].type = type;
  attribs[attribindex].normalized = normalized;
  attribs[attribindex].relativeoffset = relativeoffset;
  attribs[attribindex].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type,
                                     GLuint relativeoffset)
{
  if(attribindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribIFormat", attribindex);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;

  attribs[attribindex].Long = GL_FALSE;
  attribs[attribindex].Integer = GL_TRUE;
  attribs[attribindex].size = size;
  attribs[attribindex].type = type;
  attribs[attribindex].relativeoffset = relativeoffset;
  attribs[attribindex].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type,
                                     GLuint relativeoffset)
{
  if(attribindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribLFormat", attribindex);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;

  attribs[attribindex].Long = GL_TRUE;
  attribs[attribindex].Integer = GL_FALSE;
  attribs[attribindex].size = size;
  attribs[attribindex].type = type;
  attribs[attribindex].relativeoffset = relativeoffset;
  attribs[attribindex].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
  if(attribindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribBinding", attribindex);
    return;
  }

  if(bindingindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled binding %u in glVertexAttribBinding", bindingindex);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;

  attribs[attribindex].bindingindex = bindingindex;
  attribs[attribindex].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
  if(bindingindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled binding %u in glVertexBindingDivisor", bindingindex);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexBuffer *vbs = vao.vbs;

  vbs[bindingindex].divisor = divisor;
  vbs[bindingindex].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
  if(bindingindex >= NumEmulatedBinds)
  {
    RDCERR("Unhandled binding %u in glBindVertexBuffer", bindingindex);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexBuffer *vbs = vao.vbs;

  vbs[bindingindex].buffer = buffer;
  vbs[bindingindex].offset = offset;
  vbs[bindingindex].stride = stride;
  vbs[bindingindex].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glGetIntegerv(GLenum pname, GLint *params)
{
  switch(pname)
  {
    case eGL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET: *params = ~0U; return;
    case eGL_MAX_VERTEX_ATTRIB_BINDINGS: *params = NumEmulatedBinds; return;
    default: break;
  }

  return glGetIntegerv_real(pname, params);
}

void APIENTRY _glGetIntegeri_v(GLenum pname, GLuint index, GLint *params)
{
  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexBuffer *vbs = vao.vbs;

  switch(pname)
  {
    case eGL_VERTEX_BINDING_OFFSET: *params = (GLint)vbs[index].offset; return;
    case eGL_VERTEX_BINDING_STRIDE: *params = (GLint)vbs[index].stride; return;
    case eGL_VERTEX_BINDING_DIVISOR: *params = (GLint)vbs[index].divisor; return;
    case eGL_VERTEX_BINDING_BUFFER: *params = vbs[index].buffer; return;
    default: break;
  }

  return glGetIntegeri_v_real(pname, index, params);
}

void APIENTRY _glGetInteger64i_v(GLenum pname, GLuint index, GLint64 *params)
{
  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexBuffer *vbs = vao.vbs;

  switch(pname)
  {
    case eGL_VERTEX_BINDING_OFFSET: *params = (GLint64)vbs[index].offset; return;
    case eGL_VERTEX_BINDING_STRIDE: *params = (GLint64)vbs[index].stride; return;
    case eGL_VERTEX_BINDING_DIVISOR: *params = (GLint64)vbs[index].divisor; return;
    case eGL_VERTEX_BINDING_BUFFER: *params = vbs[index].buffer; return;
    default: break;
  }

  return glGetInteger64i_v_real(pname, index, params);
}

void APIENTRY _glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;
  EmulatedVertexBuffer *vbs = vao.vbs;

  if(index >= NumEmulatedBinds)
  {
    RDCERR("Invalid index to glGetVertexAttribiv: %u", index);
    return;
  }

  switch(pname)
  {
    case GL_VERTEX_ATTRIB_BINDING: *params = (GLint)attribs[index].bindingindex; return;
    case GL_VERTEX_ATTRIB_RELATIVE_OFFSET: *params = attribs[index].relativeoffset; return;

    case GL_VERTEX_ATTRIB_ARRAY_SIZE: *params = attribs[index].size; return;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: *params = attribs[index].type; return;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: *params = attribs[index].normalized; return;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER: *params = attribs[index].Integer; return;
    case GL_VERTEX_ATTRIB_ARRAY_LONG: *params = attribs[index].Long; return;

    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      *params = vbs[attribs[index].bindingindex].buffer;
      return;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *params = (GLint)vbs[attribs[index].bindingindex].stride;
      return;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      *params = (GLint)vbs[attribs[index].bindingindex].divisor;
      return;
    default: break;
  }

  return glGetVertexAttribiv_real(index, pname, params);
}

void APIENTRY _glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                     GLsizei stride, const void *pointer)
{
  if(index >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribPointer", index);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;
  EmulatedVertexBuffer *vbs = vao.vbs;

  attribs[index].Long = GL_FALSE;
  attribs[index].Integer = GL_FALSE;
  attribs[index].size = size;
  attribs[index].type = type;
  attribs[index].normalized = normalized;
  // spec says that the offset goes entirely into the binding offset, not relative offset
  attribs[index].relativeoffset = 0;
  // spec says that the old functions stomp the binding with the 1:1 index
  attribs[index].bindingindex = index;
  attribs[index].dirty = true;

  // whatever is currently in ARRAY_BUFFER gets bound to the VB
  glGetIntegerv_real(eGL_ARRAY_BUFFER_BINDING, (GLint *)&vbs[index].buffer);
  vbs[index].stride = stride;
  vbs[index].offset = (GLintptr)pointer;
  vbs[index].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                      const void *pointer)
{
  if(index >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribPointer", index);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;
  EmulatedVertexBuffer *vbs = vao.vbs;

  attribs[index].Long = GL_FALSE;
  attribs[index].Integer = GL_TRUE;
  attribs[index].size = size;
  attribs[index].type = type;
  // spec says that the offset goes entirely into the binding offset, not relative offset
  attribs[index].relativeoffset = 0;
  // spec says that the old functions stomp the binding with the 1:1 index
  attribs[index].bindingindex = index;
  attribs[index].dirty = true;

  // whatever is currently in ARRAY_BUFFER gets bound to the VB
  glGetIntegerv_real(eGL_ARRAY_BUFFER_BINDING, (GLint *)&vbs[index].buffer);
  vbs[index].stride = stride;
  vbs[index].offset = (GLintptr)pointer;
  vbs[index].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                      const void *pointer)
{
  if(index >= NumEmulatedBinds)
  {
    RDCERR("Unhandled attrib %u in glVertexAttribPointer", index);
    return;
  }

  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;
  EmulatedVertexBuffer *vbs = vao.vbs;

  attribs[index].Long = GL_TRUE;
  attribs[index].Integer = GL_FALSE;
  attribs[index].size = size;
  attribs[index].type = type;
  // spec says that the offset goes entirely into the binding offset, not relative offset
  attribs[index].relativeoffset = 0;
  // spec says that the old functions stomp the binding with the 1:1 index
  attribs[index].bindingindex = index;
  attribs[index].dirty = true;

  // whatever is currently in ARRAY_BUFFER gets bound to the VB
  glGetIntegerv_real(eGL_ARRAY_BUFFER_BINDING, (GLint *)&vbs[index].buffer);
  vbs[index].stride = stride;
  vbs[index].offset = (GLintptr)pointer;
  vbs[index].dirty = true;

  _FlushVertexAttribBinding();
}

void APIENTRY _glVertexAttribDivisor(GLuint index, GLuint divisor)
{
  EmulatedVAO &vao = _GetVAOData();
  EmulatedVertexAttrib *attribs = vao.attribs;
  EmulatedVertexBuffer *vbs = vao.vbs;

  // spec says that this function stomps the binding with the 1:1 index, the same as
  // glVertexAttrib?Pointer
  attribs[index].bindingindex = index;
  attribs[index].dirty = true;
  vbs[index].divisor = divisor;
  vbs[index].dirty = true;

  _FlushVertexAttribBinding();
}

#pragma endregion

#pragma region ARB_clear_buffer_object

void APIENTRY _glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset,
                                    GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
  byte *dst = (byte *)GL.glMapBufferRange(target, offset, size,
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
               ToStr(format).c_str());
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
               ToStr(type).c_str());
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

    ResourceFormat fmt = MakeResourceFormat(eGL_TEXTURE_2D, internalformat);

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

  GL.glUnmapBuffer(target);
}

void APIENTRY _glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type,
                                 const void *data)
{
  GLint size = 0;
  GL.glGetBufferParameteriv(target, eGL_BUFFER_SIZE, &size);

  _glClearBufferSubData(target, internalformat, 0, (GLsizeiptr)size, format, type, data);
}

#pragma endregion

#pragma region ARB_separate_shader_objects

void APIENTRY _glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
  // if we're emulating, programs were never separable
  if(pname == eGL_PROGRAM_SEPARABLE)
  {
    *params = 0;
    return;
  }

  glGetProgramiv_real(program, pname, params);
}

void APIENTRY _glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
  // we only set this when reflecting, so just silently drop it
  if(pname == eGL_PROGRAM_SEPARABLE)
    return;

  RDCERR("Cannot emulate glProgramParameteri(%s), capture cannot be opened", ToStr(pname).c_str());
}

void APIENTRY _glProgramUniform1i(GLuint program, GLint location, GLint x)
{
  PushPopProgram(program);
  GL.glUniform1i(location, x);
}

void APIENTRY _glProgramUniform2i(GLuint program, GLint location, GLint x, GLint y)
{
  PushPopProgram(program);
  GL.glUniform2i(location, x, y);
}

void APIENTRY _glProgramUniform3i(GLuint program, GLint location, GLint x, GLint y, GLint z)
{
  PushPopProgram(program);
  GL.glUniform3i(location, x, y, z);
}

void APIENTRY _glProgramUniform4i(GLuint program, GLint location, GLint x, GLint y, GLint z, GLint w)
{
  PushPopProgram(program);
  GL.glUniform4i(location, x, y, z, w);
}

void APIENTRY _glProgramUniform1ui(GLuint program, GLint location, GLuint x)
{
  PushPopProgram(program);
  GL.glUniform1ui(location, x);
}

void APIENTRY _glProgramUniform2ui(GLuint program, GLint location, GLuint x, GLuint y)
{
  PushPopProgram(program);
  GL.glUniform2ui(location, x, y);
}

void APIENTRY _glProgramUniform3ui(GLuint program, GLint location, GLuint x, GLuint y, GLuint z)
{
  PushPopProgram(program);
  GL.glUniform3ui(location, x, y, z);
}

void APIENTRY _glProgramUniform4ui(GLuint program, GLint location, GLuint x, GLuint y, GLuint z,
                                   GLuint w)
{
  PushPopProgram(program);
  GL.glUniform4ui(location, x, y, z, w);
}

void APIENTRY _glProgramUniform1f(GLuint program, GLint location, GLfloat x)
{
  PushPopProgram(program);
  GL.glUniform1f(location, x);
}

void APIENTRY _glProgramUniform2f(GLuint program, GLint location, GLfloat x, GLfloat y)
{
  PushPopProgram(program);
  GL.glUniform2f(location, x, y);
}

void APIENTRY _glProgramUniform3f(GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z)
{
  PushPopProgram(program);
  GL.glUniform3f(location, x, y, z);
}

void APIENTRY _glProgramUniform4f(GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z,
                                  GLfloat w)
{
  PushPopProgram(program);
  GL.glUniform4f(location, x, y, z, w);
}

void APIENTRY _glProgramUniform1d(GLuint program, GLint location, GLdouble x)
{
  PushPopProgram(program);
  GL.glUniform1d(location, x);
}

void APIENTRY _glProgramUniform2d(GLuint program, GLint location, GLdouble x, GLdouble y)
{
  PushPopProgram(program);
  GL.glUniform2d(location, x, y);
}

void APIENTRY _glProgramUniform3d(GLuint program, GLint location, GLdouble x, GLdouble y, GLdouble z)
{
  PushPopProgram(program);
  GL.glUniform3d(location, x, y, z);
}

void APIENTRY _glProgramUniform4d(GLuint program, GLint location, GLdouble x, GLdouble y,
                                  GLdouble z, GLdouble w)
{
  PushPopProgram(program);
  GL.glUniform4d(location, x, y, z, w);
}

void APIENTRY _glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
  PushPopProgram(program);
  GL.glUniform1iv(location, count, value);
}

void APIENTRY _glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
  PushPopProgram(program);
  GL.glUniform2iv(location, count, value);
}

void APIENTRY _glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
  PushPopProgram(program);
  GL.glUniform3iv(location, count, value);
}

void APIENTRY _glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
  PushPopProgram(program);
  GL.glUniform4iv(location, count, value);
}

void APIENTRY _glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
  PushPopProgram(program);
  GL.glUniform1uiv(location, count, value);
}

void APIENTRY _glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
  PushPopProgram(program);
  GL.glUniform2uiv(location, count, value);
}

void APIENTRY _glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
  PushPopProgram(program);
  GL.glUniform3uiv(location, count, value);
}

void APIENTRY _glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
  PushPopProgram(program);
  GL.glUniform4uiv(location, count, value);
}

void APIENTRY _glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniform1fv(location, count, value);
}

void APIENTRY _glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniform2fv(location, count, value);
}

void APIENTRY _glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniform3fv(location, count, value);
}

void APIENTRY _glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniform4fv(location, count, value);
}

void APIENTRY _glProgramUniform1dv(GLuint program, GLint location, GLsizei count,
                                   const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniform1dv(location, count, value);
}

void APIENTRY _glProgramUniform2dv(GLuint program, GLint location, GLsizei count,
                                   const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniform2dv(location, count, value);
}

void APIENTRY _glProgramUniform3dv(GLuint program, GLint location, GLsizei count,
                                   const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniform3dv(location, count, value);
}

void APIENTRY _glProgramUniform4dv(GLuint program, GLint location, GLsizei count,
                                   const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniform4dv(location, count, value);
}

void APIENTRY _glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count,
                                         GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix2fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count,
                                         GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix3fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count,
                                         GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count,
                                         GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix2dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count,
                                         GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix3dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count,
                                         GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix2x3fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4x2fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix2x4fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4x2fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix3x4fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLfloat *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4x3fv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix2x3dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4x2dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix2x4dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4x2dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix3x4dv(location, count, transpose, value);
}

void APIENTRY _glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count,
                                           GLboolean transpose, const GLdouble *value)
{
  PushPopProgram(program);
  GL.glUniformMatrix4x3dv(location, count, transpose, value);
}

void APIENTRY _glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

void APIENTRY _glActiveShaderProgram(GLuint pipeline, GLuint program)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

GLuint APIENTRY _glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
  return 0;
}

void APIENTRY _glBindProgramPipeline(GLuint pipeline)
{
  // we can ignore binds of 0
  if(pipeline == 0)
    return;

  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

void APIENTRY _glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

void APIENTRY _glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

GLboolean APIENTRY _glIsProgramPipeline(GLuint pipeline)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
  return GL_FALSE;
}

void APIENTRY _glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

void APIENTRY _glValidateProgramPipeline(GLuint pipeline)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

void APIENTRY _glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length,
                                           GLchar *infoLog)
{
  RDCERR(
      "Emulation of ARB_separate_shader_objects can't actually create pipelines. "
      "Capture cannot be opened.");
}

#pragma endregion

#pragma region ARB_program_interface_query

static ReflectionInterface ConvertInterface(GLenum programInterface)
{
  ReflectionInterface ret = ReflectionInterface::Uniform;

  switch(programInterface)
  {
    case eGL_UNIFORM: ret = ReflectionInterface::Uniform; break;
    case eGL_UNIFORM_BLOCK: ret = ReflectionInterface::UniformBlock; break;
    case eGL_PROGRAM_INPUT: ret = ReflectionInterface::Input; break;
    case eGL_PROGRAM_OUTPUT: ret = ReflectionInterface::Output; break;
    case eGL_SHADER_STORAGE_BLOCK: ret = ReflectionInterface::ShaderStorageBlock; break;
    case eGL_ATOMIC_COUNTER_BUFFER: ret = ReflectionInterface::AtomicCounterBuffer; break;
    case eGL_BUFFER_VARIABLE: ret = ReflectionInterface::BufferVariable; break;
    default:
      RDCERR("Unexpected/unsupported program interface being queried: %s",
             ToStr(programInterface).c_str());
      break;
  }

  return ret;
}

static ReflectionProperty ConvertProperty(GLenum prop)
{
  ReflectionProperty ret = ReflectionProperty::ActiveResources;

  switch(prop)
  {
    // internal query used for testing
    case eGL_UNIFORM:
    case eGL_UNIFORM_BLOCK_BINDING: ret = ReflectionProperty::Internal_Binding; break;

    case eGL_ACTIVE_RESOURCES: ret = ReflectionProperty::ActiveResources; break;
    case eGL_BUFFER_BINDING: ret = ReflectionProperty::BufferBinding; break;
    case eGL_TOP_LEVEL_ARRAY_STRIDE: ret = ReflectionProperty::TopLevelArrayStride; break;
    case eGL_BLOCK_INDEX: ret = ReflectionProperty::BlockIndex; break;
    case eGL_ARRAY_SIZE: ret = ReflectionProperty::ArraySize; break;
    case eGL_IS_ROW_MAJOR: ret = ReflectionProperty::IsRowMajor; break;
    case eGL_NUM_ACTIVE_VARIABLES: ret = ReflectionProperty::NumActiveVariables; break;
    case eGL_BUFFER_DATA_SIZE: ret = ReflectionProperty::BufferDataSize; break;
    case eGL_NAME_LENGTH: ret = ReflectionProperty::NameLength; break;
    case eGL_TYPE: ret = ReflectionProperty::Type; break;
    case eGL_LOCATION_COMPONENT: ret = ReflectionProperty::LocationComponent; break;
    case eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER:
    case eGL_REFERENCED_BY_VERTEX_SHADER: ret = ReflectionProperty::ReferencedByVertexShader; break;
    case eGL_REFERENCED_BY_TESS_CONTROL_SHADER:
      ret = ReflectionProperty::ReferencedByTessControlShader;
      break;
    case eGL_REFERENCED_BY_TESS_EVALUATION_SHADER:
      ret = ReflectionProperty::ReferencedByTessEvaluationShader;
      break;
    case eGL_REFERENCED_BY_GEOMETRY_SHADER:
      ret = ReflectionProperty::ReferencedByGeometryShader;
      break;
    case eGL_REFERENCED_BY_FRAGMENT_SHADER:
      ret = ReflectionProperty::ReferencedByFragmentShader;
      break;
    case eGL_REFERENCED_BY_COMPUTE_SHADER:
      ret = ReflectionProperty::ReferencedByComputeShader;
      break;
    case eGL_ATOMIC_COUNTER_BUFFER_INDEX: ret = ReflectionProperty::AtomicCounterBufferIndex; break;
    case eGL_OFFSET: ret = ReflectionProperty::Offset; break;
    case eGL_ARRAY_STRIDE: ret = ReflectionProperty::ArrayStride; break;
    case eGL_MATRIX_STRIDE: ret = ReflectionProperty::MatrixStride; break;
    case eGL_LOCATION: ret = ReflectionProperty::Location; break;
    default: RDCERR("Unexpected program property being queried: %s", ToStr(prop).c_str()); break;
  }

  return ret;
}

static glslang::TProgram *GetGlslangProgram(GLuint program, bool *hasRealProgram = NULL)
{
  if(driver == NULL)
  {
    RDCERR("No driver available, can't emulate ARB_program_interface_query");
    return NULL;
  }

  ResourceId id = driver->GetResourceManager()->GetID(ProgramRes(driver->GetCtx(), program));

  if(!driver->m_Programs[id].glslangProgram)
  {
    RDCERR("Don't have glslang program for reflecting program %u = %s", program, ToStr(id).c_str());
  }

  if(hasRealProgram)
    *hasRealProgram = !driver->m_Programs[id].shaders.empty();

  return driver->m_Programs[id].glslangProgram;
}

void APIENTRY _glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname,
                                       GLint *params)
{
  glslang::TProgram *glslangProgram = GetGlslangProgram(program);

  if(!glslangProgram)
  {
    *params = 0;
    return;
  }

  glslangGetProgramInterfaceiv(glslangProgram, ConvertInterface(programInterface),
                               ConvertProperty(pname), params);
}

void APIENTRY _glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index,
                                      GLsizei propCount, const GLenum *props, GLsizei bufSize,
                                      GLsizei *length, GLint *params)
{
  bool hasRealProgram = true;
  glslang::TProgram *glslangProgram = GetGlslangProgram(program, &hasRealProgram);

  if(!glslangProgram)
  {
    if(length)
      *length = 0;
    if(params)
      memset(params, 0, sizeof(GLint) * bufSize);
    return;
  }

  std::vector<ReflectionProperty> properties(propCount);

  for(GLsizei i = 0; i < propCount; i++)
    properties[i] = ConvertProperty(props[i]);

  glslangGetProgramResourceiv(glslangProgram, ConvertInterface(programInterface), index, properties,
                              bufSize, length, params);

  // fetch locations by hand from the driver
  for(GLsizei i = 0; i < propCount; i++)
  {
    if(props[i] == eGL_LOCATION)
    {
      if(programInterface == eGL_UNIFORM && params[i] >= 0)
      {
        const char *name =
            glslangGetProgramResourceName(glslangProgram, ConvertInterface(programInterface), index);

        if(GL.glGetUniformLocation && hasRealProgram)
          params[i] = GL.glGetUniformLocation(program, name);
      }
      else if(programInterface == eGL_PROGRAM_INPUT && params[i] < 0)
      {
        const char *name =
            glslangGetProgramResourceName(glslangProgram, ConvertInterface(programInterface), index);

        if(GL.glGetAttribLocation && hasRealProgram)
          params[i] = GL.glGetAttribLocation(program, name);
      }
      else if(programInterface == eGL_PROGRAM_OUTPUT && params[i] < 0)
      {
        const char *name =
            glslangGetProgramResourceName(glslangProgram, ConvertInterface(programInterface), index);

        if(GL.glGetFragDataLocation && hasRealProgram)
          params[i] = GL.glGetFragDataLocation(program, name);
      }
    }
    else if(props[i] == eGL_BUFFER_BINDING)
    {
      if(programInterface == eGL_UNIFORM_BLOCK)
      {
        const char *name =
            glslangGetProgramResourceName(glslangProgram, ConvertInterface(programInterface), index);

        if(GL.glGetUniformBlockIndex && hasRealProgram)
        {
          GLuint blockIndex = GL.glGetUniformBlockIndex(program, name);
          if(blockIndex != GL_INVALID_INDEX && GL.glGetActiveUniformBlockiv)
          {
            GL.glGetActiveUniformBlockiv(program, blockIndex, eGL_UNIFORM_BLOCK_BINDING, &params[i]);
          }
        }
      }
    }
  }
}

GLuint APIENTRY _glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name)
{
  glslang::TProgram *glslangProgram = GetGlslangProgram(program);

  if(!glslangProgram)
  {
    return 0;
  }

  return glslangGetProgramResourceIndex(glslangProgram, name);
}

void APIENTRY _glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index,
                                        GLsizei bufSize, GLsizei *length, GLchar *name)
{
  glslang::TProgram *glslangProgram = GetGlslangProgram(program);

  if(!glslangProgram)
  {
    if(length)
      *length = 0;
    if(name && bufSize)
      memset(name, 0, bufSize);
    return;
  }

  const char *fetchedName =
      glslangGetProgramResourceName(glslangProgram, ConvertInterface(programInterface), index);

  if(fetchedName)
  {
    size_t nameLen = strlen(fetchedName);
    if(length)
      *length = (int32_t)nameLen;

    memcpy(name, fetchedName, RDCMIN((size_t)bufSize, nameLen));
    name[bufSize - 1] = 0;
    if(nameLen < (size_t)bufSize)
      name[nameLen] = 0;
  }
  else
  {
    if(length)
      *length = 0;
    if(name && bufSize)
      memset(name, 0, bufSize);
  }
}

#pragma endregion

#pragma region GLES Compatibility

void APIENTRY _glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
  if(driver == NULL)
  {
    RDCERR("No driver available, can't emulate glGetTexLevelParameteriv");
    return;
  }

  GLint boundTexture = 0;
  GL.glGetIntegerv(TextureBinding(target), (GLint *)&boundTexture);

  ResourceId id = driver->GetResourceManager()->GetID(TextureRes(driver->GetCtx(), boundTexture));

  WrappedOpenGL::TextureData &details = driver->m_Textures[id];

  bool hasmip = (details.mipsValid & (1 << level)) != 0;

  if(details.mipsValid == 0)
    RDCWARN("No mips valid! Uninitialised texture?");

  switch(pname)
  {
    case eGL_TEXTURE_WIDTH: *params = hasmip ? RDCMAX(1, details.width >> level) : 0; return;
    case eGL_TEXTURE_HEIGHT: *params = hasmip ? RDCMAX(1, details.height >> level) : 0; return;
    case eGL_TEXTURE_DEPTH: *params = hasmip ? RDCMAX(1, details.depth >> level) : 0; return;
    case eGL_TEXTURE_INTERNAL_FORMAT: *params = details.internalFormat; return;
    default:
      // since this is internal emulation, we only handle the parameters we expect to need
      break;
  }

  RDCERR("Unhandled parameter %s", ToStr(pname).c_str());
}

void APIENTRY _glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
  // just call and upcast
  GLint param = 0;
  _glGetTexLevelParameteriv(target, level, pname, &param);
  *params = (GLfloat)param;
}

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
  GL.glGetTexLevelParameteriv(target, level, eGL_TEXTURE_WIDTH, &width);
  GL.glGetTexLevelParameteriv(target, level, eGL_TEXTURE_HEIGHT, &height);
  GL.glGetTexLevelParameteriv(target, level, eGL_TEXTURE_DEPTH, &depth);

  GLint boundTexture = 0;
  GL.glGetIntegerv(TextureBinding(target), (GLint *)&boundTexture);

  GLenum attachment = eGL_COLOR_ATTACHMENT0;
  if(format == eGL_DEPTH_COMPONENT)
    attachment = eGL_DEPTH_ATTACHMENT;
  else if(format == eGL_STENCIL)
    attachment = eGL_STENCIL_ATTACHMENT;
  else if(format == eGL_DEPTH_STENCIL)
    attachment = eGL_DEPTH_STENCIL_ATTACHMENT;

  size_t sliceSize = GetByteSize(width, height, 1, format, type);

  bool fixBGRA = false;
  if(!HasExt[EXT_read_format_bgra] && format == eGL_BGRA)
  {
    if(type == eGL_UNSIGNED_BYTE)
      fixBGRA = true;
    else
      RDCERR("Can't read back texture without EXT_read_format_bgra extension (data type: %s)",
             ToStr(type).c_str());
  }

  GLuint readtex = boundTexture;
  GLuint deltex = 0;

  GLuint fbo = 0;
  GL.glGenFramebuffers(1, &fbo);

  PushPopFramebuffer(eGL_FRAMEBUFFER, fbo);

  // ALPHA can't be bound to an FBO, so we need to blit to R8 by hand.
  if(format == eGL_LUMINANCE_ALPHA || format == eGL_LUMINANCE || format == eGL_ALPHA)
  {
    RDCDEBUG("Doing manual blit from %s to allow readback", ToStr(format).c_str());

    GLenum remapformat = eGL_RED;
    if(format == eGL_LUMINANCE_ALPHA)
      remapformat = eGL_RG;

    GLint baseLevel = 0;
    GLint maxLevel = 0;

    // sample from only the right level of the source texture
    GL.glGetTexParameteriv(target, eGL_TEXTURE_BASE_LEVEL, &baseLevel);
    GL.glGetTexParameteriv(target, eGL_TEXTURE_MAX_LEVEL, &maxLevel);
    GL.glTexParameteri(target, eGL_TEXTURE_BASE_LEVEL, level);
    GL.glTexParameteri(target, eGL_TEXTURE_MAX_LEVEL, level);

    // only support 2D textures for now
    RDCASSERT(target == eGL_TEXTURE_2D, target);
    GL.glGenTextures(1, &readtex);
    GL.glBindTexture(target, readtex);

    // allocate the R8 texture
    GL.glTexParameteri(target, eGL_TEXTURE_MAX_LEVEL, 0);
    GL.glTexImage2D(target, 0, eGL_R8, width, height, 0, remapformat, eGL_UNSIGNED_BYTE, NULL);

    // render to it
    GL.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, target, readtex, 0);

    // push rendering state
    GLPushPopState textState;

    textState.Push(true);

    // render a blit triangle to move alpha in the source to red in the dest
    GL.glDisable(eGL_BLEND);
    GL.glDisable(eGL_DEPTH_TEST);
    GL.glDisable(eGL_STENCIL_TEST);
    GL.glDisable(eGL_CULL_FACE);
    GL.glDisable(eGL_SCISSOR_TEST);
    GL.glViewport(0, 0, width, height);

    GL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    GL.glActiveTexture(eGL_TEXTURE0);
    GL.glBindTexture(eGL_TEXTURE_2D, boundTexture);

    GLuint prog;

    {
      const char *vs =
          "attribute vec2 pos;\n"
          "void main() { gl_Position = vec4(pos, 0.5, 0.5); }";

      char fs_src[] =
          "precision highp float;\n"
          "uniform vec2 res;\n"
          "uniform sampler2D srcTex;\n"
          "void main() { gl_FragColor = texture2D(srcTex, vec2(gl_FragCoord.xy)/res).?aaa; }";

      char *swizzle = strchr(fs_src, '?');

      if(format == eGL_ALPHA)
        *swizzle = 'a';
      else
        *swizzle = 'r';

      const char *fs = fs_src;

      GLuint vert = GL.glCreateShader(eGL_VERTEX_SHADER);
      GLuint frag = GL.glCreateShader(eGL_FRAGMENT_SHADER);

      GL.glShaderSource(vert, 1, &vs, NULL);
      GL.glShaderSource(frag, 1, &fs, NULL);

      GL.glCompileShader(vert);
      GL.glCompileShader(frag);

      char buffer[1024] = {0};
      GLint status = 0;

      GL.glGetShaderiv(vert, eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        GL.glGetShaderInfoLog(vert, 1024, NULL, buffer);
        RDCERR("Shader error: %s", buffer);
      }

      GL.glGetShaderiv(frag, eGL_COMPILE_STATUS, &status);
      if(status == 0)
      {
        GL.glGetShaderInfoLog(frag, 1024, NULL, buffer);
        RDCERR("Shader error: %s", buffer);
      }

      prog = GL.glCreateProgram();

      GL.glAttachShader(prog, vert);
      GL.glAttachShader(prog, frag);

      GL.glLinkProgram(prog);

      GL.glGetProgramiv(prog, eGL_LINK_STATUS, &status);
      if(status == 0)
      {
        GL.glGetProgramInfoLog(prog, 1024, NULL, buffer);
        RDCERR("Link error: %s", buffer);
      }

      GL.glDeleteShader(vert);
      GL.glDeleteShader(frag);
    }

    // fullscreen triangle
    float verts[] = {
        -1.0f, -1.0f,    // vertex 0
        3.0f,  -1.0f,    // vertex 1
        -1.0f, 3.0f,     // vertex 2
    };

    GLuint vb = 0;
    GL.glGenBuffers(1, &vb);
    GL.glBindBuffer(eGL_ARRAY_BUFFER, vb);
    GL.glBufferData(eGL_ARRAY_BUFFER, sizeof(verts), verts, eGL_STATIC_DRAW);

    GLuint vao;
    GL.glGenVertexArrays(1, &vao);

    GL.glBindVertexArray(vao);

    GLint loc = GL.glGetAttribLocation(prog, "pos");
    GL.glVertexAttribPointer((GLuint)loc, 2, eGL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
    GL.glEnableVertexAttribArray((GLuint)loc);

    GL.glUseProgram(prog);

    loc = GL.glGetUniformLocation(prog, "srcTex");
    GL.glUniform1i(loc, 0);
    loc = GL.glGetUniformLocation(prog, "res");
    GL.glUniform2f(loc, float(width), float(height));

    GL.glDrawArrays(eGL_TRIANGLES, 0, 3);

    GL.glDeleteVertexArrays(1, &vao);
    GL.glDeleteBuffers(1, &vb);
    GL.glDeleteProgram(prog);

    // pop rendering state
    textState.Pop(true);

    // delete this texture once we're done, at the end of the function
    deltex = readtex;

    // restore base/max level as we changed them to sample from the right level above
    GL.glBindTexture(target, boundTexture);
    GL.glTexParameteri(target, eGL_TEXTURE_BASE_LEVEL, baseLevel);
    GL.glTexParameteri(target, eGL_TEXTURE_MAX_LEVEL, maxLevel);

    // read from the blitted texture from level 0, as red
    GL.glBindTexture(target, readtex);
    level = 0;
    format = remapformat;

    RDCDEBUG("Done blit");
  }

  for(GLint d = 0; d < depth; ++d)
  {
    switch(target)
    {
      case eGL_TEXTURE_3D:
      case eGL_TEXTURE_2D_ARRAY:
      case eGL_TEXTURE_CUBE_MAP_ARRAY:
      case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        GL.glFramebufferTextureLayer(eGL_FRAMEBUFFER, attachment, readtex, level, d);
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
        GL.glFramebufferTexture2D(eGL_FRAMEBUFFER, attachment, target, readtex, level);
        break;
    }

    GLenum status = GL.glCheckFramebufferStatus(eGL_FRAMEBUFFER);

    if(status != eGL_FRAMEBUFFER_COMPLETE)
      RDCERR("glReadPixels emulation FBO is %s", ToStr(status).c_str());

    byte *dst = (byte *)pixels + d * sliceSize;
    GLenum readFormat = fixBGRA ? eGL_RGBA : format;

    GLenum implFormat = eGL_NONE, implType = eGL_NONE;

    GL.glGetIntegerv(eGL_IMPLEMENTATION_COLOR_READ_FORMAT, (GLint *)&implFormat);
    GL.glGetIntegerv(eGL_IMPLEMENTATION_COLOR_READ_TYPE, (GLint *)&implType);

    // spec says this must be supported
    bool validReadback = (readFormat == eGL_RGBA && type == eGL_UNSIGNED_BYTE);

    // the implementation is allowed to support one other format/type pair, it can vary by texture.
    // Normally this will be the 'natural' readback format/type pair that we are trying
    validReadback |= (readFormat == implFormat && type == implType);

    if(validReadback)
    {
      GL.glReadPixels(0, 0, width, height, readFormat, type, (void *)dst);
    }
    else
    {
      // unfortunately the readback is not supported directly, we'll need to fudge it.
      RDCDEBUG("Reading as %s/%s but impl supported pair is %s/%s", ToStr(readFormat).c_str(),
               ToStr(type).c_str(), ToStr(implFormat).c_str(), ToStr(implType).c_str());

      if(readFormat == implFormat && (type == eGL_HALF_FLOAT_OES || type == eGL_HALF_FLOAT) &&
         (implType == eGL_HALF_FLOAT_OES || implType == eGL_HALF_FLOAT))
      {
        // if the format itself is supported and we just have a mismatch between eGL_HALF_FLOAT_OES
        // and eGL_HALF_FLOAT (different enum values), we can just use the implementation's pair
        // as-is.

        GL.glReadPixels(0, 0, width, height, readFormat, implType, (void *)dst);
      }
      else if(readFormat == eGL_RGB && implFormat == eGL_RGBA && type == implType)
      {
        // if the only difference is that the implementation wants to read RGBA for an RGB format
        // (could be understandable if the native storage is RGBA anyway for RGB textures) then we
        // can readback into a temporary buffer and strip the alpha.

        size_t size = GetByteSize(width, height, 1, implFormat, type);
        byte *readback = new byte[size];
        GL.glReadPixels(0, 0, width, height, implFormat, implType, readback);

        // how big is a component (1/2/4 bytes)
        size_t compSize = GetByteSize(1, 1, 1, eGL_RED, type);

        byte *src = readback;

        // for each pixel
        for(GLint i = 0; i < width * height; i++)
        {
          // copy RGB
          memcpy(dst, src, compSize * 3);

          // advance dst by RGB
          dst += compSize * 3;

          // advance src by RGBA
          src += compSize * 4;
        }

        delete[] readback;

        fixBGRA = false;
      }
      else
      {
        RDCERR("Unhandled readback failure");
        memset(dst, 0, sliceSize);
      }
    }

    if(fixBGRA)
    {
      // since we read back the texture with RGBA format, we have to flip the R and B components
      byte *b = dst;
      for(GLint i = 0, n = width * height; i < n; ++i, b += 4)
        std::swap(*b, *(b + 2));
    }
  }

  if(deltex)
  {
    GL.glDeleteTextures(1, &deltex);
    GL.glBindTexture(target, boundTexture);
  }

  GL.glDeleteFramebuffers(1, &fbo);
}

void APIENTRY _glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                        const void *indices, GLint basevertex)
{
  if(basevertex == 0)
    GL.glDrawElements(mode, count, type, indices);
  else
    RDCERR("glDrawElementsBaseVertex is not supported! No draw will be called!");
}

void APIENTRY _glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                 const void *indices, GLsizei instancecount,
                                                 GLint basevertex)
{
  if(basevertex == 0)
    GL.glDrawElementsInstanced(mode, count, type, indices, instancecount);
  else
    RDCERR("glDrawElementsInstancedBaseVertex is not supported! No draw will be called!");
}

void APIENTRY _glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                             GLenum type, const void *indices, GLint basevertex)
{
  if(basevertex == 0)
    GL.glDrawRangeElements(mode, start, end, count, type, indices);
  else
    RDCERR("glDrawRangeElementsBaseVertex is not supported! No draw will be called!");
}

#pragma endregion

};    // namespace glEmulate

void GLDispatchTable::EmulateUnsupportedFunctions()
{
#define EMULATE_UNSUPPORTED(func)             \
  if(!this->func)                             \
  {                                           \
    RDCLOG("Emulating " #func);               \
    this->func = &CONCAT(glEmulate::_, func); \
  }

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
  //
  // Update 2018-Jan - this might be the problem with the registry having the wrong signature for
  // glClearNamedFramebufferfi - if the arguments were mismatched it would explain both invalid
  // argument errors and ABI problems. For now though (and since as mentioned above it's cheap to
  // emulate) we leave it on. See issue #842
  this->glClearNamedFramebufferfi = &glEmulate::_glClearNamedFramebufferfi;

  // workaround for AMD bug or weird behaviour. glVertexArrayElementBuffer doesn't update the
  // GL_ELEMENT_ARRAY_BUFFER_BINDING global query, when binding the VAO subsequently *will*.
  // I'm not sure if that's correct (weird) behaviour or buggy, but we can work around it just
  // by avoiding use of the DSA function and always doing our emulated version.
  //
  // VendorCheck[VendorCheck_AMD_vertex_array_elem_buffer_query]
  this->glVertexArrayElementBuffer = &glEmulate::_glVertexArrayElementBuffer;
}

void GLDispatchTable::EmulateRequiredExtensions()
{
#define EMULATE_FUNC(func) this->func = &CONCAT(glEmulate::_, func);

#define SAVE_REAL_FUNC(func)                                                       \
  if(!glEmulate::CONCAT(func, _real) && this->func != &CONCAT(glEmulate::_, func)) \
    glEmulate::CONCAT(func, _real) = this->func;

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

  // really silly case, we just forward to non-float version
  if(!IsGLES && !HasExt[ARB_ES2_compatibility])
  {
    RDCLOG("Emulating ARB_ES2_compatibility");
    EMULATE_FUNC(glClearDepthf);
  }

  // we manually implement these queries
  if(!HasExt[ARB_internalformat_query2])
  {
    RDCLOG("Emulating ARB_internalformat_query2");
    // GLES supports glGetInternalformativ from core 3.0 but it only accepts GL_NUM_SAMPLE_COUNTS
    // and GL_SAMPLES params
    SAVE_REAL_FUNC(glGetInternalformativ);
    EMULATE_FUNC(glGetInternalformativ);
  }

  if(!HasExt[ARB_separate_shader_objects])
  {
    RDCLOG("Emulating ARB_separate_shader_objects");

    // need to be able to forward any queries other than GL_PROGRAM_SEPARABLE
    SAVE_REAL_FUNC(glGetProgramiv);

    EMULATE_FUNC(glUseProgramStages);
    EMULATE_FUNC(glActiveShaderProgram);
    EMULATE_FUNC(glCreateShaderProgramv);
    EMULATE_FUNC(glBindProgramPipeline);
    EMULATE_FUNC(glDeleteProgramPipelines);
    EMULATE_FUNC(glGenProgramPipelines);
    EMULATE_FUNC(glIsProgramPipeline);
    EMULATE_FUNC(glProgramParameteri);
    EMULATE_FUNC(glGetProgramiv);
    EMULATE_FUNC(glGetProgramPipelineiv);
    EMULATE_FUNC(glProgramUniform1i);
    EMULATE_FUNC(glProgramUniform2i);
    EMULATE_FUNC(glProgramUniform3i);
    EMULATE_FUNC(glProgramUniform4i);
    EMULATE_FUNC(glProgramUniform1ui);
    EMULATE_FUNC(glProgramUniform2ui);
    EMULATE_FUNC(glProgramUniform3ui);
    EMULATE_FUNC(glProgramUniform4ui);
    EMULATE_FUNC(glProgramUniform1f);
    EMULATE_FUNC(glProgramUniform2f);
    EMULATE_FUNC(glProgramUniform3f);
    EMULATE_FUNC(glProgramUniform4f);
    EMULATE_FUNC(glProgramUniform1d);
    EMULATE_FUNC(glProgramUniform2d);
    EMULATE_FUNC(glProgramUniform3d);
    EMULATE_FUNC(glProgramUniform4d);
    EMULATE_FUNC(glProgramUniform1iv);
    EMULATE_FUNC(glProgramUniform2iv);
    EMULATE_FUNC(glProgramUniform3iv);
    EMULATE_FUNC(glProgramUniform4iv);
    EMULATE_FUNC(glProgramUniform1uiv);
    EMULATE_FUNC(glProgramUniform2uiv);
    EMULATE_FUNC(glProgramUniform3uiv);
    EMULATE_FUNC(glProgramUniform4uiv);
    EMULATE_FUNC(glProgramUniform1fv);
    EMULATE_FUNC(glProgramUniform2fv);
    EMULATE_FUNC(glProgramUniform3fv);
    EMULATE_FUNC(glProgramUniform4fv);
    EMULATE_FUNC(glProgramUniform1dv);
    EMULATE_FUNC(glProgramUniform2dv);
    EMULATE_FUNC(glProgramUniform3dv);
    EMULATE_FUNC(glProgramUniform4dv);
    EMULATE_FUNC(glProgramUniformMatrix2fv);
    EMULATE_FUNC(glProgramUniformMatrix3fv);
    EMULATE_FUNC(glProgramUniformMatrix4fv);
    EMULATE_FUNC(glProgramUniformMatrix2dv);
    EMULATE_FUNC(glProgramUniformMatrix3dv);
    EMULATE_FUNC(glProgramUniformMatrix4dv);
    EMULATE_FUNC(glProgramUniformMatrix2x3fv);
    EMULATE_FUNC(glProgramUniformMatrix3x2fv);
    EMULATE_FUNC(glProgramUniformMatrix2x4fv);
    EMULATE_FUNC(glProgramUniformMatrix4x2fv);
    EMULATE_FUNC(glProgramUniformMatrix3x4fv);
    EMULATE_FUNC(glProgramUniformMatrix4x3fv);
    EMULATE_FUNC(glProgramUniformMatrix2x3dv);
    EMULATE_FUNC(glProgramUniformMatrix3x2dv);
    EMULATE_FUNC(glProgramUniformMatrix2x4dv);
    EMULATE_FUNC(glProgramUniformMatrix4x2dv);
    EMULATE_FUNC(glProgramUniformMatrix3x4dv);
    EMULATE_FUNC(glProgramUniformMatrix4x3dv);
    EMULATE_FUNC(glValidateProgramPipeline);
    EMULATE_FUNC(glGetProgramPipelineInfoLog);
  }

  if(!HasExt[ARB_program_interface_query])
  {
    RDCLOG("Emulating ARB_program_interface_query");

    EMULATE_FUNC(glGetProgramInterfaceiv);
    EMULATE_FUNC(glGetProgramResourceIndex);
    EMULATE_FUNC(glGetProgramResourceName);
    EMULATE_FUNC(glGetProgramResourceiv);
  }

  // only emulate ARB_vertex_attrib_binding on replay
  if(!HasExt[ARB_vertex_attrib_binding] && RenderDoc::Inst().IsReplayApp())
  {
    RDCLOG("Emulating ARB_vertex_attrib_binding");

    glEmulate::_ResetVertexAttribBinding();

    EMULATE_FUNC(glBindVertexBuffer);
    EMULATE_FUNC(glVertexAttribFormat);
    EMULATE_FUNC(glVertexAttribIFormat);
    EMULATE_FUNC(glVertexAttribLFormat);
    EMULATE_FUNC(glVertexAttribBinding);
    EMULATE_FUNC(glVertexBindingDivisor);

    // we also intercept the old-style functions to ensure everything stays consistent.
    SAVE_REAL_FUNC(glVertexAttribPointer);
    SAVE_REAL_FUNC(glVertexAttribIPointer);
    SAVE_REAL_FUNC(glVertexAttribLPointer);
    SAVE_REAL_FUNC(glVertexAttribDivisor);
    EMULATE_FUNC(glVertexAttribPointer);
    EMULATE_FUNC(glVertexAttribIPointer);
    EMULATE_FUNC(glVertexAttribLPointer);
    EMULATE_FUNC(glVertexAttribDivisor);

    // need to intercept get functions to implement the new queries. Anything unknown we just
    // re-direct
    SAVE_REAL_FUNC(glGetIntegerv);
    SAVE_REAL_FUNC(glGetIntegeri_v);
    SAVE_REAL_FUNC(glGetVertexAttribiv);
    EMULATE_FUNC(glGetIntegerv);
    EMULATE_FUNC(glGetIntegeri_v);
    EMULATE_FUNC(glGetVertexAttribiv);

    // emulate the EXT_dsa accessor functions too
    EMULATE_FUNC(glVertexArrayBindVertexBufferEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribFormatEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribIFormatEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribLFormatEXT);
    EMULATE_FUNC(glVertexArrayVertexAttribBindingEXT);
    EMULATE_FUNC(glVertexArrayVertexBindingDivisorEXT);
    EMULATE_FUNC(glGetVertexArrayIntegeri_vEXT);
    EMULATE_FUNC(glGetVertexArrayIntegervEXT);

    if(GL.glGetInteger64i_v)
    {
      SAVE_REAL_FUNC(glGetInteger64i_v);
      EMULATE_FUNC(glGetInteger64i_v);
    }
  }

  // APIs that are not available at all in GLES.
  if(IsGLES)
  {
    RDCLOG("Emulating GLES 3.x functions");

    EMULATE_FUNC(glGetBufferSubData);
    EMULATE_FUNC(glGetTexImage);

    if(GLCoreVersion < 31)
    {
      RDCLOG("Emulating GLES 3.1 functions");

      EMULATE_FUNC(glGetTexLevelParameteriv);
      EMULATE_FUNC(glGetTexLevelParameterfv);
    }

    if(GLCoreVersion < 32)
    {
      RDCLOG("Emulating GLES 3.2 functions");

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
  //
  // We ALWAYS emulate on replay since the EXT_dsa functions are too buggy on drivers like NV to be
  // relied upon to work without messing up. We only 'promote' to EXT_dsa on replay so we can still
  // leave the functions as they are during capture.
  if(!HasExt[EXT_direct_state_access] || RenderDoc::Inst().IsReplayApp())
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

  // some functions we use from ARB that have no EXT equivalent, need to emulate them as well
  if(!HasExt[ARB_direct_state_access])
  {
    RDCLOG("Emulating ARB_direct_state_access");
    EMULATE_FUNC(glTransformFeedbackBufferBase)
    EMULATE_FUNC(glTransformFeedbackBufferRange)
    EMULATE_FUNC(glClearNamedFramebufferiv)
    EMULATE_FUNC(glClearNamedFramebufferuiv)
    EMULATE_FUNC(glClearNamedFramebufferfv)
    EMULATE_FUNC(glClearNamedFramebufferfi)
    EMULATE_FUNC(glBlitNamedFramebuffer)
    EMULATE_FUNC(glVertexArrayElementBuffer);
    EMULATE_FUNC(glVertexArrayVertexBuffers)
  }
}

void GLDispatchTable::DriverForEmulation(WrappedOpenGL *driver)
{
  glEmulate::driver = driver;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "../gl_shader_refl.h"
#include "3rdparty/catch/catch.hpp"
#include "strings/string_utils.h"

GLint APIENTRY _testStub_GetUniformLocation(GLuint program, const GLchar *name)
{
  // use existing ARB_program_interface_query to get value
  GLuint index = GL.glGetProgramResourceIndex(program, eGL_UNIFORM, name);
  RDCASSERT(index != GL_INVALID_INDEX);

  return index;
}

void APIENTRY _testStub_GetUniformiv(GLuint program, GLint location, GLint *params)
{
  // abuse this query which returns the right value for uniform bindings also
  GLenum prop = eGL_UNIFORM;
  GL.glGetProgramResourceiv(program, eGL_UNIFORM, location, 1, &prop, 1, NULL, params);
}

void APIENTRY _testStub_GetIntegerv(GLenum pname, GLint *params)
{
  // fixed definition
  if(pname == eGL_MAX_VERTEX_ATTRIBS)
    *params = 16;
  else
    RDCERR("Unexpected pname in test stub: %s", ToStr(pname).c_str());
}

void APIENTRY _testStub_GetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex,
                                                       GLenum pname, GLint *params)
{
  if(pname == eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER)
  {
    GLenum prop = eGL_REFERENCED_BY_VERTEX_SHADER;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else if(pname == eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER)
  {
    GLenum prop = eGL_REFERENCED_BY_TESS_CONTROL_SHADER;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else if(pname == eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER)
  {
    GLenum prop = eGL_REFERENCED_BY_TESS_EVALUATION_SHADER;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else if(pname == eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER)
  {
    GLenum prop = eGL_REFERENCED_BY_GEOMETRY_SHADER;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else if(pname == eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER)
  {
    GLenum prop = eGL_REFERENCED_BY_FRAGMENT_SHADER;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else if(pname == eGL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER)
  {
    GLenum prop = eGL_REFERENCED_BY_COMPUTE_SHADER;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else if(pname == eGL_ATOMIC_COUNTER_BUFFER_BINDING)
  {
    GLenum prop = eGL_ATOMIC_COUNTER_BUFFER_INDEX;
    GL.glGetProgramResourceiv(program, eGL_ATOMIC_COUNTER_BUFFER, bufferIndex, 1, &prop, 1, NULL,
                              params);
  }
  else
  {
    RDCERR("Unexpected pname in test stub: %s", ToStr(pname).c_str());
  }
}

GLuint APIENTRY _testStub_GetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
  return GL.glGetProgramResourceIndex(program, eGL_UNIFORM_BLOCK, uniformBlockName);
}

void APIENTRY _testStub_GetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex,
                                                GLenum pname, GLint *params)
{
  if(pname == eGL_UNIFORM_BLOCK_BINDING)
  {
    // use this internal query which returns the default binding for uniform block bindings
    GLenum prop = eGL_UNIFORM_BLOCK_BINDING;
    GL.glGetProgramResourceiv(program, eGL_UNIFORM_BLOCK, uniformBlockIndex, 1, &prop, 1, NULL,
                              params);
  }
  else
  {
    RDCERR("Unexpected pname in test stub: %s", ToStr(pname).c_str());
  }
}

GLint APIENTRY _testStub_AttribLocation(GLuint program, const GLchar *name)
{
  GLuint index = GL.glGetProgramResourceIndex(program, eGL_UNIFORM_BLOCK, name);
  RDCASSERT(index != GL_INVALID_INDEX);
  GLenum prop = eGL_LOCATION;
  GLint value = -1;
  RDCASSERT(GL.glGetAttribLocation == &_testStub_AttribLocation);
  GL.glGetAttribLocation = NULL;
  GL.glGetProgramResourceiv(program, eGL_PROGRAM_INPUT, index, 1, &prop, 1, NULL, &value);
  GL.glGetAttribLocation = &_testStub_AttribLocation;
  return value;
}

void MakeOfflineShaderReflection(ShaderStage stage, const std::string &source,
                                 ShaderReflection &refl, ShaderBindpointMapping &mapping)
{
  InitSPIRVCompiler();
  RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);

  // as a hack, create a local 'driver' and just populate m_Programs with what we want.
  GLDummyPlatform dummy;
  WrappedOpenGL driver(dummy);

  GL.DriverForEmulation(&driver);

  RDCEraseEl(HasExt);
  // need to pretend to have SSBO extension so we reflect SSBOs
  HasExt[ARB_shader_storage_buffer_object] = true;
  GL = GLDispatchTable();
  GL.EmulateRequiredExtensions();

  glslang::TShader *sh = CompileShaderForReflection(SPIRVShaderStage(stage), {source});

  REQUIRE(sh);

  glslang::TProgram *prog = LinkProgramForReflection({sh});

  REQUIRE(prog);

  // the lookup won't get a valid Id, so set the program to the ResourceId()
  driver.m_Programs[ResourceId()].glslangProgram = prog;

  GLuint fakeProg = 0;

  FixedFunctionVertexOutputs outputUsage;
  CheckVertexOutputUses({source}, outputUsage);

  MakeShaderReflection(ShaderEnum((size_t)stage), fakeProg, refl, outputUsage);

  // implement some stubs for testing
  GL.glGetUniformLocation = &_testStub_GetUniformLocation;
  GL.glGetUniformiv = &_testStub_GetUniformiv;
  GL.glGetActiveAtomicCounterBufferiv = &_testStub_GetActiveAtomicCounterBufferiv;
  GL.glGetUniformBlockIndex = &_testStub_GetUniformBlockIndex;
  GL.glGetActiveUniformBlockiv = &_testStub_GetActiveUniformBlockiv;
  GL.glGetIntegerv = &_testStub_GetIntegerv;
  GL.glGetAttribLocation = &_testStub_AttribLocation;

  GetBindpointMapping(fakeProg, (int)stage, &refl, mapping);

  RDCEraseEl(HasExt);
  GL = GLDispatchTable();
  GL.DriverForEmulation(NULL);
}

// helper function that uses the replay proxy system to compile and reflect the shader using the
// current driver. Unused by default but you can change the unit test below to call this function
// instead of MakeOfflineShaderReflection.
//
// Note that we can't fill out ShaderBindpointMapping easily on the actual driver
void MakeOnlineShaderReflection(ShaderStage stage, const std::string &source,
                                ShaderReflection &refl, ShaderBindpointMapping &mapping)
{
  ReplayStatus status = ReplayStatus::UnknownError;
  IReplayDriver *driver = NULL;

  std::map<RDCDriver, std::string> replays = RenderDoc::Inst().GetReplayDrivers();

  if(replays.find(RDCDriver::OpenGL) != replays.end())
    status = RenderDoc::Inst().CreateProxyReplayDriver(RDCDriver::OpenGL, &driver);

  if(status != ReplayStatus::Succeeded)
  {
    RDCERR("No GL support locally, couldn't create proxy GL driver for reflection");
    return;
  }

  ResourceId id;
  std::string errors;
  bytebuf buf;
  buf.resize(source.size());
  memcpy(buf.data(), source.data(), source.size());
  driver->BuildCustomShader(ShaderEncoding::GLSL, buf, "main", ShaderCompileFlags(), stage, &id,
                            &errors);

  if(id == ResourceId())
  {
    RDCERR("Couldn't build shader for reflection:\n%s", errors.c_str());
    return;
  }

  refl = *driver->GetShader(id, ShaderEntryPoint("main", ShaderStage::Fragment));

  driver->FreeCustomShader(id);

  driver->Shutdown();
}

TEST_CASE("Validate ARB_program_interface_query emulation", "[opengl][glslang]")
{
  SECTION("Single shader deep dive")
  {
    std::string source = R"(
#version 450 core

struct glstruct
{
  float a;
  int b;
  mat2x2 c;
};

layout(binding = 8, std140) uniform ubo_block {
	float ubo_a;
	layout(column_major) mat4x3 ubo_b;
	layout(row_major) mat4x3 ubo_c;
  ivec2 ubo_d;
  vec2 ubo_e[3];
  glstruct ubo_f;
  layout(offset = 256) vec4 ubo_z;
} ubo_root;

layout(binding = 2, std430) buffer ssbo
{
  uint ssbo_a[10];
  glstruct ssbo_b[3];
  float ssbo_c;
} ssbo_root;

layout(binding = 0) uniform atomic_uint atom;

layout(location = 3) in vec2 a_input;
layout(location = 6) flat in uvec3 z_input;

uniform vec3 global_var[5];
uniform mat3x2 global_var2[3];

layout(binding = 3) uniform sampler2D tex2D;
layout(binding = 5) uniform isampler3D tex3D;

layout(location = 0) out vec4 a_output;
layout(location = 1) out vec3 z_output;
layout(location = 2) out int b_output;

void main() {
  float a = ubo_root.ubo_a + global_var2[2][0][1];
  a_output = vec4(sin(float(a) + gl_FragCoord.x), 0, 0, 1);
  z_output = textureLod(tex2D, a_output.xy, a_output.z).xyz + a_input.xyx + global_var[4];
  ssbo_root.ssbo_a[5] = 4 + atomicCounter(atom) + z_input.y;
  b_output = ssbo_root.ssbo_b[2].b + texelFetch(tex3D, ivec3(z_input), 0).x;
  gl_FragDepth = z_output.y; 
}

)";

#define REQUIRE_ARRAY_SIZE(size, min) \
  REQUIRE(size >= min);               \
  CHECK(size == min);

    ShaderReflection refl;
    ShaderBindpointMapping mapping;
    MakeOfflineShaderReflection(ShaderStage::Fragment, source, refl, mapping);

    REQUIRE_ARRAY_SIZE(refl.inputSignature.size(), 3);
    {
      CHECK(refl.inputSignature[0].varName == "a_input");
      {
        const SigParameter &sig = refl.inputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 3);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.inputSignature[1].varName == "z_input");
      {
        const SigParameter &sig = refl.inputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 6);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::UInt);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.inputSignature[2].varName == "gl_FragCoord");
      {
        const SigParameter &sig = refl.inputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }
    }

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 4);
    {
      CHECK(refl.outputSignature[0].varName == "a_output");
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::ColorOutput);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName == "z_output");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 1);
        CHECK(sig.systemValue == ShaderBuiltin::ColorOutput);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[2].varName == "b_output");
      {
        const SigParameter &sig = refl.outputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 2);
        CHECK(sig.systemValue == ShaderBuiltin::ColorOutput);
        CHECK(sig.compType == CompType::SInt);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[3].varName == "gl_FragDepth");
      {
        const SigParameter &sig = refl.outputSignature[3];
        INFO("signature element: " << sig.varName.c_str());

        // when not running with a driver we default to just using the index instead of looking up
        // the location of outputs, so this will be wrong
        // CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::DepthOutput);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }
    }

    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 2);
    {
      CHECK(refl.readOnlyResources[0].name == "tex2D");
      {
        const ShaderResource &res = refl.readOnlyResources[0];
        INFO("read-only resource: " << res.name.c_str());

        CHECK(res.bindPoint == 0);
        CHECK(res.resType == TextureType::Texture2D);
        CHECK(res.variableType.members.empty());
        CHECK(res.variableType.descriptor.type == VarType::Float);
        CHECK(res.variableType.descriptor.rows == 1);
        CHECK(res.variableType.descriptor.columns == 4);
        CHECK(res.variableType.descriptor.name == "sampler2D");
      }

      CHECK(refl.readOnlyResources[1].name == "tex3D");
      {
        const ShaderResource &res = refl.readOnlyResources[1];
        INFO("read-only resource: " << res.name.c_str());

        CHECK(res.bindPoint == 1);
        CHECK(res.resType == TextureType::Texture3D);
        CHECK(res.variableType.members.empty());
        CHECK(res.variableType.descriptor.type == VarType::SInt);
        CHECK(res.variableType.descriptor.rows == 1);
        CHECK(res.variableType.descriptor.columns == 4);
        CHECK(res.variableType.descriptor.name == "isampler3D");
      }
    }

    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 2);
    {
      CHECK(refl.readWriteResources[0].name == "atom");
      {
        const ShaderResource &res = refl.readWriteResources[0];
        INFO("read-write resource: " << res.name.c_str());

        CHECK(res.bindPoint == 0);
        CHECK(res.resType == TextureType::Buffer);
        CHECK(res.variableType.members.empty());
        CHECK(res.variableType.descriptor.type == VarType::UInt);
        CHECK(res.variableType.descriptor.rows == 1);
        CHECK(res.variableType.descriptor.columns == 1);
        CHECK(res.variableType.descriptor.name == "atomic_uint");
      }

      CHECK(refl.readWriteResources[1].name == "ssbo");
      {
        const ShaderResource &res = refl.readWriteResources[1];
        INFO("read-write resource: " << res.name.c_str());

        CHECK(res.bindPoint == 1);
        CHECK(res.resType == TextureType::Buffer);
        CHECK(res.variableType.descriptor.type == VarType::UInt);
        CHECK(res.variableType.descriptor.rows == 0);
        CHECK(res.variableType.descriptor.columns == 0);
        CHECK(res.variableType.descriptor.name == "buffer");

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "ssbo_a");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 0);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::UInt);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 1);
            CHECK(member.type.descriptor.elements == 10);
            CHECK(member.type.descriptor.arrayByteStride == 4);
            CHECK(member.type.descriptor.name == "uint");
          }

          CHECK(res.variableType.members[1].name == "ssbo_b");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 40);
            // this doesn't reflect in native introspection, so we skip it
            // CHECK(member.type.descriptor.elements == 3);
            CHECK(member.type.descriptor.name == "struct");
            CHECK(member.type.descriptor.arrayByteStride == 24);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 3);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 40);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.descriptor.type == VarType::Float);
                CHECK(submember.type.descriptor.rows == 1);
                CHECK(submember.type.descriptor.columns == 1);
                CHECK(submember.type.descriptor.name == "float");
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 44);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.descriptor.type == VarType::SInt);
                CHECK(submember.type.descriptor.rows == 1);
                CHECK(submember.type.descriptor.columns == 1);
                CHECK(submember.type.descriptor.name == "int");
              }

              CHECK(member.type.members[2].name == "c");
              {
                const ShaderConstant &submember = member.type.members[2];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 48);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.descriptor.type == VarType::Float);
                CHECK(submember.type.descriptor.rows == 2);
                CHECK(submember.type.descriptor.columns == 2);
                CHECK(submember.type.descriptor.rowMajorStorage == false);
                CHECK(submember.type.descriptor.name == "mat2");
              }
            }
          }

          CHECK(res.variableType.members[2].name == "ssbo_c");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 112);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 1);
            CHECK(member.type.descriptor.name == "float");
          }
        }
      }
    }

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 2);
    {
      CHECK(refl.constantBlocks[0].name == "ubo_block");
      {
        const ConstantBlock &cblock = refl.constantBlocks[0];
        INFO("UBO: " << cblock.name.c_str());

        CHECK(cblock.bindPoint == 0);
        CHECK(cblock.bufferBacked);
        CHECK(cblock.byteSize == 272);

        REQUIRE_ARRAY_SIZE(cblock.variables.size(), 1);

        CHECK(cblock.variables[0].name == "ubo_block");
        const ShaderConstant &ubo_root = cblock.variables[0];

        CHECK(ubo_root.byteOffset == 0);
        CHECK(ubo_root.type.descriptor.name == "struct");

        REQUIRE_ARRAY_SIZE(ubo_root.type.members.size(), 7);
        {
          CHECK(ubo_root.type.members[0].name == "ubo_a");
          {
            const ShaderConstant &member = ubo_root.type.members[0];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 0);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 1);
            CHECK(member.type.descriptor.name == "float");
          }

          CHECK(ubo_root.type.members[1].name == "ubo_b");
          {
            const ShaderConstant &member = ubo_root.type.members[1];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 16);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 3);
            CHECK(member.type.descriptor.columns == 4);
            CHECK(member.type.descriptor.rowMajorStorage == false);
            CHECK(member.type.descriptor.name == "mat4x3");
          }

          CHECK(ubo_root.type.members[2].name == "ubo_c");
          {
            const ShaderConstant &member = ubo_root.type.members[2];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 80);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 3);
            CHECK(member.type.descriptor.columns == 4);
            CHECK(member.type.descriptor.rowMajorStorage == true);
            CHECK(member.type.descriptor.name == "mat4x3");
          }

          CHECK(ubo_root.type.members[3].name == "ubo_d");
          {
            const ShaderConstant &member = ubo_root.type.members[3];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 128);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::SInt);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 2);
            CHECK(member.type.descriptor.name == "ivec2");
          }

          CHECK(ubo_root.type.members[4].name == "ubo_e");
          {
            const ShaderConstant &member = ubo_root.type.members[4];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 144);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 2);
            CHECK(member.type.descriptor.elements == 3);
            CHECK(member.type.descriptor.arrayByteStride == 16);
            CHECK(member.type.descriptor.name == "vec2");
          }

          CHECK(ubo_root.type.members[5].name == "ubo_f");
          {
            const ShaderConstant &member = ubo_root.type.members[5];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 192);
            // this doesn't reflect in native introspection, so we skip it
            // CHECK(member.type.descriptor.elements == 3);
            CHECK(member.type.descriptor.name == "struct");

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 3);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("UBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 192);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.descriptor.type == VarType::Float);
                CHECK(submember.type.descriptor.rows == 1);
                CHECK(submember.type.descriptor.columns == 1);
                CHECK(submember.type.descriptor.name == "float");
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("UBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 196);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.descriptor.type == VarType::SInt);
                CHECK(submember.type.descriptor.rows == 1);
                CHECK(submember.type.descriptor.columns == 1);
                CHECK(submember.type.descriptor.name == "int");
              }

              CHECK(member.type.members[2].name == "c");
              {
                const ShaderConstant &submember = member.type.members[2];
                INFO("UBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 208);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.descriptor.type == VarType::Float);
                CHECK(submember.type.descriptor.rows == 2);
                CHECK(submember.type.descriptor.columns == 2);
                CHECK(submember.type.descriptor.rowMajorStorage == false);
                CHECK(submember.type.descriptor.name == "mat2");
              }
            }
          }

          CHECK(ubo_root.type.members[6].name == "ubo_z");
          {
            const ShaderConstant &member = ubo_root.type.members[6];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 256);
            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 4);
            CHECK(member.type.descriptor.name == "vec4");
          }
        }
      }

      CHECK(refl.constantBlocks[1].name == "$Globals");
      {
        const ConstantBlock &cblock = refl.constantBlocks[1];
        INFO("UBO: " << cblock.name.c_str());

        CHECK(cblock.bindPoint == 1);
        CHECK(!cblock.bufferBacked);

        REQUIRE_ARRAY_SIZE(cblock.variables.size(), 2);
        {
          CHECK(cblock.variables[0].name == "global_var");
          {
            const ShaderConstant &member = cblock.variables[0];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 1);
            CHECK(member.type.descriptor.columns == 3);
            CHECK(member.type.descriptor.elements == 5);
            CHECK(member.type.descriptor.name == "vec3");
          }

          CHECK(cblock.variables[1].name == "global_var2");
          {
            const ShaderConstant &member = cblock.variables[1];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.type.members.empty());
            CHECK(member.type.descriptor.type == VarType::Float);
            CHECK(member.type.descriptor.rows == 2);
            CHECK(member.type.descriptor.columns == 3);
            CHECK(member.type.descriptor.elements == 3);
            CHECK(member.type.descriptor.rowMajorStorage == false);
            CHECK(member.type.descriptor.name == "mat3x2");
          }
        }
      }
    }

    REQUIRE(refl.samplers.empty());
    REQUIRE(refl.interfaces.empty());

    REQUIRE_ARRAY_SIZE(mapping.inputAttributes.size(), 16);
    for(size_t i = 0; i < mapping.inputAttributes.size(); i++)
    {
      CHECK(mapping.inputAttributes[i] == -1);
    }

    REQUIRE_ARRAY_SIZE(mapping.readOnlyResources.size(), 2);
    {
      // tex2d
      CHECK(mapping.readOnlyResources[0].bindset == 0);
      CHECK(mapping.readOnlyResources[0].bind == 3);
      CHECK(mapping.readOnlyResources[0].arraySize == 1);
      CHECK(mapping.readOnlyResources[0].used);

      // tex3d
      CHECK(mapping.readOnlyResources[1].bindset == 0);
      CHECK(mapping.readOnlyResources[1].bind == 5);
      CHECK(mapping.readOnlyResources[1].arraySize == 1);
      CHECK(mapping.readOnlyResources[1].used);
    }

    REQUIRE_ARRAY_SIZE(mapping.readWriteResources.size(), 2);
    {
      // atom
      CHECK(mapping.readWriteResources[0].bindset == 0);
      CHECK(mapping.readWriteResources[0].bind == 0);
      CHECK(mapping.readWriteResources[0].arraySize == 1);
      CHECK(mapping.readWriteResources[0].used);

      // ssbo
      CHECK(mapping.readWriteResources[1].bindset == 0);
      CHECK(mapping.readWriteResources[1].bind == 2);
      CHECK(mapping.readWriteResources[1].arraySize == 1);
      CHECK(mapping.readWriteResources[1].used);
    }

    REQUIRE_ARRAY_SIZE(mapping.constantBlocks.size(), 2);
    {
      // ubo
      CHECK(mapping.constantBlocks[0].bindset == 0);
      CHECK(mapping.constantBlocks[0].bind == 8);
      CHECK(mapping.constantBlocks[0].arraySize == 1);
      CHECK(mapping.constantBlocks[0].used);

      // $Globals
      CHECK(mapping.constantBlocks[1].bindset == -1);
      CHECK(mapping.constantBlocks[1].bind == -1);
      CHECK(mapping.constantBlocks[1].arraySize == 1);
      CHECK(mapping.constantBlocks[1].used);
    }

    REQUIRE(mapping.samplers.empty());
  };

  SECTION("vertex shader fixed function outputs")
  {
    std::string source = R"(
#version 150 core

void main() {
  gl_Position = vec4(0, 1, 0, 1);
}

)";

    ShaderReflection refl;
    ShaderBindpointMapping mapping;
    MakeOfflineShaderReflection(ShaderStage::Vertex, source, refl, mapping);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 1);
    {
      CHECK(refl.outputSignature[0].varName == "gl_Position");
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }
    }

    std::string source2 = R"(
#version 150 core

void main() {
  gl_Position = vec4(0, 1, 0, 1);
  gl_PointSize = 1.5f;
}

)";

    refl = ShaderReflection();
    mapping = ShaderBindpointMapping();
    MakeOfflineShaderReflection(ShaderStage::Vertex, source2, refl, mapping);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 2);
    {
      CHECK(refl.outputSignature[0].varName == "gl_Position");
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName == "gl_PointSize");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::PointSize);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }
    }
  };

  SECTION("shader input/output blocks")
  {
    std::string source = R"(
#version 420 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

in block
{
	vec2 Texcoord;
} In[];

out gl_PerVertex
{
	vec4 gl_Position;
};

out block
{
	vec2 Texcoord;
} Out;

void main()
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		Out.Texcoord = In[i].Texcoord;
		EmitVertex();
	}
	EndPrimitive();
}

)";

    ShaderReflection refl;
    ShaderBindpointMapping mapping;
    MakeOfflineShaderReflection(ShaderStage::Geometry, source, refl, mapping);

    REQUIRE_ARRAY_SIZE(refl.inputSignature.size(), 2);
    {
      CHECK(refl.inputSignature[0].varName == "block.Texcoord");
      {
        const SigParameter &sig = refl.inputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.inputSignature[1].varName == "gl_PerVertex.gl_Position");
      {
        const SigParameter &sig = refl.inputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }
    }

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 2);
    {
      CHECK(refl.outputSignature[0].varName == "block.Texcoord");
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[1].varName == "gl_Position");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }
    }
  };

  SECTION("matrix and array outputs")
  {
    std::string source = R"(
#version 150 core

out vec3 outarr[3];
out mat2 outmat;

void main()
{
  gl_Position = vec4(0, 0, 0, 1);
  outarr[0] = gl_Position.xyz;
  outarr[1] = gl_Position.xyz;
  outarr[2] = gl_Position.xyz;
  outmat = mat2(0, 0, 0, 0);
}

)";

    ShaderReflection refl;
    ShaderBindpointMapping mapping;
    MakeOfflineShaderReflection(ShaderStage::Vertex, source, refl, mapping);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 6);
    {
      CHECK(refl.outputSignature[0].varName == "outarr[0]");
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.arrayIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[1].varName == "outarr[1]");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 1);
        CHECK(sig.arrayIndex == 1);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[2].varName == "outarr[2]");
      {
        const SigParameter &sig = refl.outputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 2);
        CHECK(sig.arrayIndex == 2);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[3].varName == "outmat:row0");
      {
        const SigParameter &sig = refl.outputSignature[3];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 3);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[4].varName == "outmat:row1");
      {
        const SigParameter &sig = refl.outputSignature[4];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 4);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[5].varName == "gl_Position");
      {
        const SigParameter &sig = refl.outputSignature[5];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.compType == CompType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }
    }
  };

  SECTION("shader stage references")
  {
    std::string vssource = R"(
#version 450 core

uniform float unused_uniform; // declared in both, used in neither
uniform float vsonly_uniform; // declared and used only in VS
uniform float shared_uniform; // declared and used in both
uniform float vsshared_uniform; // declared in both, used only in VS
uniform float fsshared_uniform; // declared in both, used only in FS

out vec4 vsout;

void main() {
  vsout = vec4(vsonly_uniform, shared_uniform, vsshared_uniform, 1);
}

)";

    std::string fssource = R"(
#version 450 core

uniform float unused_uniform; // declared in both, used in neither
uniform float shared_uniform; // declared and used in both
uniform float vsshared_uniform; // declared in both, used only in VS
uniform float fsshared_uniform; // declared in both, used only in FS
uniform float fsonly_uniform; // declared and used only in FS

in vec4 vsout;

out vec4 col;

void main() {
  vec4 tmp = vsout;
  tmp.w = fsonly_uniform * shared_uniform + fsshared_uniform;
  col = tmp;
}

)";

    InitSPIRVCompiler();
    RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);

    // as a hack, create a local 'driver' and just populate m_Programs with what we want.
    GLDummyPlatform dummy;
    WrappedOpenGL driver(dummy);

    GL.DriverForEmulation(&driver);

    RDCEraseEl(HasExt);
    GL = GLDispatchTable();
    GL.EmulateRequiredExtensions();

    glslang::TProgram *prog = LinkProgramForReflection(
        {CompileShaderForReflection(SPIRVShaderStage::Vertex, {vssource}),
         CompileShaderForReflection(SPIRVShaderStage::Fragment, {fssource})});

    REQUIRE(prog);

    // the lookup won't get a valid Id, so set the program to the ResourceId()
    driver.m_Programs[ResourceId()].glslangProgram = prog;

    GLint numUniforms = 0;
    GL.glGetProgramInterfaceiv(0, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &numUniforms);

    for(GLint i = 0; i < numUniforms; i++)
    {
      GLenum props[2] = {eGL_REFERENCED_BY_VERTEX_SHADER, eGL_REFERENCED_BY_FRAGMENT_SHADER};
      GLint values[2] = {};
      GL.glGetProgramResourceiv(0, eGL_UNIFORM, i, 2, props, 2, NULL, values);

      GLchar name[1024] = {};
      GL.glGetProgramResourceName(0, eGL_UNIFORM, i, 1024, NULL, name);

      if(!strcmp(name, "unused_uniform"))
      {
        FAIL_CHECK("Didn't expect to see unused_uniform");
      }
      else if(!strcmp(name, "vsonly_uniform"))
      {
        CHECK(values[0] == 1);
        CHECK(values[1] == 0);
      }
      else if(!strcmp(name, "shared_uniform"))
      {
        CHECK(values[0] == 1);
        CHECK(values[1] == 1);
      }
      else if(!strcmp(name, "vsshared_uniform"))
      {
        CHECK(values[0] == 1);
        CHECK(values[1] == 0);
      }
      else if(!strcmp(name, "fsshared_uniform"))
      {
        CHECK(values[0] == 0);
        CHECK(values[1] == 1);
      }
      else if(!strcmp(name, "fsonly_uniform"))
      {
        CHECK(values[0] == 0);
        CHECK(values[1] == 1);
      }
      else
      {
        INFO("uniform name: " << name);
        FAIL_CHECK("Unexpected uniform");
      }
    }

    RDCEraseEl(HasExt);
    GL = GLDispatchTable();
    GL.DriverForEmulation(NULL);
  };
}

#endif