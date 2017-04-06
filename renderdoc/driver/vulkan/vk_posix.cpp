/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

bool VulkanReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  VULKANNOTIMP("Optimisation missing - output window always returning true");

  return true;
}

bool WrappedVulkan::AddRequiredExtensions(bool instance, vector<string> &extensionList,
                                          const std::set<string> &supportedExtensions)
{
  bool device = !instance;

  if(instance)
  {
    // we must have VK_KHR_surface
    if(supportedExtensions.find(VK_KHR_SURFACE_EXTENSION_NAME) == supportedExtensions.end())
    {
      RDCERR("Unsupported required instance extension '%s'", VK_KHR_SURFACE_EXTENSION_NAME);
      return false;
    }

    // don't add duplicates
    if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_SURFACE_EXTENSION_NAME) ==
       extensionList.end())
      extensionList.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    bool oneSurfaceTypeSupported = false;

#if defined(VK_USE_PLATFORM_XCB_KHR)
    // check if supported
    if(supportedExtensions.find(VK_KHR_XCB_SURFACE_EXTENSION_NAME) != supportedExtensions.end())
    {
      oneSurfaceTypeSupported = true;

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
      oneSurfaceTypeSupported = true;

      m_SupportedWindowSystems.push_back(WindowingSystem::Xlib);

      // don't add duplicates
      if(std::find(extensionList.begin(), extensionList.end(), VK_KHR_XLIB_SURFACE_EXTENSION_NAME) ==
         extensionList.end())
      {
        extensionList.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
      }
    }
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    // must be supported
    RDCASSERT(supportedExtensions.find(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) !=
              supportedExtensions.end());

    oneSurfaceTypeSupported = true;
    m_SupportedWindowSystems.push_back(WindowingSystem::Android);

    // don't add duplicates, application will have added this but just be sure
    if(std::find(extensionList.begin(), extensionList.end(),
                 VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == extensionList.end())
    {
      extensionList.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    }
#endif

    if(!oneSurfaceTypeSupported)
    {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)

      RDCERR("Require the '%s' extension to be present", VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
      return false;

#elif defined(VK_USE_PLATFORM_XCB_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR)

      RDCERR("Require either the '%s' or '%s' extension to be present",
             VK_KHR_XCB_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
      return false;

#else

      // No windowing system support compiled in - allow this to continue,
      // but this will only work for headless replay (which is feasible on some platforms)
      return true;

#endif
    }
  }
  else if(device)
  {
    if(supportedExtensions.find(VK_KHR_SWAPCHAIN_EXTENSION_NAME) == supportedExtensions.end())
    {
      RDCERR("Unsupported required device extension '%s'", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      return false;
    }

    extensionList.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  return true;
}

#if defined(VK_USE_PLATFORM_XCB_KHR)

VkBool32 WrappedVulkan::vkGetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                                     uint32_t queueFamilyIndex,
                                                                     xcb_connection_t *connection,
                                                                     xcb_visualid_t visual_id)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceXcbPresentationSupportKHR(Unwrap(physicalDevice), queueFamilyIndex,
                                                   connection, visual_id);
}

namespace Keyboard
{
void UseConnection(xcb_connection_t *conn);
void CloneDisplay(Display *dpy);
}

VkResult WrappedVulkan::vkCreateXcbSurfaceKHR(VkInstance instance,
                                              const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(m_State >= WRITING);

  VkResult ret =
      ObjDisp(instance)->CreateXcbSurfaceKHR(Unwrap(instance), pCreateInfo, pAllocator, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    // since there's no point in allocating a full resource record and storing the window
    // handle under there somewhere, we just cast. We won't use the resource record for anything
    wrapped->record = (VkResourceRecord *)(uintptr_t)pCreateInfo->window;

    Keyboard::UseConnection(pCreateInfo->connection);
  }

  return ret;
}

#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)

VkBool32 WrappedVulkan::vkGetPhysicalDeviceXlibPresentationSupportKHR(
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, Display *dpy, VisualID visualID)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceXlibPresentationSupportKHR(Unwrap(physicalDevice), queueFamilyIndex, dpy,
                                                    visualID);
}

VkResult WrappedVulkan::vkCreateXlibSurfaceKHR(VkInstance instance,
                                               const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(m_State >= WRITING);

  VkResult ret =
      ObjDisp(instance)->CreateXlibSurfaceKHR(Unwrap(instance), pCreateInfo, pAllocator, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    // since there's no point in allocating a full resource record and storing the window
    // handle under there somewhere, we just cast. We won't use the resource record for anything
    wrapped->record = (VkResourceRecord *)pCreateInfo->window;

    Keyboard::CloneDisplay(pCreateInfo->dpy);
  }

  return ret;
}

VkResult WrappedVulkan::vkAcquireXlibDisplayEXT(VkPhysicalDevice physicalDevice, Display *dpy,
                                                VkDisplayKHR display)
{
  // display is not wrapped so we can pass straight through
  return ObjDisp(physicalDevice)->AcquireXlibDisplayEXT(Unwrap(physicalDevice), dpy, display);
}

VkResult WrappedVulkan::vkGetRandROutputDisplayEXT(VkPhysicalDevice physicalDevice, Display *dpy,
                                                   RROutput rrOutput, VkDisplayKHR *pDisplay)
{
  // display is not wrapped so we can pass straight through
  return ObjDisp(physicalDevice)
      ->GetRandROutputDisplayEXT(Unwrap(physicalDevice), dpy, rrOutput, pDisplay);
}

#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
VkResult WrappedVulkan::vkCreateAndroidSurfaceKHR(VkInstance instance,
                                                  const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(m_State >= WRITING);

  VkResult ret = ObjDisp(instance)->CreateAndroidSurfaceKHR(Unwrap(instance), pCreateInfo,
                                                            pAllocator, pSurface);

  if(ret == VK_SUCCESS)
  {
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    // since there's no point in allocating a full resource record and storing the window
    // handle under there somewhere, we just cast. We won't use the resource record for anything
    wrapped->record = (VkResourceRecord *)(uintptr_t)pCreateInfo->window;
  }

  return ret;
}
#endif
