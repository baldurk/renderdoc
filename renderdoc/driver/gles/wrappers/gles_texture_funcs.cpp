/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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
#include "common/common.h"
#include "serialise/string_utils.h"

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

bool WrappedGLES::Serialise_glGenTextures(GLsizei n, GLuint *textures)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), *textures)));

  if(m_State == READING)
  {
    GLuint real = 0;
    m_Real.glGenTextures(1, &real);

    GLResource res = TextureRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(id, res);

    m_Textures[live].resource = res;
    m_Textures[live].curType = eGL_NONE;
  }

  return true;
}

void WrappedGLES::glGenTextures(GLsizei n, GLuint *textures)
{
  m_Real.glGenTextures(n, textures);

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = TextureRes(GetCtx(), textures[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        SCOPED_SERIALISE_CONTEXT(GEN_TEXTURE);
        Serialise_glGenTextures(1, textures + i);

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

void WrappedGLES::glDeleteTextures(GLsizei n, const GLuint *textures)
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

bool WrappedGLES::Serialise_glBindTexture(GLenum target, GLuint texture)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(
      ResourceId, Id,
      (texture ? GetResourceManager()->GetID(TextureRes(GetCtx(), texture)) : ResourceId()));

  if(m_State == WRITING_IDLE)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    RDCASSERT(record);
    record->datatype = TextureBinding(Target);
  }
  else if(m_State < WRITING)
  {
    if(Id == ResourceId())
    {
      m_Real.glBindTexture(Target, 0);
    }
    else
    {
      GLResource res = GetResourceManager()->GetLiveResource(Id);
      m_Real.glBindTexture(Target, res.name);

      if(m_State == READING)
      {
        m_Textures[GetResourceManager()->GetLiveID(Id)].curType = TextureTarget(Target);
        m_Textures[GetResourceManager()->GetLiveID(Id)].creationFlags |= eTextureCreate_SRV;
      }
    }
  }

  return true;
}

void WrappedGLES::glBindTexture(GLenum target, GLuint texture)
{
  m_Real.glBindTexture(target, texture);

  if(texture != 0 && GetResourceManager()->GetID(TextureRes(GetCtx(), texture)) == ResourceId())
    return;

  if(m_State == WRITING_CAPFRAME)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(BIND_TEXTURE);
      Serialise_glBindTexture(target, texture);

      chunk = scope.Get();
    }

    m_ContextRecord->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }
  else if(m_State < WRITING)
  {
    m_Textures[GetResourceManager()->GetID(TextureRes(GetCtx(), texture))].curType =
        TextureTarget(target);
  }

  ContextData &cd = GetCtxData();

  if(texture == 0)
  {
    cd.m_TextureRecord[cd.m_TextureUnit][TextureTargetIndex(target)] = NULL;
    return;
  }

  if(m_State >= WRITING)
  {
    GLResourceRecord *r = cd.m_TextureRecord[cd.m_TextureUnit][TextureTargetIndex(target)] =
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
        SCOPED_SERIALISE_CONTEXT(BIND_TEXTURE);
        Serialise_glBindTexture(target, texture);

        chunk = scope.Get();
      }

      r->AddChunk(chunk);
    }
  }
}

bool WrappedGLES::Serialise_glBindImageTexture(GLuint unit, GLuint texture, GLint level,
                                                 GLboolean layered, GLint layer, GLenum access,
                                                 GLenum format)
{
  SERIALISE_ELEMENT(uint32_t, Unit, unit);
  SERIALISE_ELEMENT(ResourceId, texid, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(bool, Layered, layered == GL_TRUE);
  SERIALISE_ELEMENT(int32_t, Layer, layer);
  SERIALISE_ELEMENT(GLenum, Access, access);
  SERIALISE_ELEMENT(GLenum, Format, format);

  if(m_State <= EXECUTING)
  {
    GLuint tex = texid == ResourceId() ? 0 : GetResourceManager()->GetLiveResource(texid).name;

    m_Real.glBindImageTexture(Unit, tex, Level, Layered, Layer, Access, Format);

    if(m_State == READING)
      m_Textures[GetResourceManager()->GetLiveID(texid)].creationFlags |= eTextureCreate_UAV;
  }

  return true;
}

void WrappedGLES::glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered,
                                       GLint layer, GLenum access, GLenum format)
{
  m_Real.glBindImageTexture(unit, texture, level, layered, layer, access, format);

  if(m_State == WRITING_CAPFRAME)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(BIND_IMAGE_TEXTURE);
      Serialise_glBindImageTexture(unit, texture, level, layered, layer, access, format);

      chunk = scope.Get();
    }

    m_ContextRecord->AddChunk(chunk);
    GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), texture), eFrameRef_Read);
  }
}

bool WrappedGLES::Serialise_glTextureViewEXT(GLuint texture, GLenum target, GLuint origtexture,
                                             GLenum internalformat, GLuint minlevel,
                                             GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(GLenum, InternalFormat, internalformat);
  SERIALISE_ELEMENT(uint32_t, MinLevel, minlevel);
  SERIALISE_ELEMENT(uint32_t, NumLevels, numlevels);
  SERIALISE_ELEMENT(uint32_t, MinLayer, minlayer);
  SERIALISE_ELEMENT(uint32_t, NumLayers, numlayers);
  SERIALISE_ELEMENT(ResourceId, texid, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));
  SERIALISE_ELEMENT(ResourceId, origid,
                    GetResourceManager()->GetID(TextureRes(GetCtx(), origtexture)));

  if(m_State == READING)
  {
    GLResource tex = GetResourceManager()->GetLiveResource(texid);
    GLResource origtex = GetResourceManager()->GetLiveResource(origid);
    m_Real.glTextureViewEXT(tex.name, Target, origtex.name, InternalFormat, MinLevel, NumLevels,
                            MinLayer, NumLayers);

    ResourceId liveTexId = GetResourceManager()->GetLiveID(texid);
    ResourceId liveOrigId = GetResourceManager()->GetLiveID(origid);

    m_Textures[liveTexId].curType = TextureTarget(Target);
    m_Textures[liveTexId].internalFormat = InternalFormat;
    m_Textures[liveTexId].view = true;
    m_Textures[liveTexId].width = m_Textures[liveOrigId].width;
    m_Textures[liveTexId].height = m_Textures[liveOrigId].height;
    m_Textures[liveTexId].depth = m_Textures[liveOrigId].depth;
  }

  return true;
}

void WrappedGLES::glTextureViewEXT(GLuint texture, GLenum target, GLuint origtexture,
                                   GLenum internalformat, GLuint minlevel, GLuint numlevels,
                                   GLuint minlayer, GLuint numlayers)
{
  internalformat = GetSizedFormat(m_Real, target, internalformat);

  m_Real.glTextureViewEXT(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer,
                          numlayers);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));
    GLResourceRecord *origrecord =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), origtexture));
    RDCASSERT(record && origrecord);

    SCOPED_SERIALISE_CONTEXT(TEXTURE_VIEW);
    Serialise_glTextureViewEXT(texture, target, origtexture, internalformat, minlevel, numlevels,
                               minlayer, numlayers);

    record->AddChunk(scope.Get());
    record->AddParent(origrecord);

    // illegal to re-type textures
    record->VerifyDataType(target);

    // mark the underlying resource as dirty to avoid tracking dirty across
    // aliased resources etc.
    if(m_State == WRITING_IDLE)
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

bool WrappedGLES::Serialise_glGenerateMipmap(GLenum target)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  if(m_State <= EXECUTING)
  {
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    glGenerateMipmap(Target);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glGenerateMipmap(" + ToStr::Get(id) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_GenMips;

    AddDrawcall(draw, true);

    m_ResourceUses[GetResourceManager()->GetLiveID(id)].push_back(
        EventUsage(m_CurEventID, eUsage_GenMips));
  }

  return true;
}

void WrappedGLES::glGenerateMipmap(GLenum target)
{
  m_Real.glGenerateMipmap(target);

  CoherentMapImplicitBarrier();
  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    RDCASSERT(record);

    if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(GENERATE_MIPMAP);
      Serialise_glGenerateMipmap(target);

      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
    }
    else if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

  }
}

bool WrappedGLES::Serialise_glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel,
                                                 GLint srcX, GLint srcY, GLint srcZ, GLuint dstName,
                                                 GLenum dstTarget, GLint dstLevel, GLint dstX,
                                                 GLint dstY, GLint dstZ, GLsizei srcWidth,
                                                 GLsizei srcHeight, GLsizei srcDepth)
{
  SERIALISE_ELEMENT(ResourceId, srcid, GetResourceManager()->GetID(TextureRes(GetCtx(), srcName)));
  SERIALISE_ELEMENT(ResourceId, dstid, GetResourceManager()->GetID(TextureRes(GetCtx(), dstName)));
  SERIALISE_ELEMENT(GLenum, SourceTarget, srcTarget);
  SERIALISE_ELEMENT(GLenum, DestTarget, dstTarget);
  SERIALISE_ELEMENT(uint32_t, SourceLevel, srcLevel);
  SERIALISE_ELEMENT(uint32_t, SourceX, srcX);
  SERIALISE_ELEMENT(uint32_t, SourceY, srcY);
  SERIALISE_ELEMENT(uint32_t, SourceZ, srcZ);
  SERIALISE_ELEMENT(uint32_t, SourceWidth, srcWidth);
  SERIALISE_ELEMENT(uint32_t, SourceHeight, srcHeight);
  SERIALISE_ELEMENT(uint32_t, SourceDepth, srcDepth);
  SERIALISE_ELEMENT(uint32_t, DestLevel, dstLevel);
  SERIALISE_ELEMENT(uint32_t, DestX, dstX);
  SERIALISE_ELEMENT(uint32_t, DestY, dstY);
  SERIALISE_ELEMENT(uint32_t, DestZ, dstZ);

  if(m_State < WRITING)
  {
    GLResource srcres = GetResourceManager()->GetLiveResource(srcid);
    GLResource dstres = GetResourceManager()->GetLiveResource(dstid);
    m_Real.glCopyImageSubData(srcres.name, SourceTarget, SourceLevel, SourceX, SourceY, SourceZ,
                              dstres.name, DestTarget, DestLevel, DestX, DestY, DestZ, SourceWidth,
                              SourceHeight, SourceDepth);
  }

  const string desc = m_pSerialiser->GetDebugStr();

  Serialise_DebugMessages();

  if(m_State == READING)
  {
    AddEvent(desc);
    string name = "glCopyImageSubData(" + ToStr::Get(srcid) + ", " + ToStr::Get(dstid) + ")";

    FetchDrawcall draw;
    draw.name = name;
    draw.flags |= eDraw_Copy;

    draw.copySource = srcid;
    draw.copyDestination = dstid;

    AddDrawcall(draw, true);

    if(srcid == dstid)
    {
      m_ResourceUses[GetResourceManager()->GetLiveID(srcid)].push_back(
          EventUsage(m_CurEventID, eUsage_Copy));
    }
    else
    {
      m_ResourceUses[GetResourceManager()->GetLiveID(srcid)].push_back(
          EventUsage(m_CurEventID, eUsage_CopySrc));
      m_ResourceUses[GetResourceManager()->GetLiveID(dstid)].push_back(
          EventUsage(m_CurEventID, eUsage_CopyDst));
    }
  }

  return true;
}

void WrappedGLES::glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX,
                                       GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget,
                                       GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                                       GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
  CoherentMapImplicitBarrier();

  m_Real.glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget,
                            dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);

  if(m_State == WRITING_CAPFRAME)
  {
    GLResourceRecord *srcrecord =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), srcName));
    GLResourceRecord *dstrecord =
        GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), dstName));
    RDCASSERT(srcrecord && dstrecord);

    SCOPED_SERIALISE_CONTEXT(COPY_SUBIMAGE);
    Serialise_glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget,
                                 dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);

    m_ContextRecord->AddChunk(scope.Get());
    m_MissingTracks.insert(dstrecord->GetResourceID());
    GetResourceManager()->MarkResourceFrameReferenced(dstrecord->GetResourceID(), eFrameRef_Read);
    GetResourceManager()->MarkResourceFrameReferenced(srcrecord->GetResourceID(), eFrameRef_Read);
  }
  else if(m_State == WRITING_IDLE)
  {
    GetResourceManager()->MarkDirtyResource(TextureRes(GetCtx(), dstName));
  }
}


bool WrappedGLES::Serialise_glCopyTexSubImage2D(GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint x,
                                                GLint y, GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, Xoffset, xoffset);
  SERIALISE_ELEMENT(int32_t, Yoffset, yoffset);
  SERIALISE_ELEMENT(int32_t, X, x);
  SERIALISE_ELEMENT(int32_t, Y, y);
  SERIALISE_ELEMENT(int32_t, Width, width);
  SERIALISE_ELEMENT(int32_t, Height, height);

  if(m_State < WRITING)
  {
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    glCopyTexSubImage2D(Target, Level, Xoffset, Yoffset, X, Y, Width, Height);
  }

  return true;
}

void WrappedGLES::glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                      GLint x, GLint y, GLsizei width, GLsizei height)
{
  m_Real.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

  if(m_State >= WRITING)
  {
    CoherentMapImplicitBarrier();
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    RDCASSERT(record);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(COPY_SUBIMAGE2D);
      Serialise_glCopyTexSubImage2D(target, level, xoffset, yoffset, x,
                                    y, width, height);

      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
    }
  }
}

bool WrappedGLES::Serialise_glCopyTexSubImage3D(GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset,
                                                GLint zoffset, GLint x, GLint y,
                                                GLsizei width, GLsizei height)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, Xoffset, xoffset);
  SERIALISE_ELEMENT(int32_t, Yoffset, yoffset);
  SERIALISE_ELEMENT(int32_t, Zoffset, zoffset);
  SERIALISE_ELEMENT(int32_t, X, x);
  SERIALISE_ELEMENT(int32_t, Y, y);
  SERIALISE_ELEMENT(int32_t, Width, width);
  SERIALISE_ELEMENT(int32_t, Height, height);

  if(m_State < WRITING)
  {
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    glCopyTexSubImage3D(Target, Level, Xoffset, Yoffset, Zoffset, X, Y, Width, Height);
  }

  return true;
}


void WrappedGLES::glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                        GLint zoffset, GLint x, GLint y, GLsizei width,
                                        GLsizei height)
{
  m_Real.glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);

  if(m_State >= WRITING)
  {
    CoherentMapImplicitBarrier();
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    RDCASSERT(record);

    if(m_State == WRITING_IDLE)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
      m_MissingTracks.insert(record->GetResourceID());
    }
    else if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(COPY_SUBIMAGE3D);
      Serialise_glCopyTexSubImage3D(target, level, xoffset, yoffset,
                                           zoffset, x, y, width, height);

      m_ContextRecord->AddChunk(scope.Get());
      m_MissingTracks.insert(record->GetResourceID());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
    }
  }
}

bool WrappedGLES::Serialise_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(GLenum, PName, pname);

  int32_t ParamValue = 0;

  RDCCOMPILE_ASSERT(sizeof(int32_t) == sizeof(GLenum),
                    "int32_t isn't the same size as GLenum - aliased serialising will break");
  // special case a few parameters to serialise their value as an enum, not an int
  if(PName == GL_DEPTH_STENCIL_TEXTURE_MODE || PName == GL_TEXTURE_COMPARE_FUNC ||
     PName == GL_TEXTURE_COMPARE_MODE || PName == GL_TEXTURE_MIN_FILTER ||
     PName == GL_TEXTURE_MAG_FILTER || PName == GL_TEXTURE_SWIZZLE_R ||
     PName == GL_TEXTURE_SWIZZLE_G || PName == GL_TEXTURE_SWIZZLE_B || PName == GL_TEXTURE_SWIZZLE_A ||
     PName == GL_TEXTURE_WRAP_S || PName == GL_TEXTURE_WRAP_T || PName == GL_TEXTURE_WRAP_R)
  {
    SERIALISE_ELEMENT(GLenum, Param, (GLenum)param);

    ParamValue = (int32_t)Param;
  }
  else
  {
    SERIALISE_ELEMENT(int32_t, Param, param);

    ParamValue = Param;
  }

  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  if(m_State < WRITING)
  {
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexParameteri(Target, PName, ParamValue);
  }

  return true;
}

void WrappedGLES::glTexParameteri(GLenum target, GLenum pname, GLint param)
{
  m_Real.glTexParameteri(target, pname, param);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(TEXPARAMETERI);
    Serialise_glTexParameteri(target, pname, param);

    if(m_State == WRITING_CAPFRAME)
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
}

bool WrappedGLES::Serialise_glTexParameterf(GLenum target, GLenum pname,
                                              GLfloat param)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(float, Param, param);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  if(m_State < WRITING)
  {
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexParameterf(Target, PName, Param);
  }

  return true;
}

void WrappedGLES::glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
  m_Real.glTexParameterf(target, pname, param);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }

    if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
       m_State != WRITING_CAPFRAME)
      return;

    SCOPED_SERIALISE_CONTEXT(TEXPARAMETERF);
    Serialise_glTexParameterf(target, pname, param);

    if(m_State == WRITING_CAPFRAME)
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
}


template<typename TP, typename TF>
bool WrappedGLES::Serialise_Common_glTexParameter_v(GLenum target, GLenum pname, const TP *params, TF GLHookSet::*function)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  const size_t nParams = (PName == eGL_TEXTURE_BORDER_COLOR ? 4U : 1U);
  SERIALISE_ELEMENT_ARR(TP, Params, params, nParams);

  if(m_State < WRITING)
  {
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    (m_Real.*function)(Target, PName, Params);
  }

  delete[] Params;

  return true;
}

template <typename TP, typename TF>
void WrappedGLES::Common_glTexParameter_v(GLenum target, GLenum pname, const TP *params, TF GLHookSet::*function, const GLChunkType chunkType)
{
  (m_Real.*function)(target, pname, params);

  if(m_State >= WRITING)
  {

    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    RDCASSERT(record);

    if(m_State != WRITING_CAPFRAME && m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end())
      return;

    SCOPED_SERIALISE_CONTEXT(chunkType);
    Serialise_Common_glTexParameter_v(target, pname, params, function);

    if(m_State == WRITING_CAPFRAME)
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
}

bool WrappedGLES::Serialise_glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
  return Serialise_Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameteriv);
}

void WrappedGLES::glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
  Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameteriv, TEXPARAMETERIV);
}

bool WrappedGLES::Serialise_glTexParameterIiv(GLenum target, GLenum pname, const GLint *params)
{
  return Serialise_Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameterIiv);
}

void WrappedGLES::glTexParameterIiv(GLenum target, GLenum pname, const GLint *params)
{
  Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameterIiv, TEXPARAMETERIIV);
}

bool WrappedGLES::Serialise_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params)
{
  return Serialise_Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameterIuiv);
}

void WrappedGLES::glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params)
{
  Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameterIuiv, TEXPARAMETERIUIV);
}

bool WrappedGLES::Serialise_glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
  return Serialise_Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameterfv);
}

void WrappedGLES::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
  Common_glTexParameter_v(target, pname, params, &GLHookSet::glTexParameterfv, TEXPARAMETERFV);
}

bool WrappedGLES::Serialise_glPixelStorei(GLenum pname, GLint param)
{
  SERIALISE_ELEMENT(GLenum, PName, pname);
  SERIALISE_ELEMENT(int32_t, Param, param);

  if(m_State < WRITING)
  {
    m_Real.glPixelStorei(PName, Param);
  }

  return true;
}

void WrappedGLES::glPixelStorei(GLenum pname, GLint param)
{
  m_Real.glPixelStorei(pname, param);

  // except for capturing frames we ignore this and embed the relevant
  // parameters in the chunks that reference them.
  if(m_State == WRITING_CAPFRAME)
  {
    SCOPED_SERIALISE_CONTEXT(PIXELSTORE);
    Serialise_glPixelStorei(pname, param);

    m_ContextRecord->AddChunk(scope.Get());
  }
}

bool WrappedGLES::Serialise_glActiveTexture(GLenum texture)
{
  SERIALISE_ELEMENT(GLenum, Texture, texture);

  if(m_State < WRITING)
    m_Real.glActiveTexture(Texture);

  return true;
}

void WrappedGLES::glActiveTexture(GLenum texture)
{
  m_Real.glActiveTexture(texture);

  GetCtxData().m_TextureUnit = texture - eGL_TEXTURE0;

  if(m_State == WRITING_CAPFRAME)
  {
    Chunk *chunk = NULL;

    {
      SCOPED_SERIALISE_CONTEXT(ACTIVE_TEXTURE);
      Serialise_glActiveTexture(texture);

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


bool WrappedGLES::Serialise_glTexImage2D(GLenum target, GLint level,
                                         GLint internalformat, GLsizei width,
                                         GLsizei height, GLint border, GLenum format,
                                         GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(GLenum, IntFormat, (GLenum)internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(int32_t, Border, border);
  SERIALISE_ELEMENT(GLenum, Format, format);
  SERIALISE_ELEMENT(GLenum, Type, type);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPath(Width, Height, 0, Format, Type))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels = unpack.Unpack((byte *)pixels, Width, Height, 0, Format, Type);
  }

  size_t subimageSize = GetByteSize(Width, Height, 1, Format, Type);

  SERIALISE_ELEMENT(bool, DataProvided, pixels != NULL);
  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, subimageSize, DataProvided);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State == READING)
  {
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, IntFormat, Format);

    if(Level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetLiveID(id);
      m_Textures[liveId].width = Width;
      m_Textures[liveId].height = Height;
      m_Textures[liveId].depth = 1;
      m_Textures[liveId].curType = TextureTarget(Target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = IntFormat;
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
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);

    if(TextureBinding(Target) != eGL_TEXTURE_BINDING_CUBE_MAP)
    {
      m_Real.glTexImage2D(Target, Level, IntFormat, Width, Height, Border, Format, Type, buf);
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
        m_Real.glTexImage2D(ts[i], Level, IntFormat, Width, Height, Border, Format, Type, buf);
      }
    }

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLsizei height, GLint border, GLenum format, GLenum type,
                                 const GLvoid *pixels)
{
  m_Real.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    CoherentMapImplicitBarrier();

    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

    bool fromunpackbuf = false;
    {
      GLint unpackbuf = 0;
      m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
      fromunpackbuf = (unpackbuf != 0);
    }

    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if (record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(m_State == WRITING_IDLE && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      SCOPED_SERIALISE_CONTEXT(TEXIMAGE2D);
      Serialise_glTexImage2D(target, level, internalformat, width, height, border, format, type, fromunpackbuf ? NULL : pixels);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(m_State == WRITING_CAPFRAME)
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

    if(level == 0)
    {
      m_Textures[texId].width = width;
      m_Textures[texId].height = height;
      m_Textures[texId].depth = 1;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 2;
      m_Textures[texId].internalFormat = (GLenum)internalformat;
    }
  }
}

bool WrappedGLES::Serialise_glTexImage3D(GLenum target, GLint level,
                                         GLint internalformat, GLsizei width,
                                         GLsizei height, GLsizei depth, GLint border,
                                         GLenum format, GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(GLenum, IntFormat, (GLenum)internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(uint32_t, Depth, depth);
  SERIALISE_ELEMENT(int32_t, Border, border);
  SERIALISE_ELEMENT(GLenum, Format, format);
  SERIALISE_ELEMENT(GLenum, Type, type);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPath(Width, Height, Depth, Format, Type))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels = unpack.Unpack((byte *)pixels, Width, Height, Depth, Format, Type);
  }

  size_t subimageSize = GetByteSize(Width, Height, Depth, Format, Type);

  SERIALISE_ELEMENT(bool, DataProvided, pixels != NULL);
  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, subimageSize, DataProvided);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State == READING)
  {
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, IntFormat, Format);

    if(Level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetLiveID(id);
      m_Textures[liveId].width = Width;
      m_Textures[liveId].height = Height;
      m_Textures[liveId].depth = Depth;
      m_Textures[liveId].curType = TextureTarget(Target);
      m_Textures[liveId].dimension = 3;
      m_Textures[liveId].internalFormat = IntFormat;
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

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexImage3D(Target, Level, IntFormat, Width, Height, Depth, Border, Format, Type, buf);

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                 GLsizei height, GLsizei depth, GLint border, GLenum format,
                                 GLenum type, const GLvoid *pixels)
{
  m_Real.glTexImage3D(target, level, internalformat, width, height, depth, border, format, type,
                      pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if (record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    CoherentMapImplicitBarrier();

    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

    bool fromunpackbuf = false;
    {
      GLint unpackbuf = 0;
      m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
      fromunpackbuf = (unpackbuf != 0);
    }

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(m_State == WRITING_IDLE && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].depth == depth &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      SCOPED_SERIALISE_CONTEXT(TEXIMAGE3D);
      Serialise_glTexImage3D(target, level, internalformat, width,
                             height, depth, border, format, type,
                             fromunpackbuf ? NULL : pixels);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(m_State == WRITING_CAPFRAME)
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

    if(level == 0)
    {
      m_Textures[texId].width = width;
      m_Textures[texId].height = height;
      m_Textures[texId].depth = depth;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 3;
      m_Textures[texId].internalFormat = (GLenum)internalformat;
    }
  }
}

bool WrappedGLES::Serialise_glCompressedTexImage2D(GLenum target,
                                                   GLint level, GLenum internalformat,
                                                   GLsizei width, GLsizei height,
                                                   GLint border, GLsizei imageSize,
                                                   const GLvoid *pixels)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(GLenum, fmt, internalformat);
  SERIALISE_ELEMENT(int32_t, Border, border);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPathCompressed(Width, Height, 0))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, Width, Height, 0, imageSize);
  }

  SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
  SERIALISE_ELEMENT(bool, DataProvided, pixels != NULL);
  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, byteSize, DataProvided);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State == READING)
  {
    void *databuf = buf;

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(!DataProvided || databuf == NULL)
    {
      if((uint32_t)m_ScratchBuf.size() < byteSize)
        m_ScratchBuf.resize(byteSize);
      databuf = &m_ScratchBuf[0];
    }

    ResourceId liveId = GetResourceManager()->GetLiveID(id);

    if(Level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = Width;
      m_Textures[liveId].height = Height;
      m_Textures[liveId].depth = 1;
      m_Textures[liveId].curType = TextureTarget(Target);
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = fmt;
    }

    if (DataProvided)
    {
      RDCASSERT(GetCompressedByteSize(Width, Height, 1, fmt, Level) == byteSize);
      auto& cd = m_Textures[liveId].compressedData;
      auto& cdData = cd[Target][Level];
      cdData.resize(byteSize);
      memcpy(cdData.data(), databuf, byteSize);
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
    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);

    if(TextureBinding(Target) != eGL_TEXTURE_BINDING_CUBE_MAP)
    {
      m_Real.glCompressedTexImage2D(Target, Level, fmt, Width, Height, Border, byteSize, databuf);
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
        m_Real.glCompressedTexImage2D(ts[i], Level, fmt, Width, Height, Border, byteSize, databuf);
      }
    }

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                           GLsizei width, GLsizei height, GLint border,
                                           GLsizei imageSize, const GLvoid *pixels)
{
  m_Real.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize,
                                pixels);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    CoherentMapImplicitBarrier();

    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, internalformat);

    bool fromunpackbuf = false;
    {
      GLint unpackbuf = 0;
      m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
      fromunpackbuf = (unpackbuf != 0);
    }

    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(m_State == WRITING_IDLE && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      SCOPED_SERIALISE_CONTEXT(TEXIMAGE2D_COMPRESSED);
      Serialise_glCompressedTexImage2D(target, level, internalformat,
                                       width, height, border, imageSize,
                                       fromunpackbuf ? NULL : pixels);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(m_State == WRITING_CAPFRAME)
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

    if(level == 0)
    {
      m_Textures[texId].width = width;
      m_Textures[texId].height = height;
      m_Textures[texId].depth = 1;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 2;
      m_Textures[texId].internalFormat = internalformat;
    }
  }
}


bool WrappedGLES::Serialise_glCompressedTexImage3D(GLenum target,
                                                   GLint level, GLenum internalformat,
                                                   GLsizei width, GLsizei height,
                                                   GLsizei depth, GLint border,
                                                   GLsizei imageSize, const GLvoid *pixels)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(uint32_t, Depth, depth);
  SERIALISE_ELEMENT(GLenum, fmt, internalformat);
  SERIALISE_ELEMENT(int32_t, Border, border);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPathCompressed(Width, Height, Depth))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, Width, Height, Depth, imageSize);
  }

  SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
  SERIALISE_ELEMENT(bool, DataProvided, pixels != NULL);
  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, byteSize, DataProvided);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State == READING)
  {
    void *databuf = buf;

    // if we didn't have data provided (this is invalid, but could happen if the data
    // should have been sourced from an unpack buffer), then grow our scratch buffer if
    // necessary and use that instead to make sure we don't pass NULL to glCompressedTexImage*
    if(!DataProvided || databuf == NULL)
    {
      if((uint32_t)m_ScratchBuf.size() < byteSize)
        m_ScratchBuf.resize(byteSize);
      databuf = &m_ScratchBuf[0];
    }

    ResourceId liveId = GetResourceManager()->GetLiveID(id);

    if(Level == 0)    // assume level 0 will always get a glTexImage call
    {
      m_Textures[liveId].width = Width;
      m_Textures[liveId].height = Height;
      m_Textures[liveId].depth = Depth;
      m_Textures[liveId].curType = TextureTarget(Target);
      m_Textures[liveId].dimension = 3;
      m_Textures[liveId].internalFormat = fmt;
    }

    if (DataProvided)
    {
      RDCASSERT(GetCompressedByteSize(Width, Height, Depth, fmt, Level) == byteSize);
      auto& cd = m_Textures[liveId].compressedData;
      auto& cdData = cd[Target][Level];
      cdData.resize(byteSize);
      memcpy(cdData.data(), databuf, byteSize);
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

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glCompressedTexImage3D(Target, Level, fmt, Width, Height, Depth, Border, byteSize, databuf);

    if(unpackbuf)
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
    m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                         GLsizei width, GLsizei height, GLsizei depth,
                                         GLint border, GLsizei imageSize, const GLvoid *pixels)
{
  m_Real.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border,
                                imageSize, pixels);

  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    CoherentMapImplicitBarrier();

    // proxy formats are used for querying texture capabilities, don't serialise these
    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, internalformat);

    bool fromunpackbuf = false;
    {
      GLint unpackbuf = 0;
      m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);
      fromunpackbuf = (unpackbuf != 0);
    }


    // This is kind of an arbitary heuristic, but in the past a game has re-specified a texture with
    // glTexImage over and over
    // so we need to attempt to catch the case where glTexImage is called to re-upload data, not
    // actually re-create it.
    // Ideally we'd check for non-zero levels, but that would complicate the condition
    // if we're uploading new data but otherwise everything is identical, ignore this chunk and
    // simply mark the texture dirty
    if(m_State == WRITING_IDLE && record->AlreadyDataType(target) && level == 0 &&
       m_Textures[record->GetResourceID()].width == width &&
       m_Textures[record->GetResourceID()].height == height &&
       m_Textures[record->GetResourceID()].depth == depth &&
       m_Textures[record->GetResourceID()].internalFormat == (GLenum)internalformat)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      SCOPED_SERIALISE_CONTEXT(TEXIMAGE3D_COMPRESSED);
      Serialise_glCompressedTexImage3D(target, level, internalformat,
                                       width, height, depth, border, imageSize,
                                       fromunpackbuf ? NULL : pixels);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      if(m_State == WRITING_CAPFRAME)
        m_MissingTracks.insert(record->GetResourceID());
      else if(fromunpackbuf)
        GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }

    if(level == 0)
    {
      m_Textures[texId].width = width;
      m_Textures[texId].height = height;
      m_Textures[texId].depth = depth;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 3;
      m_Textures[texId].internalFormat = internalformat;
    }
  }
}

#pragma endregion

#pragma region Texture Creation(glCopyTexImage)

bool WrappedGLES::Serialise_glCopyTexImage2D(GLenum target, GLint level,
                                             GLenum internalformat, GLint x, GLint y,
                                             GLsizei width, GLsizei height, GLint border)
{
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(int32_t, X, x);
  SERIALISE_ELEMENT(int32_t, Y, y);
  SERIALISE_ELEMENT(int32_t, Width, width);
  SERIALISE_ELEMENT(int32_t, Height, height);
  SERIALISE_ELEMENT(int32_t, Border, border);

  if(m_State < WRITING)
  {
    if(Level == 0)    // assume level 0 will always get a glTexImage call
    {
      ResourceId liveId = GetResourceManager()->GetLiveID(id);
      m_Textures[liveId].width = Width;
      m_Textures[liveId].height = Height;
      m_Textures[liveId].depth = 1;
      m_Textures[liveId].dimension = 2;
      m_Textures[liveId].internalFormat = Format;
    }

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glCopyTexImage2D(Target, Level, Format, X, Y, Width, Height, Border);
  }
  return true;
}

void WrappedGLES::glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x,
                                   GLint y, GLsizei width, GLsizei height, GLint border)
{
  m_Real.glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
    return;
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    CoherentMapImplicitBarrier();

    // not sure if proxy formats are valid, but ignore these anyway
    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, (GLenum)internalformat);

    if(m_State == WRITING_IDLE)
    {
      SCOPED_SERIALISE_CONTEXT(TEXIMAGE2D);
      Serialise_glTexImage2D(target, level, internalformat, width,
                             height, border, GetBaseFormat(internalformat),
                             GetDataType(internalformat), NULL);

      record->AddChunk(scope.Get());

      // illegal to re-type textures
      record->VerifyDataType(target);

      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else if(m_State == WRITING_CAPFRAME)
    {
      SCOPED_SERIALISE_CONTEXT(COPY_IMAGE2D);
      Serialise_glCopyTexImage2D(target, level, internalformat, x, y,
                                 width, height, border);

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
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 2;
      m_Textures[texId].internalFormat = internalformat;
    }
  }
}

#pragma endregion

#pragma region Texture Creation(glTexStorage *)

bool WrappedGLES::Serialise_glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,
                                                  GLenum internalformat, GLsizei width)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Levels, levels);
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

  if(m_State == READING)
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, Format, dummy);

    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    m_Textures[liveId].width = Width;
    m_Textures[liveId].height = 1;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].curType = TextureTarget(Target);
    m_Textures[liveId].dimension = 1;
    m_Textures[liveId].internalFormat = Format;
    m_Textures[liveId].emulated = emulated;

    m_Real.glTextureStorage1DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Levels,
                                 Format, Width);
  }

  return true;
}

void WrappedGLES::Common_glTextureStorage1DEXT(ResourceId texId, GLenum target, GLsizei levels,
                                                 GLenum internalformat, GLsizei width)
{
  if(texId == ResourceId())
    return;

  if(internalformat == 0)
    return;

  internalformat = GetSizedFormat(m_Real, target, internalformat);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    SCOPED_SERIALISE_CONTEXT(TEXSTORAGE1D);
    Serialise_glTextureStorage1DEXT(record->Resource.name, target, levels, internalformat, width);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    m_Textures[texId].curType = TextureTarget(target);
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedGLES::glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels,
                                        GLenum internalformat, GLsizei width)
{
  m_Real.glTextureStorage1DEXT(texture, target, levels, internalformat, width);

  Common_glTextureStorage1DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                               levels, internalformat, width);
}

void WrappedGLES::glTexStorage1DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
  m_Real.glTexStorage1DEXT(target, levels, internalformat, width);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage1DEXT(record->GetResourceID(), target, levels, internalformat, width);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

bool WrappedGLES::Serialise_glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,
                                                 GLenum internalformat, GLsizei width,
                                                 GLsizei height)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Levels, levels);
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

  if(m_State == READING)
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, Format, dummy);

    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    m_Textures[liveId].width = Width;
    m_Textures[liveId].height = Height;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].curType = TextureTarget(Target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = Format;
    m_Textures[liveId].emulated = emulated;

    Compat_glTextureStorage2DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Levels,
                                   Format, Width, Height);
  }

  return true;
}

void WrappedGLES::Common_glTextureStorage2DEXT(ResourceId texId, GLenum target, GLsizei levels,
                                               GLenum internalformat, GLsizei width, GLsizei height)
{
  if(texId == ResourceId())
    return;

  if(internalformat == 0)
    return;

  internalformat = GetSizedFormat(m_Real, target, internalformat);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    SCOPED_SERIALISE_CONTEXT(TEXSTORAGE2D);
    Serialise_glTextureStorage2DEXT(record->Resource.name, target, levels, internalformat, width,
                                    height);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    m_Textures[texId].curType = TextureTarget(target);
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalformat;
  }
}

void WrappedGLES::glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width, GLsizei height)
{
  Compat_glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);

  Common_glTextureStorage2DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                               levels, internalformat, width, height);
}

void WrappedGLES::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                                 GLsizei width, GLsizei height)
{
  m_Real.glTexStorage2D(target, levels, internalformat, width, height);

  if(m_State < WRITING)
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage2DEXT(record->GetResourceID(), target,
                                   levels, internalformat, width, height);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

bool WrappedGLES::Serialise_glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,
                                                  GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLsizei depth)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Levels, levels);
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(uint32_t, Depth, depth);
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(TextureRes(GetCtx(), texture)));

  if(m_State == READING)
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, Format, dummy);

    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    m_Textures[liveId].width = Width;
    m_Textures[liveId].height = Height;
    m_Textures[liveId].depth = Depth;
    m_Textures[liveId].curType = TextureTarget(Target);
    m_Textures[liveId].dimension = 3;
    m_Textures[liveId].internalFormat = Format;
    m_Textures[liveId].emulated = emulated;

    Compat_glTextureStorage3DEXT(GetResourceManager()->GetLiveResource(id).name, Target, Levels,
                                 Format, Width, Height, Depth);
  }

  return true;
}

void WrappedGLES::Common_glTextureStorage3DEXT(ResourceId texId, GLenum target, GLsizei levels,
                                               GLenum internalformat, GLsizei width,
                                               GLsizei height, GLsizei depth)
{
  if(texId == ResourceId())
    return;

  if(internalformat == 0)
    return;

  internalformat = GetSizedFormat(m_Real, target, internalformat);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(texId);
    RDCASSERT(record);

    SCOPED_SERIALISE_CONTEXT(TEXSTORAGE3D);
    Serialise_glTextureStorage3DEXT(record->Resource.name, target, levels, internalformat, width,
                                    height, depth);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);
  }

  {
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    m_Textures[texId].curType = TextureTarget(target);
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalformat;
  }
}


void WrappedGLES::glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels,
                                          GLenum internalformat, GLsizei width, GLsizei height,
                                          GLsizei depth)
{
  Compat_glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);

  Common_glTextureStorage3DEXT(GetResourceManager()->GetID(TextureRes(GetCtx(), texture)), target,
                               levels, internalformat, width, height, depth);
}

void WrappedGLES::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat,
                                 GLsizei width, GLsizei height, GLsizei depth)
{
  m_Real.glTexStorage3D(target, levels, internalformat, width, height, depth);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    RDCERR("Internal textures should be allocated via dsa interfaces");
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record != NULL)
      Common_glTextureStorage3DEXT(record->GetResourceID(), target, levels, internalformat, width,
                                   height, depth);
    else
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
  }
}

bool WrappedGLES::Serialise_glTexStorage2DMultisample(GLenum target,
                                                      GLsizei samples, GLenum internalformat,
                                                      GLsizei width, GLsizei height,
                                                      GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Samples, samples);
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(bool, Fixedlocs, fixedsamplelocations != 0);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  if(m_State == READING)
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, Format, dummy);

    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    m_Textures[liveId].width = Width;
    m_Textures[liveId].height = Height;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].samples = Samples;
    m_Textures[liveId].curType = TextureTarget(Target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = Format;
    m_Textures[liveId].emulated = emulated;

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexStorage2DMultisample(Target, Samples, Format, Width, Height,
                                     Fixedlocs ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedGLES::glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height,
                                              GLboolean fixedsamplelocations)
{
  m_Real.glTexStorage2DMultisample(target, samples, internalformat, width, height,
                                   fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, internalformat);


    SCOPED_SERIALISE_CONTEXT(TEXSTORAGE2DMS);
    Serialise_glTexStorage2DMultisample(target, samples,
                                        internalformat, width, height, fixedsamplelocations);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);

    {
      m_Textures[texId].width = width;
      m_Textures[texId].height = height;
      m_Textures[texId].depth = 1;
      m_Textures[texId].samples = samples;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 2;
      m_Textures[texId].internalFormat = internalformat;
    }

  }
}

bool WrappedGLES::Serialise_glTexStorage3DMultisample(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat, GLsizei width,
                                                      GLsizei height, GLsizei depth,
                                                      GLboolean fixedsamplelocations)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint32_t, Samples, samples);
  SERIALISE_ELEMENT(GLenum, Format, internalformat);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(uint32_t, Depth, depth);
  SERIALISE_ELEMENT(bool, Fixedlocs, fixedsamplelocations != 0);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  if(m_State == READING)
  {
    GLenum dummy = eGL_NONE;
    bool emulated = EmulateLuminanceFormat(m_Real, GetResourceManager()->GetLiveResource(id).name,
                                           Target, Format, dummy);

    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    m_Textures[liveId].width = Width;
    m_Textures[liveId].height = Height;
    m_Textures[liveId].depth = Depth;
    m_Textures[liveId].samples = Samples;
    m_Textures[liveId].curType = TextureTarget(Target);
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = Format;
    m_Textures[liveId].emulated = emulated;

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexStorage3DMultisample(Target, Samples, Format, Width, Height, Depth,
                                         Fixedlocs ? GL_TRUE : GL_FALSE);
  }

  return true;
}

void WrappedGLES::glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height, GLsizei depth,
                                              GLboolean fixedsamplelocations)
{
  m_Real.glTexStorage3DMultisample(target, samples, internalformat, width, height, depth,
                                   fixedsamplelocations);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    if(internalformat == 0)
      return;

    internalformat = GetSizedFormat(m_Real, target, internalformat);

    SCOPED_SERIALISE_CONTEXT(TEXSTORAGE3DMS);
    Serialise_glTexStorage3DMultisample(target, samples, internalformat,
                                               width, height, depth, fixedsamplelocations);

    record->AddChunk(scope.Get());

    // illegal to re-type textures
    record->VerifyDataType(target);

    {
      m_Textures[texId].width = width;
      m_Textures[texId].height = height;
      m_Textures[texId].depth = depth;
      m_Textures[texId].samples = samples;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 3;
      m_Textures[texId].internalFormat = internalformat;
    }

  }
}

#pragma endregion

#pragma region Texture upload(glTexSubImage *)

bool WrappedGLES::Serialise_glTexSubImage2D(GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset, GLsizei width,
                                            GLsizei height, GLenum format, GLenum type,
                                            const void *pixels)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, xoff, xoffset);
  SERIALISE_ELEMENT(int32_t, yoff, yoffset);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(GLenum, Format, format);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT(bool, UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPath(Width, Height, 0, Format, Type))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels = unpack.Unpack((byte *)pixels, Width, Height, 0, Format, Type);
  }

  size_t subimageSize = GetByteSize(Width, Height, 1, Format, Type);

  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, subimageSize, !UnpackBufBound);
  SERIALISE_ELEMENT(uint64_t, bufoffs, (uint64_t)pixels);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State <= EXECUTING)
  {
    GLint align = 1;
    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    if(Format == eGL_LUMINANCE)
    {
      Format = eGL_RED;
    }
    else if(Format == eGL_LUMINANCE_ALPHA)
    {
      Format = eGL_RG;
    }
    else if(Format == eGL_ALPHA)
    {
      // check if format was converted from alpha-only format to R8, and substitute
      ResourceId liveId = GetResourceManager()->GetLiveID(id);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        Format = eGL_RED;
    }

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexSubImage2D(Target, Level,
                           xoff, yoff, Width, Height, Format, Type,
                           buf ? buf : (const void *)bufoffs);

    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    }

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLsizei width, GLsizei height, GLenum format, GLenum type,
                                    const void *pixels)
{
  m_Real.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }

    CoherentMapImplicitBarrier();

    GLint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

    if(m_State == WRITING_IDLE && unpackbuf != 0)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
         m_State == WRITING_IDLE)
        return;

      SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D);
      Serialise_glTexSubImage2D(target, level, xoffset, yoffset, width,
                                height, format, type, pixels);

      if(m_State == WRITING_CAPFRAME)
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
}

bool WrappedGLES::Serialise_glTexSubImage3D(GLenum target, GLint level,
                                            GLint xoffset, GLint yoffset, GLint zoffset,
                                            GLsizei width, GLsizei height, GLsizei depth,
                                            GLenum format, GLenum type, const void *pixels)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, xoff, xoffset);
  SERIALISE_ELEMENT(int32_t, yoff, yoffset);
  SERIALISE_ELEMENT(int32_t, zoff, zoffset);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(uint32_t, Depth, depth);
  SERIALISE_ELEMENT(GLenum, Format, format);
  SERIALISE_ELEMENT(GLenum, Type, type);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT(bool, UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPath(Width, Height, Depth, Format, Type))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels = unpack.Unpack((byte *)pixels, Width, Height, Depth, Format, Type);
  }

  size_t subimageSize = GetByteSize(Width, Height, Depth, Format, Type);

  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, subimageSize, !UnpackBufBound);
  SERIALISE_ELEMENT(uint64_t, bufoffs, (uint64_t)pixels);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State <= EXECUTING)
  {
    GLint align = 1;
    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    if(Format == eGL_LUMINANCE)
    {
      Format = eGL_RED;
    }
    else if(Format == eGL_LUMINANCE_ALPHA)
    {
      Format = eGL_RG;
    }
    else if(Format == eGL_ALPHA)
    {
      // check if format was converted from alpha-only format to R8, and substitute
      ResourceId liveId = GetResourceManager()->GetLiveID(id);
      if(m_Textures[liveId].internalFormat == eGL_R8)
        Format = eGL_RED;
    }

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glTexSubImage3D(Target, Level,
                           xoff, yoff, zoff, Width, Height, Depth, Format, Type,
                           buf ? buf : (const void *)bufoffs);

    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    }

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                    GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                    GLenum format, GLenum type, const void *pixels)
{
  m_Real.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                         type, pixels);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }

    CoherentMapImplicitBarrier();

    GLint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

    if(m_State == WRITING_IDLE && unpackbuf != 0)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
         m_State == WRITING_IDLE)
        return;

      SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D);
      Serialise_glTexSubImage3D(target, level, xoffset, yoffset,
                                zoffset, width, height, depth, format, type, pixels);

      if(m_State == WRITING_CAPFRAME)
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
}

bool WrappedGLES::Serialise_glCompressedTexSubImage2D(GLenum target,
                                                      GLint level, GLint xoffset,
                                                      GLint yoffset, GLsizei width,
                                                      GLsizei height, GLenum format,
                                                      GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, xoff, xoffset);
  SERIALISE_ELEMENT(int32_t, yoff, yoffset);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(GLenum, fmt, format);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT(bool, UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPathCompressed(Width, Height, 0))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, Width, Height, 0, imageSize);
  }

  SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, byteSize, !UnpackBufBound);
  SERIALISE_ELEMENT(uint64_t, bufoffs, (uint64_t)pixels);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State <= EXECUTING)
  {
    GLint align = 1;
    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glCompressedTexSubImage2D(Target, Level, xoff, yoff, Width, Height, fmt,
                                     byteSize, buf ? buf : (const void *)bufoffs);

    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    }

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}


void WrappedGLES::glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                              GLint yoffset, GLsizei width, GLsizei height,
                                              GLenum format, GLsizei imageSize, const void *pixels)
{
  m_Real.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                   imageSize, pixels);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }

    CoherentMapImplicitBarrier();

    GLint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

    if(m_State == WRITING_IDLE && unpackbuf != 0)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
         m_State == WRITING_IDLE)
        return;

      SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE2D_COMPRESSED);
      Serialise_glCompressedTexSubImage2D(target, level, xoffset,
                                          yoffset, width, height, format, imageSize, pixels);

      if(m_State == WRITING_CAPFRAME)
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
}

bool WrappedGLES::Serialise_glCompressedTexSubImage3D(GLenum target,
                                                      GLint level, GLint xoffset,
                                                      GLint yoffset, GLint zoffset,
                                                      GLsizei width, GLsizei height,
                                                      GLsizei depth, GLenum format,
                                                      GLsizei imageSize, const void *pixels)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(int32_t, Level, level);
  SERIALISE_ELEMENT(int32_t, xoff, xoffset);
  SERIALISE_ELEMENT(int32_t, yoff, yoffset);
  SERIALISE_ELEMENT(int32_t, zoff, zoffset);
  SERIALISE_ELEMENT(uint32_t, Width, width);
  SERIALISE_ELEMENT(uint32_t, Height, height);
  SERIALISE_ELEMENT(uint32_t, Depth, depth);
  SERIALISE_ELEMENT(GLenum, fmt, format);
  SERIALISE_ELEMENT(ResourceId, id, GetCtxData().GetActiveTexRecord(target)->GetResourceID());

  GLint unpackbuf = 0;
  m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

  SERIALISE_ELEMENT(bool, UnpackBufBound, unpackbuf != 0);

  byte *unpackedPixels = NULL;
  byte *srcPixels = NULL;

  if(m_State >= WRITING && pixels && !UnpackBufBound)
  {
    PixelUnpackState unpack;
    unpack.Fetch(&m_Real);

    if(unpack.FastPathCompressed(Width, Height, Depth))
      srcPixels = (byte *)pixels;
    else
      srcPixels = unpackedPixels =
          unpack.UnpackCompressed((byte *)pixels, Width, Height, Depth, imageSize);
  }

  SERIALISE_ELEMENT(uint32_t, byteSize, imageSize);
  SERIALISE_ELEMENT_BUF_OPT(byte *, buf, srcPixels, byteSize, !UnpackBufBound);
  SERIALISE_ELEMENT(uint64_t, bufoffs, (uint64_t)pixels);

  SAFE_DELETE_ARRAY(unpackedPixels);

  if(m_State <= EXECUTING)
  {
    GLint align = 1;
    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
      m_Real.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &align);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);
    }

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(id).name, Target);
    m_Real.glCompressedTexSubImage3D(Target, Level, xoff, yoff, zoff, Width, Height, Depth,
                                     fmt, byteSize, buf ? buf : (const void *)bufoffs);

    if(!UnpackBufBound && m_State == READING && m_CurEventID == 0)
    {
      m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackbuf);
      m_Real.glPixelStorei(eGL_UNPACK_ALIGNMENT, align);
    }

    SAFE_DELETE_ARRAY(buf);
  }

  return true;
}

void WrappedGLES::glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                                              GLint yoffset, GLint zoffset, GLsizei width,
                                              GLsizei height, GLsizei depth, GLenum format,
                                              GLsizei imageSize, const void *pixels)
{
  m_Real.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth,
                                   format, imageSize, pixels);

  if(m_State >= WRITING)
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }

    CoherentMapImplicitBarrier();

    GLint unpackbuf = 0;
    m_Real.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, &unpackbuf);

    if(m_State == WRITING_IDLE && unpackbuf != 0)
    {
      GetResourceManager()->MarkDirtyResource(record->GetResourceID());
    }
    else
    {
      if(m_HighTrafficResources.find(record->GetResourceID()) != m_HighTrafficResources.end() &&
         m_State == WRITING_IDLE)
        return;

      SCOPED_SERIALISE_CONTEXT(TEXSUBIMAGE3D_COMPRESSED);
      Serialise_glCompressedTexSubImage3D(target, level, xoffset,
                                          yoffset, zoffset, width, height, depth, format,
                                          imageSize, pixels);

      if(m_State == WRITING_CAPFRAME)
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
}


#pragma endregion

#pragma region Tex Buffer

bool WrappedGLES::Serialise_glTexBufferRange(GLenum target,
                                             GLenum internalformat, GLuint buffer,
                                             GLintptr offset, GLsizeiptr size)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(uint64_t, offs, (uint64_t)offset);
  SERIALISE_ELEMENT(uint64_t, Size, (uint64_t)size);
  SERIALISE_ELEMENT(GLenum, fmt, internalformat);
  SERIALISE_ELEMENT(ResourceId, texid, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));

  if(m_State < WRITING)
  {
    if(m_State == READING && m_CurEventID == 0)
    {
      ResourceId liveId = GetResourceManager()->GetLiveID(texid);
      m_Textures[liveId].width =
          uint32_t(Size) / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(fmt), GetDataType(fmt)));
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      m_Textures[liveId].curType = TextureTarget(Target);
      m_Textures[liveId].internalFormat = fmt;
    }

    GLuint buf = 0;

    if(GetResourceManager()->HasLiveResource(bufid))
      buf = GetResourceManager()->GetLiveResource(bufid).name;

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(texid).name, Target);
    m_Real.glTexBufferRange(Target, fmt, buf, (GLintptr)offs, (GLsizeiptr)Size);
  }

  return true;
}

void WrappedGLES::glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer,
                                   GLintptr offset, GLsizeiptr size)
{
  m_Real.glTexBufferRange(target, internalformat, buffer, offset, size);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    CoherentMapImplicitBarrier();

    ResourceId bufid = GetResourceManager()->GetID(BufferRes(GetCtx(), buffer));

    if(record->datatype == eGL_TEXTURE_BINDING_BUFFER &&
       m_Textures[texId].internalFormat == internalformat && m_State == WRITING_IDLE)
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
          record->AddParent(bufRecord);
      }

      return;
    }

    SCOPED_SERIALISE_CONTEXT(TEXBUFFER_RANGE);
    Serialise_glTexBufferRange(target, internalformat, buffer, offset, size);

    if(m_State == WRITING_CAPFRAME)
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
        record->AddParent(bufRecord);
    }


    {
      m_Textures[texId].width =
          uint32_t(size) /
          uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(internalformat), GetDataType(internalformat)));
      m_Textures[texId].height = 1;
      m_Textures[texId].depth = 1;
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 1;
      m_Textures[texId].internalFormat = internalformat;
    }

  }
}

bool WrappedGLES::Serialise_glTexBuffer(GLenum target,
                                        GLenum internalformat, GLuint buffer)
{
  SERIALISE_ELEMENT(GLenum, Target, target);
  SERIALISE_ELEMENT(GLenum, fmt, internalformat);
  SERIALISE_ELEMENT(ResourceId, texid, GetCtxData().GetActiveTexRecord(target)->GetResourceID());
  SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(BufferRes(GetCtx(), buffer)));

  if(m_State < WRITING)
  {
    buffer = GetResourceManager()->GetLiveResource(bufid).name;

    if(m_State == READING && m_CurEventID == 0)
    {
      ResourceId liveId = GetResourceManager()->GetLiveID(texid);
      uint32_t Size = 1;

      GLenum bufferTarget = m_Buffers[GetResourceManager()->GetLiveID(bufid)].curType;
      RDCASSERT(bufferTarget != eGL_NONE);

      GLenum bufferBinding = TextureBinding(bufferTarget);
      GLint prevBind = 0;
      m_Real.glGetIntegerv(bufferBinding, &prevBind);
      m_Real.glBindBuffer(bufferTarget, buffer);
      m_Real.glGetBufferParameteriv(bufferTarget, eGL_BUFFER_SIZE, (GLint *)&Size);
      m_Real.glBindBuffer(bufferTarget, prevBind);

      m_Textures[liveId].width =
          Size / uint32_t(GetByteSize(1, 1, 1, GetBaseFormat(fmt), GetDataType(fmt)));
      m_Textures[liveId].height = 1;
      m_Textures[liveId].depth = 1;
      m_Textures[liveId].curType = TextureTarget(Target);
      m_Textures[liveId].internalFormat = fmt;
    }

    SafeTextureBinder safeTextureBinder(m_Real, GetResourceManager()->GetLiveResource(texid).name, Target);
    m_Real.glTexBuffer(Target, fmt, buffer);
  }

  return true;
}

void WrappedGLES::glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
  m_Real.glTexBuffer(target, internalformat, buffer);

  // saves on queries of the currently bound texture to this target, as we don't have records on
  // replay
  if(m_State < WRITING)
  {
    return;
  }
  else
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);
    if(record == NULL)
    {
      RDCERR("Calling non-DSA texture function with no texture bound to active slot");
      return;
    }
    ResourceId texId = record->GetResourceID();

    CoherentMapImplicitBarrier();

    ResourceId bufid = GetResourceManager()->GetID(BufferRes(GetCtx(), buffer));

    if(record->datatype == eGL_TEXTURE_BINDING_BUFFER &&
       m_Textures[texId].internalFormat == internalformat && m_State == WRITING_IDLE)
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
          record->AddParent(bufRecord);
      }

      return;
    }

    SCOPED_SERIALISE_CONTEXT(TEXBUFFER);
    Serialise_glTexBuffer(target, internalformat, buffer);

    Chunk *chunk = scope.Get();

    if(m_State == WRITING_CAPFRAME)
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
        record->AddParent(bufRecord);
    }


    {
      if(buffer != 0)
      {
        uint32_t size = 1;

        GLenum bufferTarget = m_Buffers[GetResourceManager()->GetLiveID(bufid)].curType;
        RDCASSERT(bufferTarget != eGL_NONE);

        GLenum bufferBinding = TextureBinding(bufferTarget);
        GLint prevBind = 0;
        m_Real.glGetIntegerv(bufferBinding, &prevBind);
        m_Real.glBindBuffer(bufferTarget, buffer);
        m_Real.glGetBufferParameteriv(bufferTarget, eGL_BUFFER_SIZE, (GLint *)&size);
        m_Real.glBindBuffer(bufferTarget, prevBind);

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
      m_Textures[texId].curType = TextureTarget(target);
      m_Textures[texId].dimension = 1;
      m_Textures[texId].internalFormat = internalformat;
    }
  }
}

#pragma endregion
