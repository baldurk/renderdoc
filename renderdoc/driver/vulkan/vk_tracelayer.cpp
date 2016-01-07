/*
 * Vulkan
 *
 * Copyright (C) 2014 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <vulkan/vk_layer.h>

// RenderDoc Includes

#include "vk_common.h"
#include "vk_core.h"
#include "vk_resources.h"
#include "vk_hookset_defs.h"

#include "common/common.h"
#include "common/threading.h"
#include "serialise/string_utils.h"

#include "data/version.h"

#include "os/os_specific.h"

// this should be in the vulkan definition header
#ifdef WIN32
#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#endif

void InitDeviceTable(const VkBaseLayerObject *obj);
void InitInstanceTable(const VkBaseLayerObject *obj);

// in vk_<platform>.cpp
bool LayerRegistered();

struct RegisterCallback
{
	RegisterCallback()
	{
		// we assume the implicit layer is registered - the UI will prompt the user about installing it.
		Process::RegisterEnvironmentModification(Process::EnvironmentModification(Process::eEnvModification_Replace, "ENABLE_VULKAN_RENDERDOC_CAPTURE", "1"));
	}
};

static RegisterCallback registercb;

// RenderDoc State

// RenderDoc Intercepts, these must all be entry points with a dispatchable object
// as the first parameter

#define HookDefine1(ret, function, t1, p1) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1) \
	{ return CoreDisp(p1)->function(p1); }
#define HookDefine2(ret, function, t1, p1, t2, p2) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2) \
	{ return CoreDisp(p1)->function(p1, p2); }
#define HookDefine3(ret, function, t1, p1, t2, p2, t3, p3) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3) \
	{ return CoreDisp(p1)->function(p1, p2, p3); }
#define HookDefine4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ return CoreDisp(p1)->function(p1, p2, p3, p4); }
#define HookDefine5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ return CoreDisp(p1)->function(p1, p2, p3, p4, p5); }
#define HookDefine6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6); }
#define HookDefine7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7); }
#define HookDefine8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
	ret VKAPI_CALL CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ return CoreDisp(p1)->function(p1, p2, p3, p4, p5, p6, p7, p8); }

DefineHooks();

// need to implement vkCreateInstance and vkDestroyInstance specially,
// to create and destroy the core WrappedVulkan object

VkResult hooked_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
	WrappedVulkan *core = new WrappedVulkan("");
	return core->vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

void hooked_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
	WrappedVulkan *core = CoreDisp(instance);
	core->vkDestroyInstance(instance, pAllocator);
	delete core;
}

// Layer Intercepts

static const VkLayerProperties physLayers[] = {
	{
		RENDERDOC_LAYER_NAME,
		VK_API_VERSION,
		VK_MAKE_VERSION(RENDERDOC_VERSION_MAJOR, RENDERDOC_VERSION_MINOR, 0),
		"Debugging capture layer for RenderDoc",
	}
};

static const VkExtensionProperties physExts[] = {
	{
		DEBUG_MARKER_EXTENSION_NAME,
		VK_MAKE_VERSION(0, 1, 0),
	}
};

static const VkLayerProperties globalLayers[] = {
	{
		RENDERDOC_LAYER_NAME,
		VK_API_VERSION,
		VK_MAKE_VERSION(RENDERDOC_VERSION_MAJOR, RENDERDOC_VERSION_MINOR, 0),
		"Debugging capture layer for RenderDoc",
	}
};

VkResult getProps(uint32_t *dstCount, void *dstProps, uint32_t srcCount, void *srcProps, size_t elemSize)
{
	if(dstCount == NULL)
		return VK_RESULT_MAX_ENUM;

	if(dstProps == NULL)
	{
		*dstCount = srcCount;
		return VK_SUCCESS;
	}

	memcpy(dstProps, srcProps, elemSize*RDCMIN(srcCount, *dstCount));
	if(*dstCount < srcCount)
		return VK_INCOMPLETE;

	*dstCount = srcCount;

	return VK_SUCCESS;
}

extern "C" {

VK_LAYER_EXPORT VkResult VKAPI_CALL RenderDocEnumerateDeviceLayerProperties(
	VkPhysicalDevice                            physicalDevice,
	uint32_t*                                   pPropertyCount,
	VkLayerProperties*                          pProperties)
{
	return getProps(pPropertyCount, pProperties, ARRAY_COUNT(physLayers), (void *)physLayers, sizeof(VkLayerProperties));
}

VK_LAYER_EXPORT VkResult VKAPI_CALL RenderDocEnumerateDeviceExtensionProperties(
	VkPhysicalDevice        physicalDevice,
	const char             *pLayerName,
	uint32_t               *pPropertyCount,
	VkExtensionProperties  *pProperties)
{
	// if pLayerName is NULL we're calling down through the layer chain to the ICD.
	// This is our chance to filter out any reported extensions that we don't support
	if(pLayerName == NULL)
		return ObjDisp(physicalDevice)->EnumerateDeviceExtensionProperties(Unwrap(physicalDevice), pLayerName, pPropertyCount, pProperties);

	return getProps(pPropertyCount, pProperties, ARRAY_COUNT(physExts), (void *)physExts, sizeof(VkExtensionProperties));
}

#undef HookInit
#define HookInit(function) if (!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) return (PFN_vkVoidFunction) &CONCAT(hooked_vk, function);

#undef HookInitExtension
#define HookInitExtension(ext, function) if (instDevInfo->ext && !strcmp(pName, STRINGIZE(CONCAT(vk, function)))) return (PFN_vkVoidFunction) &CONCAT(hooked_vk, function);

// proc addr routines

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL RenderDoc_GetDeviceProcAddr(VkDevice device, const char* pName)
{
	if (device == NULL)
		return NULL;

	/* loader uses this to force layer initialization; device object is wrapped */
	if (!strcmp("vkGetDeviceProcAddr", pName)) {
		InitDeviceTable((const VkBaseLayerObject *) device);
		return (PFN_vkVoidFunction) &RenderDoc_GetDeviceProcAddr;
	}

	if (!strcmp("vkCreateDevice", pName))
		return (PFN_vkVoidFunction) &hooked_vkCreateDevice;
	if (!strcmp("vkDestroyDevice", pName))
		return (PFN_vkVoidFunction) &hooked_vkDestroyDevice;

	HookInitVulkanDevice();

	InstanceDeviceInfo *instDevInfo = GetRecord(device)->instDevInfo;

	HookInitVulkanDeviceExts();

	if (GetDeviceDispatchTable(device)->GetDeviceProcAddr == NULL)
		return NULL;
	return GetDeviceDispatchTable(device)->GetDeviceProcAddr(Unwrap(device), pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI_CALL RenderDoc_GetInstanceProcAddr(VkInstance instance, const char* pName)
{
	if (instance == NULL)
		return NULL;

	/* loader uses this to force layer initialization; instance object is wrapped */
	if (!strcmp("vkGetInstanceProcAddr", pName)) {
		InitInstanceTable((const VkBaseLayerObject *) instance);
		return (PFN_vkVoidFunction) &RenderDoc_GetInstanceProcAddr;
	}

	if (!strcmp("vkEnumerateDeviceLayerProperties", pName))
		return (PFN_vkVoidFunction) &RenderDocEnumerateDeviceLayerProperties;
	if (!strcmp("vkEnumerateDeviceExtensionProperties", pName))
		return (PFN_vkVoidFunction) &RenderDocEnumerateDeviceExtensionProperties;

	HookInitVulkanInstance();

	InstanceDeviceInfo *instDevInfo = GetRecord(instance)->instDevInfo;

	// TEMPORARY HACK until loader is patched
	InstanceDeviceInfo dummy;
	dummy.VK_KHR_xlib_surface = true;
	dummy.VK_KHR_xcb_surface = true;
	dummy.VK_KHR_win32_surface = true;
	dummy.VK_KHR_surface = true;
	dummy.VK_KHR_swapchain = true;

	if(!WrappedVkInstance::IsAlloc(instance))
	{
		RDCLOG("Doing workaround for non-wrapped instance passed to GIPA");
		instDevInfo = &dummy;
	}

	HookInitVulkanInstanceExts();

	// GetInstanceProcAddr must also unconditionally return all device functions
	
#undef HookInitExtension
#define HookInitExtension(ext, function) if (!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) return (PFN_vkVoidFunction) &CONCAT(hooked_vk, function);

	HookInitVulkanDevice();

	HookInitVulkanDeviceExts();

	if (GetInstanceDispatchTable(instance)->GetInstanceProcAddr == NULL)
		return NULL;
	return GetInstanceDispatchTable(instance)->GetInstanceProcAddr(Unwrap(instance), pName);
}

}
