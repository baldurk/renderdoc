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

RD_TEST(VK_Stripped_Shaders, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests shaders with their debug/reflection info stripped out and stored in separate blobs.";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex + "\n// shader source // vertex", ShaderLang::glsl,
                            ShaderStage::vert, "main", {}, SPIRVTarget::vulkan,
                            CompileOptionFlags::SeparateDebug),
        CompileShaderModule(VKDefaultPixel + "\n// shader source // pixel", ShaderLang::glsl,
                            ShaderStage::frag, "main", {}, SPIRVTarget::vulkan,
                            CompileOptionFlags::SeparateDebug | CompileOptionFlags::RelativeDebugName),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg = StartUsingBackbuffer(cmd);

      vkh::cmdClearImage(cmd, swapimg, vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f));

      vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      mainWindow->setViewScissor(cmd);
      vkh::cmdBindVertexBuffers(cmd, {DefaultTriVB.buffer});

      setMarker(cmd, "test: shader source");
      setMarker(cmd, "vertex manual absolute");
      setMarker(cmd, "pixel manual relative");
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
