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

RD_TEST(VK_Custom_Border_Color, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests the VK_EXT_custom_border_color extension.";

  const std::string vertex = R"EOSHADER(
#version 450 core

layout(location = 0) in vec3 Position;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec2 uv;

void main()
{
	gl_Position = vec4(Position.xyz*vec3(1,-1,1), 1);
	uv = UV;
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(
#version 450 core

layout(location = 0) in vec2 uv;

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main()
{
	Color = texture(tex, uv.xy);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceCustomBorderColorFeaturesEXT feat = {};
    feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
    feat.customBorderColors = VK_TRUE;

    devInfoNext = &feat;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    const DefaultA2V quad[4] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(-0.5f, -0.5f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.5f, -0.5f)},
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(-0.5f, 1.5f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.5f, 1.5f)},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(quad);

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedBuffer uploadBuf(
        this, vkh::BufferCreateInfo(4 * 4 * sizeof(Vec4f), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    Vec4f *f = (Vec4f *)uploadBuf.map();
    for(int i = 0; i < 4 * 4; i++)
      f[i] = Vec4f(float(i % 4) / 4.0f, float(i / 4) / 4.0f, 0.0f, 1.0f);
    uploadBuf.unmap();

    uploadBufferToImage(img.image, {4, 4, 1}, uploadBuf.buffer,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkImageView view = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    VkSampler blackbordersampler = createSampler(
        vkh::SamplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0.0f,
                               VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK));

    VkSamplerCustomBorderColorCreateInfoEXT custom = {};
    custom.sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;
    custom.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    custom.customBorderColor.float32[0] = 1.0f;
    custom.customBorderColor.float32[1] = 0.0f;
    custom.customBorderColor.float32[2] = 1.0f;
    custom.customBorderColor.float32[3] = 1.0f;

    VkSampler custombordersampler = createSampler(
        vkh::SamplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 0.0f,
                               VK_BORDER_COLOR_FLOAT_CUSTOM_EXT)
            .next(&custom));

    VkDescriptorSet descset0 = allocateDescriptorSet(setlayout);
    VkDescriptorSet descset1 = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(
                        descset0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        {
                            vkh::DescriptorImageInfo(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                     blackbordersampler),
                        }),
                    vkh::WriteDescriptorSet(
                        descset1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        {
                            vkh::DescriptorImageInfo(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                     custombordersampler),
                        }),
                });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      VkViewport v = mainWindow->viewport;

      v.width /= 2.0f;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset0}, {});
      vkCmdDraw(cmd, 4, 1, 0, 0);

      v.x += v.width;

      vkCmdSetViewport(cmd, 0, 1, &v);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset1}, {});
      vkCmdDraw(cmd, 4, 1, 0, 0);

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
