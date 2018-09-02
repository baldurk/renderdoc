/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

  if(ser.IsReading() && rm && !IsStructuredExporting(rm->GetState()))
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
        RDCWARN("Capture may be missing reference to %s resource (%llu).", TypeName<type>(), id);
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

#if ENABLED(RDOC_WIN32)

#define HANDLE_PNEXT_OS()                                                                             \
  /* VK_NV_external_memory_win32 */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV, VkImportMemoryWin32HandleInfoNV) \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV, VkExportMemoryWin32HandleInfoNV) \
                                                                                                      \
  /* VK_NV_win32_keyed_mutex */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV,                           \
               VkWin32KeyedMutexAcquireReleaseInfoNV)                                                 \
                                                                                                      \
  /* VK_KHR_win32_keyed_mutex */                                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR,                          \
               VkWin32KeyedMutexAcquireReleaseInfoKHR)                                                \
                                                                                                      \
  /* VK_KHR_external_memory_win32 */                                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,                                 \
               VkImportMemoryWin32HandleInfoKHR)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,                                 \
               VkImportMemoryWin32HandleInfoKHR)                                                      \
                                                                                                      \
  /* VK_KHR_external_semaphore_win32 */                                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,                              \
               VkExportSemaphoreWin32HandleInfoKHR)                                                   \
                                                                                                      \
  /* VK_KHR_external_fence_win32 */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR, VkExportFenceWin32HandleInfoKHR)

#else

#define HANDLE_PNEXT_OS()

#endif

// pNext structure type dispatch
#define HANDLE_PNEXT()                                                                               \
  /* we can ignore all external memory extension structs entirely. We don't need to       */         \
  /* serialise or replay it as we won't actually use external memory and will just create */         \
  /* normal memory to replay with. */                                                                \
                                                                                                     \
  /* OS-specific extensions */                                                                       \
  HANDLE_PNEXT_OS()                                                                                  \
                                                                                                     \
  /* VK_NV_external_memory */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV, VkExportMemoryAllocateInfoNV)       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV,                               \
               VkExternalMemoryImageCreateInfoNV)                                                    \
                                                                                                     \
  /* VK_KHR_external_memory / ..._fd  */                                                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, VkExportMemoryAllocateInfo)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, VkExternalMemoryImageCreateInfo) \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,                                 \
               VkExternalMemoryBufferCreateInfo)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, VkImportMemoryFdInfoKHR)                 \
                                                                                                     \
  /* VK_KHR_external_semaphore */                                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, VkExportSemaphoreCreateInfo)          \
                                                                                                     \
  /* VK_KHR_external_fence */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO, VkExportFenceCreateInfo)                  \
                                                                                                     \
  /* we don't create real swapchains on replay, so we can ignore surface counters */                 \
  /* VK_EXT_display_control */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT, VkSwapchainCounterCreateInfoEXT) \
                                                                                                     \
  /* for now we don't serialise dedicated memory on replay as it's only a performance */             \
  /* hint, and is only required in conjunction with shared memory (which we don't */                 \
  /* replay). In future it might be helpful to serialise this for informational purposes. */         \
                                                                                                     \
  /* VK_NV_dedicated_allocation */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV,                       \
               VkDedicatedAllocationMemoryAllocateInfoNV)                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV,                          \
               VkDedicatedAllocationImageCreateInfoNV)                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV,                         \
               VkDedicatedAllocationBufferCreateInfoNV)                                              \
                                                                                                     \
  /* VK_KHR_dedicated_allocation */                                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, VkMemoryDedicatedAllocateInfo)      \
                                                                                                     \
  /* VK_EXT_global_priority */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,                       \
               VkDeviceQueueGlobalPriorityCreateInfoEXT)                                             \
                                                                                                     \
  /* for now we don't support ycbcr and force-disable the feature bit, so no-one should try */       \
  /* to pNext any of these structs anyway, but if they do, we ignore them */                         \
  /* VK_KHR_sampler_ycbcr_conversion */                                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, VkBindImagePlaneMemoryInfo)           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, VkSamplerYcbcrConversionInfo)        \
                                                                                                     \
  /* for now we don't handle true device groups and report all physdevices in separate groups. */    \
  /* So we can safely ignore these structures as redundant/unneeded. */                              \
  /* VK_KHR_device_group */                                                                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR,                             \
               VkDeviceGroupSwapchainCreateInfoKHR)                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,                               \
               VkBindImageMemorySwapchainInfoKHR)                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR, VkImageSwapchainCreateInfoKHR)     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO,                                \
               VkBindImageMemoryDeviceGroupInfo)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO,                               \
               VkBindBufferMemoryDeviceGroupInfo)                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, VkDeviceGroupBindSparseInfo)         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO, VkDeviceGroupSubmitInfo)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO,                             \
               VkDeviceGroupCommandBufferBeginInfo)                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,                                \
               VkDeviceGroupRenderPassBeginInfo)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, VkMemoryAllocateFlagsInfo)              \
                                                                                                     \
  /* Vulkan 1.1 - protected memory */                                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO, VkProtectedSubmitInfo)                       \
                                                                                                     \
  /* VK_EXT_conservative_rasterization */                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,          \
               VkPipelineRasterizationConservativeStateCreateInfoEXT)                                \
                                                                                                     \
  /* VK_KHR_maintenance2 */                                                                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,              \
               VkPipelineTessellationDomainOriginStateCreateInfo)                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, VkImageViewUsageCreateInfo)           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO,                    \
               VkRenderPassInputAttachmentAspectCreateInfo)                                          \
                                                                                                     \
  /* VK_EXT_vertex_attribute_divisor */                                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,                \
               VkPipelineVertexInputDivisorStateCreateInfoEXT)                                       \
                                                                                                     \
  /* VK_EXT_sampler_filter_minmax */                                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,                             \
               VkSamplerReductionModeCreateInfoEXT)                                                  \
                                                                                                     \
  /* VK_KHR_multiview */                                                                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, VkRenderPassMultiviewCreateInfo) \
                                                                                                     \
  /* VK_KHR_image_format_list */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR, VkImageFormatListCreateInfoKHR)

template <typename SerialiserType>
static void SerialiseNext(SerialiserType &ser, VkStructureType &sType, const void *&pNext)
{
  // this is the parent sType, serialised here for convenience
  ser.Serialise("sType", sType);

  if(ser.IsReading())
  {
    // default to a NULL pNext
    pNext = NULL;

    // serialise a nullable next type, to tell us the type of object that's serialised. This is
    // hidden as it doesn't correspond to the actual pNext, it's just metadata to help us launch the
    // serialisation.
    VkStructureType *nextType = NULL;
    ser.SerialiseNullable("pNext", nextType);

    // most common case - no pNext serialised. Bail immediately
    if(nextType == NULL)
    {
      // fixup the structured data, set the type to void*
      ser.TypedAs("void *");
      return;
    }

    // hide and rename the pNextType if we got something - we'll want the pNext below to be the only
    // thing user-facing.
    ser.Named("pNextType").Hidden();

// if we come across a struct we should process, then serialise a pointer to it.
#define PNEXT_STRUCT(StructType, StructName)    \
  case StructType:                              \
  {                                             \
    StructName *nextStruct = NULL;              \
    ser.SerialiseNullable("pNext", nextStruct); \
    pNext = nextStruct;                         \
    break;                                      \
  }

    // this serialises the pNext with the right type, as nullable. We already know from above that
    // there IS something here, so the nullable is redundant but convenient
    switch(*nextType)
    {
      HANDLE_PNEXT();
      default:
      {
        RDCERR("Unexpected/unhandled next structure type %s", ToStr(*nextType).c_str());
        break;
      }
    }

    // delete the type itself. Any pNext we serialised is saved in the pNext pointer and will be
    // deleted in DeserialiseNext()
    delete nextType;

    // note, we don't have to serialise more of the chain - this is recursive, if there was more of
    // the pNext chain it would be done recursively above
  }
  else    // ser.IsWriting()
  {
// if we come across a struct we should process, then serialise a pointer to its type (to tell the
// reading serialisation what struct is coming up), then a pointer to it.
// We don't have to go any further, the act of serialising this struct will walk the chain further,
// so we can return immediately.
#undef PNEXT_STRUCT
#define PNEXT_STRUCT(StructType, StructName)      \
  case StructType:                                \
  {                                               \
    VkStructureType *nextType = &next->sType;     \
    ser.SerialiseNullable("pNextType", nextType); \
    StructName *actual = (StructName *)next;      \
    ser.SerialiseNullable("pNext", actual);       \
    return;                                       \
  }

    // walk the pNext chain, skipping any structs we don't care about serialising.
    VkGenericStruct *next = (VkGenericStruct *)pNext;

    while(next)
    {
      switch(next->sType)
      {
        HANDLE_PNEXT();
        default:
        {
          RDCERR("Unrecognised extension structure type %s", ToStr(next->sType).c_str());
          break;
        }
      }

      // walk to the next item if we didn't serialise the current one
      next = (VkGenericStruct *)next->pNext;
    }

    // if we got here, either pNext was NULL (common) or we skipped the whole chain. Serialise a
    // NULL structure type to indicate that.
    VkStructureType *dummy = NULL;
    ser.SerialiseNullable("pNext", dummy);
  }
}

template <typename SerialiserType>
static void SerialiseNext(SerialiserType &ser, VkStructureType &sType, void *&pNext)
{
  const void *tmpNext = pNext;
  SerialiseNext(ser, sType, tmpNext);
  pNext = (void *)tmpNext;
}

static inline void DeserialiseNext(const void *pNext)
{
  if(pNext == NULL)
    return;

  // walk the chain, deserialising from the tail back
  const VkGenericStruct *gen = (const VkGenericStruct *)pNext;
  DeserialiseNext(gen->pNext);
  delete gen;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAllocationCallbacks &el)
{
  RDCERR("Serialising VkAllocationCallbacks - this should always be a NULL optional element");
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceQueueCreateInfo &el)
{
  if(ser.IsWriting() && el.sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
    RDCWARN("sType not set properly: %u", el.sType);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkDeviceQueueCreateFlagBits, flags);
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
  SERIALISE_MEMBER_TYPED(VkMemoryHeapFlagBits, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryType &el)
{
  SERIALISE_MEMBER_TYPED(VkMemoryPropertyFlagBits, propertyFlags);
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
  SERIALISE_MEMBER_TYPED(VkQueueFlagBits, queueFlags);
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
  SERIALISE_MEMBER(queueCreateInfoCount);
  SERIALISE_MEMBER_ARRAY(pQueueCreateInfos, queueCreateInfoCount);
  SERIALISE_MEMBER(enabledLayerCount);
  SERIALISE_MEMBER_ARRAY(ppEnabledLayerNames, enabledLayerCount);
  SERIALISE_MEMBER(enabledExtensionCount);
  SERIALISE_MEMBER_ARRAY(ppEnabledExtensionNames, enabledExtensionCount);
  SERIALISE_MEMBER_OPT(pEnabledFeatures);
}

template <>
void Deserialise(const VkDeviceCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  for(uint32_t i = 0; i < el.queueCreateInfoCount; i++)
  {
    DeserialiseNext(el.pQueueCreateInfos[i].pNext);
    delete[] el.pQueueCreateInfos[i].pQueuePriorities;
  }
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
    SERIALISE_MEMBER(queueFamilyIndexCount);
    SERIALISE_MEMBER_ARRAY(pQueueFamilyIndices, queueFamilyIndexCount);
  }
  else
  {
    // otherwise do a dummy serialise so the struct is the same either way
    SERIALISE_MEMBER_EMPTY(queueFamilyIndexCount);
    SERIALISE_MEMBER_OPT_EMPTY(pQueueFamilyIndices);
  }
}

template <>
void Deserialise(const VkBufferCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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

template <>
void Deserialise(const VkBufferViewCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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

  // pQueueFamilyIndices should *only* be read if the sharing mode is concurrent
  if(el.sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    SERIALISE_MEMBER(queueFamilyIndexCount);
    SERIALISE_MEMBER_ARRAY(pQueueFamilyIndices, queueFamilyIndexCount);
  }
  else
  {
    // otherwise do a dummy serialise so the struct is the same either way
    SERIALISE_MEMBER_EMPTY(queueFamilyIndexCount);
    SERIALISE_MEMBER_ARRAY_EMPTY(pQueueFamilyIndices);
  }

  SERIALISE_MEMBER(initialLayout);
}

template <>
void Deserialise(const VkImageCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pQueueFamilyIndices;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryRequirements &el)
{
  SERIALISE_MEMBER(size);
  SERIALISE_MEMBER(alignment);
  SERIALISE_MEMBER(memoryTypeBits);
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

template <>
void Deserialise(const VkImageViewCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER(bindCount);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageOpaqueMemoryBindInfo &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(bindCount);
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
  SERIALISE_MEMBER(bindCount);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindSparseInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_SPARSE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(waitSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pWaitSemaphores, waitSemaphoreCount);

  SERIALISE_MEMBER(bufferBindCount);
  SERIALISE_MEMBER_ARRAY(pBufferBinds, bufferBindCount);
  SERIALISE_MEMBER(imageOpaqueBindCount);
  SERIALISE_MEMBER_ARRAY(pImageOpaqueBinds, imageOpaqueBindCount);
  SERIALISE_MEMBER(imageBindCount);
  SERIALISE_MEMBER_ARRAY(pImageBinds, imageBindCount);

  SERIALISE_MEMBER(signalSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pSignalSemaphores, signalSemaphoreCount);
}

template <>
void Deserialise(const VkBindSparseInfo &el)
{
  DeserialiseNext(el.pNext);
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

  SERIALISE_MEMBER(waitSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pWaitSemaphores, waitSemaphoreCount);

  // bit of a hack, we alias the ptr here to the bits type so we serialise with better type info
  union
  {
    const VkPipelineStageFlagBits **typed;
    const VkPipelineStageFlags **orig;
  } u;
  u.orig = &el.pWaitDstStageMask;

  ser.Serialise("pWaitDstStageMask", *u.typed, el.waitSemaphoreCount,
                SerialiserFlags::AllocateMemory);

  SERIALISE_MEMBER(commandBufferCount);
  SERIALISE_MEMBER_ARRAY(pCommandBuffers, commandBufferCount);
  SERIALISE_MEMBER(signalSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pSignalSemaphores, signalSemaphoreCount);
}

template <>
void Deserialise(const VkSubmitInfo &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER(attachmentCount);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  SERIALISE_MEMBER(layers);
}

template <>
void Deserialise(const VkFramebufferCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER_TYPED(VkSubpassDescriptionFlagBits, flags);
  SERIALISE_MEMBER(pipelineBindPoint);

  SERIALISE_MEMBER(inputAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pInputAttachments, inputAttachmentCount);

  SERIALISE_MEMBER(colorAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pColorAttachments, colorAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pResolveAttachments, colorAttachmentCount);

  SERIALISE_MEMBER_OPT(pDepthStencilAttachment);

  SERIALISE_MEMBER(preserveAttachmentCount);
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
  SERIALISE_MEMBER(attachmentCount);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
  SERIALISE_MEMBER(subpassCount);
  SERIALISE_MEMBER_ARRAY(pSubpasses, subpassCount);
  SERIALISE_MEMBER(dependencyCount);
  SERIALISE_MEMBER_ARRAY(pDependencies, dependencyCount);
}

template <>
void Deserialise(const VkRenderPassCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER(clearValueCount);
  SERIALISE_MEMBER_ARRAY(pClearValues, clearValueCount);
}

template <>
void Deserialise(const VkRenderPassBeginInfo &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER(vertexBindingDescriptionCount);
  SERIALISE_MEMBER_ARRAY(pVertexBindingDescriptions, vertexBindingDescriptionCount);
  SERIALISE_MEMBER(vertexAttributeDescriptionCount);
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

  SERIALISE_MEMBER(viewportCount);
  SERIALISE_MEMBER_ARRAY(pViewports, viewportCount);
  SERIALISE_MEMBER(scissorCount);
  SERIALISE_MEMBER_ARRAY(pScissors, scissorCount);
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
  SERIALISE_MEMBER(attachmentCount);
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
  SERIALISE_MEMBER(dynamicStateCount);
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

template <>
void Deserialise(const VkCommandPoolCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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

template <>
void Deserialise(const VkCommandBufferAllocateInfo &el)
{
  DeserialiseNext(el.pNext);
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
  DeserialiseNext(el.pNext);
  if(el.pInheritanceInfo)
    DeserialiseNext(el.pInheritanceInfo->pNext);
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

template <>
void Deserialise(const VkQueryPoolCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSemaphoreCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
}

template <>
void Deserialise(const VkSemaphoreCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkEventCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
}

template <>
void Deserialise(const VkEventCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFenceCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFenceCreateFlagBits, flags);
}

template <>
void Deserialise(const VkFenceCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSamplerCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(magFilter);
  SERIALISE_MEMBER(minFilter);
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

template <>
void Deserialise(const VkSamplerCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER(mapEntryCount);
  SERIALISE_MEMBER_ARRAY(pMapEntries, mapEntryCount);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t dataSize = el.dataSize;
    ser.Serialise("dataSize", dataSize);
    if(ser.IsReading())
      el.dataSize = (size_t)dataSize;
  }

  SERIALISE_MEMBER_ARRAY(pData, dataSize);
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
  DeserialiseNext(el.pNext);
  FreeAlignedBuffer((byte *)el.pInitialData);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineLayoutCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(setLayoutCount);
  SERIALISE_MEMBER_ARRAY(pSetLayouts, setLayoutCount);
  SERIALISE_MEMBER(pushConstantRangeCount);
  SERIALISE_MEMBER_ARRAY(pPushConstantRanges, pushConstantRangeCount);
}

template <>
void Deserialise(const VkPipelineLayoutCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pSetLayouts;
  delete[] el.pPushConstantRanges;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkShaderModuleCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t codeSize = el.codeSize;
    ser.Serialise("codeSize", codeSize);
    if(ser.IsReading())
      el.codeSize = (size_t)codeSize;
  }

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
  DeserialiseNext(el.pNext);
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

template <>
void Deserialise(const VkMemoryAllocateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryBarrier &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_BARRIER);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, srcAccessMask);
  SERIALISE_MEMBER_TYPED(VkAccessFlagBits, dstAccessMask);
}

template <>
void Deserialise(const VkMemoryBarrier &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferMemoryBarrier &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
  SerialiseNext(ser, el.sType, el.pNext);

  // Resources in this struct are optional, because if we decided a resource wasn't used - we
  // might still have recorded some barriers on it
  OPTIONAL_RESOURCES();

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

template <>
void Deserialise(const VkBufferMemoryBarrier &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageMemoryBarrier &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
  SerialiseNext(ser, el.sType, el.pNext);

  // Resources in this struct are optional, because if we decided a resource wasn't used - we
  // might still have recorded some barriers on it
  OPTIONAL_RESOURCES();

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

template <>
void Deserialise(const VkImageMemoryBarrier &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkGraphicsPipelineCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkPipelineCreateFlagBits, flags);
  SERIALISE_MEMBER(stageCount);
  SERIALISE_MEMBER_ARRAY(pStages, stageCount);

  SERIALISE_MEMBER_OPT(pVertexInputState);
  SERIALISE_MEMBER_OPT(pInputAssemblyState);
  SERIALISE_MEMBER_OPT(pTessellationState);
  SERIALISE_MEMBER_OPT(pViewportState);
  SERIALISE_MEMBER_OPT(pRasterizationState);
  SERIALISE_MEMBER_OPT(pMultisampleState);
  SERIALISE_MEMBER_OPT(pDepthStencilState);
  SERIALISE_MEMBER_OPT(pColorBlendState);
  SERIALISE_MEMBER_OPT(pDynamicState);

  SERIALISE_MEMBER(layout);
  SERIALISE_MEMBER(renderPass);
  SERIALISE_MEMBER(subpass);
  SERIALISE_MEMBER(basePipelineHandle);
  SERIALISE_MEMBER(basePipelineIndex);
}

template <>
void Deserialise(const VkGraphicsPipelineCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  if(el.pVertexInputState)
  {
    DeserialiseNext(el.pVertexInputState->pNext);
    delete[] el.pVertexInputState->pVertexBindingDescriptions;
    delete[] el.pVertexInputState->pVertexAttributeDescriptions;
    delete el.pVertexInputState;
  }
  if(el.pInputAssemblyState)
  {
    DeserialiseNext(el.pInputAssemblyState->pNext);
    delete el.pInputAssemblyState;
  }
  if(el.pTessellationState)
  {
    DeserialiseNext(el.pTessellationState->pNext);
    delete el.pTessellationState;
  }
  if(el.pViewportState)
  {
    DeserialiseNext(el.pViewportState->pNext);
    if(el.pViewportState->pViewports)
      delete[] el.pViewportState->pViewports;
    if(el.pViewportState->pScissors)
      delete[] el.pViewportState->pScissors;
    delete el.pViewportState;
  }
  if(el.pRasterizationState)
  {
    DeserialiseNext(el.pRasterizationState->pNext);
    delete el.pRasterizationState;
  }
  if(el.pMultisampleState)
  {
    DeserialiseNext(el.pMultisampleState->pNext);
    delete el.pMultisampleState->pSampleMask;
    delete el.pMultisampleState;
  }
  if(el.pDepthStencilState)
  {
    DeserialiseNext(el.pDepthStencilState->pNext);
    delete el.pDepthStencilState;
  }
  if(el.pColorBlendState)
  {
    DeserialiseNext(el.pColorBlendState->pNext);
    delete[] el.pColorBlendState->pAttachments;
    delete el.pColorBlendState;
  }
  if(el.pDynamicState)
  {
    DeserialiseNext(el.pDynamicState->pNext);
    if(el.pDynamicState->pDynamicStates)
      delete[] el.pDynamicState->pDynamicStates;
    delete el.pDynamicState;
  }
  for(uint32_t i = 0; i < el.stageCount; i++)
  {
    DeserialiseNext(el.pStages[i].pNext);
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

  SERIALISE_MEMBER_TYPED(VkPipelineCreateFlagBits, flags);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(layout);
  SERIALISE_MEMBER(basePipelineHandle);
  SERIALISE_MEMBER(basePipelineIndex);
}

template <>
void Deserialise(const VkComputePipelineCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  DeserialiseNext(el.stage.pNext);
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
  SERIALISE_MEMBER(poolSizeCount);
  SERIALISE_MEMBER_ARRAY(pPoolSizes, poolSizeCount);
}

template <>
void Deserialise(const VkDescriptorPoolCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pPoolSizes;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(descriptorPool);
  SERIALISE_MEMBER(descriptorSetCount);
  SERIALISE_MEMBER_ARRAY(pSetLayouts, descriptorSetCount);
}

template <>
void Deserialise(const VkDescriptorSetAllocateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pSetLayouts;
}

enum class VkDescriptorImageInfoValidity
{
  Neither = 0x0,
  Sampler = 0x1,
  ImageView = 0x100,
};

BITMASK_OPERATORS(VkDescriptorImageInfoValidity);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorImageInfo &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded some updates to it
  OPTIONAL_RESOURCES();

  VkDescriptorImageInfoValidity validity = (VkDescriptorImageInfoValidity)ser.GetStructArg();

  RDCASSERT(validity != VkDescriptorImageInfoValidity::Neither, (uint64_t)validity);

  if(validity & VkDescriptorImageInfoValidity::Sampler)
    SERIALISE_MEMBER(sampler);
  else
    SERIALISE_MEMBER_EMPTY(sampler);

  if(validity & VkDescriptorImageInfoValidity::ImageView)
    SERIALISE_MEMBER(imageView);
  else
    SERIALISE_MEMBER_EMPTY(imageView);

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
  SERIALISE_MEMBER(descriptorCount);
  SERIALISE_MEMBER(descriptorType);

  // only serialise the array type used, the others are ignored
  if(el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
  {
    VkDescriptorImageInfoValidity validity = VkDescriptorImageInfoValidity::Neither;

    if(el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
       el.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      validity = validity | VkDescriptorImageInfoValidity::Sampler;

    if(el.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
       el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
       el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
       el.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      validity = validity | VkDescriptorImageInfoValidity::ImageView;

    // set the validity flags so the serialisation of VkDescriptorImageInfo knows which members are
    // safe to read. We pass this as just flags so the comparisons happen here once, not per-element
    // in this array
    ser.SetStructArg((uint64_t)validity);

    SERIALISE_MEMBER_ARRAY(pImageInfo, descriptorCount);
  }
  else
  {
    SERIALISE_MEMBER_ARRAY_EMPTY(pImageInfo);
  }

  if(el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
  {
    SERIALISE_MEMBER_ARRAY(pBufferInfo, descriptorCount);
  }
  else
  {
    SERIALISE_MEMBER_ARRAY_EMPTY(pBufferInfo);
  }

  if(el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
  {
    SERIALISE_MEMBER_ARRAY(pTexelBufferView, descriptorCount);
  }
  else
  {
    SERIALISE_MEMBER_ARRAY_EMPTY(pTexelBufferView);
  }
}

template <>
void Deserialise(const VkWriteDescriptorSet &el)
{
  DeserialiseNext(el.pNext);
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

template <>
void Deserialise(const VkCopyDescriptorSet &el)
{
  DeserialiseNext(el.pNext);
};

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
  SERIALISE_MEMBER(descriptorCount);
  SERIALISE_MEMBER_TYPED(VkShaderStageFlagBits, stageFlags);
  SERIALISE_MEMBER_ARRAY(pImmutableSamplers, descriptorCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkDescriptorSetLayoutCreateFlagBits, flags);
  SERIALISE_MEMBER(bindingCount);
  SERIALISE_MEMBER_ARRAY(pBindings, bindingCount);
}

template <>
void Deserialise(const VkDescriptorSetLayoutCreateInfo &el)
{
  DeserialiseNext(el.pNext);
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

template <>
void Deserialise(const VkMappedMemoryRange &el)
{
  DeserialiseNext(el.pNext);
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
  SERIALISE_MEMBER(float32);
  SERIALISE_MEMBER(int32);
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
  SERIALISE_MEMBER(color);
  SERIALISE_MEMBER(depthStencil);
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
  SERIALISE_MEMBER_TYPED(VkImageAspectFlagBits, aspectMask);
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

  SERIALISE_MEMBER_TYPED(VkSwapchainCreateFlagBitsKHR, flags);

  // don't need the surface
  SERIALISE_MEMBER_EMPTY(surface);

  SERIALISE_MEMBER(minImageCount);
  SERIALISE_MEMBER(imageFormat);
  SERIALISE_MEMBER(imageColorSpace);
  SERIALISE_MEMBER(imageExtent);
  SERIALISE_MEMBER(imageArrayLayers);
  SERIALISE_MEMBER(imageUsage);
  SERIALISE_MEMBER(imageSharingMode);

  // pQueueFamilyIndices should *only* be read if the sharing mode is concurrent, and if the capture
  // is new (old captures always ignored these fields)
  if(ser.VersionAtLeast(0xD) && el.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    SERIALISE_MEMBER(queueFamilyIndexCount);
    SERIALISE_MEMBER_ARRAY(pQueueFamilyIndices, queueFamilyIndexCount);
  }
  else
  {
    // otherwise do a dummy serialise so the struct is the same either way
    SERIALISE_MEMBER_EMPTY(queueFamilyIndexCount);
    SERIALISE_MEMBER_ARRAY_EMPTY(pQueueFamilyIndices);
  }

  SERIALISE_MEMBER(preTransform);
  SERIALISE_MEMBER(compositeAlpha);
  SERIALISE_MEMBER(presentMode);
  SERIALISE_MEMBER(clipped);

  // don't need the old swap chain
  SERIALISE_MEMBER_EMPTY(oldSwapchain);
}

template <>
void Deserialise(const VkSwapchainCreateInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugMarkerMarkerInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(pMarkerName);
  SERIALISE_MEMBER(color);
}

template <>
void Deserialise(const VkDebugMarkerMarkerInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

// this isn't a real vulkan type, it's our own "anything that could be in a descriptor"
// structure that
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DescriptorSetSlot &el)
{
  // Resources in this struct are optional, because if we decided a descriptor wasn't used - we
  // might still have recorded the contents of it
  OPTIONAL_RESOURCES();

  // all members are valid because it's either NULL or pointing at an existing element, it won't
  // point to garbage.
  ser.SetStructArg(
      uint64_t(VkDescriptorImageInfoValidity::Sampler | VkDescriptorImageInfoValidity::ImageView));

  SERIALISE_MEMBER(bufferInfo);
  SERIALISE_MEMBER(imageInfo);
  SERIALISE_MEMBER(texelBufferView);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ImageRegionState &el)
{
  if(ser.VersionAtLeast(0xD))
  {
    // added in 0xD
    SERIALISE_MEMBER(dstQueueFamilyIndex);
  }
  SERIALISE_MEMBER(subresourceRange);
  SERIALISE_MEMBER(oldLayout);
  SERIALISE_MEMBER(newLayout);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ImageLayouts &el)
{
  if(ser.VersionAtLeast(0xD))
  {
    // added in 0xD
    SERIALISE_MEMBER(queueFamilyIndex);
  }
  SERIALISE_MEMBER(subresourceStates);
  SERIALISE_MEMBER(layerCount);
  SERIALISE_MEMBER(levelCount);
  SERIALISE_MEMBER(sampleCount);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(format);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorUpdateTemplateEntry &el)
{
  SERIALISE_MEMBER(dstBinding);
  SERIALISE_MEMBER(dstArrayElement);
  SERIALISE_MEMBER(descriptorCount);
  SERIALISE_MEMBER(descriptorType);

// these fields are size_t and should not be serialised as-is. They're not used so we can just
// serialise them as uint64_t. Unfortunately this wasn't correct initially and they were
// serialised as-is making a 32-bit/64-bit incompatibility, so for older versions all we can do is
// continue to serialise them as size_t as it's impossible to know which one was used.
//
// On mac we can't compile a size_t serialise, which is good in general but makes this backwards
// compatibility a bit more annoying. We just assume a 64-bit capture.

#if DISABLED(RDOC_APPLE)
  if(ser.VersionAtLeast(0xE))
#endif
  {
    uint64_t offset = el.offset;
    uint64_t stride = el.stride;
    ser.Serialise("offset", offset);
    ser.Serialise("stride", stride);
    el.offset = (size_t)offset;
    el.stride = (size_t)stride;
  }
#if DISABLED(RDOC_APPLE)
  else
  {
    SERIALISE_MEMBER(offset);
    SERIALISE_MEMBER(stride);
  }
#endif
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorUpdateTemplateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(descriptorUpdateEntryCount);
  SERIALISE_MEMBER_ARRAY(pDescriptorUpdateEntries, descriptorUpdateEntryCount);
  SERIALISE_MEMBER(templateType);

  if(el.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
  {
    SERIALISE_MEMBER(descriptorSetLayout);
  }
  else
  {
    SERIALISE_MEMBER_EMPTY(descriptorSetLayout);
  }

  if(el.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR)
  {
    SERIALISE_MEMBER(pipelineBindPoint);
    SERIALISE_MEMBER(pipelineLayout);
    SERIALISE_MEMBER(set);
  }
  else
  {
    SERIALISE_MEMBER_EMPTY(pipelineBindPoint);
    SERIALISE_MEMBER_EMPTY(pipelineLayout);
    SERIALISE_MEMBER_EMPTY(set);
  }
}

template <>
void Deserialise(const VkDescriptorUpdateTemplateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pDescriptorUpdateEntries;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindBufferMemoryInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memoryOffset);
}

template <>
void Deserialise(const VkBindBufferMemoryInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindImageMemoryInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memoryOffset);
}

template <>
void Deserialise(const VkBindImageMemoryInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineRasterizationConservativeStateCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkFlagWithNoBits, flags);
  SERIALISE_MEMBER(conservativeRasterizationMode);
  SERIALISE_MEMBER(extraPrimitiveOverestimationSize);
}

template <>
void Deserialise(const VkPipelineRasterizationConservativeStateCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineTessellationDomainOriginStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(domainOrigin);
}

template <>
void Deserialise(const VkPipelineTessellationDomainOriginStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageViewUsageCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkImageUsageFlagBits, usage);
}

template <>
void Deserialise(const VkImageViewUsageCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkInputAttachmentAspectReference &el)
{
  SERIALISE_MEMBER(subpass);
  SERIALISE_MEMBER(inputAttachmentIndex);
  SERIALISE_MEMBER_TYPED(VkImageAspectFlagBits, aspectMask);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRenderPassInputAttachmentAspectCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(aspectReferenceCount);
  SERIALISE_MEMBER_ARRAY(pAspectReferences, aspectReferenceCount);
}

template <>
void Deserialise(const VkRenderPassInputAttachmentAspectCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pAspectReferences;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkVertexInputBindingDivisorDescriptionEXT &el)
{
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(divisor);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineVertexInputDivisorStateCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(vertexBindingDivisorCount);
  SERIALISE_MEMBER_ARRAY(pVertexBindingDivisors, vertexBindingDivisorCount);
}

template <>
void Deserialise(const VkPipelineVertexInputDivisorStateCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pVertexBindingDivisors;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSamplerReductionModeCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(reductionMode);
}

template <>
void Deserialise(const VkSamplerReductionModeCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugUtilsLabelEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(pLabelName);
  SERIALISE_MEMBER(color);
}

template <>
void Deserialise(const VkDebugUtilsLabelEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSamplerYcbcrConversionCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(ycbcrModel);
  SERIALISE_MEMBER(ycbcrRange);
  SERIALISE_MEMBER(components);
  SERIALISE_MEMBER(xChromaOffset);
  SERIALISE_MEMBER(yChromaOffset);
  SERIALISE_MEMBER(chromaFilter);
  SERIALISE_MEMBER(forceExplicitReconstruction);
}

template <>
void Deserialise(const VkSamplerYcbcrConversionCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRenderPassMultiviewCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(subpassCount);
  SERIALISE_MEMBER_ARRAY(pViewMasks, subpassCount);
  SERIALISE_MEMBER(dependencyCount);
  SERIALISE_MEMBER_ARRAY(pViewOffsets, dependencyCount);
  SERIALISE_MEMBER(correlationMaskCount);
  SERIALISE_MEMBER_ARRAY(pCorrelationMasks, correlationMaskCount);
}

template <>
void Deserialise(const VkRenderPassMultiviewCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pViewMasks;
  delete[] el.pViewOffsets;
  delete[] el.pCorrelationMasks;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageFormatListCreateInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(viewFormatCount);
  SERIALISE_MEMBER_ARRAY(pViewFormats, viewFormatCount);
}

template <>
void Deserialise(const VkImageFormatListCreateInfoKHR &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pViewFormats;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceQueueInfo2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkDeviceQueueCreateFlagBits, flags);
  SERIALISE_MEMBER(queueFamilyIndex);
  SERIALISE_MEMBER(queueIndex);
}

template <>
void Deserialise(const VkDeviceQueueInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportMemoryAllocateInfoNV &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalMemoryHandleTypeFlagBitsNV, handleTypes);
}

template <>
void Deserialise(const VkExportMemoryAllocateInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalMemoryImageCreateInfoNV &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalMemoryHandleTypeFlagBitsNV, handleTypes);
}

template <>
void Deserialise(const VkExternalMemoryImageCreateInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalMemoryImageCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalMemoryHandleTypeFlagBits, handleTypes);
}

template <>
void Deserialise(const VkExternalMemoryImageCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportMemoryAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalMemoryHandleTypeFlagBits, handleTypes);
}

template <>
void Deserialise(const VkExportMemoryAllocateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalMemoryBufferCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalMemoryHandleTypeFlagBits, handleTypes);
}

template <>
void Deserialise(const VkExternalMemoryBufferCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportMemoryFdInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(handleType);
  SERIALISE_MEMBER(fd);
}

template <>
void Deserialise(const VkImportMemoryFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportSemaphoreCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalSemaphoreHandleTypeFlagBits, handleTypes);
}

template <>
void Deserialise(const VkExportSemaphoreCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportFenceCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalFenceHandleTypeFlagBits, handleTypes);
}

template <>
void Deserialise(const VkExportFenceCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSwapchainCounterCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkSurfaceCounterFlagBitsEXT, surfaceCounters);
}

template <>
void Deserialise(const VkSwapchainCounterCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDedicatedAllocationMemoryAllocateInfoNV &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(buffer);
}

template <>
void Deserialise(const VkDedicatedAllocationMemoryAllocateInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDedicatedAllocationImageCreateInfoNV &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(dedicatedAllocation);
}

template <>
void Deserialise(const VkDedicatedAllocationImageCreateInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDedicatedAllocationBufferCreateInfoNV &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(dedicatedAllocation);
}

template <>
void Deserialise(const VkDedicatedAllocationBufferCreateInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryDedicatedAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(buffer);
}

template <>
void Deserialise(const VkMemoryDedicatedAllocateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceQueueGlobalPriorityCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(globalPriority);
}

template <>
void Deserialise(const VkDeviceQueueGlobalPriorityCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindImagePlaneMemoryInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(planeAspect);
}

template <>
void Deserialise(const VkBindImagePlaneMemoryInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSamplerYcbcrConversionInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(conversion);
}

template <>
void Deserialise(const VkSamplerYcbcrConversionInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupSwapchainCreateInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkDeviceGroupPresentModeFlagBitsKHR, modes);
}

template <>
void Deserialise(const VkDeviceGroupSwapchainCreateInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindImageMemorySwapchainInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(swapchain);
  SERIALISE_MEMBER(imageIndex);
}

template <>
void Deserialise(const VkBindImageMemorySwapchainInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSwapchainCreateInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(swapchain);
}

template <>
void Deserialise(const VkImageSwapchainCreateInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindImageMemoryDeviceGroupInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceIndexCount);
  SERIALISE_MEMBER_ARRAY(pDeviceIndices, deviceIndexCount);
  SERIALISE_MEMBER(splitInstanceBindRegionCount);
  SERIALISE_MEMBER_ARRAY(pSplitInstanceBindRegions, splitInstanceBindRegionCount);
}

template <>
void Deserialise(const VkBindImageMemoryDeviceGroupInfo &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pDeviceIndices;
  delete[] el.pSplitInstanceBindRegions;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBindBufferMemoryDeviceGroupInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceIndexCount);
  SERIALISE_MEMBER_ARRAY(pDeviceIndices, deviceIndexCount);
}

template <>
void Deserialise(const VkBindBufferMemoryDeviceGroupInfo &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pDeviceIndices;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupBindSparseInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(resourceDeviceIndex);
  SERIALISE_MEMBER(memoryDeviceIndex);
}

template <>
void Deserialise(const VkDeviceGroupBindSparseInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupSubmitInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(waitSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pWaitSemaphoreDeviceIndices, waitSemaphoreCount);
  SERIALISE_MEMBER(commandBufferCount);
  SERIALISE_MEMBER_ARRAY(pCommandBufferDeviceMasks, commandBufferCount);
  SERIALISE_MEMBER(signalSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pSignalSemaphoreDeviceIndices, signalSemaphoreCount);
}

template <>
void Deserialise(const VkDeviceGroupSubmitInfo &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pWaitSemaphoreDeviceIndices;
  delete[] el.pCommandBufferDeviceMasks;
  delete[] el.pSignalSemaphoreDeviceIndices;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupCommandBufferBeginInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceMask);
}

template <>
void Deserialise(const VkDeviceGroupCommandBufferBeginInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupRenderPassBeginInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceMask);
  SERIALISE_MEMBER(deviceRenderAreaCount);
  SERIALISE_MEMBER_ARRAY(pDeviceRenderAreas, deviceRenderAreaCount);
}

template <>
void Deserialise(const VkDeviceGroupRenderPassBeginInfo &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pDeviceRenderAreas;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryAllocateFlagsInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkMemoryAllocateFlagBits, flags);
  SERIALISE_MEMBER(deviceMask);
}

template <>
void Deserialise(const VkMemoryAllocateFlagsInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkProtectedSubmitInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(protectedSubmit);
}

template <>
void Deserialise(const VkProtectedSubmitInfo &el)
{
  DeserialiseNext(el.pNext);
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
INSTANTIATE_SERIALISE_TYPE(VkAllocationCallbacks);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMemoryProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceProperties);
INSTANTIATE_SERIALISE_TYPE(VkDeviceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkBufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkBufferViewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageViewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkMemoryRequirements);
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
INSTANTIATE_SERIALISE_TYPE(VkDescriptorUpdateTemplateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindBufferMemoryInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindImageMemoryInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineRasterizationConservativeStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineVertexInputDivisorStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSamplerReductionModeCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugUtilsLabelEXT);
INSTANTIATE_SERIALISE_TYPE(VkSamplerYcbcrConversionCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassMultiviewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceQueueInfo2);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryAllocateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkExternalMemoryImageCreateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExternalMemoryBufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImportMemoryFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkExportSemaphoreCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExportFenceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSwapchainCounterCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDedicatedAllocationMemoryAllocateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkDedicatedAllocationImageCreateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkDedicatedAllocationBufferCreateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkMemoryDedicatedAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceQueueGlobalPriorityCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkBindImagePlaneMemoryInfo);
INSTANTIATE_SERIALISE_TYPE(VkSamplerYcbcrConversionInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupSwapchainCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkBindImageMemorySwapchainInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImageSwapchainCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkBindImageMemoryDeviceGroupInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindBufferMemoryDeviceGroupInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupBindSparseInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupSubmitInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupCommandBufferBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupRenderPassBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkMemoryAllocateFlagsInfo);
INSTANTIATE_SERIALISE_TYPE(VkProtectedSubmitInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageFormatListCreateInfoKHR);

INSTANTIATE_SERIALISE_TYPE(DescriptorSetSlot);
INSTANTIATE_SERIALISE_TYPE(ImageRegionState);
INSTANTIATE_SERIALISE_TYPE(ImageLayouts);

#if defined(VK_KHR_external_memory_win32) || defined(VK_NV_external_memory_win32)
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportMemoryWin32HandleInfoNV &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_TYPED(VkExternalMemoryHandleTypeFlagBitsNV, handleType);

  {
    uint64_t handle = (uint64_t)el.handle;
    ser.Serialise("handle", handle);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.handle = NULL;
  }
}

template <>
void Deserialise(const VkImportMemoryWin32HandleInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportMemoryWin32HandleInfoNV &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  {
    // serialise pointer as plain integer, rather than recursing and serialising struct
    uint64_t pAttributes = (uint64_t)el.pAttributes;
    ser.Serialise("pAttributes", pAttributes).TypedAs("SECURITY_ATTRIBUTES*");

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.pAttributes = NULL;
  }

  SERIALISE_MEMBER_TYPED(uint32_t, dwAccess);
}

template <>
void Deserialise(const VkExportMemoryWin32HandleInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportMemoryWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(handleType);

  {
    uint64_t handle = (uint64_t)el.handle;
    ser.Serialise("handle", handle);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.handle = NULL;
  }

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name", name);

    // we don't expose UTF82Wide on all platforms, but as above this struct won't be valid anyway
    if(ser.IsReading())
      el.name = NULL;
  }
}

template <>
void Deserialise(const VkImportMemoryWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

INSTANTIATE_SERIALISE_TYPE(VkImportMemoryWin32HandleInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryWin32HandleInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkImportMemoryWin32HandleInfoKHR);
#endif    // #if defined(VK_KHR_external_memory_win32) || defined(VK_NV_external_memory_win32)

#ifdef VK_KHR_external_fence_win32
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportFenceWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  {
    // serialise pointer as plain integer, rather than recursing and serialising struct
    uint64_t pAttributes = (uint64_t)el.pAttributes;
    ser.Serialise("pAttributes", pAttributes).TypedAs("SECURITY_ATTRIBUTES*");

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.pAttributes = NULL;
  }

  SERIALISE_MEMBER_TYPED(uint32_t, dwAccess);

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name", name);

    // we don't expose UTF82Wide on all platforms, but as above this struct won't be valid anyway
    if(ser.IsReading())
      el.name = L"???";
  }
}

template <>
void Deserialise(const VkExportFenceWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

INSTANTIATE_SERIALISE_TYPE(VkExportFenceWin32HandleInfoKHR);
#endif    // #ifdef VK_KHR_external_fence_win32

#ifdef VK_KHR_external_semaphore_win32
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportSemaphoreWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  {
    // serialise pointer as plain integer, rather than recursing and serialising struct
    uint64_t pAttributes = (uint64_t)el.pAttributes;
    ser.Serialise("pAttributes", pAttributes).TypedAs("SECURITY_ATTRIBUTES*");

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.pAttributes = NULL;
  }

  SERIALISE_MEMBER_TYPED(uint32_t, dwAccess);

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name", name);

    // we don't expose UTF82Wide on all platforms, but as above this struct won't be valid anyway
    if(ser.IsReading())
      el.name = L"???";
  }
}

template <>
void Deserialise(const VkExportSemaphoreWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

INSTANTIATE_SERIALISE_TYPE(VkExportSemaphoreWin32HandleInfoKHR);
#endif    // #ifdef VK_KHR_external_semaphore_win32

#if defined(VK_KHR_win32_keyed_mutex) || defined(VK_NV_win32_keyed_mutex)
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkWin32KeyedMutexAcquireReleaseInfoNV &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(acquireCount);
  SERIALISE_MEMBER_ARRAY(pAcquireSyncs, acquireCount);
  SERIALISE_MEMBER_ARRAY(pAcquireKeys, acquireCount);
  SERIALISE_MEMBER_ARRAY(pAcquireTimeoutMilliseconds, acquireCount);
  SERIALISE_MEMBER(releaseCount);
  SERIALISE_MEMBER_ARRAY(pReleaseSyncs, releaseCount);
  SERIALISE_MEMBER_ARRAY(pReleaseKeys, releaseCount);
}

template <>
void Deserialise(const VkWin32KeyedMutexAcquireReleaseInfoNV &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pAcquireSyncs;
  delete[] el.pAcquireKeys;
  delete[] el.pAcquireTimeoutMilliseconds;
  delete[] el.pReleaseSyncs;
  delete[] el.pReleaseKeys;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkWin32KeyedMutexAcquireReleaseInfoKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(acquireCount);
  SERIALISE_MEMBER_ARRAY(pAcquireSyncs, acquireCount);
  SERIALISE_MEMBER_ARRAY(pAcquireKeys, acquireCount);
  SERIALISE_MEMBER_ARRAY(pAcquireTimeouts, acquireCount);
  SERIALISE_MEMBER(releaseCount);
  SERIALISE_MEMBER_ARRAY(pReleaseSyncs, releaseCount);
  SERIALISE_MEMBER_ARRAY(pReleaseKeys, releaseCount);
}

template <>
void Deserialise(const VkWin32KeyedMutexAcquireReleaseInfoKHR &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pAcquireSyncs;
  delete[] el.pAcquireKeys;
  delete[] el.pAcquireTimeouts;
  delete[] el.pReleaseSyncs;
  delete[] el.pReleaseKeys;
}

INSTANTIATE_SERIALISE_TYPE(VkWin32KeyedMutexAcquireReleaseInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkWin32KeyedMutexAcquireReleaseInfoKHR);
#endif    // #if defined(VK_KHR_win32_keyed_mutex) || defined(VK_NV_win32_keyed_mutex)