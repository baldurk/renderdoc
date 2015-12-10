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

///////////////////////////////////////////////////////////////////////////////////////
// WSI extension

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceSupportKHR(
		VkPhysicalDevice                            physicalDevice,
		uint32_t                                    queueFamilyIndex,
		VkSurfaceKHR                                surface,
    VkBool32*                                   pSupported)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfaceSupportKHR(Unwrap(physicalDevice), queueFamilyIndex, surface, pSupported);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    VkSurfaceCapabilitiesKHR*                   pSurfaceCapabilities)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfaceCapabilitiesKHR(Unwrap(physicalDevice), surface, pSurfaceCapabilities);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceFormatsKHR(
		VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pSurfaceFormatCount,
    VkSurfaceFormatKHR*                         pSurfaceFormats)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfaceFormatsKHR(Unwrap(physicalDevice), surface, pSurfaceFormatCount, pSurfaceFormats);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfacePresentModesKHR(
		VkPhysicalDevice                            physicalDevice,
    VkSurfaceKHR                                surface,
    uint32_t*                                   pPresentModeCount,
    VkPresentModeKHR*                           pPresentModes)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfacePresentModesKHR(Unwrap(physicalDevice), surface, pPresentModeCount, pPresentModes);
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
					VkResourceRecord *swaprecord = GetRecord(swapchain);

					record->SpecialResource = true;

					record->AddParent(swaprecord);

					// note we add the chunk to the swap record, that way when the swapchain is created it will
					// always create all of its images on replay. The image's record is kept around for reference
					// tracking and any other chunks. Because it has a parent relationship on the swapchain, if
					// the image is referenced the swapchain (and thus all the getimages) will be included.
					swaprecord->AddChunk(chunk);
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
		VkDevice                                     device,
    VkSwapchainKHR                               swapchain,
    uint64_t                                     timeout,
    VkSemaphore                                  semaphore,
    VkFence                                      fence,
    uint32_t*                                    pImageIndex)
{
	return ObjDisp(device)->AcquireNextImageKHR(Unwrap(device), Unwrap(swapchain), timeout, Unwrap(semaphore), Unwrap(fence), pImageIndex);
}

bool WrappedVulkan::Serialise_vkCreateSwapchainKHR(
		Serialiser*                             localSerialiser,
		VkDevice                                device,
		const VkSwapchainCreateInfoKHR*         pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
		VkSwapchainKHR*                         pSwapChain)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSwapchainCreateInfoKHR, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapChain));

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
		swapinfo.arraySize = info.imageArrayLayers;

		swapinfo.images.resize(numSwapImages);

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		const VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D, info.imageFormat,
			{ info.imageExtent.width, info.imageExtent.height, 1 },
			1, info.imageArrayLayers, VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT|
			VK_IMAGE_USAGE_TRANSFER_DST_BIT|
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
			VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};

		for(uint32_t i=0; i < numSwapImages; i++)
		{
			VkDeviceMemory mem = VK_NULL_HANDLE;
			VkImage im = VK_NULL_HANDLE;

			VkResult vkr = ObjDisp(device)->CreateImage(Unwrap(device), &imInfo, NULL, &im);
			RDCASSERT(vkr == VK_SUCCESS);

			ResourceId liveId = GetResourceManager()->WrapResource(Unwrap(device), im);
			
			VkMemoryRequirements mrq = {0};

			ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(im), &mrq);
			
			VkMemoryAllocateInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
				mrq.size, GetGPULocalMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = ObjDisp(device)->AllocateMemory(Unwrap(device), &allocInfo, NULL, &mem);
			RDCASSERT(vkr == VK_SUCCESS);
			
			ResourceId memid = GetResourceManager()->WrapResource(Unwrap(device), mem);
			// register as a live-only resource, so it is cleaned up properly
			GetResourceManager()->AddLiveResource(memid, mem);

			vkr = ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(im), Unwrap(mem), 0);
			RDCASSERT(vkr == VK_SUCCESS);

			// image live ID will be assigned separately in Serialise_vkGetSwapChainInfoWSI
			// memory doesn't have a live ID

			swapinfo.images[i].im = im;

			// fill out image info so we track resource state barriers
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
			iminfo.arrayLayers = info.imageArrayLayers;
			iminfo.creationFlags = eTextureCreate_SRV|eTextureCreate_RTV|eTextureCreate_SwapBuffer;
			iminfo.cube = false;
			iminfo.samples = VK_SAMPLE_COUNT_1_BIT;

			m_CreationInfo.m_Names[liveId] = StringFormat::Fmt("Presentable Image %u", i);

			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArrayLayer = 0;
			range.levelCount = 1;
			range.layerCount = info.imageArrayLayers;
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			m_ImageLayouts[liveId].subresourceStates.clear();
			m_ImageLayouts[liveId].subresourceStates.push_back(ImageRegionState(range, UNKNOWN_PREV_IMG_LAYOUT, VK_IMAGE_LAYOUT_UNDEFINED));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSwapchainKHR(
		VkDevice                                device,
		const VkSwapchainCreateInfoKHR*         pCreateInfo,
		const VkAllocationCallbacks*            pAllocator,
		VkSwapchainKHR*                         pSwapChain)
{
	VkSwapchainCreateInfoKHR createInfo = *pCreateInfo;

	// make sure we can readback to get the screenshot
	createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	createInfo.surface = Unwrap(createInfo.surface);

	VkResult ret = ObjDisp(device)->CreateSwapchainKHR(Unwrap(device), &createInfo, pAllocator, pSwapChain);
	
	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSwapChain);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);
				Serialise_vkCreateSwapchainKHR(localSerialiser, device, pCreateInfo, NULL, pSwapChain);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSwapChain);
			record->AddChunk(chunk);
			
			record->swapInfo = new SwapchainInfo();
			SwapchainInfo &swapInfo = *record->swapInfo;

			// sneaky casting of window handle into record
			swapInfo.wndHandle = (RENDERDOC_WindowHandle)GetRecord(pCreateInfo->surface);

			{
				SCOPED_LOCK(m_SwapLookupLock);
				m_SwapLookup[swapInfo.wndHandle] = *pSwapChain;
			}

			RenderDoc::Inst().AddFrameCapturer(LayerDisp(m_Instance), swapInfo.wndHandle, this);
			
			swapInfo.format = pCreateInfo->imageFormat;
			swapInfo.extent = pCreateInfo->imageExtent;
			swapInfo.arraySize = pCreateInfo->imageArrayLayers;

			VkResult vkr = VK_SUCCESS;

			const VkLayerDispatchTable *vt = ObjDisp(device);

			{
				VkAttachmentDescription attDesc = {
					0, pCreateInfo->imageFormat, VK_SAMPLE_COUNT_1_BIT,
					VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

				vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, NULL, &swapInfo.rp);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), swapInfo.rp);
			}

			// serialise out the swap chain images
			{
				uint32_t numSwapImages;
				VkResult ret = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(*pSwapChain), &numSwapImages, NULL);
				RDCASSERT(ret == VK_SUCCESS);
				
				swapInfo.lastPresent = 0;
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
					range.levelCount = 1;
					range.layerCount = pCreateInfo->imageArrayLayers;
					range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					
					// fill out image info so we track resource state barriers
					{
						SCOPED_LOCK(m_ImageLayoutsLock);
						m_ImageLayouts[imid].subresourceStates.clear();
						m_ImageLayouts[imid].subresourceStates.push_back(ImageRegionState(range, UNKNOWN_PREV_IMG_LAYOUT, VK_IMAGE_LAYOUT_UNDEFINED));
					}

					{
						VkImageViewCreateInfo info = {
							VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
							Unwrap(images[i]), VK_IMAGE_VIEW_TYPE_2D,
							pCreateInfo->imageFormat,
							{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
							{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
						};

						vkr = vt->CreateImageView(Unwrap(device), &info, NULL, &swapImInfo.view);
						RDCASSERT(vkr == VK_SUCCESS);

						GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.view);

						VkFramebufferCreateInfo fbinfo = {
							VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
							Unwrap(swapInfo.rp),
							1, UnwrapPtr(swapImInfo.view),
							(uint32_t)pCreateInfo->imageExtent.width, (uint32_t)pCreateInfo->imageExtent.height, 1,
						};

						vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, NULL, &swapImInfo.fb);
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
			VkQueue                                      queue,
			const VkPresentInfoKHR*                      pPresentInfo)
{
	if(m_State == WRITING_IDLE)
	{
		RenderDoc::Inst().Tick();

		GetResourceManager()->FlushPendingDirty();
	}
	
	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame

	if(pPresentInfo->swapchainCount > 1 && (m_FrameCounter % 100) == 0)
	{
		RDCWARN("Presenting multiple swapchains at once - only first will be processed");
	}
	
	vector<VkSwapchainKHR> unwrappedSwaps;
	
	VkPresentInfoKHR unwrappedInfo = *pPresentInfo;

	for(uint32_t i=0; i < unwrappedInfo.swapchainCount; i++)
		unwrappedSwaps.push_back(Unwrap(unwrappedInfo.pSwapchains[i]));

	unwrappedInfo.pSwapchains = &unwrappedSwaps.front();

	// Don't support any extensions for present info
	RDCASSERT(pPresentInfo->pNext == NULL);
	
	VkResourceRecord *swaprecord = GetRecord(pPresentInfo->pSwapchains[0]);
	RDCASSERT(swaprecord->swapInfo);

	SwapchainInfo &swapInfo = *swaprecord->swapInfo;

	bool activeWindow = RenderDoc::Inst().IsActiveWindow(LayerDisp(m_Instance), swapInfo.wndHandle);

	// need to record which image was last flipped so we can get the correct backbuffer
	// for a thumbnail in EndFrameCapture
	swapInfo.lastPresent = pPresentInfo->pImageIndices[0];
	m_LastSwap = swaprecord->GetResourceID();
	
	VkImage backbuffer = swapInfo.images[pPresentInfo->pImageIndices[0]].im;
	
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
			VkFramebuffer fb = swapInfo.images[pPresentInfo->pImageIndices[0]].fb;

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

			// VKTODOLOW once we have a more sophisticated way of re-using submitted command
			// buffers once they've executed and are safe to recycle, this can be removed
			FlushQ();
		}
	}

	VkResult vkr = ObjDisp(queue)->QueuePresentKHR(Unwrap(queue), &unwrappedInfo);

	if(!activeWindow)
		return vkr;
	
	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);

	// kill any current capture that isn't application defined
	if(m_State == WRITING_CAPFRAME && !m_AppControlledCapture)
		RenderDoc::Inst().EndFrameCapture(LayerDisp(m_Instance), swapInfo.wndHandle);

	if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE)
	{
		RenderDoc::Inst().StartFrameCapture(LayerDisp(m_Instance), swapInfo.wndHandle);

		m_AppControlledCapture = false;
	}

	return vkr;
}

// creation functions are in vk_<platform>.cpp

void WrappedVulkan::vkDestroySurfaceKHR(
		VkInstance                                   instance,
		VkSurfaceKHR                                 surface,
		const VkAllocationCallbacks*                 pAllocator)
{
	WrappedVkSurfaceKHR *wrapper = GetWrapped(surface);

	// record pointer has window handle packed in
	Keyboard::RemoveInputWindow((void *)wrapper->record);

	// now set record pointer back to NULL so no-one tries to delete it
	wrapper->record = NULL;

	GetResourceManager()->ReleaseWrappedResource(surface, true);
	ObjDisp(instance)->DestroySurfaceKHR(Unwrap(instance), wrapper->real.As<VkSurfaceKHR>(), pAllocator);
}
