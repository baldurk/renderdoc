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

		// serialised memory type index is non-remapped, so we remap now.
		// VKTODOLOW may need to re-write info to change memory type index to the
		// appropriate index on replay
		info.memoryTypeIndex = m_PhysicalDeviceData.memIdxMap[info.memoryTypeIndex];

		VkResult ret = ObjDisp(device)->AllocMemory(Unwrap(device), &info, &mem);
		
		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), mem);
			GetResourceManager()->AddLiveResource(id, mem);

			m_CreationInfo.m_Memory[live].Init(GetResourceManager(), &info);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkAllocMemory(
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem)
{
	VkMemoryAllocInfo info = *pAllocInfo;
	if(m_State >= WRITING)
		info.memoryTypeIndex = GetRecord(device)->memIdxMap[info.memoryTypeIndex];
	VkResult ret = ObjDisp(device)->AllocMemory(Unwrap(device), &info, pMem);
	
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

			uint32_t memProps = m_PhysicalDeviceData.fakeMemProps->memoryTypes[pAllocInfo->memoryTypeIndex].propertyFlags;

			// if memory is not host visible, so not mappable, don't create map state at all
			if((memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
			{
				record->memMapState = new MemMapState();
				record->memMapState->mapCoherent = (memProps & VK_MEMORY_PROPERTY_HOST_NON_COHERENT_BIT) == 0;
				record->memMapState->refData = NULL;
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pMem);

			m_CreationInfo.m_Memory[id].Init(GetResourceManager(), pAllocInfo);
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
	void *realData = NULL;
	VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), offset, size, flags, &realData);

	if(ret == VK_SUCCESS && realData)
	{
		ResourceId id = GetResID(mem);

		if(m_State >= WRITING)
		{
			VkResourceRecord *memrecord = GetRecord(mem);

			// must have map state, only non host visible memories have no map
			// state, and they can't be mapped!
			RDCASSERT(memrecord->memMapState);
			MemMapState &state = *memrecord->memMapState;

			// ensure size is valid
			RDCASSERT(size == 0 || size <= (VkDeviceSize)memrecord->Length);

			state.mappedPtr = (byte *)realData;
			state.refData = NULL;

			state.mapOffset = offset;
			state.mapSize = size == 0 ? memrecord->Length : size;
			state.mapFlushed = false;

			*ppData = realData;

			if(state.mapCoherent)
			{
				SCOPED_LOCK(m_CoherentMapsLock);
				m_CoherentMaps.push_back(memrecord);
			}
		}
	}
	else
	{
		*ppData = NULL;
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

	MemMapState *state;
	if(m_State >= WRITING)
		state = GetRecord(mem)->memMapState;

	SERIALISE_ELEMENT(uint64_t, memOffset, state->mapOffset);
	SERIALISE_ELEMENT(uint64_t, memSize, state->mapSize);
	SERIALISE_ELEMENT_BUF(byte*, data, (byte *)state->mappedPtr + state->mapOffset, (size_t)memSize);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(id);

		// VKTODOLOW figure out what alignments there are on mapping, so we only map the region
		// we're going to modify. For now, offset/size is handled in the memcpy before and we
		// map the whole region
		void *mapPtr = NULL;
		VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), 0, 0, 0, &mapPtr);

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
	if(m_State >= WRITING)
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
				capframe = (m_State == WRITING_CAPFRAME);

				if(!capframe)
					GetResourceManager()->MarkDirtyResource(id);
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
						GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Write);
					}
				}
				else
				{
					// VKTODOLOW for now assuming flushes cover all writes. Technically
					// this is true for all non-coherent memory types.
				}
			}

			state.mappedPtr = NULL;
		}

		Serialiser::FreeAlignedBuffer(state.refData);

		if(state.mapCoherent)
		{
			SCOPED_LOCK(m_CoherentMapsLock);

			auto it = std::find(m_CoherentMaps.begin(), m_CoherentMaps.end(), memrecord);
			if(it == m_CoherentMaps.end())
				RDCERR("vkUnmapMemory for memory handle that's not currently mapped");

			m_CoherentMaps.erase(it);
		}
	}

	ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
}

bool WrappedVulkan::Serialise_vkFlushMappedMemoryRanges(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			uint32_t                                    memRangeCount,
			const VkMappedMemoryRange*                  pMemRanges)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, id, GetResID(pMemRanges->mem));
	
	MemMapState *state;
	if(m_State >= WRITING)
	{
		state = GetRecord(pMemRanges->mem)->memMapState;
	
		// don't support any extensions on VkMappedMemoryRange
		RDCASSERT(pMemRanges->pNext == NULL);
	}

	SERIALISE_ELEMENT(uint64_t, memOffset, pMemRanges->offset);
	SERIALISE_ELEMENT(uint64_t, memSize, pMemRanges->size);
	SERIALISE_ELEMENT_BUF(byte*, data, state->mappedPtr + (size_t)memOffset, (size_t)memSize);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDeviceMemory mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(id);

		// VKTODOLOW figure out what alignments there are on mapping, so we only map the region
		// we're going to modify. For no, offset/size is handled in the memcpy before and we
		// map the whole region
		void *mapPtr = NULL;
		VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), 0, 0, 0, &mapPtr);

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
	if(m_State >= WRITING)
	{
		bool capframe = false;
		{
			SCOPED_LOCK(m_CapTransitionLock);
			capframe = (m_State == WRITING_CAPFRAME);
		}

		for(uint32_t i = 0; i < memRangeCount; i++)
		{
			ResourceId memid = GetResID(pMemRanges[i].mem);
			
			MemMapState *state = GetRecord(pMemRanges[i].mem)->memMapState;
			state->mapFlushed = true;

			if(capframe)
			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(FLUSH_MEM);
				Serialise_vkFlushMappedMemoryRanges(localSerialiser, device, 1, pMemRanges + i);

				m_FrameCaptureRecord->AddChunk(scope.Get());
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pMemRanges[i].mem), eFrameRef_Write);
			}
		}
	}
	
	VkMappedMemoryRange *unwrapped = new VkMappedMemoryRange[memRangeCount];
	for(uint32_t i=0; i < memRangeCount; i++)
	{
		unwrapped[i] = pMemRanges[i];
		unwrapped[i].mem = Unwrap(unwrapped[i].mem);
	}

	VkResult ret = ObjDisp(device)->FlushMappedMemoryRanges(Unwrap(device), memRangeCount, unwrapped);

	SAFE_DELETE_ARRAY(unwrapped);

	return ret;
}

VkResult WrappedVulkan::vkInvalidateMappedMemoryRanges(
		VkDevice                                    device,
		uint32_t                                    memRangeCount,
		const VkMappedMemoryRange*                  pMemRanges)
{
	// don't need to serialise this, readback from mapped memory is not captured
	// and is only relevant for the application.
	return ObjDisp(device)->InvalidateMappedMemoryRanges(Unwrap(device), memRangeCount, pMemRanges);
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
		m_ImageLayouts[GetResID(image)].memoffs = offs;
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
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));

	SERIALISE_ELEMENT(uint32_t, num, numBindings);
	SERIALISE_ELEMENT_ARR(VkSparseMemoryBindInfo, binds, pBindInfo, num);
	
	if(m_State < WRITING && GetResourceManager()->HasLiveResource(bufid))
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(queue)->QueueBindSparseBufferMemory(Unwrap(queue), Unwrap(buffer), num, binds);
	}

	SAFE_DELETE_ARRAY(binds);

	return true;
}

VkResult WrappedVulkan::vkQueueBindSparseBufferMemory(
	VkQueue                                     queue,
	VkBuffer                                    buffer,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(BIND_SPARSE_BUF);
		Serialise_vkQueueBindSparseBufferMemory(localSerialiser, queue, buffer, numBindings, pBindInfo);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		// image isn't marked referenced. If the only ref is a memory bind, we just skip it
	}

	if(m_State >= WRITING)
		GetRecord(buffer)->sparseInfo->Update(numBindings, pBindInfo);

	VkSparseMemoryBindInfo *unwrappedBinds = GetTempArray<VkSparseMemoryBindInfo>(numBindings);
	memcpy(unwrappedBinds, pBindInfo, sizeof(VkSparseMemoryBindInfo)*numBindings);
	for(uint32_t i=0; i < numBindings; i++) unwrappedBinds[i].mem = Unwrap(unwrappedBinds[i].mem);

	return ObjDisp(queue)->QueueBindSparseBufferMemory(Unwrap(queue), Unwrap(buffer), numBindings, unwrappedBinds);
}

bool WrappedVulkan::Serialise_vkQueueBindSparseImageOpaqueMemory(
	Serialiser*                                 localSerialiser,
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, imid, GetResID(image));

	SERIALISE_ELEMENT(uint32_t, num, numBindings);
	SERIALISE_ELEMENT_ARR(VkSparseMemoryBindInfo, binds, pBindInfo, num);
	
	if(m_State < WRITING && GetResourceManager()->HasLiveResource(imid))
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imid);

		ObjDisp(queue)->QueueBindSparseImageOpaqueMemory(Unwrap(queue), Unwrap(image), num, binds);
	}

	SAFE_DELETE_ARRAY(binds);

	return true;
}

VkResult WrappedVulkan::vkQueueBindSparseImageOpaqueMemory(
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseMemoryBindInfo*               pBindInfo)
{
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(BIND_SPARSE_OPAQUE_IM);
		Serialise_vkQueueBindSparseImageOpaqueMemory(localSerialiser, queue, image, numBindings, pBindInfo);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		// image isn't marked referenced. If the only ref is a memory bind, we just skip it
	}

	if(m_State >= WRITING)
		GetRecord(image)->sparseInfo->Update(numBindings, pBindInfo);
	
	VkSparseMemoryBindInfo *unwrappedBinds = GetTempArray<VkSparseMemoryBindInfo>(numBindings);
	memcpy(unwrappedBinds, pBindInfo, sizeof(VkSparseMemoryBindInfo)*numBindings);
	for(uint32_t i=0; i < numBindings; i++) unwrappedBinds[i].mem = Unwrap(unwrappedBinds[i].mem);

	return ObjDisp(queue)->QueueBindSparseImageOpaqueMemory(Unwrap(queue), Unwrap(image), numBindings, unwrappedBinds);
}

bool WrappedVulkan::Serialise_vkQueueBindSparseImageMemory(
	Serialiser*                                 localSerialiser,
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseImageMemoryBindInfo*          pBindInfo)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, imid, GetResID(image));

	SERIALISE_ELEMENT(uint32_t, num, numBindings);
	SERIALISE_ELEMENT_ARR(VkSparseImageMemoryBindInfo, binds, pBindInfo, num);
	
	if(m_State < WRITING && GetResourceManager()->HasLiveResource(imid))
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imid);

		ObjDisp(queue)->QueueBindSparseImageMemory(Unwrap(queue), Unwrap(image), num, binds);
	}

	SAFE_DELETE_ARRAY(binds);

	return true;
}

VkResult WrappedVulkan::vkQueueBindSparseImageMemory(
	VkQueue                                     queue,
	VkImage                                     image,
	uint32_t                                    numBindings,
	const VkSparseImageMemoryBindInfo*          pBindInfo)
{
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(BIND_SPARSE_IM);
		Serialise_vkQueueBindSparseImageMemory(localSerialiser, queue, image, numBindings, pBindInfo);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		// image isn't marked referenced. If the only ref is a memory bind, we just skip it
	}

	if(m_State >= WRITING)
		GetRecord(image)->sparseInfo->Update(numBindings, pBindInfo);
	
	VkSparseImageMemoryBindInfo *unwrappedBinds = GetTempArray<VkSparseImageMemoryBindInfo>(numBindings);
	memcpy(unwrappedBinds, pBindInfo, sizeof(VkSparseImageMemoryBindInfo)*numBindings);
	for(uint32_t i=0; i < numBindings; i++) unwrappedBinds[i].mem = Unwrap(unwrappedBinds[i].mem);

	return ObjDisp(queue)->QueueBindSparseImageMemory(Unwrap(queue), Unwrap(image), numBindings, unwrappedBinds);
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

			if(pCreateInfo->flags & (VK_BUFFER_CREATE_SPARSE_BINDING_BIT|VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT))
			{
				record->sparseInfo = new SparseMapping();

				// buffers are always bound opaquely and in arbitrary divisions, sparse residency
				// only means not all the buffer needs to be bound, which is not that interesting for
				// our purposes

				{
					SCOPED_LOCK(m_CapTransitionLock);
					if(m_State != WRITING_CAPFRAME)
						GetResourceManager()->MarkDirtyResource(id);
					else
						GetResourceManager()->MarkPendingDirty(id);
				}
			}
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
			record->sparseInfo = bufferRecord->sparseInfo;
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
			
			layouts.arraySize = info.arraySize;
			layouts.mipLevels = info.mipLevels;
			layouts.extent = info.extent;
			layouts.format = info.format;

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

			if(pCreateInfo->flags & (VK_IMAGE_CREATE_SPARSE_BINDING_BIT|VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT))
			{
				record->sparseInfo = new SparseMapping();
				
				{
					SCOPED_LOCK(m_CapTransitionLock);
					if(m_State != WRITING_CAPFRAME)
						GetResourceManager()->MarkDirtyResource(id);
					else
						GetResourceManager()->MarkPendingDirty(id);
				}

				if(pCreateInfo->flags & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
				{
					// must record image and page dimension, and create page tables
					uint32_t numreqs = VK_IMAGE_ASPECT_NUM;
					VkSparseImageMemoryRequirements reqs[VK_IMAGE_ASPECT_NUM];
					ObjDisp(device)->GetImageSparseMemoryRequirements(Unwrap(device), Unwrap(*pImage), &numreqs, reqs);

					RDCASSERT(numreqs > 0);
					
					record->sparseInfo->pagedim = reqs[0].formatProps.imageGranularity;
					record->sparseInfo->imgdim = pCreateInfo->extent;
					record->sparseInfo->imgdim.width /= record->sparseInfo->pagedim.width;
					record->sparseInfo->imgdim.height /= record->sparseInfo->pagedim.height;
					record->sparseInfo->imgdim.depth /= record->sparseInfo->pagedim.depth;
					
					uint32_t numpages = record->sparseInfo->imgdim.width*record->sparseInfo->imgdim.height*record->sparseInfo->imgdim.depth;

					for(uint32_t i=0; i < numreqs; i++)
					{
						// assume all page sizes are the same for all aspects
						RDCASSERT(record->sparseInfo->pagedim.width == reqs[i].formatProps.imageGranularity.width &&
							record->sparseInfo->pagedim.height == reqs[i].formatProps.imageGranularity.height &&
							record->sparseInfo->pagedim.depth == reqs[i].formatProps.imageGranularity.depth);

						record->sparseInfo->pages[reqs[i].formatProps.aspect] = new pair<VkDeviceMemory, VkDeviceSize>[numpages];
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
			
			m_CreationInfo.m_Image[id].Init(GetResourceManager(), pCreateInfo);
		}

		VkImageSubresourceRange range;
		range.baseMipLevel = range.baseArrayLayer = 0;
		range.mipLevels = pCreateInfo->mipLevels;
		range.arraySize = pCreateInfo->arraySize;
		if(pCreateInfo->imageType == VK_IMAGE_TYPE_3D)
			range.arraySize = pCreateInfo->extent.depth;

		ImageLayouts *layout = NULL;
		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			layout = &m_ImageLayouts[id];
		}

		layout->arraySize = pCreateInfo->arraySize;
		layout->mipLevels = pCreateInfo->mipLevels;
		layout->extent = pCreateInfo->extent;
		layout->format = pCreateInfo->format;

		layout->subresourceStates.clear();
		if(!IsDepthStencilFormat(pCreateInfo->format))
		{
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  layout->subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
		}
		else
		{
			range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  layout->subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;layout->subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
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
			record->sparseInfo = imageRecord->sparseInfo;
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pView);
		
			m_CreationInfo.m_ImageView[id].Init(GetResourceManager(), &unwrappedInfo);
		}
	}

	return ret;
}
