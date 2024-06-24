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
#include "driver/shaders/spirv/spirv_reflect.h"

template <>
VkComputePipelineCreateInfo *WrappedVulkan::UnwrapInfos(CaptureState state,
                                                        const VkComputePipelineCreateInfo *info,
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
VkGraphicsPipelineCreateInfo *WrappedVulkan::UnwrapInfos(CaptureState state,
                                                         const VkGraphicsPipelineCreateInfo *info,
                                                         uint32_t count)
{
  // conservatively request memory for 5 stages on each pipeline
  // (worst case - can't have compute stage). Avoids needing to count
  size_t memSize = sizeof(VkGraphicsPipelineCreateInfo) * count;
  for(uint32_t i = 0; i < count; i++)
  {
    memSize += sizeof(VkPipelineShaderStageCreateInfo) * info[i].stageCount;
    memSize += GetNextPatchSize(info[i].pNext);
  }

  byte *tempMem = GetTempMemory(memSize);

  // keep pipelines first in the memory, then the stages
  VkGraphicsPipelineCreateInfo *unwrappedInfos = (VkGraphicsPipelineCreateInfo *)tempMem;
  tempMem = (byte *)(unwrappedInfos + count);

  for(uint32_t i = 0; i < count; i++)
  {
    VkPipelineShaderStageCreateInfo *unwrappedStages = (VkPipelineShaderStageCreateInfo *)tempMem;
    tempMem = (byte *)(unwrappedStages + info[i].stageCount);
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

    UnwrapNextChain(state, "VkGraphicsPipelineCreateInfo", tempMem,
                    (VkBaseInStructure *)&unwrappedInfos[i]);
  }

  return unwrappedInfos;
}

template <>
VkShaderCreateInfoEXT *WrappedVulkan::UnwrapInfos(CaptureState state,
                                                  const VkShaderCreateInfoEXT *info, uint32_t count)
{
  // request memory for infos, descriptor set layouts, and next chain
  size_t memSize = sizeof(VkShaderCreateInfoEXT) * count;
  for(uint32_t i = 0; i < count; i++)
  {
    memSize += sizeof(VkDescriptorSetLayout) * info[i].setLayoutCount;
    memSize += GetNextPatchSize(info[i].pNext);
  }

  byte *tempMem = GetTempMemory(memSize);

  // keep shader infos first in the memory, then descriptor set layouts, then next chain
  VkShaderCreateInfoEXT *unwrappedInfos = (VkShaderCreateInfoEXT *)tempMem;
  tempMem = (byte *)(unwrappedInfos + count);

  for(uint32_t i = 0; i < count; i++)
  {
    VkDescriptorSetLayout *unwrappedLayouts = (VkDescriptorSetLayout *)tempMem;
    tempMem = (byte *)(unwrappedLayouts + info[i].setLayoutCount);
    if(info[i].pSetLayouts)
      for(uint32_t j = 0; j < info[i].setLayoutCount; j++)
        unwrappedLayouts[j] = Unwrap(info[i].pSetLayouts[j]);

    unwrappedInfos[i] = info[i];
    unwrappedInfos[i].pSetLayouts = info[i].pSetLayouts ? unwrappedLayouts : NULL;

    UnwrapNextChain(state, "VkShaderCreateInfoEXT", tempMem, (VkBaseInStructure *)&unwrappedInfos[i]);
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

template <>
VkRayTracingPipelineCreateInfoKHR *WrappedVulkan::UnwrapInfos(
    CaptureState state, const VkRayTracingPipelineCreateInfoKHR *info, uint32_t count)
{
  size_t memSize = sizeof(VkRayTracingPipelineCreateInfoKHR) * count;
  for(uint32_t i = 0; i < count; i++)
    memSize += GetNextPatchSize(&info[i]);

  byte *tempMem = GetTempMemory(memSize);

  VkRayTracingPipelineCreateInfoKHR *unwrappedInfos = (VkRayTracingPipelineCreateInfoKHR *)tempMem;
  tempMem = (byte *)(unwrappedInfos + count);

  for(uint32_t i = 0; i < count; i++)
    unwrappedInfos[i] = *UnwrapStructAndChain(state, tempMem, &info[i]);

  return unwrappedInfos;
}

// Shader functions
template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreatePipelineLayout(SerialiserType &ser, VkDevice device,
                                                     const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkPipelineLayout *pPipelineLayout)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
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
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating pipeline layout, VkResult: %s", ToStr(ret).c_str());
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
    {
      if(CreateInfo.pSetLayouts[i] != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.pSetLayouts[i], PipelineLayout);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreatePipelineLayout(VkDevice device,
                                               const VkPipelineLayoutCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *,
                                               VkPipelineLayout *pPipelineLayout)
{
  VkPipelineLayoutCreateInfo unwrapped = UnwrapInfo(pCreateInfo);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &unwrapped, NULL,
                                                                  pPipelineLayout));

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
        if(layoutrecord)
        {
          record->AddParent(layoutrecord);

          record->pipeLayoutInfo->layouts.push_back(*layoutrecord->descInfo->layout);
        }
        else
        {
          record->pipeLayoutInfo->layouts.push_back(DescSetLayout());
        }
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
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
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating shader module, VkResult: %s", ToStr(ret).c_str());
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
                                             const VkAllocationCallbacks *,
                                             VkShaderModule *pShaderModule)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateShaderModule(Unwrap(device), pCreateInfo, NULL, pShaderModule));

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateShadersEXT(SerialiserType &ser, VkDevice device,
                                                 uint32_t createInfoCount,
                                                 const VkShaderCreateInfoEXT *pCreateInfos,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkShaderEXT *pShaders)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(createInfoCount);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfos).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Shader, GetResID(*pShaders)).TypedAs("VkShaderEXT"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkShaderEXT sh = VK_NULL_HANDLE;

    // this function is called from a loop in vkCreateShadersEXT, so we only need to unwrap one
    // then it gets replayed as if each shader was created individually
    VkShaderCreateInfoEXT *unwrapped = UnwrapInfos(m_State, &CreateInfo, 1);

    VkResult ret = ObjDisp(device)->CreateShadersEXT(Unwrap(device), 1, unwrapped, NULL, &sh);

    AddResource(Shader, ResourceType::Shader, "Shader");

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating shader object, VkResult: %s", ToStr(ret).c_str());
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
        ObjDisp(device)->DestroyShaderEXT(Unwrap(device), sh, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(Shader, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), sh);
        GetResourceManager()->AddLiveResource(Shader, sh);

        m_CreationInfo.m_ShaderObject[live].Init(GetResourceManager(), m_CreationInfo, live,
                                                 &CreateInfo);
      }
    }

    // document all derived resources
    DerivedResource(device, Shader);
    if(CreateInfo.pSetLayouts)
    {
      for(uint32_t i = 0; i < CreateInfo.setLayoutCount; i++)
        DerivedResource(CreateInfo.pSetLayouts[i], Shader);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount,
                                           const VkShaderCreateInfoEXT *pCreateInfos,
                                           const VkAllocationCallbacks *, VkShaderEXT *pShaders)
{
  VkShaderCreateInfoEXT *unwrapped = UnwrapInfos(m_State, pCreateInfos, createInfoCount);

  // to be extra sure just in case the driver doesn't, set shader objects to VK_NULL_HANDLE first.
  for(uint32_t i = 0; i < createInfoCount; i++)
  {
    // shader binaries aren't supported, and any calls to vkGetShaderBinaryData should return a
    // valid but incompatible UUID
    if(pCreateInfos[i].codeType == VK_SHADER_CODE_TYPE_BINARY_EXT)
      return VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT;
    else
      pShaders[i] = VK_NULL_HANDLE;
  }

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateShadersEXT(Unwrap(device), createInfoCount,
                                                              unwrapped, NULL, pShaders));

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < createInfoCount; i++)
    {
      // any shader objects that are VK_NULL_HANDLE, silently ignore as they failed but we might
      // have successfully created some before then.
      if(pShaders[i] == VK_NULL_HANDLE)
        continue;

      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pShaders[i]);

      // background or active capture state
      if(IsCaptureMode(m_State))
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateShadersEXT);
          Serialise_vkCreateShadersEXT(ser, device, 1, &pCreateInfos[i], NULL, &pShaders[i]);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pShaders[i]);
        record->AddChunk(chunk);

        if(pCreateInfos[i].pSetLayouts)
        {
          for(uint32_t s = 0; s < pCreateInfos[i].setLayoutCount; s++)
          {
            VkResourceRecord *layoutrecord = GetRecord(pCreateInfos[i].pSetLayouts[s]);
            record->AddParent(layoutrecord);
          }
        }
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pShaders[i]);
        m_CreationInfo.m_ShaderObject[id].Init(GetResourceManager(), m_CreationInfo, id,
                                               &pCreateInfos[i]);
      }
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(PipelineCache, GetResID(*pPipelineCache)).TypedAs("VkPipelineCache"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipelineCache cache = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), &CreateInfo, NULL, &cache);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating pipeline cache, VkResult: %s", ToStr(ret).c_str());
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
                                              const VkAllocationCallbacks *,
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
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), &createInfo, NULL,
                                                                 pPipelineCache));

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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfos).Important();
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

    // if we have pipeline executable properties, capture the data
    if(GetExtensions(NULL).ext_KHR_pipeline_executable_properties)
    {
      CreateInfo.flags |= (VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
                           VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR);
    }

    // don't fail when a compile is required because we don't currently replay caches so this will
    // always happen. This still allows application to use this flag at runtime where it will be
    // valid
    CreateInfo.flags &= ~VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;

    VkGraphicsPipelineCreateInfo *unwrapped = UnwrapInfos(m_State, &CreateInfo, 1);
    VkResult ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache),
                                                            1, unwrapped, NULL, &pipe);

    AddResource(Pipeline, ResourceType::PipelineState, "Graphics Pipeline");

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating graphics pipeline, VkResult: %s", ToStr(ret).c_str());
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

        VkGraphicsPipelineCreateInfo shadInstantiatedInfo = CreateInfo;
        VkPipelineShaderStageCreateInfo shadInstantiations[NumShaderStages];

        // search for inline shaders, and create shader modules for them so we have objects to pull
        // out for recreating graphics pipelines (and to replace for shader editing)
        for(uint32_t s = 0; s < shadInstantiatedInfo.stageCount; s++)
        {
          shadInstantiations[s] = shadInstantiatedInfo.pStages[s];

          if(shadInstantiations[s].module == VK_NULL_HANDLE)
          {
            const VkShaderModuleCreateInfo *inlineShad =
                (const VkShaderModuleCreateInfo *)FindNextStruct(
                    &shadInstantiations[s], VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
            const VkDebugUtilsObjectNameInfoEXT *shadName =
                (const VkDebugUtilsObjectNameInfoEXT *)FindNextStruct(
                    &shadInstantiations[s], VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
            if(inlineShad)
            {
              vkCreateShaderModule(device, inlineShad, NULL, &shadInstantiations[s].module);

              // this will be a replay ID, there is no equivalent original ID
              ResourceId shadId = GetResID(shadInstantiations[s].module);

              AddResource(shadId, ResourceType::Shader, "Shader Module");
              DerivedResource(device, shadId);
              DerivedResource(pipe, shadId);

              const char *names[] = {
                  " vertex shader",
                  " tess control shader",
                  " tess eval shader",
                  " geometry shader",
                  " fragment shader",
                  NULL,
                  " task shader",
                  " mesh shader",
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
              };
              RDCCOMPILE_ASSERT(ARRAY_COUNT(names) == NumShaderStages, "Array is out of date");

              if(shadName)
                GetReplay()->GetResourceDesc(shadId).SetCustomName(shadName->pObjectName);
              else
                GetReplay()->GetResourceDesc(shadId).name =
                    GetReplay()->GetResourceDesc(Pipeline).name +
                    names[StageIndex(shadInstantiations[s].stage)];
            }
            else
            {
              RDCERR("NULL module in stage %s (entry %s) with no linked module create info",
                     ToStr(shadInstantiations[s].stage).c_str(), shadInstantiations[s].pName);
            }
          }
        }

        shadInstantiatedInfo.pStages = shadInstantiations;

        VulkanCreationInfo::Pipeline &pipeInfo = m_CreationInfo.m_Pipeline[live];

        pipeInfo.Init(GetResourceManager(), m_CreationInfo, live, &shadInstantiatedInfo);

        ResourceId renderPassID = GetResID(CreateInfo.renderPass);

        if(CreateInfo.renderPass != VK_NULL_HANDLE)
        {
          CreateInfo.renderPass =
              m_CreationInfo.m_RenderPass[renderPassID].loadRPs[CreateInfo.subpass];
          CreateInfo.subpass = 0;

          unwrapped = UnwrapInfos(m_State, &CreateInfo, 1);
          ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache), 1,
                                                         unwrapped, NULL, &pipeInfo.subpass0pipe);
          RDCASSERTEQUAL(ret, VK_SUCCESS);

          ResourceId subpass0id =
              GetResourceManager()->WrapResource(Unwrap(device), pipeInfo.subpass0pipe);

          // register as a live-only resource, so it is cleaned up properly
          GetResourceManager()->AddLiveResource(subpass0id, pipeInfo.subpass0pipe);
        }
      }
    }

    DerivedResource(device, Pipeline);
    if(origCache != VK_NULL_HANDLE)
      DerivedResource(origCache, Pipeline);
    if(CreateInfo.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    {
      if(CreateInfo.basePipelineHandle != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.basePipelineHandle, Pipeline);
    }
    if(origRP != VK_NULL_HANDLE)
      DerivedResource(origRP, Pipeline);
    if(CreateInfo.layout != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.layout, Pipeline);
    for(uint32_t i = 0; i < CreateInfo.stageCount; i++)
    {
      if(CreateInfo.pStages[i].module != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.pStages[i].module, Pipeline);
    }

    VkPipelineLibraryCreateInfoKHR *libraryInfo = (VkPipelineLibraryCreateInfoKHR *)FindNextStruct(
        &CreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR);

    if(libraryInfo)
    {
      for(uint32_t l = 0; l < libraryInfo->libraryCount; l++)
      {
        DerivedResource(libraryInfo->pLibraries[l], Pipeline);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                  uint32_t count,
                                                  const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                  const VkAllocationCallbacks *,
                                                  VkPipeline *pPipelines)
{
  VkGraphicsPipelineCreateInfo *unwrapped = UnwrapInfos(m_State, pCreateInfos, count);
  VkResult ret;

  // to be extra sure just in case the driver doesn't, set pipelines to VK_NULL_HANDLE first.
  for(uint32_t i = 0; i < count; i++)
    pPipelines[i] = VK_NULL_HANDLE;

  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateGraphicsPipelines(
                          Unwrap(device), Unwrap(pipelineCache), count, unwrapped, NULL, pPipelines));

  if(ret == VK_SUCCESS || ret == VK_PIPELINE_COMPILE_REQUIRED)
  {
    for(uint32_t i = 0; i < count; i++)
    {
      // any pipelines that are VK_NULL_HANDLE, silently ignore as they failed but we might have
      // successfully created some before then.
      if(pPipelines[i] == VK_NULL_HANDLE)
        continue;

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

            RDCDEBUG("Creating pipeline %s base is %s", ToStr(record->GetResourceID()).c_str(),
                     ToStr(baserecord->GetResourceID()).c_str());
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

        if(pCreateInfos[i].renderPass != VK_NULL_HANDLE)
        {
          VkResourceRecord *rprecord = GetRecord(pCreateInfos[i].renderPass);
          record->AddParent(rprecord);
        }

        if(pCreateInfos[i].layout != VK_NULL_HANDLE)
        {
          VkResourceRecord *layoutrecord = GetRecord(pCreateInfos[i].layout);
          record->AddParent(layoutrecord);
        }

        for(uint32_t s = 0; s < pCreateInfos[i].stageCount; s++)
        {
          VkResourceRecord *modulerecord = GetRecord(pCreateInfos[i].pStages[s].module);
          if(modulerecord)
            record->AddParent(modulerecord);
        }

        VkPipelineLibraryCreateInfoKHR *libraryInfo =
            (VkPipelineLibraryCreateInfoKHR *)FindNextStruct(
                &pCreateInfos[i], VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR);

        if(libraryInfo)
        {
          for(uint32_t l = 0; l < libraryInfo->libraryCount; l++)
          {
            record->AddParent(GetRecord(libraryInfo->pLibraries[l]));
          }
        }
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pPipelines[i]);

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, id,
                                           &pCreateInfos[i]);
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
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfos).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Pipeline, GetResID(*pPipelines)).TypedAs("VkPipeline"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkPipeline pipe = VK_NULL_HANDLE;

    VkPipelineCache origCache = pipelineCache;

    // don't use pipeline caches on replay
    pipelineCache = VK_NULL_HANDLE;

    // if we have pipeline executable properties, capture the data
    if(GetExtensions(NULL).ext_KHR_pipeline_executable_properties)
    {
      CreateInfo.flags |= (VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
                           VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR);
    }

    // don't fail when a compile is required because we don't currently replay caches so this will
    // always happen. This still allows application to use this flag at runtime where it will be
    // valid
    CreateInfo.flags &= ~VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;

    VkComputePipelineCreateInfo *unwrapped = UnwrapInfos(m_State, &CreateInfo, 1);
    VkResult ret = ObjDisp(device)->CreateComputePipelines(Unwrap(device), Unwrap(pipelineCache), 1,
                                                           unwrapped, NULL, &pipe);

    AddResource(Pipeline, ResourceType::PipelineState, "Compute Pipeline");

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating compute pipeline, VkResult: %s", ToStr(ret).c_str());
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

        VkPipelineShaderStageCreateInfo shadInstantiated = CreateInfo.stage;

        // search for inline shader, and create shader module so we have objects to pull
        // out for recreating the compute pipeline (and to replace for shader editing)
        if(shadInstantiated.module == VK_NULL_HANDLE)
        {
          const VkShaderModuleCreateInfo *inlineShad =
              (const VkShaderModuleCreateInfo *)FindNextStruct(
                  &shadInstantiated, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
          const VkDebugUtilsObjectNameInfoEXT *shadName =
              (const VkDebugUtilsObjectNameInfoEXT *)FindNextStruct(
                  &shadInstantiated, VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
          if(inlineShad)
          {
            vkCreateShaderModule(device, inlineShad, NULL, &shadInstantiated.module);

            // this will be a replay ID, there is no equivalent original ID
            ResourceId shadId = GetResID(shadInstantiated.module);

            AddResource(shadId, ResourceType::Shader, "Shader Module");
            DerivedResource(device, shadId);
            DerivedResource(pipe, shadId);

            if(shadName)
              GetReplay()->GetResourceDesc(shadId).SetCustomName(shadName->pObjectName);
            else
              GetReplay()->GetResourceDesc(shadId).name =
                  GetReplay()->GetResourceDesc(Pipeline).name + " shader";
          }
          else
          {
            RDCERR("NULL module (entry %s) with no linked module create info",
                   shadInstantiated.pName);
          }
        }

        VkComputePipelineCreateInfo shadInstantiatedInfo = CreateInfo;
        shadInstantiatedInfo.stage = shadInstantiated;

        m_CreationInfo.m_Pipeline[live].Init(GetResourceManager(), m_CreationInfo, live,
                                             &shadInstantiatedInfo);
      }
    }

    DerivedResource(device, Pipeline);
    if(origCache != VK_NULL_HANDLE)
      DerivedResource(origCache, Pipeline);
    if(CreateInfo.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    {
      if(CreateInfo.basePipelineHandle != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.basePipelineHandle, Pipeline);
    }
    DerivedResource(CreateInfo.layout, Pipeline);
    if(CreateInfo.stage.module != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.stage.module, Pipeline);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                 uint32_t count,
                                                 const VkComputePipelineCreateInfo *pCreateInfos,
                                                 const VkAllocationCallbacks *,
                                                 VkPipeline *pPipelines)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateComputePipelines(
                          Unwrap(device), Unwrap(pipelineCache), count,
                          UnwrapInfos(m_State, pCreateInfos, count), NULL, pPipelines));

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
        if(modulerecord)
          record->AddParent(modulerecord);
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pPipelines[i]);

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, id,
                                           &pCreateInfos[i]);
      }
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateRayTracingPipelinesKHR(
    SerialiserType &ser, VkDevice device, VkDeferredOperationKHR deferredOperation,
    VkPipelineCache pipelineCache, uint32_t createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR *pCreateInfos, const VkAllocationCallbacks *pAllocator,
    VkPipeline *pPipelines)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(pipelineCache);
  SERIALISE_ELEMENT(createInfoCount);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfos).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Pipeline, GetResID(*pPipelines)).TypedAs("VkPipeline"_lit);

  uint32_t captureReplayHandleSize = 0;
  bytebuf captureReplayHandles;

  if(ser.IsWriting())
  {
    if(m_RTCaptureReplayHandleSize == 0)
    {
      VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayProps = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
      };

      VkPhysicalDeviceProperties2 propBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      propBase.pNext = &rayProps;
      ObjDisp(m_PhysicalDevice)->GetPhysicalDeviceProperties2(Unwrap(m_PhysicalDevice), &propBase);

      m_RTCaptureReplayHandleSize = rayProps.shaderGroupHandleCaptureReplaySize;
    }

    RDCASSERTNOTEQUAL(m_RTCaptureReplayHandleSize, 0);

    captureReplayHandleSize = m_RTCaptureReplayHandleSize;

    captureReplayHandles.resize(captureReplayHandleSize * pCreateInfos->groupCount);

    ObjDisp(device)->GetRayTracingCaptureReplayShaderGroupHandlesKHR(
        Unwrap(device), Unwrap(*pPipelines), 0, pCreateInfos->groupCount,
        captureReplayHandles.size(), captureReplayHandles.data());
  }

  SERIALISE_ELEMENT(captureReplayHandleSize).Hidden();
  SERIALISE_ELEMENT(captureReplayHandles).Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(m_RTCaptureReplayHandleSize == 0)
    {
      VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayProps = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
      };

      VkPhysicalDeviceProperties2 propBase = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      propBase.pNext = &rayProps;
      ObjDisp(m_PhysicalDevice)->GetPhysicalDeviceProperties2(Unwrap(m_PhysicalDevice), &propBase);

      m_RTCaptureReplayHandleSize = rayProps.shaderGroupHandleCaptureReplaySize;
    }

    RDCASSERTNOTEQUAL(m_RTCaptureReplayHandleSize, 0);

    if(m_RTCaptureReplayHandleSize != captureReplayHandleSize)
    {
      SET_ERROR_RESULT(
          m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Failed to re-create RT PSO as capture/replay handle size changed from %u to %u.\n"
          "\n%s",
          captureReplayHandleSize, m_RTCaptureReplayHandleSize,
          GetPhysDeviceCompatString(false, false).c_str());
      return false;
    }

    VkPipeline pipe = VK_NULL_HANDLE;

    VkPipelineCache origCache = pipelineCache;

    // don't use pipeline caches on replay
    pipelineCache = VK_NULL_HANDLE;

    // don't fail when a compile is required because we don't currently replay caches so this will
    // always happen. This still allows application to use this flag at runtime where it will be
    // valid
    CreateInfo.flags &= ~VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;

    VkRayTracingPipelineCreateInfoKHR *unwrapped = UnwrapInfos(m_State, &CreateInfo, 1);

    // patch in the capture/replay handles we saved
    VkRayTracingShaderGroupCreateInfoKHR *groups =
        (VkRayTracingShaderGroupCreateInfoKHR *)unwrapped->pGroups;

    for(uint32_t i = 0; i < unwrapped->groupCount; i++)
      groups[i].pShaderGroupCaptureReplayHandle =
          captureReplayHandles.data() + captureReplayHandleSize * i;

    VkResult ret = ObjDisp(device)->CreateRayTracingPipelinesKHR(
        Unwrap(device), VK_NULL_HANDLE, Unwrap(pipelineCache), 1, unwrapped, NULL, &pipe);

    AddResource(Pipeline, ResourceType::PipelineState, "RT Pipeline");

    if(ret == VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
    {
      SET_ERROR_RESULT(
          m_FailedReplayResult, ResultCode::APIHardwareUnsupported,
          "Failed to re-create RT PSO because capture/replay handle was incompatible.\n"
          "\n%s",
          GetPhysDeviceCompatString(false, false).c_str());
      return false;
    }
    else if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating RT pipeline, VkResult: %s", ToStr(ret).c_str());
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

        pipeInfo.Init(GetResourceManager(), m_CreationInfo, live, &CreateInfo);
      }
    }

    DerivedResource(device, Pipeline);
    if(origCache != VK_NULL_HANDLE)
      DerivedResource(origCache, Pipeline);
    if(CreateInfo.flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    {
      if(CreateInfo.basePipelineHandle != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.basePipelineHandle, Pipeline);
    }
    if(CreateInfo.layout != VK_NULL_HANDLE)
      DerivedResource(CreateInfo.layout, Pipeline);
    for(uint32_t i = 0; i < CreateInfo.stageCount; i++)
    {
      if(CreateInfo.pStages[i].module != VK_NULL_HANDLE)
        DerivedResource(CreateInfo.pStages[i].module, Pipeline);
    }

    if(CreateInfo.pLibraryInfo)
    {
      for(uint32_t l = 0; l < CreateInfo.pLibraryInfo->libraryCount; l++)
      {
        DerivedResource(CreateInfo.pLibraryInfo->pLibraries[l], Pipeline);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateRayTracingPipelinesKHR(
    VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache,
    uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
    const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
  VkResult ret;

  // to be extra sure just in case the driver doesn't, set pipelines to VK_NULL_HANDLE first.
  for(uint32_t i = 0; i < createInfoCount; i++)
    pPipelines[i] = VK_NULL_HANDLE;

  // deferred operations are currently not wrapped
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateRayTracingPipelinesKHR(
                          Unwrap(device), deferredOperation, Unwrap(pipelineCache), createInfoCount,
                          UnwrapInfos(m_State, pCreateInfos, createInfoCount), NULL, pPipelines));

  if(ret == VK_SUCCESS || ret == VK_PIPELINE_COMPILE_REQUIRED)
  {
    for(uint32_t i = 0; i < createInfoCount; i++)
    {
      // any pipelines that are VK_NULL_HANDLE, silently ignore as they failed but we might have
      // successfully created some before then.
      if(pPipelines[i] == VK_NULL_HANDLE)
        continue;

      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pPipelines[i]);

      if(IsCaptureMode(m_State))
      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          VkRayTracingPipelineCreateInfoKHR modifiedCreateInfo;
          const VkRayTracingPipelineCreateInfoKHR *createInfo = &pCreateInfos[i];

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

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateRayTracingPipelinesKHR);
          Serialise_vkCreateRayTracingPipelinesKHR(ser, device, deferredOperation, pipelineCache, 1,
                                                   createInfo, NULL, &pPipelines[i]);

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

            RDCDEBUG("Creating pipeline %s base is %s", ToStr(record->GetResourceID()).c_str(),
                     ToStr(baserecord->GetResourceID()).c_str());
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

        if(pCreateInfos[i].layout != VK_NULL_HANDLE)
        {
          VkResourceRecord *layoutrecord = GetRecord(pCreateInfos[i].layout);
          record->AddParent(layoutrecord);
        }

        for(uint32_t s = 0; s < pCreateInfos[i].stageCount; s++)
        {
          VkResourceRecord *modulerecord = GetRecord(pCreateInfos[i].pStages[s].module);
          if(modulerecord)
            record->AddParent(modulerecord);
        }

        if(pCreateInfos[i].pLibraryInfo)
        {
          for(uint32_t l = 0; l < pCreateInfos[i].pLibraryInfo->libraryCount; l++)
          {
            record->AddParent(GetRecord(pCreateInfos[i].pLibraryInfo->pLibraries[l]));
          }
        }
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pPipelines[i]);

        m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), m_CreationInfo, id,
                                           &pCreateInfos[i]);
      }
    }
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreatePipelineLayout, VkDevice device,
                                const VkPipelineLayoutCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkPipelineLayout *pPipelineLayout);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateShaderModule, VkDevice device,
                                const VkShaderModuleCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkShaderModule *pShaderModule);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreatePipelineCache, VkDevice device,
                                const VkPipelineCacheCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkPipelineCache *pPipelineCache);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateGraphicsPipelines, VkDevice device,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                const VkAllocationCallbacks *, VkPipeline *pPipelines);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateComputePipelines, VkDevice device,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkComputePipelineCreateInfo *pCreateInfos,
                                const VkAllocationCallbacks *, VkPipeline *pPipelines);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateShadersEXT, VkDevice device,
                                uint32_t createInfoCount, const VkShaderCreateInfoEXT *pCreateInfos,
                                const VkAllocationCallbacks *, VkShaderEXT *pShaders);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateRayTracingPipelinesKHR, VkDevice device,
                                VkDeferredOperationKHR deferredOperation,
                                VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
                                const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);
