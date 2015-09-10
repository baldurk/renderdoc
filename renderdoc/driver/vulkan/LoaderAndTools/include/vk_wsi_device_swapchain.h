//
// File: vk_wsi_device_swapchain.h
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

#ifndef __VK_WSI_DEVICE_SWAPCHAIN_H__
#define __VK_WSI_DEVICE_SWAPCHAIN_H__

#include "vulkan.h"

#define VK_WSI_DEVICE_SWAPCHAIN_REVISION            40
#define VK_WSI_DEVICE_SWAPCHAIN_EXTENSION_NUMBER    2
#define VK_WSI_DEVICE_SWAPCHAIN_EXTENSION_NAME      "VK_WSI_device_swapchain"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// ------------------------------------------------------------------------------------------------
// Objects

VK_DEFINE_NONDISP_HANDLE(VkSwapChainWSI);

// ------------------------------------------------------------------------------------------------
// Enumeration constants

#define VK_WSI_DEVICE_SWAPCHAIN_ENUM(type,id)    ((type)((int)0xc0000000 - VK_WSI_DEVICE_SWAPCHAIN_EXTENSION_NUMBER * -1024 + (id)))
#define VK_WSI_DEVICE_SWAPCHAIN_ENUM_POSITIVE(type,id)    ((type)((int)0x40000000 + (VK_WSI_DEVICE_SWAPCHAIN_EXTENSION_NUMBER - 1) * 1024 + (id)))

// Extend VkStructureType enum with extension specific constants
#define VK_STRUCTURE_TYPE_SWAP_CHAIN_CREATE_INFO_WSI VK_WSI_DEVICE_SWAPCHAIN_ENUM(VkStructureType, 0)
#define VK_STRUCTURE_TYPE_QUEUE_PRESENT_INFO_WSI VK_WSI_DEVICE_SWAPCHAIN_ENUM(VkStructureType, 1)

// Extend VkImageLayout enum with extension specific constants
#define VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI VK_WSI_DEVICE_SWAPCHAIN_ENUM(VkImageLayout, 2)

// Extend VkResult enum with extension specific constants
//  Return codes for successful operation execution
#define VK_SUBOPTIMAL_WSI           VK_WSI_DEVICE_SWAPCHAIN_ENUM_POSITIVE(VkResult, 3)
//  Error codes
#define VK_ERROR_OUT_OF_DATE_WSI    VK_WSI_DEVICE_SWAPCHAIN_ENUM(VkResult, 4)

// ------------------------------------------------------------------------------------------------
// Enumerations

typedef enum VkSurfaceTransformWSI_
{
    VK_SURFACE_TRANSFORM_NONE_WSI = 0,
    VK_SURFACE_TRANSFORM_ROT90_WSI = 1,
    VK_SURFACE_TRANSFORM_ROT180_WSI = 2,
    VK_SURFACE_TRANSFORM_ROT270_WSI = 3,
    VK_SURFACE_TRANSFORM_HMIRROR_WSI = 4,
    VK_SURFACE_TRANSFORM_HMIRROR_ROT90_WSI = 5,
    VK_SURFACE_TRANSFORM_HMIRROR_ROT180_WSI = 6,
    VK_SURFACE_TRANSFORM_HMIRROR_ROT270_WSI = 7,
    VK_SURFACE_TRANSFORM_INHERIT_WSI = 8,
} VkSurfaceTransformWSI;

typedef enum VkSurfaceTransformFlagBitsWSI_
{
    VK_SURFACE_TRANSFORM_NONE_BIT_WSI = 0x00000001,
    VK_SURFACE_TRANSFORM_ROT90_BIT_WSI = 0x00000002,
    VK_SURFACE_TRANSFORM_ROT180_BIT_WSI = 0x00000004,
    VK_SURFACE_TRANSFORM_ROT270_BIT_WSI = 0x00000008,
    VK_SURFACE_TRANSFORM_HMIRROR_BIT_WSI = 0x00000010,
    VK_SURFACE_TRANSFORM_HMIRROR_ROT90_BIT_WSI = 0x00000020,
    VK_SURFACE_TRANSFORM_HMIRROR_ROT180_BIT_WSI = 0x00000040,
    VK_SURFACE_TRANSFORM_HMIRROR_ROT270_BIT_WSI = 0x00000080,
    VK_SURFACE_TRANSFORM_INHERIT_BIT_WSI = 0x00000100,
} VkSurfaceTransformFlagBitsWSI;
typedef VkFlags VkSurfaceTransformFlagsWSI;

typedef enum VkSurfaceInfoTypeWSI_
{
    VK_SURFACE_INFO_TYPE_PROPERTIES_WSI = 0,
    VK_SURFACE_INFO_TYPE_FORMATS_WSI = 1,
    VK_SURFACE_INFO_TYPE_PRESENT_MODES_WSI = 2,
    VK_SURFACE_INFO_TYPE_BEGIN_RANGE_WSI = VK_SURFACE_INFO_TYPE_PROPERTIES_WSI,
    VK_SURFACE_INFO_TYPE_END_RANGE_WSI = VK_SURFACE_INFO_TYPE_PRESENT_MODES_WSI,
    VK_SURFACE_INFO_TYPE_NUM_WSI = (VK_SURFACE_INFO_TYPE_PRESENT_MODES_WSI - VK_SURFACE_INFO_TYPE_PROPERTIES_WSI + 1),
    VK_SURFACE_INFO_TYPE_MAX_ENUM_WSI = 0x7FFFFFFF
} VkSurfaceInfoTypeWSI;

typedef enum VkSwapChainInfoTypeWSI_
{
    VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI = 0,
    VK_SWAP_CHAIN_INFO_TYPE_BEGIN_RANGE_WSI = VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI,
    VK_SWAP_CHAIN_INFO_TYPE_END_RANGE_WSI = VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI,
    VK_SWAP_CHAIN_INFO_TYPE_NUM_WSI = (VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI - VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI + 1),
    VK_SWAP_CHAIN_INFO_TYPE_MAX_ENUM_WSI = 0x7FFFFFFF
} VkSwapChainInfoTypeWSI;

typedef enum VkPresentModeWSI_
{
    VK_PRESENT_MODE_IMMEDIATE_WSI = 0,
    VK_PRESENT_MODE_MAILBOX_WSI = 1,
    VK_PRESENT_MODE_FIFO_WSI = 2,
    VK_PRESENT_MODE_BEGIN_RANGE_WSI = VK_PRESENT_MODE_IMMEDIATE_WSI,
    VK_PRESENT_MODE_END_RANGE_WSI = VK_PRESENT_MODE_FIFO_WSI,
    VK_PRESENT_MODE_NUM = (VK_PRESENT_MODE_FIFO_WSI - VK_PRESENT_MODE_IMMEDIATE_WSI + 1),
    VK_PRESENT_MODE_MAX_ENUM_WSI = 0x7FFFFFFF
} VkPresentModeWSI;

// ------------------------------------------------------------------------------------------------
// Flags

// ------------------------------------------------------------------------------------------------
// Structures

typedef struct VkSurfacePropertiesWSI_
{
    uint32_t                                minImageCount;      // Supported minimum number of images for the surface
    uint32_t                                maxImageCount;      // Supported maximum number of images for the surface, 0 for unlimited

    VkExtent2D                              currentExtent;      // Current image width and height for the surface, (-1, -1) if undefined.
    VkExtent2D                              minImageExtent;     // Supported minimum image width and height for the surface
    VkExtent2D                              maxImageExtent;     // Supported maximum image width and height for the surface

    VkSurfaceTransformFlagsWSI              supportedTransforms;// 1 or more bits representing the transforms supported
    VkSurfaceTransformWSI                   currentTransform;   // The surface's current transform relative to the device's natural orientation.

    uint32_t                                maxImageArraySize;  // Supported maximum number of image layers for the surface

    VkImageUsageFlags                       supportedUsageFlags;// Supported image usage flags for the surface
} VkSurfacePropertiesWSI;

typedef struct VkSurfaceFormatPropertiesWSI_
{
    VkFormat                                format;             // Supported rendering format for the surface
} VkSurfaceFormatPropertiesWSI;

typedef struct VkSurfacePresentModePropertiesWSI_
{
    VkPresentModeWSI                        presentMode;        // Supported presention mode for the surface
} VkSurfacePresentModePropertiesWSI;

typedef struct VkSwapChainCreateInfoWSI_
{
    VkStructureType                          sType;             // Must be VK_STRUCTURE_TYPE_SWAP_CHAIN_CREATE_INFO_WSI
    const void*                              pNext;             // Pointer to next structure

    const VkSurfaceDescriptionWSI*           pSurfaceDescription;// describes the swap chain's target surface

    uint32_t                                 minImageCount;     // Minimum number of presentation images the application needs
    VkFormat                                 imageFormat;       // Format of the presentation images
    VkExtent2D                               imageExtent;       // Dimensions of the presentation images
    VkImageUsageFlags                        imageUsageFlags;   // Bits indicating how the presentation images will be used
    VkSurfaceTransformWSI                    preTransform;      // The transform, relative to the device's natural orientation, applied to the image content prior to presentation
    uint32_t                                 imageArraySize;    // Determines the number of views for multiview/stereo presentation

    VkPresentModeWSI                         presentMode;       // Which presentation mode to use for presents on this swap chain.

    VkSwapChainWSI                           oldSwapChain;      // Existing swap chain to replace, if any.

    VkBool32                                 clipped;           // Specifies whether presentable images may be affected by window clip regions.
} VkSwapChainCreateInfoWSI;

typedef struct VkSwapChainImagePropertiesWSI_
{
    VkImage                                  image;             // Persistent swap chain image handle
} VkSwapChainImagePropertiesWSI;

typedef struct VkPresentInfoWSI_
{
    VkStructureType                          sType;             // Must be VK_STRUCTURE_TYPE_QUEUE_PRESENT_INFO_WSI
    const void*                              pNext;             // Pointer to next structure
    uint32_t                                 swapChainCount;    // Number of swap chains to present in this call
    const VkSwapChainWSI*                    swapChains;        // Swap chains to present an image from.
    const uint32_t*                          imageIndices;      // Indices of which swapChain images to present
} VkPresentInfoWSI;

// ------------------------------------------------------------------------------------------------
// Function types

typedef VkResult (VKAPI *PFN_vkGetSurfaceInfoWSI)(VkDevice device, const VkSurfaceDescriptionWSI* pSurfaceDescription, VkSurfaceInfoTypeWSI infoType, size_t* pDataSize, void* pData);
typedef VkResult (VKAPI *PFN_vkCreateSwapChainWSI)(VkDevice device, const VkSwapChainCreateInfoWSI* pCreateInfo, VkSwapChainWSI* pSwapChain);
typedef VkResult (VKAPI *PFN_vkDestroySwapChainWSI)(VkDevice device, VkSwapChainWSI swapChain);
typedef VkResult (VKAPI *PFN_vkGetSwapChainInfoWSI)(VkDevice device, VkSwapChainWSI swapChain, VkSwapChainInfoTypeWSI infoType, size_t* pDataSize, void* pData);
typedef VkResult (VKAPI *PFN_vkAcquireNextImageWSI)(VkDevice device, VkSwapChainWSI swapChain, uint64_t timeout, VkSemaphore semaphore, uint32_t* pImageIndex);
typedef VkResult (VKAPI *PFN_vkQueuePresentWSI)(VkQueue queue, VkPresentInfoWSI* pPresentInfo);

// ------------------------------------------------------------------------------------------------
// Function prototypes

#ifdef VK_PROTOTYPES

VkResult VKAPI vkGetSurfaceInfoWSI(
    VkDevice                                 device,
    const VkSurfaceDescriptionWSI*           pSurfaceDescription,
    VkSurfaceInfoTypeWSI                     infoType,
    size_t*                                  pDataSize,
    void*                                    pData);

VkResult VKAPI vkCreateSwapChainWSI(
    VkDevice                                 device,
    const VkSwapChainCreateInfoWSI*          pCreateInfo,
    VkSwapChainWSI*                          pSwapChain);

VkResult VKAPI vkDestroySwapChainWSI(
    VkDevice                                 device,
    VkSwapChainWSI                           swapChain);

VkResult VKAPI vkGetSwapChainInfoWSI(
    VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    VkSwapChainInfoTypeWSI                   infoType,
    size_t*                                  pDataSize,
    void*                                    pData);

VkResult VKAPI vkAcquireNextImageWSI(
    VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    uint64_t                                 timeout,
    VkSemaphore                              semaphore,
    uint32_t*                                pImageIndex);

VkResult VKAPI vkQueuePresentWSI(
    VkQueue                                  queue,
    VkPresentInfoWSI*                        pPresentInfo);

#endif // VK_PROTOTYPES

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // __VK_WSI_SWAPCHAIN_H__
