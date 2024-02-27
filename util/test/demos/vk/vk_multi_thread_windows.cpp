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

RD_TEST(VK_Multi_Thread_Windows, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws to as many windows as it can in parallel (one queue/thread per window).";

  int main()
  {
    std::vector<VkQueueFamilyProperties> queueProps;
    vkh::getQueueFamilyProperties(queueProps, phys);

    VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT;

    for(uint32_t q = 0; q < queueProps.size(); q++)
    {
      VkQueueFlags flags = queueProps[q].queueFlags;
      if((flags & required) == required)
      {
        queueFamilyIndex = q;
        queueCount = queueProps[q].queueCount;
        break;
      }
    }

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    std::vector<VkQueue> queues(queueCount);

    for(size_t i = 0; i < queues.size(); i++)
      vkGetDeviceQueue(device, queueFamilyIndex, (uint32_t)i, &queues[i]);

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

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    std::mutex windowMutex;
    std::vector<VulkanWindow *> windows(queueCount);

    for(size_t i = 0; i < windows.size(); i++)
      windows[i] = MakeWindow(256, 256, ("Window #" + std::to_string(i)).c_str());

    auto windowThread = [&](size_t idx) {
      VulkanWindow *win = NULL;
      VkQueue q = queues[idx];

      do
      {
        windowMutex.lock();
        win = idx < windows.size() ? windows[idx] : NULL;
        windowMutex.unlock();

        if(!win)
          break;

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
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, win);

        vkEndCommandBuffer(cmd);

        win->Submit(0, 1, {cmd}, {}, q);

        win->Present(q);
      } while(win);
    };

    std::vector<std::thread> threads(queueCount);

    for(size_t i = 0; i < threads.size(); i++)
      threads[i] = std::thread(windowThread, i);

    while(FrameLimit())
    {
      bool any = false;

      windowMutex.lock();
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
      windowMutex.unlock();

      msleep(20);

      if(!any)
        break;
    }

    std::vector<VulkanWindow *> deleteWindows;

    windowMutex.lock();
    deleteWindows.swap(windows);
    windowMutex.unlock();

    for(size_t i = 0; i < threads.size(); i++)
      threads[i].join();

    for(size_t i = 0; i < deleteWindows.size(); i++)
      delete deleteWindows[i];

    return 0;
  }
};

REGISTER_TEST();
