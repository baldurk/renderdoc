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

RD_TEST(VK_Separate_Depth_Stencil_Layouts, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests support for VK_KHR_separate_depth_stencil_layouts extension.";

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

  const std::string inputAttPixel = R"EOSHADER(


layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputDepth;

void main()
{
	float depth = subpassLoad(inputDepth).r;
	Color.rgb = vec3(depth);
	Color.a = 1.0;
}
)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR separateDepthStencilFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR,
    };

    getPhysFeatures2(&separateDepthStencilFeatures);

    if(!separateDepthStencilFeatures.separateDepthStencilLayouts)
      Avail = "'separateDepthStencilLayouts' not available";

    devInfoNext = &separateDepthStencilFeatures;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const DefaultA2V stencilTri[3] = {
        {Vec3f(0.0f, -1.0f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.0f, 0.0f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    const DefaultA2V depthTri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 1.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    const DefaultA2V inputAttTri[3] = {
        {Vec3f(0.0f, -1.0f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-1.0f, 0.0f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.0f, 0.0f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    const DefaultA2V colorTri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer stencilVB(
        this,
        vkh::BufferCreateInfo(sizeof(stencilTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    stencilVB.upload(stencilTri);

    AllocatedBuffer depthVB(
        this,
        vkh::BufferCreateInfo(sizeof(depthTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    depthVB.upload(depthTri);

    AllocatedBuffer inputAttVB(
        this,
        vkh::BufferCreateInfo(sizeof(inputAttTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    inputAttVB.upload(inputAttTri);

    AllocatedBuffer colorVB(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    colorVB.upload(colorTri);

    VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
    {
      std::vector<VkFormat> formats;
      for(VkFormat fmt :
          {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT})
      {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(phys, fmt, &props);
        if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
          depthStencilFormat = fmt;
          break;
        }
      }
      TEST_ASSERT(depthStencilFormat != VK_FORMAT_UNDEFINED,
                  "Couldn't find depth/stencil attachment image format");
    }

    vkh::ImageCreateInfo depthStencilInfo(
        mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0, depthStencilFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    AllocatedImage depthStencilImg(this, depthStencilInfo,
                                   VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(depthStencilImg.image, "Image:DepthStencil");

    vkh::ImageCreateInfo colorInfo(
        mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0, mainWindow->format,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    AllocatedImage colorImg(this, colorInfo, VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(colorImg.image, "Image:Color");

    // subpass 0 (stencil only):
    //   depth: READONLY
    //   stencil: ATTACHMENT
    //     draw stencil only, masking to the RHS of the screen
    // subpass 1 (depth only):
    //   depth: ATTACHMENT
    //   stencil: READONLY
    //     draw foreground triangle, writing only to depth, reading from stencil
    // subpass 2 (color):
    //   depth: READONLY
    //   stencil: NONE
    //     draw foreground triangle, reading from depth as an input attachment,
    // subpass 3 (color):
    //   depth: READONLY
    //   stencil: READONLY
    //     draw triangle, testing for depth but not stencil

    VkRenderPass renderPass = VK_NULL_HANDLE;
    {
      vkh::RenderPassCreator2 renderPassCreateInfo;

      vkh::AttachmentDescription2KHR colorAtt(mainWindow->format,
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      renderPassCreateInfo.attachments.push_back(colorAtt);

      vkh::AttachmentDescription2KHR depthStencilAtt(depthStencilFormat,
                                                     VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR,
                                                     VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR);
      depthStencilAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      depthStencilAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      vkh::AttachmentDescriptionStencilLayoutKHR stencilLayout(
          VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR,
          VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR);
      depthStencilAtt.next(&stencilLayout);
      renderPassCreateInfo.attachments.push_back(depthStencilAtt);

      vkh::AttachmentReferenceStencilLayoutKHR stencilAttachmentLayout(
          VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR);
      vkh::AttachmentReferenceStencilLayoutKHR stencilReadOnlyLayout(
          VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR);

      renderPassCreateInfo.addSubpass(
          {vkh::AttachmentReference2KHR(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_ASPECT_COLOR_BIT)},
          vkh::AttachmentReference2KHR(1, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR,
                                       VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
              .next(&stencilAttachmentLayout));

      renderPassCreateInfo.addSubpass(
          {vkh::AttachmentReference2KHR(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_ASPECT_COLOR_BIT)},
          vkh::AttachmentReference2KHR(1, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR,
                                       VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
              .next(&stencilReadOnlyLayout));

      renderPassCreateInfo.addSubpass(
          {vkh::AttachmentReference2KHR(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_ASPECT_COLOR_BIT)},
          vkh::AttachmentReference2KHR(), {},
          {vkh::AttachmentReference2KHR(1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_IMAGE_ASPECT_DEPTH_BIT)});

      renderPassCreateInfo.addSubpass(
          {vkh::AttachmentReference2KHR(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_ASPECT_COLOR_BIT)},
          vkh::AttachmentReference2KHR(1, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR,
                                       VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
              .next(&stencilReadOnlyLayout));

      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency2KHR(
          0, 1,
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));
      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency2KHR(
          1, 2,
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));
      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency2KHR(
          2, 3,
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));

      CHECK_VKR(vkCreateRenderPass2KHR(device, renderPassCreateInfo, NULL, &renderPass));
    }

    VkImageView colorView = VK_NULL_HANDLE;
    VkImageView depthStencilView = VK_NULL_HANDLE;
    VkImageView depthOnlyView = VK_NULL_HANDLE;
    VkFramebuffer frameBuffer = VK_NULL_HANDLE;
    {
      vkh::ImageViewCreateInfo depthStencilViewInfo(depthStencilImg.image, VK_IMAGE_VIEW_TYPE_2D,
                                                    depthStencilFormat);
      depthStencilViewInfo.subresourceRange.aspectMask =
          VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      CHECK_VKR(vkCreateImageView(device, depthStencilViewInfo, NULL, &depthStencilView));
      depthStencilViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      CHECK_VKR(vkCreateImageView(device, depthStencilViewInfo, NULL, &depthOnlyView));
      CHECK_VKR(vkCreateImageView(
          device, vkh::ImageViewCreateInfo(colorImg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format),
          NULL, &colorView));
      CHECK_VKR(
          vkCreateFramebuffer(device,
                              vkh::FramebufferCreateInfo(renderPass, {colorView, depthStencilView},
                                                         mainWindow->scissor.extent),
                              NULL, &frameBuffer));
    }

    VkPipeline stencilPipe = VK_NULL_HANDLE;
    VkPipeline depthPipe = VK_NULL_HANDLE;
    VkPipeline inputAttPipe = VK_NULL_HANDLE;
    VkPipeline colorPipe = VK_NULL_HANDLE;
    VkPipelineLayout inputAttPipeLayout = VK_NULL_HANDLE;
    VkDescriptorSet inputAttDescSet = VK_NULL_HANDLE;

    {
      VkDescriptorSetLayout setLayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
          {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      }));
      inputAttDescSet = allocateDescriptorSet(setLayout);
      VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());
      inputAttPipeLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setLayout}));

      vkh::GraphicsPipelineCreateInfo stencilPipeCreateInfo;
      stencilPipeCreateInfo.layout = layout;
      stencilPipeCreateInfo.renderPass = renderPass;
      stencilPipeCreateInfo.subpass = 0;
      stencilPipeCreateInfo.vertexInputState.vertexBindingDescriptions = {
          vkh::vertexBind(0, DefaultA2V)};
      stencilPipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
          vkh::vertexAttr(0, 0, DefaultA2V, pos),
          vkh::vertexAttr(1, 0, DefaultA2V, col),
          vkh::vertexAttr(2, 0, DefaultA2V, uv),
      };
      stencilPipeCreateInfo.stages = {
          CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
          CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
      };
      stencilPipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
      stencilPipeCreateInfo.depthStencilState.depthTestEnable = VK_FALSE;
      stencilPipeCreateInfo.depthStencilState.depthWriteEnable = VK_FALSE;
      stencilPipeCreateInfo.depthStencilState.back.failOp = VK_STENCIL_OP_REPLACE;
      stencilPipeCreateInfo.depthStencilState.back.passOp = VK_STENCIL_OP_REPLACE;
      stencilPipeCreateInfo.depthStencilState.back.depthFailOp = VK_STENCIL_OP_REPLACE;
      stencilPipeCreateInfo.depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
      stencilPipeCreateInfo.depthStencilState.back.compareMask = 0xff;
      stencilPipeCreateInfo.depthStencilState.back.writeMask = 0xff;
      stencilPipeCreateInfo.depthStencilState.back.reference = 1;
      stencilPipeCreateInfo.depthStencilState.front = stencilPipeCreateInfo.depthStencilState.back;
      stencilPipe = createGraphicsPipeline(stencilPipeCreateInfo);

      vkh::GraphicsPipelineCreateInfo depthPipeCreateInfo;
      depthPipeCreateInfo.layout = layout;
      depthPipeCreateInfo.renderPass = renderPass;
      depthPipeCreateInfo.subpass = 1;
      depthPipeCreateInfo.vertexInputState.vertexBindingDescriptions = {
          vkh::vertexBind(0, DefaultA2V)};
      depthPipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
          vkh::vertexAttr(0, 0, DefaultA2V, pos),
          vkh::vertexAttr(1, 0, DefaultA2V, col),
          vkh::vertexAttr(2, 0, DefaultA2V, uv),
      };
      depthPipeCreateInfo.stages = {
          CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
          CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
      };
      depthPipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
      depthPipeCreateInfo.depthStencilState.depthTestEnable = VK_TRUE;
      depthPipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
      depthPipeCreateInfo.depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
      depthPipeCreateInfo.depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
      depthPipeCreateInfo.depthStencilState.back.depthFailOp = VK_STENCIL_OP_KEEP;
      depthPipeCreateInfo.depthStencilState.back.compareOp = VK_COMPARE_OP_EQUAL;
      depthPipeCreateInfo.depthStencilState.back.compareMask = 0xff;
      depthPipeCreateInfo.depthStencilState.back.writeMask = 0x0;
      depthPipeCreateInfo.depthStencilState.back.reference = 1;
      depthPipeCreateInfo.depthStencilState.front = depthPipeCreateInfo.depthStencilState.back;
      depthPipe = createGraphicsPipeline(depthPipeCreateInfo);

      vkh::GraphicsPipelineCreateInfo inputAttPipeCreateInfo;
      inputAttPipeCreateInfo.layout = inputAttPipeLayout;
      inputAttPipeCreateInfo.renderPass = renderPass;
      inputAttPipeCreateInfo.subpass = 2;
      inputAttPipeCreateInfo.vertexInputState.vertexBindingDescriptions = {
          vkh::vertexBind(0, DefaultA2V)};
      inputAttPipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
          vkh::vertexAttr(0, 0, DefaultA2V, pos),
          vkh::vertexAttr(1, 0, DefaultA2V, col),
          vkh::vertexAttr(2, 0, DefaultA2V, uv),
      };
      inputAttPipeCreateInfo.stages = {
          CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
          CompileShaderModule(common + inputAttPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
      };
      inputAttPipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
      inputAttPipeCreateInfo.depthStencilState.depthTestEnable = VK_FALSE;
      inputAttPipeCreateInfo.depthStencilState.depthWriteEnable = VK_FALSE;
      inputAttPipeCreateInfo.depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
      inputAttPipeCreateInfo.depthStencilState.back.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      inputAttPipeCreateInfo.depthStencilState.back.depthFailOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      inputAttPipeCreateInfo.depthStencilState.back.compareOp = VK_COMPARE_OP_EQUAL;
      inputAttPipeCreateInfo.depthStencilState.back.compareMask = 0xff;
      inputAttPipeCreateInfo.depthStencilState.back.writeMask = 0xff;
      inputAttPipeCreateInfo.depthStencilState.back.reference = 1;
      inputAttPipeCreateInfo.depthStencilState.front = inputAttPipeCreateInfo.depthStencilState.back;
      inputAttPipe = createGraphicsPipeline(inputAttPipeCreateInfo);

      vkh::GraphicsPipelineCreateInfo colorPipeCreateInfo;
      colorPipeCreateInfo.layout = layout;
      colorPipeCreateInfo.renderPass = renderPass;
      colorPipeCreateInfo.subpass = 3;
      colorPipeCreateInfo.vertexInputState.vertexBindingDescriptions = {
          vkh::vertexBind(0, DefaultA2V)};
      colorPipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
          vkh::vertexAttr(0, 0, DefaultA2V, pos),
          vkh::vertexAttr(1, 0, DefaultA2V, col),
          vkh::vertexAttr(2, 0, DefaultA2V, uv),
      };
      colorPipeCreateInfo.stages = {
          CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
          CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
      };
      colorPipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
      colorPipeCreateInfo.depthStencilState.depthTestEnable = VK_TRUE;
      colorPipeCreateInfo.depthStencilState.depthWriteEnable = VK_FALSE;
      colorPipe = createGraphicsPipeline(colorPipeCreateInfo);
    }

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg = mainWindow->GetImage();
      if((size_t)curFrame <= mainWindow->GetCount())
        setName(swapimg, "Image:Swapchain");

      setMarker(cmd, "Before Transition");

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorImg.image),
              vkh::ImageMemoryBarrier(
                  0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR,
                  depthStencilImg.image, vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT)),
              vkh::ImageMemoryBarrier(
                  0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR,
                  depthStencilImg.image, vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)),
          });

      vkCmdBeginRenderPass(
          cmd,
          vkh::RenderPassBeginInfo(renderPass, frameBuffer, mainWindow->scissor,
                                   {vkh::ClearValue(1, 0, 1, 1), vkh::ClearValue(1.0f, 0)}),
          VK_SUBPASS_CONTENTS_INLINE);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);

      pushMarker(cmd, "Stencil only");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilPipe);
      vkh::cmdBindVertexBuffers(cmd, 0, {stencilVB.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      popMarker(cmd);

      pushMarker(cmd, "Depth write, stencil read");
      vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPipe);
      vkh::cmdBindVertexBuffers(cmd, 0, {depthVB.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      popMarker(cmd);

      pushMarker(cmd, "Depth input attachment");
      vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, inputAttPipe);
      vkh::updateDescriptorSets(
          device, {vkh::WriteDescriptorSet(inputAttDescSet, 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                           {vkh::DescriptorImageInfo(depthOnlyView)})});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, inputAttPipeLayout, 0,
                                 {inputAttDescSet}, {});
      vkh::cmdBindVertexBuffers(cmd, 0, {inputAttVB.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      popMarker(cmd);

      pushMarker(cmd, "Depth/stencil read");
      vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, colorPipe);
      vkh::cmdBindVertexBuffers(cmd, 0, {colorVB.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      popMarker(cmd);

      vkCmdEndRenderPass(cmd);

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT,
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImg.image),
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, swapimg),
               });

      VkImageCopy region = {
          /* srcSubresource = */ {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          /* srcOffset = */ {0, 0, 0},
          /* dstSubresource = */ {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          /* dstOffset = */ {0, 0, 0},
          /* extent = */ {mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 1},
      };

      vkCmdCopyImage(cmd, colorImg.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapimg,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();

      vkDeviceWaitIdle(device);
    }

    vkDestroyRenderPass(device, renderPass, NULL);

    vkDestroyImageView(device, depthStencilView, NULL);
    vkDestroyImageView(device, colorView, NULL);
    vkDestroyFramebuffer(device, frameBuffer, NULL);

    return 0;
  }
};

REGISTER_TEST();
