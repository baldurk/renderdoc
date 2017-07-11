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

#include "../vk_core.h"
#include "../vk_debug.h"

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

static void StripUnwantedLayers(vector<string> &Layers)
{
  for(auto it = Layers.begin(); it != Layers.end();)
  {
    // don't try and create our own layer on replay!
    if(*it == RENDERDOC_LAYER_NAME)
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

    // filter out validation layers
    if(*it == "VK_LAYER_LUNARG_standard_validation" || *it == "VK_LAYER_LUNARG_core_validation" ||
       *it == "VK_LAYER_LUNARG_device_limits" || *it == "VK_LAYER_LUNARG_image" ||
       *it == "VK_LAYER_LUNARG_object_tracker" || *it == "VK_LAYER_LUNARG_parameter_validation" ||
       *it == "VK_LAYER_LUNARG_swapchain" || *it == "VK_LAYER_GOOGLE_threading" ||
       *it == "VK_LAYER_GOOGLE_unique_objects")
    {
      it = Layers.erase(it);
      continue;
    }

    ++it;
  }
}

ReplayStatus WrappedVulkan::Initialise(VkInitParams &params)
{
  if(m_pSerialiser->HasError())
    return ReplayStatus::FileIOFailed;

  m_InitParams = params;

  params.AppName = string("RenderDoc @ ") + params.AppName;
  params.EngineName = string("RenderDoc @ ") + params.EngineName;

  // PORTABILITY verify that layers/extensions are available
  StripUnwantedLayers(params.Layers);

#if ENABLED(FORCE_VALIDATION_LAYERS)
  params.Layers.push_back("VK_LAYER_LUNARG_standard_validation");

  params.Extensions.push_back("VK_EXT_debug_report");
#endif

  // strip out any WSI/direct display extensions. We'll add the ones we want for creating windows
  // on the current platforms below, and we don't replay any of the WSI functionality
  // directly so these extensions aren't needed
  for(auto it = params.Extensions.begin(); it != params.Extensions.end();)
  {
    if(*it == "VK_KHR_xlib_surface" || *it == "VK_KHR_xcb_surface" ||
       *it == "VK_KHR_wayland_surface" || *it == "VK_KHR_mir_surface" ||
       *it == "VK_KHR_android_surface" || *it == "VK_KHR_win32_surface" ||
       *it == "VK_KHR_display" || *it == "VK_EXT_direct_mode_display" ||
       *it == "VK_EXT_acquire_xlib_display" || *it == "VK_EXT_display_surface_counter")
    {
      it = params.Extensions.erase(it);
    }
    else
    {
      ++it;
    }
  }

  RDCEraseEl(m_ExtensionsEnabled);

  std::set<string> supportedExtensions;

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

  std::set<string> supportedLayers;

  {
    uint32_t count = 0;
    GetInstanceDispatchTable(NULL)->EnumerateInstanceLayerProperties(&count, NULL);

    VkLayerProperties *props = new VkLayerProperties[count];
    GetInstanceDispatchTable(NULL)->EnumerateInstanceLayerProperties(&count, props);

    for(uint32_t e = 0; e < count; e++)
      supportedLayers.insert(props[e].layerName);

    SAFE_DELETE_ARRAY(props);
  }

  bool ok = AddRequiredExtensions(true, params.Extensions, supportedExtensions);

  // error message will be printed to log in above function if something went wrong
  if(!ok)
    return ReplayStatus::APIHardwareUnsupported;

  // verify that extensions & layers are supported
  for(size_t i = 0; i < params.Layers.size(); i++)
  {
    if(supportedLayers.find(params.Layers[i]) == supportedLayers.end())
    {
      RDCERR("Log requires layer '%s' which is not supported", params.Layers[i].c_str());
      return ReplayStatus::APIHardwareUnsupported;
    }
  }

  for(size_t i = 0; i < params.Extensions.size(); i++)
  {
    if(supportedExtensions.find(params.Extensions[i]) == supportedExtensions.end())
    {
      RDCERR("Log requires extension '%s' which is not supported", params.Extensions[i].c_str());
      return ReplayStatus::APIHardwareUnsupported;
    }
  }

  const char **layerscstr = new const char *[params.Layers.size()];
  for(size_t i = 0; i < params.Layers.size(); i++)
    layerscstr[i] = params.Layers[i].c_str();

  const char **extscstr = new const char *[params.Extensions.size()];
  for(size_t i = 0; i < params.Extensions.size(); i++)
    extscstr[i] = params.Extensions[i].c_str();

  VkApplicationInfo appinfo = {
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      NULL,
      params.AppName.c_str(),
      params.AppVersion,
      params.EngineName.c_str(),
      params.EngineVersion,
      VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo instinfo = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      NULL,
      0,
      &appinfo,
      (uint32_t)params.Layers.size(),
      layerscstr,
      (uint32_t)params.Extensions.size(),
      extscstr,
  };

  m_Instance = VK_NULL_HANDLE;

  VkResult ret = GetInstanceDispatchTable(NULL)->CreateInstance(&instinfo, NULL, &m_Instance);

  SAFE_DELETE_ARRAY(layerscstr);
  SAFE_DELETE_ARRAY(extscstr);

  if(ret != VK_SUCCESS)
    return ReplayStatus::APIHardwareUnsupported;

  RDCASSERTEQUAL(ret, VK_SUCCESS);

  InitInstanceReplayTables(m_Instance);

  GetResourceManager()->WrapResource(m_Instance, m_Instance);
  GetResourceManager()->AddLiveResource(params.InstanceID, m_Instance);

  m_DbgMsgCallback = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_QueueFamilyIdx = ~0U;
  m_Queue = VK_NULL_HANDLE;
  m_InternalCmds.Reset();

  if(ObjDisp(m_Instance)->CreateDebugReportCallbackEXT)
  {
    VkDebugReportCallbackCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugInfo.pNext = NULL;
    debugInfo.pfnCallback = &DebugCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgMsgCallback);
  }

  uint32_t count = 0;

  VkResult vkr = ObjDisp(m_Instance)->EnumeratePhysicalDevices(Unwrap(m_Instance), &count, NULL);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
      RDCERR("File an issue on github to request support: https://github.com/baldurk/renderdoc");

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

      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  const char **addedExts = new const char *[modifiedCreateInfo.enabledExtensionCount + 1];

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
    addedExts[i] = modifiedCreateInfo.ppEnabledExtensionNames[i];

  if(RenderDoc::Inst().GetCaptureOptions().APIValidation)
    addedExts[modifiedCreateInfo.enabledExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

  modifiedCreateInfo.ppEnabledExtensionNames = addedExts;

  VkResult ret = createFunc(&modifiedCreateInfo, pAllocator, pInstance);

  m_Instance = *pInstance;

  InitInstanceTable(m_Instance, gpa);

  GetResourceManager()->WrapResource(m_Instance, m_Instance);

  *pInstance = m_Instance;

  // should only be called during capture
  RDCASSERT(m_State >= WRITING);

  m_InitParams.Set(pCreateInfo, GetResID(m_Instance));
  VkResourceRecord *record = GetResourceManager()->AddResourceRecord(m_Instance);

  record->instDevInfo = new InstanceDeviceInfo();

#undef CheckExt
#define CheckExt(name)                                              \
  if(!strcmp(modifiedCreateInfo.ppEnabledExtensionNames[i], #name)) \
  {                                                                 \
    record->instDevInfo->ext_##name = true;                         \
  }

  for(uint32_t i = 0; i < modifiedCreateInfo.enabledExtensionCount; i++)
  {
    CheckInstanceExts();
  }

  delete[] addedExts;

  InitInstanceExtensionTables(m_Instance);

  RenderDoc::Inst().AddDeviceFrameCapturer(LayerDisp(m_Instance), this);

  m_DbgMsgCallback = VK_NULL_HANDLE;
  m_PhysicalDevice = VK_NULL_HANDLE;
  m_Device = VK_NULL_HANDLE;
  m_QueueFamilyIdx = ~0U;
  m_Queue = VK_NULL_HANDLE;
  m_InternalCmds.Reset();

  if(RenderDoc::Inst().GetCaptureOptions().APIValidation &&
     ObjDisp(m_Instance)->CreateDebugReportCallbackEXT)
  {
    VkDebugReportCallbackCreateInfoEXT debugInfo = {};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugInfo.pNext = NULL;
    debugInfo.pfnCallback = &DebugCallbackStatic;
    debugInfo.pUserData = this;
    debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;

    ObjDisp(m_Instance)
        ->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgMsgCallback);
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

  // destroy the pool
  ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.cmdpool), NULL);
  GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.cmdpool);

  for(size_t i = 0; i < m_InternalCmds.freesems.size(); i++)
  {
    ObjDisp(m_Device)->DestroySemaphore(Unwrap(m_Device), Unwrap(m_InternalCmds.freesems[i]), NULL);
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freesems[i]);
  }

  // we do more in Shutdown than the equivalent vkDestroyInstance since on replay there's
  // no explicit vkDestroyDevice, we destroy the device here then the instance

  // destroy any replay objects that aren't specifically to do with the frame capture
  for(size_t i = 0; i < m_CleanupMems.size(); i++)
  {
    ObjDisp(m_Device)->FreeMemory(Unwrap(m_Device), Unwrap(m_CleanupMems[i]), NULL);
    GetResourceManager()->ReleaseWrappedResource(m_CleanupMems[i]);
  }
  m_CleanupMems.clear();

  // destroy the physical devices manually because due to remapping the may have leftover
  // refcounts
  for(size_t i = 0; i < m_ReplayPhysicalDevices.size(); i++)
    GetResourceManager()->ReleaseWrappedResource(m_ReplayPhysicalDevices[i]);

  // destroy debug manager and any objects it created
  SAFE_DELETE(m_DebugManager);

  if(ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT && m_DbgMsgCallback != VK_NULL_HANDLE)
    ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT(Unwrap(m_Instance), m_DbgMsgCallback, NULL);

  // need to store the unwrapped device and instance to destroy the
  // API object after resource manager shutdown
  VkInstance inst = Unwrap(m_Instance);
  VkDevice dev = Unwrap(m_Device);

  const VkLayerDispatchTable *vt = ObjDisp(m_Device);
  const VkLayerInstanceDispatchTable *vit = ObjDisp(m_Instance);

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
  vt->DestroyDevice(dev, NULL);
  vit->DestroyInstance(inst, NULL);
}

void WrappedVulkan::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator)
{
  RDCASSERT(m_Instance == instance);

  if(ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT && m_DbgMsgCallback != VK_NULL_HANDLE)
    ObjDisp(m_Instance)->DestroyDebugReportCallbackEXT(Unwrap(m_Instance), m_DbgMsgCallback, NULL);

  // the device should already have been destroyed, assuming that the
  // application is well behaved. If not, we just leak.

  ObjDisp(m_Instance)->DestroyInstance(Unwrap(m_Instance), NULL);
  GetResourceManager()->ReleaseWrappedResource(m_Instance);

  RenderDoc::Inst().RemoveDeviceFrameCapturer(LayerDisp(m_Instance));

  m_Instance = VK_NULL_HANDLE;
}

bool WrappedVulkan::Serialise_vkEnumeratePhysicalDevices(Serialiser *localSerialiser,
                                                         VkInstance instance,
                                                         uint32_t *pPhysicalDeviceCount,
                                                         VkPhysicalDevice *pPhysicalDevices)
{
  SERIALISE_ELEMENT(ResourceId, inst, GetResID(instance));
  SERIALISE_ELEMENT(uint32_t, physIndex, *pPhysicalDeviceCount);
  SERIALISE_ELEMENT(ResourceId, physId, GetResID(*pPhysicalDevices));

  uint32_t memIdxMap[32] = {0};
  if(m_State >= WRITING)
    memcpy(memIdxMap, GetRecord(*pPhysicalDevices)->memIdxMap, sizeof(memIdxMap));

  localSerialiser->SerialisePODArray<32>("memIdxMap", memIdxMap);

  // not used at the moment but useful for reference and might be used
  // in the future
  VkPhysicalDeviceProperties physProps;
  VkPhysicalDeviceMemoryProperties memProps;
  VkPhysicalDeviceFeatures physFeatures;
  VkQueueFamilyProperties queueProps[16];

  if(m_State >= WRITING)
  {
    ObjDisp(instance)->GetPhysicalDeviceProperties(Unwrap(*pPhysicalDevices), &physProps);
    ObjDisp(instance)->GetPhysicalDeviceMemoryProperties(Unwrap(*pPhysicalDevices), &memProps);
    ObjDisp(instance)->GetPhysicalDeviceFeatures(Unwrap(*pPhysicalDevices), &physFeatures);

    uint32_t queueCount = 0;
    ObjDisp(instance)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(*pPhysicalDevices),
                                                              &queueCount, NULL);

    if(queueCount > 16)
    {
      RDCWARN("More than 16 queues");
      queueCount = 16;
    }

    ObjDisp(instance)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(*pPhysicalDevices),
                                                              &queueCount, queueProps);
  }

  localSerialiser->Serialise("physProps", physProps);
  localSerialiser->Serialise("memProps", memProps);
  localSerialiser->Serialise("physFeatures", physFeatures);
  localSerialiser->SerialisePODArray<16>("queueProps", queueProps);

  VkPhysicalDevice pd = VK_NULL_HANDLE;

  if(m_State >= WRITING)
  {
    pd = *pPhysicalDevices;
  }
  else
  {
    {
      VkDriverInfo capturedVersion(physProps);

      RDCLOG("Captured log describes physical device %u:", physIndex);
      RDCLOG("   - %s (ver %u.%u patch 0x%x) - %04x:%04x", physProps.deviceName,
             capturedVersion.Major(), capturedVersion.Minor(), capturedVersion.Patch(),
             physProps.vendorID, physProps.deviceID);

      if(physIndex >= m_OriginalPhysicalDevices.size())
        m_OriginalPhysicalDevices.resize(physIndex + 1);

      m_OriginalPhysicalDevices[physIndex].props = physProps;
      m_OriginalPhysicalDevices[physIndex].memProps = memProps;
      m_OriginalPhysicalDevices[physIndex].features = physFeatures;
    }

    // match up physical devices to those available on replay as best as possible. In general
    // hopefully the most common case is when there's a precise match, and maybe the order changed.
    //
    // If more GPUs were present on replay than during capture, we map many-to-one which might have
    // bad side-effects as e.g. we have to pick one memidxmap, but this is as good as we can do.

    uint32_t bestIdx = 0;
    VkPhysicalDeviceProperties bestPhysProps;
    VkPhysicalDeviceMemoryProperties bestMemProps;

    pd = m_ReplayPhysicalDevices[bestIdx];

    ObjDisp(pd)->GetPhysicalDeviceProperties(Unwrap(pd), &bestPhysProps);
    ObjDisp(pd)->GetPhysicalDeviceMemoryProperties(Unwrap(pd), &bestMemProps);

    for(uint32_t i = 1; i < (uint32_t)m_ReplayPhysicalDevices.size(); i++)
    {
      VkPhysicalDeviceProperties compPhysProps;
      VkPhysicalDeviceMemoryProperties compMemProps;

      pd = m_ReplayPhysicalDevices[i];

      // find the best possible match for this physical device
      ObjDisp(pd)->GetPhysicalDeviceProperties(Unwrap(pd), &compPhysProps);
      ObjDisp(pd)->GetPhysicalDeviceMemoryProperties(Unwrap(pd), &compMemProps);

      // an exact vendorID match is a better match than not
      if(compPhysProps.vendorID == physProps.vendorID && bestPhysProps.vendorID != physProps.vendorID)
      {
        bestIdx = i;
        bestPhysProps = compPhysProps;
        bestMemProps = compMemProps;
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
        bestMemProps = compMemProps;
        continue;
      }
      else if(compPhysProps.deviceID != physProps.deviceID)
      {
        continue;
      }

      // if we have multiple identical devices, which isn't uncommon, favour the one
      // that hasn't been assigned
      if(m_ReplayPhysicalDevicesUsed[bestIdx] && !m_ReplayPhysicalDevicesUsed[i])
      {
        bestIdx = i;
        bestPhysProps = compPhysProps;
        bestMemProps = compMemProps;
        continue;
      }

      // this device isn't any better, ignore it
    }

    {
      VkDriverInfo runningVersion(bestPhysProps);

      RDCLOG("Mapping during replay to physical device %u:", bestIdx);
      RDCLOG("   - %s (ver %u.%u patch 0x%x) - %04x:%04x", bestPhysProps.deviceName,
             runningVersion.Major(), runningVersion.Minor(), runningVersion.Patch(),
             bestPhysProps.vendorID, bestPhysProps.deviceID);
    }

    pd = m_ReplayPhysicalDevices[bestIdx];

    GetResourceManager()->AddLiveResource(physId, pd);

    if(physIndex >= m_PhysicalDevices.size())
      m_PhysicalDevices.resize(physIndex + 1);
    m_PhysicalDevices[physIndex] = pd;

    if(m_ReplayPhysicalDevicesUsed[bestIdx])
    {
      // error if we're remapping multiple physical devices to the same best match
      RDCERR(
          "Mappnig multiple capture-time physical devices to a single replay-time physical device."
          "This means the HW has changed between capture and replay and may cause bugs.");
    }
    else
    {
      // the first physical device 'wins' for the memory index map
      uint32_t *storedMap = new uint32_t[32];
      memcpy(storedMap, memIdxMap, sizeof(memIdxMap));
      m_MemIdxMaps[physIndex] = storedMap;
    }
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

  vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  m_PhysicalDevices.resize(count);
  m_SupportedQueueFamilies.resize(count);

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

      if(m_State >= WRITING)
      {
        // add the record first since it's used in the serialise function below to fetch
        // the memory indices
        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(devices[i]);
        RDCASSERT(record);

        record->memProps = new VkPhysicalDeviceMemoryProperties();

        ObjDisp(devices[i])->GetPhysicalDeviceMemoryProperties(Unwrap(devices[i]), record->memProps);

        m_PhysicalDevices[i] = devices[i];

        // we remap memory indices to discourage coherent maps as much as possible
        RemapMemoryIndices(record->memProps, &record->memIdxMap);

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CONTEXT(ENUM_PHYSICALS);
          Serialise_vkEnumeratePhysicalDevices(localSerialiser, instance, &i, &devices[i]);

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

    // find the queue with the most bits set and only report that one

    {
      uint32_t queuecount = 0;
      ObjDisp(m_PhysicalDevices[i])
          ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(m_PhysicalDevices[i]), &queuecount, NULL);

      VkQueueFamilyProperties *props = new VkQueueFamilyProperties[queuecount];
      ObjDisp(m_PhysicalDevices[i])
          ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(m_PhysicalDevices[i]), &queuecount, props);

      uint32_t best = 0;

      // don't need to explicitly check for transfer, because graphics bit
      // implies it. We do have to check for compute bit, because there might
      // be a graphics only queue - it just means we have to keep looking
      // to find the grpahics & compute queue family which is guaranteed.
      for(uint32_t q = 1; q < queuecount; q++)
      {
        // compare current against the known best
        VkQueueFamilyProperties &currentProps = props[q];
        VkQueueFamilyProperties &bestProps = props[best];

        const bool currentGraphics = (currentProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool currentCompute = (currentProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
        const bool currentSparse = (currentProps.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0;

        const bool bestGraphics = (bestProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool bestCompute = (bestProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
        const bool bestSparse = (bestProps.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0;

        // if one has graphics bit set, but the other doesn't
        if(currentGraphics != bestGraphics)
        {
          // if current has graphics but best doesn't, we have a new best
          if(currentGraphics)
            best = q;
          continue;
        }

        if(currentCompute != bestCompute)
        {
          // if current has compute but best doesn't, we have a new best
          if(currentCompute)
            best = q;
          continue;
        }

        // if we've gotten here, both best and current have graphics and compute. Check
        // to see if the current is somehow better than best (in the case of a tie, we
        // keep the lower index of queue).

        if(currentSparse != bestSparse)
        {
          if(currentSparse)
            best = q;
          continue;
        }

        if(currentProps.timestampValidBits != bestProps.timestampValidBits)
        {
          if(currentProps.timestampValidBits > bestProps.timestampValidBits)
            best = q;
          continue;
        }

        if(currentProps.minImageTransferGranularity.width <
               bestProps.minImageTransferGranularity.width ||
           currentProps.minImageTransferGranularity.height <
               bestProps.minImageTransferGranularity.height ||
           currentProps.minImageTransferGranularity.depth <
               bestProps.minImageTransferGranularity.depth)
        {
          best = q;
          continue;
        }
      }

      // only report a single available queue in this family
      props[best].queueCount = 1;

      m_SupportedQueueFamilies[i] = std::make_pair(best, props[best]);

      SAFE_DELETE_ARRAY(props);
    }
  }

  if(pPhysicalDeviceCount)
    *pPhysicalDeviceCount = count;
  if(pPhysicalDevices)
    memcpy(pPhysicalDevices, devices, count * sizeof(VkPhysicalDevice));

  SAFE_DELETE_ARRAY(devices);

  return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkCreateDevice(Serialiser *localSerialiser,
                                             VkPhysicalDevice physicalDevice,
                                             const VkDeviceCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkDevice *pDevice)
{
  SERIALISE_ELEMENT(ResourceId, physId, GetResID(physicalDevice));
  SERIALISE_ELEMENT(VkDeviceCreateInfo, serCreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(*pDevice));
  SERIALISE_ELEMENT(uint32_t, queueFamily, m_SupportedQueueFamily);

  if(m_State == READING)
  {
    // we must make any modifications locally, so the free of pointers
    // in the serialised VkDeviceCreateInfo don't double-free
    VkDeviceCreateInfo createInfo = serCreateInfo;

    m_SupportedQueueFamily = queueFamily;

    std::vector<string> Extensions;
    for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
      // don't include the debug marker extension
      if(strcmp(createInfo.ppEnabledExtensionNames[i], VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        continue;

      // don't include direct-display WSI extensions
      if(strcmp(createInfo.ppEnabledExtensionNames[i], VK_KHR_DISPLAY_SWAPCHAIN_EXTENSION_NAME) ||
         strcmp(createInfo.ppEnabledExtensionNames[i], VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME) ||
         strcmp(createInfo.ppEnabledExtensionNames[i], VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME))
        continue;

      Extensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    }

    if(std::find(Extensions.begin(), Extensions.end(),
                 VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_EXTENSION_NAME) != Extensions.end())
      m_ExtensionsEnabled[VkCheckExt_AMD_neg_viewport] = true;

    std::vector<string> Layers;
    for(uint32_t i = 0; i < createInfo.enabledLayerCount; i++)
      Layers.push_back(createInfo.ppEnabledLayerNames[i]);

    StripUnwantedLayers(Layers);

    physicalDevice = GetResourceManager()->GetLiveHandle<VkPhysicalDevice>(physId);

    std::set<string> supportedExtensions;

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

#if ENABLED(FORCE_VALIDATION_LAYERS)
    Layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    createInfo.enabledLayerCount = (uint32_t)Layers.size();

    const char **layerArray = NULL;
    if(!Layers.empty())
    {
      layerArray = new const char *[createInfo.enabledLayerCount];

      for(uint32_t i = 0; i < createInfo.enabledLayerCount; i++)
        layerArray[i] = Layers[i].c_str();

      createInfo.ppEnabledLayerNames = layerArray;
    }

    createInfo.enabledExtensionCount = (uint32_t)Extensions.size();

    const char **extArray = NULL;
    if(!Extensions.empty())
    {
      extArray = new const char *[createInfo.enabledExtensionCount];

      for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
        extArray[i] = Extensions[i].c_str();

      createInfo.ppEnabledExtensionNames = extArray;
    }

    VkDevice device;

    uint32_t qCount = 0;
    ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

    VkQueueFamilyProperties *props = new VkQueueFamilyProperties[qCount];
    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);

    bool found = false;
    uint32_t qFamilyIdx = 0;
    VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT);

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
        RDCERR(
            "Can't add a queue with required properties for RenderDoc! Unsupported configuration");
      }
      else
      {
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
    }

    SAFE_DELETE_ARRAY(props);

    VkPhysicalDeviceFeatures enabledFeatures = {0};
    if(createInfo.pEnabledFeatures != NULL)
      enabledFeatures = *createInfo.pEnabledFeatures;
    createInfo.pEnabledFeatures = &enabledFeatures;

    VkPhysicalDeviceFeatures availFeatures = {0};
    ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &availFeatures);

    if(availFeatures.depthClamp)
      enabledFeatures.depthClamp = true;
    else
      RDCWARN(
          "depthClamp = false, overlays like highlight drawcall won't show depth-clipped pixels.");

    if(availFeatures.fillModeNonSolid)
      enabledFeatures.fillModeNonSolid = true;
    else
      RDCWARN("fillModeNonSolid = false, wireframe overlay will be solid");

    if(availFeatures.robustBufferAccess)
      enabledFeatures.robustBufferAccess = true;
    else
      RDCWARN(
          "robustBufferAccess = false, out of bounds access due to bugs in application or "
          "RenderDoc may cause crashes");

    if(availFeatures.vertexPipelineStoresAndAtomics)
      enabledFeatures.vertexPipelineStoresAndAtomics = true;
    else
      RDCWARN("vertexPipelineStoresAndAtomics = false, output mesh data will not be available");

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

    // PORTABILITY check that extensions and layers supported in capture (from createInfo) are
    // supported in replay

    vkr = GetDeviceDispatchTable(NULL)->CreateDevice(Unwrap(physicalDevice), &createInfo, NULL,
                                                     &device);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(device, device);
    GetResourceManager()->AddLiveResource(devId, device);

    InitDeviceReplayTables(Unwrap(device));

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

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

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

    for(size_t i = 0; i < m_PhysicalDevices.size(); i++)
    {
      if(physicalDevice == m_PhysicalDevices[i])
      {
        m_PhysicalDeviceData.memIdxMap = m_MemIdxMaps[i];
        break;
      }
    }

    m_DebugManager = new VulkanDebugManager(this, device);

    SAFE_DELETE_ARRAY(modQueues);
    SAFE_DELETE_ARRAY(layerArray);
    SAFE_DELETE_ARRAY(extArray);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDevice(VkPhysicalDevice physicalDevice,
                                       const VkDeviceCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
  VkDeviceCreateInfo createInfo = *pCreateInfo;

  uint32_t qCount = 0;
  VkResult vkr = VK_SUCCESS;

  ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

  VkQueueFamilyProperties *props = new VkQueueFamilyProperties[qCount];
  ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);

  // find a queue that supports all capabilities, and if one doesn't exist, add it.
  bool found = false;
  uint32_t qFamilyIdx = 0;
  VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT);

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

  SAFE_DELETE_ARRAY(props);

  m_QueueFamilies.resize(createInfo.queueCreateInfoCount);
  for(size_t i = 0; i < createInfo.queueCreateInfoCount; i++)
  {
    uint32_t family = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
    uint32_t count = createInfo.pQueueCreateInfos[i].queueCount;
    m_QueueFamilies.resize(RDCMAX(m_QueueFamilies.size(), size_t(family + 1)));

    m_QueueFamilies[family] = new VkQueue[count];
    for(uint32_t q = 0; q < count; q++)
      m_QueueFamilies[family][q] = VK_NULL_HANDLE;
  }

  // find the matching physical device
  for(size_t i = 0; i < m_PhysicalDevices.size(); i++)
    if(m_PhysicalDevices[i] == physicalDevice)
      m_SupportedQueueFamily = m_SupportedQueueFamilies[i].first;

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
              m_SetDeviceLoaderData, layerCreateInfo->u.pfnSetDeviceLoaderData);
    m_SetDeviceLoaderData = layerCreateInfo->u.pfnSetDeviceLoaderData;
  }

  // patch enabled features

  VkPhysicalDeviceFeatures availFeatures;

  ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &availFeatures);

  // default to all off. This is equivalent to createInfo.pEnabledFeatures == NULL
  VkPhysicalDeviceFeatures enabledFeatures = {0};

  // if the user enabled features, of course we want to enable them.
  if(createInfo.pEnabledFeatures)
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
    RDCWARN("occlusionQueryPrecise = false, samples written counter will not work");

  if(availFeatures.pipelineStatisticsQuery)
    enabledFeatures.pipelineStatisticsQuery = true;
  else
    RDCWARN("pipelineStatisticsQuery = false, pipeline counters will not work");

  createInfo.pEnabledFeatures = &enabledFeatures;

  VkResult ret = createFunc(Unwrap(physicalDevice), &createInfo, pAllocator, pDevice);

  // don't serialise out any of the pNext stuff for layer initialisation
  // (note that we asserted above that there was nothing else in the chain)
  createInfo.pNext = NULL;

  if(ret == VK_SUCCESS)
  {
    InitDeviceTable(*pDevice, gdpa);

    ResourceId id = GetResourceManager()->WrapResource(*pDevice, *pDevice);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_DEVICE);
        Serialise_vkCreateDevice(localSerialiser, physicalDevice, &createInfo, NULL, pDevice);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDevice);
      RDCASSERT(record);

      record->AddChunk(chunk);

      record->memIdxMap = GetRecord(physicalDevice)->memIdxMap;

      record->instDevInfo = new InstanceDeviceInfo();

#undef CheckExt
#define CheckExt(name) \
  record->instDevInfo->ext_##name = GetRecord(m_Instance)->instDevInfo->ext_##name;

      // inherit extension enablement from instance, that way GetDeviceProcAddress can check
      // for enabled extensions for instance functions
      CheckInstanceExts();

#undef CheckExt
#define CheckExt(name)                                      \
  if(!strcmp(createInfo.ppEnabledExtensionNames[i], #name)) \
  {                                                         \
    record->instDevInfo->ext_##name = true;                 \
  }

      for(uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
      {
        CheckDeviceExts();
      }

      InitDeviceExtensionTables(*pDevice);
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

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

    m_PhysicalDeviceData.readbackMemIndex =
        m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
    m_PhysicalDeviceData.uploadMemIndex =
        m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
    m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(
        ~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_PhysicalDeviceData.fakeMemProps = GetRecord(physicalDevice)->memProps;

    m_DebugManager = new VulkanDebugManager(this, device);
  }

  SAFE_DELETE_ARRAY(modQueues);

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

  // since we didn't create proper registered resources for our command buffers,
  // they won't be taken down properly with the pool. So we release them (just our
  // data) here.
  for(size_t i = 0; i < m_InternalCmds.freecmds.size(); i++)
    GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freecmds[i]);

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

  m_InternalCmds.Reset();

  m_QueueFamilyIdx = ~0U;
  m_Queue = VK_NULL_HANDLE;

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

bool WrappedVulkan::Serialise_vkDeviceWaitIdle(Serialiser *localSerialiser, VkDevice device)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));

  if(m_State < WRITING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(id);
    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkDeviceWaitIdle(VkDevice device)
{
  VkResult ret = ObjDisp(device)->DeviceWaitIdle(Unwrap(device));

  if(m_State >= WRITING_CAPFRAME)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(DEVICE_WAIT_IDLE);
    Serialise_vkDeviceWaitIdle(localSerialiser, device);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}
