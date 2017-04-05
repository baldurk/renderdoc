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

#pragma once

// PORTABILITY - parts of the code that need to change/update to handle
// portability between different GPU setups/capabilities etc

// MULTIDEVICE - parts of the code that will need to be updated to support
// multiple devices or queues.

// SHARING - as above, for handling resource sharing between queues

#include "common/common.h"

#define VK_NO_PROTOTYPES

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHX 1
#endif

#if ENABLED(RDOC_X64)

#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;

#else

// make handles typed even on 32-bit, by relying on C++
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(obj)                                 \
  struct obj                                                                   \
  {                                                                            \
    obj() : handle(0) {}                                                       \
    obj(uint64_t x) : handle(x) {}                                             \
    bool operator==(const obj &other) const { return handle == other.handle; } \
    bool operator<(const obj &other) const { return handle < other.handle; }   \
    bool operator!=(const obj &other) const { return handle != other.handle; } \
    uint64_t handle;                                                           \
  };
#define VK_NON_DISPATCHABLE_WRAPPER_STRUCT

#endif

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "official/vulkan.h"
#include "serialise/serialiser.h"
#include "vk_dispatchtables.h"

// uncomment this to cause every internal QueueSubmit to immediately call
// DeviceWaitIdle(), and to only submit one command buffer at once to narrow
// down the cause of device lost errors
#define SINGLE_FLUSH_VALIDATE OPTION_OFF

// uncomment this to get verbose debugging about when/where/why partial command
// buffer replay is happening
#define VERBOSE_PARTIAL_REPLAY OPTION_OFF

// uncomment this to enable validation layers on replay, useful for debugging
// problems with new replay code
#define FORCE_VALIDATION_LAYERS OPTION_OFF

ResourceFormat MakeResourceFormat(VkFormat fmt);
VkFormat MakeVkFormat(ResourceFormat fmt);
Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints);
VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo);
AddressMode MakeAddressMode(VkSamplerAddressMode addr);
void MakeBorderColor(VkBorderColor border, FloatVector *BorderColor);
CompareFunc MakeCompareFunc(VkCompareOp func);
TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
                         bool anisoEnable, bool compareEnable);
LogicOp MakeLogicOp(VkLogicOp op);
BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend);
BlendOp MakeBlendOp(VkBlendOp op);
StencilOp MakeStencilOp(VkStencilOp op);

// set conservative access bits for this image layout
VkAccessFlags MakeAccessMask(VkImageLayout layout);

void ReplacePresentableImageLayout(VkImageLayout &layout);
void ReplaceExternalQueueFamily(uint32_t &srcQueueFamily, uint32_t &dstQueueFamily);

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkImageMemoryBarrier *barriers);
void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkBufferMemoryBarrier *barriers);
void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkMemoryBarrier *barriers);

int SampleCount(VkSampleCountFlagBits countFlag);
int SampleIndex(VkSampleCountFlagBits countFlag);
int StageIndex(VkShaderStageFlagBits stageFlag);

// in vk_<platform>.cpp
extern const char *VulkanLibraryName;

extern const uint32_t AMD_PCI_ID;
extern const uint32_t NV_PCI_ID;

class VkDriverInfo
{
public:
  bool IsAMD() { return m_Vendor == AMD; }
  bool IsNV() { return m_Vendor == NV; }
  uint32_t Major() { return m_Major; }
  uint32_t Minor() { return m_Minor; }
  uint32_t Patch() { return m_Patch; }
  VkDriverInfo(const VkPhysicalDeviceProperties &physProps)
  {
    if(physProps.vendorID == AMD_PCI_ID)
      m_Vendor = AMD;
    else if(physProps.vendorID == NV_PCI_ID)
      m_Vendor = NV;
    else
      m_Vendor = UNKNOWN;

    m_Major = VK_VERSION_MAJOR(physProps.driverVersion);
    m_Minor = VK_VERSION_MINOR(physProps.driverVersion);
    m_Patch = VK_VERSION_PATCH(physProps.driverVersion);

    // nvidia uses its own version packing:
    //   10 |  8  |        8       |       6
    // major|minor|secondary_branch|tertiary_branch
    if(IsNV())
    {
      m_Major = ((uint32_t)(physProps.driverVersion) >> (8 + 8 + 6)) & 0x3ff;
      m_Minor = ((uint32_t)(physProps.driverVersion) >> (8 + 6)) & 0x0ff;

      uint32_t secondary = ((uint32_t)(physProps.driverVersion) >> 6) & 0x0ff;
      uint32_t tertiary = physProps.driverVersion & 0x03f;

      m_Patch = (secondary << 8) | tertiary;
    }
  }

private:
  enum
  {
    AMD,
    NV,
    UNKNOWN,
  } m_Vendor;

  uint32_t m_Major, m_Minor, m_Patch;
};

enum
{
  VkCheckExt_AMD_neg_viewport,
  VkCheckExt_Max,
};

// structure for casting to easily iterate and template specialising Serialise
struct VkGenericStruct
{
  VkStructureType sType;
  const VkGenericStruct *pNext;
};

// utility function for when we're modifying one struct in a pNext chain, this
// lets us just copy across a struct unmodified into some temporary memory and
// append it onto a pNext chain we're building
template <typename VkStruct>
void CopyNextChainedStruct(byte *&tempMem, const VkGenericStruct *nextInput,
                           VkGenericStruct *&nextChainTail)
{
  const VkStruct *instruct = (const VkStruct *)nextInput;
  VkStruct *outstruct = (VkStruct *)tempMem;

  tempMem = (byte *)(outstruct + 1);

  // copy the struct, nothing to unwrap
  *outstruct = *instruct;

  // default to NULL. It will be overwritten next time if there is a next object
  outstruct->pNext = NULL;

  // append this onto the chain
  nextChainTail->pNext = (const VkGenericStruct *)outstruct;
  nextChainTail = (VkGenericStruct *)outstruct;
}

// this is similar to the above function, but for use after we've modified a struct locally
// e.g. to unwrap some members or patch flags, etc.
template <typename VkStruct>
void AppendModifiedChainedStruct(byte *&tempMem, VkStruct *outputStruct,
                                 VkGenericStruct *&nextChainTail)
{
  tempMem = (byte *)(outputStruct + 1);

  // default to NULL. It will be overwritten in the next step if there is a next object
  outputStruct->pNext = NULL;

  // append this onto the chain
  nextChainTail->pNext = (const VkGenericStruct *)outputStruct;
  nextChainTail = (VkGenericStruct *)outputStruct;
}

#define RENDERDOC_LAYER_NAME "VK_LAYER_RENDERDOC_Capture"

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                              \
  bool CONCAT(Serialise_, func(Serialiser *localSerialiser, __VA_ARGS__));

template <>
void Serialiser::Serialise(const char *name, VkRect2D &el);
template <>
void Serialiser::Serialise(const char *name, VkDeviceQueueCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceFeatures &el);
template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceMemoryProperties &el);
template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceProperties &el);
template <>
void Serialiser::Serialise(const char *name, VkDeviceCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkBufferCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkBufferViewCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkImageCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkImageViewCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkSparseMemoryBind &el);
template <>
void Serialiser::Serialise(const char *name, VkBindSparseInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkFramebufferCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkRenderPassCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkRenderPassBeginInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineInputAssemblyStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineTessellationStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineViewportStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineRasterizationStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineMultisampleStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineDepthStencilStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineColorBlendStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineDynamicStateCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineLayoutCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPushConstantRange &el);
template <>
void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutBinding &el);
template <>
void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkDescriptorPoolCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkDescriptorSetAllocateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkWriteDescriptorSet &el);
template <>
void Serialiser::Serialise(const char *name, VkCopyDescriptorSet &el);
template <>
void Serialiser::Serialise(const char *name, VkCommandPoolCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkCommandBufferAllocateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkCommandBufferBeginInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkStencilOpState &el);
template <>
void Serialiser::Serialise(const char *name, VkQueryPoolCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkSemaphoreCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkEventCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkFenceCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkSamplerCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkPipelineCacheCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkShaderModuleCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkImageSubresourceRange &el);
template <>
void Serialiser::Serialise(const char *name, VkImageSubresource &el);
template <>
void Serialiser::Serialise(const char *name, VkImageSubresourceLayers &el);
template <>
void Serialiser::Serialise(const char *name, VkMemoryAllocateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkMemoryBarrier &el);
template <>
void Serialiser::Serialise(const char *name, VkBufferMemoryBarrier &el);
template <>
void Serialiser::Serialise(const char *name, VkImageMemoryBarrier &el);
template <>
void Serialiser::Serialise(const char *name, VkGraphicsPipelineCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkComputePipelineCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkComponentMapping &el);
template <>
void Serialiser::Serialise(const char *name, VkComputePipelineCreateInfo &el);
template <>
void Serialiser::Serialise(const char *name, VkBufferImageCopy &el);
template <>
void Serialiser::Serialise(const char *name, VkBufferCopy &el);
template <>
void Serialiser::Serialise(const char *name, VkImageCopy &el);
template <>
void Serialiser::Serialise(const char *name, VkImageBlit &el);
template <>
void Serialiser::Serialise(const char *name, VkImageResolve &el);

template <>
void Serialiser::Serialise(const char *name, VkSwapchainCreateInfoKHR &el);

struct DescriptorSetSlot;
template <>
void Serialiser::Serialise(const char *name, DescriptorSetSlot &el);

template <>
void Serialiser::Deserialise(const VkDeviceCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkBufferCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkImageCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkBindSparseInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkDescriptorSetAllocateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkFramebufferCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkRenderPassCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkRenderPassBeginInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkCommandBufferBeginInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkPipelineCacheCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkPipelineLayoutCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkShaderModuleCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkGraphicsPipelineCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkComputePipelineCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkDescriptorPoolCreateInfo *const el) const;
template <>
void Serialiser::Deserialise(const VkWriteDescriptorSet *const el) const;
template <>
void Serialiser::Deserialise(const VkDescriptorSetLayoutCreateInfo *const el) const;

// the possible contents of a descriptor set slot,
// taken from the VkWriteDescriptorSet
struct DescriptorSetSlot
{
  VkDescriptorBufferInfo bufferInfo;
  VkDescriptorImageInfo imageInfo;
  VkBufferView texelBufferView;
};

#define NUM_VK_IMAGE_ASPECTS 4
#define VK_ACCESS_ALL_READ_BITS                                                        \
  (VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT |                    \
   VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |                  \
   VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |                   \
   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | \
   VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT)
#define VK_ACCESS_ALL_WRITE_BITS                                                 \
  (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |           \
   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | \
   VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT)

#pragma region Chunks

enum VulkanChunkType
{
  DEVICE_INIT = FIRST_CHUNK_ID,
  CREATE_INSTANCE,
  ENUM_PHYSICALS,
  CREATE_DEVICE,
  GET_DEVICE_QUEUE,

  ALLOC_MEM,
  UNMAP_MEM,
  FLUSH_MEM,
  FREE_MEM,

  CREATE_CMD_POOL,
  RESET_CMD_POOL,

  CREATE_CMD_BUFFER,
  CREATE_FRAMEBUFFER,
  CREATE_RENDERPASS,
  CREATE_DESCRIPTOR_POOL,
  CREATE_DESCRIPTOR_SET_LAYOUT,
  CREATE_BUFFER,
  CREATE_BUFFER_VIEW,
  CREATE_IMAGE,
  CREATE_IMAGE_VIEW,
  CREATE_DEPTH_TARGET_VIEW,
  CREATE_SAMPLER,
  CREATE_SHADER_MODULE,
  CREATE_PIPE_LAYOUT,
  CREATE_PIPE_CACHE,
  CREATE_GRAPHICS_PIPE,
  CREATE_COMPUTE_PIPE,
  GET_SWAPCHAIN_IMAGE,

  CREATE_SEMAPHORE,
  CREATE_FENCE,
  GET_FENCE_STATUS,
  RESET_FENCE,
  WAIT_FENCES,

  CREATE_EVENT,
  GET_EVENT_STATUS,
  SET_EVENT,
  RESET_EVENT,

  CREATE_QUERY_POOL,

  ALLOC_DESC_SET,
  UPDATE_DESC_SET,

  BEGIN_CMD_BUFFER,
  END_CMD_BUFFER,

  QUEUE_WAIT_IDLE,
  DEVICE_WAIT_IDLE,

  QUEUE_SUBMIT,
  BIND_BUFFER_MEM,
  BIND_IMAGE_MEM,

  BIND_SPARSE,

  BEGIN_RENDERPASS,
  NEXT_SUBPASS,
  EXEC_CMDS,
  END_RENDERPASS,

  BIND_PIPELINE,

  SET_VP,
  SET_SCISSOR,
  SET_LINE_WIDTH,
  SET_DEPTH_BIAS,
  SET_BLEND_CONST,
  SET_DEPTH_BOUNDS,
  SET_STENCIL_COMP_MASK,
  SET_STENCIL_WRITE_MASK,
  SET_STENCIL_REF,

  BIND_DESCRIPTOR_SET,
  BIND_VERTEX_BUFFERS,
  BIND_INDEX_BUFFER,
  COPY_BUF2IMG,
  COPY_IMG2BUF,
  COPY_BUF,
  COPY_IMG,
  BLIT_IMG,
  RESOLVE_IMG,
  UPDATE_BUF,
  FILL_BUF,
  PUSH_CONST,

  CLEAR_COLOR,
  CLEAR_DEPTHSTENCIL,
  CLEAR_ATTACH,
  PIPELINE_BARRIER,

  WRITE_TIMESTAMP,
  COPY_QUERY_RESULTS,
  BEGIN_QUERY,
  END_QUERY,
  RESET_QUERY_POOL,

  CMD_SET_EVENT,
  CMD_RESET_EVENT,
  CMD_WAIT_EVENTS,

  DRAW,
  DRAW_INDIRECT,
  DRAW_INDEXED,
  DRAW_INDEXED_INDIRECT,
  DISPATCH,
  DISPATCH_INDIRECT,

  BEGIN_EVENT,
  SET_MARKER,
  END_EVENT,

  SET_NAME,
  SET_SHADER_DEBUG_PATH,

  CREATE_SWAP_BUFFER,

  DEBUG_MESSAGES,

  CAPTURE_SCOPE,
  CONTEXT_CAPTURE_HEADER,
  CONTEXT_CAPTURE_FOOTER,

  NUM_VULKAN_CHUNKS,
};

#pragma endregion Chunks
