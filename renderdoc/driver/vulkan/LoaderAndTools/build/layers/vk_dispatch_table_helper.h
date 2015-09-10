/* THIS FILE IS GENERATED.  DO NOT EDIT. */

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

#include <vulkan.h>
#include <vk_layer.h>
#include <string.h>

static inline void layer_initialize_dispatch_table(VkLayerDispatchTable *table,
                                                   const VkBaseLayerObject *devw)
{
    VkDevice device = (VkDevice) devw->nextObject;
    PFN_vkGetDeviceProcAddr gpa = (PFN_vkGetDeviceProcAddr) devw->pGPA;
    VkDevice baseDevice = (VkDevice) devw->baseObject;
    // GPA has to be first entry inited and uses wrapped object since it triggers init
    memset(table, 0, sizeof(*table));
    table->GetDeviceProcAddr =(PFN_vkGetDeviceProcAddr)  gpa(device,"vkGetDeviceProcAddr");
    table->CreateDevice = (PFN_vkCreateDevice) gpa(baseDevice, "vkCreateDevice");
    table->DestroyDevice = (PFN_vkDestroyDevice) gpa(baseDevice, "vkDestroyDevice");
    table->GetDeviceQueue = (PFN_vkGetDeviceQueue) gpa(baseDevice, "vkGetDeviceQueue");
    table->QueueSubmit = (PFN_vkQueueSubmit) gpa(baseDevice, "vkQueueSubmit");
    table->QueueWaitIdle = (PFN_vkQueueWaitIdle) gpa(baseDevice, "vkQueueWaitIdle");
    table->DeviceWaitIdle = (PFN_vkDeviceWaitIdle) gpa(baseDevice, "vkDeviceWaitIdle");
    table->AllocMemory = (PFN_vkAllocMemory) gpa(baseDevice, "vkAllocMemory");
    table->FreeMemory = (PFN_vkFreeMemory) gpa(baseDevice, "vkFreeMemory");
    table->MapMemory = (PFN_vkMapMemory) gpa(baseDevice, "vkMapMemory");
    table->UnmapMemory = (PFN_vkUnmapMemory) gpa(baseDevice, "vkUnmapMemory");
    table->FlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges) gpa(baseDevice, "vkFlushMappedMemoryRanges");
    table->InvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges) gpa(baseDevice, "vkInvalidateMappedMemoryRanges");
    table->GetDeviceMemoryCommitment = (PFN_vkGetDeviceMemoryCommitment) gpa(baseDevice, "vkGetDeviceMemoryCommitment");
    table->BindBufferMemory = (PFN_vkBindBufferMemory) gpa(baseDevice, "vkBindBufferMemory");
    table->BindImageMemory = (PFN_vkBindImageMemory) gpa(baseDevice, "vkBindImageMemory");
    table->GetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements) gpa(baseDevice, "vkGetBufferMemoryRequirements");
    table->GetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements) gpa(baseDevice, "vkGetImageMemoryRequirements");
    table->GetImageSparseMemoryRequirements = (PFN_vkGetImageSparseMemoryRequirements) gpa(baseDevice, "vkGetImageSparseMemoryRequirements");
    table->QueueBindSparseBufferMemory = (PFN_vkQueueBindSparseBufferMemory) gpa(baseDevice, "vkQueueBindSparseBufferMemory");
    table->QueueBindSparseImageOpaqueMemory = (PFN_vkQueueBindSparseImageOpaqueMemory) gpa(baseDevice, "vkQueueBindSparseImageOpaqueMemory");
    table->QueueBindSparseImageMemory = (PFN_vkQueueBindSparseImageMemory) gpa(baseDevice, "vkQueueBindSparseImageMemory");
    table->CreateFence = (PFN_vkCreateFence) gpa(baseDevice, "vkCreateFence");
    table->DestroyFence = (PFN_vkDestroyFence) gpa(baseDevice, "vkDestroyFence");
    table->ResetFences = (PFN_vkResetFences) gpa(baseDevice, "vkResetFences");
    table->GetFenceStatus = (PFN_vkGetFenceStatus) gpa(baseDevice, "vkGetFenceStatus");
    table->WaitForFences = (PFN_vkWaitForFences) gpa(baseDevice, "vkWaitForFences");
    table->CreateSemaphore = (PFN_vkCreateSemaphore) gpa(baseDevice, "vkCreateSemaphore");
    table->DestroySemaphore = (PFN_vkDestroySemaphore) gpa(baseDevice, "vkDestroySemaphore");
    table->QueueSignalSemaphore = (PFN_vkQueueSignalSemaphore) gpa(baseDevice, "vkQueueSignalSemaphore");
    table->QueueWaitSemaphore = (PFN_vkQueueWaitSemaphore) gpa(baseDevice, "vkQueueWaitSemaphore");
    table->CreateEvent = (PFN_vkCreateEvent) gpa(baseDevice, "vkCreateEvent");
    table->DestroyEvent = (PFN_vkDestroyEvent) gpa(baseDevice, "vkDestroyEvent");
    table->GetEventStatus = (PFN_vkGetEventStatus) gpa(baseDevice, "vkGetEventStatus");
    table->SetEvent = (PFN_vkSetEvent) gpa(baseDevice, "vkSetEvent");
    table->ResetEvent = (PFN_vkResetEvent) gpa(baseDevice, "vkResetEvent");
    table->CreateQueryPool = (PFN_vkCreateQueryPool) gpa(baseDevice, "vkCreateQueryPool");
    table->DestroyQueryPool = (PFN_vkDestroyQueryPool) gpa(baseDevice, "vkDestroyQueryPool");
    table->GetQueryPoolResults = (PFN_vkGetQueryPoolResults) gpa(baseDevice, "vkGetQueryPoolResults");
    table->CreateBuffer = (PFN_vkCreateBuffer) gpa(baseDevice, "vkCreateBuffer");
    table->DestroyBuffer = (PFN_vkDestroyBuffer) gpa(baseDevice, "vkDestroyBuffer");
    table->CreateBufferView = (PFN_vkCreateBufferView) gpa(baseDevice, "vkCreateBufferView");
    table->DestroyBufferView = (PFN_vkDestroyBufferView) gpa(baseDevice, "vkDestroyBufferView");
    table->CreateImage = (PFN_vkCreateImage) gpa(baseDevice, "vkCreateImage");
    table->DestroyImage = (PFN_vkDestroyImage) gpa(baseDevice, "vkDestroyImage");
    table->GetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout) gpa(baseDevice, "vkGetImageSubresourceLayout");
    table->CreateImageView = (PFN_vkCreateImageView) gpa(baseDevice, "vkCreateImageView");
    table->DestroyImageView = (PFN_vkDestroyImageView) gpa(baseDevice, "vkDestroyImageView");
    table->CreateAttachmentView = (PFN_vkCreateAttachmentView) gpa(baseDevice, "vkCreateAttachmentView");
    table->DestroyAttachmentView = (PFN_vkDestroyAttachmentView) gpa(baseDevice, "vkDestroyAttachmentView");
    table->CreateShaderModule = (PFN_vkCreateShaderModule) gpa(baseDevice, "vkCreateShaderModule");
    table->DestroyShaderModule = (PFN_vkDestroyShaderModule) gpa(baseDevice, "vkDestroyShaderModule");
    table->CreateShader = (PFN_vkCreateShader) gpa(baseDevice, "vkCreateShader");
    table->DestroyShader = (PFN_vkDestroyShader) gpa(baseDevice, "vkDestroyShader");
    table->CreatePipelineCache = (PFN_vkCreatePipelineCache) gpa(baseDevice, "vkCreatePipelineCache");
    table->DestroyPipelineCache = (PFN_vkDestroyPipelineCache) gpa(baseDevice, "vkDestroyPipelineCache");
    table->GetPipelineCacheSize = (PFN_vkGetPipelineCacheSize) gpa(baseDevice, "vkGetPipelineCacheSize");
    table->GetPipelineCacheData = (PFN_vkGetPipelineCacheData) gpa(baseDevice, "vkGetPipelineCacheData");
    table->MergePipelineCaches = (PFN_vkMergePipelineCaches) gpa(baseDevice, "vkMergePipelineCaches");
    table->CreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines) gpa(baseDevice, "vkCreateGraphicsPipelines");
    table->CreateComputePipelines = (PFN_vkCreateComputePipelines) gpa(baseDevice, "vkCreateComputePipelines");
    table->DestroyPipeline = (PFN_vkDestroyPipeline) gpa(baseDevice, "vkDestroyPipeline");
    table->CreatePipelineLayout = (PFN_vkCreatePipelineLayout) gpa(baseDevice, "vkCreatePipelineLayout");
    table->DestroyPipelineLayout = (PFN_vkDestroyPipelineLayout) gpa(baseDevice, "vkDestroyPipelineLayout");
    table->CreateSampler = (PFN_vkCreateSampler) gpa(baseDevice, "vkCreateSampler");
    table->DestroySampler = (PFN_vkDestroySampler) gpa(baseDevice, "vkDestroySampler");
    table->CreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout) gpa(baseDevice, "vkCreateDescriptorSetLayout");
    table->DestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout) gpa(baseDevice, "vkDestroyDescriptorSetLayout");
    table->CreateDescriptorPool = (PFN_vkCreateDescriptorPool) gpa(baseDevice, "vkCreateDescriptorPool");
    table->DestroyDescriptorPool = (PFN_vkDestroyDescriptorPool) gpa(baseDevice, "vkDestroyDescriptorPool");
    table->ResetDescriptorPool = (PFN_vkResetDescriptorPool) gpa(baseDevice, "vkResetDescriptorPool");
    table->AllocDescriptorSets = (PFN_vkAllocDescriptorSets) gpa(baseDevice, "vkAllocDescriptorSets");
    table->FreeDescriptorSets = (PFN_vkFreeDescriptorSets) gpa(baseDevice, "vkFreeDescriptorSets");
    table->UpdateDescriptorSets = (PFN_vkUpdateDescriptorSets) gpa(baseDevice, "vkUpdateDescriptorSets");
    table->CreateDynamicViewportState = (PFN_vkCreateDynamicViewportState) gpa(baseDevice, "vkCreateDynamicViewportState");
    table->DestroyDynamicViewportState = (PFN_vkDestroyDynamicViewportState) gpa(baseDevice, "vkDestroyDynamicViewportState");
    table->CreateDynamicRasterState = (PFN_vkCreateDynamicRasterState) gpa(baseDevice, "vkCreateDynamicRasterState");
    table->DestroyDynamicRasterState = (PFN_vkDestroyDynamicRasterState) gpa(baseDevice, "vkDestroyDynamicRasterState");
    table->CreateDynamicColorBlendState = (PFN_vkCreateDynamicColorBlendState) gpa(baseDevice, "vkCreateDynamicColorBlendState");
    table->DestroyDynamicColorBlendState = (PFN_vkDestroyDynamicColorBlendState) gpa(baseDevice, "vkDestroyDynamicColorBlendState");
    table->CreateDynamicDepthStencilState = (PFN_vkCreateDynamicDepthStencilState) gpa(baseDevice, "vkCreateDynamicDepthStencilState");
    table->DestroyDynamicDepthStencilState = (PFN_vkDestroyDynamicDepthStencilState) gpa(baseDevice, "vkDestroyDynamicDepthStencilState");
    table->CreateCommandPool = (PFN_vkCreateCommandPool) gpa(baseDevice, "vkCreateCommandPool");
    table->DestroyCommandPool = (PFN_vkDestroyCommandPool) gpa(baseDevice, "vkDestroyCommandPool");
    table->ResetCommandPool = (PFN_vkResetCommandPool) gpa(baseDevice, "vkResetCommandPool");
    table->CreateCommandBuffer = (PFN_vkCreateCommandBuffer) gpa(baseDevice, "vkCreateCommandBuffer");
    table->DestroyCommandBuffer = (PFN_vkDestroyCommandBuffer) gpa(baseDevice, "vkDestroyCommandBuffer");
    table->BeginCommandBuffer = (PFN_vkBeginCommandBuffer) gpa(baseDevice, "vkBeginCommandBuffer");
    table->EndCommandBuffer = (PFN_vkEndCommandBuffer) gpa(baseDevice, "vkEndCommandBuffer");
    table->ResetCommandBuffer = (PFN_vkResetCommandBuffer) gpa(baseDevice, "vkResetCommandBuffer");
    table->CmdBindPipeline = (PFN_vkCmdBindPipeline) gpa(baseDevice, "vkCmdBindPipeline");
    table->CmdBindDynamicViewportState = (PFN_vkCmdBindDynamicViewportState) gpa(baseDevice, "vkCmdBindDynamicViewportState");
    table->CmdBindDynamicRasterState = (PFN_vkCmdBindDynamicRasterState) gpa(baseDevice, "vkCmdBindDynamicRasterState");
    table->CmdBindDynamicColorBlendState = (PFN_vkCmdBindDynamicColorBlendState) gpa(baseDevice, "vkCmdBindDynamicColorBlendState");
    table->CmdBindDynamicDepthStencilState = (PFN_vkCmdBindDynamicDepthStencilState) gpa(baseDevice, "vkCmdBindDynamicDepthStencilState");
    table->CmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets) gpa(baseDevice, "vkCmdBindDescriptorSets");
    table->CmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer) gpa(baseDevice, "vkCmdBindIndexBuffer");
    table->CmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers) gpa(baseDevice, "vkCmdBindVertexBuffers");
    table->CmdDraw = (PFN_vkCmdDraw) gpa(baseDevice, "vkCmdDraw");
    table->CmdDrawIndexed = (PFN_vkCmdDrawIndexed) gpa(baseDevice, "vkCmdDrawIndexed");
    table->CmdDrawIndirect = (PFN_vkCmdDrawIndirect) gpa(baseDevice, "vkCmdDrawIndirect");
    table->CmdDrawIndexedIndirect = (PFN_vkCmdDrawIndexedIndirect) gpa(baseDevice, "vkCmdDrawIndexedIndirect");
    table->CmdDispatch = (PFN_vkCmdDispatch) gpa(baseDevice, "vkCmdDispatch");
    table->CmdDispatchIndirect = (PFN_vkCmdDispatchIndirect) gpa(baseDevice, "vkCmdDispatchIndirect");
    table->CmdCopyBuffer = (PFN_vkCmdCopyBuffer) gpa(baseDevice, "vkCmdCopyBuffer");
    table->CmdCopyImage = (PFN_vkCmdCopyImage) gpa(baseDevice, "vkCmdCopyImage");
    table->CmdBlitImage = (PFN_vkCmdBlitImage) gpa(baseDevice, "vkCmdBlitImage");
    table->CmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage) gpa(baseDevice, "vkCmdCopyBufferToImage");
    table->CmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer) gpa(baseDevice, "vkCmdCopyImageToBuffer");
    table->CmdUpdateBuffer = (PFN_vkCmdUpdateBuffer) gpa(baseDevice, "vkCmdUpdateBuffer");
    table->CmdFillBuffer = (PFN_vkCmdFillBuffer) gpa(baseDevice, "vkCmdFillBuffer");
    table->CmdClearColorImage = (PFN_vkCmdClearColorImage) gpa(baseDevice, "vkCmdClearColorImage");
    table->CmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage) gpa(baseDevice, "vkCmdClearDepthStencilImage");
    table->CmdClearColorAttachment = (PFN_vkCmdClearColorAttachment) gpa(baseDevice, "vkCmdClearColorAttachment");
    table->CmdClearDepthStencilAttachment = (PFN_vkCmdClearDepthStencilAttachment) gpa(baseDevice, "vkCmdClearDepthStencilAttachment");
    table->CmdResolveImage = (PFN_vkCmdResolveImage) gpa(baseDevice, "vkCmdResolveImage");
    table->CmdSetEvent = (PFN_vkCmdSetEvent) gpa(baseDevice, "vkCmdSetEvent");
    table->CmdResetEvent = (PFN_vkCmdResetEvent) gpa(baseDevice, "vkCmdResetEvent");
    table->CmdWaitEvents = (PFN_vkCmdWaitEvents) gpa(baseDevice, "vkCmdWaitEvents");
    table->CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier) gpa(baseDevice, "vkCmdPipelineBarrier");
    table->CmdBeginQuery = (PFN_vkCmdBeginQuery) gpa(baseDevice, "vkCmdBeginQuery");
    table->CmdEndQuery = (PFN_vkCmdEndQuery) gpa(baseDevice, "vkCmdEndQuery");
    table->CmdResetQueryPool = (PFN_vkCmdResetQueryPool) gpa(baseDevice, "vkCmdResetQueryPool");
    table->CmdWriteTimestamp = (PFN_vkCmdWriteTimestamp) gpa(baseDevice, "vkCmdWriteTimestamp");
    table->CmdCopyQueryPoolResults = (PFN_vkCmdCopyQueryPoolResults) gpa(baseDevice, "vkCmdCopyQueryPoolResults");
    table->CreateFramebuffer = (PFN_vkCreateFramebuffer) gpa(baseDevice, "vkCreateFramebuffer");
    table->DestroyFramebuffer = (PFN_vkDestroyFramebuffer) gpa(baseDevice, "vkDestroyFramebuffer");
    table->CreateRenderPass = (PFN_vkCreateRenderPass) gpa(baseDevice, "vkCreateRenderPass");
    table->DestroyRenderPass = (PFN_vkDestroyRenderPass) gpa(baseDevice, "vkDestroyRenderPass");
    table->GetRenderAreaGranularity = (PFN_vkGetRenderAreaGranularity) gpa(baseDevice, "vkGetRenderAreaGranularity");
    table->CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass) gpa(baseDevice, "vkCmdBeginRenderPass");
    table->CmdNextSubpass = (PFN_vkCmdNextSubpass) gpa(baseDevice, "vkCmdNextSubpass");
    table->CmdPushConstants = (PFN_vkCmdPushConstants) gpa(baseDevice, "vkCmdPushConstants");
    table->CmdEndRenderPass = (PFN_vkCmdEndRenderPass) gpa(baseDevice, "vkCmdEndRenderPass");
    table->CmdExecuteCommands = (PFN_vkCmdExecuteCommands) gpa(baseDevice, "vkCmdExecuteCommands");
    table->GetSurfaceInfoWSI = (PFN_vkGetSurfaceInfoWSI) gpa(baseDevice, "vkGetSurfaceInfoWSI");
    table->CreateSwapChainWSI = (PFN_vkCreateSwapChainWSI) gpa(baseDevice, "vkCreateSwapChainWSI");
    table->DestroySwapChainWSI = (PFN_vkDestroySwapChainWSI) gpa(baseDevice, "vkDestroySwapChainWSI");
    table->GetSwapChainInfoWSI = (PFN_vkGetSwapChainInfoWSI) gpa(baseDevice, "vkGetSwapChainInfoWSI");
    table->AcquireNextImageWSI = (PFN_vkAcquireNextImageWSI) gpa(baseDevice, "vkAcquireNextImageWSI");
    table->QueuePresentWSI = (PFN_vkQueuePresentWSI) gpa(baseDevice, "vkQueuePresentWSI");
}

static inline void layer_init_instance_dispatch_table(VkLayerInstanceDispatchTable *table,
                                                   const VkBaseLayerObject *instw)
{
    VkInstance instance = (VkInstance) instw->nextObject;
    PFN_vkGetInstanceProcAddr gpa = (PFN_vkGetInstanceProcAddr) instw->pGPA;
    VkInstance baseInstance = (VkInstance) instw->baseObject;
    // GPA has to be first entry inited and uses wrapped object since it triggers init
    memset(table, 0, sizeof(*table));
    table->GetInstanceProcAddr =(PFN_vkGetInstanceProcAddr)  gpa(instance,"vkGetInstanceProcAddr");
    table->CreateInstance = (PFN_vkCreateInstance) gpa(baseInstance, "vkCreateInstance");
    table->DestroyInstance = (PFN_vkDestroyInstance) gpa(baseInstance, "vkDestroyInstance");
    table->EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices) gpa(baseInstance, "vkEnumeratePhysicalDevices");
    table->GetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures) gpa(baseInstance, "vkGetPhysicalDeviceFeatures");
    table->GetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties) gpa(baseInstance, "vkGetPhysicalDeviceFormatProperties");
    table->GetPhysicalDeviceImageFormatProperties = (PFN_vkGetPhysicalDeviceImageFormatProperties) gpa(baseInstance, "vkGetPhysicalDeviceImageFormatProperties");
    table->GetPhysicalDeviceLimits = (PFN_vkGetPhysicalDeviceLimits) gpa(baseInstance, "vkGetPhysicalDeviceLimits");
    table->GetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties) gpa(baseInstance, "vkGetPhysicalDeviceProperties");
    table->GetPhysicalDeviceQueueCount = (PFN_vkGetPhysicalDeviceQueueCount) gpa(baseInstance, "vkGetPhysicalDeviceQueueCount");
    table->GetPhysicalDeviceQueueProperties = (PFN_vkGetPhysicalDeviceQueueProperties) gpa(baseInstance, "vkGetPhysicalDeviceQueueProperties");
    table->GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties) gpa(baseInstance, "vkGetPhysicalDeviceMemoryProperties");
    table->GetPhysicalDeviceExtensionProperties = (PFN_vkGetPhysicalDeviceExtensionProperties) gpa(baseInstance, "vkGetPhysicalDeviceExtensionProperties");
    table->GetPhysicalDeviceLayerProperties = (PFN_vkGetPhysicalDeviceLayerProperties) gpa(baseInstance, "vkGetPhysicalDeviceLayerProperties");
    table->GetPhysicalDeviceSparseImageFormatProperties = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties) gpa(baseInstance, "vkGetPhysicalDeviceSparseImageFormatProperties");
    table->GetPhysicalDeviceSurfaceSupportWSI = (PFN_vkGetPhysicalDeviceSurfaceSupportWSI) gpa(baseInstance, "vkGetPhysicalDeviceSurfaceSupportWSI");
}
