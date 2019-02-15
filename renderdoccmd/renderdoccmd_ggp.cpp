/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <ggp_c/ggp.h>
#include <replay/renderdoc_replay.h>

const uint64_t kMicrosecondsPerFrame = 16666L;

// struct appData: Global application data
static struct
{
  // GGP
  GgpEventQueue event_queue;
  uint32_t stream_started_handler_id;
  uint32_t stream_stopped_handler_id;
  // General
  bool quit;
} app_data = {0};

void Daemonise()
{
  // don't change dir, but close stdin/stdout
  daemon(1, 0);
}

WindowingData DisplayRemoteServerPreview(bool active, const rdcarray<WindowingSystem> &systems)
{
  static WindowingData remoteServerPreview = {WindowingSystem::Unknown};
  // No preview implemented.
  return remoteServerPreview;
}

// ClockNowMicroSeconds(): Return current time in microseconds
static inline uint64_t ClockNowMicroSeconds()
{
  struct timespec now = {};
  clock_gettime(CLOCK_MONOTONIC_RAW, &now);
  uint64_t nanoseconds = (now.tv_sec * 1000000000LL) + now.tv_nsec;
  uint64_t microseconds = nanoseconds / 1000LL;
  return microseconds;
}

// HandleStreamStarted(): Client connected handler
static void HandleStreamStarted(void *user_data)
{
  std::cout << "GGP client connected" << std::endl;
}

// HandleStreamStopped(): Client disconnected handler
static void HandleStreamStopped(const GgpStreamStoppedEvent *event, void *user_data)
{
  std::cout << "GGP client disconnected" << std::endl;
  // Exit the application if the client disconnects to exit.
  if(event->stream_stopped_reason == kGgpStreamStopped_Exited)
  {
    app_data.quit = true;
  }
  else if(event->stream_stopped_reason == kGgpStreamStopped_Unexpected)
  {
    GgpSuspended();
  }
}

// UnregisterCallback(): Handler unregistered
static void UnregisterCallback(void *user_data)
{
  std::cout << "Unregistered callback" << std::endl;
}

// Initialize(): Initialize application
static void Initialize()
{
  // Initialize event queue
  app_data.event_queue = GgpEventQueueCreate();
  std::cout << "GGP event queue created" << std::endl;
  // Add client connection handlers
  app_data.stream_started_handler_id = GgpAddStreamStartedHandler(
      app_data.event_queue, HandleStreamStarted, NULL, UnregisterCallback);
  app_data.stream_stopped_handler_id = GgpAddStreamStoppedHandler(
      app_data.event_queue, HandleStreamStopped, NULL, UnregisterCallback);
  // Signal that the session is ready to receive events.
  GgpHandlersRegistered();

  GgpReadyToStream();
  std::cout << "GGP ready to stream" << std::endl;
}

// Finalize(): Clean up application resources
static void Finalize()
{
  // Destroy the event queue
  GgpEventQueueDestroy(app_data.event_queue);
  std::cout << "GGP event queue destroyed" << std::endl;
  // Remove client connection handlers.
  GgpRemoveStreamStartedHandler(app_data.stream_started_handler_id);
  GgpRemoveStreamStoppedHandler(app_data.stream_stopped_handler_id);
  // Indicate that GGP should shutdown.
  GgpShutDown();
}

void DisplayRendererPreview(IReplayController *renderer, TextureDisplay &displayCfg, uint32_t width,
                            uint32_t height, uint32_t numLoops)
{
  Initialize();

  IReplayOutput *out = renderer->CreateOutput(CreateGgpWindowingData(), ReplayOutputType::Texture);
  out->SetTextureDisplay(displayCfg);

  // Wait until user closes the window, then exit
  while(!app_data.quit)
  {
    uint64_t whenToResume = ClockNowMicroSeconds() + kMicrosecondsPerFrame;
    while(GgpEventQueueProcessEvent(app_data.event_queue, 0))
    {
    }
    renderer->SetFrameEvent(10000000, true);
    out->Display();
    // Sleep for 1/60 second (one frame)
    uint64_t timeLeft = whenToResume - ClockNowMicroSeconds();
    if(timeLeft > 0)
    {
      struct timespec sleepTime = {};
      sleepTime.tv_nsec = timeLeft * 1000LL;
      nanosleep(&sleepTime, NULL);
    }
  }

  Finalize();
}

void sig_handler(int signo)
{
  if(usingKillSignal)
    killSignal = true;
  else
    exit(1);
}

int main(int argc, char *argv[])
{
  setlocale(LC_CTYPE, "");

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  GlobalEnvironment env;

  // add compiled-in support to version line
  {
    std::string support = "APIs supported at compile-time: ";
    int count = 0;

#if defined(RENDERDOC_SUPPORT_VULKAN)
    support += "Vulkan, ";
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

    support = "Windowing systems supported at compile-time: ";
    count = 0;

    support += "GGP, ";
    count++;

#if defined(RENDERDOC_SUPPORT_VULKAN)
    support += "Vulkan KHR_display, ";
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

  int ret = renderdoccmd(env, argc, argv);

  return ret;
}
