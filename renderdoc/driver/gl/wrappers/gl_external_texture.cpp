/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2022 Baldur Karlsson
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

#if defined(RENDERDOC_SUPPORT_EGL)
#include "../egl_dispatch_table.h"
#endif
#include "../gl_driver.h"

rdcarray<byte> WrappedOpenGL::GetExternalTextureData(GLuint texture)
{
  rdcarray<byte> pixels;
  GLuint prevTex = 0;
  GL.glGetIntegerv(eGL_TEXTURE_BINDING_EXTERNAL_OES, (GLint *)&prevTex);
  GL.glBindTexture(eGL_TEXTURE_EXTERNAL_OES, texture);

  GLint width = 0, height = 0;
  GLenum internalFormat = eGL_NONE;
  GL.glGetTexLevelParameteriv(eGL_TEXTURE_EXTERNAL_OES, 0, eGL_TEXTURE_WIDTH, &width);
  GL.glGetTexLevelParameteriv(eGL_TEXTURE_EXTERNAL_OES, 0, eGL_TEXTURE_HEIGHT, &height);
  GL.glGetTexLevelParameteriv(eGL_TEXTURE_EXTERNAL_OES, 0, eGL_TEXTURE_INTERNAL_FORMAT,
                              (GLint *)&internalFormat);
  GL.glBindTexture(eGL_TEXTURE_EXTERNAL_OES, prevTex);

  size_t size = GetByteSize(width, height, 1, GetBaseFormat(internalFormat), eGL_UNSIGNED_BYTE);

  pixels.resize(size);

  // read pixels. ref: https://developer.arm.com/documentation/ka004859/1-0
  GLuint prevReadFramebuffer = 0, prevPixelPackBuffer = 0, fb = 0;
  GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&prevPixelPackBuffer);
  GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
  GL.glGenFramebuffers(1, &fb);
  GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&prevReadFramebuffer);
  GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, fb);
  GL.glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_EXTERNAL_OES,
                            texture, 0);

  GLint prevPixelPackAlign = 0;
  GL.glGetIntegerv(eGL_PACK_ALIGNMENT, &prevPixelPackAlign);
  GL.glPixelStorei(eGL_PACK_ALIGNMENT, 1);
  GL.glReadPixels(0, 0, width, height, GetBaseFormat(internalFormat), eGL_UNSIGNED_BYTE,
                  pixels.data());
  GL.glFinish();
  GL.glPixelStorei(eGL_PACK_ALIGNMENT, prevPixelPackAlign);
  GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevReadFramebuffer);
  GL.glDeleteFramebuffers(1, &fb);
  GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, prevPixelPackBuffer);

  return pixels;
}

GLeglImageOES WrappedOpenGL::CreateEGLImage(GLint width, GLint height, GLenum internalFormat,
                                            const byte *pixels, uint64_t size)
{
  GLeglImageOES image = NULL;

#if defined(RENDERDOC_PLATFORM_ANDROID)
  uint32_t bufferFormat = 0;
  switch(internalFormat)
  {
    case eGL_RGB8: bufferFormat = AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM; break;
    case eGL_RGBA8: bufferFormat = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM; break;
    default: RDCERR("Unsupported internal format 0x%X", internalFormat);
  }
  AHardwareBuffer *hardwareBuffer = NULL;
  EGLClientBuffer clientBuffer = NULL;

  AHardwareBuffer_Desc hardwareBufferDesc{};
  hardwareBufferDesc.width = width;
  hardwareBufferDesc.height = height;
  hardwareBufferDesc.format = bufferFormat;
  hardwareBufferDesc.layers = 1;
  hardwareBufferDesc.usage =
      AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

  int res = AHardwareBuffer_allocate(&hardwareBufferDesc, &hardwareBuffer);
  RDCASSERT(res == 0);
  clientBuffer = EGL.GetNativeClientBufferANDROID(hardwareBuffer);
  RDCASSERT(clientBuffer);
  image = EGL.CreateImageKHR(EGL.GetCurrentDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                             clientBuffer, NULL);
  RDCASSERT(image != EGL_NO_IMAGE_KHR);

  m_ExternalTextureResources.push_back({image, hardwareBuffer});

  // fill data
  {
    RDCASSERT(hardwareBuffer);
    AHardwareBuffer_describe(hardwareBuffer, &hardwareBufferDesc);
    byte *pwrite = NULL;
    int res = AHardwareBuffer_lock(hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, NULL,
                                   (void **)&pwrite);
    RDCASSERT(res == 0);
    if(hardwareBufferDesc.stride == hardwareBufferDesc.width)
    {
      memcpy(pwrite, pixels, size);    // copy at once
    }
    else    // copy row by row
    {
      uint32_t pixelSize;
      switch(hardwareBufferDesc.format)
      {
        case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM: pixelSize = 3; break;
        case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
        case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM: pixelSize = 4; break;
        default:
          pixelSize = 0;
          hardwareBufferDesc.height = 0;    // to prevent copying
          RDCERR("Unknown or unsupported hardware buffer format 0x%X", hardwareBufferDesc.format);
      }
      hardwareBufferDesc.width *= pixelSize;     // hw_buf_desc.width = src row size in bytes
      hardwareBufferDesc.stride *= pixelSize;    // hw_buf_desc.stride = dst row size in bytes
      for(uint32_t h = 0; h < hardwareBufferDesc.height; ++h)
      {
        memcpy(pwrite, pixels, hardwareBufferDesc.width);
        pixels += hardwareBufferDesc.width;
        pwrite += hardwareBufferDesc.stride;
      }
    }
    res = AHardwareBuffer_unlock(hardwareBuffer, NULL);
    RDCASSERT(res == 0);
  }
#endif
  return image;
}

void WrappedOpenGL::ReleaseExternalTextureResources()
{
  for(rdcpair<GLeglImageOES, struct AHardwareBuffer *> &etr : m_ExternalTextureResources)
  {
    GLeglImageOES image = etr.first;
#if defined(RENDERDOC_SUPPORT_EGL)
    if(image && EGL.DestroyImageKHR)
    {
      EGL.DestroyImageKHR(eglGetCurrentDisplay(), image);
    }
#endif
#if defined(RENDERDOC_PLATFORM_ANDROID)
    AHardwareBuffer *hardwareBuffer = etr.second;
    if(hardwareBuffer)
    {
      AHardwareBuffer_release(hardwareBuffer);
    }
#endif
  }
  m_ExternalTextureResources.clear();
}
