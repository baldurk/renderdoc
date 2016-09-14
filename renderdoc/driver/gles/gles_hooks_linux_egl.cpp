#include "official/egl.h"

#include <cstdio>
#include <dlfcn.h>

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

typedef EGLDisplay (*PFN_eglGetDisplay)(EGLNativeDisplayType display_id);

__attribute__((visibility("default"))) EGLDisplay eglGetDisplay (EGLNativeDisplayType display)
{
    OpenGLHook::glhooks.PopulateHooks();
    PFN_eglGetDisplay real_pfn = (PFN_eglGetDisplay)dlsym(RTLD_NEXT, "eglGetDisplay");
    printf("REAL display: %p\n", real_pfn);


    Keyboard::CloneDisplay(display);

    return real_pfn(display);
}

typedef EGLContext (*eglCreateContext_pfn) (EGLDisplay dpy, EGLConfig config, EGLContext share_context, EGLint const * attrib_list);

__attribute__((visibility("default"))) EGLContext eglCreateContext(EGLDisplay display,
                                                                   EGLConfig config,
                                                                   EGLContext share_context,
                                                                   EGLint const * attrib_list)
{
    OpenGLHook::glhooks.PopulateHooks();

    eglCreateContext_pfn real_pfn = (eglCreateContext_pfn)dlsym(RTLD_NEXT, "eglCreateContext");
    printf("REAL context: %p\n", real_pfn);

    
    EGLContext ctx = real_pfn(display, config, share_context, attrib_list);

    GLESWindowingData outputWin;
    outputWin.ctx = ctx;
    outputWin.eglDisplay = display;
    
    OpenGLHook::glhooks.GetDriver()->CreateContext(outputWin, share_context, GLESInitParams(), true, true);
    return ctx;
}


__attribute__((visibility("default"))) __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *func)
{
    __eglMustCastToProperFunctionPointerType realFunc = OpenGLHook::glhooks.m_eglGetProcAddress_real(func);

    //printf("eglGetProcAddress('%s') -> real: %p\n", func, realFunc);

    if(!strcmp(func, "eglCreateContext"))
        return (__eglMustCastToProperFunctionPointerType)&eglCreateContext;

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

__attribute__((visibility("default"))) EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{

    int width;
    int height;
    OpenGLHook::glhooks.m_eglQuerySurface_real(dpy, surface, EGL_HEIGHT, &height);
    OpenGLHook::glhooks.m_eglQuerySurface_real(dpy, surface, EGL_WIDTH, &width);
    
    OpenGLHook::glhooks.GetDriver()->WindowSize(surface, width, height);
    OpenGLHook::glhooks.GetDriver()->SwapBuffers(surface);
    return OpenGLHook::glhooks.m_eglSwapBuffers_real(dpy, surface);
}


__attribute__((visibility("default"))) EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
    Bool ret = OpenGLHook::glhooks.m_eglMakeCurrent_real(display, draw, read, context);

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
