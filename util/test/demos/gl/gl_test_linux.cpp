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
#include <dlfcn.h>
#include <stdio.h>
#include "3rdparty/glad/glad_glx.h"

#include "../linux/linux_window.h"

void OpenGLGraphicsTest::Prepare(int argc, char **argv)
{
  GraphicsTest::Prepare(argc, argv);

  static bool prepared = false;
  static void *libGL = NULL;

  if(!prepared)
  {
    prepared = true;

    libGL = dlopen("libGL.so", RTLD_GLOBAL | RTLD_NOW);
  }

  if(!libGL)
    Avail = "libGL.so is not available";
}

bool OpenGLGraphicsTest::Init()
{
  if(!GraphicsTest::Init())
    return false;

  X11Window::Init();

  Display *dpy = X11Window::GetDisplay();

  gladLoadGLX(dpy, DefaultScreen(dpy));

  mainWindow = new X11Window(screenWidth, screenHeight, screenTitle);

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
  return new X11Window(width, height, title);
}

void *OpenGLGraphicsTest::MakeContext(GraphicsWindow *win, void *share)
{
  if(!GLAD_GLX_ARB_create_context_profile)
  {
    TEST_ERROR("Need GLX_ARB_create_context_profile to initialise");
    return NULL;
  }

  X11Window *x11win = (X11Window *)win;

  int attribs[64] = {0};
  int i = 0;

  attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
  attribs[i++] = glMajor;
  attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
  attribs[i++] = glMinor;
  attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
  if(debugDevice)
    attribs[i++] = GLX_CONTEXT_DEBUG_BIT_ARB;
  else
    attribs[i++] = 0;
  attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
  if(gles)
    attribs[i++] = GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
  else if(coreProfile)
    attribs[i++] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
  else
    attribs[i++] = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

  // GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB MUST be the last attrib so that we can remove it to retry
  // if we find no srgb fbconfigs
  static int visAttribs[] = {GLX_X_RENDERABLE,
                             True,
                             GLX_DRAWABLE_TYPE,
                             GLX_WINDOW_BIT,
                             GLX_RENDER_TYPE,
                             GLX_RGBA_BIT,
                             GLX_X_VISUAL_TYPE,
                             GLX_TRUE_COLOR,
                             GLX_RED_SIZE,
                             8,
                             GLX_GREEN_SIZE,
                             8,
                             GLX_BLUE_SIZE,
                             8,
                             GLX_DOUBLEBUFFER,
                             True,
                             GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB,
                             True,
                             0};

  int numCfgs = 0;
  GLXFBConfig *fbcfg = glXChooseFBConfig(x11win->xlib.display, DefaultScreen(x11win->xlib.display),
                                         visAttribs, &numCfgs);

  if(fbcfg == NULL)
  {
    const size_t len = ARRAY_COUNT(visAttribs);
    if(visAttribs[len - 3] != GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB)
    {
      TEST_ERROR(
          "GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB isn't the last attribute, and no SRGB fbconfigs were "
          "found!");
    }
    else
    {
      visAttribs[len - 3] = 0;
      fbcfg = glXChooseFBConfig(x11win->xlib.display, DefaultScreen(x11win->xlib.display),
                                visAttribs, &numCfgs);
    }
  }

  if(fbcfg == NULL)
  {
    TEST_ERROR("Couldn't choose default framebuffer config");
    return NULL;
  }

  // Choose FB config with a GLX_VISUAL_ID that matches the X screen.
  VisualID visualid_correct =
      DefaultVisual(x11win->xlib.display, DefaultScreen(x11win->xlib.display))->visualid;
  for(int i = 0; i < numCfgs; i++)
  {
    int visualid;
    glXGetFBConfigAttrib(x11win->xlib.display, fbcfg[i], GLX_VISUAL_ID, &visualid);
    if((VisualID)visualid == visualid_correct)
    {
      fbcfg[0] = fbcfg[i];
      break;
    }
  }

  GLXContext ctx =
      glXCreateContextAttribsARB(x11win->xlib.display, fbcfg[0], (GLXContext)share, true, attribs);

  XFree(fbcfg);

  return ctx;
}

void OpenGLGraphicsTest::DestroyContext(void *ctx)
{
  if(ctx == NULL)
    return;

  X11Window *x11win = (X11Window *)mainWindow;

  // we assume the display pointer is shared among all windows
  glXDestroyContext(x11win->xlib.display, (GLXContext)ctx);
}

void OpenGLGraphicsTest::ActivateContext(GraphicsWindow *win, void *ctx)
{
  X11Window *x11win = (X11Window *)win;

  glXMakeContextCurrent(x11win->xlib.display, x11win->xlib.window, x11win->xlib.window,
                        (GLXContext)ctx);
}

void OpenGLGraphicsTest::Present(GraphicsWindow *window)
{
  X11Window *x11win = (X11Window *)window;

  glXSwapBuffers(x11win->xlib.display, x11win->xlib.window);
}