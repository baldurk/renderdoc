/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

RD_TEST(VK_Overlay_Test, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Makes a couple of draws that show off all the overlays in some way";

  std::string common = R"EOSHADER(

#version 450 core

#extension GL_EXT_samplerless_texture_functions : require

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

layout(constant_id = 1) const int spec_canary = 0;

void main()
{
  if(spec_canary != 1337)
  {
    gl_Position = vertOut.pos = vec4(-1, -1, -1, 1);
    vertOut.col = vec4(0, 0, 0, 0);
    vertOut.uv = vec4(0, 0, 0, 0);
    return;
  }

	vertOut.pos = vec4(Position.xyz, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(constant_id = 2) const int spec_canary = 0;

layout(binding = 0) uniform texture2D tex[64];

void main()
{
  if(spec_canary != 1338) { Color = vec4(1.0, 0.0, 0.0, 1.0); return; }

  if(vertIn.uv.z > 100.0f)
  {
    Color += texelFetch(tex[uint(vertIn.uv.z) % 50], ivec2(vertIn.uv.xy * vec2(4,4)), 0) * 0.001f;
  }

	Color = vertIn.col;
}

)EOSHADER";

  std::string whitepixel = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

layout(constant_id = 2) const int spec_canary = 0;

void main()
{
  if(spec_canary != 1338) { Color = vec4(1.0, 0.0, 0.0, 1.0); return; }

	Color = vec4(1,1,1,1);
}

)EOSHADER";

  int main()
  {
    optDevExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool KHR_maintenance1 = hasExt(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64, VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

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
        {Vec3f(-0.9f, 0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.9f, 0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(1.0f, 0.0f)},

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

        // this triangle deliberately goes out of the viewport, it will test viewport & scissor
        // clipping
        {Vec3f(-1.3f, 1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.3f, 1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(1.0f, 0.0f)},
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
        mainWindow->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})}, 1,
                                    VK_IMAGE_LAYOUT_GENERAL);

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    renderPassCreateInfo.attachments[0].samples = VK_SAMPLE_COUNT_4_BIT;
    renderPassCreateInfo.attachments[1].samples = VK_SAMPLE_COUNT_4_BIT;

    VkRenderPass msaaRP = createRenderPass(renderPassCreateInfo);

    renderPassCreateInfo.attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    renderPassCreateInfo.attachments.pop_back();
    renderPassCreateInfo.subpasses[0].pDepthStencilAttachment = NULL;

    VkRenderPass subrp = createRenderPass(renderPassCreateInfo);

    // create framebuffers using swapchain images and DS image
    std::vector<VkFramebuffer> fbs;
    fbs.resize(mainWindow->GetCount());

    for(size_t i = 0; i < mainWindow->GetCount(); i++)
      fbs[i] = createFramebuffer(vkh::FramebufferCreateInfo(
          renderPass, {mainWindow->GetView(i), dsvview}, mainWindow->scissor.extent));

    // create PSO
    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkSpecializationMapEntry specmap[2] = {
        {1, 0 * sizeof(uint32_t), sizeof(uint32_t)}, {2, 1 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    uint32_t specvals[2] = {1337, 1338};

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = ARRAY_COUNT(specmap);
    spec.pMapEntries = specmap;
    spec.dataSize = sizeof(specvals);
    spec.pData = specvals;

    pipeCreateInfo.stages[0].pSpecializationInfo = &spec;
    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;

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
    VkPipeline depthWritePipe[2];
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeCreateInfo.renderPass = renderPass;
    depthWritePipe[0] = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
    pipeCreateInfo.renderPass = msaaRP;
    depthWritePipe[1] = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    VkPipeline stencilWritePipe[2];
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeCreateInfo.renderPass = renderPass;
    stencilWritePipe[0] = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
    pipeCreateInfo.renderPass = msaaRP;
    stencilWritePipe[1] = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    VkPipeline backgroundPipe[2];
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeCreateInfo.renderPass = renderPass;
    backgroundPipe[0] = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
    pipeCreateInfo.renderPass = msaaRP;
    backgroundPipe[1] = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.front.compareOp = VK_COMPARE_OP_GREATER;
    VkPipeline pipe[2];
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeCreateInfo.renderPass = renderPass;
    pipe[0] = createGraphicsPipeline(pipeCreateInfo);
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
    pipeCreateInfo.renderPass = msaaRP;
    pipe[1] = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[1] =
        CompileShaderModule(whitepixel, ShaderLang::glsl, ShaderStage::frag, "main");
    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeCreateInfo.renderPass = subrp;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    VkPipeline whitepipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.rasterizationState.rasterizerDiscardEnable = VK_TRUE;
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeCreateInfo.renderPass = renderPass;
    VkPipeline discardPipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedImage subimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 4, 5),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView subview[] = {
        createImageView(vkh::ImageViewCreateInfo(
            subimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format, {},
            vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 2, 1, 2, 1))),
        createImageView(vkh::ImageViewCreateInfo(
            subimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format, {},
            vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 3, 1, 2, 1))),
    };

    VkFramebuffer subfb[] = {
        createFramebuffer(vkh::FramebufferCreateInfo(
            subrp, {subview[0]},
            {mainWindow->scissor.extent.width / 4, mainWindow->scissor.extent.height / 4})),
        createFramebuffer(vkh::FramebufferCreateInfo(
            subrp, {subview[1]},
            {mainWindow->scissor.extent.width / 8, mainWindow->scissor.extent.height / 8})),
    };

    AllocatedImage msaaimg(
        this, vkh::ImageCreateInfo(mainWindow->scissor.extent.width,
                                   mainWindow->scissor.extent.height, 0, mainWindow->format,
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, 1, VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedImage msaadepthimg(
        this, vkh::ImageCreateInfo(
                  mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                  VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1, 1,
                  VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView msaaRTV = createImageView(
        vkh::ImageViewCreateInfo(msaaimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format));
    VkImageView msaaDSV = createImageView(vkh::ImageViewCreateInfo(
        msaadepthimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT_S8_UINT, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)));

    VkFramebuffer msaaFB = createFramebuffer(vkh::FramebufferCreateInfo(
        msaaRP, {msaaRTV, msaaDSV},
        {mainWindow->scissor.extent.width, mainWindow->scissor.extent.height}));

    AllocatedImage img(this, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                  VK_IMAGE_USAGE_SAMPLED_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(img.image, "Colour Tex");

    VkImageView dummyView = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> imInfo = {
        vkh::DescriptorImageInfo(dummyView, VK_IMAGE_LAYOUT_GENERAL),
    };
    for(int i = 0; i < 64; i++)
    {
      writes.push_back(
          vkh::WriteDescriptorSet(descset, 0, i, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imInfo));
    }

    vkh::updateDescriptorSets(device, writes);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(
                               renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor,
                               {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f), vkh::ClearValue(1.0f, 0)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "Discard Test");
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, discardPipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      VkViewport v;
      VkRect2D s;

      for(bool is_msaa : {false, true})
      {
        v = mainWindow->viewport;
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

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(
                     is_msaa ? msaaRP : renderPass, is_msaa ? msaaFB : fbs[mainWindow->imgIndex],
                     mainWindow->scissor,
                     {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f), vkh::ClearValue(1.0f, 0)}),
            VK_SUBPASS_CONTENTS_INLINE);

        // draw the setup triangles

        setMarker(cmd, "Setup");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthWritePipe[is_msaa ? 1 : 0]);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stencilWritePipe[is_msaa ? 1 : 0]);
        vkCmdDraw(cmd, 3, 1, 3, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, backgroundPipe[is_msaa ? 1 : 0]);
        vkCmdDraw(cmd, 3, 1, 6, 0);

        // add a marker so we can easily locate this draw
        setMarker(cmd, is_msaa ? "MSAA Test" : "Normal Test");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe[is_msaa ? 1 : 0]);
        vkCmdDraw(cmd, 24, 1, 9, 0);

        if(!is_msaa)
        {
          setMarker(cmd, "Viewport Test");
          v = {10.0f, 10.0f, 80.0f, 80.0f, 0.0f, 1.0f};
          if(KHR_maintenance1)
          {
            v.y += v.height;
            v.height = -v.height;
          }
          s = {{24, 24}, {52, 52}};
          vkCmdSetViewport(cmd, 0, 1, &v);
          vkCmdSetScissor(cmd, 0, 1, &s);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, backgroundPipe[0]);
          vkCmdDraw(cmd, 3, 1, 33, 0);
        }

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

      s = mainWindow->scissor;
      s.extent.width /= 4;
      s.extent.height /= 4;

      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &s);

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(subrp, subfb[0], s,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, whitepipe);

      setMarker(cmd, "Subresources mip 2");
      vkCmdDraw(cmd, 24, 1, 9, 0);

      vkCmdEndRenderPass(cmd);

      v = mainWindow->viewport;
      v.width /= 8.0f;
      v.height /= 8.0f;
      v.width = floorf(v.width);
      v.height = floorf(v.height);
      v.x += 2.0f;
      v.y += 2.0f;
      v.width -= 4.0f;
      v.height -= 4.0f;
      s.extent.width /= 2;
      s.extent.height /= 2;

      if(KHR_maintenance1)
      {
        v.y += v.height;
        v.height = -v.height;
      }

      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &s);

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(subrp, subfb[1], s,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, whitepipe);

      setMarker(cmd, "Subresources mip 3");
      vkCmdDraw(cmd, 24, 1, 9, 0);

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
