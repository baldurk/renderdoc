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

// VKTODOHIGH we are assuming in all the initial state handling that the image is VK_IMAGE_ASPECT_COLOR

// VKTODOLOW there's a lot of duplicated code in this file for creating a buffer to do
// a memory copy and saving to disk.
// VKTODOLOW SerialiseComplexArray not having the ability to serialise into an in-memory
// array means some redundant copies.
// VKTODOLOW The code pattern for creating a few contiguous arrays all in one
// AllocAlignedBuffer for the initial contents buffer is ugly.

struct MemIDOffset
{
	ResourceId memId;
	VkDeviceSize memOffs;
};

template<>
void Serialiser::Serialise(const char *name, MemIDOffset &el)
{
	Serialise("memId", el.memId);
	Serialise("memOffs", el.memOffs);
}

struct SparseBufferInitState
{
	uint32_t numBinds;
	VkSparseMemoryBindInfo *binds;

	uint32_t numUniqueMems;
	MemIDOffset *memDataOffs;

	VkDeviceSize totalSize;
};

struct SparseImageInitState
{
	uint32_t opaqueCount;
	VkSparseMemoryBindInfo *opaque;

	VkExtent3D imgdim; // in pages
	VkExtent3D pagedim;
	uint32_t pageCount[VK_IMAGE_ASPECT_NUM];

	// available on capture - filled out in Prepare_SparseInitialState and serialised to disk
	MemIDOffset *pages[VK_IMAGE_ASPECT_NUM];

	// available on replay - filled out in the READING path of Serialise_SparseInitialState
	VkSparseImageMemoryBindInfo *pageBinds[VK_IMAGE_ASPECT_NUM];

	uint32_t numUniqueMems;
	MemIDOffset *memDataOffs;

	VkDeviceSize totalSize;
};

static bool operator <(const VkDeviceMemory &a, const VkDeviceMemory &b)
{
	return a.handle < b.handle;
}

bool WrappedVulkan::Prepare_SparseInitialState(WrappedVkBuffer *buf)
{
	ResourceId id = buf->id;
	
	// VKTODOLOW this is a bit conservative, as we save the whole memory object rather than just the bound range.
	map<VkDeviceMemory, VkDeviceSize> boundMems;

	// value will be filled out later once all memories are added
	for(size_t i=0; i < buf->record->sparseInfo->opaquemappings.size(); i++)
		boundMems[buf->record->sparseInfo->opaquemappings[i].mem] = 0;
	
	uint32_t numElems = (uint32_t)buf->record->sparseInfo->opaquemappings.size();
	
	SparseBufferInitState *info = (SparseBufferInitState *)Serialiser::AllocAlignedBuffer(sizeof(SparseBufferInitState) +
		sizeof(VkSparseMemoryBindInfo)*numElems +
		sizeof(MemIDOffset)*boundMems.size());

	VkSparseMemoryBindInfo *binds = (VkSparseMemoryBindInfo *)(info + 1);
	MemIDOffset *memDataOffs = (MemIDOffset *)(binds + numElems);

	info->numBinds = numElems;
	info->numUniqueMems = (uint32_t)boundMems.size();
	info->memDataOffs = memDataOffs;
	info->binds = binds;

	memcpy(info, &buf->record->sparseInfo->opaquemappings[0], sizeof(VkSparseMemoryBindInfo)*numElems);

	VkDevice d = GetDev();
	// VKTODOLOW ideally the prepares could be batched up
	// a bit more - maybe not all in one command buffer, but
	// at least more than one each
	VkCmdBuffer cmd = GetNextCmd();
	
	VkBufferCreateInfo bufInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
		0, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	};

	uint32_t i=0;
	for(auto it=boundMems.begin(); it != boundMems.end(); ++it)
	{
		// store offset
		it->second = bufInfo.size;

		memDataOffs[i].memId = GetResID(it->first);
		memDataOffs[i].memOffs = bufInfo.size;

		// increase size
		bufInfo.size += (VkDeviceSize)GetRecord(it->first)->Length;
	}

	info->totalSize = bufInfo.size;

	VkDeviceMemory readbackmem = VK_NULL_HANDLE;

	// since these are very short lived, they are not wrapped
	VkBuffer dstBuf;

	VkResult vkr = VK_SUCCESS;

	vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
	RDCASSERT(vkr == VK_SUCCESS);
		
	VkMemoryRequirements mrq = { 0 };

	vkr = ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);
	RDCASSERT(vkr == VK_SUCCESS);

	VkMemoryAllocInfo allocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
		bufInfo.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
	};

	allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

	vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &readbackmem);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

	vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
	RDCASSERT(vkr == VK_SUCCESS);

	vector<VkBuffer> bufdeletes;
	bufdeletes.push_back(dstBuf);
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	// copy all of the bound memory objects
	for(auto it=boundMems.begin(); it != boundMems.end(); ++it)
	{
		VkBuffer srcBuf;

		bufInfo.size = (VkDeviceSize)GetRecord(it->first)->Length;
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &srcBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, Unwrap(it->first), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		// copy srcbuf into its area in dstbuf
		VkBufferCopy region = { 0, it->second, bufInfo.size };

		ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

		bufdeletes.push_back(srcBuf);
	}

	vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOLOW would be nice to store up all these buffers so that
	// we don't have to submit & flush here before destroying, but
	// instead could submit all cmds, then flush once, then destroy
	// buffers. (or even not flush at all until capture is over)
	SubmitCmds();
	FlushQ();

	for(size_t i=0; i < bufdeletes.size(); i++)
		ObjDisp(d)->DestroyBuffer(Unwrap(d), bufdeletes[i]);

	GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), 0, (byte *)info));

	return true;
}

bool WrappedVulkan::Prepare_SparseInitialState(WrappedVkImage *im)
{
	ResourceId id = im->id;
	
	SparseMapping *sparse = im->record->sparseInfo;
	
	// VKTODOLOW this is a bit conservative, as we save the whole memory object rather than just the bound range.
	map<VkDeviceMemory, VkDeviceSize> boundMems;

	// value will be filled out later once all memories are added
	for(size_t i=0; i < sparse->opaquemappings.size(); i++)
		boundMems[sparse->opaquemappings[i].mem] = 0;
	
	uint32_t pagePerAspect = sparse->imgdim.width*sparse->imgdim.height*sparse->imgdim.depth;

	for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
	{
		if(sparse->pages[a])
		{
			for(uint32_t i=0; i < pagePerAspect; i++)
				if(sparse->pages[a][i].first != VK_NULL_HANDLE)
					boundMems[sparse->pages[a][i].first] = 0;
		}
	}
	
	uint32_t totalPageCount = 0;
	for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
		totalPageCount += sparse->pages[a] ? pagePerAspect : 0;

	uint32_t opaqueCount = (uint32_t)sparse->opaquemappings.size();

	byte *blob = Serialiser::AllocAlignedBuffer(sizeof(SparseImageInitState) +
		sizeof(VkSparseMemoryBindInfo)*opaqueCount +
		sizeof(MemIDOffset)*totalPageCount +
		sizeof(MemIDOffset)*boundMems.size());

	SparseImageInitState *state = (SparseImageInitState *)blob;
	VkSparseMemoryBindInfo *opaque = (VkSparseMemoryBindInfo *)(state + 1);
	MemIDOffset *pages = (MemIDOffset *)(opaque + opaqueCount);
	MemIDOffset *memDataOffs = (MemIDOffset *)(pages + totalPageCount);

	state->opaque = opaque;
	state->opaqueCount = opaqueCount;
	state->pagedim = sparse->pagedim;
	state->imgdim = sparse->imgdim;
	state->numUniqueMems = (uint32_t)boundMems.size();
	state->memDataOffs = memDataOffs;

	if(opaqueCount > 0)
		memcpy(opaque, &sparse->opaquemappings[0], sizeof(VkSparseMemoryBindInfo)*opaqueCount);
	
	for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
	{
		state->pageCount[a] = (sparse->pages[a] ? pagePerAspect : 0);
		
		if(state->pageCount[a] != 0)
		{
			state->pages[a] = pages;

			for(uint32_t i=0; i < pagePerAspect; i++)
			{
				state->pages[a][i].memId = GetResID(sparse->pages[a][i].first);
				state->pages[a][i].memOffs = sparse->pages[a][i].second;
			}

			pages += pagePerAspect;
		}
		else
		{
			state->pages[a] = NULL;
		}
	}
	
	VkDevice d = GetDev();
	// VKTODOLOW ideally the prepares could be batched up
	// a bit more - maybe not all in one command buffer, but
	// at least more than one each
	VkCmdBuffer cmd = GetNextCmd();
	
	VkBufferCreateInfo bufInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
		0, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	};

	uint32_t i=0;
	for(auto it=boundMems.begin(); it != boundMems.end(); ++it)
	{
		// store offset
		it->second = bufInfo.size;

		memDataOffs[i].memId = GetResID(it->first);
		memDataOffs[i].memOffs = bufInfo.size;

		// increase size
		bufInfo.size += (VkDeviceSize)GetRecord(it->first)->Length;
	}

	state->totalSize = bufInfo.size;

	VkDeviceMemory readbackmem = VK_NULL_HANDLE;

	// since these are very short lived, they are not wrapped
	VkBuffer dstBuf;

	VkResult vkr = VK_SUCCESS;

	vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
	RDCASSERT(vkr == VK_SUCCESS);
		
	VkMemoryRequirements mrq = { 0 };

	vkr = ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);
	RDCASSERT(vkr == VK_SUCCESS);

	VkMemoryAllocInfo allocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
		bufInfo.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
	};

	allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

	vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &readbackmem);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

	vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
	RDCASSERT(vkr == VK_SUCCESS);

	vector<VkBuffer> bufdeletes;
	bufdeletes.push_back(dstBuf);
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	// copy all of the bound memory objects
	for(auto it=boundMems.begin(); it != boundMems.end(); ++it)
	{
		VkBuffer srcBuf;

		bufInfo.size = (VkDeviceSize)GetRecord(it->first)->Length;
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &srcBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, Unwrap(it->first), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		// copy srcbuf into its area in dstbuf
		VkBufferCopy region = { 0, it->second, bufInfo.size };

		ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

		bufdeletes.push_back(srcBuf);
	}

	vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOLOW would be nice to store up all these buffers so that
	// we don't have to submit & flush here before destroying, but
	// instead could submit all cmds, then flush once, then destroy
	// buffers. (or even not flush at all until capture is over)
	SubmitCmds();
	FlushQ();

	for(size_t i=0; i < bufdeletes.size(); i++)
		ObjDisp(d)->DestroyBuffer(Unwrap(d), bufdeletes[i]);

	GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), 0, (byte *)blob));

	return true;
}

bool WrappedVulkan::Serialise_SparseInitialState(ResourceId id, WrappedVkBuffer *buf, VulkanResourceManager::InitialContentData contents)
{
	if(m_State >= WRITING)
	{
		SparseBufferInitState *info = (SparseBufferInitState *)contents.blob;

		m_pSerialiser->Serialise("numBinds", info->numBinds);
		m_pSerialiser->Serialise("numUniqueMems", info->numUniqueMems);
		
		if(info->numBinds > 0)
			m_pSerialiser->SerialiseComplexArray("binds", info->binds, info->numBinds);
		
		if(info->numUniqueMems > 0)
			m_pSerialiser->SerialiseComplexArray("mems", info->memDataOffs, info->numUniqueMems);

		VkDevice d = GetDev();
		
		byte *ptr = NULL;
		ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource), 0, 0, 0, (void **)&ptr);

		size_t dataSize = (size_t)info->totalSize;

		m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

		ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource));
	}
	else
	{
		uint32_t numBinds = 0;
		uint32_t numUniqueMems = 0;

		m_pSerialiser->Serialise("numBinds", numBinds);
		m_pSerialiser->Serialise("numUniqueMems", numUniqueMems);
		
		SparseBufferInitState *info = (SparseBufferInitState *)Serialiser::AllocAlignedBuffer(sizeof(SparseBufferInitState) +
			sizeof(VkSparseMemoryBindInfo)*numBinds +
			sizeof(MemIDOffset)*numUniqueMems);
		
		VkSparseMemoryBindInfo *binds = (VkSparseMemoryBindInfo *)(info + 1);
		MemIDOffset *memDataOffs = (MemIDOffset *)(binds + numBinds);

		info->numBinds = numBinds;
		info->numUniqueMems = numUniqueMems;
		info->binds = binds;
		info->memDataOffs = memDataOffs;

		if(info->numBinds > 0)
		{
			VkSparseMemoryBindInfo *b = NULL;
			m_pSerialiser->SerialiseComplexArray("binds", b, numBinds);
			memcpy(info->binds, b, sizeof(VkSparseMemoryBindInfo)*numBinds);
			delete[] b;
		}
		else
		{
			info->binds = NULL;
		}

		if(info->numUniqueMems > 0)
		{
			MemIDOffset *m = NULL;
			m_pSerialiser->SerialiseComplexArray("mems", m, numUniqueMems);
			memcpy(info->memDataOffs, m, sizeof(MemIDOffset)*numUniqueMems);
			delete[] m;
		}
		else
		{
			info->numUniqueMems = NULL;
		}
		
		byte *data = NULL;
		size_t dataSize = 0;
		m_pSerialiser->SerialiseBuffer("data", data, dataSize);

		info->totalSize = (VkDeviceSize)dataSize;

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();

		VkDeviceMemory mem = VK_NULL_HANDLE;

		// VKTODOMED should get mem requirements for buffer - copy might enforce
		// some restrictions?
		VkMemoryRequirements mrq = { dataSize, 16, ~0U };

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			dataSize, GetUploadMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), mem);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			dataSize, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		VkBuffer buf;

		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &buf);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), buf);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		byte *ptr = NULL;
		ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, 0, 0, (void **)&ptr);

		// VKTODOLOW could deserialise directly into this ptr if we serialised
		// size separately.
		memcpy(ptr, data, dataSize);

		ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

		SAFE_DELETE_ARRAY(data);

		m_CleanupMems.push_back(mem);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(buf), 0, (byte *)info));
	}

	return true;
}

bool WrappedVulkan::Serialise_SparseInitialState(ResourceId id, WrappedVkImage *im, VulkanResourceManager::InitialContentData contents)
{
	if(m_State >= WRITING)
	{
		SparseImageInitState *state = (SparseImageInitState *)contents.blob;
		
		uint32_t totalPageCount = 0;
		for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
			totalPageCount += state->pageCount[a];
	
		m_pSerialiser->Serialise("opaqueCount", state->opaqueCount);
		m_pSerialiser->Serialise("totalPageCount", totalPageCount);
		m_pSerialiser->Serialise("imgdim", state->imgdim);
		m_pSerialiser->Serialise("pagedim", state->pagedim);
		m_pSerialiser->Serialise("numUniqueMems", state->numUniqueMems);

		if(state->opaqueCount > 0)
			m_pSerialiser->SerialiseComplexArray("opaque", state->opaque, state->opaqueCount);

		if(totalPageCount > 0)
		{
			for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
			{
				m_pSerialiser->Serialise("aspectPageCount", state->pageCount[a]);
				
				if(state->pageCount[a] > 0)
					m_pSerialiser->SerialiseComplexArray("pages", state->pages[a], state->pageCount[a]);
			}
		}

		if(state->numUniqueMems > 0)
			m_pSerialiser->SerialiseComplexArray("mems", state->memDataOffs, state->numUniqueMems);

		VkDevice d = GetDev();
		
		byte *ptr = NULL;
		ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource), 0, 0, 0, (void **)&ptr);

		size_t dataSize = (size_t)state->totalSize;

		m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

		ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource));
	}
	else
	{
		uint32_t opaqueCount = 0;
		uint32_t pageCount = 0;
		uint32_t numUniqueMems = 0;
		VkExtent3D imgdim = {};
		VkExtent3D pagedim = {};

		m_pSerialiser->Serialise("opaqueCount", opaqueCount);
		m_pSerialiser->Serialise("pageCount", pageCount);
		m_pSerialiser->Serialise("imgdim", imgdim);
		m_pSerialiser->Serialise("pagedim", pagedim);
		m_pSerialiser->Serialise("numUniqueMems", numUniqueMems);

		byte *blob = Serialiser::AllocAlignedBuffer(sizeof(SparseImageInitState) +
			sizeof(VkSparseMemoryBindInfo)*opaqueCount +
			sizeof(VkSparseImageMemoryBindInfo)*pageCount +
			sizeof(MemIDOffset)*numUniqueMems);

		SparseImageInitState *state = (SparseImageInitState *)blob;
		VkSparseMemoryBindInfo *opaque = (VkSparseMemoryBindInfo *)(state + 1);
		VkSparseImageMemoryBindInfo *pageBinds = (VkSparseImageMemoryBindInfo *)(opaque + opaqueCount);
		MemIDOffset *memDataOffs = (MemIDOffset *)(pageBinds + pageCount);
		
		RDCEraseEl(state->pageBinds);

		state->opaqueCount = opaqueCount;
		state->opaque = opaque;
		state->imgdim = imgdim;
		state->pagedim = pagedim;
		state->numUniqueMems = numUniqueMems;
		state->memDataOffs = memDataOffs;

		if(opaqueCount > 0)
		{
			VkSparseMemoryBindInfo *o = NULL;
			m_pSerialiser->SerialiseComplexArray("opaque", o, opaqueCount);
			memcpy(opaque, o, sizeof(VkSparseMemoryBindInfo)*opaqueCount);
			delete[] o;
		}
		else
		{
			state->opaque = NULL;
		}

		if(pageCount > 0)
		{
			for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
			{
				m_pSerialiser->Serialise("aspectPageCount", state->pageCount[a]);

				if(state->pageCount[a] == 0)
				{
					state->pageBinds[a] = NULL;
				}
				else
				{
					state->pageBinds[a] = pageBinds;
					pageBinds += state->pageCount[a];
					
					MemIDOffset *pages = NULL;
					m_pSerialiser->SerialiseComplexArray("pages", pages, state->pageCount[a]);

					uint32_t i=0;

					for(int32_t z=0; z < imgdim.depth; z++)
					{
						for(int32_t y=0; y < imgdim.height; y++)
						{
							for(int32_t x=0; x < imgdim.width; x++)
							{
								VkSparseImageMemoryBindInfo &p = state->pageBinds[a][i];

								p.mem = Unwrap(GetResourceManager()->GetLiveHandle<VkDeviceMemory>(pages[i].memId));
								p.memOffset = pages[i].memOffs;
								p.extent = pagedim;
								p.flags = 0; // VKTODOLOW do we need to preserve these flags?
								p.subresource.aspect = (VkImageAspect)a;
								p.subresource.arrayLayer = 0;
								p.subresource.mipLevel = 0;
								p.offset.x = x*p.extent.width;
								p.offset.y = y*p.extent.height;
								p.offset.z = z*p.extent.depth;

								i++;
							}
						}
					}

					delete[] pages;
				}
			}
		}

		if(state->numUniqueMems > 0)
		{
			MemIDOffset *m = NULL;
			m_pSerialiser->SerialiseComplexArray("opaque", m, numUniqueMems);
			memcpy(state->memDataOffs, m, sizeof(MemIDOffset)*numUniqueMems);
			delete[] m;
		}
		else
		{
			state->memDataOffs = NULL;
		}
		
		byte *data = NULL;
		size_t dataSize = 0;
		m_pSerialiser->SerialiseBuffer("data", data, dataSize);

		state->totalSize = (VkDeviceSize)dataSize;

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();

		VkDeviceMemory mem = VK_NULL_HANDLE;

		// VKTODOMED should get mem requirements for buffer - copy might enforce
		// some restrictions?
		VkMemoryRequirements mrq = { dataSize, 16, ~0U };

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			dataSize, GetUploadMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), mem);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			dataSize, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		VkBuffer buf;

		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &buf);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), buf);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		byte *ptr = NULL;
		ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, 0, 0, (void **)&ptr);

		// VKTODOLOW could deserialise directly into this ptr if we serialised
		// size separately.
		memcpy(ptr, data, dataSize);

		ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

		SAFE_DELETE_ARRAY(data);

		m_CleanupMems.push_back(mem);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(buf), eInitialContents_Sparse, blob));
	}

	return true;
}

bool WrappedVulkan::Apply_SparseInitialState(WrappedVkBuffer *buf, VulkanResourceManager::InitialContentData contents)
{
	SparseBufferInitState *info = (SparseBufferInitState *)contents.blob;

	// unbind the entire buffer so that any new areas that are bound are unbound again

	VkSparseMemoryBindInfo unbind = {
		0, m_CreationInfo.m_Buffer[buf->id].size,
		0, VK_NULL_HANDLE, 0
	};

	VkQueue q = GetQ();
	
	ObjDisp(q)->QueueBindSparseBufferMemory(Unwrap(q), buf->real.As<VkBuffer>(), 1, &unbind);

	if(info->numBinds > 0)
		ObjDisp(q)->QueueBindSparseBufferMemory(Unwrap(q), buf->real.As<VkBuffer>(), info->numBinds, info->binds);

	VkResult vkr = VK_SUCCESS;

	VkBuffer srcBuf = (VkBuffer)(uint64_t)contents.resource;

	VkDevice d = GetDev();

	VkCmdBuffer cmd = GetNextCmd();
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	VkBufferCreateInfo bufInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
		0, VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	};

	vector<VkBuffer> bufdeletes;

	for(uint32_t i=0; i < info->numUniqueMems; i++)
	{
		VkDeviceMemory dstMem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info->memDataOffs[i].memId);

		// since this is short lived it isn't wrapped. Note that we want
		// to cache this up front, so it will then be wrapped
		VkBuffer dstBuf;

		bufInfo.size = m_CreationInfo.m_Memory[GetResID(dstMem)].size;
	
		// VKTODOMED this should be created once up front, not every time
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(dstMem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		// fill the whole memory from the given offset
		VkBufferCopy region = { info->memDataOffs[i].memOffs, 0, bufInfo.size };

		ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), dstBuf, 1, &region);

		bufdeletes.push_back(dstBuf);
	}

	// add memory barrier to ensure this copy completes before any subsequent work
	VkMemoryBarrier memBarrier = {
		VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
		VK_MEMORY_OUTPUT_TRANSFER_BIT | VK_MEMORY_OUTPUT_HOST_WRITE_BIT,
		VK_MEMORY_INPUT_HOST_READ_BIT | VK_MEMORY_INPUT_UNIFORM_READ_BIT | VK_MEMORY_INPUT_SHADER_READ_BIT | VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT | VK_MEMORY_INPUT_TRANSFER_BIT,
	};

	void *barrier = (void *)&memBarrier;

	ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOLOW if this dstBuf was persistent or at least cached
	// we could batch these command buffers better and wouldn't
	// need to flush at all until application of all init states
	// is over
	SubmitCmds();
	FlushQ();

	for(size_t i=0; i < bufdeletes.size(); i++)
		ObjDisp(d)->DestroyBuffer(Unwrap(d), bufdeletes[i]);

	return true;
}

bool WrappedVulkan::Apply_SparseInitialState(WrappedVkImage *im, VulkanResourceManager::InitialContentData contents)
{
	SparseImageInitState *info = (SparseImageInitState *)contents.blob;

	VkQueue q = GetQ();
	
	if(info->opaque)
	{
		// unbind the entire image so that any new areas that are bound are unbound again

		// VKTODOMED not sure if this is the right size for opaque portion of partial resident
		// sparse image? how is that determined?
		VkSparseMemoryBindInfo unbind = {
			0, 0,
			0, VK_NULL_HANDLE, 0
		};
		
		VkMemoryRequirements mrq;
		ObjDisp(q)->GetImageMemoryRequirements(Unwrap(GetDev()), im->real.As<VkImage>(), &mrq);
		unbind.rangeSize = mrq.size;
		
		ObjDisp(q)->QueueBindSparseImageOpaqueMemory(Unwrap(q), im->real.As<VkImage>(), 1, &unbind);
		ObjDisp(q)->QueueBindSparseImageOpaqueMemory(Unwrap(q), im->real.As<VkImage>(), info->opaqueCount, info->opaque);
	}

	for(uint32_t a=0; a < VK_IMAGE_ASPECT_NUM; a++)
	{
		if(!info->pageBinds[a]) continue;
		
		ObjDisp(q)->QueueBindSparseImageMemory(Unwrap(q), im->real.As<VkImage>(), info->pageCount[a], info->pageBinds[a]);
	}

	VkResult vkr = VK_SUCCESS;

	VkBuffer srcBuf = (VkBuffer)(uint64_t)contents.resource;

	VkDevice d = GetDev();

	VkCmdBuffer cmd = GetNextCmd();
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	VkBufferCreateInfo bufInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
		0, VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	};

	vector<VkBuffer> bufdeletes;

	for(uint32_t i=0; i < info->numUniqueMems; i++)
	{
		VkDeviceMemory dstMem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info->memDataOffs[i].memId);

		// since this is short lived it isn't wrapped. Note that we want
		// to cache this up front, so it will then be wrapped
		VkBuffer dstBuf;

		bufInfo.size = m_CreationInfo.m_Memory[GetResID(dstMem)].size;
	
		// VKTODOMED this should be created once up front, not every time
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(dstMem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		// fill the whole memory from the given offset
		VkBufferCopy region = { info->memDataOffs[i].memOffs, 0, bufInfo.size };

		ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), dstBuf, 1, &region);

		bufdeletes.push_back(dstBuf);
	}

	// add memory barrier to ensure this copy completes before any subsequent work
	VkMemoryBarrier memBarrier = {
		VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
		VK_MEMORY_OUTPUT_TRANSFER_BIT | VK_MEMORY_OUTPUT_HOST_WRITE_BIT,
		VK_MEMORY_INPUT_HOST_READ_BIT | VK_MEMORY_INPUT_UNIFORM_READ_BIT | VK_MEMORY_INPUT_SHADER_READ_BIT | VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT | VK_MEMORY_INPUT_TRANSFER_BIT,
	};

	void *barrier = (void *)&memBarrier;

	ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOLOW if this dstBuf was persistent or at least cached
	// we could batch these command buffers better and wouldn't
	// need to flush at all until application of all init states
	// is over
	SubmitCmds();
	FlushQ();

	for(size_t i=0; i < bufdeletes.size(); i++)
		ObjDisp(d)->DestroyBuffer(Unwrap(d), bufdeletes[i]);

	return true;
}
		
bool WrappedVulkan::Prepare_InitialState(WrappedVkRes *res)
{
	ResourceId id = GetResourceManager()->GetID(res);

	VkResourceType type = IdentifyTypeByPtr(res);
	
	if(type == eResDescriptorSet)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
		RDCASSERT(record->descInfo && record->descInfo->layout);
		const DescSetLayout &layout = *record->descInfo->layout;

		uint32_t numElems = 0;
		for(size_t i=0; i < layout.bindings.size(); i++)
			numElems += layout.bindings[i].arraySize;

		VkDescriptorInfo *info = (VkDescriptorInfo *)Serialiser::AllocAlignedBuffer(sizeof(VkDescriptorInfo)*numElems);
		RDCEraseMem(info, sizeof(VkDescriptorInfo)*numElems);

		uint32_t e=0;
		for(size_t i=0; i < layout.bindings.size(); i++)
			for(uint32_t b=0; b < layout.bindings[i].arraySize; b++)
				info[e++] = record->descInfo->descBindings[i][b];

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, 0, (byte *)info));
		return true;
	}
	else if(type == eResBuffer)
	{
		WrappedVkBuffer *buffer = (WrappedVkBuffer *)res;
		
		// buffers are only dirty if they are sparse
		RDCASSERT(buffer->record->sparseInfo);
		
		return Prepare_SparseInitialState(buffer);
	}
	else if(type == eResImage)
	{
		VkResult vkr = VK_SUCCESS;

		WrappedVkImage *im = (WrappedVkImage *)res;
		
		if(im->record->sparseInfo)
		{
			// if the image is sparse we have to do a different kind of initial state prepare,
			// to serialise out the page mapping. The fetching of memory is also different
			return Prepare_SparseInitialState((WrappedVkImage *)res);
		}

		VkDevice d = GetDev();
		// VKTODOLOW ideally the prepares could be batched up
		// a bit more - maybe not all in one command buffer, but
		// at least more than one each
		VkCmdBuffer cmd = GetNextCmd();

		ImageLayouts *layout = NULL;
		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			layout = &m_ImageLayouts[im->id];
		}
		
		// get requirements to allocate memory large enough for all slices/mips
		VkMemoryRequirements immrq = {0};
		ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), im->real.As<VkImage>(), &immrq);

		VkDeviceMemory readbackmem = VK_NULL_HANDLE;
		
		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			immrq.size, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		// since this is very short lived, it is not wrapped
		VkBuffer dstBuf;

		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);
		
		VkMemoryRequirements mrq = { 0 };

		vkr = ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &readbackmem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), readbackmem);
		
		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkExtent3D extent = layout->extent;
			
		VkImageMemoryBarrier srcimTrans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			im->real.As<VkImage>(),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layout->arraySize }
		};

		// loop over every mip, copying it to the appropriate point in the buffer
		for(int m=0; m < layout->mipLevels; m++)
		{
			VkBufferImageCopy region = {
				0, 0, 0,
				{ VK_IMAGE_ASPECT_COLOR, m, 0, layout->arraySize },
				{ 0, 0, 0, },
				extent,
			};

			VkImageSubresource sub = { VK_IMAGE_ASPECT_COLOR, m, 0 };
			VkSubresourceLayout sublayout;

			vkr = ObjDisp(d)->GetImageSubresourceLayout(Unwrap(d), im->real.As<VkImage>(), &sub, &sublayout);
			RDCASSERT(vkr == VK_SUCCESS);
			
			// get the offset of the first array slice in this mip
			region.bufferOffset = sublayout.offset;
		
			// VKTODOMED handle getting the right origLayout for this mip, handle transitioning
			// multiple slices with different layouts etc
			VkImageLayout origLayout = layout->subresourceStates[0].newLayout;

			// transition the real image into transfer-source
			srcimTrans.oldLayout = origLayout;
			srcimTrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL;

			// ensure all previous writes have completed
			srcimTrans.outputMask =
				VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT|
				VK_MEMORY_OUTPUT_SHADER_WRITE_BIT|
				VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
				VK_MEMORY_OUTPUT_TRANSFER_BIT;
			// before we go reading
			srcimTrans.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;
			
			void *barrier = (void *)&srcimTrans;

			ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

			ObjDisp(d)->CmdCopyImageToBuffer(Unwrap(cmd), im->real.As<VkImage>(), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, dstBuf, 1, &region);

			// transfer back to whatever it was
			srcimTrans.oldLayout = srcimTrans.newLayout;
			srcimTrans.newLayout = origLayout;

			srcimTrans.outputMask = 0;
			srcimTrans.inputMask = 0;

			ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

			// update the extent for the next mip
			extent.width = RDCMAX(extent.width>>1, 1);
			extent.height = RDCMAX(extent.height>>1, 1);
			extent.depth = RDCMAX(extent.depth>>1, 1);
		}

		vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW would be nice to store up all these buffers so that
		// we don't have to submit & flush here before destroying, but
		// instead could submit all cmds, then flush once, then destroy
		// buffers. (or even not flush at all until capture is over)
		SubmitCmds();
		FlushQ();

		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), (uint32_t)mrq.size, NULL));

		return true;
	}
	else if(type == eResDeviceMemory)
	{
		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		// VKTODOLOW ideally the prepares could be batched up
		// a bit more - maybe not all in one command buffer, but
		// at least more than one each
		VkCmdBuffer cmd = GetNextCmd();
		
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
		VkDeviceSize dataoffs = 0;
		VkDeviceMemory datamem = ToHandle<VkDeviceMemory>(res);
		VkDeviceSize datasize = (VkDeviceSize)record->Length;

		RDCASSERT(datamem);

		RDCASSERT(record->Length > 0);
		VkDeviceSize memsize = (VkDeviceSize)record->Length;

		VkDeviceMemory readbackmem = VK_NULL_HANDLE;
		
		// VKTODOMED we just dump the backing memory for this image via an aliased buffer
		// copy, instead of doing a proper copy from image to buffer, which would be
		// independent of the image memory layout and do any unswizzling/untiling
		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			0, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		// since these are very short lived, they are not wrapped
		VkBuffer srcBuf, dstBuf;

		// dstBuf is just over the allocated memory, so only the image's size
		bufInfo.size = datasize;
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		// srcBuf spans the entire memory, then we copy out the sub-region we're interested in
		bufInfo.size = memsize;
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &srcBuf);
		RDCASSERT(vkr == VK_SUCCESS);
		
		VkMemoryRequirements mrq = { 0 };

		vkr = ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), srcBuf, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			datasize, GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

		vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &readbackmem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), readbackmem);
		
		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, datamem, 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { dataoffs, 0, datasize };

		ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

		vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW would be nice to store up all these buffers so that
		// we don't have to submit & flush here before destroying, but
		// instead could submit all cmds, then flush once, then destroy
		// buffers. (or even not flush at all until capture is over)
		SubmitCmds();
		FlushQ();

		ObjDisp(d)->DestroyBuffer(Unwrap(d), srcBuf);
		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), (uint32_t)datasize, NULL));

		return true;
	}
	else
	{
		RDCERR("Unhandled resource type %d", type);
	}

	return false;
}

bool WrappedVulkan::Serialise_InitialState(WrappedVkRes *res)
{
	// use same serialiser as resource manager
	Serialiser *localSerialiser = GetMainSerialiser();

	SERIALISE_ELEMENT(VkResourceType, type, IdentifyTypeByPtr(res));
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(res));

	if(m_State < WRITING) res = GetResourceManager()->GetLiveResource(id);
	
	if(m_State >= WRITING)
	{
		VulkanResourceManager::InitialContentData initContents = GetResourceManager()->GetInitialContents(id);

		if(type == eResDescriptorSet)
		{
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
			RDCASSERT(record->descInfo && record->descInfo->layout);
			const DescSetLayout &layout = *record->descInfo->layout;

			VkDescriptorInfo *info = (VkDescriptorInfo *)initContents.blob;

			uint32_t numElems = 0;
			for(size_t i=0; i < layout.bindings.size(); i++)
				numElems += layout.bindings[i].arraySize;

			m_pSerialiser->SerialiseComplexArray("Bindings", info, numElems);
		}
		else if(type == eResBuffer)
		{
			return Serialise_SparseInitialState(id, (WrappedVkBuffer *)res, initContents);
		}
		else if(type == eResDeviceMemory || type == eResImage)
		{
			// both image and memory are serialised as a whole hunk of data
			VkDevice d = GetDev();
			
			bool isSparse = (initContents.blob != NULL);
			m_pSerialiser->Serialise("isSparse", isSparse);

			if(isSparse)
			{
				// contains page mapping
				RDCASSERT(type == eResImage);
				return Serialise_SparseInitialState(id, (WrappedVkImage *)res, initContents);
			}
			
			byte *ptr = NULL;
			ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource), 0, 0, 0, (void **)&ptr);

			size_t dataSize = (size_t)initContents.num;

			m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

			ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource));
		}
		else
		{
			RDCERR("Unhandled resource type %d", type);
		}
	}
	else
	{
		RDCASSERT(res != NULL);

		ResourceId liveid = GetResourceManager()->GetLiveID(id);

		if(type == eResDescriptorSet)
		{
			uint32_t numElems;
			VkDescriptorInfo *bindings = NULL;

			m_pSerialiser->SerialiseComplexArray("Bindings", bindings, numElems);

			const DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[ m_DescriptorSetState[liveid].layout ];

			uint32_t numBinds = (uint32_t)layout.bindings.size();

			// allocate memory to keep the descriptorinfo structures around, as well as a WriteDescriptorSet array
			byte *blob = Serialiser::AllocAlignedBuffer(sizeof(VkDescriptorInfo)*numElems + sizeof(VkWriteDescriptorSet)*numBinds);

			VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)blob;
			VkDescriptorInfo *info = (VkDescriptorInfo *)(writes + numBinds);
			memcpy(info, bindings, sizeof(VkDescriptorInfo)*numElems);

			uint32_t validBinds = numBinds;

			// i is the writedescriptor that we're updating, could be
			// lower than j if a writedescriptor ended up being no-op and
			// was skipped. j is the actual index.
			for(uint32_t i=0, j=0; j < numBinds; j++)
			{
				writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[i].pNext = NULL;

				// update whole element (array or single)
				writes[i].destSet = ToHandle<VkDescriptorSet>(res);
				writes[i].destBinding = j;
				writes[i].destArrayElement = 0;
				writes[i].count = layout.bindings[j].arraySize;
				writes[i].descriptorType = layout.bindings[j].descriptorType;
				writes[i].pDescriptors = info;

				info += layout.bindings[j].arraySize;

				// check that the resources we need for this write are present,
				// as some might have been skipped due to stale descriptor set
				// slots or otherwise unreferenced objects (the descriptor set
				// initial contents do not cause a frame reference for their
				// resources
				bool valid = true;

				// quick check for slots that were completely uninitialised
				// and so don't have valid data
				if(writes[i].pDescriptors->bufferView == VK_NULL_HANDLE &&
						writes[i].pDescriptors->sampler == VK_NULL_HANDLE &&
						writes[i].pDescriptors->imageView == VK_NULL_HANDLE &&
						writes[i].pDescriptors->bufferInfo.buffer == VK_NULL_HANDLE)
				{
					valid = false;
				}
				else
				{
					switch(writes[i].descriptorType)
					{
						case VK_DESCRIPTOR_TYPE_SAMPLER:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].sampler != VK_NULL_HANDLE);
							break;
						}
						case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
							{
								valid &= (writes[i].pDescriptors[d].sampler != VK_NULL_HANDLE);
								valid &= (writes[i].pDescriptors[d].imageView != VK_NULL_HANDLE);
							}
							break;
						}
						case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
						case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
						case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].imageView != VK_NULL_HANDLE);
							break;
						}
						case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].bufferView != VK_NULL_HANDLE);
							break;
						}
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].bufferInfo.buffer != VK_NULL_HANDLE);
							break;
						}
						default:
							RDCERR("Unexpected descriptor type %d", writes[i].descriptorType);
					}
				}

				// if this write is not valid, skip it
				// and start writing the next one in here
				if(!valid)
					validBinds--;
				else
					i++;
			}

			SAFE_DELETE_ARRAY(bindings);

			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, validBinds, blob));
		}
		else if(type == eResBuffer)
		{
			return Serialise_SparseInitialState(id, (WrappedVkBuffer *)NULL, VulkanResourceManager::InitialContentData());
		}
		else if(type == eResImage)
		{
			bool isSparse = false;
			m_pSerialiser->Serialise("isSparse", isSparse);

			if(isSparse)
			{
				return Serialise_SparseInitialState(id, (WrappedVkImage *)NULL, VulkanResourceManager::InitialContentData());
			}

			byte *data = NULL;
			size_t dataSize = 0;
			m_pSerialiser->SerialiseBuffer("data", data, dataSize);

			WrappedVkImage *liveim = (WrappedVkImage *)res;

			VkResult vkr = VK_SUCCESS;

			VkDevice d = GetDev();

			VkBufferCreateInfo bufInfo = {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
				dataSize, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
				VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			};

			// short-lived, so not wrapped
			VkBuffer buf;
			VkDeviceMemory uploadmem = VK_NULL_HANDLE;

			vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &buf);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkMemoryRequirements mrq = { 0 };

			vkr = ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), buf, &mrq);
			RDCASSERT(vkr == VK_SUCCESS);

			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				dataSize, GetUploadMemoryIndex(mrq.memoryTypeBits),
			};

			// first we upload the data into a single buffer, then we do
			// a copy per-mip from that buffer to a new image
			vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &uploadmem);
			RDCASSERT(vkr == VK_SUCCESS);

			vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), buf, uploadmem, 0);
			RDCASSERT(vkr == VK_SUCCESS);

			byte *ptr = NULL;
			ObjDisp(d)->MapMemory(Unwrap(d), uploadmem, 0, 0, 0, (void **)&ptr);

			// VKTODOLOW could deserialise directly into this ptr if we serialised
			// size separately.
			memcpy(ptr, data, dataSize);

			ObjDisp(d)->UnmapMemory(Unwrap(d), uploadmem);

			SAFE_DELETE_ARRAY(data);

			// create image to copy into from the buffer
			VkImageCreateInfo imInfo = {
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
				m_CreationInfo.m_Image[liveim->id].type,
				m_CreationInfo.m_Image[liveim->id].format,
				m_CreationInfo.m_Image[liveim->id].extent,
				m_CreationInfo.m_Image[liveim->id].mipLevels,
				m_CreationInfo.m_Image[liveim->id].arraySize,
				m_CreationInfo.m_Image[liveim->id].samples,
				VK_IMAGE_TILING_OPTIMAL, // make this optimal since the format/etc is more likely supported as optimal
				VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
				0,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				NULL,
				VK_IMAGE_LAYOUT_UNDEFINED,
			};

			VkImage im = VK_NULL_HANDLE;

			vkr = ObjDisp(d)->CreateImage(Unwrap(d), &imInfo, &im);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(d), im);
			
			vkr = ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), Unwrap(im), &mrq);
			RDCASSERT(vkr == VK_SUCCESS);

			allocInfo.allocationSize = mrq.size;
			allocInfo.memoryTypeIndex = GetGPULocalMemoryIndex(mrq.memoryTypeBits);

			VkDeviceMemory mem = VK_NULL_HANDLE;

			vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(d), mem);

			vkr = ObjDisp(d)->BindImageMemory(Unwrap(d), Unwrap(im), Unwrap(mem), 0);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
			
			// VKTODOLOW ideally the prepares could be batched up
			// a bit more - maybe not all in one command buffer, but
			// at least more than one each
			VkCmdBuffer cmd = GetNextCmd();

			vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
			RDCASSERT(vkr == VK_SUCCESS);

			VkExtent3D extent = imInfo.extent;
			
			VkImageMemoryBarrier srcimTrans = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
				0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				Unwrap(im),
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, imInfo.arraySize }
			};

			// copy each mip individually
			for(uint32_t m=0; m < imInfo.mipLevels; m++)
			{
				VkBufferImageCopy region = {
					0, 0, 0,
					{ VK_IMAGE_ASPECT_COLOR, m, 0, imInfo.arraySize },
					{ 0, 0, 0, },
					extent,
				};
				
				VkImageSubresource sub = { VK_IMAGE_ASPECT_COLOR, m, 0 };
				VkSubresourceLayout sublayout;

				vkr = ObjDisp(d)->GetImageSubresourceLayout(Unwrap(d), Unwrap(im), &sub, &sublayout);
				RDCASSERT(vkr == VK_SUCCESS);

				region.bufferOffset = sublayout.offset;

				void *barrier = (void *)&srcimTrans;
				
				// first we transition from undefined to destination optimal, for the copy from the buffer
				srcimTrans.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				srcimTrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

				ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

				ObjDisp(d)->CmdCopyBufferToImage(Unwrap(cmd), buf, Unwrap(im), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &region);

				// then transition into source optimal, for all subsequent copies from this immutable initial
				// state image, to the live image.
				srcimTrans.oldLayout = srcimTrans.newLayout;
				srcimTrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL;

				ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

				extent.width = RDCMAX(extent.width>>1, 1);
				extent.height = RDCMAX(extent.height>>1, 1);
				extent.depth = RDCMAX(extent.depth>>1, 1);
			}

			vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
			RDCASSERT(vkr == VK_SUCCESS);

			// VKTODOLOW would be nice to store up all these buffers so that
			// we don't have to submit & flush here before destroying, but
			// instead could submit all cmds, then flush once, then destroy
			// buffers. (or even not flush at all until capture is over)
			SubmitCmds();
			FlushQ();

			// destroy the temporary buffer for uploading - we just keep the image
			ObjDisp(d)->DestroyBuffer(Unwrap(d), buf);
			ObjDisp(d)->FreeMemory(Unwrap(d), uploadmem);

			// remember to free this memory on shutdown
			m_CleanupMems.push_back(mem);

			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(im), 0, NULL));
		}
		else if(type == eResDeviceMemory)
		{
			// dummy since we share a serialise-write for devicememory and image. This will always be false
			bool isSparse = false;
			m_pSerialiser->Serialise("isSparse", isSparse);

			(void)isSparse;
			RDCASSERT(!isSparse);

			byte *data = NULL;
			size_t dataSize = 0;
			m_pSerialiser->SerialiseBuffer("data", data, dataSize);

			VkResult vkr = VK_SUCCESS;

			VkDevice d = GetDev();

			VkDeviceMemory mem = VK_NULL_HANDLE;

			// VKTODOMED should get mem requirements for buffer - copy might enforce
			// some restrictions?
			VkMemoryRequirements mrq = { dataSize, 16, ~0U };

			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				dataSize, GetUploadMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(d), mem);

			VkBufferCreateInfo bufInfo = {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
				dataSize, VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
				VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			};

			VkBuffer buf;

			vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &buf);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(d), buf);

			vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
			RDCASSERT(vkr == VK_SUCCESS);

			byte *ptr = NULL;
			ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, 0, 0, (void **)&ptr);

			// VKTODOLOW could deserialise directly into this ptr if we serialised
			// size separately.
			memcpy(ptr, data, dataSize);

			ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

			SAFE_DELETE_ARRAY(data);

			m_CleanupMems.push_back(mem);

			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(buf), (uint32_t)dataSize, NULL));
		}
		else
		{
			RDCERR("Unhandled resource type %d", type);
		}
	}

	return true;
}

void WrappedVulkan::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
{
	VkResourceType type = IdentifyTypeByPtr(live);
	
	if(type == eResDescriptorSet)
	{
		// There is no sensible default for a descriptor set to create. The contents are
		// undefined until written to. This means if a descriptor set was alloc'd within a
		// frame (the only time we won't have initial contents tracked for it) then the
		// contents are undefined, so using whatever is currently in the set is fine. Reading
		// from it (and thus getting data from later in the frame potentially) is an error.
		//
		// Note the same kind of problem applies if a descriptor set is alloc'd before the
		// frame and then say slot 5 is never written to until the middle of the frame, then
		// used. The initial states we have prepared won't have anything valid for 5 so when
		// we apply we won't even write anything into slot 5 - the same case as if we had
		// no initial states at all for that descriptor set
	}
	else if(type == eResImage)
	{
		ResourceId liveid = GetResourceManager()->GetLiveID(id);

		if(m_ImageLayouts.find(liveid) == m_ImageLayouts.end())
		{
			RDCERR("Couldn't find image info for %llu", id);
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearColorImage, NULL));
			return;
		}

		ImageLayouts &layouts = m_ImageLayouts[liveid];

		if(layouts.subresourceStates[0].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearColorImage, NULL));
		else
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearDepthStencilImage, NULL));
	}
	else if(type == eResDeviceMemory)
	{
		// ignore, it was probably dirty but not referenced in the frame
	}
	else
	{
		RDCERR("Unhandled resource type %d", type);
	}
}

void WrappedVulkan::Apply_InitialState(WrappedVkRes *live, VulkanResourceManager::InitialContentData initial)
{
	VkResourceType type = IdentifyTypeByPtr(live);
	
	ResourceId id = GetResourceManager()->GetID(live);

	if(type == eResDescriptorSet)
	{
		VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)initial.blob;

		// if it ended up that no descriptors were valid, just skip
		if(initial.num == 0)
			return;

		ObjDisp(GetDev())->UpdateDescriptorSets(Unwrap(GetDev()), initial.num, writes, 0, NULL);

		// need to blat over the current descriptor set contents, so these are available
		// when we want to fetch pipeline state
		vector<VkDescriptorInfo *> &bindings = m_DescriptorSetState[id].currentBindings;

		for(uint32_t i=0; i < initial.num; i++)
		{
			RDCASSERT(writes[i].destBinding < bindings.size());
			RDCASSERT(writes[i].destArrayElement == 0);

			VkDescriptorInfo *bind = bindings[writes[i].destBinding];

			for(uint32_t d=0; d < writes[i].count; d++)
				bind[d] = writes[i].pDescriptors[d];
		}
	}
	else if(type == eResBuffer)
	{
		Apply_SparseInitialState((WrappedVkBuffer *)live, initial);
	}
	else if(type == eResImage)
	{
		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		
		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		if(initial.blob != NULL)
		{
			RDCASSERT(initial.num == eInitialContents_Sparse);
			Apply_SparseInitialState((WrappedVkImage *)live, initial);
			return;
		}

		// handle any 'created' initial states, without an actual image with contents
		if(initial.resource == NULL)
		{
			RDCASSERT(initial.num != eInitialContents_Sparse);
			if(initial.num == eInitialContents_ClearColorImage)
			{
				VkCmdBuffer cmd = GetNextCmd();

				vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
				RDCASSERT(vkr == VK_SUCCESS);
				
				// VKTODOMED handle multiple subresources with different layouts etc
				VkImageMemoryBarrier barrier = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0,
					m_ImageLayouts[id].subresourceStates[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					ToHandle<VkImage>(live),
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS },
				};

				// finish any pending work before clear
				barrier.outputMask = VK_MEMORY_OUTPUT_HOST_WRITE_BIT|
					VK_MEMORY_OUTPUT_SHADER_WRITE_BIT|
					VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT|
					VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
					VK_MEMORY_OUTPUT_TRANSFER_BIT;
				// clear completes before subsequent operations
				barrier.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;

				void *barrierptr = (void *)&barrier;
				
				ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrierptr);
				
				VkClearColorValue clearval = { 0.0f, 0.0f, 0.0f, 0.0f };
				VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

				ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), ToHandle<VkImage>(live), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, &clearval, 1, &range);

				barrier.newLayout = barrier.oldLayout;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

				// complete clear before any other work
				barrier.outputMask = VK_MEMORY_OUTPUT_TRANSFER_BIT;
				barrier.inputMask = VK_MEMORY_INPUT_HOST_READ_BIT|
					VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT|
					VK_MEMORY_INPUT_INDEX_FETCH_BIT|
					VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT|
					VK_MEMORY_INPUT_UNIFORM_READ_BIT|
					VK_MEMORY_INPUT_SHADER_READ_BIT|
					VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT|
					VK_MEMORY_INPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
					VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT|
					VK_MEMORY_INPUT_TRANSFER_BIT;
				
				ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrierptr);

				vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
				RDCASSERT(vkr == VK_SUCCESS);
			}
			else if(initial.num == eInitialContents_ClearDepthStencilImage)
			{
				VkCmdBuffer cmd = GetNextCmd();

				vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
				RDCASSERT(vkr == VK_SUCCESS);
				
				// VKTODOMED handle multiple subresources with different layouts etc
				VkImageMemoryBarrier barrier = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0,
					m_ImageLayouts[id].subresourceStates[0].newLayout, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					ToHandle<VkImage>(live),
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS },
				};

				// finish any pending work before clear
				barrier.outputMask = VK_MEMORY_OUTPUT_HOST_WRITE_BIT|
					VK_MEMORY_OUTPUT_SHADER_WRITE_BIT|
					VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT|
					VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
					VK_MEMORY_OUTPUT_TRANSFER_BIT;
				// clear completes before subsequent operations
				barrier.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;

				void *barrierptr = (void *)&barrier;
				
				ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrierptr);
				
				VkClearDepthStencilValue clearval = { 1.0f, 0 };
				VkImageSubresourceRange range = { VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

				ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), ToHandle<VkImage>(live), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, &clearval, 1, &range);

				barrier.newLayout = barrier.oldLayout;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

				// complete clear before any other work
				barrier.outputMask = VK_MEMORY_OUTPUT_TRANSFER_BIT;
				barrier.inputMask = VK_MEMORY_INPUT_HOST_READ_BIT|
					VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT|
					VK_MEMORY_INPUT_INDEX_FETCH_BIT|
					VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT|
					VK_MEMORY_INPUT_UNIFORM_READ_BIT|
					VK_MEMORY_INPUT_SHADER_READ_BIT|
					VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT|
					VK_MEMORY_INPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
					VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT|
					VK_MEMORY_INPUT_TRANSFER_BIT;
				
				ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrierptr);

				vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
				RDCASSERT(vkr == VK_SUCCESS);
			}
			else
			{
				RDCERR("Unexpected initial state type %u with NULL resource", initial.num);
			}

			return;
		}

		WrappedVkImage *im = (WrappedVkImage *)initial.resource;

		VkCmdBuffer cmd = GetNextCmd();

		vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkExtent3D extent = m_CreationInfo.m_Image[id].extent;
		
		VkImageMemoryBarrier dstimTrans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			ToHandle<VkImage>(live),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, m_CreationInfo.m_Image[id].arraySize }
		};

		// loop over every mip
		for(int m=0; m < m_CreationInfo.m_Image[id].mipLevels; m++)
		{
			VkImageCopy region = {
				{ VK_IMAGE_ASPECT_COLOR, m, 0, m_CreationInfo.m_Image[id].arraySize },
				{ 0, 0, 0 },
				{ VK_IMAGE_ASPECT_COLOR, m, 0, m_CreationInfo.m_Image[id].arraySize },
				{ 0, 0, 0 },
				extent,
			};

			dstimTrans.subresourceRange.baseMipLevel = m;
		
			// VKTODOMED handle getting the right origLayout for this mip, handle multiple slices with different layouts etc
			VkImageLayout origLayout = m_ImageLayouts[id].subresourceStates[0].newLayout;

			// first transition the live image into destination optimal (the initial state
			// image is always and permanently in source optimal already).
			dstimTrans.oldLayout = origLayout;
			dstimTrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

			void *barrier = (void *)&dstimTrans;

			ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
			
			ObjDisp(cmd)->CmdCopyImage(Unwrap(cmd),
				im->real.As<VkImage>(), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
				ToHandle<VkImage>(live), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
				1, &region);

			// transition the live image back
			dstimTrans.oldLayout = dstimTrans.newLayout;
			dstimTrans.newLayout = origLayout;
		
			// make sure the apply completes before any further work
			dstimTrans.outputMask = VK_MEMORY_OUTPUT_TRANSFER_BIT;
			dstimTrans.inputMask = VK_MEMORY_INPUT_HOST_READ_BIT|
				VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT|
				VK_MEMORY_INPUT_INDEX_FETCH_BIT|
				VK_MEMORY_INPUT_VERTEX_ATTRIBUTE_FETCH_BIT|
				VK_MEMORY_INPUT_UNIFORM_READ_BIT|
				VK_MEMORY_INPUT_SHADER_READ_BIT|
				VK_MEMORY_INPUT_COLOR_ATTACHMENT_BIT|
				VK_MEMORY_INPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
				VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT|
				VK_MEMORY_INPUT_TRANSFER_BIT;

			ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

			// update the extent for the next mip
			extent.width = RDCMAX(extent.width>>1, 1);
			extent.height = RDCMAX(extent.height>>1, 1);
			extent.depth = RDCMAX(extent.depth>>1, 1);
		}

		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);
	}
	else if(type == eResDeviceMemory)
	{
		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		
		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		VkBuffer srcBuf = (VkBuffer)(uint64_t)initial.resource;
		VkDeviceSize datasize = (VkDeviceSize)initial.num;
		VkDeviceMemory dstMem = (VkDeviceMemory)(uint64_t)live; // maintain the wrapping, for consistency
		VkDeviceSize dstMemOffs = 0;
		VkDeviceSize memsize = datasize;

		VkCmdBuffer cmd = GetNextCmd();

		vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			memsize, VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		// since this is short lived it isn't wrapped. Note that we want
		// to cache this up front, so it will then be wrapped
		VkBuffer dstBuf;
		
		// VKTODOMED this should be created once up front, not every time
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(dstMem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { 0, dstMemOffs, datasize };

		ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), dstBuf, 1, &region);
	
		// add memory barrier to ensure this copy completes before any subsequent work
		VkMemoryBarrier memBarrier = {
			VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
			VK_MEMORY_OUTPUT_TRANSFER_BIT | VK_MEMORY_OUTPUT_HOST_WRITE_BIT,
			VK_MEMORY_INPUT_HOST_READ_BIT | VK_MEMORY_INPUT_UNIFORM_READ_BIT | VK_MEMORY_INPUT_SHADER_READ_BIT | VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT | VK_MEMORY_INPUT_TRANSFER_BIT,
		};

		void *barrier = (void *)&memBarrier;

		ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW if this dstBuf was persistent or at least cached
		// we could batch these command buffers better and wouldn't
		// need to flush at all until application of all init states
		// is over
		SubmitCmds();
		FlushQ();

		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);
	}
	else
	{
		RDCERR("Unhandled resource type %d", type);
	}
}

