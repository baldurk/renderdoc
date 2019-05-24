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

#include "vk_test.h"

TEST(VK_Resource_Lifetimes, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test various edge-case resource lifetimes: a resource that is first dirtied within a frame "
      "so needs initial contents created for it, and a resource that is created and destroyed "
      "mid-frame (which also gets dirtied after use).";

  std::string common = R"EOSHADER(

#version 420 core

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

)EOSHADER";

  const std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform sampler2D smiley;
layout(binding = 1) uniform sampler2D checker;

layout(binding = 2, std140) uniform constsbuf
{
  vec4 flags;
};

void main()
{
  if(flags.x != 1.0f || flags.y != 2.0f || flags.z != 4.0f || flags.w != 8.0f)
  {
    Color = vec4(1.0f, 0.0f, 1.0f, 1.0f);
    return;
  }

	Color = texture(smiley, vertIn.uv.xy * 2.0f) * texture(checker, vertIn.uv.xy * 5.0f);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    AllocatedImage smiley(
        allocator, vkh::ImageCreateInfo(rgba8.width, rgba8.height, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView smileyview = createImageView(
        vkh::ImageViewCreateInfo(smiley.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedImage badimg(allocator, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                              VK_IMAGE_USAGE_SAMPLED_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView badview = createImageView(
        vkh::ImageViewCreateInfo(badimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedBuffer uploadBuf(allocator, vkh::BufferCreateInfo(rgba8.data.size() * sizeof(uint32_t),
                                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                              VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    uploadBuf.upload(rgba8.data.data(), rgba8.data.size() * sizeof(uint32_t));

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, smiley.image),
               });

      VkBufferImageCopy copy = {};
      copy.imageExtent = {rgba8.width, rgba8.height, 1};
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.layerCount = 1;

      vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, smiley.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, smiley.image),
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, badimg.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});

      vkDeviceWaitIdle(device);
    }

    Vec4f flags = {};
    AllocatedBuffer badcb(allocator,
                          vkh::BufferCreateInfo(sizeof(flags), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    badcb.upload(&flags, sizeof(flags));

    VkSampler sampler = VK_NULL_HANDLE;

    VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(device, &sampInfo, NULL, &sampler);

    auto SetupBuffer = [this]() {
      VkBuffer cb = VK_NULL_HANDLE;

      vkCreateBuffer(device,
                     vkh::BufferCreateInfo(sizeof(Vec4f), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                     NULL, &cb);

      return cb;
    };

    const VkPhysicalDeviceMemoryProperties *props = NULL;
    vmaGetMemoryProperties(allocator, &props);

    auto SetupBufferMemory = [this, props](VkBuffer cb) {
      VkMemoryRequirements mrq;
      vkGetBufferMemoryRequirements(device, cb, &mrq);

      VkMemoryAllocateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.allocationSize = mrq.size;

      for(uint32_t i = 0; i < props->memoryTypeCount; i++)
      {
        if((mrq.memoryTypeBits & (1u << i)) &&
           (props->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        {
          info.memoryTypeIndex = i;
          break;
        }
      }

      VkDeviceMemory mem = VK_NULL_HANDLE;
      vkAllocateMemory(device, &info, NULL, &mem);
      vkBindBufferMemory(device, cb, mem, 0);

      Vec4f *data = NULL;
      vkMapMemory(device, mem, 0, sizeof(Vec4f), 0, (void **)&data);
      {
        *data = Vec4f(1.0f, 2.0f, 4.0f, 8.0f);
      }
      vkUnmapMemory(device, mem);

      return mem;
    };

    auto TrashBuffer = [this](VkBuffer cb, VkDeviceMemory mem) {
      Vec4f *data = NULL;
      vkMapMemory(device, mem, 0, sizeof(Vec4f), 0, (void **)&data);
      {
        *data = Vec4f();
      }
      vkUnmapMemory(device, mem);

      vkDestroyBuffer(device, cb, NULL);
      vkFreeMemory(device, mem, NULL);
    };

    auto SetupImage = [this]() {
      VkImage img = VK_NULL_HANDLE;

      vkCreateImage(
          device, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
          NULL, &img);

      return img;
    };

    auto SetupImageMemory = [this, props, &uploadBuf](VkImage img) {
      VkMemoryRequirements mrq;
      vkGetImageMemoryRequirements(device, img, &mrq);

      VkMemoryAllocateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.allocationSize = mrq.size;

      for(uint32_t i = 0; i < props->memoryTypeCount; i++)
      {
        if(mrq.memoryTypeBits & (1 << i))
        {
          info.memoryTypeIndex = i;
          break;
        }
      }

      VkDeviceMemory mem = VK_NULL_HANDLE;
      vkAllocateMemory(device, &info, NULL, &mem);
      vkBindImageMemory(device, img, mem, 0);

      const uint32_t checker[4 * 4] = {
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
      };

      uploadBuf.upload(checker);

      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        vkh::cmdPipelineBarrier(
            cmd,
            {
                vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img),
            });

        VkBufferImageCopy copy = {};
        copy.imageExtent = {4, 4, 1};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;

        vkCmdCopyBufferToImage(cmd, uploadBuf.buffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copy);

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, img),
                 });

        vkEndCommandBuffer(cmd);

        Submit(99, 99, {cmd});

        vkDeviceWaitIdle(device);
      }

      return mem;
    };

    auto SetupImageView = [this](VkImage img) {
      VkImageView ret;
      vkCreateImageView(
          device, vkh::ImageViewCreateInfo(img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM),
          NULL, &ret);
      return ret;
    };

    auto TrashImage = [this](VkImage img, VkDeviceMemory mem, VkImageView view) {

      vkDestroyImageView(device, view, NULL);
      vkDestroyImage(device, img, NULL);
      vkFreeMemory(device, mem, NULL);
    };

    VkDescriptorPool descpool = VK_NULL_HANDLE;

    {
      CHECK_VKR(vkCreateDescriptorPool(
          device, vkh::DescriptorPoolCreateInfo(8,
                                                {
                                                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
                                                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
                                                },
                                                VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT),
          NULL, &descpool));
    }

    auto SetupDescSet = [this, setlayout, descpool, sampler, smileyview](VkBuffer cb,
                                                                         VkImageView view) {
      VkDescriptorSet descset = VK_NULL_HANDLE;

      vkAllocateDescriptorSets(device, vkh::DescriptorSetAllocateInfo(descpool, {setlayout}),
                               &descset);

      vkh::updateDescriptorSets(
          device, {
                      vkh::WriteDescriptorSet(
                          descset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          {
                              vkh::DescriptorImageInfo(
                                  smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                          }),
                      vkh::WriteDescriptorSet(
                          descset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          {
                              vkh::DescriptorImageInfo(
                                  view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                          }),
                      vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              {vkh::DescriptorBufferInfo(cb)}),
                  });

      return descset;
    };

    auto TrashDescSet = [this, descpool, sampler, &badcb, badview](VkDescriptorSet descset) {

      // update with bad data
      vkh::updateDescriptorSets(
          device, {
                      vkh::WriteDescriptorSet(
                          descset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          {
                              vkh::DescriptorImageInfo(
                                  badview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                          }),
                      vkh::WriteDescriptorSet(
                          descset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          {
                              vkh::DescriptorImageInfo(
                                  badview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                          }),
                      vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                              {vkh::DescriptorBufferInfo(badcb.buffer)}),
                  });

      // assume we are able to re-use the same descriptor pool indefinitely since we only allocate
      // one set at a time.
      vkFreeDescriptorSets(device, descpool, 1, &descset);
    };

    VkBuffer cb = SetupBuffer();
    VkDeviceMemory cbmem = SetupBufferMemory(cb);
    VkImage img = SetupImage();
    VkDeviceMemory imgmem = SetupImageMemory(img);
    VkImageView imgview = SetupImageView(img);
    VkDescriptorSet descset = SetupDescSet(cb, imgview);
    while(Running())
    {
      // acquire and clear the backbuffer
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        vkEndCommandBuffer(cmd);

        Submit(0, 4, {cmd});
      }

      // render with last frame's resources then trash them
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset, 0,
                                NULL);
        VkViewport view = {0, 0, 128, 128, 0, 1};
        vkCmdSetViewport(cmd, 0, 1, &view);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        Submit(1, 4, {cmd});

        vkDeviceWaitIdle(device);

        TrashBuffer(cb, cbmem);
        TrashImage(img, imgmem, imgview);
        TrashDescSet(descset);
      }

      // create resources mid-frame and use then trash them
      {
        cb = SetupBuffer();
        cbmem = SetupBufferMemory(cb);
        img = SetupImage();
        imgmem = SetupImageMemory(img);
        imgview = SetupImageView(img);
        descset = SetupDescSet(cb, imgview);

        vkDeviceWaitIdle(device);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset, 0,
                                NULL);
        VkViewport view = {128, 0, 128, 128, 0, 1};
        vkCmdSetViewport(cmd, 0, 1, &view);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        Submit(2, 4, {cmd});

        vkDeviceWaitIdle(device);

        TrashBuffer(cb, cbmem);
        TrashImage(img, imgmem, imgview);
        TrashDescSet(descset);
      }

      // finish with the backbuffer
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkEndCommandBuffer(cmd);

        Submit(3, 4, {cmd});
      }

      // set up resources for next frame
      cb = SetupBuffer();
      cbmem = SetupBufferMemory(cb);
      img = SetupImage();
      imgmem = SetupImageMemory(img);
      imgview = SetupImageView(img);
      descset = SetupDescSet(cb, imgview);

      Present();
    }

    vkDeviceWaitIdle(device);

    // destroy resources
    TrashBuffer(cb, cbmem);
    TrashImage(img, imgmem, imgview);
    TrashDescSet(descset);

    vkDestroyDescriptorPool(device, descpool, NULL);
    vkDestroySampler(device, sampler, NULL);

    return 0;
  }
};

REGISTER_TEST();