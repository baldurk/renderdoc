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

#pragma once

#include "gl_common.h"

// glX functions
typedef GLXContext (*PFN_glXCreateContext)(Display *dpy, XVisualInfo *vis, GLXContext shareList,
                                           Bool direct);
typedef void (*PFN_glXDestroyContext)(Display *dpy, GLXContext ctx);
typedef Bool (*PFN_glXMakeCurrent)(Display *dpy, GLXDrawable drawable, GLXContext ctx);
typedef void (*PFN_glXSwapBuffers)(Display *dpy, GLXDrawable drawable);
typedef int (*PFN_glXGetConfig)(Display *dpy, XVisualInfo *vis, int attrib, int *value);
typedef int (*PFN_glXQueryContext)(Display *dpy, GLXContext ctx, int attribute, int *value);
typedef Bool (*PFN_glXIsDirect)(Display *dpy, GLXContext ctx);
typedef __GLXextFuncPtr (*PFN_glXGetProcAddress)(const GLubyte *);
typedef __GLXextFuncPtr (*PFN_glXGetProcAddressARB)(const GLubyte *);
typedef GLXContext (*PFN_glXGetCurrentContext)();
typedef const char *(*PFN_glXQueryExtensionsString)(Display *dpy, int screen);
typedef PFNGLXGETVISUALFROMFBCONFIGPROC PFN_glXGetVisualFromFBConfig;
typedef PFNGLXMAKECONTEXTCURRENTPROC PFN_glXMakeContextCurrent;
typedef PFNGLXCHOOSEFBCONFIGPROC PFN_glXChooseFBConfig;
typedef PFNGLXGETFBCONFIGATTRIBPROC PFN_glXGetFBConfigAttrib;
typedef PFNGLXQUERYDRAWABLEPROC PFN_glXQueryDrawable;
typedef PFNGLXCREATEPBUFFERPROC PFN_glXCreatePbuffer;
typedef PFNGLXDESTROYPBUFFERPROC PFN_glXDestroyPbuffer;
typedef PFNGLXCREATECONTEXTATTRIBSARBPROC PFN_glXCreateContextAttribsARB;

// gl functions (used for quad rendering on legacy contexts)
typedef PFNGLGETINTEGERVPROC PFN_glGetIntegerv;
typedef void (*PFN_glPushMatrix)();
typedef void (*PFN_glLoadIdentity)();
typedef void (*PFN_glMatrixMode)(GLenum);
typedef void (*PFN_glOrtho)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
typedef void (*PFN_glPopMatrix)();
typedef void (*PFN_glBegin)(GLenum);
typedef void (*PFN_glVertex2f)(float, float);
typedef void (*PFN_glTexCoord2f)(float, float);
typedef void (*PFN_glEnd)();

#define GLX_HOOKED_SYMBOLS(FUNC)    \
  FUNC(glXGetProcAddress);          \
  FUNC(glXGetProcAddressARB);       \
  FUNC(glXCreateContext);           \
  FUNC(glXDestroyContext);          \
  FUNC(glXCreateContextAttribsARB); \
  FUNC(glXMakeCurrent);             \
  FUNC(glXMakeContextCurrent);      \
  FUNC(glXSwapBuffers);

#define GLX_NONHOOKED_SYMBOLS(FUNC) \
  FUNC(glXGetCurrentContext);       \
  FUNC(glXGetConfig);               \
  FUNC(glXQueryContext);            \
  FUNC(glXIsDirect);                \
  FUNC(glXGetVisualFromFBConfig);   \
  FUNC(glXChooseFBConfig);          \
  FUNC(glXGetFBConfigAttrib);       \
  FUNC(glXQueryDrawable);           \
  FUNC(glXQueryExtensionsString);   \
  FUNC(glXCreatePbuffer);           \
  FUNC(glXDestroyPbuffer);          \
  FUNC(glGetIntegerv);              \
  FUNC(glPushMatrix);               \
  FUNC(glLoadIdentity);             \
  FUNC(glMatrixMode);               \
  FUNC(glOrtho);                    \
  FUNC(glPopMatrix);                \
  FUNC(glBegin);                    \
  FUNC(glVertex2f);                 \
  FUNC(glTexCoord2f);               \
  FUNC(glEnd);

struct GLXDispatchTable
{
  // since on posix systems we need to export the functions that we're hooking, that means on replay
  // we can't avoid coming back into those hooks again. We have a single 'hookset' that we use for
  // dispatch during capture and on replay, but it's populated in different ways.
  //
  // During capture the hooking process is the primary way of filling in the real function pointers.
  // While during replay we explicitly fill it outo the first time we need it.
  //
  // Note that we still assume all functions are populated (either with trampolines or the real
  // function pointer) by the hooking process while injected - hence the name 'PopulateForReplay'.
  bool PopulateForReplay();

// Generate the GLX function pointers. We need to consider hooked and non-hooked symbols separately
// - non-hooked symbols don't have a function hook to register, or if they do it's a dummy
// pass-through hook that will risk calling itself via trampoline.
#define GLX_PTR_GEN(func) CONCAT(PFN_, func) func;
  GLX_HOOKED_SYMBOLS(GLX_PTR_GEN)
  GLX_NONHOOKED_SYMBOLS(GLX_PTR_GEN)
#undef GLX_PTR_GEN
};

extern GLXDispatchTable GLX;