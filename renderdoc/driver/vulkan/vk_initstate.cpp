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

bool WrappedVulkan::Prepare_InitialState(WrappedVkRes *res)
{
	ResourceId id = GetResourceManager()->GetID(res);

	VkResourceType type = IdentifyTypeByPtr(res);
	
	if(type == eResDescriptorSet)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
		RDCASSERT(record->layout);
		const DescSetLayout &layout = *record->layout;

		uint32_t numElems = 0;
		for(size_t i=0; i < layout.bindings.size(); i++)
			numElems += layout.bindings[i].arraySize;

		VkDescriptorInfo *info = (VkDescriptorInfo *)Serialiser::AllocAlignedBuffer(sizeof(VkDescriptorInfo)*numElems);
		RDCEraseMem(info, sizeof(VkDescriptorInfo)*numElems);

		uint32_t e=0;
		for(size_t i=0; i < layout.bindings.size(); i++)
			for(uint32_t b=0; b < layout.bindings[i].arraySize; b++)
				info[e++] = record->descBindings[i][b];

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, 0, (byte *)info));
		return true;
	}
	else if(type == eResImage)
	{
		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		// VKTODOLOW ideally the prepares could be batched up
		// a bit more - maybe not all in one command buffer, but
		// at least more than one each
		VkCmdBuffer cmd = GetNextCmd();
		
		WrappedVkImage *im = (WrappedVkImage *)res;

		ImageLayouts *layout = NULL;
		{
			SCOPED_LOCK(m_ImageLayoutsLock);
			layout = &m_ImageLayouts[im->id];
		}
		
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
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

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

			region.bufferOffset = sublayout.offset;
		
			// VKTODOMED handle getting the right origLayout for this mip, handle multiple slices with different layouts etc
			VkImageLayout origLayout = layout->subresourceStates[0].state;
			srcimTrans.oldLayout = origLayout;

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

			srcimTrans.oldLayout = srcimTrans.newLayout;
			srcimTrans.newLayout = origLayout;

			srcimTrans.outputMask = 0;
			srcimTrans.inputMask = 0;

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
			RDCASSERT(record->layout);
			const DescSetLayout &layout = *record->layout;

			VkDescriptorInfo *info = (VkDescriptorInfo *)initContents.blob;

			uint32_t numElems = 0;
			for(size_t i=0; i < layout.bindings.size(); i++)
				numElems += layout.bindings[i].arraySize;

			m_pSerialiser->SerialiseComplexArray("Bindings", info, numElems);
		}
		else if(type == eResDeviceMemory || type == eResImage)
		{
			VkDevice d = GetDev();

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
		else if(type == eResImage)
		{
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
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};

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

				ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

				ObjDisp(d)->CmdCopyBufferToImage(Unwrap(cmd), buf, Unwrap(im), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &region);

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

			ObjDisp(d)->DestroyBuffer(Unwrap(d), buf);
			ObjDisp(d)->FreeMemory(Unwrap(d), uploadmem);

			// remember to free this memory on shutdown
			m_FreeMems.push_back(mem);

			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(im), 0, NULL));
		}
		else if(type == eResDeviceMemory)
		{
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

			m_FreeMems.push_back(mem);

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
		VULKANNOTIMP("image initial states not implemented");

		ResourceId liveid = GetResourceManager()->GetLiveID(id);

		if(m_ImageLayouts.find(liveid) == m_ImageLayouts.end())
		{
			RDCERR("Couldn't find image info for %llu", id);
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearColorImage, NULL));
			return;
		}

		ImageLayouts &layouts = m_ImageLayouts[liveid];

		if(layouts.subresourceStates[0].range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
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
	else if(type == eResImage)
	{
		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		
		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
		
		if(initial.resource == NULL)
		{
			if(initial.num == eInitialContents_ClearColorImage)
			{
				VkCmdBuffer cmd = GetNextCmd();

				vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
				RDCASSERT(vkr == VK_SUCCESS);
				
				// VKTODOMED handle multiple subresources with different layouts etc
				VkImageMemoryBarrier barrier = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0,
					m_ImageLayouts[id].subresourceStates[0].state, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
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
					m_ImageLayouts[id].subresourceStates[0].state, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
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
			VkImageLayout origLayout = m_ImageLayouts[id].subresourceStates[0].state;
			dstimTrans.oldLayout = origLayout;

			void *barrier = (void *)&dstimTrans;

			ObjDisp(d)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
			
			ObjDisp(cmd)->CmdCopyImage(Unwrap(cmd),
				im->real.As<VkImage>(), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
				ToHandle<VkImage>(live), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
				1, &region);

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

