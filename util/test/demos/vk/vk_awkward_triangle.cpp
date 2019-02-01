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

struct Awkward_Triangle : VulkanGraphicsTest
{
  static constexpr const char *Description =
      "Draws a triangle but using vertex buffers in formats that only support VBs and not "
      "any other type of buffer use (i.e. requiring manual decode)";

  struct vertin
  {
    int16_t pos[3];
    uint16_t col[4];
    double uv[3];
  };

  std::string common = R"EOSHADER(

#version 420 core

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
layout(location = 2) in dvec3 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xyz, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col/65535.0f;
}

)EOSHADER";

  int main(int argc, char **argv)
  {
    features.shaderFloat64 = true;

    // initialise, create window, create context, etc
    if(!Init(argc, argv))
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = swapRenderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, vertin)};

    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttrFormatted(0, 0, vertin, pos, VK_FORMAT_R16G16B16_SNORM),
        vkh::vertexAttrFormatted(1, 0, vertin, col, VK_FORMAT_R16G16B16A16_USCALED),
        vkh::vertexAttrFormatted(2, 0, vertin, uv, VK_FORMAT_R64G64B64_SFLOAT),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    vertin triangle[] = {
        {
            {-16000, 16000, 0},
            {51515, 2945, 5893, 492},
            {8.2645198430, 1.8813003880, -3.96710837683597},
        },
        {
            {0, -16000, 0}, {1786, 32356, 8394, 1835}, {1.646793901, 6.86148531, -1.19476386246190},
        },
        {
            {16000, 16000, 0}, {8523, 9924, 49512, 3942}, {5.206423972, 9.58934003, -5.408522446462},
        },
    };

    AllocatedBuffer vb(allocator,
                       vkh::BufferCreateInfo(sizeof(triangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(triangle);

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
          cmd, vkh::RenderPassBeginInfo(swapRenderPass, swapFramebuffers[swapIndex], scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
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

REGISTER_TEST(Awkward_Triangle);