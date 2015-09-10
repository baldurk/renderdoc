/* Need to define dispatch table
 * Core struct can then have ptr to dispatch table at the top
 * Along with object ptrs for current and next OBJ
 */
#pragma once

#include "vulkan.h"
#include "vk_debug_report_lunarg.h"
#include "vk_debug_marker_lunarg.h"
#include "vk_wsi_swapchain.h"
#include "vk_wsi_device_swapchain.h"
#if defined(__GNUC__) && __GNUC__ >= 4
#  define VK_LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#  define VK_LAYER_EXPORT __attribute__((visibility("default")))
#else
#  define VK_LAYER_EXPORT
#endif

typedef void * (*PFN_vkGPA)(void* obj, const char * pName);

typedef struct VkBaseLayerObject_
{
    PFN_vkGPA pGPA;
    void* nextObject;
    void* baseObject;
} VkBaseLayerObject;

typedef struct VkLayerDispatchTable_
{
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkCreateDevice CreateDevice;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkAllocMemory AllocMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkQueueBindSparseBufferMemory QueueBindSparseBufferMemory;
    PFN_vkQueueBindSparseImageOpaqueMemory QueueBindSparseImageOpaqueMemory;
    PFN_vkQueueBindSparseImageMemory QueueBindSparseImageMemory;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkGetFenceStatus GetFenceStatus;
    PFN_vkResetFences ResetFences;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkQueueSignalSemaphore QueueSignalSemaphore;
    PFN_vkQueueWaitSemaphore QueueWaitSemaphore;
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
    PFN_vkCreateAttachmentView CreateAttachmentView;
    PFN_vkDestroyAttachmentView DestroyAttachmentView;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreateShader CreateShader;
    PFN_vkDestroyShader DestroyShader;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;
    PFN_vkGetPipelineCacheSize GetPipelineCacheSize;
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
    PFN_vkAllocDescriptorSets AllocDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkCreateDynamicViewportState CreateDynamicViewportState;
    PFN_vkDestroyDynamicViewportState DestroyDynamicViewportState;
    PFN_vkCreateDynamicRasterState CreateDynamicRasterState;
    PFN_vkDestroyDynamicRasterState DestroyDynamicRasterState;
    PFN_vkCreateDynamicColorBlendState CreateDynamicColorBlendState;
    PFN_vkDestroyDynamicColorBlendState DestroyDynamicColorBlendState;
    PFN_vkCreateDynamicDepthStencilState CreateDynamicDepthStencilState;
    PFN_vkDestroyDynamicDepthStencilState DestroyDynamicDepthStencilState;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkCreateCommandBuffer CreateCommandBuffer;
    PFN_vkDestroyCommandBuffer DestroyCommandBuffer;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdBindDynamicViewportState CmdBindDynamicViewportState;
    PFN_vkCmdBindDynamicRasterState CmdBindDynamicRasterState;
    PFN_vkCmdBindDynamicColorBlendState CmdBindDynamicColorBlendState;
    PFN_vkCmdBindDynamicDepthStencilState CmdBindDynamicDepthStencilState;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
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
    PFN_vkCmdClearColorAttachment CmdClearColorAttachment;
    PFN_vkCmdClearDepthStencilAttachment CmdClearDepthStencilAttachment;
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
    PFN_vkGetSurfaceInfoWSI GetSurfaceInfoWSI;
    PFN_vkCreateSwapChainWSI CreateSwapChainWSI;
    PFN_vkDestroySwapChainWSI DestroySwapChainWSI;
    PFN_vkGetSwapChainInfoWSI GetSwapChainInfoWSI;
    PFN_vkAcquireNextImageWSI AcquireNextImageWSI;
    PFN_vkQueuePresentWSI QueuePresentWSI;
    PFN_vkDbgCreateMsgCallback DbgCreateMsgCallback;
    PFN_vkDbgDestroyMsgCallback DbgDestroyMsgCallback;
} VkLayerDispatchTable;

typedef struct VkLayerInstanceDispatchTable_
{
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkCreateInstance CreateInstance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceLimits GetPhysicalDeviceLimits;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueCount GetPhysicalDeviceQueueCount;
    PFN_vkGetPhysicalDeviceQueueProperties GetPhysicalDeviceQueueProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceExtensionProperties GetPhysicalDeviceExtensionProperties;
    PFN_vkGetPhysicalDeviceLayerProperties GetPhysicalDeviceLayerProperties;
    PFN_vkGetPhysicalDeviceSurfaceSupportWSI GetPhysicalDeviceSurfaceSupportWSI;
    PFN_vkDbgCreateMsgCallback DbgCreateMsgCallback;
    PFN_vkDbgDestroyMsgCallback DbgDestroyMsgCallback;
} VkLayerInstanceDispatchTable;

// LL node for tree of dbg callback functions
typedef struct VkLayerDbgFunctionNode_
{
    VkDbgMsgCallback msgCallback;
    PFN_vkDbgMsgCallback pfnMsgCallback;
    VkFlags msgFlags;
    const void *pUserData;
    struct VkLayerDbgFunctionNode_ *pNext;
} VkLayerDbgFunctionNode;

typedef enum VkLayerDbgAction_
{
    VK_DBG_LAYER_ACTION_IGNORE = 0x0,
    VK_DBG_LAYER_ACTION_CALLBACK = 0x1,
    VK_DBG_LAYER_ACTION_LOG_MSG = 0x2,
    VK_DBG_LAYER_ACTION_BREAK = 0x4
} VkLayerDbgAction;

// ------------------------------------------------------------------------------------------------
// API functions
