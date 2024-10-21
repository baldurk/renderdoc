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
    };

    struct Aabbs
    {
      VkDeviceSize stride;
    };

    VkGeometryTypeKHR geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    VkGeometryFlagsKHR flags;

    Triangles tris;
    Aabbs aabbs;

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo;
    VkDeviceSize memOffset;
  };

  ~VkAccelerationStructureInfo();

  void AddRef() { Atomic::Inc32(&refCount); }
  void Release();

  uint64_t GetSerialisedSize() const;

  void convertGeometryData(rdcarray<VkAccelerationStructureGeometryKHR> &geometry) const;
  rdcarray<VkAccelerationStructureBuildRangeInfoKHR> getBuildRanges() const;

  VkAccelerationStructureTypeKHR type =
      VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
  VkBuildAccelerationStructureFlagsKHR flags = 0;

  rdcarray<GeometryData> geometryData;

  GPUBuffer readbackMem;
  VkDeviceSize memSize = 0;

  MemoryAllocation uploadAlloc;
  VkBuffer uploadBuf = VK_NULL_HANDLE;
  VkAccelerationStructureKHR replayAS = VK_NULL_HANDLE;

  bool accelerationStructureBuilt = false;

private:
  int32_t refCount = 1;
};

class VulkanAccelerationStructureManager
{
public:
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

  uint64_t GetSize_InitialState(ResourceId id, const VkInitialContents &initial);

  template <typename SerialiserType>
  bool Serialise(SerialiserType &ser, ResourceId id, const VkInitialContents *initial,
                 CaptureState state);

  // Called when the initial state is applied.
  void Apply(ResourceId id, VkInitialContents &initial);

private:
  struct Allocation
  {
    MemoryAllocation memAlloc;
    VkBuffer buf = VK_NULL_HANDLE;
  };

  struct RecordAndOffset
  {
    VkResourceRecord *record = NULL;
    VkDeviceAddress address = 0x0;
    VkDeviceSize offset = 0;
  };

  GPUBuffer CreateTempReadBackBuffer(VkDevice device, VkDeviceSize size);
  Allocation CreateTempReplayBuffer(MemoryType memType, VkDeviceSize size, VkDeviceSize alignment,
                                    VkBufferUsageFlags extraUsageFlags = 0);

  bool FixUpReplayBDAs(VkAccelerationStructureInfo *asInfo, VkBuffer buf,
                       rdcarray<VkAccelerationStructureGeometryKHR> &geoms);

  void UpdateScratch(VkDeviceSize requiredSize);

  RecordAndOffset GetDeviceAddressData(VkDeviceAddress address) const;

  template <typename T>
  void DeletePreviousInfo(VkCommandBuffer commandBuffer, T *info);

  WrappedVulkan *m_pDriver;

  Allocation scratch;
  VkDeviceOrHostAddressKHR scratchAddressUnion;
};
