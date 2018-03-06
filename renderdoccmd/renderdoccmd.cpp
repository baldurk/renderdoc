/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "renderdoccmd.h"
#include <api/app/renderdoc_app.h>
#include <iostream>
#include <string>
#include "3rdparty/tinyfiledialogs/tinyfiledialogs.h"

RENDERDOC_API_1_1_1 *ConverterAPI = NULL;
RENDERDOC_API_1_1_1 *RENDERDOC_LoadConverter();

int renderdoccmd(std::vector<std::string> &argv)
{
  std::string filename;

  bool silent = false;

  if(argv.size() > 1)
  {
    if(argv[1] == "--help" || argv[1] == "-help" || argv[1] == "/help" || argv[1] == "help" ||
       argv[1] == "--h" || argv[1] == "-h" || argv[1] == "-?" || argv[1] == "/?" || argv[1] == "h")
    {
      std::cerr
          << "This is the RenderDoc conversion utility to convert v0.x captures to a newer version."
          << std::endl;
      std::cerr
          << "It must be able to find the newer renderdoc.dll. "
          << "Either in the library search path or else a prompt will ask you to browse to it."
          << std::endl;
      std::cerr
          << "Run either with a parameter to the file (i.e. drag the file onto this exe) "
          << "or else you can run with no parameters and it will prompt you to browse to the file"
          << std::endl;
      return 0;
    }

    filename = argv[1];

    if(argv.size() > 2 &&
       (argv[2] == "--silent" || argv[2] == "--quiet" || argv[2] == "-q" || argv[2] == "-s"))
    {
      silent = true;
    }
  }

  RENDERDOC_API_1_1_1 *api = RENDERDOC_LoadConverter();

  if(!api)
    return 1;

  FILE *f = fopen(filename.c_str(), "rb");
  if(f)
    fclose(f);
  else
    filename.clear();

  if(filename.empty())
  {
    const char *filter = "*.rdc";
    const char *ret =
        tinyfd_openFileDialog("Locate file to convert", NULL, 1, &filter, "RenderDoc capture", 0);

    if(ret)
      filename = ret;
  }

  if(filename.empty())
    return 0;

  ICaptureFile *file = RENDERDOC_OpenCaptureFile(filename.c_str());

  if(file->OpenStatus() != ReplayStatus::Succeeded)
  {
    tinyfd_messageBox("Couldn't load file", "Couldn't load specified capture file", "ok", "error", 1);
    return 1;
  }

  if(!silent)
    tinyfd_messageBox("Capture loading",
                      "The capture will load when you press OK. "
                      "This will happen invisibly, please wait.",
                      "ok", "info", 1);

  IReplayController *renderer = NULL;
  ReplayStatus status = ReplayStatus::InternalError;
  std::tie(status, renderer) = file->OpenCapture(NULL);

  file->Shutdown();

  if(status == ReplayStatus::Succeeded)
  {
    // prime the pump a couple of times
    for(size_t i = 0; i < 3; i++)
      renderer->SetFrameEvent(10000000, true);

    if(!silent)
      tinyfd_messageBox(
          "Capture conversion ready",
          "The capture is ready to convert. Press OK to begin, this may take a moment...", "ok",
          "info", 1);

    // set up for capture and do the replay we'll capture
    ConverterAPI = api;
    renderer->SetFrameEvent(10000000, true);
    ConverterAPI = NULL;

    if(!silent)
      tinyfd_messageBox("Capture converted",
                        "The capture has been converted and output next to this exe.", "ok", "info",
                        1);

    renderer->Shutdown();
    return 0;
  }

  tinyfd_messageBox("Capture open failed", "Failed to open and replay capture", "ok", "error", 1);

  return 0;
}

int renderdoccmd(int argc, char **c_argv)
{
  std::vector<std::string> argv;
  argv.resize(argc);
  for(int i = 0; i < argc; i++)
    argv[i] = c_argv[i];

  return renderdoccmd(argv);
}
