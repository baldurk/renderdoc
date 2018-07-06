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

  void *handle = DEFAULT_HANDLE;
  WrappedOpenGL driver;
  std::set<EGLContext> contexts;
  std::map<EGLContext, EGLConfig> configs;
} eglhook;

HOOK_EXPORT EGLDisplay eglGetDisplay(EGLNativeDisplayType display)
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

HOOK_EXPORT EGLContext eglCreateContext(EGLDisplay display, EGLConfig config,
                                        EGLContext shareContext, EGLint const *attribList)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.CreateContext)
      EGL.PopulateForReplay();

    return EGL.CreateContext(display, config, shareContext, attribList);
  }

  LibraryHooks::Refresh();

  vector<EGLint> attribs;

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

          flagsFound = true;
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
  data.egl_wnd = (EGLSurface)NULL;
  data.egl_ctx = ret;
  data.egl_cfg = config;

  eglhook.configs[ret] = config;

  eglhook.driver.SetDriverType(RDCDriver::OpenGLES);
  {
    SCOPED_LOCK(glLock);
    eglhook.driver.CreateContext(data, shareContext, init, true, true);
  }

  return ret;
}

HOOK_EXPORT EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.DestroyContext)
      EGL.PopulateForReplay();

    return EGL.DestroyContext(dpy, ctx);
  }

  eglhook.driver.SetDriverType(RDCDriver::OpenGLES);
  {
    SCOPED_LOCK(glLock);
    eglhook.driver.DeleteContext(ctx);
    eglhook.contexts.erase(ctx);
  }

  return EGL.DestroyContext(dpy, ctx);
}

HOOK_EXPORT EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read,
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

    // we could query this out technically but it's easier to keep a map
    data.egl_cfg = eglhook.configs[ctx];

    eglhook.driver.SetDriverType(RDCDriver::OpenGLES);

    eglhook.driver.ActivateContext(data);
  }

  return ret;
}

HOOK_EXPORT EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.SwapBuffers)
      EGL.PopulateForReplay();

    return EGL.SwapBuffers(dpy, surface);
  }

  SCOPED_LOCK(glLock);

  int height, width;
  EGL.QuerySurface(dpy, surface, EGL_HEIGHT, &height);
  EGL.QuerySurface(dpy, surface, EGL_WIDTH, &width);

  GLInitParams &init = eglhook.driver.GetInitParams();
  int colorspace = 0;
  EGL.QuerySurface(dpy, surface, EGL_GL_COLORSPACE, &colorspace);
  // GL_SRGB8_ALPHA8 is specified as color-renderable, unlike GL_SRGB8.
  init.isSRGB = init.colorBits == 32 && colorspace == EGL_GL_COLORSPACE_SRGB;

  eglhook.driver.SetDriverType(RDCDriver::OpenGLES);
  eglhook.driver.WindowSize(surface, width, height);
  if(!eglhook.driver.UsesVRFrameMarkers())
    eglhook.driver.SwapBuffers(surface);

  return EGL.SwapBuffers(dpy, surface);
}

HOOK_EXPORT EGLBoolean eglPostSubBufferNV(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y,
                                          EGLint width, EGLint height)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.PostSubBufferNV)
      EGL.PopulateForReplay();

    return EGL.PostSubBufferNV(dpy, surface, x, y, width, height);
  }

  SCOPED_LOCK(glLock);

  int winheight, winwidth;
  EGL.QuerySurface(dpy, surface, EGL_HEIGHT, &winheight);
  EGL.QuerySurface(dpy, surface, EGL_WIDTH, &winwidth);

  GLInitParams &init = eglhook.driver.GetInitParams();
  int colorspace = 0;
  EGL.QuerySurface(dpy, surface, EGL_GL_COLORSPACE, &colorspace);
  // GL_SRGB8_ALPHA8 is specified as color-renderable, unlike GL_SRGB8.
  init.isSRGB = init.colorBits == 32 && colorspace == EGL_GL_COLORSPACE_SRGB;

  eglhook.driver.SetDriverType(RDCDriver::OpenGLES);
  eglhook.driver.WindowSize(surface, winwidth, winheight);
  if(!eglhook.driver.UsesVRFrameMarkers())
    eglhook.driver.SwapBuffers(surface);

  return EGL.PostSubBufferNV(dpy, surface, x, y, width, height);
}

HOOK_EXPORT __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *func)
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

  // if the real context doesn't support this function, return NULL
  if(realFunc == NULL)
    return realFunc;

  // return our egl hooks
  if(!strcmp(func, "eglCreateContext"))
    return (__eglMustCastToProperFunctionPointerType)&eglCreateContext;
  if(!strcmp(func, "eglGetDisplay"))
    return (__eglMustCastToProperFunctionPointerType)&eglGetDisplay;
  if(!strcmp(func, "eglDestroyContext"))
    return (__eglMustCastToProperFunctionPointerType)&eglDestroyContext;
  if(!strcmp(func, "eglMakeCurrent"))
    return (__eglMustCastToProperFunctionPointerType)&eglMakeCurrent;
  if(!strcmp(func, "eglSwapBuffers"))
    return (__eglMustCastToProperFunctionPointerType)&eglSwapBuffers;
  if(!strcmp(func, "eglPostSubBufferNV"))
    return (__eglMustCastToProperFunctionPointerType)&eglPostSubBufferNV;

  // any other egl functions are safe to pass through unchanged
  if(!strncmp(func, "egl", 3))
    return realFunc;

  // otherwise, consult our database of hooks
  return (__eglMustCastToProperFunctionPointerType)HookedGetProcAddress(func, (void *)realFunc);
}

// on posix systems we need to export the whole of the EGL API, since we will have redirected any
// dlopen() for libEGL.so to ourselves, and dlsym() for any of these entry points must return a
// valid function. We don't need to intercept them, so we just pass it along

#define EGL_PASSTHRU_0(ret, function)                                                     \
  typedef ret (*CONCAT(function, _hooktype))();                                           \
  HOOK_EXPORT ret function()                                                              \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real();                                                                        \
  }

#define EGL_PASSTHRU_1(ret, function, t1, p1)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                         \
  HOOK_EXPORT ret function(t1 p1)                                                         \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1);                                                                      \
  }

#define EGL_PASSTHRU_2(ret, function, t1, p1, t2, p2)                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                                     \
  HOOK_EXPORT ret function(t1 p1, t2 p2)                                                  \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2);                                                                  \
  }

#define EGL_PASSTHRU_3(ret, function, t1, p1, t2, p2, t3, p3)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                                 \
  HOOK_EXPORT ret function(t1 p1, t2 p2, t3 p3)                                           \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3);                                                              \
  }

#define EGL_PASSTHRU_4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                             \
  HOOK_EXPORT ret function(t1 p1, t2 p2, t3 p3, t4 p4)                                    \
  {                                                                                       \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3, p4);                                                          \
  }

#define EGL_PASSTHRU_5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                         \
  HOOK_EXPORT ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                             \
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
EGL_PASSTHRU_4(EGLSurface, eglCreateWindowSurface, EGLDisplay, dpy, EGLConfig, config,
               EGLNativeWindowType, win, const EGLint *, attrib_list)
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

EGL_PASSTHRU_1(EGLBoolean, eglBindAPI, EGLenum, api)
EGL_PASSTHRU_0(EGLenum, eglQueryAPI)
EGL_PASSTHRU_5(EGLSurface, eglCreatePbufferFromClientBuffer, EGLDisplay, dpy, EGLenum, buftype,
               EGLClientBuffer, buffer, EGLConfig, config, const EGLint *, attrib_list)
EGL_PASSTHRU_0(EGLBoolean, eglReleaseThread)
EGL_PASSTHRU_0(EGLBoolean, eglWaitClient)

/* EGL 1.4 */
EGL_PASSTHRU_0(EGLContext, eglGetCurrentContext)

static void EGLHooked(void *handle)
{
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

  // Now that libEGL is loaded, we can immediately fill out any missing functions that weren't
  // library hooked by calling eglGetProcAddress.
  GL.PopulateWithCallback([](const char *funcName) {
    // on some android devices we need to hook dlsym, but eglGetProcAddress might call dlsym so we
    // need to ensure we return the 'real' pointers
    ScopedSuppressHooking suppress;
    return (void *)EGL.GetProcAddress(funcName);
  });
}

void EGLHook::RegisterHooks()
{
#if ENABLED(RENDERDOC_HOOK_EGL)

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
#define EGL_REGISTER(func, isext)     \
  LibraryHooks::RegisterFunctionHook( \
      "libEGL" LIBSUFFIX,             \
      FunctionHook("egl" STRINGIZE(func), (void **)&EGL.func, (void *)&CONCAT(egl, func)));
  EGL_HOOKED_SYMBOLS(EGL_REGISTER)
#undef EGL_REGISTER

#else

  RDCLOG("EGL hooks disabled - if GLES emulator is in use, underlying API will be captured");

#endif
}
