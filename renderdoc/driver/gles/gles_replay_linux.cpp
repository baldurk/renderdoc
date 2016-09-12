/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#define RENDERDOC_WINDOWING_XLIB 1

#include "gles_replay.h"
#include <dlfcn.h>
#include "gles_driver.h"
#include "gles_resources.h"
#include "official/egl_func_typedefs.h"

#define REAL(NAME) NAME ##_real
#define LOAD_SYM(NAME) REAL(NAME) = (PFN_##NAME)dlsym(RTLD_NEXT, #NAME)
#define DEF_FUNC(NAME) static PFN_##NAME REAL(NAME) = NULL

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


static PFN_eglGetProcAddress eglGetProcAddress_real = NULL;
const GLHookSet &GetRealGLFunctions();



void GLESReplay::SwapBuffers(GLESWindowingData* data)
{
    printf("CALL: %s\n", __FUNCTION__);
    REAL(eglSwapBuffers)(data->eglDisplay, data->surface);
    printf("ERR: %d\n", m_pDriver->m_Real.glGetError());
}

ReplayCreateStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
    RDCDEBUG("Creating an GLES replay device");

    eglGetProcAddress_real = (PFN_eglGetProcAddress)dlsym(RTLD_NEXT, "eglGetProcAddress");
    LOAD_SYM(eglSwapBuffers);

    GLESInitParams initParams;
    RDCDriver driverType = RDC_OpenGL;
    string driverName = "OpenGL";
    uint64_t machineIdent = 0;
    if(logfile)
    {
        auto status = RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, machineIdent,
                                                   (RDCInitParams *)&initParams);
        if(status != eReplayCreate_Success)
            return status;
    }

    WrappedGLES* gles = new WrappedGLES(logfile, GetRealGLFunctions());
    gles->Initialise(initParams);
    *driver = gles->GetReplay();
    return eReplayCreate_Success;
}

void GLESReplay::MakeCurrentReplayContext(GLESWindowingData *ctx)
{
}

void GLESReplay::CloseReplayContext()
{
}

uint64_t GLESReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
    Display *display = NULL;
    Drawable draw = 0;

    if(system == eWindowingSystem_Xlib)
    {
        XlibWindowData *xlib = (XlibWindowData *)data;

        display = xlib->display;
        draw = xlib->window;
    }
    else
    {
        RDCERR("Unexpected window system %u", system);
    }

    LOAD_SYM(eglBindAPI);
    LOAD_SYM(eglGetDisplay);
    LOAD_SYM(eglInitialize);
    LOAD_SYM(eglChooseConfig);
    LOAD_SYM(eglGetConfigAttrib);
    LOAD_SYM(eglCreateContext);
    LOAD_SYM(eglCreateWindowSurface);
    LOAD_SYM(eglQuerySurface);
    LOAD_SYM(eglMakeCurrent);
    LOAD_SYM(eglGetError);

    EGLDisplay egl_display = REAL(eglGetDisplay)(display);
    if (!egl_display) {
        printf("Error: eglGetDisplay() failed\n");
        return -1;
    }
    printf("DISP EGL err: 0x%x\n", REAL(eglGetError)());

    int egl_major;
    int egl_minor;

    if (!REAL(eglInitialize)(egl_display, &egl_major, &egl_minor)) {
        printf("Error: eglInitialize() failed\n");
         return -1;
    }
    printf("EGL init (%d, %d)\n", egl_major, egl_minor);
    printf("Init EGL err: 0%x\n", REAL(eglGetError)());


    REAL(eglBindAPI)(EGL_OPENGL_ES_API);

    static const EGLint attribs[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_DEPTH_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint num_configs;

    if (!REAL(eglChooseConfig)(egl_display, attribs, &config, 1, &num_configs)) {
        printf("Error: couldn't get an EGL visual config\n");
        return -1;
    }
    printf("Choose config EGL err: 0x%x\n", REAL(eglGetError)());

    EGLint vid;
    if (!REAL(eglGetConfigAttrib)(egl_display, config, EGL_NATIVE_VISUAL_ID, &vid)) {
        printf("Error: eglGetConfigAttrib() failed\n");
        return -1;
    }
    printf("Get attrib EGL err: 0x%x\n", REAL(eglGetError)());


    static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLContext ctx = REAL(eglCreateContext)(egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
    printf("Context EGL err: 0x%x\n", REAL(eglGetError)());

    EGLSurface surface = REAL(eglCreateWindowSurface)(egl_display, config, (EGLNativeWindowType)draw, NULL);
    printf("Surface EGL err: 0x%x\n", REAL(eglGetError)());


    OutputWindow outputWin;
    outputWin.ctx = ctx;
    outputWin.surface = surface;
    outputWin.eglDisplay = egl_display;
    outputWin.display = display;

    REAL(eglQuerySurface)(egl_display, surface, EGL_HEIGHT, &outputWin.height);
    printf("Query H EGL err: 0x%x\n", REAL(eglGetError)());
    REAL(eglQuerySurface)(egl_display, surface, EGL_WIDTH, &outputWin.width);
    printf("Query W EGL err: 0x%x\n", REAL(eglGetError)());
    printf("New output window (%dx%d)\n", outputWin.height, outputWin.width);

    MakeCurrentReplayContext(&outputWin);

    uint64_t windowId = m_OutputWindowIds++;
    m_OutputWindows[windowId] = outputWin;

    return windowId;
}

void GLESReplay::DestroyOutputWindow(uint64_t id)
{
 
}

void GLESReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
}

bool GLESReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  GLNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

