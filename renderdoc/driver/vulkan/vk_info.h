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

#pragma once

#include "vk_common.h"
#include "vk_manager.h"

struct VulkanCreationInfo
{
	struct Pipeline
	{
		void Init(const VkGraphicsPipelineCreateInfo* pCreateInfo);
		
		// VkGraphicsPipelineCreateInfo
		VkPipelineCreateFlags flags;

		// VkPipelineShaderStageCreateInfo
		ResourceId shaders[6];

		// VkPipelineVertexInputStateCreateInfo
		struct Binding
		{
			uint32_t vbufferBinding; // VKTODO I believe this is the meaning
			uint32_t bytestride;
			bool perInstance;
		};
		vector<Binding> vertexBindings;
		
		struct Attribute
		{
			uint32_t location;
			uint32_t binding;
			VkFormat format;
			uint32_t byteoffset;
		};
		vector<Attribute> vertexAttrs;

		// VkPipelineInputAssemblyStateCreateInfo
		VkPrimitiveTopology topology;
		bool primitiveRestartEnable;

		// VkPipelineTessellationStateCreateInfo
		uint32_t patchControlPoints;

		// VkPipelineViewportStateCreateInfo
		uint32_t viewportCount;

		// VkPipelineRasterStateCreateInfo
		bool depthClipEnable;
		bool rasterizerDiscardEnable;
		VkFillMode fillMode;
		VkCullMode cullMode;
		VkFrontFace frontFace;

		// VkPipelineMultisampleStateCreateInfo
		uint32_t rasterSamples;
		bool sampleShadingEnable;
		float minSampleShading;
		VkSampleMask sampleMask;

		// VkPipelineDepthStencilStateCreateInfo
		bool depthTestEnable;
		bool depthWriteEnable;
		VkCompareOp depthCompareOp;
		bool depthBoundsEnable;
		bool stencilTestEnable;
		VkStencilOpState front;
		VkStencilOpState back;

		// VkPipelineColorBlendStateCreateInfo
		bool alphaToCoverageEnable;
		bool logicOpEnable;
		VkLogicOp logicOp;

		struct Attachment
		{
			bool blendEnable;

			struct BlendOp
			{
				VkBlend Source;
				VkBlend Destination;
				VkBlendOp Operation;
			} blend, alphaBlend;

			uint8_t channelWriteMask;
		};
		vector<Attachment> attachments;
	};
	map<ResourceId, Pipeline> m_Pipeline;

	struct ViewportScissor
	{
		void Init(const VkDynamicViewportStateCreateInfo* pCreateInfo);
		
		vector<VkViewport> viewports;
		vector<VkRect2D> scissors;
	};
	map<ResourceId, ViewportScissor> m_VPScissor;

	struct Raster
	{
		void Init(const VkDynamicRasterStateCreateInfo* pCreateInfo);
		
		float depthBias;
		float depthBiasClamp;
		float slopeScaledDepthBias;
		float lineWidth;
	};
	map<ResourceId, Raster> m_Raster;

	struct Blend
	{
		void Init(const VkDynamicColorBlendStateCreateInfo* pCreateInfo);
		
		float blendConst[4];
	};
	map<ResourceId, Blend> m_Blend;

	struct DepthStencil
	{
		void Init(const VkDynamicDepthStencilStateCreateInfo* pCreateInfo);

		float minDepthBounds;
		float maxDepthBounds;
		uint32_t stencilReadMask;
		uint32_t stencilWriteMask;
		uint32_t stencilFrontRef;
		uint32_t stencilBackRef;
	};
	map<ResourceId, DepthStencil> m_DepthStencil;
	
	struct RenderPass
	{
		void Init(const VkRenderPassCreateInfo* pCreateInfo);

		vector<uint32_t> inputAttachments;
		vector<uint32_t> colorAttachments;
		int32_t depthstencilAttachment;
	};
	map<ResourceId, RenderPass> m_RenderPass;

	struct Framebuffer
	{
		void Init(const VkFramebufferCreateInfo* pCreateInfo);

		struct Attachment
		{
			ResourceId view;
		};
		vector<Attachment> attachments;
		
		uint32_t width, height, layers;
	};
	map<ResourceId, Framebuffer> m_Framebuffer;
	
	struct BufferView
	{
		void Init(const VkBufferViewCreateInfo* pCreateInfo);

		ResourceId buffer;
		uint64_t offset;
		uint64_t size;
	};
	map<ResourceId, BufferView> m_BufferView;

	struct ImageView
	{
		void Init(const VkImageViewCreateInfo* pCreateInfo);

		ResourceId image;
	};
	map<ResourceId, ImageView> m_ImageView;

	struct AttachmentView
	{
		void Init(const VkAttachmentViewCreateInfo* pCreateInfo);

		ResourceId image;
	};
	map<ResourceId, AttachmentView> m_AttachmentView;

	struct DescSetLayout
	{
		void Init(const VkDescriptorSetLayoutCreateInfo* pCreateInfo);

		void CreateBindingsArray(vector<VkDescriptorInfo*> &descBindings);

		struct Binding
		{
			Binding() : immutableSampler(NULL) {}
			~Binding() { SAFE_DELETE_ARRAY(immutableSampler); }

			VkDescriptorType descriptorType;
			uint32_t arraySize;
			VkShaderStageFlags stageFlags;
			ResourceId *immutableSampler;
		};
		vector<Binding> bindings;
	};
	map<ResourceId, DescSetLayout> m_DescSetLayout;
};