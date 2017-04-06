/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "vk_core.h"
#include "vk_replay.h"

static int dllLocator = 0;

void VulkanReplay::OutputWindow::SetWindowHandle(WindowingSystem system, void *data)
{
  RDCASSERT(system == WindowingSystem::Win32, system);
  wnd = (HWND)data;
  m_WindowSystem = system;
}

void VulkanReplay::OutputWindow::CreateSurface(VkInstance inst)
{
  VkWin32SurfaceCreateInfoKHR createInfo;

  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = NULL;
  createInfo.flags = 0;
  createInfo.hwnd = wnd;

  GetModuleHandleExA(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (const char *)&dllLocator, (HMODULE *)&createInfo.hinstance);

  VkResult vkr = ObjDisp(inst)->CreateWin32SurfaceKHR(Unwrap(inst), &createInfo, NULL, &surface);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void VulkanReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  RECT rect = {0};
  GetClientRect(outw.wnd, &rect);
  w = rect.right - rect.left;
  h = rect.bottom - rect.top;
}

bool VulkanReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}

bool WrappedVulkan::AddRequiredExtensions(bool instance, vector<string> &extensionList,
                                          const std::set<string> &supportedExtensions)
{
  bool device = !instance;

  if(instance)
  {
    // for windows we require both extensions as there's no alternative
    if(supportedExtensions.find(VK_KHR_SURFACE_EXTENSION_NAME) == supportedExtensions.end())
    {
      RDCERR("Unsupported required instance extension '%s'", VK_KHR_SURFACE_EXTENSION_NAME);
      return false;
    }

    if(supportedExtensions.find(VK_KHR_WIN32_SURFACE_EXTENSION_NAME) == supportedExtensions.end())
    {
      RDCERR("Unsupported required instance extension '%s'", VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
      return false;
    }

    // don't add duplicates
    if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_SURFACE_EXTENSION_NAME) ==
       extensionList.end())
      extensionList.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_WIN32_SURFACE_EXTENSION_NAME) ==
       extensionList.end())
      extensionList.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
  }
  else if(device)
  {
    if(supportedExtensions.find(VK_KHR_SWAPCHAIN_EXTENSION_NAME) == supportedExtensions.end())
    {
      RDCERR("Unsupported required device extension '%s'", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      return false;
    }

    // don't add duplicates
    if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME) ==
       extensionList.end())
      extensionList.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  return true;
}

#if !defined(VK_USE_PLATFORM_WIN32_KHR)
#error "Win32 KHR platform not defined"
#endif

VkResult WrappedVulkan::vkCreateWin32SurfaceKHR(VkInstance instance,
                                                const VkWin32SurfaceCreateInfoKHR *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(m_State >= WRITING);

  VkResult ret =
      ObjDisp(instance)->CreateWin32SurfaceKHR(Unwrap(instance), pCreateInfo, pAllocator, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    // since there's no point in allocating a full resource record and storing the window
    // handle under there somewhere, we just cast. We won't use the resource record for anything
    wrapped->record = (VkResourceRecord *)pCreateInfo->hwnd;

    Keyboard::AddInputWindow((void *)pCreateInfo->hwnd);
  }

  return ret;
}

VkBool32 WrappedVulkan::vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                                       uint32_t queueFamilyIndex)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceWin32PresentationSupportKHR(Unwrap(physicalDevice), queueFamilyIndex);
}

const char *VulkanLibraryName = "vulkan-1.dll";

std::wstring GetJSONPath(bool wow6432)
{
  wchar_t curFile[1024];
  GetModuleFileNameW(NULL, curFile, 1024);

  wchar_t *lastSlash = wcsrchr(curFile, '\\');
  if(lastSlash)
    *(lastSlash + 1) = 0;

  if(wow6432)
    wcscat_s(curFile, L"x86\\");

  wcscat_s(curFile, L"renderdoc.json");

  return curFile;
}

static HKEY GetImplicitLayersKey(bool writeable, bool wow6432)
{
  std::string basepath = "SOFTWARE\\";

  if(wow6432)
    basepath += "Wow6432Node\\";

  basepath += "Khronos\\Vulkan\\ImplicitLayers";

  HKEY key = NULL;
  LSTATUS ret = ERROR_SUCCESS;

  if(writeable)
    ret = RegCreateKeyExA(HKEY_LOCAL_MACHINE, basepath.c_str(), 0, NULL, 0, KEY_READ | KEY_WRITE,
                          NULL, &key, NULL);
  else
    ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, basepath.c_str(), 0, KEY_READ, &key);

  if(ret != ERROR_SUCCESS)
  {
    if(key)
      RegCloseKey(key);

    // find to fail to open for read, the key may not exist
    if(writeable)
      RDCERR("Couldn't open %s for write", basepath.c_str());

    return NULL;
  }

  return key;
}

bool ProcessImplicitLayersKey(HKEY key, const std::wstring &path,
                              std::vector<std::string> *otherJSONs, bool deleteOthers)
{
  bool thisRegistered = false;

  wchar_t name[1024] = {};
  DWORD nameSize = 1024;
  DWORD idx = 0;

  LONG ret = RegEnumValueW(key, idx++, name, &nameSize, NULL, NULL, NULL, NULL);

  std::wstring myJSON = path;
  for(size_t i = 0; i < myJSON.size(); i++)
    myJSON[i] = towlower(myJSON[i]);

  while(ret == ERROR_SUCCESS)
  {
    // convert the name here so we preserve casing
    std::string utf8name = StringFormat::Wide2UTF8(name);

    for(DWORD i = 0; i <= nameSize && name[i]; i++)
      name[i] = towlower(name[i]);

    if(wcscmp(name, myJSON.c_str()) == 0)
    {
      thisRegistered = true;
    }
    else if(wcsstr(name, L"renderdoc.json") != NULL)
    {
      if(otherJSONs)
        otherJSONs->push_back(utf8name);

      if(deleteOthers)
        RegDeleteValueW(key, name);
    }

    nameSize = 1024;
    ret = RegEnumValueW(key, idx++, name, &nameSize, NULL, NULL, NULL, NULL);
  }

  return thisRegistered;
}

bool VulkanReplay::CheckVulkanLayer(VulkanLayerFlags &flags, std::vector<std::string> &myJSONs,
                                    std::vector<std::string> &otherJSONs)
{
  std::wstring normalPath = GetJSONPath(false);
  myJSONs.push_back(StringFormat::Wide2UTF8(normalPath));

#if ENABLED(RDOC_X64)
  std::wstring wow6432Path = GetJSONPath(true);
  myJSONs.push_back(StringFormat::Wide2UTF8(wow6432Path));
#endif

  HKEY key = GetImplicitLayersKey(false, false);

  // if we couldn't even get the ImplicitLayers reg key the system doesn't have the
  // vulkan runtime, so we return as if we are not registered (as that's the case).
  // People not using vulkan can either ignore the message, or click to set it up
  // and it will go away as we'll have rights to create it.
  if(!key)
  {
    flags = VulkanLayerFlags::NeedElevation | VulkanLayerFlags::RegisterAll;
    return true;
  }

  bool thisRegistered = ProcessImplicitLayersKey(key, normalPath, &otherJSONs, false);

  RegCloseKey(key);

#if ENABLED(RDOC_X64)
  {
    key = GetImplicitLayersKey(false, true);

    // if we're on 64-bit, the layer isn't registered unless both keys are registered.
    thisRegistered = false;

    if(key)
    {
      thisRegistered = ProcessImplicitLayersKey(key, wow6432Path, &otherJSONs, false);

      RegCloseKey(key);
    }
  }
#endif

  flags = VulkanLayerFlags::NeedElevation | VulkanLayerFlags::RegisterAll;

  if(thisRegistered)
    flags |= VulkanLayerFlags::ThisInstallRegistered;

  if(!otherJSONs.empty())
    flags |= VulkanLayerFlags::OtherInstallsRegistered;

  // return true if any changes are needed
  return !otherJSONs.empty() || !thisRegistered;
}

void VulkanReplay::InstallVulkanLayer(bool systemLevel)
{
  HKEY key = GetImplicitLayersKey(true, false);

  const DWORD zero = 0;

  if(key)
  {
    std::wstring path = GetJSONPath(false);

    // this function will delete all non-matching renderdoc.json values, and return true if our own
    // is registered
    bool thisRegistered = ProcessImplicitLayersKey(key, path, NULL, true);

    if(!thisRegistered)
      RegSetValueExW(key, path.c_str(), 0, REG_DWORD, (const BYTE *)&zero, sizeof(zero));

    RegCloseKey(key);
  }

// if we're a 64-bit process, update the 32-bit key
#if ENABLED(RDOC_X64)
  {
    key = GetImplicitLayersKey(true, true);

    if(key)
    {
      std::wstring path = GetJSONPath(true);

      // this function will delete all non-matching renderdoc.json values, and return true if our
      // own is registered
      bool thisRegistered = ProcessImplicitLayersKey(key, path, NULL, true);

      if(!thisRegistered)
        RegSetValueExW(key, path.c_str(), 0, REG_DWORD, (const BYTE *)&zero, sizeof(zero));

      RegCloseKey(key);
    }
  }
#endif
}
