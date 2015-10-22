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

#include "../vk_core.h"
#include "../vk_debug.h"

#include "maths/formatpacking.h"

#include "jpeg-compressor/jpge.h"

// VKTODOLOW this should be tidied away
#if defined(LINUX)
struct xcb_connection_t;

// bit of a hack
namespace Keyboard { void UseConnection(xcb_connection_t *conn); }
#endif

///////////////////////////////////////////////////////////////////////////////////////
// WSI extension

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceSupportKHR(
		VkPhysicalDevice                        physicalDevice,
		uint32_t                                queueFamilyIndex,
		const VkSurfaceDescriptionKHR*          pSurfaceDescription,
		VkBool32*                               pSupported)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfaceSupportKHR(Unwrap(physicalDevice), queueFamilyIndex, pSurfaceDescription, pSupported);
}

VkResult WrappedVulkan::vkGetSurfacePropertiesKHR(
		VkDevice                                 device,
		const VkSurfaceDescriptionKHR*           pSurfaceDescription,
		VkSurfacePropertiesKHR*                  pSurfaceProperties)
{
	return ObjDisp(device)->GetSurfacePropertiesKHR(Unwrap(device), pSurfaceDescription, pSurfaceProperties);
}

VkResult WrappedVulkan::vkGetSurfaceFormatsKHR(
		VkDevice                                 device,
		const VkSurfaceDescriptionKHR*           pSurfaceDescription,
		uint32_t*                                pCount,
		VkSurfaceFormatKHR*                      pSurfaceFormats)
{
	return ObjDisp(device)->GetSurfaceFormatsKHR(Unwrap(device), pSurfaceDescription, pCount, pSurfaceFormats);
}

VkResult WrappedVulkan::vkGetSurfacePresentModesKHR(
		VkDevice                                 device,
		const VkSurfaceDescriptionKHR*           pSurfaceDescription,
		uint32_t*                                pCount,
		VkPresentModeKHR*                        pPresentModes)
{
	return ObjDisp(device)->GetSurfacePresentModesKHR(Unwrap(device), pSurfaceDescription, pCount, pPresentModes);
}

bool WrappedVulkan::Serialise_vkGetSwapchainImagesKHR(
		Serialiser*                              localSerialiser,
		VkDevice                                 device,
		VkSwapchainKHR                           swapchain,
		uint32_t*                                pCount,
		VkImage*                                 pSwapchainImages)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, swapId, GetResID(swapchain));
	SERIALISE_ELEMENT(uint32_t, idx, *pCount);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapchainImages));

	if(m_State == READING)
	{
		// use original ID because we don't create a live version of the swapchain
		auto &swapInfo = m_CreationInfo.m_SwapChain[swapId];

		// VKTODOLOW what if num images is less than on capture?
		RDCASSERT(idx < swapInfo.images.size());
		GetResourceManager()->AddLiveResource(id, swapInfo.images[idx].im);

		m_CreationInfo.m_Image[GetResID(swapInfo.images[idx].im)] = m_CreationInfo.m_Image[swapId];
	}

	return true;
}

VkResult WrappedVulkan::vkGetSwapchainImagesKHR(
		VkDevice                                 device,
		VkSwapchainKHR                           swapchain,
		uint32_t*                                pCount,
		VkImage*                                 pSwapchainImages)
{
	// make sure we always get the size
	uint32_t dummySize = 0;
	if(pCount == NULL)
		pCount = &dummySize;

	VkResult ret = ObjDisp(device)->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swapchain), pCount, pSwapchainImages);

	if(pSwapchainImages && m_State >= WRITING)
	{
		uint32_t numImages = *pCount;

		for(uint32_t i=0; i < numImages; i++)
		{
			// these were all wrapped and serialised on swapchain create - we just have to
			// return the wrapped image in that case
			if(GetResourceManager()->HasWrapper(ToTypedHandle(pSwapchainImages[i])))
			{
				pSwapchainImages[i] = (VkImage)(uint64_t)GetResourceManager()->GetWrapper(ToTypedHandle(pSwapchainImages[i]));
			}
			else
			{
				ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pSwapchainImages[i]);

				if(m_State >= WRITING)
				{
					Chunk *chunk = NULL;

					{
						CACHE_THREAD_SERIALISER();
						
						SCOPED_SERIALISE_CONTEXT(GET_SWAPCHAIN_IMAGE);
						Serialise_vkGetSwapchainImagesKHR(localSerialiser, device, swapchain, &i, &pSwapchainImages[i]);

						chunk = scope.Get();
					}

					VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pSwapchainImages[i]);
					record->AddChunk(chunk);

					// we invert the usual scheme - we make the swapchain record take parent refs
					// on these images, so that we can just ref the swapchain on present and pull
					// in all the images
					VkResourceRecord *swaprecord = GetRecord(swapchain);

					swaprecord->AddParent(record);
					// decrement refcount on swap images, so that they are only ref'd from the swapchain
					// (and will be deleted when it is deleted)
					record->Delete(GetResourceManager());
				}
				else
				{
					GetResourceManager()->AddLiveResource(id, pSwapchainImages[i]);
				}
			}
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkAcquireNextImageKHR(
		VkDevice                                 device,
		VkSwapchainKHR                           swapChain,
		uint64_t                                 timeout,
		VkSemaphore                              semaphore,
		uint32_t*                                pImageIndex)
{
	// VKTODOLOW: does this need to be intercepted/serialised?
	return ObjDisp(device)->AcquireNextImageKHR(Unwrap(device), Unwrap(swapChain), timeout, Unwrap(semaphore), pImageIndex);
}

bool WrappedVulkan::Serialise_vkCreateSwapchainKHR(
		Serialiser*                             localSerialiser,
		VkDevice                                device,
		const VkSwapchainCreateInfoKHR*         pCreateInfo,
		VkSwapchainKHR*                         pSwapChain)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSwapchainCreateInfoKHR, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapChain));
	
// VKTODOLOW this should be tidied away
#if defined(LINUX)
	if(pCreateInfo && pCreateInfo->pSurfaceDescription)
	{
		VkSurfaceDescriptionWindowKHR *surf = (VkSurfaceDescriptionWindowKHR*)pCreateInfo->pSurfaceDescription;

		if(surf->platform == VK_PLATFORM_XCB_KHR)
		{
			VkPlatformHandleXcbKHR *handle = (VkPlatformHandleXcbKHR *)surf->pPlatformHandle;
			Keyboard::UseConnection(handle->connection);
		}
	}
#endif

	uint32_t numIms = 0;

	if(m_State >= WRITING)
	{
		VkResult vkr = VK_SUCCESS;

		vkr = ObjDisp(device)->GetSwapchainImagesKHR(Unwrap(device), Unwrap(*pSwapChain), &numIms, NULL);
		RDCASSERT(vkr == VK_SUCCESS);
	}

	SERIALISE_ELEMENT(uint32_t, numSwapImages, numIms);

	if(m_State == READING)
	{
		// use original ID because we don't create a live version of the swapchain
		SwapchainInfo &swapinfo = m_CreationInfo.m_SwapChain[id];

		swapinfo.format = info.imageFormat;
		swapinfo.extent = info.imageExtent;
		swapinfo.arraySize = info.imageArraySize;

		swapinfo.images.resize(numSwapImages);

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		const VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, info.imageFormat,
			{ info.imageExtent.width, info.imageExtent.height, 1 },
			1, info.imageArraySize, 1,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|
			VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
			VK_IMAGE_USAGE_SAMPLED_BIT,
			0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].dev == device)
				m_SwapPhysDevice = (int)i;
		}

		for(uint32_t i=0; i < numSwapImages; i++)
		{
			VkDeviceMemory mem = VK_NULL_HANDLE;
			VkImage im = VK_NULL_HANDLE;

			VkResult vkr = ObjDisp(device)->CreateImage(Unwrap(device), &imInfo, &im);
			RDCASSERT(vkr == VK_SUCCESS);

			ResourceId liveId = GetResourceManager()->WrapResource(Unwrap(device), im);
			
			VkMemoryRequirements mrq = {0};

			vkr = ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(im), &mrq);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				mrq.size, GetGPULocalMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = ObjDisp(device)->AllocMemory(Unwrap(device), &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);
			
			GetResourceManager()->WrapResource(Unwrap(device), mem);

			vkr = ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(im), Unwrap(mem), 0);
			RDCASSERT(vkr == VK_SUCCESS);

			// image live ID will be assigned separately in Serialise_vkGetSwapChainInfoWSI
			// memory doesn't have a live ID

			swapinfo.images[i].im = im;

			// fill out image info so we track resource state transitions
			// sneaky-cheeky use of the swapchain's ID here (it's not a live ID because
			// we don't create a live swapchain). This will be picked up in
			// Serialise_vkGetSwapchainImagesKHR to set the data for the live IDs on the
			// swapchain images.
			VulkanCreationInfo::Image &iminfo = m_CreationInfo.m_Image[id];
			iminfo.type = VK_IMAGE_TYPE_2D;
			iminfo.format = info.imageFormat;
			iminfo.extent.width = info.imageExtent.width;
			iminfo.extent.height = info.imageExtent.height;
			iminfo.extent.depth = 1;
			iminfo.mipLevels = 1;
			iminfo.arraySize = info.imageArraySize;
			iminfo.creationFlags = eTextureCreate_SRV|eTextureCreate_RTV|eTextureCreate_SwapBuffer;
			iminfo.cube = false;
			iminfo.samples = 1;

			m_CreationInfo.m_Names[liveId] = StringFormat::Fmt("Presentable Image %u", i);

			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArrayLayer = 0;
			range.mipLevels = 1;
			range.arraySize = info.imageArraySize;
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			m_ImageLayouts[liveId].subresourceStates.clear();
			m_ImageLayouts[liveId].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSwapchainKHR(
		VkDevice                                device,
		const VkSwapchainCreateInfoKHR*         pCreateInfo,
		VkSwapchainKHR*                         pSwapChain)
{
	VkSwapchainCreateInfoKHR createInfo = *pCreateInfo;

	// make sure we can readback to get the screenshot
	createInfo.imageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT;

	VkResult ret = ObjDisp(device)->CreateSwapchainKHR(Unwrap(device), &createInfo, pSwapChain);
	
	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSwapChain);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);
				Serialise_vkCreateSwapchainKHR(localSerialiser, device, pCreateInfo, pSwapChain);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSwapChain);
			record->AddChunk(chunk);

			for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
			{
				if(m_PhysicalReplayData[i].dev == device)
					m_SwapPhysDevice = (int)i;
			}
			
			record->swapInfo = new SwapchainInfo();
			SwapchainInfo &swapInfo = *record->swapInfo;
			
			swapInfo.format = pCreateInfo->imageFormat;
			swapInfo.extent = pCreateInfo->imageExtent;
			swapInfo.arraySize = pCreateInfo->imageArraySize;

			VkResult vkr = VK_SUCCESS;

			const VkLayerDispatchTable *vt = ObjDisp(device);

			{
				VkAttachmentDescription attDesc = {
					VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
					pCreateInfo->imageFormat, 1,
					VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					0
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

				vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, &swapInfo.rp);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), swapInfo.rp);
			}

			// serialise out the swap chain images
			{
				uint32_t numSwapImages;
				VkResult ret = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(*pSwapChain), &numSwapImages, NULL);
				RDCASSERT(ret == VK_SUCCESS);
				
				swapInfo.images.resize(numSwapImages);

				VkImage* images = new VkImage[numSwapImages];

				// go through our own function so we assign these images IDs
				ret = vkGetSwapchainImagesKHR(device, *pSwapChain, &numSwapImages, images);
				RDCASSERT(ret == VK_SUCCESS);

				for(uint32_t i=0; i < numSwapImages; i++)
				{
					SwapchainInfo::SwapImage &swapImInfo = swapInfo.images[i];

					// memory doesn't exist for genuine WSI created images
					swapImInfo.im = images[i];

					ResourceId imid = GetResID(images[i]);

					VkImageSubresourceRange range;
					range.baseMipLevel = range.baseArrayLayer = 0;
					range.mipLevels = 1;
					range.arraySize = pCreateInfo->imageArraySize;
					range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					
					// fill out image info so we track resource state transitions
					{
						SCOPED_LOCK(m_ImageLayoutsLock);
						m_ImageLayouts[imid].subresourceStates.clear();
						m_ImageLayouts[imid].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
					}

					{
						VkImageViewCreateInfo info = {
							VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
							Unwrap(images[i]), VK_IMAGE_VIEW_TYPE_2D,
							pCreateInfo->imageFormat,
							{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
							{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
							0 // flags
						};

						vkr = vt->CreateImageView(Unwrap(device), &info, &swapImInfo.view);
						RDCASSERT(vkr == VK_SUCCESS);

						GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.view);

						VkFramebufferCreateInfo fbinfo = {
							VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
							Unwrap(swapInfo.rp),
							1, UnwrapPtr(swapImInfo.view),
							(uint32_t)pCreateInfo->imageExtent.width, (uint32_t)pCreateInfo->imageExtent.height, 1,
						};

						vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, &swapImInfo.fb);
						RDCASSERT(vkr == VK_SUCCESS);

						GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.fb);
					}
				}

				SAFE_DELETE_ARRAY(images);
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSwapChain);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkQueuePresentKHR(
			VkQueue                                 queue,
			VkPresentInfoKHR*                       pPresentInfo)
{
	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);
	
	if(m_State == WRITING_IDLE)
		RenderDoc::Inst().Tick();
	
	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame

	if(pPresentInfo->swapchainCount > 1 && (m_FrameCounter % 100) == 0)
	{
		RDCWARN("Presenting multiple swapchains at once - only first will be processed");
	}
	
	// VKTODOLOW handle present info pNext
	RDCASSERT(pPresentInfo->pNext == NULL);
	
	ResourceId swapid = GetResID(pPresentInfo->swapchains[0]);
	VkResourceRecord *swaprecord = GetRecord(pPresentInfo->swapchains[0]);
	RDCASSERT(swaprecord->swapInfo);

	const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

	VkImage backbuffer = swapInfo.images[pPresentInfo->imageIndices[0]].im;
	
	// VKTODOLOW multiple windows/captures etc
	bool activeWindow = true; //RenderDoc::Inst().IsActiveWindow((ID3D11Device *)this, swapdesc.OutputWindow);

	if(m_State == WRITING_IDLE)
	{
		m_FrameTimes.push_back(m_FrameTimer.GetMilliseconds());
		m_TotalTime += m_FrameTimes.back();
		m_FrameTimer.Restart();

		// update every second
		if(m_TotalTime > 1000.0)
		{
			m_MinFrametime = 10000.0;
			m_MaxFrametime = 0.0;
			m_AvgFrametime = 0.0;

			m_TotalTime = 0.0;

			for(size_t i=0; i < m_FrameTimes.size(); i++)
			{
				m_AvgFrametime += m_FrameTimes[i];
				if(m_FrameTimes[i] < m_MinFrametime)
					m_MinFrametime = m_FrameTimes[i];
				if(m_FrameTimes[i] > m_MaxFrametime)
					m_MaxFrametime = m_FrameTimes[i];
			}

			m_AvgFrametime /= double(m_FrameTimes.size());

			m_FrameTimes.clear();
		}
		
		uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

		if(overlay & eRENDERDOC_Overlay_Enabled)
		{
			VkRenderPass rp = swapInfo.rp;
			VkFramebuffer fb = swapInfo.images[pPresentInfo->imageIndices[0]].fb;

			VkLayerDispatchTable *vt = ObjDisp(GetDev());

			TextPrintState textstate = { GetNextCmd(), rp, fb, swapInfo.extent.width, swapInfo.extent.height };

			GetDebugManager()->BeginText(textstate);

			if(activeWindow)
			{
				vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetCaptureKeys();

				string overlayText = "Vulkan. ";

				for(size_t i=0; i < keys.size(); i++)
				{
					if(i > 0)
						overlayText += ", ";

					overlayText += ToStr::Get(keys[i]);
				}

				if(!keys.empty())
					overlayText += " to capture.";

				if(overlay & eRENDERDOC_Overlay_FrameNumber)
				{
					overlayText += StringFormat::Fmt(" Frame: %d.", m_FrameCounter);
				}
				if(overlay & eRENDERDOC_Overlay_FrameRate)
				{
					overlayText += StringFormat::Fmt(" %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)",
																					m_AvgFrametime, m_MinFrametime, m_MaxFrametime, 1000.0f/m_AvgFrametime);
				}

				float y=0.0f;

				if(!overlayText.empty())
				{
					GetDebugManager()->RenderText(textstate, 0.0f, y, overlayText.c_str());
					y += 1.0f;
				}

				if(overlay & eRENDERDOC_Overlay_CaptureList)
				{
					GetDebugManager()->RenderText(textstate, 0.0f, y, "%d Captures saved.\n", (uint32_t)m_FrameRecord.size());
					y += 1.0f;

					uint64_t now = Timing::GetUnixTimestamp();
					for(size_t i=0; i < m_FrameRecord.size(); i++)
					{
						if(now - m_FrameRecord[i].frameInfo.captureTime < 20)
						{
							GetDebugManager()->RenderText(textstate, 0.0f, y, "Captured frame %d.\n", m_FrameRecord[i].frameInfo.frameNumber);
							y += 1.0f;
						}
					}
				}

				// VKTODOLOW failed frames

#if !defined(RELEASE)
				GetDebugManager()->RenderText(textstate, 0.0f, y, "%llu chunks - %.2f MB", Chunk::NumLiveChunks(), float(Chunk::TotalMem())/1024.0f/1024.0f);
				y += 1.0f;
#endif
			}
			else
			{
				vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetFocusKeys();

				string str = "Vulkan. Inactive swapchain.";

				for(size_t i=0; i < keys.size(); i++)
				{
					if(i == 0)
						str += " ";
					else
						str += ", ";

					str += ToStr::Get(keys[i]);
				}

				if(!keys.empty())
					str += " to cycle between swapchains";
				
				GetDebugManager()->RenderText(textstate, 0.0f, 0.0f, str.c_str());
			}
			
			GetDebugManager()->EndText(textstate);

			SubmitCmds();
		}
	}
	
	// kill any current capture
	if(m_State == WRITING_CAPFRAME)
	{
		//if(HasSuccessfulCapture())
		{
			RDCLOG("Finished capture, Frame %u", m_FrameCounter);

			GetResourceManager()->MarkResourceFrameReferenced(swapid, eFrameRef_Read);

			// transition back to IDLE atomically
			{
				SCOPED_LOCK(m_CapTransitionLock);
				EndCaptureFrame(backbuffer);
				FinishCapture();
			}

			byte *thpixels = NULL;
			uint32_t thwidth = 0;
			uint32_t thheight = 0;

			// gather backbuffer screenshot
			const int32_t maxSize = 1024;

			// VKTODOLOW split this out properly into begin/end frame capture
			if(1)//if(wnd)
			{
				VkDevice dev = GetDev();
				VkCmdBuffer cmd = GetNextCmd();

				const VkLayerDispatchTable *vt = ObjDisp(dev);

				// VKTODOLOW idle all devices? or just the device for this queue?
				vt->DeviceWaitIdle(Unwrap(dev));

				// since these objects are very short lived (only this scope), we
				// don't wrap them.
				VkImage readbackIm = VK_NULL_HANDLE;
				VkDeviceMemory readbackMem = VK_NULL_HANDLE;

				VkResult vkr = VK_SUCCESS;

				// create identical image
				VkImageCreateInfo imInfo = {
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
					VK_IMAGE_TYPE_2D, swapInfo.format,
					{ swapInfo.extent.width, swapInfo.extent.height, 1 }, 1, 1, 1,
					VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
					0,
					VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
					VK_IMAGE_LAYOUT_UNDEFINED,
				};
				vt->CreateImage(Unwrap(dev), &imInfo, &readbackIm);
				RDCASSERT(vkr == VK_SUCCESS);

				VkMemoryRequirements mrq;
				vkr = vt->GetImageMemoryRequirements(Unwrap(dev), readbackIm, &mrq);
				RDCASSERT(vkr == VK_SUCCESS);

				VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR, 0, 0 };
				VkSubresourceLayout layout = { 0 };
				vt->GetImageSubresourceLayout(Unwrap(dev), readbackIm, &subr, &layout);

				// allocate readback memory
				VkMemoryAllocInfo allocInfo = {
					VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
					mrq.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
				};

				vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &readbackMem);
				RDCASSERT(vkr == VK_SUCCESS);
				vkr = vt->BindImageMemory(Unwrap(dev), readbackIm, readbackMem, 0);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

				// do image copy
				vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
				RDCASSERT(vkr == VK_SUCCESS);

				VkImageCopy cpy = {
					{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
					{ 0, 0, 0 },
					{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
					{ 0, 0, 0 },
					{ imInfo.extent.width, imInfo.extent.height, 1 },
				};

				// VKTODOLOW back buffer must be in this layout right?
				VkImageMemoryBarrier bbTrans = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0, VK_IMAGE_LAYOUT_PRESENT_SOURCE_KHR, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					Unwrap(backbuffer),
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

				VkImageMemoryBarrier readTrans = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					readbackIm, // was never wrapped
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

				VkImageMemoryBarrier *barriers[] = {
					&bbTrans,
					&readTrans,
				};

				vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, (void **)barriers);

				vt->CmdCopyImage(Unwrap(cmd), Unwrap(backbuffer), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, readbackIm, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &cpy);

				// transition backbuffer back
				std::swap(bbTrans.oldLayout, bbTrans.newLayout);

				// VKTODOLOW find out correct image layout for reading back
				readTrans.oldLayout = readTrans.newLayout;
				readTrans.newLayout = VK_IMAGE_LAYOUT_GENERAL;

				vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, (void **)barriers);

				vkr = vt->EndCommandBuffer(Unwrap(cmd));
				RDCASSERT(vkr == VK_SUCCESS);

				SubmitCmds();
				FlushQ(); // need to wait so we can readback

				// map memory and readback
				byte *pData = NULL;
				vkr = vt->MapMemory(Unwrap(dev), readbackMem, 0, 0, 0, (void **)&pData);
				RDCASSERT(vkr == VK_SUCCESS);

				RDCASSERT(pData != NULL);

				// point sample info into raw buffer
				{
					ResourceFormat fmt = MakeResourceFormat(imInfo.format);

					byte *data = (byte *)pData;

					data += layout.offset;

					float widthf = float(imInfo.extent.width);
					float heightf = float(imInfo.extent.height);

					float aspect = widthf/heightf;

					thwidth = RDCMIN(maxSize, imInfo.extent.width);
					thwidth &= ~0x7; // align down to multiple of 8
					thheight = uint32_t(float(thwidth)/aspect);

					thpixels = new byte[3*thwidth*thheight];

					uint32_t stride = fmt.compByteWidth*fmt.compCount;

					bool buf1010102 = false;
					bool bufBGRA = false;

					if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
					{
						stride = 4;
						buf1010102 = true;
					}
					if(fmt.special && fmt.specialFormat == eSpecial_B8G8R8A8)
					{
						stride = 4;
						bufBGRA = true;
					}

					byte *dst = thpixels;

					for(uint32_t y=0; y < thheight; y++)
					{
						for(uint32_t x=0; x < thwidth; x++)
						{
							float xf = float(x)/float(thwidth);
							float yf = float(y)/float(thheight);

							byte *src = &data[ stride*uint32_t(xf*widthf) + layout.rowPitch*uint32_t(yf*heightf) ];

							if(buf1010102)
							{
								uint32_t *src1010102 = (uint32_t *)src;
								Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
								dst[0] = (byte)(unorm.x*255.0f);
								dst[1] = (byte)(unorm.y*255.0f);
								dst[2] = (byte)(unorm.z*255.0f);
							}
							else if(bufBGRA)
							{
								dst[0] = src[2];
								dst[1] = src[1];
								dst[2] = src[0];
							}
							else if(fmt.compByteWidth == 2) // R16G16B16A16 backbuffer
							{
								uint16_t *src16 = (uint16_t *)src;

								float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
								float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
								float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

								if(linearR < 0.0031308f) dst[0] = byte(255.0f*(12.92f * linearR));
								else                     dst[0] = byte(255.0f*(1.055f * powf(linearR, 1.0f/2.4f) - 0.055f));

								if(linearG < 0.0031308f) dst[1] = byte(255.0f*(12.92f * linearG));
								else                     dst[1] = byte(255.0f*(1.055f * powf(linearG, 1.0f/2.4f) - 0.055f));

								if(linearB < 0.0031308f) dst[2] = byte(255.0f*(12.92f * linearB));
								else                     dst[2] = byte(255.0f*(1.055f * powf(linearB, 1.0f/2.4f) - 0.055f));
							}
							else
							{
								dst[0] = src[0];
								dst[1] = src[1];
								dst[2] = src[2];
							}

							dst += 3;
						}
					}
				}

				vt->UnmapMemory(Unwrap(dev), readbackMem);

				// delete all
				vt->DestroyImage(Unwrap(dev), readbackIm);
				vt->FreeMemory(Unwrap(dev), readbackMem);
			}

			byte *jpgbuf = NULL;
			int len = thwidth*thheight;

			// VKTODOLOW split this out properly into begin/end frame capture
			if(1)//if(wnd)
			{
				jpgbuf = new byte[len];

				jpge::params p;

				p.m_quality = 40;

				bool success = jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

				if(!success)
				{
					RDCERR("Failed to compress to jpg");
					SAFE_DELETE_ARRAY(jpgbuf);
					thwidth = 0;
					thheight = 0;
				}
			}

			Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

				m_pFileSerialiser->Insert(scope.Get(true));
			}

			GetResourceManager()->Hack_PropagateReferencesToMemory();

			RDCDEBUG("Inserting Resource Serialisers");	

			GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);
			
			GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

			RDCDEBUG("Creating Capture Scope");	

			{
				Serialiser *localSerialiser = GetMainSerialiser();
		
				SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

				Serialise_CaptureScope(0);

				m_pFileSerialiser->Insert(scope.Get(true));

				m_pFileSerialiser->Insert(m_HeaderChunk);
			}

			// don't need to lock access to m_CmdBufferRecords as we are no longer 
			// in capframe (the transition is thread-protected) so nothing will be
			// pushed to the vector

			{
				RDCDEBUG("Flushing %u command buffer records to file serialiser", (uint32_t)m_CmdBufferRecords.size());	

				map<int32_t, Chunk *> recordlist;

				// ensure all command buffer records within the frame evne if recorded before, but
				// otherwise order must be preserved (vs. queue submits and desc set updates)
				for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
				{
					m_CmdBufferRecords[i]->Insert(recordlist);

					RDCDEBUG("Adding %u chunks to file serialiser from command buffer %llu", (uint32_t)recordlist.size(), m_CmdBufferRecords[i]->GetResourceID());	
				}

				m_FrameCaptureRecord->Insert(recordlist);

				RDCDEBUG("Flushing %u chunks to file serialiser from context record", (uint32_t)recordlist.size());	

				for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
					m_pFileSerialiser->Insert(it->second);

				RDCDEBUG("Done");	
			}

			m_pFileSerialiser->FlushToDisk();

			RenderDoc::Inst().SuccessfullyWrittenLog();

			SAFE_DELETE(m_pFileSerialiser);
			SAFE_DELETE(m_HeaderChunk);

			m_State = WRITING_IDLE;

			// delete cmd buffers now - had to keep them alive until after serialiser flush.
			for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
				m_CmdBufferRecords[i]->Delete(GetResourceManager());

			m_CmdBufferRecords.clear();
			
			GetResourceManager()->MarkUnwrittenResources();

			GetResourceManager()->ClearReferencedResources();

			// VKTODOHIGH This is a huuuge hack while the shutdown order is all
			// messed up and normal calls to WrappedVulkan::ReleaseResource are
			// ignored while WRITING to avoid shutdown problems.
			extern bool releasingInitContents;
			releasingInitContents = true;
			GetResourceManager()->FreeInitialContents();
			releasingInitContents = false;

			GetResourceManager()->FlushPendingDirty();
		}
	}

	if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE)
	{
		FetchFrameRecord record;
		record.frameInfo.frameNumber = m_FrameCounter+1;
		record.frameInfo.captureTime = Timing::GetUnixTimestamp();
		m_FrameRecord.push_back(record);

		GetResourceManager()->ClearReferencedResources();

		GetResourceManager()->MarkResourceFrameReferenced(m_InstanceRecord->GetResourceID(), eFrameRef_Read);

		// need to do all this atomically so that no other commands
		// will check to see if they need to markdirty or markpendingdirty
		// and go into the frame record.
		{
			SCOPED_LOCK(m_CapTransitionLock);
			GetResourceManager()->PrepareInitialContents();

			AttemptCapture();
			BeginCaptureFrame();

			m_State = WRITING_CAPFRAME;
		}

		RDCLOG("Starting capture, frame %u", m_FrameCounter);
	}

	vector<VkSwapchainKHR> unwrappedSwaps;
	
	VkPresentInfoKHR unwrappedInfo = *pPresentInfo;

	for(uint32_t i=0; i < unwrappedInfo.swapchainCount; i++)
		unwrappedSwaps.push_back(Unwrap(unwrappedInfo.swapchains[i]));

	unwrappedInfo.swapchains = &unwrappedSwaps.front();

	return ObjDisp(queue)->QueuePresentKHR(Unwrap(queue), &unwrappedInfo);
}
