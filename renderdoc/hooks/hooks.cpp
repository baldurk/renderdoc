/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

#include "hooks.h"
#include "common/common.h"

static std::vector<LibraryHook *> &LibList()
{
  static std::vector<LibraryHook *> libs;
  return libs;
}

LibraryHook::LibraryHook()
{
  LibList().push_back(this);
}

void LibraryHooks::RegisterHooks()
{
  BeginHookRegistration();

  for(LibraryHook *lib : LibList())
    lib->RegisterHooks();

  EndHookRegistration();
}

void LibraryHooks::OptionsUpdated()
{
  for(LibraryHook *lib : LibList())
    lib->OptionsUpdated();
}

////////////////////////////////////////////////////////////////////////
// Very temporary compatibility layer with previous function interface.
//
// PosixHookFunction just calls LibraryHooks::RegisterFunctionHook and
// stores the resulting pointer in a map to look up later in
// PosixGetFunction
////////////////////////////////////////////////////////////////////////

static std::map<std::string, void **> origLookup;

void PosixHookFunction(const char *name, void *hook)
{
  void **orig = origLookup[name];
  if(orig == NULL)
  {
    orig = origLookup[name] = new void *;
    *orig = NULL;
  }

  LibraryHooks::RegisterFunctionHook("", FunctionHook(name, orig, hook));
}

void *PosixGetFunction(void *handle, const char *name)
{
  void **orig = origLookup[name];
  if(orig && *orig)
    return *orig;

  ScopedSuppressHooking suppress;
  return Process::GetFunctionAddress(handle, name);
}
