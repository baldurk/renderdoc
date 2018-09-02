/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

#include "driver/gl/gl_common.h"
#include "driver/gl/gl_dispatch_table.h"
#include "driver/gl/gl_dispatch_table_defs.h"
#include "driver/gl/gl_driver.h"
#include "hooks/hooks.h"

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
#define SCOPED_GLCALL(funcname)    \
  SCOPED_LOCK(glLock);             \
  gl_CurChunk = GLChunk::funcname; \
  ScopedPrinter CONCAT(scopedprint, __LINE__)(STRINGIZE(funcname));

#else

#define SCOPED_GLCALL(funcname) \
  SCOPED_LOCK(glLock);          \
  gl_CurChunk = GLChunk::funcname;

#endif

void SetDriverForHooks(WrappedOpenGL *driver)
{
  glhook.driver = driver;
}

#if ENABLED(RDOC_WIN32)
void EnableHooks()
{
  glhook.enabled = true;
}

// if we were injected and aren't ready to capture, skip out and call the real function
#define UNINIT_CALL(function, ...)                                                      \
  if(!glhook.enabled)                                                                   \
  {                                                                                     \
    if(GL.function == NULL)                                                             \
      RDCERR("No function pointer for '%s' while uninitialised!", STRINGIZE(function)); \
    return GL.function(__VA_ARGS__);                                                    \
  }

#else

// nothing to do - we always assume we are ready to capture
#define UNINIT_CALL(function, ...)

#endif

DefineSupportedHooks();
DefineUnsupportedHooks();

void *HookedGetProcAddress(const char *func, void *realFunc)
{
#define CheckFunction(function, name)                    \
  if(!strcmp(func, STRINGIZE(name)))                     \
  {                                                      \
    if(GL.function == NULL)                              \
      GL.function = (decltype(GL.function))realFunc;     \
    return (void *)&CONCAT(function, _renderdoc_hooked); \
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

void *GLHook::GetUnsupportedFunction(const char *name)
{
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
  RDCLOG("Registering OpenGL hooks");

// pick the 'primary' library we consider GL functions to come from. This is mostly important on
// windows, since hooks are library-specific, but on other platforms it's simply where we expect
// most to come from for fetching. The rest can be collected with the platform's GetProcAddress
#if ENABLED(RDOC_WIN32)
  const char *libraryName = "opengl32.dll";
#elif ENABLED(RDOC_ANDROID)
  const char *libraryName = "libEGL.so";
#elif ENABLED(RDOC_APPLE)
  const char *libraryName = "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL";
#else
  const char *libraryName = "libGL.so.1";
#endif

  LibraryHooks::RegisterLibraryHook(libraryName, &GLHooked);

#define RegisterFunc(func, name)      \
  LibraryHooks::RegisterFunctionHook( \
      libraryName,                    \
      FunctionHook(STRINGIZE(name), (void **)&GL.func, (void *)&CONCAT(func, _renderdoc_hooked)));

  ForEachSupported(RegisterFunc);

#if ENABLED(RDOC_WIN32)
  if(ShouldHookEGL())
  {
    // on windows where hooking is per-library, we also need to register these hooks for any GLES2/3
    // wrapper library, when GLES support is enabled.
    libraryName = "libGLESv2.dll";

    ForEachSupported(RegisterFunc);
  }
#endif
}

#if ENABLED(RDOC_APPLE)
#include "apple_gl_hook_defs.h"
#endif