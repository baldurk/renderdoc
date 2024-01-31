/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "core/settings.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"
#include "egl_dispatch_table.h"
#include "gl_driver.h"

RDOC_CONFIG(bool, Android_AllowAllEGLExtensions, false,
            "Normally certain extensions are removed from the EGL extension string for "
            "compatibility, but with this option that behaviour can be overridden and all "
            "extensions will be reported.");

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
void UseXlibDisplay(Display *dpy);
WindowingSystem UseUnknownDisplay(void *disp);
void UseWaylandDisplay(wl_display *disp);
}
#endif

struct SurfaceConfig
{
  WindowingSystem system;
  void *wnd;
};

struct DisplayConfig
{
  WindowingSystem system;
};

class EGLHook : LibraryHook
{
public:
  EGLHook() : driver(GetEGLPlatform()) {}
  ~EGLHook()
  {
    for(auto it : extStrings)
      SAFE_DELETE(it.second);
  }
  void RegisterHooks();

  RDCDriver activeAPI = RDCDriver::OpenGLES;

  void *handle = DEFAULT_HANDLE;
  WrappedOpenGL driver;
  std::set<EGLContext> contexts;
  std::map<EGLContext, EGLConfig> configs;
  std::map<EGLSurface, SurfaceConfig> windows;
  std::map<EGLDisplay, DisplayConfig> displays;
  std::map<EGLDisplay, rdcstr *> extStrings;

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
    EGLConfig config = data.egl_cfg;

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

      int multiSamples;
      EGL.GetConfigAttrib(display, config, EGL_SAMPLES, &multiSamples);
      if(multiSamples != 1 && multiSamples != 2 && multiSamples != 4 && multiSamples != 8)
      {
        multiSamples = 1;
      }

      params.width = width;
      params.height = height;
      params.isSRGB = isSRGB;
      params.isYFlipped = isYFlipped;
      params.multiSamples = multiSamples;
    }
  }

} eglhook;

// On linux if a user doesn't link to libEGL or try to dlopen it, but just calls dlsym with
// RTLD_NEXT it might successfully one of our functions without anything ever loading libEGL. Then
// our attempts to call onwards will fail. When any of our functions are called we check to see if
// DEFAULT_HANDLE is RTLD_NEXT and if so we manually load the library. This will trigger our hook
// callback and we'll get a specific library handle.
//
// On other platforms this is not needed because we know the real library will be loaded before any
// of our hooks can be called
static void EnsureRealLibraryLoaded()
{
#if ENABLED(RDOC_LINUX)
  if(eglhook.handle == DEFAULT_HANDLE)
  {
    if(!RenderDoc::Inst().IsReplayApp())
      RDCLOG("Loading libEGL at the last second");

    void *handle = Process::LoadModule("libEGL.so.1");

    if(!handle)
      handle = Process::LoadModule("libEGL.so");

    if(RenderDoc::Inst().IsReplayApp())
      eglhook.handle = handle;
  }
#endif
}

HOOK_EXPORT EGLDisplay EGLAPIENTRY eglGetDisplay_renderdoc_hooked(EGLNativeDisplayType display)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.GetDisplay)
      EGL.PopulateForReplay();

    return EGL.GetDisplay(display);
  }

  EnsureRealLibraryLoaded();

#if ENABLED(RDOC_LINUX)

  // display can be EGL_DEFAULT_DISPLAY which is NULL, and unfortunately we don't have anything then
  if(display)
    Keyboard::UseUnknownDisplay((void *)display);

// if xlib is compiled we can try to get the default display (which is what this will do)
#if ENABLED(RDOC_XLIB)
  else
    Keyboard::UseUnknownDisplay(XOpenDisplay(NULL));
#endif

#endif

  return EGL.GetDisplay(display);
}

HOOK_EXPORT EGLDisplay EGLAPIENTRY eglGetPlatformDisplay_renderdoc_hooked(EGLenum platform,
                                                                          void *native_display,
                                                                          const EGLAttrib *attrib_list)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.GetDisplay)
      EGL.PopulateForReplay();

    return EGL.GetPlatformDisplay(platform, native_display, attrib_list);
  }

  EnsureRealLibraryLoaded();

#if ENABLED(RDOC_LINUX)
  if(platform == EGL_PLATFORM_X11_KHR)
    Keyboard::UseXlibDisplay((Display *)native_display);
  else if(platform == EGL_PLATFORM_WAYLAND_KHR)
    Keyboard::UseWaylandDisplay((wl_display *)native_display);
  else
    RDCWARN("Unknown platform %x in eglGetPlatformDisplay", platform);
#endif

  return EGL.GetPlatformDisplay(platform, native_display, attrib_list);
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglBindAPI_renderdoc_hooked(EGLenum api)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.GetDisplay)
      EGL.PopulateForReplay();

    return EGL.BindAPI(api);
  }

  EnsureRealLibraryLoaded();

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

  EnsureRealLibraryLoaded();

  LibraryHooks::Refresh();

  rdcarray<EGLint> attribs;

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

        // remove reset notification attributes, so that we don't have to carry this bit around to
        // know how to safely create sharing contexts.
        if(name == EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT)
        {
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

  EGLint rgbSize[3] = {};
  EGL.GetConfigAttrib(display, config, EGL_RED_SIZE, &rgbSize[0]);
  EGL.GetConfigAttrib(display, config, EGL_GREEN_SIZE, &rgbSize[1]);
  EGL.GetConfigAttrib(display, config, EGL_BLUE_SIZE, &rgbSize[2]);

  if(rgbSize[0] == rgbSize[1] && rgbSize[1] == rgbSize[2] && rgbSize[2] == 10)
    init.colorBits = 10;

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

  EnsureRealLibraryLoaded();

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

  EnsureRealLibraryLoaded();

  EGLSurface ret = EGL.CreateWindowSurface(dpy, config, win, attrib_list);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    // spec says it's implementation dependent what happens, so we assume that we're using the same
    // window system as the display
    eglhook.windows[ret] = {eglhook.displays[dpy].system, (void *)win};
  }

  return ret;
}

HOOK_EXPORT EGLSurface EGLAPIENTRY eglCreatePlatformWindowSurface_renderdoc_hooked(
    EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.CreatePlatformWindowSurface)
      EGL.PopulateForReplay();

    return EGL.CreatePlatformWindowSurface(dpy, config, native_window, attrib_list);
  }

  EnsureRealLibraryLoaded();

  EGLSurface ret = EGL.CreatePlatformWindowSurface(dpy, config, native_window, attrib_list);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    // spec guarantees that we're using the same window system as the display
    eglhook.windows[ret] = {eglhook.displays[dpy].system, native_window};
  }

  return ret;
}

HOOK_EXPORT EGLBoolean EGLAPIENTRY eglMakeCurrent_renderdoc_hooked(EGLDisplay display,
                                                                   EGLSurface draw, EGLSurface read,
                                                                   EGLContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.MakeCurrent || !EGL.GetProcAddress)
      EGL.PopulateForReplay();

    // populate GL function pointers now in case linked functions are called
    if(EGL.GetProcAddress)
    {
      GL.PopulateWithCallback(
          [](const char *funcName) -> void * { return (void *)EGL.GetProcAddress(funcName); });
    }

    return EGL.MakeCurrent(display, draw, read, ctx);
  }

  EnsureRealLibraryLoaded();

  EGLBoolean ret = EGL.MakeCurrent(display, draw, read, ctx);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    SetDriverForHooks(&eglhook.driver);

    if(ctx && eglhook.contexts.find(ctx) == eglhook.contexts.end())
    {
      eglhook.contexts.insert(ctx);

      if(FetchEnabledExtensions())
      {
        // see gl_emulated.cpp
        GL.EmulateUnsupportedFunctions();
        GL.EmulateRequiredExtensions();
        GL.DriverForEmulation(&eglhook.driver);
      }
    }

    SurfaceConfig cfg = eglhook.windows[draw];

    GLWindowingData data;
    data.egl_dpy = display;
    data.egl_wnd = draw;
    data.egl_ctx = ctx;
    data.wnd = (decltype(data.wnd))cfg.wnd;

    if(!data.wnd)
    {
      // could be a pbuffer surface or other offscreen rendering. We want a valid wnd, so set it to
      // a dummy value
      data.wnd = (decltype(data.wnd))(void *)(uintptr_t(0xdeadbeef) + uintptr_t(draw));
      cfg.system = WindowingSystem::Headless;
    }

    // we could query this out technically but it's easier to keep a map
    data.egl_cfg = eglhook.configs[ctx];

    eglhook.driver.SetDriverType(eglhook.activeAPI);

    eglhook.RefreshWindowParameters(data);

    eglhook.driver.ActivateContext(data);
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

  EnsureRealLibraryLoaded();

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
  {
    GLWindowingData data;
    data.egl_dpy = dpy;
    data.egl_wnd = surface;
    data.egl_ctx = EGL.GetCurrentContext();

    eglhook.RefreshWindowParameters(data);

    SurfaceConfig cfg = eglhook.windows[surface];

    gl_CurChunk = GLChunk::eglSwapBuffers;

    eglhook.driver.SwapBuffers(cfg.system, cfg.wnd);
  }

  {
    eglhook.swapping = true;
    EGLBoolean ret = EGL.SwapBuffers(dpy, surface);
    eglhook.swapping = false;
    return ret;
  }
}

HOOK_EXPORT const char *EGLAPIENTRY eglQueryString_renderdoc_hooked(EGLDisplay dpy, EGLint name)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!EGL.QueryString)
      EGL.PopulateForReplay();

    return EGL.QueryString(dpy, name);
  }

  EnsureRealLibraryLoaded();

  SCOPED_LOCK(glLock);

  if(name == EGL_EXTENSIONS && !Android_AllowAllEGLExtensions())
  {
    rdcstr *extStr = eglhook.extStrings[dpy];
    if(extStr == NULL)
      extStr = eglhook.extStrings[dpy] = new rdcstr;

    const rdcstr implExtStr = EGL.QueryString(dpy, name);

    rdcarray<rdcstr> exts;
    split(implExtStr, exts, ' ');

    // We take the unusual approach here of explicitly _disallowing_ extensions only when we know
    // they are unsupported. The main reason for this is because EGL is the android platform API and
    // it may well be that undocumented internal or private extensions are important and should not
    // be filtered out. Also since we have minimal interaction with the API as long as they don't
    // affect the functions we care about for context management and swapping most extensions can be
    // allowed silently.
    exts.removeOne("EGL_KHR_no_config_context");

    merge(exts, *extStr, ' ');
  }

  return EGL.QueryString(dpy, name);
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

  EnsureRealLibraryLoaded();

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
  {
    SurfaceConfig cfg = eglhook.windows[surface];

    gl_CurChunk = GLChunk::eglPostSubBufferNV;

    eglhook.driver.SwapBuffers(cfg.system, cfg.wnd);
  }

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

  EnsureRealLibraryLoaded();

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
  {
    SurfaceConfig cfg = eglhook.windows[surface];

    gl_CurChunk = GLChunk::eglSwapBuffersWithDamageEXT;

    eglhook.driver.SwapBuffers(cfg.system, cfg.wnd);
  }

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

  EnsureRealLibraryLoaded();

  SCOPED_LOCK(glLock);

  eglhook.driver.SetDriverType(eglhook.activeAPI);
  if(!eglhook.driver.UsesVRFrameMarkers() && !eglhook.swapping)
  {
    SurfaceConfig cfg = eglhook.windows[surface];

    gl_CurChunk = GLChunk::eglSwapBuffersWithDamageKHR;

    eglhook.driver.SwapBuffers(cfg.system, cfg.wnd);
  }

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

  EnsureRealLibraryLoaded();

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
#define GPA_FUNCTION(name, isext, replayrequired) \
  if(!strcmp(func, "egl" STRINGIZE(name)))        \
    return (__eglMustCastToProperFunctionPointerType)&CONCAT(egl, CONCAT(name, _renderdoc_hooked));
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

HOOK_EXPORT EGLDisplay EGLAPIENTRY eglGetPlatformDisplay(EGLenum platform, void *native_display,
                                                         const EGLAttrib *attrib_list)
{
  return eglGetPlatformDisplay_renderdoc_hooked(platform, native_display, attrib_list);
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

HOOK_EXPORT EGLSurface EGLAPIENTRY eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig config,
                                                                  void *native_window,
                                                                  const EGLAttrib *attrib_list)
{
  return eglCreatePlatformWindowSurface_renderdoc_hooked(dpy, config, native_window, attrib_list);
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

HOOK_EXPORT const char *EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name)
{
  return eglQueryString_renderdoc_hooked(dpy, name);
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
    EnsureRealLibraryLoaded();                                                            \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real();                                                                        \
  }

#define EGL_PASSTHRU_1(ret, function, t1, p1)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                         \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1)                                             \
  {                                                                                       \
    EnsureRealLibraryLoaded();                                                            \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1);                                                                      \
  }

#define EGL_PASSTHRU_2(ret, function, t1, p1, t2, p2)                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                                     \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2)                                      \
  {                                                                                       \
    EnsureRealLibraryLoaded();                                                            \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2);                                                                  \
  }

#define EGL_PASSTHRU_3(ret, function, t1, p1, t2, p2, t3, p3)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                                 \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2, t3 p3)                               \
  {                                                                                       \
    EnsureRealLibraryLoaded();                                                            \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3);                                                              \
  }

#define EGL_PASSTHRU_4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                             \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2, t3 p3, t4 p4)                        \
  {                                                                                       \
    EnsureRealLibraryLoaded();                                                            \
    CONCAT(function, _hooktype)                                                           \
    real = (CONCAT(function, _hooktype))Process::GetFunctionAddress(eglhook.handle,       \
                                                                    STRINGIZE(function)); \
    return real(p1, p2, p3, p4);                                                          \
  }

#define EGL_PASSTHRU_5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                         \
  HOOK_EXPORT ret EGLAPIENTRY function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                 \
  {                                                                                       \
    EnsureRealLibraryLoaded();                                                            \
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

/* EGL 1.5 */
EGL_PASSTHRU_3(EGLSync, eglCreateSync, EGLDisplay, dpy, EGLenum, type, const EGLAttrib *, attrib_list)
EGL_PASSTHRU_2(EGLBoolean, eglDestroySync, EGLDisplay, dpy, EGLSync, sync)
EGL_PASSTHRU_4(EGLint, eglClientWaitSync, EGLDisplay, dpy, EGLSync, sync, EGLint, flags, EGLTime,
               timeout)
EGL_PASSTHRU_4(EGLBoolean, eglGetSyncAttrib, EGLDisplay, dpy, EGLSync, sync, EGLint, attribute,
               EGLAttrib *, value)
EGL_PASSTHRU_5(EGLImage, eglCreateImage, EGLDisplay, dpy, EGLContext, ctx, EGLenum, target,
               EGLClientBuffer, buffer, const EGLAttrib *, attrib_list)
EGL_PASSTHRU_2(EGLBoolean, eglDestroyImage, EGLDisplay, dpy, EGLImage, image)
EGL_PASSTHRU_4(EGLSurface, eglCreatePlatformPixmapSurface, EGLDisplay, dpy, EGLConfig, config,
               void *, native_pixmap, const EGLAttrib *, attrib_list)
EGL_PASSTHRU_3(EGLBoolean, eglWaitSync, EGLDisplay, dpy, EGLSync, sync, EGLint, flags)

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
#define EGL_FETCH(func, isext, replayrequired)                                                  \
  EGL.func = (CONCAT(PFN_egl, func))Process::GetFunctionAddress(handle, "egl" STRINGIZE(func)); \
  if(!EGL.func && CheckConstParam(isext))                                                       \
    EGL.func = (CONCAT(PFN_egl, func))EGL.GetProcAddress("egl" STRINGIZE(func));
  EGL_NONHOOKED_SYMBOLS(EGL_FETCH)
#undef EGL_FETCH

// fetch any hooked extension functions into our dispatch table since they're not necessarily
// exported
#define EGL_FETCH(func, isext, replayrequired) \
  if(!EGL.func)                                \
    EGL.func = (CONCAT(PFN_egl, func))EGL.GetProcAddress("egl" STRINGIZE(func));
  EGL_HOOKED_SYMBOLS(EGL_FETCH)
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
  rdcstr toggle = Process::GetEnvVariable("RENDERDOC_HOOK_EGL");

  // if the var is set to 0, then don't hook EGL
  if(toggle.size() >= 1 && toggle[0] == '0')
  {
    RDCLOG(
        "EGL hooks disabled by RENDERDOC_HOOK_EGL environment variable - "
        "if GLES emulator is in use, underlying API will be captured");
    return false;
  }

  return true;
}

#elif ENABLED(RDOC_ANDROID)

bool ShouldHookEGL()
{
  void *egl_handle = dlopen("libEGL.so", RTLD_LAZY);
  PFN_eglQueryString query_string = (PFN_eglQueryString)dlsym(egl_handle, "eglQueryString");
  if(!query_string)
  {
    RDCERR("Unable to find eglQueryString entry point, enabling EGL hooking");
    return true;
  }

  rdcstr ignore_layers = Process::GetEnvVariable("IGNORE_LAYERS");

  // if we set IGNORE_LAYERS externally that means the layers are broken or can't be configured, so
  // hook EGL in spite of the layers being present
  if(ignore_layers.size() >= 1 && ignore_layers[0] == '1')
    return true;

  const char *eglExts = query_string(EGL_NO_DISPLAY, EGL_EXTENSIONS);

  if(eglExts && strstr(eglExts, "EGL_ANDROID_GLES_layers"))
  {
    RDCLOG("EGL_ANDROID_GLES_layers detected, disabling EGL hooks - GLES layering in effect");
    return false;
  }

  return true;
}

#else

bool ShouldHookEGL()
{
  return true;
}

#endif

void EGLHook::RegisterHooks()
{
  if(!ShouldHookEGL())
    return;

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
#define EGL_REGISTER(func, isext, replayrequired)             \
  LibraryHooks::RegisterFunctionHook(                         \
      "libEGL" LIBSUFFIX,                                     \
      FunctionHook("egl" STRINGIZE(func), (void **)&EGL.func, \
                                   (void *)&CONCAT(egl, CONCAT(func, _renderdoc_hooked))));
  EGL_HOOKED_SYMBOLS(EGL_REGISTER)
#undef EGL_REGISTER
}

// Android GLES layering support
#if ENABLED(RDOC_ANDROID)

typedef __eglMustCastToProperFunctionPointerType(EGLAPIENTRY *PFNEGLGETNEXTLAYERPROCADDRESSPROC)(
    void *, const char *funcName);

HOOK_EXPORT void AndroidGLESLayer_Initialize(void *layer_id,
                                             PFNEGLGETNEXTLAYERPROCADDRESSPROC next_gpa)
{
  RDCLOG("Initialising Android GLES layer with ID %p", layer_id);

  // as a hook callback this is only called while capturing
  RDCASSERT(!RenderDoc::Inst().IsReplayApp());

// populate EGL dispatch table with the next layer's function pointers. Fetch all 'hooked' and
// non-hooked functions
#define EGL_FETCH(func, isext, replayrequired)                                 \
  EGL.func = (CONCAT(PFN_egl, func))next_gpa(layer_id, "egl" STRINGIZE(func)); \
  if(!EGL.func)                                                                \
    RDCWARN("Couldn't fetch function pointer for egl" STRINGIZE(func));
  EGL_HOOKED_SYMBOLS(EGL_FETCH)
  EGL_NONHOOKED_SYMBOLS(EGL_FETCH)
#undef EGL_FETCH

  // populate GL dispatch table with the next layer's function pointers
  GL.PopulateWithCallback(
      [layer_id, next_gpa](const char *f) { return (void *)next_gpa(layer_id, f); });
}

HOOK_EXPORT void *AndroidGLESLayer_GetProcAddress(const char *funcName,
                                                  __eglMustCastToProperFunctionPointerType next)
{
// return our egl hooks
#define GPA_FUNCTION(name, isext, replayrequired) \
  if(!strcmp(funcName, "egl" STRINGIZE(name)))    \
    return (void *)&CONCAT(egl, CONCAT(name, _renderdoc_hooked));
  EGL_HOOKED_SYMBOLS(GPA_FUNCTION)
#undef GPA_FUNCTION

  // otherwise, consult our database of hooks
  // Android GLES layer spec expects us to return next unmodified for functions we don't support
  return HookedGetProcAddress(funcName, (void *)next);
}

#endif    // ENABLED(RDOC_ANDROID)
