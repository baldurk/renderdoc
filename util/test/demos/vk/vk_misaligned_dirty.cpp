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

#include "vk_test.h"

RD_TEST(VK_Misaligned_Dirty, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Generate a case where the initial states for a buffer end up being misaligned with what can "
      "be cleared.";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    const float val = 2.0f / 3.0f;

    DefaultA2V tri[4] = {
        {Vec3f(-val, -val, val), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, val, val), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(val, -val, val), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {},
    };

    AllocatedBuffer copy_src(this,
                             vkh::BufferCreateInfo(sizeof(tri), VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                             VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    setName(copy_src.buffer, "copy_src");

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    setName(vb.buffer, "vb");

    vb.upload(tri);

    tri[0].pos = Vec3f(0.0f, 0.0f, 10.0f);

    copy_src.upload(tri);

    VmaAllocationInfo alloc_info;

    vmaGetAllocationInfo(copy_src.allocator, copy_src.alloc, &alloc_info);

    float *mapped = NULL;
    vkMapMemory(device, alloc_info.deviceMemory, alloc_info.offset + sizeof(DefaultA2V) * 3,
                sizeof(Vec4f), 0, (void **)&mapped);

    float counter = 0;
    while(Running())
    {
      counter += 1.0f;
      mapped[2] = counter;

      VkCommandBuffer cmd = GetCommandBuffer();

      // create a dummy submit which uses the memory. This will serialise the whole memory contents
      // (we don't create reference data until after this)
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      setMarker(cmd, "First Submit");
      vkCmdUpdateBuffer(cmd, copy_src.buffer, sizeof(Vec3f), sizeof(Vec4f), &tri[0].col);
      vkEndCommandBuffer(cmd);
      Submit(0, 3, {cmd});

      counter += 1.0f;
      mapped[2] = counter;

      cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      VkBufferCopy region = {};
      region.srcOffset = 3 + sizeof(DefaultA2V);
      region.dstOffset = 3 + sizeof(DefaultA2V);
      region.size = 7;
      vkCmdCopyBuffer(cmd, copy_src.buffer, vb.buffer, 1, &region);
      region.srcOffset = sizeof(DefaultA2V) * 3;
      region.dstOffset = sizeof(DefaultA2V) * 3;
      region.size = sizeof(DefaultA2V);
      vkCmdCopyBuffer(cmd, copy_src.buffer, vb.buffer, 1, &region);

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "Second Submit");
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(1, 3, {cmd});

      mapped[2] = counter - 1.0f;

      cmd = GetCommandBuffer();

      // create a dummy submit which uses the memory. This will serialise the whole memory contents
      // (we don't create reference data until after this)
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      setMarker(cmd, "Third Submit");
      vkCmdUpdateBuffer(cmd, copy_src.buffer, sizeof(Vec3f), sizeof(Vec4f), &tri[0].col);
      vkEndCommandBuffer(cmd);
      Submit(2, 3, {cmd});

      Present();
    }

    vkUnmapMemory(device, alloc_info.deviceMemory);

    return 0;
  }
};

REGISTER_TEST();
