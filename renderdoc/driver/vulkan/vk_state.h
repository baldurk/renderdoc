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

#include "vk_common.h"

struct VulkanCreationInfo;
class VulkanResourceManager;
class WrappedVulkan;
struct DescSetLayout;

struct VulkanStatePipeline
{
  ResourceId pipeline;

  // shader object
  bool shaderObject = false;

  struct DescriptorAndOffsets
  {
    ResourceId pipeLayout;
    ResourceId descSet;
    rdcarray<uint32_t> offsets;
  };
  rdcarray<DescriptorAndOffsets> descSets;
  // the index of the last set bound. In the case where we are re-binding sets and don't have a
  // valid pipeline to reference, this can help us resolve which descriptor sets to rebind in the
  // event that they're not all compatible
  uint32_t lastBoundSet = 0;
};

struct VulkanRenderState
{
  enum PipelineBinding
  {
    BindNone = 0x0,
    BindGraphics = 0x1,
    BindCompute = 0x2,
    BindRT = 0x4,
    BindInitial = 0x8,
  };

  VulkanRenderState();
  bool IsConditionalRenderingEnabled();
  void BeginRenderPassAndApplyState(WrappedVulkan *vk, VkCommandBuffer cmd, PipelineBinding binding,
                                    bool obeySuspending);
  void BindPipeline(WrappedVulkan *vk, VkCommandBuffer cmd, PipelineBinding binding, bool subpass0);
  void BindShaderObjects(WrappedVulkan *vk, VkCommandBuffer cmd, PipelineBinding binding);
  void BindDynamicState(WrappedVulkan *vk, VkCommandBuffer cmd);

  void EndRenderPass(VkCommandBuffer cmd);
  void FinishSuspendedRenderPass(VkCommandBuffer cmd);

  void EndTransformFeedback(WrappedVulkan *vk, VkCommandBuffer cmd);

  void EndConditionalRendering(VkCommandBuffer cmd);

  // dynamic state - this mask tracks which dynamic states have been set, since we can't rely on the
  // bound pipeline to know which ones to re-bind if we're reapplying state. It's possible to bind
  // a static pipeline then set some dynamic state. If we want to push and pop state at that point
  // (say for a discard pattern) we need to restore the dynamic state even if it's "invalid" for the
  // pipeline, as the application presumably then binds a dynamic pipeline afterwards before drawing
  rdcfixedarray<bool, VkDynamicCount> dynamicStates = {};

  rdcarray<VkViewport> views;
  rdcarray<VkRect2D> scissors;
  float lineWidth = 1.0f;
  struct
  {
    float depth = 0.0f;
    float biasclamp = 0.0f;
    float slope = 0.0f;
  } bias;
  float blendConst[4] = {};
  float mindepth = 0.0f;
  float maxdepth = 1.0f;
  struct
  {
    uint32_t compare = 0;
    uint32_t write = 0;
    uint32_t ref = 0;

    // extended dynamic state
    VkStencilOp failOp = VK_STENCIL_OP_KEEP, passOp = VK_STENCIL_OP_KEEP,
                depthFailOp = VK_STENCIL_OP_KEEP;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
  } front, back;

  struct
  {
    VkSampleCountFlagBits sampleCount;
    VkExtent2D gridSize;
    rdcarray<VkSampleLocationEXT> locations;
  } sampleLocations;

  rdcarray<VkRect2D> discardRectangles;

  // raytracing stack size
  uint32_t rtStackSize = 0;

  uint32_t stippleFactor = 0;
  uint16_t stipplePattern = 0;

  // this should be big enough for any implementation
  byte pushconsts[1024] = {};
  // the actual number of bytes that have been uploaded
  uint32_t pushConstSize = 0;
  // the layout last used to push. Used for handling cases where we are rebinding
  // partial/temporarily invalid state like if we are resetting state after a discard pattern and
  // the command buffer has only pushed some constants but not yet bound a pipeline
  ResourceId pushLayout;

  uint32_t subpass = 0;
  VkSubpassContents subpassContents;

  // helper function - if the pipeline has been changed, this forces all dynamic state to be bound
  // that the pipeline needs (and no other dynamic state). Helpful if the state is mutated just
  // before a draw, before replaying that draw
  void SetDynamicStatesFromPipeline(WrappedVulkan *m_pDriver);

  // framebuffer accessors - to allow for imageless framebuffers and prevent accidentally changing
  // only the framebuffer without updating the attachments
  void SetFramebuffer(WrappedVulkan *vk, ResourceId fb,
                      const VkRenderPassAttachmentBeginInfo *attachmentsInfo = NULL);

  void SetFramebuffer(ResourceId fb, const rdcarray<ResourceId> &dynamicAttachments)
  {
    framebuffer = fb;
    fbattachments = dynamicAttachments;
  }
  ResourceId GetFramebuffer() const { return framebuffer; }
  const rdcarray<ResourceId> &GetFramebufferAttachments() const { return fbattachments; }
  //

  // same for renderpass
  void SetRenderPass(ResourceId rp)
  {
    renderPass = rp;
    dynamicRendering = DynamicRendering();
  }
  ResourceId GetRenderPass() const { return renderPass; }
  bool ActiveRenderPass() const { return renderPass != ResourceId() || dynamicRendering.active; }
  VkRect2D renderArea = {};

  VulkanStatePipeline compute, graphics, rt;

  VulkanStatePipeline &GetPipeline(VkPipelineBindPoint pipelineBindPoint)
  {
    if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
      return graphics;
    else if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
      return rt;
    return compute;
  }

  struct IdxBuffer
  {
    ResourceId buf;
    VkDeviceSize offs = 0;
    int bytewidth = 0;
  } ibuffer;

  struct VertBuffer
  {
    ResourceId buf;
    VkDeviceSize offs = 0;

    // extended dynamic state
    VkDeviceSize size = VK_WHOLE_SIZE;
    VkDeviceSize stride = 0;
  };
  rdcarray<VertBuffer> vbuffers;

  struct XFBBuffer
  {
    ResourceId buf;
    VkDeviceSize offs = 0;
    VkDeviceSize size = 0;
  };
  rdcarray<XFBBuffer> xfbbuffers;

  struct XFBCounter
  {
    ResourceId buf;
    VkDeviceSize offs = 0;
  };
  uint32_t firstxfbcounter = 0;
  rdcarray<XFBCounter> xfbcounters;

  struct ConditionalRendering
  {
    ResourceId buffer;
    VkDeviceSize offset = 0;
    VkConditionalRenderingFlagsEXT flags = 0;

    bool forceDisable = false;
  } conditionalRendering;

  // extended dynamic state
  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
  VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;

  VkBool32 depthBoundsTestEnable = VK_FALSE;
  VkBool32 depthTestEnable = VK_FALSE;
  VkBool32 depthWriteEnable = VK_FALSE;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_ALWAYS;

  VkBool32 stencilTestEnable = VK_FALSE;

  // color write enable
  rdcarray<VkBool32> colorWriteEnable;

  // extended dynamic state 2
  VkBool32 depthBiasEnable = VK_FALSE;
  VkLogicOp logicOp = VK_LOGIC_OP_CLEAR;
  uint32_t patchControlPoints = 3;
  VkBool32 primRestartEnable = VK_FALSE;
  VkBool32 rastDiscardEnable = VK_FALSE;

  // extended dynamic state 3
  VkBool32 alphaToCoverageEnable = VK_FALSE;
  VkBool32 alphaToOneEnable = VK_FALSE;
  rdcarray<VkBool32> colorBlendEnable;
  rdcarray<VkColorBlendEquationEXT> colorBlendEquation;
  rdcarray<VkColorComponentFlags> colorWriteMask;
  VkConservativeRasterizationModeEXT conservativeRastMode =
      VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
  VkBool32 depthClampEnable = VK_FALSE;
  VkBool32 depthClipEnable = VK_FALSE;
  VkBool32 negativeOneToOne = VK_FALSE;
  float primOverestimationSize = 0.0f;
  VkLineRasterizationModeKHR lineRasterMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR;
  VkBool32 stippledLineEnable = VK_FALSE;
  VkBool32 logicOpEnable = VK_FALSE;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  VkProvokingVertexModeEXT provokingVertexMode = VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;
  VkSampleCountFlagBits rastSamples = VK_SAMPLE_COUNT_1_BIT;
  uint32_t rasterStream = 0;
  VkBool32 sampleLocEnable = VK_FALSE;
  rdcarray<VkSampleMask> sampleMask = {0};
  VkTessellationDomainOrigin domainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

  // dynamic vertex input
  rdcarray<VkVertexInputBindingDescription2EXT> vertexBindings;
  rdcarray<VkVertexInputAttributeDescription2EXT> vertexAttributes;

  // dynamic rendering
  struct DynamicRendering
  {
    bool active = false;
    bool suspended = false;
    VkRenderingFlags flags = 0;
    uint32_t layerCount = 0;
    uint32_t viewMask = 0;
    rdcarray<VkRenderingAttachmentInfo> color = {};
    VkRenderingAttachmentInfo depth = {};
    VkRenderingAttachmentInfo stencil = {};

    VkImageView fragmentDensityView = VK_NULL_HANDLE;
    VkImageLayout fragmentDensityLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageView shadingRateView = VK_NULL_HANDLE;
    VkImageLayout shadingRateLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D shadingRateTexelSize = {1, 1};

    bool tileOnlyMSAAEnable = false;
    VkSampleCountFlagBits tileOnlyMSAASampleCount = VK_SAMPLE_COUNT_1_BIT;
  } dynamicRendering;

  // fdm offset
  rdcarray<VkOffset2D> fragmentDensityMapOffsets;

  // shading rate
  VkExtent2D pipelineShadingRate = {1, 1};
  VkFragmentShadingRateCombinerOpKHR shadingRateCombiners[2] = {
      VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
      VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
  };

  // shader objects
  ResourceId shaderObjects[NumShaderStages];

  // attachment feedback loop
  VkImageAspectFlags feedbackAspects = VK_IMAGE_ASPECT_NONE;

private:
  ResourceId renderPass;
  ResourceId framebuffer;
  rdcarray<ResourceId> fbattachments;

  void BindDescriptorSetsForPipeline(WrappedVulkan *vk, VkCommandBuffer cmd,
                                     VulkanStatePipeline &pipe, VkPipelineBindPoint bindPoint);

  void BindDescriptorSetsWithoutPipeline(WrappedVulkan *vk, VkCommandBuffer cmd,
                                         VulkanStatePipeline &pipe, VkPipelineBindPoint bindPoint);

  void BindDescriptorSetsForShaders(WrappedVulkan *vk, VkCommandBuffer cmd,
                                    VulkanStatePipeline &pipe, VkPipelineBindPoint bindPoint);

  void BindDescriptorSet(WrappedVulkan *vk, const DescSetLayout &descLayout, VkCommandBuffer cmd,
                         VkPipelineBindPoint bindPoint, uint32_t setIndex, uint32_t *dynamicOffsets);

  void BindLastPushConstants(WrappedVulkan *vk, VkCommandBuffer cmd);
};
