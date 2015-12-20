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

bool WrappedVulkan::Serialise_vkCmdDraw(
	Serialiser*    localSerialiser,
	VkCommandBuffer commandBuffer,
	uint32_t       vertexCount,
	uint32_t       instanceCount,
	uint32_t       firstVertex,
	uint32_t       firstInstance)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(uint32_t, vtxCount, vertexCount);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);
	SERIALISE_ELEMENT(uint32_t, firstVtx, firstVertex);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			if(m_DrawcallCallback) m_DrawcallCallback->PreDraw(m_RootEventID);

			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdDraw(Unwrap(commandBuffer), vtxCount, instCount, firstVtx, firstInst);

			if(m_DrawcallCallback) m_DrawcallCallback->PostDraw(m_RootEventID);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

		ObjDisp(commandBuffer)->CmdDraw(Unwrap(commandBuffer), vtxCount, instCount, firstVtx, firstInst);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(DRAW, desc);
			string name = "vkCmdDraw(" +
				ToStr::Get(vtxCount) + "," +
				ToStr::Get(instCount) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.numIndices = vtxCount;
			draw.numInstances = instCount;
			draw.indexOffset = 0;
			draw.vertexOffset = firstVtx;
			draw.instanceOffset = firstInst;

			draw.flags |= eDraw_Drawcall|eDraw_Instanced;

			AddDrawcall(draw, true);
		}
	}

	return true;
}

void WrappedVulkan::vkCmdDraw(
	VkCommandBuffer commandBuffer,
	uint32_t       vertexCount,
	uint32_t       instanceCount,
	uint32_t       firstVertex,
	uint32_t       firstInstance)
{
	ObjDisp(commandBuffer)->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex, firstInstance);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW);
		Serialise_vkCmdDraw(localSerialiser, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdBlitImage(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkFilter                                    filter)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(VkFilter, f, filter);
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageBlit, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions, f);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(commandBuffer)->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions, f);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(BLIT_IMG, desc);
			string name = "vkCmdBlitImage(" +
				ToStr::Get(srcid) + "," +
				ToStr::Get(dstid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Resolve;

			draw.copySource = srcid;
			draw.copyDestination = dstid;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdBlitImage(
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkFilter                                    filter)
{
	ObjDisp(commandBuffer)->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(BLIT_IMG);
		Serialise_vkCmdBlitImage(localSerialiser, commandBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);

		record->AddChunk(scope.Get());

		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
		record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
		record->cmdInfo->dirtied.insert(GetResID(destImage));
		if(GetRecord(srcImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
		if(GetRecord(destImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdResolveImage(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageResolve*                       pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageResolve, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(commandBuffer)->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(RESOLVE_IMG, desc);
			string name = "vkCmdResolveImage(" +
				ToStr::Get(srcid) + "," +
				ToStr::Get(dstid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Resolve;

			draw.copySource = srcid;
			draw.copyDestination = dstid;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdResolveImage(
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageResolve*                       pRegions)
{
	ObjDisp(commandBuffer)->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(RESOLVE_IMG);
		Serialise_vkCmdResolveImage(localSerialiser, commandBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());

		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
		record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
		record->cmdInfo->dirtied.insert(GetResID(destImage));
		if(GetRecord(srcImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
		if(GetRecord(destImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyImage(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(commandBuffer)->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(RESOLVE_IMG, desc);
			string name = "vkCmdResolveImage(" +
				ToStr::Get(srcid) + "," +
				ToStr::Get(dstid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Resolve;

			draw.copySource = srcid;
			draw.copyDestination = dstid;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyImage(
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions)
{
	ObjDisp(commandBuffer)->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);
		
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_IMG);
		Serialise_vkCmdCopyImage(localSerialiser, commandBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
		record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
		record->cmdInfo->dirtied.insert(GetResID(destImage));
		if(GetRecord(srcImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
		if(GetRecord(destImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBufferToImage(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(srcBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(destImage));
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(VkImageLayout, layout, destImageLayout);

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage), layout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(commandBuffer)->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage), layout, count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(COPY_BUF2IMG, desc);
			string name = "vkCmdCopyBufferToImage(" +
				ToStr::Get(bufid) + "," +
				ToStr::Get(imgid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Copy;

			draw.copySource = bufid;
			draw.copyDestination = imgid;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyBufferToImage(
			VkCommandBuffer                                 commandBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions)
{
	ObjDisp(commandBuffer)->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_BUF2IMG);
		Serialise_vkCmdCopyBufferToImage(localSerialiser, commandBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());

		record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(srcBuffer)->baseResource, eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
		record->MarkResourceFrameReferenced(GetRecord(destImage)->baseResource, eFrameRef_Read);
		record->cmdInfo->dirtied.insert(GetResID(destImage));
		if(GetRecord(srcBuffer)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(srcBuffer)->sparseInfo);
		if(GetRecord(destImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(destImage)->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyImageToBuffer(
		Serialiser*                                 localSerialiser,
    VkCommandBuffer                                 commandBuffer,
		VkImage                                     srcImage,
		VkImageLayout                               srcImageLayout,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferImageCopy*                    pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(srcImage));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(VkImageLayout, layout, srcImageLayout);

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer), count, regions);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(commandBuffer)->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer), count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(COPY_BUF2IMG, desc);
			string name = "vkCmdCopyImageToBuffer(" +
				ToStr::Get(imgid) + "," +
				ToStr::Get(bufid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Copy;

			draw.copySource = imgid;
			draw.copyDestination = bufid;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyImageToBuffer(
    VkCommandBuffer                                 commandBuffer,
		VkImage                                     srcImage,
		VkImageLayout                               srcImageLayout,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferImageCopy*                    pRegions)
{
	ObjDisp(commandBuffer)->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destBuffer), regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_IMG2BUF);
		Serialise_vkCmdCopyImageToBuffer(localSerialiser, commandBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(srcImage)->baseResource, eFrameRef_Read);

		VkResourceRecord *buf = GetRecord(destBuffer);

		// mark buffer just as read, and memory behind as write & dirtied
		record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
		record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
		if(buf->baseResource != ResourceId())
			record->cmdInfo->dirtied.insert(buf->baseResource);
		if(GetRecord(srcImage)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(srcImage)->sparseInfo);
		if(buf->sparseInfo)
			record->cmdInfo->sparse.insert(buf->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBuffer(
		Serialiser*                                 localSerialiser,
    VkCommandBuffer                                 commandBuffer,
		VkBuffer                                    srcBuffer,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferCopy*                         pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcBuffer));
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destBuffer));
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

		ObjDisp(commandBuffer)->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(COPY_BUF, desc);
			string name = "vkCmdCopyBuffer(" +
				ToStr::Get(srcid) + "," +
				ToStr::Get(dstid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Copy;

			draw.copySource = srcid;
			draw.copyDestination = dstid;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyBuffer(
			VkCommandBuffer                                 commandBuffer,
			VkBuffer                                    srcBuffer,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferCopy*                         pRegions)
{
	ObjDisp(commandBuffer)->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_BUF);
		Serialise_vkCmdCopyBuffer(localSerialiser, commandBuffer, srcBuffer, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetRecord(srcBuffer)->baseResource, eFrameRef_Read);

		VkResourceRecord *buf = GetRecord(destBuffer);

		// mark buffer just as read, and memory behind as write & dirtied
		record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
		record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Write);
		if(buf->baseResource != ResourceId())
			record->cmdInfo->dirtied.insert(buf->baseResource);
		if(GetRecord(srcBuffer)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(srcBuffer)->sparseInfo);
		if(buf->sparseInfo)
			record->cmdInfo->sparse.insert(buf->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearColorImage(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
	SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
	SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State == EXECUTING)
	{
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), layout, &col, count, ranges);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(commandBuffer)->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), layout, &col, count, ranges);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(CLEAR_COLOR, desc);
			string name = "vkCmdClearColorImage(" +
				ToStr::Get(col) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Clear|eDraw_ClearColour;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(ranges);

	return true;
}

void WrappedVulkan::vkCmdClearColorImage(
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	ObjDisp(commandBuffer)->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, pColor, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
		Serialise_vkCmdClearColorImage(localSerialiser, commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
		record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
		if(GetRecord(image)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(image)->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearDepthStencilImage(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearDepthStencilValue*             pDepthStencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
	SERIALISE_ELEMENT(VkImageLayout, l, imageLayout);
	SERIALISE_ELEMENT(VkClearDepthStencilValue, ds, *pDepthStencil);
	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), l, &ds, count, ranges);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(commandBuffer)->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), l, &ds, count, ranges);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(CLEAR_DEPTHSTENCIL, desc);
			string name = "vkCmdClearDepthStencilImage(" +
				ToStr::Get(ds.depth) + "," +
				ToStr::Get(ds.stencil) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Clear|eDraw_ClearDepthStencil;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(ranges);

	return true;
}

void WrappedVulkan::vkCmdClearDepthStencilImage(
			VkCommandBuffer                                 commandBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearDepthStencilValue*             pDepthStencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	ObjDisp(commandBuffer)->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, pDepthStencil, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL);
		Serialise_vkCmdClearDepthStencilImage(localSerialiser, commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
		record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
		if(GetRecord(image)->sparseInfo)
			record->cmdInfo->sparse.insert(GetRecord(image)->sparseInfo);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearAttachments(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                             commandBuffer,
			uint32_t                                    attachmentCount,
			const VkClearAttachment*                    pAttachments,
			uint32_t                                    rectCount,
			const VkClearRect*                          pRects)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	SERIALISE_ELEMENT(uint32_t, acount, attachmentCount);
	SERIALISE_ELEMENT_ARR(VkClearAttachment, atts, pAttachments, acount);

	SERIALISE_ELEMENT(uint32_t, rcount, rectCount);
	SERIALISE_ELEMENT_ARR(VkClearRect, rects, pRects, rcount);
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdClearAttachments(Unwrap(commandBuffer), acount, atts, rcount, rects);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

		ObjDisp(commandBuffer)->CmdClearAttachments(Unwrap(commandBuffer), acount, atts, rcount, rects);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(CLEAR_ATTACH, desc);
			string name = "vkCmdClearAttachments(";
			for(uint32_t a=0; a < acount; a++)
				name += ToStr::Get(atts[a]);
			name += ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Clear;
			for(uint32_t a=0; a < acount; a++)
			{
				if(atts[a].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
					draw.flags |= eDraw_ClearColour;
				if(atts[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
					draw.flags |= eDraw_ClearDepthStencil;
			}

			AddDrawcall(draw, true);
		}
	}
	
	SAFE_DELETE_ARRAY(atts);
	SAFE_DELETE_ARRAY(rects);

	return true;
}

void WrappedVulkan::vkCmdClearAttachments(
			VkCommandBuffer                             commandBuffer,
			uint32_t                                    attachmentCount,
			const VkClearAttachment*                    pAttachments,
			uint32_t                                    rectCount,
			const VkClearRect*                          pRects)
{
	ObjDisp(commandBuffer)->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_ATTACH);
		Serialise_vkCmdClearAttachments(localSerialiser, commandBuffer, attachmentCount, pAttachments, rectCount, pRects);

		record->AddChunk(scope.Get());

		// image/attachments are referenced when the render pass is started and the framebuffer is bound.
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexed(
	Serialiser*    localSerialiser,
	VkCommandBuffer commandBuffer,
	uint32_t       indexCount,
	uint32_t       instanceCount,
	uint32_t       firstIndex,
	int32_t        vertexOffset,
	uint32_t       firstInstance)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(uint32_t, idxCount, indexCount);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);
	SERIALISE_ELEMENT(uint32_t, firstIdx, firstIndex);
	SERIALISE_ELEMENT(int32_t,  vtxOffs, vertexOffset);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			if(m_DrawcallCallback) m_DrawcallCallback->PreDraw(m_RootEventID);

			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdDrawIndexed(Unwrap(commandBuffer), idxCount, instCount, firstIdx, vtxOffs, firstInst);

			if(m_DrawcallCallback) m_DrawcallCallback->PostDraw(m_RootEventID);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

		ObjDisp(commandBuffer)->CmdDrawIndexed(Unwrap(commandBuffer), idxCount, instCount, firstIdx, vtxOffs, firstInst);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(DRAW_INDEXED, desc);
			string name = "vkCmdDrawIndexed(" +
				ToStr::Get(idxCount) + "," +
				ToStr::Get(instCount) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.numIndices = idxCount;
			draw.numInstances = instCount;
			draw.indexOffset = firstIdx;
			draw.vertexOffset = vtxOffs;
			draw.instanceOffset = firstInst;

			draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer|eDraw_Instanced;

			AddDrawcall(draw, true);
		}
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndexed(
	VkCommandBuffer commandBuffer,
	uint32_t       indexCount,
	uint32_t       instanceCount,
	uint32_t       firstIndex,
	int32_t        vertexOffset,
	uint32_t       firstInstance)
{
	ObjDisp(commandBuffer)->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED);
		Serialise_vkCmdDrawIndexed(localSerialiser, commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndirect(
		Serialiser*                                 localSerialiser,
		VkCommandBuffer                                 commandBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT(uint32_t, strd, stride);

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndirect(
		VkCommandBuffer                                 commandBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	ObjDisp(commandBuffer)->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW_INDIRECT);
		Serialise_vkCmdDrawIndirect(localSerialiser, commandBuffer, buffer, offset, count, stride);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirect(
		Serialiser*                                 localSerialiser,
		VkCommandBuffer                                 commandBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT(uint32_t, strd, stride);

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(commandBuffer)->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs, cnt, strd);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirect(
		VkCommandBuffer                                 commandBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	ObjDisp(commandBuffer)->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INDIRECT);
		Serialise_vkCmdDrawIndexedIndirect(localSerialiser, commandBuffer, buffer, offset, count, stride);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDispatch(
	Serialiser*    localSerialiser,
	VkCommandBuffer commandBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(uint32_t, X, x);
	SERIALISE_ELEMENT(uint32_t, Y, y);
	SERIALISE_ELEMENT(uint32_t, Z, z);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

		ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), X, Y, Z);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatch(
	VkCommandBuffer commandBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DISPATCH);
		Serialise_vkCmdDispatch(localSerialiser, commandBuffer, x, y, z);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDispatchIndirect(
			Serialiser*                                 localSerialiser,
			VkCommandBuffer                                 commandBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(commandBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			commandBuffer = PartialCmdBuf();
			ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs);
		}
	}
	else if(m_State == READING)
	{
		commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offs);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(
			VkCommandBuffer                                 commandBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(commandBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DISPATCH_INDIRECT);
		Serialise_vkCmdDispatchIndirect(localSerialiser, commandBuffer, buffer, offset);

		record->AddChunk(scope.Get());
	}
}
