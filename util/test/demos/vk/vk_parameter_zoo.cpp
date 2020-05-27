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

static const VkDescriptorUpdateTemplateEntryKHR constEntry = {
    4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0, 16,
};

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

  const std::string refpixel = R"EOSHADER(
#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform texture2D tex;

void main()
{
	Color = vec4(0, 1, 0, 1) * texelFetch(tex, ivec2(0), 0);
}

)EOSHADER";

  struct refdatastruct
  {
    VkDescriptorImageInfo sampler;
    VkDescriptorImageInfo combined;
    VkDescriptorImageInfo sampled;
    VkDescriptorImageInfo storage;
    VkBufferView unitexel;
    VkBufferView storetexel;
    VkDescriptorBufferInfo unibuf;
    VkDescriptorBufferInfo storebuf;
    VkDescriptorBufferInfo unibufdyn;
    VkDescriptorBufferInfo storebufdyn;
  };

  VkSampler refsamp[4];

  VkSampler refcombinedsamp[4];
  AllocatedImage refcombinedimg[4];
  VkImageView refcombinedimgview[4];

  AllocatedImage refsampled[4];
  VkImageView refsampledview[4];

  AllocatedImage refstorage[4];
  VkImageView refstorageview[4];

  AllocatedBuffer refunitexel[4];
  VkBufferView refunitexelview[4];

  AllocatedBuffer refstoretexel[4];
  VkBufferView refstoretexelview[4];

  AllocatedBuffer refunibuf[4];

  AllocatedBuffer refstorebuf[4];

  AllocatedBuffer refunibufdyn[4];

  AllocatedBuffer refstorebufdyn[4];

  refdatastruct GetData(uint32_t idx)
  {
    return {
        // sampler
        vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, refsamp[idx]),
        // combined
        vkh::DescriptorImageInfo(refcombinedimgview[idx], VK_IMAGE_LAYOUT_GENERAL,
                                 refcombinedsamp[idx]),
        // sampled
        vkh::DescriptorImageInfo(refsampledview[idx], VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE),
        // storage
        vkh::DescriptorImageInfo(refstorageview[idx], VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE),
        // unitexel
        refunitexelview[idx],
        // storetexel
        refstoretexelview[idx],
        // unibuf
        vkh::DescriptorBufferInfo(refunibuf[idx].buffer),
        // storebuf
        vkh::DescriptorBufferInfo(refstorebuf[idx].buffer),
        // unibufdyn
        vkh::DescriptorBufferInfo(refunibufdyn[idx].buffer),
        // storebufdyn
        vkh::DescriptorBufferInfo(refstorebufdyn[idx].buffer),
    };
  }

  void Prepare(int argc, char **argv)
  {
    optDevExts.push_back(VK_EXT_TOOLING_INFO_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
    };

    if(std::find(devExts.begin(), devExts.end(), VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) !=
       devExts.end())
    {
      timeline.timelineSemaphore = VK_TRUE;
      devInfoNext = &timeline;
    }

    static VkPhysicalDeviceVulkan12Features vk12 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      // don't enable any features, just link the struct in.
      // deliberately replace the VkPhysicalDeviceTimelineSemaphoreFeaturesKHR above because it was
      // rolled into this struct - so we enable that one feature if we're using it
      devInfoNext = &vk12;

      VkPhysicalDeviceVulkan12Features vk12avail = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };

      getPhysFeatures2(&vk12avail);

      if(vk12avail.timelineSemaphore)
        vk12.timelineSemaphore = VK_TRUE;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    std::vector<VkPhysicalDeviceToolPropertiesEXT> tools;

    if(std::find(devExts.begin(), devExts.end(), VK_EXT_TOOLING_INFO_EXTENSION_NAME) != devExts.end())
    {
      uint32_t toolCount = 0;
      vkGetPhysicalDeviceToolPropertiesEXT(phys, &toolCount, NULL);
      tools.resize(toolCount);
      vkGetPhysicalDeviceToolPropertiesEXT(phys, &toolCount, tools.data());

      TEST_LOG("%u tools available:", toolCount);
      for(VkPhysicalDeviceToolPropertiesEXT &tool : tools)
        TEST_LOG("  - %s", tool.name);
    }

    bool KHR_descriptor_update_template =
        std::find(devExts.begin(), devExts.end(),
                  VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME) != devExts.end();
    bool KHR_push_descriptor = std::find(devExts.begin(), devExts.end(),
                                         VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) != devExts.end();
    bool EXT_transform_feedback =
        std::find(devExts.begin(), devExts.end(), VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME) !=
        devExts.end();
    bool KHR_timeline_semaphore =
        std::find(devExts.begin(), devExts.end(), VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) !=
        devExts.end();

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      VkPhysicalDeviceVulkan12Features vk12avail = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };

      getPhysFeatures2(&vk12avail);

      if(vk12avail.timelineSemaphore)
        KHR_timeline_semaphore = true;
    }

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

    VkDescriptorSetLayout refsetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
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
        }));

    VkSampler invalidSampler = (VkSampler)0x1234;
    VkSampler validSampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));

    VkDescriptorSetLayout immutsetlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, &validSampler},
        {99, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT, &invalidSampler},
    }));

    VkPipelineLayout layout, reflayout;

    if(KHR_push_descriptor)
    {
      VkDescriptorSetLayout pushlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
          {
              {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              {20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
          },
          VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout, pushlayout}));

      VkDescriptorSetLayout refpushlayout =
          createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
              {
                  {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              },
              VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      if(KHR_descriptor_update_template)
        reflayout = createPipelineLayout(
            vkh::PipelineLayoutCreateInfo({refsetlayout, refsetlayout, refpushlayout}));
      else
        reflayout =
            createPipelineLayout(vkh::PipelineLayoutCreateInfo({refsetlayout, refpushlayout}));
    }
    else
    {
      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

      if(KHR_descriptor_update_template)
        reflayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({refsetlayout, refsetlayout}));
      else
        reflayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({refsetlayout}));
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

    pipeCreateInfo.layout = reflayout;

    VkPipeline refpipe = createGraphicsPipeline(pipeCreateInfo);

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

      VkPipelineViewportStateCreateInfo *viewState =
          (VkPipelineViewportStateCreateInfo *)&pipeCreateInfo.viewportState;

      baked->pViewportState = viewState;

      // viewport and scissor are already dynamic, so just set viewports and scissors to invalid
      // pointers
      viewState->pViewports = (VkViewport *)0x1234;
      viewState->pScissors = (VkRect2D *)0x1234;

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
    VkDescriptorSet refdescset = allocateDescriptorSet(refsetlayout);
    VkDescriptorSet reftempldescset = allocateDescriptorSet(refsetlayout);

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
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, img.image),
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

    VkDescriptorUpdateTemplateKHR reftempl = VK_NULL_HANDLE;

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

      // try with constant entry
      createInfo.descriptorUpdateEntryCount = 1;
      createInfo.pDescriptorUpdateEntries = &constEntry;
      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &templ);
      vkUpdateDescriptorSetWithTemplateKHR(device, descset, templ, &data);
      vkDestroyDescriptorUpdateTemplateKHR(device, templ, NULL);

      entries = {
          {0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, offsetof(refdatastruct, sampler), sizeof(data)},
          {1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(refdatastruct, combined),
           sizeof(data)},
          {2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, offsetof(refdatastruct, sampled), sizeof(data)},
          {3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, offsetof(refdatastruct, storage), sizeof(data)},
          {4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, offsetof(refdatastruct, unitexel),
           sizeof(data)},
          {5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(refdatastruct, storetexel),
           sizeof(data)},
          {6, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, offsetof(refdatastruct, unibuf), sizeof(data)},
          {7, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, offsetof(refdatastruct, storebuf),
           sizeof(data)},
          {8, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, offsetof(refdatastruct, unibufdyn),
           sizeof(data)},
          {9, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, offsetof(refdatastruct, storebufdyn),
           sizeof(data)},
      };

      createInfo.descriptorSetLayout = refsetlayout;
      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();

      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &reftempl);
    }

    struct pushdatastruct
    {
      VkDescriptorBufferInfo buf;
    } pushdata;

    pushdata.buf = validBufInfos[0];

    VkDescriptorUpdateTemplateKHR pushtempl = VK_NULL_HANDLE, refpushtempl = VK_NULL_HANDLE;
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

      entries = {
          {0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, offsetof(refdatastruct, sampler),
           sizeof(refdatastruct)},
          {1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(refdatastruct, combined),
           sizeof(refdatastruct)},
          {2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, offsetof(refdatastruct, sampled),
           sizeof(refdatastruct)},
          {3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, offsetof(refdatastruct, storage),
           sizeof(refdatastruct)},
          {4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, offsetof(refdatastruct, unitexel),
           sizeof(refdatastruct)},
          {5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(refdatastruct, storetexel),
           sizeof(refdatastruct)},
          {6, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, offsetof(refdatastruct, unibuf),
           sizeof(refdatastruct)},
          {7, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, offsetof(refdatastruct, storebuf),
           sizeof(refdatastruct)},
      };

      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();
      // set 0 = normal
      // set 1 = template
      // set 2 = push
      createInfo.pipelineLayout = reflayout;
      createInfo.set = 2;

      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &refpushtempl);
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
        device,
        {
            // bind view1 to binding 0, we will override this
            vkh::WriteDescriptorSet(descset2, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view1, VK_IMAGE_LAYOUT_GENERAL)}),
            // we bind view2 to binding 1. This will become stale
            vkh::WriteDescriptorSet(descset2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view2, VK_IMAGE_LAYOUT_GENERAL)}),
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
                                    {vkh::DescriptorImageInfo(view3, VK_IMAGE_LAYOUT_GENERAL)}),
            // this unbinds the stale view2. Nothing should happen, but if we're comparing by handle
            // this may remove a reference to view3 since it will have the same handle
            vkh::WriteDescriptorSet(descset2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view1, VK_IMAGE_LAYOUT_GENERAL)}),
        });

    refdatastruct resetrefdata = {};
    resetrefdata.sampler.sampler = resetrefdata.combined.sampler = validSampler;
    resetrefdata.sampled.imageView = resetrefdata.combined.imageView =
        resetrefdata.storage.imageView = validImgView;
    resetrefdata.sampled.imageLayout = resetrefdata.combined.imageLayout =
        resetrefdata.storage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    resetrefdata.unitexel = resetrefdata.storetexel = validBufView;
    resetrefdata.unibuf.buffer = resetrefdata.storebuf.buffer = resetrefdata.unibufdyn.buffer =
        resetrefdata.storebufdyn.buffer = validBuffer;
    resetrefdata.unibuf.range = resetrefdata.storebuf.range = resetrefdata.unibufdyn.range =
        resetrefdata.storebufdyn.range = VK_WHOLE_SIZE;

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });

      // create the specific resources that will only be referenced through descriptor updates
      for(int i = 0; i < 4; i++)
      {
        refsamp[i] = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
        setName(refsamp[i], "refsamp" + std::to_string(i));

        refcombinedsamp[i] = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
        setName(refcombinedsamp[i], "refcombinedsamp" + std::to_string(i));

        VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
        VmaAllocationCreateInfo allocInfo = {0, VMA_MEMORY_USAGE_GPU_ONLY};

        refcombinedimg[i] = AllocatedImage(
            this, vkh::ImageCreateInfo(2, 2, 0, fmt, VK_IMAGE_USAGE_SAMPLED_BIT), allocInfo);
        refcombinedimgview[i] = createImageView(
            vkh::ImageViewCreateInfo(refcombinedimg[i].image, VK_IMAGE_VIEW_TYPE_2D, fmt));
        setName(refcombinedimg[i].image, "refcombinedimg" + std::to_string(i));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL, refcombinedimg[i].image),
                 });

        refsampled[i] = AllocatedImage(
            this, vkh::ImageCreateInfo(2, 2, 0, fmt, VK_IMAGE_USAGE_SAMPLED_BIT), allocInfo);
        refsampledview[i] = createImageView(
            vkh::ImageViewCreateInfo(refsampled[i].image, VK_IMAGE_VIEW_TYPE_2D, fmt));
        setName(refsampled[i].image, "refsampled" + std::to_string(i));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL, refsampled[i].image),
                 });

        refstorage[i] = AllocatedImage(
            this, vkh::ImageCreateInfo(2, 2, 0, fmt, VK_IMAGE_USAGE_STORAGE_BIT), allocInfo);
        refstorageview[i] = createImageView(
            vkh::ImageViewCreateInfo(refstorage[i].image, VK_IMAGE_VIEW_TYPE_2D, fmt));
        setName(refstorage[i].image, "refstorage" + std::to_string(i));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL, refstorage[i].image),
                 });

        refunitexel[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT), allocInfo);
        refunitexelview[i] = createBufferView(vkh::BufferViewCreateInfo(refunitexel[i].buffer, fmt));
        setName(refunitexel[i].buffer, "refunitexel" + std::to_string(i));

        refstoretexel[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT), allocInfo);
        refstoretexelview[i] =
            createBufferView(vkh::BufferViewCreateInfo(refstoretexel[i].buffer, fmt));
        setName(refstoretexel[i].buffer, "refstoretexel" + std::to_string(i));

        refunibuf[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), allocInfo);
        setName(refunibuf[i].buffer, "refunibuf" + std::to_string(i));

        refstorebuf[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), allocInfo);
        setName(refstorebuf[i].buffer, "refstorebuf" + std::to_string(i));

        refunibufdyn[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), allocInfo);
        setName(refunibufdyn[i].buffer, "refunibufdyn" + std::to_string(i));

        refstorebufdyn[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), allocInfo);
        setName(refstorebufdyn[i].buffer, "refstorebufdyn" + std::to_string(i));
      }

      vkEndCommandBuffer(cmd);
      Submit(99, 99, {cmd});
    }

    VkBufferUsageFlags xfbUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if(EXT_transform_feedback)
    {
      xfbUsage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
      xfbUsage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
    }

    AllocatedBuffer xfbBuf(this, vkh::BufferCreateInfo(256, xfbUsage),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    if(vkSetDebugUtilsObjectNameEXT)
    {
      VkDebugUtilsObjectNameInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.objectType = VK_OBJECT_TYPE_BUFFER;
      info.objectHandle = (uint64_t)xfbBuf.buffer;
      info.pObjectName = NULL;
      vkSetDebugUtilsObjectNameEXT(device, &info);
    }

    VkFence fence;
    CHECK_VKR(vkCreateFence(device, vkh::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT), NULL, &fence));

    VkEvent ev;
    CHECK_VKR(vkCreateEvent(device, vkh::EventCreateInfo(), NULL, &ev));

    VkSemaphore sem = VK_NULL_HANDLE;

    if(KHR_timeline_semaphore)
    {
      VkSemaphoreTypeCreateInfo semType = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
      semType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      semType.initialValue = 1234;

      CHECK_VKR(vkCreateSemaphore(device, vkh::SemaphoreCreateInfo().next(&semType), NULL, &sem));
    }

    while(Running())
    {
      // acquire and clear the backbuffer
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        vkEndCommandBuffer(cmd);

        Submit(0, 4, {cmd});
      }

      // do a bunch of spinning on fences/semaphores that should not be serialised exhaustively
      VkResult status = VK_SUCCESS;
      for(size_t i = 0; i < 1000; i++)
        status = vkGetFenceStatus(device, fence);

      if(status != VK_SUCCESS)
        TEST_WARN("Expected fence to be set (it was created signalled)");

      for(size_t i = 0; i < 1000; i++)
        status = vkGetEventStatus(device, ev);

      if(status != VK_EVENT_RESET)
        TEST_WARN("Expected event to be unset");

      if(KHR_timeline_semaphore)
      {
        uint64_t val = 0;
        for(size_t i = 0; i < 1000; i++)
          vkGetSemaphoreCounterValueKHR(device, sem, &val);

        if(val != 1234)
          TEST_WARN("Expected timeline semaphore value to be 1234");
      }

      // reference some resources through different descriptor types to ensure that they are
      // properly included
      {
        vkDeviceWaitIdle(device);

        refdatastruct refdata = GetData(0);
        refdatastruct reftempldata = GetData(1);
        refdatastruct refpushdata = GetData(2);
        refdatastruct refpushtempldata = GetData(3);

        vkh::updateDescriptorSets(
            device,
            {
                vkh::WriteDescriptorSet(refdescset, 0, VK_DESCRIPTOR_TYPE_SAMPLER, {refdata.sampler}),
                vkh::WriteDescriptorSet(refdescset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        {refdata.combined}),
                vkh::WriteDescriptorSet(refdescset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                        {refdata.sampled}),
                vkh::WriteDescriptorSet(refdescset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                        {refdata.storage}),
                vkh::WriteDescriptorSet(refdescset, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                        {refdata.unitexel}),
                vkh::WriteDescriptorSet(refdescset, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                        {refdata.storetexel}),
                vkh::WriteDescriptorSet(refdescset, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        {refdata.unibuf}),
                vkh::WriteDescriptorSet(refdescset, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        {refdata.storebuf}),
                vkh::WriteDescriptorSet(refdescset, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                        {refdata.unibufdyn}),
                vkh::WriteDescriptorSet(refdescset, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                        {refdata.storebufdyn}),
            });

        if(KHR_descriptor_update_template)
          vkUpdateDescriptorSetWithTemplateKHR(device, reftempldescset, reftempl, &reftempldata);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        if(KHR_descriptor_update_template)
          setMarker(cmd, "KHR_descriptor_update_template");
        if(KHR_push_descriptor)
          setMarker(cmd, "KHR_push_descriptor");

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, refpipe);

        uint32_t set = 0;

        // set 0 is always the normal one
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, reflayout, set++,
                                   {refdescset}, {0, 0});

        // if we have update templates, set 1 is always the template one
        if(KHR_descriptor_update_template)
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, reflayout, set++,
                                     {reftempldescset}, {0, 0});

        // push set comes after the ones above. Note that because we can't have more than one push
        // set, we test with the first set of refs here then do a template update (if supported) and
        // draw again to test the second set of refs.
        if(KHR_push_descriptor)
        {
          vkh::cmdPushDescriptorSets(
              cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, reflayout, set,
              {
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 0, VK_DESCRIPTOR_TYPE_SAMPLER,
                                          {refpushdata.sampler}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                          {refpushdata.combined}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          {refpushdata.sampled}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                          {refpushdata.storage}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                          {refpushdata.unitexel}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                          {refpushdata.storetexel}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          {refpushdata.unibuf}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          {refpushdata.storebuf}),
              });
        }

        VkViewport view = {128, 0, 128, 128, 0, 1};
        vkCmdSetViewport(cmd, 0, 1, &view);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        setMarker(cmd, "References");
        vkCmdDraw(cmd, 1, 1, 0, 0);

        if(KHR_descriptor_update_template && KHR_push_descriptor)
        {
          setMarker(cmd, "PushTemplReferences");
          vkCmdPushDescriptorSetWithTemplateKHR(cmd, refpushtempl, reflayout, set, &refpushtempldata);
          vkCmdDraw(cmd, 1, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmd);

        if(vkCmdBeginDebugUtilsLabelEXT)
        {
          VkDebugUtilsLabelEXT info = {};
          info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
          info.pLabelName = NULL;
          vkCmdBeginDebugUtilsLabelEXT(cmd, &info);
        }

        if(vkCmdInsertDebugUtilsLabelEXT)
        {
          VkDebugUtilsLabelEXT info = {};
          info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
          info.pLabelName = NULL;
          vkCmdInsertDebugUtilsLabelEXT(cmd, &info);
        }

        if(vkCmdEndDebugUtilsLabelEXT)
          vkCmdEndDebugUtilsLabelEXT(cmd);

        vkEndCommandBuffer(cmd);

        Submit(1, 4, {cmd});

        vkDeviceWaitIdle(device);

        if(vkQueueBeginDebugUtilsLabelEXT)
        {
          VkDebugUtilsLabelEXT info = {};
          info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
          info.pLabelName = NULL;
          vkQueueBeginDebugUtilsLabelEXT(queue, &info);
        }

        if(vkQueueInsertDebugUtilsLabelEXT)
        {
          VkDebugUtilsLabelEXT info = {};
          info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
          info.pLabelName = NULL;
          vkQueueInsertDebugUtilsLabelEXT(queue, &info);
        }

        if(vkQueueEndDebugUtilsLabelEXT)
          vkQueueEndDebugUtilsLabelEXT(queue);

        // scribble over the descriptor contents so that initial contents fetch never gets these
        // resources that way
        vkh::updateDescriptorSets(
            device,
            {
                vkh::WriteDescriptorSet(refdescset, 0, VK_DESCRIPTOR_TYPE_SAMPLER,
                                        {resetrefdata.sampler}),
                vkh::WriteDescriptorSet(refdescset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        {resetrefdata.combined}),
                vkh::WriteDescriptorSet(refdescset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                        {resetrefdata.sampled}),
                vkh::WriteDescriptorSet(refdescset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                        {resetrefdata.storage}),
                vkh::WriteDescriptorSet(refdescset, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                        {resetrefdata.unitexel}),
                vkh::WriteDescriptorSet(refdescset, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                        {resetrefdata.storetexel}),
                vkh::WriteDescriptorSet(refdescset, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        {resetrefdata.unibuf}),
                vkh::WriteDescriptorSet(refdescset, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        {resetrefdata.storebuf}),
                vkh::WriteDescriptorSet(refdescset, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                        {resetrefdata.unibufdyn}),
                vkh::WriteDescriptorSet(refdescset, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                        {resetrefdata.storebufdyn}),
            });

        if(KHR_descriptor_update_template)
          vkUpdateDescriptorSetWithTemplateKHR(device, reftempldescset, reftempl, &resetrefdata);
      }

      // check the rendering with our parameter tests is OK
      {
        vkDeviceWaitIdle(device);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        if(!tools.empty())
        {
          pushMarker(cmd, "Tools available");
          for(VkPhysicalDeviceToolPropertiesEXT &tool : tools)
            setMarker(cmd, tool.name);
          popMarker(cmd);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset},
                                   {0, 0});
        if(KHR_push_descriptor)
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

        setMarker(cmd, "Color Draw");
        vkCmdDraw(cmd, 3, 1, 0, 0);

        if(EXT_transform_feedback)
        {
          VkDeviceSize offs = 0;
          // pSizes is optional and can be NULL to use the whole buffer size
          vkCmdBindTransformFeedbackBuffersEXT(cmd, 0, 1, &xfbBuf.buffer, &offs, NULL);

          // pCounterBuffers is also optional
          vkCmdBeginTransformFeedbackEXT(cmd, 0, 0, NULL, NULL);
          vkCmdEndTransformFeedbackEXT(cmd, 0, 0, NULL, NULL);
        }

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        Submit(2, 4, {cmd});
      }

      // finish with the backbuffer
      {
        vkDeviceWaitIdle(device);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkEndCommandBuffer(cmd);

        Submit(3, 4, {cmd});
      }

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroyEvent(device, ev, NULL);
    vkDestroyFence(device, fence, NULL);
    vkDestroySemaphore(device, sem, NULL);

    vkDestroyImageView(device, view1, NULL);
    vkDestroyImageView(device, view3, NULL);

    if(KHR_descriptor_update_template && KHR_push_descriptor)
      vkDestroyDescriptorUpdateTemplateKHR(device, pushtempl, NULL);

    if(KHR_descriptor_update_template)
      vkDestroyDescriptorUpdateTemplateKHR(device, reftempl, NULL);

    return 0;
  }
};

REGISTER_TEST();
