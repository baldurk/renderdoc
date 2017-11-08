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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetDeviceQueue(SerialiserType &ser, VkDevice device,
                                               uint32_t queueFamilyIndex, uint32_t queueIndex,
                                               VkQueue *pQueue)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(queueFamilyIndex);
  SERIALISE_ELEMENT(queueIndex);
  SERIALISE_ELEMENT_LOCAL(Queue, GetResID(*pQueue));

  if(IsReplayingAndReading())
  {
    VkQueue queue;
    // MULTIQUEUE - re-map the queue family/index instead of using the supported family
    ObjDisp(device)->GetDeviceQueue(Unwrap(device), m_SupportedQueueFamily, 0, &queue);

    GetResourceManager()->WrapResource(Unwrap(device), queue);
    GetResourceManager()->AddLiveResource(Queue, queue);

    if(queueFamilyIndex == m_QueueFamilyIdx)
    {
      m_Queue = queue;

      // we can now submit any cmds that were queued (e.g. from creating debug
      // manager on vkCreateDevice)
      SubmitCmds();
    }
  }

  return true;
}

void WrappedVulkan::vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
                                     uint32_t queueIndex, VkQueue *pQueue)
{
  ObjDisp(device)->GetDeviceQueue(Unwrap(device), queueFamilyIndex, queueIndex, pQueue);

  if(m_SetDeviceLoaderData)
    m_SetDeviceLoaderData(m_Device, *pQueue);
  else
    SetDispatchTableOverMagicNumber(device, *pQueue);

  RDCASSERT(IsCaptureMode(m_State));

  {
    // it's perfectly valid for enumerate type functions to return the same handle
    // each time. If that happens, we will already have a wrapper created so just
    // return the wrapped object to the user and do nothing else
    if(m_QueueFamilies[queueFamilyIndex][queueIndex] != VK_NULL_HANDLE)
    {
      *pQueue = m_QueueFamilies[queueFamilyIndex][queueIndex];
    }
    else
    {
      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueue);

      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkGetDeviceQueue);
          Serialise_vkGetDeviceQueue(ser, device, queueFamilyIndex, queueIndex, pQueue);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pQueue);
        RDCASSERT(record);

        VkResourceRecord *instrecord = GetRecord(m_Instance);

        // treat queues as pool members of the instance (ie. freed when the instance dies)
        {
          instrecord->LockChunks();
          instrecord->pooledChildren.push_back(record);
          instrecord->UnlockChunks();
        }

        record->AddChunk(chunk);
      }

      m_QueueFamilies[queueFamilyIndex][queueIndex] = *pQueue;

      if(queueFamilyIndex == m_QueueFamilyIdx)
      {
        m_Queue = *pQueue;

        // we can now submit any cmds that were queued (e.g. from creating debug
        // manager on vkCreateDevice)
        SubmitCmds();
      }
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueSubmit(SerialiserType &ser, VkQueue queue, uint32_t submitCount,
                                            const VkSubmitInfo *pSubmits, VkFence fence)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT(fence);
  SERIALISE_ELEMENT_ARRAY(pSubmits, submitCount);

  Serialise_DebugMessages(ser);

  if(IsReplayingAndReading())
  {
    // if we ever waited on any semaphores, wait for idle here.
    bool doWait = false;
    for(uint32_t i = 0; i < submitCount; i++)
      if(pSubmits[i].waitSemaphoreCount > 0)
        doWait = true;

    if(doWait)
      ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

    for(uint32_t sub = 0; sub < submitCount; sub++)
    {
      VkSubmitInfo submitInfo = pSubmits[sub];
      submitInfo.pWaitSemaphores = NULL;
      submitInfo.waitSemaphoreCount = 0;
      submitInfo.pSignalSemaphores = NULL;
      submitInfo.signalSemaphoreCount = 0;

      if(IsLoading(m_State))
      {
        // don't submit the fence, since we have nothing to wait on it being signalled, and we might
        // not have it correctly in the unsignalled state.
        VkSubmitInfo unwrapped = submitInfo;
        unwrapped.pCommandBuffers =
            UnwrapArray(unwrapped.pCommandBuffers, unwrapped.commandBufferCount);
        ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &unwrapped, VK_NULL_HANDLE);

        AddEvent();

        // we're adding multiple events, need to increment ourselves
        m_RootEventID++;

        std::string basename = StringFormat::Fmt("vkQueueSubmit(%u)", submitInfo.commandBufferCount);

        for(uint32_t c = 0; c < submitInfo.commandBufferCount; c++)
        {
          ResourceId cmd =
              GetResourceManager()->GetOriginalID(GetResID(submitInfo.pCommandBuffers[c]));
          GetResourceManager()->ApplyBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers, m_ImageLayouts);

          std::string name = StringFormat::Fmt("=> %s[%u]: vkBeginCommandBuffer(%s)",
                                               basename.c_str(), c, ToStr(cmd).c_str());

          // add a fake marker
          DrawcallDescription draw;
          draw.name = name;
          draw.flags |= DrawFlags::SetMarker;
          AddEvent();
          AddDrawcall(draw, true);
          m_RootEventID++;

          BakedCmdBufferInfo &cmdBufInfo = m_BakedCmdBufferInfo[cmd];

          // insert the baked command buffer in-line into this list of notes, assigning new event
          // and drawIDs
          InsertDrawsAndRefreshIDs(cmdBufInfo.draw->children);

          for(size_t e = 0; e < cmdBufInfo.draw->executedCmds.size(); e++)
          {
            std::vector<Submission> &submits =
                m_Partial[Secondary].cmdBufferSubmits[cmdBufInfo.draw->executedCmds[e]];

            for(size_t s = 0; s < submits.size(); s++)
            {
              if(!submits[s].rebased)
              {
                submits[s].baseEvent += m_RootEventID;
                submits[s].rebased = true;
              }
            }
          }

          for(size_t i = 0; i < cmdBufInfo.debugMessages.size(); i++)
          {
            m_DebugMessages.push_back(cmdBufInfo.debugMessages[i]);
            m_DebugMessages.back().eventID += m_RootEventID;
          }

          // only primary command buffers can be submitted
          m_Partial[Primary].cmdBufferSubmits[cmd].push_back(Submission(m_RootEventID));

          m_RootEventID += cmdBufInfo.eventCount;
          m_RootDrawcallID += cmdBufInfo.drawCount;

          name = StringFormat::Fmt("=> %s[%u]: vkEndCommandBuffer(%s)", basename.c_str(), c,
                                   ToStr(cmd).c_str());
          draw.name = name;
          AddEvent();
          AddDrawcall(draw, true);
          m_RootEventID++;
        }

        // account for the outer loop thinking we've added one event and incrementing,
        // since we've done all the handling ourselves this will be off by one.
        m_RootEventID--;
      }
      else
      {
        // account for the queue submit event
        m_RootEventID++;

        uint32_t startEID = m_RootEventID;

        // advance m_CurEventID to match the events added when reading
        for(uint32_t c = 0; c < submitInfo.commandBufferCount; c++)
        {
          ResourceId cmd =
              GetResourceManager()->GetOriginalID(GetResID(submitInfo.pCommandBuffers[c]));

          // 2 extra for the virtual labels around the command buffer
          m_RootEventID += 2 + m_BakedCmdBufferInfo[cmd].eventCount;
          m_RootDrawcallID += 2 + m_BakedCmdBufferInfo[cmd].drawCount;
        }

        // same accounting for the outer loop as above
        m_RootEventID--;

        if(submitInfo.commandBufferCount == 0)
        {
          // do nothing, don't bother with the logic below
        }
        else if(m_LastEventID <= startEID)
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit no replay %u == %u", m_LastEventID, startEID);
#endif
        }
        else if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit re-recording from %u", m_RootEventID);
#endif

          std::vector<VkCommandBuffer> rerecordedCmds;

          for(uint32_t c = 0; c < submitInfo.commandBufferCount; c++)
          {
            ResourceId cmdId =
                GetResourceManager()->GetOriginalID(GetResID(submitInfo.pCommandBuffers[c]));

            VkCommandBuffer cmd = RerecordCmdBuf(cmdId);
            ResourceId rerecord = GetResID(cmd);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            RDCDEBUG("Queue Submit fully re-recorded replay of %llu, using %llu", cmdId, rerecord);
#endif
            rerecordedCmds.push_back(Unwrap(cmd));

            GetResourceManager()->ApplyBarriers(m_BakedCmdBufferInfo[rerecord].imgbarriers,
                                                m_ImageLayouts);
          }

          submitInfo.commandBufferCount = (uint32_t)rerecordedCmds.size();
          submitInfo.pCommandBuffers = &rerecordedCmds[0];

#if ENABLED(SINGLE_FLUSH_VALIDATE)
          submitInfo.commandBufferCount = 1;
          for(uint32_t i = 0; i < submitInfo.commandBufferCount; i++)
          {
            ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
            submitInfo.pCommandBuffers++;

            FlushQ();
          }
#else
          // don't submit the fence, since we have nothing to wait on it being signalled, and we
          // might not have it correctly in the unsignalled state.
          ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
#endif
        }
        else if(m_LastEventID > startEID && m_LastEventID < m_RootEventID)
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit partial replay %u < %u", m_LastEventID, m_RootEventID);
#endif

          uint32_t eid = startEID;

          std::vector<ResourceId> trimmedCmdIds;
          std::vector<VkCommandBuffer> trimmedCmds;

          for(uint32_t c = 0; c < submitInfo.commandBufferCount; c++)
          {
            ResourceId cmdId =
                GetResourceManager()->GetOriginalID(GetResID(submitInfo.pCommandBuffers[c]));

            // account for the virtual vkBeginCommandBuffer label at the start of the events here
            // so it matches up to baseEvent
            eid++;

            uint32_t end = eid + m_BakedCmdBufferInfo[cmdId].eventCount;

            if(eid == m_Partial[Primary].baseEvent)
            {
              ResourceId partial = GetResID(RerecordCmdBuf(cmdId, Primary));
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("Queue Submit partial replay of %llu at %u, using %llu", cmdId, eid, partial);
#endif
              trimmedCmdIds.push_back(partial);
              trimmedCmds.push_back(Unwrap(RerecordCmdBuf(cmdId, Primary)));
            }
            else if(m_LastEventID >= end)
            {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("Queue Submit full replay %llu", cmdId);
#endif
              trimmedCmdIds.push_back(cmdId);
              trimmedCmds.push_back(
                  Unwrap(GetResourceManager()->GetLiveHandle<VkCommandBuffer>(cmdId)));
            }
            else
            {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("Queue not submitting %llu", cmdId);
#endif
            }

            // 1 extra to account for the virtual end command buffer label (begin is accounted for
            // above)
            eid += 1 + m_BakedCmdBufferInfo[cmdId].eventCount;
          }

          RDCASSERT(trimmedCmds.size() > 0);

          submitInfo.commandBufferCount = (uint32_t)trimmedCmds.size();
          submitInfo.pCommandBuffers = &trimmedCmds[0];

#if ENABLED(SINGLE_FLUSH_VALIDATE)
          submitInfo.commandBufferCount = 1;
          for(uint32_t i = 0; i < submitInfo.commandBufferCount; i++)
          {
            ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
            submitInfo.pCommandBuffers++;

            FlushQ();
          }
#else
          // don't submit the fence, since we have nothing to wait on it being signalled, and we
          // might not have it correctly in the unsignalled state.
          ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
#endif

          for(uint32_t i = 0; i < trimmedCmdIds.size(); i++)
          {
            ResourceId cmd = GetResID(submitInfo.pCommandBuffers[i]);
            GetResourceManager()->ApplyBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers,
                                                m_ImageLayouts);
          }
        }
        else
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit full replay %u >= %u", m_LastEventID, m_RootEventID);
#endif

          VkCommandBuffer *cmds = (VkCommandBuffer *)submitInfo.pCommandBuffers;
          for(uint32_t i = 0; i < submitInfo.commandBufferCount; i++)
          {
            ResourceId cmd = GetResID(submitInfo.pCommandBuffers[i]);
            GetResourceManager()->ApplyBarriers(m_BakedCmdBufferInfo[cmd].imgbarriers,
                                                m_ImageLayouts);

            cmds[i] = Unwrap(cmds[i]);
          }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
          submitInfo.commandBufferCount = 1;
          for(uint32_t i = 0; i < submitInfo.commandBufferCount; i++)
          {
            ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
            submitInfo.pCommandBuffers++;

            FlushQ();
          }
#else
          // don't submit the fence, since we have nothing to wait on it being signalled, and we
          // might not have it correctly in the unsignalled state.
          ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submitInfo, VK_NULL_HANDLE);
#endif
        }
      }

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      FlushQ();
#endif
    }
  }

  return true;
}

void WrappedVulkan::InsertDrawsAndRefreshIDs(vector<VulkanDrawcallTreeNode> &cmdBufNodes)
{
  // assign new drawcall IDs
  for(size_t i = 0; i < cmdBufNodes.size(); i++)
  {
    if(cmdBufNodes[i].draw.flags & DrawFlags::PopMarker)
    {
      // RDCASSERT(GetDrawcallStack().size() > 1);
      if(GetDrawcallStack().size() > 1)
        GetDrawcallStack().pop_back();

      // Skip - pop marker draws aren't processed otherwise, we just apply them to the drawcall
      // stack.
      continue;
    }

    VulkanDrawcallTreeNode n = cmdBufNodes[i];
    n.draw.eventID += m_RootEventID;
    n.draw.drawcallID += m_RootDrawcallID;

    for(APIEvent &ev : n.draw.events)
    {
      ev.eventID += m_RootEventID;
      m_Events.push_back(ev);
    }

    DrawcallUse use(m_Events.back().fileOffset, n.draw.eventID);

    // insert in sorted location
    auto drawit = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
    m_DrawcallUses.insert(drawit, use);

    RDCASSERT(n.children.empty());

    for(auto it = n.resourceUsage.begin(); it != n.resourceUsage.end(); ++it)
    {
      EventUsage u = it->second;
      u.eventID += m_RootEventID;
      m_ResourceUses[it->first].push_back(u);
    }

    GetDrawcallStack().back()->children.push_back(n);

    // if this is a push marker too, step down the drawcall stack
    if(cmdBufNodes[i].draw.flags & DrawFlags::PushMarker)
      GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());
  }
}

VkResult WrappedVulkan::vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                      const VkSubmitInfo *pSubmits, VkFence fence)
{
  SCOPED_DBG_SINK();

  size_t tempmemSize = sizeof(VkSubmitInfo) * submitCount;

  // need to count how many semaphore and command buffer arrays to allocate for
  for(uint32_t i = 0; i < submitCount; i++)
  {
    tempmemSize += pSubmits[i].commandBufferCount * sizeof(VkCommandBuffer);
    tempmemSize += pSubmits[i].signalSemaphoreCount * sizeof(VkSemaphore);
    tempmemSize += pSubmits[i].waitSemaphoreCount * sizeof(VkSemaphore);

    VkGenericStruct *next = (VkGenericStruct *)pSubmits[i].pNext;
    while(next)
    {
      if(next->sType == VK_STRUCTURE_TYPE_MAX_ENUM)
      {
        RDCERR("Invalid extension structure");
      }
      else if(next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||
              next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)
      {
#ifdef VK_USE_PLATFORM_WIN32_KHR
        // make sure the structures are still identical
        RDCCOMPILE_ASSERT(sizeof(VkWin32KeyedMutexAcquireReleaseInfoNV) ==
                              sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR),
                          "Structs are different!");

#define NV_DUMMY ((VkWin32KeyedMutexAcquireReleaseInfoNV *)NULL)
#define KHR_DUMMY ((VkWin32KeyedMutexAcquireReleaseInfoKHR *)NULL)
        RDCCOMPILE_ASSERT(&NV_DUMMY->acquireCount == &KHR_DUMMY->acquireCount,
                          "Structs are different!");
        RDCCOMPILE_ASSERT(&NV_DUMMY->releaseCount == &KHR_DUMMY->releaseCount,
                          "Structs are different!");
        RDCCOMPILE_ASSERT(&NV_DUMMY->pAcquireSyncs == &KHR_DUMMY->pAcquireSyncs,
                          "Structs are different!");
        RDCCOMPILE_ASSERT(&NV_DUMMY->pReleaseSyncs == &KHR_DUMMY->pReleaseSyncs,
                          "Structs are different!");
#undef NV_DUMMY
#undef KHR_DUMMY

        tempmemSize += sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR);

        VkWin32KeyedMutexAcquireReleaseInfoKHR *info = (VkWin32KeyedMutexAcquireReleaseInfoKHR *)next;
        tempmemSize += info->acquireCount * sizeof(VkDeviceMemory);
        tempmemSize += info->releaseCount * sizeof(VkDeviceMemory);
#else
        RDCERR("Unexpected use of Win32 Keyed Mutex extension without support compiled in");
#endif
      }
      else if(next->sType == VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR)
      {
        // nothing to do - this is plain old data with nothing to unwrap so we can keep it in the
        // pNext chain as-is
      }
      else
      {
        RDCERR("Unexpected extension structure %d", next->sType);
      }

      next = (VkGenericStruct *)next->pNext;
    }
  }

  byte *memory = GetTempMemory(tempmemSize);

  VkSubmitInfo *unwrappedSubmits = (VkSubmitInfo *)memory;
  memory += sizeof(VkSubmitInfo) * submitCount;

  for(uint32_t i = 0; i < submitCount; i++)
  {
    RDCASSERT(pSubmits[i].sType == VK_STRUCTURE_TYPE_SUBMIT_INFO);
    unwrappedSubmits[i] = pSubmits[i];

    VkSemaphore *unwrappedWaitSems = (VkSemaphore *)memory;
    memory += sizeof(VkSemaphore) * unwrappedSubmits[i].waitSemaphoreCount;

    unwrappedSubmits[i].pWaitSemaphores =
        unwrappedSubmits[i].waitSemaphoreCount ? unwrappedWaitSems : NULL;
    for(uint32_t o = 0; o < unwrappedSubmits[i].waitSemaphoreCount; o++)
      unwrappedWaitSems[o] = Unwrap(pSubmits[i].pWaitSemaphores[o]);

    VkCommandBuffer *unwrappedCommandBuffers = (VkCommandBuffer *)memory;
    memory += sizeof(VkCommandBuffer) * unwrappedSubmits[i].commandBufferCount;

    unwrappedSubmits[i].pCommandBuffers =
        unwrappedSubmits[i].commandBufferCount ? unwrappedCommandBuffers : NULL;
    for(uint32_t o = 0; o < unwrappedSubmits[i].commandBufferCount; o++)
      unwrappedCommandBuffers[o] = Unwrap(pSubmits[i].pCommandBuffers[o]);
    unwrappedCommandBuffers += unwrappedSubmits[i].commandBufferCount;

    VkSemaphore *unwrappedSignalSems = (VkSemaphore *)memory;
    memory += sizeof(VkSemaphore) * unwrappedSubmits[i].signalSemaphoreCount;

    unwrappedSubmits[i].pSignalSemaphores =
        unwrappedSubmits[i].signalSemaphoreCount ? unwrappedSignalSems : NULL;
    for(uint32_t o = 0; o < unwrappedSubmits[i].signalSemaphoreCount; o++)
      unwrappedSignalSems[o] = Unwrap(pSubmits[i].pSignalSemaphores[o]);

    VkGenericStruct **nextptr = (VkGenericStruct **)&unwrappedSubmits[i].pNext;
    while(*nextptr)
    {
      VkGenericStruct *next = *nextptr;

      if(next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)
      {
#ifdef VK_USE_PLATFORM_WIN32_KHR
        // allocate local unwrapped struct
        VkWin32KeyedMutexAcquireReleaseInfoKHR *unwrappedMutexInfoKHR =
            (VkWin32KeyedMutexAcquireReleaseInfoKHR *)memory;
        memory += sizeof(VkWin32KeyedMutexAcquireReleaseInfoKHR);

        // copy over info from original struct
        VkWin32KeyedMutexAcquireReleaseInfoKHR *wrappedMutexInfoKHR =
            (VkWin32KeyedMutexAcquireReleaseInfoKHR *)next;
        *unwrappedMutexInfoKHR = *wrappedMutexInfoKHR;

        // allocate unwrapped arrays
        VkDeviceMemory *unwrappedAcquires = (VkDeviceMemory *)memory;
        memory += sizeof(VkDeviceMemory) * unwrappedMutexInfoKHR->acquireCount;
        VkDeviceMemory *unwrappedReleases = (VkDeviceMemory *)memory;
        memory += sizeof(VkDeviceMemory) * unwrappedMutexInfoKHR->releaseCount;

        // unwrap the arrays
        for(uint32_t mem = 0; mem < unwrappedMutexInfoKHR->acquireCount; mem++)
          unwrappedAcquires[mem] = Unwrap(wrappedMutexInfoKHR->pAcquireSyncs[mem]);
        for(uint32_t mem = 0; mem < unwrappedMutexInfoKHR->releaseCount; mem++)
          unwrappedReleases[mem] = Unwrap(wrappedMutexInfoKHR->pReleaseSyncs[mem]);

        unwrappedMutexInfoKHR->pAcquireSyncs = unwrappedAcquires;
        unwrappedMutexInfoKHR->pReleaseSyncs = unwrappedReleases;

        // insert this struct into the chain.
        // nextptr is pointing to the address of the pNext, so we can overwrite it to point to our
        // locally-allocated unwrapped struct
        *nextptr = (VkGenericStruct *)unwrappedMutexInfoKHR;
#endif
      }

      nextptr = (VkGenericStruct **)&next->pNext;
    }
  }

  VkResult ret =
      ObjDisp(queue)->QueueSubmit(Unwrap(queue), submitCount, unwrappedSubmits, Unwrap(fence));

  bool capframe = false;
  set<ResourceId> refdIDs;

  for(uint32_t s = 0; s < submitCount; s++)
  {
    for(uint32_t i = 0; i < pSubmits[s].commandBufferCount; i++)
    {
      ResourceId cmd = GetResID(pSubmits[s].pCommandBuffers[i]);

      VkResourceRecord *record = GetRecord(pSubmits[s].pCommandBuffers[i]);

      {
        SCOPED_LOCK(m_ImageLayoutsLock);
        GetResourceManager()->ApplyBarriers(record->bakedCommands->cmdInfo->imgbarriers,
                                            m_ImageLayouts);
      }

      // need to lock the whole section of code, not just the check on
      // m_State, as we also need to make sure we don't check the state,
      // start marking dirty resources then while we're doing so the
      // state becomes capframe.
      // the next sections where we mark resources referenced and add
      // the submit chunk to the frame record don't have to be protected.
      // Only the decision of whether we're inframe or not, and marking
      // dirty.
      {
        SCOPED_LOCK(m_CapTransitionLock);
        capframe = IsActiveCapturing(m_State);
      }

      if(capframe)
      {
        for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
            it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
          GetResourceManager()->MarkPendingDirty(*it);

        // for each bound descriptor set, mark it referenced as well as all resources currently
        // bound to it
        for(auto it = record->bakedCommands->cmdInfo->boundDescSets.begin();
            it != record->bakedCommands->cmdInfo->boundDescSets.end(); ++it)
        {
          GetResourceManager()->MarkResourceFrameReferenced(GetResID(*it), eFrameRef_Read);

          VkResourceRecord *setrecord = GetRecord(*it);

          for(auto refit = setrecord->descInfo->bindFrameRefs.begin();
              refit != setrecord->descInfo->bindFrameRefs.end(); ++refit)
          {
            refdIDs.insert(refit->first);
            GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second.second);

            if(refit->second.first & DescriptorSetData::SPARSE_REF_BIT)
            {
              VkResourceRecord *sparserecord = GetResourceManager()->GetResourceRecord(refit->first);

              GetResourceManager()->MarkSparseMapReferenced(sparserecord->sparseInfo);
            }
          }
        }

        for(auto it = record->bakedCommands->cmdInfo->sparse.begin();
            it != record->bakedCommands->cmdInfo->sparse.end(); ++it)
          GetResourceManager()->MarkSparseMapReferenced(*it);

        // pull in frame refs from this baked command buffer
        record->bakedCommands->AddResourceReferences(GetResourceManager());
        record->bakedCommands->AddReferencedIDs(refdIDs);

        // ref the parent command buffer by itself, this will pull in the cmd buffer pool
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

        for(size_t sub = 0; sub < record->bakedCommands->cmdInfo->subcmds.size(); sub++)
        {
          record->bakedCommands->cmdInfo->subcmds[sub]->bakedCommands->AddResourceReferences(
              GetResourceManager());
          record->bakedCommands->cmdInfo->subcmds[sub]->bakedCommands->AddReferencedIDs(refdIDs);
          GetResourceManager()->MarkResourceFrameReferenced(
              record->bakedCommands->cmdInfo->subcmds[sub]->GetResourceID(), eFrameRef_Read);

          record->bakedCommands->cmdInfo->subcmds[sub]->bakedCommands->AddRef();
        }

        {
          SCOPED_LOCK(m_CmdBufferRecordsLock);
          m_CmdBufferRecords.push_back(record->bakedCommands);
          for(size_t sub = 0; sub < record->bakedCommands->cmdInfo->subcmds.size(); sub++)
            m_CmdBufferRecords.push_back(record->bakedCommands->cmdInfo->subcmds[sub]->bakedCommands);
        }

        record->bakedCommands->AddRef();
      }
      else
      {
        for(auto it = record->bakedCommands->cmdInfo->dirtied.begin();
            it != record->bakedCommands->cmdInfo->dirtied.end(); ++it)
          GetResourceManager()->MarkDirtyResource(*it);
      }

      record->cmdInfo->dirtied.clear();
    }
  }

  if(capframe)
  {
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);

    if(fence != VK_NULL_HANDLE)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);

    std::vector<VkResourceRecord *> maps;
    {
      SCOPED_LOCK(m_CoherentMapsLock);
      maps = m_CoherentMaps;
    }

    for(auto it = maps.begin(); it != maps.end(); ++it)
    {
      VkResourceRecord *record = *it;
      MemMapState &state = *record->memMapState;

      // potential persistent map
      if(state.mapCoherent && state.mappedPtr && !state.mapFlushed)
      {
        // only need to flush memory that could affect this submitted batch of work
        if(refdIDs.find(record->GetResourceID()) == refdIDs.end())
        {
          RDCDEBUG("Map of memory %llu not referenced in this queue - not flushing",
                   record->GetResourceID());
          continue;
        }

        size_t diffStart = 0, diffEnd = 0;
        bool found = true;

// enabled as this is necessary for programs with very large coherent mappings
// (> 1GB) as otherwise more than a couple of vkQueueSubmit calls leads to vast
// memory allocation. There might still be bugs lurking in here though
#if 1
        // this causes vkFlushMappedMemoryRanges call to allocate and copy to refData
        // from serialised buffer. We want to copy *precisely* the serialised data,
        // otherwise there is a gap in time between serialising out a snapshot of
        // the buffer and whenever we then copy into the ref data, e.g. below.
        // during this time, data could be written to the buffer and it won't have
        // been caught in the serialised snapshot, and if it doesn't change then
        // it *also* won't be caught in any future FindDiffRange() calls.
        //
        // Likewise once refData is allocated, the call below will also update it
        // with the data serialised out for the same reason.
        //
        // Note: it's still possible that data is being written to by the
        // application while it's being serialised out in the snapshot below. That
        // is OK, since the application is responsible for ensuring it's not writing
        // data that would be needed by the GPU in this submit. As long as the
        // refdata we use for future use is identical to what was serialised, we
        // shouldn't miss anything
        state.needRefData = true;

        // if we have a previous set of data, compare.
        // otherwise just serialise it all
        if(state.refData)
          found = FindDiffRange((byte *)state.mappedPtr, state.refData, (size_t)state.mapSize,
                                diffStart, diffEnd);
        else
#endif
          diffEnd = (size_t)state.mapSize;

        if(found)
        {
          // MULTIDEVICE should find the device for this queue.
          // MULTIDEVICE only want to flush maps associated with this queue
          VkDevice dev = GetDev();

          {
            RDCLOG("Persistent map flush forced for %llu (%llu -> %llu)", record->GetResourceID(),
                   (uint64_t)diffStart, (uint64_t)diffEnd);
            VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL,
                                         (VkDeviceMemory)(uint64_t)record->Resource,
                                         state.mapOffset + diffStart, diffEnd - diffStart};
            vkFlushMappedMemoryRanges(dev, 1, &range);
            state.mapFlushed = false;
          }

          GetResourceManager()->MarkPendingDirty(record->GetResourceID());
        }
        else
        {
          RDCDEBUG("Persistent map flush not needed for %llu", record->GetResourceID());
        }
      }
    }

    {
      CACHE_THREAD_SERIALISER();

      ser.SetDrawChunk();
      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueSubmit);
      Serialise_vkQueueSubmit(ser, queue, submitCount, pSubmits, fence);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    for(uint32_t s = 0; s < submitCount; s++)
    {
      for(uint32_t sem = 0; sem < pSubmits[s].waitSemaphoreCount; sem++)
        GetResourceManager()->MarkResourceFrameReferenced(
            GetResID(pSubmits[s].pWaitSemaphores[sem]), eFrameRef_Read);

      for(uint32_t sem = 0; sem < pSubmits[s].signalSemaphoreCount; sem++)
        GetResourceManager()->MarkResourceFrameReferenced(
            GetResID(pSubmits[s].pSignalSemaphores[sem]), eFrameRef_Read);
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueBindSparse(SerialiserType &ser, VkQueue queue,
                                                uint32_t bindInfoCount,
                                                const VkBindSparseInfo *pBindInfo, VkFence fence)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT_ARRAY(pBindInfo, bindInfoCount);
  SERIALISE_ELEMENT(fence);

  if(IsReplayingAndReading())
  {
    // similar to vkQueueSubmit we don't need semaphores at all, just whether we waited on any.
    // For waiting semaphores, since we don't track state we have to just conservatively
    // wait for queue idle. Since we do that, there's equally no point in signalling semaphores
    bool doWait = false;
    for(uint32_t i = 0; i < bindInfoCount; i++)
      if(pBindInfo[i].waitSemaphoreCount > 0)
        doWait = true;

    if(doWait)
      ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

    for(uint32_t bind = 0; bind < bindInfoCount; bind++)
    {
      // we can freely mutate the info as it's locally allocated
      VkBindSparseInfo &bindInfo = ((VkBindSparseInfo *)pBindInfo)[bind];
      bindInfo.pWaitSemaphores = NULL;
      bindInfo.waitSemaphoreCount = 0;
      bindInfo.pSignalSemaphores = NULL;
      bindInfo.signalSemaphoreCount = 0;

      // remove any binds for resources that aren't present, since this
      // is totally valid (if the resource wasn't referenced in anything
      // else, it will be omitted from the capture)
      VkSparseBufferMemoryBindInfo *buf = (VkSparseBufferMemoryBindInfo *)bindInfo.pBufferBinds;
      for(uint32_t i = 0; i < bindInfo.bufferBindCount; i++)
      {
        if(buf[i].buffer == VK_NULL_HANDLE)
        {
          bindInfo.bufferBindCount--;
          std::swap(buf[i], buf[bindInfo.bufferBindCount]);
        }
        else
        {
          buf[i].buffer = Unwrap(buf[i].buffer);

          VkSparseMemoryBind *binds = (VkSparseMemoryBind *)buf[i].pBinds;
          for(uint32_t b = 0; b < buf[i].bindCount; b++)
            binds[b].memory = Unwrap(binds[b].memory);
        }
      }

      VkSparseImageOpaqueMemoryBindInfo *imopaque =
          (VkSparseImageOpaqueMemoryBindInfo *)bindInfo.pImageOpaqueBinds;
      for(uint32_t i = 0; i < bindInfo.imageOpaqueBindCount; i++)
      {
        if(imopaque[i].image == VK_NULL_HANDLE)
        {
          bindInfo.imageOpaqueBindCount--;
          std::swap(imopaque[i], imopaque[bindInfo.imageOpaqueBindCount]);
        }
        else
        {
          imopaque[i].image = Unwrap(imopaque[i].image);

          VkSparseMemoryBind *binds = (VkSparseMemoryBind *)imopaque[i].pBinds;
          for(uint32_t b = 0; b < imopaque[i].bindCount; b++)
            binds[b].memory = Unwrap(binds[b].memory);
        }
      }

      VkSparseImageMemoryBindInfo *im = (VkSparseImageMemoryBindInfo *)bindInfo.pImageBinds;
      for(uint32_t i = 0; i < bindInfo.imageBindCount; i++)
      {
        if(im[i].image == VK_NULL_HANDLE)
        {
          bindInfo.imageBindCount--;
          std::swap(im[i], im[bindInfo.imageBindCount]);
        }
        else
        {
          im[i].image = Unwrap(im[i].image);

          VkSparseImageMemoryBind *binds = (VkSparseImageMemoryBind *)im[i].pBinds;
          for(uint32_t b = 0; b < im[i].bindCount; b++)
            binds[b].memory = Unwrap(binds[b].memory);
        }
      }
    }

    // don't submit the fence, since we have nothing to wait on it being signalled, and we might
    // not have it correctly in the unsignalled state.
    ObjDisp(queue)->QueueBindSparse(Unwrap(queue), bindInfoCount, pBindInfo, VK_NULL_HANDLE);
  }

  return true;
}

VkResult WrappedVulkan::vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount,
                                          const VkBindSparseInfo *pBindInfo, VkFence fence)
{
  // need to allocate space for each bind batch
  size_t tempmemSize = sizeof(VkBindSparseInfo) * bindInfoCount;

  for(uint32_t i = 0; i < bindInfoCount; i++)
  {
    // within each batch, need to allocate space for each resource bind
    tempmemSize += pBindInfo[i].bufferBindCount * sizeof(VkSparseBufferMemoryBindInfo);
    tempmemSize += pBindInfo[i].imageOpaqueBindCount * sizeof(VkSparseImageOpaqueMemoryBindInfo);
    tempmemSize += pBindInfo[i].imageBindCount * sizeof(VkSparseImageMemoryBindInfo);
    tempmemSize += pBindInfo[i].waitSemaphoreCount * sizeof(VkSemaphore);
    tempmemSize += pBindInfo[i].signalSemaphoreCount * sizeof(VkSparseImageMemoryBindInfo);

    // within each resource bind, need to save space for each individual bind operation
    for(uint32_t b = 0; b < pBindInfo[i].bufferBindCount; b++)
      tempmemSize += pBindInfo[i].pBufferBinds[b].bindCount * sizeof(VkSparseMemoryBind);
    for(uint32_t b = 0; b < pBindInfo[i].imageOpaqueBindCount; b++)
      tempmemSize += pBindInfo[i].pImageOpaqueBinds[b].bindCount * sizeof(VkSparseMemoryBind);
    for(uint32_t b = 0; b < pBindInfo[i].imageBindCount; b++)
      tempmemSize += pBindInfo[i].pImageBinds[b].bindCount * sizeof(VkSparseImageMemoryBind);
  }

  byte *memory = GetTempMemory(tempmemSize);

  VkBindSparseInfo *unwrapped = (VkBindSparseInfo *)memory;
  byte *next = (byte *)(unwrapped + bindInfoCount);

  // now go over each batch..
  for(uint32_t i = 0; i < bindInfoCount; i++)
  {
    // copy the original so we get all the params we don't need to change
    RDCASSERT(pBindInfo[i].sType == VK_STRUCTURE_TYPE_BIND_SPARSE_INFO && pBindInfo[i].pNext == NULL);
    unwrapped[i] = pBindInfo[i];

    // unwrap the signal semaphores into a new array
    VkSemaphore *signal = (VkSemaphore *)next;
    next += sizeof(VkSemaphore) * unwrapped[i].signalSemaphoreCount;
    unwrapped[i].pSignalSemaphores = signal;
    for(uint32_t j = 0; j < unwrapped[i].signalSemaphoreCount; j++)
      signal[j] = Unwrap(pBindInfo[i].pSignalSemaphores[j]);

    // and the wait semaphores
    VkSemaphore *wait = (VkSemaphore *)next;
    next += sizeof(VkSemaphore) * unwrapped[i].waitSemaphoreCount;
    unwrapped[i].pWaitSemaphores = wait;
    for(uint32_t j = 0; j < unwrapped[i].waitSemaphoreCount; j++)
      wait[j] = Unwrap(pBindInfo[i].pWaitSemaphores[j]);

    // now copy & unwrap the sparse buffer binds
    VkSparseBufferMemoryBindInfo *buf = (VkSparseBufferMemoryBindInfo *)next;
    next += sizeof(VkSparseBufferMemoryBindInfo) * unwrapped[i].bufferBindCount;
    unwrapped[i].pBufferBinds = buf;
    for(uint32_t j = 0; j < unwrapped[i].bufferBindCount; j++)
    {
      buf[j] = pBindInfo[i].pBufferBinds[j];
      buf[j].buffer = Unwrap(buf[j].buffer);

      // for each buffer bind, copy & unwrap the individual memory binds too
      VkSparseMemoryBind *binds = (VkSparseMemoryBind *)next;
      next += sizeof(VkSparseMemoryBind) * buf[j].bindCount;
      buf[j].pBinds = binds;
      for(uint32_t k = 0; k < buf[j].bindCount; k++)
      {
        binds[k] = pBindInfo[i].pBufferBinds[j].pBinds[k];
        binds[k].memory = Unwrap(buf[j].pBinds[k].memory);
      }
    }

    // same as above
    VkSparseImageOpaqueMemoryBindInfo *opaque = (VkSparseImageOpaqueMemoryBindInfo *)next;
    next += sizeof(VkSparseImageOpaqueMemoryBindInfo) * unwrapped[i].imageOpaqueBindCount;
    unwrapped[i].pImageOpaqueBinds = opaque;
    for(uint32_t j = 0; j < unwrapped[i].imageOpaqueBindCount; j++)
    {
      opaque[j] = pBindInfo[i].pImageOpaqueBinds[j];
      opaque[j].image = Unwrap(opaque[j].image);

      VkSparseMemoryBind *binds = (VkSparseMemoryBind *)next;
      next += sizeof(VkSparseMemoryBind) * opaque[j].bindCount;
      opaque[j].pBinds = binds;
      for(uint32_t k = 0; k < opaque[j].bindCount; k++)
      {
        binds[k] = pBindInfo[i].pImageOpaqueBinds[j].pBinds[k];
        binds[k].memory = Unwrap(opaque[j].pBinds[k].memory);
      }
    }

    // same as above
    VkSparseImageMemoryBindInfo *im = (VkSparseImageMemoryBindInfo *)next;
    next += sizeof(VkSparseImageMemoryBindInfo) * unwrapped[i].imageBindCount;
    unwrapped[i].pImageBinds = im;
    for(uint32_t j = 0; j < unwrapped[i].imageBindCount; j++)
    {
      im[j] = pBindInfo[i].pImageBinds[j];
      im[j].image = Unwrap(im[j].image);

      VkSparseImageMemoryBind *binds = (VkSparseImageMemoryBind *)next;
      next += sizeof(VkSparseImageMemoryBind) * im[j].bindCount;
      im[j].pBinds = binds;
      for(uint32_t k = 0; k < im[j].bindCount; k++)
      {
        binds[k] = pBindInfo[i].pImageBinds[j].pBinds[k];
        binds[k].memory = Unwrap(im[j].pBinds[k].memory);
      }
    }
  }

  VkResult ret =
      ObjDisp(queue)->QueueBindSparse(Unwrap(queue), bindInfoCount, unwrapped, Unwrap(fence));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    {
      SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueBindSparse);
      ser.SetDrawChunk();
      Serialise_vkQueueBindSparse(ser, queue, bindInfoCount, pBindInfo, fence);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }

    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);
      // images/buffers aren't marked referenced. If the only ref is a memory bind, we just skip it

      for(uint32_t w = 0; w < pBindInfo[i].waitSemaphoreCount; w++)
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(pBindInfo[i].pWaitSemaphores[w]),
                                                          eFrameRef_Read);
      for(uint32_t s = 0; s < pBindInfo[i].signalSemaphoreCount; s++)
        GetResourceManager()->MarkResourceFrameReferenced(
            GetResID(pBindInfo[i].pSignalSemaphores[s]), eFrameRef_Read);
    }
  }

  // update our internal page tables
  if(IsCaptureMode(m_State))
  {
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      for(uint32_t buf = 0; buf < pBindInfo[i].bufferBindCount; buf++)
      {
        const VkSparseBufferMemoryBindInfo &bind = pBindInfo[i].pBufferBinds[buf];
        GetRecord(bind.buffer)->sparseInfo->Update(bind.bindCount, bind.pBinds);
      }

      for(uint32_t op = 0; op < pBindInfo[i].imageOpaqueBindCount; op++)
      {
        const VkSparseImageOpaqueMemoryBindInfo &bind = pBindInfo[i].pImageOpaqueBinds[op];
        GetRecord(bind.image)->sparseInfo->Update(bind.bindCount, bind.pBinds);
      }

      for(uint32_t op = 0; op < pBindInfo[i].imageBindCount; op++)
      {
        const VkSparseImageMemoryBindInfo &bind = pBindInfo[i].pImageBinds[op];
        GetRecord(bind.image)->sparseInfo->Update(bind.bindCount, bind.pBinds);
      }
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueWaitIdle(SerialiserType &ser, VkQueue queue)
{
  SERIALISE_ELEMENT(queue);

  if(IsReplayingAndReading())
  {
    ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
  }

  return true;
}

VkResult WrappedVulkan::vkQueueWaitIdle(VkQueue queue)
{
  VkResult ret = ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();

    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueWaitIdle);
    Serialise_vkQueueWaitIdle(ser, queue);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
  }

  return ret;
}

INSTANTIATE_FUNCTION_SERIALISED(void, vkGetDeviceQueue, VkDevice device, uint32_t queueFamilyIndex,
                                uint32_t queueIndex, VkQueue *pQueue);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueSubmit, VkQueue queue, uint32_t submitCount,
                                const VkSubmitInfo *pSubmits, VkFence fence);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueBindSparse, VkQueue queue, uint32_t bindInfoCount,
                                const VkBindSparseInfo *pBindInfo, VkFence fence);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueWaitIdle, VkQueue queue);
