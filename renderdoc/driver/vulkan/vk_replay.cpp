/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include "data/spv/debuguniforms.h"

#include <float.h>

VulkanReplay::OutputWindow::OutputWindow() : wnd(NULL_WND_HANDLE), width(0), height(0),
	dsimg(VK_NULL_HANDLE), dsmem(VK_NULL_HANDLE)
{
	surface = VK_NULL_HANDLE;
	swap = VK_NULL_HANDLE;
	for(size_t i=0; i < ARRAY_COUNT(colimg); i++)
		colimg[i] = VK_NULL_HANDLE;

	fresh = true;

	bb = VK_NULL_HANDLE;
	bbview = VK_NULL_HANDLE;
	fb = VK_NULL_HANDLE;
	fbdepth = VK_NULL_HANDLE;
	rp = VK_NULL_HANDLE;
	rpdepth = VK_NULL_HANDLE;

	VkImageMemoryBarrier t = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED,
		0, 0, // MULTIDEVICE - need to actually pick the right queue family here maybe?
		VK_NULL_HANDLE,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	for(size_t i=0; i < ARRAY_COUNT(colBarrier); i++)
		colBarrier[i] = t;

	bbBarrier = t;

	t.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthBarrier = t;
	depthBarrier.srcAccessMask = depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
		vt->DestroyRenderPass(Unwrap(device), Unwrap(rp), NULL);
		GetResourceManager()->ReleaseWrappedResource(rp);
		rp = VK_NULL_HANDLE;
		
		vt->DestroyImage(Unwrap(device), Unwrap(bb), NULL);
		GetResourceManager()->ReleaseWrappedResource(bb);
		
		vt->DestroyImageView(Unwrap(device), Unwrap(bbview), NULL);
		GetResourceManager()->ReleaseWrappedResource(bbview);
		vt->FreeMemory(Unwrap(device), Unwrap(bbmem), NULL);
		GetResourceManager()->ReleaseWrappedResource(bbmem);
		vt->DestroyFramebuffer(Unwrap(device), Unwrap(fb), NULL);
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
		vt->DestroyRenderPass(Unwrap(device), Unwrap(rpdepth), NULL);
		GetResourceManager()->ReleaseWrappedResource(rpdepth);
		rpdepth = VK_NULL_HANDLE;
		
		vt->DestroyImage(Unwrap(device), Unwrap(dsimg), NULL);
		GetResourceManager()->ReleaseWrappedResource(dsimg);
		
		vt->DestroyImageView(Unwrap(device), Unwrap(dsview), NULL);
		GetResourceManager()->ReleaseWrappedResource(dsview);
		vt->FreeMemory(Unwrap(device), Unwrap(dsmem), NULL);
		GetResourceManager()->ReleaseWrappedResource(dsmem);
		vt->DestroyFramebuffer(Unwrap(device), Unwrap(fbdepth), NULL);
		GetResourceManager()->ReleaseWrappedResource(fbdepth);
		
		dsview = VK_NULL_HANDLE;
		dsimg = VK_NULL_HANDLE;
		dsmem = VK_NULL_HANDLE;
		fbdepth = VK_NULL_HANDLE;
		rpdepth = VK_NULL_HANDLE;
	}

	if(swap != VK_NULL_HANDLE)
	{
		vt->DestroySwapchainKHR(Unwrap(device), Unwrap(swap), NULL);
		GetResourceManager()->ReleaseWrappedResource(swap);
	}

	if(surface != VK_NULL_HANDLE)
	{
		ObjDisp(driver->GetInstance())->DestroySurfaceKHR(Unwrap(driver->GetInstance()), Unwrap(surface), NULL);
		GetResourceManager()->ReleaseWrappedResource(surface);
		surface = VK_NULL_HANDLE;
	}
}

void VulkanReplay::OutputWindow::Create(WrappedVulkan *driver, VkDevice device, bool depth)
{
	const VkLayerDispatchTable *vt = ObjDisp(device);
	VkInstance inst = driver->GetInstance();
	VkPhysicalDevice phys = driver->GetPhysDev();

	// save the old swapchain so it isn't destroyed
	VkSwapchainKHR old = swap;
	swap = VK_NULL_HANDLE;

	// we can't destroy the surface until all swapchains are destroyed, so 
	// we also save the surface here and restore it back after destroy
	VkSurfaceKHR oldsurf = surface;
	surface = VK_NULL_HANDLE;

	Destroy(driver, device);

	surface = oldsurf;

	fresh = true;

	if(surface == VK_NULL_HANDLE)
	{
		CreateSurface(inst);
		
		GetResourceManager()->WrapResource(Unwrap(inst), surface);
	}

	// sensible defaults
	VkFormat imformat = VK_FORMAT_B8G8R8A8_UNORM;
	VkPresentModeKHR presentmode = VK_PRESENT_MODE_FIFO_KHR;
	VkColorSpaceKHR imcolspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	VkResult vkr = VK_SUCCESS;

	VkSurfaceCapabilitiesKHR capabilities;

	ObjDisp(inst)->GetPhysicalDeviceSurfaceCapabilitiesKHR(Unwrap(phys), Unwrap(surface), &capabilities);
	
	RDCASSERT(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	// AMD currently doesn't report this capability but I believe it's safe.
	RDCASSERT(driver->IsAMD() || capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	RDCASSERT(capabilities.minImageCount <= 2 && (2 <= capabilities.maxImageCount || capabilities.maxImageCount == 0));

	// check format and present mode from driver
	{
		uint32_t numFormats = 0;

		vkr = ObjDisp(inst)->GetPhysicalDeviceSurfaceFormatsKHR(Unwrap(phys), Unwrap(surface), &numFormats, NULL);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		if(numFormats > 0)
		{
			VkSurfaceFormatKHR *formats = new VkSurfaceFormatKHR[numFormats];

			vkr = ObjDisp(inst)->GetPhysicalDeviceSurfaceFormatsKHR(Unwrap(phys), Unwrap(surface), &numFormats, formats);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

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

		vkr = ObjDisp(inst)->GetPhysicalDeviceSurfacePresentModesKHR(Unwrap(phys), Unwrap(surface), &numModes, NULL);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		if(numModes > 0)
		{
			VkPresentModeKHR *modes = new VkPresentModeKHR[numModes];

			vkr = ObjDisp(inst)->GetPhysicalDeviceSurfacePresentModesKHR(Unwrap(phys), Unwrap(surface), &numModes, modes);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

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

	VkBool32 supported = false;
	ObjDisp(inst)->GetPhysicalDeviceSurfaceSupportKHR(Unwrap(phys), driver->GetQFamilyIdx(), Unwrap(surface), &supported);

	// can't really recover from this anyway
	RDCASSERT(supported);

	VkSwapchainCreateInfoKHR swapInfo = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL,
			0, Unwrap(surface),
			2, imformat, imcolspace, { width, height }, 1,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			presentmode, true,
			Unwrap(old),
	};

	vkr = vt->CreateSwapchainKHR(Unwrap(device), &swapInfo, NULL, &swap);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	GetResourceManager()->WrapResource(Unwrap(device), swap);

	if(old != VK_NULL_HANDLE)
	{
		vt->DestroySwapchainKHR(Unwrap(device), Unwrap(old), NULL);
		GetResourceManager()->ReleaseWrappedResource(old);
	}

	vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swap), &numImgs, NULL);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	VkImage* imgs = new VkImage[numImgs];
	vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swap), &numImgs, imgs);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D, VK_FORMAT_D32_SFLOAT,
			{ width, height, 1 },
			1, 1, VULKAN_MESH_VIEW_SAMPLES,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		vkr = vt->CreateImage(Unwrap(device), &imInfo, NULL, &dsimg);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), dsimg);

		VkMemoryRequirements mrq = {0};

		vt->GetImageMemoryRequirements(Unwrap(device), Unwrap(dsimg), &mrq);

		VkMemoryAllocateInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocateMemory(Unwrap(device), &allocInfo, NULL, &dsmem);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), dsmem);

		vkr = vt->BindImageMemory(Unwrap(device), Unwrap(dsimg), Unwrap(dsmem), 0);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		depthBarrier.image = Unwrap(dsimg);
		depthBarrier.oldLayout = depthBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		
		VkImageViewCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			0, Unwrap(dsimg), VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_D32_SFLOAT,
			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 },
		};

		vkr = vt->CreateImageView(Unwrap(device), &info, NULL, &dsview);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), dsview);
	}

	{
		VkAttachmentDescription attDesc[] = {
			{
				0, imformat, depth ? VULKAN_MESH_VIEW_SAMPLES : VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			},
			{
				0, VK_FORMAT_D32_SFLOAT, depth ? VULKAN_MESH_VIEW_SAMPLES : VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			}
		};

		VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription sub = {
			0, VK_PIPELINE_BIND_POINT_GRAPHICS,
			0, NULL, // inputs
			1, &attRef, // color
			NULL, // resolve
			NULL, // depth-stencil
			0, NULL, // preserve
		};

		VkRenderPassCreateInfo rpinfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0,
				1, attDesc,
				1, &sub,
				0, NULL, // dependencies
		};

		vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, NULL, &rp);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), rp);

		if(dsimg != VK_NULL_HANDLE)
		{
			sub.pDepthStencilAttachment = &dsRef;

			rpinfo.attachmentCount = 2;

			vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, NULL, &rpdepth);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), rpdepth);
		}
	}

	{
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D, imformat, { width, height, 1 },
			1, 1, depth ? VULKAN_MESH_VIEW_SAMPLES : VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		vkr = vt->CreateImage(Unwrap(device), &imInfo, NULL, &bb);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), bb);

		VkMemoryRequirements mrq = {0};

		vt->GetImageMemoryRequirements(Unwrap(device), Unwrap(bb), &mrq);

		VkMemoryAllocateInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
			mrq.size, driver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocateMemory(Unwrap(device), &allocInfo, NULL, &bbmem);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), bbmem);

		vkr = vt->BindImageMemory(Unwrap(device), Unwrap(bb), Unwrap(bbmem), 0);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		bbBarrier.image = Unwrap(bb);
		bbBarrier.oldLayout = bbBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	{
		VkImageViewCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			0, Unwrap(bb), VK_IMAGE_VIEW_TYPE_2D,
			imformat,
			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
		};

		vkr = vt->CreateImageView(Unwrap(device), &info, NULL, &bbview);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(device), bbview);

		{
			VkFramebufferCreateInfo fbinfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
				Unwrap(rp),
				1, UnwrapPtr(bbview),
				(uint32_t)width, (uint32_t)height, 1,
			};

			vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, NULL, &fb);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(device), fb);
		}

		if(dsimg != VK_NULL_HANDLE)
		{
			VkImageView views[] = { Unwrap(bbview), Unwrap(dsview) };
			VkFramebufferCreateInfo fbinfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
				Unwrap(rpdepth),
				2, views,
				(uint32_t)width, (uint32_t)height, 1,
			};

			vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, NULL, &fbdepth);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
	PreDeviceShutdownCounters();

	m_pDriver->Shutdown();
	delete m_pDriver;

	VulkanReplay::PostDeviceShutdownCounters();
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

void VulkanReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
	m_pDriver->ReplayLog(0, endEventID, replayType);
}

vector<uint32_t> VulkanReplay::GetPassEvents(uint32_t eventID)
{
	vector<uint32_t> passEvents;
	
	const FetchDrawcall *draw = m_pDriver->GetDrawcall(eventID);

	if(!draw)
		return passEvents;

	// for vulkan a pass == a renderpass, if we're not inside a
	// renderpass then there are no pass events.
	const FetchDrawcall *start = draw;
	while(start)
	{
		// if we've come to the beginning of a pass, break out of the loop, we've
		// found the start.
		// Note that vkCmdNextSubPass has both Begin and End flags set, so it will
		// break out here before we hit the terminating case looking for eDraw_EndPass
		if(start->flags & eDraw_BeginPass)
			break;

		// if we come to the END of a pass, since we were iterating backwards that
		// means we started outside of a pass, so return empty set.
		// Note that vkCmdNextSubPass has both Begin and End flags set, so it will
		// break out above before we hit this terminating case
		if(start->flags & eDraw_EndPass)
			return passEvents;

		// if we've come to the start of the log we were outside of a render pass
		// to start with
		if(start->previous == 0)
			return passEvents;

		// step back
		start = m_pDriver->GetDrawcall((uint32_t)start->previous);

		// something went wrong, start->previous was non-zero but we didn't
		// get a draw. Abort
		if(!start)
			return passEvents;
	}

	// store all the draw eventIDs up to the one specified at the start
	while(start)
	{
		if(start == draw)
			break;

		// include pass boundaries, these will be filtered out later
		// so we don't actually do anything (init postvs/draw overlay)
		// but it's useful to have the first part of the pass as part
		// of the list
		if(start->flags & (eDraw_Drawcall|eDraw_PassBoundary))
			passEvents.push_back(start->eventID);

		start = m_pDriver->GetDrawcall((uint32_t)start->next);
	}

	return passEvents;
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

FetchFrameRecord VulkanReplay::GetFrameRecord()
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
	ret.arraysize = iminfo.arrayLayers;
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
			ret.resType = iminfo.arrayLayers > 1 ? eResType_Texture1DArray : eResType_Texture1D;
			ret.dimension = 1;
			break;
		case VK_IMAGE_TYPE_2D:
			     if(ret.msSamp > 1) ret.resType = iminfo.arrayLayers > 1 ? eResType_Texture2DMSArray : eResType_Texture2DMS;
			else if(ret.cubemap)    ret.resType = iminfo.arrayLayers > 6 ? eResType_TextureCubeArray : eResType_TextureCube;
			else                    ret.resType = iminfo.arrayLayers > 1 ? eResType_Texture2DArray : eResType_Texture2D;
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

ShaderReflection *VulkanReplay::GetShader(ResourceId shader, string entryPoint)
{
	auto shad = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);

	if(shad == m_pDriver->m_CreationInfo.m_ShaderModule.end())
	{
		RDCERR("Can't get shader details");
		return NULL;
	}

	// disassemble lazily on demand
	if(shad->second.m_Reflections[entryPoint].refl.Disassembly.count == 0)
		shad->second.m_Reflections[entryPoint].refl.Disassembly = shad->second.spirv.Disassemble(entryPoint);

	return &shad->second.m_Reflections[entryPoint].refl;
}

void VulkanReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4])
{
	int oldW = m_DebugWidth, oldH = m_DebugHeight;

	m_DebugWidth = m_DebugHeight = 1;
	
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texture];

	bool isStencil = IsStencilFormat(iminfo.format);

	// do a second pass to render the stencil, if needed
	for(int pass=0; pass < (isStencil ? 2 : 1); pass++)
	{
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

			// only render green (stencil) in second pass
			if(pass == 1)
			{
				texDisplay.Green = true;
				texDisplay.Red = texDisplay.Blue = texDisplay.Alpha = false;
			}

			VkClearValue clearval = {};
			VkRenderPassBeginInfo rpbegin = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
				Unwrap(GetDebugManager()->m_PickPixelRP), Unwrap(GetDebugManager()->m_PickPixelFB),
				{ { 0, 0, }, { 1, 1 } },
				1, &clearval,
			};

			RenderTextureInternal(texDisplay, rpbegin, true);
		}

		VkDevice dev = m_pDriver->GetDev();
		VkCommandBuffer cmd = m_pDriver->GetNextCmd();
		const VkLayerDispatchTable *vt = ObjDisp(dev);

		VkResult vkr = VK_SUCCESS;

		{
			VkImageMemoryBarrier pickimBarrier = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
				0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				Unwrap(GetDebugManager()->m_PickPixelImage),
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};

			// update image layout from color attachment to transfer source, with proper memory barriers
			pickimBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			pickimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

			vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			DoPipelineBarrier(cmd, 1, &pickimBarrier);
			pickimBarrier.oldLayout = pickimBarrier.newLayout;
			pickimBarrier.srcAccessMask = pickimBarrier.dstAccessMask;

			// do copy
			VkBufferImageCopy region = {
				0, 128, 1,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{ 0, 0, 0 },
				{ 1, 1, 1 },
			};
			vt->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_PickPixelImage), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.buf), 1, &region);

			// update image layout back to color attachment
			pickimBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			pickimBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			DoPipelineBarrier(cmd, 1, &pickimBarrier);

			vt->EndCommandBuffer(Unwrap(cmd));
		}

		// submit cmds and wait for idle so we can readback
		m_pDriver->SubmitCmds();
		m_pDriver->FlushQ();

		float *pData = NULL;
		vt->MapMemory(Unwrap(dev), Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.mem), 0, VK_WHOLE_SIZE, 0, (void **)&pData);

		RDCASSERT(pData != NULL);

		if(pData == NULL)
		{
			RDCERR("Failed ot map readback buffer memory");
		}
		else
		{
			// only write stencil to .y
			if(pass == 1)
			{
				pixel[1] = pData[1]/255.0f;
			}
			else
			{
				pixel[0] = pData[0];
				pixel[1] = pData[1];
				pixel[2] = pData[2];
				pixel[3] = pData[3];
			}
		}

		vt->UnmapMemory(Unwrap(dev), Unwrap(GetDebugManager()->m_PickPixelReadbackBuffer.mem));
	}

	m_DebugWidth = oldW;m_DebugHeight = oldH;
}

uint32_t VulkanReplay::PickVertex(uint32_t eventID, MeshDisplay cfg, uint32_t x, uint32_t y)
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
	
	VkClearValue clearval = {};
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
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[cfg.texid];
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[cfg.texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(cfg.texid);

	VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	
	int displayformat = 0;
	uint32_t descSetBinding = 0;

	if(IsUIntFormat(iminfo.format))
	{
		descSetBinding = 10;
		displayformat |= TEXDISPLAY_UINT_TEX;
	}
	else if(IsSIntFormat(iminfo.format))
	{
		descSetBinding = 15;
		displayformat |= TEXDISPLAY_SINT_TEX;
	}
	else
	{
		descSetBinding = 5;
	}

	if(IsDepthOnlyFormat(layouts.format))
	{
		aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else if(IsDepthStencilFormat(layouts.format))
	{
		aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		if(layouts.format == VK_FORMAT_S8_UINT || (!cfg.Red && cfg.Green))
		{
			aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
			descSetBinding = 10;
			displayformat |= TEXDISPLAY_UINT_TEX;

			// rescale the range so that stencil seems to fit to 0-1
			cfg.rangemin *= 255.0f;
			cfg.rangemax *= 255.0f;
		}
	}

	CreateTexImageView(aspectFlags, liveIm, iminfo);

	VkImageView liveImView = (aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT ? iminfo.stencilView : iminfo.view);

	RDCASSERT(liveImView != VK_NULL_HANDLE);

	uint32_t uboOffs = 0;
	
	TexDisplayUBOData *data = (TexDisplayUBOData *)GetDebugManager()->m_TexDisplayUBO.Map(vt, dev, &uboOffs);

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

	data->MipLevel = (int)cfg.mip;
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
	
	if(cfg.sampleIdx == ~0U)
		data->SampleIdx = -SampleCount(iminfo.samples);

	data->OutputRes.x = (float)m_DebugWidth;
	data->OutputRes.y = (float)m_DebugHeight;

	int textype = 0;
	
	if(iminfo.type == VK_IMAGE_TYPE_1D)
		textype = RESTYPE_TEX1D;
	if(iminfo.type == VK_IMAGE_TYPE_3D)
		textype = RESTYPE_TEX3D;
	if(iminfo.type == VK_IMAGE_TYPE_2D)
	{
		textype = RESTYPE_TEX2D;
		if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
			textype = RESTYPE_TEX2DMS;
	}

	displayformat |= textype;

	descSetBinding += textype;
	
	if(!IsSRGBFormat(iminfo.format) && cfg.linearDisplayAsGamma)
		displayformat |= TEXDISPLAY_GAMMA_CURVE;

	if(cfg.overlay == eTexOverlay_NaN)
		displayformat |= TEXDISPLAY_NANS;

	if(cfg.overlay == eTexOverlay_Clipping)
		displayformat |= TEXDISPLAY_CLIPPING;

	data->OutputDisplayFormat = displayformat;
	
	data->RawOutput = cfg.rawoutput ? 1 : 0;

	GetDebugManager()->m_TexDisplayUBO.Unmap(vt, dev);
	
	VkDescriptorImageInfo imdesc = {0};
	imdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imdesc.imageView = Unwrap(liveImView);
	imdesc.sampler = Unwrap(GetDebugManager()->m_PointSampler);
	if(cfg.mip == 0 && cfg.scale < 1.0f)
		imdesc.sampler = Unwrap(GetDebugManager()->m_LinearSampler);

	VkDescriptorSet descset = GetDebugManager()->GetTexDisplayDescSet();

	VkDescriptorBufferInfo ubodesc = {0};
	GetDebugManager()->m_TexDisplayUBO.FillDescriptor(ubodesc);

	VkWriteDescriptorSet writeSet[] = {
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(descset), descSetBinding, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			&imdesc, NULL, NULL
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(descset), 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			NULL, &ubodesc, NULL
		},
	};

	vector<VkWriteDescriptorSet> writeSets;
	for(size_t i=0; i < ARRAY_COUNT(writeSet); i++)
		writeSets.push_back(writeSet[i]);
	
	for(size_t i=0; i < ARRAY_COUNT(GetDebugManager()->m_TexDisplayDummyWrites); i++)
	{
		VkWriteDescriptorSet &write = GetDebugManager()->m_TexDisplayDummyWrites[i];

		// don't write dummy data in the actual slot
		if(write.dstBinding == descSetBinding)
			continue;

		write.dstSet = Unwrap(descset);
		writeSets.push_back(write);
	}

	vt->UpdateDescriptorSets(Unwrap(dev), (uint32_t)writeSets.size(), &writeSets[0], 0, NULL);

	VkImageMemoryBarrier srcimBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ 0, 0, 1, 0, 1 } // will be overwritten by subresourceRange
	};

	// ensure all previous writes have completed
	srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
	// before we go reading
	srcimBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
	{
		srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
		srcimBarrier.oldLayout = layouts.subresourceStates[si].newLayout;
		srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS | MakeAccessMask(srcimBarrier.oldLayout);
		DoPipelineBarrier(cmd, 1, &srcimBarrier);
	}

	srcimBarrier.oldLayout = srcimBarrier.newLayout;
	srcimBarrier.srcAccessMask = srcimBarrier.dstAccessMask;

	{
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);
		
		VkPipeline pipe = GetDebugManager()->m_TexDisplayPipeline;
		if(f32render)
			pipe = GetDebugManager()->m_TexDisplayF32Pipeline;
		else if(!cfg.rawoutput && cfg.CustomShader == ResourceId())
			pipe = GetDebugManager()->m_TexDisplayBlendPipeline;

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_TexDisplayPipeLayout), 0, 1, UnwrapPtr(descset), 1, &uboOffs);

		VkViewport viewport = {
			(float)rpbegin.renderArea.offset.x, (float)rpbegin.renderArea.offset.y,
			(float)m_DebugWidth, (float)m_DebugHeight,
			0.0f, 1.0f
		};
		vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

		vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
	{
		srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
		srcimBarrier.newLayout = layouts.subresourceStates[si].newLayout;
		srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);
		DoPipelineBarrier(cmd, 1, &srcimBarrier);
	}

	vt->EndCommandBuffer(Unwrap(cmd));

	return true;
}
	
void VulkanReplay::CreateTexImageView(VkImageAspectFlags aspectFlags, VkImage liveIm, VulkanCreationInfo::Image &iminfo)
{
	VkDevice dev = m_pDriver->GetDev();

	if(aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT)
	{
		if(iminfo.stencilView != VK_NULL_HANDLE)
			return;
	}
	else
	{
		if(iminfo.view != VK_NULL_HANDLE)
			return;
	}

	VkImageViewCreateInfo viewInfo = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
		0, Unwrap(liveIm), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		iminfo.format,
		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{ aspectFlags, 0, RDCMAX(1U, (uint32_t)iminfo.mipLevels), 0, RDCMAX(1U, (uint32_t)iminfo.arrayLayers), },
	};

	if(iminfo.type == VK_IMAGE_TYPE_1D)
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
	if(iminfo.type == VK_IMAGE_TYPE_3D)
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;

	if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT)
	{
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_ZERO;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_ZERO;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_ZERO;
	}
	else if(aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT)
	{
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_ZERO;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_ZERO;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_ZERO;
	}

	VkImageView view;

	VkResult vkr = ObjDisp(dev)->CreateImageView(Unwrap(dev), &viewInfo, NULL, &view);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	ResourceId viewid = m_pDriver->GetResourceManager()->WrapResource(Unwrap(dev), view);
	// register as a live-only resource, so it is cleaned up properly
	m_pDriver->GetResourceManager()->AddLiveResource(viewid, view);

	if(aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT)
		iminfo.stencilView = view;
	else
		iminfo.view = view;
}

void VulkanReplay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
		VkClearValue clearval = {};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			Unwrap(outw.rp), Unwrap(outw.fb),
			{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

		vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
			outw.dsimg == VK_NULL_HANDLE ? Unwrap(GetDebugManager()->m_CheckerboardPipeline) : Unwrap(GetDebugManager()->m_CheckerboardMSAAPipeline));
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_CheckerboardPipeLayout), 0, 1, UnwrapPtr(GetDebugManager()->m_CheckerboardDescSet), 1, &uboOffs);

		VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
		vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
		
		vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
}
	
void VulkanReplay::RenderHighlightBox(float w, float h, float scale)
{
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	{
		VkClearValue clearval = {};
		VkRenderPassBeginInfo rpbegin = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
			Unwrap(outw.rp), Unwrap(outw.fb),
			{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
			1, &clearval,
		};
		vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);
		
		VkClearAttachment black = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			{{{ 0.0f, 0.0f, 0.0f, 1.0f }}}
		};
		VkClearAttachment white = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			{{{ 1.0f, 1.0f, 1.0f, 1.0f }}}
		};

		uint32_t sz = uint32_t(scale);

		VkOffset2D tl = { int32_t(w/2.0f + 0.5f), int32_t(h/2.0f + 0.5f) };

		VkClearRect rect[4] = {
			{ { { tl.x            , tl.y    }, { 1, sz }, }, 0, 1 },
			{ { { tl.x+(int32_t)sz, tl.y    }, { 1, sz+1 }, }, 0, 1 },
			{ { { tl.x            , tl.y    }, { sz, 1 }, }, 0, 1 },
			{ { { tl.x            , tl.y+(int32_t)sz }, { sz, 1 }, }, 0, 1 },
		};

		// inner
		vt->CmdClearAttachments(Unwrap(cmd), 1, &white, 4, rect);

		rect[0].rect.offset.x--;
		rect[1].rect.offset.x++;
		rect[2].rect.offset.x--;
		rect[3].rect.offset.x--;

		rect[0].rect.offset.y--;
		rect[1].rect.offset.y--;
		rect[2].rect.offset.y--;
		rect[3].rect.offset.y++;

		rect[0].rect.extent.height += 2;
		rect[1].rect.extent.height += 2;
		rect[2].rect.extent.width += 2;
		rect[3].rect.extent.width += 2;

		// outer
		vt->CmdClearAttachments(Unwrap(cmd), 1, &black, 4, rect);

		vt->CmdEndRenderPass(Unwrap(cmd));
	}

	vkr = vt->EndCommandBuffer(Unwrap(cmd));
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

ResourceId VulkanReplay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	return GetDebugManager()->RenderOverlay(texid, overlay, eventID, passEvents);
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

	if(cfg.position.bgraOrder)
	{
		FloatVector reversed;
		reversed.x = ret.z;
		reversed.y = ret.y;
		reversed.z = ret.x;
		reversed.w = ret.w;
		return reversed;
	}

	return ret;
}

void VulkanReplay::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg)
{
	if(cfg.position.buf == ResourceId() || cfg.position.numVerts == 0)
		return;
	
	auto it = m_OutputWindows.find(m_ActiveWinID);
	if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkResult vkr = VK_SUCCESS;
	
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	VkClearValue clearval = {};
	VkRenderPassBeginInfo rpbegin = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		Unwrap(outw.rpdepth), Unwrap(outw.fbdepth),
		{ { 0, 0, }, { m_DebugWidth, m_DebugHeight } },
		1, &clearval,
	};
	vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);
	
	VkViewport viewport = { 0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f };
	vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
	
	Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(m_DebugWidth)/float(m_DebugHeight));
	Matrix4f InvProj = projMat.Inverse();

	Matrix4f camMat = cfg.cam ? cfg.cam->GetMatrix() : Matrix4f::Identity();

	Matrix4f ModelViewProj = projMat.Mul(camMat);
	Matrix4f guessProjInv;

	if(cfg.position.unproject)
	{
		// the derivation of the projection matrix might not be right (hell, it could be an
		// orthographic projection). But it'll be close enough likely.
		Matrix4f guessProj = cfg.position.farPlane != FLT_MAX
			? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
			: Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

		if(cfg.ortho)
		{
			guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
		}
		
		guessProjInv = guessProj.Inverse();

		ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
	}

	if(!secondaryDraws.empty())
	{
		uint32_t uboOffs = 0;
		MeshUBOData *data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);

		data->mvp = ModelViewProj;
		data->color = (Vec4f &)cfg.prevMeshColour;
		data->homogenousInput = cfg.position.unproject;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);
		data->displayFormat = MESHDISPLAY_SOLID;

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		for(size_t i=0; i < secondaryDraws.size(); i++)
		{
			const MeshFormat &fmt = secondaryDraws[i];

			if(fmt.buf != ResourceId())
			{
				MeshDisplayPipelines secondaryCache = GetDebugManager()->CacheMeshDisplayPipelines(secondaryDraws[i], secondaryDraws[i]);
				
				vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
					0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);

				vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(secondaryCache.pipes[MeshDisplayPipelines::ePipe_WireDepth]));
				
				VkBuffer vb = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.buf);

				VkDeviceSize offs = fmt.offset;
				vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);

				if(fmt.idxByteWidth)
				{
					VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
					if(fmt.idxByteWidth == 4)
						idxtype = VK_INDEX_TYPE_UINT32;

					if(fmt.idxbuf != ResourceId())
					{
						VkBuffer ib = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.idxbuf);

						vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), fmt.idxoffs, idxtype);
					}
					vt->CmdDrawIndexed(Unwrap(cmd), fmt.numVerts, 1, 0, fmt.baseVertex, 0);
				}
				else
				{
					vt->CmdDraw(Unwrap(cmd), fmt.numVerts, 1, 0, 0);
				}
			}
		}
	}

	MeshDisplayPipelines cache = GetDebugManager()->CacheMeshDisplayPipelines(cfg.position, cfg.second);
	
	if(cfg.position.buf != ResourceId())
	{
		VkBuffer vb = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.buf);

		VkDeviceSize offs = cfg.position.offset;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);
	}

	// can't support secondary shading without a buffer - no pipeline will have been created
	if(cfg.solidShadeMode == eShade_Secondary && cfg.second.buf == ResourceId())
		cfg.solidShadeMode = eShade_None;
	
	if(cfg.solidShadeMode == eShade_Secondary)
	{
		VkBuffer vb = m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.second.buf);

		VkDeviceSize offs = cfg.second.offset;
		vt->CmdBindVertexBuffers(Unwrap(cmd), 1, 1, UnwrapPtr(vb), &offs);
	}
	
	// solid render
	if(cfg.solidShadeMode != eShade_None && cfg.position.topo < eTopology_PatchList)
	{
		VkPipeline pipe = VK_NULL_HANDLE;
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
			case eShade_None:
			case eShade_Count:
				break;
		}
		
		uint32_t uboOffs = 0;
		MeshUBOData *data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);

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
			vt->CmdDrawIndexed(Unwrap(cmd), cfg.position.numVerts, 1, 0, cfg.position.baseVertex, 0);
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
		MeshUBOData *data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
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
			vt->CmdDrawIndexed(Unwrap(cmd), cfg.position.numVerts, 1, 0, cfg.position.baseVertex, 0);
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
		MeshUBOData *data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
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
		MeshUBOData *data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
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
		data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
		data->mvp = ModelViewProj;
		data->color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
		data->displayFormat = (uint32_t)eShade_Solid;
		data->homogenousInput = 0;
		data->pointSpriteSize = Vec2f(0.0f, 0.0f);

		GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
		
		vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
		                          0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
		vt->CmdDraw(Unwrap(cmd), 2, 1, 2, 0);
		
		data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
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
		MeshUBOData *data = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
		
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
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			uint64_t maxIndex = cfg.position.numVerts;

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
					GetBufferData(cfg.position.idxbuf, cfg.position.idxoffs, cfg.position.numVerts*bytesize, idxdata);

				uint8_t *idx8 = (uint8_t *)&idxdata[0];
				uint16_t *idx16 = (uint16_t *)&idxdata[0];
				uint32_t *idx32 = (uint32_t *)&idxdata[0];

				uint32_t numIndices = RDCMIN(cfg.position.numVerts, uint32_t(idxdata.size()/bytesize));

				m_HighlightCache.indices.resize(numIndices);

				if(bytesize == 1)
				{
					for(uint32_t i=0; i < numIndices; i++)
					{
						m_HighlightCache.indices[i] = uint32_t(idx8[i]);
						maxIndex = RDCMAX(maxIndex, (uint64_t)m_HighlightCache.indices[i]);
					}
				}
				else if(bytesize == 2)
				{
					for(uint32_t i=0; i < numIndices; i++)
					{
						m_HighlightCache.indices[i] = uint32_t(idx16[i]);
						maxIndex = RDCMAX(maxIndex, (uint64_t)m_HighlightCache.indices[i]);
					}
				}
				else if(bytesize == 4)
				{
					for(uint32_t i=0; i < numIndices; i++)
					{
						m_HighlightCache.indices[i] = idx32[i];
						maxIndex = RDCMAX(maxIndex, (uint64_t)m_HighlightCache.indices[i]);
					}
				}
				
				uint32_t sub = uint32_t(-cfg.position.baseVertex);
				uint32_t add = uint32_t(cfg.position.baseVertex);

				for(uint32_t i=0; cfg.position.baseVertex != 0 && i < numIndices; i++)
				{
					if(cfg.position.baseVertex < 0)
					{
						if(m_HighlightCache.indices[i] < sub)
							m_HighlightCache.indices[i] = 0;
						else
							m_HighlightCache.indices[i] -= sub;
					}
					else
						m_HighlightCache.indices[i] += add;
				}
			}
			
			GetBufferData(cfg.position.buf, cfg.position.offset, (maxIndex+1)*cfg.position.stride, m_HighlightCache.data);

			// get a new cmdbuffer and begin it
			cmd = m_pDriver->GetNextCmd();
	
			vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);
			vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);
			
			vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
		}

		PrimitiveTopology meshtopo = cfg.position.topo;

		uint32_t idx = cfg.highlightVert;

		byte *data = &m_HighlightCache.data[0]; // buffer start
		byte *dataEnd = data + m_HighlightCache.data.size();

		// we already accounted for cfg.position.offset when fetching the cache
		// above (since this is constant)
		//data += cfg.position.offset; // to start of position data
		
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

			MeshUBOData uniforms;
			uniforms.mvp = ModelViewProj;
			uniforms.color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
			uniforms.displayFormat = (uint32_t)eShade_Solid;
			uniforms.homogenousInput = cfg.position.unproject;
			uniforms.pointSpriteSize = Vec2f(0.0f, 0.0f);
			
			uint32_t uboOffs = 0;
			MeshUBOData *ubodata = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*ubodata = uniforms;
			GetDebugManager()->m_MeshUBO.Unmap(vt, dev);
			
			vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetDebugManager()->m_MeshPipeLayout),
																0, 1, UnwrapPtr(GetDebugManager()->m_MeshDescSet), 1, &uboOffs);
			
			vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Solid]));
			
			////////////////////////////////////////////////////////////////
			// render primitives
			
			// Draw active primitive (red)
			uniforms.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
			// poke the color (this would be a good candidate for a push constant)
			ubodata = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*ubodata = uniforms;
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
			ubodata = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*ubodata = uniforms;
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
			ubodata = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*ubodata = uniforms;
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
			ubodata = (MeshUBOData *)GetDebugManager()->m_MeshUBO.Map(vt, dev, &uboOffs);
			*ubodata = uniforms;
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
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
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

	if((uint32_t)w != outw.width || (uint32_t)h != outw.height)
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
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);
	
	// semaphore is short lived, so not wrapped, if it's cached (ideally)
	// then it should be wrapped
	VkSemaphore sem;
	VkPipelineStageFlags stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT };

	VkResult vkr = vt->CreateSemaphore(Unwrap(dev), &semInfo, NULL, &sem);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	vkr = vt->AcquireNextImageKHR(Unwrap(dev), Unwrap(outw.swap), UINT64_MAX, sem, VK_NULL_HANDLE, &outw.curidx);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		1, &sem, &stage,
		0, NULL, // cmd buffers
		0, NULL, // signal semaphores
	};

	vkr = vt->QueueSubmit(Unwrap(m_pDriver->GetQ()), 1, &submitInfo, VK_NULL_HANDLE);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	vt->QueueWaitIdle(Unwrap(m_pDriver->GetQ()));

	vt->DestroySemaphore(Unwrap(dev), sem, NULL);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	outw.depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	// first time rendering to the backbuffer, clear it, since our typical render pass
	// is set to LOAD_OP_LOAD
	if(outw.fresh)
	{
		outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		outw.bbBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		DoPipelineBarrier(cmd, 1, &outw.bbBarrier);
		float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		vt->CmdClearColorImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)black, 1, &outw.bbBarrier.subresourceRange);

		outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
		outw.bbBarrier.srcAccessMask = outw.bbBarrier.dstAccessMask;

		outw.fresh = false;
	}
	
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.bbBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	outw.colBarrier[outw.curidx].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	outw.colBarrier[outw.curidx].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	
	DoPipelineBarrier(cmd, 1, &outw.bbBarrier);
	DoPipelineBarrier(cmd, 1, &outw.colBarrier[outw.curidx]);
	if(depth)
		DoPipelineBarrier(cmd, 1, &outw.depthBarrier);

	outw.depthBarrier.oldLayout = outw.depthBarrier.newLayout;
	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
	outw.bbBarrier.srcAccessMask = outw.bbBarrier.dstAccessMask;
	outw.colBarrier[outw.curidx].oldLayout = outw.colBarrier[outw.curidx].newLayout;
	outw.colBarrier[outw.curidx].srcAccessMask = outw.colBarrier[outw.curidx].dstAccessMask;

	vt->EndCommandBuffer(Unwrap(cmd));
}

void VulkanReplay::ClearOutputWindowColour(uint64_t id, float col[4])
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	outw.bbBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	outw.bbBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.bbBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	
	DoPipelineBarrier(cmd, 1, &outw.bbBarrier);

	vt->CmdClearColorImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkClearColorValue *)col, 1, &outw.bbBarrier.subresourceRange);

	outw.bbBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	outw.bbBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	outw.bbBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	
	DoPipelineBarrier(cmd, 1, &outw.bbBarrier);
	
	outw.bbBarrier.srcAccessMask = outw.bbBarrier.dstAccessMask;
	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;

	vt->EndCommandBuffer(Unwrap(cmd));
}

void VulkanReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	VkClearDepthStencilValue ds = { depth, stencil };

	outw.depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	outw.depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	outw.depthBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	outw.depthBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	
	DoPipelineBarrier(cmd, 1, &outw.depthBarrier);

	vt->CmdClearDepthStencilImage(Unwrap(cmd), Unwrap(outw.dsimg), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ds, 1, &outw.depthBarrier.subresourceRange);
	
	outw.depthBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	outw.depthBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	outw.depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	outw.depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	DoPipelineBarrier(cmd, 1, &outw.depthBarrier);

	vt->EndCommandBuffer(Unwrap(cmd));
}

void VulkanReplay::FlipOutputWindow(uint64_t id)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	// ensure rendering has completed before copying
	outw.bbBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	outw.bbBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	DoPipelineBarrier(cmd, 1, &outw.bbBarrier);
	DoPipelineBarrier(cmd, 1, &outw.colBarrier[outw.curidx]);
	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
	outw.bbBarrier.srcAccessMask = 0;
	outw.bbBarrier.dstAccessMask = 0;

	VkImageCopy cpy = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ 0, 0, 0 },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ 0, 0, 0 },
		{ outw.width, outw.height, 1 },
	};

#if MSAA_MESH_VIEW
	VkImageResolve resolve = {
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ 0, 0, 0 },
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		{ 0, 0, 0 },
		{ outw.width, outw.height, 1 },
	};

	if(outw.dsimg != VK_NULL_HANDLE)
		vt->CmdResolveImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(outw.colimg[outw.curidx]), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolve);
	else
#endif
		vt->CmdCopyImage(Unwrap(cmd), Unwrap(outw.bb), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(outw.colimg[outw.curidx]), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
	
	outw.bbBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	outw.bbBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	outw.bbBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	outw.colBarrier[outw.curidx].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// make sure copy has completed before present
	outw.colBarrier[outw.curidx].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	outw.colBarrier[outw.curidx].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	
	DoPipelineBarrier(cmd, 1, &outw.bbBarrier);
	DoPipelineBarrier(cmd, 1, &outw.colBarrier[outw.curidx]);

	outw.bbBarrier.oldLayout = outw.bbBarrier.newLayout;
	outw.bbBarrier.srcAccessMask = outw.bbBarrier.dstAccessMask;
	outw.colBarrier[outw.curidx].oldLayout = outw.colBarrier[outw.curidx].newLayout;
	
	outw.colBarrier[outw.curidx].srcAccessMask = 0;
	outw.colBarrier[outw.curidx].dstAccessMask = 0;

	vt->EndCommandBuffer(Unwrap(cmd));
	
	// submit all the cmds we recorded
	m_pDriver->SubmitCmds();

	VkPresentInfoKHR presentInfo = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL,
		0, NULL, // wait semaphores
		1, UnwrapPtr(outw.swap),
		&outw.curidx,
		&vkr
	};

	VkResult retvkr = vt->QueuePresentKHR(Unwrap(m_pDriver->GetQ()), &presentInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
	RDCASSERTEQUAL(retvkr, VK_SUCCESS);

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

void VulkanReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData)
{
	GetDebugManager()->GetBufferData(buff, offset, len, retData);
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
		const VulkanRenderState &state = m_pDriver->m_RenderState;
		VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

		VulkanResourceManager *rm = m_pDriver->GetResourceManager();

		m_VulkanPipelineState = VulkanPipelineState();
		
		// General pipeline properties
		m_VulkanPipelineState.compute.obj = rm->GetOriginalID(state.compute.pipeline);
		m_VulkanPipelineState.graphics.obj = rm->GetOriginalID(state.graphics.pipeline);

		if(state.compute.pipeline != ResourceId())
		{
			const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.compute.pipeline];

			m_VulkanPipelineState.compute.flags = p.flags;

			VulkanPipelineState::ShaderStage &stage = m_VulkanPipelineState.CS;

			int i=5; // 5 is the CS idx (VS, TCS, TES, GS, FS, CS)
			{
				stage.Shader = rm->GetOriginalID(p.shaders[i].module);
				stage.entryPoint = p.shaders[i].entryPoint;
				stage.ShaderDetails = NULL;

				stage.customName = true;
				stage.ShaderName = m_pDriver->m_CreationInfo.m_Names[p.shaders[i].module];
				if(stage.ShaderName.count == 0)
				{
					stage.customName = false;
					stage.ShaderName = StringFormat::Fmt("Shader %llu", stage.Shader);
				}

				stage.stage = eShaderStage_Compute;
				if(p.shaders[i].mapping)
					stage.BindpointMapping = *p.shaders[i].mapping;

				create_array_uninit(stage.specialization, p.shaders[i].specialization.size());
				for(size_t s=0; s < p.shaders[i].specialization.size(); s++)
				{
					stage.specialization[s].specID = p.shaders[i].specialization[s].specID;
					create_array_init(stage.specialization[s].data, p.shaders[i].specialization[s].size, p.shaders[i].specialization[s].data);
				}
			}
		}

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
			};

			for(size_t i=0; i < ARRAY_COUNT(stages); i++)
			{
				stages[i]->Shader = rm->GetOriginalID(p.shaders[i].module);
				stages[i]->entryPoint = p.shaders[i].entryPoint;
				stages[i]->ShaderDetails = NULL;

				stages[i]->customName = true;
				stages[i]->ShaderName = m_pDriver->m_CreationInfo.m_Names[p.shaders[i].module];
				if(stages[i]->ShaderName.count == 0)
				{
					stages[i]->customName = false;
					stages[i]->ShaderName = StringFormat::Fmt("Shader %llu", stages[i]->Shader);
				}

				stages[i]->stage = ShaderStageType(eShaderStage_Vertex + i);
				if(p.shaders[i].mapping)
					stages[i]->BindpointMapping = *p.shaders[i].mapping;

				create_array_uninit(stages[i]->specialization, p.shaders[i].specialization.size());
				for(size_t s=0; s < p.shaders[i].specialization.size(); s++)
				{
					stages[i]->specialization[s].specID = p.shaders[i].specialization[s].specID;
					create_array_init(stages[i]->specialization[s].data, p.shaders[i].specialization[s].size, p.shaders[i].specialization[s].data);
				}
			}

			// Tessellation
			m_VulkanPipelineState.Tess.numControlPoints = p.patchControlPoints;

			// Viewport/Scissors
			size_t numViewScissors = p.viewportCount;
			create_array_uninit(m_VulkanPipelineState.VP.viewportScissors, numViewScissors);
			for(size_t i=0; i < numViewScissors; i++)
			{
				if (i < state.views.size())
				{
					m_VulkanPipelineState.VP.viewportScissors[i].vp.x = state.views[i].x;
					m_VulkanPipelineState.VP.viewportScissors[i].vp.y = state.views[i].y;
					m_VulkanPipelineState.VP.viewportScissors[i].vp.width = state.views[i].width;
					m_VulkanPipelineState.VP.viewportScissors[i].vp.height = state.views[i].height;
					m_VulkanPipelineState.VP.viewportScissors[i].vp.minDepth = state.views[i].minDepth;
					m_VulkanPipelineState.VP.viewportScissors[i].vp.maxDepth = state.views[i].maxDepth;
				}
				else
				{
					RDCEraseEl(m_VulkanPipelineState.VP.viewportScissors[i].vp);
				}

				if (i < state.scissors.size())
				{
					m_VulkanPipelineState.VP.viewportScissors[i].scissor.x = state.scissors[i].offset.x;
					m_VulkanPipelineState.VP.viewportScissors[i].scissor.y = state.scissors[i].offset.y;
					m_VulkanPipelineState.VP.viewportScissors[i].scissor.width = state.scissors[i].extent.width;
					m_VulkanPipelineState.VP.viewportScissors[i].scissor.height = state.scissors[i].extent.height;
				}
				else
				{
					RDCEraseEl(m_VulkanPipelineState.VP.viewportScissors[i].scissor);
				}
			}

			// Rasterizer
			m_VulkanPipelineState.RS.depthClampEnable = p.depthClampEnable;
			m_VulkanPipelineState.RS.rasterizerDiscardEnable = p.rasterizerDiscardEnable;
			m_VulkanPipelineState.RS.FrontCCW = p.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE;

			switch(p.polygonMode)
			{
				case VK_POLYGON_MODE_POINT:  m_VulkanPipelineState.RS.FillMode = eFill_Point;     break;
				case VK_POLYGON_MODE_LINE:   m_VulkanPipelineState.RS.FillMode = eFill_Wireframe; break;
				case VK_POLYGON_MODE_FILL:   m_VulkanPipelineState.RS.FillMode = eFill_Solid;     break;
				default:
					m_VulkanPipelineState.RS.FillMode = eFill_Solid;
					RDCERR("Unexpected value for FillMode %x", p.polygonMode);
					break;
			}

			switch(p.cullMode)
			{
				case VK_CULL_MODE_NONE:           m_VulkanPipelineState.RS.CullMode = eCull_None;         break;
				case VK_CULL_MODE_FRONT_BIT:      m_VulkanPipelineState.RS.CullMode = eCull_Front;        break;
				case VK_CULL_MODE_BACK_BIT:       m_VulkanPipelineState.RS.CullMode = eCull_Back;         break;
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
			m_VulkanPipelineState.MSAA.rasterSamples = p.rasterizationSamples;
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

			m_VulkanPipelineState.DS.front.passOp = ToStr::Get(p.front.passOp);
			m_VulkanPipelineState.DS.front.failOp = ToStr::Get(p.front.failOp);
			m_VulkanPipelineState.DS.front.depthFailOp = ToStr::Get(p.front.depthFailOp);
			m_VulkanPipelineState.DS.front.func = ToStr::Get(p.front.compareOp);

			m_VulkanPipelineState.DS.back.passOp = ToStr::Get(p.back.passOp);
			m_VulkanPipelineState.DS.back.failOp = ToStr::Get(p.back.failOp);
			m_VulkanPipelineState.DS.back.depthFailOp = ToStr::Get(p.back.depthFailOp);
			m_VulkanPipelineState.DS.back.func = ToStr::Get(p.back.compareOp);

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
			if(state.renderPass != ResourceId())
			{
				m_VulkanPipelineState.Pass.renderpass.inputAttachments = c.m_RenderPass[state.renderPass].subpasses[state.subpass].inputAttachments;
				m_VulkanPipelineState.Pass.renderpass.colorAttachments = c.m_RenderPass[state.renderPass].subpasses[state.subpass].colorAttachments;
				m_VulkanPipelineState.Pass.renderpass.depthstencilAttachment = c.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;
			}

			m_VulkanPipelineState.Pass.framebuffer.obj = rm->GetOriginalID(state.framebuffer);
			
			if(state.framebuffer != ResourceId())
			{
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
			}
			else
			{
				m_VulkanPipelineState.Pass.framebuffer.width = 0;
				m_VulkanPipelineState.Pass.framebuffer.height = 0;
				m_VulkanPipelineState.Pass.framebuffer.layers = 0;
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
			
			const vector<VulkanRenderState::Pipeline::DescriptorAndOffsets> *srcs[] = {
				&state.graphics.descSets,
				&state.compute.descSets,
			};
			
			for(size_t p=0; p < ARRAY_COUNT(srcs); p++)
			{
				for(size_t i=0; i < srcs[p]->size(); i++)
				{
					ResourceId src = (*srcs[p])[i].descSet;
					VulkanPipelineState::Pipeline::DescriptorSet &dst = (*dsts[p])[i];

					ResourceId layoutId = m_pDriver->m_DescriptorSetState[src].layout;

					dst.descset = rm->GetOriginalID(src);
					dst.layout = rm->GetOriginalID(layoutId);
					create_array_uninit(dst.bindings, m_pDriver->m_DescriptorSetState[src].currentBindings.size());
					for(size_t b=0; b < m_pDriver->m_DescriptorSetState[src].currentBindings.size(); b++)
					{
						DescriptorSetSlot *info = m_pDriver->m_DescriptorSetState[src].currentBindings[b];
						const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[b];

						bool dynamicOffset = false;

						dst.bindings[b].descriptorCount = layoutBind.descriptorCount;
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
						
						create_array_uninit(dst.bindings[b].binds, layoutBind.descriptorCount);
						for(uint32_t a=0; a < layoutBind.descriptorCount; a++)
						{
							if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
							   layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
							{
								if(layoutBind.immutableSampler)
									dst.bindings[b].binds[a].sampler = layoutBind.immutableSampler[a];
								else if(info[a].imageInfo.sampler != VK_NULL_HANDLE)
									dst.bindings[b].binds[a].sampler = rm->GetNonDispWrapper(info[a].imageInfo.sampler)->id;

								if(dst.bindings[b].binds[a].sampler != ResourceId())
								{
									VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement &el = dst.bindings[b].binds[a];
									const VulkanCreationInfo::Sampler &sampl = c.m_Sampler[el.sampler];

									el.sampler = rm->GetOriginalID(el.sampler);

									// sampler info
									el.mag = ToStr::Get(sampl.magFilter);
									el.min = ToStr::Get(sampl.minFilter);
									el.mip = ToStr::Get(sampl.mipmapMode);
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
									if(sampl.address[0] == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ||
										sampl.address[1] == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ||
										sampl.address[2] == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
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
								VkImageView view = info[a].imageInfo.imageView;

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
								VkBufferView view = info[a].texelBufferView;
								
								if(view != VK_NULL_HANDLE)
								{
									ResourceId viewid = rm->GetNonDispWrapper(view)->id;

									dst.bindings[b].binds[a].view = rm->GetOriginalID(viewid);
									dst.bindings[b].binds[a].res = rm->GetOriginalID(c.m_BufferView[viewid].buffer);
									dst.bindings[b].binds[a].offset = c.m_BufferView[viewid].offset;
									if(dynamicOffset)
										dst.bindings[b].binds[a].offset += *(uint32_t *)&info[a].imageInfo.imageLayout;
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
									dst.bindings[b].binds[a].offset += *(uint32_t *)&info[a].imageInfo.imageLayout;

								dst.bindings[b].binds[a].size = info[a].bufferInfo.range;
							}
						}
					}
				}
			}
		}
	}
}

void VulkanReplay::FillCBufferVariables(rdctype::array<ShaderConstant> invars, vector<ShaderVariable> &outvars, const vector<byte> &data, size_t baseOffset)
{
	for(int v=0; v < invars.count; v++)
	{
		string basename = invars[v].name.elems;
		
		uint32_t rows = invars[v].type.descriptor.rows;
		uint32_t cols = invars[v].type.descriptor.cols;
		uint32_t elems = RDCMAX(1U,invars[v].type.descriptor.elements);
		bool rowMajor = invars[v].type.descriptor.rowMajorStorage != 0;
		bool isArray = elems > 1;

		size_t dataOffset = baseOffset + invars[v].reg.vec*sizeof(Vec4f) + invars[v].reg.comp*sizeof(float);

		if(invars[v].type.members.count > 0)
		{
			ShaderVariable var;
			var.name = basename;
			var.rows = var.columns = 0;
			var.type = eVar_Float;
			
			vector<ShaderVariable> varmembers;

			if(isArray)
			{
				for(uint32_t i=0; i < elems; i++)
				{
					ShaderVariable vr;
					vr.name = StringFormat::Fmt("%s[%u]", basename.c_str(), i);
					vr.rows = vr.columns = 0;
					vr.type = eVar_Float;

					vector<ShaderVariable> mems;

					FillCBufferVariables(invars[v].type.members, mems, data, dataOffset);

					dataOffset += invars[v].type.descriptor.arrayStride;

					vr.isStruct = true;

					vr.members = mems;

					varmembers.push_back(vr);
				}

				var.isStruct = false;
			}
			else
			{
				var.isStruct = true;

				FillCBufferVariables(invars[v].type.members, varmembers, data, dataOffset);
			}

			{
				var.members = varmembers;
				outvars.push_back(var);
			}

			continue;
		}

		size_t outIdx = outvars.size();
		outvars.resize(outvars.size()+1);

		{
			outvars[outIdx].name = basename;
			outvars[outIdx].rows = 1;
			outvars[outIdx].type = invars[v].type.descriptor.type;
			outvars[outIdx].isStruct = false;
			outvars[outIdx].columns = cols;

			size_t elemByteSize = 4;
			if(outvars[outIdx].type == eVar_Double)
				elemByteSize = 8;

			ShaderVariable &var = outvars[outIdx];

			if(!isArray)
			{
				outvars[outIdx].rows = rows;

				if(dataOffset < data.size())
				{
					const byte *d = &data[dataOffset];

					RDCASSERT(rows <= 4 && rows*cols <= 16, rows, cols);

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
				var.name = outvars[outIdx].name;
				var.rows = 0;
				var.columns = 0;

				bool isMatrix = rows > 1 && cols > 1;

				vector<ShaderVariable> varmembers;
				varmembers.resize(elems);
				
				string base = outvars[outIdx].name.elems;

				// primary is the 'major' direction
				// so we copy secondaryDim number of primaryDim-sized elements
				uint32_t primaryDim = cols;
				uint32_t secondaryDim = rows;
				if(isMatrix && rowMajor)
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
					
					size_t rowDataOffset = dataOffset;

					dataOffset += invars[v].type.descriptor.arrayStride;

					if(rowDataOffset < data.size())
					{
						const byte *d = &data[rowDataOffset];

						// each primary element (row or column) is stored in a float4.
						// we copy some padding here, but that will come out in the wash
						// when we transpose
						for(uint32_t s=0; s < secondaryDim; s++)
						{
							uint32_t matStride = primaryDim;
							if(matStride == 3) matStride = 4;
							memcpy(&(varmembers[e].value.uv[primaryDim*s]), d + matStride*elemByteSize*s,
								RDCMIN(data.size() - rowDataOffset, elemByteSize*primaryDim));
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

void VulkanReplay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data)
{
	// Correct SPIR-V will ultimately need to set explicit layout information for each type.
	// For now, just assume D3D11 packing (float4 alignment on float4s, float3s, matrices, arrays and structures)
	
	auto it = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);

	if(it == m_pDriver->m_CreationInfo.m_ShaderModule.end())
	{
		RDCERR("Can't get shader details");
		return;
	}

	ShaderReflection &refl = it->second.m_Reflections[entryPoint].refl;

	if(cbufSlot >= (uint32_t)refl.ConstantBlocks.count)
	{
		RDCERR("Invalid cbuffer slot");
		return;
	}

	ConstantBlock &c = refl.ConstantBlocks[cbufSlot];

	if(c.bufferBacked)
	{
		FillCBufferVariables(c.variables, outvars, data, 0);
	}
	else
	{
		vector<byte> pushdata;
		pushdata.resize(sizeof(m_pDriver->m_RenderState.pushconsts));
		memcpy(&pushdata[0], m_pDriver->m_RenderState.pushconsts, pushdata.size());
		FillCBufferVariables(c.variables, outvars, pushdata, 0);
	}

}

bool VulkanReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[texid];
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(texid);
	
	VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	if(IsDepthStencilFormat(layouts.format))
		aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

	CreateTexImageView(aspectFlags, liveIm, iminfo);

	VkImageView liveImView = iminfo.view;

	RDCASSERT(liveImView != VK_NULL_HANDLE);

	VkDescriptorImageInfo imdesc = {0};
	imdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imdesc.imageView = Unwrap(liveImView);
	imdesc.sampler = Unwrap(GetDebugManager()->m_PointSampler);
	
	uint32_t descSetBinding = 0;
	uint32_t intTypeIndex = 0;

	if(IsUIntFormat(iminfo.format))
	{
		descSetBinding = 10;
		intTypeIndex = 1;
	}
	else if(IsSIntFormat(iminfo.format))
	{
		descSetBinding = 15;
		intTypeIndex = 2;
	}
	else
	{
		descSetBinding = 5;
	}

	int textype = 0;
	
	if(iminfo.type == VK_IMAGE_TYPE_1D)
		textype = RESTYPE_TEX1D;
	if(iminfo.type == VK_IMAGE_TYPE_3D)
		textype = RESTYPE_TEX3D;
	if(iminfo.type == VK_IMAGE_TYPE_2D)
	{
		textype = RESTYPE_TEX2D;
		if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
			textype = RESTYPE_TEX2DMS;
	}

	descSetBinding += textype;

	VkDescriptorBufferInfo bufdescs[3];
	RDCEraseEl(bufdescs);
	GetDebugManager()->m_MinMaxTileResult.FillDescriptor(bufdescs[0]);
	GetDebugManager()->m_MinMaxResult.FillDescriptor(bufdescs[1]);
	GetDebugManager()->m_HistogramUBO.FillDescriptor(bufdescs[2]);

	VkWriteDescriptorSet writeSet[] = {

		// first pass on tiles
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL, &bufdescs[0], NULL // destination = tile result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL, &bufdescs[0], NULL // source = unused, bind tile result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			NULL, &bufdescs[2], NULL
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			descSetBinding, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			&imdesc, NULL, NULL
		},

		// second pass from tiles to result
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[1]),
			0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL, &bufdescs[1], NULL // destination = result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[1]),
			1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL, &bufdescs[0], NULL // source = tile result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[1]),
			2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			NULL, &bufdescs[2], NULL
		},
	};

	vt->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);
	
	HistogramUBOData *data = (HistogramUBOData *)GetDebugManager()->m_HistogramUBO.Map(vt, dev, NULL);
	
	data->HistogramTextureResolution.x = (float)RDCMAX(uint32_t(iminfo.extent.width)>>mip, 1U);
	data->HistogramTextureResolution.y = (float)RDCMAX(uint32_t(iminfo.extent.height)>>mip, 1U);
	data->HistogramTextureResolution.z = (float)RDCMAX(uint32_t(iminfo.arrayLayers)>>mip, 1U);
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
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ 0, 0, 1, 0, 1 } // will be overwritten by subresourceRange below
	};

	// ensure all previous writes have completed
	srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
	// before we go reading
	srcimBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
	{
		srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
		srcimBarrier.oldLayout = layouts.subresourceStates[si].newLayout;
		DoPipelineBarrier(cmd, 1, &srcimBarrier);
	}

	srcimBarrier.oldLayout = srcimBarrier.newLayout;

	srcimBarrier.srcAccessMask = 0;
	srcimBarrier.dstAccessMask = 0;
	
	int blocksX = (int)ceil(iminfo.extent.width/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(iminfo.extent.height/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

	vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_MinMaxTilePipe[textype][intTypeIndex]));
	vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipeLayout),
														0, 1, UnwrapPtr(GetDebugManager()->m_HistogramDescSet[0]), 0, NULL);

	vt->CmdDispatch(Unwrap(cmd), blocksX, blocksY, 1);

	// image layout back to normal
	for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
	{
		srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
		srcimBarrier.newLayout = layouts.subresourceStates[si].newLayout;
		srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);
		DoPipelineBarrier(cmd, 1, &srcimBarrier);
	}

	VkBufferMemoryBarrier tilebarrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, NULL,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(GetDebugManager()->m_MinMaxTileResult.buf),
		0, GetDebugManager()->m_MinMaxTileResult.totalsize,
	};

	// ensure shader writes complete before coalescing the tiles
	DoPipelineBarrier(cmd, 1, &tilebarrier);

	vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_MinMaxResultPipe[intTypeIndex]));
	vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipeLayout),
														0, 1, UnwrapPtr(GetDebugManager()->m_HistogramDescSet[1]), 0, NULL);

	vt->CmdDispatch(Unwrap(cmd), 1, 1, 1);

	// ensure shader writes complete before copying back to readback buffer
	tilebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	tilebarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	tilebarrier.buffer = Unwrap(GetDebugManager()->m_MinMaxResult.buf);
	tilebarrier.size = GetDebugManager()->m_MinMaxResult.totalsize;
	
	DoPipelineBarrier(cmd, 1, &tilebarrier);

	VkBufferCopy bufcopy = {
		0, 0, GetDebugManager()->m_MinMaxResult.totalsize,
	};

	vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_MinMaxResult.buf), Unwrap(GetDebugManager()->m_MinMaxReadback.buf), 1, &bufcopy);
	
	// wait for copy to complete before mapping
	tilebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	tilebarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	tilebarrier.buffer = Unwrap(GetDebugManager()->m_MinMaxReadback.buf);
	tilebarrier.size = GetDebugManager()->m_MinMaxResult.totalsize;
	
	DoPipelineBarrier(cmd, 1, &tilebarrier);

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
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[texid];
	VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
	VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(texid);
	
	VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	if(IsDepthStencilFormat(layouts.format))
		aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	
	CreateTexImageView(aspectFlags, liveIm, iminfo);
	
	uint32_t descSetBinding = 0;
	uint32_t intTypeIndex = 0;

	if(IsUIntFormat(iminfo.format))
	{
		descSetBinding = 10;
		intTypeIndex = 1;
	}
	else if(IsSIntFormat(iminfo.format))
	{
		descSetBinding = 15;
		intTypeIndex = 2;
	}
	else
	{
		descSetBinding = 5;
	}

	int textype = 0;
	
	if(iminfo.type == VK_IMAGE_TYPE_1D)
		textype = RESTYPE_TEX1D;
	if(iminfo.type == VK_IMAGE_TYPE_3D)
		textype = RESTYPE_TEX3D;
	if(iminfo.type == VK_IMAGE_TYPE_2D)
	{
		textype = RESTYPE_TEX2D;
		if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
			textype = RESTYPE_TEX2DMS;
	}

	descSetBinding += textype;

	VkImageView liveImView = (aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT ? iminfo.stencilView : iminfo.view);

	RDCASSERT(liveImView != VK_NULL_HANDLE);

	VkDescriptorImageInfo imdesc = {0};
	imdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imdesc.imageView = Unwrap(liveImView);
	imdesc.sampler = Unwrap(GetDebugManager()->m_PointSampler);

	VkDescriptorBufferInfo bufdescs[2];
	RDCEraseEl(bufdescs);
	GetDebugManager()->m_HistogramBuf.FillDescriptor(bufdescs[0]);
	GetDebugManager()->m_HistogramUBO.FillDescriptor(bufdescs[1]);

	VkWriteDescriptorSet writeSet[] = {

		// histogram pass
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL, &bufdescs[0], NULL // destination = histogram result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL, &bufdescs[0], NULL // source = unused, bind histogram result
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			NULL, &bufdescs[1], NULL
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			Unwrap(GetDebugManager()->m_HistogramDescSet[0]),
			descSetBinding, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
			&imdesc, NULL, NULL
		},
	};

	vt->UpdateDescriptorSets(Unwrap(dev), ARRAY_COUNT(writeSet), writeSet, 0, NULL);
	
	HistogramUBOData *data = (HistogramUBOData *)GetDebugManager()->m_HistogramUBO.Map(vt, dev, NULL);
	
	data->HistogramTextureResolution.x = (float)RDCMAX(uint32_t(iminfo.extent.width)>>mip, 1U);
	data->HistogramTextureResolution.y = (float)RDCMAX(uint32_t(iminfo.extent.height)>>mip, 1U);
	data->HistogramTextureResolution.z = (float)RDCMAX(uint32_t(iminfo.arrayLayers)>>mip, 1U);
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
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(liveIm),
		{ 0, 0, 1, 0, 1 } // will be overwritten by subresourceRange below
	};

	// ensure all previous writes have completed
	srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
	// before we go reading
	srcimBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

	for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
	{
		srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
		srcimBarrier.oldLayout = layouts.subresourceStates[si].newLayout;
		DoPipelineBarrier(cmd, 1, &srcimBarrier);
	}

	srcimBarrier.oldLayout = srcimBarrier.newLayout;

	srcimBarrier.srcAccessMask = 0;
	srcimBarrier.dstAccessMask = 0;
	
	int blocksX = (int)ceil(iminfo.extent.width/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(iminfo.extent.height/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

	vt->CmdFillBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_HistogramBuf.buf), 0, GetDebugManager()->m_HistogramBuf.totalsize, 0);

	vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipe[textype][intTypeIndex]));
	vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetDebugManager()->m_HistogramPipeLayout),
														0, 1, UnwrapPtr(GetDebugManager()->m_HistogramDescSet[0]), 0, NULL);

	vt->CmdDispatch(Unwrap(cmd), blocksX, blocksY, 1);

	// image layout back to normal
	for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
	{
		srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
		srcimBarrier.newLayout = layouts.subresourceStates[si].newLayout;
		srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);
		DoPipelineBarrier(cmd, 1, &srcimBarrier);
	}

	VkBufferMemoryBarrier tilebarrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, NULL,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		Unwrap(GetDebugManager()->m_HistogramBuf.buf),
		0, GetDebugManager()->m_HistogramBuf.totalsize,
	};

	// ensure shader writes complete before copying to readback buf
	DoPipelineBarrier(cmd, 1, &tilebarrier);

	VkBufferCopy bufcopy = {
		0, 0, GetDebugManager()->m_HistogramBuf.totalsize,
	};

	vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(GetDebugManager()->m_HistogramBuf.buf), Unwrap(GetDebugManager()->m_HistogramReadback.buf), 1, &bufcopy);
	
	// wait for copy to complete before mapping
	tilebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	tilebarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	tilebarrier.buffer = Unwrap(GetDebugManager()->m_HistogramReadback.buf);
	tilebarrier.size = GetDebugManager()->m_HistogramReadback.totalsize;
	
	DoPipelineBarrier(cmd, 1, &tilebarrier);

	vt->EndCommandBuffer(Unwrap(cmd));
	
	// submit cmds and wait for idle so we can readback
	m_pDriver->SubmitCmds();
	m_pDriver->FlushQ();

	uint32_t *buckets = (uint32_t *)GetDebugManager()->m_HistogramReadback.Map(vt, dev, NULL);

	histogram.resize(HGRAM_NUM_BUCKETS);
	for(size_t i=0; i < HGRAM_NUM_BUCKETS; i++)
		histogram[i] = buckets[i*4];

	GetDebugManager()->m_HistogramReadback.Unmap(vt, dev);

	return true;
}

void VulkanReplay::InitPostVSBuffers(uint32_t eventID)
{
	GetDebugManager()->InitPostVSBuffers(eventID);
}

struct InitPostVSCallback : public DrawcallCallback
{
	InitPostVSCallback(WrappedVulkan *vk, const vector<uint32_t> &events)
		: m_pDriver(vk)
	  , m_Events(events)
	{ m_pDriver->SetDrawcallCB(this); }
	~InitPostVSCallback()
	{ m_pDriver->SetDrawcallCB(NULL); }

	void PreDraw(uint32_t eid, VkCommandBuffer cmd)
	{
		if(std::find(m_Events.begin(), m_Events.end(), eid) != m_Events.end())
			m_pDriver->GetDebugManager()->InitPostVSBuffers(eid);
	}

	bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
	{
		return false;
	}

	void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
	{
	}
	
	// Dispatches don't rasterize, so do nothing
	void PreDispatch(uint32_t eid, VkCommandBuffer cmd) { }
	bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
	void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) { }

	bool RecordAllCmds()
	{
		return false;
	}
	
	void AliasEvent(uint32_t primary, uint32_t alias)
	{
		if(std::find(m_Events.begin(), m_Events.end(), primary) != m_Events.end())
			m_pDriver->GetDebugManager()->AliasPostVSBuffers(primary, alias);
	}

	WrappedVulkan *m_pDriver;
	const vector<uint32_t> &m_Events;
};

void VulkanReplay::InitPostVSBuffers(const vector<uint32_t> &events)
{
	// first we must replay up to the first event without replaying it. This ensures any
	// non-command buffer calls like memory unmaps etc all happen correctly before this
	// command buffer
	m_pDriver->ReplayLog(0, events.front(), eReplay_WithoutDraw);

	InitPostVSCallback cb(m_pDriver, events);

	// now we replay the events, which are guaranteed (because we generated them in
	// GetPassEvents above) to come from the same command buffer, so the event IDs are
	// still locally continuous, even if we jump into replaying.
	m_pDriver->ReplayLog(events.front(), events.back(), eReplay_Full);
}

vector<EventUsage> VulkanReplay::GetUsage(ResourceId id)
{
	return m_pDriver->GetUsage(id);
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

MeshFormat VulkanReplay::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
	return GetDebugManager()->GetPostVSBuffers(eventID, instID, stage);
}

byte *VulkanReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize)
{
	bool wasms = false;

	VulkanCreationInfo::Image &imInfo = m_pDriver->m_CreationInfo.m_Image[tex];
	
	ImageLayouts &layouts = m_pDriver->m_ImageLayouts[tex];

	VkImageCreateInfo imCreateInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			imInfo.type, imInfo.format, imInfo.extent,
			(uint32_t)imInfo.mipLevels, (uint32_t)imInfo.arrayLayers, imInfo.samples,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
	};
	
	bool isDepth = (layouts.subresourceStates[0].subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
	VkImageAspectFlags aspectMask = layouts.subresourceStates[0].subresourceRange.aspectMask;

	VkImage srcImage = Unwrap(GetResourceManager()->GetCurrentHandle<VkImage>(tex));
	VkImage tmpImage = VK_NULL_HANDLE;
	VkDeviceMemory tmpMemory = VK_NULL_HANDLE;

	VkFramebuffer *tmpFB = NULL;
	VkImageView *tmpView = NULL;
	uint32_t numFBs = 0;
	VkRenderPass tmpRP = VK_NULL_HANDLE;
	
	VkDevice dev = m_pDriver->GetDev();
	VkCommandBuffer cmd = m_pDriver->GetNextCmd();
	const VkLayerDispatchTable *vt = ObjDisp(dev);
	
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	if(imInfo.samples > 1)
	{
		// make image n-array instead of n-samples
		imCreateInfo.arrayLayers *= imCreateInfo.samples;
		imCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		wasms = true;
	}
	
	if(forceRGBA8unorm)
	{
		// force readback texture to RGBA8 unorm
		imCreateInfo.format = IsSRGBFormat(imCreateInfo.format) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
		// force to 1 array slice, 1 mip
		imCreateInfo.arrayLayers = 1;
		imCreateInfo.mipLevels = 1;
		// force to 2D
		imCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		imCreateInfo.extent.width = RDCMAX(1U, imCreateInfo.extent.width>>mip);
		imCreateInfo.extent.height = RDCMAX(1U, imCreateInfo.extent.height>>mip);
		imCreateInfo.extent.depth = RDCMAX(1U, imCreateInfo.extent.depth>>mip);

		// create render texture similar to readback texture
		vt->CreateImage(Unwrap(dev), &imCreateInfo, NULL, &tmpImage);
		
		VkMemoryRequirements mrq = {0};
		vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

		VkMemoryAllocateInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
			mrq.size, m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);
		
		VkImageMemoryBarrier dstimBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			0, 0, // MULTIDEVICE - need to actually pick the right queue family here maybe?
			tmpImage,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
		};
		
		// move tmp image into transfer destination layout
		DoPipelineBarrier(cmd, 1, &dstimBarrier);

		// end this command buffer, the rendertexture below will use its own and we want to ensure ordering
		vt->EndCommandBuffer(Unwrap(cmd));

		// create framebuffer/render pass to render to
		VkAttachmentDescription attDesc = {
			0, imCreateInfo.format, VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription sub = {
			0, VK_PIPELINE_BIND_POINT_GRAPHICS,
			0, NULL, // inputs
			1, &attRef, // color
			NULL, // resolve
			NULL, // depth-stencil
			0, NULL, // preserve
		};

		VkRenderPassCreateInfo rpinfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0,
				1, &attDesc,
				1, &sub,
				0, NULL, // dependencies
		};
		vt->CreateRenderPass(Unwrap(dev), &rpinfo, NULL, &tmpRP);

		numFBs = (imCreateInfo.imageType == VK_IMAGE_TYPE_3D ? (imCreateInfo.extent.depth>>mip) : 1);
		tmpFB = new VkFramebuffer[numFBs];
		tmpView = new VkImageView[numFBs];

		int oldW = m_DebugWidth, oldH = m_DebugHeight;

		m_DebugWidth = imCreateInfo.extent.width;
		m_DebugHeight = imCreateInfo.extent.height;
		
		// if 3d texture, render each slice separately, otherwise render once
		for(uint32_t i=0; i < numFBs; i++)
		{
			TextureDisplay texDisplay;

			texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
			texDisplay.HDRMul = -1.0f;
			texDisplay.linearDisplayAsGamma = false;
			texDisplay.overlay = eTexOverlay_None;
			texDisplay.FlipY = false;
			texDisplay.mip = mip;
			texDisplay.sampleIdx = imCreateInfo.imageType == VK_IMAGE_TYPE_3D ? 0 : (resolve ? ~0U : arrayIdx);
			texDisplay.CustomShader = ResourceId();
			texDisplay.sliceFace = imCreateInfo.imageType == VK_IMAGE_TYPE_3D ? i : arrayIdx;
			texDisplay.rangemin = blackPoint;
			texDisplay.rangemax = whitePoint;
			texDisplay.scale = 1.0f;
			texDisplay.texid = tex;
			texDisplay.rawoutput = true;
			texDisplay.offx = 0;
			texDisplay.offy = 0;
			
			VkImageViewCreateInfo viewInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
				0, tmpImage, VK_IMAGE_VIEW_TYPE_2D,
				imCreateInfo.format,
				{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, i, 1, },
			};

			vt->CreateImageView(Unwrap(dev), &viewInfo, NULL, &tmpView[i]);
			
			VkFramebufferCreateInfo fbinfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
				0, tmpRP,
				1, &tmpView[i],
				(uint32_t)imCreateInfo.extent.width, (uint32_t)imCreateInfo.extent.height, 1,
			};

			vkr = vt->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &tmpFB[i]);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);
			
			VkClearValue clearval = {};
			VkRenderPassBeginInfo rpbegin = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
				tmpRP, tmpFB[i],
				{ { 0, 0, }, { imCreateInfo.extent.width, imCreateInfo.extent.height } },
				1, &clearval,
			};

			RenderTextureInternal(texDisplay, rpbegin, true);
		}
			
		m_DebugWidth = oldW; m_DebugHeight = oldH;

		srcImage = tmpImage;

		// fetch a new command buffer for copy & readback
		cmd = m_pDriver->GetNextCmd();

		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		// ensure all writes happen before copy & readback
		dstimBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		dstimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		dstimBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		
		DoPipelineBarrier(cmd, 1, &dstimBarrier);

		// these have already been selected, don't need to fetch that subresource
		// when copying back to readback buffer
		arrayIdx = 0;
		mip = 0;

		// no longer depth, if it was
		isDepth = false;
	}
	else if(wasms && resolve)
	{
		// force to 1 array slice, 1 mip
		imCreateInfo.arrayLayers = 1;
		imCreateInfo.mipLevels = 1;

		imCreateInfo.extent.width = RDCMAX(1U, imCreateInfo.extent.width>>mip);
		imCreateInfo.extent.height = RDCMAX(1U, imCreateInfo.extent.height>>mip);

		// create resolve texture
		vt->CreateImage(Unwrap(dev), &imCreateInfo, NULL, &tmpImage);
		
		VkMemoryRequirements mrq = {0};
		vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

		VkMemoryAllocateInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
			mrq.size, m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		VkImageResolve resolveRegion = {
			{ VkImageAspectFlags(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), mip, arrayIdx, 1 },
			{ 0, 0, 0 },
			{ VkImageAspectFlags(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 0, 1 },
			{ 0, 0, 0 },
			imCreateInfo.extent,
		};
		
		VkImageMemoryBarrier srcimBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			srcImage,
			{ aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
		};
		
		VkImageMemoryBarrier dstimBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0, 0, // MULTIDEVICE - need to actually pick the right queue family here maybe?
			tmpImage,
			{ aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
		};

		// ensure all previous writes have completed
		srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
		// before we go resolving
		srcimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
		{
			srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
			srcimBarrier.oldLayout = layouts.subresourceStates[si].newLayout;
			DoPipelineBarrier(cmd, 1, &srcimBarrier);
		}

		srcimBarrier.oldLayout = srcimBarrier.newLayout;

		srcimBarrier.srcAccessMask = 0;
		srcimBarrier.dstAccessMask = 0;
		
		// move tmp image into transfer destination layout
		DoPipelineBarrier(cmd, 1, &dstimBarrier);

		// resolve from live texture to resolve texture
		vt->CmdResolveImage(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(tmpImage), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolveRegion);
		
		// image layout back to normal
		for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
		{
			srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
			srcimBarrier.newLayout = layouts.subresourceStates[si].newLayout;
			DoPipelineBarrier(cmd, 1, &srcimBarrier);
		}

		// wait for resolve to finish before copy to buffer
		dstimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		dstimBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		
		DoPipelineBarrier(cmd, 1, &dstimBarrier);

		srcImage = tmpImage;

		// these have already been selected, don't need to fetch that subresource
		// when copying back to readback buffer
		arrayIdx = 0;
		mip = 0;
	}
	else if(wasms)
	{
		// copy/expand multisampled live texture to array readback texture
		RDCUNIMPLEMENTED("Saving multisampled textures directly as arrays");
	}
	
	VkImageMemoryBarrier srcimBarrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		srcImage,
		{ aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS }
	};

	// if we have no tmpImage, we're copying directly from the real image
	if(tmpImage == VK_NULL_HANDLE)
	{
		// ensure all previous writes have completed
		srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
		// before we go resolving
		srcimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
		{
			srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
			srcimBarrier.oldLayout = layouts.subresourceStates[si].newLayout;
			DoPipelineBarrier(cmd, 1, &srcimBarrier);
		}
	}

	VkImageSubresource sub = { VkImageAspectFlags(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), mip, arrayIdx };
	VkSubresourceLayout sublayout;

	vt->GetImageSubresourceLayout(Unwrap(dev), srcImage, &sub, &sublayout);
	
	VkBufferImageCopy copyregion = {
		0, 0, 0,
		{ VkImageAspectFlags(isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), mip, arrayIdx, 1 },
		{ 0, 0, 0, },
		imCreateInfo.extent,
	};

	VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			sublayout.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	};

	VkBuffer readbackBuf = VK_NULL_HANDLE;
	vkr = vt->CreateBuffer(Unwrap(dev), &bufInfo, NULL, &readbackBuf);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
	
	VkMemoryRequirements mrq = { 0 };

	vt->GetBufferMemoryRequirements(Unwrap(dev), readbackBuf, &mrq);

	VkMemoryAllocateInfo allocInfo = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
		sublayout.size, m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
	};

	VkDeviceMemory readbackMem = VK_NULL_HANDLE;
	vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &readbackMem);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	vkr = vt->BindBufferMemory(Unwrap(dev), readbackBuf, readbackMem, 0);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	// copy from desired subresource in srcImage to buffer
	vt->CmdCopyImageToBuffer(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuf, 1, &copyregion);
	
	// if we have no tmpImage, we're copying directly from the real image
	if(tmpImage == VK_NULL_HANDLE)
	{
		// image layout back to normal
		for (size_t si = 0; si < layouts.subresourceStates.size(); si++)
		{
			srcimBarrier.subresourceRange = layouts.subresourceStates[si].subresourceRange;
			srcimBarrier.newLayout = layouts.subresourceStates[si].newLayout;
			DoPipelineBarrier(cmd, 1, &srcimBarrier);
		}
	}

	VkBufferMemoryBarrier bufBarrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		readbackBuf,
		0, sublayout.size,
	};

	// wait for copy to finish before reading back to host
	DoPipelineBarrier(cmd, 1, &bufBarrier);

	vt->EndCommandBuffer(Unwrap(cmd));

	m_pDriver->SubmitCmds();
	m_pDriver->FlushQ();

	// map the buffer and copy to return buffer
	byte *pData = NULL;
	vkr = vt->MapMemory(Unwrap(dev), readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&pData);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	RDCASSERT(pData != NULL);

	dataSize = GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth, imCreateInfo.format, mip);
	byte *ret = new byte[dataSize];
	memcpy(ret, pData, dataSize);

	vt->UnmapMemory(Unwrap(dev), readbackMem);

	// clean up temporary objects
	vt->DestroyBuffer(Unwrap(dev), readbackBuf, NULL);
	vt->FreeMemory(Unwrap(dev), readbackMem, NULL);

	if(tmpImage != VK_NULL_HANDLE)
	{
		vt->DestroyImage(Unwrap(dev), tmpImage, NULL);
		vt->FreeMemory(Unwrap(dev), tmpMemory, NULL);
	}

	if(tmpFB != NULL)
	{
		for(uint32_t i=0; i < numFBs; i++)
		{
			vt->DestroyFramebuffer(Unwrap(dev), tmpFB[i], NULL);
			vt->DestroyImageView(Unwrap(dev), tmpView[i], NULL);
		}
		delete[] tmpFB;
		delete[] tmpView;
		vt->DestroyRenderPass(Unwrap(dev), tmpRP, NULL);
	}

	return ret;
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

vector<PixelModification> VulkanReplay::PixelHistory(vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx)
{
	VULKANNOTIMP("PixelHistory");
	return vector<PixelModification>();
}

ShaderDebugTrace VulkanReplay::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
	VULKANNOTIMP("DebugVertex");
	return ShaderDebugTrace();
}

ShaderDebugTrace VulkanReplay::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive)
{
	VULKANNOTIMP("DebugPixel");
	return ShaderDebugTrace();
}

ShaderDebugTrace VulkanReplay::DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
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

ReplayCreateStatus Vulkan_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
	RDCDEBUG("Creating a VulkanReplay replay device");

	void *module = Process::LoadModule(VulkanLibraryName);
	
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
	
	VulkanReplay::PreDeviceInitCounters();

	WrappedVulkan *vk = new WrappedVulkan(logfile);
	vk->Initialise(initParams);
	
	RDCLOG("Created device.");
	VulkanReplay *replay = vk->GetReplay();
	replay->SetProxy(logfile == NULL);

	*driver = (IReplayDriver *)replay;

	return eReplayCreate_Success;
}

static DriverRegistration VkDriverRegistration(RDC_Vulkan, "Vulkan", &Vulkan_CreateReplayDevice);
