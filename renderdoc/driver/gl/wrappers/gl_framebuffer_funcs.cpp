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

#include "../gl_driver.h"
#include "common/common.h"
#include "strings/string_utils.h"

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenFramebuffers(SerialiserType &ser, GLsizei n, GLuint *framebuffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(framebuffer,
                          GetResourceManager()->GetID(FramebufferRes(GetCtx(), *framebuffers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenFramebuffers(1, &real);
    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, real);
    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, 0);

    GLResource res = FramebufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(framebuffer, res);

    AddResource(framebuffer, ResourceType::RenderPass, "Framebuffer");
  }

  return true;
}

void WrappedOpenGL::glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
  SERIALISE_TIME_CALL(GL.glGenFramebuffers(n, framebuffers));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenFramebuffers(ser, 1, framebuffers + i);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateFramebuffers(SerialiserType &ser, GLsizei n,
                                                   GLuint *framebuffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(framebuffer,
                          GetResourceManager()->GetID(FramebufferRes(GetCtx(), *framebuffers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateFramebuffers(1, &real);

    GLResource res = FramebufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(framebuffer, res);

    AddResource(framebuffer, ResourceType::RenderPass, "Framebuffer");
  }

  return true;
}

void WrappedOpenGL::glCreateFramebuffers(GLsizei n, GLuint *framebuffers)
{
  SERIALISE_TIME_CALL(GL.glCreateFramebuffers(n, framebuffers));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateFramebuffers(ser, 1, framebuffers + i);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferTextureEXT(SerialiserType &ser,
                                                           GLuint framebufferHandle,
                                                           GLenum attachment, GLuint textureHandle,
                                                           GLint level)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glNamedFramebufferTextureEXT(framebuffer.name, attachment, texture.name, level);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment,
                                                 GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(GL.glNamedFramebufferTextureEXT(framebuffer, attachment, texture, level));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTextureEXT(ser, framebuffer, attachment, texture, level);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTexture(target, attachment, texture, level));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTextureEXT(ser, record->Resource.name, attachment, texture, level);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferTexture1DEXT(SerialiserType &ser,
                                                             GLuint framebufferHandle,
                                                             GLenum attachment, GLenum textarget,
                                                             GLuint textureHandle, GLint level)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT(textarget);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glNamedFramebufferTexture1DEXT(framebuffer.name, attachment, textarget, texture.name, level);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment,
                                                   GLenum textarget, GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(
      GL.glNamedFramebufferTexture1DEXT(framebuffer, attachment, textarget, texture, level));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTexture1DEXT(ser, framebuffer, attachment, textarget, texture, level);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget,
                                           GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTexture1D(target, attachment, textarget, texture, level));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTexture1DEXT(ser, record->Resource.name, attachment, textarget,
                                             texture, level);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferTexture2DEXT(SerialiserType &ser,
                                                             GLuint framebufferHandle,
                                                             GLenum attachment, GLenum textarget,
                                                             GLuint textureHandle, GLint level)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT(textarget);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glNamedFramebufferTexture2DEXT(framebuffer.name, attachment, textarget, texture.name, level);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment,
                                                   GLenum textarget, GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(
      GL.glNamedFramebufferTexture2DEXT(framebuffer, attachment, textarget, texture, level));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTexture2DEXT(ser, framebuffer, attachment, textarget, texture, level);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                                           GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTexture2D(target, attachment, textarget, texture, level));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTexture2DEXT(ser, record->Resource.name, attachment, textarget,
                                             texture, level);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFramebufferTexture2DMultisampleEXT(
    SerialiserType &ser, GLuint framebufferHandle, GLenum target, GLenum attachment,
    GLenum textarget, GLuint textureHandle, GLint level, GLsizei samples)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT(textarget);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(samples);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glFramebufferTexture2DMultisampleEXT);

    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GLuint prevread = 0, prevdraw = 0;
    GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevdraw);
    GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevread);

    GL.glBindFramebuffer(target, framebuffer.name);

    GL.glFramebufferTexture2DMultisampleEXT(target, attachment, textarget, texture.name, level,
                                            samples);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevdraw);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevread);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glFramebufferTexture2DMultisampleEXT(GLenum target, GLenum attachment,
                                                         GLenum textarget, GLuint texture,
                                                         GLint level, GLsizei samples)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTexture2DMultisampleEXT(target, attachment, textarget,
                                                              texture, level, samples));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferTexture2DMultisampleEXT(ser, record->Resource.name, target, attachment,
                                                   textarget, texture, level, samples);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferTexture3DEXT(SerialiserType &ser,
                                                             GLuint framebufferHandle,
                                                             GLenum attachment, GLenum textarget,
                                                             GLuint textureHandle, GLint level,
                                                             GLint zoffset)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT(textarget);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(zoffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glNamedFramebufferTexture3DEXT(framebuffer.name, attachment, textarget, texture.name, level,
                                      zoffset);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment,
                                                   GLenum textarget, GLuint texture, GLint level,
                                                   GLint zoffset)
{
  SERIALISE_TIME_CALL(GL.glNamedFramebufferTexture3DEXT(framebuffer, attachment, textarget, texture,
                                                        level, zoffset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTexture3DEXT(ser, framebuffer, attachment, textarget, texture,
                                             level, zoffset);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget,
                                           GLuint texture, GLint level, GLint zoffset)
{
  SERIALISE_TIME_CALL(
      GL.glFramebufferTexture3D(target, attachment, textarget, texture, level, zoffset));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTexture3DEXT(ser, record->Resource.name, attachment, textarget,
                                             texture, level, zoffset);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferRenderbufferEXT(SerialiserType &ser,
                                                                GLuint framebufferHandle,
                                                                GLenum attachment,
                                                                GLenum renderbuffertarget,
                                                                GLuint renderbufferHandle)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT(renderbuffertarget);
  SERIALISE_ELEMENT_LOCAL(renderbuffer, RenderbufferRes(GetCtx(), renderbufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glNamedFramebufferRenderbufferEXT(framebuffer.name, attachment, renderbuffertarget,
                                         renderbuffer.name);

    if(IsLoading(m_State) && renderbuffer.name)
    {
      m_Textures[GetResourceManager()->GetID(renderbuffer)].creationFlags |=
          TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment,
                                                      GLenum renderbuffertarget, GLuint renderbuffer)
{
  SERIALISE_TIME_CALL(GL.glNamedFramebufferRenderbufferEXT(framebuffer, attachment,
                                                           renderbuffertarget, renderbuffer));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferRenderbufferEXT(ser, framebuffer, attachment, renderbuffertarget,
                                                renderbuffer);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(RenderbufferRes(GetCtx(), renderbuffer),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                              GLenum renderbuffertarget, GLuint renderbuffer)
{
  SERIALISE_TIME_CALL(
      GL.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer));

  if(IsCaptureMode(m_State))
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
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferRenderbufferEXT(ser, record->Resource.name, attachment,
                                                renderbuffertarget, renderbuffer);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(RenderbufferRes(GetCtx(), renderbuffer),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferTextureLayerEXT(SerialiserType &ser,
                                                                GLuint framebufferHandle,
                                                                GLenum attachment,
                                                                GLuint textureHandle, GLint level,
                                                                GLint layer)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(layer);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glNamedFramebufferTextureLayerEXT(framebuffer.name, attachment, texture.name, level, layer);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ColorTarget;
    }

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment,
                                                      GLuint texture, GLint level, GLint layer)
{
  SERIALISE_TIME_CALL(
      GL.glNamedFramebufferTextureLayerEXT(framebuffer, attachment, texture, level, layer));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(texture != 0 && GetResourceManager()->HasResourceRecord(TextureRes(GetCtx(), texture)))
    {
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTextureLayerEXT(ser, framebuffer, attachment, texture, level, layer);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

void WrappedOpenGL::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                                              GLint level, GLint layer)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTextureLayer(target, attachment, texture, level, layer));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferTextureLayerEXT(ser, record->Resource.name, attachment, texture,
                                                level, layer);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFramebufferTextureMultiviewOVR(SerialiserType &ser, GLenum target,
                                                               GLenum attachment,
                                                               GLuint textureHandle, GLint level,
                                                               GLint baseViewIndex, GLsizei numViews)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(baseViewIndex);
  SERIALISE_ELEMENT(numViews);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glFramebufferTextureMultiviewOVR);

    GL.glFramebufferTextureMultiviewOVR(target, attachment, texture.name, level, baseViewIndex,
                                        numViews);

    if(IsLoading(m_State) && texture.name)
    {
      if(attachment == eGL_DEPTH_ATTACHMENT || attachment == eGL_DEPTH_STENCIL_ATTACHMENT)
        m_Textures[GetResourceManager()->GetID(texture)].creationFlags |=
            TextureCategory::DepthTarget;
      else
        m_Textures[GetResourceManager()->GetID(texture)].creationFlags |=
            TextureCategory::ColorTarget;
    }

    {
      GLuint fbo = 0;
      GL.glGetIntegerv(FramebufferBinding(target), (GLint *)&fbo);
      AddResourceInitChunk(FramebufferRes(GetCtx(), fbo));
    }
  }

  return true;
}

void WrappedOpenGL::glFramebufferTextureMultiviewOVR(GLenum target, GLenum attachment,
                                                     GLuint texture, GLint level,
                                                     GLint baseViewIndex, GLsizei numViews)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTextureMultiviewOVR(target, attachment, texture, level,
                                                          baseViewIndex, numViews));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    // because there's no DSA variant of the OVR framebuffer functions we must ensure that while
    // background capturing the correct framebuffer is bound. Normally we don't serialise
    // glBindFramebuffer calls.
    if(IsBackgroundCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glBindFramebuffer);
      Serialise_glBindFramebuffer(ser, target, record->Resource.name);

      record->AddChunk(scope.Get());
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferTextureMultiviewOVR(ser, target, attachment, texture, level,
                                               baseViewIndex, numViews);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFramebufferTextureMultisampleMultiviewOVR(
    SerialiserType &ser, GLenum target, GLenum attachment, GLuint textureHandle, GLint level,
    GLsizei samples, GLint baseViewIndex, GLsizei numViews)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(attachment);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(baseViewIndex);
  SERIALISE_ELEMENT(numViews);

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glFramebufferTextureMultisampleMultiviewOVR);

    GL.glFramebufferTextureMultisampleMultiviewOVR(target, attachment, texture.name, level, samples,
                                                   baseViewIndex, numViews);

    if(IsLoading(m_State) && texture.name)
    {
      if(attachment == eGL_DEPTH_ATTACHMENT || attachment == eGL_DEPTH_STENCIL_ATTACHMENT)
        m_Textures[GetResourceManager()->GetID(texture)].creationFlags |=
            TextureCategory::DepthTarget;
      else
        m_Textures[GetResourceManager()->GetID(texture)].creationFlags |=
            TextureCategory::ColorTarget;
    }

    {
      GLuint fbo = 0;
      GL.glGetIntegerv(FramebufferBinding(target), (GLint *)&fbo);
      AddResourceInitChunk(FramebufferRes(GetCtx(), fbo));
    }
  }

  return true;
}

void WrappedOpenGL::glFramebufferTextureMultisampleMultiviewOVR(GLenum target, GLenum attachment,
                                                                GLuint texture, GLint level,
                                                                GLsizei samples, GLint baseViewIndex,
                                                                GLsizei numViews)
{
  SERIALISE_TIME_CALL(GL.glFramebufferTextureMultisampleMultiviewOVR(
      target, attachment, texture, level, samples, baseViewIndex, numViews));

  if(IsCaptureMode(m_State))
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
      GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    // because there's no DSA variant of the OVR framebuffer functions we must ensure that while
    // background capturing the correct framebuffer is bound. Normally we don't serialise
    // glBindFramebuffer calls.
    if(IsBackgroundCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glBindFramebuffer);
      Serialise_glBindFramebuffer(ser, target, record->Resource.name);

      record->AddChunk(scope.Get());
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferTextureMultisampleMultiviewOVR(ser, target, attachment, texture, level,
                                                          samples, baseViewIndex, numViews);

    if(IsBackgroundCapturing(m_State))
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
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkFBOReferenced(record->Resource, eFrameRef_ReadBeforeWrite);
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedFramebufferParameteriEXT(SerialiserType &ser,
                                                              GLuint framebufferHandle,
                                                              GLenum pname, GLint param)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    if(framebuffer.name)
      GL.glNamedFramebufferParameteriEXT(framebuffer.name, pname, param);

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param)
{
  SERIALISE_TIME_CALL(GL.glNamedFramebufferParameteriEXT(framebuffer, pname, param));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferParameteriEXT(ser, framebuffer, pname, param);

    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
  SERIALISE_TIME_CALL(GL.glFramebufferParameteri(target, pname, param));

  if(IsCaptureMode(m_State))
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

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedFramebufferParameteriEXT(ser, record->Resource.name, pname, param);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFramebufferReadBufferEXT(SerialiserType &ser,
                                                         GLuint framebufferHandle, GLenum mode)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(mode);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    // since we are faking the default framebuffer with our own
    // to see the results, replace back/front/left/right with color attachment 0
    if(mode == eGL_BACK_LEFT || mode == eGL_BACK_RIGHT || mode == eGL_BACK ||
       mode == eGL_FRONT_LEFT || mode == eGL_FRONT_RIGHT || mode == eGL_FRONT)
      mode = eGL_COLOR_ATTACHMENT0;

    GL.glFramebufferReadBufferEXT(framebuffer.name, mode);

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glFramebufferReadBufferEXT(GLuint framebuffer, GLenum buf)
{
  SERIALISE_TIME_CALL(GL.glFramebufferReadBufferEXT(framebuffer, buf));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferReadBufferEXT(ser, framebuffer, buf);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
  else if(IsBackgroundCapturing(m_State) && framebuffer != 0)
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferReadBufferEXT(ser, framebuffer, buf);

    ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glReadBuffer(GLenum mode)
{
  SERIALISE_TIME_CALL(GL.glReadBuffer(mode));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *readrecord = GetCtxData().m_ReadFramebufferRecord;
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glFramebufferReadBufferEXT(ser, readrecord ? readrecord->Resource.name : 0, mode);

      GetContextRecord()->AddChunk(scope.Get());
      if(readrecord)
        GetResourceManager()->MarkFBOReferenced(readrecord->Resource, eFrameRef_ReadBeforeWrite);
    }
    else
    {
      if(readrecord)
        GetResourceManager()->MarkDirtyResource(readrecord->GetResourceID());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindFramebuffer(SerialiserType &ser, GLenum target,
                                                GLuint framebufferHandle)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GL.glBindFramebuffer(target, framebuffer.name);
  }

  return true;
}

void WrappedOpenGL::glBindFramebuffer(GLenum target, GLuint framebuffer)
{
  SERIALISE_TIME_CALL(GL.glBindFramebuffer(target, framebuffer));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindFramebuffer(ser, target, framebuffer);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }

  if(target == eGL_DRAW_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    GetCtxData().m_DrawFramebufferRecord =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

  if(target == eGL_READ_FRAMEBUFFER || target == eGL_FRAMEBUFFER)
    GetCtxData().m_ReadFramebufferRecord =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFramebufferDrawBufferEXT(SerialiserType &ser,
                                                         GLuint framebufferHandle, GLenum buf)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(buf);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    // since we are faking the default framebuffer with our own
    // to see the results, replace back/front/left/right with color attachment 0
    if(buf == eGL_BACK_LEFT || buf == eGL_BACK_RIGHT || buf == eGL_BACK || buf == eGL_FRONT_LEFT ||
       buf == eGL_FRONT_RIGHT || buf == eGL_FRONT)
      buf = eGL_COLOR_ATTACHMENT0;

    GL.glFramebufferDrawBufferEXT(framebuffer.name, buf);

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum buf)
{
  SERIALISE_TIME_CALL(GL.glFramebufferDrawBufferEXT(framebuffer, buf));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferDrawBufferEXT(ser, framebuffer, buf);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
  else if(IsBackgroundCapturing(m_State) && framebuffer != 0)
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferDrawBufferEXT(ser, framebuffer, buf);

    ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDrawBuffer(GLenum buf)
{
  SERIALISE_TIME_CALL(GL.glDrawBuffer(buf));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *drawrecord = GetCtxData().m_DrawFramebufferRecord;
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glFramebufferDrawBufferEXT(ser, drawrecord ? drawrecord->Resource.name : 0, buf);

      GetContextRecord()->AddChunk(scope.Get());
      if(drawrecord)
        GetResourceManager()->MarkFBOReferenced(drawrecord->Resource, eFrameRef_ReadBeforeWrite);
    }
    else
    {
      if(drawrecord)
        GetResourceManager()->MarkDirtyResource(drawrecord->GetResourceID());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glFramebufferDrawBuffersEXT(SerialiserType &ser,
                                                          GLuint framebufferHandle, GLsizei n,
                                                          const GLenum *bufs)
{
  SERIALISE_ELEMENT_LOCAL(framebuffer, FramebufferRes(GetCtx(), framebufferHandle));
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_ARRAY(bufs, n);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(framebuffer.name == 0)
      framebuffer.name = m_CurrentDefaultFBO;

    GLenum *buffers = (GLenum *)bufs;

    for(GLsizei i = 0; i < n; i++)
    {
      // since we are faking the default framebuffer with our own
      // to see the results, replace back/front/left/right with color attachment 0
      if(buffers[i] == eGL_BACK_LEFT || buffers[i] == eGL_BACK_RIGHT || buffers[i] == eGL_BACK ||
         buffers[i] == eGL_FRONT_LEFT || buffers[i] == eGL_FRONT_RIGHT || buffers[i] == eGL_FRONT)
        buffers[i] = eGL_COLOR_ATTACHMENT0;
    }

    GL.glFramebufferDrawBuffersEXT(framebuffer.name, n, bufs);

    AddResourceInitChunk(framebuffer);
  }

  return true;
}

void WrappedOpenGL::glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
  SERIALISE_TIME_CALL(GL.glFramebufferDrawBuffersEXT(framebuffer, n, bufs));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferDrawBuffersEXT(ser, framebuffer, n, bufs);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), framebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
  else if(IsBackgroundCapturing(m_State) && framebuffer != 0)
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glFramebufferDrawBuffersEXT(ser, framebuffer, n, bufs);

    ResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));
    record->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glDrawBuffers(GLsizei n, const GLenum *bufs)
{
  SERIALISE_TIME_CALL(GL.glDrawBuffers(n, bufs));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *drawrecord = GetCtxData().m_DrawFramebufferRecord;
    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      if(drawrecord)
        Serialise_glFramebufferDrawBuffersEXT(ser, drawrecord->Resource.name, n, bufs);
      else
        Serialise_glFramebufferDrawBuffersEXT(ser, 0, n, bufs);

      GetContextRecord()->AddChunk(scope.Get());
      if(drawrecord)
        GetResourceManager()->MarkFBOReferenced(drawrecord->Resource, eFrameRef_ReadBeforeWrite);
    }
    else
    {
      if(drawrecord)
        GetResourceManager()->MarkDirtyResource(drawrecord->GetResourceID());
    }
  }
}

void WrappedOpenGL::glInvalidateFramebuffer(GLenum target, GLsizei numAttachments,
                                            const GLenum *attachments)
{
  GL.glInvalidateFramebuffer(target, numAttachments, attachments);

  if(IsBackgroundCapturing(m_State))
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
  GL.glDiscardFramebufferEXT(target, numAttachments, attachments);

  if(IsBackgroundCapturing(m_State))
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
  GL.glInvalidateNamedFramebufferData(framebuffer, numAttachments, attachments);

  if(IsBackgroundCapturing(m_State))
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
  GL.glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);

  if(IsBackgroundCapturing(m_State))
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
  GL.glInvalidateNamedFramebufferSubData(framebuffer, numAttachments, attachments, x, y, width,
                                         height);

  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(FramebufferRes(GetCtx(), framebuffer));

    if(record)
      record->MarkParentsDirty(GetResourceManager());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBlitNamedFramebuffer(SerialiserType &ser,
                                                     GLuint readFramebufferHandle,
                                                     GLuint drawFramebufferHandle, GLint srcX0,
                                                     GLint srcY0, GLint srcX1, GLint srcY1,
                                                     GLint dstX0, GLint dstY0, GLint dstX1,
                                                     GLint dstY1, GLbitfield mask, GLenum filter)
{
  SERIALISE_ELEMENT_LOCAL(readFramebuffer, FramebufferRes(GetCtx(), readFramebufferHandle));
  SERIALISE_ELEMENT_LOCAL(drawFramebuffer, FramebufferRes(GetCtx(), drawFramebufferHandle));
  SERIALISE_ELEMENT(srcX0);
  SERIALISE_ELEMENT(srcY0);
  SERIALISE_ELEMENT(srcX1);
  SERIALISE_ELEMENT(srcY1);
  SERIALISE_ELEMENT(dstX0);
  SERIALISE_ELEMENT(dstY0);
  SERIALISE_ELEMENT(dstX1);
  SERIALISE_ELEMENT(dstY1);
  SERIALISE_ELEMENT_TYPED(GLframebufferbitfield, mask);
  SERIALISE_ELEMENT(filter);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(readFramebuffer.name == 0)
      readFramebuffer.name = m_CurrentDefaultFBO;
    if(drawFramebuffer.name == 0)
      drawFramebuffer.name = m_CurrentDefaultFBO;
    // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
    // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
    // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
    // and we need to support this case.
    GL.glBlitNamedFramebuffer(readFramebuffer.name, drawFramebuffer.name, srcX0, srcY0, srcX1,
                              srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    if(IsLoading(m_State))
    {
      AddEvent();

      ResourceId readId = GetResourceManager()->GetID(readFramebuffer);
      ResourceId drawId = GetResourceManager()->GetID(drawFramebuffer);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%s, %s)", ToStr(gl_CurChunk).c_str(),
                                    ToStr(GetResourceManager()->GetOriginalID(readId)).c_str(),
                                    ToStr(GetResourceManager()->GetOriginalID(drawId)).c_str());
      draw.flags |= DrawFlags::Resolve;

      GLint numCols = 8;
      GL.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

      for(int i = 0; i < numCols + 2; i++)
      {
        GLenum attachName = GLenum(eGL_COLOR_ATTACHMENT0 + i);
        if(i == numCols)
          attachName = eGL_DEPTH_ATTACHMENT;
        if(i == numCols + 1)
          attachName = eGL_STENCIL_ATTACHMENT;

        GLuint srcattachment = 0, dstattachment = 0;
        GLenum srctype = eGL_TEXTURE, dsttype = eGL_TEXTURE;

        GL.glGetNamedFramebufferAttachmentParameterivEXT(readFramebuffer.name, attachName,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&srcattachment);
        GL.glGetNamedFramebufferAttachmentParameterivEXT(readFramebuffer.name, attachName,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                         (GLint *)&srctype);

        GL.glGetNamedFramebufferAttachmentParameterivEXT(drawFramebuffer.name, attachName,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&dstattachment);
        GL.glGetNamedFramebufferAttachmentParameterivEXT(drawFramebuffer.name, attachName,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                         (GLint *)&dsttype);

        ResourceId srcid, dstid;

        if(srctype == eGL_TEXTURE)
          srcid = GetResourceManager()->GetID(TextureRes(GetCtx(), srcattachment));
        else
          srcid = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), srcattachment));

        if(dsttype == eGL_TEXTURE)
          dstid = GetResourceManager()->GetID(TextureRes(GetCtx(), dstattachment));
        else
          dstid = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), dstattachment));

        if(mask & GL_COLOR_BUFFER_BIT)
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
  }

  return true;
}

void WrappedOpenGL::glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer,
                                           GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                           GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                           GLbitfield mask, GLenum filter)
{
  CoherentMapImplicitBarrier();

  // use ARB_direct_state_access functions here as we use EXT_direct_state_access elsewhere. If
  // we are running without ARB_dsa support, these functions are emulated in the obvious way. This
  // is necessary since these functions can be serialised even if ARB_dsa was not used originally,
  // and we need to support this case.
  SERIALISE_TIME_CALL(GL.glBlitNamedFramebuffer(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1,
                                                srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlitNamedFramebuffer(ser, readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1,
                                     srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), readFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), drawFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
}

void WrappedOpenGL::glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                                      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                      GLbitfield mask, GLenum filter)
{
  CoherentMapImplicitBarrier();

  SERIALISE_TIME_CALL(
      GL.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter));

  if(IsActiveCapturing(m_State))
  {
    GLuint readFramebuffer = 0, drawFramebuffer = 0;

    if(GetCtxData().m_ReadFramebufferRecord)
      readFramebuffer = GetCtxData().m_ReadFramebufferRecord->Resource.name;
    if(GetCtxData().m_DrawFramebufferRecord)
      drawFramebuffer = GetCtxData().m_DrawFramebufferRecord->Resource.name;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBlitNamedFramebuffer(ser, readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1,
                                     srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), readFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
    GetResourceManager()->MarkFBOReferenced(FramebufferRes(GetCtx(), drawFramebuffer),
                                            eFrameRef_ReadBeforeWrite);
  }
}

void WrappedOpenGL::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = FramebufferRes(GetCtx(), framebuffers[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteFramebuffers(n, framebuffers);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenRenderbuffers(SerialiserType &ser, GLsizei n, GLuint *renderbuffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(renderbuffer,
                          GetResourceManager()->GetID(RenderbufferRes(GetCtx(), *renderbuffers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenRenderbuffers(1, &real);
    GL.glBindRenderbuffer(eGL_RENDERBUFFER, real);

    GLResource res = RenderbufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(renderbuffer, res);

    AddResource(renderbuffer, ResourceType::Texture, "Renderbuffer");

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_RENDERBUFFER;
  }

  return true;
}

void WrappedOpenGL::glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  SERIALISE_TIME_CALL(GL.glGenRenderbuffers(n, renderbuffers));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenRenderbuffers(ser, 1, renderbuffers + i);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateRenderbuffers(SerialiserType &ser, GLsizei n,
                                                    GLuint *renderbuffers)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(renderbuffer,
                          GetResourceManager()->GetID(RenderbufferRes(GetCtx(), *renderbuffers)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateRenderbuffers(1, &real);
    GL.glBindRenderbuffer(eGL_RENDERBUFFER, real);

    GLResource res = RenderbufferRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(renderbuffer, res);

    AddResource(renderbuffer, ResourceType::Texture, "Renderbuffer");

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_RENDERBUFFER;
  }

  return true;
}

void WrappedOpenGL::glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  SERIALISE_TIME_CALL(GL.glCreateRenderbuffers(n, renderbuffers));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateRenderbuffers(ser, 1, renderbuffers + i);

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

  GL.glBindRenderbuffer(target, renderbuffer);
}

void WrappedOpenGL::glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = RenderbufferRes(GetCtx(), renderbuffers[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteRenderbuffers(n, renderbuffers);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedRenderbufferStorageEXT(SerialiserType &ser,
                                                            GLuint renderbufferHandle,
                                                            GLenum internalformat, GLsizei width,
                                                            GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(renderbuffer, RenderbufferRes(GetCtx(), renderbufferHandle));
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetID(renderbuffer);
    TextureData &texDetails = m_Textures[liveId];

    GLenum fmt = GetBaseFormat(internalformat);

    texDetails.width = width;
    texDetails.height = height;
    texDetails.depth = 1;
    texDetails.samples = 1;
    texDetails.curType = eGL_RENDERBUFFER;
    texDetails.internalFormat = internalformat;
    texDetails.mipsValid = 1;

    GL.glNamedRenderbufferStorageEXT(renderbuffer.name, internalformat, width, height);

    if(internalformat == eGL_DEPTH_COMPONENT || internalformat == eGL_DEPTH_STENCIL ||
       internalformat == eGL_STENCIL || internalformat == eGL_STENCIL_INDEX)
    {
      // fetch the exact sized depth-stencil formats corresponding to whatever unsized format was
      // specified.
      GLint depth = 0;
      GLint stencil = 0;
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_DEPTH_SIZE, &depth);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_STENCIL_SIZE,
                                              &stencil);

      if(depth == 16 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT16;
      else if(depth == 24 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT24;
      else if(depth == 24 && stencil == 8)
        internalformat = eGL_DEPTH24_STENCIL8;
      else if(depth == 32 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT32F;
      else if(depth == 32 && stencil == 8)
        internalformat = eGL_DEPTH32F_STENCIL8;
      else if(depth == 0 && stencil == 8)
        internalformat = eGL_STENCIL_INDEX8;
    }
    else if(internalformat == eGL_RGBA || internalformat == eGL_RGBA_INTEGER ||
            internalformat == eGL_RGB || internalformat == eGL_RGB_INTEGER ||
            internalformat == eGL_RG || internalformat == eGL_RG_INTEGER ||
            internalformat == eGL_RED || internalformat == eGL_RED_INTEGER)
    {
      // if the color format is unsized, find the corresponding sized format

      GLint red = 0, green = 0, blue = 0, alpha = 0;
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_RED_SIZE, &red);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_GREEN_SIZE, &green);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_BLUE_SIZE, &blue);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_ALPHA_SIZE, &alpha);

      // we only handle a straight regular format here
      RDCASSERT(red > 0);
      RDCASSERT(green == 0 || green == red);
      RDCASSERT(blue == 0 || green == red);
      RDCASSERT(alpha == 0 || green == red);

      // to start with, create resource format based on the unsized internalformat
      ResourceFormat resfmt = MakeResourceFormat(eGL_TEXTURE_2D, internalformat);

      // then set the byte size
      resfmt.compByteWidth = uint8_t(red / 8);

      internalformat = MakeGLFormat(resfmt);
    }

    // create read-from texture for displaying this render buffer
    GL.glGenTextures(1, &texDetails.renderbufferReadTex);
    GL.glBindTexture(eGL_TEXTURE_2D, texDetails.renderbufferReadTex);
    GL.glTexImage2D(eGL_TEXTURE_2D, 0, internalformat, width, height, 0,
                    GetBaseFormat(internalformat), GetDataType(internalformat), NULL);
    GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
    GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
    GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR);

    GL.glGenFramebuffers(2, texDetails.renderbufferFBOs);
    GL.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
    GL.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);

    GLenum attach = eGL_COLOR_ATTACHMENT0;
    if(fmt == eGL_DEPTH_COMPONENT)
      attach = eGL_DEPTH_ATTACHMENT;
    if(fmt == eGL_STENCIL)
      attach = eGL_STENCIL_ATTACHMENT;
    if(fmt == eGL_DEPTH_STENCIL)
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
    GL.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach, eGL_RENDERBUFFER,
                                         renderbuffer.name);
    GL.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach, eGL_TEXTURE_2D,
                                      texDetails.renderbufferReadTex, 0);

    AddResourceInitChunk(renderbuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat,
                                                  GLsizei width, GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glNamedRenderbufferStorageEXT(renderbuffer, internalformat, width, height));

  ResourceId rb = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 renderbuffer);

    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glNamedRenderbufferStorageEXT(ser, record->Resource.name, internalformat, width,
                                              height);

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
    m_Textures[rb].mipsValid = 1;
  }
}

void WrappedOpenGL::glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width,
                                          GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glRenderbufferStorage(target, internalformat, width, height));

  ResourceId rb = GetCtxData().m_Renderbuffer;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify implicit renderbuffer. Not bound?", record);

    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glNamedRenderbufferStorageEXT(ser, record->Resource.name, internalformat, width,
                                              height);

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
    m_Textures[rb].mipsValid = 1;
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedRenderbufferStorageMultisampleEXT(SerialiserType &ser,
                                                                       GLuint renderbufferHandle,
                                                                       GLsizei samples,
                                                                       GLenum internalformat,
                                                                       GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(renderbuffer, RenderbufferRes(GetCtx(), renderbufferHandle));
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // the DSA function is emulated if not present, but we need to check the underlying function is
    // present
    CheckReplayFunctionPresent(GL.glRenderbufferStorageMultisample);

    ResourceId liveId = GetResourceManager()->GetID(renderbuffer);
    TextureData &texDetails = m_Textures[liveId];

    GLenum fmt = GetBaseFormat(internalformat);

    texDetails.width = width;
    texDetails.height = height;
    texDetails.depth = 1;
    texDetails.samples = samples;
    texDetails.curType = eGL_RENDERBUFFER;
    texDetails.internalFormat = internalformat;
    texDetails.mipsValid = 1;

    GL.glNamedRenderbufferStorageMultisampleEXT(renderbuffer.name, samples, internalformat, width,
                                                height);

    if(internalformat == eGL_DEPTH_COMPONENT || internalformat == eGL_DEPTH_STENCIL ||
       internalformat == eGL_STENCIL || internalformat == eGL_STENCIL_INDEX)
    {
      // fetch the exact sized depth-stencil formats corresponding to whatever unsized format was
      // specified.
      GLint depth = 0;
      GLint stencil = 0;
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_DEPTH_SIZE, &depth);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_STENCIL_SIZE,
                                              &stencil);

      if(depth == 16 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT16;
      else if(depth == 24 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT24;
      else if(depth == 24 && stencil == 8)
        internalformat = eGL_DEPTH24_STENCIL8;
      else if(depth == 32 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT32F;
      else if(depth == 32 && stencil == 8)
        internalformat = eGL_DEPTH32F_STENCIL8;
      else if(depth == 0 && stencil == 8)
        internalformat = eGL_STENCIL_INDEX8;
    }
    else if(internalformat == eGL_RGBA || internalformat == eGL_RGBA_INTEGER ||
            internalformat == eGL_RGB || internalformat == eGL_RGB_INTEGER ||
            internalformat == eGL_RG || internalformat == eGL_RG_INTEGER ||
            internalformat == eGL_RED || internalformat == eGL_RED_INTEGER)
    {
      // if the color format is unsized, find the corresponding sized format

      GLint red = 0, green = 0, blue = 0, alpha = 0;
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_RED_SIZE, &red);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_GREEN_SIZE, &green);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_BLUE_SIZE, &blue);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_ALPHA_SIZE, &alpha);

      // we only handle a straight regular format here
      RDCASSERT(red > 0);
      RDCASSERT(green == 0 || green == red);
      RDCASSERT(blue == 0 || green == red);
      RDCASSERT(alpha == 0 || green == red);

      // to start with, create resource format based on the unsized internalformat
      ResourceFormat resfmt = MakeResourceFormat(eGL_TEXTURE_2D, internalformat);

      // then set the byte size
      resfmt.compByteWidth = uint8_t(red / 8);

      internalformat = MakeGLFormat(resfmt);
    }

    // create read-from texture for displaying this render buffer
    GL.glGenTextures(1, &texDetails.renderbufferReadTex);
    GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex);
    GL.glTextureStorage2DMultisampleEXT(texDetails.renderbufferReadTex, eGL_TEXTURE_2D_MULTISAMPLE,
                                        samples, internalformat, width, height, true);

    GL.glGenFramebuffers(2, texDetails.renderbufferFBOs);
    GL.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
    GL.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);

    GLenum attach = eGL_COLOR_ATTACHMENT0;
    if(fmt == eGL_DEPTH_COMPONENT)
      attach = eGL_DEPTH_ATTACHMENT;
    if(fmt == eGL_STENCIL)
      attach = eGL_STENCIL_ATTACHMENT;
    if(fmt == eGL_DEPTH_STENCIL)
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
    GL.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach, eGL_RENDERBUFFER,
                                         renderbuffer.name);
    GL.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach,
                                      eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex, 0);

    AddResourceInitChunk(renderbuffer);
  }

  return true;
}

void WrappedOpenGL::glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples,
                                                             GLenum internalformat, GLsizei width,
                                                             GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glNamedRenderbufferStorageMultisampleEXT(renderbuffer, samples,
                                                                  internalformat, width, height));

  ResourceId rb = GetResourceManager()->GetID(RenderbufferRes(GetCtx(), renderbuffer));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify object passed to function. Mismatched or bad GLuint?", record,
                 renderbuffer);

    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glNamedRenderbufferStorageMultisampleEXT(ser, record->Resource.name, samples,
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
    m_Textures[rb].mipsValid = 1;
  }
}

void WrappedOpenGL::glRenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                                     GLenum internalformat, GLsizei width,
                                                     GLsizei height)
{
  SERIALISE_TIME_CALL(
      GL.glRenderbufferStorageMultisample(target, samples, internalformat, width, height));

  ResourceId rb = GetCtxData().m_Renderbuffer;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify implicit renderbuffer. Not bound?", record);

    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glNamedRenderbufferStorageMultisampleEXT(ser, record->Resource.name, samples,
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
    m_Textures[rb].mipsValid = 1;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// We need a separate implementation of glRenderbufferStorageMultisampleEXT because on some drivers
// there are issues with aliasing it with the core version of the function.

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glRenderbufferStorageMultisampleEXT(SerialiserType &ser,
                                                                  GLuint renderbufferHandle,
                                                                  GLsizei samples,
                                                                  GLenum internalformat,
                                                                  GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(renderbuffer, RenderbufferRes(GetCtx(), renderbufferHandle));
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glRenderbufferStorageMultisampleEXT);

    ResourceId liveId = GetResourceManager()->GetID(renderbuffer);
    TextureData &texDetails = m_Textures[liveId];

    GLenum fmt = GetBaseFormat(internalformat);

    texDetails.width = width;
    texDetails.height = height;
    texDetails.depth = 1;
    texDetails.samples = samples;
    texDetails.curType = eGL_RENDERBUFFER;
    texDetails.internalFormat = internalformat;
    texDetails.mipsValid = 1;

    GLuint oldRB = 0;
    GL.glGetIntegerv(eGL_RENDERBUFFER_BINDING, (GLint *)&oldRB);

    GL.glBindRenderbuffer(eGL_RENDERBUFFER, renderbuffer.name);

    GL.glRenderbufferStorageMultisampleEXT(eGL_RENDERBUFFER, samples, internalformat, width, height);

    if(internalformat == eGL_DEPTH_COMPONENT || internalformat == eGL_DEPTH_STENCIL ||
       internalformat == eGL_STENCIL || internalformat == eGL_STENCIL_INDEX)
    {
      // fetch the exact sized depth-stencil formats corresponding to whatever unsized format was
      // specified.
      GLint depth = 0;
      GLint stencil = 0;
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_DEPTH_SIZE, &depth);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_STENCIL_SIZE,
                                              &stencil);

      if(depth == 16 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT16;
      else if(depth == 24 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT24;
      else if(depth == 24 && stencil == 8)
        internalformat = eGL_DEPTH24_STENCIL8;
      else if(depth == 32 && stencil == 0)
        internalformat = eGL_DEPTH_COMPONENT32F;
      else if(depth == 32 && stencil == 8)
        internalformat = eGL_DEPTH32F_STENCIL8;
      else if(depth == 0 && stencil == 8)
        internalformat = eGL_STENCIL_INDEX8;
    }
    else if(internalformat == eGL_RGBA || internalformat == eGL_RGBA_INTEGER ||
            internalformat == eGL_RGB || internalformat == eGL_RGB_INTEGER ||
            internalformat == eGL_RG || internalformat == eGL_RG_INTEGER ||
            internalformat == eGL_RED || internalformat == eGL_RED_INTEGER)
    {
      // if the color format is unsized, find the corresponding sized format

      GLint red = 0, green = 0, blue = 0, alpha = 0;
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_RED_SIZE, &red);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_GREEN_SIZE, &green);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_BLUE_SIZE, &blue);
      GL.glGetNamedRenderbufferParameterivEXT(renderbuffer.name, eGL_RENDERBUFFER_ALPHA_SIZE, &alpha);

      // we only handle a straight regular format here
      RDCASSERT(red > 0);
      RDCASSERT(green == 0 || green == red);
      RDCASSERT(blue == 0 || green == red);
      RDCASSERT(alpha == 0 || green == red);

      // to start with, create resource format based on the unsized internalformat
      ResourceFormat resfmt = MakeResourceFormat(eGL_TEXTURE_2D, internalformat);

      // then set the byte size
      resfmt.compByteWidth = uint8_t(red / 8);

      internalformat = MakeGLFormat(resfmt);
    }

    // create read-from texture for displaying this render buffer
    GL.glGenTextures(1, &texDetails.renderbufferReadTex);
    GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex);
    GL.glTextureStorage2DMultisampleEXT(texDetails.renderbufferReadTex, eGL_TEXTURE_2D_MULTISAMPLE,
                                        samples, internalformat, width, height, true);

    GL.glGenFramebuffers(2, texDetails.renderbufferFBOs);
    GL.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);
    GL.glBindFramebuffer(eGL_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);

    GLenum attach = eGL_COLOR_ATTACHMENT0;
    if(fmt == eGL_DEPTH_COMPONENT)
      attach = eGL_DEPTH_ATTACHMENT;
    if(fmt == eGL_STENCIL)
      attach = eGL_STENCIL_ATTACHMENT;
    if(fmt == eGL_DEPTH_STENCIL)
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
    GL.glNamedFramebufferRenderbufferEXT(texDetails.renderbufferFBOs[0], attach, eGL_RENDERBUFFER,
                                         renderbuffer.name);
    GL.glNamedFramebufferTexture2DEXT(texDetails.renderbufferFBOs[1], attach,
                                      eGL_TEXTURE_2D_MULTISAMPLE, texDetails.renderbufferReadTex, 0);

    AddResourceInitChunk(renderbuffer);

    GL.glBindRenderbuffer(eGL_RENDERBUFFER, oldRB);
  }

  return true;
}

void WrappedOpenGL::glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples,
                                                        GLenum internalformat, GLsizei width,
                                                        GLsizei height)
{
  SERIALISE_TIME_CALL(
      GL.glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height));

  ResourceId rb = GetCtxData().m_Renderbuffer;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(rb);
    RDCASSERTMSG("Couldn't identify implicit renderbuffer. Not bound?", record);

    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glRenderbufferStorageMultisampleEXT(ser, record->Resource.name, samples,
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
    m_Textures[rb].mipsValid = 1;
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, glGenFramebuffers, GLsizei n, GLuint *framebuffers);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateFramebuffers, GLsizei n, GLuint *framebuffers);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferTextureEXT, GLuint framebufferHandle,
                                GLenum attachment, GLuint textureHandle, GLint level);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferTexture1DEXT, GLuint framebufferHandle,
                                GLenum attachment, GLenum textarget, GLuint textureHandle,
                                GLint level);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferTexture2DEXT, GLuint framebufferHandle,
                                GLenum attachment, GLenum textarget, GLuint textureHandle,
                                GLint level);
INSTANTIATE_FUNCTION_SERIALISED(void, glFramebufferTexture2DMultisampleEXT, GLuint framebufferHandle,
                                GLenum target, GLenum attachment, GLenum textarget,
                                GLuint textureHandle, GLint level, GLsizei samples);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferTexture3DEXT, GLuint framebufferHandle,
                                GLenum attachment, GLenum textarget, GLuint textureHandle,
                                GLint level, GLint zoffset);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferRenderbufferEXT, GLuint framebufferHandle,
                                GLenum attachment, GLenum renderbuffertarget,
                                GLuint renderbufferHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferTextureLayerEXT, GLuint framebufferHandle,
                                GLenum attachment, GLuint textureHandle, GLint level, GLint layer);
INSTANTIATE_FUNCTION_SERIALISED(void, glFramebufferTextureMultiviewOVR, GLenum target,
                                GLenum attachment, GLuint textureHandle, GLint level,
                                GLint baseViewIndex, GLsizei numViews);
INSTANTIATE_FUNCTION_SERIALISED(void, glFramebufferTextureMultisampleMultiviewOVR, GLenum target,
                                GLenum attachment, GLuint textureHandle, GLint level,
                                GLsizei samples, GLint baseViewIndex, GLsizei numViews);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedFramebufferParameteriEXT, GLuint framebufferHandle,
                                GLenum pname, GLint param);
INSTANTIATE_FUNCTION_SERIALISED(void, glFramebufferReadBufferEXT, GLuint framebufferHandle,
                                GLenum mode);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindFramebuffer, GLenum target, GLuint framebufferHandle);
INSTANTIATE_FUNCTION_SERIALISED(void, glFramebufferDrawBufferEXT, GLuint framebufferHandle,
                                GLenum buf);
INSTANTIATE_FUNCTION_SERIALISED(void, glFramebufferDrawBuffersEXT, GLuint framebufferHandle,
                                GLsizei n, const GLenum *bufs);
INSTANTIATE_FUNCTION_SERIALISED(void, glBlitNamedFramebuffer, GLuint readFramebufferHandle,
                                GLuint drawFramebufferHandle, GLint srcX0, GLint srcY0, GLint srcX1,
                                GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                                GLbitfield mask, GLenum filter);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenRenderbuffers, GLsizei n, GLuint *renderbuffers);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateRenderbuffers, GLsizei n, GLuint *renderbuffers);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedRenderbufferStorageEXT, GLuint renderbufferHandle,
                                GLenum internalformat, GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedRenderbufferStorageMultisampleEXT,
                                GLuint renderbufferHandle, GLsizei samples, GLenum internalformat,
                                GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glRenderbufferStorageMultisampleEXT,
                                GLuint renderbufferHandle, GLsizei samples, GLenum internalformat,
                                GLsizei width, GLsizei height);
