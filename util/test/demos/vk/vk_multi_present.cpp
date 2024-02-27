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

#include <thread>
#include "vk_test.h"

RD_TEST(VK_Multi_Present, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws to several windows and do batched presentation in vkQueuePresentKHR";

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

    const DefaultA2V red[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    const DefaultA2V green[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    const DefaultA2V blue[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb[3];

    vb[0] = AllocatedBuffer(this,
                            vkh::BufferCreateInfo(sizeof(red), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    vb[0].upload(red);
    vb[1] =
        AllocatedBuffer(this,
                        vkh::BufferCreateInfo(sizeof(green), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    vb[1].upload(green);
    vb[2] = AllocatedBuffer(this,
                            vkh::BufferCreateInfo(sizeof(blue), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    vb[2].upload(blue);

    std::vector<VulkanWindow *> windows = {
        mainWindow,
        MakeWindow(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, "green"),
        MakeWindow(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, "blue")};

    size_t frameDelay = 0;

    while(FrameLimit())
    {
      bool any = false;

      frameDelay++;

      std::vector<VulkanWindow *> presentWindows;

      // delay each window by one to try and offset image indices (if they're round robin) for a
      // better test. i.e. render only window 0 on frame 0
      for(size_t i = 0; i < std::min(windows.size(), frameDelay); i++)
      {
        VulkanWindow *win = windows[i];
        if(!win)
          break;

        presentWindows.push_back(win);

        VkCommandBuffer cmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, win);

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, win);

        vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(win->rp, win->GetFB(), win->scissor),
                             VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdSetViewport(cmd, 0, 1, &win->viewport);
        vkCmdSetScissor(cmd, 0, 1, &win->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb[i].buffer}, {0});
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, win);

        vkEndCommandBuffer(cmd);

        win->Submit(0, 1, {cmd}, {}, queue);
      }

      VulkanWindow::MultiPresent(queue, presentWindows);

      for(size_t i = 0; i < windows.size(); i++)
      {
        if(windows[i]->Update())
        {
          any = true;
        }
        else
        {
          delete windows[i];
          windows[i] = NULL;
        }
      }

      if(!any)
        break;
    }

    for(size_t i = 0; i < windows.size(); i++)
      delete windows[i];

    return 0;
  }
};

REGISTER_TEST();
