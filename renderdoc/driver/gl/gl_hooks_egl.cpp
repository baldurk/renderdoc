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
#include "strings/string_utils.h"
#include "gl_hooks_linux_shared.h"
#include "gl_library_egl.h"

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
    if(real.MakeCurrent)
      real.MakeCurrent(data.egl_dpy, data.egl_wnd, data.egl_wnd, data.egl_ctx);
  }

  GLWindowingData MakeContext(GLWindowingData share)
  {
    GLWindowingData ret;

    if(real.CreateContext && real.ChooseConfig && real.CreatePbufferSurface)
    {
      ret = CreateWindowingData(real, share.egl_dpy, share.ctx, 0);
    }

    return ret;
  }

  void DeleteContext(GLWindowingData context)
  {
    if(context.wnd && real.DestroySurface)
      real.DestroySurface(context.egl_dpy, context.egl_wnd);

    if(context.ctx && real.DestroyContext)
      real.DestroyContext(context.egl_dpy, context.egl_ctx);
  }

  void DeleteReplayContext(GLWindowingData context)
  {
    if(real.DestroyContext)
    {
      real.MakeCurrent(context.egl_dpy, 0L, 0L, NULL);
      real.DestroyContext(context.egl_dpy, context.egl_ctx);
    }
  }

  void SwapBuffers(GLWindowingData context) { real.SwapBuffers(context.egl_dpy, context.egl_wnd); }
  void GetOutputWindowDimensions(GLWindowingData context, int32_t &w, int32_t &h)
  {
    // On some Linux systems the surface seems to be context dependant.
    // Thus we need to switch to that context where the surface was created.
    // To avoid any problems because of the context change we'll save the old
    // context information so we can switch back to it after the surface query is done.
    GLWindowingData oldContext;
    oldContext.egl_ctx = real.GetCurrentContext();
    oldContext.egl_dpy = real.GetCurrentDisplay();
    oldContext.egl_wnd = real.GetCurrentSurface(EGL_READ);
    MakeContextCurrent(context);

    EGLBoolean width_ok = real.QuerySurface(context.egl_dpy, context.egl_wnd, EGL_WIDTH, &w);
    EGLBoolean height_ok = real.QuerySurface(context.egl_dpy, context.egl_wnd, EGL_HEIGHT, &h);

    if(!width_ok || !height_ok)
    {
      RDCGLenum error_code = (RDCGLenum)real.GetError();
      RDCWARN("Unable to query the surface size. Error: (0x%x) %s", error_code,
              ToStr(error_code).c_str());
    }

    MakeContextCurrent(oldContext);
  }

  bool IsOutputWindowVisible(GLWindowingData context) { return true; }
  GLWindowingData MakeOutputWindow(WindowingData window, bool depth, GLWindowingData share_context)
  {
    EGLNativeWindowType win = 0;

    switch(window.system)
    {
#if ENABLED(RDOC_ANDROID)
      case WindowingSystem::Android: win = window.android.window; break;
#elif ENABLED(RDOC_LINUX)
      case WindowingSystem::Xlib: win = window.xlib.window; break;
#endif
      case WindowingSystem::Unknown:
        // allow WindowingSystem::Unknown so that internally we can create a window-less context
        break;
      default: RDCERR("Unexpected window system %u", system); break;
    }

    EGLDisplay eglDisplay = real.GetDisplay(EGL_DEFAULT_DISPLAY);
    RDCASSERT(eglDisplay);

    return CreateWindowingData(real, eglDisplay, share_context.ctx, win);
  }

  bool DrawQuads(float width, float height, const std::vector<Vec4f> &vertices);

  WrappedOpenGL *GetDriver()
  {
    if(m_GLDriver == NULL)
    {
      m_GLDriver = new WrappedOpenGL(GL, *this);
      m_GLDriver->SetDriverType(RDCDriver::OpenGLES);
    }

    return m_GLDriver;
  }

  EGLPointers real;
  set<EGLContext> m_Contexts;

  bool m_PopulatedHooks;
  bool m_HasHooks;
  bool m_EnabledHooks;

  bool SetupHooks()
  {
    bool success = true;

    if(!real.IsInitialized())
    {
      bool symbols_ok = real.LoadSymbolsFrom(libGLdlsymHandle);
      if(!symbols_ok)
      {
        RDCWARN("Unable to load some of the EGL API functions, may cause problems");
        success = false;
      }
    }
    return success;
  }

  bool PopulateHooks();
} eglhooks;

void EGLHook::libHooked(void *realLib)
{
  libGLdlsymHandle = realLib;
  eglhooks.CreateHooks(NULL);
  eglhooks.GetDriver()->SetDriverType(RDCDriver::OpenGLES);
}

// everything below here needs to have C linkage
extern "C" {

__attribute__((visibility("default"))) EGLDisplay eglGetDisplay(EGLNativeDisplayType display)
{
  if(eglhooks.real.GetDisplay == NULL)
    eglhooks.SetupExportedFunctions();

#if DISABLED(RDOC_ANDROID)
  Keyboard::CloneDisplay(display);
#endif

  return eglhooks.real.GetDisplay(display);
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
        if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
          val |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
        else
          val &= ~EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

        flagsFound = true;
      }

      attribVec.push_back(name);
      attribVec.push_back(val);
    }

    if(!flagsFound && RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      attribVec.push_back(EGL_CONTEXT_FLAGS_KHR);
      attribVec.push_back(EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR);
    }

    attribVec.push_back(EGL_NONE);

    attribs = &attribVec[0];
  }

  if(eglhooks.real.CreateContext == NULL)
    eglhooks.SetupExportedFunctions();

  EGLContext ret = eglhooks.real.CreateContext(display, config, shareContext, attribs);

  // don't continue if context creation failed
  if(!ret)
    return ret;

  GLInitParams init;

  init.width = 0;
  init.height = 0;

  EGLint value;
  eglhooks.real.GetConfigAttrib(display, config, EGL_BUFFER_SIZE, &value);
  init.colorBits = value;
  eglhooks.real.GetConfigAttrib(display, config, EGL_DEPTH_SIZE, &value);
  init.depthBits = value;
  eglhooks.real.GetConfigAttrib(display, config, EGL_STENCIL_SIZE, &value);
  init.stencilBits = value;
  // We will set isSRGB when we see the surface.
  init.isSRGB = 0;

  GLWindowingData data;
  data.egl_dpy = display;
  data.egl_wnd = (EGLSurface)NULL;
  data.egl_ctx = ret;

  eglhooks.GetDriver()->SetDriverType(RDCDriver::OpenGLES);
  {
    SCOPED_LOCK(glLock);
    eglhooks.GetDriver()->CreateContext(data, shareContext, init, true, true);
  }

  return ret;
}

__attribute__((visibility("default"))) EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
  if(eglhooks.real.DestroyContext == NULL)
    eglhooks.SetupExportedFunctions();

  eglhooks.GetDriver()->SetDriverType(RDCDriver::OpenGLES);
  {
    SCOPED_LOCK(glLock);
    eglhooks.GetDriver()->DeleteContext(ctx);
  }

  return eglhooks.real.DestroyContext(dpy, ctx);
}

__attribute__((visibility("default"))) EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                                                 EGLSurface read, EGLContext ctx)
{
  if(eglhooks.real.MakeCurrent == NULL)
    eglhooks.SetupExportedFunctions();

  EGLBoolean ret = eglhooks.real.MakeCurrent(display, draw, read, ctx);

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

  eglhooks.GetDriver()->SetDriverType(RDCDriver::OpenGLES);
  eglhooks.GetDriver()->ActivateContext(data);

  return ret;
}

__attribute__((visibility("default"))) EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
  if(eglhooks.real.SwapBuffers == NULL)
    eglhooks.SetupExportedFunctions();

  SCOPED_LOCK(glLock);

  int height, width;
  eglhooks.real.QuerySurface(dpy, surface, EGL_HEIGHT, &height);
  eglhooks.real.QuerySurface(dpy, surface, EGL_WIDTH, &width);

  GLInitParams &init = eglhooks.GetDriver()->GetInitParams();
  int colorspace = 0;
  eglhooks.real.QuerySurface(dpy, surface, EGL_GL_COLORSPACE, &colorspace);
  // GL_SRGB8_ALPHA8 is specified as color-renderable, unlike GL_SRGB8.
  init.isSRGB = init.colorBits == 32 && colorspace == EGL_GL_COLORSPACE_SRGB;

  eglhooks.GetDriver()->SetDriverType(RDCDriver::OpenGLES);
  eglhooks.GetDriver()->WindowSize(surface, width, height);
  eglhooks.GetDriver()->SwapBuffers(surface);

  return eglhooks.real.SwapBuffers(dpy, surface);
}

__attribute__((visibility("default"))) __eglMustCastToProperFunctionPointerType eglGetProcAddress(
    const char *func)
{
  if(eglhooks.real.GetProcAddress == NULL)
    eglhooks.SetupExportedFunctions();

  __eglMustCastToProperFunctionPointerType realFunc = eglhooks.real.GetProcAddress(func);

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

  return SharedPopulateHooks(
      false,    // dlsym can return GL symbols during a GLES context
      [](const char *funcName) { return (void *)eglGetProcAddress(funcName); });
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
