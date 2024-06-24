/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

// extensions made core in 1.1
#define VK11 VK_MAKE_VERSION(1, 1, 0)

// extensions made core in 1.2
#define VK12 VK_MAKE_VERSION(1, 2, 0)

// extensions made core in 1.3
#define VK13 VK_MAKE_VERSION(1, 3, 0)

// extensions that are not core in any version
#define VKXX VK_MAKE_VERSION(99, 99, 0)

#if defined(VK_USE_PLATFORM_WIN32_KHR)

#define HookInitExtension_Instance_Win32()                                               \
  HookInitExtension(VK_KHR_win32_surface, CreateWin32SurfaceKHR);                        \
  HookInitExtension(VK_KHR_win32_surface, GetPhysicalDeviceWin32PresentationSupportKHR); \
  HookInitExtension(VK_EXT_full_screen_exclusive, GetPhysicalDeviceSurfacePresentModes2EXT);

#define HookInitExtension_PhysDev_Win32()                                                \
  HookInitExtension(VK_KHR_win32_surface, GetPhysicalDeviceWin32PresentationSupportKHR); \
  HookInitExtension(VK_EXT_full_screen_exclusive, GetPhysicalDeviceSurfacePresentModes2EXT);

#define HookInitExtension_Device_Win32()                                              \
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

#define HookDefine_Win32()                                                                       \
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
              VkExternalMemoryHandleTypeFlagBits, handleType, HANDLE, handle,                    \
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

#else    // defined(VK_USE_PLATFORM_WIN32_KHR)

#define HookInitExtension_Instance_Win32()
#define HookInitExtension_PhysDev_Win32()
#define HookInitExtension_Device_Win32()
#define HookDefine_Win32()

#endif    // defined(VK_USE_PLATFORM_WIN32_KHR)

#if defined(VK_USE_PLATFORM_MACOS_MVK)

#define HookInitExtension_Instance_MVK() \
  HookInitExtension(VK_MVK_macos_surface, CreateMacOSSurfaceMVK);

#define HookDefine_MVK()                                                                       \
  HookDefine4(VkResult, vkCreateMacOSSurfaceMVK, VkInstance, instance,                         \
              const VkMacOSSurfaceCreateInfoMVK *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);

#else    // defined(VK_USE_PLATFORM_MACOS_MVK)

#define HookInitExtension_Instance_MVK()
#define HookDefine_MVK()

#endif    // defined(VK_USE_PLATFORM_MACOS_MVK)

#if defined(VK_USE_PLATFORM_METAL_EXT)

#define HookInitExtension_Instance_Metal() \
  HookInitExtension(VK_EXT_metal_surface, CreateMetalSurfaceEXT);

#define HookDefine_Metal()                                                                     \
  HookDefine4(VkResult, vkCreateMetalSurfaceEXT, VkInstance, instance,                         \
              const VkMetalSurfaceCreateInfoEXT *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);

#else    // defined(VK_USE_PLATFORM_METAL_EXT)

#define HookInitExtension_Instance_Metal()
#define HookDefine_Metal()

#endif    // defined(VK_USE_PLATFORM_METAL_EXT)

#define HookInitExtension_Instance_Mac() \
  HookInitExtension_Instance_MVK();      \
  HookInitExtension_Instance_Metal();
#define HookDefine_Mac() \
  HookDefine_MVK();      \
  HookDefine_Metal();
#define HookInitExtension_PhysDev_Mac()
#define HookInitExtension_Device_Mac()

#if defined(VK_USE_PLATFORM_ANDROID_KHR)

#define HookInitExtension_Instance_Android() \
  HookInitExtension(VK_KHR_android_surface, CreateAndroidSurfaceKHR);

#define HookInitExtension_Device_Android()                              \
  HookInitExtension(VK_ANDROID_external_memory_android_hardware_buffer, \
                    GetMemoryAndroidHardwareBufferANDROID);             \
  HookInitExtension(VK_ANDROID_external_memory_android_hardware_buffer, \
                    GetAndroidHardwareBufferPropertiesANDROID);

#define HookDefine_Android()                                                                      \
  HookDefine4(VkResult, vkCreateAndroidSurfaceKHR, VkInstance, instance,                          \
              const VkAndroidSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *,  \
              pAllocator, VkSurfaceKHR *, pSurface);                                              \
  HookDefine3(VkResult, vkGetAndroidHardwareBufferPropertiesANDROID, VkDevice, device,            \
              const struct AHardwareBuffer *, buffer, VkAndroidHardwareBufferPropertiesANDROID *, \
              pProperties);                                                                       \
  HookDefine3(VkResult, vkGetMemoryAndroidHardwareBufferANDROID, VkDevice, device,                \
              const VkMemoryGetAndroidHardwareBufferInfoANDROID *, pInfo,                         \
              struct AHardwareBuffer **, pBuffer);

#else    // defined(VK_USE_PLATFORM_ANDROID_KHR)

#define HookInitExtension_Instance_Android()
#define HookInitExtension_Device_Android()
#define HookDefine_Android()

#endif    // defined(VK_USE_PLATFORM_ANDROID_KHR)

#define HookInitExtension_PhysDev_Android()

#if defined(VK_USE_PLATFORM_GGP)

#define HookInitExtension_Instance_GGP() \
  HookInitExtension(VK_GGP_stream_descriptor_surface, CreateStreamDescriptorSurfaceGGP);
#define HookDefine_GGP()                                                          \
  HookDefine4(VkResult, vkCreateStreamDescriptorSurfaceGGP, VkInstance, instance, \
              const VkStreamDescriptorSurfaceCreateInfoGGP *, pCreateInfo,        \
              const VkAllocationCallbacks *, pAllocator, VkSurfaceKHR *, pSurface);

#else    // defined(VK_USE_PLATFORM_GGP)

#define HookInitExtension_Instance_GGP()
#define HookDefine_GGP()

#endif    // defined(VK_USE_PLATFORM_GGP)

#define HookInitExtension_PhysDev_GGP()
#define HookInitExtension_Device_GGP()

#if defined(VK_USE_PLATFORM_XCB_KHR)

#define HookInitExtension_Instance_XCB()                      \
  HookInitExtension(VK_KHR_xcb_surface, CreateXcbSurfaceKHR); \
  HookInitExtension(VK_KHR_xcb_surface, GetPhysicalDeviceXcbPresentationSupportKHR);
#define HookInitExtension_PhysDev_XCB() \
  HookInitExtension(VK_KHR_xcb_surface, GetPhysicalDeviceXcbPresentationSupportKHR);

#define HookDefine_XCB()                                                                     \
  HookDefine4(VkResult, vkCreateXcbSurfaceKHR, VkInstance, instance,                         \
              const VkXcbSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);                                         \
  HookDefine4(VkBool32, vkGetPhysicalDeviceXcbPresentationSupportKHR, VkPhysicalDevice,      \
              physicalDevice, uint32_t, queueFamilyIndex, xcb_connection_t *, connection,    \
              xcb_visualid_t, visual_id);

#else    // defined(VK_USE_PLATFORM_XCB_KHR)

#define HookInitExtension_Instance_XCB()
#define HookInitExtension_PhysDev_XCB()
#define HookDefine_XCB()

#endif    // defined(VK_USE_PLATFORM_XCB_KHR)

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)

#define HookInitExtension_Instance_Wayland()                          \
  HookInitExtension(VK_KHR_wayland_surface, CreateWaylandSurfaceKHR); \
  HookInitExtension(VK_KHR_wayland_surface, GetPhysicalDeviceWaylandPresentationSupportKHR);
#define HookInitExtension_PhysDev_Wayland() \
  HookInitExtension(VK_KHR_wayland_surface, GetPhysicalDeviceWaylandPresentationSupportKHR);

#define HookDefine_Wayland()                                                                     \
  HookDefine4(VkResult, vkCreateWaylandSurfaceKHR, VkInstance, instance,                         \
              const VkWaylandSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *, \
              pAllocator, VkSurfaceKHR *, pSurface);                                             \
  HookDefine3(VkBool32, vkGetPhysicalDeviceWaylandPresentationSupportKHR, VkPhysicalDevice,      \
              physicalDevice, uint32_t, queueFamilyIndex, struct wl_display *, display);

#else    // defined(VK_USE_PLATFORM_WAYLAND_KHR)

#define HookInitExtension_Instance_Wayland()
#define HookInitExtension_PhysDev_Wayland()
#define HookDefine_Wayland()

#endif    // defined(VK_USE_PLATFORM_WAYLAND_KHR)

#if defined(VK_USE_PLATFORM_XLIB_KHR)

#define HookInitExtension_Instance_XLib()                                              \
  HookInitExtension(VK_KHR_xlib_surface, CreateXlibSurfaceKHR);                        \
  HookInitExtension(VK_KHR_xlib_surface, GetPhysicalDeviceXlibPresentationSupportKHR); \
  HookInitExtension(VK_EXT_acquire_xlib_display, AcquireXlibDisplayEXT);               \
  HookInitExtension(VK_EXT_acquire_xlib_display, GetRandROutputDisplayEXT);
#define HookInitExtension_PhysDev_XLib()                                               \
  HookInitExtension(VK_KHR_xlib_surface, GetPhysicalDeviceXlibPresentationSupportKHR); \
  HookInitExtension(VK_EXT_acquire_xlib_display, AcquireXlibDisplayEXT);               \
  HookInitExtension(VK_EXT_acquire_xlib_display, GetRandROutputDisplayEXT);

#define HookDefine_XLib()                                                                          \
  HookDefine4(VkResult, vkCreateXlibSurfaceKHR, VkInstance, instance,                              \
              const VkXlibSurfaceCreateInfoKHR *, pCreateInfo, const VkAllocationCallbacks *,      \
              pAllocator, VkSurfaceKHR *, pSurface);                                               \
  HookDefine4(VkBool32, vkGetPhysicalDeviceXlibPresentationSupportKHR, VkPhysicalDevice,           \
              physicalDevice, uint32_t, queueFamilyIndex, Display *, dpy, VisualID, visualID);     \
  HookDefine3(VkResult, vkAcquireXlibDisplayEXT, VkPhysicalDevice, physicalDevice, Display *, dpy, \
              VkDisplayKHR, display);                                                              \
  HookDefine4(VkResult, vkGetRandROutputDisplayEXT, VkPhysicalDevice, physicalDevice, Display *,   \
              dpy, RROutput, rrOutput, VkDisplayKHR *, pDisplay);

#else    // defined(VK_USE_PLATFORM_XLIB_KHR)

#define HookInitExtension_Instance_XLib()
#define HookInitExtension_PhysDev_XLib()
#define HookDefine_XLib()

#endif    // defined(VK_USE_PLATFORM_XLIB_KHR)

#define HookInitExtension_Instance_Linux() \
  HookInitExtension_Instance_XCB();        \
  HookInitExtension_Instance_XLib();       \
  HookInitExtension_Instance_Wayland();
#define HookInitExtension_PhysDev_Linux() \
  HookInitExtension_PhysDev_XCB();        \
  HookInitExtension_PhysDev_XLib();       \
  HookInitExtension_PhysDev_Wayland();
#define HookInitExtension_Device_Linux()

#define HookDefine_Linux() \
  HookDefine_XCB();        \
  HookDefine_XLib();       \
  HookDefine_Wayland();

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

#define HookInitVulkanInstance_PhysDev()                  \
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
#define DeclExts()                                     \
  DeclExt(KHR_xlib_surface);                           \
  DeclExt(KHR_xcb_surface);                            \
  DeclExt(KHR_win32_surface);                          \
  DeclExt(KHR_android_surface);                        \
  DeclExt(MVK_macos_surface);                          \
  DeclExt(KHR_surface);                                \
  DeclExt(GGP_stream_descriptor_surface);              \
  DeclExt(EXT_debug_report);                           \
  DeclExt(KHR_display);                                \
  DeclExt(NV_external_memory_capabilities);            \
  DeclExt(KHR_get_physical_device_properties2);        \
  DeclExt(EXT_display_surface_counter);                \
  DeclExt(EXT_direct_mode_display);                    \
  DeclExt(EXT_acquire_xlib_display);                   \
  DeclExt(KHR_external_memory_capabilities);           \
  DeclExt(KHR_external_semaphore_capabilities);        \
  DeclExt(KHR_external_fence_capabilities);            \
  DeclExt(EXT_debug_utils);                            \
  DeclExt(KHR_device_group_creation);                  \
  DeclExt(protected_memory);                           \
  DeclExt(KHR_get_surface_capabilities2);              \
  DeclExt(KHR_get_display_properties2);                \
  DeclExt(EXT_headless_surface);                       \
  DeclExt(EXT_metal_surface);                          \
  DeclExt(KHR_wayland_surface);                        \
  DeclExt(EXT_acquire_drm_display);                    \
  /* device extensions */                              \
  DeclExt(EXT_debug_marker);                           \
  DeclExt(GGP_frame_token);                            \
  DeclExt(KHR_swapchain);                              \
  DeclExt(KHR_display_swapchain);                      \
  DeclExt(NV_external_memory);                         \
  DeclExt(NV_external_memory_win32);                   \
  DeclExt(NV_win32_keyed_mutex);                       \
  DeclExt(KHR_maintenance1);                           \
  DeclExt(KHR_maintenance2);                           \
  DeclExt(KHR_maintenance3);                           \
  DeclExt(EXT_display_control);                        \
  DeclExt(KHR_external_memory);                        \
  DeclExt(KHR_external_memory_win32);                  \
  DeclExt(KHR_external_memory_fd);                     \
  DeclExt(KHR_external_semaphore);                     \
  DeclExt(KHR_external_semaphore_win32);               \
  DeclExt(KHR_external_semaphore_fd);                  \
  DeclExt(KHR_external_fence);                         \
  DeclExt(KHR_external_fence_win32);                   \
  DeclExt(KHR_external_fence_fd);                      \
  DeclExt(KHR_get_memory_requirements2);               \
  DeclExt(AMD_shader_info);                            \
  DeclExt(KHR_push_descriptor);                        \
  DeclExt(KHR_descriptor_update_template);             \
  DeclExt(KHR_bind_memory2);                           \
  DeclExt(EXT_conservative_rasterization);             \
  DeclExt(EXT_global_priority);                        \
  DeclExt(AMD_buffer_marker);                          \
  DeclExt(EXT_vertex_attribute_divisor);               \
  DeclExt(EXT_sampler_filter_minmax);                  \
  DeclExt(KHR_sampler_ycbcr_conversion);               \
  DeclExt(KHR_device_group);                           \
  DeclExt(MVK_moltenvk);                               \
  DeclExt(KHR_draw_indirect_count);                    \
  DeclExt(EXT_validation_cache);                       \
  DeclExt(KHR_shared_presentable_image);               \
  DeclExt(KHR_create_renderpass2);                     \
  DeclExt(EXT_transform_feedback);                     \
  DeclExt(EXT_conditional_rendering);                  \
  DeclExt(EXT_sample_locations);                       \
  DeclExt(EXT_discard_rectangles);                     \
  DeclExt(EXT_calibrated_timestamps);                  \
  DeclExt(EXT_host_query_reset);                       \
  DeclExt(EXT_buffer_device_address);                  \
  DeclExt(EXT_full_screen_exclusive);                  \
  DeclExt(EXT_hdr_metadata);                           \
  DeclExt(AMD_display_native_hdr);                     \
  DeclExt(EXT_depth_clip_control);                     \
  DeclExt(EXT_depth_clip_enable);                      \
  DeclExt(KHR_pipeline_executable_properties);         \
  DeclExt(AMD_negative_viewport_height);               \
  DeclExt(EXT_line_rasterization);                     \
  DeclExt(GOOGLE_display_timing);                      \
  DeclExt(KHR_timeline_semaphore);                     \
  DeclExt(KHR_performance_query);                      \
  DeclExt(KHR_buffer_device_address);                  \
  DeclExt(EXT_tooling_info);                           \
  DeclExt(KHR_separate_depth_stencil_layouts);         \
  DeclExt(KHR_shader_non_semantic_info);               \
  DeclExt(EXT_inline_uniform_block);                   \
  DeclExt(EXT_custom_border_color);                    \
  DeclExt(EXT_robustness2);                            \
  DeclExt(EXT_pipeline_creation_cache_control);        \
  DeclExt(EXT_primitive_topology_list_restart);        \
  DeclExt(EXT_primitives_generated_query);             \
  DeclExt(EXT_private_data);                           \
  DeclExt(EXT_extended_dynamic_state);                 \
  DeclExt(EXT_rasterization_order_attachment_access);  \
  DeclExt(KHR_copy_commands2);                         \
  DeclExt(KHR_synchronization2);                       \
  DeclExt(KHR_present_wait);                           \
  DeclExt(KHR_maintenance4);                           \
  DeclExt(EXT_color_write_enable);                     \
  DeclExt(EXT_extended_dynamic_state2);                \
  DeclExt(EXT_multisampled_render_to_single_sampled);  \
  DeclExt(EXT_vertex_input_dynamic_state);             \
  DeclExt(KHR_dynamic_rendering);                      \
  DeclExt(KHR_fragment_shading_rate);                  \
  DeclExt(EXT_attachment_feedback_loop_layout);        \
  DeclExt(EXT_pageable_device_local_memory);           \
  DeclExt(EXT_swapchain_maintenance1);                 \
  DeclExt(EXT_provoking_vertex);                       \
  DeclExt(EXT_attachment_feedback_loop_dynamic_state); \
  DeclExt(EXT_extended_dynamic_state3);                \
  DeclExt(EXT_mesh_shader);                            \
  DeclExt(EXT_scalar_block_layout);                    \
  DeclExt(KHR_vertex_attribute_divisor);               \
  DeclExt(KHR_line_rasterization);                     \
  DeclExt(KHR_calibrated_timestamps);                  \
  DeclExt(KHR_deferred_host_operations);               \
  DeclExt(KHR_acceleration_structure);                 \
  DeclExt(KHR_ray_query);                              \
  DeclExt(EXT_nested_command_buffer);                  \
  DeclExt(EXT_shader_object);                          \
  DeclExt(KHR_ray_tracing_pipeline);

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
  CheckExt(EXT_full_screen_exclusive, VKXX);           \
  CheckExt(EXT_headless_surface, VKXX);                \
  CheckExt(EXT_metal_surface, VKXX);                   \
  CheckExt(KHR_wayland_surface, VKXX);                 \
  CheckExt(KHR_performance_query, VKXX);               \
  CheckExt(KHR_fragment_shading_rate, VKXX);           \
  CheckExt(EXT_acquire_drm_display, VKXX);             \
  CheckExt(KHR_calibrated_timestamps, VKXX);

#define CheckDeviceExts()                                     \
  CheckExt(EXT_debug_marker, VKXX);                           \
  CheckExt(GGP_frame_token, VKXX);                            \
  CheckExt(KHR_swapchain, VKXX);                              \
  CheckExt(KHR_display_swapchain, VKXX);                      \
  CheckExt(NV_external_memory, VKXX);                         \
  CheckExt(NV_external_memory_win32, VKXX);                   \
  CheckExt(NV_win32_keyed_mutex, VKXX);                       \
  CheckExt(KHR_maintenance1, VK11);                           \
  CheckExt(KHR_maintenance2, VK11);                           \
  CheckExt(KHR_maintenance3, VK11);                           \
  CheckExt(EXT_display_control, VKXX);                        \
  CheckExt(KHR_external_memory, VK11);                        \
  CheckExt(KHR_external_memory_win32, VKXX);                  \
  CheckExt(KHR_external_memory_fd, VKXX);                     \
  CheckExt(KHR_external_semaphore, VK11);                     \
  CheckExt(KHR_external_semaphore_win32, VKXX);               \
  CheckExt(KHR_external_semaphore_fd, VKXX);                  \
  CheckExt(KHR_external_fence, VK11);                         \
  CheckExt(KHR_external_fence_win32, VKXX);                   \
  CheckExt(KHR_external_fence_fd, VKXX);                      \
  CheckExt(KHR_get_memory_requirements2, VK11);               \
  CheckExt(AMD_shader_info, VKXX);                            \
  CheckExt(KHR_push_descriptor, VKXX);                        \
  CheckExt(KHR_descriptor_update_template, VK11);             \
  CheckExt(KHR_bind_memory2, VK11);                           \
  CheckExt(EXT_conservative_rasterization, VKXX);             \
  CheckExt(EXT_global_priority, VKXX);                        \
  CheckExt(AMD_buffer_marker, VKXX);                          \
  CheckExt(EXT_sampler_filter_minmax, VK12);                  \
  CheckExt(KHR_sampler_ycbcr_conversion, VK11);               \
  CheckExt(KHR_device_group, VK11);                           \
  CheckExt(MVK_moltenvk, VKXX);                               \
  CheckExt(KHR_draw_indirect_count, VK12);                    \
  CheckExt(EXT_validation_cache, VKXX);                       \
  CheckExt(KHR_shared_presentable_image, VKXX);               \
  CheckExt(KHR_create_renderpass2, VK12);                     \
  CheckExt(EXT_transform_feedback, VKXX);                     \
  CheckExt(EXT_conditional_rendering, VKXX);                  \
  CheckExt(EXT_sample_locations, VKXX);                       \
  CheckExt(EXT_discard_rectangles, VKXX);                     \
  CheckExt(EXT_calibrated_timestamps, VKXX);                  \
  CheckExt(EXT_host_query_reset, VK12);                       \
  CheckExt(EXT_buffer_device_address, VKXX);                  \
  CheckExt(EXT_hdr_metadata, VKXX);                           \
  CheckExt(AMD_display_native_hdr, VKXX);                     \
  CheckExt(EXT_depth_clip_control, VKXX);                     \
  CheckExt(EXT_depth_clip_enable, VKXX);                      \
  CheckExt(KHR_pipeline_executable_properties, VKXX);         \
  CheckExt(AMD_negative_viewport_height, VKXX);               \
  CheckExt(EXT_line_rasterization, VKXX);                     \
  CheckExt(GOOGLE_display_timing, VKXX);                      \
  CheckExt(KHR_timeline_semaphore, VK12);                     \
  CheckExt(KHR_performance_query, VKXX);                      \
  CheckExt(KHR_buffer_device_address, VK12);                  \
  CheckExt(EXT_tooling_info, VK13);                           \
  CheckExt(KHR_separate_depth_stencil_layouts, VK12);         \
  CheckExt(KHR_shader_non_semantic_info, VK13);               \
  CheckExt(EXT_inline_uniform_block, VK13);                   \
  CheckExt(EXT_custom_border_color, VKXX);                    \
  CheckExt(EXT_robustness2, VKXX);                            \
  CheckExt(EXT_pipeline_creation_cache_control, VKXX);        \
  CheckExt(EXT_primitive_topology_list_restart, VKXX);        \
  CheckExt(EXT_primitives_generated_query, VKXX);             \
  CheckExt(EXT_private_data, VK13);                           \
  CheckExt(EXT_extended_dynamic_state, VK13);                 \
  CheckExt(EXT_rasterization_order_attachment_access, VKXX);  \
  CheckExt(KHR_copy_commands2, VK13);                         \
  CheckExt(KHR_synchronization2, VK13);                       \
  CheckExt(KHR_present_wait, VKXX);                           \
  CheckExt(KHR_maintenance4, VK13);                           \
  CheckExt(EXT_color_write_enable, VKXX);                     \
  CheckExt(EXT_extended_dynamic_state2, VK13);                \
  CheckExt(EXT_multisampled_render_to_single_sampled, VKXX);  \
  CheckExt(EXT_vertex_input_dynamic_state, VKXX);             \
  CheckExt(KHR_dynamic_rendering, VK13);                      \
  CheckExt(KHR_fragment_shading_rate, VKXX);                  \
  CheckExt(EXT_attachment_feedback_loop_layout, VKXX);        \
  CheckExt(EXT_pageable_device_local_memory, VKXX);           \
  CheckExt(EXT_swapchain_maintenance1, VKXX);                 \
  CheckExt(EXT_provoking_vertex, VKXX);                       \
  CheckExt(EXT_nested_command_buffer, VKXX);                  \
  CheckExt(EXT_attachment_feedback_loop_dynamic_state, VKXX); \
  CheckExt(EXT_extended_dynamic_state3, VKXX);                \
  CheckExt(EXT_mesh_shader, VKXX);                            \
  CheckExt(EXT_scalar_block_layout, VK12);                    \
  CheckExt(KHR_vertex_attribute_divisor, VKXX);               \
  CheckExt(KHR_line_rasterization, VKXX);                     \
  CheckExt(KHR_calibrated_timestamps, VKXX);                  \
  CheckExt(KHR_deferred_host_operations, VKXX);               \
  CheckExt(KHR_acceleration_structure, VKXX);                 \
  CheckExt(KHR_ray_query, VKXX);                              \
  CheckExt(EXT_shader_object, VKXX);                          \
  CheckExt(KHR_ray_tracing_pipeline, VKXX);

#define HookInitVulkanInstanceExts_PhysDev()                                                         \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfaceSupportKHR);                                \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfaceCapabilitiesKHR);                           \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfaceFormatsKHR);                                \
  HookInitExtension(KHR_surface, GetPhysicalDeviceSurfacePresentModesKHR);                           \
  HookInitExtension(KHR_display, GetPhysicalDeviceDisplayPropertiesKHR);                             \
  HookInitExtension(KHR_display, GetPhysicalDeviceDisplayPlanePropertiesKHR);                        \
  HookInitExtension(KHR_display, GetDisplayPlaneSupportedDisplaysKHR);                               \
  HookInitExtension(KHR_display, GetDisplayModePropertiesKHR);                                       \
  HookInitExtension(KHR_display, CreateDisplayModeKHR);                                              \
  HookInitExtension(KHR_display, GetDisplayPlaneCapabilitiesKHR);                                    \
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
  HookInitExtension(KHR_device_group_creation &&KHR_surface, GetPhysicalDevicePresentRectanglesKHR); \
  HookInitExtension(KHR_get_surface_capabilities2, GetPhysicalDeviceSurfaceFormats2KHR);             \
  HookInitExtension(KHR_get_surface_capabilities2, GetPhysicalDeviceSurfaceCapabilities2KHR);        \
  HookInitExtension(KHR_get_display_properties2, GetPhysicalDeviceDisplayProperties2KHR);            \
  HookInitExtension(KHR_get_display_properties2, GetPhysicalDeviceDisplayPlaneProperties2KHR);       \
  HookInitExtension(EXT_sample_locations, GetPhysicalDeviceMultisamplePropertiesEXT);                \
  HookInitExtension(EXT_calibrated_timestamps, GetPhysicalDeviceCalibrateableTimeDomainsEXT);        \
  HookInitExtension(KHR_performance_query,                                                           \
                    EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR);                  \
  HookInitExtension(KHR_performance_query, GetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR);   \
  HookInitPromotedExtension(EXT_tooling_info, GetPhysicalDeviceToolProperties, EXT);                 \
  HookInitExtension(KHR_fragment_shading_rate, GetPhysicalDeviceFragmentShadingRatesKHR);            \
  HookInitExtension(EXT_acquire_drm_display, AcquireDrmDisplayEXT);                                  \
  HookInitExtension(EXT_acquire_drm_display, GetDrmDisplayEXT);                                      \
  HookInitExtension(KHR_calibrated_timestamps, GetPhysicalDeviceCalibrateableTimeDomainsKHR);        \
  HookInitExtension_PhysDev_Win32();                                                                 \
  HookInitExtension_PhysDev_Linux();                                                                 \
  HookInitExtension_PhysDev_GGP();                                                                   \
  HookInitExtension_PhysDev_Android();                                                               \
  HookInitExtension_PhysDev_Mac();

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
  HookInitExtension(EXT_headless_surface, CreateHeadlessSurfaceEXT);                                 \
  HookInitExtension(KHR_performance_query,                                                           \
                    EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR);                  \
  HookInitExtension(KHR_performance_query, GetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR);   \
  HookInitPromotedExtension(EXT_tooling_info, GetPhysicalDeviceToolProperties, EXT);                 \
  HookInitExtension(KHR_fragment_shading_rate, GetPhysicalDeviceFragmentShadingRatesKHR);            \
  HookInitExtension(EXT_acquire_drm_display, AcquireDrmDisplayEXT);                                  \
  HookInitExtension(EXT_acquire_drm_display, GetDrmDisplayEXT);                                      \
  HookInitExtension(KHR_calibrated_timestamps, GetPhysicalDeviceCalibrateableTimeDomainsKHR);        \
  HookInitExtension_Instance_Win32();                                                                \
  HookInitExtension_Instance_Linux();                                                                \
  HookInitExtension_Instance_GGP();                                                                  \
  HookInitExtension_Instance_Android();                                                              \
  HookInitExtension_Instance_Mac();

#define HookInitVulkanDeviceExts()                                                                   \
  HookInitExtension(EXT_debug_marker, DebugMarkerSetObjectTagEXT);                                   \
  HookInitExtension(EXT_debug_marker, DebugMarkerSetObjectNameEXT);                                  \
  HookInitExtension(EXT_debug_marker, CmdDebugMarkerBeginEXT);                                       \
  HookInitExtension(EXT_debug_marker, CmdDebugMarkerEndEXT);                                         \
  HookInitExtension(EXT_debug_marker, CmdDebugMarkerInsertEXT);                                      \
  HookInitExtension(KHR_swapchain, CreateSwapchainKHR);                                              \
  HookInitExtension(KHR_swapchain, DestroySwapchainKHR);                                             \
  HookInitExtension(KHR_swapchain, GetSwapchainImagesKHR);                                           \
  HookInitExtension(KHR_swapchain, AcquireNextImageKHR);                                             \
  HookInitExtension(KHR_swapchain, QueuePresentKHR);                                                 \
  HookInitExtension(KHR_display_swapchain, CreateSharedSwapchainsKHR);                               \
  HookInitPromotedExtension(KHR_maintenance1, TrimCommandPool, KHR);                                 \
  HookInitExtension(EXT_display_control, DisplayPowerControlEXT);                                    \
  HookInitExtension(EXT_display_control, RegisterDeviceEventEXT);                                    \
  HookInitExtension(EXT_display_control, RegisterDisplayEventEXT);                                   \
  HookInitExtension(EXT_display_control, GetSwapchainCounterEXT);                                    \
  HookInitExtension(KHR_external_memory_fd, GetMemoryFdKHR);                                         \
  HookInitExtension(KHR_external_memory_fd, GetMemoryFdPropertiesKHR);                               \
  HookInitExtension(KHR_external_semaphore_fd, ImportSemaphoreFdKHR);                                \
  HookInitExtension(KHR_external_semaphore_fd, GetSemaphoreFdKHR);                                   \
  HookInitExtension(KHR_external_fence_fd, ImportFenceFdKHR);                                        \
  HookInitExtension(KHR_external_fence_fd, GetFenceFdKHR);                                           \
  HookInitPromotedExtension(KHR_get_memory_requirements2, GetBufferMemoryRequirements2, KHR);        \
  HookInitPromotedExtension(KHR_get_memory_requirements2, GetImageMemoryRequirements2, KHR);         \
  HookInitPromotedExtension(KHR_get_memory_requirements2, GetImageSparseMemoryRequirements2, KHR);   \
  HookInitExtension(AMD_shader_info, GetShaderInfoAMD);                                              \
  HookInitExtension(KHR_push_descriptor, CmdPushDescriptorSetKHR);                                   \
  HookInitPromotedExtension(KHR_descriptor_update_template, CreateDescriptorUpdateTemplate, KHR);    \
  HookInitPromotedExtension(KHR_descriptor_update_template, DestroyDescriptorUpdateTemplate, KHR);   \
  HookInitPromotedExtension(KHR_descriptor_update_template, UpdateDescriptorSetWithTemplate, KHR);   \
  HookInitExtension(KHR_push_descriptor &&KHR_descriptor_update_template,                            \
                    CmdPushDescriptorSetWithTemplateKHR);                                            \
  HookInitPromotedExtension(KHR_bind_memory2, BindBufferMemory2, KHR);                               \
  HookInitPromotedExtension(KHR_bind_memory2, BindImageMemory2, KHR);                                \
  HookInitPromotedExtension(KHR_maintenance3, GetDescriptorSetLayoutSupport, KHR);                   \
  HookInitExtension(AMD_buffer_marker, CmdWriteBufferMarkerAMD);                                     \
  HookInitExtension(EXT_debug_utils, SetDebugUtilsObjectNameEXT);                                    \
  HookInitExtension(EXT_debug_utils, SetDebugUtilsObjectTagEXT);                                     \
  HookInitExtension(EXT_debug_utils, QueueBeginDebugUtilsLabelEXT);                                  \
  HookInitExtension(EXT_debug_utils, QueueEndDebugUtilsLabelEXT);                                    \
  HookInitExtension(EXT_debug_utils, QueueInsertDebugUtilsLabelEXT);                                 \
  HookInitExtension(EXT_debug_utils, CmdBeginDebugUtilsLabelEXT);                                    \
  HookInitExtension(EXT_debug_utils, CmdEndDebugUtilsLabelEXT);                                      \
  HookInitExtension(EXT_debug_utils, CmdInsertDebugUtilsLabelEXT);                                   \
  HookInitPromotedExtension(KHR_sampler_ycbcr_conversion, CreateSamplerYcbcrConversion, KHR);        \
  HookInitPromotedExtension(KHR_sampler_ycbcr_conversion, DestroySamplerYcbcrConversion, KHR);       \
  HookInitPromotedExtension(KHR_device_group, GetDeviceGroupPeerMemoryFeatures, KHR);                \
  HookInitPromotedExtension(KHR_device_group, CmdSetDeviceMask, KHR);                                \
  HookInitPromotedExtension(KHR_device_group, CmdDispatchBase, KHR);                                 \
  HookInitExtension(KHR_device_group &&KHR_surface, GetDeviceGroupPresentCapabilitiesKHR);           \
  HookInitExtension(KHR_device_group &&KHR_surface, GetDeviceGroupSurfacePresentModesKHR);           \
  HookInitExtension(KHR_device_group &&KHR_swapchain, AcquireNextImage2KHR);                         \
  HookInitExtension(protected_memory, GetDeviceQueue2);                                              \
  HookInitPromotedExtension(KHR_draw_indirect_count, CmdDrawIndirectCount, KHR);                     \
  HookInitPromotedExtension(KHR_draw_indirect_count, CmdDrawIndexedIndirectCount, KHR);              \
  HookInitExtension(EXT_validation_cache, CreateValidationCacheEXT);                                 \
  HookInitExtension(EXT_validation_cache, DestroyValidationCacheEXT);                                \
  HookInitExtension(EXT_validation_cache, MergeValidationCachesEXT);                                 \
  HookInitExtension(EXT_validation_cache, GetValidationCacheDataEXT);                                \
  HookInitExtension(KHR_shared_presentable_image, GetSwapchainStatusKHR);                            \
  HookInitPromotedExtension(KHR_create_renderpass2, CreateRenderPass2, KHR);                         \
  HookInitPromotedExtension(KHR_create_renderpass2, CmdBeginRenderPass2, KHR);                       \
  HookInitPromotedExtension(KHR_create_renderpass2, CmdNextSubpass2, KHR);                           \
  HookInitPromotedExtension(KHR_create_renderpass2, CmdEndRenderPass2, KHR);                         \
  HookInitExtension(EXT_transform_feedback, CmdBindTransformFeedbackBuffersEXT);                     \
  HookInitExtension(EXT_transform_feedback, CmdBeginTransformFeedbackEXT);                           \
  HookInitExtension(EXT_transform_feedback, CmdEndTransformFeedbackEXT);                             \
  HookInitExtension(EXT_transform_feedback, CmdBeginQueryIndexedEXT);                                \
  HookInitExtension(EXT_transform_feedback, CmdEndQueryIndexedEXT);                                  \
  HookInitExtension(EXT_transform_feedback, CmdDrawIndirectByteCountEXT);                            \
  HookInitExtension(EXT_conditional_rendering, CmdBeginConditionalRenderingEXT);                     \
  HookInitExtension(EXT_conditional_rendering, CmdEndConditionalRenderingEXT);                       \
  HookInitExtension(EXT_sample_locations, CmdSetSampleLocationsEXT);                                 \
  HookInitExtension(EXT_discard_rectangles, CmdSetDiscardRectangleEXT);                              \
  HookInitExtension(EXT_calibrated_timestamps, GetCalibratedTimestampsEXT);                          \
  HookInitPromotedExtension(EXT_host_query_reset, ResetQueryPool, EXT);                              \
  HookInitExtension(EXT_buffer_device_address, GetBufferDeviceAddressEXT);                           \
  HookInitExtension(EXT_hdr_metadata, SetHdrMetadataEXT);                                            \
  HookInitExtension(AMD_display_native_hdr, SetLocalDimmingAMD);                                     \
  HookInitExtension(KHR_pipeline_executable_properties, GetPipelineExecutablePropertiesKHR);         \
  HookInitExtension(KHR_pipeline_executable_properties, GetPipelineExecutableStatisticsKHR);         \
  HookInitExtension(KHR_pipeline_executable_properties,                                              \
                    GetPipelineExecutableInternalRepresentationsKHR);                                \
  HookInitExtension(EXT_line_rasterization, CmdSetLineStippleEXT);                                   \
  HookInitExtension(GOOGLE_display_timing, GetRefreshCycleDurationGOOGLE);                           \
  HookInitExtension(GOOGLE_display_timing, GetPastPresentationTimingGOOGLE);                         \
  HookInitPromotedExtension(KHR_timeline_semaphore, GetSemaphoreCounterValue, KHR);                  \
  HookInitPromotedExtension(KHR_timeline_semaphore, WaitSemaphores, KHR);                            \
  HookInitPromotedExtension(KHR_timeline_semaphore, SignalSemaphore, KHR);                           \
  HookInitExtension(KHR_performance_query, AcquireProfilingLockKHR);                                 \
  HookInitExtension(KHR_performance_query, ReleaseProfilingLockKHR);                                 \
  HookInitPromotedExtension(KHR_buffer_device_address, GetBufferDeviceAddress, KHR);                 \
  HookInitPromotedExtension(KHR_buffer_device_address, GetBufferOpaqueCaptureAddress, KHR);          \
  HookInitPromotedExtension(KHR_buffer_device_address, GetDeviceMemoryOpaqueCaptureAddress, KHR);    \
  HookInitPromotedExtension(EXT_private_data, CreatePrivateDataSlot, EXT);                           \
  HookInitPromotedExtension(EXT_private_data, DestroyPrivateDataSlot, EXT);                          \
  HookInitPromotedExtension(EXT_private_data, SetPrivateData, EXT);                                  \
  HookInitPromotedExtension(EXT_private_data, GetPrivateData, EXT);                                  \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object, CmdSetCullMode, EXT);   \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object, CmdSetFrontFace, EXT);  \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetPrimitiveTopology, EXT);                                           \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetViewportWithCount, EXT);                                           \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetScissorWithCount, EXT);                                            \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdBindVertexBuffers2, EXT);                                             \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetDepthTestEnable, EXT);                                             \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetDepthWriteEnable, EXT);                                            \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object, CmdSetDepthCompareOp,   \
                            EXT);                                                                    \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetDepthBoundsTestEnable, EXT);                                       \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object,                         \
                            CmdSetStencilTestEnable, EXT);                                           \
  HookInitPromotedExtension(EXT_extended_dynamic_state || EXT_shader_object, CmdSetStencilOp, EXT);  \
  HookInitPromotedExtension(KHR_copy_commands2, CmdCopyBuffer2, KHR);                                \
  HookInitPromotedExtension(KHR_copy_commands2, CmdCopyImage2, KHR);                                 \
  HookInitPromotedExtension(KHR_copy_commands2, CmdCopyBufferToImage2, KHR);                         \
  HookInitPromotedExtension(KHR_copy_commands2, CmdCopyImageToBuffer2, KHR);                         \
  HookInitPromotedExtension(KHR_copy_commands2, CmdBlitImage2, KHR);                                 \
  HookInitPromotedExtension(KHR_copy_commands2, CmdResolveImage2, KHR);                              \
  HookInitPromotedExtension(KHR_synchronization2, CmdSetEvent2, KHR);                                \
  HookInitPromotedExtension(KHR_synchronization2, CmdResetEvent2, KHR);                              \
  HookInitPromotedExtension(KHR_synchronization2, CmdWaitEvents2, KHR);                              \
  HookInitPromotedExtension(KHR_synchronization2, CmdPipelineBarrier2, KHR);                         \
  HookInitPromotedExtension(KHR_synchronization2, CmdWriteTimestamp2, KHR);                          \
  HookInitPromotedExtension(KHR_synchronization2, QueueSubmit2, KHR);                                \
  HookInitExtension(KHR_synchronization2 &&AMD_buffer_marker, CmdWriteBufferMarker2AMD);             \
  /* No GetQueueCheckpointData2NV without VK_NV_device_diagnostic_checkpoints */                     \
  HookInitExtension(KHR_present_wait, WaitForPresentKHR);                                            \
  HookInitPromotedExtension(KHR_maintenance4, GetDeviceBufferMemoryRequirements, KHR);               \
  HookInitPromotedExtension(KHR_maintenance4, GetDeviceImageMemoryRequirements, KHR);                \
  HookInitPromotedExtension(KHR_maintenance4, GetDeviceImageSparseMemoryRequirements, KHR);          \
  HookInitExtension(EXT_color_write_enable, CmdSetColorWriteEnableEXT);                              \
  HookInitPromotedExtension(EXT_extended_dynamic_state2 || EXT_shader_object,                        \
                            CmdSetDepthBiasEnable, EXT);                                             \
  HookInitExtension(EXT_extended_dynamic_state2 || EXT_shader_object, CmdSetLogicOpEXT);             \
  HookInitExtension(EXT_extended_dynamic_state2 || EXT_shader_object, CmdSetPatchControlPointsEXT);  \
  HookInitPromotedExtension(EXT_extended_dynamic_state2 || EXT_shader_object,                        \
                            CmdSetPrimitiveRestartEnable, EXT);                                      \
  HookInitPromotedExtension(EXT_extended_dynamic_state2 || EXT_shader_object,                        \
                            CmdSetRasterizerDiscardEnable, EXT);                                     \
  HookInitExtension(EXT_vertex_input_dynamic_state || EXT_shader_object, CmdSetVertexInputEXT);      \
  HookInitPromotedExtension(KHR_dynamic_rendering, CmdBeginRendering, KHR);                          \
  HookInitPromotedExtension(KHR_dynamic_rendering, CmdEndRendering, KHR);                            \
  HookInitExtension(KHR_fragment_shading_rate, CmdSetFragmentShadingRateKHR);                        \
  HookInitExtension(EXT_pageable_device_local_memory, SetDeviceMemoryPriorityEXT);                   \
  HookInitExtension(EXT_swapchain_maintenance1, ReleaseSwapchainImagesEXT);                          \
  HookInitExtension(EXT_attachment_feedback_loop_dynamic_state,                                      \
                    CmdSetAttachmentFeedbackLoopEnableEXT);                                          \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetAlphaToCoverageEnableEXT);                                                 \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetAlphaToOneEnableEXT);    \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetColorBlendAdvancedEXT);  \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetColorBlendEnableEXT);    \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetColorBlendEquationEXT);  \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetColorWriteMaskEXT);      \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetConservativeRasterizationModeEXT);                                         \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetCoverageModulationModeNV);                                                 \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetCoverageModulationTableEnableNV);                                          \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetCoverageModulationTableNV);                                                \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetCoverageReductionModeNV);                                                  \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetCoverageToColorEnableNV);                                                  \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetCoverageToColorLocationNV);                                                \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetDepthClampEnableEXT);    \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetDepthClipEnableEXT);     \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetDepthClipNegativeOneToOneEXT);                                             \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetExtraPrimitiveOverestimationSizeEXT);                                      \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetLineRasterizationModeEXT);                                                 \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetLineStippleEnableEXT);   \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetLogicOpEnableEXT);       \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetPolygonModeEXT);         \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetProvokingVertexModeEXT); \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetRasterizationSamplesEXT);                                                  \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetRasterizationStreamEXT); \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetRepresentativeFragmentTestEnableNV);                                       \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetSampleLocationsEnableEXT);                                                 \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetSampleMaskEXT);          \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetShadingRateImageEnableNV);                                                 \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetTessellationDomainOriginEXT);                                              \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object, CmdSetViewportSwizzleNV);      \
  HookInitExtension(EXT_extended_dynamic_state3 || EXT_shader_object,                                \
                    CmdSetViewportWScalingEnableNV);                                                 \
  HookInitExtension(EXT_mesh_shader, CmdDrawMeshTasksEXT);                                           \
  HookInitExtension(EXT_mesh_shader, CmdDrawMeshTasksIndirectEXT);                                   \
  HookInitExtension(EXT_mesh_shader, CmdDrawMeshTasksIndirectCountEXT);                              \
  HookInitExtension(KHR_calibrated_timestamps, GetCalibratedTimestampsKHR);                          \
  HookInitExtension(KHR_line_rasterization, CmdSetLineStippleKHR);                                   \
  HookInitExtensionEXTtoKHR(CmdSetLineStipple);                                                      \
  HookInitExtension(KHR_deferred_host_operations, CreateDeferredOperationKHR);                       \
  HookInitExtension(KHR_deferred_host_operations, DeferredOperationJoinKHR);                         \
  HookInitExtension(KHR_deferred_host_operations, DestroyDeferredOperationKHR);                      \
  HookInitExtension(KHR_deferred_host_operations, GetDeferredOperationMaxConcurrencyKHR);            \
  HookInitExtension(KHR_deferred_host_operations, GetDeferredOperationResultKHR);                    \
  HookInitExtension(KHR_acceleration_structure, BuildAccelerationStructuresKHR);                     \
  HookInitExtension(KHR_acceleration_structure, CmdBuildAccelerationStructuresIndirectKHR);          \
  HookInitExtension(KHR_acceleration_structure, CmdBuildAccelerationStructuresKHR);                  \
  HookInitExtension(KHR_acceleration_structure, CmdCopyAccelerationStructureKHR);                    \
  HookInitExtension(KHR_acceleration_structure, CmdCopyAccelerationStructureToMemoryKHR);            \
  HookInitExtension(KHR_acceleration_structure, CmdCopyMemoryToAccelerationStructureKHR);            \
  HookInitExtension(KHR_acceleration_structure, CmdWriteAccelerationStructuresPropertiesKHR);        \
  HookInitExtension(KHR_acceleration_structure, CopyAccelerationStructureKHR);                       \
  HookInitExtension(KHR_acceleration_structure, CopyAccelerationStructureToMemoryKHR);               \
  HookInitExtension(KHR_acceleration_structure, CopyMemoryToAccelerationStructureKHR);               \
  HookInitExtension(KHR_acceleration_structure, CreateAccelerationStructureKHR);                     \
  HookInitExtension(KHR_acceleration_structure, DestroyAccelerationStructureKHR);                    \
  HookInitExtension(KHR_acceleration_structure, GetAccelerationStructureBuildSizesKHR);              \
  HookInitExtension(KHR_acceleration_structure, GetAccelerationStructureDeviceAddressKHR);           \
  HookInitExtension(KHR_acceleration_structure, GetDeviceAccelerationStructureCompatibilityKHR);     \
  HookInitExtension(KHR_acceleration_structure, WriteAccelerationStructuresPropertiesKHR);           \
  HookInitExtension(EXT_shader_object, CmdBindShadersEXT);                                           \
  HookInitExtension(EXT_shader_object, CreateShadersEXT);                                            \
  HookInitExtension(EXT_shader_object, DestroyShaderEXT);                                            \
  HookInitExtension(EXT_shader_object, GetShaderBinaryDataEXT);                                      \
  HookInitExtension(KHR_ray_tracing_pipeline, CmdSetRayTracingPipelineStackSizeKHR);                 \
  HookInitExtension(KHR_ray_tracing_pipeline, CmdTraceRaysIndirectKHR);                              \
  HookInitExtension(KHR_ray_tracing_pipeline, CmdTraceRaysKHR);                                      \
  HookInitExtension(KHR_ray_tracing_pipeline, CreateRayTracingPipelinesKHR);                         \
  HookInitExtension(KHR_ray_tracing_pipeline, GetRayTracingCaptureReplayShaderGroupHandlesKHR);      \
  HookInitExtension(KHR_ray_tracing_pipeline, GetRayTracingShaderGroupHandlesKHR);                   \
  HookInitExtension(KHR_ray_tracing_pipeline, GetRayTracingShaderGroupStackSizeKHR);                 \
  HookInitExtension_Device_Win32();                                                                  \
  HookInitExtension_Device_Linux();                                                                  \
  HookInitExtension_Device_GGP();                                                                    \
  HookInitExtension_Device_Android();                                                                \
  HookInitExtension_Device_Mac();

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
  HookDefine7(void, vkCmdDrawIndirectCount, VkCommandBuffer, commandBuffer, VkBuffer, buffer,        \
              VkDeviceSize, offset, VkBuffer, countBuffer, VkDeviceSize, countBufferOffset,          \
              uint32_t, maxDrawCount, uint32_t, stride);                                             \
  HookDefine7(void, vkCmdDrawIndexedIndirectCount, VkCommandBuffer, commandBuffer, VkBuffer,         \
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
  HookDefine4(VkResult, vkCreateRenderPass2, VkDevice, device, const VkRenderPassCreateInfo2 *,      \
              pCreateInfo, const VkAllocationCallbacks *, pAllocator, VkRenderPass *, pRenderPass);  \
  HookDefine3(void, vkCmdBeginRenderPass2, VkCommandBuffer, commandBuffer,                           \
              const VkRenderPassBeginInfo *, pRenderPassBegin, const VkSubpassBeginInfo *,           \
              pSubpassBeginInfo);                                                                    \
  HookDefine3(void, vkCmdNextSubpass2, VkCommandBuffer, commandBuffer, const VkSubpassBeginInfo *,   \
              pSubpassBeginInfo, const VkSubpassEndInfo *, pSubpassEndInfo);                         \
  HookDefine2(void, vkCmdEndRenderPass2, VkCommandBuffer, commandBuffer, const VkSubpassEndInfo *,   \
              pSubpassEndInfo);                                                                      \
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
              physicalDevice, uint32_t *, pTimeDomainCount, VkTimeDomainKHR *, pTimeDomains);        \
  HookDefine5(VkResult, vkGetCalibratedTimestampsEXT, VkDevice, device, uint32_t, timestampCount,    \
              const VkCalibratedTimestampInfoKHR *, pTimestampInfos, uint64_t *, pTimestamps,        \
              uint64_t *, pMaxDeviation);                                                            \
  HookDefine4(void, vkResetQueryPool, VkDevice, device, VkQueryPool, queryPool, uint32_t,            \
              firstQuery, uint32_t, queryCount);                                                     \
  HookDefine2(VkDeviceAddress, vkGetBufferDeviceAddressEXT, VkDevice, device,                        \
              VkBufferDeviceAddressInfo *, pInfo);                                                   \
  HookDefine4(void, vkSetHdrMetadataEXT, VkDevice, device, uint32_t, swapchainCount,                 \
              const VkSwapchainKHR *, pSwapchains, const VkHdrMetadataEXT *, pMetadata);             \
  HookDefine3(void, vkSetLocalDimmingAMD, VkDevice, device, VkSwapchainKHR, swapChain, VkBool32,     \
              localDimmingEnable);                                                                   \
  HookDefine4(VkResult, vkCreateHeadlessSurfaceEXT, VkInstance, instance,                            \
              const VkHeadlessSurfaceCreateInfoEXT *, pCreateInfo, const VkAllocationCallbacks *,    \
              pAllocator, VkSurfaceKHR *, pSurface);                                                 \
  HookDefine4(VkResult, vkGetPipelineExecutablePropertiesKHR, VkDevice, device,                      \
              const VkPipelineInfoKHR *, pPipelineInfo, uint32_t *, pExecutableCount,                \
              VkPipelineExecutablePropertiesKHR *, pProperties);                                     \
  HookDefine4(VkResult, vkGetPipelineExecutableStatisticsKHR, VkDevice, device,                      \
              const VkPipelineExecutableInfoKHR *, pExecutableInfo, uint32_t *, pStatisticCount,     \
              VkPipelineExecutableStatisticKHR *, pStatistics);                                      \
  HookDefine4(VkResult, vkGetPipelineExecutableInternalRepresentationsKHR, VkDevice, device,         \
              const VkPipelineExecutableInfoKHR *, pExecutableInfo, uint32_t *,                      \
              pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR *,         \
              pInternalRepresentations);                                                             \
  HookDefine3(void, vkCmdSetLineStippleEXT, VkCommandBuffer, commandBuffer, uint32_t,                \
              lineStippleFactor, uint16_t, lineStipplePattern);                                      \
  HookDefine3(VkResult, vkGetRefreshCycleDurationGOOGLE, VkDevice, device, VkSwapchainKHR,           \
              swapchain, VkRefreshCycleDurationGOOGLE *, pDisplayTimingProperties);                  \
  HookDefine4(VkResult, vkGetPastPresentationTimingGOOGLE, VkDevice, device, VkSwapchainKHR,         \
              swapchain, uint32_t *, pPresentationTimingCount, VkPastPresentationTimingGOOGLE *,     \
              pPresentationTimings);                                                                 \
  HookDefine3(VkResult, vkGetSemaphoreCounterValue, VkDevice, device, VkSemaphore, semaphore,        \
              uint64_t *, pValue);                                                                   \
  HookDefine3(VkResult, vkWaitSemaphores, VkDevice, device, const VkSemaphoreWaitInfo *,             \
              pWaitInfo, uint64_t, timeout);                                                         \
  HookDefine2(VkResult, vkSignalSemaphore, VkDevice, device, const VkSemaphoreSignalInfo *,          \
              pSignalInfo);                                                                          \
  HookDefine5(VkResult, vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR,             \
              VkPhysicalDevice, physicalDevice, uint32_t, queueFamilyIndex, uint32_t *,              \
              pCounterCount, VkPerformanceCounterKHR *, pCounters,                                   \
              VkPerformanceCounterDescriptionKHR *, pCounterDescriptions);                           \
  HookDefine3(void, vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR, VkPhysicalDevice,       \
              physicalDevice, const VkQueryPoolPerformanceCreateInfoKHR *,                           \
              pPerformanceQueryCreateInfo, uint32_t *, pNumPasses);                                  \
  HookDefine2(VkResult, vkAcquireProfilingLockKHR, VkDevice, device,                                 \
              const VkAcquireProfilingLockInfoKHR *, pInfo);                                         \
  HookDefine1(void, vkReleaseProfilingLockKHR, VkDevice, device);                                    \
  HookDefine2(VkDeviceAddress, vkGetBufferDeviceAddress, VkDevice, device,                           \
              VkBufferDeviceAddressInfo *, pInfo);                                                   \
  HookDefine2(uint64_t, vkGetBufferOpaqueCaptureAddress, VkDevice, device,                           \
              VkBufferDeviceAddressInfo *, pInfo);                                                   \
  HookDefine2(uint64_t, vkGetDeviceMemoryOpaqueCaptureAddress, VkDevice, device,                     \
              VkDeviceMemoryOpaqueCaptureAddressInfo *, pInfo);                                      \
  HookDefine3(VkResult, vkGetPhysicalDeviceToolProperties, VkPhysicalDevice, physicalDevice,         \
              uint32_t *, pToolCount, VkPhysicalDeviceToolProperties *, pToolProperties);            \
  HookDefine4(VkResult, vkCreatePrivateDataSlot, VkDevice, device,                                   \
              const VkPrivateDataSlotCreateInfo *, pCreateInfo, const VkAllocationCallbacks *,       \
              pAllocator, VkPrivateDataSlot *, pPrivateDataSlot);                                    \
  HookDefine3(void, vkDestroyPrivateDataSlot, VkDevice, device, VkPrivateDataSlot,                   \
              privateDataSlot, const VkAllocationCallbacks *, pAllocator);                           \
  HookDefine5(VkResult, vkSetPrivateData, VkDevice, device, VkObjectType, objectType, uint64_t,      \
              objectHandle, VkPrivateDataSlot, privateDataSlot, uint64_t, data);                     \
  HookDefine5(void, vkGetPrivateData, VkDevice, device, VkObjectType, objectType, uint64_t,          \
              objectHandle, VkPrivateDataSlot, privateDataSlot, uint64_t *, pData);                  \
  HookDefine2(void, vkCmdSetCullMode, VkCommandBuffer, commandBuffer, VkCullModeFlags, cullMode);    \
  HookDefine2(void, vkCmdSetFrontFace, VkCommandBuffer, commandBuffer, VkFrontFace, frontFace);      \
  HookDefine2(void, vkCmdSetPrimitiveTopology, VkCommandBuffer, commandBuffer,                       \
              VkPrimitiveTopology, primitiveTopology);                                               \
  HookDefine3(void, vkCmdSetViewportWithCount, VkCommandBuffer, commandBuffer, uint32_t,             \
              viewportCount, const VkViewport *, pViewports);                                        \
  HookDefine3(void, vkCmdSetScissorWithCount, VkCommandBuffer, commandBuffer, uint32_t,              \
              scissorCount, const VkRect2D *, pScissors);                                            \
  HookDefine7(void, vkCmdBindVertexBuffers2, VkCommandBuffer, commandBuffer, uint32_t, firstBinding, \
              uint32_t, bindingCount, const VkBuffer *, pBuffers, const VkDeviceSize *, pOffsets,    \
              const VkDeviceSize *, pSizes, const VkDeviceSize *, pStrides);                         \
  HookDefine2(void, vkCmdSetDepthTestEnable, VkCommandBuffer, commandBuffer, VkBool32,               \
              depthTestEnable);                                                                      \
  HookDefine2(void, vkCmdSetDepthWriteEnable, VkCommandBuffer, commandBuffer, VkBool32,              \
              depthWriteEnable);                                                                     \
  HookDefine2(void, vkCmdSetDepthCompareOp, VkCommandBuffer, commandBuffer, VkCompareOp,             \
              depthCompareOp);                                                                       \
  HookDefine2(void, vkCmdSetDepthBoundsTestEnable, VkCommandBuffer, commandBuffer, VkBool32,         \
              depthBoundsTestEnable);                                                                \
  HookDefine2(void, vkCmdSetStencilTestEnable, VkCommandBuffer, commandBuffer, VkBool32,             \
              stencilTestEnable);                                                                    \
  HookDefine6(void, vkCmdSetStencilOp, VkCommandBuffer, commandBuffer, VkStencilFaceFlags,           \
              faceMask, VkStencilOp, failOp, VkStencilOp, passOp, VkStencilOp, depthFailOp,          \
              VkCompareOp, compareOp);                                                               \
  HookDefine2(void, vkCmdCopyBuffer2, VkCommandBuffer, commandBuffer, const VkCopyBufferInfo2 *,     \
              pCopyBufferInfo);                                                                      \
  HookDefine2(void, vkCmdCopyImage2, VkCommandBuffer, commandBuffer, const VkCopyImageInfo2 *,       \
              pCopyImageInfo);                                                                       \
  HookDefine2(void, vkCmdCopyBufferToImage2, VkCommandBuffer, commandBuffer,                         \
              const VkCopyBufferToImageInfo2 *, pCopyBufferToImageInfo);                             \
  HookDefine2(void, vkCmdCopyImageToBuffer2, VkCommandBuffer, commandBuffer,                         \
              const VkCopyImageToBufferInfo2 *, pCopyImageToBufferInfo);                             \
  HookDefine2(void, vkCmdBlitImage2, VkCommandBuffer, commandBuffer, const VkBlitImageInfo2 *,       \
              pBlitImageInfo);                                                                       \
  HookDefine2(void, vkCmdResolveImage2, VkCommandBuffer, commandBuffer,                              \
              const VkResolveImageInfo2 *, pResolveImageInfo);                                       \
  HookDefine3(void, vkCmdSetEvent2, VkCommandBuffer, commandBuffer, VkEvent, event,                  \
              const VkDependencyInfo *, pDependencyInfo);                                            \
  HookDefine3(void, vkCmdResetEvent2, VkCommandBuffer, commandBuffer, VkEvent, event,                \
              VkPipelineStageFlags2, stageMask);                                                     \
  HookDefine4(void, vkCmdWaitEvents2, VkCommandBuffer, commandBuffer, uint32_t, eventCount,          \
              const VkEvent *, pEvents, const VkDependencyInfo *, pDependencyInfos);                 \
  HookDefine2(void, vkCmdPipelineBarrier2, VkCommandBuffer, commandBuffer,                           \
              const VkDependencyInfo *, pDependencyInfo);                                            \
  HookDefine4(void, vkCmdWriteTimestamp2, VkCommandBuffer, commandBuffer, VkPipelineStageFlags2,     \
              stage, VkQueryPool, queryPool, uint32_t, query);                                       \
  HookDefine4(VkResult, vkQueueSubmit2, VkQueue, queue, uint32_t, submitCount,                       \
              const VkSubmitInfo2 *, pSubmits, VkFence, fence);                                      \
  HookDefine5(void, vkCmdWriteBufferMarker2AMD, VkCommandBuffer, commandBuffer,                      \
              VkPipelineStageFlags2, stage, VkBuffer, dstBuffer, VkDeviceSize, dstOffset,            \
              uint32_t, marker);                                                                     \
  HookDefine4(VkResult, vkWaitForPresentKHR, VkDevice, device, VkSwapchainKHR, swapchain,            \
              uint64_t, presentId, uint64_t, timeout);                                               \
  HookDefine3(void, vkGetDeviceBufferMemoryRequirements, VkDevice, device,                           \
              const VkDeviceBufferMemoryRequirements *, pInfo, VkMemoryRequirements2 *,              \
              pMemoryRequirements);                                                                  \
  HookDefine3(void, vkGetDeviceImageMemoryRequirements, VkDevice, device,                            \
              const VkDeviceImageMemoryRequirements *, pInfo, VkMemoryRequirements2 *,               \
              pMemoryRequirements);                                                                  \
  HookDefine4(void, vkGetDeviceImageSparseMemoryRequirements, VkDevice, device,                      \
              const VkDeviceImageMemoryRequirements *, pInfo, uint32_t *,                            \
              pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2 *,                     \
              pSparseMemoryRequirements);                                                            \
  HookDefine3(void, vkCmdSetColorWriteEnableEXT, VkCommandBuffer, commandBuffer, uint32_t,           \
              attachmentCount, const VkBool32 *, pColorWriteEnables);                                \
  HookDefine2(void, vkCmdSetDepthBiasEnable, VkCommandBuffer, commandBuffer, VkBool32,               \
              depthBiasEnable);                                                                      \
  HookDefine2(void, vkCmdSetLogicOpEXT, VkCommandBuffer, commandBuffer, VkLogicOp, logicOp);         \
  HookDefine2(void, vkCmdSetPatchControlPointsEXT, VkCommandBuffer, commandBuffer, uint32_t,         \
              patchControlPoints);                                                                   \
  HookDefine2(void, vkCmdSetPrimitiveRestartEnable, VkCommandBuffer, commandBuffer, VkBool32,        \
              primitiveRestartEnable);                                                               \
  HookDefine2(void, vkCmdSetRasterizerDiscardEnable, VkCommandBuffer, commandBuffer, VkBool32,       \
              rasterizerDiscardEnable);                                                              \
  HookDefine5(void, vkCmdSetVertexInputEXT, VkCommandBuffer, commandBuffer, uint32_t,                \
              vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT *,            \
              pVertexBindingDescriptions, uint32_t, vertexAttributeDescriptionCount,                 \
              const VkVertexInputAttributeDescription2EXT *, pVertexAttributeDescriptions);          \
  HookDefine2(void, vkCmdBeginRendering, VkCommandBuffer, commandBuffer, const VkRenderingInfo *,    \
              pRenderingInfo);                                                                       \
  HookDefine1(void, vkCmdEndRendering, VkCommandBuffer, commandBuffer);                              \
  HookDefine3(void, vkCmdSetFragmentShadingRateKHR, VkCommandBuffer, commandBuffer,                  \
              const VkExtent2D *, pFragmentSize, const VkFragmentShadingRateCombinerOpKHR *,         \
              combinerOps);                                                                          \
  HookDefine3(VkResult, vkGetPhysicalDeviceFragmentShadingRatesKHR, VkPhysicalDevice,                \
              physicalDevice, uint32_t *, pFragmentShadingRateCount,                                 \
              VkPhysicalDeviceFragmentShadingRateKHR *, pFragmentShadingRates);                      \
  HookDefine3(void, vkSetDeviceMemoryPriorityEXT, VkDevice, device, VkDeviceMemory, memory, float,   \
              priority);                                                                             \
  HookDefine3(VkResult, vkAcquireDrmDisplayEXT, VkPhysicalDevice, physicalDevice, int32_t, drmFd,    \
              VkDisplayKHR, display);                                                                \
  HookDefine4(VkResult, vkGetDrmDisplayEXT, VkPhysicalDevice, physicalDevice, int32_t, drmFd,        \
              uint32_t, connectorId, VkDisplayKHR *, display);                                       \
  HookDefine2(VkResult, vkReleaseSwapchainImagesEXT, VkDevice, device,                               \
              const VkReleaseSwapchainImagesInfoEXT *, pReleaseInfo);                                \
  HookDefine2(void, vkCmdSetAttachmentFeedbackLoopEnableEXT, VkCommandBuffer, commandBuffer,         \
              VkImageAspectFlags, aspectMask);                                                       \
  HookDefine2(void, vkCmdSetAlphaToCoverageEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,      \
              alphaToCoverageEnable);                                                                \
  HookDefine2(void, vkCmdSetAlphaToOneEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,           \
              alphaToOneEnable);                                                                     \
  HookDefine4(void, vkCmdSetColorBlendAdvancedEXT, VkCommandBuffer, commandBuffer, uint32_t,         \
              firstAttachment, uint32_t, attachmentCount, const VkColorBlendAdvancedEXT *,           \
              pColorBlendAdvanced);                                                                  \
  HookDefine4(void, vkCmdSetColorBlendEnableEXT, VkCommandBuffer, commandBuffer, uint32_t,           \
              firstAttachment, uint32_t, attachmentCount, const VkBool32 *, pColorBlendEnables);     \
  HookDefine4(void, vkCmdSetColorBlendEquationEXT, VkCommandBuffer, commandBuffer, uint32_t,         \
              firstAttachment, uint32_t, attachmentCount, const VkColorBlendEquationEXT *,           \
              pColorBlendEquations);                                                                 \
  HookDefine4(void, vkCmdSetColorWriteMaskEXT, VkCommandBuffer, commandBuffer, uint32_t,             \
              firstAttachment, uint32_t, attachmentCount, const VkColorComponentFlags *,             \
              pColorWriteMasks);                                                                     \
  HookDefine2(void, vkCmdSetConservativeRasterizationModeEXT, VkCommandBuffer, commandBuffer,        \
              VkConservativeRasterizationModeEXT, conservativeRasterizationMode);                    \
  HookDefine2(void, vkCmdSetCoverageModulationModeNV, VkCommandBuffer, commandBuffer,                \
              VkCoverageModulationModeNV, coverageModulationMode);                                   \
  HookDefine2(void, vkCmdSetCoverageModulationTableEnableNV, VkCommandBuffer, commandBuffer,         \
              VkBool32, coverageModulationTableEnable);                                              \
  HookDefine3(void, vkCmdSetCoverageModulationTableNV, VkCommandBuffer, commandBuffer, uint32_t,     \
              coverageModulationTableCount, const float *, pCoverageModulationTable);                \
  HookDefine2(void, vkCmdSetCoverageReductionModeNV, VkCommandBuffer, commandBuffer,                 \
              VkCoverageReductionModeNV, coverageReductionMode);                                     \
  HookDefine2(void, vkCmdSetCoverageToColorEnableNV, VkCommandBuffer, commandBuffer, VkBool32,       \
              coverageToColorEnable);                                                                \
  HookDefine2(void, vkCmdSetCoverageToColorLocationNV, VkCommandBuffer, commandBuffer, uint32_t,     \
              coverageToColorLocation);                                                              \
  HookDefine2(void, vkCmdSetDepthClampEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,           \
              depthClampEnable);                                                                     \
  HookDefine2(void, vkCmdSetDepthClipEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,            \
              depthClipEnable);                                                                      \
  HookDefine2(void, vkCmdSetDepthClipNegativeOneToOneEXT, VkCommandBuffer, commandBuffer,            \
              VkBool32, negativeOneToOne);                                                           \
  HookDefine2(void, vkCmdSetExtraPrimitiveOverestimationSizeEXT, VkCommandBuffer, commandBuffer,     \
              float, extraPrimitiveOverestimationSize);                                              \
  HookDefine2(void, vkCmdSetLineRasterizationModeEXT, VkCommandBuffer, commandBuffer,                \
              VkLineRasterizationModeEXT, lineRasterizationMode);                                    \
  HookDefine2(void, vkCmdSetLineStippleEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,          \
              stippledLineEnable);                                                                   \
  HookDefine2(void, vkCmdSetLogicOpEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,              \
              logicOpEnable);                                                                        \
  HookDefine2(void, vkCmdSetPolygonModeEXT, VkCommandBuffer, commandBuffer, VkPolygonMode,           \
              polygonMode);                                                                          \
  HookDefine2(void, vkCmdSetProvokingVertexModeEXT, VkCommandBuffer, commandBuffer,                  \
              VkProvokingVertexModeEXT, provokingVertexMode);                                        \
  HookDefine2(void, vkCmdSetRasterizationSamplesEXT, VkCommandBuffer, commandBuffer,                 \
              VkSampleCountFlagBits, rasterizationSamples);                                          \
  HookDefine2(void, vkCmdSetRasterizationStreamEXT, VkCommandBuffer, commandBuffer, uint32_t,        \
              rasterizationStream);                                                                  \
  HookDefine2(void, vkCmdSetRepresentativeFragmentTestEnableNV, VkCommandBuffer, commandBuffer,      \
              VkBool32, representativeFragmentTestEnable);                                           \
  HookDefine2(void, vkCmdSetSampleLocationsEnableEXT, VkCommandBuffer, commandBuffer, VkBool32,      \
              sampleLocationsEnable);                                                                \
  HookDefine3(void, vkCmdSetSampleMaskEXT, VkCommandBuffer, commandBuffer, VkSampleCountFlagBits,    \
              samples, const VkSampleMask *, pSampleMask);                                           \
  HookDefine2(void, vkCmdSetShadingRateImageEnableNV, VkCommandBuffer, commandBuffer, VkBool32,      \
              shadingRateImageEnable);                                                               \
  HookDefine2(void, vkCmdSetTessellationDomainOriginEXT, VkCommandBuffer, commandBuffer,             \
              VkTessellationDomainOrigin, domainOrigin);                                             \
  HookDefine4(void, vkCmdSetViewportSwizzleNV, VkCommandBuffer, commandBuffer, uint32_t,             \
              firstViewport, uint32_t, viewportCount, const VkViewportSwizzleNV *,                   \
              pViewportSwizzles);                                                                    \
  HookDefine2(void, vkCmdSetViewportWScalingEnableNV, VkCommandBuffer, commandBuffer, VkBool32,      \
              viewportWScalingEnable);                                                               \
  HookDefine4(void, vkCmdDrawMeshTasksEXT, VkCommandBuffer, commandBuffer, uint32_t, groupCountX,    \
              uint32_t, groupCountY, uint32_t, groupCountZ);                                         \
  HookDefine5(void, vkCmdDrawMeshTasksIndirectEXT, VkCommandBuffer, commandBuffer, VkBuffer,         \
              buffer, VkDeviceSize, offset, uint32_t, drawCount, uint32_t, stride);                  \
  HookDefine7(void, vkCmdDrawMeshTasksIndirectCountEXT, VkCommandBuffer, commandBuffer, VkBuffer,    \
              buffer, VkDeviceSize, offset, VkBuffer, countBuffer, VkDeviceSize,                     \
              countBufferOffset, uint32_t, maxDrawCount, uint32_t, stride);                          \
  HookDefine3(VkResult, vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, VkPhysicalDevice,            \
              physicalDevice, uint32_t *, pTimeDomainCount, VkTimeDomainKHR *, pTimeDomains);        \
  HookDefine5(VkResult, vkGetCalibratedTimestampsKHR, VkDevice, device, uint32_t, timestampCount,    \
              const VkCalibratedTimestampInfoKHR *, pTimestampInfos, uint64_t *, pTimestamps,        \
              uint64_t *, pMaxDeviation);                                                            \
  HookDefine3(void, vkCmdSetLineStippleKHR, VkCommandBuffer, commandBuffer, uint32_t,                \
              lineStippleFactor, uint16_t, lineStipplePattern);                                      \
  HookDefine3(VkResult, vkCreateDeferredOperationKHR, VkDevice, device,                              \
              const VkAllocationCallbacks *, pAllocator, VkDeferredOperationKHR *,                   \
              pDeferredOperation);                                                                   \
  HookDefine2(VkResult, vkDeferredOperationJoinKHR, VkDevice, device, VkDeferredOperationKHR,        \
              operation);                                                                            \
  HookDefine3(void, vkDestroyDeferredOperationKHR, VkDevice, device, VkDeferredOperationKHR,         \
              operation, const VkAllocationCallbacks *, pAllocator);                                 \
  HookDefine2(uint32_t, vkGetDeferredOperationMaxConcurrencyKHR, VkDevice, device,                   \
              VkDeferredOperationKHR, operation);                                                    \
  HookDefine2(VkResult, vkGetDeferredOperationResultKHR, VkDevice, device, VkDeferredOperationKHR,   \
              operation);                                                                            \
  HookDefine5(VkResult, vkBuildAccelerationStructuresKHR, VkDevice, device,                          \
              VkDeferredOperationKHR, deferredOperation, uint32_t, infoCount,                        \
              const VkAccelerationStructureBuildGeometryInfoKHR *, pInfos,                           \
              const VkAccelerationStructureBuildRangeInfoKHR *const *, ppBuildRangeInfos);           \
  HookDefine6(void, vkCmdBuildAccelerationStructuresIndirectKHR, VkCommandBuffer, commandBuffer,     \
              uint32_t, infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *, pInfos,      \
              const VkDeviceAddress *, pIndirectDeviceAddresses, const uint32_t *,                   \
              pIndirectStrides, const uint32_t *const *, ppMaxPrimitiveCounts);                      \
  HookDefine4(void, vkCmdBuildAccelerationStructuresKHR, VkCommandBuffer, commandBuffer, uint32_t,   \
              infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *, pInfos,                \
              const VkAccelerationStructureBuildRangeInfoKHR *const *, ppBuildRangeInfos);           \
  HookDefine2(void, vkCmdCopyAccelerationStructureKHR, VkCommandBuffer, commandBuffer,               \
              const VkCopyAccelerationStructureInfoKHR *, pInfo);                                    \
  HookDefine2(void, vkCmdCopyAccelerationStructureToMemoryKHR, VkCommandBuffer, commandBuffer,       \
              const VkCopyAccelerationStructureToMemoryInfoKHR *, pInfo);                            \
  HookDefine2(void, vkCmdCopyMemoryToAccelerationStructureKHR, VkCommandBuffer, commandBuffer,       \
              const VkCopyMemoryToAccelerationStructureInfoKHR *, pInfo);                            \
  HookDefine6(void, vkCmdWriteAccelerationStructuresPropertiesKHR, VkCommandBuffer, commandBuffer,   \
              uint32_t, accelerationStructureCount, const VkAccelerationStructureKHR *,              \
              pAccelerationStructures, VkQueryType, queryType, VkQueryPool, queryPool, uint32_t,     \
              firstQuery);                                                                           \
  HookDefine3(VkResult, vkCopyAccelerationStructureKHR, VkDevice, device, VkDeferredOperationKHR,    \
              deferredOperation, const VkCopyAccelerationStructureInfoKHR *, pInfo);                 \
  HookDefine3(VkResult, vkCopyAccelerationStructureToMemoryKHR, VkDevice, device,                    \
              VkDeferredOperationKHR, deferredOperation,                                             \
              const VkCopyAccelerationStructureToMemoryInfoKHR *, pInfo);                            \
  HookDefine3(VkResult, vkCopyMemoryToAccelerationStructureKHR, VkDevice, device,                    \
              VkDeferredOperationKHR, deferredOperation,                                             \
              const VkCopyMemoryToAccelerationStructureInfoKHR *, pInfo);                            \
  HookDefine4(VkResult, vkCreateAccelerationStructureKHR, VkDevice, device,                          \
              const VkAccelerationStructureCreateInfoKHR *, pCreateInfo,                             \
              const VkAllocationCallbacks *, pAllocator, VkAccelerationStructureKHR *,               \
              pAccelerationStructure);                                                               \
  HookDefine3(void, vkDestroyAccelerationStructureKHR, VkDevice, device, VkAccelerationStructureKHR, \
              accelerationStructure, const VkAllocationCallbacks *, pAllocator);                     \
  HookDefine5(void, vkGetAccelerationStructureBuildSizesKHR, VkDevice, device,                       \
              VkAccelerationStructureBuildTypeKHR, buildType,                                        \
              const VkAccelerationStructureBuildGeometryInfoKHR *, pBuildInfo, const uint32_t *,     \
              pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *, pSizeInfo);           \
  HookDefine2(VkDeviceAddress, vkGetAccelerationStructureDeviceAddressKHR, VkDevice, device,         \
              const VkAccelerationStructureDeviceAddressInfoKHR *, pInfo);                           \
  HookDefine3(void, vkGetDeviceAccelerationStructureCompatibilityKHR, VkDevice, device,              \
              const VkAccelerationStructureVersionInfoKHR *, pVersionInfo,                           \
              VkAccelerationStructureCompatibilityKHR *, pCompatibility);                            \
  HookDefine7(VkResult, vkWriteAccelerationStructuresPropertiesKHR, VkDevice, device, uint32_t,      \
              accelerationStructureCount, const VkAccelerationStructureKHR *,                        \
              pAccelerationStructures, VkQueryType, queryType, size_t, dataSize, void *, pData,      \
              size_t, stride);                                                                       \
  HookDefine4(void, vkCmdBindShadersEXT, VkCommandBuffer, commandBuffer, uint32_t, stageCount,       \
              const VkShaderStageFlagBits *, pStages, const VkShaderEXT *, pShaders);                \
  HookDefine5(VkResult, vkCreateShadersEXT, VkDevice, device, uint32_t, createInfoCount,             \
              const VkShaderCreateInfoEXT *, pCreateInfos, const VkAllocationCallbacks *,            \
              pAllocator, VkShaderEXT *, pShaders);                                                  \
  HookDefine3(void, vkDestroyShaderEXT, VkDevice, device, VkShaderEXT, shader,                       \
              const VkAllocationCallbacks *, pAllocator);                                            \
  HookDefine4(VkResult, vkGetShaderBinaryDataEXT, VkDevice, device, VkShaderEXT, shader, size_t *,   \
              pDataSize, void *, pData);                                                             \
  HookDefine8(void, vkCmdTraceRaysKHR, VkCommandBuffer, commandBuffer,                               \
              const VkStridedDeviceAddressRegionKHR *, pRaygenShaderBindingTable,                    \
              const VkStridedDeviceAddressRegionKHR *, pMissShaderBindingTable,                      \
              const VkStridedDeviceAddressRegionKHR *, pHitShaderBindingTable,                       \
              const VkStridedDeviceAddressRegionKHR *, pCallableShaderBindingTable, uint32_t,        \
              width, uint32_t, height, uint32_t, depth);                                             \
  HookDefine7(VkResult, vkCreateRayTracingPipelinesKHR, VkDevice, device, VkDeferredOperationKHR,    \
              deferredOperation, VkPipelineCache, pipelineCache, uint32_t, createInfoCount,          \
              const VkRayTracingPipelineCreateInfoKHR *, pCreateInfos,                               \
              const VkAllocationCallbacks *, pAllocator, VkPipeline *, pPipelines);                  \
  HookDefine6(VkResult, vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, VkDevice, device,         \
              VkPipeline, pipeline, uint32_t, firstGroup, uint32_t, groupCount, size_t, dataSize,    \
              void *, pData);                                                                        \
  HookDefine6(void, vkCmdTraceRaysIndirectKHR, VkCommandBuffer, commandBuffer,                       \
              const VkStridedDeviceAddressRegionKHR *, pRaygenShaderBindingTable,                    \
              const VkStridedDeviceAddressRegionKHR *, pMissShaderBindingTable,                      \
              const VkStridedDeviceAddressRegionKHR *, pHitShaderBindingTable,                       \
              const VkStridedDeviceAddressRegionKHR *, pCallableShaderBindingTable,                  \
              VkDeviceAddress, indirectDeviceAddress);                                               \
  HookDefine6(VkResult, vkGetRayTracingShaderGroupHandlesKHR, VkDevice, device, VkPipeline,          \
              pipeline, uint32_t, firstGroup, uint32_t, groupCount, size_t, dataSize, void *,        \
              pData);                                                                                \
  HookDefine4(VkDeviceSize, vkGetRayTracingShaderGroupStackSizeKHR, VkDevice, device, VkPipeline,    \
              pipeline, uint32_t, group, VkShaderGroupShaderKHR, groupShader);                       \
  HookDefine2(void, vkCmdSetRayTracingPipelineStackSizeKHR, VkCommandBuffer, commandBuffer,          \
              uint32_t, pipelineStackSize);                                                          \
  HookDefine_Win32();                                                                                \
  HookDefine_Linux();                                                                                \
  HookDefine_GGP();                                                                                  \
  HookDefine_Android();                                                                              \
  HookDefine_Mac();
