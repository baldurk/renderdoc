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

TEST(VK_Indirect, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests different indirect drawing and dispatching functions, including parameters that are "
      "generated on the GPU and not known on the CPU at submit time";

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

  const std::string compute = R"EOSHADER(

#version 430 core

#extension GL_ARB_compute_shader : require

layout (local_size_x = 2, local_size_y = 2, local_size_z = 1) in;

layout(push_constant) uniform PushConstants {
	uint mode;
} push;

layout(binding = 0, std140) buffer general_buffer
{
	uvec4 data[];
} ssbo;

void main()
{
  if(push.mode == 0)
  {
    // this should never run, since the dispatch is indirect 0,0,0
    ssbo.data[0] = uvec4(99, 88, 77, 66);
  }
  else if(push.mode == 1)
  {
    // see below, here we write the indirect dispatch parameters
    ssbo.data[1] = uvec4(3, 4, 5, 999999);
  }
  else if(push.mode == 2)
  {
    // see below, in the indirect dispatch we write data in for each thread
    uint idx = gl_GlobalInvocationID.z * (3 * 2) * (4 * 2) +
               gl_GlobalInvocationID.y * (3 * 2) +
               gl_GlobalInvocationID.x;

    ssbo.data[100+idx] = uvec4(gl_GlobalInvocationID, 12345);

    // we also write the draw parameters for non-indexed and indexed draws.
    // The indices point just after the vertices, so we have all unique draws

    // vkCmdDrawIndirect()
    ssbo.data[2] = uvec4(3, 2, 0, 7); // draw verts 0..2

    // vkCmdDrawIndexedIndirect() (2 draws)
    ssbo.data[3] = uvec4(3, 3, 0, 0); // draw indices 0..2
    ssbo.data[4].x = 19;
    ssbo.data[5] = uvec4(6, 2, 3, 0); // draw indices 3..8
    ssbo.data[6].x = 15;

    // write count parameters for indirect count draws, although we might not need these
    // 1 draw for non-indexed, 3 draws for indexed.
    ssbo.data[10] = uvec4(1, 3, 0, 0);

    // vkCmdDrawIndirectCountKHR()
    ssbo.data[11] = uvec4(3, 4, 3, 4); // draw verts 3..5

    // vkCmdDrawIndexedIndirectCountKHR()
    ssbo.data[12] = uvec4(3, 1, 9, 0); // draw indices 9..11
    ssbo.data[13].x = 1;
    ssbo.data[14] = uvec4(0, 0, 99, 1010); // draw nothing (index/instance count 0)
    ssbo.data[15].x = 200;
    ssbo.data[16] = uvec4(6, 2, 12, 0); // draw indices 12..17
    ssbo.data[17].x = 1;
  }
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    features.multiDrawIndirect = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    optDevExts.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool KHR_draw_indirect_count =
        std::find(devExts.begin(), devExts.end(), VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) !=
        devExts.end();

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    }));

    VkPipelineLayout complayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setlayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 4)}));

    VkPipelineLayout drawlayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = drawlayout;
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

    VkPipeline drawpipe = createGraphicsPipeline(pipeCreateInfo);

    VkPipeline comppipe = createComputePipeline(vkh::ComputePipelineCreateInfo(
        complayout, CompileShaderModule(compute, ShaderLang::glsl, ShaderStage::comp, "main")));

    const DefaultA2V vbdata[24] = {
        // non-indexed indirect draw
        {Vec3f(-0.8f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.7f, 0.8f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.6f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // non-indexed KHR_draw_indirect_count draw
        {Vec3f(-0.8f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.7f, -0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.6f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // indexed indirect draw 1
        {Vec3f(-0.6f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.8f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // indexed indirect draw 2
        {Vec3f(-0.4f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.8f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.2f, 0.8f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.8f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.1f, 0.8f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // indexed KHR_draw_indirect_count draw 1
        {Vec3f(-0.6f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, -0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // indexed KHR_draw_indirect_count draw 2
        // empty

        // indexed indirect draw 3
        {Vec3f(-0.4f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, -0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.2f, -0.2f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.1f, -0.2f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(allocator,
                       vkh::BufferCreateInfo(sizeof(vbdata), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(vbdata);

    uint32_t indices[18] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    AllocatedBuffer ib(allocator,
                       vkh::BufferCreateInfo(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    ib.upload(indices);

    VkDeviceSize ssbo_size = 16 * 1024;

    AllocatedBuffer ssbo(allocator,
                         vkh::BufferCreateInfo(ssbo_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                         VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    memset(ssbo.map(), 0, (size_t)ssbo_size);
    ssbo.unmap();

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(ssbo.buffer)}),
                });

    using uvec4 = uint32_t[4];

    while(Running())
    {
      VkCommandBuffer primary = GetCommandBuffer();

      vkBeginCommandBuffer(primary, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(primary, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      setMarker(primary, "Do Clear");

      vkCmdClearColorImage(primary, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      {
        VkCommandBuffer cmd = primary;

        pushMarker(cmd, "Primary: Dispatches");

        vkh::cmdPipelineBarrier(
            cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_ACCESS_INDIRECT_COMMAND_READ_BIT, ssbo.buffer)});

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, comppipe);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, complayout, 0, {descset}, {});

        uint32_t mode = 0;
        vkCmdPushConstants(cmd, complayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

        // dispatch 0,0,0
        vkCmdDispatchIndirect(cmd, ssbo.buffer, 8 * sizeof(uvec4));

        mode = 1;
        vkCmdPushConstants(cmd, complayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

        // dispatch to fill the actual parameters
        vkCmdDispatch(cmd, 1, 1, 1);

        vkh::cmdPipelineBarrier(
            cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_ACCESS_INDIRECT_COMMAND_READ_BIT, ssbo.buffer)});

        mode = 2;
        vkCmdPushConstants(cmd, complayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

        // indirect dispatch at offset data[1], see above shader
        vkCmdDispatchIndirect(cmd, ssbo.buffer, sizeof(uvec4));

        popMarker(cmd);

        vkh::cmdPipelineBarrier(
            cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                               VK_ACCESS_INDIRECT_COMMAND_READ_BIT, ssbo.buffer)});
      }

      vkCmdBeginRenderPass(primary, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(),
                                                             mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_INLINE);

      {
        VkCommandBuffer cmd = primary;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawpipe);
        vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

        pushMarker(cmd, "Primary: Empty draws");
        vkCmdDrawIndirect(cmd, ssbo.buffer, 2 * sizeof(uvec4), 0, sizeof(uvec4));
        vkCmdDrawIndexedIndirect(cmd, ssbo.buffer, 3 * sizeof(uvec4), 0, 2 * sizeof(uvec4));
        popMarker(cmd);

        pushMarker(cmd, "Primary: Indirect draws");

        // indirect draw at offset data[2], see above shader
        vkCmdDrawIndirect(cmd, ssbo.buffer, 2 * sizeof(uvec4), 1, sizeof(uvec4));

        // indirect indexed draw at offset data[3], see above shader
        vkCmdDrawIndexedIndirect(cmd, ssbo.buffer, 3 * sizeof(uvec4), 2, 2 * sizeof(uvec4));

        popMarker(cmd);

        // if we have KHR_draw_indirect_count, test it as well
        if(KHR_draw_indirect_count)
        {
          pushMarker(cmd, "Primary: KHR_draw_indirect_count");

          pushMarker(cmd, "Primary: Empty count draws");
          // empty draws
          vkCmdDrawIndirectCountKHR(cmd, ssbo.buffer, 11 * sizeof(uvec4), ssbo.buffer,
                                    10 * sizeof(uvec4), 0, sizeof(uvec4));
          vkCmdDrawIndexedIndirectCountKHR(cmd, ssbo.buffer, 12 * sizeof(uvec4), ssbo.buffer,
                                           10 * sizeof(uvec4) + sizeof(uint32_t), 0,
                                           sizeof(uvec4) * 2);
          popMarker(cmd);

          pushMarker(cmd, "Primary: Indirect count draws");
          vkCmdDrawIndirectCountKHR(cmd, ssbo.buffer, 11 * sizeof(uvec4), ssbo.buffer,
                                    10 * sizeof(uvec4), 10, sizeof(uvec4));
          vkCmdDrawIndexedIndirectCountKHR(cmd, ssbo.buffer, 12 * sizeof(uvec4), ssbo.buffer,
                                           10 * sizeof(uvec4) + sizeof(uint32_t), 10,
                                           sizeof(uvec4) * 2);
          popMarker(cmd);

          popMarker(cmd);
        }
      }

      vkCmdEndRenderPass(primary);

      vkh::cmdPipelineBarrier(
          primary, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, ssbo.buffer)});

      // clear the buffer so that we can't read any of the data back from outside the command buffer
      vkCmdFillBuffer(primary, ssbo.buffer, 0, ssbo_size, 0);

      vkEndCommandBuffer(primary);

      Submit(0, 2, {primary});

      vkDeviceWaitIdle(device);

      // now do the same in secondary command buffers

      primary = GetCommandBuffer();

      vkBeginCommandBuffer(primary, vkh::CommandBufferBeginInfo());

      vkCmdClearColorImage(primary, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      VkCommandBuffer dispatch_secondary = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);

      vkBeginCommandBuffer(
          dispatch_secondary,
          vkh::CommandBufferBeginInfo(0, vkh::CommandBufferInheritanceInfo(VK_NULL_HANDLE, 0)));

      {
        VkCommandBuffer cmd = dispatch_secondary;

        pushMarker(cmd, "Secondary: Dispatches");

        vkh::cmdPipelineBarrier(
            cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_ACCESS_INDIRECT_COMMAND_READ_BIT, ssbo.buffer)});

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, comppipe);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, complayout, 0, {descset}, {});

        uint32_t mode = 0;
        vkCmdPushConstants(cmd, complayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

        // dispatch 0,0,0
        vkCmdDispatchIndirect(cmd, ssbo.buffer, 8 * sizeof(uvec4));

        mode = 1;
        vkCmdPushConstants(cmd, complayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

        // dispatch to fill the actual parameters
        vkCmdDispatch(cmd, 1, 1, 1);

        vkh::cmdPipelineBarrier(
            cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                               VK_ACCESS_INDIRECT_COMMAND_READ_BIT, ssbo.buffer)});

        mode = 2;
        vkCmdPushConstants(cmd, complayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

        // indirect dispatch at offset data[1], see above shader
        vkCmdDispatchIndirect(cmd, ssbo.buffer, sizeof(uvec4));

        popMarker(cmd);

        vkh::cmdPipelineBarrier(
            cmd, {}, {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                               VK_ACCESS_INDIRECT_COMMAND_READ_BIT, ssbo.buffer)});
      }

      vkEndCommandBuffer(dispatch_secondary);

      vkCmdExecuteCommands(primary, 1, &dispatch_secondary);

      vkCmdBeginRenderPass(primary, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(),
                                                             mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

      VkCommandBuffer draw_secondary = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);

      vkBeginCommandBuffer(draw_secondary, vkh::CommandBufferBeginInfo(
                                               VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                                               vkh::CommandBufferInheritanceInfo(mainWindow->rp, 0)));

      {
        VkCommandBuffer cmd = draw_secondary;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawpipe);
        vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

        pushMarker(cmd, "Secondary: Empty draws");
        vkCmdDrawIndirect(cmd, ssbo.buffer, 2 * sizeof(uvec4), 0, sizeof(uvec4));
        vkCmdDrawIndexedIndirect(cmd, ssbo.buffer, 3 * sizeof(uvec4), 0, 2 * sizeof(uvec4));
        popMarker(cmd);

        pushMarker(cmd, "Secondary: Indirect draws");

        // indirect draw at offset data[2], see above shader
        vkCmdDrawIndirect(cmd, ssbo.buffer, 2 * sizeof(uvec4), 1, sizeof(uvec4));

        // indirect indexed draw at offset data[3], see above shader
        vkCmdDrawIndexedIndirect(cmd, ssbo.buffer, 3 * sizeof(uvec4), 2, 2 * sizeof(uvec4));

        popMarker(cmd);

        // if we have KHR_draw_indirect_count, test it as well
        if(KHR_draw_indirect_count)
        {
          pushMarker(cmd, "Secondary: KHR_draw_indirect_count");

          pushMarker(cmd, "Secondary: Empty count draws");
          // empty draws
          vkCmdDrawIndirectCountKHR(cmd, ssbo.buffer, 11 * sizeof(uvec4), ssbo.buffer,
                                    10 * sizeof(uvec4), 0, sizeof(uvec4));
          vkCmdDrawIndexedIndirectCountKHR(cmd, ssbo.buffer, 12 * sizeof(uvec4), ssbo.buffer,
                                           10 * sizeof(uvec4) + sizeof(uint32_t), 0,
                                           sizeof(uvec4) * 2);
          popMarker(cmd);

          pushMarker(cmd, "Secondary: Indirect count draws");
          vkCmdDrawIndirectCountKHR(cmd, ssbo.buffer, 11 * sizeof(uvec4), ssbo.buffer,
                                    10 * sizeof(uvec4), 10, sizeof(uvec4));
          vkCmdDrawIndexedIndirectCountKHR(cmd, ssbo.buffer, 12 * sizeof(uvec4), ssbo.buffer,
                                           10 * sizeof(uvec4) + sizeof(uint32_t), 10,
                                           sizeof(uvec4) * 2);
          popMarker(cmd);

          popMarker(cmd);
        }
      }

      vkEndCommandBuffer(draw_secondary);

      vkCmdExecuteCommands(primary, 1, &draw_secondary);

      vkCmdEndRenderPass(primary);

      vkh::cmdPipelineBarrier(
          primary, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                    VK_ACCESS_TRANSFER_WRITE_BIT, ssbo.buffer)});

      // clear the buffer so that we can't read any of the data back from outside the command buffer
      vkCmdFillBuffer(primary, ssbo.buffer, 0, ssbo_size, 0);

      FinishUsingBackbuffer(primary, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(primary);

      Submit(1, 2, {primary}, {dispatch_secondary, draw_secondary});

      vkDeviceWaitIdle(device);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();