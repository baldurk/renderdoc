/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018 Baldur Karlsson
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
#include "driver/vulkan/cpp_codec/vk_cpp_codec_state.h"
#include "driver/vulkan/vk_resources.h"

namespace vk_cpp_codec
{
MemRange MemRange::MakeRange(SDObject *offset, SDObject *reqs)
{
  start = offset->AsUInt64();
  end = start + reqs->GetChild(0)->AsUInt64();
  return *this;
}

bool MemRange::Intersect(MemRange &r)
{
  // interval '1' and '2' start and end points:
  uint64_t i1_start = r.start;
  uint64_t i1_end = r.end;
  uint64_t i2_start = start;
  uint64_t i2_end = end;

  // two intervals i1 [s, e] and i2 [s, e] intersect
  // if X = max(i1.s, i2.s) < Y = min(i1.e, i2.e).
  return std::max<uint64_t>(i1_start, i2_start) < std::min<uint64_t>(i1_end, i2_end);
}

AccessState AccessStateClearTransition(AccessState s)
{
  switch(s)
  {
    case ACCESS_STATE_INIT:
    case ACCESS_STATE_WRITE: return ACCESS_STATE_CLEAR;
    case ACCESS_STATE_READ: return ACCESS_STATE_RESET;
    default: return s;
  }
}

AccessState AccessStateWriteTransition(AccessState s)
{
  switch(s)
  {
    case ACCESS_STATE_INIT: return ACCESS_STATE_WRITE;
    case ACCESS_STATE_READ: return ACCESS_STATE_RESET;
    default: return s;
  }
}

AccessState AccessStateReadTransition(AccessState s)
{
  switch(s)
  {
    case ACCESS_STATE_INIT:
    case ACCESS_STATE_WRITE: return ACCESS_STATE_READ;
    default: return s;
  }
}

AccessState AccessStateReadWriteTransition(AccessState s)
{
  switch(s)
  {
    case ACCESS_STATE_INIT:
    case ACCESS_STATE_READ:
    case ACCESS_STATE_WRITE: return ACCESS_STATE_RESET;
    default: return s;
  }
}

std::function<AccessState(AccessState)> GetAccessStateTransition(AccessAction action)
{
  switch(action)
  {
    case ACCESS_ACTION_READ: return AccessStateReadTransition;
    case ACCESS_ACTION_WRITE: return AccessStateWriteTransition;
    case ACCESS_ACTION_READ_WRITE: return AccessStateReadWriteTransition;
    case ACCESS_ACTION_CLEAR: return AccessStateClearTransition;
    default: RDCASSERT(0); return {};
  }
}

bool MemoryAllocationWithBoundResources::HasAliasedResources()
{
  if(boundResources.empty())
    hasAliasedResources = HasAliasedResourcesFalse;
  RDCASSERT(hasAliasedResources != HasAliasedResourcesUnknown);
  return hasAliasedResources == HasAliasedResourcesTrue;
}

bool MemoryAllocationWithBoundResources::NeedsReset()
{
  // allocations that have aliased resources need full reset.
  if(HasAliasedResources())
    return true;

  // Loop through the resources, looking for one that needs a reset.
  for(BoundResourcesIter it = Begin(); it != End(); it++)
  {
    // All bound resources must have a known reset requiriement before calling `NeedsReset`
    RDCASSERT(it->reset != RESET_REQUIREMENT_UNKNOWN);
    if(it->reset == RESET_REQUIREMENT_RESET)
    {
      return true;
    }
  }
  return false;
}

bool MemoryAllocationWithBoundResources::NeedsInit()
{
  // allocations that have aliased resources don't need initialization, only reset.
  if(HasAliasedResources())
    return false;

  // Loop through the resources, looking for one that needs an init.
  for(BoundResourcesIter it = Begin(); it != End(); it++)
  {
    // All bound resources must have a known reset requiriement before calling `NeedsInit`
    RDCASSERT(it->reset != RESET_REQUIREMENT_UNKNOWN);
    if(it->reset == RESET_REQUIREMENT_INIT)
    {
      return true;
    }
  }
  return false;
}

std::vector<size_t> MemoryAllocationWithBoundResources::OrderByResetRequiremnet()
{
  std::vector<size_t> result;
  result.reserve(boundResources.size());
  for(uint32_t reset_i = RESET_REQUIREMENT_RESET; reset_i <= RESET_REQUIREMENT_NO_RESET; reset_i++)
  {
    for(size_t i = 0; i < boundResources.size(); i++)
    {
      if(boundResources[i].reset == (ResetRequirement)reset_i)
      {
        result.push_back(i);
      }
    }
  }

  // All bound resources must have a known reset requiriement (RESET, INIT, NO_RESET)
  // before calling `OrderByResetRequiremnet`. Therefore, result should
  // have one entry for each bound resource.
  RDCASSERT(result.size() == boundResources.size());
  return result;
}

bool MemoryAllocationWithBoundResources::CheckAliasedResources(MemRange r)
{
  for(size_t i = 0; i < ranges.size(); i++)
  {
    if(r.Intersect(ranges[i]))
    {
      hasAliasedResources = HasAliasedResourcesTrue;
      return true;
    }
  }
  ranges.push_back(r);
  hasAliasedResources = HasAliasedResourcesFalse;
  return false;
}

void MemoryAllocationWithBoundResources::Access(uint64_t cmdQueueFamily, VkSharingMode sharingMode,
                                                AccessAction action, uint64_t offset, uint64_t size)
{
  uint64_t end = offset + size;
  std::function<AccessState(AccessState)> accessStateTransition = GetAccessStateTransition(action);
  for(IntervalsIter<MemoryState> it = memoryState.find(offset);
      it != memoryState.end() && it.start() < end; it++)
  {
    MemoryState state = it.value();
    bool modified = false;
    uint64_t memID = allocate->FindChild("Memory")->AsUInt64();
    uint64_t iStart = std::max(offset, it.start());
    uint64_t iEnd = std::min(end, it.end());
    if(state.queueFamily != cmdQueueFamily && cmdQueueFamily != VK_QUEUE_FAMILY_IGNORED &&
       sharingMode != VK_SHARING_MODE_CONCURRENT)
    {
      if(state.queueFamily == VK_QUEUE_FAMILY_IGNORED)
      {
        // Resource has not yet been used by any queue family
        // Automatically acquired by the current queue family
        state.queueFamily = cmdQueueFamily;
        state.isAcquired = true;
        modified = true;
        RDCDEBUG("Memory %llu range [%llu,%llu) implicitly acquired by queue family %llu.", memID,
                 iStart, iEnd, cmdQueueFamily);
      }
      else
      {
        RDCWARN(
            "Memory %llu range [%llu,%llu) accessed by queue family %llu while owned by queue "
            "family %llu.",
            memID, iStart, iEnd, cmdQueueFamily, state.queueFamily);
      }
    }
    AccessState newAccessState = accessStateTransition(state.accessState);
    if(newAccessState != state.accessState)
    {
      state.accessState = newAccessState;
      modified = true;
    }
    if(modified)
    {
      it.setValue(offset, end, state);
    }
  }
}

void MemoryAllocationWithBoundResources::TransitionQueueFamily(uint64_t cmdQueueFamily,
                                                               VkSharingMode sharingMode,
                                                               uint64_t srcQueueFamily,
                                                               uint64_t dstQueueFamily,
                                                               uint64_t offset, uint64_t size)
{
  if(srcQueueFamily == dstQueueFamily || sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    return;
  }
  uint64_t memID = allocate->FindChild("Memory")->AsUInt64();
  uint64_t end = offset + size;
  for(IntervalsIter<MemoryState> it = memoryState.find(offset);
      it != memoryState.end() && it.start() < end; it++)
  {
    MemoryState state = it.value();
    bool modified = false;
    uint64_t iStart = std::max(offset, it.start());
    uint64_t iEnd = std::min(end, it.end());
    if(cmdQueueFamily == srcQueueFamily)
    {
      // Release
      if(state.queueFamily == VK_QUEUE_FAMILY_IGNORED)
      {
        // We have yet to see any use of this memory on any queue.
        // Assume it was previously used on the queue that is releasing it.
        state.queueFamily = srcQueueFamily;
        state.isAcquired = true;
        modified = true;
      }

      if(srcQueueFamily != state.queueFamily)
      {
        RDCWARN(
            "Memory %llu range [%llu,%llu) released by queue family %llu while owned by queue "
            "family %llu",
            memID, iStart, iEnd, srcQueueFamily, state.queueFamily);
      }
      if(state.isAcquired)
      {
        RDCDEBUG(
            "Memory %llu range [%llu,%llu) released by queue family %llu to queue family %llu.",
            memID, iStart, iEnd, srcQueueFamily, dstQueueFamily);
        state.isAcquired = false;
        modified = true;
      }
      else
      {
        RDCWARN(
            "Memory %llu range [%llu,%llu) released by queue family %llu while it was not "
            "acquired.",
            memID, iStart, iEnd, srcQueueFamily);
      }

      if(dstQueueFamily == VK_QUEUE_FAMILY_EXTERNAL ||
         dstQueueFamily == VK_QUEUE_FAMILY_EXTERNAL_KHR ||
         dstQueueFamily == VK_QUEUE_FAMILY_FOREIGN_EXT)
      {
        // We won't see any acquires from the dstQueueFamily.
        // Assume that the external queue family immediately acquires, and then releasese the
        // resource.
        // This way, the resource will be in the correct state when it is acquired back again.
        state.queueFamily = dstQueueFamily;
        modified = true;
      }
    }
    else if(cmdQueueFamily == dstQueueFamily)
    {
      // Acquire
      if(state.queueFamily == VK_QUEUE_FAMILY_IGNORED)
      {
        // We have yet to see any use of this memory on any queue.
        // Assume it was previously used and released by the srcQueueFamily.
        state.queueFamily = srcQueueFamily;
        state.isAcquired = false;
      }

      if(srcQueueFamily != state.queueFamily)
      {
        RDCWARN(
            "Memory %llu range [%llu,%llu) acquired from family %llu while owned by queue family "
            "%llu",
            memID, iStart, iEnd, srcQueueFamily, state.queueFamily);
      }
      if(state.isAcquired)
      {
        RDCWARN(
            "Memory %llu range [%llu,%llu) acquired by queue family %llu while still owned by "
            "queue family %llu.",
            memID, iStart, iEnd, dstQueueFamily, srcQueueFamily);
      }
      else
      {
        RDCDEBUG(
            "Memory %llu range [%llu,%llu) acquired by queue family %llu from queue family %llu.",
            memID, iStart, iEnd, dstQueueFamily, srcQueueFamily);
        state.isAcquired = true;
        state.queueFamily = dstQueueFamily;
        modified = true;
      }
    }
    else
    {
      RDCWARN(
          "Memory %llu range [%llu,%llu) was transitioned from queue family %llu to queue family "
          "%llu by queue family %llu. The transition must be done by the source and destination "
          "queue families.",
          memID, iStart, iEnd, srcQueueFamily, dstQueueFamily, cmdQueueFamily);
    }
    if(modified)
    {
      it.setValue(offset, end, state);
    }
  }
}

uint32_t FrameGraph::FindCmdBufferIndex(SDObject *o)
{
  for(uint32_t i = 0; i < records.size(); i++)
  {
    if(records[i].cb->AsUInt64() == o->AsUInt64())
      return i;
  }
  RDCASSERT(0);
  return 0xFFFFFFFF;
}

size_t DescriptorBinding::Size()
{
  switch(type)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      RDCASSERT(bufferBindings.empty() && texelViewBindings.empty());
      return imageBindings.size();

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      RDCASSERT(imageBindings.empty() && texelViewBindings.empty());
      return bufferBindings.size();

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      RDCASSERT(imageBindings.empty() && bufferBindings.empty());
      return texelViewBindings.size();

    default: RDCASSERT(0); break;
  }
  return 0;
}

void DescriptorBinding::SetBindingObj(size_t index, SDObject *o, bool initialization)
{
  RDCASSERT(index < updated.size());
  if(!initialization)
    updated[index] = true;

  switch(type)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      RDCASSERT(index < imageBindings.size());
      if(o->NumChildren() == 0)
        return;    // invalidated binding
      RDCASSERT(o->NumChildren() == 3);
      imageBindings[index] = BoundImage(o->GetChild(0)->AsUInt64(),                    // sampler
                                        o->GetChild(1)->AsUInt64(),                    // imageIvew
                                        (VkImageLayout)o->GetChild(2)->AsUInt64());    // imageLayout
      break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      RDCASSERT(index < bufferBindings.size());
      if(o->NumChildren() == 0)
        return;    // invalidated binding
      RDCASSERT(o->NumChildren() == 3);
      bufferBindings[index] = BoundBuffer(o->GetChild(0)->AsUInt64(),    // buffer
                                          o->GetChild(1)->AsUInt64(),    // offset
                                          o->GetChild(2)->AsUInt64(),    // size
                                          0);                            // dynamicOffset
      break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      RDCASSERT(index < texelViewBindings.size());
      texelViewBindings[index] = BoundTexelView(o->AsUInt64());    // buffer
      break;

    default: RDCASSERT(0); break;
  }
}

void DescriptorBinding::CopyBinding(size_t index, const DescriptorBinding &other, size_t otherIndex)
{
  RDCASSERT(index < updated.size());
  updated[index] = true;

  RDCASSERT(type == other.type);
  switch(type)
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      RDCASSERT(index < imageBindings.size());
      RDCASSERT(otherIndex < other.imageBindings.size());
      imageBindings[index] = other.imageBindings[otherIndex];
      break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      RDCASSERT(index < bufferBindings.size());
      RDCASSERT(otherIndex < other.bufferBindings.size());
      bufferBindings[index] = other.bufferBindings[otherIndex];
      break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      RDCASSERT(index < texelViewBindings.size());
      RDCASSERT(otherIndex < other.texelViewBindings.size());
      texelViewBindings[index] = other.texelViewBindings[otherIndex];
      break;

    default: RDCASSERT(0); break;
  }
}

void DescriptorBinding::Resize(uint64_t aType, size_t elementCount)
{
  type = (VkDescriptorType)aType;
  switch(type)
  {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: bufferBindings.resize(elementCount); break;

    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: imageBindings.resize(elementCount); break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: texelViewBindings.resize(elementCount); break;

    default: RDCASSERT(0); break;
  }
  updated.resize(elementCount, false);
}

bool DescriptorBinding::NeedsReset(size_t element)
{
  RDCASSERT(updated.size() > element);
  return updated[element];
}

bool DescriptorSetInfo::NeedsReset(size_t binding, size_t element)
{
  RDCASSERT(bindings.find(binding) != bindings.end());
  return bindings[binding].NeedsReset(element);
}

void BindingState::attachmentUse(uint64_t subpassId, size_t attachmentId)
{
  if(attachmentId == VK_ATTACHMENT_UNUSED)
  {
    return;
  }
  RDCASSERT(attachmentId < attachmentFirstUse.size());
  attachmentFirstUse[attachmentId] = std::min(attachmentFirstUse[attachmentId], subpassId);
  attachmentLastUse[attachmentId] = subpassId;
}

void BindingState::BeginRenderPass(SDObject *aRenderPass, SDObject *aFramebuffer,
                                   SDObject *aRenderArea)
{
  subpassIndex = 0;
  renderPass = aRenderPass;
  framebuffer = aFramebuffer;
  uint64_t width = aFramebuffer->GetChild(6)->AsUInt64();
  uint64_t height = aFramebuffer->GetChild(7)->AsUInt64();
  isFullRenderArea = (aRenderArea->GetChild(0)->GetChild(0)->AsUInt64() == 0) &&
                     (aRenderArea->GetChild(0)->GetChild(1)->AsUInt64() == 0) &&
                     (aRenderArea->GetChild(1)->GetChild(0)->AsUInt64() == width) &&
                     (aRenderArea->GetChild(1)->GetChild(1)->AsUInt64() == height);

  size_t numAttachments = aRenderPass->GetChild(4)->NumChildren();
  attachmentFirstUse.clear();
  attachmentFirstUse.resize(numAttachments, UINT64_MAX);

  attachmentLastUse.clear();
  attachmentLastUse.resize(numAttachments, UINT64_MAX);

  SDObject *subpasses = aRenderPass->GetChild(6);

  for(size_t s = 0; s < subpasses->NumChildren(); s++)
  {
    SDObject *subpass = subpasses->GetChild(s);
    SDObject *inputAttachments = subpass->GetChild(3);
    SDObject *colorAttachments = subpass->GetChild(5);
    SDObject *resolveAttachments = subpass->GetChild(6);
    SDObject *depthStencilAttachment = subpass->GetChild(7);

    for(size_t j = 0; j < inputAttachments->NumChildren(); j++)
    {
      attachmentUse(s, inputAttachments->GetChild(j)->GetChild(0)->AsUInt32());
    }

    for(size_t j = 0; j < colorAttachments->NumChildren(); j++)
    {
      attachmentUse(s, colorAttachments->GetChild(j)->GetChild(0)->AsUInt32());
    }

    for(size_t j = 0; j < resolveAttachments->NumChildren(); j++)
    {
      attachmentUse(s, resolveAttachments->GetChild(j)->GetChild(0)->AsUInt32());
    }
    if(!depthStencilAttachment->IsNULL())
    {
      attachmentUse(s, depthStencilAttachment->GetChild(0)->AsUInt32());
    }
  }

  attachmentLayout.clear();
  attachmentLayout.resize(numAttachments, VK_IMAGE_LAYOUT_MAX_ENUM);

  for(size_t a = 0; a < numAttachments; a++)
  {
    SDObject *renderpassAttachments = renderPass->FindChild("pAttachments");
    SDObject *attachmentDesc = renderpassAttachments->GetChild(a);
    attachmentLayout[a] = (VkImageLayout)attachmentDesc->FindChild("initialLayout")->AsUInt64();
  }
}

ImageSubresourceRangeIter &ImageSubresourceRangeIter::operator++()
{
  res.level++;
  if(res.level >= range.baseMipLevel + range.levelCount)
  {
    res.level = range.baseMipLevel;
    res.layer++;
    if(res.layer >= range.baseArrayLayer + range.layerCount)
    {
      res.layer = range.baseArrayLayer;
      VkImageAspectFlags aspect = (VkImageAspectFlags)res.aspect;
      while(aspect < range.aspectMask && aspect < VK_IMAGE_ASPECT_END_BIT)
      {
        aspect <<= 1;
        if(aspect & range.aspectMask)
        {
          res.aspect = (VkImageAspectFlagBits)aspect;
          return *this;
        }
      }
      setEnd();
    }
  }
  return *this;
}

ImageSubresourceRangeIter ImageSubresourceRangeIter::end(const ImageSubresourceRange &range)
{
  ImageSubresourceRangeIter it;
  it.range = range;
  it.res.image = range.image;
  it.setEnd();
  return it;
}

ImageSubresourceRangeIter ImageSubresourceRangeIter::begin(const ImageSubresourceRange &range)
{
  ImageSubresourceRangeIter it;
  it.range = range;
  it.res.image = range.image;
  if(range.aspectMask == 0 || range.levelCount == 0 || range.layerCount == 0)
  {
    it.setEnd();
  }
  else
  {
    VkImageAspectFlags aspect(1);
    it.res.aspect = (VkImageAspectFlagBits)1;
    while(!(aspect & range.aspectMask))
    {
      aspect <<= 1;
    }
    it.res.aspect = (VkImageAspectFlagBits)aspect;
    it.res.level = range.baseMipLevel;
    it.res.layer = range.baseArrayLayer;
  }
  return it;
}

void ImageSubresourceState::CheckLayout(VkImageLayout requestedLayout)
{
  if(layout == VK_IMAGE_LAYOUT_MAX_ENUM)
  {
    // This image subresource has not yet been used, and had no start layout in BeginCapture.
    if(requestedLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
      RDCWARN(
          "Image first used in layout %s, but no start layout was found in BeginCapture. "
          "Image: %llu, layer: %llu, level: %llu, aspect: %s",
          ToStr(requestedLayout).c_str(), image, layer, mipLevel, ToStr(aspect).c_str());
    }
    layout = requestedLayout;
  }
  if(layout != requestedLayout && requestedLayout != VK_IMAGE_LAYOUT_UNDEFINED)
  {
    RDCWARN(
        "Image requested in layout %s, but was in layout %s. Image: %llu, layer: %llu, level: "
        "%llu, "
        "aspect: %s.",
        ToStr(requestedLayout).c_str(), ToStr(layout).c_str(), image, layer, mipLevel,
        ToStr(aspect).c_str());
  }
}

void ImageSubresourceState::CheckQueueFamily(uint64_t cmdQueueFamily)
{
  if(sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    return;
  }
  if(queueFamily == VK_QUEUE_FAMILY_IGNORED)
  {
    // No queue family has been set. Assume this use implicitly acquires the queue.
    queueFamily = cmdQueueFamily;
    isAcquiredByQueue = true;
  }
  if(cmdQueueFamily != queueFamily)
  {
    RDCWARN(
        "Image used by queue family %llu while owned by queue family %llu. "
        "Image: %llu, layer: %llu, level: %llu, aspect: %s",
        cmdQueueFamily, queueFamily, image, layer, mipLevel, ToStr(aspect).c_str());
  }
}

void ImageSubresourceState::Initialize(VkImageLayout aStartLayout, uint64_t aStartQueueFamily)
{
  isInitialized = true;
  startLayout = aStartLayout;
  layout = aStartLayout;
  startQueueFamily = aStartQueueFamily;
  queueFamily = aStartQueueFamily;
  if(aStartQueueFamily != VK_QUEUE_FAMILY_IGNORED)
  {
    isAcquiredByQueue = true;
  }
}

void ImageSubresourceState::Access(
    uint64_t cmdQueueFamily, VkImageLayout requestedLayout,
    const std::function<vk_cpp_codec::AccessState(vk_cpp_codec::AccessState)> &transition)
{
  CheckLayout(requestedLayout);
  CheckQueueFamily(cmdQueueFamily);
  accessState = transition(accessState);
}

void ImageSubresourceState::Transition(uint64_t cmdQueueFamily, VkImageLayout oldLayout,
                                       VkImageLayout newLayout, uint64_t srcQueueFamily,
                                       uint64_t dstQueueFamily)
{
  isTransitioned = true;

  if(srcQueueFamily != dstQueueFamily && sharingMode != VK_SHARING_MODE_CONCURRENT)
  {
    RDCWARN(
        "Queue transition detected! This is completely untested. Please let us know what breaks "
        "(with a capture that reproduces it, if possible).");
    if(cmdQueueFamily == srcQueueFamily)
    {
      // Release
      if(srcQueueFamily != queueFamily)
      {
        RDCWARN(
            "Image released by queue family %llu while owned by queue family %llu. "
            "Image: %llu, layer: %llu, level: %llu, aspect: %s",
            srcQueueFamily, queueFamily, image, layer, mipLevel, ToStr(aspect).c_str());
      }
      if(!isAcquiredByQueue)
      {
        RDCWARN(
            "Image released multiple times by queue family %llu. "
            "Image: %llu, layer: %llu, level: %llu, aspect: %s",
            srcQueueFamily, image, layer, mipLevel, ToStr(aspect).c_str());
      }
      isAcquiredByQueue = false;

      // Wait until the `acquire` to do the layout transition
      return;
    }
    else if(cmdQueueFamily == dstQueueFamily)
    {
      // Acquire
      if(isAcquiredByQueue)
      {
        RDCWARN(
            "Image acquired by queue %llu before being released by queue %llu. "
            "Image: %llu, layer: %llu, level: %llu, aspect: %s",
            dstQueueFamily, srcQueueFamily, image, layer, mipLevel, ToStr(aspect).c_str());
      }
      isAcquiredByQueue = true;
      queueFamily = dstQueueFamily;
    }
  }
  CheckQueueFamily(cmdQueueFamily);
  CheckLayout(oldLayout);
  layout = newLayout;
}

ImageSubresourceRange ImageState::FullRange()
{
  ImageSubresourceRange range;
  range.image = image;
  range.aspectMask = availableAspects;
  range.baseMipLevel = 0;
  range.levelCount = mipLevels;
  range.baseArrayLayer = 0;
  range.layerCount = arrayLayers;
  return range;
}

ImageState::ImageState(uint64_t aImage, SDObject *ci) : image(aImage)
{
  std::string ci_type(Type(ci));
  if(ci_type == "VkImageCreateInfo")
  {
    type = (VkImageType)ci->FindChild("imageType")->AsUInt64();
    format = (VkFormat)ci->FindChild("format")->AsUInt64();
    mipLevels = ci->FindChild("mipLevels")->AsUInt64();
    arrayLayers = ci->FindChild("arrayLayers")->AsUInt64();
    SDObject *extent = ci->FindChild("extent");
    width = extent->FindChild("width")->AsUInt64();
    height = extent->FindChild("width")->AsUInt64();
    depth = extent->FindChild("depth")->AsUInt64();
    initialLayout = (VkImageLayout)ci->FindChild("initialLayout")->AsUInt64();
  }
  else if(ci_type == "VkSwapchainCreateInfoKHR")
  {
    type = VK_IMAGE_TYPE_2D;
    format = (VkFormat)ci->FindChild("imageFormat")->AsUInt64();
    mipLevels = 1;
    arrayLayers = ci->FindChild("imageArrayLayers")->AsUInt64();
    SDObject *extent = ci->FindChild("imageExtent");
    width = extent->FindChild("width")->AsUInt64();
    height = extent->FindChild("height")->AsUInt64();
    depth = 1;
    initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  }
  else
  {
    RDCASSERT(0);
  }

  if(IsDepthAndStencilFormat(format))
  {
    // Depth and stencil image
    availableAspects = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  else if(IsDepthOrStencilFormat(format))
  {
    if(IsStencilFormat(format))
    {
      // Stencil only image
      availableAspects = VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else
    {
      // Depth only image
      availableAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
  }
  else
  {
    // Color image
    switch(GetYUVPlaneCount(format))
    {
      case 1: availableAspects = VK_IMAGE_ASPECT_COLOR_BIT; break;
      case 2: availableAspects = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT; break;
      case 3:
        availableAspects =
            VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT;
        break;
      default: RDCASSERT(0);
    }
  }
  if(type == VK_IMAGE_TYPE_3D)
  {
    arrayLayers = depth;
  }
  ImageSubresourceRange range = FullRange();
  for(ImageSubresourceRangeIter res_it = range.begin(); res_it != range.end(); res_it++)
  {
    subresourceStates.insert(ImageSubresourceStateMapPair(
        *res_it, ImageSubresourceState(image, initialLayout, sharingMode, *res_it)));
  }
}

VkImageAspectFlags ImageState::NormalizeAspectMask(VkImageAspectFlags aspectMask) const
{
  if(aspectMask > VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM)
  {
    return availableAspects;
  }
  if((aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) && GetYUVPlaneCount(format) > 1)
  {
    // Accessing the Color aspect of a multi-planar image is equivilanet to accessing all planes.
    RDCASSERT(aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
    RDCASSERT((availableAspects & (VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT |
                                   VK_IMAGE_ASPECT_PLANE_2_BIT)) == availableAspects);
    aspectMask = availableAspects;
  }
  return aspectMask;
}

ImageSubresourceRange ImageState::Range(VkImageAspectFlags aspectMask, uint64_t baseMipLevel,
                                        uint64_t levelCount, uint64_t baseArrayLayer,
                                        uint64_t layerCount, bool is2DView)
{
  ImageSubresourceRange range;
  range.image = image;
  range.aspectMask = NormalizeAspectMask(aspectMask);
  range.baseMipLevel = baseMipLevel;
  if(levelCount == VK_REMAINING_MIP_LEVELS)
  {
    range.levelCount = mipLevels - baseMipLevel;
  }
  else
  {
    range.levelCount = levelCount;
  }
  if(type == VK_IMAGE_TYPE_3D && !is2DView)
  {
    RDCASSERT(baseArrayLayer == 0);
    RDCASSERT(layerCount == 1 || layerCount == VK_REMAINING_ARRAY_LAYERS);
    range.baseArrayLayer = 0;
    range.layerCount = arrayLayers;
  }
  else
  {
    range.baseArrayLayer = baseArrayLayer;
    if(layerCount == VK_REMAINING_ARRAY_LAYERS)
    {
      range.layerCount = arrayLayers - baseArrayLayer;
    }
    else
    {
      range.layerCount = layerCount;
    }
  }
  return range;
}

ImageSubresourceRangeStateChanges ImageState::RangeChanges(ImageSubresourceRange range) const
{
  ImageSubresourceRangeStateChanges changes{};
  bool firstLayoutRes = true;
  bool firstQueueRes = true;
  for(ImageSubresourceRangeIter res_it = range.begin(); res_it != range.end(); res_it++)
  {
    const ImageSubresourceState &resState = At(*res_it);

    if(resState.StartLayout() != VK_IMAGE_LAYOUT_UNDEFINED &&
       resState.StartLayout() != VK_IMAGE_LAYOUT_MAX_ENUM)
    {
      changes.layoutChanged = changes.layoutChanged || resState.StartLayout() != resState.Layout();

      if(firstLayoutRes)
      {
        changes.startLayout = resState.StartLayout();
        changes.endLayout = resState.Layout();
        firstLayoutRes = false;
      }
      else
      {
        changes.sameStartLayout =
            changes.sameStartLayout && changes.startLayout == resState.StartLayout();
        changes.sameEndLayout = changes.sameEndLayout && changes.endLayout == resState.Layout();
      }
    }
    if(resState.StartQueueFamily() != VK_QUEUE_FAMILY_IGNORED &&
       resState.SharingMode() != VK_SHARING_MODE_CONCURRENT)
    {
      changes.queueFamilyChanged =
          changes.queueFamilyChanged || (resState.StartQueueFamily() != resState.QueueFamily() &&
                                         resState.QueueFamily() != VK_QUEUE_FAMILY_IGNORED);
      if(firstQueueRes)
      {
        changes.startQueueFamily = resState.StartQueueFamily();
        changes.endQueueFamily = resState.QueueFamily();
        firstQueueRes = false;
      }
      else
      {
        changes.sameStartQueueFamily =
            changes.sameStartQueueFamily && changes.startQueueFamily == resState.StartQueueFamily();
        changes.sameEndQueueFamily =
            changes.sameEndQueueFamily && (changes.endQueueFamily == resState.QueueFamily() ||
                                           resState.QueueFamily() == VK_QUEUE_FAMILY_IGNORED);
      }
    }
  }
  return changes;
}

}    // namespace vk_cpp_codec