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

RD_TEST(VK_Imageless_Framebuffer, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test using VK_KHR_imageless_framebuffer to specify image views at the last second";

  const std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vec4(1, 0, 0, 1);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);

    // dependencies of VK_EXT_descriptor_indexing
    devExts.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceImagelessFramebufferFeaturesKHR imageless = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR,
    };

    getPhysFeatures2(&imageless);

    if(!imageless.imagelessFramebuffer)
      Avail = "feature 'imagelessFramebuffer' not available";

    devInfoNext = &imageless;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

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

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    int lastWidth = -1;
    VkFramebuffer fb = VK_NULL_HANDLE;

    while(Running())
    {
      if(lastWidth != (int)mainWindow->scissor.extent.width)
      {
        lastWidth = (int)mainWindow->scissor.extent.width;
        if(fb != VK_NULL_HANDLE)
        {
          // be lazy, hard sync
          vkDeviceWaitIdle(device);
          vkDestroyFramebuffer(device, fb, NULL);
        }

        VkFramebufferAttachmentImageInfoKHR imageInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR,
        };

        imageInfo.width = mainWindow->scissor.extent.width;
        imageInfo.height = mainWindow->scissor.extent.height;
        imageInfo.layerCount = 1;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.viewFormatCount = 1;
        imageInfo.pViewFormats = &mainWindow->format;

        VkFramebufferAttachmentsCreateInfoKHR viewsInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO_KHR,
            NULL,
            1,
            &imageInfo,
        };

        CHECK_VKR(
            vkCreateFramebuffer(device,
                                vkh::FramebufferCreateInfo(mainWindow->rp, {(VkImageView)0x1234},
                                                           mainWindow->scissor.extent, 1,
                                                           VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT_KHR)
                                    .next(&viewsInfo),
                                NULL, &fb));
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      VkImageView curView = mainWindow->GetView();
      VkRenderPassAttachmentBeginInfoKHR usedView = {
          VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR,
          NULL,
          1,
          &curView,
      };

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, fb, mainWindow->scissor).next(&usedView),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      VkCommandBuffer cmd2 = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);

      {
        VkCommandBufferInheritanceInfo inherit = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
        inherit.framebuffer = fb;
        inherit.renderPass = mainWindow->rp;

        vkBeginCommandBuffer(cmd2, vkh::CommandBufferBeginInfo(
                                       VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inherit));

        VkViewport v = mainWindow->viewport;
        v.width /= 2;
        v.height /= 2;

        vkCmdBindPipeline(cmd2, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdSetViewport(cmd2, 0, 1, &v);
        vkCmdSetScissor(cmd2, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd2, 0, {vb.buffer}, {0});

        vkCmdDraw(cmd2, 3, 1, 0, 0);

        vkEndCommandBuffer(cmd2);
      }

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, fb, mainWindow->scissor).next(&usedView),
          VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

      vkCmdExecuteCommands(cmd, 1, &cmd2);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd}, {cmd2});

      Present();
    }

    vkDeviceWaitIdle(device);
    vkDestroyFramebuffer(device, fb, NULL);

    return 0;
  }
};

REGISTER_TEST();
