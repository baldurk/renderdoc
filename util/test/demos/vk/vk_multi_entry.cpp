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

RD_TEST(VK_Multi_Entry, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Test shader modules with multiple entry points are handled correctly.";

  std::string combined_asm = R"EOSHADER(
               OpCapability Shader
          %2 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450

               OpEntryPoint Vertex %fakev "fake" %posout
               OpEntryPoint Fragment %fakef "fake" %ColorF

               OpEntryPoint Vertex %mainv "main" %vertOut %Position %_ %Color %UV
               OpEntryPoint Fragment %mainf "main" %ColorF %vertIn

               OpExecutionMode %fakef OriginUpperLeft
               OpExecutionMode %mainf OriginUpperLeft

               OpDecorate %v2f_block Block

               OpDecorate %gl_PerVertex Block
               OpDecorate %posout BuiltIn Position
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
               OpDecorate %Position Location 0
               OpDecorate %Color Location 1
               OpDecorate %UV Location 2
               OpDecorate %vertOut Location 0

               OpDecorate %vertIn Location 0
               OpDecorate %ColorF Index 0
               OpDecorate %ColorF Location 0

               OpDecorate %PushData Block
               OpMemberDecorate %PushData 0 Offset 0

               OpDecorate %tex DescriptorSet 0
               OpDecorate %tex Binding 0

               OpDecorate %tex1 DescriptorSet 0
               OpDecorate %tex1 Binding 1

               OpDecorate %tex2 DescriptorSet 0
               OpDecorate %tex2 Binding 2

       %void = OpTypeVoid

          %4 = OpTypeFunction %void

      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
    %v2float = OpTypeVector %float 2
    %v3float = OpTypeVector %float 3

        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0

  %v2f_block = OpTypeStruct %v4float %v4float %v4float

   %PushData = OpTypeStruct %uint

        %img = OpTypeImage %float 2D 0 0 0 1 Unknown
    %sampimg = OpTypeSampledImage %img

%_ptr_Output_v2f_block = OpTypePointer Output %v2f_block
%_ptr_Input_v2f_block = OpTypePointer Input %v2f_block
%_ptr_Input_v2float = OpTypePointer Input %v2float
%_ptr_Input_v3float = OpTypePointer Input %v3float
%_ptr_Input_v4float = OpTypePointer Input %v4float
%_ptr_Output_v4float = OpTypePointer Output %v4float
%_ptr_PushConstant_PushData = OpTypePointer PushConstant %PushData
%_ptr_PushConstant_uint = OpTypePointer PushConstant %uint

%gl_PerVertex = OpTypeStruct %v4float %float
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex

      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
      %int_2 = OpConstant %int 2

    %float_0 = OpConstant %float 0
    %float_1 = OpConstant %float 1
   %float_n1 = OpConstant %float -1
 %float_tiny = OpConstant %float 0.00000001
 %float_dummy = OpConstant %float 0.234

         %20 = OpConstantComposite %v3float %float_1 %float_n1 %float_1
      %dummy = OpConstantComposite %v4float %float_dummy %float_dummy %float_dummy %float_dummy

     %uint_1 = OpConstant %uint 1
   %uint_100 = OpConstant %uint 100

          %_ = OpVariable %_ptr_Output_gl_PerVertex Output

   %Position = OpVariable %_ptr_Input_v3float Input
      %Color = OpVariable %_ptr_Input_v4float Input
         %UV = OpVariable %_ptr_Input_v2float Input
    %vertOut = OpVariable %_ptr_Output_v2f_block Output

     %posout = OpVariable %_ptr_Output_v4float Output

     %vertIn = OpVariable %_ptr_Input_v2f_block Input
     %ColorF = OpVariable %_ptr_Output_v4float Output

%_ptr_UniformConstant_sampimg = OpTypePointer UniformConstant %sampimg
%TexArray = OpTypeArray %sampimg %uint_100
%_ptr_UniformConstant_TexArray = OpTypePointer UniformConstant %TexArray

       %push = OpVariable %_ptr_PushConstant_PushData PushConstant
        %tex = OpVariable %_ptr_UniformConstant_TexArray UniformConstant

        %tex1 = OpVariable %_ptr_UniformConstant_TexArray UniformConstant
        %tex2 = OpVariable %_ptr_UniformConstant_TexArray UniformConstant

      %fakev = OpFunction %void None %4
          %3 = OpLabel
               OpStore %posout %dummy
               OpReturn
               OpFunctionEnd

      %fakef = OpFunction %void None %4
          %5 = OpLabel

        %227 = OpAccessChain %_ptr_PushConstant_uint %push %int_0
       %idx1 = OpLoad %uint %227

        %235 = OpAccessChain %_ptr_UniformConstant_sampimg %tex1 %idx1
        %236 = OpLoad %sampimg %235
        %240 = OpImageSampleImplicitLod %v4float %236 %dummy
               OpStore %ColorF %240

               OpReturn
               OpFunctionEnd

      %mainv = OpFunction %void None %4
          %6 = OpLabel
         %17 = OpLoad %v3float %Position
         %21 = OpFMul %v3float %17 %20
         %22 = OpCompositeExtract %float %21 0
         %23 = OpCompositeExtract %float %21 1
         %24 = OpCompositeExtract %float %21 2
         %25 = OpCompositeConstruct %v4float %22 %23 %24 %float_1
         %27 = OpAccessChain %_ptr_Output_v4float %vertOut %int_0
               OpStore %27 %25
         %34 = OpAccessChain %_ptr_Output_v4float %vertOut %int_0
         %35 = OpLoad %v4float %34
         %36 = OpAccessChain %_ptr_Output_v4float %_ %int_0
               OpStore %36 %35
         %40 = OpLoad %v4float %Color
         %41 = OpAccessChain %_ptr_Output_v4float %vertOut %int_1
               OpStore %41 %40
         %46 = OpLoad %v2float %UV
         %48 = OpCompositeExtract %float %46 0
         %49 = OpCompositeExtract %float %46 1
         %50 = OpCompositeConstruct %v4float %48 %49 %float_0 %float_1
         %51 = OpAccessChain %_ptr_Output_v4float %vertOut %int_2
               OpStore %51 %50
               OpReturn
               OpFunctionEnd

      %mainf = OpFunction %void None %4
         %106 = OpLabel
         %117 = OpAccessChain %_ptr_Input_v4float %vertIn %int_1
         %118 = OpLoad %v4float %117

         %127 = OpAccessChain %_ptr_PushConstant_uint %push %int_0
         %idx = OpLoad %uint %127

         %135 = OpAccessChain %_ptr_UniformConstant_sampimg %tex %idx
         %136 = OpLoad %sampimg %135
         %137 = OpAccessChain %_ptr_Input_v4float %vertIn %int_2
         %139 = OpLoad %v4float %137
         %140 = OpImageSampleImplicitLod %v4float %136 %139
         %150 = OpVectorTimesScalar %v4float %140 %float_tiny

         %160 = OpFAdd %v4float %118 %150

               OpStore %ColorF %160
               OpReturn
               OpFunctionEnd

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            100,
            VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    }));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {setlayout}, {
                         vkh::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i)),
                     }));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    VkPipelineShaderStageCreateInfo shad = CompileShaderModule(
        combined_asm, ShaderLang::spvasm, ShaderStage::vert, "main", {}, SPIRVTarget::vulkan);

    pipeCreateInfo.stages = {
        shad,
        shad,
    };

    pipeCreateInfo.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView view = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    VkSampler sampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_NEAREST));

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);

    for(size_t i = 0; i < 100; i++)
    {
      vkh::updateDescriptorSets(
          device, {
                      vkh::WriteDescriptorSet(
                          descset, 0, (uint32_t)i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          {
                              vkh::DescriptorImageInfo(
                                  view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler),
                          }),
                  });
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

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, img.image),
               });

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

      Vec4i idx = {15, 15, 15, 15};
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Vec4i), &idx);

      vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset}, {});

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
