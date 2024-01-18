/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

RD_TEST(VK_Multi_View, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Basic multi-view test like VK_Simple_Triangle but for multi-view rendering";

  const std::string common = R"EOSHADER(

#version 460 core

#extension GL_EXT_multiview : require

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

)EOSHADER";

  const std::string multiviewVertex = common + R"EOSHADER(

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

  if (gl_ViewIndex == 0)
	  vertOut.col = vec4(1, 0, 0, 1);
  if (gl_ViewIndex == 1)
	  vertOut.col = vec4(0, 1, 0, 1);
}

)EOSHADER";

  const std::string multiViewGeom = common + R"EOSHADER(

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in v2f_block
{
	vec4 pos;
	vec4 col;
	vec4 uv;
} gin[3];

layout(location = 0) out g2f_block
{
	vec4 pos;
	vec4 col;
	vec4 uv;
} gout;

void main()
{
  for(int i = 0; i < 3; i++)
  {
    gl_Position = gl_in[i].gl_Position;

    gout.pos = gin[i].pos;
    gout.col = gin[i].col;
    gout.uv = gin[i].uv;

    if (gl_ViewIndex == 0)
      gout.col = vec4(1, 0, 0, 1);
    if (gl_ViewIndex == 1)
      gout.col = vec4(0, 1, 0, 1);
    EmitVertex();
  }
  EndPrimitive();
}

)EOSHADER";
  const std::string multiViewPixel = common + R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
  if (gl_ViewIndex == 0)
	  Color = vec4(1, 0, 0, 1);
  if (gl_ViewIndex == 1)
	  Color = vec4(0, 1, 0, 1);
}

)EOSHADER";
  void Prepare(int argc, char **argv)
  {
    features.geometryShader = VK_TRUE;
    devExts.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceMultiviewFeaturesKHR multiview = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,
    };

    getPhysFeatures2(&multiview);
    if(!multiview.multiview)
      Avail = "Multiview feature 'multiview' not available";

    devInfoNext = &multiview;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE));

    renderPassCreateInfo.addSubpass(
        {VkAttachmentReference({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL})});

    uint32_t countViews = 2;
    uint32_t viewMask = (1 << countViews) - 1;
    uint32_t correlationMask = viewMask;
    uint32_t countLayers = countViews + 2;

    VkRenderPassMultiviewCreateInfo rpMultiviewCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO};
    rpMultiviewCreateInfo.subpassCount = 1;
    rpMultiviewCreateInfo.pViewMasks = &viewMask;
    rpMultiviewCreateInfo.correlationMaskCount = 1;
    rpMultiviewCreateInfo.pCorrelationMasks = &correlationMask;

    renderPassCreateInfo.next((void *)&rpMultiviewCreateInfo);

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    AllocatedImage fbColourImage(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, countLayers),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageViewCreateInfo colourViewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    colourViewInfo.image = fbColourImage.image;
    colourViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    colourViewInfo.format = mainWindow->format;
    colourViewInfo.flags = 0;
    colourViewInfo.subresourceRange = {};
    colourViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colourViewInfo.subresourceRange.baseMipLevel = 0;
    colourViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    colourViewInfo.subresourceRange.baseArrayLayer = 1;
    colourViewInfo.subresourceRange.layerCount = countViews;
    VkImageView fbColourView = createImageView(&colourViewInfo);

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {fbColourView}, mainWindow->scissor.extent));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    pipeCreateInfo.colorBlendState.attachments = {colorBlendAttachment};
    pipeCreateInfo.depthStencilState.depthTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(multiviewVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    std::vector<std::string> testNames;
    std::vector<VkPipeline> testPipes;
    testPipes.push_back(createGraphicsPipeline(pipeCreateInfo));
    testNames.push_back("Vertex: viewIndex");

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(multiViewPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };
    testPipes.push_back(createGraphicsPipeline(pipeCreateInfo));
    testNames.push_back("Fragment: viewIndex");

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
        CompileShaderModule(multiViewGeom, ShaderLang::glsl, ShaderStage::geom, "main"),
    };
    testPipes.push_back(createGraphicsPipeline(pipeCreateInfo));
    testNames.push_back("Geometry: viewIndex");

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };
    testPipes.push_back(createGraphicsPipeline(pipeCreateInfo));
    testNames.push_back("No viewIndex");

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      // Render multiview to its own framebuffer

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                                    {vkh::ClearValue(0.2f, 0.3f, 0.4f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      for(size_t i = 0; i < testPipes.size(); ++i)
      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, testPipes[i]);
        vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        setMarker(cmd, testNames[i]);
        vkCmdDraw(cmd, 3, 1, 0, 0);
      }

      vkCmdEndRenderPass(cmd);

      // TODO: in the future could copy the multiview renderpass to the framebuffer (left, right)
      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
