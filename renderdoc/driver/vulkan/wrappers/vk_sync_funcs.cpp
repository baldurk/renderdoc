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

/*
 * Events and fences need careful handling.
 *
 * Primary goal by far is correctness - these primitives are used to synchronise
 * operations between GPU-CPU and GPU-GPU, and we need to be sure that we don't
 * introduce any bugs with bad handling.
 *
 * Secondary goal and worth compromising is to be efficient in replaying them.
 *
 * Fences are comparatively 'easy'. Since the GPU can't wait on them, for the
 * moment we just implement fences as-is and do a hard sync via DeviceWaitIdle
 * whenever the status of a fence would have been fetched on the GPU. Obviously
 * this is very conservative, but it's correct and it doesn't impact efficiency
 * too badly (The replay can be bottlenecked in different ways to the real
 * application, and often has different realtime requirements for the actual
 * frame replay).
 * 
 * Events are harder because the GPU can wait on them. We need to be particularly
 * careful the GPU never waits on an event that will never become set, or the GPU
 * will lock up.
 * 
 * For now the implementation is simple, conservative and inefficient. We keep
 * events Set always, never replaying any Reset (CPU or GPU). This means any
 * wait will always succeed on the GPU.
 * 
 * On the CPU-side with GetEventStatus we do another hard sync with
 * DeviceWaitIdle.
 * 
 * On the GPU-side, whenever a command buffer contains a CmdWaitEvents we
 * create an event, reset it, and call CmdSetEvent right before the
 * CmdWaitEvents. This should provide the strictest possible ordering guarantee
 * for the CmdWaitEvents (since the event set it was waiting on must have
 * happened at or before where we are setting the event, so our event is as or
 * more conservative than the original event).
 * 
 * In future it would be nice to save the state of events at the start of
 * the frame and restore them, via GetEventStatus/SetEvent/ResetEvent. However
 * this will not be sufficient to make sure all events are set when they should
 * be - e.g. an event which is reset at start of frame, but a GPU cmd buffer is
 * in-flight that will set it, but hasn't been recorded as part of the frame.
 * Then a cmd buffer in the frame which does CmdWaitEvents will never have that
 * event set. I'm not sure if there's a way around this, we might just have to
 * make slight improvements to the current method by ensuring events are
 * properly hard-synced on the GPU.
 * 
 */

bool WrappedVulkan::Serialise_vkCreateFence(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkFenceCreateInfo*                    pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkFence*                                    pFence)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkFenceCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pFence));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkFence sem = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), &info, NULL, &sem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sem);
			GetResourceManager()->AddLiveResource(id, sem);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateFence(
			VkDevice                                device,
			const VkFenceCreateInfo*                pCreateInfo,
			const VkAllocationCallbacks*            pAllocator,
			VkFence*                                pFence)
{
	VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), pCreateInfo, pAllocator, pFence);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFence);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_FENCE);
				Serialise_vkCreateFence(localSerialiser, device, pCreateInfo, NULL, pFence);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pFence);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pFence);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkGetFenceStatus(
			Serialiser*                                 localSerialiser,
			VkDevice                                device,
			VkFence                                 fence)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, fid, GetResID(fence));
	
	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

		ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
	}

	return true;
}

VkResult WrappedVulkan::vkGetFenceStatus(
			VkDevice                                device,
			VkFence                                 fence)
{
	VkResult ret = ObjDisp(device)->GetFenceStatus(Unwrap(device), Unwrap(fence));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(GET_FENCE_STATUS);
		Serialise_vkGetFenceStatus(localSerialiser, device, fence);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkResetFences(
			Serialiser*                             localSerialiser,
			VkDevice                                device,
			uint32_t                                fenceCount,
			const VkFence*                          pFences)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(uint32_t, count, fenceCount);
	
	vector<VkFence> fences;

	for(uint32_t i=0; i < count; i++)
	{
		ResourceId id;
		if(m_State >= WRITING)
			id = GetResID(pFences[i]);

		localSerialiser->Serialise("pFences[]", id);

		if(m_State < WRITING)
			fences.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkFence>(id)));
	}

	if(m_State < WRITING)
	{
		// we don't care about fence states as we cannot record them perfectly and just
		// do full waitidle flushes.
	}

	return true;
}

VkResult WrappedVulkan::vkResetFences(
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences)
{
	VkFence *unwrapped = GetTempArray<VkFence>(fenceCount);
	for(uint32_t i=0; i < fenceCount; i++) unwrapped[i] = Unwrap(pFences[i]);
	VkResult ret = ObjDisp(device)->ResetFences(Unwrap(device), fenceCount, unwrapped);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(RESET_FENCE);
		Serialise_vkResetFences(localSerialiser, device, fenceCount, pFences);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkWaitForFences(
			Serialiser*                             localSerialiser,
			VkDevice                                device,
			uint32_t                                fenceCount,
			const VkFence*                          pFences,
			VkBool32                                waitAll,
			uint64_t                                timeout)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(VkBool32, wait, waitAll);
	SERIALISE_ELEMENT(uint64_t, tmout, timeout);
	SERIALISE_ELEMENT(uint32_t, count, fenceCount);
	
	vector<VkFence> fences;

	for(uint32_t i=0; i < count; i++)
	{
		ResourceId id;
		if(m_State >= WRITING)
			id = GetResID(pFences[i]);

		localSerialiser->Serialise("pFences[]", id);

		if(m_State < WRITING)
			fences.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkFence>(id)));
	}

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

		ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
	}

	return true;
}

VkResult WrappedVulkan::vkWaitForFences(
			VkDevice                                device,
			uint32_t                                fenceCount,
			const VkFence*                          pFences,
			VkBool32                                waitAll,
			uint64_t                                timeout)
{
	VkFence *unwrapped = GetTempArray<VkFence>(fenceCount);
	for (uint32_t i = 0; i < fenceCount; i++) unwrapped[i] = Unwrap(pFences[i]);
	VkResult ret = ObjDisp(device)->WaitForFences(Unwrap(device), fenceCount, unwrapped, waitAll, timeout);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(WAIT_FENCES);
		Serialise_vkWaitForFences(localSerialiser, device, fenceCount, pFences, waitAll, timeout);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateEvent(
			Serialiser*                             localSerialiser,
			VkDevice                                device,
			const VkEventCreateInfo*                pCreateInfo,
			const VkAllocationCallbacks*            pAllocator,
			VkEvent*                                pEvent)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkEventCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pEvent));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkEvent ev = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateEvent(Unwrap(device), &info, NULL, &ev);

		// see top of this file for current event/fence handling
		ObjDisp(device)->SetEvent(Unwrap(device), ev);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), ev);
			GetResourceManager()->AddLiveResource(id, ev);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateEvent(
			VkDevice                                device,
			const VkEventCreateInfo*                pCreateInfo,
			const VkAllocationCallbacks*            pAllocator,
			VkEvent*                                pEvent)
{
	VkResult ret = ObjDisp(device)->CreateEvent(Unwrap(device), pCreateInfo, pAllocator, pEvent);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pEvent);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_EVENT);
				Serialise_vkCreateEvent(localSerialiser, device, pCreateInfo, NULL, pEvent);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pEvent);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pEvent);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkSetEvent(
	Serialiser*                                 localSerialiser,
	VkDevice                                    device,
	VkEvent                                     event)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
	
	if(m_State < WRITING)
	{
		// see top of this file for current event/fence handling
	}

	return true;
}

VkResult WrappedVulkan::vkSetEvent(
	VkDevice                                    device,
	VkEvent                                     event)
{
	VkResult ret = ObjDisp(device)->SetEvent(Unwrap(device), Unwrap(event));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_EVENT);
		Serialise_vkSetEvent(localSerialiser, device, event);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkResetEvent(
	Serialiser*                                 localSerialiser,
	VkDevice                                    device,
	VkEvent                                     event)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
	
	if(m_State < WRITING)
	{
		// see top of this file for current event/fence handling
	}

	return true;
}

VkResult WrappedVulkan::vkResetEvent(
	VkDevice                                    device,
	VkEvent                                     event)
{
	VkResult ret = ObjDisp(device)->ResetEvent(Unwrap(device), Unwrap(event));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(RESET_EVENT);
		Serialise_vkResetEvent(localSerialiser, device, event);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkGetEventStatus(
			Serialiser*                             localSerialiser,
			VkDevice                                device,
			VkEvent                                 event)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
	
	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

		ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
	}

	return true;
}

VkResult WrappedVulkan::vkGetEventStatus(
			VkDevice                                device,
			VkEvent                                 event)
{
	VkResult ret = ObjDisp(device)->GetEventStatus(Unwrap(device), Unwrap(event));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(GET_EVENT_STATUS);
		Serialise_vkGetEventStatus(localSerialiser, device, event);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateSemaphore(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSemaphore*                                pSemaphore)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSemaphoreCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSemaphore));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkSemaphore sem = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), &info, NULL, &sem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sem);
			GetResourceManager()->AddLiveResource(id, sem);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSemaphore*                                pSemaphore)
{
	VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), pCreateInfo, pAllocator, pSemaphore);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSemaphore);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_SEMAPHORE);
				Serialise_vkCreateSemaphore(localSerialiser, device, pCreateInfo, NULL, pSemaphore);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSemaphore);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSemaphore);
		}
	}

	return ret;
}


bool WrappedVulkan::Serialise_vkCmdSetEvent(
		Serialiser*                                 localSerialiser,
		VkCommandBuffer                                 cmdBuffer,
    VkEvent                                     event,
		VkPipelineStageFlags                        stageMask)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
	SERIALISE_ELEMENT(VkPipelineStageFlagBits, mask, (VkPipelineStageFlagBits)stageMask);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	// see top of this file for current event/fence handling
	
	if(m_State == EXECUTING)
	{
		event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);
		
		if(ShouldRerecordCmd(cmdid) && InRerecordRange())
		{
			cmdBuffer = RerecordCmdBuf(cmdid);
			ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);
		
		ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
	}

	return true;
}

void WrappedVulkan::vkCmdSetEvent(
    VkCommandBuffer                                 cmdBuffer,
    VkEvent                                     event,
		VkPipelineStageFlags                        stageMask)
{
	ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), Unwrap(event), stageMask);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CMD_SET_EVENT);
		Serialise_vkCmdSetEvent(localSerialiser, cmdBuffer, event, stageMask);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdResetEvent(
		Serialiser*                                 localSerialiser,
		VkCommandBuffer                                 cmdBuffer,
    VkEvent                                     event,
		VkPipelineStageFlags                        stageMask)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
	SERIALISE_ELEMENT(VkPipelineStageFlagBits, mask, (VkPipelineStageFlagBits)stageMask);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	// see top of this file for current event/fence handling

	if(m_State == EXECUTING)
	{
		event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);

		if(ShouldRerecordCmd(cmdid) && InRerecordRange())
		{
			cmdBuffer = RerecordCmdBuf(cmdid);
			//ObjDisp(cmdBuffer)->CmdResetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);
		
		//ObjDisp(cmdBuffer)->CmdResetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
	}

	return true;
}

void WrappedVulkan::vkCmdResetEvent(
    VkCommandBuffer                                 cmdBuffer,
    VkEvent                                     event,
		VkPipelineStageFlags                        stageMask)
{
	ObjDisp(cmdBuffer)->CmdResetEvent(Unwrap(cmdBuffer), Unwrap(event), stageMask);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CMD_RESET_EVENT);
		Serialise_vkCmdResetEvent(localSerialiser, cmdBuffer, event, stageMask);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdWaitEvents(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 cmdBuffer,
			uint32_t                                    eventCount,
			const VkEvent*                              pEvents,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkPipelineStageFlags, src, srcStageMask);
	SERIALISE_ELEMENT(VkPipelineStageFlags, dest, destStageMask);
	
	// we don't serialise the original events as we are going to replace this
	// with our own

	// we keep the original memory barriers
	SERIALISE_ELEMENT(uint32_t, memCount, memBarrierCount);

	vector<VkGenericStruct*> mems;
	vector<VkImageMemoryBarrier> imBarriers;

	for(uint32_t i=0; i < memCount; i++)
	{
		SERIALISE_ELEMENT(VkStructureType, stype, ((VkGenericStruct *)ppMemBarriers[i])->sType);

		if(stype == VK_STRUCTURE_TYPE_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkMemoryBarrier, barrier, *((VkMemoryBarrier *)ppMemBarriers[i]));

			if(m_State < WRITING)
				mems.push_back((VkGenericStruct *)new VkMemoryBarrier(barrier));
		}
		else if(stype == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkBufferMemoryBarrier, barrier, *((VkBufferMemoryBarrier *)ppMemBarriers[i]));

			// it's possible for buffer to be NULL if it refers to a buffer that is otherwise
			// not in the log (barriers do not mark resources referenced). If the buffer does
			// not exist, then it's safe to skip this barrier.
			if(m_State < WRITING && barrier.buffer != VK_NULL_HANDLE)
				mems.push_back((VkGenericStruct *)new VkBufferMemoryBarrier(barrier));
		}
		else if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkImageMemoryBarrier, barrier, *((VkImageMemoryBarrier *)ppMemBarriers[i]));

			// same as buffers above, allow images to not exist and skip their barriers.
			if(m_State < WRITING && barrier.image != VK_NULL_HANDLE)
			{
				mems.push_back((VkGenericStruct *)new VkImageMemoryBarrier(barrier));
				imBarriers.push_back(barrier);
			}
		}
	}

	// see top of this file for current event/fence handling

	if(m_State == EXECUTING)
	{
		if(ShouldRerecordCmd(cmdid) && InRerecordRange())
		{
			cmdBuffer = RerecordCmdBuf(cmdid);

			VkEventCreateInfo evInfo = {
				VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, NULL, 0,
			};

			VkEvent ev = VK_NULL_HANDLE;
			ObjDisp(cmdBuffer)->CreateEvent(Unwrap(GetDev()), &evInfo, NULL, &ev);
			// don't wrap this event

			ObjDisp(cmdBuffer)->ResetEvent(Unwrap(GetDev()), ev);
			ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), ev, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

			ObjDisp(cmdBuffer)->CmdWaitEvents(Unwrap(cmdBuffer), 1, &ev, src, dest, (uint32_t)mems.size(), (const void **)&mems[0]);

			// register to clean this event up once we're done replaying this section of the log
			m_CleanupEvents.push_back(ev);

			ResourceId cmd = GetResID(RerecordCmdBuf(cmdid));
			GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts, (uint32_t)imBarriers.size(), &imBarriers[0]);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

		VkEventCreateInfo evInfo = {
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, NULL, 0,
		};

		VkEvent ev = VK_NULL_HANDLE;
		ObjDisp(cmdBuffer)->CreateEvent(Unwrap(GetDev()), &evInfo, NULL, &ev);
		// don't wrap this event

		ObjDisp(cmdBuffer)->ResetEvent(Unwrap(GetDev()), ev);
		ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), ev, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

		ObjDisp(cmdBuffer)->CmdWaitEvents(Unwrap(cmdBuffer), 1, &ev, src, dest, (uint32_t)mems.size(), (const void **)&mems[0]);

		// register to clean this event up once we're done replaying this section of the log
		m_CleanupEvents.push_back(ev);
		
		ResourceId cmd = GetResID(cmdBuffer);
		GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts, (uint32_t)imBarriers.size(), &imBarriers[0]);
	}

	for(size_t i=0; i < mems.size(); i++)
		delete mems[i];

	return true;
}

void WrappedVulkan::vkCmdWaitEvents(
			VkCommandBuffer                                 cmdBuffer,
			uint32_t                                    eventCount,
			const VkEvent*                              pEvents,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers)
{
	{
		// conservatively request memory for worst case to avoid needing to iterate
		// twice to count
		byte *memory = GetTempMemory( sizeof(VkEvent)*eventCount + ( sizeof(void*) + sizeof(VkImageMemoryBarrier) + sizeof(VkBufferMemoryBarrier) )*memBarrierCount);

		VkEvent *ev = (VkEvent *)memory;
		VkImageMemoryBarrier *im = (VkImageMemoryBarrier *)(ev + eventCount);
		VkBufferMemoryBarrier *buf = (VkBufferMemoryBarrier *)(im + memBarrierCount);

		for(uint32_t i=0; i < eventCount; i++)
			ev[i] = Unwrap(pEvents[i]);

		size_t imCount = 0, bufCount = 0;
		
		void **unwrappedBarriers = (void **)(buf + memBarrierCount);

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkGenericStruct *header = (VkGenericStruct *)ppMemBarriers[i];

			if(header->sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
			{
				VkImageMemoryBarrier &barrier = im[imCount];
				barrier = *(VkImageMemoryBarrier *)header;
				barrier.image = Unwrap(barrier.image);
				unwrappedBarriers[i] = &im[imCount];

				imCount++;
			}
			else if(header->sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
			{
				VkBufferMemoryBarrier &barrier = buf[bufCount];
				barrier = *(VkBufferMemoryBarrier *)header;
				barrier.buffer = Unwrap(barrier.buffer);
				unwrappedBarriers[i] = &buf[bufCount];

				bufCount++;
			}
			else
			{
				unwrappedBarriers[i] = (void *)ppMemBarriers[i];
			}
		}
		
		ObjDisp(cmdBuffer)->CmdWaitEvents(Unwrap(cmdBuffer), eventCount, ev, srcStageMask, destStageMask, memBarrierCount, unwrappedBarriers);
	}

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CMD_WAIT_EVENTS);
		Serialise_vkCmdWaitEvents(localSerialiser, cmdBuffer, eventCount, pEvents, srcStageMask, destStageMask, memBarrierCount, ppMemBarriers);
		
		vector<VkImageMemoryBarrier> imBarriers;

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkStructureType stype = ((VkGenericStruct *)ppMemBarriers[i])->sType;

			if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
				imBarriers.push_back(*((VkImageMemoryBarrier *)ppMemBarriers[i]));
		}
		
		ResourceId cmd = GetResID(cmdBuffer);
		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			GetResourceManager()->RecordBarriers(GetRecord(cmdBuffer)->cmdInfo->imgbarriers, m_ImageLayouts, (uint32_t)imBarriers.size(), &imBarriers[0]);
		}

		record->AddChunk(scope.Get());
		for(uint32_t i=0; i < eventCount; i++)
			record->MarkResourceFrameReferenced(GetResID(pEvents[i]), eFrameRef_Read);
	}
}
