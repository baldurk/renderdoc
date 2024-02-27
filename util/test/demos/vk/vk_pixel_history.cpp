/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

RD_TEST(VK_Pixel_History, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests pixel history";

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
	vertOut.pos = vec4(Position.xyz, 1);
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
  if (gl_FragCoord.x < 151 && gl_FragCoord.x > 150)
    discard;
	Color = vertIn.col + vec4(0, 0, 0, 1.75);
}

)EOSHADER";

  std::string mspixel = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  if(gl_PrimitiveID == 0)
  {
    Color = vec4(1, 0, 1, 2.75);
    return;
  }

  if (gl_SampleID == 0)
    Color = vec4(1, 0, 0, 2.75);
  else if (gl_SampleID == 1)
    Color = vec4(0, 0, 1, 2.75);
  else if (gl_SampleID == 2)
    Color = vec4(0, 1, 1, 2.75);
  else if (gl_SampleID == 3)
    Color = vec4(1, 1, 1, 2.75);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    features.depthBounds = true;
    features.geometryShader = true;
    features.sampleRateShading = true;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    optDevExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool KHR_maintenance1 = std::find(devExts.begin(), devExts.end(),
                                      VK_KHR_MAINTENANCE1_EXTENSION_NAME) != devExts.end();

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    // note that the Y position values are inverted for vulkan 1.0 viewport convention, relative to
    // all other APIs
    DefaultA2V VBData[] = {
        // this triangle occludes in depth
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle occludes in stencil
        {Vec3f(-0.5f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, -0.5f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle is just in the background to contribute to overdraw
        {Vec3f(-0.9f, 0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.9f, 0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // the draw has a few triangles, main one that is occluded for depth, another that is
        // adding to overdraw complexity, one that is backface culled, then a few more of various
        // sizes for triangle size overlay
        {Vec3f(-0.3f, 0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, -0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.2f, 0.2f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.2f, 0.0f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.2f, 0.4f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // backface culled
        {Vec3f(0.1f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // depth clipped (i.e. not clamped)
        {Vec3f(0.6f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, -0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.0f, 1.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // small triangles
        // size=0.005
        {Vec3f(0.0f, -0.4f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.41f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.01f, -0.4f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.015
        {Vec3f(0.0f, -0.5f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.515f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.015f, -0.5f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.02
        {Vec3f(0.0f, -0.6f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.62f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.02f, -0.6f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.025
        {Vec3f(0.0f, -0.7f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.725f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.025f, -0.7f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // dynamic triangles
        {Vec3f(-0.6f, 0.75f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.65f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, 0.75f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.6f, 0.75f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.65f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, 0.75f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.6f, 0.75f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.65f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, 0.75f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.6f, 0.75f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.65f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, 0.75f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // Different depth triangles
        {Vec3f(0.0f, 0.8f, 0.97f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, 0.2f, 0.97f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.8f, 0.97f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.2f, 0.8f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, 0.4f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, 0.8f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.2f, 0.8f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, 0.6f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, 0.8f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.2f, 0.8f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, 0.7f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, 0.8f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // Fails depth bounds test.
        {Vec3f(0.2f, 0.8f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, 0.7f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, 0.8f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // Should be back face culled.
        {Vec3f(0.6f, 0.8f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.4f, 0.7f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.2f, 0.8f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        // depth bounds prep
        {Vec3f(0.6f, -0.3f, 0.3f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, -0.5f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, -0.3f, 0.7f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // depth bounds clip
        {Vec3f(0.6f, -0.3f, 0.3f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, -0.5f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, -0.3f, 0.7f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    // negate y if we're using negative viewport height
    if(KHR_maintenance1)
    {
      for(DefaultA2V &v : VBData)
        v.pos.y = -v.pos.y;
    }

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(VBData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(VBData);

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

    // create depth-stencil image
    AllocatedImage depthimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             depthStencilFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(depthimg.image, "depthimg");

    VkImageView dsvview = createImageView(vkh::ImageViewCreateInfo(
        depthimg.image, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)));

    // create renderpass using the DS image
    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        depthStencilFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
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

    // create PSO
    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    VkPipelineShaderStageCreateInfo vertexShader =
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main");
    VkPipelineShaderStageCreateInfo fragmentShader =
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main");

    pipeCreateInfo.stages = {vertexShader, fragmentShader};

    pipeCreateInfo.rasterizationState.depthClampEnable = VK_FALSE;
    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    pipeCreateInfo.depthStencilState.depthTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_ALWAYS;
    pipeCreateInfo.depthStencilState.front.passOp = VK_STENCIL_OP_REPLACE;
    pipeCreateInfo.depthStencilState.front.reference = 0x55;
    pipeCreateInfo.depthStencilState.front.compareMask = 0xff;
    pipeCreateInfo.depthStencilState.front.writeMask = 0xff;
    pipeCreateInfo.depthStencilState.back = pipeCreateInfo.depthStencilState.front;

    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    VkPipeline depthWritePipe = createGraphicsPipeline(pipeCreateInfo);

    VkPipeline dynamicScissorPipe, fixedScissorPassPipe, fixedScissorFailPipe,
        dynamicStencilRefPipe, dynamicStencilMaskPipe;
    {
      vkh::GraphicsPipelineCreateInfo dynamicPipe = pipeCreateInfo;
      dynamicPipe.depthStencilState.depthWriteEnable = VK_FALSE;
      dynamicPipe.depthStencilState.depthTestEnable = VK_FALSE;

      dynamicScissorPipe = createGraphicsPipeline(dynamicPipe);
      setName(dynamicScissorPipe, "dynamicScissorPipe");

      dynamicPipe.dynamicState.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT};
      dynamicPipe.viewportState.scissors = {{{95, 245}, {10, 10}}};

      fixedScissorPassPipe = createGraphicsPipeline(dynamicPipe);
      setName(fixedScissorPassPipe, "fixedScissorPassPipe");

      dynamicPipe.viewportState.scissors = {{{95, 245}, {4, 4}}};

      fixedScissorFailPipe = createGraphicsPipeline(dynamicPipe);
      setName(fixedScissorFailPipe, "fixedScissorFailPipe");

      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

      dynamicStencilRefPipe = createGraphicsPipeline(dynamicPipe);
      setName(dynamicStencilRefPipe, "dynamicStencilRefPipe");

      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
      dynamicPipe.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);

      dynamicStencilMaskPipe = createGraphicsPipeline(dynamicPipe);
      setName(dynamicStencilMaskPipe, "dynamicStencilMaskPipe");
    }

    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipeline depthPipe;
    {
      vkh::GraphicsPipelineCreateInfo depthPipeInfo = pipeCreateInfo;
      depthPipeInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
      depthPipeInfo.depthStencilState.depthBoundsTestEnable = VK_TRUE;
      depthPipe = createGraphicsPipeline(depthPipeInfo);
      setName(depthPipe, "depthPipe");
    }

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    VkPipeline stencilWritePipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    VkPipeline backgroundPipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {vertexShader};
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.front.reference = 0x33;
    VkPipeline noFsPipe = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.stages = {vertexShader, fragmentShader};
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.front.reference = 0x55;

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_GREATER;
    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;

    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    VkPipeline cullFrontPipe = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

    pipeCreateInfo.depthStencilState.depthBoundsTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.minDepthBounds = 0.0f;
    pipeCreateInfo.depthStencilState.maxDepthBounds = 1.0f;
    VkPipeline depthBoundsPipe1 = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.depthStencilState.minDepthBounds = 0.4f;
    pipeCreateInfo.depthStencilState.maxDepthBounds = 0.6f;
    VkPipeline depthBoundsPipe2 = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.depthStencilState.depthBoundsTestEnable = VK_FALSE;

    renderPassCreateInfo.attachments.pop_back();
    renderPassCreateInfo.subpasses[0].pDepthStencilAttachment = NULL;

    VkRenderPass subrp = createRenderPass(renderPassCreateInfo);

    pipeCreateInfo.renderPass = subrp;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    VkPipeline whitepipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedImage subimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 4, 5),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(subimg.image, "subimg");

    VkImageView subview = createImageView(vkh::ImageViewCreateInfo(
        subimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 2, 1, 2, 1)));

    VkFramebuffer subfb = createFramebuffer(vkh::FramebufferCreateInfo(
        subrp, {subview},
        {mainWindow->scissor.extent.width / 4, mainWindow->scissor.extent.height / 4}));

    // Multi sampled

    {
      std::vector<VkFormat> formats;
      for(VkFormat fmt :
          {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT})
      {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(phys, fmt, &props);
        if(props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
           VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        {
          depthStencilFormat = fmt;
          break;
        }
      }
      TEST_ASSERT(depthStencilFormat != VK_FORMAT_UNDEFINED,
                  "Couldn't find depth/stencil attachment image format");
    }

    renderPassCreateInfo.attachments[0].samples = VK_SAMPLE_COUNT_4_BIT;
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        depthStencilFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_SAMPLE_COUNT_4_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE));
    renderPassCreateInfo.subpasses.clear();
    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})}, 1,
                                    VK_IMAGE_LAYOUT_GENERAL);

    VkRenderPass submsrp = createRenderPass(renderPassCreateInfo);

    pipeCreateInfo.stages[1] =
        CompileShaderModule(mspixel, ShaderLang::glsl, ShaderStage::frag, "main");

    pipeCreateInfo.renderPass = submsrp;
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
    VkPipeline mspipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedImage submsimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, 4,
                             VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(submsimg.image, "submsimg");

    VkImageView submsview = createImageView(vkh::ImageViewCreateInfo(
        submsimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 2, 1)));

    AllocatedImage msimgdepth(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             depthStencilFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1, 4,
                             VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(msimgdepth.image, "msimgdepth");

    VkImageView msdepthview = createImageView(vkh::ImageViewCreateInfo(
        msimgdepth.image, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 2,
                                   1)));

    VkFramebuffer submsfb = createFramebuffer(vkh::FramebufferCreateInfo(
        submsrp, {submsview, msdepthview},
        {mainWindow->scissor.extent.width, mainWindow->scissor.extent.height}));

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      VkViewport v = mainWindow->viewport;
      v.x += 10.0f;
      v.y += 10.0f;
      v.width -= 20.0f;
      v.height -= 20.0f;

      // if we're using KHR_maintenance1, check that negative viewport height is handled
      if(KHR_maintenance1)
      {
        v.y += v.height;
        v.height = -v.height;
      }

      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      setMarker(cmd, "Begin RenderPass");
      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(
                               renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor,
                               {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f), vkh::ClearValue(1.0f, 0)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      // draw the setup triangles

      setMarker(cmd, "Depth Write");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthWritePipe);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      setMarker(cmd, "Unbound Fragment Shader");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, noFsPipe);
      vkCmdDraw(cmd, 3, 1, 3, 0);

      setMarker(cmd, "Stencil Write");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilWritePipe);
      vkCmdDraw(cmd, 3, 1, 3, 0);

      setMarker(cmd, "Background");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, backgroundPipe);
      vkCmdDraw(cmd, 3, 1, 6, 0);

      setMarker(cmd, "Cull Front");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cullFrontPipe);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      setMarker(cmd, "Depth Bounds Prep");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthBoundsPipe1);
      vkCmdDraw(cmd, 3, 1, 63, 0);
      setMarker(cmd, "Depth Bounds Clip");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthBoundsPipe2);
      vkCmdDraw(cmd, 3, 1, 66, 0);

      // add a marker so we can easily locate this draw
      setMarker(cmd, "Test Begin");

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdDraw(cmd, 24, 1, 9, 0);

      setMarker(cmd, "Fixed Scissor Fail");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fixedScissorFailPipe);
      vkCmdDraw(cmd, 3, 1, 33, 0);

      setMarker(cmd, "Fixed Scissor Pass");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fixedScissorPassPipe);
      vkCmdDraw(cmd, 3, 1, 36, 0);

      setMarker(cmd, "Dynamic Stencil Ref");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dynamicStencilRefPipe);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0x67);
      vkCmdDraw(cmd, 3, 1, 39, 0);

      setMarker(cmd, "Dynamic Stencil Mask");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dynamicStencilMaskPipe);
      vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
      vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
      vkCmdDraw(cmd, 3, 1, 42, 0);

      // Six triangles, five fragments reported.
      // 0: Fails depth test
      // 1: Passes
      // 2: Fails depth test compared to 1st fragment
      // 3: Passes
      // 4: Fails depth bounds test
      // 5: Fails backface culling, not reported.
      setMarker(cmd, "Depth Test");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPipe);
      vkCmdSetDepthBounds(cmd, 0.15f, 1.0f);
      vkCmdDraw(cmd, 6 * 3, 1, 45, 0);

      vkCmdEndRenderPass(cmd);

      {
        setMarker(cmd, "Multisampled: begin renderpass");
        vkCmdBeginRenderPass(cmd,
                             vkh::RenderPassBeginInfo(
                                 submsrp, submsfb, mainWindow->scissor,
                                 {vkh::ClearValue(0.f, 1.0f, 0.f, 1.0f), vkh::ClearValue(0.f, 0)}),
                             VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mspipe);

        setMarker(cmd, "Multisampled: test");
        vkCmdDraw(cmd, 6, 1, 3, 0);

        vkCmdEndRenderPass(cmd);
      }

      v = mainWindow->viewport;
      v.width /= 4.0f;
      v.height /= 4.0f;
      v.x += 5.0f;
      v.y += 5.0f;
      v.width -= 10.0f;
      v.height -= 10.0f;

      if(KHR_maintenance1)
      {
        v.y += v.height;
        v.height = -v.height;
      }

      VkRect2D s = mainWindow->scissor;
      s.extent.width /= 4;
      s.extent.height /= 4;

      setMarker(cmd, "Begin RenderPass Secondary");
      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(subrp, subfb, s, {vkh::ClearValue(0.f, 1.0f, 0.f, 1.0f)}),
          VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

      std::vector<VkCommandBuffer> secondaries;
      // Record the first secondary command buffer.
      {
        VkCommandBuffer cmd2 = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        secondaries.push_back(cmd2);
        vkBeginCommandBuffer(
            cmd2, vkh::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                                              vkh::CommandBufferInheritanceInfo(subrp, 0, subfb)));
        vkh::cmdBindVertexBuffers(cmd2, 0, {vb.buffer}, {0});
        vkCmdBindPipeline(cmd2, VK_PIPELINE_BIND_POINT_GRAPHICS, whitepipe);
        vkCmdSetViewport(cmd2, 0, 1, &v);
        vkCmdSetScissor(cmd2, 0, 1, &s);
        setMarker(cmd2, "Secondary: background");
        vkCmdDraw(cmd2, 6, 1, 3, 0);
        setMarker(cmd2, "Secondary: culled");
        vkCmdDraw(cmd2, 6, 1, 12, 0);
        setMarker(cmd2, "Secondary: pink");
        vkCmdDraw(cmd2, 9, 1, 24, 0);
        setMarker(cmd2, "Secondary: red and blue");
        vkCmdDraw(cmd2, 6, 1, 0, 0);
        vkEndCommandBuffer(cmd2);
      }

      setMarker(cmd, "Secondary Test");
      vkCmdExecuteCommands(cmd, (uint32_t)secondaries.size(), secondaries.data());

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);
      Submit(0, 1, {cmd}, secondaries);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
