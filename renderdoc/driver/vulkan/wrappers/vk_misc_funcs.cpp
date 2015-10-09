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
	VkResult WrappedVulkan::vk ## func(VkDevice device, type obj) \
	{ \
		if(m_ImageInfo.find(GetResID(obj)) != m_ImageInfo.end()) m_ImageInfo.erase(GetResID(obj)); \
		type unwrappedObj = Unwrap(obj); \
		if(GetResourceManager()->HasWrapper(ToTypedHandle(unwrappedObj))) GetResourceManager()->ReleaseWrappedResource(obj, true); \
		return ObjDisp(device)->func(Unwrap(device), unwrappedObj); \
	}

DESTROY_IMPL(VkBuffer, DestroyBuffer)
DESTROY_IMPL(VkBufferView, DestroyBufferView)
DESTROY_IMPL(VkImage, DestroyImage)
DESTROY_IMPL(VkImageView, DestroyImageView)
DESTROY_IMPL(VkAttachmentView, DestroyAttachmentView)
DESTROY_IMPL(VkShader, DestroyShader)
DESTROY_IMPL(VkShaderModule, DestroyShaderModule)
DESTROY_IMPL(VkPipeline, DestroyPipeline)
DESTROY_IMPL(VkPipelineCache, DestroyPipelineCache)
DESTROY_IMPL(VkPipelineLayout, DestroyPipelineLayout)
DESTROY_IMPL(VkSampler, DestroySampler)
DESTROY_IMPL(VkDescriptorSetLayout, DestroyDescriptorSetLayout)
DESTROY_IMPL(VkDescriptorPool, DestroyDescriptorPool)
DESTROY_IMPL(VkDynamicViewportState, DestroyDynamicViewportState)
DESTROY_IMPL(VkDynamicRasterState, DestroyDynamicRasterState)
DESTROY_IMPL(VkDynamicColorBlendState, DestroyDynamicColorBlendState)
DESTROY_IMPL(VkDynamicDepthStencilState, DestroyDynamicDepthStencilState)
DESTROY_IMPL(VkSemaphore, DestroySemaphore)
DESTROY_IMPL(VkFence, DestroyFence)
DESTROY_IMPL(VkCmdPool, DestroyCommandPool)
DESTROY_IMPL(VkQueryPool, DestroyQueryPool)
DESTROY_IMPL(VkFramebuffer, DestroyFramebuffer)
DESTROY_IMPL(VkRenderPass, DestroyRenderPass)
DESTROY_IMPL(VkSwapChainWSI, DestroySwapChainWSI)

#undef DESTROY_IMPL

// needs to be separate since it's dispatchable
VkResult WrappedVulkan::vkDestroyCommandBuffer(VkDevice device, VkCmdBuffer obj)
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
	VkResult ret = ObjDisp(device)->DestroyCommandBuffer(Unwrap(device), wrapped->real.As<VkCmdBuffer>());

	GetResourceManager()->ReleaseWrappedResource(obj);

	return ret;
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
		case eResWSISwapChain:
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
		case eResAttachmentView:
			vt->DestroyAttachmentView(Unwrap(dev), nondisp->real.As<VkAttachmentView>());
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
		case eResViewportState:
			vt->DestroyDynamicViewportState(Unwrap(dev), nondisp->real.As<VkDynamicViewportState>());
			break;
		case eResRasterState:
			vt->DestroyDynamicViewportState(Unwrap(dev), nondisp->real.As<VkDynamicViewportState>());
			break;
		case eResColorBlendState:
			vt->DestroyDynamicColorBlendState(Unwrap(dev), nondisp->real.As<VkDynamicColorBlendState>());
			break;
		case eResDepthStencilState:
			vt->DestroyDynamicDepthStencilState(Unwrap(dev), nondisp->real.As<VkDynamicDepthStencilState>());
			break;
		case eResCmdPool:
			vt->DestroyCommandPool(Unwrap(dev), nondisp->real.As<VkCmdPool>());
			break;
		case eResFence:
			vt->DestroyFence(Unwrap(dev), nondisp->real.As<VkFence>());
			break;
		case eResEvent:
			// VKTODOLOW
			break;
		case eResQueryPool:
			// VKTODOLOW
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

bool WrappedVulkan::Serialise_vkCreateSemaphore(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSemaphoreCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSemaphore));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkSemaphore sem = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), &info, &sem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			if(GetResourceManager()->HasWrapper(ToTypedHandle(sem)))
			{
				// VKTODOMED need to handle duplicate objects better than this, perhaps
				ResourceId live = GetResourceManager()->GetNonDispWrapper(sem)->id;
				RDCDEBUG("Doing hack for duplicate objects that replay expects to be distinct - %llu -> %llu", id, live);
				GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
			}
			else
			{
				ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sem);
				GetResourceManager()->AddLiveResource(id, sem);
			}
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore)
{
	VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), pCreateInfo, pSemaphore);

	if(ret == VK_SUCCESS)
	{
		if(GetResourceManager()->HasWrapper(ToTypedHandle(*pSemaphore)))
		{
			*pSemaphore = (VkSemaphore)(uint64_t)GetResourceManager()->GetWrapper(ToTypedHandle(*pSemaphore));
		}
		else
		{
			ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSemaphore);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					CACHE_THREAD_SERIALISER();

					SCOPED_SERIALISE_CONTEXT(CREATE_SEMAPHORE);
					Serialise_vkCreateSemaphore(localSerialiser, device, pCreateInfo, pSemaphore);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSemaphore);
				record->AddChunk(chunk);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, *pSemaphore);
			}
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

bool WrappedVulkan::Serialise_vkCreateFence(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkFenceCreateInfo*                pCreateInfo,
			VkFence*                                pFence)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkFenceCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pFence));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkFence sem = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), &info, &sem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sem);
			GetResourceManager()->AddLiveResource(id, sem);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateFence(
			VkDevice                                device,
			const VkFenceCreateInfo*                pCreateInfo,
			VkFence*                                pFence)
{
	VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), pCreateInfo, pFence);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFence);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();

				SCOPED_SERIALISE_CONTEXT(CREATE_FENCE);
				Serialise_vkCreateFence(localSerialiser, device, pCreateInfo, pFence);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pFence);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pFence);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkGetFenceStatus(
			Serialiser*                                 localSerialiser,
			VkDevice                                device,
			VkFence                                 fence)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, fid, GetResID(fence));
	
	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

		// VKTODOLOW conservatively assume we have to wait for the device to be idle
		// this could probably be smarter
		ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
	}

	return true;
}

VkResult WrappedVulkan::vkGetFenceStatus(
			VkDevice                                device,
			VkFence                                 fence)
{
	VkResult ret = ObjDisp(device)->GetFenceStatus(Unwrap(device), Unwrap(fence));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(GET_FENCE_STATUS);
		Serialise_vkGetFenceStatus(localSerialiser, device, fence);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

VkResult WrappedVulkan::vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	VkAttachmentBindInfo *unwrapped = GetTempArray<VkAttachmentBindInfo>(pCreateInfo->attachmentCount);
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
	{
		unwrapped[i] = pCreateInfo->pAttachments[i];
		unwrapped[i].view = Unwrap(unwrapped[i].view);
	}

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
				record->AddParent(GetRecord(pCreateInfo->pAttachments[i].view));
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

// State object functions

bool WrappedVulkan::Serialise_vkCreateDynamicViewportState(
			Serialiser*                                 localSerialiser,
			VkDevice                                         device,
			const VkDynamicViewportStateCreateInfo*          pCreateInfo,
			VkDynamicViewportState*                          pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicViewportStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicViewportState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_VPScissor[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicViewportState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicViewportState(
			VkDevice                                    device,
			const VkDynamicViewportStateCreateInfo*           pCreateInfo,
			VkDynamicViewportState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicViewportState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_VIEWPORT_STATE);
				Serialise_vkCreateDynamicViewportState(localSerialiser, device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicRasterState(
			Serialiser*                                 localSerialiser,
			VkDevice                                        device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicRasterStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicRasterState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_Raster[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicRasterState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicRasterState(
			VkDevice                                        device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicRasterState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE);
				Serialise_vkCreateDynamicRasterState(localSerialiser, device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicColorBlendState(
			Serialiser*                                 localSerialiser,
			VkDevice                                            device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicColorBlendStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicColorBlendState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_Blend[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicColorBlendState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicColorBlendState(
			VkDevice                                    device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicColorBlendState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_BLEND_STATE);
				Serialise_vkCreateDynamicColorBlendState(localSerialiser, device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicDepthStencilState(
			Serialiser*                                 localSerialiser,
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicDepthStencilStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicDepthStencilState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_DepthStencil[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicDepthStencilState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicDepthStencilState(
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicDepthStencilState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				CACHE_THREAD_SERIALISER();
		
				SCOPED_SERIALISE_CONTEXT(CREATE_DEPTH_STATE);
				Serialise_vkCreateDynamicDepthStencilState(localSerialiser, device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
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

