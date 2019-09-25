/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#elif defined(RENDERDOC_WINDOWING_WAYLAND)
#include <wayland-client.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#endif

#include <replay/renderdoc_replay.h>

#if defined(RENDERDOC_WINDOWING_XLIB) || defined(RENDERDOC_WINDOWING_XCB)
  static Display *display = NULL;
#elif defined(RENDERDOC_WINDOWING_WAYLAND)
  static struct wl_display *display = NULL;
  static struct wl_registry *registry = NULL;
  static struct wl_compositor *compositor = NULL;

  static struct wl_surface *surface = NULL;
  static struct wl_shell *shell = NULL;
  static struct wl_shell_surface *shell_surface = NULL;
  static struct wl_shm *shm = NULL;
  static struct wl_buffer *buffer = NULL;
  static void *shm_data = NULL;
#endif

void Daemonise()
{
  // don't change dir, but close stdin/stdou
  daemon(1, 0);
}

struct VulkanRegisterCommand : public Command
{
  VulkanRegisterCommand(const GlobalEnvironment &env) : Command(env) {}
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

      std::string ignorePath = std::string(getenv("HOME")) + "/.renderdoc/";

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

void VerifyVulkanLayer(const GlobalEnvironment &env, int argc, char *argv[])
{
  VulkanLayerRegistrationInfo info;

  bool needUpdate = RENDERDOC_NeedVulkanLayerRegistration(&info);

  if(!needUpdate)
  {
    if(!(info.flags & VulkanLayerFlags::Unfixable))
      add_command("vulkanregister", new VulkanRegisterCommand(env));
    return;
  }

  std::cerr << "*************************************************************************"
            << std::endl;
  std::cerr << "**          Warning: Vulkan capture possibly not configured.           **"
            << std::endl;
  std::cerr << std::endl;

  if(info.flags & VulkanLayerFlags::OtherInstallsRegistered)
    std::cerr << "Multiple RenderDoc layers are registered, possibly from different builds."
              << std::endl;

  if(!(info.flags & VulkanLayerFlags::ThisInstallRegistered))
    std::cerr << "This build's RenderDoc layer is not registered." << std::endl;

  std::cerr << "To fix this, the following actions must take place: " << std::endl << std::endl;

  const bool registerAll = bool(info.flags & VulkanLayerFlags::RegisterAll);
  const bool updateAllowed = bool(info.flags & VulkanLayerFlags::UpdateAllowed);

  for(const rdcstr &j : info.otherJSONs)
    std::cerr << (updateAllowed ? "Unregister/update: " : "Unregister: ") << j.c_str() << std::endl;

  if(!(info.flags & VulkanLayerFlags::ThisInstallRegistered))
  {
    if(registerAll)
    {
      for(const rdcstr &j : info.myJSONs)
        std::cerr << (updateAllowed ? "Register/update: " : "Register: ") << j.c_str() << std::endl;
    }
    else
    {
      std::cerr << (updateAllowed ? "Register one of:" : "Register/update one of:") << std::endl;
      for(const rdcstr &j : info.myJSONs)
        std::cerr << "  -- " << j.c_str() << "\n";
    }
  }

  std::cerr << std::endl;

  if(info.flags & VulkanLayerFlags::Unfixable)
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

  add_command("vulkanregister", new VulkanRegisterCommand(env));
}

#if defined(RENDERDOC_WINDOWING_WAYLAND)

static void shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
}

static const struct wl_shm_listener shm_listener = {
  shm_format,
};

static void registry_handler(void *data, struct wl_registry *registry, uint32_t id,
         const char *interface, uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0)
    {
      compositor = (wl_compositor *) wl_registry_bind(registry, id,
                    &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, "wl_shell") == 0)
    {
      shell = (wl_shell *) wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        shm = (wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, 1);
        wl_shm_add_listener(shm, &shm_listener, NULL);
    }
}

static void registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
  registry_handler,
  registry_remover
};

static int set_cloexec_or_close(int fd)
{
  long flags;

  if (fd == -1)
    return -1;

  flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    goto err;

  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
    goto err;

  return fd;

err:
  close(fd);
  return -1;
}

static int create_tmpfile_cloexec(char *tmpname)
{
  int fd;

#ifdef HAVE_MKOSTEMP
  fd = mkostemp(tmpname, O_CLOEXEC);
  if (fd >= 0)
    unlink(tmpname);
#else
  fd = mkstemp(tmpname);
  if (fd >= 0) {
    fd = set_cloexec_or_close(fd);
    unlink(tmpname);
  }
#endif

  return fd;
}

static int os_create_anonymous_file(off_t size)
{
  static char temp[] = "/weston-shared-XXXXXX";
  const char *path;
  char *name;
  int fd;

  path = getenv("XDG_RUNTIME_DIR");
  if (!path) {
    errno = ENOENT;
    return -1;
  }

  name = (char *) malloc(strlen(path) + sizeof(temp));
  if (!name)
    return -1;

  strcpy(name, path);
  strcat(name, temp);

  fd = create_tmpfile_cloexec(name);

  free(name);

  if (fd < 0)
    return -1;

  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static struct wl_buffer *create_buffer(uint32_t width, uint32_t height)
{
  struct wl_shm_pool *pool;
  int stride = width * 4; // 4 bytes per pixel
  int size = stride * height;
  int fd;
  struct wl_buffer *buff;

  fd = os_create_anonymous_file(size);
  if (fd < 0)
  {
    std::cerr << "Creating a buffer file for " << size << " B failed" << std::endl;
    return NULL;
  }

  shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm_data == MAP_FAILED)
  {
    std::cerr << "mmap failed" << std::endl;
    close(fd);
    return NULL;
  }

  pool = wl_shm_create_pool(shm, fd, size);
  buff = wl_shm_pool_create_buffer(pool, 0, width, height,
          stride, WL_SHM_FORMAT_XRGB8888);

  wl_shm_pool_destroy(pool);
  return buff;
}

static void create_window(uint32_t width, uint32_t height)
{
  buffer = create_buffer(width, height);
  if(buffer == NULL) {
    std::cerr << "Failed to create buffer for Wayland surface" << std::endl;
    return;
  }

  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage(surface, 0, 0, width, height);
  wl_surface_commit(surface);
}
#endif

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
  IReplayOutput *out = NULL;
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
#elif defined(RENDERDOC_WINDOWING_WAYLAND)
  // Wayland should be used only if it is the only option set at build time.
  if(display == NULL)
  {
    std::cerr << "Could not open Wayland display" << std::endl;
    return;
  }

  struct wl_registry *registry = wl_display_get_registry(display);
  if (registry == NULL)
  {
    std::cerr << "Could not get Wayland registry!" <<std::endl;
    return;
  }

  registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);

  wl_display_dispatch(display);
  wl_display_roundtrip(display);

  if (!compositor || !shell)
  {
    std::cerr << "Could not bind Wayland protocols!" << std::endl;
    return;
  }

  surface = wl_compositor_create_surface(compositor);
  if(surface == NULL)
  {
    std::cerr << "Could not create surface for Wayland compositor" << std::endl;
    return;
  }

  shell_surface = wl_shell_get_shell_surface(shell, surface);
  if(shell_surface == NULL)
  {
    std::cerr << "Could not get shell surface for Wayland surface" << std::endl;
    return;
  }

  wl_shell_surface_set_toplevel(shell_surface);

  create_window(width, height);

  out = renderer->CreateOutput(CreateWaylandWindowingData(display, surface),
                            ReplayOutputType::Texture);

  out->SetTextureDisplay(displayCfg);

  uint32_t loopCount = 0;
  bool done = false;
  while(!done)
  {
    if(wl_display_dispatch(display) == -1)
      done = true;

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
#elif defined(RENDERDOC_WINDOWING_WAYLAND)
  display = env.waylandDisplay = wl_display_connect(NULL);
#endif

#if defined(RENDERDOC_SUPPORT_VULKAN)
  VerifyVulkanLayer(env, argc, argv);
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
