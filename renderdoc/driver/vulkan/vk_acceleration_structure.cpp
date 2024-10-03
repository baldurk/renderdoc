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

#include "vk_acceleration_structure.h"
#include "core/settings.h"
#include "vk_core.h"
#include "vk_manager.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

namespace
{
// Although the serialised data is implementation-defined in general, the header is defined:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap37.html#vkCmdCopyAccelerationStructureToMemoryKHR
constexpr std::size_t handleCountOffset = VK_UUID_SIZE + VK_UUID_SIZE + 8 + 8;
constexpr VkDeviceSize handleCountSize = 8;

// Spec says VkCopyAccelerationStructureToMemoryInfoKHR::dst::deviceAddress must be 256 bytes aligned
constexpr VkDeviceSize asBufferAlignment = 256;

VkDeviceSize IndexTypeSize(VkIndexType type)
{
  switch(type)
  {
    case VK_INDEX_TYPE_UINT32: return 4;
    case VK_INDEX_TYPE_UINT16: return 2;
    case VK_INDEX_TYPE_UINT8_KHR: return 1;
    default: return 0;
  }
}
}

VkAccelerationStructureInfo::~VkAccelerationStructureInfo()
{
  for(const GeometryData &geoData : geometryData)
  {
    if(geoData.readbackMem != VK_NULL_HANDLE)
      ObjDisp(device)->FreeMemory(Unwrap(device), geoData.readbackMem, NULL);
  }
}

void VkAccelerationStructureInfo::Release()
{
  const int32_t ref = Atomic::Dec32(&refCount);
  RDCASSERT(ref >= 0);
  if(ref <= 0)
    delete this;
}

VulkanAccelerationStructureManager::VulkanAccelerationStructureManager(WrappedVulkan *driver)
    : m_pDriver(driver)
{
}

RDResult VulkanAccelerationStructureManager::CopyInputBuffers(
    VkCommandBuffer commandBuffer, const VkAccelerationStructureBuildGeometryInfoKHR &info,
    const VkAccelerationStructureBuildRangeInfoKHR *buildRange)
{
  VkResourceRecord *cmdRecord = GetRecord(commandBuffer);
  RDCASSERT(cmdRecord);

  VkResourceRecord *asRecord = GetRecord(info.dstAccelerationStructure);
  RDCASSERT(asRecord);

  // If this is an update then replace the existing and safely delete it
  VkAccelerationStructureInfo *metadata = asRecord->accelerationStructureInfo;
  if(!metadata->geometryData.empty())
  {
    DeletePreviousInfo(commandBuffer, metadata);
    metadata = asRecord->accelerationStructureInfo = new VkAccelerationStructureInfo();
  }

  VkDevice device = cmdRecord->cmdInfo->device;
  metadata->device = device;
  metadata->type = info.type;
  metadata->flags = info.flags;

  metadata->geometryData.reserve(info.geometryCount);

  struct BufferData
  {
    BufferData() = default;
    BufferData(RecordAndOffset r) : rao(r)
    {
      if(rao.record != NULL)
        buf = ToUnwrappedHandle<VkBuffer>(rao.record->Resource);
    }

    operator bool() const { return buf != VK_NULL_HANDLE; }
    bool operator!() const { return buf == VK_NULL_HANDLE; }

    void SetReadPosition(VkDeviceSize startFrom) { start = startFrom; }
    VkDeviceSize GetReadPosition() const { return rao.offset + start; }

    RecordAndOffset rao;
    VkBuffer buf = VK_NULL_HANDLE;

    VkDeviceSize alignment = 0;
    VkDeviceSize size = 0;

  private:
    VkDeviceSize start = 0;
  };

  for(uint32_t i = 0; i < info.geometryCount; ++i)
  {
    // Work out the buffer size needed for each geometry type
    const VkAccelerationStructureGeometryKHR &geometry =
        info.pGeometries != NULL ? info.pGeometries[i] : *(info.ppGeometries[i]);
    const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo = buildRange[i];

    Allocation readbackmem;

    // Make sure nothing writes to our source buffers before we finish copying them
    VkMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT,
    };

    switch(geometry.geometryType)
    {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      {
        const VkAccelerationStructureGeometryTrianglesDataKHR &triInfo = geometry.geometry.triangles;

        // Find the associated VkBuffers
        BufferData vertexData = GetDeviceAddressData(triInfo.vertexData.deviceAddress);
        if(!vertexData)
        {
          RDCERR("Unable to find VkBuffer for vertex data at 0x%llx",
                 triInfo.vertexData.deviceAddress);
          continue;
        }

        BufferData indexData;
        if(triInfo.indexType != VK_INDEX_TYPE_NONE_KHR)
        {
          indexData = GetDeviceAddressData(triInfo.indexData.deviceAddress);
          if(!indexData)
          {
            RDCERR("Unable to find VkBuffer for index data at 0x%llx",
                   triInfo.indexData.deviceAddress);
            continue;
          }
        }

        BufferData transformData;
        if(triInfo.transformData.deviceAddress != 0x0)
        {
          transformData = GetDeviceAddressData(triInfo.transformData.deviceAddress);
          if(!transformData)
          {
            RDCERR("Unable to find VkBuffer for transform data at 0x%llx",
                   triInfo.transformData.deviceAddress);
            continue;
          }
        }

        // Find the alignment requirements for each type
        {
          VkMemoryRequirements mrq = {};

          // Vertex buffer.  The complexity here is that the rangeInfo members are interpreted
          // differently depending on whether or not index buffers are used
          ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), vertexData.buf, &mrq);
          vertexData.alignment = mrq.alignment;

          if(indexData)
          {
            // If we're using an index buffer we don't know how much of the vertex buffer we need,
            // and we can't trust the app to set maxVertex correctly, so we take the whole buffer
            vertexData.size = vertexData.rao.record->memSize - vertexData.rao.offset;
            vertexData.SetReadPosition(0);
          }
          else
          {
            vertexData.size = rangeInfo.primitiveCount * 3 * triInfo.vertexStride;
            vertexData.SetReadPosition(rangeInfo.primitiveOffset +
                                       (triInfo.vertexStride * rangeInfo.firstVertex));
          }

          // Index buffer
          if(indexData)
          {
            ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), indexData.buf, &mrq);
            indexData.alignment = mrq.alignment;
            indexData.size = rangeInfo.primitiveCount * 3 * IndexTypeSize(triInfo.indexType);
            indexData.SetReadPosition(rangeInfo.primitiveOffset);
          }

          // Transform buffer
          if(transformData)
          {
            ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), transformData.buf, &mrq);
            transformData.alignment = mrq.alignment;
            transformData.size = sizeof(VkTransformMatrixKHR);
            transformData.SetReadPosition(rangeInfo.transformOffset);
          }
        }
        const VkDeviceSize maxAlignment =
            RDCMAX(RDCMAX(vertexData.alignment, indexData.alignment), transformData.alignment);

        // We want to copy the input buffers into one big block so sum the sizes up together
        const VkDeviceSize totalMemSize = AlignUp(vertexData.size, vertexData.alignment) +
                                          AlignUp(indexData.size, indexData.alignment) +
                                          AlignUp(transformData.size, transformData.alignment);

        readbackmem = CreateReadBackMemory(device, totalMemSize, maxAlignment);
        if(readbackmem.mem == VK_NULL_HANDLE)
        {
          RDCERR("Unable to allocate AS triangle input buffer readback memory (size: %u bytes)",
                 totalMemSize);
          continue;
        }

        // Insert copy commands
        VkBufferCopy region = {
            vertexData.GetReadPosition(),
            0,
            vertexData.size,
        };
        ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), vertexData.buf, readbackmem.buf, 1,
                                       &region);

        if(indexData)
        {
          region = {
              indexData.GetReadPosition(),
              AlignUp(vertexData.size, vertexData.alignment),
              indexData.size,
          };
          ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), indexData.buf, readbackmem.buf, 1,
                                         &region);
        }

        if(transformData)
        {
          region = {
              transformData.GetReadPosition(),
              AlignUp(vertexData.size, vertexData.alignment) +
                  AlignUp(indexData.size, indexData.alignment),
              transformData.size,
          };
          ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), transformData.buf, readbackmem.buf,
                                         1, &region);
        }

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.readbackMem = readbackmem.mem;
        geoData.memSize = readbackmem.size;

        geoData.tris.vertexFormat = geometry.geometry.triangles.vertexFormat;
        geoData.tris.vertexStride = geometry.geometry.triangles.vertexStride;
        geoData.tris.maxVertex = geometry.geometry.triangles.maxVertex;
        geoData.tris.indexType = geometry.geometry.triangles.indexType;
        geoData.tris.hasTransformData = transformData;

        // Frustratingly rangeInfo.primitiveOffset represents either the offset into the index or
        // vertex buffer depending if indices are in use or not
        VkAccelerationStructureBuildRangeInfoKHR &buildData = geoData.buildRangeInfo;
        buildData.primitiveCount = rangeInfo.primitiveCount;
        buildData.primitiveOffset = 0;
        buildData.firstVertex = 0;
        buildData.transformOffset = 0;

        if(indexData)
        {
          buildData.primitiveOffset = (uint32_t)AlignUp(vertexData.size, vertexData.alignment);
          buildData.firstVertex = rangeInfo.firstVertex;
        }
        if(transformData)
          buildData.transformOffset = (uint32_t)(AlignUp(vertexData.size, vertexData.alignment) +
                                                 AlignUp(indexData.size, indexData.alignment));

        metadata->geometryData.push_back(geoData);

        break;
      }
      case VK_GEOMETRY_TYPE_AABBS_KHR:
      {
        const VkAccelerationStructureGeometryAabbsDataKHR &aabbInfo = geometry.geometry.aabbs;

        // Find the associated VkBuffer
        BufferData data = GetDeviceAddressData(aabbInfo.data.deviceAddress);
        if(!data)
        {
          RDCERR("Unable to find VkBuffer for AABB data at 0x%llx", aabbInfo.data.deviceAddress);
          continue;
        }

        data.size = rangeInfo.primitiveCount * sizeof(VkAabbPositionsKHR);
        data.SetReadPosition(rangeInfo.primitiveOffset);

        // Get the alignment
        VkMemoryRequirements mrq = {};
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), data.buf, &mrq);

        // Allocate copy buffer
        readbackmem = CreateReadBackMemory(device, data.size, mrq.alignment);
        if(readbackmem.mem == VK_NULL_HANDLE)
        {
          RDCERR("Unable to allocate AS AABB input buffer readback memory (size: %u bytes)",
                 mrq.size);
          continue;
        }

        // Insert copy commands
        VkBufferCopy region = {
            data.GetReadPosition(),
            0,
            data.size,
        };
        ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), data.buf, readbackmem.buf, 1, &region);

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.readbackMem = readbackmem.mem;
        geoData.memSize = readbackmem.size;

        geoData.aabbs.stride = aabbInfo.stride;

        geoData.buildRangeInfo = rangeInfo;
        geoData.buildRangeInfo.primitiveOffset = 0;

        metadata->geometryData.push_back(geoData);

        break;
      }
      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      {
        const VkAccelerationStructureGeometryInstancesDataKHR &instanceInfo =
            geometry.geometry.instances;

        if(instanceInfo.arrayOfPointers)
          RETURN_ERROR_RESULT(ResultCode::InternalError,
                              "AS instance build arrayOfPointers unsupported");

        // Find the associated VkBuffer
        BufferData data = GetDeviceAddressData(instanceInfo.data.deviceAddress);
        if(!data)
        {
          RDCERR("Unable to find VkBuffer for instance data at 0x%llx",
                 instanceInfo.data.deviceAddress);
          continue;
        }

        data.size = rangeInfo.primitiveCount * sizeof(VkAccelerationStructureInstanceKHR);
        data.SetReadPosition(rangeInfo.primitiveOffset);

        // Get the alignment
        VkMemoryRequirements mrq = {};
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), data.buf, &mrq);

        // Allocate copy buffer
        readbackmem = CreateReadBackMemory(device, data.size, mrq.alignment);
        if(readbackmem.mem == VK_NULL_HANDLE)
        {
          RDCERR("Unable to allocate AS instance input buffer readback memory (size: %u bytes)",
                 data.size);
          continue;
        }

        // Insert copy commands
        VkBufferCopy region = {
            data.GetReadPosition(),
            0,
            data.size,
        };
        ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), data.buf, readbackmem.buf, 1, &region);

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.readbackMem = readbackmem.mem;
        geoData.memSize = readbackmem.size;

        geoData.buildRangeInfo = rangeInfo;
        geoData.buildRangeInfo.primitiveOffset = 0;

        metadata->geometryData.push_back(geoData);

        break;
      }
      default: RDCERR("Unhandled geometry type: %d", geometry.geometryType); continue;
    }

    // Insert barriers to block any other commands until the buffers are copied
    if(readbackmem.mem != VK_NULL_HANDLE)
    {
      ObjDisp(device)->CmdPipelineBarrier(Unwrap(commandBuffer), VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1, &barrier, 0,
                                          VK_NULL_HANDLE, 0, VK_NULL_HANDLE);

      // We can schedule buffer deletion now as it isn't needed anymore
      cmdRecord->cmdInfo->pendingSubmissionCompleteCallbacks->callbacks.push_back(
          [device, buffer = readbackmem.buf]() {
            ObjDisp(device)->DestroyBuffer(Unwrap(device), buffer, NULL);
          });
    }
  }

  return {};
}

void VulkanAccelerationStructureManager::CopyAccelerationStructure(
    VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR &pInfo)
{
  VkResourceRecord *srcRecord = GetRecord(pInfo.src);
  RDCASSERT(srcRecord->accelerationStructureInfo != NULL);

  // Delete any previous data associated with AS
  VkResourceRecord *dstRecord = GetRecord(pInfo.dst);
  VkAccelerationStructureInfo *info = dstRecord->accelerationStructureInfo;
  if(!info->geometryData.empty())
    DeletePreviousInfo(commandBuffer, info);

  // Rather than copy the backing mem, we can just increase the ref count.  If there is an update
  // build to the AS then the ref will be replaced in the record, so there's no risk of aliasing.
  // The copy mode is irrelevant as it doesn't affect the rebuild
  dstRecord->accelerationStructureInfo = srcRecord->accelerationStructureInfo;
  dstRecord->accelerationStructureInfo->AddRef();
}

bool VulkanAccelerationStructureManager::Prepare(VkAccelerationStructureKHR unwrappedAs,
                                                 const rdcarray<uint32_t> &queueFamilyIndices,
                                                 ASMemory &result)
{
  const VkDeviceSize serialisedSize = SerialisedASSize(unwrappedAs);

  const VkDevice d = m_pDriver->GetDev();
  VkResult vkr = VK_SUCCESS;

  // since this happens during capture, we don't want to start serialising extra buffer creates,
  // leave this buffer as unwrapped
  VkBuffer dstBuf = VK_NULL_HANDLE;

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      serialisedSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
  };

  // we make the buffer concurrently accessible by all queue families to not invalidate the
  // contents of the memory we're reading back from.
  bufInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
  bufInfo.queueFamilyIndexCount = (uint32_t)queueFamilyIndices.size();
  bufInfo.pQueueFamilyIndices = queueFamilyIndices.data();

  // spec requires that CONCURRENT must specify more than one queue family. If there is only one
  // queue family, we can safely use exclusive.
  if(bufInfo.queueFamilyIndexCount == 1)
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
  CHECK_VKR(m_pDriver, vkr);

  m_pDriver->AddPendingObjectCleanup(
      [d, dstBuf]() { ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL); });

  VkMemoryRequirements mrq = {};
  ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);

  mrq.alignment = RDCMAX(mrq.alignment, asBufferAlignment);

  const MemoryAllocation readbackmem = m_pDriver->AllocateMemoryForResource(
      true, mrq, MemoryScope::InitialContents, MemoryType::Readback);
  if(readbackmem.mem == VK_NULL_HANDLE)
    return false;

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem.mem), readbackmem.offs);
  CHECK_VKR(m_pDriver, vkr);

  const VkBufferDeviceAddressInfo addrInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, NULL,
                                              dstBuf};
  const VkDeviceAddress dstBufAddr = ObjDisp(d)->GetBufferDeviceAddressKHR(Unwrap(d), &addrInfo);

  VkCommandBuffer cmd = m_pDriver->GetInitStateCmd();
  if(cmd == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't acquire command buffer");
    return false;
  }

  const VkDeviceSize nonCoherentAtomSize = m_pDriver->GetDeviceProps().limits.nonCoherentAtomSize;
  byte *mappedDstBuffer = NULL;
  VkDeviceSize size;

  if(m_pDriver->GetDriverInfo().MaliBrokenASDeviceSerialisation())
  {
    size = AlignUp(serialisedSize, nonCoherentAtomSize);

    vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(readbackmem.mem), readbackmem.offs, size, 0,
                                (void **)&mappedDstBuffer);
    CHECK_VKR(m_pDriver, vkr);

    // Copy the data using host-commands but into mapped memory
    VkCopyAccelerationStructureToMemoryInfoKHR copyInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR, NULL};
    copyInfo.src = unwrappedAs;
    copyInfo.dst.hostAddress = mappedDstBuffer;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR;
    ObjDisp(d)->CopyAccelerationStructureToMemoryKHR(Unwrap(d), VK_NULL_HANDLE, &copyInfo);
  }
  else
  {
    VkCopyAccelerationStructureToMemoryInfoKHR copyInfo = {
        VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR, NULL};
    copyInfo.src = unwrappedAs;
    copyInfo.dst.deviceAddress = dstBufAddr;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR;
    ObjDisp(d)->CmdCopyAccelerationStructureToMemoryKHR(Unwrap(cmd), &copyInfo);

    // It's not ideal but we have to flush here because we need to map the data in order to read
    // the BLAS addresses which means we need to have ensured that it has been copied beforehand
    m_pDriver->CloseInitStateCmd();
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    // Now serialised AS data has been copied to a readable buffer, we need to expose the data to
    // the host
    size = AlignUp(handleCountOffset + handleCountSize, nonCoherentAtomSize);

    vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(readbackmem.mem), readbackmem.offs, size, 0,
                                (void **)&mappedDstBuffer);
    CHECK_VKR(m_pDriver, vkr);
  }

  // invalidate the cpu cache for this memory range to avoid reading stale data
  const VkMappedMemoryRange range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, Unwrap(readbackmem.mem), readbackmem.offs, size,
  };
  vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
  CHECK_VKR(m_pDriver, vkr);

  // Count the BLAS device addresses to update the AS type
  const uint64_t handleCount = *(uint64_t *)(mappedDstBuffer + handleCountOffset);
  result = {readbackmem, true};
  result.isTLAS = handleCount > 0;

  ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(result.alloc.mem));

  return true;
}

template <typename SerialiserType>
bool VulkanAccelerationStructureManager::Serialise(SerialiserType &ser, ResourceId id,
                                                   const VkInitialContents *initial,
                                                   CaptureState state)
{
  VkDevice d = !IsStructuredExporting(state) ? m_pDriver->GetDev() : VK_NULL_HANDLE;
  const bool replayingAndReading = ser.IsReading() && IsReplayMode(state);
  VkResult vkr = VK_SUCCESS;

  byte *contents = NULL;
  uint64_t contentsSize = initial ? initial->mem.size : 0;
  MemoryAllocation mappedMem;

  // Serialise this separately so that it can be used on reading to prepare the upload memory
  SERIALISE_ELEMENT(contentsSize);

  const VkDeviceSize nonCoherentAtomSize = m_pDriver->GetDeviceProps().limits.nonCoherentAtomSize;

  // the memory/buffer that we allocated on read, to upload the initial contents.
  MemoryAllocation uploadMemory;
  VkBuffer uploadBuf = VK_NULL_HANDLE;

  if(ser.IsWriting())
  {
    if(initial && initial->mem.mem != VK_NULL_HANDLE)
    {
      const VkDeviceSize size = AlignUp(initial->mem.size, nonCoherentAtomSize);

      mappedMem = initial->mem;
      vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), initial->mem.offs, size, 0,
                                  (void **)&contents);
      CHECK_VKR(m_pDriver, vkr);

      // invalidate the cpu cache for this memory range to avoid reading stale data
      const VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, Unwrap(mappedMem.mem), mappedMem.offs, size,
      };

      vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
      CHECK_VKR(m_pDriver, vkr);
    }
  }
  else if(IsReplayMode(state) && !ser.IsErrored())
  {
    // create a buffer with memory attached, which we will fill with the initial contents
    const VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        contentsSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    vkr = m_pDriver->vkCreateBuffer(d, &bufInfo, NULL, &uploadBuf);
    CHECK_VKR(m_pDriver, vkr);

    VkMemoryRequirements mrq = {};
    m_pDriver->vkGetBufferMemoryRequirements(d, uploadBuf, &mrq);

    mrq.alignment = RDCMAX(mrq.alignment, asBufferAlignment);

    uploadMemory = m_pDriver->AllocateMemoryForResource(true, mrq, MemoryScope::InitialContents,
                                                        MemoryType::Upload);

    if(uploadMemory.mem == VK_NULL_HANDLE)
      return false;

    vkr = m_pDriver->vkBindBufferMemory(d, uploadBuf, uploadMemory.mem, uploadMemory.offs);
    CHECK_VKR(m_pDriver, vkr);

    mappedMem = uploadMemory;

    vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs,
                                AlignUp(mappedMem.size, nonCoherentAtomSize), 0, (void **)&contents);
    CHECK_VKR(m_pDriver, vkr);

    if(!contents)
    {
      RDCERR("Manually reporting failed memory map");
      CHECK_VKR(m_pDriver, VK_ERROR_MEMORY_MAP_FAILED);
      return false;
    }

    if(vkr != VK_SUCCESS)
      return false;
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("Serialised AS"_lit, contents, contentsSize, SerialiserFlags::NoFlags).Important();

  // unmap the resource we mapped before - we need to do this on read and on write.
  bool isTLAS = false;
  if(!IsStructuredExporting(state) && mappedMem.mem != VK_NULL_HANDLE)
  {
    if(replayingAndReading)
    {
      // first ensure we flush the writes from the cpu to gpu memory
      const VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,        NULL, Unwrap(mappedMem.mem), mappedMem.offs,
          AlignUp(mappedMem.size, nonCoherentAtomSize),
      };

      vkr = ObjDisp(d)->FlushMappedMemoryRanges(Unwrap(d), 1, &range);
      CHECK_VKR(m_pDriver, vkr);

      // Read the AS's BLAS handle count to determine if it's top or bottom level
      isTLAS = *((uint64_t *)(contents + handleCountOffset)) > 0;
    }

    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem.mem));
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayMode(state) && contentsSize > 0)
  {
    VkInitialContents initialContents(eResAccelerationStructureKHR, uploadMemory);
    initialContents.isTLAS = isTLAS;
    initialContents.buf = uploadBuf;

    m_pDriver->GetResourceManager()->SetInitialContents(id, initialContents);
  }

  return true;
}

template bool VulkanAccelerationStructureManager::Serialise(ReadSerialiser &ser, ResourceId id,
                                                            const VkInitialContents *initial,
                                                            CaptureState state);
template bool VulkanAccelerationStructureManager::Serialise(WriteSerialiser &ser, ResourceId id,
                                                            const VkInitialContents *initial,
                                                            CaptureState state);

void VulkanAccelerationStructureManager::Apply(ResourceId id, const VkInitialContents &initial)
{
  VkCommandBuffer cmd = m_pDriver->GetInitStateCmd();
  if(cmd == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't acquire command buffer");
    return;
  }

  const VkAccelerationStructureKHR unwrappedAs =
      Unwrap(m_pDriver->GetResourceManager()->GetCurrentHandle<VkAccelerationStructureKHR>(id));
  const VkDevice d = m_pDriver->GetDev();

  VkMarkerRegion::Begin(StringFormat::Fmt("Initial state for %s", ToStr(id).c_str()), cmd);

  if(m_pDriver->GetDriverInfo().MaliBrokenASDeviceSerialisation())
  {
    const VkDeviceSize size =
        AlignUp(initial.mem.size, m_pDriver->GetDeviceProps().limits.nonCoherentAtomSize);

    // Copy the data using host-commands but from mapped memory
    byte *mappedSrcBuffer = NULL;
    VkResult vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(initial.mem.mem), initial.mem.offs, size,
                                         0, (void **)&mappedSrcBuffer);
    CHECK_VKR(m_pDriver, vkr);

    VkCopyMemoryToAccelerationStructureInfoKHR copyInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR};
    copyInfo.src.hostAddress = mappedSrcBuffer;
    copyInfo.dst = unwrappedAs;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR;
    ObjDisp(d)->CopyMemoryToAccelerationStructureKHR(Unwrap(d), VK_NULL_HANDLE, &copyInfo);
  }
  else
  {
    const VkBufferDeviceAddressInfo addrInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, NULL,
                                                Unwrap(initial.buf)};
    const VkDeviceAddress uploadBufAddr = ObjDisp(d)->GetBufferDeviceAddressKHR(Unwrap(d), &addrInfo);

    VkCopyMemoryToAccelerationStructureInfoKHR copyInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR};
    copyInfo.src.deviceAddress = uploadBufAddr;
    copyInfo.dst = unwrappedAs;
    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR;
    ObjDisp(d)->CmdCopyMemoryToAccelerationStructureKHR(Unwrap(cmd), &copyInfo);
  }

  VkMarkerRegion::End(cmd);

  if(Vulkan_Debug_SingleSubmitFlushing())
  {
    m_pDriver->CloseInitStateCmd();
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }
}

VulkanAccelerationStructureManager::Allocation VulkanAccelerationStructureManager::CreateReadBackMemory(
    VkDevice device, VkDeviceSize size, VkDeviceSize alignment)
{
  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
  };

  // we make the buffer concurrently accessible by all queue families to not invalidate the
  // contents of the memory we're reading back from.
  bufInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
  bufInfo.queueFamilyIndexCount = (uint32_t)m_pDriver->GetQueueFamilyIndices().size();
  bufInfo.pQueueFamilyIndices = m_pDriver->GetQueueFamilyIndices().data();

  // spec requires that CONCURRENT must specify more than one queue family. If there is only one
  // queue family, we can safely use exclusive.
  if(bufInfo.queueFamilyIndexCount == 1)
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  Allocation readbackmem;
  VkResult vkr = ObjDisp(device)->CreateBuffer(Unwrap(device), &bufInfo, NULL, &readbackmem.buf);
  if(vkr != VK_SUCCESS)
  {
    RDCERR("Failed to create readback buffer");
    return {};
  }

  VkMemoryRequirements mrq = {};
  ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), readbackmem.buf, &mrq);

  if(alignment != 0)
    mrq.alignment = RDCMAX(mrq.alignment, alignment);

  readbackmem.size = AlignUp(mrq.size, mrq.alignment);
  readbackmem.size =
      AlignUp(readbackmem.size, m_pDriver->GetDeviceProps().limits.nonCoherentAtomSize);

  VkMemoryAllocateFlagsInfo flagsInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      NULL,
      VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };
  VkMemoryAllocateInfo info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &flagsInfo,
      readbackmem.size,
      m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
  };

  vkr = ObjDisp(device)->AllocateMemory(Unwrap(device), &info, NULL, &readbackmem.mem);
  if(vkr != VK_SUCCESS)
  {
    RDCERR("Failed to allocate readback memory");
    return {};
  }

  vkr = ObjDisp(device)->BindBufferMemory(Unwrap(device), readbackmem.buf, readbackmem.mem, 0);
  if(vkr != VK_SUCCESS)
  {
    RDCERR("Failed to bind readback memory");
    return {};
  }

  return readbackmem;
}

VulkanAccelerationStructureManager::RecordAndOffset VulkanAccelerationStructureManager::GetDeviceAddressData(
    VkDeviceAddress address) const
{
  RecordAndOffset result;

  ResourceId id;
  m_pDriver->GetResIDFromAddr(address, id, result.offset);

  // No match
  if(id == ResourceId())
    return {};

  // Convert the ID to a resource record
  result.record = m_pDriver->GetResourceManager()->GetResourceRecord(id);
  RDCASSERTMSG("Unable to find record", result.record, id);
  if(!result.record)
    return {};

  result.address = address - result.offset;
  return result;
}

template <typename T>
void VulkanAccelerationStructureManager::DeletePreviousInfo(VkCommandBuffer commandBuffer, T *info)
{
  VkResourceRecord *cmdRecord = GetRecord(commandBuffer);
  cmdRecord->cmdInfo->pendingSubmissionCompleteCallbacks->callbacks.push_back(
      [info]() { info->Release(); });
}

// OMM suport todo
template void VulkanAccelerationStructureManager::DeletePreviousInfo(VkCommandBuffer commandBuffer,
                                                                     VkAccelerationStructureInfo *info);

VkDeviceSize VulkanAccelerationStructureManager::SerialisedASSize(VkAccelerationStructureKHR unwrappedAs)
{
  VkDevice d = m_pDriver->GetDev();

  // Create query pool
  VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  info.queryCount = 1;
  info.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR;

  VkQueryPool pool;
  VkResult vkr = ObjDisp(d)->CreateQueryPool(Unwrap(d), &info, NULL, &pool);
  CHECK_VKR(m_pDriver, vkr);

  // Reset query pool
  VkCommandBuffer cmd = m_pDriver->GetInitStateCmd();
  ObjDisp(d)->CmdResetQueryPool(Unwrap(cmd), pool, 0, 1);

  // Get the size
  ObjDisp(d)->CmdWriteAccelerationStructuresPropertiesKHR(
      Unwrap(cmd), 1, &unwrappedAs, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR,
      pool, 0);

  m_pDriver->CloseInitStateCmd();
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  VkDeviceSize size = 0;
  vkr = ObjDisp(d)->GetQueryPoolResults(Unwrap(d), pool, 0, 1, sizeof(VkDeviceSize), &size,
                                        sizeof(VkDeviceSize),
                                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  CHECK_VKR(m_pDriver, vkr);

  // Clean up
  ObjDisp(d)->DestroyQueryPool(Unwrap(d), pool, NULL);

  return size;
}
