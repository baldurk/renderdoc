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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "3rdparty/fmt/core.h"
#include "vk_test.h"

RD_TEST(VK_Descriptor_Reuse, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Allocates and reuses a large number of descriptors to stress re-allocation.";

  std::string pixel = R"EOSHADER(

#version 460 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 10, std140) uniform constsbuf1
{
  vec4 val1;
} cbuf1;

layout(set = 0, binding = 11, std140) uniform constsbuf2
{
  vec4 val2;
} cbuf2;

layout(set = 0, binding = 3) uniform sampler2D samp1;
layout(set = 0, binding = 4) uniform sampler2D samp2;
layout(set = 0, binding = 5) uniform sampler2D samp3;

void main()
{
	Color = (vertIn.col * 0.4f) +
          cbuf1.val1 + cbuf2.val2 +
          texture(samp1, vec2(0)) + texture(samp2, vec2(0)) + texture(samp3, vec2(0));
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const size_t numBufs = 1024;
    const size_t numImages = 1024;
    const size_t descriptorCount = 512;
    const size_t setLayoutCount = 64;
    const size_t ringSize = 3;
    const size_t threadCount = 8;

    std::vector<VkDescriptorSetLayout> setlayout;

    for(size_t i = 0; i < setLayoutCount; i++)
      setlayout.push_back(createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
          {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      })));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout[0]}));

    AllocatedImage img[threadCount];

    for(size_t i = 0; i < threadCount; i++)
    {
      img[i] = AllocatedImage(
          this,
          vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height,
                               0, VK_FORMAT_R32G32B32A32_SFLOAT,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
      setName(img[i].image, fmt::format("Offscreen{}", i));
    }

    VkImageView imgview[threadCount];
    for(size_t i = 0; i < threadCount; i++)
      imgview[i] = createImageView(vkh::ImageViewCreateInfo(img[i].image, VK_IMAGE_VIEW_TYPE_2D,
                                                            VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer[threadCount];
    for(size_t i = 0; i < threadCount; i++)
      framebuffer[i] = createFramebuffer(
          vkh::FramebufferCreateInfo(renderPass, {imgview[i]}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    Vec4f val1(0.4f, 0.0f, 0.0f, 0.0f);
    Vec4f val2(0.0f, 0.0f, 0.4f, 0.0f);

    std::vector<AllocatedBuffer> val1bufs;
    std::vector<AllocatedBuffer> val2bufs;

    for(size_t i = 0; i < numBufs; i++)
    {
      val1bufs.push_back(AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(
              sizeof(Vec4f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU})));
      val2bufs.push_back(AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(
              sizeof(Vec4f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU})));

      val1bufs.back().upload(&val1, sizeof(Vec4f));
      val2bufs.back().upload(&val2, sizeof(Vec4f));
    }

    std::vector<AllocatedImage> samps1;
    std::vector<AllocatedImage> samps2;
    std::vector<AllocatedImage> samps3;

    std::vector<VkImageView> views1;
    std::vector<VkImageView> views2;
    std::vector<VkImageView> views3;

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      for(size_t i = 0; i < numImages; i++)
      {
        samps1.push_back(AllocatedImage(
            this,
            vkh::ImageCreateInfo(16, 16, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 3),
            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY})));
        samps2.push_back(AllocatedImage(
            this,
            vkh::ImageCreateInfo(16, 16, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 3),
            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY})));
        samps3.push_back(AllocatedImage(
            this,
            vkh::ImageCreateInfo(16, 16, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 3),
            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY})));

        views1.push_back(createImageView(vkh::ImageViewCreateInfo(
            samps1.back().image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, {},
            vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, rand() % 2))));
        views2.push_back(createImageView(vkh::ImageViewCreateInfo(
            samps2.back().image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, {},
            vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, rand() % 2))));
        views3.push_back(createImageView(vkh::ImageViewCreateInfo(
            samps3.back().image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, {},
            vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, rand() % 2))));

        vkh::cmdPipelineBarrier(
            cmd,
            {
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL, samps1.back().image),
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL, samps2.back().image),
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL, samps3.back().image),
            });

        vkCmdClearColorImage(cmd, samps1.back().image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.4f, 0.0f, 0.0f, 0.0f), 1,
                             vkh::ImageSubresourceRange());
        vkCmdClearColorImage(cmd, samps2.back().image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.0f, 0.4f, 0.0f, 0.0f), 1,
                             vkh::ImageSubresourceRange());
        vkCmdClearColorImage(cmd, samps3.back().image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.0f, 0.0f, 0.4f, 0.0f), 1,
                             vkh::ImageSubresourceRange());
      }

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

    VkSampler sampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));

    VkFence ringComplete[ringSize];
    for(size_t r = 0; r < ringSize; r++)
      CHECK_VKR(vkCreateFence(device, vkh::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT), NULL,
                              &ringComplete[r]));

    struct ThreadData
    {
      VkCommandPool cmdPool;
      VkDescriptorPool descPools[ringSize];
      VkCommandBuffer cmdBufs[ringSize];

      std::mutex lock;
      std::condition_variable cv;
      std::atomic_bool kill, run;
    };

    ThreadData threadData[threadCount];
    std::atomic_int threadsDone;
    std::mutex doneLock;
    std::condition_variable doneCV;

    threadsDone = 0;

    for(size_t t = 0; t < threadCount; t++)
    {
      CHECK_VKR(vkCreateCommandPool(
          device, vkh::CommandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT), NULL,
          &threadData[t].cmdPool));

      threadData[t].kill = threadData[t].run = false;

      for(size_t r = 0; r < ringSize; r++)
      {
        CHECK_VKR(vkCreateDescriptorPool(
            device,
            vkh::DescriptorPoolCreateInfo(
                descriptorCount,
                {
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * 3},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount * 2},
                }),
            NULL, &threadData[t].descPools[r]));
      }

      CHECK_VKR(vkAllocateCommandBuffers(
          device, vkh::CommandBufferAllocateInfo(threadData[t].cmdPool, ringSize),
          threadData[t].cmdBufs));
    }

    size_t ringIndex = 0;

    std::vector<std::thread> threads(threadCount);

    auto threadFunc = [&](size_t threadIndex) {
      // pre-bake descriptor allocate/update infos, that we just patch and use. Saves on overhead of
      // temporary std::vector work that is usually worth it for convenience

      VkDescriptorBufferInfo bufs[2] = {
          vkh::DescriptorBufferInfo(VK_NULL_HANDLE),
          vkh::DescriptorBufferInfo(VK_NULL_HANDLE),
      };

      VkDescriptorImageInfo imInfo[3] = {
          vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, sampler),
          vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, sampler),
          vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, sampler),
      };

      VkWriteDescriptorSet writes[5] = {
          vkh::WriteDescriptorSet(VK_NULL_HANDLE, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {bufs[0]}),
          vkh::WriteDescriptorSet(VK_NULL_HANDLE, 11, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {bufs[1]}),
          vkh::WriteDescriptorSet(VK_NULL_HANDLE, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  {imInfo[0]}),
          vkh::WriteDescriptorSet(VK_NULL_HANDLE, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  {imInfo[1]}),
          vkh::WriteDescriptorSet(VK_NULL_HANDLE, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  {imInfo[2]}),
      };

      writes[0].pBufferInfo = &bufs[0];
      writes[1].pBufferInfo = &bufs[1];
      writes[2].pImageInfo = &imInfo[0];
      writes[3].pImageInfo = &imInfo[1];
      writes[4].pImageInfo = &imInfo[2];

      VkDescriptorSetAllocateInfo info =
          vkh::DescriptorSetAllocateInfo(VK_NULL_HANDLE, {setlayout[0]});

      while(!threadData[threadIndex].kill)
      {
        {
          std::unique_lock<std::mutex> scoped(threadData[threadIndex].lock);
          while(!threadData[threadIndex].kill && !threadData[threadIndex].run)
            threadData[threadIndex].cv.wait(scoped);
          threadData[threadIndex].run = false;
          if(threadData[threadIndex].kill)
            break;
        }

        VkCommandBuffer cmd = threadData[threadIndex].cmdBufs[ringIndex];
        VkDescriptorPool descPool = threadData[threadIndex].descPools[ringIndex];

        info.descriptorPool = descPool;

        vkResetDescriptorPool(device, descPool, 0);
        vkResetCommandBuffer(cmd, 0);

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        vkCmdBeginRenderPass(
            cmd,
            vkh::RenderPassBeginInfo(renderPass, framebuffer[threadIndex], mainWindow->scissor,
                                     {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)}),
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

        VkRect2D s = {{0, 0},
                      {uint32_t(screenWidth / (int)sqrtf((float)descriptorCount)),
                       uint32_t(screenHeight / (int)sqrtf((float)descriptorCount))}};
        VkViewport v = {0, 0, (float)s.extent.width, (float)s.extent.height, 0, 1};

        size_t randSeed = curFrame * threadIndex + threadIndex;

        for(size_t i = 0; i < descriptorCount; i++)
        {
          randSeed = (1103515245 * randSeed + 12345) % 0x7fffffff;
          info.pSetLayouts = &setlayout[randSeed % setlayout.size()];

          VkDescriptorSet set;
          CHECK_VKR(vkAllocateDescriptorSets(device, &info, &set));

          for(VkWriteDescriptorSet &write : writes)
            write.dstSet = set;

          randSeed = (1103515245 * randSeed + 12345) % 0x7fffffff;
          bufs[0].buffer = val1bufs[randSeed % val1bufs.size()].buffer;
          randSeed = (1103515245 * randSeed + 12345) % 0x7fffffff;
          bufs[1].buffer = val2bufs[randSeed % val2bufs.size()].buffer;

          randSeed = (1103515245 * randSeed + 12345) % 0x7fffffff;
          imInfo[0].imageView = views1[randSeed % views1.size()];
          randSeed = (1103515245 * randSeed + 12345) % 0x7fffffff;
          imInfo[1].imageView = views2[randSeed % views2.size()];
          randSeed = (1103515245 * randSeed + 12345) % 0x7fffffff;
          imInfo[2].imageView = views3[randSeed % views3.size()];

          vkUpdateDescriptorSets(device, (uint32_t)ARRAY_COUNT(writes), writes, 0, NULL);

          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, NULL);
          vkCmdSetViewport(cmd, 0, 1, &v);
          vkCmdSetScissor(cmd, 0, 1, &s);
          vkCmdDraw(cmd, 3, 1, 0, 0);

          v.x += v.width;
          s.offset.x += s.extent.width;
          if(v.x >= screenWidth)
          {
            v.x = 0;
            s.offset.x = 0;
            v.y += v.height;
            s.offset.y += s.extent.height;
          }
        }

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        {
          std::unique_lock<std::mutex> scoped(doneLock);
          threadsDone++;
          doneCV.notify_one();
        }
      }
    };

    for(size_t i = 0; i < threads.size(); i++)
      threads[i] = std::thread(threadFunc, i);

    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::time_point<Clock> Time;

    Time prev = Clock::now();

    double totalMS = 0;
    uint32_t frames = 0;

    double minframetimeMS = 100000.0;

    while(Running())
    {
      // wait for the previous version of this ring to complete. We expect this to be done.
      vkWaitForFences(device, 1, &ringComplete[ringIndex], VK_TRUE, 1000000);

      // reset it so we can use it in the next submit
      vkResetFences(device, 1, &ringComplete[ringIndex]);

      for(size_t i = 0; i < threads.size(); i++)
      {
        std::unique_lock<std::mutex> scoped(threadData[i].lock);
        threadData[i].run = true;
        threadData[i].cv.notify_one();
      }

      {
        std::unique_lock<std::mutex> scoped(doneLock);
        while(threadsDone < (int)threadCount)
          doneCV.wait(scoped);
        threadsDone = 0;
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      std::vector<VkCommandBuffer> cmds;
      for(size_t i = 0; i < threadCount; i++)
        cmds.push_back(threadData[i].cmdBufs[ringIndex]);

      VkSubmitInfo submit = vkh::SubmitInfo(cmds);
      CHECK_VKR(vkQueueSubmit(queue, 1, &submit, ringComplete[ringIndex]));
      Submit(0, 1, {cmd});

      ringIndex = (ringIndex + 1) % ringSize;

      Time cur = Clock::now();
      double frametimeMS =
          double(std::chrono::duration_cast<std::chrono::microseconds>(cur - prev).count()) / 1000.0;
      prev = cur;

      if(curFrame > 1)
        minframetimeMS = std::min(minframetimeMS, frametimeMS);

      setMarker(queue, fmt::format("Min Duration = {}", minframetimeMS));

      totalMS += frametimeMS;
      frames++;

      if(totalMS > 1000.0)
      {
        TEST_LOG("%u frames in %f ms = %f average frametime", frames, totalMS, totalMS / frames);
        frames = 0;
        totalMS = 0.0;
      }

      Present();
    }

    for(size_t i = 0; i < threads.size(); i++)
    {
      std::unique_lock<std::mutex> scoped(threadData[i].lock);

      threadData[i].kill = true;
      threadData[i].cv.notify_one();
    }

    for(size_t i = 0; i < threads.size(); i++)
      threads[i].join();

    for(size_t r = 0; r < ringSize; r++)
      vkDestroyFence(device, ringComplete[r], NULL);

    for(size_t t = 0; t < threadCount; t++)
    {
      vkDestroyCommandPool(device, threadData[t].cmdPool, NULL);

      for(size_t r = 0; r < ringSize; r++)
        vkDestroyDescriptorPool(device, threadData[t].descPools[r], NULL);
    }

    return 0;
  }
};

REGISTER_TEST();
