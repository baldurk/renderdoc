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
#include "vk_info.h"

WRAPPED_POOL_INST(WrappedVkInstance)
WRAPPED_POOL_INST(WrappedVkPhysicalDevice)
WRAPPED_POOL_INST(WrappedVkDevice)
WRAPPED_POOL_INST(WrappedVkQueue)
WRAPPED_POOL_INST(WrappedVkCommandBuffer)
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
WRAPPED_POOL_INST(WrappedVkPipelineCache)
WRAPPED_POOL_INST(WrappedVkPipelineLayout)
WRAPPED_POOL_INST(WrappedVkRenderPass)
WRAPPED_POOL_INST(WrappedVkPipeline)
WRAPPED_POOL_INST(WrappedVkDescriptorSetLayout)
WRAPPED_POOL_INST(WrappedVkSampler)
WRAPPED_POOL_INST(WrappedVkDescriptorPool)
WRAPPED_POOL_INST(WrappedVkDescriptorSet)
WRAPPED_POOL_INST(WrappedVkFramebuffer)
WRAPPED_POOL_INST(WrappedVkCommandPool)

WRAPPED_POOL_INST(WrappedVkSwapchainKHR)

byte VkResourceRecord::markerValue[32] = {
	0xaa, 0xbb, 0xcc, 0xdd,
	0x88, 0x77, 0x66, 0x55,
	0x01, 0x23, 0x45, 0x67,
	0x98, 0x76, 0x54, 0x32,
};

bool IsDispatchableRes(WrappedVkRes *ptr)
{
	return (WrappedVkPhysicalDevice::IsAlloc(ptr) || WrappedVkInstance::IsAlloc(ptr)
					|| WrappedVkDevice::IsAlloc(ptr) || WrappedVkQueue::IsAlloc(ptr) || WrappedVkCommandBuffer::IsAlloc(ptr));
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
	if(WrappedVkPipelineCache::IsAlloc(ptr))            return eResPipelineCache;
	if(WrappedVkPipelineLayout::IsAlloc(ptr))           return eResPipelineLayout;
	if(WrappedVkPipeline::IsAlloc(ptr))                 return eResPipeline;
	if(WrappedVkSampler::IsAlloc(ptr))                  return eResSampler;
	if(WrappedVkDescriptorPool::IsAlloc(ptr))           return eResDescriptorPool;
	if(WrappedVkDescriptorSetLayout::IsAlloc(ptr))      return eResDescriptorSetLayout;
	if(WrappedVkDescriptorSet::IsAlloc(ptr))            return eResDescriptorSet;
	if(WrappedVkCommandPool::IsAlloc(ptr))              return eResCommandPool;
	if(WrappedVkCommandBuffer::IsAlloc(ptr))            return eResCommandBuffer;
	if(WrappedVkFence::IsAlloc(ptr))                    return eResFence;
	if(WrappedVkEvent::IsAlloc(ptr))                    return eResEvent;
	if(WrappedVkQueryPool::IsAlloc(ptr))                return eResQueryPool;
	if(WrappedVkSemaphore::IsAlloc(ptr))                return eResSemaphore;
	if(WrappedVkSwapchainKHR::IsAlloc(ptr))             return eResSwapchain;

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
		case VK_FORMAT_D24_UNORM_X8:
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

bool IsDepthOnlyFormat(VkFormat f)
{
	switch(f)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM_X8:
		case VK_FORMAT_D32_SFLOAT:
			return true;
		default:
			break;
	}

	return false;
}

bool IsSRGBFormat(VkFormat f)
{
	switch(f)
	{
		case VK_FORMAT_R8_SRGB:
		case VK_FORMAT_R8G8_SRGB:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_ASTC_4x4_SRGB:
		case VK_FORMAT_ASTC_5x4_SRGB:
		case VK_FORMAT_ASTC_5x5_SRGB:
		case VK_FORMAT_ASTC_6x5_SRGB:
		case VK_FORMAT_ASTC_6x6_SRGB:
		case VK_FORMAT_ASTC_8x5_SRGB:
		case VK_FORMAT_ASTC_8x6_SRGB:
		case VK_FORMAT_ASTC_8x8_SRGB:
		case VK_FORMAT_ASTC_10x5_SRGB:
		case VK_FORMAT_ASTC_10x6_SRGB:
		case VK_FORMAT_ASTC_10x8_SRGB:
		case VK_FORMAT_ASTC_10x10_SRGB:
		case VK_FORMAT_ASTC_12x10_SRGB:
		case VK_FORMAT_ASTC_12x12_SRGB:
		case VK_FORMAT_B8G8R8_SRGB:
		case VK_FORMAT_B8G8R8A8_SRGB:
			return true;
		default:
			break;
	}

	return false;
}

uint32_t GetByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, uint32_t mip)
{
	uint32_t w = RDCMAX(Width>>mip, 1U);
	uint32_t d = RDCMAX(Height>>mip, 1U);
	uint32_t h = RDCMAX(Depth>>mip, 1U);

	uint32_t ret = w*h*d;
	
	uint32_t astc[2] = { 0, 0 };

	switch(Format)
	{
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			ret *= 32;
			break;
		case VK_FORMAT_R64G64B64_SFLOAT:
			ret *= 24;
			break;
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
			ret *= 16;
			break;
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT:
			ret *= 12;
			break;
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			ret *= 8;
			break;
		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:
			ret *= 6;
			break;
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8_SRGB:
			ret *= 3;
			break;
		case VK_FORMAT_B10G10R10A2_UNORM:
		case VK_FORMAT_B10G10R10A2_SNORM:
		case VK_FORMAT_B10G10R10A2_USCALED:
		case VK_FORMAT_B10G10R10A2_SSCALED:
		case VK_FORMAT_B10G10R10A2_UINT:
		case VK_FORMAT_B10G10R10A2_SINT:
		case VK_FORMAT_R10G10B10A2_UNORM:
		case VK_FORMAT_R10G10B10A2_SNORM:
		case VK_FORMAT_R10G10B10A2_USCALED:
		case VK_FORMAT_R10G10B10A2_SSCALED:
		case VK_FORMAT_R10G10B10A2_UINT:
		case VK_FORMAT_R10G10B10A2_SINT:
		case VK_FORMAT_R11G11B10_UFLOAT:
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_D24_UNORM_X8:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_R9G9B9E5_UFLOAT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
			ret *= 4;
			break;
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_SRGB:
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_R5G6B5_UNORM:
		case VK_FORMAT_R5G6B5_USCALED:
		case VK_FORMAT_R5G5B5A1_UNORM:
		case VK_FORMAT_R5G5B5A1_USCALED:
		case VK_FORMAT_B5G5R5A1_UNORM:
		case VK_FORMAT_B5G6R5_UNORM:
		case VK_FORMAT_B5G6R5_USCALED:
		case VK_FORMAT_R4G4B4A4_UNORM:
		case VK_FORMAT_R4G4B4A4_USCALED:
		case VK_FORMAT_B4G4R4A4_UNORM:
			ret *= 2;
			break;
		case VK_FORMAT_R4G4_UNORM:
		case VK_FORMAT_R4G4_USCALED:
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_SRGB:
		case VK_FORMAT_S8_UINT:
			ret *= 1;
			break;
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC4_SNORM:
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11_SNORM:
			ret = AlignUp4(w)*AlignUp4(h)*d;
			ret /= 2;
			break;
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC5_SNORM:
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_BC6H_SFLOAT:
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_EAC_R11G11_UNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
			ret = AlignUp4(w)*AlignUp4(h)*d;
			ret *= 1;
			break;
		case VK_FORMAT_ASTC_4x4_UNORM:
		case VK_FORMAT_ASTC_4x4_SRGB:
			astc[0] = 4; astc[1] = 4;
			break;
		case VK_FORMAT_ASTC_5x4_UNORM:
		case VK_FORMAT_ASTC_5x4_SRGB:
			astc[0] = 5; astc[1] = 4;
			break;
		case VK_FORMAT_ASTC_5x5_UNORM:
		case VK_FORMAT_ASTC_5x5_SRGB:
			astc[0] = 5; astc[1] = 5;
			break;
		case VK_FORMAT_ASTC_6x5_UNORM:
		case VK_FORMAT_ASTC_6x5_SRGB:
			astc[0] = 6; astc[1] = 5;
			break;
		case VK_FORMAT_ASTC_6x6_UNORM:
		case VK_FORMAT_ASTC_6x6_SRGB:
			astc[0] = 6; astc[1] = 6;
			break;
		case VK_FORMAT_ASTC_8x5_UNORM:
		case VK_FORMAT_ASTC_8x5_SRGB:
			astc[0] = 8; astc[1] = 5;
			break;
		case VK_FORMAT_ASTC_8x6_UNORM:
		case VK_FORMAT_ASTC_8x6_SRGB:
			astc[0] = 8; astc[1] = 6;
			break;
		case VK_FORMAT_ASTC_8x8_UNORM:
		case VK_FORMAT_ASTC_8x8_SRGB:
			astc[0] = 8; astc[1] = 8;
			break;
		case VK_FORMAT_ASTC_10x5_UNORM:
		case VK_FORMAT_ASTC_10x5_SRGB:
			astc[0] = 10; astc[1] = 5;
			break;
		case VK_FORMAT_ASTC_10x6_UNORM:
		case VK_FORMAT_ASTC_10x6_SRGB:
			astc[0] = 10; astc[1] = 6;
			break;
		case VK_FORMAT_ASTC_10x8_UNORM:
		case VK_FORMAT_ASTC_10x8_SRGB:
			astc[0] = 10; astc[1] = 8;
			break;
		case VK_FORMAT_ASTC_10x10_UNORM:
		case VK_FORMAT_ASTC_10x10_SRGB:
			astc[0] = 10; astc[1] = 10;
			break;
		case VK_FORMAT_ASTC_12x10_UNORM:
		case VK_FORMAT_ASTC_12x10_SRGB:
			astc[0] = 12; astc[1] = 10;
			break;
		case VK_FORMAT_ASTC_12x12_UNORM:
		case VK_FORMAT_ASTC_12x12_SRGB:
			astc[0] = 12; astc[1] = 12;
			break;
		default:
			RDCFATAL("Unrecognised Vulkan Format: %d", Format);
			break;
	}
	
	if(astc[0] > 0 && astc[1] > 0)
	{
		uint32_t blocks[2] = { (w / astc[0]), (h / astc[1]) };

		// how many blocks are needed - including any extra partial blocks
		blocks[0] += (w % astc[0]) ? 1 : 0;
		blocks[1] += (h % astc[1]) ? 1 : 0;

		// ASTC blocks are all 128 bits each
		return blocks[0]*blocks[1]*16*d;
	}

	return ret;
}

VkResourceRecord::~VkResourceRecord()
{
	VkResourceType resType = Resource != NULL ? IdentifyTypeByPtr(Resource) : eResUnknown;
	
	if(resType == eResPhysicalDevice)
		SAFE_DELETE(memProps);

	// bufferviews and imageviews have non-owning pointers to the sparseinfo struct
	if(resType == eResBuffer || resType == eResImage)
		SAFE_DELETE(sparseInfo);

	if(resType == eResSwapchain)
		SAFE_DELETE(swapInfo);

	if(resType == eResDeviceMemory && memMapState)
	{
		Serialiser::FreeAlignedBuffer(memMapState->refData);
		
		SAFE_DELETE(memMapState);
	}

	if(resType == eResCommandBuffer)
		SAFE_DELETE(cmdInfo);

	if(resType == eResFramebuffer)
		SAFE_DELETE(imageAttachments);

	// only the descriptor set layout actually owns this pointer, descriptor sets
	// have a pointer to it but don't own it
	if(resType == eResDescriptorSetLayout)
		SAFE_DELETE(descInfo->layout);
	
	if(resType == eResDescriptorSetLayout || resType == eResDescriptorSet)
		SAFE_DELETE(descInfo);
}

void SparseMapping::Update(uint32_t numBindings, const VkSparseImageMemoryBindInfo *pBindings)
{
	// update image page table mappings

	for(uint32_t b=0; b < numBindings; b++)
	{
		const VkSparseImageMemoryBindInfo &newBind = pBindings[b];

		// VKTODOMED handle sparse image arrays or sparse images with mips
		RDCASSERT(newBind.subresource.arrayLayer == 0 && newBind.subresource.mipLevel == 0);

		pair<VkDeviceMemory,VkDeviceSize> *pageTable = pages[newBind.subresource.aspect];

		VkOffset3D offsInPages = newBind.offset;
		offsInPages.x /= pagedim.width;
		offsInPages.y /= pagedim.height;
		offsInPages.z /= pagedim.depth;

		VkExtent3D extInPages = newBind.extent;
		extInPages.width /= pagedim.width;
		extInPages.height /= pagedim.height;
		extInPages.depth /= pagedim.depth;

		pair<VkDeviceMemory, VkDeviceSize> mempair = std::make_pair(newBind.mem, newBind.memOffset);
		
		for(int32_t z=offsInPages.z; z < offsInPages.z+extInPages.depth; z++)
		{
			for(int32_t y=offsInPages.y; y < offsInPages.y+extInPages.height; y++)
			{
				for(int32_t x=offsInPages.x; x < offsInPages.x+extInPages.width; x++)
				{
					pageTable[ z*imgdim.width*imgdim.height + y*imgdim.width + x ] = mempair;
				}
			}
		}
	}
}

void SparseMapping::Update(uint32_t numBindings, const VkSparseMemoryBindInfo *pBindings)
{
	// update opaque mappings

	for(uint32_t b=0; b < numBindings; b++)
	{
		const VkSparseMemoryBindInfo &curRange = pBindings[b];
			
		bool found = false;

		// this could be improved to do a binary search since the vector is sorted.
		for(auto it=opaquemappings.begin(); it != opaquemappings.end(); ++it)
		{
			VkSparseMemoryBindInfo &newRange = *it;

			// the binding we're applying is after this item in the list,
			// keep searching
			if(curRange.rangeOffset+curRange.rangeSize <= newRange.rangeOffset) continue;

			// the binding we're applying is before this item, but doesn't
			// overlap. Insert before us in the list
			if(curRange.rangeOffset >= newRange.rangeOffset+newRange.rangeSize)
			{
				opaquemappings.insert(it, newRange);
				found = true;
				break;
			}

			// with sparse mappings it will be reasonably common to update an exact
			// existing range, so check that first
			if(curRange.rangeOffset == newRange.rangeOffset && curRange.rangeSize == newRange.rangeSize)
			{
				*it = curRange;
				found = true;
				break;
			}

			// handle subranges within the current range
			if(curRange.rangeOffset <= newRange.rangeOffset && curRange.rangeOffset+curRange.rangeSize >= newRange.rangeOffset+newRange.rangeSize)
			{
				// they start in the same place
				if(curRange.rangeOffset == newRange.rangeOffset)
				{
					// change the current range to be the leftover second half
					it->rangeOffset += curRange.rangeSize;

					// insert the new mapping before our current one
					opaquemappings.insert(it, newRange);
					found = true;
					break;
				}
				// they end in the same place
				else if(curRange.rangeOffset+curRange.rangeSize == newRange.rangeOffset+newRange.rangeSize)
				{
					// save a copy
					VkSparseMemoryBindInfo cur = curRange;

					// set the new size of the first half
					cur.rangeSize = newRange.rangeOffset - curRange.rangeOffset;

					// add the new range where the current iterator was
					*it = newRange;

					// insert the old truncated mapping before our current position
					opaquemappings.insert(it, cur);
					found = true;
					break;
				}
				// the new range is a subsection
				else
				{
					// save a copy
					VkSparseMemoryBindInfo first = curRange;

					// set the new size of the first part
					first.rangeSize = newRange.rangeOffset - curRange.rangeOffset;

					// set the current range (third part) to start after the new range ends
					it->rangeOffset = newRange.rangeOffset+newRange.rangeSize;
					
					// first insert the new range before our current range
					it = opaquemappings.insert(it, newRange);

					// now insert the remaining first part before that
					opaquemappings.insert(it, first);

					found = true;
					break;
				}
			}

			// this new range overlaps the current one and some subsequent ranges. Merge together
			
			// find where this new range stops overlapping
			auto endit=it;
			for(; endit != opaquemappings.end(); ++endit)
			{
				if(newRange.rangeOffset+newRange.rangeSize <= endit->rangeOffset+endit->rangeSize)
					break;
			}

			// see if there are any leftovers of the overlapped ranges at the start or end
			bool leftoverstart = (curRange.rangeOffset < newRange.rangeOffset);
			bool leftoverend = (endit != opaquemappings.end() && (endit->rangeOffset+endit->rangeSize > newRange.rangeOffset+newRange.rangeSize));

			// no leftovers, the new range entirely covers the current and last (if there is one)
			if(!leftoverstart && !leftoverend)
			{
				// erase all of the ranges. If endit points to a valid range,
				// it won't be erased, so we overwrite it. Otherwise it pointed
				// to end() so we just push_back()
				auto last = opaquemappings.erase(it, endit);
				if(last != opaquemappings.end())
					*last = newRange;
				else
					opaquemappings.push_back(newRange);
			}
			// leftover at the start, but not the end
			else if(leftoverstart && !leftoverend)
			{
				// save the current range
				VkSparseMemoryBindInfo cur = curRange;

				// modify the size to reflect what's left over
				cur.rangeSize = newRange.rangeOffset - cur.rangeOffset;

				// as above, erase and either re-insert or push_back()
				auto last = opaquemappings.erase(it, endit);
				if(last != opaquemappings.end())
				{
					*last = newRange;
					opaquemappings.insert(last, cur);
				}
				else
				{
					opaquemappings.push_back(cur);
					opaquemappings.push_back(newRange);
				}
			}
			// leftover at the end, but not the start
			else if(!leftoverstart && leftoverend)
			{
				// erase up to but not including endit
				auto last = opaquemappings.erase(it, endit);
				// modify the leftovers at the end
				last->rangeOffset = newRange.rangeOffset+newRange.rangeSize;
				// insert the new range before
				opaquemappings.insert(last, newRange);
			}
			// leftovers at both ends
			else
			{
				// save the current range
				VkSparseMemoryBindInfo cur = curRange;

				// modify the size to reflect what's left over
				cur.rangeSize = newRange.rangeOffset - cur.rangeOffset;

				// erase up to but not including endit
				auto last = opaquemappings.erase(it, endit);
				// modify the leftovers at the end
				last->rangeOffset = newRange.rangeOffset+newRange.rangeSize;
				// insert the new range before
				auto newit = opaquemappings.insert(last, newRange);
				// insert the modified leftovers before that
				opaquemappings.insert(newit, cur);
			}

			found = true;
			break;
		}

		// if it wasn't found, this binding is after all mappings in our list
		if(!found)
			opaquemappings.push_back(curRange);
	}
}
