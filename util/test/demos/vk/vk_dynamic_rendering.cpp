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

RD_TEST(VK_Dynamic_Rendering, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests use of the VK_Dynamic_Rendering extension and some interactions with other "
      "functionality.";

  std::string geom = R"EOSHADER(
#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in v2f_block
{
	vec4 pos;
	vec4 col;
	vec4 uv;
} gin[3];

layout(location = 0) out g2f_block
{
	vec4 pos;
	vec4 col;
	vec4 uv;
} gout;

void main()
{
  for(int i = 0; i < 3; i++)
  {
    gl_Position = gl_in[i].gl_Position;

    gout.pos = gin[i].pos;
    gout.col = gin[i].col;
    gout.uv = gin[i].uv;

    EmitVertex();
  }
  EndPrimitive();
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

#version 460 core

#extension GL_EXT_nonuniform_qualifier : require

layout(push_constant) uniform PushData
{
  uint bufidx;
} push;

layout(binding = 0, std430) buffer outbuftype {
  vec4 col;
} outbuf[];

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = outbuf[push.bufidx].col;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    devExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    features.geometryShader = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceDynamicRenderingFeaturesKHR dynFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        NULL,
        VK_TRUE,
    };

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexing = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    getPhysFeatures2(&descIndexing);

    if(!descIndexing.descriptorBindingPartiallyBound)
      Avail = "Descriptor indexing feature 'descriptorBindingPartiallyBound' not available";
    else if(!descIndexing.runtimeDescriptorArray)
      Avail = "Descriptor indexing feature 'runtimeDescriptorArray' not available";

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingEnable = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };

    descIndexingEnable.descriptorBindingPartiallyBound = VK_TRUE;
    descIndexingEnable.runtimeDescriptorArray = VK_TRUE;

    devInfoNext = &descIndexingEnable;
    descIndexingEnable.pNext = &dynFeats;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorBindingFlagsEXT bindFlags[] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descFlags = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
    };

    descFlags.bindingCount = ARRAY_COUNT(bindFlags);
    descFlags.pBindingFlags = bindFlags;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(
        vkh::DescriptorSetLayoutCreateInfo({
                                               {
                                                   0,
                                                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                   128,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                                               },
                                           })
            .next(&descFlags));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {
            setlayout,
        },
        {
            vkh::PushConstantRange(VK_SHADER_STAGE_ALL, 0, sizeof(uint32_t)),
        }));

    Vec4f cbufferdata[64] = {};
    cbufferdata[0] = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    cb.upload(cbufferdata);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    const uint32_t ssboIdx = 17;

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, ssboIdx, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(cb.buffer)}),
                });

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    VkPipelineRenderingCreateInfoKHR dynRendInfo = {};
    dynRendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    dynRendInfo.viewMask = 0;
    dynRendInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    dynRendInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat outFormats[] = {mainWindow->format};
    dynRendInfo.pColorAttachmentFormats = outFormats;
    dynRendInfo.colorAttachmentCount = ARRAY_COUNT(outFormats);

    pipeCreateInfo.pNext = &dynRendInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = VK_NULL_HANDLE;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
        CompileShaderModule(geom, ShaderLang::glsl, ShaderStage::geom, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

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

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, swapimg),
               });

      VkRenderingAttachmentInfoKHR colAtt = {
          VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
          NULL,
          mainWindow->GetView(),
          VK_IMAGE_LAYOUT_GENERAL,
          VK_RESOLVE_MODE_NONE,
          VK_NULL_HANDLE,
          VK_IMAGE_LAYOUT_GENERAL,
          VK_ATTACHMENT_LOAD_OP_LOAD,
          VK_ATTACHMENT_STORE_OP_STORE,
          vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f),
      };

      VkRenderingInfoKHR rendInfo = {
          VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
          NULL,
          0,
          mainWindow->scissor,
          1,
          0,
          1,
          &colAtt,
          NULL,
          NULL,
      };

      vkCmdBeginRenderingKHR(cmd, &rendInfo);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});

      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 4, &ssboIdx);

      setMarker(cmd, "Draw 0");

      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderingKHR(cmd);

      VkCommandBuffer cmd2 = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);

      {
        VkCommandBufferInheritanceInfo inherit = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};

        vkBeginCommandBuffer(cmd2, vkh::CommandBufferBeginInfo(0, &inherit));

        vkCmdBeginRenderingKHR(cmd2, &rendInfo);

        VkViewport v = mainWindow->viewport;
        v.width /= 2;
        v.height /= 2;

        vkCmdBindPipeline(cmd2, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdSetViewport(cmd2, 0, 1, &v);
        vkCmdSetScissor(cmd2, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd2, 0, {vb.buffer}, {0});
        vkh::cmdBindDescriptorSets(cmd2, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});

        vkCmdPushConstants(cmd2, layout, VK_SHADER_STAGE_ALL, 0, 4, &ssboIdx);

        setMarker(cmd2, "Draw 1");

        vkCmdDraw(cmd2, 3, 1, 0, 0);

        vkCmdEndRenderingKHR(cmd2);

        vkEndCommandBuffer(cmd2);
      }

      vkCmdExecuteCommands(cmd, 1, &cmd2);

      FinishUsingBackbuffer(cmd, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd}, {cmd2});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
