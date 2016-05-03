/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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
// On capture, WrappedVulkan is new'd and delete'd before vkCreateInstance() and after vkDestroyInstance()
// On replay,  WrappedVulkan is new'd and delete'd before Initialise()       and after Shutdown()
//
// The class constructor and destructor handle only *non-API* work. All API objects must be created and
// torn down in the latter functions (vkCreateInstance/vkDestroyInstance during capture, and
// Initialise/Shutdown during replay).
//
// Note that during capture we have vkDestroyDevice before vkDestroyDevice that does most of the work.
//
// Also we assume correctness from the application, that all objects are destroyed before the device and
// instance are destroyed. We only clean up after our own objects.

//#define FORCE_VALIDATION_LAYERS

static void StripUnwantedLayers(vector<string> &Layers)
{
	for(auto it = Layers.begin(); it != Layers.end(); )
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

		// filter out validation layers
		if(*it == "VK_LAYER_LUNARG_standard_validation" ||
			*it == "VK_LAYER_LUNARG_core_validation" ||
			*it == "VK_LAYER_LUNARG_device_limits" ||
			*it == "VK_LAYER_LUNARG_image" ||
			*it == "VK_LAYER_LUNARG_object_tracker" ||
			*it == "VK_LAYER_LUNARG_parameter_validation" ||
			*it == "VK_LAYER_LUNARG_swapchain" ||
			*it == "VK_LAYER_GOOGLE_threading"
			)
		{
			it = Layers.erase(it);
			continue;
		}       

		++it;
	}
}

void WrappedVulkan::Initialise(VkInitParams &params)
{
	m_InitParams = params;

	params.AppName = string("RenderDoc @ ") + params.AppName;
	params.EngineName = string("RenderDoc @ ") + params.EngineName;

	// PORTABILITY verify that layers/extensions are available
	StripUnwantedLayers(params.Layers);

#if defined(FORCE_VALIDATION_LAYERS)
	params.Layers.push_back("VK_LAYER_LUNARG_standard_validation");

	params.Extensions.push_back("VK_EXT_debug_report");
#endif

	const char **layerscstr = new const char *[params.Layers.size()];
	for(size_t i=0; i < params.Layers.size(); i++)
		layerscstr[i] = params.Layers[i].c_str();

	const char **extscstr = new const char *[params.Extensions.size()];
	for(size_t i=0; i < params.Extensions.size(); i++)
		extscstr[i] = params.Extensions[i].c_str();

	VkApplicationInfo appinfo = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
			params.AppName.c_str(), params.AppVersion,
			params.EngineName.c_str(), params.EngineVersion,
			VK_API_VERSION_1_0,
	};

	VkInstanceCreateInfo instinfo = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0,
			&appinfo,
			(uint32_t)params.Layers.size(), layerscstr,
			(uint32_t)params.Extensions.size(), extscstr,
	};

	m_Instance = VK_NULL_HANDLE;

	VkResult ret = GetInstanceDispatchTable(NULL)->CreateInstance(&instinfo, NULL, &m_Instance);
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
		debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT|VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT|VK_DEBUG_REPORT_ERROR_BIT_EXT;

		ObjDisp(m_Instance)->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgMsgCallback);
	}

	SAFE_DELETE_ARRAY(layerscstr);
	SAFE_DELETE_ARRAY(extscstr);
}

VkResult WrappedVulkan::vkCreateInstance(
		const VkInstanceCreateInfo*                 pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkInstance*                                 pInstance)
{
	RDCASSERT(pCreateInfo);

	// don't support any extensions for this createinfo
	RDCASSERT(pCreateInfo->pApplicationInfo == NULL || pCreateInfo->pApplicationInfo->pNext == NULL);

	VkLayerInstanceCreateInfo *layerCreateInfo = (VkLayerInstanceCreateInfo *)pCreateInfo->pNext;

	// step through the chain of pNext until we get to the link info
	while(layerCreateInfo &&
				(layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || 
				 layerCreateInfo->function != VK_LAYER_LINK_INFO)
			)
	{
		// we don't handle any pNext elements other than this create info struct
		RDCASSERT(layerCreateInfo->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO);
		layerCreateInfo = (VkLayerInstanceCreateInfo *)layerCreateInfo->pNext;
	}
	RDCASSERT(layerCreateInfo);
	// make sure there are no elements after this, that we don't handle
	RDCASSERT(layerCreateInfo->pNext == NULL);

	PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	// move chain on for next layer
	layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

	PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance)gpa(VK_NULL_HANDLE, "vkCreateInstance");
	
	VkResult ret = createFunc(pCreateInfo, pAllocator, pInstance);

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
#define CheckExt(name) if(!strcmp(pCreateInfo->ppEnabledExtensionNames[i], STRINGIZE(name))) { record->instDevInfo->name = true; }

	for(uint32_t i=0; i < pCreateInfo->enabledExtensionCount; i++)
	{
		CheckInstanceExts();
	}

	InitInstanceExtensionTables(m_Instance);

	RenderDoc::Inst().AddDeviceFrameCapturer(LayerDisp(m_Instance), this);
	
	m_DbgMsgCallback = VK_NULL_HANDLE;
	m_PhysicalDevice = VK_NULL_HANDLE;
	m_Device = VK_NULL_HANDLE;
	m_QueueFamilyIdx = ~0U;
	m_Queue = VK_NULL_HANDLE;
	m_InternalCmds.Reset();

	if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode && ObjDisp(m_Instance)->CreateDebugReportCallbackEXT)
	{
		VkDebugReportCallbackCreateInfoEXT debugInfo = {};
		debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		debugInfo.pNext = NULL;
		debugInfo.pfnCallback = &DebugCallbackStatic;
		debugInfo.pUserData = this;
		debugInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT|VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT|VK_DEBUG_REPORT_ERROR_BIT_EXT;

		ObjDisp(m_Instance)->CreateDebugReportCallbackEXT(Unwrap(m_Instance), &debugInfo, NULL, &m_DbgMsgCallback);
	}

	if(ret == VK_SUCCESS)
	{
		RDCLOG("Initialised capture layer in Vulkan instance.");
	}

	return ret;
}

void WrappedVulkan::Shutdown()
{
	// flush out any pending commands
	SubmitCmds();
	FlushQ();
	
	// since we didn't create proper registered resources for our command buffers,
	// they won't be taken down properly with the pool. So we release them (just our
	// data) here.
	for(size_t i=0; i < m_InternalCmds.freecmds.size(); i++)
		GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freecmds[i]);

	// destroy the pool
	ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.cmdpool), NULL);
	GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.cmdpool);
	
	// we do more in Shutdown than the equivalent vkDestroyInstance since on replay there's
	// no explicit vkDestroyDevice, we destroy the device here then the instance

	// destroy any replay objects that aren't specifically to do with the frame capture
	for(size_t i=0; i < m_CleanupMems.size(); i++)
	{
		ObjDisp(m_Device)->FreeMemory(Unwrap(m_Device), Unwrap(m_CleanupMems[i]), NULL);
		GetResourceManager()->ReleaseWrappedResource(m_CleanupMems[i]);
	}
	m_CleanupMems.clear();

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

	m_PhysicalDevices.clear();

	for(size_t i=0; i < m_QueueFamilies.size(); i++)
		delete[] m_QueueFamilies[i];

	m_QueueFamilies.clear();

	// finally destroy device then instance
	vt->DestroyDevice(dev, NULL);
	vit->DestroyInstance(inst, NULL);
}

void WrappedVulkan::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
	RDCASSERT(m_Instance == instance);

	// the device should already have been destroyed, assuming that the
	// application is well behaved. If not, we just leak.

	ObjDisp(m_Instance)->DestroyInstance(Unwrap(m_Instance), NULL);
	GetResourceManager()->ReleaseWrappedResource(m_Instance);
	
	RenderDoc::Inst().RemoveDeviceFrameCapturer(LayerDisp(m_Instance));

	m_Instance = VK_NULL_HANDLE;
}

bool WrappedVulkan::Serialise_vkEnumeratePhysicalDevices(
		Serialiser*                                 localSerialiser,
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
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
	
	if(m_State >= WRITING)
	{
		ObjDisp(instance)->GetPhysicalDeviceProperties(Unwrap(*pPhysicalDevices), &physProps);
		ObjDisp(instance)->GetPhysicalDeviceMemoryProperties(Unwrap(*pPhysicalDevices), &memProps);
		ObjDisp(instance)->GetPhysicalDeviceFeatures(Unwrap(*pPhysicalDevices), &physFeatures);
	}

	localSerialiser->Serialise("physProps", physProps);
	localSerialiser->Serialise("memProps", memProps);
	localSerialiser->Serialise("physFeatures", physFeatures);

	VkPhysicalDevice pd = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		pd = *pPhysicalDevices;
	}
	else
	{
		uint32_t count;
		VkPhysicalDevice *devices;

		instance = GetResourceManager()->GetLiveHandle<VkInstance>(inst);
		VkResult vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, NULL);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		RDCASSERT(count > physIndex);
		devices = new VkPhysicalDevice[count];

		if(physIndex >= m_PhysicalDevices.size())
		{
			m_PhysicalDevices.resize(physIndex+1);
			m_MemIdxMaps.resize(physIndex+1);
		}

		vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		// PORTABILITY match up physical devices to those available on replay

		pd = devices[physIndex];

		for(size_t i=0; i < m_PhysicalDevices.size(); i++)
		{
			// physical devices might be re-created inside EnumeratePhysicalDevices every time, so
			// we need to re-wrap any previously enumerated physical devices
			if(m_PhysicalDevices[i] != VK_NULL_HANDLE)
			{
				RDCASSERTNOTEQUAL(i, physIndex);
				GetWrapped(m_PhysicalDevices[i])->RewrapObject(devices[i]);
			}
		}

		SAFE_DELETE_ARRAY(devices);

		GetResourceManager()->WrapResource(instance, pd);
		GetResourceManager()->AddLiveResource(physId, pd);

		m_PhysicalDevices[physIndex] = pd;

		uint32_t *storedMap = new uint32_t[32];
		memcpy(storedMap, memIdxMap, sizeof(memIdxMap));
		m_MemIdxMaps[physIndex] = storedMap;

		RDCLOG("Captured log describes physical device %u:", physIndex);
		RDCLOG("   - %s (ver %x) - %04x:%04x", physProps.deviceName, physProps.driverVersion, physProps.vendorID, physProps.deviceID);

		ObjDisp(pd)->GetPhysicalDeviceProperties(Unwrap(pd), &physProps);
		ObjDisp(pd)->GetPhysicalDeviceMemoryProperties(Unwrap(pd), &memProps);
		ObjDisp(pd)->GetPhysicalDeviceFeatures(Unwrap(pd), &physFeatures);
		
		RDCLOG("Replaying on physical device %u:", physIndex);
		RDCLOG("   - %s (ver %x) - %04x:%04x", physProps.deviceName, physProps.driverVersion, physProps.vendorID, physProps.deviceID);

	}

	return true;
}

VkResult WrappedVulkan::vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
{
	uint32_t count;

	VkResult vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, NULL);

	if(vkr != VK_SUCCESS)
		return vkr;

	VkPhysicalDevice *devices = new VkPhysicalDevice[count];

	vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	m_PhysicalDevices.resize(count);
	
	for(uint32_t i=0; i < count; i++)
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
	}

	if(pPhysicalDeviceCount) *pPhysicalDeviceCount = count;
	if(pPhysicalDevices) memcpy(pPhysicalDevices, devices, count*sizeof(VkPhysicalDevice));

	SAFE_DELETE_ARRAY(devices);

	return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkCreateDevice(
		Serialiser*                                 localSerialiser,
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
		VkDevice*                                   pDevice)
{
	SERIALISE_ELEMENT(ResourceId, physId, GetResID(physicalDevice));
	SERIALISE_ELEMENT(VkDeviceCreateInfo, serCreateInfo, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(*pDevice));

	if(m_State == READING)
	{
		// we must make any modifications locally, so the free of pointers
		// in the serialised VkDeviceCreateInfo don't double-free
		VkDeviceCreateInfo createInfo = serCreateInfo;

		// disable this extension as we might have captured it but we don't need
		// to replay it
		for(uint32_t i=0; i < createInfo.enabledExtensionCount; i++)
		{
			const char **extNames = (const char **)createInfo.ppEnabledExtensionNames;
			if(!strcmp(extNames[i], DEBUG_MARKER_EXTENSION_NAME))
			{
				for(uint32_t j=i; j < createInfo.enabledExtensionCount-1; j++)
					extNames[j] = extNames[j+1];
				createInfo.enabledExtensionCount--;
			}
		}

		std::vector<string> Layers;
		for(uint32_t i=0; i < createInfo.enabledLayerCount; i++)
			Layers.push_back(createInfo.ppEnabledLayerNames[i]);

		StripUnwantedLayers(Layers);
		
#if defined(FORCE_VALIDATION_LAYERS)
		Layers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

		createInfo.enabledLayerCount = (uint32_t)Layers.size();

		const char **layerArray = NULL;
		if(!Layers.empty())
		{
			layerArray = new const char *[createInfo.enabledLayerCount];
			
			for(uint32_t i=0; i < createInfo.enabledLayerCount; i++)
				layerArray[i] = Layers[i].c_str();

			createInfo.ppEnabledLayerNames = layerArray;
		}

		physicalDevice = GetResourceManager()->GetLiveHandle<VkPhysicalDevice>(physId);

		VkDevice device;

		uint32_t qCount = 0;
		ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);

		VkQueueFamilyProperties *props = new VkQueueFamilyProperties[qCount];
		ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);

		bool found = false;
		uint32_t qFamilyIdx = 0;
		VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT);

		// for queue priorities, if we need it
		float one = 1.0f;

		// if we need to change the requested queues, it will point to this
		VkDeviceQueueCreateInfo *modQueues = NULL;

		for(uint32_t i=0; i < createInfo.queueCreateInfoCount; i++)
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

			for(uint32_t i=0; i < qCount; i++)
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
			}
			else
			{
				// we found the queue family, add it
				modQueues = new VkDeviceQueueCreateInfo[createInfo.queueCreateInfoCount + 1];
				for(uint32_t i=0; i < createInfo.queueCreateInfoCount; i++)
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
		if(createInfo.pEnabledFeatures != NULL) enabledFeatures = *createInfo.pEnabledFeatures;
		createInfo.pEnabledFeatures = &enabledFeatures;

		VkPhysicalDeviceFeatures availFeatures = {0};
		ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &availFeatures);

		if(availFeatures.fillModeNonSolid)
			enabledFeatures.fillModeNonSolid = true;
		else
			RDCWARN("fillModeNonSolid = false, wireframe overlay will be solid");
		
		if(availFeatures.robustBufferAccess)
			enabledFeatures.robustBufferAccess = true;
		else
			RDCWARN("robustBufferAccess = false, out of bounds access due to bugs in application or RenderDoc may cause crashes");

		if(availFeatures.vertexPipelineStoresAndAtomics)
			enabledFeatures.vertexPipelineStoresAndAtomics = true;
		else
			RDCWARN("vertexPipelineStoresAndAtomics = false, output mesh data will not be available");

		uint32_t numExts = 0;

		VkResult vkr = ObjDisp(physicalDevice)->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &numExts, NULL);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		VkExtensionProperties *exts = new VkExtensionProperties[numExts];

		vkr = ObjDisp(physicalDevice)->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), NULL, &numExts, exts);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		for(uint32_t i=0; i < numExts; i++)
			RDCLOG("Ext %u: %s (%u)", i, exts[i].extensionName, exts[i].specVersion);

		SAFE_DELETE_ARRAY(exts);

		// PORTABILITY check that extensions and layers supported in capture (from createInfo) are supported in replay

		vkr = GetDeviceDispatchTable(NULL)->CreateDevice(Unwrap(physicalDevice), &createInfo, NULL, &device);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(device, device);
		GetResourceManager()->AddLiveResource(devId, device);
		
		InitDeviceReplayTables(Unwrap(device));

		RDCASSERT(m_Device == VK_NULL_HANDLE); // MULTIDEVICE
		
		m_PhysicalDevice = physicalDevice;
		m_Device = device;

		m_QueueFamilyIdx = qFamilyIdx;

		if(m_InternalCmds.cmdpool == VK_NULL_HANDLE)
		{
			VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qFamilyIdx };
			vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL, &m_InternalCmds.cmdpool);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.cmdpool);
		}
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

		ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

		for(int i=VK_FORMAT_BEGIN_RANGE+1; i < VK_FORMAT_END_RANGE; i++)
			ObjDisp(physicalDevice)->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), VkFormat(i), &m_PhysicalDeviceData.fmtprops[i]);

		m_PhysicalDeviceData.readbackMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
		m_PhysicalDeviceData.uploadMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
		m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		for(size_t i=0; i < m_PhysicalDevices.size(); i++)
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
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDevice(
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkDevice*                                   pDevice)
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

	for(uint32_t i=0; i < createInfo.queueCreateInfoCount; i++)
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

		for(uint32_t i=0; i < qCount; i++)
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
		for(uint32_t i=0; i < createInfo.queueCreateInfoCount; i++)
			modQueues[i] = createInfo.pQueueCreateInfos[i];

		modQueues[createInfo.queueCreateInfoCount].queueFamilyIndex = qFamilyIdx;
		modQueues[createInfo.queueCreateInfoCount].queueCount = 1;
		modQueues[createInfo.queueCreateInfoCount].pQueuePriorities = &one;

		createInfo.pQueueCreateInfos = modQueues;
		createInfo.queueCreateInfoCount++;
	}

	SAFE_DELETE_ARRAY(props);

	m_QueueFamilies.resize(createInfo.queueCreateInfoCount);
	for(size_t i=0; i < createInfo.queueCreateInfoCount; i++)
	{
		uint32_t family = createInfo.pQueueCreateInfos[i].queueFamilyIndex;
		uint32_t count = createInfo.pQueueCreateInfos[i].queueCount;
		m_QueueFamilies.resize(RDCMAX(m_QueueFamilies.size(), size_t(family+1)));

		m_QueueFamilies[family] = new VkQueue[count];
		for(uint32_t q=0; q < count; q++)
			m_QueueFamilies[family][q] = VK_NULL_HANDLE;
	}

	VkLayerDeviceCreateInfo *layerCreateInfo = (VkLayerDeviceCreateInfo *)pCreateInfo->pNext;

	// step through the chain of pNext until we get to the link info
	while(layerCreateInfo &&
				(layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || 
				 layerCreateInfo->function != VK_LAYER_LINK_INFO)
			)
	{
		// we don't handle any pNext elements other than this create info struct
		RDCASSERT(layerCreateInfo->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);
		layerCreateInfo = (VkLayerDeviceCreateInfo *)layerCreateInfo->pNext;
	}
	RDCASSERT(layerCreateInfo);

	// make sure there are no elements after this, that we don't handle
	RDCASSERT(layerCreateInfo->pNext == NULL);

	PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
	PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	// move chain on for next layer
	layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

	PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice)gipa(VK_NULL_HANDLE, "vkCreateDevice");

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
#define CheckExt(name) record->instDevInfo->name = GetRecord(m_Instance)->instDevInfo->name;

			// inherit extension enablement from instance, that way GetDeviceProcAddress can check
			// for enabled extensions for instance functions
			CheckInstanceExts();

#undef CheckExt
#define CheckExt(name) if(!strcmp(createInfo.ppEnabledExtensionNames[i], STRINGIZE(name))) { record->instDevInfo->name = true; }

			for(uint32_t i=0; i < createInfo.enabledExtensionCount; i++)
			{
				CheckDeviceExts();
			}
		
			InitDeviceExtensionTables(*pDevice);

			GetRecord(m_Instance)->AddParent(record);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pDevice);
		}

		VkDevice device = *pDevice;

		RDCASSERT(m_Device == VK_NULL_HANDLE); // MULTIDEVICE

		m_PhysicalDevice = physicalDevice;
		m_Device = device;

		m_QueueFamilyIdx = qFamilyIdx;

		if(m_InternalCmds.cmdpool == VK_NULL_HANDLE)
		{
			VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, qFamilyIdx };
			vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, NULL, &m_InternalCmds.cmdpool);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.cmdpool);
		}
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

		ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

		m_PhysicalDeviceData.readbackMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
		m_PhysicalDeviceData.uploadMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
		m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		m_PhysicalDeviceData.fakeMemProps = GetRecord(physicalDevice)->memProps;

		m_DebugManager = new VulkanDebugManager(this, device);
	}

	SAFE_DELETE_ARRAY(modQueues);

	return ret;
}

void WrappedVulkan::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
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
	for(size_t i=0; i < m_InternalCmds.freecmds.size(); i++)
		GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freecmds[i]);

	// destroy our command pool
	if(m_InternalCmds.cmdpool != VK_NULL_HANDLE)
	{
		ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.cmdpool), NULL);
		GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.cmdpool);
	}
	
	for(size_t i=0; i < m_InternalCmds.freesems.size(); i++)
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

bool WrappedVulkan::Serialise_vkDeviceWaitIdle(Serialiser* localSerialiser, VkDevice device)
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
