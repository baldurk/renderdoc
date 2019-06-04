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

#include "vk_core.h"

uint32_t WrappedVulkan::GetReadbackMemoryIndex(uint32_t resourceRequiredBitmask)
{
  if(resourceRequiredBitmask & (1 << m_PhysicalDeviceData.readbackMemIndex))
    return m_PhysicalDeviceData.readbackMemIndex;

  return m_PhysicalDeviceData.GetMemoryIndex(resourceRequiredBitmask,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
}

uint32_t WrappedVulkan::GetUploadMemoryIndex(uint32_t resourceRequiredBitmask)
{
  if(resourceRequiredBitmask & (1 << m_PhysicalDeviceData.uploadMemIndex))
    return m_PhysicalDeviceData.uploadMemIndex;

  return m_PhysicalDeviceData.GetMemoryIndex(resourceRequiredBitmask,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
}

uint32_t WrappedVulkan::GetGPULocalMemoryIndex(uint32_t resourceRequiredBitmask)
{
  if(resourceRequiredBitmask & (1 << m_PhysicalDeviceData.GPULocalMemIndex))
    return m_PhysicalDeviceData.GPULocalMemIndex;

  return m_PhysicalDeviceData.GetMemoryIndex(resourceRequiredBitmask,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

uint32_t WrappedVulkan::PhysicalDeviceData::GetMemoryIndex(uint32_t resourceRequiredBitmask,
                                                           uint32_t allocRequiredProps,
                                                           uint32_t allocUndesiredProps)
{
  uint32_t best = memProps.memoryTypeCount;

  for(uint32_t memIndex = 0; memIndex < memProps.memoryTypeCount; memIndex++)
  {
    if(resourceRequiredBitmask & (1 << memIndex))
    {
      uint32_t memTypeFlags = memProps.memoryTypes[memIndex].propertyFlags;

      if((memTypeFlags & allocRequiredProps) == allocRequiredProps)
      {
        if(memTypeFlags & allocUndesiredProps)
          best = memIndex;
        else
          return memIndex;
      }
    }
  }

  if(best == memProps.memoryTypeCount)
  {
    RDCERR("Couldn't find any matching heap! requirements %x / %x too strict",
           resourceRequiredBitmask, allocRequiredProps);
    return 0;
  }
  return best;
}

#define CREATE_NON_COHERENT_ATTRACTIVE_MEMORY 0

void WrappedVulkan::RemapMemoryIndices(VkPhysicalDeviceMemoryProperties *memProps,
                                       uint32_t **memIdxMap)
{
  uint32_t *memmap = new uint32_t[VK_MAX_MEMORY_TYPES];
  *memIdxMap = memmap;
  m_MemIdxMaps.push_back(memmap);

  for(size_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    memmap[i] = ~0U;

// basic idea here:
// We want to discourage coherent memory maps as much as possible while capturing,
// as they're painful to track. Unfortunately the spec guarantees that at least
// one such memory type will be available, and we must follow that.
//
// So, rather than removing the coherent memory type we make it as unappealing as
// possible and try and ensure that only someone looking specifically for a coherent
// memory type will find it. That way hopefully memory selection algorithms will
// pick non-coherent memory and do proper flushing as necessary.

// we want to add a new heap, hopefully there is room
#if CREATE_NON_COHERENT_ATTRACTIVE_MEMORY
  RDCASSERT(memProps->memoryHeapCount < VK_MAX_MEMORY_HEAPS - 1);

  uint32_t coherentHeap = memProps->memoryHeapCount;
  memProps->memoryHeapCount++;

  // make a new heap that's tiny. If any applications look at heap sizes to determine
  // viability, they'll dislike the look of this one (the real heaps should be much
  // bigger).
  memProps->memoryHeaps[coherentHeap].flags = 0;    // not device local
  memProps->memoryHeaps[coherentHeap].size = 32 * 1024 * 1024;
#endif

  // for every coherent memory type, add a non-coherent type first, then
  // mark the coherent type with our crappy heap

  uint32_t origCount = memProps->memoryTypeCount;
  VkMemoryType origTypes[VK_MAX_MEMORY_TYPES];
  memcpy(origTypes, memProps->memoryTypes, sizeof(origTypes));

  uint32_t newtypeidx = 0;

  for(uint32_t i = 0; i < origCount; i++)
  {
#if CREATE_NON_COHERENT_ATTRACTIVE_MEMORY
    if((origTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)
    {
      // coherent type found.

      // can we still add a new type without exceeding the max?
      if(memProps->memoryTypeCount + 1 <= VK_MAX_MEMORY_TYPES)
      {
        // copy both types from the original type
        memProps->memoryTypes[newtypeidx] = origTypes[i];
        memProps->memoryTypes[newtypeidx + 1] = origTypes[i];

        // mark first as non-coherent, cached
        memProps->memoryTypes[newtypeidx].propertyFlags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        memProps->memoryTypes[newtypeidx].propertyFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        // point second at bad heap
        memProps->memoryTypes[newtypeidx + 1].heapIndex = coherentHeap;

        // point both new types at this original type
        memmap[newtypeidx++] = i;
        memmap[newtypeidx++] = i;

        // we added a type
        memProps->memoryTypeCount++;
      }
      else
      {
        // can't add a new type, but we can at least repoint this coherent
        // type at the bad heap to discourage use
        memProps->memoryTypes[newtypeidx] = origTypes[i];
        memProps->memoryTypes[newtypeidx].heapIndex = coherentHeap;
        memmap[newtypeidx++] = i;
      }
    }
    else
#endif
    {
      // non-coherent already or non-hostvisible, just copy through
      memProps->memoryTypes[newtypeidx] = origTypes[i];
      memmap[newtypeidx++] = i;
    }
  }
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

  RDCDEBUG("Allocating 0x%llx with alignment 0x%llx in 0x%x for a %s (%s in %s)", ret.size,
           mrq.alignment, mrq.memoryTypeBits, buffer ? "buffer" : "image", ToStr(type).c_str(),
           ToStr(scope).c_str());

  std::vector<MemoryAllocation> &blockList = m_MemoryBlocks[(size_t)scope];

  // first try to find a match
  int i = 0;
  for(MemoryAllocation &block : blockList)
  {
    RDCDEBUG(
        "Considering block %d: memory type %u and type %s. Total size 0x%llx, current offset "
        "0x%llx, last alloc was %s",
        i, block.memoryTypeIndex, ToStr(block.type).c_str(), block.size, block.offs,
        block.buffer ? "buffer" : "image");
    i++;

    // skip this block if it's not the memory type we want
    if(ret.type != block.type || (mrq.memoryTypeBits & (1 << block.memoryTypeIndex)) == 0)
    {
      RDCDEBUG("block type %d or memory type %d is incompatible", block.type, block.memoryTypeIndex);
      continue;
    }

    // offs is where we can put our next sub-allocation
    VkDeviceSize offs = block.offs;

    // if we are on a buffer/image, account for any alignment we might have to do
    if(ret.buffer != block.buffer)
      offs = AlignUp(offs, m_PhysicalDeviceData.props.limits.bufferImageGranularity);

    // align as required by the resource
    offs = AlignUp(offs, mrq.alignment);

    if(offs > block.size)
    {
      RDCDEBUG("Next offset 0x%llx would be off the end of the memory (size 0x%llx).", offs,
               block.size);
      continue;
    }

    VkDeviceSize avail = block.size - offs;

    RDCDEBUG("At next offset 0x%llx, there's 0x%llx bytes available for 0x%llx bytes requested",
             offs, avail, ret.size);

    // if the allocation will fit, we've found our candidate.
    if(ret.size <= avail)
    {
      // update the block offset and buffer/image bit
      block.offs = offs + ret.size;
      block.buffer = ret.buffer;

      // update our return value
      ret.offs = offs;
      ret.mem = block.mem;

      RDCDEBUG("Allocating using this block: 0x%llx -> 0x%llx", ret.offs, block.offs);

      // stop searching
      break;
    }
  }

  if(ret.mem == VK_NULL_HANDLE)
  {
    RDCDEBUG("No available block found - allocating new block");

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
        RDCDEBUG("Unexpected previous allocation size 0x%llx bytes, allocating 256MB", allocSize);
        allocSize = 256;
        break;
    }

    uint32_t memoryTypeIndex = 0;

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
      allocSize = 256;
      info.allocationSize = allocSize * 1024 * 1024;

      // if it's still over-sized, just allocate precisely enough and give it a dedicated allocation
      if(ret.size > info.allocationSize)
      {
        RDCDEBUG("Over-sized allocation for 0x%llx bytes", ret.size);
        info.allocationSize = ret.size;
      }
    }

    RDCDEBUG("Creating new allocation of 0x%llx bytes", info.allocationSize);

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
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), chunk.mem);

    // push the new chunk
    blockList.push_back(chunk);

    // return the first bytes in the new chunk
    ret.offs = 0;
    ret.mem = chunk.mem;
  }

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

void WrappedVulkan::FreeAllMemory(MemoryScope scope)
{
  std::vector<MemoryAllocation> &allocList = m_MemoryBlocks[(size_t)scope];

  if(allocList.empty())
    return;

  VkDevice d = GetDev();

  for(MemoryAllocation alloc : allocList)
  {
    ObjDisp(d)->FreeMemory(Unwrap(d), Unwrap(alloc.mem), NULL);
    GetResourceManager()->ReleaseWrappedResource(alloc.mem);
  }

  allocList.clear();
}

void WrappedVulkan::FreeMemoryAllocation(MemoryAllocation alloc)
{
  // don't do anything at the moment, we only support freeing the whole scope at once.
}
