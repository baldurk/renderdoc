/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "core/settings.h"
#include "vk_core.h"

RDOC_CONFIG(bool, Vulkan_Debug_MemoryAllocationLogging, false,
            "Output verbose debug logging messages when allocating internal memory.");

void WrappedVulkan::ChooseMemoryIndices()
{
  // we need to do this little dance because Get*MemoryIndex checks to see if the existing
  // readback index is valid, and if so just returns it without doing the proper checks.
  // so first we set the indices to something invalid then call the function
  m_PhysicalDeviceData.readbackMemIndex = m_PhysicalDeviceData.uploadMemIndex =
      m_PhysicalDeviceData.GPULocalMemIndex = ~0U;

  m_PhysicalDeviceData.readbackMemIndex = GetReadbackMemoryIndex(~0U);
  m_PhysicalDeviceData.uploadMemIndex = GetUploadMemoryIndex(~0U);
  m_PhysicalDeviceData.GPULocalMemIndex = GetGPULocalMemoryIndex(~0U);

  for(uint32_t i = 0; i < m_PhysicalDeviceData.memProps.memoryTypeCount; i++)
  {
    rdcstr selected;

    if(m_PhysicalDeviceData.GPULocalMemIndex == i)
      selected += "GPULocal|";
    if(m_PhysicalDeviceData.readbackMemIndex == i)
      selected += "readback|";
    if(m_PhysicalDeviceData.uploadMemIndex == i)
      selected += "upload|";

    selected.pop_back();

    const VkMemoryType &type = m_PhysicalDeviceData.memProps.memoryTypes[i];
    const VkMemoryHeap &heap = m_PhysicalDeviceData.memProps.memoryHeaps[type.heapIndex];

    bool giga = true;
    float div = (1024.0f * 1024.0f * 1024.0f);

    if(heap.size < 1024 * 1024 * 1024ULL)
    {
      giga = false;
      div /= 1024.0f;
    }

    RDCLOG("  Memory type %u: %s in heap %u (%s) (%.1f %s) [%s]", i,
           ToStr((VkMemoryPropertyFlagBits)type.propertyFlags).c_str(), type.heapIndex,
           ToStr((VkMemoryHeapFlagBits)heap.flags).c_str(), float(heap.size) / div,
           giga ? "GB" : "MB", selected.c_str());
  }
}

uint32_t WrappedVulkan::GetReadbackMemoryIndex(uint32_t resourceCompatibleBitmask)
{
  if(m_PhysicalDeviceData.readbackMemIndex < 32 &&
     resourceCompatibleBitmask & (1 << m_PhysicalDeviceData.readbackMemIndex))
    return m_PhysicalDeviceData.readbackMemIndex;

  // for readbacks we want cached
  return m_PhysicalDeviceData.GetMemoryIndex(resourceCompatibleBitmask,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                             VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
}

uint32_t WrappedVulkan::GetUploadMemoryIndex(uint32_t resourceCompatibleBitmask)
{
  if(m_PhysicalDeviceData.uploadMemIndex < 32 &&
     resourceCompatibleBitmask & (1 << m_PhysicalDeviceData.uploadMemIndex))
    return m_PhysicalDeviceData.uploadMemIndex;

  // for upload, we just need host visible.
  // In an ideal world we'd put our uploaded data in device-local memory too (since host->device
  // copies will be slower than device->device copies), however device-local memory is a limited
  // resource and the capture may be using almost all of it, thus device local allocations should be
  // reserved for those that really need it.
  return m_PhysicalDeviceData.GetMemoryIndex(resourceCompatibleBitmask,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
}

uint32_t WrappedVulkan::GetGPULocalMemoryIndex(uint32_t resourceCompatibleBitmask)
{
  if(m_PhysicalDeviceData.GPULocalMemIndex < 32 &&
     resourceCompatibleBitmask & (1 << m_PhysicalDeviceData.GPULocalMemIndex))
    return m_PhysicalDeviceData.GPULocalMemIndex;

  // we don't actually need to require device local, but it is preferred
  return m_PhysicalDeviceData.GetMemoryIndex(resourceCompatibleBitmask, 0,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

uint32_t WrappedVulkan::PhysicalDeviceData::GetMemoryIndex(uint32_t resourceCompatibleBitmask,
                                                           uint32_t allocRequiredProps,
                                                           uint32_t allocPreferredProps)
{
  uint32_t best = memProps.memoryTypeCount;

  for(uint32_t memIndex = 0; memIndex < memProps.memoryTypeCount; memIndex++)
  {
    if(resourceCompatibleBitmask & (1 << memIndex))
    {
      uint32_t memTypeFlags = memProps.memoryTypes[memIndex].propertyFlags;

      if((memTypeFlags & allocRequiredProps) == allocRequiredProps)
      {
        // if this type has all preferred props, it is the best we can do. The driver is required to
        // order memory types that are otherwise equal in order of ascending performance.
        if((memTypeFlags & allocPreferredProps) == allocPreferredProps)
          return memIndex;

        // no best yet, this is the best we have
        if(best == memProps.memoryTypeCount)
        {
          best = memIndex;
        }
        else
        {
          // compare to the previous best. If it has more preferred props set, this is the new best
          uint32_t prevBestFlags = memProps.memoryTypes[best].propertyFlags;
          if((prevBestFlags & allocPreferredProps) < (memTypeFlags & allocPreferredProps))
          {
            best = memIndex;
          }
        }
      }
    }
  }

  if(best == memProps.memoryTypeCount)
  {
    RDCERR("Couldn't find any matching heap! mrq allows %x but required properties %x too strict",
           resourceCompatibleBitmask, allocRequiredProps);
    return 0;
  }

  return best;
}

MemoryAllocation WrappedVulkan::AllocateMemoryForResource(bool buffer, VkMemoryRequirements mrq,
                                                          MemoryScope scope, MemoryType type)
{
  const VkDeviceSize nonCoherentAtomSize = GetDeviceProps().limits.nonCoherentAtomSize;

  MemoryAllocation ret;
  ret.scope = scope;
  ret.type = type;
  ret.buffer = buffer;
  ret.size = AlignUp(mrq.size, mrq.alignment);
  // for ease, ensure all allocations are multiples of the non-coherent atom size, so we can
  // invalidate/flush safely. This is at most 256 bytes which is likely already satisfied.
  ret.size = AlignUp(ret.size, nonCoherentAtomSize);

  if(Vulkan_Debug_MemoryAllocationLogging())
  {
    RDCLOG("Allocating 0x%llx (0x%llx requested) with alignment 0x%llx in 0x%x for a %s (%s in %s)",
           ret.size, mrq.size, mrq.alignment, mrq.memoryTypeBits, buffer ? "buffer" : "image",
           ToStr(type).c_str(), ToStr(scope).c_str());
  }

  rdcarray<MemoryAllocation> &blockList = m_MemoryBlocks[(size_t)scope];

  // first try to find a match
  int i = 0;
  for(MemoryAllocation &block : blockList)
  {
    if(Vulkan_Debug_MemoryAllocationLogging())
    {
      RDCLOG(
          "Considering block %d: memory type %u and type %s. Total size 0x%llx, current offset "
          "0x%llx, last alloc was %s",
          i, block.memoryTypeIndex, ToStr(block.type).c_str(), block.size, block.offs,
          block.buffer ? "buffer" : "image");
    }
    i++;

    // skip this block if it's not the memory type we want
    if(ret.type != block.type || (mrq.memoryTypeBits & (1 << block.memoryTypeIndex)) == 0)
    {
      if(Vulkan_Debug_MemoryAllocationLogging())
      {
        RDCLOG("block type %d or memory type %d is incompatible", block.type, block.memoryTypeIndex);
      }
      continue;
    }

    // offs is where we can put our next sub-allocation
    VkDeviceSize offs = block.offs;

    // for ease, ensure all allocations are allocated to the non-coherent atom size, so we can
    // invalidate/flush safely. This is at most 256 bytes which is likely already satisfied.
    offs = AlignUp(offs, nonCoherentAtomSize);

    // if we are on a buffer/image, account for any alignment we might have to do
    if(ret.buffer != block.buffer)
      offs = AlignUp(offs, m_PhysicalDeviceData.props.limits.bufferImageGranularity);

    // align as required by the resource
    offs = AlignUp(offs, mrq.alignment);

    if(offs > block.size)
    {
      if(Vulkan_Debug_MemoryAllocationLogging())
      {
        RDCLOG("Next offset 0x%llx would be off the end of the memory (size 0x%llx).", offs,
               block.size);
      }
      continue;
    }

    VkDeviceSize avail = block.size - offs;

    if(Vulkan_Debug_MemoryAllocationLogging())
    {
      RDCLOG("At next offset 0x%llx, there's 0x%llx bytes available for 0x%llx bytes requested",
             offs, avail, ret.size);
    }

    // if the allocation will fit, we've found our candidate.
    if(ret.size <= avail)
    {
      // update the block offset and buffer/image bit
      block.offs = offs + ret.size;
      block.buffer = ret.buffer;

      // update our return value
      ret.offs = offs;
      ret.mem = block.mem;

      if(Vulkan_Debug_MemoryAllocationLogging())
      {
        RDCLOG("Allocating using this block: 0x%llx -> 0x%llx", ret.offs, block.offs);
      }

      // stop searching
      break;
    }
  }

  if(ret.mem == VK_NULL_HANDLE)
  {
    if(Vulkan_Debug_MemoryAllocationLogging())
    {
      RDCLOG("No available block found - allocating new block");
    }

    VkDeviceSize &allocSize = m_MemoryBlockSize[(size_t)scope];

    // we start allocating 32M, then increment each time we need a new block.
    switch(allocSize)
    {
      case 0: allocSize = 32; break;
      case 32: allocSize = 64; break;
      case 64: allocSize = 128; break;
      case 128:
      case 256: allocSize = 256; break;
      default:
        RDCWARN("Unexpected previous allocation size 0x%llx bytes, allocating 256MB", allocSize);
        allocSize = 256;
        break;
    }

    uint64_t initStateLimitMB = RenderDoc::Inst().GetCaptureOptions().softMemoryLimit;
    if(initStateLimitMB > 0)
      allocSize = RDCMAX(initStateLimitMB, allocSize);

    uint32_t memoryTypeIndex = 0;

    // Upload heaps are sometimes limited in size. To prevent OOM issues, deselect any memory types
    // corresponding to a small heap (<= 512MB) if there are other memory types available.
    for(uint32_t m = 0; m < 32; m++)
    {
      if(mrq.memoryTypeBits & (1U << m))
      {
        uint32_t heap = m_PhysicalDeviceData.memProps.memoryTypes[m].heapIndex;
        if(m_PhysicalDeviceData.memProps.memoryHeaps[heap].size <= 512 * 1024 * 1024)
        {
          if(mrq.memoryTypeBits > (1U << m))
          {
            if(Vulkan_Debug_MemoryAllocationLogging())
            {
              RDCLOG("Avoiding memory type %u due to small heap size (%llu)", m,
                     m_PhysicalDeviceData.memProps.memoryHeaps[heap].size);
            }
            mrq.memoryTypeBits &= ~(1U << m);
          }
        }
      }
    }

    switch(ret.type)
    {
      case MemoryType::Upload: memoryTypeIndex = GetUploadMemoryIndex(mrq.memoryTypeBits); break;
      case MemoryType::GPULocal:
        memoryTypeIndex = GetGPULocalMemoryIndex(mrq.memoryTypeBits);
        break;
      case MemoryType::Readback:
        memoryTypeIndex = GetReadbackMemoryIndex(mrq.memoryTypeBits);
        break;
    }

    VkMemoryAllocateInfo info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, allocSize * 1024 * 1024, memoryTypeIndex,
    };

    if(ret.size > info.allocationSize)
    {
      // if we get an over-sized allocation, first try to immediately jump to the largest block
      // size.
      if(initStateLimitMB == 0)
      {
        allocSize = 256;
        info.allocationSize = allocSize * 1024 * 1024;
      }

      // if it's still over-sized, just allocate precisely enough and give it a dedicated allocation
      if(ret.size > info.allocationSize)
      {
        if(Vulkan_Debug_MemoryAllocationLogging())
        {
          RDCLOG("Over-sized allocation for 0x%llx bytes", ret.size);
        }
        info.allocationSize = ret.size;
      }
    }

    if(Vulkan_Debug_MemoryAllocationLogging())
    {
      RDCLOG("Creating new allocation of 0x%llx bytes", info.allocationSize);
    }

    MemoryAllocation chunk;
    chunk.buffer = ret.buffer;
    chunk.memoryTypeIndex = memoryTypeIndex;
    chunk.scope = scope;
    chunk.type = type;
    chunk.size = info.allocationSize;

    // the offset starts immediately after this allocation
    chunk.offs = ret.size;

    VkDevice d = GetDev();

    // do the actual allocation
    VkResult vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &info, NULL, &chunk.mem);
    CheckVkResult(vkr);

    ret.offs = 0;
    ret.mem = VK_NULL_HANDLE;

    if(vkr != VK_SUCCESS)
      return ret;

    GetResourceManager()->WrapResource(Unwrap(d), chunk.mem);

    // push the new chunk
    blockList.push_back(chunk);

    // return the first bytes in the new chunk
    ret.mem = chunk.mem;
  }

  // ensure the returned size is accurate to what was requested, not what we padded
  ret.size = mrq.size;

  return ret;
}

MemoryAllocation WrappedVulkan::AllocateMemoryForResource(VkImage im, MemoryScope scope,
                                                          MemoryType type)
{
  VkDevice d = GetDev();

  VkMemoryRequirements mrq = {};
  ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), Unwrap(im), &mrq);

  return AllocateMemoryForResource(false, mrq, scope, type);
}

MemoryAllocation WrappedVulkan::AllocateMemoryForResource(VkBuffer buf, MemoryScope scope,
                                                          MemoryType type)
{
  VkDevice d = GetDev();

  VkMemoryRequirements mrq = {};
  ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(buf), &mrq);

  return AllocateMemoryForResource(true, mrq, scope, type);
}

uint64_t WrappedVulkan::CurMemoryUsage(MemoryScope scope)
{
  rdcarray<MemoryAllocation> &allocList = m_MemoryBlocks[(size_t)scope];

  uint64_t ret = 0;
  for(MemoryAllocation &alloc : allocList)
    ret += alloc.offs;

  return ret;
}

void WrappedVulkan::FreeAllMemory(MemoryScope scope)
{
  rdcarray<MemoryAllocation> &allocList = m_MemoryBlocks[(size_t)scope];

  if(allocList.empty())
    return;

  // freeing a lot of memory can take a while on some implementations. Since this only needs to
  // externally synchronise the memory we do it on a thread and synchronise if we need to free again
  // or on device shutdown
  if(m_MemoryFreeThread)
  {
    Threading::JoinThread(m_MemoryFreeThread);
    Threading::CloseThread(m_MemoryFreeThread);
    m_MemoryFreeThread = 0;
  }

  VkDevice d = GetDev();

  rdcarray<MemoryAllocation> allocs;
  allocs.swap(allocList);

  m_MemoryFreeThread = Threading::CreateThread([this, d, allocs]() {
    for(const MemoryAllocation &alloc : allocs)
    {
      ObjDisp(d)->FreeMemory(Unwrap(d), Unwrap(alloc.mem), NULL);
      GetResourceManager()->ReleaseWrappedResource(alloc.mem);
    }
  });
}

void WrappedVulkan::ResetMemoryBlocks(MemoryScope scope)
{
  rdcarray<MemoryAllocation> &allocList = m_MemoryBlocks[(size_t)scope];

  if(allocList.empty())
    return;

  for(MemoryAllocation &alloc : allocList)
    alloc.offs = 0;
}

void WrappedVulkan::FreeMemoryAllocation(MemoryAllocation alloc)
{
  // don't do anything at the moment, we only support freeing the whole scope at once.
}
