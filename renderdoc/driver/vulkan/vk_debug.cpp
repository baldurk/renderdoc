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
#include "maths/matrix.h"

#include "stb/stb_truetype.h"

#include "3rdparty/glslang/SPIRV/spirv.hpp"

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

struct genericuniforms
{
	Vec4f Offset;
	Vec4f Scale;
	Vec4f Color;
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

struct meshuniforms
{
	Matrix4f mvp;
	Matrix4f invProj;
	Vec4f color;
	uint32_t displayFormat;
	uint32_t homogenousInput;
	Vec2f pointSpriteSize;
};

// histogram/minmax is calculated in blocks of NxN each with MxM tiles.
// e.g. a tile is 32x32 pixels, then this is arranged in blocks of 32x32 tiles.
// 1 compute thread = 1 tile, 1 compute group = 1 block
//
// NOTE because of this a block can cover more than the texture (think of a 1280x720
// texture covered by 2x1 blocks)
//
// these values are in each dimension
#define HGRAM_PIXELS_PER_TILE  64
#define HGRAM_TILES_PER_BLOCK  32

#define HGRAM_NUM_BUCKETS	   256

struct histogramuniforms
{
	uint32_t HistogramChannels;
	float HistogramMin;
	float HistogramMax;
	uint32_t HistogramFlags;
	
	float HistogramSlice;
	uint32_t HistogramMip;
	int HistogramSample;
	int HistogramNumSamples;

	Vec3f HistogramTextureResolution;
	float Padding3;
};

struct outlineuniforms
{
	Vec4f Inner_Color;
	Vec4f Border_Color;
	Vec4f ViewRect;
	uint32_t Scissor;
	Vec3f padding;
};

void VulkanDebugManager::GPUBuffer::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t ringSize, uint32_t flags)
{
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	m_ResourceManager = driver->GetResourceManager();

	align = (VkDeviceSize)driver->GetDeviceProps().limits.minUniformBufferOffsetAlignment;

	sz = size;
	// offset must be aligned, so ensure we have at least ringSize
	// copies accounting for that
	totalsize = ringSize == 1 ? size : AlignUp(size, align)*ringSize;
	curoffset = 0;

	VkBufferCreateInfo bufInfo = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
		totalsize, 0, 0,
		VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	};

	bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
	bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
	bufInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	
	if(flags & eGPUBufferVBuffer)
		bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	
	if(flags & eGPUBufferSSBO)
		bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VkResult vkr = vt->CreateBuffer(Unwrap(dev), &bufInfo, &buf);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), buf);

	VkMemoryRequirements mrq;
	vkr = vt->GetBufferMemoryRequirements(Unwrap(dev), Unwrap(buf), &mrq);
	RDCASSERT(vkr == VK_SUCCESS);

	VkMemoryAllocInfo allocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
		mrq.size,
		(flags & eGPUBufferReadback)
		? driver->GetReadbackMemoryIndex(mrq.memoryTypeBits)
		: driver->GetUploadMemoryIndex(mrq.memoryTypeBits),
	};

	vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &mem);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), mem);

	vkr = vt->BindBufferMemory(Unwrap(dev), Unwrap(buf), Unwrap(mem), 0);
	RDCASSERT(vkr == VK_SUCCESS);
}

void VulkanDebugManager::GPUBuffer::FillDescriptor(VkDescriptorInfo &desc)
{
	desc.bufferInfo.buffer = Unwrap(buf);
	desc.bufferInfo.offset = 0;
	desc.bufferInfo.range = sz;
}

void VulkanDebugManager::GPUBuffer::Destroy(const VkLayerDispatchTable *vt, VkDevice dev)
{
	VkResult vkr = VK_SUCCESS;

	if(buf != VK_NULL_HANDLE)
	{
		vt->DestroyBuffer(Unwrap(dev), Unwrap(buf));
		RDCASSERT(vkr == VK_SUCCESS);
		GetResourceManager()->ReleaseWrappedResource(buf);
		buf = VK_NULL_HANDLE;
	}

	if(mem != VK_NULL_HANDLE)
	{
		vt->FreeMemory(Unwrap(dev), Unwrap(mem));
		RDCASSERT(vkr == VK_SUCCESS);
		GetResourceManager()->ReleaseWrappedResource(mem);
		mem = VK_NULL_HANDLE;
	}
}

void *VulkanDebugManager::GPUBuffer::Map(const VkLayerDispatchTable *vt, VkDevice dev, uint32_t *bindoffset, VkDeviceSize usedsize)
{
	VkDeviceSize offset = bindoffset ? curoffset : 0;
	VkDeviceSize size = usedsize > 0 ? usedsize : sz;

	// wrap around the ring, assuming the ring is large enough
	// that this memory is now free
	if(offset + size > totalsize)
		offset = 0;
	RDCASSERT(offset + size <= totalsize);
	
	// offset must be aligned
	curoffset = AlignUp(offset+size, align);

	if(bindoffset) *bindoffset = (uint32_t)offset;

	void *ptr = NULL;
	VkResult vkr = vt->MapMemory(Unwrap(dev), Unwrap(mem), offset, size, 0, (void **)&ptr);
	RDCASSERT(vkr == VK_SUCCESS);
	return ptr;
}

void *VulkanDebugManager::GPUBuffer::Map(const VkLayerDispatchTable *vt, VkDevice dev, VkDeviceSize &bindoffset, VkDeviceSize usedsize)
{
	uint32_t offs = 0;

	void *ret = Map(vt, dev, &offs, usedsize);

	bindoffset = offs;

	return ret;
}

void VulkanDebugManager::GPUBuffer::Unmap(const VkLayerDispatchTable *vt, VkDevice dev)
{
	vt->UnmapMemory(Unwrap(dev), Unwrap(mem));
}

VulkanDebugManager::VulkanDebugManager(WrappedVulkan *driver, VkDevice dev)
{
	// VKTODOLOW needs tidy up - isn't scalable. Needs more classes like UBO above.
	m_pDriver = driver;

	m_ResourceManager = m_pDriver->GetResourceManager();

	m_DescriptorPool = VK_NULL_HANDLE;
	m_LinearSampler = VK_NULL_HANDLE;
	m_PointSampler = VK_NULL_HANDLE;

	m_CheckerboardDescSetLayout = VK_NULL_HANDLE;
	m_CheckerboardPipeLayout = VK_NULL_HANDLE;
	m_CheckerboardDescSet = VK_NULL_HANDLE;
	m_CheckerboardPipeline = VK_NULL_HANDLE;
	m_CheckerboardMSAAPipeline = VK_NULL_HANDLE;
	RDCEraseEl(m_CheckerboardUBO);

	m_TexDisplayDescSetLayout = VK_NULL_HANDLE;
	m_TexDisplayPipeLayout = VK_NULL_HANDLE;
	RDCEraseEl(m_TexDisplayDescSet);
	m_TexDisplayNextSet = 0;
	m_TexDisplayPipeline = VK_NULL_HANDLE;
	m_TexDisplayBlendPipeline = VK_NULL_HANDLE;
	m_TexDisplayF32Pipeline = VK_NULL_HANDLE;
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

	m_GenericDescSetLayout = VK_NULL_HANDLE;
	m_GenericPipeLayout = VK_NULL_HANDLE;
	m_GenericDescSet = VK_NULL_HANDLE;
	m_HighlightBoxPipeline = VK_NULL_HANDLE;
		
	m_OverlayImageMem = VK_NULL_HANDLE;
	m_OverlayImage = VK_NULL_HANDLE;
	m_OverlayImageView = VK_NULL_HANDLE;
	m_OverlayNoDepthFB = VK_NULL_HANDLE;
	m_OverlayNoDepthRP = VK_NULL_HANDLE;
	RDCEraseEl(m_OverlayDim);
	m_OverlayMemSize = 0;
		
	m_MeshDescSetLayout = VK_NULL_HANDLE;
	m_MeshPipeLayout = VK_NULL_HANDLE;
	m_MeshDescSet = VK_NULL_HANDLE;
	RDCEraseEl(m_MeshShaders);
	RDCEraseEl(m_MeshModules);

	m_HistogramDescSetLayout = VK_NULL_HANDLE;
	m_HistogramPipeLayout = VK_NULL_HANDLE;
	RDCEraseEl(m_HistogramDescSet);
	m_MinMaxResultPipe = VK_NULL_HANDLE;
	m_MinMaxTilePipe = VK_NULL_HANDLE;
	m_HistogramPipe = VK_NULL_HANDLE;

	m_OutlineDescSetLayout = VK_NULL_HANDLE;
	m_OutlinePipeLayout = VK_NULL_HANDLE;
	m_OutlineDescSet = VK_NULL_HANDLE;
	m_OutlinePipeline = VK_NULL_HANDLE;

	m_Device = dev;
	
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkResult vkr = VK_SUCCESS;

	VkSamplerCreateInfo sampInfo = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL,
		VK_TEX_FILTER_LINEAR, VK_TEX_FILTER_LINEAR,
		VK_TEX_MIPMAP_MODE_LINEAR, 
		VK_TEX_ADDRESS_MODE_CLAMP, VK_TEX_ADDRESS_MODE_CLAMP, VK_TEX_ADDRESS_MODE_CLAMP,
		0.0f, // lod bias
		1.0f, // max aniso
		false, VK_COMPARE_OP_NEVER,
		0.0f, 128.0f, // min/max lod
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		false, // unnormalized
	};

	vkr = vt->CreateSampler(Unwrap(dev), &sampInfo, &m_LinearSampler);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_LinearSampler);

	sampInfo.minFilter = VK_TEX_FILTER_NEAREST;
	sampInfo.magFilter = VK_TEX_FILTER_NEAREST;
	sampInfo.mipMode = VK_TEX_MIPMAP_MODE_NEAREST;

	vkr = vt->CreateSampler(Unwrap(dev), &sampInfo, &m_PointSampler);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_PointSampler);

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_CheckerboardDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_CheckerboardDescSetLayout);

		// identical layout
		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_GenericDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_GenericDescSetLayout);

		// identical layout
		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_MeshDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_MeshDescSetLayout);

		// identical layout
		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_OutlineDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_OutlineDescSetLayout);
	}

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_TexDisplayDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_TexDisplayDescSetLayout);
	}

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_TextDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_TextDescSetLayout);
	}

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(Unwrap(dev), &descsetLayoutInfo, &m_HistogramDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_HistogramDescSetLayout);
	}

	VkPipelineLayoutCreateInfo pipeLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL,
		1, UnwrapPtr(m_TexDisplayDescSetLayout),
		0, NULL, // push constant ranges
	};
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_TexDisplayPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_TexDisplayPipeLayout);

	pipeLayoutInfo.pSetLayouts = UnwrapPtr(m_CheckerboardDescSetLayout);
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_CheckerboardPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_CheckerboardPipeLayout);

	pipeLayoutInfo.pSetLayouts = UnwrapPtr(m_TextDescSetLayout);
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_TextPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_TextPipeLayout);

	pipeLayoutInfo.pSetLayouts = UnwrapPtr(m_GenericDescSetLayout);
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_GenericPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_GenericPipeLayout);

	pipeLayoutInfo.pSetLayouts = UnwrapPtr(m_OutlineDescSetLayout);
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_OutlinePipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_OutlinePipeLayout);

	pipeLayoutInfo.pSetLayouts = UnwrapPtr(m_MeshDescSetLayout);
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_MeshPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_MeshPipeLayout);

	pipeLayoutInfo.pSetLayouts = UnwrapPtr(m_HistogramDescSetLayout);
	
	vkr = vt->CreatePipelineLayout(Unwrap(dev), &pipeLayoutInfo, &m_HistogramPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_HistogramPipeLayout);

	VkDescriptorTypeCount descPoolTypes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024, },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024, },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024, },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024, },
	};
	
	VkDescriptorPoolCreateInfo descpoolInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL,
		VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 7+ARRAY_COUNT(m_TexDisplayDescSet),
		ARRAY_COUNT(descPoolTypes), &descPoolTypes[0],
	};
	
	vkr = vt->CreateDescriptorPool(Unwrap(dev), &descpoolInfo, &m_DescriptorPool);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_DescriptorPool);
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_CheckerboardDescSetLayout), &m_CheckerboardDescSet);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_CheckerboardDescSet);
	
	for(size_t i=0; i < ARRAY_COUNT(m_TexDisplayDescSet); i++)
	{
		vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
			UnwrapPtr(m_TexDisplayDescSetLayout), &m_TexDisplayDescSet[i]);
		RDCASSERT(vkr == VK_SUCCESS);
		
		GetResourceManager()->WrapResource(Unwrap(dev), m_TexDisplayDescSet[i]);
	}
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_TextDescSetLayout), &m_TextDescSet);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_TextDescSet);
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_GenericDescSetLayout), &m_GenericDescSet);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_GenericDescSet);
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_OutlineDescSetLayout), &m_OutlineDescSet);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_OutlineDescSet);
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_MeshDescSetLayout), &m_MeshDescSet);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_MeshDescSet);
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_HistogramDescSetLayout), &m_HistogramDescSet[0]);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_HistogramDescSet[0]);
	
	vkr = vt->AllocDescriptorSets(Unwrap(dev), Unwrap(m_DescriptorPool), VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		UnwrapPtr(m_HistogramDescSetLayout), &m_HistogramDescSet[1]);
	RDCASSERT(vkr == VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(dev), m_HistogramDescSet[1]);

	m_GenericUBO.Create(driver, dev, 128, 10, 0);
	RDCCOMPILE_ASSERT(sizeof(genericuniforms) <= 128, "generic UBO size");

	m_OutlineUBO.Create(driver, dev, 128, 10, 0);
	RDCCOMPILE_ASSERT(sizeof(outlineuniforms) <= 128, "outline UBO size");
	
	{
		float data[] = {
			0.0f,  0.0f, 0.0f, 1.0f,
			1.0f,  0.0f, 0.0f, 1.0f,

			1.0f,  0.0f, 0.0f, 1.0f,
			1.0f,  1.0f, 0.0f, 1.0f,

			1.0f,  1.0f, 0.0f, 1.0f,
			0.0f,  1.0f, 0.0f, 1.0f,

			0.0f,  1.0f, 0.0f, 1.0f,
			0.0f, -0.1f, 0.0f, 1.0f,
		};
		
		m_OutlineStripVBO.Create(driver, dev, 128, 1, 0); // doesn't need to be ring buffered
		RDCCOMPILE_ASSERT(sizeof(data) <= 128, "outline strip VBO size");
		
		float *mapped = (float *)m_OutlineStripVBO.Map(vt, dev, (uint32_t *)NULL);

		memcpy(mapped, data, sizeof(data));

		m_OutlineStripVBO.Unmap(vt, dev);
	}

	m_CheckerboardUBO.Create(driver, dev, 128, 10, 0);
	m_TexDisplayUBO.Create(driver, dev, 128, 10, 0);

	RDCCOMPILE_ASSERT(sizeof(displayuniforms) <= 128, "tex display size");
		
	m_TextGeneralUBO.Create(driver, dev, 128, 100, 0); // make the ring conservatively large to handle many lines of text * several frames
	RDCCOMPILE_ASSERT(sizeof(fontuniforms) <= 128, "font uniforms size");

	m_TextStringUBO.Create(driver, dev, 4096, 10, 0); // we only use a subset of the [256] array needed for each line, so this ring can be smaller
	RDCCOMPILE_ASSERT(sizeof(stringdata) <= 4096, "font uniforms size");
	
	string shaderSources[] = {
		GetEmbeddedResource(blitvs_spv),
		GetEmbeddedResource(checkerboardfs_spv),
		GetEmbeddedResource(texdisplayfs_spv),
		GetEmbeddedResource(textvs_spv),
		GetEmbeddedResource(textfs_spv),
		GetEmbeddedResource(genericvs_spv),
		GetEmbeddedResource(genericfs_spv),
		GetEmbeddedResource(meshvs_spv),
		GetEmbeddedResource(meshgs_spv),
		GetEmbeddedResource(meshfs_spv),
		GetEmbeddedResource(minmaxtilecs_spv),
		GetEmbeddedResource(minmaxresultcs_spv),
		GetEmbeddedResource(histogramcs_spv),
		GetEmbeddedResource(outlinefs_spv),
	};

	VkShaderStage shaderStages[] = {
		VK_SHADER_STAGE_VERTEX,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_VERTEX,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_VERTEX,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_VERTEX,
		VK_SHADER_STAGE_GEOMETRY,
		VK_SHADER_STAGE_FRAGMENT,
		VK_SHADER_STAGE_COMPUTE,
		VK_SHADER_STAGE_COMPUTE,
		VK_SHADER_STAGE_COMPUTE,
		VK_SHADER_STAGE_FRAGMENT,
	};
	
	enum shaderIdx
	{
		BLITVS,
		CHECKERBOARDFS,
		TEXDISPLAYFS,
		TEXTVS,
		TEXTFS,
		GENERICVS,
		GENERICFS,
		MESHVS,
		MESHGS,
		MESHFS,
		MINMAXTILECS,
		MINMAXRESULTCS,
		HISTOGRAMCS,
		OUTLINEFS,
		NUM_SHADERS,
	};

	RDCCOMPILE_ASSERT( ARRAY_COUNT(shaderSources) == ARRAY_COUNT(shaderStages), "Mismatched arrays!" );
	RDCCOMPILE_ASSERT( ARRAY_COUNT(shaderSources) == NUM_SHADERS, "Mismatched arrays!" );

	VkShaderModule module[ARRAY_COUNT(shaderSources)];
	VkShader shader[ARRAY_COUNT(shaderSources)];
	
	for(size_t i=0; i < ARRAY_COUNT(module); i++)
	{
		VkShaderModuleCreateInfo modinfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,
			shaderSources[i].size(), (void *)&shaderSources[i][0], 0,
		};

		vkr = vt->CreateShaderModule(Unwrap(dev), &modinfo, &module[i]);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), module[i]);

		VkShaderCreateInfo shadinfo = {
			VK_STRUCTURE_TYPE_SHADER_CREATE_INFO, NULL,
			Unwrap(module[i]), "main", 0,
			shaderStages[i],
		};

		vkr = vt->CreateShader(Unwrap(dev), &shadinfo, &shader[i]);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), shader[i]);
	}

	VkRenderPass RGBA32RP, RGBA8RP, RGBA16RP, RGBA8MSRP; // compatible render passes for creating pipelines

	{
		VkAttachmentDescription attDesc = {
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
			VK_FORMAT_R8G8B8A8_UNORM, 1,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription sub = {
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION, NULL,
			VK_PIPELINE_BIND_POINT_GRAPHICS, 0,
			0, NULL, // inputs
			1, &attRef, // color
			NULL, // resolve
			{ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED }, // depth-stencil
			0, NULL, // preserve
		};

		VkRenderPassCreateInfo rpinfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL,
				1, &attDesc,
				1, &sub,
				0, NULL, // dependencies
		};
		
		vt->CreateRenderPass(Unwrap(dev), &rpinfo, &RGBA8RP);

		attDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;

		vt->CreateRenderPass(Unwrap(dev), &rpinfo, &RGBA32RP);

		attDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;

		vt->CreateRenderPass(Unwrap(dev), &rpinfo, &RGBA16RP);

		attDesc.samples = 4;
		attDesc.format = VK_FORMAT_R8G8B8A8_SRGB;
		
		vt->CreateRenderPass(Unwrap(dev), &rpinfo, &RGBA8MSRP);
	}

	VkPipelineShaderStageCreateInfo stages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_VERTEX, VK_NULL_HANDLE, NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_FRAGMENT, VK_NULL_HANDLE, NULL },
	};

	VkPipelineInputAssemblyStateCreateInfo ia = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, false,
	};

	VkRect2D scissor = { { 0, 0 }, { 4096, 4096 } };

	VkPipelineViewportStateCreateInfo vp = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL,
		1, NULL,
		1, &scissor
	};

	VkPipelineRasterStateCreateInfo rs = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO, NULL,
		true, false, VK_FILL_MODE_SOLID, VK_CULL_MODE_NONE, VK_FRONT_FACE_CW,
		false, 0.0f, 0.0f, 0.0f, 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo msaa = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL,
		1, false, 0.0f, NULL,
	};

	VkPipelineDepthStencilStateCreateInfo ds = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL,
		false, false, VK_COMPARE_OP_ALWAYS, false, false,
		{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
		{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
		0.0f, 1.0f,
	};

	VkPipelineColorBlendAttachmentState attState = {
		false,
		VK_BLEND_ONE, VK_BLEND_ZERO, VK_BLEND_OP_ADD,
		VK_BLEND_ONE, VK_BLEND_ZERO, VK_BLEND_OP_ADD,
		0xf,
	};

	VkPipelineColorBlendStateCreateInfo cb = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL,
		false, false, false, VK_LOGIC_OP_NOOP,
		1, &attState,
		{ 1.0f, 1.0f, 1.0f, 1.0f }
	};

	VkDynamicState dynstates[] = { VK_DYNAMIC_STATE_VIEWPORT };

	VkPipelineDynamicStateCreateInfo dyn = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL,
		ARRAY_COUNT(dynstates), dynstates,
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
		&dyn,
		0, // flags
		Unwrap(m_CheckerboardPipeLayout),
		RGBA8RP,
		0, // sub pass
		VK_NULL_HANDLE, // base pipeline handle
		0, // base pipeline index
	};

	stages[0].shader = Unwrap(shader[BLITVS]);
	stages[1].shader = Unwrap(shader[CHECKERBOARDFS]);

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_CheckerboardPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_CheckerboardPipeline);

	msaa.rasterSamples = 4;
	pipeInfo.renderPass = RGBA8MSRP;

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_CheckerboardMSAAPipeline);
	RDCASSERT(vkr == VK_SUCCESS);

	msaa.rasterSamples = 1;
	pipeInfo.renderPass = RGBA8RP;
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_CheckerboardMSAAPipeline);
	
	stages[0].shader = Unwrap(shader[BLITVS]);
	stages[1].shader = Unwrap(shader[TEXDISPLAYFS]);

	pipeInfo.layout = Unwrap(m_TexDisplayPipeLayout);

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_TexDisplayPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_TexDisplayPipeline);

	pipeInfo.renderPass = RGBA32RP;

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_TexDisplayF32Pipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_TexDisplayF32Pipeline);

	pipeInfo.renderPass = RGBA8RP;

	attState.blendEnable = true;
	attState.srcBlendColor = VK_BLEND_SRC_ALPHA;
	attState.destBlendColor = VK_BLEND_ONE_MINUS_SRC_ALPHA;

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_TexDisplayBlendPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_TexDisplayBlendPipeline);

	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	
	stages[0].shader = Unwrap(shader[TEXTVS]);
	stages[1].shader = Unwrap(shader[TEXTFS]);

	pipeInfo.layout = Unwrap(m_TextPipeLayout);

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_TextPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_TextPipeline);
	
	stages[0].shader = Unwrap(shader[BLITVS]);
	stages[1].shader = Unwrap(shader[OUTLINEFS]);

	pipeInfo.layout = Unwrap(m_OutlinePipeLayout);

	pipeInfo.renderPass = RGBA16RP;
	
	attState.srcBlendAlpha = VK_BLEND_SRC_ALPHA;
	attState.destBlendAlpha = VK_BLEND_ONE_MINUS_SRC_ALPHA;

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_OutlinePipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_OutlinePipeline);

	pipeInfo.renderPass = RGBA8RP;
	
	ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	attState.blendEnable = false;

	VkVertexInputBindingDescription vertexBind = {
		0, sizeof(Vec4f), VK_VERTEX_INPUT_STEP_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertexAttr = {
		0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0,
	};

	VkPipelineVertexInputStateCreateInfo vi = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL,
		1, &vertexBind,
		1, &vertexAttr,
	};

	pipeInfo.pVertexInputState = &vi;
	
	stages[0].shader = Unwrap(shader[GENERICVS]);
	stages[1].shader = Unwrap(shader[GENERICFS]);

	pipeInfo.layout = Unwrap(m_GenericPipeLayout);

	vkr = vt->CreateGraphicsPipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &pipeInfo, &m_HighlightBoxPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_HighlightBoxPipeline);

	VkComputePipelineCreateInfo compPipeInfo = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, NULL,
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_COMPUTE, VK_NULL_HANDLE, NULL },
		0, // flags
		Unwrap(m_HistogramPipeLayout),
		VK_NULL_HANDLE, 0, // base pipeline VkPipeline
	};

	compPipeInfo.stage.shader = Unwrap(shader[MINMAXTILECS]);
	
	vkr = vt->CreateComputePipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &compPipeInfo, &m_MinMaxTilePipe);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_MinMaxTilePipe);
	
	compPipeInfo.stage.shader = Unwrap(shader[MINMAXRESULTCS]);
	
	vkr = vt->CreateComputePipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &compPipeInfo, &m_MinMaxResultPipe);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_MinMaxResultPipe);
	
	compPipeInfo.stage.shader = Unwrap(shader[HISTOGRAMCS]);
	
	vkr = vt->CreateComputePipelines(Unwrap(dev), VK_NULL_HANDLE, 1, &compPipeInfo, &m_HistogramPipe);
	RDCASSERT(vkr == VK_SUCCESS);
	
	GetResourceManager()->WrapResource(Unwrap(dev), m_HistogramPipe);
	
	vt->DestroyRenderPass(Unwrap(dev), RGBA16RP);
	vt->DestroyRenderPass(Unwrap(dev), RGBA32RP);
	vt->DestroyRenderPass(Unwrap(dev), RGBA8RP);
	vt->DestroyRenderPass(Unwrap(dev), RGBA8MSRP);

	for(size_t i=0; i < ARRAY_COUNT(module); i++)
	{
		// hold onto the mesh shaders/modules as we create these
		// pipelines later
		if(i == MESHVS)
		{
			m_MeshShaders[0] = shader[i];
			m_MeshModules[0] = module[i];
		}
		else if(i == MESHGS)
		{
			m_MeshShaders[1] = shader[i];
			m_MeshModules[1] = module[i];
		}
		else if(i == MESHFS)
		{
			m_MeshShaders[2] = shader[i];
			m_MeshModules[2] = module[i];
		}
		else
		{
			vt->DestroyShader(Unwrap(dev), Unwrap(shader[i]));
			GetResourceManager()->ReleaseWrappedResource(shader[i]);

			vt->DestroyShaderModule(Unwrap(dev), Unwrap(module[i]));
			GetResourceManager()->ReleaseWrappedResource(module[i]);
		}
	}

	VkCmdBuffer cmd = driver->GetNextCmd();

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	{
		int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, VK_FORMAT_R8_UNORM,
			{ width, height, 1 }, 1, 1, 1,
			VK_IMAGE_TILING_LINEAR,
			VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
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
			vkr = vt->CreateImage(Unwrap(dev), &imInfo, &m_TextAtlas);
			RDCASSERT(vkr == VK_SUCCESS);
				
			GetResourceManager()->WrapResource(Unwrap(dev), m_TextAtlas);

			VkMemoryRequirements mrq;
			vkr = vt->GetImageMemoryRequirements(Unwrap(dev), Unwrap(m_TextAtlas), &mrq);
			RDCASSERT(vkr == VK_SUCCESS);

			VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR, 0, 0 };
			VkSubresourceLayout layout = { 0 };
			vt->GetImageSubresourceLayout(Unwrap(dev), Unwrap(m_TextAtlas), &subr, &layout);

			// allocate readback memory
			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				mrq.size, driver->GetUploadMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &m_TextAtlasMem);
			RDCASSERT(vkr == VK_SUCCESS);
				
			GetResourceManager()->WrapResource(Unwrap(dev), m_TextAtlasMem);

			vkr = vt->BindImageMemory(Unwrap(dev), Unwrap(m_TextAtlas), Unwrap(m_TextAtlasMem), 0);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkImageViewCreateInfo viewInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
				Unwrap(m_TextAtlas), VK_IMAGE_VIEW_TYPE_2D,
				imInfo.format,
				{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, },
				0,
			};

			vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, &m_TextAtlasView);
			RDCASSERT(vkr == VK_SUCCESS);
				
			GetResourceManager()->WrapResource(Unwrap(dev), m_TextAtlasView);

			// need to transition image into valid state, then upload
			
			VkImageMemoryBarrier trans = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
				0, 0,
				VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				Unwrap(m_TextAtlas),
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};

			trans.outputMask = VK_MEMORY_OUTPUT_HOST_WRITE_BIT | VK_MEMORY_OUTPUT_TRANSFER_BIT;

			void *barrier = (void *)&trans;

			vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

			byte *pData = NULL;
			vkr = vt->MapMemory(Unwrap(dev), Unwrap(m_TextAtlasMem), 0, 0, 0, (void **)&pData);
			RDCASSERT(vkr == VK_SUCCESS);

			RDCASSERT(pData != NULL);

			for(int32_t row = 0; row < height; row++)
			{
				memcpy(pData, buf, width);
				pData += layout.rowPitch;
				buf += width;
			}

			vt->UnmapMemory(Unwrap(dev), Unwrap(m_TextAtlasMem));
		}

		m_TextGlyphUBO.Create(driver, dev, 4096, 1, 0); // doesn't need to be ring'd, as it's static
		RDCCOMPILE_ASSERT(sizeof(Vec4f)*2*(numChars+1) < 4096, "font uniform size");

		Vec4f *glyphData = (Vec4f *)m_TextGlyphUBO.Map(vt, dev, (uint32_t *)NULL);

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

	// pick pixel data
	{
		// create image
		
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT,
			{ 1, 1, 1 }, 1, 1, 1,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,
			0,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};
		
		vkr = vt->CreateImage(Unwrap(dev), &imInfo, &m_PickPixelImage);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_PickPixelImage);

		VkMemoryRequirements mrq;
		vkr = vt->GetImageMemoryRequirements(Unwrap(dev), Unwrap(m_PickPixelImage), &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR, 0, 0 };
		VkSubresourceLayout layout = { 0 };
		vt->GetImageSubresourceLayout(Unwrap(dev), Unwrap(m_PickPixelImage), &subr, &layout);

		// allocate readback memory
		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &m_PickPixelImageMem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_PickPixelImageMem);

		vkr = vt->BindImageMemory(Unwrap(dev), Unwrap(m_PickPixelImage), Unwrap(m_PickPixelImageMem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(m_PickPixelImage), VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, },
			0,
		};

		vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, &m_PickPixelImageView);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_PickPixelImageView);

		// need to transition image into valid state

		VkImageMemoryBarrier trans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			Unwrap(m_PickPixelImage),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		void *barrier = (void *)&trans;

		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

		// create render pass
		VkAttachmentDescription attDesc = {
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
			VK_FORMAT_R32G32B32A32_SFLOAT, 1,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription sub = {
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION, NULL,
			VK_PIPELINE_BIND_POINT_GRAPHICS, 0,
			0, NULL, // inputs
			1, &attRef, // color
			NULL, // resolve
			{ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED }, // depth-stencil
			0, NULL, // preserve
		};

		VkRenderPassCreateInfo rpinfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL,
				1, &attDesc,
				1, &sub,
				0, NULL, // dependencies
		};

		vkr = vt->CreateRenderPass(Unwrap(dev), &rpinfo, &m_PickPixelRP);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_PickPixelRP);

		// create framebuffer
		VkFramebufferCreateInfo fbinfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
			Unwrap(m_PickPixelRP),
			1, UnwrapPtr(m_PickPixelImageView),
			1, 1, 1,
		};

		vkr = vt->CreateFramebuffer(Unwrap(dev), &fbinfo, &m_PickPixelFB);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(dev), m_PickPixelFB);

		// since we always sync for readback, doesn't need to be ring'd
		m_PickPixelReadbackBuffer.Create(driver, dev, sizeof(float)*4, 1, GPUBuffer::eGPUBufferReadback);
	}

	m_MeshUBO.Create(driver, dev, sizeof(meshuniforms), 16, 0);
	m_MeshBBoxVB.Create(driver, dev, sizeof(Vec4f)*128, 16, GPUBuffer::eGPUBufferVBuffer);
	
	Vec4f TLN = Vec4f(-1.0f,  1.0f, 0.0f, 1.0f); // TopLeftNear, etc...
	Vec4f TRN = Vec4f( 1.0f,  1.0f, 0.0f, 1.0f);
	Vec4f BLN = Vec4f(-1.0f, -1.0f, 0.0f, 1.0f);
	Vec4f BRN = Vec4f( 1.0f, -1.0f, 0.0f, 1.0f);

	Vec4f TLF = Vec4f(-1.0f,  1.0f, 1.0f, 1.0f);
	Vec4f TRF = Vec4f( 1.0f,  1.0f, 1.0f, 1.0f);
	Vec4f BLF = Vec4f(-1.0f, -1.0f, 1.0f, 1.0f);
	Vec4f BRF = Vec4f( 1.0f, -1.0f, 1.0f, 1.0f);

	Vec4f axisFrustum[] = {
		// axis marker vertices
		Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
		Vec4f(1.0f, 0.0f, 0.0f, 1.0f),
		Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
		Vec4f(0.0f, 1.0f, 0.0f, 1.0f),
		Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
		Vec4f(0.0f, 0.0f, 1.0f, 1.0f),

		// frustum vertices
		TLN, TRN,
		TRN, BRN,
		BRN, BLN,
		BLN, TLN,

		TLN, TLF,
		TRN, TRF,
		BLN, BLF,
		BRN, BRF,

		TLF, TRF,
		TRF, BRF,
		BRF, BLF,
		BLF, TLF,
	};

	// doesn't need to be ring'd as it's immutable
	m_MeshAxisFrustumVB.Create(driver, dev, sizeof(axisFrustum), 1, GPUBuffer::eGPUBufferVBuffer);

	Vec4f *axisData = (Vec4f *)m_MeshAxisFrustumVB.Map(vt, dev, (uint32_t *)NULL);

	memcpy(axisData, axisFrustum, sizeof(axisFrustum));

	m_MeshAxisFrustumVB.Unmap(vt, dev);
	
	const uint32_t maxTexDim = 16384;
	const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK;
	const uint32_t maxBlocksNeeded = (maxTexDim*maxTexDim)/(blockPixSize*blockPixSize);

	const size_t byteSize = 2*sizeof(Vec4f)*HGRAM_TILES_PER_BLOCK*HGRAM_TILES_PER_BLOCK*maxBlocksNeeded;

	m_MinMaxTileResult.Create(driver, dev, byteSize, 1, GPUBuffer::eGPUBufferSSBO);
	m_MinMaxResult.Create(driver, dev, sizeof(Vec4f)*2, 1, GPUBuffer::eGPUBufferSSBO);
	m_MinMaxReadback.Create(driver, dev, sizeof(Vec4f)*2, 1, GPUBuffer::eGPUBufferReadback);
	m_HistogramBuf.Create(driver, dev, sizeof(uint32_t)*HGRAM_NUM_BUCKETS, 1, GPUBuffer::eGPUBufferSSBO);
	m_HistogramReadback.Create(driver, dev, sizeof(uint32_t)*HGRAM_NUM_BUCKETS, 1, GPUBuffer::eGPUBufferReadback);

	// don't need to ring this, as we hard-sync for readback anyway
	m_HistogramUBO.Create(driver, dev, sizeof(histogramuniforms), 1, 0);

	VkDescriptorInfo desc[8];
	RDCEraseEl(desc);
	
	// tex display is updated right before rendering
	
	m_CheckerboardUBO.FillDescriptor(desc[0]);
	m_TextGeneralUBO.FillDescriptor(desc[1]);
	m_TextGlyphUBO.FillDescriptor(desc[2]);
	m_TextStringUBO.FillDescriptor(desc[3]);
	desc[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc[4].imageView = Unwrap(m_TextAtlasView);
	desc[4].sampler = Unwrap(m_LinearSampler);
	
	m_GenericUBO.FillDescriptor(desc[5]);
	m_MeshUBO.FillDescriptor(desc[6]);
	m_OutlineUBO.FillDescriptor(desc[7]);

	VkWriteDescriptorSet writeSet[] = {
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_CheckerboardDescSet), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &desc[0]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_TextDescSet), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &desc[1]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_TextDescSet), 1, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[2]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_TextDescSet), 2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &desc[3]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_TextDescSet), 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc[4]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_GenericDescSet), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &desc[5]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_MeshDescSet), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &desc[6]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(m_OutlineDescSet), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &desc[7]
		},
	};

	vt->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);
	
	vt->EndCommandBuffer(Unwrap(cmd));
}

VulkanDebugManager::~VulkanDebugManager()
{
	VkDevice dev = m_Device;
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkResult vkr = VK_SUCCESS;

	// since we don't have properly registered resources, releasing our descriptor
	// pool here won't remove the descriptor sets, so we need to free our own
	// tracking data (not the API objects) for descriptor sets.

	for(auto it=m_CachedMeshPipelines.begin(); it != m_CachedMeshPipelines.end(); ++it)
	{
		for(uint32_t i=0; i < MeshDisplayPipelines::ePipe_Count; i++)
		{
			if(it->second.pipes[i] == VK_NULL_HANDLE) continue;

			vt->DestroyPipeline(Unwrap(dev), Unwrap(it->second.pipes[i]));
			GetResourceManager()->ReleaseWrappedResource(it->second.pipes[i]);
		}
	}

	for(size_t i=0; i < ARRAY_COUNT(m_MeshModules); i++)
	{
		vt->DestroyShader(Unwrap(dev), Unwrap(m_MeshShaders[i]));
		GetResourceManager()->ReleaseWrappedResource(m_MeshShaders[i]);

		vt->DestroyShaderModule(Unwrap(dev), Unwrap(m_MeshModules[i]));
		GetResourceManager()->ReleaseWrappedResource(m_MeshModules[i]);
	}
	
	GetResourceManager()->ReleaseWrappedResource(m_CheckerboardDescSet);
	GetResourceManager()->ReleaseWrappedResource(m_GenericDescSet);
	GetResourceManager()->ReleaseWrappedResource(m_TextDescSet);
	GetResourceManager()->ReleaseWrappedResource(m_MeshDescSet);
	GetResourceManager()->ReleaseWrappedResource(m_OutlineDescSet);

	
	for(size_t i=0; i < ARRAY_COUNT(m_HistogramDescSet); i++)
		GetResourceManager()->ReleaseWrappedResource(m_HistogramDescSet[i]);

	for(size_t i=0; i < ARRAY_COUNT(m_TexDisplayDescSet); i++)
		GetResourceManager()->ReleaseWrappedResource(m_TexDisplayDescSet[i]);

	if(m_DescriptorPool != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorPool(Unwrap(dev), Unwrap(m_DescriptorPool));
		GetResourceManager()->ReleaseWrappedResource(m_DescriptorPool);
	}

	if(m_LinearSampler != VK_NULL_HANDLE)
	{
		vt->DestroySampler(Unwrap(dev), Unwrap(m_LinearSampler));
		GetResourceManager()->ReleaseWrappedResource(m_LinearSampler);
	}

	if(m_PointSampler != VK_NULL_HANDLE)
	{
		vt->DestroySampler(Unwrap(dev), Unwrap(m_PointSampler));
		GetResourceManager()->ReleaseWrappedResource(m_PointSampler);
	}

	if(m_CheckerboardDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_CheckerboardDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_CheckerboardDescSetLayout);
	}

	if(m_CheckerboardPipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_CheckerboardPipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_CheckerboardPipeLayout);
	}

	if(m_CheckerboardPipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_CheckerboardPipeline));
		GetResourceManager()->ReleaseWrappedResource(m_CheckerboardPipeline);
	}

	if(m_CheckerboardMSAAPipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_CheckerboardMSAAPipeline));
		GetResourceManager()->ReleaseWrappedResource(m_CheckerboardMSAAPipeline);
	}

	if(m_TexDisplayDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_TexDisplayDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_TexDisplayDescSetLayout);
	}

	if(m_TexDisplayPipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_TexDisplayPipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_TexDisplayPipeLayout);
	}

	if(m_TexDisplayPipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_TexDisplayPipeline));
		GetResourceManager()->ReleaseWrappedResource(m_TexDisplayPipeline);
	}

	if(m_TexDisplayBlendPipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_TexDisplayBlendPipeline));
		GetResourceManager()->ReleaseWrappedResource(m_TexDisplayBlendPipeline);
	}

	if(m_TexDisplayF32Pipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_TexDisplayF32Pipeline));
		GetResourceManager()->ReleaseWrappedResource(m_TexDisplayF32Pipeline);
	}

	m_CheckerboardUBO.Destroy(vt, dev);
	m_TexDisplayUBO.Destroy(vt, dev);

	m_PickPixelReadbackBuffer.Destroy(vt, dev);

	if(m_PickPixelFB != VK_NULL_HANDLE)
	{
		vt->DestroyFramebuffer(Unwrap(dev), Unwrap(m_PickPixelFB));
		GetResourceManager()->ReleaseWrappedResource(m_PickPixelFB);
	}

	if(m_PickPixelRP != VK_NULL_HANDLE)
	{
		vt->DestroyRenderPass(Unwrap(dev), Unwrap(m_PickPixelRP));
		GetResourceManager()->ReleaseWrappedResource(m_PickPixelRP);
	}

	if(m_PickPixelImageView != VK_NULL_HANDLE)
	{
		vt->DestroyImageView(Unwrap(dev), Unwrap(m_PickPixelImageView));
		GetResourceManager()->ReleaseWrappedResource(m_PickPixelImageView);
	}

	if(m_PickPixelImage != VK_NULL_HANDLE)
	{
		vt->DestroyImage(Unwrap(dev), Unwrap(m_PickPixelImage));
		GetResourceManager()->ReleaseWrappedResource(m_PickPixelImage);
	}

	if(m_PickPixelImageMem != VK_NULL_HANDLE)
	{
		vt->FreeMemory(Unwrap(dev), Unwrap(m_PickPixelImageMem));
		GetResourceManager()->ReleaseWrappedResource(m_PickPixelImageMem);
	}

	if(m_TextDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_TextDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_TextDescSetLayout);
	}

	if(m_TextPipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_TextPipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_TextPipeLayout);
	}

	if(m_TextPipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_TextPipeline));
		GetResourceManager()->ReleaseWrappedResource(m_TextPipeline);
	}

	m_TextGeneralUBO.Destroy(vt, dev);
	m_TextGlyphUBO.Destroy(vt, dev);
	m_TextStringUBO.Destroy(vt, dev);

	if(m_TextAtlasView != VK_NULL_HANDLE)
	{
		vt->DestroyImageView(Unwrap(dev), Unwrap(m_TextAtlasView));
		GetResourceManager()->ReleaseWrappedResource(m_TextAtlasView);
	}

	if(m_TextAtlas != VK_NULL_HANDLE)
	{
		vt->DestroyImage(Unwrap(dev), Unwrap(m_TextAtlas));
		GetResourceManager()->ReleaseWrappedResource(m_TextAtlas);
	}

	if(m_TextAtlasMem != VK_NULL_HANDLE)
	{
		vt->FreeMemory(Unwrap(dev), Unwrap(m_TextAtlasMem));
		GetResourceManager()->ReleaseWrappedResource(m_TextAtlasMem);
	}

	if(m_MeshDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_MeshDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_MeshDescSetLayout);
	}

	if(m_MeshPipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_MeshPipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_MeshPipeLayout);
	}

	m_MeshUBO.Destroy(vt, dev);
	m_MeshBBoxVB.Destroy(vt, dev);
	m_MeshAxisFrustumVB.Destroy(vt, dev);

	if(m_GenericDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_GenericDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_GenericDescSetLayout);
	}

	if(m_GenericPipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_GenericPipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_GenericPipeLayout);
	}

	if(m_HighlightBoxPipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_HighlightBoxPipeline));
		GetResourceManager()->ReleaseWrappedResource(m_HighlightBoxPipeline);
	}

	m_OutlineStripVBO.Destroy(vt, dev);
	m_GenericUBO.Destroy(vt, dev);

	if(m_OutlineDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_OutlineDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_OutlineDescSetLayout);
	}

	if(m_OutlinePipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_OutlinePipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_OutlinePipeLayout);
	}

	if(m_OutlinePipeline != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_OutlinePipeline));
		GetResourceManager()->ReleaseWrappedResource(m_OutlinePipeline);
	}

	m_OutlineUBO.Destroy(vt, dev);
	
	if(m_HistogramDescSetLayout != VK_NULL_HANDLE)
	{
		vt->DestroyDescriptorSetLayout(Unwrap(dev), Unwrap(m_HistogramDescSetLayout));
		GetResourceManager()->ReleaseWrappedResource(m_HistogramDescSetLayout);
	}

	if(m_HistogramPipeLayout != VK_NULL_HANDLE)
	{
		vt->DestroyPipelineLayout(Unwrap(dev), Unwrap(m_HistogramPipeLayout));
		GetResourceManager()->ReleaseWrappedResource(m_HistogramPipeLayout);
	}

	if(m_MinMaxResultPipe != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_MinMaxResultPipe));
		GetResourceManager()->ReleaseWrappedResource(m_MinMaxResultPipe);
	}

	if(m_MinMaxTilePipe != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_MinMaxTilePipe));
		GetResourceManager()->ReleaseWrappedResource(m_MinMaxTilePipe);
	}

	if(m_HistogramPipe != VK_NULL_HANDLE)
	{
		vt->DestroyPipeline(Unwrap(dev), Unwrap(m_HistogramPipe));
		GetResourceManager()->ReleaseWrappedResource(m_HistogramPipe);
	}

	m_MinMaxTileResult.Destroy(vt, dev);
	m_MinMaxResult.Destroy(vt, dev);
	m_MinMaxReadback.Destroy(vt, dev);
	m_HistogramBuf.Destroy(vt, dev);
	m_HistogramReadback.Destroy(vt, dev);
	m_HistogramUBO.Destroy(vt, dev);

	// overlay resources are allocated through driver
	if(m_OverlayNoDepthFB != VK_NULL_HANDLE)
		m_pDriver->vkDestroyFramebuffer(dev, m_OverlayNoDepthFB);

	if(m_OverlayNoDepthRP != VK_NULL_HANDLE)
		m_pDriver->vkDestroyRenderPass(dev, m_OverlayNoDepthRP);
	
	if(m_OverlayImageView != VK_NULL_HANDLE)
		m_pDriver->vkDestroyImageView(dev, m_OverlayImageView);

	if(m_OverlayImage != VK_NULL_HANDLE)
		m_pDriver->vkDestroyImage(dev, m_OverlayImage);

	if(m_OverlayImageMem != VK_NULL_HANDLE)
		m_pDriver->vkFreeMemory(dev, m_OverlayImageMem);
}

void VulkanDebugManager::BeginText(const TextPrintState &textstate)
{
	const VkLayerDispatchTable *vt = ObjDisp(textstate.cmd);
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->BeginCommandBuffer(Unwrap(textstate.cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);
	
	VkClearValue clearval = {0};
	VkRenderPassBeginInfo rpbegin = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		Unwrap(textstate.rp), Unwrap(textstate.fb),
		{ { 0, 0, }, { textstate.w, textstate.h} },
		1, &clearval,
	};
	vt->CmdBeginRenderPass(Unwrap(textstate.cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

	vt->CmdBindPipeline(Unwrap(textstate.cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(m_TextPipeline));

	VkViewport viewport = { 0.0f, 0.0f, (float)textstate.w, (float)textstate.h, 0.0f, 1.0f };
	vt->CmdSetViewport(Unwrap(textstate.cmd), 1, &viewport);
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
	const VkLayerDispatchTable *vt = ObjDisp(textstate.cmd);
	
	VkResult vkr = VK_SUCCESS;

	uint32_t offsets[2] = { 0 };

	fontuniforms *ubo = (fontuniforms *)m_TextGeneralUBO.Map(vt, m_Device, &offsets[0]);

	ubo->TextPosition.x = x;
	ubo->TextPosition.y = y;

	ubo->FontScreenAspect.x = 1.0f/float(textstate.w);
	ubo->FontScreenAspect.y = 1.0f/float(textstate.h);

	ubo->TextSize = m_FontCharSize;
	ubo->FontScreenAspect.x *= m_FontCharAspect;

	ubo->CharacterSize.x = 1.0f/float(FONT_TEX_WIDTH);
	ubo->CharacterSize.y = 1.0f/float(FONT_TEX_HEIGHT);

	m_TextGeneralUBO.Unmap(vt, m_Device);

	// only map enough for our string
	stringdata *stringData = (stringdata *)m_TextStringUBO.Map(vt, m_Device, &offsets[1], strlen(text)*sizeof(Vec4f));

	for(size_t i=0; i < strlen(text); i++)
		stringData->str[i][0] = uint32_t(text[i] - ' ');

	m_TextStringUBO.Unmap(vt, m_Device);
	
	vt->CmdBindDescriptorSets(Unwrap(textstate.cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(m_TextPipeLayout), 0, 1, UnwrapPtr(m_TextDescSet), 2, offsets);

	vt->CmdDraw(Unwrap(textstate.cmd), 4, (uint32_t)strlen(text), 0, 0);
}

void VulkanDebugManager::EndText(const TextPrintState &textstate)
{
	ObjDisp(textstate.cmd)->CmdEndRenderPass(Unwrap(textstate.cmd));
	ObjDisp(textstate.cmd)->EndCommandBuffer(Unwrap(textstate.cmd));
}

void VulkanDebugManager::MakeGraphicsPipelineInfo(VkGraphicsPipelineCreateInfo &pipeCreateInfo, ResourceId pipeline)
{
	VulkanCreationInfo::Pipeline &pipeInfo = m_pDriver->m_CreationInfo.m_Pipeline[pipeline];

	static VkPipelineShaderStageCreateInfo stages[6];

	uint32_t stageCount = 0;

	for(uint32_t i=0; i < 6; i++)
	{
		if(pipeInfo.shaders[i] != ResourceId())
		{
			stages[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stages[stageCount].stage = (VkShaderStage)i;
			stages[stageCount].shader = GetResourceManager()->GetCurrentHandle<VkShader>(pipeInfo.shaders[i]);
			stages[stageCount].pNext = NULL;
			stages[stageCount].pSpecializationInfo = NULL;
			stageCount++;
		}
	}

	static VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	static VkVertexInputAttributeDescription viattr[128] = {0};
	static VkVertexInputBindingDescription vibind[128] = {0};

	vi.pVertexAttributeDescriptions = viattr;
	vi.pVertexBindingDescriptions = vibind;

	vi.attributeCount = (uint32_t)pipeInfo.vertexAttrs.size();
	vi.bindingCount = (uint32_t)pipeInfo.vertexBindings.size();

	for(uint32_t i=0; i < vi.attributeCount; i++)
	{
		viattr[i].binding = pipeInfo.vertexAttrs[i].binding;
		viattr[i].offsetInBytes = pipeInfo.vertexAttrs[i].byteoffset;
		viattr[i].format = pipeInfo.vertexAttrs[i].format;
		viattr[i].location = pipeInfo.vertexAttrs[i].location;
	}

	for(uint32_t i=0; i < vi.bindingCount; i++)
	{
		vibind[i].binding = pipeInfo.vertexBindings[i].vbufferBinding;
		vibind[i].strideInBytes = pipeInfo.vertexBindings[i].bytestride;
		vibind[i].stepRate = pipeInfo.vertexBindings[i].perInstance ? VK_VERTEX_INPUT_STEP_RATE_INSTANCE : VK_VERTEX_INPUT_STEP_RATE_VERTEX;
	}

	RDCASSERT(ARRAY_COUNT(viattr) >= pipeInfo.vertexAttrs.size());
	RDCASSERT(ARRAY_COUNT(vibind) >= pipeInfo.vertexBindings.size());

	static VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

	ia.topology = pipeInfo.topology;
	ia.primitiveRestartEnable = pipeInfo.primitiveRestartEnable;

	static VkPipelineTessellationStateCreateInfo tess = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };

	tess.patchControlPoints = pipeInfo.patchControlPoints;

	static VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

	static VkViewport views[32] = {0};
	static VkRect2D scissors[32] = {0};

	memcpy(views, &pipeInfo.viewports[0], pipeInfo.viewports.size()*sizeof(VkViewport));

	vp.pViewports = &views[0];
	vp.viewportCount = (uint32_t)pipeInfo.viewports.size();

	memcpy(views, &pipeInfo.scissors[0], pipeInfo.scissors.size()*sizeof(VkRect2D));

	vp.pScissors = &scissors[0];
	vp.scissorCount = (uint32_t)pipeInfo.scissors.size();

	RDCASSERT(ARRAY_COUNT(views) >= pipeInfo.viewports.size());
	RDCASSERT(ARRAY_COUNT(scissors) >= pipeInfo.scissors.size());
	
	static VkPipelineRasterStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO };

	rs.depthClipEnable = pipeInfo.depthClipEnable;
	rs.rasterizerDiscardEnable = pipeInfo.rasterizerDiscardEnable,
	rs.fillMode = pipeInfo.fillMode;
	rs.cullMode = pipeInfo.cullMode;
	rs.frontFace = pipeInfo.frontFace;
	rs.depthBiasEnable = pipeInfo.depthBiasEnable;
	rs.depthBias = pipeInfo.depthBias;
	rs.depthBiasClamp = pipeInfo.depthBiasClamp;
	rs.slopeScaledDepthBias = pipeInfo.slopeScaledDepthBias;
	rs.lineWidth = pipeInfo.lineWidth;

	static VkPipelineMultisampleStateCreateInfo msaa = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	
	msaa.rasterSamples = pipeInfo.rasterSamples;
	msaa.sampleShadingEnable = pipeInfo.sampleShadingEnable;
	msaa.minSampleShading = pipeInfo.minSampleShading;
	msaa.pSampleMask = &pipeInfo.sampleMask;

	static VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

	ds.depthTestEnable = pipeInfo.depthTestEnable;
	ds.depthWriteEnable = pipeInfo.depthWriteEnable;
	ds.depthCompareOp = pipeInfo.depthCompareOp;
	ds.depthBoundsTestEnable = pipeInfo.depthBoundsEnable;
	ds.stencilTestEnable = pipeInfo.stencilTestEnable;
	ds.front = pipeInfo.front; ds.back = pipeInfo.back;
	ds.minDepthBounds = pipeInfo.minDepthBounds;
	ds.maxDepthBounds = pipeInfo.maxDepthBounds;

	static VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };

	cb.alphaToCoverageEnable = pipeInfo.alphaToCoverageEnable;
	cb.alphaToOneEnable = pipeInfo.alphaToOneEnable;
	cb.logicOpEnable = pipeInfo.logicOpEnable;
	cb.logicOp = pipeInfo.logicOp;
	memcpy(cb.blendConst, pipeInfo.blendConst, sizeof(cb.blendConst));

	static VkPipelineColorBlendAttachmentState atts[32] = { 0 };

	cb.attachmentCount = (uint32_t)pipeInfo.attachments.size();
	cb.pAttachments = atts;

	for(uint32_t i=0; i < cb.attachmentCount; i++)
	{
		atts[i].blendEnable = pipeInfo.attachments[i].blendEnable;
		atts[i].channelWriteMask = pipeInfo.attachments[i].channelWriteMask;
		atts[i].blendOpAlpha = pipeInfo.attachments[i].alphaBlend.Operation;
		atts[i].srcBlendAlpha = pipeInfo.attachments[i].alphaBlend.Source;
		atts[i].destBlendAlpha = pipeInfo.attachments[i].alphaBlend.Destination;
		atts[i].blendOpColor = pipeInfo.attachments[i].blend.Operation;
		atts[i].srcBlendColor = pipeInfo.attachments[i].blend.Source;
		atts[i].destBlendColor = pipeInfo.attachments[i].blend.Destination;
	}

	RDCASSERT(ARRAY_COUNT(atts) >= pipeInfo.attachments.size());

	static VkDynamicState dynSt[VK_DYNAMIC_STATE_NUM];

	static VkPipelineDynamicStateCreateInfo dyn = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

	dyn.dynamicStateCount = 0;
	dyn.pDynamicStates = dynSt;

	for(uint32_t i=0; i < VK_DYNAMIC_STATE_NUM; i++)
		if(pipeInfo.dynamicStates[i])
			dynSt[dyn.dynamicStateCount++] = (VkDynamicState)i;

	// since we don't have to worry about threading, we point everything at the above static structs
	
	VkGraphicsPipelineCreateInfo ret = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL,
		stageCount, stages,
		&vi,
		&ia,
		&tess,
		&vp,
		&rs,
		&msaa,
		&ds,
		&cb,
		&dyn,
		pipeInfo.flags,
		GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeInfo.layout),
		GetResourceManager()->GetCurrentHandle<VkRenderPass>(pipeInfo.renderpass),
		pipeInfo.subpass,
		VK_NULL_HANDLE, // base pipeline handle
		0, // base pipeline index
	};

	pipeCreateInfo = ret;
}

void VulkanDebugManager::PatchFixedColShader(VkShaderModule &mod, VkShader &shad, float col[4])
{
	string fixedcol = GetEmbeddedResource(fixedcolfs_spv);

	union
	{
		char *str;
		uint32_t *spirv;
		float *data;
	} alias;

	alias.str = &fixedcol[0];

	uint32_t *spirv = alias.spirv;
	size_t spirvLength = fixedcol.size()/sizeof(uint32_t);

	size_t it = 5;
	while(it < spirvLength)
	{
		uint16_t WordCount = spirv[it]>>16;
		spv::Op opcode = spv::Op(spirv[it]&0xffff);

		if(opcode == spv::OpConstant)
		{
			     if(alias.data[it+3] == 1.1f) alias.data[it+3] = col[0];
			else if(alias.data[it+3] == 2.2f) alias.data[it+3] = col[1];
			else if(alias.data[it+3] == 3.3f) alias.data[it+3] = col[2];
			else if(alias.data[it+3] == 4.4f) alias.data[it+3] = col[3];
			else                              RDCERR("Unexpected constant value");
		}

		it += WordCount;
	}
	
	VkShaderModuleCreateInfo modinfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,
		fixedcol.size(), (void *)spirv, 0,
	};

	VkResult vkr = m_pDriver->vkCreateShaderModule(m_Device, &modinfo, &mod);
	RDCASSERT(vkr == VK_SUCCESS);

	VkShaderCreateInfo shadinfo = {
		VK_STRUCTURE_TYPE_SHADER_CREATE_INFO, NULL,
		mod, "main", 0,
		VK_SHADER_STAGE_FRAGMENT,
	};

	vkr = m_pDriver->vkCreateShader(m_Device, &shadinfo, &shad);
	RDCASSERT(vkr == VK_SUCCESS);
}

ResourceId VulkanDebugManager::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	const VkLayerDispatchTable *vt = ObjDisp(m_Device);

	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
	
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	// if the overlay image is the wrong size, free it
	if(m_OverlayImage != VK_NULL_HANDLE && (iminfo.extent.width != m_OverlayDim.width || iminfo.extent.height != m_OverlayDim.height))
	{
		m_pDriver->vkDestroyRenderPass(Unwrap(m_Device), Unwrap(m_OverlayNoDepthRP));
		m_pDriver->vkDestroyFramebuffer(Unwrap(m_Device), Unwrap(m_OverlayNoDepthFB));
		m_pDriver->vkDestroyImageView(Unwrap(m_Device), Unwrap(m_OverlayImageView));
		m_pDriver->vkDestroyImage(m_Device, m_OverlayImage);

		m_OverlayImage = VK_NULL_HANDLE;
		m_OverlayImageView = VK_NULL_HANDLE;
		m_OverlayNoDepthRP = VK_NULL_HANDLE;
		m_OverlayNoDepthFB = VK_NULL_HANDLE;
	}

	// create the overlay image if we don't have one already
	// we go through the driver's creation functions so creation info
	// is saved and the resources are registered as live resources for
	// their IDs.
	if(m_OverlayImage == VK_NULL_HANDLE)
	{
		m_OverlayDim.width = iminfo.extent.width;
		m_OverlayDim.height = iminfo.extent.height;

		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT,
			{ m_OverlayDim.width, m_OverlayDim.height, 1 }, 1, 1, 1,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT,
			0,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		vkr = m_pDriver->vkCreateImage(m_Device, &imInfo, &m_OverlayImage);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryRequirements mrq;
		vkr = m_pDriver->vkGetImageMemoryRequirements(m_Device, m_OverlayImage, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		// if no memory is allocated, or it's not enough,
		// then allocate
		if(m_OverlayImageMem == VK_NULL_HANDLE || mrq.size > m_OverlayMemSize)
		{
			if(m_OverlayImageMem != VK_NULL_HANDLE)
			{
				m_pDriver->vkFreeMemory(m_Device, m_OverlayImageMem);
			}

			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				mrq.size, m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = m_pDriver->vkAllocMemory(m_Device, &allocInfo, &m_OverlayImageMem);
			RDCASSERT(vkr == VK_SUCCESS);

			m_OverlayMemSize = mrq.size;
		}

		vkr = m_pDriver->vkBindImageMemory(m_Device, m_OverlayImage, m_OverlayImageMem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			m_OverlayImage, VK_IMAGE_VIEW_TYPE_2D,
			imInfo.format,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, },
			0,
		};

		vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, &m_OverlayImageView);
		RDCASSERT(vkr == VK_SUCCESS);

		// need to transition image into valid state

		VkImageMemoryBarrier trans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			Unwrap(m_OverlayImage),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		m_pDriver->m_ImageLayouts[GetResID(m_OverlayImage)].subresourceStates[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		void *barrier = (void *)&trans;

		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
		
		VkAttachmentDescription colDesc = {
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
			imInfo.format, 1,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkAttachmentReference colRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription sub = {
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION, NULL,
			VK_PIPELINE_BIND_POINT_GRAPHICS, 0,
			0, NULL, // inputs
			1, &colRef, // color
			NULL, // resolve
			{ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED }, // depth-stencil
			0, NULL, // preserve
		};

		VkRenderPassCreateInfo rpinfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL,
				1, &colDesc,
				1, &sub,
				0, NULL, // dependencies
		};

		vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, &m_OverlayNoDepthRP);
		RDCASSERT(vkr == VK_SUCCESS);

		// Create framebuffer rendering just to overlay image, no depth
		VkFramebufferCreateInfo fbinfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
			m_OverlayNoDepthRP,
			1, &m_OverlayImageView,
			(uint32_t)m_OverlayDim.width, (uint32_t)m_OverlayDim.height, 1,
		};

		vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, &m_OverlayNoDepthFB);
		RDCASSERT(vkr == VK_SUCCESS);

		// can't create a framebuffer or renderpass for overlay image + depth as that
		// needs to match the depth texture type wherever our draw is.
	}
	
	VkImageSubresourceRange subresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
	};
	
	if(!m_pDriver->m_PartialReplayData.renderPassActive)
	{
		// don't do anything, no drawcall capable of making overlays selected
		float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)black, 1, &subresourceRange);
	}
	else if(overlay == eTexOverlay_NaN || overlay == eTexOverlay_Clipping)
	{
		float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)black, 1, &subresourceRange);
	}
	else if(overlay == eTexOverlay_Drawcall || overlay == eTexOverlay_Wireframe)
	{
		float highlightCol[] = { 0.8f, 0.1f, 0.8f, 0.0f };

		if(overlay == eTexOverlay_Wireframe)
		{
			highlightCol[0] = 200/255.0f;
			highlightCol[1] = 1.0f;
			highlightCol[2] = 0.0f;
		}

		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)highlightCol, 1, &subresourceRange);

		highlightCol[3] = 1.0f;
		
		// backup state
		WrappedVulkan::PartialReplayData::StateVector prevstate = m_pDriver->m_PartialReplayData.state;
		
		// make patched shader
		VkShaderModule mod = VK_NULL_HANDLE;
		VkShader shad = VK_NULL_HANDLE;

		PatchFixedColShader(mod, shad, highlightCol);

		// make patched pipeline
		VkGraphicsPipelineCreateInfo pipeCreateInfo;

		MakeGraphicsPipelineInfo(pipeCreateInfo, prevstate.graphics.pipeline);

		// disable all tests possible
		VkPipelineDepthStencilStateCreateInfo *ds = (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
		ds->depthTestEnable = false;
		ds->depthWriteEnable = false;
		ds->stencilTestEnable = false;
		ds->depthBoundsTestEnable = false;

		VkPipelineRasterStateCreateInfo *rs = (VkPipelineRasterStateCreateInfo *)pipeCreateInfo.pRasterState;
		rs->cullMode = VK_CULL_MODE_NONE;
		rs->rasterizerDiscardEnable = false;
		rs->depthClipEnable = false;
		
		if(overlay == eTexOverlay_Wireframe && m_pDriver->GetDeviceFeatures().fillModeNonSolid)
		{
			rs->fillMode = VK_FILL_MODE_WIREFRAME;
			rs->lineWidth = 1.0f;
		}

		VkPipelineColorBlendStateCreateInfo *cb = (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
		cb->logicOpEnable = false;
		cb->attachmentCount = 1; // only one colour attachment
		for(uint32_t i=0; i < cb->attachmentCount; i++)
		{
			VkPipelineColorBlendAttachmentState *att = (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
			att->blendEnable = false;
			att->channelWriteMask = 0xf;
		}

		// set scissors to max
		for(size_t i=0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
		{
			VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
			sc.offset.x = 0;
			sc.offset.y = 0;
			sc.extent.width = 4096;
			sc.extent.height = 4096;
		}

		// set our renderpass and shader
		pipeCreateInfo.renderPass = m_OverlayNoDepthRP;

		bool found = false;
		for(uint32_t i=0; i < pipeCreateInfo.stageCount; i++)
		{
			VkPipelineShaderStageCreateInfo &sh = (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
			if(sh.stage == VK_SHADER_STAGE_FRAGMENT)
			{
				sh.shader = shad;
				found = true;
				break;
			}
		}
		
		if(!found)
		{
			// we know this is safe because it's pointing to a static array that's
			// big enough for all shaders
			
			VkPipelineShaderStageCreateInfo &sh = (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
			sh.pNext = NULL;
			sh.pSpecializationInfo = NULL;
			sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			sh.shader = shad;
			sh.stage = VK_SHADER_STAGE_FRAGMENT;
		}

		vkr = vt->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		VkPipeline pipe = VK_NULL_HANDLE;
		
		vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, &pipe);
		RDCASSERT(vkr == VK_SUCCESS);

		// modify state
		m_pDriver->m_PartialReplayData.state.renderPass = GetResID(m_OverlayNoDepthRP);
		m_pDriver->m_PartialReplayData.state.subpass = 0;
		m_pDriver->m_PartialReplayData.state.framebuffer = GetResID(m_OverlayNoDepthFB);

		m_pDriver->m_PartialReplayData.state.graphics.pipeline = GetResID(pipe);

		// set dynamic scissors in case pipeline was using them
		for(size_t i=0; i < m_pDriver->m_PartialReplayData.state.scissors.size(); i++)
		{
			m_pDriver->m_PartialReplayData.state.scissors[i].offset.x = 0;
			m_pDriver->m_PartialReplayData.state.scissors[i].offset.x = 0;
			m_pDriver->m_PartialReplayData.state.scissors[i].extent.width = 4096;
			m_pDriver->m_PartialReplayData.state.scissors[i].extent.height = 4096;
		}

		if(overlay == eTexOverlay_Wireframe)
			m_pDriver->m_PartialReplayData.state.lineWidth = 1.0f;

		m_pDriver->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		// submit & flush so that we don't have to keep pipeline around for a while
		m_pDriver->SubmitCmds();
		m_pDriver->FlushQ();

		cmd = m_pDriver->GetNextCmd();

		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		// restore state
		m_pDriver->m_PartialReplayData.state = prevstate;

		m_pDriver->vkDestroyPipeline(m_Device, pipe);
		m_pDriver->vkDestroyShaderModule(m_Device, mod);
		m_pDriver->vkDestroyShader(m_Device, shad);
	}
	else if(overlay == eTexOverlay_ViewportScissor)
	{
		// clear the whole image to opaque black. We'll overwite the render area with transparent black
		// before rendering the viewport/scissors
		float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)black, 1, &subresourceRange);
		
		black[3] = 0.0f;

		{
			VkClearValue clearval = {0};
			VkRenderPassBeginInfo rpbegin = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
				Unwrap(m_OverlayNoDepthRP), Unwrap(m_OverlayNoDepthFB),
				m_pDriver->m_PartialReplayData.state.renderArea,
				1, &clearval,
			};
			vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

			VkRect3D rect = {
				{
					m_pDriver->m_PartialReplayData.state.renderArea.offset.x,
					m_pDriver->m_PartialReplayData.state.renderArea.offset.y,
					0,
				},
				{
					m_pDriver->m_PartialReplayData.state.renderArea.extent.width,
					m_pDriver->m_PartialReplayData.state.renderArea.extent.height,
					1,
				},
			};
			vt->CmdClearColorAttachment(Unwrap(cmd), 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)black, 1, &rect);
			
			VkViewport viewport = m_pDriver->m_PartialReplayData.state.views[0];
			vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);

			uint32_t uboOffs = 0;

			outlineuniforms *ubo = (outlineuniforms *)m_OutlineUBO.Map(vt, m_Device, &uboOffs);

			ubo->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
			ubo->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
			ubo->Scissor = 0;
			ubo->ViewRect = (Vec4f &)viewport;
			
			m_OutlineUBO.Unmap(vt, m_Device);
				
			vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(m_OutlinePipeline));
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(m_OutlinePipeLayout),
																0, 1, UnwrapPtr(m_OutlineDescSet), 1, &uboOffs);

			vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

			if(!m_pDriver->m_PartialReplayData.state.scissors.empty())
			{
				Vec4f scissor((float)m_pDriver->m_PartialReplayData.state.scissors[0].offset.x,
				              (float)m_pDriver->m_PartialReplayData.state.scissors[0].offset.y,
											(float)m_pDriver->m_PartialReplayData.state.scissors[0].extent.width,
											(float)m_pDriver->m_PartialReplayData.state.scissors[0].extent.height);

				outlineuniforms *ubo = (outlineuniforms *)m_OutlineUBO.Map(vt, m_Device, &uboOffs);

				ubo->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
				ubo->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
				ubo->Scissor = 1;
				ubo->ViewRect = scissor;

				m_OutlineUBO.Unmap(vt, m_Device);

				viewport.originX = scissor.x;
				viewport.originY = scissor.y;
				viewport.width   = scissor.z;
				viewport.height  = scissor.w;
				
				vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);
				vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(m_OutlinePipeLayout),
																	0, 1, UnwrapPtr(m_OutlineDescSet), 1, &uboOffs);
				
				vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
			}

			vt->CmdEndRenderPass(Unwrap(cmd));
		}

	}
	else if(overlay == eTexOverlay_BackfaceCull)
	{
		float highlightCol[] = { 0.0f, 0.0f, 0.0f, 0.0f };

		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)highlightCol, 1, &subresourceRange);

		highlightCol[0] = 1.0f;
		highlightCol[3] = 1.0f;
		
		// backup state
		WrappedVulkan::PartialReplayData::StateVector prevstate = m_pDriver->m_PartialReplayData.state;
		
		// make patched shader
		VkShaderModule mod[2] = {0};
		VkShader shad[2] = {0};
		VkPipeline pipe[2] = {0};

		// first shader, no culling, writes red
		PatchFixedColShader(mod[0], shad[0], highlightCol);

		highlightCol[0] = 0.0f;
		highlightCol[1] = 1.0f;

		// second shader, normal culling, writes green
		PatchFixedColShader(mod[1], shad[1], highlightCol);

		// make patched pipeline
		VkGraphicsPipelineCreateInfo pipeCreateInfo;

		MakeGraphicsPipelineInfo(pipeCreateInfo, prevstate.graphics.pipeline);

		// disable all tests possible
		VkPipelineDepthStencilStateCreateInfo *ds = (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
		ds->depthTestEnable = false;
		ds->depthWriteEnable = false;
		ds->stencilTestEnable = false;
		ds->depthBoundsTestEnable = false;

		VkPipelineRasterStateCreateInfo *rs = (VkPipelineRasterStateCreateInfo *)pipeCreateInfo.pRasterState;
		VkCullMode origCullMode = rs->cullMode;
		rs->cullMode = VK_CULL_MODE_NONE; // first render without any culling
		rs->rasterizerDiscardEnable = false;
		rs->depthClipEnable = false;

		VkPipelineColorBlendStateCreateInfo *cb = (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
		cb->logicOpEnable = false;
		cb->attachmentCount = 1; // only one colour attachment
		for(uint32_t i=0; i < cb->attachmentCount; i++)
		{
			VkPipelineColorBlendAttachmentState *att = (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
			att->blendEnable = false;
			att->channelWriteMask = 0xf;
		}

		// set scissors to max
		for(size_t i=0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
		{
			VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
			sc.offset.x = 0;
			sc.offset.y = 0;
			sc.extent.width = 4096;
			sc.extent.height = 4096;
		}

		// set our renderpass and shader
		pipeCreateInfo.renderPass = m_OverlayNoDepthRP;

		VkPipelineShaderStageCreateInfo *fragShader = NULL;

		for(uint32_t i=0; i < pipeCreateInfo.stageCount; i++)
		{
			VkPipelineShaderStageCreateInfo &sh = (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
			if(sh.stage == VK_SHADER_STAGE_FRAGMENT)
			{
				sh.shader = shad[0];
				fragShader = &sh;
				break;
			}
		}
		
		if(fragShader == NULL)
		{
			// we know this is safe because it's pointing to a static array that's
			// big enough for all shaders
			
			VkPipelineShaderStageCreateInfo &sh = (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
			sh.pNext = NULL;
			sh.pSpecializationInfo = NULL;
			sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			sh.shader = shad[0];
			sh.stage = VK_SHADER_STAGE_FRAGMENT;

			fragShader = &sh;
		}

		vkr = vt->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);
		
		vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, &pipe[0]);
		RDCASSERT(vkr == VK_SUCCESS);
		
		fragShader->shader = shad[1];
		rs->cullMode = origCullMode;
		
		vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, &pipe[1]);
		RDCASSERT(vkr == VK_SUCCESS);

		// modify state
		m_pDriver->m_PartialReplayData.state.renderPass = GetResID(m_OverlayNoDepthRP);
		m_pDriver->m_PartialReplayData.state.subpass = 0;
		m_pDriver->m_PartialReplayData.state.framebuffer = GetResID(m_OverlayNoDepthFB);

		m_pDriver->m_PartialReplayData.state.graphics.pipeline = GetResID(pipe[0]);

		// set dynamic scissors in case pipeline was using them
		for(size_t i=0; i < m_pDriver->m_PartialReplayData.state.scissors.size(); i++)
		{
			m_pDriver->m_PartialReplayData.state.scissors[i].offset.x = 0;
			m_pDriver->m_PartialReplayData.state.scissors[i].offset.x = 0;
			m_pDriver->m_PartialReplayData.state.scissors[i].extent.width = 4096;
			m_pDriver->m_PartialReplayData.state.scissors[i].extent.height = 4096;
		}

		m_pDriver->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		m_pDriver->m_PartialReplayData.state.graphics.pipeline = GetResID(pipe[1]);

		m_pDriver->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		// submit & flush so that we don't have to keep pipeline around for a while
		m_pDriver->SubmitCmds();
		m_pDriver->FlushQ();

		cmd = m_pDriver->GetNextCmd();

		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		// restore state
		m_pDriver->m_PartialReplayData.state = prevstate;

		for(int i=0; i < 2; i++)
		{
			m_pDriver->vkDestroyPipeline(m_Device, pipe[i]);
			m_pDriver->vkDestroyShaderModule(m_Device, mod[i]);
			m_pDriver->vkDestroyShader(m_Device, shad[i]);
		}
	}
	else if(overlay == eTexOverlay_Depth || overlay == eTexOverlay_Stencil)
	{
		float highlightCol[] = { 0.0f, 0.0f, 0.0f, 0.0f };

		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(m_OverlayImage), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)highlightCol, 1, &subresourceRange);

		VkFramebuffer depthFB;
		VkRenderPass depthRP;

		const WrappedVulkan::PartialReplayData::StateVector &state = m_pDriver->m_PartialReplayData.state;
		VulkanCreationInfo &createinfo = m_pDriver->m_CreationInfo;

		RDCASSERT(state.subpass < createinfo.m_RenderPass[state.renderPass].subpasses.size());
		int32_t dsIdx = createinfo.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;


		// make a renderpass and framebuffer for rendering to overlay color and using
		// depth buffer from the orignial render
		if(dsIdx >= 0 && dsIdx < (int32_t)createinfo.m_Framebuffer[state.framebuffer].attachments.size())
		{
			VkAttachmentDescription attDescs[] = {
				{
					VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
					VK_FORMAT_R16G16B16A16_SFLOAT, 1,
					VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				},
				{
					VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
					VK_FORMAT_UNDEFINED, 1, // will patch this just below
					VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
					VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
				},
			};

			ResourceId depthView = createinfo.m_Framebuffer[state.framebuffer].attachments[dsIdx].view;
			ResourceId depthIm = createinfo.m_ImageView[depthView].image;

			attDescs[1].format = createinfo.m_Image[depthIm].format;

			VkAttachmentReference colRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription sub = {
				VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION, NULL,
				VK_PIPELINE_BIND_POINT_GRAPHICS, 0,
				0, NULL, // inputs
				1, &colRef, // color
				NULL, // resolve
				{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, // depth-stencil
				0, NULL, // preserve
			};

			VkRenderPassCreateInfo rpinfo = {
					VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL,
					2, attDescs,
					1, &sub,
					0, NULL, // dependencies
			};

			vkr = m_pDriver->vkCreateRenderPass(m_Device, &rpinfo, &depthRP);
			RDCASSERT(vkr == VK_SUCCESS);

			VkImageView views[] = {
				m_OverlayImageView,
				GetResourceManager()->GetCurrentHandle<VkImageView>(depthView),
			};

			// Create framebuffer rendering just to overlay image, no depth
			VkFramebufferCreateInfo fbinfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
				depthRP,
				2, views,
				(uint32_t)m_OverlayDim.width, (uint32_t)m_OverlayDim.height, 1,
			};

			vkr = m_pDriver->vkCreateFramebuffer(m_Device, &fbinfo, &depthFB);
			RDCASSERT(vkr == VK_SUCCESS);
		}

		// if depthRP is NULL, so is depthFB, and it means no depth buffer was
		// bound, so we just render green.

		highlightCol[0] = 1.0f;
		highlightCol[3] = 1.0f;
		
		// backup state
		WrappedVulkan::PartialReplayData::StateVector prevstate = m_pDriver->m_PartialReplayData.state;
		
		// make patched shader
		VkShaderModule mod[2] = {0};
		VkShader shad[2] = {0};
		VkPipeline pipe[2] = {0};

		// first shader, no depth testing, writes red
		PatchFixedColShader(mod[0], shad[0], highlightCol);

		highlightCol[0] = 0.0f;
		highlightCol[1] = 1.0f;

		// second shader, enabled depth testing, writes green
		PatchFixedColShader(mod[1], shad[1], highlightCol);

		// make patched pipeline
		VkGraphicsPipelineCreateInfo pipeCreateInfo;

		MakeGraphicsPipelineInfo(pipeCreateInfo, prevstate.graphics.pipeline);

		// disable all tests possible
		VkPipelineDepthStencilStateCreateInfo *ds = (VkPipelineDepthStencilStateCreateInfo *)pipeCreateInfo.pDepthStencilState;
		VkBool32 origDepthTest = ds->depthTestEnable;
		ds->depthTestEnable = false;
		ds->depthWriteEnable = false;
		VkBool32 origStencilTest = ds->stencilTestEnable;
		ds->stencilTestEnable = false;
		ds->depthBoundsTestEnable = false;

		VkPipelineRasterStateCreateInfo *rs = (VkPipelineRasterStateCreateInfo *)pipeCreateInfo.pRasterState;
		rs->cullMode = VK_CULL_MODE_NONE;
		rs->rasterizerDiscardEnable = false;
		rs->depthClipEnable = false;

		VkPipelineColorBlendStateCreateInfo *cb = (VkPipelineColorBlendStateCreateInfo *)pipeCreateInfo.pColorBlendState;
		cb->logicOpEnable = false;
		cb->attachmentCount = 1; // only one colour attachment
		for(uint32_t i=0; i < cb->attachmentCount; i++)
		{
			VkPipelineColorBlendAttachmentState *att = (VkPipelineColorBlendAttachmentState *)&cb->pAttachments[i];
			att->blendEnable = false;
			att->channelWriteMask = 0xf;
		}

		// set scissors to max
		for(size_t i=0; i < pipeCreateInfo.pViewportState->scissorCount; i++)
		{
			VkRect2D &sc = (VkRect2D &)pipeCreateInfo.pViewportState->pScissors[i];
			sc.offset.x = 0;
			sc.offset.y = 0;
			sc.extent.width = 4096;
			sc.extent.height = 4096;
		}

		// set our renderpass and shader
		pipeCreateInfo.renderPass = m_OverlayNoDepthRP;

		VkPipelineShaderStageCreateInfo *fragShader = NULL;

		for(uint32_t i=0; i < pipeCreateInfo.stageCount; i++)
		{
			VkPipelineShaderStageCreateInfo &sh = (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
			if(sh.stage == VK_SHADER_STAGE_FRAGMENT)
			{
				sh.shader = shad[0];
				fragShader = &sh;
				break;
			}
		}
		
		if(fragShader == NULL)
		{
			// we know this is safe because it's pointing to a static array that's
			// big enough for all shaders
			
			VkPipelineShaderStageCreateInfo &sh = (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[pipeCreateInfo.stageCount++];
			sh.pNext = NULL;
			sh.pSpecializationInfo = NULL;
			sh.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			sh.shader = shad[0];
			sh.stage = VK_SHADER_STAGE_FRAGMENT;

			fragShader = &sh;
		}

		vkr = vt->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);
		
		vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, &pipe[0]);
		RDCASSERT(vkr == VK_SUCCESS);
		
		fragShader->shader = shad[1];

		if(depthRP != VK_NULL_HANDLE)
		{
			if(overlay == eTexOverlay_Depth)
				ds->depthTestEnable = origDepthTest;
			else
				ds->stencilTestEnable = origStencilTest;
			pipeCreateInfo.renderPass = depthRP;
		}
		
		vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, &pipe[1]);
		RDCASSERT(vkr == VK_SUCCESS);

		// modify state
		m_pDriver->m_PartialReplayData.state.renderPass = GetResID(m_OverlayNoDepthRP);
		m_pDriver->m_PartialReplayData.state.subpass = 0;
		m_pDriver->m_PartialReplayData.state.framebuffer = GetResID(m_OverlayNoDepthFB);

		m_pDriver->m_PartialReplayData.state.graphics.pipeline = GetResID(pipe[0]);

		// set dynamic scissors in case pipeline was using them
		for(size_t i=0; i < m_pDriver->m_PartialReplayData.state.scissors.size(); i++)
		{
			m_pDriver->m_PartialReplayData.state.scissors[i].offset.x = 0;
			m_pDriver->m_PartialReplayData.state.scissors[i].offset.x = 0;
			m_pDriver->m_PartialReplayData.state.scissors[i].extent.width = 4096;
			m_pDriver->m_PartialReplayData.state.scissors[i].extent.height = 4096;
		}

		m_pDriver->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		m_pDriver->m_PartialReplayData.state.graphics.pipeline = GetResID(pipe[1]);
		if(depthRP != VK_NULL_HANDLE)
		{
			m_pDriver->m_PartialReplayData.state.renderPass = GetResID(depthRP);
			m_pDriver->m_PartialReplayData.state.framebuffer = GetResID(depthFB);
		}

		m_pDriver->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		// submit & flush so that we don't have to keep pipeline around for a while
		m_pDriver->SubmitCmds();
		m_pDriver->FlushQ();

		cmd = m_pDriver->GetNextCmd();

		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		// restore state
		m_pDriver->m_PartialReplayData.state = prevstate;

		for(int i=0; i < 2; i++)
		{
			m_pDriver->vkDestroyPipeline(m_Device, pipe[i]);
			m_pDriver->vkDestroyShaderModule(m_Device, mod[i]);
			m_pDriver->vkDestroyShader(m_Device, shad[i]);
		}
		
		if(depthRP != VK_NULL_HANDLE)
		{
			m_pDriver->vkDestroyRenderPass(m_Device, depthRP);
			m_pDriver->vkDestroyFramebuffer(m_Device, depthFB);
		}
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	return GetResID(m_OverlayImage);
}

MeshDisplayPipelines VulkanDebugManager::CacheMeshDisplayPipelines(const MeshFormat &primary, const MeshFormat &secondary)
{
	// generate a key to look up the map
	uint64_t key = 0;

	uint64_t bit = 0;

	if(primary.idxByteWidth == 4) key |= 1ULL << bit;
	bit++;

	RDCASSERT((uint32_t)primary.topo < 64);
	key |= ((uint32_t)primary.topo & 0x3f) << bit;
	bit += 6;
	
	ResourceFormat fmt;
	fmt.special = primary.specialFormat != eSpecial_Unknown;
	fmt.specialFormat = primary.specialFormat;
	fmt.compByteWidth = primary.compByteWidth;
	fmt.compCount = primary.compCount;
	fmt.compType = primary.compType;

	VkFormat primaryFmt = MakeVkFormat(fmt);
	
	fmt.special = secondary.specialFormat != eSpecial_Unknown;
	fmt.specialFormat = secondary.specialFormat;
	fmt.compByteWidth = secondary.compByteWidth;
	fmt.compCount = secondary.compCount;
	fmt.compType = secondary.compType;
	
	VkFormat secondaryFmt = secondary.buf == ResourceId() ? VK_FORMAT_UNDEFINED : MakeVkFormat(fmt);
	
	RDCCOMPILE_ASSERT(VK_FORMAT_NUM <= 255, "Mesh pipeline cache key needs an extra bit for format");
	
	key |= ((uint32_t)primaryFmt & 0xff) << bit;
	bit += 8;

	key |= ((uint32_t)secondaryFmt & 0xff) << bit;
	bit += 8;

	RDCASSERT(primary.stride <= 0xffff);
	key |= ((uint32_t)primary.stride & 0xffff) << bit;
	bit += 16;

	if(secondary.buf != ResourceId())
	{
		RDCASSERT(secondary.stride <= 0xffff);
		key |= ((uint32_t)secondary.stride & 0xffff) << bit;
	}
	bit += 16;

	MeshDisplayPipelines &cache = m_CachedMeshPipelines[key];

	if(cache.pipes[eShade_None] != VK_NULL_HANDLE)
		return cache;
	
	const VkLayerDispatchTable *vt = ObjDisp(m_Device);
	VkResult vkr = VK_SUCCESS;

	// should we try and evict old pipelines from the cache here?
	// or just keep them forever

	VkVertexInputBindingDescription binds[] = {
		// primary
		{
			0,
			primary.stride,
			VK_VERTEX_INPUT_STEP_RATE_VERTEX
		},
		// secondary
		{
			1,
			secondary.stride,
			VK_VERTEX_INPUT_STEP_RATE_VERTEX
		}
	};

	RDCASSERT(primaryFmt != VK_FORMAT_UNDEFINED);

	VkVertexInputAttributeDescription vertAttrs[] = {
		// primary
		{
			0,
			0,
			primaryFmt,
			0,
		},
		// secondary
		{
			1,
			0,
			primaryFmt,
			0,
		},
	};

	VkPipelineVertexInputStateCreateInfo vi = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL,
		1, binds,
		2, vertAttrs,
	};
	
	VkPipelineShaderStageCreateInfo stages[3] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_MAX_ENUM, VK_NULL_HANDLE, NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_MAX_ENUM, VK_NULL_HANDLE, NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, VK_SHADER_STAGE_MAX_ENUM, VK_NULL_HANDLE, NULL },
	};

	VkPipelineInputAssemblyStateCreateInfo ia = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL,
		primary.topo >= eTopology_PatchList ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : MakeVkPrimitiveTopology(primary.topo), false,
	};

	VkRect2D scissor = { { 0, 0 }, { 4096, 4096 } };

	VkPipelineViewportStateCreateInfo vp = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL,
		1, NULL,
		1, &scissor
	};

	VkPipelineRasterStateCreateInfo rs = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTER_STATE_CREATE_INFO, NULL,
		true, false, VK_FILL_MODE_SOLID, VK_CULL_MODE_NONE, VK_FRONT_FACE_CW,
		false, 0.0f, 0.0f, 0.0f, 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo msaa = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL,
		4, false, 0.0f, NULL,
	};

	VkPipelineDepthStencilStateCreateInfo ds = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL,
		true, true, VK_COMPARE_OP_LESS_EQUAL, false, false,
		{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
		{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
		0.0f, 1.0f,
	};

	VkPipelineColorBlendAttachmentState attState = {
		false,
		VK_BLEND_ONE, VK_BLEND_ZERO, VK_BLEND_OP_ADD,
		VK_BLEND_ONE, VK_BLEND_ZERO, VK_BLEND_OP_ADD,
		0xf,
	};

	VkPipelineColorBlendStateCreateInfo cb = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL,
		false, false, false, VK_LOGIC_OP_NOOP,
		1, &attState,
		{ 1.0f, 1.0f, 1.0f, 1.0f }
	};

	VkDynamicState dynstates[] = { VK_DYNAMIC_STATE_VIEWPORT };

	VkPipelineDynamicStateCreateInfo dyn = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, NULL,
		ARRAY_COUNT(dynstates), dynstates,
	};
	
	VkRenderPass rp; // compatible render pass

	{
		VkAttachmentDescription attDesc[] = {
			{
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
				VK_FORMAT_R8G8B8A8_SRGB, 4,
				VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			},
			{
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
				VK_FORMAT_D32_SFLOAT, 4,
				VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			},
		};

		VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription sub = {
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION, NULL,
			VK_PIPELINE_BIND_POINT_GRAPHICS, 0,
			0, NULL, // inputs
			1, &attRef, // color
			NULL, // resolve
			{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, // depth-stencil
			0, NULL, // preserve
		};

		VkRenderPassCreateInfo rpinfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL,
				2, attDesc,
				1, &sub,
				0, NULL, // dependencies
		};
		
		vt->CreateRenderPass(Unwrap(m_Device), &rpinfo, &rp);
	}

	VkGraphicsPipelineCreateInfo pipeInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL,
		2, stages,
		&vi,
		&ia,
		NULL, // tess
		&vp,
		&rs,
		&msaa,
		&ds,
		&cb,
		&dyn,
		0, // flags
		Unwrap(m_MeshPipeLayout),
		rp,
		0, // sub pass
		VK_NULL_HANDLE, // base pipeline handle
		0, // base pipeline index
	};
	
	// wireframe pipeline
	stages[0].shader = Unwrap(m_MeshShaders[0]);
	stages[0].stage = VK_SHADER_STAGE_VERTEX;
	stages[1].shader = Unwrap(m_MeshShaders[2]);
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT;

	rs.fillMode = VK_FILL_MODE_WIREFRAME;
	rs.lineWidth = 1.0f;
	ds.depthTestEnable = false;
	
	vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, &cache.pipes[MeshDisplayPipelines::ePipe_Wire]);
	RDCASSERT(vkr == VK_SUCCESS);
	
	ds.depthTestEnable = true;
	
	vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, &cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
	RDCASSERT(vkr == VK_SUCCESS);
	
	// solid shading pipeline
	rs.fillMode = VK_FILL_MODE_SOLID;
	ds.depthTestEnable = false;
	
	vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, &cache.pipes[MeshDisplayPipelines::ePipe_Solid]);
	RDCASSERT(vkr == VK_SUCCESS);
	
	ds.depthTestEnable = true;
	
	vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, &cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]);
	RDCASSERT(vkr == VK_SUCCESS);

	if(secondary.buf != ResourceId())
	{
		// pull secondary information from second vertex buffer
		vertAttrs[1].binding = 1;
		vertAttrs[1].format = secondaryFmt;
		RDCASSERT(secondaryFmt != VK_FORMAT_UNDEFINED);

		vi.bindingCount = 2;

		vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, &cache.pipes[MeshDisplayPipelines::ePipe_Secondary]);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	vertAttrs[1].binding = 0;
	vi.bindingCount = 1;

	// flat lit pipeline, needs geometry shader to calculate face normals
	stages[0].shader = Unwrap(m_MeshShaders[0]);
	stages[0].stage = VK_SHADER_STAGE_VERTEX;
	stages[1].shader = Unwrap(m_MeshShaders[1]);
	stages[1].stage = VK_SHADER_STAGE_GEOMETRY;
	stages[2].shader = Unwrap(m_MeshShaders[2]);
	stages[2].stage = VK_SHADER_STAGE_FRAGMENT;
	pipeInfo.stageCount = 3;

	vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, &cache.pipes[MeshDisplayPipelines::ePipe_Lit]);
	RDCASSERT(vkr == VK_SUCCESS);

	for(uint32_t i=0; i < MeshDisplayPipelines::ePipe_Count; i++)
		if(cache.pipes[i] != VK_NULL_HANDLE)
			GetResourceManager()->WrapResource(Unwrap(m_Device), cache.pipes[i]);

	vt->DestroyRenderPass(Unwrap(m_Device), rp);
	
	return cache;
}
