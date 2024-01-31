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

RD_TEST(VK_Shader_Editing, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that shader editing works with different combinations of shader re-use.";

  const char *vertex = R"EOSHADER(
#version 430 core

layout(location = 0) in vec3 Position;

void main()
{
	gl_Position = vec4(Position.xyz, 1);
}

)EOSHADER";

  const char *pixel = R"EOSHADER(
#version 430 core

layout(location = 0, index = 0) out vec4 Color;

layout(constant_id = 1) const int spec_canary = 0;

void main()
{
  if(spec_canary != 1337) { Color = vec4(0.2, 0.0, 0.2, 1.0); return; }

#if 1
	Color = vec4(0.0, 1.0, 0.0, 1.0);
#else
	Color = vec4(0.0, 1.0, 1.0, 1.0);
#endif
}

)EOSHADER";

  const std::string comp = R"EOSHADER(

struct PushData
{
  uint4 data;
};

[[vk::push_constant]]
ConstantBuffer<PushData> push;

StructuredBuffer<uint4> inbuf : register(b0);
RWStructuredBuffer<uint4> outbuf : register(b1);

[numthreads(1, 1, 1)]
void hlsl_main ()
{
  outbuf[0].x += inbuf[0].x * push.data.x;
  outbuf[0].y += inbuf[0].y * push.data.y;
  outbuf[0].z += inbuf[0].z * push.data.z;
  outbuf[0].w += inbuf[0].w * push.data.w;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {imgview}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkSpecializationMapEntry specmap[1] = {
        {1, 0 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    uint32_t specvals[1] = {1337};

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = ARRAY_COUNT(specmap);
    spec.pMapEntries = specmap;
    spec.dataSize = sizeof(specvals);
    spec.pData = specvals;

    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    // use the same source but make a distinct shader module so we can edit it separately
    pipeCreateInfo.stages[1] =
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main");
    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;

    VkPipeline pipe2 = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    VkDescriptorSetLayout compSetLayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        }));

    VkPipelineLayout compLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {compSetLayout}, {
                             vkh::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Vec4i)),
                         }));

    VkPipeline compPipe1 = createComputePipeline(vkh::ComputePipelineCreateInfo(
        compLayout, CompileShaderModule(comp, ShaderLang::hlsl, ShaderStage::comp, "hlsl_main")));

    AllocatedBuffer bufin(this,
                          vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    AllocatedBuffer bufout(this,
                           vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    setName(bufin.buffer, "bufin");
    setName(bufout.buffer, "bufout");

    VkDescriptorSet compSet = allocateDescriptorSet(compSetLayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(compSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(bufin.buffer)}),
                    vkh::WriteDescriptorSet(compSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(bufout.buffer)}),
                });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor,
                                                    {vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      VkViewport v = mainWindow->viewport;
      v.width /= 2.0f;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "Draw 1");
      vkCmdDraw(cmd, 3, 1, 0, 0);

      v.x += v.width;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe2);
      vkCmdSetViewport(cmd, 0, 1, &v);
      setMarker(cmd, "Draw 2");
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                           VK_IMAGE_LAYOUT_GENERAL, img.image),
               });

      blitToSwap(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL, swapimg, VK_IMAGE_LAYOUT_GENERAL);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      // Fill bufin with 111
      vkCmdFillBuffer(cmd, bufin.buffer, 0, 1024, 111);
      // Fill bufout with 222
      vkCmdFillBuffer(cmd, bufout.buffer, 0, 1024, 222);
      vkh::cmdPipelineBarrier(
          cmd, {},
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                       bufin.buffer, 0, 1024),
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                       bufout.buffer, 0, 1024),
          });

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compPipe1);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compLayout, 0, {compSet}, {});

      Vec4i push = {5, 6, 7, 8};
      vkCmdPushConstants(cmd, compLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Vec4i), &push);
      setMarker(cmd, "Pre-Dispatch");
      vkCmdDispatch(cmd, 1, 1, 1);
      setMarker(cmd, "Post-Dispatch");

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
