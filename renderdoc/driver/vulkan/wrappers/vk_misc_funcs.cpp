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

// note, for threading reasons we ensure to release the wrappers before
// releasing the underlying object. Otherwise after releasing the vulkan object
// that same handle could be returned by create on another thread, and we
// could end up trying to re-wrap it.
#define DESTROY_IMPL(type, func) \
	void WrappedVulkan::vk ## func(VkDevice device, type obj) \
	{ \
		type unwrappedObj = Unwrap(obj); \
		if(GetResourceManager()->HasWrapper(ToTypedHandle(unwrappedObj))) GetResourceManager()->ReleaseWrappedResource(obj, true); \
		ObjDisp(device)->func(Unwrap(device), unwrappedObj); \
	}

DESTROY_IMPL(VkBuffer, DestroyBuffer)
DESTROY_IMPL(VkBufferView, DestroyBufferView)
DESTROY_IMPL(VkImageView, DestroyImageView)
DESTROY_IMPL(VkShader, DestroyShader)
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
DESTROY_IMPL(VkCmdPool, DestroyCommandPool)
DESTROY_IMPL(VkQueryPool, DestroyQueryPool)
DESTROY_IMPL(VkFramebuffer, DestroyFramebuffer)
DESTROY_IMPL(VkRenderPass, DestroyRenderPass)

#undef DESTROY_IMPL

// needs to be separate because it returns VkResult still
VkResult WrappedVulkan::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR obj)
{
	// release internal rendering objects we created for rendering the overlay
	{
		SwapchainInfo &info = *GetRecord(obj)->swapInfo;

		RenderDoc::Inst().RemoveFrameCapturer(LayerDisp(m_Instance), info.wndHandle);

		VkRenderPass unwrappedRP = Unwrap(info.rp);
		GetResourceManager()->ReleaseWrappedResource(info.rp, true);
		ObjDisp(device)->DestroyRenderPass(Unwrap(device), unwrappedRP);

		for(size_t i=0; i < info.images.size(); i++)
		{
			VkFramebuffer unwrappedFB = Unwrap(info.images[i].fb);
			VkImageView unwrappedView = Unwrap(info.images[i].view);
			GetResourceManager()->ReleaseWrappedResource(info.images[i].fb, true);
			// note, image doesn't have to be destroyed, just untracked
			GetResourceManager()->ReleaseWrappedResource(info.images[i].im, true);
			GetResourceManager()->ReleaseWrappedResource(info.images[i].view, true);
			ObjDisp(device)->DestroyFramebuffer(Unwrap(device), unwrappedFB);
			ObjDisp(device)->DestroyImageView(Unwrap(device), unwrappedView);
		}
	}

	VkSwapchainKHR unwrappedObj = Unwrap(obj);
	if(GetResourceManager()->HasWrapper(ToTypedHandle(unwrappedObj))) GetResourceManager()->ReleaseWrappedResource(obj, true);
	return ObjDisp(device)->DestroySwapchainKHR(Unwrap(device), unwrappedObj);
}

// needs to be separate so we don't erase from m_ImageLayouts in other destroy functions
void WrappedVulkan::vkDestroyImage(VkDevice device, VkImage obj)
{
	m_ImageLayouts.erase(GetResID(obj));
	VkImage unwrappedObj = Unwrap(obj);
	if(GetResourceManager()->HasWrapper(ToTypedHandle(unwrappedObj))) GetResourceManager()->ReleaseWrappedResource(obj, true);
	return ObjDisp(device)->DestroyImage(Unwrap(device), unwrappedObj);
}

// needs to be separate since it's dispatchable
void WrappedVulkan::vkDestroyCommandBuffer(VkDevice device, VkCmdBuffer obj)
{
	WrappedVkDispRes *wrapped = (WrappedVkDispRes *)GetWrapped(obj);

	if(wrapped->record)
	{
		if(wrapped->record->bakedCommands)
		{
			wrapped->record->bakedCommands->Delete(GetResourceManager());
			wrapped->record->bakedCommands = NULL;
		}
		wrapped->record->Delete(GetResourceManager());
		wrapped->record = NULL;
	}

	VkCmdBuffer unwrapped = wrapped->real.As<VkCmdBuffer>();
	
	GetResourceManager()->ReleaseWrappedResource(obj);

	ObjDisp(device)->DestroyCommandBuffer(Unwrap(device), unwrapped);
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
		case eResSwapchain:
			if(m_State >= WRITING)
				RDCERR("Swapchain object is leaking");
			else
				RDCERR("Should be no swapchain objects created on replay");
			break;

		case eResUnknown:
			RDCERR("Unknown resource type!");
			break;
			
		case eResCmdBuffer:
			// special case here, on replay we don't have the tracking
			// to remove these with the parent object so do it here.
			// This ensures we clean up after ourselves with a well-
			// behaved application.
			if(m_State < WRITING)
				GetResourceManager()->ReleaseWrappedResource((VkCmdBuffer)res);
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
			vt->FreeMemory(Unwrap(dev), nondisp->real.As<VkDeviceMemory>());
			GetResourceManager()->ReleaseWrappedResource(VkDeviceMemory(handle));
			break;
		case eResBuffer:
			vt->DestroyBuffer(Unwrap(dev), nondisp->real.As<VkBuffer>());
			GetResourceManager()->ReleaseWrappedResource(VkBuffer(handle));
			break;
		case eResBufferView:
			vt->DestroyBufferView(Unwrap(dev), nondisp->real.As<VkBufferView>());
			GetResourceManager()->ReleaseWrappedResource(VkBufferView(handle));
			break;
		case eResImage:
			vt->DestroyImage(Unwrap(dev), nondisp->real.As<VkImage>());
			GetResourceManager()->ReleaseWrappedResource(VkImage(handle));
			break;
		case eResImageView:
			vt->DestroyImageView(Unwrap(dev), nondisp->real.As<VkImageView>());
			GetResourceManager()->ReleaseWrappedResource(VkImageView(handle));
			break;
		case eResFramebuffer:
			vt->DestroyFramebuffer(Unwrap(dev), nondisp->real.As<VkFramebuffer>());
			GetResourceManager()->ReleaseWrappedResource(VkFramebuffer(handle));
			break;
		case eResRenderPass:
			vt->DestroyRenderPass(Unwrap(dev), nondisp->real.As<VkRenderPass>());
			GetResourceManager()->ReleaseWrappedResource(VkRenderPass(handle));
			break;
		case eResShaderModule:
			vt->DestroyShaderModule(Unwrap(dev), nondisp->real.As<VkShaderModule>());
			GetResourceManager()->ReleaseWrappedResource(VkShaderModule(handle));
			break;
		case eResShader:
			vt->DestroyShader(Unwrap(dev), nondisp->real.As<VkShader>());
			GetResourceManager()->ReleaseWrappedResource(VkShader(handle));
			break;
		case eResPipelineCache:
			vt->DestroyPipelineCache(Unwrap(dev), nondisp->real.As<VkPipelineCache>());
			GetResourceManager()->ReleaseWrappedResource(VkPipelineCache(handle));
			break;
		case eResPipelineLayout:
			vt->DestroyPipelineLayout(Unwrap(dev), nondisp->real.As<VkPipelineLayout>());
			GetResourceManager()->ReleaseWrappedResource(VkPipelineLayout(handle));
			break;
		case eResPipeline:
			vt->DestroyPipeline(Unwrap(dev), nondisp->real.As<VkPipeline>());
			GetResourceManager()->ReleaseWrappedResource(VkPipeline(handle));
			break;
		case eResSampler:
			vt->DestroySampler(Unwrap(dev), nondisp->real.As<VkSampler>());
			GetResourceManager()->ReleaseWrappedResource(VkSampler(handle));
			break;
		case eResDescriptorPool:
			vt->DestroyDescriptorPool(Unwrap(dev), nondisp->real.As<VkDescriptorPool>());
			GetResourceManager()->ReleaseWrappedResource(VkDescriptorPool(handle));
			break;
		case eResDescriptorSetLayout:
			vt->DestroyDescriptorSetLayout(Unwrap(dev), nondisp->real.As<VkDescriptorSetLayout>());
			GetResourceManager()->ReleaseWrappedResource(VkDescriptorSetLayout(handle));
			break;
		case eResCmdPool:
			vt->DestroyCommandPool(Unwrap(dev), nondisp->real.As<VkCmdPool>());
			GetResourceManager()->ReleaseWrappedResource(VkCmdPool(handle));
			break;
		case eResFence:
			vt->DestroyFence(Unwrap(dev), nondisp->real.As<VkFence>());
			GetResourceManager()->ReleaseWrappedResource(VkFence(handle));
			break;
		case eResEvent:
			vt->DestroyEvent(Unwrap(dev), nondisp->real.As<VkEvent>());
			GetResourceManager()->ReleaseWrappedResource(VkEvent(handle));
			break;
		case eResQueryPool:
			vt->DestroyQueryPool(Unwrap(dev), nondisp->real.As<VkQueryPool>());
			GetResourceManager()->ReleaseWrappedResource(VkQueryPool(handle));
			break;
		case eResSemaphore:
			vt->DestroySemaphore(Unwrap(dev), nondisp->real.As<VkSemaphore>());
			GetResourceManager()->ReleaseWrappedResource(VkSemaphore(handle));
			break;
	}

	return true;
}

// Sampler functions

bool WrappedVulkan::Serialise_vkCreateSampler(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSamplerCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSampler));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkSampler samp = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), &info, &samp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), samp);
			GetResourceManager()->AddLiveResource(id, samp);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSampler(
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler)
{
	VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), pCreateInfo, pSampler);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSampler);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLER);
				Serialise_vkCreateSampler(localSerialiser, device, pCreateInfo, pSampler);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSampler);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSampler);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateFramebuffer(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkFramebufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pFramebuffer));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkFramebuffer fb = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &info, &fb);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), fb);
			GetResourceManager()->AddLiveResource(id, fb);
		
			m_CreationInfo.m_Framebuffer[live].Init(GetResourceManager(), &info);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	VkImageView *unwrapped = GetTempArray<VkImageView>(pCreateInfo->attachmentCount);
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
		unwrapped[i] = Unwrap(pCreateInfo->pAttachments[i]);

	VkFramebufferCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
	unwrappedInfo.pAttachments = unwrapped;

	VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &unwrappedInfo, pFramebuffer);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFramebuffer);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_FRAMEBUFFER);
				Serialise_vkCreateFramebuffer(localSerialiser, device, pCreateInfo, pFramebuffer);

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
		
			m_CreationInfo.m_Framebuffer[id].Init(GetResourceManager(), &unwrappedInfo);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateRenderPass(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
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
		rpinfo.Init(GetResourceManager(), &info);

		// we want to store off the data so we can display it after the pass.
		// override any user-specified DONT_CARE.
		VkAttachmentDescription *att = (VkAttachmentDescription *)info.pAttachments;
		for(uint32_t i=0; i < info.attachmentCount; i++)
		{
			att[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			att[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		}

		VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, &rp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), rp);
			GetResourceManager()->AddLiveResource(id, rp);

			// make a version of the render pass that loads from its attachments,
			// so it can be used for replaying a single draw after a render pass
			// without doing a clear or a DONT_CARE load.
			for(uint32_t i=0; i < info.attachmentCount; i++)
			{
				att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			}

			ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, &rpinfo.loadRP);
			RDCASSERT(ret == VK_SUCCESS);

			ResourceId loadRPid = GetResourceManager()->WrapResource(Unwrap(device), rpinfo.loadRP);
			
			// register as a live-only resource, so it is cleaned up properly
			GetResourceManager()->AddLiveResource(loadRPid, rpinfo.loadRP);
		
			m_CreationInfo.m_RenderPass[live] = rpinfo;
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass)
{
	VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), pCreateInfo, pRenderPass);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pRenderPass);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_RENDERPASS);
				Serialise_vkCreateRenderPass(localSerialiser, device, pCreateInfo, pRenderPass);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pRenderPass);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pRenderPass);
			
			VulkanCreationInfo::RenderPass rpinfo;
			rpinfo.Init(GetResourceManager(), pCreateInfo);

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

			ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, &rpinfo.loadRP);
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
		VkQueryPool*                                pQueryPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkQueryPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pQueryPool));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkQueryPool pool = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateQueryPool(Unwrap(device), &info, &pool);

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
		VkQueryPool*                                pQueryPool)
{
	VkResult ret = ObjDisp(device)->CreateQueryPool(Unwrap(device), pCreateInfo, pQueryPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueryPool);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_QUERY_POOL);
				Serialise_vkCreateQueryPool(localSerialiser, device, pCreateInfo, pQueryPool);

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
		uint32_t                                    startQuery,
		uint32_t                                    queryCount,
		size_t*                                     pDataSize,
		void*                                       pData,
		VkQueryResultFlags                          flags)
{
	return ObjDisp(device)->GetQueryPoolResults(Unwrap(device), Unwrap(queryPool), startQuery, queryCount, pDataSize, pData, flags);
}

VkResult WrappedVulkan::vkDbgCreateMsgCallback(
	VkInstance                          instance,
	VkFlags                             msgFlags,
	const PFN_vkDbgMsgCallback          pfnMsgCallback,
	void*                               pUserData,
	VkDbgMsgCallback*                   pMsgCallback)
{
	// VKTODOLOW intercept this and point to our own callback
	return ObjDisp(instance)->DbgCreateMsgCallback(Unwrap(instance), msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
}

VkResult WrappedVulkan::vkDbgDestroyMsgCallback(
	VkInstance                          instance,
	VkDbgMsgCallback                    msgCallback)
{
	return ObjDisp(instance)->DbgDestroyMsgCallback(Unwrap(instance), msgCallback);
}

