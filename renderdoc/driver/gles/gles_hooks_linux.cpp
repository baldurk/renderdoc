/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

// the _renderdoc_hooked variants are to make sure we always have a function symbol
// exported that we can return from glXGetProcAddress. If another library (or the app)
// creates a symbol called 'glEnable' we'll return the address of that, and break
// badly. Instead we leave the 'naked' versions for applications trying to import those
// symbols, and declare the _renderdoc_hooked for returning as a func pointer.

#define HookWrapper0(ret, function)                                \
  typedef ret (*CONCAT(function, _hooktype))();                    \
  extern "C" __attribute__((visibility("default"))) ret function() \
  {                                                                \
    SCOPED_LOCK(glLock);                                           \
    return OpenGLHook::glhooks.GetDriver()->function();            \
  }                                                                \
  ret CONCAT(function, _renderdoc_hooked)()                        \
  {                                                                \
    SCOPED_LOCK(glLock);                                           \
    return OpenGLHook::glhooks.GetDriver()->function();            \
  }
#define HookWrapper1(ret, function, t1, p1)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1) \
  {                                                                     \
    SCOPED_LOCK(glLock);                                                \
    return OpenGLHook::glhooks.GetDriver()->function(p1);               \
  }                                                                     \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                        \
  {                                                                     \
    SCOPED_LOCK(glLock);                                                \
    return OpenGLHook::glhooks.GetDriver()->function(p1);               \
  }
#define HookWrapper2(ret, function, t1, p1, t2, p2)                            \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2) \
  {                                                                            \
    SCOPED_LOCK(glLock);                                                       \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2);                  \
  }                                                                            \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                        \
  {                                                                            \
    SCOPED_LOCK(glLock);                                                       \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2);                  \
  }
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3)                           \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    SCOPED_LOCK(glLock);                                                              \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3);                     \
  }                                                                                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                        \
  {                                                                                   \
    SCOPED_LOCK(glLock);                                                              \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3);                     \
  }
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4);                        \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                        \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4);                        \
  }
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5);                           \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)                        \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5);                           \
  }
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                        \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, \
                                                                 t5 p5, t6 p6)               \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6);                \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)          \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6);                \
  }
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7)  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                    \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, \
                                                                 t5 p5, t6 p6, t7 p7)        \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7);            \
  }                                                                                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)   \
  {                                                                                          \
    SCOPED_LOCK(glLock);                                                                     \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7);            \
  }
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                       \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4,        \
                                                                 t5 p5, t6 p6, t7 p7, t8 p8)        \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8);               \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)   \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8);               \
  }
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                     p8, t9, p9)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                 \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9)                              \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);         \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9)                                                  \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);         \
  }
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10)                                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  extern "C" __attribute__((visibility("default"))) ret function(                                 \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10)                     \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);    \
  }                                                                                               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    SCOPED_LOCK(glLock);                                                                          \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);    \
  }
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,    \
                      p8, t9, p9, t10, p10, t11, p11)                                               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);         \
  extern "C" __attribute__((visibility("default"))) ret function(                                   \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11)              \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); \
  }                                                                                                 \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,   \
                                          t9 p9, t10 p10, t11 p11)                                  \
  {                                                                                                 \
    SCOPED_LOCK(glLock);                                                                            \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); \
  }
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12)                                    \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);   \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12)    \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12);                                         \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                        \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12);                                         \
  }
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13);                                                 \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13)                                                                                     \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13);                                    \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)               \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13);                                    \
  }
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14)                \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14);                                            \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14)                                                                            \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14);                               \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)      \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14);                               \
  }
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15)      \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,    \
                                             t13, t14, t15);                                       \
  extern "C" __attribute__((visibility("default"))) ret function(                                  \
      t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12,    \
      t13 p13, t14 p14, t15 p15)                                                                   \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14, p15);                          \
  }                                                                                                \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,      \
                                          t15 p15)                                                 \
  {                                                                                                \
    SCOPED_LOCK(glLock);                                                                           \
    return OpenGLHook::glhooks.GetDriver()->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, \
                                                     p12, p13, p14, p15);                          \
  }

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

DefineDLLExportHooks();
DefineGLExtensionHooks();

/*
  in bash:

    function HookWrapper()
    {
        N=$1;
        echo "#undef HookWrapper$N";
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (*CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -en "\tCONCAT(function, _hooktype) CONCAT(unsupported_real_,function) = NULL;";

        echo -en "\tret CONCAT(function,_renderdoc_hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -e "\t{ \\";
        echo -e "\tstatic bool hit = false; if(hit == false) { RDCERR(\"Function \"
  STRINGIZE(function) \" not supported - capture may be broken\"); hit = true; } \\";
        echo -en "\treturn CONCAT(unsupported_real_,function)(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -e "); \\";
        echo -e "\t}";
    }

  for I in `seq 0 15`; do HookWrapper $I; echo; done

  */

#undef HookWrapper0
#define HookWrapper0(ret, function)                                                     \
  typedef ret (*CONCAT(function, _hooktype))();                                         \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)()                                             \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)();                                       \
  }

#undef HookWrapper1
#define HookWrapper1(ret, function, t1, p1)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1);                                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1)                                        \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1);                                     \
  }

#undef HookWrapper2
#define HookWrapper2(ret, function, t1, p1, t2, p2)                                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2)                                 \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2);                                 \
  }

#undef HookWrapper3
#define HookWrapper3(ret, function, t1, p1, t2, p2, t3, p3)                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                               \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3)                          \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3);                             \
  }

#undef HookWrapper4
#define HookWrapper4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                           \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4)                   \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4);                         \
  }

#undef HookWrapper5
#define HookWrapper5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)            \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5);                     \
  }

#undef HookWrapper6
#define HookWrapper6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6);                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;               \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6)     \
  {                                                                                     \
    static bool hit = false;                                                            \
    if(hit == false)                                                                    \
    {                                                                                   \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken"); \
      hit = true;                                                                       \
    }                                                                                   \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6);                 \
  }

#undef HookWrapper7
#define HookWrapper7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7);                   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                   \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7)  \
  {                                                                                         \
    static bool hit = false;                                                                \
    if(hit == false)                                                                        \
    {                                                                                       \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");     \
      hit = true;                                                                           \
    }                                                                                       \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7);                 \
  }

#undef HookWrapper8
#define HookWrapper8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8);                       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                           \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8)   \
  {                                                                                                 \
    static bool hit = false;                                                                        \
    if(hit == false)                                                                                \
    {                                                                                               \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");             \
      hit = true;                                                                                   \
    }                                                                                               \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8);                     \
  }

#undef HookWrapper9
#define HookWrapper9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                     p8, t9, p9)                                                                  \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9);                 \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9)                                                  \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9);               \
  }

#undef HookWrapper10
#define HookWrapper10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10)                                                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);            \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10)                                         \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);          \
  }

#undef HookWrapper11
#define HookWrapper11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11)                                             \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);       \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11)                                \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);     \
  }

#undef HookWrapper12
#define HookWrapper12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,   \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12)                                    \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);   \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                          \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8,  \
                                          t9 p9, t10 p10, t11 p11, t12 p12)                        \
  {                                                                                                \
    static bool hit = false;                                                                       \
    if(hit == false)                                                                               \
    {                                                                                              \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");            \
      hit = true;                                                                                  \
    }                                                                                              \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); \
  }

#undef HookWrapper13
#define HookWrapper13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13);                                                \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13)              \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13);                                              \
  }

#undef HookWrapper14
#define HookWrapper14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14)               \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14);                                           \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14)     \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13, p14);                                         \
  }

#undef HookWrapper15
#define HookWrapper15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8,  \
                      p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15)     \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,   \
                                             t13, t14, t15);                                      \
  CONCAT(function, _hooktype) CONCAT(unsupported_real_, function) = NULL;                         \
  ret CONCAT(function, _renderdoc_hooked)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, \
                                          t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14,     \
                                          t15 p15)                                                \
  {                                                                                               \
    static bool hit = false;                                                                      \
    if(hit == false)                                                                              \
    {                                                                                             \
      RDCERR("Function " STRINGIZE(function) " not supported - capture may be broken");           \
      hit = true;                                                                                 \
    }                                                                                             \
    return CONCAT(unsupported_real_, function)(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, \
                                               p13, p14, p15);                                    \
  }

DefineUnsupportedDummies();


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
