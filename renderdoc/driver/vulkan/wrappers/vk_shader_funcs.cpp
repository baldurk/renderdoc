/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "driver/shaders/spirv/spirv_reflect.h"

template <>
VkComputePipelineCreateInfo *WrappedVulkan::UnwrapInfos(const VkComputePipelineCreateInfo *info,
                                                        uint32_t count)
{
  VkComputePipelineCreateInfo *unwrapped = GetTempArray<VkComputePipelineCreateInfo>(count);

  for(uint32_t i = 0; i < count; i++)
  {
    unwrapped[i] = info[i];
    unwrapped[i].stage.module = Unwrap(unwrapped[i].stage.module);
    unwrapped[i].layout = Unwrap(unwrapped[i].layout);
    if(unwrapped[i].flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
      unwrapped[i].basePipelineHandle = Unwrap(unwrapped[i].basePipelineHandle);
  }

  return unwrapped;
}

template <>
VkGraphicsPipelineCreateInfo *WrappedVulkan::UnwrapInfos(const VkGraphicsPipelineCreateInfo *info,
                                                         uint32_t count)
{
  // conservatively request memory for 5 stages on each pipeline
  // (worst case - can't have compute stage). Avoids needing to count
  byte *unwrapped = GetTempMemory(sizeof(VkGraphicsPipelineCreateInfo) * count +
                                  sizeof(VkPipelineShaderStageCreateInfo) * count * 5);

  // keep pipelines first in the memory, then the stages
  VkGraphicsPipelineCreateInfo *unwrappedInfos = (VkGraphicsPipelineCreateInfo *)unwrapped;
  VkPipelineShaderStageCreateInfo *nextUnwrappedStages =
      (VkPipelineShaderStageCreateInfo *)(unwrappedInfos + count);

  for(uint32_t i = 0; i < count; i++)
  {
    VkPipelineShaderStageCreateInfo *unwrappedStages = nextUnwrappedStages;
    nextUnwrappedStages += info[i].stageCount;
    for(uint32_t j = 0; j < info[i].stageCount; j++)
    {
      unwrappedStages[j] = info[i].pStages[j];
      unwrappedStages[j].module = Unwrap(unwrappedStages[j].module);
    }

    unwrappedInfos[i] = info[i];
    unwrappedInfos[i].pStages = unwrappedStages;
    unwrappedInfos[i].layout = Unwrap(unwrappedInfos[i].layout);
    unwrappedInfos[i].renderPass = Unwrap(unwrappedInfos[i].renderPass);
    if(unwrappedInfos[i].flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
      unwrappedInfos[i].basePipelineHandle = Unwrap(unwrappedInfos[i].basePipelineHandle);
  }

  return unwrappedInfos;
}

template <>
VkPipelineLayoutCreateInfo WrappedVulkan::UnwrapInfo(const VkPipelineLayoutCreateInfo *info)
{
  VkPipelineLayoutCreateInfo ret = *info;

  VkDescriptorSetLayout *unwrapped = GetTempArray<VkDescriptorSetLayout>(info->setLayoutCount);
  for(uint32_t i = 0; i < info->setLayoutCount; i++)
    unwrapped[i] = Unwrap(info->pSetLayouts[i]);

  ret.pSetLayouts = unwrapped;

  return ret;
}

// Shader functions
template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreatePipelineLayout(SerialiserType &ser, VkDevice device,
                                                     const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkPipelineLayout *pPipelineLayout)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(PipelineLayout, GetResID(*pPipelineLayout))
      .TypedAs("VkPipelineLayout"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipelineLayout layout = VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo unwrapped = UnwrapInfo(&CreateInfo);
    VkResult ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &unwrapped, NULL, &layout);

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
        ObjDisp(device)->DestroyPipelineLayout(Unwrap(device), layout, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(PipelineLayout,
                                              GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), layout);
        GetResourceManager()->AddLiveResource(PipelineLayout, layout);

        m_CreationInfo.m_PipelineLayout[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(PipelineLayout, ResourceType::ShaderBinding, "Pipeline Layout");
    DerivedResource(device, PipelineLayout);
    for(uint32_t i = 0; i < CreateInfo.setLayoutCount; i++)
      DerivedResource(CreateInfo.pSetLayouts[i], PipelineLayout);
  }

  return true;
}

VkResult WrappedVulkan::vkCreatePipelineLayout(VkDevice device,
                                               const VkPipelineLayoutCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkPipelineLayout *pPipelineLayout)
{
  VkPipelineLayoutCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &unwrapped,
                                                                  pAllocator, pPipelineLayout));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pPipelineLayout);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreatePipelineLayout);
        Serialise_vkCreatePipelineLayout(ser, device, pCreateInfo, NULL, pPipelineLayout);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pPipelineLayout);
      record->AddChunk(chunk);

      record->pipeLayoutInfo = new PipelineLayoutData();

      for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
      {
        VkResourceRecord *layoutrecord = GetRecord(pCreateInfo->pSetLayouts[i]);
        record->AddParent(layoutrecord);

        record->pipeLayoutInfo->layouts.push_back(*layoutrecord->descInfo->layout);
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pPipelineLayout);

      m_CreationInfo.m_PipelineLayout[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateShaderModule(SerialiserType &ser, VkDevice device,
                                                   const VkShaderModuleCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkShaderModule *pShaderModule)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(ShaderModule, GetResID(*pShaderModule)).TypedAs("VkShaderModule"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkShaderModule sh = VK_NULL_HANDLE;

    VkShaderModuleCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkShaderModuleCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateShaderModule(Unwrap(device), &patched, NULL, &sh);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(sh)))
      {
        live = GetResourceManager()->GetNonDispWrapper(sh)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyShaderModule(Unwrap(device), sh, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(ShaderModule,
                                              GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), sh);
        GetResourceManager()->AddLiveResource(ShaderModule, sh);

        m_CreationInfo.m_ShaderModule[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(ShaderModule, ResourceType::Shader, "Shader Module");
    DerivedResource(device, ShaderModule);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateShaderModule(VkDevice device,
                                             const VkShaderModuleCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkShaderModule *pShaderModule)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateShaderModule(Unwrap(device), pCreateInfo,
                                                                pAllocator, pShaderModule));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pShaderModule);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateShaderModule);
        Serialise_vkCreateShaderModule(ser, device, pCreateInfo, NULL, pShaderModule);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pShaderModule);
      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pShaderModule);

      m_CreationInfo.m_ShaderModule[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

// Pipeline functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreatePipelineCache(SerialiserType &ser, VkDevice device,
                                                    const VkPipelineCacheCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkPipelineCache *pPipelineCache)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(PipelineCache, GetResID(*pPipelineCache)).TypedAs("VkPipelineCache"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipelineCache cache = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), &CreateInfo, NULL, &cache);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cache);
      GetResourceManager()->AddLiveResource(PipelineCache, cache);
    }

    AddResource(PipelineCache, ResourceType::Pool, "Pipeline Cache");
    DerivedResource(device, PipelineCache);
  }

  return true;
}

VkResult WrappedVulkan::vkCreatePipelineCache(VkDevice device,
                                              const VkPipelineCacheCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkPipelineCache *pPipelineCache)
{
  // pretend the user didn't provide any cache data

  VkPipelineCacheCreateInfo createInfo = *pCreateInfo;
  createInfo.initialDataSize = 0;
  createInfo.pInitialData = NULL;

  if(pCreateInfo->initialDataSize > 0)
  {
    RDCWARN(
        "Application provided pipeline cache data! This is invalid, as RenderDoc reports "
        "incompatibility with previous caches");
  }

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), &createInfo,
                                                                 pAllocator, pPipelineCache));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pPipelineCache);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreatePipelineCache);
        Serialise_vkCreatePipelineCache(ser, device, &createInfo, NULL, pPipelineCache);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pPipelineCache);
      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pPipelineCache);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateGraphicsPipelines(
    SerialiserType &ser, VkDevice device, VkPipelineCache pipelineCache, uint32_t count,
    const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator,
    VkPipeline *pPipelines)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(pipelineCache);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfos);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Pipeline, GetResID(*pPipelines)).TypedAs("VkPipeline"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipeline pipe = VK_NULL_HANDLE;

    VkRenderPass origRP = CreateInfo.renderPass;
    VkPipelineCache origCache = pipelineCache;

    // don't use pipeline caches on replay
    pipelineCache = VK_NULL_HANDLE;

    VkGraphicsPipelineCreateInfo *unwrapped = UnwrapInfos(&CreateInfo, 1);
    VkResult ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache),
                                                            1, unwrapped, NULL, &pipe);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(pipe)))
      {
        live = GetResourceManager()->GetNonDispWrapper(pipe)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyPipeline(Unwrap(device), pipe, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(Pipeline, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), pipe);
        GetResourceManager()->AddLiveResource(Pipeline, pipe);

        VulkanCreationInfo::Pipeline &pipeInfo = m_CreationInfo.m_Pipeline[live];

        pipeInfo.Init(GetResourceManager(), m_CreationInfo, &CreateInfo);

        ResourceId renderPassID = GetResID(CreateInfo.renderPass);

        CreateInfo.renderPass = m_CreationInfo.m_RenderPass[renderPassID].loadRPs[CreateInfo.subpass];
        CreateInfo.subpass = 0;

        unwrapped = UnwrapInfos(&CreateInfo, 1);
        ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache), 1,
                                                       unwrapped, NULL, &pipeInfo.subpass0pipe);
        RDCASSERTEQUAL(ret, VK_SUCCESS);

        ResourceId subpass0id =
            GetResourceManager()->WrapResource(Unwrap(device), pipeInfo.subpass0pipe);

        // register as a live-only resource, so it is cleaned up properly
        GetResourceManager()->AddLiveResource(subpass0id, pipeInfo.subpass0pipe);
      }
    }

    AddResource(Pipeline, ResourceType::PipelineState, "Graphics Pipeline");
    DerivedResource(device, Pipeline);
    if(origCache != VK_NULL_HANDLE)
      DerivedResource(origCache, Pipeline);
    if(CreateInfo.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    {
      if(CreateInfo.basePipelineHandle != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.basePipelineHandle, Pipeline);
    }
    DerivedResource(origRP, Pipeline);
    DerivedResource(CreateInfo.layout, Pipeline);
    for(uint32_t i = 0; i < CreateInfo.stageCount; i++)
      DerivedResource(CreateInfo.pStages[i].module, Pipeline);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                  uint32_t count,
                                                  const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkPipeline *pPipelines)
{
  VkGraphicsPipelineCreateInfo *unwrapped = UnwrapInfos(pCreateInfos, count);
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache), count,
                                                     unwrapped, pAllocator, pPipelines));

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < count; i++)
    {
      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pPipelines[i]);

      if(IsCaptureMode(m_State))
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          VkGraphicsPipelineCreateInfo modifiedCreateInfo;
          const VkGraphicsPipelineCreateInfo *createInfo = &pCreateInfos[i];

          if(createInfo->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
          {
            // since we serialise one by one, we need to fixup basePipelineIndex
            if(createInfo->basePipelineIndex != -1 && createInfo->basePipelineIndex < (int)i)
            {
              modifiedCreateInfo = *createInfo;
              modifiedCreateInfo.basePipelineHandle =
                  pPipelines[modifiedCreateInfo.basePipelineIndex];
              modifiedCreateInfo.basePipelineIndex = -1;
              createInfo = &modifiedCreateInfo;
            }
          }

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateGraphicsPipelines);
          Serialise_vkCreateGraphicsPipelines(ser, device, pipelineCache, 1, createInfo, NULL,
                                              &pPipelines[i]);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pPipelines[i]);
        record->AddChunk(chunk);

        if(pCreateInfos[i].flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
        {
          if(pCreateInfos[i].basePipelineHandle != VK_NULL_HANDLE)
          {
            VkResourceRecord *baserecord = GetRecord(pCreateInfos[i].basePipelineHandle);
            record->AddParent(baserecord);

            RDCDEBUG("Creating pipeline %llu base is %llu", record->GetResourceID(),
                     baserecord->GetResourceID());
          }
          else if(pCreateInfos[i].basePipelineIndex != -1 &&
                  pCreateInfos[i].basePipelineIndex < (int)i)
          {
            VkResourceRecord *baserecord = GetRecord(pPipelines[pCreateInfos[i].basePipelineIndex]);
            record->AddParent(baserecord);
          }
        }

        if(pipelineCache != VK_NULL_HANDLE)
        {
          VkResourceRecord *cacherecord = GetRecord(pipelineCache);
          record->AddParent(cacherecord);
        }

        VkResourceRecord *rprecord = GetRecord(pCreateInfos[i].renderPass);
        record->AddParent(rprecord);

        VkResourceRecord *layoutrecord = GetRecord(pCreateInfos[i].layout);
        record->AddParent(layoutrecord);

        for(uint32_t s = 0; s < pCreateInfos[i].stageCount; s++)
        {
          VkResourceRecord *modulerecord = GetRecord(pCreateInfos[i].pStages[s].module);
          record->AddParent(modulerecord);
        }
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pPipelines[i]);

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, &pCreateInfos[i]);
      }
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateComputePipelines(SerialiserType &ser, VkDevice device,
                                                       VkPipelineCache pipelineCache, uint32_t count,
                                                       const VkComputePipelineCreateInfo *pCreateInfos,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkPipeline *pPipelines)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(pipelineCache);
  SERIALISE_ELEMENT(count);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfos);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Pipeline, GetResID(*pPipelines)).TypedAs("VkPipeline"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipeline pipe = VK_NULL_HANDLE;

    VkPipelineCache origCache = pipelineCache;

    // don't use pipeline caches on replay
    pipelineCache = VK_NULL_HANDLE;

    VkComputePipelineCreateInfo *unwrapped = UnwrapInfos(&CreateInfo, 1);
    VkResult ret = ObjDisp(device)->CreateComputePipelines(Unwrap(device), Unwrap(pipelineCache), 1,
                                                           unwrapped, NULL, &pipe);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(pipe)))
      {
        live = GetResourceManager()->GetNonDispWrapper(pipe)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroyPipeline(Unwrap(device), pipe, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(Pipeline, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), pipe);
        GetResourceManager()->AddLiveResource(Pipeline, pipe);

        m_CreationInfo.m_Pipeline[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(Pipeline, ResourceType::PipelineState, "Graphics Pipeline");
    DerivedResource(device, Pipeline);
    if(origCache != VK_NULL_HANDLE)
      DerivedResource(origCache, Pipeline);
    if(CreateInfo.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    {
      if(CreateInfo.basePipelineHandle != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.basePipelineHandle, Pipeline);
    }
    DerivedResource(CreateInfo.layout, Pipeline);
    DerivedResource(CreateInfo.stage.module, Pipeline);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                 uint32_t count,
                                                 const VkComputePipelineCreateInfo *pCreateInfos,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkPipeline *pPipelines)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateComputePipelines(
                          Unwrap(device), Unwrap(pipelineCache), count,
                          UnwrapInfos(pCreateInfos, count), pAllocator, pPipelines));

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < count; i++)
    {
      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pPipelines[i]);

      if(IsCaptureMode(m_State))
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          VkComputePipelineCreateInfo modifiedCreateInfo;
          const VkComputePipelineCreateInfo *createInfo = &pCreateInfos[i];

          if(createInfo->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
          {
            // since we serialise one by one, we need to fixup basePipelineIndex
            if(createInfo->basePipelineIndex != -1 && createInfo->basePipelineIndex < (int)i)
            {
              modifiedCreateInfo = *createInfo;
              modifiedCreateInfo.basePipelineHandle =
                  pPipelines[modifiedCreateInfo.basePipelineIndex];
              modifiedCreateInfo.basePipelineIndex = -1;
              createInfo = &modifiedCreateInfo;
            }
          }

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateComputePipelines);
          Serialise_vkCreateComputePipelines(ser, device, pipelineCache, 1, createInfo, NULL,
                                             &pPipelines[i]);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pPipelines[i]);
        record->AddChunk(chunk);

        if(pipelineCache != VK_NULL_HANDLE)
        {
          VkResourceRecord *cacherecord = GetRecord(pipelineCache);
          record->AddParent(cacherecord);
        }

        if(pCreateInfos[i].flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
        {
          if(pCreateInfos[i].basePipelineHandle != VK_NULL_HANDLE)
          {
            VkResourceRecord *baserecord = GetRecord(pCreateInfos[i].basePipelineHandle);
            record->AddParent(baserecord);
          }
          else if(pCreateInfos[i].basePipelineIndex != -1 &&
                  pCreateInfos[i].basePipelineIndex < (int)i)
          {
            VkResourceRecord *baserecord = GetRecord(pPipelines[pCreateInfos[i].basePipelineIndex]);
            record->AddParent(baserecord);
          }
        }

        VkResourceRecord *layoutrecord = GetRecord(pCreateInfos[i].layout);
        record->AddParent(layoutrecord);

        VkResourceRecord *modulerecord = GetRecord(pCreateInfos[i].stage.module);
        record->AddParent(modulerecord);
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pPipelines[i]);

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, &pCreateInfos[i]);
      }
    }
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreatePipelineLayout, VkDevice device,
                                const VkPipelineLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkPipelineLayout *pPipelineLayout);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateShaderModule, VkDevice device,
                                const VkShaderModuleCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkShaderModule *pShaderModule);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreatePipelineCache, VkDevice device,
                                const VkPipelineCacheCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator,
                                VkPipelineCache *pPipelineCache);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateGraphicsPipelines, VkDevice device,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateComputePipelines, VkDevice device,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkComputePipelineCreateInfo *pCreateInfos,
                                const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);
