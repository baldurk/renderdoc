/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#pragma once

#include "official/vk_layer.h"
#include "official/vk_layer_dispatch_table.h"

// extensions made core in 1.1
#define VK11 VK_MAKE_VERSION(1, 1, 0)

// extensions that are not core in any version
#define VKXX VK_MAKE_VERSION(99, 99, 0)

#if defined(VK_USE_PLATFORM_WIN32_KHR)

#define HookInitInstance_PlatformSpecific()                                              \
  HookInitExtension(VK_KHR_win32_surface, CreateWin32SurfaceKHR);                        \
  HookInitExtension(VK_KHR_win32_surface, GetPhysicalDeviceWin32PresentationSupportKHR); \
  HookInitExtension(VK_EXT_full_screen_exclusive, GetPhysicalDeviceSurfacePresentModes2EXT);

#define HookInitDevice_PlatformSpecific()                                             \
  HookInitExtension(VK_NV_win32_keyed_mutex, GetMemoryWin32HandleNV);                 \
  HookInitExtension(VK_KHR_external_memory_win32, GetMemoryWin32HandleKHR);           \
  HookInitExtension(VK_KHR_external_memory_win32, GetMemoryWin32HandlePropertiesKHR); \
  HookInitExtension(VK_KHR_external_semaphore_win32, ImportSemaphoreWin32HandleKHR);  \
  HookInitExtension(VK_KHR_external_semaphore_win32, GetSemaphoreWin32HandleKHR);     \
  HookInitExtension(VK_KHR_external_fence_win32, ImportFenceWin32HandleKHR);          \
  HookInitExtension(VK_KHR_external_fence_win32, GetFenceWin32HandleKHR);             \
  HookInitExtension(VK_EXT_full_screen_exclusive, AcquireFullScreenExclusiveModeEXT); \
  HookInitExtension(VK_EXT_full_screen_exclusive, ReleaseFullScreenExclusiveModeEXT); \
  HookInitExtension(VK_EXT_full_screen_exclusive, GetDeviceGroupSurfacePresentModes2EXT);

#define HookDefine_PlatformSpecific()                                                            \
  HookDefine4(VkResult, vkCreateWin32SurfaceKHR, VkInstance, instance,                           \
              const VkWin32SurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *,   \
              pAllocator, VkSurfaceKHR *, pSurface);                                             \
  HookDefine2(VkBool32, vkGetPhysicalDeviceWin32PresentationSupportKHR, VkPhysicalDevice,        \
              physicalDevice, uint32_t, queueFamilyIndex);                                       \
  HookDefine4(VkResult, vkGetMemoryWin32HandleNV, VkDevice, device, VkDeviceMemory, memory,      \
              VkExternalMemoryHandleTypeFlagsNV, handleType, HANDLE *, pHandle);                 \
  HookDefine3(VkResult, vkGetMemoryWin32HandleKHR, VkDevice, device,                             \
              const VkMemoryGetWin32HandleInfoKHR *, pGetWin32HandleInfo, HANDLE *, pHandle);    \
  HookDefine4(VkResult, vkGetMemoryWin32HandlePropertiesKHR, VkDevice, device,                   \
              VkExternalMemoryHandleTypeFlagBitsKHR, handleType, HANDLE, handle,                 \
              VkMemoryWin32HandlePropertiesKHR *, pMemoryWin32HandleProperties);                 \
  HookDefine2(VkResult, vkImportSemaphoreWin32HandleKHR, VkDevice, device,                       \
              const VkImportSemaphoreWin32HandleInfoKHR *, pImportSemaphoreWin32HandleInfo);     \
  HookDefine3(VkResult, vkGetSemaphoreWin32HandleKHR, VkDevice, device,                          \
              const VkSemaphoreGetWin32HandleInfoKHR *, pGetWin32HandleInfo, HANDLE *, pHandle); \
  HookDefine2(VkResult, vkImportFenceWin32HandleKHR, VkDevice, device,                           \
              const VkImportFenceWin32HandleInfoKHR *, pImportFenceWin32HandleInfo);             \
  HookDefine3(VkResult, vkGetFenceWin32HandleKHR, VkDevice, device,                              \
              const VkFenceGetWin32HandleInfoKHR *, pGetWin32HandleInfo, HANDLE *, pHandle);     \
  HookDefine4(VkResult, vkGetPhysicalDeviceSurfacePresentModes2EXT, VkPhysicalDevice,            \
              physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, pSurfaceInfo, uint32_t *, \
              pPresentModeCount, VkPresentModeKHR *, pPresentModes);                             \
  HookDefine3(VkResult, vkGetDeviceGroupSurfacePresentModes2EXT, VkDevice, device,               \
              const VkPhysicalDeviceSurfaceInfo2KHR *, pSurfaceInfo,                             \
              VkDeviceGroupPresentModeFlagsKHR *, pModes);                                       \
  HookDefine2(VkResult, vkAcquireFullScreenExclusiveModeEXT, VkDevice, device, VkSwapchainKHR,   \
              swapchain);                                                                        \
  HookDefine2(VkResult, vkReleaseFullScreenExclusiveModeEXT, VkDevice, device, VkSwapchainKHR,   \
              swapchain);

#elif defined(VK_USE_PLATFORM_MACOS_MVK)

#define HookInitInstance_PlatformSpecific() \
  HookInitExtension(VK_MVK_macos_surface, CreateMacOSSurfaceMVK);

#define HookInitDevice_PlatformSpecific()

#define HookDefine_PlatformSpecific()                                                          \
  HookDefine4(VkResult, vkCreateMacOSSurfaceMVK, VkInstance, instance,                         \
              const VkMacOSSurfaceCreateInfoMVK *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);

#elif defined(VK_USE_PLATFORM_ANDROID_KHR)

#define HookInitInstance_PlatformSpecific() \
  HookInitExtension(VK_KHR_android_surface, CreateAndroidSurfaceKHR);

#define HookInitDevice_PlatformSpecific()

#define HookDefine_PlatformSpecific()                                                            \
  HookDefine4(VkResult, vkCreateAndroidSurfaceKHR, VkInstance, instance,                         \
              const VkAndroidSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);

#elif defined(VK_USE_PLATFORM_GGP)

#define HookInitInstance_PlatformSpecific() \
  HookInitExtension(VK_GGP_stream_descriptor_surface, CreateStreamDescriptorSurfaceGGP);

#define HookInitDevice_PlatformSpecific()

#define HookDefine_PlatformSpecific()                                             \
  HookDefine4(VkResult, vkCreateStreamDescriptorSurfaceGGP, VkInstance, instance, \
              const VkStreamDescriptorSurfaceCreateInfoGGP *, pCreateInfo,        \
              const VkAllocationCallbacks *, pAllocator, VkSurfaceKHR *, pSurface);

#else

#if defined(VK_USE_PLATFORM_XCB_KHR)

#define HookInitInstance_PlatformSpecific_Xcb()               \
  HookInitExtension(VK_KHR_xcb_surface, CreateXcbSurfaceKHR); \
  HookInitExtension(VK_KHR_xcb_surface, GetPhysicalDeviceXcbPresentationSupportKHR);

#define HookDefine_PlatformSpecific_Xcb()                                                    \
  HookDefine4(VkResult, vkCreateXcbSurfaceKHR, VkInstance, instance,                         \
              const VkXcbSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);                                         \
  HookDefine4(VkBool32, vkGetPhysicalDeviceXcbPresentationSupportKHR, VkPhysicalDevice,      \
              physicalDevice, uint32_t, queueFamilyIndex, xcb_connection_t *, connection,    \
              xcb_visualid_t, visual_id);

#else

#define HookInitInstance_PlatformSpecific_Xcb()
#define HookDefine_PlatformSpecific_Xcb()

#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)

#define HookInitInstance_PlatformSpecific_Xlib()                                       \
  HookInitExtension(VK_KHR_xlib_surface, CreateXlibSurfaceKHR);                        \
  HookInitExtension(VK_KHR_xlib_surface, GetPhysicalDeviceXlibPresentationSupportKHR); \
  HookInitExtension(VK_EXT_acquire_xlib_display, AcquireXlibDisplayEXT);               \
  HookInitExtension(VK_EXT_acquire_xlib_display, GetRandROutputDisplayEXT);

#define HookDefine_PlatformSpecific_Xlib()                                                         \
  HookDefine4(VkResult, vkCreateXlibSurfaceKHR, VkInstance, instance,                              \
              const VkXlibSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *,      \
              pAllocator, VkSurfaceKHR *, pSurface);                                               \
  HookDefine4(VkBool32, vkGetPhysicalDeviceXlibPresentationSupportKHR, VkPhysicalDevice,           \
              physicalDevice, uint32_t, queueFamilyIndex, Display *, dpy, VisualID, visualID);     \
  HookDefine3(VkResult, vkAcquireXlibDisplayEXT, VkPhysicalDevice, physicalDevice, Display *, dpy, \
              VkDisplayKHR, display);                                                              \
  HookDefine4(VkResult, vkGetRandROutputDisplayEXT, VkPhysicalDevice, physicalDevice, Display *,   \
              dpy, RROutput, rrOutput, VkDisplayKHR *, pDisplay);

#else

#define HookInitInstance_PlatformSpecific_Xlib()
#define HookDefine_PlatformSpecific_Xlib()

#endif

#define HookInitInstance_PlatformSpecific() \
  HookInitInstance_PlatformSpecific_Xcb() HookInitInstance_PlatformSpecific_Xlib()
#define HookInitDevice_PlatformSpecific()
#define HookDefine_PlatformSpecific() \
  HookDefine_PlatformSpecific_Xcb() HookDefine_PlatformSpecific_Xlib()

#endif

#define HookInitVulkanInstance()                          \
  HookInit(CreateInstance);                               \
  HookInit(DestroyInstance);                              \
  HookInit(EnumeratePhysicalDevices);                     \
  HookInit(GetPhysicalDeviceFeatures);                    \
  HookInit(GetPhysicalDeviceImageFormatProperties);       \
  HookInit(GetPhysicalDeviceFormatProperties);            \
  HookInit(GetPhysicalDeviceSparseImageFormatProperties); \
  HookInit(GetPhysicalDeviceProperties);                  \
  HookInit(GetPhysicalDeviceQueueFamilyProperties);       \
  HookInit(GetPhysicalDeviceMemoryProperties);

#define HookInitVulkanDevice()                \
  HookInit(CreateDevice);                     \
  HookInit(DestroyDevice);                    \
  HookInit(GetDeviceQueue);                   \
  HookInit(QueueSubmit);                      \
  HookInit(QueueWaitIdle);                    \
  HookInit(DeviceWaitIdle);                   \
  HookInit(AllocateMemory);                   \
  HookInit(FreeMemory);                       \
  HookInit(MapMemory);                        \
  HookInit(UnmapMemory);                      \
  HookInit(FlushMappedMemoryRanges);          \
  HookInit(InvalidateMappedMemoryRanges);     \
  HookInit(GetDeviceMemoryCommitment);        \
  HookInit(BindBufferMemory);                 \
  HookInit(BindImageMemory);                  \
  HookInit(QueueBindSparse);                  \
  HookInit(CreateBuffer);                     \
  HookInit(DestroyBuffer);                    \
  HookInit(CreateBufferView);                 \
  HookInit(DestroyBufferView);                \
  HookInit(CreateImage);                      \
  HookInit(DestroyImage);                     \
  HookInit(GetImageSubresourceLayout);        \
  HookInit(GetBufferMemoryRequirements);      \
  HookInit(GetImageMemoryRequirements);       \
  HookInit(GetImageSparseMemoryRequirements); \
  HookInit(CreateImageView);                  \
  HookInit(DestroyImageView);                 \
  HookInit(CreateShaderModule);               \
  HookInit(DestroyShaderModule);              \
  HookInit(CreateGraphicsPipelines);          \
  HookInit(CreateComputePipelines);           \
  HookInit(DestroyPipeline);                  \
  HookInit(CreatePipelineCache);              \
  HookInit(GetPipelineCacheData);             \
  HookInit(MergePipelineCaches);              \
  HookInit(DestroyPipelineCache);             \
  HookInit(CreatePipelineLayout);             \
  HookInit(DestroyPipelineLayout);            \
  HookInit(CreateSemaphore);                  \
  HookInit(DestroySemaphore);                 \
  HookInit(CreateFence);                      \
  HookInit(GetFenceStatus);                   \
  HookInit(ResetFences);                      \
  HookInit(WaitForFences);                    \
  HookInit(DestroyFence);                     \
  HookInit(CreateEvent);                      \
  HookInit(GetEventStatus);                   \
  HookInit(ResetEvent);                       \
  HookInit(SetEvent);                         \
  HookInit(DestroyEvent);                     \
  HookInit(CreateQueryPool);                  \
  HookInit(GetQueryPoolResults);              \
  HookInit(DestroyQueryPool);                 \
  HookInit(CreateSampler);                    \
  HookInit(DestroySampler);                   \
  HookInit(CreateDescriptorSetLayout);        \
  HookInit(DestroyDescriptorSetLayout);       \
  HookInit(CreateDescriptorPool);             \
  HookInit(ResetDescriptorPool);              \
  HookInit(DestroyDescriptorPool);            \
  HookInit(AllocateDescriptorSets);           \
  HookInit(UpdateDescriptorSets);             \
  HookInit(FreeDescriptorSets);               \
  HookInit(GetRenderAreaGranularity);         \
  HookInit(CreateCommandPool);                \
  HookInit(DestroyCommandPool);               \
  HookInit(ResetCommandPool);                 \
  HookInit(AllocateCommandBuffers);           \
  HookInit(FreeCommandBuffers);               \
  HookInit(BeginCommandBuffer);               \
  HookInit(EndCommandBuffer);                 \
  HookInit(ResetCommandBuffer);               \
  HookInit(CmdBindPipeline);                  \
  HookInit(CmdSetViewport);                   \
  HookInit(CmdSetScissor);                    \
  HookInit(CmdSetLineWidth);                  \
  HookInit(CmdSetDepthBias);                  \
  HookInit(CmdSetBlendConstants);             \
  HookInit(CmdSetDepthBounds);                \
  HookInit(CmdSetStencilCompareMask);         \
  HookInit(CmdSetStencilWriteMask);           \
  HookInit(CmdSetStencilReference);           \
  HookInit(CmdBindDescriptorSets);            \
  HookInit(CmdBindVertexBuffers);             \
  HookInit(CmdBindIndexBuffer);               \
  HookInit(CmdDraw);                          \
  HookInit(CmdDrawIndirect);                  \
  HookInit(CmdDrawIndexed);                   \
  HookInit(CmdDrawIndexedIndirect);           \
  HookInit(CmdDispatch);                      \
  HookInit(CmdDispatchIndirect);              \
  HookInit(CmdCopyBufferToImage);             \
  HookInit(CmdCopyImageToBuffer);             \
  HookInit(CmdCopyBuffer);                    \
  HookInit(CmdCopyImage);                     \
  HookInit(CmdBlitImage);                     \
  HookInit(CmdResolveImage);                  \
  HookInit(CmdUpdateBuffer);                  \
  HookInit(CmdFillBuffer);                    \
  HookInit(CmdPushConstants);                 \
  HookInit(CmdClearColorImage);               \
  HookInit(CmdClearDepthStencilImage);        \
  HookInit(CmdClearAttachments);              \
  HookInit(CmdPipelineBarrier);               \
  HookInit(CmdWriteTimestamp);                \
  HookInit(CmdCopyQueryPoolResults);          \
  HookInit(CmdBeginQuery);                    \
  HookInit(CmdEndQuery);                      \
  HookInit(CmdResetQueryPool);                \
  HookInit(CmdSetEvent);                      \
  HookInit(CmdResetEvent);                    \
  HookInit(CmdWaitEvents);                    \
  HookInit(CreateFramebuffer);                \
  HookInit(DestroyFramebuffer);               \
  HookInit(CreateRenderPass);                 \
  HookInit(DestroyRenderPass);                \
  HookInit(CmdBeginRenderPass);               \
  HookInit(CmdNextSubpass);                   \
  HookInit(CmdExecuteCommands);               \
  HookInit(CmdEndRenderPass);

// We can always build in VK_KHR_display and VK_KHR_display_swapchain support
// because they don't need any libraries or headers.
// They're not really used or  relevant on win32/android but for platform simplicity
// we just include it always, it does no harm to include.

// this is the union of the lists below - necessary because some extensions are in both lists
// (device extensions with physical device functions)
#define DeclExts()                              \
  DeclExt(KHR_xlib_surface);                    \
  DeclExt(KHR_xcb_surface);                     \
  DeclExt(KHR_win32_surface);                   \
  DeclExt(KHR_android_surface);                 \
  DeclExt(MVK_macos_surface);                   \
  DeclExt(KHR_surface);                         \
  DeclExt(GGP_stream_descriptor_surface);       \
  DeclExt(EXT_debug_report);                    \
  DeclExt(KHR_display);                         \
  DeclExt(NV_external_memory_capabilities);     \
  DeclExt(KHR_get_physical_device_properties2); \
  DeclExt(EXT_display_surface_counter);         \
  DeclExt(EXT_direct_mode_display);             \
  DeclExt(EXT_acquire_xlib_display);            \
  DeclExt(KHR_external_memory_capabilities);    \
  DeclExt(KHR_external_semaphore_capabilities); \
  DeclExt(KHR_external_fence_capabilities);     \
  DeclExt(EXT_debug_utils);                     \
  DeclExt(KHR_device_group_creation);           \
  DeclExt(protected_memory);                    \
  DeclExt(KHR_get_surface_capabilities2);       \
  DeclExt(KHR_get_display_properties2);         \
  /* device extensions */                       \
  DeclExt(EXT_debug_marker);                    \
  DeclExt(GGP_frame_token);                     \
  DeclExt(KHR_swapchain);                       \
  DeclExt(KHR_display_swapchain);               \
  DeclExt(NV_external_memory);                  \
  DeclExt(NV_external_memory_win32);            \
  DeclExt(NV_win32_keyed_mutex);                \
  DeclExt(KHR_maintenance1);                    \
  DeclExt(KHR_maintenance2);                    \
  DeclExt(KHR_maintenance3);                    \
  DeclExt(EXT_display_control);                 \
  DeclExt(KHR_external_memory);                 \
  DeclExt(KHR_external_memory_win32);           \
  DeclExt(KHR_external_memory_fd);              \
  DeclExt(KHR_external_semaphore);              \
  DeclExt(KHR_external_semaphore_win32);        \
  DeclExt(KHR_external_semaphore_fd);           \
  DeclExt(KHR_external_fence);                  \
  DeclExt(KHR_external_fence_win32);            \
  DeclExt(KHR_external_fence_fd);               \
  DeclExt(KHR_get_memory_requirements2);        \
  DeclExt(AMD_shader_info);                     \
  DeclExt(KHR_push_descriptor);                 \
  DeclExt(KHR_descriptor_update_template);      \
  DeclExt(KHR_bind_memory2);                    \
  DeclExt(EXT_conservative_rasterization);      \
  DeclExt(EXT_global_priority);                 \
  DeclExt(AMD_buffer_marker);                   \
  DeclExt(EXT_vertex_attribute_divisor);        \
  DeclExt(EXT_sampler_filter_minmax);           \
  DeclExt(KHR_sampler_ycbcr_conversion);        \
  DeclExt(KHR_device_group);                    \
  DeclExt(MVK_moltenvk);                        \
  DeclExt(KHR_draw_indirect_count);             \
  DeclExt(EXT_validation_cache);                \
  DeclExt(KHR_shared_presentable_image);        \
  DeclExt(KHR_create_renderpass2);              \
  DeclExt(EXT_transform_feedback);              \
  DeclExt(EXT_conditional_rendering);           \
  DeclExt(EXT_sample_locations);                \
  DeclExt(EXT_discard_rectangles);              \
  DeclExt(EXT_calibrated_timestamps);           \
  DeclExt(EXT_host_query_reset);                \
  DeclExt(EXT_buffer_device_address);           \
  DeclExt(EXT_full_screen_exclusive);           \
  DeclExt(EXT_hdr_metadata);                    \
  DeclExt(AMD_display_native_hdr);

// for simplicity and since the check itself is platform agnostic,
// these aren't protected in platform defines
#define CheckInstanceExts()                            \
  CheckExt(KHR_xlib_surface, VKXX);                    \
  CheckExt(KHR_xcb_surface, VKXX);                     \
  CheckExt(KHR_win32_surface, VKXX);                   \
  CheckExt(KHR_android_surface, VKXX);                 \
  CheckExt(MVK_macos_surface, VKXX);                   \
  CheckExt(KHR_surface, VKXX);                         \
  CheckExt(GGP_stream_descriptor_surface, VKXX);       \
  CheckExt(EXT_debug_report, VKXX);                    \
  CheckExt(KHR_display, VKXX);                         \
  CheckExt(NV_external_memory_capabilities, VKXX);     \
  CheckExt(KHR_get_physical_device_properties2, VK11); \
  CheckExt(EXT_display_surface_counter, VKXX);         \
  CheckExt(EXT_direct_mode_display, VKXX);             \
  CheckExt(EXT_acquire_xlib_display, VKXX);            \
  CheckExt(KHR_external_memory_capabilities, VK11);    \
  CheckExt(KHR_external_semaphore_capabilities, VK11); \
  CheckExt(KHR_external_fence_capabilities, VK11);     \
  CheckExt(EXT_debug_utils, VKXX);                     \
  CheckExt(KHR_device_group_creation, VK11);           \
  CheckExt(protected_memory, VK11);                    \
  CheckExt(KHR_get_surface_capabilities2, VKXX);       \
  CheckExt(KHR_get_display_properties2, VKXX);         \
  CheckExt(EXT_sample_locations, VKXX);                \
  CheckExt(EXT_calibrated_timestamps, VKXX);           \
  CheckExt(EXT_full_screen_exclusive, VKXX);

#define CheckDeviceExts()                         \
  CheckExt(EXT_debug_marker, VKXX);               \
  CheckExt(GGP_frame_token, VKXX);                \
  CheckExt(KHR_swapchain, VKXX);                  \
  CheckExt(KHR_display_swapchain, VKXX);          \
  CheckExt(NV_external_memory, VKXX);             \
  CheckExt(NV_external_memory_win32, VKXX);       \
  CheckExt(NV_win32_keyed_mutex, VKXX);           \
  CheckExt(KHR_maintenance1, VK11);               \
  CheckExt(KHR_maintenance2, VK11);               \
  CheckExt(KHR_maintenance3, VK11);               \
  CheckExt(EXT_display_control, VKXX);            \
  CheckExt(KHR_external_memory, VK11);            \
  CheckExt(KHR_external_memory_win32, VKXX);      \
  CheckExt(KHR_external_memory_fd, VKXX);         \
  CheckExt(KHR_external_semaphore, VK11);         \
  CheckExt(KHR_external_semaphore_win32, VKXX);   \
  CheckExt(KHR_external_semaphore_fd, VKXX);      \
  CheckExt(KHR_external_fence, VK11);             \
  CheckExt(KHR_external_fence_win32, VKXX);       \
  CheckExt(KHR_external_fence_fd, VKXX);          \
  CheckExt(KHR_get_memory_requirements2, VK11);   \
  CheckExt(AMD_shader_info, VKXX);                \
  CheckExt(KHR_push_descriptor, VKXX);            \
  CheckExt(KHR_descriptor_update_template, VK11); \
  CheckExt(KHR_bind_memory2, VK11);               \
  CheckExt(EXT_conservative_rasterization, VKXX); \
  CheckExt(EXT_global_priority, VKXX);            \
  CheckExt(AMD_buffer_marker, VKXX);              \
  CheckExt(EXT_vertex_attribute_divisor, VKXX);   \
  CheckExt(EXT_sampler_filter_minmax, VKXX);      \
  CheckExt(KHR_sampler_ycbcr_conversion, VK11);   \
  CheckExt(KHR_device_group, VK11);               \
  CheckExt(MVK_moltenvk, VKXX);                   \
  CheckExt(KHR_draw_indirect_count, VKXX);        \
  CheckExt(EXT_validation_cache, VKXX);           \
  CheckExt(KHR_shared_presentable_image, VKXX);   \
  CheckExt(KHR_create_renderpass2, VKXX);         \
  CheckExt(EXT_transform_feedback, VKXX);         \
  CheckExt(EXT_conditional_rendering, VKXX);      \
  CheckExt(EXT_sample_locations, VKXX);           \
  CheckExt(EXT_discard_rectangles, VKXX);         \
  CheckExt(EXT_calibrated_timestamps, VKXX);      \
  CheckExt(EXT_host_query_reset, VKXX);           \
  CheckExt(EXT_buffer_device_address, VKXX);      \
  CheckExt(EXT_hdr_metadata, VKXX);               \
  CheckExt(AMD_display_native_hdr, VKXX);

#define HookInitVulkanInstanceExts()                                                                 \
  HookInitExtension(KHR_surface, DestroySurfaceKHR);                                                 \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfaceSupportKHR);                                \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfaceCapabilitiesKHR);                           \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfaceFormatsKHR);                                \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfacePresentModesKHR);                           \
  HookInitExtension(EXT_debug_report, CreateDebugReportCallbackEXT);                                 \
  HookInitExtension(EXT_debug_report, DestroyDebugReportCallbackEXT);                                \
  HookInitExtension(EXT_debug_report, DebugReportMessageEXT);                                        \
  HookInitExtension(KHR_display, GetPhysicalDeviceDisplayPropertiesKHR);                             \
  HookInitExtension(KHR_display, GetPhysicalDeviceDisplayPlanePropertiesKHR);                        \
  HookInitExtension(KHR_display, GetDisplayPlaneSupportedDisplaysKHR);                               \
  HookInitExtension(KHR_display, GetDisplayModePropertiesKHR);                                       \
  HookInitExtension(KHR_display, CreateDisplayModeKHR);                                              \
  HookInitExtension(KHR_display, GetDisplayPlaneCapabilitiesKHR);                                    \
  HookInitExtension(KHR_display, CreateDisplayPlaneSurfaceKHR);                                      \
  HookInitExtension(NV_external_memory_capabilities,                                                 \
                    GetPhysicalDeviceExternalImageFormatPropertiesNV);                               \
  HookInitPromotedExtension(KHR_get_physical_device_properties2, GetPhysicalDeviceFeatures2, KHR);   \
  HookInitPromotedExtension(KHR_get_physical_device_properties2, GetPhysicalDeviceProperties2, KHR); \
  HookInitPromotedExtension(KHR_get_physical_device_properties2,                                     \
                            GetPhysicalDeviceFormatProperties2, KHR);                                \
  HookInitPromotedExtension(KHR_get_physical_device_properties2,                                     \
                            GetPhysicalDeviceImageFormatProperties2, KHR);                           \
  HookInitPromotedExtension(KHR_get_physical_device_properties2,                                     \
                            GetPhysicalDeviceQueueFamilyProperties2, KHR);                           \
  HookInitPromotedExtension(KHR_get_physical_device_properties2,                                     \
                            GetPhysicalDeviceMemoryProperties2, KHR);                                \
  HookInitPromotedExtension(KHR_get_physical_device_properties2,                                     \
                            GetPhysicalDeviceSparseImageFormatProperties2, KHR);                     \
  HookInitExtension(EXT_direct_mode_display, ReleaseDisplayEXT);                                     \
  HookInitExtension(EXT_display_surface_counter, GetPhysicalDeviceSurfaceCapabilities2EXT);          \
  HookInitPromotedExtension(KHR_external_memory_capabilities,                                        \
                            GetPhysicalDeviceExternalBufferProperties, KHR);                         \
  HookInitPromotedExtension(KHR_external_semaphore_capabilities,                                     \
                            GetPhysicalDeviceExternalSemaphoreProperties, KHR);                      \
  HookInitPromotedExtension(KHR_external_fence_capabilities,                                         \
                            GetPhysicalDeviceExternalFenceProperties, KHR);                          \
  HookInitExtension(EXT_debug_utils, CreateDebugUtilsMessengerEXT);                                  \
  HookInitExtension(EXT_debug_utils, DestroyDebugUtilsMessengerEXT);                                 \
  HookInitExtension(EXT_debug_utils, SubmitDebugUtilsMessageEXT);                                    \
  HookInitPromotedExtension(KHR_device_group_creation, EnumeratePhysicalDeviceGroups, KHR);          \
  /* Not technically accurate - part of KHR_device_group - but these extensions are linked and */    \
  /* should always be present/not present together. Keying from the instance extension ensures */    \
  /* we'll load this function correctly when populating dispatch tables. */                          \
  HookInitExtension(KHR_device_group_creation &&KHR_surface, GetPhysicalDevicePresentRectanglesKHR); \
  HookInitExtension(KHR_get_surface_capabilities2, GetPhysicalDeviceSurfaceFormats2KHR);             \
  HookInitExtension(KHR_get_surface_capabilities2, GetPhysicalDeviceSurfaceCapabilities2KHR);        \
  HookInitExtension(KHR_get_display_properties2, GetPhysicalDeviceDisplayProperties2KHR);            \
  HookInitExtension(KHR_get_display_properties2, GetPhysicalDeviceDisplayPlaneProperties2KHR);       \
  HookInitExtension(KHR_get_display_properties2, GetDisplayModeProperties2KHR);                      \
  HookInitExtension(KHR_get_display_properties2, GetDisplayPlaneCapabilities2KHR);                   \
  HookInitExtension(EXT_sample_locations, GetPhysicalDeviceMultisamplePropertiesEXT);                \
  HookInitExtension(EXT_calibrated_timestamps, GetPhysicalDeviceCalibrateableTimeDomainsEXT);        \
  HookInitInstance_PlatformSpecific()

#define HookInitVulkanDeviceExts()                                                                 \
  HookInitExtension(EXT_debug_marker, DebugMarkerSetObjectTagEXT);                                 \
  HookInitExtension(EXT_debug_marker, DebugMarkerSetObjectNameEXT);                                \
  HookInitExtension(EXT_debug_marker, CmdDebugMarkerBeginEXT);                                     \
  HookInitExtension(EXT_debug_marker, CmdDebugMarkerEndEXT);                                       \
  HookInitExtension(EXT_debug_marker, CmdDebugMarkerInsertEXT);                                    \
  HookInitExtension(KHR_swapchain, CreateSwapchainKHR);                                            \
  HookInitExtension(KHR_swapchain, DestroySwapchainKHR);                                           \
  HookInitExtension(KHR_swapchain, GetSwapchainImagesKHR);                                         \
  HookInitExtension(KHR_swapchain, AcquireNextImageKHR);                                           \
  HookInitExtension(KHR_swapchain, QueuePresentKHR);                                               \
  HookInitExtension(KHR_display_swapchain, CreateSharedSwapchainsKHR);                             \
  HookInitPromotedExtension(KHR_maintenance1, TrimCommandPool, KHR);                               \
  HookInitExtension(EXT_display_control, DisplayPowerControlEXT);                                  \
  HookInitExtension(EXT_display_control, RegisterDeviceEventEXT);                                  \
  HookInitExtension(EXT_display_control, RegisterDisplayEventEXT);                                 \
  HookInitExtension(EXT_display_control, GetSwapchainCounterEXT);                                  \
  HookInitExtension(KHR_external_memory_fd, GetMemoryFdKHR);                                       \
  HookInitExtension(KHR_external_memory_fd, GetMemoryFdPropertiesKHR);                             \
  HookInitExtension(KHR_external_semaphore_fd, ImportSemaphoreFdKHR);                              \
  HookInitExtension(KHR_external_semaphore_fd, GetSemaphoreFdKHR);                                 \
  HookInitExtension(KHR_external_fence_fd, ImportFenceFdKHR);                                      \
  HookInitExtension(KHR_external_fence_fd, GetFenceFdKHR);                                         \
  HookInitPromotedExtension(KHR_get_memory_requirements2, GetBufferMemoryRequirements2, KHR);      \
  HookInitPromotedExtension(KHR_get_memory_requirements2, GetImageMemoryRequirements2, KHR);       \
  HookInitPromotedExtension(KHR_get_memory_requirements2, GetImageSparseMemoryRequirements2, KHR); \
  HookInitExtension(AMD_shader_info, GetShaderInfoAMD);                                            \
  HookInitExtension(KHR_push_descriptor, CmdPushDescriptorSetKHR);                                 \
  HookInitPromotedExtension(KHR_descriptor_update_template, CreateDescriptorUpdateTemplate, KHR);  \
  HookInitPromotedExtension(KHR_descriptor_update_template, DestroyDescriptorUpdateTemplate, KHR); \
  HookInitPromotedExtension(KHR_descriptor_update_template, UpdateDescriptorSetWithTemplate, KHR); \
  HookInitExtension(KHR_push_descriptor &&KHR_descriptor_update_template,                          \
                    CmdPushDescriptorSetWithTemplateKHR);                                          \
  HookInitPromotedExtension(KHR_bind_memory2, BindBufferMemory2, KHR);                             \
  HookInitPromotedExtension(KHR_bind_memory2, BindImageMemory2, KHR);                              \
  HookInitPromotedExtension(KHR_maintenance3, GetDescriptorSetLayoutSupport, KHR);                 \
  HookInitExtension(AMD_buffer_marker, CmdWriteBufferMarkerAMD);                                   \
  HookInitExtension(EXT_debug_utils, SetDebugUtilsObjectNameEXT);                                  \
  HookInitExtension(EXT_debug_utils, SetDebugUtilsObjectTagEXT);                                   \
  HookInitExtension(EXT_debug_utils, QueueBeginDebugUtilsLabelEXT);                                \
  HookInitExtension(EXT_debug_utils, QueueEndDebugUtilsLabelEXT);                                  \
  HookInitExtension(EXT_debug_utils, QueueInsertDebugUtilsLabelEXT);                               \
  HookInitExtension(EXT_debug_utils, CmdBeginDebugUtilsLabelEXT);                                  \
  HookInitExtension(EXT_debug_utils, CmdEndDebugUtilsLabelEXT);                                    \
  HookInitExtension(EXT_debug_utils, CmdInsertDebugUtilsLabelEXT);                                 \
  HookInitPromotedExtension(KHR_sampler_ycbcr_conversion, CreateSamplerYcbcrConversion, KHR);      \
  HookInitPromotedExtension(KHR_sampler_ycbcr_conversion, DestroySamplerYcbcrConversion, KHR);     \
  HookInitPromotedExtension(KHR_device_group, GetDeviceGroupPeerMemoryFeatures, KHR);              \
  HookInitPromotedExtension(KHR_device_group, CmdSetDeviceMask, KHR);                              \
  HookInitPromotedExtension(KHR_device_group, CmdDispatchBase, KHR);                               \
  HookInitExtension(KHR_device_group &&KHR_surface, GetDeviceGroupPresentCapabilitiesKHR);         \
  HookInitExtension(KHR_device_group &&KHR_surface, GetDeviceGroupSurfacePresentModesKHR);         \
  HookInitExtension(KHR_device_group &&KHR_swapchain, AcquireNextImage2KHR);                       \
  HookInitExtension(protected_memory, GetDeviceQueue2);                                            \
  HookInitExtension(KHR_draw_indirect_count, CmdDrawIndirectCountKHR);                             \
  HookInitExtension(KHR_draw_indirect_count, CmdDrawIndexedIndirectCountKHR);                      \
  HookInitExtension(EXT_validation_cache, CreateValidationCacheEXT);                               \
  HookInitExtension(EXT_validation_cache, DestroyValidationCacheEXT);                              \
  HookInitExtension(EXT_validation_cache, MergeValidationCachesEXT);                               \
  HookInitExtension(EXT_validation_cache, GetValidationCacheDataEXT);                              \
  HookInitExtension(KHR_shared_presentable_image, GetSwapchainStatusKHR);                          \
  HookInitExtension(KHR_create_renderpass2, CreateRenderPass2KHR);                                 \
  HookInitExtension(KHR_create_renderpass2, CmdBeginRenderPass2KHR);                               \
  HookInitExtension(KHR_create_renderpass2, CmdNextSubpass2KHR);                                   \
  HookInitExtension(KHR_create_renderpass2, CmdEndRenderPass2KHR);                                 \
  HookInitExtension(EXT_transform_feedback, CmdBindTransformFeedbackBuffersEXT);                   \
  HookInitExtension(EXT_transform_feedback, CmdBeginTransformFeedbackEXT);                         \
  HookInitExtension(EXT_transform_feedback, CmdEndTransformFeedbackEXT);                           \
  HookInitExtension(EXT_transform_feedback, CmdBeginQueryIndexedEXT);                              \
  HookInitExtension(EXT_transform_feedback, CmdEndQueryIndexedEXT);                                \
  HookInitExtension(EXT_transform_feedback, CmdDrawIndirectByteCountEXT);                          \
  HookInitExtension(EXT_conditional_rendering, CmdBeginConditionalRenderingEXT);                   \
  HookInitExtension(EXT_conditional_rendering, CmdEndConditionalRenderingEXT);                     \
  HookInitExtension(EXT_sample_locations, CmdSetSampleLocationsEXT);                               \
  HookInitExtension(EXT_discard_rectangles, CmdSetDiscardRectangleEXT);                            \
  HookInitExtension(EXT_calibrated_timestamps, GetCalibratedTimestampsEXT);                        \
  HookInitExtension(EXT_host_query_reset, ResetQueryPoolEXT);                                      \
  HookInitExtension(EXT_buffer_device_address, GetBufferDeviceAddressEXT);                         \
  HookInitExtension(EXT_hdr_metadata, SetHdrMetadataEXT);                                          \
  HookInitExtension(AMD_display_native_hdr, SetLocalDimmingAMD);                                   \
  HookInitDevice_PlatformSpecific()

#define DefineHooks()                                                                                \
  HookDefine3(VkResult, vkEnumeratePhysicalDevices, VkInstance, instance, uint32_t *,                \
              pPhysicalDeviceCount, VkPhysicalDevice *, pPhysicalDevices);                           \
  HookDefine2(void, vkGetPhysicalDeviceFeatures, VkPhysicalDevice, physicalDevice,                   \
              VkPhysicalDeviceFeatures *, pFeatures);                                                \
  HookDefine3(void, vkGetPhysicalDeviceFormatProperties, VkPhysicalDevice, physicalDevice,           \
              VkFormat, format, VkFormatProperties *, pFormatProperties);                            \
  HookDefine7(VkResult, vkGetPhysicalDeviceImageFormatProperties, VkPhysicalDevice, physicalDevice,  \
              VkFormat, format, VkImageType, type, VkImageTiling, tiling, VkImageUsageFlags,         \
              usage, VkImageCreateFlags, flags, VkImageFormatProperties *, pImageFormatProperties);  \
  HookDefine8(void, vkGetPhysicalDeviceSparseImageFormatProperties, VkPhysicalDevice,                \
              physicalDevice, VkFormat, format, VkImageType, type, VkSampleCountFlagBits, samples,   \
              VkImageUsageFlags, usage, VkImageTiling, tiling, uint32_t *, pNumProperties,           \
              VkSparseImageFormatProperties *, pProperties);                                         \
  HookDefine2(void, vkGetPhysicalDeviceProperties, VkPhysicalDevice, physicalDevice,                 \
              VkPhysicalDeviceProperties *, pProperties);                                            \
  HookDefine3(void, vkGetPhysicalDeviceQueueFamilyProperties, VkPhysicalDevice, physicalDevice,      \
              uint32_t *, pCount, VkQueueFamilyProperties *, pQueueFamilyProperties);                \
  HookDefine2(void, vkGetPhysicalDeviceMemoryProperties, VkPhysicalDevice, physicalDevice,           \
              VkPhysicalDeviceMemoryProperties *, pMemoryProperties);                                \
  HookDefine4(VkResult, vkCreateDevice, VkPhysicalDevice, physicalDevice,                            \
              const VkDeviceCreateInfo *, pCreateInfo, const VkAllocationCallbacks *, pAllocator,    \
              VkDevice *, pDevice);                                                                  \
  HookDefine2(void, vkDestroyDevice, VkDevice, device, const VkAllocationCallbacks *, pAllocator);   \
  HookDefine4(void, vkGetDeviceQueue, VkDevice, device, uint32_t, queueFamilyIndex, uint32_t,        \
              queueIndex, VkQueue *, pQueue);                                                        \
  HookDefine4(VkResult, vkQueueSubmit, VkQueue, queue, uint32_t, submitCount,                        \
              const VkSubmitInfo *, pSubmits, VkFence, fence);                                       \
  HookDefine1(VkResult, vkQueueWaitIdle, VkQueue, queue);                                            \
  HookDefine1(VkResult, vkDeviceWaitIdle, VkDevice, device);                                         \
  HookDefine4(VkResult, vkAllocateMemory, VkDevice, device, const VkMemoryAllocateInfo *,            \
              pAllocInfo, const VkAllocationCallbacks *, pAllocator, VkDeviceMemory *, pMemory);     \
  HookDefine3(void, vkFreeMemory, VkDevice, device, VkDeviceMemory, mem,                             \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine6(VkResult, vkMapMemory, VkDevice, device, VkDeviceMemory, mem, VkDeviceSize, offset,    \
              VkDeviceSize, size, VkMemoryMapFlags, flags, void **, ppData);                         \
  HookDefine2(void, vkUnmapMemory, VkDevice, device, VkDeviceMemory, mem);                           \
  HookDefine3(VkResult, vkFlushMappedMemoryRanges, VkDevice, device, uint32_t, memRangeCount,        \
              const VkMappedMemoryRange *, pMemRanges);                                              \
  HookDefine3(VkResult, vkInvalidateMappedMemoryRanges, VkDevice, device, uint32_t, memRangeCount,   \
              const VkMappedMemoryRange *, pMemRanges);                                              \
  HookDefine3(void, vkGetDeviceMemoryCommitment, VkDevice, device, VkDeviceMemory, memory,           \
              VkDeviceSize *, pCommittedMemoryInBytes);                                              \
  HookDefine4(VkResult, vkBindBufferMemory, VkDevice, device, VkBuffer, buffer, VkDeviceMemory,      \
              mem, VkDeviceSize, memOffset);                                                         \
  HookDefine4(VkResult, vkBindImageMemory, VkDevice, device, VkImage, image, VkDeviceMemory, mem,    \
              VkDeviceSize, memOffset);                                                              \
  HookDefine4(VkResult, vkQueueBindSparse, VkQueue, queue, uint32_t, bindInfoCount,                  \
              const VkBindSparseInfo *, pBindInfo, VkFence, fence);                                  \
  HookDefine4(VkResult, vkCreateBuffer, VkDevice, device, const VkBufferCreateInfo *, pCreateInfo,   \
              const VkAllocationCallbacks *, pAllocator, VkBuffer *, pBuffer);                       \
  HookDefine3(void, vkDestroyBuffer, VkDevice, device, VkBuffer, buffer,                             \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateBufferView, VkDevice, device, const VkBufferViewCreateInfo *,        \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkBufferView *, pView);        \
  HookDefine3(void, vkDestroyBufferView, VkDevice, device, VkBufferView, bufferView,                 \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateImage, VkDevice, device, const VkImageCreateInfo *, pCreateInfo,     \
              const VkAllocationCallbacks *, pAllocator, VkImage *, pImage);                         \
  HookDefine3(void, vkDestroyImage, VkDevice, device, VkImage, image,                                \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(void, vkGetImageSubresourceLayout, VkDevice, device, VkImage, image,                   \
              const VkImageSubresource *, pSubresource, VkSubresourceLayout *, pLayout);             \
  HookDefine3(void, vkGetBufferMemoryRequirements, VkDevice, device, VkBuffer, buffer,               \
              VkMemoryRequirements *, pMemoryRequirements);                                          \
  HookDefine3(void, vkGetImageMemoryRequirements, VkDevice, device, VkImage, image,                  \
              VkMemoryRequirements *, pMemoryRequirements);                                          \
  HookDefine4(void, vkGetImageSparseMemoryRequirements, VkDevice, device, VkImage, image,            \
              uint32_t *, pNumRequirements, VkSparseImageMemoryRequirements *,                       \
              pSparseMemoryRequirements);                                                            \
  HookDefine4(VkResult, vkCreateImageView, VkDevice, device, const VkImageViewCreateInfo *,          \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkImageView *, pView);         \
  HookDefine3(void, vkDestroyImageView, VkDevice, device, VkImageView, imageView,                    \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateShaderModule, VkDevice, device, const VkShaderModuleCreateInfo *,    \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkShaderModule *,              \
              pShaderModule);                                                                        \
  HookDefine3(void, vkDestroyShaderModule, VkDevice, device, VkShaderModule, shaderModule,           \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine6(VkResult, vkCreateGraphicsPipelines, VkDevice, device, VkPipelineCache,                \
              pipelineCache, uint32_t, count, const VkGraphicsPipelineCreateInfo *, pCreateInfos,    \
              const VkAllocationCallbacks *, pAllocator, VkPipeline *, pPipelines);                  \
  HookDefine6(VkResult, vkCreateComputePipelines, VkDevice, device, VkPipelineCache,                 \
              pipelineCache, uint32_t, count, const VkComputePipelineCreateInfo *, pCreateInfos,     \
              const VkAllocationCallbacks *, pAllocator, VkPipeline *, pPipelines);                  \
  HookDefine3(void, vkDestroyPipeline, VkDevice, device, VkPipeline, pipeline,                       \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreatePipelineCache, VkDevice, device,                                     \
              const VkPipelineCacheCreateInfo *, pCreateInfo, const VkAllocationCallbacks *,         \
              pAllocator, VkPipelineCache *, pPipelineCache);                                        \
  HookDefine3(void, vkDestroyPipelineCache, VkDevice, device, VkPipelineCache, pipelineCache,        \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkGetPipelineCacheData, VkDevice, device, VkPipelineCache, pipelineCache,    \
              size_t *, pDataSize, void *, pData);                                                   \
  HookDefine4(VkResult, vkMergePipelineCaches, VkDevice, device, VkPipelineCache, pipelineCache,     \
              uint32_t, srcCacheCount, const VkPipelineCache *, pSrcCaches);                         \
  HookDefine4(VkResult, vkCreatePipelineLayout, VkDevice, device,                                    \
              const VkPipelineLayoutCreateInfo *, pCreateInfo, const VkAllocationCallbacks *,        \
              pAllocator, VkPipelineLayout *, pPipelineLayout);                                      \
  HookDefine3(void, vkDestroyPipelineLayout, VkDevice, device, VkPipelineLayout, pipelineLayout,     \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateSemaphore, VkDevice, device, const VkSemaphoreCreateInfo *,          \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkSemaphore *, pSemaphore);    \
  HookDefine3(void, vkDestroySemaphore, VkDevice, device, VkSemaphore, semaphore,                    \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateFence, VkDevice, device, const VkFenceCreateInfo *, pCreateInfo,     \
              const VkAllocationCallbacks *, pAllocator, VkFence *, pFence);                         \
  HookDefine3(void, vkDestroyFence, VkDevice, device, VkFence, fence,                                \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateEvent, VkDevice, device, const VkEventCreateInfo *, pCreateInfo,     \
              const VkAllocationCallbacks *, pAllocator, VkEvent *, pEvent);                         \
  HookDefine3(void, vkDestroyEvent, VkDevice, device, VkEvent, event,                                \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine2(VkResult, vkGetEventStatus, VkDevice, device, VkEvent, event);                         \
  HookDefine2(VkResult, vkSetEvent, VkDevice, device, VkEvent, event);                               \
  HookDefine2(VkResult, vkResetEvent, VkDevice, device, VkEvent, event);                             \
  HookDefine4(VkResult, vkCreateQueryPool, VkDevice, device, const VkQueryPoolCreateInfo *,          \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkQueryPool *, pQueryPool);    \
  HookDefine3(void, vkDestroyQueryPool, VkDevice, device, VkQueryPool, queryPool,                    \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine8(VkResult, vkGetQueryPoolResults, VkDevice, device, VkQueryPool, queryPool, uint32_t,   \
              firstQuery, uint32_t, queryCount, size_t, dataSize, void *, pData, VkDeviceSize,       \
              stride, VkQueryResultFlags, flags);                                                    \
  HookDefine2(VkResult, vkGetFenceStatus, VkDevice, device, VkFence, fence);                         \
  HookDefine3(VkResult, vkResetFences, VkDevice, device, uint32_t, fenceCount, const VkFence *,      \
              pFences);                                                                              \
  HookDefine5(VkResult, vkWaitForFences, VkDevice, device, uint32_t, fenceCount, const VkFence *,    \
              pFences, VkBool32, waitAll, uint64_t, timeout);                                        \
  HookDefine4(VkResult, vkCreateSampler, VkDevice, device, const VkSamplerCreateInfo *,              \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkSampler *, pSampler);        \
  HookDefine3(void, vkDestroySampler, VkDevice, device, VkSampler, sampler,                          \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateDescriptorSetLayout, VkDevice, device,                               \
              const VkDescriptorSetLayoutCreateInfo *, pCreateInfo, const VkAllocationCallbacks *,   \
              pAllocator, VkDescriptorSetLayout *, pSetLayout);                                      \
  HookDefine3(void, vkDestroyDescriptorSetLayout, VkDevice, device, VkDescriptorSetLayout,           \
              descriptorSetLayout, const VkAllocationCallbacks *, pAllocator);                       \
  HookDefine4(VkResult, vkCreateDescriptorPool, VkDevice, device,                                    \
              const VkDescriptorPoolCreateInfo *, pCreateInfo, const VkAllocationCallbacks *,        \
              pAllocator, VkDescriptorPool *, pDescriptorPool);                                      \
  HookDefine3(void, vkDestroyDescriptorPool, VkDevice, device, VkDescriptorPool, descriptorPool,     \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine3(VkResult, vkResetDescriptorPool, VkDevice, device, VkDescriptorPool, descriptorPool,   \
              VkDescriptorPoolResetFlags, flags);                                                    \
  HookDefine3(VkResult, vkAllocateDescriptorSets, VkDevice, device,                                  \
              const VkDescriptorSetAllocateInfo *, pAllocateInfo, VkDescriptorSet *,                 \
              pDescriptorSets);                                                                      \
  HookDefine5(void, vkUpdateDescriptorSets, VkDevice, device, uint32_t, writeCount,                  \
              const VkWriteDescriptorSet *, pDescriptorWrites, uint32_t, copyCount,                  \
              const VkCopyDescriptorSet *, pDescriptorCopies);                                       \
  HookDefine4(VkResult, vkFreeDescriptorSets, VkDevice, device, VkDescriptorPool, descriptorPool,    \
              uint32_t, count, const VkDescriptorSet *, pDescriptorSets);                            \
  HookDefine4(VkResult, vkCreateCommandPool, VkDevice, device, const VkCommandPoolCreateInfo *,      \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkCommandPool *,               \
              pCommandPool);                                                                         \
  HookDefine3(void, vkDestroyCommandPool, VkDevice, device, VkCommandPool, commandPool,              \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine3(VkResult, vkResetCommandPool, VkDevice, device, VkCommandPool, commandPool,            \
              VkCommandPoolResetFlags, flags);                                                       \
  HookDefine3(VkResult, vkAllocateCommandBuffers, VkDevice, device,                                  \
              const VkCommandBufferAllocateInfo *, pCreateInfo, VkCommandBuffer *, pCommandBuffers); \
  HookDefine4(void, vkFreeCommandBuffers, VkDevice, device, VkCommandPool, commandPool, uint32_t,    \
              commandBufferCount, const VkCommandBuffer *, pCommandBuffers);                         \
  HookDefine2(VkResult, vkBeginCommandBuffer, VkCommandBuffer, commandBuffer,                        \
              const VkCommandBufferBeginInfo *, pBeginInfo);                                         \
  HookDefine1(VkResult, vkEndCommandBuffer, VkCommandBuffer, commandBuffer);                         \
  HookDefine2(VkResult, vkResetCommandBuffer, VkCommandBuffer, commandBuffer,                        \
              VkCommandBufferResetFlags, flags);                                                     \
  HookDefine3(void, vkCmdBindPipeline, VkCommandBuffer, commandBuffer, VkPipelineBindPoint,          \
              pipelineBindPoint, VkPipeline, pipeline);                                              \
  HookDefine4(void, vkCmdSetViewport, VkCommandBuffer, commandBuffer, uint32_t, firstViewport,       \
              uint32_t, viewportCount, const VkViewport *, pViewports);                              \
  HookDefine4(void, vkCmdSetScissor, VkCommandBuffer, commandBuffer, uint32_t, firstScissor,         \
              uint32_t, scissorCount, const VkRect2D *, pScissors);                                  \
  HookDefine2(void, vkCmdSetLineWidth, VkCommandBuffer, commandBuffer, float, lineWidth);            \
  HookDefine4(void, vkCmdSetDepthBias, VkCommandBuffer, commandBuffer, float, depthBias, float,      \
              depthBiasClamp, float, slopeScaledDepthBias);                                          \
  HookDefine2(void, vkCmdSetBlendConstants, VkCommandBuffer, commandBuffer, const float *,           \
              blendConst);                                                                           \
  HookDefine3(void, vkCmdSetDepthBounds, VkCommandBuffer, commandBuffer, float, minDepthBounds,      \
              float, maxDepthBounds);                                                                \
  HookDefine3(void, vkCmdSetStencilCompareMask, VkCommandBuffer, commandBuffer,                      \
              VkStencilFaceFlags, faceMask, uint32_t, compareMask);                                  \
  HookDefine3(void, vkCmdSetStencilWriteMask, VkCommandBuffer, commandBuffer, VkStencilFaceFlags,    \
              faceMask, uint32_t, writeMask);                                                        \
  HookDefine3(void, vkCmdSetStencilReference, VkCommandBuffer, commandBuffer, VkStencilFaceFlags,    \
              faceMask, uint32_t, reference);                                                        \
  HookDefine8(void, vkCmdBindDescriptorSets, VkCommandBuffer, commandBuffer, VkPipelineBindPoint,    \
              pipelineBindPoint, VkPipelineLayout, layout, uint32_t, firstSet, uint32_t, setCount,   \
              const VkDescriptorSet *, pDescriptorSets, uint32_t, dynamicOffsetCount,                \
              const uint32_t *, pDynamicOffsets);                                                    \
  HookDefine4(void, vkCmdBindIndexBuffer, VkCommandBuffer, commandBuffer, VkBuffer, buffer,          \
              VkDeviceSize, offset, VkIndexType, indexType);                                         \
  HookDefine5(void, vkCmdBindVertexBuffers, VkCommandBuffer, commandBuffer, uint32_t, firstBinding,  \
              uint32_t, bindingCount, const VkBuffer *, pBuffers, const VkDeviceSize *, pOffsets);   \
  HookDefine5(void, vkCmdDraw, VkCommandBuffer, commandBuffer, uint32_t, vertexCount, uint32_t,      \
              instanceCount, uint32_t, firstVertex, uint32_t, firstInstance);                        \
  HookDefine6(void, vkCmdDrawIndexed, VkCommandBuffer, commandBuffer, uint32_t, indexCount,          \
              uint32_t, instanceCount, uint32_t, firstIndex, int32_t, vertexOffset, uint32_t,        \
              firstInstance);                                                                        \
  HookDefine5(void, vkCmdDrawIndirect, VkCommandBuffer, commandBuffer, VkBuffer, buffer,             \
              VkDeviceSize, offset, uint32_t, count, uint32_t, stride);                              \
  HookDefine5(void, vkCmdDrawIndexedIndirect, VkCommandBuffer, commandBuffer, VkBuffer, buffer,      \
              VkDeviceSize, offset, uint32_t, count, uint32_t, stride);                              \
  HookDefine4(void, vkCmdDispatch, VkCommandBuffer, commandBuffer, uint32_t, x, uint32_t, y,         \
              uint32_t, z);                                                                          \
  HookDefine3(void, vkCmdDispatchIndirect, VkCommandBuffer, commandBuffer, VkBuffer, buffer,         \
              VkDeviceSize, offset);                                                                 \
  HookDefine6(void, vkCmdCopyBufferToImage, VkCommandBuffer, commandBuffer, VkBuffer, srcBuffer,     \
              VkImage, destImage, VkImageLayout, destImageLayout, uint32_t, regionCount,             \
              const VkBufferImageCopy *, pRegions);                                                  \
  HookDefine6(void, vkCmdCopyImageToBuffer, VkCommandBuffer, commandBuffer, VkImage, srcImage,       \
              VkImageLayout, srcImageLayout, VkBuffer, destBuffer, uint32_t, regionCount,            \
              const VkBufferImageCopy *, pRegions);                                                  \
  HookDefine5(void, vkCmdCopyBuffer, VkCommandBuffer, commandBuffer, VkBuffer, srcBuffer,            \
              VkBuffer, destBuffer, uint32_t, regionCount, const VkBufferCopy *, pRegions);          \
  HookDefine7(void, vkCmdCopyImage, VkCommandBuffer, commandBuffer, VkImage, srcImage,               \
              VkImageLayout, srcImageLayout, VkImage, destImage, VkImageLayout, destImageLayout,     \
              uint32_t, regionCount, const VkImageCopy *, pRegions);                                 \
  HookDefine8(void, vkCmdBlitImage, VkCommandBuffer, commandBuffer, VkImage, srcImage,               \
              VkImageLayout, srcImageLayout, VkImage, destImage, VkImageLayout, destImageLayout,     \
              uint32_t, regionCount, const VkImageBlit *, pRegions, VkFilter, filter);               \
  HookDefine7(void, vkCmdResolveImage, VkCommandBuffer, commandBuffer, VkImage, srcImage,            \
              VkImageLayout, srcImageLayout, VkImage, destImage, VkImageLayout, destImageLayout,     \
              uint32_t, regionCount, const VkImageResolve *, pRegions);                              \
  HookDefine5(void, vkCmdUpdateBuffer, VkCommandBuffer, commandBuffer, VkBuffer, destBuffer,         \
              VkDeviceSize, destOffset, VkDeviceSize, dataSize, const uint32_t *, pData);            \
  HookDefine5(void, vkCmdFillBuffer, VkCommandBuffer, commandBuffer, VkBuffer, destBuffer,           \
              VkDeviceSize, destOffset, VkDeviceSize, fillSize, uint32_t, data);                     \
  HookDefine6(void, vkCmdPushConstants, VkCommandBuffer, commandBuffer, VkPipelineLayout, layout,    \
              VkShaderStageFlags, stageFlags, uint32_t, start, uint32_t, length, const void *,       \
              values);                                                                               \
  HookDefine6(void, vkCmdClearColorImage, VkCommandBuffer, commandBuffer, VkImage, image,            \
              VkImageLayout, imageLayout, const VkClearColorValue *, pColor, uint32_t, rangeCount,   \
              const VkImageSubresourceRange *, pRanges);                                             \
  HookDefine6(void, vkCmdClearDepthStencilImage, VkCommandBuffer, commandBuffer, VkImage, image,     \
              VkImageLayout, imageLayout, const VkClearDepthStencilValue *, pDepthStencil,           \
              uint32_t, rangeCount, const VkImageSubresourceRange *, pRanges);                       \
  HookDefine5(void, vkCmdClearAttachments, VkCommandBuffer, commandBuffer, uint32_t,                 \
              attachmentCount, const VkClearAttachment *, pAttachments, uint32_t, rectCount,         \
              const VkClearRect *, pRects);                                                          \
  HookDefine10(void, vkCmdPipelineBarrier, VkCommandBuffer, commandBuffer, VkPipelineStageFlags,     \
               srcStageMask, VkPipelineStageFlags, destStageMask, VkDependencyFlags,                 \
               dependencyFlags, uint32_t, memoryBarrierCount, const VkMemoryBarrier *,               \
               pMemoryBarriers, uint32_t, bufferMemoryBarrierCount, const VkBufferMemoryBarrier *,   \
               pBufferMemoryBarriers, uint32_t, imageMemoryBarrierCount,                             \
               const VkImageMemoryBarrier *, pImageMemoryBarriers);                                  \
  HookDefine4(void, vkCmdWriteTimestamp, VkCommandBuffer, commandBuffer, VkPipelineStageFlagBits,    \
              pipelineStage, VkQueryPool, queryPool, uint32_t, query);                               \
  HookDefine8(void, vkCmdCopyQueryPoolResults, VkCommandBuffer, commandBuffer, VkQueryPool,          \
              queryPool, uint32_t, firstQuery, uint32_t, queryCount, VkBuffer, dstBuffer,            \
              VkDeviceSize, dstOffset, VkDeviceSize, stride, VkQueryResultFlags, flags);             \
  HookDefine4(void, vkCmdBeginQuery, VkCommandBuffer, commandBuffer, VkQueryPool, queryPool,         \
              uint32_t, query, VkQueryControlFlags, flags);                                          \
  HookDefine3(void, vkCmdEndQuery, VkCommandBuffer, commandBuffer, VkQueryPool, queryPool,           \
              uint32_t, query);                                                                      \
  HookDefine4(void, vkCmdResetQueryPool, VkCommandBuffer, commandBuffer, VkQueryPool, queryPool,     \
              uint32_t, firstQuery, uint32_t, queryCount);                                           \
  HookDefine3(void, vkCmdSetEvent, VkCommandBuffer, commandBuffer, VkEvent, event,                   \
              VkPipelineStageFlags, stageMask);                                                      \
  HookDefine3(void, vkCmdResetEvent, VkCommandBuffer, commandBuffer, VkEvent, event,                 \
              VkPipelineStageFlags, stageMask);                                                      \
  HookDefine11(void, vkCmdWaitEvents, VkCommandBuffer, commandBuffer, uint32_t, eventCount,          \
               const VkEvent *, pEvents, VkPipelineStageFlags, srcStageMask, VkPipelineStageFlags,   \
               dstStageMask, uint32_t, memoryBarrierCount, const VkMemoryBarrier *,                  \
               pMemoryBarriers, uint32_t, bufferMemoryBarrierCount, const VkBufferMemoryBarrier *,   \
               pBufferMemoryBarriers, uint32_t, imageMemoryBarrierCount,                             \
               const VkImageMemoryBarrier *, pImageMemoryBarriers);                                  \
  HookDefine4(VkResult, vkCreateFramebuffer, VkDevice, device, const VkFramebufferCreateInfo *,      \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkFramebuffer *,               \
              pFramebuffer);                                                                         \
  HookDefine3(void, vkDestroyFramebuffer, VkDevice, device, VkFramebuffer, framebuffer,              \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkCreateRenderPass, VkDevice, device, const VkRenderPassCreateInfo *,        \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkRenderPass *, pRenderPass);  \
  HookDefine3(void, vkDestroyRenderPass, VkDevice, device, VkRenderPass, renderPass,                 \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine3(void, vkGetRenderAreaGranularity, VkDevice, device, VkRenderPass, renderPass,          \
              VkExtent2D *, pGranularity);                                                           \
  HookDefine3(void, vkCmdBeginRenderPass, VkCommandBuffer, commandBuffer,                            \
              const VkRenderPassBeginInfo *, pRenderPassBegin, VkSubpassContents, contents);         \
  HookDefine2(void, vkCmdNextSubpass, VkCommandBuffer, commandBuffer, VkSubpassContents, contents);  \
  HookDefine3(void, vkCmdExecuteCommands, VkCommandBuffer, commandBuffer, uint32_t,                  \
              commandBufferCount, const VkCommandBuffer *, pCommandBuffers);                         \
  HookDefine1(void, vkCmdEndRenderPass, VkCommandBuffer, commandBuffer);                             \
  HookDefine4(VkResult, vkCreateDebugReportCallbackEXT, VkInstance, instance,                        \
              const VkDebugReportCallbackCreateInfoEXT *, pCreateInfo,                               \
              const VkAllocationCallbacks *, pAllocator, VkDebugReportCallbackEXT *, pCallback);     \
  HookDefine3(void, vkDestroyDebugReportCallbackEXT, VkInstance, instance,                           \
              VkDebugReportCallbackEXT, callback, const VkAllocationCallbacks *, pAllocator);        \
  HookDefine8(void, vkDebugReportMessageEXT, VkInstance, instance, VkDebugReportFlagsEXT, flags,     \
              VkDebugReportObjectTypeEXT, objectType, uint64_t, object, size_t, location, int32_t,   \
              messageCode, const char *, pLayerPrefix, const char *, pMessage);                      \
  HookDefine2(VkResult, vkDebugMarkerSetObjectTagEXT, VkDevice, device,                              \
              VkDebugMarkerObjectTagInfoEXT *, pTagInfo);                                            \
  HookDefine2(VkResult, vkDebugMarkerSetObjectNameEXT, VkDevice, device,                             \
              VkDebugMarkerObjectNameInfoEXT *, pNameInfo);                                          \
  HookDefine2(void, vkCmdDebugMarkerBeginEXT, VkCommandBuffer, commandBuffer,                        \
              VkDebugMarkerMarkerInfoEXT *, pMarkerInfo);                                            \
  HookDefine1(void, vkCmdDebugMarkerEndEXT, VkCommandBuffer, commandBuffer);                         \
  HookDefine2(void, vkCmdDebugMarkerInsertEXT, VkCommandBuffer, commandBuffer,                       \
              VkDebugMarkerMarkerInfoEXT *, pMarkerInfo);                                            \
  HookDefine4(VkResult, vkGetPhysicalDeviceSurfaceSupportKHR, VkPhysicalDevice, physicalDevice,      \
              uint32_t, queueFamilyIndex, VkSurfaceKHR, surface, VkBool32 *, pSupported);            \
  HookDefine3(VkResult, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, VkPhysicalDevice, physicalDevice, \
              VkSurfaceKHR, surface, VkSurfaceCapabilitiesKHR *, pSurfaceProperties);                \
  HookDefine4(VkResult, vkGetPhysicalDeviceSurfaceFormatsKHR, VkPhysicalDevice, physicalDevice,      \
              VkSurfaceKHR, surface, uint32_t *, pSurfaceFormatCount, VkSurfaceFormatKHR *,          \
              pSurfaceFormats);                                                                      \
  HookDefine4(VkResult, vkGetPhysicalDeviceSurfacePresentModesKHR, VkPhysicalDevice,                 \
              physicalDevice, VkSurfaceKHR, surface, uint32_t *, pPresentModeCount,                  \
              VkPresentModeKHR *, pPresentModes);                                                    \
  HookDefine4(VkResult, vkCreateSwapchainKHR, VkDevice, device, const VkSwapchainCreateInfoKHR *,    \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkSwapchainKHR *, pSwapchain); \
  HookDefine3(void, vkDestroySwapchainKHR, VkDevice, device, VkSwapchainKHR, swapchain,              \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkGetSwapchainImagesKHR, VkDevice, device, VkSwapchainKHR, swapchain,        \
              uint32_t *, pCount, VkImage *, pSwapchainImages);                                      \
  HookDefine6(VkResult, vkAcquireNextImageKHR, VkDevice, device, VkSwapchainKHR, swapchain,          \
              uint64_t, timeout, VkSemaphore, semaphore, VkFence, fence, uint32_t *, pImageIndex);   \
  HookDefine2(VkResult, vkQueuePresentKHR, VkQueue, queue, VkPresentInfoKHR *, pPresentInfo);        \
  HookDefine3(void, vkDestroySurfaceKHR, VkInstance, instance, VkSurfaceKHR, surface,                \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine3(VkResult, vkGetPhysicalDeviceDisplayPropertiesKHR, VkPhysicalDevice, physicalDevice,   \
              uint32_t *, pPropertyCount, VkDisplayPropertiesKHR *, pProperties);                    \
  HookDefine3(VkResult, vkGetPhysicalDeviceDisplayPlanePropertiesKHR, VkPhysicalDevice,              \
              physicalDevice, uint32_t *, pPropertyCount, VkDisplayPlanePropertiesKHR *,             \
              pProperties);                                                                          \
  HookDefine4(VkResult, vkGetDisplayPlaneSupportedDisplaysKHR, VkPhysicalDevice, physicalDevice,     \
              uint32_t, planeIndex, uint32_t *, pDisplayCount, VkDisplayKHR *, pDisplays);           \
  HookDefine4(VkResult, vkGetDisplayModePropertiesKHR, VkPhysicalDevice, physicalDevice,             \
              VkDisplayKHR, display, uint32_t *, pPropertyCount, VkDisplayModePropertiesKHR *,       \
              pProperties);                                                                          \
  HookDefine5(VkResult, vkCreateDisplayModeKHR, VkPhysicalDevice, physicalDevice, VkDisplayKHR,      \
              display, const VkDisplayModeCreateInfoKHR *, pCreateInfo,                              \
              const VkAllocationCallbacks *, pAllocator, VkDisplayModeKHR *, pMode);                 \
  HookDefine4(VkResult, vkGetDisplayPlaneCapabilitiesKHR, VkPhysicalDevice, physicalDevice,          \
              VkDisplayModeKHR, mode, uint32_t, planeIndex, VkDisplayPlaneCapabilitiesKHR *,         \
              pCapabilities);                                                                        \
  HookDefine4(VkResult, vkCreateDisplayPlaneSurfaceKHR, VkInstance, instance,                        \
              const VkDisplaySurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *,     \
              pAllocator, VkSurfaceKHR *, pSurface);                                                 \
  HookDefine5(VkResult, vkCreateSharedSwapchainsKHR, VkDevice, device, uint32_t, swapchainCount,     \
              const VkSwapchainCreateInfoKHR *, pCreateInfos, const VkAllocationCallbacks *,         \
              pAllocator, VkSwapchainKHR *, pSwapchains);                                            \
  HookDefine8(VkResult, vkGetPhysicalDeviceExternalImageFormatPropertiesNV, VkPhysicalDevice,        \
              physicalDevice, VkFormat, format, VkImageType, type, VkImageTiling, tiling,            \
              VkImageUsageFlags, usage, VkImageCreateFlags, flags,                                   \
              VkExternalMemoryHandleTypeFlagsNV, externalHandleType,                                 \
              VkExternalImageFormatPropertiesNV *, pExternalImageFormatProperties);                  \
  HookDefine3(void, vkTrimCommandPool, VkDevice, device, VkCommandPool, commandPool,                 \
              VkCommandPoolTrimFlags, flags);                                                        \
  HookDefine2(void, vkGetPhysicalDeviceFeatures2, VkPhysicalDevice, physicalDevice,                  \
              VkPhysicalDeviceFeatures2 *, pFeatures);                                               \
  HookDefine2(void, vkGetPhysicalDeviceProperties2, VkPhysicalDevice, physicalDevice,                \
              VkPhysicalDeviceProperties2 *, pProperties);                                           \
  HookDefine3(void, vkGetPhysicalDeviceFormatProperties2, VkPhysicalDevice, physicalDevice,          \
              VkFormat, format, VkFormatProperties2 *, pFormatProperties);                           \
  HookDefine3(VkResult, vkGetPhysicalDeviceImageFormatProperties2, VkPhysicalDevice,                 \
              physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *, pImageFormatInfo,            \
              VkImageFormatProperties2 *, pImageFormatProperties);                                   \
  HookDefine3(void, vkGetPhysicalDeviceQueueFamilyProperties2, VkPhysicalDevice, physicalDevice,     \
              uint32_t *, pCount, VkQueueFamilyProperties2 *, pQueueFamilyProperties);               \
  HookDefine2(void, vkGetPhysicalDeviceMemoryProperties2, VkPhysicalDevice, physicalDevice,          \
              VkPhysicalDeviceMemoryProperties2 *, pMemoryProperties);                               \
  HookDefine4(void, vkGetPhysicalDeviceSparseImageFormatProperties2, VkPhysicalDevice,               \
              physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *, pFormatInfo,           \
              uint32_t *, pPropertyCount, VkSparseImageFormatProperties2 *, pProperties);            \
  HookDefine3(VkResult, vkGetPhysicalDeviceSurfaceCapabilities2EXT, VkPhysicalDevice,                \
              physicalDevice, VkSurfaceKHR, surface, VkSurfaceCapabilities2EXT *,                    \
              pSurfaceCapabilities);                                                                 \
  HookDefine3(VkResult, vkDisplayPowerControlEXT, VkDevice, device, VkDisplayKHR, display,           \
              const VkDisplayPowerInfoEXT *, pDisplayPowerInfo);                                     \
  HookDefine4(VkResult, vkRegisterDeviceEventEXT, VkDevice, device, const VkDeviceEventInfoEXT *,    \
              pDeviceEventInfo, const VkAllocationCallbacks *, pAllocator, VkFence *, pFence);       \
  HookDefine5(VkResult, vkRegisterDisplayEventEXT, VkDevice, device, VkDisplayKHR, display,          \
              const VkDisplayEventInfoEXT *, pDisplayEventInfo, const VkAllocationCallbacks *,       \
              pAllocator, VkFence *, pFence);                                                        \
  HookDefine4(VkResult, vkGetSwapchainCounterEXT, VkDevice, device, VkSwapchainKHR, swapchain,       \
              VkSurfaceCounterFlagBitsEXT, counter, uint64_t *, pCounterValue);                      \
  HookDefine2(VkResult, vkReleaseDisplayEXT, VkPhysicalDevice, physicalDevice, VkDisplayKHR,         \
              display);                                                                              \
  HookDefine3(void, vkGetPhysicalDeviceExternalBufferProperties, VkPhysicalDevice, physicalDevice,   \
              const VkPhysicalDeviceExternalBufferInfo *, pExternalBufferInfo,                       \
              VkExternalBufferProperties *, pExternalBufferProperties);                              \
  HookDefine3(VkResult, vkGetMemoryFdKHR, VkDevice, device, const VkMemoryGetFdInfoKHR *,            \
              pGetFdInfo, int *, pFd);                                                               \
  HookDefine4(VkResult, vkGetMemoryFdPropertiesKHR, VkDevice, device,                                \
              VkExternalMemoryHandleTypeFlagBits, handleType, int, fd, VkMemoryFdPropertiesKHR *,    \
              pMemoryFdProperties);                                                                  \
  HookDefine3(void, vkGetPhysicalDeviceExternalSemaphoreProperties, VkPhysicalDevice,                \
              physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *, pExternalSemaphoreInfo, \
              VkExternalSemaphoreProperties *, pExternalSemaphoreProperties);                        \
  HookDefine2(VkResult, vkImportSemaphoreFdKHR, VkDevice, device,                                    \
              const VkImportSemaphoreFdInfoKHR *, pImportSemaphoreFdInfo);                           \
  HookDefine3(VkResult, vkGetSemaphoreFdKHR, VkDevice, device, const VkSemaphoreGetFdInfoKHR *,      \
              pGetFdInfo, int *, pFd);                                                               \
  HookDefine3(void, vkGetPhysicalDeviceExternalFenceProperties, VkPhysicalDevice, physicalDevice,    \
              const VkPhysicalDeviceExternalFenceInfo *, pExternalFenceInfo,                         \
              VkExternalFenceProperties *, pExternalFenceProperties);                                \
  HookDefine2(VkResult, vkImportFenceFdKHR, VkDevice, device, const VkImportFenceFdInfoKHR *,        \
              pImportFenceFdInfo);                                                                   \
  HookDefine3(VkResult, vkGetFenceFdKHR, VkDevice, device, const VkFenceGetFdInfoKHR *,              \
              pGetFdInfo, int *, pFd);                                                               \
  HookDefine3(void, vkGetImageMemoryRequirements2, VkDevice, device,                                 \
              const VkImageMemoryRequirementsInfo2 *, pInfo, VkMemoryRequirements2 *,                \
              pMemoryRequirements);                                                                  \
  HookDefine3(void, vkGetBufferMemoryRequirements2, VkDevice, device,                                \
              const VkBufferMemoryRequirementsInfo2 *, pInfo, VkMemoryRequirements2 *,               \
              pMemoryRequirements);                                                                  \
  HookDefine4(void, vkGetImageSparseMemoryRequirements2, VkDevice, device,                           \
              const VkImageSparseMemoryRequirementsInfo2 *, pInfo, uint32_t *,                       \
              pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *,                     \
              pSparseMemoryRequirements);                                                            \
  HookDefine6(VkResult, vkGetShaderInfoAMD, VkDevice, device, VkPipeline, pipeline,                  \
              VkShaderStageFlagBits, shaderStage, VkShaderInfoTypeAMD, infoType, size_t *,           \
              pInfoSize, void *, pInfo);                                                             \
  HookDefine6(void, vkCmdPushDescriptorSetKHR, VkCommandBuffer, commandBuffer,                       \
              VkPipelineBindPoint, pipelineBindPoint, VkPipelineLayout, layout, uint32_t, set,       \
              uint32_t, descriptorWriteCount, const VkWriteDescriptorSet *, pDescriptorWrites);      \
  HookDefine4(VkResult, vkCreateDescriptorUpdateTemplate, VkDevice, device,                          \
              const VkDescriptorUpdateTemplateCreateInfo *, pCreateInfo,                             \
              const VkAllocationCallbacks *, pAllocator, VkDescriptorUpdateTemplate *,               \
              pDescriptorUpdateTemplate);                                                            \
  HookDefine3(void, vkDestroyDescriptorUpdateTemplate, VkDevice, device, VkDescriptorUpdateTemplate, \
              descriptorUpdateTemplate, const VkAllocationCallbacks *, pAllocator);                  \
  HookDefine4(void, vkUpdateDescriptorSetWithTemplate, VkDevice, device, VkDescriptorSet,            \
              descriptorSet, VkDescriptorUpdateTemplate, descriptorUpdateTemplate, const void *,     \
              pData);                                                                                \
  HookDefine5(void, vkCmdPushDescriptorSetWithTemplateKHR, VkCommandBuffer, commandBuffer,           \
              VkDescriptorUpdateTemplate, descriptorUpdateTemplate, VkPipelineLayout, layout,        \
              uint32_t, set, const void *, pData);                                                   \
  HookDefine3(VkResult, vkBindBufferMemory2, VkDevice, device, uint32_t, bindInfoCount,              \
              const VkBindBufferMemoryInfo *, pBindInfos);                                           \
  HookDefine3(VkResult, vkBindImageMemory2, VkDevice, device, uint32_t, bindInfoCount,               \
              const VkBindImageMemoryInfo *, pBindInfos);                                            \
  HookDefine3(void, vkGetDescriptorSetLayoutSupport, VkDevice, device,                               \
              const VkDescriptorSetLayoutCreateInfo *, pCreateInfo,                                  \
              VkDescriptorSetLayoutSupport *, pSupport);                                             \
  HookDefine5(void, vkCmdWriteBufferMarkerAMD, VkCommandBuffer, commandBuffer,                       \
              VkPipelineStageFlagBits, pipelineStage, VkBuffer, dstBuffer, VkDeviceSize,             \
              dstOffset, uint32_t, marker);                                                          \
  HookDefine2(VkResult, vkSetDebugUtilsObjectNameEXT, VkDevice, device,                              \
              const VkDebugUtilsObjectNameInfoEXT *, pNameInfo);                                     \
  HookDefine2(VkResult, vkSetDebugUtilsObjectTagEXT, VkDevice, device,                               \
              const VkDebugUtilsObjectTagInfoEXT *, pTagInfo);                                       \
  HookDefine2(void, vkQueueBeginDebugUtilsLabelEXT, VkQueue, queue, const VkDebugUtilsLabelEXT *,    \
              pLabelInfo);                                                                           \
  HookDefine1(void, vkQueueEndDebugUtilsLabelEXT, VkQueue, queue);                                   \
  HookDefine2(void, vkQueueInsertDebugUtilsLabelEXT, VkQueue, queue, const VkDebugUtilsLabelEXT *,   \
              pLabelInfo);                                                                           \
  HookDefine2(void, vkCmdBeginDebugUtilsLabelEXT, VkCommandBuffer, commandBuffer,                    \
              const VkDebugUtilsLabelEXT *, pLabelInfo);                                             \
  HookDefine1(void, vkCmdEndDebugUtilsLabelEXT, VkCommandBuffer, commandBuffer);                     \
  HookDefine2(void, vkCmdInsertDebugUtilsLabelEXT, VkCommandBuffer, commandBuffer,                   \
              const VkDebugUtilsLabelEXT *, pLabelInfo);                                             \
  HookDefine4(VkResult, vkCreateDebugUtilsMessengerEXT, VkInstance, instance,                        \
              const VkDebugUtilsMessengerCreateInfoEXT *, pCreateInfo,                               \
              const VkAllocationCallbacks *, pAllocator, VkDebugUtilsMessengerEXT *, pMessenger);    \
  HookDefine3(void, vkDestroyDebugUtilsMessengerEXT, VkInstance, instance,                           \
              VkDebugUtilsMessengerEXT, messenger, const VkAllocationCallbacks *, pAllocator);       \
  HookDefine4(void, vkSubmitDebugUtilsMessageEXT, VkInstance, instance,                              \
              VkDebugUtilsMessageSeverityFlagBitsEXT, messageSeverity,                               \
              VkDebugUtilsMessageTypeFlagsEXT, messageTypes,                                         \
              const VkDebugUtilsMessengerCallbackDataEXT *, pCallbackData);                          \
  HookDefine4(VkResult, vkCreateSamplerYcbcrConversion, VkDevice, device,                            \
              const VkSamplerYcbcrConversionCreateInfo *, pCreateInfo,                               \
              const VkAllocationCallbacks *, pAllocator, VkSamplerYcbcrConversion *,                 \
              pYcbcrConversion);                                                                     \
  HookDefine3(void, vkDestroySamplerYcbcrConversion, VkDevice, device, VkSamplerYcbcrConversion,     \
              ycbcrConversion, const VkAllocationCallbacks *, pAllocator);                           \
  HookDefine3(VkResult, vkEnumeratePhysicalDeviceGroups, VkInstance, instance, uint32_t *,           \
              pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties *,                          \
              pPhysicalDeviceGroupProperties);                                                       \
  HookDefine5(void, vkGetDeviceGroupPeerMemoryFeatures, VkDevice, device, uint32_t, heapIndex,       \
              uint32_t, localDeviceIndex, uint32_t, remoteDeviceIndex, VkPeerMemoryFeatureFlags *,   \
              pPeerMemoryFeatures);                                                                  \
  HookDefine2(void, vkCmdSetDeviceMask, VkCommandBuffer, commandBuffer, uint32_t, deviceMask);       \
  HookDefine7(void, vkCmdDispatchBase, VkCommandBuffer, commandBuffer, uint32_t, baseGroupX,         \
              uint32_t, baseGroupY, uint32_t, baseGroupZ, uint32_t, groupCountX, uint32_t,           \
              groupCountY, uint32_t, groupCountZ);                                                   \
  HookDefine2(VkResult, vkGetDeviceGroupPresentCapabilitiesKHR, VkDevice, device,                    \
              VkDeviceGroupPresentCapabilitiesKHR *, pDeviceGroupPresentCapabilities);               \
  HookDefine3(VkResult, vkGetDeviceGroupSurfacePresentModesKHR, VkDevice, device, VkSurfaceKHR,      \
              surface, VkDeviceGroupPresentModeFlagsKHR *, pModes);                                  \
  HookDefine4(VkResult, vkGetPhysicalDevicePresentRectanglesKHR, VkPhysicalDevice, physicalDevice,   \
              VkSurfaceKHR, surface, uint32_t *, pRectCount, VkRect2D *, pRects);                    \
  HookDefine3(VkResult, vkAcquireNextImage2KHR, VkDevice, device,                                    \
              const VkAcquireNextImageInfoKHR *, pAcquireInfo, uint32_t *, pImageIndex);             \
  HookDefine3(void, vkGetDeviceQueue2, VkDevice, device, const VkDeviceQueueInfo2 *, pQueueInfo,     \
              VkQueue *, pQueue);                                                                    \
  HookDefine3(VkResult, vkGetPhysicalDeviceSurfaceCapabilities2KHR, VkPhysicalDevice,                \
              physicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, pSurfaceInfo,                 \
              VkSurfaceCapabilities2KHR *, pSurfaceCapabilities);                                    \
  HookDefine4(VkResult, vkGetPhysicalDeviceSurfaceFormats2KHR, VkPhysicalDevice, physicalDevice,     \
              const VkPhysicalDeviceSurfaceInfo2KHR *, pSurfaceInfo, uint32_t *,                     \
              pSurfaceFormatCount, VkSurfaceFormat2KHR *, pSurfaceFormats);                          \
  HookDefine3(VkResult, vkGetPhysicalDeviceDisplayProperties2KHR, VkPhysicalDevice,                  \
              physicalDevice, uint32_t *, pPropertyCount, VkDisplayProperties2KHR *, pProperties);   \
  HookDefine3(VkResult, vkGetPhysicalDeviceDisplayPlaneProperties2KHR, VkPhysicalDevice,             \
              physicalDevice, uint32_t *, pPropertyCount, VkDisplayPlaneProperties2KHR *,            \
              pProperties);                                                                          \
  HookDefine4(VkResult, vkGetDisplayModeProperties2KHR, VkPhysicalDevice, physicalDevice,            \
              VkDisplayKHR, display, uint32_t *, pPropertyCount, VkDisplayModeProperties2KHR *,      \
              pProperties);                                                                          \
  HookDefine3(VkResult, vkGetDisplayPlaneCapabilities2KHR, VkPhysicalDevice, physicalDevice,         \
              const VkDisplayPlaneInfo2KHR *, pDisplayPlaneInfo, VkDisplayPlaneCapabilities2KHR *,   \
              pCapabilities);                                                                        \
  HookDefine7(void, vkCmdDrawIndirectCountKHR, VkCommandBuffer, commandBuffer, VkBuffer, buffer,     \
              VkDeviceSize, offset, VkBuffer, countBuffer, VkDeviceSize, countBufferOffset,          \
              uint32_t, maxDrawCount, uint32_t, stride);                                             \
  HookDefine7(void, vkCmdDrawIndexedIndirectCountKHR, VkCommandBuffer, commandBuffer, VkBuffer,      \
              buffer, VkDeviceSize, offset, VkBuffer, countBuffer, VkDeviceSize,                     \
              countBufferOffset, uint32_t, maxDrawCount, uint32_t, stride);                          \
  HookDefine4(VkResult, vkCreateValidationCacheEXT, VkDevice, device,                                \
              const VkValidationCacheCreateInfoEXT *, pCreateInfo, const VkAllocationCallbacks *,    \
              pAllocator, VkValidationCacheEXT *, pValidationCache);                                 \
  HookDefine3(void, vkDestroyValidationCacheEXT, VkDevice, device, VkValidationCacheEXT,             \
              validationCache, const VkAllocationCallbacks *, pAllocator);                           \
  HookDefine4(VkResult, vkMergeValidationCachesEXT, VkDevice, device, VkValidationCacheEXT,          \
              dstCache, uint32_t, srcCacheCount, const VkValidationCacheEXT *, pSrcCaches);          \
  HookDefine4(VkResult, vkGetValidationCacheDataEXT, VkDevice, device, VkValidationCacheEXT,         \
              validationCache, size_t *, pDataSize, void *, pData);                                  \
  HookDefine2(VkResult, vkGetSwapchainStatusKHR, VkDevice, device, VkSwapchainKHR, swapchain);       \
  HookDefine4(VkResult, vkCreateRenderPass2KHR, VkDevice, device,                                    \
              const VkRenderPassCreateInfo2KHR *, pCreateInfo, const VkAllocationCallbacks *,        \
              pAllocator, VkRenderPass *, pRenderPass);                                              \
  HookDefine3(void, vkCmdBeginRenderPass2KHR, VkCommandBuffer, commandBuffer,                        \
              const VkRenderPassBeginInfo *, pRenderPassBegin, const VkSubpassBeginInfoKHR *,        \
              pSubpassBeginInfo);                                                                    \
  HookDefine3(void, vkCmdNextSubpass2KHR, VkCommandBuffer, commandBuffer,                            \
              const VkSubpassBeginInfoKHR *, pSubpassBeginInfo, const VkSubpassEndInfoKHR *,         \
              pSubpassEndInfo);                                                                      \
  HookDefine2(void, vkCmdEndRenderPass2KHR, VkCommandBuffer, commandBuffer,                          \
              const VkSubpassEndInfoKHR *, pSubpassEndInfo);                                         \
  HookDefine6(void, vkCmdBindTransformFeedbackBuffersEXT, VkCommandBuffer, commandBuffer,            \
              uint32_t, firstBinding, uint32_t, bindingCount, const VkBuffer *, pBuffers,            \
              const VkDeviceSize *, pOffsets, const VkDeviceSize *, pSizes);                         \
  HookDefine5(void, vkCmdBeginTransformFeedbackEXT, VkCommandBuffer, commandBuffer, uint32_t,        \
              firstBuffer, uint32_t, bufferCount, const VkBuffer *, pCounterBuffers,                 \
              const VkDeviceSize *, pCounterBufferOffsets);                                          \
  HookDefine5(void, vkCmdEndTransformFeedbackEXT, VkCommandBuffer, commandBuffer, uint32_t,          \
              firstBuffer, uint32_t, bufferCount, const VkBuffer *, pCounterBuffers,                 \
              const VkDeviceSize *, pCounterBufferOffsets);                                          \
  HookDefine5(void, vkCmdBeginQueryIndexedEXT, VkCommandBuffer, commandBuffer, VkQueryPool,          \
              queryPool, uint32_t, query, VkQueryControlFlags, flags, uint32_t, index);              \
  HookDefine4(void, vkCmdEndQueryIndexedEXT, VkCommandBuffer, commandBuffer, VkQueryPool,            \
              queryPool, uint32_t, query, uint32_t, index);                                          \
  HookDefine7(void, vkCmdDrawIndirectByteCountEXT, VkCommandBuffer, commandBuffer, uint32_t,         \
              instanceCount, uint32_t, firstInstance, VkBuffer, counterBuffer, VkDeviceSize,         \
              counterBufferOffset, uint32_t, counterOffset, uint32_t, vertexStride);                 \
  HookDefine2(void, vkCmdBeginConditionalRenderingEXT, VkCommandBuffer, commandBuffer,               \
              const VkConditionalRenderingBeginInfoEXT *, pConditionalRenderingBegin);               \
  HookDefine1(void, vkCmdEndConditionalRenderingEXT, VkCommandBuffer, commandBuffer);                \
  HookDefine2(void, vkCmdSetSampleLocationsEXT, VkCommandBuffer, commandBuffer,                      \
              const VkSampleLocationsInfoEXT *, pSampleLocationsInfo);                               \
  HookDefine3(void, vkGetPhysicalDeviceMultisamplePropertiesEXT, VkPhysicalDevice, physicalDevice,   \
              VkSampleCountFlagBits, samples, VkMultisamplePropertiesEXT *, pMultisampleProperties); \
  HookDefine4(void, vkCmdSetDiscardRectangleEXT, VkCommandBuffer, commandBuffer, uint32_t,           \
              firstDiscardRectangle, uint32_t, discardRectangleCount, const VkRect2D *,              \
              pDiscardRectangles);                                                                   \
  HookDefine3(VkResult, vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, VkPhysicalDevice,            \
              physicalDevice, uint32_t *, pTimeDomainCount, VkTimeDomainEXT *, pTimeDomains);        \
  HookDefine5(VkResult, vkGetCalibratedTimestampsEXT, VkDevice, device, uint32_t, timestampCount,    \
              const VkCalibratedTimestampInfoEXT *, pTimestampInfos, uint64_t *, pTimestamps,        \
              uint64_t *, pMaxDeviation);                                                            \
  HookDefine4(void, vkResetQueryPoolEXT, VkDevice, device, VkQueryPool, queryPool, uint32_t,         \
              firstQuery, uint32_t, queryCount);                                                     \
  HookDefine2(VkDeviceAddress, vkGetBufferDeviceAddressEXT, VkDevice, device,                        \
              VkBufferDeviceAddressInfoEXT *, pInfo);                                                \
  HookDefine4(void, vkSetHdrMetadataEXT, VkDevice, device, uint32_t, swapchainCount,                 \
              const VkSwapchainKHR *, pSwapchains, const VkHdrMetadataEXT *, pMetadata);             \
  HookDefine3(void, vkSetLocalDimmingAMD, VkDevice, device, VkSwapchainKHR, swapChain, VkBool32,     \
              localDimmingEnable);                                                                   \
  HookDefine_PlatformSpecific()

struct VkLayerInstanceDispatchTableExtended : VkLayerInstanceDispatchTable
{
};

struct VkLayerDispatchTableExtended : VkLayerDispatchTable
{
  // for consistency & ease, we declare the CreateDevice pointer here
  // even though it won't actually ever get used and is on the instance dispatch chain
  PFN_vkCreateDevice CreateDevice;
};
