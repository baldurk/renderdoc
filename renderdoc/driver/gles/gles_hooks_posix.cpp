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
#include "driver/gles/gles_common.h"
#include "driver/gles/gles_driver.h"
#include "driver/gles/gles_hookset.h"
#include "driver/gles/gles_hookset_defs.h"
#include "hooks/hooks.h"
#include "serialise/string_utils.h"

#include "official/egl_func_typedefs.h"

void *libGLdlsymHandle =
    RTLD_NEXT;    // default to RTLD_NEXT, but overwritten if app calls dlopen() on real libGL

Threading::CriticalSection glLock;

class OpenGLHook : LibraryHook
{
public:
  OpenGLHook() {

    LibraryHooks::GetInstance().RegisterHook("libEGL.so", this);

    RDCEraseEl(GL);

    m_HasHooks = false;

    m_GLESDriver = NULL;

    m_EnabledHooks = true;
    m_PopulatedHooks = false;

    m_libGLdlsymHandle = dlopen("libEGL.so", RTLD_NOW);

    PopulateHooks();
  }

  ~OpenGLHook() { delete m_GLESDriver; }

  const GLHookSet &GetRealGLFunctions()
  {
    if(!m_PopulatedHooks)
      m_PopulatedHooks = PopulateHooks();
    return GL;
  }

  static void libHooked(void *realLib)
  {
    OpenGLHook::glhooks.m_libGLdlsymHandle = realLib;
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

  WrappedGLES *GetDriver()
  {
    if(m_GLESDriver == NULL)
    {
        GLESInitParams initParams;
        m_GLESDriver = new WrappedGLES("", GL);
    }

    return m_GLESDriver;
  }

  void MakeContextCurrent(GLESWindowingData data)
  {
    if (m_eglMakeCurrent_real)
      m_eglMakeCurrent_real(data.eglDisplay, data.surface, data.surface, data.ctx);
  }


  GLESWindowingData MakeContext(GLESWindowingData share)
  {
    GLESWindowingData ret;
    return share;
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

  void *GetDLHandle() { return m_libGLdlsymHandle; }

private:
  void *m_libGLdlsymHandle =
    RTLD_NEXT; // default to RTLD_NEXT, but overwritten if app calls dlopen() on real libGL

};

OpenGLHook OpenGLHook::glhooks;

bool OpenGLHook::SetupHooks(GLHookSet &GL)
{
  bool success = true;
  if(m_eglGetProcAddress_real == NULL)
    m_eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(m_libGLdlsymHandle, "eglGetProcAddress");

  if(m_eglSwapBuffers_real == NULL)
    m_eglSwapBuffers_real = (PFN_eglSwapBuffers)dlsym(m_libGLdlsymHandle, "eglSwapBuffers");

  if(m_eglMakeCurrent_real == NULL)
    m_eglMakeCurrent_real = (PFN_eglMakeCurrent)dlsym(m_libGLdlsymHandle, "eglMakeCurrent");

  if(m_eglQuerySurface_real == NULL)
    m_eglQuerySurface_real = (PFN_eglQuerySurface)dlsym(m_libGLdlsymHandle, "eglQuerySurface");


  return success;
}

#include "gles_hooks_posix.inc.cpp"

bool OpenGLHook::PopulateHooks()
{
  bool success = true;

  if (m_PopulatedHooks)
    return success;

  if(m_eglGetProcAddress_real == NULL)
    m_eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(m_libGLdlsymHandle, "eglGetProcAddress");

#undef HookInit
#define HookInit(function) \
  if(GL.function == NULL)                                                                    \
  {                                                                                          \
    GL.function = (CONCAT(function, _hooktype))dlsym(m_libGLdlsymHandle, STRINGIZE(function)); \
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

  m_PopulatedHooks = true;
  return true;
}

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
  RDCUNIMPLEMENTED("DeleteContext");
}


#include "gles_hooks_posix_egl.inc.cpp"
