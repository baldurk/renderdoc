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

void DescSetLayout::Init(const VkDescriptorSetLayoutCreateInfo* pCreateInfo)
{
	bindings.resize(pCreateInfo->count);
	for(uint32_t i=0; i < pCreateInfo->count; i++)
	{
		bindings[i].arraySize = pCreateInfo->pBinding[i].arraySize;
		bindings[i].descriptorType = pCreateInfo->pBinding[i].descriptorType;
		bindings[i].stageFlags = pCreateInfo->pBinding[i].stageFlags;

		if(pCreateInfo->pBinding[i].pImmutableSamplers)
		{
			bindings[i].immutableSampler = new ResourceId[bindings[i].arraySize];

			for(uint32_t s=0; s < bindings[i].arraySize; s++)
				bindings[i].immutableSampler[s] = VKMGR()->GetNonDispWrapper(pCreateInfo->pBinding[i].pImmutableSamplers[s])->id;
		}
	}
}

void DescSetLayout::CreateBindingsArray(vector<VkDescriptorInfo*> &descBindings)
{
	descBindings.resize(bindings.size());
	for(size_t i=0; i < bindings.size(); i++)
	{
		descBindings[i] = new VkDescriptorInfo[bindings[i].arraySize];
		memset(descBindings[i], 0, sizeof(VkDescriptorInfo)*bindings[i].arraySize);
	}
}

void VulkanCreationInfo::Pipeline::Init(const VkGraphicsPipelineCreateInfo* pCreateInfo)
{
		flags = pCreateInfo->flags;

		// need to figure out which states are valid to be NULL
		
		// VkPipelineShaderStageCreateInfo
		RDCEraseEl(shaders);
		for(uint32_t i=0; i < pCreateInfo->stageCount; i++)
			shaders[ pCreateInfo->pStages[i].stage ] = VKMGR()->GetNonDispWrapper(pCreateInfo->pStages[i].shader)->id;

		if(pCreateInfo->pVertexInputState)
		{
			vertexBindings.resize(pCreateInfo->pVertexInputState->bindingCount);
			for(uint32_t i=0; i < pCreateInfo->pVertexInputState->bindingCount; i++)
			{
				vertexBindings[i].vbufferBinding = pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].binding;
				vertexBindings[i].bytestride = pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].strideInBytes;
				vertexBindings[i].perInstance = pCreateInfo->pVertexInputState->pVertexBindingDescriptions[i].stepRate == VK_VERTEX_INPUT_STEP_RATE_INSTANCE;
			}

			vertexAttrs.resize(pCreateInfo->pVertexInputState->attributeCount);
			for(uint32_t i=0; i < pCreateInfo->pVertexInputState->attributeCount; i++)
			{
				vertexAttrs[i].binding = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].binding;
				vertexAttrs[i].location = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].location;
				vertexAttrs[i].format = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].format;
				vertexAttrs[i].byteoffset = pCreateInfo->pVertexInputState->pVertexAttributeDescriptions[i].offsetInBytes;
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
		depthClipEnable = pCreateInfo->pRasterState->depthClipEnable ? true : false;
		rasterizerDiscardEnable = pCreateInfo->pRasterState->rasterizerDiscardEnable ? true : false;
		fillMode = pCreateInfo->pRasterState->fillMode;
		cullMode = pCreateInfo->pRasterState->cullMode;
		frontFace = pCreateInfo->pRasterState->frontFace;
		depthBiasEnable = pCreateInfo->pRasterState->depthBiasEnable ? true : false;
		depthBias = pCreateInfo->pRasterState->depthBias;
		depthBiasClamp = pCreateInfo->pRasterState->depthBiasClamp;
		slopeScaledDepthBias = pCreateInfo->pRasterState->slopeScaledDepthBias;
		lineWidth = pCreateInfo->pRasterState->lineWidth;

		// VkPipelineMultisampleStateCreateInfo
		rasterSamples = pCreateInfo->pMultisampleState->rasterSamples;
		sampleShadingEnable = pCreateInfo->pMultisampleState->sampleShadingEnable ? true : false;
		minSampleShading = pCreateInfo->pMultisampleState->minSampleShading;
		sampleMask = pCreateInfo->pMultisampleState->pSampleMask ? *pCreateInfo->pMultisampleState->pSampleMask : ~0U;

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
		alphaToCoverageEnable = pCreateInfo->pColorBlendState->alphaToCoverageEnable ? true : false;
		alphaToOneEnable = pCreateInfo->pColorBlendState->alphaToOneEnable ? true : false;
		logicOpEnable = pCreateInfo->pColorBlendState->logicOpEnable ? true : false;
		logicOp = pCreateInfo->pColorBlendState->logicOp;
		memcpy(blendConst, pCreateInfo->pColorBlendState->blendConst, sizeof(blendConst));

		attachments.resize(pCreateInfo->pColorBlendState->attachmentCount);

		for(uint32_t i=0; i < pCreateInfo->pColorBlendState->attachmentCount; i++)
		{
			attachments[i].blendEnable = pCreateInfo->pColorBlendState->pAttachments[i].blendEnable ? true : false;

			attachments[i].blend.Source = pCreateInfo->pColorBlendState->pAttachments[i].srcBlendColor;
			attachments[i].blend.Destination = pCreateInfo->pColorBlendState->pAttachments[i].destBlendColor;
			attachments[i].blend.Operation = pCreateInfo->pColorBlendState->pAttachments[i].blendOpColor;

			attachments[i].alphaBlend.Source = pCreateInfo->pColorBlendState->pAttachments[i].srcBlendAlpha;
			attachments[i].alphaBlend.Destination = pCreateInfo->pColorBlendState->pAttachments[i].destBlendAlpha;
			attachments[i].alphaBlend.Operation = pCreateInfo->pColorBlendState->pAttachments[i].blendOpAlpha;

			attachments[i].channelWriteMask = pCreateInfo->pColorBlendState->pAttachments[i].channelWriteMask;
		}

		RDCEraseEl(dynamicStates);
		if(pCreateInfo->pDynamicState)
		{
			for(uint32_t i=0; i < pCreateInfo->pDynamicState->dynamicStateCount; i++)
				dynamicStates[ pCreateInfo->pDynamicState->pDynamicStates[i] ] = true;
		}
}

void VulkanCreationInfo::Pipeline::Init(const VkComputePipelineCreateInfo* pCreateInfo)
{
		flags = pCreateInfo->flags;

		// need to figure out which states are valid to be NULL
		
		// VkPipelineShaderStageCreateInfo
		RDCEraseEl(shaders);
		shaders[0] = VKMGR()->GetNonDispWrapper(pCreateInfo->stage.shader)->id;

		topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		primitiveRestartEnable = false;

		patchControlPoints = 0;

		viewportCount = 0;

		// VkPipelineRasterStateCreateInfo
		depthClipEnable = false;
		rasterizerDiscardEnable = false;
		fillMode = VK_FILL_MODE_SOLID;
		cullMode = VK_CULL_MODE_NONE;
		frontFace = VK_FRONT_FACE_CW;

		// VkPipelineMultisampleStateCreateInfo
		rasterSamples = 1;
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
		logicOp = VK_LOGIC_OP_NOOP;
}

void VulkanCreationInfo::RenderPass::Init(const VkRenderPassCreateInfo* pCreateInfo)
{
	// VKTODOMED figure out how subpasses work
	RDCASSERT(pCreateInfo->subpassCount > 0);
	const VkSubpassDescription &subp = pCreateInfo->pSubpasses[0];

	inputAttachments.resize(subp.inputCount);
	for(uint32_t i=0; i < subp.inputCount; i++)
		inputAttachments[i] = subp.pInputAttachments[i].attachment;

	colorAttachments.resize(subp.colorCount);
	for(uint32_t i=0; i < subp.colorCount; i++)
		colorAttachments[i] = subp.pColorAttachments[i].attachment;
	
	depthstencilAttachment = (subp.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED
		? (int32_t)subp.depthStencilAttachment.attachment : -1);
}

void VulkanCreationInfo::Framebuffer::Init(const VkFramebufferCreateInfo* pCreateInfo)
{
	width = pCreateInfo->width;
	height = pCreateInfo->height;
	layers = pCreateInfo->layers;

	attachments.resize(pCreateInfo->attachmentCount);
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
		attachments[i].view = VKMGR()->GetNonDispWrapper(pCreateInfo->pAttachments[i])->id;
}

void VulkanCreationInfo::Buffer::Init(const VkBufferCreateInfo* pCreateInfo)
{
	size = pCreateInfo->size;
}

void VulkanCreationInfo::BufferView::Init(const VkBufferViewCreateInfo* pCreateInfo)
{
	buffer = VKMGR()->GetNonDispWrapper(pCreateInfo->buffer)->id;
	offset = pCreateInfo->offset;
	size = pCreateInfo->range;
}

void VulkanCreationInfo::Image::Init(const VkImageCreateInfo* pCreateInfo)
{
	view = VK_NULL_HANDLE;

	type = pCreateInfo->imageType;
	format = pCreateInfo->format;
	extent = pCreateInfo->extent;
	arraySize = pCreateInfo->arraySize;
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

	if(pCreateInfo->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
		cube = true;
}

void VulkanCreationInfo::ImageView::Init(const VkImageViewCreateInfo* pCreateInfo)
{
	image = VKMGR()->GetNonDispWrapper(pCreateInfo->image)->id;
}

void VulkanCreationInfo::ShaderModule::Init(const VkShaderModuleCreateInfo* pCreateInfo)
{
	RDCASSERT(pCreateInfo->codeSize % sizeof(uint32_t) == 0);
	ParseSPIRV((uint32_t *)pCreateInfo->pCode, pCreateInfo->codeSize/sizeof(uint32_t), spirv);

	spirv.MakeReflection(&reflTemplate, &mapping);
}

void VulkanCreationInfo::Shader::Init(const VkShaderCreateInfo* pCreateInfo, VulkanCreationInfo::ShaderModule &moduleinfo)
{
	module = VKMGR()->GetNonDispWrapper(pCreateInfo->module)->id;
	mapping = moduleinfo.mapping;
	refl = moduleinfo.reflTemplate;
	refl.DebugInfo.entryFunc = pCreateInfo->pName;
	// VKTODOLOW set this properly
	refl.DebugInfo.entryFile = 0;
}