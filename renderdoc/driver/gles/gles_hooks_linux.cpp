/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
#include <set>
#include <cstdlib>
#include "common/threading.h"
#include "driver/gles/gles_driver.h"
#include "driver/gles/gles_hookset.h"
#include "driver/gles/gles_hookset_defs.h"
#include "hooks/hooks.h"
#include "serialise/string_utils.h"

#include "official/egl_func_typedefs.h"

namespace glEmulate
{
void EmulateUnsupportedFunctions(GLHookSet *hooks);
}

// bit of a hack
namespace Keyboard
{
void CloneDisplay(Display *dpy);
}

void *libGLdlsymHandle =
    RTLD_NEXT;    // default to RTLD_NEXT, but overwritten if app calls dlopen() on real libGL


Threading::CriticalSection glLock;

class OpenGLHook : LibraryHook
{
public:
  OpenGLHook()
  {
    LibraryHooks::GetInstance().RegisterHook("libEGL.so", this);

    RDCEraseEl(GL);

    m_HasHooks = false;

    m_GLESDriver = NULL;

    m_EnabledHooks = true;
    m_PopulatedHooks = false;
  }

  ~OpenGLHook() { delete m_GLESDriver; }

  static void libHooked(void *realLib)
  {
    libGLdlsymHandle = realLib;
    OpenGLHook::glhooks.CreateHooks(NULL);
  }

  bool CreateHooks(const char *libName)
  {
    if(!m_EnabledHooks)
      return false;

    if(libName)
      PosixHookLibrary("libEGL.so", &libHooked);

    bool success = SetupHooks(GL);

    if(!success)
      return false;

    m_HasHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
  static OpenGLHook glhooks;

  const GLHookSet &GetRealGLFunctions()
  {
    if(!m_PopulatedHooks)
      m_PopulatedHooks = PopulateHooks();
    return GL;
  }

  void MakeContextCurrent(GLESWindowingData data)
  {
#if 0
    if(glXMakeCurrent_real)
      glXMakeCurrent_real(data.dpy, data.wnd, data.ctx);
#endif
  }

  GLESWindowingData MakeContext(GLESWindowingData share)
  {
    GLESWindowingData ret;
#if 0
    if(glXCreateContextAttribsARB_real)
    {
      const int attribs[] = {
          GLX_CONTEXT_MAJOR_VERSION_ARB,
          3,
          GLX_CONTEXT_MINOR_VERSION_ARB,
          2,
          GLX_CONTEXT_FLAGS_ARB,
          0,
          GLX_CONTEXT_PROFILE_MASK_ARB,
          GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
          0,
          0,
      };
      bool is_direct = false;

      PFNGLXISDIRECTPROC glXIsDirectProc = (PFNGLXISDIRECTPROC)dlsym(RTLD_NEXT, "glXIsDirect");
      PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfigProc =
          (PFNGLXCHOOSEFBCONFIGPROC)dlsym(RTLD_NEXT, "glXChooseFBConfig");

      if(glXIsDirectProc)
        is_direct = glXIsDirectProc(share.dpy, share.ctx);

      if(glXChooseFBConfigProc)
      {
        // don't need to care about the fb config as we won't be using the default framebuffer
        // (backbuffer)
        int visAttribs[] = {0};
        int numCfgs = 0;
        GLXFBConfig *fbcfg =
            glXChooseFBConfigProc(share.dpy, DefaultScreen(share.dpy), visAttribs, &numCfgs);

        if(fbcfg)
        {
          ret.dpy = share.dpy;
          ret.ctx =
              glXCreateContextAttribsARB_real(share.dpy, fbcfg[0], share.ctx, is_direct, attribs);
        }
      }
    }
#endif
    return ret;
  }

  void DeleteContext(GLESWindowingData context)
  {
#if 0
    if(context.ctx && glXDestroyContext_real)
      glXDestroyContext_real(context.display, context.ctx);
#endif
  }

  WrappedGLES *GetDriver()
  {
    if(m_GLESDriver == NULL)
    {
        GLESInitParams initParams;
        m_GLESDriver = new WrappedGLES("", GL);
    }

    return m_GLESDriver;
  }

  PFN_eglGetProcAddress m_eglGetProcAddress_real;
  PFN_eglSwapBuffers m_eglSwapBuffers_real;
  PFN_eglMakeCurrent m_eglMakeCurrent_real;
  PFN_eglQuerySurface m_eglQuerySurface_real;

  WrappedGLES *m_GLESDriver;

  GLHookSet GL;

  set<EGLContext> m_Contexts;

  bool m_PopulatedHooks;
  bool m_HasHooks;
  bool m_EnabledHooks;

  bool SetupHooks(GLHookSet &GL);
  bool PopulateHooks();
};

#include "gles_hooks_posix.inc.cpp"

bool OpenGLHook::SetupHooks(GLHookSet &GL)
{
  bool success = true;
  if(m_eglGetProcAddress_real == NULL)
    m_eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(libGLdlsymHandle, "eglGetProcAddress");

  if(m_eglSwapBuffers_real == NULL)
    m_eglSwapBuffers_real = (PFN_eglSwapBuffers)dlsym(libGLdlsymHandle, "eglSwapBuffers");

  if(m_eglMakeCurrent_real == NULL)
    m_eglMakeCurrent_real = (PFN_eglMakeCurrent)dlsym(libGLdlsymHandle, "eglMakeCurrent");

  if(m_eglQuerySurface_real == NULL)
    m_eglQuerySurface_real = (PFN_eglQuerySurface)dlsym(libGLdlsymHandle, "eglQuerySurface");

#if 0
  if(glXCreateContext_real == NULL)
    glXCreateContext_real = (PFNGLXCREATECONTEXTPROC)dlsym(libGLdlsymHandle, "glXCreateContext");
  if(glXDestroyContext_real == NULL)
    glXDestroyContext_real = (PFNGLXDESTROYCONTEXTPROC)dlsym(libGLdlsymHandle, "glXDestroyContext");
  if(glXCreateContextAttribsARB_real == NULL)
    glXCreateContextAttribsARB_real =
        (PFNGLXCREATECONTEXTATTRIBSARBPROC)dlsym(libGLdlsymHandle, "glXCreateContextAttribsARB");
  if(glXMakeCurrent_real == NULL)
    glXMakeCurrent_real = (PFNGLXMAKECURRENTPROC)dlsym(libGLdlsymHandle, "glXMakeCurrent");
  if(glXGetConfig_real == NULL)
    glXGetConfig_real = (PFNGLXGETCONFIGPROC)dlsym(libGLdlsymHandle, "glXGetConfig");
  if(glXGetVisualFromFBConfig_real == NULL)
    glXGetVisualFromFBConfig_real =
        (PFNGLXGETVISUALFROMFBCONFIGPROC)dlsym(libGLdlsymHandle, "glXGetVisualFromFBConfig");
  if(glXQueryExtension_real == NULL)
    glXQueryExtension_real = (PFNGLXQUERYEXTENSIONPROC)dlsym(libGLdlsymHandle, "glXQueryExtension");
#endif

  return success;
}

bool OpenGLHook::PopulateHooks()
{
  bool success = true;
  if(m_eglGetProcAddress_real == NULL)
    m_eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(libGLdlsymHandle, "eglGetProcAddress");

#if 0
  eglGetProcAddress_real((const GLubyte *)"eglCreateContext");
#endif

#undef HookInit
#define HookInit(function) \
  if(GL.function == NULL)                                                                    \
  {                                                                                          \
    GL.function = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, STRINGIZE(function)); \
    eglGetProcAddress((const char *)STRINGIZE(function));                                 \
  }

// cheeky
#undef HookExtension
#define HookExtension(funcPtrType, function) eglGetProcAddress((const char*)STRINGIZE(function))
#undef HookExtensionAlias
#define HookExtensionAlias(funcPtrType, function, alias)

#define HandleUnsupported(funcPtrType, function)           \
  if(GL.function == NULL)                                  \
  {                                                        \
    CONCAT(unsupported_real_ , function) = (funcPtrType)m_eglGetProcAddress_real(STRINGIZE(function));  \
    GL.function = CONCAT(function, _renderdoc_hooked);                                                   \
  }


  DLLExportHooks();
  HookCheckGLExtensions();
  CheckUnsupported();

#if 0
  // see gl_emulated.cpp
  if(RenderDoc::Inst().IsReplayApp())
    glEmulate::EmulateUnsupportedFunctions(&GL);
#endif
  return true;
}

OpenGLHook OpenGLHook::glhooks;

const GLHookSet &GetRealGLFunctions()
{
  return OpenGLHook::glhooks.GetRealGLFunctions();
}

void MakeContextCurrent(GLESWindowingData data)
{
  OpenGLHook::glhooks.MakeContextCurrent(data);
}

GLESWindowingData MakeContext(GLESWindowingData share)
{
  return OpenGLHook::glhooks.MakeContext(share);
}

void DeleteContext(GLESWindowingData context)
{
  OpenGLHook::glhooks.DeleteContext(context);
}

#include "gles_hooks_linux_egl.cpp"
