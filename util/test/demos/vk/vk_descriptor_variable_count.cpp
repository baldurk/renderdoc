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

RD_TEST(VK_Descriptor_Variable_Count, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Allocates descriptor sets using a variable count to ensure we don't pessimistically "
      "allocate and don't do anything with un-allocated descriptors.";

  std::string common = R"EOSHADER(

#version 450 core

#extension GL_EXT_nonuniform_qualifier : require
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

layout(push_constant) uniform PushData
{
  uint bufidx;
} push;

layout(binding = 0) uniform texture2D tex[];

void main()
{
  Color = texelFetch(tex[push.bufidx], ivec2(vertIn.uv.xy * vec2(4,4)), 0);
}

)EOSHADER";

  const uint32_t numDescriptorSetsInLayout = 100 * 1024;

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    // dependencies of VK_EXT_descriptor_indexing
    devExts.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);

    // enable robustness2 if possible for NULL descriptors
    optDevExts.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);

    // require at least a million descriptors - we won't use them but this gives us enough headroom
    // to check for overallocation
    if(props.limits.maxDescriptorSetSamplers < numDescriptorSetsInLayout)
      Avail = "maxDescriptorSetSamplers " + std::to_string(props.limits.maxDescriptorSetSamplers) +
              " is insufficient";
    else if(props.limits.maxDescriptorSetSampledImages < numDescriptorSetsInLayout)
      Avail = "maxDescriptorSetSampledImages " +
              std::to_string(props.limits.maxDescriptorSetSampledImages) + " is insufficient";

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexing = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingAvail = descIndexing;
    getPhysFeatures2(&indexingAvail);

    if(!indexingAvail.descriptorBindingPartiallyBound)
      Avail = "Descriptor indexing feature 'descriptorBindingPartiallyBound' not available";
    else if(!indexingAvail.descriptorBindingVariableDescriptorCount)
      Avail =
          "Descriptor indexing feature 'descriptorBindingVariableDescriptorCount' not available";
    else if(!indexingAvail.runtimeDescriptorArray)
      Avail = "Descriptor indexing feature 'runtimeDescriptorArray' not available";

    descIndexing.descriptorBindingPartiallyBound = VK_TRUE;
    descIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descIndexing.runtimeDescriptorArray = VK_TRUE;

    devInfoNext = &descIndexing;

    static VkPhysicalDeviceRobustness2FeaturesEXT robust2Feats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
    };

    // enable NULL descriptors if they're supported and the extension was enabled
    if(std::find(devExts.begin(), devExts.end(), VK_EXT_ROBUSTNESS_2_EXTENSION_NAME) != devExts.end())
    {
      VkPhysicalDeviceRobustness2FeaturesEXT robust2avail = robust2Feats;

      getPhysFeatures2(&robust2avail);

      if(robust2avail.nullDescriptor)
        robust2Feats.nullDescriptor = VK_TRUE;
      robust2Feats.pNext = (void *)devInfoNext;
      devInfoNext = &robust2Feats;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const static uint32_t numDescriptorSets = 10 * 1024;
    const static uint32_t numDescriptorsPerSet = 2;

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    };

    VkDescriptorBindingFlagsEXT bindFlags[1] = {
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
    };

    descFlags.bindingCount = 1;
    descFlags.pBindingFlags = bindFlags;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo({
                                               {
                                                   0,
                                                   VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                   numDescriptorSetsInLayout,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                                               },
                                           })
            .next(&descFlags));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {
            setlayout,
        },
        {
            vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i)),
        }));

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
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    DefaultA2V tri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(tri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(tri);

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    setName(img.image, "Colour Tex");

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    Vec4f pixels[4 * 4];
    for(int i = 0; i < 4 * 4; i++)
      pixels[i] = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

    AllocatedBuffer uploadBuf(
        this, vkh::BufferCreateInfo(sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    uploadBuf.upload(pixels);

    uploadBufferToImage(img.image, {4, 4, 1}, uploadBuf.buffer,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkDescriptorSet> descsets;
    VkDescriptorPool descpool = VK_NULL_HANDLE;

    descsets.resize(numDescriptorSets);

    {
      CHECK_VKR(vkCreateDescriptorPool(
          device,
          vkh::DescriptorPoolCreateInfo(
              numDescriptorSets,
              {
                  {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numDescriptorSets * numDescriptorsPerSet + 100},
              }),
          NULL, &descpool));

      std::vector<VkDescriptorSetLayout> setLayouts(numDescriptorSets, setlayout);
      std::vector<uint32_t> counts(numDescriptorSets, numDescriptorsPerSet);

      // make the last one large-ish, to ensure that we still pass the right count through for each
      // set
      counts.back() = std::min(100U, numDescriptorSetsInLayout);

      VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
          NULL,
          numDescriptorSets,
          counts.data(),
      };

      VkDescriptorSetAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          &countInfo,
          descpool,
          numDescriptorSets,
          setLayouts.data(),
      };

      CHECK_VKR(vkAllocateDescriptorSets(device, &allocInfo, descsets.data()));
    }

    vkh::DescriptorImageInfo iminfo(imgview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_NULL_HANDLE);
    vkh::WriteDescriptorSet up(VK_NULL_HANDLE, 0, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, {iminfo});

    up.pImageInfo = &iminfo;
    up.dstArrayElement = 0;
    up.descriptorCount = 1;

    std::vector<VkWriteDescriptorSet> ups;

    // fill the descriptor sets
    for(uint32_t i = 0; i < numDescriptorSets; i++)
    {
      up.dstSet = descsets[i];

      if(i == numDescriptorSets - 1)
        up.dstArrayElement = std::min(100U, numDescriptorSetsInLayout) - 1;

      ups.push_back(up);
    }

    vkh::updateDescriptorSets(device, ups);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      Vec4i idx = {};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i), &idx);

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      // force all descriptor sets to be referenced
      for(uint32_t i = 0; i < numDescriptorSets; i++)
      {
        // for the last set, use the last descriptor
        if(i == numDescriptorSets - 1)
          idx.x = std::min(100U, numDescriptorSetsInLayout) - 1;
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i), &idx);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descsets[i], 0,
                                NULL);

        vkCmdDraw(cmd, 3, 1, 0, 0);
      }

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroyDescriptorPool(device, descpool, NULL);

    return 0;
  }
};

REGISTER_TEST();
