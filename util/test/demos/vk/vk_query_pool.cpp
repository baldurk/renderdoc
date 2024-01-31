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

RD_TEST(VK_Query_Pool, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests using a large query pool.";

  void Prepare(int argc, char **argv)
  {
    features.pipelineStatisticsQuery = true;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

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

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(4096, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    VkQueryPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    poolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    poolInfo.queryCount = 16 * 1024 * 1024;
    poolInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                                  VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

    VkQueryPool pool;
    vkCreateQueryPool(device, &poolInfo, NULL, &pool);

    // populate the query we'll read the first frame
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkCmdResetQueryPool(cmd, pool, 100, 4);

      vkCmdBeginQuery(cmd, pool, 100, 0);
      vkCmdEndQuery(cmd, pool, 100);

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});

      vkQueueWaitIdle(queue);
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      // use a query from a previous frame then immediately reset it
      uint32_t prevQueryIdx = (curFrame % 4);

      vkCmdCopyQueryPoolResults(cmd, pool, 100 + prevQueryIdx, 1, vb.buffer,
                                1024 + sizeof(uint64_t) * prevQueryIdx, sizeof(uint64_t),
                                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
      vkCmdResetQueryPool(cmd, pool, 100 + prevQueryIdx, 1);

      uint32_t curQueryIdx = ((curFrame + 1) % 4);

      // fill the next query
      vkCmdBeginQuery(cmd, pool, 100 + curQueryIdx, 0);
      vkCmdEndQuery(cmd, pool, 100 + curQueryIdx);

      vkCmdResetQueryPool(cmd, pool, 123456, 1);

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginQuery(cmd, pool, 123456, 0);

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      vkCmdEndQuery(cmd, pool, 123456);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDestroyQueryPool(device, pool, NULL);

    return 0;
  }
};

REGISTER_TEST();
