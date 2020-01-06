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

RD_TEST(VK_Parameter_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "General tests of parameters known to cause problems - e.g. optional values that should be "
      "ignored, edge cases, special values, etc.";

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

  const std::string pixel2 = R"EOSHADER(
#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform texture2D tex;

void main()
{
	Color = vec4(0, 1, 0, 1) * texelFetch(tex, ivec2(0), 0);
}

)EOSHADER";

  int main()
  {
    optDevExts.push_back(VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool KHR_descriptor_update_template =
        std::find(devExts.begin(), devExts.end(),
                  VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME) != devExts.end();
    bool KHR_push_descriptor = std::find(devExts.begin(), devExts.end(),
                                         VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) != devExts.end();

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, VK_SHADER_STAGE_VERTEX_BIT},
    }));

    VkSampler invalidSampler = (VkSampler)0x1234;
    VkSampler validSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo sampInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampInfo.magFilter = VK_FILTER_LINEAR;
    sampInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device, &sampInfo, NULL, &validSampler);

    VkDescriptorSetLayout immutsetlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, &validSampler},
        {99, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT, &invalidSampler},
    }));

    VkDescriptorSetLayout pushlayout = VK_NULL_HANDLE;
    VkPipelineLayout layout;

    if(KHR_push_descriptor)
    {
      pushlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
          {
              {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              {20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
          },
          VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout, pushlayout}));
    }
    else
    {
      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));
    }

    VkPipelineLayout immutlayout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo({immutsetlayout}));

    VkDescriptorSetLayout setlayout2 = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkDescriptorSet descset2 = allocateDescriptorSet(setlayout2);

    VkPipelineLayout layout2 = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout2}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.layout = immutlayout;

    VkPipeline immutpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel2, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.layout = layout2;

    VkPipeline pipe2 = createGraphicsPipeline(pipeCreateInfo);

    {
      // invalid handle - should not be used because the flag for derived pipelines is not used
      pipeCreateInfo.basePipelineHandle = (VkPipeline)0x1234;

      VkPipeline dummy;
      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipeCreateInfo, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      // invalid index - again should not be used
      pipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
      pipeCreateInfo.basePipelineIndex = 3;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipeCreateInfo, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      pipeCreateInfo.basePipelineIndex = -1;

      // bake the pipeline info so we can mess with the pointers it normally doesn't handle
      VkGraphicsPipelineCreateInfo *baked =
          (VkGraphicsPipelineCreateInfo *)(const VkGraphicsPipelineCreateInfo *)pipeCreateInfo;

      // NULL should be fine, we have no tessellation shaders
      baked->pTessellationState = NULL;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      // same with a garbage pointer
      baked->pTessellationState = (VkPipelineTessellationStateCreateInfo *)0x1234;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      // if we disable rasterization, tons of things can be NULL/garbage
      pipeCreateInfo.rasterizationState.rasterizerDiscardEnable = VK_TRUE;

      baked->pViewportState = NULL;
      baked->pMultisampleState = NULL;
      baked->pDepthStencilState = NULL;
      baked->pColorBlendState = NULL;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      baked->pViewportState = (VkPipelineViewportStateCreateInfo *)0x1234;
      baked->pMultisampleState = (VkPipelineMultisampleStateCreateInfo *)0x1234;
      baked->pDepthStencilState = (VkPipelineDepthStencilStateCreateInfo *)0x1234;
      baked->pColorBlendState = (VkPipelineColorBlendStateCreateInfo *)0x1234;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);
    }

    AllocatedBuffer vb(
        this, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    VkDescriptorSet immutdescset = allocateDescriptorSet(immutsetlayout);

    AllocatedBuffer buf(this,
                        vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    VkBuffer invalidBuffer = (VkBuffer)0x1234;
    VkBuffer validBuffer = buf.buffer;

    AllocatedImage img(this,
                       vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImage validImage = img.image;

    VkImageView validImgView = createImageView(
        vkh::ImageViewCreateInfo(validImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));
    VkImageView invalidImgView = (VkImageView)0x1234;

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });
      vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f), 1,
                           vkh::ImageSubresourceRange());
      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                           VK_IMAGE_LAYOUT_GENERAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, img.image),
               });
      vkEndCommandBuffer(cmd);
      Submit(99, 99, {cmd});
    }

    VkBufferView validBufView =
        createBufferView(vkh::BufferViewCreateInfo(validBuffer, VK_FORMAT_R32G32B32A32_SFLOAT));
    VkBufferView invalidBufView = (VkBufferView)0x1234;

    // initialise the writes with the valid data
    std::vector<VkDescriptorBufferInfo> validBufInfos = {vkh::DescriptorBufferInfo(validBuffer)};
    std::vector<VkBufferView> validBufViews = {validBufView};
    std::vector<VkDescriptorImageInfo> validSoloImgs = {
        vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL),
    };
    std::vector<VkDescriptorImageInfo> validCombinedImgs = {
        vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL, validSampler),
    };
    std::vector<VkDescriptorImageInfo> validSamplers = {
        vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, validSampler),
    };

    std::vector<VkWriteDescriptorSet> writes = {
        vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_SAMPLER, validSamplers),
        vkh::WriteDescriptorSet(descset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                validCombinedImgs),
        vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, validSoloImgs),
        vkh::WriteDescriptorSet(descset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, validSoloImgs),

        vkh::WriteDescriptorSet(descset, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, validBufViews),
        vkh::WriteDescriptorSet(descset, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, validBufViews),

        vkh::WriteDescriptorSet(descset, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, validBufInfos),
        vkh::WriteDescriptorSet(descset, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, validBufInfos),
        vkh::WriteDescriptorSet(descset, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, validBufInfos),
        vkh::WriteDescriptorSet(descset, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, validBufInfos),

        vkh::WriteDescriptorSet(immutdescset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                validCombinedImgs),
        vkh::WriteDescriptorSet(immutdescset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                validSoloImgs),
    };

    // do a first update
    vkh::updateDescriptorSets(device, writes);

    // set invalid handles but valid pointers and try again
    VkDescriptorBufferInfo invalidBufInfo = {};
    invalidBufInfo.buffer = invalidBuffer;

    VkDescriptorImageInfo invalidImgInfo = {};
    invalidImgInfo.sampler = invalidSampler;
    invalidImgInfo.imageView = invalidImgView;

    validSoloImgs[0].sampler = invalidSampler;
    validSamplers[0].imageView = invalidImgView;

    writes[0].pTexelBufferView = &invalidBufView;
    writes[0].pBufferInfo = &invalidBufInfo;

    writes[1].pTexelBufferView = &invalidBufView;
    writes[1].pBufferInfo = &invalidBufInfo;

    writes[2].pTexelBufferView = &invalidBufView;
    writes[2].pBufferInfo = &invalidBufInfo;

    writes[3].pTexelBufferView = &invalidBufView;
    writes[3].pBufferInfo = &invalidBufInfo;

    writes[4].pImageInfo = &invalidImgInfo;
    writes[4].pBufferInfo = &invalidBufInfo;

    writes[5].pImageInfo = &invalidImgInfo;
    writes[5].pBufferInfo = &invalidBufInfo;

    writes[6].pTexelBufferView = &invalidBufView;
    writes[6].pImageInfo = &invalidImgInfo;

    writes[7].pTexelBufferView = &invalidBufView;
    writes[7].pImageInfo = &invalidImgInfo;

    writes[8].pTexelBufferView = &invalidBufView;
    writes[8].pImageInfo = &invalidImgInfo;

    writes[9].pTexelBufferView = &invalidBufView;
    writes[9].pImageInfo = &invalidImgInfo;

    writes[10].pTexelBufferView = &invalidBufView;
    writes[10].pBufferInfo = &invalidBufInfo;

    vkh::updateDescriptorSets(device, writes);

    // finally set invalid pointers too
    VkBufferView *invalidBufViews = (VkBufferView *)0x1234;
    vkh::DescriptorBufferInfo *invalidBufInfos = (vkh::DescriptorBufferInfo *)0x1234;
    vkh::DescriptorImageInfo *invalidImgInfos = (vkh::DescriptorImageInfo *)0x1234;

    writes[0].pTexelBufferView = invalidBufViews;
    writes[0].pBufferInfo = invalidBufInfos;

    writes[1].pTexelBufferView = invalidBufViews;
    writes[1].pBufferInfo = invalidBufInfos;

    writes[2].pTexelBufferView = invalidBufViews;
    writes[2].pBufferInfo = invalidBufInfos;

    writes[3].pTexelBufferView = invalidBufViews;
    writes[3].pBufferInfo = invalidBufInfos;

    writes[4].pImageInfo = invalidImgInfos;
    writes[4].pBufferInfo = invalidBufInfos;

    writes[5].pImageInfo = invalidImgInfos;
    writes[5].pBufferInfo = invalidBufInfos;

    writes[6].pTexelBufferView = invalidBufViews;
    writes[6].pImageInfo = invalidImgInfos;

    writes[7].pTexelBufferView = invalidBufViews;
    writes[7].pImageInfo = invalidImgInfos;

    writes[8].pTexelBufferView = invalidBufViews;
    writes[8].pImageInfo = invalidImgInfos;

    writes[9].pTexelBufferView = invalidBufViews;
    writes[9].pImageInfo = invalidImgInfos;

    vkh::updateDescriptorSets(device, writes);

    if(KHR_descriptor_update_template)
    {
      struct datastruct
      {
        VkBufferView view;
        VkDescriptorBufferInfo buf;
        VkDescriptorImageInfo img;
        VkDescriptorImageInfo combined;
        VkDescriptorImageInfo sampler;
      } data;

      data.view = validBufView;
      data.buf = validBufInfos[0];
      data.img = vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL);
      data.combined = vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL, validSampler);
      data.sampler =
          vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, validSampler);
      data.img.sampler = invalidSampler;
      data.sampler.imageView = invalidImgView;

      std::vector<VkDescriptorUpdateTemplateEntryKHR> entries = {
          // descriptor count 0 updates are allowed
          {0, 0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, 0, sizeof(data)},
          {0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, offsetof(datastruct, sampler), sizeof(data)},
          {1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(datastruct, combined),
           sizeof(data)},
          {2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, offsetof(datastruct, img), sizeof(data)},
          {3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, offsetof(datastruct, img), sizeof(data)},
          {4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, offsetof(datastruct, view), sizeof(data)},
          {5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(datastruct, view), sizeof(data)},
          {6, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, offsetof(datastruct, buf), sizeof(data)},
          {7, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, offsetof(datastruct, buf), sizeof(data)},
          {8, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, offsetof(datastruct, buf),
           sizeof(data)},
          {9, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, offsetof(datastruct, buf),
           sizeof(data)},
      };

      VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();
      createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
      createInfo.descriptorSetLayout = setlayout;
      createInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      createInfo.pipelineLayout = (VkPipelineLayout)0x1234;
      createInfo.set = 123456789;
      VkDescriptorUpdateTemplateKHR templ;
      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &templ);

      vkUpdateDescriptorSetWithTemplateKHR(device, descset, templ, &data);

      vkDestroyDescriptorUpdateTemplateKHR(device, templ, NULL);
    }

    struct pushdatastruct
    {
      VkDescriptorBufferInfo buf;
    } pushdata;

    pushdata.buf = validBufInfos[0];

    VkDescriptorUpdateTemplateKHR pushtempl = VK_NULL_HANDLE;
    if(KHR_descriptor_update_template && KHR_push_descriptor)
    {
      std::vector<VkDescriptorUpdateTemplateEntryKHR> entries = {
          {0, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, sizeof(pushdata)},
          {10, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, sizeof(pushdata)},
      };

      VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();
      createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
      createInfo.descriptorSetLayout = (VkDescriptorSetLayout)0x1234;
      createInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      createInfo.pipelineLayout = layout;
      createInfo.set = 1;
      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &pushtempl);
    }

    // check that stale views in descriptors don't cause problems if the handle is re-used

    VkImageView view1, view2;
    CHECK_VKR(vkCreateImageView(device, vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D,
                                                                 VK_FORMAT_R32G32B32A32_SFLOAT),
                                NULL, &view1));
    CHECK_VKR(vkCreateImageView(device, vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D,
                                                                 VK_FORMAT_R32G32B32A32_SFLOAT),
                                NULL, &view2));

    vkh::updateDescriptorSets(
        device, {
                    // bind view1 to binding 0, we will override this
                    vkh::WriteDescriptorSet(descset2, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                            {vkh::DescriptorImageInfo(view1)}),
                    // we bind view2 to binding 1. This will become stale
                    vkh::WriteDescriptorSet(descset2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                            {vkh::DescriptorImageInfo(view2)}),
                });

    vkDestroyImageView(device, view2, NULL);

    // create view3. Under RD, this is expected to get the same handle as view2 (but a new ID)
    VkImageView view3;
    CHECK_VKR(vkCreateImageView(device, vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D,
                                                                 VK_FORMAT_R32G32B32A32_SFLOAT),
                                NULL, &view3));

    if(rdoc)
    {
      TEST_ASSERT(view2 == view3,
                  "Expected view3 to be a re-used handle. Test isn't going to be valid");
    }

    vkh::updateDescriptorSets(
        device,
        {
            // bind view3 to 0. This means the same handle is now in both binding but only binding 0
            // is valid, binding 1 refers to the 'old' version of this handle.
            vkh::WriteDescriptorSet(descset2, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view3)}),
            // this unbinds the stale view2. Nothing should happen, but if we're comparing by handle
            // this may remove a reference to view3 since it will have the same handle
            vkh::WriteDescriptorSet(descset2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view1)}),
        });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {0, 0});
      vkCmdPushDescriptorSetKHR(
          cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
          vkh::WriteDescriptorSet(VK_NULL_HANDLE, 20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                  validBufInfos));
      if(KHR_descriptor_update_template && KHR_push_descriptor)
        vkCmdPushDescriptorSetWithTemplateKHR(cmd, pushtempl, layout, 1, &pushdata);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, immutpipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, immutlayout, 0,
                                 {immutdescset}, {});

      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe2);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout2, 0, {descset2}, {});

      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroySampler(device, validSampler, NULL);

    vkDestroyImageView(device, view1, NULL);
    vkDestroyImageView(device, view3, NULL);

    if(KHR_descriptor_update_template && KHR_push_descriptor)
      vkDestroyDescriptorUpdateTemplateKHR(device, pushtempl, NULL);

    return 0;
  }
};

REGISTER_TEST();
