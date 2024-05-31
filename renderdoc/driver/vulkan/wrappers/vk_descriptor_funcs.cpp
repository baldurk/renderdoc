/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "../vk_replay.h"
#include "core/settings.h"

RDOC_DEBUG_CONFIG(bool, Vulkan_Debug_AllowDescriptorSetReuse, true,
                  "Allow the re-use of descriptor sets via vkResetDescriptorPool.");

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

    if((unwrapped[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        unwrapped[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
       unwrapped[i].pImmutableSamplers)
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
VkDescriptorUpdateTemplateCreateInfo WrappedVulkan::UnwrapInfo(
    const VkDescriptorUpdateTemplateCreateInfo *info)
{
  VkDescriptorUpdateTemplateCreateInfo ret = *info;

  if(ret.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR)
    ret.pipelineLayout = Unwrap(ret.pipelineLayout);
  if(ret.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
    ret.descriptorSetLayout = Unwrap(ret.descriptorSetLayout);

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

  // nothing to unwrap for inline uniform block
  if(ret.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    return ret;

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
  else if(ret.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
  {
    byte *asDescMemory = GetTempMemory(sizeof(VkWriteDescriptorSetAccelerationStructureKHR) +
                                       (sizeof(VkAccelerationStructureKHR) * ret.descriptorCount));
    VkAccelerationStructureKHR *unwrappedASs =
        (VkAccelerationStructureKHR *)(asDescMemory +
                                       sizeof(VkWriteDescriptorSetAccelerationStructureKHR));
    VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
        (VkWriteDescriptorSetAccelerationStructureKHR *)memcpy(
            asDescMemory, ret.pNext, sizeof(VkWriteDescriptorSetAccelerationStructureKHR));

    for(uint32_t j = 0; j < ret.descriptorCount; j++)
    {
      unwrappedASs[j] = Unwrap(asWrite->pAccelerationStructures[j]);
    }
    asWrite->pAccelerationStructures = unwrappedASs;

    ret.pNext = asWrite;
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(DescriptorPool, GetResID(*pDescriptorPool))
      .TypedAs("VkDescriptorPool"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorPool pool = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &CreateInfo, NULL, &pool);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor pool, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
      GetResourceManager()->AddLiveResource(DescriptorPool, pool);

      m_CreationInfo.m_DescSetPool[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
    }

    AddResource(DescriptorPool, ResourceType::Pool, "Descriptor Pool");
    DerivedResource(device, DescriptorPool);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorPool(VkDevice device,
                                               const VkDescriptorPoolCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *,
                                               VkDescriptorPool *pDescriptorPool)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pCreateInfo, NULL,
                                                                  pDescriptorPool));

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

      record->descPoolInfo = new DescPoolInfo;
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(SetLayout, GetResID(*pSetLayout)).TypedAs("VkDescriptorSetLayout"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(&CreateInfo);

    // on replay we add compute access to any vertex descriptors, so that we can access them for
    // mesh output. This is only needed if we are using buffer device address and update-after-bind
    // descriptors, because non update-after-bind descriptors we can duplicate and patch. However to
    // keep things simple we just always do this whenever using BDA
    if(GetExtensions(NULL).ext_KHR_buffer_device_address ||
       GetExtensions(NULL).ext_EXT_buffer_device_address)
    {
      for(uint32_t b = 0; b < unwrapped.bindingCount; b++)
      {
        VkDescriptorSetLayoutBinding &bind = (VkDescriptorSetLayoutBinding &)unwrapped.pBindings[b];
        if(bind.stageFlags & VK_SHADER_STAGE_VERTEX_BIT)
          bind.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
      }
    }

    VkResult ret =
        ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrapped, NULL, &layout);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor layout, VkResult: %s", ToStr(ret).c_str());
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
                                                    const VkAllocationCallbacks *,
                                                    VkDescriptorSetLayout *pSetLayout)
{
  VkDescriptorSetLayoutCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrapped,
                                                                       NULL, pSetLayout));

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
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo).Important();
  SERIALISE_ELEMENT_LOCAL(DescriptorSet, GetResID(*pDescriptorSets)).TypedAs("VkDescriptorSet"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorSet descset = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo unwrapped = UnwrapInfo(&AllocateInfo);
    VkResult ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

    if(ret != VK_SUCCESS)
    {
      RDCWARN(
          "Failed to allocate descriptor set %s from pool %s on replay. Assuming pool was "
          "reset and re-used mid-capture, so overflowing.",
          ToStr(DescriptorSet).c_str(),
          ToStr(GetResourceManager()->GetOriginalID(GetResID(AllocateInfo.descriptorPool))).c_str());

      VulkanCreationInfo::DescSetPool &poolInfo =
          m_CreationInfo.m_DescSetPool[GetResID(AllocateInfo.descriptorPool)];

      if(poolInfo.overflow.empty())
      {
        RDCLOG("Creating first overflow pool");
        poolInfo.CreateOverflow(device, GetResourceManager());
      }

      // first try and use the most recent overflow pool
      unwrapped.descriptorPool = Unwrap(poolInfo.overflow.back());

      ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

      // if we got an error, maybe the latest overflow pool is full. Try to create a new one and use
      // that
      if(ret != VK_SUCCESS)
      {
        RDCLOG("Creating new overflow pool, last pool failed with %s", ToStr(ret).c_str());
        poolInfo.CreateOverflow(device, GetResourceManager());

        unwrapped.descriptorPool = Unwrap(poolInfo.overflow.back());

        ret = ObjDisp(device)->AllocateDescriptorSets(Unwrap(device), &unwrapped, &descset);

        if(ret != VK_SUCCESS)
        {
          SET_ERROR_RESULT(
              m_FailedReplayResult, ResultCode::APIReplayFailed,
              "Failed allocating descriptor sets, even after trying to overflow pool, VkResult: %s",
              ToStr(ret).c_str());
          return false;
        }
      }
    }

    // if we got here we must have succeeded
    RDCASSERTEQUAL(ret, VK_SUCCESS);

    ResourceId layoutId = GetResID(AllocateInfo.pSetLayouts[0]);

    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), descset);
      GetResourceManager()->AddLiveResource(DescriptorSet, descset);

      // this is stored in the resource record on capture, we need to be able to look to up
      m_DescriptorSetState[live].layout = layoutId;

      // If descriptorSetCount is zero or this structure is not included in the pNext chain,
      // then the variable lengths are considered to be zero.
      uint32_t variableDescriptorAlloc = 0;

      if(!m_CreationInfo.m_DescSetLayout[layoutId].bindings.empty() &&
         m_CreationInfo.m_DescSetLayout[layoutId].bindings.back().variableSize)
      {
        VkDescriptorSetVariableDescriptorCountAllocateInfo *variableAlloc =
            (VkDescriptorSetVariableDescriptorCountAllocateInfo *)FindNextStruct(
                &AllocateInfo,
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

        if(variableAlloc && variableAlloc->descriptorSetCount > 0)
        {
          // this struct will have been patched similar to VkDescriptorSetAllocateInfo so we look up
          // the [0]th element
          variableDescriptorAlloc = variableAlloc->pDescriptorCounts[0];
        }
      }

      m_CreationInfo.m_DescSetLayout[layoutId].CreateBindingsArray(m_DescriptorSetState[live].data,
                                                                   variableDescriptorAlloc);
    }

    AddResource(DescriptorSet, ResourceType::DescriptorStore, "Descriptor Set");
    DerivedResource(device, DescriptorSet);
    DerivedResource(AllocateInfo.pSetLayouts[0], DescriptorSet);
    DerivedResource(AllocateInfo.descriptorPool, DescriptorSet);

    DescriptorStoreDescription desc;
    desc.resourceId = DescriptorSet;
    desc.descriptorByteSize = 1;
    // descriptors are stored after all the inline bytes
    desc.firstDescriptorOffset = m_CreationInfo.m_DescSetLayout[layoutId].inlineByteSize;
    desc.descriptorCount =
        (uint32_t)m_DescriptorSetState[GetResID(descset)].data.totalDescriptorCount();
    GetReplay()->RegisterDescriptorStore(desc);
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

  VkDescriptorSetVariableDescriptorCountAllocateInfo *variableAlloc =
      (VkDescriptorSetVariableDescriptorCountAllocateInfo *)FindNextStruct(
          pAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

  VkDescriptorSetAllocateInfo mutableInfo = *pAllocateInfo;

  {
    byte *tempMem = GetTempMemory(GetNextPatchSize(mutableInfo.pNext));
    CopyNextChainForPatching("VkDescriptorSetAllocateInfo", tempMem,
                             (VkBaseInStructure *)&mutableInfo);
  }

  VkDescriptorSetVariableDescriptorCountAllocateInfo *mutableVariableInfo =
      (VkDescriptorSetVariableDescriptorCountAllocateInfo *)FindNextStruct(
          &mutableInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
  {
    VkResourceRecord *poolrecord = NULL;
    VkResourceRecord *layoutRecord = NULL;

    ResourceId id;
    VkResourceRecord *record = NULL;
    bool exactReuse = false;
    uint32_t variableDescriptorAlloc = 0;

    if(IsCaptureMode(m_State))
    {
      layoutRecord = GetRecord(pAllocateInfo->pSetLayouts[i]);
      poolrecord = GetRecord(pAllocateInfo->descriptorPool);

      if(!layoutRecord->descInfo->layout->bindings.empty() &&
         layoutRecord->descInfo->layout->bindings.back().variableSize && variableAlloc &&
         variableAlloc->descriptorSetCount > 0)
      {
        variableDescriptorAlloc = variableAlloc->pDescriptorCounts[i];
      }

      if(Atomic::CmpExch32(&m_ReuseEnabled, 1, 1) == 1)
      {
        rdcarray<VkResourceRecord *> &freelist = poolrecord->descPoolInfo->freelist;

        if(!freelist.empty())
        {
          DescSetLayout *search = layoutRecord->descInfo->layout;

          // try to find an exact layout match, then we don't need to re-initialise the descriptor
          // set.
          auto it = std::lower_bound(freelist.begin(), freelist.end(), search,
                                     [](VkResourceRecord *a, DescSetLayout *search) {
                                       return a->descInfo->layout < search;
                                     });

          if(it != freelist.end() && (*it)->descInfo->layout == layoutRecord->descInfo->layout &&
             (*it)->descInfo->data.variableDescriptorCount == variableDescriptorAlloc)
          {
            record = freelist.takeAt(it - freelist.begin());
            exactReuse = true;
          }
          else
          {
            record = freelist.back();
            freelist.pop_back();
          }

          if(!exactReuse)
            record->DeleteChunks();
        }
      }
    }

    if(record)
      id = GetResourceManager()->WrapReusedResource(record, pDescriptorSets[i]);
    else
      id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

    if(IsCaptureMode(m_State))
    {
      if(record == NULL)
      {
        record = GetResourceManager()->AddResourceRecord(pDescriptorSets[i]);

        poolrecord->LockChunks();
        poolrecord->pooledChildren.push_back(record);
        poolrecord->UnlockChunks();

        record->pool = poolrecord;

        // only mark descriptor set as dirty if it's not a push descriptor layout
        if((layoutRecord->descInfo->layout->flags &
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0)
        {
          GetResourceManager()->MarkDirtyResource(id);
        }

        record->descInfo = new DescriptorSetData();
      }

      if(!exactReuse)
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          VkDescriptorSetAllocateInfo info = mutableInfo;
          info.descriptorSetCount = 1;
          info.pSetLayouts = mutableInfo.pSetLayouts + i;

          if(mutableVariableInfo && variableAlloc->descriptorSetCount > 0)
          {
            mutableVariableInfo->descriptorSetCount = 1;
            mutableVariableInfo->pDescriptorCounts = variableAlloc->pDescriptorCounts + i;
          }

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkAllocateDescriptorSets);
          Serialise_vkAllocateDescriptorSets(ser, device, &info, &pDescriptorSets[i]);

          chunk = scope.Get();
        }
        record->AddChunk(chunk);

        record->FreeParents(GetResourceManager());
        record->AddParent(poolrecord);
        record->AddParent(layoutRecord);

        record->descInfo->layout = layoutRecord->descInfo->layout;
        record->descInfo->layout->CreateBindingsArray(record->descInfo->data,
                                                      variableDescriptorAlloc);
      }
      else
      {
        record->descInfo->data.reset();
      }
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
  {
    if(pDescriptorSets[i] != VK_NULL_HANDLE)
      GetResourceManager()->ReleaseWrappedResource(pDescriptorSets[i]);
  }

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
  {
    // don't reset while capture transition lock is held, so that we can't reset and potentially
    // reuse a record we might be preparing. We do this here rather than in vkAllocateDescriptorSets
    // where we actually modify the record, since that's much higher frequency
    SCOPED_READLOCK(m_CapTransitionLock);

    if(IsCaptureMode(m_State))
    {
      VkResourceRecord *record = GetRecord(descriptorPool);

      if(Vulkan_Debug_AllowDescriptorSetReuse())
      {
        for(auto it = record->pooledChildren.begin(); it != record->pooledChildren.end(); ++it)
        {
          ((WrappedVkNonDispRes *)(*it)->Resource)->real = RealVkRes(0x123456);
          (*it)->descInfo->data.reset();
        }

        record->descPoolInfo->freelist.assign(record->pooledChildren);

        // sort by layout
        std::sort(record->descPoolInfo->freelist.begin(), record->descPoolInfo->freelist.end(),
                  [](VkResourceRecord *a, VkResourceRecord *b) {
                    return a->descInfo->layout < b->descInfo->layout;
                  });
      }
      else
      {
        // if descriptor set re-use is banned, we can simply free all the sets immediately without
        // adding them to the free list and that will effectively disallow re-use.
        for(auto it = record->pooledChildren.begin(); it != record->pooledChildren.end(); ++it)
        {
          // unset record->pool so we don't recurse
          (*it)->pool = NULL;
          GetResourceManager()->ReleaseWrappedResource((VkDescriptorSet)(uint64_t)(*it)->Resource,
                                                       true);
        }

        record->pooledChildren.clear();
      }
    }
  }

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

  // ignore empty writes, for some reason this is valid with descriptor update templates.
  if(writeDesc.descriptorCount == 0)
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

          // skip past invalid padding descriptors to get to the next real one
          while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          {
            layoutBinding++;
          }
        }

        valid &= (writeDesc.pImageInfo[i].sampler != VK_NULL_HANDLE) ||
                 (layoutBinding->immutableSampler &&
                  layoutBinding->immutableSampler[curIdx] != ResourceId());

        if(!NULLDescriptorsAllowed())
          valid &= (writeDesc.pImageInfo[i].imageView != VK_NULL_HANDLE);
      }
      break;
    }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    {
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pImageInfo[i].imageView != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    {
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pTexelBufferView[i] != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    {
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
        valid &= (writeDesc.pBufferInfo[i].buffer != VK_NULL_HANDLE);
      break;
    }
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
    {
      const VkWriteDescriptorSetAccelerationStructureKHR *asDesc =
          (const VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
              &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
      for(uint32_t i = 0; !NULLDescriptorsAllowed() && i < writeDesc.descriptorCount; i++)
      {
        valid &= (asDesc->pAccelerationStructures[i] != VK_NULL_HANDLE);
      }
      break;
    }
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK: break;
    default: RDCERR("Unexpected descriptor type %d", writeDesc.descriptorType);
  }

  if(valid)
  {
    VkWriteDescriptorSet unwrapped = UnwrapInfo(&writeDesc);
    ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &unwrapped, 0, NULL);

    // update our local tracking
    rdcarray<DescriptorSetSlot *> &bindings =
        m_DescriptorSetState[GetResID(writeDesc.dstSet)].data.binds;
    bytebuf &inlineData = m_DescriptorSetState[GetResID(writeDesc.dstSet)].data.inlineBytes;

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

            // skip past invalid padding descriptors to get to the next real one
            while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
              layoutBinding++;
              bind++;
            }
          }

          (*bind)[curIdx].SetTexelBuffer(writeDesc.descriptorType,
                                         GetResID(writeDesc.pTexelBufferView[d]));
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

            // skip past invalid padding descriptors to get to the next real one
            while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
              layoutBinding++;
              bind++;
            }
          }

          (*bind)[curIdx].SetImage(writeDesc.descriptorType, writeDesc.pImageInfo[d],
                                   layoutBinding->immutableSampler == NULL);
        }
      }
      else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        VkWriteDescriptorSetInlineUniformBlock *inlineWrite =
            (VkWriteDescriptorSetInlineUniformBlock *)FindNextStruct(
                &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);
        memcpy(inlineData.data() + (*bind)->offset + writeDesc.dstArrayElement, inlineWrite->pData,
               inlineWrite->dataSize);
      }
      else
      {
        VkWriteDescriptorSetAccelerationStructureKHR *asDesc = NULL;

        for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
        {
          // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
          // explanation
          if(curIdx >= layoutBinding->descriptorCount)
          {
            layoutBinding++;
            bind++;
            curIdx = 0;

            // skip past invalid padding descriptors to get to the next real one
            while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
              layoutBinding++;
              bind++;
            }
          }

          if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
          {
            if(!asDesc)
              asDesc = (VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                  &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);

            (*bind)[curIdx].SetAccelerationStructure(writeDesc.descriptorType,
                                                     asDesc->pAccelerationStructures[d]);
          }
          else
          {
            (*bind)[curIdx].SetBuffer(writeDesc.descriptorType, writeDesc.pBufferInfo[d]);
          }
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
  rdcarray<DescriptorSetSlot *> &dstbindings = m_DescriptorSetState[dstSetId].data.binds;
  rdcarray<DescriptorSetSlot *> &srcbindings = m_DescriptorSetState[srcSetId].data.binds;

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
      if(layoutSrcBinding->layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // inline uniform blocks are special, the descriptor count is a byte count. The layouts may
        // not match so inline offsets might not match, so we just copy the data and break.

        bytebuf &dstInlineData = m_DescriptorSetState[dstSetId].data.inlineBytes;
        bytebuf &srcInlineData = m_DescriptorSetState[srcSetId].data.inlineBytes;

        memcpy(dstInlineData.data() + (*dstbind)[0].offset + copyDesc.dstArrayElement,
               srcInlineData.data() + (*srcbind)[0].offset + copyDesc.srcArrayElement,
               copyDesc.descriptorCount);

        break;
      }

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
  SERIALISE_ELEMENT(writeCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorWrites, writeCount);
  if(writeCount > 0)
    ser.Important();
  SERIALISE_ELEMENT(copyCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorCopies, copyCount);
  if(copyCount > 0)
    ser.Important();

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
    // need to count up number of descriptor infos and acceleration structures, to be able to alloc
    // enough space
    uint32_t numInfos = 0;
    uint32_t numASDescriptors = 0;
    uint32_t numASs = 0;
    for(uint32_t i = 0; i < writeCount; i++)
    {
      if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        ++numASDescriptors;
        numASs += pDescriptorWrites[i].descriptorCount;
      }
      else
      {
        numInfos += pDescriptorWrites[i].descriptorCount;
      }
    }

    byte *memory = GetTempMemory(
        sizeof(VkDescriptorBufferInfo) * numInfos + sizeof(VkWriteDescriptorSet) * writeCount +
        sizeof(VkCopyDescriptorSet) * copyCount +
        sizeof(VkWriteDescriptorSetAccelerationStructureKHR) * numASDescriptors +
        sizeof(VkAccelerationStructureKHR) * numASs);

    RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                      "Descriptor structs sizes are unexpected, ensure largest size is used");

    VkWriteDescriptorSet *unwrappedWrites = (VkWriteDescriptorSet *)memory;
    VkCopyDescriptorSet *unwrappedCopies = (VkCopyDescriptorSet *)(unwrappedWrites + writeCount);
    VkDescriptorBufferInfo *nextDescriptors = (VkDescriptorBufferInfo *)(unwrappedCopies + copyCount);
    VkWriteDescriptorSetAccelerationStructureKHR *nextASDescriptors =
        (VkWriteDescriptorSetAccelerationStructureKHR *)(nextDescriptors + numInfos);
    VkAccelerationStructureKHR *unwrappedASs =
        (VkAccelerationStructureKHR *)(nextASDescriptors + numASDescriptors);

    for(uint32_t i = 0; i < writeCount; i++)
    {
      unwrappedWrites[i] = pDescriptorWrites[i];

      bool hasImmutable = false;

      if(IsCaptureMode(m_State))
      {
        VkResourceRecord *record = GetRecord(unwrappedWrites[i].dstSet);
        RDCASSERT(record->descInfo && record->descInfo->layout);
        const DescSetLayout &layout = *record->descInfo->layout;

        RDCASSERT(unwrappedWrites[i].dstBinding < record->descInfo->data.binds.size());
        const DescSetLayout::Binding *layoutBinding = &layout.bindings[unwrappedWrites[i].dstBinding];

        hasImmutable = layoutBinding->immutableSampler != NULL;
      }
      else
      {
        const DescSetLayout &layout =
            m_CreationInfo
                .m_DescSetLayout[m_DescriptorSetState[GetResID(unwrappedWrites[i].dstSet)].layout];

        const DescSetLayout::Binding *layoutBinding = &layout.bindings[unwrappedWrites[i].dstBinding];

        hasImmutable = layoutBinding->immutableSampler != NULL;
      }

      unwrappedWrites[i].dstSet = Unwrap(unwrappedWrites[i].dstSet);

      VkDescriptorBufferInfo *bufInfos = nextDescriptors;
      VkDescriptorImageInfo *imInfos = (VkDescriptorImageInfo *)bufInfos;
      VkBufferView *bufViews = (VkBufferView *)bufInfos;

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
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
            !hasImmutable;
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
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // nothing to unwrap, the next chain contains the data which we can leave as-is
      }
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
            (VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                &pDescriptorWrites[i],
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
        asWrite = (VkWriteDescriptorSetAccelerationStructureKHR *)memcpy(
            nextASDescriptors, asWrite, sizeof(VkWriteDescriptorSetAccelerationStructureKHR));

        VkAccelerationStructureKHR *base = unwrappedASs;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          base[j] = Unwrap(asWrite->pAccelerationStructures[j]);
        }
        asWrite->pAccelerationStructures = base;

        unwrappedWrites[i].pNext = asWrite;

        ++nextASDescriptors;
        unwrappedASs += pDescriptorWrites[i].descriptorCount;
      }
      else
      {
        unwrappedWrites[i].pBufferInfo = bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          bufInfos[j].buffer = Unwrap(pDescriptorWrites[i].pBufferInfo[j].buffer);
          bufInfos[j].offset = pDescriptorWrites[i].pBufferInfo[j].offset;
          bufInfos[j].range = pDescriptorWrites[i].pBufferInfo[j].range;
          if(bufInfos[j].buffer == VK_NULL_HANDLE)
          {
            bufInfos[j].offset = 0;
            bufInfos[j].range = VK_WHOLE_SIZE;
          }
        }
      }

      // Increment nextDescriptors (a.k.a. bufInfos)
      if(pDescriptorWrites[i].descriptorType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        nextDescriptors += pDescriptorWrites[i].descriptorCount;
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

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    if(IsActiveCapturing(m_State))
    {
      // don't have to mark referenced any of the resources pointed to by the descriptor set -
      // that's
      // handled on queue submission by marking ref'd all the current bindings of the sets
      // referenced
      // by the cmd buffer

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkUpdateDescriptorSets);
        Serialise_vkUpdateDescriptorSets(ser, device, writeCount, pDescriptorWrites, copyCount,
                                         pDescriptorCopies);

        m_FrameCaptureRecord->AddChunk(scope.Get());
      }

      // previously we would not mark descriptor set destinations as ref'd here. This is because all
      // descriptor sets are implicitly dirty and they're only actually *needed* when bound - we can
      // safely skip any updates of unused descriptor sets. However for consistency with template
      // updates below, we pull them in here even if they won't technically be needed.

      for(uint32_t i = 0; i < writeCount; i++)
      {
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].dstSet),
                                                          eFrameRef_PartialWrite);

        if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
          VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
              (VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                  &pDescriptorWrites[i],
                  VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
          for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
          {
            const ResourceId id = GetResID(asWrite->pAccelerationStructures[j]);
            GetResourceManager()->MarkResourceFrameReferenced(id, eFrameRef_Read);
          }
        }
      }

      for(uint32_t i = 0; i < copyCount; i++)
      {
        // At the same time as ref'ing the source set, add it to a special list of descriptor sets
        // to pull in at the next queue submit. This is because it must be referenced even if the
        // source set is never bound to a command buffer, so that the source set's data is valid.
        //
        // This does mean a slightly conservative ref'ing if the dest set doesn't end up getting
        // bound, but we only do this during frame capture so it's not too bad.

        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].dstSet),
                                                          eFrameRef_PartialWrite);
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].srcSet),
                                                          eFrameRef_Read);

        ResourceId id = GetResID(pDescriptorCopies[i].srcSet);
        VkResourceRecord *record = GetRecord(pDescriptorCopies[i].srcSet);

        {
          SCOPED_LOCK(m_CapDescriptorsLock);
          record->AddRef();
          m_CapDescriptors.insert({id, record});
        }
      }
    }
  }

  // need to track descriptor set contents whether capframing or idle
  if(IsCaptureMode(m_State))
  {
    for(uint32_t i = 0; i < writeCount; i++)
    {
      const VkWriteDescriptorSet &descWrite = pDescriptorWrites[i];

      VkResourceRecord *record = GetRecord(descWrite.dstSet);
      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      RDCASSERT(descWrite.dstBinding < record->descInfo->data.binds.size());

      DescriptorSetSlot **binding = &record->descInfo->data.binds[descWrite.dstBinding];
      bytebuf &inlineData = record->descInfo->data.inlineBytes;

      const DescSetLayout::Binding *layoutBinding = &layout.bindings[descWrite.dstBinding];

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
      uint32_t curIdx = descWrite.dstArrayElement;

      for(uint32_t d = 0; d < descWrite.descriptorCount; d++, curIdx++)
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
        //
        // Note we don't have to worry about this interacting with variable descriptor counts
        // because the variable descriptor must be the last one, so there's no more overlap.

        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          binding++;
          curIdx = 0;

          // skip past invalid padding descriptors to get to the next real one
          while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          {
            layoutBinding++;
            binding++;
          }
        }

        DescriptorSetSlot &bind = (*binding)[curIdx];

        if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           descWrite.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind.SetTexelBuffer(descWrite.descriptorType, GetResID(descWrite.pTexelBufferView[d]));
        }
        else if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                descWrite.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          bind.SetImage(descWrite.descriptorType, descWrite.pImageInfo[d],
                        layoutBinding->immutableSampler == NULL);
        }
        else if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          VkWriteDescriptorSetInlineUniformBlock *inlineWrite =
              (VkWriteDescriptorSetInlineUniformBlock *)FindNextStruct(
                  &descWrite, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);
          memcpy(inlineData.data() + (*binding)->offset + descWrite.dstArrayElement,
                 inlineWrite->pData, inlineWrite->dataSize);

          // break now because the descriptorCount is not the number of descriptors
          break;
        }
        else if(descWrite.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
          VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
              (VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
                  &descWrite, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
          bind.SetAccelerationStructure(descWrite.descriptorType,
                                        asWrite->pAccelerationStructures[d]);
        }
        else
        {
          bind.SetBuffer(descWrite.descriptorType, descWrite.pBufferInfo[d]);
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

      RDCASSERT(pDescriptorCopies[i].dstBinding < dstrecord->descInfo->data.binds.size());
      RDCASSERT(pDescriptorCopies[i].srcBinding < srcrecord->descInfo->data.binds.size());

      DescriptorSetSlot **dstbinding =
          &dstrecord->descInfo->data.binds[pDescriptorCopies[i].dstBinding];
      DescriptorSetSlot **srcbinding =
          &srcrecord->descInfo->data.binds[pDescriptorCopies[i].srcBinding];

      const DescSetLayout::Binding *dstlayoutBinding =
          &dstlayout.bindings[pDescriptorCopies[i].dstBinding];
      const DescSetLayout::Binding *srclayoutBinding =
          &srclayout.bindings[pDescriptorCopies[i].srcBinding];

      // allow roll-over between consecutive bindings. See above in the plain write case for more
      // explanation
      uint32_t curSrcIdx = pDescriptorCopies[i].srcArrayElement;
      uint32_t curDstIdx = pDescriptorCopies[i].dstArrayElement;

      for(uint32_t d = 0; d < pDescriptorCopies[i].descriptorCount; d++, curSrcIdx++, curDstIdx++)
      {
        if(srclayoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          // inline uniform blocks are special, the descriptor count is a byte count. The layouts
          // may not match so inline offsets might not match, so we just copy the data and break.

          bytebuf &dstInlineData = dstrecord->descInfo->data.inlineBytes;
          bytebuf &srcInlineData = srcrecord->descInfo->data.inlineBytes;

          memcpy(
              dstInlineData.data() + (*dstbinding)[0].offset + pDescriptorCopies[i].dstArrayElement,
              srcInlineData.data() + (*srcbinding)[0].offset + pDescriptorCopies[i].srcArrayElement,
              pDescriptorCopies[i].descriptorCount);

          break;
        }

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

        bind = (*srcbinding)[curSrcIdx];
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateDescriptorUpdateTemplate(
    SerialiserType &ser, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(DescriptorUpdateTemplate, GetResID(*pDescriptorUpdateTemplate))
      .TypedAs("VkDescriptorUpdateTemplate"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkDescriptorUpdateTemplate templ = VK_NULL_HANDLE;

    VkDescriptorUpdateTemplateCreateInfo unwrapped = UnwrapInfo(&CreateInfo);
    VkResult ret =
        ObjDisp(device)->CreateDescriptorUpdateTemplate(Unwrap(device), &unwrapped, NULL, &templ);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating descriptor update template, VkResult: %s",
                       ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), templ);
      GetResourceManager()->AddLiveResource(DescriptorUpdateTemplate, templ);

      m_CreationInfo.m_DescUpdateTemplate[live].Init(GetResourceManager(), m_CreationInfo,
                                                     &CreateInfo);
    }

    AddResource(DescriptorUpdateTemplate, ResourceType::StateObject, "Descriptor Update Template");
    DerivedResource(device, DescriptorUpdateTemplate);
    if(CreateInfo.pipelineLayout != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.pipelineLayout, DescriptorUpdateTemplate);
    if(CreateInfo.descriptorSetLayout != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.descriptorSetLayout, DescriptorUpdateTemplate);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *, VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
  VkDescriptorUpdateTemplateCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateDescriptorUpdateTemplate(
                          Unwrap(device), &unwrapped, NULL, pDescriptorUpdateTemplate));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorUpdateTemplate);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateDescriptorUpdateTemplate);
        Serialise_vkCreateDescriptorUpdateTemplate(ser, device, pCreateInfo, NULL,
                                                   pDescriptorUpdateTemplate);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDescriptorUpdateTemplate);
      record->AddChunk(chunk);

      if(unwrapped.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR)
        record->AddParent(GetRecord(pCreateInfo->pipelineLayout));
      else if(unwrapped.templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
        record->AddParent(GetRecord(pCreateInfo->descriptorSetLayout));

      record->descTemplateInfo = new DescUpdateTemplate();
      record->descTemplateInfo->Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pDescriptorUpdateTemplate);

      m_CreationInfo.m_DescUpdateTemplate[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkUpdateDescriptorSetWithTemplate(
    SerialiserType &ser, VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(descriptorSet).Important();
  SERIALISE_ELEMENT(descriptorUpdateTemplate).Important();

  // we can't serialise pData as-is, since we need to decode to ResourceId for references, etc. The
  // sensible way to do this is to decode the data into a series of writes and serialise that.
  DescUpdateTemplateApplication apply;

  if(IsCaptureMode(m_State))
  {
    // decode while capturing.
    GetRecord(descriptorUpdateTemplate)->descTemplateInfo->Apply(pData, apply);
  }

  SERIALISE_ELEMENT(apply.writes).Named("Decoded Writes"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    for(VkWriteDescriptorSet &writeDesc : apply.writes)
    {
      writeDesc.dstSet = descriptorSet;
      ReplayDescriptorSetWrite(device, writeDesc);
    }
  }

  return true;
}

// see vkUpdateDescriptorSets for more verbose comments, the concepts are the same here except we
// apply from a template & user memory instead of arrays of VkWriteDescriptorSet/VkCopyDescriptorSet
void WrappedVulkan::vkUpdateDescriptorSetWithTemplate(
    VkDevice device, VkDescriptorSet descriptorSet,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void *pData)
{
  SCOPED_DBG_SINK();

  DescUpdateTemplate *tempInfo = GetRecord(descriptorUpdateTemplate)->descTemplateInfo;

  {
    // allocate the whole blob of memory
    byte *memory = GetTempMemory(tempInfo->unwrapByteSize);

    // iterate the entries, copy the descriptor data and unwrap
    for(const VkDescriptorUpdateTemplateEntry &entry : tempInfo->updates)
    {
      byte *dst = memory + entry.offset;
      const byte *src = (const byte *)pData + entry.offset;

      if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkBufferView));

          VkBufferView *bufView = (VkBufferView *)dst;

          *bufView = Unwrap(*bufView);

          dst += entry.stride;
          src += entry.stride;
        }
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        bool hasSampler = (entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        bool hasImage = (entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorImageInfo));

          VkDescriptorImageInfo *info = (VkDescriptorImageInfo *)dst;

          if(hasSampler)
            info->sampler = Unwrap(info->sampler);
          if(hasImage)
            info->imageView = Unwrap(info->imageView);

          dst += entry.stride;
          src += entry.stride;
        }
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // memcpy the data
        memcpy(dst, src, entry.descriptorCount);
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkAccelerationStructureKHR));

          VkAccelerationStructureKHR *as = (VkAccelerationStructureKHR *)dst;

          *as = Unwrap(*as);

          dst += entry.stride;
          src += entry.stride;
        }
      }
      else
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorBufferInfo));

          VkDescriptorBufferInfo *info = (VkDescriptorBufferInfo *)dst;

          info->buffer = Unwrap(info->buffer);

          dst += entry.stride;
          src += entry.stride;
        }
      }
    }

    SERIALISE_TIME_CALL(ObjDisp(device)->UpdateDescriptorSetWithTemplate(
        Unwrap(device), Unwrap(descriptorSet), Unwrap(descriptorUpdateTemplate), memory));
  }

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    if(IsActiveCapturing(m_State))
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkUpdateDescriptorSetWithTemplate);
      Serialise_vkUpdateDescriptorSetWithTemplate(ser, device, descriptorSet,
                                                  descriptorUpdateTemplate, pData);

      m_FrameCaptureRecord->AddChunk(scope.Get());

      // mark the destination set and template as referenced
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(descriptorSet),
                                                        eFrameRef_PartialWrite);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(descriptorUpdateTemplate),
                                                        eFrameRef_Read);
    }
  }

  // need to track descriptor set contents whether capframing or idle
  if(IsCaptureMode(m_State))
  {
    for(const VkDescriptorUpdateTemplateEntry &entry : tempInfo->updates)
    {
      VkResourceRecord *record = GetRecord(descriptorSet);

      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      RDCASSERT(entry.dstBinding < record->descInfo->data.binds.size());

      DescriptorSetSlot **binding = &record->descInfo->data.binds[entry.dstBinding];
      bytebuf &inlineData = record->descInfo->data.inlineBytes;

      const DescSetLayout::Binding *layoutBinding = &layout.bindings[entry.dstBinding];

      // start at the dstArrayElement
      uint32_t curIdx = entry.dstArrayElement;

      for(uint32_t d = 0; d < entry.descriptorCount; d++, curIdx++)
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
        //
        // Note we don't have to worry about this interacting with variable descriptor counts
        // because the variable descriptor must be the last one, so there's no more overlap.

        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          binding++;
          curIdx = 0;

          // skip past invalid padding descriptors to get to the next real one
          while(layoutBinding->layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
          {
            layoutBinding++;
            binding++;
          }
        }

        const byte *src = (const byte *)pData + entry.offset + entry.stride * d;

        DescriptorSetSlot &bind = (*binding)[curIdx];

        if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind.SetTexelBuffer(entry.descriptorType, GetResID(*(const VkBufferView *)src));
        }
        else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          const VkDescriptorImageInfo &srcInfo = *(const VkDescriptorImageInfo *)src;

          bind.SetImage(entry.descriptorType, srcInfo, layoutBinding->immutableSampler == NULL);
        }
        else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          memcpy(inlineData.data() + bind.offset + entry.dstArrayElement, src, entry.descriptorCount);

          // break now because the descriptorCount is not the number of descriptors
          break;
        }
        else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
          bind.SetAccelerationStructure(entry.descriptorType,
                                        *(const VkAccelerationStructureKHR *)src);
        }
        else
        {
          bind.SetBuffer(entry.descriptorType, *(const VkDescriptorBufferInfo *)src);
        }
      }
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorSetLayout, VkDevice device,
                                const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkDescriptorSetLayout *pSetLayout);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorPool, VkDevice device,
                                const VkDescriptorPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkDescriptorPool *pDescriptorPool);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkAllocateDescriptorSets, VkDevice device,
                                const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                VkDescriptorSet *pDescriptorSets);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUpdateDescriptorSets, VkDevice device,
                                uint32_t descriptorWriteCount,
                                const VkWriteDescriptorSet *pDescriptorWrites,
                                uint32_t descriptorCopyCount,
                                const VkCopyDescriptorSet *pDescriptorCopies);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorUpdateTemplate, VkDevice device,
                                const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *,
                                VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate);

INSTANTIATE_FUNCTION_SERIALISED(void, vkUpdateDescriptorSetWithTemplate, VkDevice device,
                                VkDescriptorSet descriptorSet,
                                VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                const void *pData);
