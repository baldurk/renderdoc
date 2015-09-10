//
// File: vk_debug_marker_lunarg.h
//
/*
** Copyright (c) 2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
**
** Authors:
**   Jon Ashburn <jon@lunarg.com>
**   Courtney Goeltzenleuchter <courtney@lunarg.com>
*/

#ifndef __VK_DEBUG_MARKER_H__
#define __VK_DEBUG_MARKER_H__

#include "vulkan.h"
#include "vk_debug_report_lunarg.h"

#define VK_DEBUG_MARKER_EXTENSION_NUMBER 3
#define VK_DEBUG_MARKER_EXTENSION_VERSION VK_MAKE_VERSION(0, 1, 0)
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*
***************************************************************************************************
*   DebugMarker Vulkan Extension API
***************************************************************************************************
*/

#define DEBUG_MARKER_EXTENSION_NAME "DEBUG_MARKER"

// ------------------------------------------------------------------------------------------------
// Enumerations

#define VK_DEBUG_MARKER_ENUM_EXTEND(type, id)    ((type)(VK_DEBUG_MARKER_EXTENSION_NUMBER * -1000 + (id)))

#define VK_OBJECT_INFO_TYPE_DBG_OBJECT_TAG VK_DEBUG_MARKER_ENUM_EXTEND(VkDbgObjectInfoType, 0)
#define VK_OBJECT_INFO_TYPE_DBG_OBJECT_NAME VK_DEBUG_MARKER_ENUM_EXTEND(VkDbgObjectInfoType, 1)

// ------------------------------------------------------------------------------------------------
// API functions

typedef void (VKAPI *PFN_vkCmdDbgMarkerBegin)(VkCmdBuffer cmdBuffer, const char* pMarker);
typedef void (VKAPI *PFN_vkCmdDbgMarkerEnd)(VkCmdBuffer cmdBuffer);
typedef VkResult (VKAPI *PFN_vkDbgSetObjectTag)(VkDevice device, VkDbgObjectType objType, uint64_t object, size_t tagSize, const void* pTag);
typedef VkResult (VKAPI *PFN_vkDbgSetObjectName)(VkDevice device, VkDbgObjectType objType, uint64_t object, size_t nameSize, const char* pName);

#ifdef VK_PROTOTYPES

// DebugMarker extension entrypoints
void VKAPI vkCmdDbgMarkerBegin(
    VkCmdBuffer                         cmdBuffer,
    const char*                         pMarker);

void VKAPI vkCmdDbgMarkerEnd(
    VkCmdBuffer                         cmdBuffer);

VkResult VKAPI vkDbgSetObjectTag(
    VkDevice                            device,
    VkDbgObjectType                     objType,
    uint64_t                            object,
    size_t                              tagSize,
    const void*                         pTag);

VkResult VKAPI vkDbgSetObjectName(
    VkDevice                            device,
    VkDbgObjectType                     objType,
    uint64_t                            object,
    size_t                              nameSize,
    const char*                         pName);

#endif // VK_PROTOTYPES

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __VK_DEBUG_MARKER_H__
