/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <map>
#include "common/threading.h"
#include "core/core.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"
#include "plthook/plthook.h"
#include "strings/string_utils.h"

Threading::CriticalSection libLock;

static std::map<rdcstr, rdcarray<FunctionLoadCallback>> libraryCallbacks;
static rdcarray<rdcstr> libraryHooks;
static rdcarray<FunctionHook> functionHooks;

void *intercept_dlopen(const char *filename, int flag, void *ret);
void plthook_lib(void *handle);

typedef pid_t (*FORKPROC)();
typedef void *(*DLOPENPROC)(const char *, int);
typedef void *(*DLSYMPROC)(void *, const char *);
DLOPENPROC realdlopen = NULL;
FORKPROC realfork = NULL;
DLSYMPROC realdlsym = NULL;

static int32_t tlsbusyflag = 0;

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

int GetIdentPort(pid_t childPid);

void StopAtMainInChild();
bool StopChildAtMain(pid_t childPid);
void ResumeProcess(pid_t childPid, uint32_t delay = 0);

__attribute__((visibility("default"))) pid_t fork()
{
  if(!realfork)
  {
    FORKPROC passthru = (FORKPROC)dlsym(RTLD_NEXT, "fork");

    return passthru();
  }

  // fork in a captured application. Need to get the child ident and register it

  pid_t ret = realfork();

  if(ret == 0)
  {
    StopAtMainInChild();
  }
  else if(ret > 0)
  {
    bool stopped = StopChildAtMain(ret);

    if(stopped)
    {
      int ident = GetIdentPort(ret);

      ResumeProcess(ret);

      if(ident)
      {
        RDCLOG("Identified child process %u with ident %u", ret, ident);
        RenderDoc::Inst().AddChildProcess((uint32_t)ret, (uint32_t)ident);
      }
      else
      {
        RDCERR("Couldn't get ident for PID %u after stopping at main", ret);
      }
    }
    else
    {
      // resume the process just in case something went wrong. This should be harmless if we're not
      // actually tracing
      ResumeProcess(ret);

      // ptrace_scope isn't amenable, or we hit an error. We'll have to spin up a thread to check
      // the ident on the child process and add it as soon as it's available
      Threading::ThreadHandle handle = Threading::CreateThread([ret]() {
        RDCLOG("Starting thread to get ident for PID %u", ret);

        // don't accept a return value of our own ident, that means we've checked too early and exec
        // hasn't run yet
        const uint32_t ownIdent = RenderDoc::Inst().GetTargetControlIdent();
        uint32_t ident = ownIdent;
        for(uint32_t i = 0; i < 10 && ident == ownIdent; i++)
        {
          ident = (uint32_t)GetIdentPort(ret);
          if(ident == ownIdent)
            usleep(1000);
        }

        RDCLOG("PID %u has ident %u", ret, ident);

        RenderDoc::Inst().AddChildProcess((uint32_t)ret, (uint32_t)ident);
        RenderDoc::Inst().CompleteChildThread((uint32_t)ret);
      });
      RenderDoc::Inst().AddChildThread((uint32_t)ret, handle);
    }
  }

  return ret;
}

#if defined(RENDERDOC_HOOK_DLSYM)

#pragma message("ALERT: dlsym() hooking enabled! This is unreliable & relies on glibc internals.")

extern "C" {

__attribute__((visibility("default"))) void *dlsym(void *handle, const char *name);

extern void *_dl_sym(void *, const char *, void *);

void bootstrap_dlsym()
{
  realdlsym = (DLSYMPROC)_dl_sym(RTLD_NEXT, "dlsym", (void *)&dlsym);
}

__attribute__((visibility("default"))) void *dlsym(void *handle, const char *name)
{
  if(!strcmp(name, "dlsym"))
    return (void *)&dlsym;

  if(!strcmp(name, "dlopen") && realdlopen)
    return (void *)&dlopen;

  if(realdlsym == NULL)
    bootstrap_dlsym();

  if(realdlsym == NULL)
  {
    fprintf(stderr, "Couldn't get onwards dlsym in hooked dlsym\n");
    exit(-1);
  }

  return realdlsym(handle, name);
}

};    // extern "C"

#endif

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
    rdcstr libName = *it;
    void *handle = realdlopen(libName.c_str(), RTLD_NOW | RTLD_NOLOAD | RTLD_GLOBAL);

    if(handle)
    {
      for(FunctionHook &hook : functionHooks)
      {
        if(hook.orig && *hook.orig == NULL)
          *hook.orig = dlsym(handle, hook.function.c_str());
      }

      rdcarray<FunctionLoadCallback> callbacks;

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

  rdcstr base = get_basename(filename);

  for(auto it = libraryHooks.begin(); it != libraryHooks.end(); ++it)
  {
    const rdcstr &libName = *it;
    if(*it == base)
    {
      RDCDEBUG("Redirecting dlopen to ourselves for %s", filename);

      for(FunctionHook &hook : functionHooks)
      {
        if(hook.orig && *hook.orig == NULL)
          *hook.orig = dlsym(ret, hook.function.c_str());
      }

      rdcarray<FunctionLoadCallback> callbacks;

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
  realfork = (FORKPROC)dlsym(RTLD_NEXT, "fork");
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

  if(!libraryHooks.contains(name))
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
