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
#include "driver/gl/gl_driver.h"
#include "driver/gl/glx_dispatch_table.h"
#include "hooks/hooks.h"

namespace Keyboard
{
void CloneDisplay(Display *dpy);
}

class GLXHook : LibraryHook
{
public:
  GLXHook() : driver(GetGLPlatform()) {}
  void RegisterHooks();

  void UpdateWindowSize(GLWindowingData data, Display *dpy, GLXDrawable drawable)
  {
    if(!data.ctx || !drawable)
      return;

    unsigned int width = 1, height = 1;
    GLX.glXQueryDrawable(dpy, drawable, GLX_WIDTH, &width);
    GLX.glXQueryDrawable(dpy, drawable, GLX_HEIGHT, &height);

    GLInitParams &params = driver.GetInitParams(data);
    params.width = width;
    params.height = height;
  }

  // default to RTLD_NEXT for GLX lookups if we haven't gotten a more specific library handle
  void *handle = RTLD_NEXT;
  WrappedOpenGL driver;
  std::set<GLXContext> contexts;
} glxhook;

HOOK_EXPORT GLXContext glXCreateContext_renderdoc_hooked(Display *dpy, XVisualInfo *vis,
                                                         GLXContext shareList, Bool direct)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXCreateContext)
      GLX.PopulateForReplay();

    return GLX.glXCreateContext(dpy, vis, shareList, direct);
  }

  GLXContext ret = GLX.glXCreateContext(dpy, vis, shareList, direct);

  // don't continue if context creation failed
  if(!ret)
    return ret;

  GLInitParams init;

  init.width = 0;
  init.height = 0;

  int value = 0;

  Keyboard::CloneDisplay(dpy);

  GLX.glXGetConfig(dpy, vis, GLX_BUFFER_SIZE, &value);
  init.colorBits = value;
  GLX.glXGetConfig(dpy, vis, GLX_DEPTH_SIZE, &value);
  init.depthBits = value;
  GLX.glXGetConfig(dpy, vis, GLX_STENCIL_SIZE, &value);
  init.stencilBits = value;
  value = 1;    // default to srgb
  GLX.glXGetConfig(dpy, vis, GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, &value);
  init.isSRGB = value;
  value = 1;
  GLX.glXGetConfig(dpy, vis, GLX_SAMPLES_ARB, &value);
  init.multiSamples = RDCMAX(1, value);

  GLWindowingData data;
  data.dpy = dpy;
  data.wnd = (GLXDrawable)NULL;
  data.ctx = ret;
  data.cfg = vis;

  {
    SCOPED_LOCK(glLock);
    glxhook.driver.CreateContext(data, shareList, init, false, false);
  }

  return ret;
}

HOOK_EXPORT void glXDestroyContext_renderdoc_hooked(Display *dpy, GLXContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXDestroyContext)
      GLX.PopulateForReplay();

    return GLX.glXDestroyContext(dpy, ctx);
  }

  {
    SCOPED_LOCK(glLock);
    glxhook.driver.DeleteContext(ctx);
    glxhook.contexts.erase(ctx);
  }

  GLX.glXDestroyContext(dpy, ctx);
}

HOOK_EXPORT GLXContext glXCreateContextAttribsARB_renderdoc_hooked(Display *dpy, GLXFBConfig config,
                                                                   GLXContext shareList, Bool direct,
                                                                   const int *attribList)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXCreateContextAttribsARB)
      GLX.PopulateForReplay();

    return GLX.glXCreateContextAttribsARB(dpy, config, shareList, direct, attribList);
  }

  int defaultAttribList[] = {0};

  const int *attribs = attribList ? attribList : defaultAttribList;
  std::vector<int> attribVec;

  // modify attribs to our liking
  {
    bool flagsFound = false;
    const int *a = attribs;
    while(*a)
    {
      int name = *a++;
      int val = *a++;

      if(name == GLX_CONTEXT_FLAGS_ARB)
      {
        if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
          val |= GLX_CONTEXT_DEBUG_BIT_ARB;
        else
          val &= ~GLX_CONTEXT_DEBUG_BIT_ARB;

        // remove NO_ERROR bit
        val &= ~GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR;

        flagsFound = true;
      }

      attribVec.push_back(name);
      attribVec.push_back(val);
    }

    if(!flagsFound && RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      attribVec.push_back(GLX_CONTEXT_FLAGS_ARB);
      attribVec.push_back(GLX_CONTEXT_DEBUG_BIT_ARB);
    }

    attribVec.push_back(0);

    attribs = &attribVec[0];
  }

  RDCDEBUG("glXCreateContextAttribsARB:");

  bool core = false, es = false;

  int *a = (int *)attribs;
  while(*a)
  {
    RDCDEBUG("%x: %d", a[0], a[1]);

    if(a[0] == GLX_CONTEXT_PROFILE_MASK_ARB)
    {
      core = (a[1] & GLX_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
      es = (a[1] & (GLX_CONTEXT_ES_PROFILE_BIT_EXT | GLX_CONTEXT_ES2_PROFILE_BIT_EXT)) != 0;
    }

    a += 2;
  }

  if(es)
  {
    glxhook.driver.SetDriverType(RDCDriver::OpenGLES);
    core = true;
  }

  GLXContext ret = GLX.glXCreateContextAttribsARB(dpy, config, shareList, direct, attribs);

  // don't continue if context creation failed
  if(!ret)
    return ret;

  XVisualInfo *vis = GLX.glXGetVisualFromFBConfig(dpy, config);

  GLInitParams init;

  init.width = 0;
  init.height = 0;

  int value = 0;

  Keyboard::CloneDisplay(dpy);

  GLX.glXGetConfig(dpy, vis, GLX_BUFFER_SIZE, &value);
  init.colorBits = value;
  GLX.glXGetConfig(dpy, vis, GLX_DEPTH_SIZE, &value);
  init.depthBits = value;
  GLX.glXGetConfig(dpy, vis, GLX_STENCIL_SIZE, &value);
  init.stencilBits = value;
  value = 1;    // default to srgb
  GLX.glXGetConfig(dpy, vis, GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, &value);
  init.isSRGB = value;
  value = 1;
  GLX.glXGetConfig(dpy, vis, GLX_SAMPLES_ARB, &value);
  init.multiSamples = RDCMAX(1, value);

  GLWindowingData data;
  data.dpy = dpy;
  data.wnd = (GLXDrawable)NULL;
  data.ctx = ret;
  data.cfg = vis;

  {
    SCOPED_LOCK(glLock);
    glxhook.driver.CreateContext(data, shareList, init, core, true);
  }

  XFree(vis);

  return ret;
}

HOOK_EXPORT Bool glXMakeCurrent_renderdoc_hooked(Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXMakeCurrent)
      GLX.PopulateForReplay();

    return GLX.glXMakeCurrent(dpy, drawable, ctx);
  }

  Bool ret = GLX.glXMakeCurrent(dpy, drawable, ctx);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    SetDriverForHooks(&glxhook.driver);

    if(ctx && glxhook.contexts.find(ctx) == glxhook.contexts.end())
    {
      glxhook.contexts.insert(ctx);

      FetchEnabledExtensions();

      // see gl_emulated.cpp
      GL.EmulateUnsupportedFunctions();
      GL.EmulateRequiredExtensions();
      GL.DriverForEmulation(&glxhook.driver);
    }

    GLWindowingData data;
    data.dpy = dpy;
    data.wnd = drawable;
    data.ctx = ctx;
    data.cfg = NULL;

    GLXFBConfig *config = NULL;

    if(ctx)
    {
      int fbconfigid = -1;
      GLX.glXQueryContext(dpy, ctx, GLX_FBCONFIG_ID, &fbconfigid);

      int attribs[] = {GLX_FBCONFIG_ID, fbconfigid, 0};

      int numElems = 0;
      config = GLX.glXChooseFBConfig(dpy, DefaultScreen(dpy), attribs, &numElems);

      if(config)
        data.cfg = GLX.glXGetVisualFromFBConfig(dpy, *config);
      else
        data.cfg = NULL;
    }

    glxhook.driver.ActivateContext(data);

    glxhook.UpdateWindowSize(data, dpy, drawable);

    if(config)
      XFree(config);
    if(data.cfg)
      XFree(data.cfg);
  }

  return ret;
}

HOOK_EXPORT Bool glXMakeContextCurrent_renderdoc_hooked(Display *dpy, GLXDrawable draw,
                                                        GLXDrawable read, GLXContext ctx)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXMakeContextCurrent)
      GLX.PopulateForReplay();

    return GLX.glXMakeContextCurrent(dpy, draw, read, ctx);
  }

  Bool ret = GLX.glXMakeContextCurrent(dpy, draw, read, ctx);

  if(ret)
  {
    SCOPED_LOCK(glLock);

    SetDriverForHooks(&glxhook.driver);

    if(ctx && glxhook.contexts.find(ctx) == glxhook.contexts.end())
    {
      glxhook.contexts.insert(ctx);

      FetchEnabledExtensions();

      // see gl_emulated.cpp
      GL.EmulateUnsupportedFunctions();
      GL.EmulateRequiredExtensions();
      GL.DriverForEmulation(&glxhook.driver);
    }

    GLWindowingData data;
    data.dpy = dpy;
    data.wnd = draw;
    data.ctx = ctx;

    GLXFBConfig *config = NULL;

    if(ctx)
    {
      int fbconfigid = -1;
      GLX.glXQueryContext(dpy, ctx, GLX_FBCONFIG_ID, &fbconfigid);

      int attribs[] = {GLX_FBCONFIG_ID, fbconfigid, 0};

      int numElems = 0;
      config = GLX.glXChooseFBConfig(dpy, DefaultScreen(dpy), attribs, &numElems);

      if(config)
        data.cfg = GLX.glXGetVisualFromFBConfig(dpy, *config);
      else
        data.cfg = NULL;
    }

    glxhook.driver.ActivateContext(data);

    glxhook.UpdateWindowSize(data, dpy, draw);

    if(config)
      XFree(config);
    if(data.cfg)
      XFree(data.cfg);
  }

  return ret;
}

HOOK_EXPORT void glXSwapBuffers_renderdoc_hooked(Display *dpy, GLXDrawable drawable)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXSwapBuffers)
      GLX.PopulateForReplay();

    return GLX.glXSwapBuffers(dpy, drawable);
  }

  SCOPED_LOCK(glLock);

  {
    GLWindowingData data;
    data.dpy = dpy;
    data.wnd = drawable;
    data.ctx = GLX.glXGetCurrentContext();
    data.cfg = NULL;

    glxhook.UpdateWindowSize(data, dpy, drawable);
  }

  glxhook.driver.SwapBuffers((void *)drawable);

  GLX.glXSwapBuffers(dpy, drawable);
}

HOOK_EXPORT __GLXextFuncPtr glXGetProcAddress_renderdoc_hooked(const GLubyte *f)
{
  if(RenderDoc::Inst().IsReplayApp())
  {
    if(!GLX.glXGetProcAddress)
      GLX.PopulateForReplay();

    return GLX.glXGetProcAddress(f);
  }

  const char *func = (const char *)f;

  __GLXextFuncPtr realFunc = NULL;
  {
    ScopedSuppressHooking suppress;
    realFunc = GLX.glXGetProcAddress(f);
  }

  // if the real context doesn't support this function, and we don't provide an implementation fully
  // ourselves, return NULL
  if(realFunc == NULL && !FullyImplementedFunction(func))
    return realFunc;

  // return our glX hooks
  if(!strcmp(func, "glXCreateContext"))
    return (__GLXextFuncPtr)&glXCreateContext_renderdoc_hooked;
  if(!strcmp(func, "glXDestroyContext"))
    return (__GLXextFuncPtr)&glXDestroyContext_renderdoc_hooked;
  if(!strcmp(func, "glXCreateContextAttribsARB"))
    return (__GLXextFuncPtr)&glXCreateContextAttribsARB_renderdoc_hooked;
  if(!strcmp(func, "glXMakeCurrent"))
    return (__GLXextFuncPtr)&glXMakeCurrent_renderdoc_hooked;
  if(!strcmp(func, "glXMakeContextCurrent"))
    return (__GLXextFuncPtr)&glXMakeContextCurrent_renderdoc_hooked;
  if(!strcmp(func, "glXSwapBuffers"))
    return (__GLXextFuncPtr)&glXSwapBuffers_renderdoc_hooked;
  if(!strcmp(func, "glXGetProcAddress"))
    return (__GLXextFuncPtr)&glXGetProcAddress_renderdoc_hooked;
  if(!strcmp(func, "glXGetProcAddressARB"))
    return (__GLXextFuncPtr)&glXGetProcAddress_renderdoc_hooked;

  // any other GLX functions are safe to pass through unchanged
  if(!strncmp(func, "glX", 3))
    return realFunc;

  // otherwise, consult our database of hooks
  return (__GLXextFuncPtr)HookedGetProcAddress(func, (void *)realFunc);
}

HOOK_EXPORT __GLXextFuncPtr glXGetProcAddressARB_renderdoc_hooked(const GLubyte *f)
{
  return glXGetProcAddress_renderdoc_hooked(f);
}

// on posix systems, someone might declare a global variable with the same name as a function. When
// doing this, it might mean that our code for "&glXSwapBuffers" looking up that global symbol will
// instead find the location fo the function pointer instead of our hook function. For this reason
// we always refer to the _renderdoc_hooked name, but we still must export the functions under their
// real names and just forward to the hook implementation.

HOOK_EXPORT GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList,
                                        Bool direct)
{
  return glXCreateContext_renderdoc_hooked(dpy, vis, shareList, direct);
}

HOOK_EXPORT void glXDestroyContext(Display *dpy, GLXContext ctx)
{
  return glXDestroyContext_renderdoc_hooked(dpy, ctx);
}

HOOK_EXPORT GLXContext glXCreateContextAttribsARB(Display *dpy, GLXFBConfig config,
                                                  GLXContext shareList, Bool direct,
                                                  const int *attribList)
{
  return glXCreateContextAttribsARB_renderdoc_hooked(dpy, config, shareList, direct, attribList);
}

HOOK_EXPORT Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
  return glXMakeCurrent_renderdoc_hooked(dpy, drawable, ctx);
}

HOOK_EXPORT Bool glXMakeContextCurrent(Display *dpy, GLXDrawable draw, GLXDrawable read,
                                       GLXContext ctx)
{
  return glXMakeContextCurrent_renderdoc_hooked(dpy, draw, read, ctx);
}

HOOK_EXPORT void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
  return glXSwapBuffers_renderdoc_hooked(dpy, drawable);
}

HOOK_EXPORT __GLXextFuncPtr glXGetProcAddress(const GLubyte *f)
{
  return glXGetProcAddress_renderdoc_hooked(f);
}

HOOK_EXPORT __GLXextFuncPtr glXGetProcAddressARB(const GLubyte *f)
{
  return glXGetProcAddressARB_renderdoc_hooked(f);
}

// on posix systems we need to export the whole of the GLX API, since we will have redirected any
// dlopen() for libGL.so to ourselves, and dlsym() for any of these entry points must return a
// valid function. We don't need to intercept them, so we just pass it along

#define GLX_PASSTHRU_0(ret, function)                                               \
  typedef ret (*CONCAT(function, _hooktype))();                                     \
  extern "C" __attribute__((visibility("default"))) ret function()                  \
  {                                                                                 \
    CONCAT(function, _hooktype)                                                     \
    real = (CONCAT(function, _hooktype))dlsym(glxhook.handle, STRINGIZE(function)); \
    return real();                                                                  \
  }

#define GLX_PASSTHRU_1(ret, function, t1, p1)                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1)             \
  {                                                                                 \
    CONCAT(function, _hooktype)                                                     \
    real = (CONCAT(function, _hooktype))dlsym(glxhook.handle, STRINGIZE(function)); \
    return real(p1);                                                                \
  }

#define GLX_PASSTHRU_2(ret, function, t1, p1, t2, p2)                               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                               \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2)      \
  {                                                                                 \
    CONCAT(function, _hooktype)                                                     \
    real = (CONCAT(function, _hooktype))dlsym(glxhook.handle, STRINGIZE(function)); \
    return real(p1, p2);                                                            \
  }

#define GLX_PASSTHRU_3(ret, function, t1, p1, t2, p2, t3, p3)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    CONCAT(function, _hooktype)                                                       \
    real = (CONCAT(function, _hooktype))dlsym(glxhook.handle, STRINGIZE(function));   \
    return real(p1, p2, p3);                                                          \
  }

#define GLX_PASSTHRU_4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                        \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    CONCAT(function, _hooktype)                                                              \
    real = (CONCAT(function, _hooktype))dlsym(glxhook.handle, STRINGIZE(function));          \
    return real(p1, p2, p3, p4);                                                             \
  }

#define GLX_PASSTHRU_5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    CONCAT(function, _hooktype)                                                                     \
    real = (CONCAT(function, _hooktype))dlsym(glxhook.handle, STRINGIZE(function));                 \
    return real(p1, p2, p3, p4, p5);                                                                \
  }

GLX_PASSTHRU_3(GLXFBConfig *, glXGetFBConfigs, Display *, dpy, int, screen, int *, nelements);
GLX_PASSTHRU_4(int, glXGetFBConfigAttrib, Display *, dpy, GLXFBConfig, config, int, attribute,
               int *, value);
GLX_PASSTHRU_2(XVisualInfo *, glXGetVisualFromFBConfig, Display *, dpy, GLXFBConfig, config);
GLX_PASSTHRU_4(GLXFBConfig *, glXChooseFBConfig, Display *, dpy, int, screen, const int *,
               attrib_list, int *, nelements);
GLX_PASSTHRU_3(XVisualInfo *, glXChooseVisual, Display *, dpy, int, screen, int *, attrib_list);
GLX_PASSTHRU_4(int, glXGetConfig, Display *, dpy, XVisualInfo *, visual, int, attribute, int *,
               value);
GLX_PASSTHRU_5(GLXContext, glXCreateNewContext, Display *, dpy, GLXFBConfig, config, int,
               renderType, GLXContext, shareList, Bool, direct);
GLX_PASSTHRU_4(void, glXCopyContext, Display *, dpy, GLXContext, source, GLXContext, dest,
               unsigned long, mask);
GLX_PASSTHRU_4(int, glXQueryContext, Display *, dpy, GLXContext, ctx, int, attribute, int *, value);
GLX_PASSTHRU_3(void, glXSelectEvent, Display *, dpy, GLXDrawable, draw, unsigned long, event_mask);
GLX_PASSTHRU_3(void, glXGetSelectedEvent, Display *, dpy, GLXDrawable, draw, unsigned long *,
               event_mask);
GLX_PASSTHRU_4(void, glXQueryDrawable, Display *, dpy, GLXDrawable, draw, int, attribute,
               unsigned int *, value);
GLX_PASSTHRU_0(GLXContext, glXGetCurrentContext);
GLX_PASSTHRU_0(GLXDrawable, glXGetCurrentDrawable);
GLX_PASSTHRU_0(GLXDrawable, glXGetCurrentReadDrawable);
GLX_PASSTHRU_0(Display *, glXGetCurrentDisplay);
GLX_PASSTHRU_3(const char *, glXQueryServerString, Display *, dpy, int, screen, int, name);
GLX_PASSTHRU_2(const char *, glXGetClientString, Display *, dpy, int, name);
GLX_PASSTHRU_2(const char *, glXQueryExtensionsString, Display *, dpy, int, screen);
GLX_PASSTHRU_3(Bool, glXQueryExtension, Display *, dpy, int *, errorBase, int *, eventBase);
GLX_PASSTHRU_3(Bool, glXQueryVersion, Display *, dpy, int *, maj, int *, min);
GLX_PASSTHRU_2(Bool, glXIsDirect, Display *, dpy, GLXContext, ctx);
GLX_PASSTHRU_0(void, glXWaitGL);
GLX_PASSTHRU_0(void, glXWaitX);
GLX_PASSTHRU_4(void, glXUseXFont, Font, font, int, first, int, count, int, list_base);
GLX_PASSTHRU_3(GLXPixmap, glXCreateGLXPixmap, Display *, dpy, XVisualInfo *, visual, Pixmap, pixmap);
GLX_PASSTHRU_2(void, glXDestroyGLXPixmap, Display *, dpy, GLXPixmap, pixmap);
GLX_PASSTHRU_4(GLXPixmap, glXCreatePixmap, Display *, dpy, GLXFBConfig, config, Pixmap, pixmap,
               const int *, attrib_list);
GLX_PASSTHRU_2(void, glXDestroyPixmap, Display *, dpy, GLXPixmap, pixmap);
GLX_PASSTHRU_3(GLXPbuffer, glXCreatePbuffer, Display *, dpy, GLXFBConfig, config, const int *,
               attrib_list);
GLX_PASSTHRU_2(void, glXDestroyPbuffer, Display *, dpy, GLXPbuffer, pbuf);
GLX_PASSTHRU_4(GLXWindow, glXCreateWindow, Display *, dpy, GLXFBConfig, config, Window, window,
               const int *, attrib_list);
GLX_PASSTHRU_2(void, glXDestroyWindow, Display *, dpy, GLXWindow, window);

static void GLXHooked(void *handle)
{
  RDCDEBUG("GLX library hooked");

  // store the handle for any pass-through implementations that need to look up their onward
  // pointers
  glxhook.handle = handle;

  // as a hook callback this is only called while capturing
  RDCASSERT(!RenderDoc::Inst().IsReplayApp());

// fetch non-hooked functions into our dispatch table
#define GLX_FETCH(func) \
  GLX.func = (CONCAT(PFN_, func))Process::GetFunctionAddress(handle, STRINGIZE(func));
  GLX_NONHOOKED_SYMBOLS(GLX_FETCH)
#undef GLX_FETCH

// fetch any functions that weren't directly exported
#define GPA_FUNC(func)                                                                         \
  if(!GLX.func && GLX.glXGetProcAddressARB)                                                    \
    GLX.func = (CONCAT(PFN_, func))GLX.glXGetProcAddressARB((const GLubyte *)STRINGIZE(func)); \
                                                                                               \
  if(!GLX.func && GLX.glXGetProcAddress)                                                       \
    GLX.func = (CONCAT(PFN_, func))GLX.glXGetProcAddress((const GLubyte *)STRINGIZE(func));

  GLX_HOOKED_SYMBOLS(GPA_FUNC)
  GLX_NONHOOKED_SYMBOLS(GPA_FUNC)
#undef GPA_FUNC

  // Now that libGL is loaded, we can immediately fill out any missing functions that weren't
  // library hooked by calling eglGetProcAddress.
  GL.PopulateWithCallback([](const char *funcName) -> void * {
    ScopedSuppressHooking suppress;
    return (void *)GLX.glXGetProcAddress((const GLubyte *)funcName);
  });
}

// declare some of the legacy functions we define as 'hooks' elsewhere.
HOOK_EXPORT void HOOK_CC glPushMatrix();
HOOK_EXPORT void HOOK_CC glLoadIdentity();
HOOK_EXPORT void HOOK_CC glMatrixMode(GLenum);
HOOK_EXPORT void HOOK_CC glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
HOOK_EXPORT void HOOK_CC glPopMatrix();
HOOK_EXPORT void HOOK_CC glBegin(GLenum);
HOOK_EXPORT void HOOK_CC glVertex2f(GLfloat, GLfloat);
HOOK_EXPORT void HOOK_CC glTexCoord2f(GLfloat, GLfloat);
HOOK_EXPORT void HOOK_CC glEnd();

void GLXHook::RegisterHooks()
{
  RDCLOG("Registering GLX hooks");

  // register library hooks
  LibraryHooks::RegisterLibraryHook("libGL.so", &GLXHooked);
  LibraryHooks::RegisterLibraryHook("libGL.so.1", &GLXHooked);
  LibraryHooks::RegisterLibraryHook("libGLX.so", &GLXHooked);
  LibraryHooks::RegisterLibraryHook("libGLX.so.0", &GLXHooked);

// register EGL hooks
#define GLX_REGISTER(func)            \
  LibraryHooks::RegisterFunctionHook( \
      "libGL.so",                     \
      FunctionHook(STRINGIZE(func), (void **)&GLX.func, (void *)&CONCAT(func, _renderdoc_hooked)));
  GLX_HOOKED_SYMBOLS(GLX_REGISTER)
#undef GLX_REGISTER
}
