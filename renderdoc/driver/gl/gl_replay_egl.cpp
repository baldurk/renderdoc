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

typedef EGLBoolean (*PFN_eglBindAPI)(EGLenum api);
typedef EGLDisplay (*PFN_eglGetDisplay)(EGLNativeDisplayType display_id);
typedef EGLContext (*PFN_eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                           EGLContext share_context, const EGLint *attrib_list);
typedef EGLBoolean (*PFN_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                                         EGLContext ctx);
typedef EGLBoolean (*PFN_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (*PFN_eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean (*PFN_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
                                          EGLint *value);
typedef EGLBoolean (*PFN_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
typedef EGLSurface (*PFN_eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                  const EGLint *attrib_list);
typedef EGLSurface (*PFN_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                                 EGLNativeWindowType win, const EGLint *attrib_list);
typedef EGLBoolean (*PFN_eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                          EGLConfig *configs, EGLint config_size, EGLint *num_config);
typedef __eglMustCastToProperFunctionPointerType (*PFN_eglGetProcAddress)(const char *procname);
typedef EGLBoolean (*PFN_eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);

PFN_eglBindAPI eglBindAPIProc = NULL;
PFN_eglInitialize eglInitializeProc = NULL;
PFN_eglGetDisplay eglGetDisplayProc = NULL;
PFN_eglCreateContext eglCreateContextProc = NULL;
PFN_eglMakeCurrent eglMakeCurrentProc = NULL;
PFN_eglSwapBuffers eglSwapBuffersProc = NULL;
PFN_eglDestroyContext eglDestroyContextProc = NULL;
PFN_eglQuerySurface eglQuerySurfaceProc = NULL;
PFN_eglDestroySurface eglDestroySurfaceProc = NULL;
PFN_eglCreatePbufferSurface eglCreatePbufferSurfaceProc = NULL;
PFN_eglCreateWindowSurface eglCreateWindowSurfaceProc = NULL;
PFN_eglChooseConfig eglChooseConfigProc = NULL;
PFN_eglGetProcAddress eglGetProcAddressProc = NULL;

void GLReplay::MakeCurrentReplayContext(GLWindowingData *ctx)
{
  static GLWindowingData *prev = NULL;

  if(eglMakeCurrentProc && ctx && ctx != prev)
  {
    prev = ctx;
    eglMakeCurrentProc(ctx->egl_dpy, ctx->egl_wnd, ctx->egl_wnd, ctx->egl_ctx);
    m_pDriver->ActivateContext(*ctx);
  }
}

void GLReplay::SwapBuffers(GLWindowingData *ctx)
{
  eglSwapBuffersProc(ctx->egl_dpy, ctx->egl_wnd);
}

void GLReplay::CloseReplayContext()
{
  if(eglDestroyContextProc)
  {
    eglMakeCurrentProc(m_ReplayCtx.egl_dpy, 0L, 0L, NULL);
    eglDestroyContextProc(m_ReplayCtx.egl_dpy, m_ReplayCtx.egl_ctx);
  }
}

uint64_t GLReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  EGLNativeWindowType window = 0;

  if(system == eWindowingSystem_Xlib)
  {
    XlibWindowData *xlib = (XlibWindowData *)data;
    window = (EGLNativeWindowType)xlib->window;
  }
  else if(system == eWindowingSystem_Unknown)
  {
    // allow undefined so that internally we can create a window-less context
    Display *dpy = XOpenDisplay(NULL);

    if(dpy == NULL)
      return 0;
  }
  else
  {
    RDCERR("Unexpected window system %u", system);
  }

  EGLDisplay eglDisplay = eglGetDisplayProc(EGL_DEFAULT_DISPLAY);
  RDCASSERT(eglDisplay);

  static const EGLint configAttribs[] = {EGL_RED_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES3_BIT,
                                         EGL_SURFACE_TYPE,
                                         EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
                                         EGL_NONE};
  EGLint numConfigs;
  EGLConfig config;

  if(!eglChooseConfigProc(eglDisplay, configAttribs, &config, 1, &numConfigs))
  {
    RDCERR("Couldn't find a suitable EGL config");
    return 0;
  }

  static const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                      EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

  EGLContext ctx = eglCreateContextProc(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);

  if(ctx == NULL)
  {
    RDCERR("Couldn't create GL ES context");
    return 0;
  }

  EGLSurface surface = 0;

  if(window != 0)
  {
    surface = eglCreateWindowSurfaceProc(eglDisplay, config, window, NULL);
  }
  else
  {
    static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
    surface = eglCreatePbufferSurfaceProc(eglDisplay, config, pbAttribs);
  }

  RDCASSERT(surface != NULL);

  OutputWindow win;
  win.egl_dpy = eglDisplay;
  win.egl_ctx = ctx;
  win.egl_wnd = surface;

  eglQuerySurfaceProc(eglDisplay, surface, EGL_WIDTH, &win.width);
  eglQuerySurfaceProc(eglDisplay, surface, EGL_HEIGHT, &win.height);

  MakeCurrentReplayContext(&win);
  InitOutputWindow(win);
  CreateOutputWindowBackbuffer(win, depth);

  uint64_t ret = m_OutputWindowID++;

  m_OutputWindows[ret] = win;

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

  eglMakeCurrentProc(outw.egl_dpy, 0L, 0L, NULL);
  eglDestroyContextProc(outw.egl_dpy, outw.egl_ctx);

  m_OutputWindows.erase(it);
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  eglQuerySurfaceProc(outw.egl_dpy, outw.egl_wnd, EGL_WIDTH, &w);
  eglQuerySurfaceProc(outw.egl_dpy, outw.egl_wnd, EGL_HEIGHT, &h);
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

  // Query the required EGL functions
  if(eglCreateContextProc == NULL)
  {
    eglGetProcAddressProc = (PFN_eglGetProcAddress)dlsym(RTLD_NEXT, "eglGetProcAddress");
    eglChooseConfigProc = (PFN_eglChooseConfig)dlsym(RTLD_NEXT, "eglChooseConfig");
    eglInitializeProc = (PFN_eglInitialize)dlsym(RTLD_NEXT, "eglInitialize");
    eglBindAPIProc = (PFN_eglBindAPI)dlsym(RTLD_NEXT, "eglBindAPI");
    eglGetDisplayProc = (PFN_eglGetDisplay)dlsym(RTLD_NEXT, "eglGetDisplay");
    eglCreateContextProc = (PFN_eglCreateContext)dlsym(RTLD_NEXT, "eglCreateContext");
    eglMakeCurrentProc = (PFN_eglMakeCurrent)dlsym(RTLD_NEXT, "eglMakeCurrent");
    eglSwapBuffersProc = (PFN_eglSwapBuffers)dlsym(RTLD_NEXT, "eglSwapBuffers");
    eglDestroyContextProc = (PFN_eglDestroyContext)dlsym(RTLD_NEXT, "eglDestroyContext");
    eglDestroySurfaceProc = (PFN_eglDestroySurface)dlsym(RTLD_NEXT, "eglDestroySurface");
    eglQuerySurfaceProc = (PFN_eglQuerySurface)dlsym(RTLD_NEXT, "eglQuerySurface");
    eglCreatePbufferSurfaceProc =
        (PFN_eglCreatePbufferSurface)dlsym(RTLD_NEXT, "eglCreatePbufferSurface");
    eglCreateWindowSurfaceProc =
        (PFN_eglCreateWindowSurface)dlsym(RTLD_NEXT, "eglCreateWindowSurface");

    if(eglGetProcAddressProc == NULL || eglBindAPIProc == NULL || eglGetDisplayProc == NULL ||
       eglCreateContextProc == NULL || eglMakeCurrentProc == NULL || eglSwapBuffersProc == NULL ||
       eglDestroyContextProc == NULL || eglDestroySurfaceProc == NULL ||
       eglQuerySurfaceProc == NULL || eglCreatePbufferSurfaceProc == NULL ||
       eglCreateWindowSurfaceProc == NULL || eglChooseConfigProc == NULL)
    {
      RDCERR(
          "Couldn't find required function addresses, eglGetProcAddress eglCreateContext"
          "eglSwapBuffers (etc.)");
      return eReplayCreate_APIInitFailed;
    }
  }

  GLInitParams initParams;
  RDCDriver driverType = RDC_OpenGL;
  string driverName = "OpenGL";
  uint64_t machineIdent = 0;

  if(logfile)
  {
    auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, machineIdent,
                                                   (RDCInitParams *)&initParams);
    if(status != eReplayCreate_Success)
      return status;
  }

  Display *dpy = XOpenDisplay(NULL);

  if(dpy == NULL)
  {
    RDCERR("Couldn't open default X display");
    return eReplayCreate_APIInitFailed;
  }

  eglBindAPIProc(EGL_OPENGL_ES_API);

  EGLDisplay eglDisplay = eglGetDisplayProc(EGL_DEFAULT_DISPLAY);
  if(!eglDisplay)
  {
    RDCERR("Couldn't open default EGL display");
    return eReplayCreate_APIInitFailed;
  }

  int major, minor;
  eglInitializeProc(eglDisplay, &major, &minor);

  static const EGLint configAttribs[] = {EGL_RED_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES3_BIT,
                                         EGL_SURFACE_TYPE,
                                         EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
                                         EGL_NONE};
  EGLint numConfigs;
  EGLConfig config;

  if(!eglChooseConfigProc(eglDisplay, configAttribs, &config, 1, &numConfigs))
  {
    RDCERR("Couldn't find a suitable EGL config");
    return eReplayCreate_APIInitFailed;
  }

  static const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                      EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

  GLReplay::PreContextInitCounters();

  EGLContext ctx = eglCreateContextProc(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
  if(ctx == NULL)
  {
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't create GL ES 3.x context - RenderDoc requires OpenGL ES 3.x availability");
    return eReplayCreate_APIHardwareUnsupported;
  }

  static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
  EGLSurface pbuffer = eglCreatePbufferSurfaceProc(eglDisplay, config, pbAttribs);

  if(pbuffer == NULL)
  {
    RDCERR("Couldn't create a suitable PBuffer");
    eglDestroySurfaceProc(eglDisplay, pbuffer);
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIInitFailed;
  }

  EGLBoolean res = eglMakeCurrentProc(eglDisplay, pbuffer, pbuffer, ctx);
  if(!res)
  {
    RDCERR("Couldn't active the created GL ES context");
    eglDestroySurfaceProc(eglDisplay, pbuffer);
    eglDestroyContextProc(eglDisplay, ctx);
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIInitFailed;
  }

  // TODO: add extesion check just like in the GL case.

  const GLHookSet &real = GetRealGLFunctionsEGL();
  bool extensionsValidated = ValidateFunctionPointers(real);
  if(!extensionsValidated)
  {
    eglDestroySurfaceProc(eglDisplay, pbuffer);
    eglDestroyContextProc(eglDisplay, ctx);
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIHardwareUnsupported;
  }

  WrappedOpenGL *gl = new WrappedOpenGL(logfile, real);
  gl->Initialise(initParams);

  if(gl->GetSerialiser()->HasError())
  {
    delete gl;
    return eReplayCreate_FileIOFailed;
  }

  RDCLOG("Created OPEN GL ES replay device.");
  GLReplay *replay = gl->GetReplay();
  replay->SetProxy(logfile == NULL);
  GLWindowingData data;
  data.egl_dpy = eglDisplay;
  data.egl_ctx = ctx;
  data.egl_wnd = pbuffer;
  replay->SetReplayData(data);

  *driver = (IReplayDriver *)replay;
  return eReplayCreate_Success;
}
