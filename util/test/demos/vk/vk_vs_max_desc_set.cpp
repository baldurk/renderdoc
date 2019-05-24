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

TEST(VK_VS_Max_Desc_Set, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Uses the device's maximum number of descriptor sets in the vertex shader.";

  std::string common = R"EOSHADER(

#version 430 core

#extension GL_EXT_samplerless_texture_functions : require

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = get_color();
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  Color = vertIn.col;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPhysicalDeviceProperties props = vkh::getPhysicalDeviceProperties(phys);
    VkPhysicalDeviceLimits &limits = props.limits;

    // we use sampled images up to the last set, since some drivers support fewer UBOs per stage
    // than descriptor sets
    VkDescriptorSetLayout imgsetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        }));

    VkDescriptorSetLayout ubosetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        }));

    std::vector<VkDescriptorSetLayout> maxSetLayouts(limits.maxBoundDescriptorSets, imgsetlayout);

    maxSetLayouts.back() = ubosetlayout;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(maxSetLayouts));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    std::string dynamic_decl, dynamic_func;

    dynamic_decl = "\n";
    dynamic_func = "vec4 get_color() { return vec4(0)";

    uint32_t numimgs = limits.maxBoundDescriptorSets - 1;

    for(uint32_t i = 0; i < limits.maxBoundDescriptorSets; i++)
    {
      std::string istr = std::to_string(i);

      if(i < numimgs)
      {
        dynamic_decl +=
            "layout(set = " + istr + ", binding = 0) uniform texture2D tex" + istr + ";\n";
        dynamic_func +=
            " + texelFetch(tex" + istr + ", ivec2(0), 0) / vec4(" + std::to_string(numimgs) + ")";
      }
      else
      {
        dynamic_decl +=
            "layout(set = " + istr + ", binding = 0, std140) uniform constsbuf { vec4 col; };\n";
        dynamic_func += " + col";
      }
    }

    dynamic_decl += "\n";

    dynamic_func += "; }\n\n";

    pipeCreateInfo.stages = {
        CompileShaderModule(common + dynamic_decl + dynamic_func + vertex, ShaderLang::glsl,
                            ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    Vec4f cbufferdata = Vec4f(0.0f, 0.2f, 0.75f, 0.8f);

    AllocatedBuffer cb(
        allocator, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(&cbufferdata, sizeof(cbufferdata));

    AllocatedImage img(allocator, vkh::ImageCreateInfo(
                                      4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, img.image),
               });

      vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           vkh::ClearColorValue(1.0f, 0.0f, 0.0f, 0.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, img.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});

      vkDeviceWaitIdle(device);
    }

    VkDescriptorSet imgdescset = allocateDescriptorSet(imgsetlayout);
    VkDescriptorSet ubodescset = allocateDescriptorSet(ubosetlayout);

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(
                imgdescset, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                {
                    vkh::DescriptorImageInfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                }),
            vkh::WriteDescriptorSet(ubodescset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    {vkh::DescriptorBufferInfo(cb.buffer)}),
        });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor,
                                        {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)}),
          VK_SUBPASS_CONTENTS_INLINE);

      std::vector<VkDescriptorSet> descsets(limits.maxBoundDescriptorSets, imgdescset);
      descsets.back() = ubodescset;
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descsets, {});
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();