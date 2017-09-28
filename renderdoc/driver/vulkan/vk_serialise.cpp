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

#include "vk_common.h"
#include "vk_manager.h"
#include "vk_resources.h"

// simple way to express "resources referenced from this struct don't have to be present."
// since this is used during read when the processing is single-threaded, we make it a static flag.
// If we multi-thread reading, this could be stored in the Serialiser context somehow.
template <typename SerialiserType>
struct OptionalResources
{
private:
  OptionalResources() = default;
};

// does nothing on writing
template <>
struct OptionalResources<Serialiser<SerialiserMode::Writing>>
{
  OptionalResources<Serialiser<SerialiserMode::Writing>>(Serialiser<SerialiserMode::Writing> &ser)
  {
  }
  ~OptionalResources<Serialiser<SerialiserMode::Writing>>() {}
};

template <>
struct OptionalResources<Serialiser<SerialiserMode::Reading>>
{
  OptionalResources<Serialiser<SerialiserMode::Reading>>(Serialiser<SerialiserMode::Reading> &ser)
  {
    Counter++;
  }
  ~OptionalResources<Serialiser<SerialiserMode::Reading>>() { Counter--; }
  static int Counter;
};

template <typename SerialiserType>
OptionalResources<SerialiserType> ScopedOptional(SerialiserType &ser)
{
  return OptionalResources<SerialiserType>(ser);
}

int OptionalResources<Serialiser<SerialiserMode::Reading>>::Counter = 0;

bool OptionalResourcesEnabled()
{
  return OptionalResources<Serialiser<SerialiserMode::Reading>>::Counter > 0;
}

// push/pop the optional flag. This doesn't allow non-optional objects in a sub-struct inside a
// struct that had optional objects... but that doesn't come up and seems unlikely.
#define OPTIONAL_RESOURCES() auto opt__LINE__ = ScopedOptional(ser);

// serialisation of object handles via IDs.
template <class SerialiserType, class type>
void DoSerialiseViaResourceId(SerialiserType &ser, type &el)
{
  VulkanResourceManager *rm = (VulkanResourceManager *)ser.GetUserData();

  ResourceId id;

  if(ser.IsWriting() && rm)
    id = GetResID(el);

  DoSerialise(ser, id);

  if(ser.IsReading())
  {
    el = VK_NULL_HANDLE;

    if(id != ResourceId() && rm)
    {
      if(rm->HasLiveResource(id))
      {
        // we leave this wrapped.
        el = rm->GetLiveHandle<type>(id);
      }
      else if(!OptionalResourcesEnabled())
      {
        // It can be OK for a resource to have no live equivalent if the capture decided its not
        // needed, which some APIs do fairly often.
        RDCWARN("Capture may be missing reference to %s resource.", TypeName<type>());
      }
    }
  }
}

#undef SERIALISE_HANDLE
#define SERIALISE_HANDLE(type)                    \
  template <class SerialiserType>                 \
  void DoSerialise(SerialiserType &ser, type &el) \
  {                                               \
    DoSerialiseViaResourceId(ser, el);            \
  }                                               \
  INSTANTIATE_SERIALISE_TYPE(type);

SERIALISE_VK_HANDLES();

template <typename SerialiserType>
static void SerialiseNext(SerialiserType &ser, VkStructureType &sType, const void *&pNext)
{
  ser.Serialise("sType", sType);

  if(ser.IsReading())
  {
    pNext = NULL;
  }
  else
  {
    if(pNext == NULL)
      return;

    VkGenericStruct *next = (VkGenericStruct *)pNext;

    while(next)
    {
      // we can ignore this entirely, we don't need to serialise or replay it as we won't
      // actually use external memory. Unwrapping, if necessary, happens elsewhere
      if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||

         next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)
      {
        // do nothing
      }
      // likewise we don't create real swapchains, so we can ignore surface counters
      else if(next->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT)
      {
        // do nothing
      }
      // for now we don't serialise dedicated memory on replay as it's only a performance hint,
      // and is only required in conjunction with shared memory (which we don't replay). In future
      // it might be helpful to serialise this for informational purposes.
      else if(next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV ||
              next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV ||
              next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV ||
              next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR)
      {
        // do nothing
      }
      else
      {
        RDCERR("Unrecognised extension structure type %d", next->sType);
      }

      next = (VkGenericStruct *)next->pNext;
    }
  }
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceQueueCreateInfo &el)
{
  if(ser.IsWriting() && el.sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
    RDCWARN("sType not set properly: %u", el.sType);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(queueFamilyIndex);
  SERIALISE_MEMBER(queueCount);
  SERIALISE_MEMBER_ARRAY(pQueuePriorities, queueCount);
}

// technically this doesn't need a serialise function as it's POD,
// but we give it one just for ease of printing etc.
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceFeatures &el)
{
  SERIALISE_MEMBER(robustBufferAccess);
  SERIALISE_MEMBER(fullDrawIndexUint32);
  SERIALISE_MEMBER(imageCubeArray);
  SERIALISE_MEMBER(independentBlend);
  SERIALISE_MEMBER(geometryShader);
  SERIALISE_MEMBER(tessellationShader);
  SERIALISE_MEMBER(sampleRateShading);
  SERIALISE_MEMBER(dualSrcBlend);
  SERIALISE_MEMBER(logicOp);
  SERIALISE_MEMBER(multiDrawIndirect);
  SERIALISE_MEMBER(drawIndirectFirstInstance);
  SERIALISE_MEMBER(depthClamp);
  SERIALISE_MEMBER(depthBiasClamp);
  SERIALISE_MEMBER(fillModeNonSolid);
  SERIALISE_MEMBER(depthBounds);
  SERIALISE_MEMBER(wideLines);
  SERIALISE_MEMBER(largePoints);
  SERIALISE_MEMBER(alphaToOne);
  SERIALISE_MEMBER(multiViewport);
  SERIALISE_MEMBER(samplerAnisotropy);
  SERIALISE_MEMBER(textureCompressionETC2);
  SERIALISE_MEMBER(textureCompressionASTC_LDR);
  SERIALISE_MEMBER(textureCompressionBC);
  SERIALISE_MEMBER(occlusionQueryPrecise);
  SERIALISE_MEMBER(pipelineStatisticsQuery);
  SERIALISE_MEMBER(vertexPipelineStoresAndAtomics);
  SERIALISE_MEMBER(fragmentStoresAndAtomics);
  SERIALISE_MEMBER(shaderTessellationAndGeometryPointSize);
  SERIALISE_MEMBER(shaderImageGatherExtended);
  SERIALISE_MEMBER(shaderStorageImageExtendedFormats);
  SERIALISE_MEMBER(shaderStorageImageMultisample);
  SERIALISE_MEMBER(shaderStorageImageReadWithoutFormat);
  SERIALISE_MEMBER(shaderStorageImageWriteWithoutFormat);
  SERIALISE_MEMBER(shaderUniformBufferArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderSampledImageArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderStorageBufferArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderStorageImageArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderClipDistance);
  SERIALISE_MEMBER(shaderCullDistance);
  SERIALISE_MEMBER(shaderFloat64);
  SERIALISE_MEMBER(shaderInt64);
  SERIALISE_MEMBER(shaderInt16);
  SERIALISE_MEMBER(shaderResourceResidency);
  SERIALISE_MEMBER(shaderResourceMinLod);
  SERIALISE_MEMBER(sparseBinding);
  SERIALISE_MEMBER(sparseResidencyBuffer);
  SERIALISE_MEMBER(sparseResidencyImage2D);
  SERIALISE_MEMBER(sparseResidencyImage3D);
  SERIALISE_MEMBER(sparseResidency2Samples);
  SERIALISE_MEMBER(sparseResidency4Samples);
  SERIALISE_MEMBER(sparseResidency8Samples);
  SERIALISE_MEMBER(sparseResidency16Samples);
  SERIALISE_MEMBER(sparseResidencyAliased);
  SERIALISE_MEMBER(variableMultisampleRate);
  SERIALISE_MEMBER(inheritedQueries);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryHeap &el)
{
  SERIALISE_MEMBER(size);
  SERIALISE_MEMBER(flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryType &el)
{
  SERIALISE_MEMBER(propertyFlags);
  SERIALISE_MEMBER(heapIndex);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMemoryProperties &el)
{
  SERIALISE_MEMBER(memoryTypeCount);
  SERIALISE_MEMBER(memoryTypes);
  SERIALISE_MEMBER(memoryHeapCount);
  SERIALISE_MEMBER(memoryHeaps);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceLimits &el)
{
  SERIALISE_MEMBER(maxImageDimension1D);
  SERIALISE_MEMBER(maxImageDimension2D);
  SERIALISE_MEMBER(maxImageDimension3D);
  SERIALISE_MEMBER(maxImageDimensionCube);
  SERIALISE_MEMBER(maxImageArrayLayers);
  SERIALISE_MEMBER(maxTexelBufferElements);
  SERIALISE_MEMBER(maxUniformBufferRange);
  SERIALISE_MEMBER(maxStorageBufferRange);
  SERIALISE_MEMBER(maxPushConstantsSize);
  SERIALISE_MEMBER(maxMemoryAllocationCount);
  SERIALISE_MEMBER(maxSamplerAllocationCount);
  SERIALISE_MEMBER(bufferImageGranularity);
  SERIALISE_MEMBER(sparseAddressSpaceSize);
  SERIALISE_MEMBER(maxBoundDescriptorSets);
  SERIALISE_MEMBER(maxPerStageDescriptorSamplers);
  SERIALISE_MEMBER(maxPerStageDescriptorUniformBuffers);
  SERIALISE_MEMBER(maxPerStageDescriptorStorageBuffers);
  SERIALISE_MEMBER(maxPerStageDescriptorSampledImages);
  SERIALISE_MEMBER(maxPerStageDescriptorStorageImages);
  SERIALISE_MEMBER(maxPerStageDescriptorInputAttachments);
  SERIALISE_MEMBER(maxPerStageResources);
  SERIALISE_MEMBER(maxDescriptorSetSamplers);
  SERIALISE_MEMBER(maxDescriptorSetUniformBuffers);
  SERIALISE_MEMBER(maxDescriptorSetUniformBuffersDynamic);
  SERIALISE_MEMBER(maxDescriptorSetStorageBuffers);
  SERIALISE_MEMBER(maxDescriptorSetStorageBuffersDynamic);
  SERIALISE_MEMBER(maxDescriptorSetSampledImages);
  SERIALISE_MEMBER(maxDescriptorSetStorageImages);
  SERIALISE_MEMBER(maxDescriptorSetInputAttachments);
  SERIALISE_MEMBER(maxVertexInputAttributes);
  SERIALISE_MEMBER(maxVertexInputBindings);
  SERIALISE_MEMBER(maxVertexInputAttributeOffset);
  SERIALISE_MEMBER(maxVertexInputBindingStride);
  SERIALISE_MEMBER(maxVertexOutputComponents);
  SERIALISE_MEMBER(maxTessellationGenerationLevel);
  SERIALISE_MEMBER(maxTessellationPatchSize);
  SERIALISE_MEMBER(maxTessellationControlPerVertexInputComponents);
  SERIALISE_MEMBER(maxTessellationControlPerVertexOutputComponents);
  SERIALISE_MEMBER(maxTessellationControlPerPatchOutputComponents);
  SERIALISE_MEMBER(maxTessellationControlTotalOutputComponents);
  SERIALISE_MEMBER(maxTessellationEvaluationInputComponents);
  SERIALISE_MEMBER(maxTessellationEvaluationOutputComponents);
  SERIALISE_MEMBER(maxGeometryShaderInvocations);
  SERIALISE_MEMBER(maxGeometryInputComponents);
  SERIALISE_MEMBER(maxGeometryOutputComponents);
  SERIALISE_MEMBER(maxGeometryOutputVertices);
  SERIALISE_MEMBER(maxGeometryTotalOutputComponents);
  SERIALISE_MEMBER(maxFragmentInputComponents);
  SERIALISE_MEMBER(maxFragmentOutputAttachments);
  SERIALISE_MEMBER(maxFragmentDualSrcAttachments);
  SERIALISE_MEMBER(maxFragmentCombinedOutputResources);
  SERIALISE_MEMBER(maxComputeSharedMemorySize);
  SERIALISE_MEMBER(maxComputeWorkGroupCount);
  SERIALISE_MEMBER(maxComputeWorkGroupInvocations);
  SERIALISE_MEMBER(maxComputeWorkGroupSize);
  SERIALISE_MEMBER(subPixelPrecisionBits);
  SERIALISE_MEMBER(subTexelPrecisionBits);
  SERIALISE_MEMBER(mipmapPrecisionBits);
  SERIALISE_MEMBER(maxDrawIndexedIndexValue);
  SERIALISE_MEMBER(maxDrawIndirectCount);
  SERIALISE_MEMBER(maxSamplerLodBias);
  SERIALISE_MEMBER(maxSamplerAnisotropy);
  SERIALISE_MEMBER(maxViewports);
  SERIALISE_MEMBER(maxViewportDimensions);
  SERIALISE_MEMBER(viewportBoundsRange);
  SERIALISE_MEMBER(viewportSubPixelBits);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t minMemoryMapAlignment = (uint64_t)el.minMemoryMapAlignment;
    ser.Serialise("minMemoryMapAlignment", minMemoryMapAlignment);
    if(ser.IsReading())
      el.minMemoryMapAlignment = (size_t)minMemoryMapAlignment;
  }

  SERIALISE_MEMBER(minTexelBufferOffsetAlignment);
  SERIALISE_MEMBER(minUniformBufferOffsetAlignment);
  SERIALISE_MEMBER(minStorageBufferOffsetAlignment);
  SERIALISE_MEMBER(minTexelOffset);
  SERIALISE_MEMBER(maxTexelOffset);
  SERIALISE_MEMBER(minTexelGatherOffset);
  SERIALISE_MEMBER(maxTexelGatherOffset);
  SERIALISE_MEMBER(minInterpolationOffset);
  SERIALISE_MEMBER(maxInterpolationOffset);
  SERIALISE_MEMBER(subPixelInterpolationOffsetBits);
  SERIALISE_MEMBER(maxFramebufferWidth);
  SERIALISE_MEMBER(maxFramebufferHeight);
  SERIALISE_MEMBER(maxFramebufferLayers);
  SERIALISE_MEMBER(framebufferColorSampleCounts);
  SERIALISE_MEMBER(framebufferDepthSampleCounts);
  SERIALISE_MEMBER(framebufferStencilSampleCounts);
  SERIALISE_MEMBER(framebufferNoAttachmentsSampleCounts);
  SERIALISE_MEMBER(maxColorAttachments);
  SERIALISE_MEMBER(sampledImageColorSampleCounts);
  SERIALISE_MEMBER(sampledImageIntegerSampleCounts);
  SERIALISE_MEMBER(sampledImageDepthSampleCounts);
  SERIALISE_MEMBER(sampledImageStencilSampleCounts);
  SERIALISE_MEMBER(storageImageSampleCounts);
  SERIALISE_MEMBER(maxSampleMaskWords);
  SERIALISE_MEMBER(timestampComputeAndGraphics);
  SERIALISE_MEMBER(timestampPeriod);
  SERIALISE_MEMBER(maxClipDistances);
  SERIALISE_MEMBER(maxCullDistances);
  SERIALISE_MEMBER(maxCombinedClipAndCullDistances);
  SERIALISE_MEMBER(discreteQueuePriorities);
  SERIALISE_MEMBER(pointSizeRange);
  SERIALISE_MEMBER(lineWidthRange);
  SERIALISE_MEMBER(pointSizeGranularity);
  SERIALISE_MEMBER(lineWidthGranularity);
  SERIALISE_MEMBER(strictLines);
  SERIALISE_MEMBER(standardSampleLocations);
  SERIALISE_MEMBER(optimalBufferCopyOffsetAlignment);
  SERIALISE_MEMBER(optimalBufferCopyRowPitchAlignment);
  SERIALISE_MEMBER(nonCoherentAtomSize);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSparseProperties &el)
{
  SERIALISE_MEMBER(residencyStandard2DBlockShape);
  SERIALISE_MEMBER(residencyStandard2DMultisampleBlockShape);
  SERIALISE_MEMBER(residencyStandard3DBlockShape);
  SERIALISE_MEMBER(residencyAlignedMipSize);
  SERIALISE_MEMBER(residencyNonResidentStrict);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkQueueFamilyProperties &el)
{
  SERIALISE_MEMBER(queueFlags);
  SERIALISE_MEMBER(queueCount);
  SERIALISE_MEMBER(timestampValidBits);
  SERIALISE_MEMBER(minImageTransferGranularity);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceProperties &el)
{
  SERIALISE_MEMBER(apiVersion);
  SERIALISE_MEMBER(driverVersion);
  SERIALISE_MEMBER(vendorID);
  SERIALISE_MEMBER(deviceID);
  SERIALISE_MEMBER(deviceType);
  SERIALISE_MEMBER(deviceName);
  SERIALISE_MEMBER(pipelineCacheUUID);
  SERIALISE_MEMBER(limits);
  SERIALISE_MEMBER(sparseProperties);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER_ARRAY(pQueueCreateInfos, queueCreateInfoCount);
  SERIALISE_MEMBER_ARRAY(ppEnabledExtensionNames, enabledExtensionCount);
  SERIALISE_MEMBER_ARRAY(ppEnabledLayerNames, enabledLayerCount);
  SERIALISE_MEMBER_OPT(pEnabledFeatures);
}

template <>
void Deserialise(const VkDeviceCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  for(uint32_t i = 0; i < el.queueCreateInfoCount; i++)
    delete[] el.pQueueCreateInfos[i].pQueuePriorities;
  delete[] el.pQueueCreateInfos;
  delete[] el.ppEnabledExtensionNames;
  delete[] el.ppEnabledLayerNames;
  delete el.pEnabledFeatures;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkBufferCreateFlagBits, flags);
  SERIALISE_MEMBER(size);
  SERIALISE_MEMBER_TYPED(VkBufferUsageFlagBits, usage);
  SERIALISE_MEMBER(sharingMode);

  // pQueueFamilyIndices should *only* be read if the sharing mode is concurrent
  if(el.sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    SERIALISE_MEMBER_ARRAY(pQueueFamilyIndices, queueFamilyIndexCount);
  }
  else if(ser.IsReading())
  {
    // otherwise just set to NULL for sanity
    el.pQueueFamilyIndices = NULL;
    el.queueFamilyIndexCount = 0;
  }
}

template <>
void Deserialise(const VkBufferCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pQueueFamilyIndices;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferViewCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(range);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkImageCreateFlagBits, flags);
  SERIALISE_MEMBER(imageType);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(mipLevels);
  SERIALISE_MEMBER(arrayLayers);
  SERIALISE_MEMBER(samples);
  SERIALISE_MEMBER(tiling);
  SERIALISE_MEMBER_TYPED(VkImageUsageFlagBits, usage);
  SERIALISE_MEMBER(sharingMode);
  SERIALISE_MEMBER(initialLayout);

  // pQueueFamilyIndices should *only* be read if the sharing mode is concurrent
  if(el.sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    SERIALISE_MEMBER_ARRAY(pQueueFamilyIndices, queueFamilyIndexCount);
  }
  else if(ser.IsReading())
  {
    // otherwise just set to NULL for sanity
    el.pQueueFamilyIndices = NULL;
    el.queueFamilyIndexCount = 0;
  }
}

template <>
void Deserialise(const VkImageCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pQueueFamilyIndices;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageViewCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(viewType);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(components);
  SERIALISE_MEMBER(subresourceRange);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseMemoryBind &el)
{
  SERIALISE_MEMBER(resourceOffset);
  SERIALISE_MEMBER(size);
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memoryOffset);
  SERIALISE_MEMBER_TYPED(VkSparseMemoryBindFlagBits, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseBufferMemoryBindInfo &el)
{
  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageOpaqueMemoryBindInfo &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageMemoryBind &el)
{
  SERIALISE_MEMBER(subresource);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memoryOffset);
  SERIALISE_MEMBER_TYPED(VkSparseMemoryBindFlagBits, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageMemoryBindInfo &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindSparseInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_SPARSE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_ARRAY(pWaitSemaphores, waitSemaphoreCount);

  SERIALISE_MEMBER_ARRAY(pBufferBinds, bufferBindCount);
  SERIALISE_MEMBER_ARRAY(pImageOpaqueBinds, imageOpaqueBindCount);
  SERIALISE_MEMBER_ARRAY(pImageBinds, imageBindCount);

  SERIALISE_MEMBER_ARRAY(pSignalSemaphores, signalSemaphoreCount);
}

template <>
void Deserialise(const VkBindSparseInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pWaitSemaphores;
  for(uint32_t i = 0; i < el.bufferBindCount; i++)
    delete[] el.pBufferBinds[i].pBinds;
  delete[] el.pBufferBinds;
  for(uint32_t i = 0; i < el.imageOpaqueBindCount; i++)
    delete[] el.pImageOpaqueBinds[i].pBinds;
  delete[] el.pImageOpaqueBinds;
  for(uint32_t i = 0; i < el.imageBindCount; i++)
    delete[] el.pImageBinds[i].pBinds;
  delete[] el.pImageBinds;
  delete[] el.pSignalSemaphores;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubmitInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  // bit of a hack, we alias the ptr here to the bits type so we serialise with better type info
  union
  {
    const VkPipelineStageFlagBits **typed;
    const VkPipelineStageFlags **orig;
  } u;
  u.orig = &el.pWaitDstStageMask;

  ser.Serialise("pWaitDstStageMask", *u.typed, el.waitSemaphoreCount,
                SerialiserFlags::AllocateMemory);
  SERIALISE_MEMBER_ARRAY(pWaitSemaphores, waitSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pCommandBuffers, commandBufferCount);
  SERIALISE_MEMBER_ARRAY(pSignalSemaphores, signalSemaphoreCount);
}

template <>
void Deserialise(const VkSubmitInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pWaitSemaphores;
  delete[] el.pCommandBuffers;
  delete[] el.pSignalSemaphores;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFramebufferCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(renderPass);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(layers);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
}

template <>
void Deserialise(const VkFramebufferCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pAttachments;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAttachmentDescription &el)
{
  SERIALISE_MEMBER_TYPED(VkAttachmentDescriptionFlagBits, flags);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(samples);
  SERIALISE_MEMBER(loadOp);
  SERIALISE_MEMBER(storeOp);
  SERIALISE_MEMBER(stencilLoadOp);
  SERIALISE_MEMBER(stencilStoreOp);
  SERIALISE_MEMBER(initialLayout);
  SERIALISE_MEMBER(finalLayout);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassDescription &el)
{
  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(pipelineBindPoint);
  SERIALISE_MEMBER_OPT(pDepthStencilAttachment);
  SERIALISE_MEMBER_ARRAY(pInputAttachments, inputAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pResolveAttachments, colorAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pColorAttachments, colorAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pPreserveAttachments, preserveAttachmentCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassDependency &el)
{
  SERIALISE_MEMBER(srcSubpass);
  SERIALISE_MEMBER(dstSubpass);
  SERIALISE_MEMBER_TYPED(VkPipelineStageFlagBits, srcStageMask);
  SERIALISE_MEMBER_TYPED(VkPipelineStageFlagBits, dstStageMask);
  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, srcAccessMask);
  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, dstAccessMask);
  SERIALISE_MEMBER_TYPED(VkDependencyFlagBits, dependencyFlags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAttachmentReference &el)
{
  SERIALISE_MEMBER(attachment);
  SERIALISE_MEMBER(layout);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRenderPassCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
  SERIALISE_MEMBER_ARRAY(pSubpasses, subpassCount);
  SERIALISE_MEMBER_ARRAY(pDependencies, dependencyCount);
}

template <>
void Deserialise(const VkRenderPassCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pAttachments;
  for(uint32_t i = 0; i < el.subpassCount; i++)
  {
    delete el.pSubpasses[i].pDepthStencilAttachment;
    delete[] el.pSubpasses[i].pInputAttachments;
    delete[] el.pSubpasses[i].pColorAttachments;
    delete[] el.pSubpasses[i].pResolveAttachments;
    if(el.pSubpasses[i].pPreserveAttachments)
      delete[] el.pSubpasses[i].pPreserveAttachments;
  }
  delete[] el.pSubpasses;
  delete[] el.pDependencies;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRenderPassBeginInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(renderPass);
  SERIALISE_MEMBER(framebuffer);
  SERIALISE_MEMBER(renderArea);
  SERIALISE_MEMBER_ARRAY(pClearValues, clearValueCount);
}

template <>
void Deserialise(const VkRenderPassBeginInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pClearValues;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkVertexInputBindingDescription &el)
{
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(stride);
  SERIALISE_MEMBER(inputRate);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkVertexInputAttributeDescription &el)
{
  SERIALISE_MEMBER(location);
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(offset);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineVertexInputStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER_ARRAY(pVertexBindingDescriptions, vertexBindingDescriptionCount);
  SERIALISE_MEMBER_ARRAY(pVertexAttributeDescriptions, vertexAttributeDescriptionCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineInputAssemblyStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(topology);
  SERIALISE_MEMBER(primitiveRestartEnable);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineTessellationStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(patchControlPoints);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineViewportStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);

  SERIALISE_MEMBER_ARRAY(pViewports, viewportCount);
  SERIALISE_MEMBER_ARRAY(pScissors, scissorCount);

  // need to handle these arrays potentially being NULL if they're dynamic, we still want the count
  SERIALISE_MEMBER(viewportCount);
  SERIALISE_MEMBER(scissorCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineRasterizationStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(depthClampEnable);
  SERIALISE_MEMBER(rasterizerDiscardEnable);
  SERIALISE_MEMBER(polygonMode);
  SERIALISE_MEMBER(cullMode);
  SERIALISE_MEMBER(frontFace);
  SERIALISE_MEMBER(depthBiasEnable);
  SERIALISE_MEMBER(depthBiasConstantFactor);
  SERIALISE_MEMBER(depthBiasClamp);
  SERIALISE_MEMBER(depthBiasSlopeFactor);
  SERIALISE_MEMBER(lineWidth);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineMultisampleStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(rasterizationSamples);
  RDCASSERT(el.rasterizationSamples <= VK_SAMPLE_COUNT_32_BIT);
  SERIALISE_MEMBER(sampleShadingEnable);
  SERIALISE_MEMBER(minSampleShading);
  SERIALISE_MEMBER_OPT(pSampleMask);
  SERIALISE_MEMBER(alphaToCoverageEnable);
  SERIALISE_MEMBER(alphaToOneEnable);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineColorBlendAttachmentState &el)
{
  SERIALISE_MEMBER(blendEnable);
  SERIALISE_MEMBER(srcColorBlendFactor);
  SERIALISE_MEMBER(dstColorBlendFactor);
  SERIALISE_MEMBER(colorBlendOp);
  SERIALISE_MEMBER(srcAlphaBlendFactor);
  SERIALISE_MEMBER(dstAlphaBlendFactor);
  SERIALISE_MEMBER(alphaBlendOp);
  SERIALISE_MEMBER(colorWriteMask);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineColorBlendStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(logicOpEnable);
  SERIALISE_MEMBER(logicOp);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
  SERIALISE_MEMBER(blendConstants);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineDepthStencilStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(depthTestEnable);
  SERIALISE_MEMBER(depthWriteEnable);
  SERIALISE_MEMBER(depthCompareOp);
  SERIALISE_MEMBER(depthBoundsTestEnable);
  SERIALISE_MEMBER(stencilTestEnable);
  SERIALISE_MEMBER(front);
  SERIALISE_MEMBER(back);
  SERIALISE_MEMBER(minDepthBounds);
  SERIALISE_MEMBER(maxDepthBounds);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineDynamicStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER_ARRAY(pDynamicStates, dynamicStateCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandPoolCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkCommandPoolCreateFlagBits, flags);
  SERIALISE_MEMBER(queueFamilyIndex);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandBufferAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(commandPool);
  SERIALISE_MEMBER(level);
  SERIALISE_MEMBER(commandBufferCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandBufferInheritanceInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(renderPass);
  SERIALISE_MEMBER(subpass);
  SERIALISE_MEMBER(framebuffer);
  SERIALISE_MEMBER(occlusionQueryEnable);
  SERIALISE_MEMBER_TYPED(VkQueryControlFlagBits, queryFlags);
  SERIALISE_MEMBER_TYPED(VkQueryPipelineStatisticFlagBits, pipelineStatistics);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandBufferBeginInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkCommandBufferUsageFlagBits, flags);
  SERIALISE_MEMBER_OPT(pInheritanceInfo);
}

template <>
void Deserialise(const VkCommandBufferBeginInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete el.pInheritanceInfo;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkStencilOpState &el)
{
  SERIALISE_MEMBER(failOp);
  SERIALISE_MEMBER(passOp);
  SERIALISE_MEMBER(depthFailOp);
  SERIALISE_MEMBER(compareOp);
  SERIALISE_MEMBER(compareMask);
  SERIALISE_MEMBER(writeMask);
  SERIALISE_MEMBER(reference);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkQueryPoolCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(queryType);
  SERIALISE_MEMBER(queryCount);
  SERIALISE_MEMBER_TYPED(VkQueryPipelineStatisticFlagBits, pipelineStatistics);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSemaphoreCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkEventCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFenceCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFenceCreateFlagBits, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSamplerCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(minFilter);
  SERIALISE_MEMBER(magFilter);
  SERIALISE_MEMBER(mipmapMode);
  SERIALISE_MEMBER(addressModeU);
  SERIALISE_MEMBER(addressModeV);
  SERIALISE_MEMBER(addressModeW);
  SERIALISE_MEMBER(mipLodBias);
  SERIALISE_MEMBER(anisotropyEnable);
  SERIALISE_MEMBER(maxAnisotropy);
  SERIALISE_MEMBER(compareEnable);
  SERIALISE_MEMBER(compareOp);
  SERIALISE_MEMBER(minLod);
  SERIALISE_MEMBER(maxLod);
  SERIALISE_MEMBER(borderColor);
  SERIALISE_MEMBER(unnormalizedCoordinates);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineShaderStageCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(module);
  SERIALISE_MEMBER(pName);
  SERIALISE_MEMBER_OPT(pSpecializationInfo);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSpecializationMapEntry &el)
{
  SERIALISE_MEMBER(constantID);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(constantID);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t size = el.size;
    ser.Serialise("size", size);
    if(ser.IsReading())
      el.size = (size_t)size;
  }
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSpecializationInfo &el)
{
  SERIALISE_MEMBER_ARRAY(pData, dataSize);
  SERIALISE_MEMBER_ARRAY(pMapEntries, mapEntryCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineCacheCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t initialDataSize = el.initialDataSize;
    ser.Serialise("initialDataSize", initialDataSize);
    if(ser.IsReading())
      el.initialDataSize = (size_t)initialDataSize;
  }

  SERIALISE_MEMBER_ARRAY(pInitialData, initialDataSize);
}

template <>
void Deserialise(const VkPipelineCacheCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  FreeAlignedBuffer((byte *)el.pInitialData);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineLayoutCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER_ARRAY(pSetLayouts, setLayoutCount);
  SERIALISE_MEMBER_ARRAY(pPushConstantRanges, pushConstantRangeCount);
}

template <>
void Deserialise(const VkPipelineLayoutCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pSetLayouts;
  delete[] el.pPushConstantRanges;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkShaderModuleCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);

  // serialise as void* so it goes through as a buffer, not an actual array of integers.
  {
    const void *pCode = el.pCode;
    ser.Serialise("pCode", pCode, el.codeSize, SerialiserFlags::AllocateMemory);
    if(ser.IsReading())
      el.pCode = (uint32_t *)pCode;
  }
}

template <>
void Deserialise(const VkShaderModuleCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  FreeAlignedBuffer((byte *)el.pCode);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSubresourceRange &el)
{
  SERIALISE_MEMBER_TYPED(VkImageAspectFlagBits, aspectMask);
  SERIALISE_MEMBER(baseMipLevel);
  SERIALISE_MEMBER(levelCount);
  SERIALISE_MEMBER(baseArrayLayer);
  SERIALISE_MEMBER(layerCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSubresourceLayers &el)
{
  SERIALISE_MEMBER_TYPED(VkImageAspectFlagBits, aspectMask);
  SERIALISE_MEMBER(mipLevel);
  SERIALISE_MEMBER(baseArrayLayer);
  SERIALISE_MEMBER(layerCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSubresource &el)
{
  SERIALISE_MEMBER_TYPED(VkImageAspectFlagBits, aspectMask);
  SERIALISE_MEMBER(mipLevel);
  SERIALISE_MEMBER(arrayLayer);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(allocationSize);
  SERIALISE_MEMBER(memoryTypeIndex);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryBarrier &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_BARRIER);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, srcAccessMask);
  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, dstAccessMask);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferMemoryBarrier &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, srcAccessMask);
  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, dstAccessMask);
  // serialise as signed because then QUEUE_FAMILY_IGNORED is -1 and queue
  // family index won't be legitimately larger than 2 billion
  SERIALISE_MEMBER_TYPED(int32_t, srcQueueFamilyIndex);
  SERIALISE_MEMBER_TYPED(int32_t, dstQueueFamilyIndex);
  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageMemoryBarrier &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, srcAccessMask);
  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, dstAccessMask);
  SERIALISE_MEMBER(oldLayout);
  SERIALISE_MEMBER(newLayout);
  // serialise as signed because then QUEUE_FAMILY_IGNORED is -1 and queue
  // family index won't be legitimately larger than 2 billion
  SERIALISE_MEMBER_TYPED(int32_t, srcQueueFamilyIndex);
  SERIALISE_MEMBER_TYPED(int32_t, dstQueueFamilyIndex);
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(subresourceRange);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkGraphicsPipelineCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkPipelineCreateFlagBits, flags);
  SERIALISE_MEMBER(layout);
  SERIALISE_MEMBER(renderPass);
  SERIALISE_MEMBER(subpass);
  SERIALISE_MEMBER(basePipelineHandle);
  SERIALISE_MEMBER(basePipelineIndex);

  SERIALISE_MEMBER_OPT(pVertexInputState);
  SERIALISE_MEMBER_OPT(pInputAssemblyState);
  SERIALISE_MEMBER_OPT(pTessellationState);
  SERIALISE_MEMBER_OPT(pViewportState);
  SERIALISE_MEMBER_OPT(pRasterizationState);
  SERIALISE_MEMBER_OPT(pMultisampleState);
  SERIALISE_MEMBER_OPT(pDepthStencilState);
  SERIALISE_MEMBER_OPT(pColorBlendState);
  SERIALISE_MEMBER_OPT(pDynamicState);
  SERIALISE_MEMBER_ARRAY(pStages, stageCount);
}

template <>
void Deserialise(const VkGraphicsPipelineCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  if(el.pVertexInputState)
  {
    RDCASSERT(el.pVertexInputState->pNext == NULL);    // otherwise delete
    delete[] el.pVertexInputState->pVertexBindingDescriptions;
    delete[] el.pVertexInputState->pVertexAttributeDescriptions;
    delete el.pVertexInputState;
  }
  if(el.pInputAssemblyState)
  {
    RDCASSERT(el.pInputAssemblyState->pNext == NULL);    // otherwise delete
    delete el.pInputAssemblyState;
  }
  if(el.pTessellationState)
  {
    RDCASSERT(el.pTessellationState->pNext == NULL);    // otherwise delete
    delete el.pTessellationState;
  }
  if(el.pViewportState)
  {
    RDCASSERT(el.pViewportState->pNext == NULL);    // otherwise delete
    if(el.pViewportState->pViewports)
      delete[] el.pViewportState->pViewports;
    if(el.pViewportState->pScissors)
      delete[] el.pViewportState->pScissors;
    delete el.pViewportState;
  }
  if(el.pRasterizationState)
  {
    RDCASSERT(el.pRasterizationState->pNext == NULL);    // otherwise delete
    delete el.pRasterizationState;
  }
  if(el.pMultisampleState)
  {
    RDCASSERT(el.pMultisampleState->pNext == NULL);    // otherwise delete
    delete el.pMultisampleState->pSampleMask;
    delete el.pMultisampleState;
  }
  if(el.pDepthStencilState)
  {
    RDCASSERT(el.pDepthStencilState->pNext == NULL);    // otherwise delete
    delete el.pDepthStencilState;
  }
  if(el.pColorBlendState)
  {
    RDCASSERT(el.pColorBlendState->pNext == NULL);    // otherwise delete
    delete[] el.pColorBlendState->pAttachments;
    delete el.pColorBlendState;
  }
  if(el.pDynamicState)
  {
    RDCASSERT(el.pDynamicState->pNext == NULL);    // otherwise delete
    if(el.pDynamicState->pDynamicStates)
      delete[] el.pDynamicState->pDynamicStates;
    delete el.pDynamicState;
  }
  for(uint32_t i = 0; i < el.stageCount; i++)
  {
    RDCASSERT(el.pStages[i].pNext == NULL);    // otherwise delete
    if(el.pStages[i].pSpecializationInfo)
    {
      FreeAlignedBuffer((byte *)el.pStages[i].pSpecializationInfo->pData);
      delete[] el.pStages[i].pSpecializationInfo->pMapEntries;
      delete el.pStages[i].pSpecializationInfo;
    }
  }
  delete[] el.pStages;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkComputePipelineCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER_TYPED(VkPipelineCreateFlagBits, flags);
  SERIALISE_MEMBER(layout);
  SERIALISE_MEMBER(basePipelineHandle);
  SERIALISE_MEMBER(basePipelineIndex);
}

template <>
void Deserialise(const VkComputePipelineCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);          // otherwise delete
  RDCASSERT(el.stage.pNext == NULL);    // otherwise delete
  if(el.stage.pSpecializationInfo)
  {
    FreeAlignedBuffer((byte *)(el.stage.pSpecializationInfo->pData));
    delete[] el.stage.pSpecializationInfo->pMapEntries;
    delete el.stage.pSpecializationInfo;
  }
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorPoolSize &el)
{
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(descriptorCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorPoolCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkDescriptorPoolCreateFlagBits, flags);
  SERIALISE_MEMBER(maxSets);
  SERIALISE_MEMBER_ARRAY(pPoolSizes, poolSizeCount);
}

template <>
void Deserialise(const VkDescriptorPoolCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pPoolSizes;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(descriptorPool);
  SERIALISE_MEMBER_ARRAY(pSetLayouts, descriptorSetCount);
}

template <>
void Deserialise(const VkDescriptorSetAllocateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  delete[] el.pSetLayouts;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorImageInfo &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded some updates to it
  OPTIONAL_RESOURCES();

  SERIALISE_MEMBER(sampler);
  SERIALISE_MEMBER(imageView);
  SERIALISE_MEMBER(imageLayout);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorBufferInfo &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded some updates to it
  OPTIONAL_RESOURCES();

  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(range);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkWriteDescriptorSet &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded some updates to it
  OPTIONAL_RESOURCES();

  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(dstSet);
  SERIALISE_MEMBER(dstBinding);
  SERIALISE_MEMBER(dstArrayElement);
  SERIALISE_MEMBER(descriptorType);

  if(ser.IsReading())
  {
    el.pImageInfo = NULL;
    el.pBufferInfo = NULL;
    el.pTexelBufferView = NULL;
  }

  // only serialise the array type used, the others are ignored
  if(el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
  {
    SERIALISE_MEMBER_ARRAY(pImageInfo, descriptorCount);
  }
  else if(el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
  {
    SERIALISE_MEMBER_ARRAY(pBufferInfo, descriptorCount);
  }
  else if(el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
  {
    SERIALISE_MEMBER_ARRAY(pTexelBufferView, descriptorCount);
  }
}

template <>
void Deserialise(const VkWriteDescriptorSet &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  if(el.pImageInfo)
    delete[] el.pImageInfo;
  if(el.pBufferInfo)
    delete[] el.pBufferInfo;
  if(el.pTexelBufferView)
    delete[] el.pTexelBufferView;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCopyDescriptorSet &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded some copies to or from it
  OPTIONAL_RESOURCES();

  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(srcSet);
  SERIALISE_MEMBER(srcBinding);
  SERIALISE_MEMBER(srcArrayElement);
  SERIALISE_MEMBER(dstSet);
  SERIALISE_MEMBER(dstBinding);
  SERIALISE_MEMBER(dstArrayElement);
  SERIALISE_MEMBER(descriptorCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPushConstantRange &el)
{
  SERIALISE_MEMBER_TYPED(VkShaderStageFlagBits, stageFlags);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutBinding &el)
{
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(descriptorType);
  SERIALISE_MEMBER_TYPED(VkShaderStageFlagBits, stageFlags);
  SERIALISE_MEMBER_ARRAY(pImmutableSamplers, descriptorCount);

  // serialise count separately after, as if pImmutableSamplers is NULL, count would be set to 0
  SERIALISE_MEMBER(descriptorCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER_ARRAY(pBindings, bindingCount);
}

template <>
void Deserialise(const VkDescriptorSetLayoutCreateInfo &el)
{
  RDCASSERT(el.pNext == NULL);    // otherwise delete
  for(uint32_t i = 0; i < el.bindingCount; i++)
    if(el.pBindings[i].pImmutableSamplers)
      delete[] el.pBindings[i].pImmutableSamplers;
  delete[] el.pBindings;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkComponentMapping &el)
{
  SERIALISE_MEMBER(r);
  SERIALISE_MEMBER(g);
  SERIALISE_MEMBER(b);
  SERIALISE_MEMBER(a);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMappedMemoryRange &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferImageCopy &el)
{
  SERIALISE_MEMBER(bufferOffset);
  SERIALISE_MEMBER(bufferRowLength);
  SERIALISE_MEMBER(bufferImageHeight);
  SERIALISE_MEMBER(imageSubresource);
  SERIALISE_MEMBER(imageOffset);
  SERIALISE_MEMBER(imageExtent);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferCopy &el)
{
  SERIALISE_MEMBER(srcOffset);
  SERIALISE_MEMBER(dstOffset);
  SERIALISE_MEMBER(size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageCopy &el)
{
  SERIALISE_MEMBER(srcSubresource);
  SERIALISE_MEMBER(srcOffset);
  SERIALISE_MEMBER(dstSubresource);
  SERIALISE_MEMBER(dstOffset);
  SERIALISE_MEMBER(extent);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageBlit &el)
{
  SERIALISE_MEMBER(srcSubresource);
  SERIALISE_MEMBER(srcOffsets);
  SERIALISE_MEMBER(dstSubresource);
  SERIALISE_MEMBER(dstOffsets);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageResolve &el)
{
  SERIALISE_MEMBER(srcSubresource);
  SERIALISE_MEMBER(srcOffset);
  SERIALISE_MEMBER(dstSubresource);
  SERIALISE_MEMBER(dstOffset);
  SERIALISE_MEMBER(extent);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkClearColorValue &el)
{
  SERIALISE_MEMBER(uint32);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkClearDepthStencilValue &el)
{
  SERIALISE_MEMBER(depth);
  SERIALISE_MEMBER(stencil);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkClearValue &el)
{
  SERIALISE_MEMBER(depthStencil);
  SERIALISE_MEMBER(color);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkClearRect &el)
{
  SERIALISE_MEMBER(rect);
  SERIALISE_MEMBER(baseArrayLayer);
  SERIALISE_MEMBER(layerCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkClearAttachment &el)
{
  SERIALISE_MEMBER(aspectMask);
  SERIALISE_MEMBER(colorAttachment);
  SERIALISE_MEMBER(clearValue);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRect2D &el)
{
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(extent);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkOffset2D &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkOffset3D &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(z);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExtent2D &el)
{
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExtent3D &el)
{
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(depth);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkViewport &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(minDepth);
  SERIALISE_MEMBER(maxDepth);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSwapchainCreateInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);

  // don't need the surface

  SERIALISE_MEMBER(minImageCount);
  SERIALISE_MEMBER(imageFormat);
  SERIALISE_MEMBER(imageColorSpace);
  SERIALISE_MEMBER(imageExtent);
  SERIALISE_MEMBER(imageArrayLayers);
  SERIALISE_MEMBER(imageUsage);
  SERIALISE_MEMBER(imageSharingMode);

  // SHARING: queueFamilyCount, pQueueFamilyIndices

  SERIALISE_MEMBER(preTransform);
  SERIALISE_MEMBER(compositeAlpha);
  SERIALISE_MEMBER(presentMode);
  SERIALISE_MEMBER(clipped);

  // don't need the old swap chain
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugMarkerMarkerInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(pMarkerName);
  SERIALISE_MEMBER(color);
}

// this isn't a real vulkan type, it's our own "anything that could be in a descriptor"
// structure that
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DescriptorSetSlot &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded the contents of it
  OPTIONAL_RESOURCES();

  SERIALISE_MEMBER(bufferInfo);
  SERIALISE_MEMBER(imageInfo);
  SERIALISE_MEMBER(texelBufferView);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ImageRegionState &el)
{
  SERIALISE_MEMBER(subresourceRange);
  SERIALISE_MEMBER(oldLayout);
  SERIALISE_MEMBER(newLayout);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ImageLayouts &el)
{
  SERIALISE_MEMBER(subresourceStates);
  SERIALISE_MEMBER(layerCount);
  SERIALISE_MEMBER(levelCount);
  SERIALISE_MEMBER(sampleCount);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(format);
}

INSTANTIATE_SERIALISE_TYPE(VkOffset2D);
INSTANTIATE_SERIALISE_TYPE(VkExtent2D);
INSTANTIATE_SERIALISE_TYPE(VkMemoryType);
INSTANTIATE_SERIALISE_TYPE(VkMemoryHeap);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceLimits);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSparseProperties);
INSTANTIATE_SERIALISE_TYPE(VkQueueFamilyProperties);
INSTANTIATE_SERIALISE_TYPE(VkExtent3D);
INSTANTIATE_SERIALISE_TYPE(VkPipelineShaderStageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkOffset3D);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferInheritanceInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineVertexInputStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseBufferMemoryBindInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageOpaqueMemoryBindInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageMemoryBindInfo);
INSTANTIATE_SERIALISE_TYPE(VkAttachmentDescription);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDescription);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDependency);
INSTANTIATE_SERIALISE_TYPE(VkClearValue);
INSTANTIATE_SERIALISE_TYPE(VkClearColorValue);
INSTANTIATE_SERIALISE_TYPE(VkClearDepthStencilValue);
INSTANTIATE_SERIALISE_TYPE(VkClearAttachment);
INSTANTIATE_SERIALISE_TYPE(VkClearRect);
INSTANTIATE_SERIALISE_TYPE(VkViewport);
INSTANTIATE_SERIALISE_TYPE(VkPipelineColorBlendAttachmentState);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorPoolSize);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorImageInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorBufferInfo);
INSTANTIATE_SERIALISE_TYPE(VkSpecializationInfo);
INSTANTIATE_SERIALISE_TYPE(VkAttachmentReference);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageMemoryBind);
INSTANTIATE_SERIALISE_TYPE(VkVertexInputBindingDescription);
INSTANTIATE_SERIALISE_TYPE(VkVertexInputAttributeDescription);
INSTANTIATE_SERIALISE_TYPE(VkSpecializationMapEntry);
INSTANTIATE_SERIALISE_TYPE(VkRect2D);
INSTANTIATE_SERIALISE_TYPE(VkDeviceQueueCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMemoryProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceProperties);
INSTANTIATE_SERIALISE_TYPE(VkDeviceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkBufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkBufferViewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageViewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseMemoryBind);
INSTANTIATE_SERIALISE_TYPE(VkBindSparseInfo);
INSTANTIATE_SERIALISE_TYPE(VkSubmitInfo);
INSTANTIATE_SERIALISE_TYPE(VkFramebufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineInputAssemblyStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineTessellationStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineViewportStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineRasterizationStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineMultisampleStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineDepthStencilStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineColorBlendStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineDynamicStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineLayoutCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPushConstantRange);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetLayoutBinding);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetLayoutCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorPoolCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkWriteDescriptorSet);
INSTANTIATE_SERIALISE_TYPE(VkCopyDescriptorSet);
INSTANTIATE_SERIALISE_TYPE(VkCommandPoolCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkStencilOpState);
INSTANTIATE_SERIALISE_TYPE(VkQueryPoolCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSemaphoreCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkEventCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkFenceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSamplerCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineCacheCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkShaderModuleCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageSubresourceRange);
INSTANTIATE_SERIALISE_TYPE(VkImageSubresource);
INSTANTIATE_SERIALISE_TYPE(VkImageSubresourceLayers);
INSTANTIATE_SERIALISE_TYPE(VkMemoryAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkMemoryBarrier);
INSTANTIATE_SERIALISE_TYPE(VkBufferMemoryBarrier);
INSTANTIATE_SERIALISE_TYPE(VkImageMemoryBarrier);
INSTANTIATE_SERIALISE_TYPE(VkGraphicsPipelineCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkComputePipelineCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkComponentMapping);
INSTANTIATE_SERIALISE_TYPE(VkMappedMemoryRange);
INSTANTIATE_SERIALISE_TYPE(VkBufferImageCopy);
INSTANTIATE_SERIALISE_TYPE(VkBufferCopy);
INSTANTIATE_SERIALISE_TYPE(VkImageCopy);
INSTANTIATE_SERIALISE_TYPE(VkImageBlit);
INSTANTIATE_SERIALISE_TYPE(VkImageResolve);
INSTANTIATE_SERIALISE_TYPE(VkSwapchainCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkDebugMarkerMarkerInfoEXT);

INSTANTIATE_SERIALISE_TYPE(DescriptorSetSlot);
INSTANTIATE_SERIALISE_TYPE(ImageRegionState);
INSTANTIATE_SERIALISE_TYPE(ImageLayouts);