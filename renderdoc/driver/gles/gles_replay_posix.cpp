/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include "gles_replay.h"
#include <dlfcn.h>
#include "gles_driver.h"
#include "gles_resources.h"
#include "official/egl_func_typedefs.h"
#include "gles_hooks_posix.h"

#define REAL(NAME) NAME ##_real
#define DEF_FUNC(NAME) static PFN_##NAME REAL(NAME) = (PFN_##NAME)dlsym(OpenGLHook::GetInstance().GetDLHandle(), #NAME)

DEF_FUNC(eglSwapBuffers);
DEF_FUNC(eglBindAPI);
DEF_FUNC(eglGetDisplay);
DEF_FUNC(eglInitialize);
DEF_FUNC(eglChooseConfig);
DEF_FUNC(eglGetConfigAttrib);
DEF_FUNC(eglCreateContext);
DEF_FUNC(eglCreateWindowSurface);
DEF_FUNC(eglQuerySurface);
DEF_FUNC(eglMakeCurrent);
DEF_FUNC(eglGetError);
DEF_FUNC(eglDestroySurface);
DEF_FUNC(eglDestroyContext);
DEF_FUNC(eglCreatePbufferSurface);
DEF_FUNC(eglGetProcAddress);

#define DEBUG_STRINGIFY(x) #x
#define DEBUG_TOSTRING(x) DEBUG_STRINGIFY(x)
#define DEBUG_LOCATION __FILE__ ":" DEBUG_TOSTRING(__LINE__)

#ifndef DEBUG
#define DEBUG
#endif

#ifdef DEBUG
#define EGL_RETURN_DEBUG(function) printEGLError(DEBUG_STRINGIFY(function), DEBUG_LOCATION)
#else
#define EGL_RETURN_DEBUG(function) while (false)
#endif

#define EGL_CONTEXT_FLAGS_KHR                              0x30FC

#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR               0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR  0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR       0x00000004

const GLHookSet &GetRealGLFunctions();

const char* getEGLErrorString(EGLint errorCode)
{
    switch (errorCode)
    {
        case EGL_SUCCESS                : return "The last function succeeded without error.";
        case EGL_NOT_INITIALIZED        : return "EGL is not initialized, or could not be initialized, for the specified EGL display connection.";
        case EGL_BAD_ACCESS             : return "EGL cannot access a requested resource (for example a context is bound in another thread).";
        case EGL_BAD_ALLOC              : return "EGL failed to allocate resources for the requested operation.";
        case EGL_BAD_ATTRIBUTE          : return "An unrecognized attribute or attribute value was passed in the attribute list.";
        case EGL_BAD_CONTEXT            : return "An EGLContext argument does not name a valid EGL rendering context.";
        case EGL_BAD_CONFIG             : return "An EGLConfig argument does not name a valid EGL frame buffer configuration.";
        case EGL_BAD_CURRENT_SURFACE    : return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.";
        case EGL_BAD_DISPLAY            : return "An EGLDisplay argument does not name a valid EGL display connection.";
        case EGL_BAD_SURFACE            : return "An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.";
        case EGL_BAD_MATCH              : return "Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).";
        case EGL_BAD_PARAMETER          : return "One or more argument values are invalid.";
        case EGL_BAD_NATIVE_PIXMAP      : return "A NativePixmapType argument does not refer to a valid native pixmap.";
        case EGL_BAD_NATIVE_WINDOW      : return "A NativeWindowType argument does not refer to a valid native window.";
        case EGL_CONTEXT_LOST           : return "A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering.";
        default                         : return "Unknown EGL error code!";
    }
    return "";
}

void printEGLError(const char* const function, const char* const location)
{
    EGLint errorCode = REAL(eglGetError)();
    if (errorCode != EGL_SUCCESS)
        RDCLOG("(%s): %s: %s\n", location, function, getEGLErrorString(errorCode));
}

void GLESReplay::MakeCurrentReplayContext(GLESWindowingData *ctx)
{
    static GLESWindowingData *prev = NULL;
    if(REAL(eglMakeCurrent) && ctx && ctx != prev)
    {
        prev = ctx;
        m_pDriver->glFinish();
        REAL(eglMakeCurrent)(ctx->eglDisplay, ctx->surface, ctx->surface, ctx->ctx);
        EGL_RETURN_DEBUG(eglMakeCurrent);
        m_pDriver->ActivateContext(*ctx);
    }
}

void GLESReplay::SwapBuffers(GLESWindowingData *data)
{
    REAL(eglSwapBuffers)(data->eglDisplay, data->surface);
    EGL_RETURN_DEBUG(eglSwapBuffers);
}

void GLESReplay::CloseReplayContext()
{
    REAL(eglDestroyContext)(m_ReplayCtx.eglDisplay, m_ReplayCtx.ctx);
    EGL_RETURN_DEBUG(eglDestroyContext);
}


static bool getEGLDisplayAndConfig(EGLDisplay * const egl_display, EGLConfig * const config, const EGLint * const attribs)
{
    *egl_display = REAL(eglGetDisplay)(EGL_DEFAULT_DISPLAY);
    EGL_RETURN_DEBUG(eglGetDisplay);
    if (!egl_display)
        return false;

    int egl_major;
    int egl_minor;

    if (!REAL(eglInitialize)(*egl_display, &egl_major, &egl_minor)) {
        EGL_RETURN_DEBUG(eglInitialize);
        return false;
    }
    RDCLOG("EGL init (%d, %d)\n", egl_major, egl_minor);

    REAL(eglBindAPI)(EGL_OPENGL_ES_API);
    EGL_RETURN_DEBUG(eglBindAPI);

    EGLint num_configs;
    if (!REAL(eglChooseConfig)(*egl_display, attribs, config, 1, &num_configs)) {
        EGL_RETURN_DEBUG(eglChooseConfig);
        return false;
    }

    EGL_RETURN_DEBUG(eglChooseConfig);
    return true;
}

uint64_t GLESReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
    EGLNativeWindowType wnd = 0;

#ifdef ANDROID
    if(system == eWindowingSystem_Android)
    {
        wnd = (EGLNativeWindowType)data;
    }
    else
#else
    if(system == eWindowingSystem_Xlib)
    {
        XlibWindowData *xlib = (XlibWindowData *)data;
        wnd = (EGLNativeWindowType)xlib->window;
    }
    else if(system == eWindowingSystem_Unknown)
    {
        // TODO(elecro): what?
        if(XOpenDisplay(NULL) == NULL)
          return 0;
    }
    else
#endif
    {
        RDCERR("Unexpected window system %u", system);
    }

    static const EGLint attribs[] = {
        eEGL_RED_SIZE, 8,
        eEGL_GREEN_SIZE, 8,
        eEGL_BLUE_SIZE, 8,
        eEGL_SURFACE_TYPE, eEGL_PBUFFER_BIT | eEGL_WINDOW_BIT,
        eEGL_RENDERABLE_TYPE, eEGL_OPENGL_ES3_BIT,
        eEGL_CONFORMANT, eEGL_OPENGL_ES3_BIT,
        eEGL_COLOR_BUFFER_TYPE, eEGL_RGB_BUFFER,

        EGL_NONE
    };

    EGLDisplay egl_display;
    EGLConfig config;
    if (!getEGLDisplayAndConfig(&egl_display, &config, attribs))
        return -1;

    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
        EGL_NONE
    };

    RDCLOG("display:%p ctx:%p\n", egl_display, m_ReplayCtx.ctx);
    EGLContext ctx = REAL(eglCreateContext)(egl_display, config, m_ReplayCtx.ctx, ctx_attribs);
    EGL_RETURN_DEBUG(eglCreateContext);

    EGLSurface surface = NULL;

    if (wnd != 0) {
        surface = REAL(eglCreateWindowSurface)(egl_display, config, (EGLNativeWindowType)wnd, NULL);
        EGL_RETURN_DEBUG(eglCreateWindowSurface);
    }
    else
    {
        static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
        surface = REAL(eglCreatePbufferSurface)(egl_display, config, pbAttribs);
        EGL_RETURN_DEBUG(eglCreatePbufferSurface);
    }

    if (!surface)
        return -1;

    OutputWindow outputWin;
    outputWin.ctx = ctx;
    outputWin.surface = surface;
    outputWin.eglDisplay = egl_display;

#ifdef ANDROID
    outputWin.wnd = (ANativeWindow*)wnd;
#endif

    REAL(eglQuerySurface)(egl_display, surface, EGL_HEIGHT, &outputWin.height);
    EGL_RETURN_DEBUG(eglQuerySurface);
    REAL(eglQuerySurface)(egl_display, surface, EGL_WIDTH, &outputWin.width);
    EGL_RETURN_DEBUG(eglQuerySurface);

    MakeCurrentReplayContext(&outputWin);

    InitOutputWindow(outputWin);
    CreateOutputWindowBackbuffer(outputWin, depth);

    uint64_t windowId = m_OutputWindowID++;
    m_OutputWindows[windowId] = outputWin;
    RDCLOG("New output window (id:%d) (%dx%d)\n", windowId, outputWin.width, outputWin.height);

    return windowId;
}

void GLESReplay::DestroyOutputWindow(uint64_t id)
{
    auto it = m_OutputWindows.find(id);
    if(id == 0 || it == m_OutputWindows.end())
        return;

    OutputWindow &outw = it->second;

    MakeCurrentReplayContext(&outw);

    WrappedGLES &gl = *m_pDriver;
    gl.glDeleteFramebuffers(1, &outw.BlitData.readFBO);

    REAL(eglMakeCurrent)(outw.eglDisplay, 0, 0, NULL);
    REAL(eglDestroySurface)(outw.eglDisplay, outw.surface);

    m_OutputWindows.erase(it);
}

void GLESReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
    if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
        return;

    OutputWindow &outw = m_OutputWindows[id];

    REAL(eglQuerySurface)(outw.eglDisplay, outw.surface, EGL_HEIGHT, &h);
    REAL(eglQuerySurface)(outw.eglDisplay, outw.surface, EGL_WIDTH, &w);
}

bool GLESReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  GLNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

ReplayCreateStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
    RDCDEBUG("Creating an GLES replay device");

    GLESInitParams initParams;
    RDCDriver driverType = RDC_OpenGLES;
    string driverName = "OpenGLES";
    uint64_t machineIdent = 0;
    if(logfile)
    {
        auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, machineIdent,
                                                   (RDCInitParams *)&initParams);
        if(status != eReplayCreate_Success)
            return status;
    }

    GLESReplay::PreContextInitCounters();

#ifndef ANDROID
    Display * display = XOpenDisplay(NULL);
    if (display == NULL)
        return eReplayCreate_InternalError;
#endif

    static const EGLint attribs[] = {
        eEGL_RED_SIZE, 8,
        eEGL_GREEN_SIZE, 8,
        eEGL_BLUE_SIZE, 8,
        eEGL_RENDERABLE_TYPE, eEGL_OPENGL_ES3_BIT,
        eEGL_CONFORMANT, eEGL_OPENGL_ES3_BIT,
        eEGL_SURFACE_TYPE, eEGL_PBUFFER_BIT | eEGL_WINDOW_BIT,
        eEGL_NONE
    };

    EGLDisplay egl_display;
    EGLConfig config;
    if (!getEGLDisplayAndConfig(&egl_display, &config, attribs))
        return eReplayCreate_InternalError;

    // don't care about pbuffer properties for same reason as backbuffer
    static const EGLint pbAttribs[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
    EGLSurface pbuffer = REAL(eglCreatePbufferSurface)(egl_display, config, pbAttribs);
    EGL_RETURN_DEBUG(eglCreatePbufferSurface);
    if (pbuffer == 0)
        return eReplayCreate_APIInitFailed;

    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
        EGL_NONE
    };

    EGLContext ctx = REAL(eglCreateContext)(egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
    EGL_RETURN_DEBUG(eglCreateContext);
    if (ctx == 0)
        return eReplayCreate_APIInitFailed;


    EGLBoolean res = REAL(eglMakeCurrent)(egl_display, pbuffer, pbuffer, ctx);
    EGL_RETURN_DEBUG(eglMakeCurrent);
    if(!res)
    {
        GLESReplay::PostContextShutdownCounters();
        RDCERR("Couldn't make pbuffer & context current");
        return eReplayCreate_APIInitFailed;
    }

    WrappedGLES *gles = new WrappedGLES(logfile, GetRealGLFunctions());
    gles->Initialise(initParams);
    GLESReplay *replay = gles->GetReplay();

    replay->SetProxy(logfile == NULL);

    GLESWindowingData data;
    data.eglDisplay = egl_display;
    data.ctx = ctx;
    data.surface = pbuffer;
    replay->SetReplayData(data);

    *driver = replay;
    return eReplayCreate_Success;
}
