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

#pragma once

#include "core/resource_manager.h"
#include "gl_common.h"

size_t GetCompressedByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum internalformat);
rdcfixedarray<uint32_t, 3> GetCompressedBlockSize(GLenum internalformat);
size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type);
GLenum GetBaseFormat(GLenum internalFormat);
GLenum GetDataType(GLenum internalFormat);
void GetFramebufferMipAndLayer(GLuint framebuffer, GLenum attachment, GLint *mip, GLint *layer);

inline void GetFramebufferMipAndLayer(GLuint framebuffer, GLenum attachment, uint8_t *mip,
                                      uint16_t *layer)
{
  GLint outMip = 0, outLayer = 0;
  GetFramebufferMipAndLayer(framebuffer, attachment, &outMip, &outLayer);
  *mip = outMip & 0xff;
  *layer = outLayer & 0xff;
}

void GetTextureSwizzle(GLuint tex, GLenum target, GLenum *swizzleRGBA);
void SetTextureSwizzle(GLuint tex, GLenum target, const GLenum *swizzleRGBA);

rdcstr GetTextureCompleteStatus(GLenum target, GLuint tex, GLuint sampler);

bool EmulateLuminanceFormat(GLuint tex, GLenum target, GLenum &internalFormat, GLenum &dataFormat);
GLenum GetSizedFormat(GLenum internalFormat);

inline void EmulateGLClamp(GLenum pname, GLenum param)
{
  if(param == eGL_CLAMP)
    param = eGL_CLAMP_TO_EDGE;
}

int GetNumMips(GLenum target, GLuint tex, GLuint w, GLuint h, GLuint d);

bool IsCompressedFormat(GLenum internalFormat);
bool IsDepthStencilFormat(GLenum internalFormat);
bool IsUIntFormat(GLenum internalFormat);
bool IsSIntFormat(GLenum internalFormat);
bool IsSRGBFormat(GLenum internalFormat);

GLenum GetViewCastedFormat(GLenum internalFormat, CompType typeCast);

bool IsCubeFace(GLenum target);
GLint CubeTargetIndex(GLenum face);
GLenum TextureBinding(GLenum target);
GLenum TextureTarget(GLenum binding);
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
  eResExternalMemory,
  eResExternalSemaphore,
};

DECLARE_REFLECTION_ENUM(GLNamespace);

enum GLSpecialResource
{
  eSpecialResDevice = 0,
  eSpecialResContext = 0,
  eSpecialResDescriptorStorage = 0,
};

enum NullInitialiser
{
  MakeNullResource
};

struct GLResource
{
  GLResource()
  {
    ContextShareGroup = NULL;
    Namespace = eResUnknown;
    name = 0;
  }
  GLResource(NullInitialiser)
  {
    ContextShareGroup = NULL;
    Namespace = eResUnknown;
    name = 0;
  }
  GLResource(void *ctx, GLNamespace n, GLuint i)
  {
    ContextShareGroup = ctx;
    Namespace = n;
    name = i;
  }

  void *ContextShareGroup;
  GLNamespace Namespace;
  GLuint name;

  bool operator==(const GLResource &o) const
  {
    return ContextShareGroup == o.ContextShareGroup && Namespace == o.Namespace && name == o.name;
  }

  bool operator!=(const GLResource &o) const { return !(*this == o); }
  bool operator<(const GLResource &o) const
  {
    if(ContextShareGroup != o.ContextShareGroup)
      return ContextShareGroup < o.ContextShareGroup;
    if(Namespace != o.Namespace)
      return Namespace < o.Namespace;
    return name < o.name;
  }
};

DECLARE_REFLECTION_STRUCT(GLResource);

struct ContextPair
{
  void *ctx;
  void *shareGroup;
};

// Shared objects currently ignore the context parameter.
// For correctness we'd need to check if the context is shared and if so move up to a 'parent'
// so the context value ends up being identical for objects being shared, but can be different
// for objects in non-shared contexts
inline GLResource TextureRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResTexture, i);
}
inline GLResource SamplerRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResSampler, i);
}
inline GLResource FramebufferRes(const ContextPair &c, GLuint i)
{
  return GLResource(VendorCheck[VendorCheck_EXT_fbo_shared] ? c.shareGroup : c.ctx, eResFramebuffer,
                    i);
}
inline GLResource RenderbufferRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResRenderbuffer, i);
}
inline GLResource BufferRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResBuffer, i);
}
inline GLResource VertexArrayRes(const ContextPair &c, GLuint i)
{
  return GLResource(VendorCheck[VendorCheck_EXT_vao_shared] ? c.shareGroup : c.ctx, eResVertexArray,
                    i);
}
inline GLResource ShaderRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResShader, i);
}
inline GLResource ProgramRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResProgram, i);
}
inline GLResource ProgramPipeRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.ctx, eResProgramPipe, i);
}
inline GLResource FeedbackRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.ctx, eResFeedback, i);
}
inline GLResource QueryRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.ctx, eResQuery, i);
}
inline GLResource SyncRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.shareGroup, eResSync, i);
}
inline GLResource ExtMemRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.ctx, eResExternalMemory, i);
}
inline GLResource ExtSemRes(const ContextPair &c, GLuint i)
{
  return GLResource(c.ctx, eResExternalSemaphore, i);
}

struct GLResourceRecord : public ResourceRecord
{
  static const NullInitialiser NullResource = MakeNullResource;

  static byte markerValue[32];

  GLResourceRecord(ResourceId id) : ResourceRecord(id, true), datatype(eGL_NONE), usage(eGL_NONE)
  {
    RDCEraseEl(ShadowPtr);
    RDCEraseEl(Map);
    ShadowSize = 0;
  }

  ~GLResourceRecord() { FreeShadowStorage(); }
  enum MapStatus
  {
    Unmapped,
    Mapped_Write,
    Mapped_Direct,
  };

  struct
  {
    GLintptr offset;
    GLsizeiptr length;
    GLbitfield access;
    MapStatus status;
    bool invalidate;
    bool verifyWrite;
    bool orphaned;
    bool persistent;
    byte *ptr;
  } Map;

  void VerifyDataType(GLenum target)
  {
#if ENABLED(RDOC_DEVEL)
    if(target == eGL_NONE)
      return;    // target == GL_NONE means ARB_dsa, target was omitted
    if(datatype != eGL_NONE)
      RDCASSERT(datatype == TextureBinding(target));
#endif
  }

  bool AlreadyDataType(GLenum target) { return datatype == TextureBinding(target); }
  union
  {
    GLenum datatype;
    uint32_t age;
  };
  GLenum usage;

  // for texture buffers, this points from the texture to the real buffer, for texture views this
  // points to the texture that's being viewed (the actual data source).
  // When we mark a resource as dirty or frame referenced, we also mark the underlyingData resource
  // the same, so that if we dirty the texture then we dirty the buffer/real texture, and so that if
  // the texture is used then we bring the buffer/real texture into the frame.
  ResourceId viewSource;

  GLResource Resource;

  void AllocShadowStorage(size_t size)
  {
    if(ShadowSize != size)
      FreeShadowStorage();

    if(ShadowPtr[0] == NULL)
    {
      ShadowPtr[0] = AllocAlignedBuffer(size + sizeof(markerValue));
      ShadowPtr[1] = AllocAlignedBuffer(size + sizeof(markerValue));

      memcpy(ShadowPtr[0] + size, markerValue, sizeof(markerValue));
      memcpy(ShadowPtr[1] + size, markerValue, sizeof(markerValue));

      ShadowSize = size;
    }
  }

  bool VerifyShadowStorage()
  {
    if(ShadowPtr[0] && memcmp(ShadowPtr[0] + ShadowSize, markerValue, sizeof(markerValue)) != 0)
      return false;

    if(ShadowPtr[1] && memcmp(ShadowPtr[1] + ShadowSize, markerValue, sizeof(markerValue)) != 0)
      return false;

    return true;
  }

  void FreeShadowStorage()
  {
    if(ShadowPtr[0] != NULL)
    {
      FreeAlignedBuffer(ShadowPtr[0]);
      FreeAlignedBuffer(ShadowPtr[1]);
    }
    ShadowPtr[0] = ShadowPtr[1] = NULL;
    ShadowSize = 0;
  }

  byte *GetShadowPtr(int p) { return ShadowPtr[p]; }
private:
  byte *ShadowPtr[2];
  size_t ShadowSize;
};

struct GLContextTLSData
{
  GLContextTLSData() : ctxPair({NULL, NULL}), ctxRecord(NULL) {}
  GLContextTLSData(ContextPair p, GLResourceRecord *r) : ctxPair(p), ctxRecord(r) {}
  ContextPair ctxPair;
  GLResourceRecord *ctxRecord;
};
