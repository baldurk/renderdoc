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

#include "driver/gl/gl_manager.h"
#include <algorithm>
#include "driver/gl/gl_driver.h"

void GLResourceManager::MarkVAOReferenced(GLResource res, FrameRefType ref, bool allowFake0)
{
  const GLHookSet &gl = m_GL->GetHookset();

  if(res.name || allowFake0)
  {
    MarkResourceFrameReferenced(res, ref == eFrameRef_Unknown ? eFrameRef_Unknown : eFrameRef_Read);

    GLint numVBufferBindings = 16;
    gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);

    for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
    {
      GLuint buffer = GetBoundVertexBuffer(gl, i);

      MarkResourceFrameReferenced(BufferRes(res.Context, buffer), ref);
    }

    GLuint ibuffer = 0;
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
    MarkResourceFrameReferenced(BufferRes(res.Context, ibuffer), ref);
  }
}

void GLResourceManager::MarkFBOReferenced(GLResource res, FrameRefType ref)
{
  if(res.name == 0)
    return;

  MarkResourceFrameReferenced(res, ref == eFrameRef_Unknown ? eFrameRef_Unknown : eFrameRef_Read);

  const GLHookSet &gl = m_GL->GetHookset();

  GLint numCols = 8;
  gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  GLenum type = eGL_TEXTURE;
  GLuint name = 0;

  for(int c = 0; c < numCols; c++)
  {
    gl.glGetNamedFramebufferAttachmentParameterivEXT(res.name, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&name);
    gl.glGetNamedFramebufferAttachmentParameterivEXT(res.name, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                     (GLint *)&type);

    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }

  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  gl.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(res.Context, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(res.Context, name), ref);
  }
}

bool GLResourceManager::SerialisableResource(ResourceId id, GLResourceRecord *record)
{
  if(id == m_GL->GetContextResourceID())
    return false;
  return true;
}
