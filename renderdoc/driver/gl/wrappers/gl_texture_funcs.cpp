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

#include "../gl_driver.h"
#include "../gl_replay.h"
#include "common/common.h"
#include "strings/string_utils.h"

static GLenum RemapGenericCompressedFormat(GLint format)
{
  switch(format)
  {
    case eGL_COMPRESSED_RGB: return eGL_RGB8;
    case eGL_COMPRESSED_RGBA: return eGL_RGBA8;
    case eGL_COMPRESSED_SRGB: return eGL_SRGB8;
    case eGL_COMPRESSED_SRGB_ALPHA: return eGL_SRGB8_ALPHA8;
    case eGL_COMPRESSED_RED: return eGL_R8;
    case eGL_COMPRESSED_RG: return eGL_RG8;
    case eGL_COMPRESSED_ALPHA: return eGL_ALPHA8_EXT;
    case eGL_COMPRESSED_LUMINANCE: return eGL_LUMINANCE8_EXT;
    case eGL_COMPRESSED_LUMINANCE_ALPHA: return eGL_LUMINANCE8_ALPHA8_EXT;
    case eGL_COMPRESSED_INTENSITY: return eGL_INTENSITY8_EXT;
    case eGL_COMPRESSED_SLUMINANCE: return eGL_SLUMINANCE8;
    case eGL_COMPRESSED_SLUMINANCE_ALPHA: return eGL_SLUMINANCE8_ALPHA8;
    default: break;
  }

  return (GLenum)format;
}

static constexpr uint32_t numParams(GLenum pname)
{
  return (pname == eGL_TEXTURE_BORDER_COLOR || pname == eGL_TEXTURE_SWIZZLE_RGBA) ? 4U : 1U;
}

// NOTE: Handling of ARB_dsa vs. EXT_dsa
//
// To avoid lots of redundancy between the ARB_dsa/EXT_dsa variants, we handle it
// by passing along GL_NONE as the target parameter where the EXT function expects
// a target but there isn't a target parameter for the ARB function.
//
// As with everywhere else, non-DSA variants are always "promoted" to DSA functions
// and serialised as such. Since we require EXT_dsa functionality on replay this
// means we only need to differentiate between ARB and EXT.
//
// On replay, we check the target and if it's GL_NONE assume that it was an ARB
// call and replay as such. If the target is valid (or at least != GL_NONE) then
// we call the EXT variant. Since GL_NONE is never a valid target, there's no risk
// of overlap. That way we don't have to worry about emulating ARB_dsa when it's
// not present, as we only ever serialise an ARB version when the original call was
// ARB, unlike the promotion to DSA from non-DSA where there's ambiguity on what
// the original call was.

// This of course means that if a log is captured using ARB_dsa functions then the
// replay context must have ARB_dsa support, but this is to be expected and it
// would be a nightmare to support replaying without extensions that were present &
// used when capturing.

// a little helper here - we want to share serialisation for the functions (as above), but we also
// would like to omit the fake target from ARB_dsa calls. This macro takes advantage of being able
// to retroactively mark a value as hidden based on its value. Usually you do
// SERIALISE_ELEMENT().Hidden(); but it can be split apart like this.
#define HIDE_ARB_DSA_TARGET()               \
  if(ser.IsReading() && target == eGL_NONE) \
    ser.Hidden();

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenTextures(SerialiserType &ser, GLsizei n, GLuint *textures)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(texture, GetResourceManager()->GetResID(TextureRes(GetCtx(), *textures)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glGenTextures(1, &real);

    GLResource res = TextureRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(texture, res);

    AddResource(texture, ResourceType::Texture, "Texture");

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_NONE;
  }

  return true;
}

void WrappedOpenGL::glGenTextures(GLsizei n, GLuint *textures)
{
  SERIALISE_TIME_CALL(GL.glGenTextures(n, textures));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = TextureRes(GetCtx(), textures[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenTextures(ser, 1, textures + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
      m_Textures[id].resource = res;
      m_Textures[id].curType = eGL_NONE;
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateTextures(SerialiserType &ser, GLenum target, GLsizei n,
                                               GLuint *textures)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(texture, GetResourceManager()->GetResID(TextureRes(GetCtx(), *textures)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    GL.glCreateTextures(target, 1, &real);

    GLResource res = TextureRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(texture, res);

    AddResource(texture, ResourceType::Texture, "Texture");

    m_Textures[live].resource = res;
    m_Textures[live].curType = TextureTarget(target);
    m_Textures[live].creationFlags |= TextureCategory::ShaderRead;
  }

  return true;
}

void WrappedOpenGL::glCreateTextures(GLenum target, GLsizei n, GLuint *textures)
{
  SERIALISE_TIME_CALL(GL.glCreateTextures(target, n, textures));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = TextureRes(GetCtx(), textures[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateTextures(ser, target, 1, textures + i);

        chunk = scope.Get();
      }

      GLResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
      RDCASSERT(record);

      record->datatype = TextureBinding(target);
      m_Textures[id].resource = res;
      m_Textures[id].curType = TextureTarget(target);

      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, res);
      m_Textures[id].resource = res;
      m_Textures[id].curType = TextureTarget(target);
      m_Textures[id].creationFlags |= TextureCategory::ShaderRead;
    }
  }
}

void WrappedOpenGL::glDeleteTextures(GLsizei n, const GLuint *textures)
{
  ContextData &cd = GetCtxData();
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = TextureRes(GetCtx(), textures[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
      {
        GLResourceRecord *record = GetResourceManager()->GetResourceRecord(res);
        cd.ClearMatchingActiveTexRecord(record);
        record->Delete(GetResourceManager());
      }
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteTextures(n, textures);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindTexture(SerialiserType &ser, GLenum target, GLuint textureHandle)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBindTexture(target, texture.name);

    if(IsLoading(m_State) && texture.name)
    {
      TextureData &tex = m_Textures[GetResourceManager()->GetResID(texture)];
      // only set texture type if we don't have one. Otherwise refuse to re-type.
      if(tex.curType == eGL_NONE)
      {
        tex.curType = TextureTarget(target);
        AddResourceInitChunk(texture);
      }
      tex.creationFlags |= TextureCategory::ShaderRead;
    }
  }

  return true;
}

void WrappedOpenGL::glBindTexture(GLenum target, GLuint texture)
{
  SERIALISE_TIME_CALL(GL.glBindTexture(target, texture));

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindTexture(ser, target, texture);

      chunk = scope.Get();
    }

    GetContextRecord()->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }

  ContextData &cd = GetCtxData();

  if(texture == 0)
  {
    cd.SetActiveTexRecord(target, NULL);
    return;
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *r = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(r == NULL)
    {
      RDCERR("Called glBindTexture with unrecognised or deleted texture");
      return;
    }

    cd.SetActiveTexRecord(target, r);

    if(r->datatype)
    {
      // it's illegal to retype a texture
      RDCASSERT(r->datatype == TextureBinding(target));
    }
    else
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glBindTexture(ser, target, texture);

        chunk = scope.Get();
      }

      r->datatype = TextureBinding(target);
      m_Textures[r->GetResourceID()].curType = TextureTarget(target);

      r->AddChunk(chunk);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindTextures(SerialiserType &ser, GLuint first, GLsizei count,
                                             const GLuint *textureHandles)
{
  SERIALISE_ELEMENT(first).Important();
  SERIALISE_ELEMENT(count);

  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  rdcarray<GLResource> textures;

  if(ser.IsWriting())
  {
    textures.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      textures.push_back(TextureRes(GetCtx(), textureHandles ? textureHandles[i] : 0));
  }

  SERIALISE_ELEMENT(textures).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<GLuint> texs;
    texs.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      texs.push_back(textures[i].name);

    GL.glBindTextures(first, count, texs.data());

    if(IsLoading(m_State))
    {
      for(GLsizei i = 0; i < count; i++)
        m_Textures[GetResourceManager()->GetResID(textures[i])].creationFlags |=
            TextureCategory::ShaderRead;
    }
  }

  return true;
}

// glBindTextures doesn't provide a target, so can't be used to "init" a texture from glGenTextures
// which makes our lives a bit easier
void WrappedOpenGL::glBindTextures(GLuint first, GLsizei count, const GLuint *textures)
{
  SERIALISE_TIME_CALL(GL.glBindTextures(first, count, textures));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindTextures(ser, first, count, textures);

    GetContextRecord()->AddChunk(scope.Get());

    for(GLsizei i = 0; i < count; i++)
      if(textures != NULL && textures[i] != 0)
        GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), textures[i]),
                                                          eFrameRef_Read);
  }

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();

    for(GLsizei i = 0; i < count; i++)
    {
      if(textures == NULL || textures[i] == 0)
      {
        // NULLs all targets
        cd.ClearAllTexUnitRecordsIndexed(first + i);
      }
      else
      {
        GLResourceRecord *texrecord =
            GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), textures[i]));
        if(texrecord)
        {
          GLenum target = TextureTarget(texrecord->datatype);

          cd.SetTexUnitRecordIndexed(target, first + i, texrecord);
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindMultiTextureEXT(SerialiserType &ser, GLenum texunit,
                                                    GLenum target, GLuint textureHandle)
{
  SERIALISE_ELEMENT(texunit).Important();
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();

  if(IsReplayingAndReading())
  {
    GL.glBindMultiTextureEXT(texunit, target, texture.name);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetResID(texture)].curType = TextureTarget(target);
      m_Textures[GetResourceManager()->GetResID(texture)].creationFlags |=
          TextureCategory::ShaderRead;
    }
  }

  return true;
}

void WrappedOpenGL::glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
  SERIALISE_TIME_CALL(GL.glBindMultiTextureEXT(texunit, target, texture));

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindMultiTextureEXT(ser, texunit, target, texture);

      chunk = scope.Get();
    }

    GetContextRecord()->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }

  ContextData &cd = GetCtxData();

  if(texture == 0)
  {
    cd.SetTexUnitRecord(target, texunit, NULL);
    return;
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *r = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(!r)
    {
      RDCERR("Called glBindMultiTextureEXT with unrecognised or deleted buffer");
      return;
    }

    cd.SetTexUnitRecord(target, texunit, r);

    if(r->datatype)
    {
      // it's illegal to retype a texture
      RDCASSERT(r->datatype == TextureBinding(target));
    }
    else
    {
      Chunk *chunk = NULL;

      // this is just a 'typing' bind, so doesn't need to be to the right slot, just anywhere.
      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(GLChunk::glBindTexture);
        Serialise_glBindTexture(ser, target, texture);

        chunk = scope.Get();
      }

      r->datatype = TextureBinding(target);
      m_Textures[r->GetResourceID()].curType = TextureTarget(target);

      r->AddChunk(chunk);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindTextureUnit(SerialiserType &ser, GLuint texunit,
                                                GLuint textureHandle)
{
  SERIALISE_ELEMENT(texunit);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBindTextureUnit(texunit, texture.name);
  }

  return true;
}

void WrappedOpenGL::glBindTextureUnit(GLuint unit, GLuint texture)
{
  SERIALISE_TIME_CALL(GL.glBindTextureUnit(unit, texture));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindTextureUnit(ser, unit, texture);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();

    if(texture == 0)
    {
      // NULLs all targets
      cd.ClearAllTexUnitRecordsIndexed(unit);
    }
    else
    {
      GLResourceRecord *texrecord =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

      if(texrecord)
      {
        GLenum target = TextureTarget(texrecord->datatype);

        cd.SetTexUnitRecordIndexed(target, unit, texrecord);
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindImageTexture(SerialiserType &ser, GLuint unit,
                                                 GLuint textureHandle, GLint level, GLboolean layered,
                                                 GLint layer, GLenum access, GLenum format)
{
  SERIALISE_ELEMENT(unit);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT_TYPED(bool, layered);
  SERIALISE_ELEMENT(layer);
  SERIALISE_ELEMENT(access);
  SERIALISE_ELEMENT(format);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glBindImageTexture(unit, texture.name, level, layered, layer, access, format);

    if(IsLoading(m_State))
      m_Textures[GetResourceManager()->GetResID(texture)].creationFlags |=
          TextureCategory::ShaderReadWrite;
  }

  return true;
}

void WrappedOpenGL::glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                                       GLint layer, GLenum access, GLenum format)
{
  if(IsCaptureMode(m_State))
  {
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture),
                                                      eFrameRef_ReadBeforeWrite);

    GetCtxData().m_MaxImgBind = RDCMAX((GLint)unit + 1, GetCtxData().m_MaxImgBind);
  }

  SERIALISE_TIME_CALL(GL.glBindImageTexture(unit, texture, level, layered, layer, access, format));

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindImageTexture(ser, unit, texture, level, layered, layer, access, format);

      chunk = scope.Get();
    }

    GetContextRecord()->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindImageTextures(SerialiserType &ser, GLuint first, GLsizei count,
                                                  const GLuint *textureHandles)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);

  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  rdcarray<GLResource> textures;

  if(ser.IsWriting())
  {
    textures.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      textures.push_back(TextureRes(GetCtx(), textureHandles ? textureHandles[i] : 0));
  }

  SERIALISE_ELEMENT(textures);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<GLuint> texs;
    texs.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      texs.push_back(textures[i].name);

    GL.glBindImageTextures(first, count, texs.data());

    if(IsLoading(m_State))
    {
      for(GLsizei i = 0; i < count; i++)
        m_Textures[GetResourceManager()->GetResID(textures[i])].creationFlags |=
            TextureCategory::ShaderReadWrite;
    }
  }

  return true;
}

void WrappedOpenGL::glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures)
{
  if(IsCaptureMode(m_State))
  {
    for(GLsizei i = 0; i < count; i++)
      if(textures != NULL && textures[i] != 0)
        GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), textures[i]),
                                                          eFrameRef_ReadBeforeWrite);

    GetCtxData().m_MaxImgBind = RDCMAX((GLint)first + count, GetCtxData().m_MaxImgBind);
  }

  SERIALISE_TIME_CALL(GL.glBindImageTextures(first, count, textures));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindImageTextures(ser, first, count, textures);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureView(SerialiserType &ser, GLuint textureHandle,
                                            GLenum target, GLuint origtextureHandle,
                                            GLenum internalformat, GLuint minlevel,
                                            GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(origtexture, TextureRes(GetCtx(), origtextureHandle));
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(minlevel);
  SERIALISE_ELEMENT(numlevels);
  SERIALISE_ELEMENT(minlayer);
  SERIALISE_ELEMENT(numlayers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum intformat = internalformat;

    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(0, target, internalformat, dummy);

    GL.glTextureView(texture.name, target, origtexture.name, internalformat, minlevel, numlevels,
                     minlayer, numlayers);

    if(emulated)
    {
      // call again, this time to apply the swizzle
      EmulateLuminanceFormat(texture.name, target, intformat, dummy);
    }

    ResourceId liveTexId = GetResourceManager()->GetResID(texture);
    ResourceId liveOrigId = GetResourceManager()->GetResID(origtexture);

    m_Textures[liveTexId].curType = TextureTarget(target);
    m_Textures[liveTexId].internalFormat = internalformat;
    m_Textures[liveTexId].view = true;
    m_Textures[liveTexId].width = RDCMAX(1, m_Textures[liveOrigId].width >> minlevel);
    m_Textures[liveTexId].height = RDCMAX(1, m_Textures[liveOrigId].height >> minlevel);
    m_Textures[liveTexId].depth = numlayers;
    if(target == eGL_TEXTURE_3D)
      m_Textures[liveTexId].depth = RDCMAX(1, m_Textures[liveOrigId].depth >> minlevel);
    m_Textures[liveTexId].mipsValid = (1 << numlevels) - 1;
    m_Textures[liveTexId].emulated = emulated;

    AddResourceInitChunk(texture);
    DerivedResource(origtexture, GetResourceManager()->GetOriginalID(liveTexId));
  }

  return true;
}

void WrappedOpenGL::glTextureView(GLuint texture, GLenum target, GLuint origtexture,
                                  GLenum internalformat, GLuint minlevel, GLuint numlevels,
                                  GLuint minlayer, GLuint numlayers)
{
  SERIALISE_TIME_CALL(GL.glTextureView(texture, target, origtexture, internalformat, minlevel,
                                       numlevels, minlayer, numlayers));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
    GLResourceRecord *origrecord =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), origtexture));

    RDCASSERTMSG("Couldn't identify texture object. Unbound or bad GLuint?", record, texture);
    RDCASSERTMSG("Couldn't identify origtexture object. Unbound or bad GLuint?", origrecord,
                 origtexture);

    if(record == NULL || origrecord == NULL)
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureView(ser, texture, target, origtexture, internalformat, minlevel, numlevels,
                            minlayer, numlayers);

    record->AddChunk(scope.Get());
    record->AddParent(origrecord);
    record->viewSource = origrecord->GetResourceID();

    // illegal to re-type textures
    record->VerifyDataType(target);

    record->datatype = TextureBinding(target);

    // mark the underlying resource as dirty to avoid tracking dirty across
    // aliased resources etc.
    GetResourceManager()->MarkDirtyResource(origrecord->GetResourceID());
  }

  {
    ResourceId texId = GetResourceManager()->GetResID(TextureRes(GetCtx(), texture));
    ResourceId viewedId = GetResourceManager()->GetResID(TextureRes(GetCtx(), origtexture));

    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].view = true;
    m_Textures[texId].dimension = m_Textures[viewedId].dimension;
    m_Textures[texId].width = m_Textures[viewedId].width;
    m_Textures[texId].height = m_Textures[viewedId].height;
    m_Textures[texId].depth = numlayers;
    m_Textures[texId].curType = TextureTarget(target);
    m_Textures[texId].mipsValid = (1 << numlevels) - 1;
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenerateTextureMipmapEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glGenerateTextureMipmapEXT(texture.name, target);
    else
      GL.glGenerateTextureMipmap(texture.name);

    if(IsLoading(m_State))
    {
      AddEvent();

      // all mips are now valid
      ResourceId liveId = GetResourceManager()->GetResID(texture);
      uint32_t mips =
          CalcNumMips(m_Textures[liveId].width, m_Textures[liveId].height, m_Textures[liveId].depth);
      m_Textures[liveId].mipsValid = (1 << mips) - 1;

      ActionDescription action;
      action.flags |= ActionFlags::GenMips;

      AddAction(action);

      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::GenMips));
    }

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glGenerateTextureMipmapEXT(GLResourceRecord *record, GLenum target)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glGenerateTextureMipmapEXT(ser, record->Resource.name, target);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    ResourceId texId = record->GetResourceID();

    GetResourceManager()->MarkDirtyResource(texId);

    // all mips are now valid
    uint32_t mips =
        CalcNumMips(m_Textures[texId].width, m_Textures[texId].height, m_Textures[texId].depth);
    m_Textures[texId].mipsValid = (1 << mips) - 1;
  }
}

void WrappedOpenGL::glGenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glGenerateTextureMipmapEXT(texture, target));

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target);
}

void WrappedOpenGL::glGenerateTextureMipmap(GLuint texture)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glGenerateTextureMipmap(texture));

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE);
}

void WrappedOpenGL::glGenerateMipmap(GLenum target)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glGenerateMipmap(target));

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(GetCtxData().GetActiveTexRecord(target), target);
}

void WrappedOpenGL::glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glGenerateMultiTexMipmapEXT(texunit, target));

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(GetCtxData().GetTexUnitRecord(target, texunit), target);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glInvalidateTexImage(SerialiserType &ser, GLuint textureHandle,
                                                   GLint level)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(level);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glInvalidateTexImage(texture.name, level);

    ResourceId liveId = GetResourceManager()->GetResID(texture);

    if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
    {
      GLenum attach = eGL_COLOR_ATTACHMENT0;

      ResourceFormat fmt =
          MakeResourceFormat(m_Textures[liveId].curType, m_Textures[liveId].internalFormat);

      if(fmt.type != ResourceFormatType::Regular && fmt.type != ResourceFormatType::D16S8 &&
         fmt.type != ResourceFormatType::D24S8 && fmt.type != ResourceFormatType::D32S8 &&
         fmt.type != ResourceFormatType::S8 && fmt.type != ResourceFormatType::R10G10B10A2 &&
         fmt.type != ResourceFormatType::R11G11B10)
      {
        // we don't expect to be able to render to this format, so fill it manually
        GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, liveId, level);
      }
      else
      {
        GLenum base = GetBaseFormat(m_Textures[liveId].internalFormat);
        if(base == eGL_DEPTH_STENCIL)
          attach = eGL_DEPTH_STENCIL_ATTACHMENT;
        else if(base == eGL_DEPTH_COMPONENT)
          attach = eGL_DEPTH_ATTACHMENT;
        else if(base == eGL_STENCIL_INDEX)
          attach = eGL_STENCIL_ATTACHMENT;

        GLuint oldFB = 0;
        GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&oldFB);

        GLuint fb = 0;
        GL.glGenFramebuffers(1, &fb);
        GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fb);

        GLenum texTarget = m_Textures[liveId].curType;

        if(texTarget == eGL_TEXTURE_3D)
        {
          for(GLsizei z = 0; z < RDCMAX(1, m_Textures[liveId].depth >> level); z++)
          {
            GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texture.name, level, z);
            GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, 0, 0,
                                                65536, 65536);
          }
        }
        else if(texTarget == eGL_TEXTURE_2D_ARRAY || texTarget == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
                texTarget == eGL_TEXTURE_CUBE_MAP || texTarget == eGL_TEXTURE_CUBE_MAP_ARRAY)
        {
          GLsizei depth = m_Textures[liveId].depth;
          if(texTarget == eGL_TEXTURE_CUBE_MAP)
            depth *= 6;
          for(GLsizei z = 0; z < depth; z++)
          {
            GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texture.name, level, z);
            GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, 0, 0,
                                                65536, 65536);
          }
        }
        else if(texTarget == eGL_TEXTURE_2D || texTarget == eGL_TEXTURE_2D_MULTISAMPLE ||
                texTarget == eGL_TEXTURE_RECTANGLE)
        {
          GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attach, texTarget, texture.name, level);
          GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, 0, 0,
                                              65536, 65536);
        }
        else if(texTarget == eGL_TEXTURE_1D_ARRAY)
        {
          for(GLsizei z = 0; z < m_Textures[liveId].height; z++)
          {
            GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texture.name, level, z);
            GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, 0, 0,
                                                65536, 1);
          }
        }
        else if(texTarget == eGL_TEXTURE_1D)
        {
          GL.glFramebufferTexture1D(eGL_DRAW_FRAMEBUFFER, attach, texTarget, texture.name, level);
          GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, 0, 0,
                                              65536, 1);
        }

        GL.glDeleteFramebuffers(1, &fb);

        GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, oldFB);
      }
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      ActionDescription action;
      action.flags |= ActionFlags::Clear;

      action.copyDestination = GetResourceManager()->GetOriginalID(liveId);

      AddAction(action);

      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Discard));
    }
  }

  return true;
}

void WrappedOpenGL::glInvalidateTexImage(GLuint texture, GLint level)
{
  SERIALISE_TIME_CALL(GL.glInvalidateTexImage(texture, level));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
    RDCASSERTMSG("Couldn't identify texture object. Unbound or bad GLuint?", record, texture);

    if(!record)
      return;

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      ser.SetActionChunk();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glInvalidateTexImage(ser, texture, level);

      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_ReadBeforeWrite);
    }
    else if(IsBackgroundCapturing(m_State))
    {
      GetResourceManager()->MarkDirtyResource(record->Resource);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glInvalidateTexSubImage(SerialiserType &ser, GLuint textureHandle,
                                                      GLint level, GLint xoffset, GLint yoffset,
                                                      GLint zoffset, GLsizei width, GLsizei height,
                                                      GLsizei depth)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(zoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glInvalidateTexSubImage(texture.name, level, xoffset, yoffset, zoffset, width, height, depth);

    ResourceId liveId = GetResourceManager()->GetResID(texture);

    if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
    {
      GLenum attach = eGL_COLOR_ATTACHMENT0;

      ResourceFormat fmt =
          MakeResourceFormat(m_Textures[liveId].curType, m_Textures[liveId].internalFormat);

      if(fmt.type != ResourceFormatType::Regular && fmt.type != ResourceFormatType::D16S8 &&
         fmt.type != ResourceFormatType::D24S8 && fmt.type != ResourceFormatType::D32S8 &&
         fmt.type != ResourceFormatType::S8 && fmt.type != ResourceFormatType::R10G10B10A2 &&
         fmt.type != ResourceFormatType::R11G11B10)
      {
        // we don't expect to be able to render to this format, so fill it manually
        GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, liveId, level, xoffset,
                                            yoffset, zoffset, width, height, depth);
      }
      else
      {
        GLenum base = GetBaseFormat(m_Textures[liveId].internalFormat);
        if(base == eGL_DEPTH_STENCIL)
          attach = eGL_DEPTH_STENCIL_ATTACHMENT;
        else if(base == eGL_DEPTH_COMPONENT)
          attach = eGL_DEPTH_ATTACHMENT;
        else if(base == eGL_STENCIL_INDEX)
          attach = eGL_STENCIL_ATTACHMENT;

        GLuint oldFB = 0;
        GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&oldFB);

        GLuint fb = 0;
        GL.glGenFramebuffers(1, &fb);
        GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fb);

        GLenum texTarget = m_Textures[liveId].curType;

        if(texTarget == eGL_TEXTURE_3D || texTarget == eGL_TEXTURE_2D_ARRAY ||
           texTarget == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY || texTarget == eGL_TEXTURE_CUBE_MAP ||
           texTarget == eGL_TEXTURE_CUBE_MAP_ARRAY)
        {
          for(GLsizei z = 0; z < depth; z++)
          {
            GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texture.name, level,
                                         zoffset + z);
            GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach,
                                                xoffset, yoffset, width, height);
          }
        }
        else if(texTarget == eGL_TEXTURE_2D || texTarget == eGL_TEXTURE_2D_MULTISAMPLE ||
                texTarget == eGL_TEXTURE_RECTANGLE)
        {
          GL.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, attach, texTarget, texture.name, level);
          GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, xoffset,
                                              yoffset, width, height);
        }
        else if(texTarget == eGL_TEXTURE_1D_ARRAY)
        {
          for(GLsizei z = 0; z < height; z++)
          {
            GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texture.name, level,
                                         z + yoffset);
            GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach,
                                                xoffset, 0, width, 1);
          }
        }
        else if(texTarget == eGL_TEXTURE_1D)
        {
          GL.glFramebufferTexture1D(eGL_DRAW_FRAMEBUFFER, attach, texTarget, texture.name, level);
          GetReplay()->FillWithDiscardPattern(DiscardType::InvalidateCall, fb, 1, &attach, xoffset,
                                              0, width, 1);
        }

        GL.glDeleteFramebuffers(1, &fb);

        GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, oldFB);
      }
    }

    if(IsLoading(m_State))
    {
      AddEvent();

      ActionDescription action;
      action.flags |= ActionFlags::Clear;

      action.copyDestination = GetResourceManager()->GetOriginalID(liveId);

      AddAction(action);

      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::Discard));
    }
  }

  return true;
}

void WrappedOpenGL::glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset,
                                            GLint yoffset, GLint zoffset, GLsizei width,
                                            GLsizei height, GLsizei depth)
{
  SERIALISE_TIME_CALL(
      GL.glInvalidateTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
    RDCASSERTMSG("Couldn't identify texture object. Unbound or bad GLuint?", record, texture);

    if(!record)
      return;

    if(IsActiveCapturing(m_State))
    {
      USE_SCRATCH_SERIALISER();
      ser.SetActionChunk();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glInvalidateTexSubImage(ser, texture, level, xoffset, yoffset, zoffset, width,
                                        height, depth);

      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_ReadBeforeWrite);
    }
    else if(IsBackgroundCapturing(m_State))
    {
      GetResourceManager()->MarkDirtyResource(record->Resource);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyImageSubData(SerialiserType &ser, GLuint srcHandle,
                                                 GLenum srcTarget, GLint srcLevel, GLint srcX,
                                                 GLint srcY, GLint srcZ, GLuint dstHandle,
                                                 GLenum dstTarget, GLint dstLevel, GLint dstX,
                                                 GLint dstY, GLint dstZ, GLsizei srcWidth,
                                                 GLsizei srcHeight, GLsizei srcDepth)
{
  SERIALISE_ELEMENT_LOCAL(srcName, srcTarget == eGL_RENDERBUFFER
                                       ? RenderbufferRes(GetCtx(), srcHandle)
                                       : TextureRes(GetCtx(), srcHandle))
      .Important();
  SERIALISE_ELEMENT(srcTarget);
  SERIALISE_ELEMENT(srcLevel);
  SERIALISE_ELEMENT(srcX);
  SERIALISE_ELEMENT(srcY);
  SERIALISE_ELEMENT(srcZ);
  SERIALISE_ELEMENT_LOCAL(dstName, dstTarget == eGL_RENDERBUFFER
                                       ? RenderbufferRes(GetCtx(), dstHandle)
                                       : TextureRes(GetCtx(), dstHandle))
      .Important();
  SERIALISE_ELEMENT(dstTarget);
  SERIALISE_ELEMENT(dstLevel);
  SERIALISE_ELEMENT(dstX);
  SERIALISE_ELEMENT(dstY);
  SERIALISE_ELEMENT(dstZ);
  SERIALISE_ELEMENT(srcWidth);
  SERIALISE_ELEMENT(srcHeight);
  SERIALISE_ELEMENT(srcDepth);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glCopyImageSubData(srcName.name, srcTarget, srcLevel, srcX, srcY, srcZ, dstName.name,
                          dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);

    if(IsLoading(m_State))
    {
      AddEvent();

      ResourceId srcid = GetResourceManager()->GetResID(srcName);
      ResourceId dstid = GetResourceManager()->GetResID(dstName);

      ActionDescription action;
      action.flags |= ActionFlags::Copy;

      action.copySource = GetResourceManager()->GetOriginalID(srcid);
      action.copyDestination = GetResourceManager()->GetOriginalID(dstid);

      action.copyDestinationSubresource.mip = dstLevel;
      if(dstTarget != eGL_TEXTURE_3D)
        action.copyDestinationSubresource.slice = dstZ;

      action.copySourceSubresource.mip = srcLevel;
      if(srcTarget != eGL_TEXTURE_3D)
        action.copySourceSubresource.slice = srcZ;

      AddAction(action);

      if(srcid == dstid)
      {
        m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::Copy));
      }
      else
      {
        m_ResourceUses[srcid].push_back(EventUsage(m_CurEventID, ResourceUsage::CopySrc));
        m_ResourceUses[dstid].push_back(EventUsage(m_CurEventID, ResourceUsage::CopyDst));
      }
    }
  }

  return true;
}

void WrappedOpenGL::glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX,
                                       GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget,
                                       GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                                       GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
  CoherentMapImplicitBarrier();

  GLResource srcRes = srcTarget == eGL_RENDERBUFFER ? RenderbufferRes(GetCtx(), srcName)
                                                    : TextureRes(GetCtx(), srcName);
  GLResource dstRes = dstTarget == eGL_RENDERBUFFER ? RenderbufferRes(GetCtx(), dstName)
                                                    : TextureRes(GetCtx(), dstName);

  if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *dstrecord = GetResourceManager()->GetResourceRecord(dstRes);

    if(dstrecord)
      GetResourceManager()->MarkResourceFrameReferenced(dstrecord->GetResourceID(),
                                                        eFrameRef_PartialWrite);
  }

  SERIALISE_TIME_CALL(GL.glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName,
                                            dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth,
                                            srcHeight, srcDepth));

  if(IsActiveCapturing(m_State))
  {
    GLResourceRecord *srcrecord = GetResourceManager()->GetResourceRecord(srcRes);
    GLResourceRecord *dstrecord = GetResourceManager()->GetResourceRecord(dstRes);

    RDCASSERTMSG("Couldn't identify src texture. Unbound or bad GLuint?", srcrecord, srcName);
    RDCASSERTMSG("Couldn't identify dst texture. Unbound or bad GLuint?", dstrecord, dstName);

    if(srcrecord == NULL || dstrecord == NULL)
      return;

    USE_SCRATCH_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyImageSubData(ser, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName,
                                 dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight,
                                 srcDepth);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(dstrecord->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(dstrecord->GetResourceID(),
                                                      eFrameRef_PartialWrite);
    GetResourceManager()->MarkResourceFrameReferenced(srcrecord->GetResourceID(), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GLResourceRecord *srcrecord = GetResourceManager()->GetResourceRecord(srcRes);
    GLResourceRecord *dstrecord = GetResourceManager()->GetResourceRecord(dstRes);

    GetResourceManager()->MarkDirtyResource(dstrecord->GetResourceID());

    // copy over compressed data, if it exists
    if(IsGLES)
    {
      TextureData &srcData = m_Textures[srcrecord->GetResourceID()];
      TextureData &dstData = m_Textures[dstrecord->GetResourceID()];

      bool dstIsCompressed = IsCompressedFormat(dstData.internalFormat);

      // only need dst's compressedData
      if(dstIsCompressed)
      {
        bool srcIsCompressed = IsCompressedFormat(srcData.internalFormat);
        GLenum srcFmt = srcIsCompressed ? eGL_NONE : GetBaseFormat(srcData.internalFormat);
        GLenum srcType = srcIsCompressed ? eGL_NONE : GetDataType(srcData.internalFormat);
        rdcfixedarray<uint32_t, 3> srcBlockSize =
            srcIsCompressed ? GetCompressedBlockSize(srcData.internalFormat)
                            : rdcfixedarray<uint32_t, 3>{1u, 1u, 1u};
        GLsizei srcLevelWidth = RDCMAX(1, srcData.width >> srcLevel);
        GLsizei srcLevelHeight = (srcData.curType != eGL_TEXTURE_1D_ARRAY)
                                     ? RDCMAX(1, srcData.height >> srcLevel)
                                     : srcData.height;
        GLsizei srcLevelDepth = (srcData.curType != eGL_TEXTURE_2D_ARRAY &&
                                 srcData.curType != eGL_TEXTURE_CUBE_MAP_ARRAY)
                                    ? RDCMAX(1, srcData.depth >> srcLevel)
                                    : srcData.depth;
        size_t srcSize =
            srcIsCompressed
                ? GetCompressedByteSize(srcLevelWidth, srcLevelHeight, srcLevelDepth,
                                        srcData.internalFormat)
                : GetByteSize(srcLevelWidth, srcLevelHeight, srcLevelDepth, srcFmt, srcType);

        rdcfixedarray<uint32_t, 3> dstBlockSize = GetCompressedBlockSize(dstData.internalFormat);
        GLsizei dstLevelWidth = RDCMAX(1, dstData.width >> dstLevel);
        GLsizei dstLevelHeight = (dstData.curType != eGL_TEXTURE_1D_ARRAY)
                                     ? RDCMAX(1, dstData.height >> dstLevel)
                                     : dstData.height;
        GLsizei dstLevelDepth = (dstData.curType != eGL_TEXTURE_2D_ARRAY &&
                                 dstData.curType != eGL_TEXTURE_CUBE_MAP_ARRAY)
                                    ? RDCMAX(1, dstData.depth >> dstLevel)
                                    : dstData.depth;
        size_t dstSize = GetCompressedByteSize(dstLevelWidth, dstLevelHeight, dstLevelDepth,
                                               dstData.internalFormat);

        bytebuf *srcCdPtr = NULL;
        bytebuf tempCd;
        // if we have source compressed data to copy
        if(srcData.compressedData.find(srcLevel) != srcData.compressedData.end())
        {
          srcCdPtr = &srcData.compressedData[srcLevel];
        }
        else if(!srcIsCompressed)
        {
          if(srcData.curType == eGL_TEXTURE_2D || srcData.curType == eGL_TEXTURE_2D_ARRAY)
          {
            // try reading back without existing compressedData
            RDCASSERT(!srcIsCompressed);

            tempCd.resize(srcSize);
            srcCdPtr = &tempCd;

            GLuint packbuf = 0;
            GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&packbuf);
            PixelPackState pack;
            pack.Fetch(false);

            GLuint prevTex = 0;
            GL.glGetIntegerv(TextureBinding(srcData.curType), (GLint *)&prevTex);

            GLenum oldActive = eGL_TEXTURE0;
            GL.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&oldActive);
            GL.glActiveTexture(eGL_TEXTURE0);

            GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
            ResetPixelPackState(false, 1);

            GL.glBindTexture(srcData.curType, srcName);
            GL.glGetTexImage(srcData.curType, srcLevel, srcFmt, srcType, tempCd.data());

            GL.glBindTexture(srcData.curType, prevTex);
            GL.glActiveTexture(oldActive);

            if(packbuf)
              GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, packbuf);
            pack.Apply(false);
          }
          else
          {
            RDCLOG("Unsupported format %d to copy from in glCopyImageSubData", srcData.curType);
          }
        }
        if(srcCdPtr)
        {
          RDCASSERT(srcWidth % srcBlockSize[0] == 0);
          RDCASSERT(srcHeight % srcBlockSize[1] == 0);
          RDCASSERT(srcDepth % srcBlockSize[2] == 0);
          RDCASSERT(srcX % srcBlockSize[0] == 0);
          RDCASSERT(srcY % srcBlockSize[1] == 0);
          RDCASSERT(srcZ % srcBlockSize[2] == 0);
          RDCASSERT(dstX % dstBlockSize[0] == 0);
          RDCASSERT(dstY % dstBlockSize[1] == 0);
          RDCASSERT(dstZ % dstBlockSize[2] == 0);
          // copy full texture rather than subregion
          if(srcX == 0 && srcY == 0 && srcZ == 0 && dstX == 0 && dstY == 0 && dstZ == 0 &&
             srcLevelWidth == srcWidth && srcLevelHeight == srcHeight && srcLevelDepth == srcDepth &&
             // equal dimension after normalising for blocks/texels
             (srcLevelWidth / srcBlockSize[0] == dstLevelWidth / dstBlockSize[0]) &&
             (srcLevelHeight / srcBlockSize[1] == dstLevelHeight / dstBlockSize[1]) &&
             (srcLevelDepth / srcBlockSize[2] == dstLevelDepth / dstBlockSize[2]) &&
             // compatible size across formats
             srcSize == dstSize)
          {
            // fast path when perform full copy
            dstData.compressedData[dstLevel] = *srcCdPtr;
          }
          else
          {
            bytebuf &dstCd = dstData.compressedData[dstLevel];

            size_t srcSliceSize =
                srcIsCompressed
                    ? GetCompressedByteSize(srcLevelWidth, srcLevelHeight, srcBlockSize[2],
                                            srcData.internalFormat)
                    : GetByteSize(srcLevelWidth, srcLevelHeight, srcBlockSize[2], srcFmt, srcType);
            size_t dstSliceSize = GetCompressedByteSize(dstLevelWidth, dstLevelHeight,
                                                        dstBlockSize[2], dstData.internalFormat);

            size_t srcRowSize =
                srcIsCompressed
                    ? GetCompressedByteSize(srcLevelWidth, (GLsizei)srcBlockSize[1],
                                            (GLsizei)srcBlockSize[2], srcData.internalFormat)
                    : GetByteSize(srcLevelWidth, srcBlockSize[1], srcBlockSize[2], srcFmt, srcType);
            size_t dstRowSize =
                GetCompressedByteSize(dstLevelWidth, (GLsizei)dstBlockSize[1],
                                      (GLsizei)dstBlockSize[2], dstData.internalFormat);

            size_t srcStartOffset =
                srcIsCompressed
                    ? GetCompressedByteSize(srcX, (GLsizei)srcBlockSize[1],
                                            (GLsizei)srcBlockSize[2], srcData.internalFormat)
                    : GetByteSize(srcX, srcBlockSize[1], srcBlockSize[2], srcFmt, srcType);
            size_t dstStartOffset = GetCompressedByteSize(
                dstX, (GLsizei)dstBlockSize[1], (GLsizei)dstBlockSize[2], dstData.internalFormat);

            size_t blockSize =
                srcIsCompressed
                    ? GetCompressedByteSize(srcWidth, (GLsizei)srcBlockSize[1],
                                            (GLsizei)srcBlockSize[2], srcData.internalFormat)
                    : GetByteSize(srcWidth, srcBlockSize[1], srcBlockSize[2], srcFmt, srcType);

            for(size_t z = 0; z < (size_t)srcDepth; z += srcBlockSize[2])
            {
              size_t srcOffset = srcSliceSize * ((srcZ + z) / (GLsizei)srcBlockSize[2]) +
                                 srcRowSize * (srcY / (GLsizei)srcBlockSize[1]) + srcStartOffset;
              size_t dstOffset = dstSliceSize * ((dstZ + z) / (GLsizei)dstBlockSize[2]) +
                                 dstRowSize * (dstY / (GLsizei)dstBlockSize[1]) + dstStartOffset;
              for(size_t y = 0; y < (size_t)srcHeight; y += srcBlockSize[1])
              {
                RDCASSERT(srcCdPtr->size() >= srcOffset + blockSize);
                if(dstCd.size() < dstOffset + blockSize)
                  dstCd.resize(dstOffset + blockSize);
                memcpy(dstCd.data() + dstOffset, srcCdPtr->data() + srcOffset, blockSize);
                srcOffset += srcRowSize;
                dstOffset += dstRowSize;
              }
            }
          }
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureSubImage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target, GLint level, GLint xoffset,
                                                         GLint x, GLint y, GLsizei width)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glCopyTextureSubImage1DEXT(texture.name, target, level, xoffset, x, y, width);
    else
      GL.glCopyTextureSubImage1D(texture.name, level, xoffset, x, y, width);
  }

  return true;
}

void WrappedOpenGL::Common_glCopyTextureSubImage1DEXT(GLResourceRecord *record, GLenum target,
                                                      GLint level, GLint xoffset, GLint x, GLint y,
                                                      GLsizei width)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureSubImage1DEXT(ser, record->Resource.name, target, level, xoffset, x, y,
                                         width);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_PartialWrite);
  }
}

void WrappedOpenGL::glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level,
                                               GLint xoffset, GLint x, GLint y, GLsizei width)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTextureSubImage1DEXT(texture, target, level, xoffset, x, y, width));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, x, y, width);
}

void WrappedOpenGL::glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x,
                                            GLint y, GLsizei width)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTextureSubImage1D(texture, level, xoffset, x, y, width));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, x, y, width);
}

void WrappedOpenGL::glCopyMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                                GLint xoffset, GLint x, GLint y, GLsizei width)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyMultiTexSubImage1DEXT(texunit, target, level, xoffset, x, y, width));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                      xoffset, x, y, width);
}

void WrappedOpenGL::glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y,
                                        GLsizei width)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTexSubImage1D(target, level, xoffset, x, y, width));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(GetCtxData().GetActiveTexRecord(target), eGL_NONE, level,
                                      xoffset, x, y, width);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureSubImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target, GLint level, GLint xoffset,
                                                         GLint yoffset, GLint x, GLint y,
                                                         GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glCopyTextureSubImage2DEXT(texture.name, target, level, xoffset, yoffset, x, y, width,
                                    height);
    else
      GL.glCopyTextureSubImage2D(texture.name, level, xoffset, yoffset, x, y, width, height);
  }

  return true;
}

void WrappedOpenGL::Common_glCopyTextureSubImage2DEXT(GLResourceRecord *record, GLenum target,
                                                      GLint level, GLint xoffset, GLint yoffset,
                                                      GLint x, GLint y, GLsizei width, GLsizei height)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureSubImage2DEXT(ser, record->Resource.name, target, level, xoffset,
                                         yoffset, x, y, width, height);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_PartialWrite);
  }
}

void WrappedOpenGL::glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLint x, GLint y,
                                               GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCopyTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                            GLint x, GLint y, GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCopyTextureSubImage2D(texture, level, xoffset, yoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint x, GLint y,
                                                GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, x, y,
                                                     width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                      xoffset, yoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                        GLint x, GLint y, GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                      xoffset, yoffset, x, y, width, height);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureSubImage3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target, GLint level, GLint xoffset,
                                                         GLint yoffset, GLint zoffset, GLint x,
                                                         GLint y, GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(zoffset);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glCopyTextureSubImage3DEXT(texture.name, target, level, xoffset, yoffset, zoffset, x, y,
                                    width, height);
    else
      GL.glCopyTextureSubImage3D(texture.name, level, xoffset, yoffset, zoffset, x, y, width, height);
  }

  return true;
}

void WrappedOpenGL::Common_glCopyTextureSubImage3DEXT(GLResourceRecord *record, GLenum target,
                                                      GLint level, GLint xoffset, GLint yoffset,
                                                      GLint zoffset, GLint x, GLint y,
                                                      GLsizei width, GLsizei height)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureSubImage3DEXT(ser, record->Resource.name, target, level, xoffset,
                                         yoffset, zoffset, x, y, width, height);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_PartialWrite);
  }
}

void WrappedOpenGL::glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                               GLint y, GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTextureSubImage3DEXT(texture, target, level, xoffset, yoffset,
                                                    zoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, zoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset,
                                            GLint yoffset, GLint zoffset, GLint x, GLint y,
                                            GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCopyTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, zoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint zoffset,
                                                GLint x, GLint y, GLsizei width, GLsizei height)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset,
                                                     zoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                      xoffset, yoffset, zoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLint x, GLint y, GLsizei width,
                                        GLsizei height)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height));

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                      xoffset, yoffset, zoffset, x, y, width, height);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameteriEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname).Important();

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(pname == GL_DEPTH_STENCIL_TEXTURE_MODE || pname == GL_TEXTURE_COMPARE_FUNC ||
     pname == GL_TEXTURE_COMPARE_MODE || pname == GL_TEXTURE_MIN_FILTER ||
     pname == GL_TEXTURE_MAG_FILTER || pname == GL_TEXTURE_SWIZZLE_R ||
     pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A ||
     pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R)
  {
    SERIALISE_ELEMENT_TYPED(GLenum, param).Important();
  }
  else
  {
    SERIALISE_ELEMENT(param).Important();
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glTextureParameteriEXT(texture.name, target, pname, param);
    else
      GL.glTextureParameteri(texture.name, pname, param);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureParameteriEXT(GLResourceRecord *record, GLenum target,
                                                  GLenum pname, GLint param)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
     IsBackgroundCapturing(m_State))
    return;

  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(param == eGL_CLAMP)
    param = eGL_CLAMP_TO_EDGE;

  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(gl_CurChunk);
  Serialise_glTextureParameteriEXT(ser, record->Resource.name, target, pname, param);

  if(IsActiveCapturing(m_State))
  {
    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else
  {
    record->AddChunk(scope.Get());
    record->UpdateCount++;

    if(record->UpdateCount > 12)
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glTextureParameteri(GLuint texture, GLenum pname, GLint param)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameteri(texture, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        param);
}

void WrappedOpenGL::glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameteriEXT(texture, target, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname, param);
}

void WrappedOpenGL::glTexParameteri(GLenum target, GLenum pname, GLint param)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTexParameteri(target, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(GetCtxData().GetActiveTexRecord(target), target, pname, param);
}

void WrappedOpenGL::glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexParameteriEXT(texunit, target, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, pname,
                                  param);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterivEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLenum pname,
                                                      const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname).Important();
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname)).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glTextureParameterivEXT(texture.name, target, pname, params);
    else
      GL.glTextureParameteriv(texture.name, pname, params);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureParameterivEXT(GLResourceRecord *record, GLenum target,
                                                   GLenum pname, const GLint *params)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  if(IsBackgroundCapturing(m_State) &&
     m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end())
    return;

  GLint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(gl_CurChunk);
  Serialise_glTextureParameterivEXT(ser, record->Resource.name, target, pname, params);

  if(IsActiveCapturing(m_State))
  {
    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else
  {
    record->AddChunk(scope.Get());
    record->UpdateCount++;

    if(record->UpdateCount > 12)
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname,
                                            const GLint *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterivEXT(texture, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameteriv(GLuint texture, GLenum pname, const GLint *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameteriv(texture, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTexParameteriv(target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(GetCtxData().GetActiveTexRecord(target), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                             const GLint *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexParameterivEXT(texunit, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, pname,
                                   params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterIivEXT(SerialiserType &ser, GLuint textureHandle,
                                                       GLenum target, GLenum pname,
                                                       const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname).Important();
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname)).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glTextureParameterIivEXT(texture.name, target, pname, params);
    else
      GL.glTextureParameterIiv(texture.name, pname, params);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureParameterIivEXT(GLResourceRecord *record, GLenum target,
                                                    GLenum pname, const GLint *params)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
     IsBackgroundCapturing(m_State))
    return;

  GLint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(gl_CurChunk);
  Serialise_glTextureParameterIivEXT(ser, record->Resource.name, target, pname, params);

  if(IsActiveCapturing(m_State))
  {
    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else
  {
    record->AddChunk(scope.Get());
    record->UpdateCount++;

    if(record->UpdateCount > 12)
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname,
                                             const GLint *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterIivEXT(texture, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameterIiv(GLuint texture, GLenum pname, const GLint *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterIiv(texture, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameterIiv(GLenum target, GLenum pname, const GLint *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTexParameterIiv(target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(GetCtxData().GetActiveTexRecord(target), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                              const GLint *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexParameterIivEXT(texunit, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, pname,
                                    params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterIuivEXT(SerialiserType &ser, GLuint textureHandle,
                                                        GLenum target, GLenum pname,
                                                        const GLuint *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname).Important();
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname)).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glTextureParameterIuivEXT(texture.name, target, pname, params);
    else
      GL.glTextureParameterIuiv(texture.name, pname, params);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureParameterIuivEXT(GLResourceRecord *record, GLenum target,
                                                     GLenum pname, const GLuint *params)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
     IsBackgroundCapturing(m_State))
    return;

  GLuint clamptoedge[4] = {eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == eGL_CLAMP)
    params = clamptoedge;

  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(gl_CurChunk);
  Serialise_glTextureParameterIuivEXT(ser, record->Resource.name, target, pname, params);

  if(IsActiveCapturing(m_State))
  {
    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else
  {
    record->AddChunk(scope.Get());
    record->UpdateCount++;

    if(record->UpdateCount > 12)
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname,
                                              const GLuint *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterIuivEXT(texture, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterIuiv(texture, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTexParameterIuiv(target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(GetCtxData().GetActiveTexRecord(target), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                               const GLuint *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexParameterIuivEXT(texunit, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, pname,
                                     params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterfEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname).Important();
  SERIALISE_ELEMENT(param).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glTextureParameterfEXT(texture.name, target, pname, param);
    else
      GL.glTextureParameterf(texture.name, pname, param);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureParameterfEXT(GLResourceRecord *record, GLenum target,
                                                  GLenum pname, GLfloat param)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
     IsBackgroundCapturing(m_State))
    return;

  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(param == (float)eGL_CLAMP)
    param = (float)eGL_CLAMP_TO_EDGE;

  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(gl_CurChunk);
  Serialise_glTextureParameterfEXT(ser, record->Resource.name, target, pname, param);

  if(IsActiveCapturing(m_State))
  {
    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else
  {
    record->AddChunk(scope.Get());
    record->UpdateCount++;

    if(record->UpdateCount > 12)
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterfEXT(texture, target, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname, param);
}

void WrappedOpenGL::glTextureParameterf(GLuint texture, GLenum pname, GLfloat param)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterf(texture, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        param);
}

void WrappedOpenGL::glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTexParameterf(target, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(GetCtxData().GetActiveTexRecord(target), target, pname, param);
}

void WrappedOpenGL::glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexParameterfEXT(texunit, target, pname, param));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, pname,
                                  param);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterfvEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLenum pname,
                                                      const GLfloat *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname).Important();
  SERIALISE_ELEMENT_ARRAY(params, numParams(pname)).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      GL.glTextureParameterfvEXT(texture.name, target, pname, params);
    else
      GL.glTextureParameterfv(texture.name, pname, params);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureParameterfvEXT(GLResourceRecord *record, GLenum target,
                                                   GLenum pname, const GLfloat *params)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
     IsBackgroundCapturing(m_State))
    return;

  GLfloat clamptoedge[4] = {(float)eGL_CLAMP_TO_EDGE};
  // CLAMP isn't supported (border texels gone), assume they meant CLAMP_TO_EDGE
  if(*params == (float)eGL_CLAMP)
    params = clamptoedge;

  USE_SCRATCH_SERIALISER();
  SCOPED_SERIALISE_CHUNK(gl_CurChunk);
  Serialise_glTextureParameterfvEXT(ser, record->Resource.name, target, pname, params);

  if(IsActiveCapturing(m_State))
  {
    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_ReadBeforeWrite);
  }
  else
  {
    record->AddChunk(scope.Get());
    record->UpdateCount++;

    if(record->UpdateCount > 12)
    {
      m_HighTrafficResources.insert(record->GetResourceID());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
}

void WrappedOpenGL::glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname,
                                            const GLfloat *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterfvEXT(texture, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *params)
{
  MarkReferencedWhileCapturing(GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTextureParameterfv(texture, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glTexParameterfv(target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(GetCtxData().GetActiveTexRecord(target), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                             const GLfloat *params)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_ReadBeforeWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexParameterfvEXT(texunit, target, pname, params));

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, pname,
                                   params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPixelStorei(SerialiserType &ser, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GL.glPixelStorei(pname, param);
  }

  return true;
}

void WrappedOpenGL::glPixelStorei(GLenum pname, GLint param)
{
  SERIALISE_TIME_CALL(GL.glPixelStorei(pname, param));

  // except for capturing frames we ignore this and embed the relevant
  // parameters in the chunks that reference them.
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPixelStorei(ser, pname, param);

    GetContextRecord()->AddChunk(scope.Get());
  }
}

void WrappedOpenGL::glPixelStoref(GLenum pname, GLfloat param)
{
  glPixelStorei(pname, (GLint)param);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glActiveTexture(SerialiserType &ser, GLenum texture)
{
  SERIALISE_ELEMENT(texture);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
    GL.glActiveTexture(texture);

  return true;
}

void WrappedOpenGL::glActiveTexture(GLenum texture)
{
  SERIALISE_TIME_CALL(GL.glActiveTexture(texture));

  GetCtxData().m_TextureUnit = texture - eGL_TEXTURE0;

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glActiveTexture(ser, texture);

      chunk = scope.Get();
    }

    GetContextRecord()->AddChunk(chunk);
  }
}

#pragma region Texture Creation(old glTexImage)

// note that we don't support/handle sourcing data from pixel unpack buffers. For the glTexImage*
// functions which
// create & source data, we will just set the pixel pointer to NULL (which means the serialise
// functions skip it)
// so that the image is created in the right format, then immediately mark the texture as dirty so
// we can fetch
// the actual contents. glTexSubImage* compressed or not we just skip if there's an unpack buffer
// bound.
// for glCompressedImage* we can't pass NULL as the pixel pointer to create, so instead we just have
// a scratch empty
// buffer that we use and resize, then the contents will be overwritten by the initial contents that
// are fetched.

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureImage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                  GLenum target, GLint level, GLint internalformat,
                                                  GLsizei width, GLint border, GLenum format,
                                                  GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT_TYPED(GLenum, internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(border);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(!unpack.FastPath(width, 0, 0, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, 0, 0, format, type);
  }

  size_t subimageSize = GetByteSize(width, 1, 1, format, type);

  SERIALISE_ELEMENT_ARRAY(pixels, subimageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum intFmt = (GLenum)internalformat;
    bool emulated = EmulateLuminanceFormat(texture.name, target, intFmt, format);
    internalformat = intFmt;

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 1;
      m_Textures[liveId].internalFormat = (GLenum)internalformat;
      m_Textures[liveId].initFormatHint = format;
      m_Textures[liveId].initTypeHint = type;
      m_Textures[liveId].emulated = emulated;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    GLint align = 1;
    GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

    PixelUnpackState unpack;
    if(pixels)
    {
      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
      RDCASSERT(!unpackbuf);
    }

    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    GL.glTextureImage1DEXT(texture.name, target, level, internalformat, width, border, format, type,
                           pixels);

    if(unpackbuf)
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    if(pixels)
      unpack.Apply(false);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureImage1DEXT(ResourceId texId, GLenum target, GLint level,
                                               GLint internalformat, GLsizei width, GLint border,
                                               GLenum format, GLenum type, const void *pixels)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  bool fromunpackbuf = false;
  {
    GLint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(IsBackgroundCapturing(m_State) && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glTextureImage1DEXT(ser, record->Resource.name, target, level, internalformat,
                                    width, border, format, type, fromunpackbuf ? NULL : pixels);

      Chunk *chunk = scope.Get();
      record->AddChunk(chunk);

      // if we're actively capturing this may be a creation but it may be a re-initialise. Insert
      // the chunk here as well to ensure consistent replay
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = (GLenum)internalformat;
    m_Textures[texId].initFormatHint = format;
    m_Textures[texId].initTypeHint = type;
  }
}

void WrappedOpenGL::glTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                        GLint internalformat, GLsizei width, GLint border,
                                        GLenum format, GLenum type, const void *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  SERIALISE_TIME_CALL(GL.glTextureImage1DEXT(texture, target, level, internalformat, width, border,
                                             format, type, pixels));

  Common_glTextureImage1DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), target,
                             level, internalformat, width, border, format, type, pixels);
}

void WrappedOpenGL::glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glTexImage1D(target, level, internalformat, width, border, format, type, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureImage1DEXT(record->GetResourceID(), target, level, internalformat, width,
                                 border, format, type, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                         GLint internalformat, GLsizei width, GLint border,
                                         GLenum format, GLenum type, const GLvoid *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  SERIALISE_TIME_CALL(GL.glMultiTexImage1DEXT(texunit, target, level, internalformat, width, border,
                                              format, type, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glTextureImage1DEXT(record->GetResourceID(), target, level, internalformat, width,
                                 border, format, type, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                  GLenum target, GLint level, GLint internalformat,
                                                  GLsizei width, GLsizei height, GLint border,
                                                  GLenum format, GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT_TYPED(GLenum, internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(border);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(!unpack.FastPath(width, 0, 0, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, height, 0, format, type);
  }

  size_t subimageSize = GetByteSize(width, height, 1, format, type);

  SERIALISE_ELEMENT_ARRAY(pixels, subimageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum intFmt = (GLenum)internalformat;
    bool emulated = EmulateLuminanceFormat(texture.name, target, intFmt, format);
    internalformat = intFmt;

    ResourceId liveId = GetResourceManager()->GetResID(texture);

    uint32_t mipsValid = m_Textures[liveId].mipsValid;
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = (GLenum)internalformat;
      m_Textures[liveId].initFormatHint = format;
      m_Textures[liveId].initTypeHint = type;
      m_Textures[liveId].emulated = emulated;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    GLint align = 1;
    GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

    PixelUnpackState unpack;
    if(pixels)
    {
      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
      RDCASSERT(!unpackbuf);
    }

    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    if(TextureBinding(target) == eGL_TEXTURE_BINDING_CUBE_MAP &&
       mipsValid != m_Textures[liveId].mipsValid)
    {
      GLenum ts[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      // special case handling for cubemaps, as we might have skipped the 'allocation' teximage
      // chunks to avoid serialising tons of 'data upload' teximage chunks. Sigh.
      // Any further chunks & initial data can overwrite this, but cubemaps must be square so all
      // parameters will be the same.
      for(size_t i = 0; i < ARRAY_COUNT(ts); i++)
      {
        GL.glTextureImage2DEXT(texture.name, ts[i], level, internalformat, width, height, border,
                               format, type, pixels);
      }
    }
    else
    {
      GL.glTextureImage2DEXT(texture.name, target, level, internalformat, width, height, border,
                             format, type, pixels);
    }

    if(unpackbuf)
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    if(pixels)
      unpack.Apply(false);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureImage2DEXT(ResourceId texId, GLenum target, GLint level,
                                               GLint internalformat, GLsizei width, GLsizei height,
                                               GLint border, GLenum format, GLenum type,
                                               const void *pixels)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  bool fromunpackbuf = false;
  {
    GLint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(IsBackgroundCapturing(m_State) && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glTextureImage2DEXT(ser, record->Resource.name, target, level, internalformat, width,
                                    height, border, format, type, fromunpackbuf ? NULL : pixels);

      Chunk *chunk = scope.Get();
      record->AddChunk(chunk);

      // if we're actively capturing this may be a creation but it may be a re-initialise. Insert
      // the chunk here as well to ensure consistent replay
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = (GLenum)internalformat;
    m_Textures[texId].initFormatHint = format;
    m_Textures[texId].initTypeHint = type;
  }
}

void WrappedOpenGL::glTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                        GLint internalformat, GLsizei width, GLsizei height,
                                        GLint border, GLenum format, GLenum type, const void *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  SERIALISE_TIME_CALL(GL.glTextureImage2DEXT(texture, target, level, internalformat, width, height,
                                             border, format, type, pixels));

  Common_glTextureImage2DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), target,
                             level, internalformat, width, height, border, format, type, pixels);
}

void WrappedOpenGL::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLsizei height, GLint border, GLenum format, GLenum type,
                                 const GLvoid *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureImage2DEXT(record->GetResourceID(), target, level, internalformat, width,
                                 height, border, format, type, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                         GLint internalformat, GLsizei width, GLsizei height,
                                         GLint border, GLenum format, GLenum type,
                                         const GLvoid *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  SERIALISE_TIME_CALL(GL.glMultiTexImage2DEXT(texunit, target, level, internalformat, width, height,
                                              border, format, type, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glTextureImage2DEXT(record->GetResourceID(), target, level, internalformat, width,
                                 height, border, format, type, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureImage3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                  GLenum target, GLint level, GLint internalformat,
                                                  GLsizei width, GLsizei height, GLsizei depth,
                                                  GLint border, GLenum format, GLenum type,
                                                  const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT_TYPED(GLenum, internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(border);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(!unpack.FastPath(width, height, depth, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, height, depth, format, type);
  }

  size_t subimageSize = GetByteSize(width, height, depth, format, type);

  SERIALISE_ELEMENT_ARRAY(pixels, subimageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum intFmt = (GLenum)internalformat;
    bool emulated = EmulateLuminanceFormat(texture.name, target, intFmt, format);
    internalformat = intFmt;

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = depth;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 3;
      m_Textures[liveId].internalFormat = (GLenum)internalformat;
      m_Textures[liveId].initFormatHint = format;
      m_Textures[liveId].initTypeHint = type;
      m_Textures[liveId].emulated = emulated;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    GLint align = 1;
    GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

    PixelUnpackState unpack;
    if(pixels)
    {
      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
      RDCASSERT(!unpackbuf);
    }

    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    GL.glTextureImage3DEXT(texture.name, target, level, internalformat, width, height, depth,
                           border, format, type, pixels);

    if(unpackbuf)
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    if(pixels)
      unpack.Apply(false);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureImage3DEXT(ResourceId texId, GLenum target, GLint level,
                                               GLint internalformat, GLsizei width, GLsizei height,
                                               GLsizei depth, GLint border, GLenum format,
                                               GLenum type, const void *pixels)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  bool fromunpackbuf = false;
  {
    GLint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(IsBackgroundCapturing(m_State) && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].depth == depth &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glTextureImage3DEXT(ser, record->Resource.name, target, level, internalformat,
                                    width, height, depth, border, format, type,
                                    fromunpackbuf ? NULL : pixels);

      Chunk *chunk = scope.Get();
      record->AddChunk(chunk);

      // if we're actively capturing this may be a creation but it may be a re-initialise. Insert
      // the chunk here as well to ensure consistent replay
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = (GLenum)internalformat;
    m_Textures[texId].initFormatHint = format;
    m_Textures[texId].initTypeHint = type;
  }
}

void WrappedOpenGL::glTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                        GLint internalformat, GLsizei width, GLsizei height,
                                        GLsizei depth, GLint border, GLenum format, GLenum type,
                                        const void *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  SERIALISE_TIME_CALL(GL.glTextureImage3DEXT(texture, target, level, internalformat, width, height,
                                             depth, border, format, type, pixels));

  Common_glTextureImage3DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), target,
                             level, internalformat, width, height, depth, border, format, type,
                             pixels);
}

void WrappedOpenGL::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLsizei height, GLsizei depth, GLint border, GLenum format,
                                 GLenum type, const GLvoid *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTexImage3D(target, level, internalformat, width, height, depth, border,
                                      format, type, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureImage3DEXT(record->GetResourceID(), target, level, internalformat, width,
                                 height, depth, border, format, type, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                         GLint internalformat, GLsizei width, GLsizei height,
                                         GLsizei depth, GLint border, GLenum format, GLenum type,
                                         const GLvoid *pixels)
{
  internalformat = RemapGenericCompressedFormat(internalformat);

  SERIALISE_TIME_CALL(GL.glMultiTexImage3DEXT(texunit, target, level, internalformat, width, height,
                                              depth, border, format, type, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glTextureImage3DEXT(record->GetResourceID(), target, level, internalformat, width,
                                 height, depth, border, format, type, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureImage1DEXT(SerialiserType &ser,
                                                            GLuint textureHandle, GLenum target,
                                                            GLint level, GLenum internalformat,
                                                            GLsizei width, GLint border,
                                                            GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(border);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    if(!unpack.FastPathCompressed(width, 0, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, 0, 0, imageSize);
  }

  SERIALISE_ELEMENT(imageSize);
  SERIALISE_ELEMENT_ARRAY(pixels, imageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    const void *databuf = pixels;

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(pixels == NULL)
    {
      if(m_ScratchBuf.size() < (size_t)imageSize)
        m_ScratchBuf.resize(imageSize);
      databuf = m_ScratchBuf.data();
    }

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 1;
      m_Textures[liveId].internalFormat = internalformat;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    GLint align = 1;
    GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    PixelUnpackState unpack;
    if(pixels)
    {
      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
      RDCASSERT(!unpackbuf);
    }

    GL.glCompressedTextureImage1DEXT(texture.name, target, level, internalformat, width, border,
                                     imageSize, databuf);

    if(unpackbuf)
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    if(pixels)
      unpack.Apply(false);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glCompressedTextureImage1DEXT(ResourceId texId, GLenum target,
                                                         GLint level, GLenum internalformat,
                                                         GLsizei width, GLint border,
                                                         GLsizei imageSize, const GLvoid *pixels)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  bool fromunpackbuf = false;
  {
    GLint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(IsBackgroundCapturing(m_State) && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCompressedTextureImage1DEXT(ser, record->Resource.name, target, level,
                                              internalformat, width, border, imageSize,
                                              fromunpackbuf ? NULL : pixels);

      Chunk *chunk = scope.Get();
      record->AddChunk(chunk);

      // if we're actively capturing this may be a creation but it may be a re-initialise. Insert
      // the chunk here as well to ensure consistent replay
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                                  GLenum internalformat, GLsizei width, GLint border,
                                                  GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedTextureImage1DEXT(texture, target, level, internalformat,
                                                       width, border, imageSize, pixels));

  Common_glCompressedTextureImage1DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                       target, level, internalformat, width, border, imageSize,
                                       pixels);
}

void WrappedOpenGL::glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLint border, GLsizei imageSize,
                                           const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(
      GL.glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glCompressedTextureImage1DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glCompressedMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                                   GLenum internalformat, GLsizei width, GLint border,
                                                   GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedMultiTexImage1DEXT(texunit, target, level, internalformat,
                                                        width, border, imageSize, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glCompressedTextureImage1DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

void WrappedOpenGL::StoreCompressedTexData(ResourceId texId, GLenum target, GLint level,
                                           bool subUpdate, GLint xoffset, GLint yoffset,
                                           GLint zoffset, GLsizei width, GLsizei height,
                                           GLsizei depth, GLenum format, GLsizei imageSize,
                                           const void *pixels)
{
  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;
  GLint unpackbuf = 0;

  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(unpackbuf == 0 && pixels != NULL)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(unpack.FastPathCompressed(width, height, depth))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, width, height, depth, imageSize);
  }

  if(unpackbuf != 0)
    srcPixels = (byte *)GL.glMapBufferRange(eGL_PIXEL_UNPACK_BUFFER, (GLintptr)pixels, imageSize,
                                            eGL_MAP_READ_BIT);

  if(srcPixels)
  {
    rdcstr error;

    if(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
       target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
       target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y || target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z ||
       target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z || target == GL_TEXTURE_2D_ARRAY ||
       target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
      if(depth <= 1)
      {
        size_t compressedImageSize = GetCompressedByteSize(width, height, 1, format);
        RDCASSERT(compressedImageSize == (size_t)imageSize);
        CompressedDataStore &cd = m_Textures[texId].compressedData;
        rdcarray<byte> &cdData = cd[level];
        GLint zoff = IsCubeFace(target) ? CubeTargetIndex(target) : zoffset;
        if(!subUpdate)
        {
          RDCASSERT(xoffset == 0);
          RDCASSERT(yoffset == 0);
          size_t startOffset = imageSize * zoff;
          if(cdData.size() < startOffset + imageSize)
            cdData.resize(startOffset + imageSize);
          memcpy(cdData.data() + startOffset, srcPixels, imageSize);
        }
        else
        {
          rdcfixedarray<uint32_t, 3> blockSize = GetCompressedBlockSize(format);
          RDCASSERT(xoffset % blockSize[0] == 0);
          RDCASSERT(yoffset % blockSize[1] == 0);
          RDCASSERT(width % blockSize[0] == 0);
          RDCASSERT(height % blockSize[1] == 0);
          GLsizei texLevelWidth = RDCMAX(1, m_Textures[texId].width >> level);
          GLsizei texLevelHeight = RDCMAX(1, m_Textures[texId].height >> level);
          size_t startOffset = GetCompressedByteSize(texLevelWidth, texLevelHeight, 1, format) * zoff;
          size_t endOffset =
              startOffset + GetCompressedByteSize(texLevelWidth, yoffset + height, 1, format);
          if(cdData.size() < endOffset)
            cdData.resize(endOffset);
          size_t srcRowSize = GetCompressedByteSize(width, (GLsizei)blockSize[1], 1, format);
          size_t dstRowSize = GetCompressedByteSize(texLevelWidth, (GLsizei)blockSize[1], 1, format);
          size_t srcOffset = 0;
          size_t dstOffset = startOffset + GetCompressedByteSize(texLevelWidth, yoffset, 1, format) +
                             GetCompressedByteSize(xoffset, (GLsizei)blockSize[1], 1, format);
          for(size_t y = 0; y < (size_t)height; y += blockSize[1])
          {
            memcpy(cdData.data() + dstOffset, srcPixels + srcOffset, srcRowSize);
            srcOffset += srcRowSize;
            dstOffset += dstRowSize;
          }
        }
      }
      else
      {
        error = StringFormat::Fmt("depth (%d)", depth);
      }
    }
    else if(target == GL_TEXTURE_3D)
    {
      // Only the trivial case is handled yet.
      if(xoffset == 0 && yoffset == 0 && zoffset == 0)
      {
        RDCASSERT(GetCompressedByteSize(width, height, depth, format) == (size_t)imageSize);
        CompressedDataStore &cd = m_Textures[texId].compressedData;
        rdcarray<byte> &cdData = cd[level];
        cdData.resize(imageSize);
        memcpy(cdData.data(), srcPixels, imageSize);
      }
      else
      {
        error = StringFormat::Fmt("xoffset (%d) and/or yoffset (%d) and/or zoffset (%d)", xoffset,
                                  yoffset, zoffset);
      }
    }
    else
    {
      error = "target";
    }

    if(unpackbuf != 0)
      GL.glUnmapBuffer(eGL_PIXEL_UNPACK_BUFFER);

    if(!error.empty())
      RDCWARN("StoreCompressedTexData: Unexpected %s (tex:%s, target:%s)", error.c_str(),
              ToStr(texId).c_str(), ToStr(target).c_str());
  }
  else
  {
    RDCWARN("StoreCompressedTexData: No source pixels to copy from (tex:%s, target:%s)",
            ToStr(texId).c_str(), ToStr(target).c_str());
  }

  SAFE_DELETE_ARRAY(unpackedPixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                            GLenum target, GLint level,
                                                            GLenum internalformat, GLsizei width,
                                                            GLsizei height, GLint border,
                                                            GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(border);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    if(!unpack.FastPathCompressed(width, height, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, height, 0, imageSize);
  }

  SERIALISE_ELEMENT(imageSize);
  SERIALISE_ELEMENT_ARRAY(pixels, imageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    const void *databuf = pixels;

    if(IsGLES)
      StoreCompressedTexData(GetResourceManager()->GetResID(texture), target, level, false, 0, 0, 0,
                             width, height, 0, internalformat, imageSize, pixels);

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(pixels == NULL)
    {
      if(m_ScratchBuf.size() < (size_t)imageSize)
        m_ScratchBuf.resize(imageSize);
      databuf = m_ScratchBuf.data();
    }

    ResourceId liveId = GetResourceManager()->GetResID(texture);

    uint32_t mipsValid = m_Textures[liveId].mipsValid;
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = internalformat;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    GLint align = 1;
    GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

    PixelUnpackState unpack;
    if(pixels)
    {
      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
      RDCASSERT(!unpackbuf);
    }

    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    if(TextureBinding(target) == eGL_TEXTURE_BINDING_CUBE_MAP &&
       mipsValid != m_Textures[liveId].mipsValid)
    {
      GLenum ts[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      // special case handling for cubemaps, as we might have skipped the 'allocation' teximage
      // chunks to avoid serialising tons of 'data upload' teximage chunks. Sigh.
      // Any further chunks & initial data can overwrite this, but cubemaps must be square so all
      // parameters will be the same.
      for(size_t i = 0; i < ARRAY_COUNT(ts); i++)
      {
        GL.glCompressedTextureImage2DEXT(texture.name, ts[i], level, internalformat, width, height,
                                         border, imageSize, databuf);
      }
    }
    else
    {
      GL.glCompressedTextureImage2DEXT(texture.name, target, level, internalformat, width, height,
                                       border, imageSize, databuf);
    }

    if(unpackbuf)
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    if(pixels)
      unpack.Apply(false);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glCompressedTextureImage2DEXT(ResourceId texId, GLenum target,
                                                         GLint level, GLenum internalformat,
                                                         GLsizei width, GLsizei height, GLint border,
                                                         GLsizei imageSize, const GLvoid *pixels)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  bool fromunpackbuf = false;
  {
    GLint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    if(IsGLES)
      StoreCompressedTexData(record->GetResourceID(), target, level, false, 0, 0, 0, width, height,
                             0, internalformat, imageSize, pixels);

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(IsBackgroundCapturing(m_State) && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCompressedTextureImage2DEXT(ser, record->Resource.name, target, level,
                                              internalformat, width, height, border, imageSize,
                                              fromunpackbuf ? NULL : pixels);

      Chunk *chunk = scope.Get();
      record->AddChunk(chunk);

      // if we're actively capturing this may be a creation but it may be a re-initialise. Insert
      // the chunk here as well to ensure consistent replay
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLint border, GLsizei imageSize,
                                                  const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedTextureImage2DEXT(texture, target, level, internalformat,
                                                       width, height, border, imageSize, pixels));

  Common_glCompressedTextureImage2DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                       target, level, internalformat, width, height, border,
                                       imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLsizei height, GLint border,
                                           GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedTexImage2D(target, level, internalformat, width, height,
                                                border, imageSize, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glCompressedTextureImage2DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, height, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                                   GLenum internalformat, GLsizei width,
                                                   GLsizei height, GLint border, GLsizei imageSize,
                                                   const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedMultiTexImage2DEXT(texunit, target, level, internalformat,
                                                        width, height, border, imageSize, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glCompressedTextureImage2DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, height, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureImage3DEXT(SerialiserType &ser,
                                                            GLuint textureHandle, GLenum target,
                                                            GLint level, GLenum internalformat,
                                                            GLsizei width, GLsizei height,
                                                            GLsizei depth, GLint border,
                                                            GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  SERIALISE_ELEMENT(level).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(depth).Important();
  SERIALISE_ELEMENT(border);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    if(!unpack.FastPathCompressed(width, height, depth))
      pixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, width, height, depth, imageSize);
  }

  SERIALISE_ELEMENT(imageSize);
  SERIALISE_ELEMENT_ARRAY(pixels, imageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    const void *databuf = pixels;

    if(IsGLES)
      StoreCompressedTexData(GetResourceManager()->GetResID(texture), target, level, false, 0, 0, 0,
                             width, height, depth, internalformat, imageSize, pixels);

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(pixels == NULL)
    {
      if(m_ScratchBuf.size() < (size_t)imageSize)
        m_ScratchBuf.resize(imageSize);
      databuf = m_ScratchBuf.data();
    }

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = depth;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 3;
      m_Textures[liveId].internalFormat = internalformat;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    GLint align = 1;
    GL.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);

    PixelUnpackState unpack;
    if(pixels)
    {
      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
      RDCASSERT(!unpackbuf);
    }

    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    GL.glCompressedTextureImage3DEXT(texture.name, target, level, internalformat, width, height,
                                     depth, border, imageSize, databuf);

    if(unpackbuf)
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    GL.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    if(pixels)
      unpack.Apply(false);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glCompressedTextureImage3DEXT(ResourceId texId, GLenum target, GLint level,
                                                         GLenum internalformat, GLsizei width,
                                                         GLsizei height, GLsizei depth, GLint border,
                                                         GLsizei imageSize, const GLvoid *pixels)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  bool fromunpackbuf = false;
  {
    GLint unpackbuf = 0;
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    if(IsGLES)
      StoreCompressedTexData(record->GetResourceID(), target, level, false, 0, 0, 0, width, height,
                             depth, internalformat, imageSize, pixels);

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(IsBackgroundCapturing(m_State) && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].depth == depth &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glCompressedTextureImage3DEXT(ser, record->Resource.name, target, level,
                                              internalformat, width, height, depth, border,
                                              imageSize, fromunpackbuf ? NULL : pixels);

      Chunk *chunk = scope.Get();
      record->AddChunk(chunk);

      // if we're actively capturing this may be a creation but it may be a re-initialise. Insert
      // the chunk here as well to ensure consistent replay
      if(IsActiveCapturing(m_State))
      {
        GetContextRecord()->AddChunk(chunk->Duplicate());
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_PartialWrite);
      }

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLsizei depth, GLint border,
                                                  GLsizei imageSize, const void *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedTextureImage3DEXT(
      texture, target, level, internalformat, width, height, depth, border, imageSize, pixels));

  Common_glCompressedTextureImage3DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                       target, level, internalformat, width, height, depth, border,
                                       imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLint border, GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedTexImage3D(target, level, internalformat, width, height, depth,
                                                border, imageSize, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glCompressedTextureImage3DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, height, depth, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                                   GLenum internalformat, GLsizei width,
                                                   GLsizei height, GLsizei depth, GLint border,
                                                   GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_TIME_CALL(GL.glCompressedMultiTexImage3DEXT(
      texunit, target, level, internalformat, width, height, depth, border, imageSize, pixels));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glCompressedTextureImage3DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, height, depth, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

#pragma endregion

#pragma region Texture Creation(glCopyTexImage)

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureImage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLint level,
                                                      GLenum internalformat, GLint x, GLint y,
                                                      GLsizei width, GLint border)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(border);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 1;
      m_Textures[liveId].internalFormat = internalformat;
    }

    GL.glCopyTextureImage1DEXT(texture.name, target, level, internalformat, x, y, width, border);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CopyDst));

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glCopyTextureImage1DEXT(GLResourceRecord *record, GLenum target,
                                                   GLint level, GLenum internalformat, GLint x,
                                                   GLint y, GLsizei width, GLint border)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // not sure if proxy formats are valid, but ignore these anyway
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsBackgroundCapturing(m_State))
  {
    // add a fake teximage1D chunk to create the texture properly on live (as we won't replay this
    // copy chunk).

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(GLChunk::glTextureImage1DEXT);
    Serialise_glTextureImage1DEXT(ser, record->Resource.name, target, level, internalformat, width,
                                  border, GetBaseFormat(internalformat),
                                  GetDataType(internalformat), NULL);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);

    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureImage1DEXT(ser, record->Resource.name, target, level, internalformat, x,
                                      y, width, border);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_PartialWrite);
  }

  ResourceId texId = record->GetResourceID();
  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                            GLenum internalformat, GLint x, GLint y, GLsizei width,
                                            GLint border)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCopyTextureImage1DEXT(texture, target, level, internalformat, x, y, width, border));

  Common_glCopyTextureImage1DEXT(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
      internalformat, x, y, width, border);
}

void WrappedOpenGL::glCopyMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                             GLenum internalformat, GLint x, GLint y, GLsizei width,
                                             GLint border)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCopyMultiTexImage1DEXT(texunit, target, level, internalformat, x, y, width, border));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else if(!IsProxyTarget(target))
    Common_glCopyTextureImage1DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                   internalformat, x, y, width, border);
}

void WrappedOpenGL::glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                     GLint y, GLsizei width, GLint border)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTexImage1D(target, level, internalformat, x, y, width, border));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else if(!IsProxyTarget(target))
    Common_glCopyTextureImage1DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                   internalformat, x, y, width, border);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLint level,
                                                      GLenum internalformat, GLint x, GLint y,
                                                      GLsizei width, GLsizei height, GLint border)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(border);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].mipsValid |= 1 << level;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = internalformat;
    }

    GL.glCopyTextureImage2DEXT(texture.name, target, level, internalformat, x, y, width, height,
                               border);

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CopyDst));

    AddResourceInitChunk(texture);
  }
  return true;
}

void WrappedOpenGL::Common_glCopyTextureImage2DEXT(GLResourceRecord *record, GLenum target,
                                                   GLint level, GLenum internalformat, GLint x,
                                                   GLint y, GLsizei width, GLsizei height,
                                                   GLint border)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // not sure if proxy formats are valid, but ignore these anyway
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsBackgroundCapturing(m_State))
  {
    // add a fake teximage1D chunk to create the texture properly on live (as we won't replay this
    // copy chunk).
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(GLChunk::glTextureImage2DEXT);
    Serialise_glTextureImage2DEXT(ser, record->Resource.name, target, level, internalformat, width,
                                  height, border, GetBaseFormat(internalformat),
                                  GetDataType(internalformat), NULL);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);

    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureImage2DEXT(ser, record->Resource.name, target, level, internalformat, x,
                                      y, width, height, border);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                      eFrameRef_PartialWrite);
  }

  ResourceId texId = record->GetResourceID();
  m_Textures[texId].mipsValid |= 1 << level;

  if(level == 0)
  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedOpenGL::glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                            GLenum internalformat, GLint x, GLint y, GLsizei width,
                                            GLsizei height, GLint border)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTextureImage2DEXT(texture, target, level, internalformat, x, y,
                                                 width, height, border));

  Common_glCopyTextureImage2DEXT(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
      internalformat, x, y, width, height, border);
}

void WrappedOpenGL::glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                             GLenum internalformat, GLint x, GLint y, GLsizei width,
                                             GLsizei height, GLint border)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyMultiTexImage2DEXT(texunit, target, level, internalformat, x, y,
                                                  width, height, border));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else if(!IsProxyTarget(target))
    Common_glCopyTextureImage2DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                   internalformat, x, y, width, height, border);
}

void WrappedOpenGL::glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                     GLint y, GLsizei width, GLsizei height, GLint border)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCopyTexImage2D(target, level, internalformat, x, y, width, height, border));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else if(!IsProxyTarget(target))
    Common_glCopyTextureImage2DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                   internalformat, x, y, width, height, border);
}

#pragma endregion

#pragma region Texture Creation(glTexStorage *)

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                    GLenum target, GLsizei levels,
                                                    GLenum internalformat, GLsizei width)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(levels).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = 1;
    m_Textures[liveId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 1;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;
    m_Textures[liveId].mipsValid = (1 << levels) - 1;

    if(target != eGL_NONE)
      GL.glTextureStorage1DEXT(texture.name, target, levels, internalformat, width);
    else
      GL.glTextureStorage1D(texture.name, levels, internalformat, width);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureStorage1DEXT(ResourceId texId, GLenum target, GLsizei levels,
                                                 GLenum internalformat, GLsizei width)
{
  if(texId == ResourceId())
    return;

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorage1DEXT(ser, record->Resource.name, target, levels, internalformat,
                                    width);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

void WrappedOpenGL::glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage1DEXT(texture, target, levels, internalformat, width));

  Common_glTextureStorage1DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                               target, levels, internalformat, width);
}

void WrappedOpenGL::glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat,
                                       GLsizei width)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage1D(texture, levels, internalformat, width));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage1DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, levels, internalformat, width);
}

void WrappedOpenGL::glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
  SERIALISE_TIME_CALL(GL.glTexStorage1D(target, levels, internalformat, width));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage1DEXT(record->GetResourceID(), target, levels, internalformat, width);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                    GLenum target, GLsizei levels,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(levels).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;
    m_Textures[liveId].mipsValid = (1 << levels) - 1;

    if(target != eGL_NONE)
      GL.glTextureStorage2DEXT(texture.name, target, levels, internalformat, width, height);
    else
      GL.glTextureStorage2D(texture.name, levels, internalformat, width, height);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureStorage2DEXT(ResourceId texId, GLenum target, GLsizei levels,
                                                 GLenum internalformat, GLsizei width, GLsizei height)
{
  if(texId == ResourceId())
    return;

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorage2DEXT(ser, record->Resource.name, target, levels, internalformat,
                                    width, height);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

void WrappedOpenGL::glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width, GLsizei height)
{
  SERIALISE_TIME_CALL(
      GL.glTextureStorage2DEXT(texture, target, levels, internalformat, width, height));

  Common_glTextureStorage2DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                               target, levels, internalformat, width, height);
}

void WrappedOpenGL::glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat,
                                       GLsizei width, GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage2D(texture, levels, internalformat, width, height));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage2DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, levels, internalformat, width, height);
}

void WrappedOpenGL::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                                   GLsizei width, GLsizei height)
{
  SERIALISE_TIME_CALL(GL.glTexStorage2D(target, levels, internalformat, width, height));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage2DEXT(record->GetResourceID(), target, levels, internalformat, width,
                                   height);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorage3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                    GLenum target, GLsizei levels,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height, GLsizei depth)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(levels).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(depth).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = depth;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 3;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;
    m_Textures[liveId].mipsValid = (1 << levels) - 1;

    if(target != eGL_NONE)
      GL.glTextureStorage3DEXT(texture.name, target, levels, internalformat, width, height, depth);
    else
      GL.glTextureStorage3D(texture.name, levels, internalformat, width, height, depth);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureStorage3DEXT(ResourceId texId, GLenum target, GLsizei levels,
                                                 GLenum internalformat, GLsizei width,
                                                 GLsizei height, GLsizei depth)
{
  if(texId == ResourceId())
    return;

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorage3DEXT(ser, record->Resource.name, target, levels, internalformat,
                                    width, height, depth);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

void WrappedOpenGL::glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width, GLsizei height,
                                          GLsizei depth)
{
  SERIALISE_TIME_CALL(
      GL.glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth));

  Common_glTextureStorage3DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                               target, levels, internalformat, width, height, depth);
}

void WrappedOpenGL::glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat,
                                       GLsizei width, GLsizei height, GLsizei depth)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage3D(texture, levels, internalformat, width, height, depth));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage3DEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, levels, internalformat, width, height, depth);
}

void WrappedOpenGL::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat,
                                   GLsizei width, GLsizei height, GLsizei depth)
{
  SERIALISE_TIME_CALL(GL.glTexStorage3D(target, levels, internalformat, width, height, depth));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage3DEXT(record->GetResourceID(), target, levels, internalformat, width,
                                   height, depth);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorage2DMultisampleEXT(SerialiserType &ser,
                                                               GLuint textureHandle, GLenum target,
                                                               GLsizei samples, GLenum internalformat,
                                                               GLsizei width, GLsizei height,
                                                               GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(samples).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT_TYPED(bool, fixedsamplelocations);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(texture.name, target, internalformat, dummy);

    // if we promoted glTexImage2DMultisample to storage, we need a sized format
    internalformat = GetSizedFormat(internalformat);

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].samples = samples;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;
    m_Textures[liveId].mipsValid = 1;

    if(target != eGL_NONE)
      GL.glTextureStorage2DMultisampleEXT(texture.name, target, samples, internalformat, width,
                                          height, fixedsamplelocations);
    else
      GL.glTextureStorage2DMultisample(texture.name, samples, internalformat, width, height,
                                       fixedsamplelocations);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureStorage2DMultisampleEXT(ResourceId texId, GLenum target,
                                                            GLsizei samples, GLenum internalformat,
                                                            GLsizei width, GLsizei height,
                                                            GLboolean fixedsamplelocations)
{
  if(texId == ResourceId())
    return;

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorage2DMultisampleEXT(ser, record->Resource.name, target, samples,
                                               internalformat, width, height, fixedsamplelocations);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    m_Textures[texId].samples = samples;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = 1;
  }
}

void WrappedOpenGL::glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                     GLenum internalformat, GLsizei width,
                                                     GLsizei height, GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage2DMultisampleEXT(texture, target, samples, internalformat,
                                                          width, height, fixedsamplelocations));

  Common_glTextureStorage2DMultisampleEXT(
      GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), target, samples,
      internalformat, width, height, fixedsamplelocations);
}

void WrappedOpenGL::glTextureStorage2DMultisample(GLuint texture, GLsizei samples,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage2DMultisample(texture, samples, internalformat, width,
                                                       height, fixedsamplelocations));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage2DMultisampleEXT(
        GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), eGL_NONE, samples,
        internalformat, width, height, fixedsamplelocations);
}

void WrappedOpenGL::glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height,
                                              GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glTexStorage2DMultisample(target, samples, internalformat, width, height,
                                                   fixedsamplelocations));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage2DMultisampleEXT(record->GetResourceID(), target, samples,
                                              internalformat, width, height, fixedsamplelocations);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                            GLsizei width, GLsizei height,
                                            GLboolean fixedsamplelocations)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTexImage2DMultisample(target, samples, internalformat, width, height,
                                                 fixedsamplelocations));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    // assuming texstorage is equivalent to teximage (this is not true in the case where someone
    // tries to re-size an image by re-calling teximage).
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage2DMultisampleEXT(record->GetResourceID(), target, samples,
                                              internalformat, width, height, fixedsamplelocations);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorage3DMultisampleEXT(SerialiserType &ser,
                                                               GLuint textureHandle, GLenum target,
                                                               GLsizei samples,
                                                               GLenum internalformat, GLsizei width,
                                                               GLsizei height, GLsizei depth,
                                                               GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target).Important();
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(samples).Important();
  SERIALISE_ELEMENT(internalformat).Important();
  SERIALISE_ELEMENT(width).Important();
  SERIALISE_ELEMENT(height).Important();
  SERIALISE_ELEMENT(depth).Important();
  SERIALISE_ELEMENT_TYPED(bool, fixedsamplelocations);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(texture.name, target, internalformat, dummy);

    // if we promoted glTexImage3DMultisample to storage, we need a sized format
    internalformat = GetSizedFormat(internalformat);

    ResourceId liveId = GetResourceManager()->GetResID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = depth;
    m_Textures[liveId].samples = samples;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;
    m_Textures[liveId].mipsValid = 1;

    if(target != eGL_NONE)
      GL.glTextureStorage3DMultisampleEXT(texture.name, target, samples, internalformat, width,
                                          height, depth, fixedsamplelocations);
    else
      GL.glTextureStorage3DMultisample(texture.name, samples, internalformat, width, height, depth,
                                       fixedsamplelocations);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureStorage3DMultisampleEXT(ResourceId texId, GLenum target,
                                                            GLsizei samples, GLenum internalformat,
                                                            GLsizei width, GLsizei height,
                                                            GLsizei depth,
                                                            GLboolean fixedsamplelocations)
{
  if(texId == ResourceId())
    return;

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(target) || internalformat == 0)
    return;

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorage3DMultisampleEXT(ser, record->Resource.name, target, samples,
                                               internalformat, width, height, depth,
                                               fixedsamplelocations);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    m_Textures[texId].samples = samples;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = 1;
  }
}

void WrappedOpenGL::glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                     GLenum internalformat, GLsizei width,
                                                     GLsizei height, GLsizei depth,
                                                     GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage3DMultisampleEXT(
      texture, target, samples, internalformat, width, height, depth, fixedsamplelocations));

  Common_glTextureStorage3DMultisampleEXT(
      GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), target, samples,
      internalformat, width, height, depth, fixedsamplelocations);
}

void WrappedOpenGL::glTextureStorage3DMultisample(GLuint texture, GLsizei samples,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLsizei depth,
                                                  GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glTextureStorage3DMultisample(texture, samples, internalformat, width,
                                                       height, depth, fixedsamplelocations));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage3DMultisampleEXT(
        GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), eGL_NONE, samples,
        internalformat, width, height, depth, fixedsamplelocations);
}

void WrappedOpenGL::glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height, GLsizei depth,
                                              GLboolean fixedsamplelocations)
{
  SERIALISE_TIME_CALL(GL.glTexStorage3DMultisample(target, samples, internalformat, width, height,
                                                   depth, fixedsamplelocations));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage3DMultisampleEXT(record->GetResourceID(), target, samples,
                                              internalformat, width, height, depth,
                                              fixedsamplelocations);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            GLboolean fixedsamplelocations)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTexImage3DMultisample(target, samples, internalformat, width, height,
                                                 depth, fixedsamplelocations));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    // assuming texstorage is equivalent to teximage (this is not true in the case where someone
    // tries to re-size an image by re-calling teximage).
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage3DMultisampleEXT(record->GetResourceID(), target, samples,
                                              internalformat, width, height, depth,
                                              fixedsamplelocations);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

#pragma endregion

#pragma region Texture upload(glTexSubImage *)

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureSubImage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLint level, GLint xoffset,
                                                     GLsizei width, GLenum format, GLenum type,
                                                     const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  GLint unpackbuf = 0;
  if(ser.IsWriting())
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0).Hidden();

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(!unpack.FastPath(width, 0, 0, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, 0, 0, format, type);
  }

  size_t subimageSize = GetByteSize(width, 1, 1, format, type);

  uint64_t UnpackOffset = 0;

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels"_lit, pixels, subimageSize, SerialiserFlags::AllocateMemory).Important();
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset).OffsetOrSize();
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(false);
      ResetPixelUnpackState(false, 1);
    }

    if(format == eGL_LUMINANCE)
    {
      format = eGL_RED;
    }
    else if(format == eGL_LUMINANCE_ALPHA)
    {
      format = eGL_RG;
    }
    else if(format == eGL_ALPHA)
    {
      // check if format was converted from alpha-only format to R8, and substitute
      ResourceId liveId = GetResourceManager()->GetResID(texture);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        format = eGL_RED;
    }

    if(target != eGL_NONE)
      GL.glTextureSubImage1DEXT(texture.name, target, level, xoffset, width, format, type,
                                pixels ? pixels : (const void *)UnpackOffset);
    else
      GL.glTextureSubImage1D(texture.name, level, xoffset, width, format, type,
                             pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(false);

      FreeAlignedBuffer((byte *)pixels);
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  if(ser.IsReading() && IsStructuredExporting(m_State))
  {
    if(!UnpackBufBound)
      FreeAlignedBuffer((byte *)pixels);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureSubImage1DEXT(GLResourceRecord *record, GLenum target,
                                                  GLint level, GLint xoffset, GLsizei width,
                                                  GLenum format, GLenum type, const void *pixels)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(format))
    return;

  GLint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsBackgroundCapturing(m_State) && unpackbuf != 0)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else
  {
    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureSubImage1DEXT(ser, record->Resource.name, target, level, xoffset, width,
                                     format, type, pixels);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

void WrappedOpenGL::glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level,
                                           GLint xoffset, GLsizei width, GLenum format, GLenum type,
                                           const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glTextureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, width, format, type, pixels);
}

void WrappedOpenGL::glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width,
                                        GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTextureSubImage1D(texture, level, xoffset, width, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, width, format, type, pixels);
}

void WrappedOpenGL::glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width,
                                    GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTexSubImage1D(target, level, xoffset, width, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(GetCtxData().GetActiveTexRecord(target), target, level, xoffset,
                                  width, format, type, pixels);
}

void WrappedOpenGL::glMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                            GLint xoffset, GLsizei width, GLenum format,
                                            GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                  xoffset, width, format, type, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureSubImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLint level, GLint xoffset,
                                                     GLint yoffset, GLsizei width, GLsizei height,
                                                     GLenum format, GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  GLint unpackbuf = 0;
  if(ser.IsWriting())
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0).Hidden();

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(!unpack.FastPath(width, height, 0, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, height, 0, format, type);
  }

  size_t subimageSize = GetByteSize(width, height, 1, format, type);

  uint64_t UnpackOffset = 0;

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels"_lit, pixels, subimageSize, SerialiserFlags::AllocateMemory).Important();
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset).OffsetOrSize();
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(false);
      ResetPixelUnpackState(false, 1);
    }

    if(format == eGL_LUMINANCE)
    {
      format = eGL_RED;
    }
    else if(format == eGL_LUMINANCE_ALPHA)
    {
      format = eGL_RG;
    }
    else if(format == eGL_ALPHA)
    {
      // check if format was converted from alpha-only format to R8, and substitute
      ResourceId liveId = GetResourceManager()->GetResID(texture);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        format = eGL_RED;
    }

    if(target != eGL_NONE)
      GL.glTextureSubImage2DEXT(texture.name, target, level, xoffset, yoffset, width, height,
                                format, type, pixels ? pixels : (const void *)UnpackOffset);
    else
      GL.glTextureSubImage2D(texture.name, level, xoffset, yoffset, width, height, format, type,
                             pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(false);

      FreeAlignedBuffer((byte *)pixels);
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  if(ser.IsReading() && IsStructuredExporting(m_State))
  {
    if(!UnpackBufBound)
      FreeAlignedBuffer((byte *)pixels);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureSubImage2DEXT(GLResourceRecord *record, GLenum target,
                                                  GLint level, GLint xoffset, GLint yoffset,
                                                  GLsizei width, GLsizei height, GLenum format,
                                                  GLenum type, const void *pixels)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(format))
    return;

  GLint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsBackgroundCapturing(m_State) && unpackbuf != 0)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else
  {
    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureSubImage2DEXT(ser, record->Resource.name, target, level, xoffset, yoffset,
                                     width, height, format, type, pixels);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

void WrappedOpenGL::glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset,
                                           GLint yoffset, GLsizei width, GLsizei height,
                                           GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width,
                                                height, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, width, height, format, type, pixels);
}

void WrappedOpenGL::glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLsizei width, GLsizei height, GLenum format, GLenum type,
                                        const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTextureSubImage2D(texture, level, xoffset, yoffset, width, height,
                                             format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, width, height, format, type, pixels);
}

void WrappedOpenGL::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLsizei width, GLsizei height, GLenum format, GLenum type,
                                    const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(GetCtxData().GetActiveTexRecord(target), target, level, xoffset,
                                  yoffset, width, height, format, type, pixels);
}

void WrappedOpenGL::glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                            GLint yoffset, GLsizei width, GLsizei height,
                                            GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width,
                                                 height, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                  xoffset, yoffset, width, height, format, type, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureSubImage3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLint level, GLint xoffset,
                                                     GLint yoffset, GLint zoffset, GLsizei width,
                                                     GLsizei height, GLsizei depth, GLenum format,
                                                     GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(zoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  GLint unpackbuf = 0;
  if(ser.IsWriting())
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0).Hidden();

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(false);

    if(!unpack.FastPath(width, height, depth, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, height, depth, format, type);
  }

  size_t subimageSize = GetByteSize(width, height, depth, format, type);

  uint64_t UnpackOffset = 0;

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels"_lit, pixels, subimageSize, SerialiserFlags::AllocateMemory).Important();
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset).OffsetOrSize();
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(false);
      ResetPixelUnpackState(false, 1);
    }

    if(format == eGL_LUMINANCE)
    {
      format = eGL_RED;
    }
    else if(format == eGL_LUMINANCE_ALPHA)
    {
      format = eGL_RG;
    }
    else if(format == eGL_ALPHA)
    {
      // check if format was converted from alpha-only format to R8, and substitute
      ResourceId liveId = GetResourceManager()->GetResID(texture);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        format = eGL_RED;
    }

    if(target != eGL_NONE)
      GL.glTextureSubImage3DEXT(texture.name, target, level, xoffset, yoffset, zoffset, width, height,
                                depth, format, type, pixels ? pixels : (const void *)UnpackOffset);
    else
      GL.glTextureSubImage3D(texture.name, level, xoffset, yoffset, zoffset, width, height, depth,
                             format, type, pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(false);

      FreeAlignedBuffer((byte *)pixels);
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  if(ser.IsReading() && IsStructuredExporting(m_State))
  {
    if(!UnpackBufBound)
      FreeAlignedBuffer((byte *)pixels);
  }

  return true;
}

void WrappedOpenGL::Common_glTextureSubImage3DEXT(GLResourceRecord *record, GLenum target,
                                                  GLint level, GLint xoffset, GLint yoffset,
                                                  GLint zoffset, GLsizei width, GLsizei height,
                                                  GLsizei depth, GLenum format, GLenum type,
                                                  const void *pixels)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(format))
    return;

  GLint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsBackgroundCapturing(m_State) && unpackbuf != 0)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else
  {
    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureSubImage3DEXT(ser, record->Resource.name, target, level, xoffset, yoffset,
                                     zoffset, width, height, depth, format, type, pixels);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

void WrappedOpenGL::glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset, GLint zoffset,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset,
                                                width, height, depth, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void WrappedOpenGL::glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                        GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width,
                                             height, depth, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void WrappedOpenGL::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                    GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                                         depth, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(GetCtxData().GetActiveTexRecord(target), target, level, xoffset,
                                  yoffset, zoffset, width, height, depth, format, type, pixels);
}

void WrappedOpenGL::glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset, GLint zoffset,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            GLenum format, GLenum type, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset,
                                                 width, height, depth, format, type, pixels));

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target, level,
                                  xoffset, yoffset, zoffset, width, height, depth, format, type,
                                  pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureSubImage1DEXT(SerialiserType &ser,
                                                               GLuint textureHandle, GLenum target,
                                                               GLint level, GLint xoffset,
                                                               GLsizei width, GLenum format,
                                                               GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(format);

  GLint unpackbuf = 0;
  if(ser.IsWriting())
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0).Hidden();

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    if(!unpack.FastPathCompressed(width, 0, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, 0, 0, imageSize);
  }

  uint64_t UnpackOffset = 0;

  SERIALISE_ELEMENT(imageSize);

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels"_lit, pixels, (uint32_t &)imageSize, SerialiserFlags::AllocateMemory)
        .Important();
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset).OffsetOrSize();
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
    }

    if(target != eGL_NONE)
      GL.glCompressedTextureSubImage1DEXT(texture.name, target, level, xoffset, width, format,
                                          imageSize, pixels ? pixels : (const void *)UnpackOffset);
    else
      GL.glCompressedTextureSubImage1D(texture.name, level, xoffset, width, format, imageSize,
                                       pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(true);

      FreeAlignedBuffer((byte *)pixels);
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  if(ser.IsReading() && IsStructuredExporting(m_State))
  {
    if(!UnpackBufBound)
      FreeAlignedBuffer((byte *)pixels);
  }

  return true;
}

void WrappedOpenGL::Common_glCompressedTextureSubImage1DEXT(GLResourceRecord *record, GLenum target,
                                                            GLint level, GLint xoffset,
                                                            GLsizei width, GLenum format,
                                                            GLsizei imageSize, const void *pixels)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(format))
    return;

  GLint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsBackgroundCapturing(m_State) && unpackbuf != 0)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else
  {
    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCompressedTextureSubImage1DEXT(ser, record->Resource.name, target, level, xoffset,
                                               width, format, imageSize, pixels);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

void WrappedOpenGL::glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level,
                                                     GLint xoffset, GLsizei width, GLenum format,
                                                     GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTextureSubImage1DEXT(texture, target, level, xoffset, width,
                                                          format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, width, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset,
                                                  GLsizei width, GLenum format, GLsizei imageSize,
                                                  const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCompressedTextureSubImage1D(texture, level, xoffset, width, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, width, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset,
                                              GLsizei width, GLenum format, GLsizei imageSize,
                                              const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(
      GL.glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                            xoffset, width, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                                      GLint xoffset, GLsizei width, GLenum format,
                                                      GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedMultiTexSubImage1DEXT(texunit, target, level, xoffset, width,
                                                           format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target,
                                            level, xoffset, width, format, imageSize, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureSubImage2DEXT(SerialiserType &ser,
                                                               GLuint textureHandle, GLenum target,
                                                               GLint level, GLint xoffset,
                                                               GLint yoffset, GLsizei width,
                                                               GLsizei height, GLenum format,
                                                               GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(format);

  GLint unpackbuf = 0;
  if(ser.IsWriting())
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0).Hidden();

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    if(!unpack.FastPathCompressed(width, height, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, height, 0, imageSize);
  }

  uint64_t UnpackOffset = 0;

  SERIALISE_ELEMENT(imageSize);
  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels"_lit, pixels, (uint32_t &)imageSize, SerialiserFlags::AllocateMemory)
        .Important();
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset).OffsetOrSize();
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && IsGLES)
    {
      StoreCompressedTexData(GetResourceManager()->GetResID(texture), target, level, true, xoffset,
                             yoffset, 0, width, height, 0, format, imageSize,
                             pixels ? pixels : (const void *)UnpackOffset);
    }

    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
    }

    if(target != eGL_NONE)
      GL.glCompressedTextureSubImage2DEXT(texture.name, target, level, xoffset, yoffset, width,
                                          height, format, imageSize,
                                          pixels ? pixels : (const void *)UnpackOffset);
    else
      GL.glCompressedTextureSubImage2D(texture.name, level, xoffset, yoffset, width, height, format,
                                       imageSize, pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(true);

      FreeAlignedBuffer((byte *)pixels);
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  if(ser.IsReading() && IsStructuredExporting(m_State))
  {
    if(!UnpackBufBound)
      FreeAlignedBuffer((byte *)pixels);
  }

  return true;
}

void WrappedOpenGL::Common_glCompressedTextureSubImage2DEXT(GLResourceRecord *record, GLenum target,
                                                            GLint level, GLint xoffset,
                                                            GLint yoffset, GLsizei width,
                                                            GLsizei height, GLenum format,
                                                            GLsizei imageSize, const void *pixels)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(format))
    return;

  GLint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsGLES)
    StoreCompressedTexData(record->GetResourceID(), target, level, true, xoffset, yoffset, 0, width,
                           height, 0, format, imageSize, pixels);

  if(IsBackgroundCapturing(m_State) && unpackbuf != 0)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else
  {
    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCompressedTextureSubImage2DEXT(ser, record->Resource.name, target, level, xoffset,
                                               yoffset, width, height, format, imageSize, pixels);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

void WrappedOpenGL::glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                                     GLint xoffset, GLint yoffset, GLsizei width,
                                                     GLsizei height, GLenum format,
                                                     GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTextureSubImage2DEXT(
      texture, target, level, xoffset, yoffset, width, height, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, width, height, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset,
                                                  GLint yoffset, GLsizei width, GLsizei height,
                                                  GLenum format, GLsizei imageSize,
                                                  const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTextureSubImage2D(texture, level, xoffset, yoffset, width,
                                                       height, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, width, height, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                              GLint yoffset, GLsizei width, GLsizei height,
                                              GLenum format, GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height,
                                                   format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                            xoffset, yoffset, width, height, format, imageSize,
                                            pixels);
}

void WrappedOpenGL::glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                                      GLint xoffset, GLint yoffset, GLsizei width,
                                                      GLsizei height, GLenum format,
                                                      GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedMultiTexSubImage2DEXT(
      texunit, target, level, xoffset, yoffset, width, height, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target,
                                            level, xoffset, yoffset, width, height, format,
                                            imageSize, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureSubImage3DEXT(
    SerialiserType &ser, GLuint textureHandle, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(zoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(format);

  GLint unpackbuf = 0;
  if(ser.IsWriting())
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0).Hidden();

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    if(!unpack.FastPathCompressed(width, height, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, height, 0, imageSize);
  }

  uint64_t UnpackOffset = 0;

  SERIALISE_ELEMENT(imageSize);
  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels"_lit, pixels, (uint32_t &)imageSize, SerialiserFlags::AllocateMemory)
        .Important();
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset).OffsetOrSize();
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && IsGLES)
      StoreCompressedTexData(GetResourceManager()->GetResID(texture), target, level, true, xoffset,
                             yoffset, zoffset, width, height, depth, format, imageSize,
                             pixels ? pixels : (const void *)UnpackOffset);

    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(true);
      ResetPixelUnpackState(true, 1);
    }

    if(target != eGL_NONE)
      GL.glCompressedTextureSubImage3DEXT(texture.name, target, level, xoffset, yoffset, zoffset,
                                          width, height, depth, format, imageSize,
                                          pixels ? pixels : (const void *)UnpackOffset);
    else
      GL.glCompressedTextureSubImage3D(texture.name, level, xoffset, yoffset, zoffset, width,
                                       height, depth, format, imageSize,
                                       pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(true);

      FreeAlignedBuffer((byte *)pixels);
    }

    if(IsLoading(m_State) && m_CurEventID > 0)
      m_ResourceUses[GetResourceManager()->GetResID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::CPUWrite));

    AddResourceInitChunk(texture);
  }

  if(ser.IsReading() && IsStructuredExporting(m_State))
  {
    if(!UnpackBufBound)
      FreeAlignedBuffer((byte *)pixels);
  }

  return true;
}

void WrappedOpenGL::Common_glCompressedTextureSubImage3DEXT(GLResourceRecord *record, GLenum target,
                                                            GLint level, GLint xoffset,
                                                            GLint yoffset, GLint zoffset,
                                                            GLsizei width, GLsizei height,
                                                            GLsizei depth, GLenum format,
                                                            GLsizei imageSize, const void *pixels)
{
  if(!record)
  {
    RDCERR(
        "Called texture function with invalid/unrecognised texture, or no texture bound to "
        "implicit slot");
    return;
  }

  CoherentMapImplicitBarrier();

  // proxy formats are used for querying texture capabilities, don't serialise these
  if(IsProxyTarget(format))
    return;

  GLint unpackbuf = 0;
  GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsGLES)
    StoreCompressedTexData(record->GetResourceID(), target, level, true, xoffset, yoffset, zoffset,
                           width, height, depth, format, imageSize, pixels);

  if(IsBackgroundCapturing(m_State) && unpackbuf != 0)
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
  else
  {
    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       IsBackgroundCapturing(m_State))
      return;

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCompressedTextureSubImage3DEXT(ser, record->Resource.name, target, level, xoffset,
                                               yoffset, zoffset, width, height, depth, format,
                                               imageSize, pixels);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                        eFrameRef_PartialWrite);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 60)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

void WrappedOpenGL::glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                                     GLint xoffset, GLint yoffset, GLint zoffset,
                                                     GLsizei width, GLsizei height, GLsizei depth,
                                                     GLenum format, GLsizei imageSize,
                                                     const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset,
                                                          zoffset, width, height, depth, format,
                                                          imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset,
                                                  GLint yoffset, GLint zoffset, GLsizei width,
                                                  GLsizei height, GLsizei depth, GLenum format,
                                                  GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTextureSubImage3D(
      texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, zoffset, width, height, depth, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                                              GLint yoffset, GLint zoffset, GLsizei width,
                                              GLsizei height, GLsizei depth, GLenum format,
                                              GLsizei imageSize, const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetActiveTexRecord(target), eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                                                   height, depth, format, imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage3DEXT(GetCtxData().GetActiveTexRecord(target), target, level,
                                            xoffset, yoffset, zoffset, width, height, depth, format,
                                            imageSize, pixels);
}

void WrappedOpenGL::glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                                      GLint xoffset, GLint yoffset, GLint zoffset,
                                                      GLsizei width, GLsizei height, GLsizei depth,
                                                      GLenum format, GLsizei imageSize,
                                                      const void *pixels)
{
  MarkReferencedWhileCapturing(GetCtxData().GetTexUnitRecord(target, texunit),
                               eFrameRef_PartialWrite);

  SERIALISE_TIME_CALL(GL.glCompressedMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset,
                                                           zoffset, width, height, depth, format,
                                                           imageSize, pixels));

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage3DEXT(GetCtxData().GetTexUnitRecord(target, texunit), target,
                                            level, xoffset, yoffset, zoffset, width, height, depth,
                                            format, imageSize, pixels);
}

#pragma endregion

#pragma region Tex Buffer

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureBufferRangeEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLenum internalformat,
                                                      GLuint bufferHandle, GLintptr offsetPtr,
                                                      GLsizeiptr sizePtr)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle)).Important();
  SERIALISE_ELEMENT_LOCAL(offs, (uint64_t)offsetPtr).OffsetOrSize();
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetResID(texture);
    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      m_Textures[liveId].width =
          uint32_t(size) /
          uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat), GetDataType(internalformat)));
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].internalFormat = internalformat;
      m_Textures[liveId].mipsValid = 1;
    }

    if(target != eGL_NONE)
      GL.glTextureBufferRangeEXT(texture.name, target, internalformat, buffer.name, (GLintptr)offs,
                                 (GLsizeiptr)size);
    else
      GL.glTextureBufferRange(texture.name, internalformat, buffer.name, (GLintptr)offs,
                              (GLsizei)size);

    AddResourceInitChunk(texture);
    DerivedResource(buffer, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::Common_glTextureBufferRangeEXT(ResourceId texId, GLenum target,
                                                   GLenum internalformat, GLuint buffer,
                                                   GLintptr offset, GLsizeiptr size)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    ResourceId bufid = GetResourceManager()->GetResID(BufferRes(GetCtx(), buffer));

    if(record->datatype == eGL_TEXTURE_BINDING_BUFFER &&
       m_Textures[texId].internalFormat == internalformat && IsBackgroundCapturing(m_State))
    {
      GetResourceManager()->MarkDirtyResource(texId);

      if(bufid != ResourceId())
      {
        GetResourceManager()->MarkDirtyResource(bufid);

        // this will lead to an accumulation of parents if the texture is continually rebound, but
        // this is unavoidable as we don't want to add tons of infrastructure just to track this
        // edge case.
        GLResourceRecord *bufRecord = GetResourceManager()->GetResourceRecord(bufid);

        if(bufRecord)
        {
          record->AddParent(bufRecord);
          record->viewSource = bufRecord->GetResourceID();
        }
      }

      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureBufferRangeEXT(ser, record->Resource.name, target, internalformat, buffer,
                                      offset, size);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

      if(bufid != ResourceId())
      {
        GetResourceManager()->MarkDirtyResource(bufid);
        GetResourceManager()->MarkResourceFrameReferenced(bufid, eFrameRef_Read);
      }
    }
    else
    {
      record->AddChunk(scope.Get());

      GLResourceRecord *bufRecord = GetResourceManager()->GetResourceRecord(bufid);

      if(bufRecord)
      {
        record->AddParent(bufRecord);
        record->viewSource = bufRecord->GetResourceID();
      }
    }
  }

  {
    m_Textures[texId].width =
        uint32_t(size) /
        uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat), GetDataType(internalformat)));
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = 1;
  }
}

void WrappedOpenGL::glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat,
                                            GLuint buffer, GLintptr offset, GLsizeiptr size)
{
  SERIALISE_TIME_CALL(
      GL.glTextureBufferRangeEXT(texture, target, internalformat, buffer, offset, size));

  Common_glTextureBufferRangeEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                 target, internalformat, buffer, offset, size);
}

void WrappedOpenGL::glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer,
                                         GLintptr offset, GLsizeiptr size)
{
  SERIALISE_TIME_CALL(GL.glTextureBufferRange(texture, internalformat, buffer, offset, size));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");

  Common_glTextureBufferRangeEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, internalformat, buffer, offset, size);
}

void WrappedOpenGL::glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer,
                                     GLintptr offset, GLsizeiptr size)
{
  SERIALISE_TIME_CALL(GL.glTexBufferRange(target, internalformat, buffer, offset, size));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureBufferRangeEXT(record->GetResourceID(), target, internalformat, buffer,
                                     offset, size);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureBufferEXT(SerialiserType &ser, GLuint textureHandle,
                                                 GLenum target, GLenum internalformat,
                                                 GLuint bufferHandle)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle)).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId liveId = GetResourceManager()->GetResID(texture);
    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      uint32_t Size = 1;
      GL.glGetNamedBufferParameterivEXT(buffer.name, eGL_BUFFER_SIZE, (GLint *)&Size);
      m_Textures[liveId].width = Size / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat),
                                                             GetDataType(internalformat)));
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].internalFormat = internalformat;
      m_Textures[liveId].mipsValid = 1;
    }

    if(target != eGL_NONE)
      GL.glTextureBufferEXT(texture.name, target, internalformat, buffer.name);
    else
      GL.glTextureBuffer(texture.name, internalformat, buffer.name);

    AddResourceInitChunk(texture);
    DerivedResource(buffer, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::Common_glTextureBufferEXT(ResourceId texId, GLenum target,
                                              GLenum internalformat, GLuint buffer)
{
  if(texId == ResourceId())
    return;

  CoherentMapImplicitBarrier();

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    ResourceId bufid = GetResourceManager()->GetResID(BufferRes(GetCtx(), buffer));

    if(record->datatype == eGL_TEXTURE_BINDING_BUFFER &&
       m_Textures[texId].internalFormat == internalformat && IsBackgroundCapturing(m_State))
    {
      GetResourceManager()->MarkDirtyResource(texId);

      if(bufid != ResourceId())
      {
        GetResourceManager()->MarkDirtyResource(bufid);

        // this will lead to an accumulation of parents if the texture is continually rebound, but
        // this is unavoidable as we don't want to add tons of infrastructure just to track this
        // edge case.
        GLResourceRecord *bufRecord = GetResourceManager()->GetResourceRecord(bufid);

        if(bufRecord)
        {
          record->AddParent(bufRecord);
          record->viewSource = bufRecord->GetResourceID();
        }
      }

      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureBufferEXT(ser, record->Resource.name, target, internalformat, buffer);

    Chunk *chunk = scope.Get();

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(chunk);
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

      if(bufid != ResourceId())
      {
        GetResourceManager()->MarkDirtyResource(bufid);
        GetResourceManager()->MarkResourceFrameReferenced(bufid, eFrameRef_Read);
      }
    }
    else
    {
      record->AddChunk(chunk);

      GLResourceRecord *bufRecord = GetResourceManager()->GetResourceRecord(bufid);

      if(bufRecord)
      {
        record->AddParent(bufRecord);
        record->viewSource = bufRecord->GetResourceID();
      }
    }
  }

  {
    if(buffer != 0)
    {
      uint32_t size = 1;
      GL.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, (GLint *)&size);
      m_Textures[texId].width =
          uint32_t(size) /
          uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat), GetDataType(internalformat)));
    }
    else
    {
      m_Textures[texId].width = 1;
    }

    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[texId].curType = TextureTarget(target);
    else
      m_Textures[texId].curType =
          TextureTarget(GetResourceManager()->GetResourceRecord(texId)->datatype);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].mipsValid = 1;
  }
}

void WrappedOpenGL::glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat,
                                       GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glTextureBufferEXT(texture, target, internalformat, buffer));

  Common_glTextureBufferEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), target,
                            internalformat, buffer);
}

void WrappedOpenGL::glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glTextureBuffer(texture, internalformat, buffer));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");

  Common_glTextureBufferEXT(GetResourceManager()->GetResID(TextureRes(GetCtx(), texture)), eGL_NONE,
                            internalformat, buffer);
}

void WrappedOpenGL::glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glTexBuffer(target, internalformat, buffer));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureBufferEXT(record->GetResourceID(), target, internalformat, buffer);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat,
                                        GLuint buffer)
{
  SERIALISE_TIME_CALL(GL.glMultiTexBufferEXT(texunit, target, internalformat, buffer));

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else if(!IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetTexUnitRecord(target, texunit);
    if(record != NULL)
      Common_glTextureBufferEXT(record->GetResourceID(), target, internalformat, buffer);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureFoveationParametersQCOM(SerialiserType &ser,
                                                               GLuint textureHandle, GLuint layer,
                                                               GLuint focalPoint, GLfloat focalX,
                                                               GLfloat focalY, GLfloat gainX,
                                                               GLfloat gainY, GLfloat foveaArea)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle)).Important();
  SERIALISE_ELEMENT(layer);
  SERIALISE_ELEMENT(focalPoint);
  SERIALISE_ELEMENT(focalX).Important();
  SERIALISE_ELEMENT(focalY).Important();
  SERIALISE_ELEMENT(gainX);
  SERIALISE_ELEMENT(gainY);
  SERIALISE_ELEMENT(foveaArea);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(glTextureFoveationParametersQCOM);

    GL.glTextureFoveationParametersQCOM(texture.name, layer, focalPoint, focalX, focalY, gainX,
                                        gainY, foveaArea);

    AddResourceInitChunk(texture);
  }

  return true;
}

void WrappedOpenGL::glTextureFoveationParametersQCOM(GLuint texture, GLuint layer, GLuint focalPoint,
                                                     GLfloat focalX, GLfloat focalY, GLfloat gainX,
                                                     GLfloat gainY, GLfloat foveaArea)
{
  SERIALISE_TIME_CALL(GL.glTextureFoveationParametersQCOM(texture, layer, focalPoint, focalX,
                                                          focalY, gainX, gainY, foveaArea));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureFoveationParametersQCOM(ser, record->Resource.name, layer, focalPoint,
                                               focalX, focalY, gainX, gainY, foveaArea);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
    }
    else
    {
      record->AddChunk(scope.Get());
      record->UpdateCount++;

      if(record->UpdateCount > 64)
      {
        m_HighTrafficResources.insert(record->GetResourceID());
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      }
    }
  }
}

#pragma endregion

INSTANTIATE_FUNCTION_SERIALISED(void, glGenTextures, GLsizei n, GLuint *textures);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateTextures, GLenum target, GLsizei n, GLuint *textures);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindTexture, GLenum target, GLuint texture);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindTextures, GLuint first, GLsizei count,
                                const GLuint *textures);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindMultiTextureEXT, GLenum texunit, GLenum target,
                                GLuint texture);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindTextureUnit, GLuint texunit, GLuint texture);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindImageTexture, GLuint unit, GLuint texture, GLint level,
                                GLboolean layered, GLint layer, GLenum access, GLenum format);
INSTANTIATE_FUNCTION_SERIALISED(void, glBindImageTextures, GLuint first, GLsizei count,
                                const GLuint *textures);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureView, GLuint texture, GLenum target,
                                GLuint origtexture, GLenum internalformat, GLuint minlevel,
                                GLuint numlevels, GLuint minlayer, GLuint numlayers);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenerateTextureMipmapEXT, GLuint texture, GLenum target);
INSTANTIATE_FUNCTION_SERIALISED(void, glCopyImageSubData, GLuint srcName, GLenum srcTarget,
                                GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                                GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY,
                                GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
INSTANTIATE_FUNCTION_SERIALISED(void, glCopyTextureSubImage1DEXT, GLuint texture, GLenum target,
                                GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
INSTANTIATE_FUNCTION_SERIALISED(void, glCopyTextureSubImage2DEXT, GLuint texture, GLenum target,
                                GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
                                GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glCopyTextureSubImage3DEXT, GLuint texture, GLenum target,
                                GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                GLint y, GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureParameteriEXT, GLuint texture, GLenum target,
                                GLenum pname, GLint param);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureParameterivEXT, GLuint texture, GLenum target,
                                GLenum pname, const GLint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureParameterIivEXT, GLuint texture, GLenum target,
                                GLenum pname, const GLint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureParameterIuivEXT, GLuint texture, GLenum target,
                                GLenum pname, const GLuint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureParameterfEXT, GLuint texture, GLenum target,
                                GLenum pname, GLfloat param);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureParameterfvEXT, GLuint texture, GLenum target,
                                GLenum pname, const GLfloat *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glPixelStorei, GLenum pname, GLint param);
INSTANTIATE_FUNCTION_SERIALISED(void, glActiveTexture, GLenum texture);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureImage1DEXT, GLuint texture, GLenum target,
                                GLint level, GLint internalformat, GLsizei width, GLint border,
                                GLenum format, GLenum type, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureImage2DEXT, GLuint texture, GLenum target,
                                GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                GLint border, GLenum format, GLenum type, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureImage3DEXT, GLuint texture, GLenum target, GLint level,
                                GLint internalformat, GLsizei width, GLsizei height, GLsizei depth,
                                GLint border, GLenum format, GLenum type, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompressedTextureImage1DEXT, GLuint texture, GLenum target,
                                GLint level, GLenum internalformat, GLsizei width, GLint border,
                                GLsizei imageSize, const GLvoid *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompressedTextureImage2DEXT, GLuint texture, GLenum target,
                                GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                GLint border, GLsizei imageSize, const GLvoid *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompressedTextureImage3DEXT, GLuint texture, GLenum target,
                                GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCopyTextureImage1DEXT, GLuint texture, GLenum target,
                                GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width,
                                GLint border);
INSTANTIATE_FUNCTION_SERIALISED(void, glCopyTextureImage2DEXT, GLuint texture, GLenum target,
                                GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width,
                                GLsizei height, GLint border);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorage1DEXT, GLuint texture, GLenum target,
                                GLsizei levels, GLenum internalformat, GLsizei width);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorage2DEXT, GLuint texture, GLenum target,
                                GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorage3DEXT, GLuint texture, GLenum target,
                                GLsizei levels, GLenum internalformat, GLsizei width,
                                GLsizei height, GLsizei depth);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorage2DMultisampleEXT, GLuint texture,
                                GLenum target, GLsizei samples, GLenum internalformat,
                                GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorage3DMultisampleEXT, GLuint texture,
                                GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureSubImage1DEXT, GLuint texture, GLenum target,
                                GLint level, GLint xoffset, GLsizei width, GLenum format,
                                GLenum type, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureSubImage2DEXT, GLuint texture, GLenum target,
                                GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                GLsizei height, GLenum format, GLenum type, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureSubImage3DEXT, GLuint texture, GLenum target,
                                GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                GLenum type, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompressedTextureSubImage1DEXT, GLuint texture,
                                GLenum target, GLint level, GLint xoffset, GLsizei width,
                                GLenum format, GLsizei imageSize, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompressedTextureSubImage2DEXT, GLuint texture,
                                GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glCompressedTextureSubImage3DEXT, GLuint texture,
                                GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                GLenum format, GLsizei imageSize, const void *pixels);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureBufferRangeEXT, GLuint texture, GLenum target,
                                GLenum internalformat, GLuint buffer, GLintptr offset,
                                GLsizeiptr size);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureBufferEXT, GLuint texture, GLenum target,
                                GLenum internalformat, GLuint buffer);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureFoveationParametersQCOM, GLuint texture,
                                GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY,
                                GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
INSTANTIATE_FUNCTION_SERIALISED(void, glInvalidateTexImage, GLuint texture, GLint level);
INSTANTIATE_FUNCTION_SERIALISED(void, glInvalidateTexSubImage, GLuint texture, GLint level,
                                GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                GLsizei height, GLsizei depth);
