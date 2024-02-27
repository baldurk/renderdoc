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

RD_TEST(VK_Extended_Dynamic_State, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests all possible dynamic state from VK_EXT_extended_dynamic_state";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);

    features.depthBounds = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
    };

    getPhysFeatures2(&extFeatures);

    if(!extFeatures.extendedDynamicState)
      Avail = "feature 'extendedDynamicState' not available";

    static VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extFeatures2 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT,
    };

    devInfoNext = &extFeatures;

    if(hasExt(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
    {
      getPhysFeatures2(&extFeatures2);

      if(!extFeatures2.extendedDynamicState2)
        Avail = "feature 'extendedDynamicState2' not available";

      extFeatures.pNext = &extFeatures2;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.dynamicState.dynamicStates = {
        VK_DYNAMIC_STATE_CULL_MODE_EXT,           VK_DYNAMIC_STATE_FRONT_FACE_EXT,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,  VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,  VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,   VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,    VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT, VK_DYNAMIC_STATE_STENCIL_OP_EXT,
    };

    if(hasExt(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
    {
      pipeCreateInfo.dynamicState.dynamicStates.push_back(
          VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT);

      pipeCreateInfo.rasterizationState.rasterizerDiscardEnable = VK_TRUE;
    }

    pipeCreateInfo.layout = layout;

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, char)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.viewportState.scissorCount = 0;
    pipeCreateInfo.viewportState.viewportCount = 0;

    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
    pipeCreateInfo.rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    pipeCreateInfo.depthStencilState.depthTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_NEVER;
    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.depthBoundsTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.minDepthBounds = 0.8f;
    pipeCreateInfo.depthStencilState.maxDepthBounds = 0.85f;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_NEVER;
    pipeCreateInfo.depthStencilState.front.failOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    pipeCreateInfo.depthStencilState.front.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    pipeCreateInfo.depthStencilState.front.depthFailOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    pipeCreateInfo.depthStencilState.back = pipeCreateInfo.depthStencilState.front;

    const DefaultA2V tris[6] = {
        {Vec3f(-0.75f, -0.5f, 0.4f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.25f, 0.5f, 0.4f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.25f, -0.5f, 0.4f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.25f, -0.5f, 0.6f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.25f, 0.5f, 0.6f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.75f, -0.5f, 0.6f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(tris) * 4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(tris);

    AllocatedImage depthimg(
        this,
        vkh::ImageCreateInfo(screenWidth, screenHeight, 0, VK_FORMAT_D32_SFLOAT_S8_UINT,
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

    pipeCreateInfo.renderPass = renderPass;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

      vkCmdBeginRenderPass(
          cmd,
          vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor,
                                   {{}, vkh::ClearValue(0.9f, 0xcc)}),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdSetViewportWithCountEXT(cmd, 1, &mainWindow->viewport);
      vkCmdSetScissorWithCountEXT(cmd, 1, &mainWindow->scissor);

      vkCmdSetCullModeEXT(cmd, VK_CULL_MODE_BACK_BIT);
      vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_CLOCKWISE);

      vkCmdSetPrimitiveTopologyEXT(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

      vkCmdSetDepthTestEnableEXT(cmd, VK_TRUE);
      vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_LESS_OR_EQUAL);
      vkCmdSetDepthWriteEnableEXT(cmd, VK_TRUE);

      vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);

      if(hasExt(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
        vkCmdSetRasterizerDiscardEnableEXT(cmd, VK_FALSE);

      vkCmdSetStencilTestEnableEXT(cmd, VK_TRUE);
      vkCmdSetStencilOpEXT(cmd, VK_STENCIL_FACE_FRONT_BIT, VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                           VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                           VK_COMPARE_OP_ALWAYS);

      VkDeviceSize offs = 0, size = sizeof(tris), stride = sizeof(DefaultA2V);
      vkCmdBindVertexBuffers2EXT(cmd, 0, 1, &vb.buffer, &offs, &size, &stride);

      vkCmdDraw(cmd, 6, 1, 0, 0);

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
