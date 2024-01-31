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

RD_TEST(VK_Compute_Only, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test that uses a compute only queue with no graphics queue.";

  const std::string comp = R"EOSHADER(

#version 450 core

layout(push_constant) uniform PushData
{
  uvec4 data;
} push;

layout(binding = 0, std430) buffer inbuftype {
  uvec4 data[];
} inbuf;

layout(binding = 1, std430) buffer outbuftype {
  uvec4 data[];
} outbuf;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
  outbuf.data[0].x += inbuf.data[0].x * push.data.x;
  outbuf.data[0].y += inbuf.data[0].y * push.data.y;
  outbuf.data[0].z += inbuf.data[0].z * push.data.z;
  outbuf.data[0].w += inbuf.data[0].w * push.data.w;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    headless = true;
    queueFlagsRequired = VK_QUEUE_COMPUTE_BIT;
    queueFlagsBanned = VK_QUEUE_GRAPHICS_BIT;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setLayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setLayout}, {
                         vkh::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Vec4i)),
                     }));

    VkPipeline pipe = createComputePipeline(vkh::ComputePipelineCreateInfo(
        layout, CompileShaderModule(comp, ShaderLang::glsl, ShaderStage::comp, "main")));

    AllocatedImage tex(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(tex.image, "tex");

    VkImageView view = createImageView(
        vkh::ImageViewCreateInfo(tex.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedBuffer bufin(this,
                          vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    AllocatedBuffer bufout(this,
                           vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    setName(bufin.buffer, "bufin");
    setName(bufout.buffer, "bufout");

    VkDescriptorSet set = allocateDescriptorSet(setLayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(bufin.buffer)}),
                    vkh::WriteDescriptorSet(set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(bufout.buffer)}),
                    vkh::WriteDescriptorSet(
                        set, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        {vkh::DescriptorImageInfo(view, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
                });

    // clear the buffers
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkCmdFillBuffer(cmd, bufin.buffer, 0, 1024, 111);
      vkCmdFillBuffer(cmd, bufout.buffer, 0, 1024, 222);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, tex.image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                       bufin.buffer, 0, 1024),
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                       bufout.buffer, 0, 1024),
          });

      vkCmdClearColorImage(cmd, tex.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.25f, 0.5f, 0.75f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});
    }

    if(rdoc)
      rdoc->StartFrameCapture(NULL, NULL);

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, {set}, {});

      setMarker(cmd, "Pre-Dispatch");

      Vec4i push = {5, 6, 7, 8};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Vec4i), &push);
      vkCmdDispatch(cmd, 1, 1, 1);

      setMarker(cmd, "Post-Dispatch");

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});
    }

    if(rdoc)
      rdoc->EndFrameCapture(NULL, NULL);

    msleep(1000);

    return 0;
  }
};

REGISTER_TEST();
