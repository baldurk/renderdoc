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

#include "../vk_core.h"

static char fakeRenderDocUUID[VK_UUID_SIZE + 1] = {};

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
    StringFormat::sntimef(fakeRenderDocUUID, VK_UUID_SIZE + 1, "rdoc%y%m%d%H%M%S");
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
  if(pMemoryProperties)
  {
    *pMemoryProperties = *GetRecord(physicalDevice)->memProps;
    return;
  }

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

  // don't do remapping here on replay.
  if(IsReplayMode(m_State))
    return;

  uint32_t bits = pMemoryRequirements->memoryTypeBits;
  uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

  pMemoryRequirements->memoryTypeBits = 0;

  // for each of our fake memory indices, check if the real
  // memory type it points to is set - if so, set our fake bit
  for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    if(memIdxMap[i] < 32U && (bits & (1U << memIdxMap[i])))
      pMemoryRequirements->memoryTypeBits |= (1U << i);
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

  // don't do remapping here on replay.
  if(IsReplayMode(m_State))
    return;

  uint32_t bits = pMemoryRequirements->memoryTypeBits;
  uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

  pMemoryRequirements->memoryTypeBits = 0;

  // for each of our fake memory indices, check if the real
  // memory type it points to is set - if so, set our fake bit
  for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    if(memIdxMap[i] < 32U && (bits & (1U << memIdxMap[i])))
      pMemoryRequirements->memoryTypeBits |= (1U << i);

  // AMD can have some variability in the returned size, so we need to pad the reported size to
  // allow for this. The variability isn't quite clear, but for now we assume aligning size to
  // alignment * 4 should be sufficient (adding on a fixed padding won't help the problem as it
  // won't remove the variability, nor will adding then aligning for the same reason).
  if(GetDriverInfo().UnreliableImageMemoryRequirements() && pMemoryRequirements->size > 0)
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

  // don't do remapping here on replay.
  if(IsReplayMode(m_State))
    return;

  uint32_t bits = pMemoryRequirements->memoryRequirements.memoryTypeBits;
  uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

  pMemoryRequirements->memoryRequirements.memoryTypeBits = 0;

  // for each of our fake memory indices, check if the real
  // memory type it points to is set - if so, set our fake bit
  for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    if(memIdxMap[i] < 32U && (bits & (1U << memIdxMap[i])))
      pMemoryRequirements->memoryRequirements.memoryTypeBits |= (1U << i);
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

  uint32_t bits = pMemoryRequirements->memoryRequirements.memoryTypeBits;
  uint32_t *memIdxMap = GetRecord(device)->memIdxMap;

  pMemoryRequirements->memoryRequirements.memoryTypeBits = 0;

  // for each of our fake memory indices, check if the real
  // memory type it points to is set - if so, set our fake bit
  for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    if(memIdxMap[i] < 32U && (bits & (1U << memIdxMap[i])))
      pMemoryRequirements->memoryRequirements.memoryTypeBits |= (1U << i);

  // AMD can have some variability in the returned size, so we need to pad the reported size to
  // allow for this. The variability isn't quite clear, but for now we assume aligning size to
  // alignment * 4 should be sufficient (adding on a fixed padding won't help the problem as it
  // won't remove the variability, nor will adding then aligning for the same reason).
  if(GetDriverInfo().UnreliableImageMemoryRequirements() &&
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
  size_t totalSize = 16 + VK_UUID_SIZE + 4;    // required header (16+UUID) and 4 0 bytes

  if(pDataSize && !pData)
    *pDataSize = totalSize;

  if(pDataSize && pData)
  {
    if(*pDataSize < totalSize)
    {
      memset(pData, 0, *pDataSize);
      return VK_INCOMPLETE;
    }

    uint32_t *ptr = (uint32_t *)pData;

    ptr[0] = (uint32_t)totalSize;
    ptr[1] = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
    // just in case the user expects a valid vendorID/deviceID, write the real one
    // MULTIDEVICE need to get the right physical device for this device
    ptr[2] = m_PhysicalDeviceData.props.vendorID;
    ptr[3] = m_PhysicalDeviceData.props.deviceID;

    MakeFakeUUID();

    memcpy(ptr + 4, fakeRenderDocUUID, VK_UUID_SIZE);
    // [4], [5], [6], [7]

    RDCCOMPILE_ASSERT(VK_UUID_SIZE == 16, "VK_UUID_SIZE has changed");

    // empty bytes
    ptr[8] = 0;
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
    VkDevice device, VkExternalMemoryHandleTypeFlagBitsKHR handleType, HANDLE handle,
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
                                                   VkExternalMemoryHandleTypeFlagBitsKHR handleType,
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
}

void WrappedVulkan::vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                   VkPhysicalDeviceProperties2 *pProperties)
{
  return ObjDisp(physicalDevice)->GetPhysicalDeviceProperties2(Unwrap(physicalDevice), pProperties);
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
  // we ignore the 'real' physical device groups, and report one group per physical device. We use
  // our internal enumerate function to make sure we handle wrapping the objects.
  uint32_t numPhys = 0;
  vkEnumeratePhysicalDevices(instance, &numPhys, NULL);

  VkPhysicalDevice *phys = new VkPhysicalDevice[numPhys];
  vkEnumeratePhysicalDevices(instance, &numPhys, phys);

  uint32_t outputSpace = pPhysicalDeviceGroupCount ? *pPhysicalDeviceGroupCount : 0;

  if(pPhysicalDeviceGroupCount)
    *pPhysicalDeviceGroupCount = numPhys;

  if(pPhysicalDeviceGroupProperties)
  {
    // list one group per device
    for(uint32_t i = 0; i < outputSpace; i++)
    {
      RDCEraseEl(pPhysicalDeviceGroupProperties[i]);
      pPhysicalDeviceGroupProperties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
      pPhysicalDeviceGroupProperties[i].physicalDeviceCount = 1;
      pPhysicalDeviceGroupProperties[i].physicalDevices[0] = phys[i];
      pPhysicalDeviceGroupProperties[i].subsetAllocation = VK_FALSE;
    }
  }

  delete[] phys;

  if(pPhysicalDeviceGroupProperties && outputSpace < numPhys)
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
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkValidationCacheEXT *pValidationCache)
{
  return ObjDisp(device)->CreateValidationCacheEXT(Unwrap(device), pCreateInfo, pAllocator,
                                                   pValidationCache);
}

void WrappedVulkan::vkDestroyValidationCacheEXT(VkDevice device, VkValidationCacheEXT validationCache,
                                                const VkAllocationCallbacks *pAllocator)
{
  return ObjDisp(device)->DestroyValidationCacheEXT(Unwrap(device), validationCache, pAllocator);
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
    VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount, VkTimeDomainEXT *pTimeDomains)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceCalibrateableTimeDomainsEXT(Unwrap(physicalDevice), pTimeDomainCount,
                                                     pTimeDomains);
}

VkResult WrappedVulkan::vkGetCalibratedTimestampsEXT(VkDevice device, uint32_t timestampCount,
                                                     const VkCalibratedTimestampInfoEXT *pTimestampInfos,
                                                     uint64_t *pTimestamps, uint64_t *pMaxDeviation)
{
  return ObjDisp(device)->GetCalibratedTimestampsEXT(Unwrap(device), timestampCount,
                                                     pTimestampInfos, pTimestamps, pMaxDeviation);
}

VkDeviceAddress WrappedVulkan::vkGetBufferDeviceAddressEXT(VkDevice device,
                                                           const VkBufferDeviceAddressInfoEXT *pInfo)
{
  VkBufferDeviceAddressInfoEXT unwrappedInfo = *pInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  return ObjDisp(device)->GetBufferDeviceAddressEXT(Unwrap(device), &unwrappedInfo);
}