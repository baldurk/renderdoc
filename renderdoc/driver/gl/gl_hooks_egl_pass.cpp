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
#include "driver/gl/gl_common.h"

// we need to export the whole of the EGL API, since we will have redirected any dlopen()
// for libEGL.so to ourselves, and dlsym() for any of these entry points must return a valid
// function. We don't need to intercept them, so we just pass it along

extern void *libGLdlsymHandle;

/*
  in bash:

    function EGLHook()
    {
        N=$1;
        echo -n "#define EGL_PASSTHRU_$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (*CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -e "\textern \"C\" __attribute__ ((visibility (\"default\"))) \\";

        echo -en "\tret function(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -en "\t{ CONCAT(function, _hooktype) real = (CONCAT(function, _hooktype))";
        echo "dlsym(libGLdlsymHandle, #function); \\";
        echo -en "\treturn real(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); }";
    }

  for I in `seq 0 5`; do EGLHook $I; echo; done

*/

#define EGL_PASSTHRU_0(ret, function)                                       \
  typedef ret (*CONCAT(function, _hooktype))();                             \
  extern "C" __attribute__((visibility("default"))) ret function()          \
  {                                                                         \
    CONCAT(function, _hooktype)                                             \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function); \
    return real();                                                          \
  }

#define EGL_PASSTHRU_1(ret, function, t1, p1)                               \
  typedef ret (*CONCAT(function, _hooktype))(t1);                           \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1)     \
  {                                                                         \
    CONCAT(function, _hooktype)                                             \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function); \
    return real(p1);                                                        \
  }

#define EGL_PASSTHRU_2(ret, function, t1, p1, t2, p2)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2) \
  {                                                                            \
    CONCAT(function, _hooktype)                                                \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);    \
    return real(p1, p2);                                                       \
  }

#define EGL_PASSTHRU_3(ret, function, t1, p1, t2, p2, t3, p3)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    CONCAT(function, _hooktype)                                                       \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);           \
    return real(p1, p2, p3);                                                          \
  }

#define EGL_PASSTHRU_4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                        \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    CONCAT(function, _hooktype)                                                              \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);                  \
    return real(p1, p2, p3, p4);                                                             \
  }

#define EGL_PASSTHRU_5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    CONCAT(function, _hooktype)                                                                     \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);                         \
    return real(p1, p2, p3, p4, p5);                                                                \
  }

/* EGL 1.0 */

EGL_PASSTHRU_5(EGLBoolean, eglChooseConfig, EGLDisplay, dpy, const EGLint *, attrib_list,
               EGLConfig *, configs, EGLint, config_size, EGLint *, num_config)
EGL_PASSTHRU_3(EGLBoolean, eglCopyBuffers, EGLDisplay, dpy, EGLSurface, surface,
               EGLNativePixmapType, target)
EGL_PASSTHRU_3(EGLSurface, eglCreatePbufferSurface, EGLDisplay, dpy, EGLConfig, config,
               const EGLint *, attrib_list)
EGL_PASSTHRU_4(EGLSurface, eglCreatePixmapSurface, EGLDisplay, dpy, EGLConfig, config,
               EGLNativePixmapType, pixmap, const EGLint *, attrib_list)
EGL_PASSTHRU_4(EGLSurface, eglCreateWindowSurface, EGLDisplay, dpy, EGLConfig, config,
               EGLNativeWindowType, win, const EGLint *, attrib_list)
EGL_PASSTHRU_2(EGLBoolean, eglDestroySurface, EGLDisplay, dpy, EGLSurface, surface)
EGL_PASSTHRU_4(EGLBoolean, eglGetConfigAttrib, EGLDisplay, dpy, EGLConfig, config, EGLint,
               attribute, EGLint *, value)
EGL_PASSTHRU_4(EGLBoolean, eglGetConfigs, EGLDisplay, dpy, EGLConfig *, configs, EGLint,
               config_size, EGLint *, num_config)
EGL_PASSTHRU_0(EGLDisplay, eglGetCurrentDisplay)
EGL_PASSTHRU_1(EGLSurface, eglGetCurrentSurface, EGLint, readdraw)
EGL_PASSTHRU_0(EGLint, eglGetError)
EGL_PASSTHRU_3(EGLBoolean, eglInitialize, EGLDisplay, dpy, EGLint *, major, EGLint *, minor)
EGL_PASSTHRU_4(EGLBoolean, eglQueryContext, EGLDisplay, dpy, EGLContext, ctx, EGLint, attribute,
               EGLint *, value)
EGL_PASSTHRU_2(const char *, eglQueryString, EGLDisplay, dpy, EGLint, name)
EGL_PASSTHRU_4(EGLBoolean, eglQuerySurface, EGLDisplay, dpy, EGLSurface, surface, EGLint, attribute,
               EGLint *, value)
EGL_PASSTHRU_1(EGLBoolean, eglTerminate, EGLDisplay, dpy)
EGL_PASSTHRU_0(EGLBoolean, eglWaitGL)
EGL_PASSTHRU_1(EGLBoolean, eglWaitNative, EGLint, engine)

/* EGL 1.1 */

EGL_PASSTHRU_3(EGLBoolean, eglBindTexImage, EGLDisplay, dpy, EGLSurface, surface, EGLint, buffer)
EGL_PASSTHRU_3(EGLBoolean, eglReleaseTexImage, EGLDisplay, dpy, EGLSurface, surface, EGLint, buffer)
EGL_PASSTHRU_4(EGLBoolean, eglSurfaceAttrib, EGLDisplay, dpy, EGLSurface, surface, EGLint,
               attribute, EGLint, value)
EGL_PASSTHRU_2(EGLBoolean, glSwapInterval, EGLDisplay, dpy, EGLint, interval)

/* EGL 1.2 */

EGL_PASSTHRU_1(EGLBoolean, eglBindAPI, EGLenum, api)
EGL_PASSTHRU_0(EGLenum, eglQueryAPI)
EGL_PASSTHRU_5(EGLSurface, eglCreatePbufferFromClientBuffer, EGLDisplay, dpy, EGLenum, buftype,
               EGLClientBuffer, buffer, EGLConfig, config, const EGLint *, attrib_list)
EGL_PASSTHRU_0(EGLBoolean, eglReleaseThread)
EGL_PASSTHRU_0(EGLBoolean, eglWaitClient)

/* EGL 1.4 */
EGL_PASSTHRU_0(EGLContext, eglGetCurrentContext)
