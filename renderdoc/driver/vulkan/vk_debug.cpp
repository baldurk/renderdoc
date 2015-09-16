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

#include "vk_debug.h"
#include "vk_core.h"

// VKTODOMED should share this between shader and C++ - need #include support in glslang
struct displayuniforms
{
	Vec2f Position;
	float Scale;
	float HDRMul;

	Vec4f Channels;

	float RangeMinimum;
	float InverseRangeSize;
	float MipLevel;
	int   FlipY;

	Vec3f TextureResolutionPS;
	int   OutputDisplayFormat;

	Vec2f OutputRes;
	int   RawOutput;
	float Slice;

	int   SampleIdx;
	int   NumSamples;
	Vec2f Padding;
};

void VulkanDebugManager::UBO::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size)
{
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	VkBufferCreateInfo bufInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
		size, VK_BUFFER_USAGE_GENERAL, 0,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	};

	VkResult vkr = vt->CreateBuffer(dev, &bufInfo, &buf);
	RDCASSERT(vkr == VK_SUCCESS);

	VkMemoryRequirements mrq;
	vkr = vt->GetBufferMemoryRequirements(dev, buf, &mrq);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOMED maybe don't require host visible, and do map & copy?
	VkMemoryAllocInfo allocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
		size, driver->GetUploadMemoryIndex(mrq.memoryTypeBits),
	};

	vkr = vt->AllocMemory(dev, &allocInfo, &mem);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->BindBufferMemory(dev, buf, mem, 0);
	RDCASSERT(vkr == VK_SUCCESS);

	VkBufferViewCreateInfo bufviewInfo = {
		VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL,
		buf, VK_BUFFER_VIEW_TYPE_RAW,
		VK_FORMAT_UNDEFINED, 0, size,
	};

	vkr = vt->CreateBufferView(dev, &bufviewInfo, &view);
	RDCASSERT(vkr == VK_SUCCESS);
}

void VulkanDebugManager::UBO::Destroy(const VkLayerDispatchTable *vt, VkDevice dev)
{
	VkResult vkr = VK_SUCCESS;
	if(view != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyBufferView(dev, view);
		RDCASSERT(vkr == VK_SUCCESS);
		view = VK_NULL_HANDLE;
	}
	
	if(buf != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyBuffer(dev, buf);
		RDCASSERT(vkr == VK_SUCCESS);
		buf = VK_NULL_HANDLE;
	}

	if(mem != VK_NULL_HANDLE)
	{
		vkr = vt->FreeMemory(dev, mem);
		RDCASSERT(vkr == VK_SUCCESS);
		mem = VK_NULL_HANDLE;
	}
}

void *VulkanDebugManager::UBO::Map(const VkLayerDispatchTable *vt, VkDevice dev, VkDeviceSize offset, VkDeviceSize size)
{
	void *ptr = NULL;
	VkResult vkr = vt->MapMemory(dev, mem, offset, size, 0, (void **)&ptr);
	RDCASSERT(vkr == VK_SUCCESS);
	return ptr;
}

void VulkanDebugManager::UBO::Unmap(const VkLayerDispatchTable *vt, VkDevice dev)
{
	vt->UnmapMemory(dev, mem);
}

VulkanDebugManager::VulkanDebugManager(WrappedVulkan *driver, VkDevice dev, VkImageView fakeBBView)
{
	m_PipelineCache = VK_NULL_HANDLE;
	m_DescriptorPool = VK_NULL_HANDLE;
	m_DynamicCBStateWhite = VK_NULL_HANDLE;
	m_DynamicRSState = VK_NULL_HANDLE;
	m_DynamicDSStateDisabled = VK_NULL_HANDLE;
	m_LinearSampler = VK_NULL_HANDLE;
	m_PointSampler = VK_NULL_HANDLE;

	m_FakeBBImView = fakeBBView;

	m_CheckerboardDescSetLayout = VK_NULL_HANDLE;
	m_CheckerboardPipeLayout = VK_NULL_HANDLE;
	m_CheckerboardDescSet = VK_NULL_HANDLE;
	m_CheckerboardPipeline = VK_NULL_HANDLE;
	RDCEraseEl(m_CheckerboardUBO);

	m_TexDisplayDescSetLayout = VK_NULL_HANDLE;
	m_TexDisplayPipeLayout = VK_NULL_HANDLE;
	m_TexDisplayDescSet = VK_NULL_HANDLE;
	m_TexDisplayPipeline = VK_NULL_HANDLE;
	m_TexDisplayBlendPipeline = VK_NULL_HANDLE;
	RDCEraseEl(m_TexDisplayUBO);

	m_Device = dev;
	
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	VkResult vkr = VK_SUCCESS;

	VkSamplerCreateInfo sampInfo = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL,
		VK_TEX_FILTER_LINEAR, VK_TEX_FILTER_LINEAR,
		VK_TEX_MIPMAP_MODE_LINEAR, 
		VK_TEX_ADDRESS_CLAMP, VK_TEX_ADDRESS_CLAMP, VK_TEX_ADDRESS_CLAMP,
		0.0f, // lod bias
		1.0f, // max aniso
		false, VK_COMPARE_OP_NEVER,
		0.0f, 0.0f, // min/max lod
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	};

	vkr = vt->CreateSampler(dev, &sampInfo, &m_LinearSampler);
	RDCASSERT(vkr == VK_SUCCESS);

	sampInfo.minFilter = VK_TEX_FILTER_NEAREST;
	sampInfo.magFilter = VK_TEX_FILTER_NEAREST;
	sampInfo.mipMode = VK_TEX_MIPMAP_MODE_NEAREST;

	vkr = vt->CreateSampler(dev, &sampInfo, &m_PointSampler);
	RDCASSERT(vkr == VK_SUCCESS);

	VkPipelineCacheCreateInfo cacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, NULL, 0, NULL, 0 };

	vkr = vt->CreatePipelineCache(dev, &cacheInfo, &m_PipelineCache);
	RDCASSERT(vkr == VK_SUCCESS);

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(dev, &descsetLayoutInfo, &m_CheckerboardDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(dev, &descsetLayoutInfo, &m_TexDisplayDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	VkPipelineLayoutCreateInfo pipeLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL,
		1, &m_TexDisplayDescSetLayout,
		0, NULL, // push constant ranges
	};
	
	vkr = vt->CreatePipelineLayout(dev, &pipeLayoutInfo, &m_TexDisplayPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	pipeLayoutInfo.pSetLayouts = &m_CheckerboardDescSetLayout;
	
	vkr = vt->CreatePipelineLayout(dev, &pipeLayoutInfo, &m_CheckerboardPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	VkDescriptorTypeCount descPoolTypes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024, },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024, },
	};
	
	VkDescriptorPoolCreateInfo descpoolInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL,
    ARRAY_COUNT(descPoolTypes), &descPoolTypes[0],
	};
	
	vkr = vt->CreateDescriptorPool(dev, VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 2, &descpoolInfo, &m_DescriptorPool);
	RDCASSERT(vkr == VK_SUCCESS);
	
	uint32_t count;
	vkr = vt->AllocDescriptorSets(dev, m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_CheckerboardDescSetLayout, &m_CheckerboardDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);
	
	vkr = vt->AllocDescriptorSets(dev, m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_TexDisplayDescSetLayout, &m_TexDisplayDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);

	m_CheckerboardUBO.Create(driver, dev, 128);
	m_TexDisplayUBO.Create(driver, dev, 128);

	RDCCOMPILE_ASSERT(sizeof(displayuniforms) < 128, "tex display size");

	VkDescriptorInfo desc[3];
	RDCEraseEl(desc);
	
	desc[0].bufferView = m_CheckerboardUBO.view;
	desc[1].bufferView = m_TexDisplayUBO.view;
	desc[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc[2].imageView = m_FakeBBImView;
	desc[2].sampler = m_LinearSampler;

	VkWriteDescriptorSet writeSet[] = {
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_CheckerboardDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[0]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_TexDisplayDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[1]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_TexDisplayDescSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc[2]
		},
	};

	if(m_FakeBBImView != VK_NULL_HANDLE)
	{
		vkr = vt->UpdateDescriptorSets(dev, ARRAY_COUNT(writeSet), writeSet, 0, NULL);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	VkDynamicRasterStateCreateInfo rsInfo = {
		VK_STRUCTURE_TYPE_DYNAMIC_RASTER_STATE_CREATE_INFO, NULL,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	vkr = vt->CreateDynamicRasterState(dev, &rsInfo, &m_DynamicRSState);
	RDCASSERT(vkr == VK_SUCCESS);
	
	VkDynamicColorBlendStateCreateInfo cbInfo = {
		VK_STRUCTURE_TYPE_DYNAMIC_COLOR_BLEND_STATE_CREATE_INFO, NULL,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
	};

	vkr = vt->CreateDynamicColorBlendState(dev, &cbInfo, &m_DynamicCBStateWhite);
	RDCASSERT(vkr == VK_SUCCESS);
	
	VkDynamicDepthStencilStateCreateInfo dsInfo = {
		VK_STRUCTURE_TYPE_DYNAMIC_DEPTH_STENCIL_STATE_CREATE_INFO, NULL,
		0.0f, 1.0f, 0xff, 0xff, 0, 0,
	};

	vkr = vt->CreateDynamicDepthStencilState(dev, &dsInfo, &m_DynamicDSStateDisabled);
	RDCASSERT(vkr == VK_SUCCESS);
	
	string shaderSources[] = {
		GetEmbeddedResource(blitvs_spv),
		GetEmbeddedResource(checkerboardfs_spv),
		GetEmbeddedResource(texdisplayfs_spv),
	};
	
	enum shaderIdx
	{
		BLITVS,
		CHECKERBOARDFS,
		TEXDISPLAYFS,
	};

	VkShaderModule module[ARRAY_COUNT(shaderSources)];
	VkShader shader[ARRAY_COUNT(shaderSources)];
	
	for(size_t i=0; i < ARRAY_COUNT(module); i++)
	{
		VkShaderModuleCreateInfo modinfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,
			shaderSources[i].size(), (void *)&shaderSources[i][0], 0,
		};

		vkr = vt->CreateShaderModule(dev, &modinfo, &module[i]);
		RDCASSERT(vkr == VK_SUCCESS);

		VkShaderCreateInfo shadinfo = {
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO, NULL,
			module[i], "main", 0,
		};

		vkr = vt->CreateShader(dev, &shadinfo, &shader[i]);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	VkPipelineShaderStageCreateInfo stages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_VERTEX, shader[0], NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_FRAGMENT, shader[1], NULL },
	};

	VkPipelineInputAssemblyStateCreateInfo ia = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, false,
	};

	VkPipelineViewportStateCreateInfo vp = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL,
		1,
	};

	VkPipelineRasterStateCreateInfo rs = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO, NULL,
		true, false, VK_FILL_MODE_SOLID, VK_CULL_MODE_NONE, VK_FRONT_FACE_CW,
	};

	VkPipelineMultisampleStateCreateInfo msaa = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL,
		1, false, 0.0f, 1,
	};

	VkPipelineDepthStencilStateCreateInfo ds = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL,
		false, false, VK_COMPARE_OP_ALWAYS, false, false,
		{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS },
		{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS },
	};

	VkPipelineColorBlendAttachmentState attState = {
		false,
		VK_BLEND_ONE, VK_BLEND_ZERO, VK_BLEND_OP_ADD,
		VK_BLEND_ONE, VK_BLEND_ZERO, VK_BLEND_OP_ADD,
		0xf,
	};

	VkPipelineColorBlendStateCreateInfo cb = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL,
		false, false, VK_LOGIC_OP_NOOP,
		1, &attState,
	};

	VkGraphicsPipelineCreateInfo pipeInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL,
		2, stages,
		NULL, // vertex input
		&ia,
		NULL, // tess
		&vp,
		&rs,
		&msaa,
		&ds,
		&cb,
		0, // flags
		m_CheckerboardPipeLayout,
		VK_NULL_HANDLE, // render pass
		0, // sub pass
		VK_NULL_HANDLE, // base pipeline handle
		0, // base pipeline index
	};

	stages[0].shader = shader[BLITVS];
	stages[1].shader = shader[CHECKERBOARDFS];

	vkr = vt->CreateGraphicsPipelines(dev, m_PipelineCache, 1, &pipeInfo, &m_CheckerboardPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	stages[1].shader = shader[TEXDISPLAYFS];

	pipeInfo.layout = m_TexDisplayPipeLayout;

	vkr = vt->CreateGraphicsPipelines(dev, m_PipelineCache, 1, &pipeInfo, &m_TexDisplayPipeline);
	RDCASSERT(vkr == VK_SUCCESS);

	attState.blendEnable = true;
	attState.srcBlendColor = VK_BLEND_SRC_ALPHA;
	attState.destBlendColor = VK_BLEND_ONE_MINUS_SRC_ALPHA;

	vkr = vt->CreateGraphicsPipelines(dev, m_PipelineCache, 1, &pipeInfo, &m_TexDisplayBlendPipeline);
	RDCASSERT(vkr == VK_SUCCESS);

	for(size_t i=0; i < ARRAY_COUNT(module); i++)
	{
		vkr = vt->DestroyShader(dev, shader[i]);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->DestroyShaderModule(dev, module[i]);
		RDCASSERT(vkr == VK_SUCCESS);
	}
}

VulkanDebugManager::~VulkanDebugManager()
{
	VkDevice dev = m_Device;
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	VkResult vkr = VK_SUCCESS;

	if(m_PipelineCache != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineCache(dev, m_PipelineCache);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DescriptorPool != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorPool(dev, m_DescriptorPool);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DynamicCBStateWhite != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDynamicColorBlendState(dev, m_DynamicCBStateWhite);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DynamicRSState != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDynamicRasterState(dev, m_DynamicRSState);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DynamicDSStateDisabled != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDynamicDepthStencilState(dev, m_DynamicDSStateDisabled);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_LinearSampler != VK_NULL_HANDLE)
	{
		vkr = vt->DestroySampler(dev, m_LinearSampler);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_PointSampler != VK_NULL_HANDLE)
	{
		vkr = vt->DestroySampler(dev, m_PointSampler);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_FakeBBImView != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyImageView(dev, m_FakeBBImView);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_CheckerboardDescSetLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorSetLayout(dev, m_CheckerboardDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_CheckerboardPipeLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineLayout(dev, m_CheckerboardPipeLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_CheckerboardPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_CheckerboardPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TexDisplayDescSetLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorSetLayout(dev, m_TexDisplayDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TexDisplayPipeLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineLayout(dev, m_TexDisplayPipeLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TexDisplayPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_TexDisplayPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TexDisplayBlendPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_TexDisplayBlendPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	m_CheckerboardUBO.Destroy(vt, dev);
	m_TexDisplayUBO.Destroy(vt, dev);
}
