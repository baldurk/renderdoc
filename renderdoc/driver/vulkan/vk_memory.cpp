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
