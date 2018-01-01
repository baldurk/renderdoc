/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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

typedef EGLBoolean (*PFN_eglBindAPI)(EGLenum api);
typedef EGLDisplay (*PFN_eglGetDisplay)(EGLNativeDisplayType display_id);
typedef EGLContext (*PFN_eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                           EGLContext share_context, const EGLint *attrib_list);
typedef EGLBoolean (*PFN_eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                                         EGLContext ctx);
typedef EGLBoolean (*PFN_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
typedef EGLBoolean (*PFN_eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
typedef EGLBoolean (*PFN_eglQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
                                          EGLint *value);
typedef EGLBoolean (*PFN_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
typedef EGLSurface (*PFN_eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                  const EGLint *attrib_list);
typedef EGLSurface (*PFN_eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                                 EGLNativeWindowType win, const EGLint *attrib_list);
typedef EGLBoolean (*PFN_eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                          EGLConfig *configs, EGLint config_size, EGLint *num_config);
typedef __eglMustCastToProperFunctionPointerType (*PFN_eglGetProcAddress)(const char *procname);
typedef EGLBoolean (*PFN_eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
typedef EGLContext (*PFN_eglGetCurrentContext)(void);
typedef EGLDisplay (*PFN_eglGetCurrentDisplay)(void);
typedef EGLSurface (*PFN_eglGetCurrentSurface)(EGLint readdraw);
typedef EGLint (*PFN_eglGetError)(void);
typedef EGLBoolean (*PFN_eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute,
                                             EGLint *value);

#define EGL_SYMBOLS(FUNC)     \
  FUNC(BindAPI);              \
  FUNC(ChooseConfig);         \
  FUNC(CreateContext);        \
  FUNC(CreatePbufferSurface); \
  FUNC(CreateWindowSurface);  \
  FUNC(DestroyContext);       \
  FUNC(DestroySurface);       \
  FUNC(GetConfigAttrib);      \
  FUNC(GetCurrentContext);    \
  FUNC(GetCurrentDisplay);    \
  FUNC(GetCurrentSurface);    \
  FUNC(GetDisplay);           \
  FUNC(GetError);             \
  FUNC(GetProcAddress);       \
  FUNC(Initialize);           \
  FUNC(MakeCurrent);          \
  FUNC(QuerySurface);         \
  FUNC(SwapBuffers);

class EGLPointers
{
public:
  EGLPointers() : m_initialized(false) {}
  bool IsInitialized() const { return m_initialized; }
  bool LoadSymbolsFrom(void *lib_handle);

// Generate the EGL function pointers
#define EGL_PTR_GEN(SYMBOL_NAME) PFN_egl##SYMBOL_NAME SYMBOL_NAME = NULL;
  EGL_SYMBOLS(EGL_PTR_GEN)
#undef EGL_PTR_GEN

private:
  bool m_initialized;
};

GLWindowingData CreateWindowingData(const EGLPointers &egl, EGLDisplay eglDisplay,
                                    EGLContext share_ctx, EGLNativeWindowType window);
