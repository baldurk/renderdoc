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
//
// Note that during capture we have vkDestroyDevice before vkDestroyDevice that does most of the work.
//
// Also we assume correctness from the application, that all objects are destroyed before the device and
// instance are destroyed. We only clean up after our own objects.

void WrappedVulkan::Initialise(VkInitParams &params)
{
	m_InitParams = params;

	params.AppName = string("RenderDoc @ ") + params.AppName;
	params.EngineName = string("RenderDoc @ ") + params.EngineName;

	// VKTODOLOW verify that layers/extensions are available

	// don't try and create our own layer on replay!
	for(auto it = params.Layers.begin(); it != params.Layers.end(); ++it)
	{
		if(*it == "RenderDoc")
		{
			params.Layers.erase(it);
			break;
		}
	}

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
	m_Device = VK_NULL_HANDLE;
	m_QueueFamilyIdx = ~0U;
	m_Queue = VK_NULL_HANDLE;
	m_InternalCmds.Reset();

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

	// don't support any extensions for this createinfo
	RDCASSERT(pCreateInfo->pAppInfo == NULL || pCreateInfo->pAppInfo->pNext == NULL);
	RDCASSERT(pCreateInfo->pNext == NULL);

	m_Instance = *pInstance;

	VkResult ret = GetInstanceDispatchTable(*pInstance)->CreateInstance(pCreateInfo, &m_Instance);

	GetResourceManager()->WrapResource(m_Instance, m_Instance);

	if(ret != VK_SUCCESS)
		return ret;

	// should only be called during capture
	RDCASSERT(m_State >= WRITING);

	m_InitParams.Set(pCreateInfo, GetResID(m_Instance));
	GetResourceManager()->AddResourceRecord(m_Instance);

	*pInstance = m_Instance;
	
	m_DbgMsgCallback = VK_NULL_HANDLE;
	m_Device = VK_NULL_HANDLE;
	m_QueueFamilyIdx = ~0U;
	m_Queue = VK_NULL_HANDLE;
	m_InternalCmds.Reset();

	return VK_SUCCESS;
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
	ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.m_CmdPool));
	GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.m_CmdPool);
	
	// we do more in Shutdown than the equivalent vkDestroyInstance since on replay there's
	// no explicit vkDestroyDevice, we destroy the device here then the instance

	// destroy any replay objects that aren't specifically to do with the frame capture
	for(size_t i=0; i < m_CleanupMems.size(); i++)
	{
		ObjDisp(m_Device)->FreeMemory(Unwrap(m_Device), Unwrap(m_CleanupMems[i]));
		GetResourceManager()->ReleaseWrappedResource(m_CleanupMems[i]);
	}
	m_CleanupMems.clear();

	// destroy debug manager and any objects it created
	SAFE_DELETE(m_DebugManager);

	if(ObjDisp(m_Instance)->DbgDestroyMsgCallback && m_DbgMsgCallback != VK_NULL_HANDLE)
		ObjDisp(m_Instance)->DbgDestroyMsgCallback(Unwrap(m_Instance), m_DbgMsgCallback);

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

	m_Device = VK_NULL_HANDLE;
	m_Instance = VK_NULL_HANDLE;

	// finally destroy device then instance
	vt->DestroyDevice(dev);
	vit->DestroyInstance(inst);
}

void WrappedVulkan::vkDestroyInstance(VkInstance instance)
{
	RDCASSERT(m_Instance == instance);

	// the device should already have been destroyed, assuming that the
	// application is well behaved. If not, we just leak.

	ObjDisp(m_Instance)->DestroyInstance(Unwrap(m_Instance));
	GetResourceManager()->ReleaseWrappedResource(m_Instance);

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
		
		if(physIndex >= m_PhysicalDevices.size())
		{
			m_PhysicalDevices.resize(physIndex+1);
			m_MemIdxMaps.resize(physIndex+1);
		}

		m_PhysicalDevices[physIndex] = pd;

		uint32_t *storedMap = new uint32_t[32];
		memcpy(storedMap, memIdxMap, sizeof(memIdxMap));
		m_MemIdxMaps[physIndex] = storedMap;
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
	RDCASSERT(vkr == VK_SUCCESS);

	m_PhysicalDevices.resize(count);
	
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
		
		// don't try and create our own layer on replay!
		for(uint32_t i=0; i < createInfo.layerCount; i++)
		{
			const char **layerNames = (const char **)createInfo.ppEnabledLayerNames;
			if(!strcmp(layerNames[i], "RenderDoc"))
			{
				for(uint32_t j=i; j < createInfo.layerCount-1; j++)
					layerNames[j] = layerNames[j+1];
				createInfo.layerCount--;
			}
		}

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
		ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &availFeatures);

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
		
		InitDeviceReplayTables(Unwrap(device));

		RDCASSERT(m_Device == VK_NULL_HANDLE); // VKTODOLOW multiple devices are not supported

		m_Device = device;

		m_QueueFamilyIdx = qFamilyIdx;

		if(m_InternalCmds.m_CmdPool == VK_NULL_HANDLE)
		{
			VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, qFamilyIdx, VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
			vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, &m_InternalCmds.m_CmdPool);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.m_CmdPool);
		}
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

		ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

		m_PhysicalDeviceData.readbackMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT);
		m_PhysicalDeviceData.uploadMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
		m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_DEVICE_ONLY, 0);

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

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDevice);
			RDCASSERT(record);

			record->AddChunk(chunk);

			record->memIdxMap = GetRecord(physicalDevice)->memIdxMap;

			GetRecord(m_Instance)->AddParent(record);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pDevice);
		}

		VkDevice device = *pDevice;

		RDCASSERT(m_Device == VK_NULL_HANDLE); // VKTODOLOW multiple devices are not supported

		m_Device = device;

		m_QueueFamilyIdx = qFamilyIdx;

		if(m_InternalCmds.m_CmdPool == VK_NULL_HANDLE)
		{
			VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, qFamilyIdx, VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
			vkr = ObjDisp(device)->CreateCommandPool(Unwrap(device), &poolInfo, &m_InternalCmds.m_CmdPool);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), m_InternalCmds.m_CmdPool);
		}
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.props);
		
		ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), &m_PhysicalDeviceData.memProps);

		ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), &m_PhysicalDeviceData.features);

		m_PhysicalDeviceData.readbackMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT);
		m_PhysicalDeviceData.uploadMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
		m_PhysicalDeviceData.GPULocalMemIndex = m_PhysicalDeviceData.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_DEVICE_ONLY, 0);

		m_PhysicalDeviceData.fakeMemProps = GetRecord(physicalDevice)->memProps;

		m_DebugManager = new VulkanDebugManager(this, device);
	}

	SAFE_DELETE_ARRAY(modQueues);

	return ret;
}

void WrappedVulkan::vkDestroyDevice(VkDevice device)
{
	// flush out any pending commands
	SubmitCmds();
	FlushQ();
	
	// VKTODOLOW handle multiple devices - this function will need to check
	// if the device is the one we used for debugmanager/cmd pool etc, and
	// only remove child queues and resources (instead of doing full resource
	// manager shutdown)
	RDCASSERT(m_Device == device);

	// delete all debug manager objects
	SAFE_DELETE(m_DebugManager);

	// since we didn't create proper registered resources for our command buffers,
	// they won't be taken down properly with the pool. So we release them (just our
	// data) here.
	for(size_t i=0; i < m_InternalCmds.freecmds.size(); i++)
		GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.freecmds[i]);

	// destroy our command pool
	if(m_InternalCmds.m_CmdPool != VK_NULL_HANDLE)
	{
		ObjDisp(m_Device)->DestroyCommandPool(Unwrap(m_Device), Unwrap(m_InternalCmds.m_CmdPool));
		GetResourceManager()->ReleaseWrappedResource(m_InternalCmds.m_CmdPool);
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
	ObjDisp(m_Device)->DestroyDevice(Unwrap(m_Device));
	GetResourceManager()->ReleaseWrappedResource(m_Device);
	m_Device = VK_NULL_HANDLE;
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
