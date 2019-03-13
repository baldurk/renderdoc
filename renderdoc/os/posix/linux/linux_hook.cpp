/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include <dlfcn.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include "3rdparty/plthook/plthook.h"
#include "common/threading.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

Threading::CriticalSection libLock;

static std::map<std::string, std::vector<FunctionLoadCallback>> libraryCallbacks;
static std::vector<std::string> libraryHooks;
static std::vector<FunctionHook> functionHooks;

void *intercept_dlopen(const char *filename, int flag, void *ret);
void plthook_lib(void *handle);

typedef void *(*DLOPENPROC)(const char *, int);
DLOPENPROC realdlopen = NULL;

static volatile int32_t tlsbusyflag = 0;

__attribute__((visibility("default"))) void *dlopen(const char *filename, int flag)
{
  if(!realdlopen)
  {
    DLOPENPROC passthru = (DLOPENPROC)dlsym(RTLD_NEXT, "dlopen");

    void *ret = passthru(filename, flag);

    if(filename && ret && (flag & RTLD_DEEPBIND))
      plthook_lib(ret);

    return ret;
  }

  // don't do any hook processing inside here even if we call dlopen again
  Atomic::Inc32(&tlsbusyflag);
  void *ret = realdlopen(filename, flag);
  Atomic::Dec32(&tlsbusyflag);

  if(filename && ret)
  {
    SCOPED_LOCK(libLock);
    ret = intercept_dlopen(filename, flag, ret);
  }

  return ret;
}

void plthook_lib(void *handle)
{
  plthook_t *plthook = NULL;

  // minimal error handling as we can't do much more than log the error, and since this is
  // 'best-effort' attempt to hook the unhookable, we just try and allow it to fail.
  if(plthook_open_by_handle(&plthook, handle))
    return;

  plthook_replace(plthook, "dlopen", (void *)dlopen, NULL);

  for(FunctionHook &hook : functionHooks)
  {
    void *orig = NULL;
    plthook_replace(plthook, hook.function.c_str(), hook.hook, &orig);
    if(hook.orig && *hook.orig == NULL && orig)
      *hook.orig = orig;
  }

  plthook_close(plthook);
}

static void CheckLoadedLibraries()
{
  // don't process anything if the busy flag was set, otherwise set it ourselves
  if(Atomic::CmpExch32(&tlsbusyflag, 0, 1) != 0)
    return;

  // iterate over the libraries and see which ones are already loaded, process function hooks for
  // them and call callbacks.
  for(auto it = libraryHooks.begin(); it != libraryHooks.end(); ++it)
  {
    std::string libName = *it;
    void *handle = realdlopen(libName.c_str(), RTLD_NOW | RTLD_NOLOAD | RTLD_GLOBAL);

    if(handle)
    {
      for(FunctionHook &hook : functionHooks)
      {
        if(hook.orig && *hook.orig == NULL)
          *hook.orig = dlsym(handle, hook.function.c_str());
      }

      std::vector<FunctionLoadCallback> callbacks;

      // don't call callbacks again if the library is dlopen'd again
      libraryCallbacks[libName].swap(callbacks);

      for(FunctionLoadCallback cb : callbacks)
        if(cb)
          cb(handle);
    }
  }

  // clear any dl errors in case twitchy applications get set off by false positive errors.
  dlerror();

  // decrement the flag counter
  Atomic::Dec32(&tlsbusyflag);
}

void *intercept_dlopen(const char *filename, int flag, void *ret)
{
  if(filename == NULL)
    return ret;

  if(flag & RTLD_DEEPBIND)
    plthook_lib(ret);

  std::string base = get_basename(filename);

  for(auto it = libraryHooks.begin(); it != libraryHooks.end(); ++it)
  {
    const std::string &libName = *it;
    if(*it == base)
    {
      RDCDEBUG("Redirecting dlopen to ourselves for %s", filename);

      for(FunctionHook &hook : functionHooks)
      {
        if(hook.orig && *hook.orig == NULL)
          *hook.orig = dlsym(ret, hook.function.c_str());
      }

      std::vector<FunctionLoadCallback> callbacks;

      // don't call callbacks again if the library is dlopen'd again
      libraryCallbacks[libName].swap(callbacks);

      for(FunctionLoadCallback cb : callbacks)
        if(cb)
          cb(ret);

      ret = realdlopen("librenderdoc.so", flag);
      break;
    }
  }

  // this library might depend on one we care about, so check again as we
  // did in EndHookRegistration to see if any library has been loaded.
  CheckLoadedLibraries();

  return ret;
}

void LibraryHooks::BeginHookRegistration()
{
  realdlopen = (DLOPENPROC)dlsym(RTLD_NEXT, "dlopen");
}

bool LibraryHooks::Detect(const char *identifier)
{
  return dlsym(RTLD_DEFAULT, identifier) != NULL;
}

void LibraryHooks::RemoveHooks()
{
  RDCERR("Removing hooks is not possible on this platform");
}

void LibraryHooks::EndHookRegistration()
{
  CheckLoadedLibraries();
}

void LibraryHooks::Refresh()
{
  // don't need to refresh on linux
}

void LibraryHooks::RegisterFunctionHook(const char *libraryName, const FunctionHook &hook)
{
  // we don't use the library name
  (void)libraryName;

  SCOPED_LOCK(libLock);
  functionHooks.push_back(hook);
}

void LibraryHooks::RegisterLibraryHook(char const *name, FunctionLoadCallback cb)
{
  SCOPED_LOCK(libLock);

  if(std::find(libraryHooks.begin(), libraryHooks.end(), name) == libraryHooks.end())
    libraryHooks.push_back(name);

  if(cb)
    libraryCallbacks[name].push_back(cb);
}

void LibraryHooks::IgnoreLibrary(const char *libraryName)
{
}

// android only hooking functions, not used on linux
ScopedSuppressHooking::ScopedSuppressHooking()
{
}

ScopedSuppressHooking::~ScopedSuppressHooking()
{
}