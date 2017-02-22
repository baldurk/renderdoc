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

using std::string;
using std::vector;

void Daemonise()
{
  // don't change dir, but close stdin/stdou
  daemon(1, 0);
}

struct VulkanRegisterCommand : public Command
{
  VulkanRegisterCommand() {}
  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add("ignore", 'i', "Do nothing and don't warn about Vulkan layer issues.");
    parser.add(
        "system", '\0',
        "Install layer registration to /etc instead of $HOME/.local (requires root privileges)");
  }
  virtual const char *Description()
  {
    return "Attempt to automatically fix Vulkan layer registration issues";
  }
  virtual bool IsInternalOnly() { return false; }
  virtual bool IsCaptureCommand() { return false; }
  virtual int Execute(cmdline::parser &parser, const CaptureOptions &)
  {
    bool ignore = (parser.exist("ignore"));

    if(ignore)
    {
      std::cout << "Not fixing vulkan layer issues, and suppressing future warnings." << std::endl;
      std::cout << "To undo, remove '$HOME/.renderdoc/ignore_vulkan_layer_issues'." << std::endl;

      string ignorePath = string(getenv("HOME")) + "/.renderdoc/";

      mkdir(ignorePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

      ignorePath += "ignore_vulkan_layer";

      FILE *f = fopen(ignorePath.c_str(), "w");

      if(f)
      {
        fputs("This file suppresses any checks for vulkan layer issues.\n", f);
        fputs("Delete this file to restore default checking.\n", f);

        fclose(f);
      }
      else
      {
        std::cerr << "Couldn't create '$HOME/.renderdoc/ignore_vulkan_layer_issues'." << std::endl;
      }

      return 0;
    }

    RENDERDOC_UpdateVulkanLayerRegistration(parser.exist("system"));

    return 0;
  }
};

void VerifyVulkanLayer(int argc, char *argv[])
{
  uint32_t flags = 0;
  rdctype::array<rdctype::str> myJSONs;
  rdctype::array<rdctype::str> otherJSONs;

  bool needUpdate = RENDERDOC_NeedVulkanLayerRegistration(&flags, &myJSONs, &otherJSONs);

  if(!needUpdate)
  {
    if(!(flags & eVulkan_Unfixable))
      add_command("vulkanregister", new VulkanRegisterCommand());
    return;
  }

  std::cerr << "*************************************************************************"
            << std::endl;
  std::cerr << "**          Warning: Vulkan capture possibly not configured.           **"
            << std::endl;
  std::cerr << std::endl;

  if(flags & eVulkan_OtherInstallsRegistered)
    std::cerr << "Multiple RenderDoc layers are registered, possibly from different builds."
              << std::endl;

  if(!(flags & eVulkan_ThisInstallRegistered))
    std::cerr << "This build's RenderDoc layer is not registered." << std::endl;

  std::cerr << "To fix this, the following actions must take place: " << std::endl << std::endl;

  const bool registerAll = (flags & eVulkan_RegisterAll);
  const bool updateAllowed = (flags & eVulkan_UpdateAllowed);

  for(const rdctype::str &j : otherJSONs)
    std::cerr << (updateAllowed ? "Unregister/update: " : "Unregister: ") << j.c_str() << std::endl;

  if(!(flags & eVulkan_ThisInstallRegistered))
  {
    if(registerAll)
    {
      for(const rdctype::str &j : myJSONs)
        std::cerr << (updateAllowed ? "Register/update: " : "Register: ") << j.c_str() << std::endl;
    }
    else
    {
      std::cerr << (updateAllowed ? "Register one of:" : "Register/update one of:") << std::endl;
      for(const rdctype::str &j : myJSONs)
        std::cerr << "  -- " << j.c_str() << "\n";
    }
  }

  std::cerr << std::endl;

  if(flags & eVulkan_Unfixable)
  {
    std::cerr << "NOTE: The renderdoc layer registered in /usr is reserved for distribution"
              << std::endl;
    std::cerr << "controlled packages. RenderDoc cannot automatically unregister this even"
              << std::endl;
    std::cerr << "with root permissions, you must fix this conflict manually." << std::endl
              << std::endl;

    std::cerr << "*************************************************************************"
              << std::endl;
    std::cerr << std::endl;

    return;
  }

  std::cerr << "NOTE: Automatically removing or changing the layer registered in /etc" << std::endl;
  std::cerr << "will require root privileges." << std::endl << std::endl;

  std::cerr << "To fix these issues run the 'vulkanregister' command." << std::endl;
  std::cerr << "Use 'vulkanregister --help' to see more information." << std::endl;
  std::cerr << std::endl;

  std::cerr << "By default 'vulkanregister' will register the layer to your $HOME folder."
            << std::endl;
  std::cerr << "This does not require root permissions." << std::endl;
  std::cerr << std::endl;
  std::cerr << "If you want to install to the system, run 'vulkanregister --system'." << std::endl;
  std::cerr << "This requires root permissions to write to /etc/vulkan/." << std::endl;

  // just in case there's a strange install that is misdetected or something then allow
  // users to suppress this message and just say "I know what I'm doing".
  std::cerr << std::endl;
  std::cerr << "To suppress this warning in future, run 'vulkanregister --ignore'." << std::endl;

  std::cerr << "*************************************************************************"
            << std::endl;
  std::cerr << std::endl;

  add_command("vulkanregister", new VulkanRegisterCommand());
}

void DisplayRendererPreview(ReplayRenderer *renderer, TextureDisplay &displayCfg, uint32_t width,
                            uint32_t height)
{
// we only have the preview implemented for platforms that have xlib & xcb. It's unlikely
// a meaningful platform exists with only one, and at the time of writing no other windowing
// systems are supported on linux for the replay
#if defined(RENDERDOC_WINDOWING_XLIB) && defined(RENDERDOC_WINDOWING_XCB)
  // need to create a hybrid setup xlib and xcb in case only one or the other is supported.
  // We'll prefer xcb

  Display *display = XOpenDisplay(NULL);

  if(display == NULL)
  {
    std::cerr << "Couldn't open X Display" << std::endl;
    return;
  }

  int scr = DefaultScreen(display);

  xcb_connection_t *connection = XGetXCBConnection(display);

  if(connection == NULL)
  {
    XCloseDisplay(display);
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

  rdctype::array<WindowingSystem> systems;
  ReplayRenderer_GetSupportedWindowSystems(renderer, &systems);

  bool xcb = false, xlib = false;

  for(int32_t i = 0; i < systems.count; i++)
  {
    if(systems[i] == eWindowingSystem_Xlib)
      xlib = true;
    if(systems[i] == eWindowingSystem_XCB)
      xcb = true;
  }

  ReplayOutput *out = NULL;

  // prefer xcb
  if(xcb)
  {
    XCBWindowData windowData;
    windowData.connection = connection;
    windowData.window = window;

    out = ReplayRenderer_CreateOutput(renderer, eWindowingSystem_XCB, &windowData,
                                      eOutputType_TexDisplay);
  }
  else if(xlib)
  {
    XlibWindowData windowData;
    windowData.display = display;
    windowData.window = (Drawable)window;    // safe to cast types

    out = ReplayRenderer_CreateOutput(renderer, eWindowingSystem_Xlib, &windowData,
                                      eOutputType_TexDisplay);
  }
  else
  {
    std::cerr << "Neither XCB nor XLib are supported, can't create window." << std::endl;
    std::cerr << "Supported systems: ";
    for(int32_t i = 0; i < systems.count; i++)
      std::cerr << systems[i] << std::endl;
    std::cerr << std::endl;
    return;
  }

  OutputConfig c = {eOutputType_TexDisplay};

  ReplayOutput_SetOutputConfig(out, c);
  ReplayOutput_SetTextureDisplay(out, displayCfg);

  xcb_flush(connection);

  bool done = false;
  while(!done)
  {
    xcb_generic_event_t *event;

    event = xcb_poll_for_event(connection);
    if(event)
    {
      switch(event->response_type & 0x7f)
      {
        case XCB_EXPOSE:
          ReplayRenderer_SetFrameEvent(renderer, 10000000, true);
          ReplayOutput_Display(out);
          break;
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

    ReplayRenderer_SetFrameEvent(renderer, 10000000, true);
    ReplayOutput_Display(out);

    usleep(100000);
  }
#else
  std::cerr << "No supporting windowing systems defined at build time (xlib and xcb)" << std::endl;
#endif
}

#if defined(RENDERDOC_SUPPORT_GL)

// symbol defined in libGL but not librenderdoc.
// Forces link of libGL after renderdoc (otherwise all symbols would
// be resolved and libGL wouldn't link, meaning dlsym(RTLD_NEXT) would fai
extern "C" void glXWaitX();

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

// symbol defined in libEGL but not in librenderdoc.
// Forces link of libEGL.
extern "C" int eglWaitGL(void);

#endif

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
  volatile bool never_run = false;

#if defined(RENDERDOC_SUPPORT_GL)

  if(never_run)
    glXWaitX();

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

  if(never_run)
    eglWaitGL();

#endif

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

#if defined(RENDERDOC_SUPPORT_VULKAN)
  VerifyVulkanLayer(argc, argv);
#endif

  // add compiled-in support to version line
  {
    string support = "APIs supported at compile-time: ";
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

  return renderdoccmd(argc, argv);
}
