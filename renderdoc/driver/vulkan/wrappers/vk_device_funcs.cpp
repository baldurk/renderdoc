/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

// Init/shutdown order:
//
// On capture, WrappedVulkan is new'd and delete'd before vkCreateInstance() and after vkDestroyInstance()
// On replay,  WrappedVulkan is new'd and delete'd before Initialise()       and after Shutdown()
//
// The class constructor and destructor handle only *non-API* work. All API objects must be created and
// torn down in the latter functions (vkCreateInstance/vkDestroyInstance during capture, and
// Initialise/Shutdown during replay).

void WrappedVulkan::Initialise(VkInitParams &params)
{
	m_InitParams = params;

	params.AppName = string("RenderDoc (") + params.AppName + ")";
	params.EngineName = string("RenderDoc (") + params.EngineName + ")";

	// VKTODOLOW verify that layers/extensions are available

	const char **layerscstr = new const char *[params.Layers.size()];
	for(size_t i=0; i < params.Layers.size(); i++)
		layerscstr[i] = params.Layers[i].c_str();

#if defined(FORCE_VALIDATION_LAYER)
	params.Extensions.push_back("DEBUG_REPORT");
#endif

	const char **extscstr = new const char *[params.Extensions.size()];
	for(size_t i=0; i < params.Extensions.size(); i++)
		extscstr[i] = params.Extensions[i].c_str();

	VkApplicationInfo appinfo = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
			params.AppName.c_str(), params.AppVersion,
			params.EngineName.c_str(), params.EngineVersion,
			VK_API_VERSION,
	};

	VkInstanceCreateInfo instinfo = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL,
			&appinfo, NULL,
			(uint32_t)params.Layers.size(), layerscstr,
			(uint32_t)params.Extensions.size(), extscstr,
	};

	m_Instance = VK_NULL_HANDLE;

	VkResult ret = GetInstanceDispatchTable(NULL)->CreateInstance(&instinfo, &m_Instance);

	InitInstanceReplayTables(m_Instance);

	GetResourceManager()->WrapResource(m_Instance, m_Instance);
	GetResourceManager()->AddLiveResource(params.InstanceID, m_Instance);

	m_DbgMsgCallback = VK_NULL_HANDLE;

	if(ObjDisp(m_Instance)->DbgCreateMsgCallback)
	{
		ObjDisp(m_Instance)->DbgCreateMsgCallback(Unwrap(m_Instance),
			VK_DBG_REPORT_WARN_BIT|VK_DBG_REPORT_PERF_WARN_BIT|VK_DBG_REPORT_ERROR_BIT,
			(PFN_vkDbgMsgCallback)&DebugCallbackStatic, this, &m_DbgMsgCallback);
	}

	SAFE_DELETE_ARRAY(layerscstr);
	SAFE_DELETE_ARRAY(extscstr);
}

VkResult WrappedVulkan::vkCreateInstance(
		const VkInstanceCreateInfo*                 pCreateInfo,
		VkInstance*                                 pInstance)
{
	RDCASSERT(pCreateInfo);

	RDCASSERT(pCreateInfo->pAppInfo == NULL || pCreateInfo->pAppInfo->pNext == NULL);
	RDCASSERT(pCreateInfo->pNext == NULL);

	m_Instance = *pInstance;

	VkResult ret = GetInstanceDispatchTable(*pInstance)->CreateInstance(pCreateInfo, &m_Instance);

	GetResourceManager()->WrapResource(m_Instance, m_Instance);

	if(ret != VK_SUCCESS)
		return ret;
	
	if(m_State >= WRITING)
	{
		m_InitParams.Set(pCreateInfo, GetResID(m_Instance));
		m_InstanceRecord = GetResourceManager()->AddResourceRecord(m_Instance);
	}

	*pInstance = m_Instance;

	return VK_SUCCESS;
}

void WrappedVulkan::Shutdown()
{
	// destroy any replay objects that aren't specifically to do with the frame capture
	VkDevice dev = GetDev();
	for(size_t i=0; i < m_FreeMems.size(); i++)
	{
		ObjDisp(dev)->FreeMemory(Unwrap(dev), Unwrap(m_FreeMems[i]));
		GetResourceManager()->ReleaseWrappedResource(m_FreeMems[i]);
	}
	m_FreeMems.clear();
	
	if(ObjDisp(m_Instance)->DbgDestroyMsgCallback && m_DbgMsgCallback != VK_NULL_HANDLE)
		ObjDisp(m_Instance)->DbgDestroyMsgCallback(Unwrap(m_Instance), m_DbgMsgCallback);

	// need to store the unwrapped devices and instance to destroy the
	// API object after resource manager shutdown
	vector<VkDevice> devs;
	VkInstance inst = Unwrap(m_Instance);
	
	for(size_t i=0; i < m_Devices.size(); i++)
		devs.push_back(Unwrap(m_Devices[i]));

	m_Devices.clear();
	m_Instance = VK_NULL_HANDLE;

	// this destroys the wrapped objects for the devices and instances
	m_ResourceManager->Shutdown();

	// second last step, destroy devices
	for(size_t i=0; i < devs.size(); i++)
	{
		VkDevice dev = devs[i];
		ObjDisp(dev)->DestroyDevice(Unwrap(dev));
	}

	// finally destroy instance
	ObjDisp(inst)->DestroyInstance(Unwrap(inst));
}

void WrappedVulkan::vkDestroyInstance(VkInstance instance)
{
	RDCASSERT(m_Instance == instance);

	// records must be deleted before resource manager shutdown
	if(m_FrameCaptureRecord)
	{
		RDCASSERT(m_FrameCaptureRecord->GetRefCount() == 1);
		m_FrameCaptureRecord->Delete(GetResourceManager());
		m_FrameCaptureRecord = NULL;
	}

	// need to store the unwrapped devices and instance to destroy the
	// API object after resource manager shutdown
	vector<VkDevice> devs;
	VkInstance inst = Unwrap(m_Instance);
	
	for(size_t i=0; i < m_Devices.size(); i++)
		devs.push_back(Unwrap(m_Devices[i]));

	m_Devices.clear();
	m_Instance = VK_NULL_HANDLE;

	// this destroys the wrapped objects for the devices and instances
	m_ResourceManager->Shutdown();

	// second last step, destroy devices
	for(size_t i=0; i < devs.size(); i++)
	{
		VkDevice dev = devs[i];
		ObjDisp(dev)->DestroyDevice(Unwrap(dev));
	}

	// finally destroy instance
	ObjDisp(inst)->DestroyInstance(Unwrap(inst));
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
		RDCASSERT(vkr == VK_SUCCESS);

		RDCASSERT(count > physIndex);
		devices = new VkPhysicalDevice[count];

		vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW match up physical devices to those available on replay

		pd = devices[physIndex];

		SAFE_DELETE_ARRAY(devices);

		GetResourceManager()->WrapResource(instance, pd);
		GetResourceManager()->AddLiveResource(physId, pd);
	}

	PhysicalDeviceData data;
	data.inst = instance;
	data.phys = pd;

	ObjDisp(pd)->GetPhysicalDeviceMemoryProperties(Unwrap(pd), &data.memProps);

	ObjDisp(pd)->GetPhysicalDeviceFeatures(Unwrap(pd), &data.features);

	data.readbackMemIndex = data.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT);
	data.uploadMemIndex = data.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
	data.GPULocalMemIndex = data.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_DEVICE_ONLY, 0);

	m_PhysicalDeviceData.push_back(data);

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
	RDCASSERT(vkr == VK_SUCCESS);
	
	for(uint32_t i=0; i < count; i++)
	{
		// it's perfectly valid for enumerate type functions to return the same handle
		// each time. If that happens, we will already have a wrapper created so just
		// return the wrapped object to the user and do nothing else
		if(GetResourceManager()->HasWrapper(ToTypedHandle(devices[i])))
		{
			devices[i] = (VkPhysicalDevice)GetResourceManager()->GetWrapper(ToTypedHandle(devices[i]));
		}
		else
		{
			GetResourceManager()->WrapResource(instance, devices[i]);

			if(m_State >= WRITING)
			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(ENUM_PHYSICALS);
				Serialise_vkEnumeratePhysicalDevices(localSerialiser, instance, &i, &devices[i]);

				m_InstanceRecord->AddChunk(scope.Get());
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

		physicalDevice = GetResourceManager()->GetLiveHandle<VkPhysicalDevice>(physId);

		VkDevice device;

		uint32_t qCount = 0;
		VkResult vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		VkQueueFamilyProperties *props = new VkQueueFamilyProperties[qCount];
		vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);
		RDCASSERT(vkr == VK_SUCCESS);

		bool found = false;
		uint32_t qFamilyIdx = 0;
		VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT);

		// if we need to change the requested queues, it will point to this
		VkDeviceQueueCreateInfo *modQueues = NULL;

		for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
		{
			uint32_t idx = createInfo.pRequestedQueues[i].queueFamilyIndex;
			RDCASSERT(idx < qCount);

			// this requested queue is one we can use too
			if((props[idx].queueFlags & search) == search && createInfo.pRequestedQueues[i].queueCount > 0)
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
				modQueues = new VkDeviceQueueCreateInfo[createInfo.queueRecordCount + 1];
				for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
					modQueues[i] = createInfo.pRequestedQueues[i];

				modQueues[createInfo.queueRecordCount].queueFamilyIndex = qFamilyIdx;
				modQueues[createInfo.queueRecordCount].queueCount = 1;

				createInfo.pRequestedQueues = modQueues;
				createInfo.queueRecordCount++;
			}
		}
		
		SAFE_DELETE_ARRAY(props);

		VkPhysicalDeviceFeatures enabledFeatures = {0};
		if(createInfo.pEnabledFeatures != NULL) enabledFeatures = *createInfo.pEnabledFeatures;
		createInfo.pEnabledFeatures = &enabledFeatures;

		VkPhysicalDeviceFeatures availFeatures = {0};
		
		for(size_t i=0; i < m_PhysicalDeviceData.size(); i++)
		{
			if(m_PhysicalDeviceData[i].phys == physicalDevice)
			{
				availFeatures = m_PhysicalDeviceData[i].features;
				break;
			}
		}

		if(availFeatures.fillModeNonSolid)
			enabledFeatures.fillModeNonSolid = true;
		else
			RDCWARN("fillModeNonSolid = false, wireframe overlay will be solid");
		
		if(availFeatures.robustBufferAccess)
			enabledFeatures.robustBufferAccess = true;
		else
			RDCWARN("robustBufferAccess = false, out of bounds access due to bugs in application or RenderDoc may cause crashes");
		
		// VKTODOLOW: check that extensions and layers supported in capture (from createInfo) are supported in replay

		VkResult ret = GetDeviceDispatchTable(NULL)->CreateDevice(Unwrap(physicalDevice), &createInfo, &device);

		GetResourceManager()->WrapResource(device, device);
		GetResourceManager()->AddLiveResource(devId, device);

		found = false;

		for(size_t i=0; i < m_PhysicalDeviceData.size(); i++)
		{
			if(m_PhysicalDeviceData[i].phys == physicalDevice)
			{
				InitDeviceReplayTables(Unwrap(device));

				// VKTODOLOW not handling multiple devices per physical devices
				RDCASSERT(m_PhysicalDeviceData[i].dev == VK_NULL_HANDLE);
				m_PhysicalDeviceData[i].dev = device;

				m_PhysicalDeviceData[i].qFamilyIdx = qFamilyIdx;
				
				// VKTODOMED this is a hack - need a more reliable way of doing this
				// when we serialise the relevant vkGetDeviceQueue, we'll search for this handle and replace it with the wrapped version
				VkResult vkr = ObjDisp(device)->GetDeviceQueue(Unwrap(device), qFamilyIdx, 0, &m_PhysicalDeviceData[i].q);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, qFamilyIdx, VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
				vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, &m_PhysicalDeviceData[i].cmdpool);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), m_PhysicalDeviceData[i].cmdpool);

				found = true;
				break;
			}
		}

		SAFE_DELETE_ARRAY(modQueues);

		if(!found)
			RDCERR("Couldn't find VkPhysicalDevice for vkCreateDevice!");
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDevice(
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		VkDevice*                                   pDevice)
{
	VkDeviceCreateInfo createInfo = *pCreateInfo;

	uint32_t qCount = 0;
	VkResult vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, NULL);
	RDCASSERT(vkr == VK_SUCCESS);

	VkQueueFamilyProperties *props = new VkQueueFamilyProperties[qCount];
	vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), &qCount, props);
	RDCASSERT(vkr == VK_SUCCESS);

	// find a queue that supports all capabilities, and if one doesn't exist, add it.
	bool found = false;
	uint32_t qFamilyIdx = 0;
	VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT);

	// if we need to change the requested queues, it will point to this
	VkDeviceQueueCreateInfo *modQueues = NULL;

	for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
	{
		uint32_t idx = createInfo.pRequestedQueues[i].queueFamilyIndex;
		RDCASSERT(idx < qCount);

		// this requested queue is one we can use too
		if((props[idx].queueFlags & search) == search && createInfo.pRequestedQueues[i].queueCount > 0)
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
			return VK_UNSUPPORTED;
		}

		// we found the queue family, add it
		modQueues = new VkDeviceQueueCreateInfo[createInfo.queueRecordCount + 1];
		for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
			modQueues[i] = createInfo.pRequestedQueues[i];

		modQueues[createInfo.queueRecordCount].queueFamilyIndex = qFamilyIdx;
		modQueues[createInfo.queueRecordCount].queueCount = 1;

		createInfo.pRequestedQueues = modQueues;
		createInfo.queueRecordCount++;
	}

	SAFE_DELETE_ARRAY(props);

	RDCDEBUG("Might want to fiddle with createinfo - e.g. to remove VK_RenderDoc from set of extensions or similar");

	VkResult ret = GetDeviceDispatchTable(*pDevice)->CreateDevice(Unwrap(physicalDevice), &createInfo, pDevice);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(*pDevice, *pDevice);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_DEVICE);
				Serialise_vkCreateDevice(localSerialiser, physicalDevice, &createInfo, pDevice);

				chunk = scope.Get();
			}

			m_InstanceRecord->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pDevice);
		}
		
		found = false;

		for(size_t i=0; i < m_PhysicalDeviceData.size(); i++)
		{
			if(m_PhysicalDeviceData[i].phys == physicalDevice)
			{
				m_PhysicalDeviceData[i].dev = *pDevice;

				m_PhysicalDeviceData[i].qFamilyIdx = qFamilyIdx;

				// we call our own vkGetDeviceQueue so that its initialisation is properly serialised in case when
				// the application fetches this queue it gets the same handle - the already wrapped one created
				// here will be returned.
				vkr = vkGetDeviceQueue(*pDevice, qFamilyIdx, 0, &m_PhysicalDeviceData[i].q);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, qFamilyIdx, VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
				vkr = ObjDisp(*pDevice)->CreateCommandPool(Unwrap(*pDevice), &poolInfo, &m_PhysicalDeviceData[i].cmdpool);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(*pDevice), m_PhysicalDeviceData[i].cmdpool);

				// VKTODOHIGH hack, need to properly handle multiple devices etc and
				// not have this 'current swap chain device' thing.
				m_SwapPhysDevice = (int)i;
				
				m_PhysicalDeviceData[i].debugMan = new VulkanDebugManager(this, *pDevice);

				found = true;
				break;
			}
		}

		if(!found)
			RDCERR("Couldn't find VkPhysicalDevice for vkCreateDevice!");
	}

	SAFE_DELETE_ARRAY(modQueues);

	return ret;
}

void WrappedVulkan::vkDestroyDevice(VkDevice device)
{
	// VKTODOHIGH this stuff should all be in vkDestroyInstance
	if(m_State >= WRITING)
	{
		for(size_t i=0; i < m_PhysicalDeviceData.size(); i++)
		{
			if(m_PhysicalDeviceData[i].dev == device)
			{
				if(m_PhysicalDeviceData[i].cmdpool != VK_NULL_HANDLE)
					ObjDisp(device)->DestroyCommandPool(Unwrap(device), Unwrap(m_PhysicalDeviceData[i].cmdpool));

				// VKTODOHIGH this data is needed in destructor for swapchains - order of shutdown needs to be revamped
				break;
			}
		}

		vector<VkQueue> &queues = m_InstanceRecord->queues;
		for(size_t i=0; i < queues.size(); i++)
			GetResourceManager()->ReleaseWrappedResource(queues[i]);
	}

	ObjDisp(device)->DestroyDevice(Unwrap(device));
	
	// VKTODOLOW on replay we're releasing this after resource manager
	// shutdown. Yes it's a hack, yes it's ugly.
	if(m_State >= WRITING)
		GetResourceManager()->ReleaseWrappedResource(device);
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
