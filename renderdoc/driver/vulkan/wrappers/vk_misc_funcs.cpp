/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

void WrappedVulkan::MakeSubpassLoadRP(VkRenderPassCreateInfo &info,
                                      const VkRenderPassCreateInfo *origInfo, uint32_t s)
{
  info.subpassCount = 1;
  info.pSubpasses = origInfo->pSubpasses + s;

  // remove any dependencies
  info.dependencyCount = 0;

  const VkSubpassDescription *sub = info.pSubpasses;
  VkAttachmentDescription *att = (VkAttachmentDescription *)info.pAttachments;

  // apply this subpass's attachment layouts to the initial and final layouts
  // so that this RP doesn't perform any layout transitions
  for(uint32_t a = 0; a < sub->colorAttachmentCount; a++)
  {
    if(sub->pColorAttachments[a].attachment != VK_ATTACHMENT_UNUSED)
    {
      att[sub->pColorAttachments[a].attachment].initialLayout =
          att[sub->pColorAttachments[a].attachment].finalLayout = sub->pColorAttachments[a].layout;
    }
  }

  for(uint32_t a = 0; a < sub->inputAttachmentCount; a++)
  {
    if(sub->pInputAttachments[a].attachment != VK_ATTACHMENT_UNUSED)
    {
      att[sub->pInputAttachments[a].attachment].initialLayout =
          att[sub->pInputAttachments[a].attachment].finalLayout = sub->pInputAttachments[a].layout;
    }
  }

  if(sub->pDepthStencilAttachment && sub->pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED)
  {
    att[sub->pDepthStencilAttachment->attachment].initialLayout =
        att[sub->pDepthStencilAttachment->attachment].finalLayout =
            sub->pDepthStencilAttachment->layout;
  }
}

// note, for threading reasons we ensure to release the wrappers before
// releasing the underlying object. Otherwise after releasing the vulkan object
// that same handle could be returned by create on another thread, and we
// could end up trying to re-wrap it.
#define DESTROY_IMPL(type, func)                                                                   \
  void WrappedVulkan::vk##func(VkDevice device, type obj, const VkAllocationCallbacks *pAllocator) \
  {                                                                                                \
    if(obj == VK_NULL_HANDLE)                                                                      \
      return;                                                                                      \
    type unwrappedObj = Unwrap(obj);                                                               \
    GetResourceManager()->ReleaseWrappedResource(obj, true);                                       \
    ObjDisp(device)->func(Unwrap(device), unwrappedObj, pAllocator);                               \
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
void WrappedVulkan::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR obj,
                                          const VkAllocationCallbacks *pAllocator)
{
  if(obj == VK_NULL_HANDLE)
    return;

  // release internal rendering objects we created for rendering the overlay
  {
    SwapchainInfo &info = *GetRecord(obj)->swapInfo;

    RenderDoc::Inst().RemoveFrameCapturer(LayerDisp(m_Instance), info.wndHandle);

    VkRenderPass unwrappedRP = Unwrap(info.rp);
    GetResourceManager()->ReleaseWrappedResource(info.rp, true);
    ObjDisp(device)->DestroyRenderPass(Unwrap(device), unwrappedRP, NULL);

    for(size_t i = 0; i < info.images.size(); i++)
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
void WrappedVulkan::vkDestroyImage(VkDevice device, VkImage obj,
                                   const VkAllocationCallbacks *pAllocator)
{
  if(obj == VK_NULL_HANDLE)
    return;

  {
    SCOPED_LOCK(m_ImageLayoutsLock);
    m_ImageLayouts.erase(GetResID(obj));
  }
  VkImage unwrappedObj = Unwrap(obj);
  GetResourceManager()->ReleaseWrappedResource(obj, true);
  return ObjDisp(device)->DestroyImage(Unwrap(device), unwrappedObj, pAllocator);
}

// needs to be separate since it's dispatchable
void WrappedVulkan::vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                         uint32_t commandBufferCount,
                                         const VkCommandBuffer *pCommandBuffers)
{
  for(uint32_t c = 0; c < commandBufferCount; c++)
  {
    if(pCommandBuffers[c] == VK_NULL_HANDLE)
      continue;

    WrappedVkDispRes *wrapped = (WrappedVkDispRes *)GetWrapped(pCommandBuffers[c]);

    VkCommandBuffer unwrapped = wrapped->real.As<VkCommandBuffer>();

    GetResourceManager()->ReleaseWrappedResource(pCommandBuffers[c]);

    ObjDisp(device)->FreeCommandBuffers(Unwrap(device), Unwrap(commandPool), 1, &unwrapped);
  }
}

bool WrappedVulkan::ReleaseResource(WrappedVkRes *res)
{
  if(res == NULL)
    return true;

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

    case eResUnknown: RDCERR("Unknown resource type!"); break;

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

bool WrappedVulkan::Serialise_vkCreateSampler(Serialiser *localSerialiser, VkDevice device,
                                              const VkSamplerCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkSampler *pSampler)
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

VkResult WrappedVulkan::vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator, VkSampler *pSampler)
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

bool WrappedVulkan::Serialise_vkCreateFramebuffer(Serialiser *localSerialiser, VkDevice device,
                                                  const VkFramebufferCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkFramebuffer *pFramebuffer)
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

VkResult WrappedVulkan::vkCreateFramebuffer(VkDevice device,
                                            const VkFramebufferCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkFramebuffer *pFramebuffer)
{
  VkImageView *unwrapped = GetTempArray<VkImageView>(pCreateInfo->attachmentCount);
  for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
    unwrapped[i] = Unwrap(pCreateInfo->pAttachments[i]);

  VkFramebufferCreateInfo unwrappedInfo = *pCreateInfo;
  unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
  unwrappedInfo.pAttachments = unwrapped;

  VkResult ret =
      ObjDisp(device)->CreateFramebuffer(Unwrap(device), &unwrappedInfo, pAllocator, pFramebuffer);

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

      record->imageAttachments = new AttachmentInfo[VkResourceRecord::MaxImageAttachments];
      RDCASSERT(pCreateInfo->attachmentCount <= VkResourceRecord::MaxImageAttachments);

      RDCEraseMem(record->imageAttachments,
                  sizeof(AttachmentInfo) * VkResourceRecord::MaxImageAttachments);

      VkResourceRecord *rpRecord = GetRecord(pCreateInfo->renderPass);

      record->AddParent(rpRecord);

      for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
      {
        VkResourceRecord *attRecord = GetRecord(pCreateInfo->pAttachments[i]);
        record->AddParent(attRecord);

        record->imageAttachments[i].record = attRecord;
        record->imageAttachments[i].barrier = rpRecord->imageAttachments[i].barrier;
        record->imageAttachments[i].barrier.image =
            GetResourceManager()->GetCurrentHandle<VkImage>(attRecord->baseResource);
        record->imageAttachments[i].barrier.subresourceRange = attRecord->viewRange;
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

bool WrappedVulkan::Serialise_vkCreateRenderPass(Serialiser *localSerialiser, VkDevice device,
                                                 const VkRenderPassCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkRenderPass *pRenderPass)
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
    // Likewise we don't want to throw away data before we're ready, so change
    // any load ops to LOAD instead of DONT_CARE (which is valid!). We of course
    // leave any LOAD_OP_CLEAR alone.
    VkAttachmentDescription *att = (VkAttachmentDescription *)info.pAttachments;
    for(uint32_t i = 0; i < info.attachmentCount; i++)
    {
      att[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      att[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

      if(att[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      if(att[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

      // renderpass can't start or end in presentable layout on replay
      ReplacePresentableImageLayout(att[i].initialLayout);
      ReplacePresentableImageLayout(att[i].finalLayout);
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
        for(uint32_t i = 0; i < info.attachmentCount; i++)
        {
          att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
          att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }

        VkRenderPassCreateInfo loadInfo = info;

        rpinfo.loadRPs.resize(info.subpassCount);

        // create a render pass for each subpass that maintains attachment layouts
        for(uint32_t s = 0; s < info.subpassCount; s++)
        {
          MakeSubpassLoadRP(loadInfo, &info, s);

          ret =
              ObjDisp(device)->CreateRenderPass(Unwrap(device), &loadInfo, NULL, &rpinfo.loadRPs[s]);
          RDCASSERTEQUAL(ret, VK_SUCCESS);

          // handle the loadRP being a duplicate
          if(GetResourceManager()->HasWrapper(ToTypedHandle(rpinfo.loadRPs[s])))
          {
            // just fetch the existing wrapped object
            rpinfo.loadRPs[s] =
                (VkRenderPass)(uint64_t)GetResourceManager()->GetNonDispWrapper(rpinfo.loadRPs[s]);

            // destroy this instance of the duplicate, as we must have matching create/destroy
            // calls and there won't be a wrapped resource hanging around to destroy this one.
            ObjDisp(device)->DestroyRenderPass(Unwrap(device), rpinfo.loadRPs[s], NULL);

            // don't need to ReplaceResource as no IDs are involved
          }
          else
          {
            ResourceId loadRPid =
                GetResourceManager()->WrapResource(Unwrap(device), rpinfo.loadRPs[s]);

            // register as a live-only resource, so it is cleaned up properly
            GetResourceManager()->AddLiveResource(loadRPid, rpinfo.loadRPs[s]);
          }
        }

        m_CreationInfo.m_RenderPass[live] = rpinfo;
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
                                           const VkAllocationCallbacks *pAllocator,
                                           VkRenderPass *pRenderPass)
{
  VkResult ret =
      ObjDisp(device)->CreateRenderPass(Unwrap(device), pCreateInfo, pAllocator, pRenderPass);

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

      record->imageAttachments = new AttachmentInfo[VkResourceRecord::MaxImageAttachments];
      RDCASSERT(pCreateInfo->attachmentCount <= VkResourceRecord::MaxImageAttachments);

      RDCEraseMem(record->imageAttachments,
                  sizeof(AttachmentInfo) * VkResourceRecord::MaxImageAttachments);

      for(uint32_t i = 0; i < pCreateInfo->attachmentCount; i++)
      {
        record->imageAttachments[i].record = NULL;
        record->imageAttachments[i].barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        record->imageAttachments[i].barrier.oldLayout = pCreateInfo->pAttachments[i].initialLayout;
        record->imageAttachments[i].barrier.newLayout = pCreateInfo->pAttachments[i].finalLayout;
      }
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
      for(uint32_t i = 0; i < info.attachmentCount; i++)
      {
        atts[i] = info.pAttachments[i];
        atts[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        atts[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      }

      info.pAttachments = atts;

      rpinfo.loadRPs.resize(pCreateInfo->subpassCount);

      // create a render pass for each subpass that maintains attachment layouts
      for(uint32_t s = 0; s < pCreateInfo->subpassCount; s++)
      {
        MakeSubpassLoadRP(info, pCreateInfo, s);

        ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, NULL, &rpinfo.loadRPs[s]);
        RDCASSERTEQUAL(ret, VK_SUCCESS);

        ResourceId loadRPid = GetResourceManager()->WrapResource(Unwrap(device), rpinfo.loadRPs[s]);

        // register as a live-only resource, so it is cleaned up properly
        GetResourceManager()->AddLiveResource(loadRPid, rpinfo.loadRPs[s]);
      }

      m_CreationInfo.m_RenderPass[id] = rpinfo;
    }
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkCreateQueryPool(Serialiser *localSerialiser, VkDevice device,
                                                const VkQueryPoolCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkQueryPool *pQueryPool)
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

VkResult WrappedVulkan::vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkQueryPool *pQueryPool)
{
  VkResult ret =
      ObjDisp(device)->CreateQueryPool(Unwrap(device), pCreateInfo, pAllocator, pQueryPool);

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

VkResult WrappedVulkan::vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool,
                                              uint32_t firstQuery, uint32_t queryCount,
                                              size_t dataSize, void *pData, VkDeviceSize stride,
                                              VkQueryResultFlags flags)
{
  return ObjDisp(device)->GetQueryPoolResults(Unwrap(device), Unwrap(queryPool), firstQuery,
                                              queryCount, dataSize, pData, stride, flags);
}

struct UserDebugCallbackData
{
  VkInstance wrappedInstance;
  VkDebugReportCallbackCreateInfoEXT createInfo;
  bool muteWarned;

  VkDebugReportCallbackEXT realObject;
};

VkBool32 VKAPI_PTR UserDebugCallback(VkDebugReportFlagsEXT flags,
                                     VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                     size_t location, int32_t messageCode, const char *pLayerPrefix,
                                     const char *pMessage, void *pUserData)
{
  UserDebugCallbackData *user = (UserDebugCallbackData *)pUserData;

  if(RenderDoc::Inst().GetCaptureOptions().DebugOutputMute)
  {
    if(user->muteWarned)
      return false;

    // once only insert a fake message notifying of the muting
    user->muteWarned = true;

    // we insert as an information message, since some trigger-happy applications might
    // debugbreak/crash/messagebox/etc on even warnings. This puts us in the same pool
    // as extremely spammy messages, but there's not much alternative.
    if(user->createInfo.flags & (VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT))
    {
      // use information type if possible, or if it's not accepted but debug is - use debug type.
      flags = (user->createInfo.flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
                  ? VK_DEBUG_REPORT_INFORMATION_BIT_EXT
                  : VK_DEBUG_REPORT_DEBUG_BIT_EXT;

      user->createInfo.pfnCallback(flags, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT,
                                   (uint64_t)user->wrappedInstance, 1, 1, "RDOC",
                                   "While debugging through RenderDoc, debug output through "
                                   "validation layers is suppressed.\n"
                                   "To show debug output look at the 'DebugOutputMute' capture "
                                   "option in RenderDoc's API, but "
                                   "be aware of false positives from the validation layers.",
                                   user->createInfo.pUserData);
    }

    return false;
  }

  return user->createInfo.pfnCallback(flags, objectType, object, location, messageCode,
                                      pLayerPrefix, pMessage, user->createInfo.pUserData);
}

VkResult WrappedVulkan::vkCreateDebugReportCallbackEXT(
    VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDebugReportCallbackEXT *pCallback)
{
  // we create an interception object here so that we can dynamically check the state of API
  // messages
  // being muted, since it's quite likely that the application will initialise Vulkan (and so create
  // a debug report callback) before it messes with RenderDoc's API to unmute messages.
  UserDebugCallbackData *user = new UserDebugCallbackData();
  user->wrappedInstance = instance;
  user->createInfo = *pCreateInfo;
  user->muteWarned = false;

  VkDebugReportCallbackCreateInfoEXT wrappedCreateInfo = *pCreateInfo;
  wrappedCreateInfo.pfnCallback = &UserDebugCallback;
  wrappedCreateInfo.pUserData = user;

  VkResult vkr = ObjDisp(instance)->CreateDebugReportCallbackEXT(
      Unwrap(instance), &wrappedCreateInfo, pAllocator, &user->realObject);

  if(vkr != VK_SUCCESS)
  {
    *pCallback = VK_NULL_HANDLE;
    delete user;
    return vkr;
  }

  *pCallback = (VkDebugReportCallbackEXT)(uint64_t)user;

  return vkr;
}

void WrappedVulkan::vkDestroyDebugReportCallbackEXT(VkInstance instance,
                                                    VkDebugReportCallbackEXT callback,
                                                    const VkAllocationCallbacks *pAllocator)
{
  UserDebugCallbackData *user = (UserDebugCallbackData *)(uintptr_t)NON_DISP_TO_UINT64(callback);

  ObjDisp(instance)->DestroyDebugReportCallbackEXT(Unwrap(instance), user->realObject, pAllocator);

  delete user;
}

void WrappedVulkan::vkDebugReportMessageEXT(VkInstance instance, VkDebugReportFlagsEXT flags,
                                            VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                            size_t location, int32_t messageCode,
                                            const char *pLayerPrefix, const char *pMessage)
{
  return ObjDisp(instance)->DebugReportMessageEXT(Unwrap(instance), flags, objectType, object,
                                                  location, messageCode, pLayerPrefix, pMessage);
}

static VkResourceRecord *GetObjRecord(VkDebugReportObjectTypeEXT objType, uint64_t object)
{
  switch(objType)
  {
    case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT: return GetRecord((VkInstance)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:
      return GetRecord((VkPhysicalDevice)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT: return GetRecord((VkDevice)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT: return GetRecord((VkQueue)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT: return GetRecord((VkCommandBuffer)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT: return GetRecord((VkDeviceMemory)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT: return GetRecord((VkBuffer)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT: return GetRecord((VkBufferView)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT: return GetRecord((VkImage)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT: return GetRecord((VkImageView)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT: return GetRecord((VkShaderModule)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT: return GetRecord((VkPipeline)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:
      return GetRecord((VkPipelineLayout)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT: return GetRecord((VkSampler)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT: return GetRecord((VkDescriptorSet)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT:
      return GetRecord((VkDescriptorSetLayout)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:
      return GetRecord((VkDescriptorPool)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT: return GetRecord((VkFence)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT: return GetRecord((VkSemaphore)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT: return GetRecord((VkEvent)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT: return GetRecord((VkQueryPool)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT: return GetRecord((VkFramebuffer)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT: return GetRecord((VkRenderPass)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT: return GetRecord((VkPipelineCache)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT: return GetRecord((VkSurfaceKHR)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT: return GetRecord((VkSwapchainKHR)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT: return GetRecord((VkCommandPool)object);
    case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_RANGE_SIZE_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_OBJECT_TABLE_NVX_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX_EXT:
    case VK_DEBUG_REPORT_OBJECT_TYPE_MAX_ENUM_EXT: break;
  }
  return NULL;
}

bool WrappedVulkan::Serialise_SetShaderDebugPath(Serialiser *localSerialiser, VkDevice device,
                                                 VkDebugMarkerObjectTagInfoEXT *pTagInfo)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetObjRecord(pTagInfo->objectType, pTagInfo->object)->GetResourceID());

  string path;
  if(m_State >= WRITING)
  {
    char *tag = (char *)pTagInfo->pTag;
    path = string(tag, tag + pTagInfo->tagSize);
  }

  localSerialiser->Serialise("path", path);

  if(m_State == READING)
  {
    m_CreationInfo.m_ShaderModule[GetResourceManager()->GetLiveID(id)].unstrippedPath = path;
  }

  return true;
}

VkResult WrappedVulkan::vkDebugMarkerSetObjectTagEXT(VkDevice device,
                                                     VkDebugMarkerObjectTagInfoEXT *pTagInfo)
{
  if(m_State >= WRITING && pTagInfo)
  {
    VkResourceRecord *record = GetObjRecord(pTagInfo->objectType, pTagInfo->object);

    if(!record)
    {
      RDCERR("Unrecognised object %d %llu", pTagInfo->objectType, pTagInfo->object);
      return VK_SUCCESS;
    }

    if(pTagInfo->tagName == RENDERDOC_ShaderDebugMagicValue_truncated &&
       pTagInfo->objectType == VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT)
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CONTEXT(SET_SHADER_DEBUG_PATH);
      Serialise_SetShaderDebugPath(localSerialiser, device, pTagInfo);
      record->AddChunk(scope.Get());
    }
    else if(ObjDisp(device)->DebugMarkerSetObjectTagEXT)
    {
      VkDebugMarkerObjectTagInfoEXT unwrapped = *pTagInfo;

      // special case for VkSurfaceKHR - the record pointer is actually just the underlying native
      // window handle, so instead we unwrap and call through.
      if(unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT)
      {
        unwrapped.object = GetWrapped((VkSurfaceKHR)unwrapped.object)->real.handle;

        return ObjDisp(device)->DebugMarkerSetObjectTagEXT(device, &unwrapped);
      }

      if(unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT ||
         unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT ||
         unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT ||
         unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT ||
         unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT)
      {
        WrappedVkDispRes *res = (WrappedVkDispRes *)record->Resource;
        unwrapped.object = res->real.handle;
      }
      else
      {
        WrappedVkNonDispRes *res = (WrappedVkNonDispRes *)record->Resource;
        unwrapped.object = res->real.handle;
      }

      return ObjDisp(device)->DebugMarkerSetObjectTagEXT(device, &unwrapped);
    }
  }

  return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkDebugMarkerSetObjectNameEXT(Serialiser *localSerialiser,
                                                            VkDevice device,
                                                            VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
  SERIALISE_ELEMENT(ResourceId, id,
                    GetObjRecord(pNameInfo->objectType, pNameInfo->object)->GetResourceID());

  string name;
  if(m_State >= WRITING)
    name = pNameInfo->pObjectName;

  localSerialiser->Serialise("name", name);

  if(m_State == READING)
  {
    // if we don't have a live resource, this is probably a command buffer being named on the
    // virtual non-existant parent, not any of the baked IDs. Just save the name on the original ID
    // and we'll propagate it in Serialise_vkBeginCommandBuffer
    if(!GetResourceManager()->HasLiveResource(id) || GetResourceManager()->HasReplacement(id))
      m_CreationInfo.m_Names[id] = name;
    else
      m_CreationInfo.m_Names[GetResourceManager()->GetLiveID(id)] = name;
  }

  return true;
}

VkResult WrappedVulkan::vkDebugMarkerSetObjectNameEXT(VkDevice device,
                                                      VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
  if(m_State >= WRITING && pNameInfo)
  {
    Chunk *chunk = NULL;

    VkResourceRecord *record = GetObjRecord(pNameInfo->objectType, pNameInfo->object);

    if(!record)
    {
      RDCERR("Unrecognised object %d %llu", pNameInfo->objectType, pNameInfo->object);
      return VK_SUCCESS;
    }

    VkDebugMarkerObjectNameInfoEXT unwrapped = *pNameInfo;

    // special case for VkSurfaceKHR - the record pointer is actually just the underlying native
    // window handle, so instead we unwrap and call through.
    if(unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT)
    {
      unwrapped.object = GetWrapped((VkSurfaceKHR)unwrapped.object)->real.handle;

      if(ObjDisp(device)->DebugMarkerSetObjectNameEXT)
        return ObjDisp(device)->DebugMarkerSetObjectNameEXT(device, &unwrapped);

      return VK_SUCCESS;
    }

    if(unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT ||
       unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT ||
       unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT ||
       unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT ||
       unwrapped.objectType == VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT)
    {
      WrappedVkDispRes *res = (WrappedVkDispRes *)record->Resource;
      unwrapped.object = res->real.handle;
    }
    else
    {
      WrappedVkNonDispRes *res = (WrappedVkNonDispRes *)record->Resource;
      unwrapped.object = res->real.handle;
    }

    if(ObjDisp(device)->DebugMarkerSetObjectNameEXT)
      ObjDisp(device)->DebugMarkerSetObjectNameEXT(device, &unwrapped);

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CONTEXT(SET_NAME);
      Serialise_vkDebugMarkerSetObjectNameEXT(localSerialiser, device, pNameInfo);

      chunk = scope.Get();
    }

    record->AddChunk(chunk);
  }

  return VK_SUCCESS;
}
