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

#include <dlfcn.h>
#include <stdio.h>

#include "hooks/hooks.h"

#include "driver/gl/gl_common.h"
#include "driver/gl/gl_driver.h"
#include "driver/gl/gl_hookset.h"

#include "driver/gl/gl_hookset_defs.h"

Threading::CriticalSection glLock;

class OpenGLHook : LibraryHook
{
public:
  OpenGLHook() {}
  ~OpenGLHook() {}
  bool CreateHooks(const char *libName) { return false; }
  void EnableHooks(const char *libName, bool enable) {}
  void OptionsUpdated(const char *libName) {}
  virtual GLWindowingData MakeContext(GLWindowingData share)
  {
    RDCUNIMPLEMENTED("MakeContext");
    return GLWindowingData();
  }
  virtual void DeleteContext(GLWindowingData context) { RDCUNIMPLEMENTED("DeleteContext"); }
  virtual void DeleteReplayContext(GLWindowingData context)
  {
    RDCUNIMPLEMENTED("DeleteReplayContext");
  }
  virtual void MakeContextCurrent(GLWindowingData data) { RDCUNIMPLEMENTED("MakeContextCurrent"); }
  virtual void SwapBuffers(GLWindowingData context) { RDCUNIMPLEMENTED("SwapBuffers"); }
  virtual void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    RDCUNIMPLEMENTED("GetOutputWindowDimensions");
  }
  virtual bool IsOutputWindowVisible(GLWindowingData context)
  {
    RDCUNIMPLEMENTED("IsOutputWindowVisible");
    return true;
  }
  virtual GLWindowingData MakeOutputWindow(WindowingSystem system, void *data, bool depth,
                                           GLWindowingData share_context)
  {
    RDCUNIMPLEMENTED("MakeOutputWindow");
    return GLWindowingData();
  }

  virtual bool DrawQuads(float width, float height, const std::vector<Vec4f> &vertices)
  {
    RDCUNIMPLEMENTED("DrawQuads");
    return false;
  }
};

const GLHookSet &GetRealGLFunctions()
{
  static GLHookSet dummyHookset = {};
  RDCUNIMPLEMENTED("GetRealGLFunctions");
  return dummyHookset;
}

Threading::CriticalSection &GetGLLock()
{
  return glLock;
}
