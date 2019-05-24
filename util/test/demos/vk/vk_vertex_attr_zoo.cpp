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

TEST(VK_Vertex_Attr_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle but using different kinds of vertex attributes, including doubles, arrays, "
      "matrices, and formats that require manual decode as they are vertex-buffer exclusive on "
      "some hardware such as USCALED.";

  struct vertin
  {
    int16_t i16[4];
    uint16_t u16[4];
    double df[2];
    float arr0[2];
    float arr1[2];
    float arr2[2];
    float mat0[2];
    float mat1[2];
  };

  std::string vertex = R"EOSHADER(
#version 450 core

layout(location = 0) in vec4 InSNorm;
layout(location = 1) in vec4 InUNorm;
layout(location = 2) in vec4 InUScaled;
layout(location = 3) in uvec4 InUInt;
layout(location = 4) in dvec2 InDouble;
layout(location = 5) in vec2 InArray[3];
layout(location = 8) in mat2x2 InMatrix;

layout(location = 0) out vec4 OutSNorm;
layout(location = 1) out vec4 OutUNorm;
layout(location = 2) out vec4 OutUScaled;
layout(location = 3) out uvec4 OutUInt;
layout(location = 4) out dvec2 OutDouble;
layout(location = 5) out vec2 OutArray[3];
layout(location = 8) out mat2x2 OutMatrix;

void main()
{
  const vec4 verts[3] = vec4[3](vec4(-0.5, 0.5, 0.0, 1.0), vec4(0.0, -0.5, 0.0, 1.0),
                                vec4(0.5, 0.5, 0.0, 1.0));

  gl_Position = verts[gl_VertexIndex];

  OutSNorm = InSNorm;
  OutUScaled = InUScaled;
  OutDouble = InDouble;
  OutUInt = InUInt;
  OutUNorm = InUNorm;
  OutArray = InArray;
  OutMatrix = InMatrix;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 450 core

layout(location = 0) in vec4 InSNorm;
layout(location = 1) in vec4 InUNorm;
layout(location = 2) in vec4 InUScaled;
layout(location = 3) flat in uvec4 InUInt;
layout(location = 4) flat in dvec2 InDouble;
layout(location = 5) in vec2 InArray[3];
layout(location = 8) in mat2x2 InMatrix;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  Color = vec4(0, 1.0f, 0, 1);

  // check values came through correctly

  // SNorm should be in [-1, 1]
  if(clamp(InSNorm, -1.0, 1.0) != InSNorm)
    Color = vec4(0.1f, 0, 0, 1);

  // UNorm should be in [0, 1]
  if(clamp(InUNorm, 0.0, 1.0) != InUNorm)
    Color = vec4(0.2f, 0, 0, 1);

  // UScaled was sourced from 16-bit and is non-zero so should be in that range
  if(clamp(InUScaled, 1.0, 65535.0) != InUScaled)
    Color = vec4(0.3f, 0, 0, 1);

  // Similar for UInt
  if(InUInt.x > 65535 || InUInt.y > 65535 || InUInt.z > 65535 || InUInt.w > 65535)
    Color = vec4(0.4f, 0, 0, 1);

  // doubles are all in range [-10, 10]
  if(clamp(InDouble, -10.0, 10.0) != InDouble)
    Color = vec4(0.5f, 0, 0, 1);
}

)EOSHADER";

  std::string geom = R"EOSHADER(
#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec4 InSNorm[3];
layout(location = 1) in vec4 InUNorm[3];
layout(location = 2) in vec4 InUScaled[3];
layout(location = 3) in uvec4 InUInt[3];
layout(location = 4) in dvec2 InDouble[3];
layout(location = 5) in vec2 InArray[3][3];
layout(location = 8) in mat2x2 InMatrix[3];

layout(location = 0) out vec4 OutSNorm;
layout(location = 1) out vec4 OutUNorm;
layout(location = 2) out vec4 OutUScaled;
layout(location = 3) out uvec4 OutUInt;
layout(location = 4) out dvec2 OutDouble;
layout(location = 5) out vec2 OutArray[3];
layout(location = 8) out mat2x2 OutMatrix;

void main()
{
  for(int i = 0; i < 3; i++)
  {
    gl_Position = vec4(gl_in[i].gl_Position.yx, 0.4f, 1.2f);

    OutSNorm = InSNorm[i];
    OutUScaled = InUScaled[i];
    OutDouble = InDouble[i];
    OutUInt = InUInt[i];
    OutUNorm = InUNorm[i];
    OutArray = InArray[i];
    OutMatrix = InMatrix[i];

    EmitVertex();
  }
  EndPrimitive();
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    // we could have fallbacks for these, but they're supported everywhere we run our tests
    features.shaderFloat64 = VK_TRUE;
    features.geometryShader = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);
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

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, vertin)};

    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttrFormatted(0, 0, vertin, i16, VK_FORMAT_R16G16B16A16_SNORM),
        vkh::vertexAttrFormatted(1, 0, vertin, u16, VK_FORMAT_R16G16B16A16_UNORM),
        vkh::vertexAttrFormatted(2, 0, vertin, u16, VK_FORMAT_R16G16B16A16_USCALED),
        vkh::vertexAttrFormatted(3, 0, vertin, u16, VK_FORMAT_R16G16B16A16_UINT),
        vkh::vertexAttrFormatted(4, 0, vertin, df, VK_FORMAT_R64G64_SFLOAT),
        vkh::vertexAttrFormatted(5, 0, vertin, arr0, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(6, 0, vertin, arr1, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(7, 0, vertin, arr2, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(8, 0, vertin, mat0, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(9, 0, vertin, mat1, VK_FORMAT_R32G32_SFLOAT),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
        CompileShaderModule(geom, ShaderLang::glsl, ShaderStage::geom, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    vertin triangle[] = {
        {
            {32767, -32768, 32767, -32767},
            {12345, 6789, 1234, 567},
            {9.8765432109, -5.6789012345},
            {1.0f, 2.0f},
            {3.0f, 4.0f},
            {5.0f, 6.0f},
            {7.0, 8.0f},
            {9.0f, 10.0f},
        },
        {
            {32766, -32766, 16000, -16000},
            {56, 7890, 123, 4567},
            {-7.89012345678, 6.54321098765},
            {11.0f, 12.0f},
            {13.0f, 14.0f},
            {15.0f, 16.0f},
            {17.0, 18.0f},
            {19.0f, 20.0f},
        },
        {
            {5, -5, 0, 0},
            {8765, 43210, 987, 65432},
            {0.1234567890123, 4.5678901234},
            {21.0f, 22.0f},
            {23.0f, 24.0f},
            {25.0f, 26.0f},
            {27.0, 28.0f},
            {29.0f, 30.0f},
        },
    };

    AllocatedBuffer vb(allocator,
                       vkh::BufferCreateInfo(sizeof(triangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(triangle);

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

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
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