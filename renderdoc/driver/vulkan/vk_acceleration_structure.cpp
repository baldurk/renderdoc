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
#include <numeric>
#include "core/settings.h"
#include "vk_core.h"

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
    default: return 0;
  }
}
}

DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData::Triangles);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData::Aabbs);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData::Instances);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData::Triangles &el)
{
  SERIALISE_MEMBER(vertexFormat);
  SERIALISE_MEMBER(vertexStride);
  SERIALISE_MEMBER(maxVertex);
  SERIALISE_MEMBER(indexType);
  SERIALISE_MEMBER(hasTransformData);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData::Triangles);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData::Aabbs &el)
{
  SERIALISE_MEMBER(stride);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData::Aabbs);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData::Instances &el)
{
  // arrayOfPointers TODO
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData::Instances);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData &el)
{
  SERIALISE_MEMBER(geometryType);
  SERIALISE_MEMBER_TYPED(VkGeometryFlagBitsKHR, flags).TypedAs("VkGeometryFlagsKHR"_lit);
  SERIALISE_MEMBER(memSize);
  SERIALISE_MEMBER(memAlignment);

  switch(el.geometryType)
  {
    case VK_GEOMETRY_TYPE_TRIANGLES_KHR: SERIALISE_MEMBER(tris); break;
    case VK_GEOMETRY_TYPE_AABBS_KHR: SERIALISE_MEMBER(aabbs); break;
    case VK_GEOMETRY_TYPE_INSTANCES_KHR: SERIALISE_MEMBER(instances); break;
    default: RDCASSERT(false);
  }
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo &el)
{
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER_TYPED(VkBuildAccelerationStructureFlagBitsKHR, flags)
      .TypedAs("VkBuildAccelerationStructureFlagsKHR"_lit);
  SERIALISE_MEMBER(geometryData);
  SERIALISE_MEMBER(buildRangeInfo);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo);

VkAccelerationStructureInfo::~VkAccelerationStructureInfo()
{
  for(const GeometryData &geoData : geometryData)
  {
    if(geoData.readbackMem != VK_NULL_HANDLE)
      ObjDisp(device)->FreeMemory(Unwrap(device), geoData.readbackMem, NULL);
  }
}

rdcarray<VkAccelerationStructureGeometryKHR> VkAccelerationStructureInfo::convertGeometryData() const
{
  rdcarray<VkAccelerationStructureGeometryKHR> result;
  result.reserve(geometryData.size());

  for(const VkAccelerationStructureInfo::GeometryData &g : geometryData)
  {
    VkAccelerationStructureGeometryDataKHR geoUnion = {};
    switch(g.geometryType)
    {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      {
        VkDeviceOrHostAddressConstKHR vData;
        vData.deviceAddress = 0x0;

        VkDeviceOrHostAddressConstKHR iData;
        iData.deviceAddress = 0x0;

        // vkGetAccelerationStructureBuildSizesKHR just checks if the transform BDA is non-null,
        // so fudge that here
        VkDeviceOrHostAddressConstKHR tData;
        tData.deviceAddress = g.tris.hasTransformData ? 0x1 : 0x0;

        geoUnion.triangles = VkAccelerationStructureGeometryTrianglesDataKHR{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            NULL,
            g.tris.vertexFormat,
            vData,
            g.tris.vertexStride,
            g.tris.maxVertex,
            g.tris.indexType,
            iData,
            tData,
        };
        break;
      }
      case VK_GEOMETRY_TYPE_AABBS_KHR:
      {
        VkDeviceOrHostAddressConstKHR aData;
        aData.deviceAddress = 0x0;

        geoUnion.aabbs = VkAccelerationStructureGeometryAabbsDataKHR{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR,
            NULL,
            aData,
            g.aabbs.stride,
        };
        break;
      }
      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      {
        VkDeviceOrHostAddressConstKHR iData;
        iData.deviceAddress = 0x0;

        geoUnion.instances = VkAccelerationStructureGeometryInstancesDataKHR{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            NULL,
            false,
            iData,
        };
        break;
      }
      default: RDCERR("Unhandled geometry type: %d", g.geometryType); return {};
    }

    result.push_back(
        VkAccelerationStructureGeometryKHR{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                                           NULL, g.geometryType, geoUnion, g.flags});
  }

  return result;
}

VulkanAccelerationStructureManager::VulkanAccelerationStructureManager(WrappedVulkan *driver)
    : m_pDriver(driver), m_ReadbackDeleteThread(0)
{
}

void VulkanAccelerationStructureManager::Shutdown()
{
  // stop the readback mem clean up thread
  if(m_ReadbackDeleteThread)
  {
    Atomic::Inc32(&m_ExitReadbackDeleteThread);
    Threading::JoinThread(m_ReadbackDeleteThread);
    Threading::CloseThread(m_ReadbackDeleteThread);
    m_ReadbackDeleteThread = 0;
  }

  // Clean up any scratch mem when the replay is shutdown
  if(scratch.mem != VK_NULL_HANDLE)
  {
    const VkDevice d = m_pDriver->GetDev();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), scratch.buf, NULL);
    ObjDisp(d)->FreeMemory(Unwrap(d), scratch.mem, NULL);

    scratch = {};
    scratchAddressUnion.deviceAddress = 0x0;
  }
}

void VulkanAccelerationStructureManager::TrackInputBuffer(VkDevice device, VkResourceRecord *record)
{
  if(!m_pDriver->AccelerationStructures() || !IsBackgroundCapturing(m_pDriver->GetState()))
    return;

  SCOPED_LOCK(m_DeviceAddressLock);

  const VkBufferDeviceAddressInfo addrInfo = {
      VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      NULL,
      ToUnwrappedHandle<VkBuffer>(record->Resource),
  };

  const VkDeviceAddress address =
      ObjDisp(device)->GetBufferDeviceAddressKHR(Unwrap(device), &addrInfo);
  m_DeviceAddressToRecord.emplace(address, record);
}

void VulkanAccelerationStructureManager::UntrackInputBuffer(VkResourceRecord *record)
{
  if(!m_pDriver->AccelerationStructures() || !IsBackgroundCapturing(m_pDriver->GetState()))
    return;

  SCOPED_LOCK(m_DeviceAddressLock);

  auto it = std::find_if(m_DeviceAddressToRecord.begin(), m_DeviceAddressToRecord.end(),
                         [&](const auto &data) { return data.second == record; });
  if(it != m_DeviceAddressToRecord.end())
    m_DeviceAddressToRecord.erase(it);
}

void VulkanAccelerationStructureManager::CopyInputBuffers(
    VkCommandBuffer commandBuffer, const VkAccelerationStructureBuildGeometryInfoKHR &info,
    const VkAccelerationStructureBuildRangeInfoKHR *buildRange)
{
  VkResourceRecord *cmdRecord = GetRecord(commandBuffer);
  RDCASSERT(cmdRecord);
  if(!cmdRecord)
    return;

  VkDevice device = cmdRecord->cmdInfo->device;

  VkResourceRecord *asRecord = GetRecord(info.dstAccelerationStructure);
  RDCASSERT(asRecord);
  if(!asRecord)
    return;

  if(asRecord->accelerationStructureInfo != NULL)
  {
    SCOPED_LOCK(m_ReadbackMemDeleteLock);
    m_ReadbackMemToDelete.emplace(commandBuffer, asRecord->accelerationStructureInfo);
  }

  VkAccelerationStructureInfo *metadata = new VkAccelerationStructureInfo();
  metadata->device = device;
  metadata->type = info.type;
  metadata->flags = info.flags;
  metadata->accelerationStructure = info.dstAccelerationStructure;

  metadata->geometryData.reserve(info.geometryCount);
  metadata->buildRangeInfo.reserve(info.geometryCount);

  SCOPED_LOCK(m_DeviceAddressLock);
  for(uint32_t i = 0; i < info.geometryCount; ++i)
  {
    // Work out the buffer size needed for each geometry type
    const VkAccelerationStructureGeometryKHR &geometry =
        info.pGeometries != NULL ? info.pGeometries[i] : *(info.ppGeometries[i]);
    const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo = buildRange[i];

    Allocation readbackmem;
    rdcarray<VkBufferMemoryBarrier> barriers;
    barriers.reserve(3);

    switch(geometry.geometryType)
    {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      {
        const VkAccelerationStructureGeometryTrianglesDataKHR &triInfo = geometry.geometry.triangles;

        // Find the associated VkBuffers
        const RecordAndOffset vertexBufferRecord =
            GetDeviceAddressData(triInfo.vertexData.deviceAddress);
        if(!vertexBufferRecord.record)
        {
          RDCERR("Unable to find VkBuffer for vertex data at 0x%llx",
                 triInfo.vertexData.deviceAddress);
          continue;
        }

        RecordAndOffset indexBufferRecord;
        if(triInfo.indexType != VK_INDEX_TYPE_NONE_KHR)
        {
          indexBufferRecord = GetDeviceAddressData(triInfo.indexData.deviceAddress);
          if(!indexBufferRecord.record)
          {
            RDCERR("Unable to find VkBuffer for index data at 0x%llx",
                   triInfo.vertexData.deviceAddress);
            continue;
          }
        }

        RecordAndOffset transformBufferRecord;
        if(triInfo.transformData.deviceAddress != VK_NULL_HANDLE)
        {
          transformBufferRecord = GetDeviceAddressData(triInfo.transformData.deviceAddress);
          if(!transformBufferRecord.record)
          {
            RDCERR("Unable to find VkBuffer for transform data at 0x%llx",
                   triInfo.vertexData.deviceAddress);
            continue;
          }
        }

        // Find the alignment requirements for each type
        VkDeviceSize vertexAlignment = 0;
        VkDeviceSize indexAlignment = 0;
        VkDeviceSize transformAlignment = 0;

        const VkBuffer vertexBuffer =
            ToUnwrappedHandle<VkBuffer>(vertexBufferRecord.record->Resource);

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        if(indexBufferRecord.record != NULL)
          indexBuffer = ToUnwrappedHandle<VkBuffer>(indexBufferRecord.record->Resource);

        VkBuffer transformBuffer = VK_NULL_HANDLE;
        if(transformBufferRecord.record != NULL)
          transformBuffer = ToUnwrappedHandle<VkBuffer>(transformBufferRecord.record->Resource);

        VkDeviceSize vertexSize = 0;
        VkDeviceSize indexSize = 0;
        VkDeviceSize transformSize = 0;

        VkDeviceSize vertexStart = 0;
        VkDeviceSize indexStart = 0;
        VkDeviceSize transformStart = 0;
        {
          VkMemoryRequirements mrq = {};

          // Vertex buffer.  The complexity here is that the rangeInfo members are interpreted
          // differently depending on whether or not index buffers are used
          ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), vertexBuffer, &mrq);
          vertexAlignment = mrq.alignment;
          vertexSize = indexBuffer != VK_NULL_HANDLE
                           ? vertexBufferRecord.record->memSize - vertexBufferRecord.offset
                           : rangeInfo.primitiveCount * 3 * triInfo.vertexStride;
          vertexStart =
              indexBuffer != VK_NULL_HANDLE
                  ? 0
                  : rangeInfo.primitiveOffset + (triInfo.vertexStride * rangeInfo.firstVertex);

          // Index buffer
          if(indexBuffer != VK_NULL_HANDLE)
          {
            ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), indexBuffer, &mrq);
            indexAlignment = mrq.alignment;
            indexSize = rangeInfo.primitiveCount * 3 * IndexTypeSize(triInfo.indexType);
            indexStart = rangeInfo.primitiveOffset;
          }

          // Transform buffer
          if(transformBuffer != VK_NULL_HANDLE)
          {
            ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), transformBuffer, &mrq);
            transformAlignment = mrq.alignment;
            transformSize = sizeof(VkTransformMatrixKHR);
            transformStart = rangeInfo.transformOffset;
          }
        }
        const VkDeviceSize maxAlignment =
            RDCMAX(RDCMAX(vertexAlignment, indexAlignment), transformAlignment);

        // We want to copy the vertex, index, and transform data into one big block so sum the
        // sizes up together
        const VkDeviceSize totalMemSize = AlignUp(vertexSize, vertexAlignment) +
                                          AlignUp(indexSize, indexAlignment) +
                                          AlignUp(transformSize, transformAlignment);

        readbackmem = CreateReadBackMemory(device, totalMemSize, maxAlignment);
        if(readbackmem.mem == VK_NULL_HANDLE)
        {
          RDCERR("Unable to allocate AS triangle input buffer readback memory (size: %u bytes)",
                 totalMemSize);
          continue;
        }

        // Insert copy commands
        VkBufferCopy region = {
            vertexBufferRecord.offset + vertexStart,
            0,
            vertexSize,
        };
        ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), vertexBuffer, readbackmem.buf, 1,
                                       &region);

        if(indexSize != 0)
        {
          region = {
              indexBufferRecord.offset + indexStart,
              AlignUp(vertexSize, vertexAlignment),
              indexSize,
          };
          ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), indexBuffer, readbackmem.buf, 1,
                                         &region);
        }

        if(transformSize != 0)
        {
          region = {
              transformBufferRecord.offset + transformStart,
              AlignUp(vertexSize, vertexAlignment) + AlignUp(indexSize, indexAlignment),
              transformSize,
          };
          ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), transformBuffer, readbackmem.buf, 1,
                                         &region);
        }

        // Prepare the barriers
        barriers = {{
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            vertexBuffer,
            vertexBufferRecord.offset + vertexStart,
            vertexSize,
        }};
        if(indexSize != 0)
        {
          barriers.push_back(barriers.back());
          VkBufferMemoryBarrier &barrier = barriers.back();

          barrier.buffer = indexBuffer;
          barrier.offset = indexBufferRecord.offset + indexStart;
          barrier.size = indexSize;
        }
        if(transformSize != 0)
        {
          barriers.push_back(barriers.back());
          VkBufferMemoryBarrier &barrier = barriers.back();

          barrier.buffer = transformBuffer;
          barrier.offset = transformBufferRecord.offset + transformStart;
          barrier.size = transformSize;
        }

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.readbackMem = readbackmem.mem;
        geoData.memSize = readbackmem.size;
        geoData.memAlignment = maxAlignment;

        geoData.tris.vertexFormat = geometry.geometry.triangles.vertexFormat;
        geoData.tris.vertexStride = geometry.geometry.triangles.vertexStride;
        geoData.tris.maxVertex = geometry.geometry.triangles.maxVertex;
        geoData.tris.indexType = geometry.geometry.triangles.indexType;
        geoData.tris.hasTransformData = transformSize != 0;

        metadata->geometryData.push_back(geoData);

        // Frustratingly rangeInfo.primitiveOffset represents either the offset into the index or
        // vertex buffer depending if indices are in use or not
        VkAccelerationStructureBuildRangeInfoKHR buildData;
        buildData.primitiveCount = rangeInfo.primitiveCount;
        buildData.primitiveOffset =
            indexSize == 0 ? 0 : (uint32_t)AlignUp(vertexSize, vertexAlignment);
        buildData.firstVertex = indexSize == 0 ? 0 : rangeInfo.firstVertex;
        buildData.transformOffset =
            (uint32_t)(AlignUp(vertexSize, vertexAlignment) + AlignUp(indexSize, indexAlignment));

        metadata->buildRangeInfo.push_back(buildData);

        break;
      }
      case VK_GEOMETRY_TYPE_AABBS_KHR:
      {
        const VkAccelerationStructureGeometryAabbsDataKHR &aabbInfo = geometry.geometry.aabbs;

        // Find the associated VkBuffer
        const RecordAndOffset bufferRecord = GetDeviceAddressData(aabbInfo.data.deviceAddress);
        if(!bufferRecord.record)
        {
          RDCERR("Unable to find VkBuffer for AABB data at 0x%llx", aabbInfo.data.deviceAddress);
          continue;
        }

        const VkBuffer buffer = ToUnwrappedHandle<VkBuffer>(bufferRecord.record->Resource);

        const VkDeviceSize size = rangeInfo.primitiveCount * sizeof(VkAabbPositionsKHR);

        // Get the alignment
        VkMemoryRequirements mrq = {};
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), buffer, &mrq);

        // Allocate copy buffer
        readbackmem = CreateReadBackMemory(device, size, mrq.alignment);
        if(readbackmem.mem == VK_NULL_HANDLE)
        {
          RDCERR("Unable to allocate AS AABB input buffer readback memory (size: %u bytes)",
                 mrq.size);
          continue;
        }

        // Insert copy commands
        VkBufferCopy region = {
            bufferRecord.offset + rangeInfo.primitiveOffset,
            0,
            size,
        };
        ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), buffer, readbackmem.buf, 1, &region);

        // Prepare barrier
        barriers = {{
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            buffer,
            bufferRecord.offset + rangeInfo.primitiveOffset,
            size,
        }};

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.readbackMem = readbackmem.mem;
        geoData.memSize = readbackmem.size;
        geoData.memAlignment = mrq.alignment;

        geoData.aabbs.stride = aabbInfo.stride;

        metadata->geometryData.push_back(geoData);

        VkAccelerationStructureBuildRangeInfoKHR buildData = rangeInfo;
        buildData.primitiveOffset = 0;

        metadata->buildRangeInfo.push_back(buildData);

        break;
      }
      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      {
        const VkAccelerationStructureGeometryInstancesDataKHR &instanceInfo =
            geometry.geometry.instances;

        // Find the associated VkBuffer
        const RecordAndOffset bufferRecord = GetDeviceAddressData(instanceInfo.data.deviceAddress);
        if(!bufferRecord.record)
        {
          RDCERR("Unable to find VkBuffer for instance data at 0x%llx",
                 instanceInfo.data.deviceAddress);
          continue;
        }

        const VkBuffer buffer = ToUnwrappedHandle<VkBuffer>(bufferRecord.record->Resource);

        const VkDeviceSize size =
            rangeInfo.primitiveCount * sizeof(VkAccelerationStructureInstanceKHR);

        // Get the alignment
        VkMemoryRequirements mrq = {};
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), buffer, &mrq);

        // Allocate copy buffer
        readbackmem = CreateReadBackMemory(device, size, mrq.alignment);
        if(readbackmem.mem == VK_NULL_HANDLE)
        {
          RDCERR("Unable to allocate AS instance input buffer readback memory (size: %u bytes)",
                 size);
          continue;
        }

        // Insert copy commands
        VkBufferCopy region = {
            bufferRecord.offset + rangeInfo.primitiveOffset,
            0,
            size,
        };
        ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), buffer, readbackmem.buf, 1, &region);

        // Prepare barrier
        barriers = {{
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            buffer,
            bufferRecord.offset + rangeInfo.primitiveOffset,
            size,
        }};

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.readbackMem = readbackmem.mem;
        geoData.memSize = readbackmem.size;
        geoData.memAlignment = mrq.alignment;

        metadata->geometryData.push_back(geoData);

        VkAccelerationStructureBuildRangeInfoKHR buildData = rangeInfo;
        buildData.primitiveOffset = 0;

        metadata->buildRangeInfo.push_back(buildData);

        break;
      }
      default: RDCERR("Unhandled geometry type: %d", geometry.geometryType); return;
    }

    // Insert barriers to block any other commands until the buffers are copied
    if(!barriers.empty())
    {
      ObjDisp(device)->CmdPipelineBarrier(Unwrap(commandBuffer), VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, VK_NULL_HANDLE,
                                          static_cast<uint32_t>(barriers.size()), barriers.data(),
                                          0, VK_NULL_HANDLE);
    }

    // We can schedule buffer deletion now as it isn't needed anymore
    if(readbackmem.buf != VK_NULL_HANDLE)
      m_pDriver->AddPendingObjectCleanup([device, readbackmem]() {
        ObjDisp(device)->DestroyBuffer(Unwrap(device), readbackmem.buf, NULL);
      });
  }

  // Attach the metadata to the AS record
  asRecord->accelerationStructureInfo = metadata;
}

void VulkanAccelerationStructureManager::CopyAccelerationStructure(
    VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR &pInfo)
{
  VkResourceRecord *srcRecord = GetRecord(pInfo.src);
  RDCASSERT(srcRecord != NULL);
  RDCASSERT(srcRecord->accelerationStructureInfo != NULL);

  VkResourceRecord *dstRecord = GetRecord(pInfo.dst);
  RDCASSERT(dstRecord != NULL);

  // If there's existing metadata in the dst, then clean up any copied input buffers before we
  // overwrite it
  SAFE_DELETE(dstRecord->accelerationStructureInfo);

  // We ignore the mode as it can only be a clone or compaction, and we perform the same task
  // regardless
  VkAccelerationStructureInfo *info = new VkAccelerationStructureInfo();
  *info = *srcRecord->accelerationStructureInfo;
  info->accelerationStructure = pInfo.dst;

  // Duplicate the readbackMem.  The better solution would be to refCount the
  // VkAccelerationStructureInfo
  for(VkAccelerationStructureInfo::GeometryData &geom : info->geometryData)
  {
    const Allocation alloc = CreateReadBackMemory(info->device, geom.memSize, geom.memAlignment);

    const VkBuffer srcBuffer = CreateReadBackBuffer(info->device, geom.memSize);
    RDCASSERT(srcBuffer != VK_NULL_HANDLE);

    VkResult vkr =
        ObjDisp(info->device)->BindBufferMemory(Unwrap(info->device), srcBuffer, geom.readbackMem, 0);
    RDCASSERT(vkr == VK_SUCCESS);

    VkBufferCopy region = {
        0,
        0,
        geom.memSize,
    };
    ObjDisp(info->device)->CmdCopyBuffer(Unwrap(commandBuffer), srcBuffer, alloc.buf, 1, &region);

    geom.readbackMem = alloc.mem;
  }

  dstRecord->accelerationStructureInfo = info;
}

VkFence VulkanAccelerationStructureManager::DeleteReadbackMemOnCompletion(uint32_t submitCount,
                                                                          const VkSubmitInfo *pSubmits,
                                                                          VkFence fence)
{
  SCOPED_LOCK(m_ReadbackMemDeleteLock);

  // Find any command buffers we're interested in
  rdcarray<VkAccelerationStructureInfo *> infos;
  for(uint32_t i = 0; i < submitCount; ++i)
  {
    for(uint32_t j = 0; j < pSubmits[i].commandBufferCount; ++j)
    {
      const VkCommandBuffer cmdBuf = pSubmits[i].pCommandBuffers[j];

      const auto rng = m_ReadbackMemToDelete.equal_range(cmdBuf);
      for(auto it = rng.first; it != rng.second; ++it)
        infos.push_back(it->second);

      m_ReadbackMemToDelete.erase(cmdBuf);
    }
  }

  // Nothing to do
  if(infos.empty())
    return VK_NULL_HANDLE;

  // Create a fence if the target hasn't provided one
  VkFence asFence = VK_NULL_HANDLE;
  if(fence == VK_NULL_HANDLE)
  {
    const VkDevice d = m_pDriver->GetDev();
    const VkFenceCreateInfo info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0};

    const VkResult vkr = ObjDisp(d)->CreateFence(Unwrap(d), &info, NULL, &asFence);
    m_pDriver->CheckVkResult(vkr);

    m_DeleteFence.push_back(asFence);
  }

  const VkFence fenceToUse = asFence == VK_NULL_HANDLE ? Unwrap(fence) : asFence;
  m_ReadbackMemDeleteSync.emplace(fenceToUse, std::move(infos));

  if(m_ReadbackDeleteThread == 0 && !m_ReadbackMemDeleteSync.empty())
  {
    m_ExitReadbackDeleteThread = 0;
    m_ReadbackDeleteThread = Threading::CreateThread([this]() {
      while(Atomic::CmpExch32(&m_ExitReadbackDeleteThread, 0, 0) == 0)
      {
        Threading::Sleep(20);

        const VkDevice d = m_pDriver->GetDev();
        if(d == VK_NULL_HANDLE)
          return;

        SCOPED_LOCK(m_ReadbackMemDeleteLock);
        for(auto it = m_ReadbackMemDeleteSync.begin(); it != m_ReadbackMemDeleteSync.end();)
        {
          const VkResult vkr = ObjDisp(d)->GetFenceStatus(Unwrap(d), it->first);
          if(vkr == VK_SUCCESS)
          {
            // Fence signalled so we can delete our readback mems, but check if the resource hasn't
            // been deleted already just in case
            for(VkAccelerationStructureInfo *info : it->second)
            {
              VkResourceRecord *asRecord = GetRecord(info->accelerationStructure);
              if(asRecord != NULL)
                SAFE_DELETE(info);
            }

            if(m_DeleteFence.contains(it->first))
            {
              ObjDisp(d)->DestroyFence(Unwrap(d), it->first, NULL);
              m_DeleteFence.removeOne(it->first);
            }

            it = m_ReadbackMemDeleteSync.erase(it);
            continue;
          }
          ++it;
        }
      }
    });
  }

  return asFence;
}

uint64_t VulkanAccelerationStructureManager::GetSize_InitialState(ResourceId id,
                                                                  const VkInitialContents &initial)
{
  const VkAccelerationStructureInfo *info = initial.accelerationStructureInfo;
  RDCASSERT(info);
  if(!info)
    return 0;

  // You can't just use sizeof(Triangles) due to padding
  const uint64_t triangleSize = sizeof(VkFormat) +        // vertexFormat
                                sizeof(VkDeviceSize) +    // vertexStride
                                sizeof(uint32_t) +        // maxVertex
                                sizeof(VkIndexType) +     // indexType
                                sizeof(bool);             // hasTransformData

  const uint64_t geomDataSize = sizeof(VkGeometryTypeKHR) +     // geometryType
                                sizeof(VkGeometryFlagsKHR) +    // flags
                                sizeof(VkDeviceSize) +          // memSize
                                sizeof(VkDeviceSize) +          // memAlignment
                                triangleSize;                   // Union size

  const uint64_t numGeomData = info->geometryData.size();
  const uint64_t numBuildInfo = info->buildRangeInfo.size();

  // Sum up the metadata size
  const uint64_t metadataSize =
      sizeof(VkAccelerationStructureTypeKHR) +             // type
      sizeof(VkBuildAccelerationStructureFlagsKHR) +       // flags
      sizeof(uint64_t) + (geomDataSize * numGeomData) +    // geometryData
      sizeof(uint64_t) +
      (sizeof(VkAccelerationStructureBuildRangeInfoKHR) * numBuildInfo);    // buildRangeInfo

  // Sum up the input buffer sizes
  uint64_t bufferSize = 0;
  for(const VkAccelerationStructureInfo::GeometryData &geom : info->geometryData)
    bufferSize += sizeof(uint64_t) + geom.memSize + WriteSerialiser::GetChunkAlignment();

  return 128ULL + metadataSize + bufferSize;
}

template <typename SerialiserType>
bool VulkanAccelerationStructureManager::Serialise(SerialiserType &ser, ResourceId id,
                                                   const VkInitialContents *initial,
                                                   CaptureState state)
{
  const VkDeviceSize nonCoherentAtomSize = m_pDriver->GetDeviceProps().limits.nonCoherentAtomSize;

  VkAccelerationStructureInfo *asInfo = initial ? initial->accelerationStructureInfo : NULL;
  if(ser.IsReading())
  {
    asInfo = new VkAccelerationStructureInfo();
  }

  RDCASSERT(asInfo != NULL);
  SERIALISE_ELEMENT(*asInfo).Hidden();

  VkDevice d = !IsStructuredExporting(state) ? m_pDriver->GetDev() : VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  for(VkAccelerationStructureInfo::GeometryData &geomData : asInfo->geometryData)
  {
    Allocation uploadMemory;
    byte *contents = NULL;

    if(ser.IsWriting())
    {
      // The input buffers have already been copied into readable memory, so they just need mapping
      // and serialising
      vkr = ObjDisp(d)->MapMemory(Unwrap(d), geomData.readbackMem, 0, geomData.memSize, 0,
                                  (void **)&contents);
      m_pDriver->CheckVkResult(vkr);

      // invalidate the cpu cache for this memory range to avoid reading stale data
      const VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, geomData.readbackMem, 0, geomData.memSize,
      };

      vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
      m_pDriver->CheckVkResult(vkr);
    }
    else if(IsReplayMode(state) && !ser.IsErrored())
    {
      uploadMemory =
          CreateReplayMemory(MemoryType::Upload, geomData.memSize, geomData.memAlignment,
                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
      if(uploadMemory.mem == VK_NULL_HANDLE)
      {
        RDCERR("Failed to allocate AS build data upload buffer");
        return false;
      }

      m_pDriver->AddPendingObjectCleanup([d, uploadMemory]() {
        ObjDisp(d)->DestroyBuffer(Unwrap(d), uploadMemory.buf, NULL);
        ObjDisp(d)->FreeMemory(Unwrap(d), uploadMemory.mem, NULL);
      });

      vkr = ObjDisp(d)->MapMemory(Unwrap(d), uploadMemory.mem, 0,
                                  AlignUp(geomData.memSize, nonCoherentAtomSize), 0,
                                  (void **)&contents);
      m_pDriver->CheckVkResult(vkr);

      if(!contents)
      {
        RDCERR("Manually reporting failed memory map");
        m_pDriver->CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
        return false;
      }

      if(vkr != VK_SUCCESS)
        return false;
    }

    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("AS Input"_lit, contents, geomData.memSize, SerialiserFlags::NoFlags).Hidden();

    if(!IsStructuredExporting(state))
    {
      if(uploadMemory.mem != VK_NULL_HANDLE)
      {
        // first ensure we flush the writes from the cpu to gpu memory
        const VkMappedMemoryRange range = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,          NULL, uploadMemory.mem, 0,
            AlignUp(geomData.memSize, nonCoherentAtomSize),
        };

        vkr = ObjDisp(d)->FlushMappedMemoryRanges(Unwrap(d), 1, &range);
        m_pDriver->CheckVkResult(vkr);

        ObjDisp(d)->UnmapMemory(Unwrap(d), uploadMemory.mem);

        // Allocate GPU memory and copy the AS input upload data into it
        const VkBufferCreateInfo gpuBufInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            NULL,
            0,
            geomData.memSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        };

        VkBuffer gpuBuf = VK_NULL_HANDLE;
        vkr = m_pDriver->vkCreateBuffer(d, &gpuBufInfo, NULL, &gpuBuf);
        m_pDriver->CheckVkResult(vkr);

        VkMemoryRequirements mrq = {};
        m_pDriver->vkGetBufferMemoryRequirements(d, gpuBuf, &mrq);

        mrq.alignment = RDCMAX(mrq.alignment, geomData.memAlignment);

        const MemoryAllocation gpuMemory = m_pDriver->AllocateMemoryForResource(
            true, mrq, MemoryScope::InitialContents, MemoryType::GPULocal);
        if(gpuMemory.mem == VK_NULL_HANDLE)
        {
          RDCERR("Failed to allocate AS build data GPU buffer");
          return false;
        }

        vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(gpuBuf), Unwrap(gpuMemory.mem),
                                           gpuMemory.offs);
        m_pDriver->CheckVkResult(vkr);

        VkCommandBuffer cmd = m_pDriver->GetInitStateCmd();
        if(cmd == VK_NULL_HANDLE)
        {
          RDCERR("Couldn't acquire command buffer");
          return false;
        }

        VkBufferCopy region = {
            0,
            0,
            AlignUp(geomData.memSize, nonCoherentAtomSize),
        };
        ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), uploadMemory.buf, Unwrap(gpuBuf), 1, &region);

        geomData.replayBuf = gpuBuf;
      }
      else
      {
        ObjDisp(d)->UnmapMemory(Unwrap(d), geomData.readbackMem);
      }
    }
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayMode(state))
  {
    VkInitialContents initialContents;
    initialContents.type = eResAccelerationStructureKHR;
    initialContents.isTLAS = asInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR ||
                             asInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
    initialContents.accelerationStructureInfo = asInfo;

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
  VkAccelerationStructureInfo *asInfo = initial.accelerationStructureInfo;
  RDCASSERT(asInfo);

  rdcarray<VkAccelerationStructureGeometryKHR> asGeomData = asInfo->convertGeometryData();
  RDCASSERT(!asGeomData.empty());
  RDCASSERT(asInfo->geometryData.size() == asGeomData.size());

  const VkDevice d = m_pDriver->GetDev();

  if(!FixUpReplayBDAs(initial.accelerationStructureInfo, asGeomData))
    return;

  // Allocate the scratch buffer which involves working out how big it should be
  VkAccelerationStructureBuildSizesInfoKHR sizeResult = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
      NULL,
  };
  {
    const VkAccelerationStructureBuildGeometryInfoKHR sizeInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        NULL,
        asInfo->type,
        asInfo->flags,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        (uint32_t)asGeomData.size(),
        asGeomData.data(),
        VK_NULL_HANDLE,
    };

    rdcarray<uint32_t> counts;
    counts.reserve(asGeomData.size());
    for(VkAccelerationStructureBuildRangeInfoKHR numPrims : asInfo->buildRangeInfo)
    {
      counts.push_back(numPrims.primitiveCount);
    }

    ObjDisp(d)->GetAccelerationStructureBuildSizesKHR(
        Unwrap(d), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &sizeInfo, counts.data(),
        &sizeResult);
  }

  const VkPhysicalDevice physDev = m_pDriver->GetPhysDev();

  VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
  };
  VkPhysicalDeviceProperties2 asPropsBase = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      &asProps,
  };
  ObjDisp(physDev)->GetPhysicalDeviceProperties2(Unwrap(physDev), &asPropsBase);

  // We serialise the AS builds, so reuse the existing scratch
  if(sizeResult.buildScratchSize > scratch.size || scratch.mem == VK_NULL_HANDLE)
  {
    // Delete the previous
    if(scratch.mem != VK_NULL_HANDLE)
    {
      m_pDriver->AddPendingObjectCleanup([d, tmp = scratch]() {
        ObjDisp(d)->DestroyBuffer(Unwrap(d), tmp.buf, NULL);
        ObjDisp(d)->FreeMemory(Unwrap(d), tmp.mem, NULL);
      });
    }

    scratch = CreateReplayMemory(MemoryType::GPULocal, sizeResult.buildScratchSize,
                                 asProps.minAccelerationStructureScratchOffsetAlignment,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if(scratch.mem == VK_NULL_HANDLE)
    {
      RDCERR("Failed to allocate AS build data scratch buffer");
      return;
    }

    const VkBufferDeviceAddressInfo scratchAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        NULL,
        scratch.buf,
    };

    scratchAddressUnion.deviceAddress =
        ObjDisp(d)->GetBufferDeviceAddressKHR(Unwrap(d), &scratchAddressInfo);
  }

  // Build the AS
  VkCommandBuffer cmd = m_pDriver->GetInitStateCmd();
  if(cmd == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't acquire command buffer");
    return;
  }

  const VkAccelerationStructureKHR wrappedAS =
      m_pDriver->GetResourceManager()->GetCurrentHandle<VkAccelerationStructureKHR>(id);

  const VkAccelerationStructureBuildGeometryInfoKHR asGeomInfo = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      NULL,
      asInfo->type,
      asInfo->flags,
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      VK_NULL_HANDLE,
      Unwrap(wrappedAS),
      (uint32_t)asGeomData.size(),
      asGeomData.data(),
      NULL,
      scratchAddressUnion,
  };

  const VkAccelerationStructureBuildRangeInfoKHR *pBuildInfo = asInfo->buildRangeInfo.data();
  ObjDisp(d)->CmdBuildAccelerationStructuresKHR(Unwrap(cmd), 1, &asGeomInfo, &pBuildInfo);

  // We serialise the AS builds so we can have just a single scratch buffer and reuse it
  m_pDriver->CloseInitStateCmd();
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();
}

VkBuffer VulkanAccelerationStructureManager::CreateReadBackBuffer(VkDevice device, VkDeviceSize size)
{
  VkResult vkr = VK_SUCCESS;

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

  VkBuffer result;
  vkr = ObjDisp(device)->CreateBuffer(Unwrap(device), &bufInfo, NULL, &result);
  if(vkr != VK_SUCCESS)
  {
    RDCERR("Failed to create readback buffer");
    return VK_NULL_HANDLE;
  }

  return result;
}

VulkanAccelerationStructureManager::Allocation VulkanAccelerationStructureManager::CreateReadBackMemory(
    VkDevice device, VkDeviceSize size, VkDeviceSize alignment)
{
  VkResult vkr = VK_SUCCESS;

  Allocation readbackmem;
  readbackmem.buf = CreateReadBackBuffer(device, size);
  if(readbackmem.buf == VK_NULL_HANDLE)
    return {};

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

VulkanAccelerationStructureManager::Allocation VulkanAccelerationStructureManager::CreateReplayMemory(
    MemoryType memType, VkDeviceSize size, VkDeviceSize alignment, VkBufferUsageFlags extraUsageFlags)
{
  const VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | extraUsageFlags,
  };

  const VkDevice d = m_pDriver->GetDev();
  VkBuffer buf = VK_NULL_HANDLE;

  VkResult vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &buf);
  m_pDriver->CheckVkResult(vkr);

  VkMemoryRequirements mrq = {};
  ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), buf, &mrq);

  if(alignment != 0)
    mrq.alignment = RDCMAX(mrq.alignment, alignment);

  uint32_t memoryTypeIndex = 0;
  switch(memType)
  {
    case MemoryType::Upload:
      memoryTypeIndex = m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits);
      break;
    case MemoryType::GPULocal:
      memoryTypeIndex = m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits);
      break;
    case MemoryType::Readback:
      memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
      break;
  }

  VkMemoryAllocateFlagsInfo flagsInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      NULL,
      VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };
  VkMemoryAllocateInfo info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      &flagsInfo,
      size,
      memoryTypeIndex,
  };

  VkDeviceMemory mem = VK_NULL_HANDLE;
  vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &info, NULL, &mem);
  m_pDriver->CheckVkResult(vkr);

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), buf, mem, 0);
  m_pDriver->CheckVkResult(vkr);

  return {mem, size, buf};
}

bool VulkanAccelerationStructureManager::FixUpReplayBDAs(
    VkAccelerationStructureInfo *asInfo, rdcarray<VkAccelerationStructureGeometryKHR> &geoms)
{
  RDCASSERT(asInfo);
  RDCASSERT(asInfo->geometryData.size() == geoms.size());

  const VkDevice d = m_pDriver->GetDev();

  for(uint64_t i = 0; i < geoms.size(); ++i)
  {
    VkBuffer buf = asInfo->geometryData[i].replayBuf;
    VkAccelerationStructureGeometryKHR &geom = geoms[i];

    const VkBufferDeviceAddressInfo addrInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, NULL,
                                                Unwrap(buf)};
    const VkDeviceAddress bufAddr = ObjDisp(d)->GetBufferDeviceAddressKHR(Unwrap(d), &addrInfo);

    switch(geom.geometryType)
    {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      {
        geom.geometry.triangles.vertexData.deviceAddress = bufAddr;
        geom.geometry.triangles.indexData.deviceAddress = bufAddr;
        geom.geometry.triangles.transformData.deviceAddress = bufAddr;
        break;
      }
      case VK_GEOMETRY_TYPE_AABBS_KHR:
      {
        geom.geometry.aabbs.data.deviceAddress = bufAddr;
        break;
      }
      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      {
        geom.geometry.instances.data.deviceAddress = bufAddr;
        break;
      }
      default: RDCERR("Unhandled geometry type: %d", geom.geometryType); return false;
    }
  }

  return true;
}

VulkanAccelerationStructureManager::RecordAndOffset VulkanAccelerationStructureManager::GetDeviceAddressData(
    VkDeviceAddress address) const
{
  if(m_DeviceAddressToRecord.empty())
    return {};

  auto it =
      std::lower_bound(m_DeviceAddressToRecord.begin(), m_DeviceAddressToRecord.end(), address,
                       [](const auto &pair, VkDeviceAddress addr) { return pair.first < addr; });
  if(it != m_DeviceAddressToRecord.end())
  {
    // It's a match!
    if(it->first == address)
      return {it->second, it->first, 0};

    // The address is before the first entry starts
    if(it == m_DeviceAddressToRecord.begin())
      return {};
  }

  // The address is inbetween two entries, so work out the offset and make sure it's less than
  // the buffer size
  auto prevIt = std::prev(it);
  const VkDeviceSize offset = address - prevIt->first;
  if(offset >= prevIt->second->memSize)
    return {};

  return {prevIt->second, prevIt->first, offset};
}
