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

#include "vk_common.h"
#include "vk_resources.h"

template <typename VkStruct>
static void UnwrapInPlace(VkStruct &s)
{
  s = Unwrap(s);
}

// utility function for when we're modifying one struct in a pNext chain, this
// lets us just copy across a struct unmodified into some temporary memory and
// append it onto a pNext chain we're building
static void CopyNextChainedStruct(const size_t structSize, byte *&tempMem,
                                  const VkBaseInStructure *nextInput,
                                  VkBaseInStructure *&nextChainTail)
{
  VkBaseInStructure *outstruct = (VkBaseInStructure *)tempMem;

  tempMem += structSize;

  // copy the struct, nothing to unwrap
  memcpy(outstruct, nextInput, structSize);

  // default to NULL. It will be overwritten next time if there is a next object
  outstruct->pNext = NULL;

  // append this onto the chain
  nextChainTail->pNext = outstruct;
  nextChainTail = outstruct;
}

// create a copy of the struct in temporary memory. Mostly only useful for when we're setting up a
// recursive next chain unwrap
template <typename VkStruct>
static VkStruct *AllocStructCopy(byte *&tempMem, const VkStruct *inputStruct)
{
  if(!inputStruct)
    return NULL;

  VkStruct *ret = (VkStruct *)tempMem;
  tempMem = (byte *)(ret + 1);

  *ret = *inputStruct;

  return ret;
}

// this is similar to the above function, but for use after we've modified a struct locally
// e.g. to unwrap some members or patch flags, etc.
template <typename VkStruct>
static void AppendModifiedChainedStruct(byte *&tempMem, VkStruct *outputStruct,
                                        VkBaseInStructure *&nextChainTail)
{
  tempMem = (byte *)(outputStruct + 1);

  // default to NULL. It will be overwritten in the next step if there is a next object
  outputStruct->pNext = NULL;

  // append this onto the chain
  nextChainTail->pNext = (const VkBaseInStructure *)outputStruct;
  nextChainTail = (VkBaseInStructure *)outputStruct;
}

// define structs that just need to be copied with no unwrapping at all, or only unwrapping some
// members - easily shared between GetNextPatchSize and UnwrapNextChain
#define PROCESS_SIMPLE_STRUCTS()                                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,                         \
              VkAccelerationStructureBuildSizesInfoKHR);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,                      \
              VkAccelerationStructureGeometryAabbsDataKHR);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,                  \
              VkAccelerationStructureGeometryInstancesDataKHR);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,                                 \
              VkAccelerationStructureGeometryKHR);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,                  \
              VkAccelerationStructureGeometryTrianglesDataKHR);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_VERSION_INFO_KHR,                             \
              VkAccelerationStructureVersionInfoKHR);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR, VkAcquireProfilingLockInfoKHR);     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_APPLICATION_INFO, VkApplicationInfo);                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, VkAttachmentDescription2);                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT,                               \
              VkAttachmentDescriptionStencilLayout);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, VkAttachmentReference2);                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT,                                 \
              VkAttachmentReferenceStencilLayout);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO,                                \
              VkBindBufferMemoryDeviceGroupInfo);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO,                                 \
              VkBindImageMemoryDeviceGroupInfo);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, VkBindImagePlaneMemoryInfo);           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_COPY_2, VkBufferCopy2);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, VkBufferCreateInfo);                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,                               \
              VkBufferDeviceAddressCreateInfoEXT);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2, VkBufferImageCopy2);                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO,                           \
              VkBufferOpaqueCaptureAddressCreateInfo);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR, VkCalibratedTimestampInfoKHR);        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, VkCommandBufferBeginInfo);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,           \
              VkCommandBufferInheritanceConditionalRenderingInfoEXT);                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, VkCommandPoolCreateInfo);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, VkDebugMarkerMarkerInfoEXT);           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,                               \
              VkDebugReportCallbackCreateInfoEXT);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, VkDebugUtilsLabelEXT);                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,                               \
              VkDebugUtilsMessengerCreateInfoEXT);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV,                          \
              VkDedicatedAllocationBufferCreateInfoNV);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV,                           \
              VkDedicatedAllocationImageCreateInfoNV);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, VkDescriptorPoolCreateInfo);            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,                    \
              VkDescriptorPoolInlineUniformBlockCreateInfo);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,                     \
              VkDescriptorSetLayoutBindingFlagsCreateInfo);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT, VkDescriptorSetLayoutSupport);        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,              \
              VkDescriptorSetVariableDescriptorCountAllocateInfo);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT,             \
              VkDescriptorSetVariableDescriptorCountLayoutSupport);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, VkDeviceCreateInfo);                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, VkDeviceGroupBindSparseInfo);         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO,                              \
              VkDeviceGroupCommandBufferBeginInfo);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR,                               \
              VkDeviceGroupPresentCapabilitiesKHR);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR, VkDeviceGroupPresentInfoKHR);         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,                                 \
              VkDeviceGroupRenderPassBeginInfo);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO, VkDeviceGroupSubmitInfo);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR,                              \
              VkDeviceGroupSwapchainCreateInfoKHR);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD,                        \
              VkDeviceMemoryOverallocationCreateInfoAMD);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO, VkDevicePrivateDataCreateInfo);     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, VkDeviceQueueCreateInfo);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR,                        \
              VkDeviceQueueGlobalPriorityCreateInfoKHR);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2, VkDeviceQueueInfo2);                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR, VkDisplayModeProperties2KHR);         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD,                         \
              VkDisplayNativeHdrSurfaceCapabilitiesAMD)                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR, VkDisplayPlaneCapabilities2KHR);   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR, VkDisplayPlaneInfo2KHR);                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PLANE_PROPERTIES_2_KHR, VkDisplayPlaneProperties2KHR);       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR, VkDisplayPresentInfoKHR);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR, VkDisplayProperties2KHR);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, VkEventCreateInfo);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES, VkExternalBufferProperties);             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES, VkExternalImageFormatProperties);  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, VkFenceCreateInfo);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT,                 \
              VkFilterCubicImageViewImageFormatPropertiesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,                           \
              VkFragmentShadingRateAttachmentInfoKHR);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, VkFormatProperties2);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3, VkFormatProperties3);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,                           \
              VkGraphicsPipelineLibraryCreateInfoEXT);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_HDR_METADATA_EXT, VkHdrMetadataEXT)                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_BLIT_2, VkImageBlit2);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_COPY_2, VkImageCopy2);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, VkImageCreateInfo);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, VkImageFormatListCreateInfo);         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, VkImageFormatProperties2);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,                                \
              VkImagePlaneMemoryRequirementsInfo);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2, VkImageResolve2);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO, VkImageStencilUsageCreateInfo);     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT, VkImageViewASTCDecodeModeEXT);      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT, VkImageViewMinLodCreateInfoEXT); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, VkImageViewUsageCreateInfo);           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, VkInstanceCreateInfo);                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, VkMemoryAllocateFlagsInfo);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, VkMemoryAllocateInfo);                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_BARRIER, VkMemoryBarrier);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, VkMemoryBarrier2);                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, VkMemoryDedicatedRequirements);       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,                         \
              VkMemoryOpaqueCaptureAddressAllocateInfo);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT, VkMemoryPriorityAllocateInfoEXT); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, VkMemoryRequirements2);                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT, VkMultisamplePropertiesEXT);             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT,                      \
              VkMultisampledRenderToSingleSampledInfoEXT);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT,                             \
              VkMutableDescriptorTypeCreateInfoEXT);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR,                                 \
              VkPerformanceCounterDescriptionKHR);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR, VkPerformanceCounterKHR);                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR, VkPerformanceQuerySubmitInfoKHR); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,                              \
              VkPhysicalDevice16BitStorageFeatures);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT,                           \
              VkPhysicalDevice4444FormatsFeaturesEXT);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES,                               \
              VkPhysicalDevice8BitStorageFeatures);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,                 \
              VkPhysicalDeviceAccelerationStructureFeaturesKHR);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,               \
              VkPhysicalDeviceAccelerationStructurePropertiesKHR);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT,                            \
              VkPhysicalDeviceASTCDecodeFeaturesEXT);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT, \
              VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT);                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT,        \
              VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT);                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT,                   \
              VkPhysicalDeviceBorderColorSwizzleFeaturesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT,                  \
              VkPhysicalDeviceBufferDeviceAddressFeaturesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,                      \
              VkPhysicalDeviceBufferDeviceAddressFeatures);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD,                        \
              VkPhysicalDeviceCoherentMemoryFeaturesAMD);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT,                     \
              VkPhysicalDeviceColorWriteEnableFeaturesEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV,              \
              VkPhysicalDeviceComputeShaderDerivativesFeaturesNV);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,                  \
              VkPhysicalDeviceConditionalRenderingFeaturesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT,           \
              VkPhysicalDeviceConservativeRasterizationPropertiesEXT);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,                    \
              VkPhysicalDeviceCustomBorderColorFeaturesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT,                  \
              VkPhysicalDeviceCustomBorderColorPropertiesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV,     \
              VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT,                   \
              VkPhysicalDeviceDepthClampZeroOneFeaturesEXT)                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT,                     \
              VkPhysicalDeviceDepthClipControlFeaturesEXT)                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,                      \
              VkPhysicalDeviceDepthClipEnableFeaturesEXT)                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES,                    \
              VkPhysicalDeviceDepthStencilResolveProperties);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,                        \
              VkPhysicalDeviceDescriptorIndexingFeatures);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES,                      \
              VkPhysicalDeviceDescriptorIndexingProperties);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT,                    \
              VkPhysicalDeviceDiscardRectanglePropertiesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,                                   \
              VkPhysicalDeviceDriverProperties);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,                          \
              VkPhysicalDeviceDynamicRenderingFeatures)                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,                 \
              VkPhysicalDeviceExtendedDynamicStateFeaturesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT,               \
              VkPhysicalDeviceExtendedDynamicState2FeaturesEXT);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,               \
              VkPhysicalDeviceExtendedDynamicState3FeaturesEXT);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT,             \
              VkPhysicalDeviceExtendedDynamicState3PropertiesEXT);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,                                \
              VkPhysicalDeviceExternalBufferInfo);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,                          \
              VkPhysicalDeviceExternalImageFormatInfo);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,                             \
              VkPhysicalDeviceExternalSemaphoreInfo);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, VkPhysicalDeviceFeatures2);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES,                           \
              VkPhysicalDeviceFloatControlsProperties);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,            \
              VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR)                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_PROPERTIES_KHR,          \
              VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR)                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR,                  \
              VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR)                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,              \
              VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT)                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT,            \
              VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT)                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES, VkPhysicalDeviceGroupProperties)   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,                        \
              VkPhysicalDeviceShaderFloat16Int8Features);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,                   \
              VkPhysicalDeviceFragmentDensityMapFeaturesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT,                 \
              VkPhysicalDeviceFragmentDensityMapPropertiesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT,                 \
              VkPhysicalDeviceFragmentDensityMap2FeaturesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT,               \
              VkPhysicalDeviceFragmentDensityMap2PropertiesEXT);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM,           \
              VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM);                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM,         \
              VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,              \
              VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,                  \
              VkPhysicalDeviceFragmentShadingRateFeaturesKHR);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR,                           \
              VkPhysicalDeviceFragmentShadingRateKHR);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR,                \
              VkPhysicalDeviceFragmentShadingRatePropertiesKHR);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES,                           \
              VkPhysicalDeviceHostQueryResetFeatures);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES, VkPhysicalDeviceIDProperties);        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT,                    \
              VkPhysicalDeviceImage2DViewOf3DFeaturesEXT);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,                                 \
              VkPhysicalDeviceImageFormatInfo2);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT,                    \
              VkPhysicalDeviceImageViewImageFormatInfoEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES,                      \
              VkPhysicalDeviceImagelessFramebufferFeatures)                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES,                           \
              VkPhysicalDeviceImageRobustnessFeatures)                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_KHR,                       \
              VkPhysicalDeviceIndexTypeUint8FeaturesKHR);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT,                     \
              VkPhysicalDeviceImageViewMinLodFeaturesEXT)                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES,                       \
              VkPhysicalDeviceInlineUniformBlockFeatures);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES,                     \
              VkPhysicalDeviceInlineUniformBlockProperties);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,                            \
              VkPhysicalDeviceMaintenance3Properties);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,                              \
              VkPhysicalDeviceMaintenance4Features);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES,                            \
              VkPhysicalDeviceMaintenance4Properties);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,                        \
              VkPhysicalDeviceMemoryBudgetPropertiesEXT);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,                                 \
              VkPhysicalDeviceMemoryProperties2);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,                        \
              VkPhysicalDeviceMemoryPriorityFeaturesEXT);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,                            \
              VkPhysicalDeviceMeshShaderFeaturesEXT);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT,                          \
              VkPhysicalDeviceMeshShaderPropertiesEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT,  \
              VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT);                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,                                  \
              VkPhysicalDeviceMultiviewFeatures);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES,                                \
              VkPhysicalDeviceMultiviewProperties);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT,                \
              VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_FEATURES_EXT,                  \
              VkPhysicalDeviceNestedCommandBufferFeaturesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_PROPERTIES_EXT,                \
              VkPhysicalDeviceNestedCommandBufferPropertiesEXT);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT,                  \
              VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT,                     \
              VkPhysicalDeviceLineRasterizationFeaturesEXT)                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT,                   \
              VkPhysicalDeviceLineRasterizationPropertiesEXT)                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT,           \
              VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT);                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT,                         \
              VkPhysicalDevicePCIBusInfoPropertiesEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR,                      \
              VkPhysicalDevicePerformanceQueryFeaturesKHR);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR,                    \
              VkPhysicalDevicePerformanceQueryPropertiesKHR);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES,            \
              VkPhysicalDevicePipelineCreationCacheControlFeatures);                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR,         \
              VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR)                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES,                           \
              VkPhysicalDevicePointClippingProperties);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR,                             \
              VkPhysicalDevicePresentIdFeaturesKHR);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,                           \
              VkPhysicalDevicePresentWaitFeaturesKHR);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT,        \
              VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT)                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT,             \
              VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES,                               \
              VkPhysicalDevicePrivateDataFeatures);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, VkPhysicalDeviceProperties2);          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,                           \
              VkPhysicalDeviceProtectedMemoryFeatures);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES,                         \
              VkPhysicalDeviceProtectedMemoryProperties);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT,                       \
              VkPhysicalDeviceProvokingVertexFeaturesEXT);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT,                     \
              VkPhysicalDeviceProvokingVertexPropertiesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR,                      \
              VkPhysicalDevicePushDescriptorPropertiesKHR);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT,  \
              VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT);                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,                   \
              VkPhysicalDeviceRayTracingPipelineFeaturesKHR);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,                 \
              VkPhysicalDeviceRayTracingPipelinePropertiesKHR);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,                              \
              VkPhysicalDeviceRayQueryFeaturesKHR);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT,                       \
              VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,                           \
              VkPhysicalDeviceRobustness2FeaturesEXT);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT,                         \
              VkPhysicalDeviceRobustness2PropertiesEXT);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT,                     \
              VkPhysicalDeviceSampleLocationsPropertiesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES,                    \
              VkPhysicalDeviceSamplerFilterMinmaxProperties);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,                   \
              VkPhysicalDeviceSamplerYcbcrConversionFeatures);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,                        \
              VkPhysicalDeviceScalarBlockLayoutFeatures);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES,             \
              VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES,                        \
              VkPhysicalDeviceShaderAtomicInt64Features);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,                    \
              VkPhysicalDeviceShaderAtomicFloatFeaturesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT,                  \
              VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD,                          \
              VkPhysicalDeviceShaderCorePropertiesAMD);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR,                           \
              VkPhysicalDeviceShaderClockFeaturesKHR);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES,         \
              VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,                     \
              VkPhysicalDeviceShaderDrawParametersFeatures);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT,              \
              VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT)                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV,                  \
              VkPhysicalDeviceShaderImageFootprintFeaturesNV);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES,                 \
              VkPhysicalDeviceShaderIntegerDotProductFeatures);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES,               \
              VkPhysicalDeviceShaderIntegerDotProductProperties);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,                          \
              VkPhysicalDeviceShaderObjectFeaturesEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_PROPERTIES_EXT,                        \
              VkPhysicalDeviceShaderObjectPropertiesEXT);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES,             \
              VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures)                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR,   \
              VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR)                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES,                \
              VkPhysicalDeviceShaderTerminateInvocationFeatures)                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2,                          \
              VkPhysicalDeviceSparseImageFormatInfo2);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,                                 \
              VkPhysicalDeviceSubgroupProperties);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES,                      \
              VkPhysicalDeviceSubgroupSizeControlFeatures)                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES,                    \
              VkPhysicalDeviceSubgroupSizeControlProperties)                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,                \
              VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT)                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,                          \
              VkPhysicalDeviceSynchronization2Features);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT,                 \
              VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES,                   \
              VkPhysicalDeviceTexelBufferAlignmentProperties);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES,               \
              VkPhysicalDeviceTextureCompressionASTCHDRFeatures);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,                         \
              VkPhysicalDeviceTimelineSemaphoreFeatures);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES,                       \
              VkPhysicalDeviceTimelineSemaphoreProperties);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES, VkPhysicalDeviceToolProperties);    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,                     \
              VkPhysicalDeviceTransformFeedbackFeaturesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT,                   \
              VkPhysicalDeviceTransformFeedbackPropertiesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES,                          \
              VkPhysicalDeviceVariablePointerFeatures);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR,               \
              VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT,             \
              VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_KHR,             \
              VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,             \
              VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES,             \
              VkPhysicalDeviceUniformBufferStandardLayoutFeatures);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,                                 \
              VkPhysicalDeviceVulkan11Features);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,                               \
              VkPhysicalDeviceVulkan11Properties);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,                                 \
              VkPhysicalDeviceVulkan12Features);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,                               \
              VkPhysicalDeviceVulkan12Properties);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,                                 \
              VkPhysicalDeviceVulkan13Features);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,                               \
              VkPhysicalDeviceVulkan13Properties);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES,                        \
              VkPhysicalDeviceVulkanMemoryModelFeatures);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR,       \
              VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR)                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT,              \
              VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT)                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT,                     \
              VkPhysicalDeviceYcbcrImageArraysFeaturesEXT)                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES,           \
              VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures)                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, VkPipelineCacheCreateInfo);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,                              \
              VkPipelineCreationFeedbackCreateInfo);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,                              \
              VkPipelineColorBlendStateCreateInfo);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,                                \
              VkPipelineColorWriteCreateInfoEXT);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,                            \
              VkPipelineDepthStencilStateCreateInfo);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT,                    \
              VkPipelineDiscardRectangleStateCreateInfoEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,                                  \
              VkPipelineDynamicStateCreateInfo);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR,                                  \
              VkPipelineExecutablePropertiesKHR)                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR, VkPipelineExecutableStatisticKHR) \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR,                     \
              VkPipelineExecutableInternalRepresentationKHR)                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,                \
              VkPipelineFragmentShadingRateStateCreateInfoKHR)                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,                           \
              VkPipelineInputAssemblyStateCreateInfo);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,                              \
              VkPipelineMultisampleStateCreateInfo);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,           \
              VkPipelineRasterizationConservativeStateCreateInfoEXT);                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,             \
              VkPipelineRasterizationDepthClipStateCreateInfoEXT);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,                   \
              VkPipelineRasterizationLineStateCreateInfoEXT)                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,       \
              VkPipelineRasterizationProvokingVertexStateCreateInfoEXT)                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,                            \
              VkPipelineRasterizationStateCreateInfo);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,                 \
              VkPipelineRasterizationStateStreamCreateInfoEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,                     \
              VkPipelineSampleLocationsStateCreateInfoEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VkPipelineShaderStageCreateInfo); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,            \
              VkPipelineShaderStageRequiredSubgroupSizeCreateInfo)                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,               \
              VkPipelineTessellationDomainOriginStateCreateInfo);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,                             \
              VkPipelineTessellationStateCreateInfo);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR,                 \
              VkPipelineVertexInputDivisorStateCreateInfoKHR);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,                             \
              VkPipelineVertexInputStateCreateInfo);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT,                \
              VkPipelineViewportDepthClipControlCreateInfoEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,                                 \
              VkPipelineViewportStateCreateInfo);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PRESENT_ID_KHR, VkPresentIdKHR);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR, VkPresentRegionsKHR);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE, VkPresentTimesInfoGOOGLE)                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO, VkPrivateDataSlotCreateInfo)          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, VkQueryPoolCreateInfo);                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR,                              \
              VkQueryPoolPerformanceCreateInfoKHR);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR,                         \
              VkQueueFamilyGlobalPriorityPropertiesKHR);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, VkQueueFamilyProperties2);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR,                      \
              VkRayTracingPipelineInterfaceCreateInfoKHR);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,                            \
              VkRayTracingShaderGroupCreateInfoKHR);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, VkRenderPassCreateInfo);                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, VkRenderPassCreateInfo2);                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,                    \
              VkRenderPassFragmentDensityMapCreateInfoEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO,                     \
              VkRenderPassInputAttachmentAspectCreateInfo);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, VkRenderPassMultiviewCreateInfo); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,                         \
              VkRenderPassSampleLocationsBeginInfoEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, VkSampleLocationsInfoEXT);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT,              \
              VkSamplerBorderColorComponentMappingCreateInfoEXT);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, VkSamplerCreateInfo);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,                         \
              VkSamplerCustomBorderColorCreateInfoEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,                                  \
              VkSamplerReductionModeCreateInfo);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,                                \
              VkSamplerYcbcrConversionCreateInfo);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES,                    \
              VkSamplerYcbcrConversionImageFormatProperties);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, VkSemaphoreCreateInfo);                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, VkSemaphoreTypeCreateInfo);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, VkShaderModuleCreateInfo);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR,                             \
              VkSharedPresentSurfaceCapabilitiesKHR);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2, VkSparseImageFormatProperties2);   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2,                                  \
              VkSparseImageMemoryRequirements2);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO, VkSubpassBeginInfo);                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2, VkSubpassDependency2);                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, VkSubpassDescription2);                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,                           \
              VkSubpassDescriptionDepthStencilResolve);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_QCOM,                   \
              VkSubpassFragmentDensityMapOffsetEndInfoQCOM);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_END_INFO, VkSubpassEndInfo);                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_RESOLVE_PERFORMANCE_QUERY_EXT,                               \
              VkSubpassResolvePerformanceQueryEXT);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT, VkSurfaceCapabilities2EXT);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, VkSurfaceCapabilities2KHR);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, VkSurfaceFormat2KHR);                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,                              \
              VkSurfacePresentModeCompatibilityEXT)                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT, VkSurfacePresentModeEXT)                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_EXT,                            \
              VkSurfacePresentScalingCapabilitiesEXT)                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR,                                  \
              VkSurfaceProtectedCapabilitiesKHR);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD,                        \
              VkSwapchainDisplayNativeHdrCreateInfoAMD)                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT, VkSwapchainPresentModeInfoEXT)      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,                             \
              VkSwapchainPresentModesCreateInfoEXT)                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,                           \
              VkSwapchainPresentScalingCreateInfoEXT)                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD,                            \
              VkTextureLODGatherFormatPropertiesAMD);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, VkTimelineSemaphoreSubmitInfo);      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT, VkValidationCacheCreateInfoEXT);   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, VkValidationFeaturesEXT);                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,                            \
              VkVertexInputAttributeDescription2EXT);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,                              \
              VkVertexInputBindingDescription2EXT);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK,                           \
              VkWriteDescriptorSetInlineUniformBlock);                                               \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO,                            \
                           VkLayerInstanceCreateInfo);                                               \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VkLayerDeviceCreateInfo);    \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT, VkDeviceEventInfoEXT);           \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT, VkDisplayEventInfoEXT);         \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT, VkDisplayPowerInfoEXT);         \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO, VkExportFenceCreateInfo);     \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,                            \
                           VkExportMemoryAllocateInfo);                                              \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV,                         \
                           VkExportMemoryAllocateInfoNV);                                            \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,                           \
                           VkExportSemaphoreCreateInfo);                                             \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES, VkExternalFenceProperties);  \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,                     \
                           VkExternalMemoryBufferCreateInfo);                                        \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,                      \
                           VkExternalMemoryImageCreateInfo);                                         \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV,                   \
                           VkExternalMemoryImageCreateInfoNV);                                       \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,                          \
                           VkExternalSemaphoreProperties);                                           \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, VkImportMemoryFdInfoKHR);    \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR, VkMemoryFdPropertiesKHR);     \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,                    \
                           VkPhysicalDeviceExternalFenceInfo);                                       \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO, VkProtectedSubmitInfo);          \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT,         \
                           VkShaderModuleValidationCacheCreateInfoEXT);                              \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT,                      \
                           VkSwapchainCounterCreateInfoEXT);                                         \
  COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT, VkValidationFlagsEXT);            \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,                    \
                VkAccelerationStructureBuildGeometryInfoKHR,                                         \
                UnwrapInPlace(out->srcAccelerationStructure),                                        \
                UnwrapInPlace(out->dstAccelerationStructure));                                       \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,                            \
                VkAccelerationStructureCreateInfoKHR, UnwrapInPlace(out->buffer));                   \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,                    \
                VkAccelerationStructureDeviceAddressInfoKHR,                                         \
                UnwrapInPlace(out->accelerationStructure));                                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO, VkBindBufferMemoryInfo,                   \
                UnwrapInPlace(out->buffer), UnwrapInPlace(out->memory));                             \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, VkBindImageMemoryInfo,                     \
                UnwrapInPlace(out->image), UnwrapInPlace(out->memory));                              \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, VkBufferMemoryBarrier,                      \
                UnwrapInPlace(out->buffer));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, VkBufferMemoryBarrier2,                   \
                UnwrapInPlace(out->buffer));                                                         \
  /* VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT aliased by KHR */                              \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, VkBufferDeviceAddressInfo,             \
                UnwrapInPlace(out->buffer));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,                                 \
                VkBufferMemoryRequirementsInfo2, UnwrapInPlace(out->buffer));                        \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, VkBufferViewCreateInfo,                   \
                UnwrapInPlace(out->buffer));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, VkCommandBufferAllocateInfo,         \
                UnwrapInPlace(out->commandPool));                                                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, VkCommandBufferInheritanceInfo,   \
                UnwrapInPlace(out->renderPass), UnwrapInPlace(out->framebuffer));                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, VkCommandBufferSubmitInfo,             \
                UnwrapInPlace(out->commandBuffer));                                                  \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,                              \
                VkConditionalRenderingBeginInfoEXT, UnwrapInPlace(out->buffer));                     \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,                              \
                VkCopyAccelerationStructureInfoKHR, UnwrapInPlace(out->src),                         \
                UnwrapInPlace(out->dst));                                                            \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR,                    \
                VkCopyAccelerationStructureToMemoryInfoKHR, UnwrapInPlace(out->src));                \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, VkCopyDescriptorSet,                          \
                UnwrapInPlace(out->srcSet), UnwrapInPlace(out->dstSet));                             \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR,                    \
                VkCopyMemoryToAccelerationStructureInfoKHR, UnwrapInPlace(out->dst));                \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,                \
                VkRenderingFragmentDensityMapAttachmentInfoEXT, UnwrapInPlace(out->imageView));      \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,               \
                VkRenderingFragmentShadingRateAttachmentInfoKHR, UnwrapInPlace(out->imageView));     \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV,                      \
                VkDedicatedAllocationMemoryAllocateInfoNV, UnwrapInPlace(out->buffer),               \
                UnwrapInPlace(out->image));                                                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,                            \
                VkDescriptorUpdateTemplateCreateInfo, UnwrapInPlace(out->descriptorSetLayout),       \
                UnwrapInPlace(out->pipelineLayout));                                                 \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO,                         \
                VkDeviceMemoryOpaqueCaptureAddressInfo, UnwrapInPlace(out->memory));                 \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, VkImageMemoryBarrier,                        \
                UnwrapInPlace(out->image));                                                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, VkImageMemoryBarrier2,                     \
                UnwrapInPlace(out->image));                                                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,                                  \
                VkImageMemoryRequirementsInfo2, UnwrapInPlace(out->image));                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2,                           \
                VkImageSparseMemoryRequirementsInfo2, UnwrapInPlace(out->image));                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, VkImageViewCreateInfo,                     \
                UnwrapInPlace(out->image));                                                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, VkMappedMemoryRange,                          \
                UnwrapInPlace(out->memory));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, VkMemoryDedicatedAllocateInfo,     \
                UnwrapInPlace(out->buffer), UnwrapInPlace(out->image));                              \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR, VkPipelineInfoKHR,                              \
                UnwrapInPlace(out->pipeline));                                                       \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR, VkPipelineExecutableInfoKHR,         \
                UnwrapInPlace(out->pipeline));                                                       \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, VkRenderingAttachmentInfo,              \
                UnwrapInPlace(out->imageView), UnwrapInPlace(out->resolveImageView));                \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, VkRenderPassBeginInfo,                     \
                UnwrapInPlace(out->renderPass), UnwrapInPlace(out->framebuffer));                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT,                                 \
                VkReleaseSwapchainImagesInfoEXT, UnwrapInPlace(out->swapchain))                      \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, VkSamplerYcbcrConversionInfo,       \
                UnwrapInPlace(out->conversion));                                                     \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, VkSemaphoreSignalInfo,                      \
                UnwrapInPlace(out->semaphore));                                                      \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, VkSemaphoreSubmitInfo,                      \
                UnwrapInPlace(out->semaphore));                                                      \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,                          \
                             VkAcquireNextImageInfoKHR, UnwrapInPlace(out->swapchain),               \
                             UnwrapInPlace(out->semaphore), UnwrapInPlace(out->fence));              \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,                 \
                             VkBindImageMemorySwapchainInfoKHR, UnwrapInPlace(out->swapchain));      \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR, VkFenceGetFdInfoKHR,           \
                             UnwrapInPlace(out->fence));                                             \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,                      \
                             VkImageSwapchainCreateInfoKHR, UnwrapInPlace(out->swapchain));          \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR, VkImportFenceFdInfoKHR,     \
                             UnwrapInPlace(out->fence));                                             \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,                         \
                             VkImportSemaphoreFdInfoKHR, UnwrapInPlace(out->semaphore));             \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, VkMemoryGetFdInfoKHR,         \
                             UnwrapInPlace(out->memory));                                            \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,                   \
                             VkPhysicalDeviceSurfaceInfo2KHR, UnwrapInPlace(out->surface));          \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, VkSemaphoreGetFdInfoKHR,   \
                             UnwrapInPlace(out->semaphore));                                         \
  UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, VkSwapchainCreateInfoKHR,  \
                             UnwrapInPlace(out->surface), UnwrapInPlace(out->oldSwapchain));

// define cases for structs we don't handle at all - only the body of the case needs to be defined
// per-function.
#define UNHANDLED_STRUCTS()                                                                 \
  /* Surface creation structs would pull in dependencies on OS-specific includes, */        \
  /* so we treat them as unsupported. */                                                    \
  case VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR:                                   \
  case VK_STRUCTURE_TYPE_DIRECTFB_SURFACE_CREATE_INFO_EXT:                                  \
  case VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR:                                      \
  case VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR:                                   \
  case VK_STRUCTURE_TYPE_IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA:                             \
  case VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK:                                       \
  case VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK:                                     \
  case VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP:                         \
  case VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN:                                         \
  case VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR:                                   \
  case VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR:                                     \
  case VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR:                                       \
  case VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR:                                      \
  /* Output structure containing objects. Must be *wrapped* not unwrapped. */               \
  /* So we treat this as unhandled in generic code and require specific handling. */        \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:           \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV:                             \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_MOTION_TRIANGLES_DATA_NV:          \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV:                                    \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV:                \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV:                             \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT:             \
  case VK_STRUCTURE_TYPE_AMIGO_PROFILING_SUBMIT_INFO_SEC:                                   \
  case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_2_ANDROID:               \
  case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_RESOLVE_PROPERTIES_ANDROID:         \
  case VK_STRUCTURE_TYPE_ATTACHMENT_SAMPLE_COUNT_INFO_AMD:                                  \
  case VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV:                        \
  case VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT:                 \
  case VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO_KHR:                                     \
  case VK_STRUCTURE_TYPE_BIND_MEMORY_STATUS_KHR:                                            \
  case VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_BLIT_IMAGE_CUBIC_WEIGHTS_INFO_QCOM:                                \
  case VK_STRUCTURE_TYPE_BUFFER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:                           \
  case VK_STRUCTURE_TYPE_BUFFER_COLLECTION_BUFFER_CREATE_INFO_FUCHSIA:                      \
  case VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CONSTRAINTS_INFO_FUCHSIA:                        \
  case VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIA:                             \
  case VK_STRUCTURE_TYPE_BUFFER_COLLECTION_IMAGE_CREATE_INFO_FUCHSIA:                       \
  case VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIA:                              \
  case VK_STRUCTURE_TYPE_BUFFER_CONSTRAINTS_INFO_FUCHSIA:                                   \
  case VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR:                              \
  case VK_STRUCTURE_TYPE_CHECKPOINT_DATA_2_NV:                                              \
  case VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV:                                                \
  case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM:        \
  case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV:               \
  case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_INDIRECT_BUFFER_INFO_NV:                          \
  case VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR:                                 \
  case VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV:                                  \
  case VK_STRUCTURE_TYPE_COPY_COMMAND_TRANSFORM_INFO_QCOM:                                  \
  case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO_EXT:                                      \
  case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_COPY_MEMORY_TO_MICROMAP_INFO_EXT:                                  \
  case VK_STRUCTURE_TYPE_COPY_MICROMAP_INFO_EXT:                                            \
  case VK_STRUCTURE_TYPE_COPY_MICROMAP_TO_MEMORY_INFO_EXT:                                  \
  case VK_STRUCTURE_TYPE_CU_FUNCTION_CREATE_INFO_NVX:                                       \
  case VK_STRUCTURE_TYPE_CU_LAUNCH_INFO_NVX:                                                \
  case VK_STRUCTURE_TYPE_CU_MODULE_CREATE_INFO_NVX:                                         \
  case VK_STRUCTURE_TYPE_CUDA_FUNCTION_CREATE_INFO_NV:                                      \
  case VK_STRUCTURE_TYPE_CUDA_LAUNCH_INFO_NV:                                               \
  case VK_STRUCTURE_TYPE_CUDA_MODULE_CREATE_INFO_NV:                                        \
  case VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT:                                               \
  case VK_STRUCTURE_TYPE_DEPTH_BIAS_REPRESENTATION_INFO_EXT:                                \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT:                                       \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT:                                \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT:       \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT:                                           \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_BINDING_REFERENCE_VALVE:                            \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_HOST_MAPPING_INFO_VALVE:                     \
  case VK_STRUCTURE_TYPE_DEVICE_ADDRESS_BINDING_CALLBACK_DATA_EXT:                          \
  case VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT:                       \
  case VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV:                          \
  case VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT:                                           \
  case VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT:                                             \
  case VK_STRUCTURE_TYPE_DEVICE_IMAGE_SUBRESOURCE_INFO_KHR:                                 \
  case VK_STRUCTURE_TYPE_DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT:                            \
  case VK_STRUCTURE_TYPE_DEVICE_QUEUE_SHADER_CORE_CONTROL_CREATE_INFO_ARM:                  \
  case VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_INFO_LUNARG:                                 \
  case VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG:                                 \
  case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT:                         \
  case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:                           \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_BUFFER_INFO_EXT:                                      \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT:                               \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT:                                      \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_IO_SURFACE_INFO_EXT:                                  \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECT_CREATE_INFO_EXT:                               \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_SHARED_EVENT_INFO_EXT:                                \
  case VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_QNX:                                               \
  case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_ACQUIRE_UNMODIFIED_EXT:                            \
  case VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT:                                                \
  case VK_STRUCTURE_TYPE_FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV:                          \
  case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV:                                        \
  case VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV:                    \
  case VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV:                                                  \
  case VK_STRUCTURE_TYPE_GEOMETRY_NV:                                                       \
  case VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV:                                             \
  case VK_STRUCTURE_TYPE_GET_LATENCY_MARKER_INFO_NV:                                        \
  case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV:                    \
  case VK_STRUCTURE_TYPE_GRAPHICS_SHADER_GROUP_CREATE_INFO_NV:                              \
  case VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT:                                  \
  case VK_STRUCTURE_TYPE_HOST_IMAGE_COPY_DEVICE_PERFORMANCE_QUERY_EXT:                      \
  case VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT:                             \
  case VK_STRUCTURE_TYPE_IMAGE_ALIGNMENT_CONTROL_CREATE_INFO_MESA:                          \
  case VK_STRUCTURE_TYPE_IMAGE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:                            \
  case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT:                                     \
  case VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT:                                  \
  case VK_STRUCTURE_TYPE_IMAGE_CONSTRAINTS_INFO_FUCHSIA:                                    \
  case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:                \
  case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:                    \
  case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT:                          \
  case VK_STRUCTURE_TYPE_IMAGE_FORMAT_CONSTRAINTS_INFO_FUCHSIA:                             \
  case VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT:                                           \
  case VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT:                                          \
  case VK_STRUCTURE_TYPE_IMAGE_VIEW_ADDRESS_PROPERTIES_NVX:                                 \
  case VK_STRUCTURE_TYPE_IMAGE_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:                       \
  case VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX:                                        \
  case VK_STRUCTURE_TYPE_IMAGE_VIEW_SAMPLE_WEIGHT_CREATE_INFO_QCOM:                         \
  case VK_STRUCTURE_TYPE_IMAGE_VIEW_SLICED_CREATE_INFO_EXT:                                 \
  case VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIA:                           \
  case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:                               \
  case VK_STRUCTURE_TYPE_IMPORT_MEMORY_ZIRCON_HANDLE_INFO_FUCHSIA:                          \
  case VK_STRUCTURE_TYPE_IMPORT_METAL_BUFFER_INFO_EXT:                                      \
  case VK_STRUCTURE_TYPE_IMPORT_METAL_IO_SURFACE_INFO_EXT:                                  \
  case VK_STRUCTURE_TYPE_IMPORT_METAL_SHARED_EVENT_INFO_EXT:                                \
  case VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_IMPORT_SCREEN_BUFFER_INFO_QNX:                                     \
  case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA:                       \
  case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV:                           \
  case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV:                                 \
  case VK_STRUCTURE_TYPE_INITIALIZE_PERFORMANCE_API_INFO_INTEL:                             \
  case VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV:                                             \
  case VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV:                                        \
  case VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV:                                  \
  case VK_STRUCTURE_TYPE_LATENCY_SURFACE_CAPABILITIES_NV:                                   \
  case VK_STRUCTURE_TYPE_LATENCY_TIMINGS_FRAME_REPORT_NV:                                   \
  case VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT:                                    \
  case VK_STRUCTURE_TYPE_MEMORY_GET_REMOTE_ADDRESS_INFO_NV:                                 \
  case VK_STRUCTURE_TYPE_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA:                             \
  case VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT:                                \
  case VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR:                                               \
  case VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT:                                        \
  case VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT:                                          \
  case VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR:                                             \
  case VK_STRUCTURE_TYPE_MEMORY_ZIRCON_HANDLE_PROPERTIES_FUCHSIA:                           \
  case VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT:                                           \
  case VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT:                                     \
  case VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT:                                          \
  case VK_STRUCTURE_TYPE_MICROMAP_VERSION_INFO_EXT:                                         \
  case VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_ATTRIBUTES_INFO_NVX:                            \
  case VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_RENDER_AREAS_RENDER_PASS_BEGIN_INFO_QCOM:       \
  case VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT:                    \
  case VK_STRUCTURE_TYPE_OPTICAL_FLOW_EXECUTE_INFO_NV:                                      \
  case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV:                                 \
  case VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_PROPERTIES_NV:                           \
  case VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_INFO_NV:                               \
  case VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_PRIVATE_DATA_INFO_NV:                  \
  case VK_STRUCTURE_TYPE_OUT_OF_BAND_QUEUE_TYPE_INFO_NV:                                    \
  case VK_STRUCTURE_TYPE_PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL:                      \
  case VK_STRUCTURE_TYPE_PERFORMANCE_MARKER_INFO_INTEL:                                     \
  case VK_STRUCTURE_TYPE_PERFORMANCE_OVERRIDE_INFO_INTEL:                                   \
  case VK_STRUCTURE_TYPE_PERFORMANCE_STREAM_MARKER_INFO_INTEL:                              \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_AMIGO_PROFILING_FEATURES_SEC:                      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_FEATURES_HUAWEI:            \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_PROPERTIES_HUAWEI:          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_VRS_FEATURES_HUAWEI:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR:                   \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_KHR:                 \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_FEATURES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_NV:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_CLAMP_FEATURES_QCOM:                         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_WEIGHTS_FEATURES_QCOM:                       \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_FEATURES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_PROPERTIES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT:                   \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_DENSITY_MAP_PROPERTIES_EXT:      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_POOL_OVERALLOCATION_FEATURES_NV:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_SET_HOST_MAPPING_FEATURES_VALVE:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_COMPUTE_FEATURES_NV:     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT:                 \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT:                                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR:         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT: \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV:                     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_FEATURES_NV:         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_PROPERTIES_NV:       \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID:          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_PROPERTIES_ANDROID:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_RDMA_FEATURES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCREEN_BUFFER_FEATURES_QNX:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT:                                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV:         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT:                       \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT:                      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_FEATURES_MESA:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_PROPERTIES_MESA:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT:            \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_FEATURES_EXT:  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_FEATURES_QCOM:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_PROPERTIES_QCOM:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_FEATURES_QCOM:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_PROPERTIES_QCOM:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT:              \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV:            \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INVOCATION_MASK_FEATURES_HUAWEI:                   \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_DRIVER_PROPERTIES_MSFT:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_DITHERING_FEATURES_EXT:                     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_VERTEX_ATTRIBUTES_FEATURES_EXT:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_VERTEX_ATTRIBUTES_PROPERTIES_EXT:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR:                        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR:                      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR:                        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR:                      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_FEATURES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_PROPERTIES_NV:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV:                           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV:                         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT:                           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT:                         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX:      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_RENDER_AREAS_FEATURES_QCOM:     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT:                     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_PROPERTIES_EXT:                   \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV:                          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_PROPERTIES_NV:                        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PER_STAGE_DESCRIPTOR_SET_FEATURES_NV:              \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_HANDLES_FEATURES_EXT:       \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROPERTIES_FEATURES_EXT:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES_EXT:            \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_PROPERTIES_EXT:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_BARRIER_FEATURES_NV:                       \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV:                     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV:      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR:            \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV:                         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RELAXED_LINE_RASTERIZATION_FEATURES_IMG:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_FEATURES_ARM:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_PROPERTIES_ARM:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV:          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_FEATURES_ARM:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_PROPERTIES_ARM:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT16_VECTOR_FEATURES_NV:          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_FEATURES_ARM:                 \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_PROPERTIES_ARM:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD:                      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_ARM:                        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS_FEATURES_AMD: \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR:                 \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT_CONTROLS_2_FEATURES_KHR:              \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL:         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MAXIMAL_RECONVERGENCE_FEATURES_KHR:         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_PROPERTIES_EXT:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_QUAD_CONTROL_FEATURES_KHR:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR:  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT:         \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_ROTATE_FEATURES_KHR:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_PROPERTIES_EXT:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_MERGE_FEEDBACK_FEATURES_EXT:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_FEATURES_HUAWEI:                   \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_PROPERTIES_HUAWEI:                 \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM:                     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR:                             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_1_FEATURES_KHR:                  \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_DEGAMMA_FEATURES_QCOM:                       \
  case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:               \
  case VK_STRUCTURE_TYPE_PIPELINE_COMPILER_CONTROL_CREATE_INFO_AMD:                         \
  case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV:                 \
  case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV:                  \
  case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV:                   \
  case VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR:                           \
  case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV:          \
  case VK_STRUCTURE_TYPE_PIPELINE_INDIRECT_DEVICE_ADDRESS_INFO_NV:                          \
  case VK_STRUCTURE_TYPE_PIPELINE_PROPERTIES_IDENTIFIER_EXT:                                \
  case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:              \
  case VK_STRUCTURE_TYPE_PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV:        \
  case VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT:                               \
  case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT:           \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV:        \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV:          \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV:         \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV:                    \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV:                  \
  case VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR:                                           \
  case VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_INFO_KHR:                                      \
  case VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_WITH_TEMPLATE_INFO_KHR:                        \
  case VK_STRUCTURE_TYPE_QUERY_LOW_LATENCY_SUPPORT_NV:                                      \
  case VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL:                    \
  case VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR:                  \
  case VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_2_NV:                           \
  case VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV:                             \
  case VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR:                   \
  case VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR:                                 \
  case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV:                               \
  case VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV:                           \
  case VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_CONTROL_EXT:                                  \
  case VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_FEEDBACK_CREATE_INFO_EXT:                     \
  case VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_BEGIN_INFO_ARM:                                 \
  case VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_INFO_ARM:                                       \
  case VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_SUBMIT_INFO_ARM:                                \
  case VK_STRUCTURE_TYPE_RENDER_PASS_SUBPASS_FEEDBACK_CREATE_INFO_EXT:                      \
  case VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM:                             \
  case VK_STRUCTURE_TYPE_RENDERING_AREA_INFO_KHR:                                           \
  case VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR:                            \
  case VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR:                         \
  case VK_STRUCTURE_TYPE_SAMPLER_BLOCK_MATCH_WINDOW_CREATE_INFO_QCOM:                       \
  case VK_STRUCTURE_TYPE_SAMPLER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT:                          \
  case VK_STRUCTURE_TYPE_SAMPLER_CUBIC_WEIGHTS_CREATE_INFO_QCOM:                            \
  case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_YCBCR_DEGAMMA_CREATE_INFO_QCOM:           \
  case VK_STRUCTURE_TYPE_SCREEN_BUFFER_FORMAT_PROPERTIES_QNX:                               \
  case VK_STRUCTURE_TYPE_SCREEN_BUFFER_PROPERTIES_QNX:                                      \
  case VK_STRUCTURE_TYPE_SCREEN_SURFACE_CREATE_INFO_QNX:                                    \
  case VK_STRUCTURE_TYPE_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA:                          \
  case VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT:                            \
  case VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV:                                        \
  case VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT:                                      \
  case VK_STRUCTURE_TYPE_SUBPASS_SHADING_PIPELINE_CREATE_INFO_HUAWEI:                       \
  case VK_STRUCTURE_TYPE_SUBRESOURCE_HOST_MEMCPY_SIZE_EXT:                                  \
  case VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_EXT:                                          \
  case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_BARRIER_NV:                           \
  case VK_STRUCTURE_TYPE_SWAPCHAIN_LATENCY_CREATE_INFO_NV:                                  \
  case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_BARRIER_CREATE_INFO_NV:                          \
  case VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA:                                        \
  case VK_STRUCTURE_TYPE_TILE_PROPERTIES_QCOM:                                              \
  case VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR:                                       \
  case VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR:                                            \
  case VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR:                                     \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR:                                 \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PICTURE_INFO_KHR:                                 \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR:                                 \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR:               \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR:                                     \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR:                               \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR:              \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR:                               \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR:              \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR:                                             \
  case VK_STRUCTURE_TYPE_VIDEO_DECODE_USAGE_INFO_KHR:                                       \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR:                                     \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_KHR:                               \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_GOP_REMAINING_FRAME_INFO_KHR:                    \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_NALU_SLICE_INFO_KHR:                             \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PICTURE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_QUALITY_LEVEL_PROPERTIES_KHR:                    \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR:                           \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_KHR:                     \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_CREATE_INFO_KHR:                         \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR:              \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_FEEDBACK_INFO_KHR:            \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_DPB_SLOT_INFO_KHR:                               \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_GOP_REMAINING_FRAME_INFO_KHR:                    \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_NALU_SLICE_SEGMENT_INFO_KHR:                     \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_QUALITY_LEVEL_PROPERTIES_KHR:                    \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR:                           \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_LAYER_INFO_KHR:                     \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_CREATE_INFO_KHR:                         \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR:              \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_FEEDBACK_INFO_KHR:            \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR:                                             \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR:                               \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_PROPERTIES_KHR:                         \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR:                          \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR:                 \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR:                      \
  case VK_STRUCTURE_TYPE_VIDEO_ENCODE_USAGE_INFO_KHR:                                       \
  case VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR:                                         \
  case VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR:                                       \
  case VK_STRUCTURE_TYPE_VIDEO_INLINE_QUERY_INFO_KHR:                                       \
  case VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR:                                   \
  case VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR:                                            \
  case VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR:                                       \
  case VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR:                                     \
  case VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR:                                     \
  case VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR:                             \
  case VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR:                          \
  case VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR:                          \
  case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV:

size_t GetNextPatchSize(const void *pNext)
{
  const VkBaseInStructure *next = (const VkBaseInStructure *)pNext;
  size_t memSize = 0;

  while(next)
  {
#undef COPY_STRUCT_CAPTURE_ONLY
#define COPY_STRUCT_CAPTURE_ONLY(StructType, StructName) \
  case StructType:                                       \
    memSize += sizeof(StructName);                       \
    break;

#undef COPY_STRUCT
#define COPY_STRUCT(StructType, StructName) \
  case StructType:                          \
    memSize += sizeof(StructName);          \
    break;

#undef UNWRAP_STRUCT
#define UNWRAP_STRUCT(StructType, StructName, ...) \
  case StructType:                                 \
    memSize += sizeof(StructName);                 \
    break;

#undef UNWRAP_STRUCT_CAPTURE_ONLY
#define UNWRAP_STRUCT_CAPTURE_ONLY(StructType, StructName, ...) \
  case StructType: memSize += sizeof(StructName); break;

    switch(next->sType)
    {
      PROCESS_SIMPLE_STRUCTS();

      // complex structs to handle - require multiple allocations
      case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      {
        memSize += sizeof(VkBindSparseInfo);

        VkBindSparseInfo *info = (VkBindSparseInfo *)next;
        memSize += info->waitSemaphoreCount * sizeof(VkSemaphore);
        memSize += info->signalSemaphoreCount * sizeof(VkSemaphore);
        memSize += info->bufferBindCount * sizeof(VkSparseBufferMemoryBindInfo);
        memSize += info->imageOpaqueBindCount * sizeof(VkSparseImageOpaqueMemoryBindInfo);
        memSize += info->imageBindCount * sizeof(VkSparseImageMemoryBindInfo);
        for(uint32_t i = 0; i < info->bufferBindCount; i++)
          memSize += info->pBufferBinds[i].bindCount * sizeof(VkSparseMemoryBind);
        for(uint32_t i = 0; i < info->imageOpaqueBindCount; i++)
          memSize += info->pImageOpaqueBinds[i].bindCount * sizeof(VkSparseMemoryBind);
        for(uint32_t i = 0; i < info->imageBindCount; i++)
          memSize += info->pImageBinds[i].bindCount * sizeof(VkSparseImageMemoryBind);
        break;
      }
      case VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2:
      {
        memSize += sizeof(VkBlitImageInfo2);
        VkBlitImageInfo2 *info = (VkBlitImageInfo2 *)next;
        memSize += info->regionCount * sizeof(VkImageBlit2);
        for(uint32_t i = 0; i < info->regionCount; i++)
          memSize += GetNextPatchSize(info->pRegions[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO:
      {
        memSize += sizeof(VkCommandBufferInheritanceRenderingInfo);

        VkCommandBufferInheritanceRenderingInfo *info =
            (VkCommandBufferInheritanceRenderingInfo *)next;
        memSize += info->colorAttachmentCount * sizeof(VkFormat);
        break;
      }
      case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
      {
        memSize += sizeof(VkComputePipelineCreateInfo);
        break;
      }
      case VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2:
      {
        memSize += sizeof(VkCopyBufferInfo2);
        VkCopyBufferInfo2 *info = (VkCopyBufferInfo2 *)next;
        memSize += info->regionCount * sizeof(VkBufferCopy2);
        for(uint32_t i = 0; i < info->regionCount; i++)
          memSize += GetNextPatchSize(info->pRegions[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2:
      {
        memSize += sizeof(VkCopyBufferToImageInfo2);
        VkCopyBufferToImageInfo2 *info = (VkCopyBufferToImageInfo2 *)next;
        memSize += info->regionCount * sizeof(VkBufferImageCopy2);
        for(uint32_t i = 0; i < info->regionCount; i++)
          memSize += GetNextPatchSize(info->pRegions[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2:
      {
        memSize += sizeof(VkCopyImageToBufferInfo2);
        VkCopyImageToBufferInfo2 *info = (VkCopyImageToBufferInfo2 *)next;
        memSize += info->regionCount * sizeof(VkBufferImageCopy2);
        for(uint32_t i = 0; i < info->regionCount; i++)
          memSize += GetNextPatchSize(info->pRegions[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2:
      {
        memSize += sizeof(VkCopyImageInfo2);
        VkCopyImageInfo2 *info = (VkCopyImageInfo2 *)next;
        memSize += info->regionCount * sizeof(VkImageCopy2);
        for(uint32_t i = 0; i < info->regionCount; i++)
          memSize += GetNextPatchSize(info->pRegions[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_DEPENDENCY_INFO:
      {
        memSize += sizeof(VkDependencyInfo);

        VkDependencyInfo *info = (VkDependencyInfo *)next;

        memSize += info->memoryBarrierCount * sizeof(VkMemoryBarrier2);
        for(uint32_t i = 0; i < info->memoryBarrierCount; i++)
          memSize += GetNextPatchSize(info->pMemoryBarriers[i].pNext);

        memSize += info->bufferMemoryBarrierCount * sizeof(VkBufferMemoryBarrier2);
        for(uint32_t i = 0; i < info->bufferMemoryBarrierCount; i++)
          memSize += GetNextPatchSize(info->pBufferMemoryBarriers[i].pNext);

        memSize += info->imageMemoryBarrierCount * sizeof(VkImageMemoryBarrier2);
        for(uint32_t i = 0; i < info->imageMemoryBarrierCount; i++)
          memSize += GetNextPatchSize(info->pImageMemoryBarriers[i].pNext);

        break;
      }
      case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO:
      {
        memSize += sizeof(VkDescriptorSetAllocateInfo);

        VkDescriptorSetAllocateInfo *info = (VkDescriptorSetAllocateInfo *)next;
        memSize += info->descriptorSetCount * sizeof(VkDescriptorSetLayout);
        break;
      }
      case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO:
      {
        memSize += sizeof(VkDescriptorSetLayoutCreateInfo);

        VkDescriptorSetLayoutCreateInfo *info = (VkDescriptorSetLayoutCreateInfo *)next;
        memSize += info->bindingCount * sizeof(VkDescriptorSetLayoutBinding);

        for(uint32_t i = 0; i < info->bindingCount; i++)
          if(info->pBindings[i].pImmutableSamplers)
            memSize += info->pBindings[i].descriptorCount * sizeof(VkSampler);
        break;
      }
      case VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS:
      {
        memSize += sizeof(VkDeviceBufferMemoryRequirements);

        VkDeviceBufferMemoryRequirements *info = (VkDeviceBufferMemoryRequirements *)next;
        memSize += GetNextPatchSize(info->pCreateInfo);
        break;
      }
      case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
      {
        memSize += sizeof(VkDeviceGroupDeviceCreateInfo);

        VkDeviceGroupDeviceCreateInfo *info = (VkDeviceGroupDeviceCreateInfo *)next;
        memSize += info->physicalDeviceCount * sizeof(VkPhysicalDevice);
        break;
      }
      case VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS:
      {
        memSize += sizeof(VkDeviceImageMemoryRequirements);

        VkDeviceImageMemoryRequirements *info = (VkDeviceImageMemoryRequirements *)next;
        memSize += GetNextPatchSize(info->pCreateInfo);
        break;
      }
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO:
      {
        memSize += sizeof(VkFramebufferCreateInfo);

        VkFramebufferCreateInfo *info = (VkFramebufferCreateInfo *)next;
        memSize += info->attachmentCount * sizeof(VkImageView);
        break;
      }
      // this struct doesn't really need to be unwrapped but we allocate space for it since it
      // contains arrays that we will very commonly need to patch, to adjust image info/formats.
      // this saves us needing to iterate it outside and allocate extra space
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
      {
        memSize += sizeof(VkFramebufferAttachmentsCreateInfo);

        VkFramebufferAttachmentsCreateInfo *info = (VkFramebufferAttachmentsCreateInfo *)next;
        memSize += info->attachmentImageInfoCount * sizeof(VkFramebufferAttachmentImageInfo);

        for(uint32_t i = 0; i < info->attachmentImageInfoCount; i++)
          memSize += GetNextPatchSize(&info->pAttachmentImageInfos[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO:
      {
        memSize += sizeof(VkFramebufferAttachmentImageInfo);

        // we add space for an extra VkFormat so we can push one onto the list
        VkFramebufferAttachmentImageInfo *info = (VkFramebufferAttachmentImageInfo *)next;
        if(info->viewFormatCount > 0)
          memSize += (info->viewFormatCount + 1) * sizeof(VkFormat);

        break;
      }
      case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
      {
        memSize += sizeof(VkGraphicsPipelineCreateInfo);

        VkGraphicsPipelineCreateInfo *info = (VkGraphicsPipelineCreateInfo *)next;
        memSize += info->stageCount * sizeof(VkPipelineShaderStageCreateInfo);
        for(uint32_t s = 0; s < info->stageCount; s++)
          memSize += GetNextPatchSize(info->pStages[s].pNext);

        // need to copy the base struct of each of these so we can potentially patch pNext inside it
        if(info->pVertexInputState)
        {
          memSize += sizeof(*info->pVertexInputState);
          memSize += GetNextPatchSize(info->pVertexInputState->pNext);
        }
        if(info->pInputAssemblyState)
        {
          memSize += sizeof(*info->pInputAssemblyState);
          memSize += GetNextPatchSize(info->pInputAssemblyState->pNext);
        }
        if(info->pTessellationState)
        {
          memSize += sizeof(*info->pTessellationState);
          memSize += GetNextPatchSize(info->pTessellationState->pNext);
        }
        if(info->pViewportState)
        {
          memSize += sizeof(*info->pViewportState);
          memSize += GetNextPatchSize(info->pViewportState->pNext);
        }
        if(info->pRasterizationState)
        {
          memSize += sizeof(*info->pRasterizationState);
          memSize += GetNextPatchSize(info->pRasterizationState->pNext);
        }
        if(info->pMultisampleState)
        {
          memSize += sizeof(*info->pMultisampleState);
          memSize += GetNextPatchSize(info->pMultisampleState->pNext);
        }
        if(info->pDepthStencilState)
        {
          memSize += sizeof(*info->pDepthStencilState);
          memSize += GetNextPatchSize(info->pDepthStencilState->pNext);
        }
        if(info->pColorBlendState)
        {
          memSize += sizeof(*info->pColorBlendState);
          memSize += GetNextPatchSize(info->pColorBlendState->pNext);
        }
        if(info->pDynamicState)
        {
          memSize += sizeof(*info->pDynamicState);
          memSize += GetNextPatchSize(info->pDynamicState->pNext);
        }
        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO:
      {
        memSize += sizeof(VkPipelineLayoutCreateInfo);

        VkPipelineLayoutCreateInfo *info = (VkPipelineLayoutCreateInfo *)next;
        memSize += info->setLayoutCount * sizeof(VkDescriptorSetLayout);
        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:
      {
        memSize += sizeof(VkPipelineLibraryCreateInfoKHR);

        VkPipelineLibraryCreateInfoKHR *info = (VkPipelineLibraryCreateInfoKHR *)next;
        memSize += info->libraryCount * sizeof(VkPipeline);
        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:
      {
        memSize += sizeof(VkPipelineRenderingCreateInfo);

        VkPipelineRenderingCreateInfo *info = (VkPipelineRenderingCreateInfo *)next;
        memSize += info->colorAttachmentCount * sizeof(VkFormat);
        break;
      }
      case VK_STRUCTURE_TYPE_PRESENT_INFO_KHR:
      {
        memSize += sizeof(VkPresentInfoKHR);

        VkPresentInfoKHR *info = (VkPresentInfoKHR *)next;
        memSize += info->waitSemaphoreCount * sizeof(VkSemaphore);
        memSize += info->swapchainCount * sizeof(VkSwapchainKHR);
        break;
      }
      case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR:
      {
        memSize += sizeof(VkRayTracingPipelineCreateInfoKHR);

        VkRayTracingPipelineCreateInfoKHR *info = (VkRayTracingPipelineCreateInfoKHR *)next;
        memSize += info->stageCount * sizeof(VkPipelineShaderStageCreateInfo);
        for(uint32_t s = 0; s < info->stageCount; s++)
          memSize += GetNextPatchSize(info->pStages[s].pNext);
        memSize += info->groupCount * sizeof(VkRayTracingShaderGroupCreateInfoKHR);
        for(uint32_t g = 0; g < info->groupCount; g++)
          memSize += GetNextPatchSize(info->pGroups[g].pNext);

        // need to copy the base struct of each of these so we can potentially patch pNext inside it
        if(info->pLibraryInfo)
        {
          memSize += sizeof(*info->pLibraryInfo);
          memSize += info->pLibraryInfo->libraryCount * sizeof(VkPipeline);
          memSize += GetNextPatchSize(info->pLibraryInfo->pNext);
        }
        if(info->pLibraryInterface)
        {
          memSize += sizeof(*info->pLibraryInterface);
          memSize += GetNextPatchSize(info->pLibraryInterface->pNext);
        }
        if(info->pDynamicState)
        {
          memSize += sizeof(*info->pDynamicState);
          memSize += GetNextPatchSize(info->pDynamicState->pNext);
        }
        break;
      }
      case VK_STRUCTURE_TYPE_RENDERING_INFO:
      {
        memSize += sizeof(VkRenderingInfo);

        VkRenderingInfo *info = (VkRenderingInfo *)next;
        memSize += info->colorAttachmentCount * sizeof(VkRenderingAttachmentInfo);
        for(uint32_t i = 0; i < info->colorAttachmentCount; i++)
          memSize += GetNextPatchSize(info->pColorAttachments[i].pNext);
        if(info->pDepthAttachment)
        {
          memSize += sizeof(*info->pDepthAttachment);
          memSize += GetNextPatchSize(info->pDepthAttachment->pNext);
        }
        if(info->pStencilAttachment)
        {
          memSize += sizeof(*info->pStencilAttachment);
          memSize += GetNextPatchSize(info->pStencilAttachment->pNext);
        }
        break;
      }
      case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
      {
        memSize += sizeof(VkRenderPassAttachmentBeginInfo);

        VkRenderPassAttachmentBeginInfo *info = (VkRenderPassAttachmentBeginInfo *)next;
        memSize += info->attachmentCount * sizeof(VkImageView);
        break;
      }
      case VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2:
      {
        memSize += sizeof(VkResolveImageInfo2);
        VkResolveImageInfo2 *info = (VkResolveImageInfo2 *)next;
        memSize += info->regionCount * sizeof(VkImageResolve2);
        for(uint32_t i = 0; i < info->regionCount; i++)
          memSize += GetNextPatchSize(info->pRegions[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO:
      {
        memSize += sizeof(VkSemaphoreWaitInfo);

        VkSemaphoreWaitInfo *info = (VkSemaphoreWaitInfo *)next;
        memSize += info->semaphoreCount * sizeof(VkSemaphore);
        break;
      }
      case VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT:
      {
        memSize += sizeof(VkShaderCreateInfoEXT);

        VkShaderCreateInfoEXT *info = (VkShaderCreateInfoEXT *)next;
        memSize += info->setLayoutCount * sizeof(VkDescriptorSetLayout);
        break;
      }
      case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      {
        memSize += sizeof(VkSubmitInfo);

        VkSubmitInfo *info = (VkSubmitInfo *)next;
        memSize += info->waitSemaphoreCount * sizeof(VkSemaphore);
        memSize += info->commandBufferCount * sizeof(VkCommandBuffer);
        memSize += info->signalSemaphoreCount * sizeof(VkSemaphore);
        break;
      }
      case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      {
        memSize += sizeof(VkSubmitInfo2);

        VkSubmitInfo2 *info = (VkSubmitInfo2 *)next;

        memSize += info->waitSemaphoreInfoCount * sizeof(VkSemaphoreSubmitInfo);
        for(uint32_t i = 0; i < info->waitSemaphoreInfoCount; i++)
          memSize += GetNextPatchSize(info->pWaitSemaphoreInfos[i].pNext);

        memSize += info->commandBufferInfoCount * sizeof(VkCommandBufferSubmitInfo);
        for(uint32_t i = 0; i < info->commandBufferInfoCount; i++)
          memSize += GetNextPatchSize(info->pCommandBufferInfos[i].pNext);

        memSize += info->signalSemaphoreInfoCount * sizeof(VkSemaphoreSubmitInfo);
        for(uint32_t i = 0; i < info->signalSemaphoreInfoCount; i++)
          memSize += GetNextPatchSize(info->pSignalSemaphoreInfos[i].pNext);
        break;
      }
      case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT:
      {
        memSize += sizeof(VkSwapchainPresentFenceInfoEXT);

        VkSwapchainPresentFenceInfoEXT *info = (VkSwapchainPresentFenceInfoEXT *)next;
        memSize += info->swapchainCount * sizeof(VkFence);
        break;
      }
      case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
      {
        memSize += sizeof(VkWriteDescriptorSet);

        VkWriteDescriptorSet *info = (VkWriteDescriptorSet *)next;
        switch(info->descriptorType)
        {
          case VK_DESCRIPTOR_TYPE_SAMPLER:
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            memSize += info->descriptorCount * sizeof(VkDescriptorImageInfo);
            break;
          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            memSize += info->descriptorCount * sizeof(VkBufferView);
            break;
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            memSize += info->descriptorCount * sizeof(VkDescriptorBufferInfo);
            break;
          case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
          case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            // nothing to unwrap for these, they're on the next chain
            break;
          default: RDCERR("Unhandled descriptor type unwrapping VkWriteDescriptorSet"); break;
        }
        break;
      }
      case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR:
      {
        memSize += sizeof(VkWriteDescriptorSetAccelerationStructureKHR);

        VkWriteDescriptorSetAccelerationStructureKHR *info =
            (VkWriteDescriptorSetAccelerationStructureKHR *)next;
        memSize += info->accelerationStructureCount * sizeof(VkAccelerationStructureKHR);
        break;
      }

// Android External Buffer Memory Extension
#if ENABLED(RDOC_ANDROID)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                                 VkImportAndroidHardwareBufferInfoANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID,
                                 VkAndroidHardwareBufferUsageANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, VkExternalFormatANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
                                 VkAndroidHardwareBufferFormatPropertiesANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
                                 VkAndroidHardwareBufferPropertiesANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                                 VkMemoryGetAndroidHardwareBufferInfoANDROID);
#else
      case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID:
      case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID:
      case VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID:
      {
        RDCERR("Support for android external memory buffer extension not compiled in");
        break;
      }
#endif

#if ENABLED(RDOC_GGP)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP, VkPresentFrameTokenGGP);
#else
      case VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP:
      {
        RDCERR("Support for GGP frame token extension not compiled in");
        break;
      }
#endif

// NV win32 external memory extensions
#if ENABLED(RDOC_WIN32)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV,
                                 VkImportMemoryWin32HandleInfoNV);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV,
                                 VkExportMemoryWin32HandleInfoNV);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                 VkImportMemoryWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                 VkExportMemoryWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                                 VkMemoryWin32HandlePropertiesKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
                                 VkExportSemaphoreWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR,
                                 VkD3D12FenceSubmitInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR,
                                 VkExportFenceWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,
                                 VkSurfaceFullScreenExclusiveWin32InfoEXT);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT,
                                 VkSurfaceCapabilitiesFullScreenExclusiveEXT);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,
                                 VkSurfaceFullScreenExclusiveInfoEXT);

      case VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR:
        memSize += sizeof(VkMemoryGetWin32HandleInfoKHR);
        break;
      case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
        memSize += sizeof(VkImportSemaphoreWin32HandleInfoKHR);
        break;
      case VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR:
        memSize += sizeof(VkSemaphoreGetWin32HandleInfoKHR);
        break;
      case VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR:
        memSize += sizeof(VkImportFenceWin32HandleInfoKHR);
        break;
      case VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR:
        memSize += sizeof(VkFenceGetWin32HandleInfoKHR);
        break;
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
      {
        // the KHR and NV structs are identical
        memSize += sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR);

        VkWin32KeyedMutexAcquireReleaseInfoKHR *info = (VkWin32KeyedMutexAcquireReleaseInfoKHR *)next;
        memSize += info->acquireCount * sizeof(VkDeviceMemory);
        memSize += info->releaseCount * sizeof(VkDeviceMemory);
        break;
      }
#else
      case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR:
      case VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR:
      case VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT:
      case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT:
      case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT:
      case VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
      {
        RDCERR("Support for win32 external memory extensions not compiled in");
        break;
      }
#endif

      case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT:
      {
        // could be implemented but would need extra work or doesn't make sense right now
        RDCERR("Struct %s not handled in pNext chain", ToStr(next->sType).c_str());
        break;
      }

        UNHANDLED_STRUCTS()
        {
          RDCERR("Unhandled struct %s in pNext chain", ToStr(next->sType).c_str());
          break;
        }

      case VK_STRUCTURE_TYPE_MAX_ENUM:
        RDCERR("Invalid value %u in pNext chain", next->sType);
        break;
    }

    next = next->pNext;
  }

  return memSize;
}

void PreprocessNextChain(const VkBaseInStructure *nextInput, NextChainFlags &nextChainFlags)
{
  while(nextInput)
  {
    switch(nextInput->sType)
    {
      case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT:
      {
        const VkGraphicsPipelineLibraryCreateInfoEXT *libCreateInfo =
            (const VkGraphicsPipelineLibraryCreateInfoEXT *)nextInput;
        nextChainFlags.dynRenderingFormatsValid =
            (libCreateInfo->flags &
             VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) != 0;
        break;
      }
      default: break;
    }

    nextInput = nextInput->pNext;
  }
}

void UnwrapNextChain(CaptureState state, const char *structName, byte *&tempMem,
                     VkBaseInStructure *infoStruct)
{
  if(!infoStruct)
    return;

  NextChainFlags nextChainFlags;
  PreprocessNextChain(infoStruct, nextChainFlags);

  // during capture, this walks the pNext chain and either copies structs that can be passed
  // straight through, or copies and modifies any with vulkan objects that need to be unwrapped.
  //
  // during replay, we do the same thing to prepare for dispatching to the driver, but we also strip
  // out any structs we don't want to replay - e.g. external memory. This means the data is
  // serialised and available for future use and for user inspection, but isn't replayed when not
  // necesary.

  VkBaseInStructure *nextChainTail = infoStruct;
  const VkBaseInStructure *nextInput = (const VkBaseInStructure *)infoStruct->pNext;

#undef COPY_STRUCT_CAPTURE_ONLY
#define COPY_STRUCT_CAPTURE_ONLY(StructType, StructName)                            \
  case StructType:                                                                  \
  {                                                                                 \
    if(IsCaptureMode(state))                                                        \
      CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;                                                                          \
  }

#undef COPY_STRUCT
#define COPY_STRUCT(StructType, StructName)                                       \
  case StructType:                                                                \
  {                                                                               \
    CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;                                                                        \
  }

#undef UNWRAP_STRUCT_INNER
#define UNWRAP_STRUCT_INNER(StructType, StructName, ...)      \
  {                                                           \
    const StructName *in = (const StructName *)nextInput;     \
    StructName *out = (StructName *)tempMem;                  \
                                                              \
    /* copy the struct */                                     \
    *out = *in;                                               \
    /* abuse comma operator to unwrap all members */          \
    __VA_ARGS__;                                              \
                                                              \
    AppendModifiedChainedStruct(tempMem, out, nextChainTail); \
  }

#undef UNWRAP_STRUCT
#define UNWRAP_STRUCT(StructType, StructName, ...)           \
  case StructType:                                           \
  {                                                          \
    UNWRAP_STRUCT_INNER(StructType, StructName, __VA_ARGS__) \
    break;                                                   \
  }

#undef UNWRAP_STRUCT_CAPTURE_ONLY
#define UNWRAP_STRUCT_CAPTURE_ONLY(StructType, StructName, ...) \
  case StructType:                                              \
  {                                                             \
    if(IsCaptureMode(state))                                    \
    {                                                           \
      UNWRAP_STRUCT_INNER(StructType, StructName, __VA_ARGS__)  \
    }                                                           \
    break;                                                      \
  }

  // start with an empty chain. Every call to AppendModifiedChainedStruct / CopyNextChainedStruct
  // pushes on a new entry, but if there's only one entry in the list and it's one we want to skip,
  // this needs to start at NULL.
  nextChainTail->pNext = NULL;
  while(nextInput)
  {
    switch(nextInput->sType)
    {
      PROCESS_SIMPLE_STRUCTS();

      // complex structs to handle - require multiple allocations
      case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
      {
        const VkBindSparseInfo *in = (const VkBindSparseInfo *)nextInput;
        VkBindSparseInfo *out = (VkBindSparseInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped arrays
        VkSemaphore *outWaitSemaphores = (VkSemaphore *)tempMem;
        tempMem += sizeof(VkSemaphore) * in->waitSemaphoreCount;
        VkSemaphore *outSignalSemaphores = (VkSemaphore *)tempMem;
        tempMem += sizeof(VkSemaphore) * in->signalSemaphoreCount;
        VkSparseBufferMemoryBindInfo *outBufferBinds = (VkSparseBufferMemoryBindInfo *)tempMem;
        tempMem += sizeof(VkSparseBufferMemoryBindInfo) * in->bufferBindCount;
        VkSparseImageOpaqueMemoryBindInfo *outImageOpaqueBinds =
            (VkSparseImageOpaqueMemoryBindInfo *)tempMem;
        tempMem += sizeof(VkSparseImageOpaqueMemoryBindInfo) * in->imageOpaqueBindCount;
        VkSparseImageMemoryBindInfo *outImageBinds = (VkSparseImageMemoryBindInfo *)tempMem;
        tempMem += sizeof(VkSparseImageMemoryBindInfo) * in->imageBindCount;

        *out = *in;

        out->pWaitSemaphores = outWaitSemaphores;
        out->pSignalSemaphores = outSignalSemaphores;
        out->pBufferBinds = outBufferBinds;
        out->pImageOpaqueBinds = outImageOpaqueBinds;
        out->pImageBinds = outImageBinds;

        for(uint32_t i = 0; i < in->waitSemaphoreCount; i++)
          outWaitSemaphores[i] = Unwrap(in->pWaitSemaphores[i]);
        for(uint32_t i = 0; i < in->signalSemaphoreCount; i++)
          outSignalSemaphores[i] = Unwrap(in->pSignalSemaphores[i]);

        VkSparseMemoryBind *outMemoryBinds = (VkSparseMemoryBind *)tempMem;

        for(uint32_t i = 0; i < in->bufferBindCount; i++)
        {
          outBufferBinds[i] = in->pBufferBinds[i];
          UnwrapInPlace(outBufferBinds[i].buffer);

          outBufferBinds[i].pBinds = outMemoryBinds;

          for(uint32_t b = 0; b < outBufferBinds[i].bindCount; b++)
          {
            outMemoryBinds[b] = in->pBufferBinds[i].pBinds[b];
            UnwrapInPlace(outMemoryBinds[b].memory);
          }

          outMemoryBinds += outBufferBinds[i].bindCount;
          tempMem += outBufferBinds[i].bindCount * sizeof(VkSparseMemoryBind);
        }

        for(uint32_t i = 0; i < in->imageOpaqueBindCount; i++)
        {
          outImageOpaqueBinds[i] = in->pImageOpaqueBinds[i];
          UnwrapInPlace(outImageOpaqueBinds[i].image);

          outImageOpaqueBinds[i].pBinds = outMemoryBinds;

          for(uint32_t b = 0; b < outBufferBinds[i].bindCount; b++)
          {
            outMemoryBinds[b] = in->pImageOpaqueBinds[i].pBinds[b];
            UnwrapInPlace(outMemoryBinds[b].memory);
          }

          outMemoryBinds += outImageOpaqueBinds[i].bindCount;
          tempMem += outImageOpaqueBinds[i].bindCount * sizeof(VkSparseMemoryBind);
        }

        VkSparseImageMemoryBind *outImageMemoryBinds = (VkSparseImageMemoryBind *)tempMem;

        for(uint32_t i = 0; i < in->imageBindCount; i++)
        {
          outImageBinds[i] = in->pImageBinds[i];
          UnwrapInPlace(outImageBinds[i].image);

          outImageBinds[i].pBinds = outImageMemoryBinds;

          for(uint32_t b = 0; b < outBufferBinds[i].bindCount; b++)
          {
            outImageMemoryBinds[b] = in->pImageBinds[i].pBinds[b];
            UnwrapInPlace(outImageMemoryBinds[b].memory);
          }

          outImageMemoryBinds += outImageBinds[i].bindCount;
          tempMem += outImageBinds[i].bindCount * sizeof(VkSparseMemoryBind);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2:
      {
        const VkBlitImageInfo2 *in = (const VkBlitImageInfo2 *)nextInput;
        VkBlitImageInfo2 *out = (VkBlitImageInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkImageBlit2 *outRegions = (VkImageBlit2 *)tempMem;
        tempMem += sizeof(VkImageBlit2) * in->regionCount;

        *out = *in;
        UnwrapInPlace(out->srcImage);
        UnwrapInPlace(out->dstImage);

        out->pRegions = outRegions;
        for(uint32_t i = 0; i < in->regionCount; i++)
        {
          outRegions[i] = in->pRegions[i];
          UnwrapNextChain(state, "VkImageBlit2", tempMem, (VkBaseInStructure *)&outRegions[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO:
      {
        const VkCommandBufferInheritanceRenderingInfo *in =
            (const VkCommandBufferInheritanceRenderingInfo *)nextInput;
        VkCommandBufferInheritanceRenderingInfo *out =
            (VkCommandBufferInheritanceRenderingInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkFormat *outFormats = (VkFormat *)tempMem;
        tempMem += sizeof(VkFormat) * in->colorAttachmentCount;

        *out = *in;

        out->pColorAttachmentFormats = outFormats;
        for(uint32_t i = 0; i < in->colorAttachmentCount; i++)
          outFormats[i] = in->pColorAttachmentFormats[i];

        break;
      }
      case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
      {
        const VkComputePipelineCreateInfo *in = (const VkComputePipelineCreateInfo *)nextInput;
        VkComputePipelineCreateInfo *out = (VkComputePipelineCreateInfo *)tempMem;

        *out = *in;
        UnwrapInPlace(out->layout);
        UnwrapInPlace(out->stage.module);
        if(out->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
          UnwrapInPlace(out->basePipelineHandle);

        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        break;
      }
      case VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2:
      {
        const VkCopyBufferInfo2 *in = (const VkCopyBufferInfo2 *)nextInput;
        VkCopyBufferInfo2 *out = (VkCopyBufferInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkBufferCopy2 *outRegions = (VkBufferCopy2 *)tempMem;
        tempMem += sizeof(VkBufferCopy2) * in->regionCount;

        *out = *in;
        UnwrapInPlace(out->srcBuffer);
        UnwrapInPlace(out->dstBuffer);

        out->pRegions = outRegions;
        for(uint32_t i = 0; i < in->regionCount; i++)
        {
          outRegions[i] = in->pRegions[i];
          UnwrapNextChain(state, "VkBufferCopy2", tempMem, (VkBaseInStructure *)&outRegions[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2:
      {
        const VkCopyBufferToImageInfo2 *in = (const VkCopyBufferToImageInfo2 *)nextInput;
        VkCopyBufferToImageInfo2 *out = (VkCopyBufferToImageInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkBufferImageCopy2 *outRegions = (VkBufferImageCopy2 *)tempMem;
        tempMem += sizeof(VkBufferImageCopy2) * in->regionCount;

        *out = *in;
        UnwrapInPlace(out->srcBuffer);
        UnwrapInPlace(out->dstImage);

        out->pRegions = outRegions;
        for(uint32_t i = 0; i < in->regionCount; i++)
        {
          outRegions[i] = in->pRegions[i];
          UnwrapNextChain(state, "VkBufferImageCopy2", tempMem, (VkBaseInStructure *)&outRegions[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2:
      {
        const VkCopyImageToBufferInfo2 *in = (const VkCopyImageToBufferInfo2 *)nextInput;
        VkCopyImageToBufferInfo2 *out = (VkCopyImageToBufferInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkBufferImageCopy2 *outRegions = (VkBufferImageCopy2 *)tempMem;
        tempMem += sizeof(VkBufferImageCopy2) * in->regionCount;

        *out = *in;
        UnwrapInPlace(out->srcImage);
        UnwrapInPlace(out->dstBuffer);

        out->pRegions = outRegions;
        for(uint32_t i = 0; i < in->regionCount; i++)
        {
          outRegions[i] = in->pRegions[i];
          UnwrapNextChain(state, "VkBufferImageCopy2", tempMem, (VkBaseInStructure *)&outRegions[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2:
      {
        const VkCopyImageInfo2 *in = (const VkCopyImageInfo2 *)nextInput;
        VkCopyImageInfo2 *out = (VkCopyImageInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkImageCopy2 *outRegions = (VkImageCopy2 *)tempMem;
        tempMem += sizeof(VkImageCopy2) * in->regionCount;

        *out = *in;
        UnwrapInPlace(out->srcImage);
        UnwrapInPlace(out->dstImage);

        out->pRegions = outRegions;
        for(uint32_t i = 0; i < in->regionCount; i++)
        {
          outRegions[i] = in->pRegions[i];
          UnwrapNextChain(state, "VkImageCopy2", tempMem, (VkBaseInStructure *)&outRegions[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_DEPENDENCY_INFO:
      {
        const VkDependencyInfo *in = (const VkDependencyInfo *)nextInput;
        VkDependencyInfo *out = (VkDependencyInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped arrays
        VkMemoryBarrier2 *outMemoryBarriers = (VkMemoryBarrier2 *)tempMem;
        tempMem += sizeof(VkMemoryBarrier2) * in->memoryBarrierCount;
        VkBufferMemoryBarrier2 *outBufferBarriers = (VkBufferMemoryBarrier2 *)tempMem;
        tempMem += sizeof(VkBufferMemoryBarrier2) * in->bufferMemoryBarrierCount;
        VkImageMemoryBarrier2 *outImageBarriers = (VkImageMemoryBarrier2 *)tempMem;
        tempMem += sizeof(VkImageMemoryBarrier2) * in->imageMemoryBarrierCount;

        *out = *in;
        out->pMemoryBarriers = outMemoryBarriers;
        out->pBufferMemoryBarriers = outBufferBarriers;
        out->pImageMemoryBarriers = outImageBarriers;

        for(uint32_t i = 0; i < in->memoryBarrierCount; i++)
        {
          outMemoryBarriers[i] = in->pMemoryBarriers[i];
          UnwrapNextChain(state, "VkMemoryBarrier2", tempMem,
                          (VkBaseInStructure *)&outMemoryBarriers[i]);
        }

        for(uint32_t i = 0; i < in->bufferMemoryBarrierCount; i++)
        {
          outBufferBarriers[i] = in->pBufferMemoryBarriers[i];
          UnwrapInPlace(outBufferBarriers[i].buffer);
          UnwrapNextChain(state, "VkBufferMemoryBarrier2", tempMem,
                          (VkBaseInStructure *)&outBufferBarriers[i]);
        }

        for(uint32_t i = 0; i < in->imageMemoryBarrierCount; i++)
        {
          outImageBarriers[i] = in->pImageMemoryBarriers[i];
          UnwrapInPlace(outImageBarriers[i].image);
          UnwrapNextChain(state, "VkImageMemoryBarrier2", tempMem,
                          (VkBaseInStructure *)&outImageBarriers[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO:
      {
        const VkDescriptorSetAllocateInfo *in = (const VkDescriptorSetAllocateInfo *)nextInput;
        VkDescriptorSetAllocateInfo *out = (VkDescriptorSetAllocateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkDescriptorSetLayout *outLayouts = (VkDescriptorSetLayout *)tempMem;
        tempMem += sizeof(VkDescriptorSetLayout) * in->descriptorSetCount;

        *out = *in;
        UnwrapInPlace(out->descriptorPool);

        out->pSetLayouts = outLayouts;
        for(uint32_t i = 0; i < in->descriptorSetCount; i++)
          outLayouts[i] = Unwrap(in->pSetLayouts[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO:
      {
        const VkDescriptorSetLayoutCreateInfo *in =
            (const VkDescriptorSetLayoutCreateInfo *)nextInput;
        VkDescriptorSetLayoutCreateInfo *out = (VkDescriptorSetLayoutCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkDescriptorSetLayoutBinding *outBindings = (VkDescriptorSetLayoutBinding *)tempMem;
        tempMem += sizeof(VkDescriptorSetLayoutBinding) * in->bindingCount;
        VkSampler *outSamplers = (VkSampler *)tempMem;

        *out = *in;
        out->pBindings = outBindings;

        for(uint32_t i = 0; i < out->bindingCount; i++)
        {
          outBindings[i] = in->pBindings[i];

          if(outBindings[i].pImmutableSamplers)
          {
            outBindings[i].pImmutableSamplers = outSamplers;

            for(uint32_t d = 0; d < out->pBindings[i].descriptorCount; d++)
              outSamplers[d] = Unwrap(in->pBindings[i].pImmutableSamplers[d]);

            tempMem += sizeof(VkSampler) * out->pBindings[i].descriptorCount;
            outSamplers += out->pBindings[i].descriptorCount;
          }
        }

        break;
      }
      case VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS:
      {
        const VkDeviceBufferMemoryRequirements *in =
            (const VkDeviceBufferMemoryRequirements *)nextInput;
        VkDeviceBufferMemoryRequirements *out = (VkDeviceBufferMemoryRequirements *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        out->sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
        out->pNext = in->pNext;

        out->pCreateInfo = AllocStructCopy(tempMem, in->pCreateInfo);
        UnwrapNextChain(state, "VkBufferCreateInfo", tempMem, (VkBaseInStructure *)out->pCreateInfo);

        break;
      }
      case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
      {
        const VkDeviceGroupDeviceCreateInfo *in = (const VkDeviceGroupDeviceCreateInfo *)nextInput;
        VkDeviceGroupDeviceCreateInfo *out = (VkDeviceGroupDeviceCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkPhysicalDevice *outDevices = (VkPhysicalDevice *)tempMem;
        tempMem += sizeof(VkPhysicalDevice) * in->physicalDeviceCount;

        *out = *in;
        out->pPhysicalDevices = outDevices;

        for(uint32_t i = 0; i < in->physicalDeviceCount; i++)
          outDevices[i] = Unwrap(in->pPhysicalDevices[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS:
      {
        const VkDeviceImageMemoryRequirements *in =
            (const VkDeviceImageMemoryRequirements *)nextInput;
        VkDeviceImageMemoryRequirements *out = (VkDeviceImageMemoryRequirements *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        out->sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
        out->pNext = in->pNext;
        out->planeAspect = in->planeAspect;

        out->pCreateInfo = AllocStructCopy(tempMem, in->pCreateInfo);
        UnwrapNextChain(state, "VkImageCreateInfo", tempMem, (VkBaseInStructure *)out->pCreateInfo);

        break;
      }
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO:
      {
        const VkFramebufferCreateInfo *in = (const VkFramebufferCreateInfo *)nextInput;
        VkFramebufferCreateInfo *out = (VkFramebufferCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkImageView *outAttachments = (VkImageView *)tempMem;
        tempMem += sizeof(VkImageView) * in->attachmentCount;

        *out = *in;
        UnwrapInPlace(out->renderPass);

        if((out->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT) == 0)
        {
          out->pAttachments = outAttachments;
          for(uint32_t i = 0; i < in->attachmentCount; i++)
            outAttachments[i] = Unwrap(in->pAttachments[i]);
        }

        break;
      }
      // this struct doesn't really need to be unwrapped but we allocate space for it since it
      // contains arrays that we will very commonly need to patch, to adjust image info/formats.
      // this saves us needing to iterate it outside and allocate extra space
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
      {
        const VkFramebufferAttachmentsCreateInfo *in =
            (const VkFramebufferAttachmentsCreateInfo *)nextInput;
        VkFramebufferAttachmentsCreateInfo *out = (VkFramebufferAttachmentsCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkFramebufferAttachmentImageInfo *outAtts = (VkFramebufferAttachmentImageInfo *)tempMem;
        tempMem += sizeof(VkFramebufferAttachmentImageInfo) * in->attachmentImageInfoCount;

        *out = *in;
        out->pAttachmentImageInfos = outAtts;
        for(uint32_t i = 0; i < in->attachmentImageInfoCount; i++)
        {
          outAtts[i] = in->pAttachmentImageInfos[i];
          UnwrapNextChain(state, "VkFramebufferAttachmentImageInfo", tempMem,
                          (VkBaseInStructure *)&outAtts[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO:
      {
        const VkFramebufferAttachmentImageInfo *in =
            (const VkFramebufferAttachmentImageInfo *)nextInput;
        VkFramebufferAttachmentImageInfo *out = (VkFramebufferAttachmentImageInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        *out = *in;

        // allocate extra array
        if(in->viewFormatCount > 0)
        {
          VkFormat *outFormats = (VkFormat *)tempMem;
          tempMem += sizeof(VkFormat) * (in->viewFormatCount + 1);

          out->pViewFormats = outFormats;
          memcpy(outFormats, in->pViewFormats, sizeof(VkFormat) * in->viewFormatCount);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
      {
        const VkGraphicsPipelineCreateInfo *in = (const VkGraphicsPipelineCreateInfo *)nextInput;
        VkGraphicsPipelineCreateInfo *out = (VkGraphicsPipelineCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkPipelineShaderStageCreateInfo *outShaders = (VkPipelineShaderStageCreateInfo *)tempMem;
        tempMem += sizeof(VkPipelineShaderStageCreateInfo) * in->stageCount;

        out->sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        out->pNext = in->pNext;
        out->flags = in->flags;
        out->stageCount = in->stageCount;
        out->pStages = outShaders;
        for(uint32_t i = 0; i < in->stageCount; i++)
        {
          outShaders[i] = in->pStages[i];
          UnwrapInPlace(outShaders[i].module);
          UnwrapNextChain(state, "VkPipelineShaderStageCreateInfo", tempMem,
                          (VkBaseInStructure *)&outShaders[i]);
        }

        out->pVertexInputState = AllocStructCopy(tempMem, in->pVertexInputState);
        UnwrapNextChain(state, "VkPipelineVertexInputStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pVertexInputState);
        out->pInputAssemblyState = AllocStructCopy(tempMem, in->pInputAssemblyState);
        UnwrapNextChain(state, "VkPipelineInputAssemblyStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pInputAssemblyState);
        out->pTessellationState = AllocStructCopy(tempMem, in->pTessellationState);
        UnwrapNextChain(state, "VkPipelineTessellationStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pTessellationState);
        out->pViewportState = AllocStructCopy(tempMem, in->pViewportState);
        UnwrapNextChain(state, "VkPipelineViewportStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pViewportState);
        out->pRasterizationState = AllocStructCopy(tempMem, in->pRasterizationState);
        UnwrapNextChain(state, "VkPipelineRasterizationStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pRasterizationState);
        out->pMultisampleState = AllocStructCopy(tempMem, in->pMultisampleState);
        UnwrapNextChain(state, "VkPipelineMultisampleStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pMultisampleState);
        out->pDepthStencilState = AllocStructCopy(tempMem, in->pDepthStencilState);
        UnwrapNextChain(state, "VkPipelineDepthStencilStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pDepthStencilState);
        out->pColorBlendState = AllocStructCopy(tempMem, in->pColorBlendState);
        UnwrapNextChain(state, "VkPipelineColorBlendStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pColorBlendState);
        out->pDynamicState = AllocStructCopy(tempMem, in->pDynamicState);
        UnwrapNextChain(state, "VkPipelineDynamicStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pDynamicState);

        UnwrapInPlace(out->layout);
        UnwrapInPlace(out->renderPass);
        out->subpass = in->subpass;
        if(out->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
          UnwrapInPlace(out->basePipelineHandle);
        else
          out->basePipelineHandle = VK_NULL_HANDLE;
        out->basePipelineIndex = in->basePipelineIndex;

        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO:
      {
        const VkPipelineLayoutCreateInfo *in = (const VkPipelineLayoutCreateInfo *)nextInput;
        VkPipelineLayoutCreateInfo *out = (VkPipelineLayoutCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkDescriptorSetLayout *outLayouts = (VkDescriptorSetLayout *)tempMem;
        tempMem += sizeof(VkDescriptorSetLayout) * in->setLayoutCount;

        *out = *in;

        out->pSetLayouts = outLayouts;
        for(uint32_t i = 0; i < in->setLayoutCount; i++)
          outLayouts[i] = Unwrap(in->pSetLayouts[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:
      {
        const VkPipelineLibraryCreateInfoKHR *in = (const VkPipelineLibraryCreateInfoKHR *)nextInput;
        VkPipelineLibraryCreateInfoKHR *out = (VkPipelineLibraryCreateInfoKHR *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkPipeline *outLibraries = (VkPipeline *)tempMem;
        tempMem += sizeof(VkPipeline) * in->libraryCount;

        *out = *in;

        out->pLibraries = outLibraries;
        for(uint32_t i = 0; i < in->libraryCount; i++)
          outLibraries[i] = Unwrap(in->pLibraries[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:
      {
        const VkPipelineRenderingCreateInfo *in = (const VkPipelineRenderingCreateInfo *)nextInput;
        VkPipelineRenderingCreateInfo *out = (VkPipelineRenderingCreateInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkFormat *outFormats = (VkFormat *)tempMem;
        tempMem += sizeof(VkFormat) * in->colorAttachmentCount;

        *out = *in;

        out->pColorAttachmentFormats = outFormats;
        if(nextChainFlags.dynRenderingFormatsValid)
        {
          for(uint32_t i = 0; i < in->colorAttachmentCount; i++)
            outFormats[i] = in->pColorAttachmentFormats[i];
        }

        break;
      }
      case VK_STRUCTURE_TYPE_PRESENT_INFO_KHR:
      {
        const VkPresentInfoKHR *in = (const VkPresentInfoKHR *)nextInput;
        VkPresentInfoKHR *out = (VkPresentInfoKHR *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped arrays
        VkSemaphore *outWaitSemaphores = (VkSemaphore *)tempMem;
        tempMem += sizeof(VkSemaphore) * in->waitSemaphoreCount;
        VkSwapchainKHR *outSwapchains = (VkSwapchainKHR *)tempMem;
        tempMem += sizeof(VkSwapchainKHR) * in->swapchainCount;

        *out = *in;
        out->pSwapchains = outSwapchains;
        out->pWaitSemaphores = outWaitSemaphores;

        for(uint32_t i = 0; i < in->swapchainCount; i++)
          outSwapchains[i] = Unwrap(in->pSwapchains[i]);
        for(uint32_t i = 0; i < in->waitSemaphoreCount; i++)
          outWaitSemaphores[i] = Unwrap(in->pWaitSemaphores[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR:
      {
        const VkRayTracingPipelineCreateInfoKHR *in =
            (const VkRayTracingPipelineCreateInfoKHR *)nextInput;
        VkRayTracingPipelineCreateInfoKHR *out = (VkRayTracingPipelineCreateInfoKHR *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkPipelineShaderStageCreateInfo *outShaders = (VkPipelineShaderStageCreateInfo *)tempMem;
        tempMem += sizeof(VkPipelineShaderStageCreateInfo) * in->stageCount;
        VkRayTracingShaderGroupCreateInfoKHR *outGroups =
            (VkRayTracingShaderGroupCreateInfoKHR *)tempMem;
        tempMem += sizeof(VkRayTracingShaderGroupCreateInfoKHR) * in->groupCount;

        *out = *in;
        out->pStages = outShaders;
        for(uint32_t i = 0; i < in->stageCount; i++)
        {
          outShaders[i] = in->pStages[i];
          UnwrapInPlace(outShaders[i].module);
          UnwrapNextChain(state, "VkPipelineShaderStageCreateInfo", tempMem,
                          (VkBaseInStructure *)&outShaders[i]);
        }
        out->pGroups = outGroups;
        for(uint32_t i = 0; i < in->groupCount; i++)
        {
          outGroups[i] = in->pGroups[i];
          UnwrapNextChain(state, "VkRayTracingShaderGroupCreateInfoKHR", tempMem,
                          (VkBaseInStructure *)&outGroups[i]);
        }

        out->pLibraryInfo = AllocStructCopy(tempMem, in->pLibraryInfo);
        if(out->pLibraryInfo)
        {
          VkPipelineLibraryCreateInfoKHR *outLibraryInfo =
              (VkPipelineLibraryCreateInfoKHR *)out->pLibraryInfo;
          VkPipeline *outLibraries = (VkPipeline *)tempMem;
          outLibraryInfo->pLibraries = outLibraries;
          tempMem += sizeof(VkPipeline) * in->pLibraryInfo->libraryCount;
          for(uint32_t i = 0; i < in->pLibraryInfo->libraryCount; i++)
            outLibraries[i] = Unwrap(in->pLibraryInfo->pLibraries[i]);
        }
        UnwrapNextChain(state, "VkPipelineLibraryCreateInfoKHR", tempMem,
                        (VkBaseInStructure *)out->pLibraryInfo);
        out->pLibraryInterface = AllocStructCopy(tempMem, in->pLibraryInterface);
        UnwrapNextChain(state, "VkRayTracingPipelineInterfaceCreateInfoKHR", tempMem,
                        (VkBaseInStructure *)out->pLibraryInterface);
        out->pDynamicState = AllocStructCopy(tempMem, in->pDynamicState);
        UnwrapNextChain(state, "VkPipelineDynamicStateCreateInfo", tempMem,
                        (VkBaseInStructure *)out->pDynamicState);
        UnwrapInPlace(out->layout);
        if(out->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
          UnwrapInPlace(out->basePipelineHandle);
        else
          out->basePipelineHandle = VK_NULL_HANDLE;
        out->basePipelineIndex = in->basePipelineIndex;

        break;
      }
      case VK_STRUCTURE_TYPE_RENDERING_INFO:
      {
        const VkRenderingInfo *in = (const VkRenderingInfo *)nextInput;
        VkRenderingInfo *out = (VkRenderingInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkRenderingAttachmentInfo *outAttachs = (VkRenderingAttachmentInfo *)tempMem;
        tempMem += sizeof(VkRenderingAttachmentInfo) * in->colorAttachmentCount;

        out->sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        out->pNext = in->pNext;
        out->flags = in->flags;
        out->renderArea = in->renderArea;
        out->layerCount = in->layerCount;
        out->viewMask = in->viewMask;
        out->colorAttachmentCount = in->colorAttachmentCount;
        out->pColorAttachments = outAttachs;
        for(uint32_t i = 0; i < in->colorAttachmentCount; i++)
        {
          outAttachs[i] = in->pColorAttachments[i];
          UnwrapInPlace(outAttachs[i].imageView);
          UnwrapInPlace(outAttachs[i].resolveImageView);
          UnwrapNextChain(state, "VkRenderingAttachmentInfo", tempMem,
                          (VkBaseInStructure *)&outAttachs[i]);
        }

        if(in->pDepthAttachment)
        {
          VkRenderingAttachmentInfo *depth = (VkRenderingAttachmentInfo *)tempMem;
          out->pDepthAttachment = depth;
          tempMem += sizeof(VkRenderingAttachmentInfo);

          *depth = *in->pDepthAttachment;
          UnwrapInPlace(depth->imageView);
          UnwrapInPlace(depth->resolveImageView);
          UnwrapNextChain(state, "VkRenderingAttachmentInfo", tempMem, (VkBaseInStructure *)depth);
        }
        else
        {
          out->pDepthAttachment = NULL;
        }

        if(in->pStencilAttachment)
        {
          VkRenderingAttachmentInfo *stencil = (VkRenderingAttachmentInfo *)tempMem;
          out->pStencilAttachment = stencil;
          tempMem += sizeof(VkRenderingAttachmentInfo);

          *stencil = *in->pStencilAttachment;
          UnwrapInPlace(stencil->imageView);
          UnwrapInPlace(stencil->resolveImageView);
          UnwrapNextChain(state, "VkRenderingAttachmentInfo", tempMem, (VkBaseInStructure *)stencil);
        }
        else
        {
          out->pStencilAttachment = NULL;
        }

        break;
      }
      case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
      {
        const VkRenderPassAttachmentBeginInfo *in =
            (const VkRenderPassAttachmentBeginInfo *)nextInput;
        VkRenderPassAttachmentBeginInfo *out = (VkRenderPassAttachmentBeginInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkImageView *outAttachments = (VkImageView *)tempMem;
        tempMem += sizeof(VkImageView) * in->attachmentCount;

        *out = *in;

        out->pAttachments = outAttachments;
        for(uint32_t i = 0; i < in->attachmentCount; i++)
          outAttachments[i] = Unwrap(in->pAttachments[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2:
      {
        const VkResolveImageInfo2 *in = (const VkResolveImageInfo2 *)nextInput;
        VkResolveImageInfo2 *out = (VkResolveImageInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkImageResolve2 *outRegions = (VkImageResolve2 *)tempMem;
        tempMem += sizeof(VkImageResolve2) * in->regionCount;

        *out = *in;
        UnwrapInPlace(out->srcImage);
        UnwrapInPlace(out->dstImage);

        out->pRegions = outRegions;
        for(uint32_t i = 0; i < in->regionCount; i++)
        {
          outRegions[i] = in->pRegions[i];
          UnwrapNextChain(state, "VkImageResolve2", tempMem, (VkBaseInStructure *)&outRegions[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO:
      {
        const VkSemaphoreWaitInfo *in = (const VkSemaphoreWaitInfo *)nextInput;
        VkSemaphoreWaitInfo *out = (VkSemaphoreWaitInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkSemaphore *outSemaphores = (VkSemaphore *)tempMem;
        tempMem += sizeof(VkSemaphore) * in->semaphoreCount;

        *out = *in;
        out->pSemaphores = outSemaphores;

        for(uint32_t i = 0; i < in->semaphoreCount; i++)
          outSemaphores[i] = Unwrap(in->pSemaphores[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT:
      {
        const VkShaderCreateInfoEXT *in = (const VkShaderCreateInfoEXT *)nextInput;
        VkShaderCreateInfoEXT *out = (VkShaderCreateInfoEXT *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkDescriptorSetLayout *outLayouts = (VkDescriptorSetLayout *)tempMem;
        tempMem += sizeof(VkDescriptorSetLayout) * in->setLayoutCount;

        *out = *in;

        out->pSetLayouts = outLayouts;
        for(uint32_t i = 0; i < in->setLayoutCount; i++)
          outLayouts[i] = Unwrap(in->pSetLayouts[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      {
        const VkSubmitInfo *in = (const VkSubmitInfo *)nextInput;
        VkSubmitInfo *out = (VkSubmitInfo *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped arrays
        VkSemaphore *outWaitSemaphores = (VkSemaphore *)tempMem;
        tempMem += sizeof(VkSemaphore) * in->waitSemaphoreCount;
        VkCommandBuffer *outCmdBuffers = (VkCommandBuffer *)tempMem;
        tempMem += sizeof(VkCommandBuffer) * in->commandBufferCount;
        VkSemaphore *outSignalSemaphores = (VkSemaphore *)tempMem;
        tempMem += sizeof(VkSemaphore) * in->signalSemaphoreCount;

        *out = *in;
        out->pWaitSemaphores = outWaitSemaphores;
        out->pCommandBuffers = outCmdBuffers;
        out->pSignalSemaphores = outSignalSemaphores;

        for(uint32_t i = 0; i < in->waitSemaphoreCount; i++)
          outWaitSemaphores[i] = Unwrap(in->pWaitSemaphores[i]);
        for(uint32_t i = 0; i < in->commandBufferCount; i++)
          outCmdBuffers[i] = Unwrap(in->pCommandBuffers[i]);
        for(uint32_t i = 0; i < in->signalSemaphoreCount; i++)
          outSignalSemaphores[i] = Unwrap(in->pSignalSemaphores[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
      {
        const VkSubmitInfo2 *in = (const VkSubmitInfo2 *)nextInput;
        VkSubmitInfo2 *out = (VkSubmitInfo2 *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped arrays
        VkSemaphoreSubmitInfo *outWaitSemaphores = (VkSemaphoreSubmitInfo *)tempMem;
        tempMem += sizeof(VkSemaphoreSubmitInfo) * in->waitSemaphoreInfoCount;
        VkCommandBufferSubmitInfo *outCmdBuffers = (VkCommandBufferSubmitInfo *)tempMem;
        tempMem += sizeof(VkCommandBufferSubmitInfo) * in->commandBufferInfoCount;
        VkSemaphoreSubmitInfo *outSignalSemaphores = (VkSemaphoreSubmitInfo *)tempMem;
        tempMem += sizeof(VkSemaphoreSubmitInfo) * in->signalSemaphoreInfoCount;

        *out = *in;
        out->pWaitSemaphoreInfos = outWaitSemaphores;
        out->pCommandBufferInfos = outCmdBuffers;
        out->pSignalSemaphoreInfos = outSignalSemaphores;

        for(uint32_t i = 0; i < in->waitSemaphoreInfoCount; i++)
        {
          outWaitSemaphores[i] = in->pWaitSemaphoreInfos[i];
          UnwrapInPlace(outWaitSemaphores[i].semaphore);
          UnwrapNextChain(state, "VkSemaphoreSubmitInfo", tempMem,
                          (VkBaseInStructure *)&outWaitSemaphores[i]);
        }
        for(uint32_t i = 0; i < in->commandBufferInfoCount; i++)
        {
          outCmdBuffers[i] = in->pCommandBufferInfos[i];
          UnwrapInPlace(outCmdBuffers[i].commandBuffer);
          UnwrapNextChain(state, "VkCommandBufferSubmitInfo", tempMem,
                          (VkBaseInStructure *)&outCmdBuffers[i]);
        }
        for(uint32_t i = 0; i < in->signalSemaphoreInfoCount; i++)
        {
          outSignalSemaphores[i] = in->pSignalSemaphoreInfos[i];
          UnwrapInPlace(outSignalSemaphores[i].semaphore);
          UnwrapNextChain(state, "VkSemaphoreSubmitInfo", tempMem,
                          (VkBaseInStructure *)&outSignalSemaphores[i]);
        }

        break;
      }
      case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT:
      {
        const VkSwapchainPresentFenceInfoEXT *in = (const VkSwapchainPresentFenceInfoEXT *)nextInput;
        VkSwapchainPresentFenceInfoEXT *out = (VkSwapchainPresentFenceInfoEXT *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkFence *outFences = (VkFence *)tempMem;
        tempMem += sizeof(VkFence) * in->swapchainCount;

        *out = *in;
        out->pFences = outFences;

        for(uint32_t i = 0; i < in->swapchainCount; i++)
          outFences[i] = Unwrap(in->pFences[i]);

        break;
      }
      case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
      {
        const VkWriteDescriptorSet *in = (const VkWriteDescriptorSet *)nextInput;
        VkWriteDescriptorSet *out = (VkWriteDescriptorSet *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        *out = *in;
        UnwrapInPlace(out->dstSet);

        switch(out->descriptorType)
        {
          case VK_DESCRIPTOR_TYPE_SAMPLER:
          case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
          {
            VkDescriptorImageInfo *outBindings = (VkDescriptorImageInfo *)tempMem;
            tempMem += sizeof(VkDescriptorImageInfo) * in->descriptorCount;

            for(uint32_t d = 0; d < in->descriptorCount; d++)
            {
              outBindings[d] = in->pImageInfo[d];
              UnwrapInPlace(outBindings[d].imageView);
              UnwrapInPlace(outBindings[d].sampler);
            }

            break;
          }
          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          {
            VkBufferView *outBindings = (VkBufferView *)tempMem;
            tempMem += sizeof(VkBufferView) * in->descriptorCount;

            for(uint32_t d = 0; d < in->descriptorCount; d++)
              outBindings[d] = Unwrap(in->pTexelBufferView[d]);

            break;
          }
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
          {
            VkDescriptorBufferInfo *outBindings = (VkDescriptorBufferInfo *)tempMem;
            tempMem += sizeof(VkDescriptorBufferInfo) * in->descriptorCount;

            for(uint32_t d = 0; d < in->descriptorCount; d++)
            {
              outBindings[d] = in->pBufferInfo[d];
              UnwrapInPlace(outBindings[d].buffer);
            }

            break;
          }
          case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
          case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
          {
            // nothing to do/patch
            break;
          }
          default: RDCERR("Unhandled descriptor type unwrapping VkWriteDescriptorSet"); break;
        }

        break;
      }
      case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR:
      {
        const VkWriteDescriptorSetAccelerationStructureKHR *in =
            (const VkWriteDescriptorSetAccelerationStructureKHR *)nextInput;
        VkWriteDescriptorSetAccelerationStructureKHR *out =
            (VkWriteDescriptorSetAccelerationStructureKHR *)tempMem;

        // append immediately so tempMem is incremented
        AppendModifiedChainedStruct(tempMem, out, nextChainTail);

        // allocate unwrapped array
        VkAccelerationStructureKHR *outAS = (VkAccelerationStructureKHR *)tempMem;
        tempMem += sizeof(VkAccelerationStructureKHR) * in->accelerationStructureCount;

        *out = *in;
        out->pAccelerationStructures = outAS;

        for(uint32_t i = 0; i < in->accelerationStructureCount; i++)
          outAS[i] = Unwrap(in->pAccelerationStructures[i]);

        break;
      }

// Android External Buffer Memory Extension
#if ENABLED(RDOC_ANDROID)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                                 VkImportAndroidHardwareBufferInfoANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID,
                                 VkAndroidHardwareBufferUsageANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, VkExternalFormatANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
                                 VkAndroidHardwareBufferFormatPropertiesANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
                                 VkAndroidHardwareBufferPropertiesANDROID);
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                                   VkMemoryGetAndroidHardwareBufferInfoANDROID,
                                   UnwrapInPlace(out->memory));
#else
      case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID:
      case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID:
      case VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID:
      {
        RDCERR("Support for android external memory buffer extension not compiled in");
        break;
      }
#endif

#if ENABLED(RDOC_GGP)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP, VkPresentFrameTokenGGP);
#else
      case VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP:
      {
        RDCERR("Support for GGP frame token extension not compiled in");
        break;
      }
#endif

// NV win32 external memory extensions
#if ENABLED(RDOC_WIN32)
        // Structs that can be copied into place
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV,
                                 VkImportMemoryWin32HandleInfoNV);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV,
                                 VkExportMemoryWin32HandleInfoNV);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                 VkImportMemoryWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                 VkExportMemoryWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                                 VkMemoryWin32HandlePropertiesKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
                                 VkExportSemaphoreWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR,
                                 VkD3D12FenceSubmitInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR,
                                 VkExportFenceWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,
                                 VkSurfaceFullScreenExclusiveWin32InfoEXT);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT,
                                 VkSurfaceCapabilitiesFullScreenExclusiveEXT);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,
                                 VkSurfaceFullScreenExclusiveInfoEXT);

        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                                   VkMemoryGetWin32HandleInfoKHR, UnwrapInPlace(out->memory));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
                                   VkImportSemaphoreWin32HandleInfoKHR,
                                   UnwrapInPlace(out->semaphore));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
                                   VkSemaphoreGetWin32HandleInfoKHR, UnwrapInPlace(out->semaphore));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR,
                                   VkImportFenceWin32HandleInfoKHR, UnwrapInPlace(out->fence));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
                                   VkFenceGetWin32HandleInfoKHR, UnwrapInPlace(out->fence));

      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
      {
        // strip during replay
        if(IsCaptureMode(state))
        {
          // KHR and NV structs are identical
          const VkWin32KeyedMutexAcquireReleaseInfoKHR *in =
              (const VkWin32KeyedMutexAcquireReleaseInfoKHR *)nextInput;
          VkWin32KeyedMutexAcquireReleaseInfoKHR *out =
              (VkWin32KeyedMutexAcquireReleaseInfoKHR *)tempMem;

          // append immediately so tempMem is incremented
          AppendModifiedChainedStruct(tempMem, out, nextChainTail);

          // copy struct across
          *out = *in;

          // allocate unwrapped arrays
          VkDeviceMemory *unwrappedAcquires = (VkDeviceMemory *)tempMem;
          tempMem += sizeof(VkDeviceMemory) * in->acquireCount;
          VkDeviceMemory *unwrappedReleases = (VkDeviceMemory *)tempMem;
          tempMem += sizeof(VkDeviceMemory) * in->releaseCount;

          // unwrap the arrays
          for(uint32_t mem = 0; mem < in->acquireCount; mem++)
            unwrappedAcquires[mem] = Unwrap(in->pAcquireSyncs[mem]);
          for(uint32_t mem = 0; mem < in->releaseCount; mem++)
            unwrappedReleases[mem] = Unwrap(in->pReleaseSyncs[mem]);

          out->pAcquireSyncs = unwrappedAcquires;
          out->pReleaseSyncs = unwrappedReleases;
        }
        break;
      }
#else
      case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR:
      case VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR:
      case VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT:
      case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT:
      case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT:
      case VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
      {
        RDCERR("Support for win32 external memory extensions not compiled in");
        nextChainTail->pNext = nextInput;
        break;
      }
#endif

      case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT:
      {
        // could be implemented but would need extra work or doesn't make sense right now
        RDCERR("Struct %s not handled in %s pNext chain", ToStr(nextInput->sType).c_str(),
               structName);
        nextChainTail->pNext = nextInput;
        break;
      }

        UNHANDLED_STRUCTS()
        {
          RDCERR("Unhandled struct %s in %s pNext chain", ToStr(nextInput->sType).c_str(),
                 structName);
          nextChainTail->pNext = nextInput;
          break;
        }

      case VK_STRUCTURE_TYPE_MAX_ENUM:
      {
        RDCERR("Invalid value %x in %s pNext chain", nextInput->sType, structName);
        nextChainTail->pNext = nextInput;
        break;
      }
    }

    nextInput = nextInput->pNext;
  }
}

void CopyNextChainForPatching(const char *structName, byte *&tempMem, VkBaseInStructure *infoStruct)
{
  VkBaseInStructure *nextChainTail = infoStruct;
  const VkBaseInStructure *nextInput = (const VkBaseInStructure *)infoStruct->pNext;

  // simplified version of UnwrapNextChain which just copies everything. Useful for when we need to
  // shallow duplicate a next chain (e.g. because we'll copy and patch one struct)

#undef COPY_STRUCT_CAPTURE_ONLY
#define COPY_STRUCT_CAPTURE_ONLY(StructType, StructName)                          \
  case StructType:                                                                \
    CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;

#undef COPY_STRUCT
#define COPY_STRUCT(StructType, StructName)                                       \
  case StructType:                                                                \
    CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;

#undef UNWRAP_STRUCT_INNER
#define UNWRAP_STRUCT_INNER(StructType, StructName, ...)                          \
  case StructType:                                                                \
    CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;

#undef UNWRAP_STRUCT
#define UNWRAP_STRUCT(StructType, StructName, ...)                                \
  case StructType:                                                                \
    CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;

#undef UNWRAP_STRUCT_CAPTURE_ONLY
#define UNWRAP_STRUCT_CAPTURE_ONLY(StructType, StructName, ...)                   \
  case StructType:                                                                \
    CopyNextChainedStruct(sizeof(StructName), tempMem, nextInput, nextChainTail); \
    break;

  nextChainTail->pNext = NULL;
  while(nextInput)
  {
    switch(nextInput->sType)
    {
      PROCESS_SIMPLE_STRUCTS();

      // complex structs to handle - require multiple allocations
      case VK_STRUCTURE_TYPE_BIND_SPARSE_INFO:
        CopyNextChainedStruct(sizeof(VkBindSparseInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2:
        CopyNextChainedStruct(sizeof(VkBlitImageInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO:
        CopyNextChainedStruct(sizeof(VkCommandBufferInheritanceRenderingInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkComputePipelineCreateInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2:
        CopyNextChainedStruct(sizeof(VkCopyBufferInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2:
        CopyNextChainedStruct(sizeof(VkCopyBufferToImageInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2:
        CopyNextChainedStruct(sizeof(VkCopyImageToBufferInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2:
        CopyNextChainedStruct(sizeof(VkCopyImageInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_DEPENDENCY_INFO:
        CopyNextChainedStruct(sizeof(VkDependencyInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO:
        CopyNextChainedStruct(sizeof(VkDescriptorSetAllocateInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkDescriptorSetLayoutCreateInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS:
        CopyNextChainedStruct(sizeof(VkDeviceBufferMemoryRequirements), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkDeviceGroupDeviceCreateInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS:
        CopyNextChainedStruct(sizeof(VkDeviceImageMemoryRequirements), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkFramebufferCreateInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkFramebufferAttachmentsCreateInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO:
        CopyNextChainedStruct(sizeof(VkFramebufferAttachmentImageInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkGraphicsPipelineCreateInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkPipelineLayoutCreateInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR:
        CopyNextChainedStruct(sizeof(VkPipelineLibraryCreateInfoKHR), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO:
        CopyNextChainedStruct(sizeof(VkPipelineRenderingCreateInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_PRESENT_INFO_KHR:
        CopyNextChainedStruct(sizeof(VkPresentInfoKHR), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR:
        CopyNextChainedStruct(sizeof(VkRayTracingPipelineCreateInfoKHR), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_RENDERING_INFO:
        CopyNextChainedStruct(sizeof(VkRenderingInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
        CopyNextChainedStruct(sizeof(VkRenderPassAttachmentBeginInfo), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2:
        CopyNextChainedStruct(sizeof(VkResolveImageInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO:
        CopyNextChainedStruct(sizeof(VkSemaphoreWaitInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT:
        CopyNextChainedStruct(sizeof(VkShaderCreateInfoEXT), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_SUBMIT_INFO:
        CopyNextChainedStruct(sizeof(VkSubmitInfo), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_SUBMIT_INFO_2:
        CopyNextChainedStruct(sizeof(VkSubmitInfo2), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT:
        CopyNextChainedStruct(sizeof(VkSwapchainPresentFenceInfoEXT), tempMem, nextInput,
                              nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET:
        CopyNextChainedStruct(sizeof(VkWriteDescriptorSet), tempMem, nextInput, nextChainTail);
        break;
      case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR:
        CopyNextChainedStruct(sizeof(VkWriteDescriptorSetAccelerationStructureKHR), tempMem,
                              nextInput, nextChainTail);
        break;

// Android External Buffer Memory Extension
#if ENABLED(RDOC_ANDROID)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                                 VkImportAndroidHardwareBufferInfoANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID,
                                 VkAndroidHardwareBufferUsageANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID, VkExternalFormatANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
                                 VkAndroidHardwareBufferFormatPropertiesANDROID);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
                                 VkAndroidHardwareBufferPropertiesANDROID);
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                                   VkMemoryGetAndroidHardwareBufferInfoANDROID,
                                   UnwrapInPlace(out->memory));
#else
      case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID:
      case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID:
      case VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:
      case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID:
      {
        RDCERR("Support for android external memory buffer extension not compiled in");
        break;
      }
#endif

#if ENABLED(RDOC_GGP)
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP, VkPresentFrameTokenGGP);
#else
      case VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP:
      {
        RDCERR("Support for GGP frame token extension not compiled in");
        break;
      }
#endif

// NV win32 external memory extensions
#if ENABLED(RDOC_WIN32)
        // Structs that can be copied into place
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV,
                                 VkImportMemoryWin32HandleInfoNV);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV,
                                 VkExportMemoryWin32HandleInfoNV);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                 VkImportMemoryWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                 VkExportMemoryWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                                 VkMemoryWin32HandlePropertiesKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
                                 VkExportSemaphoreWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR,
                                 VkD3D12FenceSubmitInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR,
                                 VkExportFenceWin32HandleInfoKHR);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,
                                 VkSurfaceFullScreenExclusiveWin32InfoEXT);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT,
                                 VkSurfaceCapabilitiesFullScreenExclusiveEXT);
        COPY_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,
                                 VkSurfaceFullScreenExclusiveInfoEXT);

        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                                   VkMemoryGetWin32HandleInfoKHR, UnwrapInPlace(out->memory));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
                                   VkImportSemaphoreWin32HandleInfoKHR,
                                   UnwrapInPlace(out->semaphore));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
                                   VkSemaphoreGetWin32HandleInfoKHR, UnwrapInPlace(out->semaphore));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR,
                                   VkImportFenceWin32HandleInfoKHR, UnwrapInPlace(out->fence));
        UNWRAP_STRUCT_CAPTURE_ONLY(VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR,
                                   VkFenceGetWin32HandleInfoKHR, UnwrapInPlace(out->fence));

      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
        CopyNextChainedStruct(sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR), tempMem, nextInput,
                              nextChainTail);
        break;
#else
      case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV:
      case VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR:
      case VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR:
      case VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT:
      case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT:
      case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT:
      case VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV:
      case VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR:
      {
        RDCERR("Support for win32 external memory extensions not compiled in");
        nextChainTail->pNext = nextInput;
        break;
      }
#endif

      case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT:
      case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT:
      {
        // could be implemented but would need extra work or doesn't make sense right now
        RDCERR("Struct %s not handled in %s pNext chain", ToStr(nextInput->sType).c_str(),
               structName);
        nextChainTail->pNext = nextInput;
        break;
      }

        UNHANDLED_STRUCTS()
        {
          RDCERR("Unhandled struct %s in %s pNext chain", ToStr(nextInput->sType).c_str(),
                 structName);
          nextChainTail->pNext = nextInput;
          break;
        }

      case VK_STRUCTURE_TYPE_MAX_ENUM:
      {
        RDCERR("Invalid value %x in %s pNext chain", nextInput->sType, structName);
        nextChainTail->pNext = nextInput;
        break;
      }
    }

    nextInput = nextInput->pNext;
  }
}
