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

// this is exported from vk_linux.cpp

#if defined(RENDERDOC_SUPPORT_VULKAN)

extern "C" void RENDERDOC_GetLayerJSON(char **txt, int *len);

#else

// just for ease of compiling, define a dummy function

void RENDERDOC_GetLayerJSON(char **txt, int *len)
{
  static char dummy[] = "";
  *txt = dummy;
  *len = 0;
}

#endif

static string GenerateJSON(const string &sopath)
{
  char *txt = NULL;
  int len = 0;

  RENDERDOC_GetLayerJSON(&txt, &len);

  if(len <= 0)
    return "";

  string json = string(txt, txt + len);

  const char dllPathString[] = ".\\\\renderdoc.dll";

  size_t idx = json.find(dllPathString);

  return json.substr(0, idx) + sopath + json.substr(idx + sizeof(dllPathString) - 1);
}

static bool FileExists(const string &path)
{
  FILE *f = fopen(path.c_str(), "r");

  if(f)
  {
    fclose(f);
    return true;
  }

  return false;
}

static string GetSOFromJSON(const string &json)
{
  char *json_string = new char[1024];
  memset(json_string, 0, 1024);

  FILE *f = fopen(json.c_str(), "r");

  if(f)
  {
    fread(json_string, 1, 1024, f);

    fclose(f);
  }

  string ret = "";

  // The line is:
  // "library_path": "/foo/bar/librenderdoc.so",
  char *c = strstr(json_string, "library_path");

  if(c)
  {
    c += sizeof("library_path\": \"") - 1;

    char *quote = strchr(c, '"');

    if(quote)
    {
      *quote = 0;
      ret = c;
    }
  }

  delete[] json_string;

  return ret;
}

enum
{
  USR,
  ETC,
  HOME,
  COUNT
};

string layerRegistrationPath[COUNT] = {
    "/usr/share/vulkan/implicit_layer.d/renderdoc_capture.json",
    "/etc/vulkan/implicit_layer.d/renderdoc_capture.json",
    string(getenv("HOME")) + "/.local/share/vulkan/implicit_layer.d/renderdoc_capture.json"};

struct VulkanRegisterCommand : public Command
{
  VulkanRegisterCommand(bool layer_exists[COUNT], const string &path)
  {
    etcExists = layer_exists[ETC];
    homeExists = layer_exists[HOME];
    libPath = path;
  }

  virtual void AddOptions(cmdline::parser &parser)
  {
    parser.add("ignore", 'i', "Do nothing and don't warn about Vulkan layer issues.");
    parser.add(
        "system", '\0',
        "Install layer registration to /etc instead of $HOME/.local (requires root privileges)");
    parser.add("dry-run", 'n', "Don't perform any actions, instead print what would happen.");
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

    bool system = (parser.exist("system"));
    bool dryrun = (parser.exist("dry-run"));

    // if we want to install to the system and there's a registration in $HOME, delete it
    if(system && homeExists)
    {
      std::cout << "Removing '" << layerRegistrationPath[HOME] << "'" << std::endl;

      if(!dryrun)
      {
        int ret = unlink(layerRegistrationPath[HOME].c_str());

        if(ret < 0)
        {
          const char *const errtext = strerror(errno);
          std::cout << "Error - " << errtext << std::endl;
        }
      }
    }

    // and vice-versa
    if(!system && etcExists)
    {
      std::cout << "Removing '" << layerRegistrationPath[ETC] << "'" << std::endl;

      if(!dryrun)
      {
        int ret = unlink(layerRegistrationPath[ETC].c_str());

        if(ret < 0)
        {
          const char *const errtext = strerror(errno);
          std::cout << "Error - " << errtext << std::endl;
        }
      }
    }

    int idx = system ? ETC : HOME;

    string path = GetSOFromJSON(layerRegistrationPath[idx]);

    if(path != libPath)
    {
      if((system && !etcExists) || (!system && !homeExists))
      {
        std::cout << "Registering '" << layerRegistrationPath[idx] << "'" << std::endl;
      }
      else
      {
        std::cout << "Updating '" << layerRegistrationPath[idx] << "'" << std::endl;
        if(path == "")
        {
          std::cout << "  JSON is corrupt or unrecognised, replacing with valid JSON pointing"
                    << std::endl;
          std::cout << "  to '" << libPath << "'" << std::endl;
        }
        else
        {
          std::cout << "  Repointing from '" << path << "'" << std::endl;
          std::cout << "  to '" << libPath << "'" << std::endl;
        }
      }

      if(!dryrun)
      {
        FILE *f = fopen(layerRegistrationPath[idx].c_str(), "w");

        if(f)
        {
          fputs(GenerateJSON(libPath).c_str(), f);

          fclose(f);
        }
        else
        {
          const char *const errtext = strerror(errno);
          std::cout << "Error - " << errtext << std::endl;
        }
      }
    }

    return 0;
  }

  bool etcExists;
  bool homeExists;
  string libPath;
};

void VerifyVulkanLayer(int argc, char *argv[])
{
  // see if the user has suppressed all this checking as a "I know what I'm doing" measure

  string ignorePath = string(getenv("HOME")) + "/.renderdoc/ignore_vulkan_layer_issues";
  if(FileExists(ignorePath))
    return;

  ////////////////////////////////////////////////////////////////////////////////////////
  // check that there's only one layer registered, and it points to the same .so file that
  // we are running with in this instance of renderdoccmd

  // this is a hack, but the only reliable way to find the absolute path to the library.
  // dladdr would be fine but it returns the wrong result for symbols in the library
  string librenderdoc_path;

  {
    FILE *f = fopen("/proc/self/maps", "r");

    if(f)
    {
      // read the whole thing in one go. There's no need to try and be tight with
      // this allocation, so just make sure we can read everything.
      char *map_string = new char[1024 * 1024];
      memset(map_string, 0, 1024 * 1024);

      fread(map_string, 1, 1024 * 1024, f);

      fclose(f);

      char *c = strstr(map_string, "/librenderdoc.so");

      if(c)
      {
        // walk backwards until we hit the start of the line
        while(c > map_string)
        {
          c--;

          if(c[0] == '\n')
          {
            c++;
            break;
          }
        }

        // walk forwards across the address range (00400000-0040c000)
        while(isalnum(c[0]) || c[0] == '-')
          c++;

        // whitespace
        while(c[0] == ' ')
          c++;

        // permissions (r-xp)
        while(isalpha(c[0]) || c[0] == '-')
          c++;

        // whitespace
        while(c[0] == ' ')
          c++;

        // offset (0000b000)
        while(isalnum(c[0]) || c[0] == '-')
          c++;

        // whitespace
        while(c[0] == ' ')
          c++;

        // dev
        while(isdigit(c[0]) || c[0] == ':')
          c++;

        // whitespace
        while(c[0] == ' ')
          c++;

        // inode
        while(isdigit(c[0]))
          c++;

        // whitespace
        while(c[0] == ' ')
          c++;

        // FINALLY we are at the start of the actual path
        char *end = strchr(c, '\n');

        if(end)
        {
          librenderdoc_path = string(c, end - c);
        }
      }

      delete[] map_string;
    }
  }

  // it's impractical to determine whether the currently running RenderDoc build is just a loose
  // extract of a tarball or a distribution that decided to put all the files in the same folder,
  // and whether or not the library is in ld's searchpath.
  //
  // Instead we just make the requirement that renderdoc.json will always contain an absolute path
  // to the matching librenderdoc.so, so that we can check if it points to this build or another
  // build etc.
  //
  // Note there are three places to register layers - /usr, /etc and /home. The first is reserved
  // for distribution packages, so if it conflicts or needs to be deleted for this install to run,
  // we can't do that and have to just prompt the user. /etc we can mess with since that's for
  // non-distribution packages, but it will need root permissions.

  bool exist[COUNT];
  bool match[COUNT];

  int numExist = 0;
  int numMatch = 0;

  for(int i = 0; i < COUNT; i++)
  {
    exist[i] = FileExists(layerRegistrationPath[i]);
    match[i] = (GetSOFromJSON(layerRegistrationPath[i]) == librenderdoc_path);

    if(exist[i])
      numExist++;

    if(match[i])
      numMatch++;
  }

  // if we only have one registration, check that it points to us. If so, we're good
  if(numExist == 1 && numMatch == 1)
    return;

  // if we're about to execute the command, don't print all this explanatory text.
  if(argc > 1 && !strcmp(argv[1], "vulkanregister"))
  {
    add_command("vulkanregister", new VulkanRegisterCommand(exist, librenderdoc_path));
    return;
  }

  std::cerr << "*************************************************************************"
            << std::endl;
  std::cerr << "**          Warning: Vulkan capture possibly not configured.           **"
            << std::endl;
  std::cerr << std::endl;
  if(numExist > 1)
    std::cerr << "Multiple RenderDoc layers are registered, possibly from different builds."
              << std::endl;
  else if(numExist < 0)
    std::cerr << "RenderDoc layer is not registered." << std::endl;
  else
    std::cerr << "RenderDoc layer is registered, but to a different library." << std::endl;
  std::cerr << "To fix this, the following actions must take place: " << std::endl << std::endl;

  bool printed = false;

  if(exist[USR] && !match[USR])
  {
    std::cerr << "* Unregister: '" << layerRegistrationPath[USR] << "'" << std::endl;
    printed = true;
  }

  if(exist[ETC] && !match[ETC])
  {
    std::cerr << "* Unregister or update: '" << layerRegistrationPath[ETC] << "'" << std::endl;
    printed = true;
  }

  if(exist[HOME] && !match[HOME])
  {
    std::cerr << "* Unregister or update: '" << layerRegistrationPath[HOME] << "'" << std::endl;
    printed = true;
  }

  if(printed)
    std::cerr << std::endl;

  if(exist[USR] && match[USR])
  {
    // just need to unregister others
  }
  else
  {
    if(!exist[ETC] && !exist[HOME])
    {
      std::cerr << "* Register either: '" << layerRegistrationPath[ETC] << "'" << std::endl;
      std::cerr << "               or: '" << layerRegistrationPath[HOME] << "'" << std::endl;
    }
    else
    {
      std::cerr << "* Update or register either: '" << layerRegistrationPath[ETC] << "'" << std::endl;
      std::cerr << "                         or: '" << layerRegistrationPath[HOME] << "'"
                << std::endl;
    }

    std::cerr << std::endl;
  }

  if(exist[USR] && !match[USR])
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

  add_command("vulkanregister", new VulkanRegisterCommand(exist, librenderdoc_path));
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

#if defined(RENDERDOC_SUPPORT_GL)

  volatile bool never_run = false;
  if(never_run)
    glXWaitX();

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

  volatile bool never_run = false;
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
