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

#include "api/replay/version.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_replay.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

bool VulkanReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  if(m_OutputWindows[id].m_WindowSystem == WindowingSystem::Headless)
    return true;

  VULKANNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

void WrappedVulkan::AddRequiredExtensions(bool instance, std::vector<std::string> &extensionList,
                                          const std::set<std::string> &supportedExtensions)
{
  bool device = !instance;

// check if our compile-time options expect any WSI to be available, or if it's all disabled
#define EXPECT_WSI 0

#if(defined(VK_USE_PLATFORM_ANDROID_KHR) || defined(VK_USE_PLATFORM_XCB_KHR) || \
    defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_MACOS_MVK) ||  \
    defined(VK_USE_PLATFORM_GGP))

#undef EXPECT_WSI
#define EXPECT_WSI 1

#endif

  if(instance)
  {
    // don't add duplicates
    if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_SURFACE_EXTENSION_NAME) ==
       extensionList.end())
      extensionList.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_XCB_KHR)
    // check if supported
    if(supportedExtensions.find(VK_KHR_XCB_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::XCB);

      // don't add duplicates
      if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_XCB_SURFACE_EXTENSION_NAME) ==
         extensionList.end())
      {
        extensionList.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
      }
    }
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
    // check if supported
    if(supportedExtensions.find(VK_KHR_XLIB_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::Xlib);

      // don't add duplicates
      if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_XLIB_SURFACE_EXTENSION_NAME) ==
         extensionList.end())
      {
        extensionList.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
      }
    }
#endif

#if defined(VK_USE_PLATFORM_MACOS_MVK)
    // must be supported
    RDCASSERT(supportedExtensions.find(VK_MVK_MACOS_SURFACE_EXTENSION_NAME) !=
              supportedExtensions.end());

    m_SupportedWindowSystems.push_back(WindowingSystem::MacOS);

    // don't add duplicates, application will have added this but just be sure
    if(std::find(extensionList.begin(), extensionList.end(), VK_MVK_MACOS_SURFACE_EXTENSION_NAME) ==
       extensionList.end())
    {
      extensionList.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    // must be supported
    RDCASSERT(supportedExtensions.find(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) !=
              supportedExtensions.end());

    m_SupportedWindowSystems.push_back(WindowingSystem::Android);

    // don't add duplicates, application will have added this but just be sure
    if(std::find(extensionList.begin(), extensionList.end(),
                 VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == extensionList.end())
    {
      extensionList.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_GGP)
    // must be supported
    RDCASSERT(supportedExtensions.find(VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME) !=
              supportedExtensions.end());

    m_SupportedWindowSystems.push_back(WindowingSystem::GGP);

    // don't add duplicates, application will have added this but just be sure
    if(std::find(extensionList.begin(), extensionList.end(),
                 VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME) == extensionList.end())
    {
      extensionList.push_back(VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME);
    }
#endif

#if EXPECT_WSI
    // we must have VK_KHR_surface to support WSI at all
    if(supportedExtensions.find(VK_KHR_SURFACE_EXTENSION_NAME) == supportedExtensions.end())
    {
      RDCWARN("Unsupported instance extension '%s' - disabling WSI support.",
              VK_KHR_SURFACE_EXTENSION_NAME);
      m_SupportedWindowSystems.clear();
    }
#endif

#if EXPECT_WSI

    // if we expected WSI support, warn about it but continue. The UI will have no supported
    // window systems to work with so will be forced to be headless.
    if(m_SupportedWindowSystems.empty())
    {
      RDCWARN("No WSI support - only headless replay allowed.");

#if defined(VK_USE_PLATFORM_MACOS_MVK)
      RDCWARN("macOS Output requires the '%s' extension to be present",
              VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
      RDCWARN("Android Output requires the '%s' extension to be present",
              VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
      RDCWARN("XCB Output requires the '%s' extension to be present",
              VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
      RDCWARN("XLib Output requires the '%s' extension to be present",
              VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_GGP)
      RDCWARN("GGP Output requires the '%s' extension to be present",
              VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME);
#endif
    }

#endif
  }
  else if(device)
  {
    if(!m_SupportedWindowSystems.empty())
    {
      if(supportedExtensions.find(VK_KHR_SWAPCHAIN_EXTENSION_NAME) == supportedExtensions.end())
      {
        RDCWARN("Unsupported required device extension '%s'", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      }
      else
      {
        extensionList.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      }
    }
  }
}

// embedded data file

extern unsigned char driver_vulkan_renderdoc_json[];
extern int driver_vulkan_renderdoc_json_len;

#if ENABLED(RDOC_ANDROID)
bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                    std::vector<std::string> &otherJSONs)
{
  return false;
}

void VulkanReplay::InstallVulkanLayer(bool systemLevel)
{
}
#else
static std::string GenerateJSON(const std::string &sopath)
{
  char *txt = (char *)driver_vulkan_renderdoc_json;
  int len = driver_vulkan_renderdoc_json_len;

  std::string json = std::string(txt, txt + len);

  const char dllPathString[] = ".\\\\renderdoc.dll";

  size_t idx = json.find(dllPathString);

  json = json.substr(0, idx) + sopath + json.substr(idx + sizeof(dllPathString) - 1);

  const char majorString[] = "[MAJOR]";

  idx = json.find(majorString);
  while(idx != std::string::npos)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MAJOR) +
           json.substr(idx + sizeof(majorString) - 1);

    idx = json.find(majorString);
  }

  const char minorString[] = "[MINOR]";

  idx = json.find(minorString);
  while(idx != std::string::npos)
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

  std::string ret = "";

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

std::string LayerRegistrationPath(LayerPath path)
{
  switch(path)
  {
    case LayerPath::usr: return "/usr/share/vulkan/implicit_layer.d/renderdoc_capture.json";
    case LayerPath::etc: return "/etc/vulkan/implicit_layer.d/renderdoc_capture.json";
    case LayerPath::home:
    {
      const char *xdg = getenv("XDG_DATA_HOME");
      if(xdg && FileIO::exists(xdg))
        return std::string(xdg) + "/vulkan/implicit_layer.d/renderdoc_capture.json";

      const char *home_path = getenv("HOME");
      return std::string(home_path != NULL ? home_path : "") +
             "/.local/share/vulkan/implicit_layer.d/renderdoc_capture.json";
    }
    default: break;
  }

  return "";
}

void MakeParentDirs(std::string file)
{
  std::string dir = get_dirname(file);

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

  const char *home_path = getenv("HOME");
  if(home_path == NULL)
    home_path = "";
  if(FileExists(std::string(home_path) + "/.renderdoc/ignore_vulkan_layer_issues"))
  {
    flags = VulkanLayerFlags::ThisInstallRegistered;
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////////////
  // check that there's only one layer registered, and it points to the same .so file that
  // we are running with in this instance of renderdoccmd

  std::string librenderdoc_path;
  FileIO::GetLibraryFilename(librenderdoc_path);

  if(librenderdoc_path.empty() || !FileExists(librenderdoc_path))
  {
    RDCERR("Couldn't determine current library path!");
    flags = VulkanLayerFlags::ThisInstallRegistered;
    return false;
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

  std::string jsonPath = LayerRegistrationPath(idx);
  std::string path = GetSOFromJSON(jsonPath);
  std::string libPath;
  FileIO::GetLibraryFilename(libPath);

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
#endif