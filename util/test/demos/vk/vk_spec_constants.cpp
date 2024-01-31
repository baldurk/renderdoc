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

RD_TEST(VK_Spec_Constants, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests using the same shader multiple times with specialisation constants";

  const std::string vertex = R"EOSHADER(
#version 420 core

layout(location = 0) in vec3 Position;

void main()
{
	gl_Position = vec4(Position.xyz*vec3(1,-1,1), 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

// glslang won't let us compile with a large value! we expand this by patching the SPIR-V
layout(constant_id = 1234) const float some_float = 1.5;
layout(constant_id = 5) const int NOT_numcols = 999;
layout(constant_id = 0) const int numcols = 0;

layout(set = 0, binding = 0, std140) uniform constsbuf
{
  vec4 col[numcols+1];
};

void main()
{
  Color = vec4(0,0,0,1);
  for(int i=0; i < numcols; i++)
    Color += col[i];
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
    };

    VkShaderModule frag = VK_NULL_HANDLE;

    static const uint32_t largeConstID = 0xfedb4231;

    {
      std::vector<uint32_t> spirv = ::CompileShaderToSpv(
          pixel, SPIRVTarget::vulkan, ShaderLang::glsl, ShaderStage::frag, "main", {});

      if(spirv.empty())
        return 4;

      // do a super quick patch to find the OpDecorate (opcode 71) decorating constant ID 1234 and
      // patch it to the one we really want, which glslang can't compile
      for(size_t offs = 5; offs < spirv.size();)
      {
        uint32_t numWords = spirv[offs] >> 16;

        if((spirv[offs] & 0xffff) == 71 && spirv[offs + 3] == 1234)
        {
          spirv[offs + 3] = largeConstID;
          break;
        }

        offs += numWords;
      }

      CHECK_VKR(vkCreateShaderModule(device, vkh::ShaderModuleCreateInfo(spirv), NULL, &frag));

      shaders.push_back(frag);
    }

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        vkh::PipelineShaderStageCreateInfo(frag, VK_SHADER_STAGE_FRAGMENT_BIT, "main"),
    };

    struct SpecData
    {
      int NOT_numcols;
      int numcols;
      float floatval;
    } specdata = {};

    std::vector<VkSpecializationMapEntry> specmap;

    VkSpecializationInfo spec = {};
    spec.dataSize = sizeof(specdata);
    spec.pData = &specdata;

    pipeCreateInfo.stages[1].pSpecializationInfo = &spec;

    VkPipeline pipe[4] = {};

    {
      specmap = {
          {0, offsetof(SpecData, numcols), sizeof(uint32_t)},
      };
      specdata.numcols = 0;

      spec.mapEntryCount = (uint32_t)specmap.size();
      spec.pMapEntries = specmap.data();

      pipe[0] = createGraphicsPipeline(pipeCreateInfo);
    }

    {
      specmap = {
          {largeConstID, offsetof(SpecData, floatval), sizeof(uint32_t)},
          {0, offsetof(SpecData, numcols), sizeof(uint32_t)},
      };
      specdata.numcols = 1;
      specdata.floatval = 2.5f;

      spec.mapEntryCount = (uint32_t)specmap.size();
      spec.pMapEntries = specmap.data();

      pipe[1] = createGraphicsPipeline(pipeCreateInfo);
    }

    {
      specmap = {
          {largeConstID, offsetof(SpecData, floatval), sizeof(uint32_t)},
          {0, offsetof(SpecData, numcols), sizeof(uint32_t)},
          {5, offsetof(SpecData, NOT_numcols), sizeof(uint32_t)},
      };
      specdata.numcols = 2;
      specdata.NOT_numcols = 9999;
      specdata.floatval = 16.5f;

      spec.mapEntryCount = (uint32_t)specmap.size();
      spec.pMapEntries = specmap.data();

      pipe[2] = createGraphicsPipeline(pipeCreateInfo);
    }

    {
      specmap = {
          {0, offsetof(SpecData, numcols), sizeof(uint32_t)},
      };
      specdata.numcols = 3;

      spec.mapEntryCount = (uint32_t)specmap.size();
      spec.pMapEntries = specmap.data();

      pipe[3] = createGraphicsPipeline(pipeCreateInfo);
    }

    Vec4f cbufferdata[4] = {
        Vec4f(1.0f, 0.0f, 0.0f, 0.0f),
        Vec4f(0.0, 1.0f, 0.0f, 0.0f),
        Vec4f(0.0, 0.0f, 1.0f, 0.0f),
    };

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(cbufferdata);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(cb.buffer)}),
                });

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

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      VkViewport v = mainWindow->viewport;
      v.width /= ARRAY_COUNT(pipe);

      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      for(size_t i = 0; i < ARRAY_COUNT(pipe); i++)
      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe[i]);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descset, 0,
                                NULL);
        vkCmdSetViewport(cmd, 0, 1, &v);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        v.x += v.width;
      }

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
