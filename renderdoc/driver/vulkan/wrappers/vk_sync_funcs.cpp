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

/*
 * Events and fences need careful handling.
 *
 * Primary goal by far is correctness - these primitives are used to synchronise
 * operations between GPU-CPU and GPU-GPU, and we need to be sure that we don't
 * introduce any bugs with bad handling.
 *
 * Secondary goal and worth compromising is to be efficient in replaying them.
 *
 * Fences are comparatively 'easy'. Since the GPU can't wait on them, for the
 * moment we just implement fences as-is and do a hard sync via DeviceWaitIdle
 * whenever the status of a fence would have been fetched on the GPU. Obviously
 * this is very conservative, but it's correct and it doesn't impact efficiency
 * too badly (The replay can be bottlenecked in different ways to the real
 * application, and often has different realtime requirements for the actual
 * frame replay).
 *
 * Events are harder because the GPU can wait on them. We need to be particularly
 * careful the GPU never waits on an event that will never become set, or the GPU
 * will lock up.
 *
 * For now the implementation is simple, conservative and inefficient. We keep
 * events Set always, never replaying any Reset (CPU or GPU). This means any
 * wait will always succeed on the GPU.
 *
 * On the CPU-side with GetEventStatus we do another hard sync with
 * DeviceWaitIdle.
 *
 * On the GPU-side, whenever a command buffer contains a CmdWaitEvents we
 * create an event, reset it, and call CmdSetEvent right before the
 * CmdWaitEvents. This should provide the strictest possible ordering guarantee
 * for the CmdWaitEvents (since the event set it was waiting on must have
 * happened at or before where we are setting the event, so our event is as or
 * more conservative than the original event).
 *
 * In future it would be nice to save the state of events at the start of
 * the frame and restore them, via GetEventStatus/SetEvent/ResetEvent. However
 * this will not be sufficient to make sure all events are set when they should
 * be - e.g. an event which is reset at start of frame, but a GPU cmd buffer is
 * in-flight that will set it, but hasn't been recorded as part of the frame.
 * Then a cmd buffer in the frame which does CmdWaitEvents will never have that
 * event set. I'm not sure if there's a way around this, we might just have to
 * make slight improvements to the current method by ensuring events are
 * properly hard-synced on the GPU.
 *
 */

bool WrappedVulkan::Serialise_vkCreateFence(Serialiser *localSerialiser, VkDevice device,
                                            const VkFenceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkFenceCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pFence));

  if(m_State == READING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    VkFence fence = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), &info, NULL, &fence);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), fence);
      GetResourceManager()->AddLiveResource(id, fence);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateFence(VkDevice device, const VkFenceCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
  VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), pCreateInfo, pAllocator, pFence);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFence);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_FENCE);
        Serialise_vkCreateFence(localSerialiser, device, pCreateInfo, NULL, pFence);

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

bool WrappedVulkan::Serialise_vkGetFenceStatus(Serialiser *localSerialiser, VkDevice device,
                                               VkFence fence)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, fid, GetResID(fence));

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkGetFenceStatus(VkDevice device, VkFence fence)
{
  SCOPED_DBG_SINK();

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

bool WrappedVulkan::Serialise_vkResetFences(Serialiser *localSerialiser, VkDevice device,
                                            uint32_t fenceCount, const VkFence *pFences)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
  SERIALISE_ELEMENT(uint32_t, count, fenceCount);

  Serialise_DebugMessages(localSerialiser, false);

  vector<VkFence> fences;

  for(uint32_t i = 0; i < count; i++)
  {
    ResourceId fence;
    if(m_State >= WRITING)
      fence = GetResID(pFences[i]);

    localSerialiser->Serialise("pFences[]", fence);

    if(m_State < WRITING && GetResourceManager()->HasLiveResource(fence))
      fences.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkFence>(fence)));
  }

  if(m_State < WRITING && !fences.empty())
  {
    // we don't care about fence states ourselves as we cannot record them perfectly and just
    // do full waitidle flushes.
    device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

    // since we don't have anything signalling or waiting on fences, don't bother to reset them
    // either
    // ObjDisp(device)->ResetFences(Unwrap(device), (uint32_t)fences.size(), &fences[0]);
  }

  return true;
}

VkResult WrappedVulkan::vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
  SCOPED_DBG_SINK();

  VkFence *unwrapped = GetTempArray<VkFence>(fenceCount);
  for(uint32_t i = 0; i < fenceCount; i++)
    unwrapped[i] = Unwrap(pFences[i]);
  VkResult ret = ObjDisp(device)->ResetFences(Unwrap(device), fenceCount, unwrapped);

  if(m_State >= WRITING_CAPFRAME)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(RESET_FENCE);
    Serialise_vkResetFences(localSerialiser, device, fenceCount, pFences);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkWaitForFences(Serialiser *localSerialiser, VkDevice device,
                                              uint32_t fenceCount, const VkFence *pFences,
                                              VkBool32 waitAll, uint64_t timeout)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
  SERIALISE_ELEMENT(VkBool32, wait, waitAll);
  SERIALISE_ELEMENT(uint64_t, tmout, timeout);
  SERIALISE_ELEMENT(uint32_t, count, fenceCount);

  Serialise_DebugMessages(localSerialiser, false);

  vector<VkFence> fences;

  for(uint32_t i = 0; i < count; i++)
  {
    ResourceId fence;
    if(m_State >= WRITING)
      fence = GetResID(pFences[i]);

    localSerialiser->Serialise("pFences[]", fence);

    if(m_State < WRITING && GetResourceManager()->HasLiveResource(fence))
      fences.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkFence>(fence)));
  }

  if(m_State < WRITING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkWaitForFences(VkDevice device, uint32_t fenceCount,
                                        const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
{
  SCOPED_DBG_SINK();

  VkFence *unwrapped = GetTempArray<VkFence>(fenceCount);
  for(uint32_t i = 0; i < fenceCount; i++)
    unwrapped[i] = Unwrap(pFences[i]);
  VkResult ret =
      ObjDisp(device)->WaitForFences(Unwrap(device), fenceCount, unwrapped, waitAll, timeout);

  if(m_State >= WRITING_CAPFRAME)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(WAIT_FENCES);
    Serialise_vkWaitForFences(localSerialiser, device, fenceCount, pFences, waitAll, timeout);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkCreateEvent(Serialiser *localSerialiser, VkDevice device,
                                            const VkEventCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkEventCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pEvent));

  if(m_State == READING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    VkEvent ev = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateEvent(Unwrap(device), &info, NULL, &ev);

    // see top of this file for current event/fence handling
    ObjDisp(device)->SetEvent(Unwrap(device), ev);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), ev);
      GetResourceManager()->AddLiveResource(id, ev);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateEvent(VkDevice device, const VkEventCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
  VkResult ret = ObjDisp(device)->CreateEvent(Unwrap(device), pCreateInfo, pAllocator, pEvent);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pEvent);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_EVENT);
        Serialise_vkCreateEvent(localSerialiser, device, pCreateInfo, NULL, pEvent);

        chunk = scope.Get();
      }

      VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pEvent);
      record->AddChunk(chunk);
    }
    else
    {
      GetResourceManager()->AddLiveResource(id, *pEvent);
    }
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkSetEvent(Serialiser *localSerialiser, VkDevice device, VkEvent event)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
  {
    // see top of this file for current event/fence handling
  }

  return true;
}

VkResult WrappedVulkan::vkSetEvent(VkDevice device, VkEvent event)
{
  SCOPED_DBG_SINK();

  VkResult ret = ObjDisp(device)->SetEvent(Unwrap(device), Unwrap(event));

  if(m_State >= WRITING_CAPFRAME)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(SET_EVENT);
    Serialise_vkSetEvent(localSerialiser, device, event);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkResetEvent(Serialiser *localSerialiser, VkDevice device, VkEvent event)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
  {
    // see top of this file for current event/fence handling
  }

  return true;
}

VkResult WrappedVulkan::vkResetEvent(VkDevice device, VkEvent event)
{
  SCOPED_DBG_SINK();

  VkResult ret = ObjDisp(device)->ResetEvent(Unwrap(device), Unwrap(event));

  if(m_State >= WRITING_CAPFRAME)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(RESET_EVENT);
    Serialise_vkResetEvent(localSerialiser, device, event);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkGetEventStatus(Serialiser *localSerialiser, VkDevice device,
                                               VkEvent event)
{
  SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
  SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(id);

    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkGetEventStatus(VkDevice device, VkEvent event)
{
  SCOPED_DBG_SINK();

  VkResult ret = ObjDisp(device)->GetEventStatus(Unwrap(device), Unwrap(event));

  if(m_State >= WRITING_CAPFRAME)
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(GET_EVENT_STATUS);
    Serialise_vkGetEventStatus(localSerialiser, device, event);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

bool WrappedVulkan::Serialise_vkCreateSemaphore(Serialiser *localSerialiser, VkDevice device,
                                                const VkSemaphoreCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkSemaphore *pSemaphore)
{
  SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
  SERIALISE_ELEMENT(VkSemaphoreCreateInfo, info, *pCreateInfo);
  SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSemaphore));

  if(m_State == READING)
  {
    device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
    VkSemaphore sem = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), &info, NULL, &sem);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
    }
    else
    {
      ResourceId live;

      if(GetResourceManager()->HasWrapper(ToTypedHandle(sem)))
      {
        live = GetResourceManager()->GetNonDispWrapper(sem)->id;

        RDCWARN(
            "On replay, semaphore got a duplicate handle - maybe a bug, or it could be an "
            "indication of an implementation that doesn't use semaphores");

        // destroy this instance of the duplicate, as we must have matching create/destroy
        // calls and there won't be a wrapped resource hanging around to destroy this one.
        ObjDisp(device)->DestroySemaphore(Unwrap(device), sem, NULL);

        // whenever the new ID is requested, return the old ID, via replacements.
        GetResourceManager()->ReplaceResource(id, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), sem);
        GetResourceManager()->AddLiveResource(id, sem);
      }
    }
  }

  return true;
}

VkResult WrappedVulkan::vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkSemaphore *pSemaphore)
{
  VkResult ret =
      ObjDisp(device)->CreateSemaphore(Unwrap(device), pCreateInfo, pAllocator, pSemaphore);

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSemaphore);

    if(m_State >= WRITING)
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CONTEXT(CREATE_SEMAPHORE);
        Serialise_vkCreateSemaphore(localSerialiser, device, pCreateInfo, NULL, pSemaphore);

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

bool WrappedVulkan::Serialise_vkCmdSetEvent(Serialiser *localSerialiser, VkCommandBuffer cmdBuffer,
                                            VkEvent event, VkPipelineStageFlags stageMask)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
  SERIALISE_ELEMENT(VkPipelineStageFlagBits, mask, (VkPipelineStageFlagBits)stageMask);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  // see top of this file for current event/fence handling

  if(m_State == EXECUTING)
  {
    event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);

    ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetEvent(VkCommandBuffer cmdBuffer, VkEvent event,
                                  VkPipelineStageFlags stageMask)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), Unwrap(event), stageMask);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(CMD_SET_EVENT);
    Serialise_vkCmdSetEvent(localSerialiser, cmdBuffer, event, stageMask);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
  }
}

bool WrappedVulkan::Serialise_vkCmdResetEvent(Serialiser *localSerialiser, VkCommandBuffer cmdBuffer,
                                              VkEvent event, VkPipelineStageFlags stageMask)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(ResourceId, eid, GetResID(event));
  SERIALISE_ELEMENT(VkPipelineStageFlagBits, mask, (VkPipelineStageFlagBits)stageMask);

  Serialise_DebugMessages(localSerialiser, false);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  // see top of this file for current event/fence handling

  if(m_State == EXECUTING)
  {
    event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);

    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);
      // ObjDisp(cmdBuffer)->CmdResetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);
    event = GetResourceManager()->GetLiveHandle<VkEvent>(eid);

    // ObjDisp(cmdBuffer)->CmdResetEvent(Unwrap(cmdBuffer), Unwrap(event), mask);
  }

  return true;
}

void WrappedVulkan::vkCmdResetEvent(VkCommandBuffer cmdBuffer, VkEvent event,
                                    VkPipelineStageFlags stageMask)
{
  SCOPED_DBG_SINK();

  ObjDisp(cmdBuffer)->CmdResetEvent(Unwrap(cmdBuffer), Unwrap(event), stageMask);

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(CMD_RESET_EVENT);
    Serialise_vkCmdResetEvent(localSerialiser, cmdBuffer, event, stageMask);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
  }
}

bool WrappedVulkan::Serialise_vkCmdWaitEvents(
    Serialiser *localSerialiser, VkCommandBuffer cmdBuffer, uint32_t eventCount,
    const VkEvent *pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
  SERIALISE_ELEMENT(VkPipelineStageFlagBits, srcStages, (VkPipelineStageFlagBits)srcStageMask);
  SERIALISE_ELEMENT(VkPipelineStageFlagBits, destStages, (VkPipelineStageFlagBits)dstStageMask);

  if(m_State < WRITING)
    m_LastCmdBufferID = cmdid;

  // we don't serialise the original events as we are going to replace this
  // with our own

  SERIALISE_ELEMENT(uint32_t, memCount, memoryBarrierCount);
  SERIALISE_ELEMENT(uint32_t, bufCount, bufferMemoryBarrierCount);
  SERIALISE_ELEMENT(uint32_t, imgCount, imageMemoryBarrierCount);

  // we keep the original memory barriers
  SERIALISE_ELEMENT_ARR(VkMemoryBarrier, memBarriers, pMemoryBarriers, memCount);
  SERIALISE_ELEMENT_ARR(VkBufferMemoryBarrier, bufMemBarriers, pBufferMemoryBarriers, bufCount);
  SERIALISE_ELEMENT_ARR(VkImageMemoryBarrier, imgMemBarriers, pImageMemoryBarriers, imgCount);

  vector<VkImageMemoryBarrier> imgBarriers;
  vector<VkBufferMemoryBarrier> bufBarriers;

  // it's possible for buffer or image to be NULL if it refers to a resource that is otherwise
  // not in the log (barriers do not mark resources referenced). If the resource in question does
  // not exist, then it's safe to skip this barrier.

  if(m_State < WRITING)
  {
    for(uint32_t i = 0; i < bufCount; i++)
      if(bufMemBarriers[i].buffer != VK_NULL_HANDLE)
        bufBarriers.push_back(bufMemBarriers[i]);

    for(uint32_t i = 0; i < imgCount; i++)
    {
      if(imgMemBarriers[i].image != VK_NULL_HANDLE)
      {
        imgBarriers.push_back(imgMemBarriers[i]);
        ReplacePresentableImageLayout(imgBarriers.back().oldLayout);
        ReplacePresentableImageLayout(imgBarriers.back().newLayout);

        ReplaceExternalQueueFamily(imgBarriers.back().srcQueueFamilyIndex,
                                   imgBarriers.back().dstQueueFamilyIndex);
      }
    }
  }

  SAFE_DELETE_ARRAY(bufMemBarriers);
  SAFE_DELETE_ARRAY(imgMemBarriers);

  // see top of this file for current event/fence handling

  if(m_State == EXECUTING)
  {
    if(ShouldRerecordCmd(cmdid) && InRerecordRange(cmdid))
    {
      cmdBuffer = RerecordCmdBuf(cmdid);

      VkEventCreateInfo evInfo = {
          VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, NULL, 0,
      };

      VkEvent ev = VK_NULL_HANDLE;
      ObjDisp(cmdBuffer)->CreateEvent(Unwrap(GetDev()), &evInfo, NULL, &ev);
      // don't wrap this event

      ObjDisp(cmdBuffer)->ResetEvent(Unwrap(GetDev()), ev);
      ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), ev, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

      ObjDisp(cmdBuffer)->CmdWaitEvents(Unwrap(cmdBuffer), 1, &ev, (VkPipelineStageFlags)srcStages,
                                        (VkPipelineStageFlags)destStages, memCount, memBarriers,
                                        (uint32_t)bufBarriers.size(), &bufBarriers[0],
                                        (uint32_t)imgBarriers.size(), &imgBarriers[0]);

      // register to clean this event up once we're done replaying this section of the log
      m_CleanupEvents.push_back(ev);

      ResourceId cmd = GetResID(RerecordCmdBuf(cmdid));
      GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                           (uint32_t)imgBarriers.size(), &imgBarriers[0]);
    }
  }
  else if(m_State == READING)
  {
    cmdBuffer = GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdid);

    VkEventCreateInfo evInfo = {
        VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, NULL, 0,
    };

    VkEvent ev = VK_NULL_HANDLE;
    ObjDisp(cmdBuffer)->CreateEvent(Unwrap(GetDev()), &evInfo, NULL, &ev);
    // don't wrap this event

    ObjDisp(cmdBuffer)->ResetEvent(Unwrap(GetDev()), ev);
    ObjDisp(cmdBuffer)->CmdSetEvent(Unwrap(cmdBuffer), ev, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    ObjDisp(cmdBuffer)->CmdWaitEvents(Unwrap(cmdBuffer), 1, &ev, (VkPipelineStageFlags)srcStages,
                                      (VkPipelineStageFlags)destStages, memCount, memBarriers,
                                      (uint32_t)bufBarriers.size(), &bufBarriers[0],
                                      (uint32_t)imgBarriers.size(), &imgBarriers[0]);

    // since we cache and replay this command buffer we can't clean up this event just when we're
    // done replaying this section. We have to keep this event until shutdown
    m_PersistentEvents.push_back(ev);

    ResourceId cmd = GetResID(cmdBuffer);
    GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                         (uint32_t)imgBarriers.size(), &imgBarriers[0]);
  }

  SAFE_DELETE_ARRAY(memBarriers);

  return true;
}

void WrappedVulkan::vkCmdWaitEvents(VkCommandBuffer cmdBuffer, uint32_t eventCount,
                                    const VkEvent *pEvents, VkPipelineStageFlags srcStageMask,
                                    VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount,
                                    const VkMemoryBarrier *pMemoryBarriers,
                                    uint32_t bufferMemoryBarrierCount,
                                    const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                    uint32_t imageMemoryBarrierCount,
                                    const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  {
    byte *memory = GetTempMemory(sizeof(VkEvent) * eventCount +
                                 sizeof(VkBufferMemoryBarrier) * bufferMemoryBarrierCount +
                                 sizeof(VkImageMemoryBarrier) * imageMemoryBarrierCount);

    VkEvent *ev = (VkEvent *)memory;
    VkImageMemoryBarrier *im = (VkImageMemoryBarrier *)(ev + eventCount);
    VkBufferMemoryBarrier *buf = (VkBufferMemoryBarrier *)(im + imageMemoryBarrierCount);

    for(uint32_t i = 0; i < eventCount; i++)
      ev[i] = Unwrap(pEvents[i]);

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

    ObjDisp(cmdBuffer)->CmdWaitEvents(Unwrap(cmdBuffer), eventCount, ev, srcStageMask, dstStageMask,
                                      memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
                                      buf, imageMemoryBarrierCount, im);
  }

  if(m_State >= WRITING)
  {
    VkResourceRecord *record = GetRecord(cmdBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CONTEXT(CMD_WAIT_EVENTS);
    Serialise_vkCmdWaitEvents(localSerialiser, cmdBuffer, eventCount, pEvents, srcStageMask,
                              dstStageMask, memoryBarrierCount, pMemoryBarriers,
                              bufferMemoryBarrierCount, pBufferMemoryBarriers,
                              imageMemoryBarrierCount, pImageMemoryBarriers);

    if(imageMemoryBarrierCount > 0)
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      GetResourceManager()->RecordBarriers(GetRecord(cmdBuffer)->cmdInfo->imgbarriers, m_ImageLayouts,
                                           imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    record->AddChunk(scope.Get());
    for(uint32_t i = 0; i < eventCount; i++)
      record->MarkResourceFrameReferenced(GetResID(pEvents[i]), eFrameRef_Read);
  }
}

VkResult WrappedVulkan::vkImportSemaphoreFdKHX(VkDevice device,
                                               const VkImportSemaphoreFdInfoKHX *pImportSemaphoreFdInfo)
{
  VkImportSemaphoreFdInfoKHX unwrappedInfo = *pImportSemaphoreFdInfo;
  unwrappedInfo.semaphore = Unwrap(unwrappedInfo.semaphore);

  return ObjDisp(device)->ImportSemaphoreFdKHX(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetSemaphoreFdKHX(VkDevice device, VkSemaphore semaphore,
                                            VkExternalSemaphoreHandleTypeFlagBitsKHX handleType,
                                            int *pFd)
{
  return ObjDisp(device)->GetSemaphoreFdKHX(Unwrap(device), Unwrap(semaphore), handleType, pFd);
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)

VkResult WrappedVulkan::vkImportSemaphoreWin32HandleKHX(
    VkDevice device, const VkImportSemaphoreWin32HandleInfoKHX *pImportSemaphoreWin32HandleInfo)
{
  VkImportSemaphoreWin32HandleInfoKHX unwrappedInfo = *pImportSemaphoreWin32HandleInfo;
  unwrappedInfo.semaphore = Unwrap(unwrappedInfo.semaphore);

  return ObjDisp(device)->ImportSemaphoreWin32HandleKHX(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetSemaphoreWin32HandleKHX(VkDevice device, VkSemaphore semaphore,
                                                     VkExternalSemaphoreHandleTypeFlagBitsKHX handleType,
                                                     HANDLE *pHandle)
{
  return ObjDisp(device)->GetSemaphoreWin32HandleKHX(Unwrap(device), Unwrap(semaphore), handleType,
                                                     pHandle);
}

#endif
