/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

RD_TEST(VK_Device_Address_Commands, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests capture and replay of VK_KHR_device_address_commands";

  const std::string pixel = R"EOSHADER(
#version 460 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform sampler2D smiley;

void main()
{
	Color = texture(smiley, vertIn.uv.xy * 2.0f);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_DEVICE_ADDRESS_COMMANDS_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

    features.depthBounds = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceDeviceAddressCommandsFeaturesKHR devAddrCommandsFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_ADDRESS_COMMANDS_FEATURES_KHR,
    };

    getPhysFeatures2(&devAddrCommandsFeatures);

    if(!devAddrCommandsFeatures.deviceAddressCommands)
      Avail = "feature 'deviceAddressCommands' not available";

    devInfoNext = &devAddrCommandsFeatures;

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufaddrFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };

    getPhysFeatures2(&bufaddrFeatures);

    if(!bufaddrFeatures.bufferDeviceAddress)
      Avail = "feature 'bufferDeviceAddress' not available";

    bufaddrFeatures.pNext = (void *)devInfoNext;
    devInfoNext = &bufaddrFeatures;

    static VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
    };

    getPhysFeatures2(&sync2Features);

    if(!sync2Features.synchronization2)
      Avail = "feature 'synchronization2' not available";

    sync2Features.pNext = (void *)devInfoNext;
    devInfoNext = &sync2Features;
  }

  int main()
  {
    vmaBDA = true;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

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
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    AllocatedImage smiley(
        this,
        vkh::ImageCreateInfo(rgba8.width, rgba8.height, 0, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {vkh::ImageMemoryBarrier(0, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                        smiley.image)});

      vkEndCommandBuffer(cmd);

      Submit(99, 99, {cmd});
    }

    VkImageView smileyview = createImageView(
        vkh::ImageViewCreateInfo(smiley.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM));

    VkSampler smileysampler = createSampler(vkh::SamplerCreateInfo(
        VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(
                descset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {vkh::DescriptorImageInfo(smileyview, VK_IMAGE_LAYOUT_GENERAL, smileysampler)}),
        });

    DefaultA2V vertices[4] = {
        {Vec3f(-1.0f, -1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.5f)},
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, -1.0f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.5f, 0.5f)},
        {Vec3f(1.0f, 1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.5f, 0.0f)},
    };

    uint32_t vertexBufferSize = sizeof(vertices);
    uint32_t indexBufferSize = sizeof(uint32_t) * 6;
    uint32_t indirectBufferSize = sizeof(VkDrawIndexedIndirectCommand);
    uint32_t imageUploadSize = (uint32_t)rgba8.data.size() * sizeof(uint32_t);
    uint32_t imageDownloadSize = sizeof(uint32_t);

    uint32_t vertexBufferOffset = 0;
    uint32_t indexBufferOffset = vertexBufferOffset + vertexBufferSize;
    uint32_t indirectBufferOffset = indexBufferOffset + indexBufferSize;
    uint32_t imageUploadOffset = indirectBufferOffset + indirectBufferSize;
    uint32_t imageDownloadOffset = imageUploadOffset + imageUploadSize;

    uint32_t wholeSize = imageDownloadOffset + imageDownloadSize;

    AllocatedBuffer buffer(
        this,
        vkh::BufferCreateInfo(
            wholeSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(buffer.buffer, "Address Buffer");

    VkAddressCommandFlagsKHR flags = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;

    VkBindVertexBuffer3InfoKHR vertexBindingInfo = {
        VK_STRUCTURE_TYPE_BIND_VERTEX_BUFFER_3_INFO_KHR,         NULL,  VK_FALSE,
        {buffer.address + vertexBufferOffset, vertexBufferSize}, flags,
    };

    VkBindIndexBuffer3InfoKHR indexBindingInfo = {
        VK_STRUCTURE_TYPE_BIND_INDEX_BUFFER_3_INFO_KHR,
        NULL,
        {buffer.address + indexBufferOffset, indexBufferSize},
        flags,
        VK_INDEX_TYPE_UINT32,
    };

    VkDrawIndirect2InfoKHR indirectInfo = {
        VK_STRUCTURE_TYPE_DRAW_INDIRECT_2_INFO_KHR,
        NULL,
        {buffer.address + indirectBufferOffset, indirectBufferSize,
         sizeof(VkDrawIndexedIndirectCommand)},
        flags,
        1,
    };

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      vkh::cmdPipelineBarrier(
          cmd, {}, {},
          {vkh::MemoryBarrier(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                              VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)});

      StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      // clear the buffer and image so a replay cannot happen to be correct due to initial contents
      // left behind by a previous frame
      vkCmdFillBuffer(cmd, buffer.buffer, 0, VK_WHOLE_SIZE, 0);

      VkClearColorValue color = {{0.0f, 0.0f, 0.0f, 0.0f}};
      VkImageSubresourceRange subresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      vkCmdClearColorImage(cmd, smiley.image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &subresource);
      vkCmdClearColorImage(cmd, mainWindow->GetImage(), VK_IMAGE_LAYOUT_GENERAL, &color, 1,
                           &subresource);

      vkh::cmdPipelineBarrier(
          cmd, {}, {},
          {vkh::MemoryBarrier(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                              VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)});

      // upload vertex data
      VkDeviceAddressRangeKHR range = {buffer.address + vertexBufferOffset, vertexBufferSize};
      vkCmdUpdateMemoryKHR(cmd, &range, flags, sizeof(vertices), vertices);

      // upload partial index data
      uint32_t indices[4] = {0, 1, 2, 3};
      vkCmdUpdateMemoryKHR(cmd, &indexBindingInfo.addressRange, flags, sizeof(indices), indices);

      // write indirect arguments
      range = {buffer.address + indirectBufferOffset, sizeof(uint32_t)};
      vkCmdFillMemoryKHR(cmd, &range, flags, 6);    // indexCount = 6
      range = {buffer.address + indirectBufferOffset + sizeof(uint32_t), sizeof(uint32_t)};
      vkCmdFillMemoryKHR(cmd, &range, flags, 1);    // instanceCount = 1
      range = {buffer.address + indirectBufferOffset + sizeof(uint32_t) * 2, sizeof(uint32_t) * 3};
      vkCmdFillMemoryKHR(cmd, &range, flags, 0);    // firstIndex = vertexOffset = firstInstance = 0

      // write image data
      range = {buffer.address + imageUploadOffset, imageUploadSize};
      vkCmdUpdateMemoryKHR(cmd, &range, flags, imageUploadSize, rgba8.data.data());

      VkMemoryRangeBarrierKHR rangeBarrier = {
          VK_STRUCTURE_TYPE_MEMORY_RANGE_BARRIER_KHR,
          NULL,
          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          {buffer.address, wholeSize},
          flags,
      };
      VkMemoryRangeBarriersInfoKHR rangeBarriers = {
          VK_STRUCTURE_TYPE_MEMORY_RANGE_BARRIERS_INFO_KHR, NULL, 1, &rangeBarrier};
      VkDependencyInfo dependency = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO, &rangeBarriers};
      vkCmdPipelineBarrier2(cmd, &dependency);

      // copy {1, 2} to the end of the buffer to form the full index buffer: {0, 1, 2, 3, 1, 2}
      VkDeviceMemoryCopyKHR copyMemoryRegion = {
          VK_STRUCTURE_TYPE_DEVICE_MEMORY_COPY_KHR,
          NULL,
          {buffer.address + indexBufferOffset + sizeof(uint32_t), sizeof(uint32_t) * 2},
          flags,
          {buffer.address + indexBufferOffset + sizeof(uint32_t) * 4, sizeof(uint32_t) * 2},
          flags,
      };
      VkCopyDeviceMemoryInfoKHR copyMemoryInfo = {VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_INFO_KHR,
                                                  NULL, 1, &copyMemoryRegion};
      vkCmdCopyMemoryKHR(cmd, &copyMemoryInfo);

      VkDeviceMemoryImageCopyKHR copyImageRegion = {
          VK_STRUCTURE_TYPE_DEVICE_MEMORY_IMAGE_COPY_KHR,
          NULL,
          {buffer.address + imageUploadOffset, imageUploadSize},
          flags,
          0,
          0,
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          VK_IMAGE_LAYOUT_GENERAL,
          {0, 0, 0},
          {rgba8.width, rgba8.height, 1},
      };

      VkCopyDeviceMemoryImageInfoKHR copyImageInfo = {
          VK_STRUCTURE_TYPE_COPY_DEVICE_MEMORY_IMAGE_INFO_KHR, NULL, smiley.image, 1,
          &copyImageRegion};
      vkCmdCopyMemoryToImageKHR(cmd, &copyImageInfo);

      // As of 621.1.0.0, Nvidia's Windows drivers incorrectly exclude vkCmdCopyMemoryToImageKHR
      // and vkCmdCopyImageToMemoryKHR from the 'transfer' synchronization scope, but only if the command
      // is in the second synchronization scope. By being precise about the barrier bits here we can
      // cause the final buffer contents to be incorrect to test for the presence of this bug.
      vkh::cmdPipelineBarrier(
          cmd, {}, {},
          {vkh::MemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)},
          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

      copyImageRegion.addressRange = {buffer.address + imageDownloadOffset, imageDownloadSize};
      copyImageRegion.imageExtent = {1, 1, 1};
      vkCmdCopyImageToMemoryKHR(cmd, &copyImageInfo);
      setMarker(cmd, "After CopyImageToMemory");

      vkh::cmdPipelineBarrier(
          cmd, {}, {},
          {vkh::MemoryBarrier(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                              VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)});

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

      vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);

      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});
      vkCmdBindVertexBuffers3KHR(cmd, 0, 1, &vertexBindingInfo);
      vkCmdBindIndexBuffer3KHR(cmd, &indexBindingInfo);
      vkCmdDrawIndexedIndirect2KHR(cmd, &indirectInfo);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
