/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

typedef Bool (*PFNGLXMAKECURRENTPROC)(Display *dpy, GLXDrawable drawable, GLXContext ctx);
typedef void (*PFNGLXDESTROYCONTEXTPROC)(Display *dpy, GLXContext ctx);
typedef void (*PFNGLXSWAPBUFFERSPROC)(Display *dpy, GLXDrawable drawable);

PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfigProc = NULL;
PFNGLXCREATEPBUFFERPROC glXCreatePbufferProc = NULL;
PFNGLXDESTROYPBUFFERPROC glXDestroyPbufferProc = NULL;
PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsProc = NULL;
PFNGLXGETPROCADDRESSPROC glXGetFuncProc = NULL;
PFNGLXMAKECONTEXTCURRENTPROC glXMakeContextCurrentProc = NULL;
PFNGLXQUERYDRAWABLEPROC glXQueryDrawableProc = NULL;
PFNGLXDESTROYCONTEXTPROC glXDestroyCtxProc = NULL;
PFNGLXSWAPBUFFERSPROC glXSwapProc = NULL;

static bool X11ErrorSeen = false;

int NonFatalX11ErrorHandler(Display *display, XErrorEvent *error)
{
  X11ErrorSeen = true;

  return 0;
}

typedef int (*X11ErrorHandler)(Display *display, XErrorEvent *error);

ReplayStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL replay device");

  if(glXCreateContextAttribsProc == NULL)
  {
    glXGetFuncProc = (PFNGLXGETPROCADDRESSPROC)dlsym(RTLD_NEXT, "glXGetProcAddress");
    glXDestroyCtxProc = (PFNGLXDESTROYCONTEXTPROC)dlsym(RTLD_NEXT, "glXDestroyContext");
    glXSwapProc = (PFNGLXSWAPBUFFERSPROC)dlsym(RTLD_NEXT, "glXSwapBuffers");
    glXChooseFBConfigProc = (PFNGLXCHOOSEFBCONFIGPROC)dlsym(RTLD_NEXT, "glXChooseFBConfig");
    glXCreatePbufferProc = (PFNGLXCREATEPBUFFERPROC)dlsym(RTLD_NEXT, "glXCreatePbuffer");
    glXDestroyPbufferProc = (PFNGLXDESTROYPBUFFERPROC)dlsym(RTLD_NEXT, "glXDestroyPbuffer");
    glXQueryDrawableProc = (PFNGLXQUERYDRAWABLEPROC)dlsym(RTLD_NEXT, "glXQueryDrawable");

    if(glXGetFuncProc == NULL || glXDestroyCtxProc == NULL || glXSwapProc == NULL ||
       glXChooseFBConfigProc == NULL || glXCreatePbufferProc == NULL ||
       glXDestroyPbufferProc == NULL || glXQueryDrawableProc == NULL)
    {
      RDCERR(
          "Couldn't find required entry points, glXGetProcAddress glXDestroyContext "
          "glXSwapBuffers");
      return ReplayStatus::APIInitFailed;
    }

    glXCreateContextAttribsProc = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetFuncProc(
        (const GLubyte *)"glXCreateContextAttribsARB");
    glXMakeContextCurrentProc =
        (PFNGLXMAKECONTEXTCURRENTPROC)glXGetFuncProc((const GLubyte *)"glXMakeContextCurrent");

    if(glXCreateContextAttribsProc == NULL || glXMakeContextCurrentProc == NULL)
    {
      RDCERR(
          "Couldn't get glx function addresses, glXCreateContextAttribsARB glXMakeContextCurrent");
      return ReplayStatus::APIInitFailed;
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
    if(status != ReplayStatus::Succeeded)
      return status;
  }

  int attribs[64] = {0};
  int i = 0;

  GLReplay::PreContextInitCounters();

  attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
  int &major = attribs[i];
  attribs[i++] = 0;
  attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
  int &minor = attribs[i];
  attribs[i++] = 0;
  attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
  attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;
#else
  attribs[i++] = 0;
#endif
  attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
  attribs[i++] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

  Display *dpy = RenderDoc::Inst().GetGlobalEnvironment().xlibDisplay;

  if(dpy == NULL)
  {
    RDCERR("Couldn't open default X display");
    return ReplayStatus::APIInitFailed;
  }

  // don't need to care about the fb config as we won't be using the default framebuffer
  // (backbuffer)
  static int visAttribs[] = {0};
  int numCfgs = 0;
  GLXFBConfig *fbcfg = glXChooseFBConfigProc(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

  if(fbcfg == NULL)
  {
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't choose default framebuffer config");
    return ReplayStatus::APIInitFailed;
  }

  GLXContext ctx = NULL;

  // try to create all versions from 4.5 down to 3.2 in order to get the
  // highest versioned context we can
  struct
  {
    int major;
    int minor;
  } versions[] = {
      {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2},
  };

  {
    X11ErrorHandler prev = XSetErrorHandler(&NonFatalX11ErrorHandler);

    for(size_t v = 0; v < ARRAY_COUNT(versions); v++)
    {
      X11ErrorSeen = false;

      major = versions[v].major;
      minor = versions[v].minor;
      ctx = glXCreateContextAttribsProc(dpy, fbcfg[0], 0, true, attribs);

      if(ctx && !X11ErrorSeen)
        break;
    }

    XSetErrorHandler(prev);
  }

  if(ctx == NULL || X11ErrorSeen)
  {
    XFree(fbcfg);
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't create 3.2 context - RenderDoc requires OpenGL 3.2 availability");
    return ReplayStatus::APIHardwareUnsupported;
  }

  GLCoreVersion = major * 10 + minor;

  // don't care about pbuffer properties for same reason as backbuffer
  int pbAttribs[] = {GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0};

  GLXPbuffer pbuffer = glXCreatePbufferProc(dpy, fbcfg[0], pbAttribs);

  XFree(fbcfg);

  Bool res = glXMakeContextCurrentProc(dpy, pbuffer, pbuffer, ctx);

  if(!res)
  {
    glXDestroyPbufferProc(dpy, pbuffer);
    glXDestroyCtxProc(dpy, ctx);
    XFree(fbcfg);
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't make pbuffer & context current");
    return ReplayStatus::APIInitFailed;
  }

  PFNGLGETINTEGERVPROC getInt =
      (PFNGLGETINTEGERVPROC)glXGetFuncProc((const GLubyte *)"glGetIntegerv");
  PFNGLGETSTRINGPROC getStr = (PFNGLGETSTRINGPROC)glXGetFuncProc((const GLubyte *)"glGetString");
  PFNGLGETSTRINGIPROC getStri =
      (PFNGLGETSTRINGIPROC)glXGetFuncProc((const GLubyte *)"glGetStringi");

  if(getInt == NULL || getStr == NULL || getStri == NULL)
  {
    RDCERR("Couldn't get glGetIntegerv (%p), glGetString (%p) or glGetStringi (%p) entry points",
           getInt, getStr, getStri);
    glXDestroyPbufferProc(dpy, pbuffer);
    glXDestroyCtxProc(dpy, ctx);
    XFree(fbcfg);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  const GLHookSet &real = GetRealGLFunctions();

  bool missingExt = CheckReplayContext(getStr, getInt, getStri);

  if(missingExt)
  {
    exit(0);
    glXDestroyPbufferProc(dpy, pbuffer);
    glXDestroyCtxProc(dpy, ctx);
    XFree(fbcfg);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIHardwareUnsupported;
  }

  bool extensionsValidated = ValidateFunctionPointers(real);

  if(!extensionsValidated)
  {
    glXDestroyPbufferProc(dpy, pbuffer);
    glXDestroyCtxProc(dpy, ctx);
    XFree(fbcfg);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIHardwareUnsupported;
  }

  if(getStr)
  {
    const char *vendor = (const char *)getStr(eGL_VENDOR);
    const char *version = (const char *)getStr(eGL_VERSION);

    if(strstr(vendor, "NVIDIA") && strstr(version, "378."))
    {
      RDCLOG("There is a known crash issue on NVIDIA 378.x series drivers.");
      RDCLOG(
          "If you hit a crash after this message, try setting __GL_THREADED_OPTIMIZATIONS=0 or "
          "upgrade to 381.x or newer.");
      RDCLOG("See https://github.com/baldurk/renderdoc/issues/609 for more information.");
    }
  }

  WrappedOpenGL *gl = new WrappedOpenGL(logfile, real, GetGLPlatform());
  gl->Initialise(initParams);

  if(gl->GetSerialiser()->HasError())
  {
    delete gl;
    return ReplayStatus::FileIOFailed;
  }

  RDCLOG("Created device.");
  GLReplay *replay = gl->GetReplay();
  replay->SetProxy(logfile == NULL);
  GLWindowingData data;
  data.dpy = dpy;
  data.ctx = ctx;
  data.wnd = pbuffer;
  replay->SetReplayData(data);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}
