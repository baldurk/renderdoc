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

bool WrappedVulkan::Serialise_vkCreateDescriptorPool(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDescriptorPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorPool));

	if(m_State == READING)
	{
		VkDescriptorPool pool = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &info, &pool);

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

VkResult WrappedVulkan::vkCreateDescriptorPool(
			VkDevice                                    device,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool)
{
	VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pCreateInfo, pDescriptorPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorPool);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_POOL);
				Serialise_vkCreateDescriptorPool(localSerialiser, device, pCreateInfo, pDescriptorPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDescriptorPool);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pDescriptorPool);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDescriptorSetLayout(
		Serialiser*                                 localSerialiser,
		VkDevice                                    device,
		const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
		VkDescriptorSetLayout*                      pSetLayout)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDescriptorSetLayoutCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSetLayout));

	// this creation info is needed at capture time (for creating/updating descriptor set bindings)
	// uses original ID in replay
	m_CreationInfo.m_DescSetLayout[id].Init(&info);

	if(m_State == READING)
	{
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &info, &layout);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), layout);
			GetResourceManager()->AddLiveResource(id, layout);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDescriptorSetLayout(
		VkDevice                                    device,
		const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
		VkDescriptorSetLayout*                      pSetLayout)
{
	size_t tempmemSize = sizeof(VkDescriptorSetLayoutBinding)*pCreateInfo->count;
	
	// need to count how many VkSampler arrays to allocate for
	for(uint32_t i=0; i < pCreateInfo->count; i++)
		if(pCreateInfo->pBinding[i].pImmutableSamplers) tempmemSize += pCreateInfo->pBinding[i].arraySize;

	byte *memory = GetTempMemory(tempmemSize);

	VkDescriptorSetLayoutBinding *unwrapped = (VkDescriptorSetLayoutBinding *)memory;
	VkSampler *nextSampler = (VkSampler *)(unwrapped + pCreateInfo->count);

	for(uint32_t i=0; i < pCreateInfo->count; i++)
	{
		unwrapped[i] = pCreateInfo->pBinding[i];

		if(unwrapped[i].pImmutableSamplers)
		{
			VkSampler *unwrappedSamplers = nextSampler; nextSampler += unwrapped[i].arraySize;
			for(uint32_t j=0; j < unwrapped[i].arraySize; j++)
				unwrappedSamplers[j] = Unwrap(unwrapped[i].pImmutableSamplers[j]);
			unwrapped[i].pImmutableSamplers = unwrappedSamplers;
		}
	}

	VkDescriptorSetLayoutCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.pBinding = unwrapped;
	VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrappedInfo, pSetLayout);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSetLayout);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_SET_LAYOUT);
				Serialise_vkCreateDescriptorSetLayout(localSerialiser, device, pCreateInfo, pSetLayout);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSetLayout);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSetLayout);
		}
	}

	return ret;
}
bool WrappedVulkan::Serialise_vkAllocDescriptorSets(
		Serialiser*                                 localSerialiser,
		VkDevice                                    device,
		VkDescriptorPool                            descriptorPool,
		VkDescriptorSetUsage                        setUsage,
		uint32_t                                    count,
		const VkDescriptorSetLayout*                pSetLayouts,
		VkDescriptorSet*                            pDescriptorSets)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, poolId, GetResID(descriptorPool));
	SERIALISE_ELEMENT(VkDescriptorSetUsage, usage, setUsage);
	SERIALISE_ELEMENT(ResourceId, layoutId, GetResID(*pSetLayouts));
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorSets));

	if(m_State == READING)
	{
		VkDescriptorSet descset = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		descriptorPool = GetResourceManager()->GetLiveHandle<VkDescriptorPool>(poolId);
		VkDescriptorSetLayout layout = GetResourceManager()->GetLiveHandle<VkDescriptorSetLayout>(layoutId);

		VkResult ret = ObjDisp(device)->AllocDescriptorSets(Unwrap(device), Unwrap(descriptorPool), usage, 1, UnwrapPtr(layout), &descset);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), descset);
			GetResourceManager()->AddLiveResource(id, descset);

			// this is stored in the resource record on capture, we need to be able to look to up
			m_DescriptorSetInfo[id].layout = layoutId;
			m_CreationInfo.m_DescSetLayout[layoutId].CreateBindingsArray(m_DescriptorSetInfo[id].currentBindings);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkAllocDescriptorSets(
		VkDevice                                    device,
		VkDescriptorPool                            descriptorPool,
		VkDescriptorSetUsage                        setUsage,
		uint32_t                                    count,
		const VkDescriptorSetLayout*                pSetLayouts,
		VkDescriptorSet*                            pDescriptorSets)
{
	VkDescriptorSetLayout *unwrapped = GetTempArray<VkDescriptorSetLayout>(count);
	for(uint32_t i=0; i < count; i++) unwrapped[i] = Unwrap(pSetLayouts[i]);

	VkResult ret = ObjDisp(device)->AllocDescriptorSets(Unwrap(device), Unwrap(descriptorPool), setUsage, count, unwrapped, pDescriptorSets);

	if(ret != VK_SUCCESS) return ret;

	for(uint32_t i=0; i < count; i++)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(ALLOC_DESC_SET);
				Serialise_vkAllocDescriptorSets(localSerialiser, device, descriptorPool, setUsage, 1, &pSetLayouts[i], &pDescriptorSets[i]);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pDescriptorSets[i]);
			record->AddChunk(chunk);

			ResourceId layoutID = GetResID(pSetLayouts[i]);

			record->AddParent(GetRecord(descriptorPool));
			record->AddParent(GetResourceManager()->GetResourceRecord(layoutID));

			// just always treat descriptor sets as dirty
			{
				SCOPED_LOCK(m_CapTransitionLock);
				if(m_State != WRITING_CAPFRAME)
					GetResourceManager()->MarkDirtyResource(id);
				else
					GetResourceManager()->MarkPendingDirty(id);
			}

			record->layout = layoutID;
			m_CreationInfo.m_DescSetLayout[layoutID].CreateBindingsArray(record->descBindings);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, pDescriptorSets[i]);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkFreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    count,
    const VkDescriptorSet*                      pDescriptorSets)
{
	VkDescriptorSet *unwrapped = GetTempArray<VkDescriptorSet>(count);
	for(uint32_t i=0; i < count; i++) unwrapped[i] = Unwrap(pDescriptorSets[i]);

	for(uint32_t i=0; i < count; i++)
		GetResourceManager()->ReleaseWrappedResource(pDescriptorSets[i]);

	VkResult ret = ObjDisp(device)->FreeDescriptorSets(Unwrap(device), Unwrap(descriptorPool), count, unwrapped);

	SAFE_DELETE_ARRAY(unwrapped);

	return ret;
}

bool WrappedVulkan::Serialise_vkUpdateDescriptorSets(
		Serialiser*                                 localSerialiser,
		VkDevice                                    device,
		uint32_t                                    writeCount,
		const VkWriteDescriptorSet*                 pDescriptorWrites,
		uint32_t                                    copyCount,
		const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(bool, writes, writeCount == 1);

	VkWriteDescriptorSet writeDesc;
	VkCopyDescriptorSet copyDesc;
	if(writes)
	{
		SERIALISE_ELEMENT(VkWriteDescriptorSet, w, *pDescriptorWrites);
		writeDesc = w;
	}
	else
	{
		SERIALISE_ELEMENT(VkCopyDescriptorSet, c, *pDescriptorCopies);
		copyDesc = c;
	}

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		if(writes)
		{
			// check for validity - if a resource wasn't referenced other than in this update
			// (ie. the descriptor set was overwritten or never bound), then the write descriptor
			// will be invalid with some missing handles. It's safe though to just skip this
			// update as we only get here if it's never used.

			// if a set was never bind, it will have been omitted and we just drop any writes to it
			bool valid = (writeDesc.destSet != VK_NULL_HANDLE);

			if(!valid)
				return true;

			switch(writeDesc.descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_SAMPLER:
				{
					for(uint32_t i=0; i < writeDesc.count; i++)
						valid &= (writeDesc.pDescriptors[i].sampler != VK_NULL_HANDLE);
					break;
				}
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				{
					for(uint32_t i=0; i < writeDesc.count; i++)
					{
						valid &= (writeDesc.pDescriptors[i].sampler != VK_NULL_HANDLE);
						valid &= (writeDesc.pDescriptors[i].imageView != VK_NULL_HANDLE);
					}
					break;
				}
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				{
					for(uint32_t i=0; i < writeDesc.count; i++)
						valid &= (writeDesc.pDescriptors[i].imageView != VK_NULL_HANDLE);
					break;
				}
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				{
					for(uint32_t i=0; i < writeDesc.count; i++)
						valid &= (writeDesc.pDescriptors[i].bufferView != VK_NULL_HANDLE);
					break;
				}
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				{
					for(uint32_t i=0; i < writeDesc.count; i++)
						valid &= (writeDesc.pDescriptors[i].bufferInfo.buffer != VK_NULL_HANDLE);
					break;
				}
				default:
					RDCERR("Unexpected descriptor type %d", writeDesc.descriptorType);
			}

			if(valid)
			{
				ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &writeDesc, 0, NULL);

				// update our local tracking
				vector<VkDescriptorInfo *> &bindings = m_DescriptorSetInfo[GetResourceManager()->GetOriginalID(GetResourceManager()->GetNonDispWrapper(writeDesc.destSet)->id)].currentBindings;

				{
					RDCASSERT(writeDesc.destBinding < bindings.size());
					RDCASSERT(writeDesc.destArrayElement == 0);

					VkDescriptorInfo *bind = bindings[writeDesc.destBinding];

					for(uint32_t d=0; d < writeDesc.count; d++)
						bind[d] = writeDesc.pDescriptors[d];
				}
			}
		}
		else
		{
			ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 0, NULL, 1, &copyDesc);
			
			// don't want to implement this blindly
			RDCUNIMPLEMENTED("Copying descriptors not implemented");
		}
	}

	return true;
}

void WrappedVulkan::vkUpdateDescriptorSets(
		VkDevice                                    device,
		uint32_t                                    writeCount,
		const VkWriteDescriptorSet*                 pDescriptorWrites,
		uint32_t                                    copyCount,
		const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	{
		// need to count up number of descriptor infos, to be able to alloc enough space
		uint32_t numInfos = 0;
		for(uint32_t i=0; i < writeCount; i++) numInfos += pDescriptorWrites[i].count;
		
		byte *memory = GetTempMemory(sizeof(VkDescriptorInfo)*numInfos +
			sizeof(VkWriteDescriptorSet)*writeCount + sizeof(VkCopyDescriptorSet)*copyCount);

		VkWriteDescriptorSet *unwrappedWrites = (VkWriteDescriptorSet *)memory;
		VkCopyDescriptorSet *unwrappedCopies = (VkCopyDescriptorSet *)(unwrappedWrites + writeCount);
		VkDescriptorInfo *nextDescriptors = (VkDescriptorInfo *)(unwrappedCopies + copyCount);
		
		for(uint32_t i=0; i < writeCount; i++)
		{
			unwrappedWrites[i] = pDescriptorWrites[i];
			unwrappedWrites[i].destSet = Unwrap(unwrappedWrites[i].destSet);

			VkDescriptorInfo *unwrappedInfos = nextDescriptors;
			nextDescriptors += pDescriptorWrites[i].count;

			for(uint32_t j=0; j < pDescriptorWrites[i].count; j++)
			{
				unwrappedInfos[j] = unwrappedWrites[i].pDescriptors[j];
				unwrappedInfos[j].bufferView = Unwrap(unwrappedInfos[j].bufferView);
				unwrappedInfos[j].sampler = Unwrap(unwrappedInfos[j].sampler);
				unwrappedInfos[j].imageView = Unwrap(unwrappedInfos[j].imageView);
				unwrappedInfos[j].bufferInfo.buffer = Unwrap(unwrappedInfos[j].bufferInfo.buffer);
			}
			
			unwrappedWrites[i].pDescriptors = unwrappedInfos;
		}

		for(uint32_t i=0; i < copyCount; i++)
		{
			unwrappedCopies[i] = pDescriptorCopies[i];
			unwrappedCopies[i].destSet = Unwrap(unwrappedCopies[i].destSet);
			unwrappedCopies[i].srcSet = Unwrap(unwrappedCopies[i].srcSet);
		}

		ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), writeCount, unwrappedWrites, copyCount, unwrappedCopies);
	}
	
	bool capframe = false;
	{
		SCOPED_LOCK(m_CapTransitionLock);
		capframe = (m_State == WRITING_CAPFRAME);
	}

	if(capframe)
	{
		// don't have to mark referenced any of the resources pointed to by the descriptor set - that's handled
		// on queue submission by marking ref'd all the current bindings of the sets referenced by the cmd buffer

		for(uint32_t i=0; i < writeCount; i++)
		{
			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
				Serialise_vkUpdateDescriptorSets(localSerialiser, device, 1, &pDescriptorWrites[i], 0, NULL);

				m_FrameCaptureRecord->AddChunk(scope.Get());
			}

			// as long as descriptor sets are forced to have initial states, we don't have to mark them ref'd for
			// write here. The reason being that as long as we only mark them as ref'd when they're actually bound,
			// we can safely skip the ref here and it means any descriptor set updates of descriptor sets that are
			// never used in the frame can be ignored.
			//GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].destSet), eFrameRef_Write);
		}

		for(uint32_t i=0; i < copyCount; i++)
		{
			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
				Serialise_vkUpdateDescriptorSets(localSerialiser, device, 0, NULL, 1, &pDescriptorCopies[i]);

				m_FrameCaptureRecord->AddChunk(scope.Get());
			}
			
			// as long as descriptor sets are forced to have initial states, we don't have to mark them ref'd for
			// write here. The reason being that as long as we only mark them as ref'd when they're actually bound,
			// we can safely skip the ref here and it means any descriptor set updates of descriptor sets that are
			// never used in the frame can be ignored.
			//GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].destSet), eFrameRef_Write);
			//GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].srcSet), eFrameRef_Read);
		}
	}

	// need to track descriptor set contents whether capframing or idle
	if(m_State >= WRITING)
	{
		for(uint32_t i=0; i < writeCount; i++)
		{
			VkResourceRecord *record = GetRecord(pDescriptorWrites[i].destSet);
			const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[record->layout];

			RDCASSERT(pDescriptorWrites[i].destBinding < record->descBindings.size());
			
			VkDescriptorInfo *binding = record->descBindings[pDescriptorWrites[i].destBinding];

			FrameRefType ref = eFrameRef_Write;

			switch(layout.bindings[pDescriptorWrites[i].destBinding].descriptorType)
			{
				case VK_DESCRIPTOR_TYPE_SAMPLER:
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
					ref = eFrameRef_Read;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
					ref = eFrameRef_Write;
					break;
				default:
					RDCERR("Unexpected descriptor type");
			}

			// We need to handle the cases where these bindings are stale:
			// ie. image handle 0xf00baa is allocated
			// bound into a descriptor set
			// image is released
			// descriptor set is bound but this image is never used by shader etc.
			//
			// worst case, a new image or something has been added with this handle -
			// in this case we end up ref'ing an image that isn't actually used.
			// Worst worst case, we ref an image as write when actually it's not, but
			// this is likewise not a serious problem, and rather difficult to solve
			// (would need to version handles somehow, but don't have enough bits
			// to do that reliably).
			//
			// This is handled by RemoveBindFrameRef silently dropping id == ResourceId()

			for(uint32_t d=0; d < pDescriptorWrites[i].count; d++)
			{
				VkDescriptorInfo &bind = binding[pDescriptorWrites[i].destArrayElement + d];

				if(bind.bufferView != VK_NULL_HANDLE)
					record->RemoveBindFrameRef(GetResID(bind.bufferView));
				if(bind.imageView != VK_NULL_HANDLE)
					record->RemoveBindFrameRef(GetResID(bind.imageView));
				if(bind.sampler != VK_NULL_HANDLE)
					record->RemoveBindFrameRef(GetResID(bind.sampler));
				if(bind.bufferInfo.buffer != VK_NULL_HANDLE)
					record->RemoveBindFrameRef(GetResID(bind.bufferInfo.buffer));

				bind = pDescriptorWrites[i].pDescriptors[d];

				if(bind.bufferView != VK_NULL_HANDLE)
					record->AddBindFrameRef(GetResID(bind.bufferView), ref);
				if(bind.imageView != VK_NULL_HANDLE)
					record->AddBindFrameRef(GetResID(bind.imageView), ref);
				if(bind.sampler != VK_NULL_HANDLE)
					record->AddBindFrameRef(GetResID(bind.sampler), ref);
				if(bind.bufferInfo.buffer != VK_NULL_HANDLE)
					record->AddBindFrameRef(GetResID(bind.bufferInfo.buffer), ref);
			}
		}

		if(copyCount > 0)
		{
			// don't want to implement this blindly
			RDCUNIMPLEMENTED("Copying descriptors not implemented");
		}
	}
}
