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

#include "core/core.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"

void dlopen_hook_init();

void readCapOpts(const char *str, CaptureOptions *opts)
{
  // serialise from string with two chars per byte
  byte *b = (byte *)opts;
  for(size_t i = 0; i < sizeof(CaptureOptions); i++)
    *(b++) = (byte(str[i * 2 + 0] - 'a') << 4) | byte(str[i * 2 + 1] - 'a');
}

// DllMain equivalent
void library_loaded()
{
  string curfile;
  FileIO::GetExecutableFilename(curfile);

  if(curfile.find("/renderdoccmd") != string::npos ||
     curfile.find("/renderdocui") != string::npos || curfile.find("/qrenderdoc") != string::npos ||
     curfile.find("org.renderdoc.renderdoccmd") != string::npos)
  {
    RDCDEBUG("Not creating hooks - in replay app");

    RenderDoc::Inst().SetReplayApp(true);

    RenderDoc::Inst().Initialise();

    return;
  }
  else
  {
    RenderDoc::Inst().Initialise();

    char *logfile = getenv("RENDERDOC_LOGFILE");
    char *opts = getenv("RENDERDOC_CAPTUREOPTS");

    if(opts)
    {
      string optstr = opts;

      CaptureOptions optstruct;
      readCapOpts(optstr.c_str(), &optstruct);

      RenderDoc::Inst().SetCaptureOptions(optstruct);
    }

    if(logfile)
    {
      RenderDoc::Inst().SetLogFile(logfile);
    }

    RDCLOG("Loading into %s", curfile.c_str());

    LibraryHooks::GetInstance().CreateHooks();
  }
}

// wrap in a struct to enforce ordering. This file is
// linked last, so all other global struct constructors
// should run first
struct init
{
  init() { library_loaded(); }
} do_init;
