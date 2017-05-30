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

#include <dlfcn.h>
#include <stdio.h>
#include "common/threading.h"
#include "driver/gl/gl_common.h"
#include "driver/gl/gl_driver.h"
#include "hooks/hooks.h"
#include "serialise/string_utils.h"
#include "gl_hooks_linux_shared.h"

typedef __eglMustCastToProperFunctionPointerType (*PFN_eglGetProcAddress)(const char *procname);
typedef EGLBoolean (*PFN_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute,
                                             EGLint *value);
typedef EGLBoolean (*PFN_eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                          EGLConfig *configs, EGLint config_size, EGLint *num_config);
typedef EGLContext (*PFN_eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                           EGLContext share_context, const EGLint *attrib_list);
typedef EGLBoolean (*PFN_eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
typedef EGLSurface (*PFN_eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                  const EGLint *attrib_list);
typedef EGLSurface (*PFN_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                                 EGLNativeWindowType win, const EGLint *attrib_list);
typedef EGLBoolean (*PFN_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
                                          EGLint *value);
typedef EGLBoolean (*PFN_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (*PFN_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                                         EGLContext ctx);
typedef EGLBoolean (*PFN_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
typedef EGLDisplay (*PFN_eglGetDisplay)(EGLNativeDisplayType display_id);

class EGLHook : LibraryHook, public GLPlatform
{
public:
  EGLHook()
  {
    LibraryHooks::GetInstance().RegisterHook("libEGL.so", this);

    RDCEraseEl(GL);

    m_HasHooks = false;

    m_GLDriver = NULL;

    m_EnabledHooks = true;
    m_PopulatedHooks = false;
  }
  ~EGLHook()
  {
    delete m_GLDriver;
    m_GLDriver = NULL;
  }
  static void libHooked(void *realLib);

  bool CreateHooks(const char *libName)
  {
    if(!m_EnabledHooks)
      return false;

    if(libName)
      PosixHookLibrary("libEGL.so", &libHooked);

    bool success = SetupHooks();

    if(!success)
      return false;

    m_HasHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
  const GLHookSet &GetRealGLFunctions()
  {
    if(!m_PopulatedHooks)
      m_PopulatedHooks = PopulateHooks();
    return GL;
  }

  void SetupExportedFunctions()
  {
    // in the replay application we need to call SetupHooks to ensure that all of our exported
    // functions like glXCreateContext etc have the 'real' pointers to call into, otherwise even the
    // replay app will resolve to our hooks first before the real libGL and call in.
    if(RenderDoc::Inst().IsReplayApp())
      SetupHooks();
  }

  void MakeContextCurrent(GLWindowingData data)
  {
    if(eglMakeCurrent_real)
      eglMakeCurrent_real(data.egl_dpy, data.egl_wnd, data.egl_wnd, data.egl_ctx);
  }

  GLWindowingData MakeContext(GLWindowingData share)
  {
    GLWindowingData ret;
    if(eglCreateContext_real)
    {
      const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                   EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

      const EGLint attribs[] = {EGL_RED_SIZE,
                                8,
                                EGL_GREEN_SIZE,
                                8,
                                EGL_BLUE_SIZE,
                                8,
                                EGL_SURFACE_TYPE,
                                EGL_PBUFFER_BIT,
                                EGL_RENDERABLE_TYPE,
                                EGL_OPENGL_ES3_BIT,
                                EGL_CONFORMANT,
                                EGL_OPENGL_ES3_BIT,
                                EGL_COLOR_BUFFER_TYPE,
                                EGL_RGB_BUFFER,
                                EGL_NONE};

      PFN_eglChooseConfig eglChooseConfig =
          (PFN_eglChooseConfig)dlsym(RTLD_NEXT, "eglChooseConfig");
      PFN_eglCreatePbufferSurface eglCreatePbufferSurface =
          (PFN_eglCreatePbufferSurface)dlsym(RTLD_NEXT, "eglCreatePbufferSurface");

      if(eglChooseConfig && eglCreatePbufferSurface)
      {
        EGLConfig config;
        EGLint numConfigs;
        EGLBoolean configFound = eglChooseConfig(share.egl_dpy, attribs, &config, 1, &numConfigs);

        if(configFound)
        {
          const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
          ret.egl_wnd = eglCreatePbufferSurface(share.egl_dpy, config, pbAttribs);
          ret.egl_dpy = share.egl_dpy;
          ret.egl_ctx = eglCreateContext_real(share.egl_dpy, config, share.ctx, ctxAttribs);
        }
      }
    }

    return ret;
  }

  void DeleteContext(GLWindowingData context)
  {
    PFN_eglDestroySurface eglDestroySurface =
        (PFN_eglDestroySurface)dlsym(RTLD_NEXT, "eglDestroySurface");

    if(context.wnd && eglDestroySurface)
      eglDestroySurface(context.egl_dpy, context.egl_wnd);

    if(context.ctx && eglDestroyContext_real)
      eglDestroyContext_real(context.egl_dpy, context.egl_ctx);
  }

  void DeleteReplayContext(GLWindowingData context)
  {
    if(eglDestroyContext_real)
    {
      eglMakeCurrent_real(context.egl_dpy, 0L, 0L, NULL);
      eglDestroyContext_real(context.egl_dpy, context.egl_ctx);
    }
  }

  void SwapBuffers(GLWindowingData context)
  {
    eglSwapBuffers_real(context.egl_dpy, context.egl_wnd);
  }

  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    eglQuerySurface_real(context.egl_dpy, context.egl_wnd, EGL_WIDTH, &w);
    eglQuerySurface_real(context.egl_dpy, context.egl_wnd, EGL_HEIGHT, &h);
  }

  bool IsOutputWindowVisible(GLWindowingData context) { return true; }
  GLWindowingData MakeOutputWindow(WindowingSystem system, void *data, bool depth,
                                   GLWindowingData share_context)
  {
    GLWindowingData ret;
    EGLNativeWindowType window = 0;

    switch(system)
    {
#if ENABLED(RDOC_ANDROID)
      case WindowingSystem::Android: window = (EGLNativeWindowType)data; break;
#elif ENABLED(RDOC_LINUX)
      case WindowingSystem::Xlib:
      {
        XlibWindowData *xlib = (XlibWindowData *)data;
        window = (EGLNativeWindowType)xlib->window;
        break;
      }
#endif
      case WindowingSystem::Unknown:
        // allow WindowingSystem::Unknown so that internally we can create a window-less context
        break;
      default: RDCERR("Unexpected window system %u", system); break;
    }

    EGLDisplay eglDisplay = eglGetDisplay_real(EGL_DEFAULT_DISPLAY);
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

    PFN_eglChooseConfig eglChooseConfig = (PFN_eglChooseConfig)dlsym(RTLD_NEXT, "eglChooseConfig");
    PFN_eglCreateWindowSurface eglCreateWindowSurface =
        (PFN_eglCreateWindowSurface)dlsym(RTLD_NEXT, "eglCreateWindowSurface");
    PFN_eglCreatePbufferSurface eglCreatePbufferSurface =
        (PFN_eglCreatePbufferSurface)dlsym(RTLD_NEXT, "eglCreatePbufferSurface");

    EGLint numConfigs;
    EGLConfig config;
    if(!eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs))
    {
      RDCERR("Couldn't find a suitable EGL config");
      return ret;
    }

    static const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                        EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

    EGLContext ctx = eglCreateContext_real(eglDisplay, config, share_context.ctx, ctxAttribs);

    if(ctx == NULL)
    {
      RDCERR("Couldn't create GL ES context");
      return ret;
    }

    EGLSurface surface = 0;

    if(window != 0)
    {
      surface = eglCreateWindowSurface(eglDisplay, config, window, NULL);
    }
    else
    {
      static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
      surface = eglCreatePbufferSurface(eglDisplay, config, pbAttribs);
    }

    ret.egl_dpy = eglDisplay;
    ret.egl_ctx = ctx;
    ret.egl_wnd = surface;
#if ENABLED(RDOC_ANDROID)
    ret.wnd = (ANativeWindow *)window;
#endif

    return ret;
  }

  bool DrawQuads(float width, float height, const std::vector<Vec4f> &vertices);

  WrappedOpenGL *GetDriver()
  {
    if(m_GLDriver == NULL)
    {
      m_GLDriver = new WrappedOpenGL("", GL, *this);
      m_GLDriver->SetDriverType(RDC_OpenGLES);
    }

    return m_GLDriver;
  }

  PFN_eglCreateContext eglCreateContext_real;
  PFN_eglDestroyContext eglDestroyContext_real;
  PFN_eglGetProcAddress eglGetProcAddress_real;
  PFN_eglSwapBuffers eglSwapBuffers_real;
  PFN_eglMakeCurrent eglMakeCurrent_real;
  PFN_eglQuerySurface eglQuerySurface_real;
  PFN_eglGetConfigAttrib eglGetConfigAttrib_real;
  PFN_eglGetDisplay eglGetDisplay_real;

  set<EGLContext> m_Contexts;

  bool m_PopulatedHooks;
  bool m_HasHooks;
  bool m_EnabledHooks;

  bool SetupHooks()
  {
    bool success = true;

    if(eglGetProcAddress_real == NULL)
      eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(libGLdlsymHandle, "eglGetProcAddress");
    if(eglCreateContext_real == NULL)
      eglCreateContext_real = (PFN_eglCreateContext)dlsym(libGLdlsymHandle, "eglCreateContext");
    if(eglDestroyContext_real == NULL)
      eglDestroyContext_real = (PFN_eglDestroyContext)dlsym(libGLdlsymHandle, "eglDestroyContext");
    if(eglMakeCurrent_real == NULL)
      eglMakeCurrent_real = (PFN_eglMakeCurrent)dlsym(libGLdlsymHandle, "eglMakeCurrent");
    if(eglSwapBuffers_real == NULL)
      eglSwapBuffers_real = (PFN_eglSwapBuffers)dlsym(libGLdlsymHandle, "eglSwapBuffers");
    if(eglQuerySurface_real == NULL)
      eglQuerySurface_real = (PFN_eglQuerySurface)dlsym(libGLdlsymHandle, "eglQuerySurface");
    if(eglGetConfigAttrib_real == NULL)
      eglGetConfigAttrib_real =
          (PFN_eglGetConfigAttrib)dlsym(libGLdlsymHandle, "eglGetConfigAttrib");
    if(eglGetDisplay_real == NULL)
      eglGetDisplay_real = (PFN_eglGetDisplay)dlsym(libGLdlsymHandle, "eglGetDisplay");

    return success;
  }

  bool PopulateHooks();
} eglhooks;

void EGLHook::libHooked(void *realLib)
{
  libGLdlsymHandle = realLib;
  eglhooks.CreateHooks(NULL);
  eglhooks.GetDriver()->SetDriverType(RDC_OpenGLES);
}

// everything below here needs to have C linkage
extern "C" {

__attribute__((visibility("default"))) EGLDisplay eglGetDisplay(EGLNativeDisplayType display)
{
  if(eglhooks.eglGetDisplay_real == NULL)
    eglhooks.SetupExportedFunctions();

#if DISABLED(RDOC_ANDROID)
  Keyboard::CloneDisplay(display);
#endif

  return eglhooks.eglGetDisplay_real(display);
}

__attribute__((visibility("default"))) EGLContext eglCreateContext(EGLDisplay display,
                                                                   EGLConfig config,
                                                                   EGLContext shareContext,
                                                                   EGLint const *attribList)
{
  EGLint defaultAttribList[] = {0};

  const EGLint *attribs = attribList ? attribList : defaultAttribList;
  vector<EGLint> attribVec;

  // modify attribs to our liking
  {
    bool flagsFound = false;
    const int *a = attribs;
    while(*a)
    {
      int name = *a++;
      int val = *a++;

      if(name == EGL_CONTEXT_FLAGS_KHR)
      {
        if(RenderDoc::Inst().GetCaptureOptions().APIValidation)
          val |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
        else
          val &= ~EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

        // remove NO_ERROR bit
        val &= ~EGL_CONTEXT_OPENGL_NO_ERROR_KHR;

        flagsFound = true;
      }

      attribVec.push_back(name);
      attribVec.push_back(val);
    }

    if(!flagsFound && RenderDoc::Inst().GetCaptureOptions().APIValidation)
    {
      attribVec.push_back(EGL_CONTEXT_FLAGS_KHR);
      attribVec.push_back(EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR);
    }

    attribVec.push_back(EGL_NONE);

    attribs = &attribVec[0];
  }

  if(eglhooks.eglCreateContext_real == NULL)
    eglhooks.SetupExportedFunctions();

  EGLContext ret = eglhooks.eglCreateContext_real(display, config, shareContext, attribs);

  // don't continue if context creation failed
  if(!ret)
    return ret;

  GLInitParams init;

  init.width = 0;
  init.height = 0;

  EGLint value;
  eglhooks.eglGetConfigAttrib_real(display, config, EGL_BUFFER_SIZE, &value);
  init.colorBits = value;
  eglhooks.eglGetConfigAttrib_real(display, config, EGL_DEPTH_SIZE, &value);
  init.depthBits = value;
  eglhooks.eglGetConfigAttrib_real(display, config, EGL_STENCIL_SIZE, &value);
  init.stencilBits = value;
  // We will set isSRGB when we see the surface.
  init.isSRGB = 0;

  GLWindowingData data;
  data.egl_dpy = display;
  data.egl_wnd = (EGLSurface)NULL;
  data.egl_ctx = ret;

  eglhooks.GetDriver()->SetDriverType(RDC_OpenGLES);
  {
    SCOPED_LOCK(glLock);
    eglhooks.GetDriver()->CreateContext(data, shareContext, init, true, true);
  }

  return ret;
}

__attribute__((visibility("default"))) EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
  if(eglhooks.eglDestroyContext_real == NULL)
    eglhooks.SetupExportedFunctions();

  eglhooks.GetDriver()->SetDriverType(RDC_OpenGLES);
  {
    SCOPED_LOCK(glLock);
    eglhooks.GetDriver()->DeleteContext(ctx);
  }

  return eglhooks.eglDestroyContext_real(dpy, ctx);
}

__attribute__((visibility("default"))) EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                                                 EGLSurface read, EGLContext ctx)
{
  if(eglhooks.eglMakeCurrent_real == NULL)
    eglhooks.SetupExportedFunctions();

  EGLBoolean ret = eglhooks.eglMakeCurrent_real(display, draw, read, ctx);

  SCOPED_LOCK(glLock);

  if(ctx && eglhooks.m_Contexts.find(ctx) == eglhooks.m_Contexts.end())
  {
    eglhooks.m_Contexts.insert(ctx);

    eglhooks.PopulateHooks();
  }

  GLWindowingData data;
  data.egl_dpy = display;
  data.egl_wnd = draw;
  data.egl_ctx = ctx;

  eglhooks.GetDriver()->SetDriverType(RDC_OpenGLES);
  eglhooks.GetDriver()->ActivateContext(data);

  return ret;
}

__attribute__((visibility("default"))) EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
  if(eglhooks.eglSwapBuffers_real == NULL)
    eglhooks.SetupExportedFunctions();

  SCOPED_LOCK(glLock);

  int height, width;
  eglhooks.eglQuerySurface_real(dpy, surface, EGL_HEIGHT, &height);
  eglhooks.eglQuerySurface_real(dpy, surface, EGL_WIDTH, &width);

  GLInitParams &init = eglhooks.GetDriver()->GetInitParams();
  int colorspace = 0;
  eglhooks.eglQuerySurface_real(dpy, surface, EGL_GL_COLORSPACE, &colorspace);
  // GL_SRGB8_ALPHA8 is specified as color-renderable, unlike GL_SRGB8.
  init.isSRGB = init.colorBits == 32 && colorspace == EGL_GL_COLORSPACE_SRGB;

  eglhooks.GetDriver()->SetDriverType(RDC_OpenGLES);
  eglhooks.GetDriver()->WindowSize(surface, width, height);
  eglhooks.GetDriver()->SwapBuffers(surface);

  return eglhooks.eglSwapBuffers_real(dpy, surface);
}

__attribute__((visibility("default"))) __eglMustCastToProperFunctionPointerType eglGetProcAddress(
    const char *func)
{
  if(eglhooks.eglGetProcAddress_real == NULL)
    eglhooks.SetupExportedFunctions();

  __eglMustCastToProperFunctionPointerType realFunc = eglhooks.eglGetProcAddress_real(func);

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
  if(!strncmp(func, "egl", 3))
    return realFunc;

  // if the real RC doesn't support this function, don't bother hooking
  if(realFunc == NULL)
    return realFunc;

  return (__eglMustCastToProperFunctionPointerType)SharedLookupFuncPtr(func, (void *)realFunc);
}

};    // extern "C"

bool EGLHook::PopulateHooks()
{
  SetupHooks();

  return SharedPopulateHooks([](const char *funcName) { return (void *)eglGetProcAddress(funcName); });
}

const GLHookSet &GetRealGLFunctionsEGL()
{
  return eglhooks.GetRealGLFunctions();
}

GLPlatform &GetGLPlatformEGL()
{
  return eglhooks;
}

// All old style things are disabled in EGL mode
bool EGLHook::DrawQuads(float width, float height, const std::vector<Vec4f> &vertices)
{
  return false;
}
