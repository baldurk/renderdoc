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
  COPY_STRUCT(VK_STRUCTURE_TYPE_APPLICATION_INFO, VkApplicationInfo);                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR, VkAttachmentDescription2KHR);          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, VkAttachmentReference2KHR);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO,                                \
              VkBindBufferMemoryDeviceGroupInfo);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO,                                 \
              VkBindImageMemoryDeviceGroupInfo);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, VkBindImagePlaneMemoryInfo);           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, VkBufferCreateInfo);                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,                               \
              VkBufferDeviceAddressCreateInfoEXT);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, VkCalibratedTimestampInfoEXT);        \
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
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,                 \
              VkDescriptorSetLayoutBindingFlagsCreateInfoEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT, VkDescriptorSetLayoutSupport);        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,          \
              VkDescriptorSetVariableDescriptorCountAllocateInfoEXT);                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT,         \
              VkDescriptorSetVariableDescriptorCountLayoutSupportEXT);                               \
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
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, VkDeviceQueueCreateInfo);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,                        \
              VkDeviceQueueGlobalPriorityCreateInfoEXT);                                             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2, VkDeviceQueueInfo2);                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR, VkDisplayModeProperties2KHR);         \
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
  COPY_STRUCT(VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, VkFormatProperties2);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, VkImageCreateInfo);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR, VkImageFormatListCreateInfoKHR);  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, VkImageFormatProperties2);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,                                \
              VkImagePlaneMemoryRequirementsInfo);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT,                                 \
              VkImageStencilUsageCreateInfoEXT);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT, VkImageViewASTCDecodeModeEXT);      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, VkImageViewUsageCreateInfo);           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, VkInstanceCreateInfo);                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, VkMemoryAllocateFlagsInfo);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, VkMemoryAllocateInfo);                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_BARRIER, VkMemoryBarrier);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, VkMemoryDedicatedRequirements);       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT, VkMemoryPriorityAllocateInfoEXT); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, VkMemoryRequirements2);                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT, VkMultisamplePropertiesEXT);             \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,                              \
              VkPhysicalDevice16BitStorageFeatures);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,                           \
              VkPhysicalDevice8BitStorageFeaturesKHR);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT,                            \
              VkPhysicalDeviceASTCDecodeFeaturesEXT);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT,                  \
              VkPhysicalDeviceBufferDeviceAddressFeaturesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,                  \
              VkPhysicalDeviceConditionalRenderingFeaturesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT,           \
              VkPhysicalDeviceConservativeRasterizationPropertiesEXT);                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV,     \
              VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,                      \
              VkPhysicalDeviceDepthClipEnableFeaturesEXT)                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR,                \
              VkPhysicalDeviceDepthStencilResolvePropertiesKHR);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,                    \
              VkPhysicalDeviceDescriptorIndexingFeaturesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,                  \
              VkPhysicalDeviceDescriptorIndexingPropertiesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT,                    \
              VkPhysicalDeviceDiscardRectanglePropertiesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,                               \
              VkPhysicalDeviceDriverPropertiesKHR);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,                                \
              VkPhysicalDeviceExternalBufferInfo);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,                          \
              VkPhysicalDeviceExternalImageFormatInfo);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,                             \
              VkPhysicalDeviceExternalSemaphoreInfo);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, VkPhysicalDeviceFeatures2);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR,                       \
              VkPhysicalDeviceFloatControlsPropertiesKHR);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,                           \
              VkPhysicalDeviceFloat16Int8FeaturesKHR);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,                   \
              VkPhysicalDeviceFragmentDensityMapFeaturesEXT);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT,                 \
              VkPhysicalDeviceFragmentDensityMapPropertiesEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT,                       \
              VkPhysicalDeviceHostQueryResetFeaturesEXT);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES, VkPhysicalDeviceIDProperties);        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,                                 \
              VkPhysicalDeviceImageFormatInfo2);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT,                    \
              VkPhysicalDeviceImageViewImageFormatInfoEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,                            \
              VkPhysicalDeviceMaintenance3Properties);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,                        \
              VkPhysicalDeviceMemoryBudgetPropertiesEXT);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,                                 \
              VkPhysicalDeviceMemoryProperties2);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,                        \
              VkPhysicalDeviceMemoryPriorityFeaturesEXT);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,                                  \
              VkPhysicalDeviceMultiviewFeatures);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES,                                \
              VkPhysicalDeviceMultiviewProperties);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT,                         \
              VkPhysicalDevicePCIBusInfoPropertiesEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES,                           \
              VkPhysicalDevicePointClippingProperties);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, VkPhysicalDeviceProperties2);          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,                           \
              VkPhysicalDeviceProtectedMemoryFeatures);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES,                         \
              VkPhysicalDeviceProtectedMemoryProperties);                                            \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR,                      \
              VkPhysicalDevicePushDescriptorPropertiesKHR);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT,                     \
              VkPhysicalDeviceSampleLocationsPropertiesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,                \
              VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT);                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,                   \
              VkPhysicalDeviceSamplerYcbcrConversionFeatures);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,                    \
              VkPhysicalDeviceScalarBlockLayoutFeaturesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR,                    \
              VkPhysicalDeviceShaderAtomicInt64FeaturesKHR);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD,                          \
              VkPhysicalDeviceShaderCorePropertiesAMD);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,                     \
              VkPhysicalDeviceShaderDrawParametersFeatures);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2,                          \
              VkPhysicalDeviceSparseImageFormatInfo2);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV,                  \
              VkPhysicalDeviceShaderImageFootprintFeaturesNV);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,                                 \
              VkPhysicalDeviceSubgroupProperties);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,                     \
              VkPhysicalDeviceTransformFeedbackFeaturesEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT,                   \
              VkPhysicalDeviceTransformFeedbackPropertiesEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES,                          \
              VkPhysicalDeviceVariablePointerFeatures);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT,               \
              VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT,             \
              VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR,                    \
              VkPhysicalDeviceVulkanMemoryModelFeaturesKHR);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT,                     \
              VkPhysicalDeviceYcbcrImageArraysFeaturesEXT)                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, VkPipelineCacheCreateInfo);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT,                          \
              VkPipelineCreationFeedbackCreateInfoEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,                              \
              VkPipelineColorBlendStateCreateInfo);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,             \
              VkPipelineRasterizationDepthClipStateCreateInfoEXT);                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,                            \
              VkPipelineDepthStencilStateCreateInfo);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT,                    \
              VkPipelineDiscardRectangleStateCreateInfoEXT);                                         \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,                                  \
              VkPipelineDynamicStateCreateInfo);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,                           \
              VkPipelineInputAssemblyStateCreateInfo);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,                              \
              VkPipelineMultisampleStateCreateInfo);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,           \
              VkPipelineRasterizationConservativeStateCreateInfoEXT);                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,                            \
              VkPipelineRasterizationStateCreateInfo);                                               \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,                 \
              VkPipelineRasterizationStateStreamCreateInfoEXT);                                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,                     \
              VkPipelineSampleLocationsStateCreateInfoEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VkPipelineShaderStageCreateInfo); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,               \
              VkPipelineTessellationDomainOriginStateCreateInfo);                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,                             \
              VkPipelineTessellationStateCreateInfo);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,                 \
              VkPipelineVertexInputDivisorStateCreateInfoEXT);                                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,                             \
              VkPipelineVertexInputStateCreateInfo);                                                 \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,                                 \
              VkPipelineViewportStateCreateInfo);                                                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR, VkPresentRegionsKHR);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, VkQueryPoolCreateInfo);                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, VkQueueFamilyProperties2);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, VkRenderPassCreateInfo);                    \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR, VkRenderPassCreateInfo2KHR);          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,                    \
              VkRenderPassFragmentDensityMapCreateInfoEXT);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO,                     \
              VkRenderPassInputAttachmentAspectCreateInfo);                                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, VkRenderPassMultiviewCreateInfo); \
  COPY_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,                         \
              VkRenderPassSampleLocationsBeginInfoEXT);                                              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, VkSampleLocationsInfoEXT);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, VkSamplerCreateInfo);                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,                              \
              VkSamplerReductionModeCreateInfoEXT);                                                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,                                \
              VkSamplerYcbcrConversionCreateInfo);                                                   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES,                    \
              VkSamplerYcbcrConversionImageFormatProperties);                                        \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, VkSemaphoreCreateInfo);                       \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, VkShaderModuleCreateInfo);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR,                             \
              VkSharedPresentSurfaceCapabilitiesKHR);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2, VkSparseImageFormatProperties2);   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2,                                  \
              VkSparseImageMemoryRequirements2);                                                     \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR, VkSubpassBeginInfoKHR);                      \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR, VkSubpassDependency2KHR);                  \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR, VkSubpassDescription2KHR);                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR,                       \
              VkSubpassDescriptionDepthStencilResolveKHR);                                           \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_END_INFO_KHR, VkSubpassEndInfoKHR);                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT, VkSurfaceCapabilities2EXT);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, VkSurfaceCapabilities2KHR);              \
  COPY_STRUCT(VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, VkSurfaceFormat2KHR);                          \
  COPY_STRUCT(VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD,                            \
              VkTextureLODGatherFormatPropertiesAMD);                                                \
  COPY_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT, VkValidationCacheCreateInfoEXT);   \
  COPY_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, VkValidationFeaturesEXT);                   \
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
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO, VkBindBufferMemoryInfo,                   \
                UnwrapInPlace(out->buffer), UnwrapInPlace(out->memory));                             \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, VkBindImageMemoryInfo,                     \
                UnwrapInPlace(out->image), UnwrapInPlace(out->memory));                              \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, VkBufferMemoryBarrier,                      \
                UnwrapInPlace(out->buffer));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT, VkBufferDeviceAddressInfoEXT,      \
                UnwrapInPlace(out->buffer));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,                                 \
                VkBufferMemoryRequirementsInfo2, UnwrapInPlace(out->buffer));                        \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, VkBufferViewCreateInfo,                   \
                UnwrapInPlace(out->buffer));                                                         \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, VkCommandBufferAllocateInfo,         \
                UnwrapInPlace(out->commandPool));                                                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, VkCommandBufferInheritanceInfo,   \
                UnwrapInPlace(out->renderPass), UnwrapInPlace(out->framebuffer));                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, VkCopyDescriptorSet,                          \
                UnwrapInPlace(out->srcSet), UnwrapInPlace(out->dstSet));                             \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV,                      \
                VkDedicatedAllocationMemoryAllocateInfoNV, UnwrapInPlace(out->buffer),               \
                UnwrapInPlace(out->image));                                                          \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,                            \
                VkDescriptorUpdateTemplateCreateInfo, UnwrapInPlace(out->descriptorSetLayout),       \
                UnwrapInPlace(out->pipelineLayout));                                                 \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, VkImageMemoryBarrier,                        \
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
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, VkRenderPassBeginInfo,                     \
                UnwrapInPlace(out->renderPass), UnwrapInPlace(out->framebuffer));                    \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, VkSamplerYcbcrConversionInfo,       \
                UnwrapInPlace(out->conversion));                                                     \
  UNWRAP_STRUCT(VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,                              \
                VkConditionalRenderingBeginInfoEXT, UnwrapInPlace(out->buffer));                     \
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
#define UNHANDLED_STRUCTS()                                                            \
  /* Surface creation structs would pull in dependencies on OS-specific includes, */   \
  /* so we treat them as unsupported. */                                               \
  case VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR:                              \
  case VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR:                                 \
  case VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR:                              \
  case VK_STRUCTURE_TYPE_IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA:                        \
  case VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK:                                  \
  case VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK:                                \
  case VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT:                                \
  case VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP:                                      \
  case VK_STRUCTURE_TYPE_STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP:                    \
  case VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN:                                    \
  case VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR:                              \
  case VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR:                                \
  case VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR:                                  \
  case VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR:                                 \
  /* Output structure containing objects. Must be *wrapped* not unwrapped. */          \
  /* So we treat this as unhandled in generic code and require specific handling. */   \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV:                        \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV:                               \
  case VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV:           \
  case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID:            \
  case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID:                   \
  case VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID:                        \
  case VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV:                   \
  case VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV:                                           \
  case VK_STRUCTURE_TYPE_CMD_PROCESS_COMMANDS_INFO_NVX:                                \
  case VK_STRUCTURE_TYPE_CMD_RESERVE_SPACE_FOR_COMMANDS_INFO_NVX:                      \
  case VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV:                             \
  case VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT:         \
  case VK_STRUCTURE_TYPE_DEVICE_GENERATED_COMMANDS_FEATURES_NVX:                       \
  case VK_STRUCTURE_TYPE_DEVICE_GENERATED_COMMANDS_LIMITS_NVX:                         \
  case VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD:                 \
  case VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD:                  \
  case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT:                           \
  case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:                      \
  case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID:                                      \
  case VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV:                                             \
  case VK_STRUCTURE_TYPE_GEOMETRY_NV:                                                  \
  case VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV:                                        \
  case VK_STRUCTURE_TYPE_HDR_METADATA_EXT:                                             \
  case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:           \
  case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:               \
  case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT:                     \
  case VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX:                                   \
  case VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:                  \
  case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:                          \
  case VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NVX:                     \
  case VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID:              \
  case VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT:                           \
  case VK_STRUCTURE_TYPE_OBJECT_TABLE_CREATE_INFO_NVX:                                 \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT:        \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT:      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV:       \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV:             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV:                \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT:          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV:      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES:                             \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT:           \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT:            \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT:          \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV:                      \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX: \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV:                    \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV:     \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV:               \
  case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV:             \
  case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:          \
  case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV:            \
  case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV:              \
  case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:         \
  case VK_STRUCTURE_TYPE_PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV:   \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV:   \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV:     \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV:    \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV:               \
  case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV:             \
  case VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE:                                    \
  case VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV:                        \
  case VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV:                          \
  case VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV:                      \
  case VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT:               \
  case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT:                       \
  case VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT:                 \
  case VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR:                           \
  case VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD:                 \
  case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV:               \
  case VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT:

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
      case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
      {
        memSize += sizeof(VkDeviceGroupDeviceCreateInfo);

        VkDeviceGroupDeviceCreateInfo *info = (VkDeviceGroupDeviceCreateInfo *)next;
        memSize += info->physicalDeviceCount * sizeof(VkPhysicalDevice);
        break;
      }
      case VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO:
      {
        memSize += sizeof(VkFramebufferCreateInfo);

        VkFramebufferCreateInfo *info = (VkFramebufferCreateInfo *)next;
        memSize += info->attachmentCount * sizeof(VkImageView);
        break;
      }
      case VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO:
      {
        memSize += sizeof(VkGraphicsPipelineCreateInfo);

        VkGraphicsPipelineCreateInfo *info = (VkGraphicsPipelineCreateInfo *)next;
        memSize += info->stageCount * sizeof(VkPipelineShaderStageCreateInfo);
        break;
      }
      case VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO:
      {
        memSize += sizeof(VkComputePipelineCreateInfo);
        break;
      }
      case VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO:
      {
        memSize += sizeof(VkPipelineLayoutCreateInfo);

        VkPipelineLayoutCreateInfo *info = (VkPipelineLayoutCreateInfo *)next;
        memSize += info->setLayoutCount * sizeof(VkDescriptorSetLayout);
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
      case VK_STRUCTURE_TYPE_SUBMIT_INFO:
      {
        memSize += sizeof(VkSubmitInfo);

        VkSubmitInfo *info = (VkSubmitInfo *)next;
        memSize += info->waitSemaphoreCount * sizeof(VkSemaphore);
        memSize += info->commandBufferCount * sizeof(VkCommandBuffer);
        memSize += info->signalSemaphoreCount * sizeof(VkSemaphore);
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
          default: RDCERR("Unhandled descriptor type unwrapping VkWriteDescriptorSet"); break;
        }
        break;
      }

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

      case VK_STRUCTURE_TYPE_RANGE_SIZE:
      case VK_STRUCTURE_TYPE_MAX_ENUM:
        RDCERR("Invalid value %x in pNext chain", next->sType);
        break;
    }

    next = next->pNext;
  }

  return memSize;
}

void UnwrapNextChain(CaptureState state, const char *structName, byte *&tempMem,
                     VkBaseInStructure *infoStruct)
{
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

        out->pAttachments = outAttachments;
        for(uint32_t i = 0; i < in->attachmentCount; i++)
          outAttachments[i] = Unwrap(in->pAttachments[i]);

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

        *out = *in;
        UnwrapInPlace(out->layout);
        UnwrapInPlace(out->renderPass);
        if(out->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
          UnwrapInPlace(out->basePipelineHandle);

        out->pStages = outShaders;
        for(uint32_t i = 0; i < in->stageCount; i++)
          outShaders[i].module = Unwrap(in->pStages[i].module);

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
          default: RDCERR("Unhandled descriptor type unwrapping VkWriteDescriptorSet"); break;
        }

        break;
      }

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

      case VK_STRUCTURE_TYPE_RANGE_SIZE:
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
