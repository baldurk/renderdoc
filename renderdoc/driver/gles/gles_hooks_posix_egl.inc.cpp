/******************************************************************************
 * The MIT License (MIT)
 *
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

#include "official/egl.h"
#include "official/egl_func_typedefs.h"

#include <cstdio>
#include <dlfcn.h>

#include "serialise/string_utils.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifndef ANDROID
namespace Keyboard
{
void CloneDisplay(Display *dpy);
}
#endif

typedef void ( *__extFuncPtr)(void);

#undef HookInit
#undef HookExtension
#undef HookExtensionAlias
#undef HandleUnsupported

#define HookInit(function)                                                   \
  if(!strcmp(func, STRINGIZE(function)))                                     \
  {                                                                          \
    OpenGLHook::glhooks.GL.function = (CONCAT(function, _hooktype))realFunc; \
    return (__extFuncPtr)&CONCAT(function, _renderdoc_hooked);            \
  }

#define HookExtension(funcPtrType, function)                      \
  if(!strcmp(func, STRINGIZE(function)))                          \
  {                                                               \
    OpenGLHook::glhooks.GL.function = (funcPtrType)realFunc;      \
    return (__extFuncPtr)&CONCAT(function, _renderdoc_hooked); \
  }

#define HookExtensionAlias(funcPtrType, function, alias)          \
  if(!strcmp(func, STRINGIZE(alias)))                             \
  {                                                               \
    OpenGLHook::glhooks.GL.function = (funcPtrType)realFunc;      \
    return (__extFuncPtr)&CONCAT(function, _renderdoc_hooked); \
  }

#if 0    // debug print for each unsupported function requested (but not used)
#define HandleUnsupported(funcPtrType, function)                                           \
  if(lowername == STRINGIZE(function))                                                     \
  {                                                                                        \
    CONCAT(unsupported_real_, function) = (CONCAT(function, _hooktype))realFunc;           \
    RDCDEBUG("Requesting function pointer for unsupported function " STRINGIZE(function)); \
    return (__extFuncPtr)&CONCAT(function, _renderdoc_hooked);                          \
  }
#else
#define HandleUnsupported(funcPtrType, function)                                 \
  if(lowername == STRINGIZE(function))                                           \
  {                                                                              \
    CONCAT(unsupported_real_, function) = (CONCAT(function, _hooktype))realFunc; \
    return (__extFuncPtr)&CONCAT(function, _renderdoc_hooked);                \
  }
#endif

/*
  in bash:

    function HookWrapper()
    {
        N=$1;
        echo -n "#define HookWrapper$N(ret, function";
            for I in `seq 1 $N`; do echo -n ", t$I, p$I"; done;
        echo ") \\";

        echo -en "\ttypedef ret (*CONCAT(function, _hooktype)) (";
            for I in `seq 1 $N`; do echo -n "t$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); \\";

        echo -e "\textern \"C\" __attribute__ ((visibility (\"default\"))) \\";

        echo -en "\tret function(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -en "\t{ SCOPED_LOCK(glLock); return OpenGLHook::glhooks.GetDriver()->function(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo "); } \\";

        echo -en "\tret CONCAT(function,_renderdoc_hooked)(";
            for I in `seq 1 $N`; do echo -n "t$I p$I"; if [ $I -ne $N ]; then echo -n ", "; fi;
  done;
        echo ") \\";

        echo -en "\t{ SCOPED_LOCK(glLock); return OpenGLHook::glhooks.GetDriver()->function(";
            for I in `seq 1 $N`; do echo -n "p$I"; if [ $I -ne $N ]; then echo -n ", "; fi; done;
        echo -n "); }";
    }

  for I in `seq 0 15`; do HookWrapper $I; echo; done

  */

// Enable to dump egl trace messages
//#define DUMP_EGL_ENTER

#define DEFAULT_VISIBILITY __attribute__((visibility("default")))

#define REAL(name) name ## _real
#define DEF_FUNC(name) static PFN_##name REAL(name) = (PFN_ ## name)dlsym(libGLdlsymHandle, #name)


DEFAULT_VISIBILITY
EGLDisplay eglGetDisplay (EGLNativeDisplayType display)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif

    OpenGLHook::glhooks.PopulateHooks();
    DEF_FUNC(eglGetDisplay);
#ifndef ANDROID
    Keyboard::CloneDisplay(display);
#endif

    return REAL(eglGetDisplay)(display);
}

DEFAULT_VISIBILITY
EGLContext eglCreateContext(EGLDisplay display, EGLConfig config, EGLContext share_context, EGLint const * attrib_list)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: eglCreateContext");
#endif
    OpenGLHook::glhooks.PopulateHooks();

    GLESInitParams init;

    init.width = 0;
    init.height = 0;

    EGLint value;
    eglGetConfigAttrib(display, config, eEGL_BUFFER_SIZE, &value);
    init.colorBits = value;
    eglGetConfigAttrib(display, config, eEGL_DEPTH_SIZE, &value);
    init.depthBits = value;
    eglGetConfigAttrib(display, config, eEGL_STENCIL_SIZE, &value);
    init.stencilBits = value;
    init.isSRGB = 1; // TODO: How can we get it from the EGL?

    DEF_FUNC(eglCreateContext);
    EGLContext ctx = REAL(eglCreateContext)(display, config, share_context, attrib_list);

    GLESWindowingData outputWin;
    outputWin.ctx = ctx;
    outputWin.eglDisplay = display;

    OpenGLHook::glhooks.GetDriver()->CreateContext(outputWin, share_context, init, true, true);
    return ctx;
}

DEFAULT_VISIBILITY
EGLContext eglGetCurrentContext()
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglGetCurrentContext);
    return REAL(eglGetCurrentContext)();
}


DEFAULT_VISIBILITY
__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *func)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    if (OpenGLHook::glhooks.m_eglGetProcAddress_real == NULL)
      OpenGLHook::glhooks.PopulateHooks();

    __eglMustCastToProperFunctionPointerType realFunc = OpenGLHook::glhooks.m_eglGetProcAddress_real(func);

    // Return our own egl implementations if requested
#define WRAP(name) \
    if (!strcmp(func, #name)) { return (__eglMustCastToProperFunctionPointerType)&name; }

    WRAP(eglCreateContext)
    WRAP(eglGetDisplay)
    WRAP(eglBindAPI)
    WRAP(eglGetConfigAttrib)
    WRAP(eglSwapInterval)
    WRAP(eglInitialize)
    WRAP(eglChooseConfig)
    WRAP(eglCreateWindowSurface)
    WRAP(eglDestroySurface)
    WRAP(eglDestroyContext)
    WRAP(eglTerminate)

#undef WRAP

    // if the real RC doesn't support this function, don't bother hooking
    if(realFunc == NULL)
       return realFunc;

    DLLExportHooks();
    HookCheckGLExtensions();

    // at the moment the unsupported functions are all lowercase (as their name is generated from the
    // typedef name).
    string lowername = strlower(string(func));

    CheckUnsupported();

    return realFunc;
}

DEFAULT_VISIBILITY
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif

    int width;
    int height;
    OpenGLHook::glhooks.m_eglQuerySurface_real(dpy, surface, EGL_HEIGHT, &height);
    OpenGLHook::glhooks.m_eglQuerySurface_real(dpy, surface, EGL_WIDTH, &width);

    OpenGLHook::glhooks.GetDriver()->WindowSize(surface, width, height);
    OpenGLHook::glhooks.GetDriver()->SwapBuffers(surface);
    return OpenGLHook::glhooks.m_eglSwapBuffers_real(dpy, surface);
}


DEFAULT_VISIBILITY
EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    EGLBoolean ret = OpenGLHook::glhooks.m_eglMakeCurrent_real(display, draw, read, context);

    if(context && OpenGLHook::glhooks.m_Contexts.find(context) == OpenGLHook::glhooks.m_Contexts.end())
    {
        OpenGLHook::glhooks.m_Contexts.insert(context);
        OpenGLHook::glhooks.PopulateHooks();
    }

    GLESWindowingData data;
    data.eglDisplay = display;
    data.surface = draw;
    data.ctx = context;

    OpenGLHook::glhooks.GetDriver()->ActivateContext(data);

    return ret;
}

DEFAULT_VISIBILITY
EGLBoolean eglBindAPI(EGLenum api)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglBindAPI);
    return REAL(eglBindAPI)(api);
}

DEFAULT_VISIBILITY
EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglGetConfigAttrib);
    return REAL(eglGetConfigAttrib)(dpy, config, attribute, value);
}

DEFAULT_VISIBILITY
EGLSurface eglGetCurrentSurface(EGLint readdraw)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglGetCurrentSurface);
    return REAL(eglGetCurrentSurface)(readdraw);
}

DEFAULT_VISIBILITY
EGLDisplay eglGetCurrentDisplay()
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglGetCurrentDisplay);
    return REAL(eglGetCurrentDisplay)();
}

DEFAULT_VISIBILITY
EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglSwapInterval);
    return REAL(eglSwapInterval)(dpy, interval);
}

DEFAULT_VISIBILITY
EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: eglInitialize");
#endif

    DEF_FUNC(eglInitialize);
    return REAL(eglInitialize)(dpy, major, minor);
}

DEFAULT_VISIBILITY
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglChooseConfig);
    return REAL(eglChooseConfig)(dpy, attrib_list, configs, config_size, num_config);
}

DEFAULT_VISIBILITY
EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglCreateWindowSurface);
    return REAL(eglCreateWindowSurface)(dpy, config, win, attrib_list);
}

DEFAULT_VISIBILITY
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglDestroySurface);
    return REAL(eglDestroySurface)(dpy, surface);
}

DEFAULT_VISIBILITY
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglDestroyContext);
    return REAL(eglDestroyContext)(dpy, ctx);
}


DEFAULT_VISIBILITY
EGLBoolean eglTerminate(EGLDisplay dpy)
{
#ifdef DUMP_EGL_ENTER
    RDCLOG("Enter: %s", __FUNCTION__);
#endif
    DEF_FUNC(eglTerminate);
    return REAL(eglTerminate)(dpy);
}


#undef REAL
#undef DEF_FUNC
#undef DEFAULT_VISIBILITY
