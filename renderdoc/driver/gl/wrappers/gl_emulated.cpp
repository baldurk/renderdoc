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

namespace glEmulate
{
GLHookSet *hookset = NULL;

void APIENTRY _glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&old);
  hookset->glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, xfb);
  hookset->glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
  hookset->glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, old);
}

void APIENTRY _glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer,
                                              GLintptr offset, GLsizeiptr size)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&old);
  hookset->glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, xfb);
  hookset->glBindBufferRange(eGL_TRANSFORM_FEEDBACK_BUFFER, index, buffer, offset, size);
  hookset->glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, old);
}

void APIENTRY _glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                         const GLint *value)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&old);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferiv(buffer, drawbuffer, value);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, old);
}

void APIENTRY _glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                          const GLuint *value)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&old);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferuiv(buffer, drawbuffer, value);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, old);
}

void APIENTRY _glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer,
                                         const GLfloat *value)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&old);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferfv(buffer, drawbuffer, value);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, old);
}

void APIENTRY _glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, const GLfloat depth,
                                         GLint stencil)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&old);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);
  hookset->glClearBufferfi(buffer, 0, depth, stencil);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, old);
}

void APIENTRY _glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0,
                                      GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                                      GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
  GLuint oldR = 0, oldD = 0;
  hookset->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&oldR);
  hookset->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&oldD);

  hookset->glBindFramebuffer(eGL_READ_FRAMEBUFFER, readFramebuffer);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFramebuffer);

  hookset->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

  hookset->glBindFramebuffer(eGL_READ_FRAMEBUFFER, oldR);
  hookset->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, oldD);
}

void APIENTRY _glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&old);
  hookset->glBindVertexArray(vaobj);
  hookset->glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, buffer);
  hookset->glBindVertexArray(old);
}

void APIENTRY _glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                                          const GLuint *buffers, const GLintptr *offsets,
                                          const GLsizei *strides)
{
  GLuint old = 0;
  hookset->glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&old);
  hookset->glBindVertexArray(vaobj);
  hookset->glBindVertexBuffers(first, count, buffers, offsets, strides);
  hookset->glBindVertexArray(old);
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

};    // namespace glEmulate
