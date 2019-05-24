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

TEST(VK_Sample_Locations, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws two triangles with different sample locations using VK_EXT_sample_locations";

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

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPhysicalDeviceSampleLocationsPropertiesEXT sampleProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT};

    vkGetPhysicalDeviceProperties2KHR(phys, vkh::PhysicalDeviceProperties2KHR().next(&sampleProps));

    TEST_ASSERT(sampleProps.sampleLocationSampleCounts & VK_SAMPLE_COUNT_4_BIT,
                "Sample locations for MSAA 4x not supported");

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    // create multi-sampled image
    AllocatedImage msaaimg(
        allocator,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             mainWindow->format,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT,
                             1, 1, VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView msview = createImageView(
        vkh::ImageViewCreateInfo(msaaimg.image, VK_IMAGE_VIEW_TYPE_2D, mainWindow->format));

    // create renderpass using the DS image
    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_4_BIT));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

    renderPassCreateInfo.addSubpass(
        {VkAttachmentReference({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL})}, VK_ATTACHMENT_UNUSED,
        VK_IMAGE_LAYOUT_UNDEFINED, {VkAttachmentReference({1, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    // create framebuffers using swapchain images and DS image
    std::vector<VkFramebuffer> fbs;
    fbs.resize(mainWindow->GetCount());

    for(size_t i = 0; i < mainWindow->GetCount(); i++)
      fbs[i] = createFramebuffer(vkh::FramebufferCreateInfo(
          renderPass, {msview, mainWindow->GetView(i)}, mainWindow->scissor.extent));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);

    VkPipelineSampleLocationsStateCreateInfoEXT samplePipe = {
        VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,
    };

    samplePipe.sampleLocationsEnable = VK_TRUE;
    samplePipe.sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;

    pipeCreateInfo.multisampleState.pNext = &samplePipe;
    pipeCreateInfo.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

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
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, msaaimg.image),
               });

      vkCmdClearColorImage(cmd, msaaimg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           vkh::ClearColorValue(0.6f, 0.5f, 0.4f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, msaaimg.image),
               });

      VkRect2D region = mainWindow->scissor;
      region.extent.width /= 2;

      VkViewport view = mainWindow->viewport;
      view.width /= 2.0f;

      VkRenderPassSampleLocationsBeginInfoEXT sampleBegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,
      };

      float gridBase = sampleProps.sampleLocationCoordinateRange[0];
      float gridDim = sampleProps.sampleLocationCoordinateRange[1];

// rescales from [-8, 8] to [maxGrid[0], maxGrid[1]] in both dimension
#define SAMPLE_POS(x, y)                                                                     \
  {                                                                                          \
    (((x) + 8.0f) / 16.0f) * gridDim + gridBase, (((y) + 8.0f) / 16.0f) * gridDim + gridBase \
  }

      // vertical grid and degenerate
      VkSampleLocationEXT locations1[4] = {
          // TL
          SAMPLE_POS(0.0f, -8.0f),
          // TR
          SAMPLE_POS(0.0f, -8.0f),
          // BL
          SAMPLE_POS(0.0f, 8.0f),
          // BR
          SAMPLE_POS(0.0f, 8.0f),
      };

      // rotated grid
      VkSampleLocationEXT locations2[4] = {
          // TL
          SAMPLE_POS(-2.0f, -6.0f),
          // TR
          SAMPLE_POS(6.0f, -2.0f),
          // BL
          SAMPLE_POS(-6.0f, 2.0f),
          // BR
          SAMPLE_POS(2.0f, 6.0f),
      };

      VkSubpassSampleLocationsEXT subpassSamples = {};
      subpassSamples.subpassIndex = 0;
      subpassSamples.sampleLocationsInfo.sType = VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT;
      subpassSamples.sampleLocationsInfo.sampleLocationGridSize.width = 1;
      subpassSamples.sampleLocationsInfo.sampleLocationGridSize.height = 1;
      subpassSamples.sampleLocationsInfo.sampleLocationsCount = 4;
      subpassSamples.sampleLocationsInfo.sampleLocationsPerPixel = VK_SAMPLE_COUNT_4_BIT;
      subpassSamples.sampleLocationsInfo.pSampleLocations = locations1;

      sampleBegin.postSubpassSampleLocationsCount = 1;
      sampleBegin.pPostSubpassSampleLocations = &subpassSamples;

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], region,
                                                    {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)})
                               .next(&sampleBegin),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetSampleLocationsEXT(cmd, &subpassSamples.sampleLocationsInfo);
      vkCmdSetViewport(cmd, 0, 1, &view);
      vkCmdSetScissor(cmd, 0, 1, &region);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "Degenerate Sample Locations");
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      region.offset.x += region.extent.width;
      view.x += view.width;

      subpassSamples.sampleLocationsInfo.pSampleLocations = locations2;

      vkCmdBeginRenderPass(cmd,
                           vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], region,
                                                    {vkh::ClearValue(0.0f, 0.0f, 0.0f, 1.0f)})
                               .next(&sampleBegin),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetSampleLocationsEXT(cmd, &subpassSamples.sampleLocationsInfo);
      vkCmdSetViewport(cmd, 0, 1, &view);
      vkCmdSetScissor(cmd, 0, 1, &region);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "Rotated Grid Sample Locations");
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