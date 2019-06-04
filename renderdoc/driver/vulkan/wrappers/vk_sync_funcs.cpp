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

void WrappedVulkan::RemapQueueFamilyIndices(uint32_t &srcQueueFamily, uint32_t &dstQueueFamily)
{
  if(srcQueueFamily == VK_QUEUE_FAMILY_EXTERNAL || dstQueueFamily == VK_QUEUE_FAMILY_EXTERNAL ||
     srcQueueFamily == VK_QUEUE_FAMILY_FOREIGN_EXT || dstQueueFamily == VK_QUEUE_FAMILY_FOREIGN_EXT)
  {
    // we should ignore this family transition since we're not synchronising with an
    // external access.
    srcQueueFamily = dstQueueFamily = VK_QUEUE_FAMILY_IGNORED;
  }
  else
  {
    if(srcQueueFamily != VK_QUEUE_FAMILY_IGNORED)
    {
      RDCASSERT(srcQueueFamily < ARRAY_COUNT(m_QueueRemapping), srcQueueFamily);
      srcQueueFamily = m_QueueRemapping[srcQueueFamily][0].family;
    }

    if(dstQueueFamily != VK_QUEUE_FAMILY_IGNORED)
    {
      RDCASSERT(dstQueueFamily < ARRAY_COUNT(m_QueueRemapping), dstQueueFamily);
      dstQueueFamily = m_QueueRemapping[dstQueueFamily][0].family;
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateFence(SerialiserType &ser, VkDevice device,
                                            const VkFenceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Fence, GetResID(*pFence)).TypedAs("VkFence"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkFence fence = VK_NULL_HANDLE;

    VkFenceCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkFenceCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateFence(Unwrap(device), &patched, NULL, &fence);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), fence);
      GetResourceManager()->AddLiveResource(Fence, fence);
    }

    AddResource(Fence, ResourceType::Sync, "Fence");
    DerivedResource(device, Fence);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateFence(VkDevice device, const VkFenceCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
  VkFenceCreateInfo info = *pCreateInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(info.pNext));

  UnwrapNextChain(m_State, "VkFenceCreateInfo", tempMem, (VkBaseInStructure *)&info);

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->CreateFence(Unwrap(device), &info, pAllocator, pFence));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFence);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateFence);
        Serialise_vkCreateFence(ser, device, pCreateInfo, NULL, pFence);

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetFenceStatus(SerialiserType &ser, VkDevice device, VkFence fence)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(fence);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkGetFenceStatus(VkDevice device, VkFence fence)
{
  SCOPED_DBG_SINK();

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->GetFenceStatus(Unwrap(device), Unwrap(fence)));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkGetFenceStatus);
    Serialise_vkGetFenceStatus(ser, device, fence);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkResetFences(SerialiserType &ser, VkDevice device,
                                            uint32_t fenceCount, const VkFence *pFences)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(fenceCount);
  SERIALISE_ELEMENT_ARRAY(pFences, fenceCount);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // we don't care about fence states ourselves as we cannot record them perfectly and just
    // do full waitidle flushes.

    // since we don't have anything signalling or waiting on fences, don't bother to reset them
    // either.
    // ObjDisp(device)->ResetFences(Unwrap(device), fenceCount, pFences);
  }

  return true;
}

VkResult WrappedVulkan::vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
  SCOPED_DBG_SINK();

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->ResetFences(Unwrap(device), fenceCount,
                                                         UnwrapArray(pFences, fenceCount)));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkResetFences);
    Serialise_vkResetFences(ser, device, fenceCount, pFences);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    for(uint32_t i = 0; i < fenceCount; i++)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFences[i]), eFrameRef_Read);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkWaitForFences(SerialiserType &ser, VkDevice device,
                                              uint32_t fenceCount, const VkFence *pFences,
                                              VkBool32 waitAll, uint64_t timeout)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(fenceCount);
  SERIALISE_ELEMENT_ARRAY(pFences, fenceCount);
  SERIALISE_ELEMENT(waitAll);
  SERIALISE_ELEMENT(timeout);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkWaitForFences(VkDevice device, uint32_t fenceCount,
                                        const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
{
  SCOPED_DBG_SINK();

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->WaitForFences(Unwrap(device), fenceCount,
                                                           UnwrapArray(pFences, fenceCount),
                                                           waitAll, timeout));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkWaitForFences);
    Serialise_vkWaitForFences(ser, device, fenceCount, pFences, waitAll, timeout);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    for(uint32_t i = 0; i < fenceCount; i++)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(pFences[i]), eFrameRef_Read);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateEvent(SerialiserType &ser, VkDevice device,
                                            const VkEventCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Event, GetResID(*pEvent)).TypedAs("VkEvent"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkEvent ev = VK_NULL_HANDLE;

    VkResult ret = ObjDisp(device)->CreateEvent(Unwrap(device), &CreateInfo, NULL, &ev);

    // see top of this file for current event/fence handling
    ObjDisp(device)->SetEvent(Unwrap(device), ev);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
    }
    else
    {
      ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), ev);
      GetResourceManager()->AddLiveResource(Event, ev);
    }

    AddResource(Event, ResourceType::Sync, "Event");
    DerivedResource(device, Event);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateEvent(VkDevice device, const VkEventCreateInfo *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateEvent(Unwrap(device), pCreateInfo, pAllocator, pEvent));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pEvent);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateEvent);
        Serialise_vkCreateEvent(ser, device, pCreateInfo, NULL, pEvent);

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkSetEvent(SerialiserType &ser, VkDevice device, VkEvent event)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(event);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // see top of this file for current event/fence handling
  }

  return true;
}

VkResult WrappedVulkan::vkSetEvent(VkDevice device, VkEvent event)
{
  SCOPED_DBG_SINK();

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->SetEvent(Unwrap(device), Unwrap(event)));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkSetEvent);
    Serialise_vkSetEvent(ser, device, event);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkResetEvent(SerialiserType &ser, VkDevice device, VkEvent event)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(event);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // see top of this file for current event/fence handling
  }

  return true;
}

VkResult WrappedVulkan::vkResetEvent(VkDevice device, VkEvent event)
{
  SCOPED_DBG_SINK();

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->ResetEvent(Unwrap(device), Unwrap(event)));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkResetEvent);
    Serialise_vkResetEvent(ser, device, event);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetEventStatus(SerialiserType &ser, VkDevice device, VkEvent event)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(event);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
  }

  return true;
}

VkResult WrappedVulkan::vkGetEventStatus(VkDevice device, VkEvent event)
{
  SCOPED_DBG_SINK();

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(device)->GetEventStatus(Unwrap(device), Unwrap(event)));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkGetEventStatus);
    Serialise_vkGetEventStatus(ser, device, event);

    m_FrameCaptureRecord->AddChunk(scope.Get());
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCreateSemaphore(SerialiserType &ser, VkDevice device,
                                                const VkSemaphoreCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkSemaphore *pSemaphore)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(CreateInfo, *pCreateInfo);
  SERIALISE_ELEMENT_OPT(pAllocator);
  SERIALISE_ELEMENT_LOCAL(Semaphore, GetResID(*pSemaphore)).TypedAs("VkSemaphore"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkSemaphore sem = VK_NULL_HANDLE;

    VkSemaphoreCreateInfo patched = CreateInfo;

    byte *tempMem = GetTempMemory(GetNextPatchSize(patched.pNext));

    UnwrapNextChain(m_State, "VkSemaphoreCreateInfo", tempMem, (VkBaseInStructure *)&patched);

    VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), &patched, NULL, &sem);

    if(ret != VK_SUCCESS)
    {
      RDCERR("Failed on resource serialise-creation, VkResult: %s", ToStr(ret).c_str());
      return false;
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
        GetResourceManager()->ReplaceResource(Semaphore, GetResourceManager()->GetOriginalID(live));
      }
      else
      {
        live = GetResourceManager()->WrapResource(Unwrap(device), sem);
        GetResourceManager()->AddLiveResource(Semaphore, sem);
      }
    }

    AddResource(Semaphore, ResourceType::Sync, "Semaphore");
    DerivedResource(device, Semaphore);
  }

  return true;
}

VkResult WrappedVulkan::vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkSemaphore *pSemaphore)
{
  VkSemaphoreCreateInfo info = *pCreateInfo;

  byte *tempMem = GetTempMemory(GetNextPatchSize(info.pNext));

  UnwrapNextChain(m_State, "VkSemaphoreCreateInfo", tempMem, (VkBaseInStructure *)&info);

  VkResult ret;
  SERIALISE_TIME_CALL(
      ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), &info, pAllocator, pSemaphore));

  if(ret == VK_SUCCESS)
  {
    ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSemaphore);

    if(IsCaptureMode(m_State))
    {
      Chunk *chunk = NULL;

      {
        CACHE_THREAD_SERIALISER();

        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCreateSemaphore);
        Serialise_vkCreateSemaphore(ser, device, pCreateInfo, NULL, pSemaphore);

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdSetEvent(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                            VkEvent event, VkPipelineStageFlags stageMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(event);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits, stageMask).TypedAs("VkPipelineStageFlags"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // see top of this file for current event/fence handling

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
      ObjDisp(commandBuffer)->CmdSetEvent(Unwrap(commandBuffer), Unwrap(event), stageMask);
  }

  return true;
}

void WrappedVulkan::vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event,
                                  VkPipelineStageFlags stageMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdSetEvent(Unwrap(commandBuffer), Unwrap(event), stageMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdSetEvent);
    Serialise_vkCmdSetEvent(ser, commandBuffer, event, stageMask);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdResetEvent(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              VkEvent event, VkPipelineStageFlags stageMask)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(event);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits, stageMask).TypedAs("VkPipelineStageFlags"_lit);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // see top of this file for current event/fence handling

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;
    }

    if(commandBuffer != VK_NULL_HANDLE)
    {
      // ObjDisp(commandBuffer)->CmdResetEvent(Unwrap(commandBuffer), Unwrap(event), mask);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event,
                                    VkPipelineStageFlags stageMask)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdResetEvent(Unwrap(commandBuffer), Unwrap(event), stageMask));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdResetEvent);
    Serialise_vkCmdResetEvent(ser, commandBuffer, event, stageMask);

    record->AddChunk(scope.Get());
    record->MarkResourceFrameReferenced(GetResID(event), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdWaitEvents(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  SERIALISE_ELEMENT(commandBuffer);

  // we serialise the original events even though we are going to replace them with our own
  SERIALISE_ELEMENT(eventCount);
  SERIALISE_ELEMENT_ARRAY(pEvents, eventCount);

  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits, srcStageMask)
      .TypedAs("VkPipelineStageFlags"_lit);
  SERIALISE_ELEMENT_TYPED(VkPipelineStageFlagBits, dstStageMask)
      .TypedAs("VkPipelineStageFlags"_lit);

  SERIALISE_ELEMENT(memoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pMemoryBarriers, memoryBarrierCount);
  SERIALISE_ELEMENT(bufferMemoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pBufferMemoryBarriers, bufferMemoryBarrierCount);
  SERIALISE_ELEMENT(imageMemoryBarrierCount);
  SERIALISE_ELEMENT_ARRAY(pImageMemoryBarriers, imageMemoryBarrierCount);

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
      }
    }

    // see top of this file for current event/fence handling

    VkEventCreateInfo evInfo = {
        VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, NULL, 0,
    };

    VkEvent ev = VK_NULL_HANDLE;
    ObjDisp(commandBuffer)->CreateEvent(Unwrap(GetDev()), &evInfo, NULL, &ev);
    // don't wrap this event

    ObjDisp(commandBuffer)->ResetEvent(Unwrap(GetDev()), ev);

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);
      else
        commandBuffer = VK_NULL_HANDLE;

      // register to clean this event up once we're done replaying this section of the log
      m_CleanupEvents.push_back(ev);
    }
    else
    {
      // since we cache and replay this command buffer we can't clean up this event just when we're
      // done replaying this section. We have to keep this event until shutdown
      m_PersistentEvents.push_back(ev);
    }

    ResourceId cmd = GetResID(commandBuffer);
    GetResourceManager()->RecordBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts,
                                         (uint32_t)imgBarriers.size(), &imgBarriers[0]);

    if(commandBuffer != VK_NULL_HANDLE)
    {
      // now sanitise layouts before passing to vulkan
      for(VkImageMemoryBarrier &barrier : imgBarriers)
      {
        SanitiseOldImageLayout(barrier.oldLayout);
        SanitiseNewImageLayout(barrier.newLayout);
      }

      ObjDisp(commandBuffer)->CmdSetEvent(Unwrap(commandBuffer), ev, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
      ObjDisp(commandBuffer)
          ->CmdWaitEvents(Unwrap(commandBuffer), 1, &ev, srcStageMask, dstStageMask,
                          memoryBarrierCount, pMemoryBarriers, (uint32_t)bufBarriers.size(),
                          bufBarriers.data(), (uint32_t)imgBarriers.size(), imgBarriers.data());
    }
  }

  return true;
}

void WrappedVulkan::vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount,
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

    SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                            ->CmdWaitEvents(Unwrap(commandBuffer), eventCount, ev, srcStageMask,
                                            dstStageMask, memoryBarrierCount, pMemoryBarriers,
                                            bufferMemoryBarrierCount, buf, imageMemoryBarrierCount,
                                            im));
  }

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdWaitEvents);
    Serialise_vkCmdWaitEvents(ser, commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask,
                              memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
                              pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);

    if(imageMemoryBarrierCount > 0)
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      GetResourceManager()->RecordBarriers(GetRecord(commandBuffer)->cmdInfo->imgbarriers,
                                           m_ImageLayouts, imageMemoryBarrierCount,
                                           pImageMemoryBarriers);
    }

    record->AddChunk(scope.Get());
    for(uint32_t i = 0; i < eventCount; i++)
      record->MarkResourceFrameReferenced(GetResID(pEvents[i]), eFrameRef_Read);
  }
}

VkResult WrappedVulkan::vkImportSemaphoreFdKHR(VkDevice device,
                                               const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo)
{
  VkImportSemaphoreFdInfoKHR unwrappedInfo = *pImportSemaphoreFdInfo;
  unwrappedInfo.semaphore = Unwrap(unwrappedInfo.semaphore);

  return ObjDisp(device)->ImportSemaphoreFdKHR(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetSemaphoreFdKHR(VkDevice device,
                                            const VkSemaphoreGetFdInfoKHR *pGetFdInfo, int *pFd)
{
  VkSemaphoreGetFdInfoKHR unwrappedInfo = *pGetFdInfo;
  unwrappedInfo.semaphore = Unwrap(unwrappedInfo.semaphore);
  return ObjDisp(device)->GetSemaphoreFdKHR(Unwrap(device), &unwrappedInfo, pFd);
}

VkResult WrappedVulkan::vkImportFenceFdKHR(VkDevice device,
                                           const VkImportFenceFdInfoKHR *pImportFenceFdInfo)
{
  VkImportFenceFdInfoKHR unwrappedInfo = *pImportFenceFdInfo;
  unwrappedInfo.fence = Unwrap(unwrappedInfo.fence);

  return ObjDisp(device)->ImportFenceFdKHR(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR *pGetFdInfo,
                                        int *pFd)
{
  VkFenceGetFdInfoKHR unwrappedInfo = *pGetFdInfo;
  unwrappedInfo.fence = Unwrap(unwrappedInfo.fence);
  return ObjDisp(device)->GetFenceFdKHR(Unwrap(device), &unwrappedInfo, pFd);
}

#if defined(VK_USE_PLATFORM_WIN32_KHR)

VkResult WrappedVulkan::vkImportSemaphoreWin32HandleKHR(
    VkDevice device, const VkImportSemaphoreWin32HandleInfoKHR *pImportSemaphoreWin32HandleInfo)
{
  VkImportSemaphoreWin32HandleInfoKHR unwrappedInfo = *pImportSemaphoreWin32HandleInfo;
  unwrappedInfo.semaphore = Unwrap(unwrappedInfo.semaphore);

  return ObjDisp(device)->ImportSemaphoreWin32HandleKHR(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetSemaphoreWin32HandleKHR(
    VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR *pGetWin32HandleInfo, HANDLE *pHandle)
{
  VkSemaphoreGetWin32HandleInfoKHR unwrappedInfo = *pGetWin32HandleInfo;
  unwrappedInfo.semaphore = Unwrap(unwrappedInfo.semaphore);
  return ObjDisp(device)->GetSemaphoreWin32HandleKHR(Unwrap(device), &unwrappedInfo, pHandle);
}

VkResult WrappedVulkan::vkImportFenceWin32HandleKHR(
    VkDevice device, const VkImportFenceWin32HandleInfoKHR *pImportFenceWin32HandleInfo)
{
  VkImportFenceWin32HandleInfoKHR unwrappedInfo = *pImportFenceWin32HandleInfo;
  unwrappedInfo.fence = Unwrap(unwrappedInfo.fence);

  return ObjDisp(device)->ImportFenceWin32HandleKHR(Unwrap(device), &unwrappedInfo);
}

VkResult WrappedVulkan::vkGetFenceWin32HandleKHR(
    VkDevice device, const VkFenceGetWin32HandleInfoKHR *pGetWin32HandleInfo, HANDLE *pHandle)
{
  VkFenceGetWin32HandleInfoKHR unwrappedInfo = *pGetWin32HandleInfo;
  unwrappedInfo.fence = Unwrap(unwrappedInfo.fence);
  return ObjDisp(device)->GetFenceWin32HandleKHR(Unwrap(device), &unwrappedInfo, pHandle);
}

#endif

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateFence, VkDevice device,
                                const VkFenceCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkFence *pFence);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkGetFenceStatus, VkDevice device, VkFence fence);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkResetFences, VkDevice device, uint32_t fenceCount,
                                const VkFence *pFences);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkWaitForFences, VkDevice device, uint32_t fenceCount,
                                const VkFence *pFences, VkBool32 waitAll, uint64_t timeout);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateEvent, VkDevice device,
                                const VkEventCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkEvent *pEvent);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkSetEvent, VkDevice device, VkEvent event);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkResetEvent, VkDevice device, VkEvent event);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkGetEventStatus, VkDevice device, VkEvent event);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkCreateSemaphore, VkDevice device,
                                const VkSemaphoreCreateInfo *pCreateInfo,
                                const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdSetEvent, VkCommandBuffer commandBuffer, VkEvent event,
                                VkPipelineStageFlags stageMask);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdResetEvent, VkCommandBuffer commandBuffer, VkEvent event,
                                VkPipelineStageFlags stageMask);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdWaitEvents, VkCommandBuffer commandBuffer,
                                uint32_t eventCount, const VkEvent *pEvents,
                                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                                uint32_t bufferMemoryBarrierCount,
                                const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                uint32_t imageMemoryBarrierCount,
                                const VkImageMemoryBarrier *pImageMemoryBarriers);
