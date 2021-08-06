/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Baldur Karlsson
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

/******************************************************************************
 * Generated from Khronos's vk.xml:
 *
 * Copyright 2015-2021 The Khronos Group Inc.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 * *
 * This file, vk.xml, is the Vulkan API Registry. It is a critically important
 * and normative part of the Vulkan Specification, including a canonical
 * machine-readable definition of the API, parameter and member validation
 * language incorporated into the Specification and reference pages, and other
 * material which is registered by Khronos, such as tags used by extension and
 * layer authors. The authoritative public version of vk.xml is maintained in
 * the default branch (currently named main) of the Khronos Vulkan GitHub
 * project. The authoritative private version is maintained in the default
 * branch of the member gitlab server.
 *******************************************************************************/

// This file is autogenerated with gen_dispatch_table.py - any changes will be overwritten next time
// that script is run.
// $ ./gen_spirv_code.py

#pragma once

#include "official/vulkan.h"

// this file is autogenerated, so don't worry about clang-format issues
// clang-format off

struct VkInstDispatchTable
{
  // Vulkan core API interface definitions
  PFN_vkCreateInstance CreateInstance;
  PFN_vkDestroyInstance DestroyInstance;
  PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
  PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
  PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
  PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties;
  PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
  PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
  PFN_vkCreateDevice CreateDevice;
  PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
  PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
  PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
  PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties;

  // Vulkan 1.1 core API interface definitions.
  PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion;
  PFN_vkEnumeratePhysicalDeviceGroups EnumeratePhysicalDeviceGroups;
  PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2;
  PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2;
  PFN_vkGetPhysicalDeviceFormatProperties2 GetPhysicalDeviceFormatProperties2;
  PFN_vkGetPhysicalDeviceImageFormatProperties2 GetPhysicalDeviceImageFormatProperties2;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties2 GetPhysicalDeviceQueueFamilyProperties2;
  PFN_vkGetPhysicalDeviceMemoryProperties2 GetPhysicalDeviceMemoryProperties2;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 GetPhysicalDeviceSparseImageFormatProperties2;
  PFN_vkGetPhysicalDeviceExternalBufferProperties GetPhysicalDeviceExternalBufferProperties;
  PFN_vkGetPhysicalDeviceExternalFenceProperties GetPhysicalDeviceExternalFenceProperties;
  PFN_vkGetPhysicalDeviceExternalSemaphoreProperties GetPhysicalDeviceExternalSemaphoreProperties;

  // VK_KHR_surface
  PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;

  // VK_KHR_swapchain
  PFN_vkGetPhysicalDevicePresentRectanglesKHR GetPhysicalDevicePresentRectanglesKHR;

  // VK_KHR_display
  PFN_vkGetPhysicalDeviceDisplayPropertiesKHR GetPhysicalDeviceDisplayPropertiesKHR;
  PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR GetPhysicalDeviceDisplayPlanePropertiesKHR;
  PFN_vkGetDisplayPlaneSupportedDisplaysKHR GetDisplayPlaneSupportedDisplaysKHR;
  PFN_vkGetDisplayModePropertiesKHR GetDisplayModePropertiesKHR;
  PFN_vkCreateDisplayModeKHR CreateDisplayModeKHR;
  PFN_vkGetDisplayPlaneCapabilitiesKHR GetDisplayPlaneCapabilitiesKHR;
  PFN_vkCreateDisplayPlaneSurfaceKHR CreateDisplayPlaneSurfaceKHR;

  // VK_KHR_xlib_surface
#ifdef VK_USE_PLATFORM_XLIB_KHR
  PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR;
  PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR GetPhysicalDeviceXlibPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR

  // VK_KHR_xcb_surface
#ifdef VK_USE_PLATFORM_XCB_KHR
  PFN_vkCreateXcbSurfaceKHR CreateXcbSurfaceKHR;
  PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR GetPhysicalDeviceXcbPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XCB_KHR

  // VK_KHR_wayland_surface
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
  PFN_vkCreateWaylandSurfaceKHR CreateWaylandSurfaceKHR;
  PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR GetPhysicalDeviceWaylandPresentationSupportKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR

  // VK_KHR_android_surface
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  PFN_vkCreateAndroidSurfaceKHR CreateAndroidSurfaceKHR;
#endif // VK_USE_PLATFORM_ANDROID_KHR

  // VK_KHR_win32_surface
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
  PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR GetPhysicalDeviceWin32PresentationSupportKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_KHR_get_physical_device_properties2
  PFN_vkGetPhysicalDeviceFeatures2KHR GetPhysicalDeviceFeatures2KHR;
  PFN_vkGetPhysicalDeviceProperties2KHR GetPhysicalDeviceProperties2KHR;
  PFN_vkGetPhysicalDeviceFormatProperties2KHR GetPhysicalDeviceFormatProperties2KHR;
  PFN_vkGetPhysicalDeviceImageFormatProperties2KHR GetPhysicalDeviceImageFormatProperties2KHR;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR GetPhysicalDeviceQueueFamilyProperties2KHR;
  PFN_vkGetPhysicalDeviceMemoryProperties2KHR GetPhysicalDeviceMemoryProperties2KHR;
  PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR GetPhysicalDeviceSparseImageFormatProperties2KHR;

  // VK_KHR_device_group_creation
  PFN_vkEnumeratePhysicalDeviceGroupsKHR EnumeratePhysicalDeviceGroupsKHR;

  // VK_KHR_external_memory_capabilities
  PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR GetPhysicalDeviceExternalBufferPropertiesKHR;

  // VK_KHR_external_semaphore_capabilities
  PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR GetPhysicalDeviceExternalSemaphorePropertiesKHR;

  // VK_KHR_external_fence_capabilities
  PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR GetPhysicalDeviceExternalFencePropertiesKHR;

  // VK_KHR_performance_query
  PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
  PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR GetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;

  // VK_KHR_get_surface_capabilities2
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR GetPhysicalDeviceSurfaceCapabilities2KHR;
  PFN_vkGetPhysicalDeviceSurfaceFormats2KHR GetPhysicalDeviceSurfaceFormats2KHR;

  // VK_KHR_get_display_properties2
  PFN_vkGetPhysicalDeviceDisplayProperties2KHR GetPhysicalDeviceDisplayProperties2KHR;
  PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR GetPhysicalDeviceDisplayPlaneProperties2KHR;
  PFN_vkGetDisplayModeProperties2KHR GetDisplayModeProperties2KHR;
  PFN_vkGetDisplayPlaneCapabilities2KHR GetDisplayPlaneCapabilities2KHR;

  // VK_KHR_fragment_shading_rate
  PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR GetPhysicalDeviceFragmentShadingRatesKHR;

  // VK_EXT_debug_report
  PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallbackEXT;
  PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallbackEXT;
  PFN_vkDebugReportMessageEXT DebugReportMessageEXT;

  // VK_GGP_stream_descriptor_surface
#ifdef VK_USE_PLATFORM_GGP
  PFN_vkCreateStreamDescriptorSurfaceGGP CreateStreamDescriptorSurfaceGGP;
#endif // VK_USE_PLATFORM_GGP

  // VK_NV_external_memory_capabilities
  PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV GetPhysicalDeviceExternalImageFormatPropertiesNV;

  // VK_NN_vi_surface
#ifdef VK_USE_PLATFORM_VI_NN
  PFN_vkCreateViSurfaceNN CreateViSurfaceNN;
#endif // VK_USE_PLATFORM_VI_NN

  // VK_EXT_direct_mode_display
  PFN_vkReleaseDisplayEXT ReleaseDisplayEXT;

  // VK_EXT_acquire_xlib_display
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
  PFN_vkAcquireXlibDisplayEXT AcquireXlibDisplayEXT;
  PFN_vkGetRandROutputDisplayEXT GetRandROutputDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT

  // VK_EXT_display_surface_counter
  PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT GetPhysicalDeviceSurfaceCapabilities2EXT;

  // VK_MVK_ios_surface
#ifdef VK_USE_PLATFORM_IOS_MVK
  PFN_vkCreateIOSSurfaceMVK CreateIOSSurfaceMVK;
#endif // VK_USE_PLATFORM_IOS_MVK

  // VK_MVK_macos_surface
#ifdef VK_USE_PLATFORM_MACOS_MVK
  PFN_vkCreateMacOSSurfaceMVK CreateMacOSSurfaceMVK;
#endif // VK_USE_PLATFORM_MACOS_MVK

  // VK_EXT_debug_utils
  PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
  PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
  PFN_vkSubmitDebugUtilsMessageEXT SubmitDebugUtilsMessageEXT;

  // VK_EXT_sample_locations
  PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT GetPhysicalDeviceMultisamplePropertiesEXT;

  // VK_EXT_calibrated_timestamps
  PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT GetPhysicalDeviceCalibrateableTimeDomainsEXT;

  // VK_FUCHSIA_imagepipe_surface
#ifdef VK_USE_PLATFORM_FUCHSIA
  PFN_vkCreateImagePipeSurfaceFUCHSIA CreateImagePipeSurfaceFUCHSIA;
#endif // VK_USE_PLATFORM_FUCHSIA

  // VK_EXT_metal_surface
#ifdef VK_USE_PLATFORM_METAL_EXT
  PFN_vkCreateMetalSurfaceEXT CreateMetalSurfaceEXT;
#endif // VK_USE_PLATFORM_METAL_EXT

  // VK_EXT_tooling_info
  PFN_vkGetPhysicalDeviceToolPropertiesEXT GetPhysicalDeviceToolPropertiesEXT;

  // VK_NV_cooperative_matrix
  PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV GetPhysicalDeviceCooperativeMatrixPropertiesNV;

  // VK_NV_coverage_reduction_mode
  PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV GetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;

  // VK_EXT_full_screen_exclusive
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT GetPhysicalDeviceSurfacePresentModes2EXT;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_EXT_headless_surface
  PFN_vkCreateHeadlessSurfaceEXT CreateHeadlessSurfaceEXT;

  // VK_NV_acquire_winrt_display
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkAcquireWinrtDisplayNV AcquireWinrtDisplayNV;
  PFN_vkGetWinrtDisplayNV GetWinrtDisplayNV;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_EXT_directfb_surface
#ifdef VK_USE_PLATFORM_DIRECTFB_EXT
  PFN_vkCreateDirectFBSurfaceEXT CreateDirectFBSurfaceEXT;
  PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT GetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif // VK_USE_PLATFORM_DIRECTFB_EXT
};

struct VkDevDispatchTable
{
  // Vulkan core API interface definitions
  PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
  PFN_vkDestroyDevice DestroyDevice;
  PFN_vkGetDeviceQueue GetDeviceQueue;
  PFN_vkQueueSubmit QueueSubmit;
  PFN_vkQueueWaitIdle QueueWaitIdle;
  PFN_vkDeviceWaitIdle DeviceWaitIdle;
  PFN_vkAllocateMemory AllocateMemory;
  PFN_vkFreeMemory FreeMemory;
  PFN_vkMapMemory MapMemory;
  PFN_vkUnmapMemory UnmapMemory;
  PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
  PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
  PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
  PFN_vkBindBufferMemory BindBufferMemory;
  PFN_vkBindImageMemory BindImageMemory;
  PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
  PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
  PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
  PFN_vkQueueBindSparse QueueBindSparse;
  PFN_vkCreateFence CreateFence;
  PFN_vkDestroyFence DestroyFence;
  PFN_vkResetFences ResetFences;
  PFN_vkGetFenceStatus GetFenceStatus;
  PFN_vkWaitForFences WaitForFences;
  PFN_vkCreateSemaphore CreateSemaphore;
  PFN_vkDestroySemaphore DestroySemaphore;
  PFN_vkCreateEvent CreateEvent;
  PFN_vkDestroyEvent DestroyEvent;
  PFN_vkGetEventStatus GetEventStatus;
  PFN_vkSetEvent SetEvent;
  PFN_vkResetEvent ResetEvent;
  PFN_vkCreateQueryPool CreateQueryPool;
  PFN_vkDestroyQueryPool DestroyQueryPool;
  PFN_vkGetQueryPoolResults GetQueryPoolResults;
  PFN_vkCreateBuffer CreateBuffer;
  PFN_vkDestroyBuffer DestroyBuffer;
  PFN_vkCreateBufferView CreateBufferView;
  PFN_vkDestroyBufferView DestroyBufferView;
  PFN_vkCreateImage CreateImage;
  PFN_vkDestroyImage DestroyImage;
  PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;
  PFN_vkCreateImageView CreateImageView;
  PFN_vkDestroyImageView DestroyImageView;
  PFN_vkCreateShaderModule CreateShaderModule;
  PFN_vkDestroyShaderModule DestroyShaderModule;
  PFN_vkCreatePipelineCache CreatePipelineCache;
  PFN_vkDestroyPipelineCache DestroyPipelineCache;
  PFN_vkGetPipelineCacheData GetPipelineCacheData;
  PFN_vkMergePipelineCaches MergePipelineCaches;
  PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
  PFN_vkCreateComputePipelines CreateComputePipelines;
  PFN_vkDestroyPipeline DestroyPipeline;
  PFN_vkCreatePipelineLayout CreatePipelineLayout;
  PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
  PFN_vkCreateSampler CreateSampler;
  PFN_vkDestroySampler DestroySampler;
  PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
  PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
  PFN_vkCreateDescriptorPool CreateDescriptorPool;
  PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
  PFN_vkResetDescriptorPool ResetDescriptorPool;
  PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
  PFN_vkFreeDescriptorSets FreeDescriptorSets;
  PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
  PFN_vkCreateFramebuffer CreateFramebuffer;
  PFN_vkDestroyFramebuffer DestroyFramebuffer;
  PFN_vkCreateRenderPass CreateRenderPass;
  PFN_vkDestroyRenderPass DestroyRenderPass;
  PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
  PFN_vkCreateCommandPool CreateCommandPool;
  PFN_vkDestroyCommandPool DestroyCommandPool;
  PFN_vkResetCommandPool ResetCommandPool;
  PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
  PFN_vkFreeCommandBuffers FreeCommandBuffers;
  PFN_vkBeginCommandBuffer BeginCommandBuffer;
  PFN_vkEndCommandBuffer EndCommandBuffer;
  PFN_vkResetCommandBuffer ResetCommandBuffer;
  PFN_vkCmdBindPipeline CmdBindPipeline;
  PFN_vkCmdSetViewport CmdSetViewport;
  PFN_vkCmdSetScissor CmdSetScissor;
  PFN_vkCmdSetLineWidth CmdSetLineWidth;
  PFN_vkCmdSetDepthBias CmdSetDepthBias;
  PFN_vkCmdSetBlendConstants CmdSetBlendConstants;
  PFN_vkCmdSetDepthBounds CmdSetDepthBounds;
  PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask;
  PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask;
  PFN_vkCmdSetStencilReference CmdSetStencilReference;
  PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
  PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
  PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
  PFN_vkCmdDraw CmdDraw;
  PFN_vkCmdDrawIndexed CmdDrawIndexed;
  PFN_vkCmdDrawIndirect CmdDrawIndirect;
  PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect;
  PFN_vkCmdDispatch CmdDispatch;
  PFN_vkCmdDispatchIndirect CmdDispatchIndirect;
  PFN_vkCmdCopyBuffer CmdCopyBuffer;
  PFN_vkCmdCopyImage CmdCopyImage;
  PFN_vkCmdBlitImage CmdBlitImage;
  PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
  PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
  PFN_vkCmdUpdateBuffer CmdUpdateBuffer;
  PFN_vkCmdFillBuffer CmdFillBuffer;
  PFN_vkCmdClearColorImage CmdClearColorImage;
  PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage;
  PFN_vkCmdClearAttachments CmdClearAttachments;
  PFN_vkCmdResolveImage CmdResolveImage;
  PFN_vkCmdSetEvent CmdSetEvent;
  PFN_vkCmdResetEvent CmdResetEvent;
  PFN_vkCmdWaitEvents CmdWaitEvents;
  PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
  PFN_vkCmdBeginQuery CmdBeginQuery;
  PFN_vkCmdEndQuery CmdEndQuery;
  PFN_vkCmdResetQueryPool CmdResetQueryPool;
  PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
  PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults;
  PFN_vkCmdPushConstants CmdPushConstants;
  PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
  PFN_vkCmdNextSubpass CmdNextSubpass;
  PFN_vkCmdEndRenderPass CmdEndRenderPass;
  PFN_vkCmdExecuteCommands CmdExecuteCommands;

  // Vulkan 1.1 core API interface definitions.
  PFN_vkBindBufferMemory2 BindBufferMemory2;
  PFN_vkBindImageMemory2 BindImageMemory2;
  PFN_vkGetDeviceGroupPeerMemoryFeatures GetDeviceGroupPeerMemoryFeatures;
  PFN_vkCmdSetDeviceMask CmdSetDeviceMask;
  PFN_vkCmdDispatchBase CmdDispatchBase;
  PFN_vkGetImageMemoryRequirements2 GetImageMemoryRequirements2;
  PFN_vkGetBufferMemoryRequirements2 GetBufferMemoryRequirements2;
  PFN_vkGetImageSparseMemoryRequirements2 GetImageSparseMemoryRequirements2;
  PFN_vkTrimCommandPool TrimCommandPool;
  PFN_vkGetDeviceQueue2 GetDeviceQueue2;
  PFN_vkCreateSamplerYcbcrConversion CreateSamplerYcbcrConversion;
  PFN_vkDestroySamplerYcbcrConversion DestroySamplerYcbcrConversion;
  PFN_vkCreateDescriptorUpdateTemplate CreateDescriptorUpdateTemplate;
  PFN_vkDestroyDescriptorUpdateTemplate DestroyDescriptorUpdateTemplate;
  PFN_vkUpdateDescriptorSetWithTemplate UpdateDescriptorSetWithTemplate;
  PFN_vkGetDescriptorSetLayoutSupport GetDescriptorSetLayoutSupport;

  // Vulkan 1.2 core API interface definitions.
  PFN_vkCmdDrawIndirectCount CmdDrawIndirectCount;
  PFN_vkCmdDrawIndexedIndirectCount CmdDrawIndexedIndirectCount;
  PFN_vkCreateRenderPass2 CreateRenderPass2;
  PFN_vkCmdBeginRenderPass2 CmdBeginRenderPass2;
  PFN_vkCmdNextSubpass2 CmdNextSubpass2;
  PFN_vkCmdEndRenderPass2 CmdEndRenderPass2;
  PFN_vkResetQueryPool ResetQueryPool;
  PFN_vkGetSemaphoreCounterValue GetSemaphoreCounterValue;
  PFN_vkWaitSemaphores WaitSemaphores;
  PFN_vkSignalSemaphore SignalSemaphore;
  PFN_vkGetBufferDeviceAddress GetBufferDeviceAddress;
  PFN_vkGetBufferOpaqueCaptureAddress GetBufferOpaqueCaptureAddress;
  PFN_vkGetDeviceMemoryOpaqueCaptureAddress GetDeviceMemoryOpaqueCaptureAddress;

  // VK_KHR_swapchain
  PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
  PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
  PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
  PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
  PFN_vkQueuePresentKHR QueuePresentKHR;
  PFN_vkGetDeviceGroupPresentCapabilitiesKHR GetDeviceGroupPresentCapabilitiesKHR;
  PFN_vkGetDeviceGroupSurfacePresentModesKHR GetDeviceGroupSurfacePresentModesKHR;
  PFN_vkAcquireNextImage2KHR AcquireNextImage2KHR;

  // VK_KHR_display_swapchain
  PFN_vkCreateSharedSwapchainsKHR CreateSharedSwapchainsKHR;

  // VK_KHR_device_group
  PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR GetDeviceGroupPeerMemoryFeaturesKHR;
  PFN_vkCmdSetDeviceMaskKHR CmdSetDeviceMaskKHR;
  PFN_vkCmdDispatchBaseKHR CmdDispatchBaseKHR;

  // VK_KHR_maintenance1
  PFN_vkTrimCommandPoolKHR TrimCommandPoolKHR;

  // VK_KHR_external_memory_win32
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkGetMemoryWin32HandleKHR GetMemoryWin32HandleKHR;
  PFN_vkGetMemoryWin32HandlePropertiesKHR GetMemoryWin32HandlePropertiesKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_KHR_external_memory_fd
  PFN_vkGetMemoryFdKHR GetMemoryFdKHR;
  PFN_vkGetMemoryFdPropertiesKHR GetMemoryFdPropertiesKHR;

  // VK_KHR_external_semaphore_win32
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkImportSemaphoreWin32HandleKHR ImportSemaphoreWin32HandleKHR;
  PFN_vkGetSemaphoreWin32HandleKHR GetSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_KHR_external_semaphore_fd
  PFN_vkImportSemaphoreFdKHR ImportSemaphoreFdKHR;
  PFN_vkGetSemaphoreFdKHR GetSemaphoreFdKHR;

  // VK_KHR_push_descriptor
  PFN_vkCmdPushDescriptorSetKHR CmdPushDescriptorSetKHR;
  PFN_vkCmdPushDescriptorSetWithTemplateKHR CmdPushDescriptorSetWithTemplateKHR;

  // VK_KHR_descriptor_update_template
  PFN_vkCreateDescriptorUpdateTemplateKHR CreateDescriptorUpdateTemplateKHR;
  PFN_vkDestroyDescriptorUpdateTemplateKHR DestroyDescriptorUpdateTemplateKHR;
  PFN_vkUpdateDescriptorSetWithTemplateKHR UpdateDescriptorSetWithTemplateKHR;

  // VK_KHR_create_renderpass2
  PFN_vkCreateRenderPass2KHR CreateRenderPass2KHR;
  PFN_vkCmdBeginRenderPass2KHR CmdBeginRenderPass2KHR;
  PFN_vkCmdNextSubpass2KHR CmdNextSubpass2KHR;
  PFN_vkCmdEndRenderPass2KHR CmdEndRenderPass2KHR;

  // VK_KHR_shared_presentable_image
  PFN_vkGetSwapchainStatusKHR GetSwapchainStatusKHR;

  // VK_KHR_external_fence_win32
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkImportFenceWin32HandleKHR ImportFenceWin32HandleKHR;
  PFN_vkGetFenceWin32HandleKHR GetFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_KHR_external_fence_fd
  PFN_vkImportFenceFdKHR ImportFenceFdKHR;
  PFN_vkGetFenceFdKHR GetFenceFdKHR;

  // VK_KHR_performance_query
  PFN_vkAcquireProfilingLockKHR AcquireProfilingLockKHR;
  PFN_vkReleaseProfilingLockKHR ReleaseProfilingLockKHR;

  // VK_KHR_get_memory_requirements2
  PFN_vkGetImageMemoryRequirements2KHR GetImageMemoryRequirements2KHR;
  PFN_vkGetBufferMemoryRequirements2KHR GetBufferMemoryRequirements2KHR;
  PFN_vkGetImageSparseMemoryRequirements2KHR GetImageSparseMemoryRequirements2KHR;

  // VK_KHR_acceleration_structure
  PFN_vkCreateAccelerationStructureKHR CreateAccelerationStructureKHR;
  PFN_vkDestroyAccelerationStructureKHR DestroyAccelerationStructureKHR;
  PFN_vkCmdBuildAccelerationStructuresKHR CmdBuildAccelerationStructuresKHR;
  PFN_vkCmdBuildAccelerationStructuresIndirectKHR CmdBuildAccelerationStructuresIndirectKHR;
  PFN_vkBuildAccelerationStructuresKHR BuildAccelerationStructuresKHR;
  PFN_vkCopyAccelerationStructureKHR CopyAccelerationStructureKHR;
  PFN_vkCopyAccelerationStructureToMemoryKHR CopyAccelerationStructureToMemoryKHR;
  PFN_vkCopyMemoryToAccelerationStructureKHR CopyMemoryToAccelerationStructureKHR;
  PFN_vkWriteAccelerationStructuresPropertiesKHR WriteAccelerationStructuresPropertiesKHR;
  PFN_vkCmdCopyAccelerationStructureKHR CmdCopyAccelerationStructureKHR;
  PFN_vkCmdCopyAccelerationStructureToMemoryKHR CmdCopyAccelerationStructureToMemoryKHR;
  PFN_vkCmdCopyMemoryToAccelerationStructureKHR CmdCopyMemoryToAccelerationStructureKHR;
  PFN_vkGetAccelerationStructureDeviceAddressKHR GetAccelerationStructureDeviceAddressKHR;
  PFN_vkCmdWriteAccelerationStructuresPropertiesKHR CmdWriteAccelerationStructuresPropertiesKHR;
  PFN_vkGetDeviceAccelerationStructureCompatibilityKHR GetDeviceAccelerationStructureCompatibilityKHR;
  PFN_vkGetAccelerationStructureBuildSizesKHR GetAccelerationStructureBuildSizesKHR;

  // VK_KHR_sampler_ycbcr_conversion
  PFN_vkCreateSamplerYcbcrConversionKHR CreateSamplerYcbcrConversionKHR;
  PFN_vkDestroySamplerYcbcrConversionKHR DestroySamplerYcbcrConversionKHR;

  // VK_KHR_bind_memory2
  PFN_vkBindBufferMemory2KHR BindBufferMemory2KHR;
  PFN_vkBindImageMemory2KHR BindImageMemory2KHR;

  // VK_KHR_maintenance3
  PFN_vkGetDescriptorSetLayoutSupportKHR GetDescriptorSetLayoutSupportKHR;

  // VK_KHR_draw_indirect_count
  PFN_vkCmdDrawIndirectCountKHR CmdDrawIndirectCountKHR;
  PFN_vkCmdDrawIndexedIndirectCountKHR CmdDrawIndexedIndirectCountKHR;

  // VK_KHR_timeline_semaphore
  PFN_vkGetSemaphoreCounterValueKHR GetSemaphoreCounterValueKHR;
  PFN_vkWaitSemaphoresKHR WaitSemaphoresKHR;
  PFN_vkSignalSemaphoreKHR SignalSemaphoreKHR;

  // VK_KHR_fragment_shading_rate
  PFN_vkCmdSetFragmentShadingRateKHR CmdSetFragmentShadingRateKHR;

  // VK_KHR_buffer_device_address
  PFN_vkGetBufferDeviceAddressKHR GetBufferDeviceAddressKHR;
  PFN_vkGetBufferOpaqueCaptureAddressKHR GetBufferOpaqueCaptureAddressKHR;
  PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR GetDeviceMemoryOpaqueCaptureAddressKHR;

  // VK_KHR_deferred_host_operations
  PFN_vkCreateDeferredOperationKHR CreateDeferredOperationKHR;
  PFN_vkDestroyDeferredOperationKHR DestroyDeferredOperationKHR;
  PFN_vkGetDeferredOperationMaxConcurrencyKHR GetDeferredOperationMaxConcurrencyKHR;
  PFN_vkGetDeferredOperationResultKHR GetDeferredOperationResultKHR;
  PFN_vkDeferredOperationJoinKHR DeferredOperationJoinKHR;

  // VK_KHR_pipeline_executable_properties
  PFN_vkGetPipelineExecutablePropertiesKHR GetPipelineExecutablePropertiesKHR;
  PFN_vkGetPipelineExecutableStatisticsKHR GetPipelineExecutableStatisticsKHR;
  PFN_vkGetPipelineExecutableInternalRepresentationsKHR GetPipelineExecutableInternalRepresentationsKHR;

  // VK_KHR_synchronization2
  PFN_vkCmdSetEvent2KHR CmdSetEvent2KHR;
  PFN_vkCmdResetEvent2KHR CmdResetEvent2KHR;
  PFN_vkCmdWaitEvents2KHR CmdWaitEvents2KHR;
  PFN_vkCmdPipelineBarrier2KHR CmdPipelineBarrier2KHR;
  PFN_vkCmdWriteTimestamp2KHR CmdWriteTimestamp2KHR;
  PFN_vkQueueSubmit2KHR QueueSubmit2KHR;
  PFN_vkCmdWriteBufferMarker2AMD CmdWriteBufferMarker2AMD;
  PFN_vkGetQueueCheckpointData2NV GetQueueCheckpointData2NV;

  // VK_KHR_copy_commands2
  PFN_vkCmdCopyBuffer2KHR CmdCopyBuffer2KHR;
  PFN_vkCmdCopyImage2KHR CmdCopyImage2KHR;
  PFN_vkCmdCopyBufferToImage2KHR CmdCopyBufferToImage2KHR;
  PFN_vkCmdCopyImageToBuffer2KHR CmdCopyImageToBuffer2KHR;
  PFN_vkCmdBlitImage2KHR CmdBlitImage2KHR;
  PFN_vkCmdResolveImage2KHR CmdResolveImage2KHR;

  // VK_KHR_ray_tracing_pipeline
  PFN_vkCmdTraceRaysKHR CmdTraceRaysKHR;
  PFN_vkCreateRayTracingPipelinesKHR CreateRayTracingPipelinesKHR;
  PFN_vkGetRayTracingShaderGroupHandlesKHR GetRayTracingShaderGroupHandlesKHR;
  PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR GetRayTracingCaptureReplayShaderGroupHandlesKHR;
  PFN_vkCmdTraceRaysIndirectKHR CmdTraceRaysIndirectKHR;
  PFN_vkGetRayTracingShaderGroupStackSizeKHR GetRayTracingShaderGroupStackSizeKHR;
  PFN_vkCmdSetRayTracingPipelineStackSizeKHR CmdSetRayTracingPipelineStackSizeKHR;

  // VK_EXT_debug_marker
  PFN_vkDebugMarkerSetObjectTagEXT DebugMarkerSetObjectTagEXT;
  PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT;
  PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT;
  PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT;

  // VK_EXT_transform_feedback
  PFN_vkCmdBindTransformFeedbackBuffersEXT CmdBindTransformFeedbackBuffersEXT;
  PFN_vkCmdBeginTransformFeedbackEXT CmdBeginTransformFeedbackEXT;
  PFN_vkCmdEndTransformFeedbackEXT CmdEndTransformFeedbackEXT;
  PFN_vkCmdBeginQueryIndexedEXT CmdBeginQueryIndexedEXT;
  PFN_vkCmdEndQueryIndexedEXT CmdEndQueryIndexedEXT;
  PFN_vkCmdDrawIndirectByteCountEXT CmdDrawIndirectByteCountEXT;

  // VK_NVX_image_view_handle
  PFN_vkGetImageViewHandleNVX GetImageViewHandleNVX;
  PFN_vkGetImageViewAddressNVX GetImageViewAddressNVX;

  // VK_AMD_draw_indirect_count
  PFN_vkCmdDrawIndirectCountAMD CmdDrawIndirectCountAMD;
  PFN_vkCmdDrawIndexedIndirectCountAMD CmdDrawIndexedIndirectCountAMD;

  // VK_AMD_shader_info
  PFN_vkGetShaderInfoAMD GetShaderInfoAMD;

  // VK_NV_external_memory_win32
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkGetMemoryWin32HandleNV GetMemoryWin32HandleNV;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_EXT_conditional_rendering
  PFN_vkCmdBeginConditionalRenderingEXT CmdBeginConditionalRenderingEXT;
  PFN_vkCmdEndConditionalRenderingEXT CmdEndConditionalRenderingEXT;

  // VK_NV_clip_space_w_scaling
  PFN_vkCmdSetViewportWScalingNV CmdSetViewportWScalingNV;

  // VK_EXT_display_control
  PFN_vkDisplayPowerControlEXT DisplayPowerControlEXT;
  PFN_vkRegisterDeviceEventEXT RegisterDeviceEventEXT;
  PFN_vkRegisterDisplayEventEXT RegisterDisplayEventEXT;
  PFN_vkGetSwapchainCounterEXT GetSwapchainCounterEXT;

  // VK_GOOGLE_display_timing
  PFN_vkGetRefreshCycleDurationGOOGLE GetRefreshCycleDurationGOOGLE;
  PFN_vkGetPastPresentationTimingGOOGLE GetPastPresentationTimingGOOGLE;

  // VK_EXT_discard_rectangles
  PFN_vkCmdSetDiscardRectangleEXT CmdSetDiscardRectangleEXT;

  // VK_EXT_hdr_metadata
  PFN_vkSetHdrMetadataEXT SetHdrMetadataEXT;

  // VK_EXT_debug_utils
  PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
  PFN_vkSetDebugUtilsObjectTagEXT SetDebugUtilsObjectTagEXT;
  PFN_vkQueueBeginDebugUtilsLabelEXT QueueBeginDebugUtilsLabelEXT;
  PFN_vkQueueEndDebugUtilsLabelEXT QueueEndDebugUtilsLabelEXT;
  PFN_vkQueueInsertDebugUtilsLabelEXT QueueInsertDebugUtilsLabelEXT;
  PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
  PFN_vkCmdInsertDebugUtilsLabelEXT CmdInsertDebugUtilsLabelEXT;

  // VK_ANDROID_external_memory_android_hardware_buffer
#ifdef VK_USE_PLATFORM_ANDROID_KHR
  PFN_vkGetAndroidHardwareBufferPropertiesANDROID GetAndroidHardwareBufferPropertiesANDROID;
  PFN_vkGetMemoryAndroidHardwareBufferANDROID GetMemoryAndroidHardwareBufferANDROID;
#endif // VK_USE_PLATFORM_ANDROID_KHR

  // VK_EXT_sample_locations
  PFN_vkCmdSetSampleLocationsEXT CmdSetSampleLocationsEXT;

  // VK_EXT_image_drm_format_modifier
  PFN_vkGetImageDrmFormatModifierPropertiesEXT GetImageDrmFormatModifierPropertiesEXT;

  // VK_EXT_validation_cache
  PFN_vkCreateValidationCacheEXT CreateValidationCacheEXT;
  PFN_vkDestroyValidationCacheEXT DestroyValidationCacheEXT;
  PFN_vkMergeValidationCachesEXT MergeValidationCachesEXT;
  PFN_vkGetValidationCacheDataEXT GetValidationCacheDataEXT;

  // VK_NV_shading_rate_image
  PFN_vkCmdBindShadingRateImageNV CmdBindShadingRateImageNV;
  PFN_vkCmdSetViewportShadingRatePaletteNV CmdSetViewportShadingRatePaletteNV;
  PFN_vkCmdSetCoarseSampleOrderNV CmdSetCoarseSampleOrderNV;

  // VK_NV_ray_tracing
  PFN_vkCreateAccelerationStructureNV CreateAccelerationStructureNV;
  PFN_vkDestroyAccelerationStructureNV DestroyAccelerationStructureNV;
  PFN_vkGetAccelerationStructureMemoryRequirementsNV GetAccelerationStructureMemoryRequirementsNV;
  PFN_vkBindAccelerationStructureMemoryNV BindAccelerationStructureMemoryNV;
  PFN_vkCmdBuildAccelerationStructureNV CmdBuildAccelerationStructureNV;
  PFN_vkCmdCopyAccelerationStructureNV CmdCopyAccelerationStructureNV;
  PFN_vkCmdTraceRaysNV CmdTraceRaysNV;
  PFN_vkCreateRayTracingPipelinesNV CreateRayTracingPipelinesNV;
  PFN_vkGetRayTracingShaderGroupHandlesNV GetRayTracingShaderGroupHandlesNV;
  PFN_vkGetAccelerationStructureHandleNV GetAccelerationStructureHandleNV;
  PFN_vkCmdWriteAccelerationStructuresPropertiesNV CmdWriteAccelerationStructuresPropertiesNV;
  PFN_vkCompileDeferredNV CompileDeferredNV;

  // VK_EXT_external_memory_host
  PFN_vkGetMemoryHostPointerPropertiesEXT GetMemoryHostPointerPropertiesEXT;

  // VK_AMD_buffer_marker
  PFN_vkCmdWriteBufferMarkerAMD CmdWriteBufferMarkerAMD;

  // VK_EXT_calibrated_timestamps
  PFN_vkGetCalibratedTimestampsEXT GetCalibratedTimestampsEXT;

  // VK_NV_mesh_shader
  PFN_vkCmdDrawMeshTasksNV CmdDrawMeshTasksNV;
  PFN_vkCmdDrawMeshTasksIndirectNV CmdDrawMeshTasksIndirectNV;
  PFN_vkCmdDrawMeshTasksIndirectCountNV CmdDrawMeshTasksIndirectCountNV;

  // VK_NV_scissor_exclusive
  PFN_vkCmdSetExclusiveScissorNV CmdSetExclusiveScissorNV;

  // VK_NV_device_diagnostic_checkpoints
  PFN_vkCmdSetCheckpointNV CmdSetCheckpointNV;
  PFN_vkGetQueueCheckpointDataNV GetQueueCheckpointDataNV;

  // VK_INTEL_performance_query
  PFN_vkInitializePerformanceApiINTEL InitializePerformanceApiINTEL;
  PFN_vkUninitializePerformanceApiINTEL UninitializePerformanceApiINTEL;
  PFN_vkCmdSetPerformanceMarkerINTEL CmdSetPerformanceMarkerINTEL;
  PFN_vkCmdSetPerformanceStreamMarkerINTEL CmdSetPerformanceStreamMarkerINTEL;
  PFN_vkCmdSetPerformanceOverrideINTEL CmdSetPerformanceOverrideINTEL;
  PFN_vkAcquirePerformanceConfigurationINTEL AcquirePerformanceConfigurationINTEL;
  PFN_vkReleasePerformanceConfigurationINTEL ReleasePerformanceConfigurationINTEL;
  PFN_vkQueueSetPerformanceConfigurationINTEL QueueSetPerformanceConfigurationINTEL;
  PFN_vkGetPerformanceParameterINTEL GetPerformanceParameterINTEL;

  // VK_AMD_display_native_hdr
  PFN_vkSetLocalDimmingAMD SetLocalDimmingAMD;

  // VK_EXT_buffer_device_address
  PFN_vkGetBufferDeviceAddressEXT GetBufferDeviceAddressEXT;

  // VK_EXT_full_screen_exclusive
#ifdef VK_USE_PLATFORM_WIN32_KHR
  PFN_vkAcquireFullScreenExclusiveModeEXT AcquireFullScreenExclusiveModeEXT;
  PFN_vkReleaseFullScreenExclusiveModeEXT ReleaseFullScreenExclusiveModeEXT;
  PFN_vkGetDeviceGroupSurfacePresentModes2EXT GetDeviceGroupSurfacePresentModes2EXT;
#endif // VK_USE_PLATFORM_WIN32_KHR

  // VK_EXT_line_rasterization
  PFN_vkCmdSetLineStippleEXT CmdSetLineStippleEXT;

  // VK_EXT_host_query_reset
  PFN_vkResetQueryPoolEXT ResetQueryPoolEXT;

  // VK_EXT_extended_dynamic_state
  PFN_vkCmdSetCullModeEXT CmdSetCullModeEXT;
  PFN_vkCmdSetFrontFaceEXT CmdSetFrontFaceEXT;
  PFN_vkCmdSetPrimitiveTopologyEXT CmdSetPrimitiveTopologyEXT;
  PFN_vkCmdSetViewportWithCountEXT CmdSetViewportWithCountEXT;
  PFN_vkCmdSetScissorWithCountEXT CmdSetScissorWithCountEXT;
  PFN_vkCmdBindVertexBuffers2EXT CmdBindVertexBuffers2EXT;
  PFN_vkCmdSetDepthTestEnableEXT CmdSetDepthTestEnableEXT;
  PFN_vkCmdSetDepthWriteEnableEXT CmdSetDepthWriteEnableEXT;
  PFN_vkCmdSetDepthCompareOpEXT CmdSetDepthCompareOpEXT;
  PFN_vkCmdSetDepthBoundsTestEnableEXT CmdSetDepthBoundsTestEnableEXT;
  PFN_vkCmdSetStencilTestEnableEXT CmdSetStencilTestEnableEXT;
  PFN_vkCmdSetStencilOpEXT CmdSetStencilOpEXT;

  // VK_NV_device_generated_commands
  PFN_vkGetGeneratedCommandsMemoryRequirementsNV GetGeneratedCommandsMemoryRequirementsNV;
  PFN_vkCmdPreprocessGeneratedCommandsNV CmdPreprocessGeneratedCommandsNV;
  PFN_vkCmdExecuteGeneratedCommandsNV CmdExecuteGeneratedCommandsNV;
  PFN_vkCmdBindPipelineShaderGroupNV CmdBindPipelineShaderGroupNV;
  PFN_vkCreateIndirectCommandsLayoutNV CreateIndirectCommandsLayoutNV;
  PFN_vkDestroyIndirectCommandsLayoutNV DestroyIndirectCommandsLayoutNV;

  // VK_EXT_private_data
  PFN_vkCreatePrivateDataSlotEXT CreatePrivateDataSlotEXT;
  PFN_vkDestroyPrivateDataSlotEXT DestroyPrivateDataSlotEXT;
  PFN_vkSetPrivateDataEXT SetPrivateDataEXT;
  PFN_vkGetPrivateDataEXT GetPrivateDataEXT;

  // VK_NV_fragment_shading_rate_enums
  PFN_vkCmdSetFragmentShadingRateEnumNV CmdSetFragmentShadingRateEnumNV;

  // for consistency with macros, we declare the CreateDevice pointer here
  // even though it won't actually ever get used and is on the instance dispatch chain
  PFN_vkCreateDevice CreateDevice;
};
