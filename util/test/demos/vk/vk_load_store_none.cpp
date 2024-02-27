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

RD_TEST(VK_Load_Store_None, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests the use of LOAD_OP_NONE and STORE_OP_NONE to preserve an attachment without "
      "modification.";

  std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0, index = 0) out vec4 Color1;
layout(location = 1, index = 0) out vec4 Color2;

void main()
{
	Color1 = Color2 = vec4(1.0, 0.0, 0.0, 1.0);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME);

    // require dynamic color write enable for the easiest time testing both load and store op none.
    //
    // these ops are there because renderpasses with different ops but everything else is the same
    // are still compatible, so these can be switched last minute without needing to recompile a
    // pipeline. So the cases they are useful are those where a pipeline is declared to use an
    // attachment then at the last minute the application realises it doesn't need it.
    //
    // NONE store op can be useful on its own in a couple of scenarios when you're using read-only
    // depth, as there's no way to express "this was not written and no synchronisation is needed"
    // because don't care and store are both write operations.
    //
    // NONE load op is only useful when you want to preserve an attachment that's now unused but had
    // it declared as modified and e.g. load/store'd at creation time. For depth this could happen
    // with EXT_extended_dynamic_state if you disabled depth testing which was previously used,
    // and wanted to preserve the depth. That's more annoying to test though, so we require
    // color_write_enable for the same purpose (dynamically disabling).
    //
    // We could just create the pipeline as not writing to those attachments from the start, but
    // that would be a bit too artificial and not how this is used in practice.
    devExts.push_back(VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceColorWriteEnableFeaturesEXT colorEnableFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT,
    };

    colorEnableFeats.colorWriteEnable = VK_TRUE;
    devInfoNext = &colorEnableFeats;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedImage preserveimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(preserveimg.image, "PreserveImg");

    VkImageView preserveimgview = createImageView(vkh::ImageViewCreateInfo(
        preserveimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));
    // unused attachment
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL}),
                                     VkAttachmentReference({1, VK_IMAGE_LAYOUT_GENERAL})});

    // this RP has clear/store
    VkRenderPass pipeRP = createRenderPass(renderPassCreateInfo);

    renderPassCreateInfo.attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_NONE_EXT;
    renderPassCreateInfo.attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_NONE_EXT;

    // this RP has none/none and will be used for rendering
    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer = createFramebuffer(vkh::FramebufferCreateInfo(
        renderPass, {imgview, preserveimgview}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = pipeRP;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);

    pipeCreateInfo.colorBlendState.attachments.push_back({
        // blendEnable
        VK_FALSE,
        // color*
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        // alpha*
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        // colorWriteMask
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
    });

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      // put the attachment in GENERAL
      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                           preserveimg.image),
               });

      // use the original pipeline's RP to clear and store
      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(pipeRP, framebuffer, mainWindow->scissor,
                                                    {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f),
                                                     vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      // clear a rect in the middle of the preserve attachment to green
      VkClearAttachment att = {};
      att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      att.clearValue.color.float32[1] = 1.0f;
      att.clearValue.color.float32[3] = 1.0f;
      att.colorAttachment = 1;
      VkClearRect rect = {};
      rect.baseArrayLayer = 0;
      rect.layerCount = 1;
      rect.rect.offset.x = 150;
      rect.rect.offset.y = 100;
      rect.rect.extent.width = 75;
      rect.rect.extent.height = 50;
      vkCmdClearAttachments(cmd, 1, &att, 1, &rect);

      vkCmdEndRenderPass(cmd);

      // we'll be reading later, synchronise here once
      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                           VK_IMAGE_LAYOUT_GENERAL, preserveimg.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                                    {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f),
                                                     vkh::ClearValue(1.0f, 0.2f, 0.2f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      VkBool32 enables[2] = {true, false};
      vkCmdSetColorWriteEnableEXT(cmd, 2, enables);

      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      // no need to synchronise here, nothing changed the preserved image

      blitToSwap(cmd, preserveimg.image, VK_IMAGE_LAYOUT_GENERAL, swapimg, VK_IMAGE_LAYOUT_GENERAL);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
