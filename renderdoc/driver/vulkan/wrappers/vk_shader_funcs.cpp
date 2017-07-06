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
#include "driver/shaders/spirv/spirv_common.h"

// Shader functions
bool WrappedVulkan::Serialise_vkCreatePipelineLayout(Serialiser *localSerialiser, VkDevice device,
                                                     const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkPipelineLayout *pPipelineLayout)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkPipelineLayoutCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelineLayout));

  if(m_State == READING)
  {
    VkPipelineLayout layout = VK_NULL_HANDLE;

    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

    VkResult ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &info, NULL, &layout);

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
        ObjDisp(device)->DestroyPipelineLayout(Unwrap(device), layout, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), layout);
        GetResourceManager()->AddLiveResource(id, layout);

        m_CreationInfo.m_PipelineLayout[live].Init(GetResourceManager(), m_CreationInfo, &info);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreatePipelineLayout(VkDevice device,
                                               const VkPipelineLayoutCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkPipelineLayout *pPipelineLayout)
{
  VkDescriptorSetLayout *unwrapped = GetTempArray<VkDescriptorSetLayout>(pCreateInfo->setLayoutCount);
  for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
    unwrapped[i] = Unwrap(pCreateInfo->pSetLayouts[i]);

  VkPipelineLayoutCreateInfo unwrappedInfo = *pCreateInfo;
  unwrappedInfo.pSetLayouts = unwrapped;

  VkResult ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &unwrappedInfo, pAllocator,
                                                       pPipelineLayout);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pPipelineLayout);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_PIPE_LAYOUT);
        Serialise_vkCreatePipelineLayout(localSerialiser, device, pCreateInfo, NULL, pPipelineLayout);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pPipelineLayout);
      record->AddChunk(chunk);

      for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
      {
        VkResourceRecord *layoutrecord = GetRecord(pCreateInfo->pSetLayouts[i]);
        record->AddParent(layoutrecord);
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pPipelineLayout);

      m_CreationInfo.m_PipelineLayout[id].Init(GetResourceManager(), m_CreationInfo, &unwrappedInfo);
    }
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkCreateShaderModule(Serialiser *localSerialiser, VkDevice device,
                                                   const VkShaderModuleCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkShaderModule *pShaderModule)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkShaderModuleCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pShaderModule));

  if(m_State == READING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    VkShaderModule sh = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateShaderModule(Unwrap(device), &info, NULL, &sh);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
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
        GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), sh);
        GetResourceManager()->AddLiveResource(id, sh);

        m_CreationInfo.m_ShaderModule[live].Init(GetResourceManager(), m_CreationInfo, &info);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateShaderModule(VkDevice device,
                                             const VkShaderModuleCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkShaderModule *pShaderModule)
{
  VkResult ret =
      ObjDisp(device)->CreateShaderModule(Unwrap(device), pCreateInfo, pAllocator, pShaderModule);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pShaderModule);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_SHADER_MODULE);
        Serialise_vkCreateShaderModule(localSerialiser, device, pCreateInfo, NULL, pShaderModule);

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

bool WrappedVulkan::Serialise_vkCreatePipelineCache(Serialiser *localSerialiser, VkDevice device,
                                                    const VkPipelineCacheCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkPipelineCache *pPipelineCache)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkPipelineCacheCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelineCache));

  if(m_State == READING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    VkPipelineCache cache = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), &info, NULL, &cache);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cache);
      GetResourceManager()->AddLiveResource(id, cache);
    }
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

  VkResult ret =
      ObjDisp(device)->CreatePipelineCache(Unwrap(device), &createInfo, pAllocator, pPipelineCache);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pPipelineCache);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_PIPE_CACHE);
        Serialise_vkCreatePipelineCache(localSerialiser, device, &createInfo, NULL, pPipelineCache);

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

bool WrappedVulkan::Serialise_vkCreateGraphicsPipelines(
    Serialiser *localSerialiser, VkDevice device, VkPipelineCache pipelineCache, uint32_t count,
    const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator,
    VkPipeline *pPipelines)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, cacheId, GetResID(pipelineCache));
  SERIALISE_ELEMENT(VkGraphicsPipelineCreateInfo, info, *pCreateInfos);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelines));

  if(m_State == READING)
  {
    VkPipeline pipe = VK_NULL_HANDLE;

    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    // don't use pipeline caches on replay
    pipelineCache =
        VK_NULL_HANDLE;    // GetResourceManager()->GetLiveHandle<VkPipelineCache>(cacheId);

    VkResult ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache),
                                                            1, &info, NULL, &pipe);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
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
        GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), pipe);
        GetResourceManager()->AddLiveResource(id, pipe);

        VulkanCreationInfo::Pipeline &pipeInfo = m_CreationInfo.m_Pipeline[live];

        pipeInfo.Init(GetResourceManager(), m_CreationInfo, &info);

        ResourceId renderPassID = GetResourceManager()->GetNonDispWrapper(info.renderPass)->id;

        info.renderPass = Unwrap(m_CreationInfo.m_RenderPass[renderPassID].loadRPs[info.subpass]);
        info.subpass = 0;

        ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache), 1,
                                                       &info, NULL, &pipeInfo.subpass0pipe);
        RDCASSERTEQUAL(ret, VK_SUCCESS);

        ResourceId subpass0id =
            GetResourceManager()->WrapResource(Unwrap(device), pipeInfo.subpass0pipe);

        // register as a live-only resource, so it is cleaned up properly
        GetResourceManager()->AddLiveResource(subpass0id, pipeInfo.subpass0pipe);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                  uint32_t count,
                                                  const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkPipeline *pPipelines)
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
    nextUnwrappedStages += pCreateInfos[i].stageCount;
    for(uint32_t j = 0; j < pCreateInfos[i].stageCount; j++)
    {
      unwrappedStages[j] = pCreateInfos[i].pStages[j];
      unwrappedStages[j].module = Unwrap(unwrappedStages[j].module);
    }

    unwrappedInfos[i] = pCreateInfos[i];
    unwrappedInfos[i].pStages = unwrappedStages;
    unwrappedInfos[i].layout = Unwrap(unwrappedInfos[i].layout);
    unwrappedInfos[i].renderPass = Unwrap(unwrappedInfos[i].renderPass);
    unwrappedInfos[i].basePipelineHandle = Unwrap(unwrappedInfos[i].basePipelineHandle);
  }

  VkResult ret = ObjDisp(device)->CreateGraphicsPipelines(
      Unwrap(device), Unwrap(pipelineCache), count, unwrappedInfos, pAllocator, pPipelines);

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < count; i++)
    {
      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pPipelines[i]);

      if(m_State >= WRITING)
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          VkGraphicsPipelineCreateInfo modifiedCreateInfo;
          const VkGraphicsPipelineCreateInfo *createInfo = &pCreateInfos[i];

          // since we serialise one by one, we need to fixup basePipelineIndex
          if(createInfo->basePipelineIndex != -1 && createInfo->basePipelineIndex < (int)i)
          {
            modifiedCreateInfo = *createInfo;
            modifiedCreateInfo.basePipelineHandle = pPipelines[modifiedCreateInfo.basePipelineIndex];
            modifiedCreateInfo.basePipelineIndex = -1;
            createInfo = &modifiedCreateInfo;
          }

          SCOPED_SERIALISE_CONTEXT(CREATE_GRAPHICS_PIPE);
          Serialise_vkCreateGraphicsPipelines(localSerialiser, device, pipelineCache, 1, createInfo,
                                              NULL, &pPipelines[i]);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pPipelines[i]);
        record->AddChunk(chunk);

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

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, &unwrappedInfos[i]);
      }
    }
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkCreateComputePipelines(Serialiser *localSerialiser, VkDevice device,
                                                       VkPipelineCache pipelineCache, uint32_t count,
                                                       const VkComputePipelineCreateInfo *pCreateInfos,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkPipeline *pPipelines)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, cacheId, GetResID(pipelineCache));
  SERIALISE_ELEMENT(VkComputePipelineCreateInfo, info, *pCreateInfos);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelines));

  if(m_State == READING)
  {
    VkPipeline pipe = VK_NULL_HANDLE;

    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    pipelineCache = GetResourceManager()->GetLiveHandle<VkPipelineCache>(cacheId);

    VkResult ret = ObjDisp(device)->CreateComputePipelines(Unwrap(device), Unwrap(pipelineCache), 1,
                                                           &info, NULL, &pipe);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
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
        GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), pipe);
        GetResourceManager()->AddLiveResource(id, pipe);

        m_CreationInfo.m_Pipeline[live].Init(GetResourceManager(), m_CreationInfo, &info);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                 uint32_t count,
                                                 const VkComputePipelineCreateInfo *pCreateInfos,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkPipeline *pPipelines)
{
  VkComputePipelineCreateInfo *unwrapped = GetTempArray<VkComputePipelineCreateInfo>(count);

  for(uint32_t i = 0; i < count; i++)
  {
    unwrapped[i] = pCreateInfos[i];
    unwrapped[i].stage.module = Unwrap(unwrapped[i].stage.module);
    unwrapped[i].layout = Unwrap(unwrapped[i].layout);
    unwrapped[i].basePipelineHandle = Unwrap(unwrapped[i].basePipelineHandle);
  }

  VkResult ret = ObjDisp(device)->CreateComputePipelines(Unwrap(device), Unwrap(pipelineCache),
                                                         count, unwrapped, pAllocator, pPipelines);

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < count; i++)
    {
      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pPipelines[i]);

      if(m_State >= WRITING)
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CONTEXT(CREATE_COMPUTE_PIPE);
          Serialise_vkCreateComputePipelines(localSerialiser, device, pipelineCache, 1,
                                             &pCreateInfos[i], NULL, &pPipelines[i]);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pPipelines[i]);
        record->AddChunk(chunk);

        if(pipelineCache != VK_NULL_HANDLE)
        {
          VkResourceRecord *cacherecord = GetRecord(pipelineCache);
          record->AddParent(cacherecord);
        }

        VkResourceRecord *layoutrecord = GetRecord(pCreateInfos[i].layout);
        record->AddParent(layoutrecord);

        VkResourceRecord *modulerecord = GetRecord(pCreateInfos[i].stage.module);
        record->AddParent(modulerecord);
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pPipelines[i]);

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, &unwrapped[i]);
      }
    }
  }

  return ret;
}
