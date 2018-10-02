/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include "cgl_dispatch_table.h"
#include "gl_common.h"

class CGLPlatform : public GLPlatform
{
  bool MakeContextCurrent(GLWindowingData data) { return false; }
  GLWindowingData CloneTemporaryContext(GLWindowingData share)
  {
    GLWindowingData ret;
    return ret;
  }

  void DeleteClonedContext(GLWindowingData context) {}
  void DeleteReplayContext(GLWindowingData context) {}
  void SwapBuffers(GLWindowingData context) {}
  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h) { w = h = 0; }
  bool IsOutputWindowVisible(GLWindowingData context) { return false; }
  GLWindowingData MakeOutputWindow(WindowingData window, bool depth, GLWindowingData share_context)
  {
    GLWindowingData ret = {};
    return ret;
  }

  void *GetReplayFunction(const char *funcname) { return NULL; }
  bool CanCreateGLESContext() { return false; }
  bool PopulateForReplay() { return false; }
  ReplayStatus InitialiseAPI(GLWindowingData &replayContext, RDCDriver api)
  {
    return ReplayStatus::APIUnsupported;
  }

  void DrawQuads(float width, float height, const std::vector<Vec4f> &vertices) {}
} cglPlatform;

CGLDispatchTable CGL = {};

GLPlatform &GetGLPlatform()
{
  return cglPlatform;
}

bool CGLDispatchTable::PopulateForReplay()
{
  return false;
}