//
// File: vk_debug_report_lunarg.h
//
/*
 * Vulkan
 *
 * Copyright (C) 2015 LunarG, Inc.
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
 *
 * Authors:
 *   Jon Ashburn <jon@lunarg.com>
 *   Courtney Goeltzenleuchter <courtney@lunarg.com>
 */

#ifndef __VK_DEBUG_REPORT_LUNARG_H__
#define __VK_DEBUG_REPORT_LUNARG_H__

#include "vulkan.h"

#define VK_DEBUG_REPORT_EXTENSION_NUMBER 2
#define VK_DEBUG_REPORT_EXTENSION_VERSION VK_MAKE_VERSION(0, 1, 0)
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*
***************************************************************************************************
*   DebugReport Vulkan Extension API
***************************************************************************************************
*/
typedef enum {
    VK_OBJECT_TYPE_INSTANCE = 0,
    VK_OBJECT_TYPE_PHYSICAL_DEVICE = 1,
    VK_OBJECT_TYPE_DEVICE = 2,
    VK_OBJECT_TYPE_QUEUE = 3,
    VK_OBJECT_TYPE_COMMAND_BUFFER = 4,
    VK_OBJECT_TYPE_DEVICE_MEMORY = 5,
    VK_OBJECT_TYPE_BUFFER = 6,
    VK_OBJECT_TYPE_BUFFER_VIEW = 7,
    VK_OBJECT_TYPE_IMAGE = 8,
    VK_OBJECT_TYPE_IMAGE_VIEW = 9,
    VK_OBJECT_TYPE_ATTACHMENT_VIEW = 10,
    VK_OBJECT_TYPE_SHADER_MODULE = 12,
    VK_OBJECT_TYPE_SHADER = 13,
    VK_OBJECT_TYPE_PIPELINE = 14,
    VK_OBJECT_TYPE_PIPELINE_LAYOUT = 15,
    VK_OBJECT_TYPE_SAMPLER = 16,
    VK_OBJECT_TYPE_DESCRIPTOR_SET = 17,
    VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT = 18,
    VK_OBJECT_TYPE_DESCRIPTOR_POOL = 19,
    VK_OBJECT_TYPE_DYNAMIC_VIEWPORT_STATE = 20,
    VK_OBJECT_TYPE_DYNAMIC_RASTER_STATE = 21,
    VK_OBJECT_TYPE_DYNAMIC_COLOR_BLEND_STATE = 22,
    VK_OBJECT_TYPE_DYNAMIC_DEPTH_STENCIL_STATE = 23,
    VK_OBJECT_TYPE_FENCE = 24,
    VK_OBJECT_TYPE_SEMAPHORE = 25,
    VK_OBJECT_TYPE_EVENT = 26,
    VK_OBJECT_TYPE_QUERY_POOL = 27,
    VK_OBJECT_TYPE_FRAMEBUFFER = 28,
    VK_OBJECT_TYPE_RENDER_PASS = 29,
    VK_OBJECT_TYPE_PIPELINE_CACHE = 30,
    VK_OBJECT_TYPE_SWAP_CHAIN_WSI = 31,
    VK_OBJECT_TYPE_CMD_POOL = 32,
    VK_OBJECT_TYPE_BEGIN_RANGE = VK_OBJECT_TYPE_INSTANCE,
    VK_OBJECT_TYPE_END_RANGE = VK_OBJECT_TYPE_CMD_POOL,
    VK_OBJECT_TYPE_NUM = (VK_OBJECT_TYPE_CMD_POOL - VK_OBJECT_TYPE_INSTANCE + 1),
    VK_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkDbgObjectType;

static inline const char* string_VkDbgObjectType(VkDbgObjectType input_value)
{
    switch ((VkDbgObjectType)input_value)
    {
        case VK_OBJECT_TYPE_CMD_POOL:
            return "VK_OBJECT_TYPE_CMD_POOL";
        case VK_OBJECT_TYPE_BUFFER:
            return "VK_OBJECT_TYPE_BUFFER";
        case VK_OBJECT_TYPE_BUFFER_VIEW:
            return "VK_OBJECT_TYPE_BUFFER_VIEW";
        case VK_OBJECT_TYPE_ATTACHMENT_VIEW:
            return "VK_OBJECT_TYPE_ATTACHMENT_VIEW";
        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            return "VK_OBJECT_TYPE_COMMAND_BUFFER";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            return "VK_OBJECT_TYPE_DESCRIPTOR_POOL";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            return "VK_OBJECT_TYPE_DESCRIPTOR_SET";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT";
        case VK_OBJECT_TYPE_DEVICE:
            return "VK_OBJECT_TYPE_DEVICE";
        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            return "VK_OBJECT_TYPE_DEVICE_MEMORY";
        case VK_OBJECT_TYPE_DYNAMIC_COLOR_BLEND_STATE:
            return "VK_OBJECT_TYPE_DYNAMIC_COLOR_BLEND_STATE";
        case VK_OBJECT_TYPE_DYNAMIC_DEPTH_STENCIL_STATE:
            return "VK_OBJECT_TYPE_DYNAMIC_DEPTH_STENCIL_STATE";
        case VK_OBJECT_TYPE_DYNAMIC_RASTER_STATE:
            return "VK_OBJECT_TYPE_DYNAMIC_RASTER_STATE";
        case VK_OBJECT_TYPE_DYNAMIC_VIEWPORT_STATE:
            return "VK_OBJECT_TYPE_DYNAMIC_VIEPORT_STATE";
        case VK_OBJECT_TYPE_EVENT:
            return "VK_OBJECT_TYPE_EVENT";
        case VK_OBJECT_TYPE_FENCE:
            return "VK_OBJECT_TYPE_FENCE";
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            return "VK_OBJECT_TYPE_FRAMEBUFFER";
        case VK_OBJECT_TYPE_IMAGE:
            return "VK_OBJECT_TYPE_IMAGE";
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            return "VK_OBJECT_TYPE_IMAGE_VIEW";
        case VK_OBJECT_TYPE_INSTANCE:
            return "VK_OBJECT_TYPE_INSTANCE";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            return "VK_OBJECT_TYPE_PHYSICAL_DEVICE";
        case VK_OBJECT_TYPE_PIPELINE:
            return "VK_OBJECT_TYPE_PIPELINE";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            return "VK_OBJECT_TYPE_PIPELINE_LAYOUT";
        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            return "VK_OBJECT_TYPE_PIPELINE_CACHE";
        case VK_OBJECT_TYPE_QUERY_POOL:
            return "VK_OBJECT_TYPE_QUERY_POOL";
        case VK_OBJECT_TYPE_QUEUE:
            return "VK_OBJECT_TYPE_QUEUE";
        case VK_OBJECT_TYPE_RENDER_PASS:
            return "VK_OBJECT_TYPE_RENDER_PASS";
        case VK_OBJECT_TYPE_SAMPLER:
            return "VK_OBJECT_TYPE_SAMPLER";
        case VK_OBJECT_TYPE_SEMAPHORE:
            return "VK_OBJECT_TYPE_SEMAPHORE";
        case VK_OBJECT_TYPE_SHADER:
            return "VK_OBJECT_TYPE_SHADER";
        case VK_OBJECT_TYPE_SHADER_MODULE:
            return "VK_OBJECT_TYPE_SHADER_MODULE";
        case VK_OBJECT_TYPE_SWAP_CHAIN_WSI:
            return "VK_OBJECT_TYPE_SWAP_CHAIN_WSI";
        default:
            return "Unhandled VkObjectType";
    }
}
#define VK_DEBUG_REPORT_EXTENSION_NAME "DEBUG_REPORT"

VK_DEFINE_NONDISP_HANDLE(VkDbgMsgCallback)

// ------------------------------------------------------------------------------------------------
// Enumerations

typedef enum VkDbgReportFlags_
{
    VK_DBG_REPORT_INFO_BIT       = 0x0001,
    VK_DBG_REPORT_WARN_BIT       = 0x0002,
    VK_DBG_REPORT_PERF_WARN_BIT  = 0x0004,
    VK_DBG_REPORT_ERROR_BIT      = 0x0008,
    VK_DBG_REPORT_DEBUG_BIT      = 0x0010,
} VkDbgReportFlags;

// Debug Report ERROR codes
typedef enum _DEBUG_REPORT_ERROR
{
    DEBUG_REPORT_NONE,                  // Used for INFO & other non-error messages
    DEBUG_REPORT_CALLBACK_REF,          // Callbacks were not destroyed prior to calling DestroyInstance
} DEBUG_REPORT_ERROR;

#define VK_DEBUG_REPORT_ENUM_EXTEND(type, id)    ((type)(VK_DEBUG_REPORT_EXTENSION_NUMBER * -1000 + (id)))

#define VK_OBJECT_TYPE_MSG_CALLBACK VK_DEBUG_REPORT_ENUM_EXTEND(VkDbgObjectType, 0)
// ------------------------------------------------------------------------------------------------
// Vulkan function pointers

typedef void (*PFN_vkDbgMsgCallback)(
    VkFlags                             msgFlags,
    VkDbgObjectType                     objType,
    uint64_t                            srcObject,
    size_t                              location,
    int32_t                             msgCode,
    const char*                         pLayerPrefix,
    const char*                         pMsg,
    void*                               pUserData);

// ------------------------------------------------------------------------------------------------
// API functions

typedef VkResult (VKAPI *PFN_vkDbgCreateMsgCallback)(VkInstance instance, VkFlags msgFlags, const PFN_vkDbgMsgCallback pfnMsgCallback, const void* pUserData, VkDbgMsgCallback* pMsgCallback);
typedef VkResult (VKAPI *PFN_vkDbgDestroyMsgCallback)(VkInstance instance, VkDbgMsgCallback msgCallback);

#ifdef VK_PROTOTYPES

// DebugReport extension entrypoints
VkResult VKAPI vkDbgCreateMsgCallback(
    VkInstance                          instance,
    VkFlags                             msgFlags,
    const PFN_vkDbgMsgCallback          pfnMsgCallback,
    void*                               pUserData,
    VkDbgMsgCallback*                   pMsgCallback);

VkResult VKAPI vkDbgDestroyMsgCallback(
    VkInstance                          instance,
    VkDbgMsgCallback                    msgCallback);

// DebugReport utility callback functions
void VKAPI vkDbgStringCallback(
    VkFlags                             msgFlags,
    VkDbgObjectType                     objType,
    uint64_t                            srcObject,
    size_t                              location,
    int32_t                             msgCode,
    const char*                         pLayerPrefix,
    const char*                         pMsg,
    void*                               pUserData);

void VKAPI vkDbgStdioCallback(
    VkFlags                             msgFlags,
    VkDbgObjectType                     objType,
    uint64_t                            srcObject,
    size_t                              location,
    int32_t                             msgCode,
    const char*                         pLayerPrefix,
    const char*                         pMsg,
    void*                               pUserData);

void VKAPI vkDbgBreakCallback(
    VkFlags                             msgFlags,
    VkDbgObjectType                     objType,
    uint64_t                            srcObject,
    size_t                              location,
    int32_t                             msgCode,
    const char*                         pLayerPrefix,
    const char*                         pMsg,
    void*                               pUserData);

#endif // VK_PROTOTYPES

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __VK_DEBUG_REPORT_LUNARG_H__
