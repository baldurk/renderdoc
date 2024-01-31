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

RD_TEST(VK_Buffer_Truncation, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests using a uniform buffer that is truncated by the descriptor range, as well as "
      "vertex/index buffers truncated by size.";

  const std::string vertex = R"EOSHADER(
#version 460 core

layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec4 COLOR;

layout(location = 0) out vec4 OUTPOSITION;
layout(location = 1) out vec4 OUTCOLOR;

void main()
{
	gl_Position = OUTPOSITION = vec4(POSITION.xyz, 1);
	OUTCOLOR = COLOR;
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 0, std140) uniform constsbuf
{
  vec4 padding[16];
  vec4 outcol;
};

void main()
{
	Color = outcol;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    // we don't care what behaviour we get but we don't want to completely crash
    features.robustBufferAccess = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const DefaultA2V OffsetTri[] = {
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(9.9f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(8.8f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };
    uint16_t indices[] = {99, 99, 99, 1, 2, 3, 4, 5};
    Vec4f cbufferdata[64] = {};
    cbufferdata[32] = Vec4f(1.0f, 2.0f, 3.0f, 4.0f);

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

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
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(OffsetTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(OffsetTri);

    AllocatedBuffer ib(this,
                       vkh::BufferCreateInfo(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    ib.upload(indices);

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    cb.upload(cbufferdata);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(
                                                cb.buffer, sizeof(Vec4f) * 16, sizeof(Vec4f) * 16)}),
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

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkCmdBindIndexBuffer(cmd, ib.buffer, sizeof(uint16_t) * 3, VK_INDEX_TYPE_UINT16);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {sizeof(DefaultA2V) * 3});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

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
