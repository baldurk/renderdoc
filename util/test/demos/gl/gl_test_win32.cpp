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

#include "gl_test.h"
#include <stdio.h>
#include "3rdparty/glad/glad_wgl.h"

#include "../win32/win32_window.h"

typedef BOOL(WINAPI *PFN_wglMakeCurrent)(HDC, HGLRC);
typedef HGLRC(WINAPI *PFN_wglCreateContext)(HDC);
typedef BOOL(WINAPI *PFN_wglDeleteContext)(HGLRC);

namespace
{
PFN_wglMakeCurrent makeCurrent = NULL;
PFN_wglCreateContext createContext = NULL;
PFN_wglDeleteContext deleteContext = NULL;
};

void OpenGLGraphicsTest::Prepare(int argc, char **argv)
{
  GraphicsTest::Prepare(argc, argv);

  static bool prepared = false;

  if(!prepared)
  {
    prepared = true;

    HMODULE opengl = LoadLibraryA("opengl32.dll");

    if(opengl)
    {
      makeCurrent = (PFN_wglMakeCurrent)GetProcAddress(opengl, "wglMakeCurrent");
      createContext = (PFN_wglCreateContext)GetProcAddress(opengl, "wglCreateContext");
      deleteContext = (PFN_wglDeleteContext)GetProcAddress(opengl, "wglDeleteContext");
    }
  }

  if(!makeCurrent)
    Avail = "opengl32.dll is not available";
}

bool OpenGLGraphicsTest::Init()
{
  if(!GraphicsTest::Init())
    return false;

  mainWindow = new Win32Window(screenWidth, screenHeight, screenTitle);

  if(!mainWindow)
  {
    TEST_ERROR("Couldn't initialise window");
    return false;
  }

  Win32Window *win32win = (Win32Window *)mainWindow;

  HDC dc = GetDC(win32win->wnd);

  PIXELFORMATDESCRIPTOR pfd = {0};
  pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iLayerType = PFD_MAIN_PLANE;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 0;
  pfd.cStencilBits = 0;

  int pf = ::ChoosePixelFormat(dc, &pfd);
  ::SetPixelFormat(dc, pf, &pfd);

  HGLRC rc = createContext(dc);

  makeCurrent(dc, rc);

  gladLoadWGL(dc);

  makeCurrent(NULL, NULL);

  ReleaseDC(win32win->wnd, dc);
  deleteContext(rc);

  mainContext = MakeContext(mainWindow, NULL);

  if(!mainWindow || !mainContext)
  {
    delete mainWindow;
    TEST_ERROR("Couldn't initialise context");
    return false;
  }

  ActivateContext(mainWindow, mainContext);

  if(!gladLoadGL())
  {
    delete mainWindow;
    TEST_ERROR("Error initialising glad");
    return false;
  }

  PostInit();

  return true;
}

GraphicsWindow *OpenGLGraphicsTest::MakeWindow(int width, int height, const char *title)
{
  if(!GLAD_WGL_ARB_pixel_format)
  {
    TEST_ERROR("Need WGL_ARB_pixel_format to initialise");
    return NULL;
  }

  Win32Window *win32win = new Win32Window(width, height, title);

  HDC dc = GetDC(win32win->wnd);

  int attrib = WGL_NUMBER_PIXEL_FORMATS_ARB;
  int value = 1;

  wglGetPixelFormatAttribivARB(dc, 1, 0, 1, &attrib, &value);

  int pf = 0;

  int numpfs = value;
  for(int i = 1; i <= numpfs; i++)
  {
    // verify that we have the properties we want
    attrib = WGL_DRAW_TO_WINDOW_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    attrib = WGL_ACCELERATION_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value == WGL_NO_ACCELERATION_ARB)
      continue;

    attrib = WGL_SUPPORT_OPENGL_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    attrib = WGL_DOUBLE_BUFFER_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    attrib = WGL_PIXEL_TYPE_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value != WGL_TYPE_RGBA_ARB)
      continue;

    attrib = WGL_COLOR_BITS_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value < 24)
      continue;

    attrib = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
    wglGetPixelFormatAttribivARB(dc, i, 0, 1, &attrib, &value);
    if(value == 0)
      continue;

    // this one suits our needs, choose it
    pf = i;
    break;
  }

  if(pf == 0)
  {
    ReleaseDC(win32win->wnd, dc);
    TEST_ERROR("Couldn't choose pixel format");
    delete win32win;
    return NULL;
  }

  PIXELFORMATDESCRIPTOR pfd = {0};
  DescribePixelFormat(dc, pf, sizeof(pfd), &pfd);
  SetPixelFormat(dc, pf, &pfd);

  ReleaseDC(win32win->wnd, dc);

  return win32win;
}

void *OpenGLGraphicsTest::MakeContext(GraphicsWindow *win, void *share)
{
  if(!GLAD_WGL_ARB_create_context_profile)
  {
    TEST_ERROR("Need WGL_ARB_create_context_profile to initialise");
    return NULL;
  }

  Win32Window *win32win = (Win32Window *)win;

  int attribs[64] = {0};
  int i = 0;

  attribs[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
  attribs[i++] = glMajor;
  attribs[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
  attribs[i++] = glMinor;
  attribs[i++] = WGL_CONTEXT_FLAGS_ARB;
  if(debugDevice)
    attribs[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
  else
    attribs[i++] = 0;
  attribs[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
  if(gles)
    attribs[i++] = WGL_CONTEXT_ES2_PROFILE_BIT_EXT;
  else if(coreProfile)
    attribs[i++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
  else
    attribs[i++] = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

  HDC dc = GetDC(win32win->wnd);

  HGLRC ctx = wglCreateContextAttribsARB(dc, (HGLRC)share, attribs);

  ReleaseDC(win32win->wnd, dc);

  return ctx;
}

void OpenGLGraphicsTest::DestroyContext(void *ctx)
{
  if(ctx == NULL)
    return;

  deleteContext((HGLRC)ctx);
}

void OpenGLGraphicsTest::ActivateContext(GraphicsWindow *win, void *ctx)
{
  Win32Window *win32win = (Win32Window *)win;

  HDC dc = GetDC(win32win->wnd);

  makeCurrent(dc, (HGLRC)ctx);

  ReleaseDC(win32win->wnd, dc);
}

void OpenGLGraphicsTest::Present(GraphicsWindow *window)
{
  Win32Window *win32win = (Win32Window *)window;

  HDC dc = GetDC(win32win->wnd);

  SwapBuffers(dc);

  ReleaseDC(win32win->wnd, dc);
}