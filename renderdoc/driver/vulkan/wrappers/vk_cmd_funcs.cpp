/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

static std::string ToHumanStr(const VkAttachmentLoadOp &el)
{
  BEGIN_ENUM_STRINGISE(VkAttachmentLoadOp);
  {
    case VK_ATTACHMENT_LOAD_OP_LOAD: return "Load";
    case VK_ATTACHMENT_LOAD_OP_CLEAR: return "Clear";
    case VK_ATTACHMENT_LOAD_OP_DONT_CARE: return "Don't Care";
  }
  END_ENUM_STRINGISE();
}

static std::string ToHumanStr(const VkAttachmentStoreOp &el)
{
  BEGIN_ENUM_STRINGISE(VkAttachmentStoreOp);
  {
    case VK_ATTACHMENT_STORE_OP_STORE: return "Store";
    case VK_ATTACHMENT_STORE_OP_DONT_CARE: return "Don't Care";
  }
  END_ENUM_STRINGISE();
}

std::vector<VkImageMemoryBarrier> WrappedVulkan::GetImplicitRenderPassBarriers(uint32_t subpass)
{
  ResourceId rp, fb;

  if(m_LastCmdBufferID == ResourceId())
  {
    rp = m_RenderState.renderPass;
    fb = m_RenderState.framebuffer;
  }
  else
  {
    rp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass;
    fb = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer;
  }

  std::vector<VkImageMemoryBarrier> ret;

  VulkanCreationInfo::Framebuffer fbinfo = m_CreationInfo.m_Framebuffer[fb];
  VulkanCreationInfo::RenderPass rpinfo = m_CreationInfo.m_RenderPass[rp];

  std::vector<VkAttachmentReference> atts;

  // a bit of dancing to get a subpass index. Because we don't increment
  // the subpass counter on EndRenderPass the value is the same for the last
  // NextSubpass. Instead we pass in the subpass index of ~0U for End
  if(subpass == ~0U)
  {
    // we transition all attachments to finalLayout from whichever they
    // were in previously
    atts.resize(rpinfo.attachments.size());
    for(size_t i = 0; i < rpinfo.attachments.size(); i++)
    {
      atts[i].attachment = (uint32_t)i;
      atts[i].layout = rpinfo.attachments[i].finalLayout;
    }
  }
  else
  {
    if(m_LastCmdBufferID == ResourceId())
      subpass = m_RenderState.subpass;
    else
      subpass = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass;

    // transition the attachments in this subpass
    for(size_t i = 0; i < rpinfo.subpasses[subpass].colorAttachments.size(); i++)
    {
      uint32_t attIdx = rpinfo.subpasses[subpass].colorAttachments[i];

      if(attIdx == VK_ATTACHMENT_UNUSED)
        continue;

      atts.push_back(VkAttachmentReference());
      atts.back().attachment = attIdx;
      atts.back().layout = rpinfo.subpasses[subpass].colorLayouts[i];
    }

    for(size_t i = 0; i < rpinfo.subpasses[subpass].inputAttachments.size(); i++)
    {
      uint32_t attIdx = rpinfo.subpasses[subpass].inputAttachments[i];

      if(attIdx == VK_ATTACHMENT_UNUSED)
        continue;

      atts.push_back(VkAttachmentReference());
      atts.back().attachment = attIdx;
      atts.back().layout = rpinfo.subpasses[subpass].inputLayouts[i];
    }

    int32_t ds = rpinfo.subpasses[subpass].depthstencilAttachment;

    if(ds != -1)
    {
      atts.push_back(VkAttachmentReference());
      atts.back().attachment = (uint32_t)rpinfo.subpasses[subpass].depthstencilAttachment;
      atts.back().layout = rpinfo.subpasses[subpass].depthstencilLayout;
    }

    int32_t fd = rpinfo.subpasses[subpass].fragmentDensityAttachment;

    if(fd != -1)
    {
      atts.push_back(VkAttachmentReference());
      atts.back().attachment = (uint32_t)rpinfo.subpasses[subpass].fragmentDensityAttachment;
      atts.back().layout = rpinfo.subpasses[subpass].fragmentDensityLayout;
    }
  }

  for(size_t i = 0; i < atts.size(); i++)
  {
    uint32_t idx = atts[i].attachment;

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    ResourceId view = fbinfo.attachments[idx].view;

    barrier.subresourceRange = m_CreationInfo.m_ImageView[view].range;
    barrier.image = Unwrap(
        GetResourceManager()->GetCurrentHandle<VkImage>(m_CreationInfo.m_ImageView[view].image));

    // When an imageView of a depth/stencil image is used as a depth/stencil framebuffer attachment,
    // the aspectMask is ignored and both depth and stencil image subresources are used.
    VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[m_CreationInfo.m_ImageView[view].image];
    if(IsDepthAndStencilFormat(c.format))
      barrier.subresourceRange.aspectMask = (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    barrier.newLayout = atts[i].layout;

    // search back from this subpass to see which layout it was in before. If it's
    // not been used in a previous subpass, then default to initialLayout
    barrier.oldLayout = rpinfo.attachments[idx].initialLayout;

    if(subpass == ~0U)
      subpass = (uint32_t)rpinfo.subpasses.size();

    // subpass is at this point a 1-indexed value essentially, as it's the index
    // of the subpass we just finished (or 0 if we're in BeginRenderPass in which
    // case the loop just skips completely and we use initialLayout, which is
    // correct).

    for(uint32_t s = subpass; s > 0; s--)
    {
      bool found = false;

      for(size_t a = 0; !found && a < rpinfo.subpasses[s - 1].colorAttachments.size(); a++)
      {
        if(rpinfo.subpasses[s - 1].colorAttachments[a] == idx)
        {
          barrier.oldLayout = rpinfo.subpasses[s - 1].colorLayouts[a];
          found = true;
          break;
        }
      }

      if(found)
        break;

      for(size_t a = 0; !found && a < rpinfo.subpasses[s - 1].inputAttachments.size(); a++)
      {
        if(rpinfo.subpasses[s - 1].inputAttachments[a] == idx)
        {
          barrier.oldLayout = rpinfo.subpasses[s - 1].inputLayouts[a];
          found = true;
          break;
        }
      }

      if(found)
        break;

      if((uint32_t)rpinfo.subpasses[s - 1].depthstencilAttachment == idx)
      {
        barrier.oldLayout = rpinfo.subpasses[s - 1].depthstencilLayout;
        break;
      }

      if((uint32_t)rpinfo.subpasses[s - 1].fragmentDensityAttachment == idx)
      {
        barrier.oldLayout = rpinfo.subpasses[s - 1].fragmentDensityLayout;
        break;
      }
    }

    ret.push_back(barrier);
  }

  // erase any do-nothing barriers
  for(auto it = ret.begin(); it != ret.end();)
  {
    if(it->oldLayout == it->newLayout)
      it = ret.erase(it);
    else
      ++it;
  }

  return ret;
}

std::string WrappedVulkan::MakeRenderPassOpString(bool store)
{
  std::string opDesc = "";

  const VulkanCreationInfo::RenderPass &info =
      m_CreationInfo.m_RenderPass[m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass];
  const VulkanCreationInfo::Framebuffer &fbinfo =
      m_CreationInfo.m_Framebuffer[m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer];

  const std::vector<VulkanCreationInfo::RenderPass::Attachment> &atts = info.attachments;

  if(atts.empty())
  {
    opDesc = "-";
  }
  else
  {
    bool colsame = true;

    uint32_t subpass = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass;

    // find which attachment is the depth-stencil one
    int32_t dsAttach = info.subpasses[subpass].depthstencilAttachment;
    bool hasStencil = false;
    bool depthonly = false;

    // if there is a depth-stencil attachment, see if it has a stencil
    // component and if the subpass is depth only (no other attachments)
    if(dsAttach >= 0)
    {
      hasStencil = !IsDepthOnlyFormat(fbinfo.attachments[dsAttach].format);
      depthonly = info.subpasses[subpass].colorAttachments.size() == 0;
    }

    const std::vector<uint32_t> &cols = info.subpasses[subpass].colorAttachments;

    // we check all non-UNUSED attachments to see if they're all the same.
    // To begin with we point to an invalid attachment index
    uint32_t col0 = VK_ATTACHMENT_UNUSED;

    // look through all other color attachments to see if they're identical
    for(size_t i = 0; i < cols.size(); i++)
    {
      const uint32_t col = cols[i];

      // skip unused attachments
      if(col == VK_ATTACHMENT_UNUSED)
        continue;

      // the first valid attachment we find, use that as our reference point
      if(col0 == VK_ATTACHMENT_UNUSED)
      {
        col0 = col;
        continue;
      }

      // for any other attachments, compare them to the reference
      if(store)
      {
        if(atts[col].storeOp != atts[col0].storeOp)
          colsame = false;
      }
      else
      {
        if(atts[col].loadOp != atts[col0].loadOp)
          colsame = false;
      }
    }

    // handle depth only passes
    if(depthonly)
    {
      opDesc = "";
    }
    else if(!colsame)
    {
      // if we have different storage for the colour, don't display
      // the full details

      opDesc = store ? "Different store ops" : "Different load ops";
    }
    else if(col0 == VK_ATTACHMENT_UNUSED)
    {
      // we're here if we didn't find any non-UNUSED color attachments at all
      opDesc = "Unused";
    }
    else
    {
      // all colour ops are the same, print it
      opDesc = store ? ToHumanStr(atts[col0].storeOp) : ToHumanStr(atts[col0].loadOp);
    }

    // do we have depth?
    if(dsAttach != -1)
    {
      // could be empty if this is a depth-only pass
      if(!opDesc.empty())
        opDesc = "C=" + opDesc + ", ";

      // if there's no stencil, just print depth op
      if(!hasStencil)
      {
        opDesc +=
            "D=" + (store ? ToHumanStr(atts[dsAttach].storeOp) : ToHumanStr(atts[dsAttach].loadOp));
      }
      else
      {
        if(store)
        {
          // if depth and stencil have same op, print together, otherwise separately
          if(atts[dsAttach].storeOp == atts[dsAttach].stencilStoreOp)
            opDesc += "DS=" + ToHumanStr(atts[dsAttach].storeOp);
          else
            opDesc += "D=" + ToHumanStr(atts[dsAttach].storeOp) + ", S=" +
                      ToHumanStr(atts[dsAttach].stencilStoreOp);
        }
        else
        {
          // if depth and stencil have same op, print together, otherwise separately
          if(atts[dsAttach].loadOp == atts[dsAttach].stencilLoadOp)
            opDesc += "DS=" + ToHumanStr(atts[dsAttach].loadOp);
          else
            opDesc += "D=" + ToHumanStr(atts[dsAttach].loadOp) + ", S=" +
                      ToHumanStr(atts[dsAttach].stencilLoadOp);
        }
      }
    }
  }

  return opDesc;
}

// Command pool functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateCommandPool(SerialiserType &ser, VkDevice device,
                                                  const VkCommandPoolCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkCommandPool *pCmdPool)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(CmdPool, GetResID(*pCmdPool)).TypedAs("VkCommandPool"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCommandPool pool = VK_NULL_HANDLE;

    // remap the queue family index
    CreateInfo.queueFamilyIndex = m_QueueRemapping[CreateInfo.queueFamilyIndex][0].family;

    VkResult ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), &CreateInfo, NULL, &pool);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
      GetResourceManager()->AddLiveResource(CmdPool, pool);
    }

    AddResource(CmdPool, ResourceType::Pool, "Command Pool");
    DerivedResource(device, CmdPool);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateCommandPool(VkDevice device,
                                            const VkCommandPoolCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator,
                                            VkCommandPool *pCmdPool)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), pCreateInfo, pAllocator, pCmdPool));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pCmdPool);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateCommandPool);
        Serialise_vkCreateCommandPool(ser, device, pCreateInfo, NULL, pCmdPool);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pCmdPool);
      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pCmdPool);
    }
  }

  return ret;
}

VkResult WrappedVulkan::vkResetCommandPool(VkDevice device, VkCommandPool cmdPool,
                                           VkCommandPoolResetFlags flags)
{
  return ObjDisp(device)->ResetCommandPool(Unwrap(device), Unwrap(cmdPool), flags);
}

void WrappedVulkan::vkTrimCommandPool(VkDevice device, VkCommandPool commandPool,
                                      VkCommandPoolTrimFlags flags)
{
  return ObjDisp(device)->TrimCommandPool(Unwrap(device), Unwrap(commandPool), flags);
}

// Command buffer functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkAllocateCommandBuffers(SerialiserType &ser, VkDevice device,
                                                       const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                       VkCommandBuffer *pCommandBuffers)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo);
  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(*pCommandBuffers)).TypedAs("VkCommandBuffer"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  // this chunk is purely for user information and consistency, the command buffer we allocate is
  // a dummy and is not used for anything.

  if(IsReplayingAndReading())
  {
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo unwrappedInfo = AllocateInfo;
    unwrappedInfo.commandBufferCount = 1;
    unwrappedInfo.commandPool = Unwrap(unwrappedInfo.commandPool);
    VkResult ret = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &unwrappedInfo, &cmd);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
      GetResourceManager()->AddLiveResource(CommandBuffer, cmd);
    }

    AddResource(CommandBuffer, ResourceType::CommandBuffer, "Command Buffer");
    DerivedResource(device, CommandBuffer);
    DerivedResource(AllocateInfo.commandPool, CommandBuffer);
  }

  return true;
}

VkResult WrappedVulkan::vkAllocateCommandBuffers(VkDevice device,
                                                 const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                 VkCommandBuffer *pCommandBuffers)
{
  VkCommandBufferAllocateInfo unwrappedInfo = *pAllocateInfo;
  unwrappedInfo.commandPool = Unwrap(unwrappedInfo.commandPool);
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &unwrappedInfo,
                                                                    pCommandBuffers));

  if(ret == VK_SUCCESS)
  {
    for(uint32_t i = 0; i < unwrappedInfo.commandBufferCount; i++)
    {
      VkCommandBuffer unwrappedReal = pCommandBuffers[i];

      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pCommandBuffers[i]);

      // we set this *after* wrapping, so that the wrapped resource copies the 'uninitialised'
      // loader table, since the loader expects to set the dispatch table onto an existing magic
      // number in the trampoline function at the start of the chain.
      if(m_SetDeviceLoaderData)
        m_SetDeviceLoaderData(device, unwrappedReal);
      else
        SetDispatchTableOverMagicNumber(device, unwrappedReal);

      if(IsCaptureMode(m_State))
      {
        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pCommandBuffers[i]);

        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkAllocateCommandBuffers);
          Serialise_vkAllocateCommandBuffers(ser, device, pAllocateInfo, pCommandBuffers + i);

          chunk = scope.Get();
        }

        // a bit of a hack, we make a parallel resource record with the same lifetime as the command
        // buffer, so it will hold onto our allocation chunk & pool parent.
        // It will be pulled into the capture explicitly, since the command buffer record itself is
        // used directly for recording in-progress commands, and we can't pull that in since it
        // might be partially recorded at the time of a submit of a previously baked list.
        VkResourceRecord *allocRecord =
            GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
        allocRecord->InternalResource = true;
        allocRecord->AddChunk(chunk);
        record->AddParent(allocRecord);
        record->InternalResource = true;

        record->bakedCommands = NULL;

        record->pool = GetRecord(pAllocateInfo->commandPool);
        allocRecord->AddParent(record->pool);

        {
          record->pool->LockChunks();
          record->pool->pooledChildren.push_back(record);
          record->pool->UnlockChunks();
        }

        // we don't serialise this as we never create this command buffer directly.
        // Instead we create a command buffer for each baked list that we find.

        // if pNext is non-NULL, need to do a deep copy
        // we don't support any extensions on VkCommandBufferCreateInfo anyway
        RDCASSERT(pAllocateInfo->pNext == NULL);

        record->cmdInfo = new CmdBufferRecordingInfo();

        record->cmdInfo->device = device;
        record->cmdInfo->allocInfo = *pAllocateInfo;
        record->cmdInfo->allocInfo.commandBufferCount = 1;
        record->cmdInfo->allocRecord = allocRecord;
        record->cmdInfo->present = false;
      }
      else
      {
        GetResourceManager()->AddLiveResource(id, pCommandBuffers[i]);
      }
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkBeginCommandBuffer(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   const VkCommandBufferBeginInfo *pBeginInfo)
{
  ResourceId BakedCommandBuffer;
  VkCommandBufferAllocateInfo AllocateInfo;
  VkDevice device = VK_NULL_HANDLE;

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      BakedCommandBuffer = record->bakedCommands->GetResourceID();

    RDCASSERT(record->cmdInfo);
    device = record->cmdInfo->device;
    AllocateInfo = record->cmdInfo->allocInfo;
  }

  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(commandBuffer)).TypedAs("VkCommandBuffer"_lit);
  SERIALISE_ELEMENT_LOCAL(BeginInfo, *pBeginInfo);
  SERIALISE_ELEMENT(BakedCommandBuffer);
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(AllocateInfo).Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = CommandBuffer;

    // when loading, allocate a new resource ID for each push descriptor slot in this command buffer
    if(IsLoading(m_State))
    {
      for(int p = 0; p < 2; p++)
        for(size_t i = 0; i < ARRAY_COUNT(BakedCmdBufferInfo::pushDescriptorID[p]); i++)
          m_BakedCmdBufferInfo[BakedCommandBuffer].pushDescriptorID[p][i] =
              ResourceIDGen::GetNewUniqueID();
    }

    // clear/invalidate descriptor set state for this command buffer.
    for(int p = 0; p < 2; p++)
    {
      for(size_t i = 0; i < ARRAY_COUNT(BakedCmdBufferInfo::pushDescriptorID[p]); i++)
      {
        m_DescriptorSetState[m_BakedCmdBufferInfo[BakedCommandBuffer].pushDescriptorID[p][i]] =
            DescriptorSetInfo(true);
      }
    }

    m_BakedCmdBufferInfo[m_LastCmdBufferID].level = m_BakedCmdBufferInfo[BakedCommandBuffer].level =
        AllocateInfo.level;
    m_BakedCmdBufferInfo[m_LastCmdBufferID].beginFlags =
        m_BakedCmdBufferInfo[BakedCommandBuffer].beginFlags = BeginInfo.flags;
    m_BakedCmdBufferInfo[m_LastCmdBufferID].markerCount = 0;

    VkCommandBufferBeginInfo unwrappedBeginInfo = BeginInfo;
    VkCommandBufferInheritanceInfo unwrappedInheritInfo;
    if(BeginInfo.pInheritanceInfo)
    {
      unwrappedInheritInfo = *BeginInfo.pInheritanceInfo;
      unwrappedInheritInfo.framebuffer = Unwrap(unwrappedInheritInfo.framebuffer);
      unwrappedInheritInfo.renderPass = Unwrap(unwrappedInheritInfo.renderPass);

      unwrappedBeginInfo.pInheritanceInfo = &unwrappedInheritInfo;

      VkCommandBufferInheritanceConditionalRenderingInfoEXT *inheritanceConditionalRenderingInfo =
          (VkCommandBufferInheritanceConditionalRenderingInfoEXT *)FindNextStruct(
              BeginInfo.pInheritanceInfo,
              VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT);

      if(inheritanceConditionalRenderingInfo)
        m_BakedCmdBufferInfo[BakedCommandBuffer].inheritConditionalRendering =
            inheritanceConditionalRenderingInfo->conditionalRenderingEnable == VK_TRUE;
    }

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedBeginInfo.pNext));

    UnwrapNextChain(m_State, "VkCommandBufferBeginInfo", tempMem,
                    (VkBaseInStructure *)&unwrappedBeginInfo);

    if(IsActiveReplaying(m_State))
    {
      const uint32_t length = m_BakedCmdBufferInfo[BakedCommandBuffer].eventCount;

      bool rerecord = false;
      bool partial = false;
      int partialType = ePartialNum;

      // check for partial execution of this command buffer
      for(int p = 0; p < ePartialNum; p++)
      {
        const std::vector<Submission> &submissions =
            m_Partial[p].cmdBufferSubmits[BakedCommandBuffer];

        for(auto it = submissions.begin(); it != submissions.end(); ++it)
        {
          if(it->baseEvent <= m_LastEventID && m_LastEventID < (it->baseEvent + length))
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("vkBegin - partial detected %u < %u < %u, %llu -> %llu", it->baseEvent,
                     m_LastEventID, it->baseEvent + length, m_LastCmdBufferID, BakedCommandBuffer);
#endif

            m_Partial[p].partialParent = BakedCommandBuffer;
            m_Partial[p].baseEvent = it->baseEvent;
            m_Partial[p].renderPassActive = false;
            m_RenderState.xfbcounters.clear();
            m_RenderState.conditionalRendering.buffer = ResourceId();

            rerecord = true;
            partial = true;
            partialType = p;
          }
          else if(it->baseEvent <= m_LastEventID)
          {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("vkBegin - full re-record detected %u < %u <= %u, %llu -> %llu", it->baseEvent,
                     it->baseEvent + length, m_LastEventID, m_LastCmdBufferID, BakedCommandBuffer);
#endif

            // this submission is completely within the range, so it should still be re-recorded
            rerecord = true;
          }
        }
      }

      if(rerecord)
      {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo unwrappedInfo = AllocateInfo;
        unwrappedInfo.commandPool = Unwrap(unwrappedInfo.commandPool);
        VkResult ret = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &unwrappedInfo, &cmd);

        if(ret != VK_SUCCESS)
        {
          RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
          return false;
        }
        else
        {
          GetResourceManager()->WrapResource(Unwrap(device), cmd);
        }

        // we store under both baked and non baked ID.
        // The baked ID is the 'real' entry, the non baked is simply so it
        // can be found in the subsequent serialised commands that ref the
        // non-baked ID. The baked ID is referenced by the submit itself.
        //
        // In vkEndCommandBuffer we erase the non-baked reference, and since
        // we know you can only be recording a command buffer once at a time
        // (even if it's baked to several command buffers in the frame)
        // there's no issue with clashes here.
        m_RerecordCmds[BakedCommandBuffer] = cmd;
        m_RerecordCmds[m_LastCmdBufferID] = cmd;

        m_RerecordCmdList.push_back({AllocateInfo.commandPool, cmd});

        m_BakedCmdBufferInfo[GetResID(cmd)].level = AllocateInfo.level;
        m_BakedCmdBufferInfo[GetResID(cmd)].beginFlags = BeginInfo.flags;

        // add one-time submit flag as this partial cmd buffer will only be submitted once
        BeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(AllocateInfo.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY)
          BeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &unwrappedBeginInfo);
      }

      // whenever a vkCmd command-building chunk asks for the command buffer, it
      // will get our baked version.
      if(GetResourceManager()->HasReplacement(m_LastCmdBufferID))
        GetResourceManager()->RemoveReplacement(m_LastCmdBufferID);

      GetResourceManager()->ReplaceResource(m_LastCmdBufferID, BakedCommandBuffer);

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID = 0;
      m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID = 0;
    }
    else
    {
      // remove one-time submit flag as we will want to submit many
      BeginInfo.flags &= ~VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      if(AllocateInfo.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY)
        BeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

      VkCommandBuffer cmd = VK_NULL_HANDLE;

      if(!GetResourceManager()->HasLiveResource(BakedCommandBuffer))
      {
        VkCommandBufferAllocateInfo unwrappedInfo = AllocateInfo;
        unwrappedInfo.commandPool = Unwrap(unwrappedInfo.commandPool);
        VkResult ret = ObjDisp(device)->AllocateCommandBuffers(Unwrap(device), &unwrappedInfo, &cmd);

        if(ret != VK_SUCCESS)
        {
          RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
          return false;
        }
        else
        {
          ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
          GetResourceManager()->AddLiveResource(BakedCommandBuffer, cmd);
        }

        AddResource(BakedCommandBuffer, ResourceType::CommandBuffer, "Baked Command Buffer");
        GetReplay()->GetResourceDesc(BakedCommandBuffer).initialisationChunks.clear();
        DerivedResource(device, BakedCommandBuffer);
        DerivedResource(AllocateInfo.commandPool, BakedCommandBuffer);

        // do this one manually since there's no live version of the swapchain, and
        // DerivedResource() assumes we're passing it a live ID (or live resource)
        GetReplay()->GetResourceDesc(CommandBuffer).derivedResources.push_back(BakedCommandBuffer);
        GetReplay()->GetResourceDesc(BakedCommandBuffer).parentResources.push_back(CommandBuffer);

        // whenever a vkCmd command-building chunk asks for the command buffer, it
        // will get our baked version.
        if(GetResourceManager()->HasReplacement(m_LastCmdBufferID))
          GetResourceManager()->RemoveReplacement(m_LastCmdBufferID);

        GetResourceManager()->ReplaceResource(m_LastCmdBufferID, BakedCommandBuffer);
      }
      else
      {
        cmd = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(BakedCommandBuffer);
      }

      // propagate any name there might be
      if(m_CreationInfo.m_Names.find(m_LastCmdBufferID) != m_CreationInfo.m_Names.end())
        m_CreationInfo.m_Names[GetResourceManager()->GetLiveID(BakedCommandBuffer)] =
            m_CreationInfo.m_Names[m_LastCmdBufferID];

      {
        VulkanDrawcallTreeNode *draw = new VulkanDrawcallTreeNode;
        m_BakedCmdBufferInfo[BakedCommandBuffer].draw = draw;

        // On queue submit we increment all child events/drawcalls by
        // m_RootEventID and insert them into the tree.
        m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID = 0;
        m_BakedCmdBufferInfo[BakedCommandBuffer].eventCount = 0;
        m_BakedCmdBufferInfo[BakedCommandBuffer].drawCount = 0;

        m_BakedCmdBufferInfo[BakedCommandBuffer].drawStack.push_back(draw);

        m_BakedCmdBufferInfo[BakedCommandBuffer].beginChunk =
            uint32_t(m_StructuredFile->chunks.size() - 1);
      }

      ObjDisp(device)->BeginCommandBuffer(Unwrap(cmd), &unwrappedBeginInfo);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                             const VkCommandBufferBeginInfo *pBeginInfo)
{
  VkCommandBufferBeginInfo beginInfo = *pBeginInfo;
  VkCommandBufferInheritanceInfo unwrappedInfo;
  if(pBeginInfo->pInheritanceInfo)
  {
    unwrappedInfo = *pBeginInfo->pInheritanceInfo;
    unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);
    unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);

    beginInfo.pInheritanceInfo = &unwrappedInfo;
  }

  byte *tempMem = GetTempMemory(GetNextPatchSize(beginInfo.pNext));

  UnwrapNextChain(m_State, "VkCommandBufferBeginInfo", tempMem, (VkBaseInStructure *)&beginInfo);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(commandBuffer)->BeginCommandBuffer(Unwrap(commandBuffer), &beginInfo));

  VkResourceRecord *record = GetRecord(commandBuffer);
  RDCASSERT(record);

  if(record)
  {
    // If a command bfufer was already recorded (ie we have some baked commands),
    // then begin is spec'd to implicitly reset. That means we need to tidy up
    // any existing baked commands before creating a new set.
    if(record->bakedCommands)
      record->bakedCommands->Delete(GetResourceManager());

    record->bakedCommands = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
    record->bakedCommands->InternalResource = true;
    record->bakedCommands->Resource = (WrappedVkRes *)commandBuffer;
    record->bakedCommands->cmdInfo = new CmdBufferRecordingInfo();

    record->bakedCommands->cmdInfo->device = record->cmdInfo->device;
    record->bakedCommands->cmdInfo->allocInfo = record->cmdInfo->allocInfo;
    record->bakedCommands->cmdInfo->present = false;

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBeginCommandBuffer);
      Serialise_vkBeginCommandBuffer(ser, commandBuffer, pBeginInfo);

      record->AddChunk(scope.Get());
    }

    if(pBeginInfo->pInheritanceInfo)
    {
      record->MarkResourceFrameReferenced(GetResID(pBeginInfo->pInheritanceInfo->renderPass),
                                          eFrameRef_Read);
      record->MarkResourceFrameReferenced(GetResID(pBeginInfo->pInheritanceInfo->framebuffer),
                                          eFrameRef_Read);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkEndCommandBuffer(SerialiserType &ser, VkCommandBuffer commandBuffer)
{
  ResourceId BakedCommandBuffer;

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);
    RDCASSERT(record->bakedCommands);
    if(record->bakedCommands)
      BakedCommandBuffer = record->bakedCommands->GetResourceID();
  }

  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(commandBuffer)).TypedAs("VkCommandBuffer"_lit);
  SERIALISE_ELEMENT(BakedCommandBuffer);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = BakedCommandBuffer;

    if(IsActiveReplaying(m_State))
    {
      if(HasRerecordCmdBuf(BakedCommandBuffer))
      {
        commandBuffer = RerecordCmdBuf(BakedCommandBuffer);

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
        RDCDEBUG("Ending re-recorded command buffer for %llu baked to %llu", CommandBuffer,
                 BakedCommandBuffer);
#endif

        if(m_Partial[Primary].partialParent == BakedCommandBuffer &&
           !m_RenderState.xfbcounters.empty())
        {
          m_RenderState.EndTransformFeedback(commandBuffer);
        }

        if(m_Partial[Primary].partialParent == BakedCommandBuffer &&
           m_RenderState.IsConditionalRenderingEnabled())
        {
          m_RenderState.EndConditionalRendering(commandBuffer);
        }

        // finish any render pass that was still active in the primary partial parent
        if(m_Partial[Primary].partialParent == BakedCommandBuffer &&
           m_Partial[Primary].renderPassActive)
        {
          uint32_t numSubpasses =
              (uint32_t)m_CreationInfo.m_RenderPass[m_RenderState.renderPass].subpasses.size();

          // for each subpass we skip, and for the finalLayout transition at the end of the
          // renderpass, record these barriers. These are executed implicitly but because we want to
          // pretend they never happened, we then reverse their effects so that our layout tracking
          // is accurate and the images end up in the layout they were in during the last active
          // subpass
          uint32_t &sub = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass;

          std::vector<rdcpair<ResourceId, ImageRegionState> > imgbarriers;

          for(sub = m_RenderState.subpass; sub < numSubpasses - 1; sub++)
          {
            ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), VK_SUBPASS_CONTENTS_INLINE);

            std::vector<VkImageMemoryBarrier> subpassBarriers = GetImplicitRenderPassBarriers();

            GetResourceManager()->RecordBarriers(
                imgbarriers, m_ImageLayouts, (uint32_t)subpassBarriers.size(), &subpassBarriers[0]);
          }

          std::vector<VkImageMemoryBarrier> finalBarriers = GetImplicitRenderPassBarriers(~0U);

          GetResourceManager()->RecordBarriers(imgbarriers, m_ImageLayouts,
                                               (uint32_t)finalBarriers.size(), &finalBarriers[0]);

          ObjDisp(commandBuffer)->CmdEndRenderPass(Unwrap(commandBuffer));

          // undo any implicit transitions we just went through, so that we can pretend that the
          // image stayed in the same layout as it was when we stopped partially replaying.
          std::vector<VkImageMemoryBarrier> revertBarriers;

          for(auto it = imgbarriers.begin(); it != imgbarriers.end(); ++it)
          {
            VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_ALL_READ_BITS | VK_ACCESS_ALL_WRITE_BITS;
            barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS | VK_ACCESS_ALL_WRITE_BITS;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = Unwrap(GetResourceManager()->GetCurrentHandle<VkImage>(it->first));

            // go from the layout we ended up in, to the layout we started in
            barrier.oldLayout = it->second.newLayout;
            barrier.newLayout = it->second.oldLayout;

            barrier.subresourceRange = it->second.subresourceRange;

            if(barrier.oldLayout != barrier.newLayout &&
               barrier.newLayout != VK_IMAGE_LAYOUT_UNDEFINED)
            {
              SanitiseOldImageLayout(barrier.oldLayout);
              SanitiseNewImageLayout(barrier.newLayout);

              revertBarriers.push_back(barrier);
            }
          }

          if(!revertBarriers.empty())
            DoPipelineBarrier(commandBuffer, (uint32_t)revertBarriers.size(), revertBarriers.data());
        }

        // also finish any nested markers we truncated and didn't finish
        if(ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT)
          for(int i = 0; i < m_BakedCmdBufferInfo[BakedCommandBuffer].markerCount; i++)
            ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT(Unwrap(commandBuffer));

        if(m_DrawcallCallback)
          m_DrawcallCallback->PreEndCommandBuffer(commandBuffer);

        ObjDisp(commandBuffer)->EndCommandBuffer(Unwrap(commandBuffer));

        if(m_Partial[Primary].partialParent == BakedCommandBuffer)
          m_Partial[Primary].partialParent = ResourceId();
      }

      m_BakedCmdBufferInfo[CommandBuffer].curEventID = 0;
    }
    else
    {
      commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(BakedCommandBuffer);

      ObjDisp(commandBuffer)->EndCommandBuffer(Unwrap(commandBuffer));

      if(!m_BakedCmdBufferInfo[BakedCommandBuffer].curEvents.empty())
      {
        DrawcallDescription draw;
        draw.name = "API Calls";
        draw.flags |= DrawFlags::APICalls;

        AddDrawcall(draw, true);

        m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID++;
      }

      {
        if(GetDrawcallStack().size() > 1)
          GetDrawcallStack().pop_back();
      }

      {
        m_BakedCmdBufferInfo[BakedCommandBuffer].eventCount =
            m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID;
        m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID = 0;

        m_BakedCmdBufferInfo[BakedCommandBuffer].endChunk =
            uint32_t(m_StructuredFile->chunks.size() - 1);

        m_BakedCmdBufferInfo[CommandBuffer].curEventID = 0;
        m_BakedCmdBufferInfo[CommandBuffer].eventCount = 0;
        m_BakedCmdBufferInfo[CommandBuffer].drawCount = 0;
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
  VkResourceRecord *record = GetRecord(commandBuffer);
  RDCASSERT(record);

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(commandBuffer)->EndCommandBuffer(Unwrap(commandBuffer)));

  if(record)
  {
    // ensure that we have a matching begin
    RDCASSERT(record->bakedCommands);

    {
      CACHE_THREAD_SERIALISER();
      ser.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkEndCommandBuffer);
      Serialise_vkEndCommandBuffer(ser, commandBuffer);

      record->AddChunk(scope.Get());
    }

    record->Bake();
  }

  return ret;
}

VkResult WrappedVulkan::vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                             VkCommandBufferResetFlags flags)
{
  VkResourceRecord *record = GetRecord(commandBuffer);
  RDCASSERT(record);

  if(record)
  {
    // all we need to do is remove the existing baked commands.
    // The application will still need to call begin command buffer itself.
    // this function is essentially a driver hint as it cleans up implicitly
    // on begin.
    //
    // Because it's totally legal for an application to record, submit, reset,
    // record, submit again, and we need some way of referencing the two different
    // sets of commands on replay, our command buffers are given new unique IDs
    // each time they are begun, so on replay it looks like they were all unique
    // (albeit with the same properties for those that share a 'parent'). Hence,
    // we don't need to record or replay when a ResetCommandBuffer happens
    if(record->bakedCommands)
      record->bakedCommands->Delete(GetResourceManager());

    record->bakedCommands = NULL;
  }

  return ObjDisp(commandBuffer)->ResetCommandBuffer(Unwrap(commandBuffer), flags);
}

// Command buffer building functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginRenderPass(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   const VkRenderPassBeginInfo *pRenderPassBegin,
                                                   VkSubpassContents contents)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(RenderPassBegin, *pRenderPassBegin);
  SERIALISE_ELEMENT(contents);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkRenderPassBeginInfo unwrappedInfo = RenderPassBegin;
    unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
    unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkRenderPassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // always track this, for WrappedVulkan::IsDrawInRenderPass()
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass = 0;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass =
            GetResID(RenderPassBegin.renderPass);
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer =
            GetResID(RenderPassBegin.framebuffer);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderState(m_LastCmdBufferID, true))
        {
          m_Partial[Primary].renderPassActive = true;

          m_RenderState.subpass = 0;

          m_RenderState.renderPass = GetResID(RenderPassBegin.renderPass);
          m_RenderState.framebuffer = GetResID(RenderPassBegin.framebuffer);
          m_RenderState.renderArea = RenderPassBegin.renderArea;
        }

        ObjDisp(commandBuffer)->CmdBeginRenderPass(Unwrap(commandBuffer), &unwrappedInfo, contents);

        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        ResourceId cmd = GetResID(commandBuffer);
        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdBeginRenderPass(Unwrap(commandBuffer), &unwrappedInfo, contents);

      // track while reading, for fetching the right set of outputs in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass = GetResID(RenderPassBegin.renderPass);
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer =
          GetResID(RenderPassBegin.framebuffer);

      std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      DrawcallDescription draw;
      draw.name =
          StringFormat::Fmt("vkCmdBeginRenderPass(%s)", MakeRenderPassOpString(false).c_str());
      draw.flags |= DrawFlags::PassBoundary | DrawFlags::BeginPass;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                                         const VkRenderPassBeginInfo *pRenderPassBegin,
                                         VkSubpassContents contents)
{
  SCOPED_DBG_SINK();

  VkRenderPassBeginInfo unwrappedInfo = *pRenderPassBegin;
  unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
  unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkRenderPassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdBeginRenderPass(Unwrap(commandBuffer), &unwrappedInfo, contents));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginRenderPass);
    Serialise_vkCmdBeginRenderPass(ser, commandBuffer, pRenderPassBegin, contents);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);

    VkResourceRecord *fb = GetRecord(pRenderPassBegin->framebuffer);

    record->MarkResourceFrameReferenced(fb->GetResourceID(), eFrameRef_Read);
    for(size_t i = 0; fb->imageAttachments[i].barrier.sType; i++)
    {
      VkResourceRecord *att = fb->imageAttachments[i].record;
      if(att == NULL)
        break;

      record->MarkImageViewFrameReferenced(att, ImageRange(), eFrameRef_ReadBeforeWrite);
    }

    record->cmdInfo->framebuffer = fb;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdNextSubpass(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               VkSubpassContents contents)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(contents);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      // don't do anything if we're executing a single draw, NextSubpass is meaningless (and invalid
      // on a partial render pass)
      if(InRerecordRange(m_LastCmdBufferID) && m_FirstEventID != m_LastEventID)
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // always track this, for WrappedVulkan::IsDrawInRenderPass()
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass++;

        if(ShouldUpdateRenderState(m_LastCmdBufferID, true))
          m_RenderState.subpass++;

        ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), contents);

        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        ResourceId cmd = GetResID(commandBuffer);
        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), contents);

      // track while reading, for fetching the right set of outputs in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass++;

      std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("vkCmdNextSubpass() => %u",
                                    m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass);
      draw.flags |= DrawFlags::PassBoundary | DrawFlags::BeginPass | DrawFlags::EndPass;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), contents));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdNextSubpass);
    Serialise_vkCmdNextSubpass(ser, commandBuffer, contents);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndRenderPass(SerialiserType &ser, VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

        // always track this, for WrappedVulkan::IsDrawInRenderPass()
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass = ResourceId();
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer = ResourceId();

        if(ShouldUpdateRenderState(m_LastCmdBufferID, true))
        {
          m_Partial[Primary].renderPassActive = false;
        }

        ObjDisp(commandBuffer)->CmdEndRenderPass(Unwrap(commandBuffer));

        ResourceId cmd = GetResID(commandBuffer);
        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdEndRenderPass(Unwrap(commandBuffer));

      // fetch any queued indirect readbacks here
      for(const VkIndirectRecordData &indirectcopy :
          m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies)
        ExecuteIndirectReadback(commandBuffer, indirectcopy);

      m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies.clear();

      std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("vkCmdEndRenderPass(%s)", MakeRenderPassOpString(true).c_str());
      draw.flags |= DrawFlags::PassBoundary | DrawFlags::EndPass;

      AddDrawcall(draw, true);

      // track while reading, reset this to empty so AddDrawcall sets no outputs,
      // but only AFTER the above AddDrawcall (we want it grouped together)
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass = ResourceId();
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer = ResourceId();
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdEndRenderPass(Unwrap(commandBuffer)));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndRenderPass);
    Serialise_vkCmdEndRenderPass(ser, commandBuffer);

    record->AddChunk(scope.Get());

    VkResourceRecord *fb = record->cmdInfo->framebuffer;

    std::vector<VkImageMemoryBarrier> barriers;

    for(size_t i = 0; fb->imageAttachments[i].barrier.sType; i++)
    {
      if(fb->imageAttachments[i].barrier.oldLayout == fb->imageAttachments[i].barrier.newLayout)
        continue;

      barriers.push_back(fb->imageAttachments[i].barrier);
    }

    // apply the implicit layout transitions here
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      GetResourceManager()->RecordBarriers(GetRecord(commandBuffer)->cmdInfo->imgbarriers,
                                           m_ImageLayouts, (uint32_t)barriers.size(),
                                           barriers.data());
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginRenderPass2KHR(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       const VkRenderPassBeginInfo *pRenderPassBegin,
                                                       const VkSubpassBeginInfoKHR *pSubpassBeginInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(RenderPassBegin, *pRenderPassBegin);
  SERIALISE_ELEMENT_LOCAL(SubpassBegin, *pSubpassBeginInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkRenderPassBeginInfo unwrappedInfo = RenderPassBegin;
    unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
    unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);

    VkSubpassBeginInfoKHR unwrappedBeginInfo = SubpassBegin;

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext) +
                                  GetNextPatchSize(unwrappedBeginInfo.pNext));

    UnwrapNextChain(m_State, "VkRenderPassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedInfo);
    UnwrapNextChain(m_State, "VkSubpassBeginInfoKHR", tempMem,
                    (VkBaseInStructure *)&unwrappedBeginInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // always track this, for WrappedVulkan::IsDrawInRenderPass()
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass = 0;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass =
            GetResID(RenderPassBegin.renderPass);
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer =
            GetResID(RenderPassBegin.framebuffer);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderState(m_LastCmdBufferID, true))
        {
          m_Partial[Primary].renderPassActive = true;

          m_RenderState.subpass = 0;

          m_RenderState.renderPass = GetResID(RenderPassBegin.renderPass);
          m_RenderState.framebuffer = GetResID(RenderPassBegin.framebuffer);
          m_RenderState.renderArea = RenderPassBegin.renderArea;
        }

        ObjDisp(commandBuffer)
            ->CmdBeginRenderPass2KHR(Unwrap(commandBuffer), &unwrappedInfo, &unwrappedBeginInfo);

        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        ResourceId cmd = GetResID(commandBuffer);
        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdBeginRenderPass2KHR(Unwrap(commandBuffer), &unwrappedInfo, &unwrappedBeginInfo);

      // track while reading, for fetching the right set of outputs in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass = GetResID(RenderPassBegin.renderPass);
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer =
          GetResID(RenderPassBegin.framebuffer);

      std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      DrawcallDescription draw;
      draw.name =
          StringFormat::Fmt("vkCmdBeginRenderPass2KHR(%s)", MakeRenderPassOpString(false).c_str());
      draw.flags |= DrawFlags::PassBoundary | DrawFlags::BeginPass;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer,
                                             const VkRenderPassBeginInfo *pRenderPassBegin,
                                             const VkSubpassBeginInfoKHR *pSubpassBeginInfo)
{
  SCOPED_DBG_SINK();

  VkRenderPassBeginInfo unwrappedInfo = *pRenderPassBegin;
  unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
  unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);

  VkSubpassBeginInfoKHR unwrappedBeginInfo = *pSubpassBeginInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext) +
                                GetNextPatchSize(unwrappedBeginInfo.pNext));

  UnwrapNextChain(m_State, "VkRenderPassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedInfo);
  UnwrapNextChain(m_State, "VkSubpassBeginInfoKHR", tempMem,
                  (VkBaseInStructure *)&unwrappedBeginInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdBeginRenderPass2KHR(Unwrap(commandBuffer), &unwrappedInfo, &unwrappedBeginInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginRenderPass2KHR);
    Serialise_vkCmdBeginRenderPass2KHR(ser, commandBuffer, pRenderPassBegin, pSubpassBeginInfo);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);

    VkResourceRecord *fb = GetRecord(pRenderPassBegin->framebuffer);

    record->MarkResourceFrameReferenced(fb->GetResourceID(), eFrameRef_Read);
    for(size_t i = 0; fb->imageAttachments[i].barrier.sType; i++)
    {
      VkResourceRecord *att = fb->imageAttachments[i].record;
      if(att == NULL)
        break;

      record->MarkResourceFrameReferenced(att->baseResource, eFrameRef_ReadBeforeWrite);
      if(att->baseResourceMem != ResourceId())
        record->MarkResourceFrameReferenced(att->baseResourceMem, eFrameRef_Read);
      if(att->resInfo && att->resInfo->IsSparse())
        record->cmdInfo->sparse.insert(att->resInfo);
      record->cmdInfo->dirtied.insert(att->baseResource);
    }

    record->cmdInfo->framebuffer = fb;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdNextSubpass2KHR(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   const VkSubpassBeginInfoKHR *pSubpassBeginInfo,
                                                   const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(SubpassBegin, *pSubpassBeginInfo);
  SERIALISE_ELEMENT_LOCAL(SubpassEnd, *pSubpassEndInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSubpassBeginInfoKHR unwrappedBeginInfo = SubpassBegin;
    VkSubpassEndInfoKHR unwrappedEndInfo = SubpassEnd;

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedBeginInfo.pNext) +
                                  GetNextPatchSize(unwrappedEndInfo.pNext));

    UnwrapNextChain(m_State, "VkSubpassBeginInfoKHR", tempMem,
                    (VkBaseInStructure *)&unwrappedBeginInfo);
    UnwrapNextChain(m_State, "VkSubpassEndInfoKHR", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      // don't do anything if we're executing a single draw, NextSubpass is meaningless (and invalid
      // on a partial render pass)
      if(InRerecordRange(m_LastCmdBufferID) && m_FirstEventID != m_LastEventID)
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // always track this, for WrappedVulkan::IsDrawInRenderPass()
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass++;

        if(ShouldUpdateRenderState(m_LastCmdBufferID, true))
          m_RenderState.subpass++;

        ObjDisp(commandBuffer)
            ->CmdNextSubpass2KHR(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo);

        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        ResourceId cmd = GetResID(commandBuffer);
        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdNextSubpass2KHR(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo);

      // track while reading, for fetching the right set of outputs in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass++;

      std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("vkCmdNextSubpass2KHR() => %u",
                                    m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass);
      draw.flags |= DrawFlags::PassBoundary | DrawFlags::BeginPass | DrawFlags::EndPass;

      AddDrawcall(draw, true);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer,
                                         const VkSubpassBeginInfoKHR *pSubpassBeginInfo,
                                         const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
  SCOPED_DBG_SINK();

  VkSubpassBeginInfoKHR unwrappedBeginInfo = *pSubpassBeginInfo;
  VkSubpassEndInfoKHR unwrappedEndInfo = *pSubpassEndInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedBeginInfo.pNext) +
                                GetNextPatchSize(unwrappedEndInfo.pNext));

  UnwrapNextChain(m_State, "VkSubpassBeginInfoKHR", tempMem,
                  (VkBaseInStructure *)&unwrappedBeginInfo);
  UnwrapNextChain(m_State, "VkSubpassEndInfoKHR", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdNextSubpass2KHR(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdNextSubpass2KHR);
    Serialise_vkCmdNextSubpass2KHR(ser, commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndRenderPass2KHR(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(SubpassEnd, *pSubpassEndInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSubpassEndInfoKHR unwrappedEndInfo = SubpassEnd;

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedEndInfo.pNext));

    UnwrapNextChain(m_State, "VkSubpassEndInfoKHR", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

        // always track this, for WrappedVulkan::IsDrawInRenderPass()
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass = ResourceId();
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer = ResourceId();

        if(ShouldUpdateRenderState(m_LastCmdBufferID, true))
        {
          m_Partial[Primary].renderPassActive = false;
        }

        ObjDisp(commandBuffer)->CmdEndRenderPass2KHR(Unwrap(commandBuffer), &unwrappedEndInfo);

        ResourceId cmd = GetResID(commandBuffer);
        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdEndRenderPass2KHR(Unwrap(commandBuffer), &unwrappedEndInfo);

      std::vector<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      DrawcallDescription draw;
      draw.name =
          StringFormat::Fmt("vkCmdEndRenderPass2KHR(%s)", MakeRenderPassOpString(true).c_str());
      draw.flags |= DrawFlags::PassBoundary | DrawFlags::EndPass;

      AddDrawcall(draw, true);

      // track while reading, reset this to empty so AddDrawcall sets no outputs,
      // but only AFTER the above AddDrawcall (we want it grouped together)
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass = ResourceId();
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer = ResourceId();
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer,
                                           const VkSubpassEndInfoKHR *pSubpassEndInfo)
{
  SCOPED_DBG_SINK();

  VkSubpassEndInfoKHR unwrappedEndInfo = *pSubpassEndInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedEndInfo.pNext));

  UnwrapNextChain(m_State, "VkSubpassEndInfoKHR", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdEndRenderPass2KHR(Unwrap(commandBuffer), &unwrappedEndInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndRenderPass2KHR);
    Serialise_vkCmdEndRenderPass2KHR(ser, commandBuffer, pSubpassEndInfo);

    record->AddChunk(scope.Get());

    VkResourceRecord *fb = record->cmdInfo->framebuffer;

    std::vector<VkImageMemoryBarrier> barriers;

    for(size_t i = 0; fb->imageAttachments[i].barrier.sType; i++)
    {
      if(fb->imageAttachments[i].barrier.oldLayout == fb->imageAttachments[i].barrier.newLayout)
        continue;

      barriers.push_back(fb->imageAttachments[i].barrier);
    }

    // apply the implicit layout transitions here
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      GetResourceManager()->RecordBarriers(GetRecord(commandBuffer)->cmdInfo->imgbarriers,
                                           m_ImageLayouts, (uint32_t)barriers.size(),
                                           barriers.data());
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindPipeline(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkPipelineBindPoint pipelineBindPoint,
                                                VkPipeline pipeline)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineBindPoint);
  SERIALISE_ELEMENT(pipeline);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        ResourceId liveid = GetResID(pipeline);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
          {
            m_RenderState.compute.pipeline = liveid;
          }
          else
          {
            m_RenderState.graphics.pipeline = liveid;

            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicViewport])
            {
              m_RenderState.views = m_CreationInfo.m_Pipeline[liveid].viewports;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicScissor])
            {
              m_RenderState.scissors = m_CreationInfo.m_Pipeline[liveid].scissors;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicLineWidth])
            {
              m_RenderState.lineWidth = m_CreationInfo.m_Pipeline[liveid].lineWidth;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicDepthBias])
            {
              m_RenderState.bias.depth = m_CreationInfo.m_Pipeline[liveid].depthBiasConstantFactor;
              m_RenderState.bias.biasclamp = m_CreationInfo.m_Pipeline[liveid].depthBiasClamp;
              m_RenderState.bias.slope = m_CreationInfo.m_Pipeline[liveid].depthBiasSlopeFactor;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicBlendConstants])
            {
              memcpy(m_RenderState.blendConst, m_CreationInfo.m_Pipeline[liveid].blendConst,
                     sizeof(float) * 4);
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicDepthBounds])
            {
              m_RenderState.mindepth = m_CreationInfo.m_Pipeline[liveid].minDepthBounds;
              m_RenderState.maxdepth = m_CreationInfo.m_Pipeline[liveid].maxDepthBounds;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicStencilCompareMask])
            {
              m_RenderState.front.compare = m_CreationInfo.m_Pipeline[liveid].front.compareMask;
              m_RenderState.back.compare = m_CreationInfo.m_Pipeline[liveid].back.compareMask;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicStencilWriteMask])
            {
              m_RenderState.front.write = m_CreationInfo.m_Pipeline[liveid].front.writeMask;
              m_RenderState.back.write = m_CreationInfo.m_Pipeline[liveid].back.writeMask;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicStencilReference])
            {
              m_RenderState.front.ref = m_CreationInfo.m_Pipeline[liveid].front.reference;
              m_RenderState.back.ref = m_CreationInfo.m_Pipeline[liveid].back.reference;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicSampleLocationsEXT])
            {
              m_RenderState.sampleLocations.locations =
                  m_CreationInfo.m_Pipeline[liveid].sampleLocations.locations;
              m_RenderState.sampleLocations.gridSize =
                  m_CreationInfo.m_Pipeline[liveid].sampleLocations.gridSize;
              m_RenderState.sampleLocations.sampleCount =
                  m_CreationInfo.m_Pipeline[liveid].rasterizationSamples;
            }
            if(!m_CreationInfo.m_Pipeline[liveid].dynamicStates[VkDynamicDiscardRectangleEXT])
            {
              m_RenderState.discardRectangles = m_CreationInfo.m_Pipeline[liveid].discardRectangles;
            }
          }
        }
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }
    else
    {
      // track while reading, as we need to bind current topology & index byte width in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.pipeline = GetResID(pipeline);
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdBindPipeline(Unwrap(commandBuffer), pipelineBindPoint, Unwrap(pipeline));
  }

  return true;
}

void WrappedVulkan::vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                      VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdBindPipeline(Unwrap(commandBuffer), pipelineBindPoint, Unwrap(pipeline)));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindPipeline);
    Serialise_vkCmdBindPipeline(ser, commandBuffer, pipelineBindPoint, pipeline);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(pipeline), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindDescriptorSets(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t firstSet, uint32_t setCount,
    const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
    const uint32_t *pDynamicOffsets)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineBindPoint);
  SERIALISE_ELEMENT(layout);
  SERIALISE_ELEMENT(firstSet);
  SERIALISE_ELEMENT(setCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorSets, setCount);
  SERIALISE_ELEMENT(dynamicOffsetCount);
  SERIALISE_ELEMENT_ARRAY(pDynamicOffsets, dynamicOffsetCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        ObjDisp(commandBuffer)
            ->CmdBindDescriptorSets(Unwrap(commandBuffer), pipelineBindPoint, Unwrap(layout),
                                    firstSet, setCount, UnwrapArray(pDescriptorSets, setCount),
                                    dynamicOffsetCount, pDynamicOffsets);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          std::vector<VulkanStatePipeline::DescriptorAndOffsets> &descsets =
              (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
                  ? m_RenderState.graphics.descSets
                  : m_RenderState.compute.descSets;

          // expand as necessary
          if(descsets.size() < firstSet + setCount)
            descsets.resize(firstSet + setCount);

          const std::vector<ResourceId> &descSetLayouts =
              m_CreationInfo.m_PipelineLayout[GetResID(layout)].descSetLayouts;

          const uint32_t *offsIter = pDynamicOffsets;
          uint32_t dynConsumed = 0;

          // consume the offsets linearly along the descriptor set layouts
          for(uint32_t i = 0; i < setCount; i++)
          {
            descsets[firstSet + i].descSet = GetResID(pDescriptorSets[i]);
            uint32_t dynCount =
                m_CreationInfo.m_DescSetLayout[descSetLayouts[firstSet + i]].dynamicCount;
            descsets[firstSet + i].offsets.assign(offsIter, offsIter + dynCount);
            offsIter += dynCount;
            dynConsumed += dynCount;
            RDCASSERT(dynConsumed <= dynamicOffsetCount);
          }

          // if there are dynamic offsets, bake them into the current bindings by alias'ing
          // the image layout member (which is never used for buffer views).
          // This lets us look it up easily when we want to show the current pipeline state
          RDCCOMPILE_ASSERT(sizeof(VkImageLayout) >= sizeof(uint32_t),
                            "Can't alias image layout for dynamic offset!");
          if(dynamicOffsetCount > 0)
          {
            uint32_t o = 0;

            // spec states that dynamic offsets precisely match all the offsets needed for these
            // sets, in order of set N before set N+1, binding X before binding X+1 within a set,
            // and in array element order within a binding
            for(uint32_t i = 0; i < setCount; i++)
            {
              ResourceId descId = GetResID(pDescriptorSets[i]);
              const DescSetLayout &layoutinfo =
                  m_CreationInfo.m_DescSetLayout[descSetLayouts[firstSet + i]];

              for(size_t b = 0; b < layoutinfo.bindings.size(); b++)
              {
                // not dynamic, doesn't need an offset
                if(layoutinfo.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
                   layoutinfo.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                  continue;

                // assign every array element an offset according to array size
                for(uint32_t a = 0; a < layoutinfo.bindings[b].descriptorCount; a++)
                {
                  RDCASSERT(o < dynamicOffsetCount);
                  uint32_t *alias =
                      (uint32_t *)&m_DescriptorSetState[descId].currentBindings[b][a].imageInfo.imageLayout;
                  *alias = pDynamicOffsets[o++];
                }
              }
            }
          }
        }
      }
    }
    else
    {
      // track while reading, as we need to track resource usage
      std::vector<BakedCmdBufferInfo::CmdBufferState::DescriptorAndOffsets> &descsets =
          (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
              ? m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphicsDescSets
              : m_BakedCmdBufferInfo[m_LastCmdBufferID].state.computeDescSets;

      // expand as necessary
      if(descsets.size() < firstSet + setCount)
        descsets.resize(firstSet + setCount);

      for(uint32_t i = 0; i < setCount; i++)
        descsets[firstSet + i].descSet = GetResID(pDescriptorSets[i]);

      ObjDisp(commandBuffer)
          ->CmdBindDescriptorSets(Unwrap(commandBuffer), pipelineBindPoint, Unwrap(layout),
                                  firstSet, setCount, UnwrapArray(pDescriptorSets, setCount),
                                  dynamicOffsetCount, pDynamicOffsets);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                                            VkPipelineBindPoint pipelineBindPoint,
                                            VkPipelineLayout layout, uint32_t firstSet,
                                            uint32_t setCount, const VkDescriptorSet *pDescriptorSets,
                                            uint32_t dynamicOffsetCount,
                                            const uint32_t *pDynamicOffsets)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBindDescriptorSets(Unwrap(commandBuffer), pipelineBindPoint,
                                                  Unwrap(layout), firstSet, setCount,
                                                  UnwrapArray(pDescriptorSets, setCount),
                                                  dynamicOffsetCount, pDynamicOffsets));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindDescriptorSets);
    Serialise_vkCmdBindDescriptorSets(ser, commandBuffer, pipelineBindPoint, layout, firstSet,
                                      setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
    record->cmdInfo->boundDescSets.insert(pDescriptorSets, pDescriptorSets + setCount);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindVertexBuffers(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     uint32_t firstBinding, uint32_t bindingCount,
                                                     const VkBuffer *pBuffers,
                                                     const VkDeviceSize *pOffsets)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBinding);
  SERIALISE_ELEMENT(bindingCount);
  SERIALISE_ELEMENT_ARRAY(pBuffers, bindingCount);
  SERIALISE_ELEMENT_ARRAY(pOffsets, bindingCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)
            ->CmdBindVertexBuffers(Unwrap(commandBuffer), firstBinding, bindingCount,
                                   UnwrapArray(pBuffers, bindingCount), pOffsets);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(m_RenderState.vbuffers.size() < firstBinding + bindingCount)
            m_RenderState.vbuffers.resize(firstBinding + bindingCount);

          for(uint32_t i = 0; i < bindingCount; i++)
          {
            m_RenderState.vbuffers[firstBinding + i].buf = GetResID(pBuffers[i]);
            m_RenderState.vbuffers[firstBinding + i].offs = pOffsets[i];
          }
        }
      }
    }
    else
    {
      // track while reading, as we need to track resource usage
      if(m_BakedCmdBufferInfo[m_LastCmdBufferID].state.vbuffers.size() < firstBinding + bindingCount)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.vbuffers.resize(firstBinding + bindingCount);

      for(uint32_t i = 0; i < bindingCount; i++)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.vbuffers[firstBinding + i] =
            GetResID(pBuffers[i]);

      ObjDisp(commandBuffer)
          ->CmdBindVertexBuffers(Unwrap(commandBuffer), firstBinding, bindingCount,
                                 UnwrapArray(pBuffers, bindingCount), pOffsets);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding,
                                           uint32_t bindingCount, const VkBuffer *pBuffers,
                                           const VkDeviceSize *pOffsets)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBindVertexBuffers(Unwrap(commandBuffer), firstBinding, bindingCount,
                                                 UnwrapArray(pBuffers, bindingCount), pOffsets));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindVertexBuffers);
    Serialise_vkCmdBindVertexBuffers(ser, commandBuffer, firstBinding, bindingCount, pBuffers,
                                     pOffsets);

    record->AddChunk(scope.Get());
    for(uint32_t i = 0; i < bindingCount; i++)
    {
      record->MarkBufferFrameReferenced(GetRecord(pBuffers[i]), pOffsets[i], VK_WHOLE_SIZE,
                                        eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindIndexBuffer(SerialiserType &ser,
                                                   VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                   VkDeviceSize offset, VkIndexType indexType)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer);
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(indexType);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)
            ->CmdBindIndexBuffer(Unwrap(commandBuffer), Unwrap(buffer), offset, indexType);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.ibuffer.buf = GetResID(buffer);
          m_RenderState.ibuffer.offs = offset;
          m_RenderState.ibuffer.bytewidth = indexType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
        }
      }
    }
    else
    {
      // track while reading, as we need to bind current topology & index byte width in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.idxWidth =
          (indexType == VK_INDEX_TYPE_UINT32 ? 4 : 2);

      // track while reading, as we need to track resource usage
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.ibuffer = GetResID(buffer);

      ObjDisp(commandBuffer)->CmdBindIndexBuffer(Unwrap(commandBuffer), Unwrap(buffer), offset, indexType);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                         VkDeviceSize offset, VkIndexType indexType)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdBindIndexBuffer(Unwrap(commandBuffer), Unwrap(buffer), offset, indexType));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindIndexBuffer);
    Serialise_vkCmdBindIndexBuffer(ser, commandBuffer, buffer, offset, indexType);

    record->AddChunk(scope.Get());
    record->MarkBufferFrameReferenced(GetRecord(buffer), 0, VK_WHOLE_SIZE, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdUpdateBuffer(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkBuffer destBuffer, VkDeviceSize destOffset,
                                                VkDeviceSize dataSize, const uint32_t *pData)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(destBuffer);
  SERIALISE_ELEMENT(destOffset);
  SERIALISE_ELEMENT(dataSize);

  // serialise as void* so it goes through as a buffer, not an actual array of integers.
  const void *Data = (const void *)pData;
  SERIALISE_ELEMENT_ARRAY(Data, dataSize);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)
          ->CmdUpdateBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset, dataSize, Data);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer destBuffer,
                                      VkDeviceSize destOffset, VkDeviceSize dataSize,
                                      const uint32_t *pData)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdUpdateBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset,
                                            dataSize, pData));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdUpdateBuffer);
    Serialise_vkCmdUpdateBuffer(ser, commandBuffer, destBuffer, destOffset, dataSize, pData);

    record->AddChunk(scope.Get());

    record->MarkBufferFrameReferenced(GetRecord(destBuffer), destOffset, dataSize,
                                      eFrameRef_CompleteWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdFillBuffer(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              VkBuffer destBuffer, VkDeviceSize destOffset,
                                              VkDeviceSize fillSize, uint32_t data)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(destBuffer);
  SERIALISE_ELEMENT(destOffset);
  SERIALISE_ELEMENT(fillSize);
  SERIALISE_ELEMENT(data);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)
          ->CmdFillBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset, fillSize, data);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer destBuffer,
                                    VkDeviceSize destOffset, VkDeviceSize fillSize, uint32_t data)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdFillBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset, fillSize, data));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdFillBuffer);
    Serialise_vkCmdFillBuffer(ser, commandBuffer, destBuffer, destOffset, fillSize, data);

    record->AddChunk(scope.Get());

    record->MarkBufferFrameReferenced(GetRecord(destBuffer), destOffset, fillSize,
                                      eFrameRef_CompleteWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdPushConstants(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                 VkPipelineLayout layout,
                                                 VkShaderStageFlags stageFlags, uint32_t start,
                                                 uint32_t length, const void *values)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(layout);
  SERIALISE_ELEMENT_TYPED(VkShaderStageFlagBits, stageFlags).TypedAs("VkShaderStageFlags"_lit);
  SERIALISE_ELEMENT(start);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT_ARRAY(values, length);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)
            ->CmdPushConstants(Unwrap(commandBuffer), Unwrap(layout), stageFlags, start, length,
                               values);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          RDCASSERT(start + length < (uint32_t)ARRAY_COUNT(m_RenderState.pushconsts));

          memcpy(m_RenderState.pushconsts + start, values, length);

          m_RenderState.pushConstSize = RDCMAX(m_RenderState.pushConstSize, start + length);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdPushConstants(Unwrap(commandBuffer), Unwrap(layout), stageFlags, start, length,
                             values);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                                       VkShaderStageFlags stageFlags, uint32_t start,
                                       uint32_t length, const void *values)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdPushConstants(Unwrap(commandBuffer), Unwrap(layout), stageFlags,
                                             start, length, values));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdPushConstants);
    Serialise_vkCmdPushConstants(ser, commandBuffer, layout, stageFlags, start, length, values);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdPipelineBarrier(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags destStageMask, VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits, srcStageMask)
      .TypedAs("VkPipelineStageFlags"_lit);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits, destStageMask)
      .TypedAs("VkPipelineStageFlags"_lit);
  SERIALISE_ELEMENT_TYPED(VkDependencyFlagBits, dependencyFlags).TypedAs("VkDependencyFlags"_lit);
  SERIALISE_ELEMENT(memoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pMemoryBarriers, memoryBarrierCount);
  SERIALISE_ELEMENT(bufferMemoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pBufferMemoryBarriers, bufferMemoryBarrierCount);
  SERIALISE_ELEMENT(imageMemoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pImageMemoryBarriers, imageMemoryBarrierCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  std::vector<VkImageMemoryBarrier> imgBarriers;
  std::vector<VkBufferMemoryBarrier> bufBarriers;

  // it's possible for buffer or image to be NULL if it refers to a resource that is otherwise
  // not in the log (barriers do not mark resources referenced). If the resource in question does
  // not exist, then it's safe to skip this barrier.
  //
  // Since it's a convenient place, we unwrap at the same time.
  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    for(uint32_t i = 0; i < bufferMemoryBarrierCount; i++)
    {
      if(pBufferMemoryBarriers[i].buffer != VK_NULL_HANDLE)
      {
        bufBarriers.push_back(pBufferMemoryBarriers[i]);
        bufBarriers.back().buffer = Unwrap(bufBarriers.back().buffer);

        RemapQueueFamilyIndices(bufBarriers.back().srcQueueFamilyIndex,
                                bufBarriers.back().dstQueueFamilyIndex);

        if(IsLoading(m_State))
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              GetResID(pBufferMemoryBarriers[i].buffer),
              EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::Barrier)));
        }
      }
    }

    for(uint32_t i = 0; i < imageMemoryBarrierCount; i++)
    {
      if(pImageMemoryBarriers[i].image != VK_NULL_HANDLE)
      {
        imgBarriers.push_back(pImageMemoryBarriers[i]);
        imgBarriers.back().image = Unwrap(imgBarriers.back().image);

        RemapQueueFamilyIndices(imgBarriers.back().srcQueueFamilyIndex,
                                imgBarriers.back().dstQueueFamilyIndex);

        if(IsLoading(m_State))
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              GetResID(pImageMemoryBarriers[i].image),
              EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::Barrier)));
        }
      }
    }

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ResourceId cmd = GetResID(commandBuffer);
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      // now sanitise layouts before passing to vulkan
      for(VkImageMemoryBarrier &barrier : imgBarriers)
      {
        SanitiseOldImageLayout(barrier.oldLayout);
        SanitiseNewImageLayout(barrier.newLayout);
      }

      ObjDisp(commandBuffer)
          ->CmdPipelineBarrier(Unwrap(commandBuffer), srcStageMask, destStageMask, dependencyFlags,
                               memoryBarrierCount, pMemoryBarriers, (uint32_t)bufBarriers.size(),
                               bufBarriers.data(), (uint32_t)imgBarriers.size(), imgBarriers.data());
    }
  }

  return true;
}

void WrappedVulkan::vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags destStageMask, VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  SCOPED_DBG_SINK();

  {
    byte *memory = GetTempMemory(sizeof(VkBufferMemoryBarrier) * bufferMemoryBarrierCount +
                                 sizeof(VkImageMemoryBarrier) * imageMemoryBarrierCount);

    VkImageMemoryBarrier *im = (VkImageMemoryBarrier *)memory;
    VkBufferMemoryBarrier *buf = (VkBufferMemoryBarrier *)(im + imageMemoryBarrierCount);

    for(uint32_t i = 0; i < bufferMemoryBarrierCount; i++)
    {
      buf[i] = pBufferMemoryBarriers[i];
      buf[i].buffer = Unwrap(buf[i].buffer);
    }

    for(uint32_t i = 0; i < imageMemoryBarrierCount; i++)
    {
      im[i] = pImageMemoryBarriers[i];
      im[i].image = Unwrap(im[i].image);
    }

    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                            ->CmdPipelineBarrier(Unwrap(commandBuffer), srcStageMask, destStageMask,
                                                 dependencyFlags, memoryBarrierCount,
                                                 pMemoryBarriers, bufferMemoryBarrierCount, buf,
                                                 imageMemoryBarrierCount, im));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdPipelineBarrier);
    Serialise_vkCmdPipelineBarrier(ser, commandBuffer, srcStageMask, destStageMask, dependencyFlags,
                                   memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
                                   pBufferMemoryBarriers, imageMemoryBarrierCount,
                                   pImageMemoryBarriers);

    record->AddChunk(scope.Get());

    if(imageMemoryBarrierCount > 0)
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      GetResourceManager()->RecordBarriers(GetRecord(commandBuffer)->cmdInfo->imgbarriers,
                                           m_ImageLayouts, imageMemoryBarrierCount,
                                           pImageMemoryBarriers);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdWriteTimestamp(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  VkPipelineStageFlagBits pipelineStage,
                                                  VkQueryPool queryPool, uint32_t query)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineStage);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(query);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)
          ->CmdWriteTimestamp(Unwrap(commandBuffer), pipelineStage, Unwrap(queryPool), query);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdWriteTimestamp(VkCommandBuffer commandBuffer,
                                        VkPipelineStageFlagBits pipelineStage,
                                        VkQueryPool queryPool, uint32_t query)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdWriteTimestamp(Unwrap(commandBuffer), pipelineStage, Unwrap(queryPool), query));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdWriteTimestamp);
    Serialise_vkCmdWriteTimestamp(ser, commandBuffer, pipelineStage, queryPool, query);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyQueryPoolResults(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery,
    uint32_t queryCount, VkBuffer destBuffer, VkDeviceSize destOffset, VkDeviceSize destStride,
    VkQueryResultFlags flags)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(firstQuery);
  SERIALISE_ELEMENT(queryCount);
  SERIALISE_ELEMENT(destBuffer);
  SERIALISE_ELEMENT(destOffset);
  SERIALISE_ELEMENT(destStride);
  SERIALISE_ELEMENT_TYPED(VkQueryResultFlagBits, flags).TypedAs("VkQueryResultFlags"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)
          ->CmdCopyQueryPoolResults(Unwrap(commandBuffer), Unwrap(queryPool), firstQuery,
                                    queryCount, Unwrap(destBuffer), destOffset, destStride, flags);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                              uint32_t firstQuery, uint32_t queryCount,
                                              VkBuffer destBuffer, VkDeviceSize destOffset,
                                              VkDeviceSize destStride, VkQueryResultFlags flags)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdCopyQueryPoolResults(Unwrap(commandBuffer), Unwrap(queryPool),
                                                    firstQuery, queryCount, Unwrap(destBuffer),
                                                    destOffset, destStride, flags));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyQueryPoolResults);
    Serialise_vkCmdCopyQueryPoolResults(ser, commandBuffer, queryPool, firstQuery, queryCount,
                                        destBuffer, destOffset, destStride, flags);

    record->AddChunk(scope.Get());

    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);

    VkDeviceSize size = (queryCount - 1) * destStride + 4;
    if(flags & VK_QUERY_RESULT_64_BIT)
    {
      size += 4;
    }
    record->MarkBufferFrameReferenced(GetRecord(destBuffer), destOffset, size,
                                      eFrameRef_PartialWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginQuery(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              VkQueryPool queryPool, uint32_t query,
                                              VkQueryControlFlags flags)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(query);
  SERIALISE_ELEMENT_TYPED(VkQueryControlFlagBits, flags).TypedAs("VkQueryControlFlags"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdBeginQuery(Unwrap(commandBuffer), Unwrap(queryPool), query, flags);
  }

  return true;
}

void WrappedVulkan::vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                    uint32_t query, VkQueryControlFlags flags)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdBeginQuery(Unwrap(commandBuffer), Unwrap(queryPool), query, flags));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginQuery);
    Serialise_vkCmdBeginQuery(ser, commandBuffer, queryPool, query, flags);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndQuery(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                            VkQueryPool queryPool, uint32_t query)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(query);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdEndQuery(Unwrap(commandBuffer), Unwrap(queryPool), query);
  }

  return true;
}

void WrappedVulkan::vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdEndQuery(Unwrap(commandBuffer), Unwrap(queryPool), query));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndQuery);
    Serialise_vkCmdEndQuery(ser, commandBuffer, queryPool, query);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdResetQueryPool(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  VkQueryPool queryPool, uint32_t firstQuery,
                                                  uint32_t queryCount)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(firstQuery);
  SERIALISE_ELEMENT(queryCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)
          ->CmdResetQueryPool(Unwrap(commandBuffer), Unwrap(queryPool), firstQuery, queryCount);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                        uint32_t firstQuery, uint32_t queryCount)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdResetQueryPool(Unwrap(commandBuffer), Unwrap(queryPool), firstQuery, queryCount));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdResetQueryPool);
    Serialise_vkCmdResetQueryPool(ser, commandBuffer, queryPool, firstQuery, queryCount);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdExecuteCommands(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   uint32_t commandBufferCount,
                                                   const VkCommandBuffer *pCommandBuffers)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(commandBufferCount);
  SERIALISE_ELEMENT_ARRAY(pCommandBuffers, commandBufferCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsLoading(m_State))
    {
      // execute the commands
      ObjDisp(commandBuffer)
          ->CmdExecuteCommands(Unwrap(commandBuffer), commandBufferCount,
                               UnwrapArray(pCommandBuffers, commandBufferCount));

      // merge barriers into parent command buffer
      for(uint32_t i = 0; i < commandBufferCount; i++)
      {
        GetResourceManager()->MergeBarriers(
            m_BakedCmdBufferInfo[GetResID(commandBuffer)].imgbarriers,
            m_BakedCmdBufferInfo[GetResID(pCommandBuffers[i])].imgbarriers);
      }

      // append deferred indirect copies
      {
        std::vector<VkIndirectRecordData> &dstIndirect =
            m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies;

        for(uint32_t i = 0; i < commandBufferCount; i++)
        {
          // indirectCopies are stored in m_BakedCmdBufferInfo[m_LastCmdBufferID] which is an
          // original ID
          ResourceId origId = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[i]));
          const std::vector<VkIndirectRecordData> &srcIndirect =
              m_BakedCmdBufferInfo[origId].indirectCopies;

          dstIndirect.insert(dstIndirect.end(), srcIndirect.begin(), srcIndirect.end());
        }
      }

      AddEvent();

      DrawcallDescription draw;
      draw.name = StringFormat::Fmt("vkCmdExecuteCommands(%u)", commandBufferCount);
      draw.flags = DrawFlags::CmdList | DrawFlags::PushMarker;

      AddDrawcall(draw, true);

      BakedCmdBufferInfo &parentCmdBufInfo = m_BakedCmdBufferInfo[m_LastCmdBufferID];

      parentCmdBufInfo.curEventID++;

      // should we add framebuffer usage to the child draws.
      bool framebufferUsage = parentCmdBufInfo.state.renderPass != ResourceId() &&
                              parentCmdBufInfo.state.framebuffer != ResourceId();

      for(uint32_t c = 0; c < commandBufferCount; c++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[c]));

        BakedCmdBufferInfo &cmdBufInfo = m_BakedCmdBufferInfo[cmd];

        // add a fake marker
        DrawcallDescription marker;
        marker.name = StringFormat::Fmt("=> vkCmdExecuteCommands()[%u]: vkBeginCommandBuffer(%s)",
                                        c, ToStr(cmd).c_str());
        marker.flags = DrawFlags::PassBoundary | DrawFlags::BeginPass;
        AddEvent();

        parentCmdBufInfo.curEvents.back().chunkIndex = cmdBufInfo.beginChunk;

        AddDrawcall(marker, true);
        parentCmdBufInfo.curEventID++;

        if(m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass == ResourceId() &&
           (cmdBufInfo.beginFlags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT))
        {
          AddDebugMessage(
              MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
              "Executing a command buffer with RENDER_PASS_CONTINUE_BIT outside of render pass");
        }

        // insert the baked command buffer in-line into this list of notes, assigning new event and
        // drawIDs
        parentCmdBufInfo.draw->InsertAndUpdateIDs(*cmdBufInfo.draw, parentCmdBufInfo.curEventID,
                                                  parentCmdBufInfo.drawCount);

        if(framebufferUsage)
        {
          size_t total = parentCmdBufInfo.draw->children.size();
          size_t numChildren = cmdBufInfo.draw->children.size();

          // iterate through the newly added draws, and recursively add usage to them using our
          // primary command buffer's state
          for(size_t i = 0; i < numChildren; i++)
          {
            AddFramebufferUsageAllChildren(parentCmdBufInfo.draw->children[total - numChildren + i],
                                           parentCmdBufInfo.state.renderPass,
                                           parentCmdBufInfo.state.framebuffer,
                                           parentCmdBufInfo.state.subpass);
          }
        }

        for(size_t i = 0; i < cmdBufInfo.debugMessages.size(); i++)
        {
          parentCmdBufInfo.debugMessages.push_back(cmdBufInfo.debugMessages[i]);
          parentCmdBufInfo.debugMessages.back().eventId += parentCmdBufInfo.curEventID;
        }

        // only primary command buffers can be submitted
        m_Partial[Secondary].cmdBufferSubmits[cmd].push_back(parentCmdBufInfo.curEventID);

        parentCmdBufInfo.draw->executedCmds.push_back(cmd);

        parentCmdBufInfo.curEventID += cmdBufInfo.eventCount;
        parentCmdBufInfo.drawCount += cmdBufInfo.drawCount;

        marker.name = StringFormat::Fmt("=> vkCmdExecuteCommands()[%u]: vkEndCommandBuffer(%s)", c,
                                        ToStr(cmd).c_str());
        marker.flags = DrawFlags::PassBoundary | DrawFlags::EndPass;
        AddEvent();
        AddDrawcall(marker, true);
        parentCmdBufInfo.curEventID++;
      }

      // add an extra pop marker
      draw = DrawcallDescription();
      draw.flags = DrawFlags::PopMarker;

      AddDrawcall(draw, true);

      // don't change curEventID here, as it will be incremented outside in the outer
      // loop for the EXEC_CMDS event. in vkQueueSubmit we need to decrement curEventID
      // because we don't have the extra popmarker event to 'absorb' the outer loop's
      // increment, and it incremented once too many for the last vkEndCommandBuffer
      // setmarker event in the loop over all commands
    }
    else
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        BakedCmdBufferInfo &parentCmdBufInfo = m_BakedCmdBufferInfo[m_LastCmdBufferID];

        // if we're replaying a range but not from the start, we are guaranteed to only be replaying
        // one of our executed command buffers and doing it to an outside command buffer. The outer
        // loop will be doing SetOffset() to jump to each event, and any time we land here is just
        // for
        // the markers we've added, which have this file offset, so just skip all of our work.
        if(m_FirstEventID > 1 && m_FirstEventID + 1 < m_LastEventID)
          return true;

        // account for the execute commands event
        parentCmdBufInfo.curEventID++;

        uint32_t startEID = parentCmdBufInfo.curEventID + m_Partial[Primary].baseEvent;

        // advance m_CurEventID to match the events added when reading
        for(uint32_t c = 0; c < commandBufferCount; c++)
        {
          ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[c]));

          // propagate renderpass state
          m_BakedCmdBufferInfo[cmd].state.renderPass = parentCmdBufInfo.state.renderPass;
          m_BakedCmdBufferInfo[cmd].state.subpass = parentCmdBufInfo.state.subpass;
          m_BakedCmdBufferInfo[cmd].state.framebuffer = parentCmdBufInfo.state.framebuffer;

          // propagate conditional rendering
          if(m_BakedCmdBufferInfo[cmd].inheritConditionalRendering &&
             parentCmdBufInfo.state.conditionalRendering.buffer != ResourceId() &&
             (startEID + m_BakedCmdBufferInfo[cmd].eventCount >= m_LastEventID))
          {
            m_RenderState.conditionalRendering.buffer =
                parentCmdBufInfo.state.conditionalRendering.buffer;
            m_RenderState.conditionalRendering.offset =
                parentCmdBufInfo.state.conditionalRendering.offset;
            m_RenderState.conditionalRendering.flags =
                parentCmdBufInfo.state.conditionalRendering.flags;
          }

          // 2 extra for the virtual labels around the command buffer
          parentCmdBufInfo.curEventID += 2 + m_BakedCmdBufferInfo[cmd].eventCount;
        }

        // same accounting for the outer loop as above means no need to change anything here

        if(commandBufferCount == 0)
        {
          // do nothing, don't bother with the logic below
        }
        else if(m_FirstEventID == m_LastEventID)
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("ExecuteCommands no OnlyDraw %u", m_FirstEventID);
#endif
        }
        else if(m_LastEventID <= startEID)
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("ExecuteCommands no replay %u == %u", m_LastEventID, startEID);
#endif
        }
        else
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("ExecuteCommands re-recording from %u", startEID);
#endif

          uint32_t eid = startEID;

          std::vector<VkCommandBuffer> rerecordedCmds;

          for(uint32_t c = 0; c < commandBufferCount; c++)
          {
            ResourceId cmdid = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[c]));

            // account for the virtual vkBeginCommandBuffer label at the start of the events here
            // so it matches up to baseEvent
            eid++;

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            uint32_t end = eid + m_BakedCmdBufferInfo[cmdid].eventCount;
#endif

            if(eid <= m_LastEventID)
            {
              VkCommandBuffer cmd = RerecordCmdBuf(cmdid);
              ResourceId rerecord = GetResID(cmd);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("ExecuteCommands re-recorded replay of %llu, using %llu (%u -> %u <= %u)",
                       cmdid, rerecord, eid, end, m_LastEventID);
#endif
              rerecordedCmds.push_back(Unwrap(cmd));

              GetResourceManager()->MergeBarriers(
                  m_BakedCmdBufferInfo[GetResID(commandBuffer)].imgbarriers,
                  m_BakedCmdBufferInfo[rerecord].imgbarriers);
            }
            else
            {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("not executing %llu", cmdid);
#endif
            }

            // 1 extra to account for the virtual end command buffer label (begin is accounted for
            // above)
            eid += 1 + m_BakedCmdBufferInfo[cmdid].eventCount;
          }

          if(!rerecordedCmds.empty())
            ObjDisp(commandBuffer)
                ->CmdExecuteCommands(Unwrap(commandBuffer), (uint32_t)rerecordedCmds.size(),
                                     rerecordedCmds.data());
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount,
                                         const VkCommandBuffer *pCommandBuffers)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdExecuteCommands(Unwrap(commandBuffer), commandBufferCount,
                                               UnwrapArray(pCommandBuffers, commandBufferCount)));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdExecuteCommands);
    Serialise_vkCmdExecuteCommands(ser, commandBuffer, commandBufferCount, pCommandBuffers);

    record->AddChunk(scope.Get());

    for(uint32_t i = 0; i < commandBufferCount; i++)
    {
      VkResourceRecord *execRecord = GetRecord(pCommandBuffers[i]);
      if(execRecord->bakedCommands)
      {
        record->cmdInfo->dirtied.insert(execRecord->bakedCommands->cmdInfo->dirtied.begin(),
                                        execRecord->bakedCommands->cmdInfo->dirtied.end());
        record->cmdInfo->boundDescSets.insert(
            execRecord->bakedCommands->cmdInfo->boundDescSets.begin(),
            execRecord->bakedCommands->cmdInfo->boundDescSets.end());
        record->cmdInfo->subcmds.push_back(execRecord);

        GetResourceManager()->MergeBarriers(record->cmdInfo->imgbarriers,
                                            execRecord->bakedCommands->cmdInfo->imgbarriers);
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDebugMarkerBeginEXT(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       const VkDebugMarkerMarkerInfoEXT *pMarker)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Marker, *pMarker).Named("pMarker"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].markerCount++;

        if(ObjDisp(commandBuffer)->CmdDebugMarkerBeginEXT)
          ObjDisp(commandBuffer)->CmdDebugMarkerBeginEXT(Unwrap(commandBuffer), &Marker);
      }
    }
    else
    {
      if(ObjDisp(commandBuffer)->CmdDebugMarkerBeginEXT)
        ObjDisp(commandBuffer)->CmdDebugMarkerBeginEXT(Unwrap(commandBuffer), &Marker);

      DrawcallDescription draw;
      draw.name = Marker.pMarkerName;
      draw.flags |= DrawFlags::PushMarker;

      draw.markerColor[0] = RDCCLAMP(Marker.color[0], 0.0f, 1.0f);
      draw.markerColor[1] = RDCCLAMP(Marker.color[1], 0.0f, 1.0f);
      draw.markerColor[2] = RDCCLAMP(Marker.color[2], 0.0f, 1.0f);
      draw.markerColor[3] = RDCCLAMP(Marker.color[3], 0.0f, 1.0f);

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDebugMarkerBeginEXT(VkCommandBuffer commandBuffer,
                                             const VkDebugMarkerMarkerInfoEXT *pMarker)
{
  if(ObjDisp(commandBuffer)->CmdDebugMarkerBeginEXT)
  {
    SERIALISE_TIME_CALL(
        ObjDisp(commandBuffer)->CmdDebugMarkerBeginEXT(Unwrap(commandBuffer), pMarker));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDebugMarkerBeginEXT);
    Serialise_vkCmdDebugMarkerBeginEXT(ser, commandBuffer, pMarker);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDebugMarkerEndEXT(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        int &markerCount = m_BakedCmdBufferInfo[m_LastCmdBufferID].markerCount;
        markerCount = RDCMAX(0, markerCount - 1);

        if(ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT)
          ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT(Unwrap(commandBuffer));
      }
    }
    else
    {
      if(ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT)
        ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT(Unwrap(commandBuffer));

      if(HasNonMarkerEvents(m_LastCmdBufferID))
      {
        DrawcallDescription draw;
        draw.name = "API Calls";
        draw.flags = DrawFlags::APICalls;

        AddDrawcall(draw, true);
      }

      // dummy draw that is consumed when this command buffer
      // is being in-lined into the call stream
      DrawcallDescription draw;
      draw.name = "Pop()";
      draw.flags = DrawFlags::PopMarker;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDebugMarkerEndEXT(VkCommandBuffer commandBuffer)
{
  if(ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT)
  {
    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT(Unwrap(commandBuffer)));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDebugMarkerEndEXT);
    Serialise_vkCmdDebugMarkerEndEXT(ser, commandBuffer);

    record->AddChunk(scope.Get());
  }
}

void WrappedVulkan::HandleVRFrameMarkers(const char *marker, VkCommandBuffer commandBuffer)
{
  if(strstr(marker, "vr-marker,frame_end,type,application") != NULL)
  {
    VkResourceRecord *record = GetRecord(commandBuffer);
    record->bakedCommands->cmdInfo->present = true;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDebugMarkerInsertEXT(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        const VkDebugMarkerMarkerInfoEXT *pMarker)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Marker, *pMarker).Named("pMarker"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        if(ObjDisp(commandBuffer)->CmdDebugMarkerInsertEXT)
          ObjDisp(commandBuffer)->CmdDebugMarkerInsertEXT(Unwrap(commandBuffer), &Marker);
      }
    }
    else
    {
      if(ObjDisp(commandBuffer)->CmdDebugMarkerInsertEXT)
        ObjDisp(commandBuffer)->CmdDebugMarkerInsertEXT(Unwrap(commandBuffer), &Marker);

      DrawcallDescription draw;
      draw.name = Marker.pMarkerName;
      draw.flags |= DrawFlags::SetMarker;

      draw.markerColor[0] = RDCCLAMP(Marker.color[0], 0.0f, 1.0f);
      draw.markerColor[1] = RDCCLAMP(Marker.color[1], 0.0f, 1.0f);
      draw.markerColor[2] = RDCCLAMP(Marker.color[2], 0.0f, 1.0f);
      draw.markerColor[3] = RDCCLAMP(Marker.color[3], 0.0f, 1.0f);

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDebugMarkerInsertEXT(VkCommandBuffer commandBuffer,
                                              const VkDebugMarkerMarkerInfoEXT *pMarker)
{
  if(ObjDisp(commandBuffer)->CmdDebugMarkerInsertEXT)
  {
    SERIALISE_TIME_CALL(
        ObjDisp(commandBuffer)->CmdDebugMarkerInsertEXT(Unwrap(commandBuffer), pMarker));
  }

  HandleVRFrameMarkers(pMarker->pMarkerName, commandBuffer);
  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDebugMarkerInsertEXT);
    Serialise_vkCmdDebugMarkerInsertEXT(ser, commandBuffer, pMarker);

    record->AddChunk(scope.Get());
  }
}

void WrappedVulkan::ApplyPushDescriptorWrites(VkPipelineBindPoint pipelineBindPoint,
                                              VkPipelineLayout layout, uint32_t set,
                                              uint32_t descriptorWriteCount,
                                              const VkWriteDescriptorSet *pDescriptorWrites)
{
  const VulkanCreationInfo::PipelineLayout &pipeLayoutInfo =
      m_CreationInfo.m_PipelineLayout[GetResID(layout)];

  ResourceId setId = m_BakedCmdBufferInfo[m_LastCmdBufferID].pushDescriptorID[pipelineBindPoint][set];

  const std::vector<ResourceId> &descSetLayouts = pipeLayoutInfo.descSetLayouts;

  const DescSetLayout &desclayout = m_CreationInfo.m_DescSetLayout[descSetLayouts[set]];

  std::vector<DescriptorSetBindingElement *> &bindings = m_DescriptorSetState[setId].currentBindings;
  ResourceId prevLayout = m_DescriptorSetState[setId].layout;

  if(prevLayout == ResourceId())
  {
    desclayout.CreateBindingsArray(bindings);
  }
  else if(prevLayout != descSetLayouts[set])
  {
    desclayout.UpdateBindingsArray(m_CreationInfo.m_DescSetLayout[prevLayout], bindings);
  }

  m_DescriptorSetState[setId].layout = descSetLayouts[set];

  // update our local tracking
  for(uint32_t i = 0; i < descriptorWriteCount; i++)
  {
    const VkWriteDescriptorSet &writeDesc = pDescriptorWrites[i];

    RDCASSERT(writeDesc.dstBinding < bindings.size());

    DescriptorSetBindingElement **bind = &bindings[writeDesc.dstBinding];
    const DescSetLayout::Binding *layoutBinding = &desclayout.bindings[writeDesc.dstBinding];
    uint32_t curIdx = writeDesc.dstArrayElement;

    if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
       writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    {
      for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
      {
        // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
        // explanation
        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          bind++;
          curIdx = 0;
        }

        (*bind)[curIdx].texelBufferView = writeDesc.pTexelBufferView[d];
      }
    }
    else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
            writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    {
      for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
      {
        // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
        // explanation
        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          bind++;
          curIdx = 0;
        }

        (*bind)[curIdx].imageInfo = writeDesc.pImageInfo[d];
      }
    }
    else
    {
      for(uint32_t d = 0; d < writeDesc.descriptorCount; d++, curIdx++)
      {
        // allow consecutive descriptor bind updates. See vkUpdateDescriptorSets for more
        // explanation
        if(curIdx >= layoutBinding->descriptorCount)
        {
          layoutBinding++;
          bind++;
          curIdx = 0;
        }

        (*bind)[curIdx].bufferInfo = writeDesc.pBufferInfo[d];
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdPushDescriptorSetKHR(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        VkPipelineBindPoint pipelineBindPoint,
                                                        VkPipelineLayout layout, uint32_t set,
                                                        uint32_t descriptorWriteCount,
                                                        const VkWriteDescriptorSet *pDescriptorWrites)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineBindPoint);
  SERIALISE_ELEMENT(layout);
  SERIALISE_ELEMENT(set);
  SERIALISE_ELEMENT(descriptorWriteCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorWrites, descriptorWriteCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    ResourceId setId =
        m_BakedCmdBufferInfo[m_LastCmdBufferID].pushDescriptorID[pipelineBindPoint][set];

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          std::vector<VulkanStatePipeline::DescriptorAndOffsets> &descsets =
              (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
                  ? m_RenderState.graphics.descSets
                  : m_RenderState.compute.descSets;

          // expand as necessary
          if(descsets.size() < set + 1)
            descsets.resize(set + 1);

          descsets[set].descSet = setId;
        }

        // actual replay of the command will happen below
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }
    else
    {
      // track while reading, as we need to track resource usage
      std::vector<BakedCmdBufferInfo::CmdBufferState::DescriptorAndOffsets> &descsets =
          (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
              ? m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphicsDescSets
              : m_BakedCmdBufferInfo[m_LastCmdBufferID].state.computeDescSets;

      // expand as necessary
      if(descsets.size() < set + 1)
        descsets.resize(set + 1);

      // we use a 'special' ID for the push descriptor at this index, since there's no actual
      // allocated object corresponding to it.
      descsets[set].descSet = setId;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      // since we version push descriptors per-command buffer, we can safely update them always
      // without worrying about overlap. We just need to check that we're in the record range so
      // that we don't pull in descriptor updates after the point in the command buffer we're
      // recording to
      ApplyPushDescriptorWrites(pipelineBindPoint, layout, set, descriptorWriteCount,
                                pDescriptorWrites);

      // now unwrap everything in-place to save on temp allocs.
      VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)pDescriptorWrites;

      for(uint32_t i = 0; i < descriptorWriteCount; i++)
      {
        for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
        {
          VkBufferView *pTexelBufferView = (VkBufferView *)writes[i].pTexelBufferView;
          VkDescriptorBufferInfo *pBufferInfo = (VkDescriptorBufferInfo *)writes[i].pBufferInfo;
          VkDescriptorImageInfo *pImageInfo = (VkDescriptorImageInfo *)writes[i].pImageInfo;

          if(pTexelBufferView)
            pTexelBufferView[d] = Unwrap(pTexelBufferView[d]);

          if(pBufferInfo)
            pBufferInfo[d].buffer = Unwrap(pBufferInfo[d].buffer);

          if(pImageInfo)
          {
            pImageInfo[d].imageView = Unwrap(pImageInfo[d].imageView);
            pImageInfo[d].sampler = Unwrap(pImageInfo[d].sampler);
          }
        }
      }

      ObjDisp(commandBuffer)
          ->CmdPushDescriptorSetKHR(Unwrap(commandBuffer), pipelineBindPoint, Unwrap(layout), set,
                                    descriptorWriteCount, pDescriptorWrites);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer,
                                              VkPipelineBindPoint pipelineBindPoint,
                                              VkPipelineLayout layout, uint32_t set,
                                              uint32_t descriptorWriteCount,
                                              const VkWriteDescriptorSet *pDescriptorWrites)
{
  SCOPED_DBG_SINK();

  {
    // need to count up number of descriptor infos, to be able to alloc enough space
    uint32_t numInfos = 0;
    for(uint32_t i = 0; i < descriptorWriteCount; i++)
      numInfos += pDescriptorWrites[i].descriptorCount;

    byte *memory = GetTempMemory(sizeof(VkDescriptorBufferInfo) * numInfos +
                                 sizeof(VkWriteDescriptorSet) * descriptorWriteCount);

    RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                      "Descriptor structs sizes are unexpected, ensure largest size is used");

    VkWriteDescriptorSet *unwrappedWrites = (VkWriteDescriptorSet *)memory;
    VkDescriptorBufferInfo *nextDescriptors =
        (VkDescriptorBufferInfo *)(unwrappedWrites + descriptorWriteCount);

    for(uint32_t i = 0; i < descriptorWriteCount; i++)
    {
      unwrappedWrites[i] = pDescriptorWrites[i];
      unwrappedWrites[i].dstSet = Unwrap(unwrappedWrites[i].dstSet);

      VkDescriptorBufferInfo *bufInfos = nextDescriptors;
      VkDescriptorImageInfo *imInfos = (VkDescriptorImageInfo *)bufInfos;
      VkBufferView *bufViews = (VkBufferView *)bufInfos;
      nextDescriptors += pDescriptorWrites[i].descriptorCount;

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Structure sizes mean not enough space is allocated for write data");
      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkBufferView),
                        "Structure sizes mean not enough space is allocated for write data");

      // unwrap and assign the appropriate array
      if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        unwrappedWrites[i].pTexelBufferView = (VkBufferView *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
          bufViews[j] = Unwrap(pDescriptorWrites[i].pTexelBufferView[j]);
      }
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        bool hasSampler =
            (pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        bool hasImage =
            (pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
             pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

        unwrappedWrites[i].pImageInfo = (VkDescriptorImageInfo *)bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          if(hasImage)
            imInfos[j].imageView = Unwrap(pDescriptorWrites[i].pImageInfo[j].imageView);
          if(hasSampler)
            imInfos[j].sampler = Unwrap(pDescriptorWrites[i].pImageInfo[j].sampler);
          imInfos[j].imageLayout = pDescriptorWrites[i].pImageInfo[j].imageLayout;
        }
      }
      else
      {
        unwrappedWrites[i].pBufferInfo = bufInfos;
        for(uint32_t j = 0; j < pDescriptorWrites[i].descriptorCount; j++)
        {
          bufInfos[j].buffer = Unwrap(pDescriptorWrites[i].pBufferInfo[j].buffer);
          bufInfos[j].offset = pDescriptorWrites[i].pBufferInfo[j].offset;
          bufInfos[j].range = pDescriptorWrites[i].pBufferInfo[j].range;
        }
      }
    }

    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                            ->CmdPushDescriptorSetKHR(Unwrap(commandBuffer), pipelineBindPoint,
                                                      Unwrap(layout), set, descriptorWriteCount,
                                                      unwrappedWrites));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdPushDescriptorSetKHR);
    Serialise_vkCmdPushDescriptorSetKHR(ser, commandBuffer, pipelineBindPoint, layout, set,
                                        descriptorWriteCount, pDescriptorWrites);

    record->AddChunk(scope.Get());
    for(uint32_t i = 0; i < descriptorWriteCount; i++)
    {
      const VkWriteDescriptorSet &write = pDescriptorWrites[i];

      FrameRefType ref = GetRefType(write.descriptorType);

      for(uint32_t d = 0; d < write.descriptorCount; d++)
      {
        if(write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          VkResourceRecord *bufView = GetRecord(write.pTexelBufferView[d]);
          record->MarkBufferViewFrameReferenced(bufView, ref);
        }
        else if(write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          // ignore descriptors not part of the write, by NULL'ing out those members
          // as they might not even point to a valid object
          if(write.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER)
          {
            VkResourceRecord *view = GetRecord(write.pImageInfo[d].imageView);
            record->MarkImageViewFrameReferenced(view, ImageRange(), ref);
          }

          if(write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
             write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            record->MarkResourceFrameReferenced(GetResID(write.pImageInfo[d].sampler),
                                                eFrameRef_Read);
        }
        else
        {
          record->MarkBufferFrameReferenced(GetRecord(write.pBufferInfo[d].buffer),
                                            write.pBufferInfo[d].offset, write.pBufferInfo[d].range,
                                            ref);
        }
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdPushDescriptorSetWithTemplateKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set,
    const void *pData)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(descriptorUpdateTemplate);
  SERIALISE_ELEMENT(layout);
  SERIALISE_ELEMENT(set);

  // we can't serialise pData as-is, since we need to decode to ResourceId for references, etc. The
  // sensible way to do this is to decode the data into a series of writes and serialise that.
  DescUpdateTemplateApplication apply;

  if(IsCaptureMode(m_State))
  {
    // decode while capturing.
    GetRecord(descriptorUpdateTemplate)->descTemplateInfo->Apply(pData, apply);
  }

  SERIALISE_ELEMENT(apply.writes).Named("Decoded Writes"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    VkPipelineBindPoint bindPoint =
        m_CreationInfo.m_DescUpdateTemplate[GetResID(descriptorUpdateTemplate)].bindPoint;

    ResourceId setId = m_BakedCmdBufferInfo[m_LastCmdBufferID].pushDescriptorID[bindPoint][set];

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          std::vector<VulkanStatePipeline::DescriptorAndOffsets> &descsets =
              (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) ? m_RenderState.graphics.descSets
                                                             : m_RenderState.compute.descSets;

          // expand as necessary
          if(descsets.size() < set + 1)
            descsets.resize(set + 1);

          descsets[set].descSet = setId;
        }

        // actual replay of the command will happen below
      }
      else
      {
        commandBuffer = VK_NULL_HANDLE;
      }
    }
    else
    {
      // track while reading, as we need to track resource usage
      std::vector<BakedCmdBufferInfo::CmdBufferState::DescriptorAndOffsets> &descsets =
          (m_CreationInfo.m_DescUpdateTemplate[GetResID(descriptorUpdateTemplate)].bindPoint ==
           VK_PIPELINE_BIND_POINT_GRAPHICS)
              ? m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphicsDescSets
              : m_BakedCmdBufferInfo[m_LastCmdBufferID].state.computeDescSets;

      // expand as necessary
      if(descsets.size() < set + 1)
        descsets.resize(set + 1);

      // we use a 'special' ID for the push descriptor at this index, since there's no actual
      // allocated object corresponding to it.
      descsets[set].descSet = setId;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      // since we version push descriptors per-command buffer, we can safely update them always
      // without worrying about overlap. We just need to check that we're in the record range so
      // that we don't pull in descriptor updates after the point in the command buffer we're
      // recording to
      ApplyPushDescriptorWrites(bindPoint, layout, set, (uint32_t)apply.writes.size(),
                                apply.writes.data());

      // now unwrap everything in-place to save on temp allocs.
      VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)apply.writes.data();

      for(size_t i = 0; i < apply.writes.size(); i++)
      {
        for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
        {
          VkBufferView *pTexelBufferView = (VkBufferView *)writes[i].pTexelBufferView;
          VkDescriptorBufferInfo *pBufferInfo = (VkDescriptorBufferInfo *)writes[i].pBufferInfo;
          VkDescriptorImageInfo *pImageInfo = (VkDescriptorImageInfo *)writes[i].pImageInfo;

          if(pTexelBufferView)
            pTexelBufferView[d] = Unwrap(pTexelBufferView[d]);

          if(pBufferInfo)
            pBufferInfo[d].buffer = Unwrap(pBufferInfo[d].buffer);

          if(pImageInfo)
          {
            pImageInfo[d].imageView = Unwrap(pImageInfo[d].imageView);
            pImageInfo[d].sampler = Unwrap(pImageInfo[d].sampler);
          }
        }
      }

      ObjDisp(commandBuffer)
          ->CmdPushDescriptorSetKHR(Unwrap(commandBuffer), bindPoint, Unwrap(layout), set,
                                    (uint32_t)apply.writes.size(), apply.writes.data());
    }
  }

  return true;
}

void WrappedVulkan::vkCmdPushDescriptorSetWithTemplateKHR(
    VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate,
    VkPipelineLayout layout, uint32_t set, const void *pData)
{
  SCOPED_DBG_SINK();

  // since it's relatively expensive to walk the memory, we gather frame references at the same time
  // as unwrapping
  std::vector<rdcpair<ResourceId, FrameRefType> > frameRefs;
  std::vector<rdcpair<VkImageView, FrameRefType> > imgViewFrameRefs;
  std::vector<rdcpair<VkBufferView, FrameRefType> > bufViewFrameRefs;
  std::vector<rdcpair<VkDescriptorBufferInfo, FrameRefType> > bufFrameRefs;

  {
    DescUpdateTemplate *tempInfo = GetRecord(descriptorUpdateTemplate)->descTemplateInfo;

    // allocate the whole blob of memory
    byte *memory = GetTempMemory(tempInfo->dataByteSize);

    // iterate the entries, copy the descriptor data and unwrap
    for(const VkDescriptorUpdateTemplateEntry &entry : tempInfo->updates)
    {
      byte *dst = memory + entry.offset;
      const byte *src = (const byte *)pData + entry.offset;

      FrameRefType ref = GetRefType(entry.descriptorType);

      if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkBufferView));

          VkBufferView *bufView = (VkBufferView *)dst;

          bufViewFrameRefs.push_back(make_rdcpair(*bufView, ref));

          *bufView = Unwrap(*bufView);
        }
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
              entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
      {
        bool hasSampler = (entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                           entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        bool hasImage = (entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                         entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);

        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorImageInfo));

          VkDescriptorImageInfo *info = (VkDescriptorImageInfo *)dst;

          if(hasSampler)
          {
            frameRefs.push_back(make_rdcpair(GetResID(info->sampler), eFrameRef_Read));
            info->sampler = Unwrap(info->sampler);
          }
          if(hasImage)
          {
            frameRefs.push_back(make_rdcpair(GetResID(info->imageView), eFrameRef_Read));
            if(GetRecord(info->imageView)->baseResource != ResourceId())
              frameRefs.push_back(make_rdcpair(GetRecord(info->imageView)->baseResource, ref));
            imgViewFrameRefs.push_back(make_rdcpair(info->imageView, ref));
            info->imageView = Unwrap(info->imageView);
          }
        }
      }
      else
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorBufferInfo));

          VkDescriptorBufferInfo *info = (VkDescriptorBufferInfo *)dst;

          bufFrameRefs.push_back(make_rdcpair(*info, ref));

          info->buffer = Unwrap(info->buffer);
        }
      }
    }

    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                            ->CmdPushDescriptorSetWithTemplateKHR(Unwrap(commandBuffer),
                                                                  Unwrap(descriptorUpdateTemplate),
                                                                  Unwrap(layout), set, memory));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdPushDescriptorSetWithTemplateKHR);
    Serialise_vkCmdPushDescriptorSetWithTemplateKHR(ser, commandBuffer, descriptorUpdateTemplate,
                                                    layout, set, pData);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(descriptorUpdateTemplate), eFrameRef_Read);
    for(size_t i = 0; i < frameRefs.size(); i++)
      record->MarkResourceFrameReferenced(frameRefs[i].first, frameRefs[i].second);
    for(size_t i = 0; i < imgViewFrameRefs.size(); i++)
    {
      VkResourceRecord *view = GetRecord(imgViewFrameRefs[i].first);
      record->MarkImageViewFrameReferenced(view, ImageRange(), imgViewFrameRefs[i].second);
    }
    for(size_t i = 0; i < bufViewFrameRefs.size(); i++)
      record->MarkBufferViewFrameReferenced(GetRecord(bufViewFrameRefs[i].first),
                                            bufViewFrameRefs[i].second);
    for(size_t i = 0; i < bufFrameRefs.size(); i++)
      record->MarkBufferFrameReferenced(GetRecord(bufFrameRefs[i].first.buffer),
                                        bufFrameRefs[i].first.offset, bufFrameRefs[i].first.range,
                                        bufFrameRefs[i].second);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdWriteBufferMarkerAMD(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        VkPipelineStageFlagBits pipelineStage,
                                                        VkBuffer dstBuffer, VkDeviceSize dstOffset,
                                                        uint32_t marker)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineStage);
  SERIALISE_ELEMENT(dstBuffer);
  SERIALISE_ELEMENT(dstOffset);
  SERIALISE_ELEMENT(marker);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)
          ->CmdWriteBufferMarkerAMD(Unwrap(commandBuffer), pipelineStage, Unwrap(dstBuffer),
                                    dstOffset, marker);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdWriteBufferMarkerAMD(VkCommandBuffer commandBuffer,
                                              VkPipelineStageFlagBits pipelineStage,
                                              VkBuffer dstBuffer, VkDeviceSize dstOffset,
                                              uint32_t marker)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdWriteBufferMarkerAMD(Unwrap(commandBuffer), pipelineStage,
                                                    Unwrap(dstBuffer), dstOffset, marker));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdWriteBufferMarkerAMD);
    Serialise_vkCmdWriteBufferMarkerAMD(ser, commandBuffer, pipelineStage, dstBuffer, dstOffset,
                                        marker);

    record->AddChunk(scope.Get());

    record->MarkBufferFrameReferenced(GetRecord(dstBuffer), dstOffset, 4, eFrameRef_PartialWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginDebugUtilsLabelEXT(SerialiserType &ser,
                                                           VkCommandBuffer commandBuffer,
                                                           const VkDebugUtilsLabelEXT *pLabelInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].markerCount++;

        if(ObjDisp(commandBuffer)->CmdBeginDebugUtilsLabelEXT)
          ObjDisp(commandBuffer)->CmdBeginDebugUtilsLabelEXT(Unwrap(commandBuffer), &Label);
      }
    }
    else
    {
      if(ObjDisp(commandBuffer)->CmdBeginDebugUtilsLabelEXT)
        ObjDisp(commandBuffer)->CmdBeginDebugUtilsLabelEXT(Unwrap(commandBuffer), &Label);

      DrawcallDescription draw;
      draw.name = Label.pLabelName;
      draw.flags |= DrawFlags::PushMarker;

      draw.markerColor[0] = RDCCLAMP(Label.color[0], 0.0f, 1.0f);
      draw.markerColor[1] = RDCCLAMP(Label.color[1], 0.0f, 1.0f);
      draw.markerColor[2] = RDCCLAMP(Label.color[2], 0.0f, 1.0f);
      draw.markerColor[3] = RDCCLAMP(Label.color[3], 0.0f, 1.0f);

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer,
                                                 const VkDebugUtilsLabelEXT *pLabelInfo)
{
  if(ObjDisp(commandBuffer)->CmdBeginDebugUtilsLabelEXT)
  {
    SERIALISE_TIME_CALL(
        ObjDisp(commandBuffer)->CmdBeginDebugUtilsLabelEXT(Unwrap(commandBuffer), pLabelInfo));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginDebugUtilsLabelEXT);
    Serialise_vkCmdBeginDebugUtilsLabelEXT(ser, commandBuffer, pLabelInfo);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndDebugUtilsLabelEXT(SerialiserType &ser,
                                                         VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        int &markerCount = m_BakedCmdBufferInfo[m_LastCmdBufferID].markerCount;
        markerCount = RDCMAX(0, markerCount - 1);

        if(ObjDisp(commandBuffer)->CmdEndDebugUtilsLabelEXT)
          ObjDisp(commandBuffer)->CmdEndDebugUtilsLabelEXT(Unwrap(commandBuffer));
      }
    }
    else
    {
      if(ObjDisp(commandBuffer)->CmdEndDebugUtilsLabelEXT)
        ObjDisp(commandBuffer)->CmdEndDebugUtilsLabelEXT(Unwrap(commandBuffer));

      if(!m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.empty())
      {
        DrawcallDescription draw;
        draw.name = "API Calls";
        draw.flags = DrawFlags::APICalls;

        AddDrawcall(draw, true);
      }

      // dummy draw that is consumed when this command buffer
      // is being in-lined into the call stream
      DrawcallDescription draw;
      draw.name = "Pop()";
      draw.flags = DrawFlags::PopMarker;

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
  if(ObjDisp(commandBuffer)->CmdEndDebugUtilsLabelEXT)
  {
    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdEndDebugUtilsLabelEXT(Unwrap(commandBuffer)));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndDebugUtilsLabelEXT);
    Serialise_vkCmdEndDebugUtilsLabelEXT(ser, commandBuffer);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdInsertDebugUtilsLabelEXT(SerialiserType &ser,
                                                            VkCommandBuffer commandBuffer,
                                                            const VkDebugUtilsLabelEXT *pLabelInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        if(ObjDisp(commandBuffer)->CmdInsertDebugUtilsLabelEXT)
          ObjDisp(commandBuffer)->CmdInsertDebugUtilsLabelEXT(Unwrap(commandBuffer), &Label);
      }
    }
    else
    {
      if(ObjDisp(commandBuffer)->CmdInsertDebugUtilsLabelEXT)
        ObjDisp(commandBuffer)->CmdInsertDebugUtilsLabelEXT(Unwrap(commandBuffer), &Label);

      DrawcallDescription draw;
      draw.name = Label.pLabelName;
      draw.flags |= DrawFlags::SetMarker;

      draw.markerColor[0] = RDCCLAMP(Label.color[0], 0.0f, 1.0f);
      draw.markerColor[1] = RDCCLAMP(Label.color[1], 0.0f, 1.0f);
      draw.markerColor[2] = RDCCLAMP(Label.color[2], 0.0f, 1.0f);
      draw.markerColor[3] = RDCCLAMP(Label.color[3], 0.0f, 1.0f);

      AddEvent();
      AddDrawcall(draw, false);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer,
                                                  const VkDebugUtilsLabelEXT *pLabelInfo)
{
  if(ObjDisp(commandBuffer)->CmdInsertDebugUtilsLabelEXT)
  {
    SERIALISE_TIME_CALL(
        ObjDisp(commandBuffer)->CmdInsertDebugUtilsLabelEXT(Unwrap(commandBuffer), pLabelInfo));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdInsertDebugUtilsLabelEXT);
    Serialise_vkCmdInsertDebugUtilsLabelEXT(ser, commandBuffer, pLabelInfo);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDeviceMask(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                 uint32_t deviceMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(deviceMask);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      ObjDisp(commandBuffer)->CmdSetDeviceMask(Unwrap(commandBuffer), deviceMask);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdSetDeviceMask(Unwrap(commandBuffer), deviceMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetDeviceMask);
    Serialise_vkCmdSetDeviceMask(ser, commandBuffer, deviceMask);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindTransformFeedbackBuffersEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount,
    const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBinding);
  SERIALISE_ELEMENT(bindingCount);
  SERIALISE_ELEMENT_ARRAY(pBuffers, bindingCount);
  SERIALISE_ELEMENT_ARRAY(pOffsets, bindingCount);
  SERIALISE_ELEMENT_ARRAY(pSizes, bindingCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)
            ->CmdBindTransformFeedbackBuffersEXT(Unwrap(commandBuffer), firstBinding, bindingCount,
                                                 UnwrapArray(pBuffers, bindingCount), pOffsets,
                                                 pSizes);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          if(m_RenderState.xfbbuffers.size() < firstBinding + bindingCount)
            m_RenderState.xfbbuffers.resize(firstBinding + bindingCount);

          for(uint32_t i = 0; i < bindingCount; i++)
          {
            m_RenderState.xfbbuffers[firstBinding + i].buf = GetResID(pBuffers[i]);
            m_RenderState.xfbbuffers[firstBinding + i].offs = pOffsets[i];
            m_RenderState.xfbbuffers[firstBinding + i].size = pSizes[i];
          }
        }
      }
    }
    else
    {
      // track while reading, as we need to track resource usage
      if(m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbbuffers.size() < firstBinding + bindingCount)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbbuffers.resize(firstBinding + bindingCount);

      for(uint32_t i = 0; i < bindingCount; i++)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbbuffers[firstBinding + i] =
            GetResID(pBuffers[i]);

      ObjDisp(commandBuffer)
          ->CmdBindTransformFeedbackBuffersEXT(Unwrap(commandBuffer), firstBinding, bindingCount,
                                               UnwrapArray(pBuffers, bindingCount), pOffsets, pSizes);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer,
                                                         uint32_t firstBinding, uint32_t bindingCount,
                                                         const VkBuffer *pBuffers,
                                                         const VkDeviceSize *pOffsets,
                                                         const VkDeviceSize *pSizes)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBindTransformFeedbackBuffersEXT(
                              Unwrap(commandBuffer), firstBinding, bindingCount,
                              UnwrapArray(pBuffers, bindingCount), pOffsets, pSizes));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindTransformFeedbackBuffersEXT);
    Serialise_vkCmdBindTransformFeedbackBuffersEXT(ser, commandBuffer, firstBinding, bindingCount,
                                                   pBuffers, pOffsets, pSizes);

    record->AddChunk(scope.Get());
    for(uint32_t i = 0; i < bindingCount; i++)
    {
      VkDeviceSize size = VK_WHOLE_SIZE;
      if(pSizes != NULL)
      {
        size = pSizes[i];
      }
      record->MarkBufferFrameReferenced(GetRecord(pBuffers[i]), pOffsets[i], size, eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginTransformFeedbackEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBuffer, uint32_t bufferCount,
    const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBuffer);
  SERIALISE_ELEMENT(bufferCount);
  SERIALISE_ELEMENT_ARRAY(pCounterBuffers, bufferCount);
  SERIALISE_ELEMENT_ARRAY(pCounterBufferOffsets, bufferCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.firstxfbcounter = firstBuffer;
          m_RenderState.xfbcounters.resize(bufferCount);

          for(uint32_t i = 0; i < bufferCount; i++)
          {
            m_RenderState.xfbcounters[i].buf =
                pCounterBuffers ? GetResID(pCounterBuffers[i]) : ResourceId();
            m_RenderState.xfbcounters[i].offs = pCounterBufferOffsets ? pCounterBufferOffsets[i] : 0;
          }
        }

        ObjDisp(commandBuffer)
            ->CmdBeginTransformFeedbackEXT(Unwrap(commandBuffer), firstBuffer, bufferCount,
                                           UnwrapArray(pCounterBuffers, bufferCount),
                                           pCounterBufferOffsets);
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdBeginTransformFeedbackEXT(Unwrap(commandBuffer), firstBuffer, bufferCount,
                                         UnwrapArray(pCounterBuffers, bufferCount),
                                         pCounterBufferOffsets);

      // track while reading, for fetching the right set of outputs in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbfirst = firstBuffer;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbcount = bufferCount;
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer,
                                                   uint32_t firstBuffer, uint32_t bufferCount,
                                                   const VkBuffer *pCounterBuffers,
                                                   const VkDeviceSize *pCounterBufferOffsets)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBeginTransformFeedbackEXT(
                              Unwrap(commandBuffer), firstBuffer, bufferCount,
                              UnwrapArray(pCounterBuffers, bufferCount), pCounterBufferOffsets));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginTransformFeedbackEXT);
    Serialise_vkCmdBeginTransformFeedbackEXT(ser, commandBuffer, firstBuffer, bufferCount,
                                             pCounterBuffers, pCounterBufferOffsets);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndTransformFeedbackEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBuffer, uint32_t bufferCount,
    const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBuffer);
  SERIALISE_ELEMENT(bufferCount);
  SERIALISE_ELEMENT_ARRAY(pCounterBuffers, bufferCount);
  SERIALISE_ELEMENT_ARRAY(pCounterBufferOffsets, bufferCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.firstxfbcounter = 0;
          m_RenderState.xfbcounters.clear();
        }

        ObjDisp(commandBuffer)
            ->CmdEndTransformFeedbackEXT(Unwrap(commandBuffer), firstBuffer, bufferCount,
                                         UnwrapArray(pCounterBuffers, bufferCount),
                                         pCounterBufferOffsets);
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdEndTransformFeedbackEXT(Unwrap(commandBuffer), firstBuffer, bufferCount,
                                       UnwrapArray(pCounterBuffers, bufferCount),
                                       pCounterBufferOffsets);

      // track while reading, for fetching the right set of outputs in AddDrawcall
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbfirst = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbcount = 0;
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer,
                                                 uint32_t firstBuffer, uint32_t bufferCount,
                                                 const VkBuffer *pCounterBuffers,
                                                 const VkDeviceSize *pCounterBufferOffsets)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdEndTransformFeedbackEXT(
                              Unwrap(commandBuffer), firstBuffer, bufferCount,
                              UnwrapArray(pCounterBuffers, bufferCount), pCounterBufferOffsets));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndTransformFeedbackEXT);
    Serialise_vkCmdEndTransformFeedbackEXT(ser, commandBuffer, firstBuffer, bufferCount,
                                           pCounterBuffers, pCounterBufferOffsets);

    record->AddChunk(scope.Get());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginQueryIndexedEXT(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        VkQueryPool queryPool, uint32_t query,
                                                        VkQueryControlFlags flags, uint32_t index)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(query);
  SERIALISE_ELEMENT_TYPED(VkQueryControlFlagBits, flags).TypedAs("VkQueryControlFlags"_lit);
  SERIALISE_ELEMENT(index);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdBeginQueryIndexedEXT(Unwrap(commandBuffer), Unwrap(queryPool), query, flags, index);
  }

  return true;
}

void WrappedVulkan::vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                              uint32_t query, VkQueryControlFlags flags,
                                              uint32_t index)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdBeginQueryIndexedEXT(Unwrap(commandBuffer), Unwrap(queryPool), query, flags, index));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginQueryIndexedEXT);
    Serialise_vkCmdBeginQueryIndexedEXT(ser, commandBuffer, queryPool, query, flags, index);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndQueryIndexedEXT(SerialiserType &ser,
                                                      VkCommandBuffer commandBuffer,
                                                      VkQueryPool queryPool, uint32_t query,
                                                      uint32_t index)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool);
  SERIALISE_ELEMENT(query);
  SERIALISE_ELEMENT(index);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdEndQueryIndexedEXT(Unwrap(commandBuffer), Unwrap(queryPool), query, index);
  }

  return true;
}

void WrappedVulkan::vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                            uint32_t query, uint32_t index)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdEndQueryIndexedEXT(Unwrap(commandBuffer), Unwrap(queryPool), query, index));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndQueryIndexedEXT);
    Serialise_vkCmdEndQueryIndexedEXT(ser, commandBuffer, queryPool, query, index);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginConditionalRenderingEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(BeginInfo, *pConditionalRenderingBegin)
      .Named("pConditionalRenderingBegin"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
        {
          m_RenderState.conditionalRendering.buffer = GetResID(BeginInfo.buffer);
          m_RenderState.conditionalRendering.offset = BeginInfo.offset;
          m_RenderState.conditionalRendering.flags = BeginInfo.flags;
        }

        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.buffer =
            GetResID(BeginInfo.buffer);
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.offset = BeginInfo.offset;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.flags = BeginInfo.flags;

        BeginInfo.buffer = Unwrap(BeginInfo.buffer);
        ObjDisp(commandBuffer)->CmdBeginConditionalRenderingEXT(Unwrap(commandBuffer), &BeginInfo);
      }
    }
    else
    {
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.buffer =
          GetResID(BeginInfo.buffer);
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.offset = BeginInfo.offset;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.flags = BeginInfo.flags;

      BeginInfo.buffer = Unwrap(BeginInfo.buffer);
      ObjDisp(commandBuffer)->CmdBeginConditionalRenderingEXT(Unwrap(commandBuffer), &BeginInfo);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginConditionalRenderingEXT(
    VkCommandBuffer commandBuffer,
    const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
{
  SCOPED_DBG_SINK();

  VkConditionalRenderingBeginInfoEXT unwrappedConditionalRenderingBegin = *pConditionalRenderingBegin;
  unwrappedConditionalRenderingBegin.buffer = Unwrap(unwrappedConditionalRenderingBegin.buffer);

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBeginConditionalRenderingEXT(Unwrap(commandBuffer),
                                                            &unwrappedConditionalRenderingBegin));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginConditionalRenderingEXT);
    Serialise_vkCmdBeginConditionalRenderingEXT(ser, commandBuffer, pConditionalRenderingBegin);

    record->AddChunk(scope.Get());

    VkResourceRecord *buf = GetRecord(pConditionalRenderingBegin->buffer);

    record->MarkResourceFrameReferenced(buf->GetResourceID(), eFrameRef_Read);
    record->MarkResourceFrameReferenced(buf->baseResource, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndConditionalRenderingEXT(SerialiserType &ser,
                                                              VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        if(ShouldUpdateRenderState(m_LastCmdBufferID))
          m_RenderState.conditionalRendering.buffer = ResourceId();

        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.buffer = ResourceId();
        ObjDisp(commandBuffer)->CmdEndConditionalRenderingEXT(Unwrap(commandBuffer));
      }
    }
    else
    {
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.conditionalRendering.buffer = ResourceId();
      ObjDisp(commandBuffer)->CmdEndConditionalRenderingEXT(Unwrap(commandBuffer));
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdEndConditionalRenderingEXT(Unwrap(commandBuffer)));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndConditionalRenderingEXT);
    Serialise_vkCmdEndConditionalRenderingEXT(ser, commandBuffer);

    record->AddChunk(scope.Get());
  }
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateCommandPool, VkDevice device,
                                const VkCommandPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkAllocateCommandBuffers, VkDevice device,
                                const VkCommandBufferAllocateInfo *pAllocateInfo,
                                VkCommandBuffer *pCommandBuffers);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkBeginCommandBuffer, VkCommandBuffer commandBuffer,
                                const VkCommandBufferBeginInfo *pBeginInfo);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkEndCommandBuffer, VkCommandBuffer commandBuffer);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass, VkCommandBuffer commandBuffer,
                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                VkSubpassContents contents);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdNextSubpass, VkCommandBuffer commandBuffer,
                                VkSubpassContents contents);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndRenderPass, VkCommandBuffer commandBuffer);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass2KHR, VkCommandBuffer commandBuffer,
                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                const VkSubpassBeginInfoKHR *pSubpassBeginInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdNextSubpass2KHR, VkCommandBuffer commandBuffer,
                                const VkSubpassBeginInfoKHR *pSubpassBeginInfo,
                                const VkSubpassEndInfoKHR *pSubpassEndInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndRenderPass2KHR, VkCommandBuffer commandBuffer,
                                const VkSubpassEndInfoKHR *pSubpassEndInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindPipeline, VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindDescriptorSets, VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                                uint32_t firstSet, uint32_t setCount,
                                const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
                                const uint32_t *pDynamicOffsets);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindIndexBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindVertexBuffers, VkCommandBuffer commandBuffer,
                                uint32_t firstBinding, uint32_t bindingCount,
                                const VkBuffer *pBuffers, const VkDeviceSize *pOffsets);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdUpdateBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize,
                                const uint32_t *pData);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdFillBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize fillSize,
                                uint32_t data);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdPushConstants, VkCommandBuffer commandBuffer,
                                VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                                uint32_t offset, uint32_t size, const void *pValues);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdPipelineBarrier, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount,
                                const VkMemoryBarrier *pMemoryBarriers,
                                uint32_t bufferMemoryBarrierCount,
                                const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                uint32_t imageMemoryBarrierCount,
                                const VkImageMemoryBarrier *pImageMemoryBarriers);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdWriteTimestamp, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool,
                                uint32_t query);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyQueryPoolResults, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride,
                                VkQueryResultFlags flags);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginQuery, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndQuery, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdResetQueryPool, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdExecuteCommands, VkCommandBuffer commandBuffer,
                                uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDebugMarkerBeginEXT, VkCommandBuffer commandBuffer,
                                const VkDebugMarkerMarkerInfoEXT *pMarker);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDebugMarkerEndEXT, VkCommandBuffer commandBuffer);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDebugMarkerInsertEXT, VkCommandBuffer commandBuffer,
                                const VkDebugMarkerMarkerInfoEXT *pMarker);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdPushDescriptorSetKHR, VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                                uint32_t set, uint32_t descriptorWriteCount,
                                const VkWriteDescriptorSet *pDescriptorWrites);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdPushDescriptorSetWithTemplateKHR,
                                VkCommandBuffer commandBuffer,
                                VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                VkPipelineLayout layout, uint32_t set, const void *pData);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdWriteBufferMarkerAMD, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer,
                                VkDeviceSize dstOffset, uint32_t marker);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginDebugUtilsLabelEXT, VkCommandBuffer commandBuffer,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndDebugUtilsLabelEXT, VkCommandBuffer commandBuffer);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdInsertDebugUtilsLabelEXT, VkCommandBuffer commandBuffer,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetDeviceMask, VkCommandBuffer commandBuffer,
                                uint32_t deviceMask);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindTransformFeedbackBuffersEXT,
                                VkCommandBuffer commandBuffer, uint32_t firstBinding,
                                uint32_t bindingCount, const VkBuffer *pBuffers,
                                const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginTransformFeedbackEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstBuffer, uint32_t bufferCount,
                                const VkBuffer *pCounterBuffers,
                                const VkDeviceSize *pCounterBufferOffsets);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndTransformFeedbackEXT, VkCommandBuffer commandBuffer,
                                uint32_t firstBuffer, uint32_t bufferCount,
                                const VkBuffer *pCounterBuffers,
                                const VkDeviceSize *pCounterBufferOffsets);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginQueryIndexedEXT, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags,
                                uint32_t index);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndQueryIndexedEXT, VkCommandBuffer commandBuffer,
                                VkQueryPool queryPool, uint32_t query, uint32_t index);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginConditionalRenderingEXT,
                                VkCommandBuffer commandBuffer,
                                const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndConditionalRenderingEXT, VkCommandBuffer commandBuffer);
