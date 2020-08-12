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

#include "vk_state.h"
#include "vk_common.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_info.h"
#include "vk_resources.h"

VulkanRenderState::VulkanRenderState()
{
  RDCEraseEl(ibuffer);
}

void VulkanRenderState::BeginRenderPassAndApplyState(WrappedVulkan *vk, VkCommandBuffer cmd,
                                                     PipelineBinding binding)
{
  RDCASSERT(renderPass != ResourceId());

  // clear values don't matter as we're using the load renderpass here, that
  // has all load ops set to load (as we're doing a partial replay - can't
  // just clear the targets that are partially written to).

  VkClearValue empty[16] = {};

  RDCASSERT(ARRAY_COUNT(empty) >=
            vk->GetDebugManager()->GetRenderPassInfo(renderPass).attachments.size());

  VulkanCreationInfo::Framebuffer fbinfo = vk->GetDebugManager()->GetFramebufferInfo(framebuffer);

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(vk->GetDebugManager()->GetRenderPassInfo(renderPass).loadRPs[subpass]),
      Unwrap(fbinfo.loadFBs[subpass]),
      renderArea,
      (uint32_t)vk->GetDebugManager()->GetRenderPassInfo(renderPass).attachments.size(),
      empty,
  };

  VkRenderPassAttachmentBeginInfo imagelessAttachments = {
      VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
  };
  rdcarray<VkImageView> imagelessViews;

  if(fbinfo.imageless)
  {
    rpbegin.pNext = &imagelessAttachments;
    imagelessAttachments.attachmentCount = (uint32_t)fbattachments.size();

    for(size_t i = 0; i < fbattachments.size(); i++)
      imagelessViews.push_back(
          Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkImageView>(fbattachments[i])));

    imagelessAttachments.pAttachments = imagelessViews.data();
  }

  ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

  BindPipeline(vk, cmd, binding, true);

  if(IsConditionalRenderingEnabled())
  {
    VkConditionalRenderingBeginInfoEXT beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    beginInfo.pNext = VK_NULL_HANDLE;
    beginInfo.buffer =
        Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(conditionalRendering.buffer));
    beginInfo.offset = conditionalRendering.offset;
    beginInfo.flags = conditionalRendering.flags;

    ObjDisp(cmd)->CmdBeginConditionalRenderingEXT(Unwrap(cmd), &beginInfo);
  }
}

void VulkanRenderState::EndRenderPass(VkCommandBuffer cmd)
{
  ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
}

void VulkanRenderState::EndTransformFeedback(WrappedVulkan *vk, VkCommandBuffer cmd)
{
  if(!xfbcounters.empty())
  {
    rdcarray<VkBuffer> buffers;
    rdcarray<VkDeviceSize> offsets;

    for(size_t i = 0; i < xfbcounters.size(); i++)
    {
      buffers.push_back(
          Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(xfbcounters[i].buf)));
      offsets.push_back(xfbcounters[i].offs);
    }

    ObjDisp(cmd)->CmdEndTransformFeedbackEXT(
        Unwrap(cmd), firstxfbcounter, (uint32_t)xfbcounters.size(), buffers.data(), offsets.data());
  }
}

void VulkanRenderState::EndConditionalRendering(VkCommandBuffer cmd)
{
  if(IsConditionalRenderingEnabled())
    ObjDisp(cmd)->CmdEndConditionalRenderingEXT(Unwrap(cmd));
}

bool VulkanRenderState::IsConditionalRenderingEnabled()
{
  return conditionalRendering.buffer != ResourceId() && !conditionalRendering.forceDisable;
}

void VulkanRenderState::BindPipeline(WrappedVulkan *vk, VkCommandBuffer cmd,
                                     PipelineBinding binding, bool subpass0)
{
  if(graphics.pipeline != ResourceId() && binding == BindGraphics)
  {
    VkPipeline pipe = vk->GetResourceManager()->GetCurrentHandle<VkPipeline>(graphics.pipeline);
    const VulkanCreationInfo::Pipeline pipeinfo =
        vk->GetDebugManager()->GetPipelineInfo(graphics.pipeline);

    if(subpass0 && pipeinfo.subpass0pipe != VK_NULL_HANDLE)
      pipe = pipeinfo.subpass0pipe;

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

    ResourceId pipeLayoutId = pipeinfo.layout;
    VkPipelineLayout layout =
        vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

    const rdcarray<VkPushConstantRange> &pushRanges =
        vk->GetDebugManager()->GetPipelineLayoutInfo(pipeLayoutId).pushRanges;

    const bool(&dynamicStates)[VkDynamicCount] = pipeinfo.dynamicStates;

    if(!views.empty() && dynamicStates[VkDynamicViewport])
      ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, (uint32_t)views.size(), &views[0]);
    if(!scissors.empty() && dynamicStates[VkDynamicScissor])
      ObjDisp(cmd)->CmdSetScissor(Unwrap(cmd), 0, (uint32_t)scissors.size(), &scissors[0]);

    if(dynamicStates[VkDynamicViewportCountEXT])
      ObjDisp(cmd)->CmdSetViewportWithCountEXT(Unwrap(cmd), (uint32_t)views.size(), views.data());
    if(dynamicStates[VkDynamicScissorCountEXT])
      ObjDisp(cmd)->CmdSetScissorWithCountEXT(Unwrap(cmd), (uint32_t)scissors.size(),
                                              scissors.data());

    if(dynamicStates[VkDynamicCullModeEXT])
      ObjDisp(cmd)->CmdSetCullModeEXT(Unwrap(cmd), cullMode);
    if(dynamicStates[VkDynamicFrontFaceEXT])
      ObjDisp(cmd)->CmdSetFrontFaceEXT(Unwrap(cmd), frontFace);
    if(dynamicStates[VkDynamicPrimitiveTopologyEXT])
      ObjDisp(cmd)->CmdSetPrimitiveTopologyEXT(Unwrap(cmd), primitiveTopology);

    if(dynamicStates[VkDynamicLineWidth])
      ObjDisp(cmd)->CmdSetLineWidth(Unwrap(cmd), lineWidth);

    if(dynamicStates[VkDynamicDepthBias])
      ObjDisp(cmd)->CmdSetDepthBias(Unwrap(cmd), bias.depth, bias.biasclamp, bias.slope);

    if(dynamicStates[VkDynamicBlendConstants])
      ObjDisp(cmd)->CmdSetBlendConstants(Unwrap(cmd), blendConst);

    if(dynamicStates[VkDynamicDepthBoundsTestEnableEXT])
      ObjDisp(cmd)->CmdSetDepthBoundsTestEnableEXT(Unwrap(cmd), depthBoundsTestEnable);
    if(dynamicStates[VkDynamicDepthBounds])
      ObjDisp(cmd)->CmdSetDepthBounds(Unwrap(cmd), mindepth, maxdepth);

    if(dynamicStates[VkDynamicDepthTestEnableEXT])
      ObjDisp(cmd)->CmdSetDepthTestEnableEXT(Unwrap(cmd), depthTestEnable);
    if(dynamicStates[VkDynamicDepthWriteEnableEXT])
      ObjDisp(cmd)->CmdSetDepthWriteEnableEXT(Unwrap(cmd), depthWriteEnable);
    if(dynamicStates[VkDynamicDepthCompareOpEXT])
      ObjDisp(cmd)->CmdSetDepthCompareOpEXT(Unwrap(cmd), depthCompareOp);

    if(dynamicStates[VkDynamicStencilTestEnableEXT])
      ObjDisp(cmd)->CmdSetStencilTestEnableEXT(Unwrap(cmd), stencilTestEnable);

    if(dynamicStates[VkDynamicStencilOpEXT])
    {
      ObjDisp(cmd)->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.failOp,
                                       front.passOp, front.depthFailOp, front.compareOp);
      ObjDisp(cmd)->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, front.failOp,
                                       front.passOp, front.depthFailOp, front.compareOp);
    }

    if(dynamicStates[VkDynamicStencilCompareMask])
    {
      ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.compare);
      ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.compare);
    }

    if(dynamicStates[VkDynamicStencilWriteMask])
    {
      ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.write);
      ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.write);
    }

    if(dynamicStates[VkDynamicStencilReference])
    {
      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.ref);
      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.ref);
    }

    if(dynamicStates[VkDynamicSampleLocationsEXT])
    {
      VkSampleLocationsInfoEXT info = {VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT};
      info.pSampleLocations = sampleLocations.locations.data();
      info.sampleLocationsCount = (uint32_t)sampleLocations.locations.size();
      info.sampleLocationsPerPixel = sampleLocations.sampleCount;
      info.sampleLocationGridSize = sampleLocations.gridSize;
      ObjDisp(cmd)->CmdSetSampleLocationsEXT(Unwrap(cmd), &info);
    }

    if(!discardRectangles.empty() && dynamicStates[VkDynamicDiscardRectangleEXT])
      ObjDisp(cmd)->CmdSetDiscardRectangleEXT(Unwrap(cmd), 0, (uint32_t)discardRectangles.size(),
                                              &discardRectangles[0]);

    if(dynamicStates[VkDynamicLineStippleEXT])
      ObjDisp(cmd)->CmdSetLineStippleEXT(Unwrap(cmd), stippleFactor, stipplePattern);

    // only set push constant ranges that the layout uses
    for(size_t i = 0; i < pushRanges.size(); i++)
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags,
                                     pushRanges[i].offset, pushRanges[i].size,
                                     pushconsts + pushRanges[i].offset);

    BindDescriptorSets(vk, cmd, graphics, VK_PIPELINE_BIND_POINT_GRAPHICS);

    if(ibuffer.buf != ResourceId())
    {
      VkIndexType type = VK_INDEX_TYPE_UINT16;
      if(ibuffer.bytewidth == 4)
        type = VK_INDEX_TYPE_UINT32;
      else if(ibuffer.bytewidth == 1)
        type = VK_INDEX_TYPE_UINT8_EXT;

      ObjDisp(cmd)->CmdBindIndexBuffer(
          Unwrap(cmd), Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(ibuffer.buf)),
          ibuffer.offs, type);
    }

    bool dynamicStride = dynamicStates[VkDynamicVertexInputBindingStrideEXT];

    for(size_t i = 0; i < vbuffers.size(); i++)
    {
      if(vbuffers[i].buf == ResourceId())
      {
        if(vk->NULLDescriptorsAllowed())
        {
          VkBuffer empty = VK_NULL_HANDLE;

          if(dynamicStride)
            ObjDisp(cmd)->CmdBindVertexBuffers2EXT(
                Unwrap(cmd), (uint32_t)i, 1, &empty, &vbuffers[i].offs,
                vbuffers[i].size == VK_WHOLE_SIZE ? NULL : &vbuffers[i].size, &vbuffers[i].stride);
          else
            ObjDisp(cmd)->CmdBindVertexBuffers(Unwrap(cmd), (uint32_t)i, 1, &empty,
                                               &vbuffers[i].offs);
        }

        continue;
      }

      if(dynamicStride)
        ObjDisp(cmd)->CmdBindVertexBuffers2EXT(
            Unwrap(cmd), (uint32_t)i, 1,
            UnwrapPtr(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(vbuffers[i].buf)),
            &vbuffers[i].offs, vbuffers[i].size == VK_WHOLE_SIZE ? NULL : &vbuffers[i].size,
            &vbuffers[i].stride);
      else
        ObjDisp(cmd)->CmdBindVertexBuffers(
            Unwrap(cmd), (uint32_t)i, 1,
            UnwrapPtr(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(vbuffers[i].buf)),
            &vbuffers[i].offs);
    }

    for(size_t i = 0; i < xfbbuffers.size(); i++)
    {
      if(xfbbuffers[i].buf == ResourceId())
        continue;

      ObjDisp(cmd)->CmdBindTransformFeedbackBuffersEXT(
          Unwrap(cmd), (uint32_t)i, 1,
          UnwrapPtr(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(xfbbuffers[i].buf)),
          &xfbbuffers[i].offs, &xfbbuffers[i].size);
    }

    if(!xfbcounters.empty())
    {
      rdcarray<VkBuffer> buffers;
      rdcarray<VkDeviceSize> offsets;

      for(size_t i = 0; i < xfbcounters.size(); i++)
      {
        buffers.push_back(
            Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(xfbcounters[i].buf)));
        offsets.push_back(xfbcounters[i].offs);
      }

      ObjDisp(cmd)->CmdBeginTransformFeedbackEXT(
          Unwrap(cmd), firstxfbcounter, (uint32_t)xfbcounters.size(), buffers.data(), offsets.data());
    }
  }

  if(compute.pipeline != ResourceId() && binding == BindCompute)
  {
    ObjDisp(cmd)->CmdBindPipeline(
        Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
        Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkPipeline>(compute.pipeline)));

    ResourceId pipeLayoutId = vk->GetDebugManager()->GetPipelineInfo(compute.pipeline).layout;
    VkPipelineLayout layout =
        vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

    const rdcarray<VkPushConstantRange> &pushRanges =
        vk->GetDebugManager()->GetPipelineLayoutInfo(pipeLayoutId).pushRanges;

    // only set push constant ranges that the layout uses
    for(size_t i = 0; i < pushRanges.size(); i++)
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), VK_SHADER_STAGE_COMPUTE_BIT,
                                     pushRanges[i].offset, pushRanges[i].size,
                                     pushconsts + pushRanges[i].offset);

    BindDescriptorSets(vk, cmd, compute, VK_PIPELINE_BIND_POINT_COMPUTE);
  }
}

void VulkanRenderState::BindDescriptorSets(WrappedVulkan *vk, VkCommandBuffer cmd,
                                           VulkanStatePipeline &pipe, VkPipelineBindPoint bindPoint)
{
  ResourceId pipeLayoutId = vk->GetDebugManager()->GetPipelineInfo(pipe.pipeline).layout;
  const rdcarray<ResourceId> &descSetLayouts =
      vk->GetDebugManager()->GetPipelineLayoutInfo(pipeLayoutId).descSetLayouts;

  for(size_t i = 0; i < descSetLayouts.size(); i++)
  {
    const DescSetLayout &descLayout = vk->GetDebugManager()->GetDescSetLayout(descSetLayouts[i]);

    if(i < pipe.descSets.size() && pipe.descSets[i].descSet != ResourceId())
    {
      // if we come to a descriptor set that isn't compatible, stop setting descriptor sets from
      // here on.
      // We can get into this situation if for example we have many sets bound at some point, then
      // there's a pipeline change that causes most or all of them to be invalidated as
      // incompatible, then the program only re-binds some subset that it knows is statically used
      // by the next drawcall. The remaining sets are invalid, but also unused and this is
      // explicitly allowed by the spec. We just have to make sure we don't try to actively bind
      // an incompatible descriptor set.
      ResourceId createdDescSetLayoutId = vk->GetDescLayoutForDescSet(pipe.descSets[i].descSet);

      if(descSetLayouts[i] != createdDescSetLayoutId)
      {
        const DescSetLayout &createdDescLayout =
            vk->GetDebugManager()->GetDescSetLayout(createdDescSetLayoutId);

        if(descLayout != createdDescLayout)
        {
          // this set is incompatible, don't rebind it. Assume the application knows the shader
          // doesn't need this set, and the binding is just stale
          continue;
        }
      }

      // if there are dynamic buffers, pass along the offsets

      uint32_t *dynamicOffsets = NULL;

      if(descLayout.dynamicCount > 0)
      {
        dynamicOffsets = &pipe.descSets[i].offsets[0];

        if(pipe.descSets[i].offsets.size() < descLayout.dynamicCount)
        {
          dynamicOffsets = new uint32_t[descLayout.dynamicCount];
          for(uint32_t o = 0; o < descLayout.dynamicCount; o++)
          {
            if(o < pipe.descSets[i].offsets.size())
            {
              dynamicOffsets[o] = pipe.descSets[i].offsets[o];
            }
            else
            {
              dynamicOffsets[o] = 0;
              RDCWARN("Missing dynamic offset for set %u!", (uint32_t)i);
            }
          }
        }
      }

      BindDescriptorSet(vk, descLayout, cmd, bindPoint, (uint32_t)i, dynamicOffsets);

      if(pipe.descSets[i].offsets.size() < descLayout.dynamicCount)
        SAFE_DELETE_ARRAY(dynamicOffsets);
    }
  }
}

void VulkanRenderState::BindDescriptorSet(WrappedVulkan *vk, const DescSetLayout &descLayout,
                                          VkCommandBuffer cmd, VkPipelineBindPoint bindPoint,
                                          uint32_t setIndex, uint32_t *dynamicOffsets)
{
  ResourceId descSet = (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
                           ? graphics.descSets[setIndex].descSet
                           : compute.descSets[setIndex].descSet;
  ResourceId pipeLayout = (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
                              ? graphics.descSets[setIndex].pipeLayout
                              : compute.descSets[setIndex].pipeLayout;
  VkPipelineLayout layout = vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayout);

  if((descLayout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0)
  {
    ObjDisp(cmd)->CmdBindDescriptorSets(
        Unwrap(cmd), bindPoint, Unwrap(layout), (uint32_t)setIndex, 1,
        UnwrapPtr(vk->GetResourceManager()->GetCurrentHandle<VkDescriptorSet>(descSet)),
        descLayout.dynamicCount, dynamicOffsets);
  }
  else
  {
    // this isn't a real descriptor set, it's a push descriptor, so we need to push the
    // current state.
    rdcarray<VkWriteDescriptorSet> writes;

    // any allocated arrays
    rdcarray<VkDescriptorImageInfo *> allocImgWrites;
    rdcarray<VkDescriptorBufferInfo *> allocBufWrites;
    rdcarray<VkBufferView *> allocBufViewWrites;
    rdcarray<VkWriteDescriptorSetInlineUniformBlockEXT *> allocInlineWrites;

    const WrappedVulkan::DescriptorSetInfo &setInfo = vk->GetDebugManager()->GetDescSetInfo(descSet);

    VkWriteDescriptorSet push = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    for(size_t b = 0; b < descLayout.bindings.size(); b++)
    {
      const DescSetLayout::Binding &bind = descLayout.bindings[b];

      // skip if this binding isn't used
      if(bind.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
        continue;

      // push.dstSet; // unused for push descriptors
      push.dstBinding = (uint32_t)b;
      push.dstArrayElement = 0;
      push.descriptorType = bind.descriptorType;
      push.descriptorCount = bind.descriptorCount;

      const DescriptorSetSlot *slots = setInfo.data.binds[b];

      if(push.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         push.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        VkBufferView *dst = new VkBufferView[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
          dst[a] = Unwrap(
              vk->GetResourceManager()->GetCurrentHandle<VkBufferView>(slots[a].texelBufferView));

        push.pTexelBufferView = dst;
        allocBufViewWrites.push_back(dst);
      }
      else if(push.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              push.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              push.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              push.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              push.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        VkDescriptorImageInfo *dst = new VkDescriptorImageInfo[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
        {
          const DescriptorSetSlotImageInfo &src = slots[a].imageInfo;

          dst[a].imageLayout = src.imageLayout;
          dst[a].sampler = Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkSampler>(src.sampler));
          dst[a].imageView =
              Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkImageView>(src.imageView));
        }

        push.pImageInfo = dst;
        allocImgWrites.push_back(dst);
      }
      else if(push.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
      {
        allocInlineWrites.push_back(new VkWriteDescriptorSetInlineUniformBlockEXT);
        VkWriteDescriptorSetInlineUniformBlockEXT *inlineWrite = allocInlineWrites.back();
        inlineWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
        inlineWrite->pNext = NULL;
        inlineWrite->dataSize = bind.descriptorCount;
        inlineWrite->pData = setInfo.data.inlineBytes.data() + slots[0].inlineOffset;

        push.pNext = inlineWrite;
        push.descriptorCount = bind.descriptorCount;
        writes.push_back(push);

        // skip validity checks
        continue;
      }
      else
      {
        VkDescriptorBufferInfo *dst = new VkDescriptorBufferInfo[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
        {
          const DescriptorSetSlotBufferInfo &src = slots[a].bufferInfo;

          dst[a].offset = src.offset;
          dst[a].range = src.range;
          dst[a].buffer = Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(src.buffer));
        }

        push.pBufferInfo = dst;
        allocBufWrites.push_back(dst);
      }

      // start with no descriptors
      push.descriptorCount = 0;

      for(uint32_t w = 0; w < bind.descriptorCount; w++)
      {
        // if this push is valid, we increment the descriptor count and continue
        if(IsValid(vk->NULLDescriptorsAllowed(), push, w - push.dstArrayElement))
        {
          push.descriptorCount++;
        }
        else
        {
          // if this push isn't valid, then we first check to see if we had any previous
          // pending pushs in the array we were going to batch together, if so we add them.
          if(push.descriptorCount > 0)
            writes.push_back(push);

          // skip past any previous descriptors we just wrote, as well as the current invalid
          // one
          if(push.pBufferInfo)
            push.pBufferInfo += push.descriptorCount + 1;
          if(push.pImageInfo)
            push.pImageInfo += push.descriptorCount + 1;
          if(push.pTexelBufferView)
            push.pTexelBufferView += push.descriptorCount + 1;

          // now start again from 0 descriptors, at the next array element
          push.dstArrayElement += push.descriptorCount + 1;
          push.descriptorCount = 0;
        }
      }

      // if there are any left, add them here
      if(push.descriptorCount > 0)
        writes.push_back(push);

      // don't leak the arrays and cause double deletes, NULL them after each time
      push.pImageInfo = NULL;
      push.pBufferInfo = NULL;
      push.pTexelBufferView = NULL;
    }

    ObjDisp(cmd)->CmdPushDescriptorSetKHR(Unwrap(cmd), bindPoint, Unwrap(layout), setIndex,
                                          (uint32_t)writes.size(), writes.data());

    // delete allocated arrays for descriptor writes
    for(VkDescriptorBufferInfo *a : allocBufWrites)
      delete[] a;
    for(VkDescriptorImageInfo *a : allocImgWrites)
      delete[] a;
    for(VkBufferView *a : allocBufViewWrites)
      delete[] a;
    for(VkWriteDescriptorSetInlineUniformBlockEXT *a : allocInlineWrites)
      delete a;
  }
}

void VulkanRenderState::SetFramebuffer(WrappedVulkan *vk, ResourceId fb,
                                       const VkRenderPassAttachmentBeginInfo *attachmentsInfo)
{
  framebuffer = fb;

  VulkanCreationInfo::Framebuffer fbinfo = vk->GetDebugManager()->GetFramebufferInfo(fb);

  fbattachments.resize(fbinfo.attachments.size());

  if(!fbinfo.imageless)
  {
    for(size_t i = 0; i < fbinfo.attachments.size(); i++)
      fbattachments[i] = fbinfo.attachments[i].createdView;
  }
  else
  {
    for(size_t i = 0; i < fbinfo.attachments.size(); i++)
      fbattachments[i] = GetResID(attachmentsInfo->pAttachments[i]);
  }
}
