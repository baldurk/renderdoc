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

static const VkDescriptorUpdateTemplateEntryKHR constEntry = {
    4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0, 16,
};

RD_TEST(VK_Parameter_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "General tests of parameters known to cause problems - e.g. optional values that should be "
      "ignored, edge cases, special values, etc.";

  std::string xfbvertex = R"EOSHADER(

#version 460 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f_block
{
	layout(xfb_buffer = 0, xfb_offset = 0) vec4 pos;
	vec4 col;
	vec4 uv;
} vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  const std::string pixel2 = R"EOSHADER(
#version 450 core

#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform texture2D tex;

void main()
{
	Color = vec4(0, 1, 0, 1) * texelFetch(tex, ivec2(0), 0);
}

)EOSHADER";

  const std::string immutpixel = R"EOSHADER(
#version 450 core

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform sampler2D tex;

void main()
{
	Color = vec4(0, 1, 0, 1) * textureLod(tex, vec2(0), 0);
}

)EOSHADER";

  const std::string dynarraypixel = R"EOSHADER(
#version 450 core

layout(location = 0, index = 0) out vec4 Color;

layout(binding=0) uniform myUniformBuffer
{
  vec4 data;
} ubos[8];

void main()
{
	Color = ubos[7].data;
}

)EOSHADER";

  const std::string refpixel = R"EOSHADER(
#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform texture2D tex;

void main()
{
	Color = vec4(0, 1, 0, 1) * texelFetch(tex, ivec2(0), 0);
}

)EOSHADER";

  const std::string asm_vertex = R"EOSHADER(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %idx %pos %imgres
               OpDecorate %idx BuiltIn VertexIndex
               OpDecorate %pos BuiltIn Position
               OpDecorate %imgres Location 0
               OpDecorate %imgs DescriptorSet 0
               OpDecorate %imgs Binding 10
               OpMemberDecorate %PushData 0 Offset 0
               OpDecorate %PushData Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
        %int = OpTypeInt 32 1
      %float = OpTypeFloat 32
      %int_0 = OpConstant %int 0
         %10 = OpTypeImage %float 2D 0 0 0 1 Unknown
     %uint_4 = OpConstant %uint 4
%_arr_10_uint_4 = OpTypeArray %10 %uint_4
%_ptr_UniformConstant__arr_10_uint_4 = OpTypePointer UniformConstant %_arr_10_uint_4
%_ptr_UniformConstant_10 = OpTypePointer UniformConstant %10
       %imgs = OpVariable %_ptr_UniformConstant__arr_10_uint_4 UniformConstant
   %PushData = OpTypeStruct %uint
%_ptr_PushConstant_PushData = OpTypePointer PushConstant %PushData
%_ptr_PushConstant_uint = OpTypePointer PushConstant %uint
       %push = OpVariable %_ptr_PushConstant_PushData PushConstant
      %v2int = OpTypeVector %int 2
    %v4float = OpTypeVector %float 4
    %v2float = OpTypeVector %float 2
%_arr_v2float_uint_4 = OpTypeArray %v2float %uint_4
%_ptr_Function__arr_v2float_uint_4 = OpTypePointer Function %_arr_v2float_uint_4
   %float_n1 = OpConstant %float -1
    %float_1 = OpConstant %float 1
         %21 = OpConstantComposite %v2float %float_n1 %float_1
         %22 = OpConstantComposite %v2float %float_1 %float_1
         %23 = OpConstantComposite %v2float %float_n1 %float_n1
         %24 = OpConstantComposite %v2float %float_1 %float_n1
         %25 = OpConstantComposite %_arr_v2float_uint_4 %21 %22 %23 %24
%_ptr_Function_v2float = OpTypePointer Function %v2float
    %float_0 = OpConstant %float 0
%_ptr_Input_uint = OpTypePointer Input %uint
        %idx = OpVariable %_ptr_Input_uint Input
%_ptr_Output_v4float = OpTypePointer Output %v4float
%pos = OpVariable %_ptr_Output_v4float Output
%imgres = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %3
          %5 = OpLabel
         %45 = OpVariable %_ptr_Function__arr_v2float_uint_4 Function
         %39 = OpLoad %uint %idx
               OpStore %45 %25
         %49 = OpAccessChain %_ptr_Function_v2float %45 %39
         %50 = OpLoad %v2float %49
         %51 = OpCompositeExtract %float %50 0
         %52 = OpCompositeExtract %float %50 1
         %53 = OpCompositeConstruct %v4float %51 %52 %float_0 %float_1

               OpStore %pos %53

         %54 = OpAccessChain %_ptr_PushConstant_uint %push %int_0
         %55 = OpLoad %uint %54
         %56 = OpAccessChain %_ptr_UniformConstant_10 %imgs %55
         %57 = OpLoad %10 %56
         %58 = OpCompositeConstruct %v2int %int_0 %int_0
         %59 = OpImageFetch %v4float %57 %58 Lod %int_0

               OpStore %imgres %59

               OpReturn
               OpFunctionEnd
)EOSHADER";

  struct refdatastruct
  {
    VkDescriptorImageInfo sampler;
    VkDescriptorImageInfo combined;
    VkDescriptorImageInfo sampled;
    VkDescriptorImageInfo storage;
    VkBufferView unitexel;
    VkBufferView storetexel;
    VkDescriptorBufferInfo unibuf;
    VkDescriptorBufferInfo storebuf;
    VkDescriptorBufferInfo unibufdyn;
    VkDescriptorBufferInfo storebufdyn;
  };

  VkSampler refsamp[4];

  VkSampler refcombinedsamp[4];
  AllocatedImage refcombinedimg[4];
  VkImageView refcombinedimgview[4];

  AllocatedImage refsampled[4];
  VkImageView refsampledview[4];

  AllocatedImage refstorage[4];
  VkImageView refstorageview[4];

  AllocatedBuffer refunitexel[4];
  VkBufferView refunitexelview[4];

  AllocatedBuffer refstoretexel[4];
  VkBufferView refstoretexelview[4];

  AllocatedBuffer refunibuf[4];

  AllocatedBuffer refstorebuf[4];

  AllocatedBuffer refunibufdyn[4];

  AllocatedBuffer refstorebufdyn[4];

  refdatastruct GetData(uint32_t idx)
  {
    return {
        // sampler
        vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, refsamp[idx]),
        // combined
        vkh::DescriptorImageInfo(refcombinedimgview[idx], VK_IMAGE_LAYOUT_GENERAL,
                                 refcombinedsamp[idx]),
        // sampled
        vkh::DescriptorImageInfo(refsampledview[idx], VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE),
        // storage
        vkh::DescriptorImageInfo(refstorageview[idx], VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE),
        // unitexel
        refunitexelview[idx],
        // storetexel
        refstoretexelview[idx],
        // unibuf
        vkh::DescriptorBufferInfo(refunibuf[idx].buffer),
        // storebuf
        vkh::DescriptorBufferInfo(refstorebuf[idx].buffer),
        // unibufdyn
        vkh::DescriptorBufferInfo(refunibufdyn[idx].buffer),
        // storebufdyn
        vkh::DescriptorBufferInfo(refstorebufdyn[idx].buffer),
    };
  }

  void Prepare(int argc, char **argv)
  {
    optDevExts.push_back(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_TOOLING_INFO_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    static VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
    };

    if(hasExt(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
    {
      timeline.timelineSemaphore = VK_TRUE;
      timeline.pNext = (void *)devInfoNext;
      devInfoNext = &timeline;
    }

    static VkPhysicalDeviceVulkan12Features vk12 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      // don't enable any features, just link the struct in.
      // deliberately replace the VkPhysicalDeviceTimelineSemaphoreFeaturesKHR above because it was
      // rolled into this struct - so we enable that one feature if we're using it
      devInfoNext = &vk12;

      VkPhysicalDeviceVulkan12Features vk12avail = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };

      getPhysFeatures2(&vk12avail);

      if(vk12avail.timelineSemaphore)
        vk12.timelineSemaphore = VK_TRUE;
    }

    static VkPhysicalDeviceTransformFeedbackFeaturesEXT xfbFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,
    };

    if(hasExt(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME))
    {
      xfbFeats.transformFeedback = VK_TRUE;
      xfbFeats.pNext = (void *)devInfoNext;
      devInfoNext = &xfbFeats;
    }

    static VkPhysicalDeviceMultiviewFeatures multiview = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
    };

    if(hasExt(VK_KHR_MULTIVIEW_EXTENSION_NAME))
    {
      multiview.multiview = VK_TRUE;
      multiview.pNext = (void *)devInfoNext;
      devInfoNext = &multiview;
    }

    static VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
    };

    if(hasExt(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
    {
      sync2Features.synchronization2 = VK_TRUE;
      sync2Features.pNext = (void *)devInfoNext;
      devInfoNext = &sync2Features;
    }

    static VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gpl = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT,
    };

    if(hasExt(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME))
    {
      gpl.graphicsPipelineLibrary = VK_TRUE;
      gpl.pNext = (void *)devInfoNext;
      devInfoNext = &gpl;
    }

    static VkPhysicalDeviceDynamicRenderingFeaturesKHR dyn = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
    };

    if(hasExt(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME))
    {
      dyn.dynamicRendering = VK_TRUE;
      dyn.pNext = (void *)devInfoNext;
      devInfoNext = &dyn;
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    std::vector<VkPhysicalDeviceToolPropertiesEXT> tools;

    if(std::find(devExts.begin(), devExts.end(), VK_EXT_TOOLING_INFO_EXTENSION_NAME) != devExts.end())
    {
      uint32_t toolCount = 0;
      vkGetPhysicalDeviceToolPropertiesEXT(phys, &toolCount, NULL);
      tools.resize(toolCount);
      vkGetPhysicalDeviceToolPropertiesEXT(phys, &toolCount, tools.data());

      TEST_LOG("%u tools available:", toolCount);
      for(VkPhysicalDeviceToolPropertiesEXT &tool : tools)
        TEST_LOG("  - %s", tool.name);
    }

    {
      VkPhysicalDeviceProperties2KHR props2 = vkh::PhysicalDeviceProperties2KHR();

      PFN_vkGetPhysicalDeviceProperties2KHR instfn =
          (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(
              instance, "vkGetPhysicalDeviceProperties2KHR");
      PFN_vkGetPhysicalDeviceProperties2KHR devfn =
          (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetDeviceProcAddr(
              device, "vkGetPhysicalDeviceProperties2KHR");

      instfn(phys, &props2);

      TEST_LOG("Got physical device %s", props2.properties.deviceName);

      props2 = vkh::PhysicalDeviceProperties2KHR();

      if(devfn != NULL)
      {
        TEST_ERROR("Unexpected non-NULL return from vkGetDeviceProcAddr for physdev function");

        // we expect this to crash
        devfn(phys, &props2);

        return 5;
      }
    }

    {
      uint32_t physCount = 0;
      vkEnumeratePhysicalDevices(instance, &physCount, NULL);

      if(physCount > 1)
      {
        std::vector<VkPhysicalDevice> physArray;
        physArray.resize(physCount);

        physCount = 1;
        VkResult vkr = vkEnumeratePhysicalDevices(instance, &physCount, physArray.data());

        if(vkr != VK_INCOMPLETE || physCount != 1 || physArray[1] != VK_NULL_HANDLE)
        {
          TEST_ERROR(
              "vkEnumeratePhysicalDevices didn't return correct results for truncated array");
          return 3;
        }
      }
    }

    bool KHR_descriptor_update_template =
        std::find(devExts.begin(), devExts.end(),
                  VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME) != devExts.end();
    bool KHR_push_descriptor = std::find(devExts.begin(), devExts.end(),
                                         VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) != devExts.end();
    bool EXT_transform_feedback =
        std::find(devExts.begin(), devExts.end(), VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME) !=
        devExts.end();
    bool KHR_timeline_semaphore =
        std::find(devExts.begin(), devExts.end(), VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) !=
        devExts.end();

    if(physProperties.apiVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      VkPhysicalDeviceVulkan12Features vk12avail = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };

      getPhysFeatures2(&vk12avail);

      if(vk12avail.timelineSemaphore)
        KHR_timeline_semaphore = true;
    }

    VkDescriptorSetLayout setlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, VK_SHADER_STAGE_VERTEX_BIT},
    }));

    std::vector<VkDescriptorSetLayoutBinding> refsetlayoutbinds = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT},
    };

    VkDescriptorSetLayout refsetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(refsetlayoutbinds));
    VkDescriptorSetLayout refsetlayout_copy =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(refsetlayoutbinds));

    VkSampler invalidSampler = (VkSampler)0x1234;
    VkSampler validSampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));

    setName(validSampler, "validSampler");

    VkDescriptorSetLayout immutsetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
             &validSampler},
            {99, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT, &invalidSampler},
        }));

    VkPipelineLayout layout, layout_copy, reflayout;

    if(KHR_push_descriptor)
    {
      VkDescriptorSetLayout pushlayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
          {
              {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              {20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
          },
          VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout, pushlayout}));
      layout_copy = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout, pushlayout}));

      VkDescriptorSetLayout refpushlayout =
          createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
              {
                  {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
                  {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
              },
              VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      if(KHR_descriptor_update_template)
        reflayout = createPipelineLayout(
            vkh::PipelineLayoutCreateInfo({refsetlayout, refsetlayout, refpushlayout}));
      else
        reflayout =
            createPipelineLayout(vkh::PipelineLayoutCreateInfo({refsetlayout, refpushlayout}));
    }
    else
    {
      layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));
      layout_copy = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout}));

      if(KHR_descriptor_update_template)
        reflayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({refsetlayout, refsetlayout}));
      else
        reflayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({refsetlayout}));
    }

    VkPipelineLayout immutlayout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo({immutsetlayout}));

    VkDescriptorSetLayout setlayout2 = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    }));

    VkPipelineLayout layout2 = createPipelineLayout(vkh::PipelineLayoutCreateInfo({setlayout2}));

    VkDescriptorSetLayout dynamicarraysetlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 8, VK_SHADER_STAGE_FRAGMENT_BIT},
        }));

    VkPipelineLayout dynamicarraylayout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo({dynamicarraysetlayout}));

    VkDescriptorSet descset2 = allocateDescriptorSet(setlayout2);

    VkDescriptorSetLayout asm_setlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, 0},
            {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, 0},
            {2, VK_DESCRIPTOR_TYPE_SAMPLER, 3, 0},
            {5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0},
            {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, VK_SHADER_STAGE_VERTEX_BIT},
            {9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, VK_SHADER_STAGE_VERTEX_BIT},
            {10, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4, VK_SHADER_STAGE_VERTEX_BIT},
        }));

    VkDescriptorSet asm_descset = allocateDescriptorSet(asm_setlayout);

    VkDescriptorSetLayout empty_setlayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({}));

    VkDescriptorSet empty_descset = allocateDescriptorSet(empty_setlayout);

    VkPipelineLayout asm_layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {asm_setlayout, empty_setlayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 4)}));

    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

    // make the renderpass multi-pass for testing, and give each one an input attachment
    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})},
                                    VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, {},
                                    {VkAttachmentReference({1, VK_IMAGE_LAYOUT_GENERAL})});
    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})},
                                    VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED, {},
                                    {VkAttachmentReference({1, VK_IMAGE_LAYOUT_GENERAL})});

    renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency(
        0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));

    // add pointless structures to ensure they are ignored
    VkRenderPassMultiviewCreateInfo nonMultiview = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
    };

    if(hasExt(VK_KHR_MULTIVIEW_EXTENSION_NAME))
    {
      nonMultiview.pNext = renderPassCreateInfo.pNext;
      renderPassCreateInfo.pNext = &nonMultiview;
    }

    // add struct that references input attachments in multiple passes
    VkRenderPassInputAttachmentAspectCreateInfo inputAspects = {
        VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO,
    };

    VkInputAttachmentAspectReference inputAspectReferences[2] = {
        {0, 0, VK_IMAGE_ASPECT_COLOR_BIT},
        {1, 0, VK_IMAGE_ASPECT_COLOR_BIT},
    };

    inputAspects.aspectReferenceCount = 2;
    inputAspects.pAspectReferences = inputAspectReferences;

    if(hasExt(VK_KHR_MAINTENANCE2_EXTENSION_NAME))
    {
      inputAspects.pNext = renderPassCreateInfo.pNext;
      renderPassCreateInfo.pNext = &inputAspects;
    }

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.layout = reflayout;

    VkPipeline refpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.layout = immutlayout;

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(immutpixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline immutpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.layout = dynamicarraylayout;

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(dynarraypixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline dynarraypipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel2, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.layout = layout2;

    VkPipeline pipe2 = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {
        CompileShaderModule(xfbvertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel2, ShaderLang::glsl, ShaderStage::frag, "main"),
    };
    VkPipeline xfbpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages = {
        CompileShaderModule(asm_vertex, ShaderLang::spvasm, ShaderStage::vert, "main"),
    };

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    pipeCreateInfo.layout = asm_layout;

    VkPipeline asm_pipe;
    {
      vkh::GraphicsPipelineCreateInfo asm_info = pipeCreateInfo;

      asm_info.viewportState.viewports.clear();
      asm_info.viewportState.scissors.clear();
      asm_info.viewportState.viewportCount = 0;
      asm_info.viewportState.scissorCount = 0;
      asm_info.rasterizationState.rasterizerDiscardEnable = VK_TRUE;

      asm_pipe = createGraphicsPipeline(asm_info);
    }

    {
      // invalid handle - should not be used because the flag for derived pipelines is not used
      pipeCreateInfo.basePipelineHandle = (VkPipeline)0x1234;

      VkPipeline dummy;
      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipeCreateInfo, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      // invalid index - again should not be used
      pipeCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
      pipeCreateInfo.basePipelineIndex = 3;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipeCreateInfo, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      pipeCreateInfo.basePipelineIndex = -1;

      // bake the pipeline info so we can mess with the pointers it normally doesn't handle
      VkGraphicsPipelineCreateInfo *baked =
          (VkGraphicsPipelineCreateInfo *)(const VkGraphicsPipelineCreateInfo *)pipeCreateInfo;

      // NULL should be fine, we have no tessellation shaders
      baked->pTessellationState = NULL;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      // same with a garbage pointer
      baked->pTessellationState = (VkPipelineTessellationStateCreateInfo *)0x1234;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      VkPipelineViewportStateCreateInfo *viewState =
          (VkPipelineViewportStateCreateInfo *)&pipeCreateInfo.viewportState;

      baked->pViewportState = viewState;

      // viewport and scissor are already dynamic, so just set viewports and scissors to invalid
      // pointers
      viewState->pViewports = (VkViewport *)0x1234;
      viewState->pScissors = (VkRect2D *)0x1234;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      // if we disable rasterization, tons of things can be NULL/garbage
      pipeCreateInfo.rasterizationState.rasterizerDiscardEnable = VK_TRUE;

      baked->pViewportState = NULL;
      baked->pMultisampleState = NULL;
      baked->pDepthStencilState = NULL;
      baked->pColorBlendState = NULL;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);

      baked->pViewportState = (VkPipelineViewportStateCreateInfo *)0x1234;
      baked->pMultisampleState = (VkPipelineMultisampleStateCreateInfo *)0x1234;
      baked->pDepthStencilState = (VkPipelineDepthStencilStateCreateInfo *)0x1234;
      baked->pColorBlendState = (VkPipelineColorBlendStateCreateInfo *)0x1234;

      CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, baked, NULL, &dummy));
      vkDestroyPipeline(device, dummy, NULL);
    }

    AllocatedBuffer vb;

    if(std::find(devExts.begin(), devExts.end(), VK_KHR_BIND_MEMORY_2_EXTENSION_NAME) != devExts.end())
    {
      // if we have bind memory 2, try to bind two buffers simultaneously in one call and delete the
      // other buffer. This ensure we don't try to bind an invalid buffer on replay.
      VkBuffer tmp = VK_NULL_HANDLE;
      VmaAllocation tmpAlloc = {};

      vkCreateBuffer(device,
                     vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                     NULL, &vb.buffer);
      vkCreateBuffer(device,
                     vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                     NULL, &tmp);

      VmaAllocationCreateInfo allocInfo = {0, VMA_MEMORY_USAGE_CPU_TO_GPU};
      allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

      VmaAllocationInfo vbInfo, tmpInfo;

      vmaAllocateMemoryForBuffer(allocator, vb.buffer, &allocInfo, &vb.alloc, &vbInfo);
      vmaAllocateMemoryForBuffer(allocator, tmp, &allocInfo, &tmpAlloc, &tmpInfo);

      VkBindBufferMemoryInfoKHR binds[2] = {};
      binds[0].sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR;
      binds[0].buffer = vb.buffer;
      binds[0].memory = vbInfo.deviceMemory;
      binds[0].memoryOffset = vbInfo.offset;
      binds[1].sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR;
      binds[1].buffer = tmp;
      binds[1].memory = tmpInfo.deviceMemory;
      binds[1].memoryOffset = tmpInfo.offset;
      vkBindBufferMemory2KHR(device, 2, binds);

      vmaFreeMemory(allocator, tmpAlloc);
      vkDestroyBuffer(device, tmp, NULL);

      vb.allocator = allocator;
      bufferAllocs[vb.buffer] = vb.alloc;
    }
    else
    {
      vb = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    }

    vb.upload(DefaultTri);

    VkDescriptorSet descset = allocateDescriptorSet(setlayout);
    VkDescriptorSet refdescset = allocateDescriptorSet(refsetlayout);
    VkDescriptorSet reftempldescset = allocateDescriptorSet(refsetlayout);

    VkDescriptorSet immutdescset = allocateDescriptorSet(immutsetlayout);
    VkDescriptorSet dynamicarrayset = allocateDescriptorSet(dynamicarraysetlayout);

    AllocatedBuffer buf(this,
                        vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                        VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT),
                        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    float bufData[1024 / sizeof(float)] = {};
    bufData[64] = 0.0f;
    bufData[65] = 1.0f;
    bufData[66] = 0.0f;
    bufData[67] = 1.0f;
    buf.upload(bufData);

    VkBuffer invalidBuffer = (VkBuffer)0x1234;
    VkBuffer validBuffer = buf.buffer;

    AllocatedImage img(this,
                       vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                                VK_IMAGE_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImage validImage = img.image;

    VkImageView validImgView = createImageView(
        vkh::ImageViewCreateInfo(validImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));
    VkImageView invalidImgView = (VkImageView)0x1234;

    AllocatedImage inputattach(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView inputview = createImageView(vkh::ImageViewCreateInfo(
        inputattach.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    VkFramebuffer fbs[8];
    for(size_t i = 0; i < mainWindow->GetCount(); i++)
      fbs[i] = createFramebuffer(vkh::FramebufferCreateInfo(
          renderPass, {mainWindow->GetView(i), inputview}, mainWindow->scissor.extent));

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });
      vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f), 1,
                           vkh::ImageSubresourceRange());
      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, img.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                      inputattach.image),
          });
      vkEndCommandBuffer(cmd);
      Submit(99, 99, {cmd});
    }

    VkBufferView validBufView =
        createBufferView(vkh::BufferViewCreateInfo(validBuffer, VK_FORMAT_R32G32B32A32_SFLOAT));
    VkBufferView invalidBufView = (VkBufferView)0x1234;

    // initialise the writes with the valid data
    std::vector<VkDescriptorBufferInfo> validBufInfos = {vkh::DescriptorBufferInfo(validBuffer)};
    std::vector<VkBufferView> validBufViews = {validBufView};
    std::vector<VkDescriptorImageInfo> validSoloImgs = {
        vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL),
    };
    std::vector<VkDescriptorImageInfo> validCombinedImgs = {
        vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL, validSampler),
    };
    std::vector<VkDescriptorImageInfo> validSamplers = {
        vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, validSampler),
    };

    std::vector<VkWriteDescriptorSet> writes = {
        vkh::WriteDescriptorSet(descset, 0, VK_DESCRIPTOR_TYPE_SAMPLER, validSamplers),
        vkh::WriteDescriptorSet(descset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                validCombinedImgs),
        vkh::WriteDescriptorSet(descset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, validSoloImgs),
        vkh::WriteDescriptorSet(descset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, validSoloImgs),

        vkh::WriteDescriptorSet(descset, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, validBufViews),
        vkh::WriteDescriptorSet(descset, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, validBufViews),

        vkh::WriteDescriptorSet(descset, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, validBufInfos),
        vkh::WriteDescriptorSet(descset, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, validBufInfos),
        vkh::WriteDescriptorSet(descset, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, validBufInfos),
        vkh::WriteDescriptorSet(descset, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, validBufInfos),

        vkh::WriteDescriptorSet(immutdescset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                validCombinedImgs),
        vkh::WriteDescriptorSet(immutdescset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                validSoloImgs),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
        vkh::WriteDescriptorSet(dynamicarrayset, 0, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                validBufInfos),
    };

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(dynamicarrayset, 0, 7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    {vkh::DescriptorBufferInfo(validBuffer, 0, 256)}),
        });

    // do a first update
    vkh::updateDescriptorSets(device, writes);

    // set invalid handles but valid pointers and try again
    VkDescriptorBufferInfo invalidBufInfo = {};
    invalidBufInfo.buffer = invalidBuffer;

    VkDescriptorImageInfo invalidImgInfo = {};
    invalidImgInfo.sampler = invalidSampler;
    invalidImgInfo.imageView = invalidImgView;

    validSoloImgs[0].sampler = invalidSampler;
    validSamplers[0].imageView = invalidImgView;

    writes[0].pTexelBufferView = &invalidBufView;
    writes[0].pBufferInfo = &invalidBufInfo;

    writes[1].pTexelBufferView = &invalidBufView;
    writes[1].pBufferInfo = &invalidBufInfo;

    writes[2].pTexelBufferView = &invalidBufView;
    writes[2].pBufferInfo = &invalidBufInfo;

    writes[3].pTexelBufferView = &invalidBufView;
    writes[3].pBufferInfo = &invalidBufInfo;

    writes[4].pImageInfo = &invalidImgInfo;
    writes[4].pBufferInfo = &invalidBufInfo;

    writes[5].pImageInfo = &invalidImgInfo;
    writes[5].pBufferInfo = &invalidBufInfo;

    writes[6].pTexelBufferView = &invalidBufView;
    writes[6].pImageInfo = &invalidImgInfo;

    writes[7].pTexelBufferView = &invalidBufView;
    writes[7].pImageInfo = &invalidImgInfo;

    writes[8].pTexelBufferView = &invalidBufView;
    writes[8].pImageInfo = &invalidImgInfo;

    writes[9].pTexelBufferView = &invalidBufView;
    writes[9].pImageInfo = &invalidImgInfo;

    writes[10].pTexelBufferView = &invalidBufView;
    writes[10].pBufferInfo = &invalidBufInfo;

    vkh::updateDescriptorSets(device, writes);

    // finally set invalid pointers too
    VkBufferView *invalidBufViews = (VkBufferView *)0x1234;
    vkh::DescriptorBufferInfo *invalidBufInfos = (vkh::DescriptorBufferInfo *)0x1234;
    vkh::DescriptorImageInfo *invalidImgInfos = (vkh::DescriptorImageInfo *)0x1234;

    writes[0].pTexelBufferView = invalidBufViews;
    writes[0].pBufferInfo = invalidBufInfos;

    writes[1].pTexelBufferView = invalidBufViews;
    writes[1].pBufferInfo = invalidBufInfos;

    writes[2].pTexelBufferView = invalidBufViews;
    writes[2].pBufferInfo = invalidBufInfos;

    writes[3].pTexelBufferView = invalidBufViews;
    writes[3].pBufferInfo = invalidBufInfos;

    writes[4].pImageInfo = invalidImgInfos;
    writes[4].pBufferInfo = invalidBufInfos;

    writes[5].pImageInfo = invalidImgInfos;
    writes[5].pBufferInfo = invalidBufInfos;

    writes[6].pTexelBufferView = invalidBufViews;
    writes[6].pImageInfo = invalidImgInfos;

    writes[7].pTexelBufferView = invalidBufViews;
    writes[7].pImageInfo = invalidImgInfos;

    writes[8].pTexelBufferView = invalidBufViews;
    writes[8].pImageInfo = invalidImgInfos;

    writes[9].pTexelBufferView = invalidBufViews;
    writes[9].pImageInfo = invalidImgInfos;

    vkh::updateDescriptorSets(device, writes);

    VkDescriptorUpdateTemplateKHR reftempl = VK_NULL_HANDLE;

    if(KHR_descriptor_update_template)
    {
      struct datastruct
      {
        VkBufferView view;
        VkDescriptorBufferInfo buf;
        VkDescriptorImageInfo img;
        VkDescriptorImageInfo combined;
        VkDescriptorImageInfo sampler;
      } data;

      data.view = validBufView;
      data.buf = validBufInfos[0];
      data.img = vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL);
      data.combined = vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL, validSampler);
      data.sampler =
          vkh::DescriptorImageInfo(VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED, validSampler);
      data.img.sampler = invalidSampler;
      data.sampler.imageView = invalidImgView;

      std::vector<VkDescriptorUpdateTemplateEntryKHR> entries = {
          // descriptor count 0 updates are allowed
          {0, 0, 0, VK_DESCRIPTOR_TYPE_SAMPLER, 0, sizeof(data)},
          {0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, offsetof(datastruct, sampler), sizeof(data)},
          {1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(datastruct, combined),
           sizeof(data)},
          {2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, offsetof(datastruct, img), sizeof(data)},
          {3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, offsetof(datastruct, img), sizeof(data)},
          {4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, offsetof(datastruct, view), sizeof(data)},
          {5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(datastruct, view), sizeof(data)},
          {6, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, offsetof(datastruct, buf), sizeof(data)},
          {7, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, offsetof(datastruct, buf), sizeof(data)},
          {8, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, offsetof(datastruct, buf),
           sizeof(data)},
          {9, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, offsetof(datastruct, buf),
           sizeof(data)},
      };

      VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();
      createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
      createInfo.descriptorSetLayout = setlayout;
      createInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      createInfo.pipelineLayout = (VkPipelineLayout)0x1234;
      createInfo.set = 123456789;
      VkDescriptorUpdateTemplateKHR templ;
      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &templ);

      vkUpdateDescriptorSetWithTemplateKHR(device, descset, templ, &data);

      vkDestroyDescriptorUpdateTemplateKHR(device, templ, NULL);

      // try with constant entry
      createInfo.descriptorUpdateEntryCount = 1;
      createInfo.pDescriptorUpdateEntries = &constEntry;
      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &templ);
      vkUpdateDescriptorSetWithTemplateKHR(device, descset, templ, &data);
      vkDestroyDescriptorUpdateTemplateKHR(device, templ, NULL);

      entries = {
          {0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, offsetof(refdatastruct, sampler), sizeof(data)},
          {1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(refdatastruct, combined),
           sizeof(data)},
          {2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, offsetof(refdatastruct, sampled), sizeof(data)},
          {3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, offsetof(refdatastruct, storage), sizeof(data)},
          {4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, offsetof(refdatastruct, unitexel),
           sizeof(data)},
          {5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(refdatastruct, storetexel),
           sizeof(data)},
          {6, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, offsetof(refdatastruct, unibuf), sizeof(data)},
          {7, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, offsetof(refdatastruct, storebuf),
           sizeof(data)},
          {8, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, offsetof(refdatastruct, unibufdyn),
           sizeof(data)},
          {9, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, offsetof(refdatastruct, storebufdyn),
           sizeof(data)},
      };

      createInfo.descriptorSetLayout = refsetlayout_copy;
      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();

      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &reftempl);
    }

    struct pushdatastruct
    {
      VkDescriptorBufferInfo buf;
    } pushdata;

    pushdata.buf = validBufInfos[0];

    VkDescriptorUpdateTemplateKHR pushtempl = VK_NULL_HANDLE, refpushtempl = VK_NULL_HANDLE;
    if(KHR_descriptor_update_template && KHR_push_descriptor)
    {
      std::vector<VkDescriptorUpdateTemplateEntryKHR> entries = {
          {0, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, sizeof(pushdata)},
          {10, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, sizeof(pushdata)},
      };

      VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();
      createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
      createInfo.descriptorSetLayout = (VkDescriptorSetLayout)0x1234;
      createInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      createInfo.pipelineLayout = layout_copy;
      createInfo.set = 1;
      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &pushtempl);

      entries = {
          {0, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLER, offsetof(refdatastruct, sampler),
           sizeof(refdatastruct)},
          {1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(refdatastruct, combined),
           sizeof(refdatastruct)},
          {2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, offsetof(refdatastruct, sampled),
           sizeof(refdatastruct)},
          {3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, offsetof(refdatastruct, storage),
           sizeof(refdatastruct)},
          {4, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, offsetof(refdatastruct, unitexel),
           sizeof(refdatastruct)},
          {5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(refdatastruct, storetexel),
           sizeof(refdatastruct)},
          {6, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, offsetof(refdatastruct, unibuf),
           sizeof(refdatastruct)},
          {7, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, offsetof(refdatastruct, storebuf),
           sizeof(refdatastruct)},
      };

      createInfo.descriptorUpdateEntryCount = (uint32_t)entries.size();
      createInfo.pDescriptorUpdateEntries = entries.data();
      // set 0 = normal
      // set 1 = template
      // set 2 = push
      createInfo.pipelineLayout = reflayout;
      createInfo.set = 2;

      vkCreateDescriptorUpdateTemplateKHR(device, &createInfo, NULL, &refpushtempl);
    }

    // check that stale views in descriptors don't cause problems if the handle is re-used

    VkImageView view1, view2;
    CHECK_VKR(vkCreateImageView(
        device,
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT),
        NULL, &view1));
    CHECK_VKR(vkCreateImageView(
        device,
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT),
        NULL, &view2));

    vkh::updateDescriptorSets(
        device,
        {
            // bind view1 to binding 0, we will override this
            vkh::WriteDescriptorSet(descset2, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view1, VK_IMAGE_LAYOUT_GENERAL)}),
            // we bind view2 to binding 1. This will become stale
            vkh::WriteDescriptorSet(descset2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view2, VK_IMAGE_LAYOUT_GENERAL)}),
        });

    vkDestroyImageView(device, view2, NULL);

    // create view3. Under RD, this is expected to get the same handle as view2 (but a new ID)
    VkImageView view3;
    CHECK_VKR(vkCreateImageView(
        device,
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT),
        NULL, &view3));

    if(rdoc)
    {
      TEST_ASSERT(view2 == view3,
                  "Expected view3 to be a re-used handle. Test isn't going to be valid");
    }

    vkh::updateDescriptorSets(
        device,
        {
            // bind view3 to 0. This means the same handle is now in both binding but only binding 0
            // is valid, binding 1 refers to the 'old' version of this handle.
            vkh::WriteDescriptorSet(descset2, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view3, VK_IMAGE_LAYOUT_GENERAL)}),
            // this unbinds the stale view2. Nothing should happen, but if we're comparing by handle
            // this may remove a reference to view3 since it will have the same handle
            vkh::WriteDescriptorSet(descset2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view1, VK_IMAGE_LAYOUT_GENERAL)}),
        });

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(asm_descset, 10, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view3, VK_IMAGE_LAYOUT_GENERAL)}),
            vkh::WriteDescriptorSet(asm_descset, 10, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view3, VK_IMAGE_LAYOUT_GENERAL)}),
            vkh::WriteDescriptorSet(asm_descset, 10, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view3, VK_IMAGE_LAYOUT_GENERAL)}),
            vkh::WriteDescriptorSet(asm_descset, 10, 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                    {vkh::DescriptorImageInfo(view3, VK_IMAGE_LAYOUT_GENERAL)}),
        });

    VkSampler mutableSampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_NEAREST));

    setName(mutableSampler, "mutableSampler");

    // try writing a different sampler to the immutable sampler, it should not be applied
    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(
                immutdescset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                {
                    vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL, mutableSampler),
                }),
        });

    refdatastruct resetrefdata = {};
    resetrefdata.sampler.sampler = resetrefdata.combined.sampler = validSampler;
    resetrefdata.sampled.imageView = resetrefdata.combined.imageView =
        resetrefdata.storage.imageView = validImgView;
    resetrefdata.sampled.imageLayout = resetrefdata.combined.imageLayout =
        resetrefdata.storage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    resetrefdata.unitexel = resetrefdata.storetexel = validBufView;
    resetrefdata.unibuf.buffer = resetrefdata.storebuf.buffer = resetrefdata.unibufdyn.buffer =
        resetrefdata.storebufdyn.buffer = validBuffer;
    resetrefdata.unibuf.range = resetrefdata.storebuf.range = resetrefdata.unibufdyn.range =
        resetrefdata.storebufdyn.range = VK_WHOLE_SIZE;

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
      vkh::cmdPipelineBarrier(cmd, {
                                       vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, img.image),
                                   });

      // create the specific resources that will only be referenced through descriptor updates
      for(int i = 0; i < 4; i++)
      {
        refsamp[i] = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
        setName(refsamp[i], "refsamp" + std::to_string(i));

        refcombinedsamp[i] = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
        setName(refcombinedsamp[i], "refcombinedsamp" + std::to_string(i));

        VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
        VmaAllocationCreateInfo allocInfo = {0, VMA_MEMORY_USAGE_GPU_ONLY};

        refcombinedimg[i] = AllocatedImage(
            this, vkh::ImageCreateInfo(2, 2, 0, fmt, VK_IMAGE_USAGE_SAMPLED_BIT), allocInfo);
        refcombinedimgview[i] = createImageView(
            vkh::ImageViewCreateInfo(refcombinedimg[i].image, VK_IMAGE_VIEW_TYPE_2D, fmt));
        setName(refcombinedimg[i].image, "refcombinedimg" + std::to_string(i));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL, refcombinedimg[i].image),
                 });

        refsampled[i] = AllocatedImage(
            this, vkh::ImageCreateInfo(2, 2, 0, fmt, VK_IMAGE_USAGE_SAMPLED_BIT), allocInfo);
        refsampledview[i] = createImageView(
            vkh::ImageViewCreateInfo(refsampled[i].image, VK_IMAGE_VIEW_TYPE_2D, fmt));
        setName(refsampled[i].image, "refsampled" + std::to_string(i));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL, refsampled[i].image),
                 });

        refstorage[i] = AllocatedImage(
            this, vkh::ImageCreateInfo(2, 2, 0, fmt, VK_IMAGE_USAGE_STORAGE_BIT), allocInfo);
        refstorageview[i] = createImageView(
            vkh::ImageViewCreateInfo(refstorage[i].image, VK_IMAGE_VIEW_TYPE_2D, fmt));
        setName(refstorage[i].image, "refstorage" + std::to_string(i));

        vkh::cmdPipelineBarrier(
            cmd, {
                     vkh::ImageMemoryBarrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL, refstorage[i].image),
                 });

        refunitexel[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT), allocInfo);
        refunitexelview[i] = createBufferView(vkh::BufferViewCreateInfo(refunitexel[i].buffer, fmt));
        setName(refunitexel[i].buffer, "refunitexel" + std::to_string(i));

        refstoretexel[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT), allocInfo);
        refstoretexelview[i] =
            createBufferView(vkh::BufferViewCreateInfo(refstoretexel[i].buffer, fmt));
        setName(refstoretexel[i].buffer, "refstoretexel" + std::to_string(i));

        refunibuf[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), allocInfo);
        setName(refunibuf[i].buffer, "refunibuf" + std::to_string(i));

        refstorebuf[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), allocInfo);
        setName(refstorebuf[i].buffer, "refstorebuf" + std::to_string(i));

        refunibufdyn[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), allocInfo);
        setName(refunibufdyn[i].buffer, "refunibufdyn" + std::to_string(i));

        refstorebufdyn[i] = AllocatedBuffer(
            this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), allocInfo);
        setName(refstorebufdyn[i].buffer, "refstorebufdyn" + std::to_string(i));
      }

      vkEndCommandBuffer(cmd);
      Submit(99, 99, {cmd});
    }

    VkBufferUsageFlags xfbUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if(EXT_transform_feedback)
    {
      xfbUsage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
      xfbUsage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
    }

    AllocatedBuffer xfbBuf(this, vkh::BufferCreateInfo(256, xfbUsage),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    if(vkSetDebugUtilsObjectNameEXT)
    {
      VkDebugUtilsObjectNameInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      info.objectType = VK_OBJECT_TYPE_BUFFER;
      info.objectHandle = (uint64_t)xfbBuf.buffer;
      info.pObjectName = NULL;
      vkSetDebugUtilsObjectNameEXT(device, &info);
    }

    VkFence fence;
    CHECK_VKR(vkCreateFence(device, vkh::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT), NULL, &fence));

    VkEvent ev;
    CHECK_VKR(vkCreateEvent(device, vkh::EventCreateInfo(), NULL, &ev));

    VkSemaphore sem = VK_NULL_HANDLE;

    if(KHR_timeline_semaphore)
    {
      VkSemaphoreTypeCreateInfo semType = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
      semType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      semType.initialValue = 1234;

      CHECK_VKR(vkCreateSemaphore(device, vkh::SemaphoreCreateInfo().next(&semType), NULL, &sem));
    }

    // check destroying NULL objects
    vkDestroyBuffer(device, NULL, NULL);
    vkDestroyBufferView(device, NULL, NULL);
    vkDestroyCommandPool(device, NULL, NULL);
    vkDestroyDescriptorPool(device, NULL, NULL);
    vkDestroyDescriptorSetLayout(device, NULL, NULL);
    vkDestroyDevice(NULL, NULL);
    vkDestroyEvent(device, NULL, NULL);
    vkDestroyFence(device, NULL, NULL);
    vkDestroyFramebuffer(device, NULL, NULL);
    vkDestroyImage(device, NULL, NULL);
    vkDestroyImageView(device, NULL, NULL);
    vkDestroyInstance(NULL, NULL);
    vkDestroyPipeline(device, NULL, NULL);
    vkDestroyPipelineCache(device, NULL, NULL);
    vkDestroyPipelineLayout(device, NULL, NULL);
    vkDestroyQueryPool(device, NULL, NULL);
    vkDestroyRenderPass(device, NULL, NULL);
    vkDestroySampler(device, NULL, NULL);
    vkDestroySemaphore(device, NULL, NULL);
    vkDestroyShaderModule(device, NULL, NULL);
    vkDestroySurfaceKHR(instance, NULL, NULL);
    vkDestroySwapchainKHR(device, NULL, NULL);
    if(KHR_descriptor_update_template)
    {
      vkDestroyDescriptorUpdateTemplateKHR(device, NULL, NULL);
    }

    VkCommandPool cmdPool;
    CHECK_VKR(vkCreateCommandPool(device, vkh::CommandPoolCreateInfo(), NULL, &cmdPool));
    VkDescriptorPool descPool;
    CHECK_VKR(vkCreateDescriptorPool(
        device,
        vkh::DescriptorPoolCreateInfo(128,
                                      {
                                          {VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
                                      },
                                      VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT),
        NULL, &descPool));

    VkCommandBuffer emptyCmd = VK_NULL_HANDLE;
    vkFreeCommandBuffers(device, cmdPool, 1, &emptyCmd);
    VkDescriptorSet emptyDesc = VK_NULL_HANDLE;
    vkFreeDescriptorSets(device, descPool, 1, &emptyDesc);
    vkFreeMemory(device, VK_NULL_HANDLE, NULL);

    vkDestroyCommandPool(device, cmdPool, NULL);
    vkDestroyDescriptorPool(device, descPool, NULL);

    if(hasExt(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME) &&
       hasExt(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME))
    {
      // create a vertex-input pipeline, with dynamic rendering enabled, and pass garbage in the
      // dynamic rendering struct. Specify the vertex-pipeline flag *after* the dynamic rendering
      // struct to force two-pass processing

      VkGraphicsPipelineLibraryCreateInfoEXT libInfo = {};
      libInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
      libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

      // none of this should be used
      VkPipelineRenderingCreateInfoKHR dynInfo = {};
      dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
      dynInfo.viewMask = 0x12345678;
      dynInfo.depthAttachmentFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
      dynInfo.stencilAttachmentFormat = VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK;
      dynInfo.pColorAttachmentFormats = (VkFormat *)0x1234;
      dynInfo.colorAttachmentCount = 1234;

      vkh::GraphicsPipelineCreateInfo libCreateInfo;

      libCreateInfo.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;

      libCreateInfo.layout = layout;

      libCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
      libCreateInfo.vertexInputState.vertexAttributeDescriptions = {
          vkh::vertexAttr(0, 0, DefaultA2V, pos),
          vkh::vertexAttr(1, 0, DefaultA2V, col),
          vkh::vertexAttr(2, 0, DefaultA2V, uv),
      };

      libCreateInfo.pNext = &dynInfo;
      dynInfo.pNext = &libInfo;

      createGraphicsPipeline(libCreateInfo);

      // for a pre-raster or fragment shader pipeline the viewmask is used, but not the formats still
      dynInfo.viewMask = 1;

      libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

      libCreateInfo.vertexInputState.vertexBindingDescriptions.clear();
      libCreateInfo.vertexInputState.vertexAttributeDescriptions.clear();

      libCreateInfo.stages = {
          CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
      };

      createGraphicsPipeline(libCreateInfo);

      libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
      libCreateInfo.stages = {
          CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
      };

      createGraphicsPipeline(libCreateInfo);
    }

    while(Running())
    {
      // acquire and clear the backbuffer
      {
        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        vkEndCommandBuffer(cmd);

        Submit(0, 4, {cmd});
      }

      // try writing with an invalid sampler to the immutable, it should be ignored
      vkh::updateDescriptorSets(
          device,
          {
              vkh::WriteDescriptorSet(
                  immutdescset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                  {
                      vkh::DescriptorImageInfo(validImgView, VK_IMAGE_LAYOUT_GENERAL, invalidSampler),
                  }),
          });

      // do a bunch of spinning on fences/semaphores that should not be serialised exhaustively
      VkResult status = VK_SUCCESS;
      for(size_t i = 0; i < 1000; i++)
        status = vkGetFenceStatus(device, fence);

      if(status != VK_SUCCESS)
        TEST_WARN("Expected fence to be set (it was created signalled)");

      for(size_t i = 0; i < 1000; i++)
        status = vkGetEventStatus(device, ev);

      if(status != VK_EVENT_RESET)
        TEST_WARN("Expected event to be unset");

      if(KHR_timeline_semaphore)
      {
        uint64_t val = 0;
        for(size_t i = 0; i < 1000; i++)
          vkGetSemaphoreCounterValueKHR(device, sem, &val);

        if(val != 1234)
          TEST_WARN("Expected timeline semaphore value to be 1234");
      }

      // reference some resources through different descriptor types to ensure that they are
      // properly included
      {
        vkDeviceWaitIdle(device);

        refdatastruct refdata = GetData(0);
        refdatastruct reftempldata = GetData(1);
        refdatastruct refpushdata = GetData(2);
        refdatastruct refpushtempldata = GetData(3);

        vkh::updateDescriptorSets(
            device,
            {
                vkh::WriteDescriptorSet(refdescset, 0, VK_DESCRIPTOR_TYPE_SAMPLER, {refdata.sampler}),
                vkh::WriteDescriptorSet(refdescset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        {refdata.combined}),
                vkh::WriteDescriptorSet(refdescset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                        {refdata.sampled}),
                vkh::WriteDescriptorSet(refdescset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                        {refdata.storage}),
                vkh::WriteDescriptorSet(refdescset, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                        {refdata.unitexel}),
                vkh::WriteDescriptorSet(refdescset, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                        {refdata.storetexel}),
                vkh::WriteDescriptorSet(refdescset, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        {refdata.unibuf}),
                vkh::WriteDescriptorSet(refdescset, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        {refdata.storebuf}),
                vkh::WriteDescriptorSet(refdescset, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                        {refdata.unibufdyn}),
                vkh::WriteDescriptorSet(refdescset, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                        {refdata.storebufdyn}),
            });

        if(KHR_descriptor_update_template)
          vkUpdateDescriptorSetWithTemplateKHR(device, reftempldescset, reftempl, &reftempldata);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        if(KHR_descriptor_update_template)
          setMarker(cmd, "KHR_descriptor_update_template");
        if(KHR_push_descriptor)
          setMarker(cmd, "KHR_push_descriptor");

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, refpipe);

        uint32_t set = 0;

        // set 0 is always the normal one
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, reflayout, set++,
                                   {refdescset}, {0, 0});

        // if we have update templates, set 1 is always the template one
        if(KHR_descriptor_update_template)
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, reflayout, set++,
                                     {reftempldescset}, {0, 0});

        // push set comes after the ones above. Note that because we can't have more than one push
        // set, we test with the first set of refs here then do a template update (if supported) and
        // draw again to test the second set of refs.
        if(KHR_push_descriptor)
        {
          vkh::cmdPushDescriptorSets(
              cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, reflayout, set,
              {
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 0, VK_DESCRIPTOR_TYPE_SAMPLER,
                                          {refpushdata.sampler}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                          {refpushdata.combined}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          {refpushdata.sampled}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                          {refpushdata.storage}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                          {refpushdata.unitexel}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                          {refpushdata.storetexel}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          {refpushdata.unibuf}),
                  vkh::WriteDescriptorSet(VK_NULL_HANDLE, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          {refpushdata.storebuf}),
              });
        }

        VkViewport view = {128, 0, 128, 128, 0, 1};
        vkCmdSetViewport(cmd, 0, 1, &view);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        setMarker(cmd, "References");
        vkCmdDraw(cmd, 1, 1, 0, 0);

        if(KHR_descriptor_update_template && KHR_push_descriptor)
        {
          setMarker(cmd, "PushTemplReferences");
          vkCmdPushDescriptorSetWithTemplateKHR(cmd, refpushtempl, reflayout, set, &refpushtempldata);
          vkCmdDraw(cmd, 1, 1, 0, 0);
        }

        vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        Submit(1, 4, {cmd});

        vkDeviceWaitIdle(device);

        // scribble over the descriptor contents so that initial contents fetch never gets these
        // resources that way
        vkh::updateDescriptorSets(
            device,
            {
                vkh::WriteDescriptorSet(refdescset, 0, VK_DESCRIPTOR_TYPE_SAMPLER,
                                        {resetrefdata.sampler}),
                vkh::WriteDescriptorSet(refdescset, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        {resetrefdata.combined}),
                vkh::WriteDescriptorSet(refdescset, 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                        {resetrefdata.sampled}),
                vkh::WriteDescriptorSet(refdescset, 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                        {resetrefdata.storage}),
                vkh::WriteDescriptorSet(refdescset, 4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                        {resetrefdata.unitexel}),
                vkh::WriteDescriptorSet(refdescset, 5, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                        {resetrefdata.storetexel}),
                vkh::WriteDescriptorSet(refdescset, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        {resetrefdata.unibuf}),
                vkh::WriteDescriptorSet(refdescset, 7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                        {resetrefdata.storebuf}),
                vkh::WriteDescriptorSet(refdescset, 8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                        {resetrefdata.unibufdyn}),
                vkh::WriteDescriptorSet(refdescset, 9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                        {resetrefdata.storebufdyn}),
            });

        if(KHR_descriptor_update_template)
          vkUpdateDescriptorSetWithTemplateKHR(device, reftempldescset, reftempl, &resetrefdata);
      }

      // check the rendering with our parameter tests is OK
      {
        vkDeviceWaitIdle(device);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        VkImage swapimg =
            StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                             vkh::ImageSubresourceRange());

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        if(!tools.empty())
        {
          pushMarker(cmd, "Tools available");
          for(VkPhysicalDeviceToolPropertiesEXT &tool : tools)
            setMarker(cmd, tool.name);
          popMarker(cmd);
        }

        uint32_t idx = 1;
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, asm_pipe);
        vkCmdPushConstants(cmd, asm_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &idx);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, asm_layout, 0,
                                   {asm_descset}, {});
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, asm_layout, 1,
                                   {empty_descset}, {});

        setMarker(cmd, "ASM Draw");
        vkCmdDraw(cmd, 4, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, {descset},
                                   {0, 0});
        if(KHR_push_descriptor)
          vkCmdPushDescriptorSetKHR(
              cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
              vkh::WriteDescriptorSet(VK_NULL_HANDLE, 20, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      validBufInfos));
        if(KHR_descriptor_update_template && KHR_push_descriptor)
          vkCmdPushDescriptorSetWithTemplateKHR(cmd, pushtempl, layout, 1, &pushdata);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, immutpipe);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, immutlayout, 0,
                                   {immutdescset}, {});

        setMarker(cmd, "Immutable Draw");
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dynarraypipe);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dynamicarraylayout, 0,
                                   {dynamicarrayset}, {0, 0, 0, 0, 0, 0, 0, 256});

        setMarker(cmd, "Dynamic Array Draw");
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe2);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout2, 0, {descset2}, {});

        setMarker(cmd, "Color Draw");
        vkCmdDraw(cmd, 3, 1, 0, 0);

        VkRect2D sc = {100, 100, 10, 10};
        vkCmdSetScissor(cmd, 0, 1, &sc);

        if(EXT_transform_feedback)
        {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, xfbpipe);

          VkDeviceSize offs = 0;
          // pSizes is optional and can be NULL to use the whole buffer size
          vkCmdBindTransformFeedbackBuffersEXT(cmd, 0, 1, &xfbBuf.buffer, &offs, NULL);

          // pCounterBuffers is also optional
          vkCmdBeginTransformFeedbackEXT(cmd, 0, 0, NULL, NULL);
          vkCmdEndTransformFeedbackEXT(cmd, 0, 0, NULL, NULL);
        }

        vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        Submit(2, 4, {cmd});
      }

      // finish with the backbuffer
      {
        vkDeviceWaitIdle(device);

        VkCommandBuffer cmd = GetCommandBuffer();

        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

        FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        vkEndCommandBuffer(cmd);

        Submit(3, 4, {cmd});
      }

      // make some empty submits

      setMarker(queue, "before_empty");

      {
        std::vector<VkCommandBuffer> cmds = {};
        VkSubmitInfo submit = vkh::SubmitInfo(cmds);
        CHECK_VKR(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
      }
      if(hasExt(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
      {
        VkSubmitInfo2KHR submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR};
        CHECK_VKR(vkQueueSubmit2KHR(queue, 1, &submit, VK_NULL_HANDLE));
      }
      setMarker(queue, "after_empty");

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroyEvent(device, ev, NULL);
    vkDestroyFence(device, fence, NULL);
    vkDestroySemaphore(device, sem, NULL);

    vkDestroyImageView(device, view1, NULL);
    vkDestroyImageView(device, view3, NULL);

    if(KHR_descriptor_update_template && KHR_push_descriptor)
    {
      vkDestroyDescriptorUpdateTemplateKHR(device, pushtempl, NULL);
      vkDestroyDescriptorUpdateTemplateKHR(device, refpushtempl, NULL);
    }

    if(KHR_descriptor_update_template)
      vkDestroyDescriptorUpdateTemplateKHR(device, reftempl, NULL);

    return 0;
  }
};

REGISTER_TEST();
