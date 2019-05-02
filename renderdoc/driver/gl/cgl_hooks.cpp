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

#include <dlfcn.h>
#include "hooks/hooks.h"
#include "cgl_dispatch_table.h"
#include "gl_driver.h"

class CGLHook : LibraryHook
{
public:
  CGLHook() : driver(GetGLPlatform()) {}
  void RegisterHooks();

  // default to RTLD_NEXT for CGL lookups if we haven't gotten a more specific library handle
  void *handle = RTLD_NEXT;
  WrappedOpenGL driver;
  std::set<CGLContextObj> contexts;

  volatile int32_t suppressed = 0;
} cglhook;

CGLError GL_EXPORT_NAME(CGLCreateContext)(CGLPixelFormatObj pix, CGLContextObj share,
                                          CGLContextObj *ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!CGL.CGLCreateContext)
      CGL.PopulateForReplay();

    return CGL.CGLCreateContext(pix, share, ctx);
  }

  CGLError ret = CGL.CGLCreateContext(pix, share, ctx);

  if(ret != kCGLNoError)
    return ret;

  GLInitParams init = {};

  init.width = 0;
  init.height = 0;

  GLint value = 0;

  CGLError err = kCGLNoError;

  CGL.CGLDescribePixelFormat(pix, 0, kCGLPFAColorSize, &value);
  init.colorBits = (uint32_t)value;
  CGL.CGLDescribePixelFormat(pix, 0, kCGLPFADepthSize, &value);
  init.depthBits = (uint32_t)value;
  CGL.CGLDescribePixelFormat(pix, 0, kCGLPFAStencilSize, &value);
  init.stencilBits = (uint32_t)value;
  // TODO: is macOS sRGB?
  init.isSRGB = 1;
  CGL.CGLDescribePixelFormat(pix, 0, kCGLPFASamples, &value);
  init.multiSamples = RDCMAX(1, value);

  CGL.CGLDescribePixelFormat(pix, 0, kCGLPFAOpenGLProfile, &value);
  bool isCore = (value >= kCGLOGLPVersion_3_2_Core);

  GLWindowingData data;
  data.wnd = NULL;
  data.ctx = *ctx;
  data.pix = pix;

  {
    SCOPED_LOCK(glLock);
    cglhook.driver.CreateContext(data, share, init, isCore, isCore);
  }

  return ret;
}

CGLError GL_EXPORT_NAME(CGLSetCurrentContext)(CGLContextObj ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!CGL.CGLSetCurrentContext)
      CGL.PopulateForReplay();

    return CGL.CGLSetCurrentContext(ctx);
  }

  CGLError ret = CGL.CGLSetCurrentContext(ctx);

  if(Atomic::CmpExch32(&cglhook.suppressed, 0, 0) != 0)
    return ret;

  if(ret == kCGLNoError)
  {
    SCOPED_LOCK(glLock);

    SetDriverForHooks(&cglhook.driver);

    if(ctx && cglhook.contexts.find(ctx) == cglhook.contexts.end())
    {
      cglhook.contexts.insert(ctx);

      FetchEnabledExtensions();

      // see gl_emulated.cpp
      GL.EmulateUnsupportedFunctions();
      GL.EmulateRequiredExtensions();
      GL.DriverForEmulation(&cglhook.driver);
    }

    CGRect rect = {};

    GLWindowingData data;
    data.wnd = NULL;
    data.ctx = ctx;
    data.pix = CGLGetPixelFormat(ctx);

    if(data.ctx)
    {
      CGSConnectionID conn = 0;
      CGSWindowID window = 0;
      CGSSurfaceID surface = 0;

      CGL.CGLGetSurface(ctx, &conn, &window, &surface);

      data.wnd = (void *)(uintptr_t)window;

      CGL.CGSGetSurfaceBounds(conn, window, surface, &rect);
    }

    cglhook.driver.ActivateContext(data);

    if(data.ctx)
    {
      GLInitParams &params = cglhook.driver.GetInitParams(data);
      params.width = (uint32_t)rect.size.width;
      params.height = (uint32_t)rect.size.height;
    }
  }

  return ret;
}

CGLError GL_EXPORT_NAME(CGLFlushDrawable)(CGLContextObj ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!CGL.CGLFlushDrawable)
      CGL.PopulateForReplay();

    return CGL.CGLFlushDrawable(ctx);
  }

  {
    SCOPED_LOCK(glLock);

    CGSConnectionID conn = 0;
    CGSWindowID window = 0;
    CGSSurfaceID surface = 0;

    CGL.CGLGetSurface(ctx, &conn, &window, &surface);

    cglhook.driver.SwapBuffers((void *)(uintptr_t)window);
  }

  CGLError ret;
  {
    DisableGLHooks();
    Atomic::Inc32(&cglhook.suppressed);
    ret = CGL.CGLFlushDrawable(ctx);
    Atomic::Dec32(&cglhook.suppressed);
    EnableGLHooks();
  }

  return ret;
}

DECL_HOOK_EXPORT(CGLCreateContext);
DECL_HOOK_EXPORT(CGLSetCurrentContext);
DECL_HOOK_EXPORT(CGLFlushDrawable);

static void CGLHooked(void *handle)
{
  RDCDEBUG("CGL library hooked");

  // store the handle for any pass-through implementations that need to look up their onward
  // pointers
  cglhook.handle = handle;

  // enable hooks immediately, we'll suppress them when calling into CGL
  EnableGLHooks();

  // as a hook callback this is only called while capturing
  RDCASSERT(!RenderDoc::Inst().IsReplayApp());

// fetch non-hooked functions into our dispatch table
#define CGL_FETCH(func) CGL.func = &func;
  CGL_NONHOOKED_SYMBOLS(CGL_FETCH)
#undef CGL_FETCH
}

void CGLHook::RegisterHooks()
{
  RDCLOG("Registering CGL hooks");

  // register library hooks
  LibraryHooks::RegisterLibraryHook(
      "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", &CGLHooked);
  LibraryHooks::RegisterLibraryHook("libGL.dylib", NULL);

// register CGL hooks
#define CGL_REGISTER(func)            \
  LibraryHooks::RegisterFunctionHook( \
      "OpenGL", FunctionHook(STRINGIZE(func), (void **)&CGL.func, (void *)&GL_EXPORT_NAME(func)));
  CGL_HOOKED_SYMBOLS(CGL_REGISTER)
#undef CGL_REGISTER
}
