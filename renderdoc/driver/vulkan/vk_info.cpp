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

#include "vk_info.h"
#include "3rdparty/glslang/SPIRV/spirv.hpp"

void DescSetLayout::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                         const VkDescriptorSetLayoutCreateInfo *pCreateInfo)
{
  dynamicCount = 0;

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
      {
        // during writing, the create info contains the *wrapped* objects.
        // on replay, we have the wrapper map so we can look it up
        if(resourceMan->IsWriting())
          bindings[b].immutableSampler[s] = GetResID(pCreateInfo->pBindings[i].pImmutableSamplers[s]);
        else
          bindings[b].immutableSampler[s] =
              resourceMan->GetNonDispWrapper(pCreateInfo->pBindings[i].pImmutableSamplers[s])->id;
      }
    }
  }
}

void DescSetLayout::CreateBindingsArray(vector<DescriptorSetSlot *> &descBindings)
{
  descBindings.resize(bindings.size());
  for(size_t i = 0; i < bindings.size(); i++)
  {
    descBindings[i] = new DescriptorSetSlot[bindings[i].descriptorCount];
    memset(descBindings[i], 0, sizeof(DescriptorSetSlot) * bindings[i].descriptorCount);
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

  layout = resourceMan->GetNonDispWrapper(pCreateInfo->layout)->id;
  renderpass = resourceMan->GetNonDispWrapper(pCreateInfo->renderPass)->id;
  subpass = pCreateInfo->subpass;

  // need to figure out which states are valid to be NULL

  // VkPipelineShaderStageCreateInfo
  for(uint32_t i = 0; i < pCreateInfo->stageCount; i++)
  {
    ResourceId id = resourceMan->GetNonDispWrapper(pCreateInfo->pStages[i].module)->id;

    // convert shader bit to shader index
    int stageIndex = StageIndex(pCreateInfo->pStages[i].stage);

    Shader &shad = shaders[stageIndex];

    shad.module = id;
    shad.entryPoint = pCreateInfo->pStages[i].pName;

    ShaderModule::Reflection &reflData = info.m_ShaderModule[id].m_Reflections[shad.entryPoint];

    if(reflData.entryPoint.empty())
    {
      SPVModule &spv = info.m_ShaderModule[id].spirv;
      spv.MakeReflection(ShaderStage(reflData.stage), reflData.entryPoint, reflData.refl,
                         reflData.mapping, reflData.patchData);

      reflData.entryPoint = shad.entryPoint;
      reflData.stage = stageIndex;
      reflData.refl.ID = resourceMan->GetOriginalID(id);
      reflData.refl.EntryPoint = shad.entryPoint;

      if(!spv.spirv.empty())
      {
        rdctype::array<byte> &bytes = reflData.refl.RawBytes;
        const vector<uint32_t> &spirv = spv.spirv;
        create_array_init(bytes, spirv.size() * sizeof(uint32_t), (byte *)&spirv[0]);
      }
    }

    if(pCreateInfo->pStages[i].pSpecializationInfo)
    {
      shad.specdata.resize(pCreateInfo->pStages[i].pSpecializationInfo->dataSize);
      memcpy(&shad.specdata[0], pCreateInfo->pStages[i].pSpecializationInfo->pData,
             shad.specdata.size());

      const VkSpecializationMapEntry *maps = pCreateInfo->pStages[i].pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->pStages[i].pSpecializationInfo->mapEntryCount; s++)
      {
        Shader::SpecInfo spec;
        spec.specID = maps[s].constantID;
        spec.data = &shad.specdata[maps[s].offset];
        spec.size = maps[s].size;
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

  RDCEraseEl(dynamicStates);
  if(pCreateInfo->pDynamicState)
  {
    for(uint32_t i = 0; i < pCreateInfo->pDynamicState->dynamicStateCount; i++)
      dynamicStates[pCreateInfo->pDynamicState->pDynamicStates[i]] = true;
  }
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                        const VkComputePipelineCreateInfo *pCreateInfo)
{
  flags = pCreateInfo->flags;

  layout = resourceMan->GetNonDispWrapper(pCreateInfo->layout)->id;

  // need to figure out which states are valid to be NULL

  // VkPipelineShaderStageCreateInfo
  {
    ResourceId id = resourceMan->GetNonDispWrapper(pCreateInfo->stage.module)->id;
    Shader &shad = shaders[5];    // 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)

    shad.module = id;
    shad.entryPoint = pCreateInfo->stage.pName;

    ShaderModule::Reflection &reflData = info.m_ShaderModule[id].m_Reflections[shad.entryPoint];

    if(reflData.entryPoint.empty())
    {
      reflData.entryPoint = shad.entryPoint;
      info.m_ShaderModule[id].spirv.MakeReflection(ShaderStage::Compute, reflData.entryPoint,
                                                   reflData.refl, reflData.mapping,
                                                   reflData.patchData);
      reflData.refl.ID = resourceMan->GetOriginalID(id);
      reflData.refl.EntryPoint = shad.entryPoint;
    }

    if(pCreateInfo->stage.pSpecializationInfo)
    {
      shad.specdata.resize(pCreateInfo->stage.pSpecializationInfo->dataSize);
      memcpy(&shad.specdata[0], pCreateInfo->stage.pSpecializationInfo->pData, shad.specdata.size());

      const VkSpecializationMapEntry *maps = pCreateInfo->stage.pSpecializationInfo->pMapEntries;
      for(uint32_t s = 0; s < pCreateInfo->stage.pSpecializationInfo->mapEntryCount; s++)
      {
        Shader::SpecInfo spec;
        spec.specID = maps[s].constantID;
        spec.data = &shad.specdata[maps[s].offset];
        spec.size = maps[s].size;
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

  viewportCount = 0;

  // VkPipelineRasterStateCreateInfo
  depthClampEnable = false;
  rasterizerDiscardEnable = false;
  polygonMode = VK_POLYGON_MODE_FILL;
  cullMode = VK_CULL_MODE_NONE;
  frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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
  descSetLayouts.resize(pCreateInfo->setLayoutCount);
  for(uint32_t i = 0; i < pCreateInfo->setLayoutCount; i++)
    descSetLayouts[i] = resourceMan->GetNonDispWrapper(pCreateInfo->pSetLayouts[i])->id;

  pushRanges.reserve(pCreateInfo->pushConstantRangeCount);
  for(uint32_t i = 0; i < pCreateInfo->pushConstantRangeCount; i++)
    pushRanges.push_back(pCreateInfo->pPushConstantRanges[i]);
}

void VulkanCreationInfo::RenderPass::Init(VulkanResourceManager *resourceMan,
                                          VulkanCreationInfo &info,
                                          const VkRenderPassCreateInfo *pCreateInfo)
{
  attachments.reserve(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    attachments.push_back(pCreateInfo->pAttachments[i]);

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
    attachments[i].view = resourceMan->GetNonDispWrapper(pCreateInfo->pAttachments[i])->id;
    attachments[i].format = info.m_ImageView[attachments[i].view].format;
  }
}

void VulkanCreationInfo::Memory::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                      const VkMemoryAllocateInfo *pAllocInfo)
{
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
  buffer = resourceMan->GetNonDispWrapper(pCreateInfo->buffer)->id;
  offset = pCreateInfo->offset;
  size = pCreateInfo->range;
}

void VulkanCreationInfo::Image::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                     const VkImageCreateInfo *pCreateInfo)
{
  view = VK_NULL_HANDLE;
  stencilView = VK_NULL_HANDLE;

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

void VulkanCreationInfo::ImageView::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info,
                                         const VkImageViewCreateInfo *pCreateInfo)
{
  image = resourceMan->GetNonDispWrapper(pCreateInfo->image)->id;
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
