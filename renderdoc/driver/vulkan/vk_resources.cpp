/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
WRAPPED_POOL_INST(WrappedVkDescriptorUpdateTemplate)
WRAPPED_POOL_INST(WrappedVkSamplerYcbcrConversion)

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
  if(WrappedVkDescriptorUpdateTemplate::IsAlloc(ptr))
    return eResDescUpdateTemplate;
  if(WrappedVkSamplerYcbcrConversion::IsAlloc(ptr))
    return eResSamplerConversion;

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
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return true;
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
    case VK_FORMAT_D24_UNORM_S8_UINT:
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
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
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
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return true;
    default: break;
  }

  return false;
}

bool IsDoubleFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64A64_UINT: return true;
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
    case VK_FORMAT_R64G64B64A64_UINT: return true;
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

bool IsYUVFormat(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return true;
    default: break;
  }

  return false;
}

uint32_t GetYUVPlaneCount(VkFormat f)
{
  switch(f)
  {
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return 2;
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return 3;
    default: break;
  }

  return 1;
}

uint32_t GetYUVNumRows(VkFormat f, uint32_t height)
{
  switch(f)
  {
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
      // all of these are 4:2:0, so number of rows is equal to height + height/2
      return height + height / 2;
    default: break;
  }

  return height;
}

VkFormat GetYUVViewPlaneFormat(VkFormat f, uint32_t plane)
{
  switch(f)
  {
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM: return f;
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return VK_FORMAT_R8_UNORM;
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
      return plane == 0 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8_UNORM;
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return VK_FORMAT_R10X6_UNORM_PACK16;
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
      return plane == 0 ? VK_FORMAT_R10X6_UNORM_PACK16 : VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return VK_FORMAT_R12X4_UNORM_PACK16;
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
      return plane == 0 ? VK_FORMAT_R12X4_UNORM_PACK16 : VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return VK_FORMAT_R16_UNORM;
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
      return plane == 0 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16G16_UNORM;
    default: break;
  }

  return f;
}

void GetYUVShaderParameters(VkFormat f, Vec4u &YUVDownsampleRate, Vec4u &YUVAChannels)
{
  if(IsYUVFormat(f))
  {
    ResourceFormat fmt = MakeResourceFormat(f);

    switch(fmt.YUVSubsampling())
    {
      case 444:
        YUVDownsampleRate.x = 1;
        YUVDownsampleRate.y = 1;
        break;
      case 422:
        YUVDownsampleRate.x = 2;
        YUVDownsampleRate.y = 1;
        break;
      case 420:
        YUVDownsampleRate.x = 2;
        YUVDownsampleRate.y = 2;
        break;
      default: break;
    }
    YUVDownsampleRate.z = fmt.YUVPlaneCount();
    switch(fmt.type)
    {
      case ResourceFormatType::YUV8: YUVDownsampleRate.w = 8; break;
      case ResourceFormatType::YUV10: YUVDownsampleRate.w = 10; break;
      case ResourceFormatType::YUV12: YUVDownsampleRate.w = 12; break;
      case ResourceFormatType::YUV16: YUVDownsampleRate.w = 16; break;
      default: break;
    }
    switch(f)
    {
      case VK_FORMAT_G8B8G8R8_422_UNORM: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_B8G8R8G8_422_UNORM: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: YUVAChannels = {0, 4, 5, 0xff}; break;
      case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: YUVAChannels = {0, 4, 8, 0xff}; break;
      case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: YUVAChannels = {0, 4, 5, 0xff}; break;
      case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: YUVAChannels = {0, 4, 8, 0xff}; break;
      case VK_FORMAT_R10X6_UNORM_PACK16: YUVAChannels = {0, 0xff, 0xff, 0xff}; break;
      case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: YUVAChannels = {0xff, 0, 1, 0xff}; break;
      case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: YUVAChannels = {1, 2, 0, 3}; break;
      case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        YUVAChannels = {0, 4, 8, 0xff};
        break;
      case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        YUVAChannels = {0, 4, 5, 0xff};
        break;
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        YUVAChannels = {0, 4, 8, 0xff};
        break;
      case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        YUVAChannels = {0, 4, 5, 0xff};
        break;
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        YUVAChannels = {0, 4, 8, 0xff};
        break;
      case VK_FORMAT_R12X4_UNORM_PACK16: YUVAChannels = {0, 0xff, 0xff, 0xff}; break;
      case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: YUVAChannels = {0xff, 0, 1, 0xff}; break;
      case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: YUVAChannels = {1, 2, 0, 3}; break;
      case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        YUVAChannels = {0, 4, 8, 0xff};
        break;
      case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        YUVAChannels = {0, 4, 5, 0xff};
        break;
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        YUVAChannels = {0, 4, 8, 0xff};
        break;
      case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        YUVAChannels = {0, 4, 5, 0xff};
        break;
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        YUVAChannels = {0, 4, 8, 0xff};
        break;
      case VK_FORMAT_G16B16G16R16_422_UNORM: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_B16G16R16G16_422_UNORM: YUVAChannels = {0, 2, 1, 0xff}; break;
      case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: YUVAChannels = {0, 4, 8, 0xff}; break;
      case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: YUVAChannels = {0, 4, 5, 0xff}; break;
      case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: YUVAChannels = {0, 4, 8, 0xff}; break;
      case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: YUVAChannels = {0, 4, 5, 0xff}; break;
      case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: YUVAChannels = {0, 4, 8, 0xff}; break;
      default: break;
    }
  }
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

VkFormat GetViewCastedFormat(VkFormat f, CompType typeHint)
{
  if(typeHint == CompType::Typeless)
    return f;

  switch(f)
  {
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R64G64B64A64_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R64G64B64A64_SINT;
      else
        return VK_FORMAT_R64G64B64A64_SFLOAT;
    }
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R64G64B64_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R64G64B64_SINT;
      else
        return VK_FORMAT_R64G64B64_SFLOAT;
    }
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R64G64_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R64G64_SINT;
      else
        return VK_FORMAT_R64G64_SFLOAT;
    }
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R64_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R64_SINT;
      else
        return VK_FORMAT_R64_SFLOAT;
    }
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R32G32B32A32_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R32G32B32A32_SINT;
      else
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R32G32B32_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R32G32B32_SINT;
      else
        return VK_FORMAT_R32G32B32_SFLOAT;
    }
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R32G32_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R32G32_SINT;
      else
        return VK_FORMAT_R32G32_SFLOAT;
    }
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT:
    {
      if(typeHint == CompType::UInt)
        return VK_FORMAT_R32_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R32_SINT;
      else if(typeHint == CompType::Depth)
        return VK_FORMAT_D32_SFLOAT;
      else
        return VK_FORMAT_R32_SFLOAT;
    }
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    {
      if(typeHint == CompType::UNorm || typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R16G16B16A16_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R16G16B16A16_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R16G16B16A16_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R16G16B16A16_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R16G16B16A16_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R16G16B16A16_SINT;
      else
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    }
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    {
      if(typeHint == CompType::UNorm || typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R16G16B16_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R16G16B16_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R16G16B16_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R16G16B16_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R16G16B16_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R16G16B16_SINT;
      else
        return VK_FORMAT_R16G16B16_SFLOAT;
    }
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    {
      if(typeHint == CompType::UNorm || typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R16G16_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R16G16_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R16G16_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R16G16_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R16G16_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R16G16_SINT;
      else
        return VK_FORMAT_R16G16_SFLOAT;
    }
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    {
      if(typeHint == CompType::UNorm || typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R16_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R16_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R16_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R16_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R16_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R16_SINT;
      else if(typeHint == CompType::Depth)
        return VK_FORMAT_D16_UNORM;
      else
        return VK_FORMAT_R16_SFLOAT;
    }
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_R8G8B8A8_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R8G8B8A8_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R8G8B8A8_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R8G8B8A8_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R8G8B8A8_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R8G8B8A8_SINT;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R8G8B8A8_SRGB;
      else
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_B8G8R8A8_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_B8G8R8A8_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_B8G8R8A8_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_B8G8R8A8_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_B8G8R8A8_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_B8G8R8A8_SINT;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_B8G8R8A8_SRGB;
      else
        return VK_FORMAT_B8G8R8A8_UNORM;
    }
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_A8B8G8R8_SNORM_PACK32;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_A8B8G8R8_USCALED_PACK32;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_A8B8G8R8_SSCALED_PACK32;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_A8B8G8R8_UINT_PACK32;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_A8B8G8R8_SINT_PACK32;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
      else
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    }
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_R8G8B8_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R8G8B8_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R8G8B8_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R8G8B8_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R8G8B8_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R8G8B8_SINT;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R8G8B8_SRGB;
      else
        return VK_FORMAT_R8G8B8_UNORM;
    }
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_B8G8R8_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_B8G8R8_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_B8G8R8_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_B8G8R8_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_B8G8R8_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_B8G8R8_SINT;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_B8G8R8_SRGB;
      else
        return VK_FORMAT_B8G8R8_UNORM;
    }
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_R8G8_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R8G8_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R8G8_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R8G8_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R8G8_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R8G8_SINT;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R8G8_SRGB;
      else
        return VK_FORMAT_R8G8_UNORM;
    }
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_S8_UINT:
    {
      if(typeHint == CompType::UNorm)
        return VK_FORMAT_R8_UNORM;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_R8_SNORM;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_R8_USCALED;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_R8_SSCALED;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_R8_UINT;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_R8_SINT;
      else if(typeHint == CompType::UNormSRGB)
        return VK_FORMAT_R8_SRGB;
      else if(typeHint == CompType::Depth)
        return VK_FORMAT_S8_UINT;
      else
        return VK_FORMAT_R8_UNORM;
    }
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    {
      if(typeHint == CompType::UNorm || typeHint == CompType::UNormSRGB)
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_A2B10G10R10_UINT_PACK32;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_A2B10G10R10_SINT_PACK32;
      else
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    }
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    {
      if(typeHint == CompType::UNorm || typeHint == CompType::UNormSRGB)
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
      else if(typeHint == CompType::SNorm)
        return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
      else if(typeHint == CompType::UScaled)
        return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
      else if(typeHint == CompType::SScaled)
        return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
      else if(typeHint == CompType::UInt)
        return VK_FORMAT_A2R10G10B10_UINT_PACK32;
      else if(typeHint == CompType::SInt)
        return VK_FORMAT_A2R10G10B10_SINT_PACK32;
      else
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    }
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_BC1_RGB_SRGB_BLOCK
                                               : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK
                                               : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
      return (typeHint == CompType::SNorm) ? VK_FORMAT_BC4_SNORM_BLOCK : VK_FORMAT_BC4_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
                                               : VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK
                                               : VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
                                               : VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
      return (typeHint == CompType::SNorm) ? VK_FORMAT_EAC_R11_SNORM_BLOCK
                                           : VK_FORMAT_EAC_R11_UNORM_BLOCK;
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
      return (typeHint == CompType::SNorm) ? VK_FORMAT_BC5_SNORM_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
      return (typeHint == CompType::SNorm) ? VK_FORMAT_BC6H_SFLOAT_BLOCK
                                           : VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
      return (typeHint == CompType::SNorm) ? VK_FORMAT_EAC_R11G11_SNORM_BLOCK
                                           : VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_4x4_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_5x4_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_5x5_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_6x5_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_6x6_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_8x5_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_8x6_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_8x8_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_10x5_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_10x6_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_10x8_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_10x10_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_12x10_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_ASTC_12x12_SRGB_BLOCK
                                               : VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG
                                               : VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG
                                               : VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG
                                               : VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
      return (typeHint == CompType::UNormSRGB) ? VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG
                                               : VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;

    // all other formats have no aliases so nothing to typecast
    default: break;
  }

  return f;
}

// The shape of blocks in (a plane of) an image format.
// Non-block formats are considered to have 1x1 blocks.
// For some planar formats, the block shape depends on the plane--
// e.g. VK_FORMAT_G8_B8R8_2PLANE_422_UNORM has 8 bits per 1x1 block in plane 0, but 16 bits per 1x1
// block in plane 1.
struct BlockShape
{
  // the width of a block, in texels (or 1 for non-block formats)
  uint32_t width;

  // the height of a block, in texels (or 1 for non-block formats)
  uint32_t height;

  // the number of bytes used to encode the block
  uint32_t bytes;
};

BlockShape GetBlockShape(VkFormat Format, uint32_t plane)
{
  switch(Format)
  {
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: return {1, 1, 32};
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT: return {1, 1, 24};
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT: return {1, 1, 16};
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT: return {1, 1, 12};
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
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT: return {1, 1, 8};
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT: return {1, 1, 6};
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return {1, 1, 8};
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
    case VK_FORMAT_B8G8R8_SRGB: return {1, 1, 3};
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
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
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
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return {1, 1, 4};
    case VK_FORMAT_D16_UNORM_S8_UINT: return {1, 1, 4};
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
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return {1, 1, 2};
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_S8_UINT: return {1, 1, 1};
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
    case VK_FORMAT_EAC_R11_SNORM_BLOCK: return {4, 4, 8};
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
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return {4, 4, 16};
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return {4, 4, 16};
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return {5, 4, 16};
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return {5, 5, 16};
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return {6, 5, 16};
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return {6, 6, 16};
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return {8, 5, 16};
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return {8, 6, 16};
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return {8, 8, 16};
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return {10, 5, 16};
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return {10, 6, 16};
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return {10, 8, 16};
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return {10, 10, 16};
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return {12, 10, 16};
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return {12, 12, 16};
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return {8, 4, 8};
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
      return {4, 4, 8};

    /*
     * YUV planar/packed subsampled textures.
     *
     * In each diagram we indicate (maybe part) of the data for a 4x4 texture:
     *
     * +---+---+---+---+
     * | 0 | 1 | 2 | 3 |
     * +---+---+---+---+
     * | 4 | 5 | 6 | 7 |
     * +---+---+---+---+
     * | 8 | 9 | A | B |
     * +---+---+---+---+
     * | C | D | E | F |
     * +---+---+---+---+
     *
     *
     * FOURCC decoding:
     *  - char 0: 'Y' = packed, 'P' = planar
     *  - char 1: '4' = 4:4:4, '2' = 4:2:2, '1' = 4:2:1, '0' = 4:2:0
     *  - char 2+3: '16' = 16-bit, '10' = 10-bit, '08' = 8-bit
     *
     * planar = Y is first, all together, then UV comes second.
     * packed = YUV is interleaved
     *
     * ======================= 4:4:4 lossless packed =========================
     *
     * Equivalent to uncompressed formats, just YUV instead of RGB. For 8-bit:
     *
     * pixel:      0            1            2            3
     * byte:  0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F
     *        Y0 U0 V0 A0  Y1 U1 V1 A1  Y2 U2 V2 A2  Y3 U3 V3 A3
     *
     * 16-bit is similar with two bytes per sample, 10-bit for uncompressed is
     * equivalent to R10G10B10A2 but with RGB=>YUV
     *
     * ============================ 4:2:2 packed =============================
     *
     * 50% horizontal subsampling packed, two Y samples for each U/V sample pair. For 8-bit:
     *
     * pixel:   0  |  1      2  |  3      4  |  5      6  |  7
     * byte:  0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F
     *        Y0 U0 Y1 V0  Y2 U1 Y3 V1  Y4 U2 Y5 V2  Y6 U3 Y7 V3
     *
     * 16-bit is similar with two bytes per sample, 10-bit is stored identically to 16-bit but in
     * the most significant bits:
     *
     * bit:    FEDCBA9876543210
     * 16-bit: XXXXXXXXXXXXXXXX
     * 10-bit: XXXXXXXXXX000000
     *
     * Since the data is unorm this just spaces out valid values.
     *
     * ============================ 4:2:0 planar =============================
     *
     * 50% horizontal and vertical subsampled planar, four Y samples for each U/V sample pair.
     * For 8-bit:
     *
     *
     * pixel: 0  1  2  3   4  5  6  7
     * byte:  0  1  2  3   4  5  6  7
     *        Y0 Y1 Y2 Y3  Y4 Y5 Y6 Y7
     *
     * pixel: 8  9  A  B   C  D  E  F
     * byte:  8  9  A  B   C  D  E  F
     *        Y8 Y9 Ya Yb  Yc Yd Ye Yf
     *
     *        ... all of the rest of Y luma ...
     *
     * pixel:  T&4 | 1&5    2&6 | 3&7
     * byte:  0  1  2  3   4  5  6  7
     *        U0 V0 U1 V1  U2 V2 U3 V3
     *
     * pixel:  8&C | 9&D    A&E | B&F
     * byte:  8  9  A  B   C  D  E  F
     *        U4 V4 U5 V5  U6 V6 U7 V7
     */
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
      // 4:2:2 packed 8-bit, so 1 byte per pixel for luma and 1 byte per pixel for chroma (2 chroma
      // samples, with 50% subsampling = 1 byte per pixel)
      return {2, 1, 4};
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return {1, 1, 1};
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
      if(plane == 0)
      {
        return {1, 1, 1};
      }
      else if(plane == 1)
      {
        return {1, 1, 2};
      }
      else
      {
        RDCERR("Invalid plane %d in 2-plane format", plane);
        return {1, 1, 1};
      }
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16:
      // basically just 16-bit format with only top 10-bits used
      // 10-bit and 12-bit formats are stored identically to 16-bit formats
      return {1, 1, 2};
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
      // just a 16-bit format with only top N-bits used
      // 10-bit and 12-bit formats are stored identically to 16-bit formats
      return {1, 1, 4};
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
      // just a 16-bit format with only top N-bits used
      // 10-bit and 12-bit formats are stored identically to 16-bit formats
      return {1, 1, 8};
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM:
      // 10-bit and 12-bit formats are stored identically to 16-bit formats
      // 4:2:2 packed 16-bit
      return {2, 1, 8};
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return {1, 1, 2};
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
      if(plane == 0)
      {
        return {1, 1, 2};
      }
      else if(plane == 1)
      {
        return {1, 1, 4};
      }
      else
      {
        RDCERR("Invalid plane %d in 2-plane format", plane);
        return {1, 1, 2};
      }
    default: RDCERR("Unrecognised Vulkan Format: %d", Format);
  }

  return {1, 1, 1};
}

VkExtent2D GetPlaneShape(uint32_t Width, uint32_t Height, VkFormat Format, uint32_t plane)
{
  switch(Format)
  {
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
      if(plane == 0)
        return {Width, Height};
      else
        return {RDCMAX(1U, (Width + 1) / 2), RDCMAX(1U, (Height + 1) / 2)};

    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
      if(plane == 0)
        return {Width, Height};
      else
        return {RDCMAX(1U, (Width + 1) / 2), Height};

    default: return {Width, Height};
  }
}

uint32_t GetPlaneByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format,
                          uint32_t mip, uint32_t plane)
{
  uint32_t mipWidth = RDCMAX(Width >> mip, 1U);
  uint32_t mipHeight = RDCMAX(Height >> mip, 1U);
  uint32_t mipDepth = RDCMAX(Depth >> mip, 1U);

  VkExtent2D planeShape = GetPlaneShape(mipWidth, mipHeight, Format, plane);

  BlockShape blockShape = GetBlockShape(Format, plane);

  uint32_t widthInBlocks = (planeShape.width + blockShape.width - 1) / blockShape.width;
  uint32_t heightInBlocks = (planeShape.height + blockShape.height - 1) / blockShape.height;

  return blockShape.bytes * widthInBlocks * heightInBlocks * mipDepth;
}

uint32_t GetByteSize(uint32_t Width, uint32_t Height, uint32_t Depth, VkFormat Format, uint32_t mip)
{
  uint32_t planeCount = GetYUVPlaneCount(Format);
  uint32_t size = 0;
  for(uint32_t p = 0; p < planeCount; p++)
    size += GetPlaneByteSize(Width, Height, Depth, Format, mip, p);
  return size;
}

ResourceFormat MakeResourceFormat(VkFormat fmt)
{
  ResourceFormat ret;

  ret.type = ResourceFormatType::Regular;
  ret.compByteWidth = 0;
  ret.compCount = 0;
  ret.compType = CompType::Typeless;

  if(fmt == VK_FORMAT_UNDEFINED)
  {
    ret.type = ResourceFormatType::Undefined;
    return ret;
  }

  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8: ret.type = ResourceFormatType::R4G4; break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16: ret.type = ResourceFormatType::R4G4B4A4; break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32: ret.type = ResourceFormatType::R10G10B10A2; break;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: ret.type = ResourceFormatType::R11G11B10; break;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: ret.type = ResourceFormatType::R9G9B9E5; break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16: ret.type = ResourceFormatType::R5G6B5; break;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16: ret.type = ResourceFormatType::R5G5B5A1; break;
    case VK_FORMAT_D16_UNORM_S8_UINT: ret.type = ResourceFormatType::D16S8; break;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT: ret.type = ResourceFormatType::D24S8; break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT: ret.type = ResourceFormatType::D32S8; break;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: ret.type = ResourceFormatType::BC1; break;
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK: ret.type = ResourceFormatType::BC2; break;
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK: ret.type = ResourceFormatType::BC3; break;
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK: ret.type = ResourceFormatType::BC4; break;
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK: ret.type = ResourceFormatType::BC5; break;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK: ret.type = ResourceFormatType::BC6; break;
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK: ret.type = ResourceFormatType::BC7; break;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: ret.type = ResourceFormatType::ETC2; break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: ret.type = ResourceFormatType::EAC; break;
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
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: ret.type = ResourceFormatType::ASTC; break;
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: ret.type = ResourceFormatType::PVRTC; break;
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: ret.type = ResourceFormatType::YUV8; break;
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
      ret.type = ResourceFormatType::YUV10;
      break;
    case VK_FORMAT_R12X4_UNORM_PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
      ret.type = ResourceFormatType::YUV12;
      break;
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: ret.type = ResourceFormatType::YUV16; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B16G16R16G16_422_UNORM: ret.SetBGRAOrder(true); break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_R10X6_UNORM_PACK16:
    case VK_FORMAT_R12X4_UNORM_PACK16: ret.compCount = 1; break;
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: ret.compCount = 2; break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_G8B8G8R8_422_UNORM:
    case VK_FORMAT_B8G8R8G8_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16B16G16R16_422_UNORM:
    case VK_FORMAT_B16G16R16G16_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: ret.compCount = 3; break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
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
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: ret.compCount = 4; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: ret.compType = CompType::UNorm; break;
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
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
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: ret.compType = CompType::UNormSRGB; break;
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32: ret.compType = CompType::SNorm; break;
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32: ret.compType = CompType::UScaled; break;
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: ret.compType = CompType::SScaled; break;
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
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
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32: ret.compType = CompType::UInt; break;
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
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
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: ret.compType = CompType::SInt; break;
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: ret.compType = CompType::Float; break;
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret.compType = CompType::Double; break;
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: ret.compType = CompType::Depth; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB: ret.compByteWidth = 1; break;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_D16_UNORM: ret.compByteWidth = 2; break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: ret.compByteWidth = 3; break;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT: ret.compByteWidth = 4; break;
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64G64_UINT:
    case VK_FORMAT_R64G64B64_UINT:
    case VK_FORMAT_R64G64B64A64_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64G64_SINT:
    case VK_FORMAT_R64G64B64_SINT:
    case VK_FORMAT_R64G64B64A64_SINT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret.compByteWidth = 8; break;
    default: break;
  }

  if(IsYUVFormat(fmt))
  {
    ret.SetYUVPlaneCount(GetYUVPlaneCount(fmt));

    switch(fmt)
    {
      case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
      case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
      case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
      case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
      case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
      case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: ret.SetYUVSubsampling(420); break;
      case VK_FORMAT_G8B8G8R8_422_UNORM:
      case VK_FORMAT_B8G8R8G8_422_UNORM:
      case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
      case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
      case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
      case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
      case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
      case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
      case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
      case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
      case VK_FORMAT_G16B16G16R16_422_UNORM:
      case VK_FORMAT_B16G16R16G16_422_UNORM:
      case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
      case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: ret.SetYUVSubsampling(422); break;
      case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
      case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
      case VK_FORMAT_R10X6_UNORM_PACK16:
      case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
      case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
      case VK_FORMAT_R12X4_UNORM_PACK16:
      case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
      case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: ret.SetYUVSubsampling(444); break;
      default: break;
    }
  }

  return ret;
}

VkFormat MakeVkFormat(ResourceFormat fmt)
{
  VkFormat ret = VK_FORMAT_UNDEFINED;

  if(fmt.Special())
  {
    switch(fmt.type)
    {
      case ResourceFormatType::Undefined: return ret;
      case ResourceFormatType::BC1:
      {
        if(fmt.compCount == 3)
          ret = fmt.SRGBCorrected() ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        else
          ret = fmt.SRGBCorrected() ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        break;
      }
      case ResourceFormatType::BC2:
        ret = fmt.SRGBCorrected() ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC3:
        ret = fmt.SRGBCorrected() ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC4:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC4_SNORM_BLOCK : VK_FORMAT_BC4_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC5:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC5_SNORM_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK;
        break;
      case ResourceFormatType::BC6:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC6H_SFLOAT_BLOCK
                                              : VK_FORMAT_BC6H_UFLOAT_BLOCK;
        break;
      case ResourceFormatType::BC7:
        ret = fmt.SRGBCorrected() ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
        break;
      case ResourceFormatType::ETC2:
      {
        if(fmt.compCount == 3)
          ret = fmt.SRGBCorrected() ? VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
                                    : VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        else
          ret = fmt.SRGBCorrected() ? VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
                                    : VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        break;
      }
      case ResourceFormatType::EAC:
      {
        if(fmt.compCount == 1)
          ret = fmt.compType == CompType::SNorm ? VK_FORMAT_EAC_R11_SNORM_BLOCK
                                                : VK_FORMAT_EAC_R11_UNORM_BLOCK;
        else if(fmt.compCount == 2)
          ret = fmt.compType == CompType::SNorm ? VK_FORMAT_EAC_R11G11_SNORM_BLOCK
                                                : VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        break;
      }
      case ResourceFormatType::R10G10B10A2:
        if(fmt.compType == CompType::UNorm)
          ret = fmt.BGRAOrder() ? VK_FORMAT_A2R10G10B10_UNORM_PACK32
                                : VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        else if(fmt.compType == CompType::UInt)
          ret = fmt.BGRAOrder() ? VK_FORMAT_A2R10G10B10_UINT_PACK32
                                : VK_FORMAT_A2B10G10R10_UINT_PACK32;
        else if(fmt.compType == CompType::UScaled)
          ret = fmt.BGRAOrder() ? VK_FORMAT_A2R10G10B10_USCALED_PACK32
                                : VK_FORMAT_A2B10G10R10_USCALED_PACK32;
        else if(fmt.compType == CompType::SNorm)
          ret = fmt.BGRAOrder() ? VK_FORMAT_A2R10G10B10_SNORM_PACK32
                                : VK_FORMAT_A2B10G10R10_SNORM_PACK32;
        else if(fmt.compType == CompType::SInt)
          ret = fmt.BGRAOrder() ? VK_FORMAT_A2R10G10B10_SINT_PACK32
                                : VK_FORMAT_A2B10G10R10_SINT_PACK32;
        else if(fmt.compType == CompType::SScaled)
          ret = fmt.BGRAOrder() ? VK_FORMAT_A2R10G10B10_SSCALED_PACK32
                                : VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
        break;
      case ResourceFormatType::R11G11B10: ret = VK_FORMAT_B10G11R11_UFLOAT_PACK32; break;
      case ResourceFormatType::R5G6B5:
        ret = fmt.BGRAOrder() ? VK_FORMAT_B5G6R5_UNORM_PACK16 : VK_FORMAT_R5G6B5_UNORM_PACK16;
        break;
      case ResourceFormatType::R5G5B5A1:
        ret = fmt.BGRAOrder() ? VK_FORMAT_B5G5R5A1_UNORM_PACK16 : VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        break;
      case ResourceFormatType::R9G9B9E5: ret = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32; break;
      case ResourceFormatType::R4G4B4A4:
        ret = fmt.BGRAOrder() ? VK_FORMAT_B4G4R4A4_UNORM_PACK16 : VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        break;
      case ResourceFormatType::R4G4: ret = VK_FORMAT_R4G4_UNORM_PACK8; break;
      case ResourceFormatType::D16S8: ret = VK_FORMAT_D16_UNORM_S8_UINT; break;
      case ResourceFormatType::D24S8: ret = VK_FORMAT_D24_UNORM_S8_UINT; break;
      case ResourceFormatType::D32S8: ret = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
      case ResourceFormatType::YUV8:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();

        // don't support anything but 3 components
        if(fmt.compCount != 3)
          return VK_FORMAT_UNDEFINED;

        if(subsampling == 444)
        {
          // only support 3-plane
          return planeCount == 3 ? VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM : VK_FORMAT_UNDEFINED;
        }
        else if(subsampling == 422)
        {
          if(planeCount == 1)
            return fmt.BGRAOrder() ? VK_FORMAT_B8G8R8G8_422_UNORM : VK_FORMAT_G8B8G8R8_422_UNORM;
          else if(planeCount == 2)
            return VK_FORMAT_G8_B8R8_2PLANE_422_UNORM;
          else if(planeCount == 3)
            return VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM;
        }
        else if(subsampling == 420)
        {
          if(planeCount == 2)
            return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
          else if(planeCount == 3)
            return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
          else
            return VK_FORMAT_UNDEFINED;
        }

        return VK_FORMAT_UNDEFINED;
      }
      case ResourceFormatType::YUV10:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();

        if(fmt.compCount == 1)
        {
          if(subsampling == 444 && planeCount == 1)
            return VK_FORMAT_R10X6_UNORM_PACK16;
          return VK_FORMAT_UNDEFINED;
        }
        else if(fmt.compCount == 2)
        {
          if(subsampling == 444 && planeCount == 1)
            return VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
          return VK_FORMAT_UNDEFINED;
        }
        else if(fmt.compCount == 4)
        {
          if(subsampling == 444 && planeCount == 1)
            return VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16;
          return VK_FORMAT_UNDEFINED;
        }

        if(subsampling == 444)
        {
          // only support 3-plane
          return planeCount == 3 ? VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16
                                 : VK_FORMAT_UNDEFINED;
        }
        else if(subsampling == 422)
        {
          if(planeCount == 1)
            return fmt.BGRAOrder() ? VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16
                                   : VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16;
          else if(planeCount == 2)
            return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
          else if(planeCount == 3)
            return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16;
        }
        else if(subsampling == 420)
        {
          if(planeCount == 2)
            return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
          else if(planeCount == 3)
            return VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
          else
            return VK_FORMAT_UNDEFINED;
        }

        return VK_FORMAT_UNDEFINED;
      }
      case ResourceFormatType::YUV12:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();

        if(fmt.compCount == 1)
        {
          if(subsampling == 444 && planeCount == 1)
            return VK_FORMAT_R12X4_UNORM_PACK16;
          return VK_FORMAT_UNDEFINED;
        }
        else if(fmt.compCount == 2)
        {
          if(subsampling == 444 && planeCount == 1)
            return VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
          return VK_FORMAT_UNDEFINED;
        }
        else if(fmt.compCount == 4)
        {
          if(subsampling == 444 && planeCount == 1)
            return VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16;
          return VK_FORMAT_UNDEFINED;
        }

        if(subsampling == 444)
        {
          // only support 3-plane
          return planeCount == 3 ? VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16
                                 : VK_FORMAT_UNDEFINED;
        }
        else if(subsampling == 422)
        {
          if(planeCount == 1)
            return fmt.BGRAOrder() ? VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16
                                   : VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16;
          else if(planeCount == 2)
            return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16;
          else if(planeCount == 3)
            return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16;
        }
        else if(subsampling == 420)
        {
          if(planeCount == 2)
            return VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16;
          else if(planeCount == 3)
            return VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16;
          else
            return VK_FORMAT_UNDEFINED;
        }

        return VK_FORMAT_UNDEFINED;
      }
      case ResourceFormatType::YUV16:
      {
        int subsampling = fmt.YUVSubsampling();
        int planeCount = fmt.YUVPlaneCount();

        if(subsampling == 444)
        {
          // only support 3-plane
          return planeCount == 3 ? VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM : VK_FORMAT_UNDEFINED;
        }
        else if(subsampling == 422)
        {
          if(planeCount == 1)
            return fmt.BGRAOrder() ? VK_FORMAT_B16G16R16G16_422_UNORM
                                   : VK_FORMAT_G16B16G16R16_422_UNORM;
          else if(planeCount == 2)
            return VK_FORMAT_G16_B16R16_2PLANE_422_UNORM;
          else if(planeCount == 3)
            return VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM;
        }
        else if(subsampling == 420)
        {
          if(planeCount == 2)
            return VK_FORMAT_G16_B16R16_2PLANE_420_UNORM;
          else if(planeCount == 3)
            return VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM;
          else
            return VK_FORMAT_UNDEFINED;
        }

        return VK_FORMAT_UNDEFINED;
      }
      default: RDCERR("Unsupported resource format type %u", fmt.type); break;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.SRGBCorrected())
    {
      ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB;
    }
    else if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64G64B64A64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64G64B64A64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64G64B64A64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32B32A32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32B32A32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32B32A32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16B16A16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16B16A16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16B16A16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16B16A16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16B16A16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16B16A16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16B16A16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_SINT : VK_FORMAT_R8G8B8A8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_UINT : VK_FORMAT_R8G8B8A8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_SSCALED : VK_FORMAT_R8G8B8A8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8A8_USCALED : VK_FORMAT_R8G8B8A8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 4-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 3)
  {
    if(fmt.SRGBCorrected())
    {
      ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_SRGB : VK_FORMAT_R8G8B8_SRGB;
    }
    else if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64G64B64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64G64B64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64G64B64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32B32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32B32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32B32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16B16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16B16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16B16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16B16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16B16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16B16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16B16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_SINT : VK_FORMAT_R8G8B8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_UINT : VK_FORMAT_R8G8B8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_SNORM : VK_FORMAT_R8G8B8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_UNORM : VK_FORMAT_R8G8B8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_SSCALED : VK_FORMAT_R8G8B8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = fmt.BGRAOrder() ? VK_FORMAT_B8G8R8_USCALED : VK_FORMAT_R8G8B8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 2)
  {
    if(fmt.SRGBCorrected())
    {
      ret = VK_FORMAT_R8G8_SRGB;
    }
    else if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64G64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64G64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64G64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8G8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8G8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8G8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8G8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8G8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8G8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 2-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 1)
  {
    if(fmt.SRGBCorrected())
    {
      ret = VK_FORMAT_R8_SRGB;
    }
    else if(fmt.compByteWidth == 8)
    {
      if(fmt.compType == CompType::Float || fmt.compType == CompType::Double)
        ret = VK_FORMAT_R64_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R64_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R64_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32_UINT;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_D32_SFLOAT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16_UNORM;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_D16_UNORM;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16_USCALED;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16_SSCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8_UNORM;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8_USCALED;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8_SSCALED;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_S8_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 1-component byte width: %d", fmt.compByteWidth);
    }
  }
  else
  {
    RDCERR("Unrecognised component count: %d", fmt.compCount);
  }

  if(ret == VK_FORMAT_UNDEFINED)
    RDCERR("No known vulkan format corresponding to resource format!");

  return ret;
}

VkResourceRecord::~VkResourceRecord()
{
  VkResourceType resType = Resource != NULL ? IdentifyTypeByPtr(Resource) : eResUnknown;

  if(resType == eResPhysicalDevice)
    SAFE_DELETE(memProps);

  // bufferviews and imageviews have non-owning pointers to the sparseinfo struct
  if(resType == eResBuffer || resType == eResImage)
    SAFE_DELETE(resInfo);

  if(resType == eResInstance || resType == eResDevice)
    SAFE_DELETE(instDevInfo);

  if(resType == eResSwapchain)
    SAFE_DELETE(swapInfo);

  if(resType == eResDeviceMemory && memMapState)
  {
    FreeAlignedBuffer(memMapState->refData);

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

  if(resType == eResPipelineLayout)
    SAFE_DELETE(pipeLayoutInfo);

  if(resType == eResDescUpdateTemplate)
    SAFE_DELETE(descTemplateInfo);
}

void VkResourceRecord::MarkMemoryFrameReferenced(ResourceId mem, VkDeviceSize offset,
                                                 VkDeviceSize size, FrameRefType refType)
{
  if(refType != eFrameRef_Read && refType != eFrameRef_None)
    cmdInfo->dirtied.insert(mem);
  FrameRefType maxRef = MarkMemoryReferenced(cmdInfo->memFrameRefs, mem, offset, size, refType);
  MarkResourceFrameReferenced(
      mem, maxRef, [](FrameRefType x, FrameRefType y) -> FrameRefType { return std::max(x, y); });
}

void VkResourceRecord::MarkBufferFrameReferenced(VkResourceRecord *buf, VkDeviceSize offset,
                                                 VkDeviceSize size, FrameRefType refType)
{
  // mark buffer just as read
  MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);

  if(size == VK_WHOLE_SIZE)
  {
    size = buf->memSize;
  }
  if(buf->resInfo)
    cmdInfo->sparse.insert(buf->resInfo);
  if(buf->baseResource != ResourceId())
    MarkMemoryFrameReferenced(buf->baseResource, buf->memOffset + offset, size, refType);
}

void VkResourceRecord::MarkBufferImageCopyFrameReferenced(
    VkResourceRecord *buf, VkResourceRecord *img, const ImageLayouts &layout, uint32_t regionCount,
    const VkBufferImageCopy *regions, FrameRefType bufRefType, FrameRefType imgRefType)
{
  MarkResourceFrameReferenced(img->GetResourceID(), imgRefType);
  MarkResourceFrameReferenced(img->baseResource, imgRefType);

  if(IsDirtyFrameRef(imgRefType))
    cmdInfo->dirtied.insert(img->GetResourceID());

  // mark buffer just as read
  MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);

  for(uint32_t ri = 0; ri < regionCount; ri++)
  {
    const VkBufferImageCopy &region = regions[ri];

    VkFormat regionFormat = layout.format;
    uint32_t plane = 0;
    switch(region.imageSubresource.aspectMask)
    {
      case VK_IMAGE_ASPECT_STENCIL_BIT: regionFormat = VK_FORMAT_S8_UINT; break;
      case VK_IMAGE_ASPECT_DEPTH_BIT: regionFormat = GetDepthOnlyFormat(layout.format); break;
      case VK_IMAGE_ASPECT_PLANE_1_BIT: plane = 1; break;
      case VK_IMAGE_ASPECT_PLANE_2_BIT: plane = 2; break;
      default: break;
    }

    // The shape of the texel blocks;
    // non-block formats are treated as having 1x1 blocks
    BlockShape blockShape = GetBlockShape(regionFormat, plane);

    // width of copied region, in blocks
    uint32_t widthInBlocks = (region.imageExtent.width + blockShape.width - 1) / blockShape.width;

    // width of copied region, in bytes (in the buffer);
    uint32_t widthInBytes = blockShape.bytes * widthInBlocks;

    // height of copied region, in blocks
    uint32_t heightInBlocks = (region.imageExtent.height + blockShape.height - 1) / blockShape.height;

    // total number of depth slices to be copied.
    uint32_t sliceCount = region.imageExtent.depth * region.imageSubresource.layerCount;

    // stride_y: number of bytes in the buffer between the start of one row of
    // blocks and the next. The buffer may have space for more blocks per row than
    // are actually being copied (specified by bufferRowLength).
    uint32_t stride_y;
    if(region.bufferRowLength == 0)
      stride_y = widthInBytes;
    else
      stride_y = blockShape.bytes * region.bufferRowLength;

    // stride_z: number of bytes in the buffer between the start of one depth
    // slice and the next. The buffer may have space for more rows per slice
    // than are actually being copied (specified by bufferImageHeight).
    uint32_t stride_z;
    if(region.bufferImageHeight == 0)
      stride_z = stride_y * heightInBlocks;
    else
      stride_z = stride_y * region.bufferImageHeight;

    // memory offset of the first byte to be copied to/from the buffer
    VkDeviceSize startRegion = buf->memOffset + region.bufferOffset;

    if(stride_z == widthInBytes * heightInBlocks)
    {
      // no gaps between slices nor between rows; single copy for entire region
      MarkMemoryFrameReferenced(buf->baseResource, startRegion,
                                widthInBytes * heightInBlocks * sliceCount, bufRefType);
    }
    else if(stride_y == widthInBytes)
    {
      // gaps between slices, but no gaps between rows; separate copies per slice
      for(uint32_t z = 0; z < sliceCount; z++)
      {
        VkDeviceSize startSlice = startRegion + z * stride_z;
        MarkMemoryFrameReferenced(buf->baseResource, startSlice, widthInBytes * heightInBlocks,
                                  bufRefType);
      }
    }
    else
    {
      // gaps between rows; separate copies for each row in each slice
      for(uint32_t z = 0; z < sliceCount; z++)
      {
        VkDeviceSize startSlice = startRegion + z * stride_z;
        for(uint32_t y = 0; y < heightInBlocks; y++)
        {
          VkDeviceSize startRow = startSlice + y * stride_y;
          MarkMemoryFrameReferenced(buf->baseResource, startRow, widthInBytes, bufRefType);
        }
      }
    }
  }
}

void VkResourceRecord::MarkBufferViewFrameReferenced(VkResourceRecord *bufView, FrameRefType refType)
{
  // mark the VkBufferView and VkBuffer as read
  MarkResourceFrameReferenced(bufView->GetResourceID(), eFrameRef_Read);
  if(bufView->baseResource != ResourceId())
    MarkResourceFrameReferenced(bufView->baseResource, eFrameRef_Read);

  if(bufView->resInfo)
    cmdInfo->sparse.insert(bufView->resInfo);
  if(bufView->baseResourceMem != ResourceId())
    MarkMemoryFrameReferenced(bufView->baseResourceMem, bufView->memOffset, bufView->memSize,
                              refType);
}

void ResourceInfo::Update(uint32_t numBindings, const VkSparseImageMemoryBind *pBindings)
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

void ResourceInfo::Update(uint32_t numBindings, const VkSparseMemoryBind *pBindings)
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

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("Vulkan formats", "[format][vulkan]")
{
  // must be updated by hand
  std::initializer_list<VkFormat> formats = {
      VK_FORMAT_UNDEFINED,
      VK_FORMAT_R4G4_UNORM_PACK8,
      VK_FORMAT_R4G4B4A4_UNORM_PACK16,
      VK_FORMAT_B4G4R4A4_UNORM_PACK16,
      VK_FORMAT_R5G6B5_UNORM_PACK16,
      VK_FORMAT_B5G6R5_UNORM_PACK16,
      VK_FORMAT_R5G5B5A1_UNORM_PACK16,
      VK_FORMAT_B5G5R5A1_UNORM_PACK16,
      VK_FORMAT_A1R5G5B5_UNORM_PACK16,
      VK_FORMAT_R8_UNORM,
      VK_FORMAT_R8_SNORM,
      VK_FORMAT_R8_USCALED,
      VK_FORMAT_R8_SSCALED,
      VK_FORMAT_R8_UINT,
      VK_FORMAT_R8_SINT,
      VK_FORMAT_R8_SRGB,
      VK_FORMAT_R8G8_UNORM,
      VK_FORMAT_R8G8_SNORM,
      VK_FORMAT_R8G8_USCALED,
      VK_FORMAT_R8G8_SSCALED,
      VK_FORMAT_R8G8_UINT,
      VK_FORMAT_R8G8_SINT,
      VK_FORMAT_R8G8_SRGB,
      VK_FORMAT_R8G8B8_UNORM,
      VK_FORMAT_R8G8B8_SNORM,
      VK_FORMAT_R8G8B8_USCALED,
      VK_FORMAT_R8G8B8_SSCALED,
      VK_FORMAT_R8G8B8_UINT,
      VK_FORMAT_R8G8B8_SINT,
      VK_FORMAT_R8G8B8_SRGB,
      VK_FORMAT_B8G8R8_UNORM,
      VK_FORMAT_B8G8R8_SNORM,
      VK_FORMAT_B8G8R8_USCALED,
      VK_FORMAT_B8G8R8_SSCALED,
      VK_FORMAT_B8G8R8_UINT,
      VK_FORMAT_B8G8R8_SINT,
      VK_FORMAT_B8G8R8_SRGB,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_SNORM,
      VK_FORMAT_R8G8B8A8_USCALED,
      VK_FORMAT_R8G8B8A8_SSCALED,
      VK_FORMAT_R8G8B8A8_UINT,
      VK_FORMAT_R8G8B8A8_SINT,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_B8G8R8A8_SNORM,
      VK_FORMAT_B8G8R8A8_USCALED,
      VK_FORMAT_B8G8R8A8_SSCALED,
      VK_FORMAT_B8G8R8A8_UINT,
      VK_FORMAT_B8G8R8A8_SINT,
      VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_A8B8G8R8_UNORM_PACK32,
      VK_FORMAT_A8B8G8R8_SNORM_PACK32,
      VK_FORMAT_A8B8G8R8_USCALED_PACK32,
      VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
      VK_FORMAT_A8B8G8R8_UINT_PACK32,
      VK_FORMAT_A8B8G8R8_SINT_PACK32,
      VK_FORMAT_A8B8G8R8_SRGB_PACK32,
      VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      VK_FORMAT_A2R10G10B10_SNORM_PACK32,
      VK_FORMAT_A2R10G10B10_USCALED_PACK32,
      VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
      VK_FORMAT_A2R10G10B10_UINT_PACK32,
      VK_FORMAT_A2R10G10B10_SINT_PACK32,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_A2B10G10R10_SNORM_PACK32,
      VK_FORMAT_A2B10G10R10_USCALED_PACK32,
      VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
      VK_FORMAT_A2B10G10R10_UINT_PACK32,
      VK_FORMAT_A2B10G10R10_SINT_PACK32,
      VK_FORMAT_R16_UNORM,
      VK_FORMAT_R16_SNORM,
      VK_FORMAT_R16_USCALED,
      VK_FORMAT_R16_SSCALED,
      VK_FORMAT_R16_UINT,
      VK_FORMAT_R16_SINT,
      VK_FORMAT_R16_SFLOAT,
      VK_FORMAT_R16G16_UNORM,
      VK_FORMAT_R16G16_SNORM,
      VK_FORMAT_R16G16_USCALED,
      VK_FORMAT_R16G16_SSCALED,
      VK_FORMAT_R16G16_UINT,
      VK_FORMAT_R16G16_SINT,
      VK_FORMAT_R16G16_SFLOAT,
      VK_FORMAT_R16G16B16_UNORM,
      VK_FORMAT_R16G16B16_SNORM,
      VK_FORMAT_R16G16B16_USCALED,
      VK_FORMAT_R16G16B16_SSCALED,
      VK_FORMAT_R16G16B16_UINT,
      VK_FORMAT_R16G16B16_SINT,
      VK_FORMAT_R16G16B16_SFLOAT,
      VK_FORMAT_R16G16B16A16_UNORM,
      VK_FORMAT_R16G16B16A16_SNORM,
      VK_FORMAT_R16G16B16A16_USCALED,
      VK_FORMAT_R16G16B16A16_SSCALED,
      VK_FORMAT_R16G16B16A16_UINT,
      VK_FORMAT_R16G16B16A16_SINT,
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R32_UINT,
      VK_FORMAT_R32_SINT,
      VK_FORMAT_R32_SFLOAT,
      VK_FORMAT_R32G32_UINT,
      VK_FORMAT_R32G32_SINT,
      VK_FORMAT_R32G32_SFLOAT,
      VK_FORMAT_R32G32B32_UINT,
      VK_FORMAT_R32G32B32_SINT,
      VK_FORMAT_R32G32B32_SFLOAT,
      VK_FORMAT_R32G32B32A32_UINT,
      VK_FORMAT_R32G32B32A32_SINT,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      VK_FORMAT_R64_UINT,
      VK_FORMAT_R64_SINT,
      VK_FORMAT_R64_SFLOAT,
      VK_FORMAT_R64G64_UINT,
      VK_FORMAT_R64G64_SINT,
      VK_FORMAT_R64G64_SFLOAT,
      VK_FORMAT_R64G64B64_UINT,
      VK_FORMAT_R64G64B64_SINT,
      VK_FORMAT_R64G64B64_SFLOAT,
      VK_FORMAT_R64G64B64A64_UINT,
      VK_FORMAT_R64G64B64A64_SINT,
      VK_FORMAT_R64G64B64A64_SFLOAT,
      VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
      VK_FORMAT_D16_UNORM,
      VK_FORMAT_X8_D24_UNORM_PACK32,
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_BC1_RGB_UNORM_BLOCK,
      VK_FORMAT_BC1_RGB_SRGB_BLOCK,
      VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
      VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
      VK_FORMAT_BC2_UNORM_BLOCK,
      VK_FORMAT_BC2_SRGB_BLOCK,
      VK_FORMAT_BC3_UNORM_BLOCK,
      VK_FORMAT_BC3_SRGB_BLOCK,
      VK_FORMAT_BC4_UNORM_BLOCK,
      VK_FORMAT_BC4_SNORM_BLOCK,
      VK_FORMAT_BC5_UNORM_BLOCK,
      VK_FORMAT_BC5_SNORM_BLOCK,
      VK_FORMAT_BC6H_UFLOAT_BLOCK,
      VK_FORMAT_BC6H_SFLOAT_BLOCK,
      VK_FORMAT_BC7_UNORM_BLOCK,
      VK_FORMAT_BC7_SRGB_BLOCK,
      VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
      VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
      VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
      VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
      VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
      VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
      VK_FORMAT_EAC_R11_UNORM_BLOCK,
      VK_FORMAT_EAC_R11_SNORM_BLOCK,
      VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
      VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
      VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
      VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
      VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
      VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
      VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
      VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
      VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
      VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
      VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
      VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
      VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
      VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
      VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
      VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
      VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
      VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
      VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
      VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
      VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
      VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
      VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
      VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
      VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
      VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
      VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
      VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
      VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
      VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
      VK_FORMAT_G8B8G8R8_422_UNORM,
      VK_FORMAT_B8G8R8G8_422_UNORM,
      VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
      VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
      VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
      VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
      VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
      VK_FORMAT_R10X6_UNORM_PACK16,
      VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
      VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
      VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
      VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
      VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
      VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
      VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
      VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
      VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
      VK_FORMAT_R12X4_UNORM_PACK16,
      VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
      VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
      VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
      VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
      VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
      VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
      VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
      VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
      VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
      VK_FORMAT_G16B16G16R16_422_UNORM,
      VK_FORMAT_B16G16R16G16_422_UNORM,
      VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
      VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
      VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
      VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
      VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
      VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG,
      VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
      VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
      VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
      VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
      VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
      VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
      VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,
  };

  SECTION("Only VK_FORMAT_UNDEFINED is ResourceFormatType::Undefined")
  {
    for(VkFormat f : formats)
    {
      ResourceFormat fmt = MakeResourceFormat(f);

      if(f == VK_FORMAT_UNDEFINED)
        CHECK(fmt.type == ResourceFormatType::Undefined);
      else
        CHECK(fmt.type != ResourceFormatType::Undefined);
    }
  };

  SECTION("MakeVkFormat is reflexive with MakeResourceFormat")
  {
    for(VkFormat f : formats)
    {
      VkFormat original = f;
      ResourceFormat fmt = MakeResourceFormat(f);

      // astc and pvrtc are not properly supported, collapse to a single type
      if(f >= VK_FORMAT_ASTC_4x4_UNORM_BLOCK && f <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
      {
        CHECK(fmt.type == ResourceFormatType::ASTC);
        continue;
      }
      if(f >= VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG && f <= VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG)
      {
        CHECK(fmt.type == ResourceFormatType::PVRTC);
        continue;
      }

      VkFormat reconstructed = MakeVkFormat(fmt);

      // we are OK with remapping these variants to another similar one, where our format doesn't
      // have enough flexibility to represent the exact type (as a trade-off vs simplicity of
      // processing/storage).
      if(f == VK_FORMAT_A1R5G5B5_UNORM_PACK16)
      {
        CHECK(reconstructed == VK_FORMAT_R5G5B5A1_UNORM_PACK16);
      }
      else if(f == VK_FORMAT_A8B8G8R8_UNORM_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_UNORM);
      }
      else if(f == VK_FORMAT_A8B8G8R8_SNORM_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_SNORM);
      }
      else if(f == VK_FORMAT_A8B8G8R8_USCALED_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_USCALED);
      }
      else if(f == VK_FORMAT_A8B8G8R8_SSCALED_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_SSCALED);
      }
      else if(f == VK_FORMAT_A8B8G8R8_UINT_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_UINT);
      }
      else if(f == VK_FORMAT_A8B8G8R8_SINT_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_SINT);
      }
      else if(f == VK_FORMAT_A8B8G8R8_SRGB_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_R8G8B8A8_SRGB);
      }
      else if(f == VK_FORMAT_X8_D24_UNORM_PACK32)
      {
        CHECK(reconstructed == VK_FORMAT_D24_UNORM_S8_UINT);
      }
      else if(f == VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK)
      {
        CHECK(reconstructed == VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
      }
      else if(f == VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK)
      {
        CHECK(reconstructed == VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
      }
      else
      {
        CHECK(reconstructed == original);
      }
    }
  };

  SECTION("MakeVkFormat concurs with helpers")
  {
    for(VkFormat f : formats)
    {
      ResourceFormat fmt = MakeResourceFormat(f);

      INFO("Format is " << ToStr(f));

      if(IsBlockFormat(f))
      {
        INFO("Format type is " << ToStr(fmt.type));

        bool bcn = fmt.type >= ResourceFormatType::BC1 && fmt.type <= ResourceFormatType::BC7;

        CHECK((bcn || fmt.type == ResourceFormatType::ASTC || fmt.type == ResourceFormatType::EAC ||
               fmt.type == ResourceFormatType::ETC2 || fmt.type == ResourceFormatType::PVRTC));
      }

      if(IsYUVFormat(f))
      {
        CHECK(fmt.type >= ResourceFormatType::YUV8);
        CHECK(fmt.type <= ResourceFormatType::YUV16);
      }

      if(IsDepthOrStencilFormat(f))
      {
        CHECK(fmt.compType == CompType::Depth);
      }
      else if(IsUIntFormat(f))
      {
        CHECK(fmt.compType == CompType::UInt);
      }
      else if(IsSIntFormat(f))
      {
        CHECK(fmt.compType == CompType::SInt);
      }

      if(IsSRGBFormat(f))
      {
        CHECK(fmt.SRGBCorrected());
      }
    }
  };

  SECTION("GetByteSize return expected values for regular formats")
  {
    for(VkFormat f : formats)
    {
      ResourceFormat fmt = MakeResourceFormat(f);

      if(fmt.type != ResourceFormatType::Regular)
        continue;

      INFO("Format is " << ToStr(f));

      uint32_t size = fmt.compCount * fmt.compByteWidth * 123 * 456;

      CHECK(size == GetByteSize(123, 456, 1, f, 0));
    }
  };

  SECTION("GetByteSize for BCn formats")
  {
    const uint32_t width = 24, height = 24;

    // reference: 24x24 = 576, 576/2 = 288

    const uint32_t bcnsizes[] = {
        288,    // VK_FORMAT_BC1_RGB_UNORM_BLOCK
        288,    // VK_FORMAT_BC1_RGB_SRGB_BLOCK
        288,    // VK_FORMAT_BC1_RGBA_UNORM_BLOCK
        288,    // VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 0.5 byte/px
        576,    // VK_FORMAT_BC2_UNORM_BLOCK
        576,    // VK_FORMAT_BC2_SRGB_BLOCK = 1 byte/px
        576,    // VK_FORMAT_BC3_UNORM_BLOCK
        576,    // VK_FORMAT_BC3_SRGB_BLOCK = 1 byte/px
        288,    // VK_FORMAT_BC4_UNORM_BLOCK
        288,    // VK_FORMAT_BC4_SNORM_BLOCK = 0.5 byte/px
        576,    // VK_FORMAT_BC5_UNORM_BLOCK
        576,    // VK_FORMAT_BC5_SNORM_BLOCK = 1 byte/px
        576,    // VK_FORMAT_BC6H_UFLOAT_BLOCK
        576,    // VK_FORMAT_BC6H_SFLOAT_BLOCK = 1 byte/px
        576,    // VK_FORMAT_BC7_UNORM_BLOCK
        576,    // VK_FORMAT_BC7_SRGB_BLOCK = 1 byte/px
    };

    int i = 0;
    for(VkFormat f : {
            VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK,
            VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
            VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK, VK_FORMAT_BC3_UNORM_BLOCK,
            VK_FORMAT_BC3_SRGB_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC4_SNORM_BLOCK,
            VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK, VK_FORMAT_BC6H_UFLOAT_BLOCK,
            VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK,
        })
    {
      INFO("Format is " << ToStr(f));

      CHECK(bcnsizes[i++] == GetByteSize(width, height, 1, f, 0));
    }
  };

  SECTION("GetByteSize for YUV formats")
  {
    const uint32_t width = 24, height = 24;

    const uint32_t yuvsizes[] = {
        1152,    // VK_FORMAT_G8B8G8R8_422_UNORM (4:2:2 8-bit packed)
        1152,    // VK_FORMAT_B8G8R8G8_422_UNORM (4:2:2 8-bit packed)
        864,     // VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM (4:2:0 8-bit 3-plane)
        864,     // VK_FORMAT_G8_B8R8_2PLANE_420_UNORM (4:2:0 8-bit 2-plane)
        1152,    // VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM (4:2:2 8-bit 3-plane)
        1152,    // VK_FORMAT_G8_B8R8_2PLANE_422_UNORM (4:2:2 8-bit 2-plane)
        1728,    // VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM (4:4:4 8-bit 3-plane)
        1152,    // VK_FORMAT_R10X6_UNORM_PACK16 (4:4:4 10-bit packed)
        2304,    // VK_FORMAT_R10X6G10X6_UNORM_2PACK16 (4:4:4 10-bit packed)
        4608,    // VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 (4:4:4 10-bit packed)
        2304,    // VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 (4:2:2 10-bit packed)
        2304,    // VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 (4:2:2 10-bit packed)
        1728,    // VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 (4:2:0 10-bit 3-plane)
        1728,    // VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 (4:2:0 10-bit 2-plane)
        2304,    // VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 (4:2:2 10-bit 3-plane)
        2304,    // VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 (4:2:2 10-bit 2-plane)
        3456,    // VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 (4:4:4 10-bit 3-plane)
        1152,    // VK_FORMAT_R12X4_UNORM_PACK16 (4:4:4 12-bit packed)
        2304,    // VK_FORMAT_R12X4G12X4_UNORM_2PACK16 (4:4:4 12-bit packed)
        4608,    // VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 (4:4:4 12-bit packed)
        2304,    // VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 (4:2:2 12-bit packed)
        2304,    // VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 (4:2:2 12-bit packed)
        1728,    // VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 (4:2:0 12-bit 3-plane)
        1728,    // VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 (4:2:0 12-bit 2-plane)
        2304,    // VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 (4:2:2 12-bit 3-plane)
        2304,    // VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 (4:2:2 12-bit 2-plane)
        3456,    // VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 (4:4:4 12-bit 3-plane)
        2304,    // VK_FORMAT_G16B16G16R16_422_UNORM (4:2:2 16-bit packed)
        2304,    // VK_FORMAT_B16G16R16G16_422_UNORM (4:2:2 16-bit packed)
        1728,    // VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM (4:2:0 16-bit 3-plane)
        1728,    // VK_FORMAT_G16_B16R16_2PLANE_420_UNORM (4:2:0 16-bit 2-plane)
        2304,    // VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM (4:2:2 16-bit 3-plane)
        2304,    // VK_FORMAT_G16_B16R16_2PLANE_422_UNORM (4:2:2 16-bit 2-plane)
        3456,    // VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM (4:4:4 16-bit 3-plane)
    };

    int i = 0;
    for(VkFormat f : {
            VK_FORMAT_G8B8G8R8_422_UNORM,
            VK_FORMAT_B8G8R8G8_422_UNORM,
            VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
            VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
            VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
            VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
            VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
            VK_FORMAT_R10X6_UNORM_PACK16,
            VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
            VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
            VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
            VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
            VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
            VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
            VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
            VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
            VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
            VK_FORMAT_R12X4_UNORM_PACK16,
            VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
            VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
            VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
            VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
            VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
            VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
            VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
            VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
            VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
            VK_FORMAT_G16B16G16R16_422_UNORM,
            VK_FORMAT_B16G16R16G16_422_UNORM,
            VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
            VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
            VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
            VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
            VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
        })
    {
      INFO("Format is " << ToStr(f));

      CHECK(yuvsizes[i++] == GetByteSize(width, height, 1, f, 0));
    }
  };

  SECTION("GetPlaneByteSize for planar YUV formats")
  {
    const uint32_t width = 24, height = 24;

    std::vector<std::pair<VkFormat, std::vector<uint32_t> > > tests = {
        {VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, {576, 144, 144}},
        {VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, {576, 288}},
        {VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM, {576, 288, 288}},
        {VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, {576, 576}},
        {VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, {576, 576, 576}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, {1152, 288, 288}},
        {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, {1152, 576}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, {1152, 576, 576}},
        {VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, {1152, 1152}},
        {VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, {1152, 1152, 1152}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16, {1152, 288, 288}},
        {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, {1152, 576}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16, {1152, 576, 576}},
        {VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16, {1152, 1152}},
        {VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16, {1152, 1152, 1152}},
        {VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM, {1152, 288, 288}},
        {VK_FORMAT_G16_B16R16_2PLANE_420_UNORM, {1152, 576}},
        {VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM, {1152, 576, 576}},
        {VK_FORMAT_G16_B16R16_2PLANE_422_UNORM, {1152, 1152}},
        {VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM, {1152, 1152, 1152}},
    };

    for(std::pair<VkFormat, std::vector<uint32_t> > e : tests)
    {
      INFO("Format is " << ToStr(e.first));
      for(uint32_t p = 0; p < e.second.size(); p++)
        CHECK(e.second[p] == GetPlaneByteSize(width, height, 1, e.first, 0, p));
    }
  };

  SECTION("GetPlaneByteSize is consistent with GetByteSize")
  {
    const uint32_t width = 24, height = 24;

    for(VkFormat f : formats)
    {
      if(f == VK_FORMAT_UNDEFINED)
        continue;

      INFO("Format is " << ToStr(f));

      uint32_t planeCount = GetYUVPlaneCount(f);

      uint32_t planeSum = 0;
      for(uint32_t p = 0; p < planeCount; p++)
        planeSum += GetPlaneByteSize(width, height, 1, f, 0, p);

      CHECK(planeSum == GetByteSize(width, height, 1, f, 0));
    }
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
