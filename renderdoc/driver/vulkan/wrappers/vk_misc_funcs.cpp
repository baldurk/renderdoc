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
		if(m_ImageInfo.find(GetResID(obj)) != m_ImageInfo.end()) m_ImageInfo.erase(GetResID(obj)); \
		type unwrappedObj = Unwrap(obj); \
		if(GetResourceManager()->HasWrapper(ToTypedHandle(unwrappedObj))) GetResourceManager()->ReleaseWrappedResource(obj, true); \
		ObjDisp(device)->func(Unwrap(device), unwrappedObj); \
	}

DESTROY_IMPL(VkBuffer, DestroyBuffer)
DESTROY_IMPL(VkBufferView, DestroyBufferView)
DESTROY_IMPL(VkImage, DestroyImage)
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
	if(m_ImageInfo.find(GetResID(obj)) != m_ImageInfo.end()) m_ImageInfo.erase(GetResID(obj));
	VkSwapchainKHR unwrappedObj = Unwrap(obj);
	if(GetResourceManager()->HasWrapper(ToTypedHandle(unwrappedObj))) GetResourceManager()->ReleaseWrappedResource(obj, true);
	return ObjDisp(device)->DestroySwapchainKHR(Unwrap(device), unwrappedObj);
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

	// VKTODOHIGH: Device-associated resources must be released before the device is
	// shutdown. This needs a rethink while writing - really everything should be cleaned
	// up explicitly by us or the app.
	if(m_State >= WRITING) return true;

	// VKTODOHIGH: release resource with device from resource record

	// VKTODOLOW - this will break if we have multiple devices and resources from each,
	// but that will likely break other things too.
	VkDevice dev = GetDev();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
	WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;

	switch(IdentifyTypeByPtr(res))
	{
		case eResSwapchain:
			RDCERR("Should be no swapchain objects created on replay");
			break;

		case eResUnknown:
			RDCERR("Unknown resource type!");
			break;

		case eResPhysicalDevice:
		case eResQueue:
		case eResDescriptorSet:
		case eResCmdBuffer:
			// nothing to do - destroyed with parent object
			break;
			
		// VKTODOLOW shut down order needs examining, in future need to figure
		// out when/how to shut these down
		case eResInstance:
		case eResDevice:
			break;
		/*
		case eResInstance:
		{
			VkInstance instance = disp->real.As<VkInstance>();
			((WrappedVkInstance::DispatchTableType *)disp->table)->DestroyInstance(instance);
			break;
		}
		case eResDevice:
			//vt->DestroyDevice(disp->real.As<VkDevice>());
			break;
		*/
		case eResDeviceMemory:
			vt->FreeMemory(Unwrap(dev), nondisp->real.As<VkDeviceMemory>());
			break;
		case eResBuffer:
			vt->DestroyBuffer(Unwrap(dev), nondisp->real.As<VkBuffer>());
			break;
		case eResBufferView:
			vt->DestroyBufferView(Unwrap(dev), nondisp->real.As<VkBufferView>());
			break;
		case eResImage:
			vt->DestroyImage(Unwrap(dev), nondisp->real.As<VkImage>());
			break;
		case eResImageView:
			vt->DestroyImageView(Unwrap(dev), nondisp->real.As<VkImageView>());
			break;
		case eResFramebuffer:
			vt->DestroyFramebuffer(Unwrap(dev), nondisp->real.As<VkFramebuffer>());
			break;
		case eResRenderPass:
			vt->DestroyRenderPass(Unwrap(dev), nondisp->real.As<VkRenderPass>());
			break;
		case eResShaderModule:
			vt->DestroyShaderModule(Unwrap(dev), nondisp->real.As<VkShaderModule>());
			break;
		case eResShader:
			vt->DestroyShader(Unwrap(dev), nondisp->real.As<VkShader>());
			break;
		case eResPipelineCache:
			vt->DestroyPipelineCache(Unwrap(dev), nondisp->real.As<VkPipelineCache>());
			break;
		case eResPipelineLayout:
			vt->DestroyPipelineLayout(Unwrap(dev), nondisp->real.As<VkPipelineLayout>());
			break;
		case eResPipeline:
			vt->DestroyPipeline(Unwrap(dev), nondisp->real.As<VkPipeline>());
			break;
		case eResSampler:
			vt->DestroySampler(Unwrap(dev), nondisp->real.As<VkSampler>());
			break;
		case eResDescriptorPool:
			vt->DestroyDescriptorPool(Unwrap(dev), nondisp->real.As<VkDescriptorPool>());
			break;
		case eResDescriptorSetLayout:
			vt->DestroyDescriptorSetLayout(Unwrap(dev), nondisp->real.As<VkDescriptorSetLayout>());
			break;
		case eResCmdPool:
			vt->DestroyCommandPool(Unwrap(dev), nondisp->real.As<VkCmdPool>());
			break;
		case eResFence:
			vt->DestroyFence(Unwrap(dev), nondisp->real.As<VkFence>());
			break;
		case eResEvent:
			vt->DestroyEvent(Unwrap(dev), nondisp->real.As<VkEvent>());
			break;
		case eResQueryPool:
			vt->DestroyQueryPool(Unwrap(dev), nondisp->real.As<VkQueryPool>());
			break;
		case eResSemaphore:
			vt->DestroySemaphore(Unwrap(dev), nondisp->real.As<VkSemaphore>());
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
		
		// use original ID
		m_CreationInfo.m_Framebuffer[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &info, &fb);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), fb);
			GetResourceManager()->AddLiveResource(id, fb);
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

			if(pCreateInfo->renderPass != VK_NULL_HANDLE)
				record->AddParent(GetRecord(pCreateInfo->renderPass));
			for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
			{
				record->AddParent(GetRecord(pCreateInfo->pAttachments[i]));
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pFramebuffer);
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
		
		// use original ID
		m_CreationInfo.m_RenderPass[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, &rp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), rp);
			GetResourceManager()->AddLiveResource(id, rp);
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
	return ObjDisp(instance)->DbgCreateMsgCallback(Unwrap(instance), msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
}

VkResult WrappedVulkan::vkDbgDestroyMsgCallback(
	VkInstance                          instance,
	VkDbgMsgCallback                    msgCallback)
{
	return ObjDisp(instance)->DbgDestroyMsgCallback(Unwrap(instance), msgCallback);
}

