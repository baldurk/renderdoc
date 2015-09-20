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
			VkDevice                                    device,
			VkDescriptorPoolUsage                       poolUsage,
			uint32_t                                    maxSets,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDescriptorPoolUsage, pooluse, poolUsage);
	SERIALISE_ELEMENT(uint32_t, maxs, maxSets);
	SERIALISE_ELEMENT(VkDescriptorPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorPool));

	if(m_State == READING)
	{
		VkDescriptorPool pool = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pooluse, maxs, &info, &pool);

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
			VkDescriptorPoolUsage                       poolUsage,
			uint32_t                                    maxSets,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool)
{
	VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), poolUsage, maxSets, pCreateInfo, pDescriptorPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorPool);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_POOL);
				Serialise_vkCreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);

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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSetLayoutBinding *unwrapped = new VkDescriptorSetLayoutBinding[pCreateInfo->count];
	for(uint32_t i=0; i < pCreateInfo->count; i++)
	{
		unwrapped[i] = pCreateInfo->pBinding[i];

		if(unwrapped[i].pImmutableSamplers)
		{
			VkSampler *unwrappedSamplers = new VkSampler[unwrapped[i].arraySize];
			for(uint32_t j=0; j < unwrapped[i].arraySize; j++)
				unwrappedSamplers[j] = Unwrap(unwrapped[i].pImmutableSamplers[j]);
			unwrapped[i].pImmutableSamplers = unwrappedSamplers;
		}
	}

	VkDescriptorSetLayoutCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.pBinding = unwrapped;
	VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrappedInfo, pSetLayout);
	
	for(uint32_t i=0; i < pCreateInfo->count; i++)
		delete[] unwrapped[i].pImmutableSamplers;
	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSetLayout);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_SET_LAYOUT);
				Serialise_vkCreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);

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
		VkDevice                                    device,
		VkDescriptorPool                            descriptorPool,
		VkDescriptorSetUsage                        setUsage,
		uint32_t                                    count,
		const VkDescriptorSetLayout*                pSetLayouts,
		VkDescriptorSet*                            pDescriptorSets,
		uint32_t*                                   pCount)
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

		uint32_t cnt = 0;
		VkResult ret = ObjDisp(device)->AllocDescriptorSets(Unwrap(device), Unwrap(descriptorPool), usage, 1, UnwrapPtr(layout), &descset, &cnt);

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
		VkDescriptorSet*                            pDescriptorSets,
		uint32_t*                                   pCount)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSetLayout *unwrapped = new VkDescriptorSetLayout[count];
	for(uint32_t i=0; i < count; i++)
		unwrapped[i] = Unwrap(pSetLayouts[i]);

	VkResult ret = ObjDisp(device)->AllocDescriptorSets(Unwrap(device), Unwrap(descriptorPool), setUsage, count, unwrapped, pDescriptorSets, pCount);

	SAFE_DELETE_ARRAY(unwrapped);
	
	RDCASSERT(pCount == NULL || *pCount == count); // VKTODOMED: find out what *pCount < count means

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(ALLOC_DESC_SET);
					Serialise_vkAllocDescriptorSets(device, descriptorPool, setUsage, 1, &pSetLayouts[i], &pDescriptorSets[i], NULL);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pDescriptorSets[i]);
				record->AddChunk(chunk);

				ResourceId layoutID = GetResID(pSetLayouts[i]);

				record->AddParent(GetRecord(descriptorPool));
				record->AddParent(GetResourceManager()->GetResourceRecord(layoutID));

				// just always treat descriptor sets as dirty
				GetResourceManager()->MarkDirtyResource(id);

				record->layout = layoutID;
				m_CreationInfo.m_DescSetLayout[layoutID].CreateBindingsArray(record->descBindings);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, pDescriptorSets[i]);
			}
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSet *unwrapped = new VkDescriptorSet[count];
	for(uint32_t i=0; i < count; i++)
		unwrapped[i] = Unwrap(pDescriptorSets[i]);

	VkResult ret = ObjDisp(device)->FreeDescriptorSets(Unwrap(device), Unwrap(descriptorPool), count, unwrapped);

	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
			GetResourceManager()->ReleaseWrappedResource(pDescriptorSets[i]);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkUpdateDescriptorSets(
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
			ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &writeDesc, 0, NULL);
		else
			ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 0, NULL, 1, &copyDesc);
	}

	return true;
}

VkResult WrappedVulkan::vkUpdateDescriptorSets(
		VkDevice                                    device,
		uint32_t                                    writeCount,
		const VkWriteDescriptorSet*                 pDescriptorWrites,
		uint32_t                                    copyCount,
		const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	VkResult ret = VK_SUCCESS;
	
	{
		// VKTODOLOW this should be a persistent per-thread array that resizes up
		// to a high water mark, so we don't have to allocate
		vector<VkDescriptorInfo> desc;

		uint32_t numInfos = 0;
		for(uint32_t i=0; i < writeCount; i++) numInfos += pDescriptorWrites[i].count;

		// ensure we don't resize while looping so we can take pointers
		desc.resize(numInfos);

		VkWriteDescriptorSet *unwrappedWrites = new VkWriteDescriptorSet[writeCount];
		VkCopyDescriptorSet *unwrappedCopies = new VkCopyDescriptorSet[copyCount];
		
		uint32_t curInfo = 0;
		for(uint32_t i=0; i < writeCount; i++)
		{
			unwrappedWrites[i] = pDescriptorWrites[i];
			unwrappedWrites[i].destSet = Unwrap(unwrappedWrites[i].destSet);

			VkDescriptorInfo *unwrappedInfos = &desc[curInfo];
			curInfo += pDescriptorWrites[i].count;

			for(uint32_t j=0; j < pDescriptorWrites[i].count; j++)
			{
				unwrappedInfos[j] = unwrappedWrites[i].pDescriptors[j];
				unwrappedInfos[j].bufferView = Unwrap(unwrappedInfos[j].bufferView);
				unwrappedInfos[j].sampler = Unwrap(unwrappedInfos[j].sampler);
				unwrappedInfos[j].imageView = Unwrap(unwrappedInfos[j].imageView);
				unwrappedInfos[j].attachmentView = Unwrap(unwrappedInfos[j].attachmentView);
			}
			
			unwrappedWrites[i].pDescriptors = unwrappedInfos;
		}

		for(uint32_t i=0; i < copyCount; i++)
		{
			unwrappedCopies[i] = pDescriptorCopies[i];
			unwrappedCopies[i].destSet = Unwrap(unwrappedCopies[i].destSet);
			unwrappedCopies[i].srcSet = Unwrap(unwrappedCopies[i].srcSet);
		}

		ret = ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), writeCount, unwrappedWrites, copyCount, unwrappedCopies);
		
		SAFE_DELETE_ARRAY(unwrappedWrites);
		SAFE_DELETE_ARRAY(unwrappedCopies);
	}

	if(ret == VK_SUCCESS)
	{
		if(m_State == WRITING_CAPFRAME)
		{
			for(uint32_t i=0; i < writeCount; i++)
			{
				{
					SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
					Serialise_vkUpdateDescriptorSets(device, 1, &pDescriptorWrites[i], 0, NULL);

					m_FrameCaptureRecord->AddChunk(scope.Get());
				}

				// don't have to mark referenced any of the resources pointed to by the descriptor set - that's handled
				// on queue submission by marking ref'd all the current bindings of the sets referenced by the cmd buffer
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].destSet), eFrameRef_Write);
			}

			for(uint32_t i=0; i < copyCount; i++)
			{
				{
					SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
					Serialise_vkUpdateDescriptorSets(device, 0, NULL, 1, &pDescriptorCopies[i]);

					m_FrameCaptureRecord->AddChunk(scope.Get());
				}
				
				// don't have to mark referenced any of the resources pointed to by the descriptor sets - that's handled
				// on queue submission by marking ref'd all the current bindings of the sets referenced by the cmd buffer
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].destSet), eFrameRef_Write);
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].srcSet), eFrameRef_Read);
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

					if(bind.attachmentView != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.attachmentView));
					if(bind.bufferView != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.bufferView));
					if(bind.imageView != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.imageView));
					if(bind.sampler != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.sampler));

					bind = pDescriptorWrites[i].pDescriptors[d];

					if(bind.attachmentView != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.attachmentView), ref);
					if(bind.bufferView != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.bufferView), ref);
					if(bind.imageView != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.imageView), ref);
					if(bind.sampler != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.sampler), ref);
				}
			}

			if(copyCount > 0)
			{
				// don't want to implement this blindly
				RDCUNIMPLEMENTED("Copying descriptors not implemented");
			}
		}
	}

	return ret;
}
