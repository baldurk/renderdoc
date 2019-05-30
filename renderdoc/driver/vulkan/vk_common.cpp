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
#include "vk_core.h"
#include "vk_manager.h"
#include "vk_resources.h"

// utility struct for firing one-shot command buffers to begin/end markers
struct ScopedCommandBuffer
{
  ScopedCommandBuffer(VkCommandBuffer cmdbuf, WrappedVulkan *vk)
  {
    core = vk;
    cmd = cmdbuf;
    local = (cmd == VK_NULL_HANDLE);

    if(local)
    {
      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      cmd = vk->GetNextCmd();

      VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }
  ~ScopedCommandBuffer()
  {
    if(local)
    {
      VkResult vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      core->SubmitCmds();
    }
  }

  WrappedVulkan *core;
  VkCommandBuffer cmd;
  bool local;
};

WrappedVulkan *VkMarkerRegion::vk = NULL;

VkMarkerRegion::VkMarkerRegion(const std::string &marker, VkCommandBuffer cmd)
{
  if(cmd == VK_NULL_HANDLE)
  {
    RDCERR("Cannot auto-allocate a command buffer for a scoped VkMarkerRegion");
    return;
  }

  cmdbuf = cmd;
  Begin(marker, cmd);
}

VkMarkerRegion::~VkMarkerRegion()
{
  if(cmdbuf)
    End(cmdbuf);
}

void VkMarkerRegion::Begin(const std::string &marker, VkCommandBuffer cmd)
{
  if(!vk)
    return;

  // check for presence of the marker extension
  if(!ObjDisp(vk->GetDev())->CmdDebugMarkerBeginEXT)
    return;

  ScopedCommandBuffer scope(cmd, vk);

  VkDebugMarkerMarkerInfoEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
  markerInfo.pMarkerName = marker.c_str();
  ObjDisp(scope.cmd)->CmdDebugMarkerBeginEXT(Unwrap(scope.cmd), &markerInfo);
}

void VkMarkerRegion::Set(const std::string &marker, VkCommandBuffer cmd)
{
  // check for presence of the marker extension
  if(!ObjDisp(vk->GetDev())->CmdDebugMarkerBeginEXT)
    return;

  ScopedCommandBuffer scope(cmd, vk);

  VkDebugMarkerMarkerInfoEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
  markerInfo.pMarkerName = marker.c_str();
  ObjDisp(scope.cmd)->CmdDebugMarkerInsertEXT(Unwrap(scope.cmd), &markerInfo);
}

void VkMarkerRegion::End(VkCommandBuffer cmd)
{
  // check for presence of the marker extension
  if(!ObjDisp(vk->GetDev())->CmdDebugMarkerBeginEXT)
    return;

  ScopedCommandBuffer scope(cmd, vk);

  ObjDisp(scope.cmd)->CmdDebugMarkerEndEXT(Unwrap(scope.cmd));
}

void GPUBuffer::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t ringSize,
                       uint32_t flags)
{
  m_pDriver = driver;
  device = dev;
  createFlags = flags;

  align = (VkDeviceSize)driver->GetDeviceProps().limits.minUniformBufferOffsetAlignment;

  // for simplicity, consider the non-coherent atom size also an alignment requirement
  align = AlignUp(align, driver->GetDeviceProps().limits.nonCoherentAtomSize);

  sz = size;
  // offset must be aligned, so ensure we have at least ringSize
  // copies accounting for that
  totalsize = AlignUp(size, align) * RDCMAX(1U, ringSize);
  curoffset = 0;

  ringCount = ringSize;

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, totalsize, 0,
  };

  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bufInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  if(flags & eGPUBufferVBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if(flags & eGPUBufferIBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  if(flags & eGPUBufferSSBO)
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if(flags & eGPUBufferIndirectBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

  if(flags & eGPUBufferAddressable)
    bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT;

  VkResult vkr = driver->vkCreateBuffer(dev, &bufInfo, NULL, &buf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {};
  driver->vkGetBufferMemoryRequirements(dev, buf, &mrq);

  VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size, 0};

  if(flags & eGPUBufferReadback)
    allocInfo.memoryTypeIndex = driver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  else if(flags & eGPUBufferGPULocal)
    allocInfo.memoryTypeIndex = driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits);
  else
    allocInfo.memoryTypeIndex = driver->GetUploadMemoryIndex(mrq.memoryTypeBits);

  vkr = driver->vkAllocateMemory(dev, &allocInfo, NULL, &mem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = driver->vkBindBufferMemory(dev, buf, mem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void GPUBuffer::FillDescriptor(VkDescriptorBufferInfo &desc)
{
  desc.buffer = Unwrap(buf);
  desc.offset = 0;
  desc.range = sz;
}

void GPUBuffer::Destroy()
{
  if(device != VK_NULL_HANDLE)
  {
    m_pDriver->vkDestroyBuffer(device, buf, NULL);
    m_pDriver->vkFreeMemory(device, mem, NULL);
  }
}

void *GPUBuffer::Map(uint32_t *bindoffset, VkDeviceSize usedsize)
{
  VkDeviceSize offset = bindoffset ? curoffset : 0;
  VkDeviceSize size = usedsize > 0 ? usedsize : sz;

  // align the size so we always consume coherent atoms
  size = AlignUp(size, align);

  // wrap around the ring as soon as the 'sz' would overflow. This is because if we're using dynamic
  // offsets in the descriptor the range is still set to that fixed size and the validation
  // complains if we go off the end (even if it's unused). Rather than constantly update the
  // descriptor, we just conservatively wrap and waste the last bit of space.
  if(offset + sz > totalsize)
    offset = 0;
  RDCASSERT(offset + size <= totalsize);

  // offset must be aligned
  curoffset = AlignUp(offset + size, align);

  if(bindoffset)
    *bindoffset = (uint32_t)offset;

  mapoffset = offset;

  void *ptr = NULL;
  VkResult vkr = m_pDriver->vkMapMemory(device, mem, offset, size, 0, (void **)&ptr);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(createFlags & eGPUBufferReadback)
  {
    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, mem, offset, size,
    };

    vkr = m_pDriver->vkInvalidateMappedMemoryRanges(device, 1, &range);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  return ptr;
}

void *GPUBuffer::Map(VkDeviceSize &bindoffset, VkDeviceSize usedsize)
{
  uint32_t offs = 0;

  void *ret = Map(&offs, usedsize);

  bindoffset = offs;

  return ret;
}

void GPUBuffer::Unmap()
{
  if(!(createFlags & eGPUBufferReadback) && !(createFlags & eGPUBufferGPULocal))
  {
    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, mem, mapoffset, VK_WHOLE_SIZE,
    };

    VkResult vkr = m_pDriver->vkFlushMappedMemoryRanges(device, 1, &range);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  m_pDriver->vkUnmapMemory(device, mem);
}

bool VkInitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // 0xF -> 0x10 - added serialisation of VkPhysicalDeviceDriverPropertiesKHR into enumerated
  // physical devices
  if(ver == 0xF)
    return true;

  // 0xE -> 0xF - serialisation of VkPhysicalDeviceVulkanMemoryModelFeaturesKHR changed in vulkan
  // 1.1.99, adding a new field
  if(ver == 0xE)
    return true;

  // 0xD -> 0xE - fixed serialisation directly of size_t members in VkDescriptorUpdateTemplateEntry
  if(ver == 0xD)
    return true;

  // 0xC -> 0xD - supported multiple queues. This didn't cause a large change to the serialisation
  // but there were some slight inconsistencies that required a version bump
  if(ver == 0xC)
    return true;

  // 0xB -> 0xC - generally this is when we started serialising pNext chains that older RenderDoc
  // couldn't support. But we don't need any special backwards compatibiltiy code as it's just added
  // serialisation.
  if(ver == 0xB)
    return true;

  return false;
}

VkAccessFlags MakeAccessMask(VkImageLayout layout)
{
  switch(layout)
  {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VkAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT);
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return VkAccessFlags(VK_ACCESS_HOST_WRITE_BIT);
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VkAccessFlags(VK_ACCESS_TRANSFER_READ_BIT);
    default: break;
  }

  return VkAccessFlags(0);
}

void SanitiseOldImageLayout(VkImageLayout &layout)
{
  // we don't replay with present layouts since we don't create actual swapchains. So change any
  // present layouts to general layouts
  if(layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR || layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR)
    layout = VK_IMAGE_LAYOUT_GENERAL;

  // we can't transition to PREINITIALIZED, so instead use GENERAL. This allows host access so we
  // can still replay maps of the image's memory. In theory we can still transition from
  // PREINITIALIZED on replay, but consider that we need to be able to reset layouts and suddenly we
  // have a problem transitioning from PREINITIALIZED more than once - so for that reason we
  // instantly promote any images that are PREINITIALIZED to GENERAL at the start of the frame
  // capture, and from then on treat it as the same
  if(layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    layout = VK_IMAGE_LAYOUT_GENERAL;
}

void SanitiseNewImageLayout(VkImageLayout &layout)
{
  // apply any general image layout sanitisation
  SanitiseOldImageLayout(layout);

  // we also can't transition to UNDEFINED, so go to GENERAL instead. This is safe since if the
  // layout was supposed to be undefined before then the only valid transition *from* the state is
  // UNDEFINED, which will work silently.
  if(layout == VK_IMAGE_LAYOUT_UNDEFINED)
    layout = VK_IMAGE_LAYOUT_GENERAL;
}

int SampleCount(VkSampleCountFlagBits countFlag)
{
  switch(countFlag)
  {
    case VK_SAMPLE_COUNT_1_BIT: return 1;
    case VK_SAMPLE_COUNT_2_BIT: return 2;
    case VK_SAMPLE_COUNT_4_BIT: return 4;
    case VK_SAMPLE_COUNT_8_BIT: return 8;
    case VK_SAMPLE_COUNT_16_BIT: return 16;
    case VK_SAMPLE_COUNT_32_BIT: return 32;
    case VK_SAMPLE_COUNT_64_BIT: return 64;
    default: RDCERR("Unrecognised/not single flag %x", countFlag); break;
  }

  return 1;
}

int SampleIndex(VkSampleCountFlagBits countFlag)
{
  switch(countFlag)
  {
    case VK_SAMPLE_COUNT_1_BIT: return 0;
    case VK_SAMPLE_COUNT_2_BIT: return 1;
    case VK_SAMPLE_COUNT_4_BIT: return 2;
    case VK_SAMPLE_COUNT_8_BIT: return 3;
    case VK_SAMPLE_COUNT_16_BIT: return 4;
    case VK_SAMPLE_COUNT_32_BIT: return 5;
    case VK_SAMPLE_COUNT_64_BIT: return 6;
    default: RDCERR("Unrecognised/not single flag %x", countFlag); break;
  }

  return 0;
}

int StageIndex(VkShaderStageFlagBits stageFlag)
{
  switch(stageFlag)
  {
    case VK_SHADER_STAGE_VERTEX_BIT: return 0;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return 1;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return 2;
    case VK_SHADER_STAGE_GEOMETRY_BIT: return 3;
    case VK_SHADER_STAGE_FRAGMENT_BIT: return 4;
    case VK_SHADER_STAGE_COMPUTE_BIT: return 5;
    default: RDCERR("Unrecognised/not single flag %x", stageFlag); break;
  }

  return 0;
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkImageMemoryBarrier *barriers)
{
  RDCASSERT(cmd != VK_NULL_HANDLE);
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                   NULL,                // global memory barriers
                                   0, NULL,             // buffer memory barriers
                                   count, barriers);    // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkBufferMemoryBarrier *barriers)
{
  RDCASSERT(cmd != VK_NULL_HANDLE);
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                   NULL,               // global memory barriers
                                   count, barriers,    // buffer memory barriers
                                   0, NULL);           // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkMemoryBarrier *barriers)
{
  RDCASSERT(cmd != VK_NULL_HANDLE);
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, count,
                                   barriers,    // global memory barriers
                                   0, NULL,     // buffer memory barriers
                                   0, NULL);    // image memory barriers
}

Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints)
{
  switch(Topo)
  {
    default: break;
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return Topology::PointList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: return Topology::LineList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return Topology::LineStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return Topology::TriangleList; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return Topology::TriangleStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: return Topology::TriangleFan; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: return Topology::LineList_Adj; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: return Topology::LineStrip_Adj; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      return Topology::TriangleList_Adj;
      break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      return Topology::TriangleStrip_Adj;
      break;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: return PatchList_Topology(patchControlPoints); break;
  }

  return Topology::Unknown;
}

VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo)
{
  switch(Topo)
  {
    case Topology::LineLoop: RDCWARN("Unsupported primitive topology on Vulkan: %x", Topo); break;
    default: return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    case Topology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Topology::LineStrip_Adj: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case Topology::LineList_Adj: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Topology::TriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Topology::TriangleStrip_Adj: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
    case Topology::TriangleList_Adj: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case Topology::PatchList_1CPs:
    case Topology::PatchList_2CPs:
    case Topology::PatchList_3CPs:
    case Topology::PatchList_4CPs:
    case Topology::PatchList_5CPs:
    case Topology::PatchList_6CPs:
    case Topology::PatchList_7CPs:
    case Topology::PatchList_8CPs:
    case Topology::PatchList_9CPs:
    case Topology::PatchList_10CPs:
    case Topology::PatchList_11CPs:
    case Topology::PatchList_12CPs:
    case Topology::PatchList_13CPs:
    case Topology::PatchList_14CPs:
    case Topology::PatchList_15CPs:
    case Topology::PatchList_16CPs:
    case Topology::PatchList_17CPs:
    case Topology::PatchList_18CPs:
    case Topology::PatchList_19CPs:
    case Topology::PatchList_20CPs:
    case Topology::PatchList_21CPs:
    case Topology::PatchList_22CPs:
    case Topology::PatchList_23CPs:
    case Topology::PatchList_24CPs:
    case Topology::PatchList_25CPs:
    case Topology::PatchList_26CPs:
    case Topology::PatchList_27CPs:
    case Topology::PatchList_28CPs:
    case Topology::PatchList_29CPs:
    case Topology::PatchList_30CPs:
    case Topology::PatchList_31CPs:
    case Topology::PatchList_32CPs: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  }

  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

AddressMode MakeAddressMode(VkSamplerAddressMode addr)
{
  switch(addr)
  {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT: return AddressMode::Wrap;
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return AddressMode::Mirror;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return AddressMode::ClampEdge;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return AddressMode::ClampBorder;
    case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

void MakeBorderColor(VkBorderColor border, FloatVector *BorderColor)
{
  // we don't distinguish float/int, assume it matches
  switch(border)
  {
    case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
    case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
      *BorderColor = FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
      break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
    case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
      *BorderColor = FloatVector(0.0f, 0.0f, 0.0f, 1.0f);
      break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
    case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
      *BorderColor = FloatVector(1.0f, 1.0f, 1.0f, 1.0f);
      break;
    default: memset(BorderColor, 0, sizeof(FloatVector)); break;
  }
}

CompareFunction MakeCompareFunc(VkCompareOp func)
{
  switch(func)
  {
    case VK_COMPARE_OP_NEVER: return CompareFunction::Never;
    case VK_COMPARE_OP_LESS: return CompareFunction::Less;
    case VK_COMPARE_OP_EQUAL: return CompareFunction::Equal;
    case VK_COMPARE_OP_LESS_OR_EQUAL: return CompareFunction::LessEqual;
    case VK_COMPARE_OP_GREATER: return CompareFunction::Greater;
    case VK_COMPARE_OP_NOT_EQUAL: return CompareFunction::NotEqual;
    case VK_COMPARE_OP_GREATER_OR_EQUAL: return CompareFunction::GreaterEqual;
    case VK_COMPARE_OP_ALWAYS: return CompareFunction::AlwaysTrue;
    default: break;
  }

  return CompareFunction::AlwaysTrue;
}

FilterMode MakeFilterMode(VkFilter f)
{
  switch(f)
  {
    case VK_FILTER_NEAREST: return FilterMode::Point;
    case VK_FILTER_LINEAR: return FilterMode::Linear;
    case VK_FILTER_CUBIC_IMG: return FilterMode::Cubic;
    default: break;
  }

  return FilterMode::NoFilter;
}

static FilterMode MakeFilterMode(VkSamplerMipmapMode f)
{
  switch(f)
  {
    case VK_SAMPLER_MIPMAP_MODE_NEAREST: return FilterMode::Point;
    case VK_SAMPLER_MIPMAP_MODE_LINEAR: return FilterMode::Linear;
    default: break;
  }

  return FilterMode::NoFilter;
}

TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
                         bool anisoEnable, bool compareEnable, VkSamplerReductionModeEXT reduction)
{
  TextureFilter ret;

  if(anisoEnable)
  {
    ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
  }
  else
  {
    ret.minify = MakeFilterMode(minFilter);
    ret.magnify = MakeFilterMode(magFilter);
    ret.mip = MakeFilterMode(mipmapMode);
  }
  ret.filter = compareEnable ? FilterFunction::Comparison : FilterFunction::Normal;

  if(compareEnable)
  {
    ret.filter = FilterFunction::Comparison;
  }
  else
  {
    switch(reduction)
    {
      default:
      case VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT:
        ret.filter = FilterFunction::Normal;
        break;
      case VK_SAMPLER_REDUCTION_MODE_MIN_EXT: ret.filter = FilterFunction::Minimum; break;
      case VK_SAMPLER_REDUCTION_MODE_MAX_EXT: ret.filter = FilterFunction::Maximum; break;
    }
  }

  return ret;
}

LogicOperation MakeLogicOp(VkLogicOp op)
{
  switch(op)
  {
    case VK_LOGIC_OP_CLEAR: return LogicOperation::Clear;
    case VK_LOGIC_OP_AND: return LogicOperation::And;
    case VK_LOGIC_OP_AND_REVERSE: return LogicOperation::AndReverse;
    case VK_LOGIC_OP_COPY: return LogicOperation::Copy;
    case VK_LOGIC_OP_AND_INVERTED: return LogicOperation::AndInverted;
    case VK_LOGIC_OP_NO_OP: return LogicOperation::NoOp;
    case VK_LOGIC_OP_XOR: return LogicOperation::Xor;
    case VK_LOGIC_OP_OR: return LogicOperation::Or;
    case VK_LOGIC_OP_NOR: return LogicOperation::Nor;
    case VK_LOGIC_OP_EQUIVALENT: return LogicOperation::Equivalent;
    case VK_LOGIC_OP_INVERT: return LogicOperation::Invert;
    case VK_LOGIC_OP_OR_REVERSE: return LogicOperation::OrReverse;
    case VK_LOGIC_OP_COPY_INVERTED: return LogicOperation::CopyInverted;
    case VK_LOGIC_OP_OR_INVERTED: return LogicOperation::OrInverted;
    case VK_LOGIC_OP_NAND: return LogicOperation::Nand;
    case VK_LOGIC_OP_SET: return LogicOperation::Set;
    default: break;
  }

  return LogicOperation::NoOp;
}

BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend)
{
  switch(blend)
  {
    case VK_BLEND_FACTOR_ZERO: return BlendMultiplier::Zero;
    case VK_BLEND_FACTOR_ONE: return BlendMultiplier::One;
    case VK_BLEND_FACTOR_SRC_COLOR: return BlendMultiplier::SrcCol;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case VK_BLEND_FACTOR_DST_COLOR: return BlendMultiplier::DstCol;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return BlendMultiplier::InvDstCol;
    case VK_BLEND_FACTOR_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case VK_BLEND_FACTOR_DST_ALPHA: return BlendMultiplier::DstAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case VK_BLEND_FACTOR_CONSTANT_COLOR: return BlendMultiplier::FactorRGB;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return BlendMultiplier::InvFactorRGB;
    case VK_BLEND_FACTOR_CONSTANT_ALPHA: return BlendMultiplier::FactorAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return BlendMultiplier::InvFactorAlpha;
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return BlendMultiplier::SrcAlphaSat;
    case VK_BLEND_FACTOR_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case VK_BLEND_FACTOR_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(VkBlendOp op)
{
  // Need to update this when we support VK_EXT_blend_operation_advanced
  switch(op)
  {
    case VK_BLEND_OP_ADD: return BlendOperation::Add;
    case VK_BLEND_OP_SUBTRACT: return BlendOperation::Subtract;
    case VK_BLEND_OP_REVERSE_SUBTRACT: return BlendOperation::ReversedSubtract;
    case VK_BLEND_OP_MIN: return BlendOperation::Minimum;
    case VK_BLEND_OP_MAX: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

StencilOperation MakeStencilOp(VkStencilOp op)
{
  switch(op)
  {
    case VK_STENCIL_OP_KEEP: return StencilOperation::Keep;
    case VK_STENCIL_OP_ZERO: return StencilOperation::Zero;
    case VK_STENCIL_OP_REPLACE: return StencilOperation::Replace;
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP: return StencilOperation::IncSat;
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP: return StencilOperation::DecSat;
    case VK_STENCIL_OP_INVERT: return StencilOperation::Invert;
    case VK_STENCIL_OP_INCREMENT_AND_WRAP: return StencilOperation::IncWrap;
    case VK_STENCIL_OP_DECREMENT_AND_WRAP: return StencilOperation::DecWrap;
    default: break;
  }

  return StencilOperation::Keep;
}

BASIC_TYPE_SERIALISE_STRINGIFY(VkPackedVersion, (uint32_t &)el, SDBasic::UnsignedInteger, 4);

INSTANTIATE_SERIALISE_TYPE(VkPackedVersion);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkInitParams &el)
{
  SERIALISE_MEMBER(AppName);
  SERIALISE_MEMBER(EngineName);
  SERIALISE_MEMBER(AppVersion);
  SERIALISE_MEMBER(EngineVersion);
  SERIALISE_MEMBER(APIVersion).TypedAs("uint32_t"_lit);
  SERIALISE_MEMBER(Layers);
  SERIALISE_MEMBER(Extensions);
  SERIALISE_MEMBER(InstanceID).TypedAs("VkInstance"_lit);
}

INSTANTIATE_SERIALISE_TYPE(VkInitParams);

VkDriverInfo::VkDriverInfo(const VkPhysicalDeviceProperties &physProps)
{
  m_Vendor = GPUVendorFromPCIVendor(physProps.vendorID);

  // add non-PCI vendor IDs
  if(physProps.vendorID == VK_VENDOR_ID_VSI)
    m_Vendor = GPUVendor::Verisilicon;

  m_Major = VK_VERSION_MAJOR(physProps.driverVersion);
  m_Minor = VK_VERSION_MINOR(physProps.driverVersion);
  m_Patch = VK_VERSION_PATCH(physProps.driverVersion);

#if ENABLED(RDOC_APPLE)
  metalBackend = true;
#endif

  // nvidia uses its own version packing:
  //   10 |  8  |        8       |       6
  // major|minor|secondary_branch|tertiary_branch
  if(m_Vendor == GPUVendor::nVidia)
  {
    m_Major = ((uint32_t)(physProps.driverVersion) >> (8 + 8 + 6)) & 0x3ff;
    m_Minor = ((uint32_t)(physProps.driverVersion) >> (8 + 6)) & 0x0ff;

    uint32_t secondary = ((uint32_t)(physProps.driverVersion) >> 6) & 0x0ff;
    uint32_t tertiary = physProps.driverVersion & 0x03f;

    m_Patch = (secondary << 8) | tertiary;
  }

  if(m_Vendor == GPUVendor::nVidia)
  {
    // drivers before 372.54 did not handle a glslang bugfix about separated samplers,
    // and disabling texelFetch works as a workaround.

    if(Major() < 372 || (Major() == 372 && Minor() < 54))
      texelFetchBrokenDriver = true;
  }

// only check this on windows. This is a bit of a hack, as really we want to check if we're
// using the AMD official driver, but there's not a great other way to distinguish it from
// the RADV open source driver.
#if ENABLED(RDOC_WIN32)
  if(m_Vendor == GPUVendor::AMD)
  {
    // for AMD the bugfix version isn't clear as version numbering wasn't strong for a while, but
    // any driver that reports a version of >= 1.0.0 is fine, as previous versions all reported
    // 0.9.0 as the version.

    if(Major() < 1)
      texelFetchBrokenDriver = true;

    // driver 18.5.2 which is vulkan version >= 2.0.33 contains the fix
    if(physProps.driverVersion < VK_MAKE_VERSION(2, 0, 33))
      unreliableImgMemReqs = true;
  }
#endif

  if(texelFetchBrokenDriver)
  {
    RDCWARN("Detected an older driver, enabling workaround. Try updating to the latest drivers.");
  }

// same as above, only affects the AMD official driver
#if ENABLED(RDOC_WIN32)
  if(m_Vendor == GPUVendor::AMD)
  {
    // driver 18.5.2 which is vulkan version >= 2.0.33 contains the fix
    if(physProps.driverVersion < VK_MAKE_VERSION(2, 0, 33))
      amdStorageMSAABrokenDriver = true;
  }
#endif

  // not fixed yet
  qualcommLeakingUBOOffsets = m_Vendor == GPUVendor::Qualcomm;
}

FrameRefType GetRefType(VkDescriptorType descType)
{
  switch(descType)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return eFrameRef_Read; break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return eFrameRef_ReadBeforeWrite; break;
    default: RDCERR("Unexpected descriptor type");
  }

  return eFrameRef_Read;
}

bool IsValid(const VkWriteDescriptorSet &write, uint32_t arrayElement)
{
  // this makes assumptions that only hold within the context of Serialise_InitialState below,
  // specifically that if pTexelBufferView/pBufferInfo is set then we are using them. In the general
  // case they can be garbage and we must ignore them based on the descriptorType

  if(write.pTexelBufferView)
    return write.pTexelBufferView[arrayElement] != VK_NULL_HANDLE;

  if(write.pBufferInfo)
    return write.pBufferInfo[arrayElement].buffer != VK_NULL_HANDLE;

  if(write.pImageInfo)
  {
    // only these two types need samplers
    bool needSampler = (write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                        write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // but all types that aren't just a sampler need an image
    bool needImage = (write.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER);

    if(needSampler && write.pImageInfo[arrayElement].sampler == VK_NULL_HANDLE)
      return false;

    if(needImage && write.pImageInfo[arrayElement].imageView == VK_NULL_HANDLE)
      return false;

    return true;
  }

  RDCERR("Encountered VkWriteDescriptorSet with no data!");

  return false;
}

void DescriptorSetBindingElement::RemoveBindRefs(VkResourceRecord *record)
{
  SCOPED_LOCK(record->descInfo->refLock);

  if(texelBufferView != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(texelBufferView));

    VkResourceRecord *viewRecord = GetRecord(texelBufferView);
    if(viewRecord && viewRecord->baseResource != ResourceId())
      record->RemoveBindFrameRef(viewRecord->baseResource);
  }
  if(imageInfo.imageView != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(imageInfo.imageView));

    VkResourceRecord *viewRecord = GetRecord(imageInfo.imageView);
    if(viewRecord)
    {
      record->RemoveBindFrameRef(viewRecord->baseResource);
      if(viewRecord->baseResourceMem != ResourceId())
        record->RemoveBindFrameRef(viewRecord->baseResourceMem);
    }
  }
  if(imageInfo.sampler != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(imageInfo.sampler));
  }
  if(bufferInfo.buffer != VK_NULL_HANDLE)
  {
    record->RemoveBindFrameRef(GetResID(bufferInfo.buffer));

    VkResourceRecord *bufRecord = GetRecord(bufferInfo.buffer);
    if(bufRecord && bufRecord->baseResource != ResourceId())
      record->RemoveBindFrameRef(bufRecord->baseResource);
  }

  // NULL everything out now so that we don't accidentally reference an object
  // that was removed already
  texelBufferView = VK_NULL_HANDLE;
  bufferInfo.buffer = VK_NULL_HANDLE;
  imageInfo.imageView = VK_NULL_HANDLE;
  imageInfo.sampler = VK_NULL_HANDLE;
}

void DescriptorSetBindingElement::AddBindRefs(VkResourceRecord *record, FrameRefType ref)
{
  SCOPED_LOCK(record->descInfo->refLock);

  if(texelBufferView != VK_NULL_HANDLE)
  {
    VkResourceRecord *bufView = GetRecord(texelBufferView);
    record->AddBindFrameRef(bufView->GetResourceID(), eFrameRef_Read,
                            bufView->resInfo && bufView->resInfo->IsSparse());
    if(bufView->baseResource != ResourceId())
      record->AddBindFrameRef(bufView->baseResource, eFrameRef_Read);
    if(bufView->baseResourceMem != ResourceId())
      record->AddMemFrameRef(bufView->baseResourceMem, bufView->memOffset, bufView->memSize, ref);
  }
  if(imageInfo.imageView != VK_NULL_HANDLE)
  {
    VkResourceRecord *view = GetRecord(imageInfo.imageView);
    record->AddImgFrameRef(view, ref);
  }
  if(imageInfo.sampler != VK_NULL_HANDLE)
  {
    record->AddBindFrameRef(GetResID(imageInfo.sampler), eFrameRef_Read);
  }
  if(bufferInfo.buffer != VK_NULL_HANDLE)
  {
    VkResourceRecord *buf = GetRecord(bufferInfo.buffer);
    record->AddBindFrameRef(GetResID(bufferInfo.buffer), eFrameRef_Read,
                            buf->resInfo && buf->resInfo->IsSparse());
    if(buf->baseResource != ResourceId())
      record->AddMemFrameRef(buf->baseResource, buf->memOffset, buf->memSize, ref);
  }
}

void DescriptorSetSlot::CreateFrom(const DescriptorSetBindingElement &slot)
{
  bufferInfo.buffer = GetResID(slot.bufferInfo.buffer);
  bufferInfo.offset = slot.bufferInfo.offset;
  bufferInfo.range = slot.bufferInfo.range;

  imageInfo.sampler = GetResID(slot.imageInfo.sampler);
  imageInfo.imageView = GetResID(slot.imageInfo.imageView);
  imageInfo.imageLayout = slot.imageInfo.imageLayout;

  texelBufferView = GetResID(slot.texelBufferView);
}
