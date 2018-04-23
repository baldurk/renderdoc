/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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
#include "vk_core.h"
#include "vk_info.h"
#include "vk_resources.h"

VulkanRenderState::VulkanRenderState(WrappedVulkan *driver, VulkanCreationInfo *createInfo)
    : m_CreationInfo(createInfo), m_pDriver(driver)
{
  compute.pipeline = graphics.pipeline = renderPass = framebuffer = ResourceId();
  compute.descSets.clear();
  graphics.descSets.clear();

  views.clear();
  scissors.clear();
  lineWidth = 1.0f;
  RDCEraseEl(bias);
  RDCEraseEl(blendConst);
  mindepth = 0.0f;
  maxdepth = 1.0f;
  RDCEraseEl(front);
  RDCEraseEl(back);
  RDCEraseEl(pushconsts);

  renderPass = ResourceId();
  subpass = 0;

  RDCEraseEl(renderArea);

  RDCEraseEl(ibuffer);
  vbuffers.clear();
}

VulkanRenderState &VulkanRenderState::operator=(const VulkanRenderState &o)
{
  views = o.views;
  scissors = o.scissors;
  lineWidth = o.lineWidth;
  bias = o.bias;
  memcpy(blendConst, o.blendConst, sizeof(blendConst));
  mindepth = o.mindepth;
  maxdepth = o.maxdepth;
  front = o.front;
  back = o.back;
  memcpy(pushconsts, o.pushconsts, sizeof(pushconsts));
  renderPass = o.renderPass;
  subpass = o.subpass;
  framebuffer = o.framebuffer;
  renderArea = o.renderArea;

  compute.pipeline = o.compute.pipeline;
  compute.descSets = o.compute.descSets;

  graphics.pipeline = o.graphics.pipeline;
  graphics.descSets = o.graphics.descSets;

  ibuffer = o.ibuffer;
  vbuffers = o.vbuffers;

  return *this;
}

void VulkanRenderState::BeginRenderPassAndApplyState(VkCommandBuffer cmd, PipelineBinding binding)
{
  RDCASSERT(renderPass != ResourceId());

  // clear values don't matter as we're using the load renderpass here, that
  // has all load ops set to load (as we're doing a partial replay - can't
  // just clear the targets that are partially written to).

  VkClearValue empty[16] = {};

  RDCASSERT(ARRAY_COUNT(empty) >= m_CreationInfo->m_RenderPass[renderPass].attachments.size());

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(m_CreationInfo->m_RenderPass[renderPass].loadRPs[subpass]),
      Unwrap(m_CreationInfo->m_Framebuffer[framebuffer].loadFBs[subpass]),
      renderArea,
      (uint32_t)m_CreationInfo->m_RenderPass[renderPass].attachments.size(),
      empty,
  };
  ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

  BindPipeline(cmd, binding, true);

  if(ibuffer.buf != ResourceId())
    ObjDisp(cmd)->CmdBindIndexBuffer(
        Unwrap(cmd), Unwrap(GetResourceManager()->GetCurrentHandle<VkBuffer>(ibuffer.buf)),
        ibuffer.offs, ibuffer.bytewidth == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);

  for(size_t i = 0; i < vbuffers.size(); i++)
    ObjDisp(cmd)->CmdBindVertexBuffers(
        Unwrap(cmd), (uint32_t)i, 1,
        UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkBuffer>(vbuffers[i].buf)),
        &vbuffers[i].offs);
}

void VulkanRenderState::BindPipeline(VkCommandBuffer cmd, PipelineBinding binding, bool subpass0)
{
  if(graphics.pipeline != ResourceId() && binding == BindGraphics)
  {
    VkPipeline pipe = GetResourceManager()->GetCurrentHandle<VkPipeline>(graphics.pipeline);

    if(subpass0 && m_CreationInfo->m_Pipeline[graphics.pipeline].subpass0pipe != VK_NULL_HANDLE)
      pipe = m_CreationInfo->m_Pipeline[graphics.pipeline].subpass0pipe;

    ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

    ResourceId pipeLayoutId = m_CreationInfo->m_Pipeline[graphics.pipeline].layout;
    VkPipelineLayout layout = GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

    const vector<VkPushConstantRange> &pushRanges =
        m_CreationInfo->m_PipelineLayout[pipeLayoutId].pushRanges;

    bool dynamicStates[VK_DYNAMIC_STATE_RANGE_SIZE] = {0};
    memcpy(dynamicStates, m_CreationInfo->m_Pipeline[graphics.pipeline].dynamicStates,
           sizeof(dynamicStates));

    RDCCOMPILE_ASSERT(sizeof(dynamicStates) ==
                          sizeof(m_CreationInfo->m_Pipeline[graphics.pipeline].dynamicStates),
                      "Dynamic states array size is out of sync");

    if(!views.empty() && dynamicStates[VK_DYNAMIC_STATE_VIEWPORT])
      ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, (uint32_t)views.size(), &views[0]);
    if(!scissors.empty() && dynamicStates[VK_DYNAMIC_STATE_SCISSOR])
      ObjDisp(cmd)->CmdSetScissor(Unwrap(cmd), 0, (uint32_t)scissors.size(), &scissors[0]);

    if(dynamicStates[VK_DYNAMIC_STATE_LINE_WIDTH])
      ObjDisp(cmd)->CmdSetLineWidth(Unwrap(cmd), lineWidth);

    if(dynamicStates[VK_DYNAMIC_STATE_DEPTH_BIAS])
      ObjDisp(cmd)->CmdSetDepthBias(Unwrap(cmd), bias.depth, bias.biasclamp, bias.slope);

    if(dynamicStates[VK_DYNAMIC_STATE_BLEND_CONSTANTS])
      ObjDisp(cmd)->CmdSetBlendConstants(Unwrap(cmd), blendConst);

    if(dynamicStates[VK_DYNAMIC_STATE_DEPTH_BOUNDS])
      ObjDisp(cmd)->CmdSetDepthBounds(Unwrap(cmd), mindepth, maxdepth);

    if(dynamicStates[VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK])
    {
      ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.compare);
      ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.compare);
    }

    if(dynamicStates[VK_DYNAMIC_STATE_STENCIL_WRITE_MASK])
    {
      ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.write);
      ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.write);
    }

    if(dynamicStates[VK_DYNAMIC_STATE_STENCIL_REFERENCE])
    {
      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.ref);
      ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.ref);
    }

    // only set push constant ranges that the layout uses
    for(size_t i = 0; i < pushRanges.size(); i++)
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags,
                                     pushRanges[i].offset, pushRanges[i].size,
                                     pushconsts + pushRanges[i].offset);

    const vector<ResourceId> &descSetLayouts =
        m_CreationInfo->m_PipelineLayout[pipeLayoutId].descSetLayouts;

    // only iterate over the desc sets that this layout actually uses, not all that were bound
    for(size_t i = 0; i < descSetLayouts.size(); i++)
    {
      const DescSetLayout &descLayout = m_CreationInfo->m_DescSetLayout[descSetLayouts[i]];

      if(i < graphics.descSets.size() && graphics.descSets[i].descSet != ResourceId())
      {
        // if we come to a descriptor set that isn't compatible, stop setting descriptor sets from
        // here on.
        // We can get into this situation if for example we have many sets bound at some point, then
        // there's a pipeline change that causes most or all of them to be invalidated as
        // incompatible, then the program only re-binds some subset that it knows is statically used
        // by the next drawcall. The remaining sets are invalid, but also unused and this is
        // explicitly allowed by the spec. We just have to make sure we don't try to actively bind
        // an incompatible descriptor set.
        ResourceId createdDescSetLayoutId =
            m_pDriver->GetDescLayoutForDescSet(graphics.descSets[i].descSet);

        if(descSetLayouts[i] != createdDescSetLayoutId)
        {
          const DescSetLayout &createdDescLayout =
              m_CreationInfo->m_DescSetLayout[createdDescSetLayoutId];

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
          dynamicOffsets = &graphics.descSets[i].offsets[0];

          if(graphics.descSets[i].offsets.size() < descLayout.dynamicCount)
          {
            dynamicOffsets = new uint32_t[descLayout.dynamicCount];
            for(uint32_t o = 0; o < descLayout.dynamicCount; o++)
            {
              if(o < graphics.descSets[i].offsets.size())
              {
                dynamicOffsets[o] = graphics.descSets[i].offsets[o];
              }
              else
              {
                dynamicOffsets[o] = 0;
                RDCWARN("Missing dynamic offset for set %u!", (uint32_t)i);
              }
            }
          }
        }

        BindDescriptorSet(descLayout, cmd, layout, VK_PIPELINE_BIND_POINT_GRAPHICS, (uint32_t)i,
                          dynamicOffsets);

        if(graphics.descSets[i].offsets.size() < descLayout.dynamicCount)
          SAFE_DELETE_ARRAY(dynamicOffsets);
      }
      else
      {
        RDCWARN("Descriptor set is not bound but pipeline layout expects one");
      }
    }
  }

  if(compute.pipeline != ResourceId() && binding == BindCompute)
  {
    ObjDisp(cmd)->CmdBindPipeline(
        Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
        Unwrap(GetResourceManager()->GetCurrentHandle<VkPipeline>(compute.pipeline)));

    ResourceId pipeLayoutId = m_CreationInfo->m_Pipeline[compute.pipeline].layout;
    VkPipelineLayout layout = GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

    const vector<VkPushConstantRange> &pushRanges =
        m_CreationInfo->m_PipelineLayout[pipeLayoutId].pushRanges;

    // only set push constant ranges that the layout uses
    for(size_t i = 0; i < pushRanges.size(); i++)
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), VK_SHADER_STAGE_COMPUTE_BIT,
                                     pushRanges[i].offset, pushRanges[i].size,
                                     pushconsts + pushRanges[i].offset);

    const vector<ResourceId> &descSetLayouts =
        m_CreationInfo->m_PipelineLayout[pipeLayoutId].descSetLayouts;

    for(size_t i = 0; i < descSetLayouts.size(); i++)
    {
      const DescSetLayout &descLayout = m_CreationInfo->m_DescSetLayout[descSetLayouts[i]];

      if(i < compute.descSets.size() && compute.descSets[i].descSet != ResourceId())
      {
        // if there are dynamic buffers, pass along the offsets

        uint32_t *dynamicOffsets = NULL;

        if(descLayout.dynamicCount > 0)
        {
          dynamicOffsets = &compute.descSets[i].offsets[0];

          if(compute.descSets[i].offsets.size() < descLayout.dynamicCount)
          {
            dynamicOffsets = new uint32_t[descLayout.dynamicCount];
            for(uint32_t o = 0; o < descLayout.dynamicCount; o++)
            {
              if(o < compute.descSets[i].offsets.size())
              {
                dynamicOffsets[o] = compute.descSets[i].offsets[o];
              }
              else
              {
                dynamicOffsets[o] = 0;
                RDCWARN("Missing dynamic offset for set %u!", (uint32_t)i);
              }
            }
          }
        }

        BindDescriptorSet(descLayout, cmd, layout, VK_PIPELINE_BIND_POINT_COMPUTE, (uint32_t)i,
                          dynamicOffsets);

        if(compute.descSets[i].offsets.size() < descLayout.dynamicCount)
          SAFE_DELETE_ARRAY(dynamicOffsets);
      }
    }
  }
}

void VulkanRenderState::BindDescriptorSet(const DescSetLayout &descLayout, VkCommandBuffer cmd,
                                          VkPipelineLayout layout, VkPipelineBindPoint bindPoint,
                                          uint32_t setIndex, uint32_t *dynamicOffsets)
{
  ResourceId descSet = (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
                           ? graphics.descSets[setIndex].descSet
                           : compute.descSets[setIndex].descSet;

  if((descLayout.flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0)
  {
    ObjDisp(cmd)->CmdBindDescriptorSets(
        Unwrap(cmd), bindPoint, Unwrap(layout), (uint32_t)setIndex, 1,
        UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkDescriptorSet>(descSet)),
        descLayout.dynamicCount, dynamicOffsets);
  }
  else
  {
    // this isn't a real descriptor set, it's a push descriptor, so we need to push the
    // current state.
    std::vector<VkWriteDescriptorSet> writes;

    WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[descSet];

    for(size_t b = 0; b < descLayout.bindings.size(); b++)
    {
      VkWriteDescriptorSet push = {};

      // skip if this binding isn't used
      if(descLayout.bindings[b].descriptorCount == 0)
        continue;

      push.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      // push.dstSet; // unused for push descriptors
      push.dstBinding = (uint32_t)b;
      push.dstArrayElement = 0;
      push.descriptorType = descLayout.bindings[b].descriptorType;
      push.descriptorCount = descLayout.bindings[b].descriptorCount;

      DescriptorSetSlot *slots = setInfo.currentBindings[b];

      if(push.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         push.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        VkBufferView *dst = new VkBufferView[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
          dst[a] = Unwrap(slots[a].texelBufferView);

        push.pTexelBufferView = dst;
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
          dst[a] = slots[a].imageInfo;
          dst[a].sampler = Unwrap(dst[a].sampler);
          dst[a].imageView = Unwrap(dst[a].imageView);
        }

        push.pImageInfo = dst;
      }
      else
      {
        VkDescriptorBufferInfo *dst = new VkDescriptorBufferInfo[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
        {
          dst[a] = slots[a].bufferInfo;
          dst[a].buffer = Unwrap(dst[a].buffer);
        }

        push.pBufferInfo = dst;
      }

      writes.push_back(push);
    }

    ObjDisp(cmd)->CmdPushDescriptorSetKHR(Unwrap(cmd), bindPoint, Unwrap(layout), setIndex,
                                          (uint32_t)writes.size(), writes.data());

    // delete dynamically allocated arrays
    for(VkWriteDescriptorSet &push : writes)
    {
      SAFE_DELETE_ARRAY(push.pImageInfo);
      SAFE_DELETE_ARRAY(push.pBufferInfo);
      SAFE_DELETE_ARRAY(push.pTexelBufferView);
    }
  }
}

void VulkanRenderState::EndRenderPass(VkCommandBuffer cmd)
{
  ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
}

VulkanResourceManager *VulkanRenderState::GetResourceManager()
{
  return m_pDriver->GetResourceManager();
}
