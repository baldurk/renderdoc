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

#define DESTROY_IMPL(type, func) \
	VkResult WrappedVulkan::vk ## func(VkDevice device, type obj) \
	{ \
		if(m_ImageInfo.find(GetResID(obj)) != m_ImageInfo.end()) m_ImageInfo.erase(GetResID(obj)); \
		VkResult ret = ObjDisp(device)->func(Unwrap(device), Unwrap(obj)); \
		GetResourceManager()->ReleaseWrappedResource(obj); \
		return ret; \
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
DESTROY_IMPL(VkCmdPool, DestroyCommandPool)
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
			// VKTODOLOW
			//vt->DestroyFence(Unwrap(dev), nondisp->real.As<VkFence>());
			break;
		case eResSemaphore:
			vt->DestroySemaphore(Unwrap(dev), nondisp->real.As<VkSemaphore>());
			break;
	}

	return true;
}

// Sampler functions

bool WrappedVulkan::Serialise_vkCreateSampler(
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
				SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLER);
				Serialise_vkCreateSampler(device, pCreateInfo, pSampler);

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
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sem);
			GetResourceManager()->AddLiveResource(id, sem);
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
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSemaphore);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SEMAPHORE);
				Serialise_vkCreateSemaphore(device, pCreateInfo, pSemaphore);

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

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateFramebuffer(
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkAttachmentBindInfo *unwrapped = new VkAttachmentBindInfo[pCreateInfo->attachmentCount];
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
	{
		unwrapped[i] = pCreateInfo->pAttachments[i];
		unwrapped[i].view = Unwrap(unwrapped[i].view);
	}

	VkFramebufferCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
	unwrappedInfo.pAttachments = unwrapped;

	VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &unwrappedInfo, pFramebuffer);

	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFramebuffer);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_FRAMEBUFFER);
				Serialise_vkCreateFramebuffer(device, pCreateInfo, pFramebuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pFramebuffer);
			record->AddChunk(chunk);

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
				SCOPED_SERIALISE_CONTEXT(CREATE_RENDERPASS);
				Serialise_vkCreateRenderPass(device, pCreateInfo, pRenderPass);

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
				SCOPED_SERIALISE_CONTEXT(CREATE_VIEWPORT_STATE);
				Serialise_vkCreateDynamicViewportState(device, pCreateInfo, pState);

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
				SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE);
				Serialise_vkCreateDynamicRasterState(device, pCreateInfo, pState);

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
				SCOPED_SERIALISE_CONTEXT(CREATE_BLEND_STATE);
				Serialise_vkCreateDynamicColorBlendState(device, pCreateInfo, pState);

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
				SCOPED_SERIALISE_CONTEXT(CREATE_DEPTH_STATE);
				Serialise_vkCreateDynamicDepthStencilState(device, pCreateInfo, pState);

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

