/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

RD_TEST(VK_Mesh_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Draws some primitives for testing the mesh view.";

  std::string vertex = R"EOSHADER(
#version 460 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;

layout(push_constant, std140) uniform pushbuf
{
  vec4 scale;
  vec4 offset;
};

layout(location = 0) out vec2 vertOutCol2;
layout(location = 1) out vec4 vertOutcol;

void main()
{
	vec4 pos = vec4(Position.xy * scale.xy + offset.xy, Position.z, 1.0f);
	vertOutcol = Color;

  if(gl_InstanceIndex > 0)
  {
    pos *= 0.3f;
    pos.xy += vec2(0.1f);
    vertOutcol.x = 1.0f; 
  }

  vertOutCol2.xy = pos.xy;

	gl_Position = pos * vec4(1, -1, 1, 1);
#if defined(USE_POINTS)
  gl_PointSize = 1.0f;
#endif
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0) in vec2 vertInCol2;
layout(location = 1) in vec4 vertIncol;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIncol + 1.0e-20 * vertInCol2.xyxy;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const DefaultA2V test[] = {
        // single color quad
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        // points, to test vertex picking
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(70.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(170.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(70.0f, 70.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };

    // create depth-stencil image
    AllocatedImage depthimg(this, vkh::ImageCreateInfo(mainWindow->scissor.extent.width,
                                                       mainWindow->scissor.extent.height, 0,
                                                       VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
                            VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView dsvview = createImageView(vkh::ImageViewCreateInfo(
        depthimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT_S8_UINT, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)));

    // create renderpass using the DS image
    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})}, 1,
                                    VK_IMAGE_LAYOUT_GENERAL);

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    // create framebuffers using swapchain images and DS image
    std::vector<VkFramebuffer> fbs;
    fbs.resize(mainWindow->GetCount());

    for(size_t i = 0; i < mainWindow->GetCount(); i++)
      fbs[i] = createFramebuffer(vkh::FramebufferCreateInfo(
          renderPass, {mainWindow->GetView(i), dsvview}, mainWindow->scissor.extent));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {},
        {
            vkh::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(Vec4f) * 2),
        }));

    VkPipelineLayout layout2 = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {},
        {
            vkh::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Vec4f) * 2),
        }));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.depthStencilState.depthTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.back = pipeCreateInfo.depthStencilState.front;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[0] = CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert,
                                                   "main", {std::make_pair("USE_POINTS", "1")}),
    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipeline pointspipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {{0, 0, VK_VERTEX_INPUT_RATE_VERTEX}};

    pipeCreateInfo.layout = layout2;

    VkPipeline stride0pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(test), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(test);

    Vec4f cbufferdata[] = {
        Vec4f(2.0f / (float)screenWidth, 2.0f / (float)screenHeight, 1.0f, 1.0f),
        Vec4f(-1.0f, -1.0f, 0.0f, 0.0f),
    };

    AllocatedBuffer cb(
        this, vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(cbufferdata);

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
          cmd, vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor,
                                        {{}, vkh::ClearValue(1.0f, 0)}),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(cbufferdata), &cbufferdata);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      vkCmdDraw(cmd, 3, 1, 10, 0);

      setMarker(cmd, "Quad");

      vkCmdDraw(cmd, 6, 2, 0, 0);

      setMarker(cmd, "Points");

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointspipe);

      vkCmdDraw(cmd, 4, 1, 6, 0);

      setMarker(cmd, "Stride 0");

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stride0pipe);
      vkCmdPushConstants(cmd, layout2, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cbufferdata),
                         &cbufferdata);

      vkCmdDraw(cmd, 1, 1, 0, 0);

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
