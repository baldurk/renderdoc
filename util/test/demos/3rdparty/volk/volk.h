/**
 * volk
 *
 * Copyright (C) 2018-2019, by Arseny Kapoulkine (arseny.kapoulkine@gmail.com)
 * Report bugs and download new versions at https://github.com/zeux/volk
 *
 * This library is distributed under the MIT License. See notice at the end of this file.
 */
#ifndef VOLK_H_
#define VOLK_H_

#if defined(VULKAN_H_) && !defined(VK_NO_PROTOTYPES)
#	error To use volk, you need to define VK_NO_PROTOTYPES before including vulkan.h
#endif

/* VOLK_GENERATE_VERSION_DEFINE */
#define VOLK_HEADER_VERSION 129
/* VOLK_GENERATE_VERSION_DEFINE */

#ifndef VK_NO_PROTOTYPES
#	define VK_NO_PROTOTYPES
#endif

#ifndef VULKAN_H_
#       ifdef VOLK_VULKAN_H_PATH
#               include VOLK_VULKAN_H_PATH
#	elif defined(VK_USE_PLATFORM_WIN32_KHR)
#		include <vulkan/vk_platform.h>
#		include <vulkan/vulkan_core.h>

		/* When VK_USE_PLATFORM_WIN32_KHR is defined, instead of including vulkan.h directly, we include individual parts of the SDK
		 * This is necessary to avoid including <windows.h> which is very heavy - it takes 200ms to parse without WIN32_LEAN_AND_MEAN
		 * and 100ms to parse with it. vulkan_win32.h only needs a few symbols that are easy to redefine ourselves.
		 */
		typedef unsigned long DWORD;
		typedef const wchar_t* LPCWSTR;
		typedef void* HANDLE;
		typedef struct HINSTANCE__* HINSTANCE;
		typedef struct HWND__* HWND;
		typedef struct HMONITOR__* HMONITOR;
		typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES;

#		include <vulkan/vulkan_win32.h>
#	else
#		include <vulkan/vulkan.h>
#	endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct VolkDeviceTable;

/**
 * Initialize library by loading Vulkan loader; call this function before creating the Vulkan instance.
 *
 * Returns VK_SUCCESS on success and VK_ERROR_INITIALIZATION_FAILED otherwise.
 */
VkResult volkInitialize(void);

/**
 * Initialize library by providing a custom handler to load global symbols.
 *
 * This function can be used instead of volkInitialize.
 * The handler function pointer will be asked to load global Vulkan symbols which require no instance
 * (such as vkCreateInstance, vkEnumerateInstance* and vkEnumerateInstanceVersion if available).
 */
void volkInitializeCustom(PFN_vkGetInstanceProcAddr handler);

/**
 * Get Vulkan instance version supported by the Vulkan loader, or 0 if Vulkan isn't supported
 *
 * Returns 0 if volkInitialize wasn't called or failed.
 */
uint32_t volkGetInstanceVersion(void);

/**
 * Load global function pointers using application-created VkInstance; call this function after creating the Vulkan instance.
 */
void volkLoadInstance(VkInstance instance);

/**
 * Load global function pointers using application-created VkDevice; call this function after creating the Vulkan device.
 *
 * Note: this is not suitable for applications that want to use multiple VkDevice objects concurrently.
 */
void volkLoadDevice(VkDevice device);

/**
 * Load function pointers using application-created VkDevice into a table.
 * Application should use function pointers from that table instead of using global function pointers.
 */
void volkLoadDeviceTable(struct VolkDeviceTable* table, VkDevice device);

/**
 * Device-specific function pointer table
 */
struct VolkDeviceTable
{
	/* VOLK_GENERATE_DEVICE_TABLE */
#if defined(VK_VERSION_1_0)
	PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
	PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
	PFN_vkAllocateMemory vkAllocateMemory;
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
	PFN_vkBindBufferMemory vkBindBufferMemory;
	PFN_vkBindImageMemory vkBindImageMemory;
	PFN_vkCmdBeginQuery vkCmdBeginQuery;
	PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
	PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
	PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
	PFN_vkCmdBindPipeline vkCmdBindPipeline;
	PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
	PFN_vkCmdBlitImage vkCmdBlitImage;
	PFN_vkCmdClearAttachments vkCmdClearAttachments;
	PFN_vkCmdClearColorImage vkCmdClearColorImage;
	PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
	PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
	PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
	PFN_vkCmdCopyImage vkCmdCopyImage;
	PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
	PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
	PFN_vkCmdDispatch vkCmdDispatch;
	PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
	PFN_vkCmdDraw vkCmdDraw;
	PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
	PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
	PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
	PFN_vkCmdEndQuery vkCmdEndQuery;
	PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
	PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
	PFN_vkCmdFillBuffer vkCmdFillBuffer;
	PFN_vkCmdNextSubpass vkCmdNextSubpass;
	PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
	PFN_vkCmdPushConstants vkCmdPushConstants;
	PFN_vkCmdResetEvent vkCmdResetEvent;
	PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
	PFN_vkCmdResolveImage vkCmdResolveImage;
	PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
	PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
	PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
	PFN_vkCmdSetEvent vkCmdSetEvent;
	PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
	PFN_vkCmdSetScissor vkCmdSetScissor;
	PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
	PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
	PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
	PFN_vkCmdSetViewport vkCmdSetViewport;
	PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
	PFN_vkCmdWaitEvents vkCmdWaitEvents;
	PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
	PFN_vkCreateBuffer vkCreateBuffer;
	PFN_vkCreateBufferView vkCreateBufferView;
	PFN_vkCreateCommandPool vkCreateCommandPool;
	PFN_vkCreateComputePipelines vkCreateComputePipelines;
	PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
	PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
	PFN_vkCreateEvent vkCreateEvent;
	PFN_vkCreateFence vkCreateFence;
	PFN_vkCreateFramebuffer vkCreateFramebuffer;
	PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
	PFN_vkCreateImage vkCreateImage;
	PFN_vkCreateImageView vkCreateImageView;
	PFN_vkCreatePipelineCache vkCreatePipelineCache;
	PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
	PFN_vkCreateQueryPool vkCreateQueryPool;
	PFN_vkCreateRenderPass vkCreateRenderPass;
	PFN_vkCreateSampler vkCreateSampler;
	PFN_vkCreateSemaphore vkCreateSemaphore;
	PFN_vkCreateShaderModule vkCreateShaderModule;
	PFN_vkDestroyBuffer vkDestroyBuffer;
	PFN_vkDestroyBufferView vkDestroyBufferView;
	PFN_vkDestroyCommandPool vkDestroyCommandPool;
	PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
	PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
	PFN_vkDestroyDevice vkDestroyDevice;
	PFN_vkDestroyEvent vkDestroyEvent;
	PFN_vkDestroyFence vkDestroyFence;
	PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
	PFN_vkDestroyImage vkDestroyImage;
	PFN_vkDestroyImageView vkDestroyImageView;
	PFN_vkDestroyPipeline vkDestroyPipeline;
	PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
	PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
	PFN_vkDestroyQueryPool vkDestroyQueryPool;
	PFN_vkDestroyRenderPass vkDestroyRenderPass;
	PFN_vkDestroySampler vkDestroySampler;
	PFN_vkDestroySemaphore vkDestroySemaphore;
	PFN_vkDestroyShaderModule vkDestroyShaderModule;
	PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
	PFN_vkEndCommandBuffer vkEndCommandBuffer;
	PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
	PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
	PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
	PFN_vkFreeMemory vkFreeMemory;
	PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
	PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
	PFN_vkGetDeviceQueue vkGetDeviceQueue;
	PFN_vkGetEventStatus vkGetEventStatus;
	PFN_vkGetFenceStatus vkGetFenceStatus;
	PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
	PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
	PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
	PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
	PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
	PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
	PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
	PFN_vkMapMemory vkMapMemory;
	PFN_vkMergePipelineCaches vkMergePipelineCaches;
	PFN_vkQueueBindSparse vkQueueBindSparse;
	PFN_vkQueueSubmit vkQueueSubmit;
	PFN_vkQueueWaitIdle vkQueueWaitIdle;
	PFN_vkResetCommandBuffer vkResetCommandBuffer;
	PFN_vkResetCommandPool vkResetCommandPool;
	PFN_vkResetDescriptorPool vkResetDescriptorPool;
	PFN_vkResetEvent vkResetEvent;
	PFN_vkResetFences vkResetFences;
	PFN_vkSetEvent vkSetEvent;
	PFN_vkUnmapMemory vkUnmapMemory;
	PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
	PFN_vkWaitForFences vkWaitForFences;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
	PFN_vkBindBufferMemory2 vkBindBufferMemory2;
	PFN_vkBindImageMemory2 vkBindImageMemory2;
	PFN_vkCmdDispatchBase vkCmdDispatchBase;
	PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
	PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
	PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
	PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
	PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
	PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
	PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
	PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
	PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
	PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
	PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
	PFN_vkTrimCommandPool vkTrimCommandPool;
	PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_AMD_buffer_marker)
	PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_display_native_hdr)
	PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
	PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
	PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
	PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
	PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
	PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_EXT_buffer_device_address)
	PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
	PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_conditional_rendering)
	PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
	PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
	PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
	PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
	PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
	PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_discard_rectangles)
	PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_display_control)
	PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
	PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
	PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
	PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_external_memory_host)
	PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_full_screen_exclusive)
	PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
	PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_hdr_metadata)
	PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_host_query_reset)
	PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
	PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
	PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_sample_locations)
	PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_transform_feedback)
	PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
	PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
	PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
	PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
	PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
	PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
	PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
	PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
	PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
	PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_GOOGLE_display_timing)
	PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
	PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_INTEL_performance_query)
	PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
	PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
	PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
	PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
	PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
	PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
	PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
	PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
	PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_bind_memory2)
	PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
	PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
	PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
	PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
	PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_create_renderpass2)
	PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
	PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
	PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
	PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_descriptor_update_template)
	PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
	PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
	PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
	PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
	PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
	PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_display_swapchain)
	PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
	PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
	PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_external_fence_fd)
	PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
	PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
	PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
	PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_fd)
	PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
	PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
	PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
	PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_fd)
	PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
	PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
	PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
	PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_get_memory_requirements2)
	PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
	PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
	PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_maintenance1)
	PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
	PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_performance_query)
	PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
	PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_executable_properties)
	PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
	PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
	PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_push_descriptor)
	PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
	PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
	PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
	PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_swapchain)
	PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
	PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
	PFN_vkQueuePresentKHR vkQueuePresentKHR;
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_timeline_semaphore)
	PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
	PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
	PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_NVX_device_generated_commands)
	PFN_vkCmdProcessCommandsNVX vkCmdProcessCommandsNVX;
	PFN_vkCmdReserveSpaceForCommandsNVX vkCmdReserveSpaceForCommandsNVX;
	PFN_vkCreateIndirectCommandsLayoutNVX vkCreateIndirectCommandsLayoutNVX;
	PFN_vkCreateObjectTableNVX vkCreateObjectTableNVX;
	PFN_vkDestroyIndirectCommandsLayoutNVX vkDestroyIndirectCommandsLayoutNVX;
	PFN_vkDestroyObjectTableNVX vkDestroyObjectTableNVX;
	PFN_vkRegisterObjectsNVX vkRegisterObjectsNVX;
	PFN_vkUnregisterObjectsNVX vkUnregisterObjectsNVX;
#endif /* defined(VK_NVX_device_generated_commands) */
#if defined(VK_NVX_image_view_handle)
	PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NV_clip_space_w_scaling)
	PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_device_diagnostic_checkpoints)
	PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
	PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_external_memory_win32)
	PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_mesh_shader)
	PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
	PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
	PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_ray_tracing)
	PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
	PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
	PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
	PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
	PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
	PFN_vkCompileDeferredNV vkCompileDeferredNV;
	PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
	PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
	PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
	PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
	PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
	PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive)
	PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
	PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
	PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
	PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#endif /* defined(VK_NV_shading_rate_image) */
#if (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1))
	PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif /* (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1))
	PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
	PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
	PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
	PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
	/* VOLK_GENERATE_DEVICE_TABLE */
};

/* VOLK_GENERATE_PROTOTYPES_H */
#if defined(VK_VERSION_1_0)
extern PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
extern PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
extern PFN_vkAllocateMemory vkAllocateMemory;
extern PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
extern PFN_vkBindBufferMemory vkBindBufferMemory;
extern PFN_vkBindImageMemory vkBindImageMemory;
extern PFN_vkCmdBeginQuery vkCmdBeginQuery;
extern PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
extern PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline vkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
extern PFN_vkCmdBlitImage vkCmdBlitImage;
extern PFN_vkCmdClearAttachments vkCmdClearAttachments;
extern PFN_vkCmdClearColorImage vkCmdClearColorImage;
extern PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
extern PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
extern PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage vkCmdCopyImage;
extern PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
extern PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
extern PFN_vkCmdDispatch vkCmdDispatch;
extern PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
extern PFN_vkCmdDraw vkCmdDraw;
extern PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
extern PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
extern PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
extern PFN_vkCmdEndQuery vkCmdEndQuery;
extern PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
extern PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
extern PFN_vkCmdFillBuffer vkCmdFillBuffer;
extern PFN_vkCmdNextSubpass vkCmdNextSubpass;
extern PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants vkCmdPushConstants;
extern PFN_vkCmdResetEvent vkCmdResetEvent;
extern PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
extern PFN_vkCmdResolveImage vkCmdResolveImage;
extern PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
extern PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
extern PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
extern PFN_vkCmdSetEvent vkCmdSetEvent;
extern PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
extern PFN_vkCmdSetScissor vkCmdSetScissor;
extern PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
extern PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
extern PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
extern PFN_vkCmdSetViewport vkCmdSetViewport;
extern PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
extern PFN_vkCmdWaitEvents vkCmdWaitEvents;
extern PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
extern PFN_vkCreateBuffer vkCreateBuffer;
extern PFN_vkCreateBufferView vkCreateBufferView;
extern PFN_vkCreateCommandPool vkCreateCommandPool;
extern PFN_vkCreateComputePipelines vkCreateComputePipelines;
extern PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
extern PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
extern PFN_vkCreateDevice vkCreateDevice;
extern PFN_vkCreateEvent vkCreateEvent;
extern PFN_vkCreateFence vkCreateFence;
extern PFN_vkCreateFramebuffer vkCreateFramebuffer;
extern PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
extern PFN_vkCreateImage vkCreateImage;
extern PFN_vkCreateImageView vkCreateImageView;
extern PFN_vkCreateInstance vkCreateInstance;
extern PFN_vkCreatePipelineCache vkCreatePipelineCache;
extern PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
extern PFN_vkCreateQueryPool vkCreateQueryPool;
extern PFN_vkCreateRenderPass vkCreateRenderPass;
extern PFN_vkCreateSampler vkCreateSampler;
extern PFN_vkCreateSemaphore vkCreateSemaphore;
extern PFN_vkCreateShaderModule vkCreateShaderModule;
extern PFN_vkDestroyBuffer vkDestroyBuffer;
extern PFN_vkDestroyBufferView vkDestroyBufferView;
extern PFN_vkDestroyCommandPool vkDestroyCommandPool;
extern PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
extern PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDevice vkDestroyDevice;
extern PFN_vkDestroyEvent vkDestroyEvent;
extern PFN_vkDestroyFence vkDestroyFence;
extern PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
extern PFN_vkDestroyImage vkDestroyImage;
extern PFN_vkDestroyImageView vkDestroyImageView;
extern PFN_vkDestroyInstance vkDestroyInstance;
extern PFN_vkDestroyPipeline vkDestroyPipeline;
extern PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
extern PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
extern PFN_vkDestroyQueryPool vkDestroyQueryPool;
extern PFN_vkDestroyRenderPass vkDestroyRenderPass;
extern PFN_vkDestroySampler vkDestroySampler;
extern PFN_vkDestroySemaphore vkDestroySemaphore;
extern PFN_vkDestroyShaderModule vkDestroyShaderModule;
extern PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
extern PFN_vkEndCommandBuffer vkEndCommandBuffer;
extern PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
extern PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
extern PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
extern PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
extern PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
extern PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
extern PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
extern PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
extern PFN_vkFreeMemory vkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
extern PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
extern PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
extern PFN_vkGetDeviceQueue vkGetDeviceQueue;
extern PFN_vkGetEventStatus vkGetEventStatus;
extern PFN_vkGetFenceStatus vkGetFenceStatus;
extern PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
extern PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
extern PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
extern PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
extern PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
extern PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
extern PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
extern PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
extern PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
extern PFN_vkMapMemory vkMapMemory;
extern PFN_vkMergePipelineCaches vkMergePipelineCaches;
extern PFN_vkQueueBindSparse vkQueueBindSparse;
extern PFN_vkQueueSubmit vkQueueSubmit;
extern PFN_vkQueueWaitIdle vkQueueWaitIdle;
extern PFN_vkResetCommandBuffer vkResetCommandBuffer;
extern PFN_vkResetCommandPool vkResetCommandPool;
extern PFN_vkResetDescriptorPool vkResetDescriptorPool;
extern PFN_vkResetEvent vkResetEvent;
extern PFN_vkResetFences vkResetFences;
extern PFN_vkSetEvent vkSetEvent;
extern PFN_vkUnmapMemory vkUnmapMemory;
extern PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
extern PFN_vkWaitForFences vkWaitForFences;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
extern PFN_vkBindBufferMemory2 vkBindBufferMemory2;
extern PFN_vkBindImageMemory2 vkBindImageMemory2;
extern PFN_vkCmdDispatchBase vkCmdDispatchBase;
extern PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
extern PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
extern PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
extern PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
extern PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
extern PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
extern PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
extern PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
extern PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
extern PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
extern PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
extern PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
extern PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
extern PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
extern PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
extern PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
extern PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
extern PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
extern PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
extern PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
extern PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 vkGetPhysicalDeviceSparseImageFormatProperties2;
extern PFN_vkTrimCommandPool vkTrimCommandPool;
extern PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_AMD_buffer_marker)
extern PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_display_native_hdr)
extern PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
extern PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
extern PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
extern PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
extern PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
extern PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_EXT_acquire_xlib_display)
extern PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
extern PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_buffer_device_address)
extern PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
extern PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
extern PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_conditional_rendering)
extern PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
extern PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
extern PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
extern PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
extern PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
extern PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
extern PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_debug_report)
extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
extern PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
extern PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
extern PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
extern PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
extern PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
extern PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_direct_mode_display)
extern PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_discard_rectangles)
extern PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_display_control)
extern PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
extern PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
extern PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
extern PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_display_surface_counter)
extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_external_memory_host)
extern PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_full_screen_exclusive)
extern PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
extern PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
extern PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_hdr_metadata)
extern PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_headless_surface)
extern PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_host_query_reset)
extern PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
extern PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
extern PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_metal_surface)
extern PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_sample_locations)
extern PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
extern PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_transform_feedback)
extern PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
extern PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
extern PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
extern PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
extern PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
extern PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
extern PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
extern PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
extern PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
extern PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_FUCHSIA_imagepipe_surface)
extern PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
extern PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_GOOGLE_display_timing)
extern PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
extern PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_INTEL_performance_query)
extern PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
extern PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
extern PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
extern PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
extern PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
extern PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
extern PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
extern PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
extern PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_android_surface)
extern PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_bind_memory2)
extern PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
extern PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
extern PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
extern PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_create_renderpass2)
extern PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
extern PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
extern PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
extern PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_descriptor_update_template)
extern PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
extern PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
extern PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
extern PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
extern PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
extern PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_device_group_creation)
extern PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
extern PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
extern PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
extern PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
extern PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
extern PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
extern PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
extern PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_display_swapchain)
extern PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
extern PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
extern PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_external_fence_capabilities)
extern PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_fence_fd)
extern PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
extern PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
extern PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
extern PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_capabilities)
extern PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_memory_fd)
extern PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
extern PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
extern PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
extern PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_capabilities)
extern PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_external_semaphore_fd)
extern PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
extern PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
extern PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
extern PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_get_display_properties2)
extern PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
extern PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
extern PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
extern PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_memory_requirements2)
extern PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
extern PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
extern PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_get_physical_device_properties2)
extern PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
extern PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
extern PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
extern PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_maintenance1)
extern PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
extern PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_performance_query)
extern PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
extern PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
extern PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
extern PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_executable_properties)
extern PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
extern PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
extern PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_push_descriptor)
extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
extern PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
extern PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
extern PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_surface)
extern PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_swapchain)
extern PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
extern PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
extern PFN_vkQueuePresentKHR vkQueuePresentKHR;
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_timeline_semaphore)
extern PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
extern PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
extern PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_wayland_surface)
extern PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
extern PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
extern PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
extern PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
extern PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
extern PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
extern PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
extern PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
extern PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
extern PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
extern PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NVX_device_generated_commands)
extern PFN_vkCmdProcessCommandsNVX vkCmdProcessCommandsNVX;
extern PFN_vkCmdReserveSpaceForCommandsNVX vkCmdReserveSpaceForCommandsNVX;
extern PFN_vkCreateIndirectCommandsLayoutNVX vkCreateIndirectCommandsLayoutNVX;
extern PFN_vkCreateObjectTableNVX vkCreateObjectTableNVX;
extern PFN_vkDestroyIndirectCommandsLayoutNVX vkDestroyIndirectCommandsLayoutNVX;
extern PFN_vkDestroyObjectTableNVX vkDestroyObjectTableNVX;
extern PFN_vkGetPhysicalDeviceGeneratedCommandsPropertiesNVX vkGetPhysicalDeviceGeneratedCommandsPropertiesNVX;
extern PFN_vkRegisterObjectsNVX vkRegisterObjectsNVX;
extern PFN_vkUnregisterObjectsNVX vkUnregisterObjectsNVX;
#endif /* defined(VK_NVX_device_generated_commands) */
#if defined(VK_NVX_image_view_handle)
extern PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NV_clip_space_w_scaling)
extern PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_cooperative_matrix)
extern PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_coverage_reduction_mode)
extern PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_device_diagnostic_checkpoints)
extern PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
extern PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_external_memory_capabilities)
extern PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_NV_external_memory_win32)
extern PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_mesh_shader)
extern PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
extern PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
extern PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_ray_tracing)
extern PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
extern PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
extern PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
extern PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
extern PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
extern PFN_vkCompileDeferredNV vkCompileDeferredNV;
extern PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
extern PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
extern PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
extern PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
extern PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
extern PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive)
extern PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
extern PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
extern PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
extern PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#endif /* defined(VK_NV_shading_rate_image) */
#if (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1))
extern PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif /* (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) || (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1))
extern PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
extern PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
extern PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
extern PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* VOLK_GENERATE_PROTOTYPES_H */

#ifdef __cplusplus
}
#endif

#endif

#ifdef VOLK_IMPLEMENTATION
#undef VOLK_IMPLEMENTATION
// Prevent tools like dependency checkers that don't evaluate
// macros from detecting a cyclic dependency.
#define VOLK_SOURCE "volk.c"
#include VOLK_SOURCE
#endif

/**
 * Copyright (c) 2018-2019 Arseny Kapoulkine
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/
