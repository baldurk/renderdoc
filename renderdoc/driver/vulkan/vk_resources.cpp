/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
WRAPPED_POOL_INST(WrappedVkSurfaceKHR)

byte VkResourceRecord::markerValue[32] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0x88, 0x77, 0x66, 0x55, 0x01, 0x23, 0x45, 0x67, 0x98, 0x76, 0x54, 0x32,
};

bool IsDispatchableRes(WrappedVkRes *ptr)
{
  return (WrappedVkPhysicalDevice::IsAlloc(ptr) || WrappedVkInstance::IsAlloc(ptr) ||
          WrappedVkDevice::IsAlloc(ptr) || WrappedVkQueue::IsAlloc(ptr) ||
          WrappedVkCommandBuffer::IsAlloc(ptr));
}

VkResourceType IdentifyTypeByPtr(WrappedVkRes *ptr)
{
  if(WrappedVkPhysicalDevice::IsAlloc(ptr))
    return eResPhysicalDevice;
  if(WrappedVkInstance::IsAlloc(ptr))
    return eResInstance;
  if(WrappedVkDevice::IsAlloc(ptr))
    return eResDevice;
  if(WrappedVkQueue::IsAlloc(ptr))
    return eResQueue;
  if(WrappedVkDeviceMemory::IsAlloc(ptr))
    return eResDeviceMemory;
  if(WrappedVkBuffer::IsAlloc(ptr))
    return eResBuffer;
  if(WrappedVkBufferView::IsAlloc(ptr))
    return eResBufferView;
  if(WrappedVkImage::IsAlloc(ptr))
    return eResImage;
  if(WrappedVkImageView::IsAlloc(ptr))
    return eResImageView;
  if(WrappedVkFramebuffer::IsAlloc(ptr))
    return eResFramebuffer;
  if(WrappedVkRenderPass::IsAlloc(ptr))
    return eResRenderPass;
  if(WrappedVkShaderModule::IsAlloc(ptr))
    return eResShaderModule;
  if(WrappedVkPipelineCache::IsAlloc(ptr))
    return eResPipelineCache;
  if(WrappedVkPipelineLayout::IsAlloc(ptr))
    return eResPipelineLayout;
  if(WrappedVkPipeline::IsAlloc(ptr))
    return eResPipeline;
  if(WrappedVkSampler::IsAlloc(ptr))
    return eResSampler;
  if(WrappedVkDescriptorPool::IsAlloc(ptr))
    return eResDescriptorPool;
  if(WrappedVkDescriptorSetLayout::IsAlloc(ptr))
    return eResDescriptorSetLayout;
  if(WrappedVkDescriptorSet::IsAlloc(ptr))
    return eResDescriptorSet;
  if(WrappedVkCommandPool::IsAlloc(ptr))
    return eResCommandPool;
  if(WrappedVkCommandBuffer::IsAlloc(ptr))
    return eResCommandBuffer;
  if(WrappedVkFence::IsAlloc(ptr))
    return eResFence;
  if(WrappedVkEvent::IsAlloc(ptr))
    return eResEvent;
  if(WrappedVkQueryPool::IsAlloc(ptr))
    return eResQueryPool;
  if(WrappedVkSemaphore::IsAlloc(ptr))
    return eResSemaphore;
  if(WrappedVkSwapchainKHR::IsAlloc(ptr))
    return eResSwapchain;
  if(WrappedVkSurfaceKHR::IsAlloc(ptr))
    return eResSurface;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return eResUnknown;
}

bool IsBlockFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return true;
    default: break;
  }

  return false;
}

bool IsDepthOrStencilFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
    default: break;
  }

  return false;
}

bool IsDepthAndStencilFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
    default: break;
  }

  return false;
}

bool IsStencilFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
    default: break;
  }

  return false;
}

bool IsDepthOnlyFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT: return true;
    default: break;
  }

  return false;
}

bool IsStencilOnlyFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_S8_UINT: return true;
    default: break;
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
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB: return true;
    default: break;
  }

  return false;
}

bool IsUIntFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_S8_UINT: return true;
    default: break;
  }

  return false;
}

bool IsSIntFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64A64_SINT: return true;
    default: break;
  }

  return false;
}

VkFormat GetDepthOnlyFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM;
    case VK_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_X8_D24_UNORM_PACK32;
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT;
    default: break;
  }

  return f;
}

VkFormat GetUIntTypedFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_UINT;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_UINT;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_UINT;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UINT;
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_UINT;
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UINT;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return VK_FORMAT_A8B8G8R8_UINT_PACK32;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32: return VK_FORMAT_A2R10G10B10_UINT_PACK32;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_SINT: return VK_FORMAT_R16_UINT;
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_UINT;
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_SINT: return VK_FORMAT_R16G16B16_UINT;
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_UINT;
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT: return VK_FORMAT_R32_UINT;
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT: return VK_FORMAT_R32G32_UINT;
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_UINT;
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_UINT;
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT: return VK_FORMAT_R64_UINT;
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT: return VK_FORMAT_R64G64_UINT;
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT: return VK_FORMAT_R64G64B64_UINT;
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: return VK_FORMAT_R64G64B64A64_UINT;
    case VK_FORMAT_S8_UINT: return VK_FORMAT_S8_UINT;
    default: break;
  }

  return f;
}

uint32_t GetByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, uint32_t mip)
{
  uint32_t w = RDCMAX(Width >> mip, 1U);
  uint32_t h = RDCMAX(Height >> mip, 1U);
  uint32_t d = RDCMAX(Depth >> mip, 1U);

  uint32_t ret = w * h * d;

  uint32_t astc[2] = {0, 0};

  switch(Format)
  {
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret *= 32; break;
    case VK_FORMAT_R64G64B64_SFLOAT: ret *= 24; break;
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT: ret *= 16; break;
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT: ret *= 12; break;
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
    case VK_FORMAT_R64_SFLOAT: ret *= 8; break;
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT: ret *= 6; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT: ret *= 8; break;
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
    case VK_FORMAT_B8G8R8_SRGB: ret *= 3; break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
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
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
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
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: ret *= 4; break;
    case VK_FORMAT_D16_UNORM_S8_UINT: ret *= 4; break;
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
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16: ret *= 2; break;
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_S8_UINT: ret *= 1; break;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
      ret = AlignUp4(w) * AlignUp4(h) * d;
      ret /= 2;
      break;
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
      ret = AlignUp4(w) * AlignUp4(h) * d;
      ret *= 1;
      break;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
      astc[0] = 4;
      astc[1] = 4;
      break;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
      astc[0] = 5;
      astc[1] = 4;
      break;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
      astc[0] = 5;
      astc[1] = 5;
      break;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
      astc[0] = 6;
      astc[1] = 5;
      break;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
      astc[0] = 6;
      astc[1] = 6;
      break;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
      astc[0] = 8;
      astc[1] = 5;
      break;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
      astc[0] = 8;
      astc[1] = 6;
      break;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
      astc[0] = 8;
      astc[1] = 8;
      break;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
      astc[0] = 10;
      astc[1] = 5;
      break;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
      astc[0] = 10;
      astc[1] = 6;
      break;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
      astc[0] = 10;
      astc[1] = 8;
      break;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
      astc[0] = 10;
      astc[1] = 10;
      break;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
      astc[0] = 12;
      astc[1] = 10;
      break;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
      astc[0] = 12;
      astc[1] = 12;
      break;
    default:
      ret = 1;
      RDCERR("Unrecognised Vulkan Format: %d", Format);
      break;
  }

  if(astc[0] > 0 && astc[1] > 0)
  {
    uint32_t blocks[2] = {(w / astc[0]), (h / astc[1])};

    // how many blocks are needed - including any extra partial blocks
    blocks[0] += (w % astc[0]) ? 1 : 0;
    blocks[1] += (h % astc[1]) ? 1 : 0;

    // ASTC blocks are all 128 bits each
    return blocks[0] * blocks[1] * 16 * d;
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

  if(resType == eResInstance || resType == eResDevice)
    SAFE_DELETE(instDevInfo);

  if(resType == eResSwapchain)
    SAFE_DELETE(swapInfo);

  if(resType == eResDeviceMemory && memMapState)
  {
    Serialiser::FreeAlignedBuffer(memMapState->refData);

    SAFE_DELETE(memMapState);
  }

  if(resType == eResCommandBuffer)
    SAFE_DELETE(cmdInfo);

  if(resType == eResFramebuffer || resType == eResRenderPass)
    SAFE_DELETE_ARRAY(imageAttachments);

  // only the descriptor set layout actually owns this pointer, descriptor sets
  // have a pointer to it but don't own it
  if(resType == eResDescriptorSetLayout)
    SAFE_DELETE(descInfo->layout);

  if(resType == eResDescriptorSetLayout || resType == eResDescriptorSet)
    SAFE_DELETE(descInfo);
}

void SparseMapping::Update(uint32_t numBindings, const VkSparseImageMemoryBind *pBindings)
{
  // update image page table mappings

  for(uint32_t b = 0; b < numBindings; b++)
  {
    const VkSparseImageMemoryBind &newBind = pBindings[b];

    // VKTODOMED handle sparse image arrays or sparse images with mips
    RDCASSERT(newBind.subresource.arrayLayer == 0 && newBind.subresource.mipLevel == 0);

    pair<VkDeviceMemory, VkDeviceSize> *pageTable = pages[newBind.subresource.aspectMask];

    VkOffset3D offsInPages = newBind.offset;
    offsInPages.x /= pagedim.width;
    offsInPages.y /= pagedim.height;
    offsInPages.z /= pagedim.depth;

    VkExtent3D extInPages = newBind.extent;
    extInPages.width /= pagedim.width;
    extInPages.height /= pagedim.height;
    extInPages.depth /= pagedim.depth;

    pair<VkDeviceMemory, VkDeviceSize> mempair = std::make_pair(newBind.memory, newBind.memoryOffset);

    for(uint32_t z = offsInPages.z; z < offsInPages.z + extInPages.depth; z++)
    {
      for(uint32_t y = offsInPages.y; y < offsInPages.y + extInPages.height; y++)
      {
        for(uint32_t x = offsInPages.x; x < offsInPages.x + extInPages.width; x++)
        {
          pageTable[z * imgdim.width * imgdim.height + y * imgdim.width + x] = mempair;
        }
      }
    }
  }
}

void SparseMapping::Update(uint32_t numBindings, const VkSparseMemoryBind *pBindings)
{
  // update opaque mappings

  for(uint32_t b = 0; b < numBindings; b++)
  {
    const VkSparseMemoryBind &curRange = pBindings[b];

    bool found = false;

    // this could be improved to do a binary search since the vector is sorted.
    for(auto it = opaquemappings.begin(); it != opaquemappings.end(); ++it)
    {
      VkSparseMemoryBind &newRange = *it;

      // the binding we're applying is after this item in the list,
      // keep searching
      if(curRange.resourceOffset + curRange.size <= newRange.resourceOffset)
        continue;

      // the binding we're applying is before this item, but doesn't
      // overlap. Insert before us in the list
      if(curRange.resourceOffset >= newRange.resourceOffset + newRange.size)
      {
        opaquemappings.insert(it, newRange);
        found = true;
        break;
      }

      // with sparse mappings it will be reasonably common to update an exact
      // existing range, so check that first
      if(curRange.resourceOffset == newRange.resourceOffset && curRange.size == newRange.size)
      {
        *it = curRange;
        found = true;
        break;
      }

      // handle subranges within the current range
      if(curRange.resourceOffset <= newRange.resourceOffset &&
         curRange.resourceOffset + curRange.size >= newRange.resourceOffset + newRange.size)
      {
        // they start in the same place
        if(curRange.resourceOffset == newRange.resourceOffset)
        {
          // change the current range to be the leftover second half
          it->resourceOffset += curRange.size;

          // insert the new mapping before our current one
          opaquemappings.insert(it, newRange);
          found = true;
          break;
        }
        // they end in the same place
        else if(curRange.resourceOffset + curRange.size == newRange.resourceOffset + newRange.size)
        {
          // save a copy
          VkSparseMemoryBind cur = curRange;

          // set the new size of the first half
          cur.size = newRange.resourceOffset - curRange.resourceOffset;

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
          VkSparseMemoryBind first = curRange;

          // set the new size of the first part
          first.size = newRange.resourceOffset - curRange.resourceOffset;

          // set the current range (third part) to start after the new range ends
          it->resourceOffset = newRange.resourceOffset + newRange.size;

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
      auto endit = it;
      for(; endit != opaquemappings.end(); ++endit)
      {
        if(newRange.resourceOffset + newRange.size <= endit->resourceOffset + endit->size)
          break;
      }

      // see if there are any leftovers of the overlapped ranges at the start or end
      bool leftoverstart = (curRange.resourceOffset < newRange.resourceOffset);
      bool leftoverend =
          (endit != opaquemappings.end() &&
           (endit->resourceOffset + endit->size > newRange.resourceOffset + newRange.size));

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
        VkSparseMemoryBind cur = curRange;

        // modify the size to reflect what's left over
        cur.size = newRange.resourceOffset - cur.resourceOffset;

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
        last->resourceOffset = newRange.resourceOffset + newRange.size;
        // insert the new range before
        opaquemappings.insert(last, newRange);
      }
      // leftovers at both ends
      else
      {
        // save the current range
        VkSparseMemoryBind cur = curRange;

        // modify the size to reflect what's left over
        cur.size = newRange.resourceOffset - cur.resourceOffset;

        // erase up to but not including endit
        auto last = opaquemappings.erase(it, endit);
        // modify the leftovers at the end
        last->resourceOffset = newRange.resourceOffset + newRange.size;
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
