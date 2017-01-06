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
#include "driver/gl/gl_common.h"
#include "driver/gl/gl_driver.h"
#include "driver/gl/gl_hookset.h"
#include "driver/gl/gl_hookset_defs.h"
#include "hooks/hooks.h"

class OpenGLHook : LibraryHook
{
public:
  OpenGLHook() {}
  ~OpenGLHook() {}
  bool CreateHooks(const char *libName) { return false; }
  void EnableHooks(const char *libName, bool enable) {}
  void OptionsUpdated(const char *libName) {}
};

const GLHookSet &GetRealGLFunctions()
{
  static GLHookSet dummyHookset = {};
  RDCUNIMPLEMENTED("GetRealGLFunctions");
  return dummyHookset;
}

void MakeContextCurrent(GLWindowingData data)
{
  RDCUNIMPLEMENTED("MakeContextCurrent");
}

GLWindowingData MakeContext(GLWindowingData share)
{
  RDCUNIMPLEMENTED("MakeContext");
  return GLWindowingData();
}

void DeleteContext(GLWindowingData context)
{
  RDCUNIMPLEMENTED("DeleteContext");
}

bool immediateBegin(GLenum mode, float width, float height)
{
  RDCUNIMPLEMENTED("immediateBegin");
  return false;
}

void immediateVert(float x, float y, float u, float v)
{
  RDCUNIMPLEMENTED("immediateVert");
}

void immediateEnd()
{
  RDCUNIMPLEMENTED("immediateEnd");
}
