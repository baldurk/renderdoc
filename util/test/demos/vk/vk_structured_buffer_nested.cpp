/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

TEST(VK_Structured_Buffer_Nested, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Just draws a simple triangle, using normal pipeline. Basic test that can be used "
      "for any dead-simple tests that don't require any particular API use";

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

  const std::string glslpixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

struct supernest
{
  float x;
};

struct nest
{
  vec3 v;
  supernest s;
  float a, b, c;
};

layout(binding = 0, std430) buffer nest_struct_buffer
{
  nest n[3];
  vec4 p;
  nest rtarray[];
} nestbuf;

layout(binding = 1) uniform samplerBuffer plainbuf;

layout(binding = 2, std430) buffer struct_buffer
{
  nest rtarray[];
} structbuf;

layout(binding = 3, std430) buffer output_buffer
{
  vec4 dump[];
} out_buf;

void main()
{
  int idx = 0;
  out_buf.dump[idx++] = vec4(nestbuf.n[0].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.n[1].a, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.n[2].c, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.n[2].s.x, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = nestbuf.p;
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[0].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[3].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[6].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[4].a, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[5].b, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[7].c, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[8].s.x, 0.0f, 0.0f, 1.0f);
  idx++;
  out_buf.dump[idx++] = texelFetch(plainbuf, 3);
  out_buf.dump[idx++] = texelFetch(plainbuf, 4);
  out_buf.dump[idx++] = texelFetch(plainbuf, 5);
  idx++;
  out_buf.dump[idx++] = vec4(structbuf.rtarray[0].v, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[3].v, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[6].v, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[4].a, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[5].b, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[7].c, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[8].s.x, 0.0f, 0.0f, 1.0f);
	Color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

)EOSHADER";

  std::string hlslpixel = R"EOSHADER(

struct supernest
{
  float x;
};

struct nest
{
  float3 v;
  supernest s;
  float a, b, c;
};

struct mystruct
{
  nest n[3];
  float3 p;
};

StructuredBuffer<mystruct> buf1 : register(t0);
Buffer<float3> buf2 : register(t1);

struct dump
{
  float4 val;
};

RWStructuredBuffer<dump> out_buf : register(u3);

float4 main() : SV_Target0
{
  int idx = 0;
  out_buf[idx++].val = float4(buf1[0].p, 1.0f);
  out_buf[idx++].val = float4(buf1[1].p, 1.0f);
  out_buf[idx++].val = float4(buf1[2].p, 1.0f);
  out_buf[idx++].val = float4(buf1[0].n[0].v, 1.0f);
  out_buf[idx++].val = float4(buf1[3].n[1].v, 1.0f);
  out_buf[idx++].val = float4(buf1[6].n[2].v, 1.0f);
  out_buf[idx++].val = float4(buf1[4].n[0].a, 0.0f, 0.0f, 1.0f);
  out_buf[idx++].val = float4(buf1[5].n[1].b, 0.0f, 0.0f, 1.0f);
  out_buf[idx++].val = float4(buf1[7].n[2].c, 0.0f, 0.0f, 1.0f);
  out_buf[idx++].val = float4(buf1[8].n[1].s.x, 0.0f, 0.0f, 1.0f);
  idx++;
  out_buf[idx++].val = float4(buf2[3], 1.0f);
  out_buf[idx++].val = float4(buf2[4], 1.0f);
  out_buf[idx++].val = float4(buf2[5], 1.0f);
  return 1.0f.xxxx;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    features.fragmentStoresAndAtomics = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

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
        CompileShaderModule(common + glslpixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline glslpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[1] =
        CompileShaderModule(hlslpixel, ShaderLang::hlsl, ShaderStage::frag, "main");

    VkPipeline hlslpipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    float data[16 * 100];

    for(int i = 0; i < 16 * 100; i++)
      data[i] = float(i);

    AllocatedBuffer ssbo(
        allocator, vkh::BufferCreateInfo(sizeof(data), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                           VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    ssbo.upload(data);

    AllocatedBuffer out_ssbo(allocator,
                             vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                             VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    VkBufferView bufview =
        createBufferView(vkh::BufferViewCreateInfo(ssbo.buffer, VK_FORMAT_R32G32B32_SFLOAT));

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(ssbo.buffer)}),
            vkh::WriteDescriptorSet(descset, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, {bufview}),
            vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(ssbo.buffer)}),
            vkh::WriteDescriptorSet(descset, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(out_ssbo.buffer)}),
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

      vkh::cmdPipelineBarrier(
          cmd, {},
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_TRANSFER_WRITE_BIT, out_ssbo.buffer),
          });

      vkCmdFillBuffer(cmd, out_ssbo.buffer, 0, 1024, 0);

      vkh::cmdPipelineBarrier(
          cmd, {},
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       out_ssbo.buffer),
          });

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      vkh::cmdPipelineBarrier(
          cmd, {},
          {
              vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                       out_ssbo.buffer),
          });

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hlslpipe);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
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
