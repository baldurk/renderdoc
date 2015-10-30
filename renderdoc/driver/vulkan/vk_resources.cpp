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
	for(size_t i=0; i < descBindings.size(); i++)
		delete[] descBindings[i];
	descBindings.clear();

	SAFE_DELETE(memProps);

	SAFE_DELETE(layout);
	SAFE_DELETE(swapInfo);
	SAFE_DELETE(cmdInfo);
}
