/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include <dlfcn.h>
#include <iconv.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#if defined(RENDERDOC_WINDOWING_XLIB)
#include <X11/Xlib-xcb.h>
#endif

#include <replay/renderdoc_replay.h>

void Daemonise()
{
  // don't change dir, but close stdin/stdou
  daemon(1, 0);
}

static Display *display = NULL;

WindowingData DisplayRemoteServerPreview(bool active, const rdcarray<WindowingSystem> &systems)
{
  static WindowingData remoteServerPreview = {WindowingSystem::Unknown};

// we only have the preview implemented for platforms that have xlib & xcb. It's unlikely
// a meaningful platform exists with only one, and at the time of writing no other windowing
// systems are supported on linux for the replay
#if defined(RENDERDOC_WINDOWING_XLIB) && defined(RENDERDOC_WINDOWING_XCB)
  if(active)
  {
    if(remoteServerPreview.system == WindowingSystem::Unknown)
    {
      // if we're first initialising, create the window
      if(display == NULL)
        return remoteServerPreview;

      int scr = DefaultScreen(display);

      xcb_connection_t *connection = XGetXCBConnection(display);

      if(connection == NULL)
      {
        std::cerr << "Couldn't get XCB connection from Xlib Display" << std::endl;
        return remoteServerPreview;
      }

      XSetEventQueueOwner(display, XCBOwnsEventQueue);

      const xcb_setup_t *setup = xcb_get_setup(connection);
      xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
      while(scr-- > 0)
        xcb_screen_next(&iter);

      xcb_screen_t *screen = iter.data;

      uint32_t value_mask, value_list[32];

      xcb_window_t window = xcb_generate_id(connection);

      value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
      value_list[0] = screen->black_pixel;
      value_list[1] =
          XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

      xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 1280, 720, 0,
                        XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, value_list);

      /* Magic code that will send notification when window is destroyed */
      xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
      xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, 0);

      xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
      xcb_intern_atom_reply_t *atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

      xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME,
                          XCB_ATOM_STRING, 8, sizeof("Remote Server Preview") - 1,
                          "Remote Server Preview");

      xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, (*reply).atom, 4, 32, 1,
                          &(*atom_wm_delete_window).atom);
      free(reply);

      xcb_map_window(connection, window);

      bool xcb = false, xlib = false;

      for(size_t i = 0; i < systems.size(); i++)
      {
        if(systems[i] == WindowingSystem::Xlib)
          xlib = true;
        if(systems[i] == WindowingSystem::XCB)
          xcb = true;
      }

      // prefer xcb
      if(xcb)
        remoteServerPreview = CreateXCBWindowingData(connection, window);
      else if(xlib)
        remoteServerPreview = CreateXlibWindowingData(display, (Drawable)window);

      xcb_flush(connection);
    }
    else
    {
      // otherwise, we can pump messages here, but we don't actually care to process any. Just clear
      // the queue
      xcb_generic_event_t *event = NULL;

      xcb_connection_t *connection = remoteServerPreview.xcb.connection;

      if(remoteServerPreview.system == WindowingSystem::Xlib)
        connection = XGetXCBConnection(remoteServerPreview.xlib.display);

      if(connection)
      {
        do
        {
          event = xcb_poll_for_event(connection);
          if(event)
            free(event);
        } while(event);
      }
    }
  }
  else
  {
    // reset the windowing data to 'no window'
    remoteServerPreview = {WindowingSystem::Unknown};
  }
#endif

  return remoteServerPreview;
}

void DisplayRendererPreview(IReplayController *renderer, TextureDisplay &displayCfg, uint32_t width,
                            uint32_t height, uint32_t numLoops)
{
// we only have the preview implemented for platforms that have xlib & xcb. It's unlikely
// a meaningful platform exists with only one, and at the time of writing no other windowing
// systems are supported on linux for the replay
#if defined(RENDERDOC_WINDOWING_XLIB) && defined(RENDERDOC_WINDOWING_XCB)
  // need to create a hybrid setup xlib and xcb in case only one or the other is supported.
  // We'll prefer xcb

  if(display == NULL)
  {
    std::cerr << "Couldn't open X Display" << std::endl;
    return;
  }

  int scr = DefaultScreen(display);

  xcb_connection_t *connection = XGetXCBConnection(display);

  if(connection == NULL)
  {
    std::cerr << "Couldn't get XCB connection from Xlib Display" << std::endl;
    return;
  }

  XSetEventQueueOwner(display, XCBOwnsEventQueue);

  const xcb_setup_t *setup = xcb_get_setup(connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  while(scr-- > 0)
    xcb_screen_next(&iter);

  xcb_screen_t *screen = iter.data;

  uint32_t value_mask, value_list[32];

  xcb_window_t window = xcb_generate_id(connection);

  value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  value_list[0] = screen->black_pixel;
  value_list[1] =
      XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

  xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, width, height, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, value_list);

  /* Magic code that will send notification when window is destroyed */
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, 0);

  xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
  xcb_intern_atom_reply_t *atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
                      8, sizeof("renderdoccmd") - 1, "renderdoccmd");

  xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, (*reply).atom, 4, 32, 1,
                      &(*atom_wm_delete_window).atom);
  free(reply);

  xcb_map_window(connection, window);

  rdcarray<WindowingSystem> systems = renderer->GetSupportedWindowSystems();

  bool xcb = false, xlib = false;

  for(size_t i = 0; i < systems.size(); i++)
  {
    if(systems[i] == WindowingSystem::Xlib)
      xlib = true;
    if(systems[i] == WindowingSystem::XCB)
      xcb = true;
  }

  IReplayOutput *out = NULL;

  // prefer xcb
  if(xcb)
  {
    out = renderer->CreateOutput(CreateXCBWindowingData(connection, window),
                                 ReplayOutputType::Texture);
  }
  else if(xlib)
  {
    out = renderer->CreateOutput(CreateXlibWindowingData(display, (Drawable)window),
                                 ReplayOutputType::Texture);
  }
  else
  {
    std::cerr << "Neither XCB nor XLib are supported, can't create window." << std::endl;
    std::cerr << "Supported systems: ";
    for(size_t i = 0; i < systems.size(); i++)
      std::cerr << (uint32_t)systems[i] << std::endl;
    std::cerr << std::endl;
    return;
  }

  out->SetTextureDisplay(displayCfg);

  xcb_flush(connection);

  uint32_t loopCount = 0;

  bool done = false;
  while(!done)
  {
    xcb_generic_event_t *event;

    event = xcb_poll_for_event(connection);
    if(event)
    {
      switch(event->response_type & 0x7f)
      {
        case XCB_EXPOSE: break;
        case XCB_CLIENT_MESSAGE:
          if((*(xcb_client_message_event_t *)event).data.data32[0] == (*atom_wm_delete_window).atom)
          {
            done = true;
          }
          break;
        case XCB_KEY_RELEASE:
        {
          const xcb_key_release_event_t *key = (const xcb_key_release_event_t *)event;

          if(key->detail == 0x9)
            done = true;
        }
        break;
        case XCB_DESTROY_NOTIFY: done = true; break;
        default: break;
      }
      free(event);
    }

    renderer->SetFrameEvent(10000000, true);
    out->Display();

    usleep(100000);

    loopCount++;

    if(numLoops > 0 && loopCount == numLoops)
      break;
  }
#else
  std::cerr << "No supporting windowing systems defined at build time (xlib and xcb)" << std::endl;
#endif
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

#if defined(RENDERDOC_WINDOWING_XLIB) || defined(RENDERDOC_WINDOWING_XCB)
  // call XInitThreads - although we don't use xlib concurrently the driver might need to.
  XInitThreads();

  // we don't check if display successfully opened, it's only a problem if it's needed later.
  display = env.xlibDisplay = XOpenDisplay(NULL);
#endif

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

#if defined(RENDERDOC_SUPPORT_GLES)
    support += "GLES, ";
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

#if defined(RENDERDOC_WINDOWING_XLIB)
    support += "xlib, ";
    count++;
#endif

#if defined(RENDERDOC_WINDOWING_XCB)
    support += "XCB, ";
    count++;
#endif

#if defined(RENDERDOC_WINDOWING_WAYLAND)
    support += "Wayland (CAPTURE ONLY), ";
    count++;
#endif

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

#if defined(RENDERDOC_WINDOWING_XLIB) || defined(RENDERDOC_WINDOWING_XCB)
  if(display)
    XCloseDisplay(display);
#endif

  return ret;
}
