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
#include "vk_debug.h"
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

struct genericuniforms
{
	Vec4f Offset;
	Vec4f Scale;
	Vec4f Color;
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

	VkImageMemoryBarrier t = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		VK_NULL_HANDLE,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	for(size_t i=0; i < ARRAY_COUNT(coltrans); i++)
		coltrans[i] = t;

	bbtrans = t;

	t.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthtrans = t;

	t.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
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
	const VkLayerDispatchTable *vt = ObjDisp(device);

	vt->DeviceWaitIdle(Unwrap(device));
	
	if(bb != VK_NULL_HANDLE)
	{
		vt->DestroyRenderPass(Unwrap(device), Unwrap(renderpass));
		renderpass = VK_NULL_HANDLE;
		
		vt->DestroyImage(Unwrap(device), Unwrap(bb));
		vt->DestroyImageView(Unwrap(device), Unwrap(bbview));
		vt->FreeMemory(Unwrap(device), Unwrap(bbmem));
		vt->DestroyFramebuffer(Unwrap(device), Unwrap(fb));
		
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
		vt->DestroyImageView(Unwrap(device), Unwrap(dsview));
		vt->DestroyImage(Unwrap(device), Unwrap(dsimg));
		vt->FreeMemory(Unwrap(device), Unwrap(dsmem));
		vt->DestroyFramebuffer(Unwrap(device), Unwrap(fbdepth));
		
		dsview = VK_NULL_HANDLE;
		dsimg = VK_NULL_HANDLE;
		dsmem = VK_NULL_HANDLE;
		fbdepth = VK_NULL_HANDLE;
	}

	if(swap != VK_NULL_HANDLE)
		vt->DestroySwapchainKHR(Unwrap(device), Unwrap(swap));
}

void VulkanReplay::OutputWindow::Create(WrappedVulkan *driver, VkDevice device, bool depth)
{
	const VkLayerDispatchTable *vt = ObjDisp(device);

	// save the old swapchain so it isn't destroyed
	VkSwapchainKHR old = swap;
	swap = VK_NULL_HANDLE;

	Destroy(driver, device);

	void *handleptr = NULL;
	void *wndptr = NULL;
	VkPlatformKHR platform = VK_PLATFORM_MAX_ENUM_KHR;

	VkSurfaceDescriptionWindowKHR surfDesc = { VK_STRUCTURE_TYPE_SURFACE_DESCRIPTION_WINDOW_KHR };

	InitSurfaceDescription(surfDesc);

	// sensible defaults
	VkFormat imformat = VK_FORMAT_B8G8R8A8_UNORM;
	VkPresentModeKHR presentmode = VK_PRESENT_MODE_FIFO_KHR;
	VkColorSpaceKHR imcolspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	VkResult vkr = VK_SUCCESS;

	// check format and present mode from driver
	{
		uint32_t numFormats = 0;

		vkr = vt->GetSurfaceFormatsKHR(Unwrap(device), (const VkSurfaceDescriptionKHR *)&surfDesc, &numFormats, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW make sure whole pipeline is SRGB correct
		if(numFormats > 0)
		{
			VkSurfaceFormatKHR *formats = new VkSurfaceFormatKHR[numFormats];

			vkr = vt->GetSurfaceFormatsKHR(Unwrap(device), (const VkSurfaceDescriptionKHR *)&surfDesc, &numFormats, formats);
			RDCASSERT(vkr == VK_SUCCESS);

			if(numFormats == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
			{
				// 1 entry with undefined means no preference, just use our default
				imformat = VK_FORMAT_B8G8R8A8_UNORM;
				imcolspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
			}
			else
			{
				// try and find a format with SRGB correction
				imformat = VK_FORMAT_UNDEFINED;
				imcolspace = formats[0].colorSpace;

				for(uint32_t i=0; i < numFormats; i++)
				{
					if(IsSRGBFormat(formats[i].format))
					{
						imformat = formats[i].format;
						imcolspace = formats[i].colorSpace;
						RDCASSERT(imcolspace == VK_COLORSPACE_SRGB_NONLINEAR_KHR);
						break;
					}
				}

				if(imformat == VK_FORMAT_UNDEFINED)
				{
					RDCWARN("Couldn't find SRGB correcting output swapchain format");
					imformat = formats[0].format;
				}
			}

			SAFE_DELETE_ARRAY(formats);
		}

		uint32_t numModes = 0;

		vkr = vt->GetSurfacePresentModesKHR(Unwrap(device), (const VkSurfaceDescriptionKHR *)&surfDesc, &numModes, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		if(numModes > 0)
		{
			VkPresentModeKHR *modes = new VkPresentModeKHR[numModes];

			vkr = vt->GetSurfacePresentModesKHR(Unwrap(device), (const VkSurfaceDescriptionKHR *)&surfDesc, &numModes, modes);
			RDCASSERT(vkr == VK_SUCCESS);

			// If mailbox mode is available, use it, as is the lowest-latency non-
			// tearing mode.  If not, try IMMEDIATE which will usually be available,
			// and is fastest (though it tears).  If not, fall back to FIFO which is
			// always available.
			for (size_t i = 0; i < numModes; i++)
			{
				if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					presentmode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}

				if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
					presentmode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}

			SAFE_DELETE_ARRAY(modes);
		}
	}

	uint32_t idx = 0;

	VkSwapchainCreateInfoKHR swapInfo = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, (VkSurfaceDescriptionKHR *)&surfDesc,
			2, imformat, imcolspace, { width, height },
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
			VK_SURFACE_TRANSFORM_NONE_KHR, 1,
			VK_SHARING_MODE_EXCLUSIVE, 0, &idx,
			presentmode,
			Unwrap(old), true,
	};

	vkr = vt->CreateSwapchainKHR(Unwrap(device), &swapInfo, &swap);
	RDCASSERT(vkr == VK_SUCCESS);

	VKMGR()->WrapResource(Unwrap(device), swap);

	if(old != VK_NULL_HANDLE)
		vt->DestroySwapchainKHR(Unwrap(device), Unwrap(old));

	vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swap), &numImgs, NULL);
	RDCASSERT(vkr == VK_SUCCESS);

	VkImage* imgs = new VkImage[numImgs];
	vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swap), &numImgs, imgs);
	RDCASSERT(vkr == VK_SUCCESS);

	for(size_t i=0; i < numImgs; i++)
	{
		colimg[i] = imgs[i];
		VKMGR()->WrapResource(Unwrap(device), colimg[i]);
		coltrans[i].image = Unwrap(colimg[i]);
		coltrans[i].oldLayout = coltrans[i].newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	curidx = 0;

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

		vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, &renderpass);
		RDCASSERT(vkr == VK_SUCCESS);

		VKMGR()->WrapResource(Unwrap(device), renderpass);
	}

	{
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, imformat, { width, height, 1 },
			1, 1, 1,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			0, VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		VkResult vkr = vt->CreateImage(Unwrap(device), &imInfo, &bb);
		RDCASSERT(vkr == VK_SUCCESS);

		VKMGR()->WrapResource(Unwrap(device), bb);

		VkMemoryRequirements mrq = {0};

		vkr = vt->GetImageMemoryRequirements(Unwrap(device), Unwrap(bb), &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(Unwrap(device), &allocInfo, &bbmem);
		RDCASSERT(vkr == VK_SUCCESS);

		VKMGR()->WrapResource(Unwrap(device), bbmem);

		vkr = vt->BindImageMemory(Unwrap(device), Unwrap(bb), Unwrap(bbmem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		bbtrans.image = Unwrap(bb);
		bbtrans.oldLayout = bbtrans.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	{
		VkImageViewCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(bb), VK_IMAGE_VIEW_TYPE_2D,
			imformat,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
			0
		};

		vkr = vt->CreateImageView(Unwrap(device), &info, &bbview);
		RDCASSERT(vkr == VK_SUCCESS);

		VKMGR()->WrapResource(Unwrap(device), bbview);

		VkFramebufferCreateInfo fbinfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
			Unwrap(renderpass),
			1, UnwrapPtr(bbview),
			(uint32_t)width, (uint32_t)height, 1,
		};

		vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, &fb);
		RDCASSERT(vkr == VK_SUCCESS);

		VKMGR()->WrapResource(Unwrap(device), fb);
	}

	if(dsimg != VK_NULL_HANDLE)
	{
		VkImageViewCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(dsimg), VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 },
			0
		};

		vkr = vt->CreateImageView(Unwrap(device), &info, &dsview);
		RDCASSERT(vkr == VK_SUCCESS);

		VKMGR()->WrapResource(Unwrap(device), dsview);
	}
}

VulkanReplay::VulkanReplay()
{
	m_pDriver = NULL;
	m_Proxy = false;

	m_OutputWinID = 1;
	m_ActiveWinID = 0;
	m_BindDepth = false;

	m_DebugWidth = m_DebugHeight = 1;
}

VulkanDebugManager *VulkanReplay::GetDebugManager()
{
	return m_pDriver->GetDebugManager();
}

void VulkanReplay::Shutdown()
{
	delete m_pDriver;
}

APIProperties VulkanReplay::GetAPIProperties()
{
	APIProperties ret;

	ret.pipelineType = ePipelineState_Vulkan;
	ret.degraded = false;

	return ret;
}

void VulkanReplay::ReadLogInitialisation()
{
	m_pDriver->ReadLogInitialisation();
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
	m_pDriver->GetMainSerialiser()->InitCallstackResolver();
}

bool VulkanReplay::HasCallstacks()
{
	return m_pDriver->GetMainSerialiser()->HasCallstacks();
}

Callstack::StackResolver *VulkanReplay::GetCallstackResolver()
{
	return m_pDriver->GetMainSerialiser()->GetCallstackResolver();
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

	for(auto it = m_pDriver->m_ImageInfo.begin(); it != m_pDriver->m_ImageInfo.end(); ++it)
	{
		// skip textures that aren't from the capture
		if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
			 continue;

		texs.push_back(it->first);
	}

	return texs;
}
	
vector<ResourceId> VulkanReplay::GetBuffers()
{
	VULKANNOTIMP("GetBuffers");
	return vector<ResourceId>();
}

void VulkanReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4])
{
	int oldW = m_DebugWidth, oldH = m_DebugHeight;

	m_DebugWidth = m_DebugHeight = 1;

	// render picked pixel to readback F32 RGBA texture
	{
		TextureDisplay texDisplay;

		texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
		texDisplay.HDRMul = -1.0f;
		texDisplay.linearDisplayAsGamma = true;
		texDisplay.FlipY = false;
		texDisplay.mip = mip;
		texDisplay.sampleIdx = sample;
		texDisplay.CustomShader = ResourceId();
		texDisplay.sliceFace = sliceFace;
		texDisplay.rangemin = 0.0f;
		texDisplay.rangemax = 1.0f;
		texDisplay.scale = 1.0f;
		texDisplay.texid = texture;
		texDisplay.rawoutput = true;
		texDisplay.offx = -float(x);
		texDisplay.offy = -float(y);
		
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			Unwrap(GetDebugManager()->m_PickPixelRP), Unwrap(GetDebugManager()->m_PickPixelFB),
			{ { 0, 0, }, { 1, 1 } },
			1, &clearval,
		};

		RenderTextureInternal(texDisplay, rpbegin, false);
	}

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkResult vkr = VK_SUCCESS;

	{
		VkImageMemoryBarrier pickimTrans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			Unwrap(GetDebugManager()->m_PickPixelImage),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		// transition from color attachment to transfer source, with proper memory barriers
		pickimTrans.outputMask = VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT;
		pickimTrans.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		void *barrier = (void *)&pickimTrans;
		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
		pickimTrans.oldLayout = pickimTrans.newLayout;

		pickimTrans.outputMask = 0;
		pickimTrans.inputMask = 0;

		// do copy
		VkBufferImageCopy region = {
			0, 128, 1,
			{ VK_IMAGE_ASPECT_COLOR, 0, 0}, { 0, 0, 0 },
			{ 1, 1, 1 },
		};
		vt->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_PickPixelImage), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.buf), 1, &region);

		// transition back to color attachment
		pickimTrans.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

		vt->EndCommandBuffer(Unwrap(cmd));

		vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);

		vt->QueueWaitIdle(Unwrap(q));
	}

	float *pData = NULL;
	vt->MapMemory(Unwrap(dev), Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.mem), 0, 0, 0, (void **)&pData);

	RDCASSERT(pData != NULL);

	if(pData == NULL)
	{
		RDCERR("Failed ot map readback buffer memory");
	}
	else
	{
		pixel[0] = pData[0];
		pixel[1] = pData[1];
		pixel[2] = pData[2];
		pixel[3] = pData[3];
	}

	vt->UnmapMemory(Unwrap(dev), Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.mem));
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
	
	VkClearValue clearval = {0};
	VkRenderPassBeginInfo rpbegin = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		Unwrap(outw.renderpass), Unwrap(outw.fb),
		{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
		1, &clearval,
	};

	return RenderTextureInternal(cfg, rpbegin, true);
}

bool VulkanReplay::RenderTextureInternal(TextureDisplay cfg, VkRenderPassBeginInfo rpbegin, bool blendAlpha)
{
	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	const ImgState &iminfo = m_pDriver->m_ImageInfo[cfg.texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(cfg.texid);

	// VKTODOMED handle multiple subresources with different layouts etc
	VkImageLayout origLayout = iminfo.subresourceStates[0].state;
	VkImageView liveImView = VK_NULL_HANDLE;

	// VKTODOLOW this view should be cached
	{
		VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(liveIm), VK_IMAGE_VIEW_TYPE_2D,
			iminfo.format,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR, 0, RDCMAX(1, iminfo.mipLevels), 0, 1, },
			0
		};

		// VKTODOMED used for texture display, but eventually will have to be created on the fly
		// for whichever image we're viewing (and cached), not specifically created here.
		VkResult vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, &liveImView);
		RDCASSERT(vkr == VK_SUCCESS);

		m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), liveImView);
	}
	
	// VKTODOHIGH once we stop doing DeviceWaitIdle/QueueWaitIdle all over, this
	// needs to be ring-buffered
	displayuniforms *data = (displayuniforms *)GetDebugManager()->m_TexDisplayUBO.Map(vt, dev);

	data->Padding = 0;
	
	float x = cfg.offx;
	float y = cfg.offy;
	
	data->Position.x = x;
	data->Position.y = y;
	data->Scale = cfg.scale;
	data->HDRMul = -1.0f;

	int32_t tex_x = iminfo.extent.width;
	int32_t tex_y = iminfo.extent.height;
	int32_t tex_z = iminfo.extent.depth;

	if(cfg.scale <= 0.0f)
	{
		float xscale = float(m_DebugWidth)/float(tex_x);
		float yscale = float(m_DebugHeight)/float(tex_y);

		float scale = data->Scale = RDCMIN(xscale, yscale);

		if(yscale > xscale)
		{
			data->Position.x = 0;
			data->Position.y = (float(m_DebugWidth)-(tex_y*scale) )*0.5f;
		}
		else
		{
			data->Position.y = 0;
			data->Position.x = (float(m_DebugHeight)-(tex_x*scale) )*0.5f;
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

	data->OutputRes.x = (float)m_DebugWidth;
	data->OutputRes.y = (float)m_DebugHeight;

	// VKTODOMED handle different texture types/displays
	data->OutputDisplayFormat = 0;
	
	if(!IsSRGBFormat(iminfo.format) && cfg.linearDisplayAsGamma)
	{
		data->OutputDisplayFormat |= 1; // VKTODOMED constants TEXDISPLAY_GAMMA_CURVE;
	}
	
	data->RawOutput = cfg.rawoutput ? 1 : 0;

	GetDebugManager()->m_TexDisplayUBO.Unmap(vt, dev);
	
	VkDescriptorInfo desc = {0};
	desc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc.imageView = Unwrap(liveImView);
	desc.sampler = Unwrap(GetDebugManager()->m_PointSampler);
	if(cfg.mip == 0 && cfg.scale < 1.0f)
		desc.sampler = Unwrap(GetDebugManager()->m_LinearSampler);

	VkWriteDescriptorSet writeSet = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
		Unwrap(GetDebugManager()->m_TexDisplayDescSet), 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc
	};

	vt->UpdateDescriptorSets(Unwrap(dev), 1, &writeSet, 0, NULL);

	VkImageMemoryBarrier srcimTrans = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, origLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	vt->ResetCommandBuffer(Unwrap(cmd), 0);
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	void *barrier = (void *)&srcimTrans;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
	srcimTrans.oldLayout = srcimTrans.newLayout;

	{
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		bool noblend = !cfg.rawoutput || !blendAlpha || cfg.CustomShader != ResourceId();

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, noblend ? Unwrap(GetDebugManager()->m_TexDisplayPipeline) : Unwrap(GetDebugManager()->m_TexDisplayBlendPipeline));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_TexDisplayPipeLayout), 0, 1, UnwrapPtr(GetDebugManager()->m_TexDisplayDescSet), 0, NULL);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);

		vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	srcimTrans.newLayout = origLayout;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vt->EndCommandBuffer(Unwrap(cmd));

	vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);

	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(Unwrap(q));

	vt->DestroyImageView(Unwrap(dev), Unwrap(liveImView));
	VKMGR()->ReleaseWrappedResource(liveImView);

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
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOHIGH once we stop doing DeviceWaitIdle/QueueWaitIdle all over, this
	// needs to be ring-buffered
	Vec4f *data = (Vec4f *)GetDebugManager()->m_CheckerboardUBO.Map(vt, dev);
	data[0].x = light.x;
	data[0].y = light.y;
	data[0].z = light.z;
	data[1].x = dark.x;
	data[1].y = dark.y;
	data[1].z = dark.z;
	GetDebugManager()->m_CheckerboardUBO.Unmap(vt, dev);

	{
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			Unwrap(outw.renderpass), Unwrap(outw.fb),
			{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_CheckerboardPipeline));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_CheckerboardPipeLayout), 0, 1, UnwrapPtr(GetDebugManager()->m_CheckerboardDescSet), 0, NULL);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);
		
		vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(Unwrap(q));
}
	
void VulkanReplay::RenderHighlightBox(float w, float h, float scale)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);
	
	const float xpixdim = 2.0f/w;
	const float ypixdim = 2.0f/h;
	
	const float xdim = scale*xpixdim;
	const float ydim = scale*ypixdim;

	// VKTODOHIGH once we stop doing DeviceWaitIdle/QueueWaitIdle all over, this
	// needs to be ring-buffered
	genericuniforms *data = (genericuniforms *)GetDebugManager()->m_GenericUBO.Map(vt, dev);
	data->Offset = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
	data->Scale = Vec4f(xdim, ydim, 1.0f, 1.0f);
	data->Color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
	GetDebugManager()->m_GenericUBO.Unmap(vt, dev);

	{
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			Unwrap(outw.renderpass), Unwrap(outw.fb),
			{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_GenericPipeline));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_GenericPipeLayout), 0, 1, UnwrapPtr(GetDebugManager()->m_GenericDescSet), 0, NULL);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);

		VkDeviceSize zero = 0;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_OutlineStripVBO.buf), &zero);
		
		vt->CmdDraw(Unwrap(cmd), 8, 1, 0, 0);

		genericuniforms secondOutline;
		secondOutline.Offset = Vec4f(-xpixdim, ypixdim, 0.0f, 0.0f);
		secondOutline.Scale = Vec4f(xdim+xpixdim*2, ydim+ypixdim*2, 1.0f, 1.0f);
		secondOutline.Color = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

		vt->CmdUpdateBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_GenericUBO.buf), 0, sizeof(genericuniforms), (uint32_t *)&secondOutline);
		
		vt->CmdDraw(Unwrap(cmd), 8, 1, 0, 0);

		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
	RDCASSERT(vkr == VK_SUCCESS);

	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(Unwrap(q));
}

ResourceId VulkanReplay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	RDCUNIMPLEMENTED("RenderOverlay");
	return ResourceId();
}
	
void VulkanReplay::RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg)
{
	VULKANNOTIMP("RenderMesh");
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

	m_DebugWidth = (int32_t)outw.width;
	m_DebugHeight = (int32_t)outw.height;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = ObjDisp(dev);
	
	// semaphore is short lived, so not wrapped, if it's cached (ideally)
	// then it should be wrapped
	VkSemaphore sem;
	VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT };

	VkResult vkr = vt->CreateSemaphore(Unwrap(dev), &semInfo, &sem);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->AcquireNextImageKHR(Unwrap(dev), Unwrap(outw.swap), UINT64_MAX, sem, &outw.curidx);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->QueueWaitSemaphore(Unwrap(q), sem);
	RDCASSERT(vkr == VK_SUCCESS);

	vt->DestroySemaphore(Unwrap(dev), sem);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier[] = {
		(void *)&outw.bbtrans,
		(void *)&outw.coltrans[outw.curidx],
	};
	
	outw.bbtrans.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.coltrans[outw.curidx].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, barrier);

	outw.bbtrans.oldLayout = outw.bbtrans.newLayout;
	outw.coltrans[outw.curidx].oldLayout = outw.bbtrans.newLayout;

	vt->EndCommandBuffer(Unwrap(cmd));

	vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
	
	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(Unwrap(q));
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
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	vt->CmdClearColorImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)col, 1, &outw.bbtrans.subresourceRange);

	vt->EndCommandBuffer(Unwrap(cmd));

	vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
	
	// VKTODOMED ideally all the commands from Bind to Flip would be recorded
	// into a single command buffer and we can just have several allocated
	// ring-buffer style
	vt->QueueWaitIdle(Unwrap(q));
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
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
	RDCASSERT(vkr == VK_SUCCESS);
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier[] = {
		(void *)&outw.bbtrans,
		(void *)&outw.coltrans[outw.curidx],
	};
	
	outw.bbtrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, barrier);
	outw.bbtrans.oldLayout = outw.bbtrans.newLayout;

	VkImageCopy cpy = {
		{ VK_IMAGE_ASPECT_COLOR, 0, 0 },
		{ 0, 0, 0 },
		{ VK_IMAGE_ASPECT_COLOR, 0, 0 },
		{ 0, 0, 0 },
		{ outw.width, outw.height, 1 },
	};

	vt->CmdCopyImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, Unwrap(outw.colimg[outw.curidx]), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &cpy);
	
	outw.bbtrans.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.coltrans[outw.curidx].newLayout = VK_IMAGE_LAYOUT_PRESENT_SOURCE_KHR;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, barrier);

	outw.bbtrans.oldLayout = outw.bbtrans.newLayout;
	outw.coltrans[outw.curidx].oldLayout = outw.coltrans[outw.curidx].newLayout;

	vt->EndCommandBuffer(Unwrap(cmd));
	
	vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, UnwrapPtr(outw.swap), &outw.curidx };

	vt->QueuePresentKHR(Unwrap(q), &presentInfo);

	vt->QueueWaitIdle(Unwrap(q));

	vt->DeviceWaitIdle(Unwrap(dev));
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
	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetCmd();
	VkQueue q = m_pDriver->GetQ();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ResourceId memid;
	
	{
		auto it = m_pDriver->m_BufferMemBinds.find(buff);
		if(it == m_pDriver->m_BufferMemBinds.end())
		{
			RDCWARN("Buffer has no memory bound, or no buffer of this ID");
			return vector<byte>();
		}

		memid = it->second;
	}

	VkBuffer srcBuf = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(buff);
	
	if(len == 0)
	{
		len = uint32_t(m_pDriver->m_MemoryInfo[memid].size - offset);
	}

	if(len > 0 && VkDeviceSize(offset+len) > m_pDriver->m_MemoryInfo[memid].size)
	{
		RDCWARN("Attempting to read off the end of the array. Will be clamped");
		len = RDCMIN(len, uint32_t(m_pDriver->m_MemoryInfo[memid].size - offset));
	}

	vector<byte> ret;
	ret.resize(len);

	// VKTODOMED - coarse: wait for all writes to this buffer
	vt->DeviceWaitIdle(Unwrap(dev));

	VkDeviceMemory readbackmem = VK_NULL_HANDLE;
	VkBuffer destbuf = VK_NULL_HANDLE;
	VkResult vkr = VK_SUCCESS;
	byte *pData = NULL;

	{
		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			(VkDeviceSize)ret.size(), VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT|VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		VkResult vkr = vt->CreateBuffer(Unwrap(dev), &bufInfo, &destbuf);
		RDCASSERT(vkr == VK_SUCCESS);
		
		VkMemoryRequirements mrq;
		vkr = vt->GetBufferMemoryRequirements(Unwrap(dev), destbuf, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &readbackmem);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->BindBufferMemory(Unwrap(dev), destbuf, readbackmem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { offset, 0, (VkDeviceSize)ret.size() };
		vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), destbuf, 1, &region);

		vkr = vt->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = vt->QueueWaitIdle(Unwrap(q));
		RDCASSERT(vkr == VK_SUCCESS);
	}

	vkr = vt->MapMemory(Unwrap(dev), readbackmem, 0, 0, 0, (void **)&pData);
	RDCASSERT(vkr == VK_SUCCESS);
	
	RDCASSERT(pData != NULL);
	memcpy(&ret[0], pData, ret.size());

	vt->UnmapMemory(Unwrap(dev), readbackmem);
	
	vt->DeviceWaitIdle(Unwrap(dev));

	vt->DestroyBuffer(Unwrap(dev), destbuf);
	vt->FreeMemory(Unwrap(dev), readbackmem);

	return ret;
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
	
	const ImgState &iminfo = m_pDriver->m_ImageInfo[id];

	FetchTexture ret;
	ret.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);
	ret.arraysize = iminfo.arraySize;
	ret.creationFlags = iminfo.creationFlags;
	ret.cubemap = iminfo.cube;
	ret.width = iminfo.extent.width;
	ret.height = iminfo.extent.height;
	ret.depth = iminfo.extent.depth;
	ret.mips = iminfo.mipLevels;
	ret.numSubresources = ret.mips*ret.arraysize;
	
	ret.byteSize = 0;
	for(uint32_t s=0; s < ret.mips; s++)
		ret.byteSize += GetByteSize(ret.width, ret.height, ret.depth, iminfo.format, s);
	ret.byteSize *= ret.arraysize;

	ret.msQual = 0;
	ret.msSamp = iminfo.samples;

	ret.format = MakeResourceFormat(iminfo.format);

	switch(iminfo.type)
	{
		case VK_IMAGE_TYPE_1D:
			ret.resType = iminfo.arraySize > 1 ? eResType_Texture1DArray : eResType_Texture1D;
			ret.dimension = 1;
			break;
		case VK_IMAGE_TYPE_2D:
			     if(ret.msSamp > 1) ret.resType = iminfo.arraySize > 1 ? eResType_Texture2DMSArray : eResType_Texture2DMS;
			else if(ret.cubemap)    ret.resType = iminfo.arraySize > 6 ? eResType_TextureCubeArray : eResType_TextureCube;
			else                    ret.resType = iminfo.arraySize > 1 ? eResType_Texture2DArray : eResType_Texture2D;
			ret.dimension = 2;
			break;
		case VK_IMAGE_TYPE_3D:
			ret.resType = eResType_Texture3D;
			ret.dimension = 3;
			break;
		default:
			RDCERR("Unexpected image type");
			break;
	}

	ret.customName = true;
	ret.name = m_pDriver->m_ObjectNames[id];
	if(ret.name.count == 0)
	{
		ret.customName = false;
		
		const char *suffix = "";
		const char *ms = "";

		if(ret.msSamp > 1)
			ms = "MS";

		if(ret.creationFlags & eTextureCreate_RTV)
			suffix = " RTV";
		if(ret.creationFlags & eTextureCreate_DSV)
			suffix = " DSV";

		if(ret.cubemap)
		{
			if(ret.arraysize > 6)
				ret.name = StringFormat::Fmt("TextureCube%sArray%s %llu", ms, suffix, ret.ID);
			else
				ret.name = StringFormat::Fmt("TextureCube%s%s %llu", ms, suffix, ret.ID);
		}
		else
		{
			if(ret.arraysize > 1)
				ret.name = StringFormat::Fmt("Texture%dD%sArray%s %llu", ret.dimension, ms, suffix, ret.ID);
			else
				ret.name = StringFormat::Fmt("Texture%dD%s%s %llu", ret.dimension, ms, suffix, ret.ID);
		}
	}

	return ret;
}

FetchBuffer VulkanReplay::GetBuffer(ResourceId id)
{
	RDCUNIMPLEMENTED("GetBuffer");
	return FetchBuffer();
}

ShaderReflection *VulkanReplay::GetShader(ResourceId id)
{
	auto it = m_pDriver->m_ShaderInfo.find(id);
	
	if(it == m_pDriver->m_ShaderInfo.end())
	{
		RDCERR("Can't get shader details");
		return NULL;
	}

	// disassemble lazily on demand
	if(it->second.refl.Disassembly.count == 0)
	{
		if(m_pDriver->m_ShaderModuleInfo[it->second.module].spirv.m_Disassembly.empty())
			m_pDriver->m_ShaderModuleInfo[it->second.module].spirv.Disassemble();

		it->second.refl.Disassembly = m_pDriver->m_ShaderModuleInfo[it->second.module].spirv.m_Disassembly;
	}

	return &it->second.refl;
}

void VulkanReplay::SavePipelineState()
{
	VULKANNOTIMP("SavePipelineState");

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
				stages[i]->Shader = rm->GetOriginalID(p.shaders[i]);
				stages[i]->ShaderDetails = NULL;
				stages[i]->customName = false;
				stages[i]->ShaderName = StringFormat::Fmt("Shader %llu", stages[i]->Shader);
				stages[i]->stage = ShaderStageType(eShaderStage_Vertex + i);
				stages[i]->BindpointMapping = m_pDriver->m_ShaderInfo[p.shaders[i]].mapping;
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

						dst.descset = src;
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
							
							create_array_uninit(dst.bindings[b].binds, layoutBind.arraySize);
							for(uint32_t a=0; a < layoutBind.arraySize; a++)
							{
								if(layoutBind.immutableSampler)
									dst.bindings[b].binds[a].sampler = layoutBind.immutableSampler[a];
								else if(info->sampler != VK_NULL_HANDLE)
									dst.bindings[b].binds[a].sampler = rm->GetOriginalID(VKMGR()->GetNonDispWrapper(info->sampler)->id);

								// only one of these is ever set
								if(info->imageView != VK_NULL_HANDLE)
								{
									dst.bindings[b].binds[a].view = rm->GetOriginalID(VKMGR()->GetNonDispWrapper(info->imageView)->id);
									dst.bindings[b].binds[a].res = rm->GetOriginalID(c.m_ImageView[dst.bindings[b].binds[a].view].image);
								}
								if(info->bufferView != VK_NULL_HANDLE)
								{
									dst.bindings[b].binds[a].view = rm->GetOriginalID(VKMGR()->GetNonDispWrapper(info->bufferView)->id);
									dst.bindings[b].binds[a].res = rm->GetOriginalID(c.m_BufferView[dst.bindings[b].binds[a].view].buffer);
									dst.bindings[b].binds[a].offset = *(uint32_t *)&info->imageLayout;
									dst.bindings[b].binds[a].offset += c.m_BufferView[dst.bindings[b].binds[a].view].offset;
									dst.bindings[b].binds[a].size = c.m_BufferView[dst.bindings[b].binds[a].view].size;
								}
								if(info->bufferInfo.buffer != VK_NULL_HANDLE)
								{
									dst.bindings[b].binds[a].view = ResourceId();
									dst.bindings[b].binds[a].res = rm->GetOriginalID(VKMGR()->GetNonDispWrapper(info->bufferInfo.buffer)->id);
								}
							}
						}
					}
				}
			}

			// Tessellation
			m_VulkanPipelineState.Tess.numControlPoints = p.patchControlPoints;

			// Viewport/Scissors
			create_array_uninit(m_VulkanPipelineState.VP.viewportScissors, RDCMIN(state.views.size(), state.scissors.size()));
			for(size_t i=0; i < state.views.size(); i++)
			{
				m_VulkanPipelineState.VP.viewportScissors[i].vp.x = state.views[i].originX;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.y = state.views[i].originY;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.width = state.views[i].width;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.height = state.views[i].height;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.minDepth = state.views[i].minDepth;
				m_VulkanPipelineState.VP.viewportScissors[i].vp.maxDepth = state.views[i].maxDepth;

				m_VulkanPipelineState.VP.viewportScissors[i].scissor.x = state.scissors[i].offset.x;
				m_VulkanPipelineState.VP.viewportScissors[i].scissor.y = state.scissors[i].offset.y;
				m_VulkanPipelineState.VP.viewportScissors[i].scissor.width = state.scissors[i].extent.width;
				m_VulkanPipelineState.VP.viewportScissors[i].scissor.height = state.scissors[i].extent.height;
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

			m_VulkanPipelineState.RS.depthBias = state.bias.depth;
			m_VulkanPipelineState.RS.depthBiasClamp = state.bias.biasclamp;
			m_VulkanPipelineState.RS.slopeScaledDepthBias = state.bias.slope;
			m_VulkanPipelineState.RS.lineWidth = state.lineWidth;

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

			memcpy(m_VulkanPipelineState.CB.blendConst, state.blendConst, sizeof(float)*4);

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

			m_VulkanPipelineState.DS.minDepthBounds = state.mindepth;
			m_VulkanPipelineState.DS.maxDepthBounds = state.maxdepth;

			m_VulkanPipelineState.DS.front.ref = state.front.ref;
			m_VulkanPipelineState.DS.front.compareMask = state.front.compare;
			m_VulkanPipelineState.DS.front.writeMask = state.front.write;

			m_VulkanPipelineState.DS.back.ref = state.back.ref;
			m_VulkanPipelineState.DS.back.compareMask = state.back.compare;
			m_VulkanPipelineState.DS.back.writeMask = state.back.write;

			// Renderpass
			m_VulkanPipelineState.Pass.renderpass.obj = state.renderPass;
			m_VulkanPipelineState.Pass.renderpass.inputAttachments = c.m_RenderPass[state.renderPass].inputAttachments;
			m_VulkanPipelineState.Pass.renderpass.colorAttachments = c.m_RenderPass[state.renderPass].colorAttachments;
			m_VulkanPipelineState.Pass.renderpass.depthstencilAttachment = c.m_RenderPass[state.renderPass].depthstencilAttachment;

			m_VulkanPipelineState.Pass.framebuffer.obj = state.framebuffer;

			m_VulkanPipelineState.Pass.framebuffer.width = c.m_Framebuffer[state.framebuffer].width;
			m_VulkanPipelineState.Pass.framebuffer.height = c.m_Framebuffer[state.framebuffer].height;
			m_VulkanPipelineState.Pass.framebuffer.layers = c.m_Framebuffer[state.framebuffer].layers;

			create_array_uninit(m_VulkanPipelineState.Pass.framebuffer.attachments, c.m_Framebuffer[state.framebuffer].attachments.size());
			for(size_t i=0; i < c.m_Framebuffer[state.framebuffer].attachments.size(); i++)
			{
				ResourceId viewid = rm->GetOriginalID(c.m_Framebuffer[state.framebuffer].attachments[i].view);

				m_VulkanPipelineState.Pass.framebuffer.attachments[i].view = viewid;
				m_VulkanPipelineState.Pass.framebuffer.attachments[i].img = rm->GetOriginalID(c.m_ImageView[viewid].image);
			}

			m_VulkanPipelineState.Pass.renderArea.x = state.renderArea.offset.x;
			m_VulkanPipelineState.Pass.renderArea.y = state.renderArea.offset.y;
			m_VulkanPipelineState.Pass.renderArea.width = state.renderArea.extent.width;
			m_VulkanPipelineState.Pass.renderArea.height = state.renderArea.extent.height;
		}
	}
}

void VulkanReplay::FillCBufferVariables(rdctype::array<ShaderConstant> invars, vector<ShaderVariable> &outvars, const vector<byte> &data, size_t &offset)
{
	for(int v=0; v < invars.count; v++)
	{
		string basename = invars[v].name.elems;
		
		uint32_t rows = invars[v].type.descriptor.rows;
		uint32_t cols = invars[v].type.descriptor.cols;
		uint32_t elems = RDCMAX(1U,invars[v].type.descriptor.elements);
		bool rowMajor = invars[v].type.descriptor.rowMajorStorage != 0;
		bool isArray = elems > 1;

		if(invars[v].type.members.count > 0)
		{
			// structs are aligned
			offset = AlignUp16(offset);

			ShaderVariable var;
			var.name = basename;
			var.rows = var.columns = 0;
			var.type = eVar_Float;
			
			vector<ShaderVariable> varmembers;

			if(isArray)
			{
				for(uint32_t i=0; i < elems; i++)
				{
					// each struct in the array is aligned
					offset = AlignUp16(offset);

					ShaderVariable vr;
					vr.name = StringFormat::Fmt("%s[%u]", basename.c_str(), i);
					vr.rows = vr.columns = 0;
					vr.type = eVar_Float;

					vector<ShaderVariable> mems;

					FillCBufferVariables(invars[v].type.members, mems, data, offset);

					vr.isStruct = true;

					vr.members = mems;

					varmembers.push_back(vr);
				}

				var.isStruct = false;
			}
			else
			{
				var.isStruct = true;

				FillCBufferVariables(invars[v].type.members, varmembers, data, offset);
			}

			{
				var.members = varmembers;
				outvars.push_back(var);
			}

			continue;
		}
		
		// NOTE this won't work as-is for doubles, the below logic
		// assumes 32-bit values. This code will all go away anyway
		// once offsets & strides are correctly listed per-element
		size_t elemByteSize = sizeof(uint32_t);
		size_t sz = elemByteSize;

		// vector
		if(cols == 1)
		{
			if(isArray)
			{
				// arrays are aligned to float4 boundary
				offset = AlignUp16(offset);

				// array elements are also aligned - note, last
				// element only takes up however much space it would
				// so e.g. a float3 array leaves one float at the end
				// that could be there
				sz *= 4*elems;
				sz -= (4-rows)*elemByteSize;
			}
			else if(rows > 2)
			{
				// float3s and float4s are aligned
				offset = AlignUp16(offset);
				sz *= rows;
			}
		}
		else
		{
			// matrices are aligned to float4 boundary
			offset = AlignUp16(offset);

			// matrices act like an array of vectors, whether they
			// are an array or not. We just need to determine if
			// those vectors are the matrix's rows, or its columns,
			// and adjust number of elements - even a float2x2 is
			// stored in two float4s.
			if(rowMajor)
			{
				// account for array elems as well as columns.
				// Note the last array elem can have space after
				// it which can be filled with another element, so
				// need to ensure the 'stride' accounts for that.
				sz *= 4*elems*cols;
				sz -= (4-rows)*elemByteSize;
			}
			else
			{
				sz *= 4*elems*rows;
				sz -= (4-cols)*elemByteSize;
			}
		}
		
		// after alignment, this is where we'll read from
		size_t dataOffset = offset;

		offset += sz;

		size_t outIdx = outvars.size();
		outvars.resize(outvars.size()+1);

		{
			outvars[outIdx].name = basename;
			outvars[outIdx].rows = 1;
			outvars[outIdx].type = invars[v].type.descriptor.type;
			outvars[outIdx].isStruct = false;
			outvars[outIdx].columns = cols;

			ShaderVariable &var = outvars[outIdx];

			if(!isArray)
			{
				outvars[outIdx].rows = rows;

				if(dataOffset < data.size())
				{
					const byte *d = &data[dataOffset];

					RDCASSERT(rows <= 4 && rows*cols <= 16);

					if(!rowMajor)
					{
						uint32_t tmp[16] = {0};

						for(uint32_t r=0; r < rows; r++)
						{
							size_t srcoffs = 4*elemByteSize*r;
							size_t dstoffs = cols*elemByteSize*r;
							memcpy((byte *)(tmp) + dstoffs, d + srcoffs,
											RDCMIN(data.size()-dataOffset + srcoffs, elemByteSize*cols));
						}

						// transpose
						for(size_t r=0; r < rows; r++)
							for(size_t c=0; c < cols; c++)
								outvars[outIdx].value.uv[r*cols+c] = tmp[c*rows+r];
					}
					else
					{
						for(uint32_t r=0; r < rows; r++)
						{
							size_t srcoffs = 4*elemByteSize*r;
							size_t dstoffs = cols*elemByteSize*r;
							memcpy((byte *)(&outvars[outIdx].value.uv[0]) + dstoffs, d + srcoffs,
											RDCMIN(data.size()-dataOffset + srcoffs, elemByteSize*cols));
						}
					}
				}
			}
			else
			{
				char buf[64] = {0};

				var.name = outvars[outIdx].name;
				var.rows = 0;
				var.columns = 0;

				bool isMatrix = rows > 1 && cols > 1;

				vector<ShaderVariable> varmembers;
				varmembers.resize(elems);
				
				string base = outvars[outIdx].name.elems;

				uint32_t primaryDim = cols;
				uint32_t secondaryDim = rows;
				if(rowMajor)
				{
					primaryDim = rows;
					secondaryDim = cols;
				}

				for(uint32_t e=0; e < elems; e++)
				{
					varmembers[e].name = StringFormat::Fmt("%s[%u]", base.c_str(), e);
					varmembers[e].rows = rows;
					varmembers[e].type = invars[v].type.descriptor.type;
					varmembers[e].isStruct = false;
					varmembers[e].columns = cols;
					
					size_t rowDataOffset = dataOffset+e*primaryDim*4*elemByteSize;

					if(rowDataOffset < data.size())
					{
						const byte *d = &data[rowDataOffset];

						// each primary element (row or column) is stored in a float4.
						// we copy some padding here, but that will come out in the wash
						// when we transpose
						for(uint32_t p=0; p < primaryDim; p++)
						{
							memcpy(&(varmembers[e].value.uv[secondaryDim*p]), d + 4*elemByteSize*p,
								RDCMIN(data.size()- rowDataOffset, elemByteSize*secondaryDim));
						}

						if(!rowMajor)
						{
							ShaderVariable tmp = varmembers[e];
							// transpose
							for(size_t ri=0; ri < rows; ri++)
								for(size_t ci=0; ci < cols; ci++)
									varmembers[e].value.uv[ri*cols+ci] = tmp.value.uv[ci*rows+ri];
						}
					}
				}

				{
					var.isStruct = false;
					var.members = varmembers;
				}
			}
		}
	}
}

void VulkanReplay::FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	// Correct SPIR-V will ultimately need to set explicit layout information for each type.
	// For now, just assume D3D11 packing (float4 alignment on float4s, float3s, matrices, arrays and structures)

	auto it = m_pDriver->m_ShaderInfo.find(shader);
	
	if(it == m_pDriver->m_ShaderInfo.end())
	{
		RDCERR("Can't get shader details");
		return;
	}

	ShaderReflection &refl = it->second.refl;

	if(cbufSlot >= (uint32_t)refl.ConstantBlocks.count)
	{
		RDCERR("Invalid cbuffer slot");
		return;
	}

	ConstantBlock &c = refl.ConstantBlocks[cbufSlot];

	size_t zero = 0;
	FillCBufferVariables(c.variables, outvars, data, zero);
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

	InitReplayTables();

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
