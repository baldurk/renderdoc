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

#include "../vk_core.h"

void WrappedVulkan::vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
	ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), pFeatures);
}

void WrappedVulkan::vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
	ObjDisp(physicalDevice)->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), format, pFormatProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          flags,
    VkImageFormatProperties*                    pImageFormatProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceImageFormatProperties(Unwrap(physicalDevice), format, type, tiling, usage, flags, pImageFormatProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceSparseImageFormatProperties(
			VkPhysicalDevice                            physicalDevice,
			VkFormat                                    format,
			VkImageType                                 type,
			VkSampleCountFlagBits                       samples,
			VkImageUsageFlags                           usage,
			VkImageTiling                               tiling,
			uint32_t*                                   pPropertyCount,
			VkSparseImageFormatProperties*              pProperties)
{
	ObjDisp(physicalDevice)->GetPhysicalDeviceSparseImageFormatProperties(Unwrap(physicalDevice), format, type, samples, usage, tiling, pPropertyCount, pProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
	ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), pProperties);
	
	// assign a random UUID, so that we get SPIR-V instead of cached pipeline data.
	srand((unsigned int)(uintptr_t)pProperties);
	for(int i=0; i < VK_UUID_SIZE; i++) pProperties->pipelineCacheUUID[i] = (rand()>>6)&0xff;
}

void WrappedVulkan::vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
	ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), pCount, pQueueFamilyProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
	if(pMemoryProperties)
	{
		*pMemoryProperties = *GetRecord(physicalDevice)->memProps;
		return;
	}

	ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), pMemoryProperties);
}

void WrappedVulkan::vkGetImageSubresourceLayout(
			VkDevice                                    device,
			VkImage                                     image,
			const VkImageSubresource*                   pSubresource,
			VkSubresourceLayout*                        pLayout)
{
	ObjDisp(device)->GetImageSubresourceLayout(Unwrap(device), Unwrap(image), pSubresource, pLayout);
}

void WrappedVulkan::vkGetBufferMemoryRequirements(
		VkDevice                                    device,
		VkBuffer                                    buffer,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), pMemoryRequirements);
	
	// don't do remapping here on replay.
	if(m_State < WRITING)
		return;
	
	uint32_t bits = pMemoryRequirements->memoryTypeBits;
	uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

	pMemoryRequirements->memoryTypeBits = 0;

	// for each of our fake memory indices, check if the real
	// memory type it points to is set - if so, set our fake bit
	for(uint32_t i=0; i < VK_MAX_MEMORY_TYPES; i++)
		if(bits & (1<<memIdxMap[i]) )
			pMemoryRequirements->memoryTypeBits |= (1<<i);
}

void WrappedVulkan::vkGetImageMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), pMemoryRequirements);

	// don't do remapping here on replay.
	if(m_State < WRITING)
		return;
	
	uint32_t bits = pMemoryRequirements->memoryTypeBits;
	uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

	pMemoryRequirements->memoryTypeBits = 0;

	// for each of our fake memory indices, check if the real
	// memory type it points to is set - if so, set our fake bit
	for(uint32_t i=0; i < VK_MAX_MEMORY_TYPES; i++)
		if(bits & (1<<memIdxMap[i]) )
			pMemoryRequirements->memoryTypeBits |= (1<<i);
}

void WrappedVulkan::vkGetImageSparseMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		uint32_t*                                   pNumRequirements,
		VkSparseImageMemoryRequirements*            pSparseMemoryRequirements)
{
	ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(image), pNumRequirements, pSparseMemoryRequirements);
}

void WrappedVulkan::vkGetDeviceMemoryCommitment(
		VkDevice                                    device,
		VkDeviceMemory                              memory,
		VkDeviceSize*                               pCommittedMemoryInBytes)
{
	ObjDisp(device)->GetDeviceMemoryCommitment(Unwrap(device), Unwrap(memory), pCommittedMemoryInBytes);
}

void WrappedVulkan::vkGetRenderAreaGranularity(
		VkDevice                                    device,
		VkRenderPass                                renderPass,
		VkExtent2D*                                 pGranularity)
{
	return ObjDisp(device)->GetRenderAreaGranularity(Unwrap(device), Unwrap(renderPass), pGranularity);
}

VkResult WrappedVulkan::vkGetPipelineCacheData(
	VkDevice                                    device,
	VkPipelineCache                             pipelineCache,
	size_t*                                     pDataSize,
	void*                                       pData)
{
	if(pDataSize)
		*pDataSize = 0;
	// we don't want the application to use pipeline caches at all, and especially
	// don't want to return any data for future use.
	return VK_INCOMPLETE;
}

VkResult WrappedVulkan::vkMergePipelineCaches(
	VkDevice                                    device,
	VkPipelineCache                             destCache,
	uint32_t                                    srcCacheCount,
	const VkPipelineCache*                      pSrcCaches)
{
	// hopefully we'll never get here, because we don't ever return a valid pipeline
	// cache ourselves, our UUID should not match anyone's so the application should
	// not upload a pipeline cache from elsewhere. So there will be nothing to merge,
	// in theory.
	return VK_INCOMPLETE;
}
