/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "common/common.h"

#define VK_NO_PROTOTYPES

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

#include "core/core.h"
#include "core/resource_manager.h"
#include "official/vk_layer.h"
#include "official/vulkan.h"
#include "serialise/serialiser.h"
#include "vk_dispatchtables.h"

#undef Bool
#undef None

// enable this to cause every internal QueueSubmit to immediately call DeviceWaitIdle(), and to only
// submit one command buffer at once to narrow down the cause of device lost errors
#define SINGLE_FLUSH_VALIDATE OPTION_OFF

// enable this to get verbose debugging about when/where/why partial command buffer replay is
// happening
#define VERBOSE_PARTIAL_REPLAY OPTION_OFF

ResourceFormat MakeResourceFormat(VkFormat fmt);
VkFormat MakeVkFormat(ResourceFormat fmt);
Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints);
VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo);
AddressMode MakeAddressMode(VkSamplerAddressMode addr);
void MakeBorderColor(VkBorderColor border, FloatVector *BorderColor);
CompareFunction MakeCompareFunc(VkCompareOp func);
FilterMode MakeFilterMode(VkFilter f);
TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
                         bool anisoEnable, bool compareEnable, VkSamplerReductionMode reduction);
LogicOperation MakeLogicOp(VkLogicOp op);
BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend);
BlendOperation MakeBlendOp(VkBlendOp op);
StencilOperation MakeStencilOp(VkStencilOp op);
rdcstr HumanDriverName(VkDriverId driverId);

// set conservative access bits for this image layout
VkAccessFlags MakeAccessMask(VkImageLayout layout);

void SanitiseOldImageLayout(VkImageLayout &layout);
void SanitiseNewImageLayout(VkImageLayout &layout);
void SanitiseReplayImageLayout(VkImageLayout &layout);

void CombineDepthStencilLayouts(rdcarray<VkImageMemoryBarrier> &barriers);

void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkImageMemoryBarrier *barriers);
void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkBufferMemoryBarrier *barriers);
void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkMemoryBarrier *barriers);

int SampleCount(VkSampleCountFlagBits countFlag);
int SampleIndex(VkSampleCountFlagBits countFlag);
int StageIndex(VkShaderStageFlagBits stageFlag);

struct PackedWindowHandle
{
  PackedWindowHandle(WindowingSystem s, void *h) : system(s), handle(h) {}
  WindowingSystem system;
  void *handle;
};

struct VkResourceRecord;

class WrappedVulkan;

struct VkPackedVersion
{
  VkPackedVersion(uint32_t v = 0) : version(v) {}
  uint32_t version;

  bool operator<(uint32_t v) const { return version < v; }
  bool operator>(uint32_t v) const { return version > v; }
  bool operator<=(uint32_t v) const { return version <= v; }
  bool operator>=(uint32_t v) const { return version >= v; }
  bool operator==(uint32_t v) const { return version == v; }
  bool operator!=(uint32_t v) const { return version != v; }
  // int overloads because VK_MAKE_VERSION is type int...
  bool operator<(int v) const { return version < (uint32_t)v; }
  bool operator>(int v) const { return version > (uint32_t)v; }
  bool operator<=(int v) const { return version <= (uint32_t)v; }
  bool operator>=(int v) const { return version >= (uint32_t)v; }
  bool operator==(int v) const { return version == (uint32_t)v; }
  bool operator!=(int v) const { return version != (uint32_t)v; }
  operator uint32_t() const { return version; }
  VkPackedVersion &operator=(uint32_t v)
  {
    version = v;
    return *this;
  }
};

DECLARE_REFLECTION_STRUCT(VkPackedVersion);

// replay only class for handling marker regions.
//
// The cmd allows you to pass in an existing command buffer to insert/add the marker to.
// If cmd is NULL, then a new command buffer is fetched, begun, the marker is applied to, then
// closed again. Note that when constructing a scoped marker, cmd cannot be NULL
//
// If VK_EXT_debug_marker isn't supported, will silently do nothing
struct VkMarkerRegion
{
  VkMarkerRegion(VkCommandBuffer cmd, const rdcstr &marker);
  VkMarkerRegion(VkQueue q, const rdcstr &marker);
  VkMarkerRegion(const rdcstr &marker) : VkMarkerRegion(VkQueue(VK_NULL_HANDLE), marker) {}
  ~VkMarkerRegion();

  static void Begin(const rdcstr &marker, VkCommandBuffer cmd);
  static void Set(const rdcstr &marker, VkCommandBuffer cmd);
  static void End(VkCommandBuffer cmd);

  static void Begin(const rdcstr &marker, VkQueue q = VK_NULL_HANDLE);
  static void Set(const rdcstr &marker, VkQueue q = VK_NULL_HANDLE);
  static void End(VkQueue q = VK_NULL_HANDLE);

  VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;

  static WrappedVulkan *vk;
};

template <typename T>
void NameVulkanObject(T obj, const rdcstr &name);

struct GPUBuffer
{
  enum CreateFlags
  {
    eGPUBufferReadback = 0x1,
    eGPUBufferVBuffer = 0x2,
    eGPUBufferIBuffer = 0x4,
    eGPUBufferSSBO = 0x8,
    eGPUBufferGPULocal = 0x10,
    eGPUBufferIndirectBuffer = 0x20,
    eGPUBufferAddressable = 0x40,
  };

  void Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t ringSize,
              uint32_t flags);
  void Destroy();

  void FillDescriptor(VkDescriptorBufferInfo &desc);

  size_t GetRingCount() { return size_t(ringCount); }
  void *Map(VkDeviceSize &bindoffset, VkDeviceSize usedsize = 0);
  void *Map(uint32_t *bindoffset = NULL, VkDeviceSize usedsize = 0);
  void Unmap();

  VkDeviceSize sz = 0;
  VkBuffer buf = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;

  // uniform buffer alignment requirement
  VkDeviceSize align = 0;

  // for handling ring allocations
  VkDeviceSize totalsize = 0;
  VkDeviceSize curoffset = 0;
  VkDeviceSize mapoffset = 0;

  uint32_t ringCount = 0;

  WrappedVulkan *m_pDriver = NULL;
  VkDevice device = VK_NULL_HANDLE;
  uint32_t createFlags = 0;
};

// in vk_<platform>.cpp
extern void *LoadVulkanLibrary();

class VkDriverInfo
{
public:
  GPUVendor Vendor() { return m_Vendor; }
  uint32_t Major() { return m_Major; }
  uint32_t Minor() { return m_Minor; }
  uint32_t Patch() { return m_Patch; }
  VkDriverInfo(const VkPhysicalDeviceProperties &physProps);

  // checks for when we're running on metal and some non-queryable things aren't supported
  bool RunningOnMetal() const { return metalBackend; }
  // A workaround for a couple of bugs, removing texelFetch use from shaders.
  // It means broken functionality but at least no instant crashes
  bool TexelFetchBrokenDriver() const { return texelFetchBrokenDriver; }
  // Older AMD driver versions could sometimes cause image memory requirements to vary randomly
  // between identical images. This means the memory required at capture could be less than at
  // replay. To counteract this, on drivers with this issue we pad out the memory requirements
  // enough to account for the change
  bool UnreliableImageMemoryRequirements() const { return unreliableImgMemReqs; }
  // another workaround, on some AMD driver versions creating an MSAA image with STORAGE_BIT
  // causes graphical corruption trying to sample from it. We workaround it by preventing the
  // MSAA <-> Array pipelines from creating, which removes the STORAGE_BIT and skips the copies.
  // It means initial contents of MSAA images are missing but that's less important than being
  // able to inspect MSAA images properly.
  bool AMDStorageMSAABrokenDriver() const { return amdStorageMSAABrokenDriver; }
  // On Qualcomm it seems like binding a descriptor set with a dynamic offset will 'leak' and affect
  // rendering on other descriptor sets that don't use offsets at all.
  bool QualcommLeakingUBOOffsets() const { return qualcommLeakingUBOOffsets; }
  // On Qualcomm emitting an image sample operation with DRef and explicit lod will crash on non-2D
  // textures. Since 2D is the common/expected case, we avoid compiling that case entirely.
  bool QualcommDrefNon2DCompileCrash() const { return qualcommDrefNon2DCompileCrash; }
private:
  GPUVendor m_Vendor;

  uint32_t m_Major, m_Minor, m_Patch;

  bool metalBackend = false;
  bool texelFetchBrokenDriver = false;
  bool unreliableImgMemReqs = false;
  bool amdStorageMSAABrokenDriver = false;
  bool qualcommLeakingUBOOffsets = false;
  bool qualcommDrefNon2DCompileCrash = false;
};

enum
{
  VkCheckLayer_unique_objects,
  VkCheckLayer_Max,
};

DECLARE_REFLECTION_STRUCT(VkBaseInStructure);

// we cast to this type when serialising as a placeholder indicating that
// the given flags field doesn't have any bits defined
enum VkFlagWithNoBits
{
  FlagWithNoBits_Dummy_Bit = 1,
};

size_t GetNextPatchSize(const void *next);
void UnwrapNextChain(CaptureState state, const char *structName, byte *&tempMem,
                     VkBaseInStructure *infoStruct);
void CopyNextChainForPatching(const char *structName, byte *&tempMem, VkBaseInStructure *infoStruct);

template <typename VkStruct>
const VkBaseInStructure *FindNextStruct(const VkStruct *haystack, VkStructureType needle)
{
  if(!haystack)
    return NULL;

  const VkBaseInStructure *next = (const VkBaseInStructure *)haystack->pNext;
  while(next)
  {
    if(next->sType == needle)
      return next;

    next = next->pNext;
  }

  return NULL;
}

template <typename VkStruct>
VkBaseInStructure *FindNextStruct(VkStruct *haystack, VkStructureType needle)
{
  if(!haystack)
    return NULL;

  VkBaseInStructure *next = (VkBaseInStructure *)haystack->pNext;
  while(next)
  {
    if(next->sType == needle)
      return next;

    // assume non-const pNext in the original struct
    next = (VkBaseInStructure *)next->pNext;
  }

  return NULL;
}

template <typename VkStruct>
bool RemoveNextStruct(VkStruct *haystack, VkStructureType needle)
{
  bool ret = false;

  // start from the haystack, and iterate
  VkBaseInStructure *root = (VkBaseInStructure *)haystack;
  while(root && root->pNext)
  {
    // at each point, if the *next* struct is the needle, then point our next pointer at whatever
    // its was - either the next in the chain or NULL if it was at the end. Then we can return true
    // because we removed the struct. We keep going to handle duplicates, but we continue and skip
    // the list iterate as we now have a new root->pNext.
    // Note that this can't remove the first struct in the chain but that's expected, we only want
    // to remove extension structs.
    if(root->pNext->sType == needle)
    {
      root->pNext = root->pNext->pNext;
      ret = true;
      continue;
    }

    // move to the next struct
    root = (VkBaseInStructure *)root->pNext;
  }

  return ret;
}

enum class MemoryScope : uint8_t
{
  InitialContents,
  First = InitialContents,
  // On replay, initial contents memory is never freed, so any immutable replay memory can be
  // allocated the same way
  ImmutableReplayDebug = InitialContents,
  IndirectReadback,
  Count,
};

ITERABLE_OPERATORS(MemoryScope);

enum class MemoryType : uint8_t
{
  Upload,
  GPULocal,
  Readback,
};

struct MemoryAllocation
{
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkDeviceSize offs = 0;
  VkDeviceSize size = 0;

  // not strictly necessary but useful for reflection/readback - what scope/type were used, what was
  // the actual memory type index selected, and was a buffer or image allocated.
  MemoryScope scope = MemoryScope::InitialContents;
  MemoryType type = MemoryType::GPULocal;
  uint32_t memoryTypeIndex = 0;
  bool buffer = false;
};

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func, ...) \
  ret func(__VA_ARGS__);                              \
  template <typename SerialiserType>                  \
  bool CONCAT(Serialise_, func(SerialiserType &ser, __VA_ARGS__));

#define INSTANTIATE_FUNCTION_SERIALISED(ret, func, ...)                                    \
  template bool WrappedVulkan::CONCAT(Serialise_, func(ReadSerialiser &ser, __VA_ARGS__)); \
  template bool WrappedVulkan::CONCAT(Serialise_, func(WriteSerialiser &ser, __VA_ARGS__));

// A handy macros to say "is the serialiser reading and we're doing replay-mode stuff?"
// The reason we check both is that checking the first allows the compiler to eliminate the other
// path at compile-time, and the second because we might be just struct-serialising in which case we
// should be doing no work to restore states.
// Writing is unambiguously during capture mode, so we don't have to check both in that case.
#define IsReplayingAndReading() (ser.IsReading() && IsReplayMode(m_State))

struct VkResourceRecord;
class VulkanResourceManager;

FrameRefType GetRefType(VkDescriptorType descType);

// tracking for descriptor set slots. Needed because if we use something without IDs for tracking
// binding elements the handles may be deleted and recreated, and stale bindings could interfere
// with new bindings
struct DescriptorSetSlotBufferInfo
{
  void SetFrom(const VkDescriptorBufferInfo &bufInfo);

  ResourceId buffer;
  VkDeviceSize offset;
  VkDeviceSize range;
};

struct DescriptorSetSlotImageInfo
{
  void SetFrom(const VkDescriptorImageInfo &imInfo, bool setSampler, bool setImageView);

  ResourceId sampler;
  ResourceId imageView;
  VkImageLayout imageLayout;
};

struct DescriptorSetSlot
{
  void RemoveBindRefs(rdcarray<ResourceId> &ids, VulkanResourceManager *rm, VkResourceRecord *record);
  void AddBindRefs(rdcarray<ResourceId> &ids, VulkanResourceManager *rm, VkResourceRecord *record,
                   FrameRefType ref);
  void AddBindRefs(rdcarray<ResourceId> &ids, VkResourceRecord *bufView, VkResourceRecord *imgView,
                   VkResourceRecord *buffer, VkResourceRecord *descSetRecord, FrameRefType ref);

  // VkDescriptorBufferInfo
  DescriptorSetSlotBufferInfo bufferInfo;

  // VkDescriptorImageInfo
  DescriptorSetSlotImageInfo imageInfo;

  ResourceId texelBufferView;

  // inline uniform block
  uint32_t inlineOffset;
};

struct BindingStorage
{
  BindingStorage() = default;
  // disallow copy
  BindingStorage(const BindingStorage &) = delete;
  BindingStorage &operator=(const BindingStorage &) = delete;

  ~BindingStorage() { clear(); }
  bytebuf inlineBytes;
  rdcarray<DescriptorSetSlot *> binds;
  uint32_t variableDescriptorCount;

  void clear()
  {
    inlineBytes.clear();
    binds.clear();
    elems.clear();
    variableDescriptorCount = 0;
  }

  void reset()
  {
    memset(inlineBytes.data(), 0, inlineBytes.size());
    memset(elems.data(), 0, elems.byteSize());
  }

  void copy(DescriptorSetSlot *&slots, uint32_t &slotCount, byte *&inlineData, size_t &inlineSize)
  {
    slotCount = elems.count();

    slots = new DescriptorSetSlot[slotCount];
    memcpy(slots, elems.data(), sizeof(DescriptorSetSlot) * slotCount);

    inlineSize = inlineBytes.size();
    inlineData = AllocAlignedBuffer(inlineSize);
    memcpy(inlineData, inlineBytes.data(), inlineSize);
  }

private:
  rdcarray<DescriptorSetSlot> elems;
  friend struct DescSetLayout;
};

DECLARE_REFLECTION_STRUCT(DescriptorSetSlotBufferInfo);
DECLARE_REFLECTION_STRUCT(DescriptorSetSlotImageInfo);
DECLARE_REFLECTION_STRUCT(DescriptorSetSlot);

bool IsValid(bool allowNULLDescriptors, const VkWriteDescriptorSet &write, uint32_t arrayElement);

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

enum class VulkanChunk : uint32_t
{
  vkEnumeratePhysicalDevices = (uint32_t)SystemChunk::FirstDriverChunk,
  vkCreateDevice,
  vkGetDeviceQueue,
  vkAllocateMemory,
  vkUnmapMemory,
  vkFlushMappedMemoryRanges,
  vkCreateCommandPool,
  vkResetCommandPool,
  vkAllocateCommandBuffers,
  vkCreateFramebuffer,
  vkCreateRenderPass,
  vkCreateDescriptorPool,
  vkCreateDescriptorSetLayout,
  vkCreateBuffer,
  vkCreateBufferView,
  vkCreateImage,
  vkCreateImageView,
  vkCreateDepthTargetView,
  vkCreateSampler,
  vkCreateShaderModule,
  vkCreatePipelineLayout,
  vkCreatePipelineCache,
  vkCreateGraphicsPipelines,
  vkCreateComputePipelines,
  vkGetSwapchainImagesKHR,
  vkCreateSemaphore,
  vkCreateFence,
  vkGetFenceStatus,
  vkResetFences,
  vkWaitForFences,
  vkCreateEvent,
  vkGetEventStatus,
  vkSetEvent,
  vkResetEvent,
  vkCreateQueryPool,
  vkAllocateDescriptorSets,
  vkUpdateDescriptorSets,
  vkBeginCommandBuffer,
  vkEndCommandBuffer,
  vkQueueWaitIdle,
  vkDeviceWaitIdle,
  vkQueueSubmit,
  vkBindBufferMemory,
  vkBindImageMemory,
  vkQueueBindSparse,
  vkCmdBeginRenderPass,
  vkCmdNextSubpass,
  vkCmdExecuteCommands,
  vkCmdEndRenderPass,
  vkCmdBindPipeline,
  vkCmdSetViewport,
  vkCmdSetScissor,
  vkCmdSetLineWidth,
  vkCmdSetDepthBias,
  vkCmdSetBlendConstants,
  vkCmdSetDepthBounds,
  vkCmdSetStencilCompareMask,
  vkCmdSetStencilWriteMask,
  vkCmdSetStencilReference,
  vkCmdBindDescriptorSets,
  vkCmdBindVertexBuffers,
  vkCmdBindIndexBuffer,
  vkCmdCopyBufferToImage,
  vkCmdCopyImageToBuffer,
  vkCmdCopyBuffer,
  vkCmdCopyImage,
  vkCmdBlitImage,
  vkCmdResolveImage,
  vkCmdUpdateBuffer,
  vkCmdFillBuffer,
  vkCmdPushConstants,
  vkCmdClearColorImage,
  vkCmdClearDepthStencilImage,
  vkCmdClearAttachments,
  vkCmdPipelineBarrier,
  vkCmdWriteTimestamp,
  vkCmdCopyQueryPoolResults,
  vkCmdBeginQuery,
  vkCmdEndQuery,
  vkCmdResetQueryPool,
  vkCmdSetEvent,
  vkCmdResetEvent,
  vkCmdWaitEvents,
  vkCmdDraw,
  vkCmdDrawIndirect,
  vkCmdDrawIndexed,
  vkCmdDrawIndexedIndirect,
  vkCmdDispatch,
  vkCmdDispatchIndirect,
  vkCmdDebugMarkerBeginEXT,
  vkCmdDebugMarkerInsertEXT,
  vkCmdDebugMarkerEndEXT,
  vkDebugMarkerSetObjectNameEXT,
  vkCreateSwapchainKHR,
  SetShaderDebugPath,
  vkRegisterDeviceEventEXT,
  vkRegisterDisplayEventEXT,
  vkCmdIndirectSubCommand,
  vkCmdPushDescriptorSetKHR,
  vkCmdPushDescriptorSetWithTemplateKHR,
  vkCreateDescriptorUpdateTemplate,
  vkUpdateDescriptorSetWithTemplate,
  vkBindBufferMemory2,
  vkBindImageMemory2,
  vkCmdWriteBufferMarkerAMD,
  vkSetDebugUtilsObjectNameEXT,
  vkQueueBeginDebugUtilsLabelEXT,
  vkQueueEndDebugUtilsLabelEXT,
  vkQueueInsertDebugUtilsLabelEXT,
  vkCmdBeginDebugUtilsLabelEXT,
  vkCmdEndDebugUtilsLabelEXT,
  vkCmdInsertDebugUtilsLabelEXT,
  vkCreateSamplerYcbcrConversion,
  vkCmdSetDeviceMask,
  vkCmdDispatchBase,
  vkGetDeviceQueue2,
  vkCmdDrawIndirectCount,
  vkCmdDrawIndexedIndirectCount,
  vkCreateRenderPass2,
  vkCmdBeginRenderPass2,
  vkCmdNextSubpass2,
  vkCmdEndRenderPass2,
  vkCmdBindTransformFeedbackBuffersEXT,
  vkCmdBeginTransformFeedbackEXT,
  vkCmdEndTransformFeedbackEXT,
  vkCmdBeginQueryIndexedEXT,
  vkCmdEndQueryIndexedEXT,
  vkCmdDrawIndirectByteCountEXT,
  vkCmdBeginConditionalRenderingEXT,
  vkCmdEndConditionalRenderingEXT,
  vkCmdSetSampleLocationsEXT,
  vkCmdSetDiscardRectangleEXT,
  DeviceMemoryRefs,
  vkResetQueryPool,
  ImageRefs,
  vkCmdSetLineStippleEXT,
  vkGetSemaphoreCounterValue,
  vkWaitSemaphores,
  vkSignalSemaphore,
  vkQueuePresentKHR,
  vkCmdSetCullModeEXT,
  vkCmdSetFrontFaceEXT,
  vkCmdSetPrimitiveTopologyEXT,
  vkCmdSetViewportWithCountEXT,
  vkCmdSetScissorWithCountEXT,
  vkCmdBindVertexBuffers2EXT,
  vkCmdSetDepthTestEnableEXT,
  vkCmdSetDepthWriteEnableEXT,
  vkCmdSetDepthCompareOpEXT,
  vkCmdSetDepthBoundsTestEnableEXT,
  vkCmdSetStencilTestEnableEXT,
  vkCmdSetStencilOpEXT,
  CoherentMapWrite,
  Max,
};

DECLARE_REFLECTION_ENUM(VulkanChunk);

// this is special - these serialise overloads will fetch the ID during capture, serialise the ID
// directly as-if it were the original type, then on replay load up the resource if available.
// Really this is only one type of serialisation, but we declare a couple of overloads to account
// for resources being accessed through different interfaces in different functions
#define SERIALISE_VK_HANDLES()                 \
  SERIALISE_HANDLE(VkInstance)                 \
  SERIALISE_HANDLE(VkPhysicalDevice)           \
  SERIALISE_HANDLE(VkDevice)                   \
  SERIALISE_HANDLE(VkQueue)                    \
  SERIALISE_HANDLE(VkCommandBuffer)            \
  SERIALISE_HANDLE(VkFence)                    \
  SERIALISE_HANDLE(VkDeviceMemory)             \
  SERIALISE_HANDLE(VkBuffer)                   \
  SERIALISE_HANDLE(VkImage)                    \
  SERIALISE_HANDLE(VkSemaphore)                \
  SERIALISE_HANDLE(VkEvent)                    \
  SERIALISE_HANDLE(VkQueryPool)                \
  SERIALISE_HANDLE(VkBufferView)               \
  SERIALISE_HANDLE(VkImageView)                \
  SERIALISE_HANDLE(VkShaderModule)             \
  SERIALISE_HANDLE(VkPipelineCache)            \
  SERIALISE_HANDLE(VkPipelineLayout)           \
  SERIALISE_HANDLE(VkRenderPass)               \
  SERIALISE_HANDLE(VkPipeline)                 \
  SERIALISE_HANDLE(VkDescriptorSetLayout)      \
  SERIALISE_HANDLE(VkSampler)                  \
  SERIALISE_HANDLE(VkDescriptorPool)           \
  SERIALISE_HANDLE(VkDescriptorSet)            \
  SERIALISE_HANDLE(VkFramebuffer)              \
  SERIALISE_HANDLE(VkCommandPool)              \
  SERIALISE_HANDLE(VkSwapchainKHR)             \
  SERIALISE_HANDLE(VkSurfaceKHR)               \
  SERIALISE_HANDLE(VkDescriptorUpdateTemplate) \
  SERIALISE_HANDLE(VkSamplerYcbcrConversion)

#define SERIALISE_HANDLE(type) DECLARE_REFLECTION_STRUCT(type)

SERIALISE_VK_HANDLES();

// declare reflect-able types

// pNext structs - always have deserialise for the next chain
DECLARE_REFLECTION_STRUCT(VkAcquireNextImageInfoKHR);
DECLARE_REFLECTION_STRUCT(VkAcquireProfilingLockInfoKHR);
DECLARE_REFLECTION_STRUCT(VkApplicationInfo);
DECLARE_REFLECTION_STRUCT(VkAttachmentDescription2);
DECLARE_REFLECTION_STRUCT(VkAttachmentDescriptionStencilLayout);
DECLARE_REFLECTION_STRUCT(VkAttachmentReference2);
DECLARE_REFLECTION_STRUCT(VkAttachmentReferenceStencilLayout);
DECLARE_REFLECTION_STRUCT(VkAttachmentSampleLocationsEXT);
DECLARE_REFLECTION_STRUCT(VkBindBufferMemoryDeviceGroupInfo);
DECLARE_REFLECTION_STRUCT(VkBindBufferMemoryInfo);
DECLARE_REFLECTION_STRUCT(VkBindImageMemoryDeviceGroupInfo);
DECLARE_REFLECTION_STRUCT(VkBindImageMemoryInfo);
DECLARE_REFLECTION_STRUCT(VkBindImageMemorySwapchainInfoKHR);
DECLARE_REFLECTION_STRUCT(VkBindImagePlaneMemoryInfo);
DECLARE_REFLECTION_STRUCT(VkBindSparseInfo);
DECLARE_REFLECTION_STRUCT(VkBufferCreateInfo);
DECLARE_REFLECTION_STRUCT(VkBufferDeviceAddressCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkBufferDeviceAddressInfo);
DECLARE_REFLECTION_STRUCT(VkBufferMemoryBarrier);
DECLARE_REFLECTION_STRUCT(VkBufferMemoryRequirementsInfo2);
DECLARE_REFLECTION_STRUCT(VkBufferOpaqueCaptureAddressCreateInfo);
DECLARE_REFLECTION_STRUCT(VkBufferViewCreateInfo);
DECLARE_REFLECTION_STRUCT(VkCalibratedTimestampInfoEXT);
DECLARE_REFLECTION_STRUCT(VkCommandBufferAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkCommandBufferBeginInfo);
DECLARE_REFLECTION_STRUCT(VkCommandBufferInheritanceConditionalRenderingInfoEXT);
DECLARE_REFLECTION_STRUCT(VkCommandBufferInheritanceInfo);
DECLARE_REFLECTION_STRUCT(VkCommandPoolCreateInfo);
DECLARE_REFLECTION_STRUCT(VkComputePipelineCreateInfo);
DECLARE_REFLECTION_STRUCT(VkConditionalRenderingBeginInfoEXT);
DECLARE_REFLECTION_STRUCT(VkCopyDescriptorSet);
DECLARE_REFLECTION_STRUCT(VkDebugMarkerMarkerInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDebugMarkerObjectNameInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDebugMarkerObjectTagInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDebugReportCallbackCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDebugUtilsLabelEXT);
DECLARE_REFLECTION_STRUCT(VkDebugUtilsMessengerCallbackDataEXT);
DECLARE_REFLECTION_STRUCT(VkDebugUtilsMessengerCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDebugUtilsObjectNameInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDebugUtilsObjectTagInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDedicatedAllocationBufferCreateInfoNV);
DECLARE_REFLECTION_STRUCT(VkDedicatedAllocationImageCreateInfoNV);
DECLARE_REFLECTION_STRUCT(VkDedicatedAllocationMemoryAllocateInfoNV);
DECLARE_REFLECTION_STRUCT(VkDescriptorPoolCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorPoolInlineUniformBlockCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutBindingFlagsCreateInfo)
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutSupport);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetVariableDescriptorCountAllocateInfo)
DECLARE_REFLECTION_STRUCT(VkDescriptorSetVariableDescriptorCountLayoutSupport)
DECLARE_REFLECTION_STRUCT(VkDescriptorUpdateTemplateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceEventInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupBindSparseInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupCommandBufferBeginInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupDeviceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupPresentCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupPresentInfoKHR);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupRenderPassBeginInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceGroupSwapchainCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkDeviceMemoryOpaqueCaptureAddressInfo);
DECLARE_REFLECTION_STRUCT(VkDevicePrivateDataCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDeviceQueueCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceQueueGlobalPriorityCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDeviceQueueInfo2);
DECLARE_REFLECTION_STRUCT(VkDisplayEventInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDisplayModeProperties2KHR);
DECLARE_REFLECTION_STRUCT(VkDisplayNativeHdrSurfaceCapabilitiesAMD);
DECLARE_REFLECTION_STRUCT(VkDisplayPlaneCapabilities2KHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPlaneInfo2KHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPlaneProperties2KHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPowerInfoEXT);
DECLARE_REFLECTION_STRUCT(VkDisplayPresentInfoKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayProperties2KHR);
DECLARE_REFLECTION_STRUCT(VkEventCreateInfo);
DECLARE_REFLECTION_STRUCT(VkExportFenceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkExportMemoryAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkExportMemoryAllocateInfoNV);
DECLARE_REFLECTION_STRUCT(VkExportSemaphoreCreateInfo);
DECLARE_REFLECTION_STRUCT(VkExternalBufferProperties);
DECLARE_REFLECTION_STRUCT(VkExternalFenceProperties);
DECLARE_REFLECTION_STRUCT(VkExternalImageFormatProperties);
DECLARE_REFLECTION_STRUCT(VkExternalMemoryBufferCreateInfo);
DECLARE_REFLECTION_STRUCT(VkExternalMemoryImageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkExternalMemoryImageCreateInfoNV);
DECLARE_REFLECTION_STRUCT(VkExternalSemaphoreProperties);
DECLARE_REFLECTION_STRUCT(VkFenceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkFenceGetFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkFilterCubicImageViewImageFormatPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkFormatProperties2);
DECLARE_REFLECTION_STRUCT(VkFramebufferAttachmentImageInfo);
DECLARE_REFLECTION_STRUCT(VkFramebufferAttachmentsCreateInfo);
DECLARE_REFLECTION_STRUCT(VkFramebufferCreateInfo);
DECLARE_REFLECTION_STRUCT(VkGraphicsPipelineCreateInfo);
DECLARE_REFLECTION_STRUCT(VkHdrMetadataEXT);
DECLARE_REFLECTION_STRUCT(VkImageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageFormatListCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageFormatProperties2);
DECLARE_REFLECTION_STRUCT(VkImageMemoryBarrier);
DECLARE_REFLECTION_STRUCT(VkImageMemoryRequirementsInfo2);
DECLARE_REFLECTION_STRUCT(VkImagePlaneMemoryRequirementsInfo);
DECLARE_REFLECTION_STRUCT(VkImageSparseMemoryRequirementsInfo2);
DECLARE_REFLECTION_STRUCT(VkImageStencilUsageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageSwapchainCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImageViewASTCDecodeModeEXT);
DECLARE_REFLECTION_STRUCT(VkImageViewCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageViewUsageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImportFenceFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImportMemoryFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImportSemaphoreFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkInstanceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkLayerDeviceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkLayerInstanceCreateInfo);
DECLARE_REFLECTION_STRUCT(VkMappedMemoryRange);
DECLARE_REFLECTION_STRUCT(VkMemoryAllocateFlagsInfo);
DECLARE_REFLECTION_STRUCT(VkMemoryAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkMemoryBarrier);
DECLARE_REFLECTION_STRUCT(VkMemoryDedicatedAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkMemoryDedicatedRequirements);
DECLARE_REFLECTION_STRUCT(VkMemoryFdPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkMemoryGetFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkMemoryOpaqueCaptureAddressAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkMemoryPriorityAllocateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkMemoryRequirements2);
DECLARE_REFLECTION_STRUCT(VkMultisamplePropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPastPresentationTimingGOOGLE);
DECLARE_REFLECTION_STRUCT(VkPerformanceCounterDescriptionKHR);
DECLARE_REFLECTION_STRUCT(VkPerformanceCounterKHR);
DECLARE_REFLECTION_STRUCT(VkPerformanceQuerySubmitInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevice16BitStorageFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevice8BitStorageFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceASTCDecodeFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceBufferDeviceAddressFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceBufferDeviceAddressFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceCoherentMemoryFeaturesAMD);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceComputeShaderDerivativesFeaturesNV);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceConditionalRenderingFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceConservativeRasterizationPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceCustomBorderColorFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceCustomBorderColorPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDepthClipEnableFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDepthStencilResolveProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDescriptorIndexingFeatures)
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDescriptorIndexingProperties)
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDiscardRectanglePropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDriverProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalBufferInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalFenceInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalImageFormatInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalSemaphoreInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFeatures2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFloatControlsProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMapFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMapPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceGroupProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceHostQueryResetFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceIDProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImageFormatInfo2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImagelessFramebufferFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImageViewImageFormatInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceIndexTypeUint8FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceInlineUniformBlockFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceInlineUniformBlockPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceLineRasterizationFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceLineRasterizationPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMaintenance3Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryBudgetPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryPriorityFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryProperties2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMultiviewFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMultiviewProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePCIBusInfoPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePerformanceQueryFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePerformanceQueryPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePointClippingProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePrivateDataFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProperties2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProtectedMemoryFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProtectedMemoryProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePushDescriptorPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRobustness2FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRobustness2PropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSampleLocationsPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSamplerFilterMinmaxProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSamplerYcbcrConversionFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceScalarBlockLayoutFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderAtomicInt64Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderClockFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderCorePropertiesAMD);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderDrawParametersFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderFloat16Int8Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderImageFootprintFeaturesNV);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSparseImageFormatInfo2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSubgroupProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSubgroupSizeControlFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSubgroupSizeControlPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSurfaceInfo2KHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTimelineSemaphoreFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTimelineSemaphoreProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceToolPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTransformFeedbackFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTransformFeedbackPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceUniformBufferStandardLayoutFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVariablePointerFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan11Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan11Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan12Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan12Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkanMemoryModelFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineCacheCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineColorBlendStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineCreationFeedbackCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineDepthStencilStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineDiscardRectangleStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineDynamicStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableInternalRepresentationKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutablePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableStatisticKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineInputAssemblyStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineLayoutCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineMultisampleStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationConservativeStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationDepthClipStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationLineStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationStateStreamCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineSampleLocationsStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineShaderStageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineTessellationDomainOriginStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineTessellationStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineVertexInputDivisorStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineVertexInputStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineViewportStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPresentInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPresentRegionsKHR);
DECLARE_REFLECTION_STRUCT(VkPresentTimeGOOGLE);
DECLARE_REFLECTION_STRUCT(VkPresentTimesInfoGOOGLE);
DECLARE_REFLECTION_STRUCT(VkPrivateDataSlotCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkProtectedSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkQueryPoolCreateInfo);
DECLARE_REFLECTION_STRUCT(VkQueryPoolPerformanceCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkQueueFamilyProperties2);
DECLARE_REFLECTION_STRUCT(VkRefreshCycleDurationGOOGLE);
DECLARE_REFLECTION_STRUCT(VkRenderPassAttachmentBeginInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassBeginInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassCreateInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassCreateInfo2);
DECLARE_REFLECTION_STRUCT(VkRenderPassFragmentDensityMapCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkRenderPassInputAttachmentAspectCreateInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassMultiviewCreateInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassSampleLocationsBeginInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSampleLocationsInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSamplerCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSamplerCustomBorderColorCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSamplerReductionModeCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSamplerYcbcrConversionCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSamplerYcbcrConversionImageFormatProperties);
DECLARE_REFLECTION_STRUCT(VkSamplerYcbcrConversionInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreGetFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkSemaphoreSignalInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreTypeCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreWaitInfo);
DECLARE_REFLECTION_STRUCT(VkShaderModuleCreateInfo);
DECLARE_REFLECTION_STRUCT(VkShaderModuleValidationCacheCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSharedPresentSurfaceCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkSparseImageFormatProperties2);
DECLARE_REFLECTION_STRUCT(VkSparseImageMemoryRequirements2);
DECLARE_REFLECTION_STRUCT(VkSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkSubpassBeginInfo);
DECLARE_REFLECTION_STRUCT(VkSubpassDependency2);
DECLARE_REFLECTION_STRUCT(VkSubpassDescription2);
DECLARE_REFLECTION_STRUCT(VkSubpassDescriptionDepthStencilResolve);
DECLARE_REFLECTION_STRUCT(VkSubpassEndInfo);
DECLARE_REFLECTION_STRUCT(VkSubpassSampleLocationsEXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilities2EXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilities2KHR);
DECLARE_REFLECTION_STRUCT(VkSurfaceFormat2KHR);
DECLARE_REFLECTION_STRUCT(VkSurfaceProtectedCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkSwapchainCounterCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSwapchainCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkSwapchainDisplayNativeHdrCreateInfoAMD);
DECLARE_REFLECTION_STRUCT(VkTextureLODGatherFormatPropertiesAMD);
DECLARE_REFLECTION_STRUCT(VkTimelineSemaphoreSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkValidationCacheCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkValidationFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkValidationFlagsEXT);
DECLARE_REFLECTION_STRUCT(VkWriteDescriptorSet);
DECLARE_REFLECTION_STRUCT(VkWriteDescriptorSetInlineUniformBlockEXT);

DECLARE_DESERIALISE_TYPE(VkAcquireNextImageInfoKHR);
DECLARE_DESERIALISE_TYPE(VkAcquireProfilingLockInfoKHR);
DECLARE_DESERIALISE_TYPE(VkApplicationInfo);
DECLARE_DESERIALISE_TYPE(VkAttachmentDescription2);
DECLARE_DESERIALISE_TYPE(VkAttachmentDescriptionStencilLayout);
DECLARE_DESERIALISE_TYPE(VkAttachmentReference2);
DECLARE_DESERIALISE_TYPE(VkAttachmentReferenceStencilLayout);
DECLARE_DESERIALISE_TYPE(VkAttachmentSampleLocationsEXT);
DECLARE_DESERIALISE_TYPE(VkBindBufferMemoryDeviceGroupInfo);
DECLARE_DESERIALISE_TYPE(VkBindBufferMemoryInfo);
DECLARE_DESERIALISE_TYPE(VkBindImageMemoryDeviceGroupInfo);
DECLARE_DESERIALISE_TYPE(VkBindImageMemoryInfo);
DECLARE_DESERIALISE_TYPE(VkBindImageMemorySwapchainInfoKHR);
DECLARE_DESERIALISE_TYPE(VkBindImagePlaneMemoryInfo);
DECLARE_DESERIALISE_TYPE(VkBindSparseInfo);
DECLARE_DESERIALISE_TYPE(VkBufferCreateInfo);
DECLARE_DESERIALISE_TYPE(VkBufferMemoryBarrier);
DECLARE_DESERIALISE_TYPE(VkBufferMemoryRequirementsInfo2);
DECLARE_DESERIALISE_TYPE(VkBufferOpaqueCaptureAddressCreateInfo);
DECLARE_DESERIALISE_TYPE(VkBufferViewCreateInfo);
DECLARE_DESERIALISE_TYPE(VkCalibratedTimestampInfoEXT);
DECLARE_DESERIALISE_TYPE(VkCommandBufferAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkCommandBufferBeginInfo);
DECLARE_DESERIALISE_TYPE(VkCommandBufferInheritanceConditionalRenderingInfoEXT);
DECLARE_DESERIALISE_TYPE(VkCommandBufferInheritanceInfo);
DECLARE_DESERIALISE_TYPE(VkCommandPoolCreateInfo);
DECLARE_DESERIALISE_TYPE(VkComputePipelineCreateInfo);
DECLARE_DESERIALISE_TYPE(VkConditionalRenderingBeginInfoEXT);
DECLARE_DESERIALISE_TYPE(VkCopyDescriptorSet);
DECLARE_DESERIALISE_TYPE(VkDebugMarkerMarkerInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDebugMarkerObjectNameInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDebugMarkerObjectTagInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDebugReportCallbackCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDebugUtilsLabelEXT);
DECLARE_DESERIALISE_TYPE(VkDebugUtilsMessengerCallbackDataEXT);
DECLARE_DESERIALISE_TYPE(VkDebugUtilsMessengerCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDebugUtilsObjectNameInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDebugUtilsObjectTagInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDedicatedAllocationBufferCreateInfoNV);
DECLARE_DESERIALISE_TYPE(VkDedicatedAllocationImageCreateInfoNV);
DECLARE_DESERIALISE_TYPE(VkDedicatedAllocationMemoryAllocateInfoNV);
DECLARE_DESERIALISE_TYPE(VkDescriptorPoolCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorPoolInlineUniformBlockCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutBindingFlagsCreateInfo)
DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutSupport);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetVariableDescriptorCountAllocateInfo)
DECLARE_DESERIALISE_TYPE(VkDescriptorSetVariableDescriptorCountLayoutSupport)
DECLARE_DESERIALISE_TYPE(VkDescriptorUpdateTemplateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceEventInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupBindSparseInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupCommandBufferBeginInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupDeviceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupPresentCapabilitiesKHR);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupPresentInfoKHR);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupRenderPassBeginInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceGroupSwapchainCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkDeviceMemoryOpaqueCaptureAddressInfo);
DECLARE_DESERIALISE_TYPE(VkDevicePrivateDataCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDeviceQueueCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceQueueGlobalPriorityCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDeviceQueueInfo2);
DECLARE_DESERIALISE_TYPE(VkDisplayEventInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDisplayModeProperties2KHR);
DECLARE_DESERIALISE_TYPE(VkDisplayNativeHdrSurfaceCapabilitiesAMD);
DECLARE_DESERIALISE_TYPE(VkDisplayPlaneCapabilities2KHR);
DECLARE_DESERIALISE_TYPE(VkDisplayPlaneInfo2KHR);
DECLARE_DESERIALISE_TYPE(VkDisplayPlaneProperties2KHR);
DECLARE_DESERIALISE_TYPE(VkDisplayPowerInfoEXT);
DECLARE_DESERIALISE_TYPE(VkDisplayPresentInfoKHR);
DECLARE_DESERIALISE_TYPE(VkDisplayProperties2KHR);
DECLARE_DESERIALISE_TYPE(VkEventCreateInfo);
DECLARE_DESERIALISE_TYPE(VkExportFenceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkExportMemoryAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkExportMemoryAllocateInfoNV);
DECLARE_DESERIALISE_TYPE(VkExportSemaphoreCreateInfo);
DECLARE_DESERIALISE_TYPE(VkExternalBufferProperties);
DECLARE_DESERIALISE_TYPE(VkExternalFenceProperties);
DECLARE_DESERIALISE_TYPE(VkExternalImageFormatProperties);
DECLARE_DESERIALISE_TYPE(VkExternalMemoryBufferCreateInfo);
DECLARE_DESERIALISE_TYPE(VkExternalMemoryImageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkExternalMemoryImageCreateInfoNV);
DECLARE_DESERIALISE_TYPE(VkExternalSemaphoreProperties);
DECLARE_DESERIALISE_TYPE(VkFenceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkFenceGetFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkFilterCubicImageViewImageFormatPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkFormatProperties2);
DECLARE_DESERIALISE_TYPE(VkFramebufferAttachmentImageInfo);
DECLARE_DESERIALISE_TYPE(VkFramebufferAttachmentsCreateInfo);
DECLARE_DESERIALISE_TYPE(VkFramebufferCreateInfo);
DECLARE_DESERIALISE_TYPE(VkGraphicsPipelineCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageFormatListCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageFormatProperties2);
DECLARE_DESERIALISE_TYPE(VkImageMemoryBarrier);
DECLARE_DESERIALISE_TYPE(VkImageMemoryRequirementsInfo2);
DECLARE_DESERIALISE_TYPE(VkImagePlaneMemoryRequirementsInfo);
DECLARE_DESERIALISE_TYPE(VkImageSparseMemoryRequirementsInfo2);
DECLARE_DESERIALISE_TYPE(VkImageStencilUsageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageSwapchainCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImageViewASTCDecodeModeEXT);
DECLARE_DESERIALISE_TYPE(VkImageViewCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageViewUsageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImportFenceFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImportMemoryFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImportSemaphoreFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkInstanceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkLayerDeviceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkLayerInstanceCreateInfo);
DECLARE_DESERIALISE_TYPE(VkMappedMemoryRange);
DECLARE_DESERIALISE_TYPE(VkMemoryAllocateFlagsInfo);
DECLARE_DESERIALISE_TYPE(VkMemoryAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkMemoryBarrier);
DECLARE_DESERIALISE_TYPE(VkMemoryDedicatedAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkMemoryDedicatedRequirements);
DECLARE_DESERIALISE_TYPE(VkMemoryFdPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkMemoryGetFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkMemoryOpaqueCaptureAddressAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkMemoryPriorityAllocateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkMemoryRequirements2);
DECLARE_DESERIALISE_TYPE(VkMultisamplePropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPerformanceCounterDescriptionKHR);
DECLARE_DESERIALISE_TYPE(VkPerformanceCounterKHR);
DECLARE_DESERIALISE_TYPE(VkPerformanceQuerySubmitInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevice16BitStorageFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevice8BitStorageFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceASTCDecodeFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceCoherentMemoryFeaturesAMD);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceComputeShaderDerivativesFeaturesNV);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceConditionalRenderingFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceConservativeRasterizationPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceCustomBorderColorFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceCustomBorderColorPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDepthClipEnableFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDepthStencilResolveProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDescriptorIndexingFeatures)
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDescriptorIndexingProperties)
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDiscardRectanglePropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDriverProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalBufferInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalFenceInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalImageFormatInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalSemaphoreInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFeatures2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFloatControlsProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceGroupProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceHostQueryResetFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceIDProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImageFormatInfo2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImagelessFramebufferFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImageViewImageFormatInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceIndexTypeUint8FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceInlineUniformBlockFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceInlineUniformBlockPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceLineRasterizationFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceLineRasterizationPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMaintenance3Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMemoryBudgetPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMemoryPriorityFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMemoryProperties2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMultiviewFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMultiviewProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePCIBusInfoPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePerformanceQueryFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePerformanceQueryPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePointClippingProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePrivateDataFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProperties2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProtectedMemoryFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProtectedMemoryProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePushDescriptorPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRobustness2FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRobustness2PropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSampleLocationsPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSamplerFilterMinmaxProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSamplerYcbcrConversionFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceScalarBlockLayoutFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderAtomicInt64Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderClockFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderCorePropertiesAMD);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderDrawParametersFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderFloat16Int8Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderImageFootprintFeaturesNV);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSparseImageFormatInfo2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSubgroupProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSubgroupSizeControlFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSubgroupSizeControlPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSurfaceInfo2KHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTimelineSemaphoreFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTimelineSemaphoreProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceToolPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTransformFeedbackFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTransformFeedbackPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceUniformBufferStandardLayoutFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVariablePointerFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan11Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan11Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan12Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan12Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkanMemoryModelFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineCacheCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineColorBlendStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineCreationFeedbackCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineDepthStencilStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineDiscardRectangleStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineDynamicStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutableInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutableInternalRepresentationKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutablePropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutableStatisticKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineInputAssemblyStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineLayoutCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineMultisampleStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationConservativeStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationDepthClipStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationLineStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationStateStreamCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineSampleLocationsStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineShaderStageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineTessellationDomainOriginStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineTessellationStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineVertexInputDivisorStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineVertexInputStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineViewportStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPresentInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPresentRegionsKHR);
DECLARE_DESERIALISE_TYPE(VkPresentTimesInfoGOOGLE);
DECLARE_DESERIALISE_TYPE(VkPrivateDataSlotCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkProtectedSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkQueryPoolCreateInfo);
DECLARE_DESERIALISE_TYPE(VkQueryPoolPerformanceCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkQueueFamilyProperties2);
DECLARE_DESERIALISE_TYPE(VkRenderPassAttachmentBeginInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassBeginInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassCreateInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassCreateInfo2);
DECLARE_DESERIALISE_TYPE(VkRenderPassInputAttachmentAspectCreateInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassMultiviewCreateInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassSampleLocationsBeginInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSampleLocationsInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSamplerCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSamplerCustomBorderColorCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSamplerReductionModeCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSamplerYcbcrConversionCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSamplerYcbcrConversionImageFormatProperties);
DECLARE_DESERIALISE_TYPE(VkSamplerYcbcrConversionInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreGetFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkSemaphoreSignalInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreTypeCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreWaitInfo);
DECLARE_DESERIALISE_TYPE(VkShaderModuleCreateInfo);
DECLARE_DESERIALISE_TYPE(VkShaderModuleValidationCacheCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSharedPresentSurfaceCapabilitiesKHR);
DECLARE_DESERIALISE_TYPE(VkSparseImageFormatProperties2);
DECLARE_DESERIALISE_TYPE(VkSparseImageMemoryRequirements2);
DECLARE_DESERIALISE_TYPE(VkSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkSubpassBeginInfo);
DECLARE_DESERIALISE_TYPE(VkSubpassDependency2);
DECLARE_DESERIALISE_TYPE(VkSubpassDescription2);
DECLARE_DESERIALISE_TYPE(VkSubpassDescriptionDepthStencilResolve);
DECLARE_DESERIALISE_TYPE(VkSubpassEndInfo);
DECLARE_DESERIALISE_TYPE(VkSubpassSampleLocationsEXT);
DECLARE_DESERIALISE_TYPE(VkSurfaceCapabilities2EXT);
DECLARE_DESERIALISE_TYPE(VkSurfaceCapabilities2KHR);
DECLARE_DESERIALISE_TYPE(VkSurfaceFormat2KHR);
DECLARE_DESERIALISE_TYPE(VkSurfaceProtectedCapabilitiesKHR);
DECLARE_DESERIALISE_TYPE(VkSwapchainCounterCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSwapchainCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkSwapchainDisplayNativeHdrCreateInfoAMD);
DECLARE_DESERIALISE_TYPE(VkTextureLODGatherFormatPropertiesAMD);
DECLARE_DESERIALISE_TYPE(VkTimelineSemaphoreSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkValidationCacheCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkValidationFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkValidationFlagsEXT);
DECLARE_DESERIALISE_TYPE(VkWriteDescriptorSet);
DECLARE_DESERIALISE_TYPE(VkWriteDescriptorSetInlineUniformBlockEXT);

// plain structs with no next chain
DECLARE_REFLECTION_STRUCT(VkAllocationCallbacks);
DECLARE_REFLECTION_STRUCT(VkAttachmentDescription);
DECLARE_REFLECTION_STRUCT(VkAttachmentReference);
DECLARE_REFLECTION_STRUCT(VkBufferCopy);
DECLARE_REFLECTION_STRUCT(VkBufferImageCopy);
DECLARE_REFLECTION_STRUCT(VkClearAttachment);
DECLARE_REFLECTION_STRUCT(VkClearColorValue);
DECLARE_REFLECTION_STRUCT(VkClearDepthStencilValue);
DECLARE_REFLECTION_STRUCT(VkClearRect);
DECLARE_REFLECTION_STRUCT(VkClearValue);
DECLARE_REFLECTION_STRUCT(VkComponentMapping);
DECLARE_REFLECTION_STRUCT(VkConformanceVersion);
DECLARE_REFLECTION_STRUCT(VkDescriptorBufferInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorImageInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorPoolSize);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutBinding);
DECLARE_REFLECTION_STRUCT(VkDescriptorUpdateTemplateEntry);
DECLARE_REFLECTION_STRUCT(VkDispatchIndirectCommand);
DECLARE_REFLECTION_STRUCT(VkDisplayModeParametersKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayModePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPlaneCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPlanePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkDrawIndexedIndirectCommand);
DECLARE_REFLECTION_STRUCT(VkDrawIndirectCommand);
DECLARE_REFLECTION_STRUCT(VkExtent2D);
DECLARE_REFLECTION_STRUCT(VkExtent3D);
DECLARE_REFLECTION_STRUCT(VkExternalMemoryProperties);
DECLARE_REFLECTION_STRUCT(VkFormatProperties);
DECLARE_REFLECTION_STRUCT(VkImageBlit);
DECLARE_REFLECTION_STRUCT(VkImageCopy);
DECLARE_REFLECTION_STRUCT(VkImageFormatProperties);
DECLARE_REFLECTION_STRUCT(VkImageResolve);
DECLARE_REFLECTION_STRUCT(VkImageSubresource);
DECLARE_REFLECTION_STRUCT(VkImageSubresourceLayers);
DECLARE_REFLECTION_STRUCT(VkImageSubresourceRange);
DECLARE_REFLECTION_STRUCT(VkInputAttachmentAspectReference);
DECLARE_REFLECTION_STRUCT(VkMemoryHeap);
DECLARE_REFLECTION_STRUCT(VkMemoryRequirements);
DECLARE_REFLECTION_STRUCT(VkMemoryType);
DECLARE_REFLECTION_STRUCT(VkOffset2D);
DECLARE_REFLECTION_STRUCT(VkOffset3D);
DECLARE_REFLECTION_STRUCT(VkPerformanceCounterResultKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceLimits);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSparseProperties);
DECLARE_REFLECTION_STRUCT(VkPipelineColorBlendAttachmentState);
DECLARE_REFLECTION_STRUCT(VkPipelineCreationFeedbackEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableStatisticValueKHR);
DECLARE_REFLECTION_STRUCT(VkPresentRegionKHR);
DECLARE_REFLECTION_STRUCT(VkPushConstantRange);
DECLARE_REFLECTION_STRUCT(VkQueueFamilyProperties);
DECLARE_REFLECTION_STRUCT(VkRect2D);
DECLARE_REFLECTION_STRUCT(VkRectLayerKHR);
DECLARE_REFLECTION_STRUCT(VkSampleLocationEXT);
DECLARE_REFLECTION_STRUCT(VkSparseBufferMemoryBindInfo);
DECLARE_REFLECTION_STRUCT(VkSparseImageFormatProperties);
DECLARE_REFLECTION_STRUCT(VkSparseImageMemoryBind);
DECLARE_REFLECTION_STRUCT(VkSparseImageMemoryBindInfo);
DECLARE_REFLECTION_STRUCT(VkSparseImageMemoryRequirements);
DECLARE_REFLECTION_STRUCT(VkSparseImageOpaqueMemoryBindInfo);
DECLARE_REFLECTION_STRUCT(VkSparseMemoryBind);
DECLARE_REFLECTION_STRUCT(VkSpecializationInfo);
DECLARE_REFLECTION_STRUCT(VkSpecializationMapEntry);
DECLARE_REFLECTION_STRUCT(VkStencilOpState);
DECLARE_REFLECTION_STRUCT(VkSubpassDependency);
DECLARE_REFLECTION_STRUCT(VkSubpassDescription);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkSurfaceFormatKHR);
DECLARE_REFLECTION_STRUCT(VkVertexInputAttributeDescription);
DECLARE_REFLECTION_STRUCT(VkVertexInputBindingDescription);
DECLARE_REFLECTION_STRUCT(VkVertexInputBindingDivisorDescriptionEXT);
DECLARE_REFLECTION_STRUCT(VkViewport);
DECLARE_REFLECTION_STRUCT(VkXYColorEXT);

DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutBinding);
DECLARE_DESERIALISE_TYPE(VkPresentRegionKHR);
DECLARE_DESERIALISE_TYPE(VkSparseBufferMemoryBindInfo);
DECLARE_DESERIALISE_TYPE(VkSparseImageMemoryBindInfo);
DECLARE_DESERIALISE_TYPE(VkSparseImageOpaqueMemoryBindInfo);
DECLARE_DESERIALISE_TYPE(VkSpecializationInfo);
DECLARE_DESERIALISE_TYPE(VkSubpassDescription);

// win32 only structs
#ifdef VK_USE_PLATFORM_WIN32_KHR
DECLARE_REFLECTION_STRUCT(VkD3D12FenceSubmitInfoKHR);
DECLARE_REFLECTION_STRUCT(VkExportFenceWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkExportMemoryWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkExportMemoryWin32HandleInfoNV);
DECLARE_REFLECTION_STRUCT(VkExportSemaphoreWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkFenceGetWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImportFenceWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImportMemoryWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImportMemoryWin32HandleInfoNV);
DECLARE_REFLECTION_STRUCT(VkImportSemaphoreWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkMemoryGetWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkMemoryWin32HandlePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkSemaphoreGetWin32HandleInfoKHR);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilitiesFullScreenExclusiveEXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceFullScreenExclusiveInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceFullScreenExclusiveWin32InfoEXT);
DECLARE_REFLECTION_STRUCT(VkWin32KeyedMutexAcquireReleaseInfoKHR);
DECLARE_REFLECTION_STRUCT(VkWin32KeyedMutexAcquireReleaseInfoNV);

DECLARE_DESERIALISE_TYPE(VkD3D12FenceSubmitInfoKHR);
DECLARE_DESERIALISE_TYPE(VkExportFenceWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkExportMemoryWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkExportMemoryWin32HandleInfoNV);
DECLARE_DESERIALISE_TYPE(VkExportSemaphoreWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkFenceGetWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImportFenceWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImportMemoryWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImportMemoryWin32HandleInfoNV);
DECLARE_DESERIALISE_TYPE(VkImportSemaphoreWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkMemoryGetWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkMemoryWin32HandlePropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkSemaphoreGetWin32HandleInfoKHR);
DECLARE_DESERIALISE_TYPE(VkSurfaceCapabilitiesFullScreenExclusiveEXT)
DECLARE_DESERIALISE_TYPE(VkSurfaceFullScreenExclusiveInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSurfaceFullScreenExclusiveWin32InfoEXT);
DECLARE_DESERIALISE_TYPE(VkWin32KeyedMutexAcquireReleaseInfoKHR);
DECLARE_DESERIALISE_TYPE(VkWin32KeyedMutexAcquireReleaseInfoNV);
#endif

// android only structs
#ifdef VK_USE_PLATFORM_ANDROID_KHR
DECLARE_REFLECTION_STRUCT(VkAndroidHardwareBufferUsageANDROID);
DECLARE_REFLECTION_STRUCT(VkAndroidHardwareBufferPropertiesANDROID);
DECLARE_REFLECTION_STRUCT(VkAndroidHardwareBufferFormatPropertiesANDROID);
DECLARE_REFLECTION_STRUCT(VkImportAndroidHardwareBufferInfoANDROID);
DECLARE_REFLECTION_STRUCT(VkMemoryGetAndroidHardwareBufferInfoANDROID);
DECLARE_REFLECTION_STRUCT(VkExternalFormatANDROID);

DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferUsageANDROID);
DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferPropertiesANDROID);
DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferFormatPropertiesANDROID);
DECLARE_DESERIALISE_TYPE(VkImportAndroidHardwareBufferInfoANDROID);
DECLARE_DESERIALISE_TYPE(VkMemoryGetAndroidHardwareBufferInfoANDROID);
DECLARE_DESERIALISE_TYPE(VkExternalFormatANDROID);
#endif

// enums

DECLARE_REFLECTION_ENUM(VkAccessFlagBits);
DECLARE_REFLECTION_ENUM(VkAcquireProfilingLockFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkAttachmentDescriptionFlagBits);
DECLARE_REFLECTION_ENUM(VkAttachmentLoadOp);
DECLARE_REFLECTION_ENUM(VkAttachmentStoreOp);
DECLARE_REFLECTION_ENUM(VkBlendFactor);
DECLARE_REFLECTION_ENUM(VkBlendOp);
DECLARE_REFLECTION_ENUM(VkBorderColor);
DECLARE_REFLECTION_ENUM(VkBufferCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkBufferUsageFlagBits);
DECLARE_REFLECTION_ENUM(VkChromaLocation);
DECLARE_REFLECTION_ENUM(VkColorComponentFlagBits);
DECLARE_REFLECTION_ENUM(VkColorSpaceKHR);
DECLARE_REFLECTION_ENUM(VkCommandBufferLevel);
DECLARE_REFLECTION_ENUM(VkCommandBufferUsageFlagBits);
DECLARE_REFLECTION_ENUM(VkCommandPoolCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkCommandPoolResetFlagBits);
DECLARE_REFLECTION_ENUM(VkCompareOp);
DECLARE_REFLECTION_ENUM(VkComponentSwizzle);
DECLARE_REFLECTION_ENUM(VkCompositeAlphaFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkConditionalRenderingFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkConservativeRasterizationModeEXT);
DECLARE_REFLECTION_ENUM(VkCullModeFlagBits);
DECLARE_REFLECTION_ENUM(VkDebugReportFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkDebugUtilsMessageSeverityFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkDebugUtilsMessageTypeFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkDependencyFlagBits);
DECLARE_REFLECTION_ENUM(VkDescriptorBindingFlagBits);
DECLARE_REFLECTION_ENUM(VkDescriptorPoolCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkDescriptorSetLayoutCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkDescriptorType);
DECLARE_REFLECTION_ENUM(VkDescriptorUpdateTemplateType);
DECLARE_REFLECTION_ENUM(VkDeviceEventTypeEXT);
DECLARE_REFLECTION_ENUM(VkDeviceGroupPresentModeFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkDeviceQueueCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkDiscardRectangleModeEXT);
DECLARE_REFLECTION_ENUM(VkDisplayEventTypeEXT);
DECLARE_REFLECTION_ENUM(VkDisplayPlaneAlphaFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkDisplayPowerStateEXT);
DECLARE_REFLECTION_ENUM(VkDriverId);
DECLARE_REFLECTION_ENUM(VkDynamicState);
DECLARE_REFLECTION_ENUM(VkExternalFenceFeatureFlagBits);
DECLARE_REFLECTION_ENUM(VkExternalFenceHandleTypeFlagBits);
DECLARE_REFLECTION_ENUM(VkExternalMemoryFeatureFlagBits);
DECLARE_REFLECTION_ENUM(VkExternalMemoryHandleTypeFlagBits);
DECLARE_REFLECTION_ENUM(VkExternalMemoryHandleTypeFlagBitsNV);
DECLARE_REFLECTION_ENUM(VkExternalSemaphoreFeatureFlagBits);
DECLARE_REFLECTION_ENUM(VkExternalSemaphoreHandleTypeFlagBits);
DECLARE_REFLECTION_ENUM(VkFenceCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkFenceImportFlagBits);
DECLARE_REFLECTION_ENUM(VkFilter);
DECLARE_REFLECTION_ENUM(VkFlagWithNoBits);
DECLARE_REFLECTION_ENUM(VkFormat);
DECLARE_REFLECTION_ENUM(VkFormatFeatureFlagBits);
DECLARE_REFLECTION_ENUM(VkFramebufferCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkFrontFace);
DECLARE_REFLECTION_ENUM(VkImageAspectFlagBits);
DECLARE_REFLECTION_ENUM(VkImageCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkImageLayout);
DECLARE_REFLECTION_ENUM(VkImageTiling);
DECLARE_REFLECTION_ENUM(VkImageType);
DECLARE_REFLECTION_ENUM(VkImageUsageFlagBits);
DECLARE_REFLECTION_ENUM(VkImageViewCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkImageViewType);
DECLARE_REFLECTION_ENUM(VkIndexType);
DECLARE_REFLECTION_ENUM(VkLineRasterizationModeEXT);
DECLARE_REFLECTION_ENUM(VkLogicOp);
DECLARE_REFLECTION_ENUM(VkMemoryAllocateFlagBits);
DECLARE_REFLECTION_ENUM(VkMemoryHeapFlagBits);
DECLARE_REFLECTION_ENUM(VkMemoryPropertyFlagBits);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterDescriptionFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterScopeKHR);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterStorageKHR);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterUnitKHR);
DECLARE_REFLECTION_ENUM(VkPhysicalDeviceType);
DECLARE_REFLECTION_ENUM(VkPipelineBindPoint);
DECLARE_REFLECTION_ENUM(VkPipelineCacheCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineCreationFeedbackFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkPipelineExecutableStatisticFormatKHR);
DECLARE_REFLECTION_ENUM(VkPipelineShaderStageCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineStageFlagBits);
DECLARE_REFLECTION_ENUM(VkPointClippingBehavior);
DECLARE_REFLECTION_ENUM(VkPolygonMode);
DECLARE_REFLECTION_ENUM(VkPresentModeKHR);
DECLARE_REFLECTION_ENUM(VkPrimitiveTopology);
DECLARE_REFLECTION_ENUM(VkPrivateDataSlotCreateFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkQueryControlFlagBits);
DECLARE_REFLECTION_ENUM(VkQueryPipelineStatisticFlagBits);
DECLARE_REFLECTION_ENUM(VkQueryResultFlagBits);
DECLARE_REFLECTION_ENUM(VkQueryType);
DECLARE_REFLECTION_ENUM(VkQueueFlagBits);
DECLARE_REFLECTION_ENUM(VkQueueGlobalPriorityEXT);
DECLARE_REFLECTION_ENUM(VkRenderPassCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkResolveModeFlagBits);
DECLARE_REFLECTION_ENUM(VkResult);
DECLARE_REFLECTION_ENUM(VkSampleCountFlagBits);
DECLARE_REFLECTION_ENUM(VkSamplerAddressMode);
DECLARE_REFLECTION_ENUM(VkSamplerCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkSamplerMipmapMode);
DECLARE_REFLECTION_ENUM(VkSamplerReductionMode);
DECLARE_REFLECTION_ENUM(VkSamplerYcbcrModelConversion);
DECLARE_REFLECTION_ENUM(VkSamplerYcbcrRange);
DECLARE_REFLECTION_ENUM(VkSemaphoreImportFlagBits);
DECLARE_REFLECTION_ENUM(VkSemaphoreType);
DECLARE_REFLECTION_ENUM(VkSemaphoreWaitFlagBits);
DECLARE_REFLECTION_ENUM(VkShaderFloatControlsIndependence);
DECLARE_REFLECTION_ENUM(VkShaderModuleCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkShaderStageFlagBits);
DECLARE_REFLECTION_ENUM(VkSharingMode);
DECLARE_REFLECTION_ENUM(VkSparseImageFormatFlagBits);
DECLARE_REFLECTION_ENUM(VkSparseMemoryBindFlagBits);
DECLARE_REFLECTION_ENUM(VkStencilFaceFlagBits);
DECLARE_REFLECTION_ENUM(VkStencilOp);
DECLARE_REFLECTION_ENUM(VkStructureType);
DECLARE_REFLECTION_ENUM(VkSubgroupFeatureFlagBits);
DECLARE_REFLECTION_ENUM(VkSubpassContents);
DECLARE_REFLECTION_ENUM(VkSubpassDescriptionFlagBits);
DECLARE_REFLECTION_ENUM(VkSurfaceCounterFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkSurfaceTransformFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkSwapchainCreateFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkTessellationDomainOrigin);
DECLARE_REFLECTION_ENUM(VkTimeDomainEXT);
DECLARE_REFLECTION_ENUM(VkToolPurposeFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkValidationCheckEXT);
DECLARE_REFLECTION_ENUM(VkValidationFeatureDisableEXT);
DECLARE_REFLECTION_ENUM(VkValidationFeatureEnableEXT);
DECLARE_REFLECTION_ENUM(VkVertexInputRate);

// win32 only enums
#ifdef VK_USE_PLATFORM_WIN32_KHR
DECLARE_REFLECTION_ENUM(VkFullScreenExclusiveEXT);
#endif
