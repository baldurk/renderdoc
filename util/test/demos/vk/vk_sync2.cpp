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

RD_TEST(VK_Synchronization_2, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests use of KHR_VK_Synchronization2.";

  void Prepare(int argc, char **argv)
  {
    instExts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
    };

    getPhysFeatures2(&sync2Features);

    if(!sync2Features.synchronization2)
      Avail = "'synchronization2' not available";

    devInfoNext = &sync2Features;
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

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    vkh::ImageCreateInfo preinitInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    preinitInfo.tiling = VK_IMAGE_TILING_LINEAR;
    preinitInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    const VkPhysicalDeviceMemoryProperties *props = NULL;
    vmaGetMemoryProperties(allocator, &props);

    VkImage unboundImg = VK_NULL_HANDLE;
    vkCreateImage(device, preinitInfo, NULL, &unboundImg);
    setName(unboundImg, "Unbound image");

    VkEvent ev = VK_NULL_HANDLE;
    CHECK_VKR(vkCreateEvent(device, vkh::EventCreateInfo(VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR), NULL,
                            &ev));

    VkQueryPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    poolInfo.queryCount = 4;

    VkQueryPool pool;
    vkCreateQueryPool(device, &poolInfo, NULL, &pool);

    int queryIdx = 0;

    while(Running())
    {
      VkImage preinitImg = VK_NULL_HANDLE;
      VkDeviceMemory preinitMem = VK_NULL_HANDLE;

      vkCreateImage(device, preinitInfo, NULL, &preinitImg);

      setName(preinitImg, "Image:Preinitialised");

      AllocatedImage undefImg(
          this,
          vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      setName(undefImg.image, "Image:Undefined");

      {
        VkMemoryRequirements mrq;
        vkGetImageMemoryRequirements(device, preinitImg, &mrq);

        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = mrq.size;
        info.memoryTypeIndex = 100;

        for(uint32_t i = 0; i < props->memoryTypeCount; i++)
        {
          if(mrq.memoryTypeBits & (1 << i) &&
             (props->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
          {
            info.memoryTypeIndex = i;
            break;
          }
        }

        TEST_ASSERT(info.memoryTypeIndex != 100, "Couldn't find compatible memory type");

        vkAllocateMemory(device, &info, NULL, &preinitMem);
        vkBindImageMemory(device, preinitImg, preinitMem, 0);

        void *data = NULL;
        vkMapMemory(device, preinitMem, 0, mrq.size, 0, &data);
        memset(data, 0x40, (size_t)mrq.size);
        vkUnmapMemory(device, preinitMem);
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkCmdResetQueryPool(cmd, pool, queryIdx % 4, 1);

      vkCmdWriteTimestamp2KHR(cmd, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR, pool, queryIdx % 4);

      queryIdx++;

      VkImage swapimg = mainWindow->GetImage();
      if((size_t)curFrame <= mainWindow->GetCount())
        setName(swapimg, "Image:Swapchain");

      setMarker(cmd, "Before Transition");

      VkDependencyInfoKHR dependency = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};

      VkBufferMemoryBarrier2KHR bufBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR};
      bufBarrier.buffer = vb.buffer;
      bufBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
      bufBarrier.dstAccessMask =
          VK_ACCESS_2_TRANSFER_READ_BIT_KHR | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
      bufBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
      bufBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR |
                                VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR |
                                VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
      bufBarrier.size = VK_WHOLE_SIZE;

      dependency.bufferMemoryBarrierCount = 1;
      dependency.pBufferMemoryBarriers = &bufBarrier;

      VkImageMemoryBarrier2KHR imgBarrier[2] = {};
      imgBarrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
      imgBarrier[0].subresourceRange = vkh::ImageSubresourceRange();
      imgBarrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
      imgBarrier[1].subresourceRange = vkh::ImageSubresourceRange();

      imgBarrier[0].srcAccessMask = VK_ACCESS_2_NONE_KHR;
      imgBarrier[0].srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
      imgBarrier[0].dstAccessMask =
          VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
      imgBarrier[0].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR |
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
      imgBarrier[0].oldLayout = (size_t)curFrame <= mainWindow->GetCount()
                                    ? VK_IMAGE_LAYOUT_UNDEFINED
                                    : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      imgBarrier[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
      imgBarrier[0].image = swapimg;

      dependency.imageMemoryBarrierCount = 1;
      dependency.pImageMemoryBarriers = imgBarrier;

      vkCmdPipelineBarrier2KHR(cmd, &dependency);

      // the manual images are transitioned into general for copying, from pre-initialised and
      // undefined
      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                           VK_IMAGE_LAYOUT_PREINITIALIZED,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, preinitImg),
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, undefImg.image),
               });

      // do two barriers that don't do anything useful but define no layout transition and don't
      // discard
      imgBarrier[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
      imgBarrier[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
      imgBarrier[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR;
      imgBarrier[0].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
      imgBarrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imgBarrier[0].newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imgBarrier[0].image = swapimg;

      imgBarrier[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
      imgBarrier[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
      imgBarrier[1].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR;
      imgBarrier[1].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
      imgBarrier[1].oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
      imgBarrier[1].newLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
      imgBarrier[1].image = preinitImg;

      dependency.bufferMemoryBarrierCount = 0;

      vkCmdResetEvent2KHR(cmd, ev, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdSetEvent2KHR(cmd, ev, &dependency);

      VkImageCopy region = {
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          {0, 0, 0},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          {0, 0, 0},
          {4, 4, 1},
      };

      vkCmdCopyImage(cmd, preinitImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, undefImg.image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      vkCmdWaitEvents2KHR(cmd, 1, &ev, &dependency);

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();

      vkDeviceWaitIdle(device);

      vkDestroyImage(device, preinitImg, NULL);
      vkFreeMemory(device, preinitMem, NULL);

      undefImg.free();
    }

    vkDestroyQueryPool(device, pool, NULL);
    vkDestroyEvent(device, ev, NULL);

    return 0;
  }
};

REGISTER_TEST();
