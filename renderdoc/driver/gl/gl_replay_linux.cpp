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

void GLReplay::MakeCurrentReplayContext(GLWindowingData *ctx)
{
  static GLWindowingData *prev = NULL;

  if(glXMakeContextCurrentProc && ctx && ctx != prev)
  {
    prev = ctx;
    glXMakeContextCurrentProc(ctx->dpy, ctx->wnd, ctx->wnd, ctx->ctx);
    m_pDriver->ActivateContext(*ctx);
  }
}

void GLReplay::SwapBuffers(GLWindowingData *ctx)
{
  glXSwapProc(ctx->dpy, ctx->wnd);
}

void GLReplay::CloseReplayContext()
{
  if(glXDestroyCtxProc)
  {
    glXMakeContextCurrentProc(m_ReplayCtx.dpy, 0L, 0L, NULL);
    glXDestroyCtxProc(m_ReplayCtx.dpy, m_ReplayCtx.ctx);
  }
}

uint64_t GLReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  Display *dpy = NULL;
  Drawable draw = 0;

  if(system == eWindowingSystem_Xlib)
  {
#if ENABLED(RDOC_XLIB)
    XlibWindowData *xlib = (XlibWindowData *)data;

    dpy = xlib->display;
    draw = xlib->window;
#else
    RDCERR(
        "Xlib windowing system data passed in, but support is not compiled in. GL must have xlib "
        "support compiled in");
#endif
  }
  else if(system == eWindowingSystem_Unknown)
  {
    // allow undefined so that internally we can create a window-less context
    dpy = XOpenDisplay(NULL);

    if(dpy == NULL)
      return 0;
  }
  else
  {
    RDCERR("Unexpected window system %u", system);
  }

  static int visAttribs[] = {GLX_X_RENDERABLE,
                             True,
                             GLX_DRAWABLE_TYPE,
                             GLX_WINDOW_BIT,
                             GLX_RENDER_TYPE,
                             GLX_RGBA_BIT,
                             GLX_X_VISUAL_TYPE,
                             GLX_TRUE_COLOR,
                             GLX_RED_SIZE,
                             8,
                             GLX_GREEN_SIZE,
                             8,
                             GLX_BLUE_SIZE,
                             8,
                             GLX_DOUBLEBUFFER,
                             True,
                             GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB,
                             True,
                             0};
  int numCfgs = 0;
  GLXFBConfig *fbcfg = glXChooseFBConfigProc(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

  if(fbcfg == NULL)
  {
    XCloseDisplay(dpy);
    RDCERR("Couldn't choose default framebuffer config");
    return eReplayCreate_APIInitFailed;
  }

  if(draw != 0)
  {
    // Choose FB config with a GLX_VISUAL_ID that matches the X screen.
    VisualID visualid_correct = DefaultVisual(dpy, DefaultScreen(dpy))->visualid;
    for(int i = 0; i < numCfgs; i++)
    {
      int visualid;
      glXGetFBConfigAttrib(dpy, fbcfg[i], GLX_VISUAL_ID, &visualid);
      if((VisualID)visualid == visualid_correct)
      {
        fbcfg[0] = fbcfg[i];
        break;
      }
    }
  }

  int attribs[64] = {0};
  int i = 0;

  attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
  attribs[i++] = 4;
  attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
  attribs[i++] = 3;
  attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
  attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;
#else
  attribs[i++] = 0;
#endif
  attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
  attribs[i++] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

  GLXContext ctx = glXCreateContextAttribsProc(dpy, fbcfg[0], m_ReplayCtx.ctx, true, attribs);

  if(ctx == NULL)
  {
    XCloseDisplay(dpy);
    RDCERR("Couldn't create 4.3 context - RenderDoc requires OpenGL 4.3 availability");
    return 0;
  }

  GLXDrawable wnd = 0;

  if(draw == 0)
  {
    // don't care about pbuffer properties as we won't render directly to this
    int pbAttribs[] = {GLX_PBUFFER_WIDTH, 32, GLX_PBUFFER_HEIGHT, 32, 0};

    wnd = glXCreatePbufferProc(dpy, fbcfg[0], pbAttribs);
  }
  else
  {
    wnd = glXCreateWindow(dpy, fbcfg[0], draw, 0);
  }

  XFree(fbcfg);

  OutputWindow win;
  win.dpy = dpy;
  win.ctx = ctx;
  win.wnd = wnd;

  glXQueryDrawableProc(dpy, wnd, GLX_WIDTH, (unsigned int *)&win.width);
  glXQueryDrawableProc(dpy, wnd, GLX_HEIGHT, (unsigned int *)&win.height);

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

  glXMakeContextCurrentProc(outw.dpy, 0L, 0L, NULL);
  glXDestroyCtxProc(outw.dpy, outw.ctx);

  m_OutputWindows.erase(it);
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  glXQueryDrawableProc(outw.dpy, outw.wnd, GLX_WIDTH, (unsigned int *)&w);
  glXQueryDrawableProc(outw.dpy, outw.wnd, GLX_HEIGHT, (unsigned int *)&h);
}

bool GLReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  GLNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

const GLHookSet &GetRealGLFunctions();

ReplayCreateStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
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
      return eReplayCreate_APIInitFailed;
    }

    glXCreateContextAttribsProc = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetFuncProc(
        (const GLubyte *)"glXCreateContextAttribsARB");
    glXMakeContextCurrentProc =
        (PFNGLXMAKECONTEXTCURRENTPROC)glXGetFuncProc((const GLubyte *)"glXMakeContextCurrent");

    if(glXCreateContextAttribsProc == NULL || glXMakeContextCurrentProc == NULL)
    {
      RDCERR(
          "Couldn't get glx function addresses, glXCreateContextAttribsARB glXMakeContextCurrent");
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

  int attribs[64] = {0};
  int i = 0;

  GLReplay::PreContextInitCounters();

  attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
  attribs[i++] = 4;
  attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
  attribs[i++] = 3;
  attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
#if ENABLED(RDOC_DEVEL)
  attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;
#else
  attribs[i++] = 0;
#endif
  attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
  attribs[i++] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

  Display *dpy = XOpenDisplay(NULL);

  if(dpy == NULL)
  {
    RDCERR("Couldn't open default X display");
    return eReplayCreate_APIInitFailed;
  }

  // don't need to care about the fb config as we won't be using the default framebuffer
  // (backbuffer)
  static int visAttribs[] = {0};
  int numCfgs = 0;
  GLXFBConfig *fbcfg = glXChooseFBConfigProc(dpy, DefaultScreen(dpy), visAttribs, &numCfgs);

  if(fbcfg == NULL)
  {
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't choose default framebuffer config");
    return eReplayCreate_APIInitFailed;
  }

  GLXContext ctx = glXCreateContextAttribsProc(dpy, fbcfg[0], 0, true, attribs);

  if(ctx == NULL)
  {
    XFree(fbcfg);
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't create 4.3 context - RenderDoc requires OpenGL 4.3 availability");
    return eReplayCreate_APIHardwareUnsupported;
  }

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
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't make pbuffer & context current");
    return eReplayCreate_APIInitFailed;
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
    XCloseDisplay(dpy);
    GLReplay::PostContextShutdownCounters();
    return eReplayCreate_APIInitFailed;
  }
  else
  {
    // eventually we want to emulate EXT_dsa on replay if it isn't present, but for
    // now we just require it.
    bool dsa = false;
    bool bufstorage = false;

    if(getStr)
      RDCLOG("Running GL replay on: %s / %s / %s", getStr(eGL_VENDOR), getStr(eGL_RENDERER),
             getStr(eGL_VERSION));

    GLint numExts = 0;
    getInt(eGL_NUM_EXTENSIONS, &numExts);
    for(GLint e = 0; e < numExts; e++)
    {
      const char *ext = (const char *)getStri(eGL_EXTENSIONS, (GLuint)e);

      RDCLOG("Extension % 3d: %s", e, ext);

      if(!strcmp(ext, "GL_EXT_direct_state_access"))
        dsa = true;
      if(!strcmp(ext, "GL_ARB_buffer_storage"))
        bufstorage = true;
    }

    if(!dsa)
      RDCERR(
          "RenderDoc requires EXT_direct_state_access availability, and it is not reported. Try "
          "updating your drivers.");

    if(!bufstorage)
      RDCERR(
          "RenderDoc requires ARB_buffer_storage availability, and it is not reported. Try "
          "updating your drivers.");

    if(!dsa || !bufstorage)
    {
      glXDestroyPbufferProc(dpy, pbuffer);
      glXDestroyCtxProc(dpy, ctx);
      XFree(fbcfg);
      XCloseDisplay(dpy);
      GLReplay::PostContextShutdownCounters();
      return eReplayCreate_APIHardwareUnsupported;
    }
  }

  WrappedOpenGL *gl = new WrappedOpenGL(logfile, GetRealGLFunctions());
  gl->Initialise(initParams);

  if(gl->GetSerialiser()->HasError())
  {
    delete gl;
    return eReplayCreate_FileIOFailed;
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
  return eReplayCreate_Success;
}
