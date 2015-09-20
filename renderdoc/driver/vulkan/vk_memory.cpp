/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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
	if(resourceRequiredBitmask & (1 << m_PhysicalReplayData[m_SwapPhysDevice].readbackMemIndex))
		return m_PhysicalReplayData[m_SwapPhysDevice].readbackMemIndex;

	return m_PhysicalReplayData[m_SwapPhysDevice].GetMemoryIndex(
		resourceRequiredBitmask,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT);
}

uint32_t WrappedVulkan::GetUploadMemoryIndex(uint32_t resourceRequiredBitmask)
{
	if(resourceRequiredBitmask & (1 << m_PhysicalReplayData[m_SwapPhysDevice].uploadMemIndex))
		return m_PhysicalReplayData[m_SwapPhysDevice].uploadMemIndex;

	return m_PhysicalReplayData[m_SwapPhysDevice].GetMemoryIndex(
		resourceRequiredBitmask,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
}

uint32_t WrappedVulkan::GetGPULocalMemoryIndex(uint32_t resourceRequiredBitmask)
{
	if(resourceRequiredBitmask & (1 << m_PhysicalReplayData[m_SwapPhysDevice].GPULocalMemIndex))
		return m_PhysicalReplayData[m_SwapPhysDevice].GPULocalMemIndex;

	return m_PhysicalReplayData[m_SwapPhysDevice].GetMemoryIndex(
		resourceRequiredBitmask,
		VK_MEMORY_PROPERTY_DEVICE_ONLY, 0);
}

uint32_t WrappedVulkan::ReplayData::GetMemoryIndex(uint32_t resourceRequiredBitmask, uint32_t allocRequiredProps, uint32_t allocUndesiredProps)
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
		RDCERR("Couldn't find any matching heap! requirements %x / %x too strict", resourceRequiredBitmask, allocRequiredProps);
		return 0;
	}
	return best;
}
