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
	VkCmdBuffer cmdBuffer,
	uint32_t       firstVertex,
	uint32_t       vertexCount,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, firstVtx, firstVertex);
	SERIALISE_ELEMENT(uint32_t, vtxCount, vertexCount);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDraw(Unwrap(cmdBuffer), firstVtx, vtxCount, firstInst, instCount);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdDraw(Unwrap(cmdBuffer), firstVtx, vtxCount, firstInst, instCount);

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

			draw.flags |= eDraw_Drawcall;

			AddDrawcall(draw, true);
		}
	}

	return true;
}

void WrappedVulkan::vkCmdDraw(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstVertex,
	uint32_t       vertexCount,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	ObjDisp(cmdBuffer)->CmdDraw(Unwrap(cmdBuffer), firstVertex, vertexCount, firstInstance, instanceCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW);
		Serialise_vkCmdDraw(localSerialiser, cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdBlitImage(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkTexFilter                                 filter)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(VkTexFilter, f, filter);
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageBlit, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBlitImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions, f);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(cmdBuffer)->CmdBlitImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions, f);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdBlitImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkTexFilter                                 filter)
{
	ObjDisp(cmdBuffer)->CmdBlitImage(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();
		
		SCOPED_SERIALISE_CONTEXT(BLIT_IMG);
		Serialise_vkCmdBlitImage(localSerialiser, cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResID(destImage));
		{
			VkResourceRecord *im = GetRecord(destImage);
			if(im->GetMemoryRecord())
				record->dirtied.insert(im->GetMemoryRecord()->GetResourceID());
		}
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdResolveImage(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageResolve*                       pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdResolveImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(cmdBuffer)->CmdResolveImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdResolveImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageResolve*                       pRegions)
{
	ObjDisp(cmdBuffer)->CmdResolveImage(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(RESOLVE_IMG);
		Serialise_vkCmdResolveImage(localSerialiser, cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResID(destImage));
		{
			VkResourceRecord *im = GetRecord(destImage);
			if(im->GetMemoryRecord())
				record->dirtied.insert(im->GetMemoryRecord()->GetResourceID());
		}
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyImage(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(cmdBuffer)->CmdCopyImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions)
{
	ObjDisp(cmdBuffer)->CmdCopyImage(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);
		
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_IMG);
		Serialise_vkCmdCopyImage(localSerialiser, cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);

		record->dirtied.insert(GetResID(destImage));
		{
			VkResourceRecord *im = GetRecord(destImage);
			if(im->GetMemoryRecord())
				record->dirtied.insert(im->GetMemoryRecord()->GetResourceID());
		}
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBufferToImage(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(srcBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(destImage));
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(cmdBuffer)->CmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(COPY_BUF2IMG, desc);
			string name = "vkCmdCopyBufferToImage(" +
				ToStr::Get(bufid) + "," +
				ToStr::Get(imgid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Copy;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyBufferToImage(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions)
{
	ObjDisp(cmdBuffer)->CmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_BUF2IMG);
		Serialise_vkCmdCopyBufferToImage(localSerialiser, cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResID(destImage));
		{
			VkResourceRecord *im = GetRecord(destImage);
			if(im->GetMemoryRecord())
				record->dirtied.insert(im->GetMemoryRecord()->GetResourceID());
		}
		record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyImageToBuffer(
		Serialiser*                                 localSerialiser,
    VkCmdBuffer                                 cmdBuffer,
		VkImage                                     srcImage,
		VkImageLayout                               srcImageLayout,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferImageCopy*                    pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyImageToBuffer(Unwrap(cmdBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer), count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdCopyImageToBuffer(Unwrap(cmdBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer), count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyImageToBuffer(
    VkCmdBuffer                                 cmdBuffer,
		VkImage                                     srcImage,
		VkImageLayout                               srcImageLayout,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferImageCopy*                    pRegions)
{
	ObjDisp(cmdBuffer)->CmdCopyImageToBuffer(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destBuffer), regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_IMG2BUF);
		Serialise_vkCmdCopyImageToBuffer(localSerialiser, cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destBuffer), eFrameRef_Write);

		// Don't dirty the buffer, just the memory behind it.
		{
			VkResourceRecord *buf = GetRecord(destBuffer);
			if(buf->GetMemoryRecord())
				record->dirtied.insert(buf->GetMemoryRecord()->GetResourceID());
		}
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBuffer(
		Serialiser*                                 localSerialiser,
    VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    srcBuffer,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferCopy*                         pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyBuffer(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

		ObjDisp(cmdBuffer)->CmdCopyBuffer(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(COPY_BUF, desc);
			string name = "vkCmdCopyBuffer(" +
				ToStr::Get(srcid) + "," +
				ToStr::Get(dstid) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Copy;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyBuffer(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferCopy*                         pRegions)
{
	ObjDisp(cmdBuffer)->CmdCopyBuffer(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(COPY_BUF);
		Serialise_vkCmdCopyBuffer(localSerialiser, cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destBuffer), eFrameRef_Write);
		
		// Don't dirty the buffer, just the memory behind it.
		{
			VkResourceRecord *buf = GetRecord(destBuffer);
			if(buf->GetMemoryRecord())
				record->dirtied.insert(buf->GetMemoryRecord()->GetResourceID());
		}
	}
}

bool WrappedVulkan::Serialise_vkCmdClearColorImage(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), layout, &col, count, ranges);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(cmdBuffer)->CmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), layout, &col, count, ranges);
	}

	SAFE_DELETE_ARRAY(ranges);

	return true;
}

void WrappedVulkan::vkCmdClearColorImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	ObjDisp(cmdBuffer)->CmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), imageLayout, pColor, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
		Serialise_vkCmdClearColorImage(localSerialiser, cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearDepthStencilImage(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
	SERIALISE_ELEMENT(VkImageLayout, l, imageLayout);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(byte, s, stencil);
	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearDepthStencilImage(Unwrap(cmdBuffer), Unwrap(image), l, d, s, count, ranges);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(cmdBuffer)->CmdClearDepthStencilImage(Unwrap(cmdBuffer), Unwrap(image), l, d, s, count, ranges);
	}

	SAFE_DELETE_ARRAY(ranges);

	return true;
}

void WrappedVulkan::vkCmdClearDepthStencilImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	ObjDisp(cmdBuffer)->CmdClearDepthStencilImage(Unwrap(cmdBuffer), Unwrap(image), imageLayout, depth, stencil, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL);
		Serialise_vkCmdClearDepthStencilImage(localSerialiser, cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearColorAttachment(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    colorAttachment,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, att, colorAttachment);
	SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
	SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkRect3D, rects, pRects, count);
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearColorAttachment(Unwrap(cmdBuffer), att, layout, &col, count, rects);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdClearColorAttachment(Unwrap(cmdBuffer), att, layout, &col, count, rects);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(CLEAR_COLOR_ATTACH, desc);
			string name = "vkCmdClearColorAttachment(" +
				ToStr::Get(att) + "," +
				ToStr::Get(col) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Clear|eDraw_ClearColour;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(rects);

	return true;
}

void WrappedVulkan::vkCmdClearColorAttachment(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    colorAttachment,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	ObjDisp(cmdBuffer)->CmdClearColorAttachment(Unwrap(cmdBuffer), colorAttachment, imageLayout, pColor, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR_ATTACH);
		Serialise_vkCmdClearColorAttachment(localSerialiser, cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);

		record->AddChunk(scope.Get());
		// VKTODOHIGH mark referenced the image under the attachment
		//record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearDepthStencilAttachment(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkImageAspectFlags                          imageAspectMask,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkImageAspectFlags, asp, imageAspectMask);
	SERIALISE_ELEMENT(VkImageLayout, lay, imageLayout);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(byte, s, stencil);
	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkRect3D, rects, pRects, count);
	
	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearDepthStencilAttachment(Unwrap(cmdBuffer), asp, lay, d, s, count, rects);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdClearDepthStencilAttachment(Unwrap(cmdBuffer), asp, lay, d, s, count, rects);

		const string desc = localSerialiser->GetDebugStr();

		{
			AddEvent(CLEAR_DEPTHSTENCIL_ATTACH, desc);
			string name = "vkCmdClearDepthStencilAttachment(" +
				ToStr::Get(d) + "," +
				ToStr::Get(s) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Clear|eDraw_ClearDepthStencil;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(rects);

	return true;
}

void WrappedVulkan::vkCmdClearDepthStencilAttachment(
			VkCmdBuffer                                 cmdBuffer,
			VkImageAspectFlags                          imageAspectMask,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	ObjDisp(cmdBuffer)->CmdClearDepthStencilAttachment(Unwrap(cmdBuffer), imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL_ATTACH);
		Serialise_vkCmdClearDepthStencilAttachment(localSerialiser, cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);

		record->AddChunk(scope.Get());
		// VKTODOHIGH mark referenced the image under the attachment
		//record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexed(
	Serialiser*    localSerialiser,
	VkCmdBuffer cmdBuffer,
	uint32_t       firstIndex,
	uint32_t       indexCount,
	int32_t        vertexOffset,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, firstIdx, firstIndex);
	SERIALISE_ELEMENT(uint32_t, idxCount, indexCount);
	SERIALISE_ELEMENT(int32_t, vtxOffs, vertexOffset);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDrawIndexed(Unwrap(cmdBuffer), firstIdx, idxCount, vtxOffs, firstInst, instCount);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdDrawIndexed(Unwrap(cmdBuffer), firstIdx, idxCount, vtxOffs, firstInst, instCount);

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

			draw.flags |= eDraw_Drawcall|eDraw_UseIBuffer;

			AddDrawcall(draw, true);
		}
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndexed(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstIndex,
	uint32_t       indexCount,
	int32_t        vertexOffset,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	ObjDisp(cmdBuffer)->CmdDrawIndexed(Unwrap(cmdBuffer), firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED);
		Serialise_vkCmdDrawIndexed(localSerialiser, cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndirect(
		Serialiser*                                 localSerialiser,
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDrawIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdDrawIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndirect(
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	ObjDisp(cmdBuffer)->CmdDrawIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW_INDIRECT);
		Serialise_vkCmdDrawIndirect(localSerialiser, cmdBuffer, buffer, offset, count, stride);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirect(
		Serialiser*                                 localSerialiser,
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDrawIndexedIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdDrawIndexedIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirect(
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	ObjDisp(cmdBuffer)->CmdDrawIndexedIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INDIRECT);
		Serialise_vkCmdDrawIndexedIndirect(localSerialiser, cmdBuffer, buffer, offset, count, stride);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDispatch(
	Serialiser*    localSerialiser,
	VkCmdBuffer cmdBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, X, x);
	SERIALISE_ELEMENT(uint32_t, Y, y);
	SERIALISE_ELEMENT(uint32_t, Z, z);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDispatch(Unwrap(cmdBuffer), x, y, z);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdDispatch(Unwrap(cmdBuffer), X, Y, Z);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatch(
	VkCmdBuffer cmdBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	ObjDisp(cmdBuffer)->CmdDispatch(Unwrap(cmdBuffer), x, y, z);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DISPATCH);
		Serialise_vkCmdDispatch(localSerialiser, cmdBuffer, x, y, z);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDispatchIndirect(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDispatchIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdDispatchIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	ObjDisp(cmdBuffer)->CmdDispatchIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offset);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DISPATCH_INDIRECT);
		Serialise_vkCmdDispatchIndirect(localSerialiser, cmdBuffer, buffer, offset);

		record->AddChunk(scope.Get());
	}
}
