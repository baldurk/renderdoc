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

#include "gl_replay.h"

#include <dlfcn.h>
#include "serialise/rdcfile.h"
#include "gl_driver.h"
#include "gl_hooks_linux_shared.h"
#include "gl_library_egl.h"
#include "gl_resources.h"

static EGLPointers egl;

void PopulateEGLFunctions();
GLPlatform &GetGLPlatformEGL();

ReplayStatus GLES_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL ES replay device");

  if(!egl.IsInitialized())
  {
#if ENABLED(RDOC_ANDROID)
    libGLdlsymHandle = dlopen("libEGL.so", RTLD_NOW);
#endif
    bool load_ok = egl.LoadSymbolsFrom(libGLdlsymHandle);

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
      RDCERR("Incompatible OpenGL serialise version %llu", ver);
      return ReplayStatus::APIIncompatibleVersion;
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

  GLWindowingData data = CreateWindowingData(egl, eglDisplay, EGL_NO_CONTEXT, 0);

  if(data.egl_ctx == NULL)
  {
    RDCERR("Couldn't create GL ES 3.x context - RenderDoc requires OpenGL ES 3.x availability");
    return ReplayStatus::APIHardwareUnsupported;
  }

  if(data.egl_wnd == NULL)
  {
    RDCERR("Couldn't create a suitable PBuffer");
    egl.DestroyContext(eglDisplay, data.egl_ctx);
    return ReplayStatus::APIInitFailed;
  }

  EGLBoolean res = egl.MakeCurrent(eglDisplay, data.egl_wnd, data.egl_wnd, data.egl_ctx);
  if(!res)
  {
    RDCERR("Couldn't active the created GL ES context");
    egl.DestroySurface(eglDisplay, data.egl_wnd);
    egl.DestroyContext(eglDisplay, data.egl_ctx);
    return ReplayStatus::APIInitFailed;
  }

  // TODO: add extesion check just like in the GL case.

  PopulateEGLFunctions();
  bool extensionsValidated = ValidateFunctionPointers();
  if(!extensionsValidated)
  {
    egl.DestroySurface(eglDisplay, data.egl_wnd);
    egl.DestroyContext(eglDisplay, data.egl_ctx);
    return ReplayStatus::APIHardwareUnsupported;
  }

  WrappedOpenGL *gl = new WrappedOpenGL(GetGLPlatformEGL());
  gl->SetDriverType(RDCDriver::OpenGLES);

  RDCLOG("Created OPEN GL ES replay device.");
  GLReplay *replay = gl->GetReplay();
  replay->SetProxy(rdc == NULL);
  replay->SetReplayData(data);

  gl->Initialise(initParams, ver);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}
