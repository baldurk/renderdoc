//
// File: vk_wsi_swapchain.h
//
/*
** Copyright (c) 2014 The Khronos Group Inc.
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
*/

#ifndef __VK_WSI_SWAPCHAIN_H__
#define __VK_WSI_SWAPCHAIN_H__

#include "vulkan.h"

#define VK_WSI_SWAPCHAIN_REVISION             12
#define VK_WSI_SWAPCHAIN_EXTENSION_NUMBER     1
#define VK_WSI_SWAPCHAIN_EXTENSION_NAME       "VK_WSI_swapchain"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ------------------------------------------------------------------------------------------------
// Objects

// ------------------------------------------------------------------------------------------------
// Enumeration constants

#define VK_WSI_SWAPCHAIN_ENUM(type,id)    ((type)((int)0xc0000000 - VK_WSI_SWAPCHAIN_EXTENSION_NUMBER * -1024 + (id)))
#define VK_WSI_SWAPCHAIN_ENUM_POSITIVE(type,id)    ((type)((int)0x40000000 + (VK_WSI_SWAPCHAIN_EXTENSION_NUMBER - 1) * 1024 + (id)))

// Extend VkStructureType enum with extension specific constants
#define VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_WINDOW_WSI            VK_WSI_SWAPCHAIN_ENUM(VkStructureType, 0) 

// ------------------------------------------------------------------------------------------------
// Enumerations

typedef enum VkPlatformWSI_
{
    VK_PLATFORM_WIN32_WSI = 0,
    VK_PLATFORM_X11_WSI = 1,
    VK_PLATFORM_XCB_WSI = 2,
    VK_PLATFORM_ANDROID_WSI = 3,
    VK_PLATFORM_WAYLAND_WSI = 4,
    VK_PLATFORM_MIR_WSI = 5,
    VK_PLATFORM_BEGIN_RANGE_WSI = VK_PLATFORM_WIN32_WSI,
    VK_PLATFORM_END_RANGE_WSI = VK_PLATFORM_MIR_WSI,
    VK_PLATFORM_NUM_WSI = (VK_PLATFORM_MIR_WSI - VK_PLATFORM_WIN32_WSI + 1),
    VK_PLATFORM_MAX_ENUM_WSI = 0x7FFFFFFF
} VkPlatformWSI;

// ------------------------------------------------------------------------------------------------
// Flags

// ------------------------------------------------------------------------------------------------
// Structures

// pPlatformHandle points to this struct when platform is VK_PLATFORM_X11_WSI
#ifdef _X11_XLIB_H_
typedef struct VkPlatformHandleX11WSI_
{
    Display*                                 dpy;               // Display connection to an X server
    Window                                   root;              // To identify the X screen
} VkPlatformHandleX11WSI;
#endif /* _X11_XLIB_H_ */

// pPlatformHandle points to this struct when platform is VK_PLATFORM_XCB_WSI
#ifdef __XCB_H__
typedef struct VkPlatformHandleXcbWSI_
{
    xcb_connection_t*                        connection;        // XCB connection to an X server
    xcb_window_t                             root;              // To identify the X screen
} VkPlatformHandleXcbWSI;
#endif /* __XCB_H__ */

// Placeholder structure header for the different types of surface description structures
typedef struct VkSurfaceDescriptionWSI_
{
    VkStructureType                          sType;             // Can be any of the VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_XXX_WSI constants
    const void*                              pNext;             // Pointer to next structure
} VkSurfaceDescriptionWSI;

// Surface description structure for a native platform window surface
typedef struct VkSurfaceDescriptionWindowWSI_
{
    VkStructureType                         sType;              // Must be VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_WINDOW_WSI
    const void*                             pNext;              // Pointer to next structure
    VkPlatformWSI                           platform;           // e.g. VK_PLATFORM_*_WSI
    void*                                   pPlatformHandle;
    void*                                   pPlatformWindow;
} VkSurfaceDescriptionWindowWSI;

// ------------------------------------------------------------------------------------------------
// Function types

typedef VkResult (VKAPI *PFN_vkGetPhysicalDeviceSurfaceSupportWSI)(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, const VkSurfaceDescriptionWSI* pSurfaceDescription, VkBool32* pSupported);

// ------------------------------------------------------------------------------------------------
// Function prototypes

#ifdef VK_PROTOTYPES

VkResult VKAPI vkGetPhysicalDeviceSurfaceSupportWSI(
    VkPhysicalDevice                        physicalDevice,
    uint32_t                                queueFamilyIndex,
    const VkSurfaceDescriptionWSI*          pSurfaceDescription,
    VkBool32*                               pSupported);

#endif // VK_PROTOTYPES

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __VK_WSI_SWAPCHAIN_H__
