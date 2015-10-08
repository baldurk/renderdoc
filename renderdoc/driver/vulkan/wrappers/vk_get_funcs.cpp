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
    VkImageFormatProperties*                    pImageFormatProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceImageFormatProperties(Unwrap(physicalDevice), format, type, tiling, usage, pImageFormatProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceLimits(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceLimits*                     pLimits)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceLimits(Unwrap(physicalDevice), pLimits);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
	VkResult ret = ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), pProperties);
	
	// assign a random UUID, so that we get SPIR-V instead of cached pipeline data.
	srand((unsigned int)pProperties);
	for(int i=0; i < VK_UUID_LENGTH; i++) pProperties->pipelineCacheUUID[i] = (rand()>>6)&0xff;

	return ret;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueCount(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceQueueCount(Unwrap(physicalDevice), pCount);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    count,
    VkPhysicalDeviceQueueProperties*            pQueueProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceQueueProperties(Unwrap(physicalDevice), count, pQueueProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
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
	return ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), pMemoryRequirements);
}

VkResult WrappedVulkan::vkGetImageMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	return ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), pMemoryRequirements);
}
