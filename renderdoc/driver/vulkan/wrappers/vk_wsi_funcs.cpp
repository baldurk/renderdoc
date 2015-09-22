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

struct xcb_connection_t;

// bit of a hack
namespace Keyboard { void UseConnection(xcb_connection_t *conn); }

///////////////////////////////////////////////////////////////////////////////////////
// WSI extension

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceSupportWSI(
		VkPhysicalDevice                        physicalDevice,
		uint32_t                                queueFamilyIndex,
		const VkSurfaceDescriptionWSI*          pSurfaceDescription,
		VkBool32*                               pSupported)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfaceSupportWSI(Unwrap(physicalDevice), queueFamilyIndex, pSurfaceDescription, pSupported);
}

VkResult WrappedVulkan::vkGetSurfaceInfoWSI(
		VkDevice                                 device,
		const VkSurfaceDescriptionWSI*           pSurfaceDescription,
		VkSurfaceInfoTypeWSI                     infoType,
		size_t*                                  pDataSize,
		void*                                    pData)
{
	return ObjDisp(device)->GetSurfaceInfoWSI(Unwrap(device), pSurfaceDescription, infoType, pDataSize, pData);
}

bool WrappedVulkan::Serialise_vkGetSwapChainInfoWSI(
		VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    VkSwapChainInfoTypeWSI                   infoType,
    size_t*                                  pDataSize,
    void*                                    pData)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, swapId, GetResID(swapChain));
	VkSwapChainImagePropertiesWSI *image = (VkSwapChainImagePropertiesWSI *)pData;
	SERIALISE_ELEMENT(size_t, idx, *pDataSize);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(image->image));

	if(m_State >= WRITING)
	{
		RDCASSERT(infoType == VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI);
	}

	if(m_State == READING)
	{
		// VKTODOLOW what if num images is less than on capture?
		RDCASSERT(idx < m_SwapChainInfo[swapId].images.size());
		GetResourceManager()->AddLiveResource(id, m_SwapChainInfo[swapId].images[idx].im);
	}

	return true;
}

VkResult WrappedVulkan::vkGetSwapChainInfoWSI(
		VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    VkSwapChainInfoTypeWSI                   infoType,
    size_t*                                  pDataSize,
    void*                                    pData)
{
	// make sure we always get the size
	size_t dummySize = 0;
	if(pDataSize == NULL)
		pDataSize = &dummySize;

	VkResult ret = ObjDisp(device)->GetSwapChainInfoWSI(Unwrap(device), Unwrap(swapChain), infoType, pDataSize, pData);

	if(infoType == VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI && pData && m_State >= WRITING)
	{
		VkSwapChainImagePropertiesWSI *images = (VkSwapChainImagePropertiesWSI *)pData;
		size_t numImages = (*pDataSize)/sizeof(VkSwapChainImagePropertiesWSI);

		for(size_t i=0; i < numImages; i++)
		{
			// these were all wrapped and serialised on swapchain create - we just have to
			// return the wrapped image in that case
			if(GetResourceManager()->HasWrapper(ToTypedHandle(images[i].image)))
			{
				images[i].image = (VkImage)(uint64_t)GetResourceManager()->GetWrapper(ToTypedHandle(images[i].image));
			}
			else
			{
				ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), images[i].image);

				if(m_State >= WRITING)
				{
					Chunk *chunk = NULL;

					{
						SCOPED_SERIALISE_CONTEXT(PRESENT_IMAGE);
						Serialise_vkGetSwapChainInfoWSI(device, swapChain, infoType, &i, (void *)&images[i]);

						chunk = scope.Get();
					}

					VkResourceRecord *record = GetResourceManager()->AddResourceRecord(images[i].image);
					record->AddChunk(chunk);

					// we invert the usual scheme - we make the swapchain record take parent refs
					// on these images, so that we can just ref the swapchain on present and pull
					// in all the images
					VkResourceRecord *swaprecord = GetRecord(swapChain);

					swaprecord->AddParent(record);
					// decrement refcount on swap images, so that they are only ref'd from the swapchain
					// (and will be deleted when it is deleted)
					record->Delete(GetResourceManager());
				}
				else
				{
					GetResourceManager()->AddLiveResource(id, images[i].image);
				}
			}
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkAcquireNextImageWSI(
		VkDevice                                 device,
		VkSwapChainWSI                           swapChain,
		uint64_t                                 timeout,
		VkSemaphore                              semaphore,
		uint32_t*                                pImageIndex)
{
	// VKTODOLOW: does this need to be intercepted/serialised?
	return ObjDisp(device)->AcquireNextImageWSI(Unwrap(device), Unwrap(swapChain), timeout, Unwrap(semaphore), pImageIndex);
}

bool WrappedVulkan::Serialise_vkCreateSwapChainWSI(
		VkDevice                                device,
		const VkSwapChainCreateInfoWSI*         pCreateInfo,
		VkSwapChainWSI*                         pSwapChain)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSwapChainCreateInfoWSI, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapChain));

	if(pCreateInfo && pCreateInfo->pSurfaceDescription)
	{
		VkSurfaceDescriptionWindowWSI *surf = (VkSurfaceDescriptionWindowWSI*)pCreateInfo->pSurfaceDescription;

		if(surf->platform == VK_PLATFORM_XCB_WSI)
		{
			VkPlatformHandleXcbWSI *handle = (VkPlatformHandleXcbWSI *)surf->pPlatformHandle;
			Keyboard::UseConnection(handle->connection);
		}
	}

	uint32_t numIms = 0;

	if(m_State >= WRITING)
	{
		VkResult vkr = VK_SUCCESS;

		size_t swapChainImagesSize;
		vkr = ObjDisp(device)->GetSwapChainInfoWSI(Unwrap(device), Unwrap(*pSwapChain), VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		numIms = uint32_t(swapChainImagesSize/sizeof(VkSwapChainImagePropertiesWSI));
	}

	SERIALISE_ELEMENT(uint32_t, numSwapImages, numIms);

	m_SwapChainInfo[id].format = info.imageFormat;
	m_SwapChainInfo[id].extent = info.imageExtent;
	m_SwapChainInfo[id].arraySize = info.imageArraySize;

	m_SwapChainInfo[id].images.resize(numSwapImages);

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		const VkImageCreateInfo imInfo = {
			/*.sType =*/ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			/*.pNext =*/ NULL,
			/*.imageType =*/ VK_IMAGE_TYPE_2D,
			/*.format =*/ info.imageFormat,
			/*.extent =*/ { info.imageExtent.width, info.imageExtent.height, 1 },
			/*.mipLevels =*/ 1,
			/*.arraySize =*/ info.imageArraySize,
			/*.samples =*/ 1,
			/*.tiling =*/ VK_IMAGE_TILING_OPTIMAL,
			/*.usage =*/
			VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|
			VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
			VK_IMAGE_USAGE_SAMPLED_BIT,
			/*.flags =*/ 0,
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

			m_SwapChainInfo[id].images[i].mem = mem;
			m_SwapChainInfo[id].images[i].im = im;

			// fill out image info so we track resource state transitions
			m_ImageInfo[liveId].mem = mem;
			m_ImageInfo[liveId].type = VK_IMAGE_TYPE_2D;
			m_ImageInfo[liveId].format = info.imageFormat;
			m_ImageInfo[liveId].extent.width = info.imageExtent.width;
			m_ImageInfo[liveId].extent.height = info.imageExtent.height;
			m_ImageInfo[liveId].extent.depth = 1;
			m_ImageInfo[liveId].mipLevels = 1;
			m_ImageInfo[liveId].arraySize = info.imageArraySize;

			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArraySlice = 0;
			range.mipLevels = 1;
			range.arraySize = info.imageArraySize;
			range.aspect = VK_IMAGE_ASPECT_COLOR;

			m_ImageInfo[liveId].subresourceStates.clear();
			m_ImageInfo[liveId].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSwapChainWSI(
		VkDevice                                device,
		const VkSwapChainCreateInfoWSI*         pCreateInfo,
		VkSwapChainWSI*                         pSwapChain)
{
	VkResult ret = ObjDisp(device)->CreateSwapChainWSI(Unwrap(device), pCreateInfo, pSwapChain);
	
	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSwapChain);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);
				Serialise_vkCreateSwapChainWSI(device, pCreateInfo, pSwapChain);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSwapChain);
			record->AddChunk(chunk);

			for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
			{
				if(m_PhysicalReplayData[i].dev == device)
					m_SwapPhysDevice = (int)i;
			}
			
			SwapInfo &swapInfo = m_SwapChainInfo[id];

			VkResult vkr = VK_SUCCESS;

			const VkLayerDispatchTable *vt = ObjDisp(device);

			{
				VkAttachmentDescription attDesc = {
					VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
					pCreateInfo->imageFormat, 1,
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

				vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, &swapInfo.rp);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), swapInfo.rp);
			}

			{
				VkViewport vp = { 0.0f, 0.0f, (float)pCreateInfo->imageExtent.width, (float)pCreateInfo->imageExtent.height, 0.0f, 1.0f, };
				VkRect2D sc = { { 0, 0 }, { pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height } };

				VkDynamicViewportStateCreateInfo vpInfo = {
					VK_STRUCTURE_TYPE_DYNAMIC_VIEWPORT_STATE_CREATE_INFO, NULL,
					1, &vp, &sc
				};

				vkr = vt->CreateDynamicViewportState(Unwrap(device), &vpInfo, &swapInfo.vp);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), swapInfo.vp);
			}

			// serialise out the swap chain images
			{
				size_t swapChainImagesSize;
				VkResult ret = vt->GetSwapChainInfoWSI(Unwrap(device), Unwrap(*pSwapChain), VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
				RDCASSERT(ret == VK_SUCCESS);

				uint32_t numSwapImages = uint32_t(swapChainImagesSize)/sizeof(VkSwapChainImagePropertiesWSI);

				VkSwapChainImagePropertiesWSI* images = new VkSwapChainImagePropertiesWSI[numSwapImages];

				// go through our own function so we assign these images IDs
				ret = vkGetSwapChainInfoWSI(device, *pSwapChain, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, images);
				RDCASSERT(ret == VK_SUCCESS);

				for(uint32_t i=0; i < numSwapImages; i++)
				{
					SwapInfo::SwapImage &swapImInfo = swapInfo.images[i];

					// memory doesn't exist for genuine WSI created images
					swapImInfo.mem = VK_NULL_HANDLE;
					swapImInfo.im = images[i].image;

					ResourceId imid = GetResID(images[i].image);

					// fill out image info so we track resource state transitions
					m_ImageInfo[imid].type = VK_IMAGE_TYPE_2D;
					m_ImageInfo[imid].format = pCreateInfo->imageFormat;
					m_ImageInfo[imid].extent.width = pCreateInfo->imageExtent.width;
					m_ImageInfo[imid].extent.height = pCreateInfo->imageExtent.height;
					m_ImageInfo[imid].extent.depth = 1;
					m_ImageInfo[imid].mipLevels = 1;
					m_ImageInfo[imid].arraySize = pCreateInfo->imageArraySize;

					VkImageSubresourceRange range;
					range.baseMipLevel = range.baseArraySlice = 0;
					range.mipLevels = 1;
					range.arraySize = pCreateInfo->imageArraySize;
					range.aspect = VK_IMAGE_ASPECT_COLOR;

					m_ImageInfo[imid].subresourceStates.clear();
					m_ImageInfo[imid].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));

					{
						VkAttachmentViewCreateInfo info = {
							VK_STRUCTURE_TYPE_ATTACHMENT_VIEW_CREATE_INFO, NULL,
							Unwrap(images[i].image), pCreateInfo->imageFormat, 0, 0, 1,
							0
						};

						vkr = vt->CreateAttachmentView(Unwrap(device), &info, &swapImInfo.view);
						RDCASSERT(vkr == VK_SUCCESS);

						GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.view);

						VkAttachmentBindInfo attBind = { Unwrap(swapImInfo.view), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

						VkFramebufferCreateInfo fbinfo = {
							VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
							Unwrap(swapInfo.rp),
							1, &attBind,
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

VkResult WrappedVulkan::vkQueuePresentWSI(
			VkQueue                                 queue,
			VkPresentInfoWSI*                       pPresentInfo)
{
	if(pPresentInfo->swapChainCount == 0)
		return VK_ERROR_INVALID_VALUE;

	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);
	
	if(m_State == WRITING_IDLE)
		RenderDoc::Inst().Tick();
	
	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame

	if(pPresentInfo->swapChainCount > 1 && (m_FrameCounter % 100) == 0)
	{
		RDCWARN("Presenting multiple swapchains at once - only first will be processed");
	}
	
	// VKTODOLOW handle present info pNext
	RDCASSERT(pPresentInfo->pNext == NULL);
	
	ResourceId swapid = GetResID(pPresentInfo->swapChains[0]);

	const SwapInfo &swapInfo = m_SwapChainInfo[swapid];

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
			VkDynamicViewportState vp = swapInfo.vp;
			VkFramebuffer fb = swapInfo.images[pPresentInfo->imageIndices[0]].fb;

			// VKTODOLOW only handling queue == GetQ()
			RDCASSERT(GetQ() == queue);
			VkQueue q = GetQ();

			VkLayerDispatchTable *vt = ObjDisp(GetDev());

			vt->QueueWaitIdle(Unwrap(q));

			TextPrintState textstate = { q, GetCmd(), rp, fb, vp, swapInfo.extent.width, swapInfo.extent.height };

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
		}
	}
	
	// kill any current capture
	if(m_State == WRITING_CAPFRAME)
	{
		//if(HasSuccessfulCapture())
		{
			RDCLOG("Finished capture, Frame %u", m_FrameCounter);

			GetResourceManager()->MarkResourceFrameReferenced(swapid, eFrameRef_Read);

			EndCaptureFrame(backbuffer);
			FinishCapture();

			byte *thpixels = NULL;
			uint32_t thwidth = 0;
			uint32_t thheight = 0;

			// gather backbuffer screenshot
			const int32_t maxSize = 1024;

			// VKTODOLOW split this out properly into begin/end frame capture
			if(1)//if(wnd)
			{
				VkDevice dev = GetDev();
				VkQueue q = GetQ();
				VkCmdBuffer cmd = GetCmd();

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
					/*.sType =*/ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					/*.pNext =*/ NULL,
					/*.imageType =*/ VK_IMAGE_TYPE_2D,
					/*.format =*/ swapInfo.format,
					/*.extent =*/ { swapInfo.extent.width, swapInfo.extent.height, 1 },
					/*.mipLevels =*/ 1,
					/*.arraySize =*/ 1,
					/*.samples =*/ 1,
					/*.tiling =*/ VK_IMAGE_TILING_LINEAR,
					/*.usage =*/
					VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
					/*.flags =*/ 0,
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
				vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
				RDCASSERT(vkr == VK_SUCCESS);
				vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
				RDCASSERT(vkr == VK_SUCCESS);

				VkImageCopy cpy = {
					subr,	{ 0, 0, 0 },
					subr,	{ 0, 0, 0 },
					{ imInfo.extent.width, imInfo.extent.height, 1 },
				};

				// VKTODOLOW back buffer must be in this layout right?
				VkImageMemoryBarrier bbTrans = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0, VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
					0, 0, Unwrap(backbuffer),
					{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

				VkImageMemoryBarrier readTrans = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
					0, 0, readbackIm, // was never wrapped
					{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

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

				vkr = vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
				RDCASSERT(vkr == VK_SUCCESS);

				// wait queue idle
				vt->QueueWaitIdle(Unwrap(q));

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

				vkr = vt->UnmapMemory(Unwrap(dev), readbackMem);
				RDCASSERT(vkr == VK_SUCCESS);

				// delete all
				vkr = vt->DestroyImage(Unwrap(dev), readbackIm);
				RDCASSERT(vkr == VK_SUCCESS);
				vkr = vt->FreeMemory(Unwrap(dev), readbackMem);
				RDCASSERT(vkr == VK_SUCCESS);
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
				SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

				m_pFileSerialiser->Insert(scope.Get(true));
			}

			RDCDEBUG("Inserting Resource Serialisers");	

			GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);
			
			GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

			RDCDEBUG("Creating Capture Scope");	

			{
				SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

				Serialise_CaptureScope(0);

				m_pFileSerialiser->Insert(scope.Get(true));

				m_pFileSerialiser->Insert(m_HeaderChunk);
			}

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

			m_CurFileSize += m_pFileSerialiser->FlushToDisk();

			RenderDoc::Inst().SuccessfullyWrittenLog();

			SAFE_DELETE(m_pFileSerialiser);
			SAFE_DELETE(m_HeaderChunk);

			m_State = WRITING_IDLE;

			// delete cmd buffers now - had to keep them alive until after serialiser flush.
			for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
				m_CmdBufferRecords[i]->Delete(GetResourceManager());
			
			GetResourceManager()->MarkUnwrittenResources();

			GetResourceManager()->ClearReferencedResources();
		}
	}

	if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE && m_FrameRecord.empty())
	{
		m_State = WRITING_CAPFRAME;

		FetchFrameRecord record;
		record.frameInfo.frameNumber = m_FrameCounter+1;
		record.frameInfo.captureTime = Timing::GetUnixTimestamp();
		m_FrameRecord.push_back(record);

		GetResourceManager()->ClearReferencedResources();

		GetResourceManager()->MarkResourceFrameReferenced(m_InstanceRecord->GetResourceID(), eFrameRef_Read);
		GetResourceManager()->PrepareInitialContents();
		
		AttemptCapture();
		BeginCaptureFrame();

		RDCLOG("Starting capture, frame %u", m_FrameCounter);
	}

	vector<VkSwapChainWSI> unwrappedSwaps;
	
	VkPresentInfoWSI unwrappedInfo = *pPresentInfo;

	for(uint32_t i=0; i < unwrappedInfo.swapChainCount; i++)
		unwrappedSwaps.push_back(Unwrap(unwrappedInfo.swapChains[i]));

	unwrappedInfo.swapChains = &unwrappedSwaps.front();

	return ObjDisp(queue)->QueuePresentWSI(Unwrap(queue), &unwrappedInfo);
}
