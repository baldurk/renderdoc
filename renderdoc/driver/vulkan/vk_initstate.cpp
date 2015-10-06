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
		const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[record->layout];

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
	else if(type == eResDeviceMemory)
	{
		if(m_MemoryInfo.find(id) == m_MemoryInfo.end())
		{
			RDCERR("Couldn't find memory info");
			return false;
		}

		MemState &meminfo = m_MemoryInfo[id];

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		VkQueue q = GetQ();
		VkCmdBuffer cmd = GetCmd();

		VkDeviceMemory mem = VK_NULL_HANDLE;
		
		// VKTODOMED should get mem requirements for buffer - copy might enforce
		// some restrictions?
		VkMemoryRequirements mrq = { meminfo.size, 16, ~0U };

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			meminfo.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), mem);

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = ObjDisp(d)->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			meminfo.size, VK_BUFFER_USAGE_GENERAL, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		// since these are very short lived, they are not wrapped
		VkBuffer srcBuf, dstBuf;

		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &srcBuf);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, ToHandle<VkDeviceMemory>(res), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(mem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { 0, 0, meminfo.size };

		ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

		vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOMED would be nice to store a fence too at this point
		// so we can sync on that on serialise rather than syncing
		// every time.
		ObjDisp(d)->QueueWaitIdle(Unwrap(q));

		ObjDisp(d)->DestroyBuffer(Unwrap(d), srcBuf);
		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(mem), (uint32_t)meminfo.size, NULL));

		return true;
	}
	else if(type == eResImage)
	{
		VULKANNOTIMP("image initial states not implemented");

		if(m_ImageInfo.find(id) == m_ImageInfo.end())
		{
			RDCERR("Couldn't find image info");
			return false;
		}

		// VKTODOHIGH: need to copy off contents to memory somewhere else

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
			const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[record->layout];

			VkDescriptorInfo *info = (VkDescriptorInfo *)initContents.blob;

			uint32_t numElems = 0;
			for(size_t i=0; i < layout.bindings.size(); i++)
				numElems += layout.bindings[i].arraySize;

			m_pSerialiser->SerialiseComplexArray("Bindings", info, numElems);
		}
		else if(type == eResDeviceMemory)
		{
			VkDevice d = GetDev();

			byte *ptr = NULL;
			ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource), 0, 0, 0, (void **)&ptr);

			size_t dataSize = (size_t)initContents.num;

			m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

			ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource));
		}
		else if(type == eResImage)
		{
			VULKANNOTIMP("image initial states not implemented");
		}
	}
	else
	{
		RDCASSERT(res != NULL);

		if(type == eResDescriptorSet)
		{
			uint32_t numElems;
			VkDescriptorInfo *bindings = NULL;

			m_pSerialiser->SerialiseComplexArray("Bindings", bindings, numElems);

			const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[ m_DescriptorSetInfo[id].layout ];

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
						writes[i].pDescriptors->attachmentView == VK_NULL_HANDLE)
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
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].imageView != VK_NULL_HANDLE);
							break;
						}
						case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].bufferView != VK_NULL_HANDLE);
							break;
						}
						case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
						{
							for(uint32_t d=0; d < writes[i].count; d++)
								valid &= (writes[i].pDescriptors[d].attachmentView != VK_NULL_HANDLE);
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

			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, validBinds, blob));
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
				dataSize, VK_BUFFER_USAGE_GENERAL, 0,
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

			// VKTODOMED leaking the memory here! needs to be cleaned up with the buffer
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(buf), eInitialContents_Copy, NULL));
		}
		else if(type == eResImage)
		{
			VULKANNOTIMP("image initial states not implemented");
		}
	}

	return true;
}

void WrappedVulkan::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
{
	VkResourceType type = IdentifyTypeByPtr(live);
	
	if(type == eResDescriptorSet)
	{
		// VKTODOMED need to create some default initial state for descriptor sets.
		// if a descriptor set is alloc'd then used in frame we won't have prepared anything,
		// but likewise all writes must happen within that frame so the initial state doesn't
		// technically matter. We assume the app doesn't try to read from an uninitialised
		// descriptor, so for now we can leave the initial state empty.
		VULKANNOTIMP("Need to create initial state for descriptor set");
	}
	else if(type == eResImage)
	{
		VULKANNOTIMP("image initial states not implemented");

		ResourceId liveid = GetResourceManager()->GetLiveID(id);

		if(m_ImageInfo.find(liveid) == m_ImageInfo.end())
		{
			RDCERR("Couldn't find image info for %llu", id);
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearColorImage, NULL));
			return;
		}

		ImgState &img = m_ImageInfo[liveid];

		if(img.subresourceStates[0].range.aspect == VK_IMAGE_ASPECT_COLOR)
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearColorImage, NULL));
		else
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearDepthStencilImage, NULL));
	}
	else if(type == eResDeviceMemory)
	{
		// ignore, it was probably dirty but not referenced in the frame
	}
	else if(type == eResFramebuffer)
	{
		RDCWARN("Framebuffer without initial state! should clear all attachments");
	}
	else if(type == eResBuffer)
	{
		// don't have to do anything for buffers, initial state is all handled by memory
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

		VkResult vkr = ObjDisp(GetDev())->UpdateDescriptorSets(Unwrap(GetDev()), initial.num, writes, 0, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		// need to blat over the current descriptor set contents, so these are available
		// when we want to fetch pipeline state
		vector<VkDescriptorInfo *> &bindings = m_DescriptorSetInfo[GetResourceManager()->GetOriginalID(id)].currentBindings;

		for(uint32_t i=0; i < initial.num; i++)
		{
			RDCASSERT(writes[i].destBinding < bindings.size());
			RDCASSERT(writes[i].destArrayElement == 0);

			VkDescriptorInfo *bind = bindings[writes[i].destBinding];

			for(uint32_t d=0; d < writes[i].count; d++)
				bind[d] = writes[i].pDescriptors[d];
		}
	}
	else if(type == eResDeviceMemory)
	{
		if(m_MemoryInfo.find(id) == m_MemoryInfo.end())
		{
			RDCERR("Couldn't find memory info");
			return;
		}

		MemState &meminfo = m_MemoryInfo[id];
		
		VkBuffer srcBuf = (VkBuffer)(uint64_t)initial.resource;
		VkDeviceMemory dstMem = (VkDeviceMemory)(uint64_t)live; // maintain the wrapping, for consistency

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		VkQueue q = GetQ();
		VkCmdBuffer cmd = GetCmd();

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
		
		vkr = ObjDisp(cmd)->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			meminfo.size, VK_BUFFER_USAGE_GENERAL, 0,
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

		VkBufferCopy region = { 0, 0, meminfo.size };

		ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), dstBuf, 1, &region);
	
		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(q)->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOMED would be nice to store a fence too at this point
		// so we can sync on that on serialise rather than syncing
		// every time.
		ObjDisp(q)->QueueWaitIdle(Unwrap(q));

		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);
	}
	else if(type == eResImage)
	{
		// VKTODOHIGH: need to copy initial copy to live
		VULKANNOTIMP("image initial states not implemented");
	}
	else
	{
		RDCERR("Unhandled resource type %d", type);
	}
}

