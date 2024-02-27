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

RD_TEST(VK_Large_Descriptor_Sets, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Allocates very large descriptor sets (with only a small segment actually used) to check that"
      "we don't allocate unreasonable amounts of memory or spend unreasonable amounts of time "
      "tracking.";

  bool updateAfterBind = false;
  static const uint32_t index = 77;
  static const uint32_t arraySize = 1000 * 1000;

  std::string common = R"EOSHADER(

#version 420 core

#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_nonuniform_qualifier : require

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

layout(push_constant) uniform PushData
{
  uint texidx;
} push;

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform texture2D smiley[];

void main()
{
	Color = texelFetch(smiley[push.texidx], ivec2(64 * vertIn.uv.xy), 0);
  Color.w = 1.0f;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    // dependencies of VK_EXT_descriptor_indexing
    devExts.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);

    VkPhysicalDeviceDescriptorIndexingPropertiesEXT descProps = {};
    descProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;
    getPhysProperties2(&descProps);

    // try to use normal descriptors if possible
    if(props.limits.maxDescriptorSetSampledImages < arraySize)
    {
      // on some IHVs the update after bind limit is *higher*. If that's good enough, use update
      // after bind pools
      if(descProps.maxDescriptorSetUpdateAfterBindSampledImages >= arraySize)
      {
        updateAfterBind = true;
      }
      else
      {
        Avail = "maxDescriptorSetSampledImages " +
                std::to_string(props.limits.maxDescriptorSetSampledImages) + " is insufficient";
      }
    }

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexing = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    getPhysFeatures2(&descIndexing);

    if(!descIndexing.descriptorBindingPartiallyBound)
      Avail = "Descriptor indexing feature 'descriptorBindingPartiallyBound' not available";
    else if(!descIndexing.runtimeDescriptorArray)
      Avail = "Descriptor indexing feature 'runtimeDescriptorArray' not available";
    else if(!descIndexing.shaderSampledImageArrayNonUniformIndexing)
      Avail =
          "Descriptor indexing feature 'shaderSampledImageArrayNonUniformIndexing' not available";

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingEnable = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    descIndexingEnable.descriptorBindingPartiallyBound = VK_TRUE;
    descIndexingEnable.runtimeDescriptorArray = VK_TRUE;
    descIndexingEnable.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    devInfoNext = &descIndexingEnable;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    };

    VkDescriptorBindingFlagsEXT bindFlags[1] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
    };

    descFlags.bindingCount = 1;
    descFlags.pBindingFlags = bindFlags;

    if(updateAfterBind)
      bindFlags[0] |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo(
            {{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, arraySize, VK_SHADER_STAGE_FRAGMENT_BIT}})
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

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

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

    VkDescriptorSet descset[5] = {};
    VkDescriptorPool descpool = VK_NULL_HANDLE;

    {
      VkDescriptorPoolCreateFlags flags =
          updateAfterBind ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT : 0;

      CHECK_VKR(vkCreateDescriptorPool(
          device,
          vkh::DescriptorPoolCreateInfo(8, {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, arraySize * 10}}, flags),
          NULL, &descpool));

      CHECK_VKR(vkAllocateDescriptorSets(
          device,
          vkh::DescriptorSetAllocateInfo(descpool,
                                         {setlayout, setlayout, setlayout, setlayout, setlayout}),
          descset));
    }

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(
                        descset[0], 0, index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        {
                            vkh::DescriptorImageInfo(
                                smileyview, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE),
                        }),
                });

    while(Running())
    {
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

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      // force all descriptor sets to be referenced
      for(int i = 0; i < ARRAY_COUNT(descset); i++)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset[i], 0,
                                NULL);

      // bind the actual one
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset[0], 0,
                              NULL);

      Vec4i idx = {(int)index};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i), &idx);

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
