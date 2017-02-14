/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "gl_replay.h"
#include <dlfcn.h>
#include "gl_driver.h"
#include "gl_resources.h"

void GLReplay::MakeCurrentReplayContext(GLWindowingData *ctx)
{
  // TODO
}

void GLReplay::SwapBuffers(GLWindowingData *ctx)
{
  // TODO
}

void GLReplay::CloseReplayContext()
{
  // TODO
}

uint64_t GLReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  // TODO
  uint64_t ret = m_OutputWindowID++;

  return ret;
}

void GLReplay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  MakeCurrentReplayContext(&outw);

  WrappedOpenGL &gl = *m_pDriver;
  gl.glDeleteFramebuffers(1, &outw.BlitData.readFBO);

  m_OutputWindows.erase(it);

  // TODO
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  // TODO
}

bool GLReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  GLNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

const GLHookSet &GetRealGLFunctionsEGL();

ReplayCreateStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL ES replay device");

  // TODO
  return eReplayCreate_APIInitFailed;
}
