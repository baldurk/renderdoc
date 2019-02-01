/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "replay/replay_driver.h"
#include "vk_common.h"
#include "vk_core.h"

struct TextPrintState
{
  VkCommandBuffer cmd;
  VkRenderPass rp;
  VkFramebuffer fb;
  uint32_t w, h;
  VkFormat fmt;
};

class VulkanTextRenderer
{
public:
  VulkanTextRenderer(WrappedVulkan *driver);
  ~VulkanTextRenderer();

  void BeginText(const TextPrintState &textstate);
  void RenderText(const TextPrintState &textstate, float x, float y, const char *fmt, ...);
  void EndText(const TextPrintState &textstate);

private:
  void RenderTextInternal(const TextPrintState &textstate, float x, float y, const char *text);

  static const uint32_t FONT_TEX_WIDTH = 256;
  static const uint32_t FONT_TEX_HEIGHT = 128;

  WrappedVulkan *m_pDriver = NULL;
  VkDevice m_Device = VK_NULL_HANDLE;

  float m_FontCharAspect = 1.0f;
  float m_FontCharSize = 1.0f;

  VkDescriptorSetLayout m_TextDescSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout m_TextPipeLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_TextDescSet = VK_NULL_HANDLE;

  // 0 - RGBA8_SRGB, 1 - RGBA8, 2 - BGRA8_SRGB, 3 - BGRA8
  VkPipeline m_TextPipeline[4] = {VK_NULL_HANDLE};

  VkSampler m_LinearSampler = VK_NULL_HANDLE;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

  GPUBuffer m_TextGeneralUBO;
  GPUBuffer m_TextGlyphUBO;
  GPUBuffer m_TextStringUBO;
  VkImage m_TextAtlas = VK_NULL_HANDLE;
  VkDeviceMemory m_TextAtlasMem = VK_NULL_HANDLE;
  VkImageView m_TextAtlasView = VK_NULL_HANDLE;
  GPUBuffer m_TextAtlasUpload;
};