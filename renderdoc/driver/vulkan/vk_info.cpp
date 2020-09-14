/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
    case VkDynamicLineStippleEXT: return VK_DYNAMIC_STATE_LINE_STIPPLE_EXT;
    case VkDynamicCullModeEXT: return VK_DYNAMIC_STATE_CULL_MODE_EXT;
    case VkDynamicFrontFaceEXT: return VK_DYNAMIC_STATE_FRONT_FACE_EXT;
    case VkDynamicPrimitiveTopologyEXT: return VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT;
    case VkDynamicViewportCountEXT: return VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT;
    case VkDynamicScissorCountEXT: return VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT;
    case VkDynamicVertexInputBindingStrideEXT:
      return VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT;
    case VkDynamicDepthTestEnableEXT: return VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT;
    case VkDynamicDepthWriteEnableEXT: return VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT;
    case VkDynamicDepthCompareOpEXT: return VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT;
    case VkDynamicDepthBoundsTestEnableEXT: return VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT;
    case VkDynamicStencilTestEnableEXT: return VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT;
    case VkDynamicStencilOpEXT: return VK_DYNAMIC_STATE_STENCIL_OP_EXT;
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
    case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT: return VkDynamicLineStippleEXT;
    case VK_DYNAMIC_STATE_CULL_MODE_EXT: return VkDynamicCullModeEXT;
    case VK_DYNAMIC_STATE_FRONT_FACE_EXT: return VkDynamicFrontFaceEXT;
    case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT: return VkDynamicPrimitiveTopologyEXT;
    case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT: return VkDynamicViewportCountEXT;
    case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT: return VkDynamicScissorCountEXT;
    case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT:
      return VkDynamicVertexInputBindingStrideEXT;
    case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT: return VkDynamicDepthTestEnableEXT;
    case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT: return VkDynamicDepthWriteEnableEXT;
    case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT: return VkDynamicDepthCompareOpEXT;
    case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT: return VkDynamicDepthBoundsTestEnableEXT;
    case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT: return VkDynamicStencilTestEnableEXT;
    case VK_DYNAMIC_STATE_STENCIL_OP_EXT: return VkDynamicStencilOpEXT;
    case VK_DYNAMIC_STATE_MAX_ENUM: break;
  }

  RDCERR("Unexpected vulkan state %u", state);

  return VkDynamicCount;
}

void DescSetLayout::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                         const VkDescriptorSetLayoutCreateInfo *pCreateInfo)
{
  dynamicCount = 0;
  inlineCount = 0;
  inlineByteSize = 0;

  flags = pCreateInfo->flags;

  VkDescriptorSetLayoutBindingFlagsCreateInfo *bindingFlags =
      (VkDescriptorSetLayoutBindingFlagsCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO);

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

    if(bindings[b].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
      inlineCount++;
      inlineByteSize = AlignUp4(inlineByteSize + bindings[b].descriptorCount);
    }

    if((bindings[b].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        bindings[b].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
       pCreateInfo->pBindings[i].pImmutableSamplers)
    {
      bindings[b].immutableSampler = new ResourceId[bindings[b].descriptorCount];

      for(uint32_t s = 0; s < bindings[b].descriptorCount; s++)
        bindings[b].immutableSampler[s] = GetResID(pCreateInfo->pBindings[i].pImmutableSamplers[s]);
    }

    if(bindingFlags &&
       (bindingFlags->pBindingFlags[i] & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT))
      bindings[b].variableSize = 1;
    else
      bindings[b].variableSize = 0;
  }

  // assign offsets in sorted bindings order, as the bindings we were provided by the application
  // don't have to appear in bindings order
  uint32_t elemOffset = 0;

  for(size_t b = 0; b < bindings.size(); b++)
  {
    bindings[b].elemOffset = elemOffset;

    // don't count the descriptors in the variable size array. We'll add on the allocated size after
    // this
    if(bindings[b].variableSize)
      break;

    if(bindings[b].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
      elemOffset++;
    }
    else
    {
      elemOffset += bindings[b].descriptorCount;
    }
  }

  totalElems = elemOffset;
}

void DescSetLayout::CreateBindingsArray(BindingStorage &bindingStorage, uint32_t variableAllocSize) const
{
  bindingStorage.variableDescriptorCount = variableAllocSize;

  if(!bindings.empty())
  {
    bindingStorage.elems.resize(totalElems + variableAllocSize);
    bindingStorage.binds.resize(bindings.size());

    if(inlineByteSize == 0)
    {
      for(size_t i = 0; i < bindings.size(); i++)
        bindingStorage.binds[i] = bindingStorage.elems.data() + bindings[i].elemOffset;

      bindingStorage.inlineBytes.clear();
    }
    else
    {
      uint32_t inlineOffset = 0;
      for(size_t i = 0; i < bindings.size(); i++)
      {
        bindingStorage.binds[i] = bindingStorage.elems.data() + bindings[i].elemOffset;

        if(bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          bindingStorage.binds[i]->inlineOffset = inlineOffset;
          inlineOffset = AlignUp4(inlineOffset + bindings[i].descriptorCount);
        }
      }

      bindingStorage.inlineBytes.resize(inlineByteSize);
    }
  }
}

void DescSetLayout::UpdateBindingsArray(const DescSetLayout &prevLayout,
                                        BindingStorage &bindingStorage) const
{
  if(bindings.empty())
  {
    bindingStorage.clear();
  }
  else
  {
    rdcarray<DescriptorSetSlot> newElems;
    newElems.resize(totalElems);

    // resize to the new size, discarding any excess we don't need anymore
    bindingStorage.binds.resize(bindings.size());

    if(inlineByteSize == 0)
    {
      for(size_t i = 0; i < bindings.size(); i++)
      {
        DescriptorSetSlot *newSlots = newElems.data() + bindings[i].elemOffset;

        // copy over any previous bindings that overlapped
        if(i < prevLayout.bindings.size())
          memcpy(newSlots, bindingStorage.binds[i],
                 sizeof(DescriptorSetSlot) *
                     RDCMIN(prevLayout.bindings[i].descriptorCount, bindings[i].descriptorCount));

        bindingStorage.binds[i] = newSlots;
      }
    }
    else
    {
      uint32_t inlineOffset = 0;
      for(size_t i = 0; i < bindings.size(); i++)
      {
        DescriptorSetSlot *newSlots = newElems.data() + bindings[i].elemOffset;

        if(bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          bindingStorage.binds[i]->inlineOffset = inlineOffset;
          inlineOffset = AlignUp4(inlineOffset + bindings[i].descriptorCount);
        }
        else
        {
          // copy over any previous bindings that overlapped
          if(i < prevLayout.bindings.size())
            memcpy(newSlots, bindingStorage.binds[i],
                   sizeof(DescriptorSetSlot) *
                       RDCMIN(prevLayout.bindings[i].descriptorCount, bindings[i].descriptorCount));
        }

        bindingStorage.binds[i] = newSlots;
      }

      bindingStorage.inlineBytes.resize(inlineByteSize);
    }

    bindingStorage.elems.swap(newElems);
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

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan,
                                        VulkanCreationInfo &info, ResourceId id,
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

    // if the viewports and counts are dynamic this supersets the viewport only being dynamic. For
    // ease of code elsewhere, turn off the older one if both are specified so that we don't call
    // vkCmdSetViewports when the count is also dynamic.
    if(dynamicStates[VkDynamicViewportCountEXT])
      dynamicStates[VkDynamicViewport] = false;
    if(dynamicStates[VkDynamicScissorCountEXT])
      dynamicStates[VkDynamicScissor] = false;
  }

  // VkPipelineShaderStageCreateInfo
  for(uint32_t i = 0; i < pCreateInfo->stageCount; i++)
  {
    ResourceId shadid = GetResID(pCreateInfo->pStages[i].module);

    // convert shader bit to shader index
    int stageIndex = StageIndex(pCreateInfo->pStages[i].stage);

    Shader &shad = shaders[stageIndex];

    shad.module = shadid;
    shad.entryPoint = pCreateInfo->pStages[i].pName;

    ShaderModuleReflectionKey key(shad.entryPoint, ResourceId());

    if(pCreateInfo->pStages[i].pSpecializationInfo)
    {
      key = ShaderModuleReflectionKey(shad.entryPoint, id);

      const byte *data = (const byte *)pCreateInfo->pStages[i].pSpecializationInfo->pData;

      const VkSpecializationMapEntry *maps = pCreateInfo->pStages[i].pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->pStages[i].pSpecializationInfo->mapEntryCount; s++)
      {
        SpecConstant spec;
        spec.specID = maps[s].constantID;
        memcpy(&spec.value, data + maps[s].offset, maps[s].size);
        spec.dataSize = maps[s].size;
        shad.specialization.push_back(spec);
      }
    }

    ShaderModuleReflection &reflData = info.m_ShaderModule[shadid].m_Reflections[key];

    reflData.Init(resourceMan, shadid, info.m_ShaderModule[shadid].spirv, shad.entryPoint,
                  pCreateInfo->pStages[i].stage, shad.specialization);

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

  // VkPipelineRasterizationDepthClipStateCreateInfoEXT

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

  // VkPipelineRasterizationLineStateCreateInfoEXT
  lineRasterMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
  stippleEnabled = false;
  stippleFactor = stipplePattern = 0;

  const VkPipelineRasterizationLineStateCreateInfoEXT *lineRasterState =
      (const VkPipelineRasterizationLineStateCreateInfoEXT *)FindNextStruct(
          pCreateInfo->pRasterizationState,
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT);
  if(lineRasterState)
  {
    lineRasterMode = lineRasterState->lineRasterizationMode;
    stippleEnabled = lineRasterState->stippledLineEnable ? true : false;
    if(lineRasterState->stippledLineEnable)
    {
      stippleFactor = lineRasterState->lineStippleFactor;
      stipplePattern = lineRasterState->lineStipplePattern;
    }
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
        sampleLocations.locations.assign(sampleLoc->sampleLocationsInfo.pSampleLocations,
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
                                        ResourceId id, const VkComputePipelineCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  layout = GetResID(pCreateInfo->layout);

  // need to figure out which states are valid to be NULL

  // VkPipelineShaderStageCreateInfo
  {
    ResourceId shadid = GetResID(pCreateInfo->stage.module);
    Shader &shad = shaders[5];    // 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)

    shad.module = shadid;
    shad.entryPoint = pCreateInfo->stage.pName;

    ShaderModuleReflectionKey key(shad.entryPoint, ResourceId());

    if(pCreateInfo->stage.pSpecializationInfo)
    {
      key = ShaderModuleReflectionKey(shad.entryPoint, id);

      const byte *data = (const byte *)pCreateInfo->stage.pSpecializationInfo->pData;

      const VkSpecializationMapEntry *maps = pCreateInfo->stage.pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->stage.pSpecializationInfo->mapEntryCount; s++)
      {
        SpecConstant spec;
        spec.specID = maps[s].constantID;
        memcpy(&spec.value, data + maps[s].offset, maps[s].size);
        spec.dataSize = maps[s].size;
        shad.specialization.push_back(spec);
      }
    }

    ShaderModuleReflection &reflData = info.m_ShaderModule[shadid].m_Reflections[key];

    reflData.Init(resourceMan, shadid, info.m_ShaderModule[shadid].spirv, shad.entryPoint,
                  pCreateInfo->stage.stage, shad.specialization);

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
    dst.used = false;
    dst.flags = pCreateInfo->pAttachments[i].flags;
    dst.format = pCreateInfo->pAttachments[i].format;
    dst.samples = pCreateInfo->pAttachments[i].samples;
    dst.loadOp = pCreateInfo->pAttachments[i].loadOp;
    dst.storeOp = pCreateInfo->pAttachments[i].storeOp;
    dst.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
    dst.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
    dst.stencilInitialLayout = dst.initialLayout = pCreateInfo->pAttachments[i].initialLayout;
    dst.stencilFinalLayout = dst.finalLayout = pCreateInfo->pAttachments[i].finalLayout;
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
    dst.inputStencilLayouts.resize(src.inputAttachmentCount);
    for(uint32_t i = 0; i < src.inputAttachmentCount; i++)
    {
      dst.inputAttachments[i] = src.pInputAttachments[i].attachment;
      dst.inputStencilLayouts[i] = dst.inputLayouts[i] = src.pInputAttachments[i].layout;
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

      if(dst.resolveAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.resolveAttachments[i]].used = true;
      if(dst.colorAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.colorAttachments[i]].used = true;
    }

    dst.depthstencilAttachment =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? (int32_t)src.pDepthStencilAttachment->attachment
             : -1);
    dst.stencilLayout = dst.depthLayout =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? src.pDepthStencilAttachment->layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    if(dst.depthstencilAttachment >= 0)
      attachments[dst.depthstencilAttachment].used = true;

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
                                          const VkRenderPassCreateInfo2 *pCreateInfo)
{
  attachments.resize(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
  {
    Attachment &dst = attachments[i];
    dst.used = false;
    dst.flags = pCreateInfo->pAttachments[i].flags;
    dst.format = pCreateInfo->pAttachments[i].format;
    dst.samples = pCreateInfo->pAttachments[i].samples;
    dst.loadOp = pCreateInfo->pAttachments[i].loadOp;
    dst.storeOp = pCreateInfo->pAttachments[i].storeOp;
    dst.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
    dst.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
    dst.stencilInitialLayout = dst.initialLayout = pCreateInfo->pAttachments[i].initialLayout;
    dst.stencilFinalLayout = dst.finalLayout = pCreateInfo->pAttachments[i].finalLayout;

    // VK_KHR_separate_depth_stencil_layouts
    const VkAttachmentDescriptionStencilLayoutKHR *separateStencil =
        (const VkAttachmentDescriptionStencilLayoutKHR *)FindNextStruct(
            &pCreateInfo->pAttachments[i],
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT_KHR);

    if(separateStencil)
    {
      dst.stencilInitialLayout = separateStencil->stencilInitialLayout;
      dst.stencilFinalLayout = separateStencil->stencilFinalLayout;
    }
  }

  // VK_EXT_fragment_density_map
  const VkRenderPassFragmentDensityMapCreateInfoEXT *fragmentDensity =
      (const VkRenderPassFragmentDensityMapCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT);

  subpasses.resize(pCreateInfo->subpassCount);
  for(uint32_t subp = 0; subp < pCreateInfo->subpassCount; subp++)
  {
    const VkSubpassDescription2 &src = pCreateInfo->pSubpasses[subp];
    Subpass &dst = subpasses[subp];

    dst.inputAttachments.resize(src.inputAttachmentCount);
    dst.inputLayouts.resize(src.inputAttachmentCount);
    dst.inputStencilLayouts.resize(src.inputAttachmentCount);
    for(uint32_t i = 0; i < src.inputAttachmentCount; i++)
    {
      dst.inputAttachments[i] = src.pInputAttachments[i].attachment;
      dst.inputStencilLayouts[i] = dst.inputLayouts[i] = src.pInputAttachments[i].layout;
      const VkAttachmentReferenceStencilLayoutKHR *stencilLayout =
          (const VkAttachmentReferenceStencilLayoutKHR *)FindNextStruct(
              &src.pInputAttachments[i], VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT_KHR);
      if(stencilLayout != NULL)
        dst.inputStencilLayouts[i] = stencilLayout->stencilLayout;
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

      if(dst.resolveAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.resolveAttachments[i]].used = true;
      if(dst.colorAttachments[i] != VK_ATTACHMENT_UNUSED)
        attachments[dst.colorAttachments[i]].used = true;
    }

    dst.depthstencilAttachment =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? (int32_t)src.pDepthStencilAttachment->attachment
             : -1);
    dst.stencilLayout = dst.depthLayout =
        (src.pDepthStencilAttachment != NULL &&
                 src.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
             ? src.pDepthStencilAttachment->layout
             : VK_IMAGE_LAYOUT_UNDEFINED);

    if(dst.depthstencilAttachment >= 0)
      attachments[dst.depthstencilAttachment].used = true;

    // VK_KHR_separate_depth_stencil_layouts
    const VkAttachmentReferenceStencilLayoutKHR *separateStencil =
        (const VkAttachmentReferenceStencilLayoutKHR *)FindNextStruct(
            src.pDepthStencilAttachment, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT_KHR);

    if(separateStencil)
      dst.stencilLayout = separateStencil->stencilLayout;

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

  imageless = false;

  attachments.resize(pCreateInfo->attachmentCount);
  if(pCreateInfo->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT)
  {
    imageless = true;

    // VK_KHR_imageless_framebuffer
    const VkFramebufferAttachmentsCreateInfo *attachmentsInfo =
        (const VkFramebufferAttachmentsCreateInfo *)FindNextStruct(
            pCreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO);

    RDCASSERTEQUAL(pCreateInfo->attachmentCount, attachmentsInfo->attachmentImageInfoCount);

    for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    {
      attachments[i].createdView = ResourceId();
      // there must be at least one format in the list so we can safely look at [0].
      // also all entries must be compatible, so if [0] doesn't have stencil then none of them do,
      // and vice-versa.
      attachments[i].hasStencil =
          IsStencilFormat(attachmentsInfo->pAttachmentImageInfos[i].pViewFormats[0]);
    }
  }
  else
  {
    for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    {
      attachments[i].createdView = GetResID(pCreateInfo->pAttachments[i]);
      attachments[i].hasStencil =
          IsStencilFormat(info.m_ImageView[attachments[i].createdView].format);
    }
  }
}

void VulkanCreationInfo::Memory::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkMemoryAllocateInfo *pAllocInfo)
{
  memoryTypeIndex = pAllocInfo->memoryTypeIndex;
  size = pAllocInfo->allocationSize;
}

void VulkanCreationInfo::Memory::SimplifyBindings()
{
  // after initialisation we're likely to end up with a lot of gaps of 'none' in between tiled or
  // linear resources. Regions of memory with no bindings are not visible in any meaningful way
  // (memory can only be read with an image or buffer bound to it) so we perform a pass collapsing
  // any 'None' intervals into the previous to be able to simplify the set of intervals. This means
  // we might promote some regions to tiled, but that's fine since as above their contents are
  // essentially meaningless.

  // if the first entry is None and we have a second entry, then set the first to whatever the
  // second is
  if(bindings.size() > 1 && bindings.begin()->value() == VulkanCreationInfo::Memory::None)
  {
    auto it = bindings.begin();
    it++;
    bindings.begin()->setValue(it->value());
  }

  for(auto it = bindings.begin(); it != bindings.end(); it++)
  {
    // if we're not at the begining and the current range is None, copy whatever was in the previous
    // range
    if(it != bindings.begin() && it->value() == VulkanCreationInfo::Memory::None)
    {
      auto previt = it;
      previt--;

      it->setValue(previt->value());
    }

    // merge left when possible
    it->mergeLeft();
  }
}

void VulkanCreationInfo::Buffer::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkBufferCreateInfo *pCreateInfo)
{
  usage = pCreateInfo->usage;
  size = pCreateInfo->size;
  gpuAddress = 0;

  external = false;

  if(FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO))
  {
    external = true;
  }
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

  linear = pCreateInfo->tiling == VK_IMAGE_TILING_LINEAR;

  external = false;

  if(FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV) ||
     FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO) ||
     FindNextStruct(pCreateInfo, VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID))
  {
    external = true;
  }

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
  maxAnisotropy = pCreateInfo->anisotropyEnable ? pCreateInfo->maxAnisotropy : 0.0f;
  compareEnable = pCreateInfo->compareEnable != 0;
  compareOp = pCreateInfo->compareOp;
  minLod = pCreateInfo->minLod;
  maxLod = pCreateInfo->maxLod;
  borderColor = pCreateInfo->borderColor;
  unnormalizedCoordinates = pCreateInfo->unnormalizedCoordinates != 0;

  reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

  const VkSamplerReductionModeCreateInfo *reduction =
      (const VkSamplerReductionModeCreateInfo *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO);
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

  customBorder = false;
  RDCEraseEl(customBorderColor);
  customBorderFormat = VK_FORMAT_UNDEFINED;

  const VkSamplerCustomBorderColorCreateInfoEXT *border =
      (const VkSamplerCustomBorderColorCreateInfoEXT *)FindNextStruct(
          pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT);
  if(border)
  {
    customBorder = true;
    customBorderColor = border->customBorderColor;
    customBorderFormat = border->format;
  }
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
    case VK_SAMPLER_YCBCR_MODEL_CONVERSION_MAX_ENUM: break;
  }

  switch(pCreateInfo->ycbcrRange)
  {
    case VK_SAMPLER_YCBCR_RANGE_ITU_FULL: ycbcrRange = YcbcrRange::ITUFull; break;
    case VK_SAMPLER_YCBCR_RANGE_ITU_NARROW: ycbcrRange = YcbcrRange::ITUNarrow; break;
    case VK_SAMPLER_YCBCR_RANGE_MAX_ENUM: break;
  }

  switch(pCreateInfo->xChromaOffset)
  {
    case VK_CHROMA_LOCATION_COSITED_EVEN: xChromaOffset = ChromaSampleLocation::CositedEven; break;
    case VK_CHROMA_LOCATION_MIDPOINT: xChromaOffset = ChromaSampleLocation::Midpoint; break;
    case VK_CHROMA_LOCATION_MAX_ENUM: break;
  }

  switch(pCreateInfo->yChromaOffset)
  {
    case VK_CHROMA_LOCATION_COSITED_EVEN: yChromaOffset = ChromaSampleLocation::CositedEven; break;
    case VK_CHROMA_LOCATION_MIDPOINT: yChromaOffset = ChromaSampleLocation::Midpoint; break;
    case VK_CHROMA_LOCATION_MAX_ENUM: break;
  }

  componentMapping = pCreateInfo->components;
  chromaFilter = MakeFilterMode(pCreateInfo->chromaFilter);
  forceExplicitReconstruction = pCreateInfo->forceExplicitReconstruction != 0;
}

void VulkanCreationInfo::ImageView::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                         const VkImageViewCreateInfo *pCreateInfo)
{
  image = GetResID(pCreateInfo->image);
  format = pCreateInfo->format;
  range = pCreateInfo->subresourceRange;
  viewType = pCreateInfo->viewType;

  if(range.levelCount == VK_REMAINING_MIP_LEVELS)
    range.levelCount = info.m_Image[image].mipLevels - range.baseMipLevel;

  if(range.layerCount == VK_REMAINING_ARRAY_LAYERS)
    range.layerCount = info.m_Image[image].arrayLayers - range.baseArrayLayer;

  componentMapping = pCreateInfo->components;
}

void VulkanCreationInfo::ShaderModule::Init(VulkanResourceManager *resourceMan,
                                            VulkanCreationInfo &info,
                                            const VkShaderModuleCreateInfo *pCreateInfo)
{
  const uint32_t SPIRVMagic = 0x07230203;
  if(pCreateInfo->codeSize < 4 || memcmp(pCreateInfo->pCode, &SPIRVMagic, sizeof(SPIRVMagic)) != 0)
  {
    RDCWARN("Shader not provided with SPIR-V");
  }
  else
  {
    RDCASSERT(pCreateInfo->codeSize % sizeof(uint32_t) == 0);
    spirv.Parse(rdcarray<uint32_t>((uint32_t *)(pCreateInfo->pCode),
                                   pCreateInfo->codeSize / sizeof(uint32_t)));
  }
}

void VulkanCreationInfo::ShaderModuleReflection::Init(VulkanResourceManager *resourceMan,
                                                      ResourceId id, const rdcspv::Reflector &spv,
                                                      const rdcstr &entry,
                                                      VkShaderStageFlagBits stage,
                                                      const rdcarray<SpecConstant> &specInfo)
{
  if(entryPoint.empty())
  {
    entryPoint = entry;
    stageIndex = StageIndex(stage);

    spv.MakeReflection(GraphicsAPI::Vulkan, ShaderStage(stageIndex), entryPoint, specInfo, refl,
                       mapping, patchData);

    refl.resourceId = resourceMan->GetOriginalID(id);
  }
}

void VulkanCreationInfo::ShaderModuleReflection::PopulateDisassembly(const rdcspv::Reflector &spirv)
{
  if(disassembly.empty())
    disassembly = spirv.Disassemble(refl.entryPoint.c_str(), instructionLines);
}

void VulkanCreationInfo::QueryPool::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                         const VkQueryPoolCreateInfo *pCreateInfo)
{
  queryType = pCreateInfo->queryType;
  queryCount = pCreateInfo->queryCount;
  pipelineStatistics = pCreateInfo->pipelineStatistics;
}

void VulkanCreationInfo::DescSetPool::Init(VulkanResourceManager *resourceMan,
                                           VulkanCreationInfo &info,
                                           const VkDescriptorPoolCreateInfo *pCreateInfo)
{
  maxSets = pCreateInfo->maxSets;
  poolSizes.assign(pCreateInfo->pPoolSizes, pCreateInfo->poolSizeCount);
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
  updates.assign(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->descriptorUpdateEntryCount);

  bindPoint = pCreateInfo->pipelineBindPoint;

  unwrapByteSize = 0;

  texelBufferViewCount = 0;
  bufferInfoCount = 0;
  imageInfoCount = 0;

  for(const VkDescriptorUpdateTemplateEntry &entry : updates)
  {
    uint32_t entrySize = 4;

    size_t stride = entry.stride;

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
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
      // a bit of magic handling. The calculation is stride * descriptorCount bytes for the data,
      // plus the size of the 'base' structure. For inline uniform blocks there's no base structure
      // and the data is in bytes, so stride 1.
      stride = 1;

      entrySize = 0;

      inlineInfoCount++;
      inlineByteSize += entry.descriptorCount;
      inlineByteSize = AlignUp4(inlineByteSize);
    }
    else
    {
      entrySize = sizeof(VkDescriptorBufferInfo);

      bufferInfoCount += entry.descriptorCount;
    }

    unwrapByteSize =
        RDCMAX(unwrapByteSize, entry.offset + stride * entry.descriptorCount + entrySize);
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
      const rdcarray<ResourceId> &descSetLayouts =
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
  application.inlineData.resize(inlineByteSize);
  application.inlineUniform.reserve(inlineInfoCount);

  uint32_t inlineOffset = 0;
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
    else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
    {
      application.inlineUniform.push_back({});

      VkWriteDescriptorSetInlineUniformBlockEXT &inlineWrite = application.inlineUniform.back();
      inlineWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT;
      inlineWrite.pNext = NULL;
      inlineWrite.dataSize = entry.descriptorCount;

      void *dst = application.inlineData.data() + inlineOffset;
      memcpy(dst, src, inlineWrite.dataSize);
      inlineWrite.pData = dst;

      write.pNext = &inlineWrite;
      write.descriptorCount = entry.descriptorCount;
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

    if(write.descriptorCount != 0)
      application.writes.push_back(write);
  }
}
