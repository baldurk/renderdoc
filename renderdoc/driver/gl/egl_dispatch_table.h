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

typedef EGLBoolean(EGLAPIENTRY *PFN_eglBindAPI)(EGLenum api);
typedef EGLDisplay(EGLAPIENTRY *PFN_eglGetDisplay)(EGLNativeDisplayType display_id);
typedef EGLContext(EGLAPIENTRY *PFN_eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                                      EGLContext share_context,
                                                      const EGLint *attrib_list);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw,
                                                    EGLSurface read, EGLContext ctx);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface,
                                                     EGLint attribute, EGLint *value);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
typedef EGLSurface(EGLAPIENTRY *PFN_eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                             const EGLint *attrib_list);
typedef EGLSurface(EGLAPIENTRY *PFN_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                                            EGLNativeWindowType win,
                                                            const EGLint *attrib_list);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                                     EGLConfig *configs, EGLint config_size,
                                                     EGLint *num_config);
typedef __eglMustCastToProperFunctionPointerType(EGLAPIENTRY *PFN_eglGetProcAddress)(
    const char *procname);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
typedef EGLContext(EGLAPIENTRY *PFN_eglGetCurrentContext)(void);
typedef EGLDisplay(EGLAPIENTRY *PFN_eglGetCurrentDisplay)(void);
typedef EGLSurface(EGLAPIENTRY *PFN_eglGetCurrentSurface)(EGLint readdraw);
typedef EGLint(EGLAPIENTRY *PFN_eglGetError)(void);
typedef EGLBoolean(EGLAPIENTRY *PFN_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config,
                                                        EGLint attribute, EGLint *value);
typedef const char *(EGLAPIENTRY *PFN_eglQueryString)(EGLDisplay dpy, EGLint name);
typedef PFNEGLPOSTSUBBUFFERNVPROC PFN_eglPostSubBufferNV;
typedef PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC PFN_eglSwapBuffersWithDamageEXT;
typedef PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC PFN_eglSwapBuffersWithDamageKHR;

#define EGL_HOOKED_SYMBOLS(FUNC)        \
  FUNC(BindAPI, false);                 \
  FUNC(GetProcAddress, false);          \
  FUNC(GetDisplay, false);              \
  FUNC(CreateContext, false);           \
  FUNC(DestroyContext, false);          \
  FUNC(CreateWindowSurface, false);     \
  FUNC(MakeCurrent, false);             \
  FUNC(SwapBuffers, false);             \
  FUNC(PostSubBufferNV, true);          \
  FUNC(SwapBuffersWithDamageEXT, true); \
  FUNC(SwapBuffersWithDamageKHR, true);

#define EGL_NONHOOKED_SYMBOLS(FUNC)  \
  FUNC(ChooseConfig, false);         \
  FUNC(CreatePbufferSurface, false); \
  FUNC(DestroySurface, false);       \
  FUNC(GetConfigAttrib, false);      \
  FUNC(GetCurrentContext, false);    \
  FUNC(GetCurrentDisplay, false);    \
  FUNC(GetCurrentSurface, false);    \
  FUNC(GetError, false);             \
  FUNC(Initialize, false);           \
  FUNC(QueryString, false);          \
  FUNC(QuerySurface, false);

struct EGLDispatchTable
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

// Generate the EGL function pointers. We need to consider hooked and non-hooked symbols separately
// - non-hooked symbols don't have a function hook to register, or if they do it's a dummy
// pass-through hook that will risk calling itself via trampoline.
#define EGL_PTR_GEN(func, isext) CONCAT(PFN_egl, func) func;
  EGL_HOOKED_SYMBOLS(EGL_PTR_GEN)
  EGL_NONHOOKED_SYMBOLS(EGL_PTR_GEN)
#undef EGL_PTR_GEN
};

extern EGLDispatchTable EGL;