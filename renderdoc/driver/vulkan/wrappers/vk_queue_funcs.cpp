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
		Serialiser*                                 localSerialiser,
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(uint32_t, familyIdx, queueFamilyIndex);
	SERIALISE_ELEMENT(uint32_t, idx, queueIndex);
	SERIALISE_ELEMENT(ResourceId, queueId, GetResID(*pQueue));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkQueue queue;
		VkResult ret = ObjDisp(device)->GetDeviceQueue(Unwrap(device), familyIdx, idx, &queue);

		VkQueue real = queue;

		GetResourceManager()->WrapResource(Unwrap(device), queue);
		GetResourceManager()->AddLiveResource(queueId, queue);

		if(familyIdx == m_QueueFamilyIdx)
		{
			m_Queue = queue;
			
			// we can now submit any cmds that were queued (e.g. from creating debug
			// manager on vkCreateDevice)
			SubmitCmds();
		}
	}

	return true;
}

VkResult WrappedVulkan::vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueFamilyIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	VkResult ret = ObjDisp(device)->GetDeviceQueue(Unwrap(device), queueFamilyIndex, queueIndex, pQueue);

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
					CACHE_THREAD_SERIALISER();

					SCOPED_SERIALISE_CONTEXT(GET_DEVICE_QUEUE);
					Serialise_vkGetDeviceQueue(localSerialiser, device, queueFamilyIndex, queueIndex, pQueue);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pQueue);
				RDCASSERT(record);

				VkResourceRecord *instrecord = GetRecord(m_Instance);

				// treat queues as pool members of the instance (ie. freed when the instance dies)
				{
					instrecord->LockChunks();
					instrecord->pooledChildren.push_back(record);
					instrecord->UnlockChunks();
				}

				record->AddChunk(chunk);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, *pQueue);
			}

			if(queueFamilyIndex == m_QueueFamilyIdx)
			{
				m_Queue = *pQueue;

				// we can now submit any cmds that were queued (e.g. from creating debug
				// manager on vkCreateDevice)
				SubmitCmds();
			}
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueSubmit(
		Serialiser*                                 localSerialiser,
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

	const string desc = localSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		ObjDisp(queue)->QueueSubmit(Unwrap(queue), numCmds, cmds, Unwrap(fence));

		for(uint32_t i=0; i < numCmds; i++)
		{
			ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
			GetResourceManager()->ApplyTransitions(m_BakedCmdBufferInfo[cmd].imgtransitions, m_ImageLayouts);
		}

		AddEvent(QUEUE_SUBMIT, desc);
		string name = "vkQueueSubmit(" +
						ToStr::Get(numCmds) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, true);

		// add command buffer draws under here
		GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());

		m_RootEventID++;

		for(uint32_t c=0; c < numCmds; c++)
		{
			string name = "[" + ToStr::Get(cmdIds[c]) + "]";

			AddEvent(QUEUE_SUBMIT, "cmd " + name);

			FetchDrawcall draw;
			draw.name = name;

			draw.flags |= eDraw_PushMarker;

			AddDrawcall(draw, true);

			DrawcallTreeNode &d = GetDrawcallStack().back()->children.back();

			// copy DrawcallTreeNode children
			d.children = m_BakedCmdBufferInfo[cmdIds[c]].draw->children;

			// assign new event and drawIDs
			RefreshIDs(d.children, m_RootEventID, m_RootDrawcallID);

			m_PartialReplayData.cmdBufferSubmits[cmdIds[c]].push_back(m_RootEventID);

			// 1 extra for the [0] virtual event for the command buffer
			m_RootEventID += 1+m_BakedCmdBufferInfo[cmdIds[c]].eventCount;
			m_RootDrawcallID += m_BakedCmdBufferInfo[cmdIds[c]].drawCount;
		}

		// the outer loop will increment the event ID but we've handled
		// it ourselves, so 'undo' that.
		m_RootEventID--;

		// done adding command buffers
		m_DrawcallStack.pop_back();
	}
	else if(m_State == EXECUTING)
	{
		m_RootEventID++;

		uint32_t startEID = m_RootEventID;

		// advance m_CurEventID to match the events added when reading
		for(uint32_t c=0; c < numCmds; c++)
		{
			// 1 extra for the [0] virtual event for the command buffer
			m_RootEventID += 1+m_BakedCmdBufferInfo[cmdIds[c]].eventCount;
			m_RootDrawcallID += m_BakedCmdBufferInfo[cmdIds[c]].drawCount;
		}

		m_RootEventID--;

		if(m_LastEventID == startEID)
		{
			RDCDEBUG("Queue Submit no replay %u == %u", m_LastEventID, startEID);
		}
		else if(m_LastEventID > startEID && m_LastEventID < m_RootEventID)
		{
			RDCDEBUG("Queue Submit partial replay %u < %u", m_LastEventID, m_RootEventID);

			uint32_t eid = startEID;

			vector<ResourceId> trimmedCmdIds;
			vector<VkCmdBuffer> trimmedCmds;

			for(uint32_t c=0; c < numCmds; c++)
			{
				uint32_t end = eid + m_BakedCmdBufferInfo[cmdIds[c]].eventCount;

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

				eid += 1+m_BakedCmdBufferInfo[cmdIds[c]].eventCount;
			}

			RDCASSERT(trimmedCmds.size() > 0);

			ObjDisp(queue)->QueueSubmit(Unwrap(queue), (uint32_t)trimmedCmds.size(), &trimmedCmds[0], Unwrap(fence));

			for(uint32_t i=0; i < trimmedCmdIds.size(); i++)
			{
				ResourceId cmd = trimmedCmdIds[i];
				GetResourceManager()->ApplyTransitions(m_BakedCmdBufferInfo[cmd].imgtransitions, m_ImageLayouts);
			}
		}
		else
		{
			ObjDisp(queue)->QueueSubmit(Unwrap(queue), numCmds, cmds, Unwrap(fence));

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
				GetResourceManager()->ApplyTransitions(m_BakedCmdBufferInfo[cmd].imgtransitions, m_ImageLayouts);
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
	VkCmdBuffer *unwrapped = GetTempArray<VkCmdBuffer>(cmdBufferCount);
	for(uint32_t i=0; i < cmdBufferCount; i++) unwrapped[i] = Unwrap(pCmdBuffers[i]);

	VkResult ret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), cmdBufferCount, unwrapped, Unwrap(fence));

	bool capframe = false;
	set<ResourceId> refdIDs;

	for(uint32_t i=0; i < cmdBufferCount; i++)
	{
		ResourceId cmd = GetResID(pCmdBuffers[i]);

		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			GetResourceManager()->ApplyTransitions(m_BakedCmdBufferInfo[cmd].imgtransitions, m_ImageLayouts);
		}

		VkResourceRecord *record = GetRecord(pCmdBuffers[i]);

		// need to lock the whole section of code, not just the check on
		// m_State, as we also need to make sure we don't check the state,
		// start marking dirty resources then while we're doing so the
		// state becomes capframe.
		// the next sections where we mark resources referenced and add
		// the submit chunk to the frame record don't have to be protected.
		// Only the decision of whether we're inframe or not, and marking
		// dirty.
		{
			SCOPED_LOCK(m_CapTransitionLock);
			if(m_State == WRITING_CAPFRAME)
			{
				for(auto it = record->bakedCommands->cmdInfo->dirtied.begin(); it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
					GetResourceManager()->MarkPendingDirty(*it);

				capframe = true;
			}
			else
			{
				for(auto it = record->bakedCommands->cmdInfo->dirtied.begin(); it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
					GetResourceManager()->MarkDirtyResource(*it);
			}
		}

		if(capframe)
		{
			// for each bound descriptor set, mark it referenced as well as all resources currently bound to it
			for(auto it = record->bakedCommands->cmdInfo->boundDescSets.begin(); it != record->bakedCommands->cmdInfo->boundDescSets.end(); ++it)
			{
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(*it), eFrameRef_Read);

				VkResourceRecord *setrecord = GetRecord(*it);

				for(auto refit = setrecord->bindFrameRefs.begin(); refit != setrecord->bindFrameRefs.end(); ++refit)
				{
					refdIDs.insert(refit->first);
					GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second.second);
				}
			}

			// pull in frame refs from this baked command buffer
			record->bakedCommands->AddResourceReferences(GetResourceManager());
			record->bakedCommands->AddReferencedIDs(refdIDs);

			// ref the parent command buffer by itself, this will pull in the cmd buffer pool
			GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);
			
			GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);

			if(fence != VK_NULL_HANDLE)
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);

			{
				SCOPED_LOCK(m_CmdBufferRecordsLock);
				m_CmdBufferRecords.push_back(record->bakedCommands);
			}

			record->bakedCommands->AddRef();
		}

		record->cmdInfo->dirtied.clear();
	}
	
	if(capframe)
	{
		vector<VkResourceRecord*> maps;
		{
			SCOPED_LOCK(m_CoherentMapsLock);
			maps = m_CoherentMaps;
		}
		
		for(auto it = maps.begin(); it != maps.end(); ++it)
		{
			VkResourceRecord *record = *it;
			MemMapState &state = *record->memMapState;

			// potential persistent map
			if(state.mapCoherent && state.mappedPtr && !state.mapFlushed)
			{
				// only need to flush memory that could affect this submitted batch of work
				if(refdIDs.find(record->GetResourceID()) == refdIDs.end())
				{
					RDCDEBUG("Map of memory %llu not referenced in this queue - not flushing", record->GetResourceID());
					continue;
				}

				size_t diffStart = 0, diffEnd = 0;
				bool found = true;

				// if we have a previous set of data, compare.
				// otherwise just serialise it all
				if(state.refData)
					found = FindDiffRange((byte *)state.mappedPtr, state.refData, (size_t)state.mapSize, diffStart, diffEnd);
				else
					diffEnd = (size_t)state.mapSize;

				if(found)
				{
					// VKTODOLOW won't work with multiple devices - maybe find device for the specified queue?
					// we probably only want to flush maps associated with this queue anyway
					VkDevice dev = GetDev();

					{
						RDCLOG("Persistent map flush forced for %llu (%llu -> %llu)", record->GetResourceID(), (uint64_t)diffStart, (uint64_t)diffEnd);
						VkMappedMemoryRange range = {
							VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL,
							(VkDeviceMemory)(uint64_t)record->Resource,
							state.mapOffset+diffStart, diffEnd-diffStart
						};
						vkFlushMappedMemoryRanges(dev, 1, &range);
						state.mapFlushed = false;
					}

					GetResourceManager()->MarkPendingDirty(record->GetResourceID());

					// allocate ref data so we can compare next time to minimise serialised data
					if(state.refData == NULL)
						state.refData = Serialiser::AllocAlignedBuffer((size_t)state.mapSize, 64);
					memcpy(state.refData, state.mappedPtr, (size_t)state.mapSize);
				}
				else
				{
					RDCDEBUG("Persistent map flush not needed for %llu", record->GetResourceID());
				}
			}
		}

		{
			CACHE_THREAD_SERIALISER();

			SCOPED_SERIALISE_CONTEXT(QUEUE_SUBMIT);
			Serialise_vkQueueSubmit(localSerialiser, queue, cmdBufferCount, pCmdBuffers, fence);

			m_FrameCaptureRecord->AddChunk(scope.Get());
		}
	}
	
	return ret;
}

bool WrappedVulkan::Serialise_vkQueueSignalSemaphore(Serialiser* localSerialiser, VkQueue queue, VkSemaphore semaphore)
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
		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(QUEUE_SIGNAL_SEMAPHORE);
		Serialise_vkQueueSignalSemaphore(localSerialiser, queue, semaphore);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitSemaphore(Serialiser* localSerialiser, VkQueue queue, VkSemaphore semaphore)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, sid, GetResID(semaphore));
	
	if(m_State < WRITING)
	{
		// we don't track semaphore state so we don't know whether this semaphore was signalled
		// or unsignalled. To be conservative, we wait for idle.
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = ObjDisp(queue)->QueueWaitSemaphore(Unwrap(queue), Unwrap(semaphore));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_SEMAPHORE);
		Serialise_vkQueueWaitSemaphore(localSerialiser, queue, semaphore);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitIdle(Serialiser* localSerialiser, VkQueue queue)
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
		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_IDLE);
		Serialise_vkQueueWaitIdle(localSerialiser, queue);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
	}

	return ret;
}
