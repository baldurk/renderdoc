/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "gles_replay.h"
#include "gles_driver.h"
#include "gles_resources.h"

void GLESReplay::MakeCurrentReplayContext(GLESWindowingData *ctx)
{
  RDCUNIMPLEMENTED("GLESReplay::MakeCurrentReplayContext");
}

void GLESReplay::SwapBuffers(GLESWindowingData *ctx)
{
  RDCUNIMPLEMENTED("GLESReplay::SwapBuffers");
}

void GLESReplay::CloseReplayContext()
{
  RDCUNIMPLEMENTED("GLESReplay::CloseReplayContext");
}

uint64_t GLESReplay::MakeOutputWindow(void *wn, bool depth)
{
  RDCUNIMPLEMENTED("GLESReplay::MakeOutputWindow");
  return 0;
}

void GLESReplay::DestroyOutputWindow(uint64_t id)
{
  RDCUNIMPLEMENTED("GLESReplay::DestroyOutputWindow");
}

void GLESReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  RDCUNIMPLEMENTED("GLESReplay::GetOutputWindowDimensions");
}

bool GLESReplay::IsOutputWindowVisible(uint64_t id)
{
  RDCUNIMPLEMENTED("GLESReplay::IsOutputWindowVisible");
  return false;
}

ReplayCreateStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCUNIMPLEMENTED("GL_CreateReplayDevice");
  return eReplayCreate_APIHardwareUnsupported;
}
