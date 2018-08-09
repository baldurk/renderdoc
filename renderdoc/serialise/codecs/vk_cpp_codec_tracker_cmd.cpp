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
* Cmd*Analyze() methods.
* These methods are called by AnalyzeCmds() for each vkCmd* call in the
* capture. These track things like reads/writes of memory and images.
***************************************************************************/

void TraceTracker::CmdBeginRenderPassAnalyze(ExtObject *o)
{
  ExtObject *bi = o->At("RenderPassBegin");
  uint64_t renderPass = bi->At("renderPass")->U64();
  uint64_t framebuffer = bi->At("framebuffer")->U64();
  ExtObject *renderPassCI = ResourceCreateFind(renderPass)->second.sdobj->At(1);
  ExtObject *framebufferCI = ResourceCreateFind(framebuffer)->second.sdobj->At(1);

  // Add the cmdBuffer ID and current ExtObject to beginRenderPassCmdBuffer map
  // to allow fetching it later on vkEndCmdRenderpass.
  uint64_t cmdBuffer = o->At("commandBuffer")->U64();
  RDCASSERT(beginRenderPassCmdBuffer.find(cmdBuffer) == beginRenderPassCmdBuffer.end());
  beginRenderPassCmdBuffer[cmdBuffer] = o;

  // Renderpass attachment list should be the same as framebuffers list
  RDCASSERT(renderPassCI->At("pAttachments")->Size() == framebufferCI->At("pAttachments")->Size());

  bindingState.BeginRenderPass(renderPassCI, framebufferCI, bi->At(4));
  BeginSubpass();
}

void TraceTracker::CmdNextSubpassAnalyze(ExtObject *o)
{
  EndSubpass();
  bindingState.subpassIndex++;
  BeginSubpass();
}

void TraceTracker::CmdEndRenderPassAnalyze(ExtObject *end)
{
  EndSubpass();
  uint64_t commandBuffer = end->At("commandBuffer")->U64();
  RDCASSERT(beginRenderPassCmdBuffer.find(commandBuffer) != beginRenderPassCmdBuffer.end());
  ExtObject *cmdBeginRenderPass = beginRenderPassCmdBuffer[commandBuffer];
  ExtObject *renderPassBegin = cmdBeginRenderPass->At("RenderPassBegin");
  uint64_t renderPassID = renderPassBegin->At("renderPass")->U64();
  uint64_t framebufferID = renderPassBegin->At("framebuffer")->U64();
  ResourceWithViewsMapIter rp_it = ResourceCreateFind(renderPassID);
  ResourceWithViewsMapIter fb_it = ResourceCreateFind(framebufferID);
  RDCASSERT(rp_it != ResourceCreateEnd() && fb_it != ResourceCreateEnd());
  ExtObject *renderPassCI = rp_it->second.sdobj->At("CreateInfo");
  ExtObject *framebufferCI = fb_it->second.sdobj->At("CreateInfo");
  ExtObject *renderPassAttachments = renderPassCI->At("pAttachments");
  ExtObject *framebufferAttachments = framebufferCI->At("pAttachments");
  RDCASSERT(renderPassAttachments->Size() == framebufferAttachments->Size());
  for(uint32_t a = 0; a < framebufferAttachments->Size(); a++)
  {
    uint64_t viewID = framebufferAttachments->At(a)->U64();
    ExtObject *attachmentDesc = renderPassAttachments->At(a);
    VkImageLayout finalLayout = (VkImageLayout)attachmentDesc->At("finalLayout")->U64();

    TransitionImageViewLayout(viewID, bindingState.attachmentLayout[a], finalLayout,
                              VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
  }

  beginRenderPassCmdBuffer.erase(commandBuffer);
  RDCASSERT(bindingState.subpassIndex == bindingState.renderPass->At(6)->Size() - 1);
}

void TraceTracker::CmdExecuteCommandsAnalyze(ExtObject *o)
{
  ExtObject *commandBuffers = o->At(2);
  for(uint64_t j = 0; j < commandBuffers->Size(); j++)
  {
    uint32_t recordIndex = fg.FindCmdBufferIndex(commandBuffers->At(j));
    CmdBufferRecord &record = fg.records[recordIndex];
    for(uint64_t k = 0; k < record.cmds.size(); k++)
    {
      AnalyzeCmd(record.cmds[k]);
    }
  }
}

void TraceTracker::CmdBindPipelineAnalyze(ExtObject *o)
{
  uint64_t pipelineBindPoint = o->At(1)->U64();
  uint64_t pipeline = o->At(2)->U64();
  RDCASSERT(createdPipelines.find(pipeline) != createdPipelines.end());
  switch(pipelineBindPoint)
  {
    case VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE:
      bindingState.computePipeline.pipeline = pipeline;
      break;
    case VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS:
      bindingState.graphicsPipeline.pipeline = pipeline;
      break;
    default: RDCASSERT(0);
  }
}

void TraceTracker::CmdBindDescriptorSetsAnalyze(ExtObject *o)
{
  // TODO(akharlamov) image can be bound to pipeline
  uint64_t pipelineBindPoint = o->At(1)->U64();
  uint64_t firstSet = o->At(3)->U64();
  uint64_t descriptorSetCount = o->At(4)->U64();
  ExtObject *descriptorSets = o->At(5);
  uint64_t dynamicOffsetCount = o->At(6)->U64();
  ExtObject *dynamicOffsets = o->At(7);

  RDCASSERT(descriptorSetCount == descriptorSets->Size());
  RDCASSERT(dynamicOffsetCount == dynamicOffsets->Size());

  BoundPipeline *boundPipeline = NULL;
  switch(pipelineBindPoint)
  {
    case VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE:
      boundPipeline = &bindingState.computePipeline;
      break;
    case VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS:
      boundPipeline = &bindingState.graphicsPipeline;
      break;
    default: RDCASSERT(0); break;
  }

  uint64_t dynamicOffsetIndex = 0;
  for(uint64_t i = 0; i < descriptorSetCount; i++)
  {
    uint64_t descSet_id = descriptorSets->At(i)->U64();
    uint64_t descSet_num = i + firstSet;
    boundPipeline->descriptorSets[descSet_num] = descSet_id;

    DescriptorSetInfoMapIter descSet_it = descriptorSetInfos.find(descSet_id);
    RDCASSERT(descSet_it != descriptorSetInfos.end());

    for(DescriptorBindingMapIter binding_it = descSet_it->second.bindings.begin();
        binding_it != descSet_it->second.bindings.end(); binding_it++)
    {
      switch(binding_it->second.type)
      {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
          for(uint64_t j = 0; j < binding_it->second.bufferBindings.size(); j++)
          {
            RDCASSERT(dynamicOffsetIndex < dynamicOffsets->Size());
            binding_it->second.bufferBindings[j].dynamicOffset =
                dynamicOffsets->At(dynamicOffsetIndex)->U64();
            dynamicOffsetIndex++;
          }
        default: break;
      }
    }
  }
}

void TraceTracker::CmdBindIndexBufferAnalyze(ExtObject *o)
{
  uint64_t buf_id = o->At(1)->U64();
  uint64_t offset = o->At(2)->U64();
  uint64_t indexType = o->At(3)->U64();
  ResourceWithViewsMapIter bufCreate_it = ResourceCreateFind(buf_id);
  RDCASSERT(bufCreate_it != ResourceCreateEnd());
  ExtObject *ci = bufCreate_it->second.sdobj->At(1);
  uint64_t bufSize = ci->At(3)->U64();
  bindingState.indexBuffer = BoundBuffer(buf_id, offset, bufSize - offset, 0);
  bindingState.indexBufferType = indexType;
}

void TraceTracker::CmdBindVertexBuffersAnalyze(ExtObject *o)
{
  uint64_t firstBinding = o->At(1)->U64();
  uint64_t bindingCount = o->At(2)->U64();
  ExtObject *buffers = o->At(3);
  ExtObject *offsets = o->At(4);
  RDCASSERT(bindingCount == buffers->Size());
  RDCASSERT(bindingCount == offsets->Size());
  for(uint64_t i = 0; i < bindingCount; i++)
  {
    uint64_t buf_id = buffers->At(i)->U64();
    uint64_t offset = offsets->At(i)->U64();
    ResourceWithViewsMapIter bufCreate_it = ResourceCreateFind(buf_id);
    RDCASSERT(bufCreate_it != ResourceCreateEnd());
    ExtObject *ci = bufCreate_it->second.sdobj->At(1);
    uint64_t bufSize = ci->At(3)->U64();
    bindingState.vertexBuffers[firstBinding + i] = BoundBuffer(buf_id, offset, bufSize - offset, 0);
  }
}

void TraceTracker::CmdCopyBufferToImageAnalyze(ExtObject *o)
{
  ExtObject *src = o->At(1);
  uint64_t src_id = src->U64();
  ExtObject *dst = o->At(2);
  VkImageLayout dst_layout = (VkImageLayout)o->At(3)->U64();
  uint64_t dst_id = dst->U64();
  ExtObject *regions = o->At(5);
  RDCASSERT(o->At(4)->U64() == regions->Size());

  BufferImageCopyHelper(src_id, dst_id, regions, dst_layout, ACCESS_ACTION_READ, ACCESS_ACTION_CLEAR);
}

void TraceTracker::CmdCopyImageToBufferAnalyze(ExtObject *o)
{
  ExtObject *src = o->At(1);
  VkImageLayout src_layout = (VkImageLayout)o->At(2)->U64();
  uint64_t src_id = src->U64();
  ExtObject *dst = o->At(3);
  uint64_t dst_id = dst->U64();
  ExtObject *regions = o->At(5);
  RDCASSERT(o->At(4)->U64() == regions->Size());

  BufferImageCopyHelper(dst_id, src_id, regions, src_layout, ACCESS_ACTION_CLEAR, ACCESS_ACTION_READ);
}

void TraceTracker::CmdCopyImageAnalyze(ExtObject *o)
{
  ExtObject *src = o->At(1);
  VkImageLayout src_layout = (VkImageLayout)o->At(2)->U64();
  uint64_t src_id = src->U64();
  ExtObject *dst = o->At(3);
  VkImageLayout dst_layout = (VkImageLayout)o->At(4)->U64();
  uint64_t dst_id = dst->U64();

  ExtObject *regions = o->At(6);
  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    ExtObject *region = regions->At(i);
    ExtObject *srcSubresource = region->At(0);
    ExtObject *srcOffset = region->At(1);
    ExtObject *dstSubresource = region->At(2);
    ExtObject *dstOffset = region->At(3);
    ExtObject *extent = region->At(4);

    AccessImage(src_id, srcSubresource, srcOffset, extent, src_layout, ACCESS_ACTION_READ);
    AccessImage(dst_id, dstSubresource, dstOffset, extent, dst_layout, ACCESS_ACTION_CLEAR);
  }
}

void TraceTracker::CmdBlitImageAnalyze(ExtObject *o)
{
  ExtObject *src = o->At(1);
  VkImageLayout src_layout = (VkImageLayout)o->At(2)->U64();
  uint64_t src_id = src->U64();
  ExtObject *dst = o->At(3);
  VkImageLayout dst_layout = (VkImageLayout)o->At(4)->U64();
  uint64_t dst_id = dst->U64();

  ExtObject *regions = o->At(6);
  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    ExtObject *region = regions->At(i);
    ExtObject *srcSubresource = region->At(0);
    ExtObject *srcOffsets = region->At(1);
    ExtObject *dstSubresource = region->At(2);
    ExtObject *dstOffsets = region->At(3);

    // Convert the two srcOffsets to a srcOffset and srcExtent,
    // and similarly for dst.
    ExtObject srcOffset("srcOffset", "VkOffset3D");
    ExtObject dstOffset("dstOffset", "VkOffset3D");
    ExtObject srcExtent("srcExtent", "VkExtent3D");
    ExtObject dstExtent("dstExtent", "VkExtent3D");
    const char *offset_names[3] = {"x", "y", "z"};
    const char *extent_names[3] = {"width", "height", "depth"};
    for(uint32_t j = 0; j < 3; j++)
    {
      uint64_t src_0 = srcOffsets->At(0)->At(j)->U64();
      uint64_t src_1 = srcOffsets->At(1)->At(j)->U64();
      uint64_t dst_0 = dstOffsets->At(0)->At(j)->U64();
      uint64_t dst_1 = dstOffsets->At(1)->At(j)->U64();
      srcOffset.AddChild(new ExtObject(offset_names[j], "int32_t", std::min(src_0, src_1)));
      dstOffset.AddChild(new ExtObject(offset_names[j], "int32_t", std::min(dst_0, dst_1)));
      srcExtent.AddChild(new ExtObject(extent_names[j], "int32_t",
                                       std::max(src_0, src_1) - std::min(src_0, src_1)));
      dstExtent.AddChild(new ExtObject(extent_names[j], "int32_t",
                                       std::max(dst_0, dst_1) - std::min(dst_0, dst_1)));
    }

    AccessImage(src_id, srcSubresource, &srcOffset, &srcExtent, src_layout, ACCESS_ACTION_READ);
    AccessImage(dst_id, dstSubresource, &dstOffset, &dstExtent, dst_layout, ACCESS_ACTION_CLEAR);
  }
}

void TraceTracker::CmdResolveImageAnalyze(ExtObject *o)
{
  ExtObject *src = o->At(1);
  VkImageLayout src_layout = (VkImageLayout)o->At(2)->U64();
  uint64_t src_id = src->U64();
  ExtObject *dst = o->At(3);
  VkImageLayout dst_layout = (VkImageLayout)o->At(4)->U64();
  uint64_t dst_id = dst->U64();

  ExtObject *regions = o->At(6);
  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    ExtObject *region = regions->At(i);
    ExtObject *srcSubresource = region->At(0);
    ExtObject *srcOffset = region->At(1);
    ExtObject *dstSubresource = region->At(2);
    ExtObject *dstOffset = region->At(3);
    ExtObject *extent = region->At(4);

    AccessImage(src_id, srcSubresource, srcOffset, extent, src_layout, ACCESS_ACTION_READ);
    AccessImage(dst_id, dstSubresource, dstOffset, extent, dst_layout, ACCESS_ACTION_CLEAR);
  }
}

void TraceTracker::CmdCopyBufferAnalyze(ExtObject *o)
{
  uint64_t src_id = o->At(1)->U64();
  uint64_t dst_id = o->At(2)->U64();
  ExtObject *regions = o->At(4);
  RDCASSERT(regions->Size() == o->At(3)->U64());

  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    ExtObject *region = regions->At(i);
    uint64_t srcOffset = region->At(0)->U64();
    uint64_t dstOffset = region->At(1)->U64();
    uint64_t size = region->At(2)->U64();
    AccessBufferMemory(src_id, srcOffset, size, ACCESS_ACTION_READ);
    AccessBufferMemory(dst_id, dstOffset, size, ACCESS_ACTION_CLEAR);
  }
}

void TraceTracker::CmdUpdateBufferAnalyze(ExtObject *o)
{
  uint64_t dst_id = o->At(1)->U64();
  uint64_t offset = o->At(2)->U64();
  uint64_t size = o->At(3)->U64();
  AccessBufferMemory(dst_id, offset, size, ACCESS_ACTION_CLEAR);
}

void TraceTracker::CmdFillBufferAnalyze(ExtObject *o)
{
  uint64_t dst_id = o->At(1)->U64();
  uint64_t offset = o->At(2)->U64();
  uint64_t size = o->At(3)->U64();
  AccessBufferMemory(dst_id, offset, size, ACCESS_ACTION_CLEAR);
}

void TraceTracker::CmdClearColorImageAnalyze(ExtObject *o)
{
  ExtObject *image = o->At(1);
  VkImageLayout image_layout = (VkImageLayout)o->At(2)->U64();
  uint64_t image_id = image->U64();

  ExtObject *regions = o->At(5);
  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    AccessImage(image_id, regions->At(i), image_layout, ACCESS_ACTION_CLEAR);
  }
}

void TraceTracker::CmdClearDepthStencilImageAnalyze(ExtObject *o)
{
  ExtObject *image = o->At(1);
  VkImageLayout image_layout = (VkImageLayout)o->At(2)->U64();
  uint64_t image_id = image->U64();

  ExtObject *regions = o->At(5);
  for(uint64_t i = 0; i < regions->Size(); i++)
  {
    AccessImage(image_id, regions->At(i), image_layout, ACCESS_ACTION_CLEAR);
  }
}

void TraceTracker::CmdClearAttachmentsAnalyze(ExtObject *o)
{
  ExtObject *subpasses = bindingState.renderPass->At(6);
  uint64_t fbWidth = bindingState.framebuffer->At(6)->U64();
  uint64_t fbHeight = bindingState.framebuffer->At(7)->U64();

  // TODO: multiview changes layers
  uint64_t fbLayers = bindingState.framebuffer->At(8)->U64();

  RDCASSERT(bindingState.subpassIndex < subpasses->Size());
  ExtObject *subpass = subpasses->At(bindingState.subpassIndex);
  ExtObject *colorAttachments = subpass->At(5);
  ExtObject *depthStencilAttachment = subpass->At(7);

  ExtObject *attachments = o->At(2);
  ExtObject *rects = o->At(4);

  std::vector<AccessAction> layerActions(fbLayers, ACCESS_ACTION_NONE);

  for(uint32_t i = 0; i < rects->Size(); i++)
  {
    ExtObject *clearRect = rects->At(i);
    ExtObject *rect2D = clearRect->At(0);
    ExtObject *offset = rect2D->At(0);
    uint64_t offset_x = offset->At(0)->U64();
    uint64_t offset_y = offset->At(1)->U64();
    ExtObject *extent = rect2D->At(1);
    uint64_t width = extent->At(0)->U64();
    uint64_t height = extent->At(1)->U64();
    uint64_t baseArrayLayer = clearRect->At(1)->U64();
    uint64_t layerCount = clearRect->At(2)->U64();
    RDCASSERT(layerCount < VK_REMAINING_ARRAY_LAYERS);

    bool fullFrame = (offset_x == 0 && offset_y == 0 && width == fbWidth && height == fbHeight);
    for(uint32_t j = 0; j < layerCount; j++)
    {
      uint64_t layer = baseArrayLayer + j;
      if(fullFrame)
      {
        layerActions[layer] = ACCESS_ACTION_CLEAR;
      }
      else
      {
        layerActions[layer] = std::max(layerActions[layer], ACCESS_ACTION_WRITE);
      }
    }
  }

  for(uint32_t i = 0; i < attachments->Size(); i++)
  {
    ExtObject *attachment = attachments->At(i);
    VkImageAspectFlags aspectMask = static_cast<VkImageAspectFlags>(attachment->At(0)->U64());
    uint64_t colorAttachment = attachment->At(1)->U64();
    for(uint32_t layer = 0; layer < fbLayers; layer++)
    {
      AccessAction action = layerActions[layer];
      if(action == ACCESS_ACTION_NONE)
      {
        continue;
      }

      if(aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
      {
        AccessAttachment(colorAttachments->At(colorAttachment)->At(0)->U64(), action, aspectMask,
                         layer, 1);
      }

      if(aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      {
        AccessAttachment(depthStencilAttachment->At(0)->U64(), action, aspectMask, layer, 1);
      }
    }
  }
}

void TraceTracker::CmdPipelineBarrierAnalyze(ExtObject *o)
{
  ExtObject *bufBarriers = o->At("pBufferMemoryBarriers");
  for(uint32_t i = 0; i < bufBarriers->Size(); i++)
  {
    ExtObject *barrier = bufBarriers->At(i);
    uint64_t bufID = barrier->At("buffer")->U64();
    uint64_t offset = barrier->At("offset")->U64();
    uint64_t size = barrier->At("size")->U64();

    uint64_t srcQueueFamilyIndex = barrier->At("srcQueueFamilyIndex")->U64();
    uint64_t dstQueueFamilyIndex = barrier->At("dstQueueFamilyIndex")->U64();

    TransitionBufferQueueFamily(bufID, srcQueueFamilyIndex, dstQueueFamilyIndex, offset, size);
  }
  ExtObject *imgBarriers = o->At("pImageMemoryBarriers");
  for(uint32_t i = 0; i < imgBarriers->Size(); i++)
  {
    ExtObject *barrier = imgBarriers->At(i);
    uint64_t imageID = barrier->At("image")->U64();

    // look for imageID in the createdResource map
    ImageStateMapIter image_it = ImageStateFind(imageID);

    // This resource wasn't properly created, skip.
    if(image_it == ImageStateEnd())
      continue;

    ExtObject *range = barrier->At("subresourceRange");
    VkImageLayout oldLayout = (VkImageLayout)barrier->At("oldLayout")->U64();
    VkImageLayout newLayout = (VkImageLayout)barrier->At("newLayout")->U64();
    uint64_t srcQueueFamilyIndex = barrier->At("srcQueueFamilyIndex")->U64();
    uint64_t dstQueueFamilyIndex = barrier->At("dstQueueFamilyIndex")->U64();

    TransitionImageLayout(imageID, range, oldLayout, newLayout, srcQueueFamilyIndex,
                          dstQueueFamilyIndex);
  }
}

void TraceTracker::CmdWaitEventsAnalyze(ExtObject *o)
{
}

void TraceTracker::CmdDispatchAnalyze(ExtObject *o)
{
  // Pessimistically read/write all memory and images accessible through bound descriptor sets
  AccessMemoryInBoundDescriptorSets(bindingState.computePipeline);
}

void TraceTracker::CmdDispatchIndirectAnalyze(ExtObject *o)
{
  uint64_t buf_id = o->At(1)->U64();
  uint64_t offset = o->At(2)->U64();
  uint64_t size = 3 * sizeof(uint32_t);
  AccessBufferMemory(buf_id, offset, size, ACCESS_ACTION_READ);

  // Pessimistically read/write all memory and images accessible through bound descriptor sets
  AccessMemoryInBoundDescriptorSets(bindingState.computePipeline);
}

void TraceTracker::CmdDrawIndirectAnalyze(ExtObject *o)
{
  uint64_t buf_id = o->At(1)->U64();
  uint64_t offset = o->At(2)->U64();
  uint64_t drawCount = o->At(3)->U64();
  uint64_t stride = o->At(3)->U64();
  uint64_t drawSize = 4 * sizeof(uint32_t);
  if(stride == drawSize)
  {
    AccessBufferMemory(buf_id, offset, drawSize * drawCount, ACCESS_ACTION_READ);
  }
  else
  {
    for(uint64_t i = 0; i < drawCount; i++)
    {
      AccessBufferMemory(buf_id, offset + i * stride, drawSize, ACCESS_ACTION_READ);
    }
  }

  // Pessimistically read all bound vertices
  ReadBoundVertexBuffers(UINT64_MAX, UINT64_MAX, 0, 0);

  // Pessimistically read/write all memory and images accessible through bound descriptor sets
  AccessMemoryInBoundDescriptorSets(bindingState.graphicsPipeline);

  bindingState.graphicsPipeline.subpassHasDraw = true;
}

void TraceTracker::CmdDrawIndexedIndirectAnalyze(ExtObject *o)
{
  uint64_t buf_id = o->At(1)->U64();
  uint64_t offset = o->At(2)->U64();
  uint64_t drawCount = o->At(3)->U64();
  uint64_t stride = o->At(3)->U64();
  uint64_t drawSize = 5 * sizeof(uint32_t);

  // Read indirect buffer
  if(stride == drawSize)
  {
    AccessBufferMemory(buf_id, offset, drawSize * drawCount, ACCESS_ACTION_READ);
  }
  else
  {
    for(uint64_t i = 0; i < drawCount; i++)
    {
      AccessBufferMemory(buf_id, offset + i * stride, drawSize, ACCESS_ACTION_READ);
    }
  }

  // Pessimistically read entire index buffer (we can't know at code gen time which parts of the
  // index buffer are actually read).
  AccessBufferMemory(bindingState.indexBuffer.buffer, bindingState.indexBuffer.offset,
                     bindingState.indexBuffer.size, ACCESS_ACTION_READ);

  // Pessimistically read all bound vertices
  ReadBoundVertexBuffers(UINT64_MAX, UINT64_MAX, 0, 0);

  // Pessimistically read/write all memory and images accessible through bound descriptor sets
  AccessMemoryInBoundDescriptorSets(bindingState.graphicsPipeline);

  bindingState.graphicsPipeline.subpassHasDraw = true;
}

void TraceTracker::CmdDrawAnalyze(ExtObject *o)
{
  uint64_t vertexCount = o->At(1)->U64();
  uint64_t instanceCount = o->At(2)->U64();
  uint64_t firstVertex = o->At(3)->U64();
  uint64_t firstInstance = o->At(4)->U64();
  ReadBoundVertexBuffers(vertexCount, instanceCount, firstVertex, firstInstance);

  // Pessimistically read/write all memory and images accessible through bound descriptor sets
  AccessMemoryInBoundDescriptorSets(bindingState.graphicsPipeline);

  bindingState.graphicsPipeline.subpassHasDraw = true;
}

void TraceTracker::CmdDrawIndexedAnalyze(ExtObject *o)
{
  uint64_t indexCount = o->At(1)->U64();
  uint64_t instanceCount = o->At(2)->U64();
  uint64_t firstIndex = o->At(3)->U64();
  uint64_t firstInstance = o->At(5)->U64();
  uint64_t indexElemSize;
  switch(bindingState.indexBufferType)
  {
    case VkIndexType::VK_INDEX_TYPE_UINT16: indexElemSize = sizeof(uint16_t); break;
    case VkIndexType::VK_INDEX_TYPE_UINT32: indexElemSize = sizeof(uint32_t); break;
    default:
      RDCASSERT(0);
      indexElemSize = 1;
      break;
  }
  uint64_t indexSize = indexCount * indexElemSize;
  uint64_t indexOffset = bindingState.indexBuffer.offset + indexElemSize * firstIndex;
  AccessBufferMemory(bindingState.indexBuffer.buffer, indexOffset, indexSize, ACCESS_ACTION_READ);
  ReadBoundVertexBuffers(UINT64_MAX, instanceCount, 0, firstInstance);

  // Pessimistically read/write all memory and images accessible through bound descriptor sets
  AccessMemoryInBoundDescriptorSets(bindingState.graphicsPipeline);

  bindingState.graphicsPipeline.subpassHasDraw = true;
}
}    // namespace vk_cpp_codec
