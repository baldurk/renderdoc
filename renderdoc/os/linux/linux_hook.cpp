/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include "os/os_specific.h"

#include "linux_hook.h"

#include "serialise/string_utils.h"
#include "common/threading.h"

#include <string.h>
#include <dlfcn.h>

#include <map>
#include <string>

// depending on symbol resolution, dlopen could get called really early.
// until we've initialised, just skip any fancy stuff
static uint32_t hookInited = 0;
#define HOOK_MAGIC_NUMBER 0xAAF00F00

void LinuxHookInit()
{
	hookInited = HOOK_MAGIC_NUMBER;
}

// need to lock around use of realdlopen and libraryHooks
Threading::CriticalSection libLock;

static std::map<std::string, dlopenCallback> libraryHooks;

void LinuxHookLibrary(const char *name, dlopenCallback cb)
{
	SCOPED_LOCK(libLock);
	libraryHooks[name] = cb;
}

typedef void* (*DLOPENPROC)(const char*,int);
DLOPENPROC realdlopen = NULL;

__attribute__ ((visibility ("default")))
void *dlopen(const char *filename, int flag)
{
	if(hookInited != HOOK_MAGIC_NUMBER)
	{
		DLOPENPROC passthru = (DLOPENPROC)dlsym(RTLD_NEXT, "dlopen");
		return passthru(filename, flag);
	}

	SCOPED_LOCK(libLock);
	if(realdlopen == NULL) realdlopen = (DLOPENPROC)dlsym(RTLD_NEXT, "dlopen");

	void *ret = realdlopen(filename, flag);

	if(filename && ret)
	{
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

