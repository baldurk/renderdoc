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
#include <stddef.h>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include "common/threading.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"

Threading::CriticalSection libLock;

static std::map<std::string, std::vector<FunctionLoadCallback>> libraryCallbacks;
static std::set<std::string> libraryHooks;
static std::vector<FunctionHook> functionHooks;
static std::set<void *> libraryHandles;

void *interposed_dlopen(const char *filename, int flag)
{
  void *handle = dlopen(filename, flag);

  std::string baseFilename = filename ? get_basename(std::string(filename)) : "";

  {
    SCOPED_LOCK(libLock);

    // we don't redirect to ourselves, but we do remember this handle so we can intercept any
    // subsequent dlsym calls.
    if(libraryHooks.find(baseFilename) != libraryHooks.end())
      libraryHandles.insert(handle);
  }

  return handle;
}

void *interposed_dlsym(void *handle, const char *name)
{
  {
    SCOPED_LOCK(libLock);
    if(libraryHandles.find(handle) != libraryHandles.end())
    {
      for(FunctionHook &hook : functionHooks)
        if(hook.function == name)
          return hook.hook;
    }
  }

  return dlsym(handle, name);
}

struct interposer
{
  const void *replacment;
  const void *replacee;
};

__attribute__((used)) static interposer dlfuncs[] __attribute__((section("__DATA,__interpose"))) = {
    {(const void *)(unsigned long)&interposed_dlsym, (const void *)(unsigned long)&dlsym},
    {(const void *)(unsigned long)&interposed_dlopen, (const void *)(unsigned long)&dlopen},
};

void LibraryHooks::BeginHookRegistration()
{
  // nothing to do
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
  // process libraries with callbacks by loading them if necessary (though we should be linked to
  // them for the dyld interposing)
  for(auto it = libraryCallbacks.begin(); it != libraryCallbacks.end(); ++it)
  {
    std::string libName = it->first;
    void *handle = dlopen(libName.c_str(), RTLD_NOW | RTLD_GLOBAL);

    if(handle)
    {
      for(FunctionLoadCallback cb : it->second)
        if(cb)
          cb(handle);

      // don't call callbacks again if the library is dlopen'd again
      it->second.clear();
    }
  }

  // get the original pointers for all hooks now. All of the ones we will be able to get should now
  // be available in the default namespace straight away
  for(FunctionHook &hook : functionHooks)
  {
    if(hook.orig && *hook.orig == NULL)
    {
      *hook.orig = dlsym(RTLD_NEXT, hook.function.c_str());
      RDCASSERT(*hook.orig != hook.hook, hook.function);
    }
  }
}

void LibraryHooks::Refresh()
{
  // don't need to refresh on mac
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

  // we match by basename for library hooks
  libraryHooks.insert(get_basename(std::string(name)));

  if(cb)
    libraryCallbacks[name].push_back(cb);
}

void LibraryHooks::IgnoreLibrary(const char *libraryName)
{
}

// android only hooking functions, not used on apple
ScopedSuppressHooking::ScopedSuppressHooking()
{
}

ScopedSuppressHooking::~ScopedSuppressHooking()
{
}