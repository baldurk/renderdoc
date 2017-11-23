/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkAllocateMemory(SerialiserType &ser, VkDevice device,
                                               const VkMemoryAllocateInfo *pAllocateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkDeviceMemory *pMemory)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo);
  SERIALISE_ELEMENT_LOCAL(Memory, GetResID(*pMemory));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDeviceMemory mem = VK_NULL_HANDLE;

    // serialised memory type index is non-remapped, so we remap now.
    // PORTABILITY may need to re-write info to change memory type index to the
    // appropriate index on replay
    AllocateInfo.memoryTypeIndex = m_PhysicalDeviceData.memIdxMap[AllocateInfo.memoryTypeIndex];

    VkResult ret = ObjDisp(device)->AllocateMemory(Unwrap(device), &AllocateInfo, NULL, &mem);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
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

      RDCASSERT(mrq.size <= AllocateInfo.allocationSize, mrq.size, AllocateInfo.allocationSize);

      ResourceId bufid = GetResourceManager()->WrapResource(Unwrap(device), buf);

      ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buf), Unwrap(mem), 0);

      // register as a live-only resource, so it is cleaned up properly
      GetResourceManager()->AddLiveResource(bufid, buf);

      m_CreationInfo.m_Memory[live].wholeMemBuf = buf;
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

  size_t memSize = 0;

  // we don't have to unwrap every struct, but unwrapping a struct means we need to copy
  // the previous one in the chain locally to modify the pNext pointer. So we just copy
  // all of them locally
  {
    const VkGenericStruct *next = (const VkGenericStruct *)info.pNext;
    while(next)
    {
      // we need to unwrap these structs
      if(next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV)
        memSize += sizeof(VkDedicatedAllocationMemoryAllocateInfoNV);
      else if(next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR)
        memSize += sizeof(VkMemoryDedicatedAllocateInfoKHR);
      // the rest we don't need to unwrap, but we need to copy locally for chaining
      else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV)
        memSize += sizeof(VkExportMemoryAllocateInfoNV);
      else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR)
        memSize += sizeof(VkExportMemoryAllocateInfoKHR);
      else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR)
        memSize += sizeof(VkImportMemoryFdInfoKHR);

#ifdef VK_NV_external_memory_win32
      else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
        memSize += sizeof(VkExportMemoryWin32HandleInfoNV);
      else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
        memSize += sizeof(VkImportMemoryWin32HandleInfoNV);
#else
      else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
        RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
      else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
        RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
#endif

#ifdef VK_KHR_external_memory_win32
      else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
        memSize += sizeof(VkExportMemoryWin32HandleInfoKHR);
      else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
        memSize += sizeof(VkImportMemoryWin32HandleInfoKHR);
#else
      else if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
        RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
      else if(next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
        RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif

      next = next->pNext;
    }
  }

  byte *tempMem = GetTempMemory(memSize);

  {
    VkGenericStruct *nextChainTail = (VkGenericStruct *)&info;
    const VkGenericStruct *nextInput = (const VkGenericStruct *)info.pNext;
    while(nextInput)
    {
      // unwrap and replace the dedicated allocation struct in the chain
      if(nextInput->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV)
      {
        const VkDedicatedAllocationMemoryAllocateInfoNV *dedicatedIn =
            (const VkDedicatedAllocationMemoryAllocateInfoNV *)nextInput;
        VkDedicatedAllocationMemoryAllocateInfoNV *dedicatedOut =
            (VkDedicatedAllocationMemoryAllocateInfoNV *)tempMem;

        // copy and unwrap the struct
        dedicatedOut->sType = dedicatedIn->sType;
        dedicatedOut->buffer = Unwrap(dedicatedIn->buffer);
        dedicatedOut->image = Unwrap(dedicatedIn->image);

        AppendModifiedChainedStruct(tempMem, dedicatedOut, nextChainTail);
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR)
      {
        const VkMemoryDedicatedAllocateInfoKHR *dedicatedIn =
            (const VkMemoryDedicatedAllocateInfoKHR *)nextInput;
        VkMemoryDedicatedAllocateInfoKHR *dedicatedOut = (VkMemoryDedicatedAllocateInfoKHR *)tempMem;

        // copy and unwrap the struct
        dedicatedOut->sType = dedicatedIn->sType;
        dedicatedOut->buffer = Unwrap(dedicatedIn->buffer);
        dedicatedOut->image = Unwrap(dedicatedIn->image);

        AppendModifiedChainedStruct(tempMem, dedicatedOut, nextChainTail);
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV)
      {
        CopyNextChainedStruct<VkExportMemoryAllocateInfoNV>(tempMem, nextInput, nextChainTail);
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR)
      {
        CopyNextChainedStruct<VkExportMemoryAllocateInfoKHR>(tempMem, nextInput, nextChainTail);
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR)
      {
        CopyNextChainedStruct<VkImportMemoryFdInfoKHR>(tempMem, nextInput, nextChainTail);
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
      {
#ifdef VK_KHR_external_memory_win32
        CopyNextChainedStruct<VkExportMemoryWin32HandleInfoKHR>(tempMem, nextInput, nextChainTail);
#else
        RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR)
      {
#ifdef VK_KHR_external_memory_win32
        CopyNextChainedStruct<VkImportMemoryWin32HandleInfoKHR>(tempMem, nextInput, nextChainTail);
#else
        RDCERR("Support for VK_KHR_external_memory_win32 not compiled in");
#endif
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
      {
#ifdef VK_NV_external_memory_win32
        CopyNextChainedStruct<VkExportMemoryWin32HandleInfoNV>(tempMem, nextInput, nextChainTail);
#else
        RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
#endif
      }
      else if(nextInput->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
      {
#ifdef VK_NV_external_memory_win32
        CopyNextChainedStruct<VkImportMemoryWin32HandleInfoNV>(tempMem, nextInput, nextChainTail);
#else
        RDCERR("Support for VK_NV_external_memory_win32 not compiled in");
#endif
      }
      else
      {
        RDCERR("unrecognised struct %d in vkAllocateMemoryInfo pNext chain", nextInput->sType);
        // can't patch this struct, have to just copy it and hope it's the last in the chain
        nextChainTail->pNext = nextInput;
      }

      nextInput = nextInput->pNext;
    }
  }

  VkResult ret = ObjDisp(device)->AllocateMemory(Unwrap(device), &info, pAllocator, pMemory);

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
      state.mapSize = size == VK_WHOLE_SIZE ? memrecord->Length : size;
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

  if(IsReplayingAndReading() && memory)
  {
    VkResult vkr = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(memory), MapOffset, MapSize, 0,
                                              (void **)&MapData);
    if(vkr != VK_SUCCESS)
      RDCERR("Error mapping memory on replay: %s", ToStr(vkr).c_str());
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("MapData", MapData, MapSize, SerialiserFlags::NoFlags);

  if(IsReplayingAndReading() && MapData && memory)
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
            GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Write);
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

  if(IsReplayingAndReading() && MemRange.memory)
  {
    VkResult ret =
        ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(MemRange.memory), MemRange.offset,
                                   MemRange.size, 0, (void **)&MappedData);
    if(ret != VK_SUCCESS)
      RDCERR("Error mapping memory on replay: %s", ToStr(ret).c_str());
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("MappedData", MappedData, memRangeSize, SerialiserFlags::NoFlags);

  if(IsReplayingAndReading() && MappedData && MemRange.memory)
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
        RDCERR("Flushing memory that isn't currently mapped");
        continue;
      }

      if(capframe)
      {
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pMemRanges[i].memory),
                                                          eFrameRef_Write);
      }
      else
      {
        GetResourceManager()->MarkDirtyResource(memid);
      }
    }
  }

  VkMappedMemoryRange *unwrapped = GetTempArray<VkMappedMemoryRange>(memRangeCount);
  for(uint32_t i = 0; i < memRangeCount; i++)
  {
    unwrapped[i] = pMemRanges[i];
    unwrapped[i].memory = Unwrap(unwrapped[i].memory);
  }

  VkResult ret = ObjDisp(device)->FlushMappedMemoryRanges(Unwrap(device), memRangeCount, unwrapped);

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
    ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(memory), memoryOffset);

    ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(buffer));
    ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(memory));

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
  }

  return ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(memory),
                                           memoryOffset);
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
    ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(memory), memoryOffset);

    ResourceId resOrigId = GetResourceManager()->GetOriginalID(GetResID(image));
    ResourceId memOrigId = GetResourceManager()->GetOriginalID(GetResID(memory));

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

  if(IsCaptureMode(m_State))
  {
    Chunk *chunk = NULL;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBindImageMemory);
      Serialise_vkBindImageMemory(ser, device, image, mem, memOffset);

      chunk = scope.Get();
    }

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

  return ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(mem), memOffset);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateBuffer(SerialiserType &ser, VkDevice device,
                                             const VkBufferCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkBuffer *pBuffer)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_LOCAL(Buffer, GetResID(*pBuffer));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkBuffer buf = VK_NULL_HANDLE;

    VkBufferUsageFlags origusage = CreateInfo.usage;

    // ensure we can always readback from buffers
    CreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &CreateInfo, NULL, &buf);

    CreateInfo.usage = origusage;

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
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

  VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &adjusted_info, pAllocator, pBuffer);

  // SHARING: pCreateInfo sharingMode, queueFamilyCount, pQueueFamilyIndices

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pBuffer);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateBuffer);
        Serialise_vkCreateBuffer(ser, device, pCreateInfo, NULL, pBuffer);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pBuffer);
      record->AddChunk(chunk);

      if(pCreateInfo->flags &
         (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT))
      {
        record->sparseInfo = new SparseMapping();

        // buffers are always bound opaquely and in arbitrary divisions, sparse residency
        // only means not all the buffer needs to be bound, which is not that interesting for
        // our purposes

        bool capframe = false;

        {
          SCOPED_LOCK(m_CapTransitionLock);
          capframe = IsActiveCapturing(m_State);
        }

        if(capframe)
          GetResourceManager()->MarkPendingDirty(id);
        else
          GetResourceManager()->MarkDirtyResource(id);
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
  SERIALISE_ELEMENT_LOCAL(View, GetResID(*pView));

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
  VkResult ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &unwrappedInfo, pAllocator, pView);

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
      record->baseResource = bufferRecord->baseResource;
      record->sparseInfo = bufferRecord->sparseInfo;
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pView);

      m_CreationInfo.m_BufferView[id].Init(GetResourceManager(), m_CreationInfo, &unwrappedInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateImage(SerialiserType &ser, VkDevice device,
                                            const VkImageCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_LOCAL(Image, GetResID(*pImage));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkImage img = VK_NULL_HANDLE;

    VkImageUsageFlags origusage = CreateInfo.usage;

    // ensure we can always display and copy from/to textures
    CreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
        if(GetDebugManager()->m_MS2ArrayPipe != VK_NULL_HANDLE)
          CreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
      }
      else
      {
        CreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }

    VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), &CreateInfo, NULL, &img);

    CreateInfo.usage = origusage;

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
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
      layouts.subresourceStates.clear();

      layouts.layerCount = CreateInfo.arrayLayers;
      layouts.sampleCount = (int)CreateInfo.samples;
      layouts.levelCount = CreateInfo.mipLevels;
      layouts.extent = CreateInfo.extent;
      layouts.format = CreateInfo.format;

      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      if(IsDepthOnlyFormat(CreateInfo.format))
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      else if(IsStencilOnlyFormat(CreateInfo.format))
        range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      else if(IsDepthOrStencilFormat(CreateInfo.format))
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

      layouts.subresourceStates.push_back(
          ImageRegionState(range, UNKNOWN_PREV_IMG_LAYOUT, VK_IMAGE_LAYOUT_UNDEFINED));
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
  }

  if(createInfo_adjusted.samples != VK_SAMPLE_COUNT_1_BIT)
  {
    createInfo_adjusted.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo_adjusted.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    // TEMP HACK: matching replay requirements
    if(IsCaptureMode(m_State))
    {
      if(!IsDepthOrStencilFormat(createInfo_adjusted.format))
      {
        if(GetDebugManager()->m_MS2ArrayPipe != VK_NULL_HANDLE)
          createInfo_adjusted.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
      }
      else
      {
        createInfo_adjusted.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }
  }

  VkResult ret =
      ObjDisp(device)->CreateImage(Unwrap(device), &createInfo_adjusted, pAllocator, pImage);

  // SHARING: pCreateInfo sharingMode, queueFamilyCount, pQueueFamilyIndices

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

      bool isSparse = (pCreateInfo->flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                                             VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)) != 0;

      bool isExternal = false;

      const VkGenericStruct *next = (const VkGenericStruct *)pCreateInfo->pNext;

      // search for external memory image create info struct in pNext chain
      while(next)
      {
        if(next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV ||
           next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR)
        {
          isExternal = true;
          break;
        }

        next = next->pNext;
      }

      bool capframe = false;

      {
        SCOPED_LOCK(m_CapTransitionLock);
        capframe = IsActiveCapturing(m_State);
      }

      // sparse and external images are considered dirty from creation. For sparse images this is
      // so that we can serialise the tracked page table, for external images this is so we can be
      // sure to fetch their contents even if we don't see any writes.
      if(isSparse || isExternal)
      {
        if(capframe)
          GetResourceManager()->MarkPendingDirty(id);
        else
          GetResourceManager()->MarkDirtyResource(id);
      }

      if(isSparse)
      {
        record->sparseInfo = new SparseMapping();

        if(pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
        {
          // must record image and page dimension, and create page tables
          uint32_t numreqs = NUM_VK_IMAGE_ASPECTS;
          VkSparseImageMemoryRequirements reqs[NUM_VK_IMAGE_ASPECTS];
          ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(*pImage),
                                                            &numreqs, reqs);

          RDCASSERT(numreqs > 0);

          record->sparseInfo->pagedim = reqs[0].formatProperties.imageGranularity;
          record->sparseInfo->imgdim = pCreateInfo->extent;
          record->sparseInfo->imgdim.width /= record->sparseInfo->pagedim.width;
          record->sparseInfo->imgdim.height /= record->sparseInfo->pagedim.height;
          record->sparseInfo->imgdim.depth /= record->sparseInfo->pagedim.depth;

          uint32_t numpages = record->sparseInfo->imgdim.width * record->sparseInfo->imgdim.height *
                              record->sparseInfo->imgdim.depth;

          for(uint32_t i = 0; i < numreqs; i++)
          {
            // assume all page sizes are the same for all aspects
            RDCASSERT(record->sparseInfo->pagedim.width ==
                          reqs[i].formatProperties.imageGranularity.width &&
                      record->sparseInfo->pagedim.height ==
                          reqs[i].formatProperties.imageGranularity.height &&
                      record->sparseInfo->pagedim.depth ==
                          reqs[i].formatProperties.imageGranularity.depth);

            int a = 0;
            for(a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
            {
              if(reqs[i].formatProperties.aspectMask & (1 << a))
                break;
            }

            record->sparseInfo->pages[a] = new pair<VkDeviceMemory, VkDeviceSize>[numpages];
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

    layout->layerCount = pCreateInfo->arrayLayers;
    layout->levelCount = pCreateInfo->mipLevels;
    layout->sampleCount = (int)pCreateInfo->samples;
    layout->extent = pCreateInfo->extent;
    layout->format = pCreateInfo->format;

    layout->subresourceStates.clear();

    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsDepthOnlyFormat(pCreateInfo->format))
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if(IsStencilOnlyFormat(pCreateInfo->format))
      range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(IsDepthOrStencilFormat(pCreateInfo->format))
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    layout->subresourceStates.push_back(
        ImageRegionState(range, UNKNOWN_PREV_IMG_LAYOUT, VK_IMAGE_LAYOUT_UNDEFINED));
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
  SERIALISE_ELEMENT_LOCAL(View, GetResID(*pView));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkImageView view = VK_NULL_HANDLE;

    VkImageViewCreateInfo unwrappedInfo = CreateInfo;
    unwrappedInfo.image = Unwrap(unwrappedInfo.image);
    VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &unwrappedInfo, NULL, &view);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
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
  VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &unwrappedInfo, pAllocator, pView);

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
      record->sparseInfo = imageRecord->sparseInfo;
      record->viewRange = pCreateInfo->subresourceRange;
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pView);

      m_CreationInfo.m_ImageView[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
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
