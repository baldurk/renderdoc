/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include "../vk_core.h"
#include "../vk_debug.h"
#include "../vk_rendertext.h"
#include "../vk_replay.h"
#include "../vk_shader_cache.h"
#include "api/replay/version.h"
#include "core/settings.h"
#include "strings/string_utils.h"

RDOC_CONFIG(
    bool, Vulkan_Debug_ReplaceAppInfo, true,
    "By default we have no choice but to replace VkApplicationInfo to safely work on all drivers. "
    "This behaviour can be disabled with this flag, which lets it through both during capture and "
    "on replay.");

// intercept and overwrite the application info if present. We must use the same appinfo on
// capture and replay, and the safer default is not to replay as if we were the original app but
// with a slightly different workload. So instead we trample what the app reported and put in our
// own info.
static VkApplicationInfo renderdocAppInfo = {
    VK_STRUCTURE_TYPE_APPLICATION_INFO,
    NULL,
    "RenderDoc Capturing App",
    VK_MAKE_VERSION(RENDERDOC_VERSION_MAJOR, RENDERDOC_VERSION_MINOR, 0),
    "RenderDoc",
    VK_MAKE_VERSION(RENDERDOC_VERSION_MAJOR, RENDERDOC_VERSION_MINOR, 0),
    VK_API_VERSION_1_0,
};

static bool equivalent(const VkQueueFamilyProperties &a, const VkQueueFamilyProperties &b)
{
  return a.timestampValidBits == b.timestampValidBits &&
         a.minImageTransferGranularity.width == b.minImageTransferGranularity.width &&
         a.minImageTransferGranularity.height == b.minImageTransferGranularity.height &&
         a.minImageTransferGranularity.depth == b.minImageTransferGranularity.depth &&
         a.queueFlags == b.queueFlags;
}

// we store the index in the loader table, since it won't be dereferenced and other parts of the
// code expect to copy it into a wrapped object
static VkPhysicalDevice MakePhysicalDeviceHandleFromIndex(uint32_t physDeviceIndex)
{
  static uintptr_t loaderTable[32];
  loaderTable[physDeviceIndex] = (0x100 + physDeviceIndex);
  return VkPhysicalDevice(&loaderTable[physDeviceIndex]);
}

static uint32_t GetPhysicalDeviceIndexFromHandle(VkPhysicalDevice physicalDevice)
{
  return uint32_t((uintptr_t)LayerDisp(physicalDevice) - 0x100);
}

static bool CheckTransferGranularity(VkExtent3D required, VkExtent3D check)
{
  // if the required granularity is (0,0,0) then any is fine - the requirement is always satisfied.
  if(required.width == required.height && required.height == required.depth && required.depth == 0)
    return true;

  // otherwise, each dimension must be <= the required dimension (i.e. more fine-grained) to support
  // any copies we might do.
  return check.width <= required.width && check.height <= required.height &&
         check.depth <= required.depth;
}

// vk_dispatchtables.cpp
void InitDeviceTable(VkDevice dev, PFN_vkGetDeviceProcAddr gpa);
void InitInstanceTable(VkInstance inst, PFN_vkGetInstanceProcAddr gpa);

// Init/shutdown order:
//
// On capture, WrappedVulkan is new'd and delete'd before vkCreateInstance() and after
// vkDestroyInstance()
// On replay,  WrappedVulkan is new'd and delete'd before Initialise()       and after Shutdown()
//
// The class constructor and destructor handle only *non-API* work. All API objects must be created
// and
// torn down in the latter functions (vkCreateInstance/vkDestroyInstance during capture, and
// Initialise/Shutdown during replay).
//
// Note that during capture we have vkDestroyDevice before vkDestroyDevice that does most of the
// work.
//
// Also we assume correctness from the application, that all objects are destroyed before the device
// and
// instance are destroyed. We only clean up after our own objects.

static void StripUnwantedLayers(rdcarray<rdcstr> &Layers)
{
  Layers.removeIf([](const rdcstr &layer) {
    // don't try and create our own layer on replay!
    if(layer == RENDERDOC_VULKAN_LAYER_NAME)
    {
      return true;
    }

    // don't enable tracing or dumping layers just in case they
    // came along with the application
    if(layer == "VK_LAYER_LUNARG_api_dump" || layer == "VK_LAYER_LUNARG_vktrace")
    {
      return true;
    }

    // also remove the framerate monitor layer as it's buggy and doesn't do anything
    // in our case
    if(layer == "VK_LAYER_LUNARG_monitor")
    {
      return true;
    }

    // remove the optimus layer just in case it was explicitly enabled.
    if(layer == "VK_LAYER_NV_optimus")
    {
      return true;
    }

    // filter out validation layers
    if(layer == "VK_LAYER_LUNARG_standard_validation" || layer == "VK_LAYER_KHRONOS_validation" ||
       layer == "VK_LAYER_LUNARG_core_validation" || layer == "VK_LAYER_LUNARG_device_limits" ||
       layer == "VK_LAYER_LUNARG_image" || layer == "VK_LAYER_LUNARG_object_tracker" ||
       layer == "VK_LAYER_LUNARG_parameter_validation" || layer == "VK_LAYER_LUNARG_swapchain" ||
       layer == "VK_LAYER_GOOGLE_threading" || layer == "VK_LAYER_GOOGLE_unique_objects" ||
       layer == "VK_LAYER_LUNARG_assistant_layer")
    {
      return true;
    }

    return false;
  });
}

static void StripUnwantedExtensions(rdcarray<rdcstr> &Extensions)
{
  // strip out any WSI/direct display extensions. We'll add the ones we want for creating windows
  // on the current platforms below, and we don't replay any of the WSI functionality
  // directly so these extensions aren't needed
  Extensions.removeIf([](const rdcstr &ext) {
    // remove surface extensions
    if(ext == "VK_KHR_xlib_surface" || ext == "VK_KHR_xcb_surface" ||
       ext == "VK_KHR_wayland_surface" || ext == "VK_KHR_mir_surface" ||
       ext == "VK_MVK_macos_surface" || ext == "VK_KHR_android_surface" ||
       ext == "VK_KHR_win32_surface" || ext == "VK_GGP_stream_descriptor_surface" ||
       ext == "VK_GGP_frame_token")
    {
      return true;
    }

    // remove direct display extensions
    if(ext == "VK_KHR_display" || ext == "VK_EXT_direct_mode_display" ||
       ext == "VK_EXT_acquire_xlib_display" || ext == "VK_EXT_display_surface_counter" ||
       ext == "VK_EXT_acquire_drm_display")
    {
      return true;
    }

    // remove platform-specific external extensions, as we don't replay external objects. We leave
    // the base extensions since they're widely supported and we don't strip all uses of e.g.
    // feature structs.
    if(ext == "VK_KHR_external_fence_fd" || ext == "VK_KHR_external_fence_win32" ||
       ext == "VK_KHR_external_memory_fd" || ext == "VK_KHR_external_memory_win32" ||
       ext == "VK_KHR_external_semaphore_fd" || ext == "VK_KHR_external_semaphore_win32" ||
       ext == "VK_KHR_win32_keyed_mutex")
    {
      return true;
    }

    // remove WSI-only extensions
    if(ext == "VK_GOOGLE_display_timing" || ext == "VK_KHR_display_swapchain" ||
       ext == "VK_EXT_display_control" || ext == "VK_KHR_present_id" ||
       ext == "VK_KHR_present_wait" || ext == "VK_EXT_surface_maintenance1" ||
       ext == "VK_EXT_swapchain_maintenance1")
      return true;

    // remove fullscreen exclusive extension
    if(ext == "VK_EXT_full_screen_exclusive")
      return true;

    // this is debug only, nothing to capture, so nothing to replay
    if(ext == "VK_EXT_tooling_info" || ext == "VK_EXT_private_data" ||
       ext == "VK_EXT_validation_features" || ext == "VK_EXT_validation_cache" ||
       ext == "VK_EXT_validation_flags")
      return true;

    // these are debug only and will be added (if supported) as optional
    if(ext == "VK_EXT_debug_utils" || ext == "VK_EXT_debug_marker")
      return true;

    return false;
  });
}

RDResult WrappedVulkan::Initialise(VkInitParams &params, uint64_t sectionVersion,
                                   const ReplayOptions &opts)
{
  m_InitParams = params;
  m_SectionVersion = sectionVersion;
  m_ReplayOptions = opts;

  m_ResourceManager->SetOptimisationLevel(m_ReplayOptions.optimisation);

  StripUnwantedLayers(params.Layers);
  StripUnwantedExtensions(params.Extensions);

  std::set<rdcstr> supportedLayers;

  {
    uint32_t count = 0;
    GetInstanceDispatchTable(NULL)->EnumerateInstanceLayerProperties(&count, NULL);

    VkLayerProperties *props = new VkLayerProperties[count];
    GetInstanceDispatchTable(NULL)->EnumerateInstanceLayerProperties(&count, props);

    for(uint32_t e = 0; e < count; e++)
      supportedLayers.insert(props[e].layerName);

    SAFE_DELETE_ARRAY(props);
  }

  if(m_ReplayOptions.apiValidation)
  {
    const char KhronosValidation[] = "VK_LAYER_KHRONOS_validation";
    const char LunarGValidation[] = "VK_LAYER_LUNARG_standard_validation";

    if(supportedLayers.find(KhronosValidation) != supportedLayers.end())
    {
      RDCLOG("Enabling %s layer for API validation", KhronosValidation);
      params.Layers.push_back(KhronosValidation);
      m_LayersEnabled[VkCheckLayer_unique_objects] = true;
    }
    else if(supportedLayers.find(LunarGValidation) != supportedLayers.end())
    {
      RDCLOG("Enabling %s layer for API validation", LunarGValidation);
      params.Layers.push_back(LunarGValidation);
      m_LayersEnabled[VkCheckLayer_unique_objects] = true;
    }
    else
    {
      RDCLOG("API validation layers are not available, check you have the Vulkan SDK installed");
    }
  }

  // complain about any missing layers, but remove them from the list and continue
  params.Layers.removeIf([&supportedLayers](const rdcstr &layer) {
    if(supportedLayers.find(layer) == supportedLayers.end())
    {
      RDCERR("Capture used layer '%s' which is not available, continuing without it", layer.c_str());
      return true;
    }

    return false;
  });

  std::set<rdcstr> supportedExtensions;

  for(size_t i = 0; i <= params.Layers.size(); i++)
  {
    const char *pLayerName = (i == 0 ? NULL : params.Layers[i - 1].c_str());

    uint32_t count = 0;
    GetInstanceDispatchTable(NULL)->EnumerateInstanceExtensionProperties(pLayerName, &count, NULL);

    VkExtensionProperties *props = new VkExtensionProperties[count];
    GetInstanceDispatchTable(NULL)->EnumerateInstanceExtensionProperties(pLayerName, &count, props);

    for(uint32_t e = 0; e < count; e++)
      supportedExtensions.insert(props[e].extensionName);

    SAFE_DELETE_ARRAY(props);
  }

  if(!m_Replay->IsRemoteProxy())
  {
    size_t i = 0;
    for(const rdcstr &ext : supportedExtensions)
    {
      RDCLOG("Inst Ext %u: %s", i, ext.c_str());
      i++;
    }

    i = 0;
    for(const rdcstr &layer : supportedLayers)
    {
      RDCLOG("Inst Layer %u: %s", i, layer.c_str());
      i++;
    }
  }

  AddRequiredExtensions(true, params.Extensions, supportedExtensions);

  // after 1.0, VK_KHR_get_physical_device_properties2 is promoted to core, but enable it if it's
  // reported as available, just in case.
  if(params.APIVersion >= VK_API_VERSION_1_0)
  {
    if(supportedExtensions.find(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      if(!params.Extensions.contains(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        params.Extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
  }
  else
  {
    if(supportedExtensions.find(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) ==
       supportedExtensions.end())
    {
      RDCWARN("Unsupported required instance extension for AMD performance counters '%s'",
              VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
    else
    {
      if(!params.Extensions.contains(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        params.Extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
  }

  // verify that extensions are supported
  for(size_t i = 0; i < params.Extensions.size(); i++)
  {
    if(supportedExtensions.find(params.Extensions[i]) == supportedExtensions.end())
    {
      RETURN_ERROR_RESULT(ResultCode::APIHardwareUnsupported,
                          "Capture requires instance extension '%s' which is not supported\n",
                          params.Extensions[i].c_str());
    }
  }

  // we always want debug extensions if it available, and not already enabled
  if(supportedExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedExtensions.end() &&
     !params.Extensions.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
  {
    if(!m_Replay->IsRemoteProxy())
      RDCLOG("Enabling VK_EXT_debug_utils");
    params.Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  else if(supportedExtensions.find(VK_EXT_DEBUG_REPORT_EXTENSION_NAME) != supportedExtensions.end() &&
          !params.Extensions.contains(VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
  {
    if(!m_Replay->IsRemoteProxy())
      RDCLOG("Enabling VK_EXT_debug_report");
    params.Extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  VkValidationFeaturesEXT featuresEXT = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  VkValidationFeatureDisableEXT disableFeatures[] = {VK_VALIDATION_FEATURE_DISABLE_SHADERS_EXT};
  featuresEXT.disabledValidationFeatureCount = ARRAY_COUNT(disableFeatures);
  featuresEXT.pDisabledValidationFeatures = disableFeatures;

// enable this to get GPU-based validation, where available, whenever we enable API validation
#if 0
  if(m_ReplayOptions.apiValidation)
  {
    VkValidationFeatureEnableEXT enableFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
    featuresEXT.enabledValidationFeatureCount = ARRAY_COUNT(enableFeatures);
    featuresEXT.pEnabledValidationFeatures = enableFeatures;
  }
#endif

  VkValidationFlagsEXT flagsEXT = {VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT};
  VkValidationCheckEXT disableChecks[] = {VK_VALIDATION_CHECK_SHADERS_EXT};
  flagsEXT.disabledValidationCheckCount = ARRAY_COUNT(disableChecks);
  flagsEXT.pDisabledValidationChecks = disableChecks;

  void *instNext = NULL;

  if(supportedExtensions.find(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) != supportedExtensions.end() &&
     !params.Extensions.contains(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME))
  {
    if(!m_Replay->IsRemoteProxy())
      RDCLOG("Enabling VK_EXT_validation_features");
    params.Extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

    instNext = &featuresEXT;
  }
  else if(supportedExtensions.find(VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME) !=
              supportedExtensions.end() &&
          !params.Extensions.contains(VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME))
  {
    if(!m_Replay->IsRemoteProxy())
      RDCLOG("Enabling VK_EXT_validation_flags");
    params.Extensions.push_back(VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME);

    instNext = &flagsEXT;
  }

  const char **layerscstr = new const char *[params.Layers.size()];
  for(size_t i = 0; i < params.Layers.size(); i++)
    layerscstr[i] = params.Layers[i].c_str();

  const char **extscstr = new const char *[params.Extensions.size()];
  for(size_t i = 0; i < params.Extensions.size(); i++)
    extscstr[i] = params.Extensions[i].c_str();

  VkInstanceCreateInfo instinfo = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      instNext,
      0,
      &renderdocAppInfo,
      (uint32_t)params.Layers.size(),
      layerscstr,
      (uint32_t)params.Extensions.size(),
      extscstr,
  };

  if(params.APIVersion >= VK_API_VERSION_1_0)
    renderdocAppInfo.apiVersion = params.APIVersion;

  m_EnabledExtensions.vulkanVersion = renderdocAppInfo.apiVersion;

  if(!Vulkan_Debug_ReplaceAppInfo())
  {
    // if we're not replacing the app info, set renderdocAppInfo's parameters to the ones from the
    // capture
    renderdocAppInfo.pEngineName = params.EngineName.c_str();
    renderdocAppInfo.engineVersion = params.EngineVersion;
    renderdocAppInfo.pApplicationName = params.AppName.c_str();
    renderdocAppInfo.applicationVersion = params.AppVersion;
  }

  m_Instance = VK_NULL_HANDLE;

  VkResult ret = GetInstanceDispatchTable(NULL)->CreateInstance(&instinfo, NULL, &m_Instance);

#undef CheckExt
#define CheckExt(name, ver)                                                                           \
  if(!strcmp(instinfo.ppEnabledExtensionNames[i], "VK_" #name) || renderdocAppInfo.apiVersion >= ver) \
  {                                                                                                   \
    m_EnabledExtensions.ext_##name = true;                                                            \
  }

  for(uint32_t i = 0; i < instinfo.enabledExtensionCount; i++)
  {
    CheckInstanceExts();
  }

  SAFE_DELETE_ARRAY(layerscstr);
  SAFE_DELETE_ARRAY(extscstr);

  if(ret != VK_SUCCESS)
  {
    RETURN_ERROR_RESULT(ResultCode::APIHardwareUnsupported, "Vulkan instance creation returned %s",
                        ToStr(ret).c_str());
  }

  RDCASSERTEQUAL(ret, VK_SUCCESS);

  GetResourceManager()->WrapResource(m_Instance, m_Instance);

  // we'll add the chunk later when we re-process it.
  if(params.InstanceID != ResourceId())
  {
    GetResourceManager()->AddLiveResource(params.InstanceID, m_Instance);

    AddResource(params.InstanceID, ResourceType::Device, "Instance");
    GetReplay()->GetResourceDesc(params.InstanceID).initialisationChunks.clear();
  }
  else
  {
    GetResourceManager()->AddLiveResource(GetResID(m_Instance), m_Instance);
  }

  InitInstanceExtensionTables(m_Instance, &m_EnabledExtensions);

  m_DbgReportCallback = VK_NULL_HANDLE;
  m_DbgUtilsCallback = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_QueueFamilyIdx = ~0U;
  m_PrevQueue = m_Queue = VK_NULL_HANDLE;
  m_InternalCmds.Reset();

  if(ObjDisp(m_Instance)->CreateDebugUtilsMessengerEXT)
  {
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.pfnUserCallback = &DebugUtilsCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugUtilsMessengerEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgUtilsCallback);
  }
  else if(ObjDisp(m_Instance)->CreateDebugReportCallbackEXT)
  {
    VkDebugReportCallbackCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debugInfo.pfnCallback = &DebugReportCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgReportCallback);
  }

  uint32_t count = 0;

  VkResult vkr = ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, NULL);
  CheckVkResult(vkr);

  if(count == 0)
  {
    RETURN_ERROR_RESULT(ResultCode::APIHardwareUnsupported,
                        "No physical devices exist in this vulkan instance");
  }

  m_ReplayPhysicalDevices.resize(count);
  m_ReplayPhysicalDevicesUsed.resize(count);
  m_OriginalPhysicalDevices.resize(count);

  vkr = ObjDisp(m_Instance)
            ->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, &m_ReplayPhysicalDevices[0]);
  CheckVkResult(vkr);

  for(uint32_t i = 0; i < count; i++)
    GetResourceManager()->WrapResource(m_Instance, m_ReplayPhysicalDevices[i]);

#if ENABLED(RDOC_WIN32)
  if(GetModuleHandleA("nvoglv64.dll"))
    LoadLibraryA("nvoglv64.dll");
#endif

  return ResultCode::Succeeded;
}

VkResult WrappedVulkan::vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator,
                                         VkInstance *pInstance)
{
  RDCASSERT(pCreateInfo);

  // don't support any extensions for this createinfo
  RDCASSERT(pCreateInfo->pApplicationInfo == NULL || pCreateInfo->pApplicationInfo->pNext == NULL);

  const bool internalInstance =
      (pCreateInfo->pApplicationInfo && pCreateInfo->pApplicationInfo->pApplicationName &&
       rdcstr(pCreateInfo->pApplicationInfo->pApplicationName) == "RenderDoc forced instance");

  VkLayerInstanceCreateInfo *layerCreateInfo = (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;

  // step through the chain of pNext until we get to the link info
  while(layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                            layerCreateInfo->function != VK_LAYER_LINK_INFO))
  {
    layerCreateInfo = (VkLayerInstanceCreateInfo *)layerCreateInfo->pNext;
  }
  RDCASSERT(layerCreateInfo);

  if(layerCreateInfo == NULL)
  {
    RDCERR("Couldn't find loader instance create info, which is required. Incompatible loader?");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  // move chain on for next layer
  layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

  PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");

  VkInstanceCreateInfo modifiedCreateInfo;
  modifiedCreateInfo = *pCreateInfo;

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
  {
    if(!IsSupportedExtension(modifiedCreateInfo.ppEnabledExtensionNames[i]))
    {
      RDCERR("RenderDoc does not support instance extension '%s'.",
             modifiedCreateInfo.ppEnabledExtensionNames[i]);
      RDCERR(
          "For KHR/EXT extensions file an issue on github to request support: "
          "https://github.com/baldurk/renderdoc");

      // see if any debug report callbacks were passed in the pNext chain
      VkDebugReportCallbackCreateInfoEXT *report =
          (VkDebugReportCallbackCreateInfoEXT *)pCreateInfo->pNext;

      rdcstr msg = StringFormat::Fmt("RenderDoc does not support requested instance extension: %s.",
                                     modifiedCreateInfo.ppEnabledExtensionNames[i]);

      while(report)
      {
        if(report->sType == VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT)
          report->pfnCallback(VK_DEBUG_REPORT_ERROR_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT,
                              0, 1, 1, "RDOC", msg.c_str(), report->pUserData);

        report = (VkDebugReportCallbackCreateInfoEXT *)report->pNext;
      }

      // or debug utils callbacks
      VkDebugUtilsMessengerCreateInfoEXT *messenger =
          (VkDebugUtilsMessengerCreateInfoEXT *)pCreateInfo->pNext;

      VkDebugUtilsMessengerCallbackDataEXT messengerData = {};

      messengerData.messageIdNumber = 1;
      messengerData.pMessageIdName = NULL;
      messengerData.pMessage = msg.c_str();
      messengerData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;

      while(messenger)
      {
        if(messenger->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT)
          messenger->pfnUserCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &messengerData,
                                     messenger->pUserData);

        messenger = (VkDebugUtilsMessengerCreateInfoEXT *)messenger->pNext;
      }

      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  const char **addedExts = NULL;

  if(!internalInstance)
  {
    addedExts = new const char *[modifiedCreateInfo.enabledExtensionCount + 2];

    bool hasDebugReport = false, hasDebugUtils = false, hasGPDP2 = false;
    for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
    {
      addedExts[i] = modifiedCreateInfo.ppEnabledExtensionNames[i];
      if(!strcmp(addedExts[i], VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        hasDebugReport = true;
      if(!strcmp(addedExts[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        hasDebugUtils = true;
      if(!strcmp(addedExts[i], VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        hasGPDP2 = true;
    }

    rdcarray<VkExtensionProperties> supportedExts;

    // enumerate what instance extensions are available
    void *module = LoadVulkanLibrary();
    if(module)
    {
      PFN_vkEnumerateInstanceExtensionProperties enumInstExts =
          (PFN_vkEnumerateInstanceExtensionProperties)Process::GetFunctionAddress(
              module, "vkEnumerateInstanceExtensionProperties");

      if(enumInstExts)
      {
        uint32_t numSupportedExts = 0;
        enumInstExts(NULL, &numSupportedExts, NULL);

        supportedExts.resize(numSupportedExts);
        enumInstExts(NULL, &numSupportedExts, &supportedExts[0]);
      }
    }

    if(supportedExts.empty())
      RDCWARN(
          "Couldn't load vkEnumerateInstanceExtensionProperties in vkCreateInstance to enumerate "
          "instance extensions");

    // always enable GPDP2 if it's available
    if(!hasGPDP2)
    {
      for(const VkExtensionProperties &ext : supportedExts)
      {
        if(!strcmp(ext.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        {
          addedExts[modifiedCreateInfo.enabledExtensionCount++] =
              VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
          break;
        }
      }
    }

    // always enable debug report/utils, if it's available
    if(!hasDebugUtils)
    {
      for(const VkExtensionProperties &ext : supportedExts)
      {
        if(!strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
          addedExts[modifiedCreateInfo.enabledExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
          break;
        }
      }
    }
    else if(!hasDebugReport)
    {
      for(const VkExtensionProperties &ext : supportedExts)
      {
        if(!strcmp(ext.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
        {
          addedExts[modifiedCreateInfo.enabledExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
          break;
        }
      }
    }

    modifiedCreateInfo.ppEnabledExtensionNames = addedExts;
  }

  bool brokenGetDeviceProcAddr = false;

  // override applicationInfo with RenderDoc's, but preserve apiVersion
  if(modifiedCreateInfo.pApplicationInfo)
  {
    if(modifiedCreateInfo.pApplicationInfo->pEngineName &&
       strlower(modifiedCreateInfo.pApplicationInfo->pEngineName) == "idtech")
      brokenGetDeviceProcAddr = true;

    if(modifiedCreateInfo.pApplicationInfo->apiVersion >= VK_API_VERSION_1_0)
      renderdocAppInfo.apiVersion = modifiedCreateInfo.pApplicationInfo->apiVersion;

    if(Vulkan_Debug_ReplaceAppInfo())
    {
      modifiedCreateInfo.pApplicationInfo = &renderdocAppInfo;
    }
  }

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledLayerCount; i++)
  {
    if(!strcmp(modifiedCreateInfo.ppEnabledLayerNames[i], "VK_LAYER_LUNARG_standard_validation") ||
       !strcmp(modifiedCreateInfo.ppEnabledLayerNames[i], "VK_LAYER_KHRONOS_validation") ||
       !strcmp(modifiedCreateInfo.ppEnabledLayerNames[i], "VK_LAYER_GOOGLE_unique_objects"))
    {
      m_LayersEnabled[VkCheckLayer_unique_objects] = true;
    }
  }

  // if we forced on API validation, it's also available
  m_LayersEnabled[VkCheckLayer_unique_objects] |= RenderDoc::Inst().GetCaptureOptions().apiValidation;

  VkResult ret = createFunc(&modifiedCreateInfo, pAllocator, pInstance);

  m_Instance = *pInstance;

  InitInstanceTable(m_Instance, gpa);

  GetResourceManager()->WrapResource(m_Instance, m_Instance);

  *pInstance = m_Instance;

  // should only be called during capture
  RDCASSERT(IsCaptureMode(m_State));

  m_InitParams.Set(pCreateInfo, GetResID(m_Instance));
  VkResourceRecord *record = GetResourceManager()->AddResourceRecord(m_Instance);

  record->instDevInfo = new InstanceDeviceInfo();

  record->instDevInfo->brokenGetDeviceProcAddr = brokenGetDeviceProcAddr;

  record->instDevInfo->vulkanVersion = VK_API_VERSION_1_0;

  // whether or not we're using it, we updated the apiVersion in renderdocAppInfo
  if(renderdocAppInfo.apiVersion > VK_API_VERSION_1_0)
    record->instDevInfo->vulkanVersion = renderdocAppInfo.apiVersion;

  std::set<rdcstr> availablePhysDeviceFunctions;

  {
    uint32_t count = 0;
    ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, NULL);

    rdcarray<VkPhysicalDevice> physDevs;
    physDevs.resize(count);
    ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, physDevs.data());

    rdcarray<VkExtensionProperties> exts;
    for(VkPhysicalDevice p : physDevs)
    {
      ObjDisp(m_Instance)->EnumerateDeviceExtensionProperties(p, NULL, &count, NULL);

      exts.resize(count);
      ObjDisp(m_Instance)->EnumerateDeviceExtensionProperties(p, NULL, &count, exts.data());

      for(const VkExtensionProperties &e : exts)
      {
        availablePhysDeviceFunctions.insert(e.extensionName);
      }
    }
    // we don't bother wrapping these, they're temporary handles
  }

// an extension is available if:
// * it's enabled in the instance creation
// * it's promoted in the selected vulkan version
// * it's a device extension and available on at least one physical device
#undef CheckExt
#define CheckExt(name, ver)                                                                \
  if(record->instDevInfo->vulkanVersion >= ver ||                                          \
     availablePhysDeviceFunctions.find("VK_" #name) != availablePhysDeviceFunctions.end()) \
  {                                                                                        \
    record->instDevInfo->ext_##name = true;                                                \
  }
  CheckInstanceExts();
#undef CheckExt
#define CheckExt(name, ver)                                               \
  if(!strcmp(modifiedCreateInfo.ppEnabledExtensionNames[i], "VK_" #name)) \
  {                                                                       \
    record->instDevInfo->ext_##name = true;                               \
  }
  for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
  {
    CheckInstanceExts();
  }

  SAFE_DELETE_ARRAY(addedExts);

  InitInstanceExtensionTables(m_Instance, record->instDevInfo);

  // don't register a frame capturer for our internal instance on android
  if(internalInstance)
  {
    RDCDEBUG("Not registering internal instance as frame capturer");
  }
  else
  {
    RenderDoc::Inst().AddDeviceFrameCapturer(LayerDisp(m_Instance), this);
  }

  m_DbgReportCallback = VK_NULL_HANDLE;
  m_DbgUtilsCallback = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_QueueFamilyIdx = ~0U;
  m_PrevQueue = m_Queue = VK_NULL_HANDLE;
  m_InternalCmds.Reset();

  if(ObjDisp(m_Instance)->CreateDebugUtilsMessengerEXT)
  {
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.pfnUserCallback = &DebugUtilsCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugUtilsMessengerEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgUtilsCallback);
  }
  else if(ObjDisp(m_Instance)->CreateDebugReportCallbackEXT)
  {
    VkDebugReportCallbackCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debugInfo.pNext = NULL;
    debugInfo.pfnCallback = &DebugReportCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgReportCallback);
  }

  if(ret == VK_SUCCESS)
  {
    RDCLOG("Initialised capture layer in Vulkan instance.");
  }

  return ret;
}

void WrappedVulkan::Shutdown()
{
  // flush out any pending commands/semaphores
  SubmitCmds();
  SubmitSemaphores();
  FlushQ();

  // idle the device as well so that external queues are idle.
  if(m_Device)
  {
    VkResult vkr = ObjDisp(m_Device)->DeviceWaitIdle(Unwrap(m_Device));
    CheckVkResult(vkr);
  }

  // since we didn't create proper registered resources for our command buffers,
  // they won't be taken down properly with the pool. So we release them (just our
  // data) here.
  for(size_t i = 0; i < m_InternalCmds.freecmds.size(); i++)
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freecmds[i]);

  if(m_IndirectCommandBuffer != VK_NULL_HANDLE)
    GetResourceManager()->ReleaseWrappedResource(m_IndirectCommandBuffer);

  // destroy the pool
  if(m_Device != VK_NULL_HANDLE && m_InternalCmds.cmdpool != VK_NULL_HANDLE)
  {
    ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.cmdpool), NULL);
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.cmdpool);
  }

  for(size_t i = 0; i < m_InternalCmds.freesems.size(); i++)
  {
    ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device), Unwrap(m_InternalCmds.freesems[i]), NULL);
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freesems[i]);
  }

  for(size_t i = 0; i < m_ExternalQueues.size(); i++)
  {
    if(m_ExternalQueues[i].pool != VK_NULL_HANDLE)
    {
      for(size_t x = 0; x < ARRAY_COUNT(m_ExternalQueues[i].ring); x++)
      {
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].acquire);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].release);

        ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device),
                                            Unwrap(m_ExternalQueues[i].ring[x].fromext), NULL);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].fromext);

        ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device),
                                            Unwrap(m_ExternalQueues[i].ring[x].toext), NULL);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].toext);

        ObjDisp(m_Device)->DestroyFence(Unwrap(m_Device), Unwrap(m_ExternalQueues[i].ring[x].fence),
                                        NULL);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].fence);
      }

      ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_ExternalQueues[i].pool), NULL);
      GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].pool);
    }
  }

  FreeAllMemory(MemoryScope::InitialContents);

  if(m_MemoryFreeThread)
  {
    Threading::JoinThread(m_MemoryFreeThread);
    Threading::CloseThread(m_MemoryFreeThread);
    m_MemoryFreeThread = 0;
  }

  // we do more in Shutdown than the equivalent vkDestroyInstance since on replay there's
  // no explicit vkDestroyDevice, we destroy the device here then the instance

  // destroy the physical devices manually because due to remapping the may have leftover
  // refcounts
  for(size_t i = 0; i < m_ReplayPhysicalDevices.size(); i++)
    GetResourceManager()->ReleaseWrappedResource(m_ReplayPhysicalDevices[i]);

  m_Replay->DestroyResources();

  m_IndirectBuffer.Destroy();

  // destroy debug manager and any objects it created
  SAFE_DELETE(m_DebugManager);
  SAFE_DELETE(m_ShaderCache);

  if(m_Instance && ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT &&
     m_DbgReportCallback != VK_NULL_HANDLE)
    ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT(Unwrap(m_Instance), m_DbgReportCallback, NULL);

  if(m_Instance && ObjDisp(m_Instance)->DestroyDebugUtilsMessengerEXT &&
     m_DbgUtilsCallback != VK_NULL_HANDLE)
    ObjDisp(m_Instance)->DestroyDebugUtilsMessengerEXT(Unwrap(m_Instance), m_DbgUtilsCallback, NULL);

  // need to store the unwrapped device and instance to destroy the
  // API object after resource manager shutdown
  VkInstance inst = Unwrap(m_Instance);
  VkDevice dev = Unwrap(m_Device);

  const VkDevDispatchTable *vt = m_Device != VK_NULL_HANDLE ? ObjDisp(m_Device) : NULL;
  const VkInstDispatchTable *vit = m_Instance != VK_NULL_HANDLE ? ObjDisp(m_Instance) : NULL;

  // this destroys the wrapped objects for the devices and instances
  m_ResourceManager->Shutdown();

  delete GetWrapped(m_Device);
  delete GetWrapped(m_Instance);

  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_Instance = VK_NULL_HANDLE;

  m_ReplayPhysicalDevices.clear();
  m_PhysicalDevices.clear();

  for(size_t i = 0; i < m_QueueFamilies.size(); i++)
    delete[] m_QueueFamilies[i];

  m_QueueFamilies.clear();

  // finally destroy device then instance
  if(vt)
    vt->DestroyDevice(dev, NULL);
  if(vit)
    vit->DestroyInstance(inst, NULL);
}

void WrappedVulkan::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
  if(instance == VK_NULL_HANDLE)
    return;

  RDCASSERT(m_Instance == instance);

  if(ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT && m_DbgReportCallback != VK_NULL_HANDLE)
    ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT(Unwrap(m_Instance), m_DbgReportCallback, NULL);

  if(ObjDisp(m_Instance)->DestroyDebugUtilsMessengerEXT && m_DbgUtilsCallback != VK_NULL_HANDLE)
    ObjDisp(m_Instance)->DestroyDebugUtilsMessengerEXT(Unwrap(m_Instance), m_DbgUtilsCallback, NULL);

  // the device should already have been destroyed, assuming that the
  // application is well behaved. If not, we just leak.

  ObjDisp(m_Instance)->DestroyInstance(Unwrap(m_Instance), NULL);
  RenderDoc::Inst().RemoveDeviceFrameCapturer(LayerDisp(m_Instance));

  GetResourceManager()->ReleaseWrappedResource(m_Instance);
  m_Instance = VK_NULL_HANDLE;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkEnumeratePhysicalDevices(SerialiserType &ser, VkInstance instance,
                                                         uint32_t *pPhysicalDeviceCount,
                                                         VkPhysicalDevice *pPhysicalDevices)
{
  SERIALISE_ELEMENT(instance);
  SERIALISE_ELEMENT_LOCAL(PhysicalDeviceIndex, *pPhysicalDeviceCount);
  SERIALISE_ELEMENT_LOCAL(PhysicalDevice, GetResID(*pPhysicalDevices))
      .TypedAs("VkPhysicalDevice"_lit)
      .Important();

  uint32_t legacyUnused_memIdxMap[VK_MAX_MEMORY_TYPES] = {0};
  // not used at the moment but useful for reference and might be used
  // in the future
  VkPhysicalDeviceProperties physProps = {};
  VkPhysicalDeviceMemoryProperties memProps = {};
  VkPhysicalDeviceFeatures physFeatures = {};
  uint32_t queueCount = 0;
  VkQueueFamilyProperties queueProps[16] = {};

  VkPhysicalDeviceDriverProperties driverProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
  };

  if(ser.IsWriting())
  {
    ObjDisp(instance)->GetPhysicalDeviceProperties(Unwrap(*pPhysicalDevices), &physProps);
    ObjDisp(instance)->GetPhysicalDeviceMemoryProperties(Unwrap(*pPhysicalDevices), &memProps);
    ObjDisp(instance)->GetPhysicalDeviceFeatures(Unwrap(*pPhysicalDevices), &physFeatures);

    ObjDisp(instance)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(*pPhysicalDevices),
                                                              &queueCount, NULL);

    if(queueCount > 16)
    {
      RDCERR("More than 16 queue families");
      queueCount = 16;
    }

    ObjDisp(instance)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(*pPhysicalDevices),
                                                              &queueCount, queueProps);

    GetPhysicalDeviceDriverProperties(ObjDisp(instance), Unwrap(*pPhysicalDevices), driverProps);
  }

  SERIALISE_ELEMENT(legacyUnused_memIdxMap).Hidden();    // was never used
  SERIALISE_ELEMENT(physProps);
  SERIALISE_ELEMENT(memProps);
  SERIALISE_ELEMENT(physFeatures);
  SERIALISE_ELEMENT(queueCount);
  SERIALISE_ELEMENT(queueProps);

  // serialisation of the driver properties was added in 0x10
  if(ser.VersionAtLeast(0x10))
  {
    SERIALISE_ELEMENT(driverProps);

    // we don't need any special handling if this is missing - the properties will be empty which is
    // the same as a new capture if we can't query the properties
  }

  VkPhysicalDevice pd = VK_NULL_HANDLE;

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    bool firstTime = true;

    for(bool used : m_ReplayPhysicalDevicesUsed)
      if(used)
        firstTime = false;

    {
      if(PhysicalDeviceIndex >= m_OriginalPhysicalDevices.size())
        m_OriginalPhysicalDevices.resize(PhysicalDeviceIndex + 1);

      m_OriginalPhysicalDevices[PhysicalDeviceIndex].props = physProps;
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].driverProps = driverProps;
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].memProps = memProps;
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].availFeatures = physFeatures;
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].queueCount = queueCount;
      memcpy(m_OriginalPhysicalDevices[PhysicalDeviceIndex].queueProps, queueProps,
             sizeof(queueProps));
    }

    // match up physical devices to those available on replay as best as possible. In general
    // hopefully the most common case is when there's a precise match, and maybe the order changed.
    //
    // If more GPUs were present on replay than during capture, we map many-to-one which might have
    // bad side-effects, but this is as good as we can do.

    uint32_t bestIdx = 0;
    VkPhysicalDeviceProperties bestPhysProps = {};
    VkPhysicalDeviceDriverProperties bestDriverProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
    };

    rdcarray<VkPhysicalDeviceProperties> compPhysPropsArray;
    rdcarray<VkPhysicalDeviceDriverProperties> compDriverPropsArray;

    compPhysPropsArray.resize(m_ReplayPhysicalDevices.size());
    compDriverPropsArray.resize(m_ReplayPhysicalDevices.size());

    // first cache all the physical device data to compare against
    for(uint32_t i = 0; i < (uint32_t)m_ReplayPhysicalDevices.size(); i++)
    {
      VkPhysicalDeviceProperties &compPhysProps = compPhysPropsArray[i];
      RDCEraseEl(compPhysProps);
      VkPhysicalDeviceDriverProperties &compDriverProps = compDriverPropsArray[i];
      RDCEraseEl(compDriverProps);
      compDriverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

      pd = m_ReplayPhysicalDevices[i];

      // find the best possible match for this physical device
      ObjDisp(pd)->GetPhysicalDeviceProperties(Unwrap(pd), &compPhysProps);

      GetPhysicalDeviceDriverProperties(ObjDisp(pd), Unwrap(pd), compDriverProps);

      if(firstTime)
      {
        VkDriverInfo runningVersion(compPhysProps, compDriverProps);

        RDCLOG("Replay has physical device %u available:", i);
        RDCLOG("   - %s (ver %u.%u patch 0x%x) - %04x:%04x", compPhysProps.deviceName,
               runningVersion.Major(), runningVersion.Minor(), runningVersion.Patch(),
               compPhysProps.vendorID, compPhysProps.deviceID);

        if(compDriverProps.driverID != 0)
        {
          RDCLOG(
              "   - %s driver: %s (%s) - CTS %d.%d.%d.%d", ToStr(compDriverProps.driverID).c_str(),
              compDriverProps.driverName, compDriverProps.driverInfo,
              compDriverProps.conformanceVersion.major, compDriverProps.conformanceVersion.minor,
              compDriverProps.conformanceVersion.subminor, compDriverProps.conformanceVersion.patch);
        }
      }
    }

    // if we're forcing use of a GPU, try to find that one
    bool forced = false;
    if(m_ReplayOptions.forceGPUVendor != GPUVendor::Unknown)
    {
      for(uint32_t i = 0; i < (uint32_t)m_ReplayPhysicalDevices.size(); i++)
      {
        const VkPhysicalDeviceProperties &compPhysProps = compPhysPropsArray[i];
        const VkPhysicalDeviceDriverProperties &compDriverProps = compDriverPropsArray[i];

        VkDriverInfo bestInfo(bestPhysProps, bestDriverProps);
        VkDriverInfo compInfo(compPhysProps, compDriverProps);

        // an exact vendorID match is a better match than not
        if(compInfo.Vendor() == m_ReplayOptions.forceGPUVendor &&
           bestInfo.Vendor() != m_ReplayOptions.forceGPUVendor)
        {
          bestIdx = i;
          bestPhysProps = compPhysProps;
          bestDriverProps = compDriverProps;
          forced = true;
          continue;
        }
        else if(compInfo.Vendor() != m_ReplayOptions.forceGPUVendor)
        {
          continue;
        }

        // ditto deviceID
        if(compPhysProps.deviceID == m_ReplayOptions.forceGPUDeviceID &&
           bestPhysProps.deviceID != m_ReplayOptions.forceGPUDeviceID)
        {
          bestIdx = i;
          bestPhysProps = compPhysProps;
          bestDriverProps = compDriverProps;
          forced = true;
          continue;
        }
        else if(compPhysProps.deviceID != m_ReplayOptions.forceGPUDeviceID)
        {
          continue;
        }

        // driver matching. Only do this if we have a driver name to look at
        if(compDriverProps.driverID && !m_ReplayOptions.forceGPUDriverName.empty())
        {
          rdcstr compHumanDriverName = HumanDriverName(compDriverProps.driverID);
          rdcstr bestHumanDriverName = HumanDriverName(bestDriverProps.driverID);

          // check for a better driverID match
          if(compHumanDriverName == m_ReplayOptions.forceGPUDriverName &&
             bestHumanDriverName != m_ReplayOptions.forceGPUDriverName)
          {
            bestIdx = i;
            bestPhysProps = compPhysProps;
            bestDriverProps = compDriverProps;
            forced = true;
            continue;
          }
        }
      }
    }

    if(forced)
    {
      RDCLOG("Forcing use of physical device");
    }
    else
    {
      bestIdx = 0;
      RDCEraseEl(bestPhysProps);
      RDCEraseEl(bestDriverProps);

      for(uint32_t i = 0; i < (uint32_t)m_ReplayPhysicalDevices.size(); i++)
      {
        const VkPhysicalDeviceProperties &compPhysProps = compPhysPropsArray[i];
        const VkPhysicalDeviceDriverProperties &compDriverProps = compDriverPropsArray[i];

        pd = m_ReplayPhysicalDevices[i];

        // the first is the best at the start
        if(i == 0)
        {
          bestPhysProps = compPhysProps;
          bestDriverProps = compDriverProps;
          continue;
        }

        // an exact vendorID match is a better match than not
        if(compPhysProps.vendorID == physProps.vendorID &&
           bestPhysProps.vendorID != physProps.vendorID)
        {
          bestIdx = i;
          bestPhysProps = compPhysProps;
          bestDriverProps = compDriverProps;
          continue;
        }
        else if(compPhysProps.vendorID != physProps.vendorID)
        {
          continue;
        }

        // ditto deviceID
        if(compPhysProps.deviceID == physProps.deviceID &&
           bestPhysProps.deviceID != physProps.deviceID)
        {
          bestIdx = i;
          bestPhysProps = compPhysProps;
          bestDriverProps = compDriverProps;
          continue;
        }
        else if(compPhysProps.deviceID != physProps.deviceID)
        {
          continue;
        }

        // driver matching. Only do this if both capture and replay gave us valid driver info to
        // compare
        if(compDriverProps.driverID && driverProps.driverID)
        {
          // check for a better driverID match
          if(compDriverProps.driverID == driverProps.driverID &&
             bestDriverProps.driverID != driverProps.driverID)
          {
            bestIdx = i;
            bestPhysProps = compPhysProps;
            bestDriverProps = compDriverProps;
            continue;
          }
          else if(compDriverProps.driverID != driverProps.driverID)
          {
            continue;
          }
        }

        // if we have an exact driver version match, prefer that
        if(compPhysProps.driverVersion == physProps.driverVersion &&
           bestPhysProps.driverVersion != physProps.driverVersion)
        {
          bestIdx = i;
          bestPhysProps = compPhysProps;
          bestDriverProps = compDriverProps;
          continue;
        }
        else if(compPhysProps.driverVersion != physProps.driverVersion)
        {
          continue;
        }

        // if we have multiple identical devices, which isn't uncommon, favour the one
        // that hasn't been assigned
        if(m_ReplayPhysicalDevicesUsed[bestIdx] && !m_ReplayPhysicalDevicesUsed[i])
        {
          bestIdx = i;
          bestPhysProps = compPhysProps;
          continue;
        }

        // this device isn't any better, ignore it
      }
    }

    {
      VkDriverInfo capturedVersion(physProps, driverProps);

      RDCLOG("Found capture physical device %u:", PhysicalDeviceIndex);
      RDCLOG("   - %s (ver %u.%u patch 0x%x) - %04x:%04x", physProps.deviceName,
             capturedVersion.Major(), capturedVersion.Minor(), capturedVersion.Patch(),
             physProps.vendorID, physProps.deviceID);

      if(driverProps.driverID != 0)
      {
        RDCLOG("   - %s driver: %s (%s) - CTS %d.%d.%d.%d", ToStr(driverProps.driverID).c_str(),
               driverProps.driverName, driverProps.driverInfo, driverProps.conformanceVersion.major,
               driverProps.conformanceVersion.minor, driverProps.conformanceVersion.subminor,
               driverProps.conformanceVersion.patch);
      }

      RDCLOG("Mapping during replay to %s physical device %u", forced ? "forced" : "best-match",
             bestIdx);
    }

    pd = m_ReplayPhysicalDevices[bestIdx];

    {
      VkPhysicalDevice fakeDevice = MakePhysicalDeviceHandleFromIndex(PhysicalDeviceIndex);

      ResourceId id = ResourceIDGen::GetNewUniqueID();
      WrappedVkPhysicalDevice *wrapped = new WrappedVkPhysicalDevice(fakeDevice, id);

      GetResourceManager()->AddCurrentResource(id, wrapped);

      if(IsReplayMode(m_State))
        GetResourceManager()->AddWrapper(wrapped, ToTypedHandle(fakeDevice));

      fakeDevice = (VkPhysicalDevice)wrapped;

      // we want to preserve the separate physical devices until we actually need the real handle,
      // so don't remap multiple capture-time physical devices to one replay-time physical device
      // yet. See below in Serialise_vkCreateDevice where this is decoded.
      // Note this allocation is pooled so we don't have to explicitly delete it.
      GetResourceManager()->AddLiveResource(PhysicalDevice, fakeDevice);
    }

    AddResource(PhysicalDevice, ResourceType::Device, "Physical Device");
    DerivedResource(m_Instance, PhysicalDevice);

    if(PhysicalDeviceIndex >= m_PhysicalDevices.size())
      m_PhysicalDevices.resize(PhysicalDeviceIndex + 1);
    m_PhysicalDevices[PhysicalDeviceIndex] = pd;

    if(m_ReplayPhysicalDevicesUsed[bestIdx])
    {
      // error if we're remapping multiple physical devices to the same best match
      RDCWARN(
          "Mapping multiple capture-time physical devices to a single replay-time physical device."
          "This means the HW has changed between capture and replay and may cause bugs.");
    }

    m_ReplayPhysicalDevicesUsed[bestIdx] = true;
  }

  return true;
}

VkResult WrappedVulkan::vkEnumeratePhysicalDevices(VkInstance instance,
                                                   uint32_t *pPhysicalDeviceCount,
                                                   VkPhysicalDevice *pPhysicalDevices)
{
  uint32_t count;

  VkResult vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, NULL);

  if(vkr != VK_SUCCESS)
    return vkr;

  VkPhysicalDevice *devices = new VkPhysicalDevice[count];

  SERIALISE_TIME_CALL(
      vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices));
  CheckVkResult(vkr);

  m_PhysicalDevices.resize(count);

  for(uint32_t i = 0; i < count; i++)
  {
    // it's perfectly valid for enumerate type functions to return the same handle
    // each time. If that happens, we will already have a wrapper created so just
    // return the wrapped object to the user and do nothing else
    if(m_PhysicalDevices[i] != VK_NULL_HANDLE)
    {
      GetWrapped(m_PhysicalDevices[i])->RewrapObject(devices[i]);
      devices[i] = m_PhysicalDevices[i];
    }
    else
    {
      GetResourceManager()->WrapResource(instance, devices[i]);

      if(IsCaptureMode(m_State))
      {
        // add the record first since it's used in the serialise function below to fetch
        // the memory indices
        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(devices[i]);
        RDCASSERT(record);

        VkResourceRecord *instrecord = GetRecord(instance);

        VkPhysicalDeviceProperties physProps;

        ObjDisp(devices[i])->GetPhysicalDeviceProperties(Unwrap(devices[i]), &physProps);

        VkPhysicalDeviceDriverProperties driverProps = {};
        GetPhysicalDeviceDriverProperties(ObjDisp(devices[i]), Unwrap(devices[i]), driverProps);

        VkDriverInfo capturedVersion(physProps, driverProps);

        RDCLOG("physical device %u: %s (ver %u.%u patch 0x%x) - %04x:%04x", i, physProps.deviceName,
               capturedVersion.Major(), capturedVersion.Minor(), capturedVersion.Patch(),
               physProps.vendorID, physProps.deviceID);

        m_PhysicalDevices[i] = devices[i];

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkEnumeratePhysicalDevices);
          Serialise_vkEnumeratePhysicalDevices(ser, instance, &i, &devices[i]);

          record->AddChunk(scope.Get());
        }

        instrecord->AddParent(record);

        // copy the instance's setup directly
        record->instDevInfo = new InstanceDeviceInfo(*instrecord->instDevInfo);

        // treat physical devices as pool members of the instance (ie. freed when the instance dies)
        {
          instrecord->LockChunks();
          instrecord->pooledChildren.push_back(record);
          instrecord->UnlockChunks();
        }
      }
    }
  }

  VkResult result = VK_SUCCESS;

  if(pPhysicalDevices)
  {
    if(count > *pPhysicalDeviceCount)
    {
      count = *pPhysicalDeviceCount;
      result = VK_INCOMPLETE;
    }
    memcpy(pPhysicalDevices, devices, count * sizeof(VkPhysicalDevice));
  }
  *pPhysicalDeviceCount = count;

  SAFE_DELETE_ARRAY(devices);

  return result;
}

bool WrappedVulkan::SelectGraphicsComputeQueue(const rdcarray<VkQueueFamilyProperties> &queueProps,
                                               VkDeviceCreateInfo &createInfo,
                                               uint32_t &queueFamilyIndex)
{
  // storage for if we need to change the requested queues
  static rdcarray<VkDeviceQueueCreateInfo> modQueues;

  bool found = false;

  // we need graphics, and if there is a graphics queue there must be a graphics & compute queue.
  const VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

  // for queue priorities, if we need it
  static const float one = 1.0f;

  for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
  {
    uint32_t idx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
    RDCASSERT(idx < queueProps.size());

    // this requested queue is one we can use too
    if((queueProps[idx].queueFlags & search) == search &&
       createInfo.pQueueCreateInfos[i].queueCount > 0)
    {
      queueFamilyIndex = idx;
      found = true;
      break;
    }
  }

  // if we didn't find it, search for which queue family we should add a request for
  if(!found)
  {
    RDCDEBUG("App didn't request a queue family we can use - adding our own");

    for(uint32_t i = 0; i < queueProps.size(); i++)
    {
      if((queueProps[i].queueFlags & search) == search)
      {
        queueFamilyIndex = i;
        found = true;
        break;
      }
    }

    if(!found)
    {
      SET_ERROR_RESULT(
          m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Can't add a queue with required properties for RenderDoc! Unsupported configuration");
      return false;
    }

    // we found the queue family, add it
    modQueues.resize(createInfo.queueCreateInfoCount + 1);
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
      modQueues[i] = createInfo.pQueueCreateInfos[i];

    modQueues[createInfo.queueCreateInfoCount].queueFamilyIndex = queueFamilyIndex;
    modQueues[createInfo.queueCreateInfoCount].queueCount = 1;
    modQueues[createInfo.queueCreateInfoCount].pQueuePriorities = &one;
    modQueues[createInfo.queueCreateInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    modQueues[createInfo.queueCreateInfoCount].pNext = NULL;
    modQueues[createInfo.queueCreateInfoCount].flags = 0;

    createInfo.pQueueCreateInfos = modQueues.data();
    createInfo.queueCreateInfoCount++;
  }

  return true;
}

void WrappedVulkan::SendUserDebugMessage(const rdcstr &msg)
{
  VkDebugUtilsMessengerCallbackDataEXT messengerData = {};

  messengerData.messageIdNumber = 1;
  messengerData.pMessageIdName = NULL;
  messengerData.pMessage = msg.c_str();
  messengerData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;

  {
    SCOPED_LOCK(m_CallbacksLock);

    for(UserDebugReportCallbackData *cb : m_ReportCallbacks)
    {
      cb->createInfo.pfnCallback(VK_DEBUG_REPORT_ERROR_BIT_EXT,
                                 VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT, 0, 1, 1, "RDOC",
                                 msg.c_str(), cb->createInfo.pUserData);
    }

    for(UserDebugUtilsCallbackData *cb : m_UtilsCallbacks)
    {
      cb->createInfo.pfnUserCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &messengerData,
                                     cb->createInfo.pUserData);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDevice(SerialiserType &ser, VkPhysicalDevice physicalDevice,
                                             const VkDeviceCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkDevice *pDevice)
{
  SERIALISE_ELEMENT(physicalDevice).Important();
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Device, GetResID(*pDevice)).TypedAs("VkDevice"_lit);

  if(ser.VersionLess(0xD))
  {
    uint32_t supportedQueueFamily;    // no longer used
    SERIALISE_ELEMENT(supportedQueueFamily).Hidden();
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // kept around only to call DerivedResource below, as this is the resource that actually has an
    // original resource ID.
    VkPhysicalDevice origPhysDevice = physicalDevice;

    // see above in Serialise_vkEnumeratePhysicalDevices where this is encoded
    uint32_t physicalDeviceIndex = GetPhysicalDeviceIndexFromHandle(Unwrap(physicalDevice));
    physicalDevice = m_PhysicalDevices[physicalDeviceIndex];

    RDCLOG("Creating replay device from physical device %u", physicalDeviceIndex);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.availFeatures);

    GetPhysicalDeviceDriverProperties(ObjDisp(physicalDevice), Unwrap(physicalDevice),
                                      m_PhysicalDeviceData.driverProps);

    m_PhysicalDeviceData.driverInfo =
        VkDriverInfo(m_PhysicalDeviceData.props, m_PhysicalDeviceData.driverProps, true);

    rdcarray<VkDeviceQueueGlobalPriorityCreateInfoKHR *> queuePriorities;

    for(uint32_t i = 0; i < CreateInfo.queueCreateInfoCount; i++)
    {
      VkDeviceQueueGlobalPriorityCreateInfoKHR *queuePrio =
          (VkDeviceQueueGlobalPriorityCreateInfoKHR *)FindNextStruct(
              &CreateInfo.pQueueCreateInfos[i],
              VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR);

      if(queuePrio)
        queuePriorities.push_back(queuePrio);
    }

    const PhysicalDeviceData &origData = m_OriginalPhysicalDevices[physicalDeviceIndex];

    m_OrigPhysicalDeviceData = origData;
    m_OrigPhysicalDeviceData.driverInfo = VkDriverInfo(origData.props, origData.driverProps, false);

    // we must make any modifications locally, so the free of pointers
    // in the serialised VkDeviceCreateInfo don't double-free
    VkDeviceCreateInfo createInfo = CreateInfo;

    rdcarray<rdcstr> Extensions;
    for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
      Extensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    }

    StripUnwantedExtensions(Extensions);

    std::set<rdcstr> supportedExtensions;

    {
      uint32_t count = 0;
      ObjDisp(physicalDevice)
          ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &count, NULL);

      VkExtensionProperties *props = new VkExtensionProperties[count];
      ObjDisp(physicalDevice)
          ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &count, props);

      for(uint32_t e = 0; e < count; e++)
        supportedExtensions.insert(props[e].extensionName);

      SAFE_DELETE_ARRAY(props);
    }

    AddRequiredExtensions(false, Extensions, supportedExtensions);

    // Drop VK_KHR_driver_properties if it's not available, but add it if it is
    bool driverPropsSupported = (supportedExtensions.find(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME) !=
                                 supportedExtensions.end());
    if(driverPropsSupported)
    {
      if(!Extensions.contains(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
        Extensions.push_back(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
    }
    else
    {
      Extensions.removeOne(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
    }

    for(size_t i = 0; i < Extensions.size(); i++)
    {
      if(supportedExtensions.find(Extensions[i]) == supportedExtensions.end())
      {
        SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                         "Capture requires extension '%s' which is not supported\n"
                         "\n%s",
                         Extensions[i].c_str(), GetPhysDeviceCompatString(false, false).c_str());
        return false;
      }
    }

    // enable VK_EXT_debug_marker if it's available, to replay markers to the driver/any other
    // layers that might be listening
    if(supportedExtensions.find(VK_EXT_DEBUG_MARKER_EXTENSION_NAME) != supportedExtensions.end())
    {
      Extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
      RDCLOG("Enabling VK_EXT_debug_marker");
    }

    // enable VK_AMD_SHADER_INFO_EXTENSION_NAME if it's available, to fetch shader disassembly
    if(supportedExtensions.find(VK_AMD_SHADER_INFO_EXTENSION_NAME) != supportedExtensions.end())
    {
      Extensions.push_back(VK_AMD_SHADER_INFO_EXTENSION_NAME);
      RDCLOG("Enabling VK_AMD_shader_info");
    }

    // enable VK_AMD_gpa_interface if it's available, for AMD counter support
    if(supportedExtensions.find("VK_AMD_gpa_interface") != supportedExtensions.end())
    {
      Extensions.push_back("VK_AMD_gpa_interface");
      RDCLOG("Enabling VK_AMD_gpa_interface");
    }

    // enable VK_AMD_shader_core_properties if it's available, for AMD counter support
    if(supportedExtensions.find(VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      Extensions.push_back(VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME);
      RDCLOG("Enabling VK_AMD_shader_core_properties");
    }

    // enable VK_MVK_moltenvk if it's available, for detecting/controlling moltenvk.
    // Currently this is used opaquely (extension present or not) rather than using anything the
    // extension provides.
    if(supportedExtensions.find("VK_MVK_moltenvk") != supportedExtensions.end())
    {
      Extensions.push_back("VK_MVK_moltenvk");
      RDCLOG("Enabling VK_MVK_moltenvk");
    }

    // enable VK_KHR_driver_properties if it's available, to match up to capture-time
    if(supportedExtensions.find(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME) != supportedExtensions.end())
    {
      Extensions.push_back(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
      RDCLOG("Enabling VK_KHR_driver_properties");
    }

    // enable VK_KHR_shader_non_semantic_info if it's available, to enable debug printf
    if(supportedExtensions.find(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      Extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
      RDCLOG("Enabling VK_KHR_shader_non_semantic_info");
    }

    bool pipeExec = false;

    // enable VK_KHR_pipeline_executable_properties if it's available, to fetch disassembly and
    // statistics
    if(supportedExtensions.find(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      pipeExec = true;
      Extensions.push_back(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
      RDCLOG("Enabling VK_KHR_pipeline_executable_properties");
    }

    bool xfb = false;

    // enable VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME if it's available, to fetch mesh output in
    // tessellation/geometry stages
    if(supportedExtensions.find(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME) != supportedExtensions.end())
    {
      xfb = true;
      Extensions.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      RDCLOG("Enabling VK_EXT_transform_feedback extension");
    }
    else
    {
      RDCWARN(
          "VK_EXT_transform_feedback extension not available, mesh output from "
          "geometry/tessellation stages will not be available");
    }

    bool KHRbuffer = false, EXTbuffer = false;

    if(supportedExtensions.find(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      Extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
      RDCLOG("Enabling VK_KHR_buffer_device_address");

      KHRbuffer = true;
    }
    else if(supportedExtensions.find(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) !=
            supportedExtensions.end())
    {
      Extensions.push_back(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
      RDCLOG("Enabling VK_EXT_buffer_device_address");

      EXTbuffer = true;
    }
    else
    {
      RDCWARN(
          "VK_[KHR|EXT]_buffer_device_address not available, feedback from "
          "bindless shader access will use less reliable fallback");
    }

    bool perfQuery = false;

    if(supportedExtensions.find(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME) != supportedExtensions.end())
    {
      perfQuery = true;
      Extensions.push_back(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME);
      RDCLOG("Enabling VK_KHR_performance_query");
    }

    VkDevice device;

    rdcarray<VkQueueFamilyProperties> queueProps;

    {
      uint32_t qCount = 0;
      ObjDisp(physicalDevice)
          ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

      queueProps.resize(qCount);
      ObjDisp(physicalDevice)
          ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount,
                                                   queueProps.data());
    }

    // to aid the search algorithm below, we apply implied transfer bit onto the queue properties.
    for(VkQueueFamilyProperties &q : queueProps)
    {
      if(q.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        q.queueFlags |= VK_QUEUE_TRANSFER_BIT;
    }

    uint32_t origQCount = origData.queueCount;
    const VkQueueFamilyProperties *origprops = origData.queueProps;

    // create queue remapping
    for(uint32_t origQIndex = 0; origQIndex < origQCount; origQIndex++)
    {
      m_QueueRemapping[origQIndex].resize(origprops[origQIndex].queueCount);
      RDCLOG("Capture describes queue family %u:", origQIndex);
      RDCLOG("   - %u queues available with %s", origprops[origQIndex].queueCount,
             ToStr(VkQueueFlagBits(origprops[origQIndex].queueFlags)).c_str());
      RDCLOG("     %u timestamp bits (%u,%u,%u) granularity",
             origprops[origQIndex].timestampValidBits,
             origprops[origQIndex].minImageTransferGranularity.width,
             origprops[origQIndex].minImageTransferGranularity.height,
             origprops[origQIndex].minImageTransferGranularity.depth);

      // find the best queue family to map to. We try and find the closest match that is at least
      // good enough. We want to try and preserve families that were separate before but we need to
      // ensure the remapped queue family is at least as good as it was at capture time.
      uint32_t destFamily = 0;

      if(origQIndex < queueProps.size() && equivalent(origprops[origQIndex], queueProps[origQIndex]))
      {
        destFamily = origQIndex;
        RDCLOG(" (identity match)");
      }
      else
      {
        // we categorise the original queue as one of four types: universal
        // (graphics/compute/transfer), graphics/transfer only (rare), compute-only
        // (compute/transfer) or transfer-only (transfer). We try first to find an exact match, then
        // move progressively up the priority list to find a broader and broader match.
        // We don't care about sparse binding - it's just treated as a requirement.
        enum class SearchType
        {
          Failed,
          Universal,
          GraphicsTransfer,
          ComputeTransfer,
          GraphicsOrComputeTransfer,
          TransferOnly,
        } search;

        VkQueueFlags mask = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

        switch(origprops[origQIndex].queueFlags & mask)
        {
          case VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT:
          case VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT:
            search = SearchType::Universal;
            break;
          case VK_QUEUE_GRAPHICS_BIT:
          case VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT:
            search = SearchType::GraphicsTransfer;
            break;
          case VK_QUEUE_COMPUTE_BIT:
          case VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT:
            search = SearchType::ComputeTransfer;
            break;
          case VK_QUEUE_TRANSFER_BIT: search = SearchType::TransferOnly; break;
          default:
            search = SearchType::Failed;
            RDCERR("Unexpected set of flags: %s",
                   ToStr(VkQueueFlagBits(origprops[origQIndex].queueFlags & mask)).c_str());
            break;
        }

        bool needSparse = (origprops[origQIndex].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0;
        VkExtent3D needGranularity = origprops[origQIndex].minImageTransferGranularity;

        while(search != SearchType::Failed)
        {
          bool found = false;

          for(uint32_t replayQIndex = 0; replayQIndex < queueProps.size(); replayQIndex++)
          {
            // ignore queues that couldn't satisfy the required transfer granularity
            if(!CheckTransferGranularity(needGranularity,
                                         queueProps[replayQIndex].minImageTransferGranularity))
              continue;

            // ignore queues that don't have sparse binding, if we need that
            if(needSparse &&
               ((queueProps[replayQIndex].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == 0))
              continue;

            switch(search)
            {
              case SearchType::Failed: break;
              case SearchType::Universal:
                if((queueProps[replayQIndex].queueFlags & mask) ==
                   (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::GraphicsTransfer:
                if((queueProps[replayQIndex].queueFlags & mask) ==
                   (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::ComputeTransfer:
                if((queueProps[replayQIndex].queueFlags & mask) ==
                   (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::GraphicsOrComputeTransfer:
                if((queueProps[replayQIndex].queueFlags & mask) ==
                       (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) ||
                   (queueProps[replayQIndex].queueFlags & mask) ==
                       (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::TransferOnly:
                if((queueProps[replayQIndex].queueFlags & mask) == VK_QUEUE_TRANSFER_BIT)
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
            }

            if(found)
              break;
          }

          if(found)
            break;

          // no such queue family found, fall back to the next type of queue to search for
          switch(search)
          {
            case SearchType::Failed: break;
            case SearchType::Universal: search = SearchType::Failed; break;
            case SearchType::GraphicsTransfer:
            case SearchType::ComputeTransfer:
            case SearchType::GraphicsOrComputeTransfer:
              // if we didn't find a graphics or compute (and transfer) queue, we have to look for a
              // universal one
              search = SearchType::Universal;
              break;
            case SearchType::TransferOnly:
              // when falling back from looking for a transfer-only queue, we consider either
              // graphics-only or compute-only as better candidates before universal
              search = SearchType::GraphicsOrComputeTransfer;
              break;
          }
        }
      }

      RDCLOG("Remapping to queue family %u:", destFamily);
      RDCLOG("   - %u queues available with %s", queueProps[destFamily].queueCount,
             ToStr(VkQueueFlagBits(queueProps[destFamily].queueFlags)).c_str());
      RDCLOG("     %u timestamp bits (%u,%u,%u) granularity",
             queueProps[destFamily].timestampValidBits,
             queueProps[destFamily].minImageTransferGranularity.width,
             queueProps[destFamily].minImageTransferGranularity.height,
             queueProps[destFamily].minImageTransferGranularity.depth);

      // loop over the queues, wrapping around if necessary to provide enough queues. The idea being
      // an application is more likely to use early queues than later ones, so if there aren't
      // enough queues in the family then we should prioritise giving unique queues to the early
      // indices
      for(uint32_t q = 0; q < origprops[origQIndex].queueCount; q++)
      {
        m_QueueRemapping[origQIndex][q] = {destFamily, q % queueProps[destFamily].queueCount};
      }
    }

    VkDeviceQueueCreateInfo *queueCreateInfos =
        (VkDeviceQueueCreateInfo *)createInfo.pQueueCreateInfos;

    // now apply the remapping to the requested queues
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      VkDeviceQueueCreateInfo &queueCreate = (VkDeviceQueueCreateInfo &)queueCreateInfos[i];

      uint32_t queueFamily = queueCreate.queueFamilyIndex;
      queueFamily = m_QueueRemapping[queueFamily][0].family;
      queueCreate.queueFamilyIndex = queueFamily;
      uint32_t queueCount = RDCMIN(queueCreate.queueCount, queueProps[queueFamily].queueCount);

      if(queueCount < queueCreate.queueCount)
        RDCWARN("Truncating queue family request from %u queues to %u queues",
                queueCreate.queueCount, queueCount);

      queueCreate.queueCount = queueCount;
    }

    // remove any duplicates that have been created
    rdcarray<VkDeviceQueueCreateInfo> queueInfos;

    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      VkDeviceQueueCreateInfo &queue1 = (VkDeviceQueueCreateInfo &)queueCreateInfos[i];

      // if we already have this one in the list, continue
      bool already = false;
      for(const VkDeviceQueueCreateInfo &queue2 : queueInfos)
      {
        if(queue1.queueFamilyIndex == queue2.queueFamilyIndex)
        {
          already = true;
          break;
        }
      }

      if(already)
        continue;

      // get the 'biggest' queue allocation from all duplicates. That way we ensure we have enough
      // queues in the queue family to satisfy any remap.
      VkDeviceQueueCreateInfo biggest = queue1;

      for(uint32_t j = i + 1; j < createInfo.queueCreateInfoCount; j++)
      {
        VkDeviceQueueCreateInfo &queue2 = (VkDeviceQueueCreateInfo &)queueCreateInfos[j];

        if(biggest.queueFamilyIndex == queue2.queueFamilyIndex)
        {
          if(queue2.queueCount > biggest.queueCount)
            biggest = queue2;
        }
      }

      queueInfos.push_back(biggest);
    }

    createInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    createInfo.pQueueCreateInfos = queueInfos.data();

    uint32_t qFamilyIdx = 0;

    if(!SelectGraphicsComputeQueue(queueProps, createInfo, qFamilyIdx))
    {
      SET_ERROR_RESULT(
          m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Can't add a queue with required properties for RenderDoc! Unsupported configuration");
      return false;
    }

    // remove structs from extensions that we have stripped but may still be referenced here,
    // to ensure we don't pass structs for disabled extensions.
    bool private_data = false;
    private_data |= RemoveNextStruct(&createInfo, VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO);
    private_data |=
        RemoveNextStruct(&createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES);
    if(private_data)
    {
      RDCLOG("Removed VK_EXT_private_data structs from vkCreateDevice pNext chain");
    }

    bool present_id = false;
    present_id |=
        RemoveNextStruct(&createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR);
    present_id |=
        RemoveNextStruct(&createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR);
    if(present_id)
    {
      RDCLOG("Removed VK_KHR_present_id/wait structs from vkCreateDevice pNext chain");
    }

    VkPhysicalDeviceFeatures enabledFeatures = {0};
    if(createInfo.pEnabledFeatures != NULL)
      enabledFeatures = *createInfo.pEnabledFeatures;

    VkPhysicalDeviceFeatures2 *enabledFeatures2 = (VkPhysicalDeviceFeatures2 *)FindNextStruct(
        &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);

    // VkPhysicalDeviceFeatures2 takes priority
    if(enabledFeatures2)
      enabledFeatures = enabledFeatures2->features;
    else if(createInfo.pEnabledFeatures)
      enabledFeatures = *createInfo.pEnabledFeatures;

    VkPhysicalDeviceFeatures availFeatures = {0};
    ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &availFeatures);

#define CHECK_PHYS_FEATURE(feature)                                            \
  if(enabledFeatures.feature && !availFeatures.feature)                        \
  {                                                                            \
    SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported, \
                     "Capture requires physical device feature '" #feature     \
                     "' which is not supported\n\n%s",                         \
                     GetPhysDeviceCompatString(false, false).c_str());         \
    return false;                                                              \
  }

    CHECK_PHYS_FEATURE(robustBufferAccess);
    CHECK_PHYS_FEATURE(fullDrawIndexUint32);
    CHECK_PHYS_FEATURE(imageCubeArray);
    CHECK_PHYS_FEATURE(independentBlend);
    CHECK_PHYS_FEATURE(geometryShader);
    CHECK_PHYS_FEATURE(tessellationShader);
    CHECK_PHYS_FEATURE(sampleRateShading);
    CHECK_PHYS_FEATURE(dualSrcBlend);
    CHECK_PHYS_FEATURE(logicOp);
    CHECK_PHYS_FEATURE(multiDrawIndirect);
    CHECK_PHYS_FEATURE(drawIndirectFirstInstance);
    CHECK_PHYS_FEATURE(depthClamp);
    CHECK_PHYS_FEATURE(depthBiasClamp);
    CHECK_PHYS_FEATURE(fillModeNonSolid);
    CHECK_PHYS_FEATURE(depthBounds);
    CHECK_PHYS_FEATURE(wideLines);
    CHECK_PHYS_FEATURE(largePoints);
    CHECK_PHYS_FEATURE(alphaToOne);
    CHECK_PHYS_FEATURE(multiViewport);
    CHECK_PHYS_FEATURE(samplerAnisotropy);
    CHECK_PHYS_FEATURE(textureCompressionETC2);
    CHECK_PHYS_FEATURE(textureCompressionASTC_LDR);
    CHECK_PHYS_FEATURE(textureCompressionBC);
    CHECK_PHYS_FEATURE(occlusionQueryPrecise);
    CHECK_PHYS_FEATURE(pipelineStatisticsQuery);
    CHECK_PHYS_FEATURE(vertexPipelineStoresAndAtomics);
    CHECK_PHYS_FEATURE(fragmentStoresAndAtomics);
    CHECK_PHYS_FEATURE(shaderTessellationAndGeometryPointSize);
    CHECK_PHYS_FEATURE(shaderImageGatherExtended);
    CHECK_PHYS_FEATURE(shaderStorageImageExtendedFormats);
    CHECK_PHYS_FEATURE(shaderStorageImageMultisample);
    CHECK_PHYS_FEATURE(shaderStorageImageReadWithoutFormat);
    CHECK_PHYS_FEATURE(shaderStorageImageWriteWithoutFormat);
    CHECK_PHYS_FEATURE(shaderUniformBufferArrayDynamicIndexing);
    CHECK_PHYS_FEATURE(shaderSampledImageArrayDynamicIndexing);
    CHECK_PHYS_FEATURE(shaderStorageBufferArrayDynamicIndexing);
    CHECK_PHYS_FEATURE(shaderStorageImageArrayDynamicIndexing);
    CHECK_PHYS_FEATURE(shaderClipDistance);
    CHECK_PHYS_FEATURE(shaderCullDistance);
    CHECK_PHYS_FEATURE(shaderFloat64);
    CHECK_PHYS_FEATURE(shaderInt64);
    CHECK_PHYS_FEATURE(shaderInt16);
    CHECK_PHYS_FEATURE(shaderResourceResidency);
    CHECK_PHYS_FEATURE(shaderResourceMinLod);
    CHECK_PHYS_FEATURE(sparseBinding);
    CHECK_PHYS_FEATURE(sparseResidencyBuffer);
    CHECK_PHYS_FEATURE(sparseResidencyImage2D);
    CHECK_PHYS_FEATURE(sparseResidencyImage3D);
    CHECK_PHYS_FEATURE(sparseResidency2Samples);
    CHECK_PHYS_FEATURE(sparseResidency4Samples);
    CHECK_PHYS_FEATURE(sparseResidency8Samples);
    CHECK_PHYS_FEATURE(sparseResidency16Samples);
    CHECK_PHYS_FEATURE(sparseResidencyAliased);
    CHECK_PHYS_FEATURE(variableMultisampleRate);
    CHECK_PHYS_FEATURE(inheritedQueries);

#define BEGIN_PHYS_EXT_CHECK(struct, stype)                                                  \
  if(struct *ext = (struct *)FindNextStruct(&createInfo, stype))                             \
  {                                                                                          \
    struct avail = {stype};                                                                  \
    VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};    \
    availBase.pNext = &avail;                                                                \
    ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase); \
    const char *structName = #struct;

#define END_PHYS_EXT_CHECK() }

#define CHECK_PHYS_EXT_FEATURE(feature)                                            \
  if(ext->feature && !avail.feature)                                               \
  {                                                                                \
    SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,     \
                     "Capture requires physical device feature '" #feature         \
                     "' in struct '%s' which is not supported\n\n%s",              \
                     structName, GetPhysDeviceCompatString(false, false).c_str()); \
    return false;                                                                  \
  }

    VkPhysicalDeviceDescriptorIndexingFeatures descIndexingFeatures = {};
    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    VkPhysicalDeviceVulkan13Features vulkan13Features = {};
    VkPhysicalDeviceSynchronization2Features sync2 = {};

    if(ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2)
    {
      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVulkan11Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(storageBuffer16BitAccess);
        CHECK_PHYS_EXT_FEATURE(uniformAndStorageBuffer16BitAccess);
        CHECK_PHYS_EXT_FEATURE(storagePushConstant16);
        CHECK_PHYS_EXT_FEATURE(storageInputOutput16);
        CHECK_PHYS_EXT_FEATURE(multiview);
        CHECK_PHYS_EXT_FEATURE(multiviewGeometryShader);
        CHECK_PHYS_EXT_FEATURE(multiviewTessellationShader);
        CHECK_PHYS_EXT_FEATURE(variablePointersStorageBuffer);
        CHECK_PHYS_EXT_FEATURE(variablePointers);
        CHECK_PHYS_EXT_FEATURE(protectedMemory);
        CHECK_PHYS_EXT_FEATURE(samplerYcbcrConversion);
        CHECK_PHYS_EXT_FEATURE(shaderDrawParameters);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVulkan12Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
      {
        vulkan12Features = *ext;

        CHECK_PHYS_EXT_FEATURE(samplerMirrorClampToEdge);
        CHECK_PHYS_EXT_FEATURE(drawIndirectCount);
        CHECK_PHYS_EXT_FEATURE(storageBuffer8BitAccess);
        CHECK_PHYS_EXT_FEATURE(uniformAndStorageBuffer8BitAccess);
        CHECK_PHYS_EXT_FEATURE(storagePushConstant8);
        CHECK_PHYS_EXT_FEATURE(shaderBufferInt64Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderSharedInt64Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderFloat16);
        CHECK_PHYS_EXT_FEATURE(shaderInt8);
        CHECK_PHYS_EXT_FEATURE(descriptorIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderInputAttachmentArrayDynamicIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderUniformTexelBufferArrayDynamicIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageTexelBufferArrayDynamicIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderUniformBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderSampledImageArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageImageArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderInputAttachmentArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderUniformTexelBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageTexelBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingUniformBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingSampledImageUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingStorageImageUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingStorageBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingUniformTexelBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingStorageTexelBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingUpdateUnusedWhilePending);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingPartiallyBound);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingVariableDescriptorCount);
        CHECK_PHYS_EXT_FEATURE(runtimeDescriptorArray);
        CHECK_PHYS_EXT_FEATURE(samplerFilterMinmax);
        CHECK_PHYS_EXT_FEATURE(scalarBlockLayout);
        CHECK_PHYS_EXT_FEATURE(imagelessFramebuffer);
        CHECK_PHYS_EXT_FEATURE(uniformBufferStandardLayout);
        CHECK_PHYS_EXT_FEATURE(shaderSubgroupExtendedTypes);
        CHECK_PHYS_EXT_FEATURE(separateDepthStencilLayouts);
        CHECK_PHYS_EXT_FEATURE(hostQueryReset);
        CHECK_PHYS_EXT_FEATURE(timelineSemaphore);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddress);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddressCaptureReplay);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddressMultiDevice);
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModel);
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModelDeviceScope);
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModelAvailabilityVisibilityChains);
        CHECK_PHYS_EXT_FEATURE(shaderOutputViewportIndex);
        CHECK_PHYS_EXT_FEATURE(shaderOutputLayer);
        CHECK_PHYS_EXT_FEATURE(subgroupBroadcastDynamicId);

        m_SeparateDepthStencil |= (ext->separateDepthStencilLayouts != VK_FALSE);

        if(ext->bufferDeviceAddress && !avail.bufferDeviceAddressCaptureReplay)
        {
          SET_ERROR_RESULT(
              m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
              "Capture requires bufferDeviceAddress support, which is available, but "
              "bufferDeviceAddressCaptureReplay support is not available which is required to "
              "replay\n"
              "\n%s",
              GetPhysDeviceCompatString(false, false).c_str());
          return false;
        }
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVulkan13Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
      {
        vulkan13Features = *ext;

        CHECK_PHYS_EXT_FEATURE(robustImageAccess);
        CHECK_PHYS_EXT_FEATURE(inlineUniformBlock);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingInlineUniformBlockUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(pipelineCreationCacheControl);
        CHECK_PHYS_EXT_FEATURE(privateData);
        CHECK_PHYS_EXT_FEATURE(shaderDemoteToHelperInvocation);
        CHECK_PHYS_EXT_FEATURE(shaderTerminateInvocation);
        CHECK_PHYS_EXT_FEATURE(subgroupSizeControl);
        CHECK_PHYS_EXT_FEATURE(computeFullSubgroups);
        CHECK_PHYS_EXT_FEATURE(synchronization2);
        CHECK_PHYS_EXT_FEATURE(textureCompressionASTC_HDR);
        CHECK_PHYS_EXT_FEATURE(shaderZeroInitializeWorkgroupMemory);
        CHECK_PHYS_EXT_FEATURE(dynamicRendering);
        CHECK_PHYS_EXT_FEATURE(shaderIntegerDotProduct);
        CHECK_PHYS_EXT_FEATURE(maintenance4);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDevice8BitStorageFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(storageBuffer8BitAccess);
        CHECK_PHYS_EXT_FEATURE(uniformAndStorageBuffer8BitAccess);
        CHECK_PHYS_EXT_FEATURE(storagePushConstant8);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDevice16BitStorageFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(storageBuffer16BitAccess);
        CHECK_PHYS_EXT_FEATURE(uniformAndStorageBuffer16BitAccess);
        CHECK_PHYS_EXT_FEATURE(storagePushConstant16);
        CHECK_PHYS_EXT_FEATURE(storageInputOutput16);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceASTCDecodeFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(decodeModeSharedExponent);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(attachmentFeedbackLoopLayout);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(fragmentShaderBarycentric);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceMultiviewFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(multiview);
        CHECK_PHYS_EXT_FEATURE(multiviewGeometryShader);
        CHECK_PHYS_EXT_FEATURE(multiviewTessellationShader);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceFragmentDensityMapFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(fragmentDensityMap);
        CHECK_PHYS_EXT_FEATURE(fragmentDensityMapDynamic);
        CHECK_PHYS_EXT_FEATURE(fragmentDensityMapNonSubsampledImages);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceFragmentDensityMap2FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(fragmentDensityMapDeferred);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM);
      {
        CHECK_PHYS_EXT_FEATURE(fragmentDensityMapOffset);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceProtectedMemoryFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(protectedMemory);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceSamplerYcbcrConversionFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(samplerYcbcrConversion);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderAtomicInt64Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderBufferInt64Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderSharedInt64Atomics);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderDrawParametersFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderDrawParameters);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderImageFootprintFeaturesNV,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV);
      {
        CHECK_PHYS_EXT_FEATURE(imageFootprint);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceTransformFeedbackFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(transformFeedback);
        CHECK_PHYS_EXT_FEATURE(geometryStreams);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVariablePointerFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(variablePointersStorageBuffer);
        CHECK_PHYS_EXT_FEATURE(variablePointers);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(vertexAttributeInstanceRateDivisor);
        CHECK_PHYS_EXT_FEATURE(vertexAttributeInstanceRateZeroDivisor);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVulkanMemoryModelFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModel);
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModelDeviceScope);
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModelAvailabilityVisibilityChains);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceConditionalRenderingFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(conditionalRendering);
        CHECK_PHYS_EXT_FEATURE(inheritedConditionalRendering);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceHostQueryResetFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(hostQueryReset);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceDepthClipControlFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(depthClipControl);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(primitiveTopologyListRestart);
        CHECK_PHYS_EXT_FEATURE(primitiveTopologyPatchListRestart);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(primitivesGeneratedQuery);
        CHECK_PHYS_EXT_FEATURE(primitivesGeneratedQueryWithRasterizerDiscard);
        CHECK_PHYS_EXT_FEATURE(primitivesGeneratedQueryWithNonZeroStreams);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(multisampledRenderToSingleSampled);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(rasterizationOrderColorAttachmentAccess);
        CHECK_PHYS_EXT_FEATURE(rasterizationOrderDepthAttachmentAccess);
        CHECK_PHYS_EXT_FEATURE(rasterizationOrderStencilAttachmentAccess);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceDepthClipEnableFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(depthClipEnable);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(ycbcrImageArrays);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceBufferDeviceAddressFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddress);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddressCaptureReplay);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddressMultiDevice);

        if(ext->bufferDeviceAddress && !avail.bufferDeviceAddressCaptureReplay)
        {
          SET_ERROR_RESULT(
              m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
              "Capture requires bufferDeviceAddress support, which is available, but "
              "bufferDeviceAddressCaptureReplay support is not available which is required to "
              "replay\n"
              "\n%s",
              GetPhysDeviceCompatString(false, false).c_str());
          return false;
        }
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceBufferDeviceAddressFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddress);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddressCaptureReplay);
        CHECK_PHYS_EXT_FEATURE(bufferDeviceAddressMultiDevice);

        if(ext->bufferDeviceAddress && !avail.bufferDeviceAddressCaptureReplay)
        {
          SET_ERROR_RESULT(
              m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
              "Capture requires bufferDeviceAddress support, which is available, but "
              "bufferDeviceAddressCaptureReplay support is not available which is required to "
              "replay\n"
              "\n%s",
              GetPhysDeviceCompatString(false, false).c_str());
          return false;
        }
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceDescriptorIndexingFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
      {
        descIndexingFeatures = *ext;

        CHECK_PHYS_EXT_FEATURE(shaderInputAttachmentArrayDynamicIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderUniformTexelBufferArrayDynamicIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageTexelBufferArrayDynamicIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderUniformBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderSampledImageArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageImageArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderInputAttachmentArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderUniformTexelBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(shaderStorageTexelBufferArrayNonUniformIndexing);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingUniformBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingSampledImageUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingStorageImageUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingStorageBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingUniformTexelBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingStorageTexelBufferUpdateAfterBind);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingUpdateUnusedWhilePending);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingPartiallyBound);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingVariableDescriptorCount);
        CHECK_PHYS_EXT_FEATURE(runtimeDescriptorArray);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceUniformBufferStandardLayoutFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(uniformBufferStandardLayout);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(fragmentShaderSampleInterlock);
        CHECK_PHYS_EXT_FEATURE(fragmentShaderPixelInterlock);
        CHECK_PHYS_EXT_FEATURE(fragmentShaderShadingRateInterlock);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderDemoteToHelperInvocation);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(texelBufferAlignment);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceIndexTypeUint8FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(indexTypeUint8);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceImagelessFramebufferFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(imagelessFramebuffer);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceSubgroupSizeControlFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(subgroupSizeControl);
        CHECK_PHYS_EXT_FEATURE(computeFullSubgroups);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(pipelineExecutableInfo);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceLineRasterizationFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(rectangularLines);
        CHECK_PHYS_EXT_FEATURE(bresenhamLines);
        CHECK_PHYS_EXT_FEATURE(smoothLines);
        CHECK_PHYS_EXT_FEATURE(stippledRectangularLines);
        CHECK_PHYS_EXT_FEATURE(stippledBresenhamLines);
        CHECK_PHYS_EXT_FEATURE(stippledSmoothLines);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderSubgroupExtendedTypes);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceCoherentMemoryFeaturesAMD,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD);
      {
        CHECK_PHYS_EXT_FEATURE(deviceCoherentMemory);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderClockFeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(shaderSubgroupClock);
        CHECK_PHYS_EXT_FEATURE(shaderDeviceClock);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceMemoryPriorityFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(memoryPriority);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceScalarBlockLayoutFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(scalarBlockLayout);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderFloat16Int8Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderFloat16);
        CHECK_PHYS_EXT_FEATURE(shaderInt8);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceTimelineSemaphoreFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(timelineSemaphore);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(separateDepthStencilLayouts);

        m_SeparateDepthStencil |= (ext->separateDepthStencilLayouts != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDevicePerformanceQueryFeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(performanceCounterQueryPools);
        CHECK_PHYS_EXT_FEATURE(performanceCounterMultipleQueryPools);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceInlineUniformBlockFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(inlineUniformBlock);
        CHECK_PHYS_EXT_FEATURE(descriptorBindingInlineUniformBlockUpdateAfterBind);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceCustomBorderColorFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(customBorderColors);
        CHECK_PHYS_EXT_FEATURE(customBorderColorWithoutFormat);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceRobustness2FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(robustBufferAccess2);
        CHECK_PHYS_EXT_FEATURE(robustImageAccess2);
        CHECK_PHYS_EXT_FEATURE(nullDescriptor);

        m_NULLDescriptorsAllowed |= (ext->nullDescriptor != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDevicePipelineCreationCacheControlFeatures,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(pipelineCreationCacheControl);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceComputeShaderDerivativesFeaturesNV,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV);
      {
        CHECK_PHYS_EXT_FEATURE(computeDerivativeGroupQuads);
        CHECK_PHYS_EXT_FEATURE(computeDerivativeGroupLinear);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(extendedDynamicState);

        m_ExtendedDynState = (ext->extendedDynamicState != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderTerminateInvocationFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderTerminateInvocation);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceImageRobustnessFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(robustImageAccess);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat32Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat32AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat64Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat64AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat32Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat32AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat64Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat64AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(shaderImageFloat32Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderImageFloat32AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(sparseImageFloat32Atomics);
        CHECK_PHYS_EXT_FEATURE(sparseImageFloat32AtomicAdd);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(shaderImageInt64Atomics);
        CHECK_PHYS_EXT_FEATURE(sparseImageInt64Atomics);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderZeroInitializeWorkgroupMemory);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(workgroupMemoryExplicitLayout);
        CHECK_PHYS_EXT_FEATURE(workgroupMemoryExplicitLayoutScalarBlockLayout);
        CHECK_PHYS_EXT_FEATURE(workgroupMemoryExplicitLayout8BitAccess);
        CHECK_PHYS_EXT_FEATURE(workgroupMemoryExplicitLayout16BitAccess);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceSynchronization2Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES);
      {
        sync2 = *ext;

        CHECK_PHYS_EXT_FEATURE(synchronization2);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceMaintenance4Features,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(maintenance4);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderIntegerDotProductFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(shaderIntegerDotProduct);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(shaderSubgroupUniformControlFlow);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat16Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat16AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat16AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat32AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(shaderBufferFloat64AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat16Atomics);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat16AtomicAdd);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat16AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat32AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(shaderSharedFloat64AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(shaderImageFloat32AtomicMinMax);
        CHECK_PHYS_EXT_FEATURE(sparseImageFloat32AtomicMinMax);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(ycbcr2plane444Formats);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(formatRgba10x6WithoutYCbCrSampler);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(globalPriorityQuery);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceColorWriteEnableFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(colorWriteEnable);

        m_DynColorWrite = (ext->colorWriteEnable != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceExtendedDynamicState2FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(extendedDynamicState2);
        CHECK_PHYS_EXT_FEATURE(extendedDynamicState2LogicOp);
        CHECK_PHYS_EXT_FEATURE(extendedDynamicState2PatchControlPoints);

        m_ExtendedDynState2 = (ext->extendedDynamicState2 != VK_FALSE);
        m_ExtendedDynState2Logic = (ext->extendedDynamicState2LogicOp != VK_FALSE);
        m_ExtendedDynState2CPs = (ext->extendedDynamicState2PatchControlPoints != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(vertexInputDynamicState);

        m_DynVertexInput = (ext->vertexInputDynamicState != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(graphicsPipelineLibrary);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceDynamicRenderingFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(dynamicRendering);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDevice4444FormatsFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(formatA4R4G4B4);
        CHECK_PHYS_EXT_FEATURE(formatA4B4G4R4);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceTextureCompressionASTCHDRFeatures,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES);
      {
        CHECK_PHYS_EXT_FEATURE(textureCompressionASTC_HDR);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceFragmentShadingRateFeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(pipelineFragmentShadingRate);
        CHECK_PHYS_EXT_FEATURE(primitiveFragmentShadingRate);
        CHECK_PHYS_EXT_FEATURE(attachmentFragmentShadingRate);

        m_FragmentShadingRate = (ext->pipelineFragmentShadingRate != VK_FALSE) ||
                                (ext->primitiveFragmentShadingRate != VK_FALSE) ||
                                (ext->attachmentFragmentShadingRate != VK_FALSE);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(mutableDescriptorType);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(
          VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT,
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(pageableDeviceLocalMemory);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(swapchainMaintenance1);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceBorderColorSwizzleFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(borderColorSwizzle);
        CHECK_PHYS_EXT_FEATURE(borderColorSwizzleFromImage);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(nonSeamlessCubeMap);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceDepthClampZeroOneFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(depthClampZeroOne);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceImageViewMinLodFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(minLod);
      }
      END_PHYS_EXT_CHECK();
    }

    if(availFeatures.depthClamp)
      enabledFeatures.depthClamp = true;
    else
      RDCWARN(
          "depthClamp = false, overlays like highlight drawcall won't show depth-clipped pixels.");

    // we have a fallback for this case, so no warning
    if(availFeatures.fillModeNonSolid)
      enabledFeatures.fillModeNonSolid = true;

    // don't warn if this isn't available, we just won't report the counters
    if(availFeatures.pipelineStatisticsQuery)
      enabledFeatures.pipelineStatisticsQuery = true;

    if(availFeatures.geometryShader)
      enabledFeatures.geometryShader = true;
    else
      RDCWARN(
          "geometryShader = false, pixel history primitive ID and triangle size overlay will not "
          "be available, and local rendering on this device will not support lit mesh views.");

    // enable these features for simplicity, since we use them when available in the shader
    // debugging to simplify samples. If minlod isn't used then we omit it, and that's fine because
    // the application's shaders wouldn't have been using minlod. We use gatherExtended for gather
    // offsets, which means if it's not supported then we can't debug constant offsets properly
    // (because we pass offsets as uniforms), but that's not a big deal.
    if(availFeatures.shaderImageGatherExtended)
      enabledFeatures.shaderImageGatherExtended = true;
    if(availFeatures.shaderResourceMinLod)
      enabledFeatures.shaderResourceMinLod = true;
    if(availFeatures.imageCubeArray)
      enabledFeatures.imageCubeArray = true;

    bool descIndexingAllowsRBA = true;

    if(vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind ||
       vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind ||
       vulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind ||
       vulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind)
    {
      // if any update after bind feature is enabled, check robustBufferAccessUpdateAfterBind
      VkPhysicalDeviceVulkan12Properties vulkan12Props = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
      };

      VkPhysicalDeviceProperties2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      availBase.pNext = &vulkan12Props;
      ObjDisp(physicalDevice)->GetPhysicalDeviceProperties2(Unwrap(physicalDevice), &availBase);

      descIndexingAllowsRBA = vulkan12Props.robustBufferAccessUpdateAfterBind != VK_FALSE;
    }

    if(descIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind ||
       descIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind ||
       descIndexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind ||
       descIndexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind)
    {
      // if any update after bind feature is enabled, check robustBufferAccessUpdateAfterBind
      VkPhysicalDeviceDescriptorIndexingProperties descIndexingProps = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES,
      };

      VkPhysicalDeviceProperties2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      availBase.pNext = &descIndexingProps;
      ObjDisp(physicalDevice)->GetPhysicalDeviceProperties2(Unwrap(physicalDevice), &availBase);

      descIndexingAllowsRBA = descIndexingProps.robustBufferAccessUpdateAfterBind != VK_FALSE;
    }

    if(availFeatures.robustBufferAccess && !descIndexingAllowsRBA)
    {
      // if the feature is available but we can't use it, warn
      RDCWARN(
          "robustBufferAccess is available, but cannot be enabled due to "
          "robustBufferAccessUpdateAfterBind not being avilable and some UpdateAfterBind features "
          "being enabled. "
          "out of bounds access due to bugs in application or RenderDoc may cause crashes");
    }
    else
    {
      // either the feature is available, and we enable it, or it's not available at all.
      if(availFeatures.robustBufferAccess)
        enabledFeatures.robustBufferAccess = true;
      else
        RDCWARN(
            "robustBufferAccess = false, out of bounds access due to bugs in application or "
            "RenderDoc may cause crashes");
    }

    if(availFeatures.shaderInt64)
      enabledFeatures.shaderInt64 = true;
    else
      RDCWARN("shaderInt64 = false, feedback from shaders will use less reliable fallback.");

    if(availFeatures.shaderStorageImageWriteWithoutFormat)
      enabledFeatures.shaderStorageImageWriteWithoutFormat = true;
    else
      RDCWARN(
          "shaderStorageImageWriteWithoutFormat = false, save/load from 2DMS textures will not be "
          "possible");

    if(availFeatures.shaderStorageImageMultisample)
      enabledFeatures.shaderStorageImageMultisample = true;
    else
      RDCWARN(
          "shaderStorageImageMultisample = false, accurately replaying 2DMS textures will not be "
          "possible");

    if(availFeatures.occlusionQueryPrecise)
      enabledFeatures.occlusionQueryPrecise = true;
    else
      RDCWARN("occlusionQueryPrecise = false, samples passed counter will not be available");

    if(availFeatures.fragmentStoresAndAtomics)
      enabledFeatures.fragmentStoresAndAtomics = true;
    else
      RDCWARN(
          "fragmentStoresAndAtomics = false, quad overdraw overlay will not be available and "
          "feedback from shaders will not be fetched for fragment stage");

    if(availFeatures.vertexPipelineStoresAndAtomics)
      enabledFeatures.vertexPipelineStoresAndAtomics = true;
    else
      RDCWARN(
          "vertexPipelineStoresAndAtomics = false, feedback from shaders will not be fetched for "
          "vertex stages");

    if(availFeatures.sampleRateShading)
      enabledFeatures.sampleRateShading = true;
    else
      RDCWARN(
          "sampleRateShading = false, save/load from depth 2DMS textures will not be "
          "possible");

    // patch the enabled features
    if(enabledFeatures2)
      enabledFeatures2->features = enabledFeatures;
    else
      createInfo.pEnabledFeatures = &enabledFeatures;

    uint32_t numExts = 0;

    VkResult vkr =
        ObjDisp(physicalDevice)
            ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &numExts, NULL);
    CheckVkResult(vkr);

    VkExtensionProperties *exts = new VkExtensionProperties[numExts];

    vkr = ObjDisp(physicalDevice)
              ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &numExts, exts);
    CheckVkResult(vkr);

    for(uint32_t i = 0; i < numExts; i++)
      RDCLOG("Dev Ext %u: %s (%u)", i, exts[i].extensionName, exts[i].specVersion);

    SAFE_DELETE_ARRAY(exts);

    VkPhysicalDeviceProperties physProps;
    ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &physProps);

    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pipeExecFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR,
    };

    if(pipeExec)
    {
      VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      availBase.pNext = &pipeExecFeatures;
      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase);

      if(pipeExecFeatures.pipelineExecutableInfo)
      {
        // see if there's an existing struct
        VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR *existing =
            (VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR *)FindNextStruct(
                &createInfo,
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR);

        if(existing)
        {
          // if so, make sure the feature is enabled
          existing->pipelineExecutableInfo = VK_TRUE;
        }
        else
        {
          // otherwise, add our own, and push it onto the pNext array
          pipeExecFeatures.pipelineExecutableInfo = VK_TRUE;

          pipeExecFeatures.pNext = (void *)createInfo.pNext;
          createInfo.pNext = &pipeExecFeatures;
        }
      }
      else
      {
        RDCWARN(
            "VK_KHR_pipeline_executable_properties is available, but the physical device feature "
            "is not. Disabling");

        Extensions.removeOne(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
      }
    }

    VkPhysicalDeviceTransformFeedbackFeaturesEXT xfbFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,
    };

    // if we're enabling XFB, make sure we can enable the physical device feature
    if(xfb)
    {
      VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      availBase.pNext = &xfbFeatures;
      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase);

      if(xfbFeatures.transformFeedback)
      {
        // see if there's an existing struct
        VkPhysicalDeviceTransformFeedbackFeaturesEXT *existing =
            (VkPhysicalDeviceTransformFeedbackFeaturesEXT *)FindNextStruct(
                &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);

        if(existing)
        {
          // if so, make sure the feature is enabled
          existing->transformFeedback = VK_TRUE;
        }
        else
        {
          // otherwise, add our own, and push it onto the pNext array
          xfbFeatures.transformFeedback = VK_TRUE;
          xfbFeatures.geometryStreams = VK_FALSE;

          xfbFeatures.pNext = (void *)createInfo.pNext;
          createInfo.pNext = &xfbFeatures;
        }
      }
      else
      {
        RDCWARN(
            "VK_EXT_transform_feedback is available, but the physical device feature is not. "
            "Disabling");

        Extensions.removeOne(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
      }
    }

    VkPhysicalDevicePerformanceQueryFeaturesKHR perfFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR,
    };

    if(perfQuery)
    {
      VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      m_PhysicalDeviceData.performanceQueryFeatures.sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR;
      availBase.pNext = &perfFeatures;
      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase);

      m_PhysicalDeviceData.performanceQueryFeatures = perfFeatures;

      if(perfFeatures.performanceCounterQueryPools)
      {
        VkPhysicalDevicePerformanceQueryFeaturesKHR *existing =
            (VkPhysicalDevicePerformanceQueryFeaturesKHR *)FindNextStruct(
                &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR);

        if(existing)
        {
          existing->performanceCounterQueryPools = VK_TRUE;
        }
        else
        {
          perfFeatures.performanceCounterQueryPools = VK_TRUE;
          perfFeatures.performanceCounterMultipleQueryPools = VK_FALSE;

          perfFeatures.pNext = (void *)createInfo.pNext;
          createInfo.pNext = &perfFeatures;
        }
      }
      else
      {
        Extensions.removeOne(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME);
      }
    }

    VkPhysicalDeviceBufferDeviceAddressFeaturesEXT bufAddrEXTFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT,
    };
    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufAddrKHRFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };

    if(RDCMIN(m_EnabledExtensions.vulkanVersion, physProps.apiVersion) >= VK_MAKE_VERSION(1, 3, 0))
    {
      // VK_EXT_extended_dynamic_state and VK_EXT_extended_dynamic_state2 were unconditionally
      // promoted and considered implicitly enabled in vulkan 1.3
      m_ExtendedDynState = true;
      m_ExtendedDynState2 = true;
      // logic and patch CPs were not
    }

    if(RDCMIN(m_EnabledExtensions.vulkanVersion, physProps.apiVersion) >= VK_MAKE_VERSION(1, 2, 0))
    {
      VkPhysicalDeviceVulkan12Features avail12Features = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };
      VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      availBase.pNext = &avail12Features;
      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase);

      if(avail12Features.bufferDeviceAddress)
      {
        VkPhysicalDeviceVulkan12Features *existing =
            (VkPhysicalDeviceVulkan12Features *)FindNextStruct(
                &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);

        if(existing)
        {
          if(existing->bufferDeviceAddress)
            existing->bufferDeviceAddressCaptureReplay = VK_TRUE;
          existing->bufferDeviceAddress = VK_TRUE;
        }
        else
        {
          VkPhysicalDeviceBufferDeviceAddressFeaturesKHR *existingKHR =
              (VkPhysicalDeviceBufferDeviceAddressFeaturesKHR *)FindNextStruct(
                  &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR);
          VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *existingEXT =
              (VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *)FindNextStruct(
                  &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT);

          if(existingKHR)
          {
            if(existingKHR->bufferDeviceAddress)
              existingKHR->bufferDeviceAddressCaptureReplay = VK_TRUE;
            existingKHR->bufferDeviceAddress = VK_TRUE;
          }
          else if(existingEXT)
          {
            if(existingEXT->bufferDeviceAddress)
              existingEXT->bufferDeviceAddressCaptureReplay = VK_TRUE;
            existingEXT->bufferDeviceAddress = VK_TRUE;
          }
          else
          {
            // don't add a new VkPhysicalDeviceVulkan12Features to the pNext chain because if we do
            // we have to remove any components etc. Instead just add the individual
            // VkPhysicalDeviceBufferDeviceAddressFeaturesKHR
            bufAddrKHRFeatures.bufferDeviceAddress = VK_TRUE;
            bufAddrKHRFeatures.bufferDeviceAddressMultiDevice = VK_FALSE;

            bufAddrKHRFeatures.pNext = (void *)createInfo.pNext;
            createInfo.pNext = &bufAddrKHRFeatures;
          }
        }
      }
    }
    else if(KHRbuffer)
    {
      VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      availBase.pNext = &bufAddrKHRFeatures;
      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase);

      if(bufAddrKHRFeatures.bufferDeviceAddress)
      {
        // see if there's an existing struct
        VkPhysicalDeviceBufferDeviceAddressFeaturesKHR *existing =
            (VkPhysicalDeviceBufferDeviceAddressFeaturesKHR *)FindNextStruct(
                &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR);

        if(existing)
        {
          if(existing->bufferDeviceAddress)
            existing->bufferDeviceAddressCaptureReplay = VK_TRUE;
          // if so, make sure the feature is enabled
          existing->bufferDeviceAddress = VK_TRUE;
        }
        else
        {
          // otherwise, add our own, and push it onto the pNext array
          bufAddrKHRFeatures.bufferDeviceAddress = VK_TRUE;
          bufAddrKHRFeatures.bufferDeviceAddressMultiDevice = VK_FALSE;

          bufAddrKHRFeatures.pNext = (void *)createInfo.pNext;
          createInfo.pNext = &bufAddrKHRFeatures;
        }
      }
      else
      {
        RDCWARN(
            "VK_KHR_buffer_device_address is available, but the physical device feature "
            "is not. Disabling");

        Extensions.removeOne(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
      }
    }
    else if(EXTbuffer)
    {
      VkPhysicalDeviceFeatures2 availBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      availBase.pNext = &bufAddrEXTFeatures;
      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &availBase);

      if(bufAddrEXTFeatures.bufferDeviceAddress)
      {
        // see if there's an existing struct
        VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *existing =
            (VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *)FindNextStruct(
                &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT);

        if(existing)
        {
          if(existing->bufferDeviceAddress)
            existing->bufferDeviceAddressCaptureReplay = VK_TRUE;

          // if so, make sure the feature is enabled
          existing->bufferDeviceAddress = VK_TRUE;
        }
        else
        {
          // otherwise, add our own, and push it onto the pNext array
          bufAddrEXTFeatures.bufferDeviceAddress = VK_TRUE;
          bufAddrEXTFeatures.bufferDeviceAddressMultiDevice = VK_FALSE;

          bufAddrEXTFeatures.pNext = (void *)createInfo.pNext;
          createInfo.pNext = &bufAddrEXTFeatures;
        }
      }
      else
      {
        RDCWARN(
            "VK_EXT_buffer_device_address is available, but the physical device feature "
            "is not. Disabling");

        Extensions.removeOne(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
      }
    }

    rdcarray<const char *> layerArray;
    layerArray.resize(m_InitParams.Layers.size());
    for(size_t i = 0; i < m_InitParams.Layers.size(); i++)
      layerArray[i] = m_InitParams.Layers[i].c_str();

    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;

    rdcarray<const char *> extArray;
    extArray.resize(Extensions.size());
    for(size_t i = 0; i < Extensions.size(); i++)
      extArray[i] = Extensions[i].c_str();

    createInfo.enabledExtensionCount = (uint32_t)extArray.size();
    createInfo.ppEnabledExtensionNames = extArray.data();

    byte *tempMem = GetTempMemory(GetNextPatchSize(createInfo.pNext));

    UnwrapNextChain(m_State, "VkDeviceCreateInfo", tempMem, (VkBaseInStructure *)&createInfo);

    VkDeviceGroupDeviceCreateInfo *device_group_info =
        (VkDeviceGroupDeviceCreateInfo *)FindNextStruct(
            &createInfo, VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO);
    // decode physical devices that are actually indices
    if(device_group_info)
    {
      VkPhysicalDevice *physDevs = (VkPhysicalDevice *)device_group_info->pPhysicalDevices;
      for(uint32_t i = 0; i < device_group_info->physicalDeviceCount; i++)
        physDevs[i] = Unwrap(m_PhysicalDevices[GetPhysicalDeviceIndexFromHandle(physDevs[i])]);
    }

    vkr = GetDeviceDispatchTable(NULL)->CreateDevice(Unwrap(physicalDevice), &createInfo, NULL,
                                                     &device);

    if(vkr != VK_SUCCESS && !queuePriorities.empty())
    {
      RDCWARN("Failed to create logical device: %s. Reducing queue priorities", ToStr(vkr).c_str());

      for(VkDeviceQueueGlobalPriorityCreateInfoKHR *q : queuePriorities)
      {
        // medium is considered the default if no priority is set otherwise
        if(q->globalPriority > VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT)
          q->globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT;
      }

      vkr = GetDeviceDispatchTable(NULL)->CreateDevice(Unwrap(physicalDevice), &createInfo, NULL,
                                                       &device);
    }

    if(vkr != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating logical device, VkResult: %s", ToStr(vkr).c_str());
      return false;
    }

    GetResourceManager()->WrapResource(device, device);
    GetResourceManager()->AddLiveResource(Device, device);

    AddResource(Device, ResourceType::Device, "Device");
    DerivedResource(origPhysDevice, Device);

// we unset the extension because it may be a 'shared' extension that's available at both instance
// and device. Only set it to enabled if it's really enabled for this device. This can happen with a
// device extension that is reported by another physical device than the one selected - it becomes
// available at instance level (e.g. for physical device queries) but is not available at *this*
// device level.
#undef CheckExt
#define CheckExt(name, ver) m_EnabledExtensions.ext_##name = false;

    CheckDeviceExts();

    uint32_t effectiveApiVersion = RDCMIN(m_EnabledExtensions.vulkanVersion, physProps.apiVersion);

#undef CheckExt
#define CheckExt(name, ver)                \
  if(effectiveApiVersion >= ver)           \
  {                                        \
    m_EnabledExtensions.ext_##name = true; \
  }
    CheckDeviceExts();

#undef CheckExt
#define CheckExt(name, ver)                                       \
  if(!strcmp(createInfo.ppEnabledExtensionNames[i], "VK_" #name)) \
  {                                                               \
    m_EnabledExtensions.ext_##name = true;                        \
  }
    for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
      CheckDeviceExts();
    }

    // for cases where a promoted extension isn't supported as the extension itself, manually
    // disable them when the feature bit is false.

    if(effectiveApiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      if(supportedExtensions.find(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) ==
             supportedExtensions.end() &&
         !vulkan12Features.bufferDeviceAddress)
        m_EnabledExtensions.ext_KHR_buffer_device_address = false;

      if(supportedExtensions.find(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) ==
             supportedExtensions.end() &&
         !vulkan12Features.drawIndirectCount)
        m_EnabledExtensions.ext_KHR_draw_indirect_count = false;

      if(supportedExtensions.find(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME) ==
             supportedExtensions.end() &&
         !vulkan12Features.samplerFilterMinmax)
        m_EnabledExtensions.ext_EXT_sampler_filter_minmax = false;

      // these features are required so this should never happen
      if(supportedExtensions.find(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME) ==
             supportedExtensions.end() &&
         !vulkan12Features.separateDepthStencilLayouts)
      {
        RDCWARN(
            "Required feature 'separateDepthStencilLayouts' not supported by 1.2 physical device.");
        m_EnabledExtensions.ext_KHR_separate_depth_stencil_layouts = false;
      }
    }

    // we also need to check for feature enablement - if an extension is promoted that doesn't mean
    // it's enabled

    if(m_EnabledExtensions.ext_KHR_synchronization2)
    {
      if(!vulkan13Features.synchronization2 && !sync2.synchronization2)
        m_EnabledExtensions.ext_KHR_synchronization2 = false;
    }

    InitInstanceExtensionTables(m_Instance, &m_EnabledExtensions);
    InitDeviceExtensionTables(device, &m_EnabledExtensions);

    RDCASSERT(m_Device == VK_NULL_HANDLE);    // MULTIDEVICE

    m_PhysicalDevice = physicalDevice;
    m_Device = device;

    m_QueueFamilyIdx = qFamilyIdx;

    if(m_InternalCmds.cmdpool == VK_NULL_HANDLE)
    {
      VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
                                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          qFamilyIdx};
      vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL,
                                               &m_InternalCmds.cmdpool);
      CheckVkResult(vkr);

      GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.cmdpool);
    }

    // for each queue family we've remapped to, ensure we have a command pool and command buffer on
    // that queue, and we'll also use the first queue that the application creates (or fetch our
    // own).
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      uint32_t qidx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
      m_ExternalQueues.resize(RDCMAX((uint32_t)m_ExternalQueues.size(), qidx + 1));

      ImageBarrierSequence::SetMaxQueueFamilyIndex(qidx);

      VkCommandPoolCreateInfo poolInfo = {
          VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qidx,
      };
      vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL,
                                               &m_ExternalQueues[qidx].pool);
      CheckVkResult(vkr);

      GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].pool);

      VkCommandBufferAllocateInfo cmdInfo = {
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          NULL,
          Unwrap(m_ExternalQueues[qidx].pool),
          VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          1,
      };

      VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL,
                                     VK_FENCE_CREATE_SIGNALED_BIT};
      VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

      for(size_t x = 0; x < ARRAY_COUNT(m_ExternalQueues[i].ring); x++)
      {
        vkr = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &cmdInfo,
                                                      &m_ExternalQueues[qidx].ring[x].acquire);
        CheckVkResult(vkr);

        if(m_SetDeviceLoaderData)
          m_SetDeviceLoaderData(device, m_ExternalQueues[qidx].ring[x].acquire);
        else
          SetDispatchTableOverMagicNumber(device, m_ExternalQueues[qidx].ring[x].acquire);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].acquire);

        vkr = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &cmdInfo,
                                                      &m_ExternalQueues[qidx].ring[x].release);
        CheckVkResult(vkr);

        if(m_SetDeviceLoaderData)
          m_SetDeviceLoaderData(device, m_ExternalQueues[qidx].ring[x].release);
        else
          SetDispatchTableOverMagicNumber(device, m_ExternalQueues[qidx].ring[x].release);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].release);

        vkr = ObjDisp(device)->CreateSemaphore(Unwrap(device), &semInfo, NULL,
                                               &m_ExternalQueues[qidx].ring[x].fromext);
        CheckVkResult(vkr);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].fromext);

        vkr = ObjDisp(device)->CreateSemaphore(Unwrap(device), &semInfo, NULL,
                                               &m_ExternalQueues[qidx].ring[x].toext);
        CheckVkResult(vkr);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].toext);

        vkr = ObjDisp(device)->CreateFence(Unwrap(device), &fenceInfo, NULL,
                                           &m_ExternalQueues[qidx].ring[x].fence);
        CheckVkResult(vkr);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].fence);
      }
    }

    m_Replay->SetDriverInformation(m_PhysicalDeviceData.props, m_PhysicalDeviceData.driverProps);

    m_PhysicalDeviceData.enabledFeatures = enabledFeatures;

    // MoltenVK reports 0x3fffffff for this limit so just ignore that value if it comes up
    RDCASSERT(m_PhysicalDeviceData.props.limits.maxBoundDescriptorSets <
                      ARRAY_COUNT(BakedCmdBufferInfo::pushDescriptorID[0]) ||
                  m_PhysicalDeviceData.props.limits.maxBoundDescriptorSets >= 0x10000000,
              m_PhysicalDeviceData.props.limits.maxBoundDescriptorSets);

    m_PhysicalDeviceData.queueCount = (uint32_t)queueProps.size();
    for(size_t i = 0; i < queueProps.size(); i++)
      m_PhysicalDeviceData.queueProps[i] = queueProps[i];

    ChooseMemoryIndices();

    APIProps.vendor = GetDriverInfo().Vendor();

    // temporarily disable the debug message sink, to ignore any false positive messages from our
    // init
    ScopedDebugMessageSink *sink = GetDebugMessageSink();
    SetDebugMessageSink(NULL);

    m_ShaderCache = new VulkanShaderCache(this);

    m_DebugManager = new VulkanDebugManager(this);

    m_Replay->CreateResources();

    SetDebugMessageSink(sink);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDevice(VkPhysicalDevice physicalDevice,
                                       const VkDeviceCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
  VkDeviceCreateInfo createInfo = *pCreateInfo;

  for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
  {
    if(!IsSupportedExtension(createInfo.ppEnabledExtensionNames[i]))
    {
      RDCERR("RenderDoc does not support device extension '%s'.",
             createInfo.ppEnabledExtensionNames[i]);
      RDCERR(
          "For KHR/EXT extensions file an issue on github to request support: "
          "https://github.com/baldurk/renderdoc");

      SendUserDebugMessage(
          StringFormat::Fmt("RenderDoc does not support requested device extension: %s.",
                            createInfo.ppEnabledExtensionNames[i]));

      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  if(m_Device != VK_NULL_HANDLE)
  {
    SendUserDebugMessage("RenderDoc does not support multiple simultaneous logical devices.");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  rdcarray<const char *> Extensions(createInfo.ppEnabledExtensionNames,
                                    createInfo.enabledExtensionCount);

  // enable VK_KHR_driver_properties if it's available
  {
    uint32_t count = 0;
    ObjDisp(physicalDevice)
        ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &count, NULL);

    VkExtensionProperties *props = new VkExtensionProperties[count];
    ObjDisp(physicalDevice)
        ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &count, props);

    for(uint32_t e = 0; e < count; e++)
    {
      if(!strcmp(props[e].extensionName, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
      {
        Extensions.push_back(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
        break;
      }
    }

    SAFE_DELETE_ARRAY(props);
  }

  createInfo.ppEnabledExtensionNames = Extensions.data();
  createInfo.enabledExtensionCount = (uint32_t)Extensions.size();

  VkResult vkr = VK_SUCCESS;

  rdcarray<VkQueueFamilyProperties> queueProps;

  {
    uint32_t qCount = 0;
    ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

    queueProps.resize(qCount);
    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, queueProps.data());
  }

  // find a queue that supports all capabilities, and if one doesn't exist, add it.
  uint32_t qFamilyIdx = 0;

  if(!SelectGraphicsComputeQueue(queueProps, createInfo, qFamilyIdx))
    return VK_ERROR_INITIALIZATION_FAILED;

  m_QueueFamilies.resize(createInfo.queueCreateInfoCount);
  m_QueueFamilyCounts.resize(createInfo.queueCreateInfoCount);
  m_QueueFamilyIndices.clear();
  for(size_t i = 0; i < createInfo.queueCreateInfoCount; i++)
  {
    uint32_t family = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
    uint32_t count = createInfo.pQueueCreateInfos[i].queueCount;
    m_QueueFamilies.resize(RDCMAX(m_QueueFamilies.size(), size_t(family) + 1));
    m_QueueFamilyCounts.resize(RDCMAX(m_QueueFamilies.size(), size_t(family) + 1));

    m_QueueFamilies[family] = new VkQueue[count];
    m_QueueFamilyCounts[family] = count;
    for(uint32_t q = 0; q < count; q++)
      m_QueueFamilies[family][q] = VK_NULL_HANDLE;

    if(!m_QueueFamilyIndices.contains(family))
      m_QueueFamilyIndices.push_back(family);
  }

  VkLayerDeviceCreateInfo *layerCreateInfo = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;

  // step through the chain of pNext until we get to the link info
  while(layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                            layerCreateInfo->function != VK_LAYER_LINK_INFO))
  {
    layerCreateInfo = (VkLayerDeviceCreateInfo *)layerCreateInfo->pNext;
  }
  RDCASSERT(layerCreateInfo);

  if(layerCreateInfo == NULL)
  {
    RDCERR("Couldn't find loader device create info, which is required. Incompatible loader?");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  // move chain on for next layer
  layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

  PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(Unwrap(m_Instance), "vkCreateDevice");

  // now search again through for the loader data callback (if it exists)
  layerCreateInfo = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;

  // step through the chain of pNext
  while(layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                            layerCreateInfo->function != VK_LOADER_DATA_CALLBACK))
  {
    layerCreateInfo = (VkLayerDeviceCreateInfo *)layerCreateInfo->pNext;
  }

  // if we found one (we might not - on old loaders), then store the func ptr for
  // use instead of SetDispatchTableOverMagicNumber
  if(layerCreateInfo)
  {
    RDCASSERT(m_SetDeviceLoaderData == layerCreateInfo->u.pfnSetDeviceLoaderData ||
                  m_SetDeviceLoaderData == NULL,
              (void *)m_SetDeviceLoaderData, (void *)layerCreateInfo->u.pfnSetDeviceLoaderData);
    m_SetDeviceLoaderData = layerCreateInfo->u.pfnSetDeviceLoaderData;
  }

  // patch enabled features needed at capture time

  VkPhysicalDeviceFeatures availFeatures;
  ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &availFeatures);

  // default to all off. This is equivalent to createInfo.pEnabledFeatures == NULL
  VkPhysicalDeviceFeatures enabledFeatures = {0};

  // allocate and unwrap the next chain, so we can patch features if we need to, as well as removing
  // the loader info later when it comes time to serialise
  byte *tempMem = GetTempMemory(GetNextPatchSize(createInfo.pNext));

  UnwrapNextChain(m_State, "VkDeviceCreateInfo", tempMem, (VkBaseInStructure *)&createInfo);

  VkPhysicalDeviceFeatures2 *enabledFeatures2 = (VkPhysicalDeviceFeatures2 *)FindNextStruct(
      &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
  /*
  VkPhysicalDeviceVulkan11Features *enabledFeaturesVK11 =
      (VkPhysicalDeviceVulkan11Features *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
          */
  VkPhysicalDeviceVulkan12Features *enabledFeaturesVK12 =
      (VkPhysicalDeviceVulkan12Features *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);

  // VkPhysicalDeviceFeatures2 takes priority
  if(enabledFeatures2)
    enabledFeatures = enabledFeatures2->features;
  else if(createInfo.pEnabledFeatures)
    enabledFeatures = *createInfo.pEnabledFeatures;

  // enable this feature as it's needed at capture time to save MSAA initial states
  if(availFeatures.shaderStorageImageWriteWithoutFormat)
    enabledFeatures.shaderStorageImageWriteWithoutFormat = true;
  else
    RDCWARN(
        "shaderStorageImageWriteWithoutFormat = false, multisampled textures will have empty "
        "contents at frame start.");

  // even though we don't actually do any multisampled stores, this is needed to be able to create
  // MSAA images with STORAGE_BIT usage
  if(availFeatures.shaderStorageImageMultisample)
    enabledFeatures.shaderStorageImageMultisample = true;
  else
    RDCWARN(
        "shaderStorageImageMultisample = false, multisampled textures will have empty "
        "contents at frame start.");

  // patch the enabled features
  if(enabledFeatures2)
    enabledFeatures2->features = enabledFeatures;
  else
    createInfo.pEnabledFeatures = &enabledFeatures;

  // we need to enable non-subsampled images because we're going to remove subsampled bit from
  // images, and we want to ensure that it's legal! We verified that this is OK before whitelisting
  // the extension

  VkPhysicalDeviceFragmentDensityMapFeaturesEXT *fragmentDensityMapFeatures =
      (VkPhysicalDeviceFragmentDensityMapFeaturesEXT *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT);
  if(fragmentDensityMapFeatures && !fragmentDensityMapFeatures->fragmentDensityMapNonSubsampledImages)
  {
    fragmentDensityMapFeatures->fragmentDensityMapNonSubsampledImages = true;
  }

  VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *bufferAddressFeaturesEXT =
      (VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT);
  VkPhysicalDeviceBufferDeviceAddressFeatures *bufferAddressFeaturesCoreKHR =
      (VkPhysicalDeviceBufferDeviceAddressFeatures *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);

  // we must turn on bufferDeviceAddressCaptureReplay. We verified that this feature was available
  // before we whitelisted the extension/feature
  if(enabledFeaturesVK12 && enabledFeaturesVK12->bufferDeviceAddress)
    enabledFeaturesVK12->bufferDeviceAddressCaptureReplay = VK_TRUE;

  if(bufferAddressFeaturesCoreKHR)
    bufferAddressFeaturesCoreKHR->bufferDeviceAddressCaptureReplay = VK_TRUE;

  if(bufferAddressFeaturesEXT)
    bufferAddressFeaturesEXT->bufferDeviceAddressCaptureReplay = VK_TRUE;

  // check features that we care about at capture time

  const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *separateDepthStencilFeatures =
      (const VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES);

  if(separateDepthStencilFeatures)
    m_SeparateDepthStencil |= (separateDepthStencilFeatures->separateDepthStencilLayouts != VK_FALSE);

  VkResult ret;
  SERIALISE_TIME_CALL(ret = createFunc(Unwrap(physicalDevice), &createInfo, pAllocator, pDevice));

  if(ret == VK_SUCCESS)
  {
    InitDeviceTable(*pDevice, gdpa);

    RDCLOG("Created capture device from physical device %d",
           m_PhysicalDevices.indexOf(physicalDevice));

    ResourceId id = GetResourceManager()->WrapResource(*pDevice, *pDevice);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      VkDeviceCreateInfo serialiseCreateInfo = *pCreateInfo;

      // don't serialise out any of the pNext stuff for layer initialisation
      RemoveNextStruct(&serialiseCreateInfo, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDevice);
        Serialise_vkCreateDevice(ser, physicalDevice, &serialiseCreateInfo, NULL, pDevice);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDevice);
      RDCASSERT(record);

      record->AddChunk(chunk);

      record->instDevInfo = new InstanceDeviceInfo();

      record->instDevInfo->brokenGetDeviceProcAddr =
          GetRecord(m_Instance)->instDevInfo->brokenGetDeviceProcAddr;

      VkPhysicalDeviceProperties physProps;
      ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &physProps);

      record->instDevInfo->vulkanVersion = physProps.apiVersion;

#undef CheckExt
#define CheckExt(name, ver) \
  record->instDevInfo->ext_##name = GetRecord(m_Instance)->instDevInfo->ext_##name;

      // inherit extension enablement from instance, that way GetDeviceProcAddress can check
      // for enabled extensions for instance functions
      CheckInstanceExts();

// we unset the extension because it may be a 'shared' extension that's available at both instance
// and device. Only set it to enabled if it's really enabled for this device. This can happen with a
// device extension that is reported by another physical device than the one selected - it becomes
// available at instance level (e.g. for physical device queries) but is not available at *this*
// device level.
#undef CheckExt
#define CheckExt(name, ver) record->instDevInfo->ext_##name = false;

      CheckDeviceExts();

#undef CheckExt
#define CheckExt(name, ver)                                         \
  if(!strcmp(createInfo.ppEnabledExtensionNames[i], "VK_" #name) || \
     record->instDevInfo->vulkanVersion >= ver)                     \
  {                                                                 \
    record->instDevInfo->ext_##name = true;                         \
  }

      for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
      {
        CheckDeviceExts();
      }

      m_EnabledExtensions = *record->instDevInfo;

      InitDeviceExtensionTables(*pDevice, record->instDevInfo);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pDevice);
    }

    VkDevice device = *pDevice;

    RDCASSERT(m_Device == VK_NULL_HANDLE);    // MULTIDEVICE

    m_PhysicalDevice = physicalDevice;
    m_Device = device;

    m_QueueFamilyIdx = qFamilyIdx;

    if(m_InternalCmds.cmdpool == VK_NULL_HANDLE)
    {
      VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
                                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          qFamilyIdx};
      vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL,
                                               &m_InternalCmds.cmdpool);
      CheckVkResult(vkr);

      GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.cmdpool);
    }

    // for each queue family that isn't our own, create a command pool and command buffer on that
    // queue
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      uint32_t qidx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
      m_ExternalQueues.resize(RDCMAX((uint32_t)m_ExternalQueues.size(), qidx + 1));

      ImageBarrierSequence::SetMaxQueueFamilyIndex(qidx);

      VkCommandPoolCreateInfo poolInfo = {
          VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qidx,
      };
      vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL,
                                               &m_ExternalQueues[qidx].pool);
      CheckVkResult(vkr);

      GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].pool);

      VkCommandBufferAllocateInfo cmdInfo = {
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          NULL,
          Unwrap(m_ExternalQueues[qidx].pool),
          VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          1,
      };

      VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL,
                                     VK_FENCE_CREATE_SIGNALED_BIT};
      VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

      for(size_t x = 0; x < ARRAY_COUNT(m_ExternalQueues[i].ring); x++)
      {
        vkr = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &cmdInfo,
                                                      &m_ExternalQueues[qidx].ring[x].acquire);
        CheckVkResult(vkr);

        if(m_SetDeviceLoaderData)
          m_SetDeviceLoaderData(device, m_ExternalQueues[qidx].ring[x].acquire);
        else
          SetDispatchTableOverMagicNumber(device, m_ExternalQueues[qidx].ring[x].acquire);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].acquire);

        vkr = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &cmdInfo,
                                                      &m_ExternalQueues[qidx].ring[x].release);
        CheckVkResult(vkr);

        if(m_SetDeviceLoaderData)
          m_SetDeviceLoaderData(device, m_ExternalQueues[qidx].ring[x].release);
        else
          SetDispatchTableOverMagicNumber(device, m_ExternalQueues[qidx].ring[x].release);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].release);

        vkr = ObjDisp(device)->CreateSemaphore(Unwrap(device), &semInfo, NULL,
                                               &m_ExternalQueues[qidx].ring[x].fromext);
        CheckVkResult(vkr);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].fromext);

        vkr = ObjDisp(device)->CreateSemaphore(Unwrap(device), &semInfo, NULL,
                                               &m_ExternalQueues[qidx].ring[x].toext);
        CheckVkResult(vkr);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].toext);

        vkr = ObjDisp(device)->CreateFence(Unwrap(device), &fenceInfo, NULL,
                                           &m_ExternalQueues[qidx].ring[x].fence);
        CheckVkResult(vkr);

        GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].ring[x].fence);
      }
    }

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.availFeatures);
    m_PhysicalDeviceData.enabledFeatures = enabledFeatures;

    GetPhysicalDeviceDriverProperties(ObjDisp(physicalDevice), Unwrap(physicalDevice),
                                      m_PhysicalDeviceData.driverProps);

    m_PhysicalDeviceData.driverInfo =
        VkDriverInfo(m_PhysicalDeviceData.props, m_PhysicalDeviceData.driverProps, true);

    // hack for steamdeck, set soft memory limit to 200MB if it's not specified
    if(m_PhysicalDeviceData.driverProps.driverID == VK_DRIVER_ID_MESA_RADV &&
       m_PhysicalDeviceData.props.vendorID == 0x1002 && m_PhysicalDeviceData.props.deviceID == 0x163F)
    {
      CaptureOptions opts = RenderDoc::Inst().GetCaptureOptions();
      if(opts.softMemoryLimit == 0)
        opts.softMemoryLimit = 200;
      RenderDoc::Inst().SetCaptureOptions(opts);
      RDCLOG("Forcing 200MB soft memory limit");
    }

    ChooseMemoryIndices();

    m_PhysicalDeviceData.queueCount = (uint32_t)queueProps.size();
    for(size_t i = 0; i < queueProps.size(); i++)
      m_PhysicalDeviceData.queueProps[i] = queueProps[i];

    m_ShaderCache = new VulkanShaderCache(this);

    m_TextRenderer = new VulkanTextRenderer(this);

    m_DebugManager = new VulkanDebugManager(this);
  }

  FirstFrame();

  return ret;
}

void WrappedVulkan::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
  if(device == VK_NULL_HANDLE)
    return;

  if(m_MemoryFreeThread)
  {
    Threading::JoinThread(m_MemoryFreeThread);
    Threading::CloseThread(m_MemoryFreeThread);
    m_MemoryFreeThread = 0;
  }

  // flush out any pending commands/semaphores
  SubmitCmds();
  SubmitSemaphores();
  FlushQ();

  // idle the device as well so that external queues are idle.
  VkResult vkr = ObjDisp(m_Device)->DeviceWaitIdle(Unwrap(m_Device));
  CheckVkResult(vkr);

  // MULTIDEVICE this function will need to check if the device is the one we
  // used for debugmanager/cmd pool etc, and only remove child queues and
  // resources (instead of doing full resource manager shutdown).
  // Or will we have a debug manager per-device?
  RDCASSERT(m_Device == device);

  // delete all debug manager objects
  SAFE_DELETE(m_DebugManager);
  SAFE_DELETE(m_ShaderCache);
  SAFE_DELETE(m_TextRenderer);

  // since we didn't create proper registered resources for our command buffers,
  // they won't be taken down properly with the pool. So we release them (just our
  // data) here.
  for(size_t i = 0; i < m_InternalCmds.freecmds.size(); i++)
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freecmds[i]);

  if(m_IndirectCommandBuffer != VK_NULL_HANDLE)
    GetResourceManager()->ReleaseWrappedResource(m_IndirectCommandBuffer);

  // destroy our command pool
  if(m_InternalCmds.cmdpool != VK_NULL_HANDLE)
  {
    ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.cmdpool), NULL);
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.cmdpool);
  }

  for(size_t i = 0; i < m_InternalCmds.freesems.size(); i++)
  {
    ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device), Unwrap(m_InternalCmds.freesems[i]), NULL);
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freesems[i]);
  }

  for(size_t i = 0; i < m_ExternalQueues.size(); i++)
  {
    if(m_ExternalQueues[i].pool != VK_NULL_HANDLE)
    {
      for(size_t x = 0; x < ARRAY_COUNT(m_ExternalQueues[i].ring); x++)
      {
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].acquire);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].release);

        ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device),
                                            Unwrap(m_ExternalQueues[i].ring[x].fromext), NULL);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].fromext);

        ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device),
                                            Unwrap(m_ExternalQueues[i].ring[x].toext), NULL);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].toext);

        ObjDisp(m_Device)->DestroyFence(Unwrap(m_Device), Unwrap(m_ExternalQueues[i].ring[x].fence),
                                        NULL);
        GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].ring[x].fence);
      }

      ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_ExternalQueues[i].pool), NULL);
      GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].pool);
    }
  }

  m_InternalCmds.Reset();

  m_QueueFamilyIdx = ~0U;
  m_PrevQueue = m_Queue = VK_NULL_HANDLE;

  m_QueueFamilies.clear();
  m_QueueFamilyCounts.clear();
  m_QueueFamilyIndices.clear();
  m_ExternalQueues.clear();

  // destroy the API device immediately. There should be no more
  // resources left in the resource manager device/physical device/instance.
  // Anything we created should be gone and anything the application created
  // should be deleted by now.
  // If there were any leaks, we will leak them ourselves in vkDestroyInstance
  // rather than try to delete API objects after the device has gone
  ObjDisp(m_Device)->DestroyDevice(Unwrap(m_Device), pAllocator);
  GetResourceManager()->ReleaseWrappedResource(m_Device);
  m_Device = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkDeviceWaitIdle(SerialiserType &ser, VkDevice device)
{
  SERIALISE_ELEMENT(device).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkDeviceWaitIdle(VkDevice device)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->DeviceWaitIdle(Unwrap(device)));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkDeviceWaitIdle);
    Serialise_vkDeviceWaitIdle(ser, device);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkEnumeratePhysicalDevices, VkInstance instance,
                                uint32_t *pPhysicalDeviceCount, VkPhysicalDevice *pPhysicalDevices);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDevice, VkPhysicalDevice physicalDevice,
                                const VkDeviceCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkDeviceWaitIdle, VkDevice device);
