/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
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

#include "3rdparty/fmt/core.h"
#include "vk_test.h"

RD_TEST(VK_Descriptor_Buffer_Analyse, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Analyses the descriptors in VK_EXT_descriptor_buffer to test that they can be identified in "
      "known patterns.";

  VkPhysicalDeviceDescriptorBufferFeaturesEXT descBufFeatures = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
  };
  VkPhysicalDeviceDescriptorBufferPropertiesEXT descBufProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
  };
  VkPhysicalDeviceTexelBufferAlignmentProperties texelAlignProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES,
  };
  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
    devExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    devExts.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME);

    optFeatures.sparseBinding = VK_TRUE;
    optFeatures.sparseResidencyBuffer = VK_TRUE;
    optFeatures.sparseResidencyImage2D = VK_TRUE;

    // require RBA as we always turn it on
    features.robustBufferAccess = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    getPhysFeatures2(&descBufFeatures);
    getPhysProperties2(&descBufProps);

    static VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT texAlignFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT,
        NULL,
        true,
    };

    texelAlignProps.storageTexelBufferOffsetAlignmentBytes =
        physProperties.limits.minTexelBufferOffsetAlignment;
    texelAlignProps.uniformTexelBufferOffsetAlignmentBytes =
        physProperties.limits.minTexelBufferOffsetAlignment;
    if(hasExt(VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME) || devVersion >= VK_MAKE_VERSION(1, 3, 0))
    {
      getPhysProperties2(&texelAlignProps);

      if(devVersion < VK_MAKE_VERSION(1, 3, 0))
      {
        texAlignFeats.pNext = (void *)devInfoNext;
        devInfoNext = &texAlignFeats;
      }
    }

    if(!descBufFeatures.descriptorBuffer)
      Avail = "Feature 'descriptorBuffer' not available";

    if(!descBufFeatures.descriptorBufferCaptureReplay)
      Avail = "Feature 'descriptorBufferCaptureReplay' not available";

    descBufFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &descBufFeatures;

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufaddrFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };

    getPhysFeatures2(&bufaddrFeatures);

    if(!bufaddrFeatures.bufferDeviceAddress)
      Avail = "feature 'bufferDeviceAddress' not available";

    if(!bufaddrFeatures.bufferDeviceAddressCaptureReplay)
      Avail = "feature 'bufferDeviceAddressCaptureReplay' not available";

    bufaddrFeatures.bufferDeviceAddressCaptureReplay = 1;
    bufaddrFeatures.bufferDeviceAddressMultiDevice = 0;

    bufaddrFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &bufaddrFeatures;

    static VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
    };

    getPhysFeatures2(&ycbcrFeats);

    if(!ycbcrFeats.samplerYcbcrConversion)
      Avail = "feature 'samplerYcbcrConversion' not available";

    ycbcrFeats.pNext = (void *)devInfoNext;
    devInfoNext = &ycbcrFeats;

    static VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    };

    if(hasExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
    {
      getPhysFeatures2(&accelFeats);

      if(!accelFeats.accelerationStructure)
        Avail = "feature 'accelerationStructure' not available";

      accelFeats.pNext = (void *)devInfoNext;
      devInfoNext = &accelFeats;
    }
  }

  struct SizedBytes
  {
    byte bytes[512];
    size_t sz;

    bool operator==(const SizedBytes &o) const
    {
      return sz == o.sz && memcmp(bytes, o.bytes, sz) == 0;
    }
  };

  struct Image
  {
    std::string name;
    VkImageCreateInfo info;
    VkDeviceSize offset;
    VkDeviceSize alignment;
    VkDeviceSize size;

    VkImage img;
    SizedBytes imgCapData;

    VkImageView view;
    SizedBytes viewCapData;
  };

  bool makeImage(Image & img, VkDeviceMemory memory, uint32_t memType, uint32_t offset,
                 std::string name, VkImageCreateInfo imageCreateInfo, VkImageAspectFlags aspect,
                 VkImageViewType viewType, VkFormat viewFormat = VK_FORMAT_UNDEFINED)
  {
    VkMemoryRequirements mrq;

    img.name = name;
    img.info = imageCreateInfo;

    imageCreateInfo.flags |= VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

    vkCreateImage(device, &imageCreateInfo, NULL, &img.img);

    vkGetImageMemoryRequirements(device, img.img, &mrq);

    if(mrq.memoryTypeBits & (1U << memType))
    {
      VkImageCaptureDescriptorDataInfoEXT imgCapInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
      };

      VkImageViewCaptureDescriptorDataInfoEXT viewCapInfo = {
          VK_STRUCTURE_TYPE_IMAGE_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
      };

      img.offset = AlignUp((VkDeviceSize)offset, mrq.alignment);
      img.alignment = mrq.alignment;
      img.size = mrq.size;
      vkBindImageMemory(device, img.img, memory, img.offset);

      if(viewFormat == VK_FORMAT_UNDEFINED)
        viewFormat = imageCreateInfo.format;

      vkh::ImageViewCreateInfo viewInfo(
          img.img, viewType, viewFormat, {},
          {aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
      viewInfo.flags = VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

      img.view = createImageView(viewInfo);

      imgCapInfo.image = img.img;
      vkGetImageOpaqueCaptureDescriptorDataEXT(device, &imgCapInfo, img.imgCapData.bytes);
      img.imgCapData.sz = descBufProps.imageCaptureReplayDescriptorDataSize;

      viewCapInfo.imageView = img.view;
      vkGetImageViewOpaqueCaptureDescriptorDataEXT(device, &viewCapInfo, img.viewCapData.bytes);
      img.viewCapData.sz = descBufProps.imageViewCaptureReplayDescriptorDataSize;

      return true;
    }

    vkDestroyImage(device, img.img, NULL);

    return false;
  }

  VkDeviceSize GetElemSize(VkFormat fmt)
  {
    if(fmt == VK_FORMAT_R32G32B32A32_SFLOAT || fmt == VK_FORMAT_R32G32B32A32_UINT ||
       fmt == VK_FORMAT_R32G32B32A32_SINT)
      return 16;
    if(fmt == VK_FORMAT_R32G32B32_SFLOAT || fmt == VK_FORMAT_R32G32B32_UINT ||
       fmt == VK_FORMAT_R32G32B32_SINT)
      return 12;
    if(fmt == VK_FORMAT_R32G32_SFLOAT || fmt == VK_FORMAT_R32G32_UINT || fmt == VK_FORMAT_R32G32_SINT)
      return 8;
    if(fmt == VK_FORMAT_R32_SFLOAT || fmt == VK_FORMAT_R32_UINT || fmt == VK_FORMAT_R32_SINT)
      return 4;
    if(fmt == VK_FORMAT_R16_SFLOAT || fmt == VK_FORMAT_R16_UINT || fmt == VK_FORMAT_R16_SINT)
      return 2;
    if(fmt == VK_FORMAT_R8_UNORM || fmt == VK_FORMAT_R8_UINT || fmt == VK_FORMAT_R8_SINT)
      return 1;
    return 0;
  }

  std::string FormatStr(VkFormat fmt)
  {
    switch(fmt)
    {
      default: return "?";

      case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
      case VK_FORMAT_R32G32B32A32_SINT: return "VK_FORMAT_R32G32B32A32_SINT";
      case VK_FORMAT_R32G32B32A32_UINT: return "VK_FORMAT_R32G32B32A32_UINT";
      case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
      case VK_FORMAT_R32G32B32_SINT: return "VK_FORMAT_R32G32B32_SINT";
      case VK_FORMAT_R32G32B32_UINT: return "VK_FORMAT_R32G32B32_UINT";
      case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
      case VK_FORMAT_R32G32_SINT: return "VK_FORMAT_R32G32_SINT";
      case VK_FORMAT_R32G32_UINT: return "VK_FORMAT_R32G32_UINT";
      case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
      case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
      case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
      case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
      case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
      case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
      case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
      case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
      case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
    }
  }

  SizedBytes GetDescriptor(VkDescriptorType type, VkDeviceAddress addr, VkDeviceSize range,
                           VkFormat fmt, VkImageLayout layout, VkSampler sampler, VkImageView view)
  {
    VkDescriptorGetInfoEXT desc = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    desc.type = type;

    VkDescriptorAddressInfoEXT bufdesc = {VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};
    bufdesc.address = addr;
    bufdesc.range = range;
    bufdesc.format = fmt;

    VkDescriptorImageInfo imgdesc = {};
    imgdesc.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    imgdesc.sampler = sampler;
    imgdesc.imageView = view;

    SizedBytes descriptorBytes;
    switch(desc.type)
    {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
        descriptorBytes.sz = descBufProps.samplerDescriptorSize;
        desc.data.pSampler = &imgdesc.sampler;
        break;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        descriptorBytes.sz = descBufProps.combinedImageSamplerDescriptorSize;
        desc.data.pCombinedImageSampler = &imgdesc;
        break;
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        descriptorBytes.sz = descBufProps.inputAttachmentDescriptorSize;
        desc.data.pCombinedImageSampler = &imgdesc;
        break;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        descriptorBytes.sz = descBufProps.sampledImageDescriptorSize;
        desc.data.pCombinedImageSampler = &imgdesc;
        break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        descriptorBytes.sz = descBufProps.storageImageDescriptorSize;
        desc.data.pCombinedImageSampler = &imgdesc;
        break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        descriptorBytes.sz = descBufProps.uniformTexelBufferDescriptorSize;
        desc.data.pUniformBuffer = &bufdesc;
        break;
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        descriptorBytes.sz = descBufProps.robustStorageBufferDescriptorSize;
        desc.data.pUniformBuffer = &bufdesc;
        break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        descriptorBytes.sz = descBufProps.robustUniformBufferDescriptorSize;
        desc.data.pUniformBuffer = &bufdesc;
        break;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        descriptorBytes.sz = descBufProps.robustStorageBufferDescriptorSize;
        desc.data.pUniformBuffer = &bufdesc;
        break;
      case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        descriptorBytes.sz = descBufProps.accelerationStructureDescriptorSize;
        desc.data.accelerationStructure = addr;
        break;
      default: break;
    }

    vkGetDescriptorEXT(device, &desc, descriptorBytes.sz, descriptorBytes.bytes);

    return descriptorBytes;
  }

  SizedBytes GetDescriptor(VkDescriptorType type, VkDeviceAddress addr, VkDeviceSize range = 0,
                           VkFormat fmt = VK_FORMAT_UNDEFINED)
  {
    return GetDescriptor(type, addr, range, fmt, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE,
                         VK_NULL_HANDLE);
  }

  SizedBytes GetDescriptor(VkImageLayout layout, VkSampler sampler, VkImageView view)
  {
    return GetDescriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, 0, VK_FORMAT_UNDEFINED,
                         layout, sampler, view);
  }

  SizedBytes GetDescriptor(VkDescriptorType type, VkImageLayout layout, VkImageView view)
  {
    return GetDescriptor(type, 0, 0, VK_FORMAT_UNDEFINED, layout, VK_NULL_HANDLE, view);
  }

  SizedBytes GetDescriptor(VkSampler sampler)
  {
    return GetDescriptor(VK_DESCRIPTOR_TYPE_SAMPLER, 0, 0, VK_FORMAT_UNDEFINED,
                         VK_IMAGE_LAYOUT_UNDEFINED, sampler, VK_NULL_HANDLE);
  }

  enum class SamplerDescriptorFormat
  {
    PalettedNV,
    AMD_SGPR,
    AMD_SGPR_Fat,
    Intel_Res,
    Intel_BMage_Res,
    ARM_Res,
    QC_Res,
    QC_ResPadded,
    Count,
  };

  enum class BufferDescriptorFormat
  {
    SizeOffset,
    Pointer,
    // this doubles as offset+size assuming 48-bit address and 32-bit size
    AMD_SGPR,
    AMD_AS,
    ARM_AS,
    Intel_Res,
    Intel_BMage_Res,
    ARM_Res,
    // QC has potentially multiple descriptors for a buffer with different strides depending on
    // capabilities, we will need to detect these via probing.
    // we assume no device needs all 3 - either only one is needed or at most two because devices
    // that support 8-bit storage can all use the 16-bit descriptor to access 32-bit
    QC_Res32,
    QC_Res16,
    QC_Res32_16,
    QC_Res16_32,
    QC_Res16_8,
    QC_Res8_16,
    NVTexel,
    Packed48_16,
    Packed45_19_Align256,
    Packed45_19,
    Count,
  };

  template <typename T>
  const std::vector<T> &enumerate()
  {
    static std::vector<T> ret;
    if(ret.empty())
    {
      for(uint32_t i = 0; i < (uint32_t)T::Count; i++)
      {
        ret.push_back(T(i));
      }
    }

    return ret;
  }

  std::string name(SamplerDescriptorFormat t)
  {
    static std::string names[(uint32_t)SamplerDescriptorFormat::Count + 1] = {
        "PalettedNV", "AMD_SGPR", "AMD_SGPR_Fat", "Intel_Res", "Intel_BMage_Res",
        "ARM_Res",    "QC_Res",   "QC_ResPadded", "<unknown>",
    };

    return names[(uint32_t)t];
  }

  std::string name(BufferDescriptorFormat t)
  {
    static std::string names[(uint32_t)BufferDescriptorFormat::Count + 1] = {
        "SizeOffset",
        "Pointer",
        "AMD_SGPR",
        "AMD_AS",
        "ARM_AS",
        "Intel_Res",
        "Intel_BMage_Res",
        "ARM_Res",
        "QC_Res32",
        "QC_Res16",
        "QC_Res32_16",
        "QC_Res16_32",
        "QC_Res16_8",
        "QC_Res8_16",
        "NVTexel",
        "Packed48_16",
        "Packed45_19_Align256",
        "Packed45_19",
        "<unknown>",

    };

    return names[(uint32_t)t];
  }

  SizedBytes PredictDescriptor(SamplerDescriptorFormat fmt, const SizedBytes &sampCapData)
  {
    SizedBytes ret = {};

    if(fmt == SamplerDescriptorFormat::PalettedNV)
    {
      if(sampCapData.sz == 4)
      {
        uint32_t idx = *(uint32_t *)sampCapData.bytes;
        ret.sz = 4;
        *((uint32_t *)ret.bytes) = idx << 20U;
      }
    }
    // all others just encode the sampler directly so we can't decode and will need to hash lookup

    return ret;
  }

#define MASK_NBITS(n) ((1ULL << n) - 1)

  SizedBytes PredictDescriptor(SamplerDescriptorFormat fmt, VkDescriptorType type,
                               VkDeviceAddress baseAddr, const SizedBytes &sampCapData,
                               const SizedBytes &imgCapData, const SizedBytes &viewCapData)
  {
    SizedBytes ret = {};

    if(fmt == SamplerDescriptorFormat::PalettedNV)
    {
      if(sampCapData.sz == 4 && viewCapData.sz == 12)
      {
        uint32_t sampIdx = *(uint32_t *)sampCapData.bytes;
        uint32_t *viewIdxs = (uint32_t *)viewCapData.bytes;
        uint32_t viewIdx = type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ? viewIdxs[2]
                           : type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE  ? viewIdxs[1]
                                                                       : viewIdxs[0];
        ret.sz = 4;
        *((uint32_t *)ret.bytes) = (sampIdx << 20U) | (viewIdx & ((1 << 20U) - 1));
      }
      else if(viewCapData.sz == 12)
      {
        uint32_t *viewIdxs = (uint32_t *)viewCapData.bytes;
        uint32_t viewIdx = type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ? viewIdxs[2]
                           : type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE  ? viewIdxs[1]
                                                                       : viewIdxs[0];
        ret.sz = 4;
        *((uint32_t *)ret.bytes) = (viewIdx & ((1 << 20U) - 1));
      }
    }
    else if(fmt == SamplerDescriptorFormat::AMD_SGPR)
    {
      // we expect samplers appended to views on AMD
      // samplers are not reconstructable, we will have to do lookups
      if(type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        ret.sz = 32;
      else
        ret.sz = 48;

      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = baseAddr >> 8;
    }
    else if(fmt == SamplerDescriptorFormat::AMD_SGPR_Fat)
    {
      // we expect samplers appended to views on AMD
      // samplers are not reconstructable, we will have to do lookups
      if(type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        ret.sz = 64;
      else
        ret.sz = 96;

      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = baseAddr >> 8;
    }
    else if(fmt == SamplerDescriptorFormat::Intel_Res)
    {
      if(type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        ret.sz = 64;
      else
        ret.sz = 128;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[4] = baseAddr;
    }
    else if(fmt == SamplerDescriptorFormat::Intel_BMage_Res)
    {
      // battlemage only uses 96 for combined
      if(type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        ret.sz = 64;
      else
        ret.sz = 96;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[4] = baseAddr & MASK_NBITS(48);
    }
    else if(fmt == SamplerDescriptorFormat::ARM_Res)
    {
      if(viewCapData.sz == 16)
      {
        uint64_t *viewBases = (uint64_t *)viewCapData.bytes;
        if(type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          ret.sz = 32;
        else
          ret.sz = 64;

        uint64_t *out = (uint64_t *)ret.bytes;
        out[2] = type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? viewBases[1] : viewBases[0];
      }
    }
    else if(fmt == SamplerDescriptorFormat::QC_Res || fmt == SamplerDescriptorFormat::QC_ResPadded)
    {
      if(type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        ret.sz = 64;
      else
        ret.sz = 80;    // 16 bytes of sampler after

      // padded to alignment of base descriptor
      if(fmt == SamplerDescriptorFormat::QC_ResPadded && ret.sz == 80)
        ret.sz = 128;

      uint64_t *out = (uint64_t *)ret.bytes;
      out[2] = baseAddr;
    }

    return ret;
  }

  SizedBytes PredictDescriptor(BufferDescriptorFormat fmt, bool storage, VkDeviceAddress ptr,
                               VkDeviceSize range, VkFormat texelFormat)
  {
    SizedBytes ret = {};

    if(fmt == BufferDescriptorFormat::AMD_SGPR)
    {
      ret.sz = 16;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = ptr;
      if(texelFormat == VK_FORMAT_UNDEFINED)
        out[1] = range;
      else
        out[1] = range / GetElemSize(texelFormat);
    }
    else if(fmt == BufferDescriptorFormat::AMD_AS)
    {
      ret.sz = 16;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = ptr;
    }
    else if(fmt == BufferDescriptorFormat::ARM_AS)
    {
      ret.sz = 32;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[1] = ptr;
    }
    else if(fmt == BufferDescriptorFormat::Intel_Res)
    {
      ret.sz = 64;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[4] = ptr;
      if(texelFormat == VK_FORMAT_UNDEFINED && !storage)
        out[5] = AlignUp(range, (VkDeviceSize)64) << 32ULL;
      else
        out[5] = range << 32ULL;
    }
    else if(fmt == BufferDescriptorFormat::Intel_BMage_Res)
    {
      ret.sz = 64;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[4] = ptr;

      // Intel battlemage stores size-1 where size is in bytes/texels
      uint64_t num = range - 1;
      if(texelFormat != VK_FORMAT_UNDEFINED)
        num = (range / GetElemSize(texelFormat)) - 1;

      // bottom 4 bits are swizzled for 1 byte texel formats or plain buffers
      if(texelFormat == VK_FORMAT_UNDEFINED || GetElemSize(texelFormat) == 1)
      {
        uint8_t x = num & 0xff;
        num = (num & ~0xff) + ((x & 0xfc) + 6 - (x & 0x3));
      }
      else if(GetElemSize(texelFormat) == 2)
      {
        // 2 byte formats have a different swizzling
        uint8_t x = num & 0xff;
        num = (num & ~0xff) + ((x & 0xfe) + 2 - (x & 0x1));
      }
      // 4 byte and up just encode elems-1

      // bits are then scattered around
      out[1] = ((num & 0x00007f) << 0) | ((num & 0x1fff80) << 9) | ((num >> 21) << 53);
    }
    else if(fmt == BufferDescriptorFormat::ARM_Res)
    {
      ret.sz = 32;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = range << 32ULL;
      out[1] = ptr;
    }
    else if(fmt == BufferDescriptorFormat::SizeOffset)
    {
      ret.sz = 16;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = range;
      out[1] = ptr;
    }
    else if(fmt == BufferDescriptorFormat::Pointer)
    {
      ret.sz = 8;
      uint64_t *out = (uint64_t *)ret.bytes;
      out[0] = ptr;
    }
    else if(fmt == BufferDescriptorFormat::QC_Res32 || fmt == BufferDescriptorFormat::QC_Res16 ||
            fmt == BufferDescriptorFormat::QC_Res32_16 || fmt == BufferDescriptorFormat::QC_Res16_32 ||
            fmt == BufferDescriptorFormat::QC_Res16_8 || fmt == BufferDescriptorFormat::QC_Res8_16)
    {
      uint64_t numDescriptors =
          (fmt == BufferDescriptorFormat::QC_Res32 || fmt == BufferDescriptorFormat::QC_Res16) ? 1
                                                                                               : 2;

      uint64_t strides[2] = {0, 0};

      switch(fmt)
      {
        case BufferDescriptorFormat::QC_Res32: strides[0] = 4; break;
        case BufferDescriptorFormat::QC_Res16: strides[0] = 2; break;
        case BufferDescriptorFormat::QC_Res32_16:
          strides[0] = 4;
          strides[1] = 2;
          break;
        case BufferDescriptorFormat::QC_Res16_32:
          strides[0] = 2;
          strides[1] = 4;
          break;
        case BufferDescriptorFormat::QC_Res16_8:
          strides[0] = 2;
          strides[1] = 1;
          break;
        case BufferDescriptorFormat::QC_Res8_16:
          strides[0] = 1;
          strides[1] = 2;
          break;
        default: break;
      }

      if(texelFormat != VK_FORMAT_UNDEFINED)
      {
        // texel buffers are treated as texture since they're formatted
        ret = PredictDescriptor(SamplerDescriptorFormat::QC_Res,
                                storage ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                                        : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                ptr, {}, {}, {});

        // add the range
        uint64_t *out = (uint64_t *)ret.bytes;
        out[0] = (range / GetElemSize(texelFormat)) << 32;
      }
      else
      {
        uint64_t offset = ptr & 0x3f;
        uint64_t alignedPtr = ptr & ~offset;

        ret.sz = 64 * numDescriptors;
        uint64_t *out = (uint64_t *)ret.bytes;
        out[0] = (AlignUp(range, strides[0]) >> strides[0]) << 32;
        out[1] = (offset >> strides[0]) << 16;
        out[2] = alignedPtr;
        if(numDescriptors == 2)
        {
          out[8 + 0] = (AlignUp(range, strides[1]) >> strides[1]) << 32;
          out[8 + 1] = (offset >> strides[1]) << 16;
          out[8 + 2] = alignedPtr;
        }
      }
    }
    else if(fmt == BufferDescriptorFormat::NVTexel && texelFormat != VK_FORMAT_UNDEFINED)
    {
      ret.sz = 16;
      uint64_t *out = (uint64_t *)ret.bytes;
      switch(GetElemSize(texelFormat))
      {
        case 16: out[0] = ptr >> 4; break;
        case 8: out[0] = ptr >> 3; break;
        case 4: out[0] = ptr >> 2; break;
        case 2: out[0] = ptr >> 1; break;
        case 1: out[0] = ptr >> 0; break;
        case 12:
        {
          uint64_t a = ptr;
          if(ptr % 3)
            a -= (3 - (ptr % 3)) << 7;
          out[0] = a / 12;
          break;
        }
      }
      out[1] = range / GetElemSize(texelFormat);
    }
    else if(fmt == BufferDescriptorFormat::Packed45_19)
    {
      // pointer must be aligned, we can just clip lower bits
      ptr >>= 4;
      // range we must round up
      range = (range + 15) >> 4;

      if((ptr & MASK_NBITS(45)) == ptr && (range & MASK_NBITS(19)) == range)
      {
        ret.sz = 8;
        *((uint64_t *)ret.bytes) = ptr | (range << 45);
      }
    }
    else if(fmt == BufferDescriptorFormat::Packed45_19_Align256)
    {
      // pointer must be aligned, we can just clip lower bits
      ptr >>= 4;
      // range we must round up
      range = AlignUp(range, (uint64_t)256) >> 4;

      if((ptr & MASK_NBITS(45)) == ptr && (range & MASK_NBITS(19)) == range)
      {
        ret.sz = 8;
        *((uint64_t *)ret.bytes) = ptr | (range << 45);
      }
    }
    else if(fmt == BufferDescriptorFormat::Packed48_16)
    {
      // range we must round up
      range = (range + 15) >> 4;

      if((ptr & MASK_NBITS(48)) == ptr && (range & MASK_NBITS(16)) == range)
      {
        ret.sz = 8;
        *((uint64_t *)ret.bytes) = ptr | (range << 48);
      }
    }

    return ret;
  }

  bool MatchPrediction(SamplerDescriptorFormat fmt, const SizedBytes &descriptor,
                       const SizedBytes &prediction)
  {
    if(prediction.sz != descriptor.sz)
      return false;

    // only compare base address
    if(fmt == SamplerDescriptorFormat::AMD_SGPR || fmt == SamplerDescriptorFormat::AMD_SGPR_Fat)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if((a[0] & MASK_NBITS(40)) == (b[0] & MASK_NBITS(40)))
        return true;

      // hack for planes with base offsets
      if((a[0] & MASK_NBITS(40)) == (b[0] & MASK_NBITS(40)) + 256)
        return true;
      if((a[0] & MASK_NBITS(40)) == (b[0] & MASK_NBITS(40)) + 512)
        return true;
      // don't know why amdvlk/radv differ on this but it won't matter
      if((a[0] & MASK_NBITS(40)) == (b[0] & MASK_NBITS(40)) + 16)
        return true;
      if((a[0] & MASK_NBITS(40)) == (b[0] & MASK_NBITS(40)) + 32)
        return true;

      return false;
    }
    else if(fmt == SamplerDescriptorFormat::Intel_Res ||
            fmt == SamplerDescriptorFormat::Intel_BMage_Res)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[4] == b[4])
        return true;
      // hack for planes with base offsets
      if(a[4] == b[4] + 0x2000)
        return true;
      if(a[4] == b[4] + 0x3000)
        return true;
      if(a[4] == b[4] + 0x10000)
        return true;
      if(a[4] == b[4] + 0x11000)
        return true;

      return false;
    }
    else if(fmt == SamplerDescriptorFormat::ARM_Res)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[2] != b[2])
        return false;

      return true;
    }
    else if(fmt == SamplerDescriptorFormat::QC_Res || fmt == SamplerDescriptorFormat::QC_ResPadded)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if((a[2] & MASK_NBITS(48)) != (b[2] & MASK_NBITS(48)))
        return false;

      return true;
    }

    return descriptor == prediction;
  }

  bool MatchPrediction(BufferDescriptorFormat fmt, const SizedBytes &descriptor,
                       const SizedBytes &prediction)
  {
    if(prediction.sz != descriptor.sz)
      return false;

    if(fmt == BufferDescriptorFormat::AMD_SGPR)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if((a[0] & MASK_NBITS(48)) != (b[0] & MASK_NBITS(48)))
        return false;

      // allow 4-byte alignment
      if(AlignUp(a[1] & MASK_NBITS(32), 4ULL) != AlignUp(b[1] & MASK_NBITS(32), 4ULL))
        return false;

      return true;
    }
    else if(fmt == BufferDescriptorFormat::AMD_AS)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if((a[0] & MASK_NBITS(48)) != (b[0] & MASK_NBITS(48)))
        return false;

      return true;
    }
    else if(fmt == BufferDescriptorFormat::ARM_AS)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[1] != b[1])
        return false;

      return true;
    }
    else if(fmt == BufferDescriptorFormat::Intel_Res)
    {
      // only compare address and size
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[4] != b[4])
        return false;

      if(a[5] >> 32ULL != b[5] >> 32ULL)
        return false;

      return true;
    }
    else if(fmt == BufferDescriptorFormat::Intel_BMage_Res)
    {
      // only compare address and size
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[4] != b[4])
        return false;

      if((a[1] & 0xFFF00000001FFFFF) != (b[1] & 0xFFF00000001FFFFF))
        return false;

      return true;
    }
    else if(fmt == BufferDescriptorFormat::ARM_Res)
    {
      // only compare address and size
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[1] != b[1])
        return false;

      if(a[0] >> 32ULL != b[0] >> 32ULL)
        return false;

      return true;
    }
    else if(fmt == BufferDescriptorFormat::QC_Res32 || fmt == BufferDescriptorFormat::QC_Res16 ||
            fmt == BufferDescriptorFormat::QC_Res32_16 || fmt == BufferDescriptorFormat::QC_Res16_32 ||
            fmt == BufferDescriptorFormat::QC_Res16_8 || fmt == BufferDescriptorFormat::QC_Res8_16)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      // size
      if(((a[0] >> 32) & 0x7fffffff) != ((b[0] >> 32) & 0x7fffffff))
        return false;

      // aligned offset
      if((a[1] & 0x3f0000) != (b[1] & 0x3f0000))
        return false;

      // aligned pointer
      if(a[2] != b[2])
        return false;

      // if there's a second descriptor, check it
      if(descriptor.sz == 128)
      {
        if(((a[8 + 0] >> 32) & 0x7fffffff) != ((b[8 + 0] >> 32) & 0x7fffffff))
          return false;

        if((a[8 + 1] & 0x3f0000) != (b[8 + 1] & 0x3f0000))
          return false;

        if(a[8 + 2] != b[8 + 2])
          return false;
      }

      return true;
    }
    else if(fmt == BufferDescriptorFormat::NVTexel)
    {
      uint64_t *a = (uint64_t *)descriptor.bytes;
      uint64_t *b = (uint64_t *)prediction.bytes;

      if(a[0] != b[0])
        return false;

      if((a[1] & 0xffffffffULL) != (b[1] & 0xffffffffULL))
        return false;

      return true;
    }

    return descriptor == prediction;
  }

  void DumpData(const std::string &name, const SizedBytes &data)
  {
    TEST_LOG("  %s is %u bytes:", name.c_str(), (uint32_t)data.sz);

    if(data.sz == 0)
    {
      TEST_LOG("    ---");
      return;
    }

    std::string dump;

    const byte *cur = &data.bytes[0];

    uint32_t i = 0;
    if(data.sz == 1)
    {
      dump += fmt::format(" {:#04x}", *cur);
      cur++;
      i++;
    }
    for(; i + 8 <= data.sz; i += 8)
    {
      dump += fmt::format(" {:#018x}", *(uint64_t *)cur);
      cur += 8;
    }
    for(; i + 4 <= data.sz; i += 4)
      dump += fmt::format(" {:#010x}", *(uint32_t *)cur);

    TEST_ASSERT(i == data.sz, "Expected 4-byte aligned capture data");

    dump.erase(0, 1);
    TEST_LOG("    %s", dump.c_str());

    dump.clear();
    cur = &data.bytes[0];

    i = 0;
    if(data.sz == 1)
    {
      dump += fmt::format(" {:#010b}", *cur);
      cur++;
      i++;
    }
    for(; i + 8 <= data.sz; i += 8)
    {
      dump += fmt::format(" {:#066b} ", *(uint64_t *)cur);
      cur += 8;
    }
    for(; i + 4 <= data.sz; i += 4)
      dump += fmt::format(" {:#034b} ", *(uint32_t *)cur);

    TEST_ASSERT(i == data.sz, "Expected 4-byte aligned capture data");

    dump.erase(0, 1);
    TEST_LOG("    %s", dump.c_str());
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    // allocate 20MB of BDA-able memory in every device local memory type with a buffer

    static const uint64_t bufferSize = 20 * 1000 * 1000;
    static const uint64_t blasSize = 9999;
    static const uint64_t blasOffset = 512;

    vkh::BufferCreateInfo bda_buffer_info(
        bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
            VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
        VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT |
            VK_BUFFER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT);

    if(hasExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
      bda_buffer_info.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    vkh::BufferCreateInfo sparse_buffer_info = bda_buffer_info;
    sparse_buffer_info.flags |=
        VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;

    VkMemoryAllocateInfo memAllocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryAllocateFlagsInfo memAllocFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    VkBufferDeviceAddressInfoKHR bda_info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};

    memAllocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR |
                          VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
    memAllocInfo.pNext = &memAllocFlags;

    const VkPhysicalDeviceMemoryProperties *memProps = NULL;
    vmaGetMemoryProperties(allocator, &memProps);

    VkDeviceMemory memory[16] = {};

    VkBuffer buffer[17] = {};
    VkDeviceAddress ptrs[17] = {};
    SizedBytes bufCapData[17] = {};

    VkBuffer sparseBuffer = {};
    Image sparseImage = {};

    VkAccelerationStructureKHR blas[16] = {};
    VkDeviceAddress blasAddr[16] = {};
    SizedBytes blasCapData[16] = {};

    std::vector<Image> images[16] = {};

    VkSamplerYcbcrConversionCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
    };
    {
      createInfo.chromaFilter = VK_FILTER_LINEAR;
      createInfo.xChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;
      createInfo.yChromaOffset = VK_CHROMA_LOCATION_MIDPOINT;

      createInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020;
      createInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
    }

    for(uint32_t i = 0; i < memProps->memoryTypeCount; i++)
    {
      if(memProps->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
      {
        vkCreateBuffer(device, bda_buffer_info, NULL, &buffer[i]);

        memAllocInfo.memoryTypeIndex = i;
        memAllocInfo.allocationSize = bda_buffer_info.size;
        vkAllocateMemory(device, &memAllocInfo, NULL, &memory[i]);
        vkBindBufferMemory(device, buffer[i], memory[i], 0);

        bda_info.buffer = buffer[i];
        ptrs[i] = vkGetBufferDeviceAddressKHR(device, &bda_info);

        VkBufferCaptureDescriptorDataInfoEXT bufCapInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
            NULL,
            buffer[i],
        };

        vkGetBufferOpaqueCaptureDescriptorDataEXT(device, &bufCapInfo, bufCapData[i].bytes);
        bufCapData[i].sz = descBufProps.bufferCaptureReplayDescriptorDataSize;

        VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        };
        blasCreateInfo.buffer = buffer[i];
        blasCreateInfo.size = blasSize;
        blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blasCreateInfo.offset = blasOffset;
        blasCreateInfo.createFlags =
            VK_ACCELERATION_STRUCTURE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT |
            VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR;

        if(hasExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
        {
          CHECK_VKR(
              vkCreateAccelerationStructureKHR(device, &blasCreateInfo, VK_NULL_HANDLE, &blas[i]))

          VkAccelerationStructureDeviceAddressInfoKHR blasDeviceAddressInfo = {
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
              NULL,
              blas[i],
          };
          blasAddr[i] = vkGetAccelerationStructureDeviceAddressKHR(device, &blasDeviceAddressInfo);

          VkAccelerationStructureCaptureDescriptorDataInfoEXT blasCapInfo = {
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
              NULL,
              blas[i],
          };

          vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT(device, &blasCapInfo,
                                                                   &blasCapData[i].bytes);
          blasCapData[i].sz = descBufProps.accelerationStructureCaptureReplayDescriptorDataSize;
        }

        Image img;

        for(uint32_t offset : {0, 1, 4, 16, 32, 128, 256, 512, 1024, 3000, 4000, 8000, 10000})
        {
          if(makeImage(img, memory[i], i, offset, "sampled",
                       vkh::ImageCreateInfo(54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "yuv3_plane0",
                       vkh::ImageCreateInfo(64, 64, 0, VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                                            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT),
                       VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM))
          {
            images[i].push_back(img);

            VkSamplerYcbcrConversionInfo ycbcrChain = {
                VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};

            createInfo.format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
            vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcrChain.conversion);

            vkh::ImageViewCreateInfo viewCreateInfo(
                img.img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, {},
                vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));

            viewCreateInfo.pNext = &ycbcrChain;

            img.name = "yuv3_combined";
            img.view = createImageView(viewCreateInfo);

            vkDestroySamplerYcbcrConversionKHR(device, ycbcrChain.conversion, NULL);
          }

          if(makeImage(img, memory[i], i, offset, "yuv3_plane1",
                       vkh::ImageCreateInfo(64, 64, 0, VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                                            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT),
                       VK_IMAGE_ASPECT_PLANE_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "yuv3_plane2",
                       vkh::ImageCreateInfo(64, 64, 0, VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                                            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT),
                       VK_IMAGE_ASPECT_PLANE_2_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "yuv2_plane0",
                       vkh::ImageCreateInfo(64, 64, 0, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                                            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT),
                       VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM))
          {
            images[i].push_back(img);

            VkSamplerYcbcrConversionInfo ycbcrChain = {
                VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};

            createInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
            vkCreateSamplerYcbcrConversionKHR(device, &createInfo, NULL, &ycbcrChain.conversion);

            vkh::ImageViewCreateInfo viewCreateInfo(
                img.img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, {},
                vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));

            viewCreateInfo.pNext = &ycbcrChain;

            img.name = "yuv2_combined";
            img.view = createImageView(viewCreateInfo);

            vkDestroySamplerYcbcrConversionKHR(device, ycbcrChain.conversion, NULL);
          }

          if(makeImage(img, memory[i], i, offset, "yuv2_plane1",
                       vkh::ImageCreateInfo(64, 64, 0, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                                            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT),
                       VK_IMAGE_ASPECT_PLANE_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8_UNORM))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "storage",
                       vkh::ImageCreateInfo(54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_USAGE_STORAGE_BIT, 1, 1, VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "sampled_storage",
                       vkh::ImageCreateInfo(54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                            1, 1, VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "sampled_colatt",
                       vkh::ImageCreateInfo(
                           54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, 1,
                           VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "msaa",
                       vkh::ImageCreateInfo(
                           54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, 1,
                           VK_SAMPLE_COUNT_4_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "storage_colatt",
                       vkh::ImageCreateInfo(
                           54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, 1,
                           VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "sampled_dsatt_depth",
                       vkh::ImageCreateInfo(
                           54, 55, 0, VK_FORMAT_D32_SFLOAT_S8_UINT,
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           1, 1, VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "sampled_dsatt_stencil",
                       vkh::ImageCreateInfo(
                           54, 55, 0, VK_FORMAT_D32_SFLOAT_S8_UINT,
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           1, 1, VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);

          if(makeImage(img, memory[i], i, offset, "input",
                       vkh::ImageCreateInfo(
                           54, 55, 0, VK_FORMAT_R8G8B8A8_UNORM,
                           VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1,
                           VK_SAMPLE_COUNT_1_BIT),
                       VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D))
            images[i].push_back(img);
        }
      }
    }

    {
      uint32_t i = memProps->memoryTypeCount;

      if(sparseBuffer == VK_NULL_HANDLE && features.sparseBinding && features.sparseResidencyBuffer)
      {
        vkCreateBuffer(device, sparse_buffer_info, NULL, &sparseBuffer);

        bda_info.buffer = sparseBuffer;
        ptrs[i] = vkGetBufferDeviceAddressKHR(device, &bda_info);

        VkBufferCaptureDescriptorDataInfoEXT bufCapInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
            NULL,
            sparseBuffer,
        };

        vkGetBufferOpaqueCaptureDescriptorDataEXT(device, &bufCapInfo, bufCapData[i].bytes);
        bufCapData[i].sz = descBufProps.bufferCaptureReplayDescriptorDataSize;
      }
      if(sparseImage.img == VK_NULL_HANDLE && features.sparseBinding &&
         features.sparseResidencyImage2D)
      {
        vkh::ImageCreateInfo imageCreateInfo(
            54, 55, 0, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, 1, 1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT);

        vkCreateImage(device, imageCreateInfo, NULL, &sparseImage.img);

        VkImageCaptureDescriptorDataInfoEXT imgCapInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
        };

        VkImageViewCaptureDescriptorDataInfoEXT viewCapInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
        };

        VkMemoryRequirements mrq;

        vkGetImageMemoryRequirements(device, sparseImage.img, &mrq);

        sparseImage.name = "SparseImage";
        sparseImage.info = imageCreateInfo;

        sparseImage.offset = 0;
        sparseImage.alignment = mrq.alignment;
        sparseImage.size = mrq.size;

        vkh::ImageViewCreateInfo viewInfo(
            sparseImage.img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32_SFLOAT, {},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
        viewInfo.flags = VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT;

        sparseImage.view = createImageView(viewInfo);

        imgCapInfo.image = sparseImage.img;
        vkGetImageOpaqueCaptureDescriptorDataEXT(device, &imgCapInfo, sparseImage.imgCapData.bytes);
        sparseImage.imgCapData.sz = descBufProps.imageCaptureReplayDescriptorDataSize;

        viewCapInfo.imageView = sparseImage.view;
        vkGetImageViewOpaqueCaptureDescriptorDataEXT(device, &viewCapInfo,
                                                     sparseImage.viewCapData.bytes);
        sparseImage.viewCapData.sz = descBufProps.imageViewCaptureReplayDescriptorDataSize;
      }
    }

    // vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT;

    VkSampler samps[] = {
        createSampler(vkh::SamplerCreateInfo(
            VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f,
            0.0f, 0.0f, VK_COMPARE_OP_NEVER, VK_FALSE,
            VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT)),
        createSampler(vkh::SamplerCreateInfo(
            VK_FILTER_NEAREST, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f,
            0.0f, 0.0f, VK_COMPARE_OP_NEVER, VK_FALSE,
            VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT)),
        createSampler(vkh::SamplerCreateInfo(
            VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f,
            0.0f, 0.0f, VK_COMPARE_OP_NEVER, VK_FALSE,
            VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT)),
        createSampler(vkh::SamplerCreateInfo(
            VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f,
            0.0f, 0.0f, VK_COMPARE_OP_NEVER, VK_FALSE,
            VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT)),
    };

    SizedBytes samplerCapData[ARRAY_COUNT(samps)] = {};

    std::vector<SamplerDescriptorFormat> samplerFormats;
    std::vector<BufferDescriptorFormat> asFormats;
    std::vector<BufferDescriptorFormat> uboFormats;
    std::vector<BufferDescriptorFormat> ssboFormats;
    std::vector<BufferDescriptorFormat> uniTexelFormats;
    std::vector<BufferDescriptorFormat> storTexelFormats;
    SamplerDescriptorFormat combined = SamplerDescriptorFormat::Count,
                            sampled = SamplerDescriptorFormat::Count,
                            storage = SamplerDescriptorFormat::Count,
                            input = SamplerDescriptorFormat::Count;
    uint32_t imperfectDetection = 0;
    uint32_t failedDetections = 0;

    for(uint32_t i = 0; i < ARRAY_COUNT(samps); i++)
    {
      VkSamplerCaptureDescriptorDataInfoEXT sampCapInfo = {
          VK_STRUCTURE_TYPE_SAMPLER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT,
          NULL,
          samps[i],
      };

      vkGetSamplerOpaqueCaptureDescriptorDataEXT(device, &sampCapInfo, samplerCapData[i].bytes);
      samplerCapData[i].sz = descBufProps.samplerCaptureReplayDescriptorDataSize;

      SizedBytes descriptor = GetDescriptor(samps[i]);

      for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
      {
        SizedBytes prediction = PredictDescriptor(fmt, samplerCapData[i]);
        if(MatchPrediction(fmt, descriptor, prediction))
        {
          if(std::find(samplerFormats.begin(), samplerFormats.end(), fmt) == samplerFormats.end())
            samplerFormats.push_back(fmt);
          break;
        }
      }

      // don't try to require a sampler format. There may not be one if it's just pure sampler encoded
    }

    for(uint32_t i = 0; i < memProps->memoryTypeCount + 1; i++)
    {
      if(ptrs[i] == 0)
        continue;

      VkDeviceSize uboAlign = physProperties.limits.minUniformBufferOffsetAlignment;
      VkDeviceSize uboMax = physProperties.limits.maxUniformBufferRange;

      for(VkDeviceSize offset : {(VkDeviceSize)0, uboAlign, uboAlign * 10, uboAlign * 16})
      {
        for(VkDeviceSize size : {(VkDeviceSize)1ULL,
                                 (VkDeviceSize)2ULL,
                                 (VkDeviceSize)3ULL,
                                 (VkDeviceSize)4ULL,
                                 (VkDeviceSize)5ULL,
                                 (VkDeviceSize)6ULL,
                                 (VkDeviceSize)7ULL,
                                 (VkDeviceSize)8ULL,
                                 (VkDeviceSize)9ULL,
                                 (VkDeviceSize)10ULL,
                                 (VkDeviceSize)11ULL,
                                 (VkDeviceSize)12ULL,
                                 (VkDeviceSize)13ULL,
                                 (VkDeviceSize)14ULL,
                                 (VkDeviceSize)15ULL,
                                 (VkDeviceSize)16ULL,
                                 (VkDeviceSize)17ULL,
                                 (VkDeviceSize)18ULL,
                                 (VkDeviceSize)19ULL,
                                 (VkDeviceSize)20ULL,
                                 (VkDeviceSize)21ULL,
                                 (VkDeviceSize)22ULL,
                                 (VkDeviceSize)23ULL,
                                 (VkDeviceSize)24ULL,
                                 (VkDeviceSize)25ULL,
                                 (VkDeviceSize)100ULL,
                                 (VkDeviceSize)128ULL,
                                 (VkDeviceSize)236ULL,
                                 (VkDeviceSize)237ULL,
                                 (VkDeviceSize)238ULL,
                                 (VkDeviceSize)239ULL,
                                 (VkDeviceSize)240ULL,
                                 (VkDeviceSize)241ULL,
                                 (VkDeviceSize)242ULL,
                                 (VkDeviceSize)243ULL,
                                 (VkDeviceSize)244ULL,
                                 (VkDeviceSize)245ULL,
                                 (VkDeviceSize)246ULL,
                                 (VkDeviceSize)247ULL,
                                 (VkDeviceSize)248ULL,
                                 (VkDeviceSize)249ULL,
                                 (VkDeviceSize)250ULL,
                                 (VkDeviceSize)251ULL,
                                 (VkDeviceSize)252ULL,
                                 (VkDeviceSize)253ULL,
                                 (VkDeviceSize)254ULL,
                                 (VkDeviceSize)255ULL,
                                 (VkDeviceSize)256ULL,
                                 (VkDeviceSize)257ULL,
                                 (VkDeviceSize)258ULL,

                                 VkDeviceSize(0x111111) + 1,
                                 VkDeviceSize(0x222222) + 1,
                                 VkDeviceSize(0x444444) + 1,
                                 VkDeviceSize(0x888888) + 1,
                                 VkDeviceSize(0xffffff) + 1,
                                 VkDeviceSize(0xfffffe) + 1,
                                 VkDeviceSize(0xfffffd) + 1,
                                 VkDeviceSize(0xfffffc) + 1,
                                 VkDeviceSize(0xfffffb) + 1,
                                 VkDeviceSize(0xfffffa) + 1,
                                 VkDeviceSize(0x1f1f1f) + 1,
                                 VkDeviceSize(0xf1f1f1) + 1,
                                 VkDeviceSize(0x2f2f2f) + 1,
                                 VkDeviceSize(0xf2f2f2) + 1,
                                 VkDeviceSize(0x4f4f4f) + 1,
                                 VkDeviceSize(0xf4f4f4) + 1,
                                 VkDeviceSize(0x8f8f8f) + 1,
                                 VkDeviceSize(0xf8f8f8) + 1,
                                 uboMax})
        {
          size = std::min(size, bda_buffer_info.size - offset);
          size = std::min(size, uboMax);

          SizedBytes descriptor =
              GetDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ptrs[i] + offset, size);

          std::vector<BufferDescriptorFormat> predicted;

          for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, false, ptrs[i] + offset, size, VK_FORMAT_UNDEFINED);
            if(MatchPrediction(fmt, descriptor, prediction))
              predicted.push_back(fmt);
          }
          if(predicted.empty())
          {
            failedDetections++;
            TEST_WARN("!!! Couldn't detect buffer format");
            DumpData("bufferCapData", bufCapData[i]);
            TEST_LOG("Base pointer is %p", ptrs[i]);

            DumpData(fmt::format("UBO descriptor with offs {} size {}", offset, size), descriptor);
            for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, false, ptrs[i] + offset, size, VK_FORMAT_UNDEFINED);
              DumpData(fmt::format("{} prediction", name(fmt)), prediction);
            }
          }

          if(!uboFormats.empty())
          {
            // ensure that at least one previously predicted format is still predicted
            bool oneKnown = false;
            for(BufferDescriptorFormat fmt : predicted)
            {
              if(std::find(uboFormats.begin(), uboFormats.end(), fmt) != uboFormats.end())
                oneKnown = true;
            }

            if(!oneKnown)
            {
              imperfectDetection++;
              TEST_WARN("No commonly predicted formats. Previous:");
              for(BufferDescriptorFormat fmt : uboFormats)
                TEST_WARN("%s", name(fmt).c_str());
              TEST_WARN("Predicted:");
              for(BufferDescriptorFormat fmt : predicted)
                TEST_WARN("%s", name(fmt).c_str());
            }

            if(predicted.size() < uboFormats.size())
              uboFormats = predicted;
          }
          else
          {
            uboFormats = predicted;
          }
        }
      }

      VkDeviceSize ssboAlign = physProperties.limits.minStorageBufferOffsetAlignment;
      VkDeviceSize ssboMax = physProperties.limits.maxStorageBufferRange;

      for(VkDeviceSize offset : {(VkDeviceSize)0ULL, ssboAlign, ssboAlign * 10, ssboAlign * 16})
      {
        for(VkDeviceSize size : {(VkDeviceSize)1ULL,
                                 (VkDeviceSize)2ULL,
                                 (VkDeviceSize)3ULL,
                                 (VkDeviceSize)4ULL,
                                 (VkDeviceSize)5ULL,
                                 (VkDeviceSize)6ULL,
                                 (VkDeviceSize)7ULL,
                                 (VkDeviceSize)8ULL,
                                 (VkDeviceSize)9ULL,
                                 (VkDeviceSize)10ULL,
                                 (VkDeviceSize)11ULL,
                                 (VkDeviceSize)12ULL,
                                 (VkDeviceSize)13ULL,
                                 (VkDeviceSize)14ULL,
                                 (VkDeviceSize)15ULL,
                                 (VkDeviceSize)16ULL,
                                 (VkDeviceSize)17ULL,
                                 (VkDeviceSize)18ULL,
                                 (VkDeviceSize)100ULL,
                                 (VkDeviceSize)128ULL,
                                 (VkDeviceSize)236ULL,
                                 (VkDeviceSize)237ULL,
                                 (VkDeviceSize)238ULL,
                                 (VkDeviceSize)239ULL,
                                 (VkDeviceSize)240ULL,
                                 (VkDeviceSize)241ULL,
                                 (VkDeviceSize)242ULL,
                                 (VkDeviceSize)243ULL,
                                 (VkDeviceSize)244ULL,
                                 (VkDeviceSize)245ULL,
                                 (VkDeviceSize)246ULL,
                                 (VkDeviceSize)247ULL,
                                 (VkDeviceSize)248ULL,
                                 (VkDeviceSize)249ULL,
                                 (VkDeviceSize)250ULL,
                                 (VkDeviceSize)251ULL,
                                 (VkDeviceSize)252ULL,
                                 (VkDeviceSize)253ULL,
                                 (VkDeviceSize)254ULL,
                                 (VkDeviceSize)255ULL,
                                 (VkDeviceSize)256ULL,
                                 (VkDeviceSize)257ULL,
                                 (VkDeviceSize)258ULL,

                                 VkDeviceSize(0x111111) + 1,
                                 VkDeviceSize(0x222222) + 1,
                                 VkDeviceSize(0x444444) + 1,
                                 VkDeviceSize(0x888888) + 1,
                                 VkDeviceSize(0xffffff) + 1,
                                 VkDeviceSize(0xfffffe) + 1,
                                 VkDeviceSize(0xfffffd) + 1,
                                 VkDeviceSize(0xfffffc) + 1,
                                 VkDeviceSize(0xfffffb) + 1,
                                 VkDeviceSize(0xfffffa) + 1,
                                 VkDeviceSize(0x1f1f1f) + 1,
                                 VkDeviceSize(0xf1f1f1) + 1,
                                 VkDeviceSize(0x2f2f2f) + 1,
                                 VkDeviceSize(0xf2f2f2) + 1,
                                 VkDeviceSize(0x4f4f4f) + 1,
                                 VkDeviceSize(0xf4f4f4) + 1,
                                 VkDeviceSize(0x8f8f8f) + 1,
                                 VkDeviceSize(0xf8f8f8) + 1,
                                 ssboMax})
        {
          size = std::min(size, bda_buffer_info.size - offset);
          size = std::min(size, ssboMax);

          SizedBytes descriptor =
              GetDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ptrs[i] + offset, size);

          std::vector<BufferDescriptorFormat> predicted;

          for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, true, ptrs[i] + offset, size, VK_FORMAT_UNDEFINED);
            if(MatchPrediction(fmt, descriptor, prediction))
              predicted.push_back(fmt);
          }
          if(predicted.empty())
          {
            failedDetections++;
            TEST_WARN("!!! Couldn't detect buffer format");
            DumpData("bufferCapData", bufCapData[i]);
            TEST_LOG("Base pointer is %p", ptrs[i]);

            DumpData(fmt::format("SSBO descriptor with offs {} size {}", offset, size), descriptor);
            for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, true, ptrs[i] + offset, size, VK_FORMAT_UNDEFINED);
              DumpData(fmt::format("{} prediction", name(fmt)), prediction);
            }
          }

          if(!ssboFormats.empty())
          {
            // ensure that at least one previously predicted format is still predicted
            bool oneKnown = false;
            for(BufferDescriptorFormat fmt : predicted)
            {
              if(std::find(ssboFormats.begin(), ssboFormats.end(), fmt) != ssboFormats.end())
                oneKnown = true;
            }

            if(!oneKnown)
            {
              imperfectDetection++;
              TEST_WARN("No commonly predicted formats. Previous:");
              for(BufferDescriptorFormat fmt : ssboFormats)
                TEST_WARN("%s", name(fmt).c_str());
              TEST_WARN("Predicted:");
              for(BufferDescriptorFormat fmt : predicted)
                TEST_WARN("%s", name(fmt).c_str());
            }

            if(predicted.size() < ssboFormats.size())
              ssboFormats = predicted;
          }
          else
          {
            ssboFormats = predicted;
          }
        }
      }

      VkDeviceSize texelAlign = texelAlignProps.uniformTexelBufferOffsetAlignmentBytes;
      VkDeviceSize texelMax = physProperties.limits.maxTexelBufferElements;

      for(VkFormat texelFmt : {
              VK_FORMAT_R8_UNORM,
              VK_FORMAT_R16_SFLOAT,
              VK_FORMAT_R32_SFLOAT,
              VK_FORMAT_R32G32_SFLOAT,
              VK_FORMAT_R32G32B32_SFLOAT,
              VK_FORMAT_R32G32B32A32_SFLOAT,
              VK_FORMAT_R8_UINT,
              VK_FORMAT_R16_UINT,
              VK_FORMAT_R32_UINT,
              VK_FORMAT_R32G32_UINT,
              VK_FORMAT_R32G32B32_UINT,
              VK_FORMAT_R32G32B32A32_UINT,
          })
      {
        VkDeviceSize elemSize = GetElemSize(texelFmt);

        VkDeviceSize alignStep = texelAlign;

        // If the single texel alignment property is VK_TRUE, then the buffer view’s offset must be
        // aligned to the lesser of the corresponding byte alignment value or the size of a single
        // texel, based on VkBufferViewCreateInfo::format
        if(texelAlignProps.uniformTexelBufferOffsetSingleTexelAlignment)
        {
          alignStep = elemSize;

          // If the size of a single texel is a multiple of three bytes, then the size of a single
          // component of the format is used instead.
          if(elemSize == 12)
            alignStep = 4;
        }

        for(const VkDeviceSize size : {
                (VkDeviceSize)1ULL,   (VkDeviceSize)2ULL,
                (VkDeviceSize)3ULL,   (VkDeviceSize)4ULL,
                (VkDeviceSize)5ULL,   (VkDeviceSize)6ULL,
                (VkDeviceSize)7ULL,   (VkDeviceSize)8ULL,
                (VkDeviceSize)9ULL,   (VkDeviceSize)10ULL,
                (VkDeviceSize)11ULL,  (VkDeviceSize)12ULL,
                (VkDeviceSize)13ULL,  (VkDeviceSize)14ULL,
                (VkDeviceSize)15ULL,  (VkDeviceSize)16ULL,
                (VkDeviceSize)17ULL,  (VkDeviceSize)18ULL,
                (VkDeviceSize)100ULL, (VkDeviceSize)128ULL,
                (VkDeviceSize)236ULL, (VkDeviceSize)237ULL,
                (VkDeviceSize)238ULL, (VkDeviceSize)239ULL,
                (VkDeviceSize)240ULL, (VkDeviceSize)241ULL,
                (VkDeviceSize)242ULL, (VkDeviceSize)243ULL,
                (VkDeviceSize)244ULL, (VkDeviceSize)245ULL,
                (VkDeviceSize)246ULL, (VkDeviceSize)247ULL,
                (VkDeviceSize)248ULL, (VkDeviceSize)249ULL,
                (VkDeviceSize)250ULL, (VkDeviceSize)251ULL,
                (VkDeviceSize)252ULL, (VkDeviceSize)253ULL,
                (VkDeviceSize)254ULL, (VkDeviceSize)255ULL,
                (VkDeviceSize)256ULL, (VkDeviceSize)257ULL,
                (VkDeviceSize)258ULL, texelMax,
            })
        {
          for(const VkDeviceSize offset : {
                  (VkDeviceSize)0ULL, alignStep,      alignStep * 2,  alignStep * 3,
                  alignStep * 4,      alignStep * 5,  alignStep * 6,  alignStep * 7,
                  alignStep * 8,      alignStep * 9,  alignStep * 10, alignStep * 11,
                  alignStep * 12,     alignStep * 13, alignStep * 14, alignStep * 15,
                  alignStep * 16,     alignStep * 32, alignStep * 60, alignStep * 61,
                  alignStep * 62,     alignStep * 63, alignStep * 64,
              })
          {
            VkDeviceSize alignedByteSize = std::min(size * elemSize, bda_buffer_info.size - offset);

            alignedByteSize -= (alignedByteSize % elemSize);

            SizedBytes descriptor = GetDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                                  ptrs[i] + offset, alignedByteSize, texelFmt);

            std::vector<BufferDescriptorFormat> predicted;

            for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, false, ptrs[i] + offset, alignedByteSize, texelFmt);
              if(MatchPrediction(fmt, descriptor, prediction))
                predicted.push_back(fmt);
            }
            if(predicted.empty())
            {
              failedDetections++;
              TEST_WARN("!!! Couldn't detect buffer format");
              DumpData("bufferCapData", bufCapData[i]);
              TEST_LOG("Base pointer is %p", ptrs[i]);

              DumpData(fmt::format("Uniform Texel descriptor with offs {} size {} fmt {}", offset,
                                   alignedByteSize, FormatStr(texelFmt).c_str()),
                       descriptor);
              for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
              {
                SizedBytes prediction =
                    PredictDescriptor(fmt, false, ptrs[i] + offset, alignedByteSize, texelFmt);
                DumpData(fmt::format("{} prediction", name(fmt)), prediction);
              }
            }

            if(!uniTexelFormats.empty())
            {
              // ensure that at least one previously predicted format is still predicted
              bool oneKnown = false;
              for(BufferDescriptorFormat fmt : predicted)
              {
                if(std::find(uniTexelFormats.begin(), uniTexelFormats.end(), fmt) !=
                   uniTexelFormats.end())
                  oneKnown = true;
              }

              if(!oneKnown)
              {
                imperfectDetection++;
                TEST_WARN("No commonly predicted formats. Previous:");
                for(BufferDescriptorFormat fmt : uniTexelFormats)
                  TEST_WARN("%s", name(fmt).c_str());
                TEST_WARN("Predicted:");
                for(BufferDescriptorFormat fmt : predicted)
                  TEST_WARN("%s", name(fmt).c_str());
              }

              if(predicted.size() < uniTexelFormats.size())
                uniTexelFormats = predicted;
            }
            else
            {
              uniTexelFormats = predicted;
            }
          }
        }
      }

      texelAlign = texelAlignProps.storageTexelBufferOffsetAlignmentBytes;

      for(VkFormat texelFmt : {
              VK_FORMAT_R8_UNORM,
              VK_FORMAT_R16_SFLOAT,
              VK_FORMAT_R32_SFLOAT,
              VK_FORMAT_R32G32_SFLOAT,
              VK_FORMAT_R32G32B32_SFLOAT,
              VK_FORMAT_R32G32B32A32_SFLOAT,
              VK_FORMAT_R8_UINT,
              VK_FORMAT_R16_UINT,
              VK_FORMAT_R32_UINT,
              VK_FORMAT_R32G32_UINT,
              VK_FORMAT_R32G32B32_UINT,
              VK_FORMAT_R32G32B32A32_UINT,
              VK_FORMAT_R32G32B32_SINT,
          })
      {
        VkDeviceSize elemSize = GetElemSize(texelFmt);

        VkDeviceSize alignStep = texelAlign;

        // If the single texel alignment property is VK_TRUE, then the buffer view’s offset must be
        // aligned to the lesser of the corresponding byte alignment value or the size of a single
        // texel, based on VkBufferViewCreateInfo::format
        if(texelAlignProps.storageTexelBufferOffsetSingleTexelAlignment)
        {
          alignStep = elemSize;

          // If the size of a single texel is a multiple of three bytes, then the size of a single
          // component of the format is used instead.
          if(elemSize == 12)
            alignStep = 4;
        }

        for(const VkDeviceSize size : {
                (VkDeviceSize)1ULL,   (VkDeviceSize)2ULL,
                (VkDeviceSize)3ULL,   (VkDeviceSize)4ULL,
                (VkDeviceSize)5ULL,   (VkDeviceSize)6ULL,
                (VkDeviceSize)7ULL,   (VkDeviceSize)8ULL,
                (VkDeviceSize)9ULL,   (VkDeviceSize)10ULL,
                (VkDeviceSize)11ULL,  (VkDeviceSize)12ULL,
                (VkDeviceSize)13ULL,  (VkDeviceSize)14ULL,
                (VkDeviceSize)15ULL,  (VkDeviceSize)16ULL,
                (VkDeviceSize)17ULL,  (VkDeviceSize)18ULL,
                (VkDeviceSize)100ULL, (VkDeviceSize)128ULL,
                (VkDeviceSize)236ULL, (VkDeviceSize)237ULL,
                (VkDeviceSize)238ULL, (VkDeviceSize)239ULL,
                (VkDeviceSize)240ULL, (VkDeviceSize)241ULL,
                (VkDeviceSize)242ULL, (VkDeviceSize)243ULL,
                (VkDeviceSize)244ULL, (VkDeviceSize)245ULL,
                (VkDeviceSize)246ULL, (VkDeviceSize)247ULL,
                (VkDeviceSize)248ULL, (VkDeviceSize)249ULL,
                (VkDeviceSize)250ULL, (VkDeviceSize)251ULL,
                (VkDeviceSize)252ULL, (VkDeviceSize)253ULL,
                (VkDeviceSize)254ULL, (VkDeviceSize)255ULL,
                (VkDeviceSize)256ULL, (VkDeviceSize)257ULL,
                (VkDeviceSize)258ULL, texelMax,
            })
        {
          for(const VkDeviceSize offset : {
                  (VkDeviceSize)0ULL, alignStep,      alignStep * 2,  alignStep * 3,
                  alignStep * 4,      alignStep * 5,  alignStep * 6,  alignStep * 7,
                  alignStep * 8,      alignStep * 9,  alignStep * 10, alignStep * 11,
                  alignStep * 12,     alignStep * 13, alignStep * 14, alignStep * 15,
                  alignStep * 16,     alignStep * 32, alignStep * 60, alignStep * 61,
                  alignStep * 62,     alignStep * 63, alignStep * 64,
              })
          {
            VkDeviceSize alignedByteSize = std::min(size * elemSize, bda_buffer_info.size - offset);

            alignedByteSize -= (alignedByteSize % elemSize);

            SizedBytes descriptor = GetDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                                  ptrs[i] + offset, alignedByteSize, texelFmt);
            std::vector<BufferDescriptorFormat> predicted;

            for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, true, ptrs[i] + offset, alignedByteSize, texelFmt);
              if(MatchPrediction(fmt, descriptor, prediction))
                predicted.push_back(fmt);
            }
            if(predicted.empty())
            {
              failedDetections++;
              TEST_WARN("!!! Couldn't detect buffer format");
              DumpData("bufferCapData", bufCapData[i]);
              TEST_LOG("Base pointer is %p", ptrs[i]);

              DumpData(fmt::format("Storage Texel descriptor with offs {} size {} fmt {}", offset,
                                   alignedByteSize, FormatStr(texelFmt).c_str()),
                       descriptor);
              for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
              {
                SizedBytes prediction =
                    PredictDescriptor(fmt, true, ptrs[i] + offset, alignedByteSize, texelFmt);
                DumpData(fmt::format("{} prediction", name(fmt)), prediction);
              }
            }

            if(!storTexelFormats.empty())
            {
              // ensure that at least one previously predicted format is still predicted
              bool oneKnown = false;
              for(BufferDescriptorFormat fmt : predicted)
              {
                if(std::find(storTexelFormats.begin(), storTexelFormats.end(), fmt) !=
                   storTexelFormats.end())
                  oneKnown = true;
              }

              if(!oneKnown)
              {
                imperfectDetection++;
                TEST_WARN("No commonly predicted formats. Previous:");
                for(BufferDescriptorFormat fmt : storTexelFormats)
                  TEST_WARN("%s", name(fmt).c_str());
                TEST_WARN("Predicted:");
                for(BufferDescriptorFormat fmt : predicted)
                  TEST_WARN("%s", name(fmt).c_str());
              }

              if(predicted.size() < storTexelFormats.size())
                storTexelFormats = predicted;
            }
            else
            {
              storTexelFormats = predicted;
            }
          }
        }
      }

      if(blasAddr[i])
      {
        SizedBytes descriptor =
            GetDescriptor(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, blasAddr[i], blasSize);

        std::vector<BufferDescriptorFormat> predicted;

        for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
        {
          SizedBytes prediction =
              PredictDescriptor(fmt, true, blasAddr[i], blasSize, VK_FORMAT_UNDEFINED);
          if(MatchPrediction(fmt, descriptor, prediction))
            predicted.push_back(fmt);
        }
        if(predicted.empty())
        {
          failedDetections++;
          TEST_WARN("!!! Couldn't detect buffer format");
          DumpData("blasCapData", blasCapData[i]);
          TEST_LOG("Base pointer is %p", blasAddr[i]);
          DumpData("asDescriptor", descriptor);
          for(BufferDescriptorFormat fmt : enumerate<BufferDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, true, blasAddr[i], blasSize, VK_FORMAT_UNDEFINED);
            DumpData(fmt::format("{} prediction", name(fmt)), prediction);
          }
        }

        if(!asFormats.empty())
        {
          // ensure that at least one previously predicted format is still predicted
          bool oneKnown = false;
          for(BufferDescriptorFormat fmt : predicted)
          {
            if(std::find(asFormats.begin(), asFormats.end(), fmt) != asFormats.end())
              oneKnown = true;
          }

          if(!oneKnown)
          {
            imperfectDetection++;
            TEST_WARN("No commonly predicted formats. Previous:");
            for(BufferDescriptorFormat fmt : asFormats)
              TEST_WARN("%s", name(fmt).c_str());
            TEST_WARN("Predicted:");
            for(BufferDescriptorFormat fmt : predicted)
              TEST_WARN("%s", name(fmt).c_str());
          }

          if(predicted.size() < asFormats.size())
            asFormats = predicted;
        }
        else
        {
          asFormats = predicted;
        }
      }
    }

    for(uint32_t i = 0; i < memProps->memoryTypeCount; i++)
    {
      if(images[i].empty())
        continue;

      for(uint32_t s = 0; s < ARRAY_COUNT(samps); s++)
      {
        SizedBytes descriptor =
            GetDescriptor(VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, samps[s], images[i][0].view);

        VkDeviceAddress baseAddr = ptrs[i] + images[i][0].offset;

        bool matched = false;

        for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
        {
          SizedBytes prediction = PredictDescriptor(
              fmt, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, baseAddr, samplerCapData[s],
              images[i][0].imgCapData, images[i][0].viewCapData);
          if(MatchPrediction(fmt, descriptor, prediction))
          {
            matched = true;
            if(combined == SamplerDescriptorFormat::Count)
              combined = fmt;
            if(combined != fmt)
            {
              imperfectDetection++;
              TEST_WARN("Duplicate/inconsistent format detection between %s and %s",
                        name(combined).c_str(), name(fmt).c_str());
            }
          }
        }

        if(!matched)
        {
          TEST_WARN("Couldn't match descriptor");

          DumpData(images[i][0].name + " imgCapData", images[i][0].imgCapData);
          DumpData(images[i][0].name + " viewCapData", images[i][0].viewCapData);
          DumpData(fmt::format("samplerCapData {}", s), samplerCapData[s]);
          DumpData(fmt::format("{} + samp{} combined descriptor", images[i][0].name, s), descriptor);

          for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
          {
            SizedBytes prediction = PredictDescriptor(
                fmt, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, baseAddr, samplerCapData[s],
                images[i][0].imgCapData, images[i][0].viewCapData);
            DumpData(fmt::format("{} prediction", name(fmt)), prediction);
          }
        }
      }

      for(uint32_t v = 0; v < images[i].size(); v++)
      {
        const Image &img = images[i][v];

        VkDeviceAddress baseAddr = ptrs[i] + img.offset;

        if(img.info.usage & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
          bool matched = false;

          SizedBytes descriptor = GetDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, img.view);

          for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, baseAddr, SizedBytes(),
                                  img.imgCapData, img.viewCapData);
            if(MatchPrediction(fmt, descriptor, prediction))
            {
              matched = true;
              if(sampled == SamplerDescriptorFormat::Count)
                sampled = fmt;
              if(sampled != fmt)
              {
                imperfectDetection++;
                TEST_WARN("Duplicate/inconsistent format detection between %s and %s",
                          name(sampled).c_str(), name(fmt).c_str());
              }
            }
          }

          if(!matched)
          {
            TEST_WARN("Couldn't match descriptor");

            TEST_LOG("at %llu offset image pointer range is 0x%016llx-0x%016llx", img.offset,
                     baseAddr, baseAddr + img.size);
            DumpData(img.name + " imgCapData", img.imgCapData);
            DumpData(img.name + " viewCapData", img.viewCapData);
            DumpData(img.name + " sampled descriptor", descriptor);

            for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, baseAddr, SizedBytes(),
                                    img.imgCapData, img.viewCapData);
              DumpData(fmt::format("{} sampled prediction", name(fmt)), prediction);
            }
          }
        }

        if(img.info.usage & VK_IMAGE_USAGE_STORAGE_BIT)
        {
          bool matched = false;

          SizedBytes descriptor = GetDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, img.view);

          for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, baseAddr, SizedBytes(),
                                  img.imgCapData, img.viewCapData);
            if(MatchPrediction(fmt, descriptor, prediction))
            {
              matched = true;
              if(storage == SamplerDescriptorFormat::Count)
                storage = fmt;
              if(storage != fmt)
              {
                imperfectDetection++;
                TEST_WARN("Duplicate/inconsistent format detection between %s and %s",
                          name(storage).c_str(), name(fmt).c_str());
              }
            }
          }

          if(!matched)
          {
            TEST_WARN("Couldn't match descriptor");

            TEST_LOG("at %llu offset image pointer range is 0x%016llx-0x%016llx", img.offset,
                     baseAddr, baseAddr + img.size);
            DumpData(img.name + " imgCapData", img.imgCapData);
            DumpData(img.name + " viewCapData", img.viewCapData);
            DumpData(img.name + " storage descriptor", descriptor);

            for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, baseAddr, SizedBytes(),
                                    img.imgCapData, img.viewCapData);
              DumpData(fmt::format("{} storage prediction", name(fmt)), prediction);
            }
          }
        }

        if(img.info.usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        {
          bool matched = false;

          SizedBytes descriptor = GetDescriptor(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                                VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, img.view);

          for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, baseAddr, SizedBytes(),
                                  img.imgCapData, img.viewCapData);
            if(MatchPrediction(fmt, descriptor, prediction))
            {
              matched = true;
              if(input == SamplerDescriptorFormat::Count)
                input = fmt;
              if(input != fmt)
              {
                imperfectDetection++;
                TEST_WARN("Duplicate/inconsistent format detection between %s and %s",
                          name(input).c_str(), name(fmt).c_str());
              }
            }
          }

          if(!matched)
          {
            TEST_WARN("Couldn't match descriptor");

            TEST_LOG("at %llu offset image pointer range is 0x%016llx-0x%016llx", img.offset,
                     baseAddr, baseAddr + img.size);
            DumpData(img.name + " imgCapData", img.imgCapData);
            DumpData(img.name + " viewCapData", img.viewCapData);
            DumpData(img.name + " input descriptor", descriptor);

            for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
            {
              SizedBytes prediction =
                  PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, baseAddr,
                                    SizedBytes(), img.imgCapData, img.viewCapData);
              DumpData(fmt::format("{} input prediction", name(fmt)), prediction);
            }
          }
        }
      }
    }

    if(sparseImage.img != VK_NULL_HANDLE)
    {
      const Image &img = sparseImage;

      VkDeviceAddress baseAddr = 0xdeadbeefdeadbeefULL;

      if(img.info.usage & VK_IMAGE_USAGE_SAMPLED_BIT)
      {
        bool matched = false;

        SizedBytes descriptor = GetDescriptor(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                              VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, img.view);

        for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
        {
          SizedBytes prediction = PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, baseAddr,
                                                    SizedBytes(), img.imgCapData, img.viewCapData);
          if(MatchPrediction(fmt, descriptor, prediction))
          {
            matched = true;
            if(sampled == SamplerDescriptorFormat::Count)
              sampled = fmt;
            if(sampled != fmt)
            {
              imperfectDetection++;
              TEST_WARN("Duplicate/inconsistent format detection between %s and %s",
                        name(sampled).c_str(), name(fmt).c_str());
            }
          }
        }

        // if(!matched)
        {
          TEST_WARN("Couldn't match descriptor");

          TEST_LOG("at %llu offset image pointer range is 0x%016llx-0x%016llx", img.offset,
                   baseAddr, baseAddr + img.size);
          DumpData(img.name + " imgCapData", img.imgCapData);
          DumpData(img.name + " viewCapData", img.viewCapData);
          DumpData(img.name + " sampled descriptor", descriptor);

          for(SamplerDescriptorFormat fmt : enumerate<SamplerDescriptorFormat>())
          {
            SizedBytes prediction =
                PredictDescriptor(fmt, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, baseAddr, SizedBytes(),
                                  img.imgCapData, img.viewCapData);
            DumpData(fmt::format("{} sampled prediction", name(fmt)), prediction);
          }
        }
      }
    }

    TEST_LOG("=============================================");
    TEST_LOG("===============    Summary    ===============");
    TEST_LOG("");
    TEST_LOG("%s", physProperties.deviceName);
    TEST_LOG("");
    TEST_LOG("%u failed detections", failedDetections);
    TEST_LOG("%u imperfect detections", imperfectDetection);
    TEST_LOG("");
    TEST_LOG("Buffers:");
    TEST_LOG(" UBO (%u): %s", (uint32_t)uboFormats.size(),
             uboFormats.empty() ? "-" : name(uboFormats[0]).c_str());
    TEST_LOG(" SSBO (%u): %s", (uint32_t)ssboFormats.size(),
             ssboFormats.empty() ? "-" : name(ssboFormats[0]).c_str());
    TEST_LOG(" UniTexel (%u): %s", (uint32_t)uniTexelFormats.size(),
             uniTexelFormats.empty() ? "-" : name(uniTexelFormats[0]).c_str());
    TEST_LOG(" StorTexel (%u): %s", (uint32_t)storTexelFormats.size(),
             storTexelFormats.empty() ? "-" : name(storTexelFormats[0]).c_str());
    TEST_LOG("");
    TEST_LOG("AS (%u): %s", (uint32_t)asFormats.size(),
             asFormats.empty() ? "-" : name(asFormats[0]).c_str());
    TEST_LOG("");
    TEST_LOG("Images:");
    TEST_LOG(" Sampler (%u): %s", (uint32_t)samplerFormats.size(),
             samplerFormats.empty() ? "-" : name(samplerFormats[0]).c_str());
    TEST_LOG(" Combined: %s", name(combined).c_str());
    TEST_LOG(" Sampled: %s", name(sampled).c_str());
    TEST_LOG(" Storage: %s", name(storage).c_str());
    TEST_LOG(" Input: %s", name(input).c_str());

    if(sparseBuffer != VK_NULL_HANDLE)
      vkDestroyBuffer(device, sparseBuffer, NULL);
    if(sparseImage.img != VK_NULL_HANDLE)
      vkDestroyImage(device, sparseImage.img, NULL);

    for(uint32_t i = 0; i < memProps->memoryTypeCount; i++)
    {
      if(memory[i] != VK_NULL_HANDLE)
        vkFreeMemory(device, memory[i], NULL);

      if(buffer[i] != VK_NULL_HANDLE)
        vkDestroyBuffer(device, buffer[i], NULL);
      if(blas[i] != VK_NULL_HANDLE)
        vkDestroyAccelerationStructureKHR(device, blas[i], NULL);

      for(const Image &img : images[i])
        if(img.img != VK_NULL_HANDLE)
          vkDestroyImage(device, img.img, NULL);
    }

    return 0;
  }
};

REGISTER_TEST();
