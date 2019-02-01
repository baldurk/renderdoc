/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#pragma once

#include "os/os_specific.h"

typedef std::function<void(void *)> FunctionLoadCallback;

struct FunctionHook
{
  FunctionHook() : orig(NULL), hook(NULL) {}
  FunctionHook(const char *f, void **o, void *d) : function(f), orig(o), hook(d) {}
  bool operator<(const FunctionHook &h) const { return function < h.function; }
  std::string function;
  void **orig;
  void *hook;
};

// == Hooking workflow overview ==
//
// Each subsystem that wants to hook libraries creates a LibraryHook instance. That registers with
// LibraryHooks via the singleton in global constructors, but does nothing initially.
//
// Early in init, during RenderDoc's initialisation, the last thing that happens is a call to
// LibraryHooks::RegisterHooks(). This iterates through the LibraryHook instances and calls
// RegisterHooks on each of them.
//
// Each subsystem should call LibraryHooks::RegisterLibraryHook() for the filename of each library
// it wants to hook, then LibraryHooks::RegisterFunctionHook() for each function it wants to hook
// within that library. Note that not all platforms will use all the information provided, but only
// in a way that's invisible to the user. These are lightweight calls to register the hooks, and
// don't do any work yet.
//
// A registered library hook can get an optional callback when that library is first loaded.
//
// Similarly a registered function hook can provide a function pointer location which will be filled
// out with the function pointer to call onwards. This may just be the real implementation, or a
// trampoline, depending on the platform hooking method.
//
// Once all of this is completed, these hooks will be applied and activated as necessary. If any
// libraries are already loaded, library callbacks will fire here. Similarly function hook original
// pointers will be filled out if they are already available. Library callbacks will fire precisely
// once, the first time the library is loaded.
//
// NOTE: An important result of the behaviour above is that original function pointers are not
// necessarily available until the function is actually hooked. The hooking will automatically
// propagate all functions hooked in a library once it's loaded, and since some platforms may
// trampoline-hook target functions it is *not* safe to just get the target function's pointer as
// this may then call back into hooks. The only exception to this is with a library-specific
// function for fetching pointers such as GetProcAddress on OpenGL/Vulkan. However in this case care
// must be taken to suppress any effect of function interception by calling the relevant suppression
// functions. This ensures that if the API-specific GetProcAddress calls into the platform function
// fetch, it doesn't form an infinite loop.
//
// Also in the case that a function may come from many libraries, or it has multiple aliased names,
// the order of registration is important. The hooking implementations will always follow the order
// provided as a priority list, so if you register libFoo before libBar, any functions in libFoo
// have precedence. Similarly if you register foofunc() before barfunc() but pointed to the same
// pointer location for storing the original function pointer, barfunc()'s pointer will not
// overwrite foofunc() if it exists.
//
// == Hooking details (platform-specific) ==
//
// The method of hooking varies by platform but follows the same general pattern. During
// registration we build up lists of which libraries and functions are to be hooked. Once the
// registration is complete we apply all of the hooks.
//
// NOTE: The library name for function hooks is only used on windows, since on linux/android
// namespace resolution is a bit fuzzier. At the time of writing there are no cases where two
// libraries have the same function that aren't functionally equivalent.
//
// - Windows -
// On windows this involves iterating over all loaded libraries and hooking their IAT tables. We
// also hook internally any functions for dynamically loading libraries or fetching functions, so
// that we can continue to re-wrap any newly loaded libraries.
//
// Loaded libraries at startup (most likely because the exe linked against them) have their
// callbacks fired immediately. Otherwise any time a new library is found on a subsequent iteration
// from a LoadLibrary-type call we fire the callback.
//
// Whenever a library is newly found, we hook all the entry points and update any original function
// pointers that are set to NULL.
//
// - Linux -
// On linux we rely on exporting all hooked symbols, and using LD_PRELOAD to load our library first
// in resolution order. All we do is hook dlopen so that when a new library is loaded we can
// redirect the returned library handle to ourselves, as well as process any pending function hooks.
//
// - Android -
// On android the implementation varies depending on whether we're using interceptor-lib or not.
// This is optional at build time but produces more reliable results:
//
// Without interceptor-lib:
// The implementation is similar to windows. Since we don't have LD_PRELOAD we need to patch import
// tables of any loaded libraries. We also cannot reliably hook dlopen on android to redirect the
// library handle to ourselves, so instead we hook dlsym and check to see if it corresponds to a
// function we're intercepting. This is the need for the 'suppress hooking' function, since if
// eglGetProcAddress calls into dlsym we don't want to intercept that dlsym and return our own
// function.
//
// With interceptor-lib:
// Instead of patching imports we overwrite the assembly at each entry point in the *target*
// library, and patch it to call into our hooks. Then we create trampolines to restore the original
// function for onwards-calling.
//
// To ensure sanity, we always load all registered libraries in the first hook applying phase. This
// ensures that we don't end up in a weird situation where one library is loaded, not all functions
// are hooked, the user code tries to populate any missing functions and then later another library
// is loaded and we patch it after having requested function pointers. Ensuring all libraries that
// we *might* patch are loaded ASAP, everything is consistent since after that any functions that
// don't have trampolines provided will never be trampolined in the future.
//
// Sometimes interceptor-lib can fail, at which point we fall back to the path above for those
// functions and try to follow and patch any imports to them.

struct LibraryHook;

// this singleton allows you to compile in code that defines a hook for a given library
// (and it will be registered). Then when the renderdoc library is initialised in the target
// program RegisterHooks() will be called to set up the hooks.
class LibraryHooks
{
public:
  // generic, implemented in hooks.cpp to iterate over all registered libraries
  static void RegisterHooks();
  static void OptionsUpdated();

  // platform specific implementations

  // Removes hooks (where possible) and restores everything to an un-hooked state
  static void RemoveHooks();

  // refreshes hooks, useful on android where hooking can be unreliable
  static void Refresh();

  // Ignore this library - i.e. do not hook any calls it makes. Useful in the case where a library
  // might call in to hooked APIs but we want to treat it as a black box.
  static void IgnoreLibrary(const char *libraryName);

  // register a library for hooking, providing an optional callback to be called the first time the
  // library has been loaded and all functions in it hooked.
  static void RegisterLibraryHook(const char *libraryName, FunctionLoadCallback loadedCallback);

  // registers a function to be hooked, and an optional location of where to store the original
  // onward function pointer
  static void RegisterFunctionHook(const char *libraryName, const FunctionHook &hook);

  // detect if an identifier is present in the current process - used as a marker to indicate
  // replay-type programs.
  static bool Detect(const char *identifier);

private:
  static void BeginHookRegistration();
  static void EndHookRegistration();
};

// defines the interface that a library hooking class will implement.
struct LibraryHook
{
  LibraryHook();
  virtual void RegisterHooks() = 0;
  virtual void OptionsUpdated() {}
private:
  friend class LibraryHooks;

  static std::vector<LibraryHook *> m_Libraries;
};

template <typename FuncType>
class HookedFunction
{
public:
  HookedFunction() { orig_funcptr = NULL; }
  ~HookedFunction() {}
  FuncType operator()() { return (FuncType)orig_funcptr; }
  void SetFuncPtr(void *ptr) { orig_funcptr = ptr; }
  void Register(const char *module_name, const char *function, void *destination_function_ptr)
  {
    LibraryHooks::RegisterFunctionHook(
        module_name, FunctionHook(function, &orig_funcptr, destination_function_ptr));
  }

private:
  void *orig_funcptr;
};

struct ScopedSuppressHooking
{
  ScopedSuppressHooking();
  ~ScopedSuppressHooking();
};
