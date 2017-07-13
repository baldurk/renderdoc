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

#include "../gl_driver.h"
#include "common/common.h"
#include "serialise/string_utils.h"

bool WrappedOpenGL::Serialise_glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(FramebufferRes(GetCtx(), *framebuffers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenFramebuffers(1, &real);
    m_Real.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, real);
    m_Real.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    GLResource res = FramebufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
  m_Real.glGenFramebuffers(n, framebuffers);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_FRAMEBUFFERS);
        Serialise_glGenFramebuffers(1, framebuffers + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

bool WrappedOpenGL::Serialise_glCreateFramebuffers(GLsizei n, GLuint *framebuffers)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(FramebufferRes(GetCtx(), *framebuffers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glCreateFramebuffers(1, &real);

    GLResource res = FramebufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);
  }

  return true;
}

void WrappedOpenGL::glCreateFramebuffers(GLsizei n, GLuint *framebuffers)
{
  m_Real.glCreateFramebuffers(n, framebuffers);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_FRAMEBUFFERS);
        Serialise_glCreateFramebuffers(1, framebuffers + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment,
                                                           GLuint texture, GLint level)
{
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State < WRITING)
  {
    GLuint tex = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                     ? 0
                     : GetResourceManager()->GetLiveResource(id).name;
    if(fbid == ResourceId())
    {
      m_Real.glNamedFramebufferTextureEXT(0, Attach, tex, Level);
    }
    else
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferTextureEXT(fbres.name, Attach, tex, Level);
    }

    if(m_State == READING && tex)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment,
                                                 GLuint texture, GLint level)
{
  m_Real.glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
    Serialise_glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
  m_Real.glFramebufferTexture(target, attachment, texture, level);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX);
    Serialise_glNamedFramebufferTextureEXT(record->Resource.name, attachment, texture, level);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment,
                                                             GLenum textarget, GLuint texture,
                                                             GLint level)
{
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State < WRITING)
  {
    GLuint tex = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                     ? 0
                     : GetResourceManager()->GetLiveResource(id).name;
    if(fbid == ResourceId())
    {
      m_Real.glNamedFramebufferTexture1DEXT(0, Attach, TexTarget, tex, Level);
    }
    else
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferTexture1DEXT(fbres.name, Attach, TexTarget, tex, Level);
    }

    if(m_State == READING && tex)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment,
                                                   GLenum textarget, GLuint texture, GLint level)
{
  m_Real.glNamedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX1D);
    Serialise_glNamedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget,
                                           GLuint texture, GLint level)
{
  m_Real.glFramebufferTexture1D(target, attachment, textarget, texture, level);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX1D);
    Serialise_glNamedFramebufferTexture1DEXT(record->Resource.name, attachment, textarget, texture,
                                             level);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment,
                                                             GLenum textarget, GLuint texture,
                                                             GLint level)
{
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State < WRITING)
  {
    GLuint tex = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                     ? 0
                     : GetResourceManager()->GetLiveResource(id).name;
    if(fbid == ResourceId())
    {
      m_Real.glNamedFramebufferTexture2DEXT(0, Attach, TexTarget, tex, Level);
    }
    else
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferTexture2DEXT(fbres.name, Attach, TexTarget, tex, Level);
    }

    if(m_State == READING && tex)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment,
                                                   GLenum textarget, GLuint texture, GLint level)
{
  m_Real.glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
    Serialise_glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                                           GLuint texture, GLint level)
{
  m_Real.glFramebufferTexture2D(target, attachment, textarget, texture, level);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2D);
    Serialise_glNamedFramebufferTexture2DEXT(record->Resource.name, attachment, textarget, texture,
                                             level);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glFramebufferTexture2DMultisampleEXT(GLuint framebuffer,
                                                                   GLenum target, GLenum attachment,
                                                                   GLenum textarget, GLuint texture,
                                                                   GLint level, GLsizei samples)

{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(uint32_t, Samples, samples);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State < WRITING)
  {
    GLuint tex = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                     ? 0
                     : GetResourceManager()->GetLiveResource(id).name;

    GLuint prevread = 0, prevdraw = 0;
    m_Real.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
    m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

    GLuint fbbinding = (fbid == ResourceId()) ? 0 : GetResourceManager()->GetLiveResource(fbid).name;
    m_Real.glBindFramebuffer(Target, fbbinding);

    m_Real.glFramebufferTexture2DMultisampleEXT(Target, Attach, TexTarget, tex, Level, Samples);

    m_Real.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
    m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);

    if(m_State == READING && tex)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glFramebufferTexture2DMultisampleEXT(GLenum target, GLenum attachment,
                                                         GLenum textarget, GLuint texture,
                                                         GLint level, GLsizei samples)
{
  m_Real.glFramebufferTexture2DMultisampleEXT(target, attachment, textarget, texture, level, samples);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX2DMS);
    Serialise_glFramebufferTexture2DMultisampleEXT(record->Resource.name, target, attachment,
                                                   textarget, texture, level, samples);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment,
                                                             GLenum textarget, GLuint texture,
                                                             GLint level, GLint zoffset)
{
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(GLenum, TexTarget, textarget);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, Zoffset, zoffset);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State < WRITING)
  {
    GLuint tex = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                     ? 0
                     : GetResourceManager()->GetLiveResource(id).name;
    if(fbid == ResourceId())
    {
      m_Real.glNamedFramebufferTexture3DEXT(0, Attach, TexTarget, tex, Level, Zoffset);
    }
    else
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferTexture3DEXT(fbres.name, Attach, TexTarget, tex, Level, Zoffset);
    }

    if(m_State == READING && tex)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment,
                                                   GLenum textarget, GLuint texture, GLint level,
                                                   GLint zoffset)
{
  m_Real.glNamedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level, zoffset);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX3D);
    Serialise_glNamedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture, level,
                                             zoffset);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget,
                                           GLuint texture, GLint level, GLint zoffset)
{
  m_Real.glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEX3D);
    Serialise_glNamedFramebufferTexture3DEXT(record->Resource.name, attachment, textarget, texture,
                                             level, zoffset);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment,
                                                                GLenum renderbuffertarget,
                                                                GLuint renderbuffer)
{
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(GLenum, RendBufTarget, renderbuffertarget);
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer)));

  if(m_State < WRITING)
  {
    GLuint rb = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                    ? 0
                    : GetResourceManager()->GetLiveResource(id).name;
    if(fbid == ResourceId())
    {
      m_Real.glNamedFramebufferRenderbufferEXT(0, Attach, RendBufTarget, rb);
    }
    else
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferRenderbufferEXT(fbres.name, Attach, RendBufTarget, rb);
    }

    if(m_State == READING && rb)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment,
                                                      GLenum renderbuffertarget, GLuint renderbuffer)
{
  m_Real.glNamedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget, renderbuffer);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_RENDBUF);
    Serialise_glNamedFramebufferRenderbufferEXT(framebuffer, attachment, renderbuffertarget,
                                                renderbuffer);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(RenderbufferRes(GetCtx(), renderbuffer),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                              GLenum renderbuffertarget, GLuint renderbuffer)
{
  m_Real.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_RENDBUF);
    Serialise_glNamedFramebufferRenderbufferEXT(record->Resource.name, attachment,
                                                renderbuffertarget, renderbuffer);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(RenderbufferRes(GetCtx(), renderbuffer),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferTextureLayerEXT(GLuint framebuffer,
                                                                GLenum attachment, GLuint texture,
                                                                GLint level, GLint layer)
{
  SERIALISE_ELEMENT(GLenum, Attach, attachment);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, Layer, layer);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State < WRITING)
  {
    GLuint tex = (id == ResourceId() || !GetResourceManager()->HasLiveResource(id))
                     ? 0
                     : GetResourceManager()->GetLiveResource(id).name;
    if(fbid == ResourceId())
    {
      m_Real.glNamedFramebufferTextureLayerEXT(0, Attach, tex, Level, Layer);
    }
    else
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferTextureLayerEXT(fbres.name, Attach, tex, Level, Layer);
    }

    if(m_State == READING && tex)
    {
      m_Textures[GetResourceManager()->GetLiveID(id)].creationFlags |= TextureCategory::ColorTarget;
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment,
                                                      GLuint texture, GLint level, GLint layer)
{
  m_Real.glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
    Serialise_glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 10)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                                              GLint level, GLint layer)
{
  m_Real.glFramebufferTextureLayer(target, attachment, texture, level, layer);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = m_DeviceRecord;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      ResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
      if(m_State == WRITING_IDLE)
        GetResourceManager()->MarkDirtyResource(texrecord->GetResourceID());
      else
        m_MissingTracks.insert(texrecord->GetResourceID());
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_TEXLAYER);
    Serialise_glNamedFramebufferTextureLayerEXT(record->Resource.name, attachment, texture, level,
                                                layer);

    if(m_State == WRITING_IDLE)
    {
      record->AddChunk(scope.Get());

      if(record != m_DeviceRecord)
      {
        record->UpdateCount++;

        if(record->UpdateCount > 10)
        {
          m_HighTrafficResources.insert(record->GetResourceID());
          GetResourceManager()->MarkDirtyResource(record->GetResourceID());
        }
      }
    }
    else
    {
      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

bool WrappedOpenGL::Serialise_glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname,
                                                              GLint param)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(int32_t, Param, param);
  SERIALISE_ELEMENT(ResourceId, fbid,
                    (framebuffer == 0 ? ResourceId() : GetResourceManager()->GetID(
                                                           FramebufferRes(GetCtx(), framebuffer))));

  if(m_State == READING)
  {
    if(fbid != ResourceId())
    {
      GLResource fbres = GetResourceManager()->GetLiveResource(fbid);
      m_Real.glNamedFramebufferParameteriEXT(fbres.name, PName, Param);
    }
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param)
{
  m_Real.glNamedFramebufferParameteriEXT(framebuffer, pname, param);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_PARAM);
    Serialise_glNamedFramebufferParameteriEXT(framebuffer, pname, param);

    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
  m_Real.glFramebufferParameteri(target, pname, param);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = NULL;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(record == NULL)
      return;

    SCOPED_SERIALISE_CONTEXT(FRAMEBUFFER_PARAM);
    Serialise_glNamedFramebufferParameteriEXT(record->Resource.name, pname, param);

    record->AddChunk(scope.Get());
  }
}

bool WrappedOpenGL::Serialise_glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));
  SERIALISE_ELEMENT(GLenum, b, mode);

  if(m_State < WRITING)
  {
    if(Id == ResourceId())
    {
      // since we are faking the default framebuffer with our own
      // to see the results, replace back/front/left/right with color attachment 0
      if(b == eGL_BACK_LEFT || b == eGL_BACK_RIGHT || b == eGL_BACK || b == eGL_FRONT_LEFT ||
         b == eGL_FRONT_RIGHT || b == eGL_FRONT)
        b = eGL_COLOR_ATTACHMENT0;

      m_Real.glFramebufferReadBufferEXT(m_FakeBB_FBO, b);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(Id);
      m_Real.glFramebufferReadBufferEXT(res.name, b);
    }
  }

  return true;
}

void WrappedOpenGL::glFramebufferReadBufferEXT(GLuint framebuffer, GLenum buf)
{
  m_Real.glFramebufferReadBufferEXT(framebuffer, buf);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(READ_BUFFER);
    Serialise_glFramebufferReadBufferEXT(framebuffer, buf);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
  else if(m_State == WRITING_IDLE && framebuffer != 0)
  {
    SCOPED_SERIALISE_CONTEXT(READ_BUFFER);
    Serialise_glFramebufferReadBufferEXT(framebuffer, buf);

    ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glReadBuffer(GLenum mode)
{
  if(m_State >= WRITING)
  {
    GLResourceRecord *readrecord = GetCtxData().m_ReadFramebufferRecord;
    if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(READ_BUFFER);
      Serialise_glFramebufferReadBufferEXT(readrecord ? readrecord->Resource.name : 0, mode);

      m_ContextRecord->AddChunk(scope.Get());
      if(readrecord)
        GetResourceManager()->MarkFBOReferenced(readrecord->Resource, eFrameRef_ReadBeforeWrite);
    }
    else
    {
      if(readrecord)
        GetResourceManager()->MarkDirtyResource(readrecord->GetResourceID());
    }
  }

  m_Real.glReadBuffer(mode);
}

bool WrappedOpenGL::Serialise_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));

  if(m_State <= EXECUTING)
  {
    if(Id == ResourceId())
    {
      m_Real.glBindFramebuffer(Target, m_FakeBB_FBO);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(Id);
      m_Real.glBindFramebuffer(Target, res.name);
    }
  }

  return true;
}

void WrappedOpenGL::glBindFramebuffer(GLenum target, GLuint framebuffer)
{
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BIND_FRAMEBUFFER);
    Serialise_glBindFramebuffer(target, framebuffer);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }

  if(framebuffer == 0 && m_State < WRITING)
    framebuffer = m_FakeBB_FBO;

  if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    GetCtxData().m_DrawFramebufferRecord =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
  else
    GetCtxData().m_ReadFramebufferRecord =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

  m_Real.glBindFramebuffer(target, framebuffer);
}

bool WrappedOpenGL::Serialise_glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum buf)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    (framebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer))
                                 : ResourceId()));
  SERIALISE_ELEMENT(GLenum, b, buf);

  if(m_State < WRITING)
  {
    if(Id == ResourceId())
    {
      // since we are faking the default framebuffer with our own
      // to see the results, replace back/front/left/right with color attachment 0
      if(b == eGL_BACK_LEFT || b == eGL_BACK_RIGHT || b == eGL_BACK || b == eGL_FRONT_LEFT ||
         b == eGL_FRONT_RIGHT || b == eGL_FRONT)
        b = eGL_COLOR_ATTACHMENT0;

      m_Real.glFramebufferDrawBufferEXT(m_FakeBB_FBO, b);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(Id);
      m_Real.glFramebufferDrawBufferEXT(res.name, b);
    }
  }

  return true;
}

void WrappedOpenGL::glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum buf)
{
  m_Real.glFramebufferDrawBufferEXT(framebuffer, buf);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_BUFFER);
    Serialise_glFramebufferDrawBufferEXT(framebuffer, buf);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
  else if(m_State == WRITING_IDLE && framebuffer != 0)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_BUFFER);
    Serialise_glFramebufferDrawBufferEXT(framebuffer, buf);

    ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDrawBuffer(GLenum buf)
{
  if(m_State >= WRITING)
  {
    GLResourceRecord *drawrecord = GetCtxData().m_DrawFramebufferRecord;
    if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(DRAW_BUFFER);
      Serialise_glFramebufferDrawBufferEXT(drawrecord ? drawrecord->Resource.name : 0, buf);

      m_ContextRecord->AddChunk(scope.Get());
      if(drawrecord)
        GetResourceManager()->MarkFBOReferenced(drawrecord->Resource, eFrameRef_ReadBeforeWrite);
    }
    else
    {
      if(drawrecord)
        GetResourceManager()->MarkDirtyResource(drawrecord->GetResourceID());
    }
  }

  m_Real.glDrawBuffer(buf);
}

bool WrappedOpenGL::Serialise_glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n,
                                                          const GLenum *bufs)
{
  SERIALISE_ELEMENT(ResourceId, Id,
                    GetResourceManager()->GetID(FramebufferRes(GetCtx(), framebuffer)));
  SERIALISE_ELEMENT(uint32_t, num, n);
  SERIALISE_ELEMENT_ARR(GLenum, buffers, bufs, num);

  if(m_State < WRITING)
  {
    for(uint32_t i = 0; i < num; i++)
    {
      // since we are faking the default framebuffer with our own
      // to see the results, replace back/front/left/right with color attachment 0
      if(buffers[i] == eGL_BACK_LEFT || buffers[i] == eGL_BACK_RIGHT || buffers[i] == eGL_BACK ||
         buffers[i] == eGL_FRONT_LEFT || buffers[i] == eGL_FRONT_RIGHT || buffers[i] == eGL_FRONT)
        buffers[i] = eGL_COLOR_ATTACHMENT0;
    }

    m_Real.glFramebufferDrawBuffersEXT(GetResourceManager()->GetLiveResource(Id).name, num, buffers);
  }

  delete[] buffers;

  return true;
}

void WrappedOpenGL::glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
  m_Real.glFramebufferDrawBuffersEXT(framebuffer, n, bufs);

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
    Serialise_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
  else if(m_State == WRITING_IDLE && framebuffer != 0)
  {
    SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
    Serialise_glFramebufferDrawBuffersEXT(framebuffer, n, bufs);

    ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDrawBuffers(GLsizei n, const GLenum *bufs)
{
  if(m_State >= WRITING)
  {
    GLResourceRecord *drawrecord = GetCtxData().m_DrawFramebufferRecord;
    if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(DRAW_BUFFERS);
      if(drawrecord)
        Serialise_glFramebufferDrawBuffersEXT(drawrecord->Resource.name, n, bufs);
      else
        Serialise_glFramebufferDrawBuffersEXT(0, n, bufs);

      m_ContextRecord->AddChunk(scope.Get());
      if(drawrecord)
        GetResourceManager()->MarkFBOReferenced(drawrecord->Resource, eFrameRef_ReadBeforeWrite);
    }
    else
    {
      if(drawrecord)
        GetResourceManager()->MarkDirtyResource(drawrecord->GetResourceID());
    }
  }

  m_Real.glDrawBuffers(n, bufs);
}

void WrappedOpenGL::glInvalidateFramebuffer(GLenum target, GLsizei numAttachments,
                                            const GLenum *attachments)
{
  m_Real.glInvalidateFramebuffer(target, numAttachments, attachments);

  if(m_State == WRITING_IDLE)
  {
    GLResourceRecord *record = NULL;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(record)
    {
      record->MarkParentsDirty(GetResourceManager());
    }
  }
}

// note: glDiscardFramebufferEXT looks the same as glInvalidateFramebuffer. Could this be a plain
// alias?
void WrappedOpenGL::glDiscardFramebufferEXT(GLenum target, GLsizei numAttachments,
                                            const GLenum *attachments)
{
  m_Real.glDiscardFramebufferEXT(target, numAttachments, attachments);

  if(m_State == WRITING_IDLE)
  {
    GLResourceRecord *record = NULL;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(record)
    {
      record->MarkParentsDirty(GetResourceManager());
    }
  }
}

void WrappedOpenGL::glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments,
                                                     const GLenum *attachments)
{
  m_Real.glInvalidateNamedFramebufferData(framebuffer, numAttachments, attachments);

  if(m_State == WRITING_IDLE)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(record)
      record->MarkParentsDirty(GetResourceManager());
  }
}

void WrappedOpenGL::glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments,
                                               const GLenum *attachments, GLint x, GLint y,
                                               GLsizei width, GLsizei height)
{
  m_Real.glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);

  if(m_State == WRITING_IDLE)
  {
    GLResourceRecord *record = NULL;

    if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    {
      if(GetCtxData().m_DrawFramebufferRecord)
        record = GetCtxData().m_DrawFramebufferRecord;
    }
    else
    {
      if(GetCtxData().m_ReadFramebufferRecord)
        record = GetCtxData().m_ReadFramebufferRecord;
    }

    if(record)
    {
      record->MarkParentsDirty(GetResourceManager());
    }
  }
}

void WrappedOpenGL::glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments,
                                                        const GLenum *attachments, GLint x, GLint y,
                                                        GLsizei width, GLsizei height)
{
  m_Real.glInvalidateNamedFramebufferSubData(framebuffer, numAttachments, attachments, x, y, width,
                                             height);

  if(m_State == WRITING_IDLE)
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(record)
      record->MarkParentsDirty(GetResourceManager());
  }
}

bool WrappedOpenGL::Serialise_glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer,
                                                     GLint srcX0, GLint srcY0, GLint srcX1,
                                                     GLint srcY1, GLint dstX0, GLint dstY0,
                                                     GLint dstX1, GLint dstY1, GLbitfield mask,
                                                     GLenum filter)
{
  SERIALISE_ELEMENT(
      ResourceId, readId,
      (readFramebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), readFramebuffer))
                       : ResourceId()));
  SERIALISE_ELEMENT(
      ResourceId, drawId,
      (drawFramebuffer ? GetResourceManager()->GetID(FramebufferRes(GetCtx(), drawFramebuffer))
                       : ResourceId()));
  SERIALISE_ELEMENT(int32_t, sX0, srcX0);
  SERIALISE_ELEMENT(int32_t, sY0, srcY0);
  SERIALISE_ELEMENT(int32_t, sX1, srcX1);
  SERIALISE_ELEMENT(int32_t, sY1, srcY1);
  SERIALISE_ELEMENT(int32_t, dX0, dstX0);
  SERIALISE_ELEMENT(int32_t, dY0, dstY0);
  SERIALISE_ELEMENT(int32_t, dX1, dstX1);
  SERIALISE_ELEMENT(int32_t, dY1, dstY1);
  SERIALISE_ELEMENT(uint32_t, msk, mask);
  SERIALISE_ELEMENT(GLenum, flt, filter);

  if(m_State <= EXECUTING)
  {
    if(readId == ResourceId())
      readFramebuffer = m_FakeBB_FBO;
    else
      readFramebuffer = GetResourceManager()->GetLiveResource(readId).name;

    if(drawId == ResourceId())
      drawFramebuffer = m_FakeBB_FBO;
    else
      drawFramebuffer = GetResourceManager()->GetLiveResource(drawId).name;

    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is
    // necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and
    // we need to support this case.
    m_Real.glBlitNamedFramebuffer(readFramebuffer, drawFramebuffer, sX0, sY0, sX1, sY1, dX0, dY0,
                                  dX1, dY1, msk, flt);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glBlitFramebuffer(" + ToStr::Get(readId) + ", " + ToStr::Get(drawId) + ")";

    DrawcallDescription draw;
    draw.name = name;
    draw.flags |= DrawFlags::Resolve;

    GLint numCols = 8;
    m_Real.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

    for(int i = 0; i < numCols + 2; i++)
    {
      GLenum attachName = GLenum(eGL_COLOR_ATTACHMENT0 + i);
      if(i == numCols)
        attachName = eGL_DEPTH_ATTACHMENT;
      if(i == numCols + 1)
        attachName = eGL_STENCIL_ATTACHMENT;

      GLuint srcattachment = 0, dstattachment = 0;
      GLenum srctype = eGL_TEXTURE, dsttype = eGL_TEXTURE;

      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(readFramebuffer, attachName,
                                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                           (GLint *)&srcattachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          readFramebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&srctype);

      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(drawFramebuffer, attachName,
                                                           eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                           (GLint *)&dstattachment);
      m_Real.glGetNamedFramebufferAttachmentParameterivEXT(
          drawFramebuffer, attachName, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&dsttype);

      ResourceId srcid, dstid;

      if(srctype == eGL_TEXTURE)
        srcid = GetResourceManager()->GetID(TextureRes(GetCtx(), srcattachment));
      else
        srcid = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), srcattachment));

      if(dsttype == eGL_TEXTURE)
        dstid = GetResourceManager()->GetID(TextureRes(GetCtx(), dstattachment));
      else
        dstid = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), dstattachment));

      if(msk & GL_COLOR_BUFFER_BIT)
      {
        if(attachName == eGL_COLOR_ATTACHMENT0)
        {
          draw.copySource = GetResourceManager()->GetOriginalID(srcid);
          draw.copyDestination = GetResourceManager()->GetOriginalID(dstid);
        }
      }
      else
      {
        if(attachName == eGL_DEPTH_ATTACHMENT)
        {
          draw.copySource = GetResourceManager()->GetOriginalID(srcid);
          draw.copyDestination = GetResourceManager()->GetOriginalID(dstid);
        }
      }

      if(dstattachment == srcattachment && srctype == dsttype)
      {
        m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::Copy));
      }
      else
      {
        // MS to non-MS is a resolve
        if((m_Textures[srcid].curType == eGL_TEXTURE_2D_MULTISAMPLE ||
            m_Textures[srcid].curType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY) &&
           m_Textures[dstid].curType != eGL_TEXTURE_2D_MULTISAMPLE &&
           m_Textures[dstid].curType != eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
          m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::ResolveSrc));
          m_ResourceUses[dstid].push_back(EventUsage(m_CurEventID, ResourceUsage::ResolveDst));
        }
        else
        {
          m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::CopySrc));
          m_ResourceUses[dstid].push_back(EventUsage(m_CurEventID, ResourceUsage::CopyDst));
        }
      }
    }

    AddDrawcall(draw, true);
  }

  return true;
}

void WrappedOpenGL::glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer,
                                           GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                           GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                           GLbitfield mask, GLenum filter)
{
  CoherentMapImplicitBarrier();

  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(BLIT_FRAMEBUFFER);
    Serialise_glBlitNamedFramebuffer(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1,
                                     dstX0, dstY0, dstX1, dstY1, mask, filter);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), readFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), drawFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }

  // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
  // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
  // is
  // necessary since these functions can be serialised even if ARB_dsa was not used originally, and
  // we need to support this case.
  m_Real.glBlitNamedFramebuffer(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0,
                                dstY0, dstX1, dstY1, mask, filter);
}

void WrappedOpenGL::glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                      GLbitfield mask, GLenum filter)
{
  CoherentMapImplicitBarrier();

  if(m_State == WRITING_CAPFRAME)
  {
    GLuint readFramebuffer = 0, drawFramebuffer = 0;

    if(GetCtxData().m_ReadFramebufferRecord)
      readFramebuffer = GetCtxData().m_ReadFramebufferRecord->Resource.name;
    if(GetCtxData().m_DrawFramebufferRecord)
      drawFramebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

    SCOPED_SERIALISE_CONTEXT(BLIT_FRAMEBUFFER);
    Serialise_glBlitNamedFramebuffer(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1,
                                     dstX0, dstY0, dstX1, dstY1, mask, filter);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), readFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), drawFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }

  m_Real.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void WrappedOpenGL::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      GetResourceManager()->MarkCleanResource(res);
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteFramebuffers(n, framebuffers);
}

bool WrappedOpenGL::Serialise_glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(RenderbufferRes(GetCtx(), *renderbuffers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenRenderbuffers(1, &real);
    m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, real);

    GLResource res = RenderbufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_RENDERBUFFER;
  }

  return true;
}

void WrappedOpenGL::glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  m_Real.glGenRenderbuffers(n, renderbuffers);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_RENDERBUFFERS);
        Serialise_glGenRenderbuffers(1, renderbuffers + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

bool WrappedOpenGL::Serialise_glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetResourceManager()->GetID(RenderbufferRes(GetCtx(), *renderbuffers)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glCreateRenderbuffers(1, &real);
    m_Real.glBindRenderbuffer(eGL_RENDERBUFFER, real);

    GLResource res = RenderbufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_RENDERBUFFER;
  }

  return true;
}

void WrappedOpenGL::glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  m_Real.glCreateRenderbuffers(n, renderbuffers);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(CREATE_RENDERBUFFERS);
        Serialise_glCreateRenderbuffers(1, renderbuffers + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
    }
  }
}

void WrappedOpenGL::glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  // don't need to serialise this, as the GL_RENDERBUFFER target does nothing
  // aside from create names (after glGen), and provide as a selector for glRenderbufferStorage*
  // which we do ourselves. We just need to know the current renderbuffer ID
  GetCtxData().m_Renderbuffer = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer));

  m_Real.glBindRenderbuffer(target, renderbuffer);
}

void WrappedOpenGL::glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      GetResourceManager()->MarkCleanResource(res);
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteRenderbuffers(n, renderbuffers);
}

bool WrappedOpenGL::Serialise_glNamedRenderbufferStorageEXT(GLuint renderbuffer,
                                                            GLenum internalformat, GLsizei width,
                                                            GLsizei height)
{
  SERIALISE_ELEMENT(
      ResourceId, id,
      (renderbuffer ? GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer))
                    : ResourceId()));
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);

  if(m_State == READING)
  {
    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    TextureData &texDetails = m_Textures[liveId];

    texDetails.width = Width;
    texDetails.height = Height;
    texDetails.depth = 1;
    texDetails.samples = 1;
    texDetails.curType = eGL_RENDERBUFFER;
    texDetails.internalFormat = Format;

    GLuint real = GetResourceManager()->GetLiveResource(id).name;

    m_Real.glNamedRenderbufferStorageEXT(real, Format, Width, Height);

    // create read-from texture for displaying this render buffer
    m_Real.glGenTextures(1, &texDetails.renderbufferReadTex);
    m_Real.glBindTexture(eGL_TEXTURE_2D, texDetails.renderbufferReadTex);
    m_Real.glTexImage2D(eGL_TEXTURE_2D, 0, Format, Width, Height, 0, GetBaseFormat(Format),
                        GetDataType(Format), NULL);
    m_Real.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
    m_Real.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
    m_Real.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR);

    m_Real.glGenFramebuffers(2, texDetails.renderbufferFBOs);
    m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
    m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);

    GLenum fmt = GetBaseFormat(Format);

    GLenum attach = eGL_COLOR_ATTACHMENT0;
    if(fmt == eGL_DEPTH_COMPONENT)
      attach = eGL_DEPTH_ATTACHMENT;
    if(fmt == eGL_STENCIL)
      attach = eGL_STENCIL_ATTACHMENT;
    if(fmt == eGL_DEPTH_STENCIL)
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
    m_Real.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach,
                                             eGL_RENDERBUFFER, real);
    m_Real.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach, eGL_TEXTURE_2D,
                                          texDetails.renderbufferReadTex, 0);
  }

  return true;
}

void WrappedOpenGL::glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat,
                                                  GLsizei width, GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, eGL_RENDERBUFFER, internalformat);

  m_Real.glNamedRenderbufferStorageEXT(renderbuffer, internalformat, width, height);

  ResourceId rb = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer));

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 renderbuffer);

    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(RENDERBUFFER_STORAGE);
      Serialise_glNamedRenderbufferStorageEXT(record->Resource.name, internalformat, width, height);

      record->AddChunk(scope.Get());
    }
  }

  {
    m_Textures[rb].width = width;
    m_Textures[rb].height = height;
    m_Textures[rb].depth = 1;
    m_Textures[rb].samples = 1;
    m_Textures[rb].curType = eGL_RENDERBUFFER;
    m_Textures[rb].dimension = 2;
    m_Textures[rb].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width,
                                          GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, eGL_RENDERBUFFER, internalformat);

  m_Real.glRenderbufferStorage(target, internalformat, width, height);

  ResourceId rb = GetCtxData().m_Renderbuffer;

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify implicit renderbuffer. Not bound?", record);

    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(RENDERBUFFER_STORAGE);
      Serialise_glNamedRenderbufferStorageEXT(record->Resource.name, internalformat, width, height);

      record->AddChunk(scope.Get());
    }
  }

  {
    m_Textures[rb].width = width;
    m_Textures[rb].height = height;
    m_Textures[rb].depth = 1;
    m_Textures[rb].samples = 1;
    m_Textures[rb].curType = eGL_RENDERBUFFER;
    m_Textures[rb].dimension = 2;
    m_Textures[rb].internalFormat = internalformat;
  }
}

bool WrappedOpenGL::Serialise_glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer,
                                                                       GLsizei samples,
                                                                       GLenum internalformat,
                                                                       GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Samples, samples);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(
      ResourceId, id,
      (renderbuffer ? GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer))
                    : ResourceId()));

  if(m_State == READING)
  {
    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    TextureData &texDetails = m_Textures[liveId];

    texDetails.width = Width;
    texDetails.height = Height;
    texDetails.depth = 1;
    texDetails.samples = Samples;
    texDetails.curType = eGL_RENDERBUFFER;
    texDetails.internalFormat = Format;

    GLuint real = GetResourceManager()->GetLiveResource(id).name;

    m_Real.glNamedRenderbufferStorageMultisampleEXT(real, Samples, Format, Width, Height);

    // create read-from texture for displaying this render buffer
    m_Real.glGenTextures(1, &texDetails.renderbufferReadTex);
    m_Real.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex);
    m_Real.glTextureStorage2DMultisampleEXT(texDetails.renderbufferReadTex,
                                            eGL_TEXTURE_2D_MULTISAMPLE, Samples, Format, Width,
                                            Height, true);

    m_Real.glGenFramebuffers(2, texDetails.renderbufferFBOs);
    m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
    m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);

    GLenum fmt = GetBaseFormat(Format);

    GLenum attach = eGL_COLOR_ATTACHMENT0;
    if(fmt == eGL_DEPTH_COMPONENT)
      attach = eGL_DEPTH_ATTACHMENT;
    if(fmt == eGL_STENCIL)
      attach = eGL_STENCIL_ATTACHMENT;
    if(fmt == eGL_DEPTH_STENCIL)
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
    m_Real.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach,
                                             eGL_RENDERBUFFER, real);
    m_Real.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach,
                                          eGL_TEXTURE_2D_MULTISAMPLE,
                                          texDetails.renderbufferReadTex, 0);
  }

  return true;
}

void WrappedOpenGL::glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples,
                                                             GLenum internalformat, GLsizei width,
                                                             GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, eGL_RENDERBUFFER, internalformat);

  m_Real.glNamedRenderbufferStorageMultisampleEXT(renderbuffer, samples, internalformat, width,
                                                  height);

  ResourceId rb = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer));

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 renderbuffer);

    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(RENDERBUFFER_STORAGEMS);
      Serialise_glNamedRenderbufferStorageMultisampleEXT(record->Resource.name, samples,
                                                         internalformat, width, height);

      record->AddChunk(scope.Get());
    }
  }

  {
    m_Textures[rb].width = width;
    m_Textures[rb].height = height;
    m_Textures[rb].depth = 1;
    m_Textures[rb].samples = samples;
    m_Textures[rb].curType = eGL_RENDERBUFFER;
    m_Textures[rb].dimension = 2;
    m_Textures[rb].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glRenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                                     GLenum internalformat, GLsizei width,
                                                     GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, eGL_RENDERBUFFER, internalformat);

  m_Real.glRenderbufferStorageMultisample(target, samples, internalformat, width, height);

  ResourceId rb = GetCtxData().m_Renderbuffer;

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify implicit renderbuffer. Not bound?", record);

    if(record)
    {
      SCOPED_SERIALISE_CONTEXT(RENDERBUFFER_STORAGEMS);
      Serialise_glNamedRenderbufferStorageMultisampleEXT(record->Resource.name, samples,
                                                         internalformat, width, height);

      record->AddChunk(scope.Get());
    }
  }

  {
    m_Textures[rb].width = width;
    m_Textures[rb].height = height;
    m_Textures[rb].depth = 1;
    m_Textures[rb].samples = samples;
    m_Textures[rb].curType = eGL_RENDERBUFFER;
    m_Textures[rb].dimension = 2;
    m_Textures[rb].internalFormat = internalformat;
  }
}
