/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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
#include "vk_cpp_codec_tracker.h"
#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
/**************************************************************************
* Helpers for the Cmd...Analyze() methods.
* These methods update state variables used to track reads/writes of memory
* and images.
***************************************************************************/

void TraceTracker::AccessBufferMemory(uint64_t buf_id, uint64_t offset, uint64_t size,
                                      AccessAction action)
{
  RDCASSERT(IsValidNonNullResouce(buf_id));
  ExtObject *memBinding = FindBufferMemBinding(buf_id);
  uint64_t mem_id = memBinding->At("memory")->U64();
  uint64_t mem_offset = memBinding->At("memoryOffset")->U64();
  MemAllocWithResourcesMapIter mem_it = MemAllocFind(mem_id);
  RDCASSERT(mem_it != MemAllocEnd());

  ResourceWithViewsMapIter buf_it = ResourceCreateFind(buf_id);
  RDCASSERT(buf_it != ResourceCreateEnd());
  ExtObject *ci = buf_it->second.sdobj->At("CreateInfo");
  VkSharingMode sharingMode = (VkSharingMode)ci->At("sharingMode")->U64();
  uint64_t buf_size = ci->At("size")->U64();

  if(size > buf_size - offset)
  {
    if(size != VK_WHOLE_SIZE)
    {
      RDCWARN("Buffer used in descriptor set update has size (%llu) but range listed is (%llu)",
              buf_size, size);
    }
    size = buf_size - offset;
  }

  mem_it->second.Access(cmdQueueFamily, sharingMode, action, offset + mem_offset, size);
}

void TraceTracker::TransitionBufferQueueFamily(uint64_t buf_id, uint64_t srcQueueFamily,
                                               uint64_t dstQueueFamily, uint64_t offset,
                                               uint64_t size)
{
  RDCASSERT(IsValidNonNullResouce(buf_id));
  ExtObject *memBinding = FindBufferMemBinding(buf_id);
  uint64_t mem_id = memBinding->At("memory")->U64();
  uint64_t mem_offset = memBinding->At("memoryOffset")->U64();
  MemAllocWithResourcesMapIter mem_it = MemAllocFind(mem_id);
  RDCASSERT(mem_it != MemAllocEnd());

  ResourceWithViewsMapIter buf_it = ResourceCreateFind(buf_id);
  RDCASSERT(buf_it != ResourceCreateEnd());
  ExtObject *ci = buf_it->second.sdobj->At("CreateInfo");
  VkSharingMode sharingMode = (VkSharingMode)ci->At("sharingMode")->U64();
  uint64_t buf_size = ci->At("size")->U64();

  if(size > buf_size - offset)
  {
    if(size != VK_WHOLE_SIZE)
    {
      RDCWARN("Buffer used in descriptor set update has size (%llu) but range listed is (%llu)",
              buf_size, size);
    }
    size = buf_size - offset;
  }

  mem_it->second.TransitionQueueFamily(cmdQueueFamily, sharingMode, srcQueueFamily, dstQueueFamily,
                                       mem_offset + offset, size);
}

void TraceTracker::ReadBoundVertexBuffers(uint64_t vertexCount, uint64_t instanceCount,
                                          uint64_t firstVertex, uint64_t firstInstance)
{
  ExtObjectIDMapIter pipeline_it = createdPipelines.find(bindingState.graphicsPipeline.pipeline);
  RDCASSERT(pipeline_it != createdPipelines.end());
  ExtObject *vertexInputState = pipeline_it->second->At(3)->At(5);
  ExtObject *boundVertexDescriptions = vertexInputState->At(4);
  for(uint64_t i = 0; i < boundVertexDescriptions->Size(); i++)
  {
    uint64_t bindingNum = boundVertexDescriptions->At(i)->At(0)->U64();
    uint64_t stride = boundVertexDescriptions->At(i)->At(1)->U64();
    uint64_t inputRate = boundVertexDescriptions->At(i)->At(2)->U64();
    uint64_t startVertex, numVertices;
    switch(inputRate)
    {
      case VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX:
        startVertex = firstVertex;
        numVertices = vertexCount;
        break;
      case VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE:
        startVertex = firstInstance;
        numVertices = instanceCount;
        break;
      default:
        RDCASSERT(0);
        startVertex = numVertices = 0;
        break;
    }
    std::map<uint64_t, BoundBuffer>::iterator boundBuffer_it =
        bindingState.vertexBuffers.find(bindingNum);
    if(boundBuffer_it != bindingState.vertexBuffers.end())
    {
      uint64_t offset = startVertex * stride;
      uint64_t size;
      if(numVertices == UINT64_MAX || (uint32_t)numVertices == UINT32_MAX)
      {
        RDCASSERT(boundBuffer_it->second.size >= offset);
        size = boundBuffer_it->second.size - offset;
      }
      else
      {
        size = numVertices * stride;
      }
      offset += boundBuffer_it->second.offset;
      AccessBufferMemory(boundBuffer_it->second.buffer, offset, size, ACCESS_ACTION_READ);
    }
  }
}

void TraceTracker::AccessMemoryInBoundDescriptorSets(BoundPipeline &boundPipeline)
{
  ExtObjectIDMapIter pipeline_it = createdPipelines.find(boundPipeline.pipeline);
  RDCASSERT(pipeline_it != createdPipelines.end());

  uint64_t pipelineLayout_id = 0;
  switch(pipeline_it->second->ChunkID())
  {
    case(uint32_t)VulkanChunk::vkCreateGraphicsPipelines:
      pipelineLayout_id = pipeline_it->second->At(3)->At(14)->U64();
      break;
    case(uint32_t)VulkanChunk::vkCreateComputePipelines:
      pipelineLayout_id = pipeline_it->second->At(3)->At(4)->U64();
      break;
    default: RDCASSERT(0);
  }
  ResourceWithViewsMapIter pipelineLayout_it = ResourceCreateFind(pipelineLayout_id);
  RDCASSERT(pipelineLayout_it != ResourceCreateEnd());
  ExtObject *pipelineLayout_ci = pipelineLayout_it->second.sdobj->At(1);

  uint64_t setLayoutCount = pipelineLayout_ci->At(3)->U64();
  ExtObject *setLayouts = pipelineLayout_ci->At(4);
  RDCASSERT(setLayoutCount == setLayouts->Size());

  for(uint64_t i = 0; i < setLayoutCount; i++)
  {
    U64MapIter descriptorSet_it = boundPipeline.descriptorSets.find(i);
    if(descriptorSet_it != boundPipeline.descriptorSets.end())
    {
      uint64_t descriptorSet = descriptorSet_it->second;
      uint64_t setLayout = setLayouts->At(i)->U64();
      AccessMemoryInDescriptorSet(descriptorSet, setLayout);
    }
  }
}

void TraceTracker::AccessMemoryInDescriptorSet(uint64_t descriptorSet_id, uint64_t setLayout_id)
{
  DescriptorSetInfoMapIter descriptorSet_it = descriptorSetInfos.find(descriptorSet_id);
  RDCASSERT(descriptorSet_it != descriptorSetInfos.end());
  DescriptorSetInfo &descriptorSet = descriptorSet_it->second;
  for(DescriptorBindingMapIter it = descriptorSet.bindings.begin();
      it != descriptorSet.bindings.end(); it++)
  {
    AccessAction action = ACCESS_ACTION_READ;
    DescriptorBinding &binding = it->second;
    switch(binding.type)
    {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
        // Only a sampler, no image to access
        break;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        action = ACCESS_ACTION_READ_WRITE;
      // Fall through; storage can also be read.
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        for(uint64_t i = 0; i < binding.imageBindings.size(); i++)
        {
          BoundImage imageBinding = binding.imageBindings[i];
          if(!imageBinding.bound)
          {
// TODO(bjoeris): This warning is extremely noisy for some traces.
// Figure out whether this is:
//  1. A code gen bug,
//  2. RenderDoc not serializing some the descriptor sets, or
//  3. Valid application behaviour. Does the validation layer allow this?
#if 0
              RDCWARN("Descriptor set %llu, binding %llu, index %d, is not bound to any image view.",
                descriptorSet_id, it->first, i);
#endif
            continue;
          }
          else if(!IsValidNonNullResouce(imageBinding.imageView))
          {
            RDCWARN("Descriptor set %llu, binding %llu, index %d, bound to invalid image view %llu",
                    descriptorSet_id, it->first, i, imageBinding.imageView);
            continue;
          }
          AccessImageView(imageBinding.imageView, imageBinding.imageLayout, action);
          // TODO: Is any layout analysis needed here?
        }
        break;

      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        action = ACCESS_ACTION_READ_WRITE;
      // Fall through; storage can also be read.
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        for(uint64_t i = 0; i < binding.bufferBindings.size(); i++)
        {
          BoundBuffer bufferBinding = binding.bufferBindings[i];
          if(!bufferBinding.bound)
          {
// TODO(bjoeris): This warning is extremely noisy for some traces.
// Figure out whether this is:
//  1. A code gen bug,
//  2. RenderDoc not serializing some the descriptor sets, or
//  3. Valid application behaviour. Does the validation layer allow this?
#if 0
              RDCWARN("Descriptor set %llu, binding %llu, index %d, is not bound to any buffer.",
                descriptorSet_id, it->first, i);
#endif
            continue;
          }
          else if(!IsValidNonNullResouce(bufferBinding.buffer))
          {
            RDCWARN("Descriptor set %llu, binding %llu, index %d, bound to invalid buffer %llu",
                    descriptorSet_id, it->first, i, bufferBinding.buffer);
            continue;
          }
          uint64_t offset = bufferBinding.offset + bufferBinding.dynamicOffset;
          AccessBufferMemory(bufferBinding.buffer, offset, bufferBinding.size, action);
        }
        break;

      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        action = ACCESS_ACTION_READ_WRITE;
      // Fall through; storage can also be read.
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        for(uint64_t i = 0; i < binding.texelViewBindings.size(); i++)
        {
          uint64_t view_id = binding.texelViewBindings[i].texelBufferView;
          if(view_id == 0)
          {
// TODO(bjoeris): This warning is extremely noisy for some traces.
// Figure out whether this is:
//  1. A code gen bug,
//  2. RenderDoc not serializing some the descriptor sets, or
//  3. Valid application behaviour. Does the validation layer allow this?
#if 0
              RDCWARN("Descriptor set %llu, binding %llu, index %d, not bound to any buffer view",
                descriptorSet_id, it->first, i);
#endif
            continue;
          }
          ResourceWithViewsMapIter view_it = ResourceCreateFind(view_id);
          // TODO(akharlamov) why is this checking ResourceCreateEnd and not
          // IsValidNonNullResouce?..
          if(view_it == ResourceCreateEnd())
          {
            RDCWARN(
                "Descriptor set %llu, binding %llu, index %d, bound to invalid buffer view %llu",
                descriptorSet_id, it->first, i, view_id);
            continue;
          }
          ExtObject *ci = view_it->second.sdobj->At(1);
          uint64_t buffer = ci->At(3)->U64();
          uint64_t offset = ci->At(5)->U64();
          uint64_t size = ci->At(6)->U64();
          if(!IsValidNonNullResouce(buffer))
          {
            RDCWARN(
                "Descriptor set %llu, binding %llu, index %d, bound to invalid buffer %llu via "
                "buffer view %llu",
                descriptorSet_id, it->first, i, buffer, view_id);
            continue;
          }
          AccessBufferMemory(buffer, offset, size, action);
        }
        break;
      default: break;
    }
  }
}

void TraceTracker::AccessImage(uint64_t image, VkImageAspectFlags aspectMask, uint64_t baseMipLevel,
                               uint64_t levelCount, uint64_t baseArrayLayer, uint64_t layerCount,
                               bool is2DView, VkImageLayout layout, AccessAction action)
{
  std::function<AccessState(AccessState)> transition = GetAccessStateTransition(action);
  ResourceWithViewsMapIter image_it = ResourceCreateFind(image);
  if(image_it == ResourceCreateEnd() && !IsPresentationResource(image))
  {
    RDCASSERT(0);    // TODO: should this ever happen?
    return;
  }
  ImageStateMapIter imageState_it = imageStates.find(image);
  RDCASSERT(imageState_it != imageStates.end());
  ImageState &imageState = imageState_it->second;

  ImageSubresourceRange range =
      imageState.Range(aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount, is2DView);
  for(ImageSubresourceRangeIter res_it = range.begin(); res_it != range.end(); res_it++)
  {
    ImageSubresourceState &resState = imageState.At(*res_it);
    resState.Access(cmdQueueFamily, layout, transition);
  }
}

void TraceTracker::AccessImage(uint64_t image, ExtObject *subresource, VkImageLayout layout,
                               AccessAction action)
{
  RDCASSERT(std::string(subresource->Type()) == "VkImageSubresourceRange");

  VkImageAspectFlags aspectMask = (VkImageAspectFlags)subresource->At(0)->U64();
  uint64_t baseMipLevel = subresource->At(1)->U64();
  uint64_t levelCount = subresource->At(2)->U64();
  uint64_t baseArrayLayer = subresource->At(3)->U64();
  uint64_t layerCount = subresource->At(4)->U64();

  AccessImage(image, aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount, false,
              layout, action);
}

bool isFullImage(ExtObject *imageExtent, ExtObject *offset, ExtObject *extent, uint64_t mipLevel)
{
  uint64_t offsetV[3]{0, 0, 0};
  uint64_t imageExtentV[3];
  uint64_t extentV[3];
  for(uint64_t i = 0; i < 3; i++)
  {
    uint64_t d = imageExtent->At(i)->U64();
    imageExtentV[i] = extentV[i] =
        (d + (1ull << mipLevel) - 1) >> mipLevel;    // ceil(d / (2^mipLevel))
  }
  if(offset != NULL)
  {
    RDCASSERT(std::string(offset->Type()).substr(0, 8) == "VkOffset");
    for(uint64_t i = 0; i < offset->Size(); i++)
    {
      offsetV[i] = offset->At(i)->U64();
    }
  }
  if(extent != NULL)
  {
    RDCASSERT(std::string(extent->Type()).substr(0, 8) == "VkExtent");
    for(uint64_t i = 0; i < extent->Size(); i++)
    {
      extentV[i] = extent->At(i)->U64();
    }
  }
  bool fullImage = true;
  for(uint64_t i = 0; i < 3; i++)
  {
    if(offsetV[i] != 0 || extentV[i] != imageExtentV[i])
    {
      fullImage = false;
      RDCASSERT(extentV[i] < imageExtentV[i]);    // TODO: are there magic values like
                                                  // VK_REMAINING_MIP_LEVELS that indicate "full
                                                  // dimension"?
    }
  }
  return fullImage;
}

void TraceTracker::AccessImage(uint64_t image, ExtObject *subresource, ExtObject *offset,
                               ExtObject *extent, VkImageLayout layout, AccessAction action)
{
  RDCASSERT(std::string(subresource->Type()) == "VkImageSubresourceLayers");
  VkImageAspectFlags aspectMask = (VkImageAspectFlags)subresource->At(0)->U64();
  uint64_t mipLevel = subresource->At(1)->U64();
  uint64_t baseArrayLayer = subresource->At(2)->U64();
  uint64_t layerCount = subresource->At(3)->U64();

  if(action == ACCESS_ACTION_CLEAR)
  {
    // The image subresource is being 'cleared', but we need to check whether
    // the whole image is cleared, or only part.

    ResourceWithViewsMapIter image_it = ResourceCreateFind(image);
    if(image_it == ResourceCreateEnd())
    {
      // TODO: this happens a lot. Is that expected?
      // RDCASSERT(0); // TODO: should this ever happen?
      return;
    }
    ExtObject *image_ci = image_it->second.sdobj->At(1);
    ExtObject *imageExtent = image_ci->At(5);

    // TODO(akharlamov, bjoeris) I think this should include aspect for depth stencil resources.
    if(!isFullImage(imageExtent, offset, extent, mipLevel))
    {
      // action is 'clear', but only part of the image is cleared, which is actually a 'write'.
      action = ACCESS_ACTION_WRITE;
    }
  }

  AccessImage(image, aspectMask, mipLevel, 1, baseArrayLayer, layerCount, false, layout, action);
}

void TraceTracker::AccessImageView(uint64_t view, VkImageLayout layout, AccessAction action,
                                   VkImageAspectFlags aspectMask, uint64_t baseArrayLayer,
                                   uint64_t layerCount)
{
  ResourceWithViewsMapIter view_it = ResourceCreateFind(view);
  ExtObjectIDMapIter present_it = presentResources.find(view);
  if(view_it == ResourceCreateEnd() || present_it != presentResources.end())
  {
    // This can happen for views of swapchain images
    return;
  }
  ExtObject *view_ci = view_it->second.sdobj->At(1);
  uint64_t image = view_ci->At(3)->U64();
  ExtObject *subresource = view_ci->At(7);

  VkImageViewType viewType = (VkImageViewType)view_ci->At(4)->U64();

  bool is2DView = (viewType == VK_IMAGE_VIEW_TYPE_2D) || (viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY);

  uint64_t viewAspectMask = subresource->At(0)->U64();
  uint64_t baseMipLevel = subresource->At(1)->U64();
  uint64_t levelCount = subresource->At(2)->U64();
  uint64_t viewBaseArrayLayer = subresource->At(3)->U64();
  uint64_t viewLayerCount = subresource->At(4)->U64();
  uint64_t lastArrayLayer, viewLastArrayLayer;

  if(layerCount == VK_REMAINING_ARRAY_LAYERS)
  {
    lastArrayLayer = VK_REMAINING_ARRAY_LAYERS;
  }
  else
  {
    lastArrayLayer = baseArrayLayer + layerCount;
  }
  if(viewLayerCount == VK_REMAINING_ARRAY_LAYERS)
  {
    viewLastArrayLayer = VK_REMAINING_ARRAY_LAYERS;
  }
  else
  {
    viewLastArrayLayer = viewBaseArrayLayer + viewLayerCount;
  }

  baseArrayLayer = std::max(baseArrayLayer, viewBaseArrayLayer);
  lastArrayLayer = std::min(lastArrayLayer, viewLastArrayLayer);
  layerCount = lastArrayLayer - baseArrayLayer;

  aspectMask &= viewAspectMask;

  AccessImage(image, aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount, is2DView,
              layout, action);
}

void TraceTracker::AccessAttachment(uint64_t attachment, AccessAction action,
                                    VkImageAspectFlags aspectMask, uint64_t baseArrayLayer,
                                    uint64_t layerCount)
{
  if(attachment == VK_ATTACHMENT_UNUSED)
  {
    return;
  }
  uint64_t view_id = bindingState.framebuffer->At(5)->At(attachment)->U64();
  VkImageLayout layout = bindingState.attachmentLayout[attachment];
  RDCASSERT(layout != VK_IMAGE_LAYOUT_MAX_ENUM);

  AccessImageView(view_id, layout, action, aspectMask, baseArrayLayer, layerCount);
}

void TraceTracker::TransitionImageLayout(uint64_t image, ExtObject *range, VkImageLayout oldLayout,
                                         VkImageLayout newLayout, uint64_t srcQueueFamily,
                                         uint64_t dstQueueFamily)
{
  VkImageAspectFlags aspectMask = (VkImageAspectFlags)range->At("aspectMask")->U64();
  uint64_t baseMip = range->At("baseMipLevel")->U64();
  uint64_t levelCount = range->At("levelCount")->U64();
  uint64_t baseLayer = range->At("baseArrayLayer")->U64();
  uint64_t layerCount = range->At("layerCount")->U64();

  ImageStateMapIter imageState_it = imageStates.find(image);
  RDCASSERT(imageState_it != imageStates.end());
  ImageState &imageState = imageState_it->second;

  ImageSubresourceRange imageRange =
      imageState.Range(aspectMask, baseMip, levelCount, baseLayer, layerCount);
  for(ImageSubresourceRangeIter res_it = imageRange.begin(); res_it != imageRange.end(); res_it++)
  {
    imageState.At(*res_it).Transition(cmdQueueFamily, oldLayout, newLayout, srcQueueFamily,
                                      dstQueueFamily);
  }
}

void TraceTracker::TransitionImageViewLayout(uint64_t viewID, VkImageLayout oldLayout,
                                             VkImageLayout newLayout, uint64_t srcQueueFamily,
                                             uint64_t dstQueueFamily)
{
  ResourceWithViewsMapIter view_it = ResourceCreateFind(viewID);
  RDCASSERT(view_it != ResourceCreateEnd());
  ExtObject *view = view_it->second.sdobj;          // get the vkCreateImageView call
  ExtObject *viewCI = view->At("CreateInfo");       // the VkImageViewCreateInfo
  uint64_t imageID = viewCI->At("image")->U64();    // get the image ID
                                                    // look for it in the createdResource map

  ExtObject *subresource = viewCI->At("subresourceRange");
  VkImageViewType viewType = (VkImageViewType)viewCI->At("viewType")->U64();
  bool is2DView = viewType == VK_IMAGE_VIEW_TYPE_2D || viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  VkImageAspectFlags aspectMask = (VkImageAspectFlags)subresource->At("aspectMask")->U64();
  uint64_t baseMip = subresource->At("baseMipLevel")->U64();
  uint64_t levelCount = subresource->At("levelCount")->U64();
  uint64_t baseLayer = subresource->At("baseArrayLayer")->U64();
  uint64_t layerCount = subresource->At("layerCount")->U64();

  ImageStateMapIter imageState_it = imageStates.find(imageID);
  RDCASSERT(imageState_it != imageStates.end());
  ImageState &imageState = imageState_it->second;

  ImageSubresourceRange range =
      imageState.Range(aspectMask, baseMip, levelCount, baseLayer, layerCount, is2DView);
  for(ImageSubresourceRangeIter res_it = range.begin(); res_it != range.end(); res_it++)
  {
    imageState.At(*res_it).Transition(cmdQueueFamily, oldLayout, newLayout, srcQueueFamily,
                                      dstQueueFamily);
  }
}

void TraceTracker::TransitionAttachmentLayout(uint64_t attachment, VkImageLayout layout)
{
  if(attachment == VK_ATTACHMENT_UNUSED)
  {
    return;
  }
  RDCASSERT(layout != VK_IMAGE_LAYOUT_UNDEFINED && layout != VK_IMAGE_LAYOUT_PREINITIALIZED);
  uint64_t viewID = bindingState.framebuffer->At("pAttachments")->At(attachment)->U64();

  VkImageLayout oldLayout = bindingState.attachmentLayout[attachment];
  RDCASSERT(oldLayout != VK_IMAGE_LAYOUT_MAX_ENUM);

  TransitionImageViewLayout(viewID, oldLayout, layout, VK_QUEUE_FAMILY_IGNORED,
                            VK_QUEUE_FAMILY_IGNORED);
}

void TraceTracker::LoadSubpassAttachment(ExtObject *attachmentRef)
{
  uint64_t attachment = attachmentRef->At("attachment")->U64();
  VkImageLayout layout = (VkImageLayout)attachmentRef->At("layout")->U64();

  if(attachment == VK_ATTACHMENT_UNUSED)
  {
    return;
  }
  ExtObject *att_desc = bindingState.renderPass->At(4)->At(attachment);
  uint64_t view_id = bindingState.framebuffer->At(5)->At(attachment)->U64();

  if(bindingState.subpassIndex == bindingState.attachmentFirstUse[attachment])
  {
    VkFormat format = (VkFormat)att_desc->At("format")->U64();
    VkImageLayout initialLayout = (VkImageLayout)att_desc->At("initialLayout")->U64();

    VkAttachmentLoadOp loadOp;
    // If the format is Depth AND Stencil, both OPs need to be taken into account.
    // If neither Op is LOAD then we can pretend loadOp is VK_ATTACHMENT_LOAD_OP_DONT_CARE
    if(IsDepthAndStencilFormat(format))
    {
      loadOp = (VkAttachmentLoadOp)att_desc->At("loadOp")->U64();
      VkAttachmentLoadOp stencilLoadOp = (VkAttachmentLoadOp)att_desc->At("stencilLoadOp")->U64();
      if(loadOp != VK_ATTACHMENT_LOAD_OP_LOAD && stencilLoadOp != VK_ATTACHMENT_LOAD_OP_LOAD)
        loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      else
        loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }
    else
    {
      loadOp = (VkAttachmentLoadOp)att_desc->At("loadOp")->U64();
    }

    if(loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
    {
      AccessAction action;
      if(bindingState.isFullRenderArea)
      {
        action = ACCESS_ACTION_CLEAR;
      }
      else
      {
        action = ACCESS_ACTION_WRITE;
      }
      AccessImageView(view_id, initialLayout, action);
    }
    else
    {
      AccessImageView(view_id, initialLayout, ACCESS_ACTION_READ);
    }
  }

  TransitionAttachmentLayout(attachment, layout);
}

void TraceTracker::BeginSubpass()
{
  ExtObject *subpasses = bindingState.renderPass->At("pSubpasses");
  RDCASSERT(bindingState.subpassIndex < subpasses->Size());
  ExtObject *subpass = subpasses->At(bindingState.subpassIndex);
  ExtObject *inputAttachments = subpass->At("pInputAttachments");
  ExtObject *colorAttachments = subpass->At("pColorAttachments");
  ExtObject *resolveAttachments = subpass->At("pResolveAttachments");
  ExtObject *depthStencilAttachment = subpass->At("pDepthStencilAttachment");

  for(uint64_t i = 0; i < inputAttachments->Size(); i++)
  {
    ExtObject *attachmentRef = inputAttachments->At(i);
    uint64_t a = attachmentRef->At("attachment")->U64();
    LoadSubpassAttachment(attachmentRef);
    AccessAttachment(a, ACCESS_ACTION_READ_WRITE);
  }
  for(uint32_t i = 0; i < colorAttachments->Size(); i++)
  {
    LoadSubpassAttachment(colorAttachments->At(i));
  }
  for(uint32_t i = 0; i < resolveAttachments->Size(); i++)
  {
    LoadSubpassAttachment(resolveAttachments->At(i));
  }
  if(!depthStencilAttachment->IsNULL())
  {
    LoadSubpassAttachment(depthStencilAttachment);
    uint64_t a = depthStencilAttachment->At("attachment")->U64();
    AccessAttachment(a, ACCESS_ACTION_READ_WRITE);
  }
  bindingState.graphicsPipeline.subpassHasDraw = false;
}

void TraceTracker::EndSubpass()
{
  if(!bindingState.graphicsPipeline.subpassHasDraw)
  {
    // No draws
    return;
  }
  ExtObject *subpasses = bindingState.renderPass->At(6);

  RDCASSERT(bindingState.subpassIndex < subpasses->Size());
  ExtObject *subpass = subpasses->At(bindingState.subpassIndex);
  ExtObject *colorAttachments = subpass->At(5);
  ExtObject *resolveAttachments = subpass->At(6);
  ExtObject *depthStencilAttachment = subpass->At(7);

  ExtObjectIDMapIter pipeline_it = createdPipelines.find(bindingState.graphicsPipeline.pipeline);
  if(pipeline_it == createdPipelines.end())
  {
    return;
  }
  ExtObject *pipeline_ci = pipeline_it->second->At(3);
  ExtObject *blendState = pipeline_ci->At(12);
  ExtObject *blendAttachmets = blendState->At(6);

  for(uint64_t i = 0; i < colorAttachments->Size(); i++)
  {
    uint64_t blendEnabled = blendAttachmets->At(i)->At(0)->U64();
    // "blendEnable controls whether blending is enabled for the corresponding color attachment. If
    // blending is not enabled, the source fragment’s color for that attachment is passed through
    // unmodified."
    if(blendEnabled)
    {
      // TODO: depending on the blending settings, this may be just a write, rather than read/write.
      AccessAttachment(colorAttachments->At(i)->At(0)->U64(), ACCESS_ACTION_READ_WRITE);
    }
  }
  for(uint64_t i = 0; i < resolveAttachments->Size(); i++)
  {
    AccessAttachment(resolveAttachments->At(i)->At(0)->U64(), ACCESS_ACTION_WRITE);
  }
  if(!depthStencilAttachment->IsNULL())
  {
    AccessAttachment(depthStencilAttachment->At(0)->U64(), ACCESS_ACTION_WRITE);
  }
}

ExtObject *TraceTracker::FindBufferMemBinding(uint64_t buf_id)
{
  ResourceWithViewsMapIter bufCreate_it = ResourceCreateFind(buf_id);
  RDCASSERT(bufCreate_it != ResourceCreateEnd());
  ExtObject *result = 0;
  for(ExtObjectIDMapIter v_it = bufCreate_it->second.views.begin();
      v_it != bufCreate_it->second.views.end(); v_it++)
  {
    if(v_it->second->name == std::string("vkBindBufferMemory"))
    {
      RDCASSERT(result == 0);    // We should only find one memory binding for the buffer
      result = v_it->second;
    }
  }
  RDCASSERT(result != 0);
  return result;
}

void TraceTracker::BufferImageCopyHelper(uint64_t buf_id, uint64_t img_id, ExtObject *regions,
                                         VkImageLayout imageLayout, AccessAction bufferAction,
                                         AccessAction imageAction)
{
  ResourceWithViewsMapIter imgCreate_it = ResourceCreateFind(img_id);

  RDCASSERT(imgCreate_it != ResourceCreateEnd());
  ExtObject *image_ci = imgCreate_it->second.sdobj->At(1);
  VkFormat imageFormat = (VkFormat)image_ci->At(4)->U64();

  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    ExtObject *region = regions->At(i);
    ExtObject *imageSubresource = region->At(3);
    uint64_t aspectMask = imageSubresource->At(0)->U64();

    uint64_t layerCount = imageSubresource->At(3)->U64();
    ExtObject *regionOffset = region->At(4);
    ExtObject *regionExtent = region->At(5);
    uint32_t regionWidth = as_uint32(regionExtent->At(0)->U64());
    uint32_t regionHeight = as_uint32(regionExtent->At(1)->U64());
    uint32_t regionDepth = as_uint32(regionExtent->At(2)->U64());
    uint64_t bufferOffset = region->At(0)->U64();

    AccessImage(img_id, imageSubresource, regionOffset, regionExtent, imageLayout, imageAction);

    uint32_t rowLength = as_uint32(region->At(1)->U64());
    if(rowLength == 0)
      rowLength = regionWidth;

    uint32_t imageHeight = as_uint32(region->At(2)->U64());
    if(imageHeight == 0)
      imageHeight = regionHeight;
    VkFormat regionFormat = imageFormat;
    switch(aspectMask)
    {
      case VK_IMAGE_ASPECT_STENCIL_BIT: regionFormat = VK_FORMAT_S8_UINT; break;
      case VK_IMAGE_ASPECT_DEPTH_BIT:
        switch(imageFormat)
        {
          case VK_FORMAT_D16_UNORM_S8_UINT: regionFormat = VK_FORMAT_D16_UNORM; break;
          case VK_FORMAT_D32_SFLOAT_S8_UINT: regionFormat = VK_FORMAT_D32_SFLOAT; break;
          default: break;
        }
        break;
      default: break;
    }

    // rowSize = # bytes accessed per row
    uint64_t rowSize = GetByteSize(regionWidth, 1, 1, regionFormat, 0);
    // stride_y = # bytes between subsequent rows
    uint64_t stride_y = GetByteSize(rowLength, 1, 1, regionFormat, 0);
    // stride_z = # bytes between subsequent depths
    uint64_t stride_z = GetByteSize(rowLength, imageHeight, 1, regionFormat, 0);
    // stride_layer = # bytes between subsequent layers
    uint64_t stride_layer = GetByteSize(rowLength, imageHeight, regionDepth, regionFormat, 0);
    // numRows = # rows of texels accessed
    uint64_t numRows = GetByteSize(rowLength, regionHeight, 1, regionFormat, 0) / stride_y;

    // Loop over all layers, depths, and rows, marking the region of memory for that row as read or
    // written
    for(uint64_t lr = 0; lr < layerCount; lr++)
    {
      for(uint64_t z = 0; z < regionDepth; z++)
      {
        for(uint64_t y = 0; y < numRows; y++)
        {
          uint64_t rowStart = bufferOffset + lr * stride_layer + z * stride_z + y * stride_y;
          AccessBufferMemory(buf_id, rowStart, rowSize, bufferAction);
        }
      }
    }
  }
}
}    // namespace vk_cpp_codec
