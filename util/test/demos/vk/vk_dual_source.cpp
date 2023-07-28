/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

static std::string common = R"EOSHADER(

#version 460 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

)EOSHADER";

const std::string pixel = common + R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 outColor0;
layout(location = 0, index = 1) out vec4 outColor1;

void main() {
    vec3 fragColor = vertIn.col.rgb;

    outColor0 = vec4(fragColor, 1.0);
    outColor1 = vec4(fragColor.brg, 1.0);
}
)EOSHADER";

// This shader is equivalent to the other one, just with a much more complicated-looking layout
const std::string pixel_complicated = common + R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0, component = 0) out float outColor0_r;
layout(location = 0, /* index = 0, */ component = 1) out float outColor0_g;
layout(location = 0, /* index = 0, */ component = 2) out vec2 outColor0_ba;
layout(location = 0, index = 1/* , component = 0 */) out float outColor1_r;
layout(location = 0, index = 1, component = 1) out vec2 outColor1_gb;
layout(location = 0, index = 1, component = 3) out float outColor1_a;
// Test so that we have an output builtin. depth_unchanged means nothing interesting
// actually happens (not sure why it exists, but it's nice here)
layout(depth_unchanged) out float gl_FragDepth;

void main() {
    vec4 outColor0, outColor1;

    vec3 fragColor = vertIn.col.rgb;

    outColor0 = vec4(fragColor, 1.0);
    outColor1 = vec4(fragColor.brg, 1.0);

    outColor0_r = outColor0.r;
    outColor0_g = outColor0.g;
    outColor0_ba = outColor0.ba;
    outColor1_r = outColor1.r;
    outColor1_gb = outColor1.gb;
    outColor1_a = outColor1.a;
    gl_FragDepth = gl_FragCoord.z;
}
)EOSHADER";

RD_TEST(VK_Dual_Source, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Draws a pair of triangles using dual source blending";

  const DefaultA2V Triangles[12] = {
      // Two partially overlapping triangles
      {Vec3f(+0.0f, -0.5f, 0.0f), Vec4f(0.5f, 0.0f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(+0.5f, +0.5f, 0.0f), Vec4f(0.5f, 0.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(-0.5f, +0.5f, 0.0f), Vec4f(0.5f, 0.0f, 0.0f, 0.0f), Vec2f(1.0f, 0.0f)},
      {Vec3f(-0.25f, -0.5f, 0.0f), Vec4f(0.5f, 0.0f, 0.5f, 0.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(+0.75f, -0.5f, 0.0f), Vec4f(0.5f, 0.0f, 0.5f, 0.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(+0.25f, +0.5f, 0.0f), Vec4f(0.5f, 0.0f, 0.5f, 0.0f), Vec2f(1.0f, 0.0f)},
  };

  void Prepare(int argc, char **argv)
  {
    features.dualSrcBlend = VK_TRUE;
    VulkanGraphicsTest::Prepare(argc, argv);
  }

  void clear(VkCommandBuffer cmd, VkImage swapimg, const AllocatedImage &offimg,
             const AllocatedImage &offimgMS, const VkClearColorValue *pColor)
  {
    pushMarker(cmd, "Clear");
    vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL, pColor, 1,
                         vkh::ImageSubresourceRange());

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL, offimg.image),
             });

    vkCmdClearColorImage(cmd, offimg.image, VK_IMAGE_LAYOUT_GENERAL, pColor, 1,
                         vkh::ImageSubresourceRange());

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL, offimgMS.image),
             });

    vkCmdClearColorImage(cmd, offimgMS.image, VK_IMAGE_LAYOUT_GENERAL, pColor, 1,
                         vkh::ImageSubresourceRange());
    popMarker(cmd);
  }

  int main()
  {
    // initialise, create window, create context, etc
    requestedSwapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    pipeCreateInfo.colorBlendState.attachments = {colorBlendAttachment};

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[1] =
        CompileShaderModule(pixel_complicated, ShaderLang::glsl, ShaderStage::frag, "main");

    VkPipeline complicatedPipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this, vkh::BufferCreateInfo(sizeof(Triangles), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(Triangles);

    AllocatedImage offimg(this, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedImage offimgMS(
        this, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R16G16B16A16_SFLOAT,
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT, 1, 1, VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      clear(cmd, swapimg, offimg, offimgMS, vkh::ClearColorValue(0.0f, 1.0f, 1.0f, 1.0f));

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      setMarker(cmd, "Begin test A1");
      vkCmdDraw(cmd, 6, 1, 0, 0);
      setMarker(cmd, "End test A1");

      vkCmdEndRenderPass(cmd);

      clear(cmd, swapimg, offimg, offimgMS, vkh::ClearColorValue(0.0f, 1.0f, 1.0f, 1.0f));

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);
      setMarker(cmd, "Begin test A2");
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdDraw(cmd, 3, 1, 3, 0);
      setMarker(cmd, "End test A2");
      vkCmdEndRenderPass(cmd);

      clear(cmd, swapimg, offimg, offimgMS, vkh::ClearColorValue(0.0f, 1.0f, 1.0f, 1.0f));

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, complicatedPipe);
      setMarker(cmd, "Begin test B1");
      vkCmdDraw(cmd, 6, 1, 0, 0);
      setMarker(cmd, "End test B1");
      vkCmdEndRenderPass(cmd);

      clear(cmd, swapimg, offimg, offimgMS, vkh::ClearColorValue(0.0f, 1.0f, 1.0f, 1.0f));

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);
      setMarker(cmd, "Begin test B2");
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdDraw(cmd, 3, 1, 3, 0);
      setMarker(cmd, "End test B2");
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
