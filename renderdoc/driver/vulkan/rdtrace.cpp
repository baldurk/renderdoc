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
#include <assert.h>
#include "vk_loader_platform.h"
#include "vk_dispatch_table_helper.h"
#include "vk_layer.h"
#include "vk_layer_table.h"
#include "vk_layer_extension_utils.h"
// The following is #included again to catch certain OS-specific functions
// being used:
#include "vk_loader_platform.h"

// Renderdoc Includes

#include "driver/vulkan/vk_common.h"
#include "driver/vulkan/vk_core.h"
#include "driver/vulkan/vk_hookset_defs.h"

#include "common/threading.h"
#include "serialise/string_utils.h"

// don't want these definitions, the only place we'll use these is as parameter/variable names
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

// Renderdoc State

WrappedVulkan *shadowVulkan = NULL;

// Layer Intercepts

static const VkLayerProperties rdt_physicaldevice_layers[] = {
    {
        "Renderdoc",
        VK_API_VERSION,
        VK_MAKE_VERSION(0, 1, 0),
        "layer: implements Renderdoc tracing",
    }
};

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceLayerProperties(
        VkPhysicalDevice                            physicalDevice,
        uint32_t*                                   pCount,
        VkLayerProperties*                          pProperties)
{
    return util_GetLayerProperties(ARRAY_SIZE(rdt_physicaldevice_layers), rdt_physicaldevice_layers,
                                   pCount, pProperties);
}

static const VkExtensionProperties rdt_physicaldevice_extensions[] = {
    {
        DEBUG_MARKER_EXTENSION_NAME,
        VK_MAKE_VERSION(0, 1, 0),
    }
};

VK_LAYER_EXPORT VkResult VKAPI vkGetPhysicalDeviceExtensionProperties(
        VkPhysicalDevice        physicalDevice,
        const char             *pLayerName,
        uint32_t               *pCount,
        VkExtensionProperties  *pProperties)
{
    return util_GetExtensionProperties(ARRAY_SIZE(rdt_physicaldevice_extensions), rdt_physicaldevice_extensions,
                                       pCount, pProperties);
}

static const VkLayerProperties rdt_GlobalLayers[] = {
    {
        "Renderdoc",
        VK_API_VERSION,
        VK_MAKE_VERSION(0, 1, 0),
        "Trace layer: Renderdoc",
    }
};

VK_LAYER_EXPORT VkResult VKAPI vkGetGlobalLayerProperties(
        uint32_t *pCount,
        VkLayerProperties*    pProperties)
{
    return util_GetLayerProperties(ARRAY_SIZE(rdt_GlobalLayers),
                                   rdt_GlobalLayers,
                                   pCount, pProperties);
}

// Renderdoc Intercepts

#define HookDefine0(ret, function) \
        VK_LAYER_EXPORT \
        ret VKAPI function() \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(); }
#define HookDefine1(ret, function, t1, p1) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1); }
#define HookDefine2(ret, function, t1, p1, t2, p2) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2); }
#define HookDefine3(ret, function, t1, p1, t2, p2, t3, p3) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3); }
#define HookDefine4(ret, function, t1, p1, t2, p2, t3, p3, t4, p4) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4); }
#define HookDefine5(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5); }
#define HookDefine6(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6); }
#define HookDefine7(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7); }
#define HookDefine8(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8); }
#define HookDefine9(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9); }
#define HookDefine10(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); }
#define HookDefine11(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); }
#define HookDefine12(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); }
#define HookDefine13(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); }
#define HookDefine14(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14); }
#define HookDefine15(ret, function, t1, p1, t2, p2, t3, p3, t4, p4, t5, p5, t6, p6, t7, p7, t8, p8, t9, p9, t10, p10, t11, p11, t12, p12, t13, p13, t14, p14, t15, p15) \
        VK_LAYER_EXPORT \
        ret VKAPI function(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9, t10 p10, t11 p11, t12 p12, t13 p13, t14 p14, t15 p15) \
        { SCOPED_LOCK(vkLock); return shadowVulkan->function(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); }

Threading::CriticalSection vkLock;

DefineHooks();

#undef HookInit
#define HookInit(function) if (!strcmp(pName, STRINGIZE(function))) return (PFN_vkVoidFunction) function;

// proc addr routines

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    if (device == NULL)
        return NULL;

    /* loader uses this to force layer initialization; device object is wrapped */
    if (!strcmp("vkGetDeviceProcAddr", pName)) {
        initDeviceTable((const VkBaseLayerObject *) device);
        return (PFN_vkVoidFunction) vkGetDeviceProcAddr;
    }

    HookInitVulkanDevice();

    if (!strcmp("vkCreateDevice", pName))
        return (PFN_vkVoidFunction) vkCreateDevice;
    if (!strcmp("vkDestroyDevice", pName))
        return (PFN_vkVoidFunction) vkDestroyDevice;
    else
    {
        if (device_dispatch_table(device)->GetDeviceProcAddr == NULL)
            return NULL;
        return device_dispatch_table(device)->GetDeviceProcAddr(device, pName);
    }
}

VK_LAYER_EXPORT PFN_vkVoidFunction VKAPI vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    if (instance == NULL)
        return NULL;

    /* loader uses this to force layer initialization; instance object is wrapped */
    if (!strcmp("vkGetInstanceProcAddr", pName)) {
        initInstanceTable((const VkBaseLayerObject *) instance);
        if (shadowVulkan == NULL) {
            shadowVulkan = new WrappedVulkan("");
        }
        return (PFN_vkVoidFunction) vkGetInstanceProcAddr;
    }

    HookInitVulkanInstance();

    if (!strcmp("vkGetPhysicalDeviceLayerProperties", pName))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceLayerProperties;
    if (!strcmp("vkGetPhysicalDeviceExtensionProperties", pName))
        return (PFN_vkVoidFunction) vkGetPhysicalDeviceExtensionProperties;
    if (!strcmp("vkGetGlobalLayerProperties", pName))
        return (PFN_vkVoidFunction) vkGetGlobalLayerProperties;

    if (instance_dispatch_table(instance)->GetInstanceProcAddr == NULL)
        return NULL;
    return instance_dispatch_table(instance)->GetInstanceProcAddr(instance, pName);
}

