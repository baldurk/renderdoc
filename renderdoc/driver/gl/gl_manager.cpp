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

#include "driver/gl/gl_manager.h"
#include <algorithm>
#include "driver/gl/gl_driver.h"

GLResourceManager::GLResourceManager(WrappedOpenGL *driver)
    : ResourceManager(), m_Driver(driver), m_SyncName(1)
{
  m_State = m_Driver->GetState();
}

void GLResourceManager::MarkVAOReferenced(GLResource res, FrameRefType ref, bool allowFake0)
{
  if(res.name || allowFake0)
  {
    ContextPair &ctx = m_Driver->GetCtx();

    MarkResourceFrameReferenced(res, ref);

    GLint numVBufferBindings = GetNumVertexBuffers();

    for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
    {
      GLuint buffer = GetBoundVertexBuffer(i);

      MarkResourceFrameReferenced(BufferRes(ctx, buffer),
                                  ref == eFrameRef_None ? eFrameRef_None : eFrameRef_Read);
    }

    GLuint ibuffer = 0;
    GL.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
    MarkResourceFrameReferenced(BufferRes(ctx, ibuffer),
                                ref == eFrameRef_None ? eFrameRef_None : eFrameRef_Read);
  }
}

void GLResourceManager::MarkFBOReferenced(GLResource res, FrameRefType ref)
{
  if(res.name == 0)
    return;

  MarkResourceFrameReferenced(res, ref);

  ContextPair &ctx = m_Driver->GetCtx();

  GLint numCols = 8;
  GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  GLenum type = eGL_TEXTURE;
  GLuint name = 0;

  for(int c = 0; c < numCols; c++)
  {
    GL.glGetNamedFramebufferAttachmentParameterivEXT(res.name, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&name);
    GL.glGetNamedFramebufferAttachmentParameterivEXT(res.name, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                     (GLint *)&type);

    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(ctx, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(ctx, name), ref);
  }

  GL.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  GL.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(ctx, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(ctx, name), ref);
  }

  GL.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);
  GL.glGetNamedFramebufferAttachmentParameterivEXT(
      res.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

  if(name)
  {
    if(type == eGL_RENDERBUFFER)
      MarkResourceFrameReferenced(RenderbufferRes(ctx, name), ref);
    else
      MarkResourceFrameReferenced(TextureRes(ctx, name), ref);
  }
}

void GLResourceManager::SetInternalResource(GLResource res)
{
  if(!RenderDoc::Inst().IsReplayApp())
  {
    GLResourceRecord *record = GetResourceRecord(res);
    if(record)
      record->InternalResource = true;
  }
}

bool GLResourceManager::ResourceTypeRelease(GLResource res)
{
  if(HasCurrentResource(res))
    UnregisterResource(res);

  m_Driver->QueueResourceRelease(res);
  return true;
}
