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

#include "test_common.h"
#include "vk_test.h"

RD_TEST(VK_Ray_Query, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Test capture and replay of VK_KHR_ray_query";

  std::string frag = R"EOSHADER(

#version 460
#extension GL_EXT_ray_query : enable

layout(location = 0) in vec4 in_scene_pos;

layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

bool intersects_light(vec3 light_origin, vec3 pos)
{
  const float tmin = 0.01, tmax = 1000;
  const vec3  direction = light_origin - pos;

  rayQueryEXT query;
  rayQueryInitializeEXT(query, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, pos, tmin, direction.xyz, 1.0);
  rayQueryProceedEXT(query);

  return rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

void main(void)
{
  vec3 light_pos = vec3(0,0,0);
  o_color = intersects_light(light_pos, in_scene_pos.xyz) ? vec4(0.6, 0.6, 0.6, 1) : vec4(1, 1, 1, 1);
}

)EOSHADER";

  std::string vert = R"EOSHADER(

#version 460
#extension GL_EXT_ray_query : enable

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(location = 0) out vec4 scene_pos;

void main(void)
{
  gl_Position = scene_pos = vec4(position.xyz, 1.0f);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);

    devExts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);

    // Required by VK_KHR_acceleration_structure
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    // Required for ray queries
    devExts.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

    // Required by VK_KHR_spirv_1_4
    devExts.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    getPhysFeatures2(&asFeatures);

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR};
    getPhysFeatures2(&bdaFeatures);

    static VkPhysicalDeviceRayQueryFeaturesKHR rqFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    getPhysFeatures2(&rqFeatures);

    if(!asFeatures.accelerationStructure)
      Avail = "Acceleration structure feature 'accelerationStructure' not available";
    if(!bdaFeatures.bufferDeviceAddress)
      Avail = "Buffer device address feature 'bufferDeviceAddress' not available";
    if(!rqFeatures.rayQuery)
      Avail = "Ray query feature 'rayQuery' not available";

    devInfoNext = &asFeatures;
    asFeatures.pNext = &bdaFeatures;
    bdaFeatures.pNext = &rqFeatures;
  }

  int main()
  {
    vmaBDA = true;

    if(!Init())
      return 3;

    Vec3f vertices[] = {
        // Triangle below
        {-0.8f, 0.8f, 0.8f},
        {0.f, -0.8f, 0.8f},
        {0.8f, 0.8f, 0.8f},

        // Triangle above
        {0.0f, 0.3f, 0.5f},
        {-0.3f, -0.3f, 0.5f},
        {0.3f, -0.3f, 0.5f},
    };

    uint32_t indices[] = {0, 1, 2, 3, 4, 5};

    uint32_t primitiveCount = (uint32_t)sizeof(indices) / (sizeof(indices[0]) * 3);
    uint32_t indexCount = (uint32_t)sizeof(indices) / sizeof(indices[0]);
    uint32_t vertexCount = (uint32_t)sizeof(vertices) / sizeof(vertices[0]);

    VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

    constexpr VkBufferUsageFlags blasInputBufferUsageFlags =
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;

    VkTransformMatrixKHR identityTransformMatrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    VkTransformMatrixKHR blasTransformMatrix = identityTransformMatrix;

    const size_t vertexBufferSize = vertexCount * sizeof(vertices[0]);
    const size_t indexBufferSize = indexCount * sizeof(indices[0]);

    AllocatedBuffer blasVertexBuffer(
        this, vkh::BufferCreateInfo(vertexBufferSize, blasInputBufferUsageFlags),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    AllocatedBuffer blasIndexBuffer(
        this, vkh::BufferCreateInfo(indexBufferSize, blasInputBufferUsageFlags),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    blasVertexBuffer.upload(vertices, vertexBufferSize);
    blasIndexBuffer.upload(indices, indexBufferSize);

    /*
     * Create bottom level acceleration structure
     */
    VkAccelerationStructureGeometryKHR blasGeometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    blasGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    blasGeometry.geometry.triangles.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    blasGeometry.geometry.triangles.vertexFormat = vertexFormat;
    blasGeometry.geometry.triangles.maxVertex = vertexCount - 1;
    blasGeometry.geometry.triangles.vertexStride = sizeof(Vec3f);
    blasGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    blasGeometry.geometry.triangles.vertexData.deviceAddress = blasVertexBuffer.address;
    blasGeometry.geometry.triangles.indexData.deviceAddress = blasIndexBuffer.address;
    blasGeometry.geometry.triangles.transformData.deviceAddress = 0;
    std::vector<VkAccelerationStructureGeometryKHR> blasGeometries = {blasGeometry};

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {primitiveCount, 0, 0, 0};

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfosVector = {buildRangeInfo};
    VkAccelerationStructureBuildRangeInfoKHR *asBuildRangeInfos = asBuildRangeInfosVector.data();
    std::vector<uint32_t> primitiveCounts = {primitiveCount};

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    blasBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blasBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    blasBuildGeometryInfo.geometryCount = (uint32_t)blasGeometries.size();
    blasBuildGeometryInfo.pGeometries = blasGeometries.data();

    VkAccelerationStructureBuildSizesInfoKHR blasBuildSizesInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &blasBuildGeometryInfo, primitiveCounts.data(),
                                            &blasBuildSizesInfo);

    AllocatedBuffer blasBuffer(
        this,
        vkh::BufferCreateInfo(blasBuildSizesInfo.accelerationStructureSize,
                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkAccelerationStructureCreateInfoKHR blasCreateInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    blasCreateInfo.buffer = blasBuffer.buffer;
    blasCreateInfo.size = blasBuildSizesInfo.accelerationStructureSize;
    blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR blas;
    CHECK_VKR(vkCreateAccelerationStructureKHR(device, &blasCreateInfo, VK_NULL_HANDLE, &blas))

    VkAccelerationStructureDeviceAddressInfoKHR blasDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, NULL, blas};
    uint64_t blasDeviceAddress =
        vkGetAccelerationStructureDeviceAddressKHR(device, &blasDeviceAddressInfo);

    AllocatedBuffer blasScratchBuffer(
        this,
        vkh::BufferCreateInfo(
            blasBuildSizesInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    blasBuildGeometryInfo.scratchData.deviceAddress = blasScratchBuffer.address;
    blasBuildGeometryInfo.dstAccelerationStructure = blas;

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      CHECK_VKR(vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo()));
      vkCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuildGeometryInfo, &asBuildRangeInfos);
      CHECK_VKR(vkEndCommandBuffer(cmd));
      Submit(99, 99, {cmd});
    }

    /*
     * Create top level acceleration structure
     */
    VkTransformMatrixKHR tlasTransformMatrix = identityTransformMatrix;

    VkAccelerationStructureInstanceKHR asInstance = {
        tlasTransformMatrix, 0, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        blasDeviceAddress,
    };

    const size_t asInstanceSize = sizeof(asInstance);

    AllocatedBuffer instancesBuffer(
        this,
        vkh::BufferCreateInfo(asInstanceSize,
                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    instancesBuffer.upload(&asInstance, asInstanceSize);

    VkAccelerationStructureGeometryKHR tlasGeometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    tlasGeometry.geometry.instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    tlasGeometry.geometry.instances.data.deviceAddress = instancesBuffer.address;

    std::vector<VkAccelerationStructureGeometryKHR> tlasGeometries = {tlasGeometry};

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlasBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuildGeometryInfo.geometryCount = (uint32_t)tlasGeometries.size();
    tlasBuildGeometryInfo.pGeometries = tlasGeometries.data();

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &tlasBuildGeometryInfo, primitiveCounts.data(),
                                            &tlasBuildSizesInfo);

    AllocatedBuffer tlasBuffer(
        this,
        vkh::BufferCreateInfo(tlasBuildSizesInfo.accelerationStructureSize,
                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    tlasCreateInfo.buffer = tlasBuffer.buffer;
    tlasCreateInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
    tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkAccelerationStructureKHR tlas;
    CHECK_VKR(vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, VK_NULL_HANDLE, &tlas));

    AllocatedBuffer tlasScratchBuffer(
        this,
        vkh::BufferCreateInfo(
            tlasBuildSizesInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    tlasBuildGeometryInfo.scratchData.deviceAddress = tlasScratchBuffer.address;
    tlasBuildGeometryInfo.dstAccelerationStructure = tlas;

    asBuildRangeInfosVector[0].primitiveCount = 1;

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      CHECK_VKR(vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo()));
      vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuildGeometryInfo, &asBuildRangeInfos);
      CHECK_VKR(vkEndCommandBuffer(cmd));
      Submit(99, 99, {cmd});
    }

    /*
     * Create a pointless copy for more API coverage
     */
    AllocatedBuffer newBlasBuffer(
        this,
        vkh::BufferCreateInfo(blasBuildSizesInfo.accelerationStructureSize,
                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkAccelerationStructureCreateInfoKHR newBlasCreateInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    newBlasCreateInfo.buffer = newBlasBuffer.buffer;
    newBlasCreateInfo.size = blasBuildSizesInfo.accelerationStructureSize;
    newBlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR newBlas;
    CHECK_VKR(vkCreateAccelerationStructureKHR(device, &newBlasCreateInfo, VK_NULL_HANDLE, &newBlas))

    /*
     * Create triangle draw buffers
     */
    AllocatedBuffer trisVertexBuffer(
        this,
        vkh::BufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));
    AllocatedBuffer trisIndexBuffer(
        this,
        vkh::BufferCreateInfo(
            indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    trisVertexBuffer.upload(vertices);
    trisIndexBuffer.upload(indices);

    /*
     * Create descriptor set layout
     */
    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings = {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}};
    VkDescriptorSetLayout descriptorSetLayout =
        createDescriptorSetLayout(vkh::DescriptorSetLayoutCreateInfo(descriptorSetLayoutBindings));

    /*
     * Prepare pipelines
     */
    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    VkPipelineLayout layout =
        createPipelineLayout(vkh::PipelineLayoutCreateInfo{{descriptorSetLayout}});
    pipeCreateInfo.layout = layout;

    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.stages = {
        CompileShaderModule(vert, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(frag, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, Vec3f)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
    };

    pipeCreateInfo.depthStencilState.depthBoundsTestEnable = VK_FALSE;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    /*
     * Create descriptor sets
     */
    VkDescriptorSet descriptorSet = allocateDescriptorSet(descriptorSetLayout);

    VkWriteDescriptorSetAccelerationStructureKHR asWriteDescriptorInfo = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        NULL,
        1,
        &tlas,
    };

    VkWriteDescriptorSet asWriteDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    asWriteDescriptorSet.dstSet = descriptorSet;
    asWriteDescriptorSet.dstBinding = 0;
    asWriteDescriptorSet.descriptorCount = 1;
    asWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asWriteDescriptorSet.pNext = &asWriteDescriptorInfo;

    vkh::updateDescriptorSets(device, {asWriteDescriptorSet}, {});

    while(Running())
    {
      {
        VkCommandBuffer cmd = GetCommandBuffer();
        vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());
        pushMarker(cmd, "Copy AS");

        VkCopyAccelerationStructureInfoKHR copyInfo = {
            VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR, NULL, blas, newBlas,
            VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR,
        };
        vkCmdCopyAccelerationStructureKHR(cmd, &copyInfo);

        popMarker(cmd);
        CHECK_VKR(vkEndCommandBuffer(cmd));

        Submit(0, 2, {cmd});
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0,
                              VK_NULL_HANDLE);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {trisVertexBuffer.buffer}, {0});
      vkCmdBindIndexBuffer(cmd, trisIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      CHECK_VKR(vkEndCommandBuffer(cmd));

      Submit(1, 2, {cmd});

      Present();
    }

    vkDeviceWaitIdle(device);

    vkDestroyAccelerationStructureKHR(device, newBlas, NULL);
    vkDestroyAccelerationStructureKHR(device, tlas, NULL);
    vkDestroyAccelerationStructureKHR(device, blas, NULL);

    return 0;
  }
};

REGISTER_TEST();
