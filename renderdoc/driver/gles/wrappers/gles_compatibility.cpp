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
