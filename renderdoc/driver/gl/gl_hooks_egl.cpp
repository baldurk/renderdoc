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
typedef EGLBoolean (*PFN_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
                                          EGLint *value);
typedef EGLBoolean (*PFN_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (*PFN_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                                         EGLContext ctx);
typedef EGLBoolean (*PFN_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
typedef EGLDisplay (*PFN_eglGetDisplay)(EGLNativeDisplayType display_id);

class EGLHook : LibraryHook
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
  ~EGLHook() { delete m_GLDriver; }
  static void libHooked(void *realLib)
  {
    libGLdlsymHandle = realLib;
    EGLHook::glhooks.CreateHooks(NULL);
  }

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
  static EGLHook glhooks;

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
        EGLBoolean configFound = eglChooseConfig(share.dpy, attribs, &config, 1, &numConfigs);

        if(configFound)
        {
          const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
          ret.egl_wnd = eglCreatePbufferSurface(share.dpy, config, pbAttribs);
          ret.egl_dpy = share.dpy;
          ret.egl_ctx = eglCreateContext_real(share.dpy, config, share.ctx, ctxAttribs);
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

  WrappedOpenGL *GetDriver()
  {
    if(m_GLDriver == NULL)
      m_GLDriver = new WrappedOpenGL("", GL);

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

  WrappedOpenGL *m_GLDriver;

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
};

// everything below here needs to have C linkage
extern "C" {

__attribute__((visibility("default"))) EGLDisplay eglGetDisplay(EGLNativeDisplayType display)
{
  if(EGLHook::glhooks.eglGetDisplay_real == NULL)
    EGLHook::glhooks.SetupExportedFunctions();

  Keyboard::CloneDisplay(display);

  return EGLHook::glhooks.eglGetDisplay_real(display);
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

  if(EGLHook::glhooks.eglCreateContext_real == NULL)
    EGLHook::glhooks.SetupExportedFunctions();

  EGLContext ret = EGLHook::glhooks.eglCreateContext_real(display, config, shareContext, attribs);

  // don't continue if context creation failed
  if(!ret)
    return ret;

  GLInitParams init;

  init.width = 0;
  init.height = 0;

  EGLint value;
  EGLHook::glhooks.eglGetConfigAttrib_real(display, config, EGL_BUFFER_SIZE, &value);
  init.colorBits = value;
  EGLHook::glhooks.eglGetConfigAttrib_real(display, config, EGL_DEPTH_SIZE, &value);
  init.depthBits = value;
  EGLHook::glhooks.eglGetConfigAttrib_real(display, config, EGL_STENCIL_SIZE, &value);
  init.stencilBits = value;
  // TODO: how to detect this?
  init.isSRGB = 1;

  GLWindowingData data;
  data.egl_dpy = display;
  data.egl_wnd = (EGLSurface)NULL;
  data.egl_ctx = ret;

  {
    SCOPED_LOCK(glLock);
    GetDriver()->CreateContext(data, shareContext, init, true, true);
  }

  return ret;
}

__attribute__((visibility("default"))) EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
  if(EGLHook::glhooks.eglDestroyContext_real == NULL)
    EGLHook::glhooks.SetupExportedFunctions();

  {
    SCOPED_LOCK(glLock);
    GetDriver()->DeleteContext(ctx);
  }

  return EGLHook::glhooks.eglDestroyContext_real(dpy, ctx);
}

__attribute__((visibility("default"))) EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                                                 EGLSurface read, EGLContext ctx)
{
  if(EGLHook::glhooks.eglMakeCurrent_real == NULL)
    EGLHook::glhooks.SetupExportedFunctions();

  EGLBoolean ret = EGLHook::glhooks.eglMakeCurrent_real(display, draw, read, ctx);

  SCOPED_LOCK(glLock);

  if(ctx && EGLHook::glhooks.m_Contexts.find(ctx) == EGLHook::glhooks.m_Contexts.end())
  {
    EGLHook::glhooks.m_Contexts.insert(ctx);

    EGLHook::glhooks.PopulateHooks();
  }

  GLWindowingData data;
  data.egl_dpy = display;
  data.egl_wnd = draw;
  data.egl_ctx = ctx;

  GetDriver()->ActivateContext(data);

  return ret;
}

__attribute__((visibility("default"))) EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
  if(EGLHook::glhooks.eglSwapBuffers_real == NULL)
    EGLHook::glhooks.SetupExportedFunctions();

  SCOPED_LOCK(glLock);

  int height, width;
  EGLHook::glhooks.eglQuerySurface_real(dpy, surface, EGL_HEIGHT, &height);
  EGLHook::glhooks.eglQuerySurface_real(dpy, surface, EGL_WIDTH, &width);

  GetDriver()->WindowSize(surface, width, height);
  GetDriver()->SwapBuffers(surface);

  return EGLHook::glhooks.eglSwapBuffers_real(dpy, surface);
}

__attribute__((visibility("default"))) __eglMustCastToProperFunctionPointerType eglGetProcAddress(
    const char *func)
{
  if(EGLHook::glhooks.eglGetProcAddress_real == NULL)
    EGLHook::glhooks.SetupExportedFunctions();

  __eglMustCastToProperFunctionPointerType realFunc = EGLHook::glhooks.eglGetProcAddress_real(func);

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
  return EGLHook::glhooks.GetRealGLFunctions();
}

EGLHook EGLHook::glhooks;

#ifndef RENDERDOC_SUPPORT_GL    // FIXME
void MakeContextCurrent(GLWindowingData data)
{
  EGLHook::glhooks.MakeContextCurrent(data);
}

GLWindowingData MakeContext(GLWindowingData share)
{
  return EGLHook::glhooks.MakeContext(share);
}

void DeleteContext(GLWindowingData context)
{
  EGLHook::glhooks.DeleteContext(context);
}
#endif

// FIXME: Combine both sets of immediate* functions in the same build of GL and GLES
#ifndef RENDERDOC_SUPPORT_GL
// All old style things are disabled in EGL mode
bool immediateBegin(GLenum mode, float width, float height)
{
  return false;
}

void immediateVert(float x, float y, float u, float v)
{
}

void immediateEnd()
{
}
#endif
