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

#include "vk_common.h"
#include "vk_hookset_defs.h"

#include "os/os_specific.h"
#include "common/threading.h"

#include <unordered_map>

static VkLayerDispatchTable replayDeviceTable = {0};
static VkLayerInstanceDispatchTable replayInstanceTable = {0};

static bool replay = false;

void InitReplayTables(void *vulkanModule)
{
	replay = true;

	// not all functions will succeed - some need to be fetched through the below InitDeviceReplayTable()
	
	#undef HookInit
	#define HookInit(name) table.name = (CONCAT(PFN_vk, name))Process::GetFunctionAddress(vulkanModule, STRINGIZE(CONCAT(vk, name)))
	
	{
		VkLayerDispatchTable &table = replayDeviceTable;
		HookInit(GetDeviceProcAddr);
		HookInitVulkanDevice();
	}

	{
		VkLayerInstanceDispatchTable &table = replayInstanceTable;
		HookInit(GetInstanceProcAddr);
		HookInitVulkanInstance();
	}
}

void InitInstanceReplayTables(VkInstance instance)
{
	VkLayerInstanceDispatchTable *table = GetInstanceDispatchTable(NULL);
	RDCASSERT(table);

	// we know we'll only have one instance, so this is safe
	
#define InstanceGPA(func) table->func = (CONCAT(PFN_vk, func))table->GetInstanceProcAddr(instance, STRINGIZE(CONCAT(vk, func)));
	
	InstanceGPA(GetPhysicalDeviceSurfaceCapabilitiesKHR)
	InstanceGPA(GetPhysicalDeviceSurfaceFormatsKHR)
	InstanceGPA(GetPhysicalDeviceSurfacePresentModesKHR)
	InstanceGPA(GetPhysicalDeviceSurfaceSupportKHR)
	InstanceGPA(DbgCreateMsgCallback)
	InstanceGPA(DbgDestroyMsgCallback)

#undef InstanceGPA
}

void InitDeviceReplayTables(VkDevice device)
{
	VkLayerDispatchTable *table = GetDeviceDispatchTable(NULL);
	RDCASSERT(table);
	
	// MULTIDEVICE each device will need a replay table

#define DeviceGPA(func) table->func = (CONCAT(PFN_vk, func))table->GetDeviceProcAddr(device, STRINGIZE(CONCAT(vk, func)));
	
	DeviceGPA(CreateSwapchainKHR)
	DeviceGPA(DestroySwapchainKHR)
	DeviceGPA(GetSwapchainImagesKHR)
	DeviceGPA(AcquireNextImageKHR)
	DeviceGPA(QueuePresentKHR)

#undef DeviceGPA
}

static Threading::CriticalSection devlock;
std::map<void *, VkLayerDispatchTable> devlookup;

static Threading::CriticalSection instlock;
std::map<void *, VkLayerInstanceDispatchTable> instlookup;

static void *GetKey(void *obj)
{
	VkLayerDispatchTable **tablePtr = (VkLayerDispatchTable **)obj;
	return (void *)*tablePtr;
}

void InitDeviceTable(const VkBaseLayerObject *obj)
{
	void *key = GetKey(obj->baseObject);
	
	VkLayerDispatchTable *table = NULL;

	{
		SCOPED_LOCK(devlock);
		RDCEraseEl(devlookup[key]);
		table = &devlookup[key];
	}

	// init the GetDeviceProcAddr function first
	table->GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)obj->pGPA((VkDevice)obj->nextObject, "vkGetDeviceProcAddr");
		
	// fetch the rest of the functions
	#undef HookInit
	#define HookInit(name) if(table->name == NULL) table->name = (CONCAT(PFN_vk, name))obj->pGPA((VkDevice)obj->baseObject, STRINGIZE(CONCAT(vk, name)))

	HookInitVulkanDevice();
}

void InitInstanceTable(const VkBaseLayerObject *obj)
{
	void *key = GetKey(obj->baseObject);
	
	VkLayerInstanceDispatchTable *table = NULL;

	{
		SCOPED_LOCK(instlock);
		RDCEraseEl(instlookup[key]);
		table = &instlookup[key];
	}

	// init the GetInstanceProcAddr function first
	table->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)obj->pGPA((VkInstance)obj->nextObject, "vkGetInstanceProcAddr");
		
	// fetch the rest of the functions
	#undef HookInit
	#define HookInit(name) if(table->name == NULL) table->name = (CONCAT(PFN_vk, name))obj->pGPA((VkInstance)obj->baseObject, STRINGIZE(CONCAT(vk, name)))

	HookInitVulkanInstance();
}

VkLayerDispatchTable *GetDeviceDispatchTable(void *device)
{
	if(replay) return &replayDeviceTable;

	void *key = GetKey(device);

	{
		SCOPED_LOCK(devlock);

		auto it = devlookup.find(key);

		if(it == devlookup.end())
			RDCFATAL("Bad device pointer");

		return &it->second;
	}
}

VkLayerInstanceDispatchTable *GetInstanceDispatchTable(void *instance)
{
	if(replay) return &replayInstanceTable;

	void *key = GetKey(instance);

	{
		SCOPED_LOCK(instlock);

		auto it = instlookup.find(key);

		if(it == instlookup.end())
			RDCFATAL("Bad device pointer");

		return &it->second;
	}
}
