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
#include "strings/string_utils.h"

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
  SERIALISE_ELEMENT_LOCAL(texture, GetResourceManager()->GetID(TextureRes(GetCtx(), *textures)));

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glGenTextures(1, &real);

    GLResource res = TextureRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(texture, res);

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_NONE;
  }

  return true;
}

void WrappedOpenGL::glGenTextures(GLsizei n, GLuint *textures)
{
  m_Real.glGenTextures(n, textures);

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
  SERIALISE_ELEMENT_LOCAL(texture, GetResourceManager()->GetID(TextureRes(GetCtx(), *textures)));
  SERIALISE_ELEMENT(target);

  if(IsReplayingAndReading())
  {
    GLuint real = 0;
    m_Real.glCreateTextures(target, 1, &real);

    GLResource res = TextureRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(texture, res);

    m_Textures[live].resource = res;
    m_Textures[live].curType = TextureTarget(target);
    m_Textures[live].creationFlags |= TextureCategory::ShaderRead;
  }

  return true;
}

void WrappedOpenGL::glCreateTextures(GLenum target, GLsizei n, GLuint *textures)
{
  m_Real.glCreateTextures(target, n, textures);

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
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = TextureRes(GetCtx(), textures[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      GetResourceManager()->MarkCleanResource(res);
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  m_Real.glDeleteTextures(n, textures);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindTexture(SerialiserType &ser, GLenum target, GLuint textureHandle)
{
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));

  if(IsReplayingAndReading())
  {
    m_Real.glBindTexture(target, texture.name);

    if(IsLoading(m_State))
    {
      m_Textures[GetResourceManager()->GetID(texture)].curType = TextureTarget(target);
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ShaderRead;
    }
  }

  return true;
}

void WrappedOpenGL::glBindTexture(GLenum target, GLuint texture)
{
  m_Real.glBindTexture(target, texture);

  if(texture != 0 && GetResourceManager()->GetID(TextureRes(GetCtx(), texture)) == ResourceId())
    return;

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindTexture(ser, target, texture);

      chunk = scope.Get();
    }

    m_ContextRecord->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    m_Textures[GetResourceManager()->GetID(TextureRes(GetCtx(), texture))].curType =
        TextureTarget(target);
  }

  ContextData &cd = GetCtxData();

  if(texture == 0)
  {
    cd.m_TextureRecord[cd.m_TextureUnit] = NULL;
    return;
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *r = cd.m_TextureRecord[cd.m_TextureUnit] =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

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

      r->AddChunk(chunk);
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindTextures(SerialiserType &ser, GLuint first, GLsizei count,
                                             const GLuint *textureHandles)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);

  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  std::vector<GLResource> textures;

  if(ser.IsWriting())
  {
    textures.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      textures.push_back(TextureRes(GetCtx(), textureHandles[i]));
  }

  SERIALISE_ELEMENT(textures);

  if(IsReplayingAndReading())
  {
    std::vector<GLuint> texs;
    texs.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      texs.push_back(textures[i].name);

    m_Real.glBindTextures(first, count, texs.data());

    if(IsLoading(m_State))
    {
      for(GLsizei i = 0; i < count; i++)
        m_Textures[GetResourceManager()->GetID(textures[i])].creationFlags |=
            TextureCategory::ShaderRead;
    }
  }

  return true;
}

// glBindTextures doesn't provide a target, so can't be used to "init" a texture from glGenTextures
// which makes our lives a bit easier
void WrappedOpenGL::glBindTextures(GLuint first, GLsizei count, const GLuint *textures)
{
  m_Real.glBindTextures(first, count, textures);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindTextures(ser, first, count, textures);

    m_ContextRecord->AddChunk(scope.Get());

    for(GLsizei i = 0; i < count; i++)
      if(textures != NULL && textures[i] != 0)
        GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), textures[i]),
                                                          eFrameRef_Read);
  }

  if(IsCaptureMode(m_State))
  {
    for(GLsizei i = 0; i < count; i++)
    {
      if(textures == NULL || textures[i] == 0)
        GetCtxData().m_TextureRecord[first + i] = 0;
      else
        GetCtxData().m_TextureRecord[first + i] =
            GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), textures[i]));
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindMultiTextureEXT(SerialiserType &ser, GLenum texunit,
                                                    GLenum target, GLuint textureHandle)
{
  SERIALISE_ELEMENT(texunit);
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));

  if(IsReplayingAndReading())
  {
    m_Real.glBindMultiTextureEXT(texunit, target, texture.name);

    if(IsLoading(m_State) && texture.name)
    {
      m_Textures[GetResourceManager()->GetID(texture)].curType = TextureTarget(target);
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |= TextureCategory::ShaderRead;
    }
  }

  return true;
}

void WrappedOpenGL::glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
  m_Real.glBindMultiTextureEXT(texunit, target, texture);

  if(texture != 0 && GetResourceManager()->GetID(TextureRes(GetCtx(), texture)) == ResourceId())
    return;

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindMultiTextureEXT(ser, texunit, target, texture);

      chunk = scope.Get();
    }

    m_ContextRecord->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    m_Textures[GetResourceManager()->GetID(TextureRes(GetCtx(), texture))].curType =
        TextureTarget(target);
  }

  ContextData &cd = GetCtxData();

  if(texture == 0)
  {
    cd.m_TextureRecord[texunit - eGL_TEXTURE0] = NULL;
    return;
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *r = cd.m_TextureRecord[texunit - eGL_TEXTURE0] =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

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

  if(IsReplayingAndReading())
  {
    m_Real.glBindTextureUnit(texunit, texture.name);
  }

  return true;
}

void WrappedOpenGL::glBindTextureUnit(GLuint unit, GLuint texture)
{
  m_Real.glBindTextureUnit(unit, texture);

  if(texture != 0 && GetResourceManager()->GetID(TextureRes(GetCtx(), texture)) == ResourceId())
    return;

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindTextureUnit(ser, unit, texture);

    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }

  if(IsCaptureMode(m_State))
  {
    ContextData &cd = GetCtxData();

    if(texture == 0)
      cd.m_TextureRecord[unit] = NULL;
    else
      cd.m_TextureRecord[unit] =
          GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
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

  if(IsReplayingAndReading())
  {
    m_Real.glBindImageTexture(unit, texture.name, level, layered, layer, access, format);

    if(IsLoading(m_State))
      m_Textures[GetResourceManager()->GetID(texture)].creationFlags |=
          TextureCategory::ShaderReadWrite;
  }

  return true;
}

void WrappedOpenGL::glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                                       GLint layer, GLenum access, GLenum format)
{
  m_Real.glBindImageTexture(unit, texture, level, layered, layer, access, format);

  if(IsActiveCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_glBindImageTexture(ser, unit, texture, level, layered, layer, access, format);

      chunk = scope.Get();
    }

    m_ContextRecord->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glBindImageTextures(SerialiserType &ser, GLuint first, GLsizei count,
                                                  const GLuint *textureHandles)
{
  SERIALISE_ELEMENT(first);
  SERIALISE_ELEMENT(count);

  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  std::vector<GLResource> textures;

  if(ser.IsWriting())
  {
    textures.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      textures.push_back(TextureRes(GetCtx(), textureHandles[i]));
  }

  SERIALISE_ELEMENT(textures);

  if(IsReplayingAndReading())
  {
    std::vector<GLuint> texs;
    texs.reserve(count);
    for(GLsizei i = 0; i < count; i++)
      texs.push_back(textures[i].name);

    m_Real.glBindImageTextures(first, count, texs.data());

    if(IsLoading(m_State))
    {
      for(GLsizei i = 0; i < count; i++)
        m_Textures[GetResourceManager()->GetID(textures[i])].creationFlags |=
            TextureCategory::ShaderReadWrite;
    }
  }

  return true;
}

void WrappedOpenGL::glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures)
{
  m_Real.glBindImageTextures(first, count, textures);

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glBindImageTextures(ser, first, count, textures);

    m_ContextRecord->AddChunk(scope.Get());

    for(GLsizei i = 0; i < count; i++)
      if(textures != NULL && textures[i] != 0)
        GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), textures[i]),
                                                          eFrameRef_Read);
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

  if(IsReplayingAndReading())
  {
    m_Real.glTextureView(texture.name, target, origtexture.name, internalformat, minlevel,
                         numlevels, minlayer, numlayers);

    ResourceId liveTexId = GetResourceManager()->GetID(texture);
    ResourceId liveOrigId = GetResourceManager()->GetID(origtexture);

    m_Textures[liveTexId].curType = TextureTarget(target);
    m_Textures[liveTexId].internalFormat = internalformat;
    m_Textures[liveTexId].view = true;
    m_Textures[liveTexId].width = m_Textures[liveOrigId].width;
    m_Textures[liveTexId].height = m_Textures[liveOrigId].height;
    m_Textures[liveTexId].depth = m_Textures[liveOrigId].depth;
  }

  return true;
}

void WrappedOpenGL::glTextureView(GLuint texture, GLenum target, GLuint origtexture,
                                  GLenum internalformat, GLuint minlevel, GLuint numlevels,
                                  GLuint minlayer, GLuint numlayers)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureView(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer,
                       numlayers);

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
    origrecord->viewTextures.insert(record->GetResourceID());

    // illegal to re-type textures
    record->VerifyDataType(target);

    // mark the underlying resource as dirty to avoid tracking dirty across
    // aliased resources etc.
    if(IsBackgroundCapturing(m_State))
      GetResourceManager()->MarkDirtyResource(origrecord->GetResourceID());
    else
      m_MissingTracks.insert(origrecord->GetResourceID());
  }

  {
    ResourceId texId = GetResourceManager()->GetID(TextureRes(GetCtx(), texture));
    ResourceId viewedId = GetResourceManager()->GetID(TextureRes(GetCtx(), origtexture));

    m_Textures[texId].internalFormat = internalformat;
    m_Textures[texId].view = true;
    m_Textures[texId].dimension = m_Textures[viewedId].dimension;
    m_Textures[texId].width = m_Textures[viewedId].width;
    m_Textures[texId].height = m_Textures[viewedId].height;
    m_Textures[texId].depth = m_Textures[viewedId].depth;
    m_Textures[texId].curType = TextureTarget(target);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenerateTextureMipmapEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glGenerateTextureMipmapEXT(texture.name, target);
    else
      m_Real.glGenerateTextureMipmap(texture.name);

    if(IsLoading(m_State))
    {
      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%llu)", ToStr(gl_CurChunk).c_str(),
                                    ToStr(GetResourceManager()->GetID(texture)).c_str());
      draw.flags |= DrawFlags::GenMips;

      AddDrawcall(draw, true);

      m_ResourceUses[GetResourceManager()->GetID(texture)].push_back(
          EventUsage(m_CurEventID, ResourceUsage::GenMips));
    }
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
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glGenerateTextureMipmapEXT(ser, record->Resource.name, target);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(record->GetResourceID());
  }
}

void WrappedOpenGL::glGenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
  m_Real.glGenerateTextureMipmapEXT(texture, target);

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target);
}

void WrappedOpenGL::glGenerateTextureMipmap(GLuint texture)
{
  m_Real.glGenerateTextureMipmap(texture);

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE);
}

void WrappedOpenGL::glGenerateMipmap(GLenum target)
{
  m_Real.glGenerateMipmap(target);

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(GetCtxData().GetActiveTexRecord(), target);
}

void WrappedOpenGL::glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target)
{
  m_Real.glGenerateMultiTexMipmapEXT(texunit, target);

  if(IsCaptureMode(m_State))
    Common_glGenerateTextureMipmapEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target);
}

void WrappedOpenGL::glInvalidateTexImage(GLuint texture, GLint level)
{
  m_Real.glInvalidateTexImage(texture, level);

  if(IsBackgroundCapturing(m_State))
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
  else
    m_MissingTracks.insert(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
}

void WrappedOpenGL::glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset,
                                            GLint yoffset, GLint zoffset, GLsizei width,
                                            GLsizei height, GLsizei depth)
{
  m_Real.glInvalidateTexSubImage(texture, level, xoffset, yoffset, zoffset, width, height, depth);

  if(IsBackgroundCapturing(m_State))
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), texture));
  else
    m_MissingTracks.insert(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyImageSubData(SerialiserType &ser, GLuint srcHandle,
                                                 GLenum srcTarget, GLint srcLevel, GLint srcX,
                                                 GLint srcY, GLint srcZ, GLuint dstHandle,
                                                 GLenum dstTarget, GLint dstLevel, GLint dstX,
                                                 GLint dstY, GLint dstZ, GLsizei srcWidth,
                                                 GLsizei srcHeight, GLsizei srcDepth)
{
  SERIALISE_ELEMENT_LOCAL(srcName, TextureRes(GetCtx(), srcHandle));
  SERIALISE_ELEMENT(srcTarget);
  SERIALISE_ELEMENT(srcLevel);
  SERIALISE_ELEMENT(srcX);
  SERIALISE_ELEMENT(srcY);
  SERIALISE_ELEMENT(srcZ);
  SERIALISE_ELEMENT_LOCAL(dstName, TextureRes(GetCtx(), dstHandle));
  SERIALISE_ELEMENT(dstTarget);
  SERIALISE_ELEMENT(dstLevel);
  SERIALISE_ELEMENT(dstX);
  SERIALISE_ELEMENT(dstY);
  SERIALISE_ELEMENT(dstZ);
  SERIALISE_ELEMENT(srcWidth);
  SERIALISE_ELEMENT(srcHeight);
  SERIALISE_ELEMENT(srcDepth);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    m_Real.glCopyImageSubData(srcName.name, srcTarget, srcLevel, srcX, srcY, srcZ, dstName.name,
                              dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);

    if(IsLoading(m_State))
    {
      AddEvent();

      ResourceId srcid = GetResourceManager()->GetID(srcName);
      ResourceId dstid = GetResourceManager()->GetID(dstName);

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("%s(%llu, %llu)", ToStr(gl_CurChunk).c_str(),
                                    ToStr(srcid).c_str(), ToStr(dstid).c_str());
      draw.flags |= DrawFlags::Copy;

      draw.copySource = srcid;
      draw.copyDestination = dstid;

      AddDrawcall(draw, true);

      if(srcid == dstid)
      {
        m_ResourceUses[GetResourceManager()->GetLiveID(srcid)].push_back(
            EventUsage(m_CurEventID, ResourceUsage::Copy));
      }
      else
      {
        m_ResourceUses[GetResourceManager()->GetLiveID(srcid)].push_back(
            EventUsage(m_CurEventID, ResourceUsage::CopySrc));
        m_ResourceUses[GetResourceManager()->GetLiveID(dstid)].push_back(
            EventUsage(m_CurEventID, ResourceUsage::CopyDst));
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

  m_Real.glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget,
                            dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);

  if(IsActiveCapturing(m_State))
  {
    GLResourceRecord *srcrecord =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), srcName));
    GLResourceRecord *dstrecord =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), dstName));

    RDCASSERTMSG("Couldn't identify src texture. Unbound or bad GLuint?", srcrecord, srcName);
    RDCASSERTMSG("Couldn't identify dst texture. Unbound or bad GLuint?", dstrecord, dstName);

    if(srcrecord == NULL || dstrecord == NULL)
      return;

    USE_SCRATCH_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyImageSubData(ser, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName,
                                 dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight,
                                 srcDepth);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(dstrecord->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(dstrecord->GetResourceID(), eFrameRef_Read);
    GetResourceManager()->MarkResourceFrameReferenced(srcrecord->GetResourceID(), eFrameRef_Read);
  }
  else if(IsBackgroundCapturing(m_State))
  {
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), dstName));
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureSubImage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target, GLint level, GLint xoffset,
                                                         GLint x, GLint y, GLsizei width)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glCopyTextureSubImage1DEXT(texture.name, target, level, xoffset, x, y, width);
    else
      m_Real.glCopyTextureSubImage1D(texture.name, level, xoffset, x, y, width);
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

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }
}

void WrappedOpenGL::glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level,
                                               GLint xoffset, GLint x, GLint y, GLsizei width)
{
  m_Real.glCopyTextureSubImage1DEXT(texture, target, level, xoffset, x, y, width);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, x, y, width);
}

void WrappedOpenGL::glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x,
                                            GLint y, GLsizei width)
{
  m_Real.glCopyTextureSubImage1D(texture, level, xoffset, x, y, width);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, x, y, width);
}

void WrappedOpenGL::glCopyMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                                GLint xoffset, GLint x, GLint y, GLsizei width)
{
  m_Real.glCopyMultiTexSubImage1DEXT(texunit, target, level, xoffset, x, y, width);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                      level, xoffset, x, y, width);
}

void WrappedOpenGL::glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y,
                                        GLsizei width)
{
  m_Real.glCopyTexSubImage1D(target, level, xoffset, x, y, width);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage1DEXT(GetCtxData().GetActiveTexRecord(), eGL_NONE, level, xoffset,
                                      x, y, width);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureSubImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target, GLint level, GLint xoffset,
                                                         GLint yoffset, GLint x, GLint y,
                                                         GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glCopyTextureSubImage2DEXT(texture.name, target, level, xoffset, yoffset, x, y, width,
                                        height);
    else
      m_Real.glCopyTextureSubImage2D(texture.name, level, xoffset, yoffset, x, y, width, height);
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

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }
}

void WrappedOpenGL::glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLint x, GLint y,
                                               GLsizei width, GLsizei height)
{
  m_Real.glCopyTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, x, y, width, height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                            GLint x, GLint y, GLsizei width, GLsizei height)
{
  m_Real.glCopyTextureSubImage2D(texture, level, xoffset, yoffset, x, y, width, height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint x, GLint y,
                                                GLsizei width, GLsizei height)
{
  m_Real.glCopyMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, x, y, width, height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                      level, xoffset, yoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                        GLint x, GLint y, GLsizei width, GLsizei height)
{
  m_Real.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage2DEXT(GetCtxData().GetActiveTexRecord(), target, level, xoffset,
                                      yoffset, x, y, width, height);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureSubImage3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                         GLenum target, GLint level, GLint xoffset,
                                                         GLint yoffset, GLint zoffset, GLint x,
                                                         GLint y, GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
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

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glCopyTextureSubImage3DEXT(texture.name, target, level, xoffset, yoffset, zoffset, x,
                                        y, width, height);
    else
      m_Real.glCopyTextureSubImage3D(texture.name, level, xoffset, yoffset, zoffset, x, y, width,
                                     height);
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
    m_MissingTracks.insert(record->GetResourceID());
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureSubImage3DEXT(ser, record->Resource.name, target, level, xoffset,
                                         yoffset, zoffset, x, y, width, height);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }
}

void WrappedOpenGL::glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level,
                                               GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                               GLint y, GLsizei width, GLsizei height)
{
  m_Real.glCopyTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, x, y, width,
                                    height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, zoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset,
                                            GLint yoffset, GLint zoffset, GLint x, GLint y,
                                            GLsizei width, GLsizei height)
{
  m_Real.glCopyTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, x, y, width, height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, zoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint zoffset,
                                                GLint x, GLint y, GLsizei width, GLsizei height)
{
  m_Real.glCopyMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, x, y, width,
                                     height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                      level, xoffset, yoffset, zoffset, x, y, width, height);
}

void WrappedOpenGL::glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLint x, GLint y, GLsizei width,
                                        GLsizei height)
{
  m_Real.glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);

  if(IsCaptureMode(m_State))
    Common_glCopyTextureSubImage3DEXT(GetCtxData().GetActiveTexRecord(), target, level, xoffset,
                                      yoffset, zoffset, x, y, width, height);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameteriEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname);

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(pname == GL_DEPTH_STENCIL_TEXTURE_MODE || pname == GL_TEXTURE_COMPARE_FUNC ||
     pname == GL_TEXTURE_COMPARE_MODE || pname == GL_TEXTURE_MIN_FILTER ||
     pname == GL_TEXTURE_MAG_FILTER || pname == GL_TEXTURE_SWIZZLE_R ||
     pname == GL_TEXTURE_SWIZZLE_G || pname == GL_TEXTURE_SWIZZLE_B || pname == GL_TEXTURE_SWIZZLE_A ||
     pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T || pname == GL_TEXTURE_WRAP_R)
  {
    SERIALISE_ELEMENT_TYPED(GLenum, param);
  }
  else
  {
    SERIALISE_ELEMENT(param);
  }

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glTextureParameteriEXT(texture.name, target, pname, param);
    else
      m_Real.glTextureParameteri(texture.name, pname, param);
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
    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureParameteri(texture, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        param);
}

void WrappedOpenGL::glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
  m_Real.glTextureParameteriEXT(texture, target, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname, param);
}

void WrappedOpenGL::glTexParameteri(GLenum target, GLenum pname, GLint param)
{
  m_Real.glTexParameteri(target, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(GetCtxData().GetActiveTexRecord(), target, pname, param);
}

void WrappedOpenGL::glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param)
{
  m_Real.glMultiTexParameteriEXT(texunit, target, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameteriEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                  pname, param);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterivEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLenum pname,
                                                      const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, FIXED_COUNT(numParams(pname)));

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glTextureParameterivEXT(texture.name, target, pname, params);
    else
      m_Real.glTextureParameteriv(texture.name, pname, params);
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
    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureParameterivEXT(texture, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameteriv(GLuint texture, GLenum pname, const GLint *params)
{
  m_Real.glTextureParameteriv(texture, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
  m_Real.glTexParameteriv(target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(GetCtxData().GetActiveTexRecord(), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname,
                                             const GLint *params)
{
  m_Real.glMultiTexParameterivEXT(texunit, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterivEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                   pname, params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterIivEXT(SerialiserType &ser, GLuint textureHandle,
                                                       GLenum target, GLenum pname,
                                                       const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, FIXED_COUNT(numParams(pname)));

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glTextureParameterIivEXT(texture.name, target, pname, params);
    else
      m_Real.glTextureParameterIiv(texture.name, pname, params);
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
    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureParameterIivEXT(texture, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameterIiv(GLuint texture, GLenum pname, const GLint *params)
{
  m_Real.glTextureParameterIiv(texture, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameterIiv(GLenum target, GLenum pname, const GLint *params)
{
  m_Real.glTexParameterIiv(target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(GetCtxData().GetActiveTexRecord(), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname,
                                              const GLint *params)
{
  m_Real.glMultiTexParameterIivEXT(texunit, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIivEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                    pname, params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterIuivEXT(SerialiserType &ser, GLuint textureHandle,
                                                        GLenum target, GLenum pname,
                                                        const GLuint *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, FIXED_COUNT(numParams(pname)));

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glTextureParameterIuivEXT(texture.name, target, pname, params);
    else
      m_Real.glTextureParameterIuiv(texture.name, pname, params);
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
    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureParameterIuivEXT(texture, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint *params)
{
  m_Real.glTextureParameterIuiv(texture, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params)
{
  m_Real.glTexParameterIuiv(target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(GetCtxData().GetActiveTexRecord(), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname,
                                               const GLuint *params)
{
  m_Real.glMultiTexParameterIuivEXT(texunit, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterIuivEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                     pname, params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterfEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLenum pname, GLfloat param)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glTextureParameterfEXT(texture.name, target, pname, param);
    else
      m_Real.glTextureParameterf(texture.name, pname, param);
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
    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureParameterfEXT(texture, target, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname, param);
}

void WrappedOpenGL::glTextureParameterf(GLuint texture, GLenum pname, GLfloat param)
{
  m_Real.glTextureParameterf(texture, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        param);
}

void WrappedOpenGL::glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
  m_Real.glTexParameterf(target, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(GetCtxData().GetActiveTexRecord(), target, pname, param);
}

void WrappedOpenGL::glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param)
{
  m_Real.glMultiTexParameterfEXT(texunit, target, pname, param);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                  pname, param);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureParameterfvEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLenum pname,
                                                      const GLfloat *params)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT_ARRAY(params, FIXED_COUNT(numParams(pname)));

  if(IsReplayingAndReading())
  {
    if(target != eGL_NONE)
      m_Real.glTextureParameterfvEXT(texture.name, target, pname, params);
    else
      m_Real.glTextureParameterfv(texture.name, pname, params);
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
    m_ContextRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureParameterfvEXT(texture, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, pname,
        params);
}

void WrappedOpenGL::glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *params)
{
  m_Real.glTextureParameterfv(texture, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, pname,
        params);
}

void WrappedOpenGL::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
  m_Real.glTexParameterfv(target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(GetCtxData().GetActiveTexRecord(), target, pname, params);
}

void WrappedOpenGL::glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname,
                                             const GLfloat *params)
{
  m_Real.glMultiTexParameterfvEXT(texunit, target, pname, params);

  if(IsCaptureMode(m_State))
    Common_glTextureParameterfvEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                   pname, params);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glPixelStorei(SerialiserType &ser, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(pname);
  SERIALISE_ELEMENT(param);

  if(IsReplayingAndReading())
  {
    m_Real.glPixelStorei(pname, param);
  }

  return true;
}

void WrappedOpenGL::glPixelStorei(GLenum pname, GLint param)
{
  m_Real.glPixelStorei(pname, param);

  // except for capturing frames we ignore this and embed the relevant
  // parameters in the chunks that reference them.
  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glPixelStorei(ser, pname, param);

    m_ContextRecord->AddChunk(scope.Get());
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

  if(IsReplayingAndReading())
    m_Real.glActiveTexture(texture);

  return true;
}

void WrappedOpenGL::glActiveTexture(GLenum texture)
{
  m_Real.glActiveTexture(texture);

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

    m_ContextRecord->AddChunk(chunk);
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT_TYPED(GLenum, internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(border);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

    if(!unpack.FastPath(width, 0, 0, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, 0, 0, format, type);
  }

  size_t subimageSize = GetByteSize(width, 1, 1, format, type);

  SERIALISE_ELEMENT_ARRAY(pixels, subimageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    GLenum intFmt = (GLenum)internalformat;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, intFmt, format);
    internalformat = intFmt;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 1;
      m_Textures[liveId].internalFormat = (GLenum)internalformat;
      m_Textures[liveId].emulated = emulated;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint align = 1;
    m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    m_Real.glTextureImage1DEXT(texture.name, target, level, internalformat, width, border, format,
                               type, pixels);

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
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

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

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
  }
}

void WrappedOpenGL::glTextureImage1DEXT(GLuint texture, GLenum target, GLint level,
                                        GLint internalformat, GLsizei width, GLint border,
                                        GLenum format, GLenum type, const void *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glTextureImage1DEXT(texture, target, level, internalformat, width, border, format, type,
                             pixels);

  Common_glTextureImage1DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                             level, internalformat, width, border, format, type, pixels);
}

void WrappedOpenGL::glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glTexImage1D(target, level, internalformat, width, border, format, type, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glMultiTexImage1DEXT(texunit, target, level, internalformat, width, border, format, type,
                              pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT_TYPED(GLenum, internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(border);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

    if(!unpack.FastPath(width, 0, 0, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, height, 0, format, type);
  }

  size_t subimageSize = GetByteSize(width, height, 1, format, type);

  SERIALISE_ELEMENT_ARRAY(pixels, subimageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    GLenum intFmt = (GLenum)internalformat;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, intFmt, format);
    internalformat = intFmt;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = (GLenum)internalformat;
      m_Textures[liveId].emulated = emulated;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint align = 1;
    m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    if(TextureBinding(target) != eGL_TEXTURE_BINDING_CUBE_MAP)
    {
      m_Real.glTextureImage2DEXT(texture.name, target, level, internalformat, width, height, border,
                                 format, type, pixels);
    }
    else
    {
      GLenum ts[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      // special case handling for cubemaps, as we might have skipped the 'allocation' teximage
      // chunks to avoid
      // serialising tons of 'data upload' teximage chunks. Sigh.
      // Any further chunks & initial data can overwrite this, but cubemaps must be square so all
      // parameters will be the same.
      for(size_t i = 0; i < ARRAY_COUNT(ts); i++)
      {
        m_Real.glTextureImage2DEXT(texture.name, ts[i], level, internalformat, width, height,
                                   border, format, type, pixels);
      }
    }

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
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

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

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
  }
}

void WrappedOpenGL::glTextureImage2DEXT(GLuint texture, GLenum target, GLint level,
                                        GLint internalformat, GLsizei width, GLsizei height,
                                        GLint border, GLenum format, GLenum type, const void *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glTextureImage2DEXT(texture, target, level, internalformat, width, height, border, format,
                             type, pixels);

  Common_glTextureImage2DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                             level, internalformat, width, height, border, format, type, pixels);
}

void WrappedOpenGL::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLsizei height, GLint border, GLenum format, GLenum type,
                                 const GLvoid *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glMultiTexImage2DEXT(texunit, target, level, internalformat, width, height, border, format,
                              type, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT_TYPED(GLenum, internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(border);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

    if(!unpack.FastPath(width, height, depth, format, type))
      pixels = unpackedPixels = unpack.Unpack((byte *)pixels, width, height, depth, format, type);
  }

  size_t subimageSize = GetByteSize(width, height, depth, format, type);

  SERIALISE_ELEMENT_ARRAY(pixels, subimageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    GLenum intFmt = (GLenum)internalformat;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, intFmt, format);
    internalformat = intFmt;

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = depth;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 3;
      m_Textures[liveId].internalFormat = (GLenum)internalformat;
      m_Textures[liveId].emulated = emulated;
    }

    // for creation type chunks we forcibly don't use the unpack buffers as we
    // didn't track and set them up, so unbind it and either we provide data (in buf)
    // or just size the texture to be filled with data later (buf=NULL)
    GLuint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint align = 1;
    m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    m_Real.glTextureImage3DEXT(texture.name, target, level, internalformat, width, height, depth,
                               border, format, type, pixels);

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
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

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

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
  }
}

void WrappedOpenGL::glTextureImage3DEXT(GLuint texture, GLenum target, GLint level,
                                        GLint internalformat, GLsizei width, GLsizei height,
                                        GLsizei depth, GLint border, GLenum format, GLenum type,
                                        const void *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glTextureImage3DEXT(texture, target, level, internalformat, width, height, depth, border,
                             format, type, pixels);

  Common_glTextureImage3DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                             level, internalformat, width, height, depth, border, format, type,
                             pixels);
}

void WrappedOpenGL::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLsizei height, GLsizei depth, GLint border, GLenum format,
                                 GLenum type, const GLvoid *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glTexImage3D(target, level, internalformat, width, height, depth, border, format, type,
                      pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat, type);

  m_Real.glMultiTexImage3DEXT(texunit, target, level, internalformat, width, height, depth, border,
                              format, type, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(border);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, true);

    if(!unpack.FastPathCompressed(width, 0, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, 0, 0, imageSize);
  }

  SERIALISE_ELEMENT_ARRAY(pixels, (uint32_t &)imageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

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
      databuf = &m_ScratchBuf[0];
    }

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint align = 1;
    m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    m_Real.glCompressedTextureImage1DEXT(texture.name, target, level, internalformat, width, border,
                                         imageSize, databuf);

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
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

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedTextureImage1DEXT(texture, target, level, internalformat, width, border,
                                       imageSize, pixels);

  Common_glCompressedTextureImage1DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                       target, level, internalformat, width, border, imageSize,
                                       pixels);
}

void WrappedOpenGL::glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLint border, GLsizei imageSize,
                                           const GLvoid *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedTexImage1D(target, level, internalformat, width, border, imageSize, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedMultiTexImage1DEXT(texunit, target, level, internalformat, width, border,
                                        imageSize, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
    if(record != NULL)
      Common_glCompressedTextureImage1DEXT(record->GetResourceID(), target, level, internalformat,
                                           width, border, imageSize, pixels);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to slot %u",
             texunit - eGL_TEXTURE0);
  }
}

void WrappedOpenGL::StoreCompressedTexData(ResourceId texId, GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset, GLint zoffset,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLenum format, GLsizei imageSize, const void *pixels)
{
  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;
  GLint unpackbuf = 0;

  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(unpackbuf == 0 && pixels != NULL)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

    if(unpack.FastPathCompressed(width, height, depth))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, width, height, depth, imageSize);
  }

  if(unpackbuf != 0)
    srcPixels = (byte *)m_Real.glMapBufferRange(eGL_PIXEL_UNPACK_BUFFER, (GLintptr)pixels,
                                                imageSize, eGL_MAP_READ_BIT);

  if(srcPixels)
  {
    string error;

    // Only the trivial case is handled yet.
    if(xoffset == 0 && yoffset == 0)
    {
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
          auto &cd = m_Textures[texId].compressedData;
          auto &cdData = cd[level];
          GLint zoff = IsCubeFace(target) ? CubeTargetIndex(target) : zoffset;
          size_t startOffset = imageSize * zoff;
          if(cdData.size() < startOffset + imageSize)
            cdData.resize(startOffset + imageSize);
          memcpy(cdData.data() + startOffset, srcPixels, imageSize);
        }
        else
        {
          error = StringFormat::Fmt("depth (%d)", depth);
        }
      }
      else if(target == GL_TEXTURE_3D)
      {
        if(zoffset == 0)
        {
          RDCASSERT(GetCompressedByteSize(width, height, depth, format) == (size_t)imageSize);
          auto &cd = m_Textures[texId].compressedData;
          auto &cdData = cd[level];
          cdData.resize(imageSize);
          memcpy(cdData.data(), srcPixels, imageSize);
        }
        else
        {
          error = StringFormat::Fmt("zoffset (%d)", zoffset);
        }
      }
      else
      {
        error = "target";
      }
    }
    else
    {
      error = StringFormat::Fmt("xoffset (%d) and/or yoffset (%d)", xoffset, yoffset);
    }

    if(unpackbuf != 0)
      m_Real.glUnmapBuffer(eGL_PIXEL_UNPACK_BUFFER);

    if(!error.empty())
      RDCWARN("StoreCompressedTexData: Unexpected %s (tex:%llu, target:%s)", error.c_str(), texId,
              ToStr(target).c_str());
  }
  else
  {
    RDCWARN("StoreCompressedTexData: No source pixels to copy from (tex:%llu, target:%s)", texId,
            ToStr(target).c_str());
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(border);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, true);

    if(!unpack.FastPathCompressed(width, height, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, height, 0, imageSize);
  }

  SERIALISE_ELEMENT_ARRAY(pixels, (uint32_t &)imageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    const void *databuf = pixels;

    if(IsGLES)
      StoreCompressedTexData(GetResourceManager()->GetID(texture), target, level, 0, 0, 0, width,
                             height, 0, internalformat, imageSize, pixels);

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(pixels == NULL)
    {
      if(m_ScratchBuf.size() < (size_t)imageSize)
        m_ScratchBuf.resize(imageSize);
      databuf = &m_ScratchBuf[0];
    }

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint align = 1;
    m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    if(TextureBinding(target) != eGL_TEXTURE_BINDING_CUBE_MAP)
    {
      m_Real.glCompressedTextureImage2DEXT(texture.name, target, level, internalformat, width,
                                           height, border, imageSize, databuf);
    }
    else
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
        m_Real.glCompressedTextureImage2DEXT(texture.name, ts[i], level, internalformat, width,
                                             height, border, imageSize, databuf);
      }
    }

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    if(IsGLES)
      StoreCompressedTexData(record->GetResourceID(), target, level, 0, 0, 0, width, height, 0,
                             internalformat, imageSize, pixels);

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

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedTextureImage2DEXT(texture, target, level, internalformat, width, height,
                                       border, imageSize, pixels);

  Common_glCompressedTextureImage2DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                       target, level, internalformat, width, height, border,
                                       imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLsizei height, GLint border,
                                           GLsizei imageSize, const GLvoid *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize,
                                pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedMultiTexImage2DEXT(texunit, target, level, internalformat, width, height,
                                        border, imageSize, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(border);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, true);

    if(!unpack.FastPathCompressed(width, height, depth))
      pixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, width, height, depth, imageSize);
  }

  SERIALISE_ELEMENT_ARRAY(pixels, (uint32_t &)imageSize);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    const void *databuf = pixels;

    if(IsGLES)
      StoreCompressedTexData(GetResourceManager()->GetID(texture), target, level, 0, 0, 0, width,
                             height, depth, internalformat, imageSize, pixels);

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(pixels == NULL)
    {
      if(m_ScratchBuf.size() < (size_t)imageSize)
        m_ScratchBuf.resize(imageSize);
      databuf = &m_ScratchBuf[0];
    }

    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&unpackbuf);
    m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint align = 1;
    m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    m_Real.glCompressedTextureImage3DEXT(texture.name, target, level, internalformat, width, height,
                                         depth, border, imageSize, databuf);

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
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
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
    fromunpackbuf = (unpackbuf != 0);
  }

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    if(IsGLES)
      StoreCompressedTexData(record->GetResourceID(), target, level, 0, 0, 0, width, height, depth,
                             internalformat, imageSize, pixels);

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

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(IsActiveCapturing(m_State))
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }

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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedTextureImage3DEXT(texture, target, level, internalformat, width, height, depth,
                                       border, imageSize, pixels);

  Common_glCompressedTextureImage3DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                       target, level, internalformat, width, height, depth, border,
                                       imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLint border, GLsizei imageSize, const GLvoid *pixels)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border,
                                imageSize, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glCompressedMultiTexImage3DEXT(texunit, target, level, internalformat, width, height,
                                        depth, border, imageSize, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(border);

  if(IsReplayingAndReading())
  {
    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 1;
      m_Textures[liveId].internalFormat = internalformat;
    }

    m_Real.glCopyTextureImage1DEXT(texture.name, target, level, internalformat, x, y, width, border);
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
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glTextureImage1DEXT);
      Serialise_glTextureImage1DEXT(ser, record->Resource.name, target, level, internalformat,
                                    width, border, GetBaseFormat(internalformat),
                                    GetDataType(internalformat), NULL);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureImage1DEXT(ser, record->Resource.name, target, level, internalformat, x,
                                      y, width, border);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }

  if(level == 0)
  {
    ResourceId texId = record->GetResourceID();

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
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

  m_Real.glCopyTextureImage1DEXT(texture, target, level, internalformat, x, y, width, border);

  Common_glCopyTextureImage1DEXT(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
      internalformat, x, y, width, border);
}

void WrappedOpenGL::glCopyMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                             GLenum internalformat, GLint x, GLint y, GLsizei width,
                                             GLint border)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

  m_Real.glCopyMultiTexImage1DEXT(texunit, target, level, internalformat, x, y, width, border);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glCopyTextureImage1DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                   level, internalformat, x, y, width, border);
}

void WrappedOpenGL::glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                     GLint y, GLsizei width, GLint border)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

  m_Real.glCopyTexImage1D(target, level, internalformat, x, y, width, border);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glCopyTextureImage1DEXT(GetCtxData().GetActiveTexRecord(), target, level, internalformat,
                                   x, y, width, border);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCopyTextureImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLint level,
                                                      GLenum internalformat, GLint x, GLint y,
                                                      GLsizei width, GLsizei height, GLint border)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(x);
  SERIALISE_ELEMENT(y);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(border);

  if(IsReplayingAndReading())
  {
    if(level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = internalformat;
    }

    m_Real.glCopyTextureImage2DEXT(texture.name, target, level, internalformat, x, y, width, height,
                                   border);
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
    if(record)
    {
      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(GLChunk::glTextureImage2DEXT);
      Serialise_glTextureImage2DEXT(ser, record->Resource.name, target, level, internalformat,
                                    width, height, border, GetBaseFormat(internalformat),
                                    GetDataType(internalformat), NULL);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
  }
  else if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glCopyTextureImage2DEXT(ser, record->Resource.name, target, level, internalformat, x,
                                      y, width, height, border);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(record->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
  }

  if(level == 0)
  {
    ResourceId texId = record->GetResourceID();

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
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

  m_Real.glCopyTextureImage2DEXT(texture, target, level, internalformat, x, y, width, height, border);

  Common_glCopyTextureImage2DEXT(
      GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
      internalformat, x, y, width, height, border);
}

void WrappedOpenGL::glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                             GLenum internalformat, GLint x, GLint y, GLsizei width,
                                             GLsizei height, GLint border)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

  m_Real.glCopyMultiTexImage2DEXT(texunit, target, level, internalformat, x, y, width, height,
                                  border);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glCopyTextureImage2DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                   level, internalformat, x, y, width, height, border);
}

void WrappedOpenGL::glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                     GLint y, GLsizei width, GLsizei height, GLint border)
{
  internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

  m_Real.glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glCopyTextureImage2DEXT(GetCtxData().GetActiveTexRecord(), target, level, internalformat,
                                   x, y, width, height, border);
}

#pragma endregion

#pragma region Texture Creation(glTexStorage *)

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorage1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                    GLenum target, GLsizei levels,
                                                    GLenum internalformat, GLsizei width)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(levels);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = 1;
    m_Textures[liveId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 1;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;

    if(target != eGL_NONE)
      m_Real.glTextureStorage1DEXT(texture.name, target, levels, internalformat, width);
    else
      m_Real.glTextureStorage1D(texture.name, levels, internalformat, width);
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
  }
}

void WrappedOpenGL::glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureStorage1DEXT(texture, target, levels, internalformat, width);

  Common_glTextureStorage1DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                               levels, internalformat, width);
}

void WrappedOpenGL::glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat,
                                       GLsizei width)
{
  internalformat = GetSizedFormat(m_Real, eGL_NONE, internalformat);

  m_Real.glTextureStorage1D(texture, levels, internalformat, width);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage1DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, levels, internalformat, width);
}

void WrappedOpenGL::glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexStorage1D(target, levels, internalformat, width);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(levels);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = 1;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;

    if(target != eGL_NONE)
      m_Real.glTextureStorage2DEXT(texture.name, target, levels, internalformat, width, height);
    else
      m_Real.glTextureStorage2D(texture.name, levels, internalformat, width, height);
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
  }
}

void WrappedOpenGL::glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width, GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);

  Common_glTextureStorage2DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                               levels, internalformat, width, height);
}

void WrappedOpenGL::glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat,
                                       GLsizei width, GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, eGL_NONE, internalformat);

  m_Real.glTextureStorage2D(texture, levels, internalformat, width, height);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage2DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, levels, internalformat, width, height);
}

void WrappedOpenGL::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                                   GLsizei width, GLsizei height)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexStorage2D(target, levels, internalformat, width, height);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(levels);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = depth;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 3;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;

    if(target != eGL_NONE)
      m_Real.glTextureStorage3DEXT(texture.name, target, levels, internalformat, width, height,
                                   depth);
    else
      m_Real.glTextureStorage3D(texture.name, levels, internalformat, width, height, depth);
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
  }
}

void WrappedOpenGL::glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width, GLsizei height,
                                          GLsizei depth)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);

  Common_glTextureStorage3DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                               levels, internalformat, width, height, depth);
}

void WrappedOpenGL::glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat,
                                       GLsizei width, GLsizei height, GLsizei depth)
{
  internalformat = GetSizedFormat(m_Real, eGL_NONE, internalformat);

  m_Real.glTextureStorage3D(texture, levels, internalformat, width, height, depth);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage3DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, levels, internalformat, width, height, depth);
}

void WrappedOpenGL::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat,
                                   GLsizei width, GLsizei height, GLsizei depth)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexStorage3D(target, levels, internalformat, width, height, depth);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT_TYPED(bool, fixedsamplelocations);

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].samples = samples;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;

    if(target != eGL_NONE)
      m_Real.glTextureStorage2DMultisampleEXT(texture.name, target, samples, internalformat, width,
                                              height, fixedsamplelocations);
    else
      m_Real.glTextureStorage2DMultisample(texture.name, samples, internalformat, width, height,
                                           fixedsamplelocations);
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
  }
}

void WrappedOpenGL::glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                     GLenum internalformat, GLsizei width,
                                                     GLsizei height, GLboolean fixedsamplelocations)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureStorage2DMultisampleEXT(texture, target, samples, internalformat, width, height,
                                          fixedsamplelocations);

  Common_glTextureStorage2DMultisampleEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                          target, samples, internalformat, width, height,
                                          fixedsamplelocations);
}

void WrappedOpenGL::glTextureStorage2DMultisample(GLuint texture, GLsizei samples,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLboolean fixedsamplelocations)
{
  internalformat = GetSizedFormat(m_Real, eGL_NONE, internalformat);

  m_Real.glTextureStorage2DMultisample(texture, samples, internalformat, width, height,
                                       fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage2DMultisampleEXT(
        GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), eGL_NONE, samples,
        internalformat, width, height, fixedsamplelocations);
}

void WrappedOpenGL::glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height,
                                              GLboolean fixedsamplelocations)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexStorage2DMultisample(target, samples, internalformat, width, height,
                                   fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexImage2DMultisample(target, samples, internalformat, width, height,
                                 fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    // assuming texstorage is equivalent to teximage (this is not true in the case where someone
    // tries to re-size an image by re-calling teximage).
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT_TYPED(bool, fixedsamplelocations);

  if(IsReplayingAndReading())
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, texture.name, target, internalformat, dummy);

    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = depth;
    m_Textures[liveId].samples = samples;
    if(target != eGL_NONE)
      m_Textures[liveId].curType = TextureTarget(target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalformat;
    m_Textures[liveId].emulated = emulated;

    if(target != eGL_NONE)
      m_Real.glTextureStorage3DMultisampleEXT(texture.name, target, samples, internalformat, width,
                                              height, depth, fixedsamplelocations);
    else
      m_Real.glTextureStorage3DMultisample(texture.name, samples, internalformat, width, height,
                                           depth, fixedsamplelocations);
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
  }
}

void WrappedOpenGL::glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples,
                                                     GLenum internalformat, GLsizei width,
                                                     GLsizei height, GLsizei depth,
                                                     GLboolean fixedsamplelocations)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureStorage3DMultisampleEXT(texture, target, samples, internalformat, width, height,
                                          depth, fixedsamplelocations);

  Common_glTextureStorage3DMultisampleEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                          target, samples, internalformat, width, height, depth,
                                          fixedsamplelocations);
}

void WrappedOpenGL::glTextureStorage3DMultisample(GLuint texture, GLsizei samples,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLsizei depth,
                                                  GLboolean fixedsamplelocations)
{
  internalformat = GetSizedFormat(m_Real, eGL_NONE, internalformat);

  m_Real.glTextureStorage3DMultisample(texture, samples, internalformat, width, height, depth,
                                       fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");
  else
    Common_glTextureStorage3DMultisampleEXT(
        GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), eGL_NONE, samples,
        internalformat, width, height, depth, fixedsamplelocations);
}

void WrappedOpenGL::glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height, GLsizei depth,
                                              GLboolean fixedsamplelocations)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexStorage3DMultisample(target, samples, internalformat, width, height, depth,
                                   fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTexImage3DMultisample(target, samples, internalformat, width, height, depth,
                                 fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    // assuming texstorage is equivalent to teximage (this is not true in the case where someone
    // tries to re-size an image by re-calling teximage).
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(format);
  SERIALISE_ELEMENT(type);

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

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
    ser.Serialise("pixels", pixels, subimageSize, SerialiserFlags::AllocateMemory);
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset);
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(&m_Real, false);
      ResetPixelUnpackState(m_Real, false, 1);
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
      ResourceId liveId = GetResourceManager()->GetID(texture);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        format = eGL_RED;
    }

    if(target != eGL_NONE)
      m_Real.glTextureSubImage1DEXT(texture.name, target, level, xoffset, width, format, type,
                                    pixels ? pixels : (const void *)UnpackOffset);
    else
      m_Real.glTextureSubImage1D(texture.name, level, xoffset, width, format, type,
                                 pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(&m_Real, false);

      FreeAlignedBuffer((byte *)pixels);
    }
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureSubImage1DEXT(texture, target, level, xoffset, width, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, width, format, type, pixels);
}

void WrappedOpenGL::glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width,
                                        GLenum format, GLenum type, const void *pixels)
{
  m_Real.glTextureSubImage1D(texture, level, xoffset, width, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, width, format, type, pixels);
}

void WrappedOpenGL::glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width,
                                    GLenum format, GLenum type, const void *pixels)
{
  m_Real.glTexSubImage1D(target, level, xoffset, width, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(GetCtxData().GetActiveTexRecord(), target, level, xoffset, width,
                                  format, type, pixels);
}

void WrappedOpenGL::glMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                            GLint xoffset, GLsizei width, GLenum format,
                                            GLenum type, const void *pixels)
{
  m_Real.glMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage1DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                  level, xoffset, width, format, type, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureSubImage2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLint level, GLint xoffset,
                                                     GLint yoffset, GLsizei width, GLsizei height,
                                                     GLenum format, GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

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
    ser.Serialise("pixels", pixels, subimageSize, SerialiserFlags::AllocateMemory);
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset);
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(&m_Real, false);
      ResetPixelUnpackState(m_Real, false, 1);
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
      ResourceId liveId = GetResourceManager()->GetID(texture);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        format = eGL_RED;
    }

    if(target != eGL_NONE)
      m_Real.glTextureSubImage2DEXT(texture.name, target, level, xoffset, yoffset, width, height,
                                    format, type, pixels ? pixels : (const void *)UnpackOffset);
    else
      m_Real.glTextureSubImage2D(texture.name, level, xoffset, yoffset, width, height, format, type,
                                 pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(&m_Real, false);

      FreeAlignedBuffer((byte *)pixels);
    }
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height, format,
                                type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, width, height, format, type, pixels);
}

void WrappedOpenGL::glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLsizei width, GLsizei height, GLenum format, GLenum type,
                                        const void *pixels)
{
  m_Real.glTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, width, height, format, type, pixels);
}

void WrappedOpenGL::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLsizei width, GLsizei height, GLenum format, GLenum type,
                                    const void *pixels)
{
  m_Real.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(GetCtxData().GetActiveTexRecord(), target, level, xoffset,
                                  yoffset, width, height, format, type, pixels);
}

void WrappedOpenGL::glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset,
                                            GLint yoffset, GLsizei width, GLsizei height,
                                            GLenum format, GLenum type, const void *pixels)
{
  m_Real.glMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height, format,
                                 type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage2DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                  level, xoffset, yoffset, width, height, format, type, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureSubImage3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                     GLenum target, GLint level, GLint xoffset,
                                                     GLint yoffset, GLint zoffset, GLsizei width,
                                                     GLsizei height, GLsizei depth, GLenum format,
                                                     GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, false);

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
    ser.Serialise("pixels", pixels, subimageSize, SerialiserFlags::AllocateMemory);
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset);
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(&m_Real, false);
      ResetPixelUnpackState(m_Real, false, 1);
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
      ResourceId liveId = GetResourceManager()->GetID(texture);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        format = eGL_RED;
    }

    if(target != eGL_NONE)
      m_Real.glTextureSubImage3DEXT(texture.name, target, level, xoffset, yoffset, zoffset, width,
                                    height, depth, format, type,
                                    pixels ? pixels : (const void *)UnpackOffset);
    else
      m_Real.glTextureSubImage3D(texture.name, level, xoffset, yoffset, zoffset, width, height,
                                 depth, format, type, pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(&m_Real, false);

      FreeAlignedBuffer((byte *)pixels);
    }
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width, height,
                                depth, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void WrappedOpenGL::glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                        GLenum format, GLenum type, const void *pixels)
{
  m_Real.glTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth,
                             format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void WrappedOpenGL::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                    GLenum format, GLenum type, const void *pixels)
{
  m_Real.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                         type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(GetCtxData().GetActiveTexRecord(), target, level, xoffset,
                                  yoffset, zoffset, width, height, depth, format, type, pixels);
}

void WrappedOpenGL::glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset, GLint zoffset,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            GLenum format, GLenum type, const void *pixels)
{
  m_Real.glMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width, height,
                                 depth, format, type, pixels);

  if(IsCaptureMode(m_State))
    Common_glTextureSubImage3DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0], target,
                                  level, xoffset, yoffset, zoffset, width, height, depth, format,
                                  type, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureSubImage1DEXT(SerialiserType &ser,
                                                               GLuint textureHandle, GLenum target,
                                                               GLint level, GLint xoffset,
                                                               GLsizei width, GLenum format,
                                                               GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(format);

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, true);

    if(!unpack.FastPathCompressed(width, 0, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, 0, 0, imageSize);
  }

  uint64_t UnpackOffset = 0;

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels", pixels, (uint32_t &)imageSize, SerialiserFlags::AllocateMemory);
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset);
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(&m_Real, true);
      ResetPixelUnpackState(m_Real, true, 1);
    }

    if(target != eGL_NONE)
      m_Real.glCompressedTextureSubImage1DEXT(texture.name, target, level, xoffset, width, format,
                                              imageSize,
                                              pixels ? pixels : (const void *)UnpackOffset);
    else
      m_Real.glCompressedTextureSubImage1D(texture.name, level, xoffset, width, format, imageSize,
                                           pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(&m_Real, true);

      FreeAlignedBuffer((byte *)pixels);
    }
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glCompressedTextureSubImage1DEXT(texture, target, level, xoffset, width, format, imageSize,
                                          pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), target, level,
        xoffset, width, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset,
                                                  GLsizei width, GLenum format, GLsizei imageSize,
                                                  const void *pixels)
{
  m_Real.glCompressedTextureSubImage1D(texture, level, xoffset, width, format, imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, width, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset,
                                              GLsizei width, GLenum format, GLsizei imageSize,
                                              const void *pixels)
{
  m_Real.glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(GetCtxData().GetActiveTexRecord(), target, level,
                                            xoffset, width, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level,
                                                      GLint xoffset, GLsizei width, GLenum format,
                                                      GLsizei imageSize, const void *pixels)
{
  m_Real.glCompressedMultiTexSubImage1DEXT(texunit, target, level, xoffset, width, format,
                                           imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage1DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0],
                                            target, level, xoffset, width, format, imageSize, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureSubImage2DEXT(SerialiserType &ser,
                                                               GLuint textureHandle, GLenum target,
                                                               GLint level, GLint xoffset,
                                                               GLint yoffset, GLsizei width,
                                                               GLsizei height, GLenum format,
                                                               GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(level);
  SERIALISE_ELEMENT(xoffset);
  SERIALISE_ELEMENT(yoffset);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(format);

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, true);

    if(!unpack.FastPathCompressed(width, height, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, height, 0, imageSize);
  }

  uint64_t UnpackOffset = 0;

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels", pixels, (uint32_t &)imageSize, SerialiserFlags::AllocateMemory);
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset);
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && IsGLES)
    {
      StoreCompressedTexData(GetResourceManager()->GetID(texture), target, level, xoffset, yoffset,
                             0, width, height, 0, format, imageSize,
                             pixels ? pixels : (const void *)UnpackOffset);
    }

    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(&m_Real, true);
      ResetPixelUnpackState(m_Real, true, 1);
    }

    if(target != eGL_NONE)
      m_Real.glCompressedTextureSubImage2DEXT(texture.name, target, level, xoffset, yoffset, width,
                                              height, format, imageSize,
                                              pixels ? pixels : (const void *)UnpackOffset);
    else
      m_Real.glCompressedTextureSubImage2D(texture.name, level, xoffset, yoffset, width, height,
                                           format, imageSize,
                                           pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(&m_Real, true);

      FreeAlignedBuffer((byte *)pixels);
    }
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsGLES)
    StoreCompressedTexData(record->GetResourceID(), target, level, xoffset, yoffset, 0, width,
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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glCompressedTextureSubImage2DEXT(texture, target, level, xoffset, yoffset, width, height,
                                          format, imageSize, pixels);

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
  m_Real.glCompressedTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format,
                                       imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture)), eGL_NONE, level,
        xoffset, yoffset, width, height, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                              GLint yoffset, GLsizei width, GLsizei height,
                                              GLenum format, GLsizei imageSize, const void *pixels)
{
  m_Real.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                   imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(GetCtxData().GetActiveTexRecord(), target, level, xoffset,
                                            yoffset, width, height, format, imageSize, pixels);
}

void WrappedOpenGL::glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level,
                                                      GLint xoffset, GLint yoffset, GLsizei width,
                                                      GLsizei height, GLenum format,
                                                      GLsizei imageSize, const void *pixels)
{
  m_Real.glCompressedMultiTexSubImage2DEXT(texunit, target, level, xoffset, yoffset, width, height,
                                           format, imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage2DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0],
                                            target, level, xoffset, yoffset, width, height, format,
                                            imageSize, pixels);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCompressedTextureSubImage3DEXT(
    SerialiserType &ser, GLuint textureHandle, GLenum target, GLint level, GLint xoffset,
    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
    GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT_LOCAL(UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;

  if(ser.IsWriting() && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real, true);

    if(!unpack.FastPathCompressed(width, height, 0))
      pixels = unpackedPixels = unpack.UnpackCompressed((byte *)pixels, width, height, 0, imageSize);
  }

  uint64_t UnpackOffset = 0;

  // we have to do this by hand, since pixels might be a buffer offset instead of a real pointer.
  // That means the serialisation must be conditional, and the automatic deserialisation would kick
  // in.
  if(!UnpackBufBound)
  {
    ser.Serialise("pixels", pixels, (uint32_t &)imageSize, SerialiserFlags::AllocateMemory);
  }
  else
  {
    UnpackOffset = (uint64_t)pixels;
    SERIALISE_ELEMENT(UnpackOffset);
  }

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && IsGLES)
      StoreCompressedTexData(GetResourceManager()->GetID(texture), target, level, xoffset, yoffset,
                             zoffset, width, height, depth, format, imageSize,
                             pixels ? pixels : (const void *)UnpackOffset);

    PixelUnpackState unpack;

    // during capture if there was any significant unpack state we decomposed it
    // before serialising, so we need to set an empty unpack state.
    // Note that if we're unpacking from a buffer, we did nothing so we should
    // preserve the state.
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      unpack.Fetch(&m_Real, true);
      ResetPixelUnpackState(m_Real, true, 1);
    }

    if(target != eGL_NONE)
      m_Real.glCompressedTextureSubImage3DEXT(texture.name, target, level, xoffset, yoffset,
                                              zoffset, width, height, depth, format, imageSize,
                                              pixels ? pixels : (const void *)UnpackOffset);
    else
      m_Real.glCompressedTextureSubImage3D(texture.name, level, xoffset, yoffset, zoffset, width,
                                           height, depth, format, imageSize,
                                           pixels ? pixels : (const void *)UnpackOffset);

    // restore pixel unpack state
    if(!UnpackBufBound)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      unpack.Apply(&m_Real, true);

      FreeAlignedBuffer((byte *)pixels);
    }
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
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  if(IsGLES)
    StoreCompressedTexData(record->GetResourceID(), target, level, xoffset, yoffset, zoffset, width,
                           height, depth, format, imageSize, pixels);

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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
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
  m_Real.glCompressedTextureSubImage3DEXT(texture, target, level, xoffset, yoffset, zoffset, width,
                                          height, depth, format, imageSize, pixels);

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
  m_Real.glCompressedTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height,
                                       depth, format, imageSize, pixels);

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
  m_Real.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                                   format, imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage3DEXT(GetCtxData().GetActiveTexRecord(), target, level,
                                            xoffset, yoffset, zoffset, width, height, depth, format,
                                            imageSize, pixels);
}

void WrappedOpenGL::glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level,
                                                      GLint xoffset, GLint yoffset, GLint zoffset,
                                                      GLsizei width, GLsizei height, GLsizei depth,
                                                      GLenum format, GLsizei imageSize,
                                                      const void *pixels)
{
  m_Real.glCompressedMultiTexSubImage3DEXT(texunit, target, level, xoffset, yoffset, zoffset, width,
                                           height, depth, format, imageSize, pixels);

  if(IsCaptureMode(m_State))
    Common_glCompressedTextureSubImage3DEXT(GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0],
                                            target, level, xoffset, yoffset, zoffset, width, height,
                                            depth, format, imageSize, pixels);
}

#pragma endregion

#pragma region Tex Buffer

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureBufferRangeEXT(SerialiserType &ser, GLuint textureHandle,
                                                      GLenum target, GLenum internalformat,
                                                      GLuint bufferHandle, GLintptr offsetPtr,
                                                      GLsizeiptr sizePtr)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(offs, (uint64_t)offsetPtr);
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizePtr);

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      m_Textures[liveId].width =
          uint32_t(size) /
          uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat), GetDataType(internalformat)));
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].internalFormat = internalformat;
    }

    if(target != eGL_NONE)
      m_Real.glTextureBufferRangeEXT(texture.name, target, internalformat, buffer.name,
                                     (GLintptr)offs, (GLsizeiptr)size);
    else
      m_Real.glTextureBufferRange(texture.name, internalformat, buffer.name, (GLintptr)offs,
                                  (GLsizei)size);
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

    ResourceId bufid = GetResourceManager()->GetID(BufferRes(GetCtx(), buffer));

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
          bufRecord->viewTextures.insert(record->GetResourceID());
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
      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

      if(bufid != ResourceId())
      {
        m_MissingTracks.insert(bufid);
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
        bufRecord->viewTextures.insert(record->GetResourceID());
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
  }
}

void WrappedOpenGL::glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat,
                                            GLuint buffer, GLintptr offset, GLsizeiptr size)
{
  m_Real.glTextureBufferRangeEXT(texture, target, internalformat, buffer, offset, size);

  Common_glTextureBufferRangeEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                                 internalformat, buffer, offset, size);
}

void WrappedOpenGL::glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer,
                                         GLintptr offset, GLsizeiptr size)
{
  m_Real.glTextureBufferRange(texture, internalformat, buffer, offset, size);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");

  Common_glTextureBufferRangeEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)),
                                 eGL_NONE, internalformat, buffer, offset, size);
}

void WrappedOpenGL::glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer,
                                     GLintptr offset, GLsizeiptr size)
{
  m_Real.glTexBufferRange(target, internalformat, buffer, offset, size);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
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
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(target);
  HIDE_ARB_DSA_TARGET();
  SERIALISE_ELEMENT(internalformat);
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));

  if(IsReplayingAndReading())
  {
    if(IsLoading(m_State) && m_CurEventID == 0)
    {
      ResourceId liveId = GetResourceManager()->GetID(texture);
      uint32_t Size = 1;
      m_Real.glGetNamedBufferParameterivEXT(buffer.name, eGL_BUFFER_SIZE, (GLint *)&Size);
      m_Textures[liveId].width = Size / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat),
                                                             GetDataType(internalformat)));
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      if(target != eGL_NONE)
        m_Textures[liveId].curType = TextureTarget(target);
      m_Textures[liveId].internalFormat = internalformat;
    }

    if(target != eGL_NONE)
      m_Real.glTextureBufferEXT(texture.name, target, internalformat, buffer.name);
    else
      m_Real.glTextureBuffer(texture.name, internalformat, buffer.name);
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

    ResourceId bufid = GetResourceManager()->GetID(BufferRes(GetCtx(), buffer));

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
          bufRecord->viewTextures.insert(record->GetResourceID());
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
      m_ContextRecord->AddChunk(chunk);
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

      if(bufid != ResourceId())
      {
        m_MissingTracks.insert(bufid);
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
        bufRecord->viewTextures.insert(record->GetResourceID());
      }
    }
  }

  {
    if(buffer != 0)
    {
      uint32_t size = 1;
      m_Real.glGetNamedBufferParameterivEXT(buffer, eGL_BUFFER_SIZE, (GLint *)&size);
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
  }
}

void WrappedOpenGL::glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat,
                                       GLuint buffer)
{
  m_Real.glTextureBufferEXT(texture, target, internalformat, buffer);

  Common_glTextureBufferEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                            internalformat, buffer);
}

void WrappedOpenGL::glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer)
{
  m_Real.glTextureBuffer(texture, internalformat, buffer);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
    RDCERR("Internal textures should be allocated via dsa interfaces");

  Common_glTextureBufferEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), eGL_NONE,
                            internalformat, buffer);
}

void WrappedOpenGL::glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
  m_Real.glTexBuffer(target, internalformat, buffer);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord();
    if(record != NULL)
      Common_glTextureBufferEXT(record->GetResourceID(), target, internalformat, buffer);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

void WrappedOpenGL::glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat,
                                        GLuint buffer)
{
  m_Real.glMultiTexBufferEXT(texunit, target, internalformat, buffer);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(IsReplayMode(m_State))
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().m_TextureRecord[texunit - eGL_TEXTURE0];
    if(record != NULL)
      Common_glTextureBufferEXT(record->GetResourceID(), target, internalformat, buffer);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
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
