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

#include <algorithm>
#include "../vk_core.h"
#include "../vk_debug.h"
#include "core/settings.h"

RDOC_CONFIG(bool, Vulkan_GPUReadbackDeviceLocal, true,
            "When reading back mapped device-local memory, use a GPU copy "
            "instead of a CPU side comparison directly to mapped memory.");

/************************************************************************
 *
 * Mapping is simpler in Vulkan, at least in concept, but that comes with
 * some restrictions/assumptions about behaviour or performance
 * guarantees.
 *
 * In general we make a distinction between coherent and non-coherent
 * memory, and then also consider persistent maps vs non-persistent maps.
 * (Important note - there is no API concept of persistent maps, any map
 * can be persistent, and we must handle this).
 *
 * For persistent coherent maps we have two options:
 * - pass an intercepted buffer back to the application, whenever any
 *   changes could be GPU-visible (at least every QueueSubmit), diff the
 *   buffer and memcpy to the real pointer & serialise it if capturing.
 * - pass the real mapped pointer back to the application. Ignore it
 *   until capturing, then do readback on the mapped pointer and
 *   diff, serialise any changes.
 *
 * For persistent non-coherent maps again we have two options:
 * - pass an intercepted buffer back to the application. At any Flush()
 *   call copy the flushed region over to the real buffer and if
 *   capturing then serialise it.
 * - pass the real mapped pointer back to the application. Ignore it
 *   until capturing, then serialise out any regions that are Flush()'d
 *   by reading back from the mapped pointer.
 *
 * Now consider transient (non-persistent) maps.
 *
 * For transient coherent maps:
 * - pass an intercepted buffer back to the application, ensuring it has
 *   the correct current contents. Once unmapped, copy the contents to
 *   the real pointer and save if capturing.
 * - return the real mapped pointer, and readback & save the contents on
 *   unmap if capturing
 *
 * For transient non-coherent maps:
 * - pass back an intercepted buffer, again ensuring it has the correct
 *   current contents, and for each Flush() copy the contents to the
 *   real pointer and save if capturing.
 * - return the real mapped pointer, and readback & save the contents on
 *   each flush if capturing.
 *
 * Note several things:
 *
 * The choices in each case are: Intercept & manage, vs. Lazily readback.
 *
 * We do not have a completely free choice. I.e. we can choose our
 * behaviour based on coherency, but not on persistent vs. transient as
 * we have no way to know whether any map we see will be persistent or
 * not.
 *
 * In the transient case we must ensure the correct contents are in an
 * intercepted buffer before returning to the application. Either to
 * ensure the copy to real doesn't upload garbage data, or to ensure a
 * diff to determine modified range is accurate. This is technically
 * required for persistent maps also, but informally we think of a
 * persistent map as from the beginning of the memory's lifetime so
 * there are no previous contents (as above though, we cannot truly
 * differentiate between transient and persistent maps).
 *
 * The essential tradeoff: overhead of managing intercepted buffer
 * against potential cost of reading back from mapped pointer. The cost
 * of reading back from the mapped pointer is essentially unknown. In
 * all likelihood it will not be as cheap as reading back from a locally
 * allocated intercepted buffer, but it might not be that bad. If the
 * cost is low enough for mapped pointer readbacks then it's definitely
 * better to do that, as it's very simple to implement and maintain
 * (no complex bookkeeping of buffers) and we only pay this cost during
 * frame capture, which has a looser performance requirement anyway.
 *
 * Note that the primary difficulty with intercepted buffers is ensuring
 * they stay in sync and have the correct contents at all times. This
 * must be done without readbacks otherwise there is no benefit. Even a
 * DMA to a readback friendly memory type means a GPU sync which is even
 * worse than reading from a mapped pointer. There is also overhead in
 * keeping a copy of the buffer and constantly copying back and forth
 * (potentially diff'ing the contents each time).
 *
 * A hybrid solution would be to use intercepted buffers for non-
 * coherent memory, with the proviso that if a buffer is regularly mapped
 * then we fallback to returning a direct pointer until the frame capture
 * begins - if a map happens within a frame capture intercept it,
 * otherwise if it was mapped before the frame resort to reading back
 * from the mapped pointer. For coherent memory, always readback from the
 * mapped pointer. This is similar to behaviour on D3D or GL except that
 * a capture would fail if the map wasn't intercepted, rather than being
 * able to fall back.
 *
 * This is likely the best option if avoiding readbacks is desired as the
 * cost of constantly monitoring coherent maps for modifications and
 * copying around is generally extremely undesirable and may well be more
 * expensive than any readback cost.
 *
 * !!!!!!!!!!!!!!!
 * The current solution is to never intercept any maps, and rely on the
 * readback from memory not being too expensive and only happening during
 * frame capture where such an impact is less severe (as opposed to
 * reading back from this memory every frame even while idle).
 * !!!!!!!!!!!!!!!
 *
 * If in future this changes, the above hybrid solution is the next best
 * option to try to avoid most of the readbacks by using intercepted
 * buffers where possible, with a fallback to mapped pointer readback if
 * necessary.
 *
 * Note: No matter what we want to discouarge coherent persistent maps
 * (coherent transient maps are less of an issue) as these must still be
 * diff'd regularly during capture which has a high overhead (higher
 * still if there is extra cost on the readback).
 *
 ************************************************************************/

// Memory functions

template <>
VkBindBufferMemoryInfo *WrappedVulkan::UnwrapInfos(CaptureState state,
                                                   const VkBindBufferMemoryInfo *info, uint32_t count)
{
  VkBindBufferMemoryInfo *ret = GetTempArray<VkBindBufferMemoryInfo>(count);

  memcpy(ret, info, count * sizeof(VkBindBufferMemoryInfo));

  for(uint32_t i = 0; i < count; i++)
  {
    ret[i].buffer = Unwrap(ret[i].buffer);
    ret[i].memory = Unwrap(ret[i].memory);
  }

  return ret;
}

template <>
VkBindImageMemoryInfo *WrappedVulkan::UnwrapInfos(CaptureState state,
                                                  const VkBindImageMemoryInfo *info, uint32_t count)
{
  size_t memSize = sizeof(VkBindImageMemoryInfo) * count;

  for(uint32_t i = 0; i < count; i++)
    memSize += GetNextPatchSize(info[i].pNext);

  byte *tempMem = GetTempMemory(memSize);

  VkBindImageMemoryInfo *ret = (VkBindImageMemoryInfo *)tempMem;

  tempMem += sizeof(VkBindImageMemoryInfo) * count;

  memcpy(ret, info, count * sizeof(VkBindImageMemoryInfo));

  for(uint32_t i = 0; i < count; i++)
  {
    UnwrapNextChain(m_State, "VkBindImageMemoryInfo", tempMem, (VkBaseInStructure *)&ret[i]);
    ret[i].image = Unwrap(ret[i].image);
    ret[i].memory = Unwrap(ret[i].memory);
  }

  return ret;
}

bool WrappedVulkan::CheckMemoryRequirements(const char *resourceName, ResourceId memId,
                                            VkDeviceSize memoryOffset,
                                            const VkMemoryRequirements &mrq, bool external,
                                            const VkMemoryRequirements &origMrq)
{
  // verify that the memory meets basic requirements. If not, something changed and we should
  // bail loading this capture. This is a bit of an under-estimate since we just make sure
  // there's enough space left in the memory, that doesn't mean that there aren't overlaps due
  // to increased size requirements.
  ResourceId memOrigId = GetResourceManager()->GetOriginalID(memId);

  VulkanCreationInfo::Memory &memInfo = m_CreationInfo.m_Memory[memId];
  uint32_t bit = 1U << memInfo.memoryTypeIndex;

  bool origInvalid = false;

  // verify type
  if((mrq.memoryTypeBits & bit) == 0)
  {
    rdcstr bitsString;

    if((origMrq.memoryTypeBits & bit) == 0)
    {
      for(uint32_t i = 0; i < 32; i++)
      {
        if(origMrq.memoryTypeBits & (1U << i))
          bitsString += StringFormat::Fmt("%s%u", bitsString.empty() ? "" : ", ", i);
      }

      origInvalid = true;
    }
    else
    {
      for(uint32_t i = 0; i < 32; i++)
      {
        if(mrq.memoryTypeBits & (1U << i))
          bitsString += StringFormat::Fmt("%s%u", bitsString.empty() ? "" : ", ", i);
      }
    }

    SET_ERROR_RESULT(
        m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
        "Trying to bind %s to %s, but memory type is %u and only types %s are allowed.\n"
        "\n%s",
        resourceName, GetResourceDesc(memOrigId).name.c_str(), memInfo.memoryTypeIndex,
        bitsString.c_str(), GetPhysDeviceCompatString(external, origInvalid).c_str());
    return false;
  }

  // verify offset alignment
  if((memoryOffset % mrq.alignment) != 0)
  {
    VkDeviceSize align = mrq.alignment;

    if((memoryOffset % origMrq.alignment) != 0)
    {
      origInvalid = true;

      align = origMrq.alignment;
    }

    SET_ERROR_RESULT(
        m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
        "Trying to bind %s to %s, but memory offset 0x%llx doesn't satisfy alignment 0x%llx.\n"
        "\n%s",
        resourceName, GetResourceDesc(memOrigId).name.c_str(), memoryOffset, mrq.alignment,
        GetPhysDeviceCompatString(external, origInvalid).c_str());
    return false;
  }

  // verify size
  if(mrq.size > memInfo.allocSize - memoryOffset)
  {
    VkDeviceSize size = mrq.size;

    if(origMrq.size > memInfo.allocSize - memoryOffset)
    {
      origInvalid = true;

      size = origMrq.size;
    }

    SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
                     "Trying to bind %s to %s, but at memory offset 0x%llx the reported size of "
                     "0x%llx won't fit the 0x%llx bytes of memory.\n"
                     "\n%s",
                     resourceName, GetResourceDesc(memOrigId).name.c_str(), memoryOffset, size,
                     memInfo.allocSize, GetPhysDeviceCompatString(external, origInvalid).c_str());
    return false;
  }

  return true;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkAllocateMemory(SerialiserType &ser, VkDevice device,
                                               const VkMemoryAllocateInfo *pAllocateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkDeviceMemory *pMemory)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Memory, GetResID(*pMemory)).TypedAs("VkDeviceMemory"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkMemoryAllocateInfo patched = AllocateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkMemoryAllocateInfo", tempMem, (VkBaseInStructure *)&patched);

    // remove dedicated memory struct if it is not allowed due to changing memory sizes
    {
      VkMemoryDedicatedAllocateInfo *dedicated = (VkMemoryDedicatedAllocateInfo *)FindNextStruct(
          &AllocateInfo, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
      if(dedicated && dedicated->image != VK_NULL_HANDLE)
      {
        VkMemoryRequirements mrq = {};
        ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(dedicated->image), &mrq);

        if(mrq.size != AllocateInfo.allocationSize)
        {
          RDCDEBUG("Removing dedicated allocation for incompatible size");
          RemoveNextStruct(&patched, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
        }
      }
    }

    if(patched.memoryTypeIndex >= m_PhysicalDeviceData.memProps.memoryTypeCount)
    {
      SET_ERROR_RESULT(
          m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Tried to allocate memory from index %u, but on replay we only have %u memory types.\n"
          "\n%s",
          patched.memoryTypeIndex, m_PhysicalDeviceData.memProps.memoryTypeCount,
          GetPhysDeviceCompatString(
              false, patched.memoryTypeIndex >= m_OrigPhysicalDeviceData.memProps.memoryTypeCount)
              .c_str());
      return false;
    }

    VkResult ret = ObjDisp(device)->AllocateMemory(Unwrap(device), &patched, NULL, &mem);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed allocating memory, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), mem);
      GetResourceManager()->AddLiveResource(Memory, mem);

      m_CreationInfo.m_Memory[live].Init(GetResourceManager(), m_CreationInfo, &AllocateInfo);

      VkMemoryDedicatedAllocateInfo *dedicated = (VkMemoryDedicatedAllocateInfo *)FindNextStruct(
          &AllocateInfo, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
      if(dedicated && dedicated->buffer == VK_NULL_HANDLE && dedicated->image == VK_NULL_HANDLE)
      {
        dedicated = NULL;
      }

      VkDedicatedAllocationMemoryAllocateInfoNV *dedicatedNV =
          (VkDedicatedAllocationMemoryAllocateInfoNV *)FindNextStruct(
              &AllocateInfo, VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV);
      if(dedicatedNV && dedicatedNV->buffer == VK_NULL_HANDLE && dedicatedNV->image == VK_NULL_HANDLE)
      {
        dedicatedNV = NULL;
      }

      if(dedicated)
      {
        // either set the buffer that's dedicated, or if this is dedicated image memory set NULL
        m_CreationInfo.m_Memory[live].wholeMemBuf = dedicated->buffer;

        uint64_t bufSize = m_CreationInfo.m_Buffer[GetResID(dedicated->buffer)].size;
        uint64_t &memSize = m_CreationInfo.m_Memory[live].wholeMemBufSize;
        if(memSize > bufSize)
        {
          RDCDEBUG("Truncating memory size %llu to dedicated buffer size %llu for %s", memSize,
                   bufSize, ToStr(Memory).c_str());
          memSize = bufSize;
        }
      }
      else if(dedicatedNV)
      {
        m_CreationInfo.m_Memory[live].wholeMemBuf = dedicatedNV->buffer;

        uint64_t bufSize = m_CreationInfo.m_Buffer[GetResID(dedicatedNV->buffer)].size;
        uint64_t &memSize = m_CreationInfo.m_Memory[live].wholeMemBufSize;
        if(memSize > bufSize)
        {
          RDCDEBUG("Truncating memory size %llu to dedicated buffer size %llu for %s", memSize,
                   bufSize, ToStr(Memory).c_str());
          memSize = bufSize;
        }
      }
      else
      {
        // create a buffer with the whole memory range bound, for copying to and from
        // conveniently (for initial state data)
        VkBuffer buf = VK_NULL_HANDLE;

        VkBufferCreateInfo bufInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            NULL,
            0,
            AllocateInfo.allocationSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };

        ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &bufInfo, NULL, &buf);
        RDCASSERTEQUAL(ret, VK_SUCCESS);

        // we already validated at replay time that the memory size is aligned/etc as necessary so
        // we can create a buffer of the whole size, but just to keep the validation layers happy
        // let's check the requirements here again.
        VkMemoryRequirements mrq = {};
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), buf, &mrq);

        // check that this allocation type can actually be bound to a buffer. Allocations that can't
        // be used with buffers we can just skip and leave wholeMemBuf as NULL.
        if((1 << AllocateInfo.memoryTypeIndex) & mrq.memoryTypeBits)
        {
          RDCASSERT(mrq.size <= AllocateInfo.allocationSize, mrq.size, AllocateInfo.allocationSize);

          ResourceId bufid = GetResourceManager()->WrapResource(Unwrap(device), buf);

          ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buf), Unwrap(mem), 0);

          // register as a live-only resource, so it is cleaned up properly
          GetResourceManager()->AddLiveResource(bufid, buf);

          m_CreationInfo.m_Memory[live].wholeMemBuf = buf;
        }
        else
        {
          RDCWARN("Can't create buffer covering memory allocation %s", ToStr(Memory).c_str());
          ObjDisp(device)->DestroyBuffer(Unwrap(device), buf, NULL);

          m_CreationInfo.m_Memory[live].wholeMemBuf = VK_NULL_HANDLE;
        }
      }
    }

    AddResource(Memory, ResourceType::Memory, "Memory");
    DerivedResource(device, Memory);
  }

  return true;
}

VkResult WrappedVulkan::vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                         const VkAllocationCallbacks *, VkDeviceMemory *pMemory)
{
  VkMemoryAllocateInfo info = *pAllocateInfo;

  {
    // we need to be able to allocate a buffer that covers the whole memory range. However
    // if the memory is e.g. 100 bytes (arbitrary example) and buffers have memory requirements
    // such that it must be bound to a multiple of 128 bytes, then we can't create a buffer
    // that entirely covers a 100 byte allocation.
    // To get around this, we create a buffer of the allocation's size with the properties we
    // want, check its required size, then bump up the allocation size to that as if the application
    // had requested more. We're assuming here no system will require something like "buffer of
    // size N must be bound to memory of size N+O for some value of O overhead bytes".
    //
    // this could be optimised as maybe we'll be creating buffers of multiple sizes, but allocation
    // in vulkan is already expensive and making it a little more expensive isn't a big deal.

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        info.allocationSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    // since this is very short lived, it's not wrapped
    VkBuffer buf;

    VkResult vkr = ObjDisp(device)->CreateBuffer(Unwrap(device), &bufInfo, NULL, &buf);
    CheckVkResult(vkr);

    if(vkr == VK_SUCCESS && buf != VK_NULL_HANDLE)
    {
      VkMemoryRequirements mrq = {0};
      ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), buf, &mrq);

      RDCASSERTMSG("memory requirements less than desired size", mrq.size >= bufInfo.size, mrq.size,
                   bufInfo.size);

      // round up allocation size to allow creation of buffers
      if(mrq.size >= bufInfo.size)
        info.allocationSize = mrq.size;
    }

    ObjDisp(device)->DestroyBuffer(Unwrap(device), buf, NULL);
  }

  VkMemoryAllocateInfo unwrapped = info;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrapped.pNext));

  UnwrapNextChain(m_State, "VkMemoryAllocateInfo", tempMem, (VkBaseInStructure *)&unwrapped);

  VkMemoryAllocateFlagsInfo *memFlags = (VkMemoryAllocateFlagsInfo *)FindNextStruct(
      &unwrapped, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);

  // since the application must specify VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT itself, we can
  // assume the struct is present and just add the capture-replay flag to allow us to specify the
  // address on replay. We ensured the physical device can support this feature (and it was enabled)
  // when whitelisting the extension and creating the device.
  if(IsCaptureMode(m_State) && memFlags && (memFlags->flags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT))
    memFlags->flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;

  // remove dedicated memory struct if it is not allowed
  {
    VkMemoryDedicatedAllocateInfo *dedicated = (VkMemoryDedicatedAllocateInfo *)FindNextStruct(
        pAllocateInfo, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
    if(dedicated && dedicated->image != VK_NULL_HANDLE)
    {
      VkResourceRecord *imageRecord = GetRecord(dedicated->image);

      if(imageRecord->resInfo->banDedicated)
        RemoveNextStruct(&unwrapped, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
    }
  }

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->AllocateMemory(Unwrap(device), &unwrapped, NULL, pMemory));

  // restore the memoryTypeIndex to the original, as that's what we want to serialise,
  // but maintain any potential modifications we made to info.allocationSize
  info.memoryTypeIndex = pAllocateInfo->memoryTypeIndex;

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pMemory);

    VkMemoryDedicatedAllocateInfo *dedicated = (VkMemoryDedicatedAllocateInfo *)FindNextStruct(
        pAllocateInfo, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO);
    if(dedicated && dedicated->buffer == VK_NULL_HANDLE && dedicated->image == VK_NULL_HANDLE)
    {
      dedicated = NULL;
    }

    VkDedicatedAllocationMemoryAllocateInfoNV *dedicatedNV =
        (VkDedicatedAllocationMemoryAllocateInfoNV *)FindNextStruct(
            pAllocateInfo, VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV);
    if(dedicatedNV && dedicatedNV->buffer == VK_NULL_HANDLE && dedicatedNV->image == VK_NULL_HANDLE)
    {
      dedicatedNV = NULL;
    }

    // create a buffer with the whole memory range bound, for copying to and from
    // conveniently (for initial state data)
    VkBuffer wholeMemBuf = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        info.allocationSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    if(IsCaptureMode(m_State))
    {
      // we make the buffer concurrently accessible by all queue families to not invalidate the
      // contents of the memory we're reading back from.
      bufInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
      bufInfo.queueFamilyIndexCount = (uint32_t)m_QueueFamilyIndices.size();
      bufInfo.pQueueFamilyIndices = m_QueueFamilyIndices.data();

      // spec requires that CONCURRENT must specify more than one queue family. If there is only one
      // queue family, we can safely use exclusive.
      if(bufInfo.queueFamilyIndexCount == 1)
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkDeviceSize memSize = info.allocationSize;
    ResourceId bufid;

    if(dedicated)
    {
      // either set the buffer that's dedicated, or if this is dedicated image memory set NULL
      wholeMemBuf = dedicated->buffer;
    }
    else if(dedicatedNV)
    {
      wholeMemBuf = dedicatedNV->buffer;
    }
    else
    {
      ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &bufInfo, NULL, &wholeMemBuf);
      RDCASSERTEQUAL(ret, VK_SUCCESS);

      // we already validated above that the memory size is aligned/etc as necessary so we can
      // create a buffer of the whole size, but just to keep the validation layers happy let's check
      // the requirements here again.
      VkMemoryRequirements mrq = {};
      ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), wholeMemBuf, &mrq);

      RDCASSERTEQUAL(mrq.size, info.allocationSize);

      if((mrq.memoryTypeBits & (1U << info.memoryTypeIndex)) != 0)
      {
        bufid = GetResourceManager()->WrapResource(Unwrap(device), wholeMemBuf);

        ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(wholeMemBuf), Unwrap(*pMemory), 0);
      }
      else
      {
        // can't create a memory-spanning buffer for this allocation. Assume this is a case where
        // this memory type is only available to images and is not mappable - in which case the
        // whole memory buffer won't be needed so we can skip this.
        ObjDisp(device)->DestroyBuffer(Unwrap(device), wholeMemBuf, NULL);
        wholeMemBuf = VK_NULL_HANDLE;
      }
    }

    if((dedicated != NULL || dedicatedNV != NULL) && wholeMemBuf != VK_NULL_HANDLE)
    {
      VkResourceRecord *bufRecord = GetRecord(wholeMemBuf);

      // make sure we have a resInfo if we don't already
      if(!bufRecord->resInfo)
      {
        bufRecord->resInfo = new ResourceInfo();

        // pre-populate memory requirements
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(wholeMemBuf),
                                                     &bufRecord->resInfo->memreqs);
      }

      RDCASSERTEQUAL(bufRecord->resInfo->dedicatedMemory, ResourceId());

      bufRecord->resInfo->dedicatedMemory = id;

      VkDeviceSize bufSize = IsCaptureMode(m_State)
                                 ? bufRecord->memSize
                                 : m_CreationInfo.m_Buffer[GetResID(wholeMemBuf)].size;
      if(memSize > bufSize)
      {
        RDCDEBUG("Truncating memory size %llu to dedicated buffer size %llu for %s", memSize,
                 bufSize, ToStr(id).c_str());
        memSize = bufSize;
      }
    }

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      VkMemoryAllocateInfo serialisedInfo = info;
      VkMemoryOpaqueCaptureAddressAllocateInfo memoryDeviceAddress = {
          VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,
      };

      // create resource record for gpu memory
      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pMemory);
      RDCASSERT(record);

      memFlags = (VkMemoryAllocateFlagsInfo *)FindNextStruct(
          &serialisedInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);

      if(memFlags && (memFlags->flags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT))
      {
        VkDeviceMemoryOpaqueCaptureAddressInfo getInfo = {
            VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO,
            NULL,
            Unwrap(*pMemory),
        };

        memoryDeviceAddress.opaqueCaptureAddress =
            ObjDisp(device)->GetDeviceMemoryOpaqueCaptureAddress(Unwrap(device), &getInfo);

        // we explicitly DON'T assert on this, because some drivers will only need the device
        // address specified at allocate time.
        // RDCASSERT(memoryDeviceAddress.opaqueCaptureAddress);

        // push this struct onto the start of the chain
        memoryDeviceAddress.pNext = serialisedInfo.pNext;
        serialisedInfo.pNext = &memoryDeviceAddress;

        memFlags->flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;

        {
          SCOPED_LOCK(m_DeviceAddressResourcesLock);
          m_DeviceAddressResources.IDs.push_back(record->GetResourceID());
        }
      }

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkAllocateMemory);
        Serialise_vkAllocateMemory(ser, device, &serialisedInfo, NULL, pMemory);

        chunk = scope.Get();
      }

      record->AddChunk(chunk);

      record->Length = memSize;

      uint32_t memProps =
          m_PhysicalDeviceData.memProps.memoryTypes[info.memoryTypeIndex].propertyFlags;

      record->memMapState = new MemMapState();
      record->memMapState->wholeMemBuf = wholeMemBuf;
      record->memMapState->dedicated = dedicated != NULL || dedicatedNV != NULL;

      // if memory is not host visible, so not mappable, don't create map state at all
      if((memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
      {
        record->memMapState->mapCoherent = (memProps & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

        // some types of memory are faster to readback via a synchronous call to the GPU
        if(Vulkan_GPUReadbackDeviceLocal())
        {
          // on discrete GPUs, all device local memory should be readback this way
          if(m_PhysicalDeviceData.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            record->memMapState->readbackOnGPU =
                (memProps & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;

          // non-cached memory are generally always faster to readback on the GPU. Some GPUs allow
          // faster readback via non-temporal loads but GPU copy speeds are comparable enough
          record->memMapState->readbackOnGPU = ((memProps & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == 0);

#if ENABLED(RDOC_ANDROID)
          // on Android all memory types are marked as device local. Types which are fast to read
          // back directly from the CPU are CACHED but *not* COHERENT. Any types which are either
          // only COHERENT, or are both CACHED and COHERENT, are still faster to readback via GPU
          // copy.
          record->memMapState->readbackOnGPU = (memProps & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
#endif

          // we need a wholeMemBuf to readback on the GPU
          if(record->memMapState->readbackOnGPU && wholeMemBuf == VK_NULL_HANDLE)
          {
            RDCWARN(
                "Memory allocation would have been readback on GPU, but can't without wholeMemBuf");
            record->memMapState->readbackOnGPU = false;
          }
        }
      }

      GetResourceManager()->AddDeviceMemory(id);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pMemory);

      m_CreationInfo.m_Memory[id].Init(GetResourceManager(), m_CreationInfo, &info);

      if(dedicated == NULL && dedicatedNV == NULL && wholeMemBuf != VK_NULL_HANDLE)
      {
        // register as a live-only resource, so it is cleaned up properly
        GetResourceManager()->AddLiveResource(bufid, wholeMemBuf);
      }

      m_CreationInfo.m_Memory[id].wholeMemBuf = wholeMemBuf;
    }
  }
  else
  {
    CheckVkResult(ret);
  }

  return ret;
}

void WrappedVulkan::vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *)
{
  if(memory == VK_NULL_HANDLE)
    return;

  // we just need to clean up after ourselves on replay
  WrappedVkNonDispRes *wrapped = (WrappedVkNonDispRes *)GetWrapped(memory);

  VkDeviceMemory unwrappedMem = wrapped->real.As<VkDeviceMemory>();

  VkBuffer wholeMemDestroy = VK_NULL_HANDLE;

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *memrecord = GetRecord(memory);

    // any forced references were already processed at the start of the frame if we're mid capture.
    // If we're background capturing though, we need to make sure not to force in buffers
    // referencing this now-dead memory, as a new memory allocation could be created and use the
    // same BDA address
    {
      SCOPED_LOCK(m_ForcedReferencesLock);
      m_ForcedReferences.removeIf(
          [memrecord](const VkResourceRecord *record) { return record->HasParent(memrecord); });
    }

    // artificially extend the lifespan of buffer device address memory or buffers, to ensure their
    // opaque capture address isn't re-used before the capture completes
    {
      SCOPED_READLOCK(m_CapTransitionLock);
      SCOPED_LOCK(m_DeviceAddressResourcesLock);
      if(IsActiveCapturing(m_State) && m_DeviceAddressResources.IDs.contains(GetResID(memory)))
      {
        m_DeviceAddressResources.DeadMemories.push_back(memory);
        return;
      }
      m_DeviceAddressResources.IDs.removeOne(GetResID(memory));
    }

    MemMapState *memMapState = wrapped->record->memMapState;

    if(memMapState)
    {
      // there is an implicit unmap on free, so make sure to tidy up
      if(memMapState->refData)
      {
        FreeAlignedBuffer(memMapState->refData);
        memMapState->refData = NULL;
      }

      // destroy the wholeMemBuf if it's one we allocated ourselves
      if(!memMapState->dedicated)
        wholeMemDestroy = memMapState->wholeMemBuf;
    }

    {
      SCOPED_LOCK(m_CoherentMapsLock);
      m_CoherentMaps.removeOne(wrapped->record);
    }

    GetResourceManager()->RemoveDeviceMemory(wrapped->id);
  }

  m_CreationInfo.erase(GetResID(memory));

  GetResourceManager()->ReleaseWrappedResource(memory);

  // destroy this last, so that any postponed resource being serialised above still has this
  // available
  if(wholeMemDestroy != VK_NULL_HANDLE)
  {
    ObjDisp(device)->DestroyBuffer(Unwrap(device), Unwrap(wholeMemDestroy), NULL);
    GetResourceManager()->ReleaseWrappedResource(wholeMemDestroy);
  }

  ObjDisp(device)->FreeMemory(Unwrap(device), unwrappedMem, NULL);
}

VkResult WrappedVulkan::vkMapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
  // ensure we always map on a 16-byte boundary. This is for our own purposes so we can
  // FindDiffRange against the mapped region. We adjust the pointer returned to the user but
  // otherwise we act as if the mapped region was 16-byte aligned. Fortunately flushed regions in
  // vkFlushMappedMemoryRanges are relative to the memory base, not the mapped region, so this
  // offset effectively only modifies the returned pointer and has no other side-effects.
  VkDeviceSize misalignedOffset = offset & 0xf;
  offset &= ~0xf;
  // need to adjust the size so the end-point is still the same!
  size += misalignedOffset;

  byte *realData = NULL;
  VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), offset, size, flags,
                                            (void **)&realData);

  if(ret == VK_SUCCESS && realData)
  {
    ResourceId id = GetResID(mem);

    if(IsCaptureMode(m_State))
    {
      VkResourceRecord *memrecord = GetRecord(mem);

      // must have map state, only non host visible memories have no map
      // state, and they can't be mapped!
      RDCASSERT(memrecord->memMapState);
      MemMapState &state = *memrecord->memMapState;

      // ensure size is valid
      RDCASSERT(size == VK_WHOLE_SIZE || (size > 0 && offset + size <= memrecord->Length),
                GetResID(mem), size, memrecord->Length);

      // flush range offsets are relative to the start of the memory so keep mappedPtr at that
      // basis. We'll only access within the mapped range
      state.cpuReadPtr = state.mappedPtr = (byte *)realData - (size_t)offset;
      state.refData = NULL;

      state.mapOffset = offset;
      state.mapSize = size == VK_WHOLE_SIZE ? (memrecord->Length - offset)
                                            : RDCMIN(memrecord->Length - offset, size);

      *ppData = realData + misalignedOffset;

      if(state.mapCoherent)
      {
        SCOPED_LOCK(m_CoherentMapsLock);
        m_CoherentMaps.push_back(memrecord);
      }
    }
    else
    {
      *ppData = realData + misalignedOffset;
    }
  }
  else
  {
    *ppData = NULL;
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkUnmapMemory(SerialiserType &ser, VkDevice device,
                                            VkDeviceMemory memory)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(memory).Important();

  uint64_t MapOffset = 0;
  uint64_t MapSize = 0;
  byte *MapData = NULL;

  MemMapState *state = NULL;
  if(IsCaptureMode(m_State))
  {
    state = GetRecord(memory)->memMapState;

    MapOffset = state->mapOffset;
    MapSize = state->mapSize;

    MapData = (byte *)state->cpuReadPtr + MapOffset;
  }

  SERIALISE_ELEMENT(MapOffset).OffsetOrSize();
  SERIALISE_ELEMENT(MapSize).OffsetOrSize();

  bool directStream = true;

  if(IsReplayingAndReading() && memory != VK_NULL_HANDLE)
  {
    if(IsLoading(m_State))
      m_ResourceUses[GetResID(memory)].push_back(EventUsage(m_RootEventID, ResourceUsage::CPUWrite));

    VkResult vkr = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(memory), MapOffset, MapSize, 0,
                                              (void **)&MapData);
    if(vkr != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error mapping memory on replay, VkResult: %s", ToStr(vkr).c_str());
      return false;
    }
    if(!MapData)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error mapping memory on replay");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      return false;
    }

    const Intervals<VulkanCreationInfo::Memory::MemoryBinding> &bindings =
        m_CreationInfo.m_Memory[GetResID(memory)].bindings;

    uint64_t finish = MapOffset + MapSize;

    auto it = bindings.find(MapOffset);

    // iterate the bindings that this map region overlaps, if we overlap with any tiled memory we
    // need to take the slow path
    while(it != bindings.end() && it->start() < finish)
    {
      if(it->value() == VulkanCreationInfo::Memory::Tiled)
      {
        if(IsLoading(m_State))
        {
          AddDebugMessage(MessageCategory::Performance, MessageSeverity::Medium,
                          MessageSource::GeneralPerformance,
                          "Unmapped memory overlaps tiled-only memory region. "
                          "Taking slow path to mask tiled memory writes");
        }
        directStream = false;
        m_MaskedMapData.resize((size_t)MapSize);
        break;
      }

      it++;
    }
  }

  if(directStream)
  {
    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("MapData"_lit, MapData, MapSize, SerialiserFlags::NoFlags).Important();
  }
  else
  {
    // serialise into temp storage
    byte *tmp = m_MaskedMapData.data();
    ser.Serialise("MapData"_lit, tmp, MapSize, SerialiserFlags::NoFlags).Important();

    const Intervals<VulkanCreationInfo::Memory::MemoryBinding> &bindings =
        m_CreationInfo.m_Memory[GetResID(memory)].bindings;

    uint64_t finish = MapOffset + MapSize;

    auto it = bindings.find(MapOffset);

    // iterate the bindings that this map region overlaps, and only memcpy the bits that we overlap
    // which are linear
    while(it != bindings.end() && it->start() < finish)
    {
      if(it->value() != VulkanCreationInfo::Memory::Tiled)
      {
        // start at the map offset or the region offset, whichever is *later*. E.g. if the region is
        // larger than the map we only start where the map started, and vice-versa if the map
        // started earlier than the region.
        // We also rebase it so that it's relative to the map, so it's the byte offset for the
        // memcpy
        size_t offs = size_t(RDCMAX(it->start(), MapOffset) - MapOffset);

        // similarly, only copy up to the end of the region or the end ofthe map whichever is
        // *sooner*.
        size_t size = size_t(RDCMIN(it->finish(), finish) - offs);

        memcpy(MapData + offs, m_MaskedMapData.data() + offs, size);
      }

      it++;
    }
  }

  if(IsReplayingAndReading() && MapData && memory != VK_NULL_HANDLE)
    ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(memory));

  SERIALISE_CHECK_READ_ERRORS();

  return true;
}

void WrappedVulkan::vkUnmapMemory(VkDevice device, VkDeviceMemory mem)
{
  if(IsCaptureMode(m_State))
  {
    ResourceId id = GetResID(mem);

    VkResourceRecord *memrecord = GetRecord(mem);

    RDCASSERT(memrecord->memMapState);
    MemMapState &state = *memrecord->memMapState;

    if(state.mapCoherent)
    {
      SCOPED_LOCK(m_CoherentMapsLock);

      int32_t idx = m_CoherentMaps.indexOf(memrecord);
      if(idx < 0)
        RDCERR("vkUnmapMemory for memory handle that's not currently mapped");
      else
        m_CoherentMaps.erase(idx);
    }

    {
      // decide atomically if this chunk should be in-frame or not
      // so that we're not in the else branch but haven't marked
      // dirty when capframe starts, then we mark dirty while in-frame

      bool capframe = false;
      {
        SCOPED_READLOCK(m_CapTransitionLock);
        capframe = IsActiveCapturing(m_State);

        if(!capframe)
        {
          GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_PartialWrite);
        }
      }

      SCOPED_LOCK(state.mrLock);

      if(capframe)
      {
        // coherent maps must always serialise all data on unmap, even if a flush was seen, because
        // unflushed data is *also* visible. This is a bit redundant since data is serialised here
        // and in any flushes, but that's the app's fault - the spec calls out flushing coherent
        // maps
        // as inefficient
        // if the memory is not coherent, we must have a flush for every region written while it is
        // mapped, there is no implicit flush on unmap, so we follow the spec strictly on this.
        if(state.mapCoherent)
        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkUnmapMemory);
          Serialise_vkUnmapMemory(ser, device, mem);

          VkResourceRecord *record = GetRecord(mem);

          if(IsBackgroundCapturing(m_State))
          {
            record->AddChunk(scope.Get());
          }
          else
          {
            m_FrameCaptureRecord->AddChunk(scope.Get());
            GetResourceManager()->MarkMemoryFrameReferenced(id, state.mapOffset, state.mapSize,
                                                            eFrameRef_PartialWrite);
          }
        }
      }

      state.cpuReadPtr = state.mappedPtr = NULL;
    }

    FreeAlignedBuffer(state.refData);
    state.refData = NULL;
  }

  ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkFlushMappedMemoryRanges(SerialiserType &ser, VkDevice device,
                                                        uint32_t memRangeCount,
                                                        const VkMappedMemoryRange *pMemRanges)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(memRangeCount);
  SERIALISE_ELEMENT_LOCAL(MemRange, *pMemRanges).Important();

  byte *MappedData = NULL;
  uint64_t memRangeSize = 1;

  MemMapState *state = NULL;
  if(ser.IsWriting())
  {
    VkResourceRecord *record = GetRecord(MemRange.memory);
    state = record->memMapState;

    memRangeSize = MemRange.size;
    if(memRangeSize == VK_WHOLE_SIZE)
      memRangeSize = record->Length - MemRange.offset;

    // don't support any extensions on VkMappedMemoryRange
    RDCASSERT(pMemRanges->pNext == NULL);

    MappedData = state->cpuReadPtr + (size_t)MemRange.offset;
  }

  bool directStream = true;

  if(IsReplayingAndReading() && MemRange.memory != VK_NULL_HANDLE && MemRange.size > 0)
  {
    if(IsLoading(m_State))
      m_ResourceUses[GetResID(MemRange.memory)].push_back(
          EventUsage(m_RootEventID, ResourceUsage::CPUWrite));

    VkResult ret =
        ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(MemRange.memory), MemRange.offset,
                                   MemRange.size, 0, (void **)&MappedData);
    CheckVkResult(ret);
    if(ret != VK_SUCCESS)
      RDCERR("Error mapping memory on replay: %s", ToStr(ret).c_str());
    if(!MappedData)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error mapping memory on replay");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      return false;
    }

    const VulkanCreationInfo::Memory &memInfo = m_CreationInfo.m_Memory[GetResID(MemRange.memory)];
    const Intervals<VulkanCreationInfo::Memory::MemoryBinding> &bindings = memInfo.bindings;

    memRangeSize = MemRange.size;
    if(memRangeSize == VK_WHOLE_SIZE)
      memRangeSize = memInfo.allocSize - MemRange.offset;

    uint64_t finish = MemRange.offset + memRangeSize;

    auto it = bindings.find(MemRange.offset);

    // iterate the bindings that this map region overlaps, if we overlap with any tiled memory we
    // need to take the slow path
    while(it != bindings.end() && it->start() < finish)
    {
      if(it->value() == VulkanCreationInfo::Memory::Tiled)
      {
        if(IsLoading(m_State))
        {
          AddDebugMessage(
              MessageCategory::Performance, MessageSeverity::Medium,
              MessageSource::GeneralPerformance,
              StringFormat::Fmt(
                  "Unmapped memory %s overlaps tiled-only memory region. "
                  "Taking slow path to mask tiled memory writes",
                  ToStr(GetResourceManager()->GetOriginalID(GetResID(MemRange.memory))).c_str()));
        }
        directStream = false;
        m_MaskedMapData.resize((size_t)memRangeSize);
        break;
      }

      it++;
    }
  }

  if(directStream)
  {
    // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
    // directly into upload memory
    ser.Serialise("MapData"_lit, MappedData, memRangeSize, SerialiserFlags::NoFlags).Important();
  }
  else
  {
    // serialise into temp storage
    byte *tmp = m_MaskedMapData.data();
    ser.Serialise("MapData"_lit, tmp, memRangeSize, SerialiserFlags::NoFlags).Important();

    const Intervals<VulkanCreationInfo::Memory::MemoryBinding> &bindings =
        m_CreationInfo.m_Memory[GetResID(MemRange.memory)].bindings;

    uint64_t mappedRegionStart = MemRange.offset;
    uint64_t mappedRegionFinish = MemRange.offset + memRangeSize;

    auto it = bindings.find(mappedRegionStart);

    // iterate the bindings that this map region overlaps, and only memcpy the bits that we overlap
    // which are linear
    while(it != bindings.end() && it->start() < mappedRegionFinish)
    {
      if(it->value() != VulkanCreationInfo::Memory::Tiled)
      {
        // start at the map offset or the region offset, whichever is *later*. E.g. if the region is
        // larger than the map we only start where the map started, and vice-versa if the map
        // started earlier than the region.
        uint64_t start = RDCMAX(it->start(), mappedRegionStart);

        // similarly, finish at the end of the region or the end of the map whichever is *sooner*.
        uint64_t finish = RDCMIN(it->finish(), mappedRegionFinish);

        // Transform now to be relative to the start of the map. Note that since we max'd with
        // the map start/finish above this won't underflow
        size_t offs = size_t(start - mappedRegionStart);
        size_t size = size_t(finish - start);

        memcpy(MappedData + offs, m_MaskedMapData.data() + offs, size);
      }

      it++;
    }
  }

  if(IsReplayingAndReading() && MappedData && MemRange.memory != VK_NULL_HANDLE && MemRange.size > 0)
  {
    const VulkanCreationInfo::Memory &memInfo = m_CreationInfo.m_Memory[GetResID(MemRange.memory)];
    if((m_PhysicalDeviceData.memProps.memoryTypes[memInfo.memoryTypeIndex].propertyFlags &
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
    {
      VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          NULL,
          Unwrap(MemRange.memory),
          MemRange.offset,
          MemRange.size,
      };

      ObjDisp(device)->FlushMappedMemoryRanges(Unwrap(device), 1, &range);
    }
    ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(MemRange.memory));
  }

  SERIALISE_CHECK_READ_ERRORS();

  // if we need to save off this serialised buffer as reference for future comparison,
  // do so now. See the call to vkFlushMappedMemoryRanges in WrappedVulkan::vkQueueSubmit()
  if(ser.IsWriting() && state->needRefData)
  {
    if(!state->refData)
    {
      // if we're in this case, the range should be for the whole memory region.
      RDCASSERT(MemRange.offset == state->mapOffset && memRangeSize == state->mapSize,
                MemRange.offset, memRangeSize, state->mapOffset, state->mapSize);

      // allocate ref data so we can compare next time to minimise serialised data
      state->refData = AllocAlignedBuffer((size_t)state->mapSize);
    }

    // the memory range offset should always be at least the map offset
    RDCASSERT(MemRange.offset >= state->mapOffset, MemRange.offset, state->mapOffset);

    // it's no longer safe to use state->mappedPtr, we need to save *precisely* what
    // was serialised. We do this by copying out of the serialiser since we know this
    // memory is not changing
    size_t offs = size_t(ser.GetWriter()->GetOffset() - memRangeSize);

    const byte *serialisedData = ser.GetWriter()->GetData() + offs;

    memcpy(state->refData + MemRange.offset - state->mapOffset, serialisedData, (size_t)memRangeSize);
  }

  return true;
}

void WrappedVulkan::InternalFlushMemoryRange(VkDevice device, const VkMappedMemoryRange &memRange,
                                             bool internalFlush, bool capframe)
{
  ResourceId memid = GetResID(memRange.memory);
  VkResourceRecord *record = GetRecord(memRange.memory);

  MemMapState *state = record->memMapState;

  if(state->mappedPtr == NULL)
  {
    RDCERR("Flushing memory %s that isn't currently mapped", ToStr(memid).c_str());
    return;
  }

  if(capframe)
  {
    SCOPED_LOCK_OPTIONAL(state->mrLock, !internalFlush);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(internalFlush ? VulkanChunk::CoherentMapWrite
                                         : VulkanChunk::vkFlushMappedMemoryRanges);
    Serialise_vkFlushMappedMemoryRanges(ser, device, 1, &memRange);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  if(capframe)
  {
    VkDeviceSize offs = memRange.offset;
    VkDeviceSize size = memRange.size;

    // map VK_WHOLE_SIZE into a specific size
    if(size == VK_WHOLE_SIZE)
      size = state->mapOffset + state->mapSize - offs;

    GetResourceManager()->MarkMemoryFrameReferenced(memid, offs, size, eFrameRef_CompleteWrite);
  }
  else
  {
    FrameRefType refType = eFrameRef_PartialWrite;
    if(memRange.offset == 0 && memRange.size >= record->Length)
      refType = eFrameRef_CompleteWrite;

    GetResourceManager()->MarkResourceFrameReferenced(memid, refType);
  }
}

VkResult WrappedVulkan::vkFlushMappedMemoryRanges(VkDevice device, uint32_t memRangeCount,
                                                  const VkMappedMemoryRange *pMemRanges)
{
  VkMappedMemoryRange *unwrapped = GetTempArray<VkMappedMemoryRange>(memRangeCount);
  for(uint32_t i = 0; i < memRangeCount; i++)
  {
    unwrapped[i] = pMemRanges[i];
    unwrapped[i].memory = Unwrap(unwrapped[i].memory);
  }

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->FlushMappedMemoryRanges(Unwrap(device), memRangeCount, unwrapped));

  if(IsCaptureMode(m_State))
  {
    bool capframe = false;

    {
      SCOPED_READLOCK(m_CapTransitionLock);
      capframe = IsActiveCapturing(m_State);
    }

    for(uint32_t i = 0; i < memRangeCount; i++)
    {
      InternalFlushMemoryRange(device, pMemRanges[i], false, capframe);
    }
  }

  return ret;
}

VkResult WrappedVulkan::vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memRangeCount,
                                                       const VkMappedMemoryRange *pMemRanges)
{
  VkMappedMemoryRange *unwrapped = GetTempArray<VkMappedMemoryRange>(memRangeCount);
  for(uint32_t i = 0; i < memRangeCount; i++)
  {
    unwrapped[i] = pMemRanges[i];
    unwrapped[i].memory = Unwrap(unwrapped[i].memory);
  }

  // don't need to serialise this, readback from mapped memory is not captured
  // and is only relevant for the application.
  return ObjDisp(device)->InvalidateMappedMemoryRanges(Unwrap(device), memRangeCount, unwrapped);
}

// Generic API object functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkBindBufferMemory(SerialiserType &ser, VkDevice device,
                                                 VkBuffer buffer, VkDeviceMemory memory,
                                                 VkDeviceSize memoryOffset)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(memory).Important();
  SERIALISE_ELEMENT(memoryOffset).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(buffer));
    ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(memory));

    VulkanCreationInfo::Buffer &bufInfo = m_CreationInfo.m_Buffer[GetResID(buffer)];

    VkMemoryRequirements mrq = {};
    ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), &mrq);

    bool ok = CheckMemoryRequirements(GetResourceDesc(resOrigId).name.c_str(), GetResID(memory),
                                      memoryOffset, mrq, bufInfo.external, bufInfo.mrq);

    if(!ok)
      return false;

    ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(memory), memoryOffset);

    GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
    GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

    AddResourceCurChunk(memOrigId);
    AddResourceCurChunk(resOrigId);

    // for buffers created with device addresses, fetch it now as that's possible for both EXT and
    // KHR variants now.
    if(bufInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
      VkBufferDeviceAddressInfo getInfo = {
          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
          NULL,
          Unwrap(buffer),
      };

      RDCCOMPILE_ASSERT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO ==
                            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT,
                        "KHR and EXT buffer_device_address should be interchangeable here.");

      if(GetExtensions(GetRecord(device)).ext_KHR_buffer_device_address)
        bufInfo.gpuAddress = ObjDisp(device)->GetBufferDeviceAddress(Unwrap(device), &getInfo);
      else if(GetExtensions(GetRecord(device)).ext_EXT_buffer_device_address)
        bufInfo.gpuAddress = ObjDisp(device)->GetBufferDeviceAddressEXT(Unwrap(device), &getInfo);
      m_CreationInfo.m_BufferAddresses[bufInfo.gpuAddress] = GetResID(buffer);
    }

    m_CreationInfo.m_Memory[GetResID(memory)].BindMemory(memoryOffset, mrq.size,
                                                         VulkanCreationInfo::Memory::Linear);
  }

  return true;
}

VkResult WrappedVulkan::vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                           VkDeviceSize memoryOffset)
{
  VkResourceRecord *record = GetRecord(buffer);

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer),
                                                              Unwrap(memory), memoryOffset));

  CheckVkResult(ret);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindBufferMemory);
      Serialise_vkBindBufferMemory(ser, device, buffer, memory, memoryOffset);

      chunk = scope.Get();
    }

    ResourceId id = GetResID(memory);

    // memory object bindings are immutable and must happen before creation or use,
    // so this can always go into the record, even if a resource is created and bound
    // to memory mid-frame
    record->AddChunk(chunk);

    VkResourceRecord *memrecord = GetRecord(memory);

    record->AddParent(memrecord);
    record->baseResourceMem = record->baseResource = id;
    record->dedicated = memrecord->memMapState->dedicated;
    record->memOffset = memoryOffset;

    memrecord->storable |= record->storable;

    // if the buffer was force-referenced, do the same with the memory
    if(IsForcedReference(record))
    {
      // in case we're currently capturing, immediately consider the buffer and backing memory as
      // read-before-write referenced
      GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
      GetResourceManager()->MarkMemoryFrameReferenced(id, memoryOffset, record->memSize,
                                                      eFrameRef_ReadBeforeWrite);

      memrecord->hasBDA = true;
    }

    // the memory is immediately dirty because we don't use dirty tracking, it's too expensive to
    // follow all frame refs in the background and it's pointless because memory almost always
    // immediately becomes dirty anyway. The one case we might care about non-dirty memory is
    // memory that has been allocated but not used, but that will be skipped or postponed as
    // appropriate.
    GetResourceManager()->MarkDirtyResource(GetResID(memory));
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkBindImageMemory(SerialiserType &ser, VkDevice device, VkImage image,
                                                VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(image).Important();
  SERIALISE_ELEMENT(memory).Important();
  SERIALISE_ELEMENT(memoryOffset).OffsetOrSize();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(image));
    ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(memory));

    VkMemoryRequirements mrq = {};
    ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), &mrq);

    VulkanCreationInfo::Image &imgInfo = m_CreationInfo.m_Image[GetResID(image)];

    bool ok = CheckMemoryRequirements(GetResourceDesc(resOrigId).name.c_str(), GetResID(memory),
                                      memoryOffset, mrq, imgInfo.external, imgInfo.mrq);

    if(!ok)
      return false;

    ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(memory), memoryOffset);

    {
      LockedImageStateRef state = FindImageState(GetResID(image));
      if(!state)
      {
        RDCERR("Binding memory for unknown image %s", ToStr(GetResID(image)).c_str());
      }
      else
      {
        state->isMemoryBound = true;
        state->boundMemory = GetResID(memory);
        state->boundMemoryOffset = memoryOffset;
        state->boundMemorySize = mrq.size;
      }
    }

    GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
    GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

    AddResourceCurChunk(memOrigId);
    AddResourceCurChunk(resOrigId);

    m_CreationInfo.m_Memory[GetResID(memory)].BindMemory(
        memoryOffset, mrq.size,
        imgInfo.linear ? VulkanCreationInfo::Memory::Linear : VulkanCreationInfo::Memory::Tiled);
  }

  return true;
}

VkResult WrappedVulkan::vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem,
                                          VkDeviceSize memOffset)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image),
                                                             Unwrap(mem), memOffset));

  CheckVkResult(ret);

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindImageMemory);
      Serialise_vkBindImageMemory(ser, device, image, mem, memOffset);

      chunk = scope.Get();
    }

    {
      LockedImageStateRef state = FindImageState(GetResID(image));
      if(!state)
        RDCERR("Binding memory to unknown image %s", ToStr(GetResID(image)).c_str());
      else
        state->isMemoryBound = true;
    }

    VkResourceRecord *record = GetRecord(image);

    if(record->resInfo->imageInfo.isAHB)
    {
      VkMemoryRequirements nonExternalMrq = record->resInfo->memreqs;
      ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image),
                                                  &record->resInfo->memreqs);

      VkMemoryRequirements &externalMrq = record->resInfo->memreqs;

      RDCDEBUG(
          "AHB-backed external image requires %llu bytes at %llu alignment, in %x memory types",
          externalMrq.size, externalMrq.alignment, externalMrq.memoryTypeBits);
      RDCDEBUG(
          "Non-external version requires %llu bytes at %llu alignment, in %x memory "
          "types",
          nonExternalMrq.size, nonExternalMrq.alignment, nonExternalMrq.memoryTypeBits);

      externalMrq.size = RDCMAX(externalMrq.size, nonExternalMrq.size);
      externalMrq.alignment = RDCMAX(externalMrq.alignment, nonExternalMrq.alignment);

      if((externalMrq.memoryTypeBits & nonExternalMrq.memoryTypeBits) == 0)
      {
        RDCWARN(
            "External image shares no memory types with non-external image. This image "
            "will not be replayable.");
      }
      else
      {
        externalMrq.memoryTypeBits &= nonExternalMrq.memoryTypeBits;
      }
    }

    // memory object bindings are immutable and must happen before creation or use,
    // so this can always go into the record, even if a resource is created and bound
    // to memory mid-frame
    record->AddChunk(chunk);

    VkResourceRecord *memrecord = GetRecord(mem);

    record->AddParent(memrecord);

    // images are a base resource but we want to track where their memory comes from.
    // Anything that looks up a baseResource for an image knows not to chase further
    // than the image.
    record->baseResourceMem = record->baseResource = memrecord->GetResourceID();
    record->dedicated = memrecord->memMapState->dedicated;
  }
  else
  {
    {
      LockedImageStateRef state = FindImageState(GetResID(image));
      if(!state)
        RDCERR("Binding memory to unknown image %s", ToStr(GetResID(image)).c_str());
      else
        state->isMemoryBound = true;
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateBuffer(SerialiserType &ser, VkDevice device,
                                             const VkBufferCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkBuffer *pBuffer)
{
  VkMemoryRequirements memoryRequirements = {};

  if(ser.IsWriting())
  {
    ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(*pBuffer),
                                                 &memoryRequirements);
  }

  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(*pBuffer)).TypedAs("VkBuffer"_lit);
  // unused at the moment, just for user information
  SERIALISE_ELEMENT(memoryRequirements);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkBuffer buf = VK_NULL_HANDLE;

    VkBufferUsageFlags origusage = CreateInfo.usage;

    // ensure we can always readback from buffers
    CreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // we only need to add TRANSFER_DST_BIT for dedicated buffers, but there's not a reliable way to
    // know if a buffer will be dedicated-allocation or not. We assume that TRANSFER_DST is
    // effectively free as a usage bit for all sensible implementations so we just add it here.
    CreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // remap the queue family indices
    if(CreateInfo.sharingMode == VK_SHARING_MODE_CONCURRENT)
    {
      uint32_t *queueFamiles = (uint32_t *)CreateInfo.pQueueFamilyIndices;
      for(uint32_t q = 0; q < CreateInfo.queueFamilyIndexCount; q++)
        queueFamiles[q] = m_QueueRemapping[queueFamiles[q]][0].family;
    }

    VkBufferCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkBufferCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &patched, NULL, &buf);

    if(CreateInfo.flags &
       (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT))
    {
      APIProps.SparseResources = true;
    }

    CreateInfo.usage = origusage;

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating buffer, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), buf);
      GetResourceManager()->AddLiveResource(Buffer, buf);

      m_CreationInfo.m_Buffer[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo,
                                         memoryRequirements);
    }

    AddResource(Buffer, ResourceType::Buffer, "Buffer");
    DerivedResource(device, Buffer);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *, VkBuffer *pBuffer)
{
  VkBufferCreateInfo adjusted_info = *pCreateInfo;

  // if you change any properties here, ensure you also update
  // vkGetDeviceBufferMemoryRequirementsKHR

  // TEMP HACK: Until we define a portable fake hardware, need to match the requirements for usage
  // on replay, so that the memory requirements are the same
  adjusted_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  // we only need to add TRANSFER_DST_BIT for dedicated buffers, but there's not a reliable way to
  // know if a buffer will be dedicated-allocation or not. We assume that TRANSFER_DST is
  // effectively free as a usage bit for all sensible implementations so we just add it here.
  adjusted_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  if(IsCaptureMode(m_State))
  {
    // If we're using this buffer for AS storage we need to enable BDA
    if(adjusted_info.usage & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
      adjusted_info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // If we're using this buffer for device addresses, ensure we force on capture replay bit.
    // We ensured the physical device can support this feature before whitelisting the extension.
    if(adjusted_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
      adjusted_info.flags |= VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
  }

  byte *tempMem = GetTempMemory(GetNextPatchSize(adjusted_info.pNext));

  UnwrapNextChain(m_State, "VkBufferCreateInfo", tempMem, (VkBaseInStructure *)&adjusted_info);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &adjusted_info, NULL, pBuffer));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pBuffer);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      VkBufferCreateInfo serialisedCreateInfo = *pCreateInfo;
      VkBufferDeviceAddressCreateInfoEXT bufferDeviceAddressEXT = {
          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,
      };
      VkBufferOpaqueCaptureAddressCreateInfo bufferDeviceAddressCoreOrKHR = {
          VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO,
      };

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pBuffer);
      record->memSize = pCreateInfo->size;

      // if we're using VK_[KHR|EXT]_buffer_device_address, we fetch the device address that's been
      // allocated and insert it into the next chain and patch the flags so that it replays
      // naturally.
      if((pCreateInfo->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0)
      {
        VkBufferDeviceAddressInfo getInfo = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            NULL,
            Unwrap(*pBuffer),
        };

        if(GetExtensions(GetRecord(device)).ext_KHR_buffer_device_address)
        {
          bufferDeviceAddressCoreOrKHR.opaqueCaptureAddress =
              ObjDisp(device)->GetBufferOpaqueCaptureAddress(Unwrap(device), &getInfo);

          // we explicitly DON'T assert on this, because some drivers will only need the device
          // address specified at allocate time.
          // RDCASSERT(bufferDeviceAddressKHR.opaqueCaptureAddress);

          // push this struct onto the start of the chain
          bufferDeviceAddressCoreOrKHR.pNext = serialisedCreateInfo.pNext;
          serialisedCreateInfo.pNext = &bufferDeviceAddressCoreOrKHR;
        }
        else if(GetExtensions(GetRecord(device)).ext_EXT_buffer_device_address)
        {
          bufferDeviceAddressEXT.deviceAddress =
              ObjDisp(device)->GetBufferDeviceAddressEXT(Unwrap(device), &getInfo);

          RDCASSERT(bufferDeviceAddressEXT.deviceAddress);

          // push this struct onto the start of the chain
          bufferDeviceAddressEXT.pNext = serialisedCreateInfo.pNext;
          serialisedCreateInfo.pNext = &bufferDeviceAddressEXT;
        }
        else
        {
          RDCERR("Device address bit specified but no device address extension enabled");
        }

        // tell the driver on replay that we're giving it a pre-allocated address to use
        serialisedCreateInfo.flags |= VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;

        // this buffer must be forced to be in any captures, since we can't track when it's used by
        // address
        AddForcedReference(record);

        {
          SCOPED_LOCK(m_DeviceAddressResourcesLock);
          m_DeviceAddressResources.IDs.push_back(record->GetResourceID());
        }
      }

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateBuffer);
        Serialise_vkCreateBuffer(ser, device, &serialisedCreateInfo, NULL, pBuffer);

        chunk = scope.Get();
      }

      record->AddChunk(chunk);

      record->storable = (pCreateInfo->usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) != 0;

      bool isSparse = (pCreateInfo->flags & (VK_BUFFER_CREATE_SPARSE_BINDING_BIT |
                                             VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT)) != 0;

      bool isExternal = FindNextStruct(&adjusted_info,
                                       VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO) != NULL;

      if(isSparse)
      {
        // buffers are always bound opaquely and in arbitrary divisions, sparse residency
        // only means not all the buffer needs to be bound, which is not that interesting for
        // our purposes. We just need to make sure sparse buffers are dirty.
        GetResourceManager()->MarkDirtyResource(id);
      }

      record->resInfo = NULL;

      if(isSparse || isExternal)
      {
        record->resInfo = new ResourceInfo();

        // pre-populate memory requirements
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(*pBuffer),
                                                     &record->resInfo->memreqs);

        // initialise the sparse page table
        if(isSparse)
          record->resInfo->sparseTable.Initialise(pCreateInfo->size,
                                                  record->resInfo->memreqs.alignment & 0xFFFFFFFFU);

        // for external buffers, try creating a non-external version and take the worst case of
        // memory requirements, in case the non-external one (as we will replay it) needs more
        // memory or a stricter alignment
        if(isExternal)
        {
          bool removed =
              RemoveNextStruct(&adjusted_info, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO);

          RDCASSERTMSG("Couldn't find next struct indicating external memory", removed);

          VkBuffer tmpbuf = VK_NULL_HANDLE;
          VkResult vkr = ObjDisp(device)->CreateBuffer(Unwrap(device), &adjusted_info, NULL, &tmpbuf);

          if(vkr == VK_SUCCESS && tmpbuf != VK_NULL_HANDLE)
          {
            VkMemoryRequirements mrq = {};
            ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), tmpbuf, &mrq);

            if(mrq.size > 0)
            {
              RDCDEBUG("External buffer requires %llu bytes at %llu alignment, in %x memory types",
                       record->resInfo->memreqs.size, record->resInfo->memreqs.alignment,
                       record->resInfo->memreqs.memoryTypeBits);
              RDCDEBUG(
                  "Non-external version requires %llu bytes at %llu alignment, in %x memory types",
                  mrq.size, mrq.alignment, mrq.memoryTypeBits);

              record->resInfo->memreqs.size = RDCMAX(record->resInfo->memreqs.size, mrq.size);
              record->resInfo->memreqs.alignment =
                  RDCMAX(record->resInfo->memreqs.alignment, mrq.alignment);
              if((record->resInfo->memreqs.memoryTypeBits & mrq.memoryTypeBits) == 0)
              {
                RDCWARN(
                    "External buffer shares no memory types with non-external buffer. This buffer "
                    "will not be replayable.");
              }
              else
              {
                record->resInfo->memreqs.memoryTypeBits &= mrq.memoryTypeBits;
              }
            }
          }
          else
          {
            RDCERR("Failed to create temporary non-external buffer to find memory requirements: %s",
                   ToStr(vkr).c_str());
          }

          if(tmpbuf != VK_NULL_HANDLE)
            ObjDisp(device)->DestroyBuffer(Unwrap(device), tmpbuf, NULL);
        }
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pBuffer);

      m_CreationInfo.m_Buffer[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo, {});
    }
  }
  else
  {
    CheckVkResult(ret);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateBufferView(SerialiserType &ser, VkDevice device,
                                                 const VkBufferViewCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkBufferView *pView)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(View, GetResID(*pView)).TypedAs("VkBufferView"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkBufferView view = VK_NULL_HANDLE;

    VkBufferViewCreateInfo unwrappedInfo = CreateInfo;
    unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
    VkResult ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &unwrappedInfo, NULL, &view);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating buffer view, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(view)))
      {
        live = GetResourceManager()->GetNonDispWrapper(view)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyBufferView(Unwrap(device), view, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(View, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), view);
        GetResourceManager()->AddLiveResource(View, view);

        m_CreationInfo.m_BufferView[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(View, ResourceType::View, "Buffer View");
    DerivedResource(device, View);
    DerivedResource(CreateInfo.buffer, View);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo,
                                           const VkAllocationCallbacks *, VkBufferView *pView)
{
  VkBufferViewCreateInfo unwrappedInfo = *pCreateInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &unwrappedInfo, NULL, pView));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateBufferView);
        Serialise_vkCreateBufferView(ser, device, pCreateInfo, NULL, pView);

        chunk = scope.Get();
      }

      VkResourceRecord *bufferRecord = GetRecord(pCreateInfo->buffer);

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pView);
      record->AddChunk(chunk);
      record->AddParent(bufferRecord);

      // store the base resource
      record->baseResource = bufferRecord->GetResourceID();
      record->baseResourceMem = bufferRecord->baseResourceMem;
      record->dedicated = bufferRecord->dedicated;
      record->resInfo = bufferRecord->resInfo;
      record->storable = bufferRecord->storable;
      record->memOffset = bufferRecord->memOffset + pCreateInfo->offset;
      record->memSize = pCreateInfo->range;
      if(record->memSize == VK_WHOLE_SIZE)
        record->memSize = bufferRecord->memSize - pCreateInfo->offset;
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pView);

      m_CreationInfo.m_BufferView[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateImage(SerialiserType &ser, VkDevice device,
                                            const VkImageCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
  VkMemoryRequirements memoryRequirements = {};

  if(ser.IsWriting())
  {
    ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(*pImage), &memoryRequirements);
  }

  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Image, GetResID(*pImage)).TypedAs("VkImage"_lit);
  // unused at the moment, just for user information
  SERIALISE_ELEMENT(memoryRequirements);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkImage img = VK_NULL_HANDLE;

    VkImageUsageFlags origusage = CreateInfo.usage;

    // ensure we can always display and copy from/to textures
    CreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    CreateInfo.usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    // remap the queue family indices
    if(CreateInfo.sharingMode == VK_SHARING_MODE_CONCURRENT)
    {
      uint32_t *queueFamiles = (uint32_t *)CreateInfo.pQueueFamilyIndices;
      for(uint32_t q = 0; q < CreateInfo.queueFamilyIndexCount; q++)
        queueFamiles[q] = m_QueueRemapping[queueFamiles[q]][0].family;
    }

    // need to be able to mutate the format for YUV textures
    if(IsYUVFormat(CreateInfo.format))
      CreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    // ensure we can cast multisampled images, for copying to arrays
    if((int)CreateInfo.samples > 1)
    {
      CreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

      // colour targets we do a simple compute copy, for depth-stencil we need
      // to take a slower path that uses drawing
      if(!IsDepthOrStencilFormat(CreateInfo.format))
      {
        // only add STORAGE_BIT if we have an Array2MS pipeline. If it failed to create due to lack
        // of capability or because we disabled it as a workaround then we don't need this
        // capability (and it might be the bug we're trying to work around by disabling the
        // pipeline)
        if(GetShaderCache()->IsBuffer2MSSupported())
          CreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        CreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      }
      else
      {
        CreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }

    // create non-subsampled image to be able to copy its content
    CreateInfo.flags &= ~VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;

    APIProps.YUVTextures |= IsYUVFormat(CreateInfo.format);

    const bool isSparse = (CreateInfo.flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                                               VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)) != 0;

    if(isSparse)
    {
      APIProps.SparseResources = true;
    }

    // we search for the separate stencil usage struct now that it's in patchable memory
    VkImageStencilUsageCreateInfo *separateStencilUsage =
        (VkImageStencilUsageCreateInfo *)FindNextStruct(
            &CreateInfo, VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO);
    if(separateStencilUsage)
    {
      separateStencilUsage->stencilUsage |= VK_IMAGE_USAGE_SAMPLED_BIT |
                                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      separateStencilUsage->stencilUsage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

      if(CreateInfo.samples != VK_SAMPLE_COUNT_1_BIT)
      {
        separateStencilUsage->stencilUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }

    rdcarray<VkFormat> patchedFormatList;

    // similarly for the image format list for MSAA textures, add the UINT cast format we will need
    if(CreateInfo.samples != VK_SAMPLE_COUNT_1_BIT)
    {
      VkImageFormatListCreateInfo *formatListInfo = (VkImageFormatListCreateInfo *)FindNextStruct(
          &CreateInfo, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);

      if(formatListInfo)
      {
        uint32_t bs = (uint32_t)GetByteSize(1, 1, 1, CreateInfo.format, 0);

        VkFormat msaaCopyFormat = VK_FORMAT_UNDEFINED;
        if(bs == 1)
          msaaCopyFormat = VK_FORMAT_R8_UINT;
        else if(bs == 2)
          msaaCopyFormat = VK_FORMAT_R16_UINT;
        else if(bs == 4)
          msaaCopyFormat = VK_FORMAT_R32_UINT;
        else if(bs == 8)
          msaaCopyFormat = VK_FORMAT_R32G32_UINT;
        else if(bs == 16)
          msaaCopyFormat = VK_FORMAT_R32G32B32A32_UINT;

        patchedFormatList.resize(formatListInfo->viewFormatCount + 1);

        const VkFormat *oldFmts = formatListInfo->pViewFormats;
        VkFormat *newFmts = patchedFormatList.data();
        formatListInfo->pViewFormats = newFmts;

        bool needAdded = true;
        uint32_t i = 0;
        for(; i < formatListInfo->viewFormatCount; i++)
        {
          newFmts[i] = oldFmts[i];
          if(newFmts[i] == msaaCopyFormat)
            needAdded = false;
        }

        if(needAdded)
        {
          newFmts[i] = msaaCopyFormat;
          formatListInfo->viewFormatCount++;
        }
      }
    }

    VkImageCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkImageCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), &patched, NULL, &img);

    CreateInfo.usage = origusage;

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating image, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), img);
      GetResourceManager()->AddLiveResource(Image, img);

      NameVulkanObject(img, StringFormat::Fmt("Image %s", ToStr(Image).c_str()));

      m_CreationInfo.m_Image[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo,
                                        memoryRequirements);

      bool inserted = false;
      auto state = InsertImageState(img, live, CreateInfo, eFrameRef_Unknown, &inserted);
      if(!inserted)
      {
        // Image state already existed.
        state->wrappedHandle = img;
        *state = state->InitialState();
      }

      if(isSparse)
        state->isMemoryBound = true;
    }

    rdcstr prefix = "Image";
    rdcstr depth = "Depth";

    if(CreateInfo.format == VK_FORMAT_S8_UINT)
    {
      depth = "Stencil";
    }
    else if(IsStencilFormat(CreateInfo.format))
    {
      depth = "Depth/Stencil";
    }

    if(CreateInfo.imageType == VK_IMAGE_TYPE_1D)
    {
      prefix = CreateInfo.arrayLayers > 1 ? "1D Array Image" : "1D Image";

      if(CreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        prefix = "1D Color Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        prefix = "1D " + depth + " Attachment";
    }
    else if(CreateInfo.imageType == VK_IMAGE_TYPE_2D)
    {
      prefix = CreateInfo.arrayLayers > 1 ? "2D Array Image" : "2D Image";

      if(CreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        prefix = "2D Color Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        prefix = "2D " + depth + " Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT)
        prefix = "2D Fragment Density Map Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)
        prefix = "2D Fragment Shading Rate Attachment";
    }
    else if(CreateInfo.imageType == VK_IMAGE_TYPE_3D)
    {
      prefix = "3D Image";

      if(CreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        prefix = "3D Color Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        prefix = "3D " + depth + " Attachment";
    }

    AddResource(Image, ResourceType::Texture, prefix.c_str());
    DerivedResource(device, Image);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *, VkImage *pImage)
{
  VkImageCreateInfo createInfo_adjusted = *pCreateInfo;

  // We can't process this call if it carries an unknown format
  if(pCreateInfo->format == VK_FORMAT_UNDEFINED)
  {
    RDCERR("Image format undefined");
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }

  // if you change any properties here, ensure you also update
  // vkGetDeviceImageMemoryRequirementsKHR

  createInfo_adjusted.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  // TEMP HACK: Until we define a portable fake hardware, need to match the requirements for usage
  // on replay, so that the memory requirements are the same
  if(IsCaptureMode(m_State))
  {
    createInfo_adjusted.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo_adjusted.usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }

  // need to be able to mutate the format for YUV textures
  if(IsYUVFormat(createInfo_adjusted.format))
    createInfo_adjusted.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

  if(createInfo_adjusted.samples != VK_SAMPLE_COUNT_1_BIT)
  {
    createInfo_adjusted.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo_adjusted.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    // TEMP HACK: matching replay requirements
    if(IsCaptureMode(m_State))
    {
      if(!IsDepthOrStencilFormat(createInfo_adjusted.format))
      {
        // need to check the debug manager here since we might be creating this internal image from
        // its constructor
        if(GetDebugManager() && GetShaderCache()->IsBuffer2MSSupported())
          createInfo_adjusted.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        createInfo_adjusted.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      }
      else
      {
        createInfo_adjusted.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }
  }

  // create non-subsampled image to be able to copy its content
  createInfo_adjusted.flags &= ~VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;

  size_t tempMemSize = GetNextPatchSize(createInfo_adjusted.pNext);

  // reserve space for a patched view format list if necessary
  if(createInfo_adjusted.samples != VK_SAMPLE_COUNT_1_BIT)
  {
    VkImageFormatListCreateInfo *formatListInfo = (VkImageFormatListCreateInfo *)FindNextStruct(
        &createInfo_adjusted, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);

    if(formatListInfo)
      tempMemSize += sizeof(VkFormat) * (formatListInfo->viewFormatCount + 1);
  }

  byte *tempMem = GetTempMemory(tempMemSize);

  UnwrapNextChain(m_State, "VkImageCreateInfo", tempMem, (VkBaseInStructure *)&createInfo_adjusted);

  // we search for the separate stencil usage struct now that it's in patchable memory
  VkImageStencilUsageCreateInfo *separateStencilUsage =
      (VkImageStencilUsageCreateInfo *)FindNextStruct(
          &createInfo_adjusted, VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO);
  if(separateStencilUsage)
  {
    separateStencilUsage->stencilUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if(IsCaptureMode(m_State))
    {
      createInfo_adjusted.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      createInfo_adjusted.usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }

    if(createInfo_adjusted.samples != VK_SAMPLE_COUNT_1_BIT)
    {
      separateStencilUsage->stencilUsage |=
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
  }

  // similarly for the image format list for MSAA textures, add the UINT cast format we will need
  if(createInfo_adjusted.samples != VK_SAMPLE_COUNT_1_BIT)
  {
    VkImageFormatListCreateInfo *formatListInfo = (VkImageFormatListCreateInfo *)FindNextStruct(
        &createInfo_adjusted, VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO);

    if(formatListInfo)
    {
      uint32_t bs = (uint32_t)GetByteSize(1, 1, 1, createInfo_adjusted.format, 0);

      VkFormat msaaCopyFormat = VK_FORMAT_UNDEFINED;
      if(bs == 1)
        msaaCopyFormat = VK_FORMAT_R8_UINT;
      else if(bs == 2)
        msaaCopyFormat = VK_FORMAT_R16_UINT;
      else if(bs == 4)
        msaaCopyFormat = VK_FORMAT_R32_UINT;
      else if(bs == 8)
        msaaCopyFormat = VK_FORMAT_R32G32_UINT;
      else if(bs == 16)
        msaaCopyFormat = VK_FORMAT_R32G32B32A32_UINT;

      const VkFormat *oldFmts = formatListInfo->pViewFormats;
      VkFormat *newFmts = (VkFormat *)tempMem;
      formatListInfo->pViewFormats = newFmts;

      bool needAdded = true;
      uint32_t i = 0;
      for(; i < formatListInfo->viewFormatCount; i++)
      {
        newFmts[i] = oldFmts[i];
        if(newFmts[i] == msaaCopyFormat)
          needAdded = false;
      }

      if(needAdded)
      {
        newFmts[i] = msaaCopyFormat;
        formatListInfo->viewFormatCount++;
      }
    }
  }

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateImage(Unwrap(device), &createInfo_adjusted, NULL, pImage));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pImage);

    const bool isSparse = (pCreateInfo->flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                                                 VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)) != 0;

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateImage);
        Serialise_vkCreateImage(ser, device, pCreateInfo, NULL, pImage);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pImage);
      record->AddChunk(chunk);

      record->resInfo = new ResourceInfo();
      ResourceInfo &resInfo = *record->resInfo;
      resInfo.imageInfo = ImageInfo(*pCreateInfo);

      record->storable = (pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;

      bool isLinear = (pCreateInfo->tiling == VK_IMAGE_TILING_LINEAR);

      bool isExternal = false;

      const VkBaseInStructure *next = (const VkBaseInStructure *)pCreateInfo->pNext;

      // search for external memory image create info struct in pNext chain
      while(next)
      {
        if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV ||
           next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO ||
           next->sType == VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID)
        {
          isExternal = true;

          // we can't call vkGetImageMemoryRequirements on AHB-backed images until they are bound
          if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO)
          {
            VkExternalMemoryImageCreateInfo *extCreateInfo = (VkExternalMemoryImageCreateInfo *)next;
            resInfo.imageInfo.isAHB =
                (extCreateInfo->handleTypes &
                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID) != 0;
            break;
          }
        }

        next = next->pNext;
      }

      // the image is immediately dirty because we don't use dirty tracking, it's too expensive to
      // follow all frame refs in the background and it's pointless because memory almost always
      // immediately becomes dirty anyway. The one case we might care about non-dirty memory is
      // memory that has been allocated but not used, but that will be skipped or postponed as
      // appropriate.
      GetResourceManager()->MarkDirtyResource(id);
      GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_ReadBeforeWrite);

      // pre-populate memory requirements
      if(!resInfo.imageInfo.isAHB)
        ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(*pImage),
                                                    &resInfo.memreqs);

      // sparse and external images should be considered dirty from creation anyway. For sparse
      // images this is so that we can serialise the tracked page table, for external images this is
      // so we can be sure to fetch their contents even if we don't see any writes.
      //
      // We also should consider linear images dirty since we may not get another chance - if they
      // are bound to host-visible memory they may only be updated via memory maps, and we want to
      // be sure to correctly copy their initial contents out rather than relying on memory contents
      // (which may not be valid to map from/into if the image isn't in GENERAL layout).
      if(isSparse || isExternal || isLinear)
      {
        GetResourceManager()->MarkDirtyResource(id);
        GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_ReadBeforeWrite);

        // for external images, try creating a non-external version and take the worst case of
        // memory requirements, in case the non-external one (as we will replay it) needs more
        // memory or a stricter alignment
        if(isExternal)
        {
          bool removed = false;
          removed |= RemoveNextStruct(&createInfo_adjusted,
                                      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV);
          removed |= RemoveNextStruct(&createInfo_adjusted,
                                      VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO);
          removed |=
              RemoveNextStruct(&createInfo_adjusted, VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID);

          RDCASSERTMSG("Couldn't find next struct indicating external memory", removed);

          VkImage tmpimg = VK_NULL_HANDLE;
          VkResult vkr =
              ObjDisp(device)->CreateImage(Unwrap(device), &createInfo_adjusted, NULL, &tmpimg);

          if(vkr == VK_SUCCESS && tmpimg != VK_NULL_HANDLE)
          {
            VkMemoryRequirements mrq = {};
            ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), tmpimg, &mrq);

            // If this is an AHB-backed image its memory requirements will need retrieving after
            // binding.  So just store the non-external data for reference
            // (see WrappedVulkan::vkBindImageMemory)
            if(resInfo.imageInfo.isAHB)
            {
              resInfo.memreqs.size = mrq.size;
              resInfo.memreqs.alignment = mrq.alignment;
              resInfo.memreqs.memoryTypeBits = mrq.memoryTypeBits;

              RDCWARN(
                  "Android hardware buffer backed image, so pre-emptively banning dedicated "
                  "memory");
              resInfo.banDedicated = true;
            }
            else
            {
              if(mrq.size > 0)
              {
                RDCDEBUG("External image requires %llu bytes at %llu alignment, in %x memory types",
                         resInfo.memreqs.size, resInfo.memreqs.alignment,
                         resInfo.memreqs.memoryTypeBits);
                RDCDEBUG(
                    "Non-external version requires %llu bytes at %llu alignment, in %x memory "
                    "types",
                    mrq.size, mrq.alignment, mrq.memoryTypeBits);

                if(resInfo.memreqs.size != mrq.size)
                {
                  RDCWARN(
                      "Required size changed on image between external/non-external, banning "
                      "dedicated memory");
                  resInfo.banDedicated = true;
                }

                resInfo.memreqs.size = RDCMAX(resInfo.memreqs.size, mrq.size);
                resInfo.memreqs.alignment = RDCMAX(resInfo.memreqs.alignment, mrq.alignment);

                if((resInfo.memreqs.memoryTypeBits & mrq.memoryTypeBits) == 0)
                {
                  RDCWARN(
                      "External image shares no memory types with non-external image. This image "
                      "will not be replayable.");
                }
                else
                {
                  resInfo.memreqs.memoryTypeBits &= mrq.memoryTypeBits;
                }
              }
            }
          }
          else
          {
            RDCERR("Failed to create temporary non-external image to find memory requirements: %s",
                   ToStr(vkr).c_str());
          }

          if(tmpimg != VK_NULL_HANDLE)
            ObjDisp(device)->DestroyImage(Unwrap(device), tmpimg, NULL);
        }
      }

      if(isSparse)
      {
        uint32_t pageByteSize = resInfo.memreqs.alignment & 0xFFFFFFFFu;

        if(pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
        {
          // must record image and page dimension, and create page tables
          uint32_t numreqs = 8;
          VkSparseImageMemoryRequirements reqs[8];
          ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(*pImage),
                                                            &numreqs, reqs);

          // we only support at most DEPTH, STENCIL, METADATA = 3 aspects
          RDCASSERT(numreqs > 0 && numreqs <= 3, numreqs);

          // if we don't have just a single
          resInfo.altSparseAspects.resize(numreqs - 1);

          Sparse::Coord dim = {pCreateInfo->extent.width, pCreateInfo->extent.height,
                               pCreateInfo->extent.depth};

          for(uint32_t r = 0; r < numreqs; r++)
          {
            if(r == 0)
              resInfo.sparseAspect = reqs[r].formatProperties.aspectMask;
            else
              resInfo.altSparseAspects[r - 1].aspectMask = reqs[r].formatProperties.aspectMask;

            Sparse::PageTable &table =
                r == 0 ? resInfo.sparseTable : resInfo.altSparseAspects[r - 1].table;

            bool singleMipTail =
                (reqs[r].formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) != 0;

            const VkExtent3D &gran = reqs[r].formatProperties.imageGranularity;
            Sparse::Coord pageSize = {gran.width, gran.height, gran.depth};

            table.Initialise(
                dim, pCreateInfo->mipLevels, pCreateInfo->arrayLayers, pageByteSize, pageSize,
                // we MIN here so if the driver returns 999 we have a consistent value, so we can
                // compare against it on replay
                RDCMIN(reqs[r].imageMipTailFirstLod, pCreateInfo->mipLevels),
                reqs[r].imageMipTailOffset,
                // if formatProperties.flags does not contain
                // VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT (otherwise the value is undefined).
                singleMipTail || pCreateInfo->arrayLayers == 0 ? 0 : reqs[r].imageMipTailStride,
                // If formatProperties.flags contains VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT,
                // this is the size of the whole mip tail, otherwise this is the size of the mip
                // tail of a single array layer.
                singleMipTail ? reqs[r].imageMipTailSize
                              : reqs[r].imageMipTailSize * pCreateInfo->arrayLayers);
          }
        }
        else
        {
          // set page table up as if it were a buffer
          resInfo.sparseTable.Initialise(resInfo.memreqs.size, pageByteSize);
        }
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pImage);

      m_CreationInfo.m_Image[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo, {});
    }

    LockedImageStateRef state =
        InsertImageState(*pImage, id, ImageInfo(*pCreateInfo), eFrameRef_None);

    // sparse resources are always treated as if memory is bound, don't skip anything
    if(isSparse)
      state->isMemoryBound = true;
  }
  else
  {
    CheckVkResult(ret);
  }

  return ret;
}

void WrappedVulkan::PatchImageViewUsage(VkImageViewUsageCreateInfo *usage, VkFormat imgFormat,
                                        VkSampleCountFlagBits samples)
{
  // this matches the mutations we do to images, so see vkCreateImage
  usage->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  usage->usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  usage->usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

  if(samples != VK_SAMPLE_COUNT_1_BIT)
  {
    usage->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if(!IsDepthOrStencilFormat(imgFormat))
    {
      if(GetDebugManager() && GetShaderCache()->IsBuffer2MSSupported())
        usage->usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else
    {
      usage->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
  }
}

// Image view functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateImageView(SerialiserType &ser, VkDevice device,
                                                const VkImageViewCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkImageView *pView)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(View, GetResID(*pView)).TypedAs("VkImageView"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkImageView view = VK_NULL_HANDLE;

    byte *tempMem = GetTempMemory(GetNextPatchSize(&CreateInfo));
    VkImageViewCreateInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, &CreateInfo);

    VkImageViewUsageCreateInfo *usageInfo = (VkImageViewUsageCreateInfo *)FindNextStruct(
        unwrappedInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);

    if(usageInfo)
    {
      VkSampleCountFlagBits samples = m_CreationInfo.m_Image[GetResID(CreateInfo.image)].samples;

      PatchImageViewUsage(usageInfo, CreateInfo.format, samples);
    }

    VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), unwrappedInfo, NULL, &view);

    APIProps.YUVTextures |= IsYUVFormat(CreateInfo.format);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating image view, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(view)))
      {
        live = GetResourceManager()->GetNonDispWrapper(view)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyImageView(Unwrap(device), view, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(View, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), view);
        GetResourceManager()->AddLiveResource(View, view);

        m_CreationInfo.m_ImageView[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(View, ResourceType::View, "Image View");
    DerivedResource(device, View);
    DerivedResource(CreateInfo.image, View);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkImageView *pView)
{
  byte *tempMem = GetTempMemory(GetNextPatchSize(pCreateInfo));
  VkImageViewCreateInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pCreateInfo);

  VkImageViewUsageCreateInfo *usageInfo = (VkImageViewUsageCreateInfo *)FindNextStruct(
      unwrappedInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);

  if(usageInfo)
  {
    VkSampleCountFlagBits samples;
    if(IsCaptureMode(m_State))
      samples = (VkSampleCountFlagBits)GetRecord(pCreateInfo->image)->resInfo->imageInfo.sampleCount;
    else
      samples = m_CreationInfo.m_Image[GetResID(pCreateInfo->image)].samples;

    PatchImageViewUsage(usageInfo, pCreateInfo->format, samples);
  }

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateImageView(Unwrap(device), unwrappedInfo, NULL, pView));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateImageView);
        Serialise_vkCreateImageView(ser, device, pCreateInfo, NULL, pView);

        chunk = scope.Get();
      }

      VkResourceRecord *imageRecord = GetRecord(pCreateInfo->image);

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pView);
      record->AddChunk(chunk);
      record->AddParent(imageRecord);

      // store the base resource. Note images have a baseResource pointing
      // to their memory, which we will also need so we store that separately
      record->baseResource = imageRecord->GetResourceID();
      record->baseResourceMem = imageRecord->baseResourceMem;
      record->dedicated = imageRecord->dedicated;
      record->resInfo = imageRecord->resInfo;
      record->viewRange = pCreateInfo->subresourceRange;
      record->viewRange.setViewType(pCreateInfo->viewType);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pView);

      m_CreationInfo.m_ImageView[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkBindBufferMemory2(SerialiserType &ser, VkDevice device,
                                                  uint32_t bindInfoCount,
                                                  const VkBindBufferMemoryInfo *pBindInfos)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(bindInfoCount);
  SERIALISE_ELEMENT_ARRAY(pBindInfos, bindInfoCount).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    rdcarray<VkMemoryRequirements> mrqs;
    mrqs.resize(bindInfoCount);
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      const VkBindBufferMemoryInfo &bindInfo = pBindInfos[i];
      const VulkanCreationInfo::Buffer &bufInfo = m_CreationInfo.m_Buffer[GetResID(bindInfo.buffer)];

      ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.buffer));

      ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(bindInfo.buffer), &mrqs[i]);

      bool ok = CheckMemoryRequirements(GetResourceDesc(resOrigId).name.c_str(),
                                        GetResID(bindInfo.memory), bindInfo.memoryOffset, mrqs[i],
                                        bufInfo.external, bufInfo.mrq);

      if(!ok)
        return false;
    }

    VkBindBufferMemoryInfo *unwrapped = UnwrapInfos(m_State, pBindInfos, bindInfoCount);
    ObjDisp(device)->BindBufferMemory2(Unwrap(device), bindInfoCount, unwrapped);

    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      const VkBindBufferMemoryInfo &bindInfo = pBindInfos[i];

      ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.buffer));
      ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.memory));

      VulkanCreationInfo::Buffer &bufInfo = m_CreationInfo.m_Buffer[GetResID(bindInfo.buffer)];

      GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
      GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

      AddResourceCurChunk(memOrigId);
      AddResourceCurChunk(resOrigId);

      // for buffers created with device addresses, fetch it now as that's possible for both EXT and
      // KHR variants now.
      if(bufInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
      {
        VkBufferDeviceAddressInfo getInfo = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            NULL,
            Unwrap(bindInfo.buffer),
        };

        if(GetExtensions(GetRecord(device)).ext_KHR_buffer_device_address)
          bufInfo.gpuAddress = ObjDisp(device)->GetBufferDeviceAddress(Unwrap(device), &getInfo);
        else if(GetExtensions(GetRecord(device)).ext_EXT_buffer_device_address)
          bufInfo.gpuAddress = ObjDisp(device)->GetBufferDeviceAddressEXT(Unwrap(device), &getInfo);
        m_CreationInfo.m_BufferAddresses[bufInfo.gpuAddress] = GetResID(bindInfo.buffer);
      }

      // the memory is immediately dirty because we don't use dirty tracking, it's too expensive to
      // follow all frame refs in the background and it's pointless because memory almost always
      // immediately becomes dirty anyway. The one case we might care about non-dirty memory is
      // memory that has been allocated but not used, but that will be skipped or postponed as
      // appropriate.
      GetResourceManager()->MarkDirtyResource(GetResID(bindInfo.memory));

      m_CreationInfo.m_Memory[GetResID(bindInfo.memory)].BindMemory(
          bindInfo.memoryOffset, mrqs[i].size, VulkanCreationInfo::Memory::Linear);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount,
                                            const VkBindBufferMemoryInfo *pBindInfos)
{
  VkBindBufferMemoryInfo *unwrapped = UnwrapInfos(m_State, pBindInfos, bindInfoCount);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->BindBufferMemory2(Unwrap(device), bindInfoCount, unwrapped));

  CheckVkResult(ret);

  if(IsCaptureMode(m_State))
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      VkResourceRecord *bufrecord = GetRecord(pBindInfos[i].buffer);
      VkResourceRecord *memrecord = GetRecord(pBindInfos[i].memory);

      Chunk *chunk = NULL;

      // we split this batch-bind up, so that each bind goes into the right record
      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindBufferMemory2);
        Serialise_vkBindBufferMemory2(ser, device, 1, pBindInfos + i);

        chunk = scope.Get();
      }

      // memory object bindings are immutable and must happen before creation or use,
      // so this can always go into the record, even if a resource is created and bound
      // to memory mid-frame
      bufrecord->AddChunk(chunk);

      bufrecord->AddParent(memrecord);
      bufrecord->baseResourceMem = bufrecord->baseResource = memrecord->GetResourceID();
      bufrecord->dedicated = memrecord->memMapState->dedicated;
      bufrecord->memOffset = pBindInfos[i].memoryOffset;

      memrecord->storable |= bufrecord->storable;

      // if the buffer was force-referenced, do the same with the memory
      if(IsForcedReference(bufrecord))
      {
        // in case we're currently capturing, immediately consider the buffer and backing memory as
        // read-before-write referenced
        GetResourceManager()->MarkResourceFrameReferenced(bufrecord->GetResourceID(), eFrameRef_Read);
        GetResourceManager()->MarkMemoryFrameReferenced(
            GetResID(pBindInfos[i].memory), pBindInfos[i].memoryOffset, bufrecord->memSize,
            eFrameRef_ReadBeforeWrite);
      }

      // the memory is immediately dirty because we don't use dirty tracking, it's too expensive to
      // follow all frame refs in the background and it's pointless because memory almost always
      // immediately becomes dirty anyway. The one case we might care about non-dirty memory is
      // memory that has been allocated but not used, but that will be skipped or postponed as
      // appropriate.
      GetResourceManager()->MarkDirtyResource(GetResID(pBindInfos[i].memory));
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkBindImageMemory2(SerialiserType &ser, VkDevice device,
                                                 uint32_t bindInfoCount,
                                                 const VkBindImageMemoryInfo *pBindInfos)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(bindInfoCount);
  SERIALISE_ELEMENT_ARRAY(pBindInfos, bindInfoCount).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      const VkBindImageMemoryInfo &bindInfo = pBindInfos[i];

      ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.image));
      ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.memory));

      VulkanCreationInfo::Image &imgInfo = m_CreationInfo.m_Image[GetResID(bindInfo.image)];

      VkMemoryRequirements mrq = {};
      ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(bindInfo.image), &mrq);

      bool ok = CheckMemoryRequirements(GetResourceDesc(resOrigId).name.c_str(),
                                        GetResID(bindInfo.memory), bindInfo.memoryOffset, mrq,
                                        imgInfo.external, imgInfo.mrq);

      if(!ok)
        return false;

      {
        ResourceId id = GetResID(bindInfo.image);
        LockedImageStateRef state = FindImageState(id);
        if(!state)
        {
          RDCERR("Binding memory for unknown image %s", ToStr(id).c_str());
        }
        else
        {
          state->isMemoryBound = true;
          state->boundMemory = GetResID(bindInfo.memory);
          state->boundMemoryOffset = bindInfo.memoryOffset;
          state->boundMemorySize = mrq.size;
        }
      }

      GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
      GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

      AddResourceCurChunk(memOrigId);
      AddResourceCurChunk(resOrigId);

      m_CreationInfo.m_Memory[GetResID(bindInfo.memory)].BindMemory(
          bindInfo.memoryOffset, mrq.size,
          imgInfo.linear ? VulkanCreationInfo::Memory::Linear : VulkanCreationInfo::Memory::Tiled);
    }

    VkBindImageMemoryInfo *unwrapped = UnwrapInfos(m_State, pBindInfos, bindInfoCount);
    ObjDisp(device)->BindImageMemory2(Unwrap(device), bindInfoCount, unwrapped);
  }

  return true;
}

VkResult WrappedVulkan::vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount,
                                           const VkBindImageMemoryInfo *pBindInfos)
{
  VkBindImageMemoryInfo *unwrapped = UnwrapInfos(m_State, pBindInfos, bindInfoCount);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->BindImageMemory2(Unwrap(device), bindInfoCount, unwrapped));

  CheckVkResult(ret);

  if(IsCaptureMode(m_State))
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      VkResourceRecord *imgrecord = GetRecord(pBindInfos[i].image);
      VkResourceRecord *memrecord = GetRecord(pBindInfos[i].memory);

      Chunk *chunk = NULL;

      // we split this batch-bind up, so that each bind goes into the right record
      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindImageMemory2);
        Serialise_vkBindImageMemory2(ser, device, 1, pBindInfos + i);

        chunk = scope.Get();
      }

      {
        ResourceId id = imgrecord->GetResourceID();
        LockedImageStateRef state = FindImageState(id);
        if(!state)
          RDCERR("Binding memory for unknown image %s", ToStr(id).c_str());
        else
          state->isMemoryBound = true;
      }

      if(imgrecord->resInfo->imageInfo.isAHB)
      {
        VkMemoryRequirements nonExternalMrq = imgrecord->resInfo->memreqs;
        ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(pBindInfos[i].image),
                                                    &imgrecord->resInfo->memreqs);

        VkMemoryRequirements &externalMrq = imgrecord->resInfo->memreqs;

        RDCDEBUG(
            "AHB-backed external image requires %llu bytes at %llu alignment, in %x memory types",
            externalMrq.size, externalMrq.alignment, externalMrq.memoryTypeBits);
        RDCDEBUG(
            "Non-external version requires %llu bytes at %llu alignment, in %x memory "
            "types",
            nonExternalMrq.size, nonExternalMrq.alignment, nonExternalMrq.memoryTypeBits);

        externalMrq.size = RDCMAX(externalMrq.size, nonExternalMrq.size);
        externalMrq.alignment = RDCMAX(externalMrq.alignment, nonExternalMrq.alignment);

        if((externalMrq.memoryTypeBits & nonExternalMrq.memoryTypeBits) == 0)
        {
          RDCWARN(
              "External image shares no memory types with non-external image. This image "
              "will not be replayable.");
        }
        else
        {
          externalMrq.memoryTypeBits &= nonExternalMrq.memoryTypeBits;
        }
      }

      // memory object bindings are immutable and must happen before creation or use,
      // so this can always go into the record, even if a resource is created and bound
      // to memory mid-frame
      imgrecord->AddChunk(chunk);

      imgrecord->AddParent(memrecord);

      // images are a base resource but we want to track where their memory comes from.
      // Anything that looks up a baseResource for an image knows not to chase further
      // than the image.
      imgrecord->baseResourceMem = imgrecord->baseResource = memrecord->GetResourceID();
      imgrecord->dedicated = memrecord->memMapState->dedicated;
    }
  }
  else
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      LockedImageStateRef state = FindImageState(GetResID(pBindInfos[i].image));
      if(!state)
        state->isMemoryBound = true;
      else
        RDCERR("Binding memory to unknown image %s", ToStr(GetResID(pBindInfos[i].image)).c_str());
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkSetDeviceMemoryPriorityEXT(SerialiserType &ser, VkDevice device,
                                                           VkDeviceMemory memory, float priority)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(memory);
  SERIALISE_ELEMENT(priority);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(device)->SetDeviceMemoryPriorityEXT(Unwrap(device), Unwrap(memory), priority);

    AddResourceCurChunk(GetResourceManager()->GetOriginalID(GetResID(memory)));
  }

  return true;
}

void WrappedVulkan::vkSetDeviceMemoryPriorityEXT(VkDevice device, VkDeviceMemory memory,
                                                 float priority)
{
  SERIALISE_TIME_CALL(
      ObjDisp(device)->SetDeviceMemoryPriorityEXT(Unwrap(device), Unwrap(memory), priority));

  // deliberately only serialise this while idle. If we serialised it while active we'd need
  // arguably to track the priority to restore it each replay, and there's a high chance that during
  // capture the program might start setting priorities in response to the overhead of capturing
  if(IsBackgroundCapturing(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkSetDeviceMemoryPriorityEXT);
      Serialise_vkSetDeviceMemoryPriorityEXT(ser, device, memory, priority);

      chunk = scope.Get();
    }

    VkResourceRecord *r = GetRecord(memory);

    // remove any previous memory priority chunks on the tail of the list. Memory records will not
    // have anything else in here so this keeps redundancy to a minimum
    r->LockChunks();
    for(;;)
    {
      Chunk *end = r->GetLastChunk();

      if(end->GetChunkType<VulkanChunk>() == VulkanChunk::vkSetDeviceMemoryPriorityEXT)
      {
        end->Delete();
        r->PopChunk();
        continue;
      }

      break;
    }
    r->UnlockChunks();

    r->AddChunk(chunk);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateAccelerationStructureKHR(
    SerialiserType &ser, VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(AccelerationStructure, GetResID(*pAccelerationStructure))
      .TypedAs("VkAccelerationStructureKHR"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkAccelerationStructureCreateInfoKHR unwrappedInfo = CreateInfo;
    unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);

    VkAccelerationStructureKHR acc = VK_NULL_HANDLE;
    VkResult ret =
        ObjDisp(device)->CreateAccelerationStructureKHR(Unwrap(device), &unwrappedInfo, NULL, &acc);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating acceleration structure, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(acc)))
      {
        live = GetResourceManager()->GetNonDispWrapper(acc)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyAccelerationStructureKHR(Unwrap(device), acc, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(AccelerationStructure,
                                              GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), acc);
        GetResourceManager()->AddLiveResource(AccelerationStructure, acc);

        m_CreationInfo.m_AccelerationStructure[live].Init(GetResourceManager(), m_CreationInfo,
                                                          &CreateInfo);
      }
    }

    AddResource(AccelerationStructure, ResourceType::AccelerationStructure, "AccelerationStructure");
    DerivedResource(device, AccelerationStructure);
    DerivedResource(CreateInfo.buffer, AccelerationStructure);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateAccelerationStructureKHR(
    VkDevice device, const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
    const VkAllocationCallbacks *, VkAccelerationStructureKHR *pAccelerationStructure)
{
  VkAccelerationStructureCreateInfoKHR unwrappedInfo = *pCreateInfo;

  // Ensure we force on capture replay bit. We ensured the physical device can support this feature
  // before whitelisting the extension.
  if(IsCaptureMode(m_State))
    unwrappedInfo.createFlags |=
        VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR;

  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateAccelerationStructureKHR(
                          Unwrap(device), &unwrappedInfo, NULL, pAccelerationStructure));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pAccelerationStructure);

    if(IsCaptureMode(m_State))
    {
      // We're capturing, so get the device address of the created AS
      VkAccelerationStructureCreateInfoKHR serialisedCreateInfo = *pCreateInfo;
      serialisedCreateInfo.createFlags |=
          VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR;

      const VkAccelerationStructureDeviceAddressInfoKHR getInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
          NULL,
          Unwrap(*pAccelerationStructure),
      };
      const VkDeviceAddress addr =
          ObjDisp(device)->GetAccelerationStructureDeviceAddressKHR(Unwrap(device), &getInfo);
      serialisedCreateInfo.deviceAddress = addr;

      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateAccelerationStructureKHR);
        Serialise_vkCreateAccelerationStructureKHR(ser, device, &serialisedCreateInfo, NULL,
                                                   pAccelerationStructure);

        chunk = scope.Get();
      }

      VkResourceRecord *bufferRecord = GetRecord(pCreateInfo->buffer);

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pAccelerationStructure);
      record->AddChunk(chunk);
      record->AddParent(bufferRecord);

      // store the base resource
      record->baseResource = bufferRecord->GetResourceID();
      record->baseResourceMem = bufferRecord->baseResource;
      record->dedicated = bufferRecord->dedicated;
      record->resInfo = bufferRecord->resInfo;
      record->storable = bufferRecord->storable;
      record->memOffset = bufferRecord->memOffset + pCreateInfo->offset;
      record->memSize = pCreateInfo->size;

      GetResourceManager()->MarkDirtyResource(id);
      if(pCreateInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR ||
         pCreateInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR)
      {
        // We force reference BLASs as it is not feasible to track at the API level which TLASs
        // reference them.  We force ref generics too as they could bottom or top level so we
        // conservatively assume they are bottom
        AddForcedReference(record);
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pAccelerationStructure);

      m_CreationInfo.m_AccelerationStructure[id].Init(GetResourceManager(), m_CreationInfo,
                                                      pCreateInfo);
    }
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkAllocateMemory, VkDevice device,
                                const VkMemoryAllocateInfo *pAllocateInfo,
                                const VkAllocationCallbacks *, VkDeviceMemory *pMemory);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUnmapMemory, VkDevice device, VkDeviceMemory memory);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkFlushMappedMemoryRanges, VkDevice device,
                                uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory, VkDevice device, VkBuffer buffer,
                                VkDeviceMemory memory, VkDeviceSize memoryOffset);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindImageMemory, VkDevice device, VkImage image,
                                VkDeviceMemory memory, VkDeviceSize memoryOffset);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateBuffer, VkDevice device,
                                const VkBufferCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkBuffer *pBuffer);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateBufferView, VkDevice device,
                                const VkBufferViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkBufferView *pView);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateImage, VkDevice device,
                                const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *,
                                VkImage *pImage);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateImageView, VkDevice device,
                                const VkImageViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkImageView *pView);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory2, VkDevice device,
                                uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindImageMemory2, VkDevice device,
                                uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos);

INSTANTIATE_FUNCTION_SERIALISED(void, vkSetDeviceMemoryPriorityEXT, VkDevice device,
                                VkDeviceMemory memory, float priority);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateAccelerationStructureKHR, VkDevice device,
                                const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
                                const VkAllocationCallbacks *,
                                VkAccelerationStructureKHR *pAccelerationStructure);
