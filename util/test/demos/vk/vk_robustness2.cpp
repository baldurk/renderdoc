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

RD_TEST(VK_Robustness2, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Checks handling of NULL descriptors and NULL vertex buffers for VK_EXT_robustness2.";

  std::string common = R"EOSHADER(

#version 460 core

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
layout(location = 2) in vec4 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = UV;
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(push_constant) uniform PushData {
  ivec4 coord;
} push;

layout(set = 0, binding = 2, std430) buffer oobbuftype
{
  vec4 arr[];
} oobbuf;

layout(set = 0, binding = 3, rgba32f) uniform coherent image2D oobImage;

layout(set = 0, binding = 10, std140) uniform constsbuf
{
  vec4 data;
} cbuf;

layout(set = 0, binding = 11) uniform sampler2D linearSampledImage;

layout(set = 0, binding = 12, std430) buffer storebuftype
{
  vec4 arr[];
} storebuf;

layout(set = 0, binding = 13, rgba32f) uniform coherent image2D storeImage;

layout(set = 1, binding = 5) uniform sampler2D linearSampledImage2;

layout(set = 1, binding = 10, std140) uniform constsbuf2
{
  vec4 data;
} cbuf2;

layout(set = 1, binding = 20, std140) uniform constsbuf3
{
  vec4 data;
} cbuf3;


layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  imageStore(oobImage, push.coord.xy, vec4(1,2,3,4));
  oobbuf.arr[push.coord.z] = vec4(1,2,3,4);
  Color = vertIn.col + storebuf.arr[0] + imageLoad(storeImage, ivec2(0, 0)) + texture(linearSampledImage, vec2(0, 0))
        + texture(linearSampledImage2, vec2(0, 0)) + cbuf.data + cbuf2.data + cbuf3.data
        + vec4(0,1,0,1);
}

)EOSHADER";

  VkPhysicalDeviceRobustness2FeaturesEXT robustnessFeatures = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
  };

  void Prepare(int argc, char **argv)
  {
    // require descriptor indexing
    devExts.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);

    optDevExts.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

    features.robustBufferAccess = VK_TRUE;

    features.fragmentStoresAndAtomics = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    getPhysFeatures2(&robustnessFeatures);

    if(!robustnessFeatures.nullDescriptor)
      Avail = "Feature 'nullDescriptor' not available";

    devInfoNext = &robustnessFeatures;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool KHR_push_descriptor = hasExt(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},

        {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {13, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout;

    if(KHR_push_descriptor)
    {
      VkDescriptorSetLayout pushlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
          {
              {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
              {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
              {20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          },
          VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
          {setlayout, pushlayout},
          {
              vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i)),
          }));
    }
    else
    {
      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
          {setlayout}, {
                           vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i)),
                       }));
    }

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V),
                                                                 vkh::vertexBind(1, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 1, DefaultA2V, col),
        vkh::vertexAttr(2, 1, DefaultA2V, uv),
    };

    // uv will test reading from a NULL buffer with an offset, since we won't be reading data anyway
    // test reading at offset 0
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions[1].offset = 0;

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

    AllocatedImage offimg(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    VkImageView store_view = createImageView(vkh::ImageViewCreateInfo(
        offimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedBuffer store_buffer(this,
                                 vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                 VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    VkSampler pointsampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_NEAREST));

    while(Running())
    {
      vkh::updateDescriptorSets(
          device,
          {
              vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      {vkh::DescriptorBufferInfo(store_buffer.buffer)}),
              vkh::WriteDescriptorSet(
                  descset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                  {vkh::DescriptorImageInfo(store_view, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),

              vkh::WriteDescriptorSet(descset, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      {vkh::DescriptorBufferInfo(VK_NULL_HANDLE)}),
              vkh::WriteDescriptorSet(descset, 11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      {vkh::DescriptorImageInfo(
                                          VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, pointsampler)}),
              vkh::WriteDescriptorSet(descset, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                      {vkh::DescriptorBufferInfo(VK_NULL_HANDLE)}),
              vkh::WriteDescriptorSet(descset, 13, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                      {vkh::DescriptorImageInfo(
                                          VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE)}),
          });

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_GENERAL, offimg.image),
               });

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      Vec4i push = {};

      if(robustnessFeatures.robustBufferAccess2)
      {
        push.z = 1000000;
        setMarker(cmd, "robustBufferAccess2");
      }

      if(robustnessFeatures.robustImageAccess2)
      {
        push.x = push.y = 1000000;
        setMarker(cmd, "robustImageAccess2");
      }

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i), &push);

      if(KHR_push_descriptor)
      {
        vkCmdPushDescriptorSetKHR(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
            vkh::WriteDescriptorSet(
                VK_NULL_HANDLE, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, pointsampler)}));
        vkCmdPushDescriptorSetKHR(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
            vkh::WriteDescriptorSet(VK_NULL_HANDLE, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    {vkh::DescriptorBufferInfo(VK_NULL_HANDLE)}));
        vkCmdPushDescriptorSetKHR(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
            vkh::WriteDescriptorSet(VK_NULL_HANDLE, 20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    {vkh::DescriptorBufferInfo(VK_NULL_HANDLE)}));
      }

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer, VK_NULL_HANDLE}, {0, 0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();

      // idle the device so we can update descriptor sets every frame without needing to
      // double-buffer.
      vkDeviceWaitIdle(device);
    }

    return 0;
  }
};

REGISTER_TEST();
