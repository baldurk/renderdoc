/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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
  RDCASSERT(IsCaptureMode(m_State));

  ID3D11Resource *real = UnwrapDXResource(dxObject);

  if(real == NULL)
  {
    SetLastError(ERROR_OPEN_FAILED);
    return 0;
  }

  WrappedHANDLE *wrapped = new WrappedHANDLE();

  if(type == eGL_RENDERBUFFER)
    wrapped->res = RenderbufferRes(GetCtx(), name);
  else if(type == eGL_NONE)
    wrapped->res = BufferRes(GetCtx(), name);
  else
    wrapped->res = TextureRes(GetCtx(), name);

  GLResourceRecord *record = GetResourceManager()->GetResourceRecord(wrapped->res);

  if(!record)
  {
    RDCERR("Unrecognised object with type %x and name %u", type, name);
    delete wrapped;
    return NULL;
  }

  SERIALISE_TIME_CALL(wrapped->real =
                          m_Real.wglDXRegisterObjectNV(hDevice, real, name, type, access));

  {
    RDCASSERT(record);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_wglDXRegisterObjectNV(ser, wrapped->res, type, dxObject);

    record->AddChunk(scope.Get());
  }

  if(type != eGL_NONE)
  {
    ResourceFormat fmt;
    uint32_t width = 0, height = 0, depth = 0, mips = 0, layers = 0, samples = 0;
    GetDXTextureProperties(dxObject, fmt, width, height, depth, mips, layers, samples);

    // defined as arrays mostly for Coverity code analysis to stay calm about passing
    // them to the *TexParameter* functions
    GLint maxlevel[4] = {GLint(mips - 1), 0, 0, 0};

    m_Real.glTextureParameteriEXT(wrapped->res.name, type, eGL_TEXTURE_MAX_LEVEL, GLint(mips - 1));

    ResourceId texId = record->GetResourceID();
    m_Textures[texId].resource = wrapped->res;
    m_Textures[texId].curType = type;
    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = RDCMAX(depth, samples);
    m_Textures[texId].samples = samples;
    m_Textures[texId].dimension = 2;
    if(type == eGL_TEXTURE_1D || type == eGL_TEXTURE_1D_ARRAY)
      m_Textures[texId].dimension = 1;
    else if(type == eGL_TEXTURE_3D)
      m_Textures[texId].dimension = 3;

    m_Textures[texId].internalFormat = MakeGLFormat(fmt);
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

  BOOL ret;
  SERIALISE_TIME_CALL(ret = m_Real.wglDXLockObjectsNV(hDevice, count, unwrapped));

  if(IsActiveCapturing(m_State))
  {
    for(GLint i = 0; i < count; i++)
    {
      WrappedHANDLE *w = (WrappedHANDLE *)hObjects[i];

      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_wglDXLockObjectsNV(ser, w->res);

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

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_wglDXRegisterObjectNV(SerialiserType &ser, GLResource Resource,
                                                    GLenum type, void *dxObject)
{
  SERIALISE_ELEMENT(Resource);

  GLenum internalFormat = eGL_NONE;
  uint32_t width = 0, height = 0, depth = 0, mips = 0, layers = 0, samples = 0;
  if(ser.IsWriting())
  {
    ResourceFormat format;
#if ENABLED(RDOC_WIN32) && ENABLED(RENDERDOC_DX_GL_INTEROP)
    GetDXTextureProperties(dxObject, format, width, height, depth, mips, layers, samples);
    if(type != eGL_NONE)
      internalFormat = MakeGLFormat(format);
#else
    RDCERR("Should never happen - cannot serialise wglDXRegisterObjectNV, interop is disabled");
#endif
  }

  SERIALISE_ELEMENT(type);
  SERIALISE_ELEMENT(internalFormat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT(mips);
  SERIALISE_ELEMENT(layers);
  SERIALISE_ELEMENT(samples);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    GLuint name = Resource.name;

    switch(type)
    {
      case eGL_NONE:
      case eGL_TEXTURE_BUFFER:
      {
        m_Real.glNamedBufferDataEXT(name, width, NULL, eGL_STATIC_DRAW);
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
      default: RDCERR("Unexpected type of interop texture: %s", ToStr(type).c_str()); break;
    }

    if(type != eGL_NONE)
    {
      ResourceId liveId = GetResourceManager()->GetID(Resource);
      m_Textures[liveId].curType = type;
      m_Textures[liveId].width = width;
      m_Textures[liveId].height = height;
      m_Textures[liveId].depth = RDCMAX(depth, samples);
      m_Textures[liveId].samples = samples;
      m_Textures[liveId].dimension = 2;
      if(type == eGL_TEXTURE_1D || type == eGL_TEXTURE_1D_ARRAY)
        m_Textures[liveId].dimension = 1;
      else if(type == eGL_TEXTURE_3D)
        m_Textures[liveId].dimension = 3;

      m_Textures[liveId].internalFormat = internalFormat;
    }

    AddResourceInitChunk(Resource);
  }

  return true;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_wglDXLockObjectsNV(SerialiserType &ser, GLResource Resource)
{
  SERIALISE_ELEMENT(Resource);
  SERIALISE_ELEMENT_LOCAL(textype, Resource.Namespace == eResBuffer
                                       ? eGL_NONE
                                       : m_Textures[GetResourceManager()->GetID(Resource)].curType)
      .Hidden();

  const GLHookSet &gl = m_Real;

  // buffer contents are easier to save
  if(textype == eGL_NONE)
  {
    byte *Contents = NULL;
    uint32_t length = 1;

    // while writing, fetch the buffer's size and contents
    if(ser.IsWriting())
    {
      gl.glGetNamedBufferParameterivEXT(Resource.name, eGL_BUFFER_SIZE, (GLint *)&length);

      Contents = new byte[length];

      GLuint oldbuf = 0;
      gl.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf);
      gl.glBindBuffer(eGL_COPY_READ_BUFFER, Resource.name);

      gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, (GLsizeiptr)length, Contents);

      gl.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf);
    }

    SERIALISE_ELEMENT_ARRAY(Contents, length);
    SERIALISE_ELEMENT(length);

    SERIALISE_CHECK_READ_ERRORS();

    // restore on replay
    if(IsReplayingAndReading())
    {
      uint32_t liveLength = 1;
      gl.glGetNamedBufferParameterivEXT(Resource.name, eGL_BUFFER_SIZE, (GLint *)&liveLength);

      gl.glNamedBufferSubData(Resource.name, 0, (GLsizeiptr)RDCMIN(length, liveLength), Contents);
    }
  }
  else
  {
    GLuint ppb = 0, pub = 0;
    PixelPackState pack;
    PixelUnpackState unpack;

    // save and restore pixel pack/unpack state. We only need one or the other but for clarity we
    // push and pop both always.
    if(ser.IsWriting() || !IsStructuredExporting(m_State))
    {
      gl.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
      gl.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
      gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
      gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      pack.Fetch(&gl, false);
      unpack.Fetch(&gl, false);

      ResetPixelPackState(gl, false, 1);
      ResetPixelUnpackState(gl, false, 1);
    }

    TextureData &details = m_Textures[GetResourceManager()->GetID(Resource)];
    GLuint tex = Resource.name;

    // serialise the metadata for convenience
    SERIALISE_ELEMENT_LOCAL(internalFormat, details.internalFormat).Hidden();
    SERIALISE_ELEMENT_LOCAL(width, details.width).Hidden();
    SERIALISE_ELEMENT_LOCAL(height, details.height).Hidden();
    SERIALISE_ELEMENT_LOCAL(depth, details.depth).Hidden();

    RDCASSERT(internalFormat == details.internalFormat, internalFormat, details.internalFormat);
    RDCASSERT(width == details.width, width, details.width);
    RDCASSERT(height == details.height, height, details.height);
    RDCASSERT(depth == details.depth, depth, details.depth);

    GLenum fmt = GetBaseFormat(internalFormat);
    GLenum type = GetDataType(internalFormat);

    GLint dim = details.dimension;

    uint32_t size = (uint32_t)GetByteSize(width, height, depth, fmt, type);

    int mips = 0;
    if(!IsStructuredExporting(m_State))
      mips = GetNumMips(gl, textype, tex, width, height, depth);

    SERIALISE_ELEMENT(mips).Hidden();

    byte *scratchBuf = NULL;

    // on read and write, we allocate a single buffer big enough for all mips and re-use it
    // to avoid repeated new/free.
    scratchBuf = AllocAlignedBuffer(size);

    GLuint prevtex = 0;
    if(!IsStructuredExporting(m_State))
    {
      gl.glGetIntegerv(TextureBinding(details.curType), (GLint *)&prevtex);
      gl.glBindTexture(textype, tex);
    }

    for(int i = 0; i < mips; i++)
    {
      int w = RDCMAX(details.width >> i, 1);
      int h = RDCMAX(details.height >> i, 1);
      int d = RDCMAX(details.depth >> i, 1);

      if(textype == eGL_TEXTURE_CUBE_MAP_ARRAY || textype == eGL_TEXTURE_1D_ARRAY ||
         textype == eGL_TEXTURE_2D_ARRAY)
        d = details.depth;

      size = (uint32_t)GetByteSize(w, h, d, fmt, type);

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
        if(ser.IsWriting())
        {
          // we avoid glGetTextureImageEXT as it seems buggy for cubemap faces
          gl.glGetTexImage(targets[trg], i, fmt, type, scratchBuf);
        }

        // serialise without allocating memory as we already have our scratch buf sized.
        ser.Serialise("SubresourceContents", scratchBuf, size, SerialiserFlags::NoFlags);

        if(IsReplayingAndReading() && !ser.IsErrored())
        {
          if(dim == 1)
            gl.glTextureSubImage1DEXT(tex, targets[trg], i, 0, w, fmt, type, scratchBuf);
          else if(dim == 2)
            gl.glTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, fmt, type, scratchBuf);
          else if(dim == 3)
            gl.glTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, fmt, type, scratchBuf);
        }
      }
    }

    FreeAlignedBuffer(scratchBuf);

    // restore pixel (un)packing state
    if(ser.IsWriting() || !IsStructuredExporting(m_State))
    {
      gl.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);
      gl.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);
      pack.Apply(&gl, false);
      unpack.Apply(&gl, false);
    }

    if(!IsStructuredExporting(m_State))
      gl.glBindTexture(textype, prevtex);

    SERIALISE_CHECK_READ_ERRORS();
  }

  return true;
}

INSTANTIATE_FUNCTION_SERIALISED(void, wglDXRegisterObjectNV, GLResource Resource, GLenum type,
                                void *dxObject);
INSTANTIATE_FUNCTION_SERIALISED(void, wglDXLockObjectsNV, GLResource Resource);