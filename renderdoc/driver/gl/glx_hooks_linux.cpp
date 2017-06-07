/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

// we need to export the whole of the GLX API, since we will have redirected any dlopen()
// for libGL.so to ourselves, and dlsym() for any of these entry points must return a valid
// function. We don't need to intercept them, so we just pass it along

extern void *libGLdlsymHandle;

/*
  in bash:

    function GLXHook()
    {
        N=$1;
        echo -n "#define GLX_PASSTHRU_$N(ret, function";
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

  for I in `seq 0 5`; do GLXHook $I; echo; done

  */

#define GLX_PASSTHRU_0(ret, function)                                       \
  typedef ret (*CONCAT(function, _hooktype))();                             \
  extern "C" __attribute__((visibility("default"))) ret function()          \
  {                                                                         \
    CONCAT(function, _hooktype)                                             \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function); \
    return real();                                                          \
  }

#define GLX_PASSTHRU_1(ret, function, t1, p1)                               \
  typedef ret (*CONCAT(function, _hooktype))(t1);                           \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1)     \
  {                                                                         \
    CONCAT(function, _hooktype)                                             \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function); \
    return real(p1);                                                        \
  }

#define GLX_PASSTHRU_2(ret, function, t1, p1, t2, p2)                          \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2);                          \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2) \
  {                                                                            \
    CONCAT(function, _hooktype)                                                \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);    \
    return real(p1, p2);                                                       \
  }

#define GLX_PASSTHRU_3(ret, function, t1, p1, t2, p2, t3, p3)                         \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3);                             \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3) \
  {                                                                                   \
    CONCAT(function, _hooktype)                                                       \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);           \
    return real(p1, p2, p3);                                                          \
  }

#define GLX_PASSTHRU_4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                        \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4);                                \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                                          \
    CONCAT(function, _hooktype)                                                              \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);                  \
    return real(p1, p2, p3, p4);                                                             \
  }

#define GLX_PASSTHRU_5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)                       \
  typedef ret (*CONCAT(function, _hooktype))(t1, t2, t3, t4, t5);                                   \
  extern "C" __attribute__((visibility("default"))) ret function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                                 \
    CONCAT(function, _hooktype)                                                                     \
    real = (CONCAT(function, _hooktype))dlsym(libGLdlsymHandle, #function);                         \
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
