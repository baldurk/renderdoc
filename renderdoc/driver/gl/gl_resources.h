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

#pragma once

#include "core/resource_manager.h"
#include "driver/gl/gl_common.h"

struct GLHookSet;

size_t GetCompressedByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum internalformat);
size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type);
GLenum GetBaseFormat(GLenum internalFormat);
GLenum GetDataType(GLenum internalFormat);
GLenum GetSizedFormat(const GLHookSet &gl, GLenum target, GLenum internalFormat,
                      GLenum type = eGL_NONE);
void GetFramebufferMipAndLayer(const GLHookSet &gl, GLenum framebuffer, GLenum attachment,
                               GLint *mip, GLint *layer);
void GetTextureSwizzle(const GLHookSet &gl, GLuint tex, GLenum target, GLenum *swizzleRGBA);
void SetTextureSwizzle(const GLHookSet &gl, GLuint tex, GLenum target, GLenum *swizzleRGBA);

bool EmulateLuminanceFormat(const GLHookSet &gl, GLuint tex, GLenum target, GLenum &internalFormat,
                            GLenum &dataFormat);

inline void EmulateGLClamp(GLenum pname, GLenum param)
{
  if(param == eGL_CLAMP)
    param = eGL_CLAMP_TO_EDGE;
}

int GetNumMips(const GLHookSet &gl, GLenum target, GLuint tex, GLuint w, GLuint h, GLuint d);

bool IsCompressedFormat(GLenum internalFormat);
bool IsDepthStencilFormat(GLenum internalFormat);
bool IsUIntFormat(GLenum internalFormat);
bool IsSIntFormat(GLenum internalFormat);
bool IsSRGBFormat(GLenum internalFormat);

bool IsCubeFace(GLenum target);
GLint CubeTargetIndex(GLenum face);
GLenum TextureBinding(GLenum target);
GLenum TextureTarget(GLenum target);
bool IsProxyTarget(GLenum target);

GLenum BufferBinding(GLenum target);
GLenum FramebufferBinding(GLenum target);

enum GLNamespace
{
  eResUnknown = 0,
  eResSpecial,
  eResTexture,
  eResSampler,
  eResFramebuffer,
  eResRenderbuffer,
  eResBuffer,
  eResVertexArray,
  eResShader,
  eResProgram,
  eResProgramPipe,
  eResFeedback,
  eResQuery,
  eResSync,
};

enum GLSpecialResource
{
  eSpecialResDevice = 0,
  eSpecialResContext = 0,
};

enum NullInitialiser
{
  MakeNullResource
};

struct GLResource
{
  GLResource()
  {
    Namespace = eResUnknown;
    Context = NULL;
    name = ~0U;
  }
  GLResource(NullInitialiser)
  {
    Namespace = eResUnknown;
    Context = NULL;
    name = ~0U;
  }
  GLResource(void *ctx, GLNamespace n, GLuint i)
  {
    Context = ctx;
    Namespace = n;
    name = i;
  }

  void *Context;
  GLNamespace Namespace;
  GLuint name;

  bool operator==(const GLResource &o) const
  {
    return Context == o.Context && Namespace == o.Namespace && name == o.name;
  }

  bool operator!=(const GLResource &o) const { return !(*this == o); }
  bool operator<(const GLResource &o) const
  {
    if(Context != o.Context)
      return Context < o.Context;
    if(Namespace != o.Namespace)
      return Namespace < o.Namespace;
    return name < o.name;
  }
};

// Shared objects currently ignore the context parameter.
// For correctness we'd need to check if the context is shared and if so move up to a 'parent'
// so the context value ends up being identical for objects being shared, but can be different
// for objects in non-shared contexts
inline GLResource TextureRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResTexture, i);
}
inline GLResource SamplerRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResSampler, i);
}
inline GLResource FramebufferRes(void *ctx, GLuint i)
{
  return GLResource(VendorCheck[VendorCheck_EXT_fbo_shared] ? NULL : ctx, eResFramebuffer, i);
}
inline GLResource RenderbufferRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResRenderbuffer, i);
}
inline GLResource BufferRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResBuffer, i);
}
inline GLResource VertexArrayRes(void *ctx, GLuint i)
{
  return GLResource(VendorCheck[VendorCheck_EXT_vao_shared] ? NULL : ctx, eResVertexArray, i);
}
inline GLResource ShaderRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResShader, i);
}
inline GLResource ProgramRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResProgram, i);
}
inline GLResource ProgramPipeRes(void *ctx, GLuint i)
{
  return GLResource(ctx, eResProgramPipe, i);
}
inline GLResource FeedbackRes(void *ctx, GLuint i)
{
  return GLResource(ctx, eResFeedback, i);
}
inline GLResource QueryRes(void *ctx, GLuint i)
{
  return GLResource(ctx, eResQuery, i);
}
inline GLResource SyncRes(void *ctx, GLuint i)
{
  (void)ctx;
  return GLResource(NULL, eResSync, i);
}

struct GLResourceRecord : public ResourceRecord
{
  static const NullInitialiser NullResource = MakeNullResource;

  static byte markerValue[32];

  GLResourceRecord(ResourceId id) : ResourceRecord(id, true), datatype(eGL_NONE), usage(eGL_NONE)
  {
    RDCEraseEl(ShadowPtr);
    RDCEraseEl(Map);
  }

  ~GLResourceRecord() { FreeShadowStorage(); }
  enum MapStatus
  {
    Unmapped,
    Mapped_Read,
    Mapped_Write,
    Mapped_Ignore_Real,
  };

  struct
  {
    GLintptr offset;
    GLsizeiptr length;
    GLbitfield access;
    MapStatus status;
    bool invalidate;
    bool verifyWrite;
    byte *ptr;

    byte *persistentPtr;
    int64_t persistentMaps;    // counter indicating how many coherent maps are 'live'
  } Map;

  template <typename ChunkFilter>
  void FilterChunks(const ChunkFilter &filter)
  {
    LockChunks();
    std::vector<std::map<int32_t, Chunk *>::iterator> deletions;
    for(auto it = m_Chunks.begin(); it != m_Chunks.end(); ++it)
    {
      if(filter(it->second))
        deletions.push_back(it);
    }
    for(size_t i = 0; i < deletions.size(); i++)
    {
      SAFE_DELETE(deletions[i]->second);
      m_Chunks.erase(deletions[i]);
    }
    UnlockChunks();
  }

  void VerifyDataType(GLenum target)
  {
#if ENABLED(RDOC_DEVEL)
    if(target == eGL_NONE)
      return;    // target == GL_NONE means ARB_dsa, target was omitted
    if(datatype == eGL_NONE)
      datatype = TextureBinding(target);
    else
      RDCASSERT(datatype == TextureBinding(target));
#endif
  }

  bool AlreadyDataType(GLenum target) { return datatype == TextureBinding(target); }
  GLenum datatype;
  GLenum usage;

  // for texture buffers and texture views, this points from the data texture (or buffer)
  // to the view texture. When preparing resource initial states, we force initial states
  // for anything that is viewed if the viewer is frame referenced. Otherwise we might
  // lose the underlying data for the view.
  // Since it's 1-to-many, we keep a set here.
  set<ResourceId> viewTextures;

  GLResource Resource;

  void AllocShadowStorage(size_t size)
  {
    if(ShadowPtr[0] == NULL)
    {
      ShadowPtr[0] = Serialiser::AllocAlignedBuffer(size + sizeof(markerValue));
      ShadowPtr[1] = Serialiser::AllocAlignedBuffer(size + sizeof(markerValue));

      memcpy(ShadowPtr[0] + size, markerValue, sizeof(markerValue));
      memcpy(ShadowPtr[1] + size, markerValue, sizeof(markerValue));

      ShadowSize = size;
    }
  }

  bool VerifyShadowStorage()
  {
    if(ShadowPtr[0] && memcmp(ShadowPtr[0] + ShadowSize, markerValue, sizeof(markerValue)))
      return false;

    if(ShadowPtr[1] && memcmp(ShadowPtr[1] + ShadowSize, markerValue, sizeof(markerValue)))
      return false;

    return true;
  }

  void FreeShadowStorage()
  {
    if(ShadowPtr[0] != NULL)
    {
      Serialiser::FreeAlignedBuffer(ShadowPtr[0]);
      Serialiser::FreeAlignedBuffer(ShadowPtr[1]);
    }
    ShadowPtr[0] = ShadowPtr[1] = NULL;
  }

  byte *GetShadowPtr(int p) { return ShadowPtr[p]; }
private:
  byte *ShadowPtr[2];
  size_t ShadowSize;
};
