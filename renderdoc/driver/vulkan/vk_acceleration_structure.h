/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#pragma once

#include "vk_resources.h"

class WrappedVulkan;
struct VkInitialContents;

struct VkAccelerationStructureInfo
{
  struct GeometryData
  {
    struct Triangles
    {
      VkFormat vertexFormat;
      VkDeviceSize vertexStride;
      uint32_t maxVertex;
      VkIndexType indexType;
      bool hasTransformData;
    };

    struct Aabbs
    {
      VkDeviceSize stride;
    };

    VkGeometryTypeKHR geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    VkGeometryFlagsKHR flags;

    VkDeviceMemory readbackMem;
    VkDeviceSize memSize;

    Triangles tris;
    Aabbs aabbs;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo;
  };

  ~VkAccelerationStructureInfo();

  void AddRef() { Atomic::Inc32(&refCount); }
  void Release();

  VkDevice device = VK_NULL_HANDLE;

  VkAccelerationStructureTypeKHR type =
      VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
  VkBuildAccelerationStructureFlagsKHR flags = 0;

  rdcarray<GeometryData> geometryData;

  bool accelerationStructureBuilt = false;

private:
  int32_t refCount = 1;
};

class VulkanAccelerationStructureManager
{
public:
  struct ASMemory
  {
    MemoryAllocation alloc;
    bool isTLAS;
  };

  struct Allocation
  {
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBuffer buf = VK_NULL_HANDLE;
  };

  struct RecordAndOffset
  {
    VkResourceRecord *record = NULL;
    VkDeviceAddress address = 0x0;
    VkDeviceSize offset = 0;
  };

  explicit VulkanAccelerationStructureManager(WrappedVulkan *driver);

  // Allocates readback mem and injects commands into the command buffer so that the input buffers
  // are copied.
  RDResult CopyInputBuffers(VkCommandBuffer commandBuffer,
                            const VkAccelerationStructureBuildGeometryInfoKHR &info,
                            const VkAccelerationStructureBuildRangeInfoKHR *buildRange);

  // Copies the metadata from src to dst, the input buffers are identical so don't need to be
  // duplicated.  Compaction is ignored but the copy is still performed so the dst handle is valid
  // on replay
  void CopyAccelerationStructure(VkCommandBuffer commandBuffer,
                                 const VkCopyAccelerationStructureInfoKHR &pInfo);

  // Called when the initial state is prepared.  Any TLAS and BLAS data is copied into temporary
  // buffers and the handles for that memory and the buffers is stored in the init state
  bool Prepare(VkAccelerationStructureKHR unwrappedAs, const rdcarray<uint32_t> &queueFamilyIndices,
               ASMemory &result);

  template <typename SerialiserType>
  bool Serialise(SerialiserType &ser, ResourceId id, const VkInitialContents *initial,
                 CaptureState state);

  // Called when the initial state is applied.  The AS data is deserialised from the upload buffer
  // into the acceleration structure
  void Apply(ResourceId id, const VkInitialContents &initial);

private:
  Allocation CreateReadBackMemory(VkDevice device, VkDeviceSize size, VkDeviceSize alignment = 0);

  RecordAndOffset GetDeviceAddressData(VkDeviceAddress address) const;

  template <typename T>
  void DeletePreviousInfo(VkCommandBuffer commandBuffer, T *info);

  VkDeviceSize SerialisedASSize(VkAccelerationStructureKHR unwrappedAs);

  WrappedVulkan *m_pDriver;
};
