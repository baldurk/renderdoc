/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2026 Baldur Karlsson
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

RD_TEST(VK_Storage_Raster, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Testing stores to images from a rasterized draw with no rasterized outputs.";

  std::string common = R"EOSHADER(

#version 450 core


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

layout(set = 0, binding = 0, std430) buffer atomicbuftype
{
  uint data[];
} atomicbuf;

void main()
{
  atomicMin(atomicbuf.data[0], int(gl_FragCoord.x));
  atomicMin(atomicbuf.data[1], int(gl_FragCoord.y));
  atomicMax(atomicbuf.data[2], int(gl_FragCoord.x));
  atomicMax(atomicbuf.data[3], int(gl_FragCoord.y));
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    features.fragmentStoresAndAtomics = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    AllocatedBuffer store_buffer(this,
                                 vkh::BufferCreateInfo(64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                 VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkRenderPass noAttRP;

    vkh::RenderPassCreator renderPassCreateInfo;
    renderPassCreateInfo.addSubpass({});
    noAttRP = createRenderPass(renderPassCreateInfo);

    VkExtent2D fbRect = {(uint32_t)screenWidth, (uint32_t)screenHeight};

    VkFramebuffer noAttFB = createFramebuffer(vkh::FramebufferCreateInfo(noAttRP, {}, fbRect));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = noAttRP;

    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

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

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(store_buffer.buffer)}),
                });

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkCmdFillBuffer(cmd, store_buffer.buffer, 0, 64, 150);

      vkh::cmdPipelineBarrier(
          cmd, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                    store_buffer.buffer)});

      VkImage swapimg = StartUsingBackbuffer(cmd);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(noAttRP, noAttFB, mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      mainWindow->setViewScissor(cmd);
      vkh::cmdBindVertexBuffers(cmd, {DefaultTriVB.buffer});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd);

      vkEndCommandBuffer(cmd);

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
