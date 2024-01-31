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

RD_TEST(VK_Postponed, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Checks that postponed resources are properly serialised both if they stay postponed, and "
      "if they are deleted mid-frame.";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    devExts.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    // no physical device features
  }

  int main()
  {
    vmaDedicated = true;

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

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    AllocatedImage offimg(this,
                          vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(offimg.image, "offimg");

    const int numRes = 10;
    const int startFrame = 500;

    AllocatedImage postponedImgs[numRes];
    AllocatedBuffer postponedDediBufs[numRes];
    VkDeviceMemory postponedMems[numRes];
    VkBuffer postponedLoneBufs[numRes];

    const VkPhysicalDeviceMemoryProperties *props = NULL;
    vmaGetMemoryProperties(allocator, &props);

    for(int i = 0; i < numRes; i++)
    {
      postponedImgs[i] = AllocatedImage(
          this,
          vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      postponedDediBufs[i] = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo(
              {VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU}));

      postponedDediBufs[i].upload(DefaultTri);

      vkCreateBuffer(device,
                     vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                     NULL, &postponedLoneBufs[i]);

      VkMemoryRequirements mrq;
      vkGetBufferMemoryRequirements(device, postponedLoneBufs[i], &mrq);

      VkMemoryAllocateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.allocationSize = mrq.size;

      for(uint32_t m = 0; m < props->memoryTypeCount; m++)
      {
        if((mrq.memoryTypeBits & (1u << m)) &&
           (props->memoryTypes[m].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        {
          info.memoryTypeIndex = m;
          break;
        }
      }

      vkAllocateMemory(device, &info, NULL, &postponedMems[i]);
      vkBindBufferMemory(device, postponedLoneBufs[i], postponedMems[i], 0);

      DefaultA2V *data = NULL;
      vkMapMemory(device, postponedMems[i], 0, sizeof(DefaultTri), 0, (void **)&data);
      memcpy(data, DefaultTri, sizeof(DefaultTri));
      vkUnmapMemory(device, postponedMems[i]);

      setName(postponedImgs[i].image, "Postponed Img " + std::to_string(i));
      setName(postponedDediBufs[i].buffer, "Postponed DediBuf " + std::to_string(i));
      setName(postponedLoneBufs[i], "Postponed LoneBuf " + std::to_string(i));
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_GENERAL, offimg.image),
               });

      vkCmdClearColorImage(cmd, offimg.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      if(curFrame == 1)
      {
        for(int i = 0; i < numRes; i++)
        {
          vkh::cmdPipelineBarrier(
              cmd,
              {
                  vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_GENERAL, postponedImgs[i].image),
              });

          vkCmdClearColorImage(cmd, postponedImgs[i].image, VK_IMAGE_LAYOUT_GENERAL,
                               vkh::ClearColorValue(0.2f, 1.0f, 0.2f, 1.0f), 1,
                               vkh::ImageSubresourceRange());
        }
      }

      // keep the first few frames slow so that enough wall-clock time and frame-count time passes
      // before startFrame regardless of fps
      if(curFrame < 50)
        msleep(100);

      std::vector<VkBuffer> curVBs = {vb.buffer, vb.buffer, vb.buffer, vb.buffer};

      if(curFrame >= startFrame && curFrame + 1 < startFrame + numRes)
      {
        int i = curFrame - startFrame;

        VkImageCopy region = {
            /* srcSubresource = */ {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            /* srcOffset = */ {0, 0, 0},
            /* dstSubresource = */ {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            /* dstOffset = */ {0, 0, 0},
            /* extent = */ {4, 4, 1},
        };

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                             VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_GENERAL, offimg.image),
                 });

        setMarker(cmd, "Pre-Copy");

        vkCmdCopyImage(cmd, postponedImgs[i].image, VK_IMAGE_LAYOUT_GENERAL, offimg.image,
                       VK_IMAGE_LAYOUT_GENERAL, 1, &region);

        setMarker(cmd, "Post-Copy");

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                             VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_GENERAL, offimg.image),
                 });

        vkCmdClearColorImage(cmd, offimg.image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        setMarker(cmd, "Pre-Copy");

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                             VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_GENERAL, offimg.image),
                 });

        vkCmdCopyImage(cmd, postponedImgs[i + 1].image, VK_IMAGE_LAYOUT_GENERAL, offimg.image,
                       VK_IMAGE_LAYOUT_GENERAL, 1, &region);

        setMarker(cmd, "Post-Copy");

        curVBs = {postponedDediBufs[i].buffer, postponedLoneBufs[i],

                  postponedDediBufs[i + 1].buffer, postponedLoneBufs[i + 1]};
      }

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      VkViewport v = mainWindow->viewport;
      v.width *= 0.5f;
      v.height *= 0.5f;

      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {curVBs[0]}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      v.x += v.width;
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkh::cmdBindVertexBuffers(cmd, 0, {curVBs[1]}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      v.x -= v.width;
      v.y += v.height;
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkh::cmdBindVertexBuffers(cmd, 0, {curVBs[2]}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      v.x += v.width;
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkh::cmdBindVertexBuffers(cmd, 0, {curVBs[3]}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      setMarker(cmd, "Post-Draw");

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      if(curFrame > startFrame && curFrame < startFrame + numRes)
      {
        vkDeviceWaitIdle(device);

        int i = curFrame - startFrame;

        postponedImgs[i].free();
        postponedDediBufs[i].free();

        vkDestroyBuffer(device, postponedLoneBufs[i], NULL);
        vkFreeMemory(device, postponedMems[i], NULL);

        postponedLoneBufs[i] = VK_NULL_HANDLE;
        postponedMems[i] = VK_NULL_HANDLE;
      }

      Present();
    }

    for(int i = 0; i < numRes; i++)
    {
      vkDestroyBuffer(device, postponedLoneBufs[i], NULL);
      vkFreeMemory(device, postponedMems[i], NULL);
    }

    return 0;
  }
};

REGISTER_TEST();
