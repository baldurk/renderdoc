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

		// VkPipelineRasterStateCreateInfo
		depthClipEnable = pCreateInfo->pRasterState->depthClipEnable ? true : false;
		rasterizerDiscardEnable = pCreateInfo->pRasterState->rasterizerDiscardEnable ? true : false;
		fillMode = pCreateInfo->pRasterState->fillMode;
		cullMode = pCreateInfo->pRasterState->cullMode;
		frontFace = pCreateInfo->pRasterState->frontFace;

		// VkPipelineMultisampleStateCreateInfo
		rasterSamples = pCreateInfo->pMultisampleState->rasterSamples;
		sampleShadingEnable = pCreateInfo->pMultisampleState->sampleShadingEnable ? true : false;
		minSampleShading = pCreateInfo->pMultisampleState->minSampleShading;
		sampleMask = pCreateInfo->pMultisampleState->sampleMask;

		// VkPipelineDepthStencilStateCreateInfo
		depthTestEnable = pCreateInfo->pDepthStencilState->depthTestEnable ? true : false;
		depthWriteEnable = pCreateInfo->pDepthStencilState->depthWriteEnable ? true : false;
		depthCompareOp = pCreateInfo->pDepthStencilState->depthCompareOp;
		depthBoundsEnable = pCreateInfo->pDepthStencilState->depthBoundsEnable ? true : false;
		stencilTestEnable = pCreateInfo->pDepthStencilState->stencilTestEnable ? true : false;
		front = pCreateInfo->pDepthStencilState->front;
		back = pCreateInfo->pDepthStencilState->back;

		// VkPipelineColorBlendStateCreateInfo
		alphaToCoverageEnable = pCreateInfo->pColorBlendState->alphaToCoverageEnable ? true : false;
		logicOpEnable = pCreateInfo->pColorBlendState->logicOpEnable ? true : false;
		logicOp = pCreateInfo->pColorBlendState->logicOp;

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
}

void VulkanCreationInfo::ViewportScissor::Init(const VkDynamicViewportStateCreateInfo* pCreateInfo)
{
	viewports.resize(pCreateInfo->viewportAndScissorCount);
	scissors.resize(pCreateInfo->viewportAndScissorCount);

	for(uint32_t i=0; i < pCreateInfo->viewportAndScissorCount; i++)
	{
		viewports[i] = pCreateInfo->pViewports[i];
		scissors[i] = pCreateInfo->pScissors[i];
	}
}

void VulkanCreationInfo::Raster::Init(const VkDynamicRasterStateCreateInfo* pCreateInfo)
{
	depthBias = pCreateInfo->depthBias;
	depthBiasClamp = pCreateInfo->depthBiasClamp;
	slopeScaledDepthBias = pCreateInfo->slopeScaledDepthBias;
	lineWidth = pCreateInfo->lineWidth;
}

void VulkanCreationInfo::Blend::Init(const VkDynamicColorBlendStateCreateInfo* pCreateInfo)
{
	RDCCOMPILE_ASSERT(sizeof(blendConst) == sizeof(pCreateInfo->blendConst), "blend constant size mismatch!");
	memcpy(blendConst, pCreateInfo->blendConst, sizeof(blendConst));
}

void VulkanCreationInfo::DepthStencil::Init(const VkDynamicDepthStencilStateCreateInfo* pCreateInfo)
{
	minDepthBounds = pCreateInfo->minDepthBounds;
	maxDepthBounds = pCreateInfo->maxDepthBounds;
	stencilReadMask = pCreateInfo->stencilReadMask;
	stencilWriteMask = pCreateInfo->stencilWriteMask;
	stencilFrontRef = pCreateInfo->stencilFrontRef;
	stencilBackRef = pCreateInfo->stencilBackRef;
}

void VulkanCreationInfo::RenderPass::Init(const VkRenderPassCreateInfo* pCreateInfo)
{
	// VKTODOMED figure out how subpasses work
	RDCASSERT(pCreateInfo->subpassCount > 0);
	const VkSubpassDescription &subp = pCreateInfo->pSubpasses[0];

	inputAttachments.resize(subp.inputCount);
	for(uint32_t i=0; i < subp.inputCount; i++)
		inputAttachments[i] = subp.inputAttachments[i].attachment;

	colorAttachments.resize(subp.colorCount);
	for(uint32_t i=0; i < subp.colorCount; i++)
		colorAttachments[i] = subp.colorAttachments[i].attachment;
	
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
		attachments[i].view = VKMGR()->GetNonDispWrapper(pCreateInfo->pAttachments[i].view)->id;
}

void VulkanCreationInfo::AttachmentView::Init(const VkAttachmentViewCreateInfo* pCreateInfo)
{
	image = VKMGR()->GetNonDispWrapper(pCreateInfo->image)->id;
}

void VulkanCreationInfo::BufferView::Init(const VkBufferViewCreateInfo* pCreateInfo)
{
	buffer = VKMGR()->GetNonDispWrapper(pCreateInfo->buffer)->id;
	offset = pCreateInfo->offset;
	size = pCreateInfo->range;
}

void VulkanCreationInfo::ImageView::Init(const VkImageViewCreateInfo* pCreateInfo)
{
	image = VKMGR()->GetNonDispWrapper(pCreateInfo->image)->id;
}

void VulkanCreationInfo::DescSetLayout::Init(const VkDescriptorSetLayoutCreateInfo* pCreateInfo)
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

void VulkanCreationInfo::DescSetLayout::CreateBindingsArray(vector<VkDescriptorInfo*> &descBindings)
{
	descBindings.resize(bindings.size());
	for(size_t i=0; i < bindings.size(); i++)
	{
		descBindings[i] = new VkDescriptorInfo[bindings[i].arraySize];
		memset(descBindings[i], 0, sizeof(VkDescriptorInfo)*bindings[i].arraySize);
	}
}
