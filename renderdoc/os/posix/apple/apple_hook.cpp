/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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

#include <dlfcn.h>
#include <stddef.h>

void LibraryHooks::BeginHookRegistration()
{
  // nothing to do
}

void LibraryHooks::RegisterFunctionHook(const char *libraryName, const FunctionHook &hook)
{
}

void LibraryHooks::RegisterLibraryHook(char const *, FunctionLoadCallback cb)
{
  if(cb)
    RDCERR("Hooking on apple not complete - function load callback not implemented");
}

void LibraryHooks::IgnoreLibrary(const char *libraryName)
{
}

void LibraryHooks::EndHookRegistration()
{
}

bool LibraryHooks::Detect(const char *identifier)
{
  return dlsym(RTLD_DEFAULT, identifier) != NULL;
}

void LibraryHooks::RemoveHooks()
{
  RDCERR("Removing hooks is not possible on this platform");
}

// android only hooking functions, not used on apple
ScopedSuppressHooking::ScopedSuppressHooking()
{
}

ScopedSuppressHooking::~ScopedSuppressHooking()
{
}

void LibraryHooks::Refresh()
{
}