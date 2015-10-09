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
#include "vk_hookset_defs.h"

#include "common/common.h"
#include "common/threading.h"

#include "data/version.h"

// this should be in the vulkan definition header
#ifdef WIN32
#undef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)
#endif

void InitDeviceTable(const VkBaseLayerObject *obj);
void InitInstanceTable(const VkBaseLayerObject *obj);

// RenderDoc State

WrappedVulkan *shadowVulkan = NULL;

// RenderDoc Intercepts

#define HookDefine0(ret, function) \
	ret VKAPI CONCAT(hooked_, function)() \
	{ return shadowVulkan->function(); }
#define HookDefine1(ret, function, t1, p1) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1) \
	{ return shadowVulkan->function(p1); }
#define HookDefine2(ret, function, t1, p1, t2, p2) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2) \
	{ return shadowVulkan->function(p1, p2); }
#define HookDefine3(ret, function, t1, p1, t2, p2, t3, p3) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3) \
	{ return shadowVulkan->function(p1, p2, p3); }
#define HookDefine4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4) \
	{ return shadowVulkan->function(p1, p2, p3, p4); }
#define HookDefine5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
	{ return shadowVulkan->function(p1, p2, p3, p4, p5); }
#define HookDefine6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
	{ return shadowVulkan->function(p1, p2, p3, p4, p5, p6); }
#define HookDefine7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
	{ return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7); }
#define HookDefine8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
	ret VKAPI CONCAT(hooked_, function)(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
	{ return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8); }

DefineHooks();

// Layer Intercepts

static const VkLayerProperties physLayers[] = {
	{
		"RenderDoc",
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
		"RenderDoc",
			VK_API_VERSION,
			VK_MAKE_VERSION(RENDERDOC_VERSION_MAJOR, RENDERDOC_VERSION_MINOR, 0),
			"Debugging capture layer for RenderDoc",
	}
};

VkResult getProps(uint32_t *dstCount, void *dstProps, uint32_t srcCount, void *srcProps, size_t elemSize)
{
	if(dstCount == NULL)
		return VK_ERROR_INVALID_POINTER;

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

VK_LAYER_EXPORT VkResult VKAPI RenderDocGetPhysicalDeviceLayerProperties(
	VkPhysicalDevice                            physicalDevice,
	uint32_t*                                   pCount,
	VkLayerProperties*                          pProperties)
{
	return getProps(pCount, pProperties, ARRAY_COUNT(physLayers), (void *)physLayers, sizeof(VkLayerProperties));
}

VK_LAYER_EXPORT VkResult VKAPI RenderDocGetPhysicalDeviceExtensionProperties(
	VkPhysicalDevice        physicalDevice,
	const char             *pLayerName,
	uint32_t               *pCount,
	VkExtensionProperties  *pProperties)
{
	return getProps(pCount, pProperties, ARRAY_COUNT(physExts), (void *)physExts, sizeof(VkExtensionProperties));
}

// VKTODOLOW this can't be intercepted? no dispatchable object
VK_LAYER_EXPORT VkResult VKAPI vkGetGlobalLayerProperties(
	uint32_t *pCount,
	VkLayerProperties*    pProperties)
{
	return getProps(pCount, pProperties, ARRAY_COUNT(globalLayers), (void *)globalLayers, sizeof(VkLayerProperties));
}

#undef HookInit
#define HookInit(function) if (!strcmp(pName, STRINGIZE(CONCAT(vk, function)))) return (PFN_vkVoidFunction) &CONCAT(hooked_vk, function);

// proc addr routines

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI RenderDocGetDeviceProcAddr(VkDevice device, const char* pName)
{
	if (device == NULL)
		return NULL;

	/* loader uses this to force layer initialization; device object is wrapped */
	if (!strcmp("vkGetDeviceProcAddr", pName)) {
		InitDeviceTable((const VkBaseLayerObject *) device);
		return (PFN_vkVoidFunction) &RenderDocGetDeviceProcAddr;
	}

	if (!strcmp("vkCreateDevice", pName))
		return (PFN_vkVoidFunction) &hooked_vkCreateDevice;
	if (!strcmp("vkDestroyDevice", pName))
		return (PFN_vkVoidFunction) &hooked_vkDestroyDevice;

	HookInitVulkanDevice();

	if (GetDeviceDispatchTable(device)->GetDeviceProcAddr == NULL)
		return NULL;
	return GetDeviceDispatchTable(device)->GetDeviceProcAddr(device, pName);
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI RenderDocGetInstanceProcAddr(VkInstance instance, const char* pName)
{
	if (instance == NULL)
		return NULL;

	/* loader uses this to force layer initialization; instance object is wrapped */
	if (!strcmp("vkGetInstanceProcAddr", pName)) {
		InitInstanceTable((const VkBaseLayerObject *) instance);
		// VKTODOLOW I think this will be created and passed down in wrapped dispatchable
		// objects
		if (shadowVulkan == NULL) {
			shadowVulkan = new WrappedVulkan("");
		}
		return (PFN_vkVoidFunction) &RenderDocGetInstanceProcAddr;
	}

	if (!strcmp("vkGetPhysicalDeviceLayerProperties", pName))
		return (PFN_vkVoidFunction) &RenderDocGetPhysicalDeviceLayerProperties;
	if (!strcmp("vkGetPhysicalDeviceExtensionProperties", pName))
		return (PFN_vkVoidFunction) &RenderDocGetPhysicalDeviceExtensionProperties;
	if (!strcmp("vkGetGlobalLayerProperties", pName))
		return (PFN_vkVoidFunction) &vkGetGlobalLayerProperties;

	HookInitVulkanInstance();

	if (GetInstanceDispatchTable(instance)->GetInstanceProcAddr == NULL)
		return NULL;
	return GetInstanceDispatchTable(instance)->GetInstanceProcAddr(instance, pName);
}

}
