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

#include "../vk_core.h"

// Memory functions

bool WrappedVulkan::Serialise_vkAllocMemory(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkMemoryAllocInfo, info, *pAllocInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pMem));

	if(m_State == READING)
	{
		VkDeviceMemory mem = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		// VKTODOLOW may need to re-write info to change memory type index to the
		// appropriate index on replay
		VkResult ret = ObjDisp(device)->AllocMemory(Unwrap(device), &info, &mem);
		
		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), mem);
			GetResourceManager()->AddLiveResource(id, mem);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkAllocMemory(
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem)
{
	VkResult ret = ObjDisp(device)->AllocMemory(Unwrap(device), pAllocInfo, pMem);
	
	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pMem);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
					
				SCOPED_SERIALISE_CONTEXT(ALLOC_MEM);
				Serialise_vkAllocMemory(localSerialiser, device, pAllocInfo, pMem);

				chunk = scope.Get();
			}
			
			// create resource record for gpu memory
			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pMem);
			RDCASSERT(record);

			record->AddChunk(chunk);

			// VKTODOLOW Change record->Length to at least int64_t (maybe uint64_t)
			record->Length = (int32_t)pAllocInfo->allocationSize;
			RDCASSERT(pAllocInfo->allocationSize < 0x7FFFFFFF);

			// VKTODOMED always treat memory as dirty for now, so its initial state
			// is guaranteed to be prepared
			{
				SCOPED_LOCK(m_CapTransitionLock);
				if(m_State != WRITING_CAPFRAME)
					GetResourceManager()->MarkDirtyResource(id);
				else
					GetResourceManager()->MarkPendingDirty(id);
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pMem);
		}
	}

	return ret;
}

void WrappedVulkan::vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	// we just need to clean up after ourselves on replay
	WrappedVkNonDispRes *wrapped = (WrappedVkNonDispRes *)GetWrapped(mem);

	VkDeviceMemory unwrappedMem = wrapped->real.As<VkDeviceMemory>();

	GetResourceManager()->ReleaseWrappedResource(mem);

	ObjDisp(device)->FreeMemory(Unwrap(device), unwrappedMem);
}

VkResult WrappedVulkan::vkMapMemory(
			VkDevice                                    device,
			VkDeviceMemory                              mem,
			VkDeviceSize                                offset,
			VkDeviceSize                                size,
			VkMemoryMapFlags                            flags,
			void**                                      ppData)
{
	VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), offset, size, flags, ppData);

	if(ret == VK_SUCCESS && ppData)
	{
		ResourceId id = GetResID(mem);

		if(m_State >= WRITING)
		{
			MapState state;
			state.device = device;
			state.mappedPtr = *ppData;
			state.mapOffset = offset;
			state.mapSize = size == 0 ? GetRecord(mem)->Length : size;
			state.mapFrame = m_FrameCounter;
			state.mapFlags = flags;
			state.mapFlushed = false;
			state.refData = NULL;

			{
				SCOPED_LOCK(m_CurrentMapsLock);
				RDCASSERT(m_CurrentMaps.find(id) == m_CurrentMaps.end());
				m_CurrentMaps[id] = state;
			}
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkUnmapMemory(
		Serialiser*                                 localSerialiser,
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, id, GetResID(mem));

	MapState state;
	if(m_State >= WRITING)
	{
		SCOPED_LOCK(m_CurrentMapsLock);
		state = m_CurrentMaps[id];
	}

	SERIALISE_ELEMENT(VkMemoryMapFlags, flags, state.mapFlags);
	SERIALISE_ELEMENT(uint64_t, memOffset, state.mapOffset);
	SERIALISE_ELEMENT(uint64_t, memSize, state.mapSize);

	// VKTODOHIGH: this is really horrible - this could be write-combined memory that we're
	// reading from to get the latest data. This saves on having to fetch the data some
	// other way and provide an interception buffer to the app, but is awful.
	// we're also not doing any diff range checks, just serialising the whole memory region.
	// In vulkan the common case will be one memory region for a large number of distinct
	// bits of data so most maps will not change the whole region.
	SERIALISE_ELEMENT_BUF(byte*, data, (byte *)state.mappedPtr + state.mapOffset, (size_t)memSize);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(id);

		// VKTODOLOW figure out what alignments there are on mapping, so we only map the region
		// we're going to modify. For no, offset/size is handled in the memcpy before and we
		// map the whole region
		void *mapPtr = NULL;
		VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), 0, 0, flags, &mapPtr);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Error mapping memory on replay: 0x%08x", ret);
		}
		else
		{
			memcpy((byte *)mapPtr+memOffset, data, (size_t)memSize);

			ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
		}

		SAFE_DELETE_ARRAY(data);
	}

	return true;
}

void WrappedVulkan::vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
	
	if(m_State >= WRITING)
	{
		ResourceId id = GetResID(mem);

		MapState state;

		{
			SCOPED_LOCK(m_CurrentMapsLock);

			auto it = m_CurrentMaps.find(id);
			if(it == m_CurrentMaps.end())
				RDCERR("vkUnmapMemory for memory handle that's not currently mapped");

			state = it->second;
		}

		{
			// decide atomically if this chunk should be in-frame or not
			// so that we're not in the else branch but haven't marked
			// dirty when capframe starts, then we mark dirty while in-frame

			bool capframe = false;
			{
				SCOPED_LOCK(m_CapTransitionLock);
				capframe = (m_State == WRITING_CAPFRAME);

				if(!capframe)
					GetResourceManager()->MarkDirtyResource(GetResID(mem));
			}

			if(capframe)
			{
				if(!state.mapFlushed)
				{
					CACHE_THREAD_SERIALISER();

					SCOPED_SERIALISE_CONTEXT(UNMAP_MEM);
					Serialise_vkUnmapMemory(localSerialiser, device, mem);

					VkResourceRecord *record = GetRecord(mem);

					if(m_State == WRITING_IDLE)
					{
						record->AddChunk(scope.Get());
					}
					else
					{
						m_FrameCaptureRecord->AddChunk(scope.Get());
						GetResourceManager()->MarkResourceFrameReferenced(GetResID(mem), eFrameRef_Write);
					}
				}
				else
				{
					// VKTODOLOW for now assuming flushes cover all writes. Technically
					// this is true for all non-coherent memory types.
				}
			}

			state.mappedPtr = NULL;
			SAFE_DELETE_ARRAY(state.refData);
		}

		{
			SCOPED_LOCK(m_CurrentMapsLock);

			auto it = m_CurrentMaps.find(id);
			if(it == m_CurrentMaps.end())
				RDCERR("vkUnmapMemory for memory handle that's not currently mapped");

			state = it->second;
			m_CurrentMaps.erase(it);
		}
	}
}

bool WrappedVulkan::Serialise_vkFlushMappedMemoryRanges(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			uint32_t                                    memRangeCount,
			const VkMappedMemoryRange*                  pMemRanges)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, id, GetResID(pMemRanges->mem));
	
	MapState state;
	if(m_State >= WRITING)
	{
		SCOPED_LOCK(m_CurrentMapsLock);
		state = m_CurrentMaps[id];
	
		// don't support any extensions on VkMappedMemoryRange
		RDCASSERT(pMemRanges->pNext == NULL);
	}

	SERIALISE_ELEMENT(VkMemoryMapFlags, flags, state.mapFlags);
	SERIALISE_ELEMENT(uint64_t, memOffset, pMemRanges->offset);
	SERIALISE_ELEMENT(uint64_t, memSize, pMemRanges->size);

	// VKTODOHIGH: this is really horrible - this could be write-combined memory that we're
	// reading from to get the latest data. This saves on having to fetch the data some
	// other way and provide an interception buffer to the app, but is awful.
	// we're also not doing any diff range checks, just serialising the whole memory region.
	// In vulkan the common case will be one memory region for a large number of distinct
	// bits of data so most maps will not change the whole region.
	SERIALISE_ELEMENT_BUF(byte*, data, (byte *)state.mappedPtr + (size_t)memOffset, (size_t)memSize);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDeviceMemory mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(id);

		// VKTODOLOW figure out what alignments there are on mapping, so we only map the region
		// we're going to modify. For no, offset/size is handled in the memcpy before and we
		// map the whole region
		void *mapPtr = NULL;
		VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), 0, 0, flags, &mapPtr);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Error mapping memory on replay: 0x%08x", ret);
		}
		else
		{
			memcpy((byte *)mapPtr+memOffset, data, (size_t)memSize);

			ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
		}

		SAFE_DELETE_ARRAY(data);
	}

	return true;
}

VkResult WrappedVulkan::vkFlushMappedMemoryRanges(
			VkDevice                                    device,
			uint32_t                                    memRangeCount,
			const VkMappedMemoryRange*                  pMemRanges)
{
	VkMappedMemoryRange *unwrapped = new VkMappedMemoryRange[memRangeCount];
	for(uint32_t i=0; i < memRangeCount; i++)
	{
		unwrapped[i] = pMemRanges[i];
		unwrapped[i].mem = Unwrap(unwrapped[i].mem);
	}

	VkResult ret = ObjDisp(device)->FlushMappedMemoryRanges(Unwrap(device), memRangeCount, unwrapped);

	SAFE_DELETE_ARRAY(unwrapped);

	for(uint32_t i=0; i < memRangeCount; i++)
	{
		ResourceId memid = GetResID(pMemRanges[i].mem);

		{
			SCOPED_LOCK(m_CurrentMapsLock);
			auto it = m_CurrentMaps.find(memid);
			RDCASSERT(it != m_CurrentMaps.end());
			it->second.mapFlushed = true;
		}

		if(ret == VK_SUCCESS && m_State >= WRITING_CAPFRAME)
		{
			CACHE_THREAD_SERIALISER();
		
			SCOPED_SERIALISE_CONTEXT(FLUSH_MEM);
			Serialise_vkFlushMappedMemoryRanges(localSerialiser, device, 1, pMemRanges+i);

			m_FrameCaptureRecord->AddChunk(scope.Get());
			GetResourceManager()->MarkResourceFrameReferenced(GetResID(pMemRanges[i].mem), eFrameRef_Write);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkInvalidateMappedMemoryRanges(
		VkDevice                                    device,
		uint32_t                                    memRangeCount,
		const VkMappedMemoryRange*                  pMemRanges)
{
	// VKTODOHIGH stub function
	return ObjDisp(device)->InvalidateMappedMemoryRanges(Unwrap(device), memRangeCount, pMemRanges);
}

VkResult WrappedVulkan::vkGetDeviceMemoryCommitment(
		VkDevice                                    device,
		VkDeviceMemory                              memory,
		VkDeviceSize*                               pCommittedMemoryInBytes)
{
	return ObjDisp(device)->GetDeviceMemoryCommitment(Unwrap(device), Unwrap(memory), pCommittedMemoryInBytes);
}

// Generic API object functions

bool WrappedVulkan::Serialise_vkBindBufferMemory(
		Serialiser*                                 localSerialiser,
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, bufId, GetResID(buffer));
	SERIALISE_ELEMENT(ResourceId, memId, GetResID(mem));
	SERIALISE_ELEMENT(uint64_t, offs, memOffset);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(memId);

		ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(mem), offs);
	}

	return true;
}

VkResult WrappedVulkan::vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	VkResourceRecord *record = GetRecord(buffer);

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			CACHE_THREAD_SERIALISER();
		
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_MEM);
			Serialise_vkBindBufferMemory(localSerialiser, device, buffer, mem, memOffset);

			chunk = scope.Get();
		}
	
		// memory object bindings are immutable and must happen before creation or use,
		// so this can always go into the record, even if a resource is created and bound
		// to memory mid-frame
		record->AddChunk(chunk);

		record->AddParent(GetRecord(mem));
		record->baseResource = GetResID(mem);
	}

	return ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(mem), memOffset);
}

bool WrappedVulkan::Serialise_vkBindImageMemory(
		Serialiser*                                 localSerialiser,
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, imgId, GetResID(image));
	SERIALISE_ELEMENT(ResourceId, memId, GetResID(mem));
	SERIALISE_ELEMENT(uint64_t, offs, memOffset);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(memId);

		ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(mem), offs);

		m_ImageLayouts[GetResID(image)].mem = mem;
	}

	return true;
}

VkResult WrappedVulkan::vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	VkResourceRecord *record = GetRecord(image);

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			CACHE_THREAD_SERIALISER();
		
			SCOPED_SERIALISE_CONTEXT(BIND_IMAGE_MEM);
			Serialise_vkBindImageMemory(localSerialiser, device, image, mem, memOffset);

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

bool WrappedVulkan::Serialise_vkQueueBindSparseBufferMemory(
	Serialiser*                                 localSerialiser,
	VkQueue                                     queue,
	VkBuffer                                    buffer,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	// VKTODOHIGH stub function
	return true;
}

VkResult WrappedVulkan::vkQueueBindSparseBufferMemory(
	VkQueue                                     queue,
	VkBuffer                                    buffer,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	// VKTODOHIGH stub function
	RDCWARN("Sparse memory operations are not yet implemented");
	return ObjDisp(queue)->QueueBindSparseBufferMemory(Unwrap(queue), Unwrap(buffer), numBindings, pBindInfo);
}

bool WrappedVulkan::Serialise_vkQueueBindSparseImageOpaqueMemory(
	Serialiser*                                 localSerialiser,
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	// VKTODOHIGH stub function
	return true;
}

VkResult WrappedVulkan::vkQueueBindSparseImageOpaqueMemory(
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	// VKTODOHIGH stub function
	RDCWARN("Sparse memory operations are not yet implemented");
	return ObjDisp(queue)->QueueBindSparseImageOpaqueMemory(Unwrap(queue), Unwrap(image), numBindings, pBindInfo);
}

bool WrappedVulkan::Serialise_vkQueueBindSparseImageMemory(
	Serialiser*                                 localSerialiser,
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseImageMemoryBindInfo*          pBindInfo)
{
	// VKTODOHIGH stub function
	return true;
}

VkResult WrappedVulkan::vkQueueBindSparseImageMemory(
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseImageMemoryBindInfo*          pBindInfo)
{
	// VKTODOHIGH stub function
	RDCWARN("Sparse memory operations are not yet implemented");
	return ObjDisp(queue)->QueueBindSparseImageMemory(Unwrap(queue), Unwrap(image), numBindings, pBindInfo);
}

bool WrappedVulkan::Serialise_vkCreateBuffer(
		Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkBufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pBuffer));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkBuffer buf = VK_NULL_HANDLE;

		VkBufferUsageFlags origusage = info.usage;

		// ensure we can always readback from buffers
		info.usage |= VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT;

		VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &info, &buf);

		info.usage = origusage;

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), buf);
			GetResourceManager()->AddLiveResource(id, buf);

			m_CreationInfo.m_Buffer[live].Init(GetResourceManager(), &info);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateBuffer(
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer)
{
	VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), pCreateInfo, pBuffer);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pBuffer);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER);
				Serialise_vkCreateBuffer(localSerialiser, device, pCreateInfo, pBuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pBuffer);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pBuffer);

			m_CreationInfo.m_Buffer[id].Init(GetResourceManager(), pCreateInfo);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateBufferView(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkBufferViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pView));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkBufferView view = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), view);
			GetResourceManager()->AddLiveResource(id, view);
		
			m_CreationInfo.m_BufferView[live].Init(GetResourceManager(), &info);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateBufferView(
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView)
{
	VkBufferViewCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
	VkResult ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &unwrappedInfo, pView);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER_VIEW);
				Serialise_vkCreateBufferView(localSerialiser, device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *bufferRecord = GetRecord(pCreateInfo->buffer);

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pView);
			record->AddChunk(chunk);
			record->AddParent(bufferRecord);

			// store the base resource
			record->baseResource = bufferRecord->baseResource;
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pView);
		
			m_CreationInfo.m_BufferView[id].Init(GetResourceManager(), &unwrappedInfo);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateImage(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkImageCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pImage));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkImage img = VK_NULL_HANDLE;

		VkImageUsageFlags origusage = info.usage;

		// ensure we can always display and copy from/to textures
		info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT;

		VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), &info, &img);

		info.usage = origusage;

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), img);
			GetResourceManager()->AddLiveResource(id, img);
			
			m_CreationInfo.m_Image[live].Init(GetResourceManager(), &info);
			
			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArrayLayer = 0;
			range.mipLevels = info.mipLevels;
			range.arraySize = info.arraySize;
			if(info.imageType == VK_IMAGE_TYPE_3D)
				range.arraySize = info.extent.depth;
			
			ImageLayouts &layouts = m_ImageLayouts[live];
			layouts.subresourceStates.clear();
			
			if(!IsDepthStencilFormat(info.format))
			{
				range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  layouts.subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			}
			else
			{
				range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  layouts.subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
				range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;layouts.subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			}
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateImage(
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage)
{
	VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), pCreateInfo, pImage);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pImage);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_IMAGE);
				Serialise_vkCreateImage(localSerialiser, device, pCreateInfo, pImage);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pImage);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pImage);
			
			m_CreationInfo.m_Image[id].Init(GetResourceManager(), pCreateInfo);
		}

		VkImageSubresourceRange range;
		range.baseMipLevel = range.baseArrayLayer = 0;
		range.mipLevels = pCreateInfo->mipLevels;
		range.arraySize = pCreateInfo->arraySize;
		if(pCreateInfo->imageType == VK_IMAGE_TYPE_3D)
			range.arraySize = pCreateInfo->extent.depth;

		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			m_ImageLayouts[id].subresourceStates.clear();
			if(!IsDepthStencilFormat(pCreateInfo->format))
			{
				range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  m_ImageLayouts[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			}
			else
			{
				range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  m_ImageLayouts[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
				range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;m_ImageLayouts[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			}
		}
	}

	return ret;
}

// Image view functions

bool WrappedVulkan::Serialise_vkCreateImageView(
		Serialiser*                                 localSerialiser,
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    VkImageView*                                pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkImageViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pView));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkImageView view = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), view);
			GetResourceManager()->AddLiveResource(id, view);
		
			m_CreationInfo.m_ImageView[live].Init(GetResourceManager(), &info);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    VkImageView*                                pView)
{
	VkImageViewCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.image = Unwrap(unwrappedInfo.image);
	VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &unwrappedInfo, pView);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_IMAGE_VIEW);
				Serialise_vkCreateImageView(localSerialiser, device, pCreateInfo, pView);

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
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pView);
		
			m_CreationInfo.m_ImageView[id].Init(GetResourceManager(), &unwrappedInfo);
		}
	}

	return ret;
}
