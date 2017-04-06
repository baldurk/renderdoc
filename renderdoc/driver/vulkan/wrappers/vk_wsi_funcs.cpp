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
#include "../vk_debug.h"

///////////////////////////////////////////////////////////////////////////////////////
// WSI extension

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                                             uint32_t queueFamilyIndex,
                                                             VkSurfaceKHR surface,
                                                             VkBool32 *pSupported)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSurfaceSupportKHR(Unwrap(physicalDevice), queueFamilyIndex,
                                           Unwrap(surface), pSupported);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSurfaceCapabilitiesKHR(Unwrap(physicalDevice), Unwrap(surface),
                                                pSurfaceCapabilities);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                                             VkSurfaceKHR surface,
                                                             uint32_t *pSurfaceFormatCount,
                                                             VkSurfaceFormatKHR *pSurfaceFormats)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSurfaceFormatsKHR(Unwrap(physicalDevice), Unwrap(surface),
                                           pSurfaceFormatCount, pSurfaceFormats);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice,
                                                                  VkSurfaceKHR surface,
                                                                  uint32_t *pPresentModeCount,
                                                                  VkPresentModeKHR *pPresentModes)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSurfacePresentModesKHR(Unwrap(physicalDevice), Unwrap(surface),
                                                pPresentModeCount, pPresentModes);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceCapabilities2EXT(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilities2EXT *pSurfaceCapabilities)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceSurfaceCapabilities2EXT(Unwrap(physicalDevice), Unwrap(surface),
                                                 pSurfaceCapabilities);
}

VkResult WrappedVulkan::vkDisplayPowerControlEXT(VkDevice device, VkDisplayKHR display,
                                                 const VkDisplayPowerInfoEXT *pDisplayPowerInfo)
{
  // displays are not wrapped
  return ObjDisp(device)->DisplayPowerControlEXT(Unwrap(device), display, pDisplayPowerInfo);
}

VkResult WrappedVulkan::vkGetSwapchainCounterEXT(VkDevice device, VkSwapchainKHR swapchain,
                                                 VkSurfaceCounterFlagBitsEXT counter,
                                                 uint64_t *pCounterValue)
{
  return ObjDisp(device)->GetSwapchainCounterEXT(Unwrap(device), Unwrap(swapchain), counter,
                                                 pCounterValue);
}

VkResult WrappedVulkan::vkRegisterDeviceEventEXT(VkDevice device,
                                                 const VkDeviceEventInfoEXT *pDeviceEventInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkFence *pFence)
{
  // for now we emulate this on replay as just a regular fence create, since we don't faithfully
  // replay sync events anyway.
  VkResult ret =
      ObjDisp(device)->RegisterDeviceEventEXT(Unwrap(device), pDeviceEventInfo, pAllocator, pFence);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFence);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        VkFenceCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT,
        };

        SCOPED_SERIALISE_CONTEXT(CREATE_FENCE);
        Serialise_vkCreateFence(localSerialiser, device, &createInfo, NULL, pFence);

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

VkResult WrappedVulkan::vkRegisterDisplayEventEXT(VkDevice device, VkDisplayKHR display,
                                                  const VkDisplayEventInfoEXT *pDisplayEventInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkFence *pFence)
{
  // for now we emulate this on replay as just a regular fence create, since we don't faithfully
  // replay sync events anyway.
  VkResult ret = ObjDisp(device)->RegisterDisplayEventEXT(Unwrap(device), display,
                                                          pDisplayEventInfo, pAllocator, pFence);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFence);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        VkFenceCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT,
        };

        SCOPED_SERIALISE_CONTEXT(CREATE_FENCE);
        Serialise_vkCreateFence(localSerialiser, device, &createInfo, NULL, pFence);

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

bool WrappedVulkan::Serialise_vkGetSwapchainImagesKHR(Serialiser *localSerialiser, VkDevice device,
                                                      VkSwapchainKHR swapchain, uint32_t *pCount,
                                                      VkImage *pSwapchainImages)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, swapId, GetResID(swapchain));
  SERIALISE_ELEMENT(uint32_t, idx, *pCount);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapchainImages));

  if(m_State == READING)
  {
    // use original ID because we don't create a live version of the swapchain
    auto &swapInfo = m_CreationInfo.m_SwapChain[swapId];

    RDCASSERT(idx < swapInfo.images.size(), idx, swapInfo.images.size());
    GetResourceManager()->AddLiveResource(id, swapInfo.images[idx].im);

    m_CreationInfo.m_Image[GetResID(swapInfo.images[idx].im)] = m_CreationInfo.m_Image[swapId];
  }

  return true;
}

VkResult WrappedVulkan::vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                uint32_t *pCount, VkImage *pSwapchainImages)
{
  // make sure we always get the size
  uint32_t dummySize = 0;
  if(pCount == NULL)
    pCount = &dummySize;

  VkResult ret = ObjDisp(device)->GetSwapchainImagesKHR(Unwrap(device), Unwrap(swapchain), pCount,
                                                        pSwapchainImages);

  if(pSwapchainImages && m_State >= WRITING)
  {
    uint32_t numImages = *pCount;

    VkResourceRecord *swapRecord = GetRecord(swapchain);

    for(uint32_t i = 0; i < numImages; i++)
    {
      // these were all wrapped and serialised on swapchain create - we just have to
      // return the wrapped image in that case
      if(swapRecord->swapInfo->images[i].im != VK_NULL_HANDLE)
      {
        pSwapchainImages[i] = swapRecord->swapInfo->images[i].im;
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
            Serialise_vkGetSwapchainImagesKHR(localSerialiser, device, swapchain, &i,
                                              &pSwapchainImages[i]);

            chunk = scope.Get();
          }

          VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pSwapchainImages[i]);
          VkResourceRecord *swaprecord = GetRecord(swapchain);

          record->SpecialResource = true;

          record->AddParent(swaprecord);

          // note we add the chunk to the swap record, that way when the swapchain is created it
          // will
          // always create all of its images on replay. The image's record is kept around for
          // reference
          // tracking and any other chunks. Because it has a parent relationship on the swapchain,
          // if
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

VkResult WrappedVulkan::vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                              uint64_t timeout, VkSemaphore semaphore,
                                              VkFence fence, uint32_t *pImageIndex)
{
  return ObjDisp(device)->AcquireNextImageKHR(Unwrap(device), Unwrap(swapchain), timeout,
                                              Unwrap(semaphore), Unwrap(fence), pImageIndex);
}

bool WrappedVulkan::Serialise_vkCreateSwapchainKHR(Serialiser *localSerialiser, VkDevice device,
                                                   const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkSwapchainKHR *pSwapChain)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkSwapchainCreateInfoKHR, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapChain));

  uint32_t numIms = 0;

  if(m_State >= WRITING)
  {
    VkResult vkr = VK_SUCCESS;

    vkr = ObjDisp(device)->GetSwapchainImagesKHR(Unwrap(device), Unwrap(*pSwapChain), &numIms, NULL);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  SERIALISE_ELEMENT(uint32_t, numSwapImages, numIms);
  SERIALISE_ELEMENT(VkSharingMode, sharingMode, pCreateInfo->imageSharingMode);

  // default to 0 for old logs, in most cases this doesn't change anything
  VkImageUsageFlags usage = pCreateInfo ? pCreateInfo->imageUsage : 0;
  if(m_State >= WRITING || GetLogVersion() >= 0x0000006)
  {
    localSerialiser->Serialise("usage", usage);
  }

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
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        NULL,
        0,
        VK_IMAGE_TYPE_2D,
        info.imageFormat,
        {info.imageExtent.width, info.imageExtent.height, 1},
        1,
        info.imageArrayLayers,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | usage,
        sharingMode,
        0,
        NULL,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    for(uint32_t i = 0; i < numSwapImages; i++)
    {
      VkDeviceMemory mem = VK_NULL_HANDLE;
      VkImage im = VK_NULL_HANDLE;

      VkResult vkr = ObjDisp(device)->CreateImage(Unwrap(device), &imInfo, NULL, &im);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      ResourceId liveId = GetResourceManager()->WrapResource(Unwrap(device), im);

      VkMemoryRequirements mrq = {0};

      ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(im), &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = ObjDisp(device)->AllocateMemory(Unwrap(device), &allocInfo, NULL, &mem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      ResourceId memid = GetResourceManager()->WrapResource(Unwrap(device), mem);
      // register as a live-only resource, so it is cleaned up properly
      GetResourceManager()->AddLiveResource(memid, mem);

      vkr = ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(im), Unwrap(mem), 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
      iminfo.creationFlags =
          TextureCategory::ShaderRead | TextureCategory::ColorTarget | TextureCategory::SwapBuffer;
      iminfo.cube = false;
      iminfo.samples = VK_SAMPLE_COUNT_1_BIT;

      m_CreationInfo.m_Names[liveId] = StringFormat::Fmt("Presentable Image %u", i);

      VkImageSubresourceRange range;
      range.baseMipLevel = range.baseArrayLayer = 0;
      range.levelCount = 1;
      range.layerCount = info.imageArrayLayers;
      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

      m_ImageLayouts[liveId].extent = iminfo.extent;
      m_ImageLayouts[liveId].format = iminfo.format;

      m_ImageLayouts[liveId].subresourceStates.clear();
      m_ImageLayouts[liveId].subresourceStates.push_back(
          ImageRegionState(range, UNKNOWN_PREV_IMG_LAYOUT, VK_IMAGE_LAYOUT_UNDEFINED));
    }
  }

  return true;
}

void WrappedVulkan::WrapAndProcessCreatedSwapchain(VkDevice device,
                                                   const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                   VkSwapchainKHR *pSwapChain)
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
          0,
          pCreateInfo->imageFormat,
          VK_SAMPLE_COUNT_1_BIT,
          VK_ATTACHMENT_LOAD_OP_LOAD,
          VK_ATTACHMENT_STORE_OP_STORE,
          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          VK_ATTACHMENT_STORE_OP_DONT_CARE,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };

      VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

      VkSubpassDescription sub = {
          0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
          0,    NULL,       // inputs
          1,    &attRef,    // color
          NULL,             // resolve
          NULL,             // depth-stencil
          0,    NULL,       // preserve
      };

      VkRenderPassCreateInfo rpinfo = {
          VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
          NULL,
          0,
          1,
          &attDesc,
          1,
          &sub,
          0,
          NULL,    // dependencies
      };

      vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, NULL, &swapInfo.rp);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(device), swapInfo.rp);
    }

    // serialise out the swap chain images
    {
      uint32_t numSwapImages;
      vkr = vt->GetSwapchainImagesKHR(Unwrap(device), Unwrap(*pSwapChain), &numSwapImages, NULL);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      swapInfo.lastPresent = 0;
      swapInfo.images.resize(numSwapImages);
      for(uint32_t i = 0; i < numSwapImages; i++)
      {
        swapInfo.images[i].im = VK_NULL_HANDLE;
        swapInfo.images[i].view = VK_NULL_HANDLE;
        swapInfo.images[i].fb = VK_NULL_HANDLE;
      }

      VkImage *images = new VkImage[numSwapImages];

      // go through our own function so we assign these images IDs
      vkr = vkGetSwapchainImagesKHR(device, *pSwapChain, &numSwapImages, images);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      for(uint32_t i = 0; i < numSwapImages; i++)
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
          m_ImageLayouts[imid].subresourceStates.push_back(
              ImageRegionState(range, UNKNOWN_PREV_IMG_LAYOUT, VK_IMAGE_LAYOUT_UNDEFINED));
        }

        {
          VkImageViewCreateInfo info = {
              VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
              NULL,
              0,
              Unwrap(images[i]),
              VK_IMAGE_VIEW_TYPE_2D,
              pCreateInfo->imageFormat,
              {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
              {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
          };

          vkr = vt->CreateImageView(Unwrap(device), &info, NULL, &swapImInfo.view);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.view);

          VkFramebufferCreateInfo fbinfo = {
              VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              NULL,
              0,
              Unwrap(swapInfo.rp),
              1,
              UnwrapPtr(swapImInfo.view),
              (uint32_t)pCreateInfo->imageExtent.width,
              (uint32_t)pCreateInfo->imageExtent.height,
              1,
          };

          vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, NULL, &swapImInfo.fb);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.fb);
        }
      }

      SAFE_DELETE_ARRAY(images);
    }

    FirstFrame(*pSwapChain);
  }
  else
  {
    GetResourceManager()->AddLiveResource(id, *pSwapChain);
  }
}

VkResult WrappedVulkan::vkCreateSwapchainKHR(VkDevice device,
                                             const VkSwapchainCreateInfoKHR *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkSwapchainKHR *pSwapChain)
{
  VkSwapchainCreateInfoKHR createInfo = *pCreateInfo;

  // make sure we can readback to get the screenshot, and render to it for the text overlay
  createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.surface = Unwrap(createInfo.surface);
  createInfo.oldSwapchain = Unwrap(createInfo.oldSwapchain);

  VkResult ret =
      ObjDisp(device)->CreateSwapchainKHR(Unwrap(device), &createInfo, pAllocator, pSwapChain);

  if(ret == VK_SUCCESS)
    WrapAndProcessCreatedSwapchain(device, pCreateInfo, pSwapChain);

  return ret;
}

VkResult WrappedVulkan::vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
  if(m_State == WRITING_IDLE)
  {
    RenderDoc::Inst().Tick();

    GetResourceManager()->FlushPendingDirty();
  }

  m_FrameCounter++;    // first present becomes frame #1, this function is at the end of the frame

  if(pPresentInfo->swapchainCount > 1 && (m_FrameCounter % 100) == 0)
  {
    RDCWARN("Presenting multiple swapchains at once - only first will be processed");
  }

  vector<VkSwapchainKHR> unwrappedSwaps;
  vector<VkSemaphore> unwrappedSems;

  VkPresentInfoKHR unwrappedInfo = *pPresentInfo;

  for(uint32_t i = 0; i < unwrappedInfo.swapchainCount; i++)
    unwrappedSwaps.push_back(Unwrap(unwrappedInfo.pSwapchains[i]));
  for(uint32_t i = 0; i < unwrappedInfo.waitSemaphoreCount; i++)
    unwrappedSems.push_back(Unwrap(unwrappedInfo.pWaitSemaphores[i]));

  unwrappedInfo.pSwapchains = unwrappedInfo.swapchainCount ? &unwrappedSwaps[0] : NULL;
  unwrappedInfo.pWaitSemaphores = unwrappedInfo.waitSemaphoreCount ? &unwrappedSems[0] : NULL;

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

  if(m_State == WRITING_IDLE)
  {
    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      VkRenderPass rp = swapInfo.rp;
      VkImage im = swapInfo.images[pPresentInfo->pImageIndices[0]].im;
      VkFramebuffer fb = swapInfo.images[pPresentInfo->pImageIndices[0]].fb;

      VkLayerDispatchTable *vt = ObjDisp(GetDev());

      TextPrintState textstate = {
          GetNextCmd(), rp, fb, swapInfo.extent.width, swapInfo.extent.height, swapInfo.format};

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      VkResult vkr = vt->BeginCommandBuffer(Unwrap(textstate.cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageMemoryBarrier bbBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        NULL,
                                        0,
                                        0,
                                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        Unwrap(im),
                                        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      bbBarrier.srcAccessMask = VK_ACCESS_ALL_READ_BITS;
      bbBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      DoPipelineBarrier(textstate.cmd, 1, &bbBarrier);

      GetDebugManager()->BeginText(textstate);

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      string overlayText = RenderDoc::Inst().GetOverlayText(RDC_Vulkan, m_FrameCounter, flags);

      if(!overlayText.empty())
        GetDebugManager()->RenderText(textstate, 0.0f, 0.0f, overlayText.c_str());

      GetDebugManager()->EndText(textstate);

      std::swap(bbBarrier.oldLayout, bbBarrier.newLayout);
      bbBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      bbBarrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

      DoPipelineBarrier(textstate.cmd, 1, &bbBarrier);

      ObjDisp(textstate.cmd)->EndCommandBuffer(Unwrap(textstate.cmd));

      SubmitCmds();

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

void WrappedVulkan::vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                        const VkAllocationCallbacks *pAllocator)
{
  WrappedVkSurfaceKHR *wrapper = GetWrapped(surface);

  // record pointer has window handle packed in
  if(wrapper->record)
    Keyboard::RemoveInputWindow((void *)wrapper->record);

  // now set record pointer back to NULL so no-one tries to delete it
  wrapper->record = NULL;

  VkSurfaceKHR unwrappedObj = wrapper->real.As<VkSurfaceKHR>();

  GetResourceManager()->ReleaseWrappedResource(surface, true);
  ObjDisp(instance)->DestroySurfaceKHR(Unwrap(instance), unwrappedObj, pAllocator);
}

// VK_KHR_display and VK_KHR_display_swapchain. These have no library or include dependencies so
// wecan just compile them in on all platforms to reduce platform-specific code. They are mostly
// only actually used though on *nix.

VkResult WrappedVulkan::vkGetPhysicalDeviceDisplayPropertiesKHR(VkPhysicalDevice physicalDevice,
                                                                uint32_t *pPropertyCount,
                                                                VkDisplayPropertiesKHR *pProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceDisplayPropertiesKHR(Unwrap(physicalDevice), pPropertyCount, pProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceDisplayPlanePropertiesKHR(
    VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount,
    VkDisplayPlanePropertiesKHR *pProperties)
{
  return ObjDisp(physicalDevice)
      ->GetPhysicalDeviceDisplayPlanePropertiesKHR(Unwrap(physicalDevice), pPropertyCount,
                                                   pProperties);
}

VkResult WrappedVulkan::vkGetDisplayPlaneSupportedDisplaysKHR(VkPhysicalDevice physicalDevice,
                                                              uint32_t planeIndex,
                                                              uint32_t *pDisplayCount,
                                                              VkDisplayKHR *pDisplays)
{
  // we don't wrap the resulting displays since there's no data we need for them
  return ObjDisp(physicalDevice)
      ->GetDisplayPlaneSupportedDisplaysKHR(Unwrap(physicalDevice), planeIndex, pDisplayCount,
                                            pDisplays);
}

VkResult WrappedVulkan::vkGetDisplayModePropertiesKHR(VkPhysicalDevice physicalDevice,
                                                      VkDisplayKHR display, uint32_t *pPropertyCount,
                                                      VkDisplayModePropertiesKHR *pProperties)
{
  // display is not wrapped since we have no need of any data associated with it
  return ObjDisp(physicalDevice)
      ->GetDisplayModePropertiesKHR(Unwrap(physicalDevice), display, pPropertyCount, pProperties);
}

VkResult WrappedVulkan::vkCreateDisplayModeKHR(VkPhysicalDevice physicalDevice, VkDisplayKHR display,
                                               const VkDisplayModeCreateInfoKHR *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkDisplayModeKHR *pMode)
{
  // we don't wrap the resulting mode since there's no data we need for it
  return ObjDisp(physicalDevice)
      ->CreateDisplayModeKHR(Unwrap(physicalDevice), display, pCreateInfo, pAllocator, pMode);
}

VkResult WrappedVulkan::vkGetDisplayPlaneCapabilitiesKHR(VkPhysicalDevice physicalDevice,
                                                         VkDisplayModeKHR mode, uint32_t planeIndex,
                                                         VkDisplayPlaneCapabilitiesKHR *pCapabilities)
{
  // mode is not wrapped since we have no need of any data associated with it
  return ObjDisp(physicalDevice)
      ->GetDisplayPlaneCapabilitiesKHR(Unwrap(physicalDevice), mode, planeIndex, pCapabilities);
}

VkResult WrappedVulkan::vkCreateDisplayPlaneSurfaceKHR(VkInstance instance,
                                                       const VkDisplaySurfaceCreateInfoKHR *pCreateInfo,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkSurfaceKHR *pSurface)
{
  // should not come in here at all on replay
  RDCASSERT(m_State >= WRITING);

  VkResult ret = ObjDisp(instance)->CreateDisplayPlaneSurfaceKHR(Unwrap(instance), pCreateInfo,
                                                                 pAllocator, pSurface);

  if(ret == VK_SUCCESS)
  {
    // we must wrap surfaces to be consistent with the rest of the code and surface handling,
    // but there's nothing actually to do here - no meaningful data we care about here.
    GetResourceManager()->WrapResource(Unwrap(instance), *pSurface);

    WrappedVkSurfaceKHR *wrapped = GetWrapped(*pSurface);

    // we don't have an actual OS handle to identify this window. Instead construct something
    // that should be unique and hopefully not clashing/overlapping with other window handles
    // in use.
    uintptr_t fakeWindowHandle;
    fakeWindowHandle = (uintptr_t)NON_DISP_TO_UINT64(pCreateInfo->displayMode);
    fakeWindowHandle += pCreateInfo->planeIndex;
    fakeWindowHandle += pCreateInfo->planeStackIndex << 4;

    // since there's no point in allocating a full resource record and storing the window
    // handle under there somewhere, we just cast. We won't use the resource record for anything

    wrapped->record = (VkResourceRecord *)fakeWindowHandle;
  }

  return ret;
}

VkResult WrappedVulkan::vkCreateSharedSwapchainsKHR(VkDevice device, uint32_t swapchainCount,
                                                    const VkSwapchainCreateInfoKHR *pCreateInfos,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkSwapchainKHR *pSwapchains)
{
  VkSwapchainCreateInfoKHR *unwrapped = GetTempArray<VkSwapchainCreateInfoKHR>(swapchainCount);
  for(uint32_t i = 0; i < swapchainCount; i++)
  {
    unwrapped[i] = pCreateInfos[i];
    // make sure we can readback to get the screenshot, and render to it for the text overlay
    unwrapped[i].imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    unwrapped[i].surface = Unwrap(unwrapped[i].surface);
    unwrapped[i].oldSwapchain = Unwrap(unwrapped[i].oldSwapchain);
  }

  VkResult ret = ObjDisp(device)->CreateSharedSwapchainsKHR(Unwrap(device), swapchainCount,
                                                            unwrapped, pAllocator, pSwapchains);

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < swapchainCount; i++)
      WrapAndProcessCreatedSwapchain(device, pCreateInfos + i, pSwapchains + i);
  }

  return ret;
}

VkResult WrappedVulkan::vkReleaseDisplayEXT(VkPhysicalDevice physicalDevice, VkDisplayKHR display)
{
  // displays are not wrapped
  return ObjDisp(physicalDevice)->ReleaseDisplayEXT(Unwrap(physicalDevice), display);
}
