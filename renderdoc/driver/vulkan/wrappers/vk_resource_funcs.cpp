/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "../vk_core.h"
#include "../vk_debug.h"

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
VkBindBufferMemoryInfo *WrappedVulkan::UnwrapInfos(const VkBindBufferMemoryInfo *info, uint32_t count)
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
VkBindImageMemoryInfo *WrappedVulkan::UnwrapInfos(const VkBindImageMemoryInfo *info, uint32_t count)
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
                                            VkDeviceSize memoryOffset, VkMemoryRequirements mrq)
{
  // verify that the memory meets basic requirements. If not, something changed and we should
  // bail loading this capture. This is a bit of an under-estimate since we just make sure
  // there's enough space left in the memory, that doesn't mean that there aren't overlaps due
  // to increased size requirements.
  ResourceId memOrigId = GetResourceManager()->GetOriginalID(memId);

  VulkanCreationInfo::Memory &memInfo = m_CreationInfo.m_Memory[memId];
  uint32_t bit = 1U << memInfo.memoryTypeIndex;

  // verify type
  if((mrq.memoryTypeBits & bit) == 0)
  {
    std::string bitsString;

    for(uint32_t i = 0; i < 32; i++)
    {
      if(mrq.memoryTypeBits & (1U << i))
        bitsString += StringFormat::Fmt("%s%u", bitsString.empty() ? "" : ", ", i);
    }

    RDCERR(
        "Trying to bind %s to memory %llu which is type %u, "
        "but only these types are allowed: %s\n"
        "This is most likely caused by incompatible hardware or drivers between capture and "
        "replay, causing a change in memory requirements.",
        resourceName, memOrigId, memInfo.memoryTypeIndex, bitsString.c_str());
    m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported;
    return false;
  }

  // verify offset alignment
  if((memoryOffset % mrq.alignment) != 0)
  {
    RDCERR(
        "Trying to bind %s to memory %llu which is type %u, "
        "but offset 0x%llx doesn't satisfy alignment 0x%llx.\n"
        "This is most likely caused by incompatible hardware or drivers between capture and "
        "replay, causing a change in memory requirements.",
        resourceName, memOrigId, memInfo.memoryTypeIndex, memoryOffset, mrq.alignment);
    m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported;
    return false;
  }

  // verify size
  if(mrq.size > memInfo.size - memoryOffset)
  {
    RDCERR(
        "Trying to bind %s to memory %llu which is type %u, "
        "but at offset 0x%llx the reported size of 0x%llx won't fit the 0x%llx bytes of memory.\n"
        "This is most likely caused by incompatible hardware or drivers between capture and "
        "replay, causing a change in memory requirements.",
        resourceName, memOrigId, memInfo.memoryTypeIndex, memoryOffset, mrq.size, memInfo.size);
    m_FailedReplayStatus = ReplayStatus::APIHardwareUnsupported;
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
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Memory, GetResID(*pMemory)).TypedAs("VkDeviceMemory"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDeviceMemory mem = VK_NULL_HANDLE;

    // serialised memory type index is non-remapped, so we remap now.
    // PORTABILITY may need to re-write info to change memory type index to the
    // appropriate index on replay
    AllocateInfo.memoryTypeIndex = m_PhysicalDeviceData.memIdxMap[AllocateInfo.memoryTypeIndex];

    VkMemoryAllocateInfo patched = AllocateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkMemoryAllocateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->AllocateMemory(Unwrap(device), &patched, NULL, &mem);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), mem);
      GetResourceManager()->AddLiveResource(Memory, mem);

      m_CreationInfo.m_Memory[live].Init(GetResourceManager(), m_CreationInfo, &AllocateInfo);

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

      // we already validated at replay time that the memory size is aligned/etc as necessary so we
      // can create a buffer of the whole size, but just to keep the validation layers happy let's
      // check the requirements here again.
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
        RDCWARN("Can't create buffer covering memory allocation %llu", Memory);
        ObjDisp(device)->DestroyBuffer(Unwrap(device), buf, NULL);

        m_CreationInfo.m_Memory[live].wholeMemBuf = VK_NULL_HANDLE;
      }
    }

    AddResource(Memory, ResourceType::Memory, "Memory");
    DerivedResource(device, Memory);
  }

  return true;
}

VkResult WrappedVulkan::vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                         const VkAllocationCallbacks *pAllocator,
                                         VkDeviceMemory *pMemory)
{
  VkMemoryAllocateInfo info = *pAllocateInfo;
  if(IsCaptureMode(m_State))
    info.memoryTypeIndex = GetRecord(device)->memIdxMap[info.memoryTypeIndex];

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
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

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

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->AllocateMemory(Unwrap(device), &unwrapped, pAllocator, pMemory));

  // restore the memoryTypeIndex to the original, as that's what we want to serialise,
  // but maintain any potential modifications we made to info.allocationSize
  info.memoryTypeIndex = pAllocateInfo->memoryTypeIndex;

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pMemory);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkAllocateMemory);
        Serialise_vkAllocateMemory(ser, device, &info, NULL, pMemory);

        chunk = scope.Get();
      }

      // create resource record for gpu memory
      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pMemory);
      RDCASSERT(record);

      record->AddChunk(chunk);

      record->Length = info.allocationSize;

      uint32_t memProps =
          m_PhysicalDeviceData.fakeMemProps->memoryTypes[info.memoryTypeIndex].propertyFlags;

      // if memory is not host visible, so not mappable, don't create map state at all
      if((memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
      {
        record->memMapState = new MemMapState();
        record->memMapState->mapCoherent = (memProps & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        record->memMapState->refData = NULL;
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pMemory);

      m_CreationInfo.m_Memory[id].Init(GetResourceManager(), m_CreationInfo, &info);

      // create a buffer with the whole memory range bound, for copying to and from
      // conveniently (for initial state data)
      VkBuffer buf = VK_NULL_HANDLE;

      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          NULL,
          0,
          info.allocationSize,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      };

      ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &bufInfo, NULL, &buf);
      RDCASSERTEQUAL(ret, VK_SUCCESS);

      // we already validated above that the memory size is aligned/etc as necessary so we can
      // create a buffer of the whole size, but just to keep the validation layers happy let's check
      // the requirements here again.
      VkMemoryRequirements mrq = {};
      ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), buf, &mrq);

      RDCASSERTEQUAL(mrq.size, info.allocationSize);

      ResourceId bufid = GetResourceManager()->WrapResource(Unwrap(device), buf);

      ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buf), Unwrap(*pMemory), 0);

      // register as a live-only resource, so it is cleaned up properly
      GetResourceManager()->AddLiveResource(bufid, buf);

      m_CreationInfo.m_Memory[id].wholeMemBuf = buf;
    }
  }

  return ret;
}

void WrappedVulkan::vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                 const VkAllocationCallbacks *pAllocator)
{
  if(memory == VK_NULL_HANDLE)
    return;

  // we just need to clean up after ourselves on replay
  WrappedVkNonDispRes *wrapped = (WrappedVkNonDispRes *)GetWrapped(memory);

  VkDeviceMemory unwrappedMem = wrapped->real.As<VkDeviceMemory>();

  if(IsCaptureMode(m_State))
  {
    // there is an implicit unmap on free, so make sure to tidy up
    if(wrapped->record->memMapState && wrapped->record->memMapState->refData)
    {
      FreeAlignedBuffer(wrapped->record->memMapState->refData);
      wrapped->record->memMapState->refData = NULL;
    }

    {
      SCOPED_LOCK(m_CoherentMapsLock);

      auto it = std::find(m_CoherentMaps.begin(), m_CoherentMaps.end(), wrapped->record);
      if(it != m_CoherentMaps.end())
        m_CoherentMaps.erase(it);
    }
  }

  m_ForcedReferences.erase(GetResID(memory));
  m_CreationInfo.erase(GetResID(memory));

  GetResourceManager()->ReleaseWrappedResource(memory);

  ObjDisp(device)->FreeMemory(Unwrap(device), unwrappedMem, pAllocator);
}

VkResult WrappedVulkan::vkMapMemory(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
  void *realData = NULL;
  VkResult ret =
      ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), offset, size, flags, &realData);

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
      RDCASSERT(size == VK_WHOLE_SIZE || (size > 0 && size <= memrecord->Length), GetResID(mem),
                size, memrecord->Length);

      state.mappedPtr = (byte *)realData - (size_t)offset;
      state.refData = NULL;

      state.mapOffset = offset;
      state.mapSize = size == VK_WHOLE_SIZE ? (memrecord->Length - offset) : size;
      state.mapFlushed = false;

      *ppData = realData;

      if(state.mapCoherent)
      {
        SCOPED_LOCK(m_CoherentMapsLock);
        m_CoherentMaps.push_back(memrecord);
      }
    }
    else
    {
      *ppData = realData;
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
  SERIALISE_ELEMENT(memory);

  uint64_t MapOffset = 0;
  uint64_t MapSize = 0;
  byte *MapData = NULL;

  MemMapState *state = NULL;
  if(IsCaptureMode(m_State))
  {
    state = GetRecord(memory)->memMapState;

    MapOffset = state->mapOffset;
    MapSize = state->mapSize;

    MapData = (byte *)state->mappedPtr + MapOffset;
  }

  SERIALISE_ELEMENT(MapOffset);
  SERIALISE_ELEMENT(MapSize);

  if(IsReplayingAndReading() && memory != VK_NULL_HANDLE)
  {
    VkResult vkr = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(memory), MapOffset, MapSize, 0,
                                              (void **)&MapData);
    if(vkr != VK_SUCCESS)
      RDCERR("Error mapping memory on replay: %s", ToStr(vkr).c_str());
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("MapData"_lit, MapData, MapSize, SerialiserFlags::NoFlags);

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

    {
      // decide atomically if this chunk should be in-frame or not
      // so that we're not in the else branch but haven't marked
      // dirty when capframe starts, then we mark dirty while in-frame

      bool capframe = false;
      {
        SCOPED_LOCK(m_CapTransitionLock);
        capframe = IsActiveCapturing(m_State);

        if(!capframe)
          GetResourceManager()->MarkDirtyResource(id);
      }

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

      state.mappedPtr = NULL;
    }

    FreeAlignedBuffer(state.refData);
    state.refData = NULL;

    if(state.mapCoherent)
    {
      SCOPED_LOCK(m_CoherentMapsLock);

      auto it = std::find(m_CoherentMaps.begin(), m_CoherentMaps.end(), memrecord);
      if(it == m_CoherentMaps.end())
        RDCERR("vkUnmapMemory for memory handle that's not currently mapped");
      else
        m_CoherentMaps.erase(it);
    }
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
  SERIALISE_ELEMENT_LOCAL(MemRange, *pMemRanges);

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

    MappedData = state->mappedPtr + (size_t)MemRange.offset;
  }

  if(IsReplayingAndReading() && MemRange.memory != VK_NULL_HANDLE)
  {
    VkResult ret =
        ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(MemRange.memory), MemRange.offset,
                                   MemRange.size, 0, (void **)&MappedData);
    if(ret != VK_SUCCESS)
      RDCERR("Error mapping memory on replay: %s", ToStr(ret).c_str());
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("MappedData"_lit, MappedData, memRangeSize, SerialiserFlags::NoFlags);

  if(IsReplayingAndReading() && MappedData && MemRange.memory != VK_NULL_HANDLE)
    ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(MemRange.memory));

  SERIALISE_CHECK_READ_ERRORS();

  // if we need to save off this serialised buffer as reference for future comparison,
  // do so now. See the call to vkFlushMappedMemoryRanges in WrappedVulkan::vkQueueSubmit()
  if(ser.IsWriting() && state->needRefData)
  {
    if(!state->refData)
    {
      // if we're in this case, the range should be for the whole memory region.
      RDCASSERT(MemRange.offset == 0 && memRangeSize == state->mapSize);

      // allocate ref data so we can compare next time to minimise serialised data
      state->refData = AllocAlignedBuffer((size_t)state->mapSize);
    }

    // it's no longer safe to use state->mappedPtr, we need to save *precisely* what
    // was serialised. We do this by copying out of the serialiser since we know this
    // memory is not changing
    size_t offs = size_t(ser.GetWriter()->GetOffset() - memRangeSize);

    const byte *serialisedData = ser.GetWriter()->GetData() + offs;

    memcpy(state->refData, serialisedData, (size_t)memRangeSize);
  }

  return true;
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
      SCOPED_LOCK(m_CapTransitionLock);
      capframe = IsActiveCapturing(m_State);
    }

    for(uint32_t i = 0; i < memRangeCount; i++)
    {
      if(capframe)
      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkFlushMappedMemoryRanges);
        Serialise_vkFlushMappedMemoryRanges(ser, device, 1, pMemRanges + i);

        m_FrameCaptureRecord->AddChunk(scope.Get());
      }

      ResourceId memid = GetResID(pMemRanges[i].memory);

      MemMapState *state = GetRecord(pMemRanges[i].memory)->memMapState;
      state->mapFlushed = true;

      if(state->mappedPtr == NULL)
      {
        RDCERR("Flushing memory %s that isn't currently mapped", ToStr(memid).c_str());
        continue;
      }

      if(capframe)
      {
        GetResourceManager()->MarkMemoryFrameReferenced(GetResID(pMemRanges[i].memory),
                                                        pMemRanges[i].offset, pMemRanges[i].size,
                                                        eFrameRef_CompleteWrite);
      }
      else
      {
        GetResourceManager()->MarkDirtyResource(memid);
      }
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
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(memory);
  SERIALISE_ELEMENT(memoryOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(buffer));
    ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(memory));

    VkMemoryRequirements mrq = {};
    ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), &mrq);

    bool ok = CheckMemoryRequirements(StringFormat::Fmt("Buffer %llu", resOrigId).c_str(),
                                      GetResID(memory), memoryOffset, mrq);

    if(!ok)
      return false;

    ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(memory), memoryOffset);

    GetReplay()->GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
    GetReplay()->GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

    AddResourceCurChunk(memOrigId);
    AddResourceCurChunk(resOrigId);
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

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindBufferMemory);
      Serialise_vkBindBufferMemory(ser, device, buffer, memory, memoryOffset);

      chunk = scope.Get();
    }

    // memory object bindings are immutable and must happen before creation or use,
    // so this can always go into the record, even if a resource is created and bound
    // to memory mid-frame
    record->AddChunk(chunk);

    record->AddParent(GetRecord(memory));
    record->baseResource = GetResID(memory);
    record->memOffset = memoryOffset;

    // if the buffer was force-referenced, do the same with the memory
    if(IsForcedReference(GetResID(buffer)))
    {
      AddForcedReference(GetResID(memory), eFrameRef_ReadBeforeWrite);

      // the memory is immediately dirty because we have no way of tracking writes to it
      GetResourceManager()->MarkDirtyResource(GetResID(memory));
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkBindImageMemory(SerialiserType &ser, VkDevice device, VkImage image,
                                                VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(image);
  SERIALISE_ELEMENT(memory);
  SERIALISE_ELEMENT(memoryOffset);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(image));
    ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(memory));

    VkMemoryRequirements mrq = {};
    ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), &mrq);

    bool ok = CheckMemoryRequirements(StringFormat::Fmt("Image %llu", resOrigId).c_str(),
                                      GetResID(memory), memoryOffset, mrq);

    if(!ok)
      return false;

    ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(memory), memoryOffset);

    m_ImageLayouts[GetResID(image)].memoryBound = true;

    GetReplay()->GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
    GetReplay()->GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

    AddResourceCurChunk(memOrigId);
    AddResourceCurChunk(resOrigId);
  }

  return true;
}

VkResult WrappedVulkan::vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory mem,
                                          VkDeviceSize memOffset)
{
  VkResourceRecord *record = GetRecord(image);

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image),
                                                             Unwrap(mem), memOffset));

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindImageMemory);
      Serialise_vkBindImageMemory(ser, device, image, mem, memOffset);

      chunk = scope.Get();
    }

    ImageLayouts *layout = NULL;
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      layout = &m_ImageLayouts[GetResID(image)];
    }

    layout->memoryBound = true;

    // memory object bindings are immutable and must happen before creation or use,
    // so this can always go into the record, even if a resource is created and bound
    // to memory mid-frame
    record->AddChunk(chunk);

    record->AddParent(GetRecord(mem));

    // images are a base resource but we want to track where their memory comes from.
    // Anything that looks up a baseResource for an image knows not to chase further
    // than the image.
    record->baseResource = GetResID(mem);
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
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

    // remap the queue family indices
    if(CreateInfo.sharingMode == VK_SHARING_MODE_EXCLUSIVE)
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
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), buf);
      GetResourceManager()->AddLiveResource(Buffer, buf);

      m_CreationInfo.m_Buffer[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
    }

    AddResource(Buffer, ResourceType::Buffer, "Buffer");
    DerivedResource(device, Buffer);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer)
{
  VkBufferCreateInfo adjusted_info = *pCreateInfo;

  // TEMP HACK: Until we define a portable fake hardware, need to match the requirements for usage
  // on replay, so that the memory requirements are the same
  adjusted_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  // If we're using this buffer for device addresses, ensure we force on capture replay bit.
  // We ensured the physical device can support this feature before whitelisting the extension.
  if(adjusted_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT)
    adjusted_info.flags |= VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT;

  byte *tempMem = GetTempMemory(GetNextPatchSize(adjusted_info.pNext));

  UnwrapNextChain(m_State, "VkBufferCreateInfo", tempMem, (VkBaseInStructure *)&adjusted_info);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &adjusted_info, pAllocator, pBuffer));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pBuffer);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      VkBufferCreateInfo serialisedCreateInfo = *pCreateInfo;
      VkBufferDeviceAddressCreateInfoEXT bufferDeviceAddress = {
          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,
      };

      // if we're using VK_EXT_buffer_device_address, we fetch the device address that's been
      // allocated and insert it into the next chain and patch the flags so that it replays
      // naturally.
      if(GetRecord(device)->instDevInfo->ext_EXT_buffer_device_address &&
         (pCreateInfo->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT) != 0)
      {
        VkBufferDeviceAddressInfoEXT getInfo = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT, NULL, Unwrap(*pBuffer),
        };

        bufferDeviceAddress.deviceAddress =
            ObjDisp(device)->GetBufferDeviceAddressEXT(Unwrap(device), &getInfo);

        RDCASSERT(bufferDeviceAddress.deviceAddress);

        // push this struct onto the start of the chain
        bufferDeviceAddress.pNext = serialisedCreateInfo.pNext;
        serialisedCreateInfo.pNext = &bufferDeviceAddress;

        // tell the driver we're giving it a pre-allocated address to use
        serialisedCreateInfo.flags |= VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT;

        // this buffer must be forced to be in any captures, since we can't track when it's used by
        // address
        AddForcedReference(GetResID(*pBuffer), eFrameRef_Read);
      }

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateBuffer);
        Serialise_vkCreateBuffer(ser, device, &serialisedCreateInfo, NULL, pBuffer);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pBuffer);
      record->AddChunk(chunk);
      record->memSize = pCreateInfo->size;

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

      if(isSparse || isExternal)
      {
        record->resInfo = new ResourceInfo();

        // pre-populate memory requirements
        ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(*pBuffer),
                                                     &record->resInfo->memreqs);

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
                  RDCMAX(record->resInfo->memreqs.size, mrq.alignment);
              record->resInfo->memreqs.memoryTypeBits &= mrq.memoryTypeBits;
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

      m_CreationInfo.m_Buffer[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
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
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
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
                                           const VkAllocationCallbacks *pAllocator,
                                           VkBufferView *pView)
{
  VkBufferViewCreateInfo unwrappedInfo = *pCreateInfo;
  unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &unwrappedInfo, pAllocator, pView));

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
      record->baseResourceMem = bufferRecord->baseResource;
      record->resInfo = bufferRecord->resInfo;
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
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
    if(CreateInfo.sharingMode == VK_SHARING_MODE_EXCLUSIVE)
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
        // only add STORAGE_BIT if we have an MS2Array pipeline. If it failed to create due to lack
        // of capability or because we disabled it as a workaround then we don't need this
        // capability (and it might be the bug we're trying to work around by disabling the
        // pipeline)
        if(GetDebugManager()->IsMS2ArraySupported())
          CreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
      }
      else
      {
        CreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }

    APIProps.YUVTextures |= IsYUVFormat(CreateInfo.format);

    if(CreateInfo.flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT))
    {
      APIProps.SparseResources = true;
    }

    // we search for the separate stencil usage struct now that it's in patchable memory
    VkImageStencilUsageCreateInfoEXT *separateStencilUsage =
        (VkImageStencilUsageCreateInfoEXT *)FindNextStruct(
            &CreateInfo, VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT);
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

    VkImageCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkImageCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), &patched, NULL, &img);

    CreateInfo.usage = origusage;

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), img);
      GetResourceManager()->AddLiveResource(Image, img);

      m_CreationInfo.m_Image[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);

      VkImageSubresourceRange range;
      range.baseMipLevel = range.baseArrayLayer = 0;
      range.levelCount = CreateInfo.mipLevels;
      range.layerCount = CreateInfo.arrayLayers;

      ImageLayouts &layouts = m_ImageLayouts[live];
      layouts.imageInfo = ImageInfo(CreateInfo);

      layouts.subresourceStates.clear();

      layouts.initialLayout = CreateInfo.initialLayout;

      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      if(IsDepthOnlyFormat(CreateInfo.format))
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      else if(IsStencilOnlyFormat(CreateInfo.format))
        range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      else if(IsDepthOrStencilFormat(CreateInfo.format))
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

      layouts.subresourceStates.push_back(ImageRegionState(
          VK_QUEUE_FAMILY_IGNORED, range, UNKNOWN_PREV_IMG_LAYOUT, CreateInfo.initialLayout));
    }

    const char *prefix = "Image";

    if(CreateInfo.imageType == VK_IMAGE_TYPE_1D)
    {
      prefix = CreateInfo.arrayLayers > 1 ? "1D Array Image" : "1D Image";

      if(CreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        prefix = "1D Color Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        prefix = "1D Depth Attachment";
    }
    else if(CreateInfo.imageType == VK_IMAGE_TYPE_2D)
    {
      prefix = CreateInfo.arrayLayers > 1 ? "2D Array Image" : "2D Image";

      if(CreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        prefix = "2D Color Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        prefix = "2D Depth Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT)
        prefix = "2D Fragment Density Map Attachment";
    }
    else if(CreateInfo.imageType == VK_IMAGE_TYPE_3D)
    {
      prefix = "3D Image";

      if(CreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        prefix = "3D Color Attachment";
      else if(CreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        prefix = "3D Depth Attachment";
    }

    AddResource(Image, ResourceType::Texture, prefix);
    DerivedResource(device, Image);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
  VkImageCreateInfo createInfo_adjusted = *pCreateInfo;

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
        if(GetDebugManager() && GetDebugManager()->IsMS2ArraySupported())
          createInfo_adjusted.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
      }
      else
      {
        createInfo_adjusted.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }
  }

  // create non-subsampled image to be able to copy its content
  createInfo_adjusted.flags &= ~VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;

  byte *tempMem = GetTempMemory(GetNextPatchSize(createInfo_adjusted.pNext));

  UnwrapNextChain(m_State, "VkImageCreateInfo", tempMem, (VkBaseInStructure *)&createInfo_adjusted);

  // we search for the separate stencil usage struct now that it's in patchable memory
  VkImageStencilUsageCreateInfoEXT *separateStencilUsage =
      (VkImageStencilUsageCreateInfoEXT *)FindNextStruct(
          &createInfo_adjusted, VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT);
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

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateImage(Unwrap(device), &createInfo_adjusted, pAllocator, pImage));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pImage);

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

      // pre-populate memory requirements
      ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(*pImage), &resInfo.memreqs);

      bool isSparse = (pCreateInfo->flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                                             VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)) != 0;

      bool isExternal = false;

      const VkBaseInStructure *next = (const VkBaseInStructure *)pCreateInfo->pNext;

      // search for external memory image create info struct in pNext chain
      while(next)
      {
        if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV ||
           next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO)
        {
          isExternal = true;
          break;
        }

        next = next->pNext;
      }

      // sparse and external images are considered dirty from creation. For sparse images this is
      // so that we can serialise the tracked page table, for external images this is so we can be
      // sure to fetch their contents even if we don't see any writes.
      if(isSparse || isExternal)
      {
        GetResourceManager()->MarkDirtyResource(id);

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

          RDCASSERTMSG("Couldn't find next struct indicating external memory", removed);

          VkImage tmpimg = VK_NULL_HANDLE;
          VkResult vkr =
              ObjDisp(device)->CreateImage(Unwrap(device), &createInfo_adjusted, NULL, &tmpimg);

          if(vkr == VK_SUCCESS && tmpimg != VK_NULL_HANDLE)
          {
            VkMemoryRequirements mrq = {};
            ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), tmpimg, &mrq);

            if(mrq.size > 0)
            {
              RDCDEBUG("External image requires %llu bytes at %llu alignment, in %x memory types",
                       resInfo.memreqs.size, resInfo.memreqs.alignment,
                       resInfo.memreqs.memoryTypeBits);
              RDCDEBUG(
                  "Non-external version requires %llu bytes at %llu alignment, in %x memory types",
                  mrq.size, mrq.alignment, mrq.memoryTypeBits);

              resInfo.memreqs.size = RDCMAX(resInfo.memreqs.size, mrq.size);
              resInfo.memreqs.alignment = RDCMAX(resInfo.memreqs.size, mrq.alignment);
              resInfo.memreqs.memoryTypeBits &= mrq.memoryTypeBits;
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
        if(pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
        {
          // must record image and page dimension, and create page tables
          uint32_t numreqs = NUM_VK_IMAGE_ASPECTS;
          VkSparseImageMemoryRequirements reqs[NUM_VK_IMAGE_ASPECTS];
          ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(*pImage),
                                                            &numreqs, reqs);

          RDCASSERT(numreqs > 0);

          resInfo.pagedim = reqs[0].formatProperties.imageGranularity;
          resInfo.imgdim = pCreateInfo->extent;
          resInfo.imgdim.width /= resInfo.pagedim.width;
          resInfo.imgdim.height /= resInfo.pagedim.height;
          resInfo.imgdim.depth /= resInfo.pagedim.depth;

          uint32_t numpages = resInfo.imgdim.width * resInfo.imgdim.height * resInfo.imgdim.depth;

          for(uint32_t i = 0; i < numreqs; i++)
          {
            // assume all page sizes are the same for all aspects
            RDCASSERT(resInfo.pagedim.width == reqs[i].formatProperties.imageGranularity.width &&
                      resInfo.pagedim.height == reqs[i].formatProperties.imageGranularity.height &&
                      resInfo.pagedim.depth == reqs[i].formatProperties.imageGranularity.depth);

            int a = 0;
            for(a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
            {
              if(reqs[i].formatProperties.aspectMask & (1 << a))
                break;
            }

            resInfo.pages[a] = new rdcpair<VkDeviceMemory, VkDeviceSize>[numpages];
          }
        }
        else
        {
          // don't have to do anything, image is opaque and must be fully bound, just need
          // to track the memory bindings.
        }
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pImage);

      m_CreationInfo.m_Image[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }

    VkImageSubresourceRange range;
    range.baseMipLevel = range.baseArrayLayer = 0;
    range.levelCount = pCreateInfo->mipLevels;
    range.layerCount = pCreateInfo->arrayLayers;

    ImageLayouts *layout = NULL;
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      layout = &m_ImageLayouts[id];
    }
    layout->imageInfo = ImageInfo(*pCreateInfo);

    layout->initialLayout = pCreateInfo->initialLayout;
    layout->subresourceStates.clear();

    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsDepthOnlyFormat(pCreateInfo->format))
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if(IsStencilOnlyFormat(pCreateInfo->format))
      range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(IsDepthOrStencilFormat(pCreateInfo->format))
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    layout->subresourceStates.push_back(ImageRegionState(
        VK_QUEUE_FAMILY_IGNORED, range, UNKNOWN_PREV_IMG_LAYOUT, pCreateInfo->initialLayout));
  }

  return ret;
}

// Image view functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateImageView(SerialiserType &ser, VkDevice device,
                                                const VkImageViewCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkImageView *pView)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(View, GetResID(*pView)).TypedAs("VkImageView"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkImageView view = VK_NULL_HANDLE;

    VkImageViewCreateInfo unwrappedInfo = CreateInfo;
    unwrappedInfo.image = Unwrap(unwrappedInfo.image);
    VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &unwrappedInfo, NULL, &view);

    APIProps.YUVTextures |= IsYUVFormat(CreateInfo.format);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
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
                                          const VkAllocationCallbacks *pAllocator, VkImageView *pView)
{
  VkImageViewCreateInfo unwrappedInfo = *pCreateInfo;
  unwrappedInfo.image = Unwrap(unwrappedInfo.image);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateImageView(Unwrap(device), &unwrappedInfo, pAllocator, pView));

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
      record->baseResourceMem = imageRecord->baseResource;
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
  SERIALISE_ELEMENT_ARRAY(pBindInfos, bindInfoCount);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      const VkBindBufferMemoryInfo &bindInfo = pBindInfos[i];

      ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.buffer));
      ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.memory));

      VkMemoryRequirements mrq = {};
      ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(bindInfo.buffer), &mrq);

      bool ok = CheckMemoryRequirements(StringFormat::Fmt("Buffer %llu", resOrigId).c_str(),
                                        GetResID(bindInfo.memory), bindInfo.memoryOffset, mrq);

      if(!ok)
        return false;

      GetReplay()->GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
      GetReplay()->GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

      AddResourceCurChunk(memOrigId);
      AddResourceCurChunk(resOrigId);
    }

    VkBindBufferMemoryInfo *unwrapped = UnwrapInfos(pBindInfos, bindInfoCount);
    ObjDisp(device)->BindBufferMemory2(Unwrap(device), bindInfoCount, unwrapped);
  }

  return true;
}

VkResult WrappedVulkan::vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount,
                                            const VkBindBufferMemoryInfo *pBindInfos)
{
  VkBindBufferMemoryInfo *unwrapped = UnwrapInfos(pBindInfos, bindInfoCount);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->BindBufferMemory2(Unwrap(device), bindInfoCount, unwrapped));

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
        Serialise_vkBindBufferMemory2(ser, device, bindInfoCount, pBindInfos);

        chunk = scope.Get();
      }

      // memory object bindings are immutable and must happen before creation or use,
      // so this can always go into the record, even if a resource is created and bound
      // to memory mid-frame
      bufrecord->AddChunk(chunk);

      bufrecord->AddParent(memrecord);
      bufrecord->baseResource = memrecord->GetResourceID();
      bufrecord->memOffset = pBindInfos[i].memoryOffset;

      // if the buffer was force-referenced, do the same with the memory
      if(IsForcedReference(GetResID(pBindInfos[i].buffer)))
      {
        AddForcedReference(GetResID(pBindInfos[i].memory), eFrameRef_ReadBeforeWrite);

        // the memory is immediately dirty because we have no way of tracking writes to it
        GetResourceManager()->MarkDirtyResource(GetResID(pBindInfos[i].memory));
      }
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
  SERIALISE_ELEMENT_ARRAY(pBindInfos, bindInfoCount);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      const VkBindImageMemoryInfo &bindInfo = pBindInfos[i];

      ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.image));
      ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(bindInfo.memory));

      VkMemoryRequirements mrq = {};
      ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(bindInfo.image), &mrq);

      bool ok = CheckMemoryRequirements(StringFormat::Fmt("Image %llu", resOrigId).c_str(),
                                        GetResID(bindInfo.memory), bindInfo.memoryOffset, mrq);

      if(!ok)
        return false;

      m_ImageLayouts[GetResID(bindInfo.image)].memoryBound = true;

      GetReplay()->GetResourceDesc(memOrigId).derivedResources.push_back(resOrigId);
      GetReplay()->GetResourceDesc(resOrigId).parentResources.push_back(memOrigId);

      AddResourceCurChunk(memOrigId);
      AddResourceCurChunk(resOrigId);
    }

    VkBindImageMemoryInfo *unwrapped = UnwrapInfos(pBindInfos, bindInfoCount);
    ObjDisp(device)->BindImageMemory2(Unwrap(device), bindInfoCount, unwrapped);
  }

  return true;
}

VkResult WrappedVulkan::vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount,
                                           const VkBindImageMemoryInfo *pBindInfos)
{
  VkBindImageMemoryInfo *unwrapped = UnwrapInfos(pBindInfos, bindInfoCount);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->BindImageMemory2(Unwrap(device), bindInfoCount, unwrapped));

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

      ImageLayouts *layout = NULL;
      {
        SCOPED_LOCK(m_ImageLayoutsLock);
        layout = &m_ImageLayouts[imgrecord->GetResourceID()];
      }

      layout->memoryBound = true;

      // memory object bindings are immutable and must happen before creation or use,
      // so this can always go into the record, even if a resource is created and bound
      // to memory mid-frame
      imgrecord->AddChunk(chunk);

      imgrecord->AddParent(memrecord);

      // images are a base resource but we want to track where their memory comes from.
      // Anything that looks up a baseResource for an image knows not to chase further
      // than the image.
      imgrecord->baseResource = memrecord->GetResourceID();
    }
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkAllocateMemory, VkDevice device,
                                const VkMemoryAllocateInfo *pAllocateInfo,
                                const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUnmapMemory, VkDevice device, VkDeviceMemory memory);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkFlushMappedMemoryRanges, VkDevice device,
                                uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory, VkDevice device, VkBuffer buffer,
                                VkDeviceMemory memory, VkDeviceSize memoryOffset);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindImageMemory, VkDevice device, VkImage image,
                                VkDeviceMemory memory, VkDeviceSize memoryOffset);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateBuffer, VkDevice device,
                                const VkBufferCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateBufferView, VkDevice device,
                                const VkBufferViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkBufferView *pView);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateImage, VkDevice device,
                                const VkImageCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkImage *pImage);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateImageView, VkDevice device,
                                const VkImageViewCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkImageView *pView);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory2, VkDevice device,
                                uint32_t bindInfoCount, const VkBindBufferMemoryInfo *pBindInfos);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBindImageMemory2, VkDevice device,
                                uint32_t bindInfoCount, const VkBindImageMemoryInfo *pBindInfos);
