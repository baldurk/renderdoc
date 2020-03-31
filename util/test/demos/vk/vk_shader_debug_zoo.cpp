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
    Vec3f pos;
    float zero;
    float one;
    float negone;
  };

  const char *vertex = R"EOSHADER(
#version 430 core

layout(location = 0) in vec3 pos;
layout(location = 1) in float zero;
layout(location = 2) in float one;
layout(location = 3) in float negone;

struct v2f
{
  vec2 zeroVal;
  float tinyVal;
  float oneVal;
  float negoneVal;
  uint test;
  uint intval;
};

layout(location = 1) out flat v2f OUT;

void main()
{
  int test = gl_InstanceIndex;
 
  gl_Position = vec4(pos.x + pos.z * float(test), pos.y, 0.0, 1.0);

  OUT.zeroVal = zero.xx;
  OUT.oneVal = one;
  OUT.negoneVal = negone;
  OUT.test = test;
  OUT.tinyVal = one * 1.0e-30;
  OUT.intval = test + 7;
}

)EOSHADER";

  const char *pixel_glsl = R"EOSHADER(
#version 460 core

struct v2f
{
  vec2 zeroVal;
  float tinyVal;
  float oneVal;
  float negoneVal;
  uint test;
  uint intval;
};

layout(location = 1) in flat v2f IN;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  if(IN.test == 0)
  {
    Color = vec4(1.0f, 2.0f, 3.0f, 4.0f);
  }
  else if(IN.test == 1)
  {
    Color = gl_FragCoord;
  }
}

)EOSHADER";

  const char *pixel_asm = R"EOSHADER(
               OpCapability Shader
    %glsl450 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %IN %Color %gl_FragCoord
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %IN Flat
               OpDecorate %IN Location 1
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

        %v2f = OpTypeStruct %float2 %float %float %float %uint %uint

    %_ptr_Input_v2f = OpTypePointer Input %v2f
   %_ptr_Input_uint = OpTypePointer Input %uint
 %_ptr_Input_float4 = OpTypePointer Input %float4
%_ptr_Output_float4 = OpTypePointer Output %float4

          %IN = OpVariable %_ptr_Input_v2f Input
%gl_FragCoord = OpVariable %_ptr_Input_float4 Input
       %Color = OpVariable %_ptr_Output_float4 Output

%v2f_test_idx = OpConstant %int 4
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
   %test_ptr = OpAccessChain %_ptr_Input_uint %IN %v2f_test_idx
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
          %3 = OpVectorShuffle %float4 %float_0000 %float_1234 3 6 2 4
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

    size_t lastTest = std::string(pixel_glsl).rfind("IN.test == ");
    lastTest += sizeof("IN.test == ") - 1;

    const uint32_t numGLSLTests = atoi(pixel_glsl + lastTest) + 1;

    const uint32_t numASMTests = 3;

    const uint32_t numTests = numGLSLTests + numASMTests;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    static const uint32_t texDim = AlignUp(numTests, 64U) * 4;

    AllocatedImage img(this, vkh::ImageCreateInfo(texDim, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
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
        createFramebuffer(vkh::FramebufferCreateInfo(renderPass, {imgview}, {texDim, 4}));

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

    float triWidth = 8.0f / float(texDim);

    ConstsA2V triangle[] = {
        {Vec3f(-1.0f, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f, -1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f + triWidth, -1.0f, triWidth), 0.0f, 1.0f, -1.0f},
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
      v.width = (float)texDim;
      v.height = 4;

      VkRect2D s = {};
      s.extent.width = texDim;
      s.extent.height = 4;

      vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(renderPass, framebuffer, s,
                                                         {vkh::ClearValue(0.0f, 0.0f, 0.0f, 0.0f)}),
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, glslpipe);
      vkCmdSetViewport(cmd, 0, 1, &v);
      vkCmdSetScissor(cmd, 0, 1, &s);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      setMarker(cmd, "GLSL tests");
      vkCmdDraw(cmd, 3, numGLSLTests, 0, 0);

      v.x += numGLSLTests * 4;
      v.width -= v.x;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, asmpipe);
      vkCmdSetViewport(cmd, 0, 1, &v);

      setMarker(cmd, "ASM tests");
      vkCmdDraw(cmd, 3, numASMTests, 0, 0);

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
