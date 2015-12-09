/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#define HookInitVulkanInstance() \
	HookInit(CreateInstance); \
	HookInit(DestroyInstance); \
	HookInit(EnumeratePhysicalDevices); \
	HookInit(GetPhysicalDeviceFeatures); \
	HookInit(GetPhysicalDeviceImageFormatProperties); \
	HookInit(GetPhysicalDeviceFormatProperties); \
	HookInit(GetPhysicalDeviceSparseImageFormatProperties); \
	HookInit(GetPhysicalDeviceProperties); \
	HookInit(GetPhysicalDeviceQueueFamilyProperties); \
	HookInit(GetPhysicalDeviceMemoryProperties); \
	HookInit(DbgCreateMsgCallback); \
	HookInit(DbgDestroyMsgCallback); \
	HookInit(GetPhysicalDeviceSurfaceSupportKHR)

#define HookInitVulkanDevice() \
	HookInit(CreateDevice); \
	HookInit(DestroyDevice); \
	HookInit(GetDeviceQueue); \
	HookInit(QueueSubmit); \
	HookInit(QueueWaitIdle); \
	HookInit(DeviceWaitIdle); \
	HookInit(AllocMemory); \
	HookInit(FreeMemory); \
	HookInit(MapMemory); \
	HookInit(UnmapMemory); \
	HookInit(FlushMappedMemoryRanges); \
	HookInit(InvalidateMappedMemoryRanges); \
	HookInit(GetDeviceMemoryCommitment); \
	HookInit(BindBufferMemory); \
	HookInit(BindImageMemory); \
	HookInit(QueueBindSparseBufferMemory); \
	HookInit(QueueBindSparseImageOpaqueMemory); \
	HookInit(QueueBindSparseImageMemory); \
	HookInit(CreateBuffer); \
	HookInit(DestroyBuffer); \
	HookInit(CreateBufferView); \
	HookInit(DestroyBufferView); \
	HookInit(CreateImage); \
	HookInit(DestroyImage); \
	HookInit(GetImageSubresourceLayout); \
	HookInit(GetBufferMemoryRequirements); \
	HookInit(GetImageMemoryRequirements); \
	HookInit(GetImageSparseMemoryRequirements); \
	HookInit(CreateImageView); \
	HookInit(DestroyImageView); \
	HookInit(CreateShader); \
	HookInit(DestroyShader); \
	HookInit(CreateShaderModule); \
	HookInit(DestroyShaderModule); \
	HookInit(CreateGraphicsPipelines); \
	HookInit(CreateComputePipelines); \
	HookInit(DestroyPipeline); \
	HookInit(CreatePipelineCache); \
	HookInit(GetPipelineCacheSize); \
	HookInit(GetPipelineCacheData); \
	HookInit(MergePipelineCaches); \
	HookInit(DestroyPipelineCache); \
	HookInit(CreatePipelineLayout); \
	HookInit(DestroyPipelineLayout); \
	HookInit(CreateSemaphore); \
	HookInit(DestroySemaphore); \
	HookInit(QueueSignalSemaphore); \
	HookInit(QueueWaitSemaphore); \
	HookInit(CreateFence); \
	HookInit(GetFenceStatus); \
	HookInit(ResetFences); \
	HookInit(WaitForFences); \
	HookInit(DestroyFence); \
	HookInit(CreateEvent); \
	HookInit(GetEventStatus); \
	HookInit(ResetEvent); \
	HookInit(SetEvent); \
	HookInit(DestroyEvent); \
	HookInit(CreateQueryPool); \
	HookInit(GetQueryPoolResults); \
	HookInit(DestroyQueryPool); \
	HookInit(CreateSampler); \
	HookInit(DestroySampler); \
	HookInit(CreateDescriptorSetLayout); \
	HookInit(DestroyDescriptorSetLayout); \
	HookInit(CreateDescriptorPool); \
	HookInit(ResetDescriptorPool); \
	HookInit(DestroyDescriptorPool); \
	HookInit(AllocDescriptorSets); \
	HookInit(UpdateDescriptorSets); \
	HookInit(FreeDescriptorSets); \
	HookInit(GetRenderAreaGranularity); \
	HookInit(CreateCommandPool); \
	HookInit(DestroyCommandPool); \
	HookInit(ResetCommandPool); \
	HookInit(CreateCommandBuffer); \
	HookInit(DestroyCommandBuffer); \
	HookInit(BeginCommandBuffer); \
	HookInit(EndCommandBuffer); \
	HookInit(ResetCommandBuffer); \
	HookInit(CmdBindPipeline); \
	HookInit(CmdSetViewport); \
	HookInit(CmdSetScissor); \
	HookInit(CmdSetLineWidth); \
	HookInit(CmdSetDepthBias); \
	HookInit(CmdSetBlendConstants); \
	HookInit(CmdSetDepthBounds); \
	HookInit(CmdSetStencilCompareMask); \
	HookInit(CmdSetStencilWriteMask); \
	HookInit(CmdSetStencilReference); \
	HookInit(CmdBindDescriptorSets); \
	HookInit(CmdBindVertexBuffers); \
	HookInit(CmdBindIndexBuffer); \
	HookInit(CmdDraw); \
	HookInit(CmdDrawIndirect); \
	HookInit(CmdDrawIndexed); \
	HookInit(CmdDrawIndexedIndirect); \
	HookInit(CmdDispatch); \
	HookInit(CmdDispatchIndirect); \
	HookInit(CmdCopyBufferToImage); \
	HookInit(CmdCopyImageToBuffer); \
	HookInit(CmdCopyBuffer); \
	HookInit(CmdCopyImage); \
	HookInit(CmdBlitImage); \
	HookInit(CmdResolveImage); \
	HookInit(CmdUpdateBuffer); \
	HookInit(CmdFillBuffer); \
	HookInit(CmdPushConstants); \
	HookInit(CmdClearColorImage); \
	HookInit(CmdClearDepthStencilImage); \
	HookInit(CmdClearColorAttachment); \
	HookInit(CmdClearDepthStencilAttachment); \
	HookInit(CmdPipelineBarrier); \
	HookInit(CmdWriteTimestamp); \
	HookInit(CmdCopyQueryPoolResults); \
	HookInit(CmdBeginQuery); \
	HookInit(CmdEndQuery); \
	HookInit(CmdResetQueryPool); \
	HookInit(CmdSetEvent); \
	HookInit(CmdResetEvent); \
	HookInit(CmdWaitEvents); \
	HookInit(CreateFramebuffer); \
	HookInit(DestroyFramebuffer); \
	HookInit(CreateRenderPass); \
	HookInit(DestroyRenderPass); \
	HookInit(CmdBeginRenderPass); \
	HookInit(CmdNextSubpass); \
	HookInit(CmdExecuteCommands); \
	HookInit(CmdEndRenderPass); \
	HookInit(GetSurfacePropertiesKHR); \
	HookInit(GetSurfaceFormatsKHR); \
	HookInit(GetSurfacePresentModesKHR); \
	HookInit(CreateSwapchainKHR); \
	HookInit(DestroySwapchainKHR); \
	HookInit(GetSwapchainImagesKHR); \
	HookInit(AcquireNextImageKHR); \
	HookInit(QueuePresentKHR);

#define DefineHooks() \
	HookDefine3(VkResult, vkEnumeratePhysicalDevices, VkInstance, instance, uint32_t*, pPhysicalDeviceCount, VkPhysicalDevice*, pPhysicalDevices); \
	HookDefine2(VkResult, vkGetPhysicalDeviceFeatures, VkPhysicalDevice, physicalDevice, VkPhysicalDeviceFeatures*, pFeatures); \
	HookDefine3(VkResult, vkGetPhysicalDeviceFormatProperties, VkPhysicalDevice, physicalDevice, VkFormat, format, VkFormatProperties*, pFormatProperties); \
	HookDefine7(VkResult, vkGetPhysicalDeviceImageFormatProperties, VkPhysicalDevice, physicalDevice, VkFormat, format, VkImageType, type, VkImageTiling, tiling, VkImageUsageFlags, usage, VkImageCreateFlags, flags, VkImageFormatProperties*, pImageFormatProperties); \
	HookDefine8(VkResult, vkGetPhysicalDeviceSparseImageFormatProperties, VkPhysicalDevice, physicalDevice, VkFormat, format, VkImageType, type, uint32_t, samples, VkImageUsageFlags, usage, VkImageTiling, tiling, uint32_t*, pNumProperties, VkSparseImageFormatProperties*, pProperties); \
	HookDefine2(VkResult, vkGetPhysicalDeviceProperties, VkPhysicalDevice, physicalDevice, VkPhysicalDeviceProperties*, pProperties); \
	HookDefine3(VkResult, vkGetPhysicalDeviceQueueFamilyProperties, VkPhysicalDevice, physicalDevice, uint32_t*, pCount, VkQueueFamilyProperties*, pQueueFamilyProperties); \
	HookDefine2(VkResult, vkGetPhysicalDeviceMemoryProperties, VkPhysicalDevice, physicalDevice, VkPhysicalDeviceMemoryProperties*, pMemoryProperties); \
	HookDefine3(VkResult, vkCreateDevice, VkPhysicalDevice, physicalDevice, const VkDeviceCreateInfo*, pCreateInfo, VkDevice*, pDevice); \
	HookDefine1(void, vkDestroyDevice, VkDevice, device); \
	HookDefine4(VkResult, vkGetDeviceQueue, VkDevice, device, uint32_t, queueFamilyIndex, uint32_t, queueIndex, VkQueue*, pQueue); \
	HookDefine4(VkResult, vkQueueSubmit, VkQueue, queue, uint32_t, cmdBufferCount, const VkCommandBuffer*, pCmdBuffers, VkFence, fence); \
	HookDefine1(VkResult, vkQueueWaitIdle, VkQueue, queue); \
	HookDefine1(VkResult, vkDeviceWaitIdle, VkDevice, device); \
	HookDefine3(VkResult, vkAllocMemory, VkDevice, device, const VkMemoryAllocInfo*, pAllocInfo, VkDeviceMemory*, pMem); \
	HookDefine2(void, vkFreeMemory, VkDevice, device, VkDeviceMemory, mem); \
	HookDefine6(VkResult, vkMapMemory, VkDevice, device, VkDeviceMemory, mem, VkDeviceSize, offset, VkDeviceSize, size, VkMemoryMapFlags, flags, void**, ppData); \
	HookDefine2(void, vkUnmapMemory, VkDevice, device, VkDeviceMemory, mem); \
	HookDefine3(VkResult, vkFlushMappedMemoryRanges, VkDevice, device, uint32_t, memRangeCount, const VkMappedMemoryRange*, pMemRanges); \
	HookDefine3(VkResult, vkInvalidateMappedMemoryRanges, VkDevice, device, uint32_t, memRangeCount, const VkMappedMemoryRange*, pMemRanges); \
	HookDefine3(VkResult, vkGetDeviceMemoryCommitment, VkDevice, device, VkDeviceMemory, memory, VkDeviceSize*, pCommittedMemoryInBytes); \
	HookDefine4(VkResult, vkBindBufferMemory, VkDevice, device, VkBuffer, buffer, VkDeviceMemory, mem, VkDeviceSize, memOffset); \
	HookDefine4(VkResult, vkBindImageMemory, VkDevice, device, VkImage, image, VkDeviceMemory, mem, VkDeviceSize, memOffset); \
	HookDefine4(VkResult, vkQueueBindSparseBufferMemory, VkQueue, queue, VkBuffer, buffer, uint32_t, numBindings, const VkSparseMemoryBindInfo*, pBindInfo); \
	HookDefine4(VkResult, vkQueueBindSparseImageOpaqueMemory, VkQueue, queue, VkImage, image, uint32_t, numBindings, const VkSparseMemoryBindInfo*, pBindInfo); \
	HookDefine4(VkResult, vkQueueBindSparseImageMemory, VkQueue, queue, VkImage, image, uint32_t, numBindings, const VkSparseImageMemoryBindInfo*, pBindInfo); \
	HookDefine3(VkResult, vkCreateBuffer, VkDevice, device, const VkBufferCreateInfo*, pCreateInfo, VkBuffer*, pBuffer); \
	HookDefine2(void, vkDestroyBuffer, VkDevice, device, VkBuffer, buffer); \
	HookDefine3(VkResult, vkCreateBufferView, VkDevice, device, const VkBufferViewCreateInfo*, pCreateInfo, VkBufferView*, pView); \
	HookDefine2(void, vkDestroyBufferView, VkDevice, device, VkBufferView, bufferView); \
	HookDefine3(VkResult, vkCreateImage, VkDevice, device, const VkImageCreateInfo*, pCreateInfo, VkImage*, pImage); \
	HookDefine2(void, vkDestroyImage, VkDevice, device, VkImage, image); \
	HookDefine4(VkResult, vkGetImageSubresourceLayout, VkDevice, device, VkImage, image, const VkImageSubresource*, pSubresource, VkSubresourceLayout*, pLayout); \
	HookDefine3(VkResult, vkGetBufferMemoryRequirements, VkDevice, device, VkBuffer, buffer, VkMemoryRequirements*, pMemoryRequirements); \
	HookDefine3(VkResult, vkGetImageMemoryRequirements, VkDevice, device, VkImage, image, VkMemoryRequirements*, pMemoryRequirements); \
	HookDefine4(VkResult, vkGetImageSparseMemoryRequirements, VkDevice, device, VkImage, image, uint32_t*, pNumRequirements, VkSparseImageMemoryRequirements*, pSparseMemoryRequirements); \
	HookDefine3(VkResult, vkCreateImageView, VkDevice, device, const VkImageViewCreateInfo*, pCreateInfo, VkImageView*, pView); \
	HookDefine2(void, vkDestroyImageView, VkDevice, device, VkImageView, imageView); \
	HookDefine3(VkResult, vkCreateShader, VkDevice, device, const VkShaderCreateInfo*, pCreateInfo, VkShader*, pShader); \
	HookDefine2(void, vkDestroyShader, VkDevice, device, VkShader, shader); \
	HookDefine3(VkResult, vkCreateShaderModule, VkDevice, device, const VkShaderModuleCreateInfo*, pCreateInfo, VkShaderModule*, pShaderModule); \
	HookDefine2(void, vkDestroyShaderModule, VkDevice, device, VkShaderModule, shaderModule); \
	HookDefine5(VkResult, vkCreateGraphicsPipelines, VkDevice, device, VkPipelineCache, pipelineCache, uint32_t, count, const VkGraphicsPipelineCreateInfo*, pCreateInfos, VkPipeline*, pPipelines); \
	HookDefine5(VkResult, vkCreateComputePipelines, VkDevice, device, VkPipelineCache, pipelineCache, uint32_t, count, const VkComputePipelineCreateInfo*, pCreateInfos, VkPipeline*, pPipelines); \
	HookDefine2(void, vkDestroyPipeline, VkDevice, device, VkPipeline, pipeline); \
	HookDefine3(VkResult, vkCreatePipelineCache, VkDevice, device, const VkPipelineCacheCreateInfo*, pCreateInfo, VkPipelineCache*, pPipelineCache); \
	HookDefine2(void, vkDestroyPipelineCache, VkDevice, device, VkPipelineCache, pipelineCache); \
	HookDefine2(size_t, vkGetPipelineCacheSize, VkDevice, device, VkPipelineCache, pipelineCache); \
	HookDefine3(VkResult, vkGetPipelineCacheData, VkDevice, device, VkPipelineCache, pipelineCache, void*, pData); \
	HookDefine4(VkResult, vkMergePipelineCaches, VkDevice, device, VkPipelineCache, pipelineCache, uint32_t, srcCacheCount, const VkPipelineCache*, pSrcCaches); \
	HookDefine3(VkResult, vkCreatePipelineLayout, VkDevice, device, const VkPipelineLayoutCreateInfo*, pCreateInfo, VkPipelineLayout*, pPipelineLayout); \
	HookDefine2(void, vkDestroyPipelineLayout, VkDevice, device, VkPipelineLayout, pipelineLayout); \
	HookDefine3(VkResult, vkCreateSemaphore, VkDevice, device, const VkSemaphoreCreateInfo*, pCreateInfo, VkSemaphore*, pSemaphore); \
	HookDefine2(void, vkDestroySemaphore, VkDevice, device, VkSemaphore, semaphore); \
	HookDefine2(VkResult, vkQueueSignalSemaphore, VkQueue, queue, VkSemaphore, semaphore); \
	HookDefine2(VkResult, vkQueueWaitSemaphore, VkQueue, queue, VkSemaphore, semaphore); \
	HookDefine3(VkResult, vkCreateFence, VkDevice, device, const VkFenceCreateInfo*, pCreateInfo, VkFence*, pFence); \
	HookDefine2(void, vkDestroyFence, VkDevice, device, VkFence, fence); \
	HookDefine3(VkResult, vkCreateEvent, VkDevice, device, const VkEventCreateInfo*, pCreateInfo, VkEvent*, pEvent); \
	HookDefine2(void, vkDestroyEvent, VkDevice, device, VkEvent, event); \
	HookDefine2(VkResult, vkGetEventStatus, VkDevice, device, VkEvent, event); \
	HookDefine2(VkResult, vkSetEvent, VkDevice, device, VkEvent, event); \
	HookDefine2(VkResult, vkResetEvent, VkDevice, device, VkEvent, event); \
	HookDefine3(VkResult, vkCreateQueryPool, VkDevice, device, const VkQueryPoolCreateInfo*, pCreateInfo, VkQueryPool*, pQueryPool); \
	HookDefine2(void, vkDestroyQueryPool, VkDevice, device, VkQueryPool, queryPool); \
	HookDefine7(VkResult, vkGetQueryPoolResults, VkDevice, device, VkQueryPool, queryPool, uint32_t, startQuery, uint32_t, queryCount, size_t*, pDataSize, void*, pData, VkQueryResultFlags, flags); \
	HookDefine2(VkResult, vkGetFenceStatus, VkDevice, device, VkFence, fence); \
	HookDefine3(VkResult, vkResetFences, VkDevice, device, uint32_t, fenceCount, const VkFence*, pFences); \
	HookDefine5(VkResult, vkWaitForFences, VkDevice, device, uint32_t, fenceCount, const VkFence*, pFences, VkBool32, waitAll, uint64_t, timeout); \
	HookDefine3(VkResult, vkCreateSampler, VkDevice, device, const VkSamplerCreateInfo*, pCreateInfo, VkSampler*, pSampler); \
	HookDefine2(void, vkDestroySampler, VkDevice, device, VkSampler, sampler); \
	HookDefine3(VkResult, vkCreateDescriptorSetLayout, VkDevice, device, const VkDescriptorSetLayoutCreateInfo*, pCreateInfo, VkDescriptorSetLayout*, pSetLayout); \
	HookDefine2(void, vkDestroyDescriptorSetLayout, VkDevice, device, VkDescriptorSetLayout, descriptorSetLayout); \
	HookDefine3(VkResult, vkCreateDescriptorPool, VkDevice, device, const VkDescriptorPoolCreateInfo*, pCreateInfo, VkDescriptorPool*, pDescriptorPool); \
	HookDefine2(void, vkDestroyDescriptorPool, VkDevice, device, VkDescriptorPool, descriptorPool); \
	HookDefine2(VkResult, vkResetDescriptorPool, VkDevice, device, VkDescriptorPool, descriptorPool); \
	HookDefine6(VkResult, vkAllocDescriptorSets, VkDevice, device, VkDescriptorPool, descriptorPool, VkDescriptorSetUsage, setUsage, uint32_t, count, const VkDescriptorSetLayout*, pSetLayouts, VkDescriptorSet*, pDescriptorSets); \
	HookDefine5(void, vkUpdateDescriptorSets, VkDevice, device, uint32_t, writeCount, const VkWriteDescriptorSet*, pDescriptorWrites, uint32_t, copyCount, const VkCopyDescriptorSet*, pDescriptorCopies); \
	HookDefine4(VkResult, vkFreeDescriptorSets, VkDevice, device, VkDescriptorPool, descriptorPool, uint32_t, count, const VkDescriptorSet*, pDescriptorSets); \
	HookDefine3(VkResult, vkCreateCommandPool, VkDevice, device, const VkCmdPoolCreateInfo*, pCreateInfo, VkCmdPool*, pCmdPool); \
	HookDefine2(void, vkDestroyCommandPool, VkDevice, device, VkCmdPool, cmdPool); \
	HookDefine3(VkResult, vkResetCommandPool, VkDevice, device, VkCmdPool, cmdPool, VkCmdPoolResetFlags, flags); \
	HookDefine3(VkResult, vkCreateCommandBuffer, VkDevice, device, const VkCmdBufferCreateInfo*, pCreateInfo, VkCommandBuffer*, pCmdBuffer); \
	HookDefine2(void, vkDestroyCommandBuffer, VkDevice, device, VkCommandBuffer, cmdBuffer); \
	HookDefine2(VkResult, vkBeginCommandBuffer, VkCommandBuffer, cmdBuffer, const VkCmdBufferBeginInfo*, pBeginInfo); \
	HookDefine1(VkResult, vkEndCommandBuffer, VkCommandBuffer, cmdBuffer); \
	HookDefine2(VkResult, vkResetCommandBuffer, VkCommandBuffer, cmdBuffer, VkCmdBufferResetFlags, flags); \
	HookDefine3(void, vkCmdBindPipeline, VkCommandBuffer, cmdBuffer, VkPipelineBindPoint, pipelineBindPoint, VkPipeline, pipeline); \
	HookDefine3(void, vkCmdSetViewport, VkCommandBuffer, cmdBuffer, uint32_t, viewportCount, const VkViewport*, pViewports); \
	HookDefine3(void, vkCmdSetScissor, VkCommandBuffer, cmdBuffer, uint32_t, scissorCount, const VkRect2D*, pScissors); \
	HookDefine2(void, vkCmdSetLineWidth, VkCommandBuffer, cmdBuffer, float, lineWidth); \
	HookDefine4(void, vkCmdSetDepthBias, VkCommandBuffer, cmdBuffer, float, depthBias, float, depthBiasClamp, float, slopeScaledDepthBias); \
	HookDefine2(void, vkCmdSetBlendConstants, VkCommandBuffer, cmdBuffer, const float*, blendConst); \
	HookDefine3(void, vkCmdSetDepthBounds, VkCommandBuffer, cmdBuffer, float, minDepthBounds, float, maxDepthBounds); \
	HookDefine3(void, vkCmdSetStencilCompareMask, VkCommandBuffer, cmdBuffer, VkStencilFaceFlags, faceMask, uint32_t, stencilCompareMask); \
	HookDefine3(void, vkCmdSetStencilWriteMask, VkCommandBuffer, cmdBuffer, VkStencilFaceFlags, faceMask, uint32_t, stencilWriteMask); \
	HookDefine3(void, vkCmdSetStencilReference, VkCommandBuffer, cmdBuffer, VkStencilFaceFlags, faceMask, uint32_t, stencilReference); \
	HookDefine8(void, vkCmdBindDescriptorSets, VkCommandBuffer, cmdBuffer, VkPipelineBindPoint, pipelineBindPoint, VkPipelineLayout, layout, uint32_t, firstSet, uint32_t, setCount, const VkDescriptorSet*, pDescriptorSets, uint32_t, dynamicOffsetCount, const uint32_t*, pDynamicOffsets); \
	HookDefine4(void, vkCmdBindIndexBuffer, VkCommandBuffer, cmdBuffer, VkBuffer, buffer, VkDeviceSize, offset, VkIndexType, indexType); \
	HookDefine5(void, vkCmdBindVertexBuffers, VkCommandBuffer, cmdBuffer, uint32_t, startBinding, uint32_t, bindingCount, const VkBuffer*, pBuffers, const VkDeviceSize*, pOffsets); \
	HookDefine5(void, vkCmdDraw, VkCommandBuffer, cmdBuffer, uint32_t, vertexCount, uint32_t, instanceCount, uint32_t, firstVertex, uint32_t, firstInstance); \
	HookDefine6(void, vkCmdDrawIndexed, VkCommandBuffer, cmdBuffer, uint32_t, indexCount, uint32_t, instanceCount, uint32_t, firstIndex, int32_t, vertexOffset, uint32_t, firstInstance); \
	HookDefine5(void, vkCmdDrawIndirect, VkCommandBuffer, cmdBuffer, VkBuffer, buffer, VkDeviceSize, offset, uint32_t, count, uint32_t, stride); \
	HookDefine5(void, vkCmdDrawIndexedIndirect, VkCommandBuffer, cmdBuffer, VkBuffer, buffer, VkDeviceSize, offset, uint32_t, count, uint32_t, stride); \
	HookDefine4(void, vkCmdDispatch, VkCommandBuffer, cmdBuffer, uint32_t, x, uint32_t, y, uint32_t, z); \
	HookDefine3(void, vkCmdDispatchIndirect, VkCommandBuffer, cmdBuffer, VkBuffer, buffer, VkDeviceSize, offset); \
	HookDefine6(void, vkCmdCopyBufferToImage, VkCommandBuffer, cmdBuffer, VkBuffer, srcBuffer, VkImage, destImage, VkImageLayout, destImageLayout, uint32_t, regionCount, const VkBufferImageCopy*, pRegions); \
	HookDefine6(void, vkCmdCopyImageToBuffer, VkCommandBuffer, cmdBuffer, VkImage, srcImage, VkImageLayout, srcImageLayout, VkBuffer, destBuffer, uint32_t, regionCount, const VkBufferImageCopy*, pRegions); \
	HookDefine5(void, vkCmdCopyBuffer, VkCommandBuffer, cmdBuffer, VkBuffer, srcBuffer, VkBuffer, destBuffer, uint32_t, regionCount, const VkBufferCopy*, pRegions); \
	HookDefine7(void, vkCmdCopyImage, VkCommandBuffer, cmdBuffer, VkImage, srcImage, VkImageLayout, srcImageLayout, VkImage, destImage, VkImageLayout, destImageLayout, uint32_t, regionCount, const VkImageCopy*, pRegions); \
	HookDefine8(void, vkCmdBlitImage, VkCommandBuffer, cmdBuffer, VkImage, srcImage, VkImageLayout, srcImageLayout, VkImage, destImage, VkImageLayout, destImageLayout, uint32_t, regionCount, const VkImageBlit*, pRegions, VkTexFilter, filter); \
	HookDefine7(void, vkCmdResolveImage, VkCommandBuffer, cmdBuffer, VkImage, srcImage, VkImageLayout, srcImageLayout, VkImage, destImage, VkImageLayout, destImageLayout, uint32_t, regionCount, const VkImageResolve*, pRegions); \
	HookDefine5(void, vkCmdUpdateBuffer, VkCommandBuffer, cmdBuffer, VkBuffer, destBuffer, VkDeviceSize, destOffset, VkDeviceSize, dataSize, const uint32_t*, pData); \
	HookDefine5(void, vkCmdFillBuffer, VkCommandBuffer, cmdBuffer, VkBuffer, destBuffer, VkDeviceSize, destOffset, VkDeviceSize, fillSize, uint32_t, data); \
	HookDefine6(void, vkCmdPushConstants, VkCommandBuffer, cmdBuffer, VkPipelineLayout, layout, VkShaderStageFlags, stageFlags, uint32_t, start, uint32_t, length, const void*, values); \
	HookDefine6(void, vkCmdClearColorImage, VkCommandBuffer, cmdBuffer, VkImage, image, VkImageLayout, imageLayout, const VkClearColorValue*, pColor, uint32_t, rangeCount, const VkImageSubresourceRange*, pRanges); \
	HookDefine6(void, vkCmdClearDepthStencilImage, VkCommandBuffer, cmdBuffer, VkImage, image, VkImageLayout, imageLayout, const VkClearDepthStencilValue*, pDepthStencil, uint32_t, rangeCount, const VkImageSubresourceRange*, pRanges); \
	HookDefine6(void, vkCmdClearColorAttachment, VkCommandBuffer, cmdBuffer, uint32_t, colorAttachment, VkImageLayout, imageLayout, const VkClearColorValue*, pColor, uint32_t, rectCount, const VkRect3D*, pRects); \
	HookDefine6(void, vkCmdClearDepthStencilAttachment, VkCommandBuffer, cmdBuffer, VkImageAspectFlags, aspectMask, VkImageLayout, imageLayout, const VkClearDepthStencilValue*, pDepthStencil, uint32_t, rectCount, const VkRect3D*, pRects); \
	HookDefine6(void, vkCmdPipelineBarrier, VkCommandBuffer, cmdBuffer, VkPipelineStageFlags, srcStageMask, VkPipelineStageFlags, destStageMask, VkBool32, byRegion, uint32_t, memBarrierCount, const void* const*, ppMemBarriers); \
	HookDefine4(void, vkCmdWriteTimestamp, VkCommandBuffer, cmdBuffer, VkTimestampType, timestampType, VkBuffer, destBuffer, VkDeviceSize, destOffset); \
	HookDefine8(void, vkCmdCopyQueryPoolResults, VkCommandBuffer, cmdBuffer, VkQueryPool, queryPool, uint32_t, startQuery, uint32_t, queryCount, VkBuffer, destBuffer, VkDeviceSize, destOffset, VkDeviceSize, destStride, VkQueryResultFlags, flags); \
	HookDefine4(void, vkCmdBeginQuery, VkCommandBuffer, cmdBuffer, VkQueryPool, queryPool, uint32_t, slot, VkQueryControlFlags, flags); \
	HookDefine3(void, vkCmdEndQuery, VkCommandBuffer, cmdBuffer, VkQueryPool, queryPool, uint32_t, slot); \
	HookDefine4(void, vkCmdResetQueryPool, VkCommandBuffer, cmdBuffer, VkQueryPool, queryPool, uint32_t, startQuery, uint32_t, queryCount); \
	HookDefine3(void, vkCmdSetEvent, VkCommandBuffer, cmdBuffer, VkEvent, event, VkPipelineStageFlags, stageMask); \
	HookDefine3(void, vkCmdResetEvent, VkCommandBuffer, cmdBuffer, VkEvent, event, VkPipelineStageFlags, stageMask); \
	HookDefine7(void, vkCmdWaitEvents, VkCommandBuffer, cmdBuffer, uint32_t, eventCount, const VkEvent*, pEvents, VkPipelineStageFlags, srcStageMask, VkPipelineStageFlags, destStageMask, uint32_t, memBarrierCount, const void* const*, ppMemBarriers); \
	HookDefine3(VkResult, vkCreateFramebuffer, VkDevice, device, const VkFramebufferCreateInfo*, pCreateInfo, VkFramebuffer*, pFramebuffer); \
	HookDefine2(void, vkDestroyFramebuffer, VkDevice, device, VkFramebuffer, framebuffer); \
	HookDefine3(VkResult, vkCreateRenderPass, VkDevice, device, const VkRenderPassCreateInfo*, pCreateInfo, VkRenderPass*, pRenderPass); \
	HookDefine2(void, vkDestroyRenderPass, VkDevice, device, VkRenderPass, renderPass); \
	HookDefine3(VkResult, vkGetRenderAreaGranularity, VkDevice, device, VkRenderPass, renderPass, VkExtent2D*, pGranularity); \
	HookDefine3(void, vkCmdBeginRenderPass, VkCommandBuffer, cmdBuffer, const VkRenderPassBeginInfo*, pRenderPassBegin, VkRenderPassContents, contents); \
	HookDefine2(void, vkCmdNextSubpass, VkCommandBuffer, cmdBuffer, VkRenderPassContents, contents); \
	HookDefine3(void, vkCmdExecuteCommands, VkCommandBuffer, cmdBuffer, uint32_t, cmdBuffersCount, const VkCommandBuffer*, pCmdBuffers); \
	HookDefine1(void, vkCmdEndRenderPass, VkCommandBuffer, cmdBuffer); \
	HookDefine5(VkResult, vkDbgCreateMsgCallback, VkInstance, instance, VkFlags, msgFlags, const PFN_vkDbgMsgCallback, pfnMsgCallback, void*, pUserData, VkDbgMsgCallback*, pMsgCallback); \
	HookDefine2(VkResult, vkDbgDestroyMsgCallback, VkInstance, instance, VkDbgMsgCallback, msgCallback); \
	HookDefine4(VkResult, vkGetPhysicalDeviceSurfaceSupportKHR, VkPhysicalDevice, physicalDevice, uint32_t, queueFamilyIndex, const VkSurfaceDescriptionKHR*, pSurfaceDescription, VkBool32*, pSupported); \
	HookDefine3(VkResult, vkGetSurfacePropertiesKHR, VkDevice, device, const VkSurfaceDescriptionKHR*, pSurfaceDescription, VkSurfacePropertiesKHR*, pSurfaceProperties); \
	HookDefine4(VkResult, vkGetSurfaceFormatsKHR, VkDevice, device, const VkSurfaceDescriptionKHR*, pSurfaceDescription, uint32_t*, pCount, VkSurfaceFormatKHR*, pSurfaceFormats); \
	HookDefine4(VkResult, vkGetSurfacePresentModesKHR, VkDevice, device, const VkSurfaceDescriptionKHR*, pSurfaceDescription, uint32_t*, pCount, VkPresentModeKHR*, pPresentModes); \
	HookDefine3(VkResult, vkCreateSwapchainKHR, VkDevice, device, const VkSwapchainCreateInfoKHR*, pCreateInfo, VkSwapchainKHR*, pSwapchain); \
	HookDefine2(VkResult, vkDestroySwapchainKHR, VkDevice, device, VkSwapchainKHR, swapchain); \
	HookDefine4(VkResult, vkGetSwapchainImagesKHR, VkDevice, device, VkSwapchainKHR, swapchain, uint32_t*, pCount, VkImage*, pSwapchainImages); \
	HookDefine5(VkResult, vkAcquireNextImageKHR, VkDevice, device, VkSwapchainKHR, swapchain, uint64_t, timeout, VkSemaphore, semaphore, uint32_t*, pImageIndex); \
	HookDefine2(VkResult, vkQueuePresentKHR, VkQueue, queue, VkPresentInfoKHR*, pPresentInfo);
