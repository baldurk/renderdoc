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

VkResult WrappedVulkan::vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), pFeatures);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), format, pFormatProperties);
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

VkResult WrappedVulkan::vkGetPhysicalDeviceSparseImageFormatProperties(
			VkPhysicalDevice                            physicalDevice,
			VkFormat                                    format,
			VkImageType                                 type,
			uint32_t                                    samples,
			VkImageUsageFlags                           usage,
			VkImageTiling                               tiling,
			uint32_t*                                   pNumProperties,
			VkSparseImageFormatProperties*              pProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSparseImageFormatProperties(Unwrap(physicalDevice), format, type, samples, usage, tiling, pNumProperties, pProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
	VkResult ret = ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), pProperties);
	
	// assign a random UUID, so that we get SPIR-V instead of cached pipeline data.
	srand((unsigned int)(uintptr_t)pProperties);
	for(int i=0; i < VK_UUID_LENGTH; i++) pProperties->pipelineCacheUUID[i] = (rand()>>6)&0xff;

	return ret;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), pCount, pQueueFamilyProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
	if(pMemoryProperties)
	{
		*pMemoryProperties = *GetRecord(physicalDevice)->memProps;
		return VK_SUCCESS;
	}

	return ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), pMemoryProperties);
}

VkResult WrappedVulkan::vkGetImageSubresourceLayout(
			VkDevice                                    device,
			VkImage                                     image,
			const VkImageSubresource*                   pSubresource,
			VkSubresourceLayout*                        pLayout)
{
	return ObjDisp(device)->GetImageSubresourceLayout(Unwrap(device), Unwrap(image), pSubresource, pLayout);
}

VkResult WrappedVulkan::vkGetBufferMemoryRequirements(
		VkDevice                                    device,
		VkBuffer                                    buffer,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	VkResult vkr = ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), pMemoryRequirements);
	
	// don't do remapping here on replay.
	if(m_State < WRITING)
		return vkr;
	
	uint32_t bits = pMemoryRequirements->memoryTypeBits;
	uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

	pMemoryRequirements->memoryTypeBits = 0;

	// for each of our fake memory indices, check if the real
	// memory type it points to is set - if so, set our fake bit
	for(uint32_t i=0; i < VK_MAX_MEMORY_TYPES; i++)
		if(bits & (1<<memIdxMap[i]) )
			pMemoryRequirements->memoryTypeBits |= (1<<i);

	return vkr;
}

VkResult WrappedVulkan::vkGetImageMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	VkResult vkr = ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), pMemoryRequirements);

	// don't do remapping here on replay.
	if(m_State < WRITING)
		return vkr;
	
	uint32_t bits = pMemoryRequirements->memoryTypeBits;
	uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

	pMemoryRequirements->memoryTypeBits = 0;

	// for each of our fake memory indices, check if the real
	// memory type it points to is set - if so, set our fake bit
	for(uint32_t i=0; i < VK_MAX_MEMORY_TYPES; i++)
		if(bits & (1<<memIdxMap[i]) )
			pMemoryRequirements->memoryTypeBits |= (1<<i);

	return vkr;
}

VkResult WrappedVulkan::vkGetImageSparseMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		uint32_t*                                   pNumRequirements,
		VkSparseImageMemoryRequirements*            pSparseMemoryRequirements)
{
	return ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(image), pNumRequirements, pSparseMemoryRequirements);
}

VkResult WrappedVulkan::vkGetRenderAreaGranularity(
		VkDevice                                    device,
		VkRenderPass                                renderPass,
		VkExtent2D*                                 pGranularity)
{
	return ObjDisp(device)->GetRenderAreaGranularity(Unwrap(device), Unwrap(renderPass), pGranularity);
}

size_t WrappedVulkan::vkGetPipelineCacheSize(
	VkDevice                                    device,
	VkPipelineCache                             pipelineCache)
{
	// we don't want the application to use pipeline caches at all, and especially
	// don't want to return any data for future use.
	return 0;
}

VkResult WrappedVulkan::vkGetPipelineCacheData(
	VkDevice                                    device,
	VkPipelineCache                             pipelineCache,
	void*                                       pData)
{
	// we don't want the application to use pipeline caches at all, and especially
	// don't want to return any data for future use.
	return VK_UNSUPPORTED;
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
	return VK_UNSUPPORTED;
}
