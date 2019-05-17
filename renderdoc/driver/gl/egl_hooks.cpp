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

#include "driver/gl/egl_dispatch_table.h"
#include "driver/gl/gl_driver.h"
#include "hooks/hooks.h"

#if ENABLED(RDOC_POSIX)
#include <dlfcn.h>

// default to RTLD_NEXT for EGL lookups if we haven't gotten a more specific library handle
#define DEFAULT_HANDLE RTLD_NEXT
#else
#define DEFAULT_HANDLE NULL
#endif

#if ENABLED(RDOC_LINUX)
namespace Keyboard
{
void CloneDisplay(Display *dpy);
}
#endif

class EGLHook : LibraryHook
{
public:
  EGLHook() : driver(GetEGLPlatform()) {}
  void RegisterHooks();

  RDCDriver activeAPI = RDCDriver::OpenGLES;

  void *handle = DEFAULT_HANDLE;
  WrappedOpenGL driver;
  std::set<EGLContext> contexts;
  std::map<EGLContext, EGLConfig> configs;
  std::map<EGLSurface, EGLNativeWindowType> windows;

  // indicates we're in a swap function, so don't process the swap any further if we recurse - could
  // happen due to driver implementation of one function calling another
  bool swapping = false;

  bool IsYFlipped(EGLDisplay dpy, EGLSurface surface)
  {
    const char *extString = EGL.QueryString(dpy, EGL_EXTENSIONS);
    if(extString && strstr(extString, "ANGLE_surface_orientation"))
    {
// https://github.com/google/angle/blob/master/extensions/EGL_ANGLE_surface_orientation.txt
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002

      int mask = 0;
      EGL.QuerySurface(dpy, surface, EGL_SURFACE_ORIENTATION_ANGLE, &mask);

      return (mask & EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE) != 0;
    }

    return false;
  }

  void RefreshWindowParameters(const GLWindowingData &data)
  {
    EGLDisplay display = data.egl_dpy;
    EGLContext ctx = data.egl_ctx;
    EGLSurface draw = data.egl_wnd;

    if(ctx && draw)
    {
      GLInitParams &params = driver.GetInitParams(data);

      int height, width;
      EGL.QuerySurface(display, draw, EGL_HEIGHT, &height);
      EGL.QuerySurface(display, draw, EGL_WIDTH, &width);

      int colorspace = 0;
      EGL.QuerySurface(display, draw, EGL_GL_COLORSPACE, &colorspace);
      // GL_SRGB8_ALPHA8 is specified as color-renderable, unlike GL_SRGB8.
      bool isSRGB = params.colorBits == 32 && colorspace == EGL_GL_COLORSPACE_SRGB;

      bool isYFlipped = IsYFlipped(display, draw);

      params.width = width;
      params.height = height;
      params.isSRGB = isSRGB;
      params.isYFlipped = isYFlipped;
    }
  }

} eglhook;

HOOK_EXPORT EGLDisplay EGLAPIENTRY eglGetDisplay_renderdoc_hooked(EGLNativeDisplayType display)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.GetDisplay)
      EGL.PopulateForReplay();

    return EGL.GetDisplay(display);
  }

#if ENABLED(RDOC_LINUX)
  Keyboard::CloneDisplay(display);
#endif

  return EGL.GetDisplay(display);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglBindAPI_renderdoc_hooked(EGLenum api)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.GetDisplay)
      EGL.PopulateForReplay();

    return EGL.BindAPI(api);
  }

  EGLBoolean ret = EGL.BindAPI(api);

  if(ret)
    eglhook.activeAPI = api == EGL_OPENGL_API ? RDCDriver::OpenGL : RDCDriver::OpenGLES;

  return ret;
}

HOOK_EXPORT EGLContext EGLAPIENTRY eglCreateContext_renderdoc_hooked(EGLDisplay display,
                                                                     EGLConfig config,
                                                                     EGLContext shareContext,
                                                                     EGLint const *attribList)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.CreateContext)
      EGL.PopulateForReplay();

    return EGL.CreateContext(display, config, shareContext, attribList);
  }

  LibraryHooks::Refresh();

  std::vector<EGLint> attribs;

  // modify attribList to our liking
  {
    bool flagsFound = false;

    if(attribList)
    {
      const EGLint *ptr = attribList;

      for(;;)
      {
        EGLint name = *ptr++;

        if(name == EGL_NONE)
        {
          break;
        }

        EGLint value = *ptr++;

        if(name == EGL_CONTEXT_FLAGS_KHR)
        {
          if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
            value |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
          else
            value &= ~EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

          // remove NO_ERROR bit
          value &= ~GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR;

          flagsFound = true;
        }

        if(name == EGL_CONTEXT_OPENGL_NO_ERROR_KHR)
        {
          // remove this attribute so that we can be more stable
          continue;
        }

        attribs.push_back(name);
        attribs.push_back(value);
      }
    }

    if(!flagsFound && RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      attribs.push_back(EGL_CONTEXT_FLAGS_KHR);
      attribs.push_back(EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR);
    }

    attribs.push_back(EGL_NONE);
  }

  RDCDEBUG("eglCreateContext:");

  if(attribList)
  {
    int *a = (int *)attribs.data();
    while(*a != EGL_NONE)
    {
      RDCDEBUG("%x: %d", a[0], a[1]);
      a += 2;
    }
  }

  EGLContext ret = EGL.CreateContext(display, config, shareContext, attribs.data());

  // don't continue if context creation failed
  if(ret == EGL_NO_CONTEXT)
    return ret;

  GLInitParams init;

  init.width = 0;
  init.height = 0;

  EGLint value;
  EGL.GetConfigAttrib(display, config, EGL_BUFFER_SIZE, &value);
  init.colorBits = value;
  EGL.GetConfigAttrib(display, config, EGL_DEPTH_SIZE, &value);
  init.depthBits = value;
  EGL.GetConfigAttrib(display, config, EGL_STENCIL_SIZE, &value);
  init.stencilBits = value;
  // We will set isSRGB when we see the surface.
  init.isSRGB = 0;

  GLWindowingData data;
  data.egl_dpy = display;
  data.wnd = 0;
  data.egl_wnd = (EGLSurface)NULL;
  data.egl_ctx = ret;
  data.egl_cfg = config;

  eglhook.configs[ret] = config;

  EnableGLHooks();
  eglhook.driver.SetDriverType(eglhook.activeAPI);
  {
    SCOPED_LOCK(glLock);
    eglhook.driver.CreateContext(data, shareContext, init, true, true);
  }

  return ret;
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglDestroyContext_renderdoc_hooked(EGLDisplay dpy, EGLContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.DestroyContext)
      EGL.PopulateForReplay();

    return EGL.DestroyContext(dpy, ctx);
  }

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  {
    SCOPED_LOCK(glLock);
    eglhook.driver.DeleteContext(ctx);
    eglhook.contexts.erase(ctx);
  }

  return EGL.DestroyContext(dpy, ctx);
}

HOOK_EXPORT EGLSurface EGLAPIENTRY eglCreateWindowSurface_renderdoc_hooked(EGLDisplay dpy,
                                                                           EGLConfig config,
                                                                           EGLNativeWindowType win,
                                                                           const EGLint *attrib_list)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.CreateWindowSurface)
      EGL.PopulateForReplay();

    return EGL.CreateWindowSurface(dpy, config, win, attrib_list);
  }

  EGLSurface ret = EGL.CreateWindowSurface(dpy, config, win, attrib_list);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    eglhook.windows[ret] = win;
  }

  return ret;
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglMakeCurrent_renderdoc_hooked(EGLDisplay display,
                                                                   EGLSurface draw, EGLSurface read,
                                                                   EGLContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.MakeCurrent)
      EGL.PopulateForReplay();

    return EGL.MakeCurrent(display, draw, read, ctx);
  }

  EGLBoolean ret = EGL.MakeCurrent(display, draw, read, ctx);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    SetDriverForHooks(&eglhook.driver);

    if(ctx && eglhook.contexts.find(ctx) == eglhook.contexts.end())
    {
      eglhook.contexts.insert(ctx);

      FetchEnabledExtensions();

      // see gl_emulated.cpp
      GL.EmulateUnsupportedFunctions();
      GL.EmulateRequiredExtensions();
      GL.DriverForEmulation(&eglhook.driver);
    }

    GLWindowingData data;
    data.egl_dpy = display;
    data.egl_wnd = draw;
    data.egl_ctx = ctx;
    data.wnd = (decltype(data.wnd))eglhook.windows[draw];

    if(!data.wnd)
    {
      // could be a pbuffer surface or other offscreen rendering. We want a valid wnd, so set it to
      // a dummy value
      data.wnd = (decltype(data.wnd))(void *)(uintptr_t(0xdeadbeef) + uintptr_t(draw));
    }

    // we could query this out technically but it's easier to keep a map
    data.egl_cfg = eglhook.configs[ctx];

    eglhook.driver.SetDriverType(eglhook.activeAPI);

    eglhook.driver.ActivateContext(data);

    eglhook.RefreshWindowParameters(data);
  }

  return ret;
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglSwapBuffers_renderdoc_hooked(EGLDisplay dpy, EGLSurface surface)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.SwapBuffers)
      EGL.PopulateForReplay();

    return EGL.SwapBuffers(dpy, surface);
  }

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
  {
    GLWindowingData data;
    data.egl_dpy = dpy;
    data.egl_wnd = surface;
    data.egl_ctx = EGL.GetCurrentContext();

    eglhook.RefreshWindowParameters(data);

    eglhook.driver.SwapBuffers(surface);
  }

  {
    eglhook.swapping = true;
    EGLBoolean ret = EGL.SwapBuffers(dpy, surface);
    eglhook.swapping = false;
    return ret;
  }
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglPostSubBufferNV_renderdoc_hooked(EGLDisplay dpy,
                                                                       EGLSurface surface, EGLint x,
                                                                       EGLint y, EGLint width,
                                                                       EGLint height)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.PostSubBufferNV)
      EGL.PopulateForReplay();

    return EGL.PostSubBufferNV(dpy, surface, x, y, width, height);
  }

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
    eglhook.driver.SwapBuffers((void *)eglhook.windows[surface]);

  {
    eglhook.swapping = true;
    EGLBoolean ret = EGL.PostSubBufferNV(dpy, surface, x, y, width, height);
    eglhook.swapping = false;
    return ret;
  }
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglSwapBuffersWithDamageEXT_renderdoc_hooked(EGLDisplay dpy,
                                                                                EGLSurface surface,
                                                                                EGLint *rects,
                                                                                EGLint n_rects)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.SwapBuffersWithDamageEXT)
      EGL.PopulateForReplay();

    return EGL.SwapBuffersWithDamageEXT(dpy, surface, rects, n_rects);
  }

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
    eglhook.driver.SwapBuffers((void *)eglhook.windows[surface]);

  {
    eglhook.swapping = true;
    EGLBoolean ret = EGL.SwapBuffersWithDamageEXT(dpy, surface, rects, n_rects);
    eglhook.swapping = false;
    return ret;
  }
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglSwapBuffersWithDamageKHR_renderdoc_hooked(EGLDisplay dpy,
                                                                                EGLSurface surface,
                                                                                EGLint *rects,
                                                                                EGLint n_rects)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.SwapBuffersWithDamageKHR)
      EGL.PopulateForReplay();

    return EGL.SwapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
  }

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
    eglhook.driver.SwapBuffers((void *)eglhook.windows[surface]);

  {
    eglhook.swapping = true;
    EGLBoolean ret = EGL.SwapBuffersWithDamageKHR(dpy, surface, rects, n_rects);
    eglhook.swapping = false;
    return ret;
  }
}

HOOK_EXPORT __eglMustCastToProperFunctionPointerType EGLAPIENTRY
eglGetProcAddress_renderdoc_hooked(const char *func)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.GetProcAddress)
      EGL.PopulateForReplay();

    return EGL.GetProcAddress(func);
  }

  __eglMustCastToProperFunctionPointerType realFunc = NULL;
  {
    ScopedSuppressHooking suppress;
    realFunc = EGL.GetProcAddress(func);
  }

  // if the real context doesn't support this function, and we don't provide an implementation fully
  // ourselves, return NULL
  if(realFunc == NULL && !FullyImplementedFunction(func))
    return realFunc;

// return our egl hooks
#define GPA_FUNCTION(name)                                                                          \
  if(!strcmp(func, "egl" STRINGIZE(name)))                                                          \
    return (__eglMustCastToProperFunctionPointerType)&CONCAT(egl, CONCAT(name, _renderdoc_hooked)); \
  EGL_HOOKED_SYMBOLS(GPA_FUNCTION)
#undef GPA_FUNCTION

  // any other egl functions are safe to pass through unchanged
  if(!strncmp(func, "egl", 3))
    return realFunc;

  // otherwise, consult our database of hooks
  return (__eglMustCastToProperFunctionPointerType)HookedGetProcAddress(func, (void *)realFunc);
}

// on posix systems, someone might declare a global variable with the same name as a function. When
// doing this, it might mean that our code for "&eglSwapBuffers" looking up that global symbol will
// instead find the location fo the function pointer instead of our hook function. For this reason
// we always refer to the _renderdoc_hooked name, but we still must export the functions under their
// real names and just forward to the hook implementation.
HOOK_EXPORT EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api)
{
  return eglBindAPI_renderdoc_hooked(api);
}

HOOK_EXPORT EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display)
{
  return eglGetDisplay_renderdoc_hooked(display);
}

HOOK_EXPORT EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay display, EGLConfig config,
                                                    EGLContext shareContext, EGLint const *attribList)
{
  return eglCreateContext_renderdoc_hooked(display, config, shareContext, attribList);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
  return eglDestroyContext_renderdoc_hooked(dpy, ctx);
}

HOOK_EXPORT EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                                          EGLNativeWindowType win,
                                                          const EGLint *attrib_list)
{
  return eglCreateWindowSurface_renderdoc_hooked(dpy, config, win, attrib_list);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                                  EGLSurface read, EGLContext ctx)
{
  return eglMakeCurrent_renderdoc_hooked(display, draw, read, ctx);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
  return eglSwapBuffers_renderdoc_hooked(dpy, surface);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglPostSubBufferNV(EGLDisplay dpy, EGLSurface surface, EGLint x,
                                                      EGLint y, EGLint width, EGLint height)
{
  return eglPostSubBufferNV_renderdoc_hooked(dpy, surface, x, y, width, height);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglSwapBuffersWithDamageEXT(EGLDisplay dpy, EGLSurface surface,
                                                               EGLint *rects, EGLint n_rects)
{
  return eglSwapBuffersWithDamageEXT_renderdoc_hooked(dpy, surface, rects, n_rects);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglSwapBuffersWithDamageKHR(EGLDisplay dpy, EGLSurface surface,
                                                               EGLint *rects, EGLint n_rects)
{
  return eglSwapBuffersWithDamageKHR_renderdoc_hooked(dpy, surface, rects, n_rects);
}

HOOK_EXPORT __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *func)
{
  return eglGetProcAddress_renderdoc_hooked(func);
}

// on posix systems we need to export the whole of the EGL API, since we will have redirected any
// dlopen() for libEGL.so to ourselves, and dlsym() for any of these entry points must return a
// valid function. We don't need to intercept them, so we just pass it along

#define EGL_PASSTHRU_0(ret, function)                                                     \
  typedef ret (*CONCAT(function, _hooktype))();                                           \
  HOOK_EXPORT ret EGLAPIENTRY function()                                                  \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real();                                                                        \
  }

#define EGL_PASSTHRU_1(ret, function, t1, p1)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                         \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1)                                             \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1);                                                                      \
  }

#define EGL_PASSTHRU_2(ret, function, t1, p1, t2, p2)                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                                     \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2)                                      \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2);                                                                  \
  }

#define EGL_PASSTHRU_3(ret, function, t1, p1, t2, p2, t3, p3)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                                 \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2, t3 p3)                               \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3);                                                              \
  }

#define EGL_PASSTHRU_4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                             \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2, t3 p3, t4 p4)                        \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3, p4);                                                          \
  }

#define EGL_PASSTHRU_5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                         \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                 \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3, p4, p5);                                                      \
  }

/* EGL 1.0 */

EGL_PASSTHRU_5(EGLBoolean, eglChooseConfig, EGLDisplay, dpy, const EGLint *, attrib_list,
               EGLConfig *, configs, EGLint, config_size, EGLint *, num_config)
EGL_PASSTHRU_3(EGLBoolean, eglCopyBuffers, EGLDisplay, dpy, EGLSurface, surface,
               EGLNativePixmapType, target)
EGL_PASSTHRU_3(EGLSurface, eglCreatePbufferSurface, EGLDisplay, dpy, EGLConfig, config,
               const EGLint *, attrib_list)
EGL_PASSTHRU_4(EGLSurface, eglCreatePixmapSurface, EGLDisplay, dpy, EGLConfig, config,
               EGLNativePixmapType, pixmap, const EGLint *, attrib_list)
EGL_PASSTHRU_2(EGLBoolean, eglDestroySurface, EGLDisplay, dpy, EGLSurface, surface)
EGL_PASSTHRU_4(EGLBoolean, eglGetConfigAttrib, EGLDisplay, dpy, EGLConfig, config, EGLint,
               attribute, EGLint *, value)
EGL_PASSTHRU_4(EGLBoolean, eglGetConfigs, EGLDisplay, dpy, EGLConfig *, configs, EGLint,
               config_size, EGLint *, num_config)
EGL_PASSTHRU_0(EGLDisplay, eglGetCurrentDisplay)
EGL_PASSTHRU_1(EGLSurface, eglGetCurrentSurface, EGLint, readdraw)
EGL_PASSTHRU_0(EGLint, eglGetError)
EGL_PASSTHRU_3(EGLBoolean, eglInitialize, EGLDisplay, dpy, EGLint *, major, EGLint *, minor)
EGL_PASSTHRU_4(EGLBoolean, eglQueryContext, EGLDisplay, dpy, EGLContext, ctx, EGLint, attribute,
               EGLint *, value)
EGL_PASSTHRU_2(const char *, eglQueryString, EGLDisplay, dpy, EGLint, name)
EGL_PASSTHRU_4(EGLBoolean, eglQuerySurface, EGLDisplay, dpy, EGLSurface, surface, EGLint, attribute,
               EGLint *, value)
EGL_PASSTHRU_1(EGLBoolean, eglTerminate, EGLDisplay, dpy)
EGL_PASSTHRU_0(EGLBoolean, eglWaitGL)
EGL_PASSTHRU_1(EGLBoolean, eglWaitNative, EGLint, engine)

/* EGL 1.1 */

EGL_PASSTHRU_3(EGLBoolean, eglBindTexImage, EGLDisplay, dpy, EGLSurface, surface, EGLint, buffer)
EGL_PASSTHRU_3(EGLBoolean, eglReleaseTexImage, EGLDisplay, dpy, EGLSurface, surface, EGLint, buffer)
EGL_PASSTHRU_4(EGLBoolean, eglSurfaceAttrib, EGLDisplay, dpy, EGLSurface, surface, EGLint,
               attribute, EGLint, value)
EGL_PASSTHRU_2(EGLBoolean, eglSwapInterval, EGLDisplay, dpy, EGLint, interval)

/* EGL 1.2 */

EGL_PASSTHRU_0(EGLenum, eglQueryAPI)
EGL_PASSTHRU_5(EGLSurface, eglCreatePbufferFromClientBuffer, EGLDisplay, dpy, EGLenum, buftype,
               EGLClientBuffer, buffer, EGLConfig, config, const EGLint *, attrib_list)
EGL_PASSTHRU_0(EGLBoolean, eglReleaseThread)
EGL_PASSTHRU_0(EGLBoolean, eglWaitClient)

/* EGL 1.4 */
EGL_PASSTHRU_0(EGLContext, eglGetCurrentContext)

static void EGLHooked(void *handle)
{
  RDCDEBUG("EGL library hooked");

  DisableWGLHooksForEGL();

  // store the handle for any pass-through implementations that need to look up their onward
  // pointers
  eglhook.handle = handle;

  // as a hook callback this is only called while capturing
  RDCASSERT(!RenderDoc::Inst().IsReplayApp());

// fetch non-hooked functions into our dispatch table
#define EGL_FETCH(func, isext)                                                                  \
  EGL.func = (CONCAT(PFN_egl, func))Process::GetFunctionAddress(handle, "egl" STRINGIZE(func)); \
  if(!EGL.func && CheckConstParam(isext))                                                       \
    EGL.func = (CONCAT(PFN_egl, func))EGL.GetProcAddress("egl" STRINGIZE(func));
  EGL_NONHOOKED_SYMBOLS(EGL_FETCH)
#undef EGL_FETCH

// on systems where EGL isn't the primary/only way to get GL function pointers, we need to ensure we
// re-fetch all function pointers through eglGetProcAddress and don't try to use any through the
// primary system library (opengl32.dll/libGL.so), since they may not work correctly.
#if DISABLED(RDOC_ANDROID)
  RDCEraseEl(GL);
#endif

  // Now that libEGL is loaded, we can immediately fill out any missing functions that weren't
  // library hooked by calling eglGetProcAddress.
  GL.PopulateWithCallback([](const char *funcName) {
    // on some android devices we need to hook dlsym, but eglGetProcAddress might call dlsym so we
    // need to ensure we return the 'real' pointers
    ScopedSuppressHooking suppress;
    return (void *)EGL.GetProcAddress(funcName);
  });
}

#if ENABLED(RDOC_WIN32)
bool ShouldHookEGL()
{
  const char *toggle = Process::GetEnvVariable("RENDERDOC_HOOK_EGL");

  // if the var is set to 0, then don't hook EGL
  if(toggle && toggle[0] == '0')
    return false;

  return true;
}
#endif

void EGLHook::RegisterHooks()
{
#if ENABLED(RDOC_WIN32)
  if(!ShouldHookEGL())
  {
    RDCLOG("EGL hooks disabled - if GLES emulator is in use, underlying API will be captured");
    return;
  }
#endif

  RDCLOG("Registering EGL hooks");

#if ENABLED(RDOC_WIN32)
#define LIBSUFFIX ".dll"
#else
#define LIBSUFFIX ".so"
#endif

  // register library hooks
  LibraryHooks::RegisterLibraryHook("libEGL" LIBSUFFIX, &EGLHooked);
  LibraryHooks::RegisterLibraryHook("libEGL" LIBSUFFIX ".1", &EGLHooked);

  // we have to specify these with the most preferred library first. If the same function is
  // exported in multiple libraries, the function we call into will be the first one found
  LibraryHooks::RegisterLibraryHook("libGLESv3" LIBSUFFIX, NULL);
  LibraryHooks::RegisterLibraryHook("libGLESv2" LIBSUFFIX ".2", NULL);
  LibraryHooks::RegisterLibraryHook("libGLESv2" LIBSUFFIX, NULL);
  LibraryHooks::RegisterLibraryHook("libGLESv1_CM" LIBSUFFIX, NULL);

#if ENABLED(RDOC_WIN32)
  // on windows, we want to ignore any GLES libraries to ensure we capture the GLES calls, not the
  // underlying GL calls
  LibraryHooks::IgnoreLibrary("libEGL.dll");
  LibraryHooks::IgnoreLibrary("libGLES_CM.dll");
  LibraryHooks::IgnoreLibrary("libGLESv1_CM.dll");
  LibraryHooks::IgnoreLibrary("libGLESv2.dll");
  LibraryHooks::IgnoreLibrary("libGLESv3.dll");
#endif

// register EGL hooks
#define EGL_REGISTER(func, isext)                                                 \
  LibraryHooks::RegisterFunctionHook(                                             \
      "libEGL" LIBSUFFIX, FunctionHook("egl" STRINGIZE(func), (void **)&EGL.func, \
                                       (void *)&CONCAT(egl, CONCAT(func, _renderdoc_hooked))));
  EGL_HOOKED_SYMBOLS(EGL_REGISTER)
#undef EGL_REGISTER
}
