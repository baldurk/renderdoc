/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

static void PatchSeparateStencil(VkAttachmentDescription &att, const VkAttachmentReference *ref)
{
}

static void PatchSeparateStencil(VkAttachmentDescription2 &att, const VkAttachmentReference2 *ref)
{
  // VK_KHR_separate_depth_stencil_layouts
  const VkAttachmentReferenceStencilLayout *separateStencilRef =
      (const VkAttachmentReferenceStencilLayout *)FindNextStruct(
          ref, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT);

  VkAttachmentDescriptionStencilLayout *separateStencilAtt =
      (VkAttachmentDescriptionStencilLayout *)FindNextStruct(
          &att, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);

  if(separateStencilRef && separateStencilAtt)
  {
    separateStencilAtt->stencilInitialLayout = separateStencilAtt->stencilFinalLayout =
        separateStencilRef->stencilLayout;
  }
}

template <typename RPCreateInfo>
static void MakeSubpassLoadRP(RPCreateInfo &info, const RPCreateInfo *origInfo, uint32_t s)
{
  info.subpassCount = 1;
  info.pSubpasses = origInfo->pSubpasses + s;

  // VK_KHR_multiview
  const VkRenderPassMultiviewCreateInfo *multiview =
      (const VkRenderPassMultiviewCreateInfo *)FindNextStruct(
          origInfo, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);

  static VkRenderPassMultiviewCreateInfo patchedMultiview;

  if(multiview)
  {
    // remove from the chain, the caller ensured we have a mutable chain so we won't be trashing the
    // pNext chain we'll look up for any subsequent subpasses
    RemoveNextStruct(&info, VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
    patchedMultiview = *multiview;

    if(patchedMultiview.subpassCount > 0)
    {
      // keep the view mask for our target subpass
      patchedMultiview.subpassCount = 1;
      patchedMultiview.pViewMasks = patchedMultiview.pViewMasks + s;

      // view offsets are not allowed for self-dependencies, and we remove all other dependencies.
      patchedMultiview.dependencyCount = 0;
      patchedMultiview.pViewOffsets = NULL;

      // add onto the chain
      patchedMultiview.pNext = info.pNext;
      info.pNext = &patchedMultiview;
    }
  }

  // remove input attachment aspect structure unconditionally rather than patching it, since it is
  // optional and omitting it only makes invalid behaviour valid
  RemoveNextStruct(&info, VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO);

  // remove any non-self dependencies
  info.dependencyCount = 0;
  for(uint32_t i = 0; i < origInfo->dependencyCount; i++)
  {
    // if this dependency is a self-dependency for the target subpass, keep it
    if(origInfo->pDependencies[i].srcSubpass == origInfo->pDependencies[i].dstSubpass &&
       origInfo->pDependencies[i].srcSubpass == s)
    {
      uint32_t d = info.dependencyCount;
      info.dependencyCount++;

      // copy the dependency
      memcpy((void *)&info.pDependencies[d], &origInfo->pDependencies[i],
             sizeof(origInfo->pDependencies[i]));

      // set the srcSubpass/dstSubpass to 0 since we're rewriting this renderpass to contain one
      // subpass only
      RDCEraseEl(info.pDependencies[d].srcSubpass);
      RDCEraseEl(info.pDependencies[d].dstSubpass);
      break;
    }
  }

  // we use decltype here because this is templated to work for regular and create_renderpass2
  // structs
  using SubpassInfo = typename std::remove_reference<decltype(info.pSubpasses[0])>::type;
  using AttachmentInfo =
      typename std::remove_cv<typename std::remove_reference<decltype(info.pAttachments[0])>::type>::type;

  SubpassInfo *sub = info.pSubpasses;
  AttachmentInfo *att = (AttachmentInfo *)info.pAttachments;

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

    // need to do this via an external overloaded function because FindNextStruct will fail to
    // compile on VkAttachmentReference
    PatchSeparateStencil(att[sub->pDepthStencilAttachment->attachment], sub->pDepthStencilAttachment);
  }
}

// note, for threading reasons we ensure to release the wrappers before
// releasing the underlying object. Otherwise after releasing the vulkan object
// that same handle could be returned by create on another thread, and we
// could end up trying to re-wrap it.
#define DESTROY_IMPL(type, func)                                                         \
  void WrappedVulkan::vk##func(VkDevice device, type obj, const VkAllocationCallbacks *) \
  {                                                                                      \
    if(obj == VK_NULL_HANDLE)                                                            \
      return;                                                                            \
    type unwrappedObj = Unwrap(obj);                                                     \
    m_ForcedReferences.removeOne(GetRecord(obj));                                        \
    if(IsReplayMode(m_State))                                                            \
      m_CreationInfo.erase(GetResID(obj));                                               \
    GetResourceManager()->ReleaseWrappedResource(obj, true);                             \
    ObjDisp(device)->func(Unwrap(device), unwrappedObj, NULL);                           \
  }

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
DESTROY_IMPL(VkDescriptorUpdateTemplate, DestroyDescriptorUpdateTemplate)
DESTROY_IMPL(VkSamplerYcbcrConversion, DestroySamplerYcbcrConversion)
DESTROY_IMPL(VkAccelerationStructureKHR, DestroyAccelerationStructureKHR)

#undef DESTROY_IMPL

void WrappedVulkan::vkDestroyFramebuffer(VkDevice device, VkFramebuffer obj,
                                         const VkAllocationCallbacks *)
{
  if(obj == VK_NULL_HANDLE)
    return;
  VkFramebuffer unwrappedObj = Unwrap(obj);
  if(IsReplayMode(m_State))
  {
    const VulkanCreationInfo::Framebuffer &rpinfo = m_CreationInfo.m_Framebuffer[GetResID(obj)];

    for(VkFramebuffer loadfb : rpinfo.loadFBs)
    {
      ObjDisp(device)->DestroyFramebuffer(Unwrap(device), Unwrap(loadfb), NULL);
      GetResourceManager()->ReleaseWrappedResource(loadfb, true);
    }

    m_CreationInfo.erase(GetResID(obj));
  }
  GetResourceManager()->ReleaseWrappedResource(obj, true);
  ObjDisp(device)->DestroyFramebuffer(Unwrap(device), unwrappedObj, NULL);
}

void WrappedVulkan::vkDestroyRenderPass(VkDevice device, VkRenderPass obj,
                                        const VkAllocationCallbacks *)
{
  if(obj == VK_NULL_HANDLE)
    return;
  VkRenderPass unwrappedObj = Unwrap(obj);
  if(IsReplayMode(m_State))
  {
    const VulkanCreationInfo::RenderPass &rpinfo = m_CreationInfo.m_RenderPass[GetResID(obj)];

    for(VkRenderPass loadrp : rpinfo.loadRPs)
    {
      ObjDisp(device)->DestroyRenderPass(Unwrap(device), Unwrap(loadrp), NULL);
      GetResourceManager()->ReleaseWrappedResource(loadrp, true);
    }

    m_CreationInfo.erase(GetResID(obj));
  }
  GetResourceManager()->ReleaseWrappedResource(obj, true);
  ObjDisp(device)->DestroyRenderPass(Unwrap(device), unwrappedObj, NULL);
}

void WrappedVulkan::vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *)
{
  if(buffer == VK_NULL_HANDLE)
    return;

  // artificially extend the lifespan of buffer device address memory or buffers, to ensure their
  // opaque capture address isn't re-used before the capture completes
  {
    SCOPED_READLOCK(m_CapTransitionLock);
    SCOPED_LOCK(m_DeviceAddressResourcesLock);
    if(IsActiveCapturing(m_State) && m_DeviceAddressResources.IDs.contains(GetResID(buffer)))
    {
      m_DeviceAddressResources.DeadBuffers.push_back(buffer);
      return;
    }
    m_DeviceAddressResources.IDs.removeOne(GetResID(buffer));
  }

  VkBuffer unwrappedObj = Unwrap(buffer);

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(buffer);

    if(record->resInfo && record->resInfo->dedicatedMemory != ResourceId())
      GetResourceManager()->PreFreeMemory(record->resInfo->dedicatedMemory);
  }

  {
    SCOPED_LOCK(m_ForcedReferencesLock);
    m_ForcedReferences.removeOne(GetRecord(buffer));
  }

  if(IsReplayMode(m_State))
    m_CreationInfo.erase(GetResID(buffer));

  GetResourceManager()->ReleaseWrappedResource(buffer, true);

  ObjDisp(device)->DestroyBuffer(Unwrap(device), unwrappedObj, NULL);
}

// needs to be separate because it releases internal resources
void WrappedVulkan::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR obj,
                                          const VkAllocationCallbacks *)
{
  if(obj == VK_NULL_HANDLE)
    return;

  // release internal rendering objects we created for rendering the overlay
  {
    SwapchainInfo &info = *GetRecord(obj)->swapInfo;

    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));

    VkRenderPass unwrappedRP = Unwrap(info.rp);
    GetResourceManager()->ReleaseWrappedResource(info.rp, true);
    ObjDisp(device)->DestroyRenderPass(Unwrap(device), unwrappedRP, NULL);

    for(size_t i = 0; i < info.images.size(); i++)
    {
      VkFramebuffer unwrappedFB = Unwrap(info.images[i].fb);
      VkImageView unwrappedView = Unwrap(info.images[i].view);
      VkSemaphore unwrappedSem = Unwrap(info.images[i].overlaydone);
      VkFence unwrappedFence = Unwrap(info.images[i].fence);
      GetResourceManager()->ReleaseWrappedResource(info.images[i].fb, true);
      // note, image doesn't have to be destroyed, just untracked
      GetResourceManager()->ReleaseWrappedResource(info.images[i].im, true);
      GetResourceManager()->ReleaseWrappedResource(info.images[i].view, true);
      GetResourceManager()->ReleaseWrappedResource(info.images[i].overlaydone);
      GetResourceManager()->ReleaseWrappedResource(info.images[i].fence);
      ObjDisp(device)->DestroyFramebuffer(Unwrap(device), unwrappedFB, NULL);
      ObjDisp(device)->DestroyImageView(Unwrap(device), unwrappedView, NULL);
      ObjDisp(device)->DestroySemaphore(Unwrap(device), unwrappedSem, NULL);
      ObjDisp(device)->DestroyFence(Unwrap(device), unwrappedFence, NULL);

      // return the command buffers to the pool
      AddFreeCommandBuffer(info.images[i].cmd);
    }
  }

  VkSwapchainKHR unwrappedObj = Unwrap(obj);
  GetResourceManager()->ReleaseWrappedResource(obj, true);
  ObjDisp(device)->DestroySwapchainKHR(Unwrap(device), unwrappedObj, NULL);
}

// needs to be separate so we don't erase from m_ImageLayouts in other destroy functions
void WrappedVulkan::vkDestroyImage(VkDevice device, VkImage obj, const VkAllocationCallbacks *)
{
  if(obj == VK_NULL_HANDLE)
    return;

  VkImage unwrappedObj = Unwrap(obj);
  GetResourceManager()->ReleaseWrappedResource(obj, true);

  EraseImageState(GetResID(obj));

  return ObjDisp(device)->DestroyImage(Unwrap(device), unwrappedObj, NULL);
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

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
    RDCLOG("vkFreeCommandBuffers(%s)", ToStr(wrapped->id).c_str());
#endif

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
  VkDevice dev = m_Device;
  const VkDevDispatchTable *vt = m_Device != VK_NULL_HANDLE ? ObjDisp(dev) : NULL;

  WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;
  WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
  uint64_t handle = (uint64_t)nondisp;

  switch(IdentifyTypeByPtr(res))
  {
    case eResSurface:
    case eResSwapchain:
      if(IsCaptureMode(m_State))
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
      if(IsReplayMode(m_State))
        GetResourceManager()->ReleaseWrappedResource((VkCommandBuffer)res);
      break;
    case eResDescriptorSet:
      if(IsReplayMode(m_State))
        GetResourceManager()->ReleaseWrappedResource(VkDescriptorSet(handle));
      break;
    case eResPhysicalDevice:
      if(IsReplayMode(m_State))
        GetResourceManager()->ReleaseWrappedResource((VkPhysicalDevice)disp);
      break;
    case eResQueue:
      if(IsReplayMode(m_State))
        GetResourceManager()->ReleaseWrappedResource((VkQueue)disp);
      break;

    case eResDevice:
      // these are explicitly released elsewhere, do not need to destroy
      // any API objects.
      // On replay though we do need to tidy up book-keeping for these.
      if(IsReplayMode(m_State))
      {
        GetResourceManager()->ReleaseCurrentResource(disp->id);
        GetResourceManager()->RemoveWrapper(ToTypedHandle(disp->real.As<VkDevice>()));
      }
      break;
    case eResInstance:
      if(IsReplayMode(m_State))
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
    case eResDescUpdateTemplate:
    {
      VkDescriptorUpdateTemplate real = nondisp->real.As<VkDescriptorUpdateTemplate>();
      GetResourceManager()->ReleaseWrappedResource(VkDescriptorUpdateTemplate(handle));
      vt->DestroyDescriptorUpdateTemplate(Unwrap(dev), real, NULL);
      break;
    }
    case eResSamplerConversion:
    {
      VkSamplerYcbcrConversion real = nondisp->real.As<VkSamplerYcbcrConversion>();
      GetResourceManager()->ReleaseWrappedResource(VkSamplerYcbcrConversion(handle));
      vt->DestroySamplerYcbcrConversion(Unwrap(dev), real, NULL);
      break;
    }
    case eResAccelerationStructureKHR:
    {
      VkAccelerationStructureKHR real = nondisp->real.As<VkAccelerationStructureKHR>();
      GetResourceManager()->ReleaseWrappedResource(VkAccelerationStructureKHR(handle));
      vt->DestroyAccelerationStructureKHR(Unwrap(dev), real, NULL);
      break;
    }
  }

  return true;
}

// Sampler functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateSampler(SerialiserType &ser, VkDevice device,
                                              const VkSamplerCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkSampler *pSampler)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Sampler, GetResID(*pSampler)).TypedAs("VkSampler"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSampler samp = VK_NULL_HANDLE;

    VkSamplerCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkSamplerCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), &patched, NULL, &samp);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating sampler, VkResult: %s", ToStr(ret).c_str());
      return false;
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
        GetResourceManager()->ReplaceResource(Sampler, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), samp);
        GetResourceManager()->AddLiveResource(Sampler, samp);

        m_CreationInfo.m_Sampler[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(Sampler, ResourceType::Sampler, "Sampler");
    DerivedResource(device, Sampler);
    const VkSamplerYcbcrConversionInfo *ycbcr = (const VkSamplerYcbcrConversionInfo *)FindNextStruct(
        &CreateInfo, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
    if(ycbcr)
    {
      DerivedResource(ycbcr->conversion, Sampler);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *, VkSampler *pSampler)
{
  VkSamplerCreateInfo info_adjusted = *pCreateInfo;

  // like in VkCreateImage, create non-subsampled sampler since the image is now non-subsampled.
  info_adjusted.flags &= ~VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;
  info_adjusted.flags &= ~VK_SAMPLER_CREATE_SUBSAMPLED_COARSE_RECONSTRUCTION_BIT_EXT;

  byte *tempMem = GetTempMemory(GetNextPatchSize(info_adjusted.pNext));

  UnwrapNextChain(m_State, "VkSamplerCreateInfo", tempMem, (VkBaseInStructure *)&info_adjusted);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateSampler(Unwrap(device), &info_adjusted, NULL, pSampler));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSampler);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateSampler);
        Serialise_vkCreateSampler(ser, device, pCreateInfo, NULL, pSampler);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSampler);
      record->AddChunk(chunk);

      const VkSamplerYcbcrConversionInfo *ycbcr =
          (const VkSamplerYcbcrConversionInfo *)FindNextStruct(
              pCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
      if(ycbcr)
      {
        VkResourceRecord *ycbcrRecord = GetRecord(ycbcr->conversion);
        record->AddParent(ycbcrRecord);
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pSampler);

      m_CreationInfo.m_Sampler[id].Init(GetResourceManager(), m_CreationInfo, pCreateInfo);
    }
  }

  return ret;
}

void WrappedVulkan::PatchAttachment(VkFramebufferAttachmentImageInfo *att, VkFormat imgFormat,
                                    VkSampleCountFlagBits samples)
{
  // this matches the mutations we do to images, so see vkCreateImage
  att->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  att->usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  att->usage &= ~VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

  if(IsYUVFormat(imgFormat))
    att->flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

  if(samples != VK_SAMPLE_COUNT_1_BIT)
  {
    att->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    att->flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    if(!IsDepthOrStencilFormat(imgFormat))
    {
      if(GetDebugManager() && GetShaderCache()->IsBuffer2MSSupported())
        att->usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else
    {
      att->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
  }

  att->flags &= ~VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateFramebuffer(SerialiserType &ser, VkDevice device,
                                                  const VkFramebufferCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkFramebuffer *pFramebuffer)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Framebuffer, GetResID(*pFramebuffer)).TypedAs("VkFramebuffer"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkFramebuffer fb = VK_NULL_HANDLE;

    byte *tempMem = GetTempMemory(GetNextPatchSize(&CreateInfo));
    VkFramebufferCreateInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, &CreateInfo);

    const VulkanCreationInfo::RenderPass &rpinfo =
        m_CreationInfo.m_RenderPass[GetResID(CreateInfo.renderPass)];

    VkFramebufferAttachmentsCreateInfo *attachmentsInfo =
        (VkFramebufferAttachmentsCreateInfo *)FindNextStruct(
            unwrappedInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO);

    for(uint32_t a = 0; attachmentsInfo && a < attachmentsInfo->attachmentImageInfoCount; a++)
    {
      VkFramebufferAttachmentImageInfo *att =
          (VkFramebufferAttachmentImageInfo *)&attachmentsInfo->pAttachmentImageInfos[a];

      PatchAttachment(att, rpinfo.attachments[a].format, rpinfo.attachments[a].samples);
    }

    VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), unwrappedInfo, NULL, &fb);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating framebuffer, VkResult: %s", ToStr(ret).c_str());
      return false;
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
        GetResourceManager()->ReplaceResource(Framebuffer, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), fb);
        GetResourceManager()->AddLiveResource(Framebuffer, fb);

        NameVulkanObject(fb, StringFormat::Fmt("Framebuffer %s", ToStr(Framebuffer).c_str()));

        VulkanCreationInfo::Framebuffer fbinfo;
        fbinfo.Init(GetResourceManager(), m_CreationInfo, &CreateInfo);

        fbinfo.loadFBs.resize(rpinfo.loadRPs.size());

        // create a render pass for each subpass that maintains attachment layouts
        for(size_t s = 0; s < fbinfo.loadFBs.size(); s++)
        {
          unwrappedInfo->renderPass = Unwrap(rpinfo.loadRPs[s]);

          ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), unwrappedInfo, NULL,
                                                   &fbinfo.loadFBs[s]);
          RDCASSERTEQUAL(ret, VK_SUCCESS);

          // handle the loadRP being a duplicate
          if(GetResourceManager()->HasWrapper(ToTypedHandle(fbinfo.loadFBs[s])))
          {
            // just fetch the existing wrapped object
            fbinfo.loadFBs[s] =
                (VkFramebuffer)(uint64_t)GetResourceManager()->GetNonDispWrapper(fbinfo.loadFBs[s]);

            // destroy this instance of the duplicate, as we must have matching create/destroy
            // calls and there won't be a wrapped resource hanging around to destroy this one.
            ObjDisp(device)->DestroyFramebuffer(Unwrap(device), fbinfo.loadFBs[s], NULL);

            // don't need to ReplaceResource as no IDs are involved
          }
          else
          {
            ResourceId loadFBid =
                GetResourceManager()->WrapResource(Unwrap(device), fbinfo.loadFBs[s]);

            // register as a live-only resource, so it is cleaned up properly
            GetResourceManager()->AddLiveResource(loadFBid, fbinfo.loadFBs[s]);

            NameVulkanObject(fbinfo.loadFBs[s], StringFormat::Fmt("Framebuffer %s loadFB %d",
                                                                  ToStr(Framebuffer).c_str(), s));
          }
        }

        m_CreationInfo.m_Framebuffer[live] = fbinfo;
      }
    }

    AddResource(Framebuffer, ResourceType::RenderPass, "Framebuffer");
    DerivedResource(device, Framebuffer);
    DerivedResource(CreateInfo.renderPass, Framebuffer);

    if((CreateInfo.flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT) == 0)
    {
      for(uint32_t i = 0; i < CreateInfo.attachmentCount; i++)
        DerivedResource(CreateInfo.pAttachments[i], Framebuffer);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateFramebuffer(VkDevice device,
                                            const VkFramebufferCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *,
                                            VkFramebuffer *pFramebuffer)
{
  byte *tempMem = GetTempMemory(GetNextPatchSize(pCreateInfo));
  VkFramebufferCreateInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pCreateInfo);

  VkFramebufferAttachmentsCreateInfo *attachmentsInfo =
      (VkFramebufferAttachmentsCreateInfo *)FindNextStruct(
          unwrappedInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO);

  AttachmentInfo *capAtts = NULL;
  VulkanCreationInfo::RenderPass::Attachment *replayAtts = NULL;
  if(IsCaptureMode(m_State))
    capAtts = GetRecord(pCreateInfo->renderPass)->renderPassInfo->imageAttachments;
  else
    replayAtts = m_CreationInfo.m_RenderPass[GetResID(pCreateInfo->renderPass)].attachments.data();

  // this matches the mutations we do to images, so see vkCreateImage
  for(uint32_t a = 0; attachmentsInfo && a < attachmentsInfo->attachmentImageInfoCount; a++)
  {
    VkFramebufferAttachmentImageInfo *att =
        (VkFramebufferAttachmentImageInfo *)&attachmentsInfo->pAttachmentImageInfos[a];

    if(IsCaptureMode(m_State))
      PatchAttachment(att, capAtts[a].format, capAtts[a].samples);
    else
      PatchAttachment(att, replayAtts[a].format, replayAtts[a].samples);
  }

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), unwrappedInfo, NULL, pFramebuffer));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFramebuffer);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateFramebuffer);
        Serialise_vkCreateFramebuffer(ser, device, pCreateInfo, NULL, pFramebuffer);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pFramebuffer);
      record->AddChunk(chunk);

      VkResourceRecord *rpRecord = GetRecord(pCreateInfo->renderPass);
      record->AddParent(rpRecord);

      RenderPassInfo *rpInfo = rpRecord->renderPassInfo;
      FramebufferInfo *fbInfo = record->framebufferInfo = new FramebufferInfo(*pCreateInfo);

      for(uint32_t i = 0, a = 0; i < pCreateInfo->attachmentCount; i++, a++)
      {
        fbInfo->imageAttachments[a].barrier = rpInfo->imageAttachments[a].barrier;
        fbInfo->imageAttachments[a].format = rpInfo->imageAttachments[a].format;
        fbInfo->imageAttachments[a].samples = rpInfo->imageAttachments[a].samples;

        if((pCreateInfo->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT) == 0)
        {
          VkResourceRecord *attRecord = GetRecord(pCreateInfo->pAttachments[i]);
          record->AddParent(attRecord);

          fbInfo->imageAttachments[a].record = attRecord;
          fbInfo->imageAttachments[a].barrier.image =
              GetResourceManager()->GetCurrentHandle<VkImage>(attRecord->baseResource);
          fbInfo->imageAttachments[a].barrier.subresourceRange = attRecord->viewRange;

          {
            auto state = FindImageState(attRecord->GetResourceID());
            if(state && state->GetImageInfo().extent.depth > 1)
            {
              fbInfo->imageAttachments[a].barrier.subresourceRange.baseArrayLayer = 0;
              fbInfo->imageAttachments[a].barrier.subresourceRange.layerCount = 1;
            }
          }

          // if the renderpass specifies an aspect mask that mean split depth/stencil handling, so
          // respect the aspect mask and the next one will be stencil
          if(rpInfo->imageAttachments[a].barrier.subresourceRange.aspectMask != 0)
          {
            // restore the aspectMask that might have been trampled by attRecord->viewRange
            fbInfo->imageAttachments[a].barrier.subresourceRange.aspectMask =
                rpInfo->imageAttachments[a].barrier.subresourceRange.aspectMask;

            a++;

            // copy most properties from previous barrier
            fbInfo->imageAttachments[a].record = fbInfo->imageAttachments[a - 1].record;
            fbInfo->imageAttachments[a].barrier = fbInfo->imageAttachments[a - 1].barrier;
            // copy aspect mask and layouts from the next barrier in the RP
            fbInfo->imageAttachments[a].barrier.subresourceRange.aspectMask =
                rpInfo->imageAttachments[a].barrier.subresourceRange.aspectMask;
            fbInfo->imageAttachments[a].barrier.oldLayout =
                rpInfo->imageAttachments[a].barrier.oldLayout;
            fbInfo->imageAttachments[a].barrier.newLayout =
                rpInfo->imageAttachments[a].barrier.newLayout;
          }
        }
      }
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pFramebuffer);

      VulkanCreationInfo::Framebuffer fbinfo;
      fbinfo.Init(GetResourceManager(), m_CreationInfo, pCreateInfo);

      const VulkanCreationInfo::RenderPass &rpinfo =
          m_CreationInfo.m_RenderPass[GetResID(pCreateInfo->renderPass)];

      fbinfo.loadFBs.resize(rpinfo.loadRPs.size());

      // create a render pass for each subpass that maintains attachment layouts
      for(size_t s = 0; s < fbinfo.loadFBs.size(); s++)
      {
        unwrappedInfo->renderPass = Unwrap(rpinfo.loadRPs[s]);

        ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), unwrappedInfo, NULL,
                                                 &fbinfo.loadFBs[s]);
        RDCASSERTEQUAL(ret, VK_SUCCESS);

        ResourceId loadFBid = GetResourceManager()->WrapResource(Unwrap(device), fbinfo.loadFBs[s]);

        // register as a live-only resource, so it is cleaned up properly
        GetResourceManager()->AddLiveResource(loadFBid, fbinfo.loadFBs[s]);
      }

      m_CreationInfo.m_Framebuffer[id] = fbinfo;
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateRenderPass(SerialiserType &ser, VkDevice device,
                                                 const VkRenderPassCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkRenderPass *pRenderPass)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(RenderPass, GetResID(*pRenderPass)).TypedAs("VkRenderPass"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkRenderPass rp = VK_NULL_HANDLE;

    VulkanCreationInfo::RenderPass rpinfo;
    rpinfo.Init(GetResourceManager(), m_CreationInfo, &CreateInfo);

    // we want to store off the data so we can display it after the pass.
    // override any user-specified DONT_CARE.
    // Likewise we don't want to throw away data before we're ready, so change
    // any load ops to LOAD instead of DONT_CARE (which is valid!). We of course
    // leave any LOAD_OP_CLEAR alone.
    VkAttachmentDescription *att = (VkAttachmentDescription *)CreateInfo.pAttachments;
    for(uint32_t i = 0; i < CreateInfo.attachmentCount; i++)
    {
      if(att[i].storeOp != VK_ATTACHMENT_STORE_OP_NONE)
        att[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      if(att[i].stencilStoreOp != VK_ATTACHMENT_STORE_OP_NONE)
        att[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

      if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
      {
        if(att[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
          att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if(att[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
          att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        if(att[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD &&
           att[i].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
          att[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        if(att[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD &&
           att[i].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
          att[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
      }

      // sanitise the actual layouts used to create the renderpass
      SanitiseOldImageLayout(att[i].initialLayout);
      SanitiseNewImageLayout(att[i].finalLayout);
    }

    VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &CreateInfo, NULL, &rp);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating render pass, VkResult: %s", ToStr(ret).c_str());
      return false;
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
        GetResourceManager()->ReplaceResource(RenderPass, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), rp);
        GetResourceManager()->AddLiveResource(RenderPass, rp);

        bool badIndirectArgDep = false;

        for(uint32_t i = 0; i < CreateInfo.dependencyCount; i++)
          if(CreateInfo.pDependencies[i].dstAccessMask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
            badIndirectArgDep = true;

        if(badIndirectArgDep)
          AddDebugMessage(MessageCategory::State_Creation, MessageSeverity::High,
                          MessageSource::RuntimeWarning,
                          StringFormat::Fmt("Creating renderpass %s contains a subpass dependency "
                                            "that would allow writing indirect command arguments.\n"
                                            "Indirect command contents are read at the end of the "
                                            "render pass, so writes during the pass will "
                                            "cause incorrect display of indirect arguments.",
                                            ToStr(RenderPass).c_str()));

        // make a version of the render pass that loads from its attachments,
        // so it can be used for replaying a single draw after a render pass
        // without doing a clear or a DONT_CARE load.
        for(uint32_t i = 0; i < CreateInfo.attachmentCount; i++)
        {
          if(att[i].loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
            att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
          if(att[i].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
            att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }

        VkRenderPassCreateInfo loadInfo = CreateInfo;

        {
          byte *tempMem = GetTempMemory(GetNextPatchSize(loadInfo.pNext));
          CopyNextChainForPatching("VkRenderPassCreateInfo", tempMem, (VkBaseInStructure *)&loadInfo);
        }

        rpinfo.loadRPs.resize(CreateInfo.subpassCount);

        // create a render pass for each subpass that maintains attachment layouts
        for(uint32_t s = 0; s < CreateInfo.subpassCount; s++)
        {
          MakeSubpassLoadRP(loadInfo, &CreateInfo, s);

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

    AddResource(RenderPass, ResourceType::RenderPass, "Render Pass");
    DerivedResource(device, RenderPass);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
                                           const VkAllocationCallbacks *, VkRenderPass *pRenderPass)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), pCreateInfo, NULL, pRenderPass));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pRenderPass);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateRenderPass);
        Serialise_vkCreateRenderPass(ser, device, pCreateInfo, NULL, pRenderPass);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pRenderPass);
      record->AddChunk(chunk);

      record->renderPassInfo = new RenderPassInfo(*pCreateInfo);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pRenderPass);

      VulkanCreationInfo::RenderPass rpinfo;
      rpinfo.Init(GetResourceManager(), m_CreationInfo, pCreateInfo);

      VkRenderPassCreateInfo info = *pCreateInfo;

      rdcarray<VkAttachmentDescription> atts;
      atts.resize(info.attachmentCount);

      // make a version of the render pass that loads from its attachments,
      // so it can be used for replaying a single draw after a render pass
      // without doing a clear or a DONT_CARE load.
      for(uint32_t i = 0; i < info.attachmentCount; i++)
      {
        atts[i] = info.pAttachments[i];
        if(atts[i].loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
          atts[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if(atts[i].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
          atts[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      }

      info.pAttachments = atts.data();

      // copy the dependencies so we can mutate them
      rdcarray<VkSubpassDependency> deps;
      deps.assign(info.pDependencies, info.dependencyCount);

      info.pDependencies = deps.data();

      {
        byte *tempMem = GetTempMemory(GetNextPatchSize(info.pNext));
        CopyNextChainForPatching("VkRenderPassCreateInfo", tempMem, (VkBaseInStructure *)&info);
      }

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateRenderPass2(SerialiserType &ser, VkDevice device,
                                                  const VkRenderPassCreateInfo2 *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkRenderPass *pRenderPass)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(RenderPass, GetResID(*pRenderPass)).TypedAs("VkRenderPass"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkRenderPass rp = VK_NULL_HANDLE;

    VulkanCreationInfo::RenderPass rpinfo;
    rpinfo.Init(GetResourceManager(), m_CreationInfo, &CreateInfo);

    // we want to store off the data so we can display it after the pass.
    // override any user-specified DONT_CARE.
    // Likewise we don't want to throw away data before we're ready, so change
    // any load ops to LOAD instead of DONT_CARE (which is valid!). We of course
    // leave any LOAD_OP_CLEAR alone.
    VkAttachmentDescription2 *att = (VkAttachmentDescription2 *)CreateInfo.pAttachments;
    for(uint32_t i = 0; i < CreateInfo.attachmentCount; i++)
    {
      if(att[i].storeOp != VK_ATTACHMENT_STORE_OP_NONE)
        att[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      if(att[i].stencilStoreOp != VK_ATTACHMENT_STORE_OP_NONE)
        att[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

      if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
      {
        if(att[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
          att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if(att[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
          att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      }

      if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
      {
        if(att[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
          att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if(att[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
          att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        if(att[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD &&
           att[i].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
          att[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        if(att[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD &&
           att[i].initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
          att[i].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
      }

      // renderpass can't start or end in presentable layout on replay
      SanitiseOldImageLayout(att[i].initialLayout);
      SanitiseNewImageLayout(att[i].finalLayout);

      // VK_KHR_separate_depth_stencil_layouts
      VkAttachmentDescriptionStencilLayout *separateStencil =
          (VkAttachmentDescriptionStencilLayout *)FindNextStruct(
              &att[i], VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT);

      if(separateStencil)
      {
        // there's not much to sanitise here but since we're loading, we sanitise undefined to
        // general.
        SanitiseOldImageLayout(separateStencil->stencilInitialLayout);
        SanitiseNewImageLayout(separateStencil->stencilFinalLayout);
      }
    }

    VkResult ret = ObjDisp(device)->CreateRenderPass2(Unwrap(device), &CreateInfo, NULL, &rp);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating render pass, VkResult: %s", ToStr(ret).c_str());
      return false;
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
        GetResourceManager()->ReplaceResource(RenderPass, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), rp);
        GetResourceManager()->AddLiveResource(RenderPass, rp);

        // make a version of the render pass that loads from its attachments,
        // so it can be used for replaying a single draw after a render pass
        // without doing a clear or a DONT_CARE load.
        for(uint32_t i = 0; i < CreateInfo.attachmentCount; i++)
        {
          if(att[i].loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
            att[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
          if(att[i].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
            att[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }

        VkRenderPassCreateInfo2 loadInfo = CreateInfo;

        {
          byte *tempMem = GetTempMemory(GetNextPatchSize(loadInfo.pNext));
          CopyNextChainForPatching("VkRenderPassCreateInfo2", tempMem,
                                   (VkBaseInStructure *)&loadInfo);
        }

        rpinfo.loadRPs.resize(CreateInfo.subpassCount);

        // create a render pass for each subpass that maintains attachment layouts
        for(uint32_t s = 0; s < CreateInfo.subpassCount; s++)
        {
          MakeSubpassLoadRP(loadInfo, &CreateInfo, s);

          ret = ObjDisp(device)->CreateRenderPass2(Unwrap(device), &loadInfo, NULL,
                                                   &rpinfo.loadRPs[s]);
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

    AddResource(RenderPass, ResourceType::RenderPass, "Render Pass");
    DerivedResource(device, RenderPass);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateRenderPass2(VkDevice device,
                                            const VkRenderPassCreateInfo2 *pCreateInfo,
                                            const VkAllocationCallbacks *, VkRenderPass *pRenderPass)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateRenderPass2(Unwrap(device), pCreateInfo, NULL, pRenderPass));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pRenderPass);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateRenderPass2);
        Serialise_vkCreateRenderPass2(ser, device, pCreateInfo, NULL, pRenderPass);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pRenderPass);
      record->AddChunk(chunk);

      record->renderPassInfo = new RenderPassInfo(*pCreateInfo);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pRenderPass);

      VulkanCreationInfo::RenderPass rpinfo;
      rpinfo.Init(GetResourceManager(), m_CreationInfo, pCreateInfo);

      VkRenderPassCreateInfo2 info = *pCreateInfo;

      rdcarray<VkAttachmentDescription2> atts;
      atts.resize(info.attachmentCount);

      // make a version of the render pass that loads from its attachments,
      // so it can be used for replaying a single draw after a render pass
      // without doing a clear or a DONT_CARE load.
      for(uint32_t i = 0; i < info.attachmentCount; i++)
      {
        atts[i] = info.pAttachments[i];
        if(atts[i].loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
          atts[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        if(atts[i].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
          atts[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      }

      info.pAttachments = atts.data();

      // copy the dependencies so we can mutate them
      rdcarray<VkSubpassDependency2> deps;
      deps.assign(info.pDependencies, info.dependencyCount);

      info.pDependencies = deps.data();

      {
        byte *tempMem = GetTempMemory(GetNextPatchSize(info.pNext));
        CopyNextChainForPatching("VkRenderPassCreateInfo2", tempMem, (VkBaseInStructure *)&info);
      }

      rpinfo.loadRPs.resize(pCreateInfo->subpassCount);

      // create a render pass for each subpass that maintains attachment layouts
      for(uint32_t s = 0; s < pCreateInfo->subpassCount; s++)
      {
        MakeSubpassLoadRP(info, pCreateInfo, s);

        ret = ObjDisp(device)->CreateRenderPass2(Unwrap(device), &info, NULL, &rpinfo.loadRPs[s]);
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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateQueryPool(SerialiserType &ser, VkDevice device,
                                                const VkQueryPoolCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkQueryPool *pQueryPool)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(QueryPool, GetResID(*pQueryPool)).TypedAs("VkQueryPool"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkQueryPool pool = VK_NULL_HANDLE;

    VkQueryPoolPerformanceCreateInfoKHR *perfInfo =
        (VkQueryPoolPerformanceCreateInfoKHR *)FindNextStruct(
            &CreateInfo, VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR);
    if(perfInfo)
    {
      perfInfo->queueFamilyIndex = m_QueueRemapping[perfInfo->queueFamilyIndex][0].family;
    }

    VkResult ret = ObjDisp(device)->CreateQueryPool(Unwrap(device), &CreateInfo, NULL, &pool);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating query pool, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
      GetResourceManager()->AddLiveResource(QueryPool, pool);

      m_CreationInfo.m_QueryPool[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);

      // We fill the query pool with valid but empty data, just so that future copies of query
      // results don't read from invalid data.

      VkCommandBuffer cmd = GetNextCmd();

      VkResult vkr = VK_SUCCESS;

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);

      ObjDisp(cmd)->CmdResetQueryPool(Unwrap(cmd), Unwrap(pool), 0, CreateInfo.queryCount);

      for(uint32_t i = 0; i < CreateInfo.queryCount; i++)
      {
        // Timestamps are easy - we can do these without needing to render
        if(CreateInfo.queryType == VK_QUERY_TYPE_TIMESTAMP)
        {
          ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          Unwrap(pool), i);
        }
        else
        {
          ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), Unwrap(pool), i, 0);
          ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), Unwrap(pool), i);
        }

        // split the command buffer and flush if the query pool is massive
        if(i > 0 && (i % (128 * 1024)) == 0)
        {
          vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
          CheckVkResult(vkr);

          SubmitCmds();
          FlushQ();

          cmd = GetNextCmd();

          vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          CheckVkResult(vkr);
        }
      }

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);
    }

    AddResource(QueryPool, ResourceType::Query, "Query Pool");
    DerivedResource(device, QueryPool);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *, VkQueryPool *pQueryPool)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateQueryPool(Unwrap(device), pCreateInfo, NULL, pQueryPool));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueryPool);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateQueryPool);
        Serialise_vkCreateQueryPool(ser, device, pCreateInfo, NULL, pQueryPool);

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkResetQueryPool(SerialiserType &ser, VkDevice device,
                                               VkQueryPool queryPool, uint32_t firstQuery,
                                               uint32_t queryCount)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(firstQuery);
  SERIALISE_ELEMENT(queryCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(device)->ResetQueryPool(Unwrap(device), Unwrap(queryPool), firstQuery, queryCount);
  }

  return true;
}

void WrappedVulkan::vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery,
                                     uint32_t queryCount)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(device)->ResetQueryPool(Unwrap(device), Unwrap(queryPool), firstQuery, queryCount));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkResetQueryPool);
    Serialise_vkResetQueryPool(ser, device, queryPool, firstQuery, queryCount);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateSamplerYcbcrConversion(
    SerialiserType &ser, VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkSamplerYcbcrConversion *pYcbcrConversion)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(ycbcrConversion, GetResID(*pYcbcrConversion))
      .TypedAs("VkSamplerYcbcrConversion"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSamplerYcbcrConversion conv = VK_NULL_HANDLE;

    VkResult ret =
        ObjDisp(device)->CreateSamplerYcbcrConversion(Unwrap(device), &CreateInfo, NULL, &conv);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Error creating YCbCr sampler, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(conv)))
      {
        live = GetResourceManager()->GetNonDispWrapper(conv)->id;

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroySamplerYcbcrConversion(Unwrap(device), conv, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(ycbcrConversion,
                                              GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), conv);
        GetResourceManager()->AddLiveResource(ycbcrConversion, conv);

        m_CreationInfo.m_YCbCrSampler[live].Init(GetResourceManager(), m_CreationInfo, &CreateInfo);
      }
    }

    AddResource(ycbcrConversion, ResourceType::Sampler, "YCbCr Sampler");
    DerivedResource(device, ycbcrConversion);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateSamplerYcbcrConversion(
    VkDevice device, const VkSamplerYcbcrConversionCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *, VkSamplerYcbcrConversion *pYcbcrConversion)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateSamplerYcbcrConversion(
                          Unwrap(device), pCreateInfo, NULL, pYcbcrConversion));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pYcbcrConversion);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateSamplerYcbcrConversion);
        Serialise_vkCreateSamplerYcbcrConversion(ser, device, pCreateInfo, NULL, pYcbcrConversion);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pYcbcrConversion);
      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pYcbcrConversion);
    }
  }

  return ret;
}

VkBool32 VKAPI_PTR UserDebugReportCallback(VkDebugReportFlagsEXT flags,
                                           VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                           size_t location, int32_t messageCode,
                                           const char *pLayerPrefix, const char *pMessage,
                                           void *pUserData)
{
  UserDebugReportCallbackData *user = (UserDebugReportCallbackData *)pUserData;

  if(RenderDoc::Inst().GetCaptureOptions().debugOutputMute)
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

VkBool32 VKAPI_PTR UserDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                                          const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                          void *pUserData)
{
  UserDebugUtilsCallbackData *user = (UserDebugUtilsCallbackData *)pUserData;

  if(RenderDoc::Inst().GetCaptureOptions().debugOutputMute)
  {
    if(user->muteWarned)
      return false;

    // once only insert a fake message notifying of the muting
    user->muteWarned = true;

    // we insert as an information message, since some trigger-happy applications might
    // debugbreak/crash/messagebox/etc on even warnings. This puts us in the same pool
    // as extremely spammy messages, but there's not much alternative.
    if(user->createInfo.messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT))
    {
      // use information type if possible, or if it's not accepted but debug is - use debug type.
      messageSeverity =
          (user->createInfo.messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
              ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
              : VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

      VkDebugUtilsMessengerCallbackDataEXT data = {};

      data.messageIdNumber = 1;
      data.pMessageIdName = NULL;
      data.pMessage =
          "While debugging through RenderDoc, debug output through validation layers is "
          "suppressed.\n"
          "To show debug output look at the 'DebugOutputMute' capture option in RenderDoc's API, "
          "but be aware of false positives from the validation layers.";
      data.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;

      user->createInfo.pfnUserCallback(messageSeverity, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                       &data, user->createInfo.pUserData);
    }

    return false;
  }

  return user->createInfo.pfnUserCallback(messageSeverity, messageType, pCallbackData,
                                          user->createInfo.pUserData);
}

VkResult WrappedVulkan::vkCreateDebugReportCallbackEXT(
    VkInstance instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *, VkDebugReportCallbackEXT *pCallback)
{
  // we create an interception object here so that we can dynamically check the state of API
  // messages being muted, since it's quite likely that the application will initialise Vulkan (and
  // so create a debug report callback) before it messes with RenderDoc's API to unmute messages.
  UserDebugReportCallbackData *user = new UserDebugReportCallbackData();
  user->wrappedInstance = instance;
  user->createInfo = *pCreateInfo;
  user->muteWarned = false;

  VkDebugReportCallbackCreateInfoEXT wrappedCreateInfo = *pCreateInfo;
  wrappedCreateInfo.pfnCallback = &UserDebugReportCallback;
  wrappedCreateInfo.pUserData = user;

  VkResult vkr = ObjDisp(instance)->CreateDebugReportCallbackEXT(
      Unwrap(instance), &wrappedCreateInfo, NULL, &user->realObject);

  if(vkr != VK_SUCCESS)
  {
    *pCallback = VK_NULL_HANDLE;
    delete user;
    return vkr;
  }

  {
    SCOPED_LOCK(m_CallbacksLock);
    m_ReportCallbacks.push_back(user);
  }

  *pCallback = (VkDebugReportCallbackEXT)(uint64_t)user;

  return vkr;
}

void WrappedVulkan::vkDestroyDebugReportCallbackEXT(VkInstance instance,
                                                    VkDebugReportCallbackEXT callback,
                                                    const VkAllocationCallbacks *)
{
  if(callback == VK_NULL_HANDLE)
    return;

  UserDebugReportCallbackData *user =
      (UserDebugReportCallbackData *)(uintptr_t)NON_DISP_TO_UINT64(callback);

  ObjDisp(instance)->DestroyDebugReportCallbackEXT(Unwrap(instance), user->realObject, NULL);

  {
    SCOPED_LOCK(m_CallbacksLock);
    m_ReportCallbacks.removeOne(user);
  }

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

void WrappedVulkan::vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount,
                                        const VkSwapchainKHR *pSwapchains,
                                        const VkHdrMetadataEXT *pMetadata)
{
  return ObjDisp(device)->SetHdrMetadataEXT(Unwrap(device), swapchainCount,
                                            UnwrapArray(pSwapchains, swapchainCount), pMetadata);
}

void WrappedVulkan::vkSetLocalDimmingAMD(VkDevice device, VkSwapchainKHR swapChain,
                                         VkBool32 localDimmingEnable)
{
  return ObjDisp(device)->SetLocalDimmingAMD(Unwrap(device), Unwrap(swapChain), localDimmingEnable);
}

// we use VkObjectType as the object type since it mostly overlaps with the debug report enum so in
// most cases we can upcast it. There's an overload to translate the few that might conflict.
// Likewise to re-use the switch in most cases, we return both the record and the unwrapped
// object at once. In *most* cases the unwrapped object comes from the record, but there are a few
// exceptions which don't have records, or aren't wrapped, etc.
struct ObjData
{
  VkResourceRecord *record;
  uint64_t unwrapped;
};

static ObjData GetObjData(VkObjectType objType, uint64_t object)
{
  ObjData ret = {};

  switch(objType)
  {
    case VK_OBJECT_TYPE_INSTANCE: ret.record = GetRecord((VkInstance)object); break;
    case VK_OBJECT_TYPE_PHYSICAL_DEVICE: ret.record = GetRecord((VkPhysicalDevice)object); break;
    case VK_OBJECT_TYPE_DEVICE: ret.record = GetRecord((VkDevice)object); break;
    case VK_OBJECT_TYPE_QUEUE: ret.record = GetRecord((VkQueue)object); break;
    case VK_OBJECT_TYPE_SEMAPHORE: ret.record = GetRecord((VkSemaphore)object); break;
    case VK_OBJECT_TYPE_COMMAND_BUFFER: ret.record = GetRecord((VkCommandBuffer)object); break;
    case VK_OBJECT_TYPE_FENCE: ret.record = GetRecord((VkFence)object); break;
    case VK_OBJECT_TYPE_DEVICE_MEMORY: ret.record = GetRecord((VkDeviceMemory)object); break;
    case VK_OBJECT_TYPE_BUFFER: ret.record = GetRecord((VkBuffer)object); break;
    case VK_OBJECT_TYPE_IMAGE: ret.record = GetRecord((VkImage)object); break;
    case VK_OBJECT_TYPE_EVENT: ret.record = GetRecord((VkEvent)object); break;
    case VK_OBJECT_TYPE_QUERY_POOL: ret.record = GetRecord((VkQueryPool)object); break;
    case VK_OBJECT_TYPE_BUFFER_VIEW: ret.record = GetRecord((VkBufferView)object); break;
    case VK_OBJECT_TYPE_IMAGE_VIEW: ret.record = GetRecord((VkImageView)object); break;
    case VK_OBJECT_TYPE_SHADER_MODULE: ret.record = GetRecord((VkShaderModule)object); break;
    case VK_OBJECT_TYPE_PIPELINE_CACHE: ret.record = GetRecord((VkPipelineCache)object); break;
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT: ret.record = GetRecord((VkPipelineLayout)object); break;
    case VK_OBJECT_TYPE_RENDER_PASS: ret.record = GetRecord((VkRenderPass)object); break;
    case VK_OBJECT_TYPE_PIPELINE: ret.record = GetRecord((VkPipeline)object); break;
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
      ret.record = GetRecord((VkDescriptorSetLayout)object);
      break;
    case VK_OBJECT_TYPE_SAMPLER: ret.record = GetRecord((VkSampler)object); break;
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL: ret.record = GetRecord((VkDescriptorPool)object); break;
    case VK_OBJECT_TYPE_DESCRIPTOR_SET: ret.record = GetRecord((VkDescriptorSet)object); break;
    case VK_OBJECT_TYPE_FRAMEBUFFER: ret.record = GetRecord((VkFramebuffer)object); break;
    case VK_OBJECT_TYPE_COMMAND_POOL: ret.record = GetRecord((VkCommandPool)object); break;

    case VK_OBJECT_TYPE_SWAPCHAIN_KHR: ret.record = GetRecord((VkSwapchainKHR)object); break;
    case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
      ret.record = GetRecord((VkDescriptorUpdateTemplate)object);
      break;
    case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
      ret.record = GetRecord((VkSamplerYcbcrConversion)object);
      break;
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
      ret.record = GetRecord((VkAccelerationStructureKHR)object);
      break;

    /////////////////////////////
    // special cases

    // VkSurfaceKHR doesn't have a record
    case VK_OBJECT_TYPE_SURFACE_KHR:
      ret.unwrapped = GetWrapped((VkSurfaceKHR)object)->real.handle;
      break;

    // VkDisplayKHR, VkDisplayModeKHR, and VkValidationCacheEXT are not wrapped
    case VK_OBJECT_TYPE_DISPLAY_KHR:
    case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
    case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT: ret.unwrapped = object; break;

    // debug report callback and messenger are not wrapped in the conventional way
    case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
    {
      UserDebugReportCallbackData *user = (UserDebugReportCallbackData *)(uintptr_t)object;
      ret.unwrapped = NON_DISP_TO_UINT64(user->realObject);
      break;
    }

    case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
    {
      UserDebugUtilsCallbackData *user = (UserDebugUtilsCallbackData *)(uintptr_t)object;
      ret.unwrapped = NON_DISP_TO_UINT64(user->realObject);
      break;
    }

    // private data slots and deferred host operations are not wrapped
    case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT:
    case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR: ret.unwrapped = object; break;

    // these objects are not supported
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
    case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:
    case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
    case VK_OBJECT_TYPE_CUDA_MODULE_NV:
    case VK_OBJECT_TYPE_CUDA_FUNCTION_NV:
    case VK_OBJECT_TYPE_CU_MODULE_NVX:
    case VK_OBJECT_TYPE_CU_FUNCTION_NVX:
    case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA:
    case VK_OBJECT_TYPE_MICROMAP_EXT:
    case VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV:
    case VK_OBJECT_TYPE_SHADER_EXT:
    case VK_OBJECT_TYPE_UNKNOWN:
    case VK_OBJECT_TYPE_VIDEO_SESSION_KHR:
    case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR:
    case VK_OBJECT_TYPE_MAX_ENUM: break;
  }

  // if we have a record and no unwrapped object, fetch it out of the record
  if(ret.record && ret.unwrapped == 0)
  {
    switch(objType)
    {
      // dispatchable objects
      case VK_OBJECT_TYPE_INSTANCE:
      case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
      case VK_OBJECT_TYPE_QUEUE:
      case VK_OBJECT_TYPE_DEVICE:
      case VK_OBJECT_TYPE_COMMAND_BUFFER:
      {
        WrappedVkDispRes *res = (WrappedVkDispRes *)ret.record->Resource;
        ret.unwrapped = res->real.handle;
        break;
      }

      // non-dispatchable objects
      default:
      {
        WrappedVkNonDispRes *res = (WrappedVkNonDispRes *)ret.record->Resource;
        ret.unwrapped = res->real.handle;
        break;
      }
    }
  }

  return ret;
}

// overload to silently cast
static ObjData GetObjData(VkDebugReportObjectTypeEXT objType, uint64_t object)
{
  VkObjectType castType = (VkObjectType)objType;

  // a few special cases don't overlap and will conflict when more core objects are added to
  // VkObjectType so we don't cast them.
  if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT)
    castType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT)
    castType = VK_OBJECT_TYPE_SURFACE_KHR;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT)
    castType = VK_OBJECT_TYPE_DISPLAY_KHR;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT)
    castType = VK_OBJECT_TYPE_DISPLAY_MODE_KHR;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT)
    castType = VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT)
    castType = VK_OBJECT_TYPE_VALIDATION_CACHE_EXT;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT)
    castType = VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT)
    castType = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR_EXT)
    castType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV_EXT)
    castType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_CUDA_MODULE_NV_EXT)
    castType = VK_OBJECT_TYPE_CUDA_MODULE_NV;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_CUDA_FUNCTION_NV_EXT)
    castType = VK_OBJECT_TYPE_CUDA_FUNCTION_NV;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_CU_MODULE_NVX_EXT)
    castType = VK_OBJECT_TYPE_CU_MODULE_NVX;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_CU_FUNCTION_NVX_EXT)
    castType = VK_OBJECT_TYPE_CU_FUNCTION_NVX;
  else if(objType == VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA_EXT)
    castType = VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA;

  return GetObjData(castType, object);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_SetShaderDebugPath(SerialiserType &ser, VkShaderModule ShaderObject,
                                                 rdcstr DebugPath)
{
  SERIALISE_ELEMENT(ShaderObject).Important();
  SERIALISE_ELEMENT(DebugPath).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_CreationInfo.m_ShaderModule[GetResID(ShaderObject)].unstrippedPath = DebugPath;
    m_CreationInfo.m_ShaderModule[GetResID(ShaderObject)].Reinit();

    AddResourceCurChunk(GetResourceManager()->GetOriginalID(GetResID(ShaderObject)));
  }

  return true;
}

VkResult WrappedVulkan::vkDebugMarkerSetObjectTagEXT(VkDevice device,
                                                     const VkDebugMarkerObjectTagInfoEXT *pTagInfo)
{
  if(IsCaptureMode(m_State) && pTagInfo)
  {
    ObjData data = GetObjData(pTagInfo->objectType, pTagInfo->object);

    if(data.record && pTagInfo->tagName == RENDERDOC_ShaderDebugMagicValue_truncated &&
       pTagInfo->objectType == VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT)
    {
      CACHE_THREAD_SERIALISER();

      rdcstr DebugPath = rdcstr((char *)pTagInfo->pTag, pTagInfo->tagSize);

      SCOPED_SERIALISE_CHUNK(VulkanChunk::SetShaderDebugPath);
      Serialise_SetShaderDebugPath(ser, (VkShaderModule)(uint64_t)data.record->Resource, DebugPath);
      data.record->AddChunk(scope.Get());
    }
    else if(data.record && pTagInfo->tagName == VR_ThumbnailTag_UUID &&
            pTagInfo->objectType == VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT)
    {
      m_CurrentVRBackbuffer = data.record->GetResourceID();
    }
    else if(ObjDisp(device)->DebugMarkerSetObjectTagEXT)
    {
      VkDebugMarkerObjectTagInfoEXT unwrapped = *pTagInfo;

      unwrapped.object = data.unwrapped;

      return ObjDisp(device)->DebugMarkerSetObjectTagEXT(Unwrap(device), &unwrapped);
    }
  }

  return VK_SUCCESS;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkDebugMarkerSetObjectNameEXT(
    SerialiserType &ser, VkDevice device, const VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
  SERIALISE_ELEMENT_LOCAL(
      Object, GetObjData(pNameInfo->objectType, pNameInfo->object).record->GetResourceID())
      .Important();
  SERIALISE_ELEMENT_LOCAL(ObjectName, pNameInfo->pObjectName).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // silently promote NULL name to empty string
    if(ObjectName == NULL)
      ObjectName = "";

    // if we don't have a live resource, this is probably a command buffer being named on the
    // virtual non-existant parent, not any of the baked IDs. Just save the name on the original ID
    // and we'll propagate it in Serialise_vkBeginCommandBuffer
    if(!GetResourceManager()->HasLiveResource(Object) || GetResourceManager()->HasReplacement(Object))
      m_CreationInfo.m_Names[Object] = ObjectName;
    else
      m_CreationInfo.m_Names[GetResourceManager()->GetLiveID(Object)] = ObjectName;

    VkDebugMarkerObjectNameInfoEXT name = {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT};
    name.pObjectName = ObjectName;
    WrappedVkRes *res = GetResourceManager()->GetLiveResource(Object);

    if(res)
    {
      if(IsDispatchableRes(res))
      {
        WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
        name.object = disp->real.handle;
      }
      else
      {
        WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;
        name.object = nondisp->real.handle;
      }

      VkDebugReportObjectTypeEXT type = VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT;

      switch(IdentifyTypeByPtr(res))
      {
        case eResUnknown: type = VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT; break;
        case eResPhysicalDevice: type = VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT; break;
        case eResInstance: type = VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT; break;
        case eResDevice: type = VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT; break;
        case eResQueue: type = VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT; break;
        case eResDeviceMemory: type = VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT; break;
        case eResBuffer: type = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT; break;
        case eResBufferView: type = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT; break;
        case eResImage: type = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT; break;
        case eResImageView: type = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT; break;
        case eResFramebuffer: type = VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT; break;
        case eResRenderPass: type = VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT; break;
        case eResShaderModule: type = VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT; break;
        case eResPipelineCache: type = VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT; break;
        case eResPipelineLayout: type = VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT; break;
        case eResPipeline: type = VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT; break;
        case eResSampler: type = VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT; break;
        case eResDescriptorPool: type = VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT; break;
        case eResDescriptorSetLayout:
          type = VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT;
          break;
        case eResDescriptorSet: type = VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT; break;
        case eResCommandPool: type = VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT; break;
        case eResCommandBuffer: type = VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT; break;
        case eResFence: type = VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT; break;
        case eResEvent: type = VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT; break;
        case eResQueryPool: type = VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT; break;
        case eResSemaphore: type = VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT; break;
        case eResSwapchain: type = VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT; break;
        case eResSurface: type = VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT; break;
        case eResDescUpdateTemplate:
          type = VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT;
          break;
        case eResSamplerConversion:
          type = VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT;
          break;
        case eResAccelerationStructureKHR:
          type = VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR_EXT;
          break;
      }

      if(ObjDisp(m_Device)->DebugMarkerSetObjectNameEXT &&
         type != VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT)
      {
        name.objectType = type;
        ObjDisp(m_Device)->DebugMarkerSetObjectNameEXT(Unwrap(m_Device), &name);
      }
    }

    ResourceDescription &descr = GetResourceDesc(Object);

    AddResourceCurChunk(descr);
    if(ObjectName[0])
      descr.SetCustomName(ObjectName);
  }

  return true;
}

VkResult WrappedVulkan::vkDebugMarkerSetObjectNameEXT(VkDevice device,
                                                      const VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
  if(IsCaptureMode(m_State) && pNameInfo)
  {
    ObjData data = GetObjData(pNameInfo->objectType, pNameInfo->object);

    VkDebugMarkerObjectNameInfoEXT unwrapped = *pNameInfo;
    unwrapped.object = data.unwrapped;

    if(ObjDisp(device)->DebugMarkerSetObjectNameEXT)
      ObjDisp(device)->DebugMarkerSetObjectNameEXT(Unwrap(device), &unwrapped);

    if(data.record)
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkDebugMarkerSetObjectNameEXT);
      Serialise_vkDebugMarkerSetObjectNameEXT(ser, device, pNameInfo);

      Chunk *chunk = scope.Get();

      data.record->AddChunk(chunk);
    }
  }

  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *, VkDebugUtilsMessengerEXT *pMessenger)
{
  // we create an interception object here so that we can dynamically check the state of API
  // messages being muted, since it's quite likely that the application will initialise Vulkan (and
  // so create a debug report callback) before it messes with RenderDoc's API to unmute messages.
  UserDebugUtilsCallbackData *user = new UserDebugUtilsCallbackData();
  user->createInfo = *pCreateInfo;
  user->muteWarned = false;

  VkDebugUtilsMessengerCreateInfoEXT wrappedCreateInfo = *pCreateInfo;
  wrappedCreateInfo.pfnUserCallback = &UserDebugUtilsCallback;
  wrappedCreateInfo.pUserData = user;

  VkResult vkr = ObjDisp(instance)->CreateDebugUtilsMessengerEXT(
      Unwrap(instance), &wrappedCreateInfo, NULL, &user->realObject);

  if(vkr != VK_SUCCESS)
  {
    *pMessenger = VK_NULL_HANDLE;
    delete user;
    return vkr;
  }

  {
    SCOPED_LOCK(m_CallbacksLock);
    m_UtilsCallbacks.push_back(user);
  }

  *pMessenger = (VkDebugUtilsMessengerEXT)(uint64_t)user;

  return vkr;
}

void WrappedVulkan::vkDestroyDebugUtilsMessengerEXT(VkInstance instance,
                                                    VkDebugUtilsMessengerEXT messenger,
                                                    const VkAllocationCallbacks *)
{
  if(messenger == VK_NULL_HANDLE)
    return;

  UserDebugUtilsCallbackData *user =
      (UserDebugUtilsCallbackData *)(uintptr_t)NON_DISP_TO_UINT64(messenger);

  ObjDisp(instance)->DestroyDebugUtilsMessengerEXT(Unwrap(instance), user->realObject, NULL);

  {
    SCOPED_LOCK(m_CallbacksLock);
    m_UtilsCallbacks.removeOne(user);
  }

  delete user;
}

void WrappedVulkan::vkSubmitDebugUtilsMessageEXT(
    VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
  return ObjDisp(instance)->SubmitDebugUtilsMessageEXT(Unwrap(instance), messageSeverity,
                                                       messageTypes, pCallbackData);
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkSetDebugUtilsObjectNameEXT(
    SerialiserType &ser, VkDevice device, const VkDebugUtilsObjectNameInfoEXT *pNameInfo)
{
  SERIALISE_ELEMENT_LOCAL(
      Object, GetObjData(pNameInfo->objectType, pNameInfo->objectHandle).record->GetResourceID())
      .Important();
  SERIALISE_ELEMENT_LOCAL(ObjectName, pNameInfo->pObjectName).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // silently promote NULL name to empty string
    if(ObjectName == NULL)
      ObjectName = "";

    // if we don't have a live resource, this is probably a command buffer being named on the
    // virtual non-existant parent, not any of the baked IDs. Just save the name on the original ID
    // and we'll propagate it in Serialise_vkBeginCommandBuffer
    if(!GetResourceManager()->HasLiveResource(Object) || GetResourceManager()->HasReplacement(Object))
      m_CreationInfo.m_Names[Object] = ObjectName;
    else
      m_CreationInfo.m_Names[GetResourceManager()->GetLiveID(Object)] = ObjectName;

    VkDebugUtilsObjectNameInfoEXT name = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name.pObjectName = ObjectName;
    WrappedVkRes *res = GetResourceManager()->GetLiveResource(Object, true);

    if(res)
    {
      if(IsDispatchableRes(res))
      {
        WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
        name.objectHandle = disp->real.handle;
      }
      else
      {
        WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;
        name.objectHandle = nondisp->real.handle;
      }

      VkObjectType type = VK_OBJECT_TYPE_UNKNOWN;

      switch(IdentifyTypeByPtr(res))
      {
        case eResUnknown: type = VK_OBJECT_TYPE_UNKNOWN; break;
        case eResPhysicalDevice: type = VK_OBJECT_TYPE_PHYSICAL_DEVICE; break;
        case eResInstance: type = VK_OBJECT_TYPE_INSTANCE; break;
        case eResDevice: type = VK_OBJECT_TYPE_DEVICE; break;
        case eResQueue: type = VK_OBJECT_TYPE_QUEUE; break;
        case eResDeviceMemory: type = VK_OBJECT_TYPE_DEVICE_MEMORY; break;
        case eResBuffer: type = VK_OBJECT_TYPE_BUFFER; break;
        case eResBufferView: type = VK_OBJECT_TYPE_BUFFER_VIEW; break;
        case eResImage: type = VK_OBJECT_TYPE_IMAGE; break;
        case eResImageView: type = VK_OBJECT_TYPE_IMAGE_VIEW; break;
        case eResFramebuffer: type = VK_OBJECT_TYPE_FRAMEBUFFER; break;
        case eResRenderPass: type = VK_OBJECT_TYPE_RENDER_PASS; break;
        case eResShaderModule: type = VK_OBJECT_TYPE_SHADER_MODULE; break;
        case eResPipelineCache: type = VK_OBJECT_TYPE_PIPELINE_CACHE; break;
        case eResPipelineLayout: type = VK_OBJECT_TYPE_PIPELINE_LAYOUT; break;
        case eResPipeline: type = VK_OBJECT_TYPE_PIPELINE; break;
        case eResSampler: type = VK_OBJECT_TYPE_SAMPLER; break;
        case eResDescriptorPool: type = VK_OBJECT_TYPE_DESCRIPTOR_POOL; break;
        case eResDescriptorSetLayout: type = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT; break;
        case eResDescriptorSet: type = VK_OBJECT_TYPE_DESCRIPTOR_SET; break;
        case eResCommandPool: type = VK_OBJECT_TYPE_COMMAND_POOL; break;
        case eResCommandBuffer: type = VK_OBJECT_TYPE_COMMAND_BUFFER; break;
        case eResFence: type = VK_OBJECT_TYPE_FENCE; break;
        case eResEvent: type = VK_OBJECT_TYPE_EVENT; break;
        case eResQueryPool: type = VK_OBJECT_TYPE_QUERY_POOL; break;
        case eResSemaphore: type = VK_OBJECT_TYPE_SEMAPHORE; break;
        case eResSwapchain: type = VK_OBJECT_TYPE_SWAPCHAIN_KHR; break;
        case eResSurface: type = VK_OBJECT_TYPE_SURFACE_KHR; break;
        case eResDescUpdateTemplate: type = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE; break;
        case eResSamplerConversion: type = VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION; break;
        case eResAccelerationStructureKHR: type = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR; break;
      }

      if(ObjDisp(m_Device)->SetDebugUtilsObjectNameEXT && type != VK_OBJECT_TYPE_UNKNOWN &&
         type != VK_OBJECT_TYPE_PHYSICAL_DEVICE)
      {
        name.objectType = type;
        ObjDisp(m_Device)->SetDebugUtilsObjectNameEXT(Unwrap(m_Device), &name);
      }
    }

    ResourceDescription &descr = GetResourceDesc(Object);

    AddResourceCurChunk(descr);
    if(ObjectName[0])
      descr.SetCustomName(ObjectName);
  }

  return true;
}

VkResult WrappedVulkan::vkSetDebugUtilsObjectNameEXT(VkDevice device,
                                                     const VkDebugUtilsObjectNameInfoEXT *pNameInfo)
{
  if(IsCaptureMode(m_State) && pNameInfo)
  {
    ObjData data = GetObjData(pNameInfo->objectType, pNameInfo->objectHandle);

    VkDebugUtilsObjectNameInfoEXT unwrapped = *pNameInfo;
    unwrapped.objectHandle = data.unwrapped;

    if(ObjDisp(device)->SetDebugUtilsObjectNameEXT)
      ObjDisp(device)->SetDebugUtilsObjectNameEXT(Unwrap(device), &unwrapped);

    if(data.record)
    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkSetDebugUtilsObjectNameEXT);
      Serialise_vkSetDebugUtilsObjectNameEXT(ser, device, pNameInfo);

      Chunk *chunk = scope.Get();

      data.record->AddChunk(chunk);
    }
  }

  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkSetDebugUtilsObjectTagEXT(VkDevice device,
                                                    const VkDebugUtilsObjectTagInfoEXT *pTagInfo)
{
  if(IsCaptureMode(m_State) && pTagInfo)
  {
    ObjData data = GetObjData(pTagInfo->objectType, pTagInfo->objectHandle);

    if(data.record && pTagInfo->tagName == RENDERDOC_ShaderDebugMagicValue_truncated &&
       pTagInfo->objectType == VK_OBJECT_TYPE_SHADER_MODULE)
    {
      CACHE_THREAD_SERIALISER();

      rdcstr DebugPath = rdcstr((char *)pTagInfo->pTag, pTagInfo->tagSize);

      SCOPED_SERIALISE_CHUNK(VulkanChunk::SetShaderDebugPath);
      Serialise_SetShaderDebugPath(ser, (VkShaderModule)(uint64_t)data.record->Resource, DebugPath);
      data.record->AddChunk(scope.Get());
    }
    else if(data.record && pTagInfo->tagName == VR_ThumbnailTag_UUID &&
            pTagInfo->objectType == VK_OBJECT_TYPE_IMAGE)
    {
      m_CurrentVRBackbuffer = data.record->GetResourceID();
    }
    else if(ObjDisp(device)->SetDebugUtilsObjectTagEXT)
    {
      VkDebugUtilsObjectTagInfoEXT unwrapped = *pTagInfo;

      unwrapped.objectHandle = data.unwrapped;

      return ObjDisp(device)->SetDebugUtilsObjectTagEXT(Unwrap(device), &unwrapped);
    }
  }

  return VK_SUCCESS;
}

VkResult WrappedVulkan::vkGetRefreshCycleDurationGOOGLE(
    VkDevice device, VkSwapchainKHR swapchain, VkRefreshCycleDurationGOOGLE *pDisplayTimingProperties)
{
  return ObjDisp(device)->GetRefreshCycleDurationGOOGLE(Unwrap(device), Unwrap(swapchain),
                                                        pDisplayTimingProperties);
}

VkResult WrappedVulkan::vkGetPastPresentationTimingGOOGLE(
    VkDevice device, VkSwapchainKHR swapchain, uint32_t *pPresentationTimingCount,
    VkPastPresentationTimingGOOGLE *pPresentationTimings)
{
  return ObjDisp(device)->GetPastPresentationTimingGOOGLE(
      Unwrap(device), Unwrap(swapchain), pPresentationTimingCount, pPresentationTimings);
}

VkResult WrappedVulkan::vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, uint32_t *pCounterCount,
    VkPerformanceCounterKHR *pCounters, VkPerformanceCounterDescriptionKHR *pCounterDescriptions)
{
  return ObjDisp(physicalDevice)
      ->EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
          Unwrap(physicalDevice), queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions);
}

void WrappedVulkan::vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(
    VkPhysicalDevice physicalDevice,
    const VkQueryPoolPerformanceCreateInfoKHR *pPerformanceQueryCreateInfo, uint32_t *pNumPasses)
{
  ObjDisp(physicalDevice)
      ->GetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(
          Unwrap(physicalDevice), pPerformanceQueryCreateInfo, pNumPasses);
}

VkResult WrappedVulkan::vkAcquireProfilingLockKHR(VkDevice device,
                                                  const VkAcquireProfilingLockInfoKHR *pInfo)
{
  return ObjDisp(device)->AcquireProfilingLockKHR(Unwrap(device), pInfo);
}

void WrappedVulkan::vkReleaseProfilingLockKHR(VkDevice device)
{
  ObjDisp(device)->ReleaseProfilingLockKHR(Unwrap(device));
}

VkResult WrappedVulkan::vkCreatePrivateDataSlot(VkDevice device,
                                                const VkPrivateDataSlotCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *,
                                                VkPrivateDataSlot *pPrivateDataSlot)
{
  // don't even wrap the slot, keep it unwrapped since we don't care about it
  return ObjDisp(device)->CreatePrivateDataSlot(Unwrap(device), pCreateInfo, NULL, pPrivateDataSlot);
}

void WrappedVulkan::vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot,
                                             const VkAllocationCallbacks *)
{
  return ObjDisp(device)->DestroyPrivateDataSlot(Unwrap(device), privateDataSlot, NULL);
}

VkResult WrappedVulkan::vkSetPrivateData(VkDevice device, VkObjectType objectType,
                                         uint64_t objectHandle, VkPrivateDataSlot privateDataSlot,
                                         uint64_t data)
{
  ObjData objdata = GetObjData(objectType, objectHandle);

  return ObjDisp(device)->SetPrivateData(Unwrap(device), objectType, objdata.unwrapped,
                                         privateDataSlot, data);
}

void WrappedVulkan::vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle,
                                     VkPrivateDataSlot privateDataSlot, uint64_t *pData)
{
  ObjData objdata = GetObjData(objectType, objectHandle);

  return ObjDisp(device)->GetPrivateData(Unwrap(device), objectType, objdata.unwrapped,
                                         privateDataSlot, pData);
}

// deferred host operations are not supported, host operations are always captured non-deferred,
// so these wrapped functions are just pass-throughs
VkResult WrappedVulkan::vkCreateDeferredOperationKHR(VkDevice device, const VkAllocationCallbacks *,
                                                     VkDeferredOperationKHR *pDeferredOperation)
{
  return ObjDisp(device)->CreateDeferredOperationKHR(Unwrap(device), NULL, pDeferredOperation);
}

VkResult WrappedVulkan::vkDeferredOperationJoinKHR(VkDevice device, VkDeferredOperationKHR operation)
{
  return ObjDisp(device)->DeferredOperationJoinKHR(Unwrap(device), operation);
}

void WrappedVulkan::vkDestroyDeferredOperationKHR(VkDevice device, VkDeferredOperationKHR operation,
                                                  const VkAllocationCallbacks *)
{
  return ObjDisp(device)->DestroyDeferredOperationKHR(Unwrap(device), operation, NULL);
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateSampler, VkDevice device,
                                const VkSamplerCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkSampler *pSampler);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateFramebuffer, VkDevice device,
                                const VkFramebufferCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkFramebuffer *pFramebuffer);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateRenderPass, VkDevice device,
                                const VkRenderPassCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkRenderPass *pRenderPass);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateRenderPass2, VkDevice device,
                                const VkRenderPassCreateInfo2 *pCreateInfo,
                                const VkAllocationCallbacks *, VkRenderPass *pRenderPass);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateQueryPool, VkDevice device,
                                const VkQueryPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkQueryPool *pQueryPool);

INSTANTIATE_FUNCTION_SERIALISED(void, SetShaderDebugPath, VkShaderModule ShaderObject,
                                rdcstr DebugPath);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkDebugMarkerSetObjectNameEXT, VkDevice device,
                                const VkDebugMarkerObjectNameInfoEXT *pNameInfo);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkSetDebugUtilsObjectNameEXT, VkDevice device,
                                const VkDebugUtilsObjectNameInfoEXT *pNameInfo);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateSamplerYcbcrConversion, VkDevice device,
                                const VkSamplerYcbcrConversionCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *,
                                VkSamplerYcbcrConversion *pYcbcrConversion);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkResetQueryPool, VkDevice device, VkQueryPool queryPool,
                                uint32_t firstQuery, uint32_t queryCount);
