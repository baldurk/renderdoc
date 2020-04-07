/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

RD_TEST(VK_Shader_Debug_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Tests shader debugging on SPIR-V opcodes.";

  struct ConstsA2V
  {
    Vec4f pos;
    float zero;
    float one;
    float negone;
  };

  std::string v2f =
      R"EOSHADER(

struct flatv2f
{
  uint test;
  uint intval;
};

struct v2f
{
  vec2 zeroVal;
  vec2 inpos;
  vec2 inposIncreased;
  float tinyVal;
  float oneVal;
  float negoneVal;
};

layout(location = 1) inout_type flat flatv2f flatData;
layout(location = 3) inout_type v2f linearData;

)EOSHADER";

  std::string vertex = R"EOSHADER(
#version 430 core

#define inout_type out

)EOSHADER" + v2f + R"EOSHADER(

layout(location = 0) in vec4 pos;
layout(location = 1) in float zero;
layout(location = 2) in float one;
layout(location = 3) in float negone;

void main()
{
  int test = gl_InstanceIndex;
 
  gl_Position = vec4(pos.x + pos.z * float(test % 256), pos.y + pos.w * float(test / 256), 0.0, 1.0);

  const vec4 verts[4] = vec4[4](vec4(-1.0, -1.0, 0.5, 1.0), vec4(1.0, -1.0, 0.5, 1.0),
                                vec4(-1.0, 1.0, 0.5, 1.0), vec4(1.0, 1.0, 0.5, 1.0));

  const vec2 data[3] = vec2[3](vec2(10.0f, 10.0f), vec2(20.0f, 10.0f), vec2(10.0f, 20.0f));

  linearData.zeroVal = zero.xx;
  linearData.oneVal = one;
  linearData.negoneVal = negone;
  linearData.tinyVal = one * 1.0e-30;
  linearData.inpos = data[gl_VertexIndex];
  linearData.inposIncreased = data[gl_VertexIndex] * 2.75f;
  flatData.test = test;
  flatData.intval = test + 7;
}

)EOSHADER";

  std::string pixel_glsl = R"EOSHADER(
#version 460 core

#define inout_type in

)EOSHADER" + v2f + R"EOSHADER(

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  float  posinf = linearData.oneVal/linearData.zeroVal.x;
  float  neginf = linearData.negoneVal/linearData.zeroVal.x;
  float  nan = linearData.zeroVal.x/linearData.zeroVal.y;

  float negone = linearData.negoneVal;
  float posone = linearData.oneVal;
  float zero = linearData.zeroVal.x;
  float tiny = linearData.tinyVal;

  int intval = flatData.intval;

  uint test = flatData.test;

  Color = vec4(0,0,0,0);
  if(test == 0)
  {
    Color = vec4(1.0f, 2.0f, 3.0f, 4.0f);
  }
  else if(test == 1)
  {
    Color = gl_FragCoord;
  }
}

)EOSHADER";

  std::string pixel_asm = R"EOSHADER(
               OpCapability Shader
    %glsl450 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %flatData %linearData %Color %gl_FragCoord
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %flatData Flat
               OpDecorate %flatData Location 1
               OpDecorate %linearData Location 3
               OpDecorate %Color Index 0
               OpDecorate %Color Location 0
               OpDecorate %gl_FragCoord BuiltIn FragCoord

       %void = OpTypeVoid
       %bool = OpTypeBool
      %float = OpTypeFloat 32
       %uint = OpTypeInt 32 0
        %int = OpTypeInt 32 1

     %float2 = OpTypeVector %float 2
     %float3 = OpTypeVector %float 3
     %float4 = OpTypeVector %float 4

   %mainfunc = OpTypeFunction %void

        %v2f = OpTypeStruct %float2 %float2 %float2 %float %float %float
    %flatv2f = OpTypeStruct %uint %uint

    %_ptr_Input_v2f = OpTypePointer Input %v2f
%_ptr_Input_flatv2f = OpTypePointer Input %flatv2f
   %_ptr_Input_uint = OpTypePointer Input %uint
 %_ptr_Input_float4 = OpTypePointer Input %float4
%_ptr_Output_float4 = OpTypePointer Output %float4

  %linearData = OpVariable %_ptr_Input_v2f Input
    %flatData = OpVariable %_ptr_Input_flatv2f Input
%gl_FragCoord = OpVariable %_ptr_Input_float4 Input
       %Color = OpVariable %_ptr_Output_float4 Output

%flatv2f_test_idx = OpConstant %int 0
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1

    %float_0 = OpConstant %float 0
    %float_1 = OpConstant %float 1
    %float_2 = OpConstant %float 2
    %float_3 = OpConstant %float 3
    %float_4 = OpConstant %float 4

 %float_0000 = OpConstantComposite %float4 %float_0 %float_0 %float_0 %float_0
 %float_1234 = OpConstantComposite %float4 %float_1 %float_2 %float_3 %float_4

       %main = OpFunction %void None %mainfunc
 %main_begin = OpLabel
   %test_ptr = OpAccessChain %_ptr_Input_uint %flatData %flatv2f_test_idx
       %test = OpLoad %uint %test_ptr

               OpSelectionMerge %break None
               OpSwitch %test
                        %default
                        0 %test_0
                        1 %test_1
                        2 %test_2

     %test_0 = OpLabel
          %1 = OpVectorShuffle %float4 %float_0000 %float_1234 0 5 0 0
               OpStore %Color %1
               OpBranch %break

     %test_1 = OpLabel
          %2 = OpVectorShuffle %float4 %float_0000 %float_1234 7 6 0 1
               OpStore %Color %2
               OpBranch %break

     %test_2 = OpLabel
          %3 = OpVectorShuffle %float4 %float_0000 %float_1234 3 2 6 4
               OpStore %Color %3
               OpBranch %break

    %default = OpLabel
               OpStore %Color %float_0000
               OpBranch %break

      %break = OpLabel
               OpReturn
               OpFunctionEnd
)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    size_t lastTest = pixel_glsl.rfind("test == ");
    lastTest += sizeof("test == ") - 1;

    const uint32_t numGLSLTests = atoi(pixel_glsl.c_str() + lastTest) + 1;

    lastTest = pixel_asm.rfind("%test_");
    lastTest += sizeof("%test_") - 1;

    const uint32_t numASMTests = atoi(pixel_asm.c_str() + lastTest) + 1;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    // calculate number of tests (align to 64)
    uint32_t texWidth = AlignUp(std::max(numGLSLTests, numASMTests), 64U);

    // wrap around after 256
    uint32_t texHeight = std::max(1U, texWidth / 256);
    texWidth /= texHeight;

    // 4x4 for each test
    texWidth *= 4;
    texHeight *= 4;

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(texWidth, texHeight, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, VK_ATTACHMENT_LOAD_OP_CLEAR));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    VkFramebuffer framebuffer =
        createFramebuffer(vkh::FramebufferCreateInfo(renderPass, {imgview}, {texWidth, texHeight}));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, ConstsA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, ConstsA2V, pos), vkh::vertexAttr(1, 0, ConstsA2V, zero),
        vkh::vertexAttr(2, 0, ConstsA2V, one), vkh::vertexAttr(3, 0, ConstsA2V, negone),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel_glsl, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline glslpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[1] =
        CompileShaderModule(pixel_asm, ShaderLang::spvasm, ShaderStage::frag, "main");

    VkPipeline asmpipe = createGraphicsPipeline(pipeCreateInfo);

    float triWidth = 8.0f / float(texWidth);
    float triHeight = 8.0f / float(texHeight);

    ConstsA2V triangle[] = {
        {Vec4f(-1.0f, -1.0f, triWidth, triHeight), 0.0f, 1.0f, -1.0f},
        {Vec4f(-1.0f + triWidth, -1.0f, triWidth, triHeight), 0.0f, 1.0f, -1.0f},
        {Vec4f(-1.0f, -1.0f + triHeight, triWidth, triHeight), 0.0f, 1.0f, -1.0f},
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

      VkViewport v = {};
      v.maxDepth = 1.0f;
      v.width = (float)texWidth;
      v.height = (float)texHeight;

      VkRect2D s = {};
      s.extent.width = texWidth;
      s.extent.height = texHeight;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe);
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &s);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "GLSL tests");
      uint32_t numTests = numGLSLTests;
      uint32_t offset = 0;
      // loop drawing 256 tests at a time
      while(numTests > 0)
      {
        uint32_t num = std::min(numTests, 256U);
        vkCmdDraw(cmd, 3, num, 0, offset);
        offset += num;
        numTests -= num;
      }
      popMarker(cmd);

      vkCmdEndRenderPass(cmd);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, asmpipe);
      vkCmdSetViewport(cmd, 0, 1, &v);

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmd, "ASM tests");
      numTests = numASMTests;
      offset = 0;
      // loop drawing 256 tests at a time
      while(numTests > 0)
      {
        uint32_t num = std::min(numTests, 256U);
        vkCmdDraw(cmd, 3, num, 0, offset);
        offset += num;
        numTests -= num;
      }
      popMarker(cmd);

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
