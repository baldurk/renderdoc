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

#include <assert.h>
#include <algorithm>
#include "../test_common.h"
#include "vk_headers.h"

#include "vk_helpers.h"

namespace vkh
{
const char *result_str(VkResult vkr)
{
#define VKR_STR(v) \
  case v: return #v;

  switch(vkr)
  {
    VKR_STR(VK_SUCCESS);
    VKR_STR(VK_NOT_READY);
    VKR_STR(VK_TIMEOUT);
    VKR_STR(VK_EVENT_SET);
    VKR_STR(VK_EVENT_RESET);
    VKR_STR(VK_INCOMPLETE);
    VKR_STR(VK_ERROR_OUT_OF_HOST_MEMORY);
    VKR_STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    VKR_STR(VK_ERROR_INITIALIZATION_FAILED);
    VKR_STR(VK_ERROR_DEVICE_LOST);
    VKR_STR(VK_ERROR_MEMORY_MAP_FAILED);
    VKR_STR(VK_ERROR_LAYER_NOT_PRESENT);
    VKR_STR(VK_ERROR_EXTENSION_NOT_PRESENT);
    VKR_STR(VK_ERROR_FEATURE_NOT_PRESENT);
    VKR_STR(VK_ERROR_INCOMPATIBLE_DRIVER);
    VKR_STR(VK_ERROR_TOO_MANY_OBJECTS);
    VKR_STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
    VKR_STR(VK_ERROR_FRAGMENTED_POOL);
    VKR_STR(VK_ERROR_SURFACE_LOST_KHR);
    VKR_STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    VKR_STR(VK_SUBOPTIMAL_KHR);
    VKR_STR(VK_ERROR_OUT_OF_DATE_KHR);
    VKR_STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    VKR_STR(VK_ERROR_VALIDATION_FAILED_EXT);
    VKR_STR(VK_ERROR_INVALID_SHADER_NV);
    VKR_STR(VK_ERROR_OUT_OF_POOL_MEMORY_KHR);
    VKR_STR(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR);
    VKR_STR(VK_ERROR_NOT_PERMITTED_EXT);
    VKR_STR(VK_RESULT_MAX_ENUM);
    default: break;
  }

  return "VK_RESULT_????";
}

template <>
VkFormat _FormatFromObj<float>()
{
  return VK_FORMAT_R32_SFLOAT;
}

template <>
VkFormat _FormatFromObj<Vec4f>()
{
  return VK_FORMAT_R32G32B32A32_SFLOAT;
}
template <>
VkFormat _FormatFromObj<Vec3f>()
{
  return VK_FORMAT_R32G32B32_SFLOAT;
}
template <>
VkFormat _FormatFromObj<Vec2f>()
{
  return VK_FORMAT_R32G32_SFLOAT;
}

void updateDescriptorSets(VkDevice device, const std::vector<VkWriteDescriptorSet> &writes,
                          const std::vector<VkCopyDescriptorSet> &copies)
{
  vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), (uint32_t)copies.size(),
                         copies.data());
}

void cmdPipelineBarrier(VkCommandBuffer cmd, std::initializer_list<VkImageMemoryBarrier> img,
                        std::initializer_list<VkBufferMemoryBarrier> buf,
                        std::initializer_list<VkMemoryBarrier> mem, VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags)
{
  vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, dependencyFlags, (uint32_t)mem.size(),
                       &(*mem.begin()), (uint32_t)buf.size(), &(*buf.begin()), (uint32_t)img.size(),
                       &(*img.begin()));
}

void cmdPipelineBarrier(VkCommandBuffer cmd, const std::vector<VkImageMemoryBarrier> &img,
                        const std::vector<VkBufferMemoryBarrier> &buf,
                        const std::vector<VkMemoryBarrier> &mem, VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags)
{
  vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, dependencyFlags, (uint32_t)mem.size(),
                       mem.data(), (uint32_t)buf.size(), buf.data(), (uint32_t)img.size(),
                       img.data());
}

void cmdClearImage(VkCommandBuffer cmd, VkImage img, const ClearColorValue &col, VkImageLayout layout)
{
  vkCmdClearColorImage(cmd, img, layout, col, 1, vkh::ImageSubresourceRange());
}

void cmdClearImage(VkCommandBuffer cmd, VkImage img, const ClearDepthStencilValue &ds,
                   VkImageLayout layout)
{
  vkCmdClearDepthStencilImage(cmd, img, layout, ds, 1, vkh::ImageSubresourceRange());
}

void cmdBindVertexBuffers(VkCommandBuffer cmd, uint32_t firstBinding,
                          std::initializer_list<VkBuffer> bufs,
                          std::initializer_list<VkDeviceSize> offsets)
{
  vkCmdBindVertexBuffers(cmd, firstBinding, (uint32_t)bufs.size(), &(*bufs.begin()),
                         &(*offsets.begin()));
}

void cmdBindVertexBuffers(VkCommandBuffer cmd, std::initializer_list<VkBuffer> bufs)
{
  VkDeviceSize offsets[32] = {};
  TEST_ASSERT(bufs.size() < 32, "More than 32 VBs being bound");
  vkCmdBindVertexBuffers(cmd, 0, (uint32_t)bufs.size(), &(*bufs.begin()), offsets);
}

void cmdBindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint pipelineBindPoint,
                           VkPipelineLayout layout, uint32_t firstSet,
                           std::vector<VkDescriptorSet> sets, std::vector<uint32_t> dynamicOffsets)
{
  vkCmdBindDescriptorSets(cmd, pipelineBindPoint, layout, firstSet, (uint32_t)sets.size(),
                          sets.data(), (uint32_t)dynamicOffsets.size(), dynamicOffsets.data());
}

void cmdPushDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint pipelineBindPoint,
                           VkPipelineLayout layout, uint32_t set,
                           std::vector<VkWriteDescriptorSet> writes)
{
  vkCmdPushDescriptorSetKHR(cmd, pipelineBindPoint, layout, set, (uint32_t)writes.size(),
                            writes.data());
}

GraphicsPipelineCreateInfo::GraphicsPipelineCreateInfo()
{
  sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pNext = NULL;
  flags = 0;
  layout = VK_NULL_HANDLE;
  renderPass = VK_NULL_HANDLE;
  subpass = 0;
  basePipelineHandle = VK_NULL_HANDLE;
  basePipelineIndex = -1;
  pVertexInputState = &vertexInputState;
  pInputAssemblyState = &inputAssemblyState;
  pTessellationState = &tessellationState;
  pViewportState = &viewportState;
  pRasterizationState = &rasterizationState;
  pMultisampleState = &multisampleState;
  pDepthStencilState = &depthStencilState;
  pColorBlendState = &colorBlendState;
  pDynamicState = &dynamicState;

  // defaults
  // 1 viewport/scissor, dynamic
  dynamicState.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  viewportState.viewportCount = viewportState.scissorCount = 1;

  inputAssemblyState = VkPipelineInputAssemblyStateCreateInfo();
  inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  tessellationState = VkPipelineTessellationStateCreateInfo();
  tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

  rasterizationState = VkPipelineRasterizationStateCreateInfo();
  rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationState.depthClampEnable = VK_FALSE;
  rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationState.cullMode = VK_CULL_MODE_NONE;
  rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizationState.lineWidth = 1.0f;

  multisampleState = VkPipelineMultisampleStateCreateInfo();
  multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  depthStencilState = VkPipelineDepthStencilStateCreateInfo();
  depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencilState.minDepthBounds = 0.0f;
  depthStencilState.maxDepthBounds = 1.0f;
  depthStencilState.front.compareMask = 0xff;
  depthStencilState.front.writeMask = 0xff;
  depthStencilState.front.compareOp = VK_COMPARE_OP_EQUAL;
  depthStencilState.front.passOp = VK_STENCIL_OP_REPLACE;
  depthStencilState.front.failOp = VK_STENCIL_OP_KEEP;
  depthStencilState.front.depthFailOp = VK_STENCIL_OP_KEEP;
  depthStencilState.back = depthStencilState.front;

  colorBlendState.attachments.push_back({
      // blendEnable
      VK_FALSE,
      // color*
      VK_BLEND_FACTOR_SRC_ALPHA,
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      VK_BLEND_OP_ADD,
      // alpha*
      VK_BLEND_FACTOR_SRC_ALPHA,
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      VK_BLEND_OP_ADD,
      // colorWriteMask
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
          VK_COLOR_COMPONENT_A_BIT,
  });
}

const GraphicsPipelineCreateInfo &GraphicsPipelineCreateInfo::operator=(
    const GraphicsPipelineCreateInfo &other)
{
  sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pNext = other.pNext;
  flags = other.flags;
  layout = other.layout;
  renderPass = other.renderPass;
  subpass = other.subpass;
  basePipelineHandle = other.basePipelineHandle;
  basePipelineIndex = other.basePipelineIndex;

  stages = other.stages;

  inputAssemblyState = other.inputAssemblyState;
  tessellationState = other.tessellationState;
  rasterizationState = other.rasterizationState;
  multisampleState = other.multisampleState;
  depthStencilState = other.depthStencilState;

  vertexInputState = other.vertexInputState;
  inputAssemblyState = other.inputAssemblyState;
  tessellationState = other.tessellationState;
  viewportState = other.viewportState;
  rasterizationState = other.rasterizationState;
  multisampleState = other.multisampleState;
  depthStencilState = other.depthStencilState;
  colorBlendState = other.colorBlendState;
  dynamicState = other.dynamicState;

  pVertexInputState = &vertexInputState;
  pInputAssemblyState = &inputAssemblyState;
  pTessellationState = &tessellationState;
  pViewportState = &viewportState;
  pRasterizationState = &rasterizationState;
  pMultisampleState = &multisampleState;
  pDepthStencilState = &depthStencilState;
  pColorBlendState = &colorBlendState;
  pDynamicState = &dynamicState;

  return *this;
}

void GraphicsPipelineCreateInfo::bake()
{
  pVertexInputState = &vertexInputState;
  pInputAssemblyState = &inputAssemblyState;
  pTessellationState = &tessellationState;
  pViewportState = &viewportState;
  pRasterizationState = &rasterizationState;
  pMultisampleState = &multisampleState;
  pDepthStencilState = &depthStencilState;
  pColorBlendState = &colorBlendState;
  pDynamicState = &dynamicState;

  stageCount = (uint32_t)stages.size();
  pStages = stages.data();

  vertexInputState.pVertexAttributeDescriptions = vertexInputState.vertexAttributeDescriptions.data();
  vertexInputState.vertexAttributeDescriptionCount =
      (uint32_t)vertexInputState.vertexAttributeDescriptions.size();
  vertexInputState.pVertexBindingDescriptions = vertexInputState.vertexBindingDescriptions.data();
  vertexInputState.vertexBindingDescriptionCount =
      (uint32_t)vertexInputState.vertexBindingDescriptions.size();

  viewportState.pViewports = viewportState.viewports.data();
  viewportState.viewportCount =
      std::max(viewportState.viewportCount, (uint32_t)viewportState.viewports.size());

  viewportState.pScissors = viewportState.scissors.data();
  viewportState.scissorCount =
      std::max(viewportState.scissorCount, (uint32_t)viewportState.scissors.size());

  colorBlendState.attachmentCount = (uint32_t)colorBlendState.attachments.size();
  colorBlendState.pAttachments = colorBlendState.attachments.data();

  dynamicState.pDynamicStates = dynamicState.dynamicStates.data();
  dynamicState.dynamicStateCount = (uint32_t)dynamicState.dynamicStates.size();
}

};    // namespace vkh
