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
#include "gl_resources.h"

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

PFN_eglBindAPI eglBindAPIProc = NULL;
PFN_eglInitialize eglInitializeProc = NULL;
PFN_eglGetDisplay eglGetDisplayProc = NULL;
PFN_eglCreateContext eglCreateContextProc = NULL;
PFN_eglMakeCurrent eglMakeCurrentProc = NULL;
PFN_eglSwapBuffers eglSwapBuffersProc = NULL;
PFN_eglDestroyContext eglDestroyContextProc = NULL;
PFN_eglQuerySurface eglQuerySurfaceProc = NULL;
PFN_eglDestroySurface eglDestroySurfaceProc = NULL;
PFN_eglCreatePbufferSurface eglCreatePbufferSurfaceProc = NULL;
PFN_eglCreateWindowSurface eglCreateWindowSurfaceProc = NULL;
PFN_eglChooseConfig eglChooseConfigProc = NULL;
PFN_eglGetProcAddress eglGetProcAddressProc = NULL;

const GLHookSet &GetRealGLFunctionsEGL();
GLPlatform &GetGLPlatformEGL();

ReplayStatus GLES_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL ES replay device");

  // Query the required EGL functions
  if(eglCreateContextProc == NULL)
  {
    eglGetProcAddressProc = (PFN_eglGetProcAddress)dlsym(RTLD_NEXT, "eglGetProcAddress");
    eglChooseConfigProc = (PFN_eglChooseConfig)dlsym(RTLD_NEXT, "eglChooseConfig");
    eglInitializeProc = (PFN_eglInitialize)dlsym(RTLD_NEXT, "eglInitialize");
    eglBindAPIProc = (PFN_eglBindAPI)dlsym(RTLD_NEXT, "eglBindAPI");
    eglGetDisplayProc = (PFN_eglGetDisplay)dlsym(RTLD_NEXT, "eglGetDisplay");
    eglCreateContextProc = (PFN_eglCreateContext)dlsym(RTLD_NEXT, "eglCreateContext");
    eglMakeCurrentProc = (PFN_eglMakeCurrent)dlsym(RTLD_NEXT, "eglMakeCurrent");
    eglSwapBuffersProc = (PFN_eglSwapBuffers)dlsym(RTLD_NEXT, "eglSwapBuffers");
    eglDestroyContextProc = (PFN_eglDestroyContext)dlsym(RTLD_NEXT, "eglDestroyContext");
    eglDestroySurfaceProc = (PFN_eglDestroySurface)dlsym(RTLD_NEXT, "eglDestroySurface");
    eglQuerySurfaceProc = (PFN_eglQuerySurface)dlsym(RTLD_NEXT, "eglQuerySurface");
    eglCreatePbufferSurfaceProc =
        (PFN_eglCreatePbufferSurface)dlsym(RTLD_NEXT, "eglCreatePbufferSurface");
    eglCreateWindowSurfaceProc =
        (PFN_eglCreateWindowSurface)dlsym(RTLD_NEXT, "eglCreateWindowSurface");

    if(eglGetProcAddressProc == NULL || eglBindAPIProc == NULL || eglGetDisplayProc == NULL ||
       eglCreateContextProc == NULL || eglMakeCurrentProc == NULL || eglSwapBuffersProc == NULL ||
       eglDestroyContextProc == NULL || eglDestroySurfaceProc == NULL ||
       eglQuerySurfaceProc == NULL || eglCreatePbufferSurfaceProc == NULL ||
       eglCreateWindowSurfaceProc == NULL || eglChooseConfigProc == NULL)
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

  eglBindAPIProc(EGL_OPENGL_ES_API);

  EGLDisplay eglDisplay = eglGetDisplayProc(EGL_DEFAULT_DISPLAY);
  if(!eglDisplay)
  {
    RDCERR("Couldn't open default EGL display");
    return ReplayStatus::APIInitFailed;
  }

  int major, minor;
  eglInitializeProc(eglDisplay, &major, &minor);

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

  if(!eglChooseConfigProc(eglDisplay, configAttribs, &config, 1, &numConfigs))
  {
    RDCERR("Couldn't find a suitable EGL config");
    return ReplayStatus::APIInitFailed;
  }

  static const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_CONTEXT_FLAGS_KHR,
                                      EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR, EGL_NONE};

  GLReplay::PreContextInitCounters();

  EGLContext ctx = eglCreateContextProc(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
  if(ctx == NULL)
  {
    GLReplay::PostContextShutdownCounters();
    RDCERR("Couldn't create GL ES 3.x context - RenderDoc requires OpenGL ES 3.x availability");
    return ReplayStatus::APIHardwareUnsupported;
  }

  static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
  EGLSurface pbuffer = eglCreatePbufferSurfaceProc(eglDisplay, config, pbAttribs);

  if(pbuffer == NULL)
  {
    RDCERR("Couldn't create a suitable PBuffer");
    eglDestroySurfaceProc(eglDisplay, pbuffer);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  EGLBoolean res = eglMakeCurrentProc(eglDisplay, pbuffer, pbuffer, ctx);
  if(!res)
  {
    RDCERR("Couldn't active the created GL ES context");
    eglDestroySurfaceProc(eglDisplay, pbuffer);
    eglDestroyContextProc(eglDisplay, ctx);
    GLReplay::PostContextShutdownCounters();
    return ReplayStatus::APIInitFailed;
  }

  // TODO: add extesion check just like in the GL case.

  const GLHookSet &real = GetRealGLFunctionsEGL();
  bool extensionsValidated = ValidateFunctionPointers(real);
  if(!extensionsValidated)
  {
    eglDestroySurfaceProc(eglDisplay, pbuffer);
    eglDestroyContextProc(eglDisplay, ctx);
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
