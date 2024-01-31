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

RD_TEST(VK_Blend, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle repeatedly to test blending within a single drawcall";

  const DefaultA2V TemplateTriangleRed[3] = {
      {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1 / 255.f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(1 / 255.f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(1 / 255.f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  };
  const int TRIANGLES_RED_INDEX = 0;
  const int NUM_TRIANGLES_RED = 16;
  const DefaultA2V TemplateTriangleGreen[3] = {
      {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1 / 255.f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1 / 255.f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1 / 255.f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
  };
  const int TRIANGLES_GREEN_INDEX = TRIANGLES_RED_INDEX + NUM_TRIANGLES_RED;
  const int NUM_TRIANGLES_GREEN = 255;
  const DefaultA2V TemplateTriangleBlue[3] = {
      {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1 / 255.f, 1.0f), Vec2f(0.0f, 0.0f)},
      {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1 / 255.f, 1.0f), Vec2f(0.0f, 1.0f)},
      {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1 / 255.f, 1.0f), Vec2f(1.0f, 0.0f)},
  };
  const int TRIANGLES_BLUE_INDEX = TRIANGLES_GREEN_INDEX + NUM_TRIANGLES_GREEN;
  const int NUM_TRIANGLES_BLUE = 512;

  const int NUM_TRIANGLES_TOTAL = TRIANGLES_BLUE_INDEX + NUM_TRIANGLES_BLUE;

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    std::vector<DefaultA2V> triangles;
    triangles.reserve(3 * NUM_TRIANGLES_TOTAL);
    for(int i = 0; i < NUM_TRIANGLES_RED; i++)
    {
      triangles.push_back(TemplateTriangleRed[0]);
      triangles.push_back(TemplateTriangleRed[1]);
      triangles.push_back(TemplateTriangleRed[2]);
    }
    for(int i = 0; i < NUM_TRIANGLES_GREEN; i++)
    {
      triangles.push_back(TemplateTriangleGreen[0]);
      triangles.push_back(TemplateTriangleGreen[1]);
      triangles.push_back(TemplateTriangleGreen[2]);
    }
    for(int i = 0; i < NUM_TRIANGLES_BLUE; i++)
    {
      triangles.push_back(TemplateTriangleBlue[0]);
      triangles.push_back(TemplateTriangleBlue[1]);
      triangles.push_back(TemplateTriangleBlue[2]);
    }

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultA2V) * 3 * NUM_TRIANGLES_TOTAL,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(triangles.data(), sizeof(DefaultA2V) * 3 * NUM_TRIANGLES_TOTAL);

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {imgview}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    pipeCreateInfo.colorBlendState.attachments = {colorBlendAttachment};

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg = StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });

      pushMarker(cmd, "Clear");
      vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      popMarker(cmd);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      pushMarker(cmd, "Red: groups of repeated draws");
      for(int i = 1; i <= NUM_TRIANGLES_RED; i *= 2)
      {
        vkCmdDraw(cmd, 3 * i, 1, TRIANGLES_RED_INDEX, 0);
      }
      setMarker(cmd, "End of red");
      popMarker(cmd);
      pushMarker(cmd, "Green: 255 (the maximum we can handle) in a single drawcall");
      vkCmdDraw(cmd, 3 * NUM_TRIANGLES_GREEN, 1, 3 * TRIANGLES_GREEN_INDEX, 0);
      popMarker(cmd);
      pushMarker(cmd, "Blue: 512 (more than the maximum) in a single drawcall");
      vkCmdDraw(cmd, 3 * NUM_TRIANGLES_BLUE, 1, 3 * TRIANGLES_BLUE_INDEX, 0);
      popMarker(cmd);

      vkCmdEndRenderPass(cmd);

      pushMarker(cmd, "Clear");
      vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f), 1,
                           vkh::ImageSubresourceRange());
      popMarker(cmd);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "All of the above in a single drawcall");
      vkCmdDraw(cmd, 3 * NUM_TRIANGLES_TOTAL, 1, 3 * TRIANGLES_RED_INDEX, 0);
      popMarker(cmd);

      setMarker(cmd, "Test End");

      vkCmdEndRenderPass(cmd);

      blitToSwap(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL, swapimg,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      vkEndCommandBuffer(cmd);

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
