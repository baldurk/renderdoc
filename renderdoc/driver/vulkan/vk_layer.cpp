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

extern "C" const rdcstr VulkanLayerJSONBasename;

// this was removed from the vulkan definition header
#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT
#if ENABLED(RDOC_WIN32)

#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)

#elif ENABLED(RDOC_LINUX) || ENABLED(RDOC_ANDROID)

#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT __attribute__((visibility("default")))

#endif

#if ENABLED(RDOC_ANDROID)
#include <dlfcn.h>

void KeepLayerAlive()
{
  static bool done = false;
  if(done)
    return;
  done = true;

  // on Android 10 the library only gets loaded for layers. If an instance is destroyed the library
  // would be unloaded. That could cause us to drop target control connections etc.
  // we create our own instance, which increases the refcount on the layer, then leak it to prevent
  // the layer being unloaded.
  RDCLOG("Creating internal instance to bump layer refcount");
  void *module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if(!module)
    module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);

  if(module)
  {
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)dlsym(module, "vkCreateInstance");
    VkApplicationInfo app = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
        "RenderDoc forced instance",        VK_MAKE_VERSION(1, 0, 0),
        "RenderDoc forced instance",        VK_MAKE_VERSION(1, 0, 0),
        VK_MAKE_VERSION(1, 0, 0),
    };
    VkInstanceCreateInfo info = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL,
    };
    VkInstance forceLiveInstance = VK_NULL_HANDLE;
    VkResult vkr = create(&info, NULL, &forceLiveInstance);
    RDCLOG("Created own instance %p: %s", forceLiveInstance, ToStr(vkr).c_str());
  }
  else
  {
    RDCERR("Couldn't load libvulkan - can't force layer to stay alive");
  }
}
#else
void KeepLayerAlive()
{
}
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
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, RENDERDOC_VULKAN_LAYER_VAR, "1"));

    // RTSS layer is buggy, disable it to avoid bug reports that are caused by it
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_RTSS_LAYER", "1"));

    // OBS's layer causes crashes, disable it too.
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_VULKAN_OBS_CAPTURE", "1"));

    // OverWolf is some shitty software that forked OBS and changed the layer value
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_VULKAN_OW_OBS_CAPTURE", "1"));

    // buggy program AgaueEye which also doesn't have a proper layer configuration. As a result
    // this is likely to have side-effects but probably also on other buggy layers that duplicate
    // sample code without even changing the layer json
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_SAMPLE_LAYER", "1"));

    // buggy overlay gamepp
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_GAMEPP_LAYER", "1"));

    // mesa device select layer crashes when it calls GPDP2 inside vkCreateInstance, which fails on
    // the current loader.
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "NODEVICE_SELECT", "1"));

    Process::RegisterEnvironmentModification(EnvironmentModification(
        EnvMod::Set, EnvSep::NoSep, "DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1", "1"));

    Process::RegisterEnvironmentModification(EnvironmentModification(
        EnvMod::Set, EnvSep::NoSep, "VK_LAYER_bandicam_helper_DEBUG_1", "1"));

    // fpsmon not only has a buggy layer but it also picks an absurdly generic disable environment
    // variable :(. Hopefully no other program picks this, or if it does then it's probably not a
    // bad thing to disable too
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_LAYER", "1"));

    // support self-hosted capture by checking our filename and tweaking the env var we set
    if(VulkanLayerJSONBasename != "renderdoc")
    {
      Process::RegisterEnvironmentModification(EnvironmentModification(
          EnvMod::Set, EnvSep::NoSep,
          "ENABLE_VULKAN_" + strupper(VulkanLayerJSONBasename) + "_CAPTURE", "1"));
    }

    // check options to set further variables, and apply
    OptionsUpdated();
  }

  void RemoveHooks()
  {
    // unset the vulkan layer environment variable
    Process::RegisterEnvironmentModification(
        EnvironmentModification(EnvMod::Set, EnvSep::NoSep, RENDERDOC_VULKAN_LAYER_VAR, "0"));
    Process::ApplyEnvironmentModification();
  }

  void OptionsUpdated()
  {
    if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
    {
      Process::RegisterEnvironmentModification(EnvironmentModification(
          EnvMod::Append, EnvSep::Platform, "VK_INSTANCE_LAYERS", "VK_LAYER_KHRONOS_validation"));
      Process::RegisterEnvironmentModification(EnvironmentModification(
          EnvMod::Append, EnvSep::Platform, "VK_DEVICE_LAYERS", "VK_LAYER_KHRONOS_validation"));
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

#define HookDefine1(ret, function, t1, p1)                   \
  VKAPI_ATTR ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1) \
  {                                                          \
    return CoreDisp(p1)->function(p1);                       \
  }
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
                                                       const VkAllocationCallbacks *,
                                                       VkInstance *pInstance)
{
  KeepLayerAlive();

  WrappedVulkan *core = new WrappedVulkan();
  return core->vkCreateInstance(pCreateInfo, NULL, pInstance);
}

VKAPI_ATTR void VKAPI_CALL hooked_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *)
{
  WrappedVulkan *core = CoreDisp(instance);
  core->vkDestroyInstance(instance, NULL);
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
        RENDERDOC_VULKAN_LAYER_NAME,
        VK_API_VERSION_1_0,
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
  if(physicalDevice != NULL &&
     (pLayerName == NULL || strcmp(pLayerName, RENDERDOC_VULKAN_LAYER_NAME) != 0))
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
#define HookInitPromotedExtension(cond, function, suffix)                     \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function))) ||                       \
             !strcmp(pName, STRINGIZE(CONCAT(vk, CONCAT(function, suffix))))) \
  {                                                                           \
    if(cond)                                                                  \
      return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function);                \
  }

#undef HookInitExtensionEXTtoKHR
#define HookInitExtensionEXTtoKHR(func) (void)0;

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
    HookInitVulkanInstanceExts_PhysDev();
    HookInitVulkanInstanceExts();
  }

  // unknown or not-enabled functions must return NULL
  return NULL;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr(VkInstance instance, const char *pName);

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr(VkInstance instance, const char *pName)
{
  // if name is NULL undefined is returned, let's return NULL
  if(pName == NULL)
    return NULL;

  // a NULL instance can return vkGetInstanceProcAddr or a global function, handle that here

  if(!strcmp("vkGetInstanceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureGetInstanceProcAddr;
  if(!strcmp("vkEnumerateInstanceExtensionProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateInstanceExtensionProperties;
  if(!strcmp("vk_layerGetPhysicalDeviceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr;

  // don't implement vkEnumerateInstanceLayerProperties or vkEnumerateInstanceVersion, the loader
  // will do that

  HookInit(CreateInstance);

  if(instance == VK_NULL_HANDLE)
    return NULL;

  if(!strcmp("vkEnumerateDeviceLayerProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateDeviceLayerProperties;
  if(!strcmp("vkEnumerateDeviceExtensionProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateDeviceExtensionProperties;
  if(!strcmp("vkGetDeviceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureGetDeviceProcAddr;
  if(!strcmp("vkCreateDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkCreateDevice;
  if(!strcmp("vkDestroyDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkDestroyDevice;

  // we should only return a function pointer for functions that are either from a supported core
  // version, an enabled instance extension or an _available_ device extension

  HookInitVulkanInstance();

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
#define HookInitPromotedExtension(cond, function, suffix)                     \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function))) ||                       \
             !strcmp(pName, STRINGIZE(CONCAT(vk, CONCAT(function, suffix))))) \
    return (PFN_vkVoidFunction)&CONCAT(hooked_vk, function);

#undef HookInitExtensionEXTtoKHR
#define HookInitExtensionEXTtoKHR(func) (void)0;

  HookInitVulkanDevice();

  HookInitVulkanDeviceExts();

  HookInitVulkanInstanceExts_PhysDev();

  // all other functions must return NULL so that GIPA can be used with NULL checks sensibly for
  // missing functionality

  return NULL;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr(VkInstance instance, const char *pName)
{
  // GetPhysicalDeviceProcAddr acts like GetInstanceProcAddr but it returns NULL for any functions
  // which are known but aren't physical device functions
  if(!strcmp("vkGetInstanceProcAddr", pName))
    return NULL;
  if(!strcmp("vk_layerGetPhysicalDeviceProcAddr", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_Capture_layerGetPhysicalDeviceProcAddr;
  if(!strcmp("vkEnumerateDeviceLayerProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateDeviceLayerProperties;
  if(!strcmp("vkEnumerateDeviceExtensionProperties", pName))
    return (PFN_vkVoidFunction)&VK_LAYER_RENDERDOC_CaptureEnumerateDeviceExtensionProperties;
  if(!strcmp("vkEnumerateInstanceExtensionProperties", pName))
    return NULL;
  if(!strcmp("vkGetDeviceProcAddr", pName))
    return NULL;
  if(!strcmp("vkCreateDevice", pName))
    return (PFN_vkVoidFunction)&hooked_vkCreateDevice;
  if(!strcmp("vkDestroyDevice", pName))
    return NULL;

  HookInitVulkanInstance_PhysDev();

// any remaining functions that are known, we must return NULL for
#undef HookInit
#define HookInit(function)                            \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) \
    return NULL;

  // any extensions that are known to be physical device functions, return here
  HookInitVulkanInstanceExts_PhysDev();

  HookInitVulkanInstance();
  HookInitVulkanDevice();

  if(instance == VK_NULL_HANDLE)
    return NULL;

  InstanceDeviceInfo *instDevInfo = NULL;

  if(WrappedVkInstance::IsAlloc(instance))
    instDevInfo = GetRecord(instance)->instDevInfo;
  else
    RDCERR(
        "GetPhysicalDeviceProcAddr passed invalid instance for %s! Possibly broken loader. "
        "Working around by assuming all extensions are enabled - WILL CAUSE SPEC-BROKEN BEHAVIOUR",
        pName);

  DeclExts();

  CheckInstanceExts();
  CheckDeviceExts();

// any remaining functions that are known, we must return NULL for
#undef HookInitExtension
#define HookInitExtension(cond, function)             \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) \
    return NULL;

#undef HookInitPromotedExtension
#define HookInitPromotedExtension(cond, function, suffix)                     \
  if(!strcmp(pName, STRINGIZE(CONCAT(vk, function))) ||                       \
             !strcmp(pName, STRINGIZE(CONCAT(vk, CONCAT(function, suffix))))) \
    return NULL;

#undef HookInitExtensionEXTtoKHR
#define HookInitExtensionEXTtoKHR(func) (void)0;

  HookInitVulkanInstanceExts();
  HookInitVulkanDeviceExts();

  // if we got here we don't recognise the function at all. Shouldn't be possible as we whitelist
  // extensions, but follow the spec and pass along

  if(GetInstanceDispatchTable(instance)->GetInstanceProcAddr == NULL)
    return NULL;

  PFN_vkGetInstanceProcAddr GPDPA =
      (PFN_vkGetInstanceProcAddr)GetInstanceDispatchTable(instance)->GetInstanceProcAddr(
          Unwrap(instance), "vk_layerGetPhysicalDeviceProcAddr");

  if(GPDPA == NULL)
    return NULL;

  return GPDPA(Unwrap(instance), pName);
}

// layer interface negotation (new interface)
VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
VK_LAYER_RENDERDOC_CaptureNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *pVersionStruct)
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
