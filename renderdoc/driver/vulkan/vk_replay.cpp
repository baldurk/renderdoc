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
#include "maths/matrix.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"

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

#define TEXDISPLAY_TYPEMASK    0xF
#define TEXDISPLAY_UINT_TEX    0x10
#define TEXDISPLAY_SINT_TEX    0x20
#define TEXDISPLAY_NANS        0x80
#define TEXDISPLAY_CLIPPING    0x100
#define TEXDISPLAY_GAMMA_CURVE 0x200

struct genericuniforms
{
	Vec4f Offset;
	Vec4f Scale;
	Vec4f Color;
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

#define MESHDISPLAY_SOLID           0x1
#define MESHDISPLAY_FACELIT         0x2
#define MESHDISPLAY_SECONDARY       0x3
#define MESHDISPLAY_SECONDARY_ALPHA 0x4

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
	rp = VK_NULL_HANDLE;
	rpdepth = VK_NULL_HANDLE;

	VkImageMemoryBarrier t = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		VK_NULL_HANDLE,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	for(size_t i=0; i < ARRAY_COUNT(colBarrier); i++)
		colBarrier[i] = t;

	bbBarrier = t;

	t.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthBarrier = t;
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
		vt->DestroyRenderPass(Unwrap(device), Unwrap(rp));
		GetResourceManager()->ReleaseWrappedResource(rp);
		rp = VK_NULL_HANDLE;
		
		vt->DestroyImage(Unwrap(device), Unwrap(bb));
		GetResourceManager()->ReleaseWrappedResource(bb);
		
		vt->DestroyImageView(Unwrap(device), Unwrap(bbview));
		GetResourceManager()->ReleaseWrappedResource(bbview);
		vt->FreeMemory(Unwrap(device), Unwrap(bbmem));
		GetResourceManager()->ReleaseWrappedResource(bbmem);
		vt->DestroyFramebuffer(Unwrap(device), Unwrap(fb));
		GetResourceManager()->ReleaseWrappedResource(fb);
		
		bb = VK_NULL_HANDLE;
		bbview = VK_NULL_HANDLE;
		bbmem = VK_NULL_HANDLE;
		fb = VK_NULL_HANDLE;
	}

	// not owned - freed with the swapchain
	for(size_t i=0; i < ARRAY_COUNT(colimg); i++)
	{
		if(colimg[i] != VK_NULL_HANDLE)
			GetResourceManager()->ReleaseWrappedResource(colimg[i]);
		colimg[i] = VK_NULL_HANDLE;
	}

	if(dsimg != VK_NULL_HANDLE)
	{
		vt->DestroyRenderPass(Unwrap(device), Unwrap(rpdepth));
		GetResourceManager()->ReleaseWrappedResource(rpdepth);
		rpdepth = VK_NULL_HANDLE;
		
		vt->DestroyImage(Unwrap(device), Unwrap(dsimg));
		GetResourceManager()->ReleaseWrappedResource(dsimg);
		
		vt->DestroyImageView(Unwrap(device), Unwrap(dsview));
		GetResourceManager()->ReleaseWrappedResource(dsview);
		vt->FreeMemory(Unwrap(device), Unwrap(dsmem));
		GetResourceManager()->ReleaseWrappedResource(dsmem);
		vt->DestroyFramebuffer(Unwrap(device), Unwrap(fbdepth));
		GetResourceManager()->ReleaseWrappedResource(fbdepth);
		
		dsview = VK_NULL_HANDLE;
		dsimg = VK_NULL_HANDLE;
		dsmem = VK_NULL_HANDLE;
		fbdepth = VK_NULL_HANDLE;
		rpdepth = VK_NULL_HANDLE;
	}

	if(swap != VK_NULL_HANDLE)
	{
		vt->DestroySwapchainKHR(Unwrap(device), Unwrap(swap));
		GetResourceManager()->ReleaseWrappedResource(swap);
	}
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

	GetResourceManager()->WrapResource(Unwrap(device), swap);

	if(old != VK_NULL_HANDLE)
	{
		vt->DestroySwapchainKHR(Unwrap(device), Unwrap(old));
		GetResourceManager()->ReleaseWrappedResource(old);
	}

	vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swap), &numImgs, NULL);
	RDCASSERT(vkr == VK_SUCCESS);

	VkImage* imgs = new VkImage[numImgs];
	vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swap), &numImgs, imgs);
	RDCASSERT(vkr == VK_SUCCESS);

	for(size_t i=0; i < numImgs; i++)
	{
		colimg[i] = imgs[i];
		GetResourceManager()->WrapResource(Unwrap(device), colimg[i]);
		colBarrier[i].image = Unwrap(colimg[i]);
		colBarrier[i].oldLayout = colBarrier[i].newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	curidx = 0;

	if(depth)
	{
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
			{ width, height, 1 },
			1, 1, VULKAN_MESH_VIEW_SAMPLES,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			0, VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		VkResult vkr = vt->CreateImage(Unwrap(device), &imInfo, &dsimg);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), dsimg);

		VkMemoryRequirements mrq = {0};

		vkr = vt->GetImageMemoryRequirements(Unwrap(device), Unwrap(dsimg), &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(Unwrap(device), &allocInfo, &dsmem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), dsmem);

		vkr = vt->BindImageMemory(Unwrap(device), Unwrap(dsimg), Unwrap(dsmem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		depthBarrier.image = Unwrap(dsimg);
		depthBarrier.oldLayout = depthBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		VkImageViewCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(dsimg), VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_D32_SFLOAT,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 },
			0
		};

		vkr = vt->CreateImageView(Unwrap(device), &info, &dsview);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), dsview);
	}

	{
		VkAttachmentDescription attDesc[] = {
			{
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
				imformat, depth ? VULKAN_MESH_VIEW_SAMPLES : 1U,
				VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			},
			{
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
				VK_FORMAT_D32_SFLOAT, depth ? VULKAN_MESH_VIEW_SAMPLES : 1U,
				VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			}
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
				1, attDesc,
				1, &sub,
				0, NULL, // dependencies
		};

		vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, &rp);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), rp);

		if(dsimg != VK_NULL_HANDLE)
		{
			sub.depthStencilAttachment.attachment = 1;
			sub.depthStencilAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			rpinfo.attachmentCount = 2;

			vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, &rpdepth);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), rpdepth);
		}
	}

	{
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, imformat, { width, height, 1 },
			1, 1, depth ? VULKAN_MESH_VIEW_SAMPLES : 1U,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			0, VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		VkResult vkr = vt->CreateImage(Unwrap(device), &imInfo, &bb);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), bb);

		VkMemoryRequirements mrq = {0};

		vkr = vt->GetImageMemoryRequirements(Unwrap(device), Unwrap(bb), &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(Unwrap(device), &allocInfo, &bbmem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), bbmem);

		vkr = vt->BindImageMemory(Unwrap(device), Unwrap(bb), Unwrap(bbmem), 0);
		RDCASSERT(vkr == VK_SUCCESS);

		bbBarrier.image = Unwrap(bb);
		bbBarrier.oldLayout = bbBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

		GetResourceManager()->WrapResource(Unwrap(device), bbview);

		VkFramebufferCreateInfo fbinfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
			Unwrap(rp),
			1, UnwrapPtr(bbview),
			(uint32_t)width, (uint32_t)height, 1,
		};

		vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, &fb);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), fb);

		if(dsimg != VK_NULL_HANDLE)
		{
			VkImageView views[] = { Unwrap(bbview), Unwrap(dsview) };
			VkFramebufferCreateInfo fbinfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
				Unwrap(rpdepth),
				2, views,
				(uint32_t)width, (uint32_t)height, 1,
			};

			vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, &fbdepth);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), fbdepth);
		}
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

VulkanResourceManager *VulkanReplay::GetResourceManager()
{
	return m_pDriver->GetResourceManager();
}

void VulkanReplay::Shutdown()
{
	m_pDriver->Shutdown();
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
	vector<ResourceId> texs;

	for(auto it = m_pDriver->m_ImageLayouts.begin(); it != m_pDriver->m_ImageLayouts.end(); ++it)
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
	vector<ResourceId> bufs;

	for(auto it = m_pDriver->m_CreationInfo.m_Buffer.begin(); it != m_pDriver->m_CreationInfo.m_Buffer.end(); ++it)
	{
		// skip textures that aren't from the capture
		if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
			 continue;

		bufs.push_back(it->first);
	}

	return bufs;
}

FetchTexture VulkanReplay::GetTexture(ResourceId id)
{
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[id];

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
	ret.name = m_pDriver->m_CreationInfo.m_Names[id];
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
	VulkanCreationInfo::Buffer &bufinfo = m_pDriver->m_CreationInfo.m_Buffer[id];

	FetchBuffer ret;
	ret.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);
	ret.byteSize = bufinfo.size;
	ret.structureSize = 0;
	ret.length = (uint32_t)ret.byteSize;

	ret.creationFlags = 0;

	if(bufinfo.usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
		ret.creationFlags |= eBufferCreate_UAV;
	if(bufinfo.usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT))
		ret.creationFlags |= eBufferCreate_CB;
	if(bufinfo.usage & (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT))
		ret.creationFlags |= eBufferCreate_Indirect;
	if(bufinfo.usage & (VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
		ret.creationFlags |= eBufferCreate_IB;
	if(bufinfo.usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		ret.creationFlags |= eBufferCreate_IB;

	ret.customName = true;
	ret.name = m_pDriver->m_CreationInfo.m_Names[id];
	if(ret.name.count == 0)
	{
		ret.customName = false;
		ret.name = StringFormat::Fmt("Buffer %llu", ret.ID);
	}

	return ret;
}

ShaderReflection *VulkanReplay::GetShader(ResourceId id)
{
	auto shad = m_pDriver->m_CreationInfo.m_Shader.find(id);
	
	if(shad == m_pDriver->m_CreationInfo.m_Shader.end())
	{
		RDCERR("Can't get shader details");
		return NULL;
	}

	// disassemble lazily on demand
	if(shad->second.refl.Disassembly.count == 0)
	{
		auto &shadmod = m_pDriver->m_CreationInfo.m_ShaderModule[shad->second.module];

		if(shadmod.spirv.m_Disassembly.empty())
			shadmod.spirv.Disassemble();

		shad->second.refl.Disassembly = shadmod.spirv.m_Disassembly;
	}

	return &shad->second.refl;
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
		texDisplay.overlay = eTexOverlay_None;
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

		RenderTextureInternal(texDisplay, rpbegin, true);
	}

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkResult vkr = VK_SUCCESS;

	{
		VkImageMemoryBarrier pickimBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			Unwrap(GetDebugManager()->m_PickPixelImage),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		// update image layout from color attachment to transfer source, with proper memory barriers
		pickimBarrier.outputMask = VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT;
		pickimBarrier.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		void *barrier = (void *)&pickimBarrier;
		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
		pickimBarrier.oldLayout = pickimBarrier.newLayout;

		pickimBarrier.outputMask = 0;
		pickimBarrier.inputMask = 0;

		// do copy
		VkBufferImageCopy region = {
			0, 128, 1,
			{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
			{ 0, 0, 0 },
			{ 1, 1, 1 },
		};
		vt->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_PickPixelImage), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.buf), 1, &region);

		// update image layout back to color attachment
		pickimBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

		vt->EndCommandBuffer(Unwrap(cmd));
	}

	// submit cmds and wait for idle so we can readback
	m_pDriver->SubmitCmds();
	m_pDriver->FlushQ();

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
	VULKANNOTIMP("PickVertex");
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
		Unwrap(outw.rp), Unwrap(outw.fb),
		{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
		1, &clearval,
	};

	return RenderTextureInternal(cfg, rpbegin, false);
}

bool VulkanReplay::RenderTextureInternal(TextureDisplay cfg, VkRenderPassBeginInfo rpbegin, bool f32render)
{
	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[cfg.texid];
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[cfg.texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(cfg.texid);

	// VKTODOMED handle multiple subresources with different layouts etc
	VkImageLayout origLayout = layouts.subresourceStates[0].newLayout;
	VkImageView liveImView = iminfo.view;

	if(liveImView == VK_NULL_HANDLE)
	{
		VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(liveIm), VK_IMAGE_VIEW_TYPE_2D,
			iminfo.format,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ layouts.subresourceStates[0].subresourceRange.aspectMask, 0, RDCMAX(1U, (uint32_t)iminfo.mipLevels), 0, 1, },
			0
		};

		if(layouts.subresourceStates[0].subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
		{
			viewInfo.channels.g = VK_CHANNEL_SWIZZLE_ZERO;
			viewInfo.channels.b = VK_CHANNEL_SWIZZLE_ZERO;
			viewInfo.channels.a = VK_CHANNEL_SWIZZLE_ZERO;
		}

		// Only needed on AMD - does the wrong thing on nvidia - so commented for now while AMD
		// drivers aren't on 0.9.2
		//if(iminfo.format == VK_FORMAT_B8G8R8A8_UNORM || iminfo.format == VK_FORMAT_B8G8R8A8_SRGB)
			//std::swap(viewInfo.channels.r, viewInfo.channels.b);

		VkResult vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, &iminfo.view);
		RDCASSERT(vkr == VK_SUCCESS);

		ResourceId viewid = m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), iminfo.view);
		// register as a live-only resource, so it is cleaned up properly
		m_pDriver->GetResourceManager()->AddLiveResource(viewid, iminfo.view);

		liveImView = iminfo.view;
	}

	uint32_t uboOffs = 0;
	
	displayuniforms *data = (displayuniforms *)GetDebugManager()->m_TexDisplayUBO.Map(vt, dev, &uboOffs);

	data->Padding = 0;
	
	float x = cfg.offx;
	float y = cfg.offy;
	
	data->Position.x = x;
	data->Position.y = y;
	data->HDRMul = -1.0f;

	int32_t tex_x = iminfo.extent.width;
	int32_t tex_y = iminfo.extent.height;
	int32_t tex_z = iminfo.extent.depth;

	if(cfg.scale <= 0.0f)
	{
		float xscale = float(m_DebugWidth)/float(tex_x);
		float yscale = float(m_DebugHeight)/float(tex_y);

		// update cfg.scale for use below
		float scale = cfg.scale = RDCMIN(xscale, yscale);

		if(yscale > xscale)
		{
			data->Position.x = 0;
			data->Position.y = (float(m_DebugHeight)-(tex_y*scale) )*0.5f;
		}
		else
		{
			data->Position.y = 0;
			data->Position.x = (float(m_DebugWidth)-(tex_x*scale) )*0.5f;
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
	if(iminfo.type != VK_IMAGE_TYPE_3D)
		data->Slice = (float)cfg.sliceFace;
	else
		data->Slice = (float)(cfg.sliceFace>>cfg.mip);

	float mipScale = float(1<<cfg.mip);
	
	data->TextureResolutionPS.x = float(tex_x)/mipScale;
	data->TextureResolutionPS.y = float(tex_y)/mipScale;
	data->TextureResolutionPS.z = float(tex_z)/mipScale;
	
	data->Scale = cfg.scale*mipScale;
	
	data->NumSamples = iminfo.samples;
	data->SampleIdx = cfg.sampleIdx;

	data->OutputRes.x = (float)m_DebugWidth;
	data->OutputRes.y = (float)m_DebugHeight;

	int displayformat = 0;
	
	if(!IsSRGBFormat(iminfo.format) && cfg.linearDisplayAsGamma)
		displayformat |= TEXDISPLAY_GAMMA_CURVE;

	if(cfg.overlay == eTexOverlay_NaN)
		displayformat |= TEXDISPLAY_NANS;

	if(cfg.overlay == eTexOverlay_Clipping)
		displayformat |= TEXDISPLAY_CLIPPING;

	// VKTODOMED handle different texture types/displays
	data->OutputDisplayFormat = displayformat;
	
	data->RawOutput = cfg.rawoutput ? 1 : 0;

	GetDebugManager()->m_TexDisplayUBO.Unmap(vt, dev);
	
	VkDescriptorInfo desc = {0};
	desc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc.imageView = Unwrap(liveImView);
	desc.sampler = Unwrap(GetDebugManager()->m_PointSampler);
	if(cfg.mip == 0 && cfg.scale < 1.0f)
		desc.sampler = Unwrap(GetDebugManager()->m_LinearSampler);

	VkDescriptorSet descset = GetDebugManager()->GetTexDisplayDescSet();

	VkDescriptorInfo ubodesc = {0};
	GetDebugManager()->m_TexDisplayUBO.FillDescriptor(ubodesc);

	VkWriteDescriptorSet writeSet[] = {
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(descset), 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(descset), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &ubodesc
		},
	};

	vt->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);

	VkImageMemoryBarrier srcimBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, origLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	// ensure all previous writes have completed
	srcimBarrier.outputMask =
		VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT|
		VK_MEMORY_OUTPUT_SHADER_WRITE_BIT|
		VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
		VK_MEMORY_OUTPUT_TRANSFER_BIT;
	// before we go reading
	srcimBarrier.inputMask = VK_MEMORY_INPUT_SHADER_READ_BIT;

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	void *barrier = (void *)&srcimBarrier;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
	srcimBarrier.oldLayout = srcimBarrier.newLayout;

	srcimBarrier.outputMask = 0;
	srcimBarrier.inputMask = 0;

	{
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);
		
		VkPipeline pipe = GetDebugManager()->m_TexDisplayPipeline;
		if(f32render)
			pipe = GetDebugManager()->m_TexDisplayF32Pipeline;
		else if(!cfg.rawoutput && cfg.CustomShader == ResourceId())
			pipe = GetDebugManager()->m_TexDisplayBlendPipeline;

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_TexDisplayPipeLayout), 0, 1, UnwrapPtr(descset), 1, &uboOffs);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);

		vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	srcimBarrier.newLayout = origLayout;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vt->EndCommandBuffer(Unwrap(cmd));

	return true;
}
	
void VulkanReplay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	uint32_t uboOffs = 0;

	Vec4f *data = (Vec4f *)GetDebugManager()->m_CheckerboardUBO.Map(vt, dev, &uboOffs);
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
			Unwrap(outw.rp), Unwrap(outw.fb),
			{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
			outw.dsimg == VK_NULL_HANDLE ? Unwrap(GetDebugManager()->m_CheckerboardPipeline) : Unwrap(GetDebugManager()->m_CheckerboardMSAAPipeline));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_CheckerboardPipeLayout), 0, 1, UnwrapPtr(GetDebugManager()->m_CheckerboardDescSet), 1, &uboOffs);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);
		
		vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);
}
	
void VulkanReplay::RenderHighlightBox(float w, float h, float scale)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);
	
	const float xpixdim = 2.0f/w;
	const float ypixdim = 2.0f/h;
	
	const float xdim = scale*xpixdim;
	const float ydim = scale*ypixdim;

	uint32_t uboOffs = 0;

	genericuniforms *data = (genericuniforms *)GetDebugManager()->m_GenericUBO.Map(vt, dev, &uboOffs);
	data->Offset = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);
	data->Scale = Vec4f(xdim, ydim, 1.0f, 1.0f);
	data->Color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
	GetDebugManager()->m_GenericUBO.Unmap(vt, dev);

	{
		VkClearValue clearval = {0};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			Unwrap(outw.rp), Unwrap(outw.fb),
			{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_HighlightBoxPipeline));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_GenericPipeLayout), 0, 1, UnwrapPtr(GetDebugManager()->m_GenericDescSet), 1, &uboOffs);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);

		VkDeviceSize zero = 0;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_OutlineStripVBO.buf), &zero);
		
		vt->CmdDraw(Unwrap(cmd), 8, 1, 0, 0);

		genericuniforms secondOutline;
		secondOutline.Offset = Vec4f(-xpixdim, -ypixdim, 0.0f, 0.0f);
		secondOutline.Scale = Vec4f(xdim+xpixdim*2, ydim+ypixdim*2, 1.0f, 1.0f);
		secondOutline.Color = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

		vt->CmdUpdateBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_GenericUBO.buf), uboOffs, sizeof(genericuniforms), (uint32_t *)&secondOutline);
		
		vt->CmdDraw(Unwrap(cmd), 8, 1, 0, 0);

		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);
}

ResourceId VulkanReplay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	return GetDebugManager()->RenderOverlay(texid, overlay, frameID, eventID, passEvents);
}

FloatVector VulkanReplay::InterpretVertex(byte *data, uint32_t vert, MeshDisplay cfg, byte *end, bool useidx, bool &valid)
{
	FloatVector ret(0.0f, 0.0f, 0.0f, 1.0f);

	if(useidx && m_HighlightCache.useidx)
	{
		if(vert >= (uint32_t)m_HighlightCache.indices.size())
		{
			valid = false;
			return ret;
		}

		vert = m_HighlightCache.indices[vert];
	}

	data += vert*cfg.position.stride;

	float *out = &ret.x;

	ResourceFormat fmt;
	fmt.compByteWidth = cfg.position.compByteWidth;
	fmt.compCount = cfg.position.compCount;
	fmt.compType = cfg.position.compType;

	if(cfg.position.specialFormat == eSpecial_R10G10B10A2)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		Vec4f v = ConvertFromR10G10B10A2(*(uint32_t *)data);
		ret.x = v.x;
		ret.y = v.y;
		ret.z = v.z;
		ret.w = v.w;
		return ret;
	}
	else if(cfg.position.specialFormat == eSpecial_R11G11B10)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		Vec3f v = ConvertFromR11G11B10(*(uint32_t *)data);
		ret.x = v.x;
		ret.y = v.y;
		ret.z = v.z;
		return ret;
	}
	else if(cfg.position.specialFormat == eSpecial_B8G8R8A8)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		fmt.compByteWidth = 1;
		fmt.compCount = 4;
		fmt.compType = eCompType_UNorm;
	}
	
	if(data + cfg.position.compCount*cfg.position.compByteWidth > end)
	{
		valid = false;
		return ret;
	}

	for(uint32_t i=0; i < cfg.position.compCount; i++)
	{
		*out = ConvertComponent(fmt, data);

		data += cfg.position.compByteWidth;
		out++;
	}

	if(cfg.position.specialFormat == eSpecial_B8G8R8A8)
	{
		FloatVector reversed;
		reversed.x = ret.x;
		reversed.y = ret.y;
		reversed.z = ret.z;
		reversed.w = ret.w;
		return reversed;
	}

	return ret;
}

void VulkanReplay::RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg)
{
	if(cfg.position.buf == ResourceId())
		return;
	
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkResult vkr = VK_SUCCESS;
	
	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	VkClearValue clearval = {0};
	VkRenderPassBeginInfo rpbegin = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		Unwrap(outw.rpdepth), Unwrap(outw.fbdepth),
		{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
		1, &clearval,
	};
	vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);
	
	VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
	vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);
	
	Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(m_DebugWidth)/float(m_DebugHeight));
	Matrix4f InvProj = projMat.Inverse();

	Matrix4f camMat = cfg.cam ? cfg.cam->GetMatrix() : Matrix4f::Identity();

	Matrix4f ModelViewProj = projMat.Mul(camMat);
	Matrix4f guessProjInv;

	if(cfg.position.unproject)
	{
		// the derivation of the projection matrix might not be right (hell, it could be an
		// orthographic projection). But it'll be close enough likely.
		Matrix4f guessProj = Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect);

		if(cfg.ortho)
		{
			guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
		}
		
		guessProjInv = guessProj.Inverse();

		ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
	}

	RDCASSERT(secondaryDraws.empty());
	
	MeshDisplayPipelines cache = GetDebugManager()->CacheMeshDisplayPipelines(cfg.position, cfg.second);
	
	if(cfg.position.buf != ResourceId())
	{
		VkBuffer vb = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.buf);

		VkDeviceSize offs = cfg.position.offset;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);
	}
	
	if(cfg.second.buf != ResourceId())
	{
		VkBuffer vb = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.second.buf);

		VkDeviceSize offs = cfg.second.offset;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 1, 1, UnwrapPtr(vb), &offs);
	}

	// can't support secondary shading without a buffer - no pipeline will have been created
	if(cfg.solidShadeMode == eShade_Secondary && cfg.second.buf == ResourceId())
		cfg.solidShadeMode = eShade_None;
	
	// solid render
	if(cfg.solidShadeMode != eShade_None && cfg.position.topo < eTopology_PatchList)
	{
		VkPipeline pipe = NULL;
		switch(cfg.solidShadeMode)
		{
			case eShade_Solid:
				pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth];
				break;
			case eShade_Lit:
				pipe = cache.pipes[MeshDisplayPipelines::ePipe_Lit];
				break;
			case eShade_Secondary:
				pipe = cache.pipes[MeshDisplayPipelines::ePipe_Secondary];
				break;
		}
		
		uint32_t uboOffs = 0;
		meshuniforms *data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);

		if(cfg.solidShadeMode == eShade_Lit)
			data->invProj = projMat.Inverse();
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(0.8f, 0.8f, 0.0f, 1.0f);
		data->homogenousInput = cfg.position.unproject;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);
		data->displayFormat = (uint32_t)cfg.solidShadeMode;

		if(cfg.solidShadeMode == eShade_Secondary && cfg.second.showAlpha)
			data->displayFormat = MESHDISPLAY_SECONDARY_ALPHA;

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
			0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

		if(cfg.position.idxByteWidth)
		{
			VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
			if(cfg.position.idxByteWidth == 4)
				idxtype = VK_INDEX_TYPE_UINT32;

			if(cfg.position.idxbuf != ResourceId())
			{
				VkBuffer ib = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.idxbuf);

				vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), cfg.position.idxoffs, idxtype);
			}
			vt->CmdDrawIndexed(Unwrap(cmd), cfg.position.numVerts, 1, 0, 0, 0);
		}
		else
		{
			vt->CmdDraw(Unwrap(cmd), cfg.position.numVerts, 1, 0, 0);
		}
	}

	// wireframe render
	if(cfg.solidShadeMode == eShade_None || cfg.wireframeDraw || cfg.position.topo >= eTopology_PatchList)
	{
		Vec4f wireCol = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
		if(!secondaryDraws.empty())
		{
			wireCol.x = cfg.currentMeshColour.x;
			wireCol.y = cfg.currentMeshColour.y;
			wireCol.z = cfg.currentMeshColour.z;
		}
		
		uint32_t uboOffs = 0;
		meshuniforms *data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = wireCol;
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = cfg.position.unproject;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
			0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		
		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]));

		if(cfg.position.idxByteWidth)
		{
			VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
			if(cfg.position.idxByteWidth == 4)
				idxtype = VK_INDEX_TYPE_UINT32;

			if(cfg.position.idxbuf != ResourceId())
			{
				VkBuffer ib = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.idxbuf);

				vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), cfg.position.idxoffs, idxtype);
			}
			vt->CmdDrawIndexed(Unwrap(cmd), cfg.position.numVerts, 1, 0, 0, 0);
		}
		else
		{
			vt->CmdDraw(Unwrap(cmd), cfg.position.numVerts, 1, 0, 0);
		}
	}

	MeshFormat helper;
	helper.idxByteWidth = 2;
	helper.topo = eTopology_LineList;
	
	helper.specialFormat = eSpecial_Unknown;
	helper.compByteWidth = 4;
	helper.compCount = 4;
	helper.compType = eCompType_Float;

	helper.stride = sizeof(Vec4f);

	// cache pipelines for use in drawing wireframe helpers
	cache = GetDebugManager()->CacheMeshDisplayPipelines(helper, helper);
	
	if(cfg.showBBox)
	{
		Vec4f a = Vec4f(cfg.minBounds.x, cfg.minBounds.y, cfg.minBounds.z, cfg.minBounds.w);
		Vec4f b = Vec4f(cfg.maxBounds.x, cfg.maxBounds.y, cfg.maxBounds.z, cfg.maxBounds.w);

		Vec4f TLN = Vec4f(a.x, b.y, a.z, 1.0f); // TopLeftNear, etc...
		Vec4f TRN = Vec4f(b.x, b.y, a.z, 1.0f);
		Vec4f BLN = Vec4f(a.x, a.y, a.z, 1.0f);
		Vec4f BRN = Vec4f(b.x, a.y, a.z, 1.0f);

		Vec4f TLF = Vec4f(a.x, b.y, b.z, 1.0f);
		Vec4f TRF = Vec4f(b.x, b.y, b.z, 1.0f);
		Vec4f BLF = Vec4f(a.x, a.y, b.z, 1.0f);
		Vec4f BRF = Vec4f(b.x, a.y, b.z, 1.0f);

		// 12 frustum lines => 24 verts
		Vec4f bbox[24] =
		{
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

		VkDeviceSize vboffs = 0;
		Vec4f *ptr = (Vec4f *)GetDebugManager()->m_MeshBBoxVB.Map(vt, dev, vboffs);

		memcpy(ptr, bbox, sizeof(bbox));

		GetDebugManager()->m_MeshBBoxVB.Unmap(vt, dev);
		
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshBBoxVB.buf), &vboffs);

		uint32_t uboOffs = 0;
		meshuniforms *data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(0.2f, 0.2f, 1.0f, 1.0f);
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = 0;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
		                          0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		
		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]));

		vt->CmdDraw(Unwrap(cmd), 24, 1, 0, 0);
	}

	// draw axis helpers
	if(!cfg.position.unproject)
	{
		VkDeviceSize vboffs = 0;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshAxisFrustumVB.buf), &vboffs);
		
		uint32_t uboOffs = 0;
		meshuniforms *data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = 0;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
		                          0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		
		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Wire]));
		
		vt->CmdDraw(Unwrap(cmd), 2, 1, 0, 0);
		
		// poke the color (this would be a good candidate for a push constant)
		data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = 0;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
		                          0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		vt->CmdDraw(Unwrap(cmd), 2, 1, 2, 0);
		
		data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = 0;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
		                          0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		vt->CmdDraw(Unwrap(cmd), 2, 1, 4, 0);
	}
	
	// 'fake' helper frustum
	if(cfg.position.unproject)
	{
		VkDeviceSize vboffs = sizeof(Vec4f)*6; // skim the axis helpers
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshAxisFrustumVB.buf), &vboffs);
		
		uint32_t uboOffs = 0;
		meshuniforms *data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = 0;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
		                          0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		
		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Wire]));
		
		vt->CmdDraw(Unwrap(cmd), 24, 1, 0, 0);
	}
	
	// show highlighted vertex
	if(cfg.highlightVert != ~0U)
	{
		MeshDataStage stage = cfg.type;
		
		if(m_HighlightCache.EID != eventID || stage != m_HighlightCache.stage ||
		   cfg.position.buf != m_HighlightCache.buf || cfg.position.offset != m_HighlightCache.offs)
		{
			m_HighlightCache.EID = eventID;
			m_HighlightCache.buf = cfg.position.buf;
			m_HighlightCache.offs = cfg.position.offset;
			m_HighlightCache.stage = stage;
			
			uint32_t bytesize = cfg.position.idxByteWidth; 

			// need to end our cmd buffer, it will be submitted in GetBufferData
			vt->CmdEndRenderPass(Unwrap(cmd));

			vkr = vt->EndCommandBuffer(Unwrap(cmd));
			RDCASSERT(vkr == VK_SUCCESS);

			m_HighlightCache.data = GetBufferData(cfg.position.buf, 0, 0);

			if(cfg.position.idxByteWidth == 0 || stage == eMeshDataStage_GSOut)
			{
				m_HighlightCache.indices.clear();
				m_HighlightCache.useidx = false;
			}
			else
			{
				m_HighlightCache.useidx = true;

				vector<byte> idxdata;
				if(cfg.position.idxbuf != ResourceId())
					idxdata = GetBufferData(cfg.position.idxbuf, cfg.position.idxoffs, cfg.position.numVerts*bytesize);

				uint8_t *idx8 = (uint8_t *)&idxdata[0];
				uint16_t *idx16 = (uint16_t *)&idxdata[0];
				uint32_t *idx32 = (uint32_t *)&idxdata[0];

				uint32_t numIndices = RDCMIN(cfg.position.numVerts, uint32_t(idxdata.size()/bytesize));

				m_HighlightCache.indices.resize(numIndices);

				if(bytesize == 1)
				{
					for(uint32_t i=0; i < numIndices; i++)
						m_HighlightCache.indices[i] = uint32_t(idx8[i]);
				}
				else if(bytesize == 2)
				{
					for(uint32_t i=0; i < numIndices; i++)
						m_HighlightCache.indices[i] = uint32_t(idx16[i]);
				}
				else if(bytesize == 4)
				{
					for(uint32_t i=0; i < numIndices; i++)
						m_HighlightCache.indices[i] = idx32[i];
				}
			}

			// get a new cmdbuffer and begin it
			cmd = m_pDriver->GetNextCmd();
	
			vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
			RDCASSERT(vkr == VK_SUCCESS);
			vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);
			
			vt->CmdSetViewport(Unwrap(cmd), 1, &viewport);
		}

		PrimitiveTopology meshtopo = cfg.position.topo;

		uint32_t idx = cfg.highlightVert;

		byte *data = &m_HighlightCache.data[0]; // buffer start
		byte *dataEnd = data + m_HighlightCache.data.size();

		data += cfg.position.offset; // to start of position data
		
		///////////////////////////////////////////////////////////////
		// vectors to be set from buffers, depending on topology

		bool valid = true;

		// this vert (blue dot, required)
		FloatVector activeVertex;
		 
		// primitive this vert is a part of (red prim, optional)
		vector<FloatVector> activePrim;

		// for patch lists, to show other verts in patch (green dots, optional)
		// for non-patch lists, we use the activePrim and adjacentPrimVertices
		// to show what other verts are related
		vector<FloatVector> inactiveVertices;

		// adjacency (line or tri, strips or lists) (green prims, optional)
		// will be N*M long, N adjacent prims of M verts each. M = primSize below
		vector<FloatVector> adjacentPrimVertices; 

		helper.topo = eTopology_TriangleList;
		uint32_t primSize = 3; // number of verts per primitive
		
		if(meshtopo == eTopology_LineList ||
		   meshtopo == eTopology_LineStrip ||
		   meshtopo == eTopology_LineList_Adj ||
		   meshtopo == eTopology_LineStrip_Adj)
		{
			primSize = 2;
			helper.topo = eTopology_LineList;
		}
		else
		{
			// update the cache, as it's currently linelist
			helper.topo = eTopology_TriangleList;
			cache = GetDebugManager()->CacheMeshDisplayPipelines(helper, helper);
		}
		
		activeVertex = InterpretVertex(data, idx, cfg, dataEnd, true, valid);

		// see Section 15.1.1 of the Vulkan 1.0 spec for
		// how primitive topologies are laid out
		if(meshtopo == eTopology_LineList)
		{
			uint32_t v = uint32_t(idx/2) * 2; // find first vert in primitive

			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, true, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, true, valid));
		}
		else if(meshtopo == eTopology_TriangleList)
		{
			uint32_t v = uint32_t(idx/3) * 3; // find first vert in primitive

			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, true, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, true, valid));
			activePrim.push_back(InterpretVertex(data, v+2, cfg, dataEnd, true, valid));
		}
		else if(meshtopo == eTopology_LineList_Adj)
		{
			uint32_t v = uint32_t(idx/4) * 4; // find first vert in primitive
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, true, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);

			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);

			activePrim.push_back(vs[1]);
			activePrim.push_back(vs[2]);
		}
		else if(meshtopo == eTopology_TriangleList_Adj)
		{
			uint32_t v = uint32_t(idx/6) * 6; // find first vert in primitive
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+4, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+5, cfg, dataEnd, true, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);
			adjacentPrimVertices.push_back(vs[2]);
			
			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);
			adjacentPrimVertices.push_back(vs[4]);
			
			adjacentPrimVertices.push_back(vs[4]);
			adjacentPrimVertices.push_back(vs[5]);
			adjacentPrimVertices.push_back(vs[0]);

			activePrim.push_back(vs[0]);
			activePrim.push_back(vs[2]);
			activePrim.push_back(vs[4]);
		}
		else if(meshtopo == eTopology_LineStrip)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 1U) - 1;
			
			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, true, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, true, valid));
		}
		else if(meshtopo == eTopology_TriangleStrip)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 2U) - 2;
			
			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, true, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, true, valid));
			activePrim.push_back(InterpretVertex(data, v+2, cfg, dataEnd, true, valid));
		}
		else if(meshtopo == eTopology_LineStrip_Adj)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 3U) - 3;
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, true, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, true, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);

			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);

			activePrim.push_back(vs[1]);
			activePrim.push_back(vs[2]);
		}
		else if(meshtopo == eTopology_TriangleStrip_Adj)
		{
			// Triangle strip with adjacency is the most complex topology, as
			// we need to handle the ends separately where the pattern breaks.

			uint32_t numidx = cfg.position.numVerts;

			if(numidx < 6)
			{
				// not enough indices provided, bail to make sure logic below doesn't
				// need to have tons of edge case detection
				valid = false;
			}
			else if(idx <= 4 || numidx <= 7)
			{
				FloatVector vs[] = {
					InterpretVertex(data, 0, cfg, dataEnd, true, valid),
					InterpretVertex(data, 1, cfg, dataEnd, true, valid),
					InterpretVertex(data, 2, cfg, dataEnd, true, valid),
					InterpretVertex(data, 3, cfg, dataEnd, true, valid),
					InterpretVertex(data, 4, cfg, dataEnd, true, valid),

					// note this one isn't used as it's adjacency for the next triangle
					InterpretVertex(data, 5, cfg, dataEnd, true, valid),

					// min() with number of indices in case this is a tiny strip
					// that is basically just a list
					InterpretVertex(data, RDCMIN(6U, numidx-1), cfg, dataEnd, true, valid),
				};

				// these are the triangles on the far left of the MSDN diagram above
				adjacentPrimVertices.push_back(vs[0]);
				adjacentPrimVertices.push_back(vs[1]);
				adjacentPrimVertices.push_back(vs[2]);

				adjacentPrimVertices.push_back(vs[4]);
				adjacentPrimVertices.push_back(vs[3]);
				adjacentPrimVertices.push_back(vs[0]);

				adjacentPrimVertices.push_back(vs[4]);
				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[6]);

				activePrim.push_back(vs[0]);
				activePrim.push_back(vs[2]);
				activePrim.push_back(vs[4]);
			}
			else if(idx > numidx-4)
			{
				// in diagram, numidx == 14

				FloatVector vs[] = {
					/*[0]=*/ InterpretVertex(data, numidx-8, cfg, dataEnd, true, valid), // 6 in diagram

					// as above, unused since this is adjacency for 2-previous triangle
					/*[1]=*/ InterpretVertex(data, numidx-7, cfg, dataEnd, true, valid), // 7 in diagram
					/*[2]=*/ InterpretVertex(data, numidx-6, cfg, dataEnd, true, valid), // 8 in diagram
					
					// as above, unused since this is adjacency for previous triangle
					/*[3]=*/ InterpretVertex(data, numidx-5, cfg, dataEnd, true, valid), // 9 in diagram
					/*[4]=*/ InterpretVertex(data, numidx-4, cfg, dataEnd, true, valid), // 10 in diagram
					/*[5]=*/ InterpretVertex(data, numidx-3, cfg, dataEnd, true, valid), // 11 in diagram
					/*[6]=*/ InterpretVertex(data, numidx-2, cfg, dataEnd, true, valid), // 12 in diagram
					/*[7]=*/ InterpretVertex(data, numidx-1, cfg, dataEnd, true, valid), // 13 in diagram
				};

				// these are the triangles on the far right of the MSDN diagram above
				adjacentPrimVertices.push_back(vs[2]); // 8 in diagram
				adjacentPrimVertices.push_back(vs[0]); // 6 in diagram
				adjacentPrimVertices.push_back(vs[4]); // 10 in diagram

				adjacentPrimVertices.push_back(vs[4]); // 10 in diagram
				adjacentPrimVertices.push_back(vs[7]); // 13 in diagram
				adjacentPrimVertices.push_back(vs[6]); // 12 in diagram

				adjacentPrimVertices.push_back(vs[6]); // 12 in diagram
				adjacentPrimVertices.push_back(vs[5]); // 11 in diagram
				adjacentPrimVertices.push_back(vs[2]); // 8 in diagram

				activePrim.push_back(vs[2]); // 8 in diagram
				activePrim.push_back(vs[4]); // 10 in diagram
				activePrim.push_back(vs[6]); // 12 in diagram
			}
			else
			{
				// we're in the middle somewhere. Each primitive has two vertices for it
				// so our step rate is 2. The first 'middle' primitive starts at indices 5&6
				// and uses indices all the way back to 0
				uint32_t v = RDCMAX( ( (idx+1) / 2) * 2, 6U) - 6;

				// these correspond to the indices in the MSDN diagram, with {2,4,6} as the
				// main triangle
				FloatVector vs[] = {
					InterpretVertex(data, v+0, cfg, dataEnd, true, valid),

					// this one is adjacency for 2-previous triangle
					InterpretVertex(data, v+1, cfg, dataEnd, true, valid),
					InterpretVertex(data, v+2, cfg, dataEnd, true, valid),

					// this one is adjacency for previous triangle
					InterpretVertex(data, v+3, cfg, dataEnd, true, valid),
					InterpretVertex(data, v+4, cfg, dataEnd, true, valid),
					InterpretVertex(data, v+5, cfg, dataEnd, true, valid),
					InterpretVertex(data, v+6, cfg, dataEnd, true, valid),
					InterpretVertex(data, v+7, cfg, dataEnd, true, valid),
					InterpretVertex(data, v+8, cfg, dataEnd, true, valid),
				};

				// these are the triangles around {2,4,6} in the MSDN diagram above
				adjacentPrimVertices.push_back(vs[0]);
				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[4]);

				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[5]);
				adjacentPrimVertices.push_back(vs[6]);

				adjacentPrimVertices.push_back(vs[6]);
				adjacentPrimVertices.push_back(vs[8]);
				adjacentPrimVertices.push_back(vs[4]);

				activePrim.push_back(vs[2]);
				activePrim.push_back(vs[4]);
				activePrim.push_back(vs[6]);
			}
		}
		else if(meshtopo >= eTopology_PatchList)
		{
			uint32_t dim = (cfg.position.topo - eTopology_PatchList_1CPs + 1);

			uint32_t v0 = uint32_t(idx/dim) * dim;

			for(uint32_t v = v0; v < v0+dim; v++)
			{
				if(v != idx && valid)
					inactiveVertices.push_back(InterpretVertex(data, v, cfg, dataEnd, true, valid));
			}
		}
		else // if(meshtopo == eTopology_PointList) point list, or unknown/unhandled type
		{
			// no adjacency, inactive verts or active primitive
		}

		if(valid)
		{
			////////////////////////////////////////////////////////////////
			// prepare rendering (for both vertices & primitives)
			
			// if data is from post transform, it will be in clipspace
			if(cfg.position.unproject)
				ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
			else
				ModelViewProj = projMat.Mul(camMat);

			meshuniforms uniforms;
			uniforms.mvp = ModelViewProj;
			uniforms.color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
			uniforms.displayFormat = (uint32_t)eShade_Solid;
			uniforms.homogenousInput = cfg.position.unproject;
			uniforms.pointSpriteSize = Vec2f(0.0f, 0.0f);
			
			uint32_t uboOffs = 0;
			meshuniforms *data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*data = uniforms;
			GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
			
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
			
			vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Solid]));
			
			////////////////////////////////////////////////////////////////
			// render primitives
			
			// Draw active primitive (red)
			uniforms.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
			// poke the color (this would be a good candidate for a push constant)
			data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*data = uniforms;
			GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);

			if(activePrim.size() >= primSize)
			{
				VkDeviceSize vboffs = 0;
				Vec4f *ptr = (Vec4f *)GetDebugManager()->m_MeshBBoxVB.Map(vt, dev, vboffs, sizeof(Vec4f)*primSize);

				memcpy(ptr, &activePrim[0], sizeof(Vec4f)*primSize);

				GetDebugManager()->m_MeshBBoxVB.Unmap(vt, dev);

				vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshBBoxVB.buf), &vboffs);

				vt->CmdDraw(Unwrap(cmd), primSize, 1, 0, 0);
			}

			// Draw adjacent primitives (green)
			uniforms.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
			// poke the color (this would be a good candidate for a push constant)
			data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*data = uniforms;
			GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);

			if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
			{
				VkDeviceSize vboffs = 0;
				Vec4f *ptr = (Vec4f *)GetDebugManager()->m_MeshBBoxVB.Map(vt, dev, vboffs, sizeof(Vec4f)*adjacentPrimVertices.size());

				memcpy(ptr, &adjacentPrimVertices[0], sizeof(Vec4f)*adjacentPrimVertices.size());

				GetDebugManager()->m_MeshBBoxVB.Unmap(vt, dev);

				vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshBBoxVB.buf), &vboffs);

				vt->CmdDraw(Unwrap(cmd), (uint32_t)adjacentPrimVertices.size(), 1, 0, 0);
			}

			////////////////////////////////////////////////////////////////
			// prepare to render dots
			float scale = 800.0f/float(m_DebugHeight);
			float asp = float(m_DebugWidth)/float(m_DebugHeight);

			uniforms.pointSpriteSize = Vec2f(scale/asp, scale);

			// Draw active vertex (blue)
			uniforms.color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
			// poke the color (this would be a good candidate for a push constant)
			data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*data = uniforms;
			GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
			
			// vertices are drawn with tri strips
			helper.topo = eTopology_TriangleStrip;
			cache = GetDebugManager()->CacheMeshDisplayPipelines(helper, helper);

			FloatVector vertSprite[4] = {
				activeVertex,
				activeVertex,
				activeVertex,
				activeVertex,
			};
			
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
			
			vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Solid]));
			
			{
				VkDeviceSize vboffs = 0;
				Vec4f *ptr = (Vec4f *)GetDebugManager()->m_MeshBBoxVB.Map(vt, dev, vboffs, sizeof(vertSprite));

				memcpy(ptr, &vertSprite[0], sizeof(vertSprite));

				GetDebugManager()->m_MeshBBoxVB.Unmap(vt, dev);

				vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshBBoxVB.buf), &vboffs);

				vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
			}

			// Draw inactive vertices (green)
			uniforms.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
			// poke the color (this would be a good candidate for a push constant)
			data = (meshuniforms *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*data = uniforms;
			GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);

			if(!inactiveVertices.empty())
			{
				VkDeviceSize vboffs = 0;
				FloatVector *ptr = (FloatVector *)GetDebugManager()->m_MeshBBoxVB.Map(vt, dev, vboffs, sizeof(vertSprite));
				
				for(size_t i=0; i < inactiveVertices.size(); i++)
				{
					*ptr++ = inactiveVertices[i];
					*ptr++ = inactiveVertices[i];
					*ptr++ = inactiveVertices[i];
					*ptr++ = inactiveVertices[i];
				}

				GetDebugManager()->m_MeshBBoxVB.Unmap(vt, dev);

				for(size_t i=0; i < inactiveVertices.size(); i++)
				{
					vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(GetDebugManager()->m_MeshBBoxVB.buf), &vboffs);

					vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

					vboffs += sizeof(FloatVector)*4;
				}
			}
		}
	}
	
	vt->CmdEndRenderPass(Unwrap(cmd));

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);
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

		if(outw.width > 0 && outw.height > 0)
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
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);
	
	// semaphore is short lived, so not wrapped, if it's cached (ideally)
	// then it should be wrapped
	VkSemaphore sem;
	VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT };

	VkResult vkr = vt->CreateSemaphore(Unwrap(dev), &semInfo, &sem);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->AcquireNextImageKHR(Unwrap(dev), Unwrap(outw.swap), UINT64_MAX, sem, &outw.curidx);
	RDCASSERT(vkr == VK_SUCCESS);

	vkr = vt->QueueWaitSemaphore(Unwrap(m_pDriver->GetQ()), sem);
	RDCASSERT(vkr == VK_SUCCESS);

	vt->DestroySemaphore(Unwrap(dev), sem);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier[] = {
		(void *)&outw.bbBarrier,
		(void *)&outw.colBarrier[outw.curidx],
		(void *)&outw.depthBarrier,
	};

	outw.depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.colBarrier[outw.curidx].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, depth ? 3 : 2, barrier);

	outw.depthBarrier.oldLayout = outw.depthBarrier.newLayout;
	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
	outw.colBarrier[outw.curidx].oldLayout = outw.colBarrier[outw.curidx].newLayout;

	vt->EndCommandBuffer(Unwrap(cmd));
}

void VulkanReplay::ClearOutputWindowColour(uint64_t id, float col[4])
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	vt->CmdClearColorImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkClearColorValue *)col, 1, &outw.bbBarrier.subresourceRange);

	vt->EndCommandBuffer(Unwrap(cmd));
}

void VulkanReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	VkClearDepthStencilValue ds = { depth, stencil };

	vt->CmdClearDepthStencilImage(Unwrap(cmd), Unwrap(outw.dsimg), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &ds, 1, &outw.depthBarrier.subresourceRange);

	vt->EndCommandBuffer(Unwrap(cmd));
}

void VulkanReplay::FlipOutputWindow(uint64_t id)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	void *barrier[] = {
		(void *)&outw.bbBarrier,
		(void *)&outw.colBarrier[outw.curidx],
	};

	// ensure rendering has completed before copying
	outw.bbBarrier.outputMask = VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT;
	outw.bbBarrier.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, barrier);
	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
	outw.bbBarrier.outputMask = 0;
	outw.bbBarrier.inputMask = 0;

	VkImageCopy cpy = {
		{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
		{ 0, 0, 0 },
		{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
		{ 0, 0, 0 },
		{ outw.width, outw.height, 1 },
	};

	VkImageResolve resolve = {
		{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
		{ 0, 0, 0 },
		{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
		{ 0, 0, 0 },
		{ outw.width, outw.height, 1 },
	};

	if(outw.dsimg != VK_NULL_HANDLE && VULKAN_MESH_VIEW_SAMPLES > 1U)
		vt->CmdResolveImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, Unwrap(outw.colimg[outw.curidx]), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &resolve);
	else
		vt->CmdCopyImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, Unwrap(outw.colimg[outw.curidx]), VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &cpy);
	
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.colBarrier[outw.curidx].newLayout = VK_IMAGE_LAYOUT_PRESENT_SOURCE_KHR;

	// not sure what input mask should be for present, so be conservative.
	// make sure copy has completed before present

	outw.colBarrier[outw.curidx].outputMask = VK_MEMORY_OUTPUT_TRANSFER_BIT;
	outw.colBarrier[outw.curidx].inputMask = VK_MEMORY_INPUT_TRANSFER_BIT|VK_MEMORY_INPUT_INPUT_ATTACHMENT_BIT|VK_MEMORY_INPUT_SHADER_READ_BIT;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, barrier);

	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
	outw.colBarrier[outw.curidx].oldLayout = outw.colBarrier[outw.curidx].newLayout;
	
	outw.colBarrier[outw.curidx].outputMask = 0;
	outw.colBarrier[outw.curidx].inputMask = 0;

	vt->EndCommandBuffer(Unwrap(cmd));
	
	// submit all the cmds we recorded
	m_pDriver->SubmitCmds();

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, UnwrapPtr(outw.swap), &outw.curidx };

	vt->QueuePresentKHR(Unwrap(m_pDriver->GetQ()), &presentInfo);

	m_pDriver->FlushQ();
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
	m_OutputWindows[id].m_ResourceManager = GetResourceManager();

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

vector<byte> VulkanReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len)
{
	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkBuffer srcBuf = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(buff);
	
	if(len == 0)
	{
		len = m_pDriver->m_CreationInfo.m_Buffer[buff].size - offset;
	}

	if(len > 0 && VkDeviceSize(offset+len) > m_pDriver->m_CreationInfo.m_Buffer[buff].size)
	{
		RDCWARN("Attempting to read off the end of the array. Will be clamped");
		len = RDCMIN(len, m_pDriver->m_CreationInfo.m_Buffer[buff].size - offset);
	}

	vector<byte> ret;
	ret.resize((size_t)len);

	// VKTODOMED - coarse: wait for all writes to this buffer
	vt->DeviceWaitIdle(Unwrap(dev));

	VkDeviceMemory readbackmem = VK_NULL_HANDLE;
	VkBuffer destbuf = VK_NULL_HANDLE;
	VkResult vkr = VK_SUCCESS;
	byte *pData = NULL;

	{
		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

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
	}

	m_pDriver->SubmitCmds();
	m_pDriver->FlushQ();

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
	for(int32_t i=0; i < m_VulkanPipelineState.Pass.framebuffer.attachments.count; i++)
	{
		if(m_VulkanPipelineState.Pass.framebuffer.attachments[i].view == id ||
			 m_VulkanPipelineState.Pass.framebuffer.attachments[i].img == id)
				return true;
	}

	return false;
}

void VulkanReplay::FileChanged()
{
}

void VulkanReplay::SavePipelineState()
{
	{
		const WrappedVulkan::PartialReplayData::StateVector &state = m_pDriver->m_PartialReplayData.state;
		VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

		VulkanResourceManager *rm = m_pDriver->GetResourceManager();

		m_VulkanPipelineState = VulkanPipelineState();
		
		// General pipeline properties
		m_VulkanPipelineState.compute.obj = rm->GetOriginalID(state.compute.pipeline);
		m_VulkanPipelineState.graphics.obj = rm->GetOriginalID(state.graphics.pipeline);

		if(state.compute.pipeline != ResourceId())
			m_VulkanPipelineState.compute.flags = c.m_Pipeline[state.compute.pipeline].flags;

		if(state.graphics.pipeline != ResourceId())
		{
			const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.graphics.pipeline];

			m_VulkanPipelineState.graphics.flags = p.flags;

			// Input Assembly
			m_VulkanPipelineState.IA.ibuffer.buf = rm->GetOriginalID(state.ibuffer.buf);
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
				m_VulkanPipelineState.VI.vbuffers[i].buffer = rm->GetOriginalID(state.vbuffers[i].buf);
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
				stages[i]->BindpointMapping = c.m_Shader[p.shaders[i]].mapping;
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
			m_VulkanPipelineState.CB.alphaToOneEnable = p.alphaToOneEnable;
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
			m_VulkanPipelineState.Pass.renderpass.obj = rm->GetOriginalID(state.renderPass);
			m_VulkanPipelineState.Pass.renderpass.inputAttachments = c.m_RenderPass[state.renderPass].subpasses[state.subpass].inputAttachments;
			m_VulkanPipelineState.Pass.renderpass.colorAttachments = c.m_RenderPass[state.renderPass].subpasses[state.subpass].colorAttachments;
			m_VulkanPipelineState.Pass.renderpass.depthstencilAttachment = c.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;

			m_VulkanPipelineState.Pass.framebuffer.obj = rm->GetOriginalID(state.framebuffer);

			m_VulkanPipelineState.Pass.framebuffer.width = c.m_Framebuffer[state.framebuffer].width;
			m_VulkanPipelineState.Pass.framebuffer.height = c.m_Framebuffer[state.framebuffer].height;
			m_VulkanPipelineState.Pass.framebuffer.layers = c.m_Framebuffer[state.framebuffer].layers;

			create_array_uninit(m_VulkanPipelineState.Pass.framebuffer.attachments, c.m_Framebuffer[state.framebuffer].attachments.size());
			for(size_t i=0; i < c.m_Framebuffer[state.framebuffer].attachments.size(); i++)
			{
				ResourceId viewid = c.m_Framebuffer[state.framebuffer].attachments[i].view;

				if(viewid != ResourceId())
				{
					m_VulkanPipelineState.Pass.framebuffer.attachments[i].view = rm->GetOriginalID(viewid);
					m_VulkanPipelineState.Pass.framebuffer.attachments[i].img = rm->GetOriginalID(c.m_ImageView[viewid].image);

					m_VulkanPipelineState.Pass.framebuffer.attachments[i].baseMip = c.m_ImageView[viewid].range.baseMipLevel;
					m_VulkanPipelineState.Pass.framebuffer.attachments[i].baseLayer = c.m_ImageView[viewid].range.baseArrayLayer;
				}
				else
				{
					m_VulkanPipelineState.Pass.framebuffer.attachments[i].view = ResourceId();
					m_VulkanPipelineState.Pass.framebuffer.attachments[i].img = ResourceId();

					m_VulkanPipelineState.Pass.framebuffer.attachments[i].baseMip = 0;
					m_VulkanPipelineState.Pass.framebuffer.attachments[i].baseLayer = 0;
				}
			}

			m_VulkanPipelineState.Pass.renderArea.x = state.renderArea.offset.x;
			m_VulkanPipelineState.Pass.renderArea.y = state.renderArea.offset.y;
			m_VulkanPipelineState.Pass.renderArea.width = state.renderArea.extent.width;
			m_VulkanPipelineState.Pass.renderArea.height = state.renderArea.extent.height;
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

					ResourceId layoutId = m_pDriver->m_DescriptorSetState[src].layout;

					dst.descset = rm->GetOriginalID(src);
					dst.layout = rm->GetOriginalID(layoutId);
					create_array_uninit(dst.bindings, m_pDriver->m_DescriptorSetState[src].currentBindings.size());
					for(size_t b=0; b < m_pDriver->m_DescriptorSetState[src].currentBindings.size(); b++)
					{
						VkDescriptorInfo *info = m_pDriver->m_DescriptorSetState[src].currentBindings[b];
						const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[b];

						bool dynamicOffset = false;

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
							case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:  dst.bindings[b].type = eBindType_ReadOnlyBuffer; dynamicOffset = true; break;
							case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:  dst.bindings[b].type = eBindType_ReadWriteBuffer; dynamicOffset = true; break;
							case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:        dst.bindings[b].type = eBindType_InputAttachment; break;
							default:
								dst.bindings[b].type = eBindType_Unknown;
								RDCERR("Unexpected descriptor type");
						}
						
						create_array_uninit(dst.bindings[b].binds, layoutBind.arraySize);
						for(uint32_t a=0; a < layoutBind.arraySize; a++)
						{
							if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
							{
								if(layoutBind.immutableSampler)
									dst.bindings[b].binds[a].sampler = layoutBind.immutableSampler[a];
								else if(info[a].sampler != VK_NULL_HANDLE)
									dst.bindings[b].binds[a].sampler = rm->GetNonDispWrapper(info[a].sampler)->id;

								if(dst.bindings[b].binds[a].sampler != ResourceId())
								{
									VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement &el = dst.bindings[b].binds[a];
									const VulkanCreationInfo::Sampler &sampl = c.m_Sampler[el.sampler];

									el.sampler = rm->GetOriginalID(el.sampler);

									// sampler info
									el.mag = ToStr::Get(sampl.magFilter);
									el.min = ToStr::Get(sampl.minFilter);
									el.mip = ToStr::Get(sampl.mipMode);
									el.addrU = ToStr::Get(sampl.address[0]);
									el.addrV = ToStr::Get(sampl.address[1]);
									el.addrW = ToStr::Get(sampl.address[2]);
									el.mipBias = sampl.mipLodBias;
									el.maxAniso = sampl.maxAnisotropy;
									el.compareEnable = sampl.compareEnable;
									el.comparison = ToStr::Get(sampl.compareOp);
									el.minlod = sampl.minLod;
									el.maxlod = sampl.maxLod;
									el.borderEnable = false;
									if(sampl.address[0] == VK_TEX_ADDRESS_MODE_CLAMP_BORDER ||
										sampl.address[1] == VK_TEX_ADDRESS_MODE_CLAMP_BORDER ||
										sampl.address[2] == VK_TEX_ADDRESS_MODE_CLAMP_BORDER)
										el.borderEnable = true;
									el.border = ToStr::Get(sampl.borderColor);
									el.unnormalized = sampl.unnormalizedCoordinates;
								}
							}

							if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ||
								 layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
							{
								VkImageView view = info[a].imageView;

								if(view != VK_NULL_HANDLE)
								{
									ResourceId viewid = rm->GetNonDispWrapper(view)->id;

									dst.bindings[b].binds[a].view = rm->GetOriginalID(viewid);
									dst.bindings[b].binds[a].res = rm->GetOriginalID(c.m_ImageView[viewid].image);
									dst.bindings[b].binds[a].baseMip = c.m_ImageView[viewid].range.baseMipLevel;
									dst.bindings[b].binds[a].baseLayer = c.m_ImageView[viewid].range.baseArrayLayer;
								}
								else
								{
									dst.bindings[b].binds[a].view = ResourceId();
									dst.bindings[b].binds[a].res = ResourceId();
									dst.bindings[b].binds[a].baseMip = 0;
									dst.bindings[b].binds[a].baseLayer = 0;
								}
							}
							if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
							{
								VkBufferView view = info[a].bufferView;
								
								if(view != VK_NULL_HANDLE)
								{
									ResourceId viewid = rm->GetNonDispWrapper(view)->id;

									dst.bindings[b].binds[a].view = rm->GetOriginalID(viewid);
									dst.bindings[b].binds[a].res = rm->GetOriginalID(c.m_BufferView[viewid].buffer);
									dst.bindings[b].binds[a].offset = c.m_BufferView[viewid].offset;
									if(dynamicOffset)
										dst.bindings[b].binds[a].offset += *(uint32_t *)&info[a].imageLayout;
									dst.bindings[b].binds[a].size = c.m_BufferView[viewid].size;
								}
								else
								{
									dst.bindings[b].binds[a].view = ResourceId();
									dst.bindings[b].binds[a].res = ResourceId();
									dst.bindings[b].binds[a].offset = 0;
									dst.bindings[b].binds[a].size = 0;
								}
							}
							if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
							{
								dst.bindings[b].binds[a].view = ResourceId();

								if(info[a].bufferInfo.buffer != VK_NULL_HANDLE)
									dst.bindings[b].binds[a].res = rm->GetOriginalID(rm->GetNonDispWrapper(info[a].bufferInfo.buffer)->id);

								dst.bindings[b].binds[a].offset = info[a].bufferInfo.offset;
								if(dynamicOffset)
									dst.bindings[b].binds[a].offset += *(uint32_t *)&info[a].imageLayout;

								dst.bindings[b].binds[a].size = info[a].bufferInfo.range;
							}
						}
					}
				}
			}
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

	auto it = m_pDriver->m_CreationInfo.m_Shader.find(shader);
	
	if(it == m_pDriver->m_CreationInfo.m_Shader.end())
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

	if(c.bufferBacked)
	{
		FillCBufferVariables(c.variables, outvars, data, zero);
	}
	else
	{
		vector<byte> pushdata;
		pushdata.resize(sizeof(m_pDriver->m_PartialReplayData.state.pushconsts));
		memcpy(&pushdata[0], m_pDriver->m_PartialReplayData.state.pushconsts, pushdata.size());
		FillCBufferVariables(c.variables, outvars, pushdata, zero);
	}

}

bool VulkanReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[texid];
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(texid);

	// VKTODOMED handle multiple subresources with different layouts etc
	VkImageLayout origLayout = layouts.subresourceStates[0].newLayout;
	VkImageView liveImView = iminfo.view;

	if(liveImView == VK_NULL_HANDLE)
	{
		VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(liveIm), VK_IMAGE_VIEW_TYPE_2D,
			iminfo.format,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, RDCMAX(1U, (uint32_t)iminfo.mipLevels), 0, 1, },
			0
		};

		// Only needed on AMD - does the wrong thing on nvidia - so commented for now while AMD
		// drivers aren't on 0.9.2
		//if(iminfo.format == VK_FORMAT_B8G8R8A8_UNORM || iminfo.format == VK_FORMAT_B8G8R8A8_SRGB)
			//std::swap(viewInfo.channels.r, viewInfo.channels.b);

		VkResult vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, &iminfo.view);
		RDCASSERT(vkr == VK_SUCCESS);

		ResourceId viewid = m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), iminfo.view);
		// register as a live-only resource, so it is cleaned up properly
		m_pDriver->GetResourceManager()->AddLiveResource(viewid, iminfo.view);

		liveImView = iminfo.view;
	}
	
	VkDescriptorInfo desc = {0};
	desc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc.imageView = Unwrap(liveImView);
	desc.sampler = Unwrap(GetDebugManager()->m_PointSampler);

	VkDescriptorInfo bufdescs[3];
	RDCEraseEl(bufdescs);
	GetDebugManager()->m_MinMaxTileResult.FillDescriptor(bufdescs[0]);
	GetDebugManager()->m_MinMaxResult.FillDescriptor(bufdescs[1]);
	GetDebugManager()->m_HistogramUBO.FillDescriptor(bufdescs[2]);

	VkWriteDescriptorSet writeSet[] = {

		// first pass on tiles
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufdescs[0] // destination = tile result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufdescs[0] // source = unused, bind tile result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufdescs[2]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc
		},

		// second pass from tiles to result
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[1]),
			0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufdescs[1] // destination = result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[1]),
			1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufdescs[0] // source = tile result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[1]),
			2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufdescs[2]
		},
	};

	vt->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);
	
	histogramuniforms *data = (histogramuniforms *)GetDebugManager()->m_HistogramUBO.Map(vt, dev, NULL);
	
	data->HistogramTextureResolution.x = (float)RDCMAX(uint32_t(iminfo.extent.width)>>mip, 1U);
	data->HistogramTextureResolution.y = (float)RDCMAX(uint32_t(iminfo.extent.height)>>mip, 1U);
	data->HistogramTextureResolution.z = (float)RDCMAX(uint32_t(iminfo.arraySize)>>mip, 1U);
	data->HistogramSlice = (float)sliceFace;
	data->HistogramMip = (int)mip;
	data->HistogramNumSamples = iminfo.samples;
	data->HistogramSample = (int)RDCCLAMP(sample, 0U, uint32_t(iminfo.samples)-1);
	if(sample == ~0U) data->HistogramSample = -iminfo.samples;
	data->HistogramMin = 0.0f;
	data->HistogramMax = 1.0f;
	data->HistogramChannels = 0xf;

	if(iminfo.type == VK_IMAGE_TYPE_3D)
		data->HistogramSlice = float(sliceFace)/float(iminfo.extent.depth);
	
	GetDebugManager()->m_HistogramUBO.Unmap(vt, dev);

	VkImageMemoryBarrier srcimBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, origLayout, VK_IMAGE_LAYOUT_GENERAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	// ensure all previous writes have completed
	srcimBarrier.outputMask =
		VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT|
		VK_MEMORY_OUTPUT_SHADER_WRITE_BIT|
		VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
		VK_MEMORY_OUTPUT_TRANSFER_BIT;
	// before we go reading
	srcimBarrier.inputMask = VK_MEMORY_INPUT_SHADER_READ_BIT;

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	void *barrier = (void *)&srcimBarrier;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
	srcimBarrier.oldLayout = srcimBarrier.newLayout;

	srcimBarrier.outputMask = 0;
	srcimBarrier.inputMask = 0;
	
	int blocksX = (int)ceil(iminfo.extent.width/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(iminfo.extent.height/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

	vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_MinMaxTilePipe));
	vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipeLayout),
														0, 1, UnwrapPtr(GetDebugManager()->m_HistogramDescSet[0]), 0, NULL);

	vt->CmdDispatch(Unwrap(cmd), blocksX, blocksY, 1);

	VkBufferMemoryBarrier tilebarrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, NULL,
		VK_MEMORY_OUTPUT_SHADER_WRITE_BIT, VK_MEMORY_INPUT_SHADER_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(GetDebugManager()->m_MinMaxTileResult.buf),
		0, GetDebugManager()->m_MinMaxTileResult.totalsize,
	};

	// image layout back to normal
	srcimBarrier.newLayout = origLayout;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	// ensure shader writes complete before coalescing the tiles
	barrier = (void *)&tilebarrier;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_MinMaxResultPipe));
	vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipeLayout),
														0, 1, UnwrapPtr(GetDebugManager()->m_HistogramDescSet[1]), 0, NULL);

	vt->CmdDispatch(Unwrap(cmd), 1, 1, 1);

	// ensure shader writes complete before copying back to readback buffer
	tilebarrier.inputMask = VK_MEMORY_INPUT_TRANSFER_BIT;
	tilebarrier.buffer = Unwrap(GetDebugManager()->m_MinMaxResult.buf);
	tilebarrier.size = GetDebugManager()->m_MinMaxResult.totalsize;
	
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	VkBufferCopy bufcopy = {
		0, 0, GetDebugManager()->m_MinMaxResult.totalsize,
	};

	vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_MinMaxResult.buf), Unwrap(GetDebugManager()->m_MinMaxReadback.buf), 1, &bufcopy);
	
	// wait for copy to complete before mapping
	tilebarrier.outputMask = VK_MEMORY_OUTPUT_TRANSFER_BIT;
	tilebarrier.inputMask = VK_MEMORY_INPUT_HOST_READ_BIT;
	tilebarrier.buffer = Unwrap(GetDebugManager()->m_MinMaxReadback.buf);
	tilebarrier.size = GetDebugManager()->m_MinMaxResult.totalsize;
	
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vt->EndCommandBuffer(Unwrap(cmd));
	
	// submit cmds and wait for idle so we can readback
	m_pDriver->SubmitCmds();
	m_pDriver->FlushQ();
	
	Vec4f *minmax = (Vec4f *)GetDebugManager()->m_MinMaxReadback.Map(vt, dev, NULL);
	
	minval[0] = minmax[0].x;
	minval[1] = minmax[0].y;
	minval[2] = minmax[0].z;
	minval[3] = minmax[0].w;

	maxval[0] = minmax[1].x;
	maxval[1] = minmax[1].y;
	maxval[2] = minmax[1].z;
	maxval[3] = minmax[1].w;

	GetDebugManager()->m_MinMaxReadback.Unmap(vt, dev);
	
	return true;
}

bool VulkanReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
{
	if(minval >= maxval) return false;

	VkDevice dev = m_pDriver->GetDev();
	VkCmdBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[texid];
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(texid);

	// VKTODOMED handle multiple subresources with different layouts etc
	VkImageLayout origLayout = layouts.subresourceStates[0].newLayout;
	VkImageView liveImView = iminfo.view;

	if(liveImView == VK_NULL_HANDLE)
	{
		VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(liveIm), VK_IMAGE_VIEW_TYPE_2D,
			iminfo.format,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, RDCMAX(1U, (uint32_t)iminfo.mipLevels), 0, 1, },
			0
		};

		// Only needed on AMD - does the wrong thing on nvidia - so commented for now while AMD
		// drivers aren't on 0.9.2
		//if(iminfo.format == VK_FORMAT_B8G8R8A8_UNORM || iminfo.format == VK_FORMAT_B8G8R8A8_SRGB)
			//std::swap(viewInfo.channels.r, viewInfo.channels.b);

		VkResult vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, &iminfo.view);
		RDCASSERT(vkr == VK_SUCCESS);

		ResourceId viewid = m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), iminfo.view);
		// register as a live-only resource, so it is cleaned up properly
		m_pDriver->GetResourceManager()->AddLiveResource(viewid, iminfo.view);

		liveImView = iminfo.view;
	}
	
	VkDescriptorInfo desc = {0};
	desc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	desc.imageView = Unwrap(liveImView);
	desc.sampler = Unwrap(GetDebugManager()->m_PointSampler);

	VkDescriptorInfo bufdescs[2];
	RDCEraseEl(bufdescs);
	GetDebugManager()->m_HistogramBuf.FillDescriptor(bufdescs[0]);
	GetDebugManager()->m_HistogramUBO.FillDescriptor(bufdescs[1]);

	VkWriteDescriptorSet writeSet[] = {

		// histogram pass
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufdescs[0] // destination = histogram result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufdescs[0] // source = unused, bind histogram result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufdescs[1]
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &desc
		},
	};

	vt->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);
	
	histogramuniforms *data = (histogramuniforms *)GetDebugManager()->m_HistogramUBO.Map(vt, dev, NULL);
	
	data->HistogramTextureResolution.x = (float)RDCMAX(uint32_t(iminfo.extent.width)>>mip, 1U);
	data->HistogramTextureResolution.y = (float)RDCMAX(uint32_t(iminfo.extent.height)>>mip, 1U);
	data->HistogramTextureResolution.z = (float)RDCMAX(uint32_t(iminfo.arraySize)>>mip, 1U);
	data->HistogramSlice = (float)sliceFace;
	data->HistogramMip = (int)mip;
	data->HistogramNumSamples = iminfo.samples;
	data->HistogramSample = (int)RDCCLAMP(sample, 0U, uint32_t(iminfo.samples)-1);
	if(sample == ~0U) data->HistogramSample = -iminfo.samples;
	data->HistogramMin = minval;
	data->HistogramMax = maxval;

	uint32_t chans = 0;
	if(channels[0]) chans |= 0x1;
	if(channels[1]) chans |= 0x2;
	if(channels[2]) chans |= 0x4;
	if(channels[3]) chans |= 0x8;
	
	data->HistogramChannels = chans;
	data->HistogramFlags = 0;

	if(iminfo.type == VK_IMAGE_TYPE_3D)
		data->HistogramSlice = float(sliceFace)/float(iminfo.extent.depth);
	
	GetDebugManager()->m_HistogramUBO.Unmap(vt, dev);

	VkImageMemoryBarrier srcimBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, origLayout, VK_IMAGE_LAYOUT_GENERAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};

	// ensure all previous writes have completed
	srcimBarrier.outputMask =
		VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT|
		VK_MEMORY_OUTPUT_SHADER_WRITE_BIT|
		VK_MEMORY_OUTPUT_DEPTH_STENCIL_ATTACHMENT_BIT|
		VK_MEMORY_OUTPUT_TRANSFER_BIT;
	// before we go reading
	srcimBarrier.inputMask = VK_MEMORY_INPUT_SHADER_READ_BIT;

	VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
	
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	void *barrier = (void *)&srcimBarrier;

	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);
	srcimBarrier.oldLayout = srcimBarrier.newLayout;

	srcimBarrier.outputMask = 0;
	srcimBarrier.inputMask = 0;
	
	int blocksX = (int)ceil(iminfo.extent.width/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(iminfo.extent.height/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

	vt->CmdFillBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_HistogramBuf.buf), 0, GetDebugManager()->m_HistogramBuf.totalsize, 0);

	vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipe));
	vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipeLayout),
														0, 1, UnwrapPtr(GetDebugManager()->m_HistogramDescSet[0]), 0, NULL);

	vt->CmdDispatch(Unwrap(cmd), blocksX, blocksY, 1);

	VkBufferMemoryBarrier tilebarrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, NULL,
		VK_MEMORY_OUTPUT_SHADER_WRITE_BIT, VK_MEMORY_INPUT_TRANSFER_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(GetDebugManager()->m_HistogramBuf.buf),
		0, GetDebugManager()->m_HistogramBuf.totalsize,
	};

	// image layout back to normal
	srcimBarrier.newLayout = origLayout;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	// ensure shader writes complete before copying to readback buf
	barrier = (void *)&tilebarrier;
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	VkBufferCopy bufcopy = {
		0, 0, GetDebugManager()->m_HistogramBuf.totalsize,
	};

	vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_HistogramBuf.buf), Unwrap(GetDebugManager()->m_HistogramReadback.buf), 1, &bufcopy);
	
	// wait for copy to complete before mapping
	tilebarrier.outputMask = VK_MEMORY_OUTPUT_TRANSFER_BIT;
	tilebarrier.inputMask = VK_MEMORY_INPUT_HOST_READ_BIT;
	tilebarrier.buffer = Unwrap(GetDebugManager()->m_HistogramReadback.buf);
	tilebarrier.size = GetDebugManager()->m_HistogramReadback.totalsize;
	
	vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, &barrier);

	vt->EndCommandBuffer(Unwrap(cmd));
	
	// submit cmds and wait for idle so we can readback
	m_pDriver->SubmitCmds();
	m_pDriver->FlushQ();

	uint32_t *buckets = (uint32_t *)GetDebugManager()->m_HistogramReadback.Map(vt, dev, NULL);

	histogram.assign(buckets, buckets+HGRAM_NUM_BUCKETS);

	GetDebugManager()->m_HistogramReadback.Unmap(vt, dev);

	return true;
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
	RDCERR("Should never hit SetContextFilter");
}

void VulkanReplay::FreeTargetResource(ResourceId id)
{
	// won't get hit until BuildTargetShader is implemented
	VULKANNOTIMP("FreeTargetResource");
}

void VulkanReplay::FreeCustomShader(ResourceId id)
{
	VULKANNOTIMP("FreeCustomShader");
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
	// won't get hit until BuildTargetShader is implemented
	VULKANNOTIMP("ReplaceResource");
}

void VulkanReplay::RemoveReplacement(ResourceId id)
{
	// won't get hit until BuildTargetShader is implemented
	VULKANNOTIMP("RemoveReplacement");
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
	VULKANNOTIMP("BuildTargetShader");
	if(errors) *errors = "Shader edit & replace is not yet supported on Vulkan";
	if(id) *id = ResourceId();
}

void VulkanReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	VULKANNOTIMP("BuildCustomShader");
}

vector<PixelModification> VulkanReplay::PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx)
{
	VULKANNOTIMP("PixelHistory");
	return vector<PixelModification>();
}

ShaderDebugTrace VulkanReplay::DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	VULKANNOTIMP("DebugVertex");
	return ShaderDebugTrace();
}

ShaderDebugTrace VulkanReplay::DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	VULKANNOTIMP("DebugPixel");
	return ShaderDebugTrace();
}

ShaderDebugTrace VulkanReplay::DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
	VULKANNOTIMP("DebugThread");
	return ShaderDebugTrace();
}

ResourceId VulkanReplay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip)
{
	VULKANNOTIMP("ApplyCustomShader");
	return ResourceId();
}

ResourceId VulkanReplay::CreateProxyTexture(FetchTexture templateTex)
{
	VULKANNOTIMP("CreateProxyTexture");
	return ResourceId();
}

void VulkanReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize)
{
	VULKANNOTIMP("SetProxyTextureData");
}

ResourceId VulkanReplay::CreateProxyBuffer(FetchBuffer templateBuf)
{
	VULKANNOTIMP("CreateProxyBuffer");
	return ResourceId();
}

void VulkanReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
	VULKANNOTIMP("SetProxyTextureData");
}

// in vk_replay_platform.cpp
void *LoadVulkanLibrary();

ReplayCreateStatus Vulkan_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
	RDCDEBUG("Creating a VulkanReplay replay device");

	void *module = LoadVulkanLibrary();
	
	if(module == NULL)
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

	InitReplayTables(module);

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
