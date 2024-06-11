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

// only support on 64-bit, just because it's easier to share CPU & GPU structs if pointer size is
// identical

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) ||        \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__) || \
    (defined(__riscv) && __riscv_xlen == 64)

RD_TEST(VK_KHR_Buffer_Address, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test capture and replay of VK_KHR_buffer_device_address";

  // should match definition below in GLSL
  struct DrawData
  {
    DefaultA2V *vert_data;
    // no alignment on Vec4f, use scalar block layout
    Vec4f tint;
    Vec2f offset;
    Vec2f scale;
    // padding to make the struct size 16 to make aligning the buffer easier.
    Vec2f padding;
  };

  std::string common = R"EOSHADER(

#version 460 core

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference_uvec2 : require

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

struct DefaultA2V {
  vec3 pos;
  vec4 col;
  vec2 uv;
};

layout(buffer_reference, scalar, buffer_reference_align = 16) buffer TriangleData {
  DefaultA2V verts[3];
};

layout(buffer_reference, scalar, buffer_reference_align = 16) buffer DrawData {
  TriangleData tri;
  vec4 tint;
  vec2 offset;
  vec2 scale;
  vec2 padding;
};

layout(push_constant) uniform PushData {
  uvec2 data_ptr;
  DrawData drawdata;
} push;

)EOSHADER";

  const std::string vertex = R"EOSHADER(

layout(location = 0) out v2f vertOut;

void main()
{
  DrawData draw = DrawData(push.data_ptr);
  DefaultA2V vert = draw.tri.verts[gl_VertexIndex];

	gl_Position = vertOut.pos = vec4(vert.pos*vec3(draw.scale,1) + vec3(draw.offset, 0), 1);
	vertOut.col = vert.col;
	vertOut.uv = vec4(vert.uv, 0, 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  DrawData draw = push.drawdata;

	Color = vertIn.col * draw.tint;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufaddrFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };

    getPhysFeatures2(&bufaddrFeatures);

    if(!bufaddrFeatures.bufferDeviceAddress)
      Avail = "Buffer device address feature 'bufferDeviceAddress' not available";

    bufaddrFeatures.bufferDeviceAddressCaptureReplay = 0;
    bufaddrFeatures.bufferDeviceAddressMultiDevice = 0;

    devInfoNext = &bufaddrFeatures;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(
        vkh::PipelineLayoutCreateInfo({}, {vkh::PushConstantRange(VK_SHADER_STAGE_ALL, 0, 16)}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    vkh::BufferCreateInfo bufinfo(0x100000, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryAllocateFlagsInfo memoryAllocateFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};

    memoryAllocateFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    memoryAllocateInfo.pNext = &memoryAllocateFlags;

    const VkPhysicalDeviceMemoryProperties *memProps = NULL;
    vmaGetMemoryProperties(allocator, &memProps);

    VkBuffer databuf;
    vkCreateBuffer(device, bufinfo, NULL, &databuf);

    VkBuffer staticBuf;
    vkCreateBuffer(device, bufinfo, NULL, &staticBuf);

    VkMemoryRequirements mrq;
    vkGetBufferMemoryRequirements(device, databuf, &mrq);

    memoryAllocateInfo.allocationSize = mrq.size;

    for(uint32_t i = 0; i < memProps->memoryTypeCount; i++)
    {
      if((mrq.memoryTypeBits & (1u << i)) &&
         (memProps->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
      {
        memoryAllocateInfo.memoryTypeIndex = i;
        break;
      }
    }

    VkDeviceMemory databufMem;
    vkAllocateMemory(device, &memoryAllocateInfo, NULL, &databufMem);
    vkBindBufferMemory(device, databuf, databufMem, 0);

    VkDeviceMemory staticBufMem;
    vkAllocateMemory(device, &memoryAllocateInfo, NULL, &staticBufMem);
    vkBindBufferMemory(device, staticBuf, staticBufMem, 0);

    // north-facing primary colours triangle
    const DefaultA2V tri1[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    // north-west-facing triangle
    const DefaultA2V tri2[3] = {
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(1.0f, 0.2f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.7f, 0.85f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 1.0f, 0.4f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    VkBufferDeviceAddressInfoKHR info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    info.buffer = databuf;

    VkDeviceAddress baseAddr = vkGetBufferDeviceAddressKHR(device, &info);
    byte *gpuptr = (byte *)baseAddr;    // not a valid cpu pointer but useful for avoiding casting

    byte *cpuptr = NULL;
    vkMapMemory(device, databufMem, 0, mrq.size, 0, (void **)&cpuptr);

    // put triangle data first
    memcpy(cpuptr, tri1, sizeof(tri1));
    DefaultA2V *gputri1 = (DefaultA2V *)gpuptr;
    cpuptr += sizeof(tri1);
    gpuptr += sizeof(tri1);

    // align to 16 bytes
    cpuptr = AlignUpPtr(cpuptr, 16);
    gpuptr = AlignUpPtr(gpuptr, 16);

    memcpy(cpuptr, tri2, sizeof(tri2));
    DefaultA2V *gputri2 = (DefaultA2V *)gpuptr;
    cpuptr += sizeof(tri2);
    gpuptr += sizeof(tri2);

    // align to 16 bytes
    cpuptr = AlignUpPtr(cpuptr, 16);
    gpuptr = AlignUpPtr(gpuptr, 16);

    DrawData *drawscpu = (DrawData *)cpuptr;
    DrawData *drawsgpu = (DrawData *)gpuptr;

    drawscpu[0].vert_data = gputri1;
    drawscpu[0].offset = Vec2f(-0.5f, 0.0f);
    drawscpu[0].scale = Vec2f(0.5f, 0.5f);
    drawscpu[0].tint = Vec4f(1.0f, 0.5f, 0.5f, 1.0f);    // tint red

    drawscpu[1].vert_data = gputri1;
    drawscpu[1].offset = Vec2f(0.0f, 0.0f);
    drawscpu[1].scale = Vec2f(0.5f, -0.5f);              // flip vertically
    drawscpu[1].tint = Vec4f(0.2f, 0.5f, 1.0f, 1.0f);    // tint blue

    drawscpu[2].vert_data = gputri2;    // use second triangle
    drawscpu[2].offset = Vec2f(0.6f, 0.0f);
    drawscpu[2].scale = Vec2f(0.5f, 0.5f);
    drawscpu[2].tint = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Make a static buffer of draw data
    info.buffer = staticBuf;
    baseAddr = vkGetBufferDeviceAddressKHR(device, &info);
    gpuptr = (byte *)baseAddr;    // not a valid cpu pointer but useful for avoiding casting
    vkMapMemory(device, staticBufMem, 0, mrq.size, 0, (void **)&cpuptr);

    // put triangle data first
    memcpy(cpuptr, tri1, sizeof(tri1));
    gputri1 = (DefaultA2V *)gpuptr;
    cpuptr += sizeof(tri1);
    gpuptr += sizeof(tri1);

    // align to 16 bytes
    cpuptr = AlignUpPtr(cpuptr, 16);
    gpuptr = AlignUpPtr(gpuptr, 16);

    memcpy(cpuptr, tri2, sizeof(tri2));
    gputri2 = (DefaultA2V *)gpuptr;
    cpuptr += sizeof(tri2);
    gpuptr += sizeof(tri2);

    // align to 16 bytes
    cpuptr = AlignUpPtr(cpuptr, 16);
    gpuptr = AlignUpPtr(gpuptr, 16);

    DrawData *staticDrawsCpu = (DrawData *)cpuptr;
    DrawData *staticDrawsGpu = (DrawData *)gpuptr;

    staticDrawsCpu[0].vert_data = gputri1;
    staticDrawsCpu[0].offset = Vec2f(-0.5f, 0.5f);
    staticDrawsCpu[0].scale = Vec2f(0.5f, 0.5f);
    staticDrawsCpu[0].tint = Vec4f(1.0f, 1.0f, 0.2f, 1.0f);    // tint yellow

    vkUnmapMemory(device, staticBufMem);

    float time = 0.0f;

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      for(int i = 0; i < 2; i++)
      {
        VkBuffer midbuf = VK_NULL_HANDLE;
        vkCreateBuffer(device, bufinfo, NULL, &midbuf);

        VkDeviceMemory midmem = VK_NULL_HANDLE;
        vkAllocateMemory(device, &memoryAllocateInfo, NULL, &midmem);
        vkBindBufferMemory(device, midbuf, midmem, 0);

        vkMapMemory(device, midmem, 0, mrq.size, 0, (void **)&cpuptr);
        vkDestroyBuffer(device, midbuf, NULL);
        vkUnmapMemory(device, midmem);
        vkFreeMemory(device, midmem, NULL);
      }

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);

      // look ma, no binds
      DrawData *bindptr = drawsgpu;
      drawscpu[0].scale.x = (abs(sinf(time)) + 0.1f) * 0.5f;
      drawscpu[0].scale.y = 0.5f;
      setMarker(cmd, "Draw 1");
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &bindptr);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 8, 8, &bindptr);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      bindptr++;
      drawscpu[1].scale.x = 0.5f;
      drawscpu[1].scale.y = (abs(cosf(time)) + 0.1f) * 0.5f;
      setMarker(cmd, "Draw 2");
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &bindptr);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 8, 8, &bindptr);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      bindptr++;
      drawscpu[2].scale = Vec2f(0.5f, 0.5f);
      drawscpu[2].tint = Vec4f(cosf(time) * 0.5f + 0.5f, sinf(time) * 0.5f + 0.5f,
                               cosf(time + 3.14f) * 0.5f + 0.5f, 1.0f);
      setMarker(cmd, "Draw 3");
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &bindptr);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 8, 8, &bindptr);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      setMarker(cmd, "Draw 4");
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 0, 8, &staticDrawsGpu);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL, 8, 8, &staticDrawsGpu);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      vkDeviceWaitIdle(device);

      drawscpu[0].scale = Vec2f(0.0f, 0.0f);
      drawscpu[1].scale = Vec2f(0.0f, 0.0f);
      drawscpu[2].scale = Vec2f(0.0f, 0.0f);

      Present();

      time += 0.1f;
    }

    CHECK_VKR(vkDeviceWaitIdle(device));

    vkDestroyBuffer(device, databuf, NULL);
    vkUnmapMemory(device, databufMem);
    vkFreeMemory(device, databufMem, NULL);

    return 0;
  }
};

REGISTER_TEST();

#endif    // if 64-bit
