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

bool WrappedVulkan::Serialise_vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(uint32_t, nodeIdx, queueNodeIndex);
	SERIALISE_ELEMENT(uint32_t, idx, queueIndex);
	SERIALISE_ELEMENT(ResourceId, queueId, GetResID(*pQueue));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkQueue queue;
		VkResult ret = ObjDisp(device)->GetDeviceQueue(Unwrap(device), nodeIdx, idx, &queue);

		VkQueue real = queue;

		GetResourceManager()->WrapResource(Unwrap(device), queue);
		GetResourceManager()->AddLiveResource(queueId, queue);
		
		// VKTODOMED hack - fixup unwrapped queue objects, because we tried to fill them
		// out early
		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
			if(m_PhysicalReplayData[i].q == real) m_PhysicalReplayData[i].q = queue;
	}

	return true;
}

VkResult WrappedVulkan::vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	VkResult ret = ObjDisp(device)->GetDeviceQueue(Unwrap(device), queueNodeIndex, queueIndex, pQueue);

	if(ret == VK_SUCCESS)
	{
		// it's perfectly valid for enumerate type functions to return the same handle
		// each time. If that happens, we will already have a wrapper created so just
		// return the wrapped object to the user and do nothing else
		if(GetResourceManager()->HasWrapper(ToTypedHandle(*pQueue)))
		{
			*pQueue = (VkQueue)GetResourceManager()->GetWrapper(ToTypedHandle(*pQueue));
		}
		else
		{
			ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueue);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(GET_DEVICE_QUEUE);
					Serialise_vkGetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pQueue);
				RDCASSERT(record);

				m_InstanceRecord->queues.push_back(*pQueue);

				record->AddChunk(chunk);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, *pQueue);
			}
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    cmdBufferCount,
    const VkCmdBuffer*                          pCmdBuffers,
    VkFence                                     fence)
{
	SERIALISE_ELEMENT(ResourceId, queueId, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, fenceId, fence != VK_NULL_HANDLE ? GetResID(fence) : ResourceId());
	
	SERIALISE_ELEMENT(uint32_t, numCmds, cmdBufferCount);

	vector<ResourceId> cmdIds;
	VkCmdBuffer *cmds = m_State >= WRITING ? NULL : new VkCmdBuffer[numCmds];
	for(uint32_t i=0; i < numCmds; i++)
	{
		ResourceId bakedId;

		if(m_State >= WRITING)
		{
			VkResourceRecord *record = GetRecord(pCmdBuffers[i]);
			RDCASSERT(record->bakedCommands);
			if(record->bakedCommands)
				bakedId = record->bakedCommands->GetResourceID();
		}

		SERIALISE_ELEMENT(ResourceId, id, bakedId);

		if(m_State < WRITING)
		{
			cmdIds.push_back(id);

			cmds[i] = id != ResourceId()
			          ? Unwrap(GetResourceManager()->GetLiveHandle<VkCmdBuffer>(id))
			          : NULL;
		}
	}
	
	if(m_State < WRITING)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(queueId);
		if(fenceId != ResourceId())
			fence = GetResourceManager()->GetLiveHandle<VkFence>(fenceId);
		else
			fence = VK_NULL_HANDLE;
	}

	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		m_SubmittedFences.insert(fenceId);

		ObjDisp(queue)->QueueSubmit(Unwrap(queue), numCmds, cmds, Unwrap(fence));

		for(uint32_t i=0; i < numCmds; i++)
		{
			ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
			GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
		}

		AddEvent(QUEUE_SUBMIT, desc);
		string name = "vkQueueSubmit(" +
						ToStr::Get(numCmds) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, true);

		// add command buffer draws under here
		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		for(uint32_t c=0; c < numCmds; c++)
		{
			AddEvent(QUEUE_SUBMIT, "");
			string name = "[" + ToStr::Get(cmdIds[c]) + "]";

			FetchDrawcall draw;
			draw.name = name;

			draw.flags |= eDraw_PushMarker;

			AddDrawcall(draw, true);

			DrawcallTreeNode &d = m_DrawcallStack.back()->children.back();

			// copy DrawcallTreeNode children
			d.children = m_CmdBufferInfo[cmdIds[c]].draw->children;

			// assign new event and drawIDs
			RefreshIDs(d.children, m_CurEventID, m_CurDrawcallID);

			m_PartialReplayData.cmdBufferSubmits[cmdIds[c]].push_back(m_CurEventID);

			// 1 extra for the [0] virtual event for the command buffer
			m_CurEventID += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			m_CurDrawcallID += m_CmdBufferInfo[cmdIds[c]].drawCount;
		}

		// the outer loop will increment the event ID but we've handled
		// it ourselves, so 'undo' that.
		m_CurEventID--;

		// done adding command buffers
		m_DrawcallStack.pop_back();
	}
	else if(m_State == EXECUTING)
	{
		m_CurEventID++;

		uint32_t startEID = m_CurEventID;

		// advance m_CurEventID to match the events added when reading
		for(uint32_t c=0; c < numCmds; c++)
		{
			// 1 extra for the [0] virtual event for the command buffer
			m_CurEventID += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			m_CurDrawcallID += m_CmdBufferInfo[cmdIds[c]].drawCount;
		}

		m_CurEventID--;

		if(m_LastEventID < m_CurEventID)
		{
			RDCDEBUG("Queue Submit partial replay %u < %u", m_LastEventID, m_CurEventID);

			uint32_t eid = startEID;

			vector<ResourceId> trimmedCmdIds;
			vector<VkCmdBuffer> trimmedCmds;

			for(uint32_t c=0; c < numCmds; c++)
			{
				uint32_t end = eid + m_CmdBufferInfo[cmdIds[c]].eventCount;

				if(eid == m_PartialReplayData.baseEvent)
				{
					ResourceId partial = GetResID(PartialCmdBuf());
					RDCDEBUG("Queue Submit partial replay of %llu at %u, using %llu", cmdIds[c], eid, partial);
					trimmedCmdIds.push_back(partial);
					trimmedCmds.push_back(Unwrap(PartialCmdBuf()));
				}
				else if(m_LastEventID >= end)
				{
					RDCDEBUG("Queue Submit full replay %llu", cmdIds[c]);
					trimmedCmdIds.push_back(cmdIds[c]);
					trimmedCmds.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdIds[c])));
				}
				else
				{
					RDCDEBUG("Queue not submitting %llu", cmdIds[c]);
				}

				eid += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			}

			RDCASSERT(trimmedCmds.size() > 0);

			m_SubmittedFences.insert(fenceId);

			ObjDisp(queue)->QueueSubmit(Unwrap(queue), (uint32_t)trimmedCmds.size(), &trimmedCmds[0], Unwrap(fence));

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = trimmedCmdIds[i];
				GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
			}
		}
		else
		{
			m_SubmittedFences.insert(fenceId);

			ObjDisp(queue)->QueueSubmit(Unwrap(queue), numCmds, cmds, Unwrap(fence));

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
				GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
			}
		}
	}

	SAFE_DELETE_ARRAY(cmds);

	return true;
}

void WrappedVulkan::RefreshIDs(vector<DrawcallTreeNode> &nodes, uint32_t baseEventID, uint32_t baseDrawID)
{
	// assign new drawcall IDs
	for(size_t i=0; i < nodes.size(); i++)
	{
		nodes[i].draw.eventID += baseEventID;
		nodes[i].draw.drawcallID += baseDrawID;

		for(int32_t e=0; e < nodes[i].draw.events.count; e++)
		{
			nodes[i].draw.events[e].eventID += baseEventID;
			m_Events.push_back(nodes[i].draw.events[e]);
		}

		RefreshIDs(nodes[i].children, baseEventID, baseDrawID);
	}
}

VkResult WrappedVulkan::vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    cmdBufferCount,
    const VkCmdBuffer*                          pCmdBuffers,
    VkFence                                     fence)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkCmdBuffer *unwrapped = new VkCmdBuffer[cmdBufferCount];
	for(uint32_t i=0; i < cmdBufferCount; i++)
		unwrapped[i] = Unwrap(pCmdBuffers[i]);

	VkResult ret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), cmdBufferCount, unwrapped, Unwrap(fence));

	SAFE_DELETE_ARRAY(unwrapped);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_SUBMIT);
		Serialise_vkQueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	for(uint32_t i=0; i < cmdBufferCount; i++)
	{
		ResourceId cmd = GetResID(pCmdBuffers[i]);
		GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);

		VkResourceRecord *record = GetRecord(pCmdBuffers[i]);
		for(auto it = record->bakedCommands->dirtied.begin(); it != record->bakedCommands->dirtied.end(); ++it)
			GetResourceManager()->MarkDirtyResource(*it);

		if(m_State == WRITING_CAPFRAME)
		{
			// for each bound descriptor set, mark it referenced as well as all resources currently bound to it
			for(auto it = record->bakedCommands->boundDescSets.begin(); it != record->bakedCommands->boundDescSets.end(); ++it)
			{
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(*it), eFrameRef_Read);

				VkResourceRecord *setrecord = GetRecord(*it);

				for(auto refit = setrecord->bindFrameRefs.begin(); refit != setrecord->bindFrameRefs.end(); ++refit)
					GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second.second);
			}

			// pull in frame refs from this baked command buffer
			record->bakedCommands->AddResourceReferences(GetResourceManager());

			// ref the parent command buffer by itself, this will pull in the cmd buffer pool
			GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

			m_CmdBufferRecords.push_back(record->bakedCommands);
			record->bakedCommands->AddRef();
		}

		record->dirtied.clear();
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, sid, GetResID(semaphore));
	
	if(m_State < WRITING)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		ObjDisp(queue)->QueueSignalSemaphore(Unwrap(queue), Unwrap(GetResourceManager()->GetLiveHandle<VkSemaphore>(sid)));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = ObjDisp(queue)->QueueSignalSemaphore(Unwrap(queue), Unwrap(semaphore));
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_SIGNAL_SEMAPHORE);
		Serialise_vkQueueSignalSemaphore(queue, semaphore);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, sid, GetResID(semaphore));
	
	if(m_State < WRITING)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		ObjDisp(queue)->QueueWaitSemaphore(Unwrap(queue), Unwrap(GetResourceManager()->GetLiveHandle<VkSemaphore>(sid)));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = ObjDisp(queue)->QueueWaitSemaphore(Unwrap(queue), Unwrap(semaphore));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_SEMAPHORE);
		Serialise_vkQueueWaitSemaphore(queue, semaphore);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitIdle(VkQueue queue)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(queue));
	
	if(m_State < WRITING_CAPFRAME)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(id);
		ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitIdle(VkQueue queue)
{
	VkResult ret = ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_IDLE);
		Serialise_vkQueueWaitIdle(queue);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
	}

	return ret;
}
