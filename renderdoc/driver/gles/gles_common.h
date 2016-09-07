#pragma once

#include "official/gl32.h"


#define GLNOTIMP(...) RDCDEBUG("GLES not implemented - " __VA_ARGS__)

#if defined(RENDERDOC_PLATFORM_LINUX)
#include "official/egl.h"

struct GLESWindowingData
{
  GLESWindowingData()
  {
    display = NULL;
    context = NULL;
    surface = 0;
  }

  void SetCtx(void *ctx) { context = (EGLContext)ctx; }

  Display *display;
  EGLDisplay eglDisplay;
  EGLContext context;
  EGLSurface surface;
};


#endif
