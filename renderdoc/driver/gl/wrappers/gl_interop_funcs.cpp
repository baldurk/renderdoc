/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
  return GL.wglDXSetResourceShareHandleNV(dxObject, shareHandle);
}

HANDLE WrappedOpenGL::wglDXOpenDeviceNV(void *dxDevice)
{
  void *unwrapped = UnwrapDXDevice(dxDevice);
  if(unwrapped)
  {
    HANDLE ret = (HANDLE)GL.wglDXOpenDeviceNV(unwrapped);

    SetLastError(S_OK);

    return ret;
  }

  SetLastError(ERROR_NOT_SUPPORTED);

  return 0;
}

BOOL WrappedOpenGL::wglDXCloseDeviceNV(HANDLE hDevice)
{
  return GL.wglDXCloseDeviceNV(hDevice);
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

  SERIALISE_TIME_CALL(wrapped->real = GL.wglDXRegisterObjectNV(hDevice, real, name, type, access));

  if(wrapped->real == NULL)
  {
    delete wrapped;
    return NULL;
  }

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

    GL.glTextureParameteriEXT(wrapped->res.name, type, eGL_TEXTURE_MAX_LEVEL, GLint(mips - 1));

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
    m_Textures[texId].mipsValid = (1 << mips) - 1;
  }

  return wrapped;
}

BOOL WrappedOpenGL::wglDXUnregisterObjectNV(HANDLE hDevice, HANDLE hObject)
{
  // don't need to intercept this, as the DX and GL textures will be deleted independently
  BOOL ret = GL.wglDXUnregisterObjectNV(hDevice, Unwrap(hObject));

  delete(WrappedHANDLE *)hObject;

  return ret;
}

BOOL WrappedOpenGL::wglDXObjectAccessNV(HANDLE hObject, GLenum access)
{
  // we don't need to care about access
  return GL.wglDXObjectAccessNV(Unwrap(hObject), access);
}

BOOL WrappedOpenGL::wglDXLockObjectsNV(HANDLE hDevice, GLint count, HANDLE *hObjects)
{
  HANDLE *unwrapped = new HANDLE[count];
  for(GLint i = 0; i < count; i++)
    unwrapped[i] = Unwrap(hObjects[i]);

  BOOL ret;
  SERIALISE_TIME_CALL(ret = GL.wglDXLockObjectsNV(hDevice, count, unwrapped));

  if(IsActiveCapturing(m_State))
  {
    for(GLint i = 0; i < count; i++)
    {
      WrappedHANDLE *w = (WrappedHANDLE *)hObjects[i];

      USE_SCRATCH_SERIALISER();
      SCOPED_SERIALISE_CHUNK(gl_CurChunk);
      Serialise_wglDXLockObjectsNV(ser, w->res);

      GetContextRecord()->AddChunk(scope.Get());
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
  BOOL ret = GL.wglDXUnlockObjectsNV(hDevice, count, unwrapped);
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
        GL.glNamedBufferDataEXT(name, width, NULL, eGL_STATIC_DRAW);
        break;
      }
      case eGL_TEXTURE_1D: GL.glTextureStorage1DEXT(name, type, mips, internalFormat, width); break;
      case eGL_TEXTURE_1D_ARRAY:
        GL.glTextureStorage2DEXT(name, type, mips, internalFormat, width, layers);
        break;
      // treat renderbuffers and texture rects as tex2D just to make things easier
      case eGL_RENDERBUFFER:
      case eGL_TEXTURE_RECTANGLE:
      case eGL_TEXTURE_2D:
      case eGL_TEXTURE_CUBE_MAP:
        GL.glTextureStorage2DEXT(name, type, mips, internalFormat, width, height);
        break;
      case eGL_TEXTURE_2D_ARRAY:
      case eGL_TEXTURE_CUBE_MAP_ARRAY:
        GL.glTextureStorage3DEXT(name, type, mips, internalFormat, width, height, layers);
        break;
      case eGL_TEXTURE_2D_MULTISAMPLE:
        GL.glTextureStorage2DMultisampleEXT(name, type, samples, internalFormat, width, height,
                                            GL_TRUE);
        break;
      case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        GL.glTextureStorage3DMultisampleEXT(name, type, samples, internalFormat, width, height,
                                            layers, GL_TRUE);
        break;
      case eGL_TEXTURE_3D:
        GL.glTextureStorage3DEXT(name, type, mips, internalFormat, width, height, depth);
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
      m_Textures[liveId].mipsValid = (1 << mips) - 1;
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

  // buffer contents are easier to save
  if(textype == eGL_NONE)
  {
    byte *Contents = NULL;
    uint32_t length = 1;

    // while writing, fetch the buffer's size and contents
    if(ser.IsWriting())
    {
      GL.glGetNamedBufferParameterivEXT(Resource.name, eGL_BUFFER_SIZE, (GLint *)&length);

      Contents = new byte[length];

      GLuint oldbuf = 0;
      GL.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf);
      GL.glBindBuffer(eGL_COPY_READ_BUFFER, Resource.name);

      GL.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, (GLsizeiptr)length, Contents);

      GL.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf);
    }

    SERIALISE_ELEMENT_ARRAY(Contents, length);
    SERIALISE_ELEMENT(length);

    SERIALISE_CHECK_READ_ERRORS();

    // restore on replay
    if(IsReplayingAndReading())
    {
      uint32_t liveLength = 1;
      GL.glGetNamedBufferParameterivEXT(Resource.name, eGL_BUFFER_SIZE, (GLint *)&liveLength);

      GL.glNamedBufferSubData(Resource.name, 0, (GLsizeiptr)RDCMIN(length, liveLength), Contents);
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
      GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&ppb);
      GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pub);
      GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

      pack.Fetch(false);
      unpack.Fetch(false);

      ResetPixelPackState(false, 1);
      ResetPixelUnpackState(false, 1);
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
      mips = GetNumMips(textype, tex, width, height, depth);

    SERIALISE_ELEMENT(mips).Hidden();

    byte *scratchBuf = NULL;

    // on read and write, we allocate a single buffer big enough for all mips and re-use it
    // to avoid repeated new/free.
    scratchBuf = AllocAlignedBuffer(size);

    GLuint prevtex = 0;
    if(!IsStructuredExporting(m_State))
    {
      GL.glGetIntegerv(TextureBinding(details.curType), (GLint *)&prevtex);
      GL.glBindTexture(textype, tex);
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
          GL.glGetTexImage(targets[trg], i, fmt, type, scratchBuf);
        }

        // serialise without allocating memory as we already have our scratch buf sized.
        ser.Serialise("SubresourceContents"_lit, scratchBuf, size, SerialiserFlags::NoFlags);

        if(IsReplayingAndReading() && !ser.IsErrored())
        {
          if(dim == 1)
            GL.glTextureSubImage1DEXT(tex, targets[trg], i, 0, w, fmt, type, scratchBuf);
          else if(dim == 2)
            GL.glTextureSubImage2DEXT(tex, targets[trg], i, 0, 0, w, h, fmt, type, scratchBuf);
          else if(dim == 3)
            GL.glTextureSubImage3DEXT(tex, targets[trg], i, 0, 0, 0, w, h, d, fmt, type, scratchBuf);
        }
      }
    }

    FreeAlignedBuffer(scratchBuf);

    // restore pixel (un)packing state
    if(ser.IsWriting() || !IsStructuredExporting(m_State))
    {
      GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, ppb);
      GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pub);
      pack.Apply(false);
      unpack.Apply(false);
    }

    if(!IsStructuredExporting(m_State))
      GL.glBindTexture(textype, prevtex);

    SERIALISE_CHECK_READ_ERRORS();
  }

  return true;
}

#pragma region Memory Objects

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glCreateMemoryObjectsEXT(SerialiserType &ser, GLsizei n,
                                                       GLuint *memoryObjects)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(memory, GetResourceManager()->GetID(ExtMemRes(GetCtx(), *memoryObjects)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glCreateMemoryObjectsEXT);

    GLuint real = 0;
    GL.glCreateMemoryObjectsEXT(1, &real);

    GLResource res = ExtMemRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(memory, res);

    AddResource(memory, ResourceType::Memory, "Memory Object");
  }

  return true;
}

void WrappedOpenGL::glCreateMemoryObjectsEXT(GLsizei n, GLuint *memoryObjects)
{
  SERIALISE_TIME_CALL(GL.glCreateMemoryObjectsEXT(n, memoryObjects));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ExtMemRes(GetCtx(), memoryObjects[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glCreateMemoryObjectsEXT(ser, 1, memoryObjects + i);

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

void WrappedOpenGL::glDeleteMemoryObjectsEXT(GLsizei n, const GLuint *memoryObjects)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ExtMemRes(GetCtx(), memoryObjects[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteMemoryObjectsEXT(n, memoryObjects);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glMemoryObjectParameterivEXT(SerialiserType &ser,
                                                           GLuint memoryObjectHandle, GLenum pname,
                                                           const GLint *params)
{
  SERIALISE_ELEMENT_LOCAL(memoryObject, ExtMemRes(GetCtx(), memoryObjectHandle));
  SERIALISE_ELEMENT(pname);
  // if other parameters are added in future that take more than one value, change the array count
  // here.
  SERIALISE_ELEMENT_ARRAY(params, 1U);

  SERIALISE_CHECK_READ_ERRORS();

  RDCASSERT(pname == eGL_DEDICATED_MEMORY_OBJECT_EXT || pname == eGL_PROTECTED_MEMORY_OBJECT_EXT);

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glMemoryObjectParameterivEXT);

    GL.glMemoryObjectParameterivEXT(memoryObject.name, pname, params);

    AddResourceInitChunk(memoryObject);
  }

  return true;
}

void WrappedOpenGL::glMemoryObjectParameterivEXT(GLuint memoryObject, GLenum pname,
                                                 const GLint *params)
{
  SERIALISE_TIME_CALL(GL.glMemoryObjectParameterivEXT(memoryObject, pname, params));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ExtMemRes(GetCtx(), memoryObject));

    if(!record)
    {
      RDCERR("Called glMemoryObjectParameterivEXT with invalid/unrecognised memory object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glMemoryObjectParameterivEXT(ser, memoryObject, pname, params);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
    }
    else
    {
      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glImportMemoryFdEXT(SerialiserType &ser, GLuint memoryHandle,
                                                  GLuint64 size, GLenum handleType, GLint fd)
{
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(handleType);
  SERIALISE_ELEMENT(fd);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay external memory we just allocate textures/buffers with their
    // own backing store. Keep this around for tracking purposes

    AddResourceInitChunk(memory);
  }

  return true;
}

void WrappedOpenGL::glImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd)
{
  SERIALISE_TIME_CALL(GL.glImportMemoryFdEXT(memory, size, handleType, fd));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ExtMemRes(GetCtx(), memory));

    if(!record)
    {
      RDCERR("Called glImportMemoryFdEXT with invalid/unrecognised memory object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glImportMemoryFdEXT(ser, memory, size, handleType, fd);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glImportMemoryWin32HandleEXT(SerialiserType &ser, GLuint memoryHandle,
                                                           GLuint64 size, GLenum handleType,
                                                           void *handlePtr)
{
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(handleType);
  SERIALISE_ELEMENT_LOCAL(handle, uint64_t(handlePtr));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay external memory we just allocate textures/buffers with their
    // own backing store. Keep this around for tracking purposes

    AddResourceInitChunk(memory);
  }

  return true;
}

void WrappedOpenGL::glImportMemoryWin32HandleEXT(GLuint memory, GLuint64 size, GLenum handleType,
                                                 void *handle)
{
  SERIALISE_TIME_CALL(GL.glImportMemoryWin32HandleEXT(memory, size, handleType, handle));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ExtMemRes(GetCtx(), memory));

    if(!record)
    {
      RDCERR("Called glImportMemoryWin32HandleEXT with invalid/unrecognised memory object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glImportMemoryWin32HandleEXT(ser, memory, size, handleType, handle);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glImportMemoryWin32NameEXT(SerialiserType &ser, GLuint memoryHandle,
                                                         GLuint64 size, GLenum handleType,
                                                         const void *namePtr)
{
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(size);
  SERIALISE_ELEMENT(handleType);
  SERIALISE_ELEMENT_LOCAL(name, StringFormat::Wide2UTF8((const wchar_t *)namePtr));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay external memory we just allocate textures/buffers with their
    // own backing store. Keep this around for tracking purposes

    AddResourceInitChunk(memory);
  }

  return true;
}

void WrappedOpenGL::glImportMemoryWin32NameEXT(GLuint memory, GLuint64 size, GLenum handleType,
                                               const void *name)
{
  SERIALISE_TIME_CALL(GL.glImportMemoryWin32NameEXT(memory, size, handleType, name));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(ExtMemRes(GetCtx(), memory));

    if(!record)
    {
      RDCERR("Called glImportMemoryWin32NameEXT with invalid/unrecognised memory object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glImportMemoryWin32NameEXT(ser, memory, size, handleType, name);

    record->AddChunk(scope.Get());
  }
}

#pragma endregion

#pragma region Semaphores

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glGenSemaphoresEXT(SerialiserType &ser, GLsizei n, GLuint *semaphores)
{
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT_LOCAL(semaphore, GetResourceManager()->GetID(ExtSemRes(GetCtx(), *semaphores)))
      .TypedAs("GLResource"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glGenSemaphoresEXT);

    GLuint real = 0;
    GL.glGenSemaphoresEXT(1, &real);

    GLResource res = ExtSemRes(GetCtx(), real);

    ResourceId live = m_ResourceManager->RegisterResource(res);
    GetResourceManager()->AddLiveResource(semaphore, res);

    AddResource(semaphore, ResourceType::Sync, "Semaphore");
  }

  return true;
}

void WrappedOpenGL::glGenSemaphoresEXT(GLsizei n, GLuint *semaphores)
{
  SERIALISE_TIME_CALL(GL.glGenSemaphoresEXT(n, semaphores));

  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ExtSemRes(GetCtx(), semaphores[i]);
    ResourceId id = GetResourceManager()->RegisterResource(res);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        USE_SCRATCH_SERIALISER();
        SCOPED_SERIALISE_CHUNK(gl_CurChunk);
        Serialise_glGenSemaphoresEXT(ser, 1, semaphores + i);

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

void WrappedOpenGL::glDeleteSemaphoresEXT(GLsizei n, const GLuint *semaphores)
{
  for(GLsizei i = 0; i < n; i++)
  {
    GLResource res = ExtSemRes(GetCtx(), semaphores[i]);
    if(GetResourceManager()->HasCurrentResource(res))
    {
      if(GetResourceManager()->HasResourceRecord(res))
        GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
      GetResourceManager()->UnregisterResource(res);
    }
  }

  GL.glDeleteSemaphoresEXT(n, semaphores);
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSemaphoreParameterui64vEXT(SerialiserType &ser,
                                                           GLuint semaphoreHandle, GLenum pname,
                                                           const GLuint64 *params)
{
  SERIALISE_ELEMENT_LOCAL(semaphore, ExtSemRes(GetCtx(), semaphoreHandle));
  SERIALISE_ELEMENT(pname);
  // if other parameters are added in future that take more than one value, change the array count
  // here.
  SERIALISE_ELEMENT_ARRAY(params, 1U);

  SERIALISE_CHECK_READ_ERRORS();

  RDCASSERT(pname == eGL_D3D12_FENCE_VALUE_EXT);

  if(IsReplayingAndReading())
  {
    CheckReplayFunctionPresent(GL.glSemaphoreParameterui64vEXT);

    GL.glSemaphoreParameterui64vEXT(semaphore.name, pname, params);

    AddResourceInitChunk(semaphore);
  }

  return true;
}

void WrappedOpenGL::glSemaphoreParameterui64vEXT(GLuint semaphore, GLenum pname,
                                                 const GLuint64 *params)
{
  SERIALISE_TIME_CALL(GL.glSemaphoreParameterui64vEXT(semaphore, pname, params));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ExtSemRes(GetCtx(), semaphore));

    if(!record)
    {
      RDCERR("Called glSemaphoreParameterui64vEXT with invalid/unrecognised semaphore");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSemaphoreParameterui64vEXT(ser, semaphore, pname, params);

    if(IsActiveCapturing(m_State))
    {
      GetContextRecord()->AddChunk(scope.Get());
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
    }
    else
    {
      record->AddChunk(scope.Get());
    }
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glImportSemaphoreFdEXT(SerialiserType &ser, GLuint semaphoreHandle,
                                                     GLenum handleType, GLint fd)
{
  SERIALISE_ELEMENT_LOCAL(semaphore, ExtSemRes(GetCtx(), semaphoreHandle));
  SERIALISE_ELEMENT(handleType);
  SERIALISE_ELEMENT(fd);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay semaphores we just fully glFinish() when we need to wait on
    // them (just in case).

    AddResourceInitChunk(semaphore);
  }

  return true;
}

void WrappedOpenGL::glImportSemaphoreFdEXT(GLuint semaphore, GLenum handleType, GLint fd)
{
  SERIALISE_TIME_CALL(GL.glImportSemaphoreFdEXT(semaphore, handleType, fd));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ExtSemRes(GetCtx(), semaphore));

    if(!record)
    {
      RDCERR("Called glImportSemaphoreFdEXT with invalid/unrecognised semaphore object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glImportSemaphoreFdEXT(ser, semaphore, handleType, fd);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glImportSemaphoreWin32HandleEXT(SerialiserType &ser,
                                                              GLuint semaphoreHandle,
                                                              GLenum handleType, void *handlePtr)
{
  SERIALISE_ELEMENT_LOCAL(semaphore, ExtSemRes(GetCtx(), semaphoreHandle));
  SERIALISE_ELEMENT(handleType);
  SERIALISE_ELEMENT_LOCAL(handle, uint64_t(handlePtr));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay semaphores we just fully glFinish() when we need to wait on
    // them (just in case).

    AddResourceInitChunk(semaphore);
  }

  return true;
}

void WrappedOpenGL::glImportSemaphoreWin32HandleEXT(GLuint semaphore, GLenum handleType, void *handle)
{
  SERIALISE_TIME_CALL(GL.glImportSemaphoreWin32HandleEXT(semaphore, handleType, handle));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ExtSemRes(GetCtx(), semaphore));

    if(!record)
    {
      RDCERR("Called glImportSemaphoreWin32HandleEXT with invalid/unrecognised semaphore object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glImportSemaphoreWin32HandleEXT(ser, semaphore, handleType, handle);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glImportSemaphoreWin32NameEXT(SerialiserType &ser,
                                                            GLuint semaphoreHandle,
                                                            GLenum handleType, const void *namePtr)
{
  SERIALISE_ELEMENT_LOCAL(semaphore, ExtSemRes(GetCtx(), semaphoreHandle));
  SERIALISE_ELEMENT(handleType);
  SERIALISE_ELEMENT_LOCAL(name, StringFormat::Wide2UTF8((const wchar_t *)namePtr));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay semaphores we just fully glFinish() when we need to wait on
    // them (just in case).

    AddResourceInitChunk(semaphore);
  }

  return true;
}

void WrappedOpenGL::glImportSemaphoreWin32NameEXT(GLuint semaphore, GLenum handleType,
                                                  const void *name)
{
  SERIALISE_TIME_CALL(GL.glImportSemaphoreWin32NameEXT(semaphore, handleType, name));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record =
        GetResourceManager()->GetResourceRecord(ExtSemRes(GetCtx(), semaphore));

    if(!record)
    {
      RDCERR("Called glImportSemaphoreWin32NameEXT with invalid/unrecognised semaphore object");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glImportSemaphoreWin32NameEXT(ser, semaphore, handleType, name);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glWaitSemaphoreEXT(SerialiserType &ser, GLuint semaphoreHandle,
                                                 GLuint numBufferBarriers,
                                                 const GLuint *bufferHandles,
                                                 GLuint numTextureBarriers,
                                                 const GLuint *textureHandles,
                                                 const GLenum *srcLayouts)
{
  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  std::vector<GLResource> buffers, textures;

  if(ser.IsWriting())
  {
    buffers.reserve(numBufferBarriers);
    for(GLuint i = 0; i < numBufferBarriers; i++)
      buffers.push_back(BufferRes(GetCtx(), bufferHandles ? bufferHandles[i] : 0));

    textures.reserve(numTextureBarriers);
    for(GLuint i = 0; i < numTextureBarriers; i++)
      textures.push_back(TextureRes(GetCtx(), textureHandles ? textureHandles[i] : 0));
  }

  SERIALISE_ELEMENT_LOCAL(semaphore, ExtSemRes(GetCtx(), semaphoreHandle));
  SERIALISE_ELEMENT(numBufferBarriers);
  SERIALISE_ELEMENT(buffers);
  SERIALISE_ELEMENT(numTextureBarriers);
  SERIALISE_ELEMENT(textures);
  SERIALISE_ELEMENT_ARRAY(srcLayouts, numTextureBarriers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay semaphores we just fully glFinish() when we need to wait on
    // them (just in case).

    GL.glFinish();
  }

  return true;
}

void WrappedOpenGL::glWaitSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers,
                                       const GLuint *buffers, GLuint numTextureBarriers,
                                       const GLuint *textures, const GLenum *srcLayouts)
{
  SERIALISE_TIME_CALL(GL.glWaitSemaphoreEXT(semaphore, numBufferBarriers, buffers,
                                            numTextureBarriers, textures, srcLayouts));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glWaitSemaphoreEXT(ser, semaphore, numBufferBarriers, buffers, numTextureBarriers,
                                 textures, srcLayouts);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(ExtSemRes(GetCtx(), semaphore), eFrameRef_Read);

    for(GLuint b = 0; buffers && b < numBufferBarriers; b++)
      GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffers[b]),
                                                        eFrameRef_Read);

    for(GLuint t = 0; textures && t < numTextureBarriers; t++)
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), textures[t]),
                                                        eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glSignalSemaphoreEXT(SerialiserType &ser, GLuint semaphoreHandle,
                                                   GLuint numBufferBarriers,
                                                   const GLuint *bufferHandles,
                                                   GLuint numTextureBarriers,
                                                   const GLuint *textureHandles,
                                                   const GLenum *dstLayouts)
{
  // can't serialise arrays of GL handles since they're not wrapped or typed :(.
  std::vector<GLResource> buffers, textures;

  if(ser.IsWriting())
  {
    buffers.reserve(numBufferBarriers);
    for(GLuint i = 0; i < numBufferBarriers; i++)
      buffers.push_back(BufferRes(GetCtx(), bufferHandles ? bufferHandles[i] : 0));

    textures.reserve(numTextureBarriers);
    for(GLuint i = 0; i < numTextureBarriers; i++)
      textures.push_back(TextureRes(GetCtx(), textureHandles ? textureHandles[i] : 0));
  }

  SERIALISE_ELEMENT_LOCAL(semaphore, ExtSemRes(GetCtx(), semaphoreHandle));
  SERIALISE_ELEMENT(numBufferBarriers);
  SERIALISE_ELEMENT(buffers);
  SERIALISE_ELEMENT(numTextureBarriers);
  SERIALISE_ELEMENT(textures);
  SERIALISE_ELEMENT_ARRAY(dstLayouts, numTextureBarriers);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay semaphores we just fully glFinish() when we need to wait on
    // them (just in case).
  }

  return true;
}

void WrappedOpenGL::glSignalSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers,
                                         const GLuint *buffers, GLuint numTextureBarriers,
                                         const GLuint *textures, const GLenum *dstLayouts)
{
  SERIALISE_TIME_CALL(GL.glSignalSemaphoreEXT(semaphore, numBufferBarriers, buffers,
                                              numTextureBarriers, textures, dstLayouts));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glSignalSemaphoreEXT(ser, semaphore, numBufferBarriers, buffers, numTextureBarriers,
                                   textures, dstLayouts);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(ExtSemRes(GetCtx(), semaphore), eFrameRef_Read);

    for(GLuint b = 0; buffers && b < numBufferBarriers; b++)
      GetResourceManager()->MarkResourceFrameReferenced(BufferRes(GetCtx(), buffers[b]),
                                                        eFrameRef_Read);

    for(GLuint t = 0; textures && t < numTextureBarriers; t++)
      GetResourceManager()->MarkResourceFrameReferenced(TextureRes(GetCtx(), textures[t]),
                                                        eFrameRef_Read);
  }
}

#pragma endregion

#pragma region Keyed Mutexes

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glAcquireKeyedMutexWin32EXT(SerialiserType &ser, GLuint memoryHandle,
                                                          GLuint64 key, GLuint timeout)
{
  SERIALISE_ELEMENT_LOCAL(memory, ExtSemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(key);
  SERIALISE_ELEMENT(timeout);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay keyed mutexes as we don't create external memory
  }

  return true;
}

GLboolean WrappedOpenGL::glAcquireKeyedMutexWin32EXT(GLuint memory, GLuint64 key, GLuint timeout)
{
  GLboolean ret;
  SERIALISE_TIME_CALL(ret = GL.glAcquireKeyedMutexWin32EXT(memory, key, timeout));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glAcquireKeyedMutexWin32EXT(ser, memory, key, timeout);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(ExtMemRes(GetCtx(), memory), eFrameRef_Read);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glReleaseKeyedMutexWin32EXT(SerialiserType &ser, GLuint memoryHandle,
                                                          GLuint64 key)
{
  SERIALISE_ELEMENT_LOCAL(memory, ExtSemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(key);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // nothing to do - we don't replay keyed mutexes as we don't create external memory
  }

  return true;
}

GLboolean WrappedOpenGL::glReleaseKeyedMutexWin32EXT(GLuint memory, GLuint64 key)
{
  GLboolean ret;
  SERIALISE_TIME_CALL(ret = GL.glReleaseKeyedMutexWin32EXT(memory, key));

  if(IsActiveCapturing(m_State))
  {
    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glReleaseKeyedMutexWin32EXT(ser, memory, key);

    GetContextRecord()->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(ExtMemRes(GetCtx(), memory), eFrameRef_Read);
  }

  return ret;
}

#pragma endregion

#pragma region Buffers

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glNamedBufferStorageMemEXT(SerialiserType &ser, GLuint bufferHandle,
                                                         GLsizeiptr sizeptr, GLuint memoryHandle,
                                                         GLuint64 offset)
{
  SERIALISE_ELEMENT_LOCAL(buffer, BufferRes(GetCtx(), bufferHandle));
  SERIALISE_ELEMENT_LOCAL(size, (uint64_t)sizeptr);
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Replay external buffer storage backed by external memory as just a plain buffer.

    // we have to come up with flags that will work regardless, so we're conservative here.
    // The spec says memory object backed buffers can't be mapped, but we set DYNAMIC_STORAGE as
    // it's unclear if they can be updated with glBufferSubData or not.
    GLbitfield flags = eGL_DYNAMIC_STORAGE_BIT;

    GL.glNamedBufferStorageEXT(buffer.name, (GLsizeiptr)size, NULL, flags);

    ResourceId id = GetResourceManager()->GetID(buffer);

    m_Buffers[id].size = size;

    AddResourceInitChunk(buffer);
    DerivedResource(memory, GetResourceManager()->GetOriginalID(id));
  }

  return true;
}

void WrappedOpenGL::glNamedBufferStorageMemEXT(GLuint buffer, GLsizeiptr size, GLuint memory,
                                               GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glNamedBufferStorageMemEXT(buffer, size, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord =
        GetResourceManager()->GetResourceRecord(BufferRes(GetCtx(), buffer));
    GLResourceRecord *memrecord =
        GetResourceManager()->GetResourceRecord(ExtMemRes(GetCtx(), memory));

    if(!bufrecord)
    {
      RDCERR("Called glNamedBufferStorageMemEXT with invalid buffer");
      return;
    }

    if(!memrecord)
    {
      RDCERR("Called glNamedBufferStorageMemEXT with invalid memory object");
      return;
    }

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(bufrecord->Resource);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferStorageMemEXT(ser, buffer, size, memory, offset);

    bufrecord->AddChunk(scope.Get());
    bufrecord->AddParent(memrecord);
    bufrecord->Length = (int32_t)size;
  }
}

void WrappedOpenGL::glBufferStorageMemEXT(GLenum target, GLsizeiptr size, GLuint memory,
                                          GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glBufferStorageMemEXT(target, size, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *bufrecord = GetCtxData().m_BufferRecord[BufferIdx(target)];
    GLResourceRecord *memrecord =
        GetResourceManager()->GetResourceRecord(ExtMemRes(GetCtx(), memory));

    if(!bufrecord)
    {
      RDCERR("Called glBufferStorageMemEXT with no buffer bound to %s", ToStr(target).c_str());
      return;
    }

    if(!memrecord)
    {
      RDCERR("Called glNamedBufferStorageMemEXT with invalid memory object");
      return;
    }

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(bufrecord->Resource);

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glNamedBufferStorageMemEXT(ser, bufrecord->Resource.name, size, memory, offset);

    bufrecord->AddChunk(scope.Get());
    bufrecord->AddParent(memrecord);
    bufrecord->Length = (int32_t)size;
  }
}

#pragma endregion

#pragma region Textures

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorageMem1DEXT(SerialiserType &ser, GLuint textureHandle,
                                                       GLsizei levels, GLenum internalFormat,
                                                       GLsizei width, GLuint memoryHandle,
                                                       GLuint64 offset)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(levels);
  SERIALISE_ELEMENT(internalFormat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Replay external texture storage backed by external memory as just a plain texture.
    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = 1;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].dimension = 1;
    m_Textures[liveId].internalFormat = internalFormat;
    m_Textures[liveId].emulated = false;
    m_Textures[liveId].mipsValid = (1 << levels) - 1;

    GL.glTextureStorage1DEXT(texture.name, m_Textures[liveId].curType, levels, internalFormat, width);

    AddResourceInitChunk(texture);
    DerivedResource(memory, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::glTextureStorageMem1DEXT(GLuint texture, GLsizei levels, GLenum internalFormat,
                                             GLsizei width, GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(
      GL.glTextureStorageMem1DEXT(texture, levels, internalFormat, width, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem1DEXT with unrecognised texture");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem1DEXT(ser, texture, levels, internalFormat, width, memory, offset);

    record->AddChunk(scope.Get());

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

void WrappedOpenGL::glTexStorageMem1DEXT(GLenum target, GLsizei levels, GLenum internalFormat,
                                         GLsizei width, GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTexStorageMem1DEXT(target, levels, internalFormat, width, memory, offset));

  if(IsCaptureMode(m_State) && !IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem1DEXT with no texture bound");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem1DEXT(ser, record->Resource.name, levels, internalFormat, width,
                                       memory, offset);

    record->AddChunk(scope.Get());

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = 1;
    m_Textures[texId].depth = 1;
    m_Textures[texId].dimension = 1;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorageMem2DEXT(SerialiserType &ser, GLuint textureHandle,
                                                       GLsizei levels, GLenum internalFormat,
                                                       GLsizei width, GLsizei height,
                                                       GLuint memoryHandle, GLuint64 offset)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(levels);
  SERIALISE_ELEMENT(internalFormat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Replay external texture storage backed by external memory as just a plain texture.
    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalFormat;
    m_Textures[liveId].emulated = false;
    m_Textures[liveId].mipsValid = (1 << levels) - 1;

    GL.glTextureStorage2DEXT(texture.name, m_Textures[liveId].curType, levels, internalFormat,
                             width, height);

    AddResourceInitChunk(texture);
    DerivedResource(memory, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::glTextureStorageMem2DEXT(GLuint texture, GLsizei levels, GLenum internalFormat,
                                             GLsizei width, GLsizei height, GLuint memory,
                                             GLuint64 offset)
{
  SERIALISE_TIME_CALL(
      GL.glTextureStorageMem2DEXT(texture, levels, internalFormat, width, height, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem2DEXT with unrecognised texture");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem2DEXT(ser, texture, levels, internalFormat, width, height, memory,
                                       offset);

    record->AddChunk(scope.Get());

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

void WrappedOpenGL::glTexStorageMem2DEXT(GLenum target, GLsizei levels, GLenum internalFormat,
                                         GLsizei width, GLsizei height, GLuint memory,
                                         GLuint64 offset)
{
  SERIALISE_TIME_CALL(
      GL.glTexStorageMem2DEXT(target, levels, internalFormat, width, height, memory, offset));

  if(IsCaptureMode(m_State) && !IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem2DEXT with no texture bound");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem2DEXT(ser, record->Resource.name, levels, internalFormat, width,
                                       height, memory, offset);

    record->AddChunk(scope.Get());

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = 1;
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorageMem2DMultisampleEXT(
    SerialiserType &ser, GLuint textureHandle, GLsizei samples, GLenum internalFormat, GLsizei width,
    GLsizei height, GLboolean fixedSampleLocations, GLuint memoryHandle, GLuint64 offset)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(internalFormat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT_TYPED(bool, fixedSampleLocations);
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // the DSA function is emulated if not present, but we need to check the underlying function is
    // present
    CheckReplayFunctionPresent(GL.glTexStorage2DMultisample);

    // Replay external texture storage backed by external memory as just a plain texture.
    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = 1;
    m_Textures[liveId].samples = samples;
    m_Textures[liveId].dimension = 2;
    m_Textures[liveId].internalFormat = internalFormat;
    m_Textures[liveId].emulated = false;
    m_Textures[liveId].mipsValid = 1;

    GL.glTextureStorage2DMultisampleEXT(texture.name, m_Textures[liveId].curType, samples,
                                        internalFormat, width, height, fixedSampleLocations);

    AddResourceInitChunk(texture);
    DerivedResource(memory, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::glTextureStorageMem2DMultisampleEXT(GLuint texture, GLsizei samples,
                                                        GLenum internalFormat, GLsizei width,
                                                        GLsizei height,
                                                        GLboolean fixedSampleLocations,
                                                        GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTextureStorageMem2DMultisampleEXT(
      texture, samples, internalFormat, width, height, fixedSampleLocations, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem2DMultisampleEXT with unrecognised texture");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem2DMultisampleEXT(ser, texture, samples, internalFormat, width,
                                                  height, fixedSampleLocations, memory, offset);

    record->AddChunk(scope.Get());

    // when bound to external memory, immediately consider dirty
    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].samples = samples;
    m_Textures[texId].depth = 1;
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = 1;
  }
}

void WrappedOpenGL::glTexStorageMem2DMultisampleEXT(GLenum target, GLsizei samples,
                                                    GLenum internalFormat, GLsizei width,
                                                    GLsizei height, GLboolean fixedSampleLocations,
                                                    GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTexStorageMem2DMultisampleEXT(
      target, samples, internalFormat, width, height, fixedSampleLocations, memory, offset));

  if(IsCaptureMode(m_State) && !IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);

    if(!record)
    {
      RDCERR("Calling glTexStorageMem2DMultisampleEXT with no texture bound");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem2DMultisampleEXT(ser, record->Resource.name, samples,
                                                  internalFormat, width, height,
                                                  fixedSampleLocations, memory, offset);

    record->AddChunk(scope.Get());

    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].samples = samples;
    m_Textures[texId].depth = 1;
    m_Textures[texId].dimension = 2;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = 1;
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorageMem3DEXT(SerialiserType &ser, GLuint textureHandle,
                                                       GLsizei levels, GLenum internalFormat,
                                                       GLsizei width, GLsizei height, GLsizei depth,
                                                       GLuint memoryHandle, GLuint64 offset)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(levels);
  SERIALISE_ELEMENT(internalFormat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Replay external texture storage backed by external memory as just a plain texture.
    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = depth;
    m_Textures[liveId].dimension = 3;
    m_Textures[liveId].internalFormat = internalFormat;
    m_Textures[liveId].emulated = false;
    m_Textures[liveId].mipsValid = (1 << levels) - 1;

    GL.glTextureStorage3DEXT(texture.name, m_Textures[liveId].curType, levels, internalFormat,
                             width, height, depth);

    AddResourceInitChunk(texture);
    DerivedResource(memory, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::glTextureStorageMem3DEXT(GLuint texture, GLsizei levels, GLenum internalFormat,
                                             GLsizei width, GLsizei height, GLsizei depth,
                                             GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTextureStorageMem3DEXT(texture, levels, internalFormat, width, height,
                                                  depth, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem3DEXT with unrecognised texture");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem3DEXT(ser, texture, levels, internalFormat, width, height, depth,
                                       memory, offset);

    record->AddChunk(scope.Get());

    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

void WrappedOpenGL::glTexStorageMem3DEXT(GLenum target, GLsizei levels, GLenum internalFormat,
                                         GLsizei width, GLsizei height, GLsizei depth,
                                         GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTexStorageMem3DEXT(target, levels, internalFormat, width, height, depth,
                                              memory, offset));

  if(IsCaptureMode(m_State) && !IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem3DEXT with no texture bound");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem3DEXT(ser, record->Resource.name, levels, internalFormat, width,
                                       height, depth, memory, offset);

    record->AddChunk(scope.Get());

    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].depth = depth;
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = (1 << levels) - 1;
  }
}

template <typename SerialiserType>
bool WrappedOpenGL::Serialise_glTextureStorageMem3DMultisampleEXT(
    SerialiserType &ser, GLuint textureHandle, GLsizei samples, GLenum internalFormat,
    GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations,
    GLuint memoryHandle, GLuint64 offset)
{
  SERIALISE_ELEMENT_LOCAL(texture, TextureRes(GetCtx(), textureHandle));
  SERIALISE_ELEMENT(samples);
  SERIALISE_ELEMENT(internalFormat);
  SERIALISE_ELEMENT(width);
  SERIALISE_ELEMENT(height);
  SERIALISE_ELEMENT(depth);
  SERIALISE_ELEMENT_TYPED(bool, fixedSampleLocations);
  SERIALISE_ELEMENT_LOCAL(memory, ExtMemRes(GetCtx(), memoryHandle));
  SERIALISE_ELEMENT(offset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // Replay external texture storage backed by external memory as just a plain texture.
    ResourceId liveId = GetResourceManager()->GetID(texture);
    m_Textures[liveId].width = width;
    m_Textures[liveId].height = height;
    m_Textures[liveId].depth = depth;
    m_Textures[liveId].samples = samples;
    m_Textures[liveId].dimension = 3;
    m_Textures[liveId].internalFormat = internalFormat;
    m_Textures[liveId].emulated = false;
    m_Textures[liveId].mipsValid = 1;

    GL.glTextureStorage3DMultisampleEXT(texture.name, m_Textures[liveId].curType, samples,
                                        internalFormat, width, height, depth, fixedSampleLocations);

    AddResourceInitChunk(texture);
    DerivedResource(memory, GetResourceManager()->GetOriginalID(liveId));
  }

  return true;
}

void WrappedOpenGL::glTextureStorageMem3DMultisampleEXT(GLuint texture, GLsizei samples,
                                                        GLenum internalFormat, GLsizei width,
                                                        GLsizei height, GLsizei depth,
                                                        GLboolean fixedSampleLocations,
                                                        GLuint memory, GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTextureStorageMem3DMultisampleEXT(
      texture, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset));

  if(IsCaptureMode(m_State))
  {
    GLResourceRecord *record = GetResourceManager()->GetResourceRecord(TextureRes(GetCtx(), texture));

    if(!record)
    {
      RDCERR("Calling glTextureStorageMem3DMultisampleEXT with unrecognised texture");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem3DMultisampleEXT(ser, texture, samples, internalFormat, width,
                                                  height, depth, fixedSampleLocations, memory,
                                                  offset);

    record->AddChunk(scope.Get());

    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].samples = samples;
    m_Textures[texId].depth = depth;
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = 1;
  }
}

void WrappedOpenGL::glTexStorageMem3DMultisampleEXT(GLenum target, GLsizei samples,
                                                    GLenum internalFormat, GLsizei width,
                                                    GLsizei height, GLsizei depth,
                                                    GLboolean fixedSampleLocations, GLuint memory,
                                                    GLuint64 offset)
{
  SERIALISE_TIME_CALL(GL.glTexStorageMem3DMultisampleEXT(
      target, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset));

  if(IsCaptureMode(m_State) && !IsProxyTarget(target))
  {
    GLResourceRecord *record = GetCtxData().GetActiveTexRecord(target);

    if(!record)
    {
      RDCERR("Calling glTexStorageMem3DMultisampleEXT with no texture bound");
      return;
    }

    USE_SCRATCH_SERIALISER();
    SCOPED_SERIALISE_CHUNK(gl_CurChunk);
    Serialise_glTextureStorageMem3DMultisampleEXT(ser, record->Resource.name, samples,
                                                  internalFormat, width, height, depth,
                                                  fixedSampleLocations, memory, offset);

    record->AddChunk(scope.Get());

    GetResourceManager()->MarkDirtyResource(record->Resource);

    ResourceId texId = record->GetResourceID();

    m_Textures[texId].width = width;
    m_Textures[texId].height = height;
    m_Textures[texId].samples = samples;
    m_Textures[texId].depth = depth;
    m_Textures[texId].dimension = 3;
    m_Textures[texId].internalFormat = internalFormat;
    m_Textures[texId].mipsValid = 1;
  }
}

#pragma endregion

INSTANTIATE_FUNCTION_SERIALISED(void, wglDXRegisterObjectNV, GLResource Resource, GLenum type,
                                void *dxObject);
INSTANTIATE_FUNCTION_SERIALISED(void, wglDXLockObjectsNV, GLResource Resource);
INSTANTIATE_FUNCTION_SERIALISED(void, glCreateMemoryObjectsEXT, GLsizei n, GLuint *memoryObjects);
INSTANTIATE_FUNCTION_SERIALISED(void, glMemoryObjectParameterivEXT, GLuint memoryObject,
                                GLenum pname, const GLint *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorageMem1DEXT, GLuint texture, GLsizei levels,
                                GLenum internalFormat, GLsizei width, GLuint memory, GLuint64 offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorageMem2DEXT, GLuint texture, GLsizei levels,
                                GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory,
                                GLuint64 offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorageMem2DMultisampleEXT, GLuint texture,
                                GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height,
                                GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorageMem3DEXT, GLuint texture, GLsizei levels,
                                GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                                GLuint memory, GLuint64 offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glTextureStorageMem3DMultisampleEXT, GLuint texture,
                                GLsizei samples, GLenum internalFormat, GLsizei width,
                                GLsizei height, GLsizei depth, GLboolean fixedSampleLocations,
                                GLuint memory, GLuint64 offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glNamedBufferStorageMemEXT, GLuint buffer, GLsizeiptr size,
                                GLuint memory, GLuint64 offset);
INSTANTIATE_FUNCTION_SERIALISED(void, glGenSemaphoresEXT, GLsizei n, GLuint *semaphores);
INSTANTIATE_FUNCTION_SERIALISED(void, glSemaphoreParameterui64vEXT, GLuint semaphore, GLenum pname,
                                const GLuint64 *params);
INSTANTIATE_FUNCTION_SERIALISED(void, glWaitSemaphoreEXT, GLuint semaphore, GLuint numBufferBarriers,
                                const GLuint *buffers, GLuint numTextureBarriers,
                                const GLuint *textures, const GLenum *srcLayouts);
INSTANTIATE_FUNCTION_SERIALISED(void, glSignalSemaphoreEXT, GLuint semaphore,
                                GLuint numBufferBarriers, const GLuint *buffers,
                                GLuint numTextureBarriers, const GLuint *textures,
                                const GLenum *dstLayouts);
INSTANTIATE_FUNCTION_SERIALISED(void, glImportMemoryFdEXT, GLuint memory, GLuint64 size,
                                GLenum handleType, GLint fd);
INSTANTIATE_FUNCTION_SERIALISED(void, glImportSemaphoreFdEXT, GLuint semaphore, GLenum handleType,
                                GLint fd);
INSTANTIATE_FUNCTION_SERIALISED(void, glImportMemoryWin32HandleEXT, GLuint memory, GLuint64 size,
                                GLenum handleType, void *handle);
INSTANTIATE_FUNCTION_SERIALISED(void, glImportMemoryWin32NameEXT, GLuint memory, GLuint64 size,
                                GLenum handleType, const void *name);
INSTANTIATE_FUNCTION_SERIALISED(void, glImportSemaphoreWin32HandleEXT, GLuint semaphore,
                                GLenum handleType, void *handle);
INSTANTIATE_FUNCTION_SERIALISED(void, glImportSemaphoreWin32NameEXT, GLuint semaphore,
                                GLenum handleType, const void *name);
INSTANTIATE_FUNCTION_SERIALISED(GLboolean, glAcquireKeyedMutexWin32EXT, GLuint memory, GLuint64 key,
                                GLuint timeout);
INSTANTIATE_FUNCTION_SERIALISED(GLboolean, glReleaseKeyedMutexWin32EXT, GLuint memory, GLuint64 key);