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

#include "gl_library_egl.h"
#include <dlfcn.h>

bool EGLPointers::LoadSymbolsFrom(void *lib_handle)
{
  if(m_initialized)
  {
    RDCDEBUG("EGL function pointers already loaded, skipping");
    return m_initialized;
  }

  bool symbols_ok = true;
#define LOAD_SYM(SYMBOL_NAME)                                                        \
  do                                                                                 \
  {                                                                                  \
    this->SYMBOL_NAME = (PFN_egl##SYMBOL_NAME)dlsym(lib_handle, "egl" #SYMBOL_NAME); \
    if(this->SYMBOL_NAME == NULL)                                                    \
    {                                                                                \
      symbols_ok = false;                                                            \
      RDCWARN("Unable to load symbol: %s", #SYMBOL_NAME);                            \
    }                                                                                \
  } while(0)

  EGL_SYMBOLS(LOAD_SYM)

#undef LOAD_SYM
  m_initialized = symbols_ok;
  return symbols_ok;
}

GLWindowingData CreateWindowingData(const EGLPointers &egl, EGLDisplay eglDisplay,
                                    EGLContext share_ctx, EGLNativeWindowType window)
{
  GLWindowingData ret;
  ret.egl_dpy = eglDisplay;
  ret.egl_ctx = NULL;
  ret.egl_wnd = NULL;

  EGLint surfaceType = (window == 0) ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT;
  const EGLint configAttribs[] = {EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES3_BIT,
                                  EGL_CONFORMANT,
                                  EGL_OPENGL_ES3_BIT,
                                  EGL_SURFACE_TYPE,
                                  surfaceType,
                                  EGL_COLOR_BUFFER_TYPE,
                                  EGL_RGB_BUFFER,
                                  EGL_NONE};

  EGLint numConfigs;
  EGLConfig config;
  if(!egl.ChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs))
  {
    RDCERR("Couldn't find a suitable EGL config");
    return ret;
  }

  static const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                      EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

  EGLContext ctx = egl.CreateContext(eglDisplay, config, share_ctx, ctxAttribs);
  if(ctx == NULL)
  {
    RDCERR("Couldn't create GL ES context");
    return ret;
  }
  ret.egl_ctx = ctx;

  EGLSurface surface = 0;
  if(window != 0)
  {
    surface = egl.CreateWindowSurface(eglDisplay, config, window, NULL);
  }
  else
  {
    static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
    surface = egl.CreatePbufferSurface(eglDisplay, config, pbAttribs);
  }

  ret.egl_wnd = surface;

  return ret;
}
