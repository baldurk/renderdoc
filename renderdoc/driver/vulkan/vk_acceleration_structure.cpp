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

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

namespace
{
// Although the serialised data is implementation-defined in general, the header is defined:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap37.html#vkCmdCopyAccelerationStructureToMemoryKHR
constexpr std::size_t handleCountOffset = VK_UUID_SIZE + VK_UUID_SIZE + 8 + 8;
constexpr VkDeviceSize handleCountSize = 8;

// Spec says VkCopyAccelerationStructureToMemoryInfoKHR::dst::deviceAddress must be 256 bytes aligned
constexpr VkDeviceSize asBufferAlignment = 256;
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
  m_pDriver->CheckVkResult(vkr);

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
  m_pDriver->CheckVkResult(vkr);

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
    m_pDriver->CheckVkResult(vkr);

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
    m_pDriver->CheckVkResult(vkr);
  }

  // invalidate the cpu cache for this memory range to avoid reading stale data
  const VkMappedMemoryRange range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, Unwrap(readbackmem.mem), readbackmem.offs, size,
  };
  vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
  m_pDriver->CheckVkResult(vkr);

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
      m_pDriver->CheckVkResult(vkr);

      // invalidate the cpu cache for this memory range to avoid reading stale data
      const VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, Unwrap(mappedMem.mem), mappedMem.offs, size,
      };

      vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
      m_pDriver->CheckVkResult(vkr);
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
    m_pDriver->CheckVkResult(vkr);

    VkMemoryRequirements mrq = {};
    m_pDriver->vkGetBufferMemoryRequirements(d, uploadBuf, &mrq);

    mrq.alignment = RDCMAX(mrq.alignment, asBufferAlignment);

    uploadMemory = m_pDriver->AllocateMemoryForResource(true, mrq, MemoryScope::InitialContents,
                                                        MemoryType::Upload);

    if(uploadMemory.mem == VK_NULL_HANDLE)
      return false;

    vkr = m_pDriver->vkBindBufferMemory(d, uploadBuf, uploadMemory.mem, uploadMemory.offs);
    m_pDriver->CheckVkResult(vkr);

    mappedMem = uploadMemory;

    vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs,
                                AlignUp(mappedMem.size, nonCoherentAtomSize), 0, (void **)&contents);
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
      m_pDriver->CheckVkResult(vkr);

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
    m_pDriver->CheckVkResult(vkr);

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

VkDeviceSize VulkanAccelerationStructureManager::SerialisedASSize(VkAccelerationStructureKHR unwrappedAs)
{
  VkDevice d = m_pDriver->GetDev();

  // Create query pool
  VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  info.queryCount = 1;
  info.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR;

  VkQueryPool pool;
  VkResult vkr = ObjDisp(d)->CreateQueryPool(Unwrap(d), &info, NULL, &pool);
  m_pDriver->CheckVkResult(vkr);

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
  m_pDriver->CheckVkResult(vkr);

  // Clean up
  ObjDisp(d)->DestroyQueryPool(Unwrap(d), pool, NULL);

  return size;
}
