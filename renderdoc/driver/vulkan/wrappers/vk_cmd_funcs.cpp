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

// Command pool functions

bool WrappedVulkan::Serialise_vkCreateCommandPool(
			VkDevice                                    device,
			const VkCmdPoolCreateInfo*                  pCreateInfo,
			VkCmdPool*                                  pCmdPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkCmdPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pCmdPool));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkCmdPool pool = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), &info, &pool);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
			GetResourceManager()->AddLiveResource(id, pool);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateCommandPool(
			VkDevice                                    device,
			const VkCmdPoolCreateInfo*                  pCreateInfo,
			VkCmdPool*                                  pCmdPool)
{
	VkResult ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), pCreateInfo, pCmdPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pCmdPool);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_CMD_POOL);
				Serialise_vkCreateCommandPool(device, pCreateInfo, pCmdPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pCmdPool);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pCmdPool);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkResetCommandPool(
			VkDevice                                    device,
			VkCmdPool                                   cmdPool,
			VkCmdPoolResetFlags                         flags)
{
	// VKTODOMED do I need to serialise this? just a driver hint..
	return ObjDisp(device)->ResetCommandPool(device, cmdPool, flags);
}


// Command buffer functions

VkResult WrappedVulkan::vkCreateCommandBuffer(
	VkDevice                        device,
	const VkCmdBufferCreateInfo* pCreateInfo,
	VkCmdBuffer*                   pCmdBuffer)
{
	VkCmdBufferCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.cmdPool = Unwrap(unwrappedInfo.cmdPool);
	VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &unwrappedInfo, pCmdBuffer);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pCmdBuffer);
		
		if(m_State >= WRITING)
		{
			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pCmdBuffer);

			record->bakedCommands = NULL;

			record->AddParent(GetRecord(pCreateInfo->cmdPool));

			// we don't serialise this as we never create this command buffer directly.
			// Instead we create a command buffer for each baked list that we find.
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pCmdBuffer);
		}

		m_CmdBufferInfo[id].device = device;
		m_CmdBufferInfo[id].createInfo = *pCreateInfo;
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkBeginCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));

	ResourceId bakedCmdId;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
	}

	SERIALISE_ELEMENT(VkCmdBufferBeginInfo, info, *pBeginInfo);
	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);
	
	VkCmdBufferCreateInfo createInfo;
	VkDevice device = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		device = m_CmdBufferInfo[cmdId].device;
		createInfo = m_CmdBufferInfo[cmdId].createInfo;
	}

	if(m_State < WRITING)
	{
		m_LastCmdBufferID = cmdId;
		m_CmdBuffersInProgress++;
	}
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	m_pSerialiser->Serialise("createInfo", createInfo);

	if(m_State < WRITING)
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

	if(m_State == EXECUTING)
	{
		const vector<uint32_t> &baseEvents = m_PartialReplayData.cmdBufferSubmits[bakeId];
		uint32_t length = m_CmdBufferInfo[bakeId].eventCount;

		for(auto it=baseEvents.begin(); it != baseEvents.end(); ++it)
		{
			if(*it < m_LastEventID && m_LastEventID < (*it + length))
			{
				RDCDEBUG("vkBegin - partial detected %u < %u < %u, %llu -> %llu", *it, m_LastEventID, *it + length, cmdId, bakeId);

				m_PartialReplayData.partialParent = cmdId;
				m_PartialReplayData.baseEvent = *it;
				m_PartialReplayData.renderPassActive = false;

				VkCmdBuffer cmd = VK_NULL_HANDLE;
				VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &createInfo, &cmd);

				if(ret != VK_SUCCESS)
				{
					RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
				}
				else
				{
					GetResourceManager()->WrapResource(Unwrap(device), cmd);
				}

				m_PartialReplayData.resultPartialCmdBuffer = cmd;
				m_PartialReplayData.partialDevice = device;

				// add one-time submit flag as this partial cmd buffer will only be submitted once
				info.flags |= VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT;

				ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &info);
			}
		}

		m_CmdBufferInfo[cmdId].curEventID = 0;
	}
	else if(m_State == READING)
	{
		// remove one-time submit flag as we will want to submit many
		info.flags &= ~VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT;

		VkCmdBuffer cmd = VK_NULL_HANDLE;

		if(!GetResourceManager()->HasLiveResource(bakeId))
		{
			VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &createInfo, &cmd);

			if(ret != VK_SUCCESS)
			{
				RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
			}
			else
			{
				ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
				GetResourceManager()->AddLiveResource(bakeId, cmd);
			}

			// whenever a vkCmd command-building chunk asks for the command buffer, it
			// will get our baked version.
			GetResourceManager()->ReplaceResource(cmdId, bakeId);
		}
		else
		{
			cmd = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);
		}

		{
			DrawcallTreeNode *draw = new DrawcallTreeNode;
			m_CmdBufferInfo[cmdId].draw = draw;
			
			// On queue submit we increment all child events/drawcalls by
			// m_CurEventID insert them into the tree.
			m_CmdBufferInfo[cmdId].curEventID = 0;
			m_CmdBufferInfo[cmdId].eventCount = 0;
			m_CmdBufferInfo[cmdId].drawCount = 0;

			m_CmdBufferInfo[cmdId].drawStack.push_back(draw);
		}

		ObjDisp(device)->BeginCommandBuffer(Unwrap(cmd), &info);
	}

	return true;
}

VkResult WrappedVulkan::vkBeginCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo)
{
	VkResourceRecord *record = GetRecord(cmdBuffer);
	RDCASSERT(record);

	if(record)
	{
		if(record->bakedCommands)
			record->bakedCommands->Delete(GetResourceManager());

		record->bakedCommands = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());

		{
			SCOPED_SERIALISE_CONTEXT(BEGIN_CMD_BUFFER);
			Serialise_vkBeginCommandBuffer(cmdBuffer, pBeginInfo);
			
			record->AddChunk(scope.Get());
		}
	}

	VkCmdBufferBeginInfo unwrappedInfo = *pBeginInfo;
	unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);

	return ObjDisp(cmdBuffer)->BeginCommandBuffer(Unwrap(cmdBuffer), &unwrappedInfo);
}

bool WrappedVulkan::Serialise_vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));

	ResourceId bakedCmdId;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
	}

	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);

	if(m_State < WRITING)
	{
		m_LastCmdBufferID = cmdId;
		m_CmdBuffersInProgress--;
	}
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdId))
		{
			cmdBuffer = PartialCmdBuf();
			RDCDEBUG("Ending partial command buffer for %llu baked to %llu", cmdId, bakeId);

			if(m_PartialReplayData.renderPassActive)
				ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));

			ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));

			m_PartialReplayData.partialParent = ResourceId();
		}

		m_CmdBufferInfo[cmdId].curEventID = 0;
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);

		GetResourceManager()->RemoveReplacement(cmdId);

		ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));

		if(!m_CmdBufferInfo[m_LastCmdBufferID].curEvents.empty())
		{
			FetchDrawcall draw;
			draw.name = "API Calls";
			draw.flags |= eDraw_SetMarker;

			// the outer loop will increment the event ID but we've not
			// actually added anything just wrapped up the existing EIDs.
			m_CmdBufferInfo[m_LastCmdBufferID].curEventID--;

			AddDrawcall(draw, true);
		}

		{
			if(GetDrawcallStack().size() > 1)
				GetDrawcallStack().pop_back();
		}

		{
			m_CmdBufferInfo[bakeId].draw = m_CmdBufferInfo[m_LastCmdBufferID].draw;
			m_CmdBufferInfo[bakeId].curEventID = 0;
			m_CmdBufferInfo[bakeId].eventCount = m_CmdBufferInfo[m_LastCmdBufferID].curEventID-1;
			m_CmdBufferInfo[bakeId].drawCount = m_CmdBufferInfo[m_LastCmdBufferID].drawCount;
			
			m_CmdBufferInfo[m_LastCmdBufferID].draw = NULL;
			m_CmdBufferInfo[m_LastCmdBufferID].curEventID = 0;
			m_CmdBufferInfo[m_LastCmdBufferID].eventCount = 0;
			m_CmdBufferInfo[m_LastCmdBufferID].drawCount = 0;
		}
	}

	return true;
}

VkResult WrappedVulkan::vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
	VkResourceRecord *record = GetRecord(cmdBuffer);
	RDCASSERT(record);

	if(record)
	{
		RDCASSERT(record->bakedCommands);

		{
			SCOPED_SERIALISE_CONTEXT(END_CMD_BUFFER);
			Serialise_vkEndCommandBuffer(cmdBuffer);
			
			record->AddChunk(scope.Get());
		}

		record->Bake();
	}

	return ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));
}

bool WrappedVulkan::Serialise_vkResetCommandBuffer(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkCmdBufferResetFlags, fl, flags);

	ResourceId bakedCmdId;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
	}

	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);
	
	VkCmdBufferCreateInfo info;
	VkDevice device = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		device = m_CmdBufferInfo[cmdId].device;
		info = m_CmdBufferInfo[cmdId].createInfo;
	}
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	m_pSerialiser->Serialise("createInfo", info);

	if(m_State == EXECUTING)
	{
		// VKTODOHIGH check how vkResetCommandBuffer interacts with partial replays
	}
	else if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkCmdBuffer cmd = VK_NULL_HANDLE;

		if(!GetResourceManager()->HasLiveResource(bakeId))
		{
			VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &info, &cmd);

			if(ret != VK_SUCCESS)
			{
				RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
			}
			else
			{
				ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
				GetResourceManager()->AddLiveResource(bakeId, cmd);
			}

			// whenever a vkCmd command-building chunk asks for the command buffer, it
			// will get our baked version.
			GetResourceManager()->ReplaceResource(cmdId, bakeId);
		}
		else
		{
			cmd = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);
		}

		ObjDisp(device)->ResetCommandBuffer(Unwrap(cmd), fl);
	}

	return true;
}

VkResult WrappedVulkan::vkResetCommandBuffer(
	  VkCmdBuffer                                 cmdBuffer,
    VkCmdBufferResetFlags                       flags)
{
	VkResourceRecord *record = GetRecord(cmdBuffer);
	RDCASSERT(record);

	if(record)
	{
		if(record->bakedCommands)
			record->bakedCommands->Delete(GetResourceManager());
		
		record->bakedCommands = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());

		// VKTODOHIGH do we need to actually serialise this at all? all it does is
		// reset a command buffer to be able to begin again. We could just move the
		// logic to create new baked commands from begin to here, and skip 
		// serialising this (as we never re-begin a cmd buffer, we make a new copy
		// for each bake).
		{
			SCOPED_SERIALISE_CONTEXT(RESET_CMD_BUFFER);
			Serialise_vkResetCommandBuffer(cmdBuffer, flags);
			
			record->AddChunk(scope.Get());
		}
	}

	return ObjDisp(cmdBuffer)->ResetCommandBuffer(Unwrap(cmdBuffer), flags);
}

// Command buffer building functions

bool WrappedVulkan::Serialise_vkCmdBeginRenderPass(
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkRenderPassBeginInfo, beginInfo, *pRenderPassBegin);
	SERIALISE_ELEMENT(VkRenderPassContents, cont, contents);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			m_PartialReplayData.renderPassActive = true;
			ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &beginInfo, cont);

			m_PartialReplayData.state.renderPass = GetResourceManager()->GetOriginalID(VKMGR()->GetNonDispWrapper(beginInfo.renderPass)->id);
			m_PartialReplayData.state.framebuffer = GetResourceManager()->GetOriginalID(VKMGR()->GetNonDispWrapper(beginInfo.framebuffer)->id);
			m_PartialReplayData.state.renderArea = beginInfo.renderArea;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &beginInfo, cont);

		const string desc = m_pSerialiser->GetDebugStr();

		// VKTODOMED change the name to show render pass load-op
		AddEvent(BEGIN_RENDERPASS, desc);
		FetchDrawcall draw;
		draw.name = "Render Pass Start";
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedVulkan::vkCmdBeginRenderPass(
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents)
{
	VkRenderPassBeginInfo unwrappedInfo = *pRenderPassBegin;
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
	unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);
	ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &unwrappedInfo, contents);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BEGIN_RENDERPASS);
		Serialise_vkCmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);
		// VKTODOMED should mark framebuffer read and attachments write
		record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->framebuffer), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			m_PartialReplayData.renderPassActive = false;
			ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));

			m_PartialReplayData.state.renderPass = ResourceId();
			m_PartialReplayData.state.framebuffer = ResourceId();
			RDCEraseEl(m_PartialReplayData.state.renderArea);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));
	}

	return true;
}

void WrappedVulkan::vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(END_RENDERPASS);
		Serialise_vkCmdEndRenderPass(cmdBuffer);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdBindPipeline(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkPipelineBindPoint, bind, pipelineBindPoint);
	SERIALISE_ELEMENT(ResourceId, pipeid, GetResID(pipeline));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			pipeline = GetResourceManager()->GetLiveHandle<VkPipeline>(pipeid);
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindPipeline(Unwrap(cmdBuffer), bind, Unwrap(pipeline));
			if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				m_PartialReplayData.state.graphics.pipeline = pipeid;
			else
				m_PartialReplayData.state.compute.pipeline = pipeid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		pipeline = GetResourceManager()->GetLiveHandle<VkPipeline>(pipeid);

		// track this while reading, as we need to bind current topology & index byte width to draws
		if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
			m_PartialReplayData.state.graphics.pipeline = pipeid;
		else
			m_PartialReplayData.state.compute.pipeline = pipeid;

		ObjDisp(cmdBuffer)->CmdBindPipeline(Unwrap(cmdBuffer), bind, Unwrap(pipeline));
	}

	return true;
}

void WrappedVulkan::vkCmdBindPipeline(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline)
{
	ObjDisp(cmdBuffer)->CmdBindPipeline(Unwrap(cmdBuffer), pipelineBindPoint, Unwrap(pipeline));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_PIPELINE);
		Serialise_vkCmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(pipeline), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDescriptorSets(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipelineLayout                            layout,
			uint32_t                                    firstSet,
			uint32_t                                    setCount,
			const VkDescriptorSet*                      pDescriptorSets,
			uint32_t                                    dynamicOffsetCount,
			const uint32_t*                             pDynamicOffsets)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, layoutid, GetResID(layout));
	SERIALISE_ELEMENT(VkPipelineBindPoint, bind, pipelineBindPoint);
	SERIALISE_ELEMENT(uint32_t, first, firstSet);

	SERIALISE_ELEMENT(uint32_t, numSets, setCount);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	ResourceId *descriptorIDs = new ResourceId[numSets];

	VkDescriptorSet *sets = (VkDescriptorSet *)pDescriptorSets;
	if(m_State < WRITING)
		sets = new VkDescriptorSet[numSets];

	for(uint32_t i=0; i < numSets; i++)
	{
		if(m_State >= WRITING) descriptorIDs[i] = GetResID(sets[i]);
		m_pSerialiser->Serialise("DescriptorSet", descriptorIDs[i]);
		if(m_State < WRITING)  sets[i] = Unwrap(GetResourceManager()->GetLiveHandle<VkDescriptorSet>(descriptorIDs[i]));
	}

	SERIALISE_ELEMENT(uint32_t, offsCount, dynamicOffsetCount);
	SERIALISE_ELEMENT_ARR_OPT(uint32_t, offs, pDynamicOffsets, offsCount, offsCount > 0);

	if(m_State == EXECUTING)
	{
		layout = GetResourceManager()->GetLiveHandle<VkPipelineLayout>(layoutid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), bind, Unwrap(layout), first, numSets, sets, offsCount, offs);

			vector<ResourceId> &descsets =
				(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				? m_PartialReplayData.state.graphics.descSets
				: m_PartialReplayData.state.compute.descSets;

			// expand as necessary
			if(descsets.size() < first + numSets)
				descsets.resize(first + numSets);

			for(uint32_t i=0; i < numSets; i++)
				descsets[first+i] = descriptorIDs[i];

			// if there are dynamic offsets, bake them into the current bindings by alias'ing
			// the image layout member (which is never used for buffer views).
			// This lets us look it up easily when we want to show the current pipeline state
			RDCCOMPILE_ASSERT(sizeof(VkImageLayout) >= sizeof(uint32_t), "Can't alias image layout for dynamic offset!");
			if(offsCount > 0)
			{
				uint32_t o = 0;

				// spec states that dynamic offsets precisely match all the offsets needed for these
				// sets, in order of set N before set N+1, binding X before binding X+1 within a set,
				// and in array element order within a binding
				for(uint32_t i=0; i < numSets; i++)
				{
					const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[descriptorIDs[i]];

					for(size_t b=0; b < layout.bindings.size(); b++)
					{
						// not dynamic, doesn't need an offset
						if(layout.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
							 layout.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
							 continue;

						// assign every array element an offset according to array size
						for(uint32_t a=0; a < layout.bindings[b].arraySize; a++)
						{
							RDCASSERT(o < offsCount);
							uint32_t *alias = (uint32_t *)&m_DescriptorSetInfo[descriptorIDs[i]].currentBindings[b][a].imageLayout;
							*alias = offs[o++];
						}
					}
				}
			}
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		layout = GetResourceManager()->GetLiveHandle<VkPipelineLayout>(layoutid);

		ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), bind, Unwrap(layout), first, numSets, sets, offsCount, offs);
	}

	if(m_State < WRITING)
		SAFE_DELETE_ARRAY(sets);

	SAFE_DELETE_ARRAY(descriptorIDs);
	SAFE_DELETE_ARRAY(offs);

	return true;
}

void WrappedVulkan::vkCmdBindDescriptorSets(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipelineLayout                            layout,
			uint32_t                                    firstSet,
			uint32_t                                    setCount,
			const VkDescriptorSet*                      pDescriptorSets,
			uint32_t                                    dynamicOffsetCount,
			const uint32_t*                             pDynamicOffsets)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSet *unwrapped = new VkDescriptorSet[setCount];
	for(uint32_t i=0; i < setCount; i++)
		unwrapped[i] = Unwrap(pDescriptorSets[i]);

	ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), pipelineBindPoint, Unwrap(layout), firstSet, setCount, unwrapped, dynamicOffsetCount, pDynamicOffsets);

	SAFE_DELETE_ARRAY(unwrapped);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_DESCRIPTOR_SET);
		Serialise_vkCmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
		record->boundDescSets.insert(pDescriptorSets, pDescriptorSets + setCount);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicViewportState));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		dynamicViewportState = GetResourceManager()->GetLiveHandle<VkDynamicViewportState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindDynamicViewportState(Unwrap(cmdBuffer), Unwrap(dynamicViewportState));
			m_PartialReplayData.state.dynamicVP = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicViewportState = GetResourceManager()->GetLiveHandle<VkDynamicViewportState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicViewportState(Unwrap(cmdBuffer), Unwrap(dynamicViewportState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicViewportState(Unwrap(cmdBuffer), Unwrap(dynamicViewportState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_VP_STATE);
		Serialise_vkCmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicViewportState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                        dynamicRasterState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicRasterState));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		dynamicRasterState = GetResourceManager()->GetLiveHandle<VkDynamicRasterState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindDynamicRasterState(Unwrap(cmdBuffer), Unwrap(dynamicRasterState));
			m_PartialReplayData.state.dynamicRS = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicRasterState = GetResourceManager()->GetLiveHandle<VkDynamicRasterState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicRasterState(Unwrap(cmdBuffer), Unwrap(dynamicRasterState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                      dynamicRasterState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicRasterState(Unwrap(cmdBuffer), Unwrap(dynamicRasterState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_RS_STATE);
		Serialise_vkCmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicRasterState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicColorBlendState));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		dynamicColorBlendState = GetResourceManager()->GetLiveHandle<VkDynamicColorBlendState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindDynamicColorBlendState(Unwrap(cmdBuffer), Unwrap(dynamicColorBlendState));
			m_PartialReplayData.state.dynamicCB = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicColorBlendState = GetResourceManager()->GetLiveHandle<VkDynamicColorBlendState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicColorBlendState(Unwrap(cmdBuffer), Unwrap(dynamicColorBlendState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicColorBlendState(Unwrap(cmdBuffer), Unwrap(dynamicColorBlendState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_CB_STATE);
		Serialise_vkCmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicColorBlendState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicDepthStencilState));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		dynamicDepthStencilState = GetResourceManager()->GetLiveHandle<VkDynamicDepthStencilState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindDynamicDepthStencilState(Unwrap(cmdBuffer), Unwrap(dynamicDepthStencilState));
			m_PartialReplayData.state.dynamicDS = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicDepthStencilState = GetResourceManager()->GetLiveHandle<VkDynamicDepthStencilState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicDepthStencilState(Unwrap(cmdBuffer), Unwrap(dynamicDepthStencilState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicDepthStencilState(Unwrap(cmdBuffer), Unwrap(dynamicDepthStencilState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_DS_STATE);
		Serialise_vkCmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicDepthStencilState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, start, startBinding);
	SERIALISE_ELEMENT(uint32_t, count, bindingCount);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	vector<ResourceId> bufids;
	vector<VkBuffer> bufs;
	vector<VkDeviceSize> offs;

	for(uint32_t i=0; i < count; i++)
	{
		ResourceId id;
		VkDeviceSize o;
		if(m_State >= WRITING)
		{
			id = GetResID(pBuffers[i]);
			o = pOffsets[i];
		}

		m_pSerialiser->Serialise("pBuffers[]", id);
		m_pSerialiser->Serialise("pOffsets[]", o);

		if(m_State < WRITING)
		{
			bufids.push_back(id);
			bufs.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkBuffer>(id)));
			offs.push_back(o);
		}
	}

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), start, count, &bufs[0], &offs[0]);

			if(m_PartialReplayData.state.vbuffers.size() < start + count)
				m_PartialReplayData.state.vbuffers.resize(start + count);

			for(uint32_t i=0; i < count; i++)
			{
				m_PartialReplayData.state.vbuffers[start + i].buf = bufids[i];
				m_PartialReplayData.state.vbuffers[start + i].offs = offs[i];
			}
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		
		ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), start, count, &bufs[0], &offs[0]);
	}

	return true;
}

void WrappedVulkan::vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkBuffer *unwrapped = new VkBuffer[bindingCount];
	for(uint32_t i=0; i < bindingCount; i++)
		unwrapped[i] = Unwrap(pBuffers[i]);

	ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), startBinding, bindingCount, unwrapped, pOffsets);

	SAFE_DELETE_ARRAY(unwrapped);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_VERTEX_BUFFERS);
		Serialise_vkCmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);

		record->AddChunk(scope.Get());
		for(uint32_t i=0; i < bindingCount; i++)
			record->MarkResourceFrameReferenced(GetResID(pBuffers[i]), eFrameRef_Read);
	}
}


bool WrappedVulkan::Serialise_vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);
	SERIALISE_ELEMENT(VkIndexType, idxType, indexType);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindIndexBuffer(Unwrap(cmdBuffer), Unwrap(buffer), offs, idxType);

			m_PartialReplayData.state.ibuffer.buf = bufid;
			m_PartialReplayData.state.ibuffer.offs = offs;
			m_PartialReplayData.state.ibuffer.bytewidth = idxType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		// track this while reading, as we need to bind current topology & index byte width to draws
		m_PartialReplayData.state.ibuffer.bytewidth = idxType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
		
		ObjDisp(cmdBuffer)->CmdBindIndexBuffer(Unwrap(cmdBuffer), Unwrap(buffer), offs, idxType);
	}

	return true;
}

void WrappedVulkan::vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	ObjDisp(cmdBuffer)->CmdBindIndexBuffer(Unwrap(cmdBuffer), Unwrap(buffer), offset, indexType);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_INDEX_BUFFER);
		Serialise_vkCmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdPipelineBarrier(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			VkBool32                                    byRegion,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkPipelineStageFlags, src, srcStageMask);
	SERIALISE_ELEMENT(VkPipelineStageFlags, dest, destStageMask);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	SERIALISE_ELEMENT(VkBool32, region, byRegion);

	SERIALISE_ELEMENT(uint32_t, memCount, memBarrierCount);

	vector<VkGenericStruct*> mems;
	vector<VkImageMemoryBarrier> imTrans;

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

			if(m_State < WRITING)
				mems.push_back((VkGenericStruct *)new VkBufferMemoryBarrier(barrier));
		}
		else if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkImageMemoryBarrier, barrier, *((VkImageMemoryBarrier *)ppMemBarriers[i]));

			if(m_State < WRITING)
			{
				mems.push_back((VkGenericStruct *)new VkImageMemoryBarrier(barrier));
				imTrans.push_back(barrier);
			}
		}
	}
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), src, dest, region, memCount, (const void **)&mems[0]);

			ResourceId cmd = GetResID(PartialCmdBuf());
			GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), src, dest, region, memCount, (const void **)&mems[0]);
		
		ResourceId cmd = GetResID(cmdBuffer);
		GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);
	}

	for(size_t i=0; i < mems.size(); i++)
		delete mems[i];

	return true;
}

void WrappedVulkan::vkCmdPipelineBarrier(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			VkBool32                                    byRegion,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers)
{

	{
		// VKTODOLOW this should be a persistent per-thread array that resizes up
		// to a high water mark, so we don't have to allocate
		vector<VkImageMemoryBarrier> im;
		vector<VkBufferMemoryBarrier> buf;

		// ensure we don't resize while looping so we can take pointers
		im.reserve(memBarrierCount);
		buf.reserve(memBarrierCount);

		void **unwrappedBarriers = new void*[memBarrierCount];

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkGenericStruct *header = (VkGenericStruct *)ppMemBarriers[i];

			if(header->sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
			{
				VkImageMemoryBarrier barrier = *(VkImageMemoryBarrier *)header;
				barrier.image = Unwrap(barrier.image);
				im.push_back(barrier);
				unwrappedBarriers[i] = &im.back();
			}
			else if(header->sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
			{
				VkBufferMemoryBarrier barrier = *(VkBufferMemoryBarrier *)header;
				barrier.buffer = Unwrap(barrier.buffer);
				buf.push_back(barrier);
				unwrappedBarriers[i] = &buf.back();
			}
			else
			{
				unwrappedBarriers[i] = (void *)ppMemBarriers[i];
			}
		}

		ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), srcStageMask, destStageMask, byRegion, memBarrierCount, unwrappedBarriers);

		SAFE_DELETE_ARRAY(unwrappedBarriers);
	}

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(PIPELINE_BARRIER);
		Serialise_vkCmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);

		record->AddChunk(scope.Get());

		vector<VkImageMemoryBarrier> imTrans;

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkStructureType stype = ((VkGenericStruct *)ppMemBarriers[i])->sType;

			if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
				imTrans.push_back(*((VkImageMemoryBarrier *)ppMemBarriers[i]));
		}
		
		ResourceId cmd = GetResID(cmdBuffer);
		GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);

		// VKTODOMED do we need to mark frame referenced the resources in the barrier? if they're not referenced
		// elsewhere, perhaps they can be dropped
	}
}

bool WrappedVulkan::Serialise_vkCmdBeginQuery(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    slot,
    VkQueryControlFlags                         flags)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queryPool));
	SERIALISE_ELEMENT(uint32_t, s, slot);
	SERIALISE_ELEMENT(VkQueryControlFlagBits, f, (VkQueryControlFlagBits)flags); // serialise as 'bits' type to get nice enum values

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBeginQuery(Unwrap(cmdBuffer), Unwrap(queryPool), s, f);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);
		
		ObjDisp(cmdBuffer)->CmdBeginQuery(Unwrap(cmdBuffer), Unwrap(queryPool), s, f);
	}

	return true;
}

void WrappedVulkan::vkCmdBeginQuery(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    slot,
    VkQueryControlFlags                         flags)
{
	ObjDisp(cmdBuffer)->CmdBeginQuery(Unwrap(cmdBuffer), Unwrap(queryPool), slot, flags);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BEGIN_QUERY);
		Serialise_vkCmdBeginQuery(cmdBuffer, queryPool, slot, flags);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdEndQuery(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    slot)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queryPool));
	SERIALISE_ELEMENT(uint32_t, s, slot);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdEndQuery(Unwrap(cmdBuffer), Unwrap(queryPool), s);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);
		
		ObjDisp(cmdBuffer)->CmdEndQuery(Unwrap(cmdBuffer), Unwrap(queryPool), s);
	}

	return true;
}

void WrappedVulkan::vkCmdEndQuery(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    slot)
{
	ObjDisp(cmdBuffer)->CmdEndQuery(Unwrap(cmdBuffer), Unwrap(queryPool), slot);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(END_QUERY);
		Serialise_vkCmdEndQuery(cmdBuffer, queryPool, slot);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdResetQueryPool(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    startQuery,
    uint32_t                                    queryCount)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queryPool));
	SERIALISE_ELEMENT(uint32_t, start, startQuery);
	SERIALISE_ELEMENT(uint32_t, count, queryCount);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdResetQueryPool(Unwrap(cmdBuffer), Unwrap(queryPool), start, count);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);
		
		ObjDisp(cmdBuffer)->CmdResetQueryPool(Unwrap(cmdBuffer), Unwrap(queryPool), start, count);
	}

	return true;
}

void WrappedVulkan::vkCmdResetQueryPool(
    VkCmdBuffer                                 cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    startQuery,
    uint32_t                                    queryCount)
{
	ObjDisp(cmdBuffer)->CmdResetQueryPool(Unwrap(cmdBuffer), Unwrap(queryPool), startQuery, queryCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(RESET_QUERY_POOL);
		Serialise_vkCmdResetQueryPool(cmdBuffer, queryPool, startQuery, queryCount);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdDbgMarkerBegin(
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker)
{
	string name = pMarker ? string(pMarker) : "";

	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	m_pSerialiser->Serialise("Name", name);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == READING)
	{
		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, false);
	}

	return true;
}

void WrappedVulkan::vkCmdDbgMarkerBegin(
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker)
{
	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
		Serialise_vkCmdDbgMarkerBegin(cmdBuffer, pMarker);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDbgMarkerEnd(VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == READING && !m_CmdBufferInfo[m_LastCmdBufferID].curEvents.empty())
	{
		FetchDrawcall draw;
		draw.name = "API Calls";
		draw.flags |= eDraw_SetMarker;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedVulkan::vkCmdDbgMarkerEnd(
	VkCmdBuffer  cmdBuffer)
{
	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(END_EVENT);
		Serialise_vkCmdDbgMarkerEnd(cmdBuffer);

		record->AddChunk(scope.Get());
	}
}
