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
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_manager.h"
#include "vk_resources.h"

WrappedVulkan *VkMarkerRegion::vk = NULL;

VkMarkerRegion::VkMarkerRegion(VkCommandBuffer cmd, const rdcstr &marker)
{
  if(cmd == VK_NULL_HANDLE)
    return;

  cmdbuf = cmd;
  Begin(marker, cmd);
}

VkMarkerRegion::VkMarkerRegion(VkQueue q, const rdcstr &marker)
{
  if(q == VK_NULL_HANDLE)
  {
    if(vk)
      q = vk->GetQ();
    else
      return;
  }

  queue = q;
  Begin(marker, q);
}

VkMarkerRegion::~VkMarkerRegion()
{
  if(queue)
    End(queue);
  else if(cmdbuf)
    End(cmdbuf);
}

void VkMarkerRegion::Begin(const rdcstr &marker, VkCommandBuffer cmd)
{
  if(cmd == VK_NULL_HANDLE)
    return;

  // check for presence of the marker extension
  if(!ObjDisp(cmd)->CmdBeginDebugUtilsLabelEXT)
    return;

  VkDebugUtilsLabelEXT label = {};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = marker.c_str();
  ObjDisp(cmd)->CmdBeginDebugUtilsLabelEXT(Unwrap(cmd), &label);
}

void VkMarkerRegion::Set(const rdcstr &marker, VkCommandBuffer cmd)
{
  if(cmd == VK_NULL_HANDLE)
    return;

  // check for presence of the marker extension
  if(!ObjDisp(cmd)->CmdInsertDebugUtilsLabelEXT)
    return;

  VkDebugUtilsLabelEXT label = {};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = marker.c_str();
  ObjDisp(cmd)->CmdInsertDebugUtilsLabelEXT(Unwrap(cmd), &label);
}

void VkMarkerRegion::End(VkCommandBuffer cmd)
{
  if(cmd == VK_NULL_HANDLE)
    return;

  // check for presence of the marker extension
  if(!ObjDisp(cmd)->CmdEndDebugUtilsLabelEXT)
    return;

  ObjDisp(cmd)->CmdEndDebugUtilsLabelEXT(Unwrap(cmd));
}

VkDevice VkMarkerRegion::GetDev()
{
  return vk->GetDev();
}

void VkMarkerRegion::Begin(const rdcstr &marker, VkQueue q)
{
  if(q == VK_NULL_HANDLE)
  {
    if(vk)
      q = vk->GetQ();
    else
      return;
  }

  // check for presence of the marker extension
  if(!ObjDisp(q)->QueueBeginDebugUtilsLabelEXT)
    return;

  VkDebugUtilsLabelEXT label = {};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = marker.c_str();
  ObjDisp(q)->QueueBeginDebugUtilsLabelEXT(Unwrap(q), &label);
}

void VkMarkerRegion::Set(const rdcstr &marker, VkQueue q)
{
  if(q == VK_NULL_HANDLE)
  {
    if(vk)
      q = vk->GetQ();
    else
      return;
  }

  // check for presence of the marker extension
  if(!ObjDisp(q)->QueueInsertDebugUtilsLabelEXT)
    return;

  VkDebugUtilsLabelEXT label = {};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = marker.c_str();
  ObjDisp(q)->QueueInsertDebugUtilsLabelEXT(Unwrap(q), &label);
}

void VkMarkerRegion::End(VkQueue q)
{
  if(q == VK_NULL_HANDLE)
  {
    if(vk)
      q = vk->GetQ();
    else
      return;
  }

  // check for presence of the marker extension
  if(!ObjDisp(q)->QueueEndDebugUtilsLabelEXT)
    return;

  ObjDisp(q)->QueueEndDebugUtilsLabelEXT(Unwrap(q));
}

template <>
VkObjectType objType<VkBuffer>()
{
  return VK_OBJECT_TYPE_BUFFER;
}
template <>
VkObjectType objType<VkImage>()
{
  return VK_OBJECT_TYPE_IMAGE;
}
template <>
VkObjectType objType<VkImageView>()
{
  return VK_OBJECT_TYPE_IMAGE_VIEW;
}
template <>
VkObjectType objType<VkFramebuffer>()
{
  return VK_OBJECT_TYPE_FRAMEBUFFER;
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

  bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if((flags & eGPUBufferReadback) == 0)
  {
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }

  if(flags & eGPUBufferVBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  if(flags & eGPUBufferIBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  if(flags & eGPUBufferSSBO)
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  if(flags & eGPUBufferIndirectBuffer)
    bufInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

  if(flags & eGPUBufferAddressable)
    bufInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  VkResult vkr = driver->vkCreateBuffer(dev, &bufInfo, NULL, &buf);
  driver->CheckVkResult(vkr);

  VkMemoryRequirements mrq = {};
  driver->vkGetBufferMemoryRequirements(dev, buf, &mrq);

  VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size, 0};

  if(flags & eGPUBufferReadback)
    allocInfo.memoryTypeIndex = driver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  else if(flags & eGPUBufferGPULocal)
    allocInfo.memoryTypeIndex = driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits);
  else
    allocInfo.memoryTypeIndex = driver->GetUploadMemoryIndex(mrq.memoryTypeBits);

  bool useBufferAddressKHR = driver->GetExtensions(NULL).ext_KHR_buffer_device_address;

  VkMemoryAllocateFlagsInfo memFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
  if(useBufferAddressKHR && (flags & eGPUBufferAddressable))
  {
    allocInfo.pNext = &memFlags;
    memFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
  }

  vkr = driver->vkAllocateMemory(dev, &allocInfo, NULL, &mem);
  driver->CheckVkResult(vkr);

  if(vkr != VK_SUCCESS)
    return;

  vkr = driver->vkBindBufferMemory(dev, buf, mem, 0);
  driver->CheckVkResult(vkr);
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

  if(mem == VK_NULL_HANDLE)
  {
    RDCERR("Manually reporting failed memory map with no memory");
    m_pDriver->CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
  }

  void *ptr = NULL;
  VkResult vkr = m_pDriver->vkMapMemory(device, mem, offset, size, 0, (void **)&ptr);
  m_pDriver->CheckVkResult(vkr);

  if(!ptr)
  {
    RDCERR("Manually reporting failed memory map");
    m_pDriver->CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
  }

  if(createFlags & eGPUBufferReadback)
  {
    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, mem, offset, size,
    };

    vkr = m_pDriver->vkInvalidateMappedMemoryRanges(device, 1, &range);
    m_pDriver->CheckVkResult(vkr);
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
    m_pDriver->CheckVkResult(vkr);
  }

  m_pDriver->vkUnmapMemory(device, mem);
}

bool VkInitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // 0x15 -> 0x16 - added support for acceleration structures
  if(ver == 0x15)
    return true;

  // 0x14 -> 0x15 - added support for mutable descriptors
  if(ver == 0x14)
    return true;

  // 0x13 -> 0x14 - added missing VkCommandBufferInheritanceRenderingInfo::flags
  if(ver == 0x13)
    return true;

  // 0x12 -> 0x13 - added full sparse resource support
  if(ver == 0x12)
    return true;

  // 0x11 -> 0x12 - added inline uniform block support
  if(ver == 0x11)
    return true;

  // 0x10 -> 0x11 - non-breaking changes to image state serialization
  if(ver == 0x10)
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

void SanitiseReplayImageLayout(VkImageLayout &layout)
{
  // we don't replay with present layouts since we don't create actual swapchains. So change any
  // present layouts to general layouts
  if(layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR || layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR)
    layout = VK_IMAGE_LAYOUT_GENERAL;
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

void CombineDepthStencilLayouts(rdcarray<VkImageMemoryBarrier> &barriers)
{
  for(size_t i = 0; i < barriers.size(); i++)
  {
    // only consider barriers on depth
    // barriers not on D/S at all can be ignored
    // barriers on both D/S already can be ignored
    // barriers on stencil only can be ignored, because we expect to always find depth before
    // stencil
    if(barriers[i].subresourceRange.aspectMask != VK_IMAGE_ASPECT_DEPTH_BIT)
      continue;

    // search forward to see if we have an identical barrier on stencil for the same image. We
    // expect a loose sort so all barriers for the same image are together.
    // This means when we don't have separate depth-stencil layout support, the aspects should
    // always be in the same layout so can be combined.
    for(size_t j = i + 1; j < barriers.size(); j++)
    {
      // stop when we reach another image, no more possible matches expected after this
      if(barriers[i].image != barriers[j].image)
        break;

      // only consider stencil aspect barriers
      if(barriers[j].subresourceRange.aspectMask != VK_IMAGE_ASPECT_STENCIL_BIT)
        continue;

      // if the barriers are equal apart from the aspect mask, we can promote [i] to depth and
      // stencil, and erase j
      if(barriers[i].oldLayout == barriers[j].oldLayout &&
         barriers[i].newLayout == barriers[j].newLayout &&
         barriers[i].srcAccessMask == barriers[j].srcAccessMask &&
         barriers[i].dstAccessMask == barriers[j].dstAccessMask &&
         barriers[i].srcQueueFamilyIndex == barriers[j].srcQueueFamilyIndex &&
         barriers[i].dstQueueFamilyIndex == barriers[j].dstQueueFamilyIndex &&
         barriers[i].subresourceRange.baseArrayLayer == barriers[j].subresourceRange.baseArrayLayer &&
         barriers[i].subresourceRange.baseMipLevel == barriers[j].subresourceRange.baseMipLevel &&
         barriers[i].subresourceRange.layerCount == barriers[j].subresourceRange.layerCount &&
         barriers[i].subresourceRange.levelCount == barriers[j].subresourceRange.levelCount)
      {
        barriers[i].subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        barriers.erase(j);
        break;
      }
    }

    // either we merged the i'th element and we can skip it, or it's not mergeable and we can skip
    // it. Either way we can continue the loop at i+1
  }
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
    case VK_SHADER_STAGE_TASK_BIT_EXT: return 6;
    case VK_SHADER_STAGE_MESH_BIT_EXT: return 7;
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR: return 8;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: return 9;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: return 10;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: return 11;
    case VK_SHADER_STAGE_MISS_BIT_KHR: return 12;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR: return 13;
    default: RDCERR("Unrecognised/not single flag %x", stageFlag); break;
  }

  return 0;
}

VkShaderStageFlags ShaderMaskFromIndex(size_t index)
{
  VkShaderStageFlagBits mask[] = {
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
      VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_COMPUTE_BIT,
      VK_SHADER_STAGE_TASK_BIT_EXT,
      VK_SHADER_STAGE_MESH_BIT_EXT,
      VK_SHADER_STAGE_RAYGEN_BIT_KHR,
      VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
      VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
      VK_SHADER_STAGE_MISS_BIT_KHR,
      VK_SHADER_STAGE_CALLABLE_BIT_KHR,
  };

  RDCCOMPILE_ASSERT(ARRAY_COUNT(mask) == NumShaderStages, "Array is out of date");

  if(index < ARRAY_COUNT(mask))
    return mask[index];

  RDCERR("Unrecognised shader stage index %d", index);

  return 0;
}

void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkImageMemoryBarrier *barriers)
{
  RDCASSERT(cmd != VK_NULL_HANDLE);
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                   NULL,                          // global memory barriers
                                   0, NULL,                       // buffer memory barriers
                                   (uint32_t)count, barriers);    // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkBufferMemoryBarrier *barriers)
{
  RDCASSERT(cmd != VK_NULL_HANDLE);
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                   NULL,                         // global memory barriers
                                   (uint32_t)count, barriers,    // buffer memory barriers
                                   0, NULL);                     // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, size_t count, const VkMemoryBarrier *barriers)
{
  RDCASSERT(cmd != VK_NULL_HANDLE);
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, (uint32_t)count,
                                   barriers,    // global memory barriers
                                   0, NULL,     // buffer memory barriers
                                   0, NULL);    // image memory barriers
}

Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints)
{
  switch(Topo)
  {
    default: break;
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return Topology::PointList;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: return Topology::LineList;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return Topology::LineStrip;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return Topology::TriangleList;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return Topology::TriangleStrip;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: return Topology::TriangleFan;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: return Topology::LineList_Adj;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: return Topology::LineStrip_Adj;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY: return Topology::TriangleList_Adj;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY: return Topology::TriangleStrip_Adj;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: return PatchList_Topology(patchControlPoints);
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

void MakeBorderColor(VkBorderColor border, rdcfixedarray<float, 4> &BorderColor)
{
  // we don't distinguish float/int, assume it matches
  switch(border)
  {
    case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
    case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK: BorderColor = {0.0f, 0.0f, 0.0f, 0.0f}; break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
    case VK_BORDER_COLOR_INT_OPAQUE_BLACK: BorderColor = {0.0f, 0.0f, 0.0f, 1.0f}; break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
    case VK_BORDER_COLOR_INT_OPAQUE_WHITE: BorderColor = {1.0f, 1.0f, 1.0f, 1.0f}; break;
    default: BorderColor = {0.0f, 0.0f, 0.0f, 0.0f}; break;
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
    case VK_FILTER_CUBIC_EXT: return FilterMode::Cubic;
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
                         bool anisoEnable, bool compareEnable, VkSamplerReductionMode reduction)
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
      case VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE: ret.filter = FilterFunction::Normal; break;
      case VK_SAMPLER_REDUCTION_MODE_MIN: ret.filter = FilterFunction::Minimum; break;
      case VK_SAMPLER_REDUCTION_MODE_MAX: ret.filter = FilterFunction::Maximum; break;
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

rdcstr HumanDriverName(VkDriverId driverId)
{
  switch(driverId)
  {
    case VK_DRIVER_ID_AMD_PROPRIETARY: return "AMD Proprietary";
    case VK_DRIVER_ID_AMD_OPEN_SOURCE: return "AMD Open-source";
    case VK_DRIVER_ID_MESA_RADV: return "AMD RADV";
    case VK_DRIVER_ID_NVIDIA_PROPRIETARY: return "NVIDIA Proprietary";
    case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS: return "Intel Proprietary";
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA: return "Intel Open-source";
    case VK_DRIVER_ID_IMAGINATION_PROPRIETARY: return "Imagination Proprietary";
    case VK_DRIVER_ID_QUALCOMM_PROPRIETARY: return "Qualcomm Proprietary";
    case VK_DRIVER_ID_ARM_PROPRIETARY: return "Arm Proprietary";
    case VK_DRIVER_ID_GOOGLE_SWIFTSHADER: return "Swiftshader";
    case VK_DRIVER_ID_GGP_PROPRIETARY: return "GGP Proprietary";
    case VK_DRIVER_ID_BROADCOM_PROPRIETARY: return "Broadcom Proprietary";
    case VK_DRIVER_ID_MESA_LLVMPIPE: return "Mesa LLVMPipe";
    case VK_DRIVER_ID_MOLTENVK: return "MoltenVK";
    case VK_DRIVER_ID_COREAVI_PROPRIETARY: return "Coreavi Proprietary";
    case VK_DRIVER_ID_JUICE_PROPRIETARY: return "Juice Proprietary";
    case VK_DRIVER_ID_VERISILICON_PROPRIETARY: return "Verisilicon Proprietary";
    case VK_DRIVER_ID_MESA_TURNIP: return "Mesa Turnip";
    case VK_DRIVER_ID_MESA_V3DV: return "Mesa V3DV";
    case VK_DRIVER_ID_MESA_PANVK: return "Mesa Panvk";
    case VK_DRIVER_ID_SAMSUNG_PROPRIETARY: return "Samsung Proprietary";
    case VK_DRIVER_ID_MESA_VENUS: return "Mesa Venus";
    case VK_DRIVER_ID_MESA_DOZEN: return "Mesa Dozen";
    case VK_DRIVER_ID_MESA_NVK: return "Mesa NVK";
    case VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA: return "Imagination Open-source";
    case VK_DRIVER_ID_MESA_HONEYKRISP: return "Mesa Honeykrisp";
    case VK_DRIVER_ID_RESERVED_27: return "<Unknown>";
    case VK_DRIVER_ID_MAX_ENUM: break;
  }

  return "";
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
  SERIALISE_MEMBER(Extensions).Important();
  SERIALISE_MEMBER(InstanceID).TypedAs("VkInstance"_lit);
}

INSTANTIATE_SERIALISE_TYPE(VkInitParams);

void GetPhysicalDeviceDriverProperties(VkInstDispatchTable *instDispatchTable,
                                       VkPhysicalDevice unwrappedPhysicalDevice,
                                       VkPhysicalDeviceDriverProperties &driverProps)
{
  uint32_t count = 0;
  instDispatchTable->EnumerateDeviceExtensionProperties(unwrappedPhysicalDevice, NULL, &count, NULL);

  VkExtensionProperties *props = new VkExtensionProperties[count];
  instDispatchTable->EnumerateDeviceExtensionProperties(unwrappedPhysicalDevice, NULL, &count, props);

  RDCEraseEl(driverProps);
  driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

  for(uint32_t e = 0; e < count; e++)
  {
    // GPDP2 must be available if the driver properties extension is, and we always enable it if
    // available, so we can unconditionally query here
    if(!strcmp(props[e].extensionName, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
    {
      VkPhysicalDeviceProperties2 physProps2 = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      };

      physProps2.pNext = &driverProps;
      instDispatchTable->GetPhysicalDeviceProperties2(unwrappedPhysicalDevice, &physProps2);
      break;
    }
  }

  SAFE_DELETE_ARRAY(props);
}

VkDriverInfo::VkDriverInfo(const VkPhysicalDeviceProperties &physProps,
                           const VkPhysicalDeviceDriverProperties &origDriverProps, bool active)
{
  m_Vendor = GPUVendorFromPCIVendor(physProps.vendorID);

  // add non-PCI vendor IDs
  if(physProps.vendorID == VK_VENDOR_ID_VSI)
    m_Vendor = GPUVendor::Verisilicon;

  // Swiftshader
  if(physProps.vendorID == 0x1AE0 && physProps.deviceID == 0xC0DE)
    m_Vendor = GPUVendor::Software;

  // mesa software
  if(physProps.vendorID == VK_VENDOR_ID_MESA)
    m_Vendor = GPUVendor::Software;

  // take a copy so we can patch the driverID
  VkPhysicalDeviceDriverProperties driverProps = origDriverProps;

  switch(driverProps.driverID)
  {
    case VK_DRIVER_ID_GOOGLE_SWIFTSHADER:
    case VK_DRIVER_ID_MESA_LLVMPIPE: m_Vendor = GPUVendor::Software; break;
    case VK_DRIVER_ID_MOLTENVK: metalBackend = true;
    default: break;
  }

// true by definition
#if ENABLED(RDOC_APPLE)
  metalBackend = true;
#endif

  // guess driver by OS & vendor, if we don't have a driver ID. This is mostly for cases where only
  // the proprietary driver exists
  if(driverProps.driverID == 0)
  {
    RDCWARN("Estimating driver based on OS & vendor ID - may be inaccurate");
    switch(m_Vendor)
    {
#if ENABLED(RDOC_WIN32)
      case GPUVendor::AMD:
      case GPUVendor::Samsung: driverProps.driverID = VK_DRIVER_ID_AMD_PROPRIETARY; break;
#elif ENABLED(RDOC_LINUX)
      // this could be radv, but we expect radv to provide the driverID
      case GPUVendor::AMD:
      case GPUVendor::Samsung: driverProps.driverID = VK_DRIVER_ID_AMD_OPEN_SOURCE; break;
#endif

#if ENABLED(RDOC_WIN32)
      case GPUVendor::Intel: driverProps.driverID = VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS; break;
#elif ENABLED(RDOC_LINUX)
      case GPUVendor::Intel: driverProps.driverID = VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA; break;
#endif

      case GPUVendor::nVidia: driverProps.driverID = VK_DRIVER_ID_NVIDIA_PROPRIETARY; break;
      case GPUVendor::Qualcomm: driverProps.driverID = VK_DRIVER_ID_QUALCOMM_PROPRIETARY; break;
      case GPUVendor::ARM: driverProps.driverID = VK_DRIVER_ID_ARM_PROPRIETARY; break;
      case GPUVendor::Imagination:
        driverProps.driverID = VK_DRIVER_ID_IMAGINATION_PROPRIETARY;
        break;
      case GPUVendor::Broadcom: driverProps.driverID = VK_DRIVER_ID_BROADCOM_PROPRIETARY; break;
      default: break;
    }
  }

  m_Major = VK_VERSION_MAJOR(physProps.driverVersion);
  m_Minor = VK_VERSION_MINOR(physProps.driverVersion);
  m_Patch = VK_VERSION_PATCH(physProps.driverVersion);

  // nvidia proprietary uses its own version packing:
  //   10 |  8  |        8       |       6
  // major|minor|secondary_branch|tertiary_branch
  if(driverProps.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY)
  {
    m_Major = ((uint32_t)(physProps.driverVersion) >> (8 + 8 + 6)) & 0x3ff;
    m_Minor = ((uint32_t)(physProps.driverVersion) >> (8 + 6)) & 0x0ff;

    uint32_t secondary = ((uint32_t)(physProps.driverVersion) >> 6) & 0x0ff;
    uint32_t tertiary = physProps.driverVersion & 0x03f;

    m_Patch = (secondary << 8) | tertiary;
  }

  // Ditto for Intel proprietary
  //  18  | 14
  // major|minor
  if(driverProps.driverID == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS)
  {
    m_Major = ((uint32_t)(physProps.driverVersion) >> 14) & 0x3fff;
    m_Minor = (uint32_t)(physProps.driverVersion) & 0x3fff;
    m_Patch = 0;
  }

  if(driverProps.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY)
  {
    // drivers before 372.54 did not handle a glslang bugfix about separated samplers,
    // and disabling texelFetch works as a workaround.

    if(Major() < 372 || (Major() == 372 && Minor() < 54))
    {
      if(active)
        RDCLOG("Enabling NV texel fetch workaround - update to a newer driver for fix");
      texelFetchBrokenDriver = true;
    }

    // this isn't exactly when the root problem started happening, but it is when it started
    // happening in a way that was easy to notice. In this version NV applied a optimisation
    // to not re-set static pipeline state when a renderpass was begun, which was previously
    // hiding the issue by conservatively re-setting the state.
    if(Major() > 532)
    {
      if(active)
        RDCLOG("Enabling NV workaround for static pipeline force-bind to preserve state");
      nvidiaStaticPipelineRebindStates = true;
    }
  }

  if(driverProps.driverID == VK_DRIVER_ID_AMD_PROPRIETARY ||
     driverProps.driverID == VK_DRIVER_ID_AMD_OPEN_SOURCE)
  {
    // for AMD the bugfix version isn't clear as version numbering wasn't strong for a while, but
    // any driver that reports a version of >= 1.0.0 is fine, as previous versions all reported
    // 0.9.0 as the version.

    if(Major() < 1)
    {
      if(active)
        RDCLOG("Enabling AMD texel fetch workaround - update to a newer driver for fix");
      texelFetchBrokenDriver = true;
    }

    // driver 18.5.2 which is vulkan version >= 2.0.33 contains the fix
    if(physProps.driverVersion < VK_MAKE_VERSION(2, 0, 33))
    {
      if(active)
        RDCLOG(
            "Enabling AMD image memory requirements workaround - update to a newer driver for fix");
      amdUnreliableImgMemReqs = true;
    }

    // driver 18.5.2 which is vulkan version >= 2.0.33 contains the fix
    if(physProps.driverVersion < VK_MAKE_VERSION(2, 0, 33))
    {
      if(active)
        RDCLOG("Enabling AMD image MSAA storage workaround - update to a newer driver for fix");
      amdStorageMSAABrokenDriver = true;
    }

    // driver 21.3.1 which is vulkan version >= 2.0.179 contains the fix
    if(physProps.driverVersion < VK_MAKE_VERSION(2, 0, 179))
    {
      if(active)
        RDCLOG("Disabling buffer_device_address on AMD - update to a newer driver for fix");
      bdaBrokenDriver = true;
    }
  }

  if(driverProps.driverID == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS)
  {
    // buffer device address doesn't work well on older drivers, even using it internally we get
    // VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS thrown when creating multiple buffers, even though we
    // don't provide opaque capture addresses at all...
    // seems fixed in 100.9466. Intel's driver versioning is inconsistent and some drivers don't
    // follow this scheme, but they also seem old?
    if(m_Major <= 100 && m_Minor < 9466)
    {
      if(active)
        RDCLOG("Disabling buffer_device_address on Intel - update to a newer driver for fix");
      bdaBrokenDriver = true;
    }

    // Currently unfixed at the time of writing, Intel's drivers require manually inserted
    // side-effects for occlusion queries to function properly if there are no other effects from a
    // pixel shader.
    // Only affects windows drivers, linux drivers are unaffected.
    intelBrokenOcclusionQueries = true;
  }

  if(driverProps.driverID == VK_DRIVER_ID_QUALCOMM_PROPRIETARY)
  {
    if(active)
      RDCLOG("Enabling Qualcomm driver workarounds");

    // not fixed yet that I know of, or unknown driver with fixes
    qualcommDrefNon2DCompileCrash = true;
    qualcommLineWidthCrash = true;

    // KHR_buffer_device_address has been tested on 622 (Quest2)
    // UBO dynamic offset leak has been fixed in early 2020, 622 tested.
    if(physProps.driverVersion < VK_MAKE_VERSION(512, 622, 0))
    {
      bdaBrokenDriver = true;
      qualcommLeakingUBOOffsets = true;
    }
  }

  if(driverProps.driverID == VK_DRIVER_ID_ARM_PROPRIETARY)
  {
    if(Major() >= 36 && Major() < 43)
    {
      if(active)
        RDCLOG(
            "Using host acceleration structure deserialisation commands on Mali - update to a "
            "newer "
            "driver for fix");
      maliBrokenASDeviceSerialisation = true;
    }
  }
}

FrameRefType GetRefType(DescriptorSlotType descType)
{
  switch(descType)
  {
    case DescriptorSlotType::Unwritten:
    case DescriptorSlotType::Sampler:
    case DescriptorSlotType::CombinedImageSampler:
    case DescriptorSlotType::SampledImage:
    case DescriptorSlotType::UniformTexelBuffer:
    case DescriptorSlotType::UniformBuffer:
    case DescriptorSlotType::UniformBufferDynamic:
    case DescriptorSlotType::InputAttachment:
    case DescriptorSlotType::InlineBlock:
    case DescriptorSlotType::AccelerationStructure: return eFrameRef_Read;
    case DescriptorSlotType::StorageImage:
    case DescriptorSlotType::StorageTexelBuffer:
    case DescriptorSlotType::StorageBuffer:
    case DescriptorSlotType::StorageBufferDynamic: return eFrameRef_ReadBeforeWrite;
    default: RDCERR("Unexpected descriptor type");
  }

  return eFrameRef_Read;
}

void DescriptorSetSlot::SetBuffer(VkDescriptorType writeType, const VkDescriptorBufferInfo &bufInfo)
{
  type = convert(writeType);
  resource = GetResID(bufInfo.buffer);
  offset = bufInfo.offset;
  range = bufInfo.range;
  if(bufInfo.range > VK_WHOLE_SIZE)
    RDCWARN("Unrepresentable buffer range size: %llx", bufInfo.range);
}

void DescriptorSetSlot::SetImage(VkDescriptorType writeType, const VkDescriptorImageInfo &imInfo,
                                 bool useSampler)
{
  type = convert(writeType);
  if(useSampler &&
     (type == DescriptorSlotType::CombinedImageSampler || type == DescriptorSlotType::Sampler))
    sampler = GetResID(imInfo.sampler);
  if(type != DescriptorSlotType::Sampler)
    resource = GetResID(imInfo.imageView);
  imageLayout = convert(imInfo.imageLayout);
}

void DescriptorSetSlot::SetTexelBuffer(VkDescriptorType writeType, ResourceId id)
{
  type = convert(writeType);
  resource = id;
}

void DescriptorSetSlot::SetAccelerationStructure(VkDescriptorType writeType,
                                                 VkAccelerationStructureKHR accelerationStructure)
{
  type = convert(writeType);
  resource = GetResID(accelerationStructure);
}

void AddBindFrameRef(DescriptorBindRefs &refs, ResourceId id, FrameRefType ref)
{
  if(id == ResourceId())
  {
    RDCERR("Unexpected NULL resource ID being added as a bind frame ref");
    return;
  }
  FrameRefType &p = refs.bindFrameRefs[id];
  // be conservative - mark refs as read before write if we see a write and a read ref on it
  p = ComposeFrameRefsUnordered(p, ref);
}

void AddImgFrameRef(DescriptorBindRefs &refs, VkResourceRecord *view, FrameRefType refType)
{
  AddBindFrameRef(refs, view->GetResourceID(), eFrameRef_Read);
  if(view->resInfo && view->resInfo->IsSparse())
    refs.sparseRefs.insert(view);
  if(view->baseResourceMem != ResourceId())
    AddBindFrameRef(refs, view->baseResourceMem, eFrameRef_Read);

  FrameRefType &p = refs.bindFrameRefs[view->baseResource];

  ImageRange imgRange = ImageRange((VkImageSubresourceRange)view->viewRange);
  imgRange.viewType = view->viewRange.viewType();

  FrameRefType maxRef =
      MarkImageReferenced(refs.bindImageStates, view->baseResource, view->resInfo->imageInfo,
                          ImageSubresourceRange(imgRange), VK_QUEUE_FAMILY_IGNORED, refType);

  p = ComposeFrameRefsDisjoint(p, maxRef);
}

void AddMemFrameRef(DescriptorBindRefs &refs, ResourceId mem, VkDeviceSize offset,
                    VkDeviceSize size, FrameRefType refType)
{
  if(mem == ResourceId())
  {
    RDCERR("Unexpected NULL resource ID being added as a bind frame ref");
    return;
  }
  FrameRefType &p = refs.bindFrameRefs[mem];
  FrameRefType maxRef =
      MarkMemoryReferenced(refs.bindMemRefs, mem, offset, size, refType, ComposeFrameRefsUnordered);
  p = ComposeFrameRefsDisjoint(p, maxRef);
}

void DescriptorSetSlot::AccumulateBindRefs(DescriptorBindRefs &refs, VulkanResourceManager *rm) const
{
  RDCCOMPILE_ASSERT(uint64_t(DescriptorSlotImageLayout::Count) <= 0xff,
                    "DescriptorSlotImageLayout is no longer 8-bit");
  RDCCOMPILE_ASSERT(uint64_t(DescriptorSlotType::Count) <= 0xff,
                    "DescriptorSlotType is no longer 8-bit");
  RDCCOMPILE_ASSERT(sizeof(DescriptorSetSlot) == 32, "DescriptorSetSlot is no longer 32 bytes");
  RDCCOMPILE_ASSERT(offsetof(DescriptorSetSlot, offset) == 8,
                    "DescriptorSetSlot first uint64_t bitpacking isn't working as expected");

  VkResourceRecord *bufView = NULL, *imgView = NULL, *buffer = NULL, *accStruct = NULL;

  switch(type)
  {
    case DescriptorSlotType::UniformTexelBuffer:
    case DescriptorSlotType::StorageTexelBuffer: bufView = rm->GetResourceRecord(resource); break;
    case DescriptorSlotType::StorageBuffer:
    case DescriptorSlotType::StorageBufferDynamic:
    case DescriptorSlotType::UniformBuffer:
    case DescriptorSlotType::UniformBufferDynamic: buffer = rm->GetResourceRecord(resource); break;
    case DescriptorSlotType::CombinedImageSampler:
    case DescriptorSlotType::SampledImage:
    case DescriptorSlotType::StorageImage:
    case DescriptorSlotType::InputAttachment: imgView = rm->GetResourceRecord(resource); break;
    case DescriptorSlotType::AccelerationStructure:
      accStruct = rm->GetResourceRecord(resource);
      break;
    default: break;
  }

  FrameRefType ref = GetRefType(type);

  if(bufView)
  {
    AddBindFrameRef(refs, bufView->GetResourceID(), eFrameRef_Read);
    if(bufView->resInfo && bufView->resInfo->IsSparse())
      refs.sparseRefs.insert(bufView);
    if(bufView->baseResource != ResourceId())
      AddBindFrameRef(refs, bufView->baseResource, eFrameRef_Read);
    if(bufView->baseResourceMem != ResourceId())
      AddMemFrameRef(refs, bufView->baseResourceMem, bufView->memOffset, bufView->memSize, ref);
    if(bufView->storable)
      refs.storableRefs.insert(rm->GetResourceRecord(bufView->baseResource));
  }
  if(imgView)
  {
    AddImgFrameRef(refs, imgView, ref);
  }
  if(sampler != ResourceId())
  {
    AddBindFrameRef(refs, sampler, eFrameRef_Read);
  }
  if(buffer)
  {
    AddBindFrameRef(refs, resource, eFrameRef_Read);
    if(buffer->resInfo && buffer->resInfo->IsSparse())
      refs.sparseRefs.insert(buffer);
    if(buffer->baseResource != ResourceId())
      AddMemFrameRef(refs, buffer->baseResource, buffer->memOffset, buffer->memSize, ref);
    if(buffer->storable)
      refs.storableRefs.insert(buffer);
  }
  if(accStruct)
  {
    AddBindFrameRef(refs, resource, eFrameRef_Read);
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None
#undef Always

#include "catch/catch.hpp"

bool operator==(const VkImageMemoryBarrier &a, const VkImageMemoryBarrier &b)
{
  return memcmp(&a, &b, sizeof(VkImageMemoryBarrier)) == 0;
}
bool operator<(const VkImageMemoryBarrier &a, const VkImageMemoryBarrier &b)
{
  return memcmp(&a, &b, sizeof(VkImageMemoryBarrier)) < 0;
}

TEST_CASE("Validate CombineDepthStencilLayouts works", "[vulkan]")
{
  VkImage imga, imgb;

  // give the fake handles values
  {
    uint64_t a = 1;
    uint64_t b = 2;
    memcpy(&imga, &a, sizeof(a));
    memcpy(&imgb, &b, sizeof(b));
  }

  rdcarray<VkImageMemoryBarrier> barriers, ref;

  VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  SECTION("Ignored cases")
  {
    VkImageAspectFlags aspects[] = {VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                                    VK_IMAGE_ASPECT_STENCIL_BIT,
                                    VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT};
    // only care about split depth and stencil for the same image
    for(VkImageAspectFlags aspect : aspects)
    {
      barriers.clear();

      // whole image
      b.image = imga;
      b.subresourceRange = {aspect, 0, 4, 0, 6};
      barriers.push_back(b);

      b.image = imgb;
      b.subresourceRange = {aspect, 0, 4, 0, 6};
      barriers.push_back(b);

      ref = barriers;
      CombineDepthStencilLayouts(barriers);

      CHECK((ref == barriers));

      barriers.clear();

      // images split into different mips/arrays
      b.image = imga;
      b.subresourceRange = {aspect, 0, 1, 0, 3};
      barriers.push_back(b);
      b.subresourceRange = {aspect, 1, 2, 0, 3};
      barriers.push_back(b);
      b.subresourceRange = {aspect, 3, 1, 0, 3};
      barriers.push_back(b);
      b.subresourceRange = {aspect, 0, 4, 3, 3};
      barriers.push_back(b);

      b.image = imgb;
      b.subresourceRange = {aspect, 0, 1, 0, 3};
      barriers.push_back(b);
      b.subresourceRange = {aspect, 1, 3, 0, 3};
      barriers.push_back(b);
      b.subresourceRange = {aspect, 0, 4, 3, 3};
      barriers.push_back(b);

      ref = barriers;
      CombineDepthStencilLayouts(barriers);

      CHECK((ref == barriers));
    }
  };

  SECTION("Possible but unmergeable cases")
  {
    barriers.clear();

    // could merge, but different images
    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imgb;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    ref = barriers;
    CombineDepthStencilLayouts(barriers);

    CHECK((ref == barriers));

    barriers.clear();

    // could merge, but different subresource ranges
    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imgb;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 2, 0, 1};
    barriers.push_back(b);

    ref = barriers;
    CombineDepthStencilLayouts(barriers);

    CHECK((ref == barriers));

    barriers.clear();

    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imgb;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 1, 1};
    barriers.push_back(b);

    ref = barriers;
    CombineDepthStencilLayouts(barriers);

    CHECK((ref == barriers));

    barriers.clear();

    // could merge, but different layouts
    b.image = imga;
    b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 2, 0, 1};
    barriers.push_back(b);

    b.image = imgb;
    b.oldLayout = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 2, 0, 1};
    barriers.push_back(b);

    ref = barriers;
    CombineDepthStencilLayouts(barriers);

    CHECK((ref == barriers));

    barriers.clear();

    b.image = imga;
    b.newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 2, 0, 1};
    barriers.push_back(b);

    b.image = imgb;
    b.newLayout = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 2, 0, 1};
    barriers.push_back(b);

    ref = barriers;
    CombineDepthStencilLayouts(barriers);

    CHECK((ref == barriers));
  };

  SECTION("Whole-image depth and separate stencil barriers are merged when possible")
  {
    barriers.clear();

    CHECK((ref == barriers));
    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imgb;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    ref = barriers;
    CombineDepthStencilLayouts(barriers);

    REQUIRE(barriers.size() == 2);
    // aspect mask is combined now
    CHECK(barriers[0].subresourceRange.aspectMask ==
          (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
    // otherwise the barrier should be the same
    CHECK(barriers[0].subresourceRange.baseMipLevel == 0);
    CHECK(barriers[0].subresourceRange.levelCount == 1);
    CHECK(barriers[0].subresourceRange.baseArrayLayer == 0);
    CHECK(barriers[0].subresourceRange.layerCount == 1);
    CHECK(barriers[0].oldLayout == ref[0].oldLayout);
    CHECK(barriers[0].newLayout == ref[0].newLayout);
    // the last barrier should be the same
    CHECK((barriers[1] == ref[2]));
  };

  SECTION("Split depth and separate stencil barriers are merged when possible")
  {
    barriers.clear();

    CHECK((ref == barriers));
    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 0, 1};
    barriers.push_back(b);

    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    barriers.push_back(b);

    b.image = imga;
    b.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 1, 1, 0, 1};
    barriers.push_back(b);

    CombineDepthStencilLayouts(barriers);

    REQUIRE(barriers.size() == 2);

    // aspect mask is combined now
    CHECK(barriers[0].subresourceRange.aspectMask ==
          (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
    // otherwise the barrier should be the same
    CHECK(barriers[0].subresourceRange.baseMipLevel == 0);
    CHECK(barriers[0].subresourceRange.levelCount == 1);
    CHECK(barriers[0].subresourceRange.baseArrayLayer == 0);
    CHECK(barriers[0].subresourceRange.layerCount == 1);

    // aspect mask is combined now
    CHECK(barriers[1].subresourceRange.aspectMask ==
          (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
    // otherwise the barrier should be the same
    CHECK(barriers[1].subresourceRange.baseMipLevel == 1);
    CHECK(barriers[1].subresourceRange.levelCount == 1);
    CHECK(barriers[1].subresourceRange.baseArrayLayer == 0);
    CHECK(barriers[1].subresourceRange.layerCount == 1);
  };
}

#endif
