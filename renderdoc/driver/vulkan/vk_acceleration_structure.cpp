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
#include "limits"
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

DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData::Triangles);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData::Aabbs);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo::GeometryData);
DECLARE_STRINGISE_TYPE(VkAccelerationStructureInfo);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData::Triangles &el)
{
  SERIALISE_MEMBER(vertexFormat);
  SERIALISE_MEMBER(vertexStride);
  SERIALISE_MEMBER(maxVertex);
  SERIALISE_MEMBER(indexType);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData::Triangles);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData::Aabbs &el)
{
  SERIALISE_MEMBER(stride);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData::Aabbs);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo::GeometryData &el)
{
  SERIALISE_MEMBER(geometryType);
  SERIALISE_MEMBER_TYPED(VkGeometryFlagBitsKHR, flags).TypedAs("VkGeometryFlagsKHR"_lit);

  SERIALISE_MEMBER(tris);
  SERIALISE_MEMBER(aabbs);

  SERIALISE_MEMBER(buildRangeInfo);
  SERIALISE_MEMBER(memOffset);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo::GeometryData);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, VkAccelerationStructureInfo &el)
{
  SERIALISE_MEMBER(type);
  SERIALISE_MEMBER_TYPED(VkBuildAccelerationStructureFlagBitsKHR, flags)
      .TypedAs("VkBuildAccelerationStructureFlagsKHR"_lit);
  SERIALISE_MEMBER(geometryData);
  SERIALISE_MEMBER(memSize);
}
INSTANTIATE_SERIALISE_TYPE(VkAccelerationStructureInfo);

uint64_t VkAccelerationStructureInfo::GeometryData::GetSerialisedSize() const
{
  return sizeof(GeometryData);
}

VkAccelerationStructureInfo::~VkAccelerationStructureInfo()
{
  if(readbackMem != VK_NULL_HANDLE)
    ObjDisp(device)->FreeMemory(Unwrap(device), readbackMem, NULL);
}

void VkAccelerationStructureInfo::Release()
{
  const int32_t ref = Atomic::Dec32(&refCount);
  RDCASSERT(ref >= 0);
  if(ref <= 0)
    delete this;
}

uint64_t VkAccelerationStructureInfo::GetSerialisedSize() const
{
  uint64_t geomDataSize = 0;
  for(const GeometryData &geoData : geometryData)
    geomDataSize += geoData.GetSerialisedSize();

  const uint64_t size = sizeof(VkAccelerationStructureTypeKHR) +          // type
                        sizeof(VkBuildAccelerationStructureFlagsKHR) +    // flags
                        sizeof(uint64_t) + geomDataSize;                  // geometryData;

  // Add the readbackmem buffer sizes
  const uint64_t bufferSize = sizeof(uint64_t) + memSize + WriteSerialiser::GetChunkAlignment();

  return size + bufferSize;
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
        // We'll write the offset into buffer address so when FixUpReplayBDAs is called, the real
        // base address is just added on
        VkDeviceOrHostAddressConstKHR vData;
        vData.deviceAddress = g.memOffset;

        VkDeviceOrHostAddressConstKHR iData;
        iData.deviceAddress = g.memOffset;

        // vkGetAccelerationStructureBuildSizesKHR just checks if the transform BDA is non-null,
        // so fudge that here
        VkDeviceOrHostAddressConstKHR tData;
        tData.deviceAddress = g.buildRangeInfo.transformOffset
                                  ? g.memOffset
                                  : std::numeric_limits<VkDeviceAddress>::max();

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
        aData.deviceAddress = g.memOffset;

        geoUnion.aabbs = {
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
        iData.deviceAddress = g.memOffset;

        geoUnion.instances = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            NULL,
            false,
            iData,
        };
        break;
      }
      default: RDCERR("Unhandled geometry type: %d", g.geometryType); return {};
    }

    result.push_back({VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, NULL, g.geometryType,
                      geoUnion, g.flags});
  }

  return result;
}

rdcarray<VkAccelerationStructureBuildRangeInfoKHR> VkAccelerationStructureInfo::getBuildRanges() const
{
  rdcarray<VkAccelerationStructureBuildRangeInfoKHR> result;
  result.reserve(geometryData.size());

  for(const GeometryData &geom : geometryData)
    result.push_back(geom.buildRangeInfo);

  return result;
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

    VkBufferCopy region;

  private:
    VkDeviceSize start = 0;
  };

  VkDeviceSize currentDstOffset = 0;
  rdcarray<BufferData> inputBuffersData;

  for(uint32_t i = 0; i < info.geometryCount; ++i)
  {
    // Work out the buffer size needed for each geometry type
    const VkAccelerationStructureGeometryKHR &geometry =
        info.pGeometries != NULL ? info.pGeometries[i] : *(info.ppGeometries[i]);
    const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo = buildRange[i];

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

        // Gather the buffer requirements for each type
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

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.memOffset = currentDstOffset;

        geoData.tris.vertexFormat = geometry.geometry.triangles.vertexFormat;
        geoData.tris.vertexStride = geometry.geometry.triangles.vertexStride;
        geoData.tris.maxVertex = geometry.geometry.triangles.maxVertex;
        geoData.tris.indexType = geometry.geometry.triangles.indexType;

        // Frustratingly rangeInfo.primitiveOffset represents either the offset into the index or
        // vertex buffer depending if indices are in use or not
        VkAccelerationStructureBuildRangeInfoKHR &buildData = geoData.buildRangeInfo;
        buildData.primitiveCount = rangeInfo.primitiveCount;
        buildData.primitiveOffset = 0;
        buildData.firstVertex = 0;
        buildData.transformOffset = 0;

        // Store the data and update the current destinaton offset
        vertexData.region = {
            vertexData.GetReadPosition(),
            currentDstOffset,
            vertexData.size,
        };

        inputBuffersData.push_back(vertexData);
        currentDstOffset += AlignUp(vertexData.size, vertexData.alignment);

        if(indexData)
        {
          // The index primitiveOffset has its own alignment requirements
          buildData.primitiveOffset = (uint32_t)(currentDstOffset - geoData.memOffset);
          const uint32_t primOffsetAlign =
              AlignUp(buildData.primitiveOffset, (uint32_t)IndexTypeSize(triInfo.indexType)) -
              buildData.primitiveOffset;
          buildData.primitiveOffset += primOffsetAlign;
          currentDstOffset += primOffsetAlign;

          buildData.firstVertex = rangeInfo.firstVertex;

          indexData.region = {
              indexData.GetReadPosition(),
              currentDstOffset,
              indexData.size,
          };

          inputBuffersData.push_back(indexData);
          currentDstOffset += AlignUp(indexData.size, indexData.alignment);
        }
        if(transformData)
        {
          // The transform primitiveOffset has its own alignment requirements
          buildData.transformOffset = (uint32_t)(currentDstOffset - geoData.memOffset);
          const uint32_t primOffsetAlign =
              AlignUp(buildData.transformOffset, (uint32_t)16) - buildData.transformOffset;
          buildData.transformOffset += primOffsetAlign;
          currentDstOffset += primOffsetAlign;

          transformData.region = {
              transformData.GetReadPosition(),
              currentDstOffset,
              transformData.size,
          };

          inputBuffersData.push_back(transformData);
          currentDstOffset += AlignUp(transformData.size, transformData.alignment);
        }

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

        // Insert copy commands
        data.region = {
            data.GetReadPosition(),
            currentDstOffset,
            data.size,
        };

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.memOffset = currentDstOffset;

        geoData.aabbs.stride = aabbInfo.stride;

        geoData.buildRangeInfo = rangeInfo;
        geoData.buildRangeInfo.primitiveOffset = 0;

        metadata->geometryData.push_back(geoData);

        currentDstOffset += AlignUp(data.size, mrq.alignment);
        inputBuffersData.push_back(data);

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

        // Insert copy commands
        data.region = {
            data.GetReadPosition(),
            currentDstOffset,
            data.size,
        };

        // Store the metadata
        VkAccelerationStructureInfo::GeometryData geoData;
        geoData.geometryType = geometry.geometryType;
        geoData.flags = geometry.flags;
        geoData.memOffset = currentDstOffset;

        geoData.buildRangeInfo = rangeInfo;
        geoData.buildRangeInfo.primitiveOffset = 0;

        metadata->geometryData.push_back(geoData);

        currentDstOffset += AlignUp(data.size, mrq.alignment);
        inputBuffersData.push_back(data);

        break;
      }
      default: RDCERR("Unhandled geometry type: %d", geometry.geometryType); continue;
    }
  }

  if(currentDstOffset == 0)
  {
    RDCWARN("Cannot copy empty AS input buffers, ignoring");
    return {};
  }

  // Allocate the required memory block
  Allocation readbackmem = CreateReadBackMemory(device, currentDstOffset);
  if(readbackmem.mem == VK_NULL_HANDLE)
  {
    RDCERR("Unable to allocate AS input buffer readback memory (size: %u bytes)", currentDstOffset);
    return {};
  }

  metadata->readbackMem = readbackmem.mem;
  metadata->memSize = currentDstOffset;

  // Queue the copying
  for(const BufferData &bufData : inputBuffersData)
    ObjDisp(device)->CmdCopyBuffer(Unwrap(commandBuffer), bufData.buf, readbackmem.buf, 1,
                                   &bufData.region);

  // Make sure nothing writes to our source buffers before we finish copying them
  VkMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_MEMORY_WRITE_BIT,
  };
  ObjDisp(device)->CmdPipelineBarrier(Unwrap(commandBuffer), VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1, &barrier, 0,
                                      VK_NULL_HANDLE, 0, VK_NULL_HANDLE);

  // We can schedule buffer deletion now as it isn't needed anymore
  cmdRecord->cmdInfo->pendingSubmissionCompleteCallbacks->callbacks.push_back(
      [device, buffer = readbackmem.buf]() {
        ObjDisp(device)->DestroyBuffer(Unwrap(device), buffer, NULL);
      });

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

uint64_t VulkanAccelerationStructureManager::GetSize_InitialState(ResourceId id,
                                                                  const VkInitialContents &initial)
{
  const uint64_t infoSize = initial.accelerationStructureInfo->GetSerialisedSize();
  const uint64_t serialisedASSize =
      (sizeof(uint64_t) * 2) + initial.mem.size + WriteSerialiser::GetChunkAlignment();

  return 128ULL + infoSize + serialisedASSize;
}

template <typename SerialiserType>
bool VulkanAccelerationStructureManager::Serialise(SerialiserType &ser, ResourceId id,
                                                   const VkInitialContents *initial,
                                                   CaptureState state)
{
  VkDevice d = !IsStructuredExporting(state) ? m_pDriver->GetDev() : VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  byte *contents = NULL;

  if(ser.IsWriting())
  {
    VkAccelerationStructureInfo *asInfo = initial->accelerationStructureInfo;
    RDCASSERT(asInfo != NULL);
    SERIALISE_ELEMENT(*asInfo).Hidden();

    RDCASSERT(asInfo->readbackMem != VK_NULL_HANDLE);

    // The input buffers have already been copied into readable memory, so they just need
    // mapping and serialising
    vkr = ObjDisp(d)->MapMemory(Unwrap(d), asInfo->readbackMem, 0, asInfo->memSize, 0,
                                (void **)&contents);
    CHECK_VKR(m_pDriver, vkr);

    // invalidate the cpu cache for this memory range to avoid reading stale data
    const VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, asInfo->readbackMem, 0, asInfo->memSize,
    };
    vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
    CHECK_VKR(m_pDriver, vkr);

    ser.Serialise("AS Input"_lit, contents, asInfo->memSize, SerialiserFlags::NoFlags).Hidden();

    ObjDisp(d)->UnmapMemory(Unwrap(d), asInfo->readbackMem);
  }
  else
  {
    const VkDeviceSize nonCoherentAtomSize = m_pDriver->GetDeviceProps().limits.nonCoherentAtomSize;

    VkAccelerationStructureInfo *asInfo = new VkAccelerationStructureInfo();
    SERIALISE_ELEMENT(*asInfo).Hidden();

    Allocation uploadMemory;

    if(IsReplayMode(state) && !ser.IsErrored())
    {
      uploadMemory =
          CreateReplayMemory(MemoryType::Upload, asInfo->memSize,
                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
      if(uploadMemory.mem == VK_NULL_HANDLE)
      {
        RDCERR("Failed to allocate AS build data upload buffer");
        return false;
      }

      vkr = ObjDisp(d)->MapMemory(Unwrap(d), uploadMemory.mem, 0,
                                  AlignUp(asInfo->memSize, nonCoherentAtomSize), 0,
                                  (void **)&contents);
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
    ser.Serialise("AS Input"_lit, contents, asInfo->memSize, SerialiserFlags::NoFlags).Hidden();

    if(!IsStructuredExporting(state) && uploadMemory.mem != VK_NULL_HANDLE)
    {
      // first ensure we flush the writes from the cpu to gpu memory
      const VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,    //
          NULL,
          uploadMemory.mem,
          0,
          AlignUp(asInfo->memSize, nonCoherentAtomSize),
      };
      vkr = ObjDisp(d)->FlushMappedMemoryRanges(Unwrap(d), 1, &range);
      CHECK_VKR(m_pDriver, vkr);

      ObjDisp(d)->UnmapMemory(Unwrap(d), uploadMemory.mem);

      asInfo->uploadMem = uploadMemory.mem;
      asInfo->uploadBuf = uploadMemory.buf;
    }

    SERIALISE_CHECK_READ_ERRORS();

    if(IsReplayMode(state))
    {
      VkInitialContents initialContents;
      initialContents.type = eResAccelerationStructureKHR;
      initialContents.accelerationStructureInfo = asInfo;

      m_pDriver->GetResourceManager()->SetInitialContents(id, initialContents);
    }
    else
    {
      asInfo->Release();
    }
  }

  return true;
}

template bool VulkanAccelerationStructureManager::Serialise(WriteSerialiser &ser, ResourceId id,
                                                            const VkInitialContents *initial,
                                                            CaptureState state);
template bool VulkanAccelerationStructureManager::Serialise(ReadSerialiser &ser, ResourceId id,
                                                            const VkInitialContents *initial,
                                                            CaptureState state);

void VulkanAccelerationStructureManager::Apply(ResourceId id, VkInitialContents &initial)
{
  const VkAccelerationStructureKHR wrappedAS =
      m_pDriver->GetResourceManager()->GetCurrentHandle<VkAccelerationStructureKHR>(id);
  VkAccelerationStructureInfo *asInfo = initial.accelerationStructureInfo;
  RDCASSERT(asInfo);

  VkCommandBuffer cmd;
  const VkDevice d = m_pDriver->GetDev();

  // If our 'base' AS has not been created yet, build it now
  if(asInfo->replayAS == VK_NULL_HANDLE)
  {
    rdcarray<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos = asInfo->getBuildRanges();
    rdcarray<VkAccelerationStructureGeometryKHR> asGeomData = asInfo->convertGeometryData();
    RDCASSERT(!asGeomData.empty());
    RDCASSERT(asInfo->geometryData.size() == asGeomData.size());

    if(!FixUpReplayBDAs(asInfo, asGeomData))
      return;

    // Allocate the scratch buffer which involves working out how big it should be
    VkAccelerationStructureBuildSizesInfoKHR sizeResult = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
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
      for(VkAccelerationStructureBuildRangeInfoKHR numPrims : buildRangeInfos)
        counts.push_back(numPrims.primitiveCount);

      ObjDisp(d)->GetAccelerationStructureBuildSizesKHR(
          Unwrap(d), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &sizeInfo, counts.data(),
          &sizeResult);
    }
    UpdateScratch(sizeResult.buildScratchSize);

    cmd = m_pDriver->GetInitStateCmd();
    if(cmd == VK_NULL_HANDLE)
    {
      RDCERR("Couldn't acquire command buffer");
      return;
    }

    // Create the base AS
    const VkBufferCreateInfo gpuBufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        sizeResult.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
    };

    VkBuffer asBuf = VK_NULL_HANDLE;
    VkResult vkr = m_pDriver->vkCreateBuffer(d, &gpuBufInfo, NULL, &asBuf);
    CHECK_VKR(m_pDriver, vkr);

    VkMemoryRequirements mrq = {};
    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(asBuf), &mrq);
    mrq.alignment = AlignUp(mrq.alignment, asBufferAlignment);

    const MemoryAllocation asMemory = m_pDriver->AllocateMemoryForResource(
        true, mrq, MemoryScope::InitialContents, MemoryType::GPULocal);
    vkr = m_pDriver->vkBindBufferMemory(d, asBuf, asMemory.mem, asMemory.offs);
    CHECK_VKR(m_pDriver, vkr);

    const VkAccelerationStructureCreateInfoKHR asCreateInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        NULL,
        0,
        asBuf,
        0,
        sizeResult.accelerationStructureSize,
        asInfo->type,
        0x0,
    };
    m_pDriver->vkCreateAccelerationStructureKHR(d, &asCreateInfo, NULL, &asInfo->replayAS);

    // Build the AS
    const VkAccelerationStructureBuildGeometryInfoKHR asGeomInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        NULL,
        asInfo->type,
        asInfo->flags,
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        VK_NULL_HANDLE,
        Unwrap(asInfo->replayAS),
        (uint32_t)asGeomData.size(),
        asGeomData.data(),
        NULL,
        scratchAddressUnion,
    };

    const VkAccelerationStructureBuildRangeInfoKHR *pBuildInfo = buildRangeInfos.data();
    ObjDisp(d)->CmdBuildAccelerationStructuresKHR(Unwrap(cmd), 1, &asGeomInfo, &pBuildInfo);

    m_pDriver->AddPendingObjectCleanup(
        [d, uploadMem = asInfo->uploadMem, uploadBuf = asInfo->uploadBuf]() {
          ObjDisp(d)->DestroyBuffer(Unwrap(d), uploadBuf, NULL);
          ObjDisp(d)->FreeMemory(Unwrap(d), uploadMem, NULL);
        });

    // Make sure the AS builds are serialised as the scratch mem is shared
    VkMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
    };
    ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd),
                                   VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                   VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
                                   &barrier, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
  }

  cmd = m_pDriver->GetInitStateCmd();
  if(cmd == VK_NULL_HANDLE)
  {
    RDCERR("Couldn't acquire command buffer");
    return;
  }

  // Copy the base AS to the captured one to reset it
  const VkCopyAccelerationStructureInfoKHR asCopyInfo = {
      VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
      NULL,
      Unwrap(asInfo->replayAS),
      Unwrap(wrappedAS),
      VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR,
  };
  ObjDisp(d)->CmdCopyAccelerationStructureKHR(Unwrap(cmd), &asCopyInfo);

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

VulkanAccelerationStructureManager::Allocation VulkanAccelerationStructureManager::CreateReplayMemory(
    MemoryType memType, VkDeviceSize size, VkBufferUsageFlags extraUsageFlags)
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

  Allocation result;
  result.size = size;

  VkResult vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &result.buf);
  CHECK_VKR(m_pDriver, vkr);

  VkMemoryRequirements mrq = {};
  ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), result.buf, &mrq);

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

  vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &info, NULL, &result.mem);
  CHECK_VKR(m_pDriver, vkr);

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), result.buf, result.mem, 0);
  CHECK_VKR(m_pDriver, vkr);

  return result;
}

bool VulkanAccelerationStructureManager::FixUpReplayBDAs(
    VkAccelerationStructureInfo *asInfo, rdcarray<VkAccelerationStructureGeometryKHR> &geoms)
{
  RDCASSERT(asInfo);
  RDCASSERT(asInfo->geometryData.size() == geoms.size());

  const VkDevice d = m_pDriver->GetDev();

  const VkBufferDeviceAddressInfo addrInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, NULL,
                                              asInfo->uploadBuf};
  const VkDeviceAddress bufAddr = ObjDisp(d)->GetBufferDeviceAddressKHR(Unwrap(d), &addrInfo);

  for(size_t i = 0; i < geoms.size(); ++i)
  {
    VkAccelerationStructureGeometryKHR &geom = geoms[i];
    switch(geom.geometryType)
    {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      {
        VkAccelerationStructureGeometryTrianglesDataKHR &tri = geom.geometry.triangles;

        tri.vertexData.deviceAddress += bufAddr;

        if(tri.indexType != VK_INDEX_TYPE_NONE_KHR)
          tri.indexData.deviceAddress += bufAddr;

        if(tri.transformData.deviceAddress != std::numeric_limits<VkDeviceAddress>::max())
          tri.transformData.deviceAddress += bufAddr;
        else
          tri.transformData.deviceAddress = 0x0;

        break;
      }
      case VK_GEOMETRY_TYPE_AABBS_KHR:
      {
        geom.geometry.aabbs.data.deviceAddress += bufAddr;
        break;
      }
      case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      {
        geom.geometry.instances.data.deviceAddress += bufAddr;
        break;
      }
      default: RDCERR("Unhandled geometry type: %d", geom.geometryType); return false;
    }
  }

  return true;
}

void VulkanAccelerationStructureManager::UpdateScratch(VkDeviceSize requiredSize)
{
  const VkDevice d = m_pDriver->GetDev();
  const VkPhysicalDevice physDev = m_pDriver->GetPhysDev();

  VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
  };
  VkPhysicalDeviceProperties2 asPropsBase = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      &asProps,
  };
  ObjDisp(physDev)->GetPhysicalDeviceProperties2(Unwrap(physDev), &asPropsBase);

  requiredSize =
      AlignUp(requiredSize, (VkDeviceSize)asProps.minAccelerationStructureScratchOffsetAlignment);

  // We serialise the AS builds, so reuse the existing scratch
  if(requiredSize > scratch.size || scratch.mem == VK_NULL_HANDLE)
  {
    // Delete the previous
    if(scratch.mem != VK_NULL_HANDLE)
    {
      m_pDriver->AddPendingObjectCleanup([d, tmp = scratch]() {
        ObjDisp(d)->DestroyBuffer(Unwrap(d), tmp.buf, NULL);
        ObjDisp(d)->FreeMemory(Unwrap(d), tmp.mem, NULL);
      });

      RDCDEBUG("AS build shared scratch changed to size %llu, flushing", requiredSize);
      m_pDriver->CloseInitStateCmd();
      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();
    }

    scratch =
        CreateReplayMemory(MemoryType::GPULocal, requiredSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
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
