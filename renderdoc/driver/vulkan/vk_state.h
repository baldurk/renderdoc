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
  VulkanRenderState &operator=(const VulkanRenderState &o);
  void BeginRenderPassAndApplyState(VkCommandBuffer cmd, PipelineBinding binding);
  void EndRenderPass(VkCommandBuffer cmd);

  void EndTransformFeedback(VkCommandBuffer cmd);

  void EndConditionalRendering(VkCommandBuffer cmd);

  void BindPipeline(VkCommandBuffer cmd, PipelineBinding binding, bool subpass0);
  void BindDescriptorSet(const DescSetLayout &descLayout, VkCommandBuffer cmd,
                         VkPipelineLayout layout, VkPipelineBindPoint bindPoint, uint32_t setIndex,
                         uint32_t *dynamicOffsets);

  bool IsConditionalRenderingEnabled();

  // dynamic state
  std::vector<VkViewport> views;
  std::vector<VkRect2D> scissors;
  float lineWidth;
  struct
  {
    float depth, biasclamp, slope;
  } bias;
  float blendConst[4];
  float mindepth, maxdepth;
  struct
  {
    uint32_t compare, write, ref;
  } front, back;

  struct
  {
    VkSampleCountFlagBits sampleCount;
    VkExtent2D gridSize;
    std::vector<VkSampleLocationEXT> locations;
  } sampleLocations;

  std::vector<VkRect2D> discardRectangles;

  // this should be big enough for any implementation
  byte pushconsts[1024];
  // the actual number of bytes that have been uploaded
  uint32_t pushConstSize = 0;

  ResourceId renderPass;
  uint32_t subpass;

  ResourceId framebuffer;
  VkRect2D renderArea;

  VulkanStatePipeline compute, graphics;

  struct IdxBuffer
  {
    ResourceId buf;
    VkDeviceSize offs;
    int bytewidth;
  } ibuffer;

  struct VertBuffer
  {
    ResourceId buf;
    VkDeviceSize offs;
  };
  std::vector<VertBuffer> vbuffers;

  struct XFBBuffer
  {
    ResourceId buf;
    VkDeviceSize offs;
    VkDeviceSize size;
  };
  std::vector<XFBBuffer> xfbbuffers;

  struct XFBCounter
  {
    ResourceId buf;
    VkDeviceSize offs;
  };
  uint32_t firstxfbcounter = 0;
  std::vector<XFBCounter> xfbcounters;

  struct ConditionalRendering
  {
    ResourceId buffer;
    VkDeviceSize offset;
    VkConditionalRenderingFlagsEXT flags;

    bool forceDisable;
  } conditionalRendering;

  VulkanResourceManager *GetResourceManager();
  VulkanCreationInfo *m_CreationInfo;
  WrappedVulkan *m_pDriver;

  std::vector<VkImageMemoryBarrier> rpBarriers;
};
