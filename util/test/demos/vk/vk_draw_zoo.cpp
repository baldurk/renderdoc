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

TEST(VK_Draw_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws several variants using different vertex/index offsets.";

  std::string common = R"EOSHADER(

#version 420 core

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
  float vertidx;
  float instidx;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
  vertOut.pos.x += 0.1f*gl_InstanceIndex - 0.5f;
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);

  vertOut.vertidx = float(gl_VertexIndex);
  vertOut.instidx = float(gl_InstanceIndex);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
  Color.g = vertIn.vertidx/30.0f;
  Color.b = vertIn.instidx/10.0f;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

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

    VkPipeline noInstPipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.inputAssemblyState.primitiveRestartEnable = true;
    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipeline stripPipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.inputAssemblyState.primitiveRestartEnable = false;
    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // add an instance vertex buffer for colours
    pipeCreateInfo.vertexInputState.vertexBindingDescriptions.push_back(vkh::instanceBind(1, Vec4f));
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions[1].binding = 1;
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions[1].offset = 0;

    VkPipeline instPipe = createGraphicsPipeline(pipeCreateInfo);

    DefaultA2V vertData[] = {
        // 0
        {Vec3f(-1.0f, -1.0f, -1.0f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(-1.0f, -1.0f)},
        // 1, 2, 3
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // 4, 5, 6
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // 7, 8, 9
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // 10, 11, 12
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // strips: 13, 14, 15, ...
        {Vec3f(-0.5f, 0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.2f, 0.1f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.2f, 0.0f), Vec4f(0.4f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.3f, 0.0f, 0.0f), Vec4f(0.6f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.2f, 0.0f), Vec4f(0.8f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.0f, 0.0f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.1f, 0.2f, 0.0f), Vec4f(0.0f, 0.8f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.1f, 0.0f, 0.0f), Vec4f(0.2f, 0.1f, 0.5f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.3f, 0.2f, 0.0f), Vec4f(0.4f, 0.3f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.3f, 0.0f, 0.0f), Vec4f(0.6f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, 0.2f, 0.0f), Vec4f(0.8f, 0.3f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb1(allocator, vkh::BufferCreateInfo(sizeof(DefaultA2V) * 66000,
                                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    {
      DefaultA2V *src = (DefaultA2V *)vertData;
      DefaultA2V *dst = (DefaultA2V *)vb1.map();

      memset(dst, 0x5c, sizeof(DefaultA2V) * 66000);

      // up-pointing triangle to offset 0
      memcpy(dst + 0, src + 1, sizeof(DefaultA2V));
      memcpy(dst + 1, src + 2, sizeof(DefaultA2V));
      memcpy(dst + 2, src + 3, sizeof(DefaultA2V));

      // invalid vert for index 3 and 4
      memcpy(dst + 3, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 4, src + 0, sizeof(DefaultA2V));

      // down-pointing triangle at offset 5
      memcpy(dst + 5, src + 4, sizeof(DefaultA2V));
      memcpy(dst + 6, src + 5, sizeof(DefaultA2V));
      memcpy(dst + 7, src + 6, sizeof(DefaultA2V));

      // invalid vert for 8 - 12
      memcpy(dst + 8, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 9, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 10, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 11, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 12, src + 0, sizeof(DefaultA2V));

      // left-pointing triangle data to offset 13
      memcpy(dst + 13, src + 7, sizeof(DefaultA2V));
      memcpy(dst + 14, src + 8, sizeof(DefaultA2V));
      memcpy(dst + 15, src + 9, sizeof(DefaultA2V));

      // invalid vert for 16-22
      memcpy(dst + 16, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 17, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 18, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 19, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 20, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 21, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 22, src + 0, sizeof(DefaultA2V));

      // right-pointing triangle data to offset 23
      memcpy(dst + 23, src + 10, sizeof(DefaultA2V));
      memcpy(dst + 24, src + 11, sizeof(DefaultA2V));
      memcpy(dst + 25, src + 12, sizeof(DefaultA2V));

      // strip after 30
      memcpy(dst + 30, src + 13, sizeof(DefaultA2V));
      memcpy(dst + 31, src + 14, sizeof(DefaultA2V));
      memcpy(dst + 32, src + 15, sizeof(DefaultA2V));
      memcpy(dst + 33, src + 16, sizeof(DefaultA2V));
      memcpy(dst + 34, src + 17, sizeof(DefaultA2V));
      memcpy(dst + 35, src + 18, sizeof(DefaultA2V));
      memcpy(dst + 36, src + 19, sizeof(DefaultA2V));
      memcpy(dst + 37, src + 20, sizeof(DefaultA2V));
      memcpy(dst + 38, src + 21, sizeof(DefaultA2V));
      memcpy(dst + 39, src + 22, sizeof(DefaultA2V));
      memcpy(dst + 40, src + 23, sizeof(DefaultA2V));
      memcpy(dst + 41, src + 24, sizeof(DefaultA2V));

      vb1.unmap();
    }

    AllocatedBuffer vb2(
        allocator, vkh::BufferCreateInfo(sizeof(Vec4f) * 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    {
      Vec4f *dst = (Vec4f *)vb2.map();

      memset(dst, 0, sizeof(Vec4f) * 60);

      dst[0] = Vec4f(0.0f, 0.5f, 1.0f, 1.0f);
      dst[1] = Vec4f(1.0f, 0.5f, 0.0f, 1.0f);

      dst[5] = Vec4f(1.0f, 0.5f, 0.5f, 1.0f);
      dst[6] = Vec4f(0.5f, 0.5f, 1.0f, 1.0f);

      dst[13] = Vec4f(0.1f, 0.8f, 0.3f, 1.0f);
      dst[14] = Vec4f(0.8f, 0.1f, 0.1f, 1.0f);

      vb2.unmap();
    }

    AllocatedBuffer ib1(allocator, vkh::BufferCreateInfo(sizeof(uint32_t) * 100,
                                                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    {
      uint16_t *dst = (uint16_t *)ib1.map();

      memset(dst, 0, sizeof(uint32_t) * 100);

      dst[0] = 0;
      dst[1] = 1;
      dst[2] = 2;

      dst[5] = 5;
      dst[6] = 6;
      dst[7] = 7;

      dst[13] = 63;
      dst[14] = 64;
      dst[15] = 65;

      dst[23] = 103;
      dst[24] = 104;
      dst[25] = 105;

      dst[37] = 104;
      dst[38] = 105;
      dst[39] = 106;

      dst[42] = 30;
      dst[43] = 31;
      dst[44] = 32;
      dst[45] = 33;
      dst[46] = 34;
      dst[47] = 0xffff;
      dst[48] = 36;
      dst[49] = 37;
      dst[50] = 38;
      dst[51] = 39;
      dst[52] = 40;
      dst[53] = 41;

      dst[54] = 130;
      dst[55] = 131;
      dst[56] = 132;
      dst[57] = 133;
      dst[58] = 134;
      dst[59] = 0xffff;
      dst[60] = 136;
      dst[61] = 137;
      dst[62] = 138;
      dst[63] = 139;
      dst[64] = 140;
      dst[65] = 141;

      ib1.unmap();
    }

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

      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);

      VkViewport vp = mainWindow->viewport;
      vp.width = 48.0f;
      vp.height = 48.0f;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, noInstPipe);

      ///////////////////////////////////////////////////
      // non-indexed, non-instanced

      // basic test
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vp.x += vp.width;

      // test with vertex offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 5, 0);
      vp.x += vp.width;

      // test with vertex offset and vbuffer offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {5 * sizeof(DefaultA2V)});
      vkCmdDraw(cmd, 3, 1, 8, 0);
      vp.x += vp.width;

      // adjust to next row
      vp.x = 0.0f;
      vp.y += vp.height;

      ///////////////////////////////////////////////////
      // indexed, non-instanced

      // basic test
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);
      vp.x += vp.width;

      // test with first index
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 1, 5, 0, 0);
      vp.x += vp.width;

      // test with first index and vertex offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 1, 13, -50, 0);
      vp.x += vp.width;

      // test with first index and vertex offset and vbuffer offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {10 * sizeof(DefaultA2V)});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 1, 23, -100, 0);
      vp.x += vp.width;

      // test with first index and vertex offset and vbuffer offset and ibuffer offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {19 * sizeof(DefaultA2V)});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 14 * sizeof(uint16_t), VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 1, 23, -100, 0);
      vp.x += vp.width;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stripPipe);

      // indexed strip with primitive restart
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 12, 1, 42, 0, 0);
      vp.x += vp.width;

      // indexed strip with primitive restart and vertex offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer}, {0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 12, 1, 54, -100, 0);
      vp.x += vp.width;

      // adjust to next row
      vp.x = 0.0f;
      vp.y += vp.height;

      ///////////////////////////////////////////////////
      // non-indexed, instanced

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, instPipe);

      // basic test
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer, vb2.buffer}, {0, 0});
      vkCmdDraw(cmd, 3, 2, 0, 0);
      vp.x += vp.width;

      // basic test with first instance
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer, vb2.buffer}, {5 * sizeof(DefaultA2V), 0});
      vkCmdDraw(cmd, 3, 2, 0, 5);
      vp.x += vp.width;

      // basic test with first instance and instance buffer offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer, vb2.buffer},
                                {13 * sizeof(DefaultA2V), 8 * sizeof(Vec4f)});
      vkCmdDraw(cmd, 3, 2, 0, 5);
      vp.x += vp.width;

      // adjust to next row
      vp.x = 0.0f;
      vp.y += vp.height;

      ///////////////////////////////////////////////////
      // indexed, instanced

      // basic test
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer, vb2.buffer}, {0, 0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 2, 5, 0, 0);
      vp.x += vp.width;

      // basic test with first instance
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer, vb2.buffer}, {0, 0});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 2, 13, -50, 5);
      vp.x += vp.width;

      // basic test with first instance and instance buffer offset
      vkCmdSetViewport(cmd, 0, 1, &vp);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb1.buffer, vb2.buffer}, {0, 8 * sizeof(Vec4f)});
      vkCmdBindIndexBuffer(cmd, ib1.buffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd, 3, 2, 23, -80, 5);
      vp.x += vp.width;

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
