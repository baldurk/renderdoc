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

#include "vk_info.h"

void DescSetLayout::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkDescriptorSetLayoutCreateInfo* pCreateInfo)
{
	dynamicCount = 0;

	bindings.resize(pCreateInfo->bindingCount);
	for(uint32_t i=0; i < pCreateInfo->bindingCount; i++)
	{
		bindings[i].descriptorCount = pCreateInfo->pBinding[i].descriptorCount;
		bindings[i].descriptorType = pCreateInfo->pBinding[i].descriptorType;
		bindings[i].stageFlags = pCreateInfo->pBinding[i].stageFlags;

		if(bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
			 bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
			dynamicCount++;

		if(pCreateInfo->pBinding[i].pImmutableSamplers)
		{
			bindings[i].immutableSampler = new ResourceId[bindings[i].descriptorCount];

			for(uint32_t s=0; s < bindings[i].descriptorCount; s++)
				bindings[i].immutableSampler[s] = resourceMan->GetNonDispWrapper(pCreateInfo->pBinding[i].pImmutableSamplers[s])->id;
		}
	}
}

void DescSetLayout::CreateBindingsArray(vector<DescriptorSetSlot*> &descBindings)
{
	descBindings.resize(bindings.size());
	for(size_t i=0; i < bindings.size(); i++)
	{
		descBindings[i] = new DescriptorSetSlot[bindings[i].descriptorCount];
		memset(descBindings[i], 0, sizeof(DescriptorSetSlot)*bindings[i].descriptorCount);
	}
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkGraphicsPipelineCreateInfo* pCreateInfo)
{
		flags = pCreateInfo->flags;

		layout = resourceMan->GetNonDispWrapper(pCreateInfo->layout)->id;
		renderpass = resourceMan->GetNonDispWrapper(pCreateInfo->renderPass)->id;
		subpass = pCreateInfo->subpass;

		// need to figure out which states are valid to be NULL
		
		// VkPipelineShaderStageCreateInfo
		for(uint32_t i=0; i < pCreateInfo->stageCount; i++)
		{
			ResourceId id = resourceMan->GetNonDispWrapper(pCreateInfo->pStages[i].module)->id;

			// convert shader bit to shader index
			int s=0;
			for(; s < 6; s++)
				if(pCreateInfo->pStages[i].stage == (1<<s))
					break;

			Shader &shad = shaders[s];
			
			shad.module = id;
			shad.name = pCreateInfo->pStages[i].pName;
			shad.refl = new ShaderReflection(info.m_ShaderModule[id].refl);
			shad.refl->DebugInfo.entryFunc = pCreateInfo->pStages[i].pName;
			// VKTODOLOW set this properly
			shad.refl->DebugInfo.entryFile = 0;
		}

		if(pCreateInfo->pVertexInputState)
		{
			vertexBindings.resize(pCreateInfo->pVertexInputState->vertexBindingDescriptionCount);
			for(uint32_t i=0; i < pCreateInfo->pVertexInputState->vertexBindingDescriptionCount; i++)
			{
				vertexBindings[i].vbufferBinding = pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].binding;
				vertexBindings[i].bytestride = pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].stride;
				vertexBindings[i].perInstance = pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE;
			}

			vertexAttrs.resize(pCreateInfo->pVertexInputState->vertexAttributeDescriptionCount);
			for(uint32_t i=0; i < pCreateInfo->pVertexInputState->vertexAttributeDescriptionCount; i++)
			{
				vertexAttrs[i].binding = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].binding;
				vertexAttrs[i].location = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].location;
				vertexAttrs[i].format = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].format;
				vertexAttrs[i].byteoffset = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].offset;
			}
		}

		topology = pCreateInfo->pInputAssemblyState->topology;
		primitiveRestartEnable = pCreateInfo->pInputAssemblyState->primitiveRestartEnable ? true : false;

		if(pCreateInfo->pTessellationState)
			patchControlPoints = pCreateInfo->pTessellationState->patchControlPoints;
		else
			patchControlPoints = 0;

		viewportCount = pCreateInfo->pViewportState->viewportCount;

		viewports.resize(viewportCount);
		scissors.resize(viewportCount);

		for(size_t i=0; i < viewports.size(); i++)
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
		rasterizationSamples = pCreateInfo->pMultisampleState->rasterizationSamples;
		sampleShadingEnable = pCreateInfo->pMultisampleState->sampleShadingEnable ? true : false;
		minSampleShading = pCreateInfo->pMultisampleState->minSampleShading;
		sampleMask = pCreateInfo->pMultisampleState->pSampleMask ? *pCreateInfo->pMultisampleState->pSampleMask : ~0U;
		alphaToCoverageEnable = pCreateInfo->pMultisampleState->alphaToCoverageEnable ? true : false;
		alphaToOneEnable = pCreateInfo->pMultisampleState->alphaToOneEnable ? true : false;

		// VkPipelineDepthStencilStateCreateInfo
		depthTestEnable = pCreateInfo->pDepthStencilState->depthTestEnable ? true : false;
		depthWriteEnable = pCreateInfo->pDepthStencilState->depthWriteEnable ? true : false;
		depthCompareOp = pCreateInfo->pDepthStencilState->depthCompareOp;
		depthBoundsEnable = pCreateInfo->pDepthStencilState->depthBoundsTestEnable ? true : false;
		stencilTestEnable = pCreateInfo->pDepthStencilState->stencilTestEnable ? true : false;
		front = pCreateInfo->pDepthStencilState->front;
		back = pCreateInfo->pDepthStencilState->back;
		minDepthBounds = pCreateInfo->pDepthStencilState->minDepthBounds;
		maxDepthBounds = pCreateInfo->pDepthStencilState->maxDepthBounds;

		// VkPipelineColorBlendStateCreateInfo
		logicOpEnable = pCreateInfo->pColorBlendState->logicOpEnable ? true : false;
		logicOp = pCreateInfo->pColorBlendState->logicOp;
		memcpy(blendConst, pCreateInfo->pColorBlendState->blendConstants, sizeof(blendConst));

		attachments.resize(pCreateInfo->pColorBlendState->attachmentCount);

		for(uint32_t i=0; i < pCreateInfo->pColorBlendState->attachmentCount; i++)
		{
			attachments[i].blendEnable = pCreateInfo->pColorBlendState->pAttachments[i].blendEnable ? true : false;

			attachments[i].blend.Source = pCreateInfo->pColorBlendState->pAttachments[i].srcColorBlendFactor;
			attachments[i].blend.Destination = pCreateInfo->pColorBlendState->pAttachments[i].dstColorBlendFactor;
			attachments[i].blend.Operation = pCreateInfo->pColorBlendState->pAttachments[i].colorBlendOp;

			attachments[i].alphaBlend.Source = pCreateInfo->pColorBlendState->pAttachments[i].srcAlphaBlendFactor;
			attachments[i].alphaBlend.Destination = pCreateInfo->pColorBlendState->pAttachments[i].dstAlphaBlendFactor;
			attachments[i].alphaBlend.Operation = pCreateInfo->pColorBlendState->pAttachments[i].alphaBlendOp;

			attachments[i].channelWriteMask = pCreateInfo->pColorBlendState->pAttachments[i].colorWriteMask;
		}

		RDCEraseEl(dynamicStates);
		if(pCreateInfo->pDynamicState)
		{
			for(uint32_t i=0; i < pCreateInfo->pDynamicState->dynamicStateCount; i++)
				dynamicStates[ pCreateInfo->pDynamicState->pDynamicStates[i] ] = true;
		}
}

void VulkanCreationInfo::Pipeline::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkComputePipelineCreateInfo* pCreateInfo)
{
		flags = pCreateInfo->flags;

		layout = resourceMan->GetNonDispWrapper(pCreateInfo->layout)->id;

		// need to figure out which states are valid to be NULL
		
		// VkPipelineShaderStageCreateInfo
		{
			ResourceId id = resourceMan->GetNonDispWrapper(pCreateInfo->stage.module)->id;
			Shader &shad = shaders[0];
			
			shad.module = id;
			shad.name = pCreateInfo->stage.pName;
			shad.refl = new ShaderReflection(info.m_ShaderModule[id].refl);
			shad.refl->DebugInfo.entryFunc = pCreateInfo->stage.pName;
			// VKTODOLOW set this properly
			shad.refl->DebugInfo.entryFile = 0;
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

void VulkanCreationInfo::PipelineLayout::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkPipelineLayoutCreateInfo* pCreateInfo)
{
	descSetLayouts.resize(pCreateInfo->setLayoutCount);
	for(uint32_t i=0; i < pCreateInfo->setLayoutCount; i++)
		descSetLayouts[i] = resourceMan->GetNonDispWrapper(pCreateInfo->pSetLayouts[i])->id;

	pushRanges.reserve(pCreateInfo->pushConstantRangeCount);
	for(uint32_t i=0; i < pCreateInfo->pushConstantRangeCount; i++)
		pushRanges.push_back(pCreateInfo->pPushConstantRanges[i]);
}

void VulkanCreationInfo::RenderPass::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkRenderPassCreateInfo* pCreateInfo)
{
	attachments.reserve(pCreateInfo->attachmentCount);
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
	{
		Attachment a;
		a.loadOp = pCreateInfo->pAttachments[i].loadOp;
		a.storeOp = pCreateInfo->pAttachments[i].storeOp;
		a.stencilLoadOp = pCreateInfo->pAttachments[i].stencilLoadOp;
		a.stencilStoreOp = pCreateInfo->pAttachments[i].stencilStoreOp;
		attachments.push_back(a);
	}

	subpasses.resize(pCreateInfo->subpassCount);
	for(uint32_t i=0; i < pCreateInfo->subpassCount; i++)
	{
		const VkSubpassDescription &subp = pCreateInfo->pSubpasses[i];
		Subpass &s = subpasses[i];

		s.inputAttachments.resize(subp.inputAttachmentCount);
		for(uint32_t i=0; i < subp.inputAttachmentCount; i++)
			s.inputAttachments[i] = subp.pInputAttachments[i].attachment;

		s.colorAttachments.resize(subp.colorAttachmentCount);
		for(uint32_t i=0; i < subp.colorAttachmentCount; i++)
			s.colorAttachments[i] = subp.pColorAttachments[i].attachment;

		s.depthstencilAttachment = (subp.pDepthStencilAttachment != NULL && subp.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED
			? (int32_t)subp.pDepthStencilAttachment->attachment : -1);
	}
}

void VulkanCreationInfo::Framebuffer::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkFramebufferCreateInfo* pCreateInfo)
{
	width = pCreateInfo->width;
	height = pCreateInfo->height;
	layers = pCreateInfo->layers;

	attachments.resize(pCreateInfo->attachmentCount);
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
	{
		attachments[i].view = resourceMan->GetNonDispWrapper(pCreateInfo->pAttachments[i])->id;
		attachments[i].format = info.m_ImageView[attachments[i].view].format;
	}
}

void VulkanCreationInfo::Memory::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkMemoryAllocateInfo* pAllocInfo)
{
	size = pAllocInfo->allocationSize;
}

void VulkanCreationInfo::Buffer::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkBufferCreateInfo* pCreateInfo)
{
	usage = pCreateInfo->usage;
	size = pCreateInfo->size;
}

void VulkanCreationInfo::BufferView::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkBufferViewCreateInfo* pCreateInfo)
{
	buffer = resourceMan->GetNonDispWrapper(pCreateInfo->buffer)->id;
	offset = pCreateInfo->offset;
	size = pCreateInfo->range;
}

void VulkanCreationInfo::Image::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkImageCreateInfo* pCreateInfo)
{
	view = VK_NULL_HANDLE;

	type = pCreateInfo->imageType;
	format = pCreateInfo->format;
	extent = pCreateInfo->extent;
	arrayLayers = pCreateInfo->arrayLayers;
	mipLevels = pCreateInfo->mipLevels;
	samples = pCreateInfo->samples;

	creationFlags = 0;

	if(pCreateInfo->usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		creationFlags |= eTextureCreate_SRV;
	if(pCreateInfo->usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT))
		creationFlags |= eTextureCreate_RTV;
	if(pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		creationFlags |= eTextureCreate_DSV;
	if(pCreateInfo->usage & VK_IMAGE_USAGE_STORAGE_BIT)
		creationFlags |= eTextureCreate_UAV;

	cube = (pCreateInfo->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) ? true : false;
}

void VulkanCreationInfo::Sampler::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkSamplerCreateInfo* pCreateInfo)
{
	magFilter = pCreateInfo->magFilter;
	minFilter = pCreateInfo->minFilter;
	mipmapMode = pCreateInfo->mipmapMode;
	address[0] = pCreateInfo->addressModeU;
	address[1] = pCreateInfo->addressModeV;
	address[2] = pCreateInfo->addressModeW;
	mipLodBias = pCreateInfo->mipLodBias;
	maxAnisotropy = pCreateInfo->maxAnisotropy;
	compareEnable = pCreateInfo->compareEnable != 0;
	compareOp = pCreateInfo->compareOp;
	minLod = pCreateInfo->minLod;
	maxLod = pCreateInfo->maxLod;
	borderColor = pCreateInfo->borderColor;
	unnormalizedCoordinates = pCreateInfo->unnormalizedCoordinates != 0;
}
	
void VulkanCreationInfo::ImageView::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkImageViewCreateInfo* pCreateInfo)
{
	image = resourceMan->GetNonDispWrapper(pCreateInfo->image)->id;
	format = pCreateInfo->format;
	range = pCreateInfo->subresourceRange;
}

void VulkanCreationInfo::ShaderModule::Init(VulkanResourceManager *resourceMan, VulkanCreationInfo &info, const VkShaderModuleCreateInfo* pCreateInfo)
{
	const uint32_t SPIRVMagic = 0x07230203;
	if(pCreateInfo->codeSize < 4 || memcmp(pCreateInfo->pCode, &SPIRVMagic, sizeof(SPIRVMagic)))
	{
		RDCWARN("Shader not provided with SPIR-V");
	}
	else
	{
		RDCASSERT(pCreateInfo->codeSize % sizeof(uint32_t) == 0);
		ParseSPIRV((uint32_t *)pCreateInfo->pCode, pCreateInfo->codeSize/sizeof(uint32_t), spirv);
	}

	spirv.MakeReflection(&refl, &mapping);
}
