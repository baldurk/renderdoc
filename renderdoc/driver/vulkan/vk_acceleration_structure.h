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

#include "vk_manager.h"

class WrappedVulkan;

struct VkAccelerationStructureInfo
{
  struct GeometryData
  {
    VkGeometryTypeKHR geometryType;
    VkGeometryFlagsKHR flags;

    VkDeviceMemory readbackMem;
    VkDeviceSize memSize;
    VkDeviceSize memAlignment;

    VkBuffer replayBuf;

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

    struct Instances
    {
      // arrayOfPointers TODO
    };

    union
    {
      Triangles tris;
      Aabbs aabbs;
      Instances instances;
    };
  };

  ~VkAccelerationStructureInfo();

  rdcarray<VkAccelerationStructureGeometryKHR> convertGeometryData() const;

  VkDevice device;

  VkAccelerationStructureTypeKHR type =
      VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
  VkBuildAccelerationStructureFlagsKHR flags = 0;
  VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

  rdcarray<GeometryData> geometryData;
  rdcarray<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfo;

  bool accelerationStructureBuilt = false;
};

class VulkanAccelerationStructureManager
{
public:
  struct ASMemory
  {
    MemoryAllocation alloc;
    bool isTLAS;
  };

  explicit VulkanAccelerationStructureManager(WrappedVulkan *driver);

  void Shutdown();

  // During background capture we can't know in advance which input buffers will be used for AS
  // building, so we need to track all of them in order to map the device address back to a
  // ResourceId.  BDA isn't known at creation time and the usage flags aren't known at bind time so
  // initialise is a two-step process.  These should only be called when capturing
  void TrackInputBuffer(VkDevice device, VkResourceRecord *record);
  void UntrackInputBuffer(VkResourceRecord *record);

  // Allocates readback mem and injects commands into the command buffer so that the iput buffers
  // are copied.
  void CopyInputBuffers(VkCommandBuffer commandBuffer,
                        const VkAccelerationStructureBuildGeometryInfoKHR &info,
                        const VkAccelerationStructureBuildRangeInfoKHR *buildRange);

  // Compaction is ignored but the copy is still performed so the dst handle is valid on replay
  void CopyAccelerationStructure(VkCommandBuffer commandBuffer,
                                 const VkCopyAccelerationStructureInfoKHR &pInfo);

  VkFence DeleteReadbackMemOnCompletion(uint32_t submitCount, const VkSubmitInfo *pSubmits,
                                        VkFence fence);

  uint64_t GetSize_InitialState(ResourceId id, const VkInitialContents &initial);

  template <typename SerialiserType>
  bool Serialise(SerialiserType &ser, ResourceId id, const VkInitialContents *initial,
                 CaptureState state);

  void Apply(ResourceId id, const VkInitialContents &initial);

private:
  struct Allocation
  {
    Allocation(VkDeviceMemory m = VK_NULL_HANDLE, VkDeviceSize s = 0, VkBuffer b = VK_NULL_HANDLE)
        : mem(m), size(s), buf(b)
    {
    }

    VkDeviceMemory mem;
    VkDeviceSize size;
    VkBuffer buf;
  };

  struct RecordAndOffset
  {
    RecordAndOffset(VkResourceRecord *r = NULL, VkDeviceAddress a = 0x0, VkDeviceSize o = 0)
        : record(r), address(a), offset(o)
    {
    }

    VkResourceRecord *record;
    VkDeviceAddress address;
    VkDeviceSize offset;
  };

  template <typename SerialiserType>
  bool SerialiseASBuildData(SerialiserType &ser, ResourceId id, const VkInitialContents *initial,
                            CaptureState state);

  VkBuffer CreateReadBackBuffer(VkDevice device, VkDeviceSize size);
  Allocation CreateReadBackMemory(VkDevice device, VkDeviceSize size, VkDeviceSize alignment = 0);
  Allocation CreateReplayMemory(MemoryType memType, VkDeviceSize size, VkDeviceSize alignment = 0,
                                VkBufferUsageFlags extraUsageFlags = 0);

  bool FixUpReplayBDAs(VkAccelerationStructureInfo *asInfo,
                       rdcarray<VkAccelerationStructureGeometryKHR> &geoms);

  RecordAndOffset GetDeviceAddressData(VkDeviceAddress address) const;

  WrappedVulkan *m_pDriver;

  Threading::CriticalSection m_DeviceAddressLock;
  std::map<VkDeviceAddress, VkResourceRecord *> m_DeviceAddressToRecord;

  Threading::CriticalSection m_ReadbackMemDeleteLock;
  std::multimap<VkCommandBuffer, VkAccelerationStructureInfo *> m_ReadbackMemToDelete;
  std::map<VkFence, rdcarray<VkAccelerationStructureInfo *>> m_ReadbackMemDeleteSync;
  rdcarray<VkFence> m_DeleteFence;
  Threading::ThreadHandle m_ReadbackDeleteThread;
  int32_t m_ExitReadbackDeleteThread;

  Allocation scratch;
  VkDeviceOrHostAddressKHR scratchAddressUnion;
};
