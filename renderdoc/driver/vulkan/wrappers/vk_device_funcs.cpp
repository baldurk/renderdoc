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

#include "../vk_core.h"
#include "../vk_debug.h"
#include "../vk_rendertext.h"
#include "../vk_shader_cache.h"
#include "api/replay/version.h"
#include "strings/string_utils.h"

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

static void StripUnwantedLayers(std::vector<std::string> &Layers)
{
  for(auto it = Layers.begin(); it != Layers.end();)
  {
    // don't try and create our own layer on replay!
    if(*it == RENDERDOC_VULKAN_LAYER_NAME)
    {
      it = Layers.erase(it);
      continue;
    }

    // don't enable tracing or dumping layers just in case they
    // came along with the application
    if(*it == "VK_LAYER_LUNARG_api_dump" || *it == "VK_LAYER_LUNARG_vktrace")
    {
      it = Layers.erase(it);
      continue;
    }

    // also remove the framerate monitor layer as it's buggy and doesn't do anything
    // in our case
    if(*it == "VK_LAYER_LUNARG_monitor")
    {
      it = Layers.erase(it);
      continue;
    }

    // remove the optimus layer just in case it was explicitly enabled.
    if(*it == "VK_LAYER_NV_optimus")
    {
      it = Layers.erase(it);
      continue;
    }

    // filter out validation layers
    if(*it == "VK_LAYER_LUNARG_standard_validation" || *it == "VK_LAYER_KHRONOS_validation" ||
       *it == "VK_LAYER_LUNARG_core_validation" || *it == "VK_LAYER_LUNARG_device_limits" ||
       *it == "VK_LAYER_LUNARG_image" || *it == "VK_LAYER_LUNARG_object_tracker" ||
       *it == "VK_LAYER_LUNARG_parameter_validation" || *it == "VK_LAYER_LUNARG_swapchain" ||
       *it == "VK_LAYER_GOOGLE_threading" || *it == "VK_LAYER_GOOGLE_unique_objects" ||
       *it == "VK_LAYER_LUNARG_assistant_layer")
    {
      it = Layers.erase(it);
      continue;
    }

    ++it;
  }
}

static void StripUnwantedExtensions(std::vector<std::string> &Extensions)
{
  // strip out any WSI/direct display extensions. We'll add the ones we want for creating windows
  // on the current platforms below, and we don't replay any of the WSI functionality
  // directly so these extensions aren't needed
  for(auto it = Extensions.begin(); it != Extensions.end();)
  {
    // remove surface extensions
    if(*it == "VK_KHR_xlib_surface" || *it == "VK_KHR_xcb_surface" ||
       *it == "VK_KHR_wayland_surface" || *it == "VK_KHR_mir_surface" ||
       *it == "VK_MVK_macos_surface" || *it == "VK_KHR_android_surface" ||
       *it == "VK_KHR_win32_surface" || *it == "VK_GGP_stream_descriptor_surface")
    {
      it = Extensions.erase(it);
      continue;
    }

    // remove direct display extensions
    if(*it == "VK_KHR_display" || *it == "VK_EXT_direct_mode_display" ||
       *it == "VK_EXT_acquire_xlib_display" || *it == "VK_EXT_display_surface_counter")
    {
      it = Extensions.erase(it);
      continue;
    }

    ++it;
  }
}

ReplayStatus WrappedVulkan::Initialise(VkInitParams &params, uint64_t sectionVersion)
{
  m_InitParams = params;
  m_SectionVersion = sectionVersion;

  StripUnwantedLayers(params.Layers);
  StripUnwantedExtensions(params.Extensions);

#if ENABLED(FORCE_VALIDATION_LAYERS) && DISABLED(RDOC_ANDROID)
  params.Layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

  std::set<std::string> supportedLayers;

  {
    uint32_t count = 0;
    GetInstanceDispatchTable(NULL)->EnumerateInstanceLayerProperties(&count, NULL);

    VkLayerProperties *props = new VkLayerProperties[count];
    GetInstanceDispatchTable(NULL)->EnumerateInstanceLayerProperties(&count, props);

    for(uint32_t e = 0; e < count; e++)
      supportedLayers.insert(props[e].layerName);

    SAFE_DELETE_ARRAY(props);
  }

  // complain about any missing layers, but remove them from the list and continue
  for(auto it = params.Layers.begin(); it != params.Layers.end();)
  {
    if(supportedLayers.find(*it) == supportedLayers.end())
    {
      RDCERR("Capture used layer '%s' which is not available, continuing without it", it->c_str());
      it = params.Layers.erase(it);
      continue;
    }

    ++it;
  }

  std::set<std::string> supportedExtensions;

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

  AddRequiredExtensions(true, params.Extensions, supportedExtensions);

  // after 1.0, VK_KHR_get_physical_device_properties2 is promoted to core, but enable it if it's
  // reported as available, just in case.
  if(params.APIVersion >= VK_API_VERSION_1_0)
  {
    if(supportedExtensions.find(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      if(std::find(params.Extensions.begin(), params.Extensions.end(),
                   VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == params.Extensions.end())
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
      if(std::find(params.Extensions.begin(), params.Extensions.end(),
                   VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == params.Extensions.end())
        params.Extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }
  }

  // verify that extensions are supported
  for(size_t i = 0; i < params.Extensions.size(); i++)
  {
    if(supportedExtensions.find(params.Extensions[i]) == supportedExtensions.end())
    {
      RDCERR("Capture requires extension '%s' which is not supported", params.Extensions[i].c_str());
      return ReplayStatus::APIHardwareUnsupported;
    }
  }

  // we always want debug extensions if it available, and not already enabled
  if(supportedExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedExtensions.end() &&
     std::find(params.Extensions.begin(), params.Extensions.end(),
               VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == params.Extensions.end())
  {
    RDCLOG("Enabling VK_EXT_debug_utils");
    params.Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  else if(supportedExtensions.find(VK_EXT_DEBUG_REPORT_EXTENSION_NAME) != supportedExtensions.end() &&
          std::find(params.Extensions.begin(), params.Extensions.end(),
                    VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == params.Extensions.end())
  {
    RDCLOG("Enabling VK_EXT_debug_report");
    params.Extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  const char **layerscstr = new const char *[params.Layers.size()];
  for(size_t i = 0; i < params.Layers.size(); i++)
    layerscstr[i] = params.Layers[i].c_str();

  const char **extscstr = new const char *[params.Extensions.size()];
  for(size_t i = 0; i < params.Extensions.size(); i++)
    extscstr[i] = params.Extensions[i].c_str();

  VkInstanceCreateInfo instinfo = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      NULL,
      0,
      &renderdocAppInfo,
      (uint32_t)params.Layers.size(),
      layerscstr,
      (uint32_t)params.Extensions.size(),
      extscstr,
  };

  if(params.APIVersion >= VK_API_VERSION_1_0)
    renderdocAppInfo.apiVersion = params.APIVersion;

  m_Instance = VK_NULL_HANDLE;

  VkValidationFeaturesEXT featuresEXT = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  VkValidationFeatureDisableEXT disableFeatures[] = {VK_VALIDATION_FEATURE_DISABLE_SHADERS_EXT};
  featuresEXT.disabledValidationFeatureCount = ARRAY_COUNT(disableFeatures);
  featuresEXT.pDisabledValidationFeatures = disableFeatures;

  VkValidationFlagsEXT flagsEXT = {VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT};
  VkValidationCheckEXT disableChecks[] = {VK_VALIDATION_CHECK_SHADERS_EXT};
  flagsEXT.disabledValidationCheckCount = ARRAY_COUNT(disableChecks);
  flagsEXT.pDisabledValidationChecks = disableChecks;

  if(supportedExtensions.find(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) != supportedExtensions.end() &&
     std::find(params.Extensions.begin(), params.Extensions.end(),
               VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == params.Extensions.end())
  {
    RDCLOG("Enabling VK_EXT_validation_features");
    params.Extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

    instinfo.pNext = &featuresEXT;
  }
  else if(supportedExtensions.find(VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME) !=
              supportedExtensions.end() &&
          std::find(params.Extensions.begin(), params.Extensions.end(),
                    VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME) == params.Extensions.end())
  {
    RDCLOG("Enabling VK_EXT_validation_flags");
    params.Extensions.push_back(VK_EXT_VALIDATION_FLAGS_EXTENSION_NAME);

    instinfo.pNext = &flagsEXT;
  }

  VkResult ret = GetInstanceDispatchTable(NULL)->CreateInstance(&instinfo, NULL, &m_Instance);

#undef CheckExt
#define CheckExt(name, ver)                                       \
  if(!strcmp(instinfo.ppEnabledExtensionNames[i], "VK_" #name) || \
     (int)renderdocAppInfo.apiVersion >= ver)                     \
  {                                                               \
    m_EnabledExtensions.ext_##name = true;                        \
  }

  for(uint32_t i = 0; i < instinfo.enabledExtensionCount; i++)
  {
    CheckInstanceExts();
  }

  SAFE_DELETE_ARRAY(layerscstr);
  SAFE_DELETE_ARRAY(extscstr);

  if(ret != VK_SUCCESS)
    return ReplayStatus::APIHardwareUnsupported;

  RDCASSERTEQUAL(ret, VK_SUCCESS);

  GetResourceManager()->WrapResource(m_Instance, m_Instance);

  // we'll add the chunk later when we re-process it.
  if(params.InstanceID != ResourceId())
  {
    GetResourceManager()->AddLiveResource(params.InstanceID, m_Instance);

    AddResource(params.InstanceID, ResourceType::Device, "Instance");
    GetReplay()->GetResourceDesc(params.InstanceID).initialisationChunks.clear();
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
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugInfo.pfnCallback = &DebugReportCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgReportCallback);
  }

  uint32_t count = 0;

  VkResult vkr = ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, NULL);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(count == 0)
    return ReplayStatus::APIHardwareUnsupported;

  m_ReplayPhysicalDevices.resize(count);
  m_ReplayPhysicalDevicesUsed.resize(count);
  m_OriginalPhysicalDevices.resize(count);
  m_MemIdxMaps.resize(count);

  vkr = ObjDisp(m_Instance)
            ->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, &m_ReplayPhysicalDevices[0]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t i = 0; i < count; i++)
    GetResourceManager()->WrapResource(m_Instance, m_ReplayPhysicalDevices[i]);

  return ReplayStatus::Succeeded;
}

VkResult WrappedVulkan::vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                         const VkAllocationCallbacks *pAllocator,
                                         VkInstance *pInstance)
{
  RDCASSERT(pCreateInfo);

  // don't support any extensions for this createinfo
  RDCASSERT(pCreateInfo->pApplicationInfo == NULL || pCreateInfo->pApplicationInfo->pNext == NULL);

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

      while(report)
      {
        if(report && report->sType == VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT)
          report->pfnCallback(VK_DEBUG_REPORT_ERROR_BIT_EXT,
                              VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT, 0, 1, 1, "RDOC",
                              "RenderDoc does not support a requested instance extension.",
                              report->pUserData);

        report = (VkDebugReportCallbackCreateInfoEXT *)report->pNext;
      }

      // or debug utils callbacks
      VkDebugUtilsMessengerCreateInfoEXT *messenger =
          (VkDebugUtilsMessengerCreateInfoEXT *)pCreateInfo->pNext;

      VkDebugUtilsMessengerCallbackDataEXT messengerData = {};

      messengerData.messageIdNumber = 1;
      messengerData.pMessageIdName = NULL;
      messengerData.pMessage = "RenderDoc does not support a requested instance extension.";
      messengerData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;

      while(messenger)
      {
        if(messenger && messenger->sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT)
          messenger->pfnUserCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &messengerData,
                                     messenger->pUserData);

        messenger = (VkDebugUtilsMessengerCreateInfoEXT *)messenger->pNext;
      }

      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  const char **addedExts = new const char *[modifiedCreateInfo.enabledExtensionCount + 1];

  bool hasDebugReport = false, hasDebugUtils = false;

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
  {
    addedExts[i] = modifiedCreateInfo.ppEnabledExtensionNames[i];
    if(!strcmp(addedExts[i], VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
      hasDebugReport = true;
    if(!strcmp(addedExts[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
      hasDebugUtils = true;
  }

  std::vector<VkExtensionProperties> supportedExts;

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

  bool brokenGetDeviceProcAddr = false;

  // override applicationInfo with RenderDoc's, but preserve apiVersion
  if(modifiedCreateInfo.pApplicationInfo)
  {
    if(modifiedCreateInfo.pApplicationInfo->pEngineName &&
       strlower(modifiedCreateInfo.pApplicationInfo->pEngineName) == "idtech")
      brokenGetDeviceProcAddr = true;

    if(modifiedCreateInfo.pApplicationInfo->apiVersion >= VK_API_VERSION_1_0)
      renderdocAppInfo.apiVersion = modifiedCreateInfo.pApplicationInfo->apiVersion;

    modifiedCreateInfo.pApplicationInfo = &renderdocAppInfo;
  }

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledLayerCount; i++)
  {
    if(!strcmp(modifiedCreateInfo.ppEnabledLayerNames[i], "VK_LAYER_LUNARG_standard_validation") ||
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

  if(renderdocAppInfo.apiVersion > VK_API_VERSION_1_0)
    record->instDevInfo->vulkanVersion = renderdocAppInfo.apiVersion;

  std::set<std::string> availablePhysDeviceFunctions;

  {
    uint32_t count = 0;
    ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, NULL);

    std::vector<VkPhysicalDevice> physDevs(count);
    ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, physDevs.data());

    std::vector<VkExtensionProperties> exts;
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
  if(!strcmp(modifiedCreateInfo.ppEnabledExtensionNames[i], "VK_" #name) ||                \
     record->instDevInfo->vulkanVersion >= ver ||                                          \
     availablePhysDeviceFunctions.find("VK_" #name) != availablePhysDeviceFunctions.end()) \
  {                                                                                        \
    record->instDevInfo->ext_##name = true;                                                \
  }

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
  {
    CheckInstanceExts();
  }

  delete[] addedExts;

  InitInstanceExtensionTables(m_Instance, record->instDevInfo);

  RenderDoc::Inst().AddDeviceFrameCapturer(LayerDisp(m_Instance), this);

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
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
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

  // destroy any events we created for waiting on
  for(size_t i = 0; i < m_PersistentEvents.size(); i++)
    ObjDisp(GetDev())->DestroyEvent(Unwrap(GetDev()), m_PersistentEvents[i], NULL);

  m_PersistentEvents.clear();

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
    if(m_ExternalQueues[i].buffer != VK_NULL_HANDLE)
    {
      GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].buffer);

      ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_ExternalQueues[i].pool), NULL);
      GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].pool);
    }
  }

  FreeAllMemory(MemoryScope::InitialContents);

  // we do more in Shutdown than the equivalent vkDestroyInstance since on replay there's
  // no explicit vkDestroyDevice, we destroy the device here then the instance

  // destroy the physical devices manually because due to remapping the may have leftover
  // refcounts
  for(size_t i = 0; i < m_ReplayPhysicalDevices.size(); i++)
    GetResourceManager()->ReleaseWrappedResource(m_ReplayPhysicalDevices[i]);

  m_Replay.DestroyResources();

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

  const VkLayerDispatchTable *vt = m_Device != VK_NULL_HANDLE ? ObjDisp(m_Device) : NULL;
  const VkLayerInstanceDispatchTable *vit = m_Instance != VK_NULL_HANDLE ? ObjDisp(m_Instance) : NULL;

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
      .TypedAs("VkPhysicalDevice"_lit);

  uint32_t memIdxMap[VK_MAX_MEMORY_TYPES] = {0};
  // not used at the moment but useful for reference and might be used
  // in the future
  VkPhysicalDeviceProperties physProps = {};
  VkPhysicalDeviceMemoryProperties memProps = {};
  VkPhysicalDeviceFeatures physFeatures = {};
  uint32_t queueCount = 0;
  VkQueueFamilyProperties queueProps[16] = {};

  VkPhysicalDeviceDriverPropertiesKHR driverProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
  };

  if(ser.IsWriting())
  {
    memcpy(memIdxMap, GetRecord(*pPhysicalDevices)->memIdxMap, sizeof(memIdxMap));

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

    if(GetExtensions(GetRecord(instance)).ext_KHR_get_physical_device_properties2)
    {
      uint32_t count = 0;
      ObjDisp(*pPhysicalDevices)
          ->EnumerateDeviceExtensionProperties(Unwrap(*pPhysicalDevices), NULL, &count, NULL);

      VkExtensionProperties *props = new VkExtensionProperties[count];
      ObjDisp(*pPhysicalDevices)
          ->EnumerateDeviceExtensionProperties(Unwrap(*pPhysicalDevices), NULL, &count, props);

      for(uint32_t e = 0; e < count; e++)
      {
        if(!strcmp(props[e].extensionName, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
        {
          VkPhysicalDeviceProperties2 physProps2 = {
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
          };

          physProps2.pNext = &driverProps;
          ObjDisp(instance)->GetPhysicalDeviceProperties2(Unwrap(*pPhysicalDevices), &physProps2);
          break;
        }
      }

      SAFE_DELETE_ARRAY(props);
    }
  }

  SERIALISE_ELEMENT(memIdxMap);
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
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].memProps = memProps;
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].features = physFeatures;
      m_OriginalPhysicalDevices[PhysicalDeviceIndex].queueCount = queueCount;
      memcpy(m_OriginalPhysicalDevices[PhysicalDeviceIndex].queueProps, queueProps,
             sizeof(queueProps));
    }

    // match up physical devices to those available on replay as best as possible. In general
    // hopefully the most common case is when there's a precise match, and maybe the order changed.
    //
    // If more GPUs were present on replay than during capture, we map many-to-one which might have
    // bad side-effects as e.g. we have to pick one memidxmap, but this is as good as we can do.

    uint32_t bestIdx = 0;
    VkPhysicalDeviceProperties bestPhysProps = {};
    VkPhysicalDeviceDriverPropertiesKHR bestDriverProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
    };

    for(uint32_t i = 0; i < (uint32_t)m_ReplayPhysicalDevices.size(); i++)
    {
      VkPhysicalDeviceProperties compPhysProps = {};
      VkPhysicalDeviceDriverPropertiesKHR compDriverProps = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
      };

      pd = m_ReplayPhysicalDevices[i];

      // find the best possible match for this physical device
      ObjDisp(pd)->GetPhysicalDeviceProperties(Unwrap(pd), &compPhysProps);

      if(m_EnabledExtensions.ext_KHR_get_physical_device_properties2)
      {
        uint32_t count = 0;
        ObjDisp(pd)->EnumerateDeviceExtensionProperties(Unwrap(pd), NULL, &count, NULL);

        VkExtensionProperties *props = new VkExtensionProperties[count];
        ObjDisp(pd)->EnumerateDeviceExtensionProperties(Unwrap(pd), NULL, &count, props);

        for(uint32_t e = 0; e < count; e++)
        {
          if(!strcmp(props[e].extensionName, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
          {
            VkPhysicalDeviceProperties2 physProps2 = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            };

            physProps2.pNext = &compDriverProps;
            ObjDisp(pd)->GetPhysicalDeviceProperties2(Unwrap(pd), &physProps2);
            break;
          }
        }

        SAFE_DELETE_ARRAY(props);
      }

      if(firstTime)
      {
        VkDriverInfo runningVersion(compPhysProps);

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

      // the first is the best at the start
      if(i == 0)
      {
        bestPhysProps = compPhysProps;
        bestDriverProps = compDriverProps;
        continue;
      }

      // an exact vendorID match is a better match than not
      if(compPhysProps.vendorID == physProps.vendorID && bestPhysProps.vendorID != physProps.vendorID)
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
      if(compPhysProps.deviceID == physProps.deviceID && bestPhysProps.deviceID != physProps.deviceID)
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

    {
      VkDriverInfo capturedVersion(physProps);

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

      RDCLOG("Mapping during replay to best-match physical device %u", bestIdx);
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
    else if(m_MemIdxMaps[bestIdx] == NULL)
    {
      // the first physical device 'wins' for the memory index map
      uint32_t *storedMap = new uint32_t[32];
      memcpy(storedMap, memIdxMap, sizeof(memIdxMap));

      for(uint32_t i = 0; i < 32; i++)
        storedMap[i] = i;

      m_MemIdxMaps[bestIdx] = storedMap;
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
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

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

        record->memProps = new VkPhysicalDeviceMemoryProperties();

        ObjDisp(devices[i])->GetPhysicalDeviceMemoryProperties(Unwrap(devices[i]), record->memProps);

        VkPhysicalDeviceProperties physProps;

        ObjDisp(devices[i])->GetPhysicalDeviceProperties(Unwrap(devices[i]), &physProps);

        VkDriverInfo capturedVersion(physProps);

        RDCLOG("physical device %u: %s (ver %u.%u patch 0x%x) - %04x:%04x", i, physProps.deviceName,
               capturedVersion.Major(), capturedVersion.Minor(), capturedVersion.Patch(),
               physProps.vendorID, physProps.deviceID);

        m_PhysicalDevices[i] = devices[i];

        // we remap memory indices to discourage coherent maps as much as possible
        RemapMemoryIndices(record->memProps, &record->memIdxMap);

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkEnumeratePhysicalDevices);
          Serialise_vkEnumeratePhysicalDevices(ser, instance, &i, &devices[i]);

          record->AddChunk(scope.Get());
        }

        VkResourceRecord *instrecord = GetRecord(instance);

        instrecord->AddParent(record);

        // treat physical devices as pool members of the instance (ie. freed when the instance dies)
        {
          instrecord->LockChunks();
          instrecord->pooledChildren.push_back(record);
          instrecord->UnlockChunks();
        }
      }
    }
  }

  if(pPhysicalDeviceCount)
    *pPhysicalDeviceCount = count;
  if(pPhysicalDevices)
    memcpy(pPhysicalDevices, devices, count * sizeof(VkPhysicalDevice));

  SAFE_DELETE_ARRAY(devices);

  return VK_SUCCESS;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDevice(SerialiserType &ser, VkPhysicalDevice physicalDevice,
                                             const VkDeviceCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkDevice *pDevice)
{
  SERIALISE_ELEMENT(physicalDevice);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
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

    // we must make any modifications locally, so the free of pointers
    // in the serialised VkDeviceCreateInfo don't double-free
    VkDeviceCreateInfo createInfo = CreateInfo;

    std::vector<std::string> Extensions;
    for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
      // don't include the debug marker extension
      if(!strcmp(createInfo.ppEnabledExtensionNames[i], VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        continue;

      // don't include the validation cache extension
      if(!strcmp(createInfo.ppEnabledExtensionNames[i], VK_EXT_VALIDATION_CACHE_EXTENSION_NAME))
        continue;

      // don't include direct-display WSI extensions
      if(!strcmp(createInfo.ppEnabledExtensionNames[i], VK_KHR_DISPLAY_SWAPCHAIN_EXTENSION_NAME) ||
         !strcmp(createInfo.ppEnabledExtensionNames[i], VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME))
        continue;

      Extensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    }

    if(std::find(Extensions.begin(), Extensions.end(),
                 VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_EXTENSION_NAME) != Extensions.end())
      m_ExtensionsEnabled[VkCheckExt_AMD_neg_viewport] = true;

    if(std::find(Extensions.begin(), Extensions.end(), VK_KHR_MAINTENANCE1_EXTENSION_NAME) !=
       Extensions.end())
      m_ExtensionsEnabled[VkCheckExt_KHR_maintenance1] = true;

    if(std::find(Extensions.begin(), Extensions.end(),
                 VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME) != Extensions.end())
      m_ExtensionsEnabled[VkCheckExt_EXT_conserv_rast] = true;

    if(std::find(Extensions.begin(), Extensions.end(),
                 VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME) != Extensions.end())
      m_ExtensionsEnabled[VkCheckExt_EXT_vertex_divisor] = true;

    std::vector<std::string> Layers;
    for(uint32_t i = 0; i < createInfo.enabledLayerCount; i++)
      Layers.push_back(createInfo.ppEnabledLayerNames[i]);

    StripUnwantedLayers(Layers);

    std::set<std::string> supportedExtensions;

    for(size_t i = 0; i <= Layers.size(); i++)
    {
      const char *pLayerName = (i == 0 ? NULL : Layers[i - 1].c_str());

      uint32_t count = 0;
      ObjDisp(physicalDevice)
          ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), pLayerName, &count, NULL);

      VkExtensionProperties *props = new VkExtensionProperties[count];
      ObjDisp(physicalDevice)
          ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), pLayerName, &count, props);

      for(uint32_t e = 0; e < count; e++)
        supportedExtensions.insert(props[e].extensionName);

      SAFE_DELETE_ARRAY(props);
    }

    AddRequiredExtensions(false, Extensions, supportedExtensions);

    for(size_t i = 0; i < Extensions.size(); i++)
    {
      if(supportedExtensions.find(Extensions[i]) == supportedExtensions.end())
      {
        m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported;
        RDCERR("Capture requires extension '%s' which is not supported", Extensions[i].c_str());
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

    if(supportedExtensions.find(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) !=
       supportedExtensions.end())
    {
      Extensions.push_back(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
      RDCLOG("Enabling VK_EXT_buffer_device_address");
    }
    else
    {
      RDCWARN(
          "VK_EXT_buffer_device_address not available, feedback from "
          "bindless shader access will use less reliable fallback");
    }

    VkDevice device;

    uint32_t qCount = 0;
    ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

    if(qCount > 16)
    {
      RDCERR("Unexpected number of queue families: %u", qCount);
      qCount = 16;
    }

    VkQueueFamilyProperties props[16] = {};
    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);

    // to aid the search algorithm below, we apply implied transfer bit onto the queue properties.
    for(uint32_t i = 0; i < qCount; i++)
    {
      if(props[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        props[i].queueFlags |= VK_QUEUE_TRANSFER_BIT;
    }

    PhysicalDeviceData &origData = m_OriginalPhysicalDevices[physicalDeviceIndex];

    uint32_t origQCount = origData.queueCount;
    VkQueueFamilyProperties *origprops = origData.queueProps;

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

          for(uint32_t replayQIndex = 0; replayQIndex < qCount; replayQIndex++)
          {
            // ignore queues that couldn't satisfy the required transfer granularity
            if(!CheckTransferGranularity(needGranularity,
                                         props[replayQIndex].minImageTransferGranularity))
              continue;

            // ignore queues that don't have sparse binding, if we need that
            if(needSparse && ((props[replayQIndex].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == 0))
              continue;

            switch(search)
            {
              case SearchType::Failed: break;
              case SearchType::Universal:
                if((props[replayQIndex].queueFlags & mask) ==
                   (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::GraphicsTransfer:
                if((props[replayQIndex].queueFlags & mask) ==
                   (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::ComputeTransfer:
                if((props[replayQIndex].queueFlags & mask) ==
                   (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::GraphicsOrComputeTransfer:
                if((props[replayQIndex].queueFlags & mask) ==
                       (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) ||
                   (props[replayQIndex].queueFlags & mask) ==
                       (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT))
                {
                  destFamily = replayQIndex;
                  found = true;
                }
                break;
              case SearchType::TransferOnly:
                if((props[replayQIndex].queueFlags & mask) == VK_QUEUE_TRANSFER_BIT)
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
      RDCLOG("   - %u queues available with %s", props[destFamily].queueCount,
             ToStr(VkQueueFlagBits(props[destFamily].queueFlags)).c_str());
      RDCLOG("     %u timestamp bits (%u,%u,%u) granularity", props[destFamily].timestampValidBits,
             props[destFamily].minImageTransferGranularity.width,
             props[destFamily].minImageTransferGranularity.height,
             props[destFamily].minImageTransferGranularity.depth);

      // loop over the queues, wrapping around if necessary to provide enough queues. The idea being
      // an application is more likely to use early queues than later ones, so if there aren't
      // enough queues in the family then we should prioritise giving unique queues to the early
      // indices
      for(uint32_t q = 0; q < origprops[origQIndex].queueCount; q++)
      {
        m_QueueRemapping[origQIndex][q] = {destFamily, q % props[destFamily].queueCount};
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
      uint32_t queueCount = RDCMIN(queueCreate.queueCount, props[queueFamily].queueCount);

      if(queueCount < queueCreate.queueCount)
        RDCWARN("Truncating queue family request from %u queues to %u queues",
                queueCreate.queueCount, queueCount);

      queueCreate.queueCount = queueCount;
    }

    // remove any duplicates that have been created
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

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

    bool found = false;
    uint32_t qFamilyIdx = 0;

    // we need graphics, and if there is a graphics queue there must be a graphics & compute queue.
    VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    // for queue priorities, if we need it
    float one = 1.0f;

    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      uint32_t idx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
      RDCASSERT(idx < qCount);

      // this requested queue is one we can use too
      if((props[idx].queueFlags & search) == search && createInfo.pQueueCreateInfos[i].queueCount > 0)
      {
        qFamilyIdx = idx;
        found = true;
        break;
      }
    }

    // if we didn't find it, search for which queue family we should add a request for
    if(!found)
    {
      RDCDEBUG("App didn't request a queue family we can use - adding our own");

      for(uint32_t i = 0; i < qCount; i++)
      {
        if((props[i].queueFlags & search) == search)
        {
          qFamilyIdx = i;
          found = true;
          break;
        }
      }

      if(!found)
      {
        RDCERR(
            "Can't add a queue with required properties for RenderDoc! Unsupported configuration");
      }
      else
      {
        // we found the queue family, add it
        VkDeviceQueueCreateInfo newQueue;

        newQueue.queueFamilyIndex = qFamilyIdx;
        newQueue.queueCount = 1;
        newQueue.pQueuePriorities = &one;

        queueInfos.push_back(newQueue);

        // reset these in case the vector resized
        createInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
        createInfo.pQueueCreateInfos = queueInfos.data();
      }
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

#define CHECK_PHYS_FEATURE(feature)                                                           \
  if(enabledFeatures.feature && !availFeatures.feature)                                       \
  {                                                                                           \
    m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported;                              \
    RDCERR("Capture requires physical device feature '" #feature "' which is not supported"); \
    return false;                                                                             \
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

#define CHECK_PHYS_EXT_FEATURE(feature)                          \
  if(ext->feature && !avail.feature)                             \
  {                                                              \
    m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported; \
    RDCERR("Capture requires physical device feature '" #feature \
           "' in struct '%s' which is not supported",            \
           structName);                                          \
    return false;                                                \
  }

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingFeatures = {};

    if(ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2)
    {
      BEGIN_PHYS_EXT_CHECK(VkPhysicalDevice8BitStorageFeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR);
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

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV);
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

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceShaderAtomicInt64FeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR);
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

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceVulkanMemoryModelFeaturesKHR,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR);
      {
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModel);
        CHECK_PHYS_EXT_FEATURE(vulkanMemoryModelDeviceScope);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceConditionalRenderingFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(conditionalRendering);
        CHECK_PHYS_EXT_FEATURE(inheritedConditionalRendering);
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceHostQueryResetFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT);
      {
        CHECK_PHYS_EXT_FEATURE(hostQueryReset);
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
          m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported;
          RDCERR(
              "Capture requires bufferDeviceAddress support, which is available, but "
              "bufferDeviceAddressCaptureReplay support is not available which is required to "
              "replay");
          return false;
        }
      }
      END_PHYS_EXT_CHECK();

      BEGIN_PHYS_EXT_CHECK(VkPhysicalDeviceDescriptorIndexingFeaturesEXT,
                           VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT);
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
    }

    if(availFeatures.depthClamp)
      enabledFeatures.depthClamp = true;
    else
      RDCWARN(
          "depthClamp = false, overlays like highlight drawcall won't show depth-clipped pixels.");

    if(availFeatures.fillModeNonSolid)
      enabledFeatures.fillModeNonSolid = true;

    // we have a fallback for this case, so no warning

    if(availFeatures.geometryShader)
      enabledFeatures.geometryShader = true;
    else
      RDCWARN(
          "geometryShader = false, lit mesh rendering will not be available if rendering on this "
          "device.");

    bool descIndexingAllowsRBA = true;

    if(descIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind ||
       descIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind ||
       descIndexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind ||
       descIndexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind)
    {
      // if any update after bind feature is enabled, check robustBufferAccessUpdateAfterBind
      VkPhysicalDeviceDescriptorIndexingPropertiesEXT descIndexingProps = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
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
      RDCWARN(
          "shaderInt64 = false, feedback from bindless shader access will use less reliable "
          "fallback.");

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
          "shaderStorageImageMultisample = false, save/load from 2DMS textures will not be "
          "possible");

    if(availFeatures.fragmentStoresAndAtomics)
      enabledFeatures.fragmentStoresAndAtomics = true;
    else
      RDCWARN("fragmentStoresAndAtomics = false, quad overdraw overlay will not be available");

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
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkExtensionProperties *exts = new VkExtensionProperties[numExts];

    vkr = ObjDisp(physicalDevice)
              ->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &numExts, exts);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    for(uint32_t i = 0; i < numExts; i++)
      RDCLOG("Ext %u: %s (%u)", i, exts[i].extensionName, exts[i].specVersion);

    SAFE_DELETE_ARRAY(exts);

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

        auto it = std::find(Extensions.begin(), Extensions.end(),
                            VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
        RDCASSERT(it != Extensions.end());
        Extensions.erase(it);
      }
    }

    std::vector<const char *> layerArray(Layers.size());
    for(size_t i = 0; i < Layers.size(); i++)
      layerArray[i] = Layers[i].c_str();

    createInfo.enabledLayerCount = (uint32_t)layerArray.size();
    createInfo.ppEnabledLayerNames = layerArray.data();

    std::vector<const char *> extArray(Extensions.size());
    for(size_t i = 0; i < Extensions.size(); i++)
      extArray[i] = Extensions[i].c_str();

    createInfo.enabledExtensionCount = (uint32_t)extArray.size();
    createInfo.ppEnabledExtensionNames = extArray.data();

    vkr = GetDeviceDispatchTable(NULL)->CreateDevice(Unwrap(physicalDevice), &createInfo, NULL,
                                                     &device);

    if(vkr != VK_SUCCESS)
    {
      RDCERR("Failed to create logical device: %s", ToStr(vkr).c_str());
      return false;
    }

    GetResourceManager()->WrapResource(device, device);
    GetResourceManager()->AddLiveResource(Device, device);

    AddResource(Device, ResourceType::Device, "Device");
    DerivedResource(origPhysDevice, Device);

#undef CheckExt
#define CheckExt(name, ver)                                         \
  if(!strcmp(createInfo.ppEnabledExtensionNames[i], "VK_" #name) || \
     (int)renderdocAppInfo.apiVersion >= ver)                       \
  {                                                                 \
    m_EnabledExtensions.ext_##name = true;                          \
  }

    for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
      CheckDeviceExts();
    }

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
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.cmdpool);
    }

    // for each queue family we've remapped to, ensure we have a command pool and command buffer on
    // that queue, and we'll also use the first queue that the application creates (or fetch our
    // own).
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      uint32_t qidx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
      m_ExternalQueues.resize(RDCMAX((uint32_t)m_ExternalQueues.size(), qidx + 1));

      VkCommandPoolCreateInfo poolInfo = {
          VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qidx,
      };
      vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL,
                                               &m_ExternalQueues[qidx].pool);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].pool);

      VkCommandBufferAllocateInfo cmdInfo = {
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          NULL,
          Unwrap(m_ExternalQueues[qidx].pool),
          VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          1,
      };

      vkr = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &cmdInfo,
                                                    &m_ExternalQueues[qidx].buffer);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(m_SetDeviceLoaderData)
        m_SetDeviceLoaderData(device, m_ExternalQueues[qidx].buffer);
      else
        SetDispatchTableOverMagicNumber(device, m_ExternalQueues[qidx].buffer);

      GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].buffer);
    }

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

    m_PhysicalDeviceData.driverInfo = VkDriverInfo(m_PhysicalDeviceData.props);

    m_Replay.SetDriverInformation(m_PhysicalDeviceData.props);

    // MoltenVK reports 0x3fffffff for this limit so just ignore that value if it comes up
    RDCASSERT(m_PhysicalDeviceData.props.limits.maxBoundDescriptorSets <
                      ARRAY_COUNT(BakedCmdBufferInfo::pushDescriptorID[0]) ||
                  m_PhysicalDeviceData.props.limits.maxBoundDescriptorSets >= 0x10000000,
              m_PhysicalDeviceData.props.limits.maxBoundDescriptorSets);

    for(int i = VK_FORMAT_BEGIN_RANGE + 1; i < VK_FORMAT_END_RANGE; i++)
      ObjDisp(physicalDevice)
          ->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), VkFormat(i),
                                              &m_PhysicalDeviceData.fmtprops[i]);

    m_PhysicalDeviceData.queueCount = qCount;
    memcpy(m_PhysicalDeviceData.queueProps, props, qCount * sizeof(VkQueueFamilyProperties));

    m_PhysicalDeviceData.readbackMemIndex =
        m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
    m_PhysicalDeviceData.uploadMemIndex =
        m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
    m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(
        ~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    for(size_t i = 0; i < m_ReplayPhysicalDevices.size(); i++)
    {
      if(physicalDevice == m_ReplayPhysicalDevices[i])
      {
        m_PhysicalDeviceData.memIdxMap = m_MemIdxMaps[i];
        break;
      }
    }

    APIProps.vendor = GetDriverInfo().Vendor();

    m_ShaderCache = new VulkanShaderCache(this);

    m_DebugManager = new VulkanDebugManager(this);

    m_Replay.CreateResources();
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

      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  std::vector<const char *> Extensions(
      createInfo.ppEnabledExtensionNames,
      createInfo.ppEnabledExtensionNames + createInfo.enabledExtensionCount);

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

  uint32_t qCount = 0;
  VkResult vkr = VK_SUCCESS;

  ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

  VkQueueFamilyProperties *props = new VkQueueFamilyProperties[qCount];
  ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);

  // find a queue that supports all capabilities, and if one doesn't exist, add it.
  bool found = false;
  uint32_t qFamilyIdx = 0;

  // we need graphics, and if there is a graphics queue there must be a graphics & compute queue.
  VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

  // for queue priorities, if we need it
  float one = 1.0f;

  // if we need to change the requested queues, it will point to this
  VkDeviceQueueCreateInfo *modQueues = NULL;

  for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
  {
    uint32_t idx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
    RDCASSERT(idx < qCount);

    // this requested queue is one we can use too
    if((props[idx].queueFlags & search) == search && createInfo.pQueueCreateInfos[i].queueCount > 0)
    {
      qFamilyIdx = idx;
      found = true;
      break;
    }
  }

  // if we didn't find it, search for which queue family we should add a request for
  if(!found)
  {
    RDCDEBUG("App didn't request a queue family we can use - adding our own");

    for(uint32_t i = 0; i < qCount; i++)
    {
      if((props[i].queueFlags & search) == search)
      {
        qFamilyIdx = i;
        found = true;
        break;
      }
    }

    if(!found)
    {
      SAFE_DELETE_ARRAY(props);
      RDCERR("Can't add a queue with required properties for RenderDoc! Unsupported configuration");
      return VK_ERROR_INITIALIZATION_FAILED;
    }

    // we found the queue family, add it
    modQueues = new VkDeviceQueueCreateInfo[createInfo.queueCreateInfoCount + 1];
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
      modQueues[i] = createInfo.pQueueCreateInfos[i];

    modQueues[createInfo.queueCreateInfoCount].queueFamilyIndex = qFamilyIdx;
    modQueues[createInfo.queueCreateInfoCount].queueCount = 1;
    modQueues[createInfo.queueCreateInfoCount].pQueuePriorities = &one;

    createInfo.pQueueCreateInfos = modQueues;
    createInfo.queueCreateInfoCount++;
  }

  m_QueueFamilies.resize(createInfo.queueCreateInfoCount);
  m_QueueFamilyCounts.resize(createInfo.queueCreateInfoCount);
  m_QueueFamilyIndices.clear();
  for(size_t i = 0; i < createInfo.queueCreateInfoCount; i++)
  {
    uint32_t family = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
    uint32_t count = createInfo.pQueueCreateInfos[i].queueCount;
    m_QueueFamilies.resize(RDCMAX(m_QueueFamilies.size(), size_t(family + 1)));
    m_QueueFamilyCounts.resize(RDCMAX(m_QueueFamilies.size(), size_t(family + 1)));

    m_QueueFamilies[family] = new VkQueue[count];
    m_QueueFamilyCounts[family] = count;
    for(uint32_t q = 0; q < count; q++)
      m_QueueFamilies[family][q] = VK_NULL_HANDLE;

    if(std::find(m_QueueFamilyIndices.begin(), m_QueueFamilyIndices.end(), family) ==
       m_QueueFamilyIndices.end())
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
    SAFE_DELETE_ARRAY(props);
    RDCERR("Couldn't find loader device create info, which is required. Incompatible loader?");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
  PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
  // move chain on for next layer
  layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

  PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice");

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

  // patch enabled features

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

  // VkPhysicalDeviceFeatures2 takes priority
  if(enabledFeatures2)
    enabledFeatures = enabledFeatures2->features;
  else if(createInfo.pEnabledFeatures)
    enabledFeatures = *createInfo.pEnabledFeatures;

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
        "shaderStorageImageMultisample = false, save/load from 2DMS textures will not be "
        "possible");

  if(availFeatures.sampleRateShading)
    enabledFeatures.sampleRateShading = true;
  else
    RDCWARN(
        "sampleRateShading = false, save/load from depth 2DMS textures will not be "
        "possible");

  if(availFeatures.occlusionQueryPrecise)
    enabledFeatures.occlusionQueryPrecise = true;
  else
    RDCWARN("occlusionQueryPrecise = false, samples passed counter will not be available");

  if(availFeatures.pipelineStatisticsQuery)
    enabledFeatures.pipelineStatisticsQuery = true;
  else
    RDCWARN("pipelineStatisticsQuery = false, pipeline counters will not work");

  // patch the enabled features
  if(enabledFeatures2)
    enabledFeatures2->features = enabledFeatures;
  else
    createInfo.pEnabledFeatures = &enabledFeatures;

  VkPhysicalDeviceFragmentDensityMapFeaturesEXT *fragmentDensityMapFeatures =
      (VkPhysicalDeviceFragmentDensityMapFeaturesEXT *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT);
  if(fragmentDensityMapFeatures && !fragmentDensityMapFeatures->fragmentDensityMapNonSubsampledImages)
  {
    fragmentDensityMapFeatures->fragmentDensityMapNonSubsampledImages = true;
  }

  VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *bufferAddressFeatures =
      (VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *)FindNextStruct(
          &createInfo, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT);

  if(bufferAddressFeatures)
  {
    // we must turn on bufferDeviceAddressCaptureReplay. We verified that this feature was available
    // before we whitelisted the extension
    bufferAddressFeatures->bufferDeviceAddressCaptureReplay = VK_TRUE;
  }

  VkResult ret;
  SERIALISE_TIME_CALL(ret = createFunc(Unwrap(physicalDevice), &createInfo, pAllocator, pDevice));

  // don't serialise out any of the pNext stuff for layer initialisation
  RemoveNextStruct(&createInfo, VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);

  if(ret == VK_SUCCESS)
  {
    InitDeviceTable(*pDevice, gdpa);

    ResourceId id = GetResourceManager()->WrapResource(*pDevice, *pDevice);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDevice);
        Serialise_vkCreateDevice(ser, physicalDevice, &createInfo, NULL, pDevice);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDevice);
      RDCASSERT(record);

      record->AddChunk(chunk);

      record->memIdxMap = GetRecord(physicalDevice)->memIdxMap;

      record->instDevInfo = new InstanceDeviceInfo();

      record->instDevInfo->brokenGetDeviceProcAddr =
          GetRecord(m_Instance)->instDevInfo->brokenGetDeviceProcAddr;

      record->instDevInfo->vulkanVersion = GetRecord(m_Instance)->instDevInfo->vulkanVersion;

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
     GetRecord(m_Instance)->instDevInfo->vulkanVersion >= ver)      \
  {                                                                 \
    record->instDevInfo->ext_##name = true;                         \
  }

      for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
      {
        CheckDeviceExts();
      }

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
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.cmdpool);
    }

    // for each queue family that isn't our own, create a command pool and command buffer on that
    // queue
    for(uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
      uint32_t qidx = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
      m_ExternalQueues.resize(RDCMAX((uint32_t)m_ExternalQueues.size(), qidx + 1));

      VkCommandPoolCreateInfo poolInfo = {
          VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qidx,
      };
      vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL,
                                               &m_ExternalQueues[qidx].pool);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].pool);

      VkCommandBufferAllocateInfo cmdInfo = {
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          NULL,
          Unwrap(m_ExternalQueues[qidx].pool),
          VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          1,
      };

      vkr = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &cmdInfo,
                                                    &m_ExternalQueues[qidx].buffer);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      if(m_SetDeviceLoaderData)
        m_SetDeviceLoaderData(device, m_ExternalQueues[qidx].buffer);
      else
        SetDispatchTableOverMagicNumber(device, m_ExternalQueues[qidx].buffer);

      GetResourceManager()->WrapResource(Unwrap(device), m_ExternalQueues[qidx].buffer);
    }

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

    m_PhysicalDeviceData.driverInfo = VkDriverInfo(m_PhysicalDeviceData.props);

    for(int i = VK_FORMAT_BEGIN_RANGE + 1; i < VK_FORMAT_END_RANGE; i++)
      ObjDisp(physicalDevice)
          ->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), VkFormat(i),
                                              &m_PhysicalDeviceData.fmtprops[i]);

    m_PhysicalDeviceData.readbackMemIndex =
        m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
    m_PhysicalDeviceData.uploadMemIndex =
        m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
    m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(
        ~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_PhysicalDeviceData.queueCount = qCount;
    memcpy(m_PhysicalDeviceData.queueProps, props, qCount * sizeof(VkQueueFamilyProperties));

    m_PhysicalDeviceData.fakeMemProps = GetRecord(physicalDevice)->memProps;

    m_ShaderCache = new VulkanShaderCache(this);

    m_TextRenderer = new VulkanTextRenderer(this);

    m_DebugManager = new VulkanDebugManager(this);
  }

  SAFE_DELETE_ARRAY(props);
  SAFE_DELETE_ARRAY(modQueues);

  FirstFrame();

  return ret;
}

void WrappedVulkan::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
  // flush out any pending commands/semaphores
  SubmitCmds();
  SubmitSemaphores();
  FlushQ();

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
    if(m_ExternalQueues[i].buffer != VK_NULL_HANDLE)
    {
      GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].buffer);

      ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_ExternalQueues[i].pool), NULL);
      GetResourceManager()->ReleaseWrappedResource(m_ExternalQueues[i].pool);
    }
  }

  m_InternalCmds.Reset();

  m_QueueFamilyIdx = ~0U;
  m_PrevQueue = m_Queue = VK_NULL_HANDLE;

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
  SERIALISE_ELEMENT(device);

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
