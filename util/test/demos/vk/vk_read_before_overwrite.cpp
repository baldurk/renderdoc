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

RD_TEST(VK_Read_Before_Overwrite, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Reads from a buffer and image before completely overwriting them, both in the same command "
      "buffer and in the next one. Tests that contents are properly preserved for first use in "
      "shaders even if they are later discarded.";

  const std::string comp = R"EOSHADER(

#version 450 core

#extension GL_EXT_samplerless_texture_functions : require

layout(binding = 0, std430) buffer storebuftype {
  vec4 data;
} storebuf;

layout(binding = 1, std140) uniform ubotype
{
  vec4 data;
} ubo;

layout(binding = 2) uniform texture2D sampledImage;
layout(binding = 3, rgba32f) uniform coherent image2D storeImage;

struct DefaultA2V
{
  // unrolled to hack alignment
  float pos_x;
  float pos_y;
  float pos_z;
  float col_r;
  float col_g;
  float col_b;
  float col_a;
  float uv_x;
  float uv_y;
};

layout(binding = 4, std430) buffer outbuftype {
  DefaultA2V data[];
} outbuf;

const DefaultA2V DefaultTri[3] = {
    {-0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f,   0.0f, 0.0f},
    { 0.0f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f,   0.0f, 1.0f},
    { 0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f, 1.0f,   1.0f, 0.0f},
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
  vec4 samp = texelFetch(sampledImage, ivec2(0,0), 0);
  vec4 stor = imageLoad(storeImage, ivec2(0,0));

  for(int i=0; i < 3; i++)
  {
    outbuf.data[i].pos_x = DefaultTri[i].pos_x * ubo.data.x;
    outbuf.data[i].pos_y = DefaultTri[i].pos_y * ubo.data.y;
    outbuf.data[i].pos_z = DefaultTri[i].pos_z * storebuf.data.x;
    outbuf.data[i].col_r = DefaultTri[i].col_r * storebuf.data.y;
    outbuf.data[i].col_g = DefaultTri[i].col_g * samp.x;
    outbuf.data[i].col_b = DefaultTri[i].col_b * samp.y;
    outbuf.data[i].col_a = DefaultTri[i].col_a * stor.x;
  }
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout compSetLayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        }));

    VkPipelineLayout compLayout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo({compSetLayout}));

    VkPipeline compPipe = createComputePipeline(vkh::ComputePipelineCreateInfo(
        compLayout, CompileShaderModule(comp, ShaderLang::glsl, ShaderStage::comp)));

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

    pipeCreateInfo.colorBlendState.attachments[0].blendEnable = VK_TRUE;

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    // [0] = cleared in same cmd buf, [1] = cleared in second cmd buf
    AllocatedBuffer ubo[2];
    AllocatedBuffer ssbo[2];
    AllocatedImage sampled[2];
    VkImageView sampledView[2];
    AllocatedImage storeIm[2];
    VkImageView storeView[2];
    VkDescriptorSet sets[2];

    AllocatedBuffer outbuf(this,
                           vkh::BufferCreateInfo(2048, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(outbuf.buffer, "outbuf");

    for(int i = 0; i < 2; i++)
    {
      ssbo[i] = AllocatedBuffer(this,
                                vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
      ubo[i] = AllocatedBuffer(this,
                               vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                               VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      sampled[i] = AllocatedImage(
          this,
          vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      sampledView[i] = createImageView(vkh::ImageViewCreateInfo(
          sampled[i].image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

      storeIm[i] = AllocatedImage(
          this,
          vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      storeView[i] = createImageView(vkh::ImageViewCreateInfo(
          storeIm[i].image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

      sets[i] = allocateDescriptorSet(compSetLayout);

      // each set outputs to a different location
      vkh::updateDescriptorSets(
          device,
          {
              vkh::WriteDescriptorSet(sets[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      {vkh::DescriptorBufferInfo(ssbo[i].buffer)}),
              vkh::WriteDescriptorSet(sets[i], 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      {vkh::DescriptorBufferInfo(ubo[i].buffer)}),
              vkh::WriteDescriptorSet(sets[i], 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                      {vkh::DescriptorImageInfo(
                                          sampledView[i], VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
              vkh::WriteDescriptorSet(
                  sets[i], 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                  {vkh::DescriptorImageInfo(storeView[i], VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
              vkh::WriteDescriptorSet(sets[i], 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      {vkh::DescriptorBufferInfo(outbuf.buffer, i * 1024)}),
          });
    }

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                           sampled[0].image),
                   vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                           sampled[1].image),
                   vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                           storeIm[0].image),
                   vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                           storeIm[1].image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compPipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compLayout, 0, {sets[0]}, {});
      vkCmdDispatch(cmd, 1, 1, 1);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compLayout, 0, {sets[1]}, {});
      vkCmdDispatch(cmd, 1, 1, 1);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                      VK_IMAGE_LAYOUT_GENERAL, sampled[0].image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                      VK_IMAGE_LAYOUT_GENERAL, storeIm[0].image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, ubo[0].buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, ssbo[0].buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, outbuf.buffer),
          });

      vkCmdClearColorImage(cmd, sampled[0].image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdClearColorImage(cmd, storeIm[0].image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdFillBuffer(cmd, ubo[0].buffer, 0, 1024, 0);
      vkCmdFillBuffer(cmd, ssbo[0].buffer, 0, 1024, 0);

      vkEndCommandBuffer(cmd);

      Submit(0, 3, {cmd});

      cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                      VK_IMAGE_LAYOUT_GENERAL, sampled[1].image),
              vkh::ImageMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                      VK_IMAGE_LAYOUT_GENERAL, storeIm[1].image),
          },
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, ubo[1].buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, ssbo[1].buffer),
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, outbuf.buffer),
          });

      vkCmdClearColorImage(cmd, sampled[1].image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdClearColorImage(cmd, storeIm[1].image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdFillBuffer(cmd, ubo[1].buffer, 0, 1024, 0);
      vkCmdFillBuffer(cmd, ssbo[1].buffer, 0, 1024, 0);

      vkEndCommandBuffer(cmd);

      Submit(1, 3, {cmd});

      cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      VkViewport v = mainWindow->viewport;
      v.width /= 2.0f;
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {outbuf.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      v.x += v.width;
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkh::cmdBindVertexBuffers(cmd, 0, {outbuf.buffer}, {1024});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      setMarker(cmd, "checkpoint");

      vkCmdEndRenderPass(cmd);

      // set data for next iteration
      for(int i = 0; i < 2; i++)
      {
        vkh::cmdPipelineBarrier(
            cmd,
            {
                vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                        sampled[i].image),
                vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                        storeIm[i].image),
            },
            {
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         ubo[i].buffer),
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         ssbo[i].buffer),
            });

        vkCmdClearColorImage(cmd, sampled[i].image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f), 1,
                             vkh::ImageSubresourceRange());
        vkCmdClearColorImage(cmd, storeIm[i].image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f), 1,
                             vkh::ImageSubresourceRange());
        vkCmdFillBuffer(cmd, ubo[i].buffer, 0, 1024, 0x3f800000);
        vkCmdFillBuffer(cmd, ssbo[i].buffer, 0, 1024, 0x3f800000);
      }

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(2, 3, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
