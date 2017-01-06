/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include <map>
#include <string>
#include "3rdparty/plthook/plthook.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "os/posix/posix_hook.h"
#include "serialise/string_utils.h"

// depending on symbol resolution, dlopen could get called really early.
// until we've initialised, just skip any fancy stuff
static uint32_t hookInited = 0;
#define HOOK_MAGIC_NUMBER 0xAAF00F00

void PosixHookInit()
{
  hookInited = HOOK_MAGIC_NUMBER;
}

// need to lock around use of realdlopen and libraryHooks
Threading::CriticalSection libLock;

static std::map<std::string, dlopenCallback> libraryHooks;

void PosixHookLibrary(const char *name, dlopenCallback cb)
{
  SCOPED_LOCK(libLock);
  libraryHooks[name] = cb;
}

void plthook_lib(void *handle);

typedef void *(*DLOPENPROC)(const char *, int);
DLOPENPROC realdlopen = NULL;

__attribute__((visibility("default"))) void *dlopen(const char *filename, int flag)
{
  if(hookInited != HOOK_MAGIC_NUMBER)
  {
    DLOPENPROC passthru = (DLOPENPROC)dlsym(RTLD_NEXT, "dlopen");

    void *ret = passthru(filename, flag);

    if(filename && ret && (flag & RTLD_DEEPBIND))
      plthook_lib(ret);

    return ret;
  }

  SCOPED_LOCK(libLock);
  if(realdlopen == NULL)
    realdlopen = (DLOPENPROC)dlsym(RTLD_NEXT, "dlopen");

  void *ret = realdlopen(filename, flag);

  if(filename && ret)
  {
    if(flag & RTLD_DEEPBIND)
      plthook_lib(ret);

    for(auto it = libraryHooks.begin(); it != libraryHooks.end(); ++it)
    {
      if(strstr(filename, it->first.c_str()))
      {
        RDCDEBUG("Redirecting dlopen to ourselves for %s", filename);

        it->second(ret);

        ret = realdlopen("librenderdoc.so", flag);
      }
    }
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
  plthook_close(plthook);
}
