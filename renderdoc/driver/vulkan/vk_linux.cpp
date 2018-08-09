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

#include "api/replay/renderdoc_replay.h"
#include "api/replay/version.h"
#include "strings/string_utils.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vk_core.h"
#include "vk_replay.h"

void VulkanReplay::OutputWindow::SetWindowHandle(WindowingData window)
{
#if ENABLED(RDOC_XLIB)
  if(window.system == WindowingSystem::Xlib)
  {
    xlib.display = window.xlib.display;
    xlib.window = window.xlib.window;
    return;
  }
#endif

#if ENABLED(RDOC_XCB)
  if(window.system == WindowingSystem::XCB)
  {
    xcb.connection = window.xcb.connection;
    xcb.window = window.xcb.window;
    return;
  }
#endif

  RDCERR("Unrecognised/unsupported window system %d", window.system);
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
#if ENABLED(RDOC_XLIB)
  if(m_WindowSystem == WindowingSystem::Xlib)
  {
    VkXlibSurfaceCreateInfoKHR createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.dpy = xlib.display;
    createInfo.window = xlib.window;

    VkResult vkr = ObjDisp(inst)->CreateXlibSurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    return;
  }
#endif

#if ENABLED(RDOC_XCB)
  if(m_WindowSystem == WindowingSystem::XCB)
  {
    VkXcbSurfaceCreateInfoKHR createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.connection = xcb.connection;
    createInfo.window = xcb.window;

    VkResult vkr = ObjDisp(inst)->CreateXcbSurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    return;
  }
#endif

  RDCERR("Unrecognised/unsupported window system %d", m_WindowSystem);
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

#if ENABLED(RDOC_XLIB)
  if(outw.m_WindowSystem == WindowingSystem::Xlib)
  {
    XWindowAttributes attr = {};
    XGetWindowAttributes(outw.xlib.display, outw.xlib.window, &attr);

    w = (int32_t)attr.width;
    h = (int32_t)attr.height;

    return;
  }
#endif

#if ENABLED(RDOC_XCB)
  if(outw.m_WindowSystem == WindowingSystem::XCB)
  {
    xcb_get_geometry_cookie_t geomCookie =
        xcb_get_geometry(outw.xcb.connection, outw.xcb.window);    // window is a xcb_drawable_t
    xcb_get_geometry_reply_t *geom = xcb_get_geometry_reply(outw.xcb.connection, geomCookie, NULL);

    w = (int32_t)geom->width;
    h = (int32_t)geom->height;

    free(geom);

    return;
  }
#endif

  RDCERR("Unrecognised/unsupported window system %d", outw.m_WindowSystem);
}

const char *VulkanLibraryName = "libvulkan.so.1";

// embedded data file

extern unsigned char driver_vulkan_renderdoc_json[];
extern int driver_vulkan_renderdoc_json_len;

static std::string GenerateJSON(const std::string &sopath)
{
  char *txt = (char *)driver_vulkan_renderdoc_json;
  int len = driver_vulkan_renderdoc_json_len;

  string json = string(txt, txt + len);

  const char dllPathString[] = ".\\\\renderdoc.dll";

  size_t idx = json.find(dllPathString);

  json = json.substr(0, idx) + sopath + json.substr(idx + sizeof(dllPathString) - 1);

  const char majorString[] = "[MAJOR]";

  idx = json.find(majorString);
  while(idx != string::npos)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MAJOR) +
           json.substr(idx + sizeof(majorString) - 1);

    idx = json.find(majorString);
  }

  const char minorString[] = "[MINOR]";

  idx = json.find(minorString);
  while(idx != string::npos)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MINOR) +
           json.substr(idx + sizeof(minorString) - 1);

    idx = json.find(minorString);
  }

  return json;
}

static bool FileExists(const std::string &path)
{
  return access(path.c_str(), F_OK) == 0;
}

static std::string GetSOFromJSON(const std::string &json)
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

enum class LayerPath : int
{
  usr,
  First = usr,
  etc,
  home,
  Count,
};

ITERABLE_OPERATORS(LayerPath);

string LayerRegistrationPath(LayerPath path)
{
  switch(path)
  {
    case LayerPath::usr: return "/usr/share/vulkan/implicit_layer.d/renderdoc_capture.json";
    case LayerPath::etc: return "/etc/vulkan/implicit_layer.d/renderdoc_capture.json";
    case LayerPath::home:
    {
      const char *xdg = getenv("XDG_DATA_HOME");
      if(xdg && FileIO::exists(xdg))
        return string(xdg) + "/vulkan/implicit_layer.d/renderdoc_capture.json";
      const char *home = getenv("HOME");
      if(home != NULL)
        return string(home) + "/.local/share/vulkan/implicit_layer.d/renderdoc_capture.json";
    }
    default: break;
  }

  return "";
}

string GetThisLibPath()
{
  string librenderdoc_path;

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
      while(isalnum(c[0]) || c[0] == ':')
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
        librenderdoc_path = string(c, end - c);
    }

    delete[] map_string;
  }

  return librenderdoc_path;
}

void MakeParentDirs(std::string file)
{
  std::string dir = dirname(file);

  if(dir == "/" || dir.empty())
    return;

  MakeParentDirs(dir);

  if(FileExists(dir))
    return;

  mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                    std::vector<std::string> &otherJSONs)
{
  // see if the user has suppressed all this checking as a "I know what I'm doing" measure

  const char *home = getenv("HOME");
  if(home == NULL)
    home = "";
  if(FileExists(string(home) + "/.renderdoc/ignore_vulkan_layer_issues"))
  {
    flags = VulkanLayerFlags::ThisInstallRegistered;
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // check that there's only one layer registered, and it points to the same .so file that
  // we are running with in this instance of renderdoccmd

  // this is a hack, but the only reliable way to find the absolute path to the library.
  // dladdr would be fine but it returns the wrong result for symbols in the library
  string librenderdoc_path = GetThisLibPath();

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

  bool exist[arraydim<LayerPath>()];
  bool match[arraydim<LayerPath>()];

  int numExist = 0;
  int numMatch = 0;

  for(LayerPath i : values<LayerPath>())
  {
    exist[(int)i] = FileExists(LayerRegistrationPath(i));
    match[(int)i] = (GetSOFromJSON(LayerRegistrationPath(i)) == librenderdoc_path);

    if(exist[(int)i])
      numExist++;

    if(match[(int)i])
      numMatch++;
  }

  flags = VulkanLayerFlags::CouldElevate | VulkanLayerFlags::UpdateAllowed;

  if(numMatch >= 1)
    flags |= VulkanLayerFlags::ThisInstallRegistered;

  // if we only have one registration, check that it points to us. If so, we're good
  if(numExist == 1 && numMatch == 1)
    return false;

  if(exist[(int)LayerPath::usr] && !match[(int)LayerPath::usr])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::usr));

  if(exist[(int)LayerPath::etc] && !match[(int)LayerPath::etc])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::etc));

  if(exist[(int)LayerPath::home] && !match[(int)LayerPath::home])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));

  if(!otherJSONs.empty())
    flags |= VulkanLayerFlags::OtherInstallsRegistered;

  if(exist[(int)LayerPath::usr] && match[(int)LayerPath::usr])
  {
    // just need to unregister others
  }
  else
  {
    myJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
    myJSONs.push_back(LayerRegistrationPath(LayerPath::home));
  }

  if(exist[(int)LayerPath::usr] && !match[(int)LayerPath::usr])
  {
    flags = VulkanLayerFlags::Unfixable | VulkanLayerFlags::OtherInstallsRegistered;
    otherJSONs.clear();
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::usr));
  }

  return true;
}

void VulkanReplay::InstallVulkanLayer(bool systemLevel)
{
  std::string homePath = LayerRegistrationPath(LayerPath::home);

  // if we want to install to the system and there's a registration in $HOME, delete it
  if(systemLevel && FileExists(homePath))
  {
    if(unlink(homePath.c_str()) < 0)
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error removing %s: %s", homePath.c_str(), errtext);
    }
  }

  std::string etcPath = LayerRegistrationPath(LayerPath::etc);

  // and vice-versa
  if(!systemLevel && FileExists(etcPath))
  {
    if(unlink(etcPath.c_str()) < 0)
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error removing %s: %s", etcPath.c_str(), errtext);
    }
  }

  LayerPath idx = systemLevel ? LayerPath::etc : LayerPath::home;

  string jsonPath = LayerRegistrationPath(idx);
  string path = GetSOFromJSON(jsonPath);
  string libPath = GetThisLibPath();

  if(path != libPath)
  {
    MakeParentDirs(jsonPath);

    FILE *f = fopen(jsonPath.c_str(), "w");

    if(f)
    {
      fputs(GenerateJSON(libPath).c_str(), f);

      fclose(f);
    }
    else
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error writing %s: %s", jsonPath.c_str(), errtext);
    }
  }
}