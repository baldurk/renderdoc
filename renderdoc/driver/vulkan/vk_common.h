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
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(obj) \
  struct obj                                   \
  {                                            \
    obj() : handle(0)                          \
    {                                          \
    }                                          \
    obj(uint64_t x) : handle(x)                \
    {                                          \
    }                                          \
    bool operator==(const obj &other) const    \
    {                                          \
      return handle == other.handle;           \
    }                                          \
    bool operator<(const obj &other) const     \
    {                                          \
      return handle < other.handle;            \
    }                                          \
    bool operator!=(const obj &other) const    \
    {                                          \
      return handle != other.handle;           \
    }                                          \
    uint64_t handle;                           \
  };
#define VK_NON_DISPATCHABLE_WRAPPER_STRUCT

#endif

// set up these defines so that vulkan_core.h doesn't trash the enum names we want to define as real
// 64-bit enums
#define VkAccessFlagBits2 VkAccessFlagBits2_VkFlags64_typedef
#define VkPipelineStageFlagBits2 VkPipelineStageFlagBits2_VkFlags64_typedef
#define VkFormatFeatureFlagBits2 VkFormatFeatureFlagBits2_VkFlags64_typedef

#include "core/core.h"
#include "core/resource_manager.h"
#include "official/vk_layer.h"
#include "official/vulkan.h"
#include "serialise/serialiser.h"
#include "vk_dispatchtables.h"

#undef VkAccessFlagBits2
#undef VkPipelineStageFlagBits2
#undef VkFormatFeatureFlagBits2

#undef Bool
#undef None

typedef VkBufferDeviceAddressInfo VkBufferDeviceAddressInfoEXT;
typedef VkPhysicalDeviceBufferDeviceAddressFeatures VkPhysicalDeviceBufferDeviceAddressFeaturesKHR;

// enable this to get verbose debugging about when/where/why partial command buffer replay is
// happening
#define VERBOSE_PARTIAL_REPLAY OPTION_OFF

// UUID shared with VR runtimes to specify which vkImage is currently presented to the screen
#define VR_ThumbnailTag_UUID 0x94F5B9E495BCC552ULL

ResourceFormat MakeResourceFormat(VkFormat fmt);
VkFormat MakeVkFormat(ResourceFormat fmt);
Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints);
VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo);
AddressMode MakeAddressMode(VkSamplerAddressMode addr);
void MakeBorderColor(VkBorderColor border, rdcfixedarray<float, 4> &BorderColor);
CompareFunction MakeCompareFunc(VkCompareOp func);
FilterMode MakeFilterMode(VkFilter f);
TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
                         bool anisoEnable, bool compareEnable, VkSamplerReductionMode reduction);
LogicOperation MakeLogicOp(VkLogicOp op);
BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend);
BlendOperation MakeBlendOp(VkBlendOp op);
StencilOperation MakeStencilOp(VkStencilOp op);
rdcstr HumanDriverName(VkDriverId driverId);

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
VkShaderStageFlags ShaderMaskFromIndex(size_t index);

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

  static VkDevice GetDev();

  VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;

  static WrappedVulkan *vk;
};

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

void GetPhysicalDeviceDriverProperties(VkInstDispatchTable *instDispatchTable,
                                       VkPhysicalDevice unwrappedPhysicalDevice,
                                       VkPhysicalDeviceDriverProperties &driverProps);

class VkDriverInfo
{
public:
  GPUVendor Vendor() const { return m_Vendor; }
  uint32_t Major() const { return m_Major; }
  uint32_t Minor() const { return m_Minor; }
  uint32_t Patch() const { return m_Patch; }
  VkDriverInfo(bool){};
  VkDriverInfo(const VkPhysicalDeviceProperties &physProps,
               const VkPhysicalDeviceDriverProperties &driverProps, bool active = false);

  bool operator==(const VkDriverInfo &o) const
  {
    return m_Vendor == o.m_Vendor && m_Major == o.m_Major && m_Minor == o.m_Minor &&
           m_Patch == o.m_Patch;
  }

  // checks for when we're running on metal and some non-queryable things aren't supported
  bool RunningOnMetal() const { return metalBackend; }
  // A workaround for a couple of bugs, removing texelFetch use from shaders.
  // It means broken functionality but at least no instant crashes
  bool TexelFetchBrokenDriver() const { return texelFetchBrokenDriver; }
  // Many drivers have issues with KHR_buffer_device_address :(
  bool BufferDeviceAddressBrokenDriver() const { return bdaBrokenDriver; }
  // Older AMD driver versions could sometimes cause image memory requirements to vary randomly
  // between identical images. This means the memory required at capture could be less than at
  // replay. To counteract this, on drivers with this issue we pad out the memory requirements
  // enough to account for the change
  bool AMDUnreliableImageMemoryRequirements() const { return amdUnreliableImgMemReqs; }
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
  // On Qualcomm calling vkCmdSetLineWidth() before binding and dispatching a compute shader will
  // crash in vkCmdDispatch. This works around the problem by avoiding setting that dynamic state
  // unless there's a graphics pipeline when doing a partial replay, and it's unlikely the user will
  // hit the case where it's necessary (doing 'whole pass' partial replay of a subsection of a
  // command buffer where we need to apply dynamic state from earlier in the command buffer).
  bool QualcommLineWidthDynamicStateCrash() const { return qualcommLineWidthCrash; }
  // on Intel, occlusion queries are broken unless the shader has some effects. When we don't want
  // it to have visible effects during pixel history we have to insert some manual side-effects
  bool IntelBrokenOcclusionQueries() const { return intelBrokenOcclusionQueries; }
  // on NV binding a static pipeline does not re-set static state which may have been perturbed by
  // dynamic state setting, if the previous bound pipeline was identical.
  // to work around this, whenever we are setting state and we do not have a pipeline to bind, we
  // bind a dummy pipeline to ensure the pipeline always changes when we are setting dynamic state.
  // If we do have a pipeline to bind, we should never be perturbing dynamic state in between static
  // pipeline binds.
  bool NVStaticPipelineRebindStates() const { return nvidiaStaticPipelineRebindStates; }
  // On Mali there are some known issues regarding acceleration structure serialisation to device
  // memory, for the affected driver versions we switch to the host command variants
  bool MaliBrokenASDeviceSerialisation() const { return maliBrokenASDeviceSerialisation; }
private:
  GPUVendor m_Vendor;

  uint32_t m_Major, m_Minor, m_Patch;

  bool metalBackend = false;
  bool texelFetchBrokenDriver = false;
  bool bdaBrokenDriver = false;
  bool amdUnreliableImgMemReqs = false;
  bool amdStorageMSAABrokenDriver = false;
  bool qualcommLeakingUBOOffsets = false;
  bool qualcommDrefNon2DCompileCrash = false;
  bool qualcommLineWidthCrash = false;
  bool intelBrokenOcclusionQueries = false;
  bool nvidiaStaticPipelineRebindStates = false;
  bool maliBrokenASDeviceSerialisation = false;
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

// global per-chain flags to use in a double-pass processing
struct NextChainFlags
{
  // VkPipelineRenderingCreateInfoKHR provides a list of formats which is normally valid except if
  // we're creating a pipeline library without the fragment output interface. We need to detect that
  // first before processing it
  bool dynRenderingFormatsValid = true;
};

size_t GetNextPatchSize(const void *next);
void PreprocessNextChain(const VkBaseInStructure *nextInput, NextChainFlags &nextChainFlags);
void UnwrapNextChain(CaptureState state, const char *structName, byte *&tempMem,
                     VkBaseInStructure *infoStruct);
void CopyNextChainForPatching(const char *structName, byte *&tempMem, VkBaseInStructure *infoStruct);

template <typename VkStruct>
VkStruct *UnwrapStructAndChain(CaptureState state, byte *&tempMem, const VkStruct *base)
{
  VkBaseInStructure dummy;
  dummy.pNext = (const VkBaseInStructure *)base;

  UnwrapNextChain(state, TypeName<VkStruct>().c_str(), tempMem, &dummy);

  return (VkStruct *)dummy.pNext;
}

template <typename VkStruct>
void AppendNextStruct(VkStruct &base, void *newStruct)
{
  VkBaseOutStructure *next = (VkBaseOutStructure *)&base;

  while(next->pNext)
    next = next->pNext;

  next->pNext = (VkBaseOutStructure *)newStruct;
}

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

// gcc is buggy garbage, and if these enums are compiled as uint64_t it will warn (until gcc 9) that
// the enum won't fit in an 8-bit bitfield.
// As a workaround we use pragma push to pack an explicit uint8_t into a uint64_t:48

#if defined(__GNUC__) && (__GNUC__ < 10)

#define EnumBaseType uint8_t
#define GCC_WORKAROUND 1

#else

#define EnumBaseType uint64_t
#define GCC_WORKAROUND 0

#endif

// we inherit from uint64_t to make this more bitfield-able but we intend for this to fit in uint8_t
enum class DescriptorSlotType : EnumBaseType
{
  // we want an unwritten type as 0 so that zero-initialised descriptors that haven't been written
  // don't look like samplers, so these unfortunately don't match VkDescriptorType in value.
  Unwritten = 0,
  Sampler,
  CombinedImageSampler,
  SampledImage,
  StorageImage,
  UniformTexelBuffer,
  StorageTexelBuffer,
  UniformBuffer,
  StorageBuffer,
  UniformBufferDynamic,
  StorageBufferDynamic,
  InputAttachment,
  InlineBlock,
  AccelerationStructure,
  Count,
};

FrameRefType GetRefType(DescriptorSlotType descType);

constexpr VkDescriptorType convert(DescriptorSlotType type)
{
  return type == DescriptorSlotType::Unwritten ? VK_DESCRIPTOR_TYPE_MAX_ENUM
         : type == DescriptorSlotType::Sampler ? VK_DESCRIPTOR_TYPE_SAMPLER
         : type == DescriptorSlotType::CombinedImageSampler
             ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
         : type == DescriptorSlotType::SampledImage       ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
         : type == DescriptorSlotType::StorageImage       ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
         : type == DescriptorSlotType::UniformTexelBuffer ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
         : type == DescriptorSlotType::StorageTexelBuffer ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
         : type == DescriptorSlotType::UniformBuffer      ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
         : type == DescriptorSlotType::StorageBuffer      ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
         : type == DescriptorSlotType::UniformBufferDynamic
             ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
         : type == DescriptorSlotType::StorageBufferDynamic
             ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
         : type == DescriptorSlotType::InputAttachment ? VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
         : type == DescriptorSlotType::InlineBlock     ? VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK
         : type == DescriptorSlotType::AccelerationStructure
             ? VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
             : VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

constexpr DescriptorSlotType convert(VkDescriptorType type)
{
  return type == VK_DESCRIPTOR_TYPE_SAMPLER ? DescriptorSlotType::Sampler
         : type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
             ? DescriptorSlotType::CombinedImageSampler
         : type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE        ? DescriptorSlotType::SampledImage
         : type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE        ? DescriptorSlotType::StorageImage
         : type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ? DescriptorSlotType::UniformTexelBuffer
         : type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ? DescriptorSlotType::StorageTexelBuffer
         : type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER       ? DescriptorSlotType::UniformBuffer
         : type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER       ? DescriptorSlotType::StorageBuffer
         : type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
             ? DescriptorSlotType::UniformBufferDynamic
         : type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
             ? DescriptorSlotType::StorageBufferDynamic
         : type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT     ? DescriptorSlotType::InputAttachment
         : type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK ? DescriptorSlotType::InlineBlock
         : type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
             ? DescriptorSlotType::AccelerationStructure
             : DescriptorSlotType::Unwritten;
}

enum class DescriptorSlotImageLayout : EnumBaseType
{
  // these match the core types
  Undefined = 0,
  General = 1,
  ColorAttach = 2,
  DepthStencilAttach = 3,
  DepthStencilRead = 4,
  ShaderRead = 5,
  TransferSrc = 6,
  TransferDst = 7,
  Preinit = 8,
  // these are extensions
  DepthReadStencilAttach,
  DepthAttachStencilRead,
  DepthAttach,
  DepthRead,
  StencilAttach,
  StencilRead,
  Read,
  Attach,
  Present,
  SharedPresent,
  FragmentDensity,
  FragmentShadingRate,
  FeedbackLoop,
  DynamicLocalRead,

  Count,
};

constexpr VkImageLayout convert(DescriptorSlotImageLayout layout)
{
  // temporarily disable clang-format to make this more readable.
  // Ideally we'd use a simple switch() but VS2015 doesn't support that :(.
  // clang-format off
  return layout == DescriptorSlotImageLayout::Undefined              ? VK_IMAGE_LAYOUT_UNDEFINED
       : layout == DescriptorSlotImageLayout::General                ? VK_IMAGE_LAYOUT_GENERAL
       : layout == DescriptorSlotImageLayout::ColorAttach            ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
       : layout == DescriptorSlotImageLayout::DepthStencilAttach     ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
       : layout == DescriptorSlotImageLayout::DepthStencilRead       ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
       : layout == DescriptorSlotImageLayout::ShaderRead             ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
       : layout == DescriptorSlotImageLayout::TransferSrc            ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
       : layout == DescriptorSlotImageLayout::TransferDst            ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
       : layout == DescriptorSlotImageLayout::Preinit                ? VK_IMAGE_LAYOUT_PREINITIALIZED
       : layout == DescriptorSlotImageLayout::DepthReadStencilAttach ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
       : layout == DescriptorSlotImageLayout::DepthAttachStencilRead ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
       : layout == DescriptorSlotImageLayout::DepthAttach            ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
       : layout == DescriptorSlotImageLayout::DepthRead              ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
       : layout == DescriptorSlotImageLayout::StencilAttach          ? VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL
       : layout == DescriptorSlotImageLayout::StencilRead            ? VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL
       : layout == DescriptorSlotImageLayout::Read                   ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
       : layout == DescriptorSlotImageLayout::Attach                 ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
       : layout == DescriptorSlotImageLayout::Present                ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
       : layout == DescriptorSlotImageLayout::SharedPresent          ? VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
       : layout == DescriptorSlotImageLayout::FragmentDensity        ? VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
       : layout == DescriptorSlotImageLayout::FragmentShadingRate    ? VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
       : layout == DescriptorSlotImageLayout::FeedbackLoop           ? VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
       : layout == DescriptorSlotImageLayout::DynamicLocalRead       ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR
       : VK_IMAGE_LAYOUT_MAX_ENUM;
  // clang-format on
}

constexpr DescriptorSlotImageLayout convert(VkImageLayout layout)
{
  // temporarily disable clang-format to make this more readable.
  // Ideally we'd use a simple switch() but VS2015 doesn't support that :(.
  // clang-format off
  return layout == VK_IMAGE_LAYOUT_UNDEFINED                                    ? DescriptorSlotImageLayout::Undefined
       : layout == VK_IMAGE_LAYOUT_GENERAL                                      ? DescriptorSlotImageLayout::General
       : layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL                     ? DescriptorSlotImageLayout::ColorAttach
       : layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL             ? DescriptorSlotImageLayout::DepthStencilAttach
       : layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL              ? DescriptorSlotImageLayout::DepthStencilRead
       : layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL                     ? DescriptorSlotImageLayout::ShaderRead
       : layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL                         ? DescriptorSlotImageLayout::TransferSrc
       : layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL                         ? DescriptorSlotImageLayout::TransferDst
       : layout == VK_IMAGE_LAYOUT_PREINITIALIZED                               ? DescriptorSlotImageLayout::Preinit
       : layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL   ? DescriptorSlotImageLayout::DepthReadStencilAttach
       : layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL   ? DescriptorSlotImageLayout::DepthAttachStencilRead
       : layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL                     ? DescriptorSlotImageLayout::DepthAttach
       : layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL                      ? DescriptorSlotImageLayout::DepthRead
       : layout == VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL                   ? DescriptorSlotImageLayout::StencilAttach
       : layout == VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL                    ? DescriptorSlotImageLayout::StencilRead
       : layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL                            ? DescriptorSlotImageLayout::Read
       : layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL                           ? DescriptorSlotImageLayout::Attach
       : layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR                              ? DescriptorSlotImageLayout::Present
       : layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR                           ? DescriptorSlotImageLayout::SharedPresent
       : layout == VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT             ? DescriptorSlotImageLayout::FragmentDensity
       : layout == VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR ? DescriptorSlotImageLayout::FragmentShadingRate
       : layout == VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT         ? DescriptorSlotImageLayout::FeedbackLoop
       : layout == VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR                     ? DescriptorSlotImageLayout::DynamicLocalRead
       : DescriptorSlotImageLayout::Count;
  // clang-format on
}

struct DescriptorBindRefs;

// tracking for descriptor set slots. Needed because if we use something without IDs for tracking
// binding elements the handles may be deleted and recreated, and stale bindings could interfere
// with new bindings
// this struct is crushed as much as possible in size to minimise memory overhead for descriptor
// tracking when applications allocate many many millions of descriptors
struct DescriptorSetSlot
{
  void AccumulateBindRefs(DescriptorBindRefs &refs, VulkanResourceManager *rm) const;

  void SetBuffer(VkDescriptorType writeType, const VkDescriptorBufferInfo &bufInfo);
  void SetImage(VkDescriptorType writeType, const VkDescriptorImageInfo &imInfo, bool useSampler);
  void SetTexelBuffer(VkDescriptorType writeType, ResourceId id);
  void SetAccelerationStructure(VkDescriptorType writeType,
                                VkAccelerationStructureKHR accelerationStructure);

  // 48-bit truncated VK_WHOLE_SIZE
  static const VkDeviceSize WholeSizeRange = 0xFFFFFFFFFFFF;
  VkDeviceSize GetRange() const { return range == WholeSizeRange ? VK_WHOLE_SIZE : range; }
#if GCC_WORKAROUND
#pragma pack(push, 1)
#endif

  // used for buffers, we assume the max buffer size is less than 1<<48.
  // this is placed first to allow writes to just mask the top bits on read or write and remain
  // aligned, then the type/layout below can be accessed directly as bytes.
  VkDeviceSize range : 48;
  // mutable type - for simplicity we treat all descriptors as mutable. It penalises all
  // applications for mutable descriptors, but there's little point in having a separate path for
  // normal descriptors.
  DescriptorSlotType type : 8;
  // used for images, the image layout
  DescriptorSlotImageLayout imageLayout : 8;

#if GCC_WORKAROUND
#pragma pack(pop)
#endif

  // used for buffers and inline blocks. We could steal some bits here if we needed them since 48
  // bits would be plenty for a long time.
  //
  // Immutable samplers set this to 1 to indicate for replay purposes that the sampler came from an
  // immutable binding when looking purely at the descriptor without knowing its layout
  VkDeviceSize offset;

  // resource IDs are kept separate rather than overlapping/union'ing with other types. This
  // prevents a potential problem where a descriptor has a resource ID written in, then is re-used
  // as a different type and the resource ID is partly trampled. Since these are disjoint we know
  // that even if they're stale they're valid IDs.

  // main contents: buffer, image, texel buffer view, or acceleration structure. NOT the sampler for
  // sampler-only descriptors, just to avoid confusion
  ResourceId resource;
  // sampler for sampler-only descriptors, or sampler for combined image-sampler descriptors
  ResourceId sampler;
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

  size_t totalDescriptorCount() const { return elems.size(); }

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

DECLARE_REFLECTION_STRUCT(DescriptorSetSlot);

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

// linearised version of VkDynamicState
enum VulkanDynamicStateIndex
{
  VkDynamicViewport,
  VkDynamicScissor,
  VkDynamicLineWidth,
  VkDynamicDepthBias,
  VkDynamicBlendConstants,
  VkDynamicDepthBounds,
  VkDynamicStencilCompareMask,
  VkDynamicStencilWriteMask,
  VkDynamicStencilReference,
  VkDynamicViewportWScalingNV,
  VkDynamicDiscardRectangleEXT,
  VkDynamicDiscardRectangleEnableEXT,
  VkDynamicDiscardRectangleModeEXT,
  VkDynamicSampleLocationsEXT,
  VkDynamicViewportShadingRatePaletteNV,
  VkDynamicViewportCoarseSampleOrderNV,
  VkDynamicExclusiveScissorNV,
  VkDynamicExclusiveScissorEnableNV,
  VkDynamicShadingRateKHR,
  VkDynamicLineStippleKHR,
  VkDynamicCullMode,
  VkDynamicFrontFace,
  VkDynamicPrimitiveTopology,
  VkDynamicViewportCount,
  VkDynamicScissorCount,
  VkDynamicVertexInputBindingStride,
  VkDynamicDepthTestEnable,
  VkDynamicDepthWriteEnable,
  VkDynamicDepthCompareOp,
  VkDynamicDepthBoundsTestEnable,
  VkDynamicStencilTestEnable,
  VkDynamicStencilOp,
  VkDynamicRayTracingStackSizeKHR,
  VkDynamicVertexInputEXT,
  VkDynamicControlPointsEXT,
  VkDynamicRastDiscard,
  VkDynamicDepthBiasEnable,
  VkDynamicLogicOpEXT,
  VkDynamicPrimRestart,
  VkDynamicColorWriteEXT,
  VkDynamicTessDomainOriginEXT,
  VkDynamicDepthClampEnableEXT,
  VkDynamicPolygonModeEXT,
  VkDynamicRasterizationSamplesEXT,
  VkDynamicSampleMaskEXT,
  VkDynamicAlphaToCoverageEXT,
  VkDynamicAlphaToOneEXT,
  VkDynamicLogicOpEnableEXT,
  VkDynamicColorBlendEnableEXT,
  VkDynamicColorBlendEquationEXT,
  VkDynamicColorWriteMaskEXT,
  VkDynamicRasterizationStreamEXT,
  VkDynamicConservativeRastModeEXT,
  VkDynamicOverstimationSizeEXT,
  VkDynamicDepthClipEnableEXT,
  VkDynamicSampleLocationsEnableEXT,
  VkDynamicStateColorBlendAdvancedEXT,
  VkDynamicProvokingVertexModeEXT,
  VkDynamicLineRastModeEXT,
  VkDynamicLineStippleEnableEXT,
  VkDynamicDepthClipNegativeOneEXT,
  VkDynamicViewportWScalingEXT,
  VkDynamicViewportSwizzleEXT,
  VkDynamicCoverageToColorEnableEXT,
  VkDynamicCoverageToColorLocationEXT,
  VkDynamicCoverageModulationModeEXT,
  VkDynamicCoverageModulationTableEnableEXT,
  VkDynamicCoverageModulationTableEXT,
  VkDynamicShadingRateImageEnableEXT,
  VkDynamicRepresentativeFragTestEXT,
  VkDynamicCoverageReductionModeEXT,
  VkDynamicAttachmentFeedbackLoopEnableEXT,
  VkDynamicCount,
};

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
  vkCmdSetLineStippleKHR,
  vkGetSemaphoreCounterValue,
  vkWaitSemaphores,
  vkSignalSemaphore,
  vkQueuePresentKHR,
  vkCmdSetCullMode,
  vkCmdSetFrontFace,
  vkCmdSetPrimitiveTopology,
  vkCmdSetViewportWithCount,
  vkCmdSetScissorWithCount,
  vkCmdBindVertexBuffers2,
  vkCmdSetDepthTestEnable,
  vkCmdSetDepthWriteEnable,
  vkCmdSetDepthCompareOp,
  vkCmdSetDepthBoundsTestEnable,
  vkCmdSetStencilTestEnable,
  vkCmdSetStencilOp,
  CoherentMapWrite,
  vkCmdCopyBuffer2,
  vkCmdCopyImage2,
  vkCmdCopyBufferToImage2,
  vkCmdCopyImageToBuffer2,
  vkCmdBlitImage2,
  vkCmdResolveImage2,
  vkCmdSetEvent2,
  vkCmdResetEvent2,
  vkCmdWaitEvents2,
  vkCmdPipelineBarrier2,
  vkCmdWriteTimestamp2,
  vkQueueSubmit2,
  vkCmdWriteBufferMarker2AMD,
  vkCmdSetColorWriteEnableEXT,
  vkCmdSetDepthBiasEnable,
  vkCmdSetLogicOpEXT,
  vkCmdSetPatchControlPointsEXT,
  vkCmdSetPrimitiveRestartEnable,
  vkCmdSetRasterizerDiscardEnable,
  vkCmdSetVertexInputEXT,
  vkCmdBeginRendering,
  vkCmdEndRendering,
  vkCmdSetFragmentShadingRateKHR,
  vkSetDeviceMemoryPriorityEXT,
  vkCmdSetAttachmentFeedbackLoopEnableEXT,
  vkCmdSetAlphaToCoverageEnableEXT,
  vkCmdSetAlphaToOneEnableEXT,
  vkCmdSetColorBlendEnableEXT,
  vkCmdSetColorBlendEquationEXT,
  vkCmdSetColorWriteMaskEXT,
  vkCmdSetConservativeRasterizationModeEXT,
  vkCmdSetDepthClampEnableEXT,
  vkCmdSetDepthClipEnableEXT,
  vkCmdSetDepthClipNegativeOneToOneEXT,
  vkCmdSetExtraPrimitiveOverestimationSizeEXT,
  vkCmdSetLineRasterizationModeEXT,
  vkCmdSetLineStippleEnableEXT,
  vkCmdSetLogicOpEnableEXT,
  vkCmdSetPolygonModeEXT,
  vkCmdSetProvokingVertexModeEXT,
  vkCmdSetRasterizationSamplesEXT,
  vkCmdSetRasterizationStreamEXT,
  vkCmdSetSampleLocationsEnableEXT,
  vkCmdSetSampleMaskEXT,
  vkCmdSetTessellationDomainOriginEXT,
  vkCmdDrawMeshTasksEXT,
  vkCmdDrawMeshTasksIndirectEXT,
  vkCmdDrawMeshTasksIndirectCountEXT,
  vkCmdBuildAccelerationStructuresIndirectKHR,
  vkCmdBuildAccelerationStructuresKHR,
  vkCmdCopyAccelerationStructureKHR,
  vkCmdCopyAccelerationStructureToMemoryKHR,
  vkCmdCopyMemoryToAccelerationStructureKHR,
  vkCreateAccelerationStructureKHR,
  vkCmdBindShadersEXT,
  vkCreateShadersEXT,
  vkCmdSetRayTracingPipelineStackSizeKHR,
  vkCmdTraceRaysIndirectKHR,
  vkCmdTraceRaysKHR,
  vkCreateRayTracingPipelinesKHR,
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
  SERIALISE_HANDLE(VkSamplerYcbcrConversion)   \
  SERIALISE_HANDLE(VkAccelerationStructureKHR) \
  SERIALISE_HANDLE(VkShaderEXT)

#define SERIALISE_HANDLE(type) DECLARE_REFLECTION_STRUCT(type)

SERIALISE_VK_HANDLES();

// declare reflect-able types

// pNext structs - always have deserialise for the next chain
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureBuildGeometryInfoKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureBuildSizesInfoKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureDeviceAddressInfoKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureGeometryAabbsDataKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureGeometryInstancesDataKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureGeometryKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureGeometryTrianglesDataKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureVersionInfoKHR);
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
DECLARE_REFLECTION_STRUCT(VkBlitImageInfo2);
DECLARE_REFLECTION_STRUCT(VkBufferCopy2);
DECLARE_REFLECTION_STRUCT(VkBufferCreateInfo);
DECLARE_REFLECTION_STRUCT(VkBufferDeviceAddressCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkBufferDeviceAddressInfo);
DECLARE_REFLECTION_STRUCT(VkBufferImageCopy2);
DECLARE_REFLECTION_STRUCT(VkBufferMemoryBarrier);
DECLARE_REFLECTION_STRUCT(VkBufferMemoryBarrier2);
DECLARE_REFLECTION_STRUCT(VkBufferMemoryRequirementsInfo2);
DECLARE_REFLECTION_STRUCT(VkBufferOpaqueCaptureAddressCreateInfo);
DECLARE_REFLECTION_STRUCT(VkBufferViewCreateInfo);
DECLARE_REFLECTION_STRUCT(VkCalibratedTimestampInfoKHR);
DECLARE_REFLECTION_STRUCT(VkCommandBufferAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkCommandBufferBeginInfo);
DECLARE_REFLECTION_STRUCT(VkCommandBufferInheritanceConditionalRenderingInfoEXT);
DECLARE_REFLECTION_STRUCT(VkCommandBufferInheritanceInfo);
DECLARE_REFLECTION_STRUCT(VkCommandBufferInheritanceRenderingInfo);
DECLARE_REFLECTION_STRUCT(VkCommandBufferSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkCommandPoolCreateInfo);
DECLARE_REFLECTION_STRUCT(VkComputePipelineCreateInfo);
DECLARE_REFLECTION_STRUCT(VkConditionalRenderingBeginInfoEXT);
DECLARE_REFLECTION_STRUCT(VkCopyAccelerationStructureInfoKHR);
DECLARE_REFLECTION_STRUCT(VkCopyAccelerationStructureToMemoryInfoKHR);
DECLARE_REFLECTION_STRUCT(VkCopyBufferInfo2);
DECLARE_REFLECTION_STRUCT(VkCopyBufferToImageInfo2);
DECLARE_REFLECTION_STRUCT(VkCopyDescriptorSet);
DECLARE_REFLECTION_STRUCT(VkCopyImageInfo2);
DECLARE_REFLECTION_STRUCT(VkCopyImageToBufferInfo2);
DECLARE_REFLECTION_STRUCT(VkCopyMemoryToAccelerationStructureInfoKHR);
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
DECLARE_REFLECTION_STRUCT(VkDependencyInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorPoolCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorPoolInlineUniformBlockCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutBindingFlagsCreateInfo)
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutSupport);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetVariableDescriptorCountAllocateInfo)
DECLARE_REFLECTION_STRUCT(VkDescriptorSetVariableDescriptorCountLayoutSupport)
DECLARE_REFLECTION_STRUCT(VkDescriptorUpdateTemplateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceBufferMemoryRequirements);
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
DECLARE_REFLECTION_STRUCT(VkDeviceImageMemoryRequirements);
DECLARE_REFLECTION_STRUCT(VkDeviceMemoryOpaqueCaptureAddressInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceMemoryOverallocationCreateInfoAMD);
DECLARE_REFLECTION_STRUCT(VkDevicePrivateDataCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceQueueCreateInfo);
DECLARE_REFLECTION_STRUCT(VkDeviceQueueGlobalPriorityCreateInfoKHR);
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
DECLARE_REFLECTION_STRUCT(VkFormatProperties3KHR);
DECLARE_REFLECTION_STRUCT(VkFragmentShadingRateAttachmentInfoKHR);
DECLARE_REFLECTION_STRUCT(VkFramebufferAttachmentImageInfo);
DECLARE_REFLECTION_STRUCT(VkFramebufferAttachmentsCreateInfo);
DECLARE_REFLECTION_STRUCT(VkFramebufferCreateInfo);
DECLARE_REFLECTION_STRUCT(VkGraphicsPipelineCreateInfo);
DECLARE_REFLECTION_STRUCT(VkGraphicsPipelineLibraryCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkHdrMetadataEXT);
DECLARE_REFLECTION_STRUCT(VkImageBlit2);
DECLARE_REFLECTION_STRUCT(VkImageCopy2);
DECLARE_REFLECTION_STRUCT(VkImageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageFormatListCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageFormatProperties2);
DECLARE_REFLECTION_STRUCT(VkImageMemoryBarrier);
DECLARE_REFLECTION_STRUCT(VkImageMemoryBarrier2);
DECLARE_REFLECTION_STRUCT(VkImageMemoryRequirementsInfo2);
DECLARE_REFLECTION_STRUCT(VkImagePlaneMemoryRequirementsInfo);
DECLARE_REFLECTION_STRUCT(VkImageResolve2);
DECLARE_REFLECTION_STRUCT(VkImageSparseMemoryRequirementsInfo2);
DECLARE_REFLECTION_STRUCT(VkImageStencilUsageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageSwapchainCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkImageViewASTCDecodeModeEXT);
DECLARE_REFLECTION_STRUCT(VkImageViewCreateInfo);
DECLARE_REFLECTION_STRUCT(VkImageViewMinLodCreateInfoEXT);
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
DECLARE_REFLECTION_STRUCT(VkMemoryBarrier2);
DECLARE_REFLECTION_STRUCT(VkMemoryDedicatedAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkMemoryDedicatedRequirements);
DECLARE_REFLECTION_STRUCT(VkMemoryFdPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkMemoryGetFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkMemoryOpaqueCaptureAddressAllocateInfo);
DECLARE_REFLECTION_STRUCT(VkMemoryPriorityAllocateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkMemoryRequirements2);
DECLARE_REFLECTION_STRUCT(VkMultisampledRenderToSingleSampledInfoEXT);
DECLARE_REFLECTION_STRUCT(VkMultisamplePropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkMutableDescriptorTypeCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPastPresentationTimingGOOGLE);
DECLARE_REFLECTION_STRUCT(VkPerformanceCounterDescriptionKHR);
DECLARE_REFLECTION_STRUCT(VkPerformanceCounterKHR);
DECLARE_REFLECTION_STRUCT(VkPerformanceQuerySubmitInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevice16BitStorageFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevice4444FormatsFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevice8BitStorageFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceAccelerationStructureFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceAccelerationStructurePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceASTCDecodeFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceBorderColorSwizzleFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceBufferDeviceAddressFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceBufferDeviceAddressFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceCoherentMemoryFeaturesAMD);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceColorWriteEnableFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceComputeShaderDerivativesFeaturesNV);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceConditionalRenderingFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceConservativeRasterizationPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceCustomBorderColorFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceCustomBorderColorPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDepthClampZeroOneFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDepthClipControlFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDepthClipEnableFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDepthStencilResolveProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDescriptorIndexingFeatures)
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDescriptorIndexingProperties)
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDiscardRectanglePropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDriverProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceDynamicRenderingFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExtendedDynamicState2FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExtendedDynamicState3FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExtendedDynamicState3PropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalBufferInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalFenceInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalImageFormatInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceExternalSemaphoreInfo);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFeatures2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFloatControlsProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMap2FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMap2PropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMapFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentDensityMapPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShadingRateFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShadingRateKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFragmentShadingRatePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceGroupProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceHostQueryResetFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceIDProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImage2DViewOf3DFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImageFormatInfo2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImagelessFramebufferFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImageRobustnessFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImageViewImageFormatInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceImageViewMinLodFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceIndexTypeUint8FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceInlineUniformBlockFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceInlineUniformBlockProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceLineRasterizationFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceLineRasterizationPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMaintenance3Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMaintenance4Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMaintenance4Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryBudgetPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryPriorityFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryProperties2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMeshShaderFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMeshShaderPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMultiviewFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMultiviewProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceNestedCommandBufferFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceNestedCommandBufferPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePCIBusInfoPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePerformanceQueryFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePerformanceQueryPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePipelineCreationCacheControlFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePointClippingProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePresentIdFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePresentWaitFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePrivateDataFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProperties2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProtectedMemoryFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProtectedMemoryProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProvokingVertexFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProvokingVertexPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDevicePushDescriptorPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRayQueryFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRayTracingPipelineFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRayTracingPipelinePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRobustness2FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceRobustness2PropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSampleLocationsPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSamplerFilterMinmaxProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSamplerYcbcrConversionFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceScalarBlockLayoutFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderAtomicInt64Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderClockFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderCorePropertiesAMD);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderDrawParametersFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderFloat16Int8Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderImageFootprintFeaturesNV);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderIntegerDotProductFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderIntegerDotProductProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderObjectFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderObjectPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceShaderTerminateInvocationFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSparseImageFormatInfo2);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSubgroupProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSubgroupSizeControlFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSubgroupSizeControlProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSurfaceInfo2KHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSynchronization2Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTexelBufferAlignmentProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTextureCompressionASTCHDRFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTimelineSemaphoreFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTimelineSemaphoreProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceToolProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTransformFeedbackFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceTransformFeedbackPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceUniformBufferStandardLayoutFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVariablePointerFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan11Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan11Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan12Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan12Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan13Features);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkan13Properties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceVulkanMemoryModelFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures);
DECLARE_REFLECTION_STRUCT(VkPipelineCacheCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineColorBlendStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineColorWriteCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineCreationFeedbackCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineDepthStencilStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineDiscardRectangleStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineDynamicStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableInternalRepresentationKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutablePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineExecutableStatisticKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineFragmentShadingRateStateCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineInputAssemblyStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineLayoutCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineLibraryCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineMultisampleStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationConservativeStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationDepthClipStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationLineStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationProvokingVertexStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineRasterizationStateStreamCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineRenderingCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineSampleLocationsStateCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineShaderStageCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineShaderStageRequiredSubgroupSizeCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineTessellationDomainOriginStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineTessellationStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineVertexInputDivisorStateCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPipelineVertexInputStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPipelineViewportDepthClipControlCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkPipelineViewportStateCreateInfo);
DECLARE_REFLECTION_STRUCT(VkPresentIdKHR);
DECLARE_REFLECTION_STRUCT(VkPresentInfoKHR);
DECLARE_REFLECTION_STRUCT(VkPresentRegionsKHR);
DECLARE_REFLECTION_STRUCT(VkPresentTimeGOOGLE);
DECLARE_REFLECTION_STRUCT(VkPresentTimesInfoGOOGLE);
DECLARE_REFLECTION_STRUCT(VkPrivateDataSlotCreateInfo);
DECLARE_REFLECTION_STRUCT(VkProtectedSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkQueryPoolCreateInfo);
DECLARE_REFLECTION_STRUCT(VkQueryPoolPerformanceCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkQueueFamilyGlobalPriorityPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkQueueFamilyProperties2);
DECLARE_REFLECTION_STRUCT(VkRayTracingPipelineCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkRayTracingPipelineInterfaceCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkRayTracingShaderGroupCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkRefreshCycleDurationGOOGLE);
DECLARE_REFLECTION_STRUCT(VkReleaseSwapchainImagesInfoEXT);
DECLARE_REFLECTION_STRUCT(VkRenderingAttachmentInfo);
DECLARE_REFLECTION_STRUCT(VkRenderingFragmentDensityMapAttachmentInfoEXT);
DECLARE_REFLECTION_STRUCT(VkRenderingFragmentShadingRateAttachmentInfoKHR);
DECLARE_REFLECTION_STRUCT(VkRenderingInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassAttachmentBeginInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassBeginInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassCreateInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassCreateInfo2);
DECLARE_REFLECTION_STRUCT(VkRenderPassFragmentDensityMapCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkRenderPassInputAttachmentAspectCreateInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassMultiviewCreateInfo);
DECLARE_REFLECTION_STRUCT(VkRenderPassSampleLocationsBeginInfoEXT);
DECLARE_REFLECTION_STRUCT(VkResolveImageInfo2);
DECLARE_REFLECTION_STRUCT(VkSampleLocationsInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSamplerBorderColorComponentMappingCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSamplerCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSamplerCustomBorderColorCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSamplerReductionModeCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSamplerYcbcrConversionCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSamplerYcbcrConversionImageFormatProperties);
DECLARE_REFLECTION_STRUCT(VkSamplerYcbcrConversionInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreGetFdInfoKHR);
DECLARE_REFLECTION_STRUCT(VkSemaphoreSignalInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreTypeCreateInfo);
DECLARE_REFLECTION_STRUCT(VkSemaphoreWaitInfo);
DECLARE_REFLECTION_STRUCT(VkShaderCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkShaderModuleCreateInfo);
DECLARE_REFLECTION_STRUCT(VkShaderModuleValidationCacheCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSharedPresentSurfaceCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkSparseImageFormatProperties2);
DECLARE_REFLECTION_STRUCT(VkSparseImageMemoryRequirements2);
DECLARE_REFLECTION_STRUCT(VkSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkSubmitInfo2);
DECLARE_REFLECTION_STRUCT(VkSubpassBeginInfo);
DECLARE_REFLECTION_STRUCT(VkSubpassDependency2);
DECLARE_REFLECTION_STRUCT(VkSubpassDescription2);
DECLARE_REFLECTION_STRUCT(VkSubpassDescriptionDepthStencilResolve);
DECLARE_REFLECTION_STRUCT(VkSubpassEndInfo);
DECLARE_REFLECTION_STRUCT(VkSubpassFragmentDensityMapOffsetEndInfoQCOM);
DECLARE_REFLECTION_STRUCT(VkSubpassResolvePerformanceQueryEXT);
DECLARE_REFLECTION_STRUCT(VkSubpassSampleLocationsEXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilities2EXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilities2KHR);
DECLARE_REFLECTION_STRUCT(VkSurfaceFormat2KHR);
DECLARE_REFLECTION_STRUCT(VkSurfacePresentModeCompatibilityEXT);
DECLARE_REFLECTION_STRUCT(VkSurfacePresentModeEXT);
DECLARE_REFLECTION_STRUCT(VkSurfacePresentScalingCapabilitiesEXT);
DECLARE_REFLECTION_STRUCT(VkSurfaceProtectedCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkSwapchainCounterCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSwapchainCreateInfoKHR);
DECLARE_REFLECTION_STRUCT(VkSwapchainDisplayNativeHdrCreateInfoAMD);
DECLARE_REFLECTION_STRUCT(VkSwapchainPresentFenceInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSwapchainPresentModeInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSwapchainPresentModesCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkSwapchainPresentScalingCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkTextureLODGatherFormatPropertiesAMD);
DECLARE_REFLECTION_STRUCT(VkTimelineSemaphoreSubmitInfo);
DECLARE_REFLECTION_STRUCT(VkValidationCacheCreateInfoEXT);
DECLARE_REFLECTION_STRUCT(VkValidationFeaturesEXT);
DECLARE_REFLECTION_STRUCT(VkValidationFlagsEXT);
DECLARE_REFLECTION_STRUCT(VkVertexInputAttributeDescription2EXT);
DECLARE_REFLECTION_STRUCT(VkVertexInputBindingDescription2EXT);
DECLARE_REFLECTION_STRUCT(VkWriteDescriptorSet);
DECLARE_REFLECTION_STRUCT(VkWriteDescriptorSetAccelerationStructureKHR);
DECLARE_REFLECTION_STRUCT(VkWriteDescriptorSetInlineUniformBlock);

DECLARE_DESERIALISE_TYPE(VkAccelerationStructureBuildGeometryInfoKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureBuildSizesInfoKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureDeviceAddressInfoKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureGeometryAabbsDataKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureGeometryInstancesDataKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureGeometryKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureGeometryTrianglesDataKHR);
DECLARE_DESERIALISE_TYPE(VkAccelerationStructureVersionInfoKHR);
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
DECLARE_DESERIALISE_TYPE(VkBlitImageInfo2);
DECLARE_DESERIALISE_TYPE(VkBufferCopy2);
DECLARE_DESERIALISE_TYPE(VkBufferCreateInfo);
DECLARE_DESERIALISE_TYPE(VkBufferImageCopy2);
DECLARE_DESERIALISE_TYPE(VkBufferMemoryBarrier);
DECLARE_DESERIALISE_TYPE(VkBufferMemoryBarrier2);
DECLARE_DESERIALISE_TYPE(VkBufferMemoryRequirementsInfo2);
DECLARE_DESERIALISE_TYPE(VkBufferOpaqueCaptureAddressCreateInfo);
DECLARE_DESERIALISE_TYPE(VkBufferViewCreateInfo);
DECLARE_DESERIALISE_TYPE(VkCalibratedTimestampInfoKHR);
DECLARE_DESERIALISE_TYPE(VkCommandBufferAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkCommandBufferBeginInfo);
DECLARE_DESERIALISE_TYPE(VkCommandBufferInheritanceConditionalRenderingInfoEXT);
DECLARE_DESERIALISE_TYPE(VkCommandBufferInheritanceInfo);
DECLARE_DESERIALISE_TYPE(VkCommandBufferInheritanceRenderingInfo);
DECLARE_DESERIALISE_TYPE(VkCommandBufferSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkCommandPoolCreateInfo);
DECLARE_DESERIALISE_TYPE(VkComputePipelineCreateInfo);
DECLARE_DESERIALISE_TYPE(VkConditionalRenderingBeginInfoEXT);
DECLARE_DESERIALISE_TYPE(VkCopyAccelerationStructureInfoKHR);
DECLARE_DESERIALISE_TYPE(VkCopyAccelerationStructureToMemoryInfoKHR);
DECLARE_DESERIALISE_TYPE(VkCopyBufferInfo2);
DECLARE_DESERIALISE_TYPE(VkCopyBufferToImageInfo2);
DECLARE_DESERIALISE_TYPE(VkCopyDescriptorSet);
DECLARE_DESERIALISE_TYPE(VkCopyImageInfo2);
DECLARE_DESERIALISE_TYPE(VkCopyImageToBufferInfo2);
DECLARE_DESERIALISE_TYPE(VkCopyMemoryToAccelerationStructureInfoKHR);
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
DECLARE_DESERIALISE_TYPE(VkDependencyInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorPoolCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorPoolInlineUniformBlockCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutBindingFlagsCreateInfo)
DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetLayoutSupport);
DECLARE_DESERIALISE_TYPE(VkDescriptorSetVariableDescriptorCountAllocateInfo)
DECLARE_DESERIALISE_TYPE(VkDescriptorSetVariableDescriptorCountLayoutSupport)
DECLARE_DESERIALISE_TYPE(VkDescriptorUpdateTemplateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceBufferMemoryRequirements);
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
DECLARE_DESERIALISE_TYPE(VkDeviceImageMemoryRequirements);
DECLARE_DESERIALISE_TYPE(VkDeviceMemoryOpaqueCaptureAddressInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceMemoryOverallocationCreateInfoAMD);
DECLARE_DESERIALISE_TYPE(VkDevicePrivateDataCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceQueueCreateInfo);
DECLARE_DESERIALISE_TYPE(VkDeviceQueueGlobalPriorityCreateInfoKHR);
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
DECLARE_DESERIALISE_TYPE(VkFormatProperties3KHR);
DECLARE_DESERIALISE_TYPE(VkFragmentShadingRateAttachmentInfoKHR);
DECLARE_DESERIALISE_TYPE(VkFramebufferAttachmentImageInfo);
DECLARE_DESERIALISE_TYPE(VkFramebufferAttachmentsCreateInfo);
DECLARE_DESERIALISE_TYPE(VkFramebufferCreateInfo);
DECLARE_DESERIALISE_TYPE(VkGraphicsPipelineCreateInfo);
DECLARE_DESERIALISE_TYPE(VkGraphicsPipelineLibraryCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkImageBlit2);
DECLARE_DESERIALISE_TYPE(VkImageCopy2);
DECLARE_DESERIALISE_TYPE(VkImageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageFormatListCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageFormatProperties2);
DECLARE_DESERIALISE_TYPE(VkImageMemoryBarrier);
DECLARE_DESERIALISE_TYPE(VkImageMemoryBarrier2);
DECLARE_DESERIALISE_TYPE(VkImageMemoryRequirementsInfo2);
DECLARE_DESERIALISE_TYPE(VkImagePlaneMemoryRequirementsInfo);
DECLARE_DESERIALISE_TYPE(VkImageResolve2);
DECLARE_DESERIALISE_TYPE(VkImageSparseMemoryRequirementsInfo2);
DECLARE_DESERIALISE_TYPE(VkImageStencilUsageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageSwapchainCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkImageViewASTCDecodeModeEXT);
DECLARE_DESERIALISE_TYPE(VkImageViewCreateInfo);
DECLARE_DESERIALISE_TYPE(VkImageViewMinLodCreateInfoEXT);
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
DECLARE_DESERIALISE_TYPE(VkMemoryBarrier2);
DECLARE_DESERIALISE_TYPE(VkMemoryDedicatedAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkMemoryDedicatedRequirements);
DECLARE_DESERIALISE_TYPE(VkMemoryFdPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkMemoryGetFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkMemoryOpaqueCaptureAddressAllocateInfo);
DECLARE_DESERIALISE_TYPE(VkMemoryPriorityAllocateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkMemoryRequirements2);
DECLARE_DESERIALISE_TYPE(VkMultisampledRenderToSingleSampledInfoEXT);
DECLARE_DESERIALISE_TYPE(VkMultisamplePropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkMutableDescriptorTypeCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPerformanceCounterDescriptionKHR);
DECLARE_DESERIALISE_TYPE(VkPerformanceCounterKHR);
DECLARE_DESERIALISE_TYPE(VkPerformanceQuerySubmitInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevice16BitStorageFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevice4444FormatsFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevice8BitStorageFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceAccelerationStructureFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceAccelerationStructurePropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceASTCDecodeFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceBorderColorSwizzleFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceCoherentMemoryFeaturesAMD);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceColorWriteEnableFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceComputeShaderDerivativesFeaturesNV);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceConditionalRenderingFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceConservativeRasterizationPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceCustomBorderColorFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceCustomBorderColorPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDepthClampZeroOneFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDepthClipControlFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDepthClipEnableFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDepthStencilResolveProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDescriptorIndexingFeatures)
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDescriptorIndexingProperties)
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDiscardRectanglePropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDriverProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceDynamicRenderingFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExtendedDynamicState2FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExtendedDynamicState3FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExtendedDynamicState3PropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalBufferInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalFenceInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalImageFormatInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceExternalSemaphoreInfo);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFeatures2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFloatControlsProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMap2FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMap2PropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapOffsetPropertiesQCOM);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentDensityMapPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShadingRateFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShadingRateKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceFragmentShadingRatePropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceGroupProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceHostQueryResetFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceIDProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImage2DViewOf3DFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImageFormatInfo2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImagelessFramebufferFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImageRobustnessFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImageViewImageFormatInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceImageViewMinLodFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceIndexTypeUint8FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceInlineUniformBlockFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceInlineUniformBlockProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceLineRasterizationFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceLineRasterizationPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMaintenance3Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMaintenance4Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMaintenance4Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMemoryBudgetPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMemoryPriorityFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMemoryProperties2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMeshShaderFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMeshShaderPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMultiviewFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMultiviewProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceNestedCommandBufferFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceNestedCommandBufferPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePCIBusInfoPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePerformanceQueryFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePerformanceQueryPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePipelineCreationCacheControlFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePointClippingProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePresentIdFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePresentWaitFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePrivateDataFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProperties2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProtectedMemoryFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProtectedMemoryProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProvokingVertexFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceProvokingVertexPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDevicePushDescriptorPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRayQueryFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRayTracingPipelineFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRayTracingPipelinePropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRobustness2FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceRobustness2PropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSampleLocationsPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSamplerFilterMinmaxProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSamplerYcbcrConversionFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceScalarBlockLayoutFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderAtomicInt64Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderClockFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderCorePropertiesAMD);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderDrawParametersFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderFloat16Int8Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderImageFootprintFeaturesNV);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderIntegerDotProductFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderIntegerDotProductProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderObjectFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderObjectPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceShaderTerminateInvocationFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSparseImageFormatInfo2);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSubgroupProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSubgroupSizeControlFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSubgroupSizeControlProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSurfaceInfo2KHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceSynchronization2Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTexelBufferAlignmentProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTextureCompressionASTCHDRFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTimelineSemaphoreFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTimelineSemaphoreProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceToolProperties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTransformFeedbackFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceTransformFeedbackPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceUniformBufferStandardLayoutFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVariablePointerFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan11Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan11Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan12Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan12Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan13Features);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkan13Properties);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceVulkanMemoryModelFeatures);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures);
DECLARE_DESERIALISE_TYPE(VkPipelineCacheCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineColorBlendStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineColorWriteCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineCreationFeedbackCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineDepthStencilStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineDiscardRectangleStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineDynamicStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutableInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutableInternalRepresentationKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutablePropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineExecutableStatisticKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineFragmentShadingRateStateCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineInputAssemblyStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineLayoutCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineLibraryCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineMultisampleStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationConservativeStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationDepthClipStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationLineStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationProvokingVertexStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineRasterizationStateStreamCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineRenderingCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineSampleLocationsStateCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineShaderStageCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineShaderStageRequiredSubgroupSizeCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineTessellationDomainOriginStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineTessellationStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineVertexInputDivisorStateCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPipelineVertexInputStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPipelineViewportDepthClipControlCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkPipelineViewportStateCreateInfo);
DECLARE_DESERIALISE_TYPE(VkPresentIdKHR);
DECLARE_DESERIALISE_TYPE(VkPresentInfoKHR);
DECLARE_DESERIALISE_TYPE(VkPresentRegionsKHR);
DECLARE_DESERIALISE_TYPE(VkPresentTimesInfoGOOGLE);
DECLARE_DESERIALISE_TYPE(VkPrivateDataSlotCreateInfo);
DECLARE_DESERIALISE_TYPE(VkProtectedSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkQueryPoolCreateInfo);
DECLARE_DESERIALISE_TYPE(VkQueryPoolPerformanceCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkQueueFamilyGlobalPriorityPropertiesKHR);
DECLARE_DESERIALISE_TYPE(VkQueueFamilyProperties2);
DECLARE_DESERIALISE_TYPE(VkRayTracingPipelineCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkRayTracingPipelineInterfaceCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkRayTracingShaderGroupCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkReleaseSwapchainImagesInfoEXT);
DECLARE_DESERIALISE_TYPE(VkRenderingAttachmentInfo);
DECLARE_DESERIALISE_TYPE(VkRenderingFragmentDensityMapAttachmentInfoEXT);
DECLARE_DESERIALISE_TYPE(VkRenderingFragmentShadingRateAttachmentInfoKHR);
DECLARE_DESERIALISE_TYPE(VkRenderingInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassAttachmentBeginInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassBeginInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassCreateInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassCreateInfo2);
DECLARE_DESERIALISE_TYPE(VkRenderPassInputAttachmentAspectCreateInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassMultiviewCreateInfo);
DECLARE_DESERIALISE_TYPE(VkRenderPassSampleLocationsBeginInfoEXT);
DECLARE_DESERIALISE_TYPE(VkResolveImageInfo2);
DECLARE_DESERIALISE_TYPE(VkSampleLocationsInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSamplerBorderColorComponentMappingCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSamplerCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSamplerCustomBorderColorCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSamplerReductionModeCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSamplerYcbcrConversionCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSamplerYcbcrConversionImageFormatProperties);
DECLARE_DESERIALISE_TYPE(VkSamplerYcbcrConversionInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreGetFdInfoKHR);
DECLARE_DESERIALISE_TYPE(VkSemaphoreSignalInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreTypeCreateInfo);
DECLARE_DESERIALISE_TYPE(VkSemaphoreWaitInfo);
DECLARE_DESERIALISE_TYPE(VkShaderCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkShaderModuleCreateInfo);
DECLARE_DESERIALISE_TYPE(VkShaderModuleValidationCacheCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSharedPresentSurfaceCapabilitiesKHR);
DECLARE_DESERIALISE_TYPE(VkSparseImageFormatProperties2);
DECLARE_DESERIALISE_TYPE(VkSparseImageMemoryRequirements2);
DECLARE_DESERIALISE_TYPE(VkSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkSubmitInfo2);
DECLARE_DESERIALISE_TYPE(VkSubpassBeginInfo);
DECLARE_DESERIALISE_TYPE(VkSubpassDependency2);
DECLARE_DESERIALISE_TYPE(VkSubpassDescription2);
DECLARE_DESERIALISE_TYPE(VkSubpassDescriptionDepthStencilResolve);
DECLARE_DESERIALISE_TYPE(VkSubpassEndInfo);
DECLARE_DESERIALISE_TYPE(VkSubpassFragmentDensityMapOffsetEndInfoQCOM);
DECLARE_DESERIALISE_TYPE(VkSubpassResolvePerformanceQueryEXT);
DECLARE_DESERIALISE_TYPE(VkSubpassSampleLocationsEXT);
DECLARE_DESERIALISE_TYPE(VkSurfaceCapabilities2EXT);
DECLARE_DESERIALISE_TYPE(VkSurfaceCapabilities2KHR);
DECLARE_DESERIALISE_TYPE(VkSurfaceFormat2KHR);
DECLARE_DESERIALISE_TYPE(VkSurfacePresentModeCompatibilityEXT);
DECLARE_DESERIALISE_TYPE(VkSurfacePresentModeEXT);
DECLARE_DESERIALISE_TYPE(VkSurfacePresentScalingCapabilitiesEXT);
DECLARE_DESERIALISE_TYPE(VkSurfaceProtectedCapabilitiesKHR);
DECLARE_DESERIALISE_TYPE(VkSwapchainCounterCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSwapchainCreateInfoKHR);
DECLARE_DESERIALISE_TYPE(VkSwapchainDisplayNativeHdrCreateInfoAMD);
DECLARE_DESERIALISE_TYPE(VkSwapchainPresentFenceInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSwapchainPresentModeInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSwapchainPresentModesCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkSwapchainPresentScalingCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkTextureLODGatherFormatPropertiesAMD);
DECLARE_DESERIALISE_TYPE(VkTimelineSemaphoreSubmitInfo);
DECLARE_DESERIALISE_TYPE(VkValidationCacheCreateInfoEXT);
DECLARE_DESERIALISE_TYPE(VkValidationFeaturesEXT);
DECLARE_DESERIALISE_TYPE(VkValidationFlagsEXT);
DECLARE_DESERIALISE_TYPE(VkVertexInputAttributeDescription2EXT);
DECLARE_DESERIALISE_TYPE(VkVertexInputBindingDescription2EXT);
DECLARE_DESERIALISE_TYPE(VkWriteDescriptorSet);
DECLARE_DESERIALISE_TYPE(VkWriteDescriptorSetAccelerationStructureKHR);
DECLARE_DESERIALISE_TYPE(VkWriteDescriptorSetInlineUniformBlock);

// plain structs with no next chain
DECLARE_REFLECTION_STRUCT(VkAabbPositionsKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureBuildRangeInfoKHR);
DECLARE_REFLECTION_STRUCT(VkAccelerationStructureInstanceKHR);
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
DECLARE_REFLECTION_STRUCT(VkColorBlendEquationEXT);
DECLARE_REFLECTION_STRUCT(VkComponentMapping);
DECLARE_REFLECTION_STRUCT(VkConformanceVersion);
DECLARE_REFLECTION_STRUCT(VkDescriptorBufferInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorImageInfo);
DECLARE_REFLECTION_STRUCT(VkDescriptorPoolSize);
DECLARE_REFLECTION_STRUCT(VkDescriptorSetLayoutBinding);
DECLARE_REFLECTION_STRUCT(VkDescriptorUpdateTemplateEntry);
DECLARE_REFLECTION_STRUCT(VkDeviceOrHostAddressConstKHR);
DECLARE_REFLECTION_STRUCT(VkDeviceOrHostAddressKHR);
DECLARE_REFLECTION_STRUCT(VkDispatchIndirectCommand);
DECLARE_REFLECTION_STRUCT(VkDisplayModeParametersKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayModePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPlaneCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPlanePropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkDisplayPropertiesKHR);
DECLARE_REFLECTION_STRUCT(VkDrawIndexedIndirectCommand);
DECLARE_REFLECTION_STRUCT(VkDrawIndirectCommand);
DECLARE_REFLECTION_STRUCT(VkDrawMeshTasksIndirectCommandEXT);
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
DECLARE_REFLECTION_STRUCT(VkMutableDescriptorTypeListEXT);
DECLARE_REFLECTION_STRUCT(VkOffset2D);
DECLARE_REFLECTION_STRUCT(VkOffset3D);
DECLARE_REFLECTION_STRUCT(VkPerformanceCounterResultKHR);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceFeatures);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceLimits);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceMemoryProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceProperties);
DECLARE_REFLECTION_STRUCT(VkPhysicalDeviceSparseProperties);
DECLARE_REFLECTION_STRUCT(VkPipelineColorBlendAttachmentState);
DECLARE_REFLECTION_STRUCT(VkPipelineCreationFeedback);
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
DECLARE_REFLECTION_STRUCT(VkStridedDeviceAddressRegionKHR);
DECLARE_REFLECTION_STRUCT(VkSubpassDependency);
DECLARE_REFLECTION_STRUCT(VkSubpassDescription);
DECLARE_REFLECTION_STRUCT(VkSurfaceCapabilitiesKHR);
DECLARE_REFLECTION_STRUCT(VkSurfaceFormatKHR);
DECLARE_REFLECTION_STRUCT(VkTransformMatrixKHR);
DECLARE_REFLECTION_STRUCT(VkVertexInputAttributeDescription);
DECLARE_REFLECTION_STRUCT(VkVertexInputBindingDescription);
DECLARE_REFLECTION_STRUCT(VkVertexInputBindingDivisorDescriptionEXT);
DECLARE_REFLECTION_STRUCT(VkViewport);
DECLARE_REFLECTION_STRUCT(VkXYColorEXT);

// rdcarray serialisation is generic but the stringification is not
DECLARE_STRINGISE_TYPE(rdcarray<VkAccelerationStructureBuildRangeInfoKHR>);

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
DECLARE_REFLECTION_STRUCT(VkAndroidHardwareBufferFormatProperties2ANDROID);

DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferUsageANDROID);
DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferPropertiesANDROID);
DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferFormatPropertiesANDROID);
DECLARE_DESERIALISE_TYPE(VkImportAndroidHardwareBufferInfoANDROID);
DECLARE_DESERIALISE_TYPE(VkMemoryGetAndroidHardwareBufferInfoANDROID);
DECLARE_DESERIALISE_TYPE(VkExternalFormatANDROID);
DECLARE_DESERIALISE_TYPE(VkAndroidHardwareBufferFormatProperties2ANDROID);
#endif

// GGP only structs
#ifdef VK_USE_PLATFORM_GGP
DECLARE_REFLECTION_STRUCT(VkPresentFrameTokenGGP);

DECLARE_DESERIALISE_TYPE(VkPresentFrameTokenGGP);
#endif

// we add these fake enums so we have a type for type-dispatch in the serialiser. Due to C ABI rules
// the vulkan API doesn't define native 64-bit enums itself
//
// if you get a compile error here, then somehow the macro shenanigans we play above to stop
// vulkan_core.h from defining 'typedef VkFlags64 VkAccessFlagBits2' has failed. We try to make
// it use a different non-clashing name so that this name can be clear for a proper separate type.
enum VkAccessFlagBits2 : uint64_t
{
};

enum VkPipelineStageFlagBits2 : uint64_t
{
};

enum VkFormatFeatureFlagBits2 : uint64_t
{
};

// enums

DECLARE_REFLECTION_ENUM(VkAccelerationStructureBuildTypeKHR);
DECLARE_REFLECTION_ENUM(VkAccelerationStructureCompatibilityKHR);
DECLARE_REFLECTION_ENUM(VkAccelerationStructureCreateFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkAccelerationStructureTypeKHR);
DECLARE_REFLECTION_ENUM(VkAccessFlagBits);
DECLARE_REFLECTION_ENUM(VkAccessFlagBits2);
DECLARE_REFLECTION_ENUM(VkAcquireProfilingLockFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkAttachmentDescriptionFlagBits);
DECLARE_REFLECTION_ENUM(VkAttachmentLoadOp);
DECLARE_REFLECTION_ENUM(VkAttachmentStoreOp);
DECLARE_REFLECTION_ENUM(VkBlendFactor);
DECLARE_REFLECTION_ENUM(VkBlendOp);
DECLARE_REFLECTION_ENUM(VkBorderColor);
DECLARE_REFLECTION_ENUM(VkBufferCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkBufferUsageFlagBits);
DECLARE_REFLECTION_ENUM(VkBuildAccelerationStructureFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkBuildAccelerationStructureModeKHR);
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
DECLARE_REFLECTION_ENUM(VkCopyAccelerationStructureModeKHR);
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
DECLARE_REFLECTION_ENUM(VkEventCreateFlagBits);
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
DECLARE_REFLECTION_ENUM(VkFormatFeatureFlagBits2);
DECLARE_REFLECTION_ENUM(VkFragmentShadingRateCombinerOpKHR);
DECLARE_REFLECTION_ENUM(VkFramebufferCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkGeometryFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkGeometryInstanceFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkGeometryTypeKHR);
DECLARE_REFLECTION_ENUM(VkGraphicsPipelineLibraryFlagBitsEXT);
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
DECLARE_REFLECTION_ENUM(VkLineRasterizationModeKHR);
DECLARE_REFLECTION_ENUM(VkLogicOp);
DECLARE_REFLECTION_ENUM(VkMemoryAllocateFlagBits);
DECLARE_REFLECTION_ENUM(VkMemoryHeapFlagBits);
DECLARE_REFLECTION_ENUM(VkMemoryPropertyFlagBits);
DECLARE_REFLECTION_ENUM(VkMemoryOverallocationBehaviorAMD);
DECLARE_REFLECTION_ENUM(VkObjectType);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterDescriptionFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterScopeKHR);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterStorageKHR);
DECLARE_REFLECTION_ENUM(VkPerformanceCounterUnitKHR);
DECLARE_REFLECTION_ENUM(VkPhysicalDeviceType);
DECLARE_REFLECTION_ENUM(VkPipelineBindPoint);
DECLARE_REFLECTION_ENUM(VkPipelineCacheCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineColorBlendStateCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineCreationFeedbackFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineDepthStencilStateCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineExecutableStatisticFormatKHR);
DECLARE_REFLECTION_ENUM(VkPipelineLayoutCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineShaderStageCreateFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineStageFlagBits);
DECLARE_REFLECTION_ENUM(VkPipelineStageFlagBits2);
DECLARE_REFLECTION_ENUM(VkPointClippingBehavior);
DECLARE_REFLECTION_ENUM(VkPolygonMode);
DECLARE_REFLECTION_ENUM(VkPresentGravityFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkPresentModeKHR);
DECLARE_REFLECTION_ENUM(VkPresentScalingFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkPrimitiveTopology);
DECLARE_REFLECTION_ENUM(VkProvokingVertexModeEXT);
DECLARE_REFLECTION_ENUM(VkRayTracingShaderGroupTypeKHR);
DECLARE_REFLECTION_ENUM(VkRenderingFlagBits);
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
DECLARE_REFLECTION_ENUM(VkShaderCodeTypeEXT);
DECLARE_REFLECTION_ENUM(VkShaderCreateFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkShaderFloatControlsIndependence);
DECLARE_REFLECTION_ENUM(VkShaderStageFlagBits);
DECLARE_REFLECTION_ENUM(VkSharingMode);
DECLARE_REFLECTION_ENUM(VkSparseImageFormatFlagBits);
DECLARE_REFLECTION_ENUM(VkSparseMemoryBindFlagBits);
DECLARE_REFLECTION_ENUM(VkStencilFaceFlagBits);
DECLARE_REFLECTION_ENUM(VkStencilOp);
DECLARE_REFLECTION_ENUM(VkStructureType);
DECLARE_REFLECTION_ENUM(VkSubgroupFeatureFlagBits);
DECLARE_REFLECTION_ENUM(VkSubmitFlagBits);
DECLARE_REFLECTION_ENUM(VkSubpassContents);
DECLARE_REFLECTION_ENUM(VkSubpassDescriptionFlagBits);
DECLARE_REFLECTION_ENUM(VkSurfaceCounterFlagBitsEXT);
DECLARE_REFLECTION_ENUM(VkSurfaceTransformFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkSwapchainCreateFlagBitsKHR);
DECLARE_REFLECTION_ENUM(VkTessellationDomainOrigin);
DECLARE_REFLECTION_ENUM(VkTimeDomainEXT);
DECLARE_REFLECTION_ENUM(VkToolPurposeFlagBits);
DECLARE_REFLECTION_ENUM(VkValidationCheckEXT);
DECLARE_REFLECTION_ENUM(VkValidationFeatureDisableEXT);
DECLARE_REFLECTION_ENUM(VkValidationFeatureEnableEXT);
DECLARE_REFLECTION_ENUM(VkVertexInputRate);

// win32 only enums
#ifdef VK_USE_PLATFORM_WIN32_KHR
DECLARE_REFLECTION_ENUM(VkFullScreenExclusiveEXT);
#endif
