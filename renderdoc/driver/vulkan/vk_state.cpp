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

#include "vk_state.h"
#include "vk_common.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_info.h"
#include "vk_resources.h"

namespace
{
struct RenderingInfoStructs
{
  VkRenderingInfo info;

  rdcarray<VkRenderingAttachmentInfo> color;
  VkRenderingAttachmentInfo depth;
  VkRenderingAttachmentInfo stencil;

  VkRenderingFragmentDensityMapAttachmentInfoEXT fragmentDensity;
  VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRate;
  VkMultisampledRenderToSingleSampledInfoEXT tileOnlyMSAA;
};

void setupRenderingInfo(const VulkanRenderState::DynamicRendering &dynamicRendering,
                        RenderingInfoStructs *structs, VkRenderingFlags flags,
                        const VkRect2D &renderArea)
{
  VkRenderingInfo *info = &structs->info;

  *info = {};
  info->sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  info->flags = flags;
  info->layerCount = dynamicRendering.layerCount;
  info->renderArea = renderArea;
  info->viewMask = dynamicRendering.viewMask;

  structs->depth = dynamicRendering.depth;
  info->pDepthAttachment = &structs->depth;
  if(structs->depth.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    info->pDepthAttachment = NULL;
  structs->stencil = dynamicRendering.stencil;
  info->pStencilAttachment = &structs->stencil;
  if(structs->stencil.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    info->pStencilAttachment = NULL;

  structs->color = dynamicRendering.color;

  info->colorAttachmentCount = (uint32_t)structs->color.size();
  info->pColorAttachments = structs->color.data();

  // patch the load/store actions and unwrap
  for(uint32_t i = 0; i < (uint32_t)structs->color.size() + 2; i++)
  {
    VkRenderingAttachmentInfo *att = (VkRenderingAttachmentInfo *)info->pColorAttachments + i;

    if(i == info->colorAttachmentCount)
      att = (VkRenderingAttachmentInfo *)info->pDepthAttachment;
    else if(i == info->colorAttachmentCount + 1)
      att = (VkRenderingAttachmentInfo *)info->pStencilAttachment;

    if(!att)
      continue;

    if(att->loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
      att->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    if(att->storeOp != VK_ATTACHMENT_STORE_OP_NONE)
      att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    att->imageView = Unwrap(att->imageView);
    att->resolveImageView = Unwrap(att->resolveImageView);
  }

  structs->fragmentDensity = {
      VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,
      NULL,
      Unwrap(dynamicRendering.fragmentDensityView),
      dynamicRendering.fragmentDensityLayout,
  };

  if(dynamicRendering.fragmentDensityView != VK_NULL_HANDLE)
  {
    structs->fragmentDensity.pNext = info->pNext;
    info->pNext = &structs->fragmentDensity;
  }

  structs->shadingRate = {
      VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,
      NULL,
      Unwrap(dynamicRendering.shadingRateView),
      dynamicRendering.shadingRateLayout,
      dynamicRendering.shadingRateTexelSize,
  };

  if(dynamicRendering.shadingRateView != VK_NULL_HANDLE)
  {
    structs->shadingRate.pNext = info->pNext;
    info->pNext = &structs->shadingRate;
  }

  structs->tileOnlyMSAA = {
      VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT,
      NULL,
      dynamicRendering.tileOnlyMSAAEnable,
      dynamicRendering.tileOnlyMSAASampleCount,
  };

  if(dynamicRendering.tileOnlyMSAAEnable)
  {
    structs->tileOnlyMSAA.pNext = info->pNext;
    info->pNext = &structs->tileOnlyMSAA;
  }
}
}    // namespace

VulkanRenderState::VulkanRenderState()
{
  RDCEraseEl(ibuffer);
}

void VulkanRenderState::BeginRenderPassAndApplyState(WrappedVulkan *vk, VkCommandBuffer cmd,
                                                     PipelineBinding binding, bool obeySuspending)
{
  if(dynamicRendering.active)
  {
    // for action callbacks that want to stop the renderpass, do something, then start it with
    // original state, we need to preserve the suspending flag instead of removing it. For other
    // uses, we remove both flags as we're just doing a manual start/stop and we're not in a
    // suspended pass
    VkRenderingFlags flags = dynamicRendering.flags;
    if(obeySuspending)
    {
      flags &= ~VK_RENDERING_RESUMING_BIT;
    }
    else
    {
      flags &= ~(VK_RENDERING_RESUMING_BIT | VK_RENDERING_SUSPENDING_BIT);
    }

    RenderingInfoStructs structs;
    setupRenderingInfo(dynamicRendering, &structs, flags, renderArea);

    ObjDisp(cmd)->CmdBeginRendering(Unwrap(cmd), &structs.info);
  }
  else
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

    ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, subpassContents);
  }

  if(subpassContents != VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS)
  {
    if(graphics.shaderObject)
      BindShaderObjects(vk, cmd, binding);
    else
      BindPipeline(vk, cmd, binding, true);
  }

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
  if(dynamicRendering.active)
  {
    if(!dynamicRendering.suspended)
      ObjDisp(cmd)->CmdEndRendering(Unwrap(cmd));
  }
  else
  {
    ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
  }
}

void VulkanRenderState::FinishSuspendedRenderPass(VkCommandBuffer cmd)
{
  if(dynamicRendering.active && dynamicRendering.suspended)
  {
    // still resume the existing pass, but don't suspend again after that
    const VkRenderingFlags flags = dynamicRendering.flags & ~VK_RENDERING_SUSPENDING_BIT;

    RenderingInfoStructs structs;
    setupRenderingInfo(dynamicRendering, &structs, flags, renderArea);

    // do nothing, just resume and then end without suspending
    ObjDisp(cmd)->CmdBeginRendering(Unwrap(cmd), &structs.info);
    ObjDisp(cmd)->CmdEndRendering(Unwrap(cmd));
  }
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
  // subpass0 is a patched version of the pipeline created against subpass 0, in case for old style
  // renderpasses we need to use a pipeline that was previously in subpass 1 against our loadrp with
  // only one subpass. It's not needed for dynamic rendering, we can always use the original
  // pipeline
  if(subpass0 && dynamicRendering.active)
    subpass0 = false;

  if(binding == BindGraphics || binding == BindInitial)
  {
    if(graphics.pipeline != ResourceId())
    {
      VkPipeline pipe = vk->GetResourceManager()->GetCurrentHandle<VkPipeline>(graphics.pipeline);
      const VulkanCreationInfo::Pipeline pipeinfo =
          vk->GetDebugManager()->GetPipelineInfo(graphics.pipeline);

      if(subpass0 && pipeinfo.subpass0pipe != VK_NULL_HANDLE)
        pipe = pipeinfo.subpass0pipe;

      ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

      // don't have to handle separate vert/frag layouts as push constant ranges must be identical
      ResourceId pipeLayoutId = pipeinfo.vertLayout;
      VkPipelineLayout layout =
          vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

      const rdcarray<VkPushConstantRange> &pushRanges =
          vk->GetDebugManager()->GetPipelineLayoutInfo(pipeLayoutId).pushRanges;

      // only set push constant ranges that the layout uses
      for(size_t i = 0; i < pushRanges.size(); i++)
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags,
                                       pushRanges[i].offset, pushRanges[i].size,
                                       pushconsts + pushRanges[i].offset);
    }
    else if(binding == BindInitial)
    {
      if(vk->GetDriverInfo().NVStaticPipelineRebindStates())
        ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      Unwrap(vk->GetDebugManager()->GetDummyPipeline()));

      BindLastPushConstants(vk, cmd);
    }

    if(graphics.pipeline != ResourceId())
      BindDescriptorSetsForPipeline(vk, cmd, graphics, VK_PIPELINE_BIND_POINT_GRAPHICS);
    else
      BindDescriptorSetsWithoutPipeline(vk, cmd, graphics, VK_PIPELINE_BIND_POINT_GRAPHICS);

    BindDynamicState(vk, cmd);
  }

  if(binding == BindCompute || binding == BindInitial)
  {
    if(compute.pipeline != ResourceId())
    {
      ObjDisp(cmd)->CmdBindPipeline(
          Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
          Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkPipeline>(compute.pipeline)));

      ResourceId pipeLayoutId = vk->GetDebugManager()->GetPipelineInfo(compute.pipeline).compLayout;
      VkPipelineLayout layout =
          vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

      const rdcarray<VkPushConstantRange> &pushRanges =
          vk->GetDebugManager()->GetPipelineLayoutInfo(pipeLayoutId).pushRanges;

      // only set push constant ranges that the layout uses
      for(size_t i = 0; i < pushRanges.size(); i++)
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags,
                                       pushRanges[i].offset, pushRanges[i].size,
                                       pushconsts + pushRanges[i].offset);

      BindDescriptorSetsForPipeline(vk, cmd, compute, VK_PIPELINE_BIND_POINT_COMPUTE);
    }
    else if(binding == BindInitial)
    {
      BindLastPushConstants(vk, cmd);

      BindDescriptorSetsWithoutPipeline(vk, cmd, compute, VK_PIPELINE_BIND_POINT_COMPUTE);
    }
  }

  if(binding == BindRT || binding == BindInitial)
  {
    if(rt.pipeline != ResourceId())
    {
      ObjDisp(cmd)->CmdBindPipeline(
          Unwrap(cmd), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
          Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkPipeline>(rt.pipeline)));

      ResourceId pipeLayoutId = vk->GetDebugManager()->GetPipelineInfo(rt.pipeline).compLayout;
      VkPipelineLayout layout =
          vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

      const rdcarray<VkPushConstantRange> &pushRanges =
          vk->GetDebugManager()->GetPipelineLayoutInfo(pipeLayoutId).pushRanges;

      // only set push constant ranges that the layout uses
      for(size_t i = 0; i < pushRanges.size(); i++)
        ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags,
                                       pushRanges[i].offset, pushRanges[i].size,
                                       pushconsts + pushRanges[i].offset);

      BindDescriptorSetsForPipeline(vk, cmd, rt, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
    }
  }
}

void VulkanRenderState::BindShaderObjects(WrappedVulkan *vk, VkCommandBuffer cmd,
                                          PipelineBinding binding)
{
  if(binding == BindGraphics || binding == BindInitial)
  {
    if(graphics.shaderObject)
    {
      // According to spec, shader objects don't support ray tracing shaders
      for(uint32_t i = 0; i < (uint32_t)ShaderStage::RayGen; i++)
      {
        if(i == (uint32_t)ShaderStage::Compute)
          continue;

        const VkShaderStageFlagBits stage = (VkShaderStageFlagBits)(1 << (uint32_t)i);
        const VkShaderEXT shader =
            Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkShaderEXT>(shaderObjects[i]));

        ObjDisp(cmd)->CmdBindShadersEXT(Unwrap(cmd), 1, &stage, &shader);
      }
    }

    BindLastPushConstants(vk, cmd);

    BindDescriptorSetsForShaders(vk, cmd, graphics, VK_PIPELINE_BIND_POINT_GRAPHICS);

    BindDynamicState(vk, cmd);
  }

  if(binding == BindCompute || binding == BindInitial)
  {
    if(compute.shaderObject && shaderObjects[(uint32_t)ShaderStage::Compute] != ResourceId())
    {
      const VkShaderStageFlagBits stage =
          (VkShaderStageFlagBits)(1 << (uint32_t)ShaderStage::Compute);
      const VkShaderEXT shader = Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkShaderEXT>(
          shaderObjects[(uint32_t)ShaderStage::Compute]));
      ObjDisp(cmd)->CmdBindShadersEXT(Unwrap(cmd), 1, &stage, &shader);
    }

    BindLastPushConstants(vk, cmd);

    BindDescriptorSetsForShaders(vk, cmd, compute, VK_PIPELINE_BIND_POINT_COMPUTE);
  }
}

void VulkanRenderState::BindDynamicState(WrappedVulkan *vk, VkCommandBuffer cmd)
{
  if(dynamicStates[VkDynamicRayTracingStackSizeKHR])
    ObjDisp(cmd)->CmdSetRayTracingPipelineStackSizeKHR(Unwrap(cmd), rtStackSize);

  if(!views.empty() && dynamicStates[VkDynamicViewport])
    ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), 0, (uint32_t)views.size(), &views[0]);
  if(!scissors.empty() && dynamicStates[VkDynamicScissor])
    ObjDisp(cmd)->CmdSetScissor(Unwrap(cmd), 0, (uint32_t)scissors.size(), &scissors[0]);

  if(vk->DynamicColorWrite())
  {
    if(!colorWriteEnable.empty() && dynamicStates[VkDynamicColorWriteEXT])
      ObjDisp(cmd)->CmdSetColorWriteEnableEXT(Unwrap(cmd), (uint32_t)colorWriteEnable.size(),
                                              colorWriteEnable.data());
  }

  if(vk->ExtendedDynamicState() || vk->ShaderObject())
  {
    if(!views.empty() && dynamicStates[VkDynamicViewportCount])
      ObjDisp(cmd)->CmdSetViewportWithCountEXT(Unwrap(cmd), (uint32_t)views.size(), views.data());
    if(!scissors.empty() && dynamicStates[VkDynamicScissorCount])
      ObjDisp(cmd)->CmdSetScissorWithCountEXT(Unwrap(cmd), (uint32_t)scissors.size(),
                                              scissors.data());

    if(dynamicStates[VkDynamicCullMode])
      ObjDisp(cmd)->CmdSetCullModeEXT(Unwrap(cmd), cullMode);
    if(dynamicStates[VkDynamicFrontFace])
      ObjDisp(cmd)->CmdSetFrontFaceEXT(Unwrap(cmd), frontFace);
    if(dynamicStates[VkDynamicPrimitiveTopology])
      ObjDisp(cmd)->CmdSetPrimitiveTopologyEXT(Unwrap(cmd), primitiveTopology);

    if(dynamicStates[VkDynamicDepthBoundsTestEnable])
      ObjDisp(cmd)->CmdSetDepthBoundsTestEnableEXT(Unwrap(cmd), depthBoundsTestEnable);

    if(dynamicStates[VkDynamicDepthTestEnable])
      ObjDisp(cmd)->CmdSetDepthTestEnableEXT(Unwrap(cmd), depthTestEnable);
    if(dynamicStates[VkDynamicDepthWriteEnable])
      ObjDisp(cmd)->CmdSetDepthWriteEnableEXT(Unwrap(cmd), depthWriteEnable);
    if(dynamicStates[VkDynamicDepthCompareOp])
      ObjDisp(cmd)->CmdSetDepthCompareOpEXT(Unwrap(cmd), depthCompareOp);

    if(dynamicStates[VkDynamicStencilTestEnable])
      ObjDisp(cmd)->CmdSetStencilTestEnableEXT(Unwrap(cmd), stencilTestEnable);

    if(dynamicStates[VkDynamicStencilOp])
    {
      ObjDisp(cmd)->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.failOp,
                                       front.passOp, front.depthFailOp, front.compareOp);
      ObjDisp(cmd)->CmdSetStencilOpEXT(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, front.failOp,
                                       front.passOp, front.depthFailOp, front.compareOp);
    }
  }

  if(vk->ExtendedDynamicState2() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicDepthBiasEnable])
      ObjDisp(cmd)->CmdSetDepthBiasEnableEXT(Unwrap(cmd), depthBiasEnable);
    if(dynamicStates[VkDynamicPrimRestart])
      ObjDisp(cmd)->CmdSetPrimitiveRestartEnableEXT(Unwrap(cmd), primRestartEnable);
    if(dynamicStates[VkDynamicRastDiscard])
      ObjDisp(cmd)->CmdSetRasterizerDiscardEnableEXT(Unwrap(cmd), rastDiscardEnable);
  }
  if(vk->ExtendedDynamicState2Logic() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicLogicOpEXT])
      ObjDisp(cmd)->CmdSetLogicOpEXT(Unwrap(cmd), logicOp);
  }
  if(vk->ExtendedDynamicState2CPs() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicControlPointsEXT])
      ObjDisp(cmd)->CmdSetPatchControlPointsEXT(Unwrap(cmd), patchControlPoints);
  }

  if(vk->ExtendedDynamicState3AlphaToCover() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicAlphaToCoverageEXT])
      ObjDisp(cmd)->CmdSetAlphaToCoverageEnableEXT(Unwrap(cmd), alphaToCoverageEnable);
  }
  if(vk->ExtendedDynamicState3AlphaToOne() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicAlphaToOneEXT])
      ObjDisp(cmd)->CmdSetAlphaToOneEnableEXT(Unwrap(cmd), alphaToOneEnable);
  }
  if(vk->ExtendedDynamicState3CBEnable() || vk->ShaderObject())
  {
    if(!colorBlendEnable.empty() && dynamicStates[VkDynamicColorBlendEnableEXT])
      ObjDisp(cmd)->CmdSetColorBlendEnableEXT(Unwrap(cmd), 0, (uint32_t)colorBlendEnable.size(),
                                              colorBlendEnable.data());
  }
  if(vk->ExtendedDynamicState3CBEquation() || vk->ShaderObject())
  {
    if(!colorBlendEquation.empty() && dynamicStates[VkDynamicColorBlendEquationEXT])
      ObjDisp(cmd)->CmdSetColorBlendEquationEXT(Unwrap(cmd), 0, (uint32_t)colorBlendEquation.size(),
                                                colorBlendEquation.data());
  }
  if(vk->ExtendedDynamicState3WriteMask() || vk->ShaderObject())
  {
    if(!colorWriteMask.empty() && dynamicStates[VkDynamicColorWriteMaskEXT])
      ObjDisp(cmd)->CmdSetColorWriteMaskEXT(Unwrap(cmd), 0, (uint32_t)colorWriteMask.size(),
                                            colorWriteMask.data());
  }
  if(vk->ExtendedDynamicState3ConservRast() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicConservativeRastModeEXT])
      ObjDisp(cmd)->CmdSetConservativeRasterizationModeEXT(Unwrap(cmd), conservativeRastMode);
  }
  if(vk->ExtendedDynamicState3DepthClampEnable() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicDepthClampEnableEXT])
      ObjDisp(cmd)->CmdSetDepthClampEnableEXT(Unwrap(cmd), depthClampEnable);
  }
  if(vk->ExtendedDynamicState3DepthClip() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicDepthClipEnableEXT])
      ObjDisp(cmd)->CmdSetDepthClipEnableEXT(Unwrap(cmd), depthClipEnable);
  }
  if(vk->ExtendedDynamicState3DepthClipNegative() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicDepthClipNegativeOneEXT])
      ObjDisp(cmd)->CmdSetDepthClipNegativeOneToOneEXT(Unwrap(cmd), negativeOneToOne);
  }
  if(vk->ExtendedDynamicState3PrimOverest() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicOverstimationSizeEXT])
      ObjDisp(cmd)->CmdSetExtraPrimitiveOverestimationSizeEXT(Unwrap(cmd), primOverestimationSize);
  }
  if(vk->ExtendedDynamicState3LineRast() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicLineRastModeEXT])
      ObjDisp(cmd)->CmdSetLineRasterizationModeEXT(Unwrap(cmd), lineRasterMode);
  }
  if(vk->ExtendedDynamicState3LineStipple() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicLineStippleEnableEXT])
      ObjDisp(cmd)->CmdSetLineStippleEnableEXT(Unwrap(cmd), stippledLineEnable);
  }
  if(vk->ExtendedDynamicState3LogicEnable() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicLogicOpEnableEXT])
      ObjDisp(cmd)->CmdSetLogicOpEnableEXT(Unwrap(cmd), logicOpEnable);
  }
  if(vk->ExtendedDynamicState3PolyMode() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicPolygonModeEXT])
      ObjDisp(cmd)->CmdSetPolygonModeEXT(Unwrap(cmd), polygonMode);
  }
  if(vk->ExtendedDynamicState3ProvokingVert() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicProvokingVertexModeEXT])
      ObjDisp(cmd)->CmdSetProvokingVertexModeEXT(Unwrap(cmd), provokingVertexMode);
  }
  if(vk->ExtendedDynamicState3RastSamples() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicRasterizationSamplesEXT])
      ObjDisp(cmd)->CmdSetRasterizationSamplesEXT(Unwrap(cmd), rastSamples);
  }
  if(vk->ExtendedDynamicState3RastStream() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicRasterizationStreamEXT])
      ObjDisp(cmd)->CmdSetRasterizationStreamEXT(Unwrap(cmd), rasterStream);
  }
  if(vk->ExtendedDynamicState3SampleLoc() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicSampleLocationsEnableEXT])
      ObjDisp(cmd)->CmdSetSampleLocationsEnableEXT(Unwrap(cmd), sampleLocEnable);
  }
  if(vk->ExtendedDynamicState3SampleMask() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicSampleMaskEXT])
      ObjDisp(cmd)->CmdSetSampleMaskEXT(Unwrap(cmd), rastSamples, sampleMask.data());
  }
  if(vk->ExtendedDynamicState3TesselDomain() || vk->ShaderObject())
  {
    if(dynamicStates[VkDynamicTessDomainOriginEXT])
      ObjDisp(cmd)->CmdSetTessellationDomainOriginEXT(Unwrap(cmd), domainOrigin);
  }

  if(dynamicStates[VkDynamicLineWidth] && !vk->GetDriverInfo().QualcommLineWidthDynamicStateCrash())
    ObjDisp(cmd)->CmdSetLineWidth(Unwrap(cmd), lineWidth);

  if(dynamicStates[VkDynamicDepthBias])
    ObjDisp(cmd)->CmdSetDepthBias(Unwrap(cmd), bias.depth, bias.biasclamp, bias.slope);

  if(dynamicStates[VkDynamicBlendConstants])
    ObjDisp(cmd)->CmdSetBlendConstants(Unwrap(cmd), blendConst);

  if(dynamicStates[VkDynamicDepthBounds])
    ObjDisp(cmd)->CmdSetDepthBounds(Unwrap(cmd), mindepth, maxdepth);

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

  if(!sampleLocations.locations.empty() && dynamicStates[VkDynamicSampleLocationsEXT])
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

  if(stippleFactor && dynamicStates[VkDynamicLineStippleKHR])
    ObjDisp(cmd)->CmdSetLineStippleEXT(Unwrap(cmd), stippleFactor, stipplePattern);

  if(vk->FragmentShadingRate())
  {
    if(dynamicStates[VkDynamicShadingRateKHR])
      ObjDisp(cmd)->CmdSetFragmentShadingRateKHR(Unwrap(cmd), &pipelineShadingRate,
                                                 shadingRateCombiners);
  }

  if(vk->DynamicAttachmentLoop())
  {
    if(dynamicStates[VkDynamicAttachmentFeedbackLoopEnableEXT])
      ObjDisp(cmd)->CmdSetAttachmentFeedbackLoopEnableEXT(Unwrap(cmd), feedbackAspects);
  }

  if(ibuffer.buf != ResourceId())
  {
    VkIndexType type = VK_INDEX_TYPE_UINT16;
    if(ibuffer.bytewidth == 4)
      type = VK_INDEX_TYPE_UINT32;
    else if(ibuffer.bytewidth == 1)
      type = VK_INDEX_TYPE_UINT8_KHR;

    ObjDisp(cmd)->CmdBindIndexBuffer(
        Unwrap(cmd), Unwrap(vk->GetResourceManager()->GetCurrentHandle<VkBuffer>(ibuffer.buf)),
        ibuffer.offs, type);
  }

  if((vk->DynamicVertexInput() || vk->ShaderObject()) && dynamicStates[VkDynamicVertexInputEXT])
  {
    ObjDisp(cmd)->CmdSetVertexInputEXT(Unwrap(cmd), (uint32_t)vertexBindings.size(),
                                       vertexBindings.data(), (uint32_t)vertexAttributes.size(),
                                       vertexAttributes.data());
  }

  bool dynamicStride = dynamicStates[VkDynamicVertexInputBindingStride] &&
                       (vk->ExtendedDynamicState() || vk->ShaderObject());

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
          ObjDisp(cmd)->CmdBindVertexBuffers(Unwrap(cmd), (uint32_t)i, 1, &empty, &vbuffers[i].offs);
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

void VulkanRenderState::BindDescriptorSetsForPipeline(WrappedVulkan *vk, VkCommandBuffer cmd,
                                                      VulkanStatePipeline &pipe,
                                                      VkPipelineBindPoint bindPoint)
{
  const rdcarray<ResourceId> &descSetLayouts =
      vk->GetDebugManager()->GetPipelineInfo(pipe.pipeline).descSetLayouts;

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
      // by the next action. The remaining sets are invalid, but also unused and this is
      // explicitly allowed by the spec. We just have to make sure we don't try to actively bind
      // an incompatible descriptor set.
      ResourceId createdDescSetLayoutId = vk->GetDescLayoutForDescSet(pipe.descSets[i].descSet);

      if(descSetLayouts[i] != createdDescSetLayoutId)
      {
        const DescSetLayout &createdDescLayout =
            vk->GetDebugManager()->GetDescSetLayout(createdDescSetLayoutId);

        if(!descLayout.isCompatible(createdDescLayout))
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

void VulkanRenderState::BindDescriptorSetsWithoutPipeline(WrappedVulkan *vk, VkCommandBuffer cmd,
                                                          VulkanStatePipeline &pipe,
                                                          VkPipelineBindPoint bindPoint)
{
  if(pipe.descSets.empty())
    return;

  // we try to bind descriptor sets before a pipeline when we don't have the knowledge that all sets
  // are up to date. This is used when perturbing state at an arbitrary point mid-record rather than
  // just before an action
  //
  // to do this we take the last known bound set as a 'reference' and bind everything that can be
  // compatible with it. Anything not compatible by definition has been invalidated so we don't need
  // to rebind it to be valid.

  const VulkanCreationInfo::PipelineLayout &refPipeLayout =
      vk->GetDebugManager()->GetPipelineLayoutInfo(pipe.descSets[pipe.lastBoundSet].pipeLayout);

  for(size_t i = 0; i < pipe.descSets.size(); i++)
  {
    if(pipe.descSets[i].pipeLayout == ResourceId() || pipe.descSets[i].descSet == ResourceId())
      continue;

    const VulkanCreationInfo::PipelineLayout &iPipeLayout =
        vk->GetDebugManager()->GetPipelineLayoutInfo(pipe.descSets[i].pipeLayout);

    if(i != pipe.lastBoundSet)
    {
      // if we come to a descriptor set that isn't compatible with the pipeline layout used in the
      // last bound set, don't bind this descriptor set
      // We can get into this situation if for example we have many sets bound at some point, then a
      // new descriptor set is bound to a different number which is incompatible and only that set
      // (and not the other stale ones) are needed by the next action. The remaining sets are
      // invalid, but also unused and this is explicitly allowed by the spec. We just have to make
      // sure we don't try to actively bind an incompatible descriptor set.

      // quick check, if the pipeline layout is the same as the one used to bind the reference set
      // then its certainly compatible
      if(pipe.descSets[i].pipeLayout != pipe.descSets[pipe.lastBoundSet].pipeLayout)
      {
        // are we below or above the last bound set
        if(i < pipe.lastBoundSet)
        {
          // we only check if this set is compatible with the pipeline layout on this set.
          // Technically the set might have been perturbed still, or we might invalidate this
          // binding subsequently if there was some other difference between here and the last bound
          // set, but it's fine to bind a compatible set which would be invalid - it is undefined
          // behaviour to use it anyway. If this binding *should* be valid, it will still be
          // valid at the end.

          const DescSetLayout &iDescLayout =
              vk->GetDebugManager()->GetDescSetLayout(iPipeLayout.descSetLayouts[i]);
          const DescSetLayout &refDescLayout =
              vk->GetDebugManager()->GetDescSetLayout(refPipeLayout.descSetLayouts[i]);

          if(iPipeLayout.descSetLayouts[i] != refPipeLayout.descSetLayouts[i] &&
             !iDescLayout.isCompatible(refDescLayout))
          {
            // set is incompatible, don't rebind it
            continue;
          }
        }
        else
        {
          // when binding sets above the last bound set, we need to be careful not to accidentally
          // invalidate it or any previous sets it might have been compatible with.
          // so instead of only checking this set, we check all sets up to this one are compatible
          bool compatible = true;

          for(size_t j = 0; j <= i; j++)
          {
            // if this binding only exists in the current set's pipeline layout (e.g. the reference
            // pipeline layout only had 0..4 and this is 5) then it's automatically considered
            // compatible as everything in the reference layout was compatible up to this point
            if(j >= refPipeLayout.descSetLayouts.size())
              break;

            const DescSetLayout &iDescLayout =
                vk->GetDebugManager()->GetDescSetLayout(iPipeLayout.descSetLayouts[j]);
            const DescSetLayout &refDescLayout =
                vk->GetDebugManager()->GetDescSetLayout(refPipeLayout.descSetLayouts[j]);

            if(iPipeLayout.descSetLayouts[j] != refPipeLayout.descSetLayouts[j] &&
               !iDescLayout.isCompatible(refDescLayout))
            {
              compatible = false;
              break;
            }
          }

          if(!compatible)
            continue;
        }
      }
    }

    if(pipe.descSets[i].descSet != ResourceId())
    {
      const DescSetLayout &descLayout =
          vk->GetDebugManager()->GetDescSetLayout(iPipeLayout.descSetLayouts[i]);

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

void VulkanRenderState::BindDescriptorSetsForShaders(WrappedVulkan *vk, VkCommandBuffer cmd,
                                                     VulkanStatePipeline &pipe,
                                                     VkPipelineBindPoint bindPoint)
{
  if(pipe.descSets.empty())
    return;

  const rdcarray<ResourceId> &descSetLayouts =
      vk->GetDebugManager()->GetPipelineLayoutInfo(pipe.descSets[pipe.lastBoundSet].pipeLayout).descSetLayouts;

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
      // by the next action. The remaining sets are invalid, but also unused and this is
      // explicitly allowed by the spec. We just have to make sure we don't try to actively bind
      // an incompatible descriptor set.
      ResourceId createdDescSetLayoutId = vk->GetDescLayoutForDescSet(pipe.descSets[i].descSet);

      if(descSetLayouts[i] != createdDescSetLayoutId)
      {
        const DescSetLayout &createdDescLayout =
            vk->GetDebugManager()->GetDescSetLayout(createdDescSetLayoutId);

        if(!descLayout.isCompatible(createdDescLayout))
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
  ResourceId descSet = GetPipeline(bindPoint).descSets[setIndex].descSet;
  ResourceId pipeLayout = GetPipeline(bindPoint).descSets[setIndex].pipeLayout;
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
    rdcarray<VkWriteDescriptorSetInlineUniformBlock *> allocInlineWrites;
    rdcarray<VkWriteDescriptorSetAccelerationStructureKHR *> allocASWrites;

    const WrappedVulkan::DescriptorSetInfo &setInfo = vk->GetDebugManager()->GetDescSetInfo(descSet);

    VkWriteDescriptorSet push = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    VulkanResourceManager *rm = vk->GetResourceManager();

    for(size_t b = 0; b < descLayout.bindings.size(); b++)
    {
      const DescSetLayout::Binding &layoutBind = descLayout.bindings[b];

      // skip if this binding isn't used
      if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
        continue;

      // push.dstSet; // unused for push descriptors
      push.dstBinding = (uint32_t)b;
      push.dstArrayElement = 0;
      // we can use the layout type here, since push descriptors are not allowed to be mutable
      push.descriptorType = layoutBind.layoutDescType;
      push.descriptorCount = layoutBind.descriptorCount;

      const DescriptorSetSlot *slots = setInfo.data.binds[b];

      if(push.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         push.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        VkBufferView *dst = new VkBufferView[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
          dst[a] = Unwrap(rm->GetCurrentHandle<VkBufferView>(slots[a].resource));

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
          dst[a].imageLayout = convert(slots[a].imageLayout);
          dst[a].sampler = Unwrap(rm->GetCurrentHandle<VkSampler>(slots[a].sampler));
          dst[a].imageView = Unwrap(rm->GetCurrentHandle<VkImageView>(slots[a].resource));

          if(layoutBind.immutableSampler && push.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER)
            dst[a].sampler = Unwrap(rm->GetCurrentHandle<VkSampler>(layoutBind.immutableSampler[a]));
        }

        push.pImageInfo = dst;
        allocImgWrites.push_back(dst);
      }
      else if(push.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        allocInlineWrites.push_back(new VkWriteDescriptorSetInlineUniformBlock);
        VkWriteDescriptorSetInlineUniformBlock *inlineWrite = allocInlineWrites.back();
        inlineWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
        inlineWrite->pNext = NULL;
        inlineWrite->dataSize = layoutBind.descriptorCount;
        inlineWrite->pData = setInfo.data.inlineBytes.data() + slots[0].offset;

        push.pNext = inlineWrite;
        push.descriptorCount = layoutBind.descriptorCount;
        writes.push_back(push);

        // skip validity checks
        continue;
      }
      else if(push.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        VkAccelerationStructureKHR *dst = new VkAccelerationStructureKHR[push.descriptorCount];
        for(uint32_t a = 0; a < push.descriptorCount; a++)
          dst[a] = Unwrap(rm->GetCurrentHandle<VkAccelerationStructureKHR>(slots[a].resource));

        allocASWrites.push_back(new VkWriteDescriptorSetAccelerationStructureKHR);
        VkWriteDescriptorSetAccelerationStructureKHR *asWrite = allocASWrites.back();

        asWrite->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asWrite->pNext = NULL;
        asWrite->accelerationStructureCount = layoutBind.descriptorCount;
        asWrite->pAccelerationStructures = dst;

        push.pNext = asWrite;
        push.descriptorCount = layoutBind.descriptorCount;
        writes.push_back(push);
      }
      else
      {
        VkDescriptorBufferInfo *dst = new VkDescriptorBufferInfo[push.descriptorCount];

        for(uint32_t a = 0; a < push.descriptorCount; a++)
        {
          dst[a].offset = slots[a].offset;
          dst[a].range = slots[a].GetRange();
          dst[a].buffer = Unwrap(rm->GetCurrentHandle<VkBuffer>(slots[a].resource));
        }

        push.pBufferInfo = dst;
        allocBufWrites.push_back(dst);
      }

      // start with no descriptors
      push.descriptorCount = 0;

      for(uint32_t w = 0; w < layoutBind.descriptorCount; w++)
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
    for(VkWriteDescriptorSetInlineUniformBlock *a : allocInlineWrites)
      delete a;
    for(VkWriteDescriptorSetAccelerationStructureKHR *d : allocASWrites)
    {
      delete[] d->pAccelerationStructures;
      delete d;
    }
  }
}

void VulkanRenderState::SetDynamicStatesFromPipeline(WrappedVulkan *vk)
{
  dynamicStates = vk->GetDebugManager()->GetPipelineInfo(graphics.pipeline).dynamicStates;
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

void VulkanRenderState::BindLastPushConstants(WrappedVulkan *vk, VkCommandBuffer cmd)
{
  if(pushLayout != ResourceId())
  {
    // set push constants with the last layout used
    VkPipelineLayout layout =
        vk->GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pushLayout);

    const rdcarray<VkPushConstantRange> &pushRanges =
        vk->GetDebugManager()->GetPipelineLayoutInfo(pushLayout).pushRanges;

    // only set push constant ranges that the layout uses
    for(size_t i = 0; i < pushRanges.size(); i++)
      ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags,
                                     pushRanges[i].offset, pushRanges[i].size,
                                     pushconsts + pushRanges[i].offset);
  }
}
