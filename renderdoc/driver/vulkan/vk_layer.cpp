/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include <stdlib.h>
#include <string.h>

#include "api/replay/version.h"
#include "common/common.h"
#include "common/threading.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"
#include "vk_common.h"
#include "vk_core.h"
#include "vk_hookset_defs.h"
#include "vk_resources.h"

// this should be in the vulkan definition header
#if ENABLED(RDOC_WIN32)
#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#endif

// we don't actually hook any modules here. This is just used so that it's called
// at the right time in initialisation (after capture options are available) to
// set environment variables
class VulkanHook : LibraryHook
{
  VulkanHook() {}
  void RegisterHooks()
  {
    RDCLOG("Registering Vulkan hooks");

    // we don't register any library or function hooks because we use the layer system

    // we assume the implicit layer is registered - the UI will prompt the user about installing it.
    Process::RegisterEnvironmentModification(EnvironmentModification(
        EnvMod::Set, EnvSep::NoSep, "ENABLE_VULKAN_RENDERDOC_CAPTURE", "1"));

    // check options to set further variables, and apply
    OptionsUpdated();
  }

  void OptionsUpdated()
  {
    if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      Process::RegisterEnvironmentModification(
          EnvironmentModification(EnvMod::Append, EnvSep::Platform, "VK_INSTANCE_LAYERS",
                                  "VK_LAYER_LUNARG_standard_validation"));
      Process::RegisterEnvironmentModification(
          EnvironmentModification(EnvMod::Append, EnvSep::Platform, "VK_DEVICE_LAYERS",
                                  "VK_LAYER_LUNARG_standard_validation"));
    }
    else
    {
      // can't disable if APIValidation is not set
    }

    Process::ApplyEnvironmentModification();
  }

  static VulkanHook vkhooks;
};

VulkanHook VulkanHook::vkhooks;

// RenderDoc State

// RenderDoc Intercepts, these must all be entry points with a dispatchable object
// as the first parameter

#define HookDefine1(ret, function, t1, p1) \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1) { return CoreDisp(p1)->function(p1); }
#define HookDefine2(ret, function, t1, p1, t2, p2)                  \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2) \
  {                                                                 \
    return CoreDisp(p1)->function(p1, p2);                          \
  }
#define HookDefine3(ret, function, t1, p1, t2, p2, t3, p3)                 \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3) \
  {                                                                        \
    return CoreDisp(p1)->function(p1, p2, p3);                             \
  }
#define HookDefine4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4)                \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4) \
  {                                                                               \
    return CoreDisp(p1)->function(p1, p2, p3, p4);                                \
  }
#define HookDefine5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5)               \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
  {                                                                                      \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5);                                   \
  }
#define HookDefine6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6)              \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
  {                                                                                             \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6);                                      \
  }
#define HookDefine7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7)      \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, \
                                                      t7 p7)                                    \
  {                                                                                             \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7);                                  \
  }
#define HookDefine8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6,    \
                                                      t7 p7, t8 p8)                                \
  {                                                                                                \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7, p8);                                 \
  }
#define HookDefine9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, \
                    t9, p9)                                                                        \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6,    \
                                                      t7 p7, t8 p8, t9, p9)                        \
  {                                                                                                \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7, p8, p9);                             \
  }
#define HookDefine10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, \
                     p8, t9, p9, t10, p10)                                                      \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, \
                                                      t7 p7, t8 p8, t9 p9, t10 p10)             \
  {                                                                                             \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);                     \
  }
#define HookDefine11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, \
                     p8, t9, p9, t10, p10, t11, p11)                                            \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, \
                                                      t7 p7, t8 p8, t9 p9, t10 p10, t11 p11)    \
  {                                                                                             \
    return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);                \
  }

DefineHooks();

// need to implement vkCreateInstance and vkDestroyInstance specially,
// to create and destroy the core WrappedVulkan object

VKAPI_ATTR VkResult VKAPI_CALL hooked_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkInstance *pInstance)
{
  WrappedVulkan *core = new WrappedVulkan();
  return core->vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

VKAPI_ATTR void VKAPI_CALL hooked_vkDestroyInstance(VkInstance instance,
                                                    const VkAllocationCallbacks *pAllocator)
{
  WrappedVulkan *core = CoreDisp(instance);
  core->vkDestroyInstance(instance, pAllocator);
  delete core;
}

// Layer Intercepts

#if ENABLED(RDOC_WIN32) && DISABLED(RDOC_X64)

// Win32 __stdcall will still mangle even with extern "C", set up aliases

#pragma comment( \
    linker,      \
    "/EXPORT:VK_LAYER_RENDERDOC_CaptureEnumerateDeviceLayerProperties=_VK_LAYER_RENDERDOC_CaptureEnumerateDeviceLayerProperties@12")
#pragma comment( \
    linker,      \
    "/EXPORT:VK_LAYER_RENDERDOC_CaptureEnumerateDeviceExtensionProperties=_VK_LAYER_RENDERDOC_CaptureEnumerateDeviceExtensionProperties@16")
#pragma comment( \
    linker,      \
    "/EXPORT:VK_LAYER_RENDERDOC_CaptureEnumerateInstanceExtensionProperties=_VK_LAYER_RENDERDOC_CaptureEnumerateInstanceExtensionProperties@16")
#pragma comment( \
    linker,      \
    "/EXPORT:VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr=_VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr@8")
#pragma comment( \
    linker,      \
    "/EXPORT:VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr=_VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr@8")
#endif

extern "C" {

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL VK_LAYER_RENDERDOC_CaptureEnumerateDeviceLayerProperties(
    VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
  // must have a property count, either to fill out or use as a size
  if(pPropertyCount == NULL)
    return VK_INCOMPLETE;

  // if we're not writing the properties, just say we have one layer
  if(pProperties == NULL)
  {
    *pPropertyCount = 1;
    return VK_SUCCESS;
  }
  else
  {
    // if the property count is somehow zero, return incomplete
    if(*pPropertyCount == 0)
      return VK_INCOMPLETE;

    const VkLayerProperties layerProperties = {
        RENDERDOC_VULKAN_LAYER_NAME, VK_API_VERSION_1_0,
        VK_MAKE_VERSION(RENDERDOC_VERSION_MAJOR, RENDERDOC_VERSION_MINOR, 0),
        "Debugging capture layer for RenderDoc",
    };

    // set the one layer property
    *pProperties = layerProperties;

    return VK_SUCCESS;
  }
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
VK_LAYER_RENDERDOC_CaptureEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                             const char *pLayerName,
                                                             uint32_t *pPropertyCount,
                                                             VkExtensionProperties *pProperties)
{
  // if pLayerName is NULL or not ours we're calling down through the layer chain to the ICD.
  // This is our chance to filter out any reported extensions that we don't support
  if(physicalDevice != NULL && (pLayerName == NULL || strcmp(pLayerName, RENDERDOC_VULKAN_LAYER_NAME)))
    return CoreDisp(physicalDevice)
        ->FilterDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);

  return WrappedVulkan::GetProvidedDeviceExtensionProperties(pPropertyCount, pProperties);
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
VK_LAYER_RENDERDOC_CaptureEnumerateInstanceExtensionProperties(
    const VkEnumerateInstanceExtensionPropertiesChain *pChain, const char *pLayerName,
    uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
  if(pLayerName && !strcmp(pLayerName, RENDERDOC_VULKAN_LAYER_NAME))
    return WrappedVulkan::GetProvidedInstanceExtensionProperties(pPropertyCount, pProperties);

  return WrappedVulkan::FilterInstanceExtensionProperties(pChain, pLayerName, pPropertyCount,
                                                          pProperties);
}

#undef DeclExt
#define DeclExt(name) \
  bool name = false;  \
  (void)name;

#undef CheckExt
#define CheckExt(name, ver) name = instDevInfo == NULL || instDevInfo->ext_##name;

#undef HookInit
#define HookInit(function)                            \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) \
    return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function);

#undef HookInitExtension
#define HookInitExtension(cond, function)                      \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function))))          \
  {                                                            \
    if(cond)                                                   \
      return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function); \
  }

// for promoted extensions, we return the function pointer for either name as an alias.
#undef HookInitPromotedExtension
#define HookInitPromotedExtension(cond, function, suffix)             \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function))) ||               \
     !strcmp(pName, STRINGIZE(CONCAT(vk, CONCAT(function, suffix))))) \
  {                                                                   \
    if(cond)                                                          \
      return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function);        \
  }

// proc addr routines

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr(VkDevice device, const char *pName)
{
  if(!strcmp("vkGetDeviceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr;
  if(!strcmp("vkCreateDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkCreateDevice;
  if(!strcmp("vkDestroyDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkDestroyDevice;

  HookInitVulkanDevice();

  if(device == VK_NULL_HANDLE)
    return NULL;

  InstanceDeviceInfo *instDevInfo = GetRecord(device)->instDevInfo;

  DeclExts();

  CheckInstanceExts();
  CheckDeviceExts();

  HookInitVulkanDeviceExts();

  if(instDevInfo->brokenGetDeviceProcAddr)
  {
    HookInitVulkanInstanceExts();
  }

  if(GetDeviceDispatchTable(device)->GetDeviceProcAddr == NULL)
    return NULL;
  return GetDeviceDispatchTable(device)->GetDeviceProcAddr(Unwrap(device), pName);
}

VKAPI_ATTR VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr(VkInstance instance, const char *pName)
{
  if(instance == VK_NULL_HANDLE)
    return NULL;

  if(GetInstanceDispatchTable(instance)->GetInstanceProcAddr == NULL)
    return NULL;

  PFN_vkGetInstanceProcAddr GPDA =
      (PFN_vkGetInstanceProcAddr)GetInstanceDispatchTable(instance)->GetInstanceProcAddr(
          Unwrap(instance), "vk_layerGetPhysicalDeviceProcAddr");

  if(GPDA == NULL)
    return NULL;

  return GPDA(Unwrap(instance), pName);
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr(VkInstance instance, const char *pName)
{
  if(!strcmp("vkGetInstanceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr;
  if(!strcmp("vk_layerGetPhysicalDeviceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr;
  if(!strcmp("vkEnumerateDeviceLayerProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateDeviceLayerProperties;
  if(!strcmp("vkEnumerateDeviceExtensionProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateDeviceExtensionProperties;
  if(!strcmp("vkEnumerateInstanceExtensionProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateInstanceExtensionProperties;
  if(!strcmp("vkGetDeviceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr;
  if(!strcmp("vkCreateDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkCreateDevice;
  if(!strcmp("vkDestroyDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkDestroyDevice;

  HookInitVulkanInstance();

  if(instance == VK_NULL_HANDLE)
    return NULL;

  InstanceDeviceInfo *instDevInfo = NULL;

  if(WrappedVkInstance::IsAlloc(instance))
    instDevInfo = GetRecord(instance)->instDevInfo;
  else
    RDCERR(
        "GetInstanceProcAddr passed invalid instance for %s! Possibly broken loader. "
        "Working around by assuming all extensions are enabled - WILL CAUSE SPEC-BROKEN BEHAVIOUR",
        pName);

  DeclExts();

  CheckInstanceExts();
  CheckDeviceExts();

  HookInitVulkanInstanceExts();

// GetInstanceProcAddr must also unconditionally return all device functions

#undef HookInitExtension
#define HookInitExtension(cond, function)             \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) \
    return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function);

#undef HookInitPromotedExtension
#define HookInitPromotedExtension(cond, function, suffix)             \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function))) ||               \
     !strcmp(pName, STRINGIZE(CONCAT(vk, CONCAT(function, suffix))))) \
    return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function);

  HookInitVulkanDevice();

  HookInitVulkanDeviceExts();

  if(GetInstanceDispatchTable(instance)->GetInstanceProcAddr == NULL)
    return NULL;
  return GetInstanceDispatchTable(instance)->GetInstanceProcAddr(Unwrap(instance), pName);
}

// layer interface negotation (new interface)
VK_LAYER_EXPORT VKAPI_ATTR VkResult VK_LAYER_RENDERDOC_CaptureNegotiateLoaderLayerInterfaceVersion(
    VkNegotiateLayerInterface *pVersionStruct)
{
  if(pVersionStruct->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
    return VK_ERROR_INITIALIZATION_FAILED;

  if(pVersionStruct->loaderLayerInterfaceVersion >= 2)
  {
    pVersionStruct->pfnGetInstanceProcAddr = VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr;
    pVersionStruct->pfnGetDeviceProcAddr = VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr;
    pVersionStruct->pfnGetPhysicalDeviceProcAddr =
        VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr;
  }

  // we only support the current version. Don't let updating the header silently make us report a
  // higher version without examining what this means
  RDCCOMPILE_ASSERT(CURRENT_LOADER_LAYER_INTERFACE_VERSION == 2,
                    "Loader/layer interface version has been bumped");

  if(pVersionStruct->loaderLayerInterfaceVersion > 2)
    pVersionStruct->loaderLayerInterfaceVersion = 2;

  return VK_SUCCESS;
}
}
