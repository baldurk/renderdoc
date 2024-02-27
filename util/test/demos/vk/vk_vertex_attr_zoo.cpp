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

RD_TEST(VK_Vertex_Attr_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle but using different kinds of vertex attributes, including doubles, arrays, "
      "matrices, and formats that require manual decode as they are vertex-buffer exclusive on "
      "some hardware such as USCALED.";

  struct vertin
  {
    int16_t i16[4];
    uint16_t u16[4];
    double df[3];
    float arr0[2];
    float arr1[2];
    float mat0[2];
    float mat1[2];
    uint64_t lf[3];
    int64_t slf[3];
  };

  std::string vertex = R"EOSHADER(
layout(location = 0) in vec4 InSNorm;
layout(location = 1) in vec4 InUNorm;
layout(location = 2) in vec4 InUScaled;
layout(location = 3) in uvec2 InUInt;
layout(location = 3, component = 2) in uint InUInt1;
layout(location = 3, component = 3) in uint InUInt2;
#if DOUBLES
layout(location = 4) in dvec3 InDouble;
#endif
layout(location = 6) in vec2 InArray[2];
layout(location = 8) in mat2x2 InMatrix;
#if LONGS
layout(location = 10) in u64vec3 InULong;
layout(location = 12) in i64vec3 InSLong;
#endif

layout(location = 0) out vec4 OutSNorm;
layout(location = 1) out vec4 OutUNorm;
layout(location = 2) out vec4 OutUScaled;
layout(location = 3) flat out uvec2 OutUInt;
layout(location = 3, component = 2) flat out uint OutUInt1;
layout(location = 3, component = 3) flat out uint OutUInt2;
#if DOUBLES
layout(location = 4) out dvec3 OutDouble;
#endif
layout(location = 6) out vec2 OutArray[2];
layout(location = 8) out mat2x2 OutMatrix;
#if LONGS
layout(location = 10) out u64vec3 OutULong;
layout(location = 12) out i64vec3 OutSLong;
#endif

void main()
{
  const vec4 verts[3] = vec4[3](vec4(-0.5, 0.5, 0.0, 1.0), vec4(0.0, -0.5, 0.0, 1.0),
                                vec4(0.5, 0.5, 0.0, 1.0));

  gl_Position = verts[gl_VertexIndex];

  OutSNorm = InSNorm;
  OutUScaled = InUScaled;
#if DOUBLES
  OutDouble = InDouble;
#endif
  OutUInt = InUInt;
  OutUInt1 = InUInt1;
  OutUInt2 = InUInt2;
  OutUNorm = InUNorm;
  OutArray = InArray;
  OutMatrix = InMatrix;
#if LONGS
  OutULong = InULong;
  OutSLong = InSLong;
#endif
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
layout(location = 0) in vec4 InSNorm;
layout(location = 1) in vec4 InUNorm;
layout(location = 2) in vec4 InUScaled;
layout(location = 3) flat in uvec2 InUInt;
layout(location = 3, component = 2) flat in uint InUInt1;
layout(location = 3, component = 3) flat in uint InUInt2;
#if DOUBLES
layout(location = 4) flat in dvec3 InDouble;
#endif
layout(location = 6) in vec2 InArray[2];
layout(location = 8) in mat2x2 InMatrix;
#if LONGS
layout(location = 10) flat in u64vec3 InULong;
layout(location = 12) flat in i64vec3 InSLong;
#endif

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
  if(InUInt.x > 65535 || InUInt.y > 65535 || InUInt1.x > 65535 || InUInt2.x > 65535)
    Color = vec4(0.4f, 0, 0, 1);

#if DOUBLES
  // doubles are all in range [-10, 10]
  if(clamp(InDouble, -10.0, 10.0) != InDouble)
    Color = vec4(0.5f, 0, 0, 1);
#endif

#if LONGS
  if(InULong.x < 10000000000UL || InULong.y < 10000000000UL || InULong.z < 10000000000UL)
    Color = vec4(0.6f, 0, 0, 1);
  if(InSLong.x > -10000000000UL || InSLong.y > -10000000000UL || InSLong.z > -10000000000UL)
    Color = vec4(0.7f, 0, 0, 1);
#endif
}

)EOSHADER";

  std::string geom = R"EOSHADER(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec4 InSNorm[3];
layout(location = 1) in vec4 InUNorm[3];
layout(location = 2) in vec4 InUScaled[3];
layout(location = 3) flat in uvec2 InUInt[3];
layout(location = 3, component = 2) flat in uint InUInt1[3];
layout(location = 3, component = 3) flat in uint InUInt2[3];
#if DOUBLES
layout(location = 4) in dvec3 InDouble[3];
#endif
layout(location = 6) in vec2 InArray[3][2];
layout(location = 8) in mat2x2 InMatrix[3];
#if LONGS
layout(location = 10) in u64vec3 InULong[3];
layout(location = 12) in i64vec3 InSLong[3];
#endif

layout(location = 0) out vec4 OutSNorm;
layout(location = 1) out vec4 OutUNorm;
layout(location = 2) out vec4 OutUScaled;
layout(location = 3) flat out uvec2 OutUInt;
layout(location = 3, component = 2) flat out uint OutUInt1;
layout(location = 3, component = 3) flat out uint OutUInt2;
#if DOUBLES
layout(location = 4) out dvec3 OutDouble;
#endif
layout(location = 6) out vec2 OutArray[2];
layout(location = 8) out mat2x2 OutMatrix;
#if LONGS
layout(location = 10) out u64vec3 OutULong;
layout(location = 12) out i64vec3 OutSLong;
#endif

void main()
{
  for(int i = 0; i < 3; i++)
  {
    gl_Position = vec4(gl_in[i].gl_Position.yx, 0.4f, 1.2f);

    OutSNorm = InSNorm[i];
    OutUScaled = InUScaled[i];
#if DOUBLES
    OutDouble = InDouble[i];
#endif
    OutUInt = InUInt[i];
    OutUInt1 = InUInt1[i];
    OutUInt2 = InUInt2[i];
    OutUNorm = InUNorm[i];
    OutArray = InArray[i];
    OutMatrix = InMatrix[i];
#if LONGS
    OutULong = InULong[i];
    OutSLong = InSLong[i];
#endif

    EmitVertex();
  }
  EndPrimitive();
}

)EOSHADER";

  std::string vertex2 = R"EOSHADER(
layout(location = 0) out vec4 OutDummy;

struct ArrayWrapper
{
  float foo[2];
};

struct SimpleWrapper
{
  float foo;
};

struct MyStruct
{
  float a;
  float b[2][3];
  ArrayWrapper c;
  SimpleWrapper d[2];
};

layout(location = 1) out OutData
{
  MyStruct outStruct;
} outData;

void main()
{
  const vec4 verts[3] = vec4[3](vec4(-0.5, 0.5, 0.0, 1.0), vec4(0.0, -0.5, 0.0, 1.0),
                                vec4(0.5, 0.5, 0.0, 1.0));

  gl_Position = verts[gl_VertexIndex];

  OutDummy = vec4(0,0,0,0);

  outData.outStruct.a = 1.1f;
  outData.outStruct.c.foo[0] = 4.4f;
  outData.outStruct.c.foo[1] = 5.5f;
  outData.outStruct.d[0].foo = 6.6f;
  outData.outStruct.d[1].foo = 7.7f;
  outData.outStruct.b[0][0] = 2.2f;
  outData.outStruct.b[0][1] = 3.3f;
  outData.outStruct.b[0][2] = 8.8f;
  outData.outStruct.b[1][0] = 9.9f;
  outData.outStruct.b[1][1] = 9.1f;
  outData.outStruct.b[1][2] = 8.2f;
}

)EOSHADER";

  std::string geom2 = R"EOSHADER(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec4 InDummy[3];

struct ArrayWrapper
{
  float foo[2];
};

struct SimpleWrapper
{
  float foo;
};

struct MyStruct
{
  float a;
  float b[2][3];
  ArrayWrapper c;
  SimpleWrapper d[2];
};

layout(location = 1) in OutData
{
  MyStruct inStruct;
} inData[3];

layout(location = 0) out vec4 OutDummy;

layout(location = 1) out OutData
{
  MyStruct outStruct;
} outData;

void main()
{
  for(int i = 0; i < 3; i++)
  {
    gl_Position = vec4(gl_in[i].gl_Position.yx, 0.4f, 1.2f);

    OutDummy = InDummy[i];
    outData.outStruct = inData[i].inStruct;

    EmitVertex();
  }
  EndPrimitive();
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    features.geometryShader = VK_TRUE;

    // radv doesn't support doubles :(
    optFeatures.shaderFloat64 = VK_TRUE;

    optFeatures.shaderInt64 = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(physProperties.limits.maxVertexOutputComponents < 128)
      Avail = "Not enough vertex output components to run test";

    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_R16G16B16A16_USCALED, &props);

    if((props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0)
    {
      Avail = "VK_FORMAT_R16G16B16A16_USCALED not supported in vertex buffers";
      return;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_R64G64B64_SFLOAT, &props);

    const bool doubles =
        features.shaderFloat64 && (props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0;

    props = {};
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_R64G64B64_SINT, &props);
    const bool slongs = (props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0;

    props = {};
    vkGetPhysicalDeviceFormatProperties(phys, VK_FORMAT_R64G64B64_UINT, &props);
    const bool ulongs = (props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0;

    const bool longs = features.shaderInt64 && slongs && ulongs;

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
        vkh::vertexAttrFormatted(6, 0, vertin, arr0, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(7, 0, vertin, arr1, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(8, 0, vertin, mat0, VK_FORMAT_R32G32_SFLOAT),
        vkh::vertexAttrFormatted(9, 0, vertin, mat1, VK_FORMAT_R32G32_SFLOAT),
    };

    std::string common = "#version 450 core\n\n";

    if(longs)
    {
      pipeCreateInfo.vertexInputState.vertexAttributeDescriptions.push_back(
          vkh::vertexAttrFormatted(10, 0, vertin, lf, VK_FORMAT_R64G64B64_UINT));
      pipeCreateInfo.vertexInputState.vertexAttributeDescriptions.push_back(
          vkh::vertexAttrFormatted(12, 0, vertin, slf, VK_FORMAT_R64G64B64_SINT));

      common += "#extension GL_ARB_gpu_shader_int64 : require\n\n#define LONGS 1\n\n";
    }
    else
    {
      common += "#define LONGS 0\n\n";
    }

    if(doubles)
    {
      pipeCreateInfo.vertexInputState.vertexAttributeDescriptions.push_back(
          vkh::vertexAttrFormatted(4, 0, vertin, df, VK_FORMAT_R64G64B64_SFLOAT));

      common += "#define DOUBLES 1\n\n";
    }
    else
    {
      common += "#define DOUBLES 0\n\n";
    }

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
        CompileShaderModule(common + geom, ShaderLang::glsl, ShaderStage::geom, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex2, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + geom2, ShaderLang::glsl, ShaderStage::geom, "main"),
    };

    pipeCreateInfo.rasterizationState.rasterizerDiscardEnable = VK_TRUE;

    VkPipeline pipe2 = createGraphicsPipeline(pipeCreateInfo);

    vertin triangle[] = {
        {
            {32767, -32768, 32767, -32767},
            {12345, 6789, 1234, 567},
            {9.8765432109, -5.6789012345, 1.2345},
            {1.0f, 2.0f},
            {3.0f, 4.0f},
            {7.0f, 8.0f},
            {9.0f, 10.0f},
            {10000012345, 10000006789, 10000001234},
            {-10000012345, -10000006789, -10000001234},
        },
        {
            {32766, -32766, 16000, -16000},
            {56, 7890, 123, 4567},
            {-7.89012345678, 6.54321098765, 1.2345},
            {11.0f, 12.0f},
            {13.0f, 14.0f},
            {17.0f, 18.0f},
            {19.0f, 20.0f},
            {10000000056, 10000007890, 10000000123},
            {-10000000056, -10000007890, -10000000123},
        },
        {
            {5, -5, 0, 0},
            {8765, 43210, 987, 65432},
            {0.1234567890123, 4.5678901234, 1.2345},
            {21.0f, 22.0f},
            {23.0f, 24.0f},
            {27.0f, 28.0f},
            {29.0f, 30.0f},
            {10000008765, 10000043210, 10000000987},
            {-10000008765, -10000043210, -10000000987},
        },
    };

    AllocatedBuffer vb(this,
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
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      if(doubles)
        setMarker(cmd, "DoublesEnabled");

      if(longs)
        setMarker(cmd, "LongsEnabled");

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe2);
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
