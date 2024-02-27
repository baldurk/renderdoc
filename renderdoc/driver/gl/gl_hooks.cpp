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

#include "hooks/hooks.h"
#include "apple_gl_hook_defs.h"
#include "gl_common.h"
#include "gl_dispatch_table.h"
#include "gl_dispatch_table_defs.h"
#include "gl_driver.h"

#if ENABLED(RDOC_POSIX)
#include <dlfcn.h>
#endif

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

class GLHook : LibraryHook
{
public:
  GLHook()
  {
#if ENABLED(RDOC_POSIX)
    // default to RTLD_NEXT for GL lookups if we haven't gotten a more specific library handle
    handle = RTLD_NEXT;
#endif
  }

  void RegisterHooks();

  void UseUnusedSupportedFunction(const char *name);
  void *GetUnsupportedFunction(const char *name);

  void *handle = NULL;
  WrappedOpenGL *driver = NULL;
  bool enabled = false;
} glhook;

#if ENABLED(RDOC_DEVEL)

struct ScopedPrinter
{
  ScopedPrinter(const char *f) : func(f)
  {
    // RDCLOG("Entering %s", func);
    depth++;
    if(depth > 100)
      RDCFATAL("Infinite recursion detected!");
  }
  ~ScopedPrinter()
  {
    // RDCLOG("Exiting %s", func);
    depth--;
  }
  const char *func = NULL;
  static int depth;
};

int ScopedPrinter::depth = 0;

// This checks that we're not infinite looping by calling our own hooks from ourselves. Mostly
// useful on android where you can only debug by printf and the stack dumps are often corrupted when
// the callstack overflows.
#define SCOPED_GLCALL(funcname)           \
  SCOPED_LOCK(glLock);                    \
  gl_CurChunk = GLChunk::funcname;        \
  if(glhook.enabled)                      \
  {                                       \
    glhook.driver->CheckImplicitThread(); \
  }                                       \
  ScopedPrinter CONCAT(scopedprint, __LINE__)(STRINGIZE(funcname));

#else

#define SCOPED_GLCALL(funcname)           \
  SCOPED_LOCK(glLock);                    \
  gl_CurChunk = GLChunk::funcname;        \
  if(glhook.enabled)                      \
  {                                       \
    glhook.driver->CheckImplicitThread(); \
  }

#endif

void SetDriverForHooks(WrappedOpenGL *driver)
{
  glhook.driver = driver;
}

void EnableGLHooks()
{
  glhook.enabled = true;
}

void DisableGLHooks()
{
  glhook.enabled = false;
}

template <typename ret_type>
ret_type default_ret()
{
  return (ret_type)0;
}

template <>
void default_ret()
{
}

template <>
const char *default_ret()
{
  return "";
}

template <>
const GLubyte *default_ret()
{
  return (const GLubyte *)"";
}

// on windows we can be injected and not ready to capture when we intercept a GL call. If that
// happens we need to skip and call the real function

// on linux some systems inject external code into Qt which initialises GL behind our back. If this
// calls glXGetProcAddress it will get the real function pointers, but if it links against GL it
// will get routed here via our public exported symbols so we try to call the real function
#define UNINIT_CALL(function, ...)                                                              \
  if(!glhook.enabled)                                                                           \
  {                                                                                             \
    if(GL.function == NULL)                                                                     \
    {                                                                                           \
      RDCERR("No function pointer for '%s' while doing replay fallback!", STRINGIZE(function)); \
      return default_ret<decltype(GL.function(__VA_ARGS__))>();                                 \
    }                                                                                           \
    return GL.function(__VA_ARGS__);                                                            \
  }

DefineSupportedHooks();
DefineUnsupportedHooks();

// these functions we provide ourselves, so we should return our hook even if there's no onward
// implementation.
bool FullyImplementedFunction(const char *funcname)
{
  return
      // GL_GREMEDY_frame_terminator
      !strcmp(funcname, "glFrameTerminatorGREMEDY") ||
      // GL_GREMEDY_string_marker
      !strcmp(funcname, "glStringMarkerGREMEDY") ||
      // GL_EXT_debug_label
      !strcmp(funcname, "glLabelObjectEXT") || !strcmp(funcname, "glGetObjectLabelEXT") ||
      // GL_EXT_debug_marker
      !strcmp(funcname, "glInsertEventMarkerEXT") || !strcmp(funcname, "glPushGroupMarkerEXT") ||
      !strcmp(funcname, "glPopGroupMarkerEXT") ||
      // GL_KHR_debug (Core variants)
      !strcmp(funcname, "glDebugMessageControl") || !strcmp(funcname, "glDebugMessageInsert") ||
      !strcmp(funcname, "glDebugMessageCallback") || !strcmp(funcname, "glGetDebugMessageLog") ||
      !strcmp(funcname, "glGetPointerv") || !strcmp(funcname, "glPushDebugGroup") ||
      !strcmp(funcname, "glPopDebugGroup") || !strcmp(funcname, "glObjectLabel") ||
      !strcmp(funcname, "glGetObjectLabel") || !strcmp(funcname, "glObjectPtrLabel") ||
      !strcmp(funcname, "glGetObjectPtrLabel") ||
      // GL_KHR_debug (KHR variants)
      !strcmp(funcname, "glDebugMessageControlKHR") ||
      !strcmp(funcname, "glDebugMessageInsertKHR") ||
      !strcmp(funcname, "glDebugMessageCallbackKHR") ||
      !strcmp(funcname, "glGetDebugMessageLogKHR") || !strcmp(funcname, "glGetPointervKHR") ||
      !strcmp(funcname, "glPushDebugGroupKHR") || !strcmp(funcname, "glPopDebugGroupKHR") ||
      !strcmp(funcname, "glObjectLabelKHR") || !strcmp(funcname, "glGetObjectLabelKHR") ||
      !strcmp(funcname, "glObjectPtrLabelKHR") || !strcmp(funcname, "glGetObjectPtrLabelKHR");
}

void *HookedGetProcAddress(const char *func, void *realFunc)
{
#define CheckFunction(function, aliasName)                \
  if(!strcmp(func, STRINGIZE(aliasName)))                 \
  {                                                       \
    if(GL.function == NULL)                               \
      GL.function = (decltype(GL.function))realFunc;      \
    return (void *)&CONCAT(aliasName, _renderdoc_hooked); \
  }

#define CheckUnsupported(function)                                               \
  if(!strcmp(func, STRINGIZE(function)))                                         \
  {                                                                              \
    CONCAT(unsupported_real_, function) = (CONCAT(function, _hooktype))realFunc; \
    return (void *)&CONCAT(function, _renderdoc_hooked);                         \
  }

  ForEachSupported(CheckFunction);
  ForEachUnsupported(CheckUnsupported);

  // for any other function, if it's not a core or extension function we know about,
  // return the real function pointer as this may be something internal
  RDCDEBUG("Returning real pointer for entirely unknown function '%s': %p", func, realFunc);

  return realFunc;
}

void GLHook::UseUnusedSupportedFunction(const char *name)
{
  SCOPED_LOCK(glLock);
  if(glhook.driver)
    glhook.driver->UseUnusedSupportedFunction(name);
}

void *GLHook::GetUnsupportedFunction(const char *name)
{
#if ENABLED(RDOC_APPLE)
  RDCERR("GetUnsupportedFunction called on apple - this should be available at compile time");
#endif

  void *ret = Process::GetFunctionAddress(handle, name);
  if(ret)
    return ret;

  RDCERR("Couldn't find real pointer for %s - will crash", name);

  return NULL;
}

void GLDispatchTable::PopulateWithCallback(PlatformGetProcAddr lookupFunc)
{
#define HookFunc(function, name)                                                    \
  if(GL.function == NULL)                                                           \
  {                                                                                 \
    ScopedSuppressHooking suppress;                                                 \
    GL.function = (decltype(GL.function))lookupFunc((const char *)STRINGIZE(name)); \
  }

  ForEachSupported(HookFunc);
}

static void GLHooked(void *handle)
{
  // store the handle for any unimplemented functions that need to look up their onward
  // pointers
  glhook.handle = handle;
}

void GLHook::RegisterHooks()
{
#if ENABLED(RDOC_ANDROID)
  // on android if EGL hooking is disabled we're using GLES layering, don't register any GL hooks
  if(!ShouldHookEGL())
  {
    RDCLOG("Not registering OpenGL hooks for Android");
    return;
  }
#endif

  RDCLOG("Registering OpenGL hooks");

// pick the 'primary' library we consider GL functions to come from. This is mostly important on
// windows, since hooks are library-specific, but on other platforms it's simply where we expect
// most to come from for fetching. The rest can be collected with the platform's GetProcAddress
#if ENABLED(RDOC_WIN32)
  const char *libraryName = "opengl32.dll";
#elif ENABLED(RDOC_ANDROID)
  const char *libraryName = "libEGL.so";
#elif ENABLED(RDOC_APPLE)
  const char *libraryName = "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib";
#else
  const char *libraryName = "libGL.so.1";
#endif

  LibraryHooks::RegisterLibraryHook(libraryName, &GLHooked);

  // MSVC compiles this function to use a huge amount of stack by initialising all the FunctionHook
  // locals all at once. So we instead explicitly re-use the same hook (since it's going to be
  // copied anyway, these are temporaries).
  FunctionHook tmphook;

#define RegisterFunc(func, name)                              \
  {                                                           \
    tmphook.function = STRINGIZE(name);                       \
    tmphook.orig = (void **)&GL.func;                         \
    tmphook.hook = (void *)&CONCAT(func, _renderdoc_hooked);  \
    LibraryHooks::RegisterFunctionHook(libraryName, tmphook); \
  }

#define RegisterUnsupportedFunc(name)                         \
  {                                                           \
    tmphook.function = STRINGIZE(name);                       \
    tmphook.orig = NULL;                                      \
    tmphook.hook = (void *)&CONCAT(name, _renderdoc_hooked);  \
    LibraryHooks::RegisterFunctionHook(libraryName, tmphook); \
  }

  ForEachSupported(RegisterFunc);
  ForEachUnsupported(RegisterUnsupportedFunc);

#if ENABLED(RDOC_WIN32)
  if(ShouldHookEGL())
  {
    // on windows where hooking is per-library, we also need to register these hooks for any GLES2/3
    // wrapper library, when GLES support is enabled.
    libraryName = "libGLESv2.dll";

    ForEachSupported(RegisterFunc);
  }
#endif

#if ENABLED(RDOC_APPLE)

  // dlsym is unreliable with interposing, we must fetch the functions directly here at compile-time.

#undef APPLE_FUNC
#define APPLE_FUNC(function) CONCAT(unsupported_real_, function) = &function;

  ForEachAppleUnsupported();

#endif
}

#if ENABLED(RDOC_APPLE)

// from dyld-interposing.h - DYLD_INTERPOSE
#undef APPLE_FUNC
#define APPLE_FUNC(function) DECL_HOOK_EXPORT(function)

ForEachAppleSupported();

#endif
