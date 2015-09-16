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

#include "vk_replay.h"
#include "vk_core.h"
#include "vk_resources.h"

#include "serialise/string_utils.h"

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

VulkanReplay::OutputWindow::OutputWindow() : wnd(NULL_WND_HANDLE), width(0), height(0),
	dsimg(VK_NULL_HANDLE), dsmem(VK_NULL_HANDLE)
{
	swap = VK_NULL_HANDLE;
	for(size_t i=0; i < ARRAY_COUNT(colimg); i++)
		colimg[i] = VK_NULL_HANDLE;

	bb = VK_NULL_HANDLE;
	bbview = VK_NULL_HANDLE;
	fb = VK_NULL_HANDLE;
	fbdepth = VK_NULL_HANDLE;
	renderpass = VK_NULL_HANDLE;
	fullVP = VK_NULL_HANDLE;

	VkImageMemoryBarrier t = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED,
		0, 0, VK_NULL_HANDLE,
		{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 }
	};
	for(size_t i=0; i < ARRAY_COUNT(coltrans); i++)
		coltrans[i] = t;

	bbtrans = t;

	t.subresourceRange.aspect = VK_IMAGE_ASPECT_DEPTH;
	depthtrans = t;

	t.subresourceRange.aspect = VK_IMAGE_ASPECT_STENCIL;
	stenciltrans = t;
}

void VulkanReplay::OutputWindow::SetCol(VkDeviceMemory mem, VkImage img)
{
}

void VulkanReplay::OutputWindow::SetDS(VkDeviceMemory mem, VkImage img)
{
}

void VulkanReplay::OutputWindow::Destroy(WrappedVulkan *driver, VkDevice device)
{
	const VkLayerDispatchTable *vt = device_dispatch_table(device);

	vt->DeviceWaitIdle(device);
	
	if(bb != VK_NULL_HANDLE)
	{
		vt->DestroyRenderPass(device, renderpass);
		renderpass = VK_NULL_HANDLE;

		vt->DestroyDynamicViewportState(device, fullVP);
		fullVP = VK_NULL_HANDLE;
		
		vt->DestroyImage(device, bb);
		vt->DestroyAttachmentView(device, bbview);
		vt->FreeMemory(device, bbmem);
		vt->DestroyFramebuffer(device, fb);
		
		bb = VK_NULL_HANDLE;
		bbview = VK_NULL_HANDLE;
		bbmem = VK_NULL_HANDLE;
		fb = VK_NULL_HANDLE;
	}

	// not owned - freed with the swapchain
	for(size_t i=0; i < ARRAY_COUNT(colimg); i++)
		colimg[i] = VK_NULL_HANDLE;

	if(dsimg != VK_NULL_HANDLE)
	{
		vt->DestroyAttachmentView(device, dsview);
		vt->DestroyImage(device, dsimg);
		vt->FreeMemory(device, dsmem);
		vt->DestroyFramebuffer(device, fbdepth);
		
		dsview = VK_NULL_HANDLE;
		dsimg = VK_NULL_HANDLE;
		dsmem = VK_NULL_HANDLE;
		fbdepth = VK_NULL_HANDLE;
	}

	if(swap != VK_NULL_HANDLE)
		vt->DestroySwapChainWSI(device, swap);
}

void VulkanReplay::OutputWindow::Create(WrappedVulkan *driver, VkDevice device, bool depth)
{
	const VkLayerDispatchTable *vt = device_dispatch_table(device);
	
	// save the old swapchain so it isn't destroyed
	VkSwapChainWSI old = swap;
	swap = VK_NULL_HANDLE;

	Destroy(driver, device);

	void *handleptr = NULL;
	void *wndptr = NULL;
	VkPlatformWSI platform = VK_PLATFORM_MAX_ENUM_WSI;

	VkSurfaceDescriptionWindowWSI surfDesc = { VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_WINDOW_WSI };

	InitSurfaceDescription(surfDesc);

	// sensible defaults
	VkFormat imformat = VK_FORMAT_B8G8R8A8_UNORM;
	VkPresentModeWSI presentmode = VK_PRESENT_MODE_FIFO_WSI;

	VkResult vkr = VK_SUCCESS;

	// check format and present mode from driver
	{
		size_t sz = 0;

		vkr = vt->GetSurfaceInfoWSI(device, (const VkSurfaceDescriptionWSI *)&surfDesc, VK_SURFACE_INFO_TYPE_FORMATS_WSI, &sz, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW make sure whole pipeline is SRGB correct
		if(sz > 0)
		{
			size_t numFormats = sz / sizeof(VkSurfaceFormatPropertiesWSI);
			VkSurfaceFormatPropertiesWSI *formats = new VkSurfaceFormatPropertiesWSI[numFormats];

			vkr = vt->GetSurfaceInfoWSI(device, (const VkSurfaceDescriptionWSI *)&surfDesc, VK_SURFACE_INFO_TYPE_FORMATS_WSI, &sz, formats);
			RDCASSERT(vkr == VK_SUCCESS);

			if(numFormats == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
			{
				// 1 entry with undefined means no preference, just use our default
				imformat = VK_FORMAT_B8G8R8A8_UNORM;
			}
			else
			{
				// since we don't care, if the driver has a preference just pick the first
				imformat = formats[0].format;
			}

			SAFE_DELETE_ARRAY(formats);
		}

		sz = 0;

		vkr = vt->GetSurfaceInfoWSI(device, (const VkSurfaceDescriptionWSI *)&surfDesc, VK_SURFACE_INFO_TYPE_PRESENT_MODES_WSI, &sz, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		if(sz > 0)
		{
			size_t numModes = sz / sizeof(VkSurfacePresentModePropertiesWSI);
			VkSurfacePresentModePropertiesWSI *modes = new VkSurfacePresentModePropertiesWSI[numModes];

			vkr = vt->GetSurfaceInfoWSI(device, (const VkSurfaceDescriptionWSI *)&surfDesc, VK_SURFACE_INFO_TYPE_PRESENT_MODES_WSI, &sz, modes);
			RDCASSERT(vkr == VK_SUCCESS);

			// If mailbox mode is available, use it, as is the lowest-latency non-
			// tearing mode.  If not, try IMMEDIATE which will usually be available,
			// and is fastest (though it tears).  If not, fall back to FIFO which is
			// always available.
			for (size_t i = 0; i < numModes; i++)
			{
				if (modes[i].presentMode == VK_PRESENT_MODE_MAILBOX_WSI)
				{
					presentmode = VK_PRESENT_MODE_MAILBOX_WSI;
					break;
				}

				if (modes[i].presentMode == VK_PRESENT_MODE_IMMEDIATE_WSI)
					presentmode = VK_PRESENT_MODE_IMMEDIATE_WSI;
			}

			SAFE_DELETE_ARRAY(modes);
		}
	}

	VkSwapChainCreateInfoWSI swapInfo = {
			VK_STRUCTURE_TYPE_SWAP_CHAIN_CREATE_INFO_WSI, NULL, (VkSurfaceDescriptionWSI *)&surfDesc,
			2, imformat, { width, height },
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
			VK_SURFACE_TRANSFORM_NONE_WSI, 1, presentmode,
			old, true,
	};

	vkr = vt->CreateSwapChainWSI(device, &swapInfo, &swap);
	RDCASSERT(vkr == VK_SUCCESS);

	if(old != VK_NULL_HANDLE)
		vt->DestroySwapChainWSI(device, old);

	size_t sz;
	vkr = vt->GetSwapChainInfoWSI(device, swap, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &sz, NULL);
	RDCASSERT(vkr == VK_SUCCESS);

	numImgs = uint32_t(sz/sizeof(VkSwapChainImagePropertiesWSI));

	VkSwapChainImagePropertiesWSI* imgs = new VkSwapChainImagePropertiesWSI[numImgs];
	vkr = vt->GetSwapChainInfoWSI(device, swap, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &sz, imgs);
	RDCASSERT(vkr == VK_SUCCESS);

	for(size_t i=0; i < numImgs; i++)
	{
		colimg[i] = imgs[i].image;
		coltrans[i].image = imgs[i].image;
		coltrans[i].oldLayout = coltrans[i].newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	if(depth)
	{
		VULKANNOTIMP("Allocating depth-stencil image");

		/*
		dsmem = mem;
		dsimg = img;
		depthtrans.image = stenciltrans.image = img;
		depthtrans.oldLayout = depthtrans.newLayout = 
			stenciltrans.oldLayout = stenciltrans.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		*/
	}

	{
		VkAttachmentDescription attDesc = {
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
			imformat, 1,
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

		vkr = vt->CreateRenderPass(device, &rpinfo, &renderpass);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	{
		VkViewport vp = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f, };
		VkRect2D sc = { { 0, 0 }, { width, height } };

		VkDynamicViewportStateCreateInfo vpInfo = {
			VK_STRUCTURE_TYPE_DYNAMIC_VIEWPORT_STATE_CREATE_INFO, NULL,
			1, &vp, &sc
		};

		VkResult vkr = vt->CreateDynamicViewportState(device, &vpInfo, &fullVP);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	{
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, imformat, { width, height, 1 },
			1, 1, 1,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
			0, VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
		};

		VkResult vkr = vt->CreateImage(device, &imInfo, &bb);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryRequirements mrq = {0};

		vkr = vt->GetImageMemoryRequirements(device, bb, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(device, &allocInfo, &bbmem);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->BindImageMemory(device, bb, bbmem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		bbtrans.image = bb;
		bbtrans.oldLayout = bbtrans.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	{
		VkAttachmentViewCreateInfo info = {
			VK_STRUCTURE_TYPE_ATTACHMENT_VIEW_CREATE_INFO, NULL,
			bb, imformat, 0, 0, 1,
			0 };

		vkr = vt->CreateAttachmentView(device, &info, &bbview);
		RDCASSERT(vkr == VK_SUCCESS);

		VkAttachmentBindInfo attBind = { bbview, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkFramebufferCreateInfo fbinfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
			renderpass,
			1, &attBind,
			(uint32_t)width, (uint32_t)height, 1,
		};

		vkr = vt->CreateFramebuffer(device, &fbinfo, &fb);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(dsimg != VK_NULL_HANDLE)
	{
		VkAttachmentViewCreateInfo info = {
			VK_STRUCTURE_TYPE_ATTACHMENT_VIEW_CREATE_INFO, NULL,
			dsimg, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, 0, 1,
			0 };

		vkr = vt->CreateAttachmentView(device, &info, &dsview);
		RDCASSERT(vkr == VK_SUCCESS);
	}
}

void VulkanReplay::UBO::Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size)
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

void VulkanReplay::UBO::Destroy(const VkLayerDispatchTable *vt, VkDevice dev)
{
	VkResult vkr = VK_SUCCESS;
	if(view != VK_NULL_HANDLE)
	{
		vt->DestroyBufferView(dev, view);
		RDCASSERT(vkr == VK_SUCCESS);
		view = VK_NULL_HANDLE;
	}
	
	if(buf != VK_NULL_HANDLE)
	{
		vt->DestroyBuffer(dev, buf);
		RDCASSERT(vkr == VK_SUCCESS);
		buf = VK_NULL_HANDLE;
	}

	if(mem != VK_NULL_HANDLE)
	{
		vt->FreeMemory(dev, mem);
		RDCASSERT(vkr == VK_SUCCESS);
		mem = VK_NULL_HANDLE;
	}
}

void *VulkanReplay::UBO::Map(const VkLayerDispatchTable *vt, VkDevice dev, VkDeviceSize offset, VkDeviceSize size)
{
	void *ptr = NULL;
	VkResult vkr = vt->MapMemory(dev, mem, offset, size, 0, (void **)&ptr);
	RDCASSERT(vkr == VK_SUCCESS);
	return ptr;
}

void VulkanReplay::UBO::Unmap(const VkLayerDispatchTable *vt, VkDevice dev)
{
	vt->UnmapMemory(dev, mem);
}

VulkanReplay::VulkanReplay()
{
	m_pDriver = NULL;
	m_Proxy = false;

	m_OutputWinID = 1;
	m_ActiveWinID = 0;
	m_BindDepth = false;

	RDCEraseEl(m_DebugData);
}

void VulkanReplay::InitDebugData()
{
	VkDevice dev = m_pDriver->GetDev();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);
	
	VkResult vkr = VK_SUCCESS;
	
	ResourceId id;
	VkImage fakeBBIm = VK_NULL_HANDLE;
	VkExtent3D fakeBBext;
	ResourceFormat fakeBBfmt;
	m_pDriver->GetFakeBB(id, fakeBBIm, fakeBBext, fakeBBfmt);

	VkImageViewCreateInfo bbviewInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
		fakeBBIm, VK_IMAGE_VIEW_TYPE_2D,
		(VkFormat)fakeBBfmt.rawType,
		{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
		{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1, }
	};
	
	// VKTODOMED will have to be created on the fly for whichever image we're
	// viewing (and cached)
	vkr = vt->CreateImageView(dev, &bbviewInfo, &m_DebugData.m_FakeBBImView);
	RDCASSERT(vkr == VK_SUCCESS);

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

	vkr = vt->CreateSampler(dev, &sampInfo, &m_DebugData.m_LinearSampler);
	RDCASSERT(vkr == VK_SUCCESS);

	sampInfo.minFilter = VK_TEX_FILTER_NEAREST;
	sampInfo.magFilter = VK_TEX_FILTER_NEAREST;
	sampInfo.mipMode = VK_TEX_MIPMAP_MODE_NEAREST;

	vkr = vt->CreateSampler(dev, &sampInfo, &m_DebugData.m_PointSampler);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOMED all of this is leaking

	VkPipelineCacheCreateInfo cacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, NULL, 0, NULL, 0 };

	vkr = vt->CreatePipelineCache(dev, &cacheInfo, &m_DebugData.m_PipelineCache);
	RDCASSERT(vkr == VK_SUCCESS);

	{
		VkDescriptorSetLayoutBinding layoutBinding[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, NULL, }
		};

		VkDescriptorSetLayoutCreateInfo descsetLayoutInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL,
			ARRAY_COUNT(layoutBinding), &layoutBinding[0],
		};

		vkr = vt->CreateDescriptorSetLayout(dev, &descsetLayoutInfo, &m_DebugData.m_CheckerboardDescSetLayout);
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

		vkr = vt->CreateDescriptorSetLayout(dev, &descsetLayoutInfo, &m_DebugData.m_TexDisplayDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	VkPipelineLayoutCreateInfo pipeLayoutInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL,
		1, &m_DebugData.m_TexDisplayDescSetLayout,
		0, NULL, // push constant ranges
	};
	
	vkr = vt->CreatePipelineLayout(dev, &pipeLayoutInfo, &m_DebugData.m_TexDisplayPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	pipeLayoutInfo.pSetLayouts = &m_DebugData.m_CheckerboardDescSetLayout;
	
	vkr = vt->CreatePipelineLayout(dev, &pipeLayoutInfo, &m_DebugData.m_CheckerboardPipeLayout);
	RDCASSERT(vkr == VK_SUCCESS);

	VkDescriptorTypeCount descPoolTypes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024, },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024, },
	};
	
	VkDescriptorPoolCreateInfo descpoolInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL,
    ARRAY_COUNT(descPoolTypes), &descPoolTypes[0],
	};
	
	vkr = vt->CreateDescriptorPool(dev, VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 2, &descpoolInfo, &m_DebugData.m_DescriptorPool);
	RDCASSERT(vkr == VK_SUCCESS);
	
	uint32_t count;
	vkr = vt->AllocDescriptorSets(dev, m_DebugData.m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_DebugData.m_CheckerboardDescSetLayout, &m_DebugData.m_CheckerboardDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);
	
	vkr = vt->AllocDescriptorSets(dev, m_DebugData.m_DescriptorPool, VK_DESCRIPTOR_SET_USAGE_STATIC, 1,
		&m_DebugData.m_TexDisplayDescSetLayout, &m_DebugData.m_TexDisplayDescSet, &count);
	RDCASSERT(vkr == VK_SUCCESS);

	m_DebugData.m_CheckerboardUBO.Create(m_pDriver, dev, 128);
	m_DebugData.m_TexDisplayUBO.Create(m_pDriver, dev, 128);

	RDCCOMPILE_ASSERT(sizeof(displayuniforms) < 128, "tex display size");

	VkDescriptorInfo desc[3];
	RDCEraseEl(desc);
	
	desc[0].bufferView = m_DebugData.m_CheckerboardUBO.view;
	desc[1].bufferView = m_DebugData.m_TexDisplayUBO.view;
	desc[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc[2].imageView = m_DebugData.m_FakeBBImView;
	desc[2].sampler = m_DebugData.m_LinearSampler;

	VkWriteDescriptorSet writeSet[] = {
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_DebugData.m_CheckerboardDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[0]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_DebugData.m_TexDisplayDescSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &desc[1]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			m_DebugData.m_TexDisplayDescSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc[2]
		},
	};

	vkr = vt->UpdateDescriptorSets(dev, ARRAY_COUNT(writeSet), writeSet, 0, NULL);
	RDCASSERT(vkr == VK_SUCCESS);

	VkDynamicRasterStateCreateInfo rsInfo = {
		VK_STRUCTURE_TYPE_DYNAMIC_RASTER_STATE_CREATE_INFO, NULL,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	vkr = vt->CreateDynamicRasterState(dev, &rsInfo, &m_DebugData.m_DynamicRSState);
	RDCASSERT(vkr == VK_SUCCESS);
	
	VkDynamicColorBlendStateCreateInfo cbInfo = {
		VK_STRUCTURE_TYPE_DYNAMIC_COLOR_BLEND_STATE_CREATE_INFO, NULL,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
	};

	vkr = vt->CreateDynamicColorBlendState(dev, &cbInfo, &m_DebugData.m_DynamicCBStateWhite);
	RDCASSERT(vkr == VK_SUCCESS);
	
	VkDynamicDepthStencilStateCreateInfo dsInfo = {
		VK_STRUCTURE_TYPE_DYNAMIC_DEPTH_STENCIL_STATE_CREATE_INFO, NULL,
		0.0f, 1.0f, 0xff, 0xff, 0, 0,
	};

	vkr = vt->CreateDynamicDepthStencilState(dev, &dsInfo, &m_DebugData.m_DynamicDSStateDisabled);
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
		m_DebugData.m_CheckerboardPipeLayout,
		VK_NULL_HANDLE, // render pass
		0, // sub pass
		VK_NULL_HANDLE, // base pipeline handle
		0, // base pipeline index
	};

	stages[0].shader = shader[BLITVS];
	stages[1].shader = shader[CHECKERBOARDFS];

	vkr = vt->CreateGraphicsPipelines(dev, m_DebugData.m_PipelineCache, 1, &pipeInfo, &m_DebugData.m_CheckerboardPipeline);
	RDCASSERT(vkr == VK_SUCCESS);
	
	stages[1].shader = shader[TEXDISPLAYFS];

	pipeInfo.layout = m_DebugData.m_TexDisplayPipeLayout;

	vkr = vt->CreateGraphicsPipelines(dev, m_DebugData.m_PipelineCache, 1, &pipeInfo, &m_DebugData.m_TexDisplayPipeline);
	RDCASSERT(vkr == VK_SUCCESS);

	attState.blendEnable = true;
	attState.srcBlendColor = VK_BLEND_SRC_ALPHA;
	attState.destBlendColor = VK_BLEND_ONE_MINUS_SRC_ALPHA;

	vkr = vt->CreateGraphicsPipelines(dev, m_DebugData.m_PipelineCache, 1, &pipeInfo, &m_DebugData.m_TexDisplayBlendPipeline);
	RDCASSERT(vkr == VK_SUCCESS);

	for(size_t i=0; i < ARRAY_COUNT(module); i++)
	{
		vkr = vt->DestroyShader(dev, shader[i]);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->DestroyShaderModule(dev, module[i]);
		RDCASSERT(vkr == VK_SUCCESS);
	}
}

void VulkanReplay::ShutdownDebugData()
{
	VkDevice dev = m_pDriver->GetDev();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);
	
	VkResult vkr = VK_SUCCESS;

	if(m_DebugData.m_PipelineCache != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineCache(dev, m_DebugData.m_PipelineCache);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_DescriptorPool != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorPool(dev, m_DebugData.m_DescriptorPool);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_DynamicCBStateWhite != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDynamicColorBlendState(dev, m_DebugData.m_DynamicCBStateWhite);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_DynamicRSState != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDynamicRasterState(dev, m_DebugData.m_DynamicRSState);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_DynamicDSStateDisabled != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDynamicDepthStencilState(dev, m_DebugData.m_DynamicDSStateDisabled);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_LinearSampler != VK_NULL_HANDLE)
	{
		vkr = vt->DestroySampler(dev, m_DebugData.m_LinearSampler);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_PointSampler != VK_NULL_HANDLE)
	{
		vkr = vt->DestroySampler(dev, m_DebugData.m_PointSampler);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_FakeBBImView != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyImageView(dev, m_DebugData.m_FakeBBImView);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_CheckerboardDescSetLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorSetLayout(dev, m_DebugData.m_CheckerboardDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_CheckerboardPipeLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineLayout(dev, m_DebugData.m_CheckerboardPipeLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_CheckerboardPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_DebugData.m_CheckerboardPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_TexDisplayDescSetLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyDescriptorSetLayout(dev, m_DebugData.m_TexDisplayDescSetLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_TexDisplayPipeLayout != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipelineLayout(dev, m_DebugData.m_TexDisplayPipeLayout);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_TexDisplayPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_DebugData.m_TexDisplayPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	if(m_DebugData.m_TexDisplayBlendPipeline != VK_NULL_HANDLE)
	{
		vkr = vt->DestroyPipeline(dev, m_DebugData.m_TexDisplayBlendPipeline);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	m_DebugData.m_CheckerboardUBO.Destroy(vt, dev);
	m_DebugData.m_TexDisplayUBO.Destroy(vt, dev);

	RDCEraseEl(m_DebugData);
}

void VulkanReplay::Shutdown()
{
	ShutdownDebugData();

	delete m_pDriver;
}

APIProperties VulkanReplay::GetAPIProperties()
{
	APIProperties ret;

	ret.pipelineType = ePipelineState_D3D11;
	ret.degraded = false;

	return ret;
}

void VulkanReplay::ReadLogInitialisation()
{
	m_pDriver->ReadLogInitialisation();

	InitDebugData();
}

void VulkanReplay::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	m_pDriver->ReplayLog(frameID, startEventID, endEventID, replayType);
}

ResourceId VulkanReplay::GetLiveID(ResourceId id)
{
	return m_pDriver->GetResourceManager()->GetLiveID(id);
}

void VulkanReplay::InitCallstackResolver()
{
	m_pDriver->GetSerialiser()->InitCallstackResolver();
}

bool VulkanReplay::HasCallstacks()
{
	return m_pDriver->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *VulkanReplay::GetCallstackResolver()
{
	return m_pDriver->GetSerialiser()->GetCallstackResolver();
}

vector<FetchFrameRecord> VulkanReplay::GetFrameRecord()
{
	return m_pDriver->GetFrameRecord();
}

vector<DebugMessage> VulkanReplay::GetDebugMessages()
{
	VULKANNOTIMP("GetDebugMessages");
	return vector<DebugMessage>();
}

vector<ResourceId> VulkanReplay::GetTextures()
{
	VULKANNOTIMP("GetTextures");
	vector<ResourceId> texs;

	ResourceId id;
	VkImage fakeBBIm = VK_NULL_HANDLE;
	VkExtent3D fakeBBext;
	ResourceFormat fakeBBfmt;
	m_pDriver->GetFakeBB(id, fakeBBIm, fakeBBext, fakeBBfmt);

	texs.push_back(id);
	return texs;
}
	
vector<ResourceId> VulkanReplay::GetBuffers()
{
	VULKANNOTIMP("GetBuffers");
	return vector<ResourceId>();
}

void VulkanReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4])
{
	//VULKANNOTIMP("PickPixel");

	ResourceId resid;
	VkImage fakeBBIm = VK_NULL_HANDLE;
	VkExtent3D fakeBBext;
	ResourceFormat fakeBBfmt;
	m_pDriver->GetFakeBB(resid, fakeBBIm, fakeBBext, fakeBBfmt);

	if(x >= (uint32_t)fakeBBext.width || y >= (uint32_t)fakeBBext.height)
	{
		RDCEraseEl(pixel);
		return;
	}

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	// VKTODOMED this should be all created offline, including separate host and
	// readback buffers
	VkDeviceMemory readbackmem = VK_NULL_HANDLE;
	VkBuffer destbuf = VK_NULL_HANDLE;

	{
		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			128, VK_BUFFER_USAGE_GENERAL, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		VkResult vkr = vt->CreateBuffer(dev, &bufInfo, &destbuf);
		RDCASSERT(vkr == VK_SUCCESS);
		
		VkMemoryRequirements mrq;
		vkr = vt->GetBufferMemoryRequirements(dev, destbuf, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			128, m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(dev, &allocInfo, &readbackmem);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->BindBufferMemory(dev, destbuf, readbackmem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOHIGH find out the actual current image state
		VkImageMemoryBarrier fakeTrans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
			0, 0, fakeBBIm,
			{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = vt->ResetCommandBuffer(cmd, 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		void *barrier = (void *)&fakeTrans;
		vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
		fakeTrans.oldLayout = fakeTrans.newLayout;

		VkBufferImageCopy region = {
			0, 128, 1,
			{ VK_IMAGE_ASPECT_COLOR, 0, 0}, { (int)x, (int)y, 0 },
			{ 1, 1, 1 },
		};
		vt->CmdCopyImageToBuffer(cmd, fakeBBIm, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, destbuf, 1, &region);

		fakeTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI;
		vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

		vt->EndCommandBuffer(cmd);

		vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);

		vt->QueueWaitIdle(q);
	}

	// VKTODOHIGH ultra cheeky - map memory directly without copying
	// to host-visible memory
	byte *pData = NULL;
	vt->MapMemory(dev, readbackmem, 0, 0, 0, (void **)&pData);

	RDCASSERT(pData != NULL);

	// VKTODOMED this should go through render texture so that we
	// just get pure F32 RGBA data out
	if(fakeBBfmt.specialFormat == eSpecial_B8G8R8A8)
	{
		pixel[0] = float(pData[2])/255.0f;
		pixel[1] = float(pData[1])/255.0f;
		pixel[2] = float(pData[0])/255.0f;
		pixel[3] = float(pData[3])/255.0f;
	}
	else
	{
		pixel[0] = float(pData[0])/255.0f;
		pixel[1] = float(pData[1])/255.0f;
		pixel[2] = float(pData[2])/255.0f;
		pixel[3] = float(pData[3])/255.0f;
	}

	vt->UnmapMemory(dev, readbackmem);

	vt->DeviceWaitIdle(dev);

	vt->DestroyBuffer(dev, destbuf);
	vt->FreeMemory(dev, readbackmem);
}

uint32_t VulkanReplay::PickVertex(uint32_t frameID, uint32_t eventID, MeshDisplay cfg, uint32_t x, uint32_t y)
{
	RDCUNIMPLEMENTED("PickVertex");
	return ~0U;
}

bool VulkanReplay::RenderTexture(TextureDisplay cfg)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(it == m_OutputWindows.end())
	{
		RDCERR("output window not bound");
		return false;
	}

	OutputWindow &outw = it->second;

	ResourceId resid;
	VkImage fakeBBIm = VK_NULL_HANDLE;
	VkExtent3D fakeBBext;
	ResourceFormat fakeBBfmt;
	m_pDriver->GetFakeBB(resid, fakeBBIm, fakeBBext, fakeBBfmt);
	
	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	// VKTODOHIGH once we stop doing DeviceWaitIdle/QueueWaitIdle all over, this
	// needs to be ring-buffered
	displayuniforms *data = (displayuniforms *)m_DebugData.m_TexDisplayUBO.Map(vt, dev);

	data->Padding = 0;
	
	float x = cfg.offx;
	float y = cfg.offy;
	
	data->Position.x = x;
	data->Position.y = y;
	data->Scale = cfg.scale;
	data->HDRMul = -1.0f;

	int32_t tex_x = fakeBBext.width;
	int32_t tex_y = fakeBBext.height;
	int32_t tex_z = fakeBBext.depth;
	
	if(cfg.scale <= 0.0f)
	{
		float xscale = float(outw.width)/float(tex_x);
		float yscale = float(outw.height)/float(tex_y);

		float scale = data->Scale = RDCMIN(xscale, yscale);

		if(yscale > xscale)
		{
			data->Position.x = 0;
			data->Position.y = (float(outw.width)-(tex_y*scale) )*0.5f;
		}
		else
		{
			data->Position.y = 0;
			data->Position.x = (float(outw.height)-(tex_x*scale) )*0.5f;
		}
	}

	data->Channels.x = cfg.Red ? 1.0f : 0.0f;
	data->Channels.y = cfg.Green ? 1.0f : 0.0f;
	data->Channels.z = cfg.Blue ? 1.0f : 0.0f;
	data->Channels.w = cfg.Alpha ? 1.0f : 0.0f;
	
	if(cfg.rangemax <= cfg.rangemin) cfg.rangemax += 0.00001f;
	
	data->RangeMinimum = cfg.rangemin;
	data->InverseRangeSize = 1.0f/(cfg.rangemax-cfg.rangemin);
	
	data->FlipY = cfg.FlipY ? 1 : 0;

	data->MipLevel = (float)cfg.mip;
	data->Slice = 0;
	if(1 /* VKTODOLOW check texture type texDetails.curType != eGL_TEXTURE_3D*/)
		data->Slice = (float)cfg.sliceFace;
	else
		data->Slice = (float)(cfg.sliceFace>>cfg.mip);
	
	data->TextureResolutionPS.x = float(tex_x);
	data->TextureResolutionPS.y = float(tex_y);
	data->TextureResolutionPS.z = float(tex_z);

	float mipScale = float(1<<cfg.mip);

	// VKTODOMED reading from data pointer (should not)
	data->Scale *= mipScale;
	data->TextureResolutionPS.x /= mipScale;
	data->TextureResolutionPS.y /= mipScale;
	data->TextureResolutionPS.z /= mipScale;
	
	// VKTODOLOW multisampled texture display
	data->NumSamples = 1;
	data->SampleIdx = 0;

	data->OutputRes.x = (float)outw.width;
	data->OutputRes.y = (float)outw.height;

	// VKTODOMED handle different texture types/displays
	data->OutputDisplayFormat = 0;
	
	data->RawOutput = cfg.rawoutput ? 1 : 0;

	m_DebugData.m_TexDisplayUBO.Unmap(vt, dev);
	
	VkDescriptorInfo desc = {0};
	desc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc.imageView = m_DebugData.m_FakeBBImView;
	desc.sampler = m_DebugData.m_PointSampler;
	if(cfg.mip == 0 && cfg.scale < 1.0f)
		desc.sampler = m_DebugData.m_LinearSampler;

	VkWriteDescriptorSet writeSet = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
		m_DebugData.m_TexDisplayDescSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc
	};

	VkResult vkr = vt->UpdateDescriptorSets(dev, 1, &writeSet, 0, NULL);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOHIGH find out the actual current image state
	VkImageMemoryBarrier fakeTrans = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0, fakeBBIm,
		{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	vkr = vt->ResetCommandBuffer(cmd, 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier = (void *)&fakeTrans;

	vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
	fakeTrans.oldLayout = fakeTrans.newLayout;

	{
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			outw.renderpass, outw.fb,
			{ { 0, 0, }, { outw.width, outw.height } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(cmd, &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		// VKTODOMED will need a way to disable blend for other things
		vt->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cfg.rawoutput ? m_DebugData.m_TexDisplayPipeline : m_DebugData.m_TexDisplayBlendPipeline);
		vt->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DebugData.m_TexDisplayPipeLayout, 0, 1, &m_DebugData.m_TexDisplayDescSet, 0, NULL);

		vt->CmdBindDynamicViewportState(cmd, outw.fullVP);
		vt->CmdBindDynamicRasterState(cmd, m_DebugData.m_DynamicRSState);
		vt->CmdBindDynamicColorBlendState(cmd, m_DebugData.m_DynamicCBStateWhite);
		vt->CmdBindDynamicDepthStencilState(cmd, m_DebugData.m_DynamicDSStateDisabled);

		vt->CmdDraw(cmd, 0, 4, 0, 1);
		vt->CmdEndRenderPass(cmd);
	}

	fakeTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI;
	vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vt->EndCommandBuffer(cmd);

	vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);

	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(q);

	return false;
}
	
void VulkanReplay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->ResetCommandBuffer(cmd, 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier = (void *)&outw.bbtrans;

	// VKTODOHIGH once we stop doing DeviceWaitIdle/QueueWaitIdle all over, this
	// needs to be ring-buffered
	Vec4f *data = (Vec4f *)m_DebugData.m_CheckerboardUBO.Map(vt, dev);
	data[0].x = light.x;
	data[0].y = light.y;
	data[0].z = light.z;
	data[1].x = dark.x;
	data[1].y = dark.y;
	data[1].z = dark.z;
	m_DebugData.m_CheckerboardUBO.Unmap(vt, dev);

	{
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			outw.renderpass, outw.fb,
			{ { 0, 0, }, { outw.width, outw.height } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(cmd, &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		vt->CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DebugData.m_CheckerboardPipeline);
		vt->CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DebugData.m_CheckerboardPipeLayout, 0, 1, &m_DebugData.m_CheckerboardDescSet, 0, NULL);

		vt->CmdBindDynamicViewportState(cmd, outw.fullVP);
		vt->CmdBindDynamicRasterState(cmd, m_DebugData.m_DynamicRSState);
		vt->CmdBindDynamicColorBlendState(cmd, m_DebugData.m_DynamicCBStateWhite);
		vt->CmdBindDynamicDepthStencilState(cmd, m_DebugData.m_DynamicDSStateDisabled);

		vt->CmdDraw(cmd, 0, 4, 0, 1);
		vt->CmdEndRenderPass(cmd);
	}

	vkr = vt->EndCommandBuffer(cmd);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(q);
}
	
void VulkanReplay::RenderHighlightBox(float w, float h, float scale)
{
	VULKANNOTIMP("RenderHighlightBox");
}
	
ResourceId VulkanReplay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	RDCUNIMPLEMENTED("RenderOverlay");
	return ResourceId();
}
	
void VulkanReplay::RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg)
{
	RDCUNIMPLEMENTED("RenderMesh");
}

bool VulkanReplay::CheckResizeOutputWindow(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;
	
	OutputWindow &outw = m_OutputWindows[id];

	if(outw.wnd == NULL_WND_HANDLE)
		return false;
	
	int32_t w, h;
	GetOutputWindowDimensions(id, w, h);

	if(w != outw.width || h != outw.height)
	{
		outw.width = w;
		outw.height = h;

		// VKTODOHIGH Currently the resize code crashes - unsure why
		if(outw.width > 0 && outw.height > 0 && 0)
		{
			bool depth = (outw.dsimg != VK_NULL_HANDLE);

			outw.Create(m_pDriver, m_pDriver->GetDev(), depth);
		}

		return true;
	}

	return false;
}

void VulkanReplay::BindOutputWindow(uint64_t id, bool depth)
{
	m_ActiveWinID = id;
	m_BindDepth = depth;
	
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);
	
	VkSemaphore sem;
	VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT };

	VkResult vkr = vt->CreateSemaphore(dev, &semInfo, &sem);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->AcquireNextImageWSI(dev, outw.swap, UINT64_MAX, sem, &outw.curidx);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->QueueWaitSemaphore(q, sem);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->DestroySemaphore(dev, sem);
	RDCASSERT(vkr == VK_SUCCESS);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = vt->ResetCommandBuffer(cmd, 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier[] = {
		(void *)&outw.bbtrans,
		(void *)&outw.coltrans[outw.curidx],
	};
	
	outw.bbtrans.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.coltrans[outw.curidx].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

	vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, barrier);

	outw.bbtrans.oldLayout = outw.bbtrans.newLayout;
	outw.coltrans[outw.curidx].oldLayout = outw.bbtrans.newLayout;

	vt->EndCommandBuffer(cmd);

	vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);
	
	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(q);
}

void VulkanReplay::ClearOutputWindowColour(uint64_t id, float col[4])
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->ResetCommandBuffer(cmd, 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	vt->CmdClearColorImage(cmd, outw.bb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)col, 1, &outw.bbtrans.subresourceRange);

	vt->EndCommandBuffer(cmd);

	vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);
	
	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(q);
}

void VulkanReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	VULKANNOTIMP("ClearOutputWindowDepth");

	// VKTODOMED: same as FlipOutputWindow but do a depth clear
}

void VulkanReplay::FlipOutputWindow(uint64_t id)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = device_dispatch_table(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->ResetCommandBuffer(cmd, 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(cmd, &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier[] = {
		(void *)&outw.bbtrans,
		(void *)&outw.coltrans[outw.curidx],
	};
	
	outw.bbtrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL;
	vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, barrier);
	outw.bbtrans.oldLayout = outw.bbtrans.newLayout;

	VkImageCopy cpy = {
		{ VK_IMAGE_ASPECT_COLOR, 0, 0 },
		{ 0, 0, 0 },
		{ VK_IMAGE_ASPECT_COLOR, 0, 0 },
		{ 0, 0, 0 },
		{ outw.width, outw.height, 1 },
	};

	vt->CmdCopyImage(cmd, outw.bb, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, outw.colimg[outw.curidx], VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &cpy);
	
	outw.bbtrans.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.coltrans[outw.curidx].newLayout = VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI;

	vt->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, barrier);

	outw.bbtrans.oldLayout = outw.bbtrans.newLayout;
	outw.coltrans[outw.curidx].oldLayout = outw.coltrans[outw.curidx].newLayout;

	vt->EndCommandBuffer(cmd);

	vt->QueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);

	VkPresentInfoWSI presentInfo = { VK_STRUCTURE_TYPE_QUEUE_PRESENT_INFO_WSI, NULL, 1, &outw.swap, &outw.curidx };

	vt->QueuePresentWSI(q, &presentInfo);

	vt->QueueWaitIdle(q);

	vt->DeviceWaitIdle(dev);
}

void VulkanReplay::DestroyOutputWindow(uint64_t id)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	outw.Destroy(m_pDriver, m_pDriver->GetDev());

	m_OutputWindows.erase(it);
}
	
uint64_t VulkanReplay::MakeOutputWindow(void *wn, bool depth)
{
	uint64_t id = m_OutputWinID;
	m_OutputWinID++;

	m_OutputWindows[id].SetWindowHandle(wn);

	if(wn != NULL)
	{
		int32_t w, h;
		GetOutputWindowDimensions(id, w, h);

		m_OutputWindows[id].width = w;
		m_OutputWindows[id].height = h;
		
		m_OutputWindows[id].Create(m_pDriver, m_pDriver->GetDev(), depth);
	}

	return id;
}

vector<byte> VulkanReplay::GetBufferData(ResourceId buff, uint32_t offset, uint32_t len)
{
	RDCUNIMPLEMENTED("GetBufferData");
	return vector<byte>();
}

bool VulkanReplay::IsRenderOutput(ResourceId id)
{
	RDCUNIMPLEMENTED("IsRenderOutput");
	return false;
}

void VulkanReplay::FileChanged()
{
}

FetchTexture VulkanReplay::GetTexture(ResourceId id)
{
	VULKANNOTIMP("GetTexture");
	
	ResourceId resid;
	VkImage fakeBBIm = VK_NULL_HANDLE;
	VkExtent3D fakeBBext;
	ResourceFormat fakeBBfmt;
	m_pDriver->GetFakeBB(resid, fakeBBIm, fakeBBext, fakeBBfmt);

	FetchTexture ret;
	ret.arraysize = 1;
	ret.byteSize = fakeBBext.width*fakeBBext.height*4;
	ret.creationFlags = eTextureCreate_SwapBuffer|eTextureCreate_SRV|eTextureCreate_RTV;
	ret.cubemap = false;
	ret.customName = false;
	ret.depth = 1;
	ret.width = fakeBBext.width;
	ret.height = fakeBBext.height;
	ret.dimension = 2;
	ret.ID = id;
	ret.mips = 1;
	ret.msQual = 0;
	ret.msSamp = 1;
	ret.name = "WSI Presentable Image";
	ret.numSubresources = 1;
	ret.format = fakeBBfmt;
	return ret;
}

FetchBuffer VulkanReplay::GetBuffer(ResourceId id)
{
	RDCUNIMPLEMENTED("GetBuffer");
	return FetchBuffer();
}

ShaderReflection *VulkanReplay::GetShader(ResourceId id)
{
	RDCUNIMPLEMENTED("GetShader");
	return NULL;
}

void VulkanReplay::SavePipelineState()
{
	VULKANNOTIMP("SavePipelineState");

	{
		create_array_uninit(m_D3D11PipelineState.m_OM.RenderTargets, 1);

		ResourceId id;
		VkImage fakeBBIm = VK_NULL_HANDLE;
		VkExtent3D fakeBBext;
		ResourceFormat fakeBBfmt;
		m_pDriver->GetFakeBB(id, fakeBBIm, fakeBBext, fakeBBfmt);

		m_D3D11PipelineState.m_OM.RenderTargets[0].Resource = id;
	}

	{
		const WrappedVulkan::PartialReplayData::StateVector &state = m_pDriver->m_PartialReplayData.state;
		VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

		VulkanResourceManager *rm = m_pDriver->GetResourceManager();

		m_VulkanPipelineState = VulkanPipelineState();
		
		// General pipeline properties
		m_VulkanPipelineState.compute.obj = state.compute.pipeline;
		m_VulkanPipelineState.graphics.obj = state.graphics.pipeline;

		if(state.compute.pipeline != ResourceId())
			m_VulkanPipelineState.compute.flags = c.m_Pipeline[state.compute.pipeline].flags;

		if(state.graphics.pipeline != ResourceId())
		{
			const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.graphics.pipeline];

			m_VulkanPipelineState.graphics.flags = p.flags;

			// Input Assembly
			m_VulkanPipelineState.IA.ibuffer.buf = state.ibuffer.buf;
			m_VulkanPipelineState.IA.ibuffer.offs = state.ibuffer.offs;
			m_VulkanPipelineState.IA.primitiveRestartEnable = p.primitiveRestartEnable;

			// Vertex Input
			create_array_uninit(m_VulkanPipelineState.VI.attrs, p.vertexAttrs.size());
			for(size_t i=0; i < p.vertexAttrs.size(); i++)
			{
				m_VulkanPipelineState.VI.attrs[i].location = p.vertexAttrs[i].location;
				m_VulkanPipelineState.VI.attrs[i].binding = p.vertexAttrs[i].binding;
				m_VulkanPipelineState.VI.attrs[i].byteoffset = p.vertexAttrs[i].byteoffset;
				m_VulkanPipelineState.VI.attrs[i].format = MakeResourceFormat(p.vertexAttrs[i].format);
			}

			create_array_uninit(m_VulkanPipelineState.VI.binds, p.vertexBindings.size());
			for(size_t i=0; i < p.vertexBindings.size(); i++)
			{
				m_VulkanPipelineState.VI.binds[i].bytestride = p.vertexBindings[i].bytestride;
				m_VulkanPipelineState.VI.binds[i].vbufferBinding = p.vertexBindings[i].vbufferBinding;
				m_VulkanPipelineState.VI.binds[i].perInstance = p.vertexBindings[i].perInstance;
			}

			create_array_uninit(m_VulkanPipelineState.VI.vbuffers, state.vbuffers.size());
			for(size_t i=0; i < state.vbuffers.size(); i++)
			{
				m_VulkanPipelineState.VI.vbuffers[i].buffer = state.vbuffers[i].buf;
				m_VulkanPipelineState.VI.vbuffers[i].offset = state.vbuffers[i].offs;
			}

			// Shader Stages
			VulkanPipelineState::ShaderStage *stages[] = {
				&m_VulkanPipelineState.VS,
				&m_VulkanPipelineState.TCS,
				&m_VulkanPipelineState.TES,
				&m_VulkanPipelineState.GS,
				&m_VulkanPipelineState.FS,
				&m_VulkanPipelineState.CS,
			};

			for(size_t i=0; i < ARRAY_COUNT(stages); i++)
			{
				stages[i]->Shader = p.shaders[i];
				stages[i]->ShaderDetails = NULL;
				stages[i]->customName = false;
				stages[i]->ShaderName = StringFormat::Fmt("Shader %llu", p.shaders[i]);
				stages[i]->stage = ShaderStageType(eShaderStage_Vertex + i);
			}

			// Descriptor sets
			create_array_uninit(m_VulkanPipelineState.graphics.DescSets, state.graphics.descSets.size());
			create_array_uninit(m_VulkanPipelineState.compute.DescSets, state.compute.descSets.size());

			{
				rdctype::array<VulkanPipelineState::Pipeline::DescriptorSet> *dsts[] = {
					&m_VulkanPipelineState.graphics.DescSets,
					&m_VulkanPipelineState.compute.DescSets,
				};
				
				const vector<ResourceId> *srcs[] = {
					&state.graphics.descSets,
					&state.compute.descSets,
				};
				
				for(size_t p=0; p < ARRAY_COUNT(srcs); p++)
				{
					for(size_t i=0; i < srcs[p]->size(); i++)
					{
						ResourceId src = (*srcs[p])[i];
						VulkanPipelineState::Pipeline::DescriptorSet &dst = (*dsts[p])[i];

						dst.layout = m_pDriver->m_DescriptorSetInfo[src].layout;
						create_array_uninit(dst.bindings, m_pDriver->m_DescriptorSetInfo[src].currentBindings.size());
						for(size_t b=0; b < m_pDriver->m_DescriptorSetInfo[src].currentBindings.size(); b++)
						{
							VkDescriptorInfo *info = m_pDriver->m_DescriptorSetInfo[src].currentBindings[b];
							const VulkanCreationInfo::DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[dst.layout].bindings[b];

							dst.bindings[b].arraySize = layoutBind.arraySize;
							dst.bindings[b].stageFlags = (ShaderStageBits)layoutBind.stageFlags;
							switch(layoutBind.descriptorType)
							{
								case VK_DESCRIPTOR_TYPE_SAMPLER:                 dst.bindings[b].type = eBindType_Sampler; break;
								case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:  dst.bindings[b].type = eBindType_ImageSampler; break;
								case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:           dst.bindings[b].type = eBindType_ReadOnlyImage; break;
								case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:           dst.bindings[b].type = eBindType_ReadWriteImage; break;
								case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:    dst.bindings[b].type = eBindType_ReadOnlyTBuffer; break;
								case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:    dst.bindings[b].type = eBindType_ReadWriteTBuffer; break;
								case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:          dst.bindings[b].type = eBindType_ReadOnlyBuffer; break;
								case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:          dst.bindings[b].type = eBindType_ReadWriteBuffer; break;
								case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:  dst.bindings[b].type = eBindType_ReadOnlyBuffer; break;
								case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:  dst.bindings[b].type = eBindType_ReadWriteBuffer; break;
								case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:        dst.bindings[b].type = eBindType_InputAttachment; break;
								default:
									dst.bindings[b].type = eBindType_Unknown;
									RDCERR("Unexpected descriptor type");
							}
							
							create_array_uninit(dst.bindings[b].elems, layoutBind.arraySize);
							for(uint32_t a=0; a < layoutBind.arraySize; a++)
							{
								if(layoutBind.immutableSampler)
									dst.bindings[b].elems[a].sampler = layoutBind.immutableSampler[a];
								else if(info->sampler != VK_NULL_HANDLE)
									dst.bindings[b].elems[a].sampler = rm->GetOriginalID(rm->GetID(MakeRes(info->sampler)));

								// only one of these is ever set
								if(info->imageView != VK_NULL_HANDLE)
									dst.bindings[b].elems[a].view = rm->GetOriginalID(rm->GetID(MakeRes(info->imageView)));
								if(info->bufferView != VK_NULL_HANDLE)
									dst.bindings[b].elems[a].view = rm->GetOriginalID(rm->GetID(MakeRes(info->bufferView)));
								if(info->attachmentView != VK_NULL_HANDLE)
									dst.bindings[b].elems[a].view = rm->GetOriginalID(rm->GetID(MakeRes(info->attachmentView)));
							}
						}
					}
				}
			}

			// Tessellation
			m_VulkanPipelineState.Tess.numControlPoints = p.patchControlPoints;

			// Viewport/Scissors
			m_VulkanPipelineState.VP.state = state.dynamicVP;
			create_array_uninit(m_VulkanPipelineState.VP.viewportScissors, c.m_VPScissor[state.dynamicVP].viewports.size());
			for(size_t i=0; i < c.m_VPScissor[state.dynamicVP].viewports.size(); i++)
			{
				m_VulkanPipelineState.VP.viewportScissors[i].vp.x = c.m_VPScissor[state.dynamicVP].viewports[i].originX;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.y = c.m_VPScissor[state.dynamicVP].viewports[i].originY;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.width = c.m_VPScissor[state.dynamicVP].viewports[i].width;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.height = c.m_VPScissor[state.dynamicVP].viewports[i].height;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.minDepth = c.m_VPScissor[state.dynamicVP].viewports[i].minDepth;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.maxDepth = c.m_VPScissor[state.dynamicVP].viewports[i].maxDepth;

				m_VulkanPipelineState.VP.viewportScissors[i].scissor.x = c.m_VPScissor[state.dynamicVP].scissors[i].offset.x;
				m_VulkanPipelineState.VP.viewportScissors[i].scissor.y = c.m_VPScissor[state.dynamicVP].scissors[i].offset.y;
				m_VulkanPipelineState.VP.viewportScissors[i].scissor.width = c.m_VPScissor[state.dynamicVP].scissors[i].extent.width;
				m_VulkanPipelineState.VP.viewportScissors[i].scissor.height = c.m_VPScissor[state.dynamicVP].scissors[i].extent.height;
			}

			// Rasterizer
			m_VulkanPipelineState.RS.depthClipEnable = p.depthClipEnable;
			m_VulkanPipelineState.RS.rasterizerDiscardEnable = p.rasterizerDiscardEnable;
			m_VulkanPipelineState.RS.FrontCCW = p.frontFace == VK_FRONT_FACE_CCW;

			switch(p.fillMode)
			{
				case VK_FILL_MODE_POINTS:    m_VulkanPipelineState.RS.FillMode = eFill_Point;     break;
				case VK_FILL_MODE_WIREFRAME: m_VulkanPipelineState.RS.FillMode = eFill_Wireframe; break;
				case VK_FILL_MODE_SOLID:     m_VulkanPipelineState.RS.FillMode = eFill_Solid;     break;
				default:
					m_VulkanPipelineState.RS.FillMode = eFill_Solid;
					RDCERR("Unexpected value for FillMode %x", p.fillMode);
					break;
			}

			switch(p.cullMode)
			{
				case VK_CULL_MODE_NONE:           m_VulkanPipelineState.RS.CullMode = eCull_None;         break;
				case VK_CULL_MODE_FRONT:          m_VulkanPipelineState.RS.CullMode = eCull_Front;        break;
				case VK_CULL_MODE_BACK:           m_VulkanPipelineState.RS.CullMode = eCull_Back;         break;
				case VK_CULL_MODE_FRONT_AND_BACK: m_VulkanPipelineState.RS.CullMode = eCull_FrontAndBack; break;
				default:
					m_VulkanPipelineState.RS.CullMode = eCull_None;
					RDCERR("Unexpected value for CullMode %x", p.cullMode);
					break;
			}

			m_VulkanPipelineState.RS.state = state.dynamicRS;
			m_VulkanPipelineState.RS.depthBias = c.m_Raster[state.dynamicRS].depthBias;
			m_VulkanPipelineState.RS.depthBiasClamp = c.m_Raster[state.dynamicRS].depthBiasClamp;
			m_VulkanPipelineState.RS.slopeScaledDepthBias = c.m_Raster[state.dynamicRS].slopeScaledDepthBias;
			m_VulkanPipelineState.RS.lineWidth = c.m_Raster[state.dynamicRS].lineWidth;

			// MSAA
			m_VulkanPipelineState.MSAA.rasterSamples = p.rasterSamples;
			m_VulkanPipelineState.MSAA.sampleShadingEnable = p.sampleShadingEnable;
			m_VulkanPipelineState.MSAA.minSampleShading = p.minSampleShading;
			m_VulkanPipelineState.MSAA.sampleMask = p.sampleMask;

			// Color Blend
			m_VulkanPipelineState.CB.logicOpEnable = p.logicOpEnable;
			m_VulkanPipelineState.CB.alphaToCoverageEnable = p.alphaToCoverageEnable;
			m_VulkanPipelineState.CB.logicOp = ToStr::Get(p.logicOp);

			create_array_uninit(m_VulkanPipelineState.CB.attachments, p.attachments.size());
			for(size_t i=0; i < p.attachments.size(); i++)
			{
				m_VulkanPipelineState.CB.attachments[i].blendEnable = p.attachments[i].blendEnable;

				m_VulkanPipelineState.CB.attachments[i].blend.Source = ToStr::Get(p.attachments[i].blend.Source);
				m_VulkanPipelineState.CB.attachments[i].blend.Destination = ToStr::Get(p.attachments[i].blend.Destination);
				m_VulkanPipelineState.CB.attachments[i].blend.Operation = ToStr::Get(p.attachments[i].blend.Operation);

				m_VulkanPipelineState.CB.attachments[i].alphaBlend.Source = ToStr::Get(p.attachments[i].alphaBlend.Source);
				m_VulkanPipelineState.CB.attachments[i].alphaBlend.Destination = ToStr::Get(p.attachments[i].alphaBlend.Destination);
				m_VulkanPipelineState.CB.attachments[i].alphaBlend.Operation = ToStr::Get(p.attachments[i].alphaBlend.Operation);

				m_VulkanPipelineState.CB.attachments[i].writeMask = p.attachments[i].channelWriteMask;
			}

			m_VulkanPipelineState.CB.state = state.dynamicCB;
			memcpy(m_VulkanPipelineState.CB.blendConst, c.m_Blend[state.dynamicCB].blendConst, sizeof(float)*4);

			// Depth Stencil
			m_VulkanPipelineState.DS.depthTestEnable = p.depthTestEnable;
			m_VulkanPipelineState.DS.depthWriteEnable = p.depthWriteEnable;
			m_VulkanPipelineState.DS.depthBoundsEnable = p.depthBoundsEnable;
			m_VulkanPipelineState.DS.depthCompareOp = ToStr::Get(p.depthCompareOp);
			m_VulkanPipelineState.DS.stencilTestEnable = p.stencilTestEnable;

			m_VulkanPipelineState.DS.front.passOp = ToStr::Get(p.front.stencilPassOp);
			m_VulkanPipelineState.DS.front.failOp = ToStr::Get(p.front.stencilFailOp);
			m_VulkanPipelineState.DS.front.depthFailOp = ToStr::Get(p.front.stencilDepthFailOp);
			m_VulkanPipelineState.DS.front.func = ToStr::Get(p.front.stencilCompareOp);

			m_VulkanPipelineState.DS.back.passOp = ToStr::Get(p.back.stencilPassOp);
			m_VulkanPipelineState.DS.back.failOp = ToStr::Get(p.back.stencilFailOp);
			m_VulkanPipelineState.DS.back.depthFailOp = ToStr::Get(p.back.stencilDepthFailOp);
			m_VulkanPipelineState.DS.back.func = ToStr::Get(p.back.stencilCompareOp);

			m_VulkanPipelineState.DS.state = state.dynamicDS;
			m_VulkanPipelineState.DS.minDepthBounds = c.m_DepthStencil[state.dynamicDS].minDepthBounds;
			m_VulkanPipelineState.DS.maxDepthBounds = c.m_DepthStencil[state.dynamicDS].maxDepthBounds;

			m_VulkanPipelineState.DS.front.ref = c.m_DepthStencil[state.dynamicDS].stencilFrontRef;
			m_VulkanPipelineState.DS.back.ref = c.m_DepthStencil[state.dynamicDS].stencilBackRef;

			m_VulkanPipelineState.DS.stencilReadMask = c.m_DepthStencil[state.dynamicDS].stencilReadMask;
			m_VulkanPipelineState.DS.stencilWriteMask = c.m_DepthStencil[state.dynamicDS].stencilWriteMask;

			// Renderpass
			m_VulkanPipelineState.Pass.renderpass.obj = state.renderPass;
			m_VulkanPipelineState.Pass.framebuffer.obj = state.framebuffer;

			m_VulkanPipelineState.Pass.framebuffer.width = c.m_Framebuffer[state.framebuffer].width;
			m_VulkanPipelineState.Pass.framebuffer.height = c.m_Framebuffer[state.framebuffer].height;
			m_VulkanPipelineState.Pass.framebuffer.layers = c.m_Framebuffer[state.framebuffer].layers;

			create_array_uninit(m_VulkanPipelineState.Pass.framebuffer.attachments, c.m_Framebuffer[state.framebuffer].attachments.size());
			for(size_t i=0; i < c.m_Framebuffer[state.framebuffer].attachments.size(); i++)
			{
				m_VulkanPipelineState.Pass.framebuffer.attachments[i].view = 
					c.m_Framebuffer[state.framebuffer].attachments[i].view;
			}

			m_VulkanPipelineState.Pass.renderArea.x = state.renderArea.offset.x;
			m_VulkanPipelineState.Pass.renderArea.y = state.renderArea.offset.y;
			m_VulkanPipelineState.Pass.renderArea.width = state.renderArea.extent.width;
			m_VulkanPipelineState.Pass.renderArea.height = state.renderArea.extent.height;
		}
	}
}

void VulkanReplay::FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	RDCUNIMPLEMENTED("FillCBufferVariables");
}

bool VulkanReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	RDCUNIMPLEMENTED("GetMinMax");
	return false;
}

bool VulkanReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
{
	RDCUNIMPLEMENTED("GetHistogram");
	return false;
}

void VulkanReplay::InitPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	VULKANNOTIMP("VulkanReplay::InitPostVSBuffers");
}

vector<EventUsage> VulkanReplay::GetUsage(ResourceId id)
{
	VULKANNOTIMP("GetUsage");
	return vector<EventUsage>();
}

void VulkanReplay::SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv)
{
	RDCUNIMPLEMENTED("SetContextFilter");
}

void VulkanReplay::FreeTargetResource(ResourceId id)
{
	RDCUNIMPLEMENTED("FreeTargetResource");
}

void VulkanReplay::FreeCustomShader(ResourceId id)
{
	RDCUNIMPLEMENTED("FreeCustomShader");
}

MeshFormat VulkanReplay::GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
	MeshFormat ret;
	RDCEraseEl(ret);

	VULKANNOTIMP("VulkanReplay::GetPostVSBuffers");

	return ret;
}

byte *VulkanReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize)
{
	RDCUNIMPLEMENTED("GetTextureData");
	return NULL;
}

void VulkanReplay::ReplaceResource(ResourceId from, ResourceId to)
{
	RDCUNIMPLEMENTED("ReplaceResource");
}

void VulkanReplay::RemoveReplacement(ResourceId id)
{
	RDCUNIMPLEMENTED("RemoveReplacement");
}

vector<uint32_t> VulkanReplay::EnumerateCounters()
{
	VULKANNOTIMP("EnumerateCounters");
	return vector<uint32_t>();
}

void VulkanReplay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
	RDCUNIMPLEMENTED("DescribeCounter");
}

vector<CounterResult> VulkanReplay::FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counters)
{
	RDCUNIMPLEMENTED("FetchCounters");
	return vector<CounterResult>();
}

void VulkanReplay::BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	RDCUNIMPLEMENTED("BuildTargetShader");
}

void VulkanReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	VULKANNOTIMP("BuildCustomShader");
}

vector<PixelModification> VulkanReplay::PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx)
{
	RDCUNIMPLEMENTED("VulkanReplay::PixelHistory");
	return vector<PixelModification>();
}

ShaderDebugTrace VulkanReplay::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	RDCUNIMPLEMENTED("DebugVertex");
	return ShaderDebugTrace();
}

ShaderDebugTrace VulkanReplay::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	RDCUNIMPLEMENTED("DebugPixel");
	return ShaderDebugTrace();
}

ShaderDebugTrace VulkanReplay::DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	RDCUNIMPLEMENTED("DebugThread");
	return ShaderDebugTrace();
}

ResourceId VulkanReplay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
{
	RDCUNIMPLEMENTED("ApplyCustomShader");
	return ResourceId();
}

ResourceId VulkanReplay::CreateProxyTexture( FetchTexture templateTex )
{
	RDCUNIMPLEMENTED("CreateProxyTexture");
	return ResourceId();
}

void VulkanReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
{
	RDCUNIMPLEMENTED("SetProxyTextureData");
}

ResourceId VulkanReplay::CreateProxyBuffer(FetchBuffer templateBuf)
{
	RDCUNIMPLEMENTED("CreateProxyBuffer");
	return ResourceId();
}

void VulkanReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
	RDCUNIMPLEMENTED("SetProxyTextureData");
}

// in vk_replay_platform.cpp
bool LoadVulkanLibrary();

static VkLayerDispatchTable replayDeviceTable = {0};
static VkLayerInstanceDispatchTable replayInstanceTable = {0};

VkLayerDispatchTable *dummyDeviceTable = NULL;
VkLayerInstanceDispatchTable *dummyInstanceTable = NULL;

#if !defined(WIN32)
#include <dlfcn.h>
#endif

ReplayCreateStatus Vulkan_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
	RDCDEBUG("Creating a VulkanReplay replay device");
	
	if(!LoadVulkanLibrary())
	{
		RDCERR("Failed to load vulkan library");
		return eReplayCreate_APIInitFailed;
	}
	
	VkInitParams initParams;
	RDCDriver driverType = RDC_Vulkan;
	string driverName = "VulkanReplay";
	if(logfile)
		RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, (RDCInitParams *)&initParams);
	
	if(initParams.SerialiseVersion != VkInitParams::VK_SERIALISE_VERSION)
	{
		RDCERR("Incompatible VulkanReplay serialise version, expected %d got %d", VkInitParams::VK_SERIALISE_VERSION, initParams.SerialiseVersion);
		return eReplayCreate_APIIncompatibleVersion;
	}

	dummyDeviceTable = &replayDeviceTable;
	dummyInstanceTable = &replayInstanceTable;

#ifdef WIN32

#define GetProcAddr(type, name) (type)GetProcAddress(LoadLibraryA("vulkan.0.dll"), STRINGIZE(CONCAT(vk, name)))

#else

	void *libhandle = dlopen("libvulkan.so", RTLD_NOW);

#define GetProcAddr(type, name) (type)dlsym(libhandle, STRINGIZE(CONCAT(vk, name)))

#endif

	{
		VkLayerDispatchTable &table = replayDeviceTable;

		table.GetDeviceProcAddr = GetProcAddr(PFN_vkGetDeviceProcAddr, GetDeviceProcAddr);
		table.CreateDevice = GetProcAddr(PFN_vkCreateDevice, CreateDevice);
		table.DestroyDevice = GetProcAddr(PFN_vkDestroyDevice, DestroyDevice);
		table.GetDeviceQueue = GetProcAddr(PFN_vkGetDeviceQueue, GetDeviceQueue);
		table.QueueSubmit = GetProcAddr(PFN_vkQueueSubmit, QueueSubmit);
		table.QueueWaitIdle = GetProcAddr(PFN_vkQueueWaitIdle, QueueWaitIdle);
		table.DeviceWaitIdle = GetProcAddr(PFN_vkDeviceWaitIdle, DeviceWaitIdle);
		table.AllocMemory = GetProcAddr(PFN_vkAllocMemory, AllocMemory);
		table.FreeMemory = GetProcAddr(PFN_vkFreeMemory, FreeMemory);
		table.MapMemory = GetProcAddr(PFN_vkMapMemory, MapMemory);
		table.UnmapMemory = GetProcAddr(PFN_vkUnmapMemory, UnmapMemory);
		table.FlushMappedMemoryRanges = GetProcAddr(PFN_vkFlushMappedMemoryRanges, FlushMappedMemoryRanges);
		table.InvalidateMappedMemoryRanges = GetProcAddr(PFN_vkInvalidateMappedMemoryRanges, InvalidateMappedMemoryRanges);
		table.GetDeviceMemoryCommitment = GetProcAddr(PFN_vkGetDeviceMemoryCommitment, GetDeviceMemoryCommitment);
		table.GetImageSparseMemoryRequirements = GetProcAddr(PFN_vkGetImageSparseMemoryRequirements, GetImageSparseMemoryRequirements);
		table.GetImageMemoryRequirements = GetProcAddr(PFN_vkGetImageMemoryRequirements, GetImageMemoryRequirements);
		table.GetBufferMemoryRequirements = GetProcAddr(PFN_vkGetBufferMemoryRequirements, GetBufferMemoryRequirements);
		table.BindImageMemory = GetProcAddr(PFN_vkBindImageMemory, BindImageMemory);
		table.BindBufferMemory = GetProcAddr(PFN_vkBindBufferMemory, BindBufferMemory);
		table.QueueBindSparseBufferMemory = GetProcAddr(PFN_vkQueueBindSparseBufferMemory, QueueBindSparseBufferMemory);
		table.QueueBindSparseImageOpaqueMemory = GetProcAddr(PFN_vkQueueBindSparseImageOpaqueMemory, QueueBindSparseImageOpaqueMemory);
		table.QueueBindSparseImageMemory = GetProcAddr(PFN_vkQueueBindSparseImageMemory, QueueBindSparseImageMemory);
		table.CreateFence = GetProcAddr(PFN_vkCreateFence, CreateFence);
		table.DestroyFence = GetProcAddr(PFN_vkDestroyFence, DestroyFence);
		table.GetFenceStatus = GetProcAddr(PFN_vkGetFenceStatus, GetFenceStatus);
		table.ResetFences = GetProcAddr(PFN_vkResetFences, ResetFences);
		table.WaitForFences = GetProcAddr(PFN_vkWaitForFences, WaitForFences);
		table.CreateSemaphore = GetProcAddr(PFN_vkCreateSemaphore, CreateSemaphore);
		table.DestroySemaphore = GetProcAddr(PFN_vkDestroySemaphore, DestroySemaphore);
		table.QueueSignalSemaphore = GetProcAddr(PFN_vkQueueSignalSemaphore, QueueSignalSemaphore);
		table.QueueWaitSemaphore = GetProcAddr(PFN_vkQueueWaitSemaphore, QueueWaitSemaphore);
		table.CreateEvent = GetProcAddr(PFN_vkCreateEvent, CreateEvent);
		table.DestroyEvent = GetProcAddr(PFN_vkDestroyEvent, DestroyEvent);
		table.GetEventStatus = GetProcAddr(PFN_vkGetEventStatus, GetEventStatus);
		table.SetEvent = GetProcAddr(PFN_vkSetEvent, SetEvent);
		table.ResetEvent = GetProcAddr(PFN_vkResetEvent, ResetEvent);
		table.CreateQueryPool = GetProcAddr(PFN_vkCreateQueryPool, CreateQueryPool);
		table.DestroyQueryPool = GetProcAddr(PFN_vkDestroyQueryPool, DestroyQueryPool);
		table.GetQueryPoolResults = GetProcAddr(PFN_vkGetQueryPoolResults, GetQueryPoolResults);
		table.CreateBuffer = GetProcAddr(PFN_vkCreateBuffer, CreateBuffer);
		table.DestroyBuffer = GetProcAddr(PFN_vkDestroyBuffer, DestroyBuffer);
		table.CreateBufferView = GetProcAddr(PFN_vkCreateBufferView, CreateBufferView);
		table.DestroyBufferView = GetProcAddr(PFN_vkDestroyBufferView, DestroyBufferView);
		table.CreateImage = GetProcAddr(PFN_vkCreateImage, CreateImage);
		table.DestroyImage = GetProcAddr(PFN_vkDestroyImage, DestroyImage);
		table.GetImageSubresourceLayout = GetProcAddr(PFN_vkGetImageSubresourceLayout, GetImageSubresourceLayout);
		table.CreateImageView = GetProcAddr(PFN_vkCreateImageView, CreateImageView);
		table.DestroyImageView = GetProcAddr(PFN_vkDestroyImageView, DestroyImageView);
		table.CreateAttachmentView = GetProcAddr(PFN_vkCreateAttachmentView, CreateAttachmentView);
		table.DestroyAttachmentView = GetProcAddr(PFN_vkDestroyAttachmentView, DestroyAttachmentView);
		table.CreateShaderModule = GetProcAddr(PFN_vkCreateShaderModule, CreateShaderModule);
		table.DestroyShaderModule = GetProcAddr(PFN_vkDestroyShaderModule, DestroyShaderModule);
		table.CreateShader = GetProcAddr(PFN_vkCreateShader, CreateShader);
		table.DestroyShader = GetProcAddr(PFN_vkDestroyShader, DestroyShader);
		table.CreatePipelineCache = GetProcAddr(PFN_vkCreatePipelineCache, CreatePipelineCache);
		table.DestroyPipelineCache = GetProcAddr(PFN_vkDestroyPipelineCache, DestroyPipelineCache);
		table.GetPipelineCacheSize = GetProcAddr(PFN_vkGetPipelineCacheSize, GetPipelineCacheSize);
		table.GetPipelineCacheData = GetProcAddr(PFN_vkGetPipelineCacheData, GetPipelineCacheData);
		table.MergePipelineCaches = GetProcAddr(PFN_vkMergePipelineCaches, MergePipelineCaches);
		table.CreateGraphicsPipelines = GetProcAddr(PFN_vkCreateGraphicsPipelines, CreateGraphicsPipelines);
		table.CreateComputePipelines = GetProcAddr(PFN_vkCreateComputePipelines, CreateComputePipelines);
		table.DestroyPipeline = GetProcAddr(PFN_vkDestroyPipeline, DestroyPipeline);
		table.CreatePipelineLayout = GetProcAddr(PFN_vkCreatePipelineLayout, CreatePipelineLayout);
		table.DestroyPipelineLayout = GetProcAddr(PFN_vkDestroyPipelineLayout, DestroyPipelineLayout);
		table.CreateSampler = GetProcAddr(PFN_vkCreateSampler, CreateSampler);
		table.DestroySampler = GetProcAddr(PFN_vkDestroySampler, DestroySampler);
		table.CreateDescriptorSetLayout = GetProcAddr(PFN_vkCreateDescriptorSetLayout, CreateDescriptorSetLayout);
		table.DestroyDescriptorSetLayout = GetProcAddr(PFN_vkDestroyDescriptorSetLayout, DestroyDescriptorSetLayout);
		table.CreateDescriptorPool = GetProcAddr(PFN_vkCreateDescriptorPool, CreateDescriptorPool);
		table.DestroyDescriptorPool = GetProcAddr(PFN_vkDestroyDescriptorPool, DestroyDescriptorPool);
		table.ResetDescriptorPool = GetProcAddr(PFN_vkResetDescriptorPool, ResetDescriptorPool);
		table.AllocDescriptorSets = GetProcAddr(PFN_vkAllocDescriptorSets, AllocDescriptorSets);
		table.FreeDescriptorSets = GetProcAddr(PFN_vkFreeDescriptorSets, FreeDescriptorSets);
		table.UpdateDescriptorSets = GetProcAddr(PFN_vkUpdateDescriptorSets, UpdateDescriptorSets);
		table.CreateDynamicViewportState = GetProcAddr(PFN_vkCreateDynamicViewportState, CreateDynamicViewportState);
		table.DestroyDynamicViewportState = GetProcAddr(PFN_vkDestroyDynamicViewportState, DestroyDynamicViewportState);
		table.CreateDynamicRasterState = GetProcAddr(PFN_vkCreateDynamicRasterState, CreateDynamicRasterState);
		table.DestroyDynamicRasterState = GetProcAddr(PFN_vkDestroyDynamicRasterState, DestroyDynamicRasterState);
		table.CreateDynamicColorBlendState = GetProcAddr(PFN_vkCreateDynamicColorBlendState, CreateDynamicColorBlendState);
		table.DestroyDynamicColorBlendState = GetProcAddr(PFN_vkDestroyDynamicColorBlendState, DestroyDynamicColorBlendState);
		table.CreateDynamicDepthStencilState = GetProcAddr(PFN_vkCreateDynamicDepthStencilState, CreateDynamicDepthStencilState);
		table.DestroyDynamicDepthStencilState = GetProcAddr(PFN_vkDestroyDynamicDepthStencilState, DestroyDynamicDepthStencilState);
		table.CreateFramebuffer = GetProcAddr(PFN_vkCreateFramebuffer, CreateFramebuffer);
		table.DestroyFramebuffer = GetProcAddr(PFN_vkDestroyFramebuffer, DestroyFramebuffer);
		table.CreateRenderPass = GetProcAddr(PFN_vkCreateRenderPass, CreateRenderPass);
		table.DestroyRenderPass = GetProcAddr(PFN_vkDestroyRenderPass, DestroyRenderPass);
		table.GetRenderAreaGranularity = GetProcAddr(PFN_vkGetRenderAreaGranularity, GetRenderAreaGranularity);
		table.CreateCommandPool = GetProcAddr(PFN_vkCreateCommandPool, CreateCommandPool);
		table.DestroyCommandPool = GetProcAddr(PFN_vkDestroyCommandPool, DestroyCommandPool);
		table.ResetCommandPool = GetProcAddr(PFN_vkResetCommandPool, ResetCommandPool);
		table.CreateCommandBuffer = GetProcAddr(PFN_vkCreateCommandBuffer, CreateCommandBuffer);
		table.DestroyCommandBuffer = GetProcAddr(PFN_vkDestroyCommandBuffer, DestroyCommandBuffer);
		table.BeginCommandBuffer = GetProcAddr(PFN_vkBeginCommandBuffer, BeginCommandBuffer);
		table.EndCommandBuffer = GetProcAddr(PFN_vkEndCommandBuffer, EndCommandBuffer);
		table.ResetCommandBuffer = GetProcAddr(PFN_vkResetCommandBuffer, ResetCommandBuffer);
		table.CmdBindPipeline = GetProcAddr(PFN_vkCmdBindPipeline, CmdBindPipeline);
		table.CmdBindDynamicViewportState = GetProcAddr(PFN_vkCmdBindDynamicViewportState, CmdBindDynamicViewportState);
		table.CmdBindDynamicRasterState = GetProcAddr(PFN_vkCmdBindDynamicRasterState, CmdBindDynamicRasterState);
		table.CmdBindDynamicColorBlendState = GetProcAddr(PFN_vkCmdBindDynamicColorBlendState, CmdBindDynamicColorBlendState);
		table.CmdBindDynamicDepthStencilState = GetProcAddr(PFN_vkCmdBindDynamicDepthStencilState, CmdBindDynamicDepthStencilState);
		table.CmdBindDescriptorSets = GetProcAddr(PFN_vkCmdBindDescriptorSets, CmdBindDescriptorSets);
		table.CmdBindVertexBuffers = GetProcAddr(PFN_vkCmdBindVertexBuffers, CmdBindVertexBuffers);
		table.CmdBindIndexBuffer = GetProcAddr(PFN_vkCmdBindIndexBuffer, CmdBindIndexBuffer);
		table.CmdDraw = GetProcAddr(PFN_vkCmdDraw, CmdDraw);
		table.CmdDrawIndexed = GetProcAddr(PFN_vkCmdDrawIndexed, CmdDrawIndexed);
		table.CmdDrawIndirect = GetProcAddr(PFN_vkCmdDrawIndirect, CmdDrawIndirect);
		table.CmdDrawIndexedIndirect = GetProcAddr(PFN_vkCmdDrawIndexedIndirect, CmdDrawIndexedIndirect);
		table.CmdDispatch = GetProcAddr(PFN_vkCmdDispatch, CmdDispatch);
		table.CmdDispatchIndirect = GetProcAddr(PFN_vkCmdDispatchIndirect, CmdDispatchIndirect);
		table.CmdCopyBuffer = GetProcAddr(PFN_vkCmdCopyBuffer, CmdCopyBuffer);
		table.CmdCopyImage = GetProcAddr(PFN_vkCmdCopyImage, CmdCopyImage);
		table.CmdBlitImage = GetProcAddr(PFN_vkCmdBlitImage, CmdBlitImage);
		table.CmdCopyBufferToImage = GetProcAddr(PFN_vkCmdCopyBufferToImage, CmdCopyBufferToImage);
		table.CmdCopyImageToBuffer = GetProcAddr(PFN_vkCmdCopyImageToBuffer, CmdCopyImageToBuffer);
		table.CmdUpdateBuffer = GetProcAddr(PFN_vkCmdUpdateBuffer, CmdUpdateBuffer);
		table.CmdFillBuffer = GetProcAddr(PFN_vkCmdFillBuffer, CmdFillBuffer);
		table.CmdClearColorImage = GetProcAddr(PFN_vkCmdClearColorImage, CmdClearColorImage);
		table.CmdClearDepthStencilImage = GetProcAddr(PFN_vkCmdClearDepthStencilImage, CmdClearDepthStencilImage);
		table.CmdClearColorAttachment = GetProcAddr(PFN_vkCmdClearColorAttachment, CmdClearColorAttachment);
		table.CmdClearDepthStencilAttachment = GetProcAddr(PFN_vkCmdClearDepthStencilAttachment, CmdClearDepthStencilAttachment);
		table.CmdResolveImage = GetProcAddr(PFN_vkCmdResolveImage, CmdResolveImage);
		table.CmdSetEvent = GetProcAddr(PFN_vkCmdSetEvent, CmdSetEvent);
		table.CmdResetEvent = GetProcAddr(PFN_vkCmdResetEvent, CmdResetEvent);
		table.CmdWaitEvents = GetProcAddr(PFN_vkCmdWaitEvents, CmdWaitEvents);
		table.CmdPipelineBarrier = GetProcAddr(PFN_vkCmdPipelineBarrier, CmdPipelineBarrier);
		table.CmdBeginQuery = GetProcAddr(PFN_vkCmdBeginQuery, CmdBeginQuery);
		table.CmdEndQuery = GetProcAddr(PFN_vkCmdEndQuery, CmdEndQuery);
		table.CmdResetQueryPool = GetProcAddr(PFN_vkCmdResetQueryPool, CmdResetQueryPool);
		table.CmdWriteTimestamp = GetProcAddr(PFN_vkCmdWriteTimestamp, CmdWriteTimestamp);
		table.CmdCopyQueryPoolResults = GetProcAddr(PFN_vkCmdCopyQueryPoolResults, CmdCopyQueryPoolResults);
		table.CmdPushConstants = GetProcAddr(PFN_vkCmdPushConstants, CmdPushConstants);
		table.CmdBeginRenderPass = GetProcAddr(PFN_vkCmdBeginRenderPass, CmdBeginRenderPass);
		table.CmdNextSubpass = GetProcAddr(PFN_vkCmdNextSubpass, CmdNextSubpass);
		table.CmdEndRenderPass = GetProcAddr(PFN_vkCmdEndRenderPass, CmdEndRenderPass);
		table.CmdExecuteCommands = GetProcAddr(PFN_vkCmdExecuteCommands, CmdExecuteCommands);
		table.GetSurfaceInfoWSI = GetProcAddr(PFN_vkGetSurfaceInfoWSI, GetSurfaceInfoWSI);
		table.CreateSwapChainWSI = GetProcAddr(PFN_vkCreateSwapChainWSI, CreateSwapChainWSI);
		table.DestroySwapChainWSI = GetProcAddr(PFN_vkDestroySwapChainWSI, DestroySwapChainWSI);
		table.GetSwapChainInfoWSI = GetProcAddr(PFN_vkGetSwapChainInfoWSI, GetSwapChainInfoWSI);
		table.AcquireNextImageWSI = GetProcAddr(PFN_vkAcquireNextImageWSI, AcquireNextImageWSI);
		table.QueuePresentWSI = GetProcAddr(PFN_vkQueuePresentWSI, QueuePresentWSI);
		table.DbgCreateMsgCallback = GetProcAddr(PFN_vkDbgCreateMsgCallback, DbgCreateMsgCallback);
		table.DbgDestroyMsgCallback = GetProcAddr(PFN_vkDbgDestroyMsgCallback, DbgDestroyMsgCallback);
	}

	{
		VkLayerInstanceDispatchTable &table = replayInstanceTable;

		table.GetInstanceProcAddr = GetProcAddr(PFN_vkGetInstanceProcAddr, GetInstanceProcAddr);
		table.CreateInstance = GetProcAddr(PFN_vkCreateInstance, CreateInstance);
		table.DestroyInstance = GetProcAddr(PFN_vkDestroyInstance, DestroyInstance);
		table.EnumeratePhysicalDevices = GetProcAddr(PFN_vkEnumeratePhysicalDevices, EnumeratePhysicalDevices);
		table.GetPhysicalDeviceFeatures = GetProcAddr(PFN_vkGetPhysicalDeviceFeatures, GetPhysicalDeviceFeatures);
		table.GetPhysicalDeviceImageFormatProperties = GetProcAddr(PFN_vkGetPhysicalDeviceImageFormatProperties, GetPhysicalDeviceImageFormatProperties);
		table.GetPhysicalDeviceFormatProperties = GetProcAddr(PFN_vkGetPhysicalDeviceFormatProperties, GetPhysicalDeviceFormatProperties);
		table.GetPhysicalDeviceLimits = GetProcAddr(PFN_vkGetPhysicalDeviceLimits, GetPhysicalDeviceLimits);
		table.GetPhysicalDeviceSparseImageFormatProperties = GetProcAddr(PFN_vkGetPhysicalDeviceSparseImageFormatProperties, GetPhysicalDeviceSparseImageFormatProperties);
		table.GetPhysicalDeviceProperties = GetProcAddr(PFN_vkGetPhysicalDeviceProperties, GetPhysicalDeviceProperties);
		table.GetPhysicalDeviceQueueCount = GetProcAddr(PFN_vkGetPhysicalDeviceQueueCount, GetPhysicalDeviceQueueCount);
		table.GetPhysicalDeviceQueueProperties = GetProcAddr(PFN_vkGetPhysicalDeviceQueueProperties, GetPhysicalDeviceQueueProperties);
		table.GetPhysicalDeviceMemoryProperties = GetProcAddr(PFN_vkGetPhysicalDeviceMemoryProperties, GetPhysicalDeviceMemoryProperties);
		table.GetPhysicalDeviceExtensionProperties = GetProcAddr(PFN_vkGetPhysicalDeviceExtensionProperties, GetPhysicalDeviceExtensionProperties);
		table.GetPhysicalDeviceLayerProperties = GetProcAddr(PFN_vkGetPhysicalDeviceLayerProperties, GetPhysicalDeviceLayerProperties);
		table.GetPhysicalDeviceSurfaceSupportWSI = GetProcAddr(PFN_vkGetPhysicalDeviceSurfaceSupportWSI, GetPhysicalDeviceSurfaceSupportWSI);
		table.DbgCreateMsgCallback = GetProcAddr(PFN_vkDbgCreateMsgCallback, DbgCreateMsgCallback);
		table.DbgDestroyMsgCallback = GetProcAddr(PFN_vkDbgDestroyMsgCallback, DbgDestroyMsgCallback);
	}
	
	if(initParams.APIVersion != VK_API_VERSION)
	{
		RDCLOG("Captured API version is not the same as RenderDoc's built version, expected %d got %d", VK_API_VERSION, initParams.APIVersion);
		RDCLOG("This isn't a problem as this information is optional, but RenderDoc will replay with its own API version");
	}

	WrappedVulkan *vk = new WrappedVulkan(logfile);
	vk->Initialise(initParams);
	
	RDCLOG("Created device.");
	VulkanReplay *replay = vk->GetReplay();
	replay->SetProxy(logfile == NULL);

	*driver = (IReplayDriver *)replay;

	return eReplayCreate_Success;
}

static DriverRegistration VkDriverRegistration(RDC_Vulkan, "Vulkan", &Vulkan_CreateReplayDevice);
