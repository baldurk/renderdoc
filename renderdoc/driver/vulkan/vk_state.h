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

#pragma once

#include "vk_common.h"

struct VulkanCreationInfo;
class VulkanResourceManager;
class WrappedVulkan;
struct DescSetLayout;

struct VulkanStatePipeline
{
  ResourceId pipeline;

  struct DescriptorAndOffsets
  {
    ResourceId pipeLayout;
    ResourceId descSet;
    rdcarray<uint32_t> offsets;
  };
  rdcarray<DescriptorAndOffsets> descSets;
};

struct VulkanRenderState
{
  enum PipelineBinding
  {
    BindNone = 0x0,
    BindGraphics = 0x1,
    BindCompute = 0x2,
  };

  VulkanRenderState();
  bool IsConditionalRenderingEnabled();
  void BeginRenderPassAndApplyState(WrappedVulkan *vk, VkCommandBuffer cmd, PipelineBinding binding);
  void BindPipeline(WrappedVulkan *vk, VkCommandBuffer cmd, PipelineBinding binding, bool subpass0);

  void BindDescriptorSets(WrappedVulkan *vk, VkCommandBuffer cmd, VulkanStatePipeline &pipe,
                          VkPipelineBindPoint bindPoint);

  void BindDescriptorSet(WrappedVulkan *vk, const DescSetLayout &descLayout, VkCommandBuffer cmd,
                         VkPipelineBindPoint bindPoint, uint32_t setIndex, uint32_t *dynamicOffsets);

  void EndRenderPass(VkCommandBuffer cmd);

  void EndTransformFeedback(WrappedVulkan *vk, VkCommandBuffer cmd);

  void EndConditionalRendering(VkCommandBuffer cmd);

  // dynamic state
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

  uint32_t stippleFactor = 0;
  uint16_t stipplePattern = 0;

  // this should be big enough for any implementation
  byte pushconsts[1024] = {};
  // the actual number of bytes that have been uploaded
  uint32_t pushConstSize = 0;

  ResourceId renderPass;
  uint32_t subpass = 0;
  VkSubpassContents subpassContents;

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

  VkRect2D renderArea = {};

  VulkanStatePipeline compute, graphics;

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

private:
  ResourceId framebuffer;
  rdcarray<ResourceId> fbattachments;
};
