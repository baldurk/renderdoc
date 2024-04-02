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

RD_TEST(VK_Graphics_Pipeline, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests the use of graphics pipelines and makes sure different features handle them.";

  const std::string vertex = R"EOSHADER(

#version 420 core

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

layout(set = 0, binding = 0) uniform ubo
{
  vec2 offset;
  vec2 pad;
  vec4 scale;
};

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
#if 0
  vertOut.col = Color.yxzw;
#endif
	vertOut.uv = vec4(UV.xy + vec2(100.0f, 100.0f) + offset.xy, 0, 1) * scale;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

#version 460 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;
layout(location = 1, index = 0) out vec4 Color1;

layout(set = 1, binding = 0) uniform sampler2D smiley[16];

layout(constant_id = 1) const int spec_canary = 0;

layout(push_constant) uniform PushData
{
  uint idx;
} push;

void main()
{
  if(spec_canary != 1337) { Color = vec4(0.2, 0.0, 0.2, 1.0); return; }

	Color = vertIn.col * 0.5f + 0.5f * texture(smiley[push.idx], vec2(0.4f, 0.6f));
  Color1 = vec4(1.0 - vertIn.col.x, 1.0 - vertIn.col.y, 1.0 - vertIn.col.z, 1.0);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    devExts.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphlibFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
    };

    if(hasExt(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME))
    {
      graphlibFeats.graphicsPipelineLibrary = VK_TRUE;
      graphlibFeats.pNext = (void *)devInfoNext;
      devInfoNext = &graphlibFeats;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout vsetlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_VERTEX_BIT,
        },
    }));
    VkDescriptorSetLayout fsetlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            16,
            VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    }));

    VkPipelineLayoutCreateFlags layoutFlags = 0;

    if(hasExt(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME))
      layoutFlags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

    VkPipelineLayout vlayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {vsetlayout, VK_NULL_HANDLE}, {vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4)},
        layoutFlags));
    VkPipelineLayout flayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {VK_NULL_HANDLE, fsetlayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4)},
        layoutFlags));
    VkPipelineLayout fulllayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {vsetlayout, fsetlayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4)},
        layoutFlags));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL}),
                                     VkAttachmentReference({1, VK_IMAGE_LAYOUT_GENERAL})},
                                    2, VK_IMAGE_LAYOUT_GENERAL);

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = {};
    libInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    pipeCreateInfo.pNext = &libInfo;

    VkPipeline libList[4] = {};

    libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
    VkGraphicsPipelineCreateInfo *info = pipeCreateInfo;
    info->pTessellationState = NULL;
    info->pViewportState = NULL;
    info->pRasterizationState = NULL;
    info->pMultisampleState = NULL;
    info->pDepthStencilState = NULL;
    info->pColorBlendState = NULL;
    info->flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    libList[0] = createGraphicsPipeline(info);

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {};

    std::vector<uint32_t> spirv = ::CompileShaderToSpv(
        vertex, SPIRVTarget::vulkan12, ShaderLang::glsl, ShaderStage::vert, "main", {});

    VkShaderModuleCreateInfo vertShad = vkh::ShaderModuleCreateInfo(spirv);

    pipeCreateInfo.stages = {
        vkh::PipelineShaderStageCreateInfo(VK_NULL_HANDLE, VK_SHADER_STAGE_VERTEX_BIT),
    };
    pipeCreateInfo.stages[0].pNext = &vertShad;

    pipeCreateInfo.layout = vlayout;
    pipeCreateInfo.renderPass = renderPass;

    libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
    info = pipeCreateInfo;
    info->pTessellationState = NULL;
    info->pMultisampleState = NULL;
    info->flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    libList[1] = createGraphicsPipeline(info);

    spirv = ::CompileShaderToSpv(pixel, SPIRVTarget::vulkan12, ShaderLang::glsl, ShaderStage::frag,
                                 "main", {});

    VkShaderModuleCreateInfo fragShad = vkh::ShaderModuleCreateInfo(spirv);

    pipeCreateInfo.stages = {
        vkh::PipelineShaderStageCreateInfo(VK_NULL_HANDLE, VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    pipeCreateInfo.stages[0].pNext = &fragShad;

    VkSpecializationMapEntry specmap[1] = {
        {1, 0 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    uint32_t specvals[1] = {1337};

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = ARRAY_COUNT(specmap);
    spec.pMapEntries = specmap;
    spec.dataSize = sizeof(specvals);
    spec.pData = specvals;

    pipeCreateInfo.stages[0].pSpecializationInfo = &spec;

    pipeCreateInfo.layout = flayout;

    libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
    info = pipeCreateInfo;
    info->pVertexInputState = NULL;
    info->pInputAssemblyState = NULL;
    info->pRasterizationState = NULL;
    info->pTessellationState = NULL;
    info->pViewportState = NULL;
    info->pColorBlendState = NULL;
    info->flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    libList[2] = createGraphicsPipeline(info);

    pipeCreateInfo.stages = {};
    pipeCreateInfo.layout = VK_NULL_HANDLE;

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

    libInfo.pNext = NULL;
    libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
    info = pipeCreateInfo;
    info->pVertexInputState = NULL;
    info->pInputAssemblyState = NULL;
    info->pTessellationState = NULL;
    info->pViewportState = NULL;
    info->pRasterizationState = NULL;
    info->flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    libList[3] = createGraphicsPipeline(info);

    VkPipelineLibraryCreateInfoKHR libs = {};
    libs.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
    libs.libraryCount = 4;
    libs.pLibraries = libList;

    VkGraphicsPipelineCreateInfo linkedPipeInfo = {};
    linkedPipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    linkedPipeInfo.layout = fulllayout;
    linkedPipeInfo.pNext = &libs;

    VkPipeline pipe = createGraphicsPipeline(&linkedPipeInfo);

    AllocatedImage offimg(
        this,
        vkh::ImageCreateInfo(screenWidth, screenHeight, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView offview = createImageView(vkh::ImageViewCreateInfo(
        offimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedImage depthimg(
        this,
        vkh::ImageCreateInfo(screenWidth, screenHeight, 0, VK_FORMAT_D32_SFLOAT_S8_UINT,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView dsvview = createImageView(vkh::ImageViewCreateInfo(
        depthimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT_S8_UINT, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)));

    // create framebuffers using swapchain images and DS image
    std::vector<VkFramebuffer> fbs;
    fbs.resize(mainWindow->GetCount());

    for(size_t i = 0; i < mainWindow->GetCount(); i++)
      fbs[i] = createFramebuffer(vkh::FramebufferCreateInfo(
          renderPass, {mainWindow->GetView(i), offview, dsvview}, mainWindow->scissor.extent));

    Vec4f cbufferdata[2] = {
        Vec4f(-100.0f, -100.0f, 0.0f, 0.0f),
        Vec4f(1.0f, 1.0f, 1.0f, 1.0f),
    };

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(cbufferdata);

    VkDescriptorSet vdescset = allocateDescriptorSet(vsetlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(vdescset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(cb.buffer, 0)}),
                });

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    AllocatedImage smiley(
        this,
        vkh::ImageCreateInfo(rgba8.width, rgba8.height, 0, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView smileyview = createImageView(
        vkh::ImageViewCreateInfo(smiley.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM));

    AllocatedBuffer uploadBuf(this,
                              vkh::BufferCreateInfo(rgba8.data.size() * sizeof(uint32_t),
                                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                              VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    uploadBuf.upload(rgba8.data.data(), rgba8.data.size() * sizeof(uint32_t));

    uploadBufferToImage(smiley.image, {rgba8.width, rgba8.height, 1}, uploadBuf.buffer,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkSampler smileysampler = createSampler(
        vkh::SamplerCreateInfo(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR,
                               VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));

    VkDescriptorSet fdescset = allocateDescriptorSet(fsetlayout);

    uint32_t idx = 13;

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(
                        fdescset, 0, idx, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        {
                            vkh::DescriptorImageInfo(
                                smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, smileysampler),
                        }),
                });

    AllocatedImage badimg(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(badimg.image, "Black Tex");

    VkImageView badimgview = createImageView(vkh::ImageViewCreateInfo(
        badimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    for(uint32_t i = 0; i < 16; i++)
    {
      if(i == idx)
        continue;

      vkh::updateDescriptorSets(
          device,
          {
              vkh::WriteDescriptorSet(
                  fdescset, 0, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                  {
                      vkh::DescriptorImageInfo(badimgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                               smileysampler),
                  }),
          });
    }

    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, badimg.image),
               });

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

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

      StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdBeginRenderPass(
          cmd,
          vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor,
                                   {
                                       vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f),
                                       vkh::ClearValue(0.0f, 0.0f, 0.2f, 1.0f),
                                       vkh::ClearValue(1.0f, 0),
                                   }),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdPushConstants(cmd, fulllayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &idx);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fulllayout, 0, {vdescset}, {});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fulllayout, 1, {fdescset}, {});
      vkCmdDraw(cmd, 3, 1, 0, 0);

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
