/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2025 Baldur Karlsson
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
#include "../vk_replay.h"
#include "core/settings.h"

RDOC_DEBUG_CONFIG(bool, Vulkan_Debug_AllowDescriptorSetReuse, true,
                  "Allow the re-use of descriptor sets via vkResetDescriptorPool.");

RDOC_CONFIG(
    uint32_t, Vulkan_Debug_DangerousDescriptorSerialisation, 0,
    "DANGEROUS, MAY CAUSE CAPTURE FAILURES: Disable serialising plain untyped buffer descriptors.");

RDOC_CONFIG(bool, Vulkan_Debug_UseFastDescriptorLookup, true,
            "Use fast pattern-matching lookup to try to identify descriptors before falling back "
            "to trie lookup.");

uint32_t WrappedVulkan::DescriptorDataSize(VkDescriptorType type)
{
  return ::DescriptorDataSize(m_DescriptorBufferProperties, type);
}

void WrappedVulkan::EstimateDescriptorFormats()
{
  // we want to differentiate the descriptor in as few tests as possible. We don't necessarily care
  // if we falsely identify a descriptor format once we've isolated it down to one since worst case
  // we'd have to fall back to nothing.

  // create an image first so we can make a buffer on the same memory type and re-use the memory
  // allocation for everything. We make it with a format that should be guaranteed supported by all drivers
  VkImageCreateInfo imCreateInfo = {
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      NULL,
      VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
      VK_IMAGE_TYPE_2D,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      {128, 128, 1},
      1,
      1,
      VK_SAMPLE_COUNT_1_BIT,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
      NULL,
      VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage image = VK_NULL_HANDLE;
  VkResult vkr = ObjDisp(m_Device)->CreateImage(Unwrap(m_Device), &imCreateInfo, NULL, &image);
  CHECK_VKR(this, vkr);

  VkMemoryRequirements imgMrq = {0};
  ObjDisp(m_Device)->GetImageMemoryRequirements(Unwrap(m_Device), image, &imgMrq);

  // we make the memory at least 4MB since that's pretty modest still and gives us enough room to
  // try different buffer sizes to determine weird swizzling.
  const VkDeviceSize size = 0x400000;

  RDCASSERT(size >= imgMrq.size);

  VkMemoryAllocateFlagsInfo memFlags = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, 0,
      VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT};
  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &memFlags,
      size,
      GetGPULocalMemoryIndex(imgMrq.memoryTypeBits),
  };

  VkDeviceMemory memory = VK_NULL_HANDLE;
  vkr = ObjDisp(m_Device)->AllocateMemory(Unwrap(m_Device), &allocInfo, NULL, &memory);
  CHECK_VKR(this, vkr);

  // allocate a buffer onto the memory too. We assume that reasonable usage will not exclude the
  // image's memory type

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      VK_BUFFER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT |
          VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT,
      size,
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
  };
  VkBuffer buffer = VK_NULL_HANDLE;
  vkr = ObjDisp(m_Device)->CreateBuffer(Unwrap(m_Device), &bufInfo, NULL, &buffer);
  CHECK_VKR(this, vkr);

  VkMemoryRequirements bufMrq;
  ObjDisp(m_Device)->GetBufferMemoryRequirements(Unwrap(m_Device), buffer, &bufMrq);
  if((bufMrq.memoryTypeBits & (1 << allocInfo.memoryTypeIndex)) == 0)
  {
    RDCERR("Can't detect descriptor types, image memory type can't bind buffer");
    ObjDisp(m_Device)->FreeMemory(Unwrap(m_Device), memory, NULL);
    ObjDisp(m_Device)->DestroyImage(Unwrap(m_Device), image, NULL);
    ObjDisp(m_Device)->DestroyBuffer(Unwrap(m_Device), buffer, NULL);
    return;
  }

  ObjDisp(m_Device)->BindBufferMemory(Unwrap(m_Device), buffer, memory, 0);
  ObjDisp(m_Device)->BindImageMemory(Unwrap(m_Device), image, memory, 0);

  // create image view
  VkImageView imageView = VK_NULL_HANDLE;

  VkImageViewCreateInfo imgViewInfo = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      NULL,
      VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
      image,
      VK_IMAGE_VIEW_TYPE_2D,
      VK_FORMAT_R32G32B32A32_SFLOAT,
      {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
       VK_COMPONENT_SWIZZLE_IDENTITY},
      {
          VK_IMAGE_ASPECT_COLOR_BIT,
          0,
          VK_REMAINING_MIP_LEVELS,
          0,
          1,
      },
  };

  vkr = ObjDisp(m_Device)->CreateImageView(Unwrap(m_Device), &imgViewInfo, NULL, &imageView);
  CHECK_VKR(this, vkr);

  // make a couple of samplers also to be able to decode combined image/sampler layouts
  VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampInfo.minFilter = sampInfo.magFilter = VK_FILTER_NEAREST;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampInfo.mipLodBias = 1.6f;
  sampInfo.flags = VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

  VkSampler sampler;
  vkr = ObjDisp(m_Device)->CreateSampler(Unwrap(m_Device), &sampInfo, NULL, &sampler);
  CHECK_VKR(this, vkr);

  VkSampler altSampler;
  sampInfo.minFilter = sampInfo.magFilter = VK_FILTER_LINEAR;
  sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampInfo.addressModeU = sampInfo.addressModeV = sampInfo.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampInfo.mipLodBias = -1.6f;
  vkr = ObjDisp(m_Device)->CreateSampler(Unwrap(m_Device), &sampInfo, NULL, &altSampler);
  CHECK_VKR(this, vkr);

  VkBufferDeviceAddressInfo getInfo = {
      VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      NULL,
      buffer,
  };
  VkDeviceAddress addr = ObjDisp(m_Device)->GetBufferDeviceAddress(Unwrap(m_Device), &getInfo);

  DescriptorTrieNode::rangeToleranceMask = ~0ULL;

  m_DescriptorLookup.uniformBuffer =
      EstimateBufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, addr);
  m_DescriptorLookup.storageBuffer =
      EstimateBufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, addr);
  // we use a 4-byte format here to distinguish byte size from elem size, but otherwise don't need
  // to check multiple formats. There are no possible cases where there's ambiguity between *known*
  // descriptor formats. If a descriptor format looks like one of ours for this but not for other
  // formats there's not much we can do about that.
  //
  // we'd slightly prefer a 2-byte format but these aren't required and 4-byte does just as well
  m_DescriptorLookup.uniformTexelBuffer =
      EstimateBufferDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, addr, VK_FORMAT_R32_UINT);
  m_DescriptorLookup.storageTexelBuffer =
      EstimateBufferDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, addr, VK_FORMAT_R32_UINT);

  if(DescriptorDataSize(VK_DESCRIPTOR_TYPE_SAMPLER) == 4 &&
     DescriptorDataSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) == 4 &&
     DescriptorDataSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) == 4 &&
     DescriptorDataSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) == 4)
  {
    // there's only one possible encoding that puts this in 4 bytes
    m_DescriptorLookup.storage = m_DescriptorLookup.sampled = ImageDescriptorFormat::Indexed2012;

    m_DescriptorLookup.samplerPalette.resize(0xfff);
    m_DescriptorLookup.imageViewPalette.resize(0xfffff);
  }
  else
  {
    rdcpair<VkDescriptorType, ImageDescriptorFormat &> formats[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_DescriptorLookup.sampled},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_DescriptorLookup.storage},
    };

    VkDescriptorGetInfoEXT info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        NULL,
    };

    VkDescriptorImageInfo imginfo = {};
    info.data.pSampledImage = &imginfo;
    imginfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imginfo.imageView = imageView;

    uint64_t descData[8] = {};

    // we only compare the bottom 48 bits of pointers (after shifting appropriately) since some
    // descriptors stuff things in the upper bits
    const uint64_t ptrMask = ((1ULL << 48) - 1);

    // we allow any pointer within the image to work, since some may be offset (not likely for the
    // format we have chosen, but just in case)
    VkDeviceAddress imgBase = addr & ptrMask;
    VkDeviceAddress imgEnd = imgBase + imgMrq.size;

    for(size_t i = 0; i < ARRAY_COUNT(formats); i++)
    {
      size_t descSize = DescriptorDataSize(formats[i].first);

      info.type = formats[i].first;

      ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descData);

      if(descSize == 32 && ((descData[0] << 8) & ptrMask) >= imgBase &&
         ((descData[0] << 8) & ptrMask) < imgEnd)
      {
        formats[i].second = ImageDescriptorFormat::PointerShifted_32;
      }
      else if(descSize == 64 && ((descData[0] << 8) & ptrMask) >= imgBase &&
              ((descData[0] << 8) & ptrMask) < imgEnd)
      {
        formats[i].second = ImageDescriptorFormat::PointerShifted_64;
      }
      else if(descSize == 64 && (descData[2] & ptrMask) >= imgBase && (descData[2] & ptrMask) < imgEnd)
      {
        formats[i].second = ImageDescriptorFormat::Pointer2_64;
      }
      else if(descSize == 64 && (descData[4] & ptrMask) >= imgBase && (descData[4] & ptrMask) < imgEnd)
      {
        formats[i].second = ImageDescriptorFormat::Pointer2_64;
      }
      else
      {
        RDCERR("Couldn't determine %s descriptor format for image %llx-%llx",
               ToStr(info.type).c_str(), imgBase, imgEnd);
        // dump the descriptor
        for(uint32_t d = 0; d * 8 < descSize; d++)
          RDCLOG("[%u]: %llx", d, descData[d]);
      }
    }

    size_t combinedSize = m_DescriptorBufferProperties.combinedImageSamplerDescriptorSize;
    size_t sampledSize = m_DescriptorBufferProperties.sampledImageDescriptorSize;
    size_t samplerSize = m_DescriptorBufferProperties.samplerDescriptorSize;

    if(combinedSize == sampledSize + samplerSize)
    {
      m_DescriptorLookup.combinedSamplerOffset = (uint32_t)sampledSize;
    }
    else if(combinedSize >= sampledSize + sampledSize)
    {
      byte combined1[256] = {};
      byte combined2[256] = {};

      imginfo.sampler = sampler;
      info.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      size_t descSize = DescriptorDataSize(info.type);

      ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, combined1);

      imginfo.sampler = altSampler;

      ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, combined2);

      for(uint32_t i = 0; i < combinedSize; i++)
      {
        if(combined1[i] != combined2[i])
        {
          m_DescriptorLookup.combinedSamplerOffset = i;
          break;
        }
      }

      if(m_DescriptorLookup.combinedSamplerOffset < sampledSize)
      {
        RDCERR(
            "Unexpected descriptor difference at byte %u, less than sampled size %u in combined %u",
            m_DescriptorLookup.combinedSamplerOffset, sampledSize, combinedSize);
        m_DescriptorLookup.combinedSamplerOffset = (uint32_t)sampledSize;
      }
      else if(m_DescriptorLookup.combinedSamplerOffset + samplerSize < combinedSize)
      {
        if(memcmp(combined1 + m_DescriptorLookup.combinedSamplerOffset + samplerSize,
                  combined2 + m_DescriptorLookup.combinedSamplerOffset + samplerSize,
                  combinedSize - (m_DescriptorLookup.combinedSamplerOffset + samplerSize)) != 0)
        {
          RDCERR(
              "Unexpected descriptor difference after sampler at offset %u + size %u, before end "
              "of combined %u bytes",
              m_DescriptorLookup.combinedSamplerOffset, samplerSize, combinedSize);
        }
      }

      RDCLOG("Combined sampler offset is %u into %u byte combined descriptor",
             m_DescriptorLookup.combinedSamplerOffset, combinedSize);
    }
    else
    {
      RDCLOG("Unexpected combined size %u with sampled size %u and sampler size %u", combinedSize,
             sampledSize, samplerSize);
    }
  }

  // check for AS descriptors too, and do this last as it stomps the union a bit
  if(AccelerationStructures())
  {
    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    const VkAccelerationStructureCreateInfoKHR asCreateInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        NULL,
        VK_ACCELERATION_STRUCTURE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT,
        buffer,
        0,
        size,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        0x0,
    };
    vkr = ObjDisp(m_Device)->CreateAccelerationStructureKHR(Unwrap(m_Device), &asCreateInfo, NULL,
                                                            &as);
    CHECK_VKR(this, vkr);

    VkAccelerationStructureDeviceAddressInfoKHR asGetInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        NULL,
        as,
    };

    VkDeviceAddress asAddr =
        ObjDisp(m_Device)->GetAccelerationStructureDeviceAddressKHR(Unwrap(m_Device), &asGetInfo);

    m_DescriptorLookup.accelStructure =
        EstimateBufferDescriptor(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, asAddr);

    ObjDisp(m_Device)->DestroyAccelerationStructureKHR(Unwrap(m_Device), as, NULL);
  }

  // shutdown and destroy the objects we made
  ObjDisp(m_Device)->DestroySampler(Unwrap(m_Device), sampler, NULL);
  ObjDisp(m_Device)->DestroySampler(Unwrap(m_Device), altSampler, NULL);
  ObjDisp(m_Device)->DestroyImageView(Unwrap(m_Device), imageView, NULL);
  ObjDisp(m_Device)->DestroyImage(Unwrap(m_Device), image, NULL);
  ObjDisp(m_Device)->DestroyBuffer(Unwrap(m_Device), buffer, NULL);
  ObjDisp(m_Device)->FreeMemory(Unwrap(m_Device), memory, NULL);

  RDCLOG("Descriptor format estimates:");
  RDCLOG("  Uniform buffers: %s", ToStr(m_DescriptorLookup.uniformBuffer).c_str());
  RDCLOG("  Storage buffers: %s", ToStr(m_DescriptorLookup.storageBuffer).c_str());
  RDCLOG("  Uniform texel buffers: %s", ToStr(m_DescriptorLookup.uniformTexelBuffer).c_str());
  RDCLOG("  Storage texel buffers: %s", ToStr(m_DescriptorLookup.storageTexelBuffer).c_str());
  RDCLOG("  Accel Structs: %s", ToStr(m_DescriptorLookup.accelStructure).c_str());
  RDCLOG("  Sampled images: %s", ToStr(m_DescriptorLookup.sampled).c_str());
  RDCLOG("  Storage images: %s", ToStr(m_DescriptorLookup.storage).c_str());
}

BufferDescriptorFormat WrappedVulkan::EstimateBufferDescriptor(VkDescriptorType type,
                                                               VkDeviceAddress addr,
                                                               VkFormat texelFormat)
{
  BufferDescriptorFormat outFormat = BufferDescriptorFormat::UnknownBufferDescriptor;

  union
  {
    byte descriptorBytes[256];
    uint64_t descriptorU64[32];
  };

  VkDescriptorGetInfoEXT info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
  };

  info.type = type;

  const uint64_t texelSize =
      texelFormat == VK_FORMAT_UNDEFINED ? 1 : GetByteSize(1, 1, 1, texelFormat, 0);

  // start with a size that will never run afoul of alignment problems but isn't likely to be
  // misidentified by a random bit. This also stays under 64k which is the minimum limit for some buffers
  VkDeviceAddress byteSize = 0xd300;
  VkDeviceAddress elemSize = byteSize / texelSize;

  size_t descSize = DescriptorDataSize(info.type);

  VkDescriptorAddressInfoEXT bufinfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};

  if(info.type != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
  {
    bufinfo.address = addr;
    bufinfo.range = byteSize;
    bufinfo.format = texelFormat;
    info.data.pUniformBuffer = &bufinfo;
  }
  else
  {
    info.data.accelerationStructure = addr;
  }

  ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descriptorBytes);

  // we only compare the bottom 48 bits of pointers (after shifting appropriately) since some
  // descriptors stuff things in the upper bits
  const uint64_t ptrMask = ((1ULL << 48) - 1);

  addr &= ptrMask;

  if(addr == 0)
  {
    RDCERR("Invalid address returned");
  }
  else if(type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
  {
    if(descSize == 8 && (descriptorU64[0] & ptrMask) == addr)
      outFormat = BufferDescriptorFormat::Pointer_8;
    else if(descSize == 16 && (descriptorU64[0] & ptrMask) == addr)
      outFormat = BufferDescriptorFormat::Pointer0_16;
    else if(descSize == 32 && (descriptorU64[1] & ptrMask) == addr)
      outFormat = BufferDescriptorFormat::Pointer1_32;
    else if(descSize == 64 && (descriptorU64[2] & ptrMask) == addr)
      outFormat = BufferDescriptorFormat::Pointer2_64;
  }
  else if(descSize == 8 && descriptorU64[0] == ((bufinfo.address >> 4) | ((byteSize >> 4) << 45)))
  {
    // check alignment
    bufinfo.range = byteSize = 16;
    ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descriptorBytes);

    if((descriptorU64[0] & ptrMask) == ((bufinfo.address >> 4) | ((byteSize >> 4) << 45)))
    {
      outFormat = BufferDescriptorFormat::Packed_4519_Aligned16_8;
      DescriptorTrieNode::rangeToleranceMask &= 0xFULL;
    }
    else if((descriptorU64[0] & ptrMask) == ((bufinfo.address >> 4) | ((256ULL >> 4) << 45)))
    {
      outFormat = BufferDescriptorFormat::Packed_4519_Aligned256_8;
      DescriptorTrieNode::rangeToleranceMask &= 0xFFULL;
    }
    else
    {
      outFormat = BufferDescriptorFormat::UnknownBufferDescriptor;
    }
  }
  else if(descSize == 16)
  {
    if((descriptorU64[0] & ptrMask) == (bufinfo.address & ptrMask) &&
       (descriptorU64[1] & 0xffffffff) == elemSize)
      outFormat = BufferDescriptorFormat::Pointer_ElemSize_16;
    else if((descriptorU64[0] & ptrMask) == ((bufinfo.address & ptrMask) / texelSize) &&
            (descriptorU64[1] & 0xffffffff) == elemSize)
      outFormat = BufferDescriptorFormat::PointerDivided_ElemSize_16;
  }
  else if(descSize == 32)
  {
    if((descriptorU64[1] & ptrMask) == (bufinfo.address & ptrMask) &&
       (descriptorU64[0] >> 32) == byteSize)
      outFormat = BufferDescriptorFormat::ByteSize0_Pointer1_32;
    if((descriptorU64[4] & ptrMask) == (bufinfo.address & ptrMask) &&
       (descriptorU64[5] >> 32) == byteSize)
      outFormat = BufferDescriptorFormat::Pointer4_ByteSize5_Unaligned_64;
  }
  else if(descSize == 64)
  {
    // don't check ElemSize0_Pointer2_64 here due to possible aliasing with Strided*_MultiDescriptor_64
    /*
    if((descriptorU64[2] & ptrMask) == bufinfo.address && (descriptorU64[0] >> 32) == elemSize)
    {
      outFormat = BufferDescriptorFormat::ElemSize0_Pointer2_64;
    }
    else
    */

    if((descriptorU64[4] & ptrMask) == (bufinfo.address & ptrMask) &&
       (descriptorU64[5] >> 32) == byteSize)
    {
      // check alignment
      bufinfo.range = 16;
      ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descriptorBytes);

      if((descriptorU64[4] & ptrMask) == (bufinfo.address & ptrMask) && (descriptorU64[5] >> 32) == 64)
      {
        outFormat = BufferDescriptorFormat::Pointer4_ByteSize5_Aligned_64;
        DescriptorTrieNode::rangeToleranceMask &= 0x3FULL;
      }
      else if((descriptorU64[4] & ptrMask) == (bufinfo.address & ptrMask) &&
              (descriptorU64[5] >> 32) == 16)
      {
        outFormat = BufferDescriptorFormat::Pointer4_ByteSize5_Unaligned_64;
      }
    }
    else if((descriptorU64[4] & ptrMask) == (bufinfo.address & ptrMask))
    {
      // check for complex scattering, we sized the memory large enough for 3 million specifically to test this
      uint64_t inputs[3] = {256, 200, 3000000};
      uint64_t scattered[3] = {descriptorU64[1]};

      bufinfo.range = inputs[1];
      ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descriptorBytes);
      scattered[1] = descriptorU64[1];

      bufinfo.range = inputs[2];
      ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descriptorBytes);
      scattered[2] = descriptorU64[1];

      bool match = true;
      for(size_t i = 0; i < ARRAY_COUNT(inputs); i++)
      {
        uint64_t num = inputs[i] - 1;
        if(texelSize > 1)
          num = (inputs[i] / texelSize) - 1;

        if(texelSize == 1)
        {
          uint8_t x = num & 0xff;
          num = (num & ~0xff) + ((x & 0xfc) + 6 - (x & 0x3));
        }
        else if(texelSize == 2)
        {
          uint8_t x = num & 0xff;
          num = (num & ~0xff) + ((x & 0xfe) + 2 - (x & 0x1));
        }

        uint64_t expected = ((num & 0x00007f) << 0) | ((num & 0x1fff80) << 9) | ((num >> 21) << 53);
        if(expected != (scattered[i] & 0xffe00000001fffffULL))
        {
          match = false;
          break;
        }
      }

      if(match)
        outFormat = BufferDescriptorFormat::ElemSizeScattered1_Pointer4_64;
    }
  }

  // variable size descriptors
  if(outFormat == BufferDescriptorFormat::UnknownBufferDescriptor &&
     (descSize == 64 || descSize == 128))
  {
    // check with non-64byte aligned pointer just to check. 16 bytes is safe for all possible inputs
    bufinfo.address += 16;
    ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descSize, descriptorBytes);

    VkDeviceAddress remainder = bufinfo.address & 0x3f;
    VkDeviceAddress alignedAddr = (bufinfo.address & ptrMask) - remainder;
    RDCASSERT(remainder != 0, bufinfo.address);

    BufferDescriptorFormat fmts[] = {
        BufferDescriptorFormat::Strided4_MultiDescriptor_64,
        BufferDescriptorFormat::Strided2_MultiDescriptor_64,
        BufferDescriptorFormat::Strided1_MultiDescriptor_64,
    };
    uint32_t strides[] = {
        4,
        2,
        1,
    };
    RDCCOMPILE_ASSERT(ARRAY_COUNT(fmts) == ARRAY_COUNT(strides),
                      "Strides don't match number of descriptor formats");

    for(size_t i = 0; i < ARRAY_COUNT(fmts); i++)
    {
      uint32_t stride = strides[i];

      if((descriptorU64[2] & ptrMask) == alignedAddr &&
         descriptorU64[0] >> 32 == (byteSize >> stride) &&
         ((descriptorU64[1] >> 16) & 0x3f) == (remainder >> stride))
      {
        outFormat = fmts[i];
        break;
      }
    }

    if(outFormat == BufferDescriptorFormat::UnknownBufferDescriptor &&
       (descriptorU64[2] & ptrMask) == (bufinfo.address & ptrMask) &&
       (descriptorU64[0] >> 32) == elemSize)
    {
      outFormat = BufferDescriptorFormat::ElemSize0_Pointer2_64;
    }
  }

  if(outFormat == BufferDescriptorFormat::UnknownBufferDescriptor)
  {
    RDCERR("Couldn't determine %s descriptor format for address %llx range %llx",
           ToStr(info.type).c_str(), addr, bufinfo.range);
    // dump the descriptor
    for(uint32_t i = 0; i * 8 < descSize; i++)
      RDCLOG("[%u]: %llx", i, descriptorU64[i]);
  }

  return outFormat;
}

void WrappedVulkan::LookupDescriptor(byte *descriptorBytes, size_t descriptorSize,
                                     DescriptorType type, DescriptorSetSlot &data)
{
  const size_t combinedSize = m_DescriptorBufferProperties.combinedImageSamplerDescriptorSize;
  const size_t sampledSize = m_DescriptorBufferProperties.sampledImageDescriptorSize;
  const size_t samplerSize = m_DescriptorBufferProperties.samplerDescriptorSize;

  VkDescriptorGetInfoEXT info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
  };

  byte tempMem[256] = {};
  // start with the descriptor bytes in case the driver doesn't initialise them all. If this
  // contains random bytes from user memory we want it to match
  memcpy(tempMem, descriptorBytes, descriptorSize);
  if(Vulkan_Debug_UseFastDescriptorLookup())
  {
    switch(type)
    {
      case DescriptorType::Sampler:
      {
        ResourceId samp = GetSamplerForDescriptor(descriptorBytes, descriptorSize);

        if(samp != ResourceId())
        {
          data = {};
          data.SetSampler(samp);

          // verify that descriptor roundtrips that our detection was correct
          info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
          VkSampler sampler = Unwrap(GetResourceManager()->GetCurrentHandle<VkSampler>(samp));
          info.data.pSampler = &sampler;

          ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descriptorSize, tempMem);

          if(memcmp(tempMem, descriptorBytes, descriptorSize) == 0)
            return;
        }

        break;
      }
      case DescriptorType::ImageSampler:
      case DescriptorType::Image:
      case DescriptorType::ReadWriteImage:
      {
        if(type == DescriptorType::ImageSampler)
          info.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        else if(type == DescriptorType::Image)
          info.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        else if(type == DescriptorType::ReadWriteImage)
          info.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        ResourceId samp;
        ResourceId view;

        if(type == DescriptorType::ImageSampler)
        {
          if(m_DescriptorLookup.sampled == ImageDescriptorFormat::Indexed2012)
          {
            // for the indexed format, the sampler/view are encoded together
            view = GetImageViewForDescriptor(descriptorBytes, descriptorSize, type);
            samp = GetSamplerForDescriptor(descriptorBytes, descriptorSize);
          }
          else if(m_DescriptorLookup.sampled == ImageDescriptorFormat::UnknownImageDescriptor)
          {
            break;
          }
          else
          {
            // all known formats we expect an image view to be followed by a sampler
            if(combinedSize == m_DescriptorLookup.combinedSamplerOffset + samplerSize)
            {
              view = GetImageViewForDescriptor(descriptorBytes, sampledSize, type);
              samp = GetSamplerForDescriptor(
                  descriptorBytes + m_DescriptorLookup.combinedSamplerOffset, samplerSize);
            }
            else
            {
              RDCWARN(
                  "non-indexed combined image/sampler is not (padded) image followed by sampler");
            }
          }

          if(samp == ResourceId())
          {
            RDCWARN("Fast-detection failed to get sampler for image/sampler descriptor");
            break;
          }
        }
        else
        {
          view = GetImageViewForDescriptor(descriptorBytes, descriptorSize, type);
        }

        // exit silently, may be unknown descriptor format which would spam
        if(view == ResourceId())
        {
          // check if this is a NULL descriptor, in which case we've identified correctly!
          // only works if we don't need to care about the layout
          if(m_IgnoreLayoutForDescriptors &&
             m_DescriptorLookup.nullPatterns[(uint32_t)convert(info.type)] ==
                 bytebuf(descriptorBytes, descriptorSize))
          {
            data = {};
            data.SetImageSampler(info.type, ResourceId(), ResourceId(), VK_IMAGE_LAYOUT_GENERAL);
            return;
          }
          break;
        }

        rdcarray<VkImageLayout> layouts;

        if(!m_IgnoreLayoutForDescriptors)
        {
          layouts = m_DescriptorLookup.generalImageLayouts;

          if(m_CreationInfo.m_ImageView[view].isDepthImage)
            layouts.append(m_DescriptorLookup.depthImageLayouts);
        }

        VkDescriptorImageInfo imInfo = {};
        info.data.pCombinedImageSampler = &imInfo;

        imInfo.sampler = Unwrap(GetResourceManager()->GetCurrentHandle<VkSampler>(samp));
        imInfo.imageView = Unwrap(GetResourceManager()->GetCurrentHandle<VkImageView>(view));

        // always iterate at least once even if the layouts array is empty
        for(size_t i = 0; i < layouts.size() || (i == 0 && layouts.empty()); i++)
        {
          imInfo.imageLayout = i < layouts.size() ? layouts[i] : VK_IMAGE_LAYOUT_GENERAL;

          ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descriptorSize, tempMem);

          if(memcmp(tempMem, descriptorBytes, descriptorSize) == 0)
          {
            data.SetImageSampler(info.type, view, samp, imInfo.imageLayout);
            return;
          }
        }

        RDCWARN("Fast-detection failed for %s descriptor", ToStr(type).c_str());

        break;
      }
      case DescriptorType::TypedBuffer:
      case DescriptorType::ReadWriteTypedBuffer:
      case DescriptorType::ConstantBuffer:
      case DescriptorType::ReadWriteBuffer:
      case DescriptorType::AccelerationStructure:
      {
        if(type == DescriptorType::TypedBuffer)
          info.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        else if(type == DescriptorType::ReadWriteTypedBuffer)
          info.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        else if(type == DescriptorType::ConstantBuffer)
          info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        else if(type == DescriptorType::ReadWriteBuffer)
          info.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        else if(type == DescriptorType::AccelerationStructure)
          info.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

        const bool texelBuffer =
            type == DescriptorType::TypedBuffer || type == DescriptorType::ReadWriteTypedBuffer;

        VkDeviceAddress address;
        VkDeviceSize size;

        GetPointerAndSizeForDescriptor(descriptorBytes, descriptorSize, type, address, size);

        if(address == 0)
        {
          // exit silently, may be unknown descriptor format which would spam
          // check if this is a NULL descriptor, in which case we've identified correctly!
          // can't work for texel buffers that may have a format encoded
          if(type != DescriptorType::TypedBuffer && type != DescriptorType::ReadWriteTypedBuffer &&
             m_DescriptorLookup.nullPatterns[(uint32_t)convert(info.type)] ==
                 bytebuf(descriptorBytes, descriptorSize))
          {
            data = {};
            if(type == DescriptorType::AccelerationStructure)
              data.SetAccelerationStructure(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                            VK_NULL_HANDLE);
            else
              data.SetBuffer(info.type, ResourceId(), 0, 0, VK_FORMAT_UNDEFINED);
            return;
          }
          break;
        }
        else
        {
          VkDescriptorAddressInfoEXT bufinfo = {};
          info.data.pUniformBuffer = &bufinfo;

          if(type == DescriptorType::AccelerationStructure)
          {
            info.data.accelerationStructure = address;
          }
          else
          {
            bufinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
            bufinfo.address = address;
            bufinfo.range = size;
          }

          bufinfo.format = VK_FORMAT_UNDEFINED;
          for(size_t i = 0, n = texelBuffer ? m_DescriptorLookup.texelFormats.size() : 1; i < n; i++)
          {
            if(texelBuffer)
              bufinfo.format = m_DescriptorLookup.texelFormats[i];

            if(type != DescriptorType::AccelerationStructure)
            {
              // a couple of formats modify the address or size in non-trivial ways that need to be patched
              // here. This also handles converting an element size back into a byte size trivially
              GetFinalBufferParameters(descriptorBytes, descriptorSize, type, bufinfo.format,
                                       address, size, bufinfo.address, bufinfo.range);
            }

            ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descriptorSize, tempMem);

            if(memcmp(tempMem, descriptorBytes, descriptorSize) != 0)
            {
              // try sign extending if the top bit is set
              address |= (0xffffULL << 48);
              if(type == DescriptorType::AccelerationStructure)
                info.data.accelerationStructure = address;
              else
                bufinfo.address = address;

              ObjDisp(m_Device)->GetDescriptorEXT(Unwrap(m_Device), &info, descriptorSize, tempMem);
            }

            if(memcmp(tempMem, descriptorBytes, descriptorSize) == 0)
            {
              ResourceId id;
              VkDeviceSize offs;

              if(type != DescriptorType::AccelerationStructure)
              {
                GetResIDFromAddr(bufinfo.address, id, offs);

                if(id == ResourceId() && (bufinfo.address & (1ULL << 47)))
                {
                  // try sign extending if the top bit is set
                  GetResIDFromAddr(bufinfo.address | (0xffffULL << 48), id, offs);
                }

                if(id == ResourceId())
                {
                  RDCWARN("Unknown buffer at descriptor address %llx", bufinfo.address);
                }
                else
                {
                  data.SetBuffer(info.type, id, offs, bufinfo.range, bufinfo.format);

                  return;
                }
              }
              else
              {
                id = m_ASLookupByAddr[address];

                if(id == ResourceId() && (address & (1ULL << 47)))
                {
                  // try sign extending if the top bit is set
                  id = m_ASLookupByAddr[address | (0xffffULL << 48)];
                }

                if(id == ResourceId())
                {
                  RDCWARN("Unknown AS at descriptor address %llx", address);
                }
                else
                {
                  data.SetAccelerationStructure(
                      VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                      GetResourceManager()->GetCurrentHandle<VkAccelerationStructureKHR>(id));

                  return;
                }
              }
            }
          }

          RDCWARN("Fast-detection failed to get match for %s descriptor", ToStr(type).c_str());
        }

        break;
      }
      case DescriptorType::Buffer:
      case DescriptorType::Unknown: RDCERR("Invalid descriptor type being looked up"); break;
    }
  }

  data = m_DescriptorLookup.fallback.lookup({descriptorBytes, descriptorSize});

#if ENABLED(RDOC_DEVEL)
  if(!m_DescriptorLookup.fallback.contains({descriptorBytes, descriptorSize}))
  {
    RDCERR("Trie descriptor lookup failed");
    // dump the descriptor
    uint64_t *descriptorU64 = (uint64_t *)descriptorBytes;
    for(uint32_t i = 0; i * 8 < descriptorSize; i++)
      RDCLOG("  [%u]: %llx", i, descriptorU64[i]);
  }
#endif
}

ResourceId WrappedVulkan::GetSamplerForDescriptor(byte *descriptorBytes, size_t descriptorSize)
{
  if(m_DescriptorLookup.sampled == ImageDescriptorFormat::Indexed2012)
  {
    if(descriptorSize == sizeof(uint32_t))
    {
      uint32_t idx = (*(uint32_t *)descriptorBytes) >> 20;

      if(idx > 0 && idx < m_DescriptorLookup.samplerPalette.size())
      {
        return m_DescriptorLookup.samplerPalette[idx];
      }

      RDCWARN("Indexed sampler descriptor index is %u", idx);
      return ResourceId();
    }

    RDCWARN("Indexed sampler descriptor is %zu bytes", descriptorSize);
    return ResourceId();
  }

  DescriptorTrieNode data = m_DescriptorLookup.samplers.lookup({descriptorBytes, descriptorSize});
  // we should find this
  if(data.sampler == ResourceId())
    RDCWARN("Couldn't find solo sampler in sampler lookup trie");

  return data.sampler;
}

ResourceId WrappedVulkan::GetImageViewForDescriptor(byte *descriptorBytes, size_t descriptorSize,
                                                    DescriptorType type)
{
  ImageDescriptorFormat format = m_DescriptorLookup.sampled;
  if(type == DescriptorType::ReadWriteImage)
    format = m_DescriptorLookup.storage;

  // we assumed if one image type is indexed, all are
  if(format == ImageDescriptorFormat::Indexed2012)
  {
    if(descriptorSize == sizeof(uint32_t))
    {
      uint32_t idx = (*(uint32_t *)descriptorBytes) & 0xfffff;

      if(idx > 0 && idx < m_DescriptorLookup.imageViewPalette.size())
      {
        return m_DescriptorLookup.imageViewPalette[idx];
      }

      RDCWARN("Indexed view descriptor index is %u", idx);
      return ResourceId();
    }

    RDCWARN("Indexed view descriptor is %zu bytes", descriptorSize);
    return ResourceId();
  }

  // other descriptors are recognised by pointer
  uint64_t ptr = 0;

  if(format == ImageDescriptorFormat::PointerShifted_32 ||
     format == ImageDescriptorFormat::PointerShifted_64)
  {
    if(descriptorSize == 32 || descriptorSize == 64)
    {
      ptr = (((uint64_t *)descriptorBytes)[0] << 8) & ((1ULL << 48) - 1);
    }
    else
    {
      RDCWARN("Unexpected descriptor format for detected format %u: %u", format, descriptorSize);
      return ResourceId();
    }
  }
  else if(format == ImageDescriptorFormat::Pointer2_64)
  {
    if(descriptorSize == 64)
    {
      ptr = ((uint64_t *)descriptorBytes)[2] & ((1ULL << 48) - 1);
    }
    else
    {
      RDCWARN("Unexpected descriptor format for detected format %u: %u", format, descriptorSize);
      return ResourceId();
    }
  }
  else if(format == ImageDescriptorFormat::Pointer4_64)
  {
    if(descriptorSize == 64)
    {
      ptr = ((uint64_t *)descriptorBytes)[4] & ((1ULL << 48) - 1);
    }
    else
    {
      RDCWARN("Unexpected descriptor format for detected format %u: %u", format, descriptorSize);
      return ResourceId();
    }
  }
  else
  {
    return ResourceId();
  }

  ResourceId imageId;
  uint64_t unused;
  m_DescriptorLookup.imageAddresses.GetResIDFromAddr(ptr, imageId, unused);

  if(imageId == ResourceId() && (ptr & (1ULL << 47)))
  {
    // try sign extending if the top bit is set
    m_DescriptorLookup.imageAddresses.GetResIDFromAddr(ptr | (0xffffULL << 48), imageId, unused);
  }

  if(imageId == ResourceId())
  {
    RDCWARN("View descriptor gave unrecognised pointer %llx", ptr);
    return ResourceId();
  }

  ResourceId viewId =
      m_CreationInfo.m_Image[imageId].getViewFromDescriptor(descriptorBytes, descriptorSize);

  if(viewId == ResourceId())
  {
    RDCWARN("View descriptor gave pointer %llx for image %s but was unrecognised", ptr,
            ToStr(imageId).c_str());
    return ResourceId();
  }

  return viewId;
}

void WrappedVulkan::GetPointerAndSizeForDescriptor(byte *descriptorBytes, size_t descriptorSize,
                                                   DescriptorType type, VkDeviceAddress &address,
                                                   VkDeviceSize &size)
{
  address = 0;
  size = 0;

  BufferDescriptorFormat format = m_DescriptorLookup.uniformBuffer;
  if(type == DescriptorType::ReadWriteBuffer)
    format = m_DescriptorLookup.storageBuffer;
  else if(type == DescriptorType::TypedBuffer)
    format = m_DescriptorLookup.uniformTexelBuffer;
  else if(type == DescriptorType::ReadWriteTypedBuffer)
    format = m_DescriptorLookup.storageTexelBuffer;
  else if(type == DescriptorType::AccelerationStructure)
    format = m_DescriptorLookup.accelStructure;

  if(format == BufferDescriptorFormat::Pointer_8 && descriptorSize == sizeof(uint64_t))
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[0] & ((1ULL << 48) - 1));
  }
  else if(format == BufferDescriptorFormat::Packed_4519_Aligned16_8 &&
          descriptorSize == sizeof(uint64_t))
  {
    uint64_t packed = *(uint64_t *)descriptorBytes;
    address = (packed & ((1ULL << 45) - 1)) << 4;
    size = (packed >> 45) << 4;
  }
  else if(format == BufferDescriptorFormat::Packed_4519_Aligned256_8 &&
          descriptorSize == sizeof(uint64_t))
  {
    uint64_t packed = *(uint64_t *)descriptorBytes;
    address = (packed & ((1ULL << 45) - 1)) << 4;
    size = (packed >> 45) << 4;
  }
  else if(format == BufferDescriptorFormat::Pointer_ElemSize_16 &&
          descriptorSize == sizeof(uint64_t) * 2)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[0] & ((1ULL << 48) - 1));
    size = (packed[1] & 0xFFFFFFFFULL);
  }
  else if(format == BufferDescriptorFormat::PointerDivided_ElemSize_16 &&
          descriptorSize == sizeof(uint64_t) * 2)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[0] & ((1ULL << 48) - 1));
    size = (packed[1] & 0xFFFFFFFFULL);

    // address is incomplete, but can't be fixed until we know the texel size
  }
  else if(format == BufferDescriptorFormat::Pointer0_16 && descriptorSize == sizeof(uint64_t) * 2)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[0] & ((1ULL << 48) - 1));
  }
  else if(format == BufferDescriptorFormat::ByteSize0_Pointer1_32 &&
          descriptorSize == sizeof(uint64_t) * 4)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[1] & ((1ULL << 48) - 1));
    size = (packed[0] >> 32);
  }
  else if(format == BufferDescriptorFormat::Pointer1_32 && descriptorSize == sizeof(uint64_t) * 4)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[1] & ((1ULL << 48) - 1));
  }
  else if(format == BufferDescriptorFormat::Pointer2_64 && descriptorSize == sizeof(uint64_t) * 4)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[2] & ((1ULL << 48) - 1));
  }
  else if(format == BufferDescriptorFormat::Pointer4_ByteSize5_Unaligned_64 &&
          descriptorSize == sizeof(uint64_t) * 8)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[4] & ((1ULL << 48) - 1));
    size = (packed[5] >> 32);
  }
  else if(format == BufferDescriptorFormat::Pointer4_ByteSize5_Aligned_64 &&
          descriptorSize == sizeof(uint64_t) * 8)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[4] & ((1ULL << 48) - 1));
    size = (packed[5] >> 32);
  }
  else if(format == BufferDescriptorFormat::ElemSizeScattered1_Pointer4_64 &&
          descriptorSize == sizeof(uint64_t) * 8)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[4] & ((1ULL << 48) - 1));

    uint64_t sizeScattered = packed[1];
    size = (sizeScattered & 0x7f) | ((sizeScattered >> 9) & 0x1fff80) | ((sizeScattered >> 53) << 21);

    // this is still swizzled on the low bits, but we won't know how to decode that until we have the texel format
  }
  else if((format == BufferDescriptorFormat::Strided4_MultiDescriptor_64 ||
           format == BufferDescriptorFormat::Strided2_MultiDescriptor_64 ||
           format == BufferDescriptorFormat::Strided1_MultiDescriptor_64) &&
          (descriptorSize == sizeof(uint64_t) * 8 * 1 || descriptorSize == sizeof(uint64_t) * 8 * 2))
  {
    const uint32_t stride = format == BufferDescriptorFormat::Strided4_MultiDescriptor_64   ? 4
                            : format == BufferDescriptorFormat::Strided2_MultiDescriptor_64 ? 2
                                                                                            : 1;

    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[2] & ((1ULL << 48) - 1));

    address += ((packed[1] >> 16) & 0x3f) << stride;

    size = (packed[0] >> 32) << stride;
  }
  else if(format == BufferDescriptorFormat::ElemSize0_Pointer2_64 &&
          descriptorSize == sizeof(uint64_t) * 8)
  {
    uint64_t *packed = (uint64_t *)descriptorBytes;
    address = (packed[2] & ((1ULL << 48) - 1));
    size = (packed[0] >> 32);
  }
}

void WrappedVulkan::GetFinalBufferParameters(byte *descriptorBytes, size_t descriptorSize,
                                             DescriptorType type, VkFormat texelFormat,
                                             VkDeviceAddress inAddress, VkDeviceSize inSize,
                                             VkDeviceAddress &outAddress, VkDeviceSize &outSize)
{
  BufferDescriptorFormat format = m_DescriptorLookup.uniformBuffer;

  if(type == DescriptorType::ReadWriteBuffer)
    format = m_DescriptorLookup.storageBuffer;
  else if(type == DescriptorType::TypedBuffer)
    format = m_DescriptorLookup.uniformTexelBuffer;
  else if(type == DescriptorType::ReadWriteTypedBuffer)
    format = m_DescriptorLookup.storageTexelBuffer;
  else if(type == DescriptorType::AccelerationStructure)
    format = m_DescriptorLookup.accelStructure;

  uint32_t elemSize = 1;
  if(type == DescriptorType::TypedBuffer || type == DescriptorType::ReadWriteTypedBuffer)
    elemSize = GetByteSize(1, 1, 1, texelFormat, 0) & 0xffff;

  if(format == BufferDescriptorFormat::ElemSize0_Pointer2_64 ||
     format == BufferDescriptorFormat::Pointer_ElemSize_16)
  {
    outAddress = inAddress;
    outSize = inSize * elemSize;
  }
  else if(format == BufferDescriptorFormat::PointerDivided_ElemSize_16)
  {
    outAddress = inAddress * elemSize;

    // for elemSize==12 the address didn't divide evenly so we need to grab the remainder
    if(elemSize == 12)
    {
      uint64_t *packed = (uint64_t *)descriptorBytes;
      uint64_t remainder = (packed[1] >> 32) & 0x3f;
      // the actual pattern to this is entirely unknown and it's assumed to be some internal base
      // offset, but this seems consistent as the 3 remainders (0, 4, 8 bytes) are always 23 apart in these bits
      outAddress += 4 * (remainder / 23);
    }

    // outAddress += remainder;
    outSize = inSize * elemSize;
  }
  else if(format == BufferDescriptorFormat::ElemSizeScattered1_Pointer4_64)
  {
    outAddress = inAddress;

    if(elemSize >= 4)
    {
      outSize = (inSize + 1) * elemSize;
    }
    else
    {
      // unswizzle the 2-byte/1-byte size. There's probably a fancier way to express this
      // bit-twiddling but it's more readable to have this verbosely specified
      //
      // the general scheme for encoding is:
      //
      // lop off the bottom 8 bits, which we call 'x'
      //
      // swizzle those bits in this formula:
      // 1 byte elements: ((x & 0xfc) + 6 - (x & 0x3))
      // 2 byte elements: ((x & 0xfe) + 2 - (x & 0x1))
      //
      // note that this means the bottom bits can be swizzled into 9 bits of data and increment into
      // the upper bits, so we need to deal with carrying. E.g. for x = 0xfd this produces 0x102 result

      const uint32_t upperMask = elemSize == 1 ? 0xfc : 0xfe;
      const uint32_t lowerMask = 0xff - upperMask;
      const uint32_t offset = lowerMask << 1;

      // segment the lower swizzled bits. There may be leakage due to the carry bit but we handle that
      const uint64_t upperSize = inSize & ~0xff;
      const uint32_t lowerSize = inSize & 0xff;

      // need a carry bit for some cases, this will be subtracted later
      const uint32_t carry = lowerSize < offset ? 128 : 0;

      // this is (mostly) the result of the (x & 0xfc) - (x & 0x3) subtraction
      const uint32_t xsubbed = lowerSize + carry - offset;

      // assuming a lower mask of 0x3 (bottom two bits) if xsubbed ends in 00 then it must have been
      // aligned, if it ended in 11 then it must have been 01 subtracted from the value above, etc.
      // this will give us the original bottom two bits of x by subtracting and masking
      const uint32_t xlow = ((lowerMask + 1) - (xsubbed & lowerMask)) & lowerMask;

      // the upper bits of the size are added on unconditionally, we also undo the +1 to the range here
      outSize = upperSize + 1;

      // no bits = no subtraction! x must have been aligned when we did the sum so just the upperMask bits are used
      if(xlow == 0)
        outSize += xsubbed;
      else
        // xlow had some bits, so we figure out what it must have been subtracted from and add that to the upper mask
        outSize += ((xsubbed & upperMask) + (lowerMask + 1) + xlow);

      // subtract any carry we added now
      outSize -= carry;

      // finally convert to bytes
      outSize *= elemSize;
    }
  }
  else
  {
    // byte sizes, no translation needed
    outAddress = inAddress;
    outSize = inSize;
  }
}

void WrappedVulkan::RegisterDescriptor(const bytebuf &key, const DescriptorSetSlot &data)
{
  m_DescriptorLookup.fallback.insert(key, data);

  // only register NULL patterns for non-sampler types
  if(data.type != DescriptorSlotType::Sampler &&
     data.type != DescriptorSlotType::CombinedImageSampler && data.resource == ResourceId())
    m_DescriptorLookup.nullPatterns[(uint32_t)data.type] = key;

  // store unique texel buffer formats used, expecting this to be small and it will help descriptor lookups
  if(data.type == DescriptorSlotType::UniformTexelBuffer ||
     data.type == DescriptorSlotType::StorageTexelBuffer)
  {
    VkFormat fmt = VkFormat(data.imageLayoutOrFormat);
    if(data.resource != ResourceId() && !m_DescriptorLookup.texelFormats.contains(fmt))
      m_DescriptorLookup.texelFormats.push_back(fmt);
  }
  else if(data.type == DescriptorSlotType::CombinedImageSampler ||
          data.type == DescriptorSlotType::SampledImage ||
          data.type == DescriptorSlotType::StorageImage ||
          data.type == DescriptorSlotType::InputAttachment)
  {
    VkImageLayout layout = convert(data.imageLayoutOrFormat);

    if(layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
       layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
       layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
       layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL ||
       layout == VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL)
    {
      if(!m_DescriptorLookup.depthImageLayouts.contains(layout))
        m_DescriptorLookup.depthImageLayouts.push_back(layout);
    }
    else
    {
      if(!m_DescriptorLookup.generalImageLayouts.contains(layout))
      {
        m_DescriptorLookup.generalImageLayouts.push_back(layout);
        // keep the list sorted so that rare/niche layouts like feedback loop or local read are tried last
        std::sort(m_DescriptorLookup.generalImageLayouts.begin(),
                  m_DescriptorLookup.generalImageLayouts.end());
      }
    }
  }

  size_t combinedSize = m_DescriptorBufferProperties.combinedImageSamplerDescriptorSize;
  size_t sampledSize = m_DescriptorBufferProperties.sampledImageDescriptorSize;
  size_t samplerSize = m_DescriptorBufferProperties.samplerDescriptorSize;

  // if this is just a sampler descriptor, store it directly (unless we're not using indexed)
  if(data.type == DescriptorSlotType::Sampler &&
     m_DescriptorLookup.sampled != ImageDescriptorFormat::Indexed2012)
  {
    m_DescriptorLookup.samplers.insert(key, data);
  }
  // if this is a combined descriptor but it looks like it's an image+sampler (which is common) and
  // we're not indexed, store the second part as sampler bytes. This may be wrong, but that's fine
  // and worst case we pollute the samplers lookup and fail to do a fast lookup of this descriptor
  else if(data.type == DescriptorSlotType::CombinedImageSampler &&
          combinedSize == m_DescriptorLookup.combinedSamplerOffset + samplerSize)
  {
    DescriptorSetSlot samplerData;
    samplerData.SetSampler(data.sampler);

    m_DescriptorLookup.samplers.insert(
        {key.data() + m_DescriptorLookup.combinedSamplerOffset, samplerSize}, samplerData);
  }

  if((data.type == DescriptorSlotType::InputAttachment ||
      data.type == DescriptorSlotType::StorageImage || data.type == DescriptorSlotType::SampledImage ||
      data.type == DescriptorSlotType::CombinedImageSampler) &&
     m_DescriptorLookup.sampled != ImageDescriptorFormat::Indexed2012)
  {
    m_CreationInfo.m_Image[m_CreationInfo.m_ImageView[data.resource].image].viewDescriptors.push_back(
        {bytebuf(key.data(), sampledSize), data.resource});
  }
}

template <>
VkDescriptorSetLayoutCreateInfo WrappedVulkan::UnwrapInfo(const VkDescriptorSetLayoutCreateInfo *info)
{
  VkDescriptorSetLayoutCreateInfo ret = *info;

  size_t tempmemSize = sizeof(VkDescriptorSetLayoutBinding) * info->bindingCount;

  // need to count how many VkSampler arrays to allocate for
  for(uint32_t i = 0; i < info->bindingCount; i++)
    if(info->pBindings[i].pImmutableSamplers)
      tempmemSize += info->pBindings[i].descriptorCount * sizeof(VkSampler);

  byte *memory = GetTempMemory(tempmemSize);

  VkDescriptorSetLayoutBinding *unwrapped = (VkDescriptorSetLayoutBinding *)memory;
  VkSampler *nextSampler = (VkSampler *)(unwrapped + info->bindingCount);

  for(uint32_t i = 0; i < info->bindingCount; i++)
  {
    unwrapped[i] = info->pBindings[i];

    if((unwrapped[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        unwrapped[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
       unwrapped[i].pImmutableSamplers)
    {
      VkSampler *unwrappedSamplers = nextSampler;
      nextSampler += unwrapped[i].descriptorCount;
      for(uint32_t j = 0; j < unwrapped[i].descriptorCount; j++)
        unwrappedSamplers[j] = Unwrap(unwrapped[i].pImmutableSamplers[j]);
      unwrapped[i].pImmutableSamplers = unwrappedSamplers;
    }
  }

  ret.pBindings = unwrapped;

  return ret;
}

template <>
VkDescriptorSetAllocateInfo WrappedVulkan::UnwrapInfo(const VkDescriptorSetAllocateInfo *info)
{
  VkDescriptorSetAllocateInfo ret = *info;

  VkDescriptorSetLayout *layouts = GetTempArray<VkDescriptorSetLayout>(info->descriptorSetCount);

  ret.descriptorPool = Unwrap(ret.descriptorPool);
  for(uint32_t i = 0; i < info->descriptorSetCount; i++)
    layouts[i] = Unwrap(info->pSetLayouts[i]);
  ret.pSetLayouts = layouts;

  return ret;
}

template <>
VkDescriptorUpdateTemplateCreateInfo WrappedVulkan::UnwrapInfo(
    const VkDescriptorUpdateTemplateCreateInfo *info)
{
  VkDescriptorUpdateTemplateCreateInfo ret = *info;

  if(ret.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS)
    ret.pipelineLayout = Unwrap(ret.pipelineLayout);
  if(ret.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
    ret.descriptorSetLayout = Unwrap(ret.descriptorSetLayout);

  return ret;
}

template <>
VkWriteDescriptorSet WrappedVulkan::UnwrapInfo(const VkWriteDescriptorSet *writeDesc)
{
  VkWriteDescriptorSet ret = *writeDesc;

  byte *memory = GetTempMemory(sizeof(VkDescriptorBufferInfo) * writeDesc->descriptorCount);

  VkDescriptorBufferInfo *bufInfos = (VkDescriptorBufferInfo *)memory;
  VkDescriptorImageInfo *imInfos = (VkDescriptorImageInfo *)memory;
  VkBufferView *bufViews = (VkBufferView *)memory;

  ret.dstSet = Unwrap(ret.dstSet);

  // nothing to unwrap for inline uniform block
  if(ret.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    return ret;

  RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                    "Structure sizes mean not enough space is allocated for write data");
  RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkBufferView),
                    "Structure sizes mean not enough space is allocated for write data");

  // unwrap and assign the appropriate array
  if(ret.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
     ret.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
  {
    for(uint32_t j = 0; j < ret.descriptorCount; j++)
      bufViews[j] = Unwrap(ret.pTexelBufferView[j]);
    ret.pTexelBufferView = bufViews;
  }
  else if(ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
  {
    bool hasSampler = (ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                       ret.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    bool hasImage = (ret.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                     ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                     ret.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                     ret.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

    for(uint32_t j = 0; j < ret.descriptorCount; j++)
    {
      if(hasImage)
        imInfos[j].imageView = Unwrap(ret.pImageInfo[j].imageView);
      else
        imInfos[j].imageView = VK_NULL_HANDLE;

      if(hasSampler)
        imInfos[j].sampler = Unwrap(ret.pImageInfo[j].sampler);
      else
        imInfos[j].sampler = VK_NULL_HANDLE;

      imInfos[j].imageLayout = ret.pImageInfo[j].imageLayout;
    }
    ret.pImageInfo = imInfos;
  }
  else if(ret.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
  {
    byte *asDescMemory = GetTempMemory(sizeof(VkWriteDescriptorSetAccelerationStructureKHR) +
                                       (sizeof(VkAccelerationStructureKHR) * ret.descriptorCount));
    VkAccelerationStructureKHR *unwrappedASs =
        (VkAccelerationStructureKHR *)(asDescMemory +
                                       sizeof(VkWriteDescriptorSetAccelerationStructureKHR));
    VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
        (VkWriteDescriptorSetAccelerationStructureKHR *)memcpy(
            asDescMemory, ret.pNext, sizeof(VkWriteDescriptorSetAccelerationStructureKHR));

    for(uint32_t j = 0; j < ret.descriptorCount; j++)
    {
      unwrappedASs[j] = Unwrap(asWrite->pAccelerationStructures[j]);
    }
    asWrite->pAccelerationStructures = unwrappedASs;

    ret.pNext = asWrite;
  }
  else
  {
    for(uint32_t j = 0; j < ret.descriptorCount; j++)
    {
      bufInfos[j].buffer = Unwrap(ret.pBufferInfo[j].buffer);
      bufInfos[j].offset = ret.pBufferInfo[j].offset;
      bufInfos[j].range = ret.pBufferInfo[j].range;
    }
    ret.pBufferInfo = bufInfos;
  }

  return ret;
}

template <>
VkCopyDescriptorSet WrappedVulkan::UnwrapInfo(const VkCopyDescriptorSet *copyDesc)
{
  VkCopyDescriptorSet ret = *copyDesc;

  ret.dstSet = Unwrap(ret.dstSet);
  ret.srcSet = Unwrap(ret.srcSet);

  return ret;
}

template <>
VkDescriptorGetInfoEXT WrappedVulkan::UnwrapInfo(const VkDescriptorGetInfoEXT *pDescriptorInfo)
{
  VkDescriptorGetInfoEXT ret = *pDescriptorInfo;

  byte *memory = GetTempMemory(sizeof(VkDescriptorAddressInfoEXT) + GetNextPatchSize(ret.pNext));
  RDCCOMPILE_ASSERT(sizeof(VkDescriptorAddressInfoEXT) >= sizeof(VkDescriptorImageInfo),
                    "Structure sizes mean not enough space is allocated for write data");

  if(pDescriptorInfo->data.pUniformBuffer)
    ret.data.pUniformBuffer = (VkDescriptorAddressInfoEXT *)memory;

  byte *nextMem = memory + sizeof(VkDescriptorBufferInfo);

  UnwrapNextChain(m_State, "VkDescriptorGetInfoEXT", nextMem, (VkBaseInStructure *)&ret);

  switch(ret.type)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    {
      VkSampler *samp = (VkSampler *)memory;
      if(pDescriptorInfo->data.pSampler)
      {
        *samp = Unwrap(*pDescriptorInfo->data.pSampler);
      }
      break;
    }
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      // ignore the sampler part
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    {
      VkDescriptorImageInfo *img = (VkDescriptorImageInfo *)ret.data.pCombinedImageSampler;
      if(pDescriptorInfo->data.pCombinedImageSampler)
      {
        img->imageView = Unwrap(pDescriptorInfo->data.pCombinedImageSampler->imageView);
        img->sampler = Unwrap(pDescriptorInfo->data.pCombinedImageSampler->sampler);
        img->imageLayout = pDescriptorInfo->data.pCombinedImageSampler->imageLayout;
      }
      break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    {
      // no unwrap, just copy
      VkDescriptorAddressInfoEXT *buf = (VkDescriptorAddressInfoEXT *)ret.data.pUniformBuffer;
      if(pDescriptorInfo->data.pUniformBuffer)
        *buf = *pDescriptorInfo->data.pUniformBuffer;
      break;
    }
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    {
      // no unwrap, just copy
      ret.data.accelerationStructure = pDescriptorInfo->data.accelerationStructure;
      break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
    case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
    case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
    case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
    case VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV:
    case VK_DESCRIPTOR_TYPE_TENSOR_ARM:
    case VK_DESCRIPTOR_TYPE_MAX_ENUM:
    {
      RDCERR("Invalid descriptor type %s", ToStr(ret.type).c_str());
      break;
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDescriptorPool(SerialiserType &ser, VkDevice device,
                                                     const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkDescriptorPool *pDescriptorPool)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(DescriptorPool, GetResID(*pDescriptorPool))
      .TypedAs("VkDescriptorPool"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorPool pool = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &CreateInfo, NULL, &pool);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor pool, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
      GetResourceManager()->AddLiveResource(DescriptorPool, pool);

      m_CreationInfo.m_DescSetPool[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
    }

    AddResource(DescriptorPool, ResourceType::Pool, "Descriptor Pool");
    DerivedResource(device, DescriptorPool);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorPool(VkDevice device,
                                               const VkDescriptorPoolCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *,
                                               VkDescriptorPool *pDescriptorPool)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pCreateInfo, NULL,
                                                                  pDescriptorPool));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorPool);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDescriptorPool);
        Serialise_vkCreateDescriptorPool(ser, device, pCreateInfo, NULL, pDescriptorPool);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDescriptorPool);
      record->AddChunk(chunk);

      record->descPoolInfo = new DescPoolInfo;
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pDescriptorPool);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDescriptorSetLayout(
    SerialiserType &ser, VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(SetLayout, GetResID(*pSetLayout)).TypedAs("VkDescriptorSetLayout"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(&CreateInfo);

    // on replay we add compute access to any vertex descriptors, so that we can access them for
    // mesh output. This is only needed if we are using buffer device address and update-after-bind
    // descriptors, because non update-after-bind descriptors we can duplicate and patch. However to
    // keep things simple we just always do this whenever using BDA
    if(GetExtensions(NULL).ext_KHR_buffer_device_address ||
       GetExtensions(NULL).ext_EXT_buffer_device_address)
    {
      for(uint32_t b = 0; b < unwrapped.bindingCount; b++)
      {
        VkDescriptorSetLayoutBinding &bind = (VkDescriptorSetLayoutBinding &)unwrapped.pBindings[b];
        if(bind.stageFlags & VK_SHADER_STAGE_VERTEX_BIT)
          bind.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
      }
    }

    if(m_DescriptorBuffers && GetDriverInfo().NVDescriptorBufferExtraBinding() &&
       (unwrapped.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT))
    {
      if(unwrapped.bindingCount > 0)
      {
        VkDescriptorSetLayoutBinding &bind = (VkDescriptorSetLayoutBinding &)unwrapped.pBindings[0];
        bind.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
      }
    }

    VkResult ret =
        ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrapped, NULL, &layout);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor layout, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(layout)))
      {
        live = GetResourceManager()->GetNonDispWrapper(layout)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyDescriptorSetLayout(Unwrap(device), layout, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(SetLayout, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), layout);
        GetResourceManager()->AddLiveResource(SetLayout, layout);

        m_CreationInfo.m_DescSetLayout[live].Init(GetResourceManager(), m_CreationInfo, live,
                                                  &CreateInfo);

        if((CreateInfo.flags & (VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
                                VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT)) ==
           VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
        {
          // fetch actual offsets. Sizes we defer due to possible mutable descriptors
          rdcarray<DescSetLayout::Binding> &bindings = m_CreationInfo.m_DescSetLayout[live].bindings;
          for(uint32_t b = 0; b < bindings.size(); b++)
          {
            if(bindings[b].layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
              continue;

            VkDeviceSize offs = 0;
            ObjDisp(device)->GetDescriptorSetLayoutBindingOffsetEXT(Unwrap(device), Unwrap(layout),
                                                                    b, &offs);
            bindings[b].elemOffset = offs & 0xffffffffU;
            RDCASSERTEQUAL(bindings[b].elemOffset, offs);
          }
        }
      }

      AddResource(SetLayout, ResourceType::ShaderBinding, "Descriptor Layout");
      DerivedResource(device, SetLayout);

      for(uint32_t i = 0; i < CreateInfo.bindingCount; i++)
      {
        bool usesSampler =
            CreateInfo.pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            CreateInfo.pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        if(usesSampler && CreateInfo.pBindings[i].pImmutableSamplers != NULL)
        {
          for(uint32_t d = 0; d < CreateInfo.pBindings[i].descriptorCount; d++)
            DerivedResource(CreateInfo.pBindings[i].pImmutableSamplers[d], SetLayout);
        }
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorSetLayout(VkDevice device,
                                                    const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *,
                                                    VkDescriptorSetLayout *pSetLayout)
{
  VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(pCreateInfo);

  if(m_DescriptorBuffers &&
     (unwrapped.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT))
  {
    for(uint32_t b = 0; b < unwrapped.bindingCount; b++)
    {
      VkDescriptorSetLayoutBinding &bind = (VkDescriptorSetLayoutBinding &)unwrapped.pBindings[b];
      if(b == 0 || (bind.stageFlags & VK_SHADER_STAGE_VERTEX_BIT))
      {
        bind.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
      }
    }
  }

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrapped,
                                                                       NULL, pSetLayout));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSetLayout);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDescriptorSetLayout);
        Serialise_vkCreateDescriptorSetLayout(ser, device, pCreateInfo, NULL, pSetLayout);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSetLayout);
      record->AddChunk(chunk);

      record->descInfo = new DescriptorSetData();
      record->descInfo->layout = new DescSetLayout();
      record->descInfo->layout->Init(GetResourceManager(), m_CreationInfo, id, pCreateInfo);

      for(uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
      {
        bool usesSampler =
            pCreateInfo->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            pCreateInfo->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        if(usesSampler && pCreateInfo->pBindings[i].pImmutableSamplers != NULL)
        {
          for(uint32_t d = 0; d < pCreateInfo->pBindings[i].descriptorCount; d++)
            record->AddParent(GetRecord(pCreateInfo->pBindings[i].pImmutableSamplers[d]));
        }
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pSetLayout);

      m_CreationInfo.m_DescSetLayout[id].Init(GetResourceManager(), m_CreationInfo, id, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkAllocateDescriptorSets(SerialiserType &ser, VkDevice device,
                                                       const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                       VkDescriptorSet *pDescriptorSets)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo).Important();
  SERIALISE_ELEMENT_LOCAL(DescriptorSet, GetResID(*pDescriptorSets)).TypedAs("VkDescriptorSet"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorSet descset = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo unwrapped = UnwrapInfo(&AllocateInfo);
    VkResult ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

    if(ret != VK_SUCCESS)
    {
      RDCWARN(
          "Failed to allocate descriptor set %s from pool %s on replay. Assuming pool was "
          "reset and re-used mid-capture, so overflowing.",
          ToStr(DescriptorSet).c_str(),
          ToStr(GetResourceManager()->GetOriginalID(GetResID(AllocateInfo.descriptorPool))).c_str());

      VulkanCreationInfo::DescSetPool &poolInfo =
          m_CreationInfo.m_DescSetPool[GetResID(AllocateInfo.descriptorPool)];

      if(poolInfo.overflow.empty())
      {
        RDCLOG("Creating first overflow pool");
        poolInfo.CreateOverflow(device, GetResourceManager());
      }

      // first try and use the most recent overflow pool
      unwrapped.descriptorPool = Unwrap(poolInfo.overflow.back());

      ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

      // if we got an error, maybe the latest overflow pool is full. Try to create a new one and use
      // that
      if(ret != VK_SUCCESS)
      {
        RDCLOG("Creating new overflow pool, last pool failed with %s", ToStr(ret).c_str());
        poolInfo.CreateOverflow(device, GetResourceManager());

        unwrapped.descriptorPool = Unwrap(poolInfo.overflow.back());

        ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

        if(ret != VK_SUCCESS)
        {
          SET_ERROR_RESULT(
              m_FailedReplayResult, ResultCode::APIReplayFailed,
              "Failed allocating descriptor sets, even after trying to overflow pool, VkResult: %s",
              ToStr(ret).c_str());
          return false;
        }
      }
    }

    // if we got here we must have succeeded
    RDCASSERTEQUAL(ret, VK_SUCCESS);

    ResourceId layoutId = GetResID(AllocateInfo.pSetLayouts[0]);

    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), descset);
      GetResourceManager()->AddLiveResource(DescriptorSet, descset);

      // this is stored in the resource record on capture, we need to be able to look to up
      m_DescriptorSetState[live].layout = layoutId;

      // If descriptorSetCount is zero or this structure is not included in the pNext chain,
      // then the variable lengths are considered to be zero.
      uint32_t variableDescriptorAlloc = 0;

      if(!m_CreationInfo.m_DescSetLayout[layoutId].bindings.empty() &&
         m_CreationInfo.m_DescSetLayout[layoutId].bindings.back().variableSize)
      {
        const VkDescriptorSetVariableDescriptorCountAllocateInfo *variableAlloc =
            (const VkDescriptorSetVariableDescriptorCountAllocateInfo *)FindNextStruct(
                &AllocateInfo,
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

        if(variableAlloc && variableAlloc->descriptorSetCount > 0)
        {
          // this struct will have been patched similar to VkDescriptorSetAllocateInfo so we look up
          // the [0]th element
          variableDescriptorAlloc = variableAlloc->pDescriptorCounts[0];
        }
      }

      m_CreationInfo.m_DescSetLayout[layoutId].CreateBindingsArray(m_DescriptorSetState[live].data,
                                                                   variableDescriptorAlloc);
    }

    AddResource(DescriptorSet, ResourceType::DescriptorStore, "Descriptor Set");
    DerivedResource(device, DescriptorSet);
    DerivedResource(AllocateInfo.pSetLayouts[0], DescriptorSet);
    DerivedResource(AllocateInfo.descriptorPool, DescriptorSet);

    DescriptorStoreDescription desc;
    desc.resourceId = DescriptorSet;
    desc.descriptorByteSize = 1;
    // descriptors are stored after all the inline bytes
    desc.firstDescriptorOffset = m_CreationInfo.m_DescSetLayout[layoutId].inlineByteSize;
    desc.descriptorCount =
        (uint32_t)m_DescriptorSetState[GetResID(descset)].data.totalDescriptorCount();
    GetReplay()->RegisterDescriptorStore(desc);
  }

  return true;
}

VkResult WrappedVulkan::vkAllocateDescriptorSets(VkDevice device,
                                                 const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                 VkDescriptorSet *pDescriptorSets)
{
  VkDescriptorSetAllocateInfo unwrapped = UnwrapInfo(pAllocateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, pDescriptorSets));

  if(ret != VK_SUCCESS)
    return ret;

  const VkDescriptorSetVariableDescriptorCountAllocateInfo *variableAlloc =
      (const VkDescriptorSetVariableDescriptorCountAllocateInfo *)FindNextStruct(
          pAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

  VkDescriptorSetAllocateInfo mutableInfo = *pAllocateInfo;

  {
    byte *tempMem = GetTempMemory(GetNextPatchSize(mutableInfo.pNext));
    CopyNextChainForPatching("VkDescriptorSetAllocateInfo", tempMem,
                             (VkBaseInStructure *)&mutableInfo);
  }

  VkDescriptorSetVariableDescriptorCountAllocateInfo *mutableVariableInfo =
      (VkDescriptorSetVariableDescriptorCountAllocateInfo *)FindNextStruct(
          &mutableInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
  {
    VkResourceRecord *poolrecord = NULL;
    VkResourceRecord *layoutRecord = NULL;

    ResourceId id;
    VkResourceRecord *record = NULL;
    bool exactReuse = false;
    uint32_t variableDescriptorAlloc = 0;

    if(IsCaptureMode(m_State))
    {
      layoutRecord = GetRecord(pAllocateInfo->pSetLayouts[i]);
      poolrecord = GetRecord(pAllocateInfo->descriptorPool);

      if(!layoutRecord->descInfo->layout->bindings.empty() &&
         layoutRecord->descInfo->layout->bindings.back().variableSize && variableAlloc &&
         variableAlloc->descriptorSetCount > 0)
      {
        variableDescriptorAlloc = variableAlloc->pDescriptorCounts[i];
      }

      if(Atomic::CmpExch32(&m_ReuseEnabled, 1, 1) == 1)
      {
        rdcarray<VkResourceRecord *> &freelist = poolrecord->descPoolInfo->freelist;

        if(!freelist.empty())
        {
          DescSetLayout *search = layoutRecord->descInfo->layout;

          // try to find an exact layout match, then we don't need to re-initialise the descriptor
          // set.
          auto it = std::lower_bound(freelist.begin(), freelist.end(), search,
                                     [](VkResourceRecord *a, DescSetLayout *search) {
                                       return a->descInfo->layout < search;
                                     });

          if(it != freelist.end() && (*it)->descInfo->layout == layoutRecord->descInfo->layout &&
             (*it)->descInfo->data.variableDescriptorCount == variableDescriptorAlloc)
          {
            record = freelist.takeAt(it - freelist.begin());
            exactReuse = true;
          }
          else
          {
            record = freelist.back();
            freelist.pop_back();
          }

          if(!exactReuse)
            record->DeleteChunks();
        }
      }
    }

    if(record)
      id = GetResourceManager()->WrapReusedResource(record, pDescriptorSets[i]);
    else
      id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

    if(IsCaptureMode(m_State))
    {
      if(record == NULL)
      {
        record = GetResourceManager()->AddResourceRecord(pDescriptorSets[i]);

        poolrecord->LockChunks();
        poolrecord->pooledChildren.push_back(record);
        poolrecord->UnlockChunks();

        record->pool = poolrecord;

        // only mark descriptor set as dirty if it's not a push descriptor layout
        if((layoutRecord->descInfo->layout->flags &
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT) == 0)
        {
          GetResourceManager()->MarkDirtyResource(id);
        }

        record->descInfo = new DescriptorSetData();
      }

      if(!exactReuse)
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          VkDescriptorSetAllocateInfo info = mutableInfo;
          info.descriptorSetCount = 1;
          info.pSetLayouts = mutableInfo.pSetLayouts + i;

          if(mutableVariableInfo && variableAlloc->descriptorSetCount > 0)
          {
            mutableVariableInfo->descriptorSetCount = 1;
            mutableVariableInfo->pDescriptorCounts = variableAlloc->pDescriptorCounts + i;
          }

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkAllocateDescriptorSets);
          Serialise_vkAllocateDescriptorSets(ser, device, &info, &pDescriptorSets[i]);

          chunk = scope.Get();
        }
        record->AddChunk(chunk);

        record->FreeParents(GetResourceManager());
        record->AddParent(poolrecord);
        record->AddParent(layoutRecord);

        record->descInfo->layout = layoutRecord->descInfo->layout;
        record->descInfo->layout->CreateBindingsArray(record->descInfo->data,
                                                      variableDescriptorAlloc);
      }
      else
      {
        record->descInfo->data.reset();
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, pDescriptorSets[i]);

      m_DescriptorSetState[id].layout = GetResID(pAllocateInfo->pSetLayouts[i]);
    }
  }

  return ret;
}

VkResult WrappedVulkan::vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
                                             uint32_t count, const VkDescriptorSet *pDescriptorSets)
{
  VkDescriptorSet *unwrapped = GetTempArray<VkDescriptorSet>(count);
  for(uint32_t i = 0; i < count; i++)
    unwrapped[i] = Unwrap(pDescriptorSets[i]);

  for(uint32_t i = 0; i < count; i++)
  {
    if(pDescriptorSets[i] != VK_NULL_HANDLE)
      GetResourceManager()->ReleaseWrappedResource(pDescriptorSets[i]);
  }

  VkResult ret =
      ObjDisp(device)->FreeDescriptorSets(Unwrap(device), Unwrap(descriptorPool), count, unwrapped);

  return ret;
}

VkResult WrappedVulkan::vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                              VkDescriptorPoolResetFlags flags)
{
  // need to free all child descriptor pools. Application is responsible for
  // ensuring no concurrent use with alloc/free from this pool, the same as
  // for DestroyDescriptorPool.
  {
    // don't reset while capture transition lock is held, so that we can't reset and potentially
    // reuse a record we might be preparing. We do this here rather than in vkAllocateDescriptorSets
    // where we actually modify the record, since that's much higher frequency
    SCOPED_READLOCK(m_CapTransitionLock);

    if(IsCaptureMode(m_State))
    {
      VkResourceRecord *record = GetRecord(descriptorPool);

      if(Vulkan_Debug_AllowDescriptorSetReuse())
      {
        for(auto it = record->pooledChildren.begin(); it != record->pooledChildren.end(); ++it)
        {
          ((WrappedVkNonDispRes *)(*it)->Resource)->real = RealVkRes(0x123456);
          (*it)->descInfo->data.reset();
        }

        record->descPoolInfo->freelist.assign(record->pooledChildren);

        // sort by layout
        std::sort(record->descPoolInfo->freelist.begin(), record->descPoolInfo->freelist.end(),
                  [](VkResourceRecord *a, VkResourceRecord *b) {
                    return a->descInfo->layout < b->descInfo->layout;
                  });
      }
      else
      {
        // if descriptor set re-use is banned, we can simply free all the sets immediately without
        // adding them to the free list and that will effectively disallow re-use.
        for(auto it = record->pooledChildren.begin(); it != record->pooledChildren.end(); ++it)
        {
          // unset record->pool so we don't recurse
          (*it)->pool = NULL;
          GetResourceManager()->ReleaseWrappedResource((VkDescriptorSet)(uint64_t)(*it)->Resource,
                                                       true);
        }

        record->pooledChildren.clear();
      }
    }
  }

  return ObjDisp(device)->ResetDescriptorPool(Unwrap(device), Unwrap(descriptorPool), flags);
}

void WrappedVulkan::ReplayDescriptorSetWrite(VkDevice device, const VkWriteDescriptorSet &writeDesc)
{
  // check for validity - if a resource wasn't referenced other than in this update
  // (ie. the descriptor set was overwritten or never bound), then the write descriptor
  // will be invalid with some missing handles. It's safe though to just skip this
  // update as we only get here if it's never used.

  // if a set was never bound, it will have been omitted and we just drop any writes to it
  bool valid = (writeDesc.dstSet != VK_NULL_HANDLE);

  if(!valid)
    return;

  // ignore empty writes, for some reason this is valid with descriptor update templates.
  if(writeDesc.descriptorCount == 0)
    return;

  const DescSetLayout &layout =
      m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[GetResID(writeDesc.dstSet)].layout];

  const DescSetLayout::Binding *layoutBinding = &layout.bindings[writeDesc.dstBinding];
  uint32_t curIdx = writeDesc.dstArrayElement;

  switch(writeDesc.descriptorType)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    {
      for(uint32_t i = 0; i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pImageInfo[i].sampler != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    {
      for(uint32_t i = 0; i < writeDesc.descriptorCount; i++, curIdx++)
      {
        // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
        // explanation
        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          curIdx = 0;

          // skip past invalid padding descriptors to get to the next real one
          while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          {
            layoutBinding++;
          }
        }

        valid &= (writeDesc.pImageInfo[i].sampler != VK_NULL_HANDLE) ||
                 (layoutBinding->immutableSampler &&
                  layoutBinding->immutableSampler[curIdx] != ResourceId());

        if(!NULLDescriptorsAllowed())
          valid &= (writeDesc.pImageInfo[i].imageView != VK_NULL_HANDLE);
      }
      break;
    }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    {
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pImageInfo[i].imageView != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    {
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pTexelBufferView[i] != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    {
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pBufferInfo[i].buffer != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    {
      const VkWriteDescriptorSetAccelerationStructureKHR *asDesc =
          (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
              &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
      {
        valid &= (asDesc->pAccelerationStructures[i] != VK_NULL_HANDLE);
      }
      break;
    }
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK: break;
    default: RDCERR("Unexpected descriptor type %d", writeDesc.descriptorType);
  }

  if(valid)
  {
    VkWriteDescriptorSet unwrapped = UnwrapInfo(&writeDesc);
    ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &unwrapped, 0, NULL);

    // update our local tracking
    rdcarray<DescriptorSetSlot *> &bindings =
        m_DescriptorSetState[GetResID(writeDesc.dstSet)].data.binds;
    bytebuf &inlineData = m_DescriptorSetState[GetResID(writeDesc.dstSet)].data.inlineBytes;

    {
      RDCASSERT(writeDesc.dstBinding < bindings.size());

      DescriptorSetSlot **bind = &bindings[writeDesc.dstBinding];
      layoutBinding = &layout.bindings[writeDesc.dstBinding];
      curIdx = writeDesc.dstArrayElement;

      if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
        {
          // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
          // explanation
          if(curIdx >= layoutBinding->descriptorCount)
          {
            layoutBinding++;
            bind++;
            curIdx = 0;

            // skip past invalid padding descriptors to get to the next real one
            while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
              layoutBinding++;
              bind++;
            }
          }

          (*bind)[curIdx].SetTexelBuffer(writeDesc.descriptorType,
                                         GetResID(writeDesc.pTexelBufferView[d]));
        }
      }
      else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
        {
          // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
          // explanation
          if(curIdx >= layoutBinding->descriptorCount)
          {
            layoutBinding++;
            bind++;
            curIdx = 0;

            // skip past invalid padding descriptors to get to the next real one
            while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
              layoutBinding++;
              bind++;
            }
          }

          (*bind)[curIdx].SetImage(writeDesc.descriptorType, writeDesc.pImageInfo[d],
                                   layoutBinding->immutableSampler == NULL);
        }
      }
      else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        const VkWriteDescriptorSetInlineUniformBlock *inlineWrite =
            (const VkWriteDescriptorSetInlineUniformBlock *)FindNextStruct(
                &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);
        memcpy(inlineData.data() + (*bind)->offset + writeDesc.dstArrayElement, inlineWrite->pData,
               inlineWrite->dataSize);
      }
      else
      {
        const VkWriteDescriptorSetAccelerationStructureKHR *asDesc = NULL;

        for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
        {
          // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
          // explanation
          if(curIdx >= layoutBinding->descriptorCount)
          {
            layoutBinding++;
            bind++;
            curIdx = 0;

            // skip past invalid padding descriptors to get to the next real one
            while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
              layoutBinding++;
              bind++;
            }
          }

          if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
          {
            if(!asDesc)
              asDesc = (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                  &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);

            (*bind)[curIdx].SetAccelerationStructure(writeDesc.descriptorType,
                                                     asDesc->pAccelerationStructures[d]);
          }
          else
          {
            (*bind)[curIdx].SetBuffer(writeDesc.descriptorType, writeDesc.pBufferInfo[d]);
          }
        }
      }
    }
  }
}

void WrappedVulkan::ReplayDescriptorSetCopy(VkDevice device, const VkCopyDescriptorSet &copyDesc)
{
  // if a set was never bound, it will have been omitted and we just drop any copies to it
  if(copyDesc.dstSet == VK_NULL_HANDLE || copyDesc.srcSet == VK_NULL_HANDLE)
    return;

  VkCopyDescriptorSet unwrapped = UnwrapInfo(&copyDesc);
  ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 0, NULL, 1, &unwrapped);

  ResourceId dstSetId = GetResID(copyDesc.dstSet);
  ResourceId srcSetId = GetResID(copyDesc.srcSet);

  // update our local tracking
  rdcarray<DescriptorSetSlot *> &dstbindings = m_DescriptorSetState[dstSetId].data.binds;
  rdcarray<DescriptorSetSlot *> &srcbindings = m_DescriptorSetState[srcSetId].data.binds;

  {
    RDCASSERT(copyDesc.dstBinding < dstbindings.size());
    RDCASSERT(copyDesc.srcBinding < srcbindings.size());

    const DescSetLayout &dstlayout =
        m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[dstSetId].layout];
    const DescSetLayout &srclayout =
        m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[srcSetId].layout];

    const DescSetLayout::Binding *layoutSrcBinding = &srclayout.bindings[copyDesc.srcBinding];
    const DescSetLayout::Binding *layoutDstBinding = &dstlayout.bindings[copyDesc.dstBinding];

    DescriptorSetSlot **dstbind = &dstbindings[copyDesc.dstBinding];
    DescriptorSetSlot **srcbind = &srcbindings[copyDesc.srcBinding];

    uint32_t curDstIdx = copyDesc.dstArrayElement;
    uint32_t curSrcIdx = copyDesc.srcArrayElement;

    for(uint32_t d = 0; d < copyDesc.descriptorCount; d++, curSrcIdx++, curDstIdx++)
    {
      if(layoutSrcBinding->layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // inline uniform blocks are special, the descriptor count is a byte count. The layouts may
        // not match so inline offsets might not match, so we just copy the data and break.

        bytebuf &dstInlineData = m_DescriptorSetState[dstSetId].data.inlineBytes;
        bytebuf &srcInlineData = m_DescriptorSetState[srcSetId].data.inlineBytes;

        memcpy(dstInlineData.data() + (*dstbind)[0].offset + copyDesc.dstArrayElement,
               srcInlineData.data() + (*srcbind)[0].offset + copyDesc.srcArrayElement,
               copyDesc.descriptorCount);

        break;
      }

      // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
      // explanation
      if(curSrcIdx >= layoutSrcBinding->descriptorCount)
      {
        layoutSrcBinding++;
        srcbind++;
        curSrcIdx = 0;
      }

      // src and dst could wrap independently - think copying from
      // { sampler2D, sampler2D[4], sampler2D } to a { sampler2D[3], sampler2D[3] }
      // or copying from different starting array elements
      if(curDstIdx >= layoutDstBinding->descriptorCount)
      {
        layoutDstBinding++;
        dstbind++;
        curDstIdx = 0;
      }

      (*dstbind)[curDstIdx] = (*srcbind)[curSrcIdx];
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkUpdateDescriptorSets(SerialiserType &ser, VkDevice device,
                                                     uint32_t writeCount,
                                                     const VkWriteDescriptorSet *pDescriptorWrites,
                                                     uint32_t copyCount,
                                                     const VkCopyDescriptorSet *pDescriptorCopies)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(writeCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorWrites, writeCount);
  if(writeCount > 0)
    ser.Important();
  SERIALISE_ELEMENT(copyCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorCopies, copyCount);
  if(copyCount > 0)
    ser.Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(uint32_t i = 0; i < writeCount; i++)
      ReplayDescriptorSetWrite(device, pDescriptorWrites[i]);

    for(uint32_t i = 0; i < copyCount; i++)
      ReplayDescriptorSetCopy(device, pDescriptorCopies[i]);
  }

  return true;
}

void WrappedVulkan::vkUpdateDescriptorSets(VkDevice device, uint32_t writeCount,
                                           const VkWriteDescriptorSet *pDescriptorWrites,
                                           uint32_t copyCount,
                                           const VkCopyDescriptorSet *pDescriptorCopies)
{
  SCOPED_DBG_SINK();

  // we don't implement this into an UnwrapInfo because it's awkward to have this unique case of
  // two parallel struct arrays, and also we don't need to unwrap it on replay in the same way
  {
    // need to count up number of descriptor infos and acceleration structures, to be able to alloc
    // enough space
    uint32_t numInfos = 0;
    uint32_t numASDescriptors = 0;
    uint32_t numASs = 0;
    for(uint32_t i = 0; i < writeCount; i++)
    {
      if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        ++numASDescriptors;
        numASs += pDescriptorWrites[i].descriptorCount;
      }
      else
      {
        numInfos += pDescriptorWrites[i].descriptorCount;
      }
    }

    byte *memory = GetTempMemory(
        sizeof(VkDescriptorBufferInfo) * numInfos + sizeof(VkWriteDescriptorSet) * writeCount +
        sizeof(VkCopyDescriptorSet) * copyCount +
        sizeof(VkWriteDescriptorSetAccelerationStructureKHR) * numASDescriptors +
        sizeof(VkAccelerationStructureKHR) * numASs);

    RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                      "Descriptor structs sizes are unexpected, ensure largest size is used");

    VkWriteDescriptorSet *unwrappedWrites = (VkWriteDescriptorSet *)memory;
    VkCopyDescriptorSet *unwrappedCopies = (VkCopyDescriptorSet *)(unwrappedWrites + writeCount);
    VkDescriptorBufferInfo *nextDescriptors = (VkDescriptorBufferInfo *)(unwrappedCopies + copyCount);
    VkWriteDescriptorSetAccelerationStructureKHR *nextASDescriptors =
        (VkWriteDescriptorSetAccelerationStructureKHR *)(nextDescriptors + numInfos);
    VkAccelerationStructureKHR *unwrappedASs =
        (VkAccelerationStructureKHR *)(nextASDescriptors + numASDescriptors);

    for(uint32_t i = 0; i < writeCount; i++)
    {
      unwrappedWrites[i] = pDescriptorWrites[i];

      bool hasImmutable = false;

      if(IsCaptureMode(m_State))
      {
        VkResourceRecord *record = GetRecord(unwrappedWrites[i].dstSet);
        RDCASSERT(record->descInfo && record->descInfo->layout);
        const DescSetLayout &layout = *record->descInfo->layout;

        RDCASSERT(unwrappedWrites[i].dstBinding < record->descInfo->data.binds.size());
        const DescSetLayout::Binding *layoutBinding = &layout.bindings[unwrappedWrites[i].dstBinding];

        hasImmutable = layoutBinding->immutableSampler != NULL;
      }
      else
      {
        const DescSetLayout &layout =
            m_CreationInfo
                .m_DescSetLayout[m_DescriptorSetState[GetResID(unwrappedWrites[i].dstSet)].layout];

        const DescSetLayout::Binding *layoutBinding = &layout.bindings[unwrappedWrites[i].dstBinding];

        hasImmutable = layoutBinding->immutableSampler != NULL;
      }

      unwrappedWrites[i].dstSet = Unwrap(unwrappedWrites[i].dstSet);

      VkDescriptorBufferInfo *bufInfos = nextDescriptors;
      VkDescriptorImageInfo *imInfos = (VkDescriptorImageInfo *)bufInfos;
      VkBufferView *bufViews = (VkBufferView *)bufInfos;

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Structure sizes mean not enough space is allocated for write data");
      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkBufferView),
                        "Structure sizes mean not enough space is allocated for write data");

      // unwrap and assign the appropriate array
      if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        unwrappedWrites[i].pTexelBufferView = (VkBufferView *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
          bufViews[j] = Unwrap(pDescriptorWrites[i].pTexelBufferView[j]);
      }
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        bool hasSampler =
            (pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
            !hasImmutable;
        bool hasImage =
            (pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

        unwrappedWrites[i].pImageInfo = (VkDescriptorImageInfo *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          if(hasImage)
            imInfos[j].imageView = Unwrap(pDescriptorWrites[i].pImageInfo[j].imageView);
          if(hasSampler)
            imInfos[j].sampler = Unwrap(pDescriptorWrites[i].pImageInfo[j].sampler);
          imInfos[j].imageLayout = pDescriptorWrites[i].pImageInfo[j].imageLayout;
        }
      }
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // nothing to unwrap, the next chain contains the data which we can leave as-is
      }
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        const VkWriteDescriptorSetAccelerationStructureKHR *asRead =
            (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                &pDescriptorWrites[i],
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
        VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
            (VkWriteDescriptorSetAccelerationStructureKHR *)memcpy(
                nextASDescriptors, asRead, sizeof(VkWriteDescriptorSetAccelerationStructureKHR));

        VkAccelerationStructureKHR *base = unwrappedASs;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          base[j] = Unwrap(asWrite->pAccelerationStructures[j]);
        }
        asWrite->pAccelerationStructures = base;

        unwrappedWrites[i].pNext = asWrite;

        ++nextASDescriptors;
        unwrappedASs += pDescriptorWrites[i].descriptorCount;
      }
      else
      {
        unwrappedWrites[i].pBufferInfo = bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          bufInfos[j].buffer = Unwrap(pDescriptorWrites[i].pBufferInfo[j].buffer);
          bufInfos[j].offset = pDescriptorWrites[i].pBufferInfo[j].offset;
          bufInfos[j].range = pDescriptorWrites[i].pBufferInfo[j].range;
          if(bufInfos[j].buffer == VK_NULL_HANDLE)
          {
            bufInfos[j].offset = 0;
            bufInfos[j].range = VK_WHOLE_SIZE;
          }
        }
      }

      // Increment nextDescriptors (a.k.a. bufInfos)
      if(pDescriptorWrites[i].descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        nextDescriptors += pDescriptorWrites[i].descriptorCount;
      }
    }

    for(uint32_t i = 0; i < copyCount; i++)
    {
      unwrappedCopies[i] = pDescriptorCopies[i];
      unwrappedCopies[i].dstSet = Unwrap(unwrappedCopies[i].dstSet);
      unwrappedCopies[i].srcSet = Unwrap(unwrappedCopies[i].srcSet);
    }

    SERIALISE_TIME_CALL(ObjDisp(device)->UpdateDescriptorSets(
        Unwrap(device), writeCount, unwrappedWrites, copyCount, unwrappedCopies));
  }

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    if(IsActiveCapturing(m_State))
    {
      // don't have to mark referenced any of the resources pointed to by the descriptor set -
      // that's
      // handled on queue submission by marking ref'd all the current bindings of the sets
      // referenced
      // by the cmd buffer

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkUpdateDescriptorSets);
        Serialise_vkUpdateDescriptorSets(ser, device, writeCount, pDescriptorWrites, copyCount,
                                         pDescriptorCopies);

        m_FrameCaptureRecord->AddChunk(scope.Get());
      }

      // previously we would not mark descriptor set destinations as ref'd here. This is because all
      // descriptor sets are implicitly dirty and they're only actually *needed* when bound - we can
      // safely skip any updates of unused descriptor sets. However for consistency with template
      // updates below, we pull them in here even if they won't technically be needed.

      for(uint32_t i = 0; i < writeCount; i++)
      {
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].dstSet),
                                                          eFrameRef_PartialWrite);

        if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
          const VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
              (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                  &pDescriptorWrites[i],
                  VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
          for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
          {
            const ResourceId id = GetResID(asWrite->pAccelerationStructures[j]);
            GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);
          }
        }
      }

      for(uint32_t i = 0; i < copyCount; i++)
      {
        // At the same time as ref'ing the source set, add it to a special list of descriptor sets
        // to pull in at the next queue submit. This is because it must be referenced even if the
        // source set is never bound to a command buffer, so that the source set's data is valid.
        //
        // This does mean a slightly conservative ref'ing if the dest set doesn't end up getting
        // bound, but we only do this during frame capture so it's not too bad.

        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].dstSet),
                                                          eFrameRef_PartialWrite);
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].srcSet),
                                                          eFrameRef_Read);

        ResourceId id = GetResID(pDescriptorCopies[i].srcSet);
        VkResourceRecord *record = GetRecord(pDescriptorCopies[i].srcSet);

        {
          SCOPED_LOCK(m_CapDescriptorsLock);
          record->AddRef();
          m_CapDescriptors.insert({id, record});
        }
      }
    }
  }

  // need to track descriptor set contents whether capframing or idle
  if(IsCaptureMode(m_State))
  {
    for(uint32_t i = 0; i < writeCount; i++)
    {
      const VkWriteDescriptorSet &descWrite = pDescriptorWrites[i];

      VkResourceRecord *record = GetRecord(descWrite.dstSet);
      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      RDCASSERT(descWrite.dstBinding < record->descInfo->data.binds.size());

      DescriptorSetSlot **binding = &record->descInfo->data.binds[descWrite.dstBinding];
      bytebuf &inlineData = record->descInfo->data.inlineBytes;

      const DescSetLayout::Binding *layoutBinding = &layout.bindings[descWrite.dstBinding];

      // We need to handle the cases where these bindings are stale:
      // ie. image handle 0xf00baa is allocated
      // bound into a descriptor set
      // image is released
      // descriptor set is bound but this image is never used by shader etc.
      //
      // worst case, a new image or something has been added with this handle -
      // in this case we end up ref'ing an image that isn't actually used.
      // Worst worst case, we ref an image as write when actually it's not, but
      // this is likewise not a serious problem, and rather difficult to solve
      // (would need to version handles somehow, but don't have enough bits
      // to do that reliably).
      //
      // This is handled by RemoveBindFrameRef silently dropping id == ResourceId()

      // start at the dstArrayElement
      uint32_t curIdx = descWrite.dstArrayElement;

      for(uint32_t d = 0; d < descWrite.descriptorCount; d++, curIdx++)
      {
        // roll over onto the next binding, on the assumption that it is the same
        // type and there is indeed a next binding at all. See spec language:
        //
        // If the dstBinding has fewer than descriptorCount array elements remaining starting from
        // dstArrayElement, then the remainder will be used to update the subsequent binding -
        // dstBinding+1 starting at array element zero. This behavior applies recursively, with the
        // update affecting consecutive bindings as needed to update all descriptorCount
        // descriptors. All consecutive bindings updated via a single VkWriteDescriptorSet structure
        // must have identical descriptorType and stageFlags, and must all either use immutable
        // samplers or must all not use immutable samplers.
        //
        // Note we don't have to worry about this interacting with variable descriptor counts
        // because the variable descriptor must be the last one, so there's no more overlap.

        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          binding++;
          curIdx = 0;

          // skip past invalid padding descriptors to get to the next real one
          while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          {
            layoutBinding++;
            binding++;
          }
        }

        DescriptorSetSlot &bind = (*binding)[curIdx];

        if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           descWrite.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind.SetTexelBuffer(descWrite.descriptorType, GetResID(descWrite.pTexelBufferView[d]));
        }
        else if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          bind.SetImage(descWrite.descriptorType, descWrite.pImageInfo[d],
                        layoutBinding->immutableSampler == NULL);
        }
        else if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          const VkWriteDescriptorSetInlineUniformBlock *inlineWrite =
              (const VkWriteDescriptorSetInlineUniformBlock *)FindNextStruct(
                  &descWrite, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);
          memcpy(inlineData.data() + (*binding)->offset + descWrite.dstArrayElement,
                 inlineWrite->pData, inlineWrite->dataSize);

          // break now because the descriptorCount is not the number of descriptors
          break;
        }
        else if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
          const VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
              (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                  &descWrite, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
          bind.SetAccelerationStructure(descWrite.descriptorType,
                                        asWrite->pAccelerationStructures[d]);
        }
        else
        {
          bind.SetBuffer(descWrite.descriptorType, descWrite.pBufferInfo[d]);
        }
      }
    }

    // this is almost identical to the above loop, except that instead of sourcing the descriptors
    // from the writedescriptor struct, we source it from our stored bindings on the source
    // descrpitor set

    for(uint32_t i = 0; i < copyCount; i++)
    {
      VkResourceRecord *dstrecord = GetRecord(pDescriptorCopies[i].dstSet);
      RDCASSERT(dstrecord->descInfo && dstrecord->descInfo->layout);
      const DescSetLayout &dstlayout = *dstrecord->descInfo->layout;

      VkResourceRecord *srcrecord = GetRecord(pDescriptorCopies[i].srcSet);
      RDCASSERT(srcrecord->descInfo && srcrecord->descInfo->layout);
      const DescSetLayout &srclayout = *srcrecord->descInfo->layout;

      RDCASSERT(pDescriptorCopies[i].dstBinding < dstrecord->descInfo->data.binds.size());
      RDCASSERT(pDescriptorCopies[i].srcBinding < srcrecord->descInfo->data.binds.size());

      DescriptorSetSlot **dstbinding =
          &dstrecord->descInfo->data.binds[pDescriptorCopies[i].dstBinding];
      DescriptorSetSlot **srcbinding =
          &srcrecord->descInfo->data.binds[pDescriptorCopies[i].srcBinding];

      const DescSetLayout::Binding *dstlayoutBinding =
          &dstlayout.bindings[pDescriptorCopies[i].dstBinding];
      const DescSetLayout::Binding *srclayoutBinding =
          &srclayout.bindings[pDescriptorCopies[i].srcBinding];

      // allow roll-over between consecutive bindings. See above in the plain write case for more
      // explanation
      uint32_t curSrcIdx = pDescriptorCopies[i].srcArrayElement;
      uint32_t curDstIdx = pDescriptorCopies[i].dstArrayElement;

      for(uint32_t d = 0; d < pDescriptorCopies[i].descriptorCount; d++, curSrcIdx++, curDstIdx++)
      {
        if(srclayoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          // inline uniform blocks are special, the descriptor count is a byte count. The layouts
          // may not match so inline offsets might not match, so we just copy the data and break.

          bytebuf &dstInlineData = dstrecord->descInfo->data.inlineBytes;
          bytebuf &srcInlineData = srcrecord->descInfo->data.inlineBytes;

          memcpy(
              dstInlineData.data() + (*dstbinding)[0].offset + pDescriptorCopies[i].dstArrayElement,
              srcInlineData.data() + (*srcbinding)[0].offset + pDescriptorCopies[i].srcArrayElement,
              pDescriptorCopies[i].descriptorCount);

          break;
        }

        if(curDstIdx >= dstlayoutBinding->descriptorCount)
        {
          dstlayoutBinding++;
          dstbinding++;
          curDstIdx = 0;
        }

        // dst and src indices must roll-over independently
        if(curSrcIdx >= srclayoutBinding->descriptorCount)
        {
          srclayoutBinding++;
          srcbinding++;
          curSrcIdx = 0;
        }

        DescriptorSetSlot &bind = (*dstbinding)[curDstIdx];

        bind = (*srcbinding)[curSrcIdx];
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDescriptorUpdateTemplate(
    SerialiserType &ser, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(DescriptorUpdateTemplate, GetResID(*pDescriptorUpdateTemplate))
      .TypedAs("VkDescriptorUpdateTemplate"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorUpdateTemplate templ = VK_NULL_HANDLE;

    VkDescriptorUpdateTemplateCreateInfo unwrapped = UnwrapInfo(&CreateInfo);
    VkResult ret =
        ObjDisp(device)->CreateDescriptorUpdateTemplate(Unwrap(device), &unwrapped, NULL, &templ);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor update template, VkResult: %s",
                       ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), templ);
      GetResourceManager()->AddLiveResource(DescriptorUpdateTemplate, templ);

      m_CreationInfo.m_DescUpdateTemplate[live].Init(GetResourceManager(), m_CreationInfo,
                                                     &CreateInfo);
    }

    AddResource(DescriptorUpdateTemplate, ResourceType::StateObject, "Descriptor Update Template");
    DerivedResource(device, DescriptorUpdateTemplate);
    if(CreateInfo.pipelineLayout != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.pipelineLayout, DescriptorUpdateTemplate);
    if(CreateInfo.descriptorSetLayout != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.descriptorSetLayout, DescriptorUpdateTemplate);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
  VkDescriptorUpdateTemplateCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorUpdateTemplate(
                          Unwrap(device), &unwrapped, NULL, pDescriptorUpdateTemplate));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorUpdateTemplate);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDescriptorUpdateTemplate);
        Serialise_vkCreateDescriptorUpdateTemplate(ser, device, pCreateInfo, NULL,
                                                   pDescriptorUpdateTemplate);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDescriptorUpdateTemplate);
      record->AddChunk(chunk);

      if(unwrapped.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS)
        record->AddParent(GetRecord(pCreateInfo->pipelineLayout));
      else if(unwrapped.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
        record->AddParent(GetRecord(pCreateInfo->descriptorSetLayout));

      record->descTemplateInfo = new DescUpdateTemplate();
      record->descTemplateInfo->Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pDescriptorUpdateTemplate);

      m_CreationInfo.m_DescUpdateTemplate[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkUpdateDescriptorSetWithTemplate(
    SerialiserType &ser, VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(descriptorSet).Important();
  SERIALISE_ELEMENT(descriptorUpdateTemplate).Important();

  // we can't serialise pData as-is, since we need to decode to ResourceId for references, etc. The
  // sensible way to do this is to decode the data into a series of writes and serialise that.
  DescUpdateTemplateApplication apply;

  if(IsCaptureMode(m_State))
  {
    // decode while capturing.
    GetRecord(descriptorUpdateTemplate)->descTemplateInfo->Apply(pData, apply);
  }

  SERIALISE_ELEMENT(apply.writes).Named("Decoded Writes"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(VkWriteDescriptorSet &writeDesc : apply.writes)
    {
      writeDesc.dstSet = descriptorSet;
      ReplayDescriptorSetWrite(device, writeDesc);
    }
  }

  return true;
}

// see vkUpdateDescriptorSets for more verbose comments, the concepts are the same here except we
// apply from a template & user memory instead of arrays of VkWriteDescriptorSet/VkCopyDescriptorSet
void WrappedVulkan::vkUpdateDescriptorSetWithTemplate(
    VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
  SCOPED_DBG_SINK();

  DescUpdateTemplate *tempInfo = GetRecord(descriptorUpdateTemplate)->descTemplateInfo;

  {
    // allocate the whole blob of memory
    byte *memory = GetTempMemory(tempInfo->unwrapByteSize);

    // iterate the entries, copy the descriptor data and unwrap
    for(const VkDescriptorUpdateTemplateEntry &entry : tempInfo->updates)
    {
      byte *dst = memory + entry.offset;
      const byte *src = (const byte *)pData + entry.offset;

      if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkBufferView));

          VkBufferView *bufView = (VkBufferView *)dst;

          *bufView = Unwrap(*bufView);

          dst += entry.stride;
          src += entry.stride;
        }
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        bool hasSampler = (entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        bool hasImage = (entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorImageInfo));

          VkDescriptorImageInfo *info = (VkDescriptorImageInfo *)dst;

          if(hasSampler)
            info->sampler = Unwrap(info->sampler);
          if(hasImage)
            info->imageView = Unwrap(info->imageView);

          dst += entry.stride;
          src += entry.stride;
        }
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // memcpy the data
        memcpy(dst, src, entry.descriptorCount);
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkAccelerationStructureKHR));

          VkAccelerationStructureKHR *as = (VkAccelerationStructureKHR *)dst;

          *as = Unwrap(*as);

          dst += entry.stride;
          src += entry.stride;
        }
      }
      else
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorBufferInfo));

          VkDescriptorBufferInfo *info = (VkDescriptorBufferInfo *)dst;

          info->buffer = Unwrap(info->buffer);

          dst += entry.stride;
          src += entry.stride;
        }
      }
    }

    SERIALISE_TIME_CALL(ObjDisp(device)->UpdateDescriptorSetWithTemplate(
        Unwrap(device), Unwrap(descriptorSet), Unwrap(descriptorUpdateTemplate), memory));
  }

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    if(IsActiveCapturing(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkUpdateDescriptorSetWithTemplate);
      Serialise_vkUpdateDescriptorSetWithTemplate(ser, device, descriptorSet,
                                                  descriptorUpdateTemplate, pData);

      m_FrameCaptureRecord->AddChunk(scope.Get());

      // mark the destination set and template as referenced
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(descriptorSet),
                                                        eFrameRef_PartialWrite);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(descriptorUpdateTemplate),
                                                        eFrameRef_Read);
    }
  }

  // need to track descriptor set contents whether capframing or idle
  if(IsCaptureMode(m_State))
  {
    for(const VkDescriptorUpdateTemplateEntry &entry : tempInfo->updates)
    {
      VkResourceRecord *record = GetRecord(descriptorSet);

      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      RDCASSERT(entry.dstBinding < record->descInfo->data.binds.size());

      DescriptorSetSlot **binding = &record->descInfo->data.binds[entry.dstBinding];
      bytebuf &inlineData = record->descInfo->data.inlineBytes;

      const DescSetLayout::Binding *layoutBinding = &layout.bindings[entry.dstBinding];

      // start at the dstArrayElement
      uint32_t curIdx = entry.dstArrayElement;

      for(uint32_t d = 0; d < entry.descriptorCount; d++, curIdx++)
      {
        // roll over onto the next binding, on the assumption that it is the same
        // type and there is indeed a next binding at all. See spec language:
        //
        // If the dstBinding has fewer than descriptorCount array elements remaining starting from
        // dstArrayElement, then the remainder will be used to update the subsequent binding -
        // dstBinding+1 starting at array element zero. This behavior applies recursively, with the
        // update affecting consecutive bindings as needed to update all descriptorCount
        // descriptors. All consecutive bindings updated via a single VkWriteDescriptorSet structure
        // must have identical descriptorType and stageFlags, and must all either use immutable
        // samplers or must all not use immutable samplers.
        //
        // Note we don't have to worry about this interacting with variable descriptor counts
        // because the variable descriptor must be the last one, so there's no more overlap.

        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          binding++;
          curIdx = 0;

          // skip past invalid padding descriptors to get to the next real one
          while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          {
            layoutBinding++;
            binding++;
          }
        }

        const byte *src = (const byte *)pData + entry.offset + entry.stride * d;

        DescriptorSetSlot &bind = (*binding)[curIdx];

        if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind.SetTexelBuffer(entry.descriptorType, GetResID(*(const VkBufferView *)src));
        }
        else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          const VkDescriptorImageInfo &srcInfo = *(const VkDescriptorImageInfo *)src;

          bind.SetImage(entry.descriptorType, srcInfo, layoutBinding->immutableSampler == NULL);
        }
        else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          memcpy(inlineData.data() + bind.offset + entry.dstArrayElement, src, entry.descriptorCount);

          // break now because the descriptorCount is not the number of descriptors
          break;
        }
        else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
          bind.SetAccelerationStructure(entry.descriptorType,
                                        *(const VkAccelerationStructureKHR *)src);
        }
        else
        {
          bind.SetBuffer(entry.descriptorType, *(const VkDescriptorBufferInfo *)src);
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetDescriptorEXT(SerialiserType &ser, VkDevice device,
                                                 const VkDescriptorGetInfoEXT *pDescriptorInfo,
                                                 size_t dataSize_, void *pDescriptor)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(DescriptorInfo, *pDescriptorInfo).Important();
  SERIALISE_ELEMENT_LOCAL(dataSize, uint64_t(dataSize_));
  SERIALISE_ELEMENT_ARRAY(pDescriptor, dataSize_);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    byte *tempMem = GetTempMemory(GetNextPatchSize(&DescriptorInfo));
    VkDescriptorGetInfoEXT *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, &DescriptorInfo);

    uint64_t curDataSize = DescriptorDataSize(DescriptorInfo.type);
    if(dataSize != curDataSize)
    {
      SET_ERROR_RESULT(
          m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Descriptor of type %s changed size, it was %llu bytes during capture but is %llu bytes "
          "during replay.\n"
          "\n%s",
          ToStr(DescriptorInfo.type).c_str(), dataSize, curDataSize,
          GetPhysDeviceCompatString(false, false).c_str());
      return false;
    }

    bytebuf replayDescriptor;
    replayDescriptor.resize(size_t(dataSize));

    // verify the descriptor is bitwise identical
    ObjDisp(device)->GetDescriptorEXT(Unwrap(device), unwrappedInfo, (size_t)dataSize,
                                      replayDescriptor.data());

    if(memcmp(replayDescriptor.data(), pDescriptor, size_t(dataSize)) != 0)
    {
      rdcstr bitDifferences;

      uint32_t *capU32 = (uint32_t *)pDescriptor;
      uint32_t *replayU32 = (uint32_t *)replayDescriptor.data();

      bitDifferences = "Capture:\n";
      for(uint32_t d = 0; d * 4 < dataSize; d++)
        bitDifferences += StringFormat::Fmt("%08llx ", capU32[d]);
      bitDifferences += "\n\n";
      bitDifferences += "Replay:\n";
      for(uint32_t d = 0; d * 4 < dataSize; d++)
        bitDifferences += StringFormat::Fmt("%08llx ", replayU32[d]);
      bitDifferences += "\n";

      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                       "Descriptor of type %s changed bit pattern.\n"
                       "\n%s"
                       "\n%s",
                       ToStr(DescriptorInfo.type).c_str(), bitDifferences.c_str(),
                       GetPhysDeviceCompatString(false, false).c_str());
      return false;
    }

    DescriptorSetSlot descriptorData = {};
    descriptorData.SetDescriptor(this, DescriptorInfo);

    RegisterDescriptor(replayDescriptor, descriptorData);
  }

  return true;
}

void WrappedVulkan::vkGetDescriptorEXT(VkDevice device, const VkDescriptorGetInfoEXT *pDescriptorInfo,
                                       size_t dataSize, void *pDescriptor)
{
  // the user *could* be querying straight into GPU upload memory which would be very bad to read
  // back from. To avoid that, we read into our temporary memory then memcpy to user memory but only
  // if we're actually going to process this.

  // first determine where this should go, based on the 'primary' resource. For combined
  // image/samplers, we pre-populated the sampler one so we treat them as-if they're just images
  VkResourceRecord *dstRecord = NULL;

  if(IsCaptureMode(m_State))
  {
    switch(pDescriptorInfo->type)
    {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      {
        if(pDescriptorInfo->data.pSampler)
        {
          dstRecord = GetRecord(*pDescriptorInfo->data.pSampler);
          // don't need to worry about the race here, worst case we save an extra descriptor
          if(dstRecord->hasDescriptorSaved)
            dstRecord = NULL;
          else
            dstRecord->hasDescriptorSaved = true;
        }
        else
        {
          dstRecord = GetRecord(m_Device);
          if(m_NULLDescriptorPatternSaved)
            dstRecord = NULL;
        }
        break;
      }
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      {
        // sampled/storage/input attachment are identical in the union. Since the type forms part of
        // our this logic can be done in common
        if(pDescriptorInfo->data.pSampledImage &&
           pDescriptorInfo->data.pSampledImage->imageView != VK_NULL_HANDLE)
        {
          dstRecord = GetRecord(pDescriptorInfo->data.pSampledImage->imageView);

          DescriptorUniquenessKey descKey(
              m_IgnoreLayoutForDescriptors ? VK_IMAGE_LAYOUT_UNDEFINED
                                           : pDescriptorInfo->data.pCombinedImageSampler->imageLayout,
              pDescriptorInfo->type);

          // this is internally locked
          if(!dstRecord->resInfo->AddDescriptor(descKey))
            dstRecord = NULL;
        }
        else if(pDescriptorInfo->data.pCombinedImageSampler &&
                pDescriptorInfo->data.pCombinedImageSampler->imageView == VK_NULL_HANDLE &&
                pDescriptorInfo->data.pCombinedImageSampler->sampler != VK_NULL_HANDLE)
        {
          dstRecord = GetRecord(pDescriptorInfo->data.pCombinedImageSampler->sampler);
          // don't need to worry about the race here, worst case we save an extra descriptor
          if(dstRecord->hasNULLDescriptorSaved)
            dstRecord = NULL;
          else
            dstRecord->hasNULLDescriptorSaved = true;
        }
        else
        {
          dstRecord = GetRecord(m_Device);
          if(m_NULLDescriptorPatternSaved)
            dstRecord = NULL;
        }
        break;
      }
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      {
        // uniform/storage are identical in the union. Since the type forms part of our this
        // logic can be done in common
        if(pDescriptorInfo->data.pUniformBuffer && pDescriptorInfo->data.pUniformBuffer->address)
        {
          VkFormat fmt = VK_FORMAT_UNDEFINED;
          if(pDescriptorInfo->type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
             pDescriptorInfo->type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
          {
            fmt = pDescriptorInfo->data.pUniformTexelBuffer->format;
          }

          if(Vulkan_Debug_DangerousDescriptorSerialisation() == 122333)
          {
            if(pDescriptorInfo->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
               pDescriptorInfo->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            {
              dstRecord = NULL;
              break;
            }
            // serialise any one random descriptor per unique format so the tracking on replay still works
            if(pDescriptorInfo->type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
               pDescriptorInfo->type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
            {
              SCOPED_LOCK(m_DescriptorLookup.lock);
              if(m_DescriptorLookup.texelFormats.contains(fmt))
              {
                dstRecord = NULL;
                break;
              }
              m_DescriptorLookup.texelFormats.push_back(fmt);
            }
          }

          ResourceId id;
          uint64_t offs = 0;
          GetResIDFromAddr(pDescriptorInfo->data.pUniformBuffer->address, id, offs);

          dstRecord = GetResourceManager()->GetResourceRecord(id);

          DescriptorUniquenessKey descKey(offs, pDescriptorInfo->data.pUniformBuffer->range, fmt,
                                          pDescriptorInfo->type);

          // this is internally locked
          if(!dstRecord->resInfo->AddDescriptor(descKey))
            dstRecord = NULL;
        }
        else
        {
          dstRecord = GetRecord(m_Device);
          if(m_NULLDescriptorPatternSaved)
            dstRecord = NULL;
        }
        break;
      }
      case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
      {
        if(pDescriptorInfo->data.accelerationStructure)
        {
          ResourceId id;
          {
            SCOPED_LOCK(m_ASLookupByAddrLock);
            id = m_ASLookupByAddr[pDescriptorInfo->data.accelerationStructure];
          }

          dstRecord = GetResourceManager()->GetResourceRecord(id);
          // don't need to worry about the race here, worst case we save an extra descriptor
          if(dstRecord->hasDescriptorSaved)
            dstRecord = NULL;
          else
            dstRecord->hasDescriptorSaved = true;
        }
        else
        {
          dstRecord = GetRecord(m_Device);
          if(m_NULLDescriptorPatternSaved)
            dstRecord = NULL;
        }
        break;
      }
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
      case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
      case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
      case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
      case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
      case VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV:
      case VK_DESCRIPTOR_TYPE_TENSOR_ARM:
      case VK_DESCRIPTOR_TYPE_MAX_ENUM:
        RDCERR("Invalid descriptor type passed to vkGetDescriptorEXT");
        break;
    }
  }

  size_t localSize = 0;
  if(dstRecord)
    localSize = dataSize;

  byte tempMem[256] = {};
  VkDescriptorGetInfoEXT unwrappedInfo = UnwrapInfo(pDescriptorInfo);

  SERIALISE_TIME_CALL(ObjDisp(device)->GetDescriptorEXT(Unwrap(device), &unwrappedInfo, dataSize,
                                                        dstRecord ? tempMem : pDescriptor));

  // if we needed to serialise this descriptor
  if(dstRecord)
  {
    // copy to the user's memory
    memcpy(pDescriptor, tempMem, dataSize);

    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkGetDescriptorEXT);
      Serialise_vkGetDescriptorEXT(ser, device, pDescriptorInfo, dataSize, tempMem);

      chunk = scope.Get();
    }

    dstRecord->AddChunk(chunk);
  }
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorSetLayout, VkDevice device,
                                const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkDescriptorSetLayout *pSetLayout);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorPool, VkDevice device,
                                const VkDescriptorPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkDescriptorPool *pDescriptorPool);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkAllocateDescriptorSets, VkDevice device,
                                const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                VkDescriptorSet *pDescriptorSets);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUpdateDescriptorSets, VkDevice device,
                                uint32_t descriptorWriteCount,
                                const VkWriteDescriptorSet *pDescriptorWrites,
                                uint32_t descriptorCopyCount,
                                const VkCopyDescriptorSet *pDescriptorCopies);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorUpdateTemplate, VkDevice device,
                                const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *,
                                VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUpdateDescriptorSetWithTemplate, VkDevice device,
                                VkDescriptorSet descriptorSet,
                                VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                const void *pData);

INSTANTIATE_FUNCTION_SERIALISED(void, vkGetDescriptorEXT, VkDevice device,
                                const VkDescriptorGetInfoEXT *pDescriptorInfo, size_t dataSize,
                                void *pDescriptor);
