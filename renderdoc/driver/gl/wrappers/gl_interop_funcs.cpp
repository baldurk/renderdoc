/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#if ENABLED(RDOC_WIN32) && ENABLED(RENDERDOC_DX_GL_INTEROP)

struct ID3D11Resource;

void *UnwrapDXDevice(void *dxDevice);
ID3D11Resource *UnwrapDXResource(void *dxObject);
void GetDXTextureProperties(void *dxObject, ResourceFormat &fmt, uint32_t &width, uint32_t &height,
                            uint32_t &depth, uint32_t &mips, uint32_t &layers, uint32_t &samples);

BOOL WrappedOpenGL::wglDXSetResourceShareHandleNV(void *dxObject, HANDLE shareHandle)
{
  // no-op
  return m_Real.wglDXSetResourceShareHandleNV(dxObject, shareHandle);
}

HANDLE WrappedOpenGL::wglDXOpenDeviceNV(void *dxDevice)
{
  void *unwrapped = UnwrapDXDevice(dxDevice);
  if(unwrapped)
  {
    HANDLE ret = (HANDLE)m_Real.wglDXOpenDeviceNV(unwrapped);

    SetLastError(S_OK);

    return ret;
  }

  SetLastError(ERROR_NOT_SUPPORTED);

  return 0;
}

BOOL WrappedOpenGL::wglDXCloseDeviceNV(HANDLE hDevice)
{
  return m_Real.wglDXCloseDeviceNV(hDevice);
}

struct WrappedHANDLE
{
  HANDLE real;
  GLResource res;
};

HANDLE Unwrap(HANDLE h)
{
  return ((WrappedHANDLE *)h)->real;
}

HANDLE WrappedOpenGL::wglDXRegisterObjectNV(HANDLE hDevice, void *dxObject, GLuint name,
                                            GLenum type, GLenum access)
{
  RDCASSERT(m_State >= WRITING);

  ID3D11Resource *real = UnwrapDXResource(dxObject);

  if(real == NULL)
  {
    SetLastError(ERROR_OPEN_FAILED);
    return 0;
  }

  WrappedHANDLE *wrapped = new WrappedHANDLE();
  wrapped->res =
      type == eGL_RENDERBUFFER ? RenderbufferRes(GetCtx(), name) : TextureRes(GetCtx(), name);
  wrapped->real = m_Real.wglDXRegisterObjectNV(hDevice, real, name, type, access);

  GLResourceRecord *record = GetResourceManager()->GetResourceRecord(wrapped->res);

  {
    RDCASSERT(record);

    SCOPED_SERIALISE_CONTEXT(INTEROP_INIT);
    Serialise_wglDXRegisterObjectNV(wrapped->res, type, dxObject);

    record->AddChunk(scope.Get());
  }

  {
    ResourceFormat fmt;
    uint32_t width = 0, height = 0, depth = 0, mips = 0, layers = 0, samples = 0;
    GetDXTextureProperties(dxObject, fmt, width, height, depth, mips, layers, samples);

    ResourceId texId = record->GetResourceID();
    m_Textures[texId].resource = wrapped->res;
    m_Textures[texId].curType = type;
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = RDCMAX(depth, samples);
    m_Textures[texId].samples = samples;
    m_Textures[texId].mips = mips;
    m_Textures[texId].dimension = 2;
    if(type == eGL_TEXTURE_1D || type == eGL_TEXTURE_1D_ARRAY)
      m_Textures[texId].dimension = 1;
    else if(type == eGL_TEXTURE_3D)
      m_Textures[texId].dimension = 3;

    if(type == eGL_NONE)
    {
      m_Textures[texId].curType = eGL_TEXTURE_BUFFER;
      m_Textures[texId].dimension = 1;
    }

    m_Textures[texId].internalFormat = MakeGLFormat(*this, fmt);
  }

  return wrapped;
}

BOOL WrappedOpenGL::wglDXUnregisterObjectNV(HANDLE hDevice, HANDLE hObject)
{
  // don't need to intercept this, as the DX and GL textures will be deleted independently
  BOOL ret = m_Real.wglDXUnregisterObjectNV(hDevice, Unwrap(hObject));

  delete(WrappedHANDLE *)hObject;

  return ret;
}

BOOL WrappedOpenGL::wglDXObjectAccessNV(HANDLE hObject, GLenum access)
{
  // we don't need to care about access
  return m_Real.wglDXObjectAccessNV(Unwrap(hObject), access);
}

BOOL WrappedOpenGL::wglDXLockObjectsNV(HANDLE hDevice, GLint count, HANDLE *hObjects)
{
  HANDLE *unwrapped = new HANDLE[count];
  for(GLint i = 0; i < count; i++)
    unwrapped[i] = Unwrap(hObjects[i]);

  BOOL ret = m_Real.wglDXLockObjectsNV(hDevice, count, unwrapped);

  if(m_State == WRITING_CAPFRAME)
  {
    for(GLint i = 0; i < count; i++)
    {
      WrappedHANDLE *w = (WrappedHANDLE *)hObjects[i];

      SCOPED_SERIALISE_CONTEXT(INTEROP_DATA);
      Serialise_wglDXLockObjectsNV(w->res);

      m_ContextRecord->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(GetResourceManager()->GetID(w->res),
                                                        eFrameRef_Read);
    }
  }

  SAFE_DELETE_ARRAY(unwrapped);
  return ret;
}

BOOL WrappedOpenGL::wglDXUnlockObjectsNV(HANDLE hDevice, GLint count, HANDLE *hObjects)
{
  HANDLE *unwrapped = new HANDLE[count];
  for(GLint i = 0; i < count; i++)
    unwrapped[i] = Unwrap(hObjects[i]);
  BOOL ret = m_Real.wglDXUnlockObjectsNV(hDevice, count, unwrapped);
  SAFE_DELETE_ARRAY(unwrapped);
  return ret;
}

#else

BOOL WrappedOpenGL::wglDXSetResourceShareHandleNV(void *dxObject, HANDLE shareHandle)
{
  return 0;
}

HANDLE WrappedOpenGL::wglDXOpenDeviceNV(void *dxDevice)
{
  return 0;
}

BOOL WrappedOpenGL::wglDXCloseDeviceNV(HANDLE hDevice)
{
  return 0;
}

HANDLE WrappedOpenGL::wglDXRegisterObjectNV(HANDLE hDevice, void *dxObject, GLuint name,
                                            GLenum type, GLenum access)
{
  return 0;
}

BOOL WrappedOpenGL::wglDXUnregisterObjectNV(HANDLE hDevice, HANDLE hObject)
{
  return 0;
}

BOOL WrappedOpenGL::wglDXObjectAccessNV(HANDLE hObject, GLenum access)
{
  return 0;
}

BOOL WrappedOpenGL::wglDXLockObjectsNV(HANDLE hDevice, GLint count, HANDLE *hObjects)
{
  return 0;
}

BOOL WrappedOpenGL::wglDXUnlockObjectsNV(HANDLE hDevice, GLint count, HANDLE *hObjects)
{
  return 0;
}

#endif

bool WrappedOpenGL::Serialise_wglDXRegisterObjectNV(GLResource res, GLenum type, void *dxObject)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(res));

  GLenum internalFormat = eGL_NONE;
  uint32_t width = 0, height = 0, depth = 0, mips = 0, layers = 0, samples = 0;
  if(m_State >= WRITING)
  {
    ResourceFormat format;
#if ENABLED(RDOC_WIN32) && ENABLED(RENDERDOC_DX_GL_INTEROP)
    GetDXTextureProperties(dxObject, format, width, height, depth, mips, layers, samples);
    internalFormat = MakeGLFormat(*this, format);
#else
    RDCERR("Should never happen - cannot serialise wglDXRegisterObjectNV, interop is disabled");
#endif
  }

  m_pSerialiser->Serialise("type", type);
  m_pSerialiser->Serialise("internalFormat", internalFormat);
  m_pSerialiser->Serialise("width", width);
  m_pSerialiser->Serialise("height", height);
  m_pSerialiser->Serialise("depth", depth);
  m_pSerialiser->Serialise("mips", mips);
  m_pSerialiser->Serialise("layers", layers);
  m_pSerialiser->Serialise("samples", samples);

  if(m_State < WRITING)
  {
    GLuint name = GetResourceManager()->GetLiveResource(id).name;

    switch(type)
    {
      case eGL_NONE:
      case eGL_TEXTURE_BUFFER:
      {
        GLuint buf = 0;
        m_Real.glGenBuffers(1, &buf);
        m_Real.glBindBuffer(eGL_TEXTURE_BUFFER, buf);
        m_Real.glNamedBufferStorageEXT(name, width, NULL, GL_DYNAMIC_STORAGE_BIT);
        m_Real.glTextureBufferEXT(name, type, internalFormat, buf);
        break;
      }
      case eGL_TEXTURE_1D:
        m_Real.glTextureStorage1DEXT(name, type, mips, internalFormat, width);
        break;
      case eGL_TEXTURE_1D_ARRAY:
        m_Real.glTextureStorage2DEXT(name, type, mips, internalFormat, width, layers);
        break;
      // treat renderbuffers and texture rects as tex2D just to make things easier
      case eGL_RENDERBUFFER:
      case eGL_TEXTURE_RECTANGLE:
      case eGL_TEXTURE_2D:
      case eGL_TEXTURE_CUBE_MAP:
        m_Real.glTextureStorage2DEXT(name, type, mips, internalFormat, width, height);
        break;
      case eGL_TEXTURE_2D_ARRAY:
      case eGL_TEXTURE_CUBE_MAP_ARRAY:
        m_Real.glTextureStorage3DEXT(name, type, mips, internalFormat, width, height, layers);
        break;
      case eGL_TEXTURE_2D_MULTISAMPLE:
        m_Real.glTextureStorage2DMultisampleEXT(name, type, samples, internalFormat, width, height,
                                                GL_TRUE);
        break;
      case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        m_Real.glTextureStorage3DMultisampleEXT(name, type, samples, internalFormat, width, height,
                                                layers, GL_TRUE);
        break;
      case eGL_TEXTURE_3D:
        m_Real.glTextureStorage3DEXT(name, type, mips, internalFormat, width, height, depth);
        break;
      default: RDCERR("Unexpected type of interop texture: %s", ToStr::Get(type).c_str()); break;
    }

    ResourceId liveId = GetResourceManager()->GetLiveID(id);
    m_Textures[liveId].curType = type;
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = RDCMAX(depth, samples);
    m_Textures[liveId].samples = samples;
    m_Textures[liveId].mips = mips;
    m_Textures[liveId].dimension = 2;
    if(type == eGL_TEXTURE_1D || type == eGL_TEXTURE_1D_ARRAY)
      m_Textures[liveId].dimension = 1;
    else if(type == eGL_TEXTURE_3D)
      m_Textures[liveId].dimension = 3;

    if(type == eGL_NONE)
    {
      m_Textures[liveId].curType = eGL_TEXTURE_BUFFER;
      m_Textures[liveId].dimension = 1;
    }

    m_Textures[liveId].internalFormat = internalFormat;
  }

  return true;
}

bool WrappedOpenGL::Serialise_wglDXLockObjectsNV(GLResource res)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(res));
  SERIALISE_ELEMENT(GLenum, textype, m_Textures[id].curType);

  const GLHookSet &gl = m_Real;

  if(m_State >= WRITING)
  {
    GLuint ppb = 0;
    gl.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
    gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);

    GLint packParams[8];
    gl.glGetIntegerv(eGL_PACK_SWAP_BYTES, &packParams[0]);
    gl.glGetIntegerv(eGL_PACK_LSB_FIRST, &packParams[1]);
    gl.glGetIntegerv(eGL_PACK_ROW_LENGTH, &packParams[2]);
    gl.glGetIntegerv(eGL_PACK_IMAGE_HEIGHT, &packParams[3]);
    gl.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &packParams[4]);
    gl.glGetIntegerv(eGL_PACK_SKIP_ROWS, &packParams[5]);
    gl.glGetIntegerv(eGL_PACK_SKIP_IMAGES, &packParams[6]);
    gl.glGetIntegerv(eGL_PACK_ALIGNMENT, &packParams[7]);

    gl.glPixelStorei(eGL_PACK_SWAP_BYTES, 0);
    gl.glPixelStorei(eGL_PACK_LSB_FIRST, 0);
    gl.glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
    gl.glPixelStorei(eGL_PACK_IMAGE_HEIGHT, 0);
    gl.glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
    gl.glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
    gl.glPixelStorei(eGL_PACK_SKIP_IMAGES, 0);
    gl.glPixelStorei(eGL_PACK_ALIGNMENT, 1);

    {
      TextureData &details = m_Textures[id];
      GLuint tex = res.name;

      GLenum fmt = GetBaseFormat(details.internalFormat);
      GLenum type = GetDataType(details.internalFormat);

      size_t size = GetByteSize(details.width, details.height, details.depth, fmt, type);

      byte *buf = new byte[size];

      GLenum binding = TextureBinding(details.curType);

      GLuint prevtex = 0;
      gl.glGetIntegerv(binding, (GLint *)&prevtex);

      gl.glBindTexture(textype, tex);

      for(int i = 0; i < details.mips; i++)
      {
        int w = RDCMAX(details.width >> i, 1);
        int h = RDCMAX(details.height >> i, 1);
        int d = RDCMAX(details.depth >> i, 1);

        if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_1D_ARRAY ||
           textype == eGL_TEXTURE_2D_ARRAY)
          d = details.depth;

        size = GetByteSize(w, h, d, fmt, type);

        GLenum targets[] = {
            eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        };

        int count = ARRAY_COUNT(targets);

        if(textype != eGL_TEXTURE_CUBE_MAP)
        {
          targets[0] = textype;
          count = 1;
        }

        for(int trg = 0; trg < count; trg++)
        {
          // we avoid glGetTextureImageEXT as it seems buggy for cubemap faces
          gl.glGetTexImage(targets[trg], i, fmt, type, buf);

          m_pSerialiser->SerialiseBuffer("image", buf, size);
        }
      }

      gl.glBindTexture(textype, prevtex);

      SAFE_DELETE_ARRAY(buf);
    }

    gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);

    gl.glPixelStorei(eGL_PACK_SWAP_BYTES, packParams[0]);
    gl.glPixelStorei(eGL_PACK_LSB_FIRST, packParams[1]);
    gl.glPixelStorei(eGL_PACK_ROW_LENGTH, packParams[2]);
    gl.glPixelStorei(eGL_PACK_IMAGE_HEIGHT, packParams[3]);
    gl.glPixelStorei(eGL_PACK_SKIP_PIXELS, packParams[4]);
    gl.glPixelStorei(eGL_PACK_SKIP_ROWS, packParams[5]);
    gl.glPixelStorei(eGL_PACK_SKIP_IMAGES, packParams[6]);
    gl.glPixelStorei(eGL_PACK_ALIGNMENT, packParams[7]);
  }
  else
  {
    GLuint pub = 0;
    gl.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
    gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

    GLint unpackParams[8];
    gl.glGetIntegerv(eGL_UNPACK_SWAP_BYTES, &unpackParams[0]);
    gl.glGetIntegerv(eGL_UNPACK_LSB_FIRST, &unpackParams[1]);
    gl.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &unpackParams[2]);
    gl.glGetIntegerv(eGL_UNPACK_IMAGE_HEIGHT, &unpackParams[3]);
    gl.glGetIntegerv(eGL_UNPACK_SKIP_PIXELS, &unpackParams[4]);
    gl.glGetIntegerv(eGL_UNPACK_SKIP_ROWS, &unpackParams[5]);
    gl.glGetIntegerv(eGL_UNPACK_SKIP_IMAGES, &unpackParams[6]);
    gl.glGetIntegerv(eGL_UNPACK_ALIGNMENT, &unpackParams[7]);

    gl.glPixelStorei(eGL_UNPACK_SWAP_BYTES, 0);
    gl.glPixelStorei(eGL_UNPACK_LSB_FIRST, 0);
    gl.glPixelStorei(eGL_UNPACK_ROW_LENGTH, 0);
    gl.glPixelStorei(eGL_UNPACK_IMAGE_HEIGHT, 0);
    gl.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, 0);
    gl.glPixelStorei(eGL_UNPACK_SKIP_ROWS, 0);
    gl.glPixelStorei(eGL_UNPACK_SKIP_IMAGES, 0);
    gl.glPixelStorei(eGL_UNPACK_ALIGNMENT, 1);

    {
      TextureData &details = m_Textures[GetResourceManager()->GetLiveID(id)];
      GLuint tex = GetResourceManager()->GetLiveResource(id).name;

      GLenum fmt = GetBaseFormat(details.internalFormat);
      GLenum type = GetDataType(details.internalFormat);

      GLint dim = details.dimension;

      for(int i = 0; i < details.mips; i++)
      {
        uint32_t w = RDCMAX(details.width >> i, 1);
        uint32_t h = RDCMAX(details.height >> i, 1);
        uint32_t d = RDCMAX(details.depth >> i, 1);

        if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_1D_ARRAY ||
           textype == eGL_TEXTURE_2D_ARRAY)
          d = details.depth;

        GLenum targets[] = {
            eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        };

        int count = ARRAY_COUNT(targets);

        if(textype != eGL_TEXTURE_CUBE_MAP)
        {
          targets[0] = textype;
          count = 1;
        }

        for(int trg = 0; trg < count; trg++)
        {
          size_t size = 0;
          byte *buf = NULL;
          m_pSerialiser->SerialiseBuffer("image", buf, size);

          if(dim == 1)
            gl.glTextureSubImage1DEXT(tex, targets[trg], i, 0, w, fmt, type, buf);
          else if(dim == 2)
            gl.glTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, fmt, type, buf);
          else if(dim == 3)
            gl.glTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, fmt, type, buf);

          delete[] buf;
        }
      }
    }

    gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);

    gl.glPixelStorei(eGL_UNPACK_SWAP_BYTES, unpackParams[0]);
    gl.glPixelStorei(eGL_UNPACK_LSB_FIRST, unpackParams[1]);
    gl.glPixelStorei(eGL_UNPACK_ROW_LENGTH, unpackParams[2]);
    gl.glPixelStorei(eGL_UNPACK_IMAGE_HEIGHT, unpackParams[3]);
    gl.glPixelStorei(eGL_UNPACK_SKIP_PIXELS, unpackParams[4]);
    gl.glPixelStorei(eGL_UNPACK_SKIP_ROWS, unpackParams[5]);
    gl.glPixelStorei(eGL_UNPACK_SKIP_IMAGES, unpackParams[6]);
    gl.glPixelStorei(eGL_UNPACK_ALIGNMENT, unpackParams[7]);
  }

  return true;
}
