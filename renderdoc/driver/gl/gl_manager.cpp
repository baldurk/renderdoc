/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "gl_manager.h"
#include <algorithm>
#include "gl_driver.h"

GLResourceManager::GLResourceManager(CaptureState &state, WrappedOpenGL *driver)
    : ResourceManager(state), m_Driver(driver), m_SyncName(1)
{
}

bool GLResourceManager::IsResourceTrackedForPersistency(const GLResource &res)
{
  return res.Namespace == eResTexture || res.Namespace == eResBuffer;
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

  rdcpair<ResourceId, GLResourceRecord *> &it = m_CurrentResources[res];

  MarkResourceFrameReferenced(it.first, ref);

  MarkFBOAttachmentsReferenced(it.first, it.second, ref, false);
}

void GLResourceManager::MarkFBODirtyWithWriteReference(GLResourceRecord *record)
{
  MarkFBOAttachmentsReferenced(record->GetResourceID(), record, eFrameRef_ReadBeforeWrite, true);
}

void GLResourceManager::MarkFBOAttachmentsReferenced(ResourceId fboid, GLResourceRecord *record,
                                                     FrameRefType ref, bool markDirty)
{
  FBOCache *cache = m_FBOAttachmentsCache[fboid];

  if(!cache)
  {
    cache = m_FBOAttachmentsCache[fboid] = new FBOCache;
    // ensure the starting age is wrong, for whatever age we have
    cache->age = (record->age ^ 1);
  }

  if(cache->age != record->age)
  {
    cache->age = record->age;
    cache->attachments.clear();

    GLuint fbo = record->Resource.name;

    ContextPair &ctx = m_Driver->GetCtx();

    GLint numCols = 8;
    GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

    GLenum type = eGL_TEXTURE;
    GLuint name = 0;

    ResourceId id;

    for(int c = 0; c < numCols; c++)
    {
      GL.glGetNamedFramebufferAttachmentParameterivEXT(fbo, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                       eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                       (GLint *)&name);

      if(name)
      {
        GL.glGetNamedFramebufferAttachmentParameterivEXT(fbo, GLenum(eGL_COLOR_ATTACHMENT0 + c),
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                         (GLint *)&type);

        if(type == eGL_RENDERBUFFER)
          id = GetResID(RenderbufferRes(ctx, name));
        else
          id = GetResID(TextureRes(ctx, name));

        cache->attachments.push_back(id);
        GLResourceRecord *r = GetResourceRecord(id);
        if(r && r->viewSource != ResourceId())
          cache->attachments.push_back(r->viewSource);
      }
    }

    GL.glGetNamedFramebufferAttachmentParameterivEXT(
        fbo, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&name);

    if(name)
    {
      GL.glGetNamedFramebufferAttachmentParameterivEXT(
          fbo, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      if(type == eGL_RENDERBUFFER)
        id = GetResID(RenderbufferRes(ctx, name));
      else
        id = GetResID(TextureRes(ctx, name));

      cache->attachments.push_back(id);
      GLResourceRecord *r = GetResourceRecord(id);
      if(r && r->viewSource != ResourceId())
        cache->attachments.push_back(r->viewSource);
    }

    GLuint stencilName = 0;

    GL.glGetNamedFramebufferAttachmentParameterivEXT(
        fbo, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&stencilName);

    if(stencilName && stencilName != name)
    {
      GL.glGetNamedFramebufferAttachmentParameterivEXT(
          fbo, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);

      if(type == eGL_RENDERBUFFER)
        id = GetResID(RenderbufferRes(ctx, stencilName));
      else
        id = GetResID(TextureRes(ctx, stencilName));

      cache->attachments.push_back(id);
      GLResourceRecord *r = GetResourceRecord(id);
      if(r && r->viewSource != ResourceId())
        cache->attachments.push_back(r->viewSource);
    }
  }

  // we took care of viewSource above so we can directly call the real thing
  for(ResourceId id : cache->attachments)
  {
    ResourceManager::MarkResourceFrameReferenced(id, ref);
    if(markDirty)
      ResourceManager::MarkDirtyResource(id);
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

  if(res.name)
  {
    ContextPair &ctx = m_Driver->GetCtx();
    if(res.ContextShareGroup == ctx.ctx || res.ContextShareGroup == ctx.shareGroup)
    {
      m_Driver->ReleaseResource(res);
    }
    else if(IsResourceTrackedForPersistency(res))
    {
      ContextShareGroup *contextShareGroup = (ContextShareGroup *)res.ContextShareGroup;

      GLWindowingData oldContextData = m_Driver->m_ActiveContexts[Threading::GetCurrentID()];

      GLWindowingData savedContext;

      if(m_Driver->m_Platform.PushChildContext(oldContextData, contextShareGroup->m_BackDoor,
                                               &savedContext))
      {
        m_Driver->ReleaseResource(res);

        // restore the context
        m_Driver->m_Platform.PopChildContext(oldContextData, contextShareGroup->m_BackDoor,
                                             savedContext);
      }
      else
      {
        m_Driver->QueueResourceRelease(res);
      }
    }
    else
    {
      // queue if we can't use the backdoor
      m_Driver->QueueResourceRelease(res);
    }
  }
  return true;
}
