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

#include "gl_replay.h"

#include <dlfcn.h>
#include "serialise/rdcfile.h"
#include "gl_driver.h"
#include "gl_library_egl.h"
#include "gl_resources.h"

static EGLPointers egl;

const GLHookSet &GetRealGLFunctionsEGL();
GLPlatform &GetGLPlatformEGL();

ReplayStatus GLES_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL ES replay device");

  if(!egl.IsInitialized())
  {
    bool load_ok = egl.LoadSymbolsFrom(RTLD_NEXT);

    if(!load_ok)
    {
      RDCERR(
          "Couldn't find required function addresses, eglGetProcAddress eglCreateContext"
          "eglSwapBuffers (etc.)");
      return ReplayStatus::APIInitFailed;
    }
  }

  GLInitParams initParams;
  uint64_t ver = GLInitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!GLInitParams::IsSupportedVersion(ver))
    {
      RDCERR("Incompatible D3D11 serialise version %llu", ver);
      return ReplayStatus::APIUnsupported;
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RDCERR("Expected to get a DriverInit chunk, instead got %u", chunk);
      return ReplayStatus::FileCorrupted;
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      RDCERR("Failed reading driver init params.");
      return ReplayStatus::FileIOFailed;
    }
  }

#if ENABLED(RDOC_ANDROID)
  initParams.isSRGB = 0;
#endif

  egl.BindAPI(EGL_OPENGL_ES_API);

  EGLDisplay eglDisplay = egl.GetDisplay(EGL_DEFAULT_DISPLAY);
  if(!eglDisplay)
  {
    RDCERR("Couldn't open default EGL display");
    return ReplayStatus::APIInitFailed;
  }

  int major, minor;
  egl.Initialize(eglDisplay, &major, &minor);

  static const EGLint configAttribs[] = {EGL_RED_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_ES3_BIT,
                                         EGL_SURFACE_TYPE,
                                         EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
                                         EGL_NONE};
  EGLint numConfigs;
  EGLConfig config;

  if(!egl.ChooseConfig(eglDisplay, configAttribs, &config, 1, &numConfigs))
  {
    RDCERR("Couldn't find a suitable EGL config");
    return ReplayStatus::APIInitFailed;
  }

  static const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                      EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

  GLReplay::PreContextInitCounters();

  EGLContext ctx = egl.CreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
  if(ctx == NULL)
  {
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't create GL ES 3.x context - RenderDoc requires OpenGL ES 3.x availability");
    return ReplayStatus::APIHardwareUnsupported;
  }

  static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
  EGLSurface pbuffer = egl.CreatePbufferSurface(eglDisplay, config, pbAttribs);

  if(pbuffer == NULL)
  {
    RDCERR("Couldn't create a suitable PBuffer");
    egl.DestroySurface(eglDisplay, pbuffer);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  EGLBoolean res = egl.MakeCurrent(eglDisplay, pbuffer, pbuffer, ctx);
  if(!res)
  {
    RDCERR("Couldn't active the created GL ES context");
    egl.DestroySurface(eglDisplay, pbuffer);
    egl.DestroyContext(eglDisplay, ctx);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  // TODO: add extesion check just like in the GL case.

  const GLHookSet &real = GetRealGLFunctionsEGL();
  bool extensionsValidated = ValidateFunctionPointers(real);
  if(!extensionsValidated)
  {
    egl.DestroySurface(eglDisplay, pbuffer);
    egl.DestroyContext(eglDisplay, ctx);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIHardwareUnsupported;
  }

  WrappedOpenGL *gl = new WrappedOpenGL(real, GetGLPlatformEGL());
  gl->SetDriverType(RDC_OpenGLES);
  gl->Initialise(initParams, ver);

  RDCLOG("Created OPEN GL ES replay device.");
  GLReplay *replay = gl->GetReplay();
  replay->SetProxy(rdc == NULL);
  GLWindowingData data;
  data.egl_dpy = eglDisplay;
  data.egl_ctx = ctx;
  data.egl_wnd = pbuffer;
  replay->SetReplayData(data);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}
