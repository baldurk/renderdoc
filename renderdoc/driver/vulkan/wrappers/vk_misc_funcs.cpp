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

#include "../vk_core.h"

// note, for threading reasons we ensure to release the wrappers before
// releasing the underlying object. Otherwise after releasing the vulkan object
// that same handle could be returned by create on another thread, and we
// could end up trying to re-wrap it.
#define DESTROY_IMPL(type, func) \
	void WrappedVulkan::vk ## func(VkDevice device, type obj, const VkAllocationCallbacks* pAllocator) \
	{ \
		type unwrappedObj = Unwrap(obj); \
		GetResourceManager()->ReleaseWrappedResource(obj, true); \
		ObjDisp(device)->func(Unwrap(device), unwrappedObj, pAllocator); \
	}

DESTROY_IMPL(VkBuffer, DestroyBuffer)
DESTROY_IMPL(VkBufferView, DestroyBufferView)
DESTROY_IMPL(VkImageView, DestroyImageView)
DESTROY_IMPL(VkShaderModule, DestroyShaderModule)
DESTROY_IMPL(VkPipeline, DestroyPipeline)
DESTROY_IMPL(VkPipelineCache, DestroyPipelineCache)
DESTROY_IMPL(VkPipelineLayout, DestroyPipelineLayout)
DESTROY_IMPL(VkSampler, DestroySampler)
DESTROY_IMPL(VkDescriptorSetLayout, DestroyDescriptorSetLayout)
DESTROY_IMPL(VkDescriptorPool, DestroyDescriptorPool)
DESTROY_IMPL(VkSemaphore, DestroySemaphore)
DESTROY_IMPL(VkFence, DestroyFence)
DESTROY_IMPL(VkEvent, DestroyEvent)
DESTROY_IMPL(VkCommandPool, DestroyCommandPool)
DESTROY_IMPL(VkQueryPool, DestroyQueryPool)
DESTROY_IMPL(VkFramebuffer, DestroyFramebuffer)
DESTROY_IMPL(VkRenderPass, DestroyRenderPass)

#undef DESTROY_IMPL

// needs to be separate because it releases internal resources
void WrappedVulkan::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR obj, const VkAllocationCallbacks* pAllocator)
{
	// release internal rendering objects we created for rendering the overlay
	{
		SwapchainInfo &info = *GetRecord(obj)->swapInfo;

		RenderDoc::Inst().RemoveFrameCapturer(LayerDisp(m_Instance), info.wndHandle);

		VkRenderPass unwrappedRP = Unwrap(info.rp);
		GetResourceManager()->ReleaseWrappedResource(info.rp, true);
		ObjDisp(device)->DestroyRenderPass(Unwrap(device), unwrappedRP, NULL);

		for(size_t i=0; i < info.images.size(); i++)
		{
			VkFramebuffer unwrappedFB = Unwrap(info.images[i].fb);
			VkImageView unwrappedView = Unwrap(info.images[i].view);
			GetResourceManager()->ReleaseWrappedResource(info.images[i].fb, true);
			// note, image doesn't have to be destroyed, just untracked
			GetResourceManager()->ReleaseWrappedResource(info.images[i].im, true);
			GetResourceManager()->ReleaseWrappedResource(info.images[i].view, true);
			ObjDisp(device)->DestroyFramebuffer(Unwrap(device), unwrappedFB, NULL);
			ObjDisp(device)->DestroyImageView(Unwrap(device), unwrappedView, NULL);
		}
	}

	VkSwapchainKHR unwrappedObj = Unwrap(obj);
	GetResourceManager()->ReleaseWrappedResource(obj, true);
	ObjDisp(device)->DestroySwapchainKHR(Unwrap(device), unwrappedObj, pAllocator);
}

// needs to be separate so we don't erase from m_ImageLayouts in other destroy functions
void WrappedVulkan::vkDestroyImage(VkDevice device, VkImage obj, const VkAllocationCallbacks* pAllocator)
{
	m_ImageLayouts.erase(GetResID(obj));
	VkImage unwrappedObj = Unwrap(obj);
	GetResourceManager()->ReleaseWrappedResource(obj, true);
	return ObjDisp(device)->DestroyImage(Unwrap(device), unwrappedObj, pAllocator);
}

// needs to be separate since it's dispatchable
void WrappedVulkan::vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
	for(uint32_t c=0; c < commandBufferCount; c++)
	{
		WrappedVkDispRes *wrapped = (WrappedVkDispRes *)GetWrapped(pCommandBuffers[c]);

		VkCommandBuffer unwrapped = wrapped->real.As<VkCommandBuffer>();

		GetResourceManager()->ReleaseWrappedResource(pCommandBuffers[c]);

		ObjDisp(device)->FreeCommandBuffers(Unwrap(device), Unwrap(commandPool), 1, &unwrapped);
	}
}

bool WrappedVulkan::ReleaseResource(WrappedVkRes *res)
{
	if(res == NULL) return true;

	// MULTIDEVICE need to get the actual device that created this object
	VkDevice dev = GetDev();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;
	WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
	uint64_t handle = (uint64_t)nondisp;

	switch(IdentifyTypeByPtr(res))
	{
		case eResSurface:
		case eResSwapchain:
			if(m_State >= WRITING)
				RDCERR("Swapchain/swapchain object is leaking");
			else
				RDCERR("Should be no swapchain/surface objects created on replay");
			break;

		case eResUnknown:
			RDCERR("Unknown resource type!");
			break;
			
		case eResCommandBuffer:
			// special case here, on replay we don't have the tracking
			// to remove these with the parent object so do it here.
			// This ensures we clean up after ourselves with a well-
			// behaved application.
			if(m_State < WRITING)
				GetResourceManager()->ReleaseWrappedResource((VkCommandBuffer)res);
			break;
		case eResDescriptorSet:
			if(m_State < WRITING)
				GetResourceManager()->ReleaseWrappedResource(VkDescriptorSet(handle));
			break;
		case eResPhysicalDevice:
			if(m_State < WRITING)
				GetResourceManager()->ReleaseWrappedResource((VkPhysicalDevice)disp);
			break;
		case eResQueue:
			if(m_State < WRITING)
				GetResourceManager()->ReleaseWrappedResource((VkQueue)disp);
			break;
			
		case eResDevice:
			// these are explicitly released elsewhere, do not need to destroy
			// any API objects.
			// On replay though we do need to tidy up book-keeping for these.
			if(m_State < WRITING)
			{
				GetResourceManager()->ReleaseCurrentResource(disp->id);
				GetResourceManager()->RemoveWrapper(ToTypedHandle(disp->real.As<VkDevice>()));
			}
			break;
		case eResInstance:
			if(m_State < WRITING)
			{
				GetResourceManager()->ReleaseCurrentResource(disp->id);
				GetResourceManager()->RemoveWrapper(ToTypedHandle(disp->real.As<VkInstance>()));
			}
			break;

		case eResDeviceMemory:
		{
			VkDeviceMemory real = nondisp->real.As<VkDeviceMemory>();
			GetResourceManager()->ReleaseWrappedResource(VkDeviceMemory(handle));
			vt->FreeMemory(Unwrap(dev), real, NULL);
			break;
		}
		case eResBuffer:
		{
			VkBuffer real = nondisp->real.As<VkBuffer>();
			GetResourceManager()->ReleaseWrappedResource(VkBuffer(handle));
			vt->DestroyBuffer(Unwrap(dev), real, NULL);
			break;
		}
		case eResBufferView:
		{
			VkBufferView real = nondisp->real.As<VkBufferView>();
			GetResourceManager()->ReleaseWrappedResource(VkBufferView(handle));
			vt->DestroyBufferView(Unwrap(dev), real, NULL);
			break;
		}
		case eResImage:
		{
			VkImage real = nondisp->real.As<VkImage>();
			GetResourceManager()->ReleaseWrappedResource(VkImage(handle));
			vt->DestroyImage(Unwrap(dev), real, NULL);
			break;
		}
		case eResImageView:
		{
			VkImageView real = nondisp->real.As<VkImageView>();
			GetResourceManager()->ReleaseWrappedResource(VkImageView(handle));
			vt->DestroyImageView(Unwrap(dev), real, NULL);
			break;
		}
		case eResFramebuffer:
		{
			VkFramebuffer real = nondisp->real.As<VkFramebuffer>();
			GetResourceManager()->ReleaseWrappedResource(VkFramebuffer(handle));
			vt->DestroyFramebuffer(Unwrap(dev), real, NULL);
			break;
		}
		case eResRenderPass:
		{
			VkRenderPass real = nondisp->real.As<VkRenderPass>();
			GetResourceManager()->ReleaseWrappedResource(VkRenderPass(handle));
			vt->DestroyRenderPass(Unwrap(dev), real, NULL);
			break;
		}
		case eResShaderModule:
		{
			VkShaderModule real = nondisp->real.As<VkShaderModule>();
			GetResourceManager()->ReleaseWrappedResource(VkShaderModule(handle));
			vt->DestroyShaderModule(Unwrap(dev), real, NULL);
			break;
		}
		case eResPipelineCache:
		{
			VkPipelineCache real = nondisp->real.As<VkPipelineCache>();
			GetResourceManager()->ReleaseWrappedResource(VkPipelineCache(handle));
			vt->DestroyPipelineCache(Unwrap(dev), real, NULL);
			break;
		}
		case eResPipelineLayout:
		{
			VkPipelineLayout real = nondisp->real.As<VkPipelineLayout>();
			GetResourceManager()->ReleaseWrappedResource(VkPipelineLayout(handle));
			vt->DestroyPipelineLayout(Unwrap(dev), real, NULL);
			break;
		}
		case eResPipeline:
		{
			VkPipeline real = nondisp->real.As<VkPipeline>();
			GetResourceManager()->ReleaseWrappedResource(VkPipeline(handle));
			vt->DestroyPipeline(Unwrap(dev), real, NULL);
			break;
		}
		case eResSampler:
		{
			VkSampler real = nondisp->real.As<VkSampler>();
			GetResourceManager()->ReleaseWrappedResource(VkSampler(handle));
			vt->DestroySampler(Unwrap(dev), real, NULL);
			break;
		}
		case eResDescriptorPool:
		{
			VkDescriptorPool real = nondisp->real.As<VkDescriptorPool>();
			GetResourceManager()->ReleaseWrappedResource(VkDescriptorPool(handle));
			vt->DestroyDescriptorPool(Unwrap(dev), real, NULL);
			break;
		}
		case eResDescriptorSetLayout:
		{
			VkDescriptorSetLayout real = nondisp->real.As<VkDescriptorSetLayout>();
			GetResourceManager()->ReleaseWrappedResource(VkDescriptorSetLayout(handle));
			vt->DestroyDescriptorSetLayout(Unwrap(dev), real, NULL);
			break;
		}
		case eResCommandPool:
		{
			VkCommandPool real = nondisp->real.As<VkCommandPool>();
			GetResourceManager()->ReleaseWrappedResource(VkCommandPool(handle));
			vt->DestroyCommandPool(Unwrap(dev), real, NULL);
			break;
		}
		case eResFence:
		{
			VkFence real = nondisp->real.As<VkFence>();
			GetResourceManager()->ReleaseWrappedResource(VkFence(handle));
			vt->DestroyFence(Unwrap(dev), real, NULL);
			break;
		}
		case eResEvent:
		{
			VkEvent real = nondisp->real.As<VkEvent>();
			GetResourceManager()->ReleaseWrappedResource(VkEvent(handle));
			vt->DestroyEvent(Unwrap(dev), real, NULL);
			break;
		}
		case eResQueryPool:
		{
			VkQueryPool real = nondisp->real.As<VkQueryPool>();
			GetResourceManager()->ReleaseWrappedResource(VkQueryPool(handle));
			vt->DestroyQueryPool(Unwrap(dev), real, NULL);
			break;
		}
		case eResSemaphore:
		{
			VkSemaphore real = nondisp->real.As<VkSemaphore>();
			GetResourceManager()->ReleaseWrappedResource(VkSemaphore(handle));
			vt->DestroySemaphore(Unwrap(dev), real, NULL);
			break;
		}
	}

	return true;
}

// Sampler functions

bool WrappedVulkan::Serialise_vkCreateSampler(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSampler*                                  pSampler)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSamplerCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSampler));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkSampler samp = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), &info, NULL, &samp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live;

			if(GetResourceManager()->HasWrapper(ToTypedHandle(samp)))
			{
				live = GetResourceManager()->GetNonDispWrapper(samp)->id;

				// destroy this instance of the duplicate, as we must have matching create/destroy
				// calls and there won't be a wrapped resource hanging around to destroy this one.
				ObjDisp(device)->DestroySampler(Unwrap(device), samp, NULL);

				// whenever the new ID is requested, return the old ID, via replacements.
				GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
			}
			else
			{
				live = GetResourceManager()->WrapResource(Unwrap(device), samp);
				GetResourceManager()->AddLiveResource(id, samp);
			
				m_CreationInfo.m_Sampler[live].Init(GetResourceManager(), m_CreationInfo, &info);
			}
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSampler(
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSampler*                                  pSampler)
{
	VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), pCreateInfo, pAllocator, pSampler);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSampler);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLER);
				Serialise_vkCreateSampler(localSerialiser, device, pCreateInfo, NULL, pSampler);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSampler);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSampler);
		
			m_CreationInfo.m_Sampler[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateFramebuffer(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkFramebuffer*                              pFramebuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkFramebufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pFramebuffer));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkFramebuffer fb = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &info, NULL, &fb);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live;

			if(GetResourceManager()->HasWrapper(ToTypedHandle(fb)))
			{
				live = GetResourceManager()->GetNonDispWrapper(fb)->id;

				// destroy this instance of the duplicate, as we must have matching create/destroy
				// calls and there won't be a wrapped resource hanging around to destroy this one.
				ObjDisp(device)->DestroyFramebuffer(Unwrap(device), fb, NULL);

				// whenever the new ID is requested, return the old ID, via replacements.
				GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
			}
			else
			{
				live = GetResourceManager()->WrapResource(Unwrap(device), fb);
				GetResourceManager()->AddLiveResource(id, fb);
			
				m_CreationInfo.m_Framebuffer[live].Init(GetResourceManager(), m_CreationInfo, &info);
			}
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkFramebuffer*                              pFramebuffer)
{
	VkImageView *unwrapped = GetTempArray<VkImageView>(pCreateInfo->attachmentCount);
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
		unwrapped[i] = Unwrap(pCreateInfo->pAttachments[i]);

	VkFramebufferCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
	unwrappedInfo.pAttachments = unwrapped;

	VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &unwrappedInfo, pAllocator, pFramebuffer);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFramebuffer);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_FRAMEBUFFER);
				Serialise_vkCreateFramebuffer(localSerialiser, device, pCreateInfo, NULL, pFramebuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pFramebuffer);
			record->AddChunk(chunk);

			record->imageAttachments = new VkResourceRecord*[VkResourceRecord::MaxImageAttachments];
			RDCASSERT(pCreateInfo->attachmentCount < VkResourceRecord::MaxImageAttachments);

			RDCEraseMem(record->imageAttachments, sizeof(ResourceId)*VkResourceRecord::MaxImageAttachments);

			if(pCreateInfo->renderPass != VK_NULL_HANDLE)
				record->AddParent(GetRecord(pCreateInfo->renderPass));
			for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
			{
				VkResourceRecord *attRecord = GetRecord(pCreateInfo->pAttachments[i]);
				record->AddParent(attRecord);

				record->imageAttachments[i] = attRecord;
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pFramebuffer);
		
			m_CreationInfo.m_Framebuffer[id].Init(GetResourceManager(), m_CreationInfo, &unwrappedInfo);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateRenderPass(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkRenderPass*                               pRenderPass)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkRenderPassCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pRenderPass));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkRenderPass rp = VK_NULL_HANDLE;

		VulkanCreationInfo::RenderPass rpinfo;
		rpinfo.Init(GetResourceManager(), m_CreationInfo, &info);

		// we want to store off the data so we can display it after the pass.
		// override any user-specified DONT_CARE.
		VkAttachmentDescription *att = (VkAttachmentDescription *)info.pAttachments;
		for(uint32_t i=0; i < info.attachmentCount; i++)
		{
			att[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			att[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		}

		VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, NULL, &rp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live;

			if(GetResourceManager()->HasWrapper(ToTypedHandle(rp)))
			{
				live = GetResourceManager()->GetNonDispWrapper(rp)->id;

				// destroy this instance of the duplicate, as we must have matching create/destroy
				// calls and there won't be a wrapped resource hanging around to destroy this one.
				ObjDisp(device)->DestroyRenderPass(Unwrap(device), rp, NULL);

				// whenever the new ID is requested, return the old ID, via replacements.
				GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
			}
			else
			{
				live = GetResourceManager()->WrapResource(Unwrap(device), rp);
				GetResourceManager()->AddLiveResource(id, rp);

				// make a version of the render pass that loads from its attachments,
				// so it can be used for replaying a single draw after a render pass
				// without doing a clear or a DONT_CARE load.
				for(uint32_t i=0; i < info.attachmentCount; i++)
				{
					att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				}

				ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, NULL, &rpinfo.loadRP);
				RDCASSERT(ret == VK_SUCCESS);
				
				// handle the loadRP being a duplicate
				if(GetResourceManager()->HasWrapper(ToTypedHandle(rpinfo.loadRP)))
				{
					// just fetch the existing wrapped object
					rpinfo.loadRP = (VkRenderPass)(uint64_t)GetResourceManager()->GetNonDispWrapper(rpinfo.loadRP);

					// destroy this instance of the duplicate, as we must have matching create/destroy
					// calls and there won't be a wrapped resource hanging around to destroy this one.
					ObjDisp(device)->DestroyRenderPass(Unwrap(device), rpinfo.loadRP, NULL);

					// don't need to ReplaceResource as no IDs are involved
				}
				else
				{
					ResourceId loadRPid = GetResourceManager()->WrapResource(Unwrap(device), rpinfo.loadRP);

					// register as a live-only resource, so it is cleaned up properly
					GetResourceManager()->AddLiveResource(loadRPid, rpinfo.loadRP);
				}
				
				m_CreationInfo.m_RenderPass[live] = rpinfo;
			}
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkRenderPass*                               pRenderPass)
{
	VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), pCreateInfo, pAllocator, pRenderPass);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pRenderPass);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_RENDERPASS);
				Serialise_vkCreateRenderPass(localSerialiser, device, pCreateInfo, NULL, pRenderPass);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pRenderPass);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pRenderPass);
			
			VulkanCreationInfo::RenderPass rpinfo;
			rpinfo.Init(GetResourceManager(), m_CreationInfo, pCreateInfo);

			VkRenderPassCreateInfo info = *pCreateInfo;

			VkAttachmentDescription atts[16];
			RDCASSERT(ARRAY_COUNT(atts) >= (size_t)info.attachmentCount);

			// make a version of the render pass that loads from its attachments,
			// so it can be used for replaying a single draw after a render pass
			// without doing a clear or a DONT_CARE load.
			for(uint32_t i=0; i < info.attachmentCount; i++)
			{
				atts[i] = info.pAttachments[i];
				atts[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				atts[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			}
			
			info.pAttachments = atts;

			ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, NULL, &rpinfo.loadRP);
			RDCASSERT(ret == VK_SUCCESS);

			ResourceId loadRPid = GetResourceManager()->WrapResource(Unwrap(device), rpinfo.loadRP);
			
			// register as a live-only resource, so it is cleaned up properly
			GetResourceManager()->AddLiveResource(loadRPid, rpinfo.loadRP);

			m_CreationInfo.m_RenderPass[id] = rpinfo;
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateQueryPool(
		Serialiser*                                 localSerialiser,
		VkDevice                                    device,
		const VkQueryPoolCreateInfo*                pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkQueryPool*                                pQueryPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkQueryPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pQueryPool));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkQueryPool pool = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateQueryPool(Unwrap(device), &info, NULL, &pool);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
			GetResourceManager()->AddLiveResource(id, pool);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateQueryPool(
		VkDevice                                    device,
		const VkQueryPoolCreateInfo*                pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkQueryPool*                                pQueryPool)
{
	VkResult ret = ObjDisp(device)->CreateQueryPool(Unwrap(device), pCreateInfo, pAllocator, pQueryPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueryPool);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_QUERY_POOL);
				Serialise_vkCreateQueryPool(localSerialiser, device, pCreateInfo, NULL, pQueryPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pQueryPool);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pQueryPool);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkGetQueryPoolResults(
		VkDevice                                    device,
		VkQueryPool                                 queryPool,
		uint32_t                                    firstQuery,
		uint32_t                                    queryCount,
		size_t                                      dataSize,
		void*                                       pData,
		VkDeviceSize                                stride,
		VkQueryResultFlags                          flags)
{
	return ObjDisp(device)->GetQueryPoolResults(Unwrap(device), Unwrap(queryPool), firstQuery, queryCount, dataSize, pData, stride, flags);
}

VkResult WrappedVulkan::vkCreateDebugReportCallbackEXT(
		VkInstance                                  instance,
    const VkDebugReportCallbackCreateInfoEXT*   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDebugReportCallbackEXT*                   pCallback)
{
	// don't need to wrap this, as we will create our own independent callback
	return ObjDisp(instance)->CreateDebugReportCallbackEXT(Unwrap(instance), pCreateInfo, pAllocator, pCallback);
}

void WrappedVulkan::vkDestroyDebugReportCallbackEXT(
		VkInstance                                  instance,
		VkDebugReportCallbackEXT                    callback,
    const VkAllocationCallbacks*                pAllocator)
{
	return ObjDisp(instance)->DestroyDebugReportCallbackEXT(Unwrap(instance), callback, pAllocator);
}

void WrappedVulkan::vkDebugReportMessageEXT(
    VkInstance                                  instance,
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char*                                 pLayerPrefix,
    const char*                                 pMessage)
{
	return ObjDisp(instance)->DebugReportMessageEXT(Unwrap(instance), flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
}

VkResult WrappedVulkan::vkDbgSetObjectTag(
		VkDevice device,
		VkDebugReportObjectTypeEXT objType,
		uint64_t object,
		size_t tagSize,
		const void* pTag)
{
	if(ObjDisp(device)->DbgSetObjectTag)
		ObjDisp(device)->DbgSetObjectTag(device, objType, object, tagSize, pTag);

	// don't do anything with the tags

	return VK_SUCCESS;
}

static VkResourceRecord *GetObjRecord(VkDebugReportObjectTypeEXT objType, uint64_t object)
{
	switch(objType)
	{
		case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT:
			return GetRecord((VkInstance)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:
			return GetRecord((VkPhysicalDevice)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT:
			return GetRecord((VkDevice)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT:
			return GetRecord((VkQueue)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT:
			return GetRecord((VkCommandBuffer)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT:
			return GetRecord((VkDeviceMemory)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT:
			return GetRecord((VkBuffer)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT:
			return GetRecord((VkBufferView)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT:
			return GetRecord((VkImage)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT:
			return GetRecord((VkImageView)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT:
			return GetRecord((VkShaderModule)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT:
			return GetRecord((VkPipeline)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:
			return GetRecord((VkPipelineLayout)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT:
			return GetRecord((VkSampler)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT:
			return GetRecord((VkDescriptorSet)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT:
			return GetRecord((VkDescriptorSetLayout)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:
			return GetRecord((VkDescriptorPool)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT:
			return GetRecord((VkFence)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT:
			return GetRecord((VkSemaphore)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT:
			return GetRecord((VkEvent)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT:
			return GetRecord((VkQueryPool)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT:
			return GetRecord((VkFramebuffer)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT:
			return GetRecord((VkRenderPass)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT:
			return GetRecord((VkPipelineCache)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:
			return GetRecord((VkSurfaceKHR)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:
			return GetRecord((VkSwapchainKHR)object);
		case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT:
			return GetRecord((VkCommandPool)object);
	}
	return NULL;
}

bool WrappedVulkan::Serialise_vkDbgSetObjectName(
		Serialiser *localSerialiser,
		VkDevice device,
		VkDebugReportObjectTypeEXT objType,
		uint64_t object,
		size_t nameSize,
		const char* pName)
{
	SERIALISE_ELEMENT(ResourceId, id, GetObjRecord(objType, object)->GetResourceID());
	
	string name;
	if(m_State >= WRITING)
		name = string(pName, pName+nameSize);

	localSerialiser->Serialise("name", name);

	if(m_State == READING)
		m_CreationInfo.m_Names[GetResourceManager()->GetLiveID(id)] = name;

	return true;
}

VkResult WrappedVulkan::vkDbgSetObjectName(
		VkDevice device,
		VkDebugReportObjectTypeEXT objType,
		uint64_t object,
		size_t nameSize,
		const char* pName)
{
	if(ObjDisp(device)->DbgSetObjectName)
		ObjDisp(device)->DbgSetObjectName(device, objType, object, nameSize, pName);
	
	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;
		
		VkResourceRecord *record = GetObjRecord(objType, object);

		if(!record)
		{
			RDCERR("Unrecognised object %d %llu", objType, object);
			return VK_SUCCESS;
		}

		{
			CACHE_THREAD_SERIALISER();

			SCOPED_SERIALISE_CONTEXT(SET_NAME);
			Serialise_vkDbgSetObjectName(localSerialiser, device, objType, object, nameSize, pName);

			chunk = scope.Get();
		}

		record->AddChunk(chunk);
	}

	return VK_SUCCESS;
}
