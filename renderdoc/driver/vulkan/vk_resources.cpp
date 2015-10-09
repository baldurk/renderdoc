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

#include "vk_resources.h"

WRAPPED_POOL_INST(WrappedVkInstance)
WRAPPED_POOL_INST(WrappedVkPhysicalDevice)
WRAPPED_POOL_INST(WrappedVkDevice)
WRAPPED_POOL_INST(WrappedVkQueue)
WRAPPED_POOL_INST(WrappedVkCmdBuffer)
WRAPPED_POOL_INST(WrappedVkFence)
WRAPPED_POOL_INST(WrappedVkDeviceMemory)
WRAPPED_POOL_INST(WrappedVkBuffer)
WRAPPED_POOL_INST(WrappedVkImage)
WRAPPED_POOL_INST(WrappedVkSemaphore)
WRAPPED_POOL_INST(WrappedVkEvent)
WRAPPED_POOL_INST(WrappedVkQueryPool)
WRAPPED_POOL_INST(WrappedVkBufferView)
WRAPPED_POOL_INST(WrappedVkImageView)
WRAPPED_POOL_INST(WrappedVkShaderModule)
WRAPPED_POOL_INST(WrappedVkShader)
WRAPPED_POOL_INST(WrappedVkPipelineCache)
WRAPPED_POOL_INST(WrappedVkPipelineLayout)
WRAPPED_POOL_INST(WrappedVkRenderPass)
WRAPPED_POOL_INST(WrappedVkPipeline)
WRAPPED_POOL_INST(WrappedVkDescriptorSetLayout)
WRAPPED_POOL_INST(WrappedVkSampler)
WRAPPED_POOL_INST(WrappedVkDescriptorPool)
WRAPPED_POOL_INST(WrappedVkDescriptorSet)
WRAPPED_POOL_INST(WrappedVkFramebuffer)
WRAPPED_POOL_INST(WrappedVkCmdPool)

WRAPPED_POOL_INST(WrappedVkSwapChainWSI)

bool IsDispatchableRes(WrappedVkRes *ptr)
{
	return (WrappedVkPhysicalDevice::IsAlloc(ptr) || WrappedVkInstance::IsAlloc(ptr)
					|| WrappedVkDevice::IsAlloc(ptr) || WrappedVkQueue::IsAlloc(ptr) || WrappedVkCmdBuffer::IsAlloc(ptr));
}

VkResourceType IdentifyTypeByPtr(WrappedVkRes *ptr)
{
	if(WrappedVkPhysicalDevice::IsAlloc(ptr))           return eResPhysicalDevice;
	if(WrappedVkInstance::IsAlloc(ptr))                 return eResInstance;
	if(WrappedVkDevice::IsAlloc(ptr))                   return eResDevice;
	if(WrappedVkQueue::IsAlloc(ptr))                    return eResQueue;
	if(WrappedVkDeviceMemory::IsAlloc(ptr))             return eResDeviceMemory;
	if(WrappedVkBuffer::IsAlloc(ptr))                   return eResBuffer;
	if(WrappedVkBufferView::IsAlloc(ptr))               return eResBufferView;
	if(WrappedVkImage::IsAlloc(ptr))                    return eResImage;
	if(WrappedVkImageView::IsAlloc(ptr))                return eResImageView;
	if(WrappedVkFramebuffer::IsAlloc(ptr))              return eResFramebuffer;
	if(WrappedVkRenderPass::IsAlloc(ptr))               return eResRenderPass;
	if(WrappedVkShaderModule::IsAlloc(ptr))             return eResShaderModule;
	if(WrappedVkShader::IsAlloc(ptr))                   return eResShader;
	if(WrappedVkPipelineCache::IsAlloc(ptr))            return eResPipelineCache;
	if(WrappedVkPipelineLayout::IsAlloc(ptr))           return eResPipelineLayout;
	if(WrappedVkPipeline::IsAlloc(ptr))                 return eResPipeline;
	if(WrappedVkSampler::IsAlloc(ptr))                  return eResSampler;
	if(WrappedVkDescriptorPool::IsAlloc(ptr))           return eResDescriptorPool;
	if(WrappedVkDescriptorSetLayout::IsAlloc(ptr))      return eResDescriptorSetLayout;
	if(WrappedVkDescriptorSet::IsAlloc(ptr))            return eResDescriptorSet;
	if(WrappedVkCmdPool::IsAlloc(ptr))                  return eResCmdPool;
	if(WrappedVkCmdBuffer::IsAlloc(ptr))                return eResCmdBuffer;
	if(WrappedVkFence::IsAlloc(ptr))                    return eResFence;
	if(WrappedVkEvent::IsAlloc(ptr))                    return eResEvent;
	if(WrappedVkQueryPool::IsAlloc(ptr))                return eResQueryPool;
	if(WrappedVkSemaphore::IsAlloc(ptr))                return eResSemaphore;
	if(WrappedVkSwapChainWSI::IsAlloc(ptr))             return eResWSISwapChain;

	RDCERR("Unknown type for ptr 0x%p", ptr);

	return eResUnknown;
}

bool IsBlockFormat(VkFormat f)
{
	switch(f)
	{
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC4_SNORM:
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC5_SNORM:
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_BC6H_SFLOAT:
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11_SNORM:
		case VK_FORMAT_EAC_R11G11_UNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
		case VK_FORMAT_ASTC_4x4_UNORM:
		case VK_FORMAT_ASTC_4x4_SRGB:
		case VK_FORMAT_ASTC_5x4_UNORM:
		case VK_FORMAT_ASTC_5x4_SRGB:
		case VK_FORMAT_ASTC_5x5_UNORM:
		case VK_FORMAT_ASTC_5x5_SRGB:
		case VK_FORMAT_ASTC_6x5_UNORM:
		case VK_FORMAT_ASTC_6x5_SRGB:
		case VK_FORMAT_ASTC_6x6_UNORM:
		case VK_FORMAT_ASTC_6x6_SRGB:
		case VK_FORMAT_ASTC_8x5_UNORM:
		case VK_FORMAT_ASTC_8x5_SRGB:
		case VK_FORMAT_ASTC_8x6_UNORM:
		case VK_FORMAT_ASTC_8x6_SRGB:
		case VK_FORMAT_ASTC_8x8_UNORM:
		case VK_FORMAT_ASTC_8x8_SRGB:
		case VK_FORMAT_ASTC_10x5_UNORM:
		case VK_FORMAT_ASTC_10x5_SRGB:
		case VK_FORMAT_ASTC_10x6_UNORM:
		case VK_FORMAT_ASTC_10x6_SRGB:
		case VK_FORMAT_ASTC_10x8_UNORM:
		case VK_FORMAT_ASTC_10x8_SRGB:
		case VK_FORMAT_ASTC_10x10_UNORM:
		case VK_FORMAT_ASTC_10x10_SRGB:
		case VK_FORMAT_ASTC_12x10_UNORM:
		case VK_FORMAT_ASTC_12x10_SRGB:
		case VK_FORMAT_ASTC_12x12_UNORM:
		case VK_FORMAT_ASTC_12x12_SRGB:
			return true;
		default:
			break;
	}

	return false;
}

bool IsDepthStencilFormat(VkFormat f)
{
	switch(f)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_S8_UINT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			break;
	}

	return false;
}
