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

#pragma once

#include <vector>
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
    std::vector<uint32_t> offsets;
  };
  std::vector<DescriptorAndOffsets> descSets;
};

struct VulkanRenderState
{
  enum PipelineBinding
  {
    BindNone = 0x0,
    BindGraphics = 0x1,
    BindCompute = 0x2,
  };

  VulkanRenderState(WrappedVulkan *driver, VulkanCreationInfo *createInfo);
  void BeginRenderPassAndApplyState(VkCommandBuffer cmd, PipelineBinding binding);
  void EndRenderPass(VkCommandBuffer cmd);

  void EndTransformFeedback(VkCommandBuffer cmd);

  void EndConditionalRendering(VkCommandBuffer cmd);

  void BindPipeline(VkCommandBuffer cmd, PipelineBinding binding, bool subpass0);
  void BindDescriptorSet(const DescSetLayout &descLayout, VkCommandBuffer cmd,
                         VkPipelineBindPoint bindPoint, uint32_t setIndex, uint32_t *dynamicOffsets);

  bool IsConditionalRenderingEnabled();

  // dynamic state
  std::vector<VkViewport> views;
  std::vector<VkRect2D> scissors;
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
  } front, back;

  struct
  {
    VkSampleCountFlagBits sampleCount;
    VkExtent2D gridSize;
    std::vector<VkSampleLocationEXT> locations;
  } sampleLocations;

  std::vector<VkRect2D> discardRectangles;

  uint32_t stippleFactor = 0;
  uint16_t stipplePattern = 0;

  // this should be big enough for any implementation
  byte pushconsts[1024] = {};
  // the actual number of bytes that have been uploaded
  uint32_t pushConstSize = 0;

  ResourceId renderPass;
  uint32_t subpass = 0;

  // framebuffer accessors - to allow for imageless framebuffers and prevent accidentally changing
  // only the framebuffer without updating the attachments
  void SetFramebuffer(ResourceId fb,
                      const VkRenderPassAttachmentBeginInfoKHR *attachmentsInfo = NULL);
  void SetFramebuffer(ResourceId fb, const std::vector<ResourceId> &dynamicAttachments)
  {
    framebuffer = fb;
    fbattachments = dynamicAttachments;
  }
  ResourceId GetFramebuffer() const { return framebuffer; }
  const std::vector<ResourceId> &GetFramebufferAttachments() const { return fbattachments; }
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
  };
  std::vector<VertBuffer> vbuffers;

  struct XFBBuffer
  {
    ResourceId buf;
    VkDeviceSize offs = 0;
    VkDeviceSize size = 0;
  };
  std::vector<XFBBuffer> xfbbuffers;

  struct XFBCounter
  {
    ResourceId buf;
    VkDeviceSize offs = 0;
  };
  uint32_t firstxfbcounter = 0;
  std::vector<XFBCounter> xfbcounters;

  struct ConditionalRendering
  {
    ResourceId buffer;
    VkDeviceSize offset = 0;
    VkConditionalRenderingFlagsEXT flags = 0;

    bool forceDisable = false;
  } conditionalRendering;

  VulkanResourceManager *GetResourceManager();
  VulkanCreationInfo *m_CreationInfo;
  WrappedVulkan *m_pDriver;

private:
  ResourceId framebuffer;
  std::vector<ResourceId> fbattachments;
};
