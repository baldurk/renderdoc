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

// exported wgl functions
typedef HGLRC(WINAPI *PFN_wglCreateContext)(HDC);
typedef BOOL(WINAPI *PFN_wglDeleteContext)(HGLRC);
typedef BOOL(WINAPI *PFN_wglShareLists)(HGLRC, HGLRC);
typedef HGLRC(WINAPI *PFN_wglCreateLayerContext)(HDC, int);
typedef BOOL(WINAPI *PFN_wglMakeCurrent)(HDC, HGLRC);
typedef PROC(WINAPI *PFN_wglGetProcAddress)(const char *);
typedef BOOL(WINAPI *PFN_wglSwapBuffers)(HDC);
typedef BOOL(WINAPI *PFN_wglSwapLayerBuffers)(HDC, UINT);
typedef BOOL(WINAPI *PFN_wglSwapMultipleBuffers)(UINT, CONST WGLSWAP *);
typedef HGLRC(WINAPI *PFN_wglGetCurrentContext)();
typedef HDC(WINAPI *PFN_wglGetCurrentDC)();

// wgl extensions
typedef PFNWGLCREATECONTEXTATTRIBSARBPROC PFN_wglCreateContextAttribsARB;
typedef PFNWGLGETPIXELFORMATATTRIBIVARBPROC PFN_wglGetPixelFormatAttribivARB;
typedef PFNWGLGETEXTENSIONSSTRINGEXTPROC PFN_wglGetExtensionsStringEXT;
typedef PFNWGLGETEXTENSIONSSTRINGARBPROC PFN_wglGetExtensionsStringARB;

// gl functions (used for quad rendering on legacy contexts)
typedef PFNGLGETINTEGERVPROC PFN_glGetIntegerv;
typedef void(WINAPI *PFN_glPushMatrix)();
typedef void(WINAPI *PFN_glLoadIdentity)();
typedef void(WINAPI *PFN_glMatrixMode)(GLenum);
typedef void(WINAPI *PFN_glOrtho)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
typedef void(WINAPI *PFN_glPopMatrix)();
typedef void(WINAPI *PFN_glBegin)(GLenum);
typedef void(WINAPI *PFN_glVertex2f)(float, float);
typedef void(WINAPI *PFN_glTexCoord2f)(float, float);
typedef void(WINAPI *PFN_glEnd)();

// non wgl functions
typedef BOOL(WINAPI *PFN_SwapBuffers)(HDC);
typedef LONG(WINAPI *PFN_ChangeDisplaySettingsA)(DEVMODEA *, DWORD);
typedef LONG(WINAPI *PFN_ChangeDisplaySettingsW)(DEVMODEW *, DWORD);
typedef LONG(WINAPI *PFN_ChangeDisplaySettingsExA)(LPCSTR, DEVMODEA *, HWND, DWORD, LPVOID);
typedef LONG(WINAPI *PFN_ChangeDisplaySettingsExW)(LPCWSTR, DEVMODEW *, HWND, DWORD, LPVOID);

#define WGL_HOOKED_SYMBOLS(FUNC)                \
  FUNC("opengl32.dll", wglCreateContext);       \
  FUNC("opengl32.dll", wglDeleteContext);       \
  FUNC("opengl32.dll", wglCreateLayerContext);  \
  FUNC("opengl32.dll", wglMakeCurrent);         \
  FUNC("opengl32.dll", wglGetProcAddress);      \
  FUNC("opengl32.dll", wglSwapBuffers);         \
  FUNC("opengl32.dll", wglShareLists);          \
  FUNC("opengl32.dll", wglSwapLayerBuffers);    \
  FUNC("opengl32.dll", wglSwapMultipleBuffers); \
  FUNC("", wglCreateContextAttribsARB);         \
  FUNC("gdi32.dll", SwapBuffers);               \
  FUNC("user32.dll", ChangeDisplaySettingsA);   \
  FUNC("user32.dll", ChangeDisplaySettingsW);   \
  FUNC("user32.dll", ChangeDisplaySettingsExA); \
  FUNC("user32.dll", ChangeDisplaySettingsExW);

#define WGL_NONHOOKED_SYMBOLS(FUNC)           \
  FUNC("opengl32.dll", wglGetCurrentContext); \
  FUNC("opengl32.dll", wglGetCurrentDC);      \
  FUNC("", wglGetPixelFormatAttribivARB);     \
  FUNC("", wglGetExtensionsStringEXT);        \
  FUNC("", wglGetExtensionsStringARB);        \
  FUNC("opengl32.dll", glGetIntegerv);        \
  FUNC("opengl32.dll", glPushMatrix);         \
  FUNC("opengl32.dll", glLoadIdentity);       \
  FUNC("opengl32.dll", glMatrixMode);         \
  FUNC("opengl32.dll", glOrtho);              \
  FUNC("opengl32.dll", glPopMatrix);          \
  FUNC("opengl32.dll", glBegin);              \
  FUNC("opengl32.dll", glVertex2f);           \
  FUNC("opengl32.dll", glTexCoord2f);         \
  FUNC("opengl32.dll", glEnd);

struct WGLDispatchTable
{
  // Although not needed on windows, for consistency we follow the same pattern as EGLDispatchTable
  // and GLXDispatchTable.
  //
  // Note that there's one exception here - a couple of wgl ARB functions cannot be populated here
  // since they have to be fetched after creating a context, so they are done manually.
  bool PopulateForReplay();

// Generate the WGL function pointers. We need to consider hooked and non-hooked symbols separately
// - non-hooked symbols don't have a function hook to register, or if they do it's a dummy
// pass-through hook that will risk calling itself via trampoline.
#define WGL_PTR_GEN(library, func) CONCAT(PFN_, func) func;
  WGL_HOOKED_SYMBOLS(WGL_PTR_GEN)
  WGL_NONHOOKED_SYMBOLS(WGL_PTR_GEN)
#undef WGL_PTR_GEN
};

extern WGLDispatchTable WGL;
