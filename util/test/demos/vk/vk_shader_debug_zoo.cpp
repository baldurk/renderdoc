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
  float zerof = linearData.zeroVal.x;
  float tiny = linearData.tinyVal;

  int intval = int(flatData.intval);
  int zerou = int(flatData.intval - flatData.test - 7);
  int zeroi = int(zerou);

  uint test = flatData.test;

  vec2 inpos = linearData.inpos;
  vec2 inposIncreased = linearData.inposIncreased;

  Color = vec4(0,0,0,0);
  switch(test)
  {
    case 0:
    {
      Color = gl_FragCoord;
      break;
    }
    case 1:
    {
      Color = dFdx(gl_FragCoord);
      break;
    }
    case 2:
    {
      Color = dFdy(gl_FragCoord);
      break;
    }
    case 3:
    {
      Color = dFdxCoarse(gl_FragCoord);
      break;
    }
    case 4:
    {
      Color = dFdyCoarse(gl_FragCoord);
      break;
    }
    case 5:
    {
      Color = dFdxFine(gl_FragCoord);
      break;
    }
    case 6:
    {
      Color = dFdyFine(gl_FragCoord);
      break;
    }
    case 7:
    {
      Color = dFdx(vec4(inpos, inposIncreased));
      break;
    }
    case 8:
    {
      Color = dFdy(vec4(inpos, inposIncreased));
      break;
    }
    case 9:
    {
      Color = dFdxCoarse(vec4(inpos, inposIncreased));
      break;
    }
    case 10:
    {
      Color = dFdyCoarse(vec4(inpos, inposIncreased));
      break;
    }
    case 11:
    {
      Color = dFdxFine(vec4(inpos, inposIncreased));
      break;
    }
    case 12:
    {
      Color = dFdyFine(vec4(inpos, inposIncreased));
      break;
    }
    case 13:
    {
      Color = vec4(abs(posone*2.5f), abs(negone*2.5f), abs(zerof*2.5f), 1.0f);
      break;
    }
    case 14:
    {
      Color = vec4(pow(posone*2.5f, posone*1.3f), pow(posone*2.5f, posone*0.45f),
                   pow(vec2(posone*2.5f, posone*1.3f), vec2(posone*0.9f, posone*8.5f)));
      break;
    }
    case 15:
    {
      Color = vec4(normalize(posone*2.5f), normalize(posone), normalize(negone), 1.0f);
      break;
    }
    case 16:
    {
      Color = vec4(normalize(vec2(posone*2.5f, negone*1.8f)), normalize(vec2(posone*8.5f, negone*7.1f)));
      break;
    }
    case 17:
    {
      Color = vec4(normalize(vec3(posone*2.5f, negone*1.8f, posone*8.5f)), 1.0f);
      break;
    }
    case 18:
    {
      Color = normalize(vec4(posone*2.5f, negone*1.8f, posone*8.5f, negone*5.2f));
      break;
    }
    case 19:
    {
      Color = vec4(floor(posone*2.5f), floor(posone*2.4f), floor(posone*2.6f), floor(zerof));
      break;
    }
    case 20:
    {
      Color = vec4(floor(negone*2.5f), floor(negone*2.4f), floor(negone*2.6f), 1.0f);
      break;
    }
    case 21:
    {
      Color = vec4(mix(posone*1.1f, posone*3.3f, 0.5f),
                   mix(posone*1.1f, posone*3.3f, 0.2f),
                   mix(posone*1.1f, posone*3.3f, 0.8f),
                   1.0f);
      break;
    }
    case 22:
    {
      Color = vec4(mix(posone*1.1f, posone*3.3f, 1.5f),
                   mix(posone*1.1f, posone*3.3f, -0.3f),
                   0.0f,
                   1.0f);
      break;
    }
    case 23:
    {
      Color = vec4(mix(posone*3.3f, posone*1.1f, 0.5f),
                   mix(posone*3.3f, posone*1.1f, 0.2f),
                   mix(posone*3.3f, posone*1.1f, 0.8f),
                   1.0f);
      break;
    }
    case 24:
    {
      vec3 a = vec3(posone*2.5f, negone*1.8f, posone*8.5f);
      vec3 b = vec3(negone*6.3f, posone*3.2f, negone*0.4f);
      Color = vec4(cross(a, b), 1.0f);
      break;
    }
    default: break;
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
    %_ptr_Input_int = OpTypePointer Input %int
  %_ptr_Input_float = OpTypePointer Input %float
 %_ptr_Input_float2 = OpTypePointer Input %float2
 %_ptr_Input_float4 = OpTypePointer Input %float4
%_ptr_Output_float4 = OpTypePointer Output %float4

  %linearData = OpVariable %_ptr_Input_v2f Input
    %flatData = OpVariable %_ptr_Input_flatv2f Input
%gl_FragCoord = OpVariable %_ptr_Input_float4 Input
       %Color = OpVariable %_ptr_Output_float4 Output

       %flatv2f_test_idx = OpConstant %int 0
     %flatv2f_intval_idx = OpConstant %int 1

        %v2f_zeroVal_idx = OpConstant %int 0
          %v2f_inpos_idx = OpConstant %int 1
 %v2f_inposIncreased_idx = OpConstant %int 2
        %v2f_tinyVal_idx = OpConstant %int 3
         %v2f_oneVal_idx = OpConstant %int 4
      %v2f_negoneVal_idx = OpConstant %int 5

     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
     %uint_3 = OpConstant %uint 3
     %uint_4 = OpConstant %uint 4
     %uint_15 = OpConstant %uint 15

     %uint_1_234 = OpConstant %uint 0x3f9df3b6

     %int_4 = OpConstant %int 4
     %int_neg4 = OpConstant %int -4
     %int_7 = OpConstant %int 7
     %int_15 = OpConstant %int 15
     %int_neg15 = OpConstant %int -15

    %float_0 = OpConstant %float 0
    %float_1 = OpConstant %float 1
    %float_2 = OpConstant %float 2
    %float_3 = OpConstant %float 3
    %float_4 = OpConstant %float 4
    %float_neg4 = OpConstant %float -4
    %float_15 = OpConstant %float 15
    %float_neg15 = OpConstant %float -15

 %float_0000 = OpConstantComposite %float4 %float_0 %float_0 %float_0 %float_0
 %float_1234 = OpConstantComposite %float4 %float_1 %float_2 %float_3 %float_4

       %main = OpFunction %void None %mainfunc
 %main_begin = OpLabel
   %test_ptr = OpAccessChain %_ptr_Input_uint %flatData %flatv2f_test_idx
       %test = OpLoad %uint %test_ptr

%zeroVal_ptr = OpAccessChain %_ptr_Input_float2 %linearData %v2f_zeroVal_idx
    %zeroVal = OpLoad %float2 %zeroVal_ptr
  %zeroVal_x = OpCompositeExtract %float %zeroVal 0
  %zeroVal_y = OpCompositeExtract %float %zeroVal 1
      %zerof = OpCompositeExtract %float %zeroVal 0

  %inpos_ptr = OpAccessChain %_ptr_Input_float2 %linearData %v2f_inpos_idx
      %inpos = OpLoad %float2 %inpos_ptr

  %inposIncreased_ptr = OpAccessChain %_ptr_Input_float2 %linearData %v2f_inposIncreased_idx
      %inposIncreased = OpLoad %float2 %inposIncreased_ptr

  %tinyVal_ptr = OpAccessChain %_ptr_Input_float %linearData %v2f_tinyVal_idx
      %tinyVal = OpLoad %float %tinyVal_ptr

  %oneVal_ptr = OpAccessChain %_ptr_Input_float %linearData %v2f_oneVal_idx
      %oneVal = OpLoad %float %oneVal_ptr

  %negoneVal_ptr = OpAccessChain %_ptr_Input_float %linearData %v2f_negoneVal_idx
      %negoneVal = OpLoad %float %negoneVal_ptr

   %posinf = OpFDiv %float %oneVal %zerof
   %neginf = OpFDiv %float %negoneVal %zerof
      %nan = OpFDiv %float %zerof %zerof

  %intval_ptr = OpAccessChain %_ptr_Input_uint %flatData %flatv2f_intval_idx
   %intval = OpLoad %uint %intval_ptr
    %_tmp_7 = OpISub %uint %intval %test
    %zerou = OpISub %uint %_tmp_7 %int_7
    %zeroi = OpBitcast %int %zerou

               OpSelectionMerge %break None
               OpSwitch %test
                        %default
                         0 %test_0
                         1 %test_1
                         2 %test_2
                         3 %test_3
                         4 %test_4
                         5 %test_5
                         6 %test_6
                         7 %test_7
                         8 %test_8
                         9 %test_9
                        10 %test_10
                        11 %test_11
                        12 %test_12
                        13 %test_13
                        14 %test_14
                        15 %test_15
                        16 %test_16
                        17 %test_17
                        18 %test_18
                        19 %test_19
                        20 %test_20

     ; test OpVectorShuffle
     %test_0 = OpLabel
    %Color_0 = OpVectorShuffle %float4 %float_0000 %float_1234 7 6 0 1
               OpStore %Color %Color_0
               OpBranch %break

     ; test OpFMod
     %test_1 = OpLabel
        %a_1 = OpFAdd %float %zerof %float_15
        %b_1 = OpFAdd %float %zerof %float_4
         %_1 = OpFMod %float %a_1 %b_1
    %Color_1 = OpCompositeConstruct %float4 %_1 %_1 %_1 %_1
               OpStore %Color %Color_1
               OpBranch %break

     %test_2 = OpLabel
        %a_2 = OpFAdd %float %zerof %float_neg15
        %b_2 = OpFAdd %float %zerof %float_4
         %_2 = OpFMod %float %a_2 %b_2
    %Color_2 = OpCompositeConstruct %float4 %_2 %_2 %_2 %_2
               OpStore %Color %Color_2
               OpBranch %break

     %test_3 = OpLabel
        %a_3 = OpFAdd %float %zerof %float_15
        %b_3 = OpFAdd %float %zerof %float_neg4
         %_3 = OpFMod %float %a_3 %b_3
    %Color_3 = OpCompositeConstruct %float4 %_3 %_3 %_3 %_3
               OpStore %Color %Color_3
               OpBranch %break

     %test_4 = OpLabel
        %a_4 = OpFAdd %float %zerof %float_neg15
        %b_4 = OpFAdd %float %zerof %float_neg4
         %_4 = OpFMod %float %a_4 %b_4
    %Color_4 = OpCompositeConstruct %float4 %_4 %_4 %_4 %_4
               OpStore %Color %Color_4
               OpBranch %break

     ; test OpFRem
     %test_5 = OpLabel
        %a_5 = OpFAdd %float %zerof %float_15
        %b_5 = OpFAdd %float %zerof %float_4
         %_5 = OpFRem %float %a_5 %b_5
    %Color_5 = OpCompositeConstruct %float4 %_5 %_5 %_5 %_5
               OpStore %Color %Color_5
               OpBranch %break

     %test_6 = OpLabel
        %a_6 = OpFAdd %float %zerof %float_neg15
        %b_6 = OpFAdd %float %zerof %float_4
         %_6 = OpFRem %float %a_6 %b_6
    %Color_6 = OpCompositeConstruct %float4 %_6 %_6 %_6 %_6
               OpStore %Color %Color_6
               OpBranch %break

     %test_7 = OpLabel
        %a_7 = OpFAdd %float %zerof %float_15
        %b_7 = OpFAdd %float %zerof %float_neg4
         %_7 = OpFRem %float %a_7 %b_7
    %Color_7 = OpCompositeConstruct %float4 %_7 %_7 %_7 %_7
               OpStore %Color %Color_7
               OpBranch %break

     %test_8 = OpLabel
        %a_8 = OpFAdd %float %zerof %float_neg15
        %b_8 = OpFAdd %float %zerof %float_neg4
         %_8 = OpFRem %float %a_8 %b_8
    %Color_8 = OpCompositeConstruct %float4 %_8 %_8 %_8 %_8
               OpStore %Color %Color_8
               OpBranch %break

     ; test OpUMod
     %test_9 = OpLabel
        %a_9 = OpIAdd %uint %zerou %uint_15
        %b_9 = OpIAdd %uint %zerou %uint_4
      %mod_9 = OpUMod %uint %a_9 %b_9
         %_9 = OpConvertUToF %float %mod_9
    %Color_9 = OpCompositeConstruct %float4 %_9 %_9 %_9 %_9
               OpStore %Color %Color_9
               OpBranch %break

     ; test OpSRem
    %test_10 = OpLabel
       %a_10 = OpIAdd %int %zeroi %int_15
       %b_10 = OpIAdd %int %zeroi %int_4
     %mod_10 = OpSRem %int %a_10 %b_10
        %_10 = OpConvertSToF %float %mod_10
   %Color_10 = OpCompositeConstruct %float4 %_10 %_10 %_10 %_10
               OpStore %Color %Color_10
               OpBranch %break

    %test_11 = OpLabel
       %a_11 = OpIAdd %int %zeroi %int_neg15
       %b_11 = OpIAdd %int %zeroi %int_4
     %mod_11 = OpSRem %int %a_11 %b_11
        %_11 = OpConvertSToF %float %mod_11
   %Color_11 = OpCompositeConstruct %float4 %_11 %_11 %_11 %_11
               OpStore %Color %Color_11
               OpBranch %break

    %test_12 = OpLabel
       %a_12 = OpIAdd %int %zeroi %int_15
       %b_12 = OpIAdd %int %zeroi %int_neg4
     %mod_12 = OpSRem %int %a_12 %b_12
        %_12 = OpConvertSToF %float %mod_12
   %Color_12 = OpCompositeConstruct %float4 %_12 %_12 %_12 %_12
               OpStore %Color %Color_12
               OpBranch %break

    %test_13 = OpLabel
       %a_13 = OpIAdd %int %zeroi %int_neg15
       %b_13 = OpIAdd %int %zeroi %int_neg4
     %mod_13 = OpSRem %int %a_13 %b_13
        %_13 = OpConvertSToF %float %mod_13
   %Color_13 = OpCompositeConstruct %float4 %_13 %_13 %_13 %_13
               OpStore %Color %Color_13
               OpBranch %break

     ; test mod/rem with zeros
    %test_14 = OpLabel
       %b_14 = OpFAdd %float %zerof %float_4
        %_14 = OpFMod %float %zerof %b_14
   %Color_14 = OpCompositeConstruct %float4 %_14 %_14 %_14 %_14
               OpStore %Color %Color_14
               OpBranch %break

    %test_15 = OpLabel
       %b_15 = OpFAdd %float %zerof %float_4
        %_15 = OpFRem %float %zerof %b_15
   %Color_15 = OpCompositeConstruct %float4 %_15 %_15 %_15 %_15
               OpStore %Color %Color_15
               OpBranch %break

    %test_16 = OpLabel
       %b_16 = OpIAdd %uint %zerou %uint_4
     %mod_16 = OpUMod %uint %zerou %b_16
        %_16 = OpConvertUToF %float %mod_16
   %Color_16 = OpCompositeConstruct %float4 %_16 %_16 %_16 %_16
               OpStore %Color %Color_16
               OpBranch %break

    %test_17 = OpLabel
       %b_17 = OpIAdd %int %zeroi %int_4
     %mod_17 = OpSRem %int %zeroi %b_17
        %_17 = OpConvertSToF %float %mod_17
   %Color_17 = OpCompositeConstruct %float4 %_17 %_17 %_17 %_17
               OpStore %Color %Color_17
               OpBranch %break

     ; test bitcast
    %test_18 = OpLabel
        %_18 = OpBitcast %float %uint_1_234
   %Color_18 = OpCompositeConstruct %float4 %_18 %_18 %_18 %_18
               OpStore %Color %Color_18
               OpBranch %break

     ; test fnegate
    %test_19 = OpLabel
       %a_19 = OpFAdd %float %zerof %float_4
       %b_19 = OpFAdd %float %zerof %float_neg15
       %c_19 = OpFAdd %float %zerof %zerof
       %d_19 = OpFAdd %float %zerof %float_3
       %x_19 = OpFNegate %float %a_19
       %y_19 = OpFNegate %float %b_19
       %z_19 = OpFNegate %float %c_19
       %w_19 = OpFNegate %float %d_19
   %Color_19 = OpCompositeConstruct %float4 %x_19 %y_19 %z_19 %w_19
               OpStore %Color %Color_19
               OpBranch %break

     ; test snegate
    %test_20 = OpLabel
       %a_20 = OpIAdd %int %zeroi %int_4
       %b_20 = OpIAdd %int %zeroi %int_neg15
       %c_20 = OpIAdd %int %zeroi %zeroi
       %d_20 = OpIAdd %int %zeroi %int_7
       %x_20 = OpSNegate %int %a_20
       %y_20 = OpSNegate %int %b_20
       %z_20 = OpSNegate %int %c_20
       %w_20 = OpSNegate %int %d_20
      %xf_20 = OpConvertSToF %float %x_20
      %yf_20 = OpConvertSToF %float %y_20
      %zf_20 = OpConvertSToF %float %z_20
      %wf_20 = OpConvertSToF %float %w_20
   %Color_20 = OpCompositeConstruct %float4 %xf_20 %yf_20 %zf_20 %wf_20
               OpStore %Color %Color_20
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

    size_t lastTest = pixel_glsl.rfind("case ");
    lastTest += sizeof("case ") - 1;

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
