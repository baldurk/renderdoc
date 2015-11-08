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
			Serialiser*                                 localSerialiser,
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
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_CMD_POOL);
				Serialise_vkCreateCommandPool(localSerialiser, device, pCreateInfo, pCmdPool);

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

			// if pNext is non-NULL, need to do a deep copy
			// we don't support any extensions on VkCmdBufferCreateInfo anyway
			RDCASSERT(pCreateInfo->pNext == NULL);

			record->cmdInfo = new CmdBufferRecordingInfo();

			record->cmdInfo->device = device;
			record->cmdInfo->createInfo = *pCreateInfo;
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pCmdBuffer);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkBeginCommandBuffer(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));

	ResourceId bakedCmdId;
	VkCmdBufferCreateInfo createInfo;
	VkDevice device = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
		
		RDCASSERT(record->cmdInfo);
		device = record->cmdInfo->device;
		createInfo = record->cmdInfo->createInfo;
	}

	SERIALISE_ELEMENT(VkCmdBufferBeginInfo, info, *pBeginInfo);
	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);

	if(m_State < WRITING)
	{
		m_LastCmdBufferID = cmdId;
		m_CmdBuffersInProgress++;
	}
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	localSerialiser->Serialise("createInfo", createInfo);

	if(m_State < WRITING)
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

	if(m_State == EXECUTING)
	{
		const vector<uint32_t> &baseEvents = m_PartialReplayData.cmdBufferSubmits[bakeId];
		uint32_t length = m_BakedCmdBufferInfo[bakeId].eventCount;

		for(auto it=baseEvents.begin(); it != baseEvents.end(); ++it)
		{
			if(*it <= m_LastEventID && m_LastEventID < (*it + length))
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

		m_BakedCmdBufferInfo[cmdId].curEventID = 1;
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
			m_BakedCmdBufferInfo[cmdId].draw = draw;
			
			// On queue submit we increment all child events/drawcalls by
			// m_RootEventID insert them into the tree.
			m_BakedCmdBufferInfo[cmdId].curEventID = 1;
			m_BakedCmdBufferInfo[cmdId].eventCount = 0;
			m_BakedCmdBufferInfo[cmdId].drawCount = 0;

			m_BakedCmdBufferInfo[cmdId].drawStack.push_back(draw);
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
		record->bakedCommands->cmdInfo = new CmdBufferRecordingInfo();

		record->bakedCommands->cmdInfo->device = record->cmdInfo->device;
		record->bakedCommands->cmdInfo->createInfo = record->cmdInfo->createInfo;

		{
			CACHE_THREAD_SERIALISER();

			SCOPED_SERIALISE_CONTEXT(BEGIN_CMD_BUFFER);
			Serialise_vkBeginCommandBuffer(localSerialiser, cmdBuffer, pBeginInfo);
			
			record->AddChunk(scope.Get());
		}
	}

	VkCmdBufferBeginInfo unwrappedInfo = *pBeginInfo;
	unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);

	return ObjDisp(cmdBuffer)->BeginCommandBuffer(Unwrap(cmdBuffer), &unwrappedInfo);
}

bool WrappedVulkan::Serialise_vkEndCommandBuffer(Serialiser* localSerialiser, VkCmdBuffer cmdBuffer)
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

		m_BakedCmdBufferInfo[cmdId].curEventID = 0;
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);

		GetResourceManager()->RemoveReplacement(cmdId);

		ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));

		if(!m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.empty())
		{
			FetchDrawcall draw;
			draw.name = "API Calls";
			draw.flags |= eDraw_SetMarker;

			// VKTODOLOW hack, give this drawcall the same event ID as its last child, by
			// decrementing then incrementing again.
			m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID--;

			AddDrawcall(draw, true);
			
			m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
		}

		{
			if(GetDrawcallStack().size() > 1)
				GetDrawcallStack().pop_back();
		}

		{
			m_BakedCmdBufferInfo[bakeId].draw = m_BakedCmdBufferInfo[m_LastCmdBufferID].draw;
			m_BakedCmdBufferInfo[bakeId].curEventID = 0;
			m_BakedCmdBufferInfo[bakeId].eventCount = m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID-1;
			RDCASSERT(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID >= 1);
			m_BakedCmdBufferInfo[bakeId].drawCount = m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount;
			
			m_BakedCmdBufferInfo[m_LastCmdBufferID].draw = NULL;
			m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID = 0;
			m_BakedCmdBufferInfo[m_LastCmdBufferID].eventCount = 0;
			m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount = 0;
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
			CACHE_THREAD_SERIALISER();

			SCOPED_SERIALISE_CONTEXT(END_CMD_BUFFER);
			Serialise_vkEndCommandBuffer(localSerialiser, cmdBuffer);
			
			record->AddChunk(scope.Get());
		}

		record->Bake();
	}

	return ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));
}

bool WrappedVulkan::Serialise_vkResetCommandBuffer(Serialiser* localSerialiser, VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkCmdBufferResetFlags, fl, flags);

	ResourceId bakedCmdId;
	VkCmdBufferCreateInfo createInfo;
	VkDevice device = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
		
		RDCASSERT(record->cmdInfo);
		device = record->cmdInfo->device;
		createInfo = record->cmdInfo->createInfo;
	}

	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);

	if(m_State < WRITING)
	{
		m_LastCmdBufferID = cmdId;
		m_CmdBuffersInProgress++;
	}
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	localSerialiser->Serialise("createInfo", createInfo);

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
		record->bakedCommands->cmdInfo = new CmdBufferRecordingInfo();

		record->bakedCommands->cmdInfo->device = record->cmdInfo->device;
		record->bakedCommands->cmdInfo->createInfo = record->cmdInfo->createInfo;

		// VKTODOHIGH do we need to actually serialise this at all? all it does is
		// reset a command buffer to be able to begin again. We could just move the
		// logic to create new baked commands from begin to here, and skip 
		// serialising this (as we never re-begin a cmd buffer, we make a new copy
		// for each bake).
		{
			CACHE_THREAD_SERIALISER();

			SCOPED_SERIALISE_CONTEXT(RESET_CMD_BUFFER);
			Serialise_vkResetCommandBuffer(localSerialiser, cmdBuffer, flags);
			
			record->AddChunk(scope.Get());
		}
	}

	return ObjDisp(cmdBuffer)->ResetCommandBuffer(Unwrap(cmdBuffer), flags);
}

// Command buffer building functions

bool WrappedVulkan::Serialise_vkCmdBeginRenderPass(
			Serialiser*                                 localSerialiser,
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

			m_PartialReplayData.state.subpass = 0;

			m_PartialReplayData.state.renderPass = GetResourceManager()->GetNonDispWrapper(beginInfo.renderPass)->id;
			m_PartialReplayData.state.framebuffer = GetResourceManager()->GetNonDispWrapper(beginInfo.framebuffer)->id;
			m_PartialReplayData.state.renderArea = beginInfo.renderArea;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &beginInfo, cont);
		
		// track during reading
		m_PartialReplayData.state.subpass = 0;
		m_PartialReplayData.state.renderPass = GetResourceManager()->GetNonDispWrapper(beginInfo.renderPass)->id;

		const string desc = localSerialiser->GetDebugStr();

		string loadDesc = "";

		const VulkanCreationInfo::RenderPass &info = m_CreationInfo.m_RenderPass[m_PartialReplayData.state.renderPass];

		const vector<VulkanCreationInfo::RenderPass::Attachment> &atts = info.attachments;

		if(atts.empty())
		{
			loadDesc = "-";
		}
		else
		{
			bool allsame = true;
			bool allsameexceptstencil = true;

			for(size_t i=1; i < atts.size(); i++)
				if(atts[i].loadOp != atts[0].loadOp)
					allsame = allsameexceptstencil = false;

			int32_t dsAttach = -1;

			for(size_t i=0; i < info.subpasses.size(); i++)
			{
				if(info.subpasses[i].depthstencilAttachment != -1)
				{
					dsAttach = info.subpasses[i].depthstencilAttachment;
					break;
				}
			}

			if(dsAttach != -1 && allsame && atts.size() > 1)
			{
				size_t o = (dsAttach == 0) ? 1 : 0;

				if(atts[dsAttach].stencilLoadOp != atts[o].loadOp)
					allsame = false;
			}

			if(allsame)
			{
				if(atts[0].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
					loadDesc = "Clear";
				if(atts[0].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
					loadDesc = "Load";
				if(atts[0].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
					loadDesc = "Don't Care";
			}
			else if(allsameexceptstencil)
			{
				if(atts[0].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
					loadDesc = "Clear";
				if(atts[0].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
					loadDesc = "Load";
				if(atts[0].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
					loadDesc = "Don't Care";
				
				if(dsAttach >= 0 && dsAttach < atts.size())
				{
					if(atts[dsAttach].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
						loadDesc += ", Stencil=Clear";
					if(atts[dsAttach].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
						loadDesc += ", Stencil=Load";
					if(atts[dsAttach].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
						loadDesc += ", Stencil=Don't Care";
				}
			}
			else
			{
				// VKTODOLOW improve text for this path
				loadDesc = "Different load ops";
			}
		}

		AddEvent(BEGIN_RENDERPASS, desc);
		FetchDrawcall draw;
		draw.name = StringFormat::Fmt("vkCmdBeginRenderPass(%s)", loadDesc.c_str());
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BEGIN_RENDERPASS);
		Serialise_vkCmdBeginRenderPass(localSerialiser, cmdBuffer, pRenderPassBegin, contents);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);

		VkResourceRecord *fb = GetRecord(pRenderPassBegin->framebuffer);
		
		record->MarkResourceFrameReferenced(fb->GetResourceID(), eFrameRef_Read);
		for(size_t i=0; i < ARRAY_COUNT(fb->imageAttachments); i++)
		{
			if(fb->imageAttachments[i] == NULL) break;
			record->MarkResourceFrameReferenced(fb->imageAttachments[i]->baseResource, eFrameRef_Write);
			record->MarkResourceFrameReferenced(fb->imageAttachments[i]->baseResourceMem, eFrameRef_Read);
			record->cmdInfo->dirtied.insert(fb->imageAttachments[i]->baseResource);
		}
	}
}

bool WrappedVulkan::Serialise_vkCmdNextSubpass(
	Serialiser*                                 localSerialiser,
	VkCmdBuffer                                 cmdBuffer,
	VkRenderPassContents                        contents)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkRenderPassContents, cont, contents);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			m_PartialReplayData.state.subpass++;

			ObjDisp(cmdBuffer)->CmdNextSubpass(Unwrap(cmdBuffer), cont);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdNextSubpass(Unwrap(cmdBuffer), cont);

		// track during reading
		m_PartialReplayData.state.subpass++;

		const string desc = localSerialiser->GetDebugStr();

		AddEvent(NEXT_SUBPASS, desc);
		FetchDrawcall draw;
		draw.name = StringFormat::Fmt("vkCmdNextSubpass() => %u", m_PartialReplayData.state.subpass);
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedVulkan::vkCmdNextSubpass(
	VkCmdBuffer                                 cmdBuffer,
	VkRenderPassContents                        contents)
{
	ObjDisp(cmdBuffer)->CmdNextSubpass(Unwrap(cmdBuffer), contents);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(NEXT_SUBPASS);
		Serialise_vkCmdNextSubpass(localSerialiser, cmdBuffer, contents);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdExecuteCommands(
	Serialiser*                                 localSerialiser,
	VkCmdBuffer                                 cmdBuffer,
	uint32_t                                    cmdBuffersCount,
	const VkCmdBuffer*                          pCmdBuffers)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, count, cmdBuffersCount);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	vector<ResourceId> cmdids;
	vector<VkCmdBuffer> cmds;

	for(uint32_t i=0; i < count; i++)
	{
		ResourceId id;
		if(m_State >= WRITING)
			id = GetRecord(pCmdBuffers[i])->bakedCommands->GetResourceID();

		localSerialiser->Serialise("pCmdBuffers[]", id);

		if(m_State < WRITING)
		{
			cmdids.push_back(id);
			cmds.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkCmdBuffer>(id)));
		}
	}

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			
			// VKTODOHIGH proper handling of partial sub-executes
			ObjDisp(cmdBuffer)->CmdExecuteCommands(Unwrap(cmdBuffer), count, &cmds[0]);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdExecuteCommands(Unwrap(cmdBuffer), count, &cmds[0]);

		const string desc = localSerialiser->GetDebugStr();

		AddEvent(NEXT_SUBPASS, desc);
		FetchDrawcall draw;
		draw.name = "vkCmdExecuteCommands()";
		draw.flags |= eDraw_CmdList;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedVulkan::vkCmdExecuteCommands(
	VkCmdBuffer                                 cmdBuffer,
	uint32_t                                    cmdBuffersCount,
	const VkCmdBuffer*                          pCmdBuffers)
{
	VkCmdBuffer *unwrapped = GetTempArray<VkCmdBuffer>(cmdBuffersCount);
	for(uint32_t i=0; i < cmdBuffersCount; i++) unwrapped[i] = Unwrap(pCmdBuffers[i]);
	ObjDisp(cmdBuffer)->CmdExecuteCommands(Unwrap(cmdBuffer), cmdBuffersCount, unwrapped);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(EXEC_CMDS);
		Serialise_vkCmdExecuteCommands(localSerialiser, cmdBuffer, cmdBuffersCount, pCmdBuffers);

		record->AddChunk(scope.Get());
		
		for(uint32_t i=0; i < cmdBuffersCount; i++)
		{
			VkResourceRecord *execRecord = GetRecord(pCmdBuffers[i]);
			record->cmdInfo->dirtied.insert(execRecord->bakedCommands->cmdInfo->dirtied.begin(), execRecord->bakedCommands->cmdInfo->dirtied.end());
			record->cmdInfo->boundDescSets.insert(execRecord->bakedCommands->cmdInfo->boundDescSets.begin(), execRecord->bakedCommands->cmdInfo->boundDescSets.end());
			record->cmdInfo->subcmds.push_back(execRecord);

			GetResourceManager()->MergeTransitions(record->cmdInfo->imgtransitions, execRecord->bakedCommands->cmdInfo->imgtransitions);
		}
	}
}

bool WrappedVulkan::Serialise_vkCmdEndRenderPass(
			Serialiser*                                 localSerialiser,
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
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));
		
		const string desc = localSerialiser->GetDebugStr();
		
		string storeDesc = "";

		const VulkanCreationInfo::RenderPass &info = m_CreationInfo.m_RenderPass[m_PartialReplayData.state.renderPass];

		const vector<VulkanCreationInfo::RenderPass::Attachment> &atts = info.attachments;

		if(atts.empty())
		{
			storeDesc = "-";
		}
		else
		{
			bool allsame = true;
			bool allsameexceptstencil = true;

			for(size_t i=1; i < atts.size(); i++)
				if(atts[i].storeOp != atts[0].storeOp)
					allsame = allsameexceptstencil = false;

			int32_t dsAttach = -1;

			for(size_t i=0; i < info.subpasses.size(); i++)
			{
				if(info.subpasses[i].depthstencilAttachment != -1)
				{
					dsAttach = info.subpasses[i].depthstencilAttachment;
					break;
				}
			}

			if(dsAttach != -1 && allsame && atts.size() > 1)
			{
				size_t o = (dsAttach == 0) ? 1 : 0;

				if(atts[dsAttach].stencilStoreOp != atts[o].storeOp)
					allsameexceptstencil = false;
			}

			if(allsame)
			{
				if(atts[0].storeOp == VK_ATTACHMENT_STORE_OP_STORE)
					storeDesc = "Store";
				if(atts[0].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
					storeDesc = "Don't Care";
			}
			else if(allsameexceptstencil)
			{
				if(atts[0].storeOp == VK_ATTACHMENT_STORE_OP_STORE)
					storeDesc = "Store";
				if(atts[0].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
					storeDesc = "Don't Care";
				
				if(dsAttach >= 0 && dsAttach < atts.size())
				{
					if(atts[dsAttach].stencilStoreOp == VK_ATTACHMENT_STORE_OP_STORE)
						storeDesc += ", Stencil=Store";
					if(atts[dsAttach].stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
						storeDesc += ", Stencil=Don't Care";
				}
			}
			else
			{
				// VKTODOLOW improve text for this path
				storeDesc = "Different store ops";
			}
		}

		AddEvent(END_RENDERPASS, desc);
		FetchDrawcall draw;
		draw.name = StringFormat::Fmt("vkCmdEndRenderPass(%s)", storeDesc.c_str());
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(END_RENDERPASS);
		Serialise_vkCmdEndRenderPass(localSerialiser, cmdBuffer);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdBindPipeline(
			Serialiser*                                 localSerialiser,
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

			ResourceId liveid = GetResID(pipeline);

			if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				m_PartialReplayData.state.graphics.pipeline = liveid;
			else
				m_PartialReplayData.state.compute.pipeline = liveid;

			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_VIEWPORT])
			{
				m_PartialReplayData.state.views = m_CreationInfo.m_Pipeline[liveid].viewports;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_SCISSOR])
			{
				m_PartialReplayData.state.scissors = m_CreationInfo.m_Pipeline[liveid].scissors;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_LINE_WIDTH])
			{
				m_PartialReplayData.state.lineWidth = m_CreationInfo.m_Pipeline[liveid].lineWidth;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_DEPTH_BIAS])
			{
				m_PartialReplayData.state.bias.depth = m_CreationInfo.m_Pipeline[liveid].depthBias;
				m_PartialReplayData.state.bias.biasclamp = m_CreationInfo.m_Pipeline[liveid].depthBiasClamp;
				m_PartialReplayData.state.bias.slope = m_CreationInfo.m_Pipeline[liveid].slopeScaledDepthBias;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_BLEND_CONSTANTS])
			{
				memcpy(m_PartialReplayData.state.blendConst, m_CreationInfo.m_Pipeline[liveid].blendConst, sizeof(float)*4);
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_DEPTH_BOUNDS])
			{
				m_PartialReplayData.state.mindepth = m_CreationInfo.m_Pipeline[liveid].minDepthBounds;
				m_PartialReplayData.state.maxdepth = m_CreationInfo.m_Pipeline[liveid].maxDepthBounds;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK])
			{
				m_PartialReplayData.state.front.compare = m_CreationInfo.m_Pipeline[liveid].front.stencilCompareMask;
				m_PartialReplayData.state.back.compare = m_CreationInfo.m_Pipeline[liveid].back.stencilCompareMask;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_STENCIL_WRITE_MASK])
			{
				m_PartialReplayData.state.front.write = m_CreationInfo.m_Pipeline[liveid].front.stencilWriteMask;
				m_PartialReplayData.state.back.write = m_CreationInfo.m_Pipeline[liveid].back.stencilWriteMask;
			}
			if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VK_DYNAMIC_STATE_STENCIL_REFERENCE])
			{
				m_PartialReplayData.state.front.ref = m_CreationInfo.m_Pipeline[liveid].front.stencilReference;
				m_PartialReplayData.state.back.ref = m_CreationInfo.m_Pipeline[liveid].back.stencilReference;
			}
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		pipeline = GetResourceManager()->GetLiveHandle<VkPipeline>(pipeid);

		// track this while reading, as we need to bind current topology & index byte width to draws
		if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
			m_PartialReplayData.state.graphics.pipeline = GetResID(pipeline);
		else
			m_PartialReplayData.state.compute.pipeline = GetResID(pipeline);

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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BIND_PIPELINE);
		Serialise_vkCmdBindPipeline(localSerialiser, cmdBuffer, pipelineBindPoint, pipeline);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(pipeline), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDescriptorSets(
			Serialiser*                                 localSerialiser,
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
		if(m_State >= WRITING)
			descriptorIDs[i] = GetResID(sets[i]);

		localSerialiser->Serialise("DescriptorSet", descriptorIDs[i]);

		if(m_State < WRITING)
		{
			sets[i] = GetResourceManager()->GetLiveHandle<VkDescriptorSet>(descriptorIDs[i]);
			descriptorIDs[i] = GetResID(sets[i]);
			sets[i] = Unwrap(sets[i]);
		}
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
			
			vector< vector<uint32_t> > &offsets =
				(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				? m_PartialReplayData.state.graphics.offsets
				: m_PartialReplayData.state.compute.offsets;

			// expand as necessary
			if(descsets.size() < first + numSets)
			{
				descsets.resize(first + numSets);
				offsets.resize(first + numSets);
			}

			const vector<ResourceId> &descSetLayouts = m_CreationInfo.m_PipelineLayout[GetResID(layout)].descSetLayouts;

			uint32_t *offsIter = offs;
			uint32_t dynConsumed = 0;

			// consume the offsets linearly along the descriptor set layouts
			for(uint32_t i=0; i < numSets; i++)
			{
				descsets[first+i] = descriptorIDs[i];
				uint32_t dynCount = m_CreationInfo.m_DescSetLayout[ descSetLayouts[first+i] ].dynamicCount;
				offsets[first+i].assign(offsIter, offsIter+dynCount);
				dynConsumed += dynCount;
				RDCASSERT(dynConsumed <= offsCount);
			}

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
					const DescSetLayout &layoutinfo = m_CreationInfo.m_DescSetLayout[ descSetLayouts[first+i] ];

					for(size_t b=0; b < layoutinfo.bindings.size(); b++)
					{
						// not dynamic, doesn't need an offset
						if(layoutinfo.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
							 layoutinfo.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
							 continue;

						// assign every array element an offset according to array size
						for(uint32_t a=0; a < layoutinfo.bindings[b].arraySize; a++)
						{
							RDCASSERT(o < offsCount);
							uint32_t *alias = (uint32_t *)&m_DescriptorSetState[descriptorIDs[i]].currentBindings[b][a].imageLayout;
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
	VkDescriptorSet *unwrapped = GetTempArray<VkDescriptorSet>(setCount);
	for(uint32_t i=0; i < setCount; i++) unwrapped[i] = Unwrap(pDescriptorSets[i]);

	ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), pipelineBindPoint, Unwrap(layout), firstSet, setCount, unwrapped, dynamicOffsetCount, pDynamicOffsets);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BIND_DESCRIPTOR_SET);
		Serialise_vkCmdBindDescriptorSets(localSerialiser, cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
		record->cmdInfo->boundDescSets.insert(pDescriptorSets, pDescriptorSets + setCount);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindVertexBuffers(
		Serialiser*                                 localSerialiser,
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

		localSerialiser->Serialise("pBuffers[]", id);
		localSerialiser->Serialise("pOffsets[]", o);

		if(m_State < WRITING)
		{
			VkBuffer buf = GetResourceManager()->GetLiveHandle<VkBuffer>(id);
			bufids.push_back(GetResID(buf));
			bufs.push_back(Unwrap(buf));
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
	VkBuffer *unwrapped = GetTempArray<VkBuffer>(bindingCount);
	for(uint32_t i=0; i < bindingCount; i++) unwrapped[i] = Unwrap(pBuffers[i]);

	ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), startBinding, bindingCount, unwrapped, pOffsets);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BIND_VERTEX_BUFFERS);
		Serialise_vkCmdBindVertexBuffers(localSerialiser, cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);

		record->AddChunk(scope.Get());
		for(uint32_t i=0; i < bindingCount; i++)
		{
			record->MarkResourceFrameReferenced(GetResID(pBuffers[i]), eFrameRef_Read);
			record->MarkResourceFrameReferenced(GetRecord(pBuffers[i])->baseResource, eFrameRef_Read);
		}
	}
}


bool WrappedVulkan::Serialise_vkCmdBindIndexBuffer(
		Serialiser*                                 localSerialiser,
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

			m_PartialReplayData.state.ibuffer.buf = GetResID(buffer);
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BIND_INDEX_BUFFER);
		Serialise_vkCmdBindIndexBuffer(localSerialiser, cmdBuffer, buffer, offset, indexType);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(buffer)->baseResource, eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdUpdateBuffer(
	Serialiser*                                 localSerialiser,
	VkCmdBuffer                                 cmdBuffer,
	VkBuffer                                    destBuffer,
	VkDeviceSize                                destOffset,
	VkDeviceSize                                dataSize,
	const uint32_t*                             pData)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
	SERIALISE_ELEMENT(VkDeviceSize, offs, destOffset);
	SERIALISE_ELEMENT(VkDeviceSize, sz, dataSize);
	SERIALISE_ELEMENT_BUF(byte *, bufdata, (byte *)pData, (size_t)dataSize);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdUpdateBuffer(Unwrap(cmdBuffer), Unwrap(destBuffer), offs, sz, (uint32_t *)bufdata);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdUpdateBuffer(Unwrap(cmdBuffer), Unwrap(destBuffer), offs, sz, (uint32_t *)bufdata);
	}

	SAFE_DELETE_ARRAY(bufdata);

	return true;
}

void WrappedVulkan::vkCmdUpdateBuffer(
	VkCmdBuffer                                 cmdBuffer,
	VkBuffer                                    destBuffer,
	VkDeviceSize                                destOffset,
	VkDeviceSize                                dataSize,
	const uint32_t*                             pData)
{
	ObjDisp(cmdBuffer)->CmdUpdateBuffer(Unwrap(cmdBuffer), Unwrap(destBuffer), destOffset, dataSize, pData);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(UPDATE_BUF);
		Serialise_vkCmdUpdateBuffer(localSerialiser, cmdBuffer, destBuffer, destOffset, dataSize, pData);

		record->AddChunk(scope.Get());

		VkResourceRecord *buf = GetRecord(destBuffer);

		// mark buffer just as read, and memory behind as write & dirtied
		record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
		record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
		record->cmdInfo->dirtied.insert(buf->baseResource);
	}
}

bool WrappedVulkan::Serialise_vkCmdFillBuffer(
	Serialiser*                                 localSerialiser,
	VkCmdBuffer                                 cmdBuffer,
	VkBuffer                                    destBuffer,
	VkDeviceSize                                destOffset,
	VkDeviceSize                                fillSize,
	uint32_t                                    data)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
	SERIALISE_ELEMENT(VkDeviceSize, offs, destOffset);
	SERIALISE_ELEMENT(VkDeviceSize, sz, fillSize);
	SERIALISE_ELEMENT(uint32_t, d, data);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdFillBuffer(Unwrap(cmdBuffer), Unwrap(destBuffer), offs, sz, d);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdFillBuffer(Unwrap(cmdBuffer), Unwrap(destBuffer), offs, sz, d);
	}

	return true;
}

void WrappedVulkan::vkCmdFillBuffer(
	VkCmdBuffer                                 cmdBuffer,
	VkBuffer                                    destBuffer,
	VkDeviceSize                                destOffset,
	VkDeviceSize                                fillSize,
	uint32_t                                    data)
{
	ObjDisp(cmdBuffer)->CmdFillBuffer(Unwrap(cmdBuffer), Unwrap(destBuffer), destOffset, fillSize, data);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(FILL_BUF);
		Serialise_vkCmdFillBuffer(localSerialiser, cmdBuffer, destBuffer, destOffset, fillSize, data);

		record->AddChunk(scope.Get());

		VkResourceRecord *buf = GetRecord(destBuffer);

		// mark buffer just as read, and memory behind as write & dirtied
		record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
		record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
		record->cmdInfo->dirtied.insert(buf->baseResource);
	}
}

bool WrappedVulkan::Serialise_vkCmdPushConstants(
	Serialiser*                                 localSerialiser,
	VkCmdBuffer                                 cmdBuffer,
	VkPipelineLayout                            layout,
	VkShaderStageFlags                          stageFlags,
	uint32_t                                    start,
	uint32_t                                    length,
	const void*                                 values)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, layid, GetResID(layout));
	SERIALISE_ELEMENT(VkShaderStageFlagBits, flags, (VkShaderStageFlagBits)stageFlags);
	SERIALISE_ELEMENT(uint32_t, s, start);
	SERIALISE_ELEMENT(uint32_t, len, length);
	SERIALISE_ELEMENT_BUF(byte *, vals, (byte *)values, (size_t)(len*4));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdPushConstants(Unwrap(cmdBuffer), Unwrap(layout), flags, s, len, vals);

			RDCASSERT(s+len < (uint32_t)ARRAY_COUNT(m_PartialReplayData.state.pushconsts));

			memcpy(m_PartialReplayData.state.pushconsts + s, vals, len);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdPushConstants(Unwrap(cmdBuffer), Unwrap(layout), flags, s, len, vals);
	}

	if(m_State < WRITING)
		SAFE_DELETE_ARRAY(vals);

	return true;
}

void WrappedVulkan::vkCmdPushConstants(
	VkCmdBuffer                                 cmdBuffer,
	VkPipelineLayout                            layout,
	VkShaderStageFlags                          stageFlags,
	uint32_t                                    start,
	uint32_t                                    length,
	const void*                                 values)
{
	ObjDisp(cmdBuffer)->CmdPushConstants(Unwrap(cmdBuffer), Unwrap(layout), stageFlags, start, length, values);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(PUSH_CONST);
		Serialise_vkCmdPushConstants(localSerialiser, cmdBuffer, layout, stageFlags, start, length, values);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdPipelineBarrier(
			Serialiser*                                 localSerialiser,
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
				imTrans.push_back(barrier);
			}
		}
	}
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), src, dest, region, (uint32_t)mems.size(), (const void **)&mems[0]);

			ResourceId cmd = GetResID(PartialCmdBuf());
			GetResourceManager()->RecordTransitions(m_BakedCmdBufferInfo[cmd].imgtransitions, m_ImageLayouts, (uint32_t)imTrans.size(), &imTrans[0]);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), src, dest, region, (uint32_t)mems.size(), (const void **)&mems[0]);
		
		ResourceId cmd = GetResID(cmdBuffer);
		GetResourceManager()->RecordTransitions(m_BakedCmdBufferInfo[cmd].imgtransitions, m_ImageLayouts, (uint32_t)imTrans.size(), &imTrans[0]);
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
		// conservatively request memory for worst case to avoid needing to iterate
		// twice to count
		byte *memory = GetTempMemory( ( sizeof(void*) + sizeof(VkImageMemoryBarrier) + sizeof(VkBufferMemoryBarrier) )*memBarrierCount);

		VkImageMemoryBarrier *im = (VkImageMemoryBarrier *)memory;
		VkBufferMemoryBarrier *buf = (VkBufferMemoryBarrier *)(im + memBarrierCount);

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

		ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), srcStageMask, destStageMask, byRegion, memBarrierCount, unwrappedBarriers);
	}

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(PIPELINE_BARRIER);
		Serialise_vkCmdPipelineBarrier(localSerialiser, cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);

		record->AddChunk(scope.Get());

		vector<VkImageMemoryBarrier> imTrans;

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkStructureType stype = ((VkGenericStruct *)ppMemBarriers[i])->sType;

			if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
				imTrans.push_back(*((VkImageMemoryBarrier *)ppMemBarriers[i]));
		}
		
		ResourceId cmd = GetResID(cmdBuffer);
		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			GetResourceManager()->RecordTransitions(GetRecord(cmdBuffer)->cmdInfo->imgtransitions, m_ImageLayouts, (uint32_t)imTrans.size(), &imTrans[0]);
		}
	}
}

bool WrappedVulkan::Serialise_vkCmdWriteTimestamp(
		Serialiser*                                 localSerialiser,
		VkCmdBuffer                                 cmdBuffer,
		VkTimestampType                             timestampType,
		VkBuffer                                    destBuffer,
		VkDeviceSize                                destOffset)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkTimestampType, type, timestampType);
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
	SERIALISE_ELEMENT(VkDeviceSize, offs, destOffset);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdWriteTimestamp(Unwrap(cmdBuffer), type, Unwrap(destBuffer), offs);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdWriteTimestamp(Unwrap(cmdBuffer), type, Unwrap(destBuffer), offs);
	}

	return true;
}

void WrappedVulkan::vkCmdWriteTimestamp(
		VkCmdBuffer                                 cmdBuffer,
		VkTimestampType                             timestampType,
		VkBuffer                                    destBuffer,
		VkDeviceSize                                destOffset)
{
	ObjDisp(cmdBuffer)->CmdWriteTimestamp(Unwrap(cmdBuffer), timestampType, Unwrap(destBuffer), destOffset);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(WRITE_TIMESTAMP);
		Serialise_vkCmdWriteTimestamp(localSerialiser, cmdBuffer, timestampType, destBuffer, destOffset);

		record->AddChunk(scope.Get());

		VkResourceRecord *buf = GetRecord(destBuffer);

		// mark buffer just as read, and memory behind as write & dirtied
		record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
		record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
		record->cmdInfo->dirtied.insert(buf->baseResource);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyQueryPoolResults(
		Serialiser*                                 localSerialiser,
		VkCmdBuffer                                 cmdBuffer,
		VkQueryPool                                 queryPool,
		uint32_t                                    startQuery,
		uint32_t                                    queryCount,
		VkBuffer                                    destBuffer,
		VkDeviceSize                                destOffset,
		VkDeviceSize                                destStride,
		VkQueryResultFlags                          flags)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queryPool));
	SERIALISE_ELEMENT(uint32_t, start, startQuery);
	SERIALISE_ELEMENT(uint32_t, count, queryCount);
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
	SERIALISE_ELEMENT(VkDeviceSize, offs, destOffset);
	SERIALISE_ELEMENT(VkDeviceSize, stride, destStride);
	SERIALISE_ELEMENT(VkQueryResultFlagBits, f, (VkQueryResultFlagBits)flags);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyQueryPoolResults(Unwrap(cmdBuffer), Unwrap(queryPool), start, count, Unwrap(destBuffer), offs, stride, f);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		queryPool = GetResourceManager()->GetLiveHandle<VkQueryPool>(qid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdCopyQueryPoolResults(Unwrap(cmdBuffer), Unwrap(queryPool), start, count, Unwrap(destBuffer), offs, stride, f);
	}

	return true;
}

void WrappedVulkan::vkCmdCopyQueryPoolResults(
		VkCmdBuffer                                 cmdBuffer,
		VkQueryPool                                 queryPool,
		uint32_t                                    startQuery,
		uint32_t                                    queryCount,
		VkBuffer                                    destBuffer,
		VkDeviceSize                                destOffset,
		VkDeviceSize                                destStride,
		VkQueryResultFlags                          flags)
{
	ObjDisp(cmdBuffer)->CmdCopyQueryPoolResults(Unwrap(cmdBuffer), Unwrap(queryPool), startQuery, queryCount, Unwrap(destBuffer), destOffset, destStride, flags);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_QUERY_RESULTS);
		Serialise_vkCmdCopyQueryPoolResults(localSerialiser, cmdBuffer, queryPool, startQuery, queryCount, destBuffer, destOffset, destStride, flags);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);

		VkResourceRecord *buf = GetRecord(destBuffer);

		// mark buffer just as read, and memory behind as write & dirtied
		record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
		record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
		record->cmdInfo->dirtied.insert(buf->baseResource);
	}
}

bool WrappedVulkan::Serialise_vkCmdBeginQuery(
		Serialiser*                                 localSerialiser,
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BEGIN_QUERY);
		Serialise_vkCmdBeginQuery(localSerialiser, cmdBuffer, queryPool, slot, flags);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdEndQuery(
		Serialiser*                                 localSerialiser,
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(END_QUERY);
		Serialise_vkCmdEndQuery(localSerialiser, cmdBuffer, queryPool, slot);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdResetQueryPool(
		Serialiser*                                 localSerialiser,
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(RESET_QUERY_POOL);
		Serialise_vkCmdResetQueryPool(localSerialiser, cmdBuffer, queryPool, startQuery, queryCount);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdDbgMarkerBegin(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(string, name, pMarker ? string(pMarker) : "");
	
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
		Serialise_vkCmdDbgMarkerBegin(localSerialiser, cmdBuffer, pMarker);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDbgMarkerEnd(Serialiser* localSerialiser, VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == READING && !m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.empty())
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

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(END_EVENT);
		Serialise_vkCmdDbgMarkerEnd(localSerialiser, cmdBuffer);

		record->AddChunk(scope.Get());
	}
}
