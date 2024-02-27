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

#include <algorithm>
#include "api/replay/version.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_replay.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" const rdcstr VulkanLayerJSONBasename;

bool VulkanReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  if(m_OutputWindows[id].m_WindowSystem == WindowingSystem::Headless)
    return true;

  VULKANNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

void WrappedVulkan::AddRequiredExtensions(bool instance, rdcarray<rdcstr> &extensionList,
                                          const std::set<rdcstr> &supportedExtensions)
{
  bool device = !instance;

// check if our compile-time options expect any WSI to be available, or if it's all disabled
#define EXPECT_WSI 0

#if(defined(VK_USE_PLATFORM_ANDROID_KHR) || defined(VK_USE_PLATFORM_XCB_KHR) ||  \
    defined(VK_USE_PLATFORM_WAYLAND_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR) || \
    defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT) ||  \
    defined(VK_USE_PLATFORM_GGP))

#undef EXPECT_WSI
#define EXPECT_WSI 1

#endif

  if(instance)
  {
    // don't add duplicates
    if(!extensionList.contains(VK_KHR_SURFACE_EXTENSION_NAME))
      extensionList.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    // check if supported
    if(supportedExtensions.find(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::Wayland);

      // don't add duplicates
      if(!extensionList.contains(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME))
        extensionList.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
    // check if supported
    if(supportedExtensions.find(VK_KHR_XCB_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::XCB);

      // don't add duplicates
      if(!extensionList.contains(VK_KHR_XCB_SURFACE_EXTENSION_NAME))
        extensionList.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
    // check if supported
    if(supportedExtensions.find(VK_KHR_XLIB_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::Xlib);

      // don't add duplicates
      if(!extensionList.contains(VK_KHR_XLIB_SURFACE_EXTENSION_NAME))
        extensionList.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_METAL_EXT)
    // check if supported
    if(supportedExtensions.find(VK_EXT_METAL_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::MacOS);

      RDCLOG("Will create surfaces using " VK_EXT_METAL_SURFACE_EXTENSION_NAME);

      // don't add duplicates, application will have added this but just be sure
      if(!extensionList.contains(VK_EXT_METAL_SURFACE_EXTENSION_NAME))
        extensionList.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_MACOS_MVK)
    // check if supported
    if(supportedExtensions.find(VK_MVK_MACOS_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      m_SupportedWindowSystems.push_back(WindowingSystem::MacOS);

      RDCLOG("Will create surfaces using " VK_MVK_MACOS_SURFACE_EXTENSION_NAME);

      // don't add duplicates, application will have added this but just be sure
      if(!extensionList.contains(VK_MVK_MACOS_SURFACE_EXTENSION_NAME))
        extensionList.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
    }
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    // must be supported
    RDCASSERT(supportedExtensions.find(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) !=
              supportedExtensions.end());

    m_SupportedWindowSystems.push_back(WindowingSystem::Android);

    // don't add duplicates, application will have added this but just be sure
    if(!extensionList.contains(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME))
      extensionList.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_GGP)
    // must be supported
    RDCASSERT(supportedExtensions.find(VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME) !=
              supportedExtensions.end());

    m_SupportedWindowSystems.push_back(WindowingSystem::GGP);

    // don't add duplicates, application will have added this but just be sure
    if(!extensionList.contains(VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME))
      extensionList.push_back(VK_GGP_STREAM_DESCRIPTOR_SURFACE_EXTENSION_NAME);
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

#if defined(VK_USE_PLATFORM_MACOS_MVK) && defined(VK_USE_PLATFORM_METAL_EXT)
      RDCWARN("macOS Output requires the '%s' or '%s' extensions to be present",
              VK_MVK_MACOS_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT)
      RDCWARN("macOS Output requires the '%s' extension to be present",
#if defined(VK_USE_PLATFORM_MACOS_MVK)
              VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_METAL_EXT)
              VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
      );
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
      RDCWARN("Android Output requires the '%s' extension to be present",
              VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
      RDCWARN("Wayland Output requires the '%s' extension to be present",
              VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
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
bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, rdcarray<rdcstr> &myJSONs,
                                    rdcarray<rdcstr> &otherJSONs)
{
  return false;
}

void VulkanReplay::InstallVulkanLayer(bool systemLevel)
{
}
#else
static rdcstr GenerateJSON(const rdcstr &sopath)
{
  char *txt = (char *)driver_vulkan_renderdoc_json;
  int len = driver_vulkan_renderdoc_json_len;

  rdcstr json = rdcstr(txt, len);

  const char modulePathString[] = "@VULKAN_LAYER_MODULE_PATH@";

  int32_t idx = json.find(modulePathString);

  json = json.substr(0, idx) + sopath + json.substr(idx + sizeof(modulePathString) - 1);

  const char majorString[] = "@RENDERDOC_VERSION_MAJOR@";

  idx = json.find(majorString);
  while(idx >= 0)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MAJOR) +
                                           json.substr(idx + sizeof(majorString) - 1);

    idx = json.find(majorString);
  }

  const char minorString[] = "@RENDERDOC_VERSION_MINOR@";

  idx = json.find(minorString);
  while(idx >= 0)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MINOR) +
                                           json.substr(idx + sizeof(minorString) - 1);

    idx = json.find(minorString);
  }

  const char enableVarString[] = "@VULKAN_ENABLE_VAR@";

  idx = json.find(enableVarString);
  while(idx >= 0)
  {
    json = json.substr(0, idx) + "ENABLE_VULKAN_" + strupper(VulkanLayerJSONBasename) + "_CAPTURE" +
           json.substr(idx + sizeof(enableVarString) - 1);

    idx = json.find(enableVarString);
  }

  return json;
}

static bool FileExists(const rdcstr &path)
{
  return access(path.c_str(), F_OK) == 0;
}

static rdcstr GetSOFromJSON(const rdcstr &json)
{
  char *json_string = new char[1024];
  memset(json_string, 0, 1024);

  FILE *f = FileIO::fopen(json, FileIO::ReadText);

  if(f)
  {
    FileIO::fread(json_string, 1, 1024, f);

    FileIO::fclose(f);
  }

  rdcstr ret = "";

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

  // get the realpath, if this is a real filename
  char *resolved = realpath(ret.c_str(), NULL);
  if(resolved && resolved[0])
  {
    ret = resolved;
    free(resolved);
  }

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

rdcstr LayerRegistrationPath(LayerPath path)
{
  const rdcstr json_filename =
      VulkanLayerJSONBasename + "_capture" STRINGIZE(RENDERDOC_VULKAN_JSON_SUFFIX) ".json";

  switch(path)
  {
    case LayerPath::usr: return "/usr/share/vulkan/implicit_layer.d/" + json_filename;
    case LayerPath::etc: return "/etc/vulkan/implicit_layer.d/" + json_filename;
    case LayerPath::home:
    {
      rdcstr xdg = Process::GetEnvVariable("XDG_DATA_HOME");
      if(!xdg.empty() && FileIO::exists(xdg))
        return xdg + "/vulkan/implicit_layer.d/" + json_filename;

      rdcstr home_path = Process::GetEnvVariable("HOME");
      return home_path + "/.local/share/vulkan/implicit_layer.d/" + json_filename;
    }
    default: break;
  }

  return "";
}

void MakeParentDirs(rdcstr file)
{
  rdcstr dir = get_dirname(file);

  if(dir == "/" || dir.empty())
    return;

  MakeParentDirs(dir);

  if(FileExists(dir))
    return;

  mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, rdcarray<rdcstr> &myJSONs,
                                    rdcarray<rdcstr> &otherJSONs)
{
  ////////////////////////////////////////////////////////////////////////////////////////
  // check that there's only one layer registered, and it points to the same .so file that
  // we are running with in this instance of renderdoccmd

  rdcstr librenderdoc_path;
  FileIO::GetLibraryFilename(librenderdoc_path);

  char *resolved = realpath(librenderdoc_path.c_str(), NULL);
  if(resolved && resolved[0])
  {
    librenderdoc_path = resolved;
    free(resolved);
  }

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

  flags = VulkanLayerFlags::UserRegisterable | VulkanLayerFlags::UpdateAllowed;

  if(numMatch >= 1)
    flags |= VulkanLayerFlags::ThisInstallRegistered;

  // if we only have one registration, check that it points to us. If so, we're good
  if(numExist == 1 && numMatch == 1)
    return false;

  if(numMatch == 1 && exist[(int)LayerPath::etc] && match[(int)LayerPath::etc])
  {
    // if only /etc is registered matching us, keep things simple and don't allow unregistering it
    // and registering the /home. Just unregister the /home that doesn't match
    flags &= ~(VulkanLayerFlags::UserRegisterable | VulkanLayerFlags::UpdateAllowed);
  }

  if(exist[(int)LayerPath::usr] && !match[(int)LayerPath::usr])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::usr));

  if(exist[(int)LayerPath::etc] && !match[(int)LayerPath::etc])
  {
    // if the /etc manifest doesn't match we need to elevate to fix it regardless of whether we
    // delete it in favour of a /home manifest, or if we update it.
    flags |= VulkanLayerFlags::NeedElevation;
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
  }

  if(exist[(int)LayerPath::home] && !match[(int)LayerPath::home])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));

  if(!otherJSONs.empty())
    flags |= VulkanLayerFlags::OtherInstallsRegistered;

  if(exist[(int)LayerPath::usr] && match[(int)LayerPath::usr])
  {
    // just need to unregister others, but we can't user-local register anymore (as that would
    // require removing the one in /usr which we can't do)
    flags &= ~VulkanLayerFlags::UserRegisterable;

    // any other manifests that exist, even if they match, are considered others.
    if(exist[(int)LayerPath::home])
    {
      otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));
      flags |= VulkanLayerFlags::OtherInstallsRegistered;
    }

    // any other manifests that exist, even if they match, are considered others.
    if(exist[(int)LayerPath::etc])
    {
      otherJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
      flags |= VulkanLayerFlags::OtherInstallsRegistered | VulkanLayerFlags::NeedElevation;
    }
  }
  else
  {
    // if we have multiple matches but they are all correct, and there are no other JSONs we just
    // report that home needs to be unregistered.
    if(otherJSONs.empty() && exist[(int)LayerPath::etc] && match[(int)LayerPath::etc])
    {
      flags &= ~(VulkanLayerFlags::UserRegisterable | VulkanLayerFlags::UpdateAllowed);
      flags |= VulkanLayerFlags::OtherInstallsRegistered;
      myJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
      otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));
    }
    else
    {
      myJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
      myJSONs.push_back(LayerRegistrationPath(LayerPath::home));
    }
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
  rdcstr usrPath = LayerRegistrationPath(LayerPath::usr);
  rdcstr homePath = LayerRegistrationPath(LayerPath::home);
  rdcstr etcPath = LayerRegistrationPath(LayerPath::etc);

  if(FileExists(usrPath))
  {
    // if the usr path exists, all we can do is try to remove etc & home. This assumes a
    // system-level install
    if(!systemLevel)
    {
      RDCERR("Can't register user-local with manifest under /usr");
      return;
    }

    if(FileExists(homePath))
    {
      if(unlink(homePath.c_str()) < 0)
      {
        const char *const errtext = strerror(errno);
        RDCERR("Error removing %s: %s", homePath.c_str(), errtext);
      }
    }
    if(FileExists(etcPath))
    {
      if(unlink(etcPath.c_str()) < 0)
      {
        const char *const errtext = strerror(errno);
        RDCERR("Error removing %s: %s", etcPath.c_str(), errtext);
      }
    }

    return;
  }

  // if we want to install to the system and there's a registration in $HOME, delete it
  if(systemLevel && FileExists(homePath))
  {
    if(unlink(homePath.c_str()) < 0)
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error removing %s: %s", homePath.c_str(), errtext);
    }
  }

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

  rdcstr jsonPath = LayerRegistrationPath(idx);
  rdcstr path = GetSOFromJSON(jsonPath);
  rdcstr libPath;
  FileIO::GetLibraryFilename(libPath);

  if(path != libPath)
  {
    MakeParentDirs(jsonPath);

    FILE *f = FileIO::fopen(jsonPath, FileIO::WriteText);

    if(f)
    {
      fputs(GenerateJSON(libPath).c_str(), f);

      FileIO::fclose(f);
    }
    else
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error writing %s: %s", jsonPath.c_str(), errtext);
    }
  }
}
#endif
