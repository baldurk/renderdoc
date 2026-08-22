/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

std::string simple_mesh = R"EOSHADER(

#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(triangles, max_vertices = 6, max_primitives = 2) out;
layout(location = 0) out vec4 outColor[];

void main()
{
  uint triangleCount = 2;
  uint vertexCount = 3 * triangleCount;

  SetMeshOutputsEXT(vertexCount, triangleCount);

  for (uint i = 0; i < 2; ++i)
  {
    uint vertIdx = i * 3;
    uint tri = i + 2 * gl_WorkGroupID.x;
    vec4 org = vec4(-0.65, +0.65, 0.0, 0.0) + vec4(0.42, 0.0, 0.0, 0.0) * tri;

    uint vert0 = 0 + vertIdx;
    uint vert1 = 1 + vertIdx;
    uint vert2 = 2 + vertIdx;

    gl_MeshVerticesEXT[vert0].gl_Position = vec4(-0.2, -0.2, 0.0, 1.0) + org;
    gl_MeshVerticesEXT[vert1].gl_Position = vec4(0.0, 0.2, 0.0, 1.0) + org;
    gl_MeshVerticesEXT[vert2].gl_Position = vec4(0.2, -0.2, 0.0, 1.0) + org;

    outColor[vert0] = vec4(1.0, 0.0, 0.0, 1.0);
    outColor[vert1] = vec4(1.0, 0.0, 0.0, 1.0);
    outColor[vert2] = vec4(1.0, 0.0, 0.0, 1.0);

    gl_PrimitiveTriangleIndicesEXT[i] =  uvec3(vert0, vert1, vert2);
  }
}

)EOSHADER";

std::string simple_mesh_pixel = R"EOSHADER(

#version 460

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
  outColor = inColor;
}

)EOSHADER";

std::string pixel = R"EOSHADER(

#version 460 core

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

layout(location = 0) in v2f vertIn;
layout(location = 0, index = 0) out vec4 Color;
layout(binding = 2) uniform sampler2D inTex;

void main()
{
	Color = vertIn.col;
  Color += texture(inTex, vertIn.uv.xy);
}

)EOSHADER";

const std::string compute = R"EOSHADER(

#version 450 core

#extension GL_ARB_compute_shader : require

layout(binding = 0) uniform inbuftype {
  uvec4 data[];
} inbuf;

layout(binding = 1, std430) buffer outbuftype {
  uvec4 data[];
} outbuf;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
  outbuf.data[0] += inbuf.data[0];
}

)EOSHADER";

const std::string computeWriteData = R"EOSHADER(

#version 430 core

#extension GL_ARB_compute_shader : require

layout(push_constant) uniform PushConstants {
	uint mode;
} push;

layout(binding = 0, std140) buffer general_buffer
{
	uvec4 data[];
} indirectData;

layout (local_size_x = 2, local_size_y = 2, local_size_z = 1) in;

void main()
{
  if(push.mode == 0)
  {
    // see below, here we write the indirect dispatch parameters
    indirectData.data[0] = uvec4(3, 4, 5, 999999);
  }
  else if(push.mode == 1)
  {
    // see below, in the indirect dispatch we write data in for each thread
    uint idx = gl_GlobalInvocationID.z * (3 * 2) * (4 * 2) +
               gl_GlobalInvocationID.y * (3 * 2) +
               gl_GlobalInvocationID.x;

    indirectData.data[100+idx] = uvec4(gl_GlobalInvocationID, 12345);

    // we also write the draw parameters for non-indexed and indexed draws.
    // The indices point just after the vertices, so we have all unique draws

    // vkCmdDrawIndirect() (4 draws)
    indirectData.data[1] = uvec4(3, 1, 3, 0); // draw verts 3..5
    indirectData.data[2] = uvec4(3, 1, 6, 0); // draw verts 6..8
    indirectData.data[3] = uvec4(3, 1, 9, 0); // draw verts 9..11
    indirectData.data[4] = uvec4(3, 1, 12, 0); // draw verts 12..14

    // vkCmdDrawIndexedIndirect() (3 draws)
    indirectData.data[5] = uvec4(3, 1, 3, 0); // draw indices 3..5
    indirectData.data[6].x = 0;
    indirectData.data[7] = uvec4(3, 1, 6, 0); // draw indices 6..8
    indirectData.data[8].x = 0;
    indirectData.data[9] = uvec4(3, 1, 9, 0); // draw indices 9..11
    indirectData.data[10].x = 0;

    // Counts
    indirectData.data[10].x = 3;
    indirectData.data[10].y = 2;
    indirectData.data[10].z = 0;
    indirectData.data[10].w = 0;

    // DrawMeshIndirect
    indirectData.data[11].x = 9;
    indirectData.data[11].y = 7;
    indirectData.data[11].z = 5;
    indirectData.data[11].w = 0;

    indirectData.data[12].x = 2;
    indirectData.data[12].y = 1;
    indirectData.data[12].z = 1;
    indirectData.data[12].w = 0;

    // DrawMeshIndirect Counts
    indirectData.data[13].x = 1;
    indirectData.data[13].y = 0;
    indirectData.data[13].z = 0;
    indirectData.data[13].w = 0;
  }
}

)EOSHADER";

RD_TEST(VK_Resource_Usage, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Test resource usage in a variety of scenarios.";

  VkPhysicalDeviceNestedCommandBufferFeaturesEXT nestedFeats = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_FEATURES_EXT,
  };

  VkPhysicalDeviceNestedCommandBufferPropertiesEXT nestedProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_PROPERTIES_EXT,
  };

  VkPhysicalDeviceDescriptorBufferFeaturesEXT descBufFeats = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
  };

  VkPhysicalDeviceDescriptorBufferPropertiesEXT descBufProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
  };

  VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeats = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
  };

  float sqSize;
  VkViewport viewPort;

  void NextTest()
  {
    viewPort.x += sqSize;

    if(viewPort.x + sqSize >= (float)screenWidth)
    {
      viewPort.x = 0.0f;
      viewPort.y += sqSize;
    }
  }

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    features.multiDrawIndirect = VK_TRUE;

    optDevExts.push_back(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_NESTED_COMMAND_BUFFER_EXTENSION_NAME);
    optDevExts.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(devVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      static VkPhysicalDeviceVulkan12Features feats = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };

      VkPhysicalDeviceVulkan12Features vk12avail = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      };

      getPhysFeatures2(&vk12avail);

      if(vk12avail.drawIndirectCount)
        feats.drawIndirectCount = VK_TRUE;

      if(vk12avail.bufferDeviceAddress)
        feats.bufferDeviceAddress = VK_TRUE;

      feats.pNext = (void *)devInfoNext;
      devInfoNext = &feats;

      if(!vk12avail.bufferDeviceAddress)
        Avail = "feature 'bufferDeviceAddress' not available";
    }
    else
    {
      static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufaddrFeatures = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
      };

      getPhysFeatures2(&bufaddrFeatures);

      if(!bufaddrFeatures.bufferDeviceAddress)
        Avail = "feature 'bufferDeviceAddress' not available";

      bufaddrFeatures.pNext = (void *)devInfoNext;
      devInfoNext = &bufaddrFeatures;
    }

    if(hasExt(VK_EXT_NESTED_COMMAND_BUFFER_EXTENSION_NAME))
    {
      getPhysFeatures2(&nestedFeats);

      if((nestedFeats.nestedCommandBuffer) && (nestedFeats.nestedCommandBufferRendering))
      {
        nestedFeats.pNext = (void *)devInfoNext;
        devInfoNext = &nestedFeats;
      }
    }

    if(hasExt(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME))
    {
      getPhysFeatures2(&descBufFeats);
      if(descBufFeats.descriptorBuffer && descBufFeats.descriptorBufferCaptureReplay)
      {
        descBufFeats.pNext = (void *)devInfoNext;
        devInfoNext = &descBufFeats;
      }
    }

    if(hasExt(VK_EXT_MESH_SHADER_EXTENSION_NAME))
    {
      getPhysFeatures2(&meshShaderFeats);
      if(meshShaderFeats.meshShader)
      {
        meshShaderFeats.multiviewMeshShader = VK_FALSE;
        meshShaderFeats.primitiveFragmentShadingRateMeshShader = VK_FALSE;
        meshShaderFeats.pNext = (void *)devInfoNext;
        devInfoNext = &meshShaderFeats;
      }
    }
  }

  size_t DescSize(VkDescriptorType type)
  {
    if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
      return descBufProps.uniformBufferDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
      return descBufProps.storageBufferDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
      return descBufProps.storageImageDescriptorSize;
    else if(type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
      return descBufProps.combinedImageSamplerDescriptorSize;

    return 0;
  }

  void FillDescriptor(VkDescriptorSetLayout layout, uint32_t bind, VkDescriptorType type,
                      VkDeviceSize range, VkDeviceAddress dataAddress, byte *const descMem)
  {
    VkDescriptorAddressInfoEXT buf = {VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};
    buf.address = dataAddress;
    buf.range = range;
    buf.format = VK_FORMAT_UNDEFINED;

    VkDescriptorGetInfoEXT get = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    get.type = type;
    get.data.pStorageBuffer = &buf;

    VkDeviceSize bindOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, bind, &bindOffset);
    void *dst = descMem + bindOffset;

    vkGetDescriptorEXT(device, &get, DescSize(type), dst);
  }

  void FillDescriptor(VkDescriptorSetLayout layout, uint32_t bind, VkDescriptorType type,
                      VkSampler sampler, VkImageView view, byte *const descMem)
  {
    VkDescriptorImageInfo im;
    im.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    im.imageView = view;
    im.sampler = sampler;

    VkDescriptorGetInfoEXT get = {VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
    get.type = type;
    get.data.pCombinedImageSampler = &im;

    VkDeviceSize bindOffset;
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, layout, bind, &bindOffset);
    void *dst = descMem + bindOffset;

    vkGetDescriptorEXT(device, &get, DescSize(type), dst);
  }

  void BufferUpload(AllocatedBuffer & buffer, void *data, size_t countBytes, size_t offset = 0)
  {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(buffer.allocator, buffer.alloc, &alloc_info);
    uint16_t *dst = NULL;
    vkMapMemory(device, alloc_info.deviceMemory, alloc_info.offset, VK_WHOLE_SIZE, 0, (void **)&dst);
    memcpy(dst + offset, data, countBytes);
    vkUnmapMemory(device, alloc_info.deviceMemory);
  }

  void BufferFill(AllocatedBuffer & buffer, byte data, size_t countBytes)
  {
    VmaAllocationInfo alloc_info;
    vmaGetAllocationInfo(buffer.allocator, buffer.alloc, &alloc_info);
    uint16_t *dst = NULL;
    vkMapMemory(device, alloc_info.deviceMemory, alloc_info.offset, VK_WHOLE_SIZE, 0, (void **)&dst);
    memset(dst, data, countBytes);
    vkUnmapMemory(device, alloc_info.deviceMemory);
  }

  int main()
  {
    vmaBDA = true;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool nestedSecondaries = hasExt(VK_EXT_NESTED_COMMAND_BUFFER_EXTENSION_NAME);
    if(nestedSecondaries)
    {
      getPhysFeatures2(&nestedFeats);
      if(!nestedFeats.nestedCommandBuffer)
        nestedSecondaries = false;
      if(!nestedFeats.nestedCommandBufferRendering)
        nestedSecondaries = false;
      getPhysProperties2(&nestedProps);
      if(nestedProps.maxCommandBufferNestingLevel < 5)
        nestedSecondaries = false;
    }

    bool descBuffer = hasExt(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    if(descBuffer)
    {
      getPhysFeatures2(&descBufFeats);
      if(!descBufFeats.descriptorBuffer)
        descBuffer = false;
      if(!descBufFeats.descriptorBufferCaptureReplay)
        descBuffer = false;
      getPhysProperties2(&descBufProps);
    }

    bool meshShader = hasExt(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    if(meshShader)
    {
      getPhysFeatures2(&meshShaderFeats);
      if(!meshShaderFeats.meshShader)
        meshShader = false;
    }

    bool draw_indirect_count = false;
    if(devVersion >= VK_MAKE_VERSION(1, 2, 0))
    {
      draw_indirect_count = true;
    }
    else
    {
      draw_indirect_count = hasExt(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
    }

    if(draw_indirect_count)
      TEST_LOG("Running tests with draw indirect count");
    if(nestedSecondaries)
      TEST_LOG("Running tests with nested secondaries");
    if(descBuffer)
      TEST_LOG("Running tests with descriptor buffer");
    if(meshShader)
      TEST_LOG("Running tests with mesh shaders");

    setName(queue, "Main Queue");

    vkh::RenderPassCreator renderPassCreateInfo;
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_SAMPLE_COUNT_1_BIT));
    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})},
                                    VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED);
    VkRenderPass renderPass = mainWindow->rp;
    setName(renderPass, "Main Render Pass");

    VkPipelineLayout noDescSetPipeLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());
    setName(noDescSetPipeLayout, "No Descriptor Set Pipeline Layout");

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = noDescSetPipeLayout;
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
    setName(pipeCreateInfo.stages[0].module, "Default Vertex Shader");
    setName(pipeCreateInfo.stages[1].module, "Default Pixel Shader");

    VkPipeline noDescSetPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(noDescSetPipe, "No Descriptor Set Pipeline");

    VkDescriptorSetLayout descSetLayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        }));
    setName(descSetLayout, "Descriptor Set Layout");

    VkPipelineLayout descSetPipeLayout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo({descSetLayout}));
    setName(descSetPipeLayout, "Descriptor Set Pipeline Layout");

    pipeCreateInfo.layout = descSetPipeLayout;
    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };
    setName(pipeCreateInfo.stages[0].module, "Descriptor Vertex Shader");
    setName(pipeCreateInfo.stages[1].module, "Descriptor Pixel Shader");

    VkPipeline descSetPipe = createGraphicsPipeline(pipeCreateInfo);
    setName(descSetPipe, "Descriptor Set Pipeline");

    VkDescriptorSetLayout descBuffLayout = VK_NULL_HANDLE;
    VkPipelineLayout descBuffPipeLayout = VK_NULL_HANDLE;
    VkPipeline descBuffPipe = VK_NULL_HANDLE;
    if(descBuffer)
    {
      descBuffLayout = createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(
          {
              {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
              {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
              {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
          },
          VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));
      setName(descBuffLayout, "Descriptor Buffer Layout");

      descBuffPipeLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({descBuffLayout}));
      setName(descBuffPipeLayout, "Descriptor Buffer Pipeline Layout");

      pipeCreateInfo.layout = descBuffPipeLayout;
      pipeCreateInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
      descBuffPipe = createGraphicsPipeline(pipeCreateInfo);
      setName(descBuffPipe, "Descriptor Buffer Pipeline");
    }

    VkPipelineLayout meshShaderLayout = createPipelineLayout(
        vkh::PipelineLayoutCreateInfo({}, {vkh::PushConstantRange(VK_SHADER_STAGE_ALL, 0, 8)}));

    VkPipeline meshShaderPipe = VK_NULL_HANDLE;
    if(meshShader)
    {
      vkh::GraphicsPipelineCreateInfo meshShaderPipeCreateInfo;

      meshShaderPipeCreateInfo.layout = meshShaderLayout;
      meshShaderPipeCreateInfo.renderPass = renderPass;
      meshShaderPipeCreateInfo.stages = {
          CompileShaderModule(simple_mesh, ShaderLang::glsl, ShaderStage::mesh, "main", {},
                              SPIRVTarget::vulkan12),
          CompileShaderModule(simple_mesh_pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
      };
      setName(meshShaderPipeCreateInfo.stages[0].module, "Mesh Mesh Shader");
      setName(meshShaderPipeCreateInfo.stages[1].module, "Mesh Pixel Shader");

      VkGraphicsPipelineCreateInfo *vkMeshShaderPipeCreateInfo = meshShaderPipeCreateInfo;
      vkMeshShaderPipeCreateInfo->pVertexInputState = NULL;
      vkMeshShaderPipeCreateInfo->pInputAssemblyState = NULL;

      meshShaderPipe = createGraphicsPipeline(vkMeshShaderPipeCreateInfo);
    }

    VkDescriptorSetLayout compDescSetLayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        }));
    setName(compDescSetLayout, "Compute Descriptor Set Layout");

    VkPipelineLayout compDescSetPipeLayout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo({compDescSetLayout}));
    setName(compDescSetPipeLayout, "Compute Pipeline Layout");

    vkh::ComputePipelineCreateInfo compDescSetPipeCreateInfo(
        compDescSetPipeLayout,
        CompileShaderModule(compute, ShaderLang::glsl, ShaderStage::comp, "main"));
    VkPipeline compDescSetPipe = createComputePipeline(compDescSetPipeCreateInfo);
    setName(compDescSetPipe, "Compute Descriptor Set Pipeline");
    setName(compDescSetPipeCreateInfo.stage.module, "Descriptor Set Compute Shader");

    VkDescriptorSetLayout compWriteDataSetLayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo({
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        }));
    setName(compWriteDataSetLayout, "Compute WriteData Descriptor Set Layout");

    VkPipelineLayout compWriteDataPipeLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {compWriteDataSetLayout}, {vkh::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 4)}));
    setName(compWriteDataPipeLayout, "Compute WriteData Pipeline Layout");

    vkh::ComputePipelineCreateInfo compWriteDataPipeCreateInfo(
        compWriteDataPipeLayout,
        CompileShaderModule(computeWriteData, ShaderLang::glsl, ShaderStage::comp, "main"));
    VkPipeline compWriteDataPipe = createComputePipeline(compWriteDataPipeCreateInfo);
    setName(compWriteDataPipe, "Compute WriteData Pipeline");
    setName(compWriteDataPipeCreateInfo.stage.module, "WriteData Compute Shader");

    VkPipelineLayout compDescBuffPipeLayout = VK_NULL_HANDLE;
    VkPipeline compDescBuffPipe = VK_NULL_HANDLE;
    if(descBuffer)
    {
      compDescBuffPipeLayout = createPipelineLayout(vkh::PipelineLayoutCreateInfo({descBuffLayout}));
      setName(compDescSetPipeLayout, "Compute Descriptor Buffer Pipeline Layout");

      vkh::ComputePipelineCreateInfo compDescBuffPipeCreateInfo(
          compDescBuffPipeLayout,
          CompileShaderModule(compute, ShaderLang::glsl, ShaderStage::comp, "main"),
          VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
      compDescBuffPipe = createComputePipeline(compDescBuffPipeCreateInfo);
      setName(compDescBuffPipe, "Compute Descriptor Buffer Pipeline");
      setName(compDescBuffPipeCreateInfo.stage.module, "Descriptor Buffer Compute Shader");
    }

    const DefaultA2V vbData[15] = {
        // Direct Draw Triangle
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // InDirect Draw Triangle draw 1
        // InDirect Draw Indexed draw 1
        {Vec3f(-0.25, -0.25, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.25f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.25f, -0.25f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // InDirect Draw Triangle draw 2
        // InDirect Draw Indexed draw 2
        {Vec3f(0.25, -0.25f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, 0.25f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.75, -0.25f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // InDirect Draw Triangle draw 3
        // InDirect Draw Indexed draw 3
        {Vec3f(-0.25, 0.25, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.75f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.25f, 0.25f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // InDirect Draw Triangle draw 4
        {Vec3f(0.25, 0.25f, 0.0f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, 0.75f, 0.0f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.75, 0.25f, 0.0f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };
    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(vbData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    setName(vb.buffer, "Vertex Buffer");
    BufferUpload(vb, (void *)vbData, sizeof(vbData));

    uint32_t indices[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    AllocatedBuffer ib(this,
                       vkh::BufferCreateInfo(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    setName(ib.buffer, "Index Buffer");
    BufferUpload(ib, (void *)indices, sizeof(indices));

    VkDescriptorSet descSet = allocateDescriptorSet(descSetLayout);
    setName(descSet, "Descriptor Set");

    VkDescriptorSet compDescSet = allocateDescriptorSet(compDescSetLayout);
    setName(compDescSet, "Compute Descriptor Set");

    VkDescriptorSet compWriteDataDescSet = allocateDescriptorSet(compWriteDataSetLayout);
    setName(compWriteDataDescSet, "Compute WriteData Descriptor Set");

    AllocatedImage offimg(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo(
            {VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(offimg.image, "Offscreen Image");
    VkImageView offimgRTV = createImageView(vkh::ImageViewCreateInfo(
        offimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));
    setName(offimgRTV, "Offscreen Image RTV");

    AllocatedImage offimgMS(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R16G16B16A16_SFLOAT,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT, 1, 1, VK_SAMPLE_COUNT_4_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(offimgMS.image, "Offscreen MSAA Image");

    VkSampler linearSampler = createSampler(vkh::SamplerCreateInfo(VK_FILTER_LINEAR));
    setName(linearSampler, "Linear Sampler");

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(descSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            {vkh::DescriptorImageInfo(
                                                offimgRTV, VK_IMAGE_LAYOUT_GENERAL, linearSampler)}),
                });

    AllocatedBuffer compBufIn(
        this,
        vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    setName(compBufIn.buffer, "Compute Buffer In");
    BufferFill(compBufIn, 0xDE, 1024);

    AllocatedBuffer compBufOut(
        this,
        vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    setName(compBufOut.buffer, "Compute Buffer Out");

    VkDeviceSize indirectDataSize = 16 * 1024;

    AllocatedBuffer indirectData(
        this,
        vkh::BufferCreateInfo(indirectDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    setName(indirectData.buffer, "Indirect Data");
    BufferUpload(indirectData, (void *)indices, sizeof(indices), 1024);

    AllocatedBuffer barrierBuffer(this,
                                  vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(barrierBuffer.buffer, "Barrier Buffer");

    AllocatedBuffer barrier2Buffer(this,
                                   vkh::BufferCreateInfo(1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                   VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    setName(barrier2Buffer.buffer, "Barrier2 Buffer");

    vkh::updateDescriptorSets(
        device,
        {
            vkh::WriteDescriptorSet(compWriteDataDescSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(indirectData.buffer)}),
            vkh::WriteDescriptorSet(compWriteDataDescSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(barrierBuffer.buffer)}),
            vkh::WriteDescriptorSet(compWriteDataDescSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    {vkh::DescriptorBufferInfo(barrier2Buffer.buffer)}),
        });

    VkDescriptorBufferBindingInfoEXT descBuffBind = {};
    AllocatedBuffer descBuf;
    AllocatedBuffer descBackupBuf;
    if(descBuffer)
    {
      descBuf = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(0x1000, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
                                            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                                            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
      setName(descBuf.buffer, "Descriptor Buffer");
      descBackupBuf =
          AllocatedBuffer(this,
                          vkh::BufferCreateInfo(0x1000, VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                          VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      setName(descBackupBuf.buffer, "Descriptor Backup Buffer");
      VmaAllocationInfo alloc_info;
      vmaGetAllocationInfo(descBuf.allocator, descBuf.alloc, &alloc_info);
      byte *descBufMem = NULL;
      vkMapMemory(device, alloc_info.deviceMemory, alloc_info.offset, VK_WHOLE_SIZE, 0,
                  (void **)&descBufMem);
      memset(descBufMem, 0xCC, 0x1000);

      VkDeviceAddress compBufInBDA = compBufIn.address;
      FillDescriptor(descBuffLayout, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024, compBufInBDA,
                     descBufMem);
      VkDeviceAddress compBufOutBDA = compBufOut.address;
      FillDescriptor(descBuffLayout, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024, compBufOutBDA,
                     descBufMem);
      FillDescriptor(descBuffLayout, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, linearSampler,
                     offimgRTV, descBufMem);
      vkUnmapMemory(device, alloc_info.deviceMemory);

      descBuffBind = {
          VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
          NULL,
          descBuf.address,
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR |
              VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
              VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
      };
    }

    vkh::updateDescriptorSets(
        device, {
                    vkh::WriteDescriptorSet(compDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                            {vkh::DescriptorBufferInfo(compBufIn.buffer)}),
                    vkh::WriteDescriptorSet(compDescSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            {vkh::DescriptorBufferInfo(compBufOut.buffer)}),
                });

    sqSize = float(screenHeight) / 6.0f;

    using uvec4 = uint32_t[4];

    VkCommandPool barrierCmdPool;
    CHECK_VKR(vkCreateCommandPool(
        device,
        vkh::CommandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex),
        NULL, &barrierCmdPool));
    setName(barrierCmdPool, "BarrierCommand Pool");

    VkCommandBuffer barrierCmd = VK_NULL_HANDLE;
    CHECK_VKR(vkAllocateCommandBuffers(device, vkh::CommandBufferAllocateInfo(barrierCmdPool, 1),
                                       &barrierCmd));
    setName(barrierCmd, "Barrier Command Buffer");
    vkBeginCommandBuffer(barrierCmd,
                         vkh::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT));
    setMarker(barrierCmd, "Multiple Command Buffer Submits");
    vkh::cmdPipelineBarrier(
        barrierCmd, {},
        {vkh::BufferMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_NONE, barrierBuffer.buffer)});
    vkEndCommandBuffer(barrierCmd);

    VkFence barrierCmdSubmitFence;
    CHECK_VKR(vkCreateFence(device, vkh::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT), NULL,
                            &barrierCmdSubmitFence));
    setName(barrierCmdSubmitFence, "Barrier Command Submit Fence");

    while(Running())
    {
      viewPort = {0.0f, 0.0f, sqSize, sqSize, 0.0f, 1.0f};
      setName(mainWindow->GetFB(), "Main Framebuffer");

      VkCommandBuffer barrierSecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
      vkBeginCommandBuffer(barrierSecCmd, vkh::CommandBufferBeginInfo(
                                              VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                              vkh::CommandBufferInheritanceInfo(VK_NULL_HANDLE, 0)));
      vkh::cmdPipelineBarrier(
          barrierSecCmd, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_NONE, barrier2Buffer.buffer)});
      vkEndCommandBuffer(barrierSecCmd);

      VkCommandBuffer secCmdBuffers[3];
      for(size_t i = 0; i < 2; i++)
      {
        VkCommandBuffer secCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        vkBeginCommandBuffer(
            secCmd, vkh::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
                                                    VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                                vkh::CommandBufferInheritanceInfo(renderPass, 0)));

        pushMarker(secCmd, "No Descriptor Set");
        {
          vkCmdSetScissor(secCmd, 0, 1, &mainWindow->scissor);
          vkh::cmdBindVertexBuffers(secCmd, 0, {vb.buffer}, {0});
          vkCmdBindIndexBuffer(secCmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
          vkCmdBindPipeline(secCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, noDescSetPipe);

          // Vertex draw
          setMarker(secCmd, "Vertex Draw");
          vkCmdSetViewport(secCmd, 0, 1, &viewPort);
          vkCmdDraw(secCmd, 3, 1, 0, 0);
          NextTest();
          // Indexed draw
          setMarker(secCmd, "Indexed Draw");
          vkCmdSetViewport(secCmd, 0, 1, &viewPort);
          vkCmdDrawIndexed(secCmd, 3, 1, 0, 0, 0);
          NextTest();
        }
        popMarker(secCmd);

        vkEndCommandBuffer(secCmd);
        secCmdBuffers[i] = secCmd;
      }
      {
        VkCommandBuffer emptySecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        vkBeginCommandBuffer(emptySecCmd, vkh::CommandBufferBeginInfo(
                                              VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
                                                  VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                              vkh::CommandBufferInheritanceInfo(renderPass, 0)));
        vkEndCommandBuffer(emptySecCmd);
        secCmdBuffers[2] = emptySecCmd;
      }

      VkCommandBuffer nestedCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
      if(nestedSecondaries)
      {
        VkCommandBuffer nestedSecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        vkBeginCommandBuffer(nestedSecCmd, vkh::CommandBufferBeginInfo(
                                               VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
                                                   VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                               vkh::CommandBufferInheritanceInfo(renderPass, 0)));
        vkCmdSetScissor(nestedSecCmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(nestedSecCmd, 0, {vb.buffer}, {0});
        vkCmdBindIndexBuffer(nestedSecCmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(nestedSecCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, noDescSetPipe);

        // Vertex draw
        setMarker(nestedSecCmd, "Vertex Draw");
        vkCmdSetViewport(nestedSecCmd, 0, 1, &viewPort);
        vkCmdDraw(nestedSecCmd, 3, 1, 0, 0);
        NextTest();
        // Indexed draw
        setMarker(nestedSecCmd, "Indexed Draw");
        vkCmdSetViewport(nestedSecCmd, 0, 1, &viewPort);
        vkCmdDrawIndexed(nestedSecCmd, 3, 1, 0, 0, 0);
        NextTest();
        vkEndCommandBuffer(nestedSecCmd);

        vkBeginCommandBuffer(nestedCmd, vkh::CommandBufferBeginInfo(
                                            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                                            vkh::CommandBufferInheritanceInfo(renderPass, 0)));

        vkCmdExecuteCommands(nestedCmd, 1, &nestedSecCmd);
        vkEndCommandBuffer(nestedCmd);
      }

      VkCommandBuffer compSecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
      vkBeginCommandBuffer(compSecCmd, vkh::CommandBufferBeginInfo(
                                           VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                           vkh::CommandBufferInheritanceInfo(VK_NULL_HANDLE, 0)));
      vkh::cmdBindDescriptorSets(compSecCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compDescSetPipeLayout,
                                 0, {compDescSet}, {});
      vkCmdBindPipeline(compSecCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compDescSetPipe);
      vkCmdDispatch(compSecCmd, 1, 1, 1);
      vkh::cmdPipelineBarrier(
          compSecCmd, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                    indirectData.buffer)});
      vkEndCommandBuffer(compSecCmd);

      VkCommandBuffer compNestedSecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
      if(nestedSecondaries)
      {
        vkBeginCommandBuffer(
            compNestedSecCmd,
            vkh::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                        vkh::CommandBufferInheritanceInfo(VK_NULL_HANDLE, 0)));
        vkCmdExecuteCommands(compNestedSecCmd, 1, &compSecCmd);
        vkEndCommandBuffer(compNestedSecCmd);
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);
      setName(swapimg, "Main Swapchain Image");
      VkImageView view = mainWindow->GetView();
      setName(view, "Main Swapchain ImageView");

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, swapimg),
               });

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_GENERAL, offimg.image),
               });

      vkCmdClearColorImage(cmd, offimg.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_GENERAL, offimgMS.image),
               });

      vkCmdClearColorImage(cmd, offimgMS.image, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      pushMarker(cmd, "Indirect Write IndirectDispatch Data");
      {
        vkh::cmdPipelineBarrier(
            cmd, {},
            {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                      indirectData.buffer)});
        vkCmdFillBuffer(cmd, indirectData.buffer, 0, indirectDataSize, 0);
        vkh::cmdPipelineBarrier(
            cmd, {},
            {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                      indirectData.buffer)});

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compWriteDataPipe);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compWriteDataPipeLayout, 0,
                                   {compWriteDataDescSet}, {});

        uint32_t mode = 0;
        vkCmdPushConstants(cmd, compWriteDataPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);
        // Fill in draw and indirect dispatch parameters
        vkCmdDispatch(cmd, 1, 1, 1);

        vkh::cmdPipelineBarrier(cmd, {},
                                {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                                          VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
                                                              VK_ACCESS_SHADER_WRITE_BIT,
                                                          indirectData.buffer)});
      }
      popMarker(cmd);

      // Graphics
      pushMarker(cmd, "Graphics");
      {
        vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);

        // No Descriptor Set Usage
        pushMarker(cmd, "No Descriptor Set");
        {
          vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
          vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
          vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, noDescSetPipe);

          // Vertex draw
          setMarker(cmd, "Vertex Draw");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDraw(cmd, 3, 1, 0, 0);
          NextTest();
          // Indexed draw
          setMarker(cmd, "Indexed Draw");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);
          NextTest();
        }
        popMarker(cmd);

        // Descriptor Set Usage
        pushMarker(cmd, "Descriptor Set");
        {
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipeLayout, 0,
                                     {descSet}, {});
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipe);

          // Vertex draw
          setMarker(cmd, "Vertex Draw");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDraw(cmd, 3, 1, 0, 0);
          NextTest();
          // Indexed draw
          setMarker(cmd, "Indexed Draw");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);
          NextTest();
        }
        popMarker(cmd);
        vkCmdEndRenderPass(cmd);

        // Secondary Command Buffer
        pushMarker(cmd, "Secondary Command Buffer");
        {
          vkCmdBeginRenderPass(cmd, mainWindow->beginRP(),
                               VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
          vkCmdExecuteCommands(cmd, 3, secCmdBuffers);
          vkCmdEndRenderPass(cmd);
        }
        popMarker(cmd);
      }
      popMarker(cmd);

      // Compute
      pushMarker(cmd, "Compute");
      {
        // Descriptor Set Usage
        pushMarker(cmd, "Descriptor Set");
        {
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compDescSetPipeLayout, 0,
                                     {compDescSet}, {});
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compDescSetPipe);
          vkCmdDispatch(cmd, 1, 1, 1);
        }
        popMarker(cmd);

        // Secondary Command Buffer
        pushMarker(cmd, "Secondary Command Buffer");
        {
          vkCmdExecuteCommands(cmd, 1, &compSecCmd);
        }
        popMarker(cmd);
      }
      popMarker(cmd);

      pushMarker(cmd, "Indirect");
      {
        pushMarker(cmd, "Indirect Dispatch Write IndirectDraw Data");
        {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compWriteDataPipe);
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compWriteDataPipeLayout,
                                     0, {compWriteDataDescSet}, {});

          uint32_t mode = 1;
          vkCmdPushConstants(cmd, compWriteDataPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);
          setMarker(cmd, "DispatchIndirect");
          vkCmdDispatchIndirect(cmd, indirectData.buffer, 0);

          vkh::cmdPipelineBarrier(
              cmd, {},
              {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_ACCESS_INDIRECT_COMMAND_READ_BIT, indirectData.buffer)});
        }
        popMarker(cmd);

        pushMarker(cmd, "Indirect Draws");
        {
          size_t offset = sizeof(uvec4);
          uint32_t countDraws = 4;
          uint32_t strideDraw = sizeof(uvec4);

          vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);

          vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
          vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipeLayout, 0,
                                     {descSet}, {});
          vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
          vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipe);

          setMarker(cmd, "DrawIndirect: Zero");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndirect(cmd, indirectData.buffer, offset, 0, strideDraw);
          NextTest();

          setMarker(cmd, "DrawIndirect: Single");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndirect(cmd, indirectData.buffer, offset, 1, strideDraw);
          NextTest();

          setMarker(cmd, "DrawIndirect: Multiple");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndirect(cmd, indirectData.buffer, offset, countDraws, strideDraw);
          NextTest();
          offset += countDraws * strideDraw;

          uint32_t countDrawIndexed = 3;
          uint32_t strideDrawIndexed = 2 * sizeof(uvec4);
          setMarker(cmd, "DrawIndexedIndirect");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndexedIndirect(cmd, indirectData.buffer, offset, countDrawIndexed,
                                   strideDrawIndexed);
          NextTest();

          setMarker(cmd, "DrawIndexedIndirect : Zero");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndexedIndirect(cmd, indirectData.buffer, offset, 0, strideDrawIndexed);
          NextTest();

          vkCmdEndRenderPass(cmd);
          vkh::cmdPipelineBarrier(
              cmd, {},
              {vkh::BufferMemoryBarrier(VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                        VK_ACCESS_INDIRECT_COMMAND_READ_BIT, indirectData.buffer)});
        }
        popMarker(cmd);

        // Secondary Command Buffer
        pushMarker(cmd, "Secondary Command Buffer");
        {
          VkCommandBuffer indirectCompSecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
          {
            vkBeginCommandBuffer(indirectCompSecCmd,
                                 vkh::CommandBufferBeginInfo(
                                     0, vkh::CommandBufferInheritanceInfo(VK_NULL_HANDLE, 0)));
            vkh::cmdPipelineBarrier(
                indirectCompSecCmd, {},
                {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_ACCESS_TRANSFER_WRITE_BIT, indirectData.buffer)});
            vkCmdFillBuffer(indirectCompSecCmd, indirectData.buffer, 0, indirectDataSize, 0);
            vkh::cmdPipelineBarrier(
                indirectCompSecCmd, {},
                {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_ACCESS_TRANSFER_WRITE_BIT, indirectData.buffer)});

            vkCmdBindPipeline(indirectCompSecCmd, VK_PIPELINE_BIND_POINT_COMPUTE, compWriteDataPipe);
            vkh::cmdBindDescriptorSets(indirectCompSecCmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                       compWriteDataPipeLayout, 0, {compWriteDataDescSet}, {});

            uint32_t mode = 0;
            vkCmdPushConstants(indirectCompSecCmd, compWriteDataPipeLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);
            // Fill in draw and indirect dispatch parameters
            vkCmdDispatch(indirectCompSecCmd, 1, 1, 1);

            mode = 1;
            vkCmdPushConstants(indirectCompSecCmd, compWriteDataPipeLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &mode);

            vkh::cmdPipelineBarrier(
                indirectCompSecCmd, {},
                {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_ACCESS_TRANSFER_WRITE_BIT, indirectData.buffer)});

            setMarker(indirectCompSecCmd, "DispatchIndirect");
            vkCmdDispatchIndirect(indirectCompSecCmd, indirectData.buffer, 0);
            vkCmdDispatchIndirect(indirectCompSecCmd, indirectData.buffer, 0);

            vkh::cmdPipelineBarrier(
                indirectCompSecCmd, {},
                {vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                          VK_ACCESS_TRANSFER_WRITE_BIT, indirectData.buffer)});

            vkCmdDispatchIndirect(indirectCompSecCmd, indirectData.buffer, 0);

            vkh::cmdPipelineBarrier(indirectCompSecCmd, {},
                                    {vkh::BufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                                              VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                                              indirectData.buffer)});

            vkEndCommandBuffer(indirectCompSecCmd);
          }

          vkCmdExecuteCommands(cmd, 1, &indirectCompSecCmd);

          vkCmdBeginRenderPass(cmd, mainWindow->beginRP(),
                               VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

          VkCommandBuffer indirectDrawSecCmd = GetCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);
          {
            vkBeginCommandBuffer(
                indirectDrawSecCmd,
                vkh::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                                            vkh::CommandBufferInheritanceInfo(mainWindow->rp, 0)));

            size_t offset = sizeof(uvec4);
            uint32_t countDraws = 4;
            uint32_t strideDraw = sizeof(uvec4);

            vkCmdSetScissor(indirectDrawSecCmd, 0, 1, &mainWindow->scissor);
            vkh::cmdBindDescriptorSets(indirectDrawSecCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       descSetPipeLayout, 0, {descSet}, {});
            vkh::cmdBindVertexBuffers(indirectDrawSecCmd, 0, {vb.buffer}, {0});
            vkCmdBindIndexBuffer(indirectDrawSecCmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindPipeline(indirectDrawSecCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipe);

            setMarker(indirectDrawSecCmd, "DrawIndirect: Single");

            vkCmdSetViewport(indirectDrawSecCmd, 0, 1, &viewPort);
            vkCmdDrawIndirect(indirectDrawSecCmd, indirectData.buffer, offset, 1, strideDraw);
            NextTest();

            setMarker(indirectDrawSecCmd, "DrawIndirect: Multiple");
            vkCmdSetViewport(indirectDrawSecCmd, 0, 1, &viewPort);
            vkCmdDrawIndirect(indirectDrawSecCmd, indirectData.buffer, offset, countDraws,
                              strideDraw);
            NextTest();

            offset += countDraws * strideDraw;

            uint32_t countDrawIndexed = 3;
            uint32_t strideDrawIndexed = 2 * sizeof(uvec4);
            setMarker(indirectDrawSecCmd, "DrawIndexedIndirect");
            vkCmdSetViewport(indirectDrawSecCmd, 0, 1, &viewPort);
            vkCmdDrawIndexedIndirect(indirectDrawSecCmd, indirectData.buffer, offset,
                                     countDrawIndexed, strideDrawIndexed);
            NextTest();
            vkEndCommandBuffer(indirectDrawSecCmd);
          }
          vkCmdExecuteCommands(cmd, 1, &indirectDrawSecCmd);

          vkCmdEndRenderPass(cmd);
        }
        popMarker(cmd);
      }
      popMarker(cmd);

      pushMarker(cmd, "Loose Events After Indirect Draws");
      {
        size_t offset = sizeof(uvec4);
        uint32_t countDraw = 4;
        uint32_t strideDraw = sizeof(uvec4);

        vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipeLayout, 0,
                                   {descSet}, {});
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descSetPipe);

        setMarker(cmd, "DrawIndirect: Single");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        size_t drawIndirectOffset = offset;
        vkCmdDrawIndirect(cmd, indirectData.buffer, drawIndirectOffset, 1, strideDraw);
        NextTest();

        setMarker(cmd, "DrawIndirect: Multiple");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        size_t drawIndexedIndirectOffset = offset;
        vkCmdDrawIndirect(cmd, indirectData.buffer, drawIndexedIndirectOffset, countDraw, strideDraw);
        NextTest();
        offset += countDraw * strideDraw;

        uint32_t countDrawIndexed = 3;
        uint32_t strideDrawIndexed = 2 * sizeof(uvec4);
        setMarker(cmd, "DrawIndexedIndirect");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawIndexedIndirect(cmd, indirectData.buffer, offset, countDrawIndexed,
                                 strideDrawIndexed);
        NextTest();

        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdBindIndexBuffer(cmd, indirectData.buffer, 1024, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirect(cmd, indirectData.buffer, offset, 1, strideDrawIndexed);
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        NextTest();
        offset += countDrawIndexed * strideDrawIndexed;

        if(draw_indirect_count)
        {
          pushMarker(cmd, "Draw Indirect Count");

          setMarker(cmd, "DrawIndirectCount(0:0)");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          size_t countOffset = 10 * sizeof(uvec4);
          vkCmdDrawIndirectCountKHR(cmd, indirectData.buffer, drawIndirectOffset,
                                    indirectData.buffer, countOffset, 0, strideDraw);
          NextTest();

          setMarker(cmd, "DrawIndexedIndirectCount(0:0)");
          size_t indexedCountOffset = countOffset + sizeof(uint32_t);
          vkCmdDrawIndexedIndirectCountKHR(cmd, indirectData.buffer, drawIndexedIndirectOffset,
                                           indirectData.buffer, countOffset, 0, strideDrawIndexed);
          NextTest();

          size_t countZeroOffset = indexedCountOffset + sizeof(uint32_t);
          setMarker(cmd, "DrawIndirectCount(10:0)");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndirectCountKHR(cmd, indirectData.buffer, drawIndirectOffset,
                                    indirectData.buffer, countZeroOffset, 10, strideDraw);
          NextTest();

          setMarker(cmd, "DrawIndexedIndirectCount(10:0)");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndexedIndirectCountKHR(cmd, indirectData.buffer, drawIndexedIndirectOffset,
                                           indirectData.buffer, countZeroOffset, 10,
                                           strideDrawIndexed);
          NextTest();

          setMarker(cmd, "DrawIndirectCount(10:N)");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndirectCountKHR(cmd, indirectData.buffer, drawIndirectOffset,
                                    indirectData.buffer, countOffset, 10, strideDraw);
          NextTest();

          setMarker(cmd, "DrawIndexedIndirectCount(10:N)");
          vkCmdSetViewport(cmd, 0, 1, &viewPort);
          vkCmdDrawIndexedIndirectCountKHR(cmd, indirectData.buffer, drawIndexedIndirectOffset,
                                           indirectData.buffer, indexedCountOffset, 10,
                                           strideDrawIndexed);
          NextTest();
          popMarker(cmd);
        }

        vkCmdEndRenderPass(cmd);
      }
      popMarker(cmd);

      vkh::cmdPipelineBarrier(
          cmd, {},
          {vkh::BufferMemoryBarrier(VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT, indirectData.buffer)});

      vkEndCommandBuffer(cmd);

      Submit(0, 3, {cmd});

      cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      // Nested Secondary Command Buffer
      if(nestedSecondaries)
      {
        pushMarker(cmd, "Nested Secondary Command Buffer");
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

        setMarker(cmd, "Draw");
        vkCmdBeginRenderPass(cmd, mainWindow->beginRP(),
                             VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkCmdExecuteCommands(cmd, 1, &nestedCmd);
        vkCmdEndRenderPass(cmd);

        setMarker(cmd, "Dispatch");
        vkCmdExecuteCommands(cmd, 1, &compNestedSecCmd);

        popMarker(cmd);
      }

      VkCommandBuffer backupDescBufCmd = VK_NULL_HANDLE;
      VkCommandBuffer restoreDescBufCmd = VK_NULL_HANDLE;
      std::vector<VkCommandBuffer> cmds;

      // Descriptor Buffer
      if(descBuffer)
      {
        VkBufferCopy copyRegion;
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = 0x1000;

        backupDescBufCmd = GetCommandBuffer();
        vkBeginCommandBuffer(backupDescBufCmd, vkh::CommandBufferBeginInfo());
        vkh::cmdPipelineBarrier(
            backupDescBufCmd, {},
            {
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                         descBuf.buffer),
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         descBackupBuf.buffer),
            });
        vkCmdCopyBuffer(backupDescBufCmd, descBuf.buffer, descBackupBuf.buffer, 1, &copyRegion);
        vkh::cmdPipelineBarrier(
            backupDescBufCmd, {},
            {
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         descBuf.buffer),
            });
        vkCmdFillBuffer(backupDescBufCmd, descBuf.buffer, 0, 0x1000, 0);
        vkEndCommandBuffer(backupDescBufCmd);

        restoreDescBufCmd = GetCommandBuffer();
        vkBeginCommandBuffer(restoreDescBufCmd, vkh::CommandBufferBeginInfo());
        vkh::cmdPipelineBarrier(
            restoreDescBufCmd, {},
            {
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                         descBackupBuf.buffer),
                vkh::BufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         descBuf.buffer),
            });
        vkCmdCopyBuffer(restoreDescBufCmd, descBackupBuf.buffer, descBuf.buffer, 1, &copyRegion);
        vkEndCommandBuffer(restoreDescBufCmd);

        pushMarker(cmd, "Descriptor Buffer");
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
        vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorBuffersEXT(cmd, 1, &descBuffBind);
        uint32_t descBuffSetIndex = 0;
        VkDeviceSize descBuffSetOffset = 0;

        setMarker(cmd, "Draw");
        vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descBuffPipeLayout,
                                           0, 1, &descBuffSetIndex, &descBuffSetOffset);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, descBuffPipe);
        // Vertex draw
        setMarker(cmd, "Vertex Draw");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        NextTest();
        // Indexed draw
        setMarker(cmd, "Indexed Draw");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);
        NextTest();
        vkCmdEndRenderPass(cmd);

        setMarker(cmd, "Dispatch");
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                           compDescBuffPipeLayout, 0, 1, &descBuffSetIndex,
                                           &descBuffSetOffset);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compDescBuffPipe);
        vkCmdDispatch(cmd, 1, 1, 1);

        popMarker(cmd);
        cmds.push_back(backupDescBufCmd);
        cmds.push_back(restoreDescBufCmd);
      }

      // Mesh Shaders
      if(meshShader)
      {
        pushMarker(cmd, "Mesh Shaders");
        vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
        vkCmdBeginRenderPass(cmd, mainWindow->beginRP(), VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshShaderPipe);

        setMarker(cmd, "Draw Mesh");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksEXT(cmd, 3, 1, 1);
        NextTest();

        setMarker(cmd, "Draw Mesh Indirect");
        uint32_t stride = sizeof(uvec4);
        size_t drawOffset = stride * 11;

        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksIndirectEXT(cmd, indirectData.buffer, drawOffset, 2, stride);
        NextTest();

        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksIndirectEXT(cmd, indirectData.buffer, drawOffset, 0, stride);
        NextTest();

        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksIndirectEXT(cmd, indirectData.buffer, drawOffset, 1, stride);
        NextTest();

        size_t countOffset = stride * 13;
        size_t countZeroOffset = countOffset + sizeof(uint32_t);

        setMarker(cmd, "Draw Mesh Indirect Count(20:1)");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksIndirectCountEXT(cmd, indirectData.buffer, drawOffset,
                                           indirectData.buffer, countOffset, 20, stride);
        NextTest();

        setMarker(cmd, "Draw Mesh Indirect Count(0:0)");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksIndirectCountEXT(cmd, indirectData.buffer, drawOffset,
                                           indirectData.buffer, countOffset, 0, stride);
        NextTest();

        setMarker(cmd, "Draw Mesh Indirect Count(10:0)");
        vkCmdSetViewport(cmd, 0, 1, &viewPort);
        vkCmdDrawMeshTasksIndirectCountEXT(cmd, indirectData.buffer, drawOffset,
                                           indirectData.buffer, countZeroOffset, 10, stride);
        NextTest();

        vkCmdEndRenderPass(cmd);
        popMarker(cmd);
      }

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      setMarker(cmd, "Barrier Command Submit Fence");

      vkEndCommandBuffer(cmd);
      cmds.push_back(cmd);

      Submit(1, 3, cmds);

      std::vector<VkCommandBuffer> cmds2;
      cmds2.push_back(barrierCmd);
      VkSubmitInfo submit = vkh::SubmitInfo(cmds2);
      for(uint32_t i = 0; i < 10; ++i)
      {
        vkWaitForFences(device, 1, &barrierCmdSubmitFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &barrierCmdSubmitFence);
        CHECK_VKR(vkQueueSubmit(queue, 1, &submit, barrierCmdSubmitFence));
        vkWaitForFences(device, 1, &barrierCmdSubmitFence, VK_TRUE, UINT64_MAX);
      }

      cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      pushMarker(cmd, "Multiple Secondary Command Buffer Executes");
      {
        vkCmdExecuteCommands(cmd, 1, &barrierSecCmd);
        vkCmdExecuteCommands(cmd, 1, &barrierSecCmd);
        vkCmdExecuteCommands(cmd, 1, &barrierSecCmd);
        vkCmdExecuteCommands(cmd, 1, &barrierSecCmd);
        popMarker(cmd);
      }

      vkEndCommandBuffer(cmd);

      Submit(2, 3, {cmd});

      Present();
    }

    vkDestroyFence(device, barrierCmdSubmitFence, NULL);
    vkDestroyCommandPool(device, barrierCmdPool, NULL);

    return 0;
  }
};

REGISTER_TEST();
