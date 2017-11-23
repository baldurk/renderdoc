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

template <>
VkDescriptorSetLayoutCreateInfo WrappedVulkan::UnwrapInfo(const VkDescriptorSetLayoutCreateInfo *info)
{
  VkDescriptorSetLayoutCreateInfo ret = *info;

  size_t tempmemSize = sizeof(VkDescriptorSetLayoutBinding) * info->bindingCount;

  // need to count how many VkSampler arrays to allocate for
  for(uint32_t i = 0; i < info->bindingCount; i++)
    if(info->pBindings[i].pImmutableSamplers)
      tempmemSize += info->pBindings[i].descriptorCount * sizeof(VkSampler);

  byte *memory = GetTempMemory(tempmemSize);

  VkDescriptorSetLayoutBinding *unwrapped = (VkDescriptorSetLayoutBinding *)memory;
  VkSampler *nextSampler = (VkSampler *)(unwrapped + info->bindingCount);

  for(uint32_t i = 0; i < info->bindingCount; i++)
  {
    unwrapped[i] = info->pBindings[i];

    if(unwrapped[i].pImmutableSamplers)
    {
      VkSampler *unwrappedSamplers = nextSampler;
      nextSampler += unwrapped[i].descriptorCount;
      for(uint32_t j = 0; j < unwrapped[i].descriptorCount; j++)
        unwrappedSamplers[j] = Unwrap(unwrapped[i].pImmutableSamplers[j]);
      unwrapped[i].pImmutableSamplers = unwrappedSamplers;
    }
  }

  ret.pBindings = unwrapped;

  return ret;
}

template <>
VkDescriptorSetAllocateInfo WrappedVulkan::UnwrapInfo(const VkDescriptorSetAllocateInfo *info)
{
  VkDescriptorSetAllocateInfo ret = *info;

  VkDescriptorSetLayout *layouts = GetTempArray<VkDescriptorSetLayout>(info->descriptorSetCount);

  ret.descriptorPool = Unwrap(ret.descriptorPool);
  for(uint32_t i = 0; i < info->descriptorSetCount; i++)
    layouts[i] = Unwrap(info->pSetLayouts[i]);
  ret.pSetLayouts = layouts;

  return ret;
}

template <>
VkWriteDescriptorSet WrappedVulkan::UnwrapInfo(const VkWriteDescriptorSet *writeDesc)
{
  VkWriteDescriptorSet ret = *writeDesc;

  byte *memory = GetTempMemory(sizeof(VkDescriptorBufferInfo) * writeDesc->descriptorCount);

  VkDescriptorBufferInfo *bufInfos = (VkDescriptorBufferInfo *)memory;
  VkDescriptorImageInfo *imInfos = (VkDescriptorImageInfo *)memory;
  VkBufferView *bufViews = (VkBufferView *)memory;

  ret.dstSet = Unwrap(ret.dstSet);

  RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                    "Structure sizes mean not enough space is allocated for write data");
  RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkBufferView),
                    "Structure sizes mean not enough space is allocated for write data");

  // unwrap and assign the appropriate array
  if(ret.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
     ret.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
  {
    for(uint32_t j = 0; j < ret.descriptorCount; j++)
      bufViews[j] = Unwrap(ret.pTexelBufferView[j]);
    ret.pTexelBufferView = bufViews;
  }
  else if(ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
          ret.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
  {
    bool hasSampler = (ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                       ret.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    bool hasImage = (ret.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                     ret.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                     ret.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                     ret.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

    for(uint32_t j = 0; j < ret.descriptorCount; j++)
    {
      if(hasImage)
        imInfos[j].imageView = Unwrap(ret.pImageInfo[j].imageView);
      else
        imInfos[j].imageView = VK_NULL_HANDLE;

      if(hasSampler)
        imInfos[j].sampler = Unwrap(ret.pImageInfo[j].sampler);
      else
        imInfos[j].sampler = VK_NULL_HANDLE;

      imInfos[j].imageLayout = ret.pImageInfo[j].imageLayout;
    }
    ret.pImageInfo = imInfos;
  }
  else
  {
    for(uint32_t j = 0; j < ret.descriptorCount; j++)
    {
      bufInfos[j].buffer = Unwrap(ret.pBufferInfo[j].buffer);
      bufInfos[j].offset = ret.pBufferInfo[j].offset;
      bufInfos[j].range = ret.pBufferInfo[j].range;
    }
    ret.pBufferInfo = bufInfos;
  }

  return ret;
}

template <>
VkCopyDescriptorSet WrappedVulkan::UnwrapInfo(const VkCopyDescriptorSet *copyDesc)
{
  VkCopyDescriptorSet ret = *copyDesc;

  ret.dstSet = Unwrap(ret.dstSet);
  ret.srcSet = Unwrap(ret.srcSet);

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDescriptorPool(SerialiserType &ser, VkDevice device,
                                                     const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkDescriptorPool *pDescriptorPool)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_LOCAL(DescriptorPool, GetResID(*pDescriptorPool));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorPool pool = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &CreateInfo, NULL, &pool);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
      GetResourceManager()->AddLiveResource(DescriptorPool, pool);
    }

    AddResource(DescriptorPool, ResourceType::Pool, "Descriptor Pool");
    DerivedResource(device, DescriptorPool);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorPool(VkDevice device,
                                               const VkDescriptorPoolCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkDescriptorPool *pDescriptorPool)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pCreateInfo,
                                                                  pAllocator, pDescriptorPool));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorPool);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDescriptorPool);
        Serialise_vkCreateDescriptorPool(ser, device, pCreateInfo, NULL, pDescriptorPool);

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDescriptorSetLayout(
    SerialiserType &ser, VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_LOCAL(SetLayout, GetResID(*pSetLayout));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(&CreateInfo);
    VkResult ret =
        ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrapped, NULL, &layout);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
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
        GetResourceManager()->ReplaceResource(SetLayout, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), layout);
        GetResourceManager()->AddLiveResource(SetLayout, layout);

        m_CreationInfo.m_DescSetLayout[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }

      AddResource(SetLayout, ResourceType::ShaderBinding, "Descriptor Layout");
      DerivedResource(device, SetLayout);

      for(uint32_t i = 0; i < CreateInfo.bindingCount; i++)
      {
        bool usesSampler =
            CreateInfo.pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            CreateInfo.pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        if(usesSampler && CreateInfo.pBindings[i].pImmutableSamplers != NULL)
        {
          for(uint32_t d = 0; d < CreateInfo.pBindings[i].descriptorCount; d++)
            DerivedResource(CreateInfo.pBindings[i].pImmutableSamplers[d], SetLayout);
        }
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
  VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrapped,
                                                                       pAllocator, pSetLayout));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSetLayout);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDescriptorSetLayout);
        Serialise_vkCreateDescriptorSetLayout(ser, device, pCreateInfo, NULL, pSetLayout);

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

      m_CreationInfo.m_DescSetLayout[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkAllocateDescriptorSets(SerialiserType &ser, VkDevice device,
                                                       const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                       VkDescriptorSet *pDescriptorSets)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo);
  SERIALISE_ELEMENT_LOCAL(DescriptorSet, GetResID(*pDescriptorSets));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorSet descset = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo unwrapped = UnwrapInfo(&AllocateInfo);
    VkResult ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), descset);
      GetResourceManager()->AddLiveResource(DescriptorSet, descset);

      ResourceId layoutId = GetResID(AllocateInfo.pSetLayouts[0]);

      // this is stored in the resource record on capture, we need to be able to look to up
      m_DescriptorSetState[live].layout = layoutId;
      m_CreationInfo.m_DescSetLayout[layoutId].CreateBindingsArray(
          m_DescriptorSetState[live].currentBindings);
    }

    AddResource(DescriptorSet, ResourceType::ShaderBinding, "Descriptor Set");
    DerivedResource(device, DescriptorSet);
    DerivedResource(AllocateInfo.pSetLayouts[0], DescriptorSet);
  }

  return true;
}

VkResult WrappedVulkan::vkAllocateDescriptorSets(VkDevice device,
                                                 const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                 VkDescriptorSet *pDescriptorSets)
{
  VkDescriptorSetAllocateInfo unwrapped = UnwrapInfo(pAllocateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, pDescriptorSets));

  if(ret != VK_SUCCESS)
    return ret;

  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        VkDescriptorSetAllocateInfo info = *pAllocateInfo;
        info.descriptorSetCount = 1;
        info.pSetLayouts += i;

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkAllocateDescriptorSets);
        Serialise_vkAllocateDescriptorSets(ser, device, &info, &pDescriptorSets[i]);

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

      bool capframe = false;
      // just always treat descriptor sets as dirty
      {
        SCOPED_LOCK(m_CapTransitionLock);
        capframe = IsActiveCapturing(m_State);
      }

      if(capframe)
        GetResourceManager()->MarkPendingDirty(id);
      else
        GetResourceManager()->MarkDirtyResource(id);

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

void WrappedVulkan::ReplayDescriptorSetWrite(VkDevice device, const VkWriteDescriptorSet &writeDesc)
{
  // check for validity - if a resource wasn't referenced other than in this update
  // (ie. the descriptor set was overwritten or never bound), then the write descriptor
  // will be invalid with some missing handles. It's safe though to just skip this
  // update as we only get here if it's never used.

  // if a set was never bound, it will have been omitted and we just drop any writes to it
  bool valid = (writeDesc.dstSet != VK_NULL_HANDLE);

  if(!valid)
    return;

  const DescSetLayout &layout =
      m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[GetResID(writeDesc.dstSet)].layout];

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
    VkWriteDescriptorSet unwrapped = UnwrapInfo(&writeDesc);
    ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &unwrapped, 0, NULL);

    // update our local tracking
    std::vector<DescriptorSetSlot *> &bindings =
        m_DescriptorSetState[GetResID(writeDesc.dstSet)].currentBindings;

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

void WrappedVulkan::ReplayDescriptorSetCopy(VkDevice device, const VkCopyDescriptorSet &copyDesc)
{
  // if a set was never bound, it will have been omitted and we just drop any copies to it
  if(copyDesc.dstSet == VK_NULL_HANDLE || copyDesc.srcSet == VK_NULL_HANDLE)
    return;

  VkCopyDescriptorSet unwrapped = UnwrapInfo(&copyDesc);
  ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 0, NULL, 1, &unwrapped);

  ResourceId dstSetId = GetResID(copyDesc.dstSet);
  ResourceId srcSetId = GetResID(copyDesc.srcSet);

  // update our local tracking
  std::vector<DescriptorSetSlot *> &dstbindings = m_DescriptorSetState[dstSetId].currentBindings;
  std::vector<DescriptorSetSlot *> &srcbindings = m_DescriptorSetState[srcSetId].currentBindings;

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkUpdateDescriptorSets(SerialiserType &ser, VkDevice device,
                                                     uint32_t writeCount,
                                                     const VkWriteDescriptorSet *pDescriptorWrites,
                                                     uint32_t copyCount,
                                                     const VkCopyDescriptorSet *pDescriptorCopies)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_ARRAY(pDescriptorWrites, writeCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorCopies, copyCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(uint32_t i = 0; i < writeCount; i++)
      ReplayDescriptorSetWrite(device, pDescriptorWrites[i]);

    for(uint32_t i = 0; i < copyCount; i++)
      ReplayDescriptorSetCopy(device, pDescriptorCopies[i]);
  }

  return true;
}

void WrappedVulkan::vkUpdateDescriptorSets(VkDevice device, uint32_t writeCount,
                                           const VkWriteDescriptorSet *pDescriptorWrites,
                                           uint32_t copyCount,
                                           const VkCopyDescriptorSet *pDescriptorCopies)
{
  SCOPED_DBG_SINK();

  // we don't implement this into an UnwrapInfo because it's awkward to have this unique case of
  // two parallel struct arrays, and also we don't need to unwrap it on replay in the same way
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

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Structure sizes mean not enough space is allocated for write data");
      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkBufferView),
                        "Structure sizes mean not enough space is allocated for write data");

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
        bool hasSampler =
            (pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        bool hasImage =
            (pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

        unwrappedWrites[i].pImageInfo = (VkDescriptorImageInfo *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          if(hasImage)
            imInfos[j].imageView = Unwrap(pDescriptorWrites[i].pImageInfo[j].imageView);
          if(hasSampler)
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

    SERIALISE_TIME_CALL(ObjDisp(device)->UpdateDescriptorSets(
        Unwrap(device), writeCount, unwrappedWrites, copyCount, unwrappedCopies));
  }

  bool capframe = false;
  {
    SCOPED_LOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  if(capframe)
  {
    // don't have to mark referenced any of the resources pointed to by the descriptor set - that's
    // handled on queue submission by marking ref'd all the current bindings of the sets referenced
    // by the cmd buffer

    // as long as descriptor sets are forced to have initial states, we don't have to mark them
    // ref'd for write here. The reason being that as long as we only mark them as ref'd when
    // they're actually bound, we can safely skip the ref here and it means any descriptor set
    // updates of descriptor sets that are never used in the frame can be ignored.
    // GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].destSet),
    // eFrameRef_Write);

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkUpdateDescriptorSets);
      Serialise_vkUpdateDescriptorSets(ser, device, writeCount, pDescriptorWrites, copyCount,
                                       pDescriptorCopies);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    for(uint32_t i = 0; i < copyCount; i++)
    {
      // Like writes we don't have to mark the written descriptor set as used because unless it's
      // bound somewhere we don't need it anyway. However we DO have to mark the source set as used
      // because it doesn't have to be bound to still be needed (think about if the dest set is
      // bound somewhere after this copy - what refs the source set?).
      //
      // At the same time as ref'ing the source set, we must ref all of its resources (via the
      // bindFrameRefs).
      //
      // We just ref all rather than looking at only the copied sets to keep things simple.
      // This does mean a slightly conservative ref'ing if the dest set doesn't end up getting
      // bound, but we only do this during frame capture so it's not too bad.

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

  // need to track descriptor set contents whether capframing or idle
  if(IsCaptureMode(m_State))
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

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorSetLayout, VkDevice device,
                                const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDescriptorSetLayout *pSetLayout);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorPool, VkDevice device,
                                const VkDescriptorPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkDescriptorPool *pDescriptorPool);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkAllocateDescriptorSets, VkDevice device,
                                const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                VkDescriptorSet *pDescriptorSets);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUpdateDescriptorSets, VkDevice device,
                                uint32_t descriptorWriteCount,
                                const VkWriteDescriptorSet *pDescriptorWrites,
                                uint32_t descriptorCopyCount,
                                const VkCopyDescriptorSet *pDescriptorCopies);
