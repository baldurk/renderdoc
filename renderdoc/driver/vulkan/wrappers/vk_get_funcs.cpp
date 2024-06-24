/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "../vk_debug.h"
#include "api/replay/version.h"

static char fakeRenderDocUUID[VK_UUID_SIZE] = {};

void MakeFakeUUID()
{
  // assign a fake UUID, so that we get SPIR-V instead of cached pipeline data.
  // the start is "rdoc", and the end is the time that this call was first made
  if(fakeRenderDocUUID[0] == 0)
  {
    // 0123456789ABCDEF
    // rdocyymmddHHMMSS
    // we pass size+1 so that there's room for a null terminator (the UUID doesn't
    // need a null terminator as it's a fixed size non-string array)
    rdcstr uuid = StringFormat::sntimef(Timing::GetUTCTime(), "rdoc%y%m%d%H%M%S");
    RDCASSERT(uuid.size() == sizeof(fakeRenderDocUUID));
    memcpy(fakeRenderDocUUID, uuid.c_str(), RDCMIN((size_t)VK_UUID_SIZE, uuid.size()));
  }
}

void ClampPhysDevAPIVersion(VkPhysicalDeviceProperties *pProperties, VkPhysicalDevice physicalDevice)
{
  // for Vulkan 1.3 bufferDeviceAddress is core. If the bufferDeviceAddressCaptureReplay feature is
  // not available, we can't support it so we must clamp to version 1.2 for that physical device.
  if(pProperties->apiVersion >= VK_API_VERSION_1_3)
  {
    // for 1.1 this is core so we should definitely have this function.
    if(ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2 != NULL)
    {
      VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

      // similarly this struct must be valid if the device is 1.3
      VkPhysicalDeviceVulkan12Features vk12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};

      features.pNext = &vk12;

      ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), &features);

      if(vk12.bufferDeviceAddressCaptureReplay == VK_FALSE)
      {
        RDCWARN(
            "Vulkan feature bufferDeviceAddressCaptureReplay is not available. Clamping physical "
            "device %s from reported version %d.%d to 1.2",
            pProperties->deviceName, VK_VERSION_MAJOR(pProperties->apiVersion),
            VK_VERSION_MINOR(pProperties->apiVersion));

        pProperties->apiVersion = VK_API_VERSION_1_2;
      }
    }
    else
    {
      // if we don't have GPDP2 the application has not initialised the instance at 1.3+
      // let's clamp the version just to be safe since we can't check, and this will help protect
      // against buggy applications
      pProperties->apiVersion = VK_API_VERSION_1_2;
    }
  }
}

void WrappedVulkan::vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                                                VkPhysicalDeviceFeatures *pFeatures)
{
  ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), pFeatures);
}

void WrappedVulkan::vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                                        VkFormat format,
                                                        VkFormatProperties *pFormatProperties)
{
  ObjDisp(physicalDevice)
      ->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), format, pFormatProperties);

  // we require all these properties at minimum for an image to be created, since we add these to
  // any usage. Fortunately, in the formats the spec requires an implementation to support,
  // optimalTiledFeatures must contain all these and more, so we can safely remove support for any
  // format that only includes a subset.
  uint32_t minRequiredMask = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

  const InstanceDeviceInfo &exts = GetExtensions(GetRecord(physicalDevice));

  // transfer src/dst bits were added in KHR_maintenance1. Before then we assume that if
  // SAMPLED_IMAGE_BIT was present it's safe to add the transfer bits too.
  if(exts.ext_KHR_maintenance1)
    minRequiredMask |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

  if((pFormatProperties->linearTilingFeatures & minRequiredMask) != minRequiredMask)
    pFormatProperties->linearTilingFeatures = 0;
  if((pFormatProperties->optimalTilingFeatures & minRequiredMask) != minRequiredMask)
    pFormatProperties->optimalTilingFeatures = 0;

  // don't report support for DISJOINT_BIT_KHR binding
  pFormatProperties->linearTilingFeatures &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;
  pFormatProperties->optimalTilingFeatures &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;
}

void WrappedVulkan::vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice,
                                                         VkFormat format,
                                                         VkFormatProperties2 *pFormatProperties)
{
  ObjDisp(physicalDevice)
      ->GetPhysicalDeviceFormatProperties2(Unwrap(physicalDevice), format, pFormatProperties);

  // we require transfer source and dest these properties at minimum for an image to be created,
  // since we add these to
  // any usage. Fortunately, in the formats the spec requires an implementation to support,
  // optimalTiledFeatures must contain all these and more, so we can safely remove support for any
  // format that only includes a subset.
  uint32_t minRequiredMask = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

  const InstanceDeviceInfo &exts = GetExtensions(GetRecord(physicalDevice));

  // transfer src/dst bits were added in KHR_maintenance1. Before then we assume that if
  // SAMPLED_IMAGE_BIT was present it's safe to add the transfer bits too.
  if(exts.ext_KHR_maintenance1)
    minRequiredMask |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

  if((pFormatProperties->formatProperties.linearTilingFeatures & minRequiredMask) != minRequiredMask)
    pFormatProperties->formatProperties.linearTilingFeatures = 0;
  if((pFormatProperties->formatProperties.optimalTilingFeatures & minRequiredMask) != minRequiredMask)
    pFormatProperties->formatProperties.optimalTilingFeatures = 0;

  // don't report support for DISJOINT_BIT_KHR binding
  pFormatProperties->formatProperties.linearTilingFeatures &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;
  pFormatProperties->formatProperties.optimalTilingFeatures &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkImageFormatProperties *pImageFormatProperties)
{
  // we're going to add these usage bits implicitly on image create, so ensure we get an accurate
  // response by adding them here. It's OK to add these, since these can't make a required format
  // suddenly report as unsupported (all required formats must support these usages), so it can only
  // make an optional format unsupported which is what we want.
  usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
           VK_IMAGE_USAGE_SAMPLED_BIT;

  VkResult vkr =
      ObjDisp(physicalDevice)
          ->GetPhysicalDeviceImageFormatProperties(Unwrap(physicalDevice), format, type, tiling,
                                                   usage, flags, pImageFormatProperties);

  if(vkr == VK_SUCCESS)
  {
    // check that the format is one we allow to be supported - if not we return an error to be
    // consistent.
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    if(props.linearTilingFeatures == 0 && props.optimalTilingFeatures == 0)
    {
      RDCEraseEl(*pImageFormatProperties);
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
  }

  return vkr;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo,
    VkImageFormatProperties2 *pImageFormatProperties)
{
  // we're going to add these usage bits implicitly on image create, so ensure we get an accurate
  // response by adding them here. It's OK to add these, since these can't make a required format
  // suddenly report as unsupported (all required formats must support these usages), so it can only
  // make an optional format unsupported which is what we want.
  VkPhysicalDeviceImageFormatInfo2 info = *pImageFormatInfo;
  info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT;

  VkResult vkr = ObjDisp(physicalDevice)
                     ->GetPhysicalDeviceImageFormatProperties2(Unwrap(physicalDevice), &info,
                                                               pImageFormatProperties);

  if(vkr == VK_SUCCESS)
  {
    // check that the format is one we allow to be supported - if not we return an error to be
    // consistent.
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, pImageFormatInfo->format, &props);
    if(props.linearTilingFeatures == 0 && props.optimalTilingFeatures == 0)
    {
      RDCEraseEl(pImageFormatProperties->imageFormatProperties);
      return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
  }

  return vkr;
}

void WrappedVulkan::vkGetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
    VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling,
    uint32_t *pPropertyCount, VkSparseImageFormatProperties *pProperties)
{
  ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSparseImageFormatProperties(Unwrap(physicalDevice), format, type, samples,
                                                     usage, tiling, pPropertyCount, pProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                  VkPhysicalDeviceProperties *pProperties)
{
  ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), pProperties);

  MakeFakeUUID();

  memcpy(pProperties->pipelineCacheUUID, fakeRenderDocUUID, VK_UUID_SIZE);

  ClampPhysDevAPIVersion(pProperties, physicalDevice);
}

void WrappedVulkan::vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice, uint32_t *pCount, VkQueueFamilyProperties *pQueueFamilyProperties)
{
  // report the actual physical device properties - this will be remapped on replay if necessary
  ObjDisp(physicalDevice)
      ->GetPhysicalDeviceQueueFamilyProperties(Unwrap(physicalDevice), pCount,
                                               pQueueFamilyProperties);

  // remove any protected bits that might be set
  if(pCount && pQueueFamilyProperties)
  {
    for(uint32_t i = 0; i < *pCount; i++)
      pQueueFamilyProperties[i].queueFlags &= ~VK_QUEUE_PROTECTED_BIT;
  }
}

void WrappedVulkan::vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
  ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), pMemoryProperties);
}

void WrappedVulkan::vkGetImageSubresourceLayout(VkDevice device, VkImage image,
                                                const VkImageSubresource *pSubresource,
                                                VkSubresourceLayout *pLayout)
{
  ObjDisp(device)->GetImageSubresourceLayout(Unwrap(device), Unwrap(image), pSubresource, pLayout);
}

void WrappedVulkan::vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                                  VkMemoryRequirements *pMemoryRequirements)
{
  // if we have cached memory requirements, use them. These were fetched at create time (which is
  // still valid, they don't change over the lifetime of the resource) and may be slightly more
  // pessimistic for the case of external memory bound resources. See vkCreateBuffer/vkCreateImage
  if(IsCaptureMode(m_State) && GetRecord(buffer)->resInfo)
    *pMemoryRequirements = GetRecord(buffer)->resInfo->memreqs;
  else
    ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), pMemoryRequirements);
}

void WrappedVulkan::vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                                 VkMemoryRequirements *pMemoryRequirements)
{
  // if we have cached memory requirements, use them. These were fetched at create time (which is
  // still valid, they don't change over the lifetime of the resource) and may be slightly more
  // pessimistic for the case of external memory bound resources. See vkCreateBuffer/vkCreateImage
  if(IsCaptureMode(m_State) && GetRecord(image)->resInfo)
    *pMemoryRequirements = GetRecord(image)->resInfo->memreqs;
  else
    ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), pMemoryRequirements);

  // AMD can have some variability in the returned size, so we need to pad the reported size to
  // allow for this. The variability isn't quite clear, but for now we assume aligning size to
  // alignment * 4 should be sufficient (adding on a fixed padding won't help the problem as it
  // won't remove the variability, nor will adding then aligning for the same reason).
  if(GetDriverInfo().AMDUnreliableImageMemoryRequirements() && pMemoryRequirements->size > 0)
  {
    VkMemoryRequirements &memreq = *pMemoryRequirements;

    VkDeviceSize oldsize = memreq.size;
    memreq.size = AlignUp(memreq.size, memreq.alignment * 4);

    // if it's already 'super aligned', then bump it up a little. We assume that this case
    // represents the low-end of the variation range, and other variations will be a little higher.
    // The other alternative is the variations are all lower and this one happened to be super
    // aligned, which I think (arbitrarily really) is less likely.
    if(oldsize == memreq.size)
      memreq.size = AlignUp(memreq.size + 1, memreq.alignment * 4);

    RDCDEBUG(
        "Padded image memory requirements from %llu to %llu (base alignment %llu) (%f%% increase)",
        oldsize, memreq.size, memreq.alignment,
        (100.0 * double(memreq.size - oldsize)) / double(oldsize));
  }
}

void WrappedVulkan::vkGetImageSparseMemoryRequirements(
    VkDevice device, VkImage image, uint32_t *pNumRequirements,
    VkSparseImageMemoryRequirements *pSparseMemoryRequirements)
{
  ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(image), pNumRequirements,
                                                    pSparseMemoryRequirements);
}

void WrappedVulkan::vkGetDeviceBufferMemoryRequirements(VkDevice device,
                                                        const VkDeviceBufferMemoryRequirements *pInfo,
                                                        VkMemoryRequirements2 *pMemoryRequirements)
{
  byte *tempMem = GetTempMemory(GetNextPatchSize(pInfo));
  VkDeviceBufferMemoryRequirements *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pInfo);

  VkBufferCreateInfo *info = (VkBufferCreateInfo *)unwrappedInfo->pCreateInfo;

  // patch the create info the same as we would for vkCreateBuffer
  info->usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  info->usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if(IsCaptureMode(m_State) && (info->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
    info->flags |= VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;

  ObjDisp(device)->GetDeviceBufferMemoryRequirements(Unwrap(device), unwrappedInfo,
                                                     pMemoryRequirements);

  // if the buffer is external, create a non-external and return the worst case memory requirements
  // so that the memory allocated is sufficient for us on replay when the buffer is non-external
  bool isExternal = FindNextStruct(unwrappedInfo->pCreateInfo,
                                   VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO) != NULL;

  if(isExternal)
  {
    bool removed =
        RemoveNextStruct(unwrappedInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO);

    RDCASSERTMSG("Couldn't find next struct indicating external memory", removed);

    VkMemoryRequirements2 nonExternalReq = {};
    nonExternalReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

    ObjDisp(device)->GetDeviceBufferMemoryRequirements(Unwrap(device), unwrappedInfo,
                                                       &nonExternalReq);

    pMemoryRequirements->memoryRequirements.size =
        RDCMAX(pMemoryRequirements->memoryRequirements.size, nonExternalReq.memoryRequirements.size);
    pMemoryRequirements->memoryRequirements.alignment =
        RDCMAX(pMemoryRequirements->memoryRequirements.alignment,
               nonExternalReq.memoryRequirements.alignment);

    if((pMemoryRequirements->memoryRequirements.memoryTypeBits &
        nonExternalReq.memoryRequirements.memoryTypeBits) == 0)
    {
      RDCWARN(
          "External buffer shares no memory types with non-external buffer. This buffer "
          "will not be replayable.");
    }
    else
    {
      pMemoryRequirements->memoryRequirements.memoryTypeBits &=
          nonExternalReq.memoryRequirements.memoryTypeBits;
    }
  }
}

void WrappedVulkan::vkGetDeviceImageMemoryRequirements(VkDevice device,
                                                       const VkDeviceImageMemoryRequirements *pInfo,
                                                       VkMemoryRequirements2 *pMemoryRequirements)
{
  size_t tempMemSize = GetNextPatchSize(pInfo);

  // reserve space for a patched view format list if necessary
  if(pInfo->pCreateInfo->samples != VK_SAMPLE_COUNT_1_BIT)
  {
    const VkImageFormatListCreateInfo *formatListInfo =
        (const VkImageFormatListCreateInfo *)FindNextStruct(
            pInfo->pCreateInfo, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);

    if(formatListInfo)
      tempMemSize += sizeof(VkFormat) * (formatListInfo->viewFormatCount + 1);
  }

  byte *tempMem = GetTempMemory(tempMemSize);
  VkDeviceImageMemoryRequirements *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pInfo);

  VkImageCreateInfo *info = (VkImageCreateInfo *)unwrappedInfo->pCreateInfo;

  info->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  if(IsCaptureMode(m_State))
  {
    info->usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info->usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }

  if(IsYUVFormat(info->format))
    info->flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

  if(info->samples != VK_SAMPLE_COUNT_1_BIT)
  {
    info->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    info->flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    if(IsCaptureMode(m_State))
    {
      if(!IsDepthOrStencilFormat(info->format))
      {
        if(GetDebugManager() && GetShaderCache()->IsBuffer2MSSupported())
          info->usage |= VK_IMAGE_USAGE_STORAGE_BIT;
      }
      else
      {
        info->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }
  }

  info->flags &= ~VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;

  VkImageStencilUsageCreateInfo *separateStencilUsage =
      (VkImageStencilUsageCreateInfo *)FindNextStruct(
          info, VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO);
  if(separateStencilUsage)
  {
    separateStencilUsage->stencilUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if(IsCaptureMode(m_State))
    {
      info->usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      info->usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }

    if(info->samples != VK_SAMPLE_COUNT_1_BIT)
    {
      separateStencilUsage->stencilUsage |=
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
  }

  // similarly for the image format list for MSAA textures, add the UINT cast format we will need
  if(info->samples != VK_SAMPLE_COUNT_1_BIT)
  {
    VkImageFormatListCreateInfo *formatListInfo = (VkImageFormatListCreateInfo *)FindNextStruct(
        info, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);

    if(formatListInfo)
    {
      uint32_t bs = (uint32_t)GetByteSize(1, 1, 1, info->format, 0);

      VkFormat msaaCopyFormat = VK_FORMAT_UNDEFINED;
      if(bs == 1)
        msaaCopyFormat = VK_FORMAT_R8_UINT;
      else if(bs == 2)
        msaaCopyFormat = VK_FORMAT_R16_UINT;
      else if(bs == 4)
        msaaCopyFormat = VK_FORMAT_R32_UINT;
      else if(bs == 8)
        msaaCopyFormat = VK_FORMAT_R32G32_UINT;
      else if(bs == 16)
        msaaCopyFormat = VK_FORMAT_R32G32B32A32_UINT;

      const VkFormat *oldFmts = formatListInfo->pViewFormats;
      VkFormat *newFmts = (VkFormat *)tempMem;
      formatListInfo->pViewFormats = newFmts;

      bool needAdded = true;
      uint32_t i = 0;
      for(; i < formatListInfo->viewFormatCount; i++)
      {
        newFmts[i] = oldFmts[i];
        if(newFmts[i] == msaaCopyFormat)
          needAdded = false;
      }

      if(needAdded)
      {
        newFmts[i] = msaaCopyFormat;
        formatListInfo->viewFormatCount++;
      }
    }
  }

  ObjDisp(device)->GetDeviceImageMemoryRequirements(Unwrap(device), unwrappedInfo,
                                                    pMemoryRequirements);

  // if the image is external, create a non-external and return the worst case memory requirements
  // so that the memory allocated is sufficient for us on replay when the image is non-external
  bool isExternal = FindNextStruct(unwrappedInfo->pCreateInfo,
                                   VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO) != NULL;

  if(isExternal)
  {
    bool removed =
        RemoveNextStruct(unwrappedInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);

    RDCASSERTMSG("Couldn't find next struct indicating external memory", removed);

    VkMemoryRequirements2 nonExternalReq = {};
    nonExternalReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

    ObjDisp(device)->GetDeviceImageMemoryRequirements(Unwrap(device), unwrappedInfo, &nonExternalReq);

    pMemoryRequirements->memoryRequirements.size =
        RDCMAX(pMemoryRequirements->memoryRequirements.size, nonExternalReq.memoryRequirements.size);
    pMemoryRequirements->memoryRequirements.alignment =
        RDCMAX(pMemoryRequirements->memoryRequirements.alignment,
               nonExternalReq.memoryRequirements.alignment);

    if((pMemoryRequirements->memoryRequirements.memoryTypeBits &
        nonExternalReq.memoryRequirements.memoryTypeBits) == 0)
    {
      RDCWARN(
          "External image shares no memory types with non-external image. This image "
          "will not be replayable.");
    }
    else
    {
      pMemoryRequirements->memoryRequirements.memoryTypeBits &=
          nonExternalReq.memoryRequirements.memoryTypeBits;
    }
  }
}

void WrappedVulkan::vkGetDeviceImageSparseMemoryRequirements(
    VkDevice device, const VkDeviceImageMemoryRequirements *pInfo,
    uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
  byte *tempMem = GetTempMemory(GetNextPatchSize(pInfo));
  VkDeviceImageMemoryRequirements *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pInfo);

  ObjDisp(device)->GetDeviceImageSparseMemoryRequirements(
      Unwrap(device), unwrappedInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

void WrappedVulkan::vkGetBufferMemoryRequirements2(VkDevice device,
                                                   const VkBufferMemoryRequirementsInfo2 *pInfo,
                                                   VkMemoryRequirements2 *pMemoryRequirements)
{
  VkBufferMemoryRequirementsInfo2 unwrappedInfo = *pInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  ObjDisp(device)->GetBufferMemoryRequirements2(Unwrap(device), &unwrappedInfo, pMemoryRequirements);

  // if we have cached memory requirements, use them. These were fetched at create time (which is
  // still valid, they don't change over the lifetime of the resource) and may be slightly more
  // pessimistic for the case of external memory bound resources. See vkCreateBuffer/vkCreateImage
  if(IsCaptureMode(m_State) && GetRecord(pInfo->buffer)->resInfo)
    pMemoryRequirements->memoryRequirements = GetRecord(pInfo->buffer)->resInfo->memreqs;
}

void WrappedVulkan::vkGetImageMemoryRequirements2(VkDevice device,
                                                  const VkImageMemoryRequirementsInfo2 *pInfo,
                                                  VkMemoryRequirements2 *pMemoryRequirements)
{
  VkImageMemoryRequirementsInfo2 unwrappedInfo = *pInfo;
  unwrappedInfo.image = Unwrap(unwrappedInfo.image);
  ObjDisp(device)->GetImageMemoryRequirements2(Unwrap(device), &unwrappedInfo, pMemoryRequirements);

  // if we have cached memory requirements, use them. These were fetched at create time (which is
  // still valid, they don't change over the lifetime of the resource) and may be slightly more
  // pessimistic for the case of external memory bound resources. See vkCreateBuffer/vkCreateImage
  if(IsCaptureMode(m_State) && GetRecord(pInfo->image)->resInfo)
    pMemoryRequirements->memoryRequirements = GetRecord(pInfo->image)->resInfo->memreqs;

  // don't do remapping here on replay.
  if(IsReplayMode(m_State))
    return;

  // AMD can have some variability in the returned size, so we need to pad the reported size to
  // allow for this. The variability isn't quite clear, but for now we assume aligning size to
  // alignment * 4 should be sufficient (adding on a fixed padding won't help the problem as it
  // won't remove the variability, nor will adding then aligning for the same reason).
  if(GetDriverInfo().AMDUnreliableImageMemoryRequirements() &&
     pMemoryRequirements->memoryRequirements.size > 0)
  {
    VkMemoryRequirements &memreq = pMemoryRequirements->memoryRequirements;

    VkDeviceSize oldsize = memreq.size;
    memreq.size = AlignUp(memreq.size, memreq.alignment * 4);

    // if it's already 'super aligned', then bump it up a little. We assume that this case
    // represents the low-end of the variation range, and other variations will be a little higher.
    // The other alternative is the variations are all lower and this one happened to be super
    // aligned, which I think (arbitrarily really) is less likely.
    if(oldsize == memreq.size)
      memreq.size = AlignUp(memreq.size + 1, memreq.alignment * 4);

    RDCDEBUG(
        "Padded image memory requirements from %llu to %llu (base alignment %llu) (%f%% increase)",
        oldsize, memreq.size, memreq.alignment,
        (100.0 * double(memreq.size - oldsize)) / double(oldsize));
  }
}

void WrappedVulkan::vkGetImageSparseMemoryRequirements2(
    VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo,
    uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements)
{
  VkImageSparseMemoryRequirementsInfo2 unwrappedInfo = *pInfo;
  unwrappedInfo.image = Unwrap(unwrappedInfo.image);
  ObjDisp(device)->GetImageSparseMemoryRequirements2(
      Unwrap(device), &unwrappedInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

void WrappedVulkan::vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory,
                                                VkDeviceSize *pCommittedMemoryInBytes)
{
  ObjDisp(device)->GetDeviceMemoryCommitment(Unwrap(device), Unwrap(memory), pCommittedMemoryInBytes);
}

void WrappedVulkan::vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass,
                                               VkExtent2D *pGranularity)
{
  return ObjDisp(device)->GetRenderAreaGranularity(Unwrap(device), Unwrap(renderPass), pGranularity);
}

VkResult WrappedVulkan::vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache,
                                               size_t *pDataSize, void *pData)
{
  // required header and 4 NULL bytes
  size_t totalSize = sizeof(VkPipelineCacheHeaderVersionOne) + 4;

  if(pDataSize && !pData)
    *pDataSize = totalSize;

  if(pDataSize && pData)
  {
    if(*pDataSize < totalSize)
    {
      memset(pData, 0, *pDataSize);
      return VK_INCOMPLETE;
    }

    VkPipelineCacheHeaderVersionOne *header = (VkPipelineCacheHeaderVersionOne *)pData;

    RDCCOMPILE_ASSERT(sizeof(VkPipelineCacheHeaderVersionOne) == 16 + VK_UUID_SIZE,
                      "Pipeline cache header size is wrong");

    header->headerSize = sizeof(VkPipelineCacheHeaderVersionOne);
    header->headerVersion = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
    // just in case the user expects a valid vendorID/deviceID, write the real one
    // MULTIDEVICE need to get the right physical device for this device
    header->vendorID = m_PhysicalDeviceData.props.vendorID;
    header->deviceID = m_PhysicalDeviceData.props.deviceID;

    MakeFakeUUID();

    memcpy(header->pipelineCacheUUID, fakeRenderDocUUID, VK_UUID_SIZE);

    RDCCOMPILE_ASSERT(VK_UUID_SIZE == 16, "VK_UUID_SIZE has changed");

    // empty bytes
    uint32_t *ptr = (uint32_t *)(header + 1);
    *ptr = 0;
  }

  // we don't want the application to use pipeline caches at all, and especially
  // don't want to return any data for future use. We thus return a technically
  // valid but empty pipeline cache. Our UUID changes every run so in theory the
  // application should never provide an old cache, but just in case we will nop
  // it out in create pipeline cache
  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkMergePipelineCaches(VkDevice device, VkPipelineCache destCache,
                                              uint32_t srcCacheCount,
                                              const VkPipelineCache *pSrcCaches)
{
  // do nothing, our pipeline caches are always dummies
  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceExternalImageFormatPropertiesNV(
    VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkExternalMemoryHandleTypeFlagsNV externalHandleType,
    VkExternalImageFormatPropertiesNV *pExternalImageFormatProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceExternalImageFormatPropertiesNV(Unwrap(physicalDevice), format, type,
                                                         tiling, usage, flags, externalHandleType,
                                                         pExternalImageFormatProperties);
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)
VkResult WrappedVulkan::vkGetMemoryWin32HandleNV(VkDevice device, VkDeviceMemory memory,
                                                 VkExternalMemoryHandleTypeFlagsNV handleType,
                                                 HANDLE *pHandle)
{
  return ObjDisp(device)->GetMemoryWin32HandleNV(Unwrap(device), Unwrap(memory), handleType, pHandle);
}

VkResult WrappedVulkan::vkGetMemoryWin32HandleKHR(
    VkDevice device, const VkMemoryGetWin32HandleInfoKHR *pGetWin32HandleInfo, HANDLE *pHandle)
{
  VkMemoryGetWin32HandleInfoKHR unwrappedInfo = *pGetWin32HandleInfo;
  unwrappedInfo.memory = Unwrap(unwrappedInfo.memory);
  return ObjDisp(device)->GetMemoryWin32HandleKHR(Unwrap(device), &unwrappedInfo, pHandle);
}

VkResult WrappedVulkan::vkGetMemoryWin32HandlePropertiesKHR(
    VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, HANDLE handle,
    VkMemoryWin32HandlePropertiesKHR *pMemoryWin32HandleProperties)
{
  return ObjDisp(device)->GetMemoryWin32HandlePropertiesKHR(Unwrap(device), handleType, handle,
                                                            pMemoryWin32HandleProperties);
}
#endif

VkResult WrappedVulkan::vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR *pGetFdInfo,
                                         int *pFd)
{
  VkMemoryGetFdInfoKHR unwrappedInfo = *pGetFdInfo;
  unwrappedInfo.memory = Unwrap(unwrappedInfo.memory);
  return ObjDisp(device)->GetMemoryFdKHR(Unwrap(device), &unwrappedInfo, pFd);
}

VkResult WrappedVulkan::vkGetMemoryFdPropertiesKHR(VkDevice device,
                                                   VkExternalMemoryHandleTypeFlagBits handleType,
                                                   int fd,
                                                   VkMemoryFdPropertiesKHR *pMemoryFdProperties)
{
  return ObjDisp(device)->GetMemoryFdPropertiesKHR(Unwrap(device), handleType, fd,
                                                   pMemoryFdProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
    VkExternalBufferProperties *pExternalBufferProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceExternalBufferProperties(Unwrap(physicalDevice), pExternalBufferInfo,
                                                  pExternalBufferProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceExternalSemaphoreProperties(Unwrap(physicalDevice), pExternalSemaphoreInfo,
                                                     pExternalSemaphoreProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceExternalFenceProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
    VkExternalFenceProperties *pExternalFenceProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceExternalFenceProperties(Unwrap(physicalDevice), pExternalFenceInfo,
                                                 pExternalFenceProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                                 VkPhysicalDeviceFeatures2 *pFeatures)
{
  ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures2(Unwrap(physicalDevice), pFeatures);

  // if the user is requesting protected memory, make sure it's reported as NOT supported
  VkPhysicalDeviceProtectedMemoryFeatures *protectedMem =
      (VkPhysicalDeviceProtectedMemoryFeatures *)FindNextStruct(
          pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);

  if(protectedMem)
  {
    RDCWARN("Forcibly disabling support for protected memory");
    protectedMem->protectedMemory = VK_FALSE;
  }

  // in Vulkan 1.2 buffer_device_address can be used without an extension, so we can't hide the
  // extension when capture/replay is not supported. Instead we hide the feature bit here.
  VkPhysicalDeviceVulkan12Features *vulkan12 = (VkPhysicalDeviceVulkan12Features *)FindNextStruct(
      pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);

  if(vulkan12)
  {
    if(vulkan12->bufferDeviceAddressCaptureReplay == VK_FALSE)
    {
      RDCWARN(
          "VkPhysicalDeviceVulkan12Features::bufferDeviceAddressCaptureReplay is false, "
          "can't support capture of bufferDeviceAddress");
      vulkan12->bufferDeviceAddress = vulkan12->bufferDeviceAddressMultiDevice = VK_FALSE;
    }
  }

  // we don't want to report support for mesh shaders + multiview
  VkPhysicalDeviceMeshShaderFeaturesEXT *mesh =
      (VkPhysicalDeviceMeshShaderFeaturesEXT *)FindNextStruct(
          pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);

  if(mesh)
  {
    if(mesh->multiviewMeshShader)
    {
      RDCWARN("Disabling support for multiview + mesh shaders");
      mesh->multiviewMeshShader = VK_FALSE;
    }
  }

  // report features depending on extensions not supported in RenderDoc as not supported
  VkPhysicalDeviceExtendedDynamicState3FeaturesEXT *dynState3 =
      (VkPhysicalDeviceExtendedDynamicState3FeaturesEXT *)FindNextStruct(
          pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT);

#define DISABLE_EDS3_FEATURE(feature)                                                 \
  if(dynState3->feature == VK_TRUE)                                                   \
  {                                                                                   \
    RDCWARN("Forcibly disabling support for physical device feature '" #feature "'"); \
    dynState3->feature = VK_FALSE;                                                    \
  }

  if(dynState3)
  {
    // need VK_EXT_blend_operation_advanced
    DISABLE_EDS3_FEATURE(extendedDynamicState3ColorBlendAdvanced);
    // need VK_NV_clip_space_w_scaling
    DISABLE_EDS3_FEATURE(extendedDynamicState3ViewportWScalingEnable);
    // need VK_NV_viewport_swizzle
    DISABLE_EDS3_FEATURE(extendedDynamicState3ViewportSwizzle);
    // need VK_NV_fragment_coverage_to_color
    DISABLE_EDS3_FEATURE(extendedDynamicState3CoverageToColorEnable);
    DISABLE_EDS3_FEATURE(extendedDynamicState3CoverageToColorLocation);
    // need VK_NV_framebuffer_mixed_samples
    DISABLE_EDS3_FEATURE(extendedDynamicState3CoverageModulationMode);
    DISABLE_EDS3_FEATURE(extendedDynamicState3CoverageModulationTableEnable);
    DISABLE_EDS3_FEATURE(extendedDynamicState3CoverageModulationTable);
    // need VK_NV_coverage_reduction_mode
    DISABLE_EDS3_FEATURE(extendedDynamicState3CoverageReductionMode);
    // need VK_NV_representative_fragment_test
    DISABLE_EDS3_FEATURE(extendedDynamicState3RepresentativeFragmentTestEnable);
    // VK_NV_shading_rate_image
    DISABLE_EDS3_FEATURE(extendedDynamicState3ShadingRateImageEnable);
  }

#undef DISABLE_EDS3_FEATURE

  // we don't want to report support for acceleration structure host commands
  VkPhysicalDeviceAccelerationStructureFeaturesKHR *accStruct =
      (VkPhysicalDeviceAccelerationStructureFeaturesKHR *)FindNextStruct(
          pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);

  if(accStruct && accStruct->accelerationStructureHostCommands)
  {
    RDCWARN("Disabling support for acceleration structure host commands");
    accStruct->accelerationStructureHostCommands = VK_FALSE;
  }
}

void WrappedVulkan::vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                   VkPhysicalDeviceProperties2 *pProperties)
{
  ObjDisp(physicalDevice)->GetPhysicalDeviceProperties2(Unwrap(physicalDevice), pProperties);

  MakeFakeUUID();

  memcpy(pProperties->properties.pipelineCacheUUID, fakeRenderDocUUID, VK_UUID_SIZE);

  ClampPhysDevAPIVersion(&pProperties->properties, physicalDevice);

  // internal RenderDoc UUID for shader object binary
  VkPhysicalDeviceShaderObjectPropertiesEXT *shadObj =
      (VkPhysicalDeviceShaderObjectPropertiesEXT *)FindNextStruct(
          pProperties, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_PROPERTIES_EXT);

  if(shadObj)
  {
    memcpy(shadObj->shaderBinaryUUID, fakeRenderDocUUID, VK_UUID_SIZE);
  }
}

void WrappedVulkan::vkGetPhysicalDeviceQueueFamilyProperties2(
    VkPhysicalDevice physicalDevice, uint32_t *pCount,
    VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceQueueFamilyProperties2(Unwrap(physicalDevice), pCount,
                                                pQueueFamilyProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceMemoryProperties2(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceMemoryProperties2(Unwrap(physicalDevice), pMemoryProperties);
}

void WrappedVulkan::vkGetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
    uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSparseImageFormatProperties2(Unwrap(physicalDevice), pFormatInfo,
                                                      pPropertyCount, pProperties);
}

VkResult WrappedVulkan::vkGetShaderInfoAMD(VkDevice device, VkPipeline pipeline,
                                           VkShaderStageFlagBits shaderStage,
                                           VkShaderInfoTypeAMD infoType, size_t *pInfoSize,
                                           void *pInfo)
{
  return ObjDisp(device)->GetShaderInfoAMD(Unwrap(device), Unwrap(pipeline), shaderStage, infoType,
                                           pInfoSize, pInfo);
}

void WrappedVulkan::vkGetDescriptorSetLayoutSupport(VkDevice device,
                                                    const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                    VkDescriptorSetLayoutSupport *pSupport)
{
  VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  return ObjDisp(device)->GetDescriptorSetLayoutSupport(Unwrap(device), &unwrapped, pSupport);
}

VkResult WrappedVulkan::vkEnumeratePhysicalDeviceGroups(
    VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
    VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties)
{
  // We ignore the 'real' physical device groups, and report one group per physical device.
  // We use our internal enumerate function to make sure we handle wrapping the objects.
  RDCASSERT(pPhysicalDeviceGroupCount);

  // Total number of available physical device groups.
  uint32_t physicalDevicesNumber = 0;
  vkEnumeratePhysicalDevices(instance, &physicalDevicesNumber, NULL);

  // vkEnumeratePhysicalDeviceGroups - Return number of available physical device groups.
  if(pPhysicalDeviceGroupProperties == NULL)
  {
    *pPhysicalDeviceGroupCount = physicalDevicesNumber;
    return VK_SUCCESS;
  }

  // vkEnumeratePhysicalDeviceGroups - Query properties of available physical device groups.

  // Number of physical device groups to query.
  *pPhysicalDeviceGroupCount = RDCMIN(*pPhysicalDeviceGroupCount, physicalDevicesNumber);

  rdcarray<VkPhysicalDevice> physicalDevices;
  physicalDevices.resize(*pPhysicalDeviceGroupCount);
  vkEnumeratePhysicalDevices(instance, pPhysicalDeviceGroupCount, physicalDevices.data());

  // List one group per device.
  for(uint32_t i = 0; i < *pPhysicalDeviceGroupCount; i++)
  {
    RDCEraseEl(pPhysicalDeviceGroupProperties[i]);
    pPhysicalDeviceGroupProperties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
    pPhysicalDeviceGroupProperties[i].physicalDeviceCount = 1;
    pPhysicalDeviceGroupProperties[i].physicalDevices[0] = physicalDevices[i];
    pPhysicalDeviceGroupProperties[i].subsetAllocation = VK_FALSE;
  }

  if(*pPhysicalDeviceGroupCount < physicalDevicesNumber)
    return VK_INCOMPLETE;

  return VK_SUCCESS;
}

void WrappedVulkan::vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex,
                                                       uint32_t localDeviceIndex,
                                                       uint32_t remoteDeviceIndex,
                                                       VkPeerMemoryFeatureFlags *pPeerMemoryFeatures)
{
  return ObjDisp(device)->GetDeviceGroupPeerMemoryFeatures(
      Unwrap(device), heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

VkResult WrappedVulkan::vkCreateValidationCacheEXT(VkDevice device,
                                                   const VkValidationCacheCreateInfoEXT *pCreateInfo,
                                                   const VkAllocationCallbacks *,
                                                   VkValidationCacheEXT *pValidationCache)
{
  return ObjDisp(device)->CreateValidationCacheEXT(Unwrap(device), pCreateInfo, NULL,
                                                   pValidationCache);
}

void WrappedVulkan::vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache,
                                                const VkAllocationCallbacks *)
{
  return ObjDisp(device)->DestroyValidationCacheEXT(Unwrap(device), validationCache, NULL);
}

VkResult WrappedVulkan::vkMergeValidationCachesEXT(VkDevice device, VkValidationCacheEXT dstCache,
                                                   uint32_t srcCacheCount,
                                                   const VkValidationCacheEXT *pSrcCaches)
{
  return ObjDisp(device)->MergeValidationCachesEXT(Unwrap(device), dstCache, srcCacheCount,
                                                   pSrcCaches);
}

VkResult WrappedVulkan::vkGetValidationCacheDataEXT(VkDevice device,
                                                    VkValidationCacheEXT validationCache,
                                                    size_t *pDataSize, void *pData)
{
  return ObjDisp(device)->GetValidationCacheDataEXT(Unwrap(device), validationCache, pDataSize,
                                                    pData);
}
void WrappedVulkan::vkGetPhysicalDeviceMultisamplePropertiesEXT(
    VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples,
    VkMultisamplePropertiesEXT *pMultisampleProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceMultisamplePropertiesEXT(Unwrap(physicalDevice), samples,
                                                  pMultisampleProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(
    VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainKHR *pTimeDomains)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceCalibrateableTimeDomainsEXT(Unwrap(physicalDevice), pTimeDomainCount,
                                                     pTimeDomains);
}

VkResult WrappedVulkan::vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount,
                                                     const VkCalibratedTimestampInfoKHR *pTimestampInfos,
                                                     uint64_t *pTimestamps, uint64_t *pMaxDeviation)
{
  return ObjDisp(device)->GetCalibratedTimestampsEXT(Unwrap(device), timestampCount,
                                                     pTimestampInfos, pTimestamps, pMaxDeviation);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(
    VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainKHR *pTimeDomains)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceCalibrateableTimeDomainsKHR(Unwrap(physicalDevice), pTimeDomainCount,
                                                     pTimeDomains);
}

VkResult WrappedVulkan::vkGetCalibratedTimestampsKHR(VkDevice device, uint32_t timestampCount,
                                                     const VkCalibratedTimestampInfoKHR *pTimestampInfos,
                                                     uint64_t *pTimestamps, uint64_t *pMaxDeviation)
{
  return ObjDisp(device)->GetCalibratedTimestampsKHR(Unwrap(device), timestampCount,
                                                     pTimestampInfos, pTimestamps, pMaxDeviation);
}

VkDeviceAddress WrappedVulkan::vkGetBufferDeviceAddressEXT(VkDevice device,
                                                           const VkBufferDeviceAddressInfoEXT *pInfo)
{
  VkBufferDeviceAddressInfoEXT unwrappedInfo = *pInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  return ObjDisp(device)->GetBufferDeviceAddressEXT(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetPipelineExecutablePropertiesKHR(
    VkDevice device, const VkPipelineInfoKHR *pPipelineInfo, uint32_t *pExecutableCount,
    VkPipelineExecutablePropertiesKHR *pProperties)
{
  VkPipelineInfoKHR unwrappedInfo = *pPipelineInfo;
  unwrappedInfo.pipeline = Unwrap(unwrappedInfo.pipeline);
  return ObjDisp(device)->GetPipelineExecutablePropertiesKHR(Unwrap(device), &unwrappedInfo,
                                                             pExecutableCount, pProperties);
}

VkResult WrappedVulkan::vkGetPipelineExecutableStatisticsKHR(
    VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo, uint32_t *pStatisticCount,
    VkPipelineExecutableStatisticKHR *pStatistics)
{
  VkPipelineExecutableInfoKHR unwrappedInfo = *pExecutableInfo;
  unwrappedInfo.pipeline = Unwrap(unwrappedInfo.pipeline);
  return ObjDisp(device)->GetPipelineExecutableStatisticsKHR(Unwrap(device), &unwrappedInfo,
                                                             pStatisticCount, pStatistics);
}

VkResult WrappedVulkan::vkGetPipelineExecutableInternalRepresentationsKHR(
    VkDevice device, const VkPipelineExecutableInfoKHR *pExecutableInfo,
    uint32_t *pInternalRepresentationCount,
    VkPipelineExecutableInternalRepresentationKHR *pInternalRepresentations)
{
  VkPipelineExecutableInfoKHR unwrappedInfo = *pExecutableInfo;
  unwrappedInfo.pipeline = Unwrap(unwrappedInfo.pipeline);
  return ObjDisp(device)->GetPipelineExecutableInternalRepresentationsKHR(
      Unwrap(device), &unwrappedInfo, pInternalRepresentationCount, pInternalRepresentations);
}

VkDeviceAddress WrappedVulkan::vkGetBufferDeviceAddress(VkDevice device,
                                                        VkBufferDeviceAddressInfo *pInfo)
{
  VkBufferDeviceAddressInfo unwrappedInfo = *pInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  return ObjDisp(device)->GetBufferDeviceAddress(Unwrap(device), &unwrappedInfo);
}

uint64_t WrappedVulkan::vkGetBufferOpaqueCaptureAddress(VkDevice device,
                                                        VkBufferDeviceAddressInfo *pInfo)
{
  VkBufferDeviceAddressInfo unwrappedInfo = *pInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  return ObjDisp(device)->GetBufferOpaqueCaptureAddress(Unwrap(device), &unwrappedInfo);
}

uint64_t WrappedVulkan::vkGetDeviceMemoryOpaqueCaptureAddress(
    VkDevice device, VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo)
{
  VkDeviceMemoryOpaqueCaptureAddressInfo unwrappedInfo = *pInfo;
  unwrappedInfo.memory = Unwrap(unwrappedInfo.memory);
  return ObjDisp(device)->GetDeviceMemoryOpaqueCaptureAddress(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice,
                                                          uint32_t *pToolCount,
                                                          VkPhysicalDeviceToolProperties *pToolProperties)
{
  // check how many tools are downstream. The function pointer will be NULL if no-one else supports
  // this extension except us.
  uint32_t downstreamCount = 0;
  if(ObjDisp(physicalDevice)->GetPhysicalDeviceToolProperties != NULL)
    ObjDisp(physicalDevice)
        ->GetPhysicalDeviceToolProperties(Unwrap(physicalDevice), &downstreamCount, NULL);

  // if we're just enumerating, pToolProperties is NULL, so set the tool count and return
  if(pToolCount && pToolProperties == NULL)
  {
    *pToolCount = downstreamCount + 1;
    return VK_SUCCESS;
  }

  // otherwise we expect both to be non-NULL
  if(pToolCount == NULL || pToolProperties == NULL)
    return VK_INCOMPLETE;

  // this is how much space is in the array, don't forget it
  uint32_t availableCount = *pToolCount;

  VkResult vkr = VK_SUCCESS;

  if(ObjDisp(physicalDevice)->GetPhysicalDeviceToolProperties != NULL)
  {
    // call downstream to populate the array (up to what's available)
    // this writes up to availableCount properties into pToolProperties, and sets the number written
    // in pToolCount
    vkr = ObjDisp(physicalDevice)
              ->GetPhysicalDeviceToolProperties(Unwrap(physicalDevice), pToolCount, pToolProperties);
  }
  else
  {
    // nothing written downstream
    *pToolCount = 0;
  }

  // if available isn't enough, return VK_INCOMPLETE now
  if(vkr == VK_INCOMPLETE || availableCount < downstreamCount + 1)
    return VK_INCOMPLETE;

  // otherwise we write our own properties in after any downstream properties, then increment
  // pToolCount

  VkPhysicalDeviceToolProperties &props = *(pToolProperties + *pToolCount);

  const rdcstr name = "RenderDoc"_lit;
  const rdcstr version = StringFormat::Fmt(
      "%s (%s)", FULL_VERSION_STRING, GitVersionHash[0] == 'N' ? "Unknown revision" : GitVersionHash);
  const rdcstr description = "Debugging capture layer for RenderDoc"_lit;

  RDCASSERTMSG("Name is too long for VkPhysicalDeviceToolProperties",
               name.length() < sizeof(props.name));
  RDCASSERTMSG("Version is too long for VkPhysicalDeviceToolProperties",
               version.length() < sizeof(props.version));
  RDCASSERTMSG("Description is too long for VkPhysicalDeviceToolProperties",
               description.length() < sizeof(props.description));

  memcpy(props.name, name.c_str(), name.length() + 1);
  memcpy(props.version, version.c_str(), version.length() + 1);
  props.purposes = VK_TOOL_PURPOSE_TRACING_BIT | VK_TOOL_PURPOSE_DEBUG_MARKERS_BIT_EXT |
                   VK_TOOL_PURPOSE_MODIFYING_FEATURES_BIT;
  memcpy(props.description, description.c_str(), description.length() + 1);
  // do not tell people about the layer
  RDCEraseEl(props.layer);

  (*pToolCount)++;
  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceFragmentShadingRatesKHR(
    VkPhysicalDevice physicalDevice, uint32_t *pFragmentShadingRateCount,
    VkPhysicalDeviceFragmentShadingRateKHR *pFragmentShadingRates)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceFragmentShadingRatesKHR(Unwrap(physicalDevice), pFragmentShadingRateCount,
                                                 pFragmentShadingRates);
}

uint32_t WrappedVulkan::vkGetDeferredOperationMaxConcurrencyKHR(VkDevice device,
                                                                VkDeferredOperationKHR operation)
{
  return ObjDisp(device)->GetDeferredOperationMaxConcurrencyKHR(Unwrap(device), operation);
}

VkResult WrappedVulkan::vkGetDeferredOperationResultKHR(VkDevice device,
                                                        VkDeferredOperationKHR operation)
{
  return ObjDisp(device)->GetDeferredOperationResultKHR(Unwrap(device), operation);
}

void WrappedVulkan::vkGetAccelerationStructureBuildSizesKHR(
    VkDevice device, VkAccelerationStructureBuildTypeKHR buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo,
    const uint32_t *pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
  VkAccelerationStructureBuildGeometryInfoKHR unwrapped = *pBuildInfo;
  unwrapped.srcAccelerationStructure = Unwrap(unwrapped.srcAccelerationStructure);
  unwrapped.dstAccelerationStructure = Unwrap(unwrapped.dstAccelerationStructure);

  ObjDisp(device)->GetAccelerationStructureBuildSizesKHR(Unwrap(device), buildType, pBuildInfo,
                                                         pMaxPrimitiveCounts, pSizeInfo);
}

VkDeviceAddress WrappedVulkan::vkGetAccelerationStructureDeviceAddressKHR(
    VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR *pInfo)
{
  VkAccelerationStructureDeviceAddressInfoKHR info = *pInfo;
  info.accelerationStructure = Unwrap(info.accelerationStructure);
  return ObjDisp(device)->GetAccelerationStructureDeviceAddressKHR(Unwrap(device), &info);
}

void WrappedVulkan::vkGetDeviceAccelerationStructureCompatibilityKHR(
    VkDevice device, const VkAccelerationStructureVersionInfoKHR *pVersionInfo,
    VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
  *pCompatibility = VK_ACCELERATION_STRUCTURE_COMPATIBILITY_INCOMPATIBLE_KHR;
}

VkResult WrappedVulkan::vkGetShaderBinaryDataEXT(VkDevice device, VkShaderEXT shader,
                                                 size_t *pDataSize, void *pData)
{
  // renderdoc doesn't support shader binaries, but should comply with the spec
  // so we return four NULL bytes if this function is called and would otherwise
  // return a valid binary
  size_t totalSize = 4;

  if(pDataSize && !pData)
    *pDataSize = totalSize;

  if(pDataSize && pData)
  {
    if(*pDataSize < totalSize)
    {
      memset(pData, 0, *pDataSize);
      return VK_INCOMPLETE;
    }

    // empty bytes
    memset(pData, 0, 4);
  }

  // we don't want the application to use shader binaries at all, and especially
  // don't want to return any data for future use. We thus return a technically
  // valid but empty shader binary. Our UUID changes every run so in theory the
  // application should never provide an old binary.
  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline,
                                                             uint32_t firstGroup, uint32_t groupCount,
                                                             size_t dataSize, void *pData)
{
  return ObjDisp(device)->GetRayTracingShaderGroupHandlesKHR(
      Unwrap(device), Unwrap(pipeline), firstGroup, groupCount, dataSize, pData);
}

VkResult WrappedVulkan::vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
    VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize,
    void *pData)
{
  return ObjDisp(device)->GetRayTracingCaptureReplayShaderGroupHandlesKHR(
      Unwrap(device), Unwrap(pipeline), firstGroup, groupCount, dataSize, pData);
}

VkDeviceSize WrappedVulkan::vkGetRayTracingShaderGroupStackSizeKHR(
    VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader)
{
  return ObjDisp(device)->GetRayTracingShaderGroupStackSizeKHR(Unwrap(device), Unwrap(pipeline),
                                                               group, groupShader);
}
