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

RD_TEST(VK_Line_Raster, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test using VK_EXT_line_rasterization to do funky rasterization of lines";

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

void main()
{
	Color = vec4(0,1,1,1);
}

)EOSHADER";

  struct A2V
  {
    Vec3f pos;
  };

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);

    features.wideLines = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceLineRasterizationFeaturesEXT lineraster = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT,
    };

    getPhysFeatures2(&lineraster);

    // enable all features that are available
    devInfoNext = &lineraster;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, A2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, A2V, pos),
    };

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    // if the feature is supported (which we checked) this is within the minimum supported range
    pipeCreateInfo.rasterizationState.lineWidth = 6.0f;

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipelineRasterizationLineStateCreateInfoEXT lineRasterSetup = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,
    };

    pipeCreateInfo.rasterizationState.pNext = &lineRasterSetup;

    A2V linePoints[2] = {
        {Vec3f(0.9f, 0.9f, 0.0f)},
        {Vec3f(-0.9f, -0.9f, 0.0f)},
    };

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(linePoints),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(linePoints);

    struct Test
    {
      VkPipeline pipe;
      uint32_t stippleFactor;
      uint16_t stipplePattern;
    };

    std::vector<Test> tests;

    static VkPhysicalDeviceLineRasterizationFeaturesEXT lineRasterFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT,
    };

    getPhysFeatures2(&lineRasterFeatures);

    lineRasterSetup.stippledLineEnable = VK_FALSE;
    lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
    tests.push_back({createGraphicsPipeline(pipeCreateInfo)});
    tests.push_back({VK_NULL_HANDLE});
    tests.push_back({VK_NULL_HANDLE});
    tests.push_back({VK_NULL_HANDLE});

    if(lineRasterFeatures.rectangularLines)
    {
      lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;
      tests.push_back({createGraphicsPipeline(pipeCreateInfo)});
    }
    else
    {
      tests.push_back({VK_NULL_HANDLE});
    }

    if(lineRasterFeatures.bresenhamLines)
    {
      lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
      tests.push_back({createGraphicsPipeline(pipeCreateInfo)});
    }
    else
    {
      tests.push_back({VK_NULL_HANDLE});
    }

    if(lineRasterFeatures.smoothLines)
    {
      lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;
      tests.push_back({createGraphicsPipeline(pipeCreateInfo)});
    }
    else
    {
      tests.push_back({VK_NULL_HANDLE});
    }

    // dummy, to push to the next row
    tests.push_back({VK_NULL_HANDLE});

    lineRasterSetup.stippledLineEnable = VK_TRUE;
    pipeCreateInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_STIPPLE_EXT);

    if(lineRasterFeatures.rectangularLines && lineRasterFeatures.stippledRectangularLines)
    {
      lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;
      tests.push_back({createGraphicsPipeline(pipeCreateInfo), 2, 0xC3C3U});
    }
    else
    {
      tests.push_back({VK_NULL_HANDLE});
    }

    if(lineRasterFeatures.bresenhamLines && lineRasterFeatures.stippledBresenhamLines)
    {
      lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
      tests.push_back({createGraphicsPipeline(pipeCreateInfo), 2, 0x1F1FU});
    }
    else
    {
      tests.push_back({VK_NULL_HANDLE});
    }

    if(lineRasterFeatures.smoothLines && lineRasterFeatures.stippledSmoothLines)
    {
      lineRasterSetup.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;
      tests.push_back({createGraphicsPipeline(pipeCreateInfo), 2, 0xC3C3U});
    }
    else
    {
      tests.push_back({VK_NULL_HANDLE});
    }

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

      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      VkViewport view = mainWindow->viewport;
      view.width /= 4.0f;
      view.height /= 4.0f;

      for(const Test &t : tests)
      {
        if(t.pipe != VK_NULL_HANDLE)
        {
          vkCmdSetViewport(cmd, 0, 1, &view);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, t.pipe);

          if(t.stipplePattern)
            vkCmdSetLineStippleEXT(cmd, t.stippleFactor, t.stipplePattern);

          vkCmdDraw(cmd, 2, 1, 0, 0);
        }

        view.x += view.width;
        if(view.x >= mainWindow->viewport.width)
        {
          view.x = 0.0f;
          view.y += view.height;
        }
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
