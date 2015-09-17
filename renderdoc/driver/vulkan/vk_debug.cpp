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

#include "stb/stb_truetype.h"

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

struct fontuniforms
{
	Vec2f TextPosition;
	float txtpadding;
	float TextSize;

	Vec2f CharacterSize;
	Vec2f FontScreenAspect;
};
		
struct glyph
{
	Vec4f posdata;
	Vec4f uvdata;
};

struct glyphdata
{
	glyph glyphs[127-32];
};

struct stringdata
{
	uint32_t str[256][4];
};

void VulkanDebugManager::UBO::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size)
{
	const VkLayerDispatchTable *vt = ObjDisp(dev);

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
	// VKTODOLOW needs tidy up - isn't scalable. Needs more classes like UBO above.

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
			
	m_TextDescSetLayout = VK_NULL_HANDLE;
	m_TextPipeLayout = VK_NULL_HANDLE;
	m_TextDescSet = VK_NULL_HANDLE;
	m_TextPipeline = VK_NULL_HANDLE;
	RDCEraseEl(m_TextGeneralUBO);
	RDCEraseEl(m_TextGlyphUBO);
	RDCEraseEl(m_TextStringUBO);
	m_TextAtlas = VK_NULL_HANDLE;
	m_TextAtlasMem = VK_NULL_HANDLE;
	m_TextAtlasView = VK_NULL_HANDLE;

	m_Device = dev;
	
	const VkLayerDispatchTable *vt = ObjDisp(dev);

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
		// VKTODOLOW not sure if these stage flags VK_SHADER_STAGE_... work yet?
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

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(dev, &descsetLayoutInfo, &m_TextDescSetLayout);
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

	pipeLayoutInfo.pSetLayouts = &m_TextDescSetLayout;
	
	vkr = vt->CreatePipelineLayout(dev, &pipeLayoutInfo, &m_TextPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	VkDescriptorTypeCount descPoolTypes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024, },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024, },
	};
	
	VkDescriptorPoolCreateInfo descpoolInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL,
    ARRAY_COUNT(descPoolTypes), &descPoolTypes[0],
	};
	
	vkr = vt->CreateDescriptorPool(dev, VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 3, &descpoolInfo, &m_DescriptorPool);
	RDCASSERT(vkr == VK_SUCCESS);
	
	uint32_t count;
	vkr = vt->AllocDescriptorSets(dev, m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_CheckerboardDescSetLayout, &m_CheckerboardDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);
	
	vkr = vt->AllocDescriptorSets(dev, m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_TexDisplayDescSetLayout, &m_TexDisplayDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);
	
	vkr = vt->AllocDescriptorSets(dev, m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_TextDescSetLayout, &m_TextDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);

	m_CheckerboardUBO.Create(driver, dev, 128);
	m_TexDisplayUBO.Create(driver, dev, 128);

	RDCCOMPILE_ASSERT(sizeof(displayuniforms) <= 128, "tex display size");
		
	m_TextGeneralUBO.Create(driver, dev, 128);
	RDCCOMPILE_ASSERT(sizeof(fontuniforms) <= 128, "font uniforms size");

	m_TextStringUBO.Create(driver, dev, 4096);
	RDCCOMPILE_ASSERT(sizeof(stringdata) <= 4096, "font uniforms size");

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
		GetEmbeddedResource(textvs_spv),
		GetEmbeddedResource(textfs_spv),
	};
	
	enum shaderIdx
	{
		BLITVS,
		CHECKERBOARDFS,
		TEXDISPLAYFS,
		TEXTVS,
		TEXTFS,
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

	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	
	stages[0].shader = shader[TEXTVS];
	stages[1].shader = shader[TEXTFS];

	pipeInfo.layout = m_TextPipeLayout;

	vkr = vt->CreateGraphicsPipelines(dev, m_PipelineCache, 1, &pipeInfo, &m_TextPipeline);
	RDCASSERT(vkr == VK_SUCCESS);

	for(size_t i=0; i < ARRAY_COUNT(module); i++)
	{
		vkr = vt->DestroyShader(dev, shader[i]);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->DestroyShaderModule(dev, module[i]);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	{
		int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

		VkImageCreateInfo imInfo = {
			/*.sType =*/ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			/*.pNext =*/ NULL,
			/*.imageType =*/ VK_IMAGE_TYPE_2D,
			/*.format =*/ VK_FORMAT_R8_UNORM,
			/*.extent =*/ { width, height, 1 },
			/*.mipLevels =*/ 1,
			/*.arraySize =*/ 1,
			/*.samples =*/ 1,
			/*.tiling =*/ VK_IMAGE_TILING_LINEAR,
			/*.usage =*/ VK_IMAGE_USAGE_SAMPLED_BIT,
			/*.flags =*/ 0,
		};

		string font = GetEmbeddedResource(sourcecodepro_ttf);
		byte *ttfdata = (byte *)font.c_str();

		const int firstChar = int(' ') + 1;
		const int lastChar = 127;
		const int numChars = lastChar-firstChar;

		byte *buf = new byte[width*height];

		const float pixelHeight = 20.0f;

		stbtt_bakedchar chardata[numChars];
		int ret = stbtt_BakeFontBitmap(ttfdata, 0, pixelHeight, buf, width, height, firstChar, numChars, chardata);

		m_FontCharSize = pixelHeight;
		m_FontCharAspect = chardata->xadvance / pixelHeight;

		stbtt_fontinfo f = {0};
		stbtt_InitFont(&f, ttfdata, 0);

		int ascent = 0;
		stbtt_GetFontVMetrics(&f, &ascent, NULL, NULL);

		float maxheight = float(ascent)*stbtt_ScaleForPixelHeight(&f, pixelHeight);

		// create and fill image
		{
			vkr = vt->CreateImage(dev, &imInfo, &m_TextAtlas);
			RDCASSERT(vkr == VK_SUCCESS);

			VkMemoryRequirements mrq;
			vkr = vt->GetImageMemoryRequirements(dev, m_TextAtlas, &mrq);
			RDCASSERT(vkr == VK_SUCCESS);

			VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR, 0, 0 };
			VkSubresourceLayout layout = { 0 };
			vt->GetImageSubresourceLayout(dev, m_TextAtlas, &subr, &layout);

			// allocate readback memory
			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				mrq.size, driver->GetUploadMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = vt->AllocMemory(dev, &allocInfo, &m_TextAtlasMem);
			RDCASSERT(vkr == VK_SUCCESS);
			vkr = vt->BindImageMemory(dev, m_TextAtlas, m_TextAtlasMem, 0);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkImageViewCreateInfo viewInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
				m_TextAtlas, VK_IMAGE_VIEW_TYPE_2D,
				imInfo.format,
				{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
				{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1, }
			};

			// VKTODOMED used for texture display, but eventually will have to be created on the fly
			// for whichever image we're viewing (and cached), not specifically created here.
			VkResult vkr = vt->CreateImageView(dev, &viewInfo, &m_TextAtlasView);
			RDCASSERT(vkr == VK_SUCCESS);

			// need to transition image into valid state, then upload
			VkCmdBuffer cmd = driver->GetCmd();
			VkQueue q = driver->GetQ();
			
			VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

			vkr = vt->ResetCommandBuffer(cmd, 0);
			RDCASSERT(vkr == VK_SUCCESS);
			vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkImageMemoryBarrier trans = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
				0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0, 0, m_TextAtlas,
				{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

			void *barrier = (void *)&trans;

			vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

			vt->EndCommandBuffer(cmd);

			vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);

			// VKTODOMED ideally all the commands from Bind to Flip would be recorded
			// into a single command buffer and we can just have several allocated
			// ring-buffer style
			vt->QueueWaitIdle(q);

			byte *pData = NULL;
			vkr = vt->MapMemory(dev, m_TextAtlasMem, 0, 0, 0, (void **)&pData);
			RDCASSERT(vkr == VK_SUCCESS);

			RDCASSERT(pData != NULL);

			for(int32_t row = 0; row < height; row++)
			{
				memcpy(pData, buf, width);
				pData += layout.rowPitch;
				buf += width;
			}

			vkr = vt->UnmapMemory(dev, m_TextAtlasMem);
			RDCASSERT(vkr == VK_SUCCESS);
		}

		m_TextGlyphUBO.Create(driver, dev, 4096);
		RDCCOMPILE_ASSERT(sizeof(Vec4f)*2*(numChars+1) < 4096, "font uniform size");

		Vec4f *glyphData = (Vec4f *)m_TextGlyphUBO.Map(vt, dev);

		for(int i=0; i < numChars; i++)
		{
			stbtt_bakedchar *b = chardata+i;

			float x = b->xoff;
			float y = b->yoff + maxheight;

			glyphData[(i+1)*2 + 0] = Vec4f(x/b->xadvance, y/pixelHeight, b->xadvance/float(b->x1 - b->x0), pixelHeight/float(b->y1 - b->y0));
			glyphData[(i+1)*2 + 1] = Vec4f(b->x0, b->y0, b->x1, b->y1);
		}

		m_TextGlyphUBO.Unmap(vt, dev);
	}

	VkDescriptorInfo desc[7];
	RDCEraseEl(desc);
	
	// checkerboard
	desc[0].bufferView = m_CheckerboardUBO.view;

	// tex display
	desc[1].bufferView = m_TexDisplayUBO.view;
	desc[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc[2].imageView = m_FakeBBImView;
	desc[2].sampler = m_LinearSampler;

	// text
	desc[3].bufferView = m_TextGeneralUBO.view;
	desc[4].bufferView = m_TextGlyphUBO.view;
	desc[5].bufferView = m_TextStringUBO.view;
	desc[6].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc[6].imageView = m_TextAtlasView;
	desc[6].sampler = m_LinearSampler;

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
			m_TextDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[3]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_TextDescSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[4]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_TextDescSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[5]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_TextDescSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc[6]
		},
			// this one is last so that we can skip it if we don't have m_FakeBBImView
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_TexDisplayDescSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc[2]
		},
	};

	uint32_t writeCount = (uint32_t)ARRAY_COUNT(writeSet);
	if(m_FakeBBImView == VK_NULL_HANDLE)
		writeCount--;

	vkr = vt->UpdateDescriptorSets(dev, writeCount, writeSet, 0, NULL);
	RDCASSERT(vkr == VK_SUCCESS);
}

VulkanDebugManager::~VulkanDebugManager()
{
	VkDevice dev = m_Device;
	const VkLayerDispatchTable *vt = ObjDisp(dev);

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

	if(m_TextDescSetLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorSetLayout(dev, m_TextDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TextPipeLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineLayout(dev, m_TextPipeLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TextPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_TextPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	m_TextGeneralUBO.Destroy(vt, dev);
	m_TextGlyphUBO.Destroy(vt, dev);
	m_TextStringUBO.Destroy(vt, dev);

	if(m_TextAtlasView != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyImageView(dev, m_TextAtlasView);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TextAtlas != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyImage(dev, m_TextAtlas);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_TextAtlasMem != VK_NULL_HANDLE)
	{
		vkr = vt->FreeMemory(dev, m_TextAtlasMem);
		RDCASSERT(vkr == VK_SUCCESS);
	}
}

void VulkanDebugManager::RenderText(const TextPrintState &textstate, float x, float y, const char *textfmt, ...)
{
	static char tmpBuf[4096];

	va_list args;
	va_start(args, textfmt);
	StringFormat::vsnprintf( tmpBuf, 4095, textfmt, args );
	tmpBuf[4095] = '\0';
	va_end(args);

	RenderTextInternal(textstate, x, y, tmpBuf);
}

void VulkanDebugManager::RenderTextInternal(const TextPrintState &textstate, float x, float y, const char *text)
{
	const VkLayerDispatchTable *vt = ObjDisp(m_Device);

	// VKTODOMED needs to be optimised to do all in one cmd buffer with
	// a start/stop pair of calls that map a UBO, then do each draw with
	// a push constant to tell it what the line should be.
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = VK_SUCCESS;

	fontuniforms *ubo = (fontuniforms *)m_TextGeneralUBO.Map(vt, m_Device);

	ubo->TextPosition.x = x;
	ubo->TextPosition.y = y;

	ubo->FontScreenAspect.x = 1.0f/float(textstate.w);
	ubo->FontScreenAspect.y = 1.0f/float(textstate.h);

	ubo->TextSize = m_FontCharSize;
	ubo->FontScreenAspect.x *= m_FontCharAspect;

	ubo->CharacterSize.x = 1.0f/float(FONT_TEX_WIDTH);
	ubo->CharacterSize.y = 1.0f/float(FONT_TEX_HEIGHT);

	m_TextGeneralUBO.Unmap(vt, m_Device);

	stringdata *stringData = (stringdata *)m_TextStringUBO.Map(vt, m_Device);

	for(size_t i=0; i < strlen(text); i++)
		stringData->str[i][0] = uint32_t(text[i] - ' ');

	m_TextStringUBO.Unmap(vt, m_Device);

	vkr = vt->ResetCommandBuffer(textstate.cmd, 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(textstate.cmd, &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	{
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			textstate.rp, textstate.fb,
			{ { 0, 0, }, { textstate.w, textstate.h} },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(textstate.cmd, &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		// VKTODOMED will need a way to disable blend for other things
		vt->CmdBindPipeline(textstate.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TextPipeline);
		vt->CmdBindDescriptorSets(textstate.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TextPipeLayout, 0, 1, &m_TextDescSet, 0, NULL);

		vt->CmdBindDynamicViewportState(textstate.cmd, textstate.vp);
		vt->CmdBindDynamicRasterState(textstate.cmd, m_DynamicRSState);
		vt->CmdBindDynamicColorBlendState(textstate.cmd, m_DynamicCBStateWhite);
		vt->CmdBindDynamicDepthStencilState(textstate.cmd, m_DynamicDSStateDisabled);

		// VKTODOMED strip + instance ID doesn't seem to work atm? instance ID comes through 0
		// for now, do lists, but want to change back 
		vt->CmdDraw(textstate.cmd, 0, 6*(uint32_t)strlen(text), 0, 1);
		vt->CmdEndRenderPass(textstate.cmd);
	}

	vt->EndCommandBuffer(textstate.cmd);

	vt->QueueSubmit(textstate.q, 1, &textstate.cmd, VK_NULL_HANDLE);

	vt->QueueWaitIdle(textstate.q);
}