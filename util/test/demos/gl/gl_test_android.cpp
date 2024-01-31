/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "3rdparty/glad/glad_egl.h"

#include "../android/android_window.h"

EGLDisplay eglDisplay;
EGLConfig config;

void OpenGLGraphicsTest::Prepare(int argc, char **argv)
{
  GraphicsTest::Prepare(argc, argv);

  static bool prepared = false;
  static void *libEGL = NULL;

  if(!prepared)
  {
    prepared = true;

    libEGL = dlopen("libEGL.so", RTLD_GLOBAL | RTLD_NOW);
  }

  if(!libEGL)
    Avail = "libEGL.so is not available";
}

bool OpenGLGraphicsTest::Init()
{
  if(!GraphicsTest::Init())
    return false;

  gladLoadEGL();

  eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  TEST_LOG("android display %p %d", eglDisplay, eglGetError());

  int major = 0, minor = 0;
  EGLBoolean initialised = eglInitialize(eglDisplay, &major, &minor);
  TEST_LOG("android init %d %d => %d", major, minor, initialised);

  const EGLint configAttribs[] = {EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_SURFACE_TYPE,
                                  EGL_WINDOW_BIT,
                                  EGL_COLOR_BUFFER_TYPE,
                                  EGL_RGB_BUFFER,
                                  EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES2_BIT,
                                  EGL_NONE};

  EGLint numConfigs;
  eglChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs);

  TEST_LOG("android config %p %d (%d)", config, eglGetError(), numConfigs);

  mainWindow = new AndroidWindow(screenWidth, screenHeight, screenTitle);

  mainContext = MakeContext(mainWindow, NULL);

  if(!mainWindow || !mainContext)
  {
    delete mainWindow;
    TEST_ERROR("Couldn't initialise context");
    return false;
  }

  ActivateContext(mainWindow, mainContext);

  if(!gladLoadGLES2Loader((GLADloadproc)eglGetProcAddress))
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
  return new AndroidWindow(width, height, title);
}

void *OpenGLGraphicsTest::MakeContext(GraphicsWindow *win, void *share)
{
  AndroidWindow *droidwin = (AndroidWindow *)win;

  int attribs[64] = {0};
  int i = 0;

  attribs[i++] = EGL_CONTEXT_CLIENT_VERSION;
  attribs[i++] = 3;
  attribs[i++] = EGL_CONTEXT_FLAGS_KHR;
  if(debugDevice)
    attribs[i++] = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
  else
    attribs[i++] = 0;
  attribs[i++] = EGL_NONE;
  attribs[i++] = EGL_NONE;

  EGLContext ctx = eglCreateContext(eglDisplay, config, share, attribs);

  TEST_LOG("android context %p %d", ctx, eglGetError());
  droidwin->surface = eglCreateWindowSurface(eglDisplay, config, droidwin->window, NULL);
  TEST_LOG("android surface %p %d", droidwin->surface, eglGetError());

  return ctx;
}

void OpenGLGraphicsTest::DestroyContext(void *ctx)
{
  if(ctx == NULL)
    return;

  eglMakeCurrent(eglDisplay, 0L, 0L, NULL);
  eglDestroyContext(eglDisplay, ctx);
}

void OpenGLGraphicsTest::ActivateContext(GraphicsWindow *win, void *ctx, bool alt)
{
  AndroidWindow *droidwin = (AndroidWindow *)win;

  if(ctx == NULL)
  {
    if(win == NULL)
      return;

    eglMakeCurrent(eglDisplay, NULL, NULL, NULL);
    return;
  }

  eglMakeCurrent(eglDisplay, droidwin->surface, droidwin->surface, (EGLContext)ctx);
}

void OpenGLGraphicsTest::Present(GraphicsWindow *window)
{
  AndroidWindow *droidwin = (AndroidWindow *)window;

  eglSwapBuffers(eglDisplay, droidwin->surface);
}
