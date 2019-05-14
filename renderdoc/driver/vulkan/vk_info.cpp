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

#include "vk_info.h"
#include "3rdparty/glslang/SPIRV/spirv.hpp"

VkDynamicState ConvertDynamicState(VulkanDynamicStateIndex idx)
{
  switch(idx)
  {
    case VkDynamicViewport: return VK_DYNAMIC_STATE_VIEWPORT;
    case VkDynamicScissor: return VK_DYNAMIC_STATE_SCISSOR;
    case VkDynamicLineWidth: return VK_DYNAMIC_STATE_LINE_WIDTH;
    case VkDynamicDepthBias: return VK_DYNAMIC_STATE_DEPTH_BIAS;
    case VkDynamicBlendConstants: return VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    case VkDynamicDepthBounds: return VK_DYNAMIC_STATE_DEPTH_BOUNDS;
    case VkDynamicStencilCompareMask: return VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
    case VkDynamicStencilWriteMask: return VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
    case VkDynamicStencilReference: return VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    case VkDynamicViewportWScalingNV: return VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV;
    case VkDynamicDiscardRectangleEXT: return VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT;
    case VkDynamicSampleLocationsEXT: return VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT;
    case VkDynamicViewportShadingRatePaletteNV:
      return VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV;
    case VkDynamicViewportCoarseSampleOrderNV:
      return VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV;
    case VkDynamicExclusiveScissorNV: return VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV;
    case VkDynamicCount: break;
  }

  RDCERR("Unexpected vulkan dynamic state index %u", idx);

  return VK_DYNAMIC_STATE_MAX_ENUM;
}

VulkanDynamicStateIndex ConvertDynamicState(VkDynamicState state)
{
  switch(state)
  {
    case VK_DYNAMIC_STATE_VIEWPORT: return VkDynamicViewport;
    case VK_DYNAMIC_STATE_SCISSOR: return VkDynamicScissor;
    case VK_DYNAMIC_STATE_LINE_WIDTH: return VkDynamicLineWidth;
    case VK_DYNAMIC_STATE_DEPTH_BIAS: return VkDynamicDepthBias;
    case VK_DYNAMIC_STATE_BLEND_CONSTANTS: return VkDynamicBlendConstants;
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS: return VkDynamicDepthBounds;
    case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK: return VkDynamicStencilCompareMask;
    case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK: return VkDynamicStencilWriteMask;
    case VK_DYNAMIC_STATE_STENCIL_REFERENCE: return VkDynamicStencilReference;
    case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV: return VkDynamicViewportWScalingNV;
    case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT: return VkDynamicDiscardRectangleEXT;
    case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT: return VkDynamicSampleLocationsEXT;
    case VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV:
      return VkDynamicViewportShadingRatePaletteNV;
    case VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV:
      return VkDynamicViewportCoarseSampleOrderNV;
    case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV: return VkDynamicExclusiveScissorNV;
    case VK_DYNAMIC_STATE_RANGE_SIZE:
    case VK_DYNAMIC_STATE_MAX_ENUM: break;
  }

  RDCERR("Unexpected vulkan state %u", state);

  return VkDynamicCount;
}

void DescSetLayout::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                         const VkDescriptorSetLayoutCreateInfo *pCreateInfo)
{
  dynamicCount = 0;

  flags = pCreateInfo->flags;

  // descriptor set layouts can be sparse, such that only three bindings exist
  // but they are at 0, 5 and 10.
  // We assume here that while the layouts may be sparse that's mostly to allow
  // multiple layouts to co-exist nicely, and that we can allocate our bindings
  // array to cover the whole size, and leave some elements unused.

  // will be at least this size.
  bindings.resize(pCreateInfo->bindingCount);
  for(uint32_t i = 0; i < pCreateInfo->bindingCount; i++)
  {
    uint32_t b = pCreateInfo->pBindings[i].binding;
    // expand to fit the binding
    if(b >= bindings.size())
      bindings.resize(b + 1);

    bindings[b].descriptorCount = pCreateInfo->pBindings[i].descriptorCount;
    bindings[b].descriptorType = pCreateInfo->pBindings[i].descriptorType;
    bindings[b].stageFlags = pCreateInfo->pBindings[i].stageFlags;

    if(bindings[b].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
       bindings[b].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
      dynamicCount++;

    if(pCreateInfo->pBindings[i].pImmutableSamplers)
    {
      bindings[b].immutableSampler = new ResourceId[bindings[b].descriptorCount];

      for(uint32_t s = 0; s < bindings[b].descriptorCount; s++)
        bindings[b].immutableSampler[s] = GetResID(pCreateInfo->pBindings[i].pImmutableSamplers[s]);
    }
  }
}

void DescSetLayout::CreateBindingsArray(std::vector<DescriptorSetBindingElement *> &descBindings) const
{
  descBindings.resize(bindings.size());
  for(size_t i = 0; i < bindings.size(); i++)
  {
    descBindings[i] = new DescriptorSetBindingElement[bindings[i].descriptorCount];
    memset(descBindings[i], 0, sizeof(DescriptorSetBindingElement) * bindings[i].descriptorCount);
  }
}

void DescSetLayout::UpdateBindingsArray(const DescSetLayout &prevLayout,
                                        std::vector<DescriptorSetBindingElement *> &descBindings) const
{
  // if we have fewer bindings now, delete the orphaned bindings arrays
  for(size_t i = bindings.size(); i < prevLayout.bindings.size(); i++)
    SAFE_DELETE_ARRAY(descBindings[i]);

  // resize to the new number of bindings
  descBindings.resize(bindings.size());

  // re-allocate slots and move any previous bindings that overlapped over.
  for(size_t i = 0; i < bindings.size(); i++)
  {
    // allocate new slot array
    DescriptorSetBindingElement *newSlots =
        new DescriptorSetBindingElement[bindings[i].descriptorCount];
    memset(newSlots, 0, sizeof(DescriptorSetBindingElement) * bindings[i].descriptorCount);

    // copy over any previous bindings that overlapped
    if(i < prevLayout.bindings.size())
      memcpy(newSlots, descBindings[i],
             sizeof(DescriptorSetBindingElement) *
                 RDCMIN(prevLayout.bindings[i].descriptorCount, bindings[i].descriptorCount));

    // delete old array, and assign the new one
    SAFE_DELETE_ARRAY(descBindings[i]);
    descBindings[i] = newSlots;
  }
}

bool DescSetLayout::operator==(const DescSetLayout &other) const
{
  // shortcut for equality to ourselves
  if(this == &other)
    return true;

  // descriptor set layouts are different if they have different set of bindings.
  if(bindings.size() != other.bindings.size())
    return false;

  // iterate over each binding (we know this loop indexes validly in both arrays
  for(size_t i = 0; i < bindings.size(); i++)
  {
    const Binding &a = bindings[i];
    const Binding &b = other.bindings[i];

    // if the type/stages/count are different, the layout is different
    if(a.descriptorCount != b.descriptorCount || a.descriptorType != b.descriptorType ||
       a.stageFlags != b.stageFlags)
      return false;

    // if one has immutable samplers but the other doesn't, they're different
    if((a.immutableSampler && !b.immutableSampler) || (!a.immutableSampler && b.immutableSampler))
      return false;

    // if we DO have immutable samplers, they must all point to the same sampler objects.
    if(a.immutableSampler)
    {
      for(uint32_t s = 0; s < a.descriptorCount; s++)
      {
        if(a.immutableSampler[s] != b.immutableSampler[s])
          return false;
      }
    }
  }

  return true;
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                        const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  layout = GetResID(pCreateInfo->layout);
  renderpass = GetResID(pCreateInfo->renderPass);
  subpass = pCreateInfo->subpass;

  RDCEraseEl(dynamicStates);
  if(pCreateInfo->pDynamicState)
  {
    for(uint32_t i = 0; i < pCreateInfo->pDynamicState->dynamicStateCount; i++)
      dynamicStates[ConvertDynamicState(pCreateInfo->pDynamicState->pDynamicStates[i])] = true;
  }

  // VkPipelineShaderStageCreateInfo
  for(uint32_t i = 0; i < pCreateInfo->stageCount; i++)
  {
    ResourceId id = GetResID(pCreateInfo->pStages[i].module);

    // convert shader bit to shader index
    int stageIndex = StageIndex(pCreateInfo->pStages[i].stage);

    Shader &shad = shaders[stageIndex];

    shad.module = id;
    shad.entryPoint = pCreateInfo->pStages[i].pName;

    ShaderModule::Reflection &reflData = info.m_ShaderModule[id].m_Reflections[shad.entryPoint];

    reflData.Init(resourceMan, id, info.m_ShaderModule[id].spirv, shad.entryPoint,
                  pCreateInfo->pStages[i].stage);

    if(pCreateInfo->pStages[i].pSpecializationInfo)
    {
      const byte *data = (const byte *)pCreateInfo->pStages[i].pSpecializationInfo->pData;

      const VkSpecializationMapEntry *maps = pCreateInfo->pStages[i].pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->pStages[i].pSpecializationInfo->mapEntryCount; s++)
      {
        SpecConstant spec;
        spec.specID = maps[s].constantID;
        spec.data.assign(data + maps[s].offset, data + maps[s].offset + maps[s].size);
        // ignore maps[s].size, assume it's enough for the type
        shad.specialization.push_back(spec);
      }
    }

    shad.refl = &reflData.refl;
    shad.mapping = &reflData.mapping;
    shad.patchData = &reflData.patchData;
  }

  if(pCreateInfo->pVertexInputState)
  {
    vertexBindings.resize(pCreateInfo->pVertexInputState->vertexBindingDescriptionCount);
    for(uint32_t i = 0; i < pCreateInfo->pVertexInputState->vertexBindingDescriptionCount; i++)
    {
      vertexBindings[i].vbufferBinding =
          pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].binding;
      vertexBindings[i].bytestride =
          pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].stride;
      vertexBindings[i].perInstance =
          pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].inputRate ==
          VK_VERTEX_INPUT_RATE_INSTANCE;
      vertexBindings[i].instanceDivisor = 1;
    }

    // if there's a divisors struct, apply them now
    const VkPipelineVertexInputDivisorStateCreateInfoEXT *divisors =
        (const VkPipelineVertexInputDivisorStateCreateInfoEXT *)FindNextStruct(
            pCreateInfo->pVertexInputState,
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT);
    if(divisors)
    {
      for(uint32_t b = 0; b < divisors->vertexBindingDivisorCount; b++)
      {
        const VkVertexInputBindingDivisorDescriptionEXT &div = divisors->pVertexBindingDivisors[b];

        if(div.binding < vertexBindings.size())
          vertexBindings[div.binding].instanceDivisor = div.divisor;
      }
    }

    vertexAttrs.resize(pCreateInfo->pVertexInputState->vertexAttributeDescriptionCount);
    for(uint32_t i = 0; i < pCreateInfo->pVertexInputState->vertexAttributeDescriptionCount; i++)
    {
      vertexAttrs[i].binding =
          pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].binding;
      vertexAttrs[i].location =
          pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].location;
      vertexAttrs[i].format = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].format;
      vertexAttrs[i].byteoffset =
          pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].offset;
    }
  }

  topology = pCreateInfo->pInputAssemblyState->topology;
  primitiveRestartEnable = pCreateInfo->pInputAssemblyState->primitiveRestartEnable ? true : false;

  if(pCreateInfo->pTessellationState)
    patchControlPoints = pCreateInfo->pTessellationState->patchControlPoints;
  else
    patchControlPoints = 0;

  tessellationDomainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

  const VkPipelineTessellationDomainOriginStateCreateInfo *tessDomain =
      (const VkPipelineTessellationDomainOriginStateCreateInfo *)FindNextStruct(
          pCreateInfo->pTessellationState,
          VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO);
  if(tessDomain)
    tessellationDomainOrigin = tessDomain->domainOrigin;

  if(pCreateInfo->pViewportState)
    viewportCount = pCreateInfo->pViewportState->viewportCount;
  else
    viewportCount = 0;

  viewports.resize(viewportCount);
  scissors.resize(viewportCount);

  for(uint32_t i = 0; i < viewportCount; i++)
  {
    if(pCreateInfo->pViewportState->pViewports)
      viewports[i] = pCreateInfo->pViewportState->pViewports[i];

    if(pCreateInfo->pViewportState->pScissors)
      scissors[i] = pCreateInfo->pViewportState->pScissors[i];
  }

  // VkPipelineDiscardRectangleStateCreateInfoEXT
  discardMode = VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT;

  const VkPipelineDiscardRectangleStateCreateInfoEXT *discardRects =
      (const VkPipelineDiscardRectangleStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT);
  if(discardRects)
  {
    discardRectangles.resize(discardRects->discardRectangleCount);

    if(discardRects->pDiscardRectangles)
    {
      for(uint32_t i = 0; i < discardRects->discardRectangleCount; i++)
        discardRectangles[i] = discardRects->pDiscardRectangles[i];
    }

    discardMode = discardRects->discardRectangleMode;
  }

  // VkPipelineRasterStateCreateInfo
  depthClampEnable = pCreateInfo->pRasterizationState->depthClampEnable ? true : false;
  rasterizerDiscardEnable = pCreateInfo->pRasterizationState->rasterizerDiscardEnable ? true : false;
  polygonMode = pCreateInfo->pRasterizationState->polygonMode;
  cullMode = pCreateInfo->pRasterizationState->cullMode;
  frontFace = pCreateInfo->pRasterizationState->frontFace;
  depthBiasEnable = pCreateInfo->pRasterizationState->depthBiasEnable ? true : false;
  depthBiasConstantFactor = pCreateInfo->pRasterizationState->depthBiasConstantFactor;
  depthBiasClamp = pCreateInfo->pRasterizationState->depthBiasClamp;
  depthBiasSlopeFactor = pCreateInfo->pRasterizationState->depthBiasSlopeFactor;
  lineWidth = pCreateInfo->pRasterizationState->lineWidth;

  // VkPipelineRasterizationStateStreamCreateInfoEXT
  rasterizationStream = 0;

  const VkPipelineRasterizationStateStreamCreateInfoEXT *rastStream =
      (const VkPipelineRasterizationStateStreamCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT);
  if(rastStream)
    rasterizationStream = rastStream->rasterizationStream;

  // VkPipelineRasterizationStateStreamCreateInfoEXT

  // default to the opposite of depthClampEnable
  depthClipEnable = !depthClampEnable;

  const VkPipelineRasterizationDepthClipStateCreateInfoEXT *depthClipState =
      (const VkPipelineRasterizationDepthClipStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT);
  if(depthClipState)
    depthClipEnable = depthClipState->depthClipEnable != VK_FALSE;

  // VkPipelineRasterizationConservativeStateCreateInfoEXT
  conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
  extraPrimitiveOverestimationSize = 0.0f;

  const VkPipelineRasterizationConservativeStateCreateInfoEXT *conservRast =
      (const VkPipelineRasterizationConservativeStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT);
  if(conservRast)
  {
    conservativeRasterizationMode = conservRast->conservativeRasterizationMode;
    extraPrimitiveOverestimationSize = conservRast->extraPrimitiveOverestimationSize;
  }

  // VkPipelineMultisampleStateCreateInfo
  if(pCreateInfo->pMultisampleState)
  {
    rasterizationSamples = pCreateInfo->pMultisampleState->rasterizationSamples;
    sampleShadingEnable = pCreateInfo->pMultisampleState->sampleShadingEnable ? true : false;
    minSampleShading = pCreateInfo->pMultisampleState->minSampleShading;
    sampleMask = pCreateInfo->pMultisampleState->pSampleMask
                     ? *pCreateInfo->pMultisampleState->pSampleMask
                     : ~0U;
    alphaToCoverageEnable = pCreateInfo->pMultisampleState->alphaToCoverageEnable ? true : false;
    alphaToOneEnable = pCreateInfo->pMultisampleState->alphaToOneEnable ? true : false;

    // VkPipelineSampleLocationsStateCreateInfoEXT
    sampleLocations.enabled = false;
    sampleLocations.gridSize.width = 1;
    sampleLocations.gridSize.height = 1;

    const VkPipelineSampleLocationsStateCreateInfoEXT *sampleLoc =
        (const VkPipelineSampleLocationsStateCreateInfoEXT *)FindNextStruct(
            pCreateInfo->pMultisampleState,
            VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT);
    if(sampleLoc)
    {
      sampleLocations.enabled = sampleLoc->sampleLocationsEnable ? true : false;

      if(!dynamicStates[VkDynamicSampleLocationsEXT])
      {
        sampleLocations.gridSize = sampleLoc->sampleLocationsInfo.sampleLocationGridSize;
        sampleLocations.locations.insert(sampleLocations.locations.begin(),
                                         sampleLoc->sampleLocationsInfo.pSampleLocations,
                                         sampleLoc->sampleLocationsInfo.pSampleLocations +
                                             sampleLoc->sampleLocationsInfo.sampleLocationsCount);

        RDCASSERTEQUAL(sampleLoc->sampleLocationsInfo.sampleLocationsPerPixel, rasterizationSamples);
      }
    }
  }
  else
  {
    rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    sampleShadingEnable = false;
    minSampleShading = 1.0f;
    sampleMask = ~0U;
    alphaToCoverageEnable = false;
    alphaToOneEnable = false;
  }

  // VkPipelineDepthStencilStateCreateInfo
  if(pCreateInfo->pDepthStencilState)
  {
    depthTestEnable = pCreateInfo->pDepthStencilState->depthTestEnable ? true : false;
    depthWriteEnable = pCreateInfo->pDepthStencilState->depthWriteEnable ? true : false;
    depthCompareOp = pCreateInfo->pDepthStencilState->depthCompareOp;
    depthBoundsEnable = pCreateInfo->pDepthStencilState->depthBoundsTestEnable ? true : false;
    stencilTestEnable = pCreateInfo->pDepthStencilState->stencilTestEnable ? true : false;
    front = pCreateInfo->pDepthStencilState->front;
    back = pCreateInfo->pDepthStencilState->back;
    minDepthBounds = pCreateInfo->pDepthStencilState->minDepthBounds;
    maxDepthBounds = pCreateInfo->pDepthStencilState->maxDepthBounds;
  }
  else
  {
    depthTestEnable = false;
    depthWriteEnable = false;
    depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthBoundsEnable = false;
    stencilTestEnable = false;
    front.failOp = VK_STENCIL_OP_KEEP;
    front.passOp = VK_STENCIL_OP_KEEP;
    front.depthFailOp = VK_STENCIL_OP_KEEP;
    front.compareOp = VK_COMPARE_OP_ALWAYS;
    front.compareMask = 0xff;
    front.writeMask = 0xff;
    front.reference = 0;
    back = front;
    minDepthBounds = 0.0f;
    maxDepthBounds = 1.0f;
  }

  // VkPipelineColorBlendStateCreateInfo
  if(pCreateInfo->pColorBlendState)
  {
    logicOpEnable = pCreateInfo->pColorBlendState->logicOpEnable ? true : false;
    logicOp = pCreateInfo->pColorBlendState->logicOp;
    memcpy(blendConst, pCreateInfo->pColorBlendState->blendConstants, sizeof(blendConst));

    attachments.resize(pCreateInfo->pColorBlendState->attachmentCount);

    for(uint32_t i = 0; i < pCreateInfo->pColorBlendState->attachmentCount; i++)
    {
      attachments[i].blendEnable =
          pCreateInfo->pColorBlendState->pAttachments[i].blendEnable ? true : false;

      attachments[i].blend.Source =
          pCreateInfo->pColorBlendState->pAttachments[i].srcColorBlendFactor;
      attachments[i].blend.Destination =
          pCreateInfo->pColorBlendState->pAttachments[i].dstColorBlendFactor;
      attachments[i].blend.Operation = pCreateInfo->pColorBlendState->pAttachments[i].colorBlendOp;

      attachments[i].alphaBlend.Source =
          pCreateInfo->pColorBlendState->pAttachments[i].srcAlphaBlendFactor;
      attachments[i].alphaBlend.Destination =
          pCreateInfo->pColorBlendState->pAttachments[i].dstAlphaBlendFactor;
      attachments[i].alphaBlend.Operation =
          pCreateInfo->pColorBlendState->pAttachments[i].alphaBlendOp;

      attachments[i].channelWriteMask =
          (uint8_t)pCreateInfo->pColorBlendState->pAttachments[i].colorWriteMask;
    }
  }
  else
  {
    logicOpEnable = false;
    logicOp = VK_LOGIC_OP_NO_OP;
    RDCEraseEl(blendConst);

    attachments.clear();
  }
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                        const VkComputePipelineCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  layout = GetResID(pCreateInfo->layout);

  // need to figure out which states are valid to be NULL

  // VkPipelineShaderStageCreateInfo
  {
    ResourceId id = GetResID(pCreateInfo->stage.module);
    Shader &shad = shaders[5];    // 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)

    shad.module = id;
    shad.entryPoint = pCreateInfo->stage.pName;

    ShaderModule::Reflection &reflData = info.m_ShaderModule[id].m_Reflections[shad.entryPoint];

    reflData.Init(resourceMan, id, info.m_ShaderModule[id].spirv, shad.entryPoint,
                  pCreateInfo->stage.stage);

    if(pCreateInfo->stage.pSpecializationInfo)
    {
      const byte *data = (const byte *)pCreateInfo->stage.pSpecializationInfo->pData;

      const VkSpecializationMapEntry *maps = pCreateInfo->stage.pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->stage.pSpecializationInfo->mapEntryCount; s++)
      {
        SpecConstant spec;
        spec.specID = maps[s].constantID;
        spec.data.assign(data + maps[s].offset, data + maps[s].offset + maps[s].size);
        // ignore maps[s].size, assume it's enough for the type
        shad.specialization.push_back(spec);
      }
    }

    shad.refl = &reflData.refl;
    shad.mapping = &reflData.mapping;
    shad.patchData = &reflData.patchData;
  }

  topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  primitiveRestartEnable = false;

  patchControlPoints = 0;

  tessellationDomainOrigin = VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

  viewportCount = 0;

  // VkPipelineRasterStateCreateInfo
  depthClampEnable = false;
  rasterizerDiscardEnable = false;
  polygonMode = VK_POLYGON_MODE_FILL;
  cullMode = VK_CULL_MODE_NONE;
  frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  // VkPipelineRasterizationConservativeStateCreateInfoEXT
  conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
  extraPrimitiveOverestimationSize = 0.0f;

  // VkPipelineMultisampleStateCreateInfo
  rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  sampleShadingEnable = false;
  minSampleShading = 1.0f;
  sampleMask = ~0U;

  // VkPipelineDepthStencilStateCreateInfo
  depthTestEnable = false;
  depthWriteEnable = false;
  depthCompareOp = VK_COMPARE_OP_ALWAYS;
  depthBoundsEnable = false;
  stencilTestEnable = false;
  RDCEraseEl(front);
  RDCEraseEl(back);

  // VkPipelineColorBlendStateCreateInfo
  alphaToCoverageEnable = false;
  logicOpEnable = false;
  logicOp = VK_LOGIC_OP_NO_OP;
}

void VulkanCreationInfo::PipelineLayout::Init(VulkanResourceManager *resourceMan,
                                              VulkanCreationInfo &info,
                                              const VkPipelineLayoutCreateInfo *pCreateInfo)
{
  if(pCreateInfo->pSetLayouts)
  {
    descSetLayouts.resize(pCreateInfo->setLayoutCount);
    for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
      descSetLayouts[i] = GetResID(pCreateInfo->pSetLayouts[i]);
  }

  if(pCreateInfo->pPushConstantRanges)
  {
    pushRanges.reserve(pCreateInfo->pushConstantRangeCount);
    for(uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; i++)
      pushRanges.push_back(pCreateInfo->pPushConstantRanges[i]);
  }
}

void VulkanCreationInfo::RenderPass::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkRenderPassCreateInfo *pCreateInfo)
{
  attachments.resize(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
  {
    Attachment &dst = attachments[i];
    dst.flags = pCreateInfo->pAttachments[i].flags;
    dst.format = pCreateInfo->pAttachments[i].format;
    dst.samples = pCreateInfo->pAttachments[i].samples;
    dst.loadOp = pCreateInfo->pAttachments[i].loadOp;
    dst.storeOp = pCreateInfo->pAttachments[i].storeOp;
    dst.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
    dst.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
    dst.initialLayout = pCreateInfo->pAttachments[i].initialLayout;
    dst.finalLayout = pCreateInfo->pAttachments[i].finalLayout;
  }

  // VK_KHR_multiview
  const VkRenderPassMultiviewCreateInfo *multiview =
      (const VkRenderPassMultiviewCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);

  // VK_EXT_fragment_density_map
  const VkRenderPassFragmentDensityMapCreateInfoEXT *fragmentDensity =
      (const VkRenderPassFragmentDensityMapCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);

  subpasses.resize(pCreateInfo->subpassCount);
  for(uint32_t subp = 0; subp < pCreateInfo->subpassCount; subp++)
  {
    const VkSubpassDescription &src = pCreateInfo->pSubpasses[subp];
    Subpass &dst = subpasses[subp];

    dst.inputAttachments.resize(src.inputAttachmentCount);
    dst.inputLayouts.resize(src.inputAttachmentCount);
    for(uint32_t i = 0; i < src.inputAttachmentCount; i++)
    {
      dst.inputAttachments[i] = src.pInputAttachments[i].attachment;
      dst.inputLayouts[i] = src.pInputAttachments[i].layout;
    }

    dst.colorAttachments.resize(src.colorAttachmentCount);
    dst.resolveAttachments.resize(src.colorAttachmentCount);
    dst.colorLayouts.resize(src.colorAttachmentCount);
    for(uint32_t i = 0; i < src.colorAttachmentCount; i++)
    {
      dst.resolveAttachments[i] =
          src.pResolveAttachments ? src.pResolveAttachments[i].attachment : ~0U;
      dst.colorAttachments[i] = src.pColorAttachments[i].attachment;
      dst.colorLayouts[i] = src.pColorAttachments[i].layout;
    }

    dst.depthstencilAttachment =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? (int32_t)src.pDepthStencilAttachment->attachment
             : -1);
    dst.depthstencilLayout = (src.pDepthStencilAttachment != NULL &&
                                      src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
                                  ? src.pDepthStencilAttachment->layout
                                  : VK_IMAGE_LAYOUT_UNDEFINED);

    dst.fragmentDensityAttachment =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.attachment
             : -1);

    dst.fragmentDensityLayout =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    if(multiview && multiview->subpassCount > 0)
    {
      uint32_t mask = multiview->pViewMasks[subp];
      for(uint32_t i = 0; i < 32; i++)
      {
        if(mask & (1 << i))
          dst.multiviews.push_back(i);
      }
    }
  }
}

void VulkanCreationInfo::RenderPass::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkRenderPassCreateInfo2KHR *pCreateInfo)
{
  attachments.resize(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
  {
    Attachment &dst = attachments[i];
    dst.flags = pCreateInfo->pAttachments[i].flags;
    dst.format = pCreateInfo->pAttachments[i].format;
    dst.samples = pCreateInfo->pAttachments[i].samples;
    dst.loadOp = pCreateInfo->pAttachments[i].loadOp;
    dst.storeOp = pCreateInfo->pAttachments[i].storeOp;
    dst.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
    dst.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
    dst.initialLayout = pCreateInfo->pAttachments[i].initialLayout;
    dst.finalLayout = pCreateInfo->pAttachments[i].finalLayout;
  }

  // VK_EXT_fragment_density_map
  const VkRenderPassFragmentDensityMapCreateInfoEXT *fragmentDensity =
      (const VkRenderPassFragmentDensityMapCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);

  subpasses.resize(pCreateInfo->subpassCount);
  for(uint32_t subp = 0; subp < pCreateInfo->subpassCount; subp++)
  {
    const VkSubpassDescription2KHR &src = pCreateInfo->pSubpasses[subp];
    Subpass &dst = subpasses[subp];

    dst.inputAttachments.resize(src.inputAttachmentCount);
    dst.inputLayouts.resize(src.inputAttachmentCount);
    for(uint32_t i = 0; i < src.inputAttachmentCount; i++)
    {
      dst.inputAttachments[i] = src.pInputAttachments[i].attachment;
      dst.inputLayouts[i] = src.pInputAttachments[i].layout;
    }

    dst.colorAttachments.resize(src.colorAttachmentCount);
    dst.resolveAttachments.resize(src.colorAttachmentCount);
    dst.colorLayouts.resize(src.colorAttachmentCount);
    for(uint32_t i = 0; i < src.colorAttachmentCount; i++)
    {
      dst.resolveAttachments[i] =
          src.pResolveAttachments ? src.pResolveAttachments[i].attachment : ~0U;
      dst.colorAttachments[i] = src.pColorAttachments[i].attachment;
      dst.colorLayouts[i] = src.pColorAttachments[i].layout;
    }

    dst.depthstencilAttachment =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? (int32_t)src.pDepthStencilAttachment->attachment
             : -1);
    dst.depthstencilLayout = (src.pDepthStencilAttachment != NULL &&
                                      src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
                                  ? src.pDepthStencilAttachment->layout
                                  : VK_IMAGE_LAYOUT_UNDEFINED);

    dst.fragmentDensityAttachment =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.attachment
             : -1);

    dst.fragmentDensityLayout =
        (fragmentDensity &&
                 fragmentDensity->fragmentDensityMapAttachment.attachment != VK_ATTACHMENT_UNUSED
             ? fragmentDensity->fragmentDensityMapAttachment.layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    for(uint32_t i = 0; i < 32; i++)
    {
      if(src.viewMask & (1 << i))
        dst.multiviews.push_back(i);
    }
  }
}

void VulkanCreationInfo::Framebuffer::Init(VulkanResourceManager *resourceMan,
                                           VulkanCreationInfo &info,
                                           const VkFramebufferCreateInfo *pCreateInfo)
{
  width = pCreateInfo->width;
  height = pCreateInfo->height;
  layers = pCreateInfo->layers;

  attachments.resize(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
  {
    attachments[i].view = GetResID(pCreateInfo->pAttachments[i]);
    attachments[i].format = info.m_ImageView[attachments[i].view].format;
  }
}

void VulkanCreationInfo::Memory::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkMemoryAllocateInfo *pAllocInfo)
{
  memoryTypeIndex = pAllocInfo->memoryTypeIndex;
  size = pAllocInfo->allocationSize;
}

void VulkanCreationInfo::Buffer::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkBufferCreateInfo *pCreateInfo)
{
  usage = pCreateInfo->usage;
  size = pCreateInfo->size;
}

void VulkanCreationInfo::BufferView::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkBufferViewCreateInfo *pCreateInfo)
{
  buffer = GetResID(pCreateInfo->buffer);
  format = pCreateInfo->format;
  offset = pCreateInfo->offset;
  size = pCreateInfo->range;
}

void VulkanCreationInfo::Image::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                     const VkImageCreateInfo *pCreateInfo)
{
  type = pCreateInfo->imageType;
  format = pCreateInfo->format;
  extent = pCreateInfo->extent;
  arrayLayers = pCreateInfo->arrayLayers;
  mipLevels = pCreateInfo->mipLevels;
  samples = RDCMAX(VK_SAMPLE_COUNT_1_BIT, pCreateInfo->samples);

  creationFlags = TextureCategory::NoFlags;

  if(pCreateInfo->usage & VK_IMAGE_USAGE_SAMPLED_BIT)
    creationFlags |= TextureCategory::ShaderRead;
  if(pCreateInfo->usage &
     (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT))
    creationFlags |= TextureCategory::ColorTarget;
  if(pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    creationFlags |= TextureCategory::DepthTarget;
  if(pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT)
    creationFlags |= TextureCategory::ShaderReadWrite;

  cube = (pCreateInfo->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) ? true : false;
}

void VulkanCreationInfo::Sampler::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                       const VkSamplerCreateInfo *pCreateInfo)
{
  magFilter = pCreateInfo->magFilter;
  minFilter = pCreateInfo->minFilter;
  mipmapMode = pCreateInfo->mipmapMode;
  address[0] = pCreateInfo->addressModeU;
  address[1] = pCreateInfo->addressModeV;
  address[2] = pCreateInfo->addressModeW;
  mipLodBias = pCreateInfo->mipLodBias;
  maxAnisotropy = pCreateInfo->anisotropyEnable ? pCreateInfo->maxAnisotropy : 1.0f;
  compareEnable = pCreateInfo->compareEnable != 0;
  compareOp = pCreateInfo->compareOp;
  minLod = pCreateInfo->minLod;
  maxLod = pCreateInfo->maxLod;
  borderColor = pCreateInfo->borderColor;
  unnormalizedCoordinates = pCreateInfo->unnormalizedCoordinates != 0;

  reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT;

  const VkSamplerReductionModeCreateInfoEXT *reduction =
      (const VkSamplerReductionModeCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT);
  if(reduction)
  {
    reductionMode = reduction->reductionMode;
  }

  const VkSamplerYcbcrConversionInfo *ycbcrInfo =
      (const VkSamplerYcbcrConversionInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
  if(ycbcrInfo)
  {
    ycbcr = GetResID(ycbcrInfo->conversion);
  }
}

static TextureSwizzle Convert(VkComponentSwizzle s, int i)
{
  switch(s)
  {
    default: RDCWARN("Unexpected component swizzle value %d", (int)s);
    case VK_COMPONENT_SWIZZLE_IDENTITY: break;
    case VK_COMPONENT_SWIZZLE_ZERO: return TextureSwizzle::Zero;
    case VK_COMPONENT_SWIZZLE_ONE: return TextureSwizzle::One;
    case VK_COMPONENT_SWIZZLE_R: return TextureSwizzle::Red;
    case VK_COMPONENT_SWIZZLE_G: return TextureSwizzle::Green;
    case VK_COMPONENT_SWIZZLE_B: return TextureSwizzle::Blue;
    case VK_COMPONENT_SWIZZLE_A: return TextureSwizzle::Alpha;
  }

  return TextureSwizzle(uint32_t(TextureSwizzle::Red) + i);
}

void VulkanCreationInfo::YCbCrSampler::Init(VulkanResourceManager *resourceMan,
                                            VulkanCreationInfo &info,
                                            const VkSamplerYcbcrConversionCreateInfo *pCreateInfo)
{
  switch(pCreateInfo->ycbcrModel)
  {
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY: ycbcrModel = YcbcrConversion::Raw; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY:
      ycbcrModel = YcbcrConversion::RangeOnly;
      break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709: ycbcrModel = YcbcrConversion::BT709; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601: ycbcrModel = YcbcrConversion::BT601; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020: ycbcrModel = YcbcrConversion::BT2020; break;
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_MAX_ENUM:
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_RANGE_SIZE: break;
  }

  switch(pCreateInfo->ycbcrRange)
  {
    case VK_SAMPLER_YCBCR_RANGE_ITU_FULL: ycbcrRange = YcbcrRange::ITUFull; break;
    case VK_SAMPLER_YCBCR_RANGE_ITU_NARROW: ycbcrRange = YcbcrRange::ITUNarrow; break;
    case VK_SAMPLER_YCBCR_RANGE_MAX_ENUM:
    case VK_SAMPLER_YCBCR_RANGE_RANGE_SIZE: break;
  }

  switch(pCreateInfo->xChromaOffset)
  {
    case VK_CHROMA_LOCATION_COSITED_EVEN: xChromaOffset = ChromaSampleLocation::CositedEven; break;
    case VK_CHROMA_LOCATION_MIDPOINT: xChromaOffset = ChromaSampleLocation::Midpoint; break;
    case VK_CHROMA_LOCATION_MAX_ENUM:
    case VK_CHROMA_LOCATION_RANGE_SIZE: break;
  }

  switch(pCreateInfo->yChromaOffset)
  {
    case VK_CHROMA_LOCATION_COSITED_EVEN: yChromaOffset = ChromaSampleLocation::CositedEven; break;
    case VK_CHROMA_LOCATION_MIDPOINT: yChromaOffset = ChromaSampleLocation::Midpoint; break;
    case VK_CHROMA_LOCATION_MAX_ENUM:
    case VK_CHROMA_LOCATION_RANGE_SIZE: break;
  }

  swizzle[0] = Convert(pCreateInfo->components.r, 0);
  swizzle[1] = Convert(pCreateInfo->components.g, 1);
  swizzle[2] = Convert(pCreateInfo->components.b, 2);
  swizzle[3] = Convert(pCreateInfo->components.a, 3);
  chromaFilter = MakeFilterMode(pCreateInfo->chromaFilter);
  forceExplicitReconstruction = pCreateInfo->forceExplicitReconstruction != 0;
}

void VulkanCreationInfo::ImageView::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                         const VkImageViewCreateInfo *pCreateInfo)
{
  image = GetResID(pCreateInfo->image);
  format = pCreateInfo->format;
  range = pCreateInfo->subresourceRange;

  if(range.levelCount == VK_REMAINING_MIP_LEVELS)
    range.levelCount = info.m_Image[image].mipLevels - range.baseMipLevel;

  if(range.layerCount == VK_REMAINING_ARRAY_LAYERS)
    range.layerCount = info.m_Image[image].arrayLayers - range.baseArrayLayer;

  swizzle[0] = Convert(pCreateInfo->components.r, 0);
  swizzle[1] = Convert(pCreateInfo->components.g, 1);
  swizzle[2] = Convert(pCreateInfo->components.b, 2);
  swizzle[3] = Convert(pCreateInfo->components.a, 3);
}

void VulkanCreationInfo::ShaderModule::Init(VulkanResourceManager *resourceMan,
                                            VulkanCreationInfo &info,
                                            const VkShaderModuleCreateInfo *pCreateInfo)
{
  const uint32_t SPIRVMagic = 0x07230203;
  if(pCreateInfo->codeSize < 4 || memcmp(pCreateInfo->pCode, &SPIRVMagic, sizeof(SPIRVMagic)))
  {
    RDCWARN("Shader not provided with SPIR-V");
  }
  else
  {
    RDCASSERT(pCreateInfo->codeSize % sizeof(uint32_t) == 0);
    ParseSPIRV((uint32_t *)pCreateInfo->pCode, pCreateInfo->codeSize / sizeof(uint32_t), spirv);
  }
}

void VulkanCreationInfo::ShaderModule::Reflection::Init(VulkanResourceManager *resourceMan,
                                                        ResourceId id, const SPVModule &spv,
                                                        const std::string &entry,
                                                        VkShaderStageFlagBits stage)
{
  if(entryPoint.empty())
  {
    entryPoint = entry;
    stageIndex = StageIndex(stage);

    spv.MakeReflection(GraphicsAPI::Vulkan, ShaderStage(stageIndex), entryPoint, refl, mapping,
                       patchData);

    refl.resourceId = resourceMan->GetOriginalID(id);
    refl.entryPoint = entryPoint;

    if(!spv.spirv.empty())
    {
      refl.encoding = ShaderEncoding::SPIRV;
      refl.rawBytes.assign((byte *)spv.spirv.data(), spv.spirv.size() * sizeof(uint32_t));
    }
  }
}

void VulkanCreationInfo::DescSetPool::Init(VulkanResourceManager *resourceMan,
                                           VulkanCreationInfo &info,
                                           const VkDescriptorPoolCreateInfo *pCreateInfo)
{
  maxSets = pCreateInfo->maxSets;
  poolSizes.assign(pCreateInfo->pPoolSizes, pCreateInfo->pPoolSizes + pCreateInfo->poolSizeCount);
}

void VulkanCreationInfo::DescSetPool::CreateOverflow(VkDevice device,
                                                     VulkanResourceManager *resourceMan)
{
  VkDescriptorPoolCreateInfo poolInfo = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      NULL,
      0,
      maxSets,
      (uint32_t)poolSizes.size(),
      &poolSizes[0],
  };

  VkDescriptorPool pool;

  VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), &poolInfo, NULL, &pool);
  RDCASSERTEQUAL(ret, VK_SUCCESS);

  ResourceId poolid = resourceMan->WrapResource(Unwrap(device), pool);

  // register as a live-only resource, so it is cleaned up properly
  resourceMan->AddLiveResource(poolid, pool);

  overflow.push_back(pool);
}

void DescUpdateTemplate::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                              const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo)
{
  updates.insert(updates.begin(), pCreateInfo->pDescriptorUpdateEntries,
                 pCreateInfo->pDescriptorUpdateEntries + pCreateInfo->descriptorUpdateEntryCount);

  bindPoint = pCreateInfo->pipelineBindPoint;

  dataByteSize = 0;

  texelBufferViewCount = 0;
  bufferInfoCount = 0;
  imageInfoCount = 0;

  for(const VkDescriptorUpdateTemplateEntry &entry : updates)
  {
    uint32_t entrySize = 4;

    if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
       entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
      entrySize = sizeof(VkBufferView);

      texelBufferViewCount += entry.descriptorCount;
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    {
      entrySize = sizeof(VkDescriptorImageInfo);

      imageInfoCount += entry.descriptorCount;
    }
    else
    {
      entrySize = sizeof(VkDescriptorBufferInfo);

      bufferInfoCount += entry.descriptorCount;
    }

    dataByteSize =
        RDCMAX(dataByteSize, entry.offset + entry.stride * entry.descriptorCount + entrySize);
  }

  if(pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
  {
    if(IsCaptureMode(resourceMan->GetState()))
    {
      layout = *GetRecord(pCreateInfo->descriptorSetLayout)->descInfo->layout;
    }
    else
    {
      layout = info.m_DescSetLayout[GetResID(pCreateInfo->descriptorSetLayout)];
    }
  }
  else
  {
    if(IsCaptureMode(resourceMan->GetState()))
    {
      layout = GetRecord(pCreateInfo->pipelineLayout)->pipeLayoutInfo->layouts[pCreateInfo->set];
    }
    else
    {
      const std::vector<ResourceId> &descSetLayouts =
          info.m_PipelineLayout[GetResID(pCreateInfo->pipelineLayout)].descSetLayouts;

      layout = info.m_DescSetLayout[descSetLayouts[pCreateInfo->set]];
    }
  }
}

void DescUpdateTemplate::Apply(const void *pData, DescUpdateTemplateApplication &application)
{
  application.bufView.reserve(texelBufferViewCount);
  application.bufInfo.reserve(bufferInfoCount);
  application.imgInfo.reserve(imageInfoCount);

  for(const VkDescriptorUpdateTemplateEntry &entry : updates)
  {
    VkWriteDescriptorSet write = {};

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = VK_NULL_HANDLE;    // set externally for non-push descriptor template updates.
    write.dstBinding = entry.dstBinding;
    write.dstArrayElement = entry.dstArrayElement;
    write.descriptorType = entry.descriptorType;
    write.descriptorCount = entry.descriptorCount;

    const byte *src = (const byte *)pData + entry.offset;

    if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
       entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
      size_t idx = application.bufView.size();

      application.bufView.resize(idx + entry.descriptorCount);

      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.bufView[idx + d], src, sizeof(VkBufferView));
        src += entry.stride;
      }

      write.pTexelBufferView = &application.bufView[idx];
    }
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    {
      size_t idx = application.imgInfo.size();

      application.imgInfo.resize(idx + entry.descriptorCount);

      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.imgInfo[idx + d], src, sizeof(VkDescriptorImageInfo));
        src += entry.stride;
      }

      write.pImageInfo = &application.imgInfo[idx];
    }
    else
    {
      size_t idx = application.bufInfo.size();

      application.bufInfo.resize(idx + entry.descriptorCount);

      for(uint32_t d = 0; d < entry.descriptorCount; d++)
      {
        memcpy(&application.bufInfo[idx + d], src, sizeof(VkDescriptorBufferInfo));
        src += entry.stride;
      }

      write.pBufferInfo = &application.bufInfo[idx];
    }

    application.writes.push_back(write);
  }
}
