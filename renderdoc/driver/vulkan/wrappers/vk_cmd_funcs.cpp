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

#include "../vk_common.h"
#include "../vk_core.h"
#include "../vk_debug.h"
#include "core/settings.h"

RDOC_DEBUG_CONFIG(
    bool, Vulkan_Debug_VerboseCommandRecording, false,
    "Add verbose logging around recording and submission of command buffers in vulkan.");

static rdcstr ToHumanStr(const VkAttachmentLoadOp &el)
{
  BEGIN_ENUM_STRINGISE(VkAttachmentLoadOp);
  {
    case VK_ATTACHMENT_LOAD_OP_LOAD: return "Load";
    case VK_ATTACHMENT_LOAD_OP_CLEAR: return "Clear";
    case VK_ATTACHMENT_LOAD_OP_DONT_CARE: return "Don't Care";
    case VK_ATTACHMENT_LOAD_OP_NONE_KHR: return "None";
  }
  END_ENUM_STRINGISE();
}

static rdcstr ToHumanStr(const VkAttachmentStoreOp &el)
{
  BEGIN_ENUM_STRINGISE(VkAttachmentStoreOp);
  {
    case VK_ATTACHMENT_STORE_OP_STORE: return "Store";
    case VK_ATTACHMENT_STORE_OP_DONT_CARE: return "Don't Care";
    case VK_ATTACHMENT_STORE_OP_NONE: return "None";
  }
  END_ENUM_STRINGISE();
}

void WrappedVulkan::AddImplicitResolveResourceUsage(uint32_t subpass)
{
  ResourceId rp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetRenderPass();
  const VulkanCreationInfo::RenderPass &rpinfo = m_CreationInfo.m_RenderPass[rp];

  // Ending a render pass instance performs any multisample operations
  // on the final subpass. ~0U is the end of a RenderPass.
  if(subpass == ~0U)
    subpass = (uint32_t)rpinfo.subpasses.size() - 1;
  else
    subpass = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass;

  const rdcarray<ResourceId> &fbattachments =
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetFramebufferAttachments();
  for(size_t i = 0; i < rpinfo.subpasses[subpass].resolveAttachments.size(); i++)
  {
    uint32_t attIdx = rpinfo.subpasses[subpass].resolveAttachments[i];
    if(attIdx == VK_ATTACHMENT_UNUSED)
      continue;
    ResourceId image = m_CreationInfo.m_ImageView[fbattachments[attIdx]].image;
    m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
        image,
        EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::ResolveDst)));

    attIdx = rpinfo.subpasses[subpass].colorAttachments[i];
    if(attIdx == VK_ATTACHMENT_UNUSED)
      continue;
    image = m_CreationInfo.m_ImageView[fbattachments[attIdx]].image;
    m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
        image,
        EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::ResolveSrc)));
  }

  // also add any discards on the final subpass
  if(subpass + 1 == rpinfo.subpasses.size())
  {
    for(size_t i = 0; i < rpinfo.attachments.size(); i++)
    {
      if(rpinfo.attachments[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
      {
        ResourceId image = m_CreationInfo.m_ImageView[fbattachments[i]].image;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
            image,
            EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::Discard)));
      }
    }
  }
}

rdcarray<VkImageMemoryBarrier> WrappedVulkan::GetImplicitRenderPassBarriers(uint32_t subpass)
{
  ResourceId rp, fb;
  rdcarray<ResourceId> fbattachments;

  if(m_LastCmdBufferID == ResourceId())
  {
    rp = m_RenderState.GetRenderPass();
    fb = m_RenderState.GetFramebuffer();
    fbattachments = m_RenderState.GetFramebufferAttachments();
  }
  else
  {
    const VulkanRenderState &renderstate = GetCmdRenderState();
    rp = renderstate.GetRenderPass();
    fb = renderstate.GetFramebuffer();
    fbattachments = renderstate.GetFramebufferAttachments();
  }

  rdcarray<VkImageMemoryBarrier> ret;

  VulkanCreationInfo::Framebuffer fbinfo = m_CreationInfo.m_Framebuffer[fb];
  VulkanCreationInfo::RenderPass rpinfo = m_CreationInfo.m_RenderPass[rp];

  struct AttachmentRefSeparateStencil : VkAttachmentReference
  {
    VkImageLayout stencilLayout;
  };

  rdcarray<AttachmentRefSeparateStencil> atts;

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
      atts[i].stencilLayout = rpinfo.attachments[i].stencilFinalLayout;
    }
  }
  else
  {
    if(m_LastCmdBufferID == ResourceId())
      subpass = m_RenderState.subpass;
    else
      subpass = m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass;

    // transition the attachments in this subpass
    for(size_t i = 0; i < rpinfo.subpasses[subpass].colorAttachments.size(); i++)
    {
      uint32_t attIdx = rpinfo.subpasses[subpass].colorAttachments[i];

      if(attIdx == VK_ATTACHMENT_UNUSED)
        continue;

      atts.push_back({});
      atts.back().attachment = attIdx;
      atts.back().layout = rpinfo.subpasses[subpass].colorLayouts[i];
    }

    for(size_t i = 0; i < rpinfo.subpasses[subpass].inputAttachments.size(); i++)
    {
      uint32_t attIdx = rpinfo.subpasses[subpass].inputAttachments[i];

      if(attIdx == VK_ATTACHMENT_UNUSED)
        continue;

      atts.push_back({});
      atts.back().attachment = attIdx;
      atts.back().layout = rpinfo.subpasses[subpass].inputLayouts[i];
      atts.back().stencilLayout = rpinfo.subpasses[subpass].inputStencilLayouts[i];
    }

    int32_t ds = rpinfo.subpasses[subpass].depthstencilAttachment;

    if(ds != -1)
    {
      atts.push_back({});
      atts.back().attachment = (uint32_t)rpinfo.subpasses[subpass].depthstencilAttachment;
      atts.back().layout = rpinfo.subpasses[subpass].depthLayout;
      atts.back().stencilLayout = rpinfo.subpasses[subpass].stencilLayout;
    }

    int32_t fd = rpinfo.subpasses[subpass].fragmentDensityAttachment;

    if(fd != -1)
    {
      atts.push_back({});
      atts.back().attachment = (uint32_t)rpinfo.subpasses[subpass].fragmentDensityAttachment;
      atts.back().layout = rpinfo.subpasses[subpass].fragmentDensityLayout;
    }

    int32_t sr = rpinfo.subpasses[subpass].shadingRateAttachment;

    if(sr != -1)
    {
      atts.push_back({});
      atts.back().attachment = (uint32_t)rpinfo.subpasses[subpass].shadingRateAttachment;
      atts.back().layout = rpinfo.subpasses[subpass].shadingRateLayout;
    }
  }

  for(size_t i = 0; i < atts.size(); i++)
  {
    uint32_t idx = atts[i].attachment;

    // we keep two barriers, one for most aspects, one for stencil separately, to allow for separate
    // layout transitions on stencil if that's in use
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    VkImageMemoryBarrier barrierStencil = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierStencil.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierStencil.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    ResourceId view = fbattachments[idx];

    barrierStencil.subresourceRange = barrier.subresourceRange =
        m_CreationInfo.m_ImageView[view].range;

    barrierStencil.image = barrier.image = Unwrap(
        GetResourceManager()->GetCurrentHandle<VkImage>(m_CreationInfo.m_ImageView[view].image));

    // When an imageView of a depth/stencil image is used as a depth/stencil framebuffer attachment,
    // the aspectMask is ignored and both depth and stencil image subresources are used.
    VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[m_CreationInfo.m_ImageView[view].image];

    // if we don't support separate depth stencil, barrier on a combined depth/stencil image will
    // transition both aspects together
    if(!SeparateDepthStencil())
    {
      if(IsDepthAndStencilFormat(c.format))
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else
    {
      // otherwise they will be separate
      if(IsDepthOrStencilFormat(c.format))
      {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrierStencil.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      }
    }

    if(c.type == VK_IMAGE_TYPE_3D)
    {
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;
      barrierStencil.subresourceRange.baseArrayLayer = 0;
      barrierStencil.subresourceRange.layerCount = 1;
    }

    barrier.newLayout = atts[i].layout;
    barrierStencil.newLayout = atts[i].stencilLayout;

    // search back from this subpass to see which layout it was in before. If it's
    // not been used in a previous subpass, then default to initialLayout
    barrier.oldLayout = rpinfo.attachments[idx].initialLayout;
    barrierStencil.oldLayout = rpinfo.attachments[idx].stencilInitialLayout;

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
          barrierStencil.oldLayout = rpinfo.subpasses[s - 1].inputStencilLayouts[a];
          found = true;
          break;
        }
      }

      if(found)
        break;

      if((uint32_t)rpinfo.subpasses[s - 1].depthstencilAttachment == idx)
      {
        barrier.oldLayout = rpinfo.subpasses[s - 1].depthLayout;
        barrierStencil.oldLayout = rpinfo.subpasses[s - 1].stencilLayout;
        break;
      }

      if((uint32_t)rpinfo.subpasses[s - 1].fragmentDensityAttachment == idx)
      {
        barrier.oldLayout = rpinfo.subpasses[s - 1].fragmentDensityLayout;
        break;
      }

      if((uint32_t)rpinfo.subpasses[s - 1].shadingRateAttachment == idx)
      {
        barrier.oldLayout = rpinfo.subpasses[s - 1].shadingRateLayout;
        break;
      }
    }

    SanitiseOldImageLayout(barrier.oldLayout);
    SanitiseNewImageLayout(barrier.newLayout);
    SanitiseOldImageLayout(barrierStencil.oldLayout);
    SanitiseNewImageLayout(barrierStencil.newLayout);

    // if we support separate depth stencil and the format contains stencil, add barriers
    // separately
    if(SeparateDepthStencil())
    {
      if(!IsStencilOnlyFormat(c.format))
        ret.push_back(barrier);

      if(IsStencilFormat(c.format))
        ret.push_back(barrierStencil);
    }
    else
    {
      ret.push_back(barrier);
    }
  }

  // erase any do-nothing barriers
  ret.removeIf(
      [](const VkImageMemoryBarrier &barrier) { return barrier.oldLayout == barrier.newLayout; });

  return ret;
}

rdcstr WrappedVulkan::MakeRenderPassOpString(bool store)
{
  rdcstr opDesc = "";

  const VulkanRenderState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

  if(state.dynamicRendering.active)
  {
    const VulkanRenderState::DynamicRendering &dyn = state.dynamicRendering;

    if(dyn.color.empty() && dyn.depth.imageView == VK_NULL_HANDLE &&
       dyn.stencil.imageView == VK_NULL_HANDLE)
    {
      opDesc = "-";
    }
    else
    {
      bool colsame = true;
      for(size_t i = 1; i < dyn.color.size(); i++)
      {
        if(store)
        {
          if(dyn.color[i].storeOp != dyn.color[0].storeOp)
            colsame = false;
        }
        else
        {
          if(dyn.color[i].loadOp != dyn.color[0].loadOp)
            colsame = false;
        }
      }

      // handle depth only passes
      if(dyn.color.empty())
      {
      }
      else if(!colsame)
      {
        // if we have different storage for the colour, don't display
        // the full details

        opDesc = store ? "Different store ops" : "Different load ops";
      }
      else
      {
        // all colour ops are the same, print it
        opDesc = store ? ToHumanStr(dyn.color[0].storeOp) : ToHumanStr(dyn.color[0].loadOp);
      }

      // do we have depth?
      if(dyn.depth.imageView != VK_NULL_HANDLE || dyn.stencil.imageView != VK_NULL_HANDLE)
      {
        // could be empty if this is a depth-only pass
        if(!opDesc.empty())
          opDesc = "C=" + opDesc + ", ";

        // if there's no stencil, just print depth op
        if(dyn.stencil.imageView == VK_NULL_HANDLE)
        {
          opDesc += "D=" + (store ? ToHumanStr(dyn.depth.storeOp) : ToHumanStr(dyn.depth.loadOp));
        }
        // same for stencil-only
        else if(dyn.depth.imageView == VK_NULL_HANDLE)
        {
          opDesc += "S=" + (store ? ToHumanStr(dyn.stencil.storeOp) : ToHumanStr(dyn.stencil.loadOp));
        }
        else
        {
          if(store)
          {
            // if depth and stencil have same op, print together, otherwise separately
            if(dyn.depth.storeOp == dyn.stencil.storeOp)
              opDesc += "DS=" + ToHumanStr(dyn.depth.storeOp);
            else
              opDesc +=
                  "D=" + ToHumanStr(dyn.depth.storeOp) + ", S=" + ToHumanStr(dyn.stencil.storeOp);
          }
          else
          {
            // if depth and stencil have same op, print together, otherwise separately
            if(dyn.depth.loadOp == dyn.stencil.loadOp)
              opDesc += "DS=" + ToHumanStr(dyn.depth.loadOp);
            else
              opDesc += "D=" + ToHumanStr(dyn.depth.loadOp) + ", S=" + ToHumanStr(dyn.stencil.loadOp);
          }
        }
      }
    }

    // prepend suspend/resume info
    if(!store && (dyn.flags & VK_RENDERING_RESUMING_BIT))
    {
      if(opDesc.empty())
        opDesc = "Resume";
      else
        opDesc = "Resume, " + opDesc;
    }
    else if(store && (dyn.flags & VK_RENDERING_SUSPENDING_BIT))
    {
      if(opDesc.empty())
        opDesc = "Suspend";
      else
        opDesc = "Suspend, " + opDesc;
    }

    return opDesc;
  }

  const VulkanCreationInfo::RenderPass &info = m_CreationInfo.m_RenderPass[state.GetRenderPass()];
  const VulkanCreationInfo::Framebuffer &fbinfo =
      m_CreationInfo.m_Framebuffer[state.GetFramebuffer()];

  const rdcarray<VulkanCreationInfo::RenderPass::Attachment> &atts = info.attachments;

  if(atts.empty())
  {
    opDesc = "-";
  }
  else
  {
    bool colsame = true;

    uint32_t subpass = state.subpass;

    // find which attachment is the depth-stencil one
    int32_t dsAttach = info.subpasses[subpass].depthstencilAttachment;
    bool hasStencil = false;
    bool depthonly = false;

    // if there is a depth-stencil attachment, see if it has a stencil
    // component and if the subpass is depth only (no other attachments)
    if(dsAttach >= 0)
    {
      hasStencil = fbinfo.attachments[dsAttach].hasStencil;
      depthonly = info.subpasses[subpass].colorAttachments.size() == 0;
    }

    const rdcarray<uint32_t> &cols = info.subpasses[subpass].colorAttachments;

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
            opDesc += "D=" + ToHumanStr(atts[dsAttach].storeOp) +
                      ", S=" + ToHumanStr(atts[dsAttach].stencilStoreOp);
        }
        else
        {
          // if depth and stencil have same op, print together, otherwise separately
          if(atts[dsAttach].loadOp == atts[dsAttach].stencilLoadOp)
            opDesc += "DS=" + ToHumanStr(atts[dsAttach].loadOp);
          else
            opDesc += "D=" + ToHumanStr(atts[dsAttach].loadOp) +
                      ", S=" + ToHumanStr(atts[dsAttach].stencilLoadOp);
        }
      }
    }
  }

  return opDesc;
}

void WrappedVulkan::ApplyRPLoadDiscards(VkCommandBuffer commandBuffer, VkRect2D renderArea)
{
  if(m_ReplayOptions.optimisation == ReplayOptimisationLevel::Fastest)
    return;

  ResourceId rpId = GetCmdRenderState().GetRenderPass();

  const VulkanCreationInfo::RenderPass &rpinfo = m_CreationInfo.m_RenderPass[rpId];

  const rdcarray<ResourceId> &attachments = GetCmdRenderState().GetFramebufferAttachments();

  bool feedbackLoop = false;

  // this is a bit of a coarse check and may have false positives, but the cases should be
  // extremely rare where it fires at all. We look for any attachment that is detectably resolved to
  // after it is read, and avoid applying discard patterns anywhere to avoid pollution across
  // partial replays.
  //
  // The reason we only look at resolves is because without significant work those can't be avoided
  // and so they still continue to happen even if we are not intending to replay to the end where
  // the resolve logically happens (as we must always finish a renderpass we started). If that
  // resolve then writes over an attachment which was read earlier in the renderpass we are now
  // polluting results with an effective time-travel via the feedback loop.
  //
  // Subpass 0 reads from attachment 0 and writes to attachment 1
  // Subpass 1 reads from attachment 1 and resolves to attachment 0
  //
  // when selecting a draw in subpass 0 attachment 0 we'll replay up to the draw, then finish the
  // renderpass, but the act of finishing that renderpass will resolve into attachment 0 trashing
  // the contents that should be there. Later replaying the draw alone we'll read the wrong data.
  //
  // note this also doesn't cover all cases, because we only handle detecting input attachment
  // reads, but it would be perfectly valid for subpass 0 to read via a descriptor above.
  //
  // the only 'perfect' solution is extremely invasive and requires either completely splitting
  // apart render passes to manually invoke all resolve actions, which interacts poorly with other
  // things, or else have some kind of future-knowledge at begin renderpass time to know how far
  // into the RP we're going to go, and substitute in a patched RP if needed to avoid resolves.
  // That solution is more maintenance burden & bug surface than handling this case merits.
  rdcarray<rdcpair<bool, bool>> readResolves;
  readResolves.resize(rpinfo.attachments.size());
  for(size_t i = 0; i < rpinfo.subpasses.size(); i++)
  {
    // if the subpass is explicitly marked as a feedback loop, consider that as a read for all
    // attachments since we don't know which will be read from. If there are no resolve attachments
    // this still won't make the whole RP considered a feedback loop for our purposes since there
    // won't be any accidental time travel
    if(rpinfo.subpasses[i].feedbackLoop)
    {
      for(rdcpair<bool, bool> &rw : readResolves)
        rw.first = true;
    }
    else
    {
      for(uint32_t a : rpinfo.subpasses[i].inputAttachments)
        if(a < readResolves.size())
          readResolves[a].first = true;
    }

    for(uint32_t a : rpinfo.subpasses[i].resolveAttachments)
      if(a < readResolves.size())
        readResolves[a].second = true;
  }

  // if any attachment is (provably) read and resolved to, we've got a feedback loop
  for(rdcpair<bool, bool> &rw : readResolves)
    feedbackLoop |= (rw.first && rw.second);

  if(feedbackLoop)
  {
    if(!m_FeedbackRPs.contains(rpId))
    {
      m_FeedbackRPs.push_back(rpId);

      const rdcstr rpName = ToStr(GetResourceManager()->GetOriginalID(rpId));

      AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Render pass %s has resolve feedback loop detected with at "
                            "least one attachment read before it is resolved to.\n"
                            "No discard patterns will be shown to avoid cross-pollution.",
                            rpName.c_str()));
    }

    return;
  }

  for(size_t i = 0; i < attachments.size(); i++)
  {
    const VulkanCreationInfo::ImageView &viewInfo = m_CreationInfo.m_ImageView[attachments[i]];
    VkImage image = GetResourceManager()->GetCurrentHandle<VkImage>(viewInfo.image);
    const VulkanCreationInfo::Image &imInfo = GetDebugManager()->GetImageInfo(GetResID(image));

    VkImageLayout initialLayout = rpinfo.attachments[i].initialLayout;

    bool depthDontCareLoad = (rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    bool stencilDifferentDontCare = false;
    bool stencilDontCareLoad = false;

    if(IsStencilFormat(viewInfo.format))
    {
      stencilDontCareLoad = (rpinfo.attachments[i].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE);

      stencilDifferentDontCare = (depthDontCareLoad != stencilDontCareLoad);
    }

    bool dontCareLoad = depthDontCareLoad || stencilDontCareLoad;

    // if it's used and has a don't care loadop, or undefined transition (i.e. discard) we
    // need to fill a discard pattern)
    if((dontCareLoad || initialLayout == VK_IMAGE_LAYOUT_UNDEFINED) && rpinfo.attachments[i].used)
    {
      // if originally it was UNDEFINED (which is fine with DONT_CARE) and we promoted to
      // load so we could preserve the discard pattern, transition to general.
      if(initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
      {
        VkImageMemoryBarrier dstimBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            Unwrap(image),
            viewInfo.range,
        };

        DoPipelineBarrier(commandBuffer, 1, &dstimBarrier);

        initialLayout = VK_IMAGE_LAYOUT_GENERAL;

        // undefined transitions apply to the whole subresource not just the render area.
        // But we don't want to do an undefined discard pattern that will be completely
        // overwritten, and it's common for the render area to be the whole subresource. So
        // check that here now and only do the undefined if we're not about to DONT_CARE
        // over it or if the render area is a subset
        // note if there's a separate stencil op and only one of them is getting don't
        // care'd then we still need the undefined for the other. stencilDifferentDontCare
        // is only true if ONLY one of depth & stencil is being don't care'd. dontCareLoad
        // is only false if nothing at all is getting don't care'd.
        if(!dontCareLoad || stencilDifferentDontCare || renderArea.offset.x > 0 ||
           renderArea.offset.y > 0 ||
           renderArea.extent.width < RDCMAX(1U, imInfo.extent.width >> viewInfo.range.baseMipLevel) ||
           renderArea.extent.height < RDCMAX(1U, imInfo.extent.height >> viewInfo.range.baseMipLevel))
        {
          GetDebugManager()->FillWithDiscardPattern(
              commandBuffer, DiscardType::UndefinedTransition, image, initialLayout, viewInfo.range,
              {{0, 0}, {imInfo.extent.width, imInfo.extent.height}});
        }
      }

      if(!stencilDifferentDontCare && dontCareLoad)
      {
        GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassLoad, image,
                                                  initialLayout, viewInfo.range, renderArea);
      }
      else if(stencilDifferentDontCare)
      {
        VkImageSubresourceRange range = viewInfo.range;

        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if(depthDontCareLoad && (viewInfo.range.aspectMask & range.aspectMask) != 0)
          GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassLoad,
                                                    image, initialLayout, range, renderArea);

        range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        if(stencilDontCareLoad && (viewInfo.range.aspectMask & range.aspectMask) != 0)
          GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassLoad,
                                                    image, initialLayout, range, renderArea);
      }
    }
  }
}

// Command pool functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateCommandPool(SerialiserType &ser, VkDevice device,
                                                  const VkCommandPoolCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkCommandPool *pCmdPool)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo).Important();
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(CmdPool, GetResID(*pCmdPool)).TypedAs("VkCommandPool"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCommandPool pool = VK_NULL_HANDLE;

    // remap the queue family index
    CreateInfo.queueFamilyIndex = m_QueueRemapping[CreateInfo.queueFamilyIndex][0].family;

    InsertCommandQueueFamily(CmdPool, CreateInfo.queueFamilyIndex);

    VkResult ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), &CreateInfo, NULL, &pool);

    if(ret != VK_SUCCESS)
    {
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed creating command pool, VkResult: %s", ToStr(ret).c_str());
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
                                            const VkAllocationCallbacks *, VkCommandPool *pCmdPool)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), pCreateInfo, NULL, pCmdPool));

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
      // if we can reset command buffers we need to allocate smaller pages because command buffers
      // may be reset, so each page can only be allocated by at most one command buffer.
      // if not, we allocate bigger pages on the assumption that the application won't waste memory
      // by allocating lots of command pools that barely get used.
      record->cmdPoolInfo = new CmdPoolInfo;
      record->cmdPoolInfo->queueFamilyIndex = pCreateInfo->queueFamilyIndex;
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
  if(Vulkan_Debug_VerboseCommandRecording())
  {
    RDCLOG("Reset command pool %s", ToStr(GetResID(cmdPool)).c_str());
  }

  if(Atomic::CmpExch32(&m_ReuseEnabled, 1, 1) == 1)
    GetRecord(cmdPool)->cmdPoolInfo->pool.Reset();

  {
    VkResourceRecord *poolRecord = GetRecord(cmdPool);
    poolRecord->LockChunks();
    for(auto it = poolRecord->pooledChildren.begin(); it != poolRecord->pooledChildren.end(); ++it)
    {
      (*it)->cmdInfo->alloc.Reset();
    }
    poolRecord->UnlockChunks();
  }

  return ObjDisp(device)->ResetCommandPool(Unwrap(device), Unwrap(cmdPool), flags);
}

void WrappedVulkan::vkTrimCommandPool(VkDevice device, VkCommandPool commandPool,
                                      VkCommandPoolTrimFlags flags)
{
  GetRecord(commandPool)->cmdPoolInfo->pool.Trim();

  return ObjDisp(device)->TrimCommandPool(Unwrap(device), Unwrap(commandPool), flags);
}

// Command buffer functions

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkAllocateCommandBuffers(SerialiserType &ser, VkDevice device,
                                                       const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                       VkCommandBuffer *pCommandBuffers)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(AllocateInfo, *pAllocateInfo).Important();
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
      SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                       "Failed allocating command buffer, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
      GetResourceManager()->AddLiveResource(CommandBuffer, cmd);
      ResourceId poolId = GetResourceManager()->GetOriginalID(GetResID(AllocateInfo.commandPool));
      auto cmdQueueFamilyIt = m_commandQueueFamilies.find(poolId);
      if(cmdQueueFamilyIt == m_commandQueueFamilies.end())
      {
        RDCERR("Missing queue family for %s", ToStr(poolId).c_str());
      }
      else
      {
        InsertCommandQueueFamily(CommandBuffer, cmdQueueFamilyIt->second);
      }
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

        record->DisableChunkLocking();

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

        if(Vulkan_Debug_VerboseCommandRecording())
        {
          RDCLOG("Allocate command buffer %s from pool %s", ToStr(record->GetResourceID()).c_str(),
                 ToStr(record->pool->GetResourceID()).c_str());
        }

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

        record->cmdInfo = new CmdBufferRecordingInfo(*record->pool->cmdPoolInfo);

        record->cmdInfo->device = device;
        record->cmdInfo->allocInfo = *pAllocateInfo;
        record->cmdInfo->allocInfo.commandBufferCount = 1;
        record->cmdInfo->allocRecord = allocRecord;
        record->cmdInfo->present = false;
        record->cmdInfo->beginCapture = false;
        record->cmdInfo->endCapture = false;
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

  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(commandBuffer))
      .TypedAs("VkCommandBuffer"_lit)
      .Important();
  SERIALISE_ELEMENT_LOCAL(BeginInfo, *pBeginInfo).Important();
  SERIALISE_ELEMENT(BakedCommandBuffer);
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(AllocateInfo).Hidden();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    auto cmdQueueFamilyIt = m_commandQueueFamilies.find(CommandBuffer);
    if(cmdQueueFamilyIt == m_commandQueueFamilies.end())
    {
      RDCERR("Unknown queue family for %s", ToStr(CommandBuffer).c_str());
    }
    else
    {
      InsertCommandQueueFamily(BakedCommandBuffer, cmdQueueFamilyIt->second);
    }

    m_LastCmdBufferID = BakedCommandBuffer;

    // when loading, allocate a new resource ID for each push descriptor slot in this command buffer
    if(IsLoading(m_State))
    {
      for(int p = 0; p < 2; p++)
      {
        for(size_t i = 0; i < ARRAY_COUNT(BakedCmdBufferInfo::pushDescriptorID[p]); i++)
        {
          VkDescriptorSet descset = MakeFakePushDescSet();
          ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), descset);
          m_BakedCmdBufferInfo[BakedCommandBuffer].pushDescriptorID[p][i] = id;
          GetResourceManager()->AddLiveResource(id, descset);
        }
      }
    }

    // clear/invalidate descriptor set state for this command buffer.
    for(int p = 0; p < 2; p++)
    {
      for(size_t i = 0; i < ARRAY_COUNT(BakedCmdBufferInfo::pushDescriptorID[p]); i++)
      {
        DescriptorSetInfo &pushDesc =
            m_DescriptorSetState[m_BakedCmdBufferInfo[BakedCommandBuffer].pushDescriptorID[p][i]];
        pushDesc.clear();
        pushDesc.push = true;
      }
    }

    m_BakedCmdBufferInfo[CommandBuffer].level = m_BakedCmdBufferInfo[BakedCommandBuffer].level =
        AllocateInfo.level;
    m_BakedCmdBufferInfo[CommandBuffer].beginFlags =
        m_BakedCmdBufferInfo[BakedCommandBuffer].beginFlags = BeginInfo.flags;
    m_BakedCmdBufferInfo[CommandBuffer].markerCount = 0;
    m_BakedCmdBufferInfo[CommandBuffer].imageStates.clear();
    m_BakedCmdBufferInfo[BakedCommandBuffer].imageStates.clear();
    m_BakedCmdBufferInfo[CommandBuffer].renderPassOpen =
        m_BakedCmdBufferInfo[BakedCommandBuffer].renderPassOpen = false;
    m_BakedCmdBufferInfo[CommandBuffer].activeSubpass =
        m_BakedCmdBufferInfo[BakedCommandBuffer].activeSubpass = 0;
    m_BakedCmdBufferInfo[CommandBuffer].endBarriers.clear();
    m_BakedCmdBufferInfo[BakedCommandBuffer].endBarriers.clear();

    VkCommandBufferBeginInfo unwrappedBeginInfo = BeginInfo;
    VkCommandBufferInheritanceInfo unwrappedInheritInfo;
    if(BeginInfo.pInheritanceInfo)
    {
      unwrappedInheritInfo = *BeginInfo.pInheritanceInfo;

      if(m_ActionCallback && m_ActionCallback->ForceLoadRPs())
      {
        if(unwrappedInheritInfo.framebuffer != VK_NULL_HANDLE)
        {
          const VulkanCreationInfo::Framebuffer &fbinfo =
              m_CreationInfo.m_Framebuffer[GetResID(unwrappedInheritInfo.framebuffer)];

          unwrappedInheritInfo.framebuffer = Unwrap(fbinfo.loadFBs[unwrappedInheritInfo.subpass]);
        }

        if(unwrappedInheritInfo.renderPass != VK_NULL_HANDLE)
        {
          const VulkanCreationInfo::RenderPass &rpinfo =
              m_CreationInfo.m_RenderPass[GetResID(unwrappedInheritInfo.renderPass)];
          unwrappedInheritInfo.renderPass = Unwrap(rpinfo.loadRPs[unwrappedInheritInfo.subpass]);
        }
      }
      else
      {
        unwrappedInheritInfo.framebuffer = Unwrap(unwrappedInheritInfo.framebuffer);
        unwrappedInheritInfo.renderPass = Unwrap(unwrappedInheritInfo.renderPass);
      }

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
      const rdcarray<CommandBufferNode *> &submits = m_Partial.submitLookup[BakedCommandBuffer];

      bool rerecord = false;

      // check for partial execution of this command buffer
      for(const CommandBufferNode *submit : submits)
      {
        if(IsEventInCommandBuffer(submit, m_LastEventID,
                                  m_BakedCmdBufferInfo[BakedCommandBuffer].eventCount))
        {
          SetPartialStack(submit, m_LastEventID);

          GetCmdRenderState().xfbcounters.clear();
          GetCmdRenderState().conditionalRendering.buffer = ResourceId();

          m_PushCommandBuffer = m_LastCmdBufferID;

          rerecord = true;
        }
        else if(submit->beginEvent <= m_LastEventID)
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("vkBegin - full re-record detected %u < %u <= %u, %s -> %s", it->baseEvent,
                   it->baseEvent + length, m_LastEventID, ToStr(CommandBuffer).c_str(),
                   ToStr(BakedCommandBuffer).c_str());
#endif

          // this submission is completely within the range, so it should still be re-recorded
          rerecord = true;
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
          SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                           "Failed beginning command buffer, VkResult: %s", ToStr(ret).c_str());
          return false;
        }
        else
        {
          GetResourceManager()->WrapResource(Unwrap(device), cmd);
        }

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
        RDCDEBUG("vkBegin - re-recording %s -> %s into %s", ToStr(CommandBuffer).c_str(),
                 ToStr(BakedCommandBuffer).c_str(), ToStr(GetResID(cmd)).c_str());
#endif

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
        m_RerecordCmds[CommandBuffer] = cmd;
        InsertCommandQueueFamily(BakedCommandBuffer, FindCommandQueueFamily(CommandBuffer));

        m_RerecordCmdList.push_back({AllocateInfo.commandPool, cmd});

        // add one-time submit flag as this partial cmd buffer will only be submitted once
        BeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(AllocateInfo.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY)
        {
          BeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

          if(BeginInfo.pInheritanceInfo->renderPass != VK_NULL_HANDLE)
            m_BakedCmdBufferInfo[BakedCommandBuffer].state.SetRenderPass(
                GetResID(BeginInfo.pInheritanceInfo->renderPass));
          m_BakedCmdBufferInfo[BakedCommandBuffer].state.subpass =
              BeginInfo.pInheritanceInfo->subpass;
          // framebuffer is not useful here since it may be incomplete (imageless) and it's
          // optional, so we should just treat it as never present.
        }

        ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &unwrappedBeginInfo);
      }

      // whenever a vkCmd command-building chunk asks for the command buffer, it
      // will get our baked version.
      if(GetResourceManager()->HasReplacement(CommandBuffer))
        GetResourceManager()->RemoveReplacement(CommandBuffer);

      GetResourceManager()->ReplaceResource(CommandBuffer, BakedCommandBuffer);

      m_BakedCmdBufferInfo[CommandBuffer].curEventID = 0;
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
          SET_ERROR_RESULT(m_FailedReplayResult, ResultCode::APIReplayFailed,
                           "Failed allocating re-recording command buffer, VkResult: %s",
                           ToStr(ret).c_str());
          return false;
        }
        else
        {
          ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
          GetResourceManager()->AddLiveResource(BakedCommandBuffer, cmd);
        }

        AddResource(BakedCommandBuffer, ResourceType::CommandBuffer, "Baked Command Buffer");
        GetResourceDesc(BakedCommandBuffer).initialisationChunks.clear();
        DerivedResource(device, BakedCommandBuffer);
        DerivedResource(AllocateInfo.commandPool, BakedCommandBuffer);

        // do this one manually since there's no live version of the swapchain, and
        // DerivedResource() assumes we're passing it a live ID (or live resource)
        GetResourceDesc(CommandBuffer).derivedResources.push_back(BakedCommandBuffer);
        GetResourceDesc(BakedCommandBuffer).parentResources.push_back(CommandBuffer);

        // whenever a vkCmd command-building chunk asks for the command buffer, it
        // will get our baked version.
        if(GetResourceManager()->HasReplacement(CommandBuffer))
          GetResourceManager()->RemoveReplacement(CommandBuffer);

        GetResourceManager()->ReplaceResource(CommandBuffer, BakedCommandBuffer);
      }
      else
      {
        cmd = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(BakedCommandBuffer);
      }

      // propagate any name there might be
      if(m_CreationInfo.m_Names.find(CommandBuffer) != m_CreationInfo.m_Names.end())
        m_CreationInfo.m_Names[GetResourceManager()->GetLiveID(BakedCommandBuffer)] =
            m_CreationInfo.m_Names[CommandBuffer];

      {
        VulkanActionTreeNode *action = new VulkanActionTreeNode;
        m_BakedCmdBufferInfo[BakedCommandBuffer].action = action;

        // On queue submit we increment all child events/actions by
        // m_RootEventID and insert them into the tree.
        m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID = 0;
        m_BakedCmdBufferInfo[BakedCommandBuffer].eventCount = 0;
        m_BakedCmdBufferInfo[BakedCommandBuffer].actionCount = 0;

        m_BakedCmdBufferInfo[BakedCommandBuffer].actionStack.push_back(action);

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
    record->bakedCommands->resType = eResCommandBuffer;
    record->bakedCommands->DisableChunkLocking();
    record->bakedCommands->InternalResource = true;
    record->bakedCommands->Resource = (WrappedVkRes *)commandBuffer;
    record->bakedCommands->cmdInfo = new CmdBufferRecordingInfo(*record->pool->cmdPoolInfo);

    record->bakedCommands->cmdInfo->device = record->cmdInfo->device;
    record->bakedCommands->cmdInfo->allocInfo = record->cmdInfo->allocInfo;
    record->bakedCommands->cmdInfo->present = false;
    record->bakedCommands->cmdInfo->beginCapture = false;
    record->bakedCommands->cmdInfo->endCapture = false;

    if(Vulkan_Debug_VerboseCommandRecording())
    {
      RDCLOG("Begin command buffer %s baked to %s", ToStr(record->GetResourceID()).c_str(),
             ToStr(record->bakedCommands->GetResourceID()).c_str());
    }

    record->DeleteChunks();

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkBeginCommandBuffer);
      Serialise_vkBeginCommandBuffer(ser, commandBuffer, pBeginInfo);

      record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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

  SERIALISE_ELEMENT_LOCAL(CommandBuffer, GetResID(commandBuffer))
      .TypedAs("VkCommandBuffer"_lit)
      .Important();
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
        RDCDEBUG("Ending re-recorded command buffer for %s baked to %s as %s",
                 ToStr(CommandBuffer).c_str(), ToStr(BakedCommandBuffer).c_str(),
                 ToStr(GetResID(commandBuffer)).c_str());
#endif

        VulkanRenderState &renderstate = GetCmdRenderState();

        if(IsCommandBufferPartialPrimary(BakedCommandBuffer))
        {
          if(!renderstate.xfbcounters.empty())
            renderstate.EndTransformFeedback(this, commandBuffer);

          if(renderstate.IsConditionalRenderingEnabled())
            renderstate.EndConditionalRendering(commandBuffer);
        }

        // finish any render pass that was still active in the primary partial parent
        if(IsCommandBufferPartial(m_LastCmdBufferID) &&
           GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive)
        {
          if(m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen)
          {
            RDCERR(
                "We shouldn't expect any render pass to still be open at "
                "vkEndCommandBuffer time");
          }

          if(renderstate.dynamicRendering.active)
          {
            // the only way dynamic rendering can be active in a partial command buffer is
            // if it's suspended, as the matching vkCmdEndRendering will be replayed before
            // vkEndCommandBuffer even if outside of rerecord range.
            // We need to resume and then end without suspending.
            bool suspended = (renderstate.dynamicRendering.flags & VK_RENDERING_SUSPENDING_BIT) != 0;
            if(suspended)
            {
              VkRenderingInfo info = {};
              info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

              // resume but don't suspend - end for real
              info.flags = renderstate.dynamicRendering.flags &
                           ~(VK_RENDERING_RESUMING_BIT | VK_RENDERING_SUSPENDING_BIT);
              info.flags |= VK_RENDERING_RESUMING_BIT;

              info.layerCount = renderstate.dynamicRendering.layerCount;
              info.renderArea = renderstate.renderArea;
              info.viewMask = renderstate.dynamicRendering.viewMask;

              info.pDepthAttachment = &renderstate.dynamicRendering.depth;
              if(renderstate.dynamicRendering.depth.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                info.pDepthAttachment = NULL;
              info.pStencilAttachment = &renderstate.dynamicRendering.stencil;
              if(renderstate.dynamicRendering.stencil.imageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                info.pStencilAttachment = NULL;

              info.colorAttachmentCount = (uint32_t)renderstate.dynamicRendering.color.size();
              info.pColorAttachments = renderstate.dynamicRendering.color.data();

              VkRenderingFragmentDensityMapAttachmentInfoEXT fragmentDensity = {
                  VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,
                  NULL,
                  renderstate.dynamicRendering.fragmentDensityView,
                  renderstate.dynamicRendering.fragmentDensityLayout,
              };

              if(renderstate.dynamicRendering.fragmentDensityView != VK_NULL_HANDLE)
              {
                fragmentDensity.pNext = info.pNext;
                info.pNext = &fragmentDensity;
              }

              VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRate = {
                  VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,
                  NULL,
                  renderstate.dynamicRendering.shadingRateView,
                  renderstate.dynamicRendering.shadingRateLayout,
                  renderstate.dynamicRendering.shadingRateTexelSize,
              };

              if(renderstate.dynamicRendering.shadingRateView != VK_NULL_HANDLE)
              {
                shadingRate.pNext = info.pNext;
                info.pNext = &shadingRate;
              }

              VkMultisampledRenderToSingleSampledInfoEXT tileOnlyMSAA = {
                  VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT,
                  NULL,
                  renderstate.dynamicRendering.tileOnlyMSAAEnable,
                  renderstate.dynamicRendering.tileOnlyMSAASampleCount,
              };

              if(renderstate.dynamicRendering.tileOnlyMSAAEnable)
              {
                tileOnlyMSAA.pNext = info.pNext;
                info.pNext = &tileOnlyMSAA;
              }

              byte *tempMem = GetTempMemory(GetNextPatchSize(&info));
              VkRenderingInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, &info);

              // do the same load/store patching as normal here too
              if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
              {
                for(uint32_t i = 0; i < unwrappedInfo->colorAttachmentCount + 2; i++)
                {
                  VkRenderingAttachmentInfo *att =
                      (VkRenderingAttachmentInfo *)unwrappedInfo->pColorAttachments + i;

                  if(i == unwrappedInfo->colorAttachmentCount)
                    att = (VkRenderingAttachmentInfo *)unwrappedInfo->pDepthAttachment;
                  else if(i == unwrappedInfo->colorAttachmentCount + 1)
                    att = (VkRenderingAttachmentInfo *)unwrappedInfo->pStencilAttachment;

                  if(!att)
                    continue;

                  if(att->storeOp != VK_ATTACHMENT_STORE_OP_NONE)
                    att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                  if(att->loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
                    att->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                }
              }

              ObjDisp(commandBuffer)->CmdBeginRendering(Unwrap(commandBuffer), unwrappedInfo);
              ObjDisp(commandBuffer)->CmdEndRendering(Unwrap(commandBuffer));
            }
          }
          else
          {
            // for each subpass we skip, and for the finalLayout transition at the end of the
            // renderpass, replay the recorded barriers from the implicit transitions in
            // renderPassEndStates. These are executed implicitly but because we want
            // to pretend they never happened, we then reverse their effects so that our layout
            // tracking is accurate and the images end up in the layout they were in during the last
            // active subpass when we stopped partially replaying
            rdcarray<VkImageMemoryBarrier> &endBarriers =
                m_BakedCmdBufferInfo[m_LastCmdBufferID].endBarriers;

            // do the barriers in reverse order
            std::reverse(endBarriers.begin(), endBarriers.end());
            for(VkImageMemoryBarrier &barrier : endBarriers)
            {
              std::swap(barrier.oldLayout, barrier.newLayout);

              // sanitise layouts before passing to vulkan
              SanitiseOldImageLayout(barrier.oldLayout);
              SanitiseReplayImageLayout(barrier.newLayout);
            }

            // it's unnecessary to replay barriers towards an undefined layout, since every layout
            // can be considered as undefined
            endBarriers.removeIf([](const VkImageMemoryBarrier &b) {
              return b.newLayout == VK_IMAGE_LAYOUT_UNDEFINED;
            });

            DoPipelineBarrier(commandBuffer, endBarriers.size(), endBarriers.data());
          }
        }

        // also finish any nested markers we truncated and didn't finish
        if(ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT)
          for(int i = 0; i < m_BakedCmdBufferInfo[BakedCommandBuffer].markerCount; i++)
            ObjDisp(commandBuffer)->CmdDebugMarkerEndEXT(Unwrap(commandBuffer));

        if(m_ActionCallback)
          m_ActionCallback->PreEndCommandBuffer(commandBuffer);

        ObjDisp(commandBuffer)->EndCommandBuffer(Unwrap(commandBuffer));

        // TODO: preserve so that m_RenderState can be updated at the end
        // of replay.
        // if(m_Partial[Primary].partialParent == BakedCommandBuffer)
        //  m_Partial[Primary].partialParent = ResourceId();
      }

      m_BakedCmdBufferInfo[CommandBuffer].curEventID = 0;
    }
    else
    {
      commandBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(BakedCommandBuffer);

      ObjDisp(commandBuffer)->EndCommandBuffer(Unwrap(commandBuffer));

      {
        if(GetActionStack().size() > 1)
          GetActionStack().pop_back();
      }

      {
        m_BakedCmdBufferInfo[BakedCommandBuffer].eventCount =
            m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID;
        m_BakedCmdBufferInfo[BakedCommandBuffer].curEventID = 0;

        m_BakedCmdBufferInfo[BakedCommandBuffer].endChunk =
            uint32_t(m_StructuredFile->chunks.size() - 1);

        m_BakedCmdBufferInfo[CommandBuffer].curEventID = 0;
        m_BakedCmdBufferInfo[CommandBuffer].eventCount = 0;
        m_BakedCmdBufferInfo[CommandBuffer].actionCount = 0;
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

    if(Vulkan_Debug_VerboseCommandRecording())
    {
      RDCLOG("End command buffer %s baked to %s", ToStr(record->GetResourceID()).c_str(),
             ToStr(record->bakedCommands->GetResourceID()).c_str());
    }

    {
      CACHE_THREAD_SERIALISER();
      ser.SetActionChunk();
      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkEndCommandBuffer);
      Serialise_vkEndCommandBuffer(ser, commandBuffer);

      record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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
    if(Vulkan_Debug_VerboseCommandRecording())
    {
      RDCLOG(
          "Reset command buffer %s (baked was %s)", ToStr(record->GetResourceID()).c_str(),
          ToStr(record->bakedCommands ? record->bakedCommands->GetResourceID() : ResourceId()).c_str());
    }

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
  SERIALISE_ELEMENT_LOCAL(RenderPassBegin, *pRenderPassBegin).Important();
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

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderpassActive(m_LastCmdBufferID, false))
        {
          GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = true;
        }

        m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass = 0;

        ResourceId fb = GetResID(RenderPassBegin.framebuffer);
        VulkanCreationInfo::Framebuffer fbinfo = m_CreationInfo.m_Framebuffer[fb];

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.subpass = 0;
          renderstate.SetRenderPass(GetResID(RenderPassBegin.renderPass));
          renderstate.renderArea = RenderPassBegin.renderArea;
          renderstate.subpassContents = contents;

          const VkRenderPassAttachmentBeginInfo *attachmentsInfo =
              (const VkRenderPassAttachmentBeginInfo *)FindNextStruct(
                  &RenderPassBegin, VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);

          rdcarray<ResourceId> attachments;
          attachments.resize(fbinfo.attachments.size());

          // set framebuffer attachments - by default from the ones used to create it, but if it is
          // imageless then look for the attachments in our pNext chain
          if(!fbinfo.imageless)
          {
            for(size_t i = 0; i < fbinfo.attachments.size(); i++)
              attachments[i] = fbinfo.attachments[i].createdView;
          }
          else
          {
            for(size_t i = 0; i < fbinfo.attachments.size(); i++)
              attachments[i] = GetResID(attachmentsInfo->pAttachments[i]);
          }
          renderstate.SetFramebuffer(GetResID(RenderPassBegin.framebuffer), attachments);
        }

        const VulkanCreationInfo::RenderPass &rpinfo =
            m_CreationInfo.m_RenderPass[GetCmdRenderState().GetRenderPass()];

        rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        ApplyRPLoadDiscards(commandBuffer, RenderPassBegin.renderArea);

        // if we're just replaying the vkCmdBeginRenderPass on its own, we use the first loadRP
        // instead of the real thing. This then doesn't require us to finish off any subpasses etc.
        // we need to manually do the subpass 0 barriers, since loadRP expects the image to already
        // be in subpass 0's layout
        // we also need to manually do any clears, since the loadRP will load all attachments
        if(m_FirstEventID == m_LastEventID)
        {
          unwrappedInfo.renderPass = Unwrap(rpinfo.loadRPs[0]);
          unwrappedInfo.framebuffer = Unwrap(fbinfo.loadFBs[0]);

          if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
          {
            for(VkImageMemoryBarrier &barrier : imgBarriers)
            {
              if(barrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            }
          }

          DoPipelineBarrier(commandBuffer, imgBarriers.size(), imgBarriers.data());
        }

        ActionFlags drawFlags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        ObjDisp(commandBuffer)->CmdBeginRenderPass(Unwrap(commandBuffer), &unwrappedInfo, contents);

        if(m_FirstEventID == m_LastEventID)
        {
          const rdcarray<ResourceId> &fbattachments =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetFramebufferAttachments();

          rdcarray<VkClearAttachment> clearatts;
          rdcarray<VkClearRect> clearrects;
          for(int32_t c = 0; c < rpinfo.subpasses[0].colorAttachments.count() + 1; c++)
          {
            uint32_t att = ~0U;

            if(c < rpinfo.subpasses[0].colorAttachments.count())
              att = rpinfo.subpasses[0].colorAttachments[c];
            else if(rpinfo.subpasses[0].depthstencilAttachment >= 0)
              att = (uint32_t)rpinfo.subpasses[0].depthstencilAttachment;

            if(att >= rpinfo.attachments.size())
              continue;

            VkImageAspectFlags clearAspects = 0;

            // loadOp governs color, and depth
            if(rpinfo.attachments[att].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
              clearAspects |= VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
            // stencilLoadOp governs the stencil
            if(rpinfo.attachments[att].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
              clearAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;

            // if any aspect is set to clear, go check it in more detail
            if(clearAspects != 0)
            {
              VulkanCreationInfo::ImageView viewinfo = m_CreationInfo.m_ImageView[fbattachments[att]];
              bool isMultiview = rpinfo.subpasses[0].multiviews.size() > 1;

              VkClearRect rect = {unwrappedInfo.renderArea, 0,
                                  isMultiview ? 1 : viewinfo.range.layerCount};
              VkClearAttachment clear = {};
              clear.aspectMask = FormatImageAspects(rpinfo.attachments[att].format) & clearAspects;
              clear.colorAttachment = c;
              if(att < unwrappedInfo.clearValueCount)
                clear.clearValue = unwrappedInfo.pClearValues[att];
              else
                RDCWARN("Missing clear value for attachment %u", att);

              // check that the actual aspects in the attachment overlap with those being cleared.
              // In particular this means we ignore stencil load op being CLEAR for a color
              // attachment - that doesn't mean we should clear the color. This also means we don't
              // clear the stencil if it's not specified, even when clearing depth *is*
              if(clear.aspectMask != 0)
              {
                clearrects.push_back(rect);
                clearatts.push_back(clear);
              }
            }
          }

          if(!clearatts.empty())
            ObjDisp(commandBuffer)
                ->CmdClearAttachments(Unwrap(commandBuffer), (uint32_t)clearatts.size(),
                                      clearatts.data(), (uint32_t)clearrects.size(),
                                      clearrects.data());
        }

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdBeginRenderPass again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                             FindCommandQueueFamily(m_LastCmdBufferID),
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdBeginRenderPass(Unwrap(commandBuffer), &unwrappedInfo, contents);

      // track while reading, for fetching the right set of outputs in AddAction
      m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetRenderPass(
          GetResID(RenderPassBegin.renderPass));

      ResourceId fb = GetResID(RenderPassBegin.framebuffer);

      // set framebuffer attachments - by default from the ones used to create it, but if it is
      // imageless then look for the attachments in our pNext chain
      {
        VulkanCreationInfo::Framebuffer fbinfo = m_CreationInfo.m_Framebuffer[fb];
        rdcarray<ResourceId> attachments;
        attachments.resize(fbinfo.attachments.size());

        if(!fbinfo.imageless)
        {
          for(size_t i = 0; i < fbinfo.attachments.size(); i++)
            attachments[i] = fbinfo.attachments[i].createdView;
        }
        else
        {
          const VkRenderPassAttachmentBeginInfo *attachmentsInfo =
              (const VkRenderPassAttachmentBeginInfo *)FindNextStruct(
                  &RenderPassBegin, VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);

          for(size_t i = 0; i < fbinfo.attachments.size(); i++)
            attachments[i] = GetResID(attachmentsInfo->pAttachments[i]);
        }
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetFramebuffer(fb, attachments);
      }

      // Record image usage for images cleared in the beginning of the render pass.
      const VulkanCreationInfo::RenderPass &rpinfo =
          m_CreationInfo.m_RenderPass[GetResID(RenderPassBegin.renderPass)];
      const rdcarray<ResourceId> &fbattachments =
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetFramebufferAttachments();
      for(size_t i = 0; i < rpinfo.attachments.size(); i++)
      {
        if(rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ||
           rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        {
          ResourceId image = m_CreationInfo.m_ImageView[fbattachments[i]].image;
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              image, EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID,
                                rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
                                    ? ResourceUsage::Clear
                                    : ResourceUsage::Discard,
                                fbattachments[i])));
        }
      }

      rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      ActionDescription action;
      action.customName =
          StringFormat::Fmt("vkCmdBeginRenderPass(%s)", MakeRenderPassOpString(false).c_str());
      action.flags |= ActionFlags::PassBoundary | ActionFlags::BeginPass;

      AddAction(action);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginRenderPass);
    Serialise_vkCmdBeginRenderPass(ser, commandBuffer, pRenderPassBegin, contents);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);

    VkResourceRecord *fb = GetRecord(pRenderPassBegin->framebuffer);
    VkResourceRecord *rp = GetRecord(pRenderPassBegin->renderPass);

    record->MarkResourceFrameReferenced(fb->GetResourceID(), eFrameRef_Read);

    rdcarray<VkImageMemoryBarrier> &barriers = record->cmdInfo->rpbarriers;

    barriers.clear();

    FramebufferInfo *fbInfo = fb->framebufferInfo;
    RenderPassInfo *rpInfo = rp->renderPassInfo;

    bool renderArea_covers_entire_framebuffer =
        pRenderPassBegin->renderArea.offset.x == 0 && pRenderPassBegin->renderArea.offset.y == 0 &&
        pRenderPassBegin->renderArea.extent.width >= fbInfo->width &&
        pRenderPassBegin->renderArea.extent.height >= fbInfo->height;

    const VkRenderPassAttachmentBeginInfo *attachmentsInfo =
        (const VkRenderPassAttachmentBeginInfo *)FindNextStruct(
            pRenderPassBegin, VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);

    // ignore degenerate struct (which is only valid - and indeed required - for a non-imageless
    // framebuffer)
    if(attachmentsInfo && attachmentsInfo->attachmentCount == 0)
      attachmentsInfo = NULL;

    for(size_t i = 0; fbInfo->imageAttachments[i].barrier.sType; i++)
    {
      VkResourceRecord *att = fbInfo->imageAttachments[i].record;

      if(attachmentsInfo && !att)
        att = GetRecord(attachmentsInfo->pAttachments[i]);

      if(att == NULL)
        break;

      bool framebuffer_reference_entire_attachment =
          fbInfo->AttachmentFullyReferenced(i, att, att->viewRange, rpInfo);

      FrameRefType refType = eFrameRef_ReadBeforeWrite;

      if(renderArea_covers_entire_framebuffer && framebuffer_reference_entire_attachment)
      {
        if(rpInfo->loadOpTable[i] != VK_ATTACHMENT_LOAD_OP_LOAD &&
           rpInfo->loadOpTable[i] != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
        {
          refType = eFrameRef_CompleteWrite;
        }
      }

      // if we're completely writing this resource (i.e. nothing from previous data is visible) and
      // it's also DONT_CARE storage (so nothing from this render pass will be visible after) then
      // it's completely written and discarded in one go.
      if(refType == eFrameRef_CompleteWrite &&
         rpInfo->storeOpTable[i] == VK_ATTACHMENT_STORE_OP_DONT_CARE)
      {
        refType = eFrameRef_CompleteWriteAndDiscard;
      }

      record->MarkImageViewFrameReferenced(att, ImageRange(), refType);

      if(fbInfo->imageAttachments[i].barrier.oldLayout !=
         fbInfo->imageAttachments[i].barrier.newLayout)
      {
        VkImageMemoryBarrier barrier = fbInfo->imageAttachments[i].barrier;

        if(attachmentsInfo)
        {
          barrier.image = GetResourceManager()->GetCurrentHandle<VkImage>(att->baseResource);
          barrier.subresourceRange = att->viewRange;
        }

        barriers.push_back(barrier);
      }
    }

    record->cmdInfo->framebuffer = fb;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdNextSubpass(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               VkSubpassContents contents)
{
  SERIALISE_ELEMENT(commandBuffer).Unimportant();
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

        {
          GetCmdRenderState().subpass++;
          m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass++;
        }

        ActionFlags drawFlags =
            ActionFlags::PassBoundary | ActionFlags::BeginPass | ActionFlags::EndPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), contents);

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdNextSubpass again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                             FindCommandQueueFamily(m_LastCmdBufferID),
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
      else if(IsRenderpassOpen(m_LastCmdBufferID) && m_FirstEventID != m_LastEventID)
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), contents);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass++;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].endBarriers.append(GetImplicitRenderPassBarriers());
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdNextSubpass(Unwrap(commandBuffer), contents);

      AddImplicitResolveResourceUsage();

      // track while reading, for fetching the right set of outputs in AddAction
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass++;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass++;

      rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("vkCmdNextSubpass() => %u",
                                            m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass);
      action.flags |= ActionFlags::PassBoundary | ActionFlags::BeginPass | ActionFlags::EndPass;

      AddAction(action);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdNextSubpass);
    Serialise_vkCmdNextSubpass(ser, commandBuffer, contents);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndRenderPass(SerialiserType &ser, VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer).Unimportant();

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

        rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderpassActive(m_LastCmdBufferID, false))
        {
          GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = false;
        }

        rdcarray<ResourceId> attachments;
        VkRect2D renderArea;

        // save the renderpass that we were in here, so we can look up the rpinfo below
        ResourceId currentRP = GetCmdRenderState().GetRenderPass();

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          attachments = GetCmdRenderState().GetFramebufferAttachments();
          renderArea = GetCmdRenderState().renderArea;

          renderstate.SetRenderPass(ResourceId());
          renderstate.SetFramebuffer(ResourceId(), rdcarray<ResourceId>());
          renderstate.subpassContents = VK_SUBPASS_CONTENTS_MAX_ENUM;
        }

        ActionFlags drawFlags = ActionFlags::PassBoundary | ActionFlags::EndPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        ObjDisp(commandBuffer)->CmdEndRenderPass(Unwrap(commandBuffer));

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdEndRenderPass again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest &&
           !m_FeedbackRPs.contains(currentRP))
        {
          const VulkanCreationInfo::RenderPass &rpinfo = m_CreationInfo.m_RenderPass[currentRP];

          for(size_t i = 0; i < attachments.size(); i++)
          {
            if(!rpinfo.attachments[i].used)
              continue;

            const VulkanCreationInfo::ImageView &viewInfo =
                m_CreationInfo.m_ImageView[attachments[i]];
            VkImage image = GetResourceManager()->GetCurrentHandle<VkImage>(viewInfo.image);

            if(IsStencilFormat(viewInfo.format))
            {
              // check to see if stencil and depth store ops are different and apply them
              // individually here
              const bool depthDontCareStore =
                  (rpinfo.attachments[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);
              const bool stencilDontCareStore =
                  (rpinfo.attachments[i].stencilStoreOp == VK_ATTACHMENT_STORE_OP_DONT_CARE);

              // if they're both don't care then we can do a simple discard clear
              if(depthDontCareStore && stencilDontCareStore)
              {
                GetDebugManager()->FillWithDiscardPattern(
                    commandBuffer, DiscardType::RenderPassStore, image,
                    rpinfo.attachments[i].finalLayout, viewInfo.range, renderArea);
              }
              else
              {
                // otherwise only don't care the appropriate aspects
                VkImageSubresourceRange range = viewInfo.range;

                range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                if(depthDontCareStore && (viewInfo.range.aspectMask & range.aspectMask) != 0)
                  GetDebugManager()->FillWithDiscardPattern(
                      commandBuffer, DiscardType::RenderPassStore, image,
                      rpinfo.attachments[i].finalLayout, range, renderArea);

                range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                if(stencilDontCareStore && (viewInfo.range.aspectMask & range.aspectMask) != 0)
                  GetDebugManager()->FillWithDiscardPattern(
                      commandBuffer, DiscardType::RenderPassStore, image,
                      rpinfo.attachments[i].finalLayout, range, renderArea);
              }
            }
            else if(rpinfo.attachments[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
            {
              GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassStore,
                                                        image, rpinfo.attachments[i].finalLayout,
                                                        viewInfo.range, renderArea);
            }
          }
        }

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                             FindCommandQueueFamily(m_LastCmdBufferID),
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
      else if(IsRenderpassOpen(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)->CmdEndRenderPass(Unwrap(commandBuffer));

        m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = false;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].endBarriers.append(GetImplicitRenderPassBarriers(~0U));
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

      rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddImplicitResolveResourceUsage(~0U);

      AddEvent();
      ActionDescription action;
      action.customName =
          StringFormat::Fmt("vkCmdEndRenderPass(%s)", MakeRenderPassOpString(true).c_str());
      action.flags |= ActionFlags::PassBoundary | ActionFlags::EndPass;

      AddAction(action);

      // track while reading, reset this to empty so AddAction sets no outputs,
      // but only AFTER the above AddAction (we want it grouped together)
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetRenderPass(ResourceId());
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetFramebuffer(ResourceId(),
                                                                   rdcarray<ResourceId>());
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndRenderPass);
    Serialise_vkCmdEndRenderPass(ser, commandBuffer);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    const rdcarray<VkImageMemoryBarrier> &barriers = record->cmdInfo->rpbarriers;

    // apply the implicit layout transitions here
    GetResourceManager()->RecordBarriers(record->cmdInfo->imageStates,
                                         record->pool->cmdPoolInfo->queueFamilyIndex,
                                         (uint32_t)barriers.size(), barriers.data());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginRenderPass2(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer,
                                                    const VkRenderPassBeginInfo *pRenderPassBegin,
                                                    const VkSubpassBeginInfo *pSubpassBeginInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(RenderPassBegin, *pRenderPassBegin).Important();
  SERIALISE_ELEMENT_LOCAL(SubpassBegin, *pSubpassBeginInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkRenderPassBeginInfo unwrappedInfo = RenderPassBegin;
    unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
    unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);

    VkSubpassBeginInfo unwrappedBeginInfo = SubpassBegin;

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext) +
                                  GetNextPatchSize(unwrappedBeginInfo.pNext));

    UnwrapNextChain(m_State, "VkRenderPassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedInfo);
    UnwrapNextChain(m_State, "VkSubpassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedBeginInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderpassActive(m_LastCmdBufferID, false))
        {
          GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = true;
        }

        m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass = 0;

        ResourceId fb = GetResID(RenderPassBegin.framebuffer);
        VulkanCreationInfo::Framebuffer fbinfo = m_CreationInfo.m_Framebuffer[fb];
        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.subpass = 0;
          renderstate.SetRenderPass(GetResID(RenderPassBegin.renderPass));
          renderstate.renderArea = RenderPassBegin.renderArea;
          renderstate.subpassContents = SubpassBegin.contents;

          const VkRenderPassAttachmentBeginInfo *attachmentsInfo =
              (const VkRenderPassAttachmentBeginInfo *)FindNextStruct(
                  &RenderPassBegin, VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);

          rdcarray<ResourceId> attachments;
          attachments.resize(fbinfo.attachments.size());

          // set framebuffer attachments - by default from the ones used to create it, but if it is
          // imageless then look for the attachments in our pNext chain
          if(!fbinfo.imageless)
          {
            for(size_t i = 0; i < fbinfo.attachments.size(); i++)
              attachments[i] = fbinfo.attachments[i].createdView;
          }
          else
          {
            for(size_t i = 0; i < fbinfo.attachments.size(); i++)
              attachments[i] = GetResID(attachmentsInfo->pAttachments[i]);
          }
          renderstate.SetFramebuffer(GetResID(RenderPassBegin.framebuffer), attachments);
        }

        const VulkanCreationInfo::RenderPass &rpinfo =
            m_CreationInfo.m_RenderPass[GetCmdRenderState().GetRenderPass()];

        ApplyRPLoadDiscards(commandBuffer, RenderPassBegin.renderArea);

        rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        // if we're just replaying the vkCmdBeginRenderPass on its own, we use the first loadRP
        // instead of the real thing. This then doesn't require us to finish off any subpasses etc.
        // we need to manually do the subpass 0 barriers, since loadRP expects the image to already
        // be in subpass 0's layout
        // we also need to manually do any clears, since the loadRP will load all attachments
        if(m_FirstEventID == m_LastEventID)
        {
          unwrappedInfo.renderPass = Unwrap(rpinfo.loadRPs[0]);
          unwrappedInfo.framebuffer = Unwrap(fbinfo.loadFBs[0]);

          DoPipelineBarrier(commandBuffer, imgBarriers.size(), imgBarriers.data());
        }

        ActionFlags drawFlags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        ObjDisp(commandBuffer)
            ->CmdBeginRenderPass2(Unwrap(commandBuffer), &unwrappedInfo, &unwrappedBeginInfo);

        if(m_FirstEventID == m_LastEventID)
        {
          const rdcarray<ResourceId> &fbattachments =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetFramebufferAttachments();

          rdcarray<VkClearAttachment> clearatts;
          rdcarray<VkClearRect> clearrects;
          RDCASSERT(unwrappedInfo.clearValueCount <= (uint32_t)rpinfo.attachments.size(),
                    unwrappedInfo.clearValueCount, rpinfo.attachments.size());
          for(int32_t c = 0; c < rpinfo.subpasses[0].colorAttachments.count() + 1; c++)
          {
            uint32_t att = ~0U;

            if(c < rpinfo.subpasses[0].colorAttachments.count())
              att = rpinfo.subpasses[0].colorAttachments[c];
            else if(rpinfo.subpasses[0].depthstencilAttachment >= 0)
              att = (uint32_t)rpinfo.subpasses[0].depthstencilAttachment;

            if(att >= rpinfo.attachments.size())
              continue;

            VkImageAspectFlags clearAspects = 0;

            // loadOp governs color, and depth
            if(rpinfo.attachments[att].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
              clearAspects |= VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
            // stencilLoadOp governs the stencil
            if(rpinfo.attachments[att].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
              clearAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;

            // if any aspect is set to clear, go check it in more detail
            if(clearAspects != 0)
            {
              VulkanCreationInfo::ImageView viewinfo = m_CreationInfo.m_ImageView[fbattachments[att]];
              bool isMultiview = rpinfo.subpasses[0].multiviews.size() > 1;

              VkClearRect rect = {unwrappedInfo.renderArea, 0,
                                  isMultiview ? 1 : viewinfo.range.layerCount};
              VkClearAttachment clear = {};
              clear.aspectMask = FormatImageAspects(rpinfo.attachments[att].format) & clearAspects;
              clear.colorAttachment = c;
              if(att < unwrappedInfo.clearValueCount)
                clear.clearValue = unwrappedInfo.pClearValues[att];
              else
                RDCWARN("Missing clear value for attachment %u", att);

              // check that the actual aspects in the attachment overlap with those being cleared.
              // In particular this means we ignore stencil load op being CLEAR for a color
              // attachment - that doesn't mean we should clear the color. This also means we don't
              // clear the stencil if it's not specified, even when clearing depth *is*
              if(clear.aspectMask != 0)
              {
                clearrects.push_back(rect);
                clearatts.push_back(clear);
              }
            }
          }

          if(!clearatts.empty())
            ObjDisp(commandBuffer)
                ->CmdClearAttachments(Unwrap(commandBuffer), (uint32_t)clearatts.size(),
                                      clearatts.data(), (uint32_t)clearrects.size(),
                                      clearrects.data());
        }

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdBeginRenderPass2 again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                             FindCommandQueueFamily(m_LastCmdBufferID),
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdBeginRenderPass2(Unwrap(commandBuffer), &unwrappedInfo, &unwrappedBeginInfo);

      // track while reading, for fetching the right set of outputs in AddAction
      m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetRenderPass(
          GetResID(RenderPassBegin.renderPass));

      ResourceId fb = GetResID(RenderPassBegin.framebuffer);

      // set framebuffer attachments - by default from the ones used to create it, but if it is
      // imageless then look for the attachments in our pNext chain
      {
        VulkanCreationInfo::Framebuffer fbinfo = m_CreationInfo.m_Framebuffer[fb];
        rdcarray<ResourceId> attachments;
        attachments.resize(fbinfo.attachments.size());

        if(!fbinfo.imageless)
        {
          for(size_t i = 0; i < fbinfo.attachments.size(); i++)
            attachments[i] = fbinfo.attachments[i].createdView;
        }
        else
        {
          const VkRenderPassAttachmentBeginInfo *attachmentsInfo =
              (const VkRenderPassAttachmentBeginInfo *)FindNextStruct(
                  &RenderPassBegin, VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);

          for(size_t i = 0; i < fbinfo.attachments.size(); i++)
            attachments[i] = GetResID(attachmentsInfo->pAttachments[i]);
        }
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetFramebuffer(fb, attachments);
      }

      // Record image usage for images cleared in the beginning of the render pass.
      const VulkanCreationInfo::RenderPass &rpinfo =
          m_CreationInfo.m_RenderPass[GetResID(RenderPassBegin.renderPass)];
      const rdcarray<ResourceId> &fbattachments =
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetFramebufferAttachments();
      for(size_t i = 0; i < rpinfo.attachments.size(); i++)
      {
        if(rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ||
           rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        {
          ResourceId image = m_CreationInfo.m_ImageView[fbattachments[i]].image;
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              image, EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID,
                                rpinfo.attachments[i].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR
                                    ? ResourceUsage::Clear
                                    : ResourceUsage::Discard,
                                fbattachments[i])));
        }
      }

      rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      ActionDescription action;
      action.customName =
          StringFormat::Fmt("vkCmdBeginRenderPass2(%s)", MakeRenderPassOpString(false).c_str());
      action.flags |= ActionFlags::PassBoundary | ActionFlags::BeginPass;

      AddAction(action);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                                          const VkRenderPassBeginInfo *pRenderPassBegin,
                                          const VkSubpassBeginInfo *pSubpassBeginInfo)
{
  SCOPED_DBG_SINK();

  VkRenderPassBeginInfo unwrappedInfo = *pRenderPassBegin;
  unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
  unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);

  VkSubpassBeginInfo unwrappedBeginInfo = *pSubpassBeginInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext) +
                                GetNextPatchSize(unwrappedBeginInfo.pNext));

  UnwrapNextChain(m_State, "VkRenderPassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedInfo);
  UnwrapNextChain(m_State, "VkSubpassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedBeginInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdBeginRenderPass2(Unwrap(commandBuffer), &unwrappedInfo, &unwrappedBeginInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginRenderPass2);
    Serialise_vkCmdBeginRenderPass2(ser, commandBuffer, pRenderPassBegin, pSubpassBeginInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);

    VkResourceRecord *fb = GetRecord(pRenderPassBegin->framebuffer);
    VkResourceRecord *rp = GetRecord(pRenderPassBegin->renderPass);

    record->MarkResourceFrameReferenced(fb->GetResourceID(), eFrameRef_Read);

    rdcarray<VkImageMemoryBarrier> &barriers = record->cmdInfo->rpbarriers;

    barriers.clear();

    FramebufferInfo *fbInfo = fb->framebufferInfo;
    RenderPassInfo *rpInfo = rp->renderPassInfo;
    bool renderArea_covers_entire_framebuffer =
        pRenderPassBegin->renderArea.offset.x == 0 && pRenderPassBegin->renderArea.offset.y == 0 &&
        pRenderPassBegin->renderArea.extent.width >= fbInfo->width &&
        pRenderPassBegin->renderArea.extent.height >= fbInfo->height;

    const VkRenderPassAttachmentBeginInfo *attachmentsInfo =
        (const VkRenderPassAttachmentBeginInfo *)FindNextStruct(
            pRenderPassBegin, VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO);

    // ignore degenerate struct (which is only valid - and indeed required - for a non-imageless
    // framebuffer)
    if(attachmentsInfo && attachmentsInfo->attachmentCount == 0)
      attachmentsInfo = NULL;

    for(size_t i = 0; fbInfo->imageAttachments[i].barrier.sType; i++)
    {
      VkResourceRecord *att = fbInfo->imageAttachments[i].record;

      if(attachmentsInfo && !att)
        att = GetRecord(attachmentsInfo->pAttachments[i]);

      if(att == NULL)
        break;

      bool framebuffer_reference_entire_attachment =
          fbInfo->AttachmentFullyReferenced(i, att, att->viewRange, rpInfo);

      FrameRefType refType = eFrameRef_ReadBeforeWrite;

      if(renderArea_covers_entire_framebuffer && framebuffer_reference_entire_attachment)
      {
        if(rpInfo->loadOpTable[i] != VK_ATTACHMENT_LOAD_OP_LOAD &&
           rpInfo->loadOpTable[i] != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
        {
          refType = eFrameRef_CompleteWrite;
        }
      }

      // if we're completely writing this resource (i.e. nothing from previous data is visible) and
      // it's also DONT_CARE storage (so nothing from this render pass will be visible after) then
      // it's completely written and discarded in one go.
      if(refType == eFrameRef_CompleteWrite &&
         rpInfo->storeOpTable[i] == VK_ATTACHMENT_STORE_OP_DONT_CARE)
      {
        refType = eFrameRef_CompleteWriteAndDiscard;
      }

      record->MarkImageViewFrameReferenced(att, ImageRange(), refType);

      if(fbInfo->imageAttachments[i].barrier.oldLayout !=
         fbInfo->imageAttachments[i].barrier.newLayout)
      {
        VkImageMemoryBarrier barrier = fbInfo->imageAttachments[i].barrier;

        if(attachmentsInfo)
        {
          barrier.image = GetResourceManager()->GetCurrentHandle<VkImage>(att->baseResource);
          barrier.subresourceRange = att->viewRange;
        }

        barriers.push_back(barrier);
      }
    }

    record->cmdInfo->framebuffer = fb;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdNextSubpass2(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                const VkSubpassBeginInfo *pSubpassBeginInfo,
                                                const VkSubpassEndInfo *pSubpassEndInfo)
{
  SERIALISE_ELEMENT(commandBuffer).Unimportant();
  SERIALISE_ELEMENT_LOCAL(SubpassBegin, *pSubpassBeginInfo);
  SERIALISE_ELEMENT_LOCAL(SubpassEnd, *pSubpassEndInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSubpassBeginInfo unwrappedBeginInfo = SubpassBegin;
    VkSubpassEndInfo unwrappedEndInfo = SubpassEnd;

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedBeginInfo.pNext) +
                                  GetNextPatchSize(unwrappedEndInfo.pNext));

    UnwrapNextChain(m_State, "VkSubpassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedBeginInfo);
    UnwrapNextChain(m_State, "VkSubpassEndInfo", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      // don't do anything if we're executing a single draw, NextSubpass is meaningless (and invalid
      // on a partial render pass)
      if(InRerecordRange(m_LastCmdBufferID) && m_FirstEventID != m_LastEventID)
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        {
          GetCmdRenderState().subpass++;
          m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass++;
        }

        ActionFlags drawFlags =
            ActionFlags::PassBoundary | ActionFlags::BeginPass | ActionFlags::EndPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        ObjDisp(commandBuffer)
            ->CmdNextSubpass2(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdNextSubpass2 again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                             FindCommandQueueFamily(m_LastCmdBufferID),
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
      else if(IsRenderpassOpen(m_LastCmdBufferID) && m_FirstEventID != m_LastEventID)
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)
            ->CmdNextSubpass2(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass++;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].endBarriers.append(GetImplicitRenderPassBarriers());
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdNextSubpass2(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo);

      AddImplicitResolveResourceUsage();

      // track while reading, for fetching the right set of outputs in AddAction
      m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass++;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass++;

      rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers();

      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      ActionDescription action;
      action.customName = StringFormat::Fmt("vkCmdNextSubpass2() => %u",
                                            m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass);
      action.flags |= ActionFlags::PassBoundary | ActionFlags::BeginPass | ActionFlags::EndPass;

      AddAction(action);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdNextSubpass2(VkCommandBuffer commandBuffer,
                                      const VkSubpassBeginInfo *pSubpassBeginInfo,
                                      const VkSubpassEndInfo *pSubpassEndInfo)
{
  SCOPED_DBG_SINK();

  VkSubpassBeginInfo unwrappedBeginInfo = *pSubpassBeginInfo;
  VkSubpassEndInfo unwrappedEndInfo = *pSubpassEndInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedBeginInfo.pNext) +
                                GetNextPatchSize(unwrappedEndInfo.pNext));

  UnwrapNextChain(m_State, "VkSubpassBeginInfo", tempMem, (VkBaseInStructure *)&unwrappedBeginInfo);
  UnwrapNextChain(m_State, "VkSubpassEndInfo", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdNextSubpass2(Unwrap(commandBuffer), &unwrappedBeginInfo, &unwrappedEndInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdNextSubpass2);
    Serialise_vkCmdNextSubpass2(ser, commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndRenderPass2(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  const VkSubpassEndInfo *pSubpassEndInfo)
{
  SERIALISE_ELEMENT(commandBuffer).Unimportant();
  SERIALISE_ELEMENT_LOCAL(SubpassEnd, *pSubpassEndInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSubpassEndInfo unwrappedEndInfo = SubpassEnd;

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedEndInfo.pNext));

    UnwrapNextChain(m_State, "VkSubpassEndInfo", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderpassActive(m_LastCmdBufferID, false))
        {
          GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = false;
        }

        ResourceId currentRP = GetCmdRenderState().GetRenderPass();

        rdcarray<ResourceId> attachments;
        VkRect2D renderArea;
        const VulkanCreationInfo::RenderPass &rpinfo = m_CreationInfo.m_RenderPass[currentRP];

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          attachments = GetCmdRenderState().GetFramebufferAttachments();
          renderArea = GetCmdRenderState().renderArea;

          renderstate.SetRenderPass(ResourceId());
          renderstate.SetFramebuffer(ResourceId(), rdcarray<ResourceId>());
          renderstate.subpassContents = VK_SUBPASS_CONTENTS_MAX_ENUM;
        }

        ActionFlags drawFlags = ActionFlags::PassBoundary | ActionFlags::EndPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);
        ObjDisp(commandBuffer)->CmdEndRenderPass2(Unwrap(commandBuffer), &unwrappedEndInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdEndRenderPass2 again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest &&
           !m_FeedbackRPs.contains(currentRP))
        {
          for(size_t i = 0; i < attachments.size(); i++)
          {
            const VulkanCreationInfo::ImageView &viewInfo =
                m_CreationInfo.m_ImageView[attachments[i]];
            VkImage image = GetResourceManager()->GetCurrentHandle<VkImage>(viewInfo.image);

            if(rpinfo.attachments[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE &&
               rpinfo.attachments[i].used)
            {
              GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassStore,
                                                        image, rpinfo.attachments[i].finalLayout,
                                                        viewInfo.range, renderArea);
            }
          }
        }

        GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                             FindCommandQueueFamily(m_LastCmdBufferID),
                                             (uint32_t)imgBarriers.size(), imgBarriers.data());
      }
      else if(IsRenderpassOpen(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)->CmdEndRenderPass2(Unwrap(commandBuffer), &unwrappedEndInfo);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = false;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].endBarriers.append(GetImplicitRenderPassBarriers(~0U));

        VkSubpassFragmentDensityMapOffsetEndInfoQCOM *fragmentDensityOffsetStruct =
            (VkSubpassFragmentDensityMapOffsetEndInfoQCOM *)FindNextStruct(
                &unwrappedEndInfo,
                VK_STRUCTURE_TYPE_SUBPASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_QCOM);

        if(fragmentDensityOffsetStruct)
        {
          rdcarray<VkOffset2D> &stateOffsets = GetCmdRenderState().fragmentDensityMapOffsets;
          stateOffsets.resize(fragmentDensityOffsetStruct->fragmentDensityOffsetCount);
          for(uint32_t i = 0; i < fragmentDensityOffsetStruct->fragmentDensityOffsetCount; i++)
          {
            stateOffsets[i] = fragmentDensityOffsetStruct->pFragmentDensityOffsets[i];
          }
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdEndRenderPass2(Unwrap(commandBuffer), &unwrappedEndInfo);

      // fetch any queued indirect readbacks here
      for(const VkIndirectRecordData &indirectcopy :
          m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies)
        ExecuteIndirectReadback(commandBuffer, indirectcopy);

      rdcarray<VkImageMemoryBarrier> imgBarriers = GetImplicitRenderPassBarriers(~0U);

      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      AddEvent();
      ActionDescription action;
      action.customName =
          StringFormat::Fmt("vkCmdEndRenderPass2(%s)", MakeRenderPassOpString(true).c_str());
      action.flags |= ActionFlags::PassBoundary | ActionFlags::EndPass;

      AddAction(action);

      // track while reading, reset this to empty so AddAction sets no outputs,
      // but only AFTER the above AddAction (we want it grouped together)
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetRenderPass(ResourceId());
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.SetFramebuffer(ResourceId(),
                                                                   rdcarray<ResourceId>());
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndRenderPass2(VkCommandBuffer commandBuffer,
                                        const VkSubpassEndInfo *pSubpassEndInfo)
{
  SCOPED_DBG_SINK();

  VkSubpassEndInfo unwrappedEndInfo = *pSubpassEndInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedEndInfo.pNext));

  UnwrapNextChain(m_State, "VkSubpassEndInfo", tempMem, (VkBaseInStructure *)&unwrappedEndInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdEndRenderPass2(Unwrap(commandBuffer), &unwrappedEndInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndRenderPass2);
    Serialise_vkCmdEndRenderPass2(ser, commandBuffer, pSubpassEndInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    const rdcarray<VkImageMemoryBarrier> &barriers = record->cmdInfo->rpbarriers;

    // apply the implicit layout transitions here
    GetResourceManager()->RecordBarriers(record->cmdInfo->imageStates,
                                         record->pool->cmdPoolInfo->queueFamilyIndex,
                                         (uint32_t)barriers.size(), barriers.data());
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindPipeline(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkPipelineBindPoint pipelineBindPoint,
                                                VkPipeline pipeline)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(pipelineBindPoint);
  SERIALISE_ELEMENT(pipeline).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
          {
            renderstate.compute.pipeline = liveid;
            renderstate.compute.shaderObject = false;

            // disturb compute shader bound via vkCmdBindShadersEXT, if any
            renderstate.shaderObjects[(uint32_t)ShaderStage::Compute] = ResourceId();
          }
          else if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
          {
            renderstate.rt.pipeline = liveid;
          }
          else
          {
            renderstate.graphics.pipeline = liveid;
            renderstate.graphics.shaderObject = false;

            // disturb graphics shaders bound via vkCmdBindShadersEXT, if any
            for(uint32_t i = 0; i < (uint32_t)ShaderStage::Count; i++)
            {
              if(i == (uint32_t)ShaderStage::Compute)
                continue;
              renderstate.shaderObjects[i] = ResourceId();
            }

            const VulkanCreationInfo::Pipeline &pipeInfo = m_CreationInfo.m_Pipeline[liveid];

            // any static state from the pipeline invalidates any dynamic state previously bound
            for(uint32_t i = 0; i < VkDynamicCount; i++)
              renderstate.dynamicStates[i] &= pipeInfo.dynamicStates[i];

            if(!pipeInfo.dynamicStates[VkDynamicViewport] &&
               !pipeInfo.dynamicStates[VkDynamicViewportCount])
            {
              renderstate.views = pipeInfo.viewports;
            }
            if(!pipeInfo.dynamicStates[VkDynamicScissor] &&
               !pipeInfo.dynamicStates[VkDynamicScissorCount])
            {
              renderstate.scissors = pipeInfo.scissors;
            }

            if(!pipeInfo.dynamicStates[VkDynamicViewportCount])
            {
              renderstate.views.resize(RDCMIN(renderstate.views.size(), pipeInfo.viewports.size()));
            }
            if(!pipeInfo.dynamicStates[VkDynamicScissorCount])
            {
              renderstate.scissors.resize(
                  RDCMIN(renderstate.scissors.size(), pipeInfo.scissors.size()));
            }

            if(!pipeInfo.dynamicStates[VkDynamicLineWidth])
            {
              renderstate.lineWidth = pipeInfo.lineWidth;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthBias])
            {
              renderstate.bias.depth = pipeInfo.depthBiasConstantFactor;
              renderstate.bias.biasclamp = pipeInfo.depthBiasClamp;
              renderstate.bias.slope = pipeInfo.depthBiasSlopeFactor;
            }
            if(!pipeInfo.dynamicStates[VkDynamicBlendConstants])
            {
              memcpy(renderstate.blendConst, pipeInfo.blendConst, sizeof(float) * 4);
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthBounds])
            {
              renderstate.mindepth = pipeInfo.minDepthBounds;
              renderstate.maxdepth = pipeInfo.maxDepthBounds;
            }
            if(!pipeInfo.dynamicStates[VkDynamicStencilCompareMask])
            {
              renderstate.front.compare = pipeInfo.front.compareMask;
              renderstate.back.compare = pipeInfo.back.compareMask;
            }
            if(!pipeInfo.dynamicStates[VkDynamicStencilWriteMask])
            {
              renderstate.front.write = pipeInfo.front.writeMask;
              renderstate.back.write = pipeInfo.back.writeMask;
            }
            if(!pipeInfo.dynamicStates[VkDynamicStencilReference])
            {
              renderstate.front.ref = pipeInfo.front.reference;
              renderstate.back.ref = pipeInfo.back.reference;
            }
            if(!pipeInfo.dynamicStates[VkDynamicSampleLocationsEXT])
            {
              renderstate.sampleLocations.locations = pipeInfo.sampleLocations.locations;
              renderstate.sampleLocations.gridSize = pipeInfo.sampleLocations.gridSize;
              renderstate.sampleLocations.sampleCount = pipeInfo.rasterizationSamples;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDiscardRectangleEXT])
            {
              renderstate.discardRectangles = pipeInfo.discardRectangles;
            }
            if(!pipeInfo.dynamicStates[VkDynamicShadingRateKHR])
            {
              renderstate.pipelineShadingRate = pipeInfo.shadingRate;
              renderstate.shadingRateCombiners[0] = pipeInfo.shadingRateCombiners[0];
              renderstate.shadingRateCombiners[1] = pipeInfo.shadingRateCombiners[1];
            }
            if(!pipeInfo.dynamicStates[VkDynamicLineStippleKHR])
            {
              renderstate.stippleFactor = pipeInfo.stippleFactor;
              renderstate.stipplePattern = pipeInfo.stipplePattern;
            }
            if(!pipeInfo.dynamicStates[VkDynamicCullMode])
            {
              renderstate.cullMode = pipeInfo.cullMode;
            }
            if(!pipeInfo.dynamicStates[VkDynamicFrontFace])
            {
              renderstate.frontFace = pipeInfo.frontFace;
            }
            if(!pipeInfo.dynamicStates[VkDynamicPrimitiveTopology])
            {
              renderstate.primitiveTopology = pipeInfo.topology;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthTestEnable])
            {
              renderstate.depthTestEnable = pipeInfo.depthTestEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthWriteEnable])
            {
              renderstate.depthWriteEnable = pipeInfo.depthWriteEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthCompareOp])
            {
              renderstate.depthCompareOp = pipeInfo.depthCompareOp;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthBoundsTestEnable])
            {
              renderstate.depthBoundsTestEnable = pipeInfo.depthBoundsEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicStencilTestEnable])
            {
              renderstate.stencilTestEnable = pipeInfo.stencilTestEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicStencilOp])
            {
              renderstate.front.passOp = pipeInfo.front.passOp;
              renderstate.back.passOp = pipeInfo.back.passOp;

              renderstate.front.failOp = pipeInfo.front.failOp;
              renderstate.back.failOp = pipeInfo.back.failOp;

              renderstate.front.depthFailOp = pipeInfo.front.depthFailOp;
              renderstate.back.depthFailOp = pipeInfo.back.depthFailOp;

              renderstate.front.compareOp = pipeInfo.front.compareOp;
              renderstate.back.compareOp = pipeInfo.back.compareOp;
            }
            if(!pipeInfo.dynamicStates[VkDynamicVertexInputBindingStride])
            {
              for(const VulkanCreationInfo::Pipeline::VertBinding &bind : pipeInfo.vertexBindings)
              {
                renderstate.vbuffers.resize_for_index(bind.vbufferBinding);
                renderstate.vbuffers[bind.vbufferBinding].stride = bind.bytestride;
              }
            }
            if(!pipeInfo.dynamicStates[VkDynamicColorWriteEXT])
            {
              renderstate.colorWriteEnable.resize(pipeInfo.attachments.size());
              for(size_t i = 0; i < renderstate.colorWriteEnable.size(); i++)
                renderstate.colorWriteEnable[i] = pipeInfo.attachments[i].channelWriteMask != 0;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthBiasEnable])
            {
              renderstate.depthBiasEnable = pipeInfo.depthBiasEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicLogicOpEXT])
            {
              renderstate.logicOp = pipeInfo.logicOp;
            }
            if(!pipeInfo.dynamicStates[VkDynamicControlPointsEXT])
            {
              renderstate.patchControlPoints = pipeInfo.patchControlPoints;
            }
            if(!pipeInfo.dynamicStates[VkDynamicPrimRestart])
            {
              renderstate.primRestartEnable = pipeInfo.primitiveRestartEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicRastDiscard])
            {
              renderstate.rastDiscardEnable = pipeInfo.rasterizerDiscardEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicVertexInputEXT])
            {
              renderstate.vertexAttributes.resize(pipeInfo.vertexAttrs.size());
              for(size_t i = 0; i < renderstate.vertexAttributes.size(); i++)
              {
                renderstate.vertexAttributes[i].sType =
                    VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
                renderstate.vertexAttributes[i].pNext = NULL;
                renderstate.vertexAttributes[i].format = pipeInfo.vertexAttrs[i].format;
                renderstate.vertexAttributes[i].binding = pipeInfo.vertexAttrs[i].binding;
                renderstate.vertexAttributes[i].offset = pipeInfo.vertexAttrs[i].byteoffset;
                renderstate.vertexAttributes[i].location = pipeInfo.vertexAttrs[i].location;
              }
              renderstate.vertexBindings.resize(pipeInfo.vertexBindings.size());
              for(size_t i = 0; i < renderstate.vertexBindings.size(); i++)
              {
                renderstate.vertexBindings[i].sType =
                    VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
                renderstate.vertexBindings[i].pNext = NULL;
                renderstate.vertexBindings[i].binding = pipeInfo.vertexBindings[i].vbufferBinding;
                renderstate.vertexBindings[i].inputRate = pipeInfo.vertexBindings[i].perInstance
                                                              ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                              : VK_VERTEX_INPUT_RATE_VERTEX;
                renderstate.vertexBindings[i].stride = pipeInfo.vertexBindings[i].bytestride;
                renderstate.vertexBindings[i].divisor = pipeInfo.vertexBindings[i].instanceDivisor;
              }
            }
            if(!pipeInfo.dynamicStates[VkDynamicAttachmentFeedbackLoopEnableEXT])
            {
              renderstate.feedbackAspects = VK_IMAGE_ASPECT_NONE;
              if(pipeInfo.flags & VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
                renderstate.feedbackAspects |= VK_IMAGE_ASPECT_COLOR_BIT;
              if(pipeInfo.flags & VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
                renderstate.feedbackAspects |=
                    VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            if(!pipeInfo.dynamicStates[VkDynamicAlphaToCoverageEXT])
            {
              renderstate.alphaToCoverageEnable = pipeInfo.alphaToCoverageEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicAlphaToOneEXT])
            {
              renderstate.alphaToOneEnable = pipeInfo.alphaToOneEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicColorBlendEnableEXT])
            {
              renderstate.colorBlendEnable.resize(pipeInfo.attachments.size());
              for(size_t i = 0; i < renderstate.colorBlendEnable.size(); i++)
                renderstate.colorBlendEnable[i] = pipeInfo.attachments[i].blendEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicColorBlendEquationEXT])
            {
              renderstate.colorBlendEquation.resize(pipeInfo.attachments.size());
              for(size_t i = 0; i < renderstate.colorBlendEquation.size(); i++)
              {
                renderstate.colorBlendEquation[i].srcColorBlendFactor =
                    pipeInfo.attachments[i].blend.Source;
                renderstate.colorBlendEquation[i].dstColorBlendFactor =
                    pipeInfo.attachments[i].blend.Destination;
                renderstate.colorBlendEquation[i].colorBlendOp =
                    pipeInfo.attachments[i].blend.Operation;
                renderstate.colorBlendEquation[i].srcAlphaBlendFactor =
                    pipeInfo.attachments[i].alphaBlend.Source;
                renderstate.colorBlendEquation[i].dstAlphaBlendFactor =
                    pipeInfo.attachments[i].alphaBlend.Destination;
                renderstate.colorBlendEquation[i].alphaBlendOp =
                    pipeInfo.attachments[i].alphaBlend.Operation;
              }
            }
            if(!pipeInfo.dynamicStates[VkDynamicColorWriteMaskEXT])
            {
              renderstate.colorWriteMask.resize(pipeInfo.attachments.size());
              for(size_t i = 0; i < renderstate.colorWriteMask.size(); i++)
              {
                renderstate.colorWriteMask[i] = (uint32_t)pipeInfo.attachments[i].channelWriteMask;
              }
            }
            if(!pipeInfo.dynamicStates[VkDynamicConservativeRastModeEXT])
            {
              renderstate.conservativeRastMode = pipeInfo.conservativeRasterizationMode;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthClampEnableEXT])
            {
              renderstate.depthClampEnable = pipeInfo.depthClampEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthClipEnableEXT])
            {
              renderstate.depthClipEnable = pipeInfo.depthClipEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicDepthClipNegativeOneEXT])
            {
              renderstate.negativeOneToOne = pipeInfo.negativeOneToOne;
            }
            if(!pipeInfo.dynamicStates[VkDynamicOverstimationSizeEXT])
            {
              renderstate.primOverestimationSize = pipeInfo.extraPrimitiveOverestimationSize;
            }
            if(!pipeInfo.dynamicStates[VkDynamicLineRastModeEXT])
            {
              renderstate.lineRasterMode = pipeInfo.lineRasterMode;
            }
            if(!pipeInfo.dynamicStates[VkDynamicLineStippleEnableEXT])
            {
              renderstate.stippledLineEnable = pipeInfo.stippleEnabled;
            }
            if(!pipeInfo.dynamicStates[VkDynamicLogicOpEnableEXT])
            {
              renderstate.logicOpEnable = pipeInfo.logicOpEnable;
            }
            if(!pipeInfo.dynamicStates[VkDynamicPolygonModeEXT])
            {
              renderstate.polygonMode = pipeInfo.polygonMode;
            }
            if(!pipeInfo.dynamicStates[VkDynamicProvokingVertexModeEXT])
            {
              renderstate.provokingVertexMode = pipeInfo.provokingVertex;
            }
            if(!pipeInfo.dynamicStates[VkDynamicRasterizationSamplesEXT])
            {
              renderstate.rastSamples = pipeInfo.rasterizationSamples;
            }
            if(!pipeInfo.dynamicStates[VkDynamicRasterizationStreamEXT])
            {
              renderstate.rasterStream = pipeInfo.rasterizationStream;
            }
            if(!pipeInfo.dynamicStates[VkDynamicSampleLocationsEnableEXT])
            {
              renderstate.sampleLocEnable = pipeInfo.sampleLocations.enabled;
            }
            if(!pipeInfo.dynamicStates[VkDynamicSampleMaskEXT])
            {
              renderstate.sampleMask[0] = pipeInfo.sampleMask;
            }
            if(!pipeInfo.dynamicStates[VkDynamicTessDomainOriginEXT])
            {
              renderstate.domainOrigin = pipeInfo.tessellationDomainOrigin;
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
      ResourceId liveid = GetResID(pipeline);

      // track while reading, as we need to bind current topology & index byte width in AddAction
      if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
      {
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.compute.pipeline = liveid;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.compute.shaderObject = false;
      }
      else if(pipelineBindPoint == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
      {
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.rt.pipeline = liveid;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.rt.shaderObject = false;
      }
      else
      {
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphics.pipeline = liveid;
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphics.shaderObject = false;

        const VulkanCreationInfo::Pipeline &pipeInfo = m_CreationInfo.m_Pipeline[liveid];

        if(!pipeInfo.dynamicStates[VkDynamicPrimitiveTopology])
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.primitiveTopology = pipeInfo.topology;
        }
      }
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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
  SERIALISE_ELEMENT(firstSet).Important();
  SERIALISE_ELEMENT(setCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorSets, setCount).Important();
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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          VulkanStatePipeline &pipeline = renderstate.GetPipeline(pipelineBindPoint);
          rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descsets = pipeline.descSets;

          // expand as necessary
          if(descsets.size() < firstSet + setCount)
            descsets.resize(firstSet + setCount);

          pipeline.lastBoundSet = firstSet;

          const rdcarray<ResourceId> &descSetLayouts =
              m_CreationInfo.m_PipelineLayout[GetResID(layout)].descSetLayouts;

          const uint32_t *offsIter = pDynamicOffsets;
          uint32_t dynConsumed = 0;

          // consume the offsets linearly along the descriptor set layouts
          for(uint32_t i = 0; i < setCount; i++)
          {
            descsets[firstSet + i].pipeLayout = GetResID(layout);
            descsets[firstSet + i].descSet = GetResID(pDescriptorSets[i]);
            descsets[firstSet + i].offsets.clear();

            if(descSetLayouts[firstSet + i] == ResourceId())
              continue;

            uint32_t dynCount =
                m_CreationInfo.m_DescSetLayout[descSetLayouts[firstSet + i]].dynamicCount;
            descsets[firstSet + i].offsets.assign(offsIter, dynCount);
            offsIter += dynCount;
            dynConsumed += dynCount;
            RDCASSERT(dynConsumed <= dynamicOffsetCount);
          }
        }
      }
    }
    else
    {
      // track while reading, as we need to track resource usage
      rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descsets =
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetPipeline(pipelineBindPoint).descSets;

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
    for(uint32_t i = 0; i < setCount; i++)
    {
      if(pDescriptorSets[i] != VK_NULL_HANDLE)
        record->cmdInfo->boundDescSets.insert(
            {GetResID(pDescriptorSets[i]), GetRecord(pDescriptorSets[i])});
    }
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
  SERIALISE_ELEMENT(firstBinding).Important();
  SERIALISE_ELEMENT(bindingCount);
  SERIALISE_ELEMENT_ARRAY(pBuffers, bindingCount).Important();
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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          if(renderstate.vbuffers.size() < firstBinding + bindingCount)
            renderstate.vbuffers.resize(firstBinding + bindingCount);

          for(uint32_t i = 0; i < bindingCount; i++)
          {
            renderstate.vbuffers[firstBinding + i].buf = GetResID(pBuffers[i]);
            renderstate.vbuffers[firstBinding + i].offs = pOffsets[i];
            renderstate.vbuffers[firstBinding + i].size = VK_WHOLE_SIZE;
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
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.vbuffers[firstBinding + i].buf =
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < bindingCount; i++)
    {
      // binding NULL is legal with robustness2
      if(pBuffers[i] != VK_NULL_HANDLE)
        record->MarkBufferFrameReferenced(GetRecord(pBuffers[i]), pOffsets[i], VK_WHOLE_SIZE,
                                          eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindVertexBuffers2(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBinding,
    uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets,
    const VkDeviceSize *pSizes, const VkDeviceSize *pStrides)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBinding).Important();
  SERIALISE_ELEMENT(bindingCount);
  SERIALISE_ELEMENT_ARRAY(pBuffers, bindingCount).Important();
  SERIALISE_ELEMENT_ARRAY(pOffsets, bindingCount).OffsetOrSize();
  SERIALISE_ELEMENT_ARRAY(pSizes, bindingCount).OffsetOrSize();
  SERIALISE_ELEMENT_ARRAY(pStrides, bindingCount).OffsetOrSize();

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
            ->CmdBindVertexBuffers2(Unwrap(commandBuffer), firstBinding, bindingCount,
                                    UnwrapArray(pBuffers, bindingCount), pOffsets, pSizes, pStrides);

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          if(renderstate.vbuffers.size() < firstBinding + bindingCount)
            renderstate.vbuffers.resize(firstBinding + bindingCount);

          for(uint32_t i = 0; i < bindingCount; i++)
          {
            renderstate.vbuffers[firstBinding + i].buf = GetResID(pBuffers[i]);
            renderstate.vbuffers[firstBinding + i].offs = pOffsets[i];
            renderstate.vbuffers[firstBinding + i].size = pSizes ? pSizes[i] : VK_WHOLE_SIZE;

            // if strides is NULL the pipeline bound must have had no dynamic state for stride and
            // so stride was filled out then, we leave it as-is.
            if(pStrides)
            {
              renderstate.dynamicStates[VkDynamicVertexInputBindingStride] = true;

              renderstate.vbuffers[firstBinding + i].stride = pStrides[i];

              // if we have dynamic vertex input data, update the strides. If we don't have any
              // that's fine we can skip this, it means the application must provide a later
              // vkCmdSetVertexInput which overrides anything we'd set here
              if(firstBinding + i < renderstate.vertexBindings.size())
                renderstate.vertexBindings[firstBinding + i].stride = (uint32_t)pStrides[i];
            }
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
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.vbuffers[firstBinding + i].buf =
            GetResID(pBuffers[i]);

      ObjDisp(commandBuffer)
          ->CmdBindVertexBuffers2(Unwrap(commandBuffer), firstBinding, bindingCount,
                                  UnwrapArray(pBuffers, bindingCount), pOffsets, pSizes, pStrides);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding,
                                            uint32_t bindingCount, const VkBuffer *pBuffers,
                                            const VkDeviceSize *pOffsets,
                                            const VkDeviceSize *pSizes, const VkDeviceSize *pStrides)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBindVertexBuffers2(Unwrap(commandBuffer), firstBinding, bindingCount,
                                                  UnwrapArray(pBuffers, bindingCount), pOffsets,
                                                  pSizes, pStrides));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindVertexBuffers2);
    Serialise_vkCmdBindVertexBuffers2(ser, commandBuffer, firstBinding, bindingCount, pBuffers,
                                      pOffsets, pSizes, pStrides);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < bindingCount; i++)
    {
      // binding NULL is legal with robustness2
      if(pBuffers[i] != VK_NULL_HANDLE)
        record->MarkBufferFrameReferenced(GetRecord(pBuffers[i]), pOffsets[i],
                                          pSizes ? pSizes[i] : VK_WHOLE_SIZE, eFrameRef_Read);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindIndexBuffer(SerialiserType &ser,
                                                   VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                   VkDeviceSize offset, VkIndexType indexType)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset).OffsetOrSize();
  SERIALISE_ELEMENT(indexType).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.ibuffer.buf = GetResID(buffer);
          renderstate.ibuffer.offs = offset;

          if(indexType == VK_INDEX_TYPE_UINT32)
            renderstate.ibuffer.bytewidth = 4;
          else if(indexType == VK_INDEX_TYPE_UINT8_KHR)
            renderstate.ibuffer.bytewidth = 1;
          else
            renderstate.ibuffer.bytewidth = 2;
        }
      }
    }
    else
    {
      // track while reading, as we need to bind current topology & index byte width in AddAction
      if(indexType == VK_INDEX_TYPE_UINT32)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.ibuffer.bytewidth = 4;
      else if(indexType == VK_INDEX_TYPE_UINT8_KHR)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.ibuffer.bytewidth = 1;
      else
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.ibuffer.bytewidth = 2;

      // track while reading, as we need to track resource usage
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.ibuffer.buf = GetResID(buffer);

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkBufferFrameReferenced(GetRecord(buffer), 0, VK_WHOLE_SIZE, eFrameRef_Read);
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
  SERIALISE_ELEMENT_TYPED(VkShaderStageFlagBits, stageFlags).TypedAs("VkShaderStageFlags"_lit).Important();
  SERIALISE_ELEMENT(start);
  SERIALISE_ELEMENT(length);
  SERIALISE_ELEMENT_ARRAY(values, length).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          RDCASSERT(start + length < (uint32_t)ARRAY_COUNT(renderstate.pushconsts));

          memcpy(renderstate.pushconsts + start, values, length);

          renderstate.pushConstSize = RDCMAX(renderstate.pushConstSize, start + length);
          renderstate.pushLayout = GetResID(layout);

          m_PushCommandBuffer = m_LastCmdBufferID;
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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
  if(memoryBarrierCount > 0)
    ser.Important();
  SERIALISE_ELEMENT_ARRAY(pMemoryBarriers, memoryBarrierCount);
  SERIALISE_ELEMENT(bufferMemoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pBufferMemoryBarriers, bufferMemoryBarrierCount);
  if(bufferMemoryBarrierCount > 0)
    ser.Important();
  SERIALISE_ELEMENT(imageMemoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pImageMemoryBarriers, imageMemoryBarrierCount);
  if(imageMemoryBarrierCount > 0)
    ser.Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  rdcarray<VkImageMemoryBarrier> imgBarriers;
  rdcarray<VkBufferMemoryBarrier> bufBarriers;

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
    else
    {
      for(uint32_t i = 0; i < imageMemoryBarrierCount; i++)
      {
        const VkImageMemoryBarrier &b = pImageMemoryBarriers[i];
        if(b.image != VK_NULL_HANDLE && b.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              GetResID(b.image), EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID,
                                            ResourceUsage::Discard)));
        }
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      // now sanitise layouts before passing to vulkan
      for(VkImageMemoryBarrier &barrier : imgBarriers)
      {
        SanitiseOldImageLayout(barrier.oldLayout);
        SanitiseReplayImageLayout(barrier.newLayout);
      }

      ObjDisp(commandBuffer)
          ->CmdPipelineBarrier(Unwrap(commandBuffer), srcStageMask, destStageMask, dependencyFlags,
                               memoryBarrierCount, pMemoryBarriers, (uint32_t)bufBarriers.size(),
                               bufBarriers.data(), (uint32_t)imgBarriers.size(), imgBarriers.data());

      if(IsActiveReplaying(m_State) &&
         m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
      {
        for(uint32_t i = 0; i < imageMemoryBarrierCount; i++)
        {
          const VkImageMemoryBarrier &b = pImageMemoryBarriers[i];
          if(b.image != VK_NULL_HANDLE && b.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
          {
            VkImageLayout newLayout = b.newLayout;
            SanitiseNewImageLayout(newLayout);
            GetDebugManager()->FillWithDiscardPattern(
                commandBuffer, DiscardType::UndefinedTransition, b.image, newLayout,
                b.subresourceRange, {{0, 0}, {65536, 65536}});
          }
        }
      }
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    if(imageMemoryBarrierCount > 0)
    {
      GetResourceManager()->RecordBarriers(record->cmdInfo->imageStates,
                                           record->pool->cmdPoolInfo->queueFamilyIndex,
                                           imageMemoryBarrierCount, pImageMemoryBarriers);
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
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(query).Important();

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdPipelineBarrier2(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer,
                                                    const VkDependencyInfo *pDependencyInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(DependencyInfo, *pDependencyInfo).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  rdcarray<VkImageMemoryBarrier2> imgBarriers;
  rdcarray<VkBufferMemoryBarrier2> bufBarriers;

  // it's possible for buffer or image to be NULL if it refers to a resource that is otherwise
  // not in the log (barriers do not mark resources referenced). If the resource in question does
  // not exist, then it's safe to skip this barrier.
  //
  // Since it's a convenient place, we unwrap at the same time.
  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    for(uint32_t i = 0; i < DependencyInfo.bufferMemoryBarrierCount; i++)
    {
      if(DependencyInfo.pBufferMemoryBarriers[i].buffer != VK_NULL_HANDLE)
      {
        bufBarriers.push_back(DependencyInfo.pBufferMemoryBarriers[i]);
        bufBarriers.back().buffer = Unwrap(bufBarriers.back().buffer);

        RemapQueueFamilyIndices(bufBarriers.back().srcQueueFamilyIndex,
                                bufBarriers.back().dstQueueFamilyIndex);

        if(IsLoading(m_State))
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              GetResID(DependencyInfo.pBufferMemoryBarriers[i].buffer),
              EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::Barrier)));
        }
      }
    }

    for(uint32_t i = 0; i < DependencyInfo.imageMemoryBarrierCount; i++)
    {
      if(DependencyInfo.pImageMemoryBarriers[i].image != VK_NULL_HANDLE)
      {
        imgBarriers.push_back(DependencyInfo.pImageMemoryBarriers[i]);
        imgBarriers.back().image = Unwrap(imgBarriers.back().image);

        RemapQueueFamilyIndices(imgBarriers.back().srcQueueFamilyIndex,
                                imgBarriers.back().dstQueueFamilyIndex);

        if(IsLoading(m_State))
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              GetResID(DependencyInfo.pImageMemoryBarriers[i].image),
              EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID, ResourceUsage::Barrier)));
        }
      }
    }

    VkDependencyInfo UnwrappedDependencyInfo = DependencyInfo;

    UnwrappedDependencyInfo.pBufferMemoryBarriers = bufBarriers.data();
    UnwrappedDependencyInfo.bufferMemoryBarrierCount = (uint32_t)bufBarriers.size();
    UnwrappedDependencyInfo.pImageMemoryBarriers = imgBarriers.data();
    UnwrappedDependencyInfo.imageMemoryBarrierCount = (uint32_t)imgBarriers.size();

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }
    else
    {
      for(uint32_t i = 0; i < DependencyInfo.imageMemoryBarrierCount; i++)
      {
        const VkImageMemoryBarrier2 &b = DependencyInfo.pImageMemoryBarriers[i];
        if(b.image != VK_NULL_HANDLE && b.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
           b.newLayout != VK_IMAGE_LAYOUT_UNDEFINED)
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              GetResID(b.image), EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID,
                                            ResourceUsage::Discard)));
        }
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                           FindCommandQueueFamily(m_LastCmdBufferID),
                                           (uint32_t)imgBarriers.size(), imgBarriers.data());

      // now sanitise layouts before passing to vulkan
      for(VkImageMemoryBarrier2 &barrier : imgBarriers)
      {
        if(barrier.oldLayout == barrier.newLayout)
        {
          barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          continue;
        }

        SanitiseOldImageLayout(barrier.oldLayout);
        SanitiseReplayImageLayout(barrier.newLayout);
      }

      ObjDisp(commandBuffer)->CmdPipelineBarrier2(Unwrap(commandBuffer), &UnwrappedDependencyInfo);

      if(IsActiveReplaying(m_State) &&
         m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
      {
        for(uint32_t i = 0; i < DependencyInfo.imageMemoryBarrierCount; i++)
        {
          const VkImageMemoryBarrier2 &b = DependencyInfo.pImageMemoryBarriers[i];
          if(b.image != VK_NULL_HANDLE && b.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             b.newLayout != VK_IMAGE_LAYOUT_UNDEFINED)
          {
            GetDebugManager()->FillWithDiscardPattern(
                commandBuffer, DiscardType::UndefinedTransition, b.image, b.newLayout,
                b.subresourceRange, {{0, 0}, {65536, 65536}});
          }
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer,
                                          const VkDependencyInfo *pDependencyInfo)
{
  SCOPED_DBG_SINK();

  byte *tempMem = GetTempMemory(GetNextPatchSize(pDependencyInfo));
  VkDependencyInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pDependencyInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdPipelineBarrier2(Unwrap(commandBuffer), unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdPipelineBarrier2);
    Serialise_vkCmdPipelineBarrier2(ser, commandBuffer, pDependencyInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    if(pDependencyInfo->imageMemoryBarrierCount > 0)
    {
      GetResourceManager()->RecordBarriers(
          record->cmdInfo->imageStates, record->pool->cmdPoolInfo->queueFamilyIndex,
          pDependencyInfo->imageMemoryBarrierCount, pDependencyInfo->pImageMemoryBarriers);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdWriteTimestamp2(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   VkPipelineStageFlags2 stage,
                                                   VkQueryPool queryPool, uint32_t query)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits2, stage).TypedAs("VkPipelineStageFlags2"_lit);
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(query).Important();

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
      ObjDisp(commandBuffer)->CmdWriteTimestamp2(Unwrap(commandBuffer), stage, Unwrap(queryPool), query);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage,
                                         VkQueryPool queryPool, uint32_t query)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdWriteTimestamp2(Unwrap(commandBuffer), stage, Unwrap(queryPool), query));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdWriteTimestamp2);
    Serialise_vkCmdWriteTimestamp2(ser, commandBuffer, stage, queryPool, query);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

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
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(firstQuery);
  SERIALISE_ELEMENT(queryCount);
  SERIALISE_ELEMENT(destBuffer).Important();
  SERIALISE_ELEMENT(destOffset).OffsetOrSize();
  SERIALISE_ELEMENT(destStride).OffsetOrSize();
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

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
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(query).Important();
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndQuery(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                            VkQueryPool queryPool, uint32_t query)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(query).Important();

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdResetQueryPool(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  VkQueryPool queryPool, uint32_t firstQuery,
                                                  uint32_t queryCount)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool).Important();
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

      m_ResetQueries.push_back({queryPool, firstQuery, queryCount});
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(queryPool), eFrameRef_Read);
  }
}

void WrappedVulkan::UpdateRenderStateForSecondaries(BakedCmdBufferInfo &ancestorCB,
                                                    BakedCmdBufferInfo &currentCB)
{
  currentCB.state.SetRenderPass(ancestorCB.state.GetRenderPass());
  currentCB.state.subpass = ancestorCB.state.subpass;
  currentCB.state.dynamicRendering = ancestorCB.state.dynamicRendering;
  currentCB.state.SetFramebuffer(ancestorCB.state.GetFramebuffer(),
                                 ancestorCB.state.GetFramebufferAttachments());
  currentCB.state.renderArea = ancestorCB.state.renderArea;
  currentCB.state.subpassContents = ancestorCB.state.subpassContents;

  if(currentCB.action)
  {
    for(const ResourceId &childCB : currentCB.action->executedCmds)
      UpdateRenderStateForSecondaries(ancestorCB, m_BakedCmdBufferInfo[childCB]);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdExecuteCommands(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   uint32_t commandBufferCount,
                                                   const VkCommandBuffer *pCommandBuffers)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(commandBufferCount);
  SERIALISE_ELEMENT_ARRAY(pCommandBuffers, commandBufferCount).Important();

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

      // append deferred indirect copies and merge barriers into parent command buffer
      {
        BakedCmdBufferInfo &dst = m_BakedCmdBufferInfo[m_LastCmdBufferID];

        for(uint32_t i = 0; i < commandBufferCount; i++)
        {
          // indirectCopies are stored in m_BakedCmdBufferInfo[m_LastCmdBufferID] which is an
          // original ID
          ResourceId origSecondId = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[i]));
          BakedCmdBufferInfo &src = m_BakedCmdBufferInfo[origSecondId];

          dst.indirectCopies.append(src.indirectCopies);

          ImageState::Merge(dst.imageStates, src.imageStates, GetImageTransitionInfo());
        }
      }

      AddEvent();

      ActionDescription action;
      action.customName = StringFormat::Fmt("vkCmdExecuteCommands(%u)", commandBufferCount);
      action.flags = ActionFlags::CmdList | ActionFlags::PushMarker;

      AddAction(action);

      BakedCmdBufferInfo &parentCmdBufInfo = m_BakedCmdBufferInfo[m_LastCmdBufferID];

      parentCmdBufInfo.curEventID++;

      // should we add framebuffer usage to the child draws.
      bool framebufferUsage = parentCmdBufInfo.state.GetRenderPass() != ResourceId() &&
                              parentCmdBufInfo.state.GetFramebuffer() != ResourceId();

      for(uint32_t c = 0; c < commandBufferCount; c++)
      {
        ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[c]));

        BakedCmdBufferInfo &cmdBufInfo = m_BakedCmdBufferInfo[cmd];

        // add a fake marker
        ActionDescription marker;
        marker.customName = StringFormat::Fmt(
            "=> vkCmdExecuteCommands()[%u]: vkBeginCommandBuffer(%s)", c, ToStr(cmd).c_str());
        marker.flags =
            ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary | ActionFlags::BeginPass;
        AddEvent();

        parentCmdBufInfo.curEvents.back().chunkIndex = cmdBufInfo.beginChunk;

        AddAction(marker);
        parentCmdBufInfo.curEventID++;

        if(m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetRenderPass() == ResourceId() &&
           !m_BakedCmdBufferInfo[m_LastCmdBufferID].state.dynamicRendering.active &&
           (cmdBufInfo.beginFlags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT))
        {
          AddDebugMessage(
              MessageCategory::Execution, MessageSeverity::High, MessageSource::IncorrectAPIUse,
              "Executing a command buffer with RENDER_PASS_CONTINUE_BIT outside of render pass");
        }

        // insert the baked command buffer in-line into this list of notes, assigning new event and
        // drawIDs
        parentCmdBufInfo.action->InsertAndUpdateIDs(*cmdBufInfo.action, parentCmdBufInfo.curEventID,
                                                    parentCmdBufInfo.actionCount);

        if(framebufferUsage)
        {
          size_t total = parentCmdBufInfo.action->children.size();
          size_t numChildren = cmdBufInfo.action->children.size();

          // iterate through the newly added draws, and recursively add usage to them using our
          // primary command buffer's state
          for(size_t i = 0; i < numChildren; i++)
          {
            AddFramebufferUsageAllChildren(
                parentCmdBufInfo.action->children[total - numChildren + i], parentCmdBufInfo.state);
          }
        }

        for(size_t i = 0; i < cmdBufInfo.debugMessages.size(); i++)
        {
          parentCmdBufInfo.debugMessages.push_back(cmdBufInfo.debugMessages[i]);
          parentCmdBufInfo.debugMessages.back().eventId += parentCmdBufInfo.curEventID;
        }

        // Record execution of the secondary command buffer in the parent's CommandBufferNode
        // Only primary command buffers can be submitted
        CommandBufferExecuteInfo execInfo;
        execInfo.cmdId = cmd;
        execInfo.relPos = parentCmdBufInfo.curEventID;

        m_CommandBufferExecutes[m_LastCmdBufferID].push_back(execInfo);

        parentCmdBufInfo.action->executedCmds.push_back(cmd);

        parentCmdBufInfo.curEventID += cmdBufInfo.eventCount;
        parentCmdBufInfo.actionCount += cmdBufInfo.actionCount;

        // pull in any remaining events on the command buffer that weren't added to an action
        uint32_t i = 0;
        for(APIEvent &apievent : cmdBufInfo.curEvents)
        {
          apievent.eventId = parentCmdBufInfo.curEventID - cmdBufInfo.curEvents.count() + i;

          parentCmdBufInfo.curEvents.push_back(apievent);
          i++;
        }

        cmdBufInfo.curEvents.clear();

        marker.customName = StringFormat::Fmt(
            "=> vkCmdExecuteCommands()[%u]: vkEndCommandBuffer(%s)", c, ToStr(cmd).c_str());
        marker.flags =
            ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary | ActionFlags::EndPass;
        AddEvent();
        AddAction(marker);
        parentCmdBufInfo.curEventID++;
      }

      // add an extra pop marker
      AddEvent();
      action = ActionDescription();
      action.flags = ActionFlags::PopMarker;

      AddAction(action);

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
        // for the markers we've added, which have this file offset, so just skip all of our work.
        if(m_FirstEventID > 1 && m_FirstEventID + 1 < m_LastEventID)
          return true;

        // account for the execute commands event
        parentCmdBufInfo.curEventID++;

        bool fullRecord = false;
        uint32_t startEID = parentCmdBufInfo.curEventID;
        if(IsCommandBufferPartial(m_LastCmdBufferID))
          startEID += GetCommandBufferPartialSubmission(m_LastCmdBufferID)->beginEvent;

        // if we're in the re-record range and this command buffer isn't partial, we execute all
        // command buffers because m_Partial[Primary].baseEvent above is only valid for the partial
        // command buffer
        if(!IsCommandBufferPartial(m_LastCmdBufferID))
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Fully re-recording non-partial execute in command buffer %s for %s",
                   ToStr(GetResID(commandBuffer)).c_str(), ToStr(m_LastCmdBufferID).c_str());
#endif
          fullRecord = true;
        }

        // advance m_CurEventID to match the events added when reading
        for(uint32_t c = 0; c < commandBufferCount; c++)
        {
          ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[c]));

          // propagate renderpass state if active. If it's inactive the renderpass might be
          // activated inside the secondary which we should not overwrite.
          if(parentCmdBufInfo.state.ActiveRenderPass())
          {
            UpdateRenderStateForSecondaries(parentCmdBufInfo, m_BakedCmdBufferInfo[cmd]);
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
        else if(m_LastEventID <= startEID && !fullRecord)
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

          rdcarray<VkCommandBuffer> rerecordedCmds;

          for(uint32_t c = 0; c < commandBufferCount; c++)
          {
            ResourceId cmdid = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[c]));

            // account for the virtual vkBeginCommandBuffer label at the start of the events here
            // so it matches up to baseEvent
            eid++;

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            uint32_t end = eid + m_BakedCmdBufferInfo[cmdid].eventCount;
#endif

            if(eid <= m_LastEventID || fullRecord)
            {
              VkCommandBuffer cmd = RerecordCmdBuf(cmdid);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              ResourceId rerecord = GetResID(cmd);
              RDCDEBUG("ExecuteCommands re-recorded replay of %s, using %s (%u -> %u <= %u)",
                       ToStr(cmdid).c_str(), ToStr(rerecord).c_str(), eid, end, m_LastEventID);
#endif
              rerecordedCmds.push_back(Unwrap(cmd));

              ImageState::Merge(m_BakedCmdBufferInfo[m_LastCmdBufferID].imageStates,
                                m_BakedCmdBufferInfo[cmdid].imageStates, GetImageTransitionInfo());
            }
            else
            {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("not executing %s", ToStr(cmdid).c_str());
#endif
            }

            // 1 extra to account for the virtual end command buffer label (begin is accounted for
            // above)
            eid += 1 + m_BakedCmdBufferInfo[cmdid].eventCount;
          }

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("executing %zu commands in %s", rerecordedCmds.size(),
                   ToStr(GetResID(commandBuffer)).c_str());
#endif

          if(!rerecordedCmds.empty())
          {
            if(m_ActionCallback && m_ActionCallback->SplitSecondary())
            {
              ActionUse use(m_CurChunkOffset, 0);
              auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);
              if(it != m_ActionUses.end())
              {
                uint32_t eventId = it->eventId + 2;

                for(uint32_t i = 0; i < (uint32_t)rerecordedCmds.size(); i++)
                {
                  ResourceId cmd = GetResourceManager()->GetOriginalID(GetResID(pCommandBuffers[i]));
                  BakedCmdBufferInfo &info = m_BakedCmdBufferInfo[cmd];
                  if(info.action && info.action->children.size() > 0)
                  {
                    uint32_t firstEventId = eventId + info.action->children.front().action.eventId;
                    uint32_t lastEventId = eventId + info.action->children.back().action.eventId;
                    m_ActionCallback->PreCmdExecute(eventId, firstEventId, lastEventId,
                                                    commandBuffer);
                    ObjDisp(commandBuffer)
                        ->CmdExecuteCommands(Unwrap(commandBuffer), 1, &rerecordedCmds[i]);
                    m_ActionCallback->PostCmdExecute(eventId, firstEventId, lastEventId,
                                                     commandBuffer);
                  }
                  else
                  {
                    ObjDisp(commandBuffer)
                        ->CmdExecuteCommands(Unwrap(commandBuffer), 1, &rerecordedCmds[i]);
                  }

                  eventId += 2 + m_BakedCmdBufferInfo[cmd].eventCount;
                }
              }
            }
            else
            {
              ObjDisp(commandBuffer)
                  ->CmdExecuteCommands(Unwrap(commandBuffer), (uint32_t)rerecordedCmds.size(),
                                       rerecordedCmds.data());
            }
          }
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdExecuteCommands);
    Serialise_vkCmdExecuteCommands(ser, commandBuffer, commandBufferCount, pCommandBuffers);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < commandBufferCount; i++)
    {
      VkResourceRecord *execRecord = GetRecord(pCommandBuffers[i]);
      if(execRecord->bakedCommands)
      {
        record->cmdInfo->boundDescSets.insert(
            execRecord->bakedCommands->cmdInfo->boundDescSets.begin(),
            execRecord->bakedCommands->cmdInfo->boundDescSets.end());
        record->cmdInfo->subcmds.push_back(execRecord);

        if(Vulkan_Debug_VerboseCommandRecording())
        {
          RDCLOG("Execute command buffer %s (baked was %s) in %s (baked to %s)",
                 ToStr(execRecord->GetResourceID()).c_str(),
                 ToStr(execRecord->bakedCommands->GetResourceID()).c_str(),
                 ToStr(record->GetResourceID()).c_str(),
                 ToStr(record->bakedCommands ? record->bakedCommands->GetResourceID() : ResourceId())
                     .c_str());
        }

        ImageState::Merge(record->cmdInfo->imageStates,
                          execRecord->bakedCommands->cmdInfo->imageStates, GetImageTransitionInfo());
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
  SERIALISE_ELEMENT_LOCAL(Marker, *pMarker).Named("pMarker"_lit).Important();

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

      ActionDescription action;
      action.customName = Marker.pMarkerName ? Marker.pMarkerName : "";
      action.flags |= ActionFlags::PushMarker;

      action.markerColor.x = RDCCLAMP(Marker.color[0], 0.0f, 1.0f);
      action.markerColor.y = RDCCLAMP(Marker.color[1], 0.0f, 1.0f);
      action.markerColor.z = RDCCLAMP(Marker.color[2], 0.0f, 1.0f);
      action.markerColor.w = RDCCLAMP(Marker.color[3], 0.0f, 1.0f);

      AddEvent();
      AddAction(action);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDebugMarkerBeginEXT);
    Serialise_vkCmdDebugMarkerBeginEXT(ser, commandBuffer, pMarker);

    if(Vulkan_Debug_VerboseCommandRecording())
    {
      RDCLOG(
          "Begin marker %s in %s (baked to %s)", pMarker->pMarkerName,
          ToStr(record->GetResourceID()).c_str(),
          ToStr(record->bakedCommands ? record->bakedCommands->GetResourceID() : ResourceId()).c_str());
    }

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDebugMarkerEndEXT(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer).Unimportant();

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

      // dummy action that is consumed when this command buffer
      // is being in-lined into the call stream
      ActionDescription action;
      action.flags = ActionFlags::PopMarker;

      AddEvent();
      AddAction(action);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDebugMarkerEndEXT);
    Serialise_vkCmdDebugMarkerEndEXT(ser, commandBuffer);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDebugMarkerInsertEXT(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        const VkDebugMarkerMarkerInfoEXT *pMarker)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Marker, *pMarker).Named("pMarker"_lit).Important();

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

      ActionDescription action;
      action.customName = Marker.pMarkerName ? Marker.pMarkerName : "";
      action.flags |= ActionFlags::SetMarker;

      action.markerColor.x = RDCCLAMP(Marker.color[0], 0.0f, 1.0f);
      action.markerColor.y = RDCCLAMP(Marker.color[1], 0.0f, 1.0f);
      action.markerColor.z = RDCCLAMP(Marker.color[2], 0.0f, 1.0f);
      action.markerColor.w = RDCCLAMP(Marker.color[3], 0.0f, 1.0f);

      AddEvent();
      AddAction(action);
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

  if(pMarker)
    HandleFrameMarkers(pMarker->pMarkerName, commandBuffer);
  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDebugMarkerInsertEXT);
    Serialise_vkCmdDebugMarkerInsertEXT(ser, commandBuffer, pMarker);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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

  const rdcarray<ResourceId> &descSetLayouts = pipeLayoutInfo.descSetLayouts;

  const DescSetLayout &desclayout = m_CreationInfo.m_DescSetLayout[descSetLayouts[set]];

  rdcarray<DescriptorSetSlot *> &bindings = m_DescriptorSetState[setId].data.binds;
  bytebuf &inlineData = m_DescriptorSetState[setId].data.inlineBytes;
  ResourceId prevLayout = m_DescriptorSetState[setId].layout;

  if(prevLayout == ResourceId())
  {
    // push descriptors can't have variable count, so just pass 0
    desclayout.CreateBindingsArray(m_DescriptorSetState[setId].data, 0);
  }
  else if(prevLayout != descSetLayouts[set])
  {
    desclayout.UpdateBindingsArray(m_CreationInfo.m_DescSetLayout[prevLayout],
                                   m_DescriptorSetState[setId].data);
  }

  m_DescriptorSetState[setId].layout = descSetLayouts[set];

  // update our local tracking
  for(uint32_t i = 0; i < descriptorWriteCount; i++)
  {
    const VkWriteDescriptorSet &writeDesc = pDescriptorWrites[i];

    RDCASSERT(writeDesc.dstBinding < bindings.size());

    DescriptorSetSlot **bind = &bindings[writeDesc.dstBinding];
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

        (*bind)[curIdx].SetTexelBuffer(writeDesc.descriptorType,
                                       GetResID(writeDesc.pTexelBufferView[d]));
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

        (*bind)[curIdx].SetImage(writeDesc.descriptorType, writeDesc.pImageInfo[d],
                                 layoutBinding->immutableSampler == NULL);
      }
    }
    else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
      VkWriteDescriptorSetAccelerationStructureKHR *asWrite =
          (VkWriteDescriptorSetAccelerationStructureKHR *)FindNextStruct(
              &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
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

        (*bind)[curIdx].SetAccelerationStructure(writeDesc.descriptorType,
                                                 asWrite->pAccelerationStructures[d]);
      }
    }
    else if(writeDesc.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
    {
      VkWriteDescriptorSetInlineUniformBlock *inlineWrite =
          (VkWriteDescriptorSetInlineUniformBlock *)FindNextStruct(
              &writeDesc, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK);
      memcpy(inlineData.data() + (*bind)->offset + writeDesc.dstArrayElement, inlineWrite->pData,
             inlineWrite->dataSize);
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

        (*bind)[curIdx].SetBuffer(writeDesc.descriptorType, writeDesc.pBufferInfo[d]);
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
  SERIALISE_ELEMENT(set).Important();
  SERIALISE_ELEMENT(descriptorWriteCount);
  SERIALISE_ELEMENT_ARRAY(pDescriptorWrites, descriptorWriteCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          VulkanStatePipeline &pipeline = renderstate.GetPipeline(pipelineBindPoint);
          rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descsets = pipeline.descSets;

          // expand as necessary
          if(descsets.size() < set + 1)
            descsets.resize(set + 1);

          pipeline.lastBoundSet = set;

          descsets[set].pipeLayout = GetResID(layout);
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
      rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descsets =
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetPipeline(pipelineBindPoint).descSets;

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
      else if(pDescriptorWrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // nothing to unwrap, the next chain contains the data which we can leave as-is
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
    for(uint32_t i = 0; i < descriptorWriteCount; i++)
    {
      const VkWriteDescriptorSet &write = pDescriptorWrites[i];

      FrameRefType ref = GetRefType(convert(write.descriptorType));

      for(uint32_t d = 0; d < write.descriptorCount; d++)
      {
        if(write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          if(write.pTexelBufferView[d] != VK_NULL_HANDLE)
          {
            VkResourceRecord *bufView = GetRecord(write.pTexelBufferView[d]);
            record->MarkBufferViewFrameReferenced(bufView, ref);
          }
        }
        else if(write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
        {
          // ignore descriptors not part of the write, by NULL'ing out those members
          // as they might not even point to a valid object
          if(write.descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER &&
             write.pImageInfo[d].imageView != VK_NULL_HANDLE)
          {
            VkResourceRecord *view = GetRecord(write.pImageInfo[d].imageView);
            record->MarkImageViewFrameReferenced(view, ImageRange(), ref);
          }

          if((write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
              write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) &&
             write.pImageInfo[d].sampler != VK_NULL_HANDLE)
            record->MarkResourceFrameReferenced(GetResID(write.pImageInfo[d].sampler),
                                                eFrameRef_Read);
        }
        else if(write.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          // no bindings in this type
        }
        else
        {
          if(write.pBufferInfo[d].buffer != VK_NULL_HANDLE)
          {
            record->MarkBufferFrameReferenced(GetRecord(write.pBufferInfo[d].buffer),
                                              write.pBufferInfo[d].offset,
                                              write.pBufferInfo[d].range, ref);
          }
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
  SERIALISE_ELEMENT(descriptorUpdateTemplate).Important();
  SERIALISE_ELEMENT(layout);
  SERIALISE_ELEMENT(set).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          VulkanStatePipeline &pipeline = renderstate.GetPipeline(bindPoint);
          rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descsets = pipeline.descSets;

          // expand as necessary
          if(descsets.size() < set + 1)
            descsets.resize(set + 1);

          pipeline.lastBoundSet = set;

          descsets[set].pipeLayout = GetResID(layout);
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
      rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descsets =
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.GetPipeline(bindPoint).descSets;

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
    VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate,
    VkPipelineLayout layout, uint32_t set, const void *pData)
{
  SCOPED_DBG_SINK();

  // since it's relatively expensive to walk the memory, we gather frame references at the same time
  // as unwrapping
  rdcarray<rdcpair<ResourceId, FrameRefType>> frameRefs;
  rdcarray<rdcpair<VkImageView, FrameRefType>> imgViewFrameRefs;
  rdcarray<rdcpair<VkBufferView, FrameRefType>> bufViewFrameRefs;
  rdcarray<rdcpair<VkDescriptorBufferInfo, FrameRefType>> bufFrameRefs;

  {
    DescUpdateTemplate *tempInfo = GetRecord(descriptorUpdateTemplate)->descTemplateInfo;

    // allocate the whole blob of memory
    byte *memory = GetTempMemory(tempInfo->unwrapByteSize);

    // iterate the entries, copy the descriptor data and unwrap
    for(const VkDescriptorUpdateTemplateEntry &entry : tempInfo->updates)
    {
      byte *dst = memory + entry.offset;
      const byte *src = (const byte *)pData + entry.offset;

      FrameRefType ref = GetRefType(convert(entry.descriptorType));

      if(entry.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
         entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkBufferView));

          VkBufferView *bufView = (VkBufferView *)dst;

          if(*bufView != VK_NULL_HANDLE)
          {
            bufViewFrameRefs.push_back(make_rdcpair(*bufView, ref));

            *bufView = Unwrap(*bufView);
          }
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

          if(hasSampler && info->sampler != VK_NULL_HANDLE)
          {
            frameRefs.push_back(make_rdcpair(GetResID(info->sampler), eFrameRef_Read));
            info->sampler = Unwrap(info->sampler);
          }
          if(hasImage && info->imageView != VK_NULL_HANDLE)
          {
            frameRefs.push_back(make_rdcpair(GetResID(info->imageView), eFrameRef_Read));
            if(GetRecord(info->imageView)->baseResource != ResourceId())
              frameRefs.push_back(make_rdcpair(GetRecord(info->imageView)->baseResource, ref));
            imgViewFrameRefs.push_back(make_rdcpair(info->imageView, ref));
            info->imageView = Unwrap(info->imageView);
          }
        }
      }
      else if(entry.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
      {
        // memcpy the data
        memcpy(dst, src, entry.descriptorCount);
      }
      else
      {
        for(uint32_t d = 0; d < entry.descriptorCount; d++)
        {
          memcpy(dst, src, sizeof(VkDescriptorBufferInfo));

          VkDescriptorBufferInfo *info = (VkDescriptorBufferInfo *)dst;

          if(info->buffer != VK_NULL_HANDLE)
          {
            bufFrameRefs.push_back(make_rdcpair(*info, ref));

            info->buffer = Unwrap(info->buffer);
          }
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(descriptorUpdateTemplate), eFrameRef_Read);
    record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
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
  SERIALISE_ELEMENT(dstBuffer).Important();
  SERIALISE_ELEMENT(dstOffset).OffsetOrSize();
  SERIALISE_ELEMENT(marker).Important();

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(dstBuffer), dstOffset, 4, eFrameRef_PartialWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdWriteBufferMarker2AMD(SerialiserType &ser,
                                                         VkCommandBuffer commandBuffer,
                                                         VkPipelineStageFlags2 stage,
                                                         VkBuffer dstBuffer, VkDeviceSize dstOffset,
                                                         uint32_t marker)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits2, stage).TypedAs("VkPipelineStageFlags2"_lit);
  SERIALISE_ELEMENT(dstBuffer).Important();
  SERIALISE_ELEMENT(dstOffset).OffsetOrSize();
  SERIALISE_ELEMENT(marker).Important();

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
          ->CmdWriteBufferMarker2AMD(Unwrap(commandBuffer), stage, Unwrap(dstBuffer), dstOffset,
                                     marker);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdWriteBufferMarker2AMD(VkCommandBuffer commandBuffer,
                                               VkPipelineStageFlags2 stage, VkBuffer dstBuffer,
                                               VkDeviceSize dstOffset, uint32_t marker)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdWriteBufferMarker2AMD(Unwrap(commandBuffer), stage,
                                                     Unwrap(dstBuffer), dstOffset, marker));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdWriteBufferMarker2AMD);
    Serialise_vkCmdWriteBufferMarker2AMD(ser, commandBuffer, stage, dstBuffer, dstOffset, marker);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(dstBuffer), dstOffset, 4, eFrameRef_PartialWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginDebugUtilsLabelEXT(SerialiserType &ser,
                                                           VkCommandBuffer commandBuffer,
                                                           const VkDebugUtilsLabelEXT *pLabelInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo).Important();

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

      ActionDescription action;
      action.customName = Label.pLabelName ? Label.pLabelName : "";
      action.flags |= ActionFlags::PushMarker;

      action.markerColor.x = RDCCLAMP(Label.color[0], 0.0f, 1.0f);
      action.markerColor.y = RDCCLAMP(Label.color[1], 0.0f, 1.0f);
      action.markerColor.z = RDCCLAMP(Label.color[2], 0.0f, 1.0f);
      action.markerColor.w = RDCCLAMP(Label.color[3], 0.0f, 1.0f);

      AddEvent();
      AddAction(action);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginDebugUtilsLabelEXT);
    Serialise_vkCmdBeginDebugUtilsLabelEXT(ser, commandBuffer, pLabelInfo);

    if(Vulkan_Debug_VerboseCommandRecording())
    {
      RDCLOG(
          "End marker %s in %s (baked to %s)", pLabelInfo->pLabelName,
          ToStr(record->GetResourceID()).c_str(),
          ToStr(record->bakedCommands ? record->bakedCommands->GetResourceID() : ResourceId()).c_str());
    }

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndDebugUtilsLabelEXT(SerialiserType &ser,
                                                         VkCommandBuffer commandBuffer)
{
  SERIALISE_ELEMENT(commandBuffer).Unimportant();

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

      ActionDescription action;
      action.flags = ActionFlags::PopMarker;

      AddEvent();
      AddAction(action);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndDebugUtilsLabelEXT);
    Serialise_vkCmdEndDebugUtilsLabelEXT(ser, commandBuffer);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdInsertDebugUtilsLabelEXT(SerialiserType &ser,
                                                            VkCommandBuffer commandBuffer,
                                                            const VkDebugUtilsLabelEXT *pLabelInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo).Important();

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

      ActionDescription action;
      action.customName = Label.pLabelName ? Label.pLabelName : "";
      action.flags |= ActionFlags::SetMarker;

      action.markerColor.x = RDCCLAMP(Label.color[0], 0.0f, 1.0f);
      action.markerColor.y = RDCCLAMP(Label.color[1], 0.0f, 1.0f);
      action.markerColor.z = RDCCLAMP(Label.color[2], 0.0f, 1.0f);
      action.markerColor.w = RDCCLAMP(Label.color[3], 0.0f, 1.0f);

      AddEvent();
      AddAction(action);
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

  if(pLabelInfo)
    HandleFrameMarkers(pLabelInfo->pLabelName, commandBuffer);
  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdInsertDebugUtilsLabelEXT);
    Serialise_vkCmdInsertDebugUtilsLabelEXT(ser, commandBuffer, pLabelInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetDeviceMask(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                 uint32_t deviceMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(deviceMask).Important();

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindTransformFeedbackBuffersEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount,
    const VkBuffer *pBuffers, const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBinding).Important();
  SERIALISE_ELEMENT(bindingCount);
  SERIALISE_ELEMENT_ARRAY(pBuffers, bindingCount).Important();
  SERIALISE_ELEMENT_ARRAY(pOffsets, bindingCount).OffsetOrSize();
  SERIALISE_ELEMENT_ARRAY(pSizes, bindingCount).OffsetOrSize();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          if(renderstate.xfbbuffers.size() < firstBinding + bindingCount)
            renderstate.xfbbuffers.resize(firstBinding + bindingCount);

          for(uint32_t i = 0; i < bindingCount; i++)
          {
            renderstate.xfbbuffers[firstBinding + i].buf = GetResID(pBuffers[i]);
            renderstate.xfbbuffers[firstBinding + i].offs = pOffsets[i];
            renderstate.xfbbuffers[firstBinding + i].size = pSizes ? pSizes[i] : VK_WHOLE_SIZE;
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
        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbbuffers[firstBinding + i].buf =
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < bindingCount; i++)
    {
      VkDeviceSize size = VK_WHOLE_SIZE;
      if(pSizes != NULL)
      {
        size = pSizes[i];
      }
      record->MarkBufferFrameReferenced(GetRecord(pBuffers[i]), pOffsets[i], size,
                                        eFrameRef_PartialWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginTransformFeedbackEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBuffer, uint32_t bufferCount,
    const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBuffer).Important();
  SERIALISE_ELEMENT(bufferCount).Important();
  SERIALISE_ELEMENT_ARRAY(pCounterBuffers, bufferCount);
  SERIALISE_ELEMENT_ARRAY(pCounterBufferOffsets, bufferCount).OffsetOrSize();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.firstxfbcounter = firstBuffer;
          renderstate.xfbcounters.resize(bufferCount);

          for(uint32_t i = 0; i < bufferCount; i++)
          {
            renderstate.xfbcounters[i].buf =
                pCounterBuffers ? GetResID(pCounterBuffers[i]) : ResourceId();
            renderstate.xfbcounters[i].offs = pCounterBufferOffsets ? pCounterBufferOffsets[i] : 0;
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

      // track while reading, for fetching the right set of outputs in AddAction
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.firstxfbcounter = firstBuffer;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbcounters.resize(bufferCount);
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginTransformFeedbackEXT);
    Serialise_vkCmdBeginTransformFeedbackEXT(ser, commandBuffer, firstBuffer, bufferCount,
                                             pCounterBuffers, pCounterBufferOffsets);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < bufferCount; i++)
    {
      if(pCounterBuffers && pCounterBuffers[i] != VK_NULL_HANDLE)
      {
        VkDeviceSize offset = pCounterBufferOffsets ? pCounterBufferOffsets[i] : 0;
        record->MarkBufferFrameReferenced(GetRecord(pCounterBuffers[i]), offset, 4,
                                          eFrameRef_ReadBeforeWrite);
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndTransformFeedbackEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t firstBuffer, uint32_t bufferCount,
    const VkBuffer *pCounterBuffers, const VkDeviceSize *pCounterBufferOffsets)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(firstBuffer).Important();
  SERIALISE_ELEMENT(bufferCount).Important();
  SERIALISE_ELEMENT_ARRAY(pCounterBuffers, bufferCount);
  SERIALISE_ELEMENT_ARRAY(pCounterBufferOffsets, bufferCount).OffsetOrSize();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.firstxfbcounter = 0;
          renderstate.xfbcounters.clear();
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

      // track while reading, for fetching the right set of outputs in AddAction
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.firstxfbcounter = 0;
      m_BakedCmdBufferInfo[m_LastCmdBufferID].state.xfbcounters.clear();
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndTransformFeedbackEXT);
    Serialise_vkCmdEndTransformFeedbackEXT(ser, commandBuffer, firstBuffer, bufferCount,
                                           pCounterBuffers, pCounterBufferOffsets);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < bufferCount; i++)
    {
      if(pCounterBuffers && pCounterBuffers[i] != VK_NULL_HANDLE)
      {
        VkDeviceSize offset = pCounterBufferOffsets ? pCounterBufferOffsets[i] : 0;
        record->MarkBufferFrameReferenced(GetRecord(pCounterBuffers[i]), offset, 4,
                                          eFrameRef_ReadBeforeWrite);
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginQueryIndexedEXT(SerialiserType &ser,
                                                        VkCommandBuffer commandBuffer,
                                                        VkQueryPool queryPool, uint32_t query,
                                                        VkQueryControlFlags flags, uint32_t index)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(query).Important();
  SERIALISE_ELEMENT_TYPED(VkQueryControlFlagBits, flags).TypedAs("VkQueryControlFlags"_lit);
  SERIALISE_ELEMENT(index).Important();

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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
  SERIALISE_ELEMENT(queryPool).Important();
  SERIALISE_ELEMENT(query).Important();
  SERIALISE_ELEMENT(index).Important();

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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
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
      .Named("pConditionalRenderingBegin"_lit)
      .Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.conditionalRendering.buffer = GetResID(BeginInfo.buffer);
          renderstate.conditionalRendering.offset = BeginInfo.offset;
          renderstate.conditionalRendering.flags = BeginInfo.flags;
        }

        BeginInfo.buffer = Unwrap(BeginInfo.buffer);
        ObjDisp(commandBuffer)->CmdBeginConditionalRenderingEXT(Unwrap(commandBuffer), &BeginInfo);
      }
    }
    else
    {
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();
          renderstate.conditionalRendering.buffer = ResourceId();
        }

        ObjDisp(commandBuffer)->CmdEndConditionalRenderingEXT(Unwrap(commandBuffer));
      }
    }
    else
    {
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetVertexInputEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(vertexBindingDescriptionCount).Important();
  SERIALISE_ELEMENT_ARRAY(pVertexBindingDescriptions, vertexBindingDescriptionCount);
  SERIALISE_ELEMENT(vertexAttributeDescriptionCount).Important();
  SERIALISE_ELEMENT_ARRAY(pVertexAttributeDescriptions, vertexAttributeDescriptionCount);

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
            ->CmdSetVertexInputEXT(Unwrap(commandBuffer), vertexBindingDescriptionCount,
                                   pVertexBindingDescriptions, vertexAttributeDescriptionCount,
                                   pVertexAttributeDescriptions);

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          renderstate.dynamicStates[VkDynamicVertexInputEXT] = true;

          renderstate.vertexBindings.assign(pVertexBindingDescriptions,
                                            vertexBindingDescriptionCount);
          renderstate.vertexAttributes.assign(pVertexAttributeDescriptions,
                                              vertexAttributeDescriptionCount);

          for(uint32_t i = 0; i < vertexBindingDescriptionCount; i++)
          {
            // set strides whether or not the vertex buffers have been bound, so that the stride is
            // available if a later call to BindVertexBuffers2 doesn't pass any strides (it should
            // use the strides from here)
            renderstate.vbuffers.resize_for_index(i);
            renderstate.vbuffers[i].stride = pVertexBindingDescriptions[i].stride;
          }
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdSetVertexInputEXT(Unwrap(commandBuffer), vertexBindingDescriptionCount,
                                 pVertexBindingDescriptions, vertexAttributeDescriptionCount,
                                 pVertexAttributeDescriptions);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdSetVertexInputEXT(
    VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdSetVertexInputEXT(Unwrap(commandBuffer), vertexBindingDescriptionCount,
                                 pVertexBindingDescriptions, vertexAttributeDescriptionCount,
                                 pVertexAttributeDescriptions));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetVertexInputEXT);
    Serialise_vkCmdSetVertexInputEXT(ser, commandBuffer, vertexBindingDescriptionCount,
                                     pVertexBindingDescriptions, vertexAttributeDescriptionCount,
                                     pVertexAttributeDescriptions);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBeginRendering(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  const VkRenderingInfo *pRenderingInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(RenderingInfo, *pRenderingInfo).Named("pRenderingInfo"_lit).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    byte *tempMem = GetTempMemory(GetNextPatchSize(&RenderingInfo));
    VkRenderingInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, &RenderingInfo);

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        // only if we're partially recording do we update this state
        if(ShouldUpdateRenderpassActive(m_LastCmdBufferID, true))
        {
          GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive =
              m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = true;
        }
        m_BakedCmdBufferInfo[m_LastCmdBufferID].activeSubpass = 0;

        VulkanRenderState &renderstate = GetCmdRenderState();

        {
          renderstate.subpass = 0;
          renderstate.SetRenderPass(ResourceId());
          renderstate.renderArea = RenderingInfo.renderArea;
          renderstate.dynamicRendering = VulkanRenderState::DynamicRendering();
          renderstate.dynamicRendering.active = true;
          renderstate.dynamicRendering.suspended = false;
          renderstate.dynamicRendering.flags = RenderingInfo.flags;
          renderstate.dynamicRendering.layerCount = RenderingInfo.layerCount;
          renderstate.dynamicRendering.viewMask = RenderingInfo.viewMask;
          renderstate.dynamicRendering.color.assign(RenderingInfo.pColorAttachments,
                                                    RenderingInfo.colorAttachmentCount);
          if(RenderingInfo.pDepthAttachment)
            renderstate.dynamicRendering.depth = *RenderingInfo.pDepthAttachment;
          if(RenderingInfo.pStencilAttachment)
            renderstate.dynamicRendering.stencil = *RenderingInfo.pStencilAttachment;

          VkRenderingFragmentDensityMapAttachmentInfoEXT *fragmentDensityAttachment =
              (VkRenderingFragmentDensityMapAttachmentInfoEXT *)FindNextStruct(
                  &RenderingInfo,
                  VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT);

          if(fragmentDensityAttachment)
          {
            renderstate.dynamicRendering.fragmentDensityView = fragmentDensityAttachment->imageView;
            renderstate.dynamicRendering.fragmentDensityLayout =
                fragmentDensityAttachment->imageLayout;
          }

          VkRenderingFragmentShadingRateAttachmentInfoKHR *shadingRateAttachment =
              (VkRenderingFragmentShadingRateAttachmentInfoKHR *)FindNextStruct(
                  &RenderingInfo,
                  VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);

          if(shadingRateAttachment)
          {
            renderstate.dynamicRendering.shadingRateView = shadingRateAttachment->imageView;
            renderstate.dynamicRendering.shadingRateLayout = shadingRateAttachment->imageLayout;
            renderstate.dynamicRendering.shadingRateTexelSize =
                shadingRateAttachment->shadingRateAttachmentTexelSize;
          }

          VkMultisampledRenderToSingleSampledInfoEXT *tileOnlyMSAA =
              (VkMultisampledRenderToSingleSampledInfoEXT *)FindNextStruct(
                  &RenderingInfo, VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT);

          if(tileOnlyMSAA)
          {
            renderstate.dynamicRendering.tileOnlyMSAAEnable =
                tileOnlyMSAA->multisampledRenderToSingleSampledEnable != VK_FALSE;
            renderstate.dynamicRendering.tileOnlyMSAASampleCount = tileOnlyMSAA->rasterizationSamples;
          }

          rdcarray<ResourceId> attachments;

          for(size_t i = 0; i < renderstate.dynamicRendering.color.size(); i++)
            attachments.push_back(GetResID(renderstate.dynamicRendering.color[i].imageView));

          attachments.push_back(GetResID(renderstate.dynamicRendering.depth.imageView));
          attachments.push_back(GetResID(renderstate.dynamicRendering.stencil.imageView));

          renderstate.SetFramebuffer(ResourceId(), attachments);
        }

        // only do discards when not resuming!
        if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest &&
           (RenderingInfo.flags & VK_RENDERING_RESUMING_BIT) == 0)
        {
          rdcarray<VkRenderingAttachmentInfo> dynAtts = renderstate.dynamicRendering.color;

          size_t depthIdx = ~0U;
          size_t stencilIdx = ~0U;
          VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;

          if(renderstate.dynamicRendering.depth.imageView != VK_NULL_HANDLE)
          {
            dynAtts.push_back(renderstate.dynamicRendering.depth);
            depthIdx = dynAtts.size() - 1;
          }

          // if we have different images attached, or different store ops, treat stencil separately
          if(renderstate.dynamicRendering.depth.imageView != VK_NULL_HANDLE &&
             renderstate.dynamicRendering.stencil.imageView != VK_NULL_HANDLE &&
             (renderstate.dynamicRendering.depth.imageView !=
                  renderstate.dynamicRendering.stencil.imageView ||
              renderstate.dynamicRendering.depth.loadOp !=
                  renderstate.dynamicRendering.stencil.loadOp))
          {
            dynAtts.push_back(renderstate.dynamicRendering.stencil);
            stencilIdx = dynAtts.size() - 1;
          }
          // otherwise if the same image is bound and the storeOp is the same then include it
          else if(renderstate.dynamicRendering.stencil.imageView != VK_NULL_HANDLE)
          {
            depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;

            if(renderstate.dynamicRendering.depth.imageView == VK_NULL_HANDLE)
            {
              dynAtts.push_back(renderstate.dynamicRendering.stencil);
              stencilIdx = dynAtts.size() - 1;
            }
          }

          for(size_t i = 0; i < dynAtts.size(); i++)
          {
            if(dynAtts[i].imageView == VK_NULL_HANDLE)
              continue;

            const VulkanCreationInfo::ImageView &viewInfo =
                m_CreationInfo.m_ImageView[GetResID(dynAtts[i].imageView)];
            VkImage image = GetResourceManager()->GetCurrentHandle<VkImage>(viewInfo.image);

            if(dynAtts[i].loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
            {
              VkImageSubresourceRange range = viewInfo.range;

              if(i == depthIdx)
                range.aspectMask = depthAspects;

              // if this is a stencil-only attachment this will override depthAspects
              if(i == stencilIdx)
                range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

              GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassLoad,
                                                        image, dynAtts[i].imageLayout, range,
                                                        renderstate.renderArea);
            }
          }
        }

        ActionFlags drawFlags = ActionFlags::PassBoundary | ActionFlags::BeginPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        // do the same load/store op patching that we do for regular renderpass creates to enable
        // introspection. It doesn't matter that we don't do this before during load because the
        // effects of that are never user-visible.
        if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest)
        {
          for(uint32_t i = 0; i < unwrappedInfo->colorAttachmentCount + 2; i++)
          {
            VkRenderingAttachmentInfo *att =
                (VkRenderingAttachmentInfo *)unwrappedInfo->pColorAttachments + i;

            if(i == unwrappedInfo->colorAttachmentCount)
              att = (VkRenderingAttachmentInfo *)unwrappedInfo->pDepthAttachment;
            else if(i == unwrappedInfo->colorAttachmentCount + 1)
              att = (VkRenderingAttachmentInfo *)unwrappedInfo->pStencilAttachment;

            if(!att)
              continue;

            if(att->storeOp != VK_ATTACHMENT_STORE_OP_NONE)
              att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            if(att->loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
              att->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
          }
        }

        ObjDisp(commandBuffer)->CmdBeginRendering(Unwrap(commandBuffer), unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdBeginRendering again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdBeginRendering(Unwrap(commandBuffer), unwrappedInfo);

      VulkanRenderState &renderstate = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

      {
        renderstate.renderArea = RenderingInfo.renderArea;
        renderstate.dynamicRendering = VulkanRenderState::DynamicRendering();
        renderstate.dynamicRendering.active = true;
        renderstate.dynamicRendering.suspended = false;
        renderstate.dynamicRendering.flags = RenderingInfo.flags;
        renderstate.dynamicRendering.layerCount = RenderingInfo.layerCount;
        renderstate.dynamicRendering.viewMask = RenderingInfo.viewMask;
        renderstate.dynamicRendering.color.assign(RenderingInfo.pColorAttachments,
                                                  RenderingInfo.colorAttachmentCount);
        if(RenderingInfo.pDepthAttachment)
          renderstate.dynamicRendering.depth = *RenderingInfo.pDepthAttachment;
        if(RenderingInfo.pStencilAttachment)
          renderstate.dynamicRendering.stencil = *RenderingInfo.pStencilAttachment;

        VkRenderingFragmentDensityMapAttachmentInfoEXT *fragmentDensityAttachment =
            (VkRenderingFragmentDensityMapAttachmentInfoEXT *)FindNextStruct(
                &RenderingInfo, VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT);

        if(fragmentDensityAttachment)
        {
          renderstate.dynamicRendering.fragmentDensityView = fragmentDensityAttachment->imageView;
          renderstate.dynamicRendering.fragmentDensityLayout = fragmentDensityAttachment->imageLayout;
        }

        VkRenderingFragmentShadingRateAttachmentInfoKHR *shadingRateAttachment =
            (VkRenderingFragmentShadingRateAttachmentInfoKHR *)FindNextStruct(
                &RenderingInfo,
                VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);

        if(shadingRateAttachment)
        {
          renderstate.dynamicRendering.shadingRateView = shadingRateAttachment->imageView;
          renderstate.dynamicRendering.shadingRateLayout = shadingRateAttachment->imageLayout;
          renderstate.dynamicRendering.shadingRateTexelSize =
              shadingRateAttachment->shadingRateAttachmentTexelSize;
        }

        VkMultisampledRenderToSingleSampledInfoEXT *tileOnlyMSAA =
            (VkMultisampledRenderToSingleSampledInfoEXT *)FindNextStruct(
                &RenderingInfo, VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT);

        if(tileOnlyMSAA)
        {
          renderstate.dynamicRendering.tileOnlyMSAAEnable =
              tileOnlyMSAA->multisampledRenderToSingleSampledEnable != VK_FALSE;
          renderstate.dynamicRendering.tileOnlyMSAASampleCount = tileOnlyMSAA->rasterizationSamples;
        }

        rdcarray<ResourceId> attachments;

        for(size_t i = 0; i < renderstate.dynamicRendering.color.size(); i++)
          attachments.push_back(
              m_CreationInfo.m_ImageView[GetResID(renderstate.dynamicRendering.color[i].imageView)].image);

        attachments.push_back(
            m_CreationInfo.m_ImageView[GetResID(renderstate.dynamicRendering.depth.imageView)].image);
        attachments.push_back(
            m_CreationInfo.m_ImageView[GetResID(renderstate.dynamicRendering.stencil.imageView)].image);

        renderstate.SetFramebuffer(ResourceId(), attachments);
      }

      for(size_t i = 0; i < renderstate.dynamicRendering.color.size() + 2; i++)
      {
        VkRenderingAttachmentInfo *att =
            (VkRenderingAttachmentInfo *)&renderstate.dynamicRendering.color[i];

        if(i == renderstate.dynamicRendering.color.size())
          att = (VkRenderingAttachmentInfo *)&renderstate.dynamicRendering.depth;
        else if(i == renderstate.dynamicRendering.color.size() + 1)
          att = (VkRenderingAttachmentInfo *)&renderstate.dynamicRendering.stencil;

        if(!att || att->imageView == VK_NULL_HANDLE)
          continue;

        if(att->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ||
           att->loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        {
          ResourceId image = m_CreationInfo.m_ImageView[GetResID(att->imageView)].image;
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage.push_back(make_rdcpair(
              image, EventUsage(m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID,
                                att->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ? ResourceUsage::Clear
                                                                           : ResourceUsage::Discard,
                                GetResID(att->imageView))));
        }
      }

      AddEvent();
      ActionDescription action;
      action.customName =
          StringFormat::Fmt("vkCmdBeginRendering(%s)", MakeRenderPassOpString(false).c_str());
      action.flags |= ActionFlags::PassBoundary | ActionFlags::BeginPass;

      AddAction(action);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBeginRendering(VkCommandBuffer commandBuffer,
                                        const VkRenderingInfo *pRenderingInfo)
{
  SCOPED_DBG_SINK();

  byte *tempMem = GetTempMemory(GetNextPatchSize(pRenderingInfo));
  VkRenderingInfo *unwrappedInfo = UnwrapStructAndChain(m_State, tempMem, pRenderingInfo);

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdBeginRendering(Unwrap(commandBuffer), unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBeginRendering);
    Serialise_vkCmdBeginRendering(ser, commandBuffer, pRenderingInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    VkRenderingFragmentDensityMapAttachmentInfoEXT *densityMap =
        (VkRenderingFragmentDensityMapAttachmentInfoEXT *)FindNextStruct(
            pRenderingInfo, VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT);

    if(densityMap)
    {
      VkResourceRecord *viewRecord = GetRecord(densityMap->imageView);
      if(viewRecord)
        record->MarkImageViewFrameReferenced(viewRecord, ImageRange(), eFrameRef_Read);
    }

    VkRenderingFragmentShadingRateAttachmentInfoKHR *shadingRate =
        (VkRenderingFragmentShadingRateAttachmentInfoKHR *)FindNextStruct(
            pRenderingInfo, VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR);

    if(shadingRate)
    {
      VkResourceRecord *viewRecord = GetRecord(shadingRate->imageView);
      if(viewRecord)
        record->MarkImageViewFrameReferenced(viewRecord, ImageRange(), eFrameRef_Read);
    }

    for(uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount + 2; i++)
    {
      const VkRenderingAttachmentInfo *att = pRenderingInfo->pColorAttachments + i;

      if(i == pRenderingInfo->colorAttachmentCount)
        att = pRenderingInfo->pDepthAttachment;
      else if(i == pRenderingInfo->colorAttachmentCount + 1)
        att = pRenderingInfo->pStencilAttachment;

      if(!att || att->imageView == VK_NULL_HANDLE)
        continue;

      FrameRefType refType = eFrameRef_ReadBeforeWrite;

      VkResourceRecord *viewRecord = GetRecord(att->imageView);
      const ImageInfo &imInfo = viewRecord->resInfo->imageInfo;

      // if the view covers the whole image
      if(viewRecord->viewRange.baseArrayLayer == 0 && viewRecord->viewRange.baseMipLevel == 0 &&
         viewRecord->viewRange.layerCount() == imInfo.layerCount &&
         viewRecord->viewRange.levelCount() == imInfo.levelCount &&
         // and we're rendering to all layers
         pRenderingInfo->layerCount == imInfo.layerCount &&
         // and the render area covers the whole image dimension
         pRenderingInfo->renderArea.offset.x == 0 && pRenderingInfo->renderArea.offset.y == 0 &&
         pRenderingInfo->renderArea.extent.width == imInfo.extent.width &&
         pRenderingInfo->renderArea.extent.height == imInfo.extent.height)
      {
        // if we're either clearing or discarding, this can be considered completely written
        if(att->loadOp != VK_ATTACHMENT_LOAD_OP_LOAD && att->loadOp != VK_ATTACHMENT_LOAD_OP_NONE_KHR)
        {
          refType = eFrameRef_CompleteWrite;
        }
      }

      // if we're completely writing this resource (i.e. nothing from previous data is visible) and
      // it's also DONT_CARE storage (so nothing from this render pass will be visible after) then
      // it's completely written and discarded in one go.
      if(refType == eFrameRef_CompleteWrite && att->storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
      {
        refType = eFrameRef_CompleteWriteAndDiscard;
      }

      record->MarkImageViewFrameReferenced(viewRecord, ImageRange(), refType);
      if(att->resolveMode)
        record->MarkImageViewFrameReferenced(GetRecord(att->resolveImageView), ImageRange(), refType);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdEndRendering(SerialiserType &ser, VkCommandBuffer commandBuffer)
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

        VulkanRenderState &renderstate = GetCmdRenderState();

        bool suspending = (renderstate.dynamicRendering.flags & VK_RENDERING_SUSPENDING_BIT) != 0;

        if(ShouldUpdateRenderpassActive(m_LastCmdBufferID, true))
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = false;

          // if this rendering is just being suspended, the pass is still active
          if(!suspending && IsCommandBufferPartial(m_LastCmdBufferID))
          {
            GetCommandBufferPartialSubmission(m_LastCmdBufferID)->renderPassActive = false;
          }
        }

        ActionFlags drawFlags = ActionFlags::PassBoundary | ActionFlags::EndPass;
        uint32_t eventId = HandlePreCallback(commandBuffer, drawFlags);

        ObjDisp(commandBuffer)->CmdEndRendering(Unwrap(commandBuffer));

        if(eventId && m_ActionCallback->PostMisc(eventId, drawFlags, commandBuffer))
        {
          // Do not call vkCmdEndRendering again.
          m_ActionCallback->PostRemisc(eventId, drawFlags, commandBuffer);
        }

        // only do discards when not suspending!
        if(m_ReplayOptions.optimisation != ReplayOptimisationLevel::Fastest && !suspending)
        {
          rdcarray<VkRenderingAttachmentInfo> dynAtts = renderstate.dynamicRendering.color;
          dynAtts.push_back(renderstate.dynamicRendering.depth);

          size_t depthIdx = dynAtts.size() - 1;
          size_t stencilIdx = ~0U;
          VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;

          // if we have different images attached, or different store ops, treat stencil separately
          if(renderstate.dynamicRendering.stencil.imageView != VK_NULL_HANDLE &&
             (renderstate.dynamicRendering.depth.imageView !=
                  renderstate.dynamicRendering.stencil.imageView ||
              renderstate.dynamicRendering.depth.storeOp !=
                  renderstate.dynamicRendering.stencil.storeOp))
          {
            dynAtts.push_back(renderstate.dynamicRendering.stencil);
            stencilIdx = dynAtts.size() - 1;
          }
          // otherwise if the same image is bound and the storeOp is the same then include it
          else if(renderstate.dynamicRendering.stencil.imageView != VK_NULL_HANDLE)
          {
            depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
          }

          for(size_t i = 0; i < dynAtts.size(); i++)
          {
            if(dynAtts[i].imageView == VK_NULL_HANDLE)
              continue;

            const VulkanCreationInfo::ImageView &viewInfo =
                m_CreationInfo.m_ImageView[GetResID(dynAtts[i].imageView)];
            VkImage image = GetResourceManager()->GetCurrentHandle<VkImage>(viewInfo.image);

            if(dynAtts[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
            {
              VkImageSubresourceRange range = viewInfo.range;

              if(i == depthIdx)
                range.aspectMask = depthAspects;

              if(i == stencilIdx)
                range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

              GetDebugManager()->FillWithDiscardPattern(commandBuffer, DiscardType::RenderPassStore,
                                                        image, dynAtts[i].imageLayout, range,
                                                        renderstate.renderArea);
            }
          }
        }

        if(suspending)
        {
          renderstate.dynamicRendering.suspended = true;
        }
        else
        {
          renderstate.dynamicRendering = VulkanRenderState::DynamicRendering();
          renderstate.SetFramebuffer(ResourceId(), rdcarray<ResourceId>());
        }
      }
      else if(IsRenderpassOpen(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
        ObjDisp(commandBuffer)->CmdEndRendering(Unwrap(commandBuffer));

        m_BakedCmdBufferInfo[m_LastCmdBufferID].renderPassOpen = false;
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdEndRendering(Unwrap(commandBuffer));

      // fetch any queued indirect readbacks here
      for(const VkIndirectRecordData &indirectcopy :
          m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies)
        ExecuteIndirectReadback(commandBuffer, indirectcopy);

      m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies.clear();

      VulkanRenderState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

      uint32_t eid = m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID;
      rdcarray<rdcpair<ResourceId, EventUsage>> &usage =
          m_BakedCmdBufferInfo[m_LastCmdBufferID].resourceUsage;

      VulkanRenderState::DynamicRendering &dyn = state.dynamicRendering;

      bool suspending = (dyn.flags & VK_RENDERING_SUSPENDING_BIT) != 0;

      rdcarray<VkRenderingAttachmentInfo> dynAtts = dyn.color;
      dynAtts.push_back(dyn.depth);

      // if stencil attachment is different, or only one is resolving, add the stencil attachment.
      // Otherwise depth will cover both (at most)
      if((dyn.depth.imageView != dyn.stencil.imageView) ||
         (dyn.depth.resolveMode != 0) != (dyn.stencil.resolveMode != 0))
        dynAtts.push_back(dyn.stencil);

      for(size_t i = 0; i < dynAtts.size(); i++)
      {
        if(dynAtts[i].resolveMode && dynAtts[i].imageView != VK_NULL_HANDLE &&
           dynAtts[i].resolveImageView != VK_NULL_HANDLE)
        {
          usage.push_back(make_rdcpair(m_CreationInfo.m_ImageView[GetResID(dynAtts[i].imageView)].image,
                                       EventUsage(eid, ResourceUsage::ResolveSrc)));

          usage.push_back(
              make_rdcpair(m_CreationInfo.m_ImageView[GetResID(dynAtts[i].resolveImageView)].image,
                           EventUsage(eid, ResourceUsage::ResolveDst)));
        }

        // also add any discards
        if(dynAtts[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE)
        {
          usage.push_back(make_rdcpair(m_CreationInfo.m_ImageView[GetResID(dynAtts[i].imageView)].image,
                                       EventUsage(eid, ResourceUsage::Discard)));
        }
      }

      AddEvent();
      ActionDescription action;
      action.customName =
          StringFormat::Fmt("vkCmdEndRendering(%s)", MakeRenderPassOpString(true).c_str());
      action.flags |= ActionFlags::PassBoundary | ActionFlags::EndPass;

      AddAction(action);

      if(!suspending)
      {
        // track while reading, reset this to empty so AddAction sets no outputs,
        // but only AFTER the above AddAction (we want it grouped together)
        state.dynamicRendering = VulkanRenderState::DynamicRendering();
        state.SetFramebuffer(ResourceId(), rdcarray<ResourceId>());
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdEndRendering(VkCommandBuffer commandBuffer)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdEndRendering(Unwrap(commandBuffer)));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdEndRendering);
    Serialise_vkCmdEndRendering(ser, commandBuffer);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

VkResult WrappedVulkan::vkBuildAccelerationStructuresKHR(
    VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
  // CPU-side VK_KHR_acceleration_structure calls are not supported for now
  return VK_ERROR_UNKNOWN;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBuildAccelerationStructuresIndirectKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkDeviceAddress *pIndirectDeviceAddresses, const uint32_t *pIndirectStrides,
    const uint32_t *const *ppMaxPrimitiveCounts)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(infoCount).Important();
  SERIALISE_ELEMENT_ARRAY(pInfos, infoCount);
  SERIALISE_ELEMENT_ARRAY(pIndirectDeviceAddresses, infoCount);
  SERIALISE_ELEMENT_ARRAY(pIndirectStrides, infoCount);

  // Convert the array of arrays for easier serialisation
  rdcarray<rdcarray<uint32_t>> maxPrimitives;
  if(ser.IsWriting())
  {
    maxPrimitives.reserve(infoCount);

    for(uint32_t i = 0; i < infoCount; ++i)
      maxPrimitives.emplace_back(ppMaxPrimitiveCounts[i], pInfos[i].geometryCount);
  }

  SERIALISE_ELEMENT(maxPrimitives);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    size_t tempmemSize = sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;
    for(uint32_t i = 0; i < infoCount; ++i)
      tempmemSize += GetNextPatchSize(&pInfos[i]);

    byte *memory = GetTempMemory(tempmemSize);
    VkAccelerationStructureBuildGeometryInfoKHR *unwrappedInfos =
        (VkAccelerationStructureBuildGeometryInfoKHR *)memory;
    memory += sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;

    for(uint32_t i = 0; i < infoCount; ++i)
      unwrappedInfos[i] = *UnwrapStructAndChain(m_State, memory, &pInfos[i]);

    // Convert the maxPrimitives back to a C-style array-of-arrays
    rdcarray<const uint32_t *> tmpMaxPrimitiveCounts(NULL, infoCount);
    for(uint32_t i = 0; i < infoCount; ++i)
      tmpMaxPrimitiveCounts[i] = maxPrimitives[i].data();

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        return true;
    }

    ObjDisp(commandBuffer)
        ->CmdBuildAccelerationStructuresIndirectKHR(Unwrap(commandBuffer), infoCount,
                                                    unwrappedInfos, pIndirectDeviceAddresses,
                                                    pIndirectStrides, tmpMaxPrimitiveCounts.data());
  }

  return true;
}

void WrappedVulkan::vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer commandBuffer, uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkDeviceAddress *pIndirectDeviceAddresses, const uint32_t *pIndirectStrides,
    const uint32_t *const *ppMaxPrimitiveCounts)
{
  {
    size_t tempmemSize = sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;
    for(uint32_t i = 0; i < infoCount; ++i)
      tempmemSize += GetNextPatchSize(&pInfos[i]);

    byte *memory = GetTempMemory(tempmemSize);
    VkAccelerationStructureBuildGeometryInfoKHR *unwrappedInfos =
        (VkAccelerationStructureBuildGeometryInfoKHR *)memory;
    memory += sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;

    for(uint32_t i = 0; i < infoCount; ++i)
      unwrappedInfos[i] = *UnwrapStructAndChain(m_State, memory, &pInfos[i]);

    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                            ->CmdBuildAccelerationStructuresIndirectKHR(
                                Unwrap(commandBuffer), infoCount, unwrappedInfos,
                                pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBuildAccelerationStructuresIndirectKHR);
    Serialise_vkCmdBuildAccelerationStructuresIndirectKHR(ser, commandBuffer, infoCount, pInfos,
                                                          pIndirectDeviceAddresses,
                                                          pIndirectStrides, ppMaxPrimitiveCounts);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < infoCount; ++i)
    {
      const VkAccelerationStructureBuildGeometryInfoKHR &geomInfo = pInfos[i];
      if(geomInfo.srcAccelerationStructure != VK_NULL_HANDLE)
        GetResourceManager()->MarkResourceFrameReferenced(
            GetResID(geomInfo.srcAccelerationStructure), eFrameRef_Read);

      GetResourceManager()->MarkResourceFrameReferenced(GetResID(geomInfo.dstAccelerationStructure),
                                                        eFrameRef_CompleteWrite);

      // Add to the command buffer metadata, so we can know when it has been submitted
      record->cmdInfo->accelerationStructures.push_back(GetRecord(geomInfo.dstAccelerationStructure));
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBuildAccelerationStructuresKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(infoCount).Important();
  SERIALISE_ELEMENT_ARRAY(pInfos, infoCount);

  // Convert the array of arrays for easier serialisation
  rdcarray<rdcarray<VkAccelerationStructureBuildRangeInfoKHR>> rangeInfos;
  if(ser.IsWriting())
  {
    rangeInfos.reserve(infoCount);

    for(uint32_t i = 0; i < infoCount; ++i)
      rangeInfos.emplace_back(ppBuildRangeInfos[i], pInfos[i].geometryCount);
  }

  SERIALISE_ELEMENT(rangeInfos);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    size_t tempmemSize = sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;
    for(uint32_t i = 0; i < infoCount; ++i)
      tempmemSize += GetNextPatchSize(&pInfos[i]);

    byte *memory = GetTempMemory(tempmemSize);
    VkAccelerationStructureBuildGeometryInfoKHR *unwrappedInfos =
        (VkAccelerationStructureBuildGeometryInfoKHR *)memory;
    memory += sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;

    for(uint32_t i = 0; i < infoCount; ++i)
      unwrappedInfos[i] = *UnwrapStructAndChain(m_State, memory, &pInfos[i]);

    // Convert the rangeInfos back to a C-style array-of-arrays
    rdcarray<const VkAccelerationStructureBuildRangeInfoKHR *> tmpBuildRangeInfos;
    tmpBuildRangeInfos.resize(infoCount);
    for(uint32_t i = 0; i < infoCount; ++i)
      tmpBuildRangeInfos[i] = rangeInfos[i].data();

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        return true;
    }

    ObjDisp(commandBuffer)
        ->CmdBuildAccelerationStructuresKHR(Unwrap(commandBuffer), infoCount, unwrappedInfos,
                                            tmpBuildRangeInfos.data());
  }

  return true;
}

void WrappedVulkan::vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer commandBuffer, uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
  {
    size_t tempmemSize = sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;
    for(uint32_t i = 0; i < infoCount; ++i)
      tempmemSize += GetNextPatchSize(&pInfos[i]);

    byte *memory = GetTempMemory(tempmemSize);
    VkAccelerationStructureBuildGeometryInfoKHR *unwrappedInfos =
        (VkAccelerationStructureBuildGeometryInfoKHR *)memory;
    memory += sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount;

    for(uint32_t i = 0; i < infoCount; ++i)
      unwrappedInfos[i] = *UnwrapStructAndChain(m_State, memory, &pInfos[i]);

    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                            ->CmdBuildAccelerationStructuresKHR(Unwrap(commandBuffer), infoCount,
                                                                unwrappedInfos, ppBuildRangeInfos));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBuildAccelerationStructuresKHR);
    Serialise_vkCmdBuildAccelerationStructuresKHR(ser, commandBuffer, infoCount, pInfos,
                                                  ppBuildRangeInfos);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < infoCount; ++i)
    {
      const VkAccelerationStructureBuildGeometryInfoKHR &geomInfo = pInfos[i];
      if(geomInfo.srcAccelerationStructure != VK_NULL_HANDLE)
        GetResourceManager()->MarkResourceFrameReferenced(
            GetResID(geomInfo.srcAccelerationStructure), eFrameRef_Read);

      GetResourceManager()->MarkResourceFrameReferenced(GetResID(geomInfo.dstAccelerationStructure),
                                                        eFrameRef_CompleteWrite);

      // Add to the command buffer metadata, so we can know when it has been submitted
      record->cmdInfo->accelerationStructures.push_back(GetRecord(geomInfo.dstAccelerationStructure));
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyAccelerationStructureKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkCopyAccelerationStructureInfoKHR *pInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Info, *pInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyAccelerationStructureInfoKHR unwrappedInfo = Info;
    unwrappedInfo.src = Unwrap(unwrappedInfo.src);
    unwrappedInfo.dst = Unwrap(unwrappedInfo.dst);

    ObjDisp(commandBuffer)->CmdCopyAccelerationStructureKHR(Unwrap(commandBuffer), &unwrappedInfo);
  }

  return true;
}

void WrappedVulkan::vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer,
                                                      const VkCopyAccelerationStructureInfoKHR *pInfo)
{
  VkCopyAccelerationStructureInfoKHR unwrappedInfo = *pInfo;
  unwrappedInfo.src = Unwrap(unwrappedInfo.src);
  unwrappedInfo.dst = Unwrap(unwrappedInfo.dst);
  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdCopyAccelerationStructureKHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyAccelerationStructureKHR);
    Serialise_vkCmdCopyAccelerationStructureKHR(ser, commandBuffer, pInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pInfo->src), eFrameRef_Read);
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pInfo->dst), eFrameRef_CompleteWrite);

    // Add to the command buffer metadata, so we can know when it has been submitted
    record->cmdInfo->accelerationStructures.push_back(GetRecord(pInfo->dst));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyAccelerationStructureToMemoryKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Info, *pInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyAccelerationStructureToMemoryInfoKHR unwrappedInfo = Info;
    unwrappedInfo.src = Unwrap(unwrappedInfo.src);

    ObjDisp(commandBuffer)->CmdCopyAccelerationStructureToMemoryKHR(Unwrap(commandBuffer), &unwrappedInfo);
  }

  return true;
}

void WrappedVulkan::vkCmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
  // We will always report ASes as incompatible so this would be an illegal call
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyMemoryToAccelerationStructureKHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(Info, *pInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyMemoryToAccelerationStructureInfoKHR unwrappedInfo = Info;
    unwrappedInfo.dst = Unwrap(unwrappedInfo.dst);

    ObjDisp(commandBuffer)->CmdCopyMemoryToAccelerationStructureKHR(Unwrap(commandBuffer), &unwrappedInfo);
  }

  return true;
}

void WrappedVulkan::vkCmdCopyMemoryToAccelerationStructureKHR(
    VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
  VkCopyMemoryToAccelerationStructureInfoKHR unwrappedInfo = *pInfo;
  unwrappedInfo.dst = Unwrap(unwrappedInfo.dst);
  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdCopyMemoryToAccelerationStructureKHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyMemoryToAccelerationStructureKHR);
    Serialise_vkCmdCopyMemoryToAccelerationStructureKHR(ser, commandBuffer, pInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(pInfo->dst), eFrameRef_CompleteWrite);
  }
}

void WrappedVulkan::vkCmdWriteAccelerationStructuresPropertiesKHR(
    VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount,
    const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType,
    VkQueryPool queryPool, uint32_t firstQuery)
{
  byte *memory = GetTempMemory(sizeof(VkAccelerationStructureKHR) * accelerationStructureCount);
  VkAccelerationStructureKHR *unwrappedASes = (VkAccelerationStructureKHR *)memory;
  for(uint32_t i = 0; i < accelerationStructureCount; ++i)
    unwrappedASes[i] = Unwrap(pAccelerationStructures[i]);

  ObjDisp(commandBuffer)
      ->CmdWriteAccelerationStructuresPropertiesKHR(Unwrap(commandBuffer),
                                                    accelerationStructureCount, unwrappedASes,
                                                    queryType, Unwrap(queryPool), firstQuery);
}

VkResult WrappedVulkan::vkWriteAccelerationStructuresPropertiesKHR(
    VkDevice device, uint32_t accelerationStructureCount,
    const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType,
    size_t dataSize, void *pData, size_t stride)
{
  byte *memory = GetTempMemory(sizeof(VkAccelerationStructureKHR) * accelerationStructureCount);
  VkAccelerationStructureKHR *unwrappedASes = (VkAccelerationStructureKHR *)memory;
  for(uint32_t i = 0; i < accelerationStructureCount; ++i)
    unwrappedASes[i] = Unwrap(pAccelerationStructures[i]);

  return ObjDisp(device)->WriteAccelerationStructuresPropertiesKHR(
      Unwrap(device), accelerationStructureCount, unwrappedASes, queryType, dataSize, pData, stride);
}

// CPU-side VK_KHR_acceleration_structure calls are not supported for now
VkResult WrappedVulkan::vkCopyAccelerationStructureKHR(VkDevice device,
                                                       VkDeferredOperationKHR deferredOperation,
                                                       const VkCopyAccelerationStructureInfoKHR *pInfo)
{
  return VK_ERROR_UNKNOWN;
}

VkResult WrappedVulkan::vkCopyAccelerationStructureToMemoryKHR(
    VkDevice device, VkDeferredOperationKHR deferredOperation,
    const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
  return VK_ERROR_UNKNOWN;
}

VkResult WrappedVulkan::vkCopyMemoryToAccelerationStructureKHR(
    VkDevice device, VkDeferredOperationKHR deferredOperation,
    const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
  return VK_ERROR_UNKNOWN;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBindShadersEXT(SerialiserType &ser,
                                                  VkCommandBuffer commandBuffer, uint32_t stageCount,
                                                  const VkShaderStageFlagBits *pStages,
                                                  const VkShaderEXT *pShaders)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(stageCount);
  SERIALISE_ELEMENT_ARRAY(pStages, stageCount);
  SERIALISE_ELEMENT_ARRAY(pShaders, stageCount).Important();

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

        {
          VulkanRenderState &renderstate = GetCmdRenderState();

          for(uint32_t i = 0; i < stageCount; i++)
          {
            int stageIndex = StageIndex(pStages[i]);

            renderstate.shaderObjects[stageIndex] =
                pShaders && (pShaders[i] != VK_NULL_HANDLE) ? GetResID(pShaders[i]) : ResourceId();

            // calling vkCmdBindShadersEXT disturbs the corresponding pipeline bind points
            // such that any pipelines previously bound to those points are no longer bound
            if(stageIndex == (int)ShaderStage::Compute)
            {
              renderstate.compute.shaderObject = true;
              renderstate.compute.pipeline = ResourceId();
            }
            else
            {
              renderstate.graphics.shaderObject = true;
              renderstate.graphics.pipeline = ResourceId();
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
      // track while reading since Serialise_vkCmdBindPipeline does as well
      for(uint32_t i = 0; i < stageCount; i++)
      {
        int stageIndex = StageIndex(pStages[i]);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].state.shaderObjects[stageIndex] =
            pShaders && (pShaders[i] != VK_NULL_HANDLE) ? GetResID(pShaders[i]) : ResourceId();

        if(stageIndex == (int)ShaderStage::Compute)
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.compute.pipeline = ResourceId();
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.compute.shaderObject = true;
        }
        else
        {
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphics.pipeline = ResourceId();
          m_BakedCmdBufferInfo[m_LastCmdBufferID].state.graphics.shaderObject = true;
        }
      }
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)
          ->CmdBindShadersEXT(Unwrap(commandBuffer), stageCount, pStages,
                              UnwrapArray(pShaders, stageCount));
  }

  return true;
}

void WrappedVulkan::vkCmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount,
                                        const VkShaderStageFlagBits *pStages,
                                        const VkShaderEXT *pShaders)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBindShadersEXT(Unwrap(commandBuffer), stageCount, pStages,
                                              UnwrapArray(pShaders, stageCount)));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBindShadersEXT);
    Serialise_vkCmdBindShadersEXT(ser, commandBuffer, stageCount, pStages, pShaders);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    if(pShaders)
    {
      for(uint32_t i = 0; i < stageCount; i++)
      {
        // binding NULL is legal
        if(pShaders[i] != VK_NULL_HANDLE)
          record->MarkResourceFrameReferenced(GetResID(pShaders[i]), eFrameRef_Read);
      }
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateCommandPool, VkDevice device,
                                const VkCommandPoolCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *, VkCommandPool *pCommandPool);

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

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass2, VkCommandBuffer commandBuffer,
                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                const VkSubpassBeginInfo *pSubpassBeginInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdNextSubpass2, VkCommandBuffer commandBuffer,
                                const VkSubpassBeginInfo *pSubpassBeginInfo,
                                const VkSubpassEndInfo *pSubpassEndInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndRenderPass2, VkCommandBuffer commandBuffer,
                                const VkSubpassEndInfo *pSubpassEndInfo);

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

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdWriteBufferMarker2AMD, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlags2 stage, VkBuffer dstBuffer,
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

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindVertexBuffers2, VkCommandBuffer commandBuffer,
                                uint32_t firstBinding, uint32_t bindingCount,
                                const VkBuffer *pBuffers, const VkDeviceSize *pOffsets,
                                const VkDeviceSize *pSizes, const VkDeviceSize *pStrides);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdPipelineBarrier2, VkCommandBuffer commandBuffer,
                                const VkDependencyInfo *pDependencyInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdWriteTimestamp2, VkCommandBuffer commandBuffer,
                                VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query);

INSTANTIATE_FUNCTION_SERIALISED(
    void, vkCmdSetVertexInputEXT, VkCommandBuffer commandBuffer,
    uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBeginRendering, VkCommandBuffer commandBuffer,
                                const VkRenderingInfo *pRenderingInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdEndRendering, VkCommandBuffer commandBuffer);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBuildAccelerationStructuresIndirectKHR,
                                VkCommandBuffer commandBuffer, uint32_t infoCount,
                                const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
                                const VkDeviceAddress *pIndirectDeviceAddresses,
                                const uint32_t *pIndirectStrides,
                                const uint32_t *const *ppMaxPrimitiveCounts);
INSTANTIATE_FUNCTION_SERIALISED(
    void, vkCmdBuildAccelerationStructuresKHR, VkCommandBuffer commandBuffer, uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyAccelerationStructureKHR,
                                VkCommandBuffer commandBuffer,
                                const VkCopyAccelerationStructureInfoKHR *pInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyAccelerationStructureToMemoryKHR,
                                VkCommandBuffer commandBuffer,
                                const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyMemoryToAccelerationStructureKHR,
                                VkCommandBuffer commandBuffer,
                                const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBindShadersEXT, VkCommandBuffer commandBuffer,
                                uint32_t stageCount, const VkShaderStageFlagBits *pStages,
                                const VkShaderEXT *pShaders);
