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

#include "core/plugins.h"
#include "strings/string_utils.h"
#include "egl_dispatch_table.h"
#include "gl_common.h"

static void *GetEGLHandle()
{
#if ENABLED(RDOC_WIN32)
  rdcstr libEGL = LocatePluginFile("gles", "libEGL.dll");

  // refuse to load libEGL.dll globally, as this is too likely to pick up ANGLE from some program
  // with poor PATH control. Instead try to explicitly load it from next to the DLL in case someone
  // has put it there.
  if(libEGL == "libEGL.dll")
  {
    rdcstr libpath;
    FileIO::GetLibraryFilename(libpath);
    libpath = get_dirname(libpath);

    libEGL = libpath + "/libEGL.dll";
  }

  return Process::LoadModule(libEGL);
#else
  void *handle = Process::LoadModule("libEGL.so.1");

  if(!handle)
    handle = Process::LoadModule("libEGL.so");

  return handle;
#endif
}

class EGLPlatform : public GLPlatform
{
  RDCDriver m_API = RDCDriver::OpenGLES;

  bool MakeContextCurrent(GLWindowingData data)
  {
    if(EGL.MakeCurrent)
      return EGL.MakeCurrent(data.egl_dpy, data.egl_wnd, data.egl_wnd, data.egl_ctx) == EGL_TRUE;

    return false;
  }

  GLWindowingData CloneTemporaryContext(GLWindowingData share)
  {
    GLWindowingData ret = share;

    ret.egl_ctx = NULL;

    if(EGL.CreateContext)
    {
      EGLint baseAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                              EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

      ret.egl_ctx = EGL.CreateContext(share.egl_dpy, share.egl_cfg, share.egl_ctx, baseAttribs);

      if(ret.egl_ctx == EGL_NO_CONTEXT)
      {
        EGLint err = EGL.GetError();
        EGL.QueryContext(share.egl_dpy, share.egl_ctx, eEGL_CONTEXT_CLIENT_VERSION, &baseAttribs[1]);

        RDCWARN(
            "Creating cloned context failed (%x). Trying again with queried old EGL client "
            "version: %d",
            err, baseAttribs[1]);

        ret.egl_ctx = EGL.CreateContext(share.egl_dpy, share.egl_cfg, share.egl_ctx, baseAttribs);
        err = EGL.GetError();
        if(ret.egl_ctx == EGL_NO_CONTEXT)
          RDCERR("Cloned context failed again (%x). Capture will likely fail", err);
      }
    }

    return ret;
  }

  void DeleteClonedContext(GLWindowingData context)
  {
    if(context.ctx && EGL.DestroyContext)
      EGL.DestroyContext(context.egl_dpy, context.egl_ctx);
  }

  void DeleteReplayContext(GLWindowingData context)
  {
    if(EGL.DestroyContext)
    {
      EGL.MakeCurrent(context.egl_dpy, 0L, 0L, NULL);
      EGL.DestroySurface(context.egl_dpy, context.egl_wnd);
      EGL.DestroyContext(context.egl_dpy, context.egl_ctx);
    }
  }

  void SwapBuffers(GLWindowingData context) { EGL.SwapBuffers(context.egl_dpy, context.egl_wnd); }
  void WindowResized(GLWindowingData context) {}
  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    // On some Linux systems the surface seems to be context dependant.
    // Thus we need to switch to that context where the surface was created.
    // To avoid any problems because of the context change we'll save the old
    // context information so we can switch back to it after the surface query is done.
    GLWindowingData oldContext;
    oldContext.egl_ctx = EGL.GetCurrentContext();
    oldContext.egl_dpy = EGL.GetCurrentDisplay();
    oldContext.egl_wnd = EGL.GetCurrentSurface(EGL_READ);
    MakeContextCurrent(context);

    EGLBoolean width_ok = EGL.QuerySurface(context.egl_dpy, context.egl_wnd, EGL_WIDTH, &w);
    EGLBoolean height_ok = EGL.QuerySurface(context.egl_dpy, context.egl_wnd, EGL_HEIGHT, &h);

    if(!width_ok || !height_ok)
    {
      RDCGLenum error_code = (RDCGLenum)EGL.GetError();
      RDCWARN("Unable to query the surface size. Error: (0x%x) %s", error_code,
              ToStr(error_code).c_str());
    }

    MakeContextCurrent(oldContext);
  }

  bool IsOutputWindowVisible(GLWindowingData context) { return true; }
  GLWindowingData MakeOutputWindow(WindowingData window, bool depth, GLWindowingData share_context)
  {
    EGLNativeDisplayType display = EGL_DEFAULT_DISPLAY;
    EGLNativeWindowType win = 0;

    switch(window.system)
    {
#if ENABLED(RDOC_WIN32)
      case WindowingSystem::Win32: win = window.win32.window; break;
#elif ENABLED(RDOC_ANDROID)
      case WindowingSystem::Android: win = window.android.window; break;
#elif ENABLED(RDOC_LINUX)
      case WindowingSystem::Xlib:
      {
        Display *xlibDisplay = RenderDoc::Inst().GetGlobalEnvironment().xlibDisplay;

        display = (EGLNativeDisplayType)window.xlib.display;
        win = (EGLNativeWindowType)window.xlib.window;

        // ensure we're using the same display as the share context, and the same as our global
        // display that we used at init time to create the share context's display
        RDCASSERT((void *)display == (void *)xlibDisplay && display != NULL, (void *)display,
                  (void *)xlibDisplay);
        break;
      }
      case WindowingSystem::Wayland:
      {
        wl_display *waylandDisplay = RenderDoc::Inst().GetGlobalEnvironment().waylandDisplay;

        display = (EGLNativeDisplayType)window.wayland.display;
        win = (EGLNativeWindowType)window.wayland.window;

        // ensure we're using the same display as the share context, and the same as our global
        // display
        RDCASSERT((void *)display == (void *)waylandDisplay && display != NULL, (void *)display,
                  (void *)waylandDisplay);
        break;
      }
#endif
      case WindowingSystem::Unknown:
      case WindowingSystem::Headless:
        // allow Unknown and Headless so that internally we can create a window-less context
        break;
      default: RDCERR("Unexpected window system %u", window.system); break;
    }

    EGLDisplay eglDisplay = NULL;

    if(share_context.egl_dpy)
      eglDisplay = share_context.egl_dpy;
    else
      eglDisplay = EGL.GetDisplay(display);

    RDCASSERT(eglDisplay);

    int major, minor;
    EGL.Initialize(eglDisplay, &major, &minor);

    return CreateWindowingData(eglDisplay, share_context.ctx, win, false);
  }

  GLWindowingData CreateWindowingData(EGLDisplay eglDisplay, EGLContext share_ctx,
                                      EGLNativeWindowType window, bool debug)
  {
#if ENABLED(RDOC_DEVEL)
    debug = true;
#endif

    GLWindowingData ret;
    ret.egl_dpy = eglDisplay;
    ret.egl_ctx = NULL;
    ret.egl_wnd = NULL;

    EGLint surfaceType = (window == 0) ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT;
    const EGLint configAttribs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_RENDERABLE_TYPE,
        m_API == RDCDriver::OpenGLES ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT,
        EGL_CONFORMANT,
        m_API == RDCDriver::OpenGLES ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE,
        surfaceType,
        EGL_COLOR_BUFFER_TYPE,
        EGL_RGB_BUFFER,
        EGL_NONE,
    };

    EGLint numConfigs = 0;
    EGLBoolean success = EGL.ChooseConfig(eglDisplay, configAttribs, &ret.egl_cfg, 1, &numConfigs);
    if(!success || numConfigs == 0)
    {
      RDCERR("Couldn't find a suitable EGL config");
      return ret;
    }

    // we try and create the highest versioned context we can, but we need at least ES3.0 (and
    // extensions) to function.
    EGLContext ctx = NULL;

    // first we try with the debug bit set, then if that fails we try without debug
    for(int debugPass = 0; debugPass < 2; debugPass++)
    {
      if(!debug && debugPass == 0)
        continue;

      // don't change this ar ray without changing indices in the loop below
      EGLint verAttribs[] = {
          EGL_CONTEXT_MAJOR_VERSION_KHR,
          3,
          EGL_CONTEXT_MINOR_VERSION_KHR,
          1,
          debugPass == 0 ? EGL_CONTEXT_FLAGS_KHR : EGL_NONE,
          EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
          EGL_NONE,
      };

      rdcarray<GLVersion> versions = GetReplayVersions(RDCDriver::OpenGLES);

      for(GLVersion v : versions)
      {
        verAttribs[1] = v.major;
        verAttribs[3] = v.minor;
        ctx = EGL.CreateContext(eglDisplay, ret.egl_cfg, share_ctx, verAttribs);

        if(ctx)
          break;
      }

      // if we got it using the major/minor pattern, then stop
      if(ctx)
        break;

      // if none of the above worked, try just creating with the client version as 3
      const EGLint baseAttribs[] = {
          EGL_CONTEXT_CLIENT_VERSION,
          3,
          debugPass == 0 ? EGL_CONTEXT_FLAGS_KHR : EGL_NONE,
          EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
          EGL_NONE,
      };

      ctx = EGL.CreateContext(eglDisplay, ret.egl_cfg, share_ctx, baseAttribs);

      // if it succeeded, stop
      if(ctx)
        break;
    }

    // if after that loop we still don't have a context, bail completely
    if(ctx == NULL)
    {
      RDCERR("Couldn't create GLES3 context");
      return ret;
    }

    ret.egl_ctx = ctx;

    EGLSurface surface = 0;
    if(window != 0)
    {
      const char *exts = EGL.QueryString(eglDisplay, EGL_EXTENSIONS);

      // create an sRGB surface if possible
      const EGLint *attrib_list = NULL;
      const EGLint srgb_attribs[] = {EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE};

      if(exts && strstr(exts, "KHR_gl_colorspace"))
        attrib_list = srgb_attribs;

      surface = EGL.CreateWindowSurface(eglDisplay, ret.egl_cfg, window, attrib_list);

      // if the sRGB surface request failed, fall back to linear
      if(surface == NULL && attrib_list != NULL)
        surface = EGL.CreateWindowSurface(eglDisplay, ret.egl_cfg, window, NULL);

      if(surface == NULL)
        RDCERR("Couldn't create surface for window");
    }
    else
    {
      static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
      surface = EGL.CreatePbufferSurface(eglDisplay, ret.egl_cfg, pbAttribs);

      if(surface == NULL)
        RDCERR("Couldn't create a suitable PBuffer");
    }

    ret.wnd = (decltype(ret.wnd))window;
    ret.egl_wnd = surface;

    return ret;
  }

  bool CanCreateGLContext()
  {
#if ENABLED(RDOC_ANDROID)
    // we don't trust the EGL API query to work reliably on Android, so treat
    // it as special case
    return false;
#else
    bool success = EGL.PopulateForReplay();

    // if we can't populate our functions we bail now.
    if(!success)
      return false;

    EGLenum previousAPI = EGL.QueryAPI();
    EGLBoolean supportsGL = EGL.BindAPI(EGL_OPENGL_API);

    // restore previous API
    if(previousAPI != EGL_NONE)
      EGL.BindAPI(previousAPI);

    return supportsGL == EGL_TRUE;
#endif
  }

  bool CanCreateGLESContext()
  {
    // as long as we can get libEGL we're fine
    return GetEGLHandle() != NULL;
  }

  bool PopulateForReplay() { return EGL.PopulateForReplay(); }
  void SetDriverType(RDCDriver api) { m_API = api; }
  RDResult InitialiseAPI(GLWindowingData &replayContext, RDCDriver api, bool debug)
  {
    Display *xlibDisplay = RenderDoc::Inst().GetGlobalEnvironment().xlibDisplay;
    wl_display *waylandDisplay = RenderDoc::Inst().GetGlobalEnvironment().waylandDisplay;

    // we support replaying both GLES and GL through EGL
    RDCASSERT(api == RDCDriver::OpenGLES || api == RDCDriver::OpenGL);

    m_API = api;

    if(api == RDCDriver::OpenGLES)
      EGL.BindAPI(EGL_OPENGL_ES_API);
    else
      EGL.BindAPI(EGL_OPENGL_API);

    EGLNativeDisplayType display = EGL_DEFAULT_DISPLAY;
    if(waylandDisplay)
      display = (EGLNativeDisplayType)waylandDisplay;
    else if(xlibDisplay)
      display = (EGLNativeDisplayType)xlibDisplay;

    EGLDisplay eglDisplay = EGL.GetDisplay(display);
    if(!eglDisplay)
    {
      RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Couldn't open default EGL display");
    }

    int major, minor;
    EGL.Initialize(eglDisplay, &major, &minor);

    replayContext = CreateWindowingData(eglDisplay, EGL_NO_CONTEXT, 0, debug);

    if(!replayContext.ctx)
    {
      DeleteReplayContext(replayContext);
      RDCEraseEl(replayContext);
      RETURN_ERROR_RESULT(ResultCode::APIHardwareUnsupported,
                          "Couldn't create OpenGL ES 3.x replay context - required for replay");
    }

    return ResultCode::Succeeded;
  }

  void *GetReplayFunction(const char *funcname)
  {
    void *ret = (void *)EGL.GetProcAddress(funcname);
    if(ret)
      return ret;

    ret = Process::GetFunctionAddress(GetEGLHandle(), funcname);
    if(ret)
      return ret;

#if ENABLED(RDOC_WIN32)
#define LIBSUFFIX ".dll"
#else
#define LIBSUFFIX ".so"
#endif
    const char *libs[] = {
        "libGLESv3" LIBSUFFIX,
        "libGLESv2" LIBSUFFIX ".2",
        "libGLESv2" LIBSUFFIX,
        "libGLESv1_CM" LIBSUFFIX,
    };

    for(size_t i = 0; i < ARRAY_COUNT(libs); i++)
    {
      ret = Process::GetFunctionAddress(Process::LoadModule(libs[i]), funcname);
      if(ret)
        return ret;
    }

    return NULL;
  }
  void DrawQuads(float width, float height, const rdcarray<Vec4f> &vertices)
  {
    // legacy quad rendering is not supported on GLES
  }
} eglPlatform;

GLPlatform &GetEGLPlatform()
{
  return eglPlatform;
}

EGLDispatchTable EGL = {};

bool EGLDispatchTable::PopulateForReplay()
{
  RDCASSERT(RenderDoc::Inst().IsReplayApp());

  void *handle = GetEGLHandle();

  if(!handle)
  {
    RDCERR("Can't load libEGL");
    return false;
  }

  RDCDEBUG("Initialising EGL function pointers");

  bool symbols_ok = true;

#define LOAD_FUNC(func, isext, replayrequired)                                                      \
  if(!this->func)                                                                                   \
    this->func = (CONCAT(PFN_egl, func))Process::GetFunctionAddress(handle, "egl" STRINGIZE(func)); \
  if(!this->func && CheckConstParam(isext))                                                         \
    this->func = (CONCAT(PFN_egl, func))this->GetProcAddress("egl" STRINGIZE(func));                \
                                                                                                    \
  if(!this->func && !CheckConstParam(isext))                                                        \
  {                                                                                                 \
    if(CheckConstParam(replayrequired))                                                             \
      symbols_ok = false;                                                                           \
    RDCWARN("Unable to load '%s'", STRINGIZE(func));                                                \
  }

  EGL_HOOKED_SYMBOLS(LOAD_FUNC)
  EGL_NONHOOKED_SYMBOLS(LOAD_FUNC)

#undef LOAD_FUNC
  return symbols_ok;
}
