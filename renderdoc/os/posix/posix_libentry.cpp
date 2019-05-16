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

#include "core/core.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"

void dlopen_hook_init();

// DllMain equivalent
void library_loaded()
{
  if(LibraryHooks::Detect("renderdoc__replay__marker"))
  {
    RDCDEBUG("Not creating hooks - in replay app");

    RenderDoc::Inst().SetReplayApp(true);

    RenderDoc::Inst().Initialise();

    return;
  }
  else
  {
    RenderDoc::Inst().Initialise();

    const char *capturefile = Process::GetEnvVariable("RENDERDOC_CAPFILE");
    const char *opts = Process::GetEnvVariable("RENDERDOC_CAPOPTS");

    if(opts)
    {
      CaptureOptions optstruct;
      optstruct.DecodeFromString(opts);

      RDCLOG("Using delay for debugger %u", optstruct.delayForDebugger);

      RenderDoc::Inst().SetCaptureOptions(optstruct);
    }

    if(capturefile)
    {
      RenderDoc::Inst().SetCaptureFileTemplate(capturefile);
    }

    std::string curfile;
    FileIO::GetExecutableFilename(curfile);

    RDCLOG("Loading into %s", curfile.c_str());

    LibraryHooks::RegisterHooks();

    // we have a short sleep here to allow target control to connect, since unlike windows we can't
    // suspend the process during startup.
    Threading::Sleep(15);
  }
}

// wrap in a struct to enforce ordering. This file is
// linked last, so all other global struct constructors
// should run first
struct init
{
  init() { library_loaded(); }
} do_init;

// we want to be sure the constructor and library_loaded are included even when this is in a static
// library, so we have this global function that does nothing but takes the address.
extern "C" __attribute__((visibility("default"))) void *force_include_libentry()
{
  return &do_init;
}
