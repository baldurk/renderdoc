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
#include "vk_manager.h"
#include "vk_resources.h"

// there's an annoying little difference between VkFoo*Flags* and VkFoo*FlagBits* meaning we can't
// just append 'Bits' onto a flag type to get the bits enum. So we declare typedefs to do that for
// us so the macros are cleaner below.
// we could avoid this but we'd need to do the base + suffix thing in SERIALISE_MEMBER_VKFLAGS()
// which is uglier.

#define DECL_VKFLAG(flagbase) typedef CONCAT(flagbase, FlagBits) CONCAT(flagbase, FlagsBits);
#define DECL_VKFLAG_EXT(flagbase, suffix)            \
  typedef CONCAT(flagbase, CONCAT(FlagBits, suffix)) \
      CONCAT(flagbase, CONCAT(Flags, CONCAT(suffix, Bits)));

// duplicates for enums that don't yet have a FlagBits enum, which we direct to VkFlagWithNoBits
// we declare our own enum of the same name will then compile error if this macro is mistakenly
// used for a flags type with some bits (for example, if a bit is later added to a previously empty
// flags field).
#define DECL_VKFLAG_EMPTY(flagbase) \
  enum CONCAT(flagbase, FlagBits)   \
  {                                 \
  };                                \
  typedef VkFlagWithNoBits CONCAT(flagbase, FlagsBits);
#define DECL_VKFLAG_EMPTY_EXT(flagbase, suffix)   \
  enum CONCAT(flagbase, CONCAT(FlagBits, suffix)) \
  {                                               \
  };                                              \
  typedef VkFlagWithNoBits CONCAT(flagbase, CONCAT(Flags, CONCAT(suffix, Bits)));

DECL_VKFLAG(VkAccess);
DECL_VKFLAG(VkAttachmentDescription);
DECL_VKFLAG(VkBufferCreate);
DECL_VKFLAG(VkBufferUsage);
DECL_VKFLAG_EMPTY(VkBufferViewCreate);
DECL_VKFLAG(VkColorComponent);
DECL_VKFLAG(VkCommandBufferReset);
DECL_VKFLAG(VkCommandBufferUsage);
DECL_VKFLAG(VkCommandPoolCreate);
DECL_VKFLAG(VkCommandPoolReset);
DECL_VKFLAG_EMPTY(VkCommandPoolTrim);
DECL_VKFLAG(VkCullMode);
DECL_VKFLAG(VkDependency);
DECL_VKFLAG(VkDescriptorPoolCreate);
DECL_VKFLAG_EMPTY(VkDescriptorPoolReset);
DECL_VKFLAG(VkDescriptorSetLayoutCreate);
DECL_VKFLAG_EMPTY(VkDescriptorUpdateTemplateCreate);
DECL_VKFLAG_EMPTY(VkDeviceCreate);
DECL_VKFLAG(VkDeviceQueueCreate);
DECL_VKFLAG_EMPTY(VkEventCreate);
DECL_VKFLAG(VkExternalFenceHandleType);
DECL_VKFLAG(VkExternalFenceFeature);
DECL_VKFLAG(VkExternalMemoryHandleType);
DECL_VKFLAG(VkExternalMemoryFeature);
DECL_VKFLAG(VkExternalSemaphoreHandleType);
DECL_VKFLAG(VkExternalSemaphoreFeature);
DECL_VKFLAG(VkFormatFeature);
DECL_VKFLAG(VkImageAspect);
DECL_VKFLAG(VkImageUsage);
DECL_VKFLAG(VkImageCreate);
DECL_VKFLAG(VkImageViewCreate);
DECL_VKFLAG_EMPTY(VkInstanceCreate);
DECL_VKFLAG(VkFenceCreate);
DECL_VKFLAG(VkFenceImport);
DECL_VKFLAG_EMPTY(VkFramebufferCreate);
DECL_VKFLAG(VkMemoryAllocate);
DECL_VKFLAG(VkMemoryHeap);
DECL_VKFLAG_EMPTY(VkMemoryMap);
DECL_VKFLAG(VkMemoryProperty);
DECL_VKFLAG(VkPeerMemoryFeature);
DECL_VKFLAG_EMPTY(VkPipelineCacheCreate);
DECL_VKFLAG_EMPTY(VkPipelineColorBlendStateCreate);
DECL_VKFLAG(VkPipelineCreate);
DECL_VKFLAG_EMPTY(VkPipelineDepthStencilStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineDynamicStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineInputAssemblyStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineLayoutCreate);
DECL_VKFLAG_EMPTY(VkPipelineMultisampleStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineRasterizationStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineShaderStageCreate);
DECL_VKFLAG(VkPipelineStage);
DECL_VKFLAG_EMPTY(VkPipelineTessellationStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineVertexInputStateCreate);
DECL_VKFLAG_EMPTY(VkPipelineViewportStateCreate);
DECL_VKFLAG(VkQueryControl);
DECL_VKFLAG(VkQueryPipelineStatistic);
DECL_VKFLAG_EMPTY(VkQueryPoolCreate);
DECL_VKFLAG(VkQueryResult);
DECL_VKFLAG(VkQueue);
DECL_VKFLAG_EMPTY(VkRenderPassCreate);
DECL_VKFLAG(VkSamplerCreate);
DECL_VKFLAG(VkSampleCount);
DECL_VKFLAG_EMPTY(VkSemaphoreCreate);
DECL_VKFLAG(VkSemaphoreImport);
DECL_VKFLAG_EMPTY(VkShaderModuleCreate);
DECL_VKFLAG(VkShaderStage);
DECL_VKFLAG(VkSparseImageFormat);
DECL_VKFLAG(VkSparseMemoryBind);
DECL_VKFLAG(VkStencilFace);
DECL_VKFLAG(VkSubgroupFeature);
DECL_VKFLAG(VkSubpassDescription);
DECL_VKFLAG_EXT(VkBuildAccelerationStructure, NV);
DECL_VKFLAG_EXT(VkCompositeAlpha, KHR);
DECL_VKFLAG_EXT(VkConditionalRendering, EXT);
DECL_VKFLAG_EXT(VkDebugReport, EXT);
DECL_VKFLAG_EMPTY_EXT(VkDebugUtilsMessengerCallbackData, EXT);
DECL_VKFLAG_EMPTY_EXT(VkDebugUtilsMessengerCreate, EXT);
DECL_VKFLAG_EXT(VkDebugUtilsMessageSeverity, EXT);
DECL_VKFLAG_EXT(VkDebugUtilsMessageType, EXT);
DECL_VKFLAG_EXT(VkDescriptorBinding, EXT);
DECL_VKFLAG_EXT(VkDeviceGroupPresentMode, KHR);
DECL_VKFLAG_EMPTY_EXT(VkDisplayModeCreate, KHR);
DECL_VKFLAG_EXT(VkDisplayPlaneAlpha, KHR);
DECL_VKFLAG_EMPTY_EXT(VkDisplaySurfaceCreate, KHR);
DECL_VKFLAG_EXT(VkExternalMemoryHandleType, NV);
DECL_VKFLAG_EXT(VkExternalMemoryFeature, NV);
DECL_VKFLAG_EXT(VkGeometry, NV);
DECL_VKFLAG_EXT(VkGeometryInstance, NV);
DECL_VKFLAG_EXT(VkIndirectCommandsLayoutUsage, NVX);
DECL_VKFLAG_EXT(VkObjectEntryUsage, NVX);
DECL_VKFLAG_EMPTY_EXT(VkPipelineCoverageModulationStateCreate, NV);
DECL_VKFLAG_EMPTY_EXT(VkPipelineCoverageToColorStateCreate, NV);
DECL_VKFLAG_EMPTY_EXT(VkPipelineDiscardRectangleStateCreate, EXT);
DECL_VKFLAG_EMPTY_EXT(VkPipelineRasterizationConservativeStateCreate, EXT);
DECL_VKFLAG_EMPTY_EXT(VkPipelineRasterizationStateStreamCreate, EXT);
DECL_VKFLAG_EMPTY_EXT(VkPipelineViewportSwizzleStateCreate, NV);
DECL_VKFLAG_EXT(VkSurfaceCounter, EXT);
DECL_VKFLAG_EXT(VkSurfaceTransform, KHR);
DECL_VKFLAG_EXT(VkSwapchainCreate, KHR);
DECL_VKFLAG_EMPTY_EXT(VkValidationCacheCreate, EXT);
DECL_VKFLAG_EMPTY_EXT(VkPipelineRasterizationDepthClipStateCreate, EXT);
DECL_VKFLAG_EXT(VkDescriptorBinding, EXT);

// serialise a member as flags - cast to the Bits enum for serialisation so the stringification
// picks up the bitfield and doesn't treat it as uint32_t. Then we rename the type back to the base
// flags type so the structured data is as accurate as possible.
#define SERIALISE_MEMBER_VKFLAGS(flagstype, name)       \
  SERIALISE_MEMBER_TYPED(CONCAT(flagstype, Bits), name) \
      .TypedAs(STRING_LITERAL(STRINGIZE(flagstype)))

#define SERIALISE_MEMBER_ARRAY_VKFLAGS(flagstype, name, count)       \
  SERIALISE_MEMBER_ARRAY_TYPED(CONCAT(flagstype, Bits), name, count) \
      .TypedAs(STRING_LITERAL(STRINGIZE(flagstype)))

// simple way to express "resources referenced from this struct don't have to be present."
// since this is used during read when the processing is single-threaded, we make it a static
// flag.
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
               VkExportMemoryWin32HandleInfoKHR)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,                                  \
               VkMemoryWin32HandlePropertiesKHR)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR, VkMemoryGetWin32HandleInfoKHR)     \
                                                                                                      \
  /* VK_KHR_external_semaphore_win32 */                                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,                              \
               VkExportSemaphoreWin32HandleInfoKHR)                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,                              \
               VkImportSemaphoreWin32HandleInfoKHR)                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR, VkD3D12FenceSubmitInfoKHR)              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,                                 \
               VkSemaphoreGetWin32HandleInfoKHR)                                                      \
                                                                                                      \
  /* VK_KHR_external_fence_win32 */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR, VkExportFenceWin32HandleInfoKHR) \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR, VkImportFenceWin32HandleInfoKHR) \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR, VkFenceGetWin32HandleInfoKHR)       \
                                                                                                      \
  /* VK_EXT_full_screen_exclusive */                                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,                        \
               VkSurfaceFullScreenExclusiveWin32InfoEXT)                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT,                      \
               VkSurfaceCapabilitiesFullScreenExclusiveEXT)                                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,                              \
               VkSurfaceFullScreenExclusiveInfoEXT)

#else

#define HANDLE_PNEXT_OS()                                                             \
  /* VK_NV_external_memory_win32 */                                                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)             \
                                                                                      \
  /* VK_NV_win32_keyed_mutex */                                                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV)      \
                                                                                      \
  /* VK_KHR_win32_keyed_mutex */                                                      \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)     \
                                                                                      \
  /* VK_KHR_external_memory_win32 */                                                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)            \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)            \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR)             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR)               \
                                                                                      \
  /* VK_KHR_external_semaphore_win32 */                                               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR)         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR)                    \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR)            \
                                                                                      \
  /* VK_KHR_external_fence_win32 */                                                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR)             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR)             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR)                \
                                                                                      \
  /* VK_EXT_full_screen_exclusive */                                                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT)   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT) \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT)

#endif

// pNext structure type dispatch
#define HANDLE_PNEXT()                                                                                 \
  /* OS-specific extensions */                                                                         \
  HANDLE_PNEXT_OS()                                                                                    \
                                                                                                       \
  /* Core 1.0 structs. Should never be serialised in a pNext chain */                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_APPLICATION_INFO, VkApplicationInfo)                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, VkInstanceCreateInfo)                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, VkDeviceQueueCreateInfo)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, VkDeviceCreateInfo)                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SUBMIT_INFO, VkSubmitInfo)                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, VkMemoryAllocateInfo)                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, VkMappedMemoryRange)                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, VkBindSparseInfo)                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, VkFenceCreateInfo)                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, VkSemaphoreCreateInfo)                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, VkEventCreateInfo)                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, VkQueryPoolCreateInfo)                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, VkBufferCreateInfo)                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, VkBufferViewCreateInfo)                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, VkImageCreateInfo)                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, VkImageViewCreateInfo)                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, VkShaderModuleCreateInfo)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, VkPipelineCacheCreateInfo)                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, VkPipelineShaderStageCreateInfo)   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,                              \
               VkPipelineVertexInputStateCreateInfo)                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,                            \
               VkPipelineInputAssemblyStateCreateInfo)                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,                              \
               VkPipelineTessellationStateCreateInfo)                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,                                  \
               VkPipelineViewportStateCreateInfo)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,                             \
               VkPipelineRasterizationStateCreateInfo)                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,                               \
               VkPipelineMultisampleStateCreateInfo)                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,                             \
               VkPipelineDepthStencilStateCreateInfo)                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,                               \
               VkPipelineColorBlendStateCreateInfo)                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,                                   \
               VkPipelineDynamicStateCreateInfo)                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, VkGraphicsPipelineCreateInfo)          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, VkComputePipelineCreateInfo)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, VkPipelineLayoutCreateInfo)              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, VkSamplerCreateInfo)                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, VkDescriptorSetLayoutCreateInfo)   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, VkDescriptorPoolCreateInfo)              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, VkDescriptorSetAllocateInfo)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, VkWriteDescriptorSet)                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, VkCopyDescriptorSet)                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, VkFramebufferCreateInfo)                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, VkRenderPassCreateInfo)                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, VkCommandPoolCreateInfo)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, VkCommandBufferAllocateInfo)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, VkCommandBufferInheritanceInfo)      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, VkCommandBufferBeginInfo)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, VkRenderPassBeginInfo)                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, VkBufferMemoryBarrier)                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, VkImageMemoryBarrier)                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_BARRIER, VkMemoryBarrier)                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO, VkLayerInstanceCreateInfo)               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO, VkLayerDeviceCreateInfo)                   \
                                                                                                       \
  /* Vulkan 1.1 only, no extension - subgroups, protected memory */                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,                                  \
               VkPhysicalDeviceSubgroupProperties)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO, VkProtectedSubmitInfo)                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2, VkDeviceQueueInfo2)                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,                            \
               VkPhysicalDeviceProtectedMemoryFeatures)                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES,                          \
               VkPhysicalDeviceProtectedMemoryProperties)                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,                      \
               VkPhysicalDeviceShaderDrawParametersFeatures)                                           \
                                                                                                       \
  /* VK_AMD_display_native_hdr */                                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD,                         \
               VkSwapchainDisplayNativeHdrCreateInfoAMD)                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD,                          \
               VkDisplayNativeHdrSurfaceCapabilitiesAMD)                                               \
                                                                                                       \
  /* VK_AMD_shader_core_properties */                                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD,                           \
               VkPhysicalDeviceShaderCorePropertiesAMD)                                                \
                                                                                                       \
  /* VK_AMD_texture_gather_bias_lod */                                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD,                             \
               VkTextureLODGatherFormatPropertiesAMD)                                                  \
                                                                                                       \
  /* VK_EXT_astc_decode_mode */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT, VkImageViewASTCDecodeModeEXT)        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT,                             \
               VkPhysicalDeviceASTCDecodeFeaturesEXT)                                                  \
                                                                                                       \
  /* VK_EXT_buffer_device_address */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT, VkBufferDeviceAddressInfoEXT)         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,                                \
               VkBufferDeviceAddressCreateInfoEXT)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT,                   \
               VkPhysicalDeviceBufferDeviceAddressFeaturesEXT)                                         \
                                                                                                       \
  /* VK_EXT_calibrated_timestamps */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, VkCalibratedTimestampInfoEXT)          \
                                                                                                       \
  /* VK_EXT_conditional_rendering */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,            \
               VkCommandBufferInheritanceConditionalRenderingInfoEXT)                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT,                   \
               VkPhysicalDeviceConditionalRenderingFeaturesEXT)                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT,                                 \
               VkConditionalRenderingBeginInfoEXT)                                                     \
                                                                                                       \
  /* VK_EXT_conservative_rasterization */                                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT,            \
               VkPhysicalDeviceConservativeRasterizationPropertiesEXT)                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,            \
               VkPipelineRasterizationConservativeStateCreateInfoEXT)                                  \
                                                                                                       \
  /* VK_EXT_debug_marker */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT, VkDebugMarkerObjectNameInfoEXT)    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT, VkDebugMarkerObjectTagInfoEXT)      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, VkDebugMarkerMarkerInfoEXT)             \
                                                                                                       \
  /* VK_EXT_debug_report */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,                                \
               VkDebugReportCallbackCreateInfoEXT)                                                     \
                                                                                                       \
  /* VK_EXT_debug_utils */                                                                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, VkDebugUtilsObjectNameInfoEXT)      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT, VkDebugUtilsObjectTagInfoEXT)        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, VkDebugUtilsLabelEXT)                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,                              \
               VkDebugUtilsMessengerCallbackDataEXT)                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,                                \
               VkDebugUtilsMessengerCreateInfoEXT)                                                     \
                                                                                                       \
  /* VK_EXT_depth_clip_enable */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,                       \
               VkPhysicalDeviceDepthClipEnableFeaturesEXT)                                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT,              \
               VkPipelineRasterizationDepthClipStateCreateInfoEXT)                                     \
                                                                                                       \
  /* VK_EXT_descriptor_indexing */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,                  \
               VkDescriptorSetLayoutBindingFlagsCreateInfoEXT)                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,                     \
               VkPhysicalDeviceDescriptorIndexingFeaturesEXT)                                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,                   \
               VkPhysicalDeviceDescriptorIndexingPropertiesEXT)                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,           \
               VkDescriptorSetVariableDescriptorCountAllocateInfoEXT)                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT,          \
               VkDescriptorSetVariableDescriptorCountLayoutSupportEXT)                                 \
                                                                                                       \
  /* VK_EXT_discard_rectangles */                                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT,                     \
               VkPhysicalDeviceDiscardRectanglePropertiesEXT)                                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT,                     \
               VkPipelineDiscardRectangleStateCreateInfoEXT)                                           \
                                                                                                       \
  /* VK_EXT_display_control */                                                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT, VkDisplayPowerInfoEXT)                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT, VkDeviceEventInfoEXT)                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT, VkDisplayEventInfoEXT)                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT, VkSwapchainCounterCreateInfoEXT)   \
                                                                                                       \
  /* VK_EXT_display_surface_counter */                                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT, VkSurfaceCapabilities2EXT)                \
                                                                                                       \
  /* VK_EXT_fragment_density_map */                                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,                    \
               VkPhysicalDeviceFragmentDensityMapFeaturesEXT)                                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT,                  \
               VkPhysicalDeviceFragmentDensityMapPropertiesEXT)                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,                     \
               VkRenderPassFragmentDensityMapCreateInfoEXT)                                            \
                                                                                                       \
  /* VK_EXT_global_priority */                                                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,                         \
               VkDeviceQueueGlobalPriorityCreateInfoEXT)                                               \
                                                                                                       \
  /* VK_EXT_hdr_metadata */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_HDR_METADATA_EXT, VkHdrMetadataEXT)                                   \
                                                                                                       \
  /* VK_EXT_host_query_reset */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT,                        \
               VkPhysicalDeviceHostQueryResetFeaturesEXT)                                              \
                                                                                                       \
  /* VK_EXT_memory_budget */                                                                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,                         \
               VkPhysicalDeviceMemoryBudgetPropertiesEXT)                                              \
                                                                                                       \
  /* VK_EXT_memory_priority */                                                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,                         \
               VkPhysicalDeviceMemoryPriorityFeaturesEXT)                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT, VkMemoryPriorityAllocateInfoEXT)   \
                                                                                                       \
  /* VK_EXT_pci_bus_info */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT,                          \
               VkPhysicalDevicePCIBusInfoPropertiesEXT)                                                \
                                                                                                       \
  /* VK_EXT_pipeline_creation_feedback */                                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT,                           \
               VkPipelineCreationFeedbackCreateInfoEXT)                                                \
                                                                                                       \
  /* VK_EXT_sampler_filter_minmax */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT,                 \
               VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT)                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,                               \
               VkSamplerReductionModeCreateInfoEXT)                                                    \
                                                                                                       \
  /* VK_EXT_sample_locations */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT, VkSampleLocationsInfoEXT)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,                          \
               VkRenderPassSampleLocationsBeginInfoEXT)                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,                      \
               VkPipelineSampleLocationsStateCreateInfoEXT)                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT,                      \
               VkPhysicalDeviceSampleLocationsPropertiesEXT)                                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT, VkMultisamplePropertiesEXT)               \
                                                                                                       \
  /* VK_EXT_scalar_block_layout */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,                     \
               VkPhysicalDeviceScalarBlockLayoutFeaturesEXT)                                           \
                                                                                                       \
  /* VK_EXT_separate_stencil_usage */                                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT,                                  \
               VkImageStencilUsageCreateInfoEXT)                                                       \
                                                                                                       \
  /* VK_EXT_transform_feedback */                                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,                      \
               VkPhysicalDeviceTransformFeedbackFeaturesEXT)                                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT,                    \
               VkPhysicalDeviceTransformFeedbackPropertiesEXT)                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,                  \
               VkPipelineRasterizationStateStreamCreateInfoEXT)                                        \
                                                                                                       \
  /* VK_EXT_validation_cache */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT, VkValidationCacheCreateInfoEXT)     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT,                       \
               VkShaderModuleValidationCacheCreateInfoEXT)                                             \
                                                                                                       \
  /* VK_EXT_validation_features */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, VkValidationFeaturesEXT)                     \
                                                                                                       \
  /* VK_EXT_validation_flags */                                                                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT, VkValidationFlagsEXT)                           \
                                                                                                       \
  /* VK_EXT_vertex_attribute_divisor */                                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT,              \
               VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT)                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT,                  \
               VkPipelineVertexInputDivisorStateCreateInfoEXT)                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT,                \
               VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT)                                      \
                                                                                                       \
  /* VK_EXT_ycbcr_image_arrays */                                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT,                      \
               VkPhysicalDeviceYcbcrImageArraysFeaturesEXT)                                            \
                                                                                                       \
  /* VK_KHR_8bit_storage */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,                            \
               VkPhysicalDevice8BitStorageFeaturesKHR)                                                 \
                                                                                                       \
  /* VK_KHR_16bit_storage */                                                                           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,                               \
               VkPhysicalDevice16BitStorageFeatures)                                                   \
                                                                                                       \
  /* VK_KHR_bind_memory2 */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO, VkBindBufferMemoryInfo)                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, VkBindImageMemoryInfo)                        \
                                                                                                       \
  /* VK_KHR_create_renderpass2 */                                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR, VkAttachmentDescription2KHR)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR, VkAttachmentReference2KHR)                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR, VkSubpassDescription2KHR)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR, VkSubpassDependency2KHR)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR, VkRenderPassCreateInfo2KHR)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR, VkSubpassBeginInfoKHR)                        \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_END_INFO_KHR, VkSubpassEndInfoKHR)                            \
                                                                                                       \
  /* VK_KHR_dedicated_allocation */                                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, VkMemoryDedicatedRequirements)         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, VkMemoryDedicatedAllocateInfo)        \
                                                                                                       \
  /* VK_KHR_depth_stencil_resolve */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR,                 \
               VkPhysicalDeviceDepthStencilResolvePropertiesKHR)                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR,                        \
               VkSubpassDescriptionDepthStencilResolveKHR)                                             \
                                                                                                       \
  /* VK_KHR_descriptor_update_template */                                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,                               \
               VkDescriptorUpdateTemplateCreateInfo)                                                   \
                                                                                                       \
  /* VK_KHR_device_group_creation */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES, VkPhysicalDeviceGroupProperties)    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO, VkDeviceGroupDeviceCreateInfo)       \
                                                                                                       \
  /* VK_KHR_device_group */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR, VkImageSwapchainCreateInfoKHR)       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO,                                  \
               VkBindImageMemoryDeviceGroupInfo)                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO,                                 \
               VkBindBufferMemoryDeviceGroupInfo)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, VkDeviceGroupBindSparseInfo)           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO, VkDeviceGroupSubmitInfo)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO,                               \
               VkDeviceGroupCommandBufferBeginInfo)                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,                                  \
               VkDeviceGroupRenderPassBeginInfo)                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, VkMemoryAllocateFlagsInfo)                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR,                               \
               VkDeviceGroupSwapchainCreateInfoKHR)                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,                                 \
               VkBindImageMemorySwapchainInfoKHR)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR,                                \
               VkDeviceGroupPresentCapabilitiesKHR)                                                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR, VkDeviceGroupPresentInfoKHR)           \
                                                                                                       \
  /* VK_KHR_display_swapchain */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR, VkDisplayPresentInfoKHR)                    \
                                                                                                       \
  /* VK_KHR_driver_properties */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,                                \
               VkPhysicalDeviceDriverPropertiesKHR)                                                    \
                                                                                                       \
  /* VK_KHR_external_fence_capabilities */                                                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,                                  \
               VkPhysicalDeviceExternalFenceInfo)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES, VkExternalFenceProperties)                 \
                                                                                                       \
  /* VK_KHR_external_fence / ..._fd */                                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO, VkExportFenceCreateInfo)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR, VkImportFenceFdInfoKHR)                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR, VkFenceGetFdInfoKHR)                           \
                                                                                                       \
  /* VK_KHR_external_memory_capabilities */                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,                           \
               VkPhysicalDeviceExternalImageFormatInfo)                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES, VkExternalImageFormatProperties)    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,                                 \
               VkPhysicalDeviceExternalBufferInfo)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES, VkExternalBufferProperties)               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES, VkPhysicalDeviceIDProperties)          \
                                                                                                       \
  /* VK_KHR_external_memory / ..._fd */                                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, VkExportMemoryAllocateInfo)              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, VkExternalMemoryImageCreateInfo)   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,                                   \
               VkExternalMemoryBufferCreateInfo)                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, VkImportMemoryFdInfoKHR)                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR, VkMemoryFdPropertiesKHR)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, VkMemoryGetFdInfoKHR)                         \
                                                                                                       \
  /* VK_KHR_external_semaphore_capabilities */                                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,                              \
               VkPhysicalDeviceExternalSemaphoreInfo)                                                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES, VkExternalSemaphoreProperties)         \
                                                                                                       \
  /* VK_KHR_external_semaphore / ..._fd */                                                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, VkExportSemaphoreCreateInfo)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR, VkImportSemaphoreFdInfoKHR)             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, VkSemaphoreGetFdInfoKHR)                   \
                                                                                                       \
  /* VK_KHR_get_display_properties2 */                                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR, VkDisplayProperties2KHR)                    \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PLANE_PROPERTIES_2_KHR, VkDisplayPlaneProperties2KHR)         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR, VkDisplayModeProperties2KHR)           \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR, VkDisplayPlaneInfo2KHR)                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR, VkDisplayPlaneCapabilities2KHR)     \
                                                                                                       \
  /* VK_KHR_get_memory_requirements2 */                                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, VkBufferMemoryRequirementsInfo2)   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, VkImageMemoryRequirementsInfo2)     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2,                              \
               VkImageSparseMemoryRequirementsInfo2)                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, VkMemoryRequirements2)                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2,                                   \
               VkSparseImageMemoryRequirements2)                                                       \
                                                                                                       \
  /* VK_KHR_get_physical_device_properties2 */                                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, VkPhysicalDeviceFeatures2)                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, VkPhysicalDeviceProperties2)            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, VkFormatProperties2)                             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, VkImageFormatProperties2)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,                                  \
               VkPhysicalDeviceImageFormatInfo2)                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, VkQueueFamilyProperties2)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,                                  \
               VkPhysicalDeviceMemoryProperties2)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2, VkSparseImageFormatProperties2)     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2,                           \
               VkPhysicalDeviceSparseImageFormatInfo2)                                                 \
                                                                                                       \
  /* VK_KHR_get_surface_capabilities2 */                                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, VkPhysicalDeviceSurfaceInfo2KHR)  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, VkSurfaceCapabilities2KHR)                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, VkSurfaceFormat2KHR)                            \
                                                                                                       \
  /* VK_KHR_image_format_list */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR, VkImageFormatListCreateInfoKHR)    \
                                                                                                       \
  /* VK_KHR_incremental_present */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR, VkPresentRegionsKHR)                             \
                                                                                                       \
  /* VK_KHR_maintenance2 */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES,                            \
               VkPhysicalDevicePointClippingProperties)                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO,                      \
               VkRenderPassInputAttachmentAspectCreateInfo)                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO, VkImageViewUsageCreateInfo)             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO,                \
               VkPipelineTessellationDomainOriginStateCreateInfo)                                      \
                                                                                                       \
  /* VK_KHR_maintenance3 */                                                                            \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,                             \
               VkPhysicalDeviceMaintenance3Properties)                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT, VkDescriptorSetLayoutSupport)          \
                                                                                                       \
  /* VK_KHR_multiview */                                                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, VkRenderPassMultiviewCreateInfo)   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,                                   \
               VkPhysicalDeviceMultiviewFeatures)                                                      \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES,                                 \
               VkPhysicalDeviceMultiviewProperties)                                                    \
                                                                                                       \
  /* VK_KHR_push_descriptor */                                                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR,                       \
               VkPhysicalDevicePushDescriptorPropertiesKHR)                                            \
                                                                                                       \
  /* VK_KHR_sampler_ycbcr_conversion */                                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,                                 \
               VkSamplerYcbcrConversionCreateInfo)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, VkSamplerYcbcrConversionInfo)          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, VkBindImagePlaneMemoryInfo)             \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,                                 \
               VkImagePlaneMemoryRequirementsInfo)                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,                    \
               VkPhysicalDeviceSamplerYcbcrConversionFeatures)                                         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES,                     \
               VkSamplerYcbcrConversionImageFormatProperties)                                          \
                                                                                                       \
  /* VK_KHR_shader_atomic_int64 */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR,                     \
               VkPhysicalDeviceShaderAtomicInt64FeaturesKHR)                                           \
                                                                                                       \
  /* VK_KHR_shader_float16_int8 */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR,                            \
               VkPhysicalDeviceFloat16Int8FeaturesKHR)                                                 \
                                                                                                       \
  /* VK_KHR_shader_float_controls */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR,                        \
               VkPhysicalDeviceFloatControlsPropertiesKHR)                                             \
                                                                                                       \
  /* VK_KHR_shared_presentable_image */                                                                \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR,                              \
               VkSharedPresentSurfaceCapabilitiesKHR)                                                  \
                                                                                                       \
  /* VK_KHR_swapchain */                                                                               \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, VkSwapchainCreateInfoKHR)                  \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, VkPresentInfoKHR)                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR, VkAcquireNextImageInfoKHR)               \
                                                                                                       \
  /* VK_KHR_variable_pointers */                                                                       \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES,                           \
               VkPhysicalDeviceVariablePointerFeatures)                                                \
                                                                                                       \
  /* VK_KHR_vulkan_memory_model */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR,                     \
               VkPhysicalDeviceVulkanMemoryModelFeaturesKHR)                                           \
                                                                                                       \
  /* VK_NV_dedicated_allocation */                                                                     \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV,                         \
               VkDedicatedAllocationMemoryAllocateInfoNV)                                              \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV,                            \
               VkDedicatedAllocationImageCreateInfoNV)                                                 \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV,                           \
               VkDedicatedAllocationBufferCreateInfoNV)                                                \
                                                                                                       \
  /* VK_NV_external_memory */                                                                          \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV, VkExportMemoryAllocateInfoNV)         \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV,                                 \
               VkExternalMemoryImageCreateInfoNV)                                                      \
                                                                                                       \
  /* VK_NV_shader_image_footprint */                                                                   \
  PNEXT_STRUCT(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV,                   \
               VkPhysicalDeviceShaderImageFootprintFeaturesNV)                                         \
                                                                                                       \
  /* Surface creation structs. These would pull in dependencies on OS-specific includes. */            \
  /* So treat them as unsupported. */                                                                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR)                                 \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR)                                    \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR)                                 \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA)                           \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK)                                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK)                                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT)                                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP)                                         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP)                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN)                                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR)                                 \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR)                                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR)                                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR)                                    \
                                                                                                       \
  /* VK_AMD_memory_overallocation_behavior */                                                          \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD)                    \
                                                                                                       \
  /* VK_AMD_rasterization_order */                                                                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD)            \
                                                                                                       \
  /* VK_ANDROID_external_memory_android_hardware_buffer */                                             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID)                           \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID)                      \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID)               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID)                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID)                 \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID)                                         \
                                                                                                       \
  /* VK_EXT_blend_operation_advanced */                                                                \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT)           \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT)         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT)             \
                                                                                                       \
  /* VK_EXT_external_memory_host */                                                                    \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT)                             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT)                              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT)             \
                                                                                                       \
  /* VK_EXT_image_drm_format_modifier */                                                               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT)                         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT)                              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT)              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT)                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT)              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT)                        \
                                                                                                       \
  /* VK_EXT_inline_uniform_block */                                                                    \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT)               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT)             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT)                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT)            \
                                                                                                       \
  /* VK_EXT_filter_cubic */                                                                            \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT)                \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT)             \
                                                                                                       \
  /* VK_GOOGLE_display_timing */                                                                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE)                                       \
                                                                                                       \
  /* VK_KHR_surface_protected_capabilities */                                                          \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR)                              \
                                                                                                       \
  /* VK_NV_clip_space_w_scaling */                                                                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV)                \
                                                                                                       \
  /* VK_NV_compute_shader_derivatives */                                                               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV)          \
                                                                                                       \
  /* VK_NV_cooperative_matrix */                                                                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV)                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV)                                \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV)                \
                                                                                                       \
  /* VK_NV_corner_sampled_image */                                                                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV)                \
                                                                                                       \
  /* VK_NV_dedicated_allocation_image_aliasing */                                                      \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV) \
                                                                                                       \
  /* VK_NV_device_diagnostic_checkpoints */                                                            \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV)                                              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV)                           \
                                                                                                       \
  /* VK_NV_fragment_coverage_to_color */                                                               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV)                 \
                                                                                                       \
  /* VK_NV_fragment_shader_barycentric */                                                              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV)         \
                                                                                                       \
  /* VK_NV_framebuffer_mixed_samples */                                                                \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV)               \
                                                                                                       \
  /* VK_NVX_image_view_handle */                                                                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX)                                      \
                                                                                                       \
  /* VK_NV_mesh_shader */                                                                              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV)                         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV)                       \
                                                                                                       \
  /* VK_NV_ray_tracing */                                                                              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV)                             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV)                           \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_GEOMETRY_NV)                                                     \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV)                                           \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV)                                                \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV)                      \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV)                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV)              \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV)                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV)                         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV)                                  \
                                                                                                       \
  /* VK_NV_representative_fragment_test */                                                             \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV)        \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV)      \
                                                                                                       \
  /* VK_NV_scissor_exclusive */                                                                        \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV)        \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV)                   \
                                                                                                       \
  /* VK_NV_shading_rate_image */                                                                       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV)       \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV)                  \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV)                \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV)      \
                                                                                                       \
  /* VK_NV_viewport_swizzle */                                                                         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV)                  \
                                                                                                       \
  /* VK_NVX_device_generated_commands */                                                               \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_OBJECT_TABLE_CREATE_INFO_NVX)                                    \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NVX)                        \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_CMD_PROCESS_COMMANDS_INFO_NVX)                                   \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_CMD_RESERVE_SPACE_FOR_COMMANDS_INFO_NVX)                         \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DEVICE_GENERATED_COMMANDS_LIMITS_NVX)                            \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_DEVICE_GENERATED_COMMANDS_FEATURES_NVX)                          \
                                                                                                       \
  /* VK_NVX_multiview_per_view_attributes */                                                           \
  PNEXT_UNSUPPORTED(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX)

template <typename SerialiserType>
static void SerialiseNext(SerialiserType &ser, VkStructureType &sType, const void *&pNext)
{
  // this is the parent sType, serialised here for convenience
  ser.Serialise("sType"_lit, sType);

  if(ser.IsReading())
  {
    // default to a NULL pNext
    pNext = NULL;

    // serialise a nullable next type, to tell us the type of object that's serialised. This is
    // hidden as it doesn't correspond to the actual pNext, it's just metadata to help us launch the
    // serialisation.
    VkStructureType *nextType = NULL;
    ser.SerialiseNullable("pNext"_lit, nextType);

    // most common case - no pNext serialised. Bail immediately
    if(nextType == NULL)
    {
      // fixup the structured data, set the type to void*
      ser.TypedAs("void *"_lit);
      return;
    }

    // hide and rename the pNextType if we got something - we'll want the pNext below to be the only
    // thing user-facing.
    ser.Named("pNextType"_lit).Hidden();

#define PNEXT_UNSUPPORTED(StructType)                                    \
  case StructType:                                                       \
  {                                                                      \
    RDCERR("No support for " #StructType " is available in this build"); \
    pNext = NULL;                                                        \
    break;                                                               \
  }

// if we come across a struct we should process, then serialise a pointer to it.
#define PNEXT_STRUCT(StructType, StructName)        \
  case StructType:                                  \
  {                                                 \
    StructName *nextStruct = NULL;                  \
    ser.SerialiseNullable("pNext"_lit, nextStruct); \
    pNext = nextStruct;                             \
    handled = true;                                 \
    break;                                          \
  }

    // we don't want a default case to ensure we get a compile error if we forget to implement a
    // structure type, but we also want to error if the input is invalid, so have this flag here.
    bool handled = false;

    // this serialises the pNext with the right type, as nullable. We already know from above that
    // there IS something here, so the nullable is redundant but convenient
    switch(*nextType)
    {
      HANDLE_PNEXT();
      case VK_STRUCTURE_TYPE_RANGE_SIZE:
      case VK_STRUCTURE_TYPE_MAX_ENUM: break;
    }

    if(!handled)
      RDCERR("Invalid next structure sType: %x", *nextType);

    // delete the type itself. Any pNext we serialised is saved in the pNext pointer and will be
    // deleted in DeserialiseNext()
    delete nextType;

    // note, we don't have to serialise more of the chain - this is recursive, if there was more of
    // the pNext chain it would be done recursively above
  }
  else    // ser.IsWriting()
  {
#undef PNEXT_UNSUPPORTED
#define PNEXT_UNSUPPORTED(StructType)                                    \
  case StructType:                                                       \
  {                                                                      \
    RDCERR("No support for " #StructType " is available in this build"); \
    return;                                                              \
  }

// if we come across a struct we should process, then serialise a pointer to its type (to tell the
// reading serialisation what struct is coming up), then a pointer to it.
// We don't have to go any further, the act of serialising this struct will walk the chain further,
// so we can return immediately.
#undef PNEXT_STRUCT
#define PNEXT_STRUCT(StructType, StructName)          \
  case StructType:                                    \
  {                                                   \
    VkStructureType *nextType = &next->sType;         \
    ser.SerialiseNullable("pNextType"_lit, nextType); \
    StructName *actual = (StructName *)next;          \
    ser.SerialiseNullable("pNext"_lit, actual);       \
    handled = true;                                   \
    return;                                           \
  }

    // we don't want a default case to ensure we get a compile error if we forget to implement a
    // structure type, but we also want to error if the input is invalid, so have this flag here.
    bool handled = false;

    // walk the pNext chain, skipping any structs we don't care about serialising.
    VkBaseInStructure *next = (VkBaseInStructure *)pNext;

    while(next)
    {
      switch(next->sType)
      {
        HANDLE_PNEXT();
        case VK_STRUCTURE_TYPE_RANGE_SIZE:
        case VK_STRUCTURE_TYPE_MAX_ENUM: break;
      }

      if(!handled)
        RDCERR("Invalid pNext structure sType: %x", next->sType);

      // walk to the next item if we didn't serialise the current one
      next = (VkBaseInStructure *)next->pNext;
    }

    // if we got here, either pNext was NULL (common) or we skipped the whole chain. Serialise a
    // NULL structure type to indicate that.
    VkStructureType *dummy = NULL;
    ser.SerialiseNullable("pNext"_lit, dummy);
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

#undef PNEXT_STRUCT
#define PNEXT_STRUCT(StructType, StructName)            \
  case StructType:                                      \
  {                                                     \
    VkStructureType *nextType = (VkStructureType *)gen; \
    Deserialise(nextType);                              \
    return;                                             \
  }

  // walk the chain, deserialising from the tail back
  const VkBaseInStructure *gen = (const VkBaseInStructure *)pNext;
  switch(gen->sType)
  {
    HANDLE_PNEXT();
    case VK_STRUCTURE_TYPE_RANGE_SIZE:
    case VK_STRUCTURE_TYPE_MAX_ENUM: DeserialiseNext(gen); break;
  }
  delete gen;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkApplicationInfo &el)
{
  RDCERR("Serialising VkApplicationInfo - this should always be a NULL optional element");
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkInstanceCreateInfo &el)
{
  RDCERR("Serialising VkInstanceCreateInfo - this should always be a NULL optional element");
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkLayerInstanceCreateInfo &el)
{
  RDCERR("Serialising VkLayerInstanceCreateInfo - this should always be a NULL optional element");
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkLayerDeviceCreateInfo &el)
{
  RDCERR("Serialising VkLayerDeviceCreateInfo - this should always be a NULL optional element");
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAllocationCallbacks &el)
{
  RDCERR("Serialising VkAllocationCallbacks - this should always be a NULL optional element");
  RDCEraseEl(el);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugMarkerObjectNameInfoEXT &el)
{
  RDCERR("Serialising VkDebugMarkerObjectNameInfoEXT - this should be handled specially");
  // can't handle it here without duplicating objectType logic
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugMarkerObjectTagInfoEXT &el)
{
  RDCERR("Serialising VkDebugMarkerObjectTagInfoEXT - this should be handled specially");
  // can't handle it here without duplicating objectType logic
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugUtilsObjectNameInfoEXT &el)
{
  RDCERR("Serialising VkDebugUtilsObjectNameInfoEXT - this should be handled specially");
  // can't handle it here without duplicating objectType logic
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
}

template <>
void Deserialise(const VkDebugUtilsObjectNameInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugUtilsObjectTagInfoEXT &el)
{
  RDCERR("Serialising VkDebugUtilsObjectTagInfoEXT - this should be handled specially");
  // can't handle it here without duplicating objectType logic
  RDCEraseEl(el);
  el.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceQueueCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkDeviceQueueCreateFlags, flags);
  SERIALISE_MEMBER(queueFamilyIndex);
  SERIALISE_MEMBER(queueCount);
  SERIALISE_MEMBER_ARRAY(pQueuePriorities, queueCount);
}

template <>
void Deserialise(const VkDeviceQueueCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pQueuePriorities;
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
  SERIALISE_MEMBER_VKFLAGS(VkMemoryHeapFlags, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryType &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkMemoryPropertyFlags, propertyFlags);
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
    ser.Serialise("minMemoryMapAlignment"_lit, minMemoryMapAlignment);
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
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, framebufferColorSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, framebufferDepthSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, framebufferStencilSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, framebufferNoAttachmentsSampleCounts);
  SERIALISE_MEMBER(maxColorAttachments);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, sampledImageColorSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, sampledImageIntegerSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, sampledImageDepthSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, sampledImageStencilSampleCounts);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, storageImageSampleCounts);
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
  SERIALISE_MEMBER_VKFLAGS(VkQueueFlags, queueFlags);
  SERIALISE_MEMBER(queueCount);
  SERIALISE_MEMBER(timestampValidBits);
  SERIALISE_MEMBER(minImageTransferGranularity);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceProperties &el)
{
  // serialise apiVersion as packed version so we get a nice display.
  VkPackedVersion packedVer = el.apiVersion;
  ser.Serialise("apiVersion"_lit, packedVer).TypedAs("uint32_t"_lit);
  el.apiVersion = packedVer;

  // driver version is *not* necessarily via VK_MAKE_VERSION
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

  SERIALISE_MEMBER_VKFLAGS(VkDeviceCreateFlags, flags);
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
  for(uint32_t i = 0; el.pQueueCreateInfos && i < el.queueCreateInfoCount; i++)
    Deserialise(el.pQueueCreateInfos[i]);
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

  SERIALISE_MEMBER_VKFLAGS(VkBufferCreateFlags, flags);
  SERIALISE_MEMBER(size);
  SERIALISE_MEMBER_VKFLAGS(VkBufferUsageFlags, usage);
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

  SERIALISE_MEMBER_VKFLAGS(VkBufferViewCreateFlags, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkImageCreateFlags, flags);
  SERIALISE_MEMBER(imageType);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(mipLevels);
  SERIALISE_MEMBER(arrayLayers);
  SERIALISE_MEMBER(samples);
  SERIALISE_MEMBER(tiling);
  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, usage);
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

  SERIALISE_MEMBER_VKFLAGS(VkImageViewCreateFlags, flags);
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
  SERIALISE_MEMBER_VKFLAGS(VkSparseMemoryBindFlags, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseBufferMemoryBindInfo &el)
{
  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(bindCount);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <>
void Deserialise(const VkSparseBufferMemoryBindInfo &el)
{
  delete[] el.pBinds;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageOpaqueMemoryBindInfo &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(bindCount);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <>
void Deserialise(const VkSparseImageOpaqueMemoryBindInfo &el)
{
  delete[] el.pBinds;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageMemoryBind &el)
{
  SERIALISE_MEMBER(subresource);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memoryOffset);
  SERIALISE_MEMBER_VKFLAGS(VkSparseMemoryBindFlags, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageMemoryBindInfo &el)
{
  SERIALISE_MEMBER(image);
  SERIALISE_MEMBER(bindCount);
  SERIALISE_MEMBER_ARRAY(pBinds, bindCount);
}

template <>
void Deserialise(const VkSparseImageMemoryBindInfo &el)
{
  delete[] el.pBinds;
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
  for(uint32_t i = 0; el.pBufferBinds && i < el.bufferBindCount; i++)
    Deserialise(el.pBufferBinds[i]);
  delete[] el.pBufferBinds;
  for(uint32_t i = 0; el.pImageOpaqueBinds && i < el.imageOpaqueBindCount; i++)
    Deserialise(el.pImageOpaqueBinds[i]);
  delete[] el.pImageOpaqueBinds;
  for(uint32_t i = 0; el.pImageBinds && i < el.imageBindCount; i++)
    Deserialise(el.pImageBinds[i]);
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

  ser.Serialise("pWaitDstStageMask"_lit, *u.typed, el.waitSemaphoreCount,
                SerialiserFlags::AllocateMemory)
      .TypedAs("VkPipelineStageFlags"_lit);

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
  delete[] el.pWaitDstStageMask;
  delete[] el.pCommandBuffers;
  delete[] el.pSignalSemaphores;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFramebufferCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkFramebufferCreateFlags, flags);
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
  SERIALISE_MEMBER_VKFLAGS(VkAttachmentDescriptionFlags, flags);
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
  SERIALISE_MEMBER_VKFLAGS(VkSubpassDescriptionFlags, flags);
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

template <>
void Deserialise(const VkSubpassDescription &el)
{
  delete[] el.pInputAttachments;
  delete[] el.pColorAttachments;
  delete[] el.pResolveAttachments;
  delete el.pDepthStencilAttachment;
  delete[] el.pPreserveAttachments;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassDependency &el)
{
  SERIALISE_MEMBER(srcSubpass);
  SERIALISE_MEMBER(dstSubpass);
  SERIALISE_MEMBER_VKFLAGS(VkPipelineStageFlags, srcStageMask);
  SERIALISE_MEMBER_VKFLAGS(VkPipelineStageFlags, dstStageMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, srcAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, dstAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkDependencyFlags, dependencyFlags);
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

  SERIALISE_MEMBER_VKFLAGS(VkRenderPassCreateFlags, flags);
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
  for(uint32_t i = 0; el.pSubpasses && i < el.subpassCount; i++)
    Deserialise(el.pSubpasses[i]);
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

  SERIALISE_MEMBER_VKFLAGS(VkPipelineVertexInputStateCreateFlags, flags);
  SERIALISE_MEMBER(vertexBindingDescriptionCount);
  SERIALISE_MEMBER_ARRAY(pVertexBindingDescriptions, vertexBindingDescriptionCount);
  SERIALISE_MEMBER(vertexAttributeDescriptionCount);
  SERIALISE_MEMBER_ARRAY(pVertexAttributeDescriptions, vertexAttributeDescriptionCount);
}

template <>
void Deserialise(const VkPipelineVertexInputStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pVertexBindingDescriptions;
  delete[] el.pVertexAttributeDescriptions;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineInputAssemblyStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineInputAssemblyStateCreateFlags, flags);
  SERIALISE_MEMBER(topology);
  SERIALISE_MEMBER(primitiveRestartEnable);
}

template <>
void Deserialise(const VkPipelineInputAssemblyStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineTessellationStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineTessellationStateCreateFlags, flags);
  SERIALISE_MEMBER(patchControlPoints);
}

template <>
void Deserialise(const VkPipelineTessellationStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineViewportStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineViewportStateCreateFlags, flags);

  SERIALISE_MEMBER(viewportCount);
  SERIALISE_MEMBER_ARRAY(pViewports, viewportCount);
  SERIALISE_MEMBER(scissorCount);
  SERIALISE_MEMBER_ARRAY(pScissors, scissorCount);
}

template <>
void Deserialise(const VkPipelineViewportStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pViewports;
  delete[] el.pScissors;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineRasterizationStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineRasterizationStateCreateFlags, flags);
  SERIALISE_MEMBER(depthClampEnable);
  SERIALISE_MEMBER(rasterizerDiscardEnable);
  SERIALISE_MEMBER(polygonMode);
  SERIALISE_MEMBER_VKFLAGS(VkCullModeFlags, cullMode);
  SERIALISE_MEMBER(frontFace);
  SERIALISE_MEMBER(depthBiasEnable);
  SERIALISE_MEMBER(depthBiasConstantFactor);
  SERIALISE_MEMBER(depthBiasClamp);
  SERIALISE_MEMBER(depthBiasSlopeFactor);
  SERIALISE_MEMBER(lineWidth);
}

template <>
void Deserialise(const VkPipelineRasterizationStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineMultisampleStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineMultisampleStateCreateFlags, flags);
  SERIALISE_MEMBER(rasterizationSamples);
  RDCASSERT(el.rasterizationSamples <= VK_SAMPLE_COUNT_32_BIT);
  SERIALISE_MEMBER(sampleShadingEnable);
  SERIALISE_MEMBER(minSampleShading);
  SERIALISE_MEMBER_OPT(pSampleMask);
  SERIALISE_MEMBER(alphaToCoverageEnable);
  SERIALISE_MEMBER(alphaToOneEnable);
}

template <>
void Deserialise(const VkPipelineMultisampleStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete el.pSampleMask;
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
  SERIALISE_MEMBER_VKFLAGS(VkColorComponentFlags, colorWriteMask);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineColorBlendStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineColorBlendStateCreateFlags, flags);
  SERIALISE_MEMBER(logicOpEnable);
  SERIALISE_MEMBER(logicOp);
  SERIALISE_MEMBER(attachmentCount);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
  SERIALISE_MEMBER(blendConstants);
}

template <>
void Deserialise(const VkPipelineColorBlendStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pAttachments;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineDepthStencilStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineDepthStencilStateCreateFlags, flags);
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

template <>
void Deserialise(const VkPipelineDepthStencilStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineDynamicStateCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineDynamicStateCreateFlags, flags);
  SERIALISE_MEMBER(dynamicStateCount);
  SERIALISE_MEMBER_ARRAY(pDynamicStates, dynamicStateCount);
}

template <>
void Deserialise(const VkPipelineDynamicStateCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pDynamicStates;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandPoolCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkCommandPoolCreateFlags, flags);
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
  SERIALISE_MEMBER_VKFLAGS(VkQueryControlFlags, queryFlags);
  SERIALISE_MEMBER_VKFLAGS(VkQueryPipelineStatisticFlags, pipelineStatistics);
}

template <>
void Deserialise(const VkCommandBufferInheritanceInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandBufferBeginInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkCommandBufferUsageFlags, flags);
  SERIALISE_MEMBER_OPT(pInheritanceInfo);
}

template <>
void Deserialise(const VkCommandBufferBeginInfo &el)
{
  DeserialiseNext(el.pNext);
  if(el.pInheritanceInfo)
    Deserialise(*el.pInheritanceInfo);
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

  SERIALISE_MEMBER_VKFLAGS(VkQueryPoolCreateFlags, flags);
  SERIALISE_MEMBER(queryType);
  SERIALISE_MEMBER(queryCount);
  SERIALISE_MEMBER_VKFLAGS(VkQueryPipelineStatisticFlags, pipelineStatistics);
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

  SERIALISE_MEMBER_VKFLAGS(VkSemaphoreCreateFlags, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkEventCreateFlags, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkFenceCreateFlags, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkSamplerCreateFlags, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkPipelineShaderStageCreateFlags, flags);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(module);
  SERIALISE_MEMBER(pName);
  SERIALISE_MEMBER_OPT(pSpecializationInfo);
}

template <>
void Deserialise(const VkPipelineShaderStageCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  if(el.pSpecializationInfo)
  {
    Deserialise(*el.pSpecializationInfo);
    delete el.pSpecializationInfo;
  }
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
    ser.Serialise("size"_lit, size);
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
    ser.Serialise("dataSize"_lit, dataSize);
    if(ser.IsReading())
      el.dataSize = (size_t)dataSize;
  }

  SERIALISE_MEMBER_ARRAY(pData, dataSize);
}

template <>
void Deserialise(const VkSpecializationInfo &el)
{
  FreeAlignedBuffer((byte *)el.pData);
  delete[] el.pMapEntries;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineCacheCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineCacheCreateFlags, flags);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t initialDataSize = el.initialDataSize;
    ser.Serialise("initialDataSize"_lit, initialDataSize);
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

  SERIALISE_MEMBER_VKFLAGS(VkPipelineLayoutCreateFlags, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkShaderModuleCreateFlags, flags);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t codeSize = el.codeSize;
    ser.Serialise("codeSize"_lit, codeSize);
    if(ser.IsReading())
      el.codeSize = (size_t)codeSize;
  }

  // serialise as void* so it goes through as a buffer, not an actual array of integers.
  {
    const void *pCode = el.pCode;
    ser.Serialise("pCode"_lit, pCode, el.codeSize, SerialiserFlags::AllocateMemory);
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
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
  SERIALISE_MEMBER(baseMipLevel);
  SERIALISE_MEMBER(levelCount);
  SERIALISE_MEMBER(baseArrayLayer);
  SERIALISE_MEMBER(layerCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSubresourceLayers &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
  SERIALISE_MEMBER(mipLevel);
  SERIALISE_MEMBER(baseArrayLayer);
  SERIALISE_MEMBER(layerCount);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSubresource &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
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

  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, srcAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, dstAccessMask);
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

  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, srcAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, dstAccessMask);
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

  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, srcAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, dstAccessMask);
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

  SERIALISE_MEMBER_VKFLAGS(VkPipelineCreateFlags, flags);
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

  if(el.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    SERIALISE_MEMBER(basePipelineHandle);
  else
    SERIALISE_MEMBER_EMPTY(basePipelineHandle);

  SERIALISE_MEMBER(basePipelineIndex);
}

template <>
void Deserialise(const VkGraphicsPipelineCreateInfo &el)
{
  DeserialiseNext(el.pNext);

  if(el.pVertexInputState)
  {
    Deserialise(*el.pVertexInputState);
    delete el.pVertexInputState;
  }

  if(el.pInputAssemblyState)
  {
    Deserialise(*el.pInputAssemblyState);
    delete el.pInputAssemblyState;
  }

  if(el.pTessellationState)
  {
    Deserialise(*el.pTessellationState);
    delete el.pTessellationState;
  }
  if(el.pViewportState)
  {
    Deserialise(*el.pViewportState);
    delete el.pViewportState;
  }
  if(el.pRasterizationState)
  {
    Deserialise(*el.pRasterizationState);
    delete el.pRasterizationState;
  }
  if(el.pMultisampleState)
  {
    Deserialise(*el.pMultisampleState);
    delete el.pMultisampleState;
  }
  if(el.pDepthStencilState)
  {
    Deserialise(*el.pDepthStencilState);
    delete el.pDepthStencilState;
  }
  if(el.pColorBlendState)
  {
    Deserialise(*el.pColorBlendState);
    delete el.pColorBlendState;
  }
  if(el.pDynamicState)
  {
    Deserialise(*el.pDynamicState);
    delete el.pDynamicState;
  }
  for(uint32_t i = 0; el.pStages && i < el.stageCount; i++)
  {
    Deserialise(el.pStages[i]);
  }
  delete[] el.pStages;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkComputePipelineCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineCreateFlags, flags);
  SERIALISE_MEMBER(stage);
  SERIALISE_MEMBER(layout);

  if(el.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    SERIALISE_MEMBER(basePipelineHandle);
  else
    SERIALISE_MEMBER_EMPTY(basePipelineHandle);

  SERIALISE_MEMBER(basePipelineIndex);
}

template <>
void Deserialise(const VkComputePipelineCreateInfo &el)
{
  DeserialiseNext(el.pNext);
  DeserialiseNext(el.stage.pNext);
  if(el.stage.pSpecializationInfo)
  {
    Deserialise(*el.stage.pSpecializationInfo);
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

  SERIALISE_MEMBER_VKFLAGS(VkDescriptorPoolCreateFlags, flags);
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
  SERIALISE_MEMBER_VKFLAGS(VkShaderStageFlags, stageFlags);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(size);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutBinding &el)
{
  SERIALISE_MEMBER(binding);
  SERIALISE_MEMBER(descriptorType);
  SERIALISE_MEMBER(descriptorCount);
  SERIALISE_MEMBER_VKFLAGS(VkShaderStageFlags, stageFlags);
  SERIALISE_MEMBER_ARRAY(pImmutableSamplers, descriptorCount);
}

template <>
void Deserialise(const VkDescriptorSetLayoutBinding &el)
{
  delete[] el.pImmutableSamplers;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkDescriptorSetLayoutCreateFlags, flags);
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
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
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
void DoSerialise(SerialiserType &ser, VkSharedPresentSurfaceCapabilitiesKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, sharedPresentSupportedUsageFlags);
}

template <>
void Deserialise(const VkSharedPresentSurfaceCapabilitiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSwapchainCreateInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkSwapchainCreateFlagsKHR, flags);

  // don't need the surface
  SERIALISE_MEMBER_EMPTY(surface);

  SERIALISE_MEMBER(minImageCount);
  SERIALISE_MEMBER(imageFormat);
  SERIALISE_MEMBER(imageColorSpace);
  SERIALISE_MEMBER(imageExtent);
  SERIALISE_MEMBER(imageArrayLayers);
  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, imageUsage);
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
void DoSerialise(SerialiserType &ser, VkSwapchainDisplayNativeHdrCreateInfoAMD &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(localDimmingEnable);
}

template <>
void Deserialise(const VkSwapchainDisplayNativeHdrCreateInfoAMD &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPresentInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(waitSemaphoreCount);
  SERIALISE_MEMBER_ARRAY(pWaitSemaphores, waitSemaphoreCount);

  SERIALISE_MEMBER(swapchainCount);
  SERIALISE_MEMBER_ARRAY_EMPTY(pSwapchains);
  SERIALISE_MEMBER_ARRAY(pImageIndices, swapchainCount);
  SERIALISE_MEMBER_ARRAY(pResults, swapchainCount);
}

template <>
void Deserialise(const VkPresentInfoKHR &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pWaitSemaphores;
  delete[] el.pSwapchains;
  delete[] el.pImageIndices;
  delete[] el.pResults;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAcquireNextImageInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  // don't need the swap chain itself
  SERIALISE_MEMBER_EMPTY(swapchain);
  SERIALISE_MEMBER(timeout);
  SERIALISE_MEMBER(semaphore);
  SERIALISE_MEMBER(fence);
  SERIALISE_MEMBER(deviceMask);
}

template <>
void Deserialise(const VkAcquireNextImageInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceVariablePointerFeatures &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(variablePointersStorageBuffer);
  SERIALISE_MEMBER(variablePointers);
}

template <>
void Deserialise(const VkPhysicalDeviceVariablePointerFeatures &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceVulkanMemoryModelFeaturesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(vulkanMemoryModel);
  SERIALISE_MEMBER(vulkanMemoryModelDeviceScope);

  // this field was added in Vulkan 1.1.99
  if(ser.VersionAtLeast(0xF))
  {
    SERIALISE_MEMBER(vulkanMemoryModelAvailabilityVisibilityChains);
  }
  else if(ser.IsReading())
  {
    // default to FALSE conservatively
    el.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
  }
}

template <>
void Deserialise(const VkPhysicalDeviceVulkanMemoryModelFeaturesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceYcbcrImageArraysFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(ycbcrImageArrays);
}

template <>
void Deserialise(const VkPhysicalDeviceYcbcrImageArraysFeaturesEXT &el)
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

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugReportCallbackCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkDebugReportFlagsEXT, flags);

  // serialise pointers as uint64 to ensure portability. We can't do much with the values apart from
  // display them
  uint64_t pfnCallback = (uint64_t)el.pfnCallback;
  uint64_t pUserData = (uint64_t)el.pUserData;
  ser.Serialise("pfnCallback"_lit, pfnCallback);
  ser.Serialise("pUserData"_lit, pUserData);
}

template <>
void Deserialise(const VkDebugReportCallbackCreateInfoEXT &el)
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
void DoSerialise(SerialiserType &ser, VkDebugUtilsMessengerCallbackDataEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkDebugUtilsMessengerCallbackDataFlagsEXT, flags);
  SERIALISE_MEMBER(pMessageIdName);
  SERIALISE_MEMBER(messageIdNumber);
  SERIALISE_MEMBER(pMessage);
  SERIALISE_MEMBER(queueLabelCount);
  SERIALISE_MEMBER_ARRAY(pQueueLabels, queueLabelCount);
  SERIALISE_MEMBER(cmdBufLabelCount);
  SERIALISE_MEMBER_ARRAY(pCmdBufLabels, cmdBufLabelCount);
  SERIALISE_MEMBER(objectCount);
  SERIALISE_MEMBER_ARRAY(pObjects, objectCount);
}

template <>
void Deserialise(const VkDebugUtilsMessengerCallbackDataEXT &el)
{
  DeserialiseNext(el.pNext);

  for(uint32_t i = 0; el.pQueueLabels && i < el.queueLabelCount; i++)
    Deserialise(el.pQueueLabels[i]);
  delete[] el.pQueueLabels;
  for(uint32_t i = 0; el.pCmdBufLabels && i < el.cmdBufLabelCount; i++)
    Deserialise(el.pCmdBufLabels[i]);
  delete[] el.pCmdBufLabels;
  for(uint32_t i = 0; el.pObjects && i < el.objectCount; i++)
    Deserialise(el.pObjects[i]);
  delete[] el.pObjects;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDebugUtilsMessengerCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkDebugUtilsMessengerCreateFlagsEXT, flags);
  SERIALISE_MEMBER_VKFLAGS(VkDebugUtilsMessageSeverityFlagsEXT, messageSeverity);
  SERIALISE_MEMBER_VKFLAGS(VkDebugUtilsMessageTypeFlagsEXT, messageType);

  // serialise pointers as uint64 to ensure portability. We can't do much with the values apart from
  // display them
  uint64_t pfnUserCallback = (uint64_t)el.pfnUserCallback;
  uint64_t pUserData = (uint64_t)el.pUserData;
  ser.Serialise("pfnUserCallback"_lit, pfnUserCallback);
  ser.Serialise("pUserData"_lit, pUserData);
}

template <>
void Deserialise(const VkDebugUtilsMessengerCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

// this isn't a real vulkan type, it's our own "anything that could be in a descriptor"
// structure that
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DescriptorSetSlotImageInfo &el)
{
  SERIALISE_MEMBER(sampler).TypedAs("VkSampler"_lit);
  SERIALISE_MEMBER(imageView).TypedAs("VkImageView"_lit);
  SERIALISE_MEMBER(imageLayout);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DescriptorSetSlotBufferInfo &el)
{
  SERIALISE_MEMBER(buffer).TypedAs("VkBuffer"_lit);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(range);
}

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

  SERIALISE_MEMBER(bufferInfo).TypedAs("VkDescriptorBufferInfo"_lit);
  SERIALISE_MEMBER(imageInfo).TypedAs("VkDescriptorImageInfo"_lit);
  SERIALISE_MEMBER(texelBufferView).TypedAs("VkBufferView"_lit);
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
  SERIALISE_MEMBER(imageInfo);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, ImageInfo &el)
{
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
    ser.Serialise("offset"_lit, offset);
    ser.Serialise("stride"_lit, stride);
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

  SERIALISE_MEMBER_VKFLAGS(VkDescriptorUpdateTemplateCreateFlags, flags);
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
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceConservativeRasterizationPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(primitiveOverestimationSize);
  SERIALISE_MEMBER(maxExtraPrimitiveOverestimationSize);
  SERIALISE_MEMBER(extraPrimitiveOverestimationSizeGranularity);
  SERIALISE_MEMBER(primitiveUnderestimation);
  SERIALISE_MEMBER(conservativePointAndLineRasterization);
  SERIALISE_MEMBER(degenerateTrianglesRasterized);
  SERIALISE_MEMBER(degenerateLinesRasterized);
  SERIALISE_MEMBER(fullyCoveredFragmentShaderInputVariable);
  SERIALISE_MEMBER(conservativeRasterizationPostDepthCoverage);
}

template <>
void Deserialise(const VkPhysicalDeviceConservativeRasterizationPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceDepthStencilResolvePropertiesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(supportedDepthResolveModes);
  SERIALISE_MEMBER(supportedStencilResolveModes);
  SERIALISE_MEMBER(independentResolveNone);
  SERIALISE_MEMBER(independentResolve);
}

template <>
void Deserialise(const VkPhysicalDeviceDepthStencilResolvePropertiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassDescriptionDepthStencilResolveKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(depthResolveMode);
  SERIALISE_MEMBER(stencilResolveMode);
  SERIALISE_MEMBER_OPT(pDepthStencilResolveAttachment);
}

template <>
void Deserialise(const VkSubpassDescriptionDepthStencilResolveKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineRasterizationConservativeStateCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineRasterizationConservativeStateCreateFlagsEXT, flags);
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

  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, usage);
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
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDevicePointClippingProperties &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(pointClippingBehavior);
}

template <>
void Deserialise(const VkPhysicalDevicePointClippingProperties &el)
{
  DeserialiseNext(el.pNext);
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
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxVertexAttribDivisor);
}

template <>
void Deserialise(const VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
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
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(vertexAttributeInstanceRateDivisor);
  SERIALISE_MEMBER(vertexAttributeInstanceRateZeroDivisor);
}

template <>
void Deserialise(const VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDevice8BitStorageFeaturesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(storageBuffer8BitAccess);
  SERIALISE_MEMBER(uniformAndStorageBuffer8BitAccess);
  SERIALISE_MEMBER(storagePushConstant8);
}

template <>
void Deserialise(const VkPhysicalDevice8BitStorageFeaturesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDevice16BitStorageFeatures &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(storageBuffer16BitAccess);
  SERIALISE_MEMBER(uniformAndStorageBuffer16BitAccess);
  SERIALISE_MEMBER(storagePushConstant16);
  SERIALISE_MEMBER(storageInputOutput16);
}

template <>
void Deserialise(const VkPhysicalDevice16BitStorageFeatures &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(filterMinmaxSingleComponentFormats);
  SERIALISE_MEMBER(filterMinmaxImageComponentMapping);
}

template <>
void Deserialise(const VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
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
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMaintenance3Properties &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxPerSetDescriptors);
  SERIALISE_MEMBER(maxMemoryAllocationSize);
}

template <>
void Deserialise(const VkPhysicalDeviceMaintenance3Properties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutSupport &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(supported);
}

template <>
void Deserialise(const VkDescriptorSetLayoutSupport &el)
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
void DoSerialise(SerialiserType &ser, VkRenderPassFragmentDensityMapCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fragmentDensityMapAttachment);
}

template <>
void Deserialise(const VkRenderPassFragmentDensityMapCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSampleLocationEXT &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSampleLocationsInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(sampleLocationsPerPixel);
  SERIALISE_MEMBER(sampleLocationGridSize);
  SERIALISE_MEMBER(sampleLocationsCount);
  SERIALISE_MEMBER_ARRAY(pSampleLocations, sampleLocationsCount);
}

template <>
void Deserialise(const VkSampleLocationsInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pSampleLocations;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassSampleLocationsEXT &el)
{
  SERIALISE_MEMBER(subpassIndex);
  SERIALISE_MEMBER(sampleLocationsInfo);
}

template <>
void Deserialise(const VkSubpassSampleLocationsEXT &el)
{
  Deserialise(el.sampleLocationsInfo);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAttachmentSampleLocationsEXT &el)
{
  SERIALISE_MEMBER(attachmentIndex);
  SERIALISE_MEMBER(sampleLocationsInfo);
}

template <>
void Deserialise(const VkAttachmentSampleLocationsEXT &el)
{
  Deserialise(el.sampleLocationsInfo);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRenderPassSampleLocationsBeginInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(attachmentInitialSampleLocationsCount);
  SERIALISE_MEMBER_ARRAY(pAttachmentInitialSampleLocations, attachmentInitialSampleLocationsCount);
  SERIALISE_MEMBER(postSubpassSampleLocationsCount);
  SERIALISE_MEMBER_ARRAY(pPostSubpassSampleLocations, postSubpassSampleLocationsCount);
}

template <>
void Deserialise(const VkRenderPassSampleLocationsBeginInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  for(uint32_t j = 0;
      el.pAttachmentInitialSampleLocations && j < el.attachmentInitialSampleLocationsCount; j++)
    Deserialise(el.pAttachmentInitialSampleLocations[j]);
  delete[] el.pAttachmentInitialSampleLocations;
  for(uint32_t j = 0; el.pPostSubpassSampleLocations && j < el.postSubpassSampleLocationsCount; j++)
    Deserialise(el.pPostSubpassSampleLocations[j]);
  delete[] el.pPostSubpassSampleLocations;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceFragmentDensityMapFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fragmentDensityMap);
  SERIALISE_MEMBER(fragmentDensityMapDynamic);
  SERIALISE_MEMBER(fragmentDensityMapNonSubsampledImages);
}

template <>
void Deserialise(const VkPhysicalDeviceFragmentDensityMapFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceHostQueryResetFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(hostQueryReset);
}

template <>
void Deserialise(const VkPhysicalDeviceHostQueryResetFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceFragmentDensityMapPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(minFragmentDensityTexelSize);
  SERIALISE_MEMBER(maxFragmentDensityTexelSize);
  SERIALISE_MEMBER(fragmentDensityInvocations);
}

template <>
void Deserialise(const VkPhysicalDeviceFragmentDensityMapPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMultiviewFeatures &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(multiview);
  SERIALISE_MEMBER(multiviewGeometryShader);
  SERIALISE_MEMBER(multiviewTessellationShader);
}

template <>
void Deserialise(const VkPhysicalDeviceMultiviewFeatures &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMultiviewProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxMultiviewViewCount);
  SERIALISE_MEMBER(maxMultiviewInstanceIndex);
}

template <>
void Deserialise(const VkPhysicalDeviceMultiviewProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDevicePushDescriptorPropertiesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxPushDescriptors);
}

template <>
void Deserialise(const VkPhysicalDevicePushDescriptorPropertiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSurfaceInfo2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  // don't need the surface
  SERIALISE_MEMBER_EMPTY(surface);
}

template <>
void Deserialise(const VkPhysicalDeviceSurfaceInfo2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceCapabilitiesKHR &el)
{
  SERIALISE_MEMBER(minImageCount);
  SERIALISE_MEMBER(maxImageCount);
  SERIALISE_MEMBER(currentExtent);
  SERIALISE_MEMBER(minImageExtent);
  SERIALISE_MEMBER(maxImageExtent);
  SERIALISE_MEMBER(maxImageArrayLayers);
  SERIALISE_MEMBER_VKFLAGS(VkSurfaceTransformFlagsKHR, supportedTransforms);
  SERIALISE_MEMBER(currentTransform);
  SERIALISE_MEMBER_VKFLAGS(VkCompositeAlphaFlagsKHR, supportedCompositeAlpha);
  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, supportedUsageFlags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceCapabilities2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(surfaceCapabilities);
}

template <>
void Deserialise(const VkSurfaceCapabilities2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceFormatKHR &el)
{
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(colorSpace);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceFormat2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(surfaceFormat);
}

template <>
void Deserialise(const VkSurfaceFormat2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayNativeHdrSurfaceCapabilitiesAMD &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(localDimmingSupport);
}

template <>
void Deserialise(const VkDisplayNativeHdrSurfaceCapabilitiesAMD &el)
{
  DeserialiseNext(el.pNext);
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
void DoSerialise(SerialiserType &ser, VkRectLayerKHR &el)
{
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER(extent);
  SERIALISE_MEMBER(layer);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPresentRegionKHR &el)
{
  SERIALISE_MEMBER(rectangleCount);
  SERIALISE_MEMBER_ARRAY(pRectangles, rectangleCount);
}

template <>
void Deserialise(const VkPresentRegionKHR &el)
{
  delete[] el.pRectangles;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPresentRegionsKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(swapchainCount);
  SERIALISE_MEMBER_ARRAY(pRegions, swapchainCount);
}

template <>
void Deserialise(const VkPresentRegionsKHR &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pRegions;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceShaderCorePropertiesAMD &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(shaderEngineCount);
  SERIALISE_MEMBER(shaderArraysPerEngineCount);
  SERIALISE_MEMBER(computeUnitsPerShaderArray);
  SERIALISE_MEMBER(simdPerComputeUnit);
  SERIALISE_MEMBER(wavefrontsPerSimd);
  SERIALISE_MEMBER(wavefrontSize);
  SERIALISE_MEMBER(sgprsPerSimd);
  SERIALISE_MEMBER(minSgprAllocation);
  SERIALISE_MEMBER(maxSgprAllocation);
  SERIALISE_MEMBER(sgprAllocationGranularity);
  SERIALISE_MEMBER(vgprsPerSimd);
  SERIALISE_MEMBER(minVgprAllocation);
  SERIALISE_MEMBER(maxVgprAllocation);
  SERIALISE_MEMBER(vgprAllocationGranularity);
}

template <>
void Deserialise(const VkPhysicalDeviceShaderCorePropertiesAMD &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkTextureLODGatherFormatPropertiesAMD &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(supportsTextureGatherLODBiasAMD);
}

template <>
void Deserialise(const VkTextureLODGatherFormatPropertiesAMD &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageViewASTCDecodeModeEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(decodeMode);
}

template <>
void Deserialise(const VkImageViewASTCDecodeModeEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceASTCDecodeFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(decodeModeSharedExponent);
}

template <>
void Deserialise(const VkPhysicalDeviceASTCDecodeFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferDeviceAddressInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(buffer);
}

template <>
void Deserialise(const VkBufferDeviceAddressInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferDeviceAddressCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceAddress);
}

template <>
void Deserialise(const VkBufferDeviceAddressCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceBufferDeviceAddressFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(bufferDeviceAddress);
  SERIALISE_MEMBER(bufferDeviceAddressCaptureReplay);
  SERIALISE_MEMBER(bufferDeviceAddressMultiDevice);
}

template <>
void Deserialise(const VkPhysicalDeviceBufferDeviceAddressFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkValidationCacheCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkValidationCacheCreateFlagsEXT, flags);

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t initialDataSize = (uint64_t)el.initialDataSize;
    ser.Serialise("initialDataSize"_lit, initialDataSize);
    if(ser.IsReading())
      el.initialDataSize = (size_t)initialDataSize;
  }

  // don't serialise the data
  // SERIALISE_MEMBER_ARRAY(pInitialData, el.initialDataSize);
}

template <>
void Deserialise(const VkValidationCacheCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  // FreeAlignedBuffer((byte *)el.pInitialData);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkShaderModuleValidationCacheCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  // we skip this, because it's unwrapped and has no ResourceId. The presence of this struct is
  // enough
  // SERIALISE_MEMBER(validationCache);
}

template <>
void Deserialise(const VkShaderModuleValidationCacheCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkValidationFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(enabledValidationFeatureCount);
  SERIALISE_MEMBER_ARRAY(pEnabledValidationFeatures, enabledValidationFeatureCount);
  SERIALISE_MEMBER(disabledValidationFeatureCount);
  SERIALISE_MEMBER_ARRAY(pDisabledValidationFeatures, disabledValidationFeatureCount);
}

template <>
void Deserialise(const VkValidationFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pEnabledValidationFeatures;
  delete[] el.pDisabledValidationFeatures;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkValidationFlagsEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(disabledValidationCheckCount);
  SERIALISE_MEMBER_ARRAY(pDisabledValidationChecks, disabledValidationCheckCount);
}

template <>
void Deserialise(const VkValidationFlagsEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pDisabledValidationChecks;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSampleLocationsPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(sampleLocationSampleCounts);
  SERIALISE_MEMBER(maxSampleLocationGridSize);
  SERIALISE_MEMBER(sampleLocationCoordinateRange);
  SERIALISE_MEMBER(sampleLocationSubPixelBits);
  SERIALISE_MEMBER(variableSampleLocations);
}

template <>
void Deserialise(const VkPhysicalDeviceSampleLocationsPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMultisamplePropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxSampleLocationGridSize);
}

template <>
void Deserialise(const VkMultisamplePropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageStencilUsageCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(stencilUsage);
}

template <>
void Deserialise(const VkImageStencilUsageCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceTransformFeedbackFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(transformFeedback);
  SERIALISE_MEMBER(geometryStreams);
}

template <>
void Deserialise(const VkPhysicalDeviceTransformFeedbackFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceTransformFeedbackPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxTransformFeedbackStreams);
  SERIALISE_MEMBER(maxTransformFeedbackBuffers);
  SERIALISE_MEMBER(maxTransformFeedbackBufferSize);
  SERIALISE_MEMBER(maxTransformFeedbackStreamDataSize);
  SERIALISE_MEMBER(maxTransformFeedbackBufferDataSize);
  SERIALISE_MEMBER(maxTransformFeedbackBufferDataStride);
  SERIALISE_MEMBER(transformFeedbackQueries);
  SERIALISE_MEMBER(transformFeedbackStreamsLinesTriangles);
  SERIALISE_MEMBER(transformFeedbackRasterizationStreamSelect);
  SERIALISE_MEMBER(transformFeedbackDraw);
}

template <>
void Deserialise(const VkPhysicalDeviceTransformFeedbackPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineSampleLocationsStateCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(sampleLocationsEnable);
  SERIALISE_MEMBER(sampleLocationsInfo);
}

template <>
void Deserialise(const VkPipelineSampleLocationsStateCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineRasterizationStateStreamCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineRasterizationStateStreamCreateFlagsEXT, flags);
  SERIALISE_MEMBER(rasterizationStream);
}

template <>
void Deserialise(const VkPipelineRasterizationStateStreamCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAttachmentDescription2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkAttachmentDescriptionFlags, flags);
  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(samples);
  SERIALISE_MEMBER(loadOp);
  SERIALISE_MEMBER(storeOp);
  SERIALISE_MEMBER(stencilLoadOp);
  SERIALISE_MEMBER(stencilStoreOp);
  SERIALISE_MEMBER(initialLayout);
  SERIALISE_MEMBER(finalLayout);
}

template <>
void Deserialise(const VkAttachmentDescription2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAttachmentReference2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(attachment);
  SERIALISE_MEMBER(layout);
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
}

template <>
void Deserialise(const VkAttachmentReference2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassDescription2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkSubpassDescriptionFlags, flags);
  SERIALISE_MEMBER(pipelineBindPoint);
  SERIALISE_MEMBER(viewMask);

  SERIALISE_MEMBER(inputAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pInputAttachments, inputAttachmentCount);

  SERIALISE_MEMBER(colorAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pColorAttachments, colorAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pResolveAttachments, colorAttachmentCount);

  SERIALISE_MEMBER_OPT(pDepthStencilAttachment);

  SERIALISE_MEMBER(preserveAttachmentCount);
  SERIALISE_MEMBER_ARRAY(pPreserveAttachments, preserveAttachmentCount);
}

template <>
void Deserialise(const VkSubpassDescription2KHR &el)
{
  DeserialiseNext(el.pNext);

  if(el.pDepthStencilAttachment)
    Deserialise(*el.pDepthStencilAttachment);
  delete el.pDepthStencilAttachment;

  for(uint32_t j = 0; el.pColorAttachments && j < el.colorAttachmentCount; j++)
  {
    Deserialise(el.pColorAttachments[j]);
    if(el.pResolveAttachments)
      Deserialise(el.pResolveAttachments[j]);
  }
  delete[] el.pColorAttachments;
  delete[] el.pResolveAttachments;

  for(uint32_t j = 0; el.pInputAttachments && j < el.inputAttachmentCount; j++)
    Deserialise(el.pInputAttachments[j]);

  delete[] el.pInputAttachments;
  delete[] el.pPreserveAttachments;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassDependency2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(srcSubpass);
  SERIALISE_MEMBER(dstSubpass);
  SERIALISE_MEMBER_VKFLAGS(VkPipelineStageFlags, srcStageMask);
  SERIALISE_MEMBER_VKFLAGS(VkPipelineStageFlags, dstStageMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, srcAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkAccessFlags, dstAccessMask);
  SERIALISE_MEMBER_VKFLAGS(VkDependencyFlags, dependencyFlags);
  SERIALISE_MEMBER(viewOffset);
}

template <>
void Deserialise(const VkSubpassDependency2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkRenderPassCreateInfo2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkRenderPassCreateFlags, flags);
  SERIALISE_MEMBER(attachmentCount);
  SERIALISE_MEMBER_ARRAY(pAttachments, attachmentCount);
  SERIALISE_MEMBER(subpassCount);
  SERIALISE_MEMBER_ARRAY(pSubpasses, subpassCount);
  SERIALISE_MEMBER(dependencyCount);
  SERIALISE_MEMBER_ARRAY(pDependencies, dependencyCount);
  SERIALISE_MEMBER(correlatedViewMaskCount);
  SERIALISE_MEMBER_ARRAY(pCorrelatedViewMasks, correlatedViewMaskCount);
}

template <>
void Deserialise(const VkRenderPassCreateInfo2KHR &el)
{
  DeserialiseNext(el.pNext);

  for(uint32_t i = 0; el.pAttachments && i < el.attachmentCount; i++)
    Deserialise(el.pAttachments[i]);
  delete[] el.pAttachments;

  for(uint32_t i = 0; el.pSubpasses && i < el.subpassCount; i++)
    Deserialise(el.pSubpasses[i]);
  delete[] el.pSubpasses;

  for(uint32_t i = 0; el.pDependencies && i < el.dependencyCount; i++)
    Deserialise(el.pDependencies[i]);
  delete[] el.pDependencies;

  delete[] el.pCorrelatedViewMasks;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassBeginInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(contents);
}

template <>
void Deserialise(const VkSubpassBeginInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSubpassEndInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SUBPASS_END_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);
}

template <>
void Deserialise(const VkSubpassEndInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDispatchIndirectCommand &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(z);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDrawIndirectCommand &el)
{
  SERIALISE_MEMBER(vertexCount);
  SERIALISE_MEMBER(instanceCount);
  SERIALISE_MEMBER(firstVertex);
  SERIALISE_MEMBER(firstInstance);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDrawIndexedIndirectCommand &el)
{
  SERIALISE_MEMBER(indexCount);
  SERIALISE_MEMBER(instanceCount);
  SERIALISE_MEMBER(firstIndex);
  SERIALISE_MEMBER(vertexOffset);
  SERIALISE_MEMBER(firstInstance);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceQueueInfo2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkDeviceQueueCreateFlags, flags);
  SERIALISE_MEMBER(queueFamilyIndex);
  SERIALISE_MEMBER(queueIndex);
}

template <>
void Deserialise(const VkDeviceQueueInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceProtectedMemoryFeatures &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(protectedMemory);
}

template <>
void Deserialise(const VkPhysicalDeviceProtectedMemoryFeatures &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceProtectedMemoryProperties &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(protectedNoFault);
}

template <>
void Deserialise(const VkPhysicalDeviceProtectedMemoryProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceShaderDrawParametersFeatures &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(shaderDrawParameters);
}

template <>
void Deserialise(const VkPhysicalDeviceShaderDrawParametersFeatures &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportMemoryAllocateInfoNV &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlagsNV, handleTypes);
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

  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlagsNV, handleTypes);
}

template <>
void Deserialise(const VkExternalMemoryImageCreateInfoNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceShaderImageFootprintFeaturesNV &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(imageFootprint);
}

template <>
void Deserialise(const VkPhysicalDeviceShaderImageFootprintFeaturesNV &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalMemoryImageCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlags, handleTypes);
}

template <>
void Deserialise(const VkExternalMemoryImageCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceExternalImageFormatInfo &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkPhysicalDeviceExternalImageFormatInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalMemoryProperties &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryFeatureFlags, externalMemoryFeatures);
  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlags, exportFromImportedHandleTypes);
  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlags, compatibleHandleTypes);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalImageFormatProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(externalMemoryProperties);
}

template <>
void Deserialise(const VkExternalImageFormatProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceExternalBufferInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkBufferCreateFlags, flags);
  SERIALISE_MEMBER_VKFLAGS(VkBufferUsageFlags, usage);
  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkPhysicalDeviceExternalBufferInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalBufferProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(externalMemoryProperties);
}

template <>
void Deserialise(const VkExternalBufferProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceIDProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceUUID);
  SERIALISE_MEMBER(driverUUID);
  SERIALISE_MEMBER(deviceLUID);
  SERIALISE_MEMBER(deviceNodeMask);
  SERIALISE_MEMBER(deviceLUIDValid);
}

template <>
void Deserialise(const VkPhysicalDeviceIDProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportMemoryAllocateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlags, handleTypes);
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

  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlags, handleTypes);
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
  SERIALISE_MEMBER_TYPED(int32_t, fd);
}

template <>
void Deserialise(const VkImportMemoryFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryFdPropertiesKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memoryTypeBits);
}

template <>
void Deserialise(const VkMemoryFdPropertiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryGetFdInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkMemoryGetFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceExternalSemaphoreInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkPhysicalDeviceExternalSemaphoreInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalSemaphoreProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalSemaphoreHandleTypeFlags, exportFromImportedHandleTypes);
  SERIALISE_MEMBER_VKFLAGS(VkExternalSemaphoreHandleTypeFlags, compatibleHandleTypes);
  SERIALISE_MEMBER_VKFLAGS(VkExternalSemaphoreFeatureFlags, externalSemaphoreFeatures);
}

template <>
void Deserialise(const VkExternalSemaphoreProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportSemaphoreCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalSemaphoreHandleTypeFlags, handleTypes);
}

template <>
void Deserialise(const VkExportSemaphoreCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportSemaphoreFdInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(semaphore);
  SERIALISE_MEMBER_VKFLAGS(VkSemaphoreImportFlags, flags);
  SERIALISE_MEMBER_VKFLAGS(VkExternalSemaphoreHandleTypeFlags, handleType);
  SERIALISE_MEMBER_TYPED(int32_t, fd);
}

template <>
void Deserialise(const VkImportSemaphoreFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSemaphoreGetFdInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(semaphore);
  SERIALISE_MEMBER_VKFLAGS(VkExternalSemaphoreHandleTypeFlags, handleType);
}

template <>
void Deserialise(const VkSemaphoreGetFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPresentInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(srcRect);
  SERIALISE_MEMBER(dstRect);
  SERIALISE_MEMBER(persistent);
}

template <>
void Deserialise(const VkDisplayPresentInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkConformanceVersionKHR &el)
{
  SERIALISE_MEMBER(major);
  SERIALISE_MEMBER(minor);
  SERIALISE_MEMBER(subminor);
  SERIALISE_MEMBER(patch);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceDriverPropertiesKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(driverID);
  SERIALISE_MEMBER(driverName);
  SERIALISE_MEMBER(driverInfo);
  SERIALISE_MEMBER(conformanceVersion);
}

template <>
void Deserialise(const VkPhysicalDeviceDriverPropertiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceExternalFenceInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkPhysicalDeviceExternalFenceInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExternalFenceProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalFenceHandleTypeFlags, exportFromImportedHandleTypes);
  SERIALISE_MEMBER_VKFLAGS(VkExternalFenceHandleTypeFlags, compatibleHandleTypes);
  SERIALISE_MEMBER_VKFLAGS(VkExternalFenceFeatureFlags, externalFenceFeatures);
}

template <>
void Deserialise(const VkExternalFenceProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportFenceCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalFenceHandleTypeFlags, handleTypes);
}

template <>
void Deserialise(const VkExportFenceCreateInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportFenceFdInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fence);
  SERIALISE_MEMBER_VKFLAGS(VkFenceImportFlags, flags);
  SERIALISE_MEMBER_VKFLAGS(VkExternalFenceHandleTypeFlags, handleType);
  SERIALISE_MEMBER_TYPED(int32_t, fd);
}

template <>
void Deserialise(const VkImportFenceFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFenceGetFdInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkFenceGetFdInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetLayoutBindingFlagsCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(bindingCount);
  SERIALISE_MEMBER_ARRAY_VKFLAGS(VkDescriptorBindingFlagsEXT, pBindingFlags, bindingCount);
}

template <>
void Deserialise(const VkDescriptorSetLayoutBindingFlagsCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pBindingFlags;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceDescriptorIndexingFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(shaderInputAttachmentArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderUniformTexelBufferArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderStorageTexelBufferArrayDynamicIndexing);
  SERIALISE_MEMBER(shaderUniformBufferArrayNonUniformIndexing);
  SERIALISE_MEMBER(shaderSampledImageArrayNonUniformIndexing);
  SERIALISE_MEMBER(shaderStorageBufferArrayNonUniformIndexing);
  SERIALISE_MEMBER(shaderStorageImageArrayNonUniformIndexing);
  SERIALISE_MEMBER(shaderInputAttachmentArrayNonUniformIndexing);
  SERIALISE_MEMBER(shaderUniformTexelBufferArrayNonUniformIndexing);
  SERIALISE_MEMBER(shaderStorageTexelBufferArrayNonUniformIndexing);
  SERIALISE_MEMBER(descriptorBindingUniformBufferUpdateAfterBind);
  SERIALISE_MEMBER(descriptorBindingSampledImageUpdateAfterBind);
  SERIALISE_MEMBER(descriptorBindingStorageImageUpdateAfterBind);
  SERIALISE_MEMBER(descriptorBindingStorageBufferUpdateAfterBind);
  SERIALISE_MEMBER(descriptorBindingUniformTexelBufferUpdateAfterBind);
  SERIALISE_MEMBER(descriptorBindingStorageTexelBufferUpdateAfterBind);
  SERIALISE_MEMBER(descriptorBindingUpdateUnusedWhilePending);
  SERIALISE_MEMBER(descriptorBindingPartiallyBound);
  SERIALISE_MEMBER(descriptorBindingVariableDescriptorCount);
  SERIALISE_MEMBER(runtimeDescriptorArray);
}

template <>
void Deserialise(const VkPhysicalDeviceDescriptorIndexingFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceDescriptorIndexingPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxUpdateAfterBindDescriptorsInAllPools);
  SERIALISE_MEMBER(shaderUniformBufferArrayNonUniformIndexingNative);
  SERIALISE_MEMBER(shaderSampledImageArrayNonUniformIndexingNative);
  SERIALISE_MEMBER(shaderStorageBufferArrayNonUniformIndexingNative);
  SERIALISE_MEMBER(shaderStorageImageArrayNonUniformIndexingNative);
  SERIALISE_MEMBER(shaderInputAttachmentArrayNonUniformIndexingNative);
  SERIALISE_MEMBER(robustBufferAccessUpdateAfterBind);
  SERIALISE_MEMBER(quadDivergentImplicitLod);
  SERIALISE_MEMBER(maxPerStageDescriptorUpdateAfterBindSamplers);
  SERIALISE_MEMBER(maxPerStageDescriptorUpdateAfterBindUniformBuffers);
  SERIALISE_MEMBER(maxPerStageDescriptorUpdateAfterBindStorageBuffers);
  SERIALISE_MEMBER(maxPerStageDescriptorUpdateAfterBindSampledImages);
  SERIALISE_MEMBER(maxPerStageDescriptorUpdateAfterBindStorageImages);
  SERIALISE_MEMBER(maxPerStageDescriptorUpdateAfterBindInputAttachments);
  SERIALISE_MEMBER(maxPerStageUpdateAfterBindResources);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindSamplers);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindUniformBuffers);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindStorageBuffers);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindSampledImages);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindStorageImages);
  SERIALISE_MEMBER(maxDescriptorSetUpdateAfterBindInputAttachments);
}

template <>
void Deserialise(const VkPhysicalDeviceDescriptorIndexingPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetVariableDescriptorCountAllocateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(descriptorSetCount);
  SERIALISE_MEMBER_ARRAY(pDescriptorCounts, descriptorSetCount);
}

template <>
void Deserialise(const VkDescriptorSetVariableDescriptorCountAllocateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pDescriptorCounts;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDescriptorSetVariableDescriptorCountLayoutSupportEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxVariableDescriptorCount);
}

template <>
void Deserialise(const VkDescriptorSetVariableDescriptorCountLayoutSupportEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceDiscardRectanglePropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(maxDiscardRectangles);
}

template <>
void Deserialise(const VkPhysicalDeviceDiscardRectanglePropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineDiscardRectangleStateCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineDiscardRectangleStateCreateFlagsEXT, flags);

  SERIALISE_MEMBER(discardRectangleMode);
  SERIALISE_MEMBER(discardRectangleCount);
  SERIALISE_MEMBER_ARRAY(pDiscardRectangles, discardRectangleCount);
}

template <>
void Deserialise(const VkPipelineDiscardRectangleStateCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pDiscardRectangles;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceDepthClipEnableFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(depthClipEnable);
}

template <>
void Deserialise(const VkPhysicalDeviceDepthClipEnableFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineRasterizationDepthClipStateCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkPipelineRasterizationDepthClipStateCreateFlagsEXT, flags);

  SERIALISE_MEMBER(depthClipEnable);
}

template <>
void Deserialise(const VkPipelineRasterizationDepthClipStateCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineCreationFeedbackEXT &el)
{
  SERIALISE_MEMBER(flags);
  SERIALISE_MEMBER(duration);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPipelineCreationFeedbackCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_OPT(pPipelineCreationFeedback);
  SERIALISE_MEMBER(pipelineStageCreationFeedbackCount);
  SERIALISE_MEMBER_ARRAY(pPipelineStageCreationFeedbacks, pipelineStageCreationFeedbackCount);
}

template <>
void Deserialise(const VkPipelineCreationFeedbackCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
  delete el.pPipelineCreationFeedback;
  delete[] el.pPipelineStageCreationFeedbacks;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPowerInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(powerState);
}

template <>
void Deserialise(const VkDisplayPowerInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceEventInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(deviceEvent);
}

template <>
void Deserialise(const VkDeviceEventInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayEventInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(displayEvent);
}

template <>
void Deserialise(const VkDisplayEventInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCalibratedTimestampInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(timeDomain);
}

template <>
void Deserialise(const VkCalibratedTimestampInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSwapchainCounterCreateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkSurfaceCounterFlagsEXT, surfaceCounters);
}

template <>
void Deserialise(const VkSwapchainCounterCreateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceCapabilities2EXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(minImageCount);
  SERIALISE_MEMBER(maxImageCount);
  SERIALISE_MEMBER(currentExtent);
  SERIALISE_MEMBER(minImageExtent);
  SERIALISE_MEMBER(maxImageExtent);
  SERIALISE_MEMBER(maxImageArrayLayers);
  SERIALISE_MEMBER_VKFLAGS(VkSurfaceTransformFlagsKHR, supportedTransforms);
  SERIALISE_MEMBER(currentTransform);
  SERIALISE_MEMBER_VKFLAGS(VkCompositeAlphaFlagsKHR, supportedCompositeAlpha);
  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, supportedUsageFlags);
  SERIALISE_MEMBER_VKFLAGS(VkSurfaceCounterFlagsEXT, supportedSurfaceCounters);
}

template <>
void Deserialise(const VkSurfaceCapabilities2EXT &el)
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
void DoSerialise(SerialiserType &ser, VkMemoryDedicatedRequirements &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(prefersDedicatedAllocation);
  SERIALISE_MEMBER(requiresDedicatedAllocation);
}

template <>
void Deserialise(const VkMemoryDedicatedRequirements &el)
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
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMemoryBudgetPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(heapBudget);
  SERIALISE_MEMBER(heapUsage);
}

template <>
void Deserialise(const VkPhysicalDeviceMemoryBudgetPropertiesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMemoryPriorityFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memoryPriority);
}

template <>
void Deserialise(const VkPhysicalDeviceMemoryPriorityFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryPriorityAllocateInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(priority);
}

template <>
void Deserialise(const VkMemoryPriorityAllocateInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDevicePCIBusInfoPropertiesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(pciDomain);
  SERIALISE_MEMBER(pciBus);
  SERIALISE_MEMBER(pciDevice);
  SERIALISE_MEMBER(pciFunction);
}

template <>
void Deserialise(const VkPhysicalDevicePCIBusInfoPropertiesEXT &el)
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
void DoSerialise(SerialiserType &ser, VkImagePlaneMemoryRequirementsInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(planeAspect);
}

template <>
void Deserialise(const VkImagePlaneMemoryRequirementsInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSamplerYcbcrConversionFeatures &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(samplerYcbcrConversion);
}

template <>
void Deserialise(const VkPhysicalDeviceSamplerYcbcrConversionFeatures &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSamplerYcbcrConversionImageFormatProperties &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(combinedImageSamplerDescriptorCount);
}

template <>
void Deserialise(const VkSamplerYcbcrConversionImageFormatProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceShaderAtomicInt64FeaturesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(shaderBufferInt64Atomics);
  SERIALISE_MEMBER(shaderSharedInt64Atomics);
}

template <>
void Deserialise(const VkPhysicalDeviceShaderAtomicInt64FeaturesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceScalarBlockLayoutFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(scalarBlockLayout);
}

template <>
void Deserialise(const VkPhysicalDeviceScalarBlockLayoutFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceFloat16Int8FeaturesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(shaderFloat16);
  SERIALISE_MEMBER(shaderInt8);
}

template <>
void Deserialise(const VkPhysicalDeviceFloat16Int8FeaturesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceFloatControlsPropertiesKHR &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(separateDenormSettings);
  SERIALISE_MEMBER(separateRoundingModeSettings);
  SERIALISE_MEMBER(shaderSignedZeroInfNanPreserveFloat16);
  SERIALISE_MEMBER(shaderSignedZeroInfNanPreserveFloat32);
  SERIALISE_MEMBER(shaderSignedZeroInfNanPreserveFloat64);
  SERIALISE_MEMBER(shaderDenormPreserveFloat16);
  SERIALISE_MEMBER(shaderDenormPreserveFloat32);
  SERIALISE_MEMBER(shaderDenormPreserveFloat64);
  SERIALISE_MEMBER(shaderDenormFlushToZeroFloat16);
  SERIALISE_MEMBER(shaderDenormFlushToZeroFloat32);
  SERIALISE_MEMBER(shaderDenormFlushToZeroFloat64);
  SERIALISE_MEMBER(shaderRoundingModeRTEFloat16);
  SERIALISE_MEMBER(shaderRoundingModeRTEFloat32);
  SERIALISE_MEMBER(shaderRoundingModeRTEFloat64);
  SERIALISE_MEMBER(shaderRoundingModeRTZFloat16);
  SERIALISE_MEMBER(shaderRoundingModeRTZFloat32);
  SERIALISE_MEMBER(shaderRoundingModeRTZFloat64);
}

template <>
void Deserialise(const VkPhysicalDeviceFloatControlsPropertiesKHR &el)
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

  SERIALISE_MEMBER_VKFLAGS(VkDeviceGroupPresentModeFlagsKHR, modes);
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
void DoSerialise(SerialiserType &ser, VkDeviceGroupPresentCapabilitiesKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(presentMask);
  SERIALISE_MEMBER_VKFLAGS(VkDeviceGroupPresentModeFlagsKHR, modes);
}

template <>
void Deserialise(const VkDeviceGroupPresentCapabilitiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupPresentInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(swapchainCount);
  SERIALISE_MEMBER_ARRAY(pDeviceMasks, swapchainCount);
  SERIALISE_MEMBER_VKFLAGS(VkDeviceGroupPresentModeFlagsKHR, mode);
}

template <>
void Deserialise(const VkDeviceGroupPresentInfoKHR &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pDeviceMasks;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceGroupProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  if(ser.IsReading())
  {
    for(uint32_t i = 0; i < VK_MAX_DEVICE_GROUP_SIZE; i++)
      el.physicalDevices[i] = VK_NULL_HANDLE;
  }

  SERIALISE_MEMBER(physicalDeviceCount);
  // manual call to Serialise to ensure we only serialise the number of devices, but also don't
  // allocate memory
  VkPhysicalDevice *devs = el.physicalDevices;
  ser.Serialise("physicalDevices"_lit, devs, (uint64_t)el.physicalDeviceCount,
                SerialiserFlags::NoFlags);
  SERIALISE_MEMBER(subsetAllocation);
}

template <>
void Deserialise(const VkPhysicalDeviceGroupProperties &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDeviceGroupDeviceCreateInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(physicalDeviceCount);
  SERIALISE_MEMBER_ARRAY(pPhysicalDevices, physicalDeviceCount);
}

template <>
void Deserialise(const VkDeviceGroupDeviceCreateInfo &el)
{
  DeserialiseNext(el.pNext);

  delete[] el.pPhysicalDevices;
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
void DoSerialise(SerialiserType &ser, VkHdrMetadataEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_HDR_METADATA_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(displayPrimaryRed);
  SERIALISE_MEMBER(displayPrimaryGreen);
  SERIALISE_MEMBER(displayPrimaryBlue);
  SERIALISE_MEMBER(whitePoint);
  SERIALISE_MEMBER(maxLuminance);
  SERIALISE_MEMBER(minLuminance);
  SERIALISE_MEMBER(maxContentLightLevel);
  SERIALISE_MEMBER(maxFrameAverageLightLevel);
}

template <>
void Deserialise(const VkHdrMetadataEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkXYColorEXT &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
}

template <>
void Deserialise(const VkXYColorEXT &el)
{
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryAllocateFlagsInfo &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkMemoryAllocateFlags, flags);
  SERIALISE_MEMBER(deviceMask);
}

template <>
void Deserialise(const VkMemoryAllocateFlagsInfo &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSubgroupProperties &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(subgroupSize);
  SERIALISE_MEMBER_VKFLAGS(VkShaderStageFlags, supportedStages);
  SERIALISE_MEMBER_VKFLAGS(VkSubgroupFeatureFlags, supportedOperations);
  SERIALISE_MEMBER(quadOperationsInAllStages);
}

template <>
void Deserialise(const VkPhysicalDeviceSubgroupProperties &el)
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

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPlaneInfo2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  // don't serialise unwrapped VkDisplayModeKHR
  // SERIALISE_MEMBER(mode);
  SERIALISE_MEMBER(planeIndex);
}

template <>
void Deserialise(const VkDisplayPlaneInfo2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPlaneCapabilitiesKHR &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkDisplayPlaneAlphaFlagsKHR, supportedAlpha);
  SERIALISE_MEMBER(minSrcPosition);
  SERIALISE_MEMBER(maxSrcPosition);
  SERIALISE_MEMBER(minSrcExtent);
  SERIALISE_MEMBER(maxSrcExtent);
  SERIALISE_MEMBER(minDstPosition);
  SERIALISE_MEMBER(maxDstPosition);
  SERIALISE_MEMBER(minDstExtent);
  SERIALISE_MEMBER(maxDstExtent);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPlaneCapabilities2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(capabilities);
}

template <>
void Deserialise(const VkDisplayPlaneCapabilities2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPropertiesKHR &el)
{
  // don't serialise unwrapped VkDisplayKHR
  // SERIALISE_MEMBER(display);
  SERIALISE_MEMBER(displayName);
  SERIALISE_MEMBER(physicalDimensions);
  SERIALISE_MEMBER(physicalResolution);
  SERIALISE_MEMBER_VKFLAGS(VkSurfaceTransformFlagsKHR, supportedTransforms);
  SERIALISE_MEMBER(planeReorderPossible);
  SERIALISE_MEMBER(persistentContent);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayProperties2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(displayProperties);
}

template <>
void Deserialise(const VkDisplayProperties2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPlanePropertiesKHR &el)
{
  // don't serialise unwrapped VkDisplayKHR
  // SERIALISE_MEMBER(currentDisplay);
  SERIALISE_MEMBER(currentStackIndex);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayPlaneProperties2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_PLANE_PROPERTIES_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(displayPlaneProperties);
}

template <>
void Deserialise(const VkDisplayPlaneProperties2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayModeParametersKHR &el)
{
  SERIALISE_MEMBER(visibleRegion);
  SERIALISE_MEMBER(refreshRate);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayModePropertiesKHR &el)
{
  // don't serialise unwrapped VkDisplayModeKHR
  // SERIALISE_MEMBER(displayMode);
  SERIALISE_MEMBER(parameters);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkDisplayModeProperties2KHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(displayModeProperties);
}

template <>
void Deserialise(const VkDisplayModeProperties2KHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkBufferMemoryRequirementsInfo2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(buffer);
}

template <>
void Deserialise(const VkBufferMemoryRequirementsInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageMemoryRequirementsInfo2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(image);
}

template <>
void Deserialise(const VkImageMemoryRequirementsInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageSparseMemoryRequirementsInfo2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(image);
}

template <>
void Deserialise(const VkImageSparseMemoryRequirementsInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryRequirements2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memoryRequirements);
}

template <>
void Deserialise(const VkMemoryRequirements2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageMemoryRequirements &el)
{
  SERIALISE_MEMBER(formatProperties);
  SERIALISE_MEMBER(imageMipTailFirstLod);
  SERIALISE_MEMBER(imageMipTailSize);
  SERIALISE_MEMBER(imageMipTailOffset);
  SERIALISE_MEMBER(imageMipTailStride);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageMemoryRequirements2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memoryRequirements);
}

template <>
void Deserialise(const VkSparseImageMemoryRequirements2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceFeatures2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(features);
}

template <>
void Deserialise(const VkPhysicalDeviceFeatures2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceProperties2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(properties);
}

template <>
void Deserialise(const VkPhysicalDeviceProperties2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFormatProperties &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkFormatFeatureFlags, linearTilingFeatures);
  SERIALISE_MEMBER_VKFLAGS(VkFormatFeatureFlags, optimalTilingFeatures);
  SERIALISE_MEMBER_VKFLAGS(VkFormatFeatureFlags, bufferFeatures);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFormatProperties2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(formatProperties);
}

template <>
void Deserialise(const VkFormatProperties2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageFormatProperties &el)
{
  SERIALISE_MEMBER(maxExtent);
  SERIALISE_MEMBER(maxMipLevels);
  SERIALISE_MEMBER(maxArrayLayers);
  SERIALISE_MEMBER_VKFLAGS(VkSampleCountFlags, sampleCounts);
  SERIALISE_MEMBER(maxResourceSize);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImageFormatProperties2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(imageFormatProperties);
}

template <>
void Deserialise(const VkImageFormatProperties2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceImageFormatInfo2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(tiling);
  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, usage);
  SERIALISE_MEMBER_VKFLAGS(VkImageCreateFlags, flags);
}

template <>
void Deserialise(const VkPhysicalDeviceImageFormatInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkQueueFamilyProperties2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(queueFamilyProperties);
}

template <>
void Deserialise(const VkQueueFamilyProperties2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceMemoryProperties2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memoryProperties);
}

template <>
void Deserialise(const VkPhysicalDeviceMemoryProperties2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageFormatProperties &el)
{
  SERIALISE_MEMBER_VKFLAGS(VkImageAspectFlags, aspectMask);
  SERIALISE_MEMBER(imageGranularity);
  SERIALISE_MEMBER_VKFLAGS(VkSparseImageFormatFlags, flags);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSparseImageFormatProperties2 &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(properties);
}

template <>
void Deserialise(const VkSparseImageFormatProperties2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceSparseImageFormatInfo2 &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(format);
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER(samples);
  SERIALISE_MEMBER_VKFLAGS(VkImageUsageFlags, usage);
  SERIALISE_MEMBER(tiling);
}

template <>
void Deserialise(const VkPhysicalDeviceSparseImageFormatInfo2 &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkCommandBufferInheritanceConditionalRenderingInfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(conditionalRenderingEnable);
}

template <>
void Deserialise(const VkCommandBufferInheritanceConditionalRenderingInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkPhysicalDeviceConditionalRenderingFeaturesEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(conditionalRendering);
  SERIALISE_MEMBER(inheritedConditionalRendering);
}

template <>
void Deserialise(const VkPhysicalDeviceConditionalRenderingFeaturesEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkConditionalRenderingBeginInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(buffer);
  SERIALISE_MEMBER(offset);
  SERIALISE_MEMBER_VKFLAGS(VkConditionalRenderingFlagsEXT, flags);
}

template <>
void Deserialise(const VkConditionalRenderingBeginInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

// pNext structs - always have deserialise for the next chain
INSTANTIATE_SERIALISE_TYPE(VkAcquireNextImageInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkApplicationInfo);
INSTANTIATE_SERIALISE_TYPE(VkAttachmentDescription2KHR);
INSTANTIATE_SERIALISE_TYPE(VkAttachmentReference2KHR);
INSTANTIATE_SERIALISE_TYPE(VkBindBufferMemoryDeviceGroupInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindBufferMemoryInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindImageMemoryDeviceGroupInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindImageMemoryInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindImageMemorySwapchainInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkBindImagePlaneMemoryInfo);
INSTANTIATE_SERIALISE_TYPE(VkBindSparseInfo);
INSTANTIATE_SERIALISE_TYPE(VkBufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkBufferDeviceAddressInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkBufferDeviceAddressCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkBufferMemoryBarrier);
INSTANTIATE_SERIALISE_TYPE(VkBufferMemoryRequirementsInfo2);
INSTANTIATE_SERIALISE_TYPE(VkBufferViewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkCalibratedTimestampInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferInheritanceInfo);
INSTANTIATE_SERIALISE_TYPE(VkCommandBufferInheritanceConditionalRenderingInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkCommandPoolCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkComputePipelineCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkCopyDescriptorSet);
INSTANTIATE_SERIALISE_TYPE(VkDebugMarkerMarkerInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugMarkerObjectNameInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugMarkerObjectTagInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugReportCallbackCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugUtilsLabelEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugUtilsMessengerCallbackDataEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugUtilsMessengerCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugUtilsObjectNameInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDebugUtilsObjectTagInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDedicatedAllocationBufferCreateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkDedicatedAllocationImageCreateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkDedicatedAllocationMemoryAllocateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorPoolCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetLayoutBindingFlagsCreateInfoEXT)
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetLayoutCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetLayoutSupport);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetVariableDescriptorCountAllocateInfoEXT)
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetVariableDescriptorCountLayoutSupportEXT)
INSTANTIATE_SERIALISE_TYPE(VkDescriptorUpdateTemplateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceEventInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupBindSparseInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupCommandBufferBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupDeviceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupPresentCapabilitiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupPresentInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupRenderPassBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupSubmitInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceGroupSwapchainCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkDeviceQueueCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkDeviceQueueGlobalPriorityCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDeviceQueueInfo2);
INSTANTIATE_SERIALISE_TYPE(VkDisplayEventInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDisplayModeProperties2KHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayNativeHdrSurfaceCapabilitiesAMD);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPlaneCapabilities2KHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPlaneInfo2KHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPlaneProperties2KHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPowerInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPresentInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayProperties2KHR);
INSTANTIATE_SERIALISE_TYPE(VkEventCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExportFenceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryAllocateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkExportSemaphoreCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExternalBufferProperties);
INSTANTIATE_SERIALISE_TYPE(VkExternalFenceProperties);
INSTANTIATE_SERIALISE_TYPE(VkExternalImageFormatProperties);
INSTANTIATE_SERIALISE_TYPE(VkExternalMemoryBufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExternalMemoryImageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkExternalMemoryImageCreateInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkExternalSemaphoreProperties);
INSTANTIATE_SERIALISE_TYPE(VkFenceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkFenceGetFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkFormatProperties2);
INSTANTIATE_SERIALISE_TYPE(VkFramebufferCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkGraphicsPipelineCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkHdrMetadataEXT);
INSTANTIATE_SERIALISE_TYPE(VkImageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageFormatListCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImageFormatProperties2);
INSTANTIATE_SERIALISE_TYPE(VkImageMemoryBarrier);
INSTANTIATE_SERIALISE_TYPE(VkImageMemoryRequirementsInfo2);
INSTANTIATE_SERIALISE_TYPE(VkImagePlaneMemoryRequirementsInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageStencilUsageCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkImageSparseMemoryRequirementsInfo2);
INSTANTIATE_SERIALISE_TYPE(VkImageSwapchainCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImageViewASTCDecodeModeEXT);
INSTANTIATE_SERIALISE_TYPE(VkImageViewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImageViewUsageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkImportFenceFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImportMemoryFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImportSemaphoreFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkInstanceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkLayerDeviceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkLayerInstanceCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkMappedMemoryRange);
INSTANTIATE_SERIALISE_TYPE(VkMemoryAllocateFlagsInfo);
INSTANTIATE_SERIALISE_TYPE(VkMemoryAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkMemoryBarrier);
INSTANTIATE_SERIALISE_TYPE(VkMemoryDedicatedAllocateInfo);
INSTANTIATE_SERIALISE_TYPE(VkMemoryDedicatedRequirements);
INSTANTIATE_SERIALISE_TYPE(VkMemoryFdPropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkMemoryGetFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkMemoryPriorityAllocateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkMemoryRequirements2);
INSTANTIATE_SERIALISE_TYPE(VkMultisamplePropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDevice16BitStorageFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDevice8BitStorageFeaturesKHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceASTCDecodeFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceBufferDeviceAddressFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceConditionalRenderingFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceConservativeRasterizationPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceDepthStencilResolvePropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceDepthClipEnableFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceDescriptorIndexingFeaturesEXT)
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceDescriptorIndexingPropertiesEXT)
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceDiscardRectanglePropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceDriverPropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceExternalBufferInfo);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceExternalFenceInfo);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceExternalImageFormatInfo);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceExternalSemaphoreInfo);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceFeatures2);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceGroupProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceHostQueryResetFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceIDProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceImageFormatInfo2);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMaintenance3Properties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMemoryBudgetPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMemoryPriorityFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMemoryProperties2);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMultiviewFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMultiviewProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDevicePCIBusInfoPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDevicePointClippingProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceProperties2);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceProtectedMemoryFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceProtectedMemoryProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDevicePushDescriptorPropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSamplerYcbcrConversionFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceScalarBlockLayoutFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceShaderAtomicInt64FeaturesKHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceShaderCorePropertiesAMD);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceShaderDrawParametersFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSampleLocationsPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSparseImageFormatInfo2);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSubgroupProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSurfaceInfo2KHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceTransformFeedbackFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceTransformFeedbackPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceVariablePointerFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceVulkanMemoryModelFeaturesKHR);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineCacheCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineCreationFeedbackCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineColorBlendStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineDepthStencilStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineDiscardRectangleStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineDynamicStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineInputAssemblyStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineLayoutCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineMultisampleStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineRasterizationConservativeStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineRasterizationDepthClipStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineRasterizationStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineRasterizationStateStreamCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineSampleLocationsStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineShaderStageCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineTessellationDomainOriginStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineTessellationStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineVertexInputDivisorStateCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineVertexInputStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPipelineViewportStateCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkPresentInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkPresentRegionsKHR);
INSTANTIATE_SERIALISE_TYPE(VkProtectedSubmitInfo);
INSTANTIATE_SERIALISE_TYPE(VkQueryPoolCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkQueueFamilyProperties2);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassBeginInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassCreateInfo2KHR);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassFragmentDensityMapCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassInputAttachmentAspectCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassMultiviewCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkRenderPassSampleLocationsBeginInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSampleLocationsInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSamplerCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSamplerReductionModeCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSamplerYcbcrConversionCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSamplerYcbcrConversionImageFormatProperties);
INSTANTIATE_SERIALISE_TYPE(VkSamplerYcbcrConversionInfo);
INSTANTIATE_SERIALISE_TYPE(VkSemaphoreCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkSemaphoreGetFdInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkShaderModuleCreateInfo);
INSTANTIATE_SERIALISE_TYPE(VkShaderModuleValidationCacheCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSharedPresentSurfaceCapabilitiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageFormatProperties2);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageMemoryRequirements2);
INSTANTIATE_SERIALISE_TYPE(VkSubmitInfo);
INSTANTIATE_SERIALISE_TYPE(VkSubpassBeginInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDependency2KHR);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDescription2KHR);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDescriptionDepthStencilResolveKHR);
INSTANTIATE_SERIALISE_TYPE(VkSubpassEndInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceCapabilities2EXT);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceCapabilities2KHR);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceFormat2KHR);
INSTANTIATE_SERIALISE_TYPE(VkSwapchainCounterCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSwapchainCreateInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkSwapchainDisplayNativeHdrCreateInfoAMD);
INSTANTIATE_SERIALISE_TYPE(VkTextureLODGatherFormatPropertiesAMD);
INSTANTIATE_SERIALISE_TYPE(VkValidationCacheCreateInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkValidationFlagsEXT);
INSTANTIATE_SERIALISE_TYPE(VkWriteDescriptorSet);
INSTANTIATE_SERIALISE_TYPE(VkConditionalRenderingBeginInfoEXT);

// plain structs with no next chain
INSTANTIATE_SERIALISE_TYPE(VkAllocationCallbacks);
INSTANTIATE_SERIALISE_TYPE(VkAttachmentDescription);
INSTANTIATE_SERIALISE_TYPE(VkAttachmentReference);
INSTANTIATE_SERIALISE_TYPE(VkBufferCopy);
INSTANTIATE_SERIALISE_TYPE(VkBufferImageCopy);
INSTANTIATE_SERIALISE_TYPE(VkClearAttachment);
INSTANTIATE_SERIALISE_TYPE(VkClearColorValue);
INSTANTIATE_SERIALISE_TYPE(VkClearDepthStencilValue);
INSTANTIATE_SERIALISE_TYPE(VkClearRect);
INSTANTIATE_SERIALISE_TYPE(VkClearValue);
INSTANTIATE_SERIALISE_TYPE(VkComponentMapping);
INSTANTIATE_SERIALISE_TYPE(VkConformanceVersionKHR);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorBufferInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorImageInfo);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorPoolSize);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorSetLayoutBinding);
INSTANTIATE_SERIALISE_TYPE(VkDescriptorUpdateTemplateEntry);
INSTANTIATE_SERIALISE_TYPE(VkDispatchIndirectCommand);
INSTANTIATE_SERIALISE_TYPE(VkDisplayModeParametersKHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayModePropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPlaneCapabilitiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPlanePropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkDisplayPropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkDrawIndexedIndirectCommand);
INSTANTIATE_SERIALISE_TYPE(VkDrawIndirectCommand);
INSTANTIATE_SERIALISE_TYPE(VkExtent2D);
INSTANTIATE_SERIALISE_TYPE(VkExtent3D);
INSTANTIATE_SERIALISE_TYPE(VkExternalMemoryProperties);
INSTANTIATE_SERIALISE_TYPE(VkFormatProperties);
INSTANTIATE_SERIALISE_TYPE(VkImageBlit);
INSTANTIATE_SERIALISE_TYPE(VkImageCopy);
INSTANTIATE_SERIALISE_TYPE(VkImageFormatProperties);
INSTANTIATE_SERIALISE_TYPE(VkImageResolve);
INSTANTIATE_SERIALISE_TYPE(VkImageSubresource);
INSTANTIATE_SERIALISE_TYPE(VkImageSubresourceLayers);
INSTANTIATE_SERIALISE_TYPE(VkImageSubresourceRange);
INSTANTIATE_SERIALISE_TYPE(VkInputAttachmentAspectReference);
INSTANTIATE_SERIALISE_TYPE(VkMemoryHeap);
INSTANTIATE_SERIALISE_TYPE(VkMemoryRequirements);
INSTANTIATE_SERIALISE_TYPE(VkMemoryType);
INSTANTIATE_SERIALISE_TYPE(VkOffset2D);
INSTANTIATE_SERIALISE_TYPE(VkOffset3D);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceFeatures);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceLimits);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceMemoryProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceProperties);
INSTANTIATE_SERIALISE_TYPE(VkPhysicalDeviceSparseProperties);
INSTANTIATE_SERIALISE_TYPE(VkPipelineCreationFeedbackEXT);
INSTANTIATE_SERIALISE_TYPE(VkPipelineColorBlendAttachmentState);
INSTANTIATE_SERIALISE_TYPE(VkPresentRegionKHR);
INSTANTIATE_SERIALISE_TYPE(VkPushConstantRange);
INSTANTIATE_SERIALISE_TYPE(VkQueueFamilyProperties);
INSTANTIATE_SERIALISE_TYPE(VkRect2D);
INSTANTIATE_SERIALISE_TYPE(VkRectLayerKHR);
INSTANTIATE_SERIALISE_TYPE(VkSparseBufferMemoryBindInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageFormatProperties);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageMemoryBind);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageMemoryBindInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageMemoryRequirements);
INSTANTIATE_SERIALISE_TYPE(VkSparseImageOpaqueMemoryBindInfo);
INSTANTIATE_SERIALISE_TYPE(VkSparseMemoryBind);
INSTANTIATE_SERIALISE_TYPE(VkSpecializationInfo);
INSTANTIATE_SERIALISE_TYPE(VkSpecializationMapEntry);
INSTANTIATE_SERIALISE_TYPE(VkStencilOpState);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDependency);
INSTANTIATE_SERIALISE_TYPE(VkSubpassDescription);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceCapabilitiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceFormatKHR);
INSTANTIATE_SERIALISE_TYPE(VkVertexInputAttributeDescription);
INSTANTIATE_SERIALISE_TYPE(VkVertexInputBindingDescription);
INSTANTIATE_SERIALISE_TYPE(VkVertexInputBindingDivisorDescriptionEXT);
INSTANTIATE_SERIALISE_TYPE(VkViewport);
INSTANTIATE_SERIALISE_TYPE(VkXYColorEXT);

INSTANTIATE_SERIALISE_TYPE(DescriptorSetSlot);
INSTANTIATE_SERIALISE_TYPE(ImageRegionState);
INSTANTIATE_SERIALISE_TYPE(ImageLayouts);
INSTANTIATE_SERIALISE_TYPE(ImageInfo);

#if ENABLED(RDOC_WIN32)
template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportMemoryWin32HandleInfoNV &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER_VKFLAGS(VkExternalMemoryHandleTypeFlagsNV, handleType);

  {
    uint64_t handle = (uint64_t)el.handle;
    ser.Serialise("handle"_lit, handle);

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
    ser.Serialise("pAttributes"_lit, pAttributes).TypedAs("SECURITY_ATTRIBUTES*"_lit);

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
    ser.Serialise("handle"_lit, handle);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.handle = NULL;
  }

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name"_lit, name);

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

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportMemoryWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  {
    // serialise pointer as plain integer, rather than recursing and serialising struct
    uint64_t pAttributes = (uint64_t)el.pAttributes;
    ser.Serialise("pAttributes"_lit, pAttributes).TypedAs("SECURITY_ATTRIBUTES*"_lit);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.pAttributes = NULL;
  }

  SERIALISE_MEMBER_TYPED(uint32_t, dwAccess);

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name"_lit, name);

    // we don't expose UTF82Wide on all platforms, but as above this struct won't be valid anyway
    if(ser.IsReading())
      el.name = NULL;
  }
}

template <>
void Deserialise(const VkExportMemoryWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryWin32HandlePropertiesKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memoryTypeBits);
}

template <>
void Deserialise(const VkMemoryWin32HandlePropertiesKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkMemoryGetWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkMemoryGetWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportFenceWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  {
    // serialise pointer as plain integer, rather than recursing and serialising struct
    uint64_t pAttributes = (uint64_t)el.pAttributes;
    ser.Serialise("pAttributes"_lit, pAttributes).TypedAs("SECURITY_ATTRIBUTES*"_lit);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.pAttributes = NULL;
  }

  SERIALISE_MEMBER_TYPED(uint32_t, dwAccess);

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name"_lit, name);

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

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportFenceWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fence);
  SERIALISE_MEMBER_VKFLAGS(VkFenceImportFlags, flags);
  SERIALISE_MEMBER(handleType);

  {
    uint64_t handle = (uint64_t)el.handle;
    ser.Serialise("handle"_lit, handle);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.handle = NULL;
  }

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name"_lit, name);

    // we don't expose UTF82Wide on all platforms, but as above this struct won't be valid anyway
    if(ser.IsReading())
      el.name = L"???";
  }
}

template <>
void Deserialise(const VkImportFenceWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkFenceGetWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fence);
  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkFenceGetWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkExportSemaphoreWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  {
    // serialise pointer as plain integer, rather than recursing and serialising struct
    uint64_t pAttributes = (uint64_t)el.pAttributes;
    ser.Serialise("pAttributes"_lit, pAttributes).TypedAs("SECURITY_ATTRIBUTES*"_lit);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.pAttributes = NULL;
  }

  SERIALISE_MEMBER_TYPED(uint32_t, dwAccess);

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name"_lit, name);

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

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkImportSemaphoreWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(semaphore);
  SERIALISE_MEMBER_VKFLAGS(VkSemaphoreImportFlags, flags);
  SERIALISE_MEMBER(handleType);

  {
    uint64_t handle = (uint64_t)el.handle;
    ser.Serialise("handle"_lit, handle);

    // won't be valid on read, though we won't try to replay this anyway
    if(ser.IsReading())
      el.handle = NULL;
  }

  {
    std::string name;

    if(ser.IsWriting())
      name = el.name ? StringFormat::Wide2UTF8(std::wstring(el.name)) : "";

    ser.Serialise("name"_lit, name);

    // we don't expose UTF82Wide on all platforms, but as above this struct won't be valid anyway
    if(ser.IsReading())
      el.name = L"???";
  }
}

template <>
void Deserialise(const VkImportSemaphoreWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkD3D12FenceSubmitInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(waitSemaphoreValuesCount);
  SERIALISE_MEMBER_ARRAY(pWaitSemaphoreValues, waitSemaphoreValuesCount);
  SERIALISE_MEMBER(signalSemaphoreValuesCount);
  SERIALISE_MEMBER_ARRAY(pSignalSemaphoreValues, signalSemaphoreValuesCount);
}

template <>
void Deserialise(const VkD3D12FenceSubmitInfoKHR &el)
{
  DeserialiseNext(el.pNext);
  delete[] el.pWaitSemaphoreValues;
  delete[] el.pSignalSemaphoreValues;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSemaphoreGetWin32HandleInfoKHR &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(semaphore);
  SERIALISE_MEMBER(handleType);
}

template <>
void Deserialise(const VkSemaphoreGetWin32HandleInfoKHR &el)
{
  DeserialiseNext(el.pNext);
}

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

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceFullScreenExclusiveInfoEXT &el)
{
  RDCASSERT(ser.IsReading() || el.sType == VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fullScreenExclusive);
}

template <>
void Deserialise(const VkSurfaceFullScreenExclusiveInfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceCapabilitiesFullScreenExclusiveEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  SERIALISE_MEMBER(fullScreenExclusiveSupported);
}

template <>
void Deserialise(const VkSurfaceCapabilitiesFullScreenExclusiveEXT &el)
{
  DeserialiseNext(el.pNext);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkSurfaceFullScreenExclusiveWin32InfoEXT &el)
{
  RDCASSERT(ser.IsReading() ||
            el.sType == VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT);
  SerialiseNext(ser, el.sType, el.pNext);

  uint64_t hmonitor = (uint64_t)el.hmonitor;
  ser.Serialise("hmonitor"_lit, hmonitor);
}

template <>
void Deserialise(const VkSurfaceFullScreenExclusiveWin32InfoEXT &el)
{
  DeserialiseNext(el.pNext);
}

INSTANTIATE_SERIALISE_TYPE(VkImportMemoryWin32HandleInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryWin32HandleInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkWin32KeyedMutexAcquireReleaseInfoNV);
INSTANTIATE_SERIALISE_TYPE(VkWin32KeyedMutexAcquireReleaseInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImportMemoryWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkExportMemoryWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkMemoryWin32HandlePropertiesKHR);
INSTANTIATE_SERIALISE_TYPE(VkMemoryGetWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkExportSemaphoreWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImportSemaphoreWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkD3D12FenceSubmitInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkSemaphoreGetWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkExportFenceWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkImportFenceWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkFenceGetWin32HandleInfoKHR);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceCapabilitiesFullScreenExclusiveEXT);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceFullScreenExclusiveInfoEXT);
INSTANTIATE_SERIALISE_TYPE(VkSurfaceFullScreenExclusiveWin32InfoEXT);
#endif
