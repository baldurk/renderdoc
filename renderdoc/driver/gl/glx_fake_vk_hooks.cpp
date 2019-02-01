/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include <dlfcn.h>
#include <stddef.h>
#include "common/common.h"
#include "hooks/hooks.h"

class FakeVkHook : LibraryHook
{
public:
  void RegisterHooks()
  {
    LibraryHooks::RegisterLibraryHook("libGL.so", &FakeVkHooked);
    LibraryHooks::RegisterLibraryHook("libGL.so.1", &FakeVkHooked);
  }

  static void FakeVkHooked(void *handle) { searchHandle = handle; }
  static void *searchHandle;
} fakevkhook;

void *FakeVkHook::searchHandle = RTLD_NEXT;

extern "C" {

// because we intercept all dlopen calls to "libGL.so*" to ourselves, we can interfere with some
// very poorly configured vulkan ICDs. For some reason they point the vulkan ICD to libGL.so, and so
// the vulkan loader tries to get the bootstrap entry points from our library after the redirect. I
// think this is a distribution thing and is not true in the official nvidia package, I'm not sure.
//
// Unfortunately there's no perfect way to fix this, since if we declare a function the ICD doesn't
// support we're screwed as dlsym will find ours but we'll have nothing to call onwards to. We just
// have to hope the ICD exports all these functions that we can forward on to.

// declare minimal typedefs to get by
typedef void *VkInstance;
enum VkResult
{
  VK_ERROR_INCOMPATIBLE_DRIVER = -9
};
struct VkNegotiateLayerInterface;

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *pName);
typedef PFN_vkVoidFunction (*PFN_GetPhysicalDeviceProcAddr)(VkInstance instance, const char *pName);
typedef VkResult (*PFN_vkNegotiateLoaderLayerInterfaceVersion)(VkNegotiateLayerInterface *pVersionStruct);

__attribute__((visibility("default"))) PFN_vkVoidFunction vk_icdGetInstanceProcAddr(
    VkInstance instance, const char *pName)
{
  PFN_vkGetInstanceProcAddr real =
      (PFN_vkGetInstanceProcAddr)dlsym(fakevkhook.searchHandle, "vk_icdGetInstanceProcAddr");

  if(!real)
    real = (PFN_vkGetInstanceProcAddr)dlsym(RTLD_NEXT, "vk_icdGetInstanceProcAddr");

  if(real)
    return real(instance, pName);

  RDCERR("Couldn't get real vk_icdGetInstanceProcAddr!");

  return NULL;
}

__attribute__((visibility("default"))) PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(
    VkInstance instance, const char *pName)
{
  PFN_GetPhysicalDeviceProcAddr real = (PFN_GetPhysicalDeviceProcAddr)dlsym(
      fakevkhook.searchHandle, "vk_icdGetPhysicalDeviceProcAddr");

  if(!real)
    real = (PFN_GetPhysicalDeviceProcAddr)dlsym(RTLD_NEXT, "vk_icdGetPhysicalDeviceProcAddr");

  if(real)
    return real(instance, pName);

  RDCERR("Couldn't get real vk_icdGetPhysicalDeviceProcAddr!");

  return NULL;
}

__attribute__((visibility("default"))) VkResult vk_icdNegotiateLoaderLayerInterfaceVersion(
    VkNegotiateLayerInterface *pVersionStruct)
{
  PFN_vkNegotiateLoaderLayerInterfaceVersion real = (PFN_vkNegotiateLoaderLayerInterfaceVersion)dlsym(
      fakevkhook.searchHandle, "vk_icdNegotiateLoaderLayerInterfaceVersion");

  if(!real)
    real = (PFN_vkNegotiateLoaderLayerInterfaceVersion)dlsym(
        RTLD_NEXT, "vk_icdNegotiateLoaderLayerInterfaceVersion");

  if(real)
    return real(pVersionStruct);

  RDCERR("Couldn't get real vk_icdNegotiateLoaderLayerInterfaceVersion!");

  return VK_ERROR_INCOMPATIBLE_DRIVER;
}

};    // extern "C"
