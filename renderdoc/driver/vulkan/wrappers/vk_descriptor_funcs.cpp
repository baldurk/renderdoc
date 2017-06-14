/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

bool WrappedVulkan::Serialise_vkCreateDescriptorPool(Serialiser *localSerialiser, VkDevice device,
                                                     const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkDescriptorPool *pDescriptorPool)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkDescriptorPoolCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorPool));

  if(m_State == READING)
  {
    VkDescriptorPool pool = VK_NULL_HANDLE;

    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

    VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &info, NULL, &pool);

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

VkResult WrappedVulkan::vkCreateDescriptorPool(VkDevice device,
                                               const VkDescriptorPoolCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkDescriptorPool *pDescriptorPool)
{
  VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pCreateInfo, pAllocator,
                                                       pDescriptorPool);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorPool);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_POOL);
        Serialise_vkCreateDescriptorPool(localSerialiser, device, pCreateInfo, NULL, pDescriptorPool);

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
    Serialiser *localSerialiser, VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkDescriptorSetLayoutCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSetLayout));

  if(m_State == READING)
  {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

    VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &info, NULL, &layout);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(layout)))
      {
        live = GetResourceManager()->GetNonDispWrapper(layout)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyDescriptorSetLayout(Unwrap(device), layout, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), layout);
        GetResourceManager()->AddLiveResource(id, layout);

        m_CreationInfo.m_DescSetLayout[live].Init(GetResourceManager(), m_CreationInfo, &info);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorSetLayout(VkDevice device,
                                                    const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkDescriptorSetLayout *pSetLayout)
{
  size_t tempmemSize = sizeof(VkDescriptorSetLayoutBinding) * pCreateInfo->bindingCount;

  // need to count how many VkSampler arrays to allocate for
  for(uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
    if(pCreateInfo->pBindings[i].pImmutableSamplers)
      tempmemSize += pCreateInfo->pBindings[i].descriptorCount * sizeof(VkSampler);

  byte *memory = GetTempMemory(tempmemSize);

  VkDescriptorSetLayoutBinding *unwrapped = (VkDescriptorSetLayoutBinding *)memory;
  VkSampler *nextSampler = (VkSampler *)(unwrapped + pCreateInfo->bindingCount);

  for(uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
  {
    unwrapped[i] = pCreateInfo->pBindings[i];

    if(unwrapped[i].pImmutableSamplers)
    {
      VkSampler *unwrappedSamplers = nextSampler;
      nextSampler += unwrapped[i].descriptorCount;
      for(uint32_t j = 0; j < unwrapped[i].descriptorCount; j++)
        unwrappedSamplers[j] = Unwrap(unwrapped[i].pImmutableSamplers[j]);
      unwrapped[i].pImmutableSamplers = unwrappedSamplers;
    }
  }

  VkDescriptorSetLayoutCreateInfo unwrappedInfo = *pCreateInfo;
  unwrappedInfo.pBindings = unwrapped;
  VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrappedInfo,
                                                            pAllocator, pSetLayout);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSetLayout);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_SET_LAYOUT);
        Serialise_vkCreateDescriptorSetLayout(localSerialiser, device, pCreateInfo, NULL, pSetLayout);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSetLayout);
      record->AddChunk(chunk);

      record->descInfo = new DescriptorSetData();
      record->descInfo->layout = new DescSetLayout();
      record->descInfo->layout->Init(GetResourceManager(), m_CreationInfo, pCreateInfo);

      for(uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
      {
        bool usesSampler =
            pCreateInfo->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            pCreateInfo->pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        if(usesSampler && pCreateInfo->pBindings[i].pImmutableSamplers != NULL)
        {
          for(uint32_t d = 0; d < pCreateInfo->pBindings[i].descriptorCount; d++)
            record->AddParent(GetRecord(pCreateInfo->pBindings[i].pImmutableSamplers[d]));
        }
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pSetLayout);

      m_CreationInfo.m_DescSetLayout[id].Init(GetResourceManager(), m_CreationInfo, &unwrappedInfo);
    }
  }

  return ret;
}
bool WrappedVulkan::Serialise_vkAllocateDescriptorSets(Serialiser *localSerialiser, VkDevice device,
                                                       const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                       VkDescriptorSet *pDescriptorSets)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkDescriptorSetAllocateInfo, allocInfo, *pAllocateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorSets));

  if(m_State == READING)
  {
    VkDescriptorSet descset = VK_NULL_HANDLE;

    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

    VkResult ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &allocInfo, &descset);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), descset);
      GetResourceManager()->AddLiveResource(id, descset);

      ResourceId layoutId = GetResourceManager()->GetNonDispWrapper(allocInfo.pSetLayouts[0])->id;

      // this is stored in the resource record on capture, we need to be able to look to up
      m_DescriptorSetState[live].layout = layoutId;
      m_CreationInfo.m_DescSetLayout[layoutId].CreateBindingsArray(
          m_DescriptorSetState[live].currentBindings);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkAllocateDescriptorSets(VkDevice device,
                                                 const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                 VkDescriptorSet *pDescriptorSets)
{
  size_t tempmemSize = sizeof(VkDescriptorSetAllocateInfo) +
                       sizeof(VkDescriptorSetLayout) * pAllocateInfo->descriptorSetCount;

  byte *memory = GetTempMemory(tempmemSize);

  VkDescriptorSetAllocateInfo *unwrapped = (VkDescriptorSetAllocateInfo *)memory;
  VkDescriptorSetLayout *layouts = (VkDescriptorSetLayout *)(unwrapped + 1);

  *unwrapped = *pAllocateInfo;
  unwrapped->pSetLayouts = layouts;
  unwrapped->descriptorPool = Unwrap(unwrapped->descriptorPool);
  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
    layouts[i] = Unwrap(pAllocateInfo->pSetLayouts[i]);

  VkResult ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), unwrapped, pDescriptorSets);

  if(ret != VK_SUCCESS)
    return ret;

  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        VkDescriptorSetAllocateInfo info = *pAllocateInfo;
        info.descriptorSetCount = 1;
        info.pSetLayouts += i;

        SCOPED_SERIALISE_CONTEXT(ALLOC_DESC_SET);
        Serialise_vkAllocateDescriptorSets(localSerialiser, device, &info, &pDescriptorSets[i]);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pDescriptorSets[i]);
      record->AddChunk(chunk);

      ResourceId layoutID = GetResID(pAllocateInfo->pSetLayouts[i]);
      VkResourceRecord *layoutRecord = GetRecord(pAllocateInfo->pSetLayouts[i]);

      VkResourceRecord *poolrecord = GetRecord(pAllocateInfo->descriptorPool);

      {
        poolrecord->LockChunks();
        poolrecord->pooledChildren.push_back(record);
        poolrecord->UnlockChunks();
      }

      record->pool = poolrecord;

      record->AddParent(poolrecord);
      record->AddParent(GetResourceManager()->GetResourceRecord(layoutID));

      // just always treat descriptor sets as dirty
      {
        SCOPED_LOCK(m_CapTransitionLock);
        if(m_State != WRITING_CAPFRAME)
          GetResourceManager()->MarkDirtyResource(id);
        else
          GetResourceManager()->MarkPendingDirty(id);
      }

      record->descInfo = new DescriptorSetData();
      record->descInfo->layout = layoutRecord->descInfo->layout;
      record->descInfo->layout->CreateBindingsArray(record->descInfo->descBindings);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, pDescriptorSets[i]);

      m_DescriptorSetState[id].layout = GetResID(pAllocateInfo->pSetLayouts[i]);
    }
  }

  return ret;
}

VkResult WrappedVulkan::vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
                                             uint32_t count, const VkDescriptorSet *pDescriptorSets)
{
  VkDescriptorSet *unwrapped = GetTempArray<VkDescriptorSet>(count);
  for(uint32_t i = 0; i < count; i++)
    unwrapped[i] = Unwrap(pDescriptorSets[i]);

  for(uint32_t i = 0; i < count; i++)
    GetResourceManager()->ReleaseWrappedResource(pDescriptorSets[i]);

  VkResult ret =
      ObjDisp(device)->FreeDescriptorSets(Unwrap(device), Unwrap(descriptorPool), count, unwrapped);

  return ret;
}

VkResult WrappedVulkan::vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                              VkDescriptorPoolResetFlags flags)
{
  // need to free all child descriptor pools. Application is responsible for
  // ensuring no concurrent use with alloc/free from this pool, the same as
  // for DestroyDescriptorPool.
  VkResourceRecord *record = GetRecord(descriptorPool);

  // delete all of the children
  for(auto it = record->pooledChildren.begin(); it != record->pooledChildren.end(); ++it)
  {
    // unset record->pool so we don't recurse
    (*it)->pool = NULL;
    GetResourceManager()->ReleaseWrappedResource((VkDescriptorSet)(uint64_t)(*it)->Resource, true);
  }
  record->pooledChildren.clear();

  return ObjDisp(device)->ResetDescriptorPool(Unwrap(device), Unwrap(descriptorPool), flags);
}

bool WrappedVulkan::Serialise_vkUpdateDescriptorSets(Serialiser *localSerialiser, VkDevice device,
                                                     uint32_t writeCount,
                                                     const VkWriteDescriptorSet *pDescriptorWrites,
                                                     uint32_t copyCount,
                                                     const VkCopyDescriptorSet *pDescriptorCopies)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(bool, writes, writeCount == 1);

  VkWriteDescriptorSet writeDesc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, 0};
  VkCopyDescriptorSet copyDesc = {VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, 0};
  if(writes)
  {
    SERIALISE_ELEMENT(VkWriteDescriptorSet, w, *pDescriptorWrites);
    writeDesc = w;
    // take ownership of the arrays (we will delete manually)
    w.pBufferInfo = NULL;
    w.pImageInfo = NULL;
    w.pTexelBufferView = NULL;
  }
  else
  {
    SERIALISE_ELEMENT(VkCopyDescriptorSet, c, *pDescriptorCopies);
    copyDesc = c;
  }

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

    if(writes)
    {
      // check for validity - if a resource wasn't referenced other than in this update
      // (ie. the descriptor set was overwritten or never bound), then the write descriptor
      // will be invalid with some missing handles. It's safe though to just skip this
      // update as we only get here if it's never used.

      // if a set was never bound, it will have been omitted and we just drop any writes to it
      bool valid = (writeDesc.dstSet != VK_NULL_HANDLE);

      if(!valid)
        return true;

      const DescSetLayout &layout =
          m_CreationInfo.m_DescSetLayout
              [m_DescriptorSetState[GetResourceManager()->GetNonDispWrapper(writeDesc.dstSet)->id].layout];

      const DescSetLayout::Binding *layoutBinding = &layout.bindings[writeDesc.dstBinding];
      uint32_t curIdx = writeDesc.dstArrayElement;

      switch(writeDesc.descriptorType)
      {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        {
          for(uint32_t i = 0; i < writeDesc.descriptorCount; i++)
            valid &= (writeDesc.pImageInfo[i].sampler != VK_NULL_HANDLE);
          break;
        }
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        {
          for(uint32_t i = 0; i < writeDesc.descriptorCount; i++, curIdx++)
          {
            // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
            // explanation
            if(curIdx >= layoutBinding->descriptorCount)
            {
              layoutBinding++;
              curIdx = 0;
            }

            valid &= (writeDesc.pImageInfo[i].sampler != VK_NULL_HANDLE) ||
                     (layoutBinding->immutableSampler &&
                      layoutBinding->immutableSampler[curIdx] != ResourceId());
            valid &= (writeDesc.pImageInfo[i].imageView != VK_NULL_HANDLE);
          }
          break;
        }
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        {
          for(uint32_t i = 0; i < writeDesc.descriptorCount; i++)
            valid &= (writeDesc.pImageInfo[i].imageView != VK_NULL_HANDLE);
          break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        {
          for(uint32_t i = 0; i < writeDesc.descriptorCount; i++)
            valid &= (writeDesc.pTexelBufferView[i] != VK_NULL_HANDLE);
          break;
        }
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        {
          for(uint32_t i = 0; i < writeDesc.descriptorCount; i++)
            valid &= (writeDesc.pBufferInfo[i].buffer != VK_NULL_HANDLE);
          break;
        }
        default: RDCERR("Unexpected descriptor type %d", writeDesc.descriptorType);
      }

      if(valid)
      {
        ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &writeDesc, 0, NULL);

        // update our local tracking
        vector<DescriptorSetSlot *> &bindings =
            m_DescriptorSetState[GetResourceManager()->GetNonDispWrapper(writeDesc.dstSet)->id]
                .currentBindings;

        {
          RDCASSERT(writeDesc.dstBinding < bindings.size());

          DescriptorSetSlot **bind = &bindings[writeDesc.dstBinding];
          layoutBinding = &layout.bindings[writeDesc.dstBinding];
          curIdx = writeDesc.dstArrayElement;

          if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
             writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
          {
            for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
            {
              // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
              // explanation
              if(curIdx >= layoutBinding->descriptorCount)
              {
                layoutBinding++;
                bind++;
                curIdx = 0;
              }

              (*bind)[curIdx].texelBufferView = writeDesc.pTexelBufferView[d];
            }
          }
          else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                  writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                  writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                  writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                  writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
          {
            for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
            {
              // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
              // explanation
              if(curIdx >= layoutBinding->descriptorCount)
              {
                layoutBinding++;
                bind++;
                curIdx = 0;
              }

              (*bind)[curIdx].imageInfo = writeDesc.pImageInfo[d];
            }
          }
          else
          {
            for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
            {
              // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
              // explanation
              if(curIdx >= layoutBinding->descriptorCount)
              {
                layoutBinding++;
                bind++;
                curIdx = 0;
              }

              (*bind)[curIdx].bufferInfo = writeDesc.pBufferInfo[d];
            }
          }
        }
      }
    }
    else
    {
      // if a set was never bound, it will have been omitted and we just drop any copies to it
      if(copyDesc.dstSet == VK_NULL_HANDLE || copyDesc.srcSet == VK_NULL_HANDLE)
        return true;

      ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 0, NULL, 1, &copyDesc);

      ResourceId dstSetId = GetResourceManager()->GetNonDispWrapper(copyDesc.dstSet)->id;
      ResourceId srcSetId = GetResourceManager()->GetNonDispWrapper(copyDesc.srcSet)->id;

      // update our local tracking
      vector<DescriptorSetSlot *> &dstbindings = m_DescriptorSetState[dstSetId].currentBindings;
      vector<DescriptorSetSlot *> &srcbindings = m_DescriptorSetState[srcSetId].currentBindings;

      {
        RDCASSERT(copyDesc.dstBinding < dstbindings.size());
        RDCASSERT(copyDesc.srcBinding < srcbindings.size());

        const DescSetLayout &dstlayout =
            m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[dstSetId].layout];
        const DescSetLayout &srclayout =
            m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[srcSetId].layout];

        const DescSetLayout::Binding *layoutSrcBinding = &srclayout.bindings[copyDesc.srcBinding];
        const DescSetLayout::Binding *layoutDstBinding = &dstlayout.bindings[copyDesc.dstBinding];

        DescriptorSetSlot **dstbind = &dstbindings[copyDesc.dstBinding];
        DescriptorSetSlot **srcbind = &srcbindings[copyDesc.srcBinding];

        uint32_t curDstIdx = copyDesc.dstArrayElement;
        uint32_t curSrcIdx = copyDesc.srcArrayElement;

        for(uint32_t d = 0; d < copyDesc.descriptorCount; d++, curSrcIdx++, curDstIdx++)
        {
          // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
          // explanation
          if(curSrcIdx >= layoutSrcBinding->descriptorCount)
          {
            layoutSrcBinding++;
            srcbind++;
            curSrcIdx = 0;
          }

          // src and dst could wrap independently - think copying from
          // { sampler2D, sampler2D[4], sampler2D } to a { sampler2D[3], sampler2D[3] }
          // or copying from different starting array elements
          if(curDstIdx >= layoutDstBinding->descriptorCount)
          {
            layoutDstBinding++;
            dstbind++;
            curDstIdx = 0;
          }

          (*dstbind)[curDstIdx] = (*srcbind)[curSrcIdx];
        }
      }
    }

    // delete serialised descriptors array
    delete[] writeDesc.pBufferInfo;
    delete[] writeDesc.pImageInfo;
    delete[] writeDesc.pTexelBufferView;
  }

  return true;
}

void WrappedVulkan::vkUpdateDescriptorSets(VkDevice device, uint32_t writeCount,
                                           const VkWriteDescriptorSet *pDescriptorWrites,
                                           uint32_t copyCount,
                                           const VkCopyDescriptorSet *pDescriptorCopies)
{
  SCOPED_DBG_SINK();

  {
    // need to count up number of descriptor infos, to be able to alloc enough space
    uint32_t numInfos = 0;
    for(uint32_t i = 0; i < writeCount; i++)
      numInfos += pDescriptorWrites[i].descriptorCount;

    byte *memory = GetTempMemory(sizeof(VkDescriptorBufferInfo) * numInfos +
                                 sizeof(VkWriteDescriptorSet) * writeCount +
                                 sizeof(VkCopyDescriptorSet) * copyCount);

    RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                      "Descriptor structs sizes are unexpected, ensure largest size is used");

    VkWriteDescriptorSet *unwrappedWrites = (VkWriteDescriptorSet *)memory;
    VkCopyDescriptorSet *unwrappedCopies = (VkCopyDescriptorSet *)(unwrappedWrites + writeCount);
    VkDescriptorBufferInfo *nextDescriptors = (VkDescriptorBufferInfo *)(unwrappedCopies + copyCount);

    for(uint32_t i = 0; i < writeCount; i++)
    {
      unwrappedWrites[i] = pDescriptorWrites[i];
      unwrappedWrites[i].dstSet = Unwrap(unwrappedWrites[i].dstSet);

      VkDescriptorBufferInfo *bufInfos = nextDescriptors;
      VkDescriptorImageInfo *imInfos = (VkDescriptorImageInfo *)bufInfos;
      VkBufferView *bufViews = (VkBufferView *)bufInfos;
      nextDescriptors += pDescriptorWrites[i].descriptorCount;

      // unwrap and assign the appropriate array
      if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        unwrappedWrites[i].pTexelBufferView = (VkBufferView *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
          bufViews[j] = Unwrap(pDescriptorWrites[i].pTexelBufferView[j]);
      }
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        unwrappedWrites[i].pImageInfo = (VkDescriptorImageInfo *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          imInfos[j].imageView = Unwrap(pDescriptorWrites[i].pImageInfo[j].imageView);
          imInfos[j].sampler = Unwrap(pDescriptorWrites[i].pImageInfo[j].sampler);
          imInfos[j].imageLayout = pDescriptorWrites[i].pImageInfo[j].imageLayout;
        }
      }
      else
      {
        unwrappedWrites[i].pBufferInfo = bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          bufInfos[j].buffer = Unwrap(pDescriptorWrites[i].pBufferInfo[j].buffer);
          bufInfos[j].offset = pDescriptorWrites[i].pBufferInfo[j].offset;
          bufInfos[j].range = pDescriptorWrites[i].pBufferInfo[j].range;
        }
      }
    }

    for(uint32_t i = 0; i < copyCount; i++)
    {
      unwrappedCopies[i] = pDescriptorCopies[i];
      unwrappedCopies[i].dstSet = Unwrap(unwrappedCopies[i].dstSet);
      unwrappedCopies[i].srcSet = Unwrap(unwrappedCopies[i].srcSet);
    }

    ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), writeCount, unwrappedWrites, copyCount,
                                          unwrappedCopies);
  }

  bool capframe = false;
  {
    SCOPED_LOCK(m_CapTransitionLock);
    capframe = (m_State == WRITING_CAPFRAME);
  }

  if(capframe)
  {
    // don't have to mark referenced any of the resources pointed to by the descriptor set - that's
    // handled
    // on queue submission by marking ref'd all the current bindings of the sets referenced by the
    // cmd buffer

    for(uint32_t i = 0; i < writeCount; i++)
    {
      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
        Serialise_vkUpdateDescriptorSets(localSerialiser, device, 1, &pDescriptorWrites[i], 0, NULL);

        m_FrameCaptureRecord->AddChunk(scope.Get());
      }

      // as long as descriptor sets are forced to have initial states, we don't have to mark them
      // ref'd for
      // write here. The reason being that as long as we only mark them as ref'd when they're
      // actually bound,
      // we can safely skip the ref here and it means any descriptor set updates of descriptor sets
      // that are
      // never used in the frame can be ignored.
      // GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].destSet),
      // eFrameRef_Write);
    }

    for(uint32_t i = 0; i < copyCount; i++)
    {
      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
        Serialise_vkUpdateDescriptorSets(localSerialiser, device, 0, NULL, 1, &pDescriptorCopies[i]);

        m_FrameCaptureRecord->AddChunk(scope.Get());
      }

      // Like writes we don't have to mark the written descriptor set as used because unless it's
      // bound somewhere
      // we don't need it anyway. However we DO have to mark the source set as used because it
      // doesn't have to
      // be bound to still be needed (think about if the dest set is bound somewhere after this copy
      // - what refs
      // the source set?).
      // At the same time as ref'ing the source set, we must ref all of its resources (via the
      // bindFrameRefs).
      // We just ref all rather than looking at only the copied sets to keep things simple.
      // This does mean a slightly conservative ref'ing if the dest set doesn't end up getting
      // bound, but we only
      // do this during frame capture so it's not too bad.
      // GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].destSet),
      // eFrameRef_Write);

      {
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].srcSet),
                                                          eFrameRef_Read);

        VkResourceRecord *setrecord = GetRecord(pDescriptorCopies[i].srcSet);

        for(auto refit = setrecord->descInfo->bindFrameRefs.begin();
            refit != setrecord->descInfo->bindFrameRefs.end(); ++refit)
        {
          GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second.second);

          if(refit->second.first & DescriptorSetData::SPARSE_REF_BIT)
          {
            VkResourceRecord *record = GetResourceManager()->GetResourceRecord(refit->first);

            GetResourceManager()->MarkSparseMapReferenced(record->sparseInfo);
          }
        }
      }
    }
  }

  // need to track descriptor set contents whether capframing or idle
  if(m_State >= WRITING)
  {
    for(uint32_t i = 0; i < writeCount; i++)
    {
      VkResourceRecord *record = GetRecord(pDescriptorWrites[i].dstSet);
      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      RDCASSERT(pDescriptorWrites[i].dstBinding < record->descInfo->descBindings.size());

      DescriptorSetSlot **binding = &record->descInfo->descBindings[pDescriptorWrites[i].dstBinding];

      const DescSetLayout::Binding *layoutBinding = &layout.bindings[pDescriptorWrites[i].dstBinding];

      FrameRefType ref = eFrameRef_Write;

      switch(layoutBinding->descriptorType)
      {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: ref = eFrameRef_Read; break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: ref = eFrameRef_Write; break;
        default: RDCERR("Unexpected descriptor type");
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

      // start at the dstArrayElement
      uint32_t curIdx = pDescriptorWrites[i].dstArrayElement;

      for(uint32_t d = 0; d < pDescriptorWrites[i].descriptorCount; d++, curIdx++)
      {
        // roll over onto the next binding, on the assumption that it is the same
        // type and there is indeed a next binding at all. See spec language:
        //
        // If the dstBinding has fewer than descriptorCount array elements remaining starting from
        // dstArrayElement, then the remainder will be used to update the subsequent binding -
        // dstBinding+1 starting at array element zero. This behavior applies recursively, with the
        // update affecting consecutive bindings as needed to update all descriptorCount
        // descriptors. All consecutive bindings updated via a single VkWriteDescriptorSet structure
        // must have identical descriptorType and stageFlags, and must all either use immutable
        // samplers or must all not use immutable samplers.

        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          binding++;
          curIdx = 0;
        }

        DescriptorSetSlot &bind = (*binding)[curIdx];

        if(bind.texelBufferView != VK_NULL_HANDLE)
        {
          record->RemoveBindFrameRef(GetResID(bind.texelBufferView));

          VkResourceRecord *viewRecord = GetRecord(bind.texelBufferView);
          if(viewRecord && viewRecord->baseResource != ResourceId())
            record->RemoveBindFrameRef(viewRecord->baseResource);
        }
        if(bind.imageInfo.imageView != VK_NULL_HANDLE)
        {
          record->RemoveBindFrameRef(GetResID(bind.imageInfo.imageView));

          VkResourceRecord *viewRecord = GetRecord(bind.imageInfo.imageView);
          if(viewRecord)
          {
            record->RemoveBindFrameRef(viewRecord->baseResource);
            if(viewRecord->baseResourceMem != ResourceId())
              record->RemoveBindFrameRef(viewRecord->baseResourceMem);
          }
        }
        if(bind.imageInfo.sampler != VK_NULL_HANDLE)
        {
          record->RemoveBindFrameRef(GetResID(bind.imageInfo.sampler));
        }
        if(bind.bufferInfo.buffer != VK_NULL_HANDLE)
        {
          record->RemoveBindFrameRef(GetResID(bind.bufferInfo.buffer));

          VkResourceRecord *bufRecord = GetRecord(bind.bufferInfo.buffer);
          if(bufRecord && bufRecord->baseResource != ResourceId())
            record->RemoveBindFrameRef(bufRecord->baseResource);
        }

        // NULL everything out now so that we don't accidentally reference an object
        // that was removed already
        bind.texelBufferView = VK_NULL_HANDLE;
        bind.bufferInfo.buffer = VK_NULL_HANDLE;
        bind.imageInfo.imageView = VK_NULL_HANDLE;
        bind.imageInfo.sampler = VK_NULL_HANDLE;

        if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind.texelBufferView = pDescriptorWrites[i].pTexelBufferView[d];
        }
        else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          bind.imageInfo = pDescriptorWrites[i].pImageInfo[d];

          // ignore descriptors not part of the write, by NULL'ing out those members
          // as they might not even point to a valid object
          if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
            bind.imageInfo.imageView = VK_NULL_HANDLE;
          else if(pDescriptorWrites[i].descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            bind.imageInfo.sampler = VK_NULL_HANDLE;
        }
        else
        {
          bind.bufferInfo = pDescriptorWrites[i].pBufferInfo[d];
        }

        if(bind.texelBufferView != VK_NULL_HANDLE)
        {
          record->AddBindFrameRef(GetResID(bind.texelBufferView), eFrameRef_Read,
                                  GetRecord(bind.texelBufferView)->sparseInfo != NULL);
          if(GetRecord(bind.texelBufferView)->baseResource != ResourceId())
            record->AddBindFrameRef(GetRecord(bind.texelBufferView)->baseResource, ref);
        }
        if(bind.imageInfo.imageView != VK_NULL_HANDLE)
        {
          record->AddBindFrameRef(GetResID(bind.imageInfo.imageView), eFrameRef_Read,
                                  GetRecord(bind.imageInfo.imageView)->sparseInfo != NULL);
          record->AddBindFrameRef(GetRecord(bind.imageInfo.imageView)->baseResource, ref);
          if(GetRecord(bind.imageInfo.imageView)->baseResourceMem != ResourceId())
            record->AddBindFrameRef(GetRecord(bind.imageInfo.imageView)->baseResourceMem,
                                    eFrameRef_Read);
        }
        if(bind.imageInfo.sampler != VK_NULL_HANDLE)
        {
          record->AddBindFrameRef(GetResID(bind.imageInfo.sampler), eFrameRef_Read);
        }
        if(bind.bufferInfo.buffer != VK_NULL_HANDLE)
        {
          record->AddBindFrameRef(GetResID(bind.bufferInfo.buffer), eFrameRef_Read,
                                  GetRecord(bind.bufferInfo.buffer)->sparseInfo != NULL);
          if(GetRecord(bind.bufferInfo.buffer)->baseResource != ResourceId())
            record->AddBindFrameRef(GetRecord(bind.bufferInfo.buffer)->baseResource, ref);
        }
      }
    }

    // this is almost identical to the above loop, except that instead of sourcing the descriptors
    // from the writedescriptor struct, we source it from our stored bindings on the source
    // descrpitor set

    for(uint32_t i = 0; i < copyCount; i++)
    {
      VkResourceRecord *dstrecord = GetRecord(pDescriptorCopies[i].dstSet);
      RDCASSERT(dstrecord->descInfo && dstrecord->descInfo->layout);
      const DescSetLayout &dstlayout = *dstrecord->descInfo->layout;

      VkResourceRecord *srcrecord = GetRecord(pDescriptorCopies[i].srcSet);
      RDCASSERT(srcrecord->descInfo && srcrecord->descInfo->layout);
      const DescSetLayout &srclayout = *srcrecord->descInfo->layout;

      RDCASSERT(pDescriptorCopies[i].dstBinding < dstrecord->descInfo->descBindings.size());
      RDCASSERT(pDescriptorCopies[i].srcBinding < srcrecord->descInfo->descBindings.size());

      DescriptorSetSlot **dstbinding =
          &dstrecord->descInfo->descBindings[pDescriptorCopies[i].dstBinding];
      DescriptorSetSlot **srcbinding =
          &srcrecord->descInfo->descBindings[pDescriptorCopies[i].srcBinding];

      const DescSetLayout::Binding *dstlayoutBinding =
          &dstlayout.bindings[pDescriptorCopies[i].dstBinding];
      const DescSetLayout::Binding *srclayoutBinding =
          &srclayout.bindings[pDescriptorCopies[i].srcBinding];

      FrameRefType ref = eFrameRef_Write;

      switch(dstlayoutBinding->descriptorType)
      {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: ref = eFrameRef_Read; break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: ref = eFrameRef_Write; break;
        default: RDCERR("Unexpected descriptor type");
      }

      // allow roll-over between consecutive bindings. See above in the plain write case for more
      // explanation
      uint32_t curSrcIdx = pDescriptorCopies[i].srcArrayElement;
      uint32_t curDstIdx = pDescriptorCopies[i].dstArrayElement;

      for(uint32_t d = 0; d < pDescriptorCopies[i].descriptorCount; d++, curSrcIdx++, curDstIdx++)
      {
        if(curDstIdx >= dstlayoutBinding->descriptorCount)
        {
          dstlayoutBinding++;
          dstbinding++;
          curDstIdx = 0;
        }

        // dst and src indices must roll-over independently
        if(curSrcIdx >= srclayoutBinding->descriptorCount)
        {
          srclayoutBinding++;
          srcbinding++;
          curSrcIdx = 0;
        }

        DescriptorSetSlot &bind = (*dstbinding)[curDstIdx];

        if(bind.texelBufferView != VK_NULL_HANDLE)
        {
          dstrecord->RemoveBindFrameRef(GetResID(bind.texelBufferView));
          if(GetRecord(bind.texelBufferView)->baseResource != ResourceId())
            dstrecord->RemoveBindFrameRef(GetRecord(bind.texelBufferView)->baseResource);
        }
        if(bind.imageInfo.imageView != VK_NULL_HANDLE)
        {
          dstrecord->RemoveBindFrameRef(GetResID(bind.imageInfo.imageView));
          dstrecord->RemoveBindFrameRef(GetRecord(bind.imageInfo.imageView)->baseResource);
          if(GetRecord(bind.imageInfo.imageView)->baseResourceMem != ResourceId())
            dstrecord->RemoveBindFrameRef(GetRecord(bind.imageInfo.imageView)->baseResourceMem);
        }
        if(bind.imageInfo.sampler != VK_NULL_HANDLE)
        {
          dstrecord->RemoveBindFrameRef(GetResID(bind.imageInfo.sampler));
        }
        if(bind.bufferInfo.buffer != VK_NULL_HANDLE)
        {
          dstrecord->RemoveBindFrameRef(GetResID(bind.bufferInfo.buffer));
          if(GetRecord(bind.bufferInfo.buffer)->baseResource != ResourceId())
            dstrecord->RemoveBindFrameRef(GetRecord(bind.bufferInfo.buffer)->baseResource);
        }

        bind = (*srcbinding)[curSrcIdx];

        if(bind.texelBufferView != VK_NULL_HANDLE)
        {
          dstrecord->AddBindFrameRef(GetResID(bind.texelBufferView), eFrameRef_Read,
                                     GetRecord(bind.texelBufferView)->sparseInfo != NULL);
          if(GetRecord(bind.texelBufferView)->baseResource != ResourceId())
            dstrecord->AddBindFrameRef(GetRecord(bind.texelBufferView)->baseResource, ref);
        }
        if(bind.imageInfo.imageView != VK_NULL_HANDLE)
        {
          dstrecord->AddBindFrameRef(GetResID(bind.imageInfo.imageView), eFrameRef_Read,
                                     GetRecord(bind.imageInfo.imageView)->sparseInfo != NULL);
          dstrecord->AddBindFrameRef(GetRecord(bind.imageInfo.imageView)->baseResource, ref);
          if(GetRecord(bind.imageInfo.imageView)->baseResourceMem != ResourceId())
            dstrecord->AddBindFrameRef(GetRecord(bind.imageInfo.imageView)->baseResourceMem,
                                       eFrameRef_Read);
        }
        if(bind.imageInfo.sampler != VK_NULL_HANDLE)
        {
          dstrecord->AddBindFrameRef(GetResID(bind.imageInfo.sampler), ref);
        }
        if(bind.bufferInfo.buffer != VK_NULL_HANDLE)
        {
          dstrecord->AddBindFrameRef(GetResID(bind.bufferInfo.buffer), eFrameRef_Read,
                                     GetRecord(bind.bufferInfo.buffer)->sparseInfo != NULL);
          if(GetRecord(bind.bufferInfo.buffer)->baseResource != ResourceId())
            dstrecord->AddBindFrameRef(GetRecord(bind.bufferInfo.buffer)->baseResource, ref);
        }
      }
    }
  }
}
