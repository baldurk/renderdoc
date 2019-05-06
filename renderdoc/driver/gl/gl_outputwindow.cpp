/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "gl_driver.h"
#include "gl_replay.h"

void GLReplay::CreateOutputWindowBackbuffer(OutputWindow &outwin, bool depth)
{
  if(m_pDriver == NULL)
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &drv = *m_pDriver;

  // create fake backbuffer for this output window.
  // We'll make an FBO for this backbuffer on the replay context, so we can
  // use the replay context to do the hard work of rendering to it, then just
  // blit across to the real default framebuffer on the output window context
  drv.glGenFramebuffers(1, &outwin.BlitData.windowFBO);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, outwin.BlitData.windowFBO);

  drv.glGenTextures(1, &outwin.BlitData.backbuffer);
  drv.glBindTexture(eGL_TEXTURE_2D, outwin.BlitData.backbuffer);

  drv.glTextureImage2DEXT(outwin.BlitData.backbuffer, eGL_TEXTURE_2D, 0, eGL_SRGB8_ALPHA8,
                          outwin.width, outwin.height, 0, eGL_RGBA, eGL_UNSIGNED_BYTE, NULL);
  drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
  drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                             outwin.BlitData.backbuffer, 0);

  if(depth)
  {
    drv.glGenTextures(1, &outwin.BlitData.depthstencil);
    drv.glBindTexture(eGL_TEXTURE_2D, outwin.BlitData.depthstencil);

    drv.glTextureImage2DEXT(outwin.BlitData.depthstencil, eGL_TEXTURE_2D, 0, eGL_DEPTH_COMPONENT24,
                            outwin.width, outwin.height, 0, eGL_DEPTH_COMPONENT, eGL_UNSIGNED_INT,
                            NULL);
    drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  }
  else
  {
    outwin.BlitData.depthstencil = 0;
  }

  outwin.BlitData.replayFBO = 0;
}

void GLReplay::InitOutputWindow(OutputWindow &outwin)
{
  if(m_pDriver == NULL)
    return;

  MakeCurrentReplayContext(&outwin);

  WrappedOpenGL &drv = *m_pDriver;

  drv.glGenVertexArrays(1, &outwin.BlitData.emptyVAO);
  drv.glBindVertexArray(outwin.BlitData.emptyVAO);

  drv.glGenFramebuffers(1, &outwin.BlitData.readFBO);
  drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, outwin.BlitData.readFBO);
  drv.glReadBuffer(eGL_COLOR_ATTACHMENT0);
}

bool GLReplay::CheckResizeOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.ctx == NULL || outw.system == WindowingSystem::Headless)
    return false;

  int32_t w, h;
  GetOutputWindowDimensions(id, w, h);

  if(w != outw.width || h != outw.height)
  {
    outw.width = w;
    outw.height = h;

    MakeCurrentReplayContext(&outw);

    m_pDriver->m_Platform.WindowResized(outw);

    MakeCurrentReplayContext(m_DebugCtx);

    WrappedOpenGL &drv = *m_pDriver;

    bool haddepth = false;

    drv.glDeleteTextures(1, &outw.BlitData.backbuffer);
    if(outw.BlitData.depthstencil)
    {
      haddepth = true;
      drv.glDeleteTextures(1, &outw.BlitData.depthstencil);
    }
    drv.glDeleteFramebuffers(1, &outw.BlitData.windowFBO);

    CreateOutputWindowBackbuffer(outw, haddepth);

    return true;
  }

  return false;
}

void GLReplay::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glBindFramebuffer(eGL_FRAMEBUFFER, outw.BlitData.windowFBO);
  m_pDriver->glViewport(0, 0, outw.width, outw.height);

  m_pDriver->glFramebufferTexture2D(
      eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_TEXTURE_2D,
      depth && outw.BlitData.depthstencil ? outw.BlitData.depthstencil : 0, 0);

  DebugData.outWidth = float(outw.width);
  DebugData.outHeight = float(outw.height);
}

void GLReplay::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glClearBufferfv(eGL_COLOR, 0, &col.x);
}

void GLReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glClearBufferfi(eGL_DEPTH_STENCIL, 0, depth, (GLint)stencil);
}

void GLReplay::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.system == WindowingSystem::Headless)
    return;

  MakeCurrentReplayContext(&outw);

  WrappedOpenGL &drv = *m_pDriver;

  // go directly to real function so we don't try to bind the 'fake' backbuffer FBO.
  GL.glBindFramebuffer(eGL_FRAMEBUFFER, 0);
  drv.glViewport(0, 0, outw.width, outw.height);

  drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, outw.BlitData.readFBO);

  drv.glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                             outw.BlitData.backbuffer, 0);
  drv.glReadBuffer(eGL_COLOR_ATTACHMENT0);

  if(HasExt[EXT_framebuffer_sRGB])
    drv.glEnable(eGL_FRAMEBUFFER_SRGB);

  drv.glBlitFramebuffer(0, 0, outw.width, outw.height, 0, 0, outw.width, outw.height,
                        GL_COLOR_BUFFER_BIT, eGL_NEAREST);

  SwapBuffers(&outw);
}

uint64_t GLReplay::MakeOutputWindow(WindowingData window, bool depth)
{
  OutputWindow win = m_pDriver->m_Platform.MakeOutputWindow(window, depth, m_ReplayCtx);
  if(!win.ctx)
    return 0;

  win.system = window.system;

  if(window.system == WindowingSystem::Headless)
  {
    win.width = window.headless.width;
    win.height = window.headless.height;
  }
  else
  {
    m_pDriver->m_Platform.GetOutputWindowDimensions(win, win.width, win.height);
  }

  MakeCurrentReplayContext(&win);

  m_pDriver->RegisterReplayContext(win, m_ReplayCtx.ctx, true, true);

  InitOutputWindow(win);
  CreateOutputWindowBackbuffer(win, depth);

  uint64_t ret = m_OutputWindowID++;

  m_OutputWindows[ret] = win;

  return ret;
}

void GLReplay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  MakeCurrentReplayContext(&outw);

  m_pDriver->glDeleteFramebuffers(1, &outw.BlitData.readFBO);

  m_pDriver->m_Platform.DeleteReplayContext(outw);

  m_OutputWindows.erase(it);
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.system == WindowingSystem::Headless)
  {
    w = outw.width;
    h = outw.height;
    return;
  }

  m_pDriver->m_Platform.GetOutputWindowDimensions(outw, w, h);
}

void GLReplay::SetOutputWindowDimensions(uint64_t id, int32_t w, int32_t h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  // can't resize an output with an actual window backing
  if(outw.system != WindowingSystem::Headless)
    return;

  outw.width = w;
  outw.height = h;

  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &drv = *m_pDriver;

  bool haddepth = false;

  drv.glDeleteTextures(1, &outw.BlitData.backbuffer);
  if(outw.BlitData.depthstencil)
  {
    haddepth = true;
    drv.glDeleteTextures(1, &outw.BlitData.depthstencil);
  }
  drv.glDeleteFramebuffers(1, &outw.BlitData.windowFBO);

  CreateOutputWindowBackbuffer(outw, haddepth);
}

void GLReplay::GetOutputWindowData(uint64_t id, bytebuf &retData)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, outw.BlitData.windowFBO);
  m_pDriver->glReadBuffer(eGL_BACK);
  m_pDriver->glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
  m_pDriver->glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
  m_pDriver->glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
  m_pDriver->glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
  m_pDriver->glPixelStorei(eGL_PACK_ALIGNMENT, 1);

  // read as RGBA for maximum compatibility.
  retData.resize(outw.width * outw.height * 4);
  GL.glReadPixels(0, 0, outw.width, outw.height, eGL_RGBA, eGL_UNSIGNED_BYTE, retData.data());

  // y-flip
  for(int32_t row = 0; row < outw.height / 2; row++)
  {
    const uint32_t stride = outw.width * 4;
    const int32_t fliprow = outw.height - 1 - row;

    for(int32_t x = 0; x < outw.width; x++)
    {
      std::swap(retData[row * stride + x * 4 + 0], retData[fliprow * stride + x * 4 + 0]);
      std::swap(retData[row * stride + x * 4 + 1], retData[fliprow * stride + x * 4 + 1]);
      std::swap(retData[row * stride + x * 4 + 2], retData[fliprow * stride + x * 4 + 2]);
      std::swap(retData[row * stride + x * 4 + 3], retData[fliprow * stride + x * 4 + 3]);
    }
  }

  // compact from RGBA to RGB.
  byte *src = retData.data();
  byte *dst = retData.data();
  for(int32_t row = 0; row < outw.height; row++)
  {
    for(int32_t x = 0; x < outw.width; x++)
    {
      memcpy(dst, src, 3);
      dst += 3;
      src += 4;
    }
  }

  retData.resize(outw.width * outw.height * 3);
}

bool GLReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  if(m_OutputWindows[id].system == WindowingSystem::Headless)
    return true;

  return m_pDriver->m_Platform.IsOutputWindowVisible(m_OutputWindows[id]);
}
