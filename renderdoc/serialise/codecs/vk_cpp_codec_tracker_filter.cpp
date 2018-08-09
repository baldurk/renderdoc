/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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
#include "vk_cpp_codec_tracker.h"
#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
// --------------------------------------------------------------------------
// Vulkan API specific tracking functions
// --------------------------------------------------------------------------
void TraceTracker::CreateResource(ExtObject *o)
{
  if(InitResourceFind(o->At(3)->U64()) != InitResourceEnd())
  {
    ExtObject *ci = o->At(1);
    // FindChild is used here because buffers and images have different CreateInfo structures
    ExtObject *usage = as_ext(ci->FindChild("usage"));
    usage->U64() |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    usage->data.str =
        std::string(usage->Str()) + std::string("| /*rdoc:init*/ VK_IMAGE_USAGE_TRANSFER_DST_BIT");
  }
}

bool TraceTracker::CreateFramebuffer(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  ExtObject *renderpass = ci->At(3);
  ExtObject *attachments = ci->At(5);
  ExtObject *framebuffer = o->At(3);

  bool attachment_is_swapchain_image = false;
  for(uint64_t i = 0; i < attachments->Size(); i++)
  {
    if(IsPresentationResource(attachments->At(i)->U64()))
    {
      attachment_is_swapchain_image = true;
      break;
    }
  }

  bool renderpass_presents = IsPresentationResource(renderpass->U64());
  if(attachment_is_swapchain_image || renderpass_presents)
  {
    presentResources.insert(ExtObjectIDMapPair(framebuffer->U64(), o));
    std::string name = code->MakeVarName(framebuffer->Type(), framebuffer->U64());
    std::string acquired = name + std::string("[acquired_frame]");
    TrackVarInMap(resources, framebuffer->Type(), acquired.c_str(),
                  framebuffer->U64() + PRESENT_VARIABLE_OFFSET);
    return true;
  }
  return false;
}

bool TraceTracker::CreateImageView(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  ExtObject *view = o->At(3);
  ExtObject *image = ci->At(3);

  if(IsPresentationResource(image->U64()))
  {
    // Mark these view are presentation.
    presentResources.insert(ExtObjectIDMapPair(view->U64(), o));

    std::string name = code->MakeVarName(view->Type(), view->U64());
    // For each view there is a 'VkImageView_<id>[acquired_frame]' which is used in the render loop.
    std::string acquired = name + std::string("[acquired_frame]");
    TrackVarInMap(resources, view->Type(), acquired.c_str(), view->U64() + PRESENT_VARIABLE_OFFSET);
    return true;
  }

  return false;
}

// The purpose of this function is to track what's happening on queue submit.
// We are interested in a few things here:
// 1. If the queue submitting any command buffer that has transfered an image
// to a presentation layout. If yes, use this queue as a present queue.
// 2. Accumulate semaphore from p[Wait|Signal]Semaphores arrays. We need to\
// make sure that there are no 'waits' that are never signalled and also to
// make sure Present() waits on all signalled semaphores later.
// 3. Any Queue that submits anything needs to do a WaitIdle at the end of the
// frame in order to avoid synchronization problems. This can be optimized later.
void TraceTracker::QueueSubmit(ExtObject *o)
{
  ExtObject *queue = o->At(0);
  // Multiple submissions can happen at the same time in Vulkan
  ExtObject *submits = o->At(2);
  for(uint32_t s = 0; s < submits->Size(); s++)
  {
    // Multiple command buffers can be submitted at the same time
    ExtObject *cmd_buffers = submits->At(s)->At(6);

    // Check if a command buffer is transferring an image for Presentation.
    // If it does, remember this queue as a Present Queue.
    bool is_presenting = false;
    for(uint32_t b = 0; b < cmd_buffers->Size(); b++)
    {
      if(IsPresentationResource(cmd_buffers->At(b)->U64()))
      {
        presentResources.insert(ExtObjectIDMapPair(queue->U64(), o));
        presentQueueID = queue->U64();
        is_presenting = true;
      }
    }

    ExtObject *wait = submits->At(s)->At(3);
    ExtObject *wait_dst_stage = submits->At(s)->At(4);
    ExtObject *signal = submits->At(s)->At(8);
    ExtObject *wait_count = submits->At(s)->At(2);

    // If presenting, add a dependency on acquire_semaphore.
    if(is_presenting && signalSemaphoreIDs.find(ACQUIRE_SEMAPHORE_VAR_ID) == signalSemaphoreIDs.end())
    {
      RDCASSERT(wait_dst_stage->Size() == wait->Size());

      wait->PushOne(new ExtObject("aux.semaphore", "VkSemaphore", ACQUIRE_SEMAPHORE_VAR_ID,
                                  SDBasic::Resource));

      wait_dst_stage->PushOne(new ExtObject("$el", "VkPipelineStageFlagBits",
                                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT"));
      wait_count->U64()++;

      signalSemaphoreIDs.insert(U64MapPair(ACQUIRE_SEMAPHORE_VAR_ID, 1));
    }

    // Check that 'wait' semaphores have been signaled, otherwise, there will
    // be a deadlock during the execution. This includes removing acquire_semaphore
    // if for some reason it was added a second time.
    std::vector<ExtObject *> remove_list;
    for(uint32_t i = 0; i < wait->Size(); i++)
    {
      ExtObject *semaphore = wait->At(i);
      ExtObject *dst_stage = wait_dst_stage->At(i);
      U64MapIter find = signalSemaphoreIDs.find(semaphore->U64());

      if(find == signalSemaphoreIDs.end() || find->second == 0)
      {
        // We are waiting on this semaphore, but it was never signaled.
        remove_list.push_back(semaphore);
        remove_list.push_back(dst_stage);
        wait_count->U64()--;
      }
      else
      {
        find->second--;
      }
    }

    while(remove_list.size() > 0)
    {
      bool early_continue = false;
      ExtObjectVec::iterator rem = remove_list.begin();
      for(uint32_t i = 0; i < wait->Size(); i++)
      {
        if(wait->At(i) == *rem)
        {
          remove_list.erase(rem);
          wait->RemoveOne(i);
          early_continue = true;
          break;
        }
      }
      if(early_continue)
        continue;
      for(uint32_t i = 0; i < wait_dst_stage->Size(); i++)
      {
        if(wait_dst_stage->At(i) == *rem)
        {
          remove_list.erase(rem);
          wait_dst_stage->RemoveOne(i);
          break;
        }
      }
    }

    // Accumulate semaphore signals to correctly evaluate 'wait' semaphores
    // will work correctly later.
    for(uint32_t i = 0; i < signal->Size(); i++)
    {
      ExtObject *semaphore = signal->At(i);
      U64MapIter find = signalSemaphoreIDs.find(semaphore->U64());
      if(find == signalSemaphoreIDs.end())
      {
        signalSemaphoreIDs.insert(U64MapPair(semaphore->U64(), 1));
      }
      else
      {
        find->second++;
      }
    }
  }

  // Add queue to a list of submitted queues;
  submittedQueues.insert(U64MapPair(queue->U64(), queue->U64()));
}

void TraceTracker::BeginCommandBuffer(ExtObject *o)
{
  ExtObject *inherit = o->At(1)->At(3);
  if(inherit == NULL || inherit->Size() == 0)
    return;

  ExtObject *renderpass = inherit->At(2);
  ExtObject *framebuffer = inherit->At(4);

  bool is_presenting_rp = IsPresentationResource(renderpass->U64());
  bool is_presenting_fb = IsPresentationResource(framebuffer->U64());
  ExtObject *cmd = o->At(0);

  if(is_presenting_rp || is_presenting_fb)
    presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));

  if(is_presenting_fb)
  {
    framebuffer->U64() += PRESENT_VARIABLE_OFFSET;
  }
}

bool TraceTracker::FilterCmdPipelineBarrier(ExtObject *o)
{
  ExtObject *cmd = o->At(0);
  ExtObject *memory_count = o->At(4);
  ExtObject *memory = o->At(5);
  memory_count->U64() = memory->Size();

  ExtObject *buffer_count = o->At(6);
  ExtObject *buffer = o->At(7);
  for(uint64_t i = 0; i < buffer->Size();)
  {
    ExtObject *resource = buffer->At(i)->At(6);
    if(!IsValidNonNullResouce(resource->U64()))
    {
      buffer->RemoveOne(buffer->At(i));
    }
    else
    {
      i++;
    }
  }
  buffer_count->U64() = buffer->Size();

  ExtObject *image_count = o->At(8);
  ExtObject *image = o->At(9);
  for(uint64_t i = 0; i < image->Size();)
  {
    ExtObject *resource = image->At(i)->At(8);
    if (IsPresentationResource(resource->U64())) {
      resource->U64() = PRESENT_IMAGE_OFFSET;
      presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));
      i++;
    } else if (!IsValidNonNullResouce(resource->U64())) {
      image->RemoveOne(image->At(i));
    } else {
      i++;
    }
  }
  image_count->U64() = image->Size();

  return (memory->Size() != 0 || buffer->Size() != 0 || image->Size() != 0);
}

bool TraceTracker::CmdWaitEvents(ExtObject *o)
{
  ExtObject *event_count = o->At(1);
  ExtObject *events = o->At(2);
  for(uint64_t i = 0; i < events->Size();)
  {
    if(!IsValidNonNullResouce(events->At(i)->U64()))
    {
      events->RemoveOne(events->At(i));
    }
    else
    {
      i++;
    }
  }
  event_count->U64() = events->Size();

  ExtObject *memory_count = o->At(5);
  ExtObject *memory = o->At(6);
  memory_count->U64() = memory->Size();

  ExtObject *buffer_count = o->At(7);
  ExtObject *buffer = o->At(8);
  for(uint64_t i = 0; i < buffer->Size();)
  {
    ExtObject *resource = buffer->At(i)->At(6);
    if(!IsValidNonNullResouce(resource->U64()))
    {
      buffer->RemoveOne(buffer->At(i));
    }
    else
    {
      i++;
    }
  }
  buffer_count->U64() = buffer->Size();

  ExtObject *image_count = o->At(9);
  ExtObject *image = o->At(10);
  for(uint64_t i = 0; i < image->Size();)
  {
    ExtObject *resource = image->At(i)->At(8);
    if(!IsValidNonNullResouce(resource->U64()))
    {
      image->RemoveOne(image->At(i));
    }
    else
    {
      i++;
    }
  }
  image_count->U64() = image->Size();

  return (events->Size() != 0 || memory->Size() != 0 || buffer->Size() != 0 || image->Size() != 0);
}

// The purpose of this function is to do several things:
// 1. Keep track of command buffers that transfer a resource into a Present state
// 2. Figure out which image and which image view is transfered into Present state
// 3. For the image view that is transfered into Present state, find it's index
// in the swapchain
void TraceTracker::CmdBeginRenderPass(ExtObject *o)
{
  ExtObject *renderpass_bi = o->At(1);
  ExtObject *renderpass = renderpass_bi->At(2);
  ExtObject *framebuffer = renderpass_bi->At(3);
  if(IsPresentationResource(renderpass->U64()) || IsPresentationResource(framebuffer->U64()))
  {
    // If the renderpass shows up in presentResources, framebuffer
    // must be there too.
    RDCASSERT(IsPresentationResource(framebuffer->U64()));
    framebuffer->U64() += PRESENT_VARIABLE_OFFSET;

    // Save the current command buffer to the list of presentation resources.
    ExtObject *cmd = o->At(0);
    presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));
  }
}

void TraceTracker::FilterCmdCopyImageToBuffer(ExtObject *o)
{
  ExtObject *cmd = o->At(0);
  if(IsPresentationResource(o->At(1)->U64()))
  {
    presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));
    o->At(3)->U64() = PRESENT_IMAGE_OFFSET;
  }
}

void TraceTracker::FilterCmdCopyImage(ExtObject *o)
{
  ExtObject *cmd = o->At(0);
  if(IsPresentationResource(o->At(1)->U64()) || IsPresentationResource(o->At(3)->U64()))
  {
    presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));
    uint32_t idx = IsPresentationResource(o->At(1)->U64()) ? 1 : 3;
    o->At(idx)->U64() = PRESENT_IMAGE_OFFSET;
  }
}

void TraceTracker::FilterCmdBlitImage(ExtObject *o)
{
  ExtObject *cmd = o->At(0);
  if(IsPresentationResource(o->At(1)->U64()) || IsPresentationResource(o->At(3)->U64()))
  {
    presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));
    uint32_t idx = IsPresentationResource(o->At(1)->U64()) ? 1 : 3;
    o->At(idx)->U64() = PRESENT_IMAGE_OFFSET;
  }
}

void TraceTracker::FilterCmdResolveImage(ExtObject *o)
{
  ExtObject *cmd = o->At(0);
  if(IsPresentationResource(o->At(1)->U64()) || IsPresentationResource(o->At(3)->U64()))
  {
    presentResources.insert(ExtObjectIDMapPair(cmd->U64(), o));
    uint32_t idx = IsPresentationResource(o->At(1)->U64()) ? 1 : 3;
    o->At(idx)->U64() = PRESENT_IMAGE_OFFSET;
  }
}

void TraceTracker::FilterCreateDevice(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  uint64_t queueCreateInfoCount = ci->At("queueCreateInfoCount")->U64();
  ExtObject *queueCreateInfos = ci->At("pQueueCreateInfos");
  RDCASSERT(queueCreateInfoCount <= queueUsed.size());
  RDCASSERT(queueCreateInfoCount <= queueCreateInfos->Size());
  for(size_t i = 0; i < queueCreateInfoCount; i++)
  {
    ExtObject *queueCreateInfo = queueCreateInfos->At(i);
    uint64_t &queueCount = queueCreateInfo->At("queueCount")->U64();
    uint64_t lastUsedQueue = 0;
    for(size_t j = 0; j < queueUsed[i].size(); j++)
    {
      if(queueUsed[i][j])
      {
        lastUsedQueue = j;
      }
    }
    queueCount = lastUsedQueue + 1;
  }
}

bool TraceTracker::FilterUpdateDescriptorSetWithTemplate(ExtObject *o)
{
  ExtObject *writeDescriptorSets = o->At(3);
  writeDescriptorSets->name = "VkWriteDescriptorSets";
  for(uint64_t i = 0; i < writeDescriptorSets->Size(); i++)
  {
    ExtObject *wds = writeDescriptorSets->At(i);
    wds->At(2)->U64() = o->At(1)->U64();
    if(!FilterWriteDescriptorSet(wds))
    {
      writeDescriptorSets->RemoveOne(wds);
      i--;
    }
  }
  return (writeDescriptorSets->Size() > 0);
}

bool TraceTracker::FilterCreateGraphicsPipelines(ExtObject *o)
{
  RDCASSERT(o->At(2)->U64() == 1);    // only one pipeline gets created at a time.
  ExtObject *ci = o->At(3);
  ExtObject *ms = ci->At(10);
  if(!ms->IsNULL())
  {
    ExtObject *sample_mask = ms->At(6);
    if(!sample_mask->IsNULL())
    {
      ExtObject *sample_mask_el = as_ext(sample_mask->Duplicate());
      sample_mask->type.basetype = SDBasic::Array;
      sample_mask->PushOne(sample_mask_el);
    }
  }

  // For some reason VkSpecializationMapEntry objects have 'constantID' field duplicated.
  // This removes the duplicate fields.
  ExtObject *stages = ci->At(4);
  for(uint64_t i = 0; i < stages->Size(); i++)
  {
    ExtObject *specializationInfo = stages->At(i)->At(6);
    if(specializationInfo->IsNULL())
    {
      continue;
    }
    ExtObject *mapEntries = specializationInfo->At(1);
    for(uint64_t j = 0; j < mapEntries->Size(); j++)
    {
      ExtObject *mapEntry = mapEntries->At(j);
      if(mapEntry->Size() != 3)
      {
        RDCASSERT(mapEntry->Size() == 4);
        RDCASSERT(std::string(mapEntry->At(0)->Name()) == "constantID");
        RDCASSERT(std::string(mapEntry->At(2)->Name()) == "constantID");
        mapEntry->data.children.erase(2);
      }
    }
  }
  return true;
}

bool TraceTracker::FilterCreateComputePipelines(ExtObject *o)
{
  RDCASSERT(o->At(2)->U64() == 1);    // only one pipeline gets created at a time.
  ExtObject *ci = o->At(3);

  // For some reason VkSpecializationMapEntry objects have 'constantID' field duplicated.
  // This removes the duplicate fields.
  ExtObject *stage = ci->At(3);
  ExtObject *specializationInfo = stage->At(6);
  if(!specializationInfo->IsNULL())
  {
    ExtObject *mapEntries = specializationInfo->At(1);
    for(uint64_t j = 0; j < mapEntries->Size(); j++)
    {
      ExtObject *mapEntry = mapEntries->At(j);
      if(mapEntry->Size() != 3)
      {
        RDCASSERT(mapEntry->Size() == 4);
        RDCASSERT(std::string(mapEntry->At(0)->Name()) == "constantID");
        RDCASSERT(std::string(mapEntry->At(2)->Name()) == "constantID");
        mapEntry->data.children.erase(2);
      }
    }
  }
  return true;
}

bool TraceTracker::FilterCreateImage(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  // RenderDoc adds this entry for extension support in 1.17.
  if(ci->At(1)->Name() == std::string("pNextType"))
  {
    ExtObject *pNextType = ci->At(1);
    ci->RemoveOne(pNextType);
  }
  return true;
}

bool TraceTracker::FilterImageInfoDescSet(uint64_t type, uint64_t image_id, uint64_t sampler_id,
                                          uint64_t immut_sampler_id, ExtObject *layout,
                                          ExtObject *descImageInfo)
{
  bool is_sampler = type == VK_DESCRIPTOR_TYPE_SAMPLER;
  bool has_sampler = is_sampler || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bool is_sampled_image = type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bool is_presented = presentResources.find(image_id) != presentResources.end();
  if(is_presented)
    descImageInfo->At(1)->U64() += PRESENT_VARIABLE_OFFSET;
  if(has_sampler)
  {    // If descriptorType has_sampler, it may come from immutable samples in desc set layout.
    if(!IsValidNonNullResouce(sampler_id) && !IsValidNonNullResouce(immut_sampler_id))
    {
      return false;
    }
    if(is_sampled_image && (!IsValidNonNullResouce(image_id)))
    {
      return false;
    }
  }
  else
  {
    if(!IsValidNonNullResouce(image_id))
    {
      return false;
    }
  }

  if(is_sampler)
  {
    layout->data.str = "VK_IMAGE_LAYOUT_UNDEFINED";
  }
  else
  {
    // TODO(akharlamov) I think this is not needed anymore.
    // Find and replace VkImageLayout<> entries.
    std::string result = layout->Str();
    size_t open_bracket = result.find("<");
    size_t close_bracket = result.find(">");
    if(open_bracket != std::string::npos && close_bracket != std::string::npos)
    {
      result.replace(open_bracket, 1, "(");
      result.replace(close_bracket, 1, ")");
    }
    layout->data.str = result;
  }

  return true;
}

bool TraceTracker::FilterBufferInfoDescSet(uint64_t buffer_id, uint64_t offset, ExtObject *range)
{
  if(!IsValidNonNullResouce(buffer_id))
  {
    return false;
  }
  ExtObject *buffer_ci = ResourceCreateFind(buffer_id)->second.sdobj->At(1);
  ExtObject *buffer_size = buffer_ci->At(3);
  if(range->U64() > buffer_size->U64() - offset && range->U64() != VK_WHOLE_SIZE)
  {
    RDCWARN(
        "Buffer %llu has size (%llu) and is bound with (range %llu, offset %llu). "
        "Replacing with ~0ULL",    // should I replace it with (range - offset) instead?
        buffer_id,
        buffer_size->U64(), range->U64(), offset);
    range->U64() = VK_WHOLE_SIZE;
    // Force it to be unsigned int type
    range->type.basetype = SDBasic::UnsignedInteger;
  }
  return true;
}

bool TraceTracker::FilterTexelBufferViewDescSet(uint64_t texelview_id)
{
  return IsValidNonNullResouce(texelview_id);
}

bool TraceTracker::FilterWriteDescriptorSet(ExtObject *wds)
{
  uint64_t descriptorSet_id = wds->At(2)->U64();
  if(!IsValidNonNullResouce(descriptorSet_id))
  {
    return false;
  }
  // Descriptor Set Layout Create Info aka dsLayoutCI.
  ExtObject *dsLayoutCI = DescSetInfosFindLayout(descriptorSet_id)->At(1);
  ExtObject *dsLayoutBindings = dsLayoutCI->At(4);

  ExtObject *dsBinding = wds->At(3);
  ExtObject *dsArrayElement = wds->At(4);
  ExtObject *dsType = wds->At(6);

  // TODO(akharlamov) is it legal to a binding # that's larger than layout binding size?
  ExtObject *dsLayoutBinding = NULL;
  for(uint64_t i = 0; i < dsLayoutBindings->Size(); i++)
  {
    ExtObject *layoutBinding = dsLayoutBindings->At(i);
    if(layoutBinding->At(0)->U64() == dsBinding->U64())
    {
      dsLayoutBinding = layoutBinding;
      if(dsType->U64() != layoutBinding->At(1)->U64())
      {
        RDCWARN(
            "Descriptor set binding type %s at %llu doesn't match descriptor set layout bindings "
            "type %s at %llu",
            dsType->ValueStr().c_str(), dsBinding->U64(), layoutBinding->At(1)->ValueStr().c_str(),
            dsBinding->U64());
        RDCASSERT(0);    // THIS SHOULD REALLY NEVER HAPPEN!
      }
      break;
    }
  }

  if(dsLayoutBinding == NULL)
  {
    RDCWARN(
        "Descriptor set layout with binding # == %llu is not found in "
        "VkDescriptorSetLayoutCreateInfo.CreateInfo.pBindings[%llu]",
        dsBinding->U64(), dsLayoutBindings->Size());
    RDCASSERT(0);    // THIS SHOULD REALLY NEVER HAPPEN!
    return false;
  }
  ExtObject *dsImmutSamplers = dsLayoutBinding->At(4);
  // Either there were no Immutable Samplers OR there is an immutable sampler for each element.
  RDCASSERT(dsImmutSamplers->Size() == 0 || dsImmutSamplers->Size() == dsLayoutBinding->At(2)->U64());
  if(dsImmutSamplers->Size() > 0)
    dsImmutSamplers = dsImmutSamplers->At(dsArrayElement->U64());

  switch(dsType->U64())
  {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    {    // use image info
      ExtObject *images = wds->At(7);
      for(uint64_t i = 0; i < images->Size();)
      {
        ExtObject *image = images->At(i);
        if(!FilterImageInfoDescSet(dsType->U64(), image->At(1)->U64(), image->At(0)->U64(),
                                   dsImmutSamplers->U64(), image->At(2), image))
          images->RemoveOne(image);
        else
          i++;
      }
      if(images->Size() == 0)
        return false;
    }
    break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    {    // use buffer info
      ExtObject *buffers = wds->At(8);
      for(uint64_t i = 0; i < buffers->Size();)
      {
        ExtObject *buffer = buffers->At(i);
        if(!FilterBufferInfoDescSet(buffer->At(0)->U64(), buffer->At(1)->U64(), buffer->At(2)))
        {
          buffers->RemoveOne(buffer);
        }
        else
          i++;
      }
      if(buffers->Size() == 0)
        return false;
    }
    break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    {    // use texel view info
      ExtObject *texelviews = wds->At(9);
      for(uint64_t i = 0; i < texelviews->Size();)
      {
        ExtObject *texelview = texelviews->At(i);
        if(!FilterTexelBufferViewDescSet(texelview->U64()))
        {
          texelviews->RemoveOne(texelview);
        }
        else
          i++;
      }
      if(texelviews->Size() == 0)
        return false;
    }
    break;
  }
  return true;
}

bool TraceTracker::FilterUpdateDescriptorSets(ExtObject *o)
{
  ExtObject *dsWrite = o->At(2);
  for(uint64_t i = 0; i < dsWrite->Size(); i++)
  {
    ExtObject *wds = dsWrite->At(i);
    if(!FilterWriteDescriptorSet(wds))
    {
      dsWrite->RemoveOne(wds);
      i--;
    }
  }

  ExtObject *dsWrite_count = o->At(1);
  dsWrite_count->U64() = dsWrite->Size();

  ExtObject *dsCopy = o->At(4);
  for(uint64_t i = 0; i < dsCopy->Size(); i++)
  {
    ExtObject *cds = dsCopy->At(i);
    ExtObject *src_desc_set = cds->At(2);
    ExtObject *dst_desc_set = cds->At(5);
    if(!IsValidNonNullResouce(src_desc_set->U64()) || !IsValidNonNullResouce(dst_desc_set->U64()))
    {
      dsCopy->RemoveOne(cds);
      i--;
    }
  }

  ExtObject *dsCopy_count = o->At(3);
  dsCopy_count->U64() = dsCopy->Size();
  return (dsCopy->Size() > 0) || (dsWrite->Size() > 0);
}

bool TraceTracker::FilterInitDescSet(ExtObject *o)
{
  if(o->At(0)->U64() != VkResourceType::eResDescriptorSet)
  {    // FilterInitDescSet only filters out invalid descriptor set updates.
    return true;
  }

  uint64_t descriptorSet_id = o->At(1)->U64();
  ExtObject *initBindings = o->At(2);
  ExtObject *dsLayoutCI = DescSetInfosFindLayout(descriptorSet_id)->At(1);
  ExtObject *dsLayoutBindings = dsLayoutCI->At(4);

  if(initBindings->Size() == 0)
    return false;

  std::vector<uint64_t> initBindingsSizes;
  for(uint64_t i = 0; i < initBindings->Size(); i++)
  {
    initBindingsSizes.push_back(initBindings->At(i)->Size());
    RDCASSERT(initBindingsSizes[i] == 3);
  }

  struct BindingInfo
  {
    uint64_t binding;
    uint64_t type;
    uint64_t count;
    std::string typeStr;
    uint64_t index;
    bool operator<(const BindingInfo &r) const { return binding < r.binding; }
  };

  std::vector<BindingInfo> bindingInfo;
  for(uint64_t i = 0; i < dsLayoutBindings->Size(); i++)
  {
    BindingInfo dsInfo;
    dsInfo.binding = dsLayoutBindings->At(i)->At(0)->U64();
    dsInfo.type = dsLayoutBindings->At(i)->At(1)->U64();
    dsInfo.count = dsLayoutBindings->At(i)->At(2)->U64();
    dsInfo.typeStr = dsLayoutBindings->At(i)->At(1)->Str();
    dsInfo.index = i;
    bindingInfo.push_back(dsInfo);
  }
  std::sort(bindingInfo.begin(), bindingInfo.end());

  uint64_t initBindings_index = bindingInfo[0].binding;
  int lastLayoutBinding = static_cast<int>(initBindings_index);

  for(size_t i = 0; i < bindingInfo.size(); i++)
  {
    uint64_t layoutIndex = bindingInfo[i].index;
    int layoutBinding = static_cast<int>(bindingInfo[i].binding);
    RDCASSERT(layoutBinding >= lastLayoutBinding);
    // descriptor set layouts can be sparse, such that only three bindings exist
    // but they are at 0, 5 and 10. If descriptor set bindings are sparse, for
    // example 5, followed by a 10, skip '10-5' descriptor bindingInfo.
    initBindings_index += std::max(layoutBinding - lastLayoutBinding - 1, 0);
    lastLayoutBinding = layoutBinding;

    ExtObject *dsLayoutBinding = dsLayoutBindings->At(layoutIndex);
    for(uint64_t j = 0; j < bindingInfo[i].count; j++, initBindings_index++)
    {
      ExtObject *dsImmutSamplers = dsLayoutBinding->At(4);
      // Either there were no Immutable Samplers OR there is an immutable sampler for each element.
      RDCASSERT(dsImmutSamplers->Size() == 0 || dsImmutSamplers->Size() == bindingInfo[i].count);
      if(dsImmutSamplers->Size() > 0)
        dsImmutSamplers = dsImmutSamplers->At(j);

      switch(bindingInfo[i].type)
      {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        {    // use buffer info
          ExtObject *buffer = initBindings->At(initBindings_index)->At(0);
          if(!FilterBufferInfoDescSet(buffer->At(0)->U64(), buffer->At(1)->U64(), buffer->At(2)))
          {
            continue;
          }
        }
        break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        {    // use image info
          ExtObject *image = initBindings->At(initBindings_index)->At(1);
          if(!FilterImageInfoDescSet(bindingInfo[i].type, image->At(1)->U64(), image->At(0)->U64(),
                                     dsImmutSamplers->U64(), image->At(2), image))
          {
            continue;
          }
        }
        break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        {    // use texel view info
          ExtObject *texelview = initBindings->At(initBindings_index)->At(2);
          if(!FilterTexelBufferViewDescSet(texelview->U64()))
          {
            continue;
          }
        }
        break;
      }

      // If desc set initialization data is invalid, the loop execution won't
      // get to this point.
      ExtObject *extBinding = new ExtObject("binding", "uint64_t", bindingInfo[i].binding);
      ExtObject *extType =
          new ExtObject("type", "VkDescriptorType", bindingInfo[i].type, bindingInfo[i].typeStr);
      ExtObject *extArrayElement = new ExtObject("arrayElement", "uint64_t", j);
      initBindings->At(initBindings_index)->PushOne(extBinding);
      initBindings->At(initBindings_index)->PushOne(extType);
      initBindings->At(initBindings_index)->PushOne(extArrayElement);
    }
  }

  RDCASSERT(initBindings_index == initBindings->Size());

  // Now remove all elements from initBindings elements that haven't changed
  // in size as they are not used by this descriptor set.
  for(uint64_t i = 0; i < initBindings->Size();)
  {
    if(initBindings->At(i)->Size() == initBindingsSizes[i])
    {
      initBindings->RemoveOne(initBindings->At(i));
    }
    else
    {
      i++;
    }
  }

  return initBindings->Size() > 0;
}

}    // namespace vk_cpp_codec