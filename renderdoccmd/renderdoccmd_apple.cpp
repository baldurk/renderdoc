/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <string>

// helpers defined in cocoa_window.mm
extern void *cocoa_windowCreate(int width, int height, const char *title);
extern void *cocoa_windowGetView(void *cocoaWindow);
extern void *cocoa_windowGetLayer(void *cocoaWindow);
extern bool cocoa_windowShouldClose(void *cocoaWindow);
extern bool cocoa_windowPoll(unsigned short &appleKeyCode);

void Daemonise()
{
}

WindowingData DisplayRemoteServerPreview(bool active, const rdcarray<WindowingSystem> &systems)
{
  WindowingData ret = {WindowingSystem::Unknown};
  return ret;
}

void DisplayRendererPreview(IReplayController *renderer, TextureDisplay &displayCfg, uint32_t width,
                            uint32_t height, uint32_t numLoops)
{
  void *cocoaWindow = cocoa_windowCreate(width, height, "renderdoccmd");
  void *view = cocoa_windowGetView(cocoaWindow);
  void *layer = cocoa_windowGetLayer(cocoaWindow);
  IReplayOutput *out =
      renderer->CreateOutput(CreateMacOSWindowingData(view, layer), ReplayOutputType::Texture);

  out->SetTextureDisplay(displayCfg);

  uint32_t loopCount = 0;

  bool done = false;
  while(!done)
  {
    if(cocoa_windowShouldClose(cocoaWindow))
    {
      break;
    }

    unsigned short appleKeyCode;
    if(cocoa_windowPoll(appleKeyCode))
    {
      // kVK_Escape
      if(appleKeyCode == 0x35)
      {
        break;
      }
    }

    renderer->SetFrameEvent(10000000, true);
    out->Display();

    usleep(100000);

    loopCount++;

    if(numLoops > 0 && loopCount == numLoops)
      break;
  }
}

int main(int argc, char *argv[])
{
  setlocale(LC_CTYPE, "");

  // do any apple-specific setup here

  // process any apple-specific arguments here

  GlobalEnvironment env;

  // add compiled-in support to version line
  {
    std::string support = "APIs supported at compile-time: ";
    int count = 0;

#if defined(RENDERDOC_SUPPORT_VULKAN)
    support += "Vulkan, ";
    count++;
#endif

#if defined(RENDERDOC_SUPPORT_GL)
    support += "GL, ";
    count++;
#endif

#if defined(RENDERDOC_SUPPORT_METAL)
    support += "Metal, ";
    count++;
#endif

    if(count == 0)
    {
      support += "None.";
    }
    else
    {
      // remove trailing ', '
      support.pop_back();
      support.pop_back();
      support += ".";
    }

    add_version_line(support);
  }

  return renderdoccmd(env, argc, argv);
}
