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

TEST(VK_Discard_Rectangles, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a large number of triangles using VK_EXT_discard_rectangles discard rectangles to "
      "either cut-out or filter for a series of rects";

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

void main()
{
	Color = vertIn.col;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_DISCARD_RECTANGLES_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPhysicalDeviceDiscardRectanglePropertiesEXT discardProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT};

    vkGetPhysicalDeviceProperties2KHR(phys, vkh::PhysicalDeviceProperties2KHR().next(&discardProps));

    const int32_t w = (int32_t)mainWindow->scissor.extent.width;
    const int32_t h = (int32_t)mainWindow->scissor.extent.height;

    VkRect2D discardRects[] = {
        // TL eye
        {{64, 64}, {64, 64}},
        // TR eye
        {{w - 64 * 2, 64}, {64, 64}},
        // nose
        {{w / 2 - 16, 128}, {32, 32}},
        // long mouth
        {{96, h - 48}, {(uint32_t)w - 96 * 2, 32}},
        // left mouth edge
        {{64, h - 48 - 32}, {32, 32}},
        // right mouth edge
        {{w - 96, h - 48 - 32}, {32, 32}},
    };

    TEST_ASSERT(discardProps.maxDiscardRectangles >= ARRAY_COUNT(discardRects),
                "not enough discard rectangles supported");

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

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

    pipeCreateInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT);

    VkPipelineDiscardRectangleStateCreateInfoEXT discardInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT,
    };
    discardInfo.discardRectangleMode = VK_DISCARD_RECTANGLE_MODE_INCLUSIVE_EXT;
    discardInfo.discardRectangleCount = ARRAY_COUNT(discardRects);

    pipeCreateInfo.pNext = &discardInfo;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    discardInfo.discardRectangleMode = VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT;

    VkPipeline pipe2 = createGraphicsPipeline(pipeCreateInfo);

    DefaultA2V trispam[3000];
    for(int i = 0; i < 3000; i++)
    {
      trispam[i].pos = Vec3f(RANDF(-1.0f, 1.0f), RANDF(-1.0f, 1.0f), RANDF(0.0f, 1.0f));
      trispam[i].col = Vec4f(RANDF(0.0f, 1.0f), RANDF(0.0f, 1.0f), RANDF(0.0f, 1.0f), 1.0f);
      trispam[i].uv = Vec2f(0.0f, 0.0f);
    }

    AllocatedBuffer vb(allocator,
                       vkh::BufferCreateInfo(sizeof(trispam), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(trispam);

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
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      VkViewport view = mainWindow->viewport;
      view.width /= 2.0f;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetDiscardRectangleEXT(cmd, 0, ARRAY_COUNT(discardRects), discardRects);
      vkCmdSetViewport(cmd, 0, 1, &view);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3000, 1, 0, 0);

      view.x += view.width;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe2);
      vkCmdSetViewport(cmd, 0, 1, &view);
      vkCmdDraw(cmd, 3000, 1, 0, 0);

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