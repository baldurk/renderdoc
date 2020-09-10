/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <algorithm>
#include "../vk_core.h"
#include "../vk_debug.h"

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetDeviceQueue(SerialiserType &ser, VkDevice device,
                                               uint32_t queueFamilyIndex, uint32_t queueIndex,
                                               VkQueue *pQueue)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(queueFamilyIndex);
  SERIALISE_ELEMENT(queueIndex);
  SERIALISE_ELEMENT_LOCAL(Queue, GetResID(*pQueue)).TypedAs("VkQueue"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkQueue queue;

    uint32_t remapFamily = m_QueueRemapping[queueFamilyIndex][queueIndex].family;
    uint32_t remapIndex = m_QueueRemapping[queueFamilyIndex][queueIndex].index;

    if(remapFamily != queueFamilyIndex || remapIndex != queueIndex)
      RDCLOG("Remapped Queue %u/%u from capture to %u/%u on replay", queueFamilyIndex, queueIndex,
             remapFamily, remapIndex);

    ObjDisp(device)->GetDeviceQueue(Unwrap(device), remapFamily, remapIndex, &queue);

    if(GetResourceManager()->HasWrapper(ToTypedHandle(queue)))
    {
      ResourceId live = GetResourceManager()->GetDispWrapper(queue)->id;

      // whenever the new ID is requested, return the old ID, via replacements.
      GetResourceManager()->ReplaceResource(Queue, GetResourceManager()->GetOriginalID(live));
    }
    else
    {
      GetResourceManager()->WrapResource(Unwrap(device), queue);
      GetResourceManager()->AddLiveResource(Queue, queue);
    }

    if(remapFamily == m_QueueFamilyIdx && m_Queue == VK_NULL_HANDLE)
    {
      m_Queue = queue;

      // we can now submit any cmds that were queued (e.g. from creating debug
      // manager on vkCreateDevice)
      SubmitCmds();
    }

    if(remapFamily < m_ExternalQueues.size())
    {
      if(m_ExternalQueues[remapFamily].queue == VK_NULL_HANDLE)
        m_ExternalQueues[remapFamily].queue = queue;
    }
    else
    {
      RDCERR("Unexpected queue family index %u", remapFamily);
    }

    m_CreationInfo.m_Queue[GetResID(queue)] = remapFamily;

    AddResource(Queue, ResourceType::Queue, "Queue");
    DerivedResource(device, Queue);
  }

  return true;
}

void WrappedVulkan::vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
                                     uint32_t queueIndex, VkQueue *pQueue)
{
  SERIALISE_TIME_CALL(
      ObjDisp(device)->GetDeviceQueue(Unwrap(device), queueFamilyIndex, queueIndex, pQueue));

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

        record->queueFamilyIndex = queueFamilyIndex;

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

      if(queueFamilyIndex < m_ExternalQueues.size())
      {
        if(m_ExternalQueues[queueFamilyIndex].queue == VK_NULL_HANDLE)
          m_ExternalQueues[queueFamilyIndex].queue = *pQueue;
      }
      else
      {
        RDCERR("Unexpected queue family index %u", queueFamilyIndex);
      }

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

static void appendChain(VkBaseInStructure *chain, void *item)
{
  while(chain->pNext != NULL)
    chain = (VkBaseInStructure *)chain->pNext;
  chain->pNext = (VkBaseInStructure *)item;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueSubmit(SerialiserType &ser, VkQueue queue, uint32_t submitCount,
                                            const VkSubmitInfo *pSubmits, VkFence fence)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT(submitCount);
  SERIALISE_ELEMENT_ARRAY(pSubmits, submitCount);
  SERIALISE_ELEMENT(fence);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // if there are multiple queue submissions in flight, wait for the previous queue to finish
    // before executing this, as we don't have the sync information to properly sync.
    if(m_PrevQueue != queue)
    {
      RDCDEBUG("Previous queue execution was on queue %s, now executing %s, syncing GPU",
               ToStr(GetResID(m_PrevQueue)).c_str(), ToStr(GetResID(queue)).c_str());
      if(m_PrevQueue != VK_NULL_HANDLE)
        ObjDisp(m_PrevQueue)->QueueWaitIdle(Unwrap(m_PrevQueue));

      m_PrevQueue = queue;
    }

    // if we ever waited on any semaphores, wait for idle here.
    bool doWait = false;
    for(uint32_t i = 0; i < submitCount; i++)
      if(pSubmits[i].waitSemaphoreCount > 0)
        doWait = true;

    if(doWait)
      ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

    // add a drawcall use for this submission, to tally up with any debug messages that come from it
    if(IsLoading(m_State))
    {
      DrawcallUse use(m_CurChunkOffset, m_RootEventID);

      // insert in sorted location
      auto drawit = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
      m_DrawcallUses.insert(drawit - m_DrawcallUses.begin(), use);
    }

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

        size_t tempMemSize = unwrapped.commandBufferCount * sizeof(VkCommandBuffer) +
                             GetNextPatchSize(unwrapped.pNext);

        byte *tempMem = GetTempMemory(tempMemSize);

        VkCommandBuffer *unwrappedCmds = (VkCommandBuffer *)tempMem;
        unwrapped.pCommandBuffers = unwrappedCmds;
        for(uint32_t i = 0; i < unwrapped.commandBufferCount; i++)
          unwrappedCmds[i] = Unwrap(submitInfo.pCommandBuffers[i]);

        tempMem += unwrapped.commandBufferCount * sizeof(VkCommandBuffer);

        UnwrapNextChain(m_State, "VkSubmitInfo", tempMem, (VkBaseInStructure *)&unwrapped);
        appendChain((VkBaseInStructure *)&unwrapped, m_SubmitChain);

        ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &unwrapped, VK_NULL_HANDLE);

        AddEvent();

        // we're adding multiple events, need to increment ourselves
        m_RootEventID++;

        rdcstr basename = StringFormat::Fmt("vkQueueSubmit(%u)", submitInfo.commandBufferCount);

        for(uint32_t c = 0; c < submitInfo.commandBufferCount; c++)
        {
          ResourceId cmd =
              GetResourceManager()->GetOriginalID(GetResID(submitInfo.pCommandBuffers[c]));

          BakedCmdBufferInfo &cmdBufInfo = m_BakedCmdBufferInfo[cmd];

          UpdateImageStates(m_BakedCmdBufferInfo[cmd].imageStates);

          rdcstr name = StringFormat::Fmt("=> %s[%u]: vkBeginCommandBuffer(%s)", basename.c_str(),
                                          c, ToStr(cmd).c_str());

          // add a fake marker
          DrawcallDescription draw;
          draw.name = name;
          draw.flags |= DrawFlags::PassBoundary | DrawFlags::BeginPass;
          AddEvent();

          m_RootEvents.back().chunkIndex = cmdBufInfo.beginChunk;
          m_Events.back().chunkIndex = cmdBufInfo.beginChunk;

          AddDrawcall(draw, true);
          m_RootEventID++;

          // insert the baked command buffer in-line into this list of notes, assigning new event
          // and drawIDs
          InsertDrawsAndRefreshIDs(cmdBufInfo);

          for(size_t e = 0; e < cmdBufInfo.draw->executedCmds.size(); e++)
          {
            rdcarray<Submission> &submits =
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
            m_DebugMessages.back().eventId += m_RootEventID;
          }

          // only primary command buffers can be submitted
          m_Partial[Primary].cmdBufferSubmits[cmd].push_back(Submission(m_RootEventID));

          m_RootEventID += cmdBufInfo.eventCount;
          m_RootDrawcallID += cmdBufInfo.drawCount;

          name = StringFormat::Fmt("=> %s[%u]: vkEndCommandBuffer(%s)", basename.c_str(), c,
                                   ToStr(cmd).c_str());
          draw.name = name;
          draw.flags = DrawFlags::PassBoundary | DrawFlags::EndPass;
          AddEvent();

          m_RootEvents.back().chunkIndex = cmdBufInfo.endChunk;
          m_Events.back().chunkIndex = cmdBufInfo.endChunk;

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
        else
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue Submit from re-recorded commands, root EID %u last EID", m_RootEventID,
                   m_LastEventID);
#endif

          uint32_t eid = startEID;

          rdcarray<VkCommandBuffer> rerecordedCmds;

          for(uint32_t c = 0; c < submitInfo.commandBufferCount; c++)
          {
            ResourceId cmdId =
                GetResourceManager()->GetOriginalID(GetResID(submitInfo.pCommandBuffers[c]));

            // account for the virtual vkBeginCommandBuffer label at the start of the events here
            // so it matches up to baseEvent
            eid++;

#if ENABLED(VERBOSE_PARTIAL_REPLAY)
            uint32_t end = eid + m_BakedCmdBufferInfo[cmdId].eventCount;
#endif

            if(eid <= m_LastEventID)
            {
              VkCommandBuffer cmd = RerecordCmdBuf(cmdId);
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              ResourceId rerecord = GetResID(cmd);
              RDCDEBUG("Queue Submit re-recorded replay of %s, using %s (%u -> %u <= %u)",
                       ToStr(cmdId).c_str(), ToStr(rerecord).c_str(), eid, end, m_LastEventID);
#endif
              rerecordedCmds.push_back(Unwrap(cmd));

              UpdateImageStates(m_BakedCmdBufferInfo[cmdId].imageStates);
            }
            else
            {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
              RDCDEBUG("Queue not submitting %s", ToStr(cmdId).c_str());
#endif
            }

            // 1 extra to account for the virtual end command buffer label (begin is accounted for
            // above)
            eid += 1 + m_BakedCmdBufferInfo[cmdId].eventCount;
          }

          VkSubmitInfo rerecordedSubmit = submitInfo;

          byte *tempMem = GetTempMemory(GetNextPatchSize(rerecordedSubmit.pNext));

          UnwrapNextChain(m_State, "VkSubmitInfo", tempMem, (VkBaseInStructure *)&rerecordedSubmit);
          appendChain((VkBaseInStructure *)&rerecordedSubmit, m_SubmitChain);

          rerecordedSubmit.commandBufferCount = (uint32_t)rerecordedCmds.size();
          rerecordedSubmit.pCommandBuffers = &rerecordedCmds[0];

#if ENABLED(SINGLE_FLUSH_VALIDATE)
          rerecordedSubmit.commandBufferCount = 1;
          for(size_t i = 0; i < rerecordedCmds.size(); i++)
          {
            ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &rerecordedSubmit, VK_NULL_HANDLE);
            rerecordedSubmit.pCommandBuffers++;

            FlushQ();
          }
#else
          // don't submit the fence, since we have nothing to wait on it being signalled, and we
          // might not have it correctly in the unsignalled state.
          ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &rerecordedSubmit, VK_NULL_HANDLE);
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

bool WrappedVulkan::PatchIndirectDraw(size_t drawIndex, uint32_t paramStride,
                                      VkIndirectPatchType type, DrawcallDescription &draw,
                                      byte *&argptr, byte *argend)
{
  bool valid = false;

  draw.drawIndex = (uint32_t)drawIndex;

  if(type == VkIndirectPatchType::DrawIndirect || type == VkIndirectPatchType::DrawIndirectCount)
  {
    if(argptr && argptr + sizeof(VkDrawIndirectCommand) <= argend)
    {
      VkDrawIndirectCommand *arg = (VkDrawIndirectCommand *)argptr;

      draw.numIndices = arg->vertexCount;
      draw.numInstances = arg->instanceCount;
      draw.vertexOffset = arg->firstVertex;
      draw.instanceOffset = arg->firstInstance;

      valid = true;
    }
  }
  else if(type == VkIndirectPatchType::DrawIndirectByteCount)
  {
    if(argptr && argptr + 4 <= argend)
    {
      uint32_t *arg = (uint32_t *)argptr;

      draw.numIndices = *arg;

      valid = true;
    }
  }
  else if(type == VkIndirectPatchType::DrawIndexedIndirect ||
          type == VkIndirectPatchType::DrawIndexedIndirectCount)
  {
    if(argptr && argptr + sizeof(VkDrawIndexedIndirectCommand) <= argend)
    {
      VkDrawIndexedIndirectCommand *arg = (VkDrawIndexedIndirectCommand *)argptr;

      draw.numIndices = arg->indexCount;
      draw.numInstances = arg->instanceCount;
      draw.vertexOffset = arg->vertexOffset;
      draw.indexOffset = arg->firstIndex;
      draw.instanceOffset = arg->firstInstance;

      valid = true;
    }
  }
  else
  {
    RDCERR("Unexpected indirect draw type");
  }

  if(valid && !draw.events.empty())
  {
    SDChunk *chunk = m_StructuredFile->chunks[draw.events.back().chunkIndex];

    if(chunk->metadata.chunkID != (uint32_t)VulkanChunk::vkCmdIndirectSubCommand)
      chunk = m_StructuredFile->chunks[draw.events.back().chunkIndex - 1];

    SDObject *drawIdx = chunk->FindChild("drawIndex");

    if(drawIdx)
      drawIdx->data.basic.u = drawIndex;

    SDObject *offset = chunk->FindChild("offset");

    if(offset)
      offset->data.basic.u += drawIndex * paramStride;

    SDObject *command = chunk->FindChild("command");

    // single draw indirect draws don't have a command child since it can't be added without
    // breaking serialising the chunk.
    if(command)
    {
      // patch up structured data contents
      if(SDObject *sub = command->FindChild("vertexCount"))
        sub->data.basic.u = draw.numIndices;
      if(SDObject *sub = command->FindChild("indexCount"))
        sub->data.basic.u = draw.numIndices;
      if(SDObject *sub = command->FindChild("instanceCount"))
        sub->data.basic.u = draw.numInstances;
      if(SDObject *sub = command->FindChild("firstVertex"))
        sub->data.basic.u = draw.vertexOffset;
      if(SDObject *sub = command->FindChild("vertexOffset"))
        sub->data.basic.u = draw.vertexOffset;
      if(SDObject *sub = command->FindChild("firstIndex"))
        sub->data.basic.u = draw.indexOffset;
      if(SDObject *sub = command->FindChild("firstInstance"))
        sub->data.basic.u = draw.instanceOffset;
    }
  }

  return valid;
}

void WrappedVulkan::InsertDrawsAndRefreshIDs(BakedCmdBufferInfo &cmdBufInfo)
{
  rdcarray<VulkanDrawcallTreeNode> &cmdBufNodes = cmdBufInfo.draw->children;

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
    n.draw.eventId += m_RootEventID;
    n.draw.drawcallId += m_RootDrawcallID;

    if(n.indirectPatch.type == VkIndirectPatchType::DispatchIndirect)
    {
      VkDispatchIndirectCommand unknown = {0};
      bytebuf argbuf;
      GetDebugManager()->GetBufferData(GetResID(n.indirectPatch.buf), 0, 0, argbuf);
      VkDispatchIndirectCommand *args = (VkDispatchIndirectCommand *)&argbuf[0];

      if(argbuf.size() < sizeof(VkDispatchIndirectCommand))
      {
        RDCERR("Couldn't fetch arguments buffer for vkCmdDispatchIndirect");
        args = &unknown;
      }

      n.draw.name =
          StringFormat::Fmt("vkCmdDispatchIndirect(<%u, %u, %u>)", args->x, args->y, args->z);
      n.draw.dispatchDimension[0] = args->x;
      n.draw.dispatchDimension[1] = args->y;
      n.draw.dispatchDimension[2] = args->z;
    }
    else if(n.indirectPatch.type == VkIndirectPatchType::DrawIndirectByteCount ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndirect ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndexedIndirect ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndirectCount ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndexedIndirectCount)
    {
      bool hasCount = (n.indirectPatch.type == VkIndirectPatchType::DrawIndirectCount ||
                       n.indirectPatch.type == VkIndirectPatchType::DrawIndexedIndirectCount);
      bytebuf argbuf;
      GetDebugManager()->GetBufferData(GetResID(n.indirectPatch.buf), 0, 0, argbuf);

      byte *ptr = argbuf.begin(), *end = argbuf.end();

      uint32_t indirectCount = n.indirectPatch.count;
      if(hasCount)
      {
        if(argbuf.size() >= 16)
        {
          uint32_t *count = (uint32_t *)end;
          count -= 4;
          indirectCount = *count;
        }
        else
        {
          RDCERR("Couldn't get indirect draw count");
        }

        if(indirectCount > n.indirectPatch.count)
        {
          RDCERR("Indirect count higher than maxCount, clamping");
          indirectCount = n.indirectPatch.count;
        }

        // this can be negative if indirectCount is 0
        int32_t eidShift = indirectCount - 1;

        // we reserved one event and drawcall for the indirect count based draw.
        // if we ended up with a different number eidShift will be non-zero, so we need to adjust
        // all subsequent EIDs and draw IDs and either remove the subdraw we allocated (if no draws
        // happened) or clone the subdraw to create more that we can then patch.
        if(eidShift != 0)
        {
          // i is the pushmarker, so i + 1 is the sub draws, and i + 2 is the pop marker.
          // adjust all EIDs and draw IDs after that point
          for(size_t j = i + 2; j < cmdBufNodes.size(); j++)
          {
            cmdBufNodes[j].draw.eventId += eidShift;
            cmdBufNodes[j].draw.drawcallId += eidShift;

            for(APIEvent &ev : cmdBufNodes[j].draw.events)
              ev.eventId += eidShift;

            for(rdcpair<ResourceId, EventUsage> &use : cmdBufNodes[j].resourceUsage)
              use.second.eventId += eidShift;
          }

          for(size_t j = 0; j < cmdBufInfo.debugMessages.size(); j++)
          {
            if(cmdBufInfo.debugMessages[j].eventId >= cmdBufNodes[i].draw.eventId + 2)
              cmdBufInfo.debugMessages[j].eventId += eidShift;
          }

          cmdBufInfo.eventCount += eidShift;
          cmdBufInfo.drawCount += eidShift;

          // we also need to patch the original secondary command buffer here, if the indirect call
          // was on a secondary, so that vkCmdExecuteCommands knows accurately how many events are
          // in the command buffer.
          if(n.indirectPatch.commandBuffer != ResourceId())
          {
            m_BakedCmdBufferInfo[n.indirectPatch.commandBuffer].eventCount += eidShift;
            m_BakedCmdBufferInfo[n.indirectPatch.commandBuffer].drawCount += eidShift;
          }

          for(size_t e = 0; e < cmdBufInfo.draw->executedCmds.size(); e++)
          {
            rdcarray<Submission> &submits =
                m_Partial[Secondary].cmdBufferSubmits[cmdBufInfo.draw->executedCmds[e]];

            for(size_t s = 0; s < submits.size(); s++)
            {
              if(submits[s].baseEvent >= cmdBufNodes[i].draw.eventId + 2)
                submits[s].baseEvent += eidShift;
            }
          }

          RDCASSERT(cmdBufNodes[i + 1].draw.events.size() == 1);
          uint32_t chunkIndex = cmdBufNodes[i + 1].draw.events[0].chunkIndex;

          // everything afterwards is adjusted. Now see if we need to remove the subdraw or clone it
          if(indirectCount == 0)
          {
            // i is the pushmarker, which we leave. i+1 is the subdraw
            cmdBufNodes.erase(i + 1);
          }
          else
          {
            // duplicate the fake structured data chunk N times
            SDChunk *chunk = m_StructuredFile->chunks[chunkIndex];

            uint32_t baseAddedChunk = (uint32_t)m_StructuredFile->chunks.size();
            for(int32_t e = 0; e < eidShift; e++)
              m_StructuredFile->chunks.push_back(chunk->Duplicate());

            // now copy the subdraw so we're not inserting into the array from itself
            VulkanDrawcallTreeNode node = cmdBufNodes[i + 1];

            // then insert enough duplicates
            for(int32_t e = 0; e < eidShift; e++)
            {
              node.draw.eventId++;
              node.draw.drawcallId++;

              for(APIEvent &ev : node.draw.events)
              {
                ev.eventId++;
                ev.chunkIndex = baseAddedChunk + e;
              }

              for(rdcpair<ResourceId, EventUsage> &use : node.resourceUsage)
                use.second.eventId++;

              cmdBufNodes.insert(i + 2 + e, node);
            }
          }
        }
      }

      // indirect count versions always have a multidraw marker regions, but static count of 1 would
      // be in-lined as a single draw, so we patch in-place
      if(!hasCount && indirectCount == 1)
      {
        bool valid =
            PatchIndirectDraw(0, n.indirectPatch.stride, n.indirectPatch.type, n.draw, ptr, end);

        if(n.indirectPatch.type == VkIndirectPatchType::DrawIndirectByteCount)
        {
          if(n.draw.numIndices > n.indirectPatch.vertexoffset)
            n.draw.numIndices -= n.indirectPatch.vertexoffset;
          else
            n.draw.numIndices = 0;

          n.draw.numIndices /= n.indirectPatch.stride;
        }

        // if the actual draw count was greater than 1, display this as an indirect count
        const char *countString = (n.indirectPatch.count > 1 ? "<1>" : "1");

        if(valid)
          n.draw.name = StringFormat::Fmt("%s(%s) => <%u, %u>", n.draw.name.c_str(), countString,
                                          n.draw.numIndices, n.draw.numInstances);
        else
          n.draw.name = StringFormat::Fmt("%s(%s) => <?, ?>", n.draw.name.c_str(), countString);
      }
      else
      {
        // we should have N draws immediately following this one, check that that's the case
        RDCASSERT(i + indirectCount < cmdBufNodes.size(), i, indirectCount, n.indirectPatch.count,
                  cmdBufNodes.size());

        // patch the count onto the root drawcall name. The root is otherwise un-suffixed to allow
        // for collapsing non-multidraws and making everything generally simpler
        if(hasCount)
          n.draw.name = StringFormat::Fmt("%s(<%u>)", n.draw.name.c_str(), indirectCount);
        else
          n.draw.name = StringFormat::Fmt("%s(%u)", n.draw.name.c_str(), n.indirectPatch.count);

        for(size_t j = 0; j < (size_t)indirectCount && i + j + 1 < cmdBufNodes.size(); j++)
        {
          VulkanDrawcallTreeNode &n2 = cmdBufNodes[i + j + 1];

          bool valid =
              PatchIndirectDraw(j, n.indirectPatch.stride, n.indirectPatch.type, n2.draw, ptr, end);

          if(valid)
            n2.draw.name = StringFormat::Fmt("%s[%zu](<%u, %u>)", n2.draw.name.c_str(), j,
                                             n2.draw.numIndices, n2.draw.numInstances);
          else
            n2.draw.name = StringFormat::Fmt("%s[%zu](<?, ?>)", n2.draw.name.c_str(), j);

          if(ptr)
            ptr += n.indirectPatch.stride;
        }
      }
    }

    for(APIEvent &ev : n.draw.events)
    {
      ev.eventId += m_RootEventID;
      m_Events.resize(ev.eventId + 1);
      m_Events[ev.eventId] = ev;
    }

    if(!n.draw.events.empty())
    {
      DrawcallUse use(n.draw.events.back().fileOffset, n.draw.eventId);

      // insert in sorted location
      auto drawit = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
      m_DrawcallUses.insert(drawit - m_DrawcallUses.begin(), use);
    }

    RDCASSERT(n.children.empty());

    for(auto it = n.resourceUsage.begin(); it != n.resourceUsage.end(); ++it)
    {
      EventUsage u = it->second;
      u.eventId += m_RootEventID;
      m_ResourceUses[it->first].push_back(u);
      m_EventFlags[u.eventId] |= PipeRWUsageEventFlags(u.usage);
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

  if(!m_MarkedActive)
  {
    m_MarkedActive = true;
    RenderDoc::Inst().AddActiveDriver(RDCDriver::Vulkan, false);
  }

  if(IsActiveCapturing(m_State))
  {
    // 15 is quite a lot of submissions.
    const int expectedMaxSubmissions = 15;

    RenderDoc::Inst().SetProgress(CaptureProgress::FrameCapture, FakeProgress(m_SubmitCounter, 15));
    m_SubmitCounter++;
  }

  size_t tempmemSize = sizeof(VkSubmitInfo) * submitCount;

  // need to count how many semaphore and command buffer arrays to allocate for
  for(uint32_t i = 0; i < submitCount; i++)
  {
    tempmemSize += pSubmits[i].commandBufferCount * sizeof(VkCommandBuffer);
    tempmemSize += pSubmits[i].signalSemaphoreCount * sizeof(VkSemaphore);
    tempmemSize += pSubmits[i].waitSemaphoreCount * sizeof(VkSemaphore);

    tempmemSize += GetNextPatchSize(pSubmits[i].pNext);
  }

  VkResult ret = VK_SUCCESS;
  bool present = false;
  bool beginCapture = false;
  bool endCapture = false;

  for(uint32_t s = 0; s < submitCount; s++)
  {
    for(uint32_t i = 0; i < pSubmits[s].commandBufferCount; i++)
    {
      VkResourceRecord *record = GetRecord(pSubmits[s].pCommandBuffers[i]);
      present |= record->bakedCommands->cmdInfo->present;
      beginCapture |= record->bakedCommands->cmdInfo->beginCapture;
      endCapture |= record->bakedCommands->cmdInfo->endCapture;
    }
  }

  if(beginCapture)
  {
    RenderDoc::Inst().StartFrameCapture(LayerDisp(m_Instance), NULL);
  }

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    bool capframe = IsActiveCapturing(m_State);
    bool backframe = IsBackgroundCapturing(m_State);

    std::set<ResourceId> refdIDs;

    for(uint32_t s = 0; s < submitCount; s++)
    {
      for(uint32_t i = 0; i < pSubmits[s].commandBufferCount; i++)
      {
        ResourceId cmd = GetResID(pSubmits[s].pCommandBuffers[i]);

        VkResourceRecord *record = GetRecord(pSubmits[s].pCommandBuffers[i]);

        UpdateImageStates(record->bakedCommands->cmdInfo->imageStates);

        if(capframe)
        {
          // for each bound descriptor set, mark it referenced as well as all resources currently
          // bound to it
          for(auto it = record->bakedCommands->cmdInfo->boundDescSets.begin();
              it != record->bakedCommands->cmdInfo->boundDescSets.end(); ++it)
          {
            GetResourceManager()->MarkResourceFrameReferenced(GetResID(*it), eFrameRef_Read);

            VkResourceRecord *setrecord = GetRecord(*it);

            SCOPED_LOCK(setrecord->descInfo->refLock);

            for(auto refit = setrecord->descInfo->bindFrameRefs.begin();
                refit != setrecord->descInfo->bindFrameRefs.end(); ++refit)
            {
              refdIDs.insert(refit->first);
              GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second.second);

              if(refit->second.first & DescriptorSetData::SPARSE_REF_BIT)
              {
                VkResourceRecord *sparserecord =
                    GetResourceManager()->GetResourceRecord(refit->first);

                GetResourceManager()->MarkSparseMapReferenced(sparserecord->resInfo);
              }
            }
            UpdateImageStates(setrecord->descInfo->bindImageStates);
            GetResourceManager()->MergeReferencedMemory(setrecord->descInfo->bindMemRefs);
          }

          for(auto it = record->bakedCommands->cmdInfo->sparse.begin();
              it != record->bakedCommands->cmdInfo->sparse.end(); ++it)
            GetResourceManager()->MarkSparseMapReferenced(*it);

          // pull in frame refs from this baked command buffer
          record->bakedCommands->AddResourceReferences(GetResourceManager());
          record->bakedCommands->AddReferencedIDs(refdIDs);

          GetResourceManager()->MergeReferencedMemory(record->bakedCommands->cmdInfo->memFrameRefs);

          // ref the parent command buffer's alloc record, this will pull in the cmd buffer pool
          GetResourceManager()->MarkResourceFrameReferenced(
              record->cmdInfo->allocRecord->GetResourceID(), eFrameRef_Read);

          const rdcarray<VkResourceRecord *> &subcmds = record->bakedCommands->cmdInfo->subcmds;

          for(size_t sub = 0; sub < subcmds.size(); sub++)
          {
            VkResourceRecord *bakedSubcmds = subcmds[sub]->bakedCommands;
            bakedSubcmds->AddResourceReferences(GetResourceManager());
            bakedSubcmds->AddReferencedIDs(refdIDs);
            UpdateImageStates(bakedSubcmds->cmdInfo->imageStates);
            GetResourceManager()->MergeReferencedMemory(bakedSubcmds->cmdInfo->memFrameRefs);
            GetResourceManager()->MarkResourceFrameReferenced(
                subcmds[sub]->cmdInfo->allocRecord->GetResourceID(), eFrameRef_Read);

            bakedSubcmds->AddRef();
          }

          {
            SCOPED_LOCK(m_CmdBufferRecordsLock);
            m_CmdBufferRecords.push_back(record->bakedCommands);
            for(size_t sub = 0; sub < subcmds.size(); sub++)
              m_CmdBufferRecords.push_back(subcmds[sub]->bakedCommands);
          }

          record->bakedCommands->AddRef();
        }
      }
    }

    if(backframe)
    {
      rdcarray<VkResourceRecord *> maps;
      {
        SCOPED_LOCK(m_CoherentMapsLock);
        maps = m_CoherentMaps;
      }

      for(auto it = maps.begin(); it != maps.end(); ++it)
      {
        VkResourceRecord *record = *it;
        GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(),
                                                          eFrameRef_ReadBeforeWrite);
      }

      // pull in frame refs while background capturing too
      for(uint32_t s = 0; s < submitCount; s++)
      {
        for(uint32_t i = 0; i < pSubmits[s].commandBufferCount; i++)
        {
          VkResourceRecord *record = GetRecord(pSubmits[s].pCommandBuffers[i]);

          for(auto it = record->bakedCommands->cmdInfo->boundDescSets.begin();
              it != record->bakedCommands->cmdInfo->boundDescSets.end(); ++it)
          {
            VkResourceRecord *setrecord = GetRecord(*it);

            SCOPED_LOCK(setrecord->descInfo->refLock);

            GetResourceManager()->MarkBackgroundFrameReferenced(
                setrecord->descInfo->backgroundFrameRefs);
          }

          record->bakedCommands->AddResourceReferences(GetResourceManager());

          for(VkResourceRecord *sub : record->bakedCommands->cmdInfo->subcmds)
            sub->bakedCommands->AddResourceReferences(GetResourceManager());
        }
      }

      // every 20 submits clean background references, in case the application isn't presenting.
      if((Atomic::Inc64(&m_QueueCounter) % 20) == 0)
      {
        GetResourceManager()->CleanBackgroundFrameReferences();
      }
    }

    if(capframe)
    {
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);

      if(fence != VK_NULL_HANDLE)
        GetResourceManager()->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);

      rdcarray<VkResourceRecord *> maps;
      {
        SCOPED_LOCK(m_CoherentMapsLock);
        maps = m_CoherentMaps;
      }

      for(auto it = maps.begin(); it != maps.end(); ++it)
      {
        VkResourceRecord *record = *it;
        MemMapState &state = *record->memMapState;

        SCOPED_LOCK(state.mrLock);

        // potential persistent map
        if(state.mapCoherent && state.mappedPtr && !state.mapFlushed)
        {
          // only need to flush memory that could affect this submitted batch of work
          if(refdIDs.find(record->GetResourceID()) == refdIDs.end())
          {
            RDCDEBUG("Map of memory %s not referenced in this queue - not flushing",
                     ToStr(record->GetResourceID()).c_str());
            continue;
          }

          size_t diffStart = 0, diffEnd = 0;
          bool found = true;

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

          if(state.readbackOnGPU)
          {
            RDCDEBUG("Reading back %s with GPU for comparison",
                     ToStr(record->GetResourceID()).c_str());

            GetDebugManager()->InitReadbackBuffer();

            // immediately issue a command buffer to copy back the data. We do that on this queue to
            // avoid complexity with synchronising with another queue, but the transfer queue if
            // available would be better for this purpose.
            VkCommandBuffer copycmd = GetNextCmd();

            VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                                  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

            ObjDisp(copycmd)->BeginCommandBuffer(Unwrap(copycmd), &beginInfo);

            VkBufferCopy region = {state.mapOffset, state.mapOffset, state.mapSize};

            ObjDisp(copycmd)->CmdCopyBuffer(Unwrap(copycmd), Unwrap(state.wholeMemBuf),
                                            Unwrap(GetDebugManager()->GetReadbackBuffer()), 1,
                                            &region);

            // wait for transfer to finish before reading on CPU
            VkBufferMemoryBarrier bufBarrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                NULL,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_HOST_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                Unwrap(GetDebugManager()->GetReadbackBuffer()),
                0,
                VK_WHOLE_SIZE,
            };

            DoPipelineBarrier(copycmd, 1, &bufBarrier);

            ObjDisp(copycmd)->EndCommandBuffer(Unwrap(copycmd));

            VkSubmitInfo submit = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, UnwrapPtr(copycmd),
            };
            VkResult copyret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submit, VK_NULL_HANDLE);
            RDCASSERTEQUAL(copyret, VK_SUCCESS);

            ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

            VkMappedMemoryRange range = {
                VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                NULL,
                Unwrap(GetDebugManager()->GetReadbackMemory()),
                0,
                VK_WHOLE_SIZE,
            };

            copyret = ObjDisp(queue)->InvalidateMappedMemoryRanges(Unwrap(m_Device), 1, &range);
            RDCASSERTEQUAL(copyret, VK_SUCCESS);

            RemovePendingCommandBuffer(copycmd);
            AddFreeCommandBuffer(copycmd);

            state.cpuReadPtr = GetDebugManager()->GetReadbackPtr();
          }
          else
          {
            state.cpuReadPtr = state.mappedPtr;
          }

          // if we have a previous set of data, compare.
          // otherwise just serialise it all
          if(state.refData)
            found = FindDiffRange(((byte *)state.cpuReadPtr) + state.mapOffset, state.refData,
                                  (size_t)state.mapSize, diffStart, diffEnd);
          else
            diffEnd = (size_t)state.mapSize;

          // sanitise diff start/end. Since the mapped pointer might be written on another thread
          // (or even the GPU) this could cause a difference to appear and disappear transiently. In
          // this case FindDiffRange could find the difference when locating the start but not find
          // it when locating the end. In this case we don't need to write the difference (the
          // application is responsible for ensuring it's not writing to memory the GPU might need)
          if(diffEnd <= diffStart)
            found = false;

          if(found)
          {
            // MULTIDEVICE should find the device for this queue.
            // MULTIDEVICE only want to flush maps associated with this queue
            VkDevice dev = GetDev();

            {
              RDCLOG("Persistent map flush forced for %s (%llu -> %llu)",
                     ToStr(record->GetResourceID()).c_str(), (uint64_t)diffStart, (uint64_t)diffEnd);
              VkMappedMemoryRange range = {
                  VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                  &internalMemoryFlushMarker,
                  (VkDeviceMemory)(uint64_t)record->Resource,
                  state.mapOffset + diffStart,
                  diffEnd - diffStart,
              };
              vkFlushMappedMemoryRanges(dev, 1, &range);
            }
          }
          else
          {
            RDCDEBUG("Persistent map flush not needed for %s",
                     ToStr(record->GetResourceID()).c_str());
          }

          // restore this just in case
          state.cpuReadPtr = state.mappedPtr;
        }
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

      UnwrapNextChain(m_State, "VkSubmitInfo", memory, (VkBaseInStructure *)&unwrappedSubmits[i]);
      appendChain((VkBaseInStructure *)&unwrappedSubmits[i], m_SubmitChain);
    }

    SERIALISE_TIME_CALL(ret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), submitCount,
                                                          unwrappedSubmits, Unwrap(fence)));

    if(capframe)
    {
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
  }

  if(endCapture)
  {
    RenderDoc::Inst().EndFrameCapture(LayerDisp(m_Instance), NULL);
  }

  if(present)
  {
    AdvanceFrame();
    Present(LayerDisp(m_Instance), NULL);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueBindSparse(SerialiserType &ser, VkQueue queue,
                                                uint32_t bindInfoCount,
                                                const VkBindSparseInfo *pBindInfo, VkFence fence)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT(bindInfoCount);
  SERIALISE_ELEMENT_ARRAY(pBindInfo, bindInfoCount);
  SERIALISE_ELEMENT(fence);

  SERIALISE_CHECK_READ_ERRORS();

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
    tempmemSize += GetNextPatchSize(pBindInfo[i].pNext);

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

    UnwrapNextChain(m_State, "VkBindSparseInfo", next, (VkBaseInStructure *)&unwrapped[i]);

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

  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(queue)->QueueBindSparse(Unwrap(queue), bindInfoCount, unwrapped,
                                                            Unwrap(fence)));

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
        GetRecord(bind.buffer)->resInfo->Update(bind.bindCount, bind.pBinds);
      }

      for(uint32_t op = 0; op < pBindInfo[i].imageOpaqueBindCount; op++)
      {
        const VkSparseImageOpaqueMemoryBindInfo &bind = pBindInfo[i].pImageOpaqueBinds[op];
        GetRecord(bind.image)->resInfo->Update(bind.bindCount, bind.pBinds);
      }

      for(uint32_t op = 0; op < pBindInfo[i].imageBindCount; op++)
      {
        const VkSparseImageMemoryBindInfo &bind = pBindInfo[i].pImageBinds[op];
        GetRecord(bind.image)->resInfo->Update(bind.bindCount, bind.pBinds);
      }
    }
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueWaitIdle(SerialiserType &ser, VkQueue queue)
{
  SERIALISE_ELEMENT(queue);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
  }

  return true;
}

VkResult WrappedVulkan::vkQueueWaitIdle(VkQueue queue)
{
  VkResult ret;
  SERIALISE_TIME_CALL(ret = ObjDisp(queue)->QueueWaitIdle(Unwrap(queue)));

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

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueBeginDebugUtilsLabelEXT(SerialiserType &ser, VkQueue queue,
                                                             const VkDebugUtilsLabelEXT *pLabelInfo)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ObjDisp(queue)->QueueBeginDebugUtilsLabelEXT)
      ObjDisp(queue)->QueueBeginDebugUtilsLabelEXT(Unwrap(queue), &Label);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = Label.pLabelName ? Label.pLabelName : "";
      draw.flags |= DrawFlags::PushMarker;

      draw.markerColor[0] = RDCCLAMP(Label.color[0], 0.0f, 1.0f);
      draw.markerColor[1] = RDCCLAMP(Label.color[1], 0.0f, 1.0f);
      draw.markerColor[2] = RDCCLAMP(Label.color[2], 0.0f, 1.0f);
      draw.markerColor[3] = RDCCLAMP(Label.color[3], 0.0f, 1.0f);

      AddEvent();
      m_RootEventID++;
      AddDrawcall(draw, false);

      // now push the drawcall stack
      GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());
    }
    else
    {
      m_RootEventID++;
    }
  }

  return true;
}

void WrappedVulkan::vkQueueBeginDebugUtilsLabelEXT(VkQueue queue,
                                                   const VkDebugUtilsLabelEXT *pLabelInfo)
{
  if(ObjDisp(queue)->QueueBeginDebugUtilsLabelEXT)
  {
    SERIALISE_TIME_CALL(ObjDisp(queue)->QueueBeginDebugUtilsLabelEXT(Unwrap(queue), pLabelInfo));
  }

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueBeginDebugUtilsLabelEXT);
    Serialise_vkQueueBeginDebugUtilsLabelEXT(ser, queue, pLabelInfo);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueEndDebugUtilsLabelEXT(SerialiserType &ser, VkQueue queue)
{
  SERIALISE_ELEMENT(queue);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ObjDisp(queue)->QueueEndDebugUtilsLabelEXT)
      ObjDisp(queue)->QueueEndDebugUtilsLabelEXT(Unwrap(queue));

    if(IsLoading(m_State))
    {
      if(GetDrawcallStack().size() > 1)
        GetDrawcallStack().pop_back();
    }
  }

  return true;
}

void WrappedVulkan::vkQueueEndDebugUtilsLabelEXT(VkQueue queue)
{
  if(ObjDisp(queue)->QueueEndDebugUtilsLabelEXT)
  {
    SERIALISE_TIME_CALL(ObjDisp(queue)->QueueEndDebugUtilsLabelEXT(Unwrap(queue)));
  }

  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueEndDebugUtilsLabelEXT);
    Serialise_vkQueueEndDebugUtilsLabelEXT(ser, queue);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueInsertDebugUtilsLabelEXT(SerialiserType &ser, VkQueue queue,
                                                              const VkDebugUtilsLabelEXT *pLabelInfo)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ObjDisp(queue)->QueueInsertDebugUtilsLabelEXT)
      ObjDisp(queue)->QueueInsertDebugUtilsLabelEXT(Unwrap(queue), &Label);

    if(IsLoading(m_State))
    {
      DrawcallDescription draw;
      draw.name = Label.pLabelName ? Label.pLabelName : "";
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

void WrappedVulkan::vkQueueInsertDebugUtilsLabelEXT(VkQueue queue,
                                                    const VkDebugUtilsLabelEXT *pLabelInfo)
{
  if(ObjDisp(queue)->QueueInsertDebugUtilsLabelEXT)
  {
    SERIALISE_TIME_CALL(ObjDisp(queue)->QueueInsertDebugUtilsLabelEXT(Unwrap(queue), pLabelInfo));
  }

  if(pLabelInfo)
    HandleFrameMarkers(pLabelInfo->pLabelName, queue);
  if(IsActiveCapturing(m_State))
  {
    CACHE_THREAD_SERIALISER();
    ser.SetDrawChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueInsertDebugUtilsLabelEXT);
    Serialise_vkQueueInsertDebugUtilsLabelEXT(ser, queue, pLabelInfo);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetDeviceQueue2(SerialiserType &ser, VkDevice device,
                                                const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT_LOCAL(QueueInfo, *pQueueInfo);
  SERIALISE_ELEMENT_LOCAL(Queue, GetResID(*pQueue)).TypedAs("VkQueue"_lit);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    uint32_t queueFamilyIndex = QueueInfo.queueFamilyIndex;
    uint32_t queueIndex = QueueInfo.queueIndex;

    uint32_t remapFamily = m_QueueRemapping[queueFamilyIndex][queueIndex].family;
    uint32_t remapIndex = m_QueueRemapping[queueFamilyIndex][queueIndex].index;

    if(remapFamily != queueFamilyIndex || remapIndex != queueIndex)
      RDCLOG("Remapped Queue %u/%u from capture to %u/%u on replay", queueFamilyIndex, queueIndex,
             remapFamily, remapIndex);

    VkQueue queue;
    QueueInfo.queueFamilyIndex = remapFamily;
    QueueInfo.queueIndex = remapIndex;
    ObjDisp(device)->GetDeviceQueue2(Unwrap(device), &QueueInfo, &queue);

    GetResourceManager()->WrapResource(Unwrap(device), queue);
    GetResourceManager()->AddLiveResource(Queue, queue);

    if(remapFamily == m_QueueFamilyIdx && m_Queue == VK_NULL_HANDLE)
    {
      m_Queue = queue;

      // we can now submit any cmds that were queued (e.g. from creating debug
      // manager on vkCreateDevice)
      SubmitCmds();
    }

    if(remapFamily < m_ExternalQueues.size())
    {
      if(m_ExternalQueues[remapFamily].queue == VK_NULL_HANDLE)
        m_ExternalQueues[remapFamily].queue = queue;
    }
    else
    {
      RDCERR("Unexpected queue family index %u", remapFamily);
    }

    m_CreationInfo.m_Queue[GetResID(queue)] = remapFamily;

    AddResource(Queue, ResourceType::Queue, "Queue");
    DerivedResource(device, Queue);
  }

  return true;
}

void WrappedVulkan::vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo,
                                      VkQueue *pQueue)
{
  SERIALISE_TIME_CALL(ObjDisp(device)->GetDeviceQueue2(Unwrap(device), pQueueInfo, pQueue));

  if(m_SetDeviceLoaderData)
    m_SetDeviceLoaderData(m_Device, *pQueue);
  else
    SetDispatchTableOverMagicNumber(device, *pQueue);

  RDCASSERT(IsCaptureMode(m_State));

  {
    // it's perfectly valid for enumerate type functions to return the same handle
    // each time. If that happens, we will already have a wrapper created so just
    // return the wrapped object to the user and do nothing else
    if(m_QueueFamilies[pQueueInfo->queueFamilyIndex][pQueueInfo->queueIndex] != VK_NULL_HANDLE)
    {
      *pQueue = m_QueueFamilies[pQueueInfo->queueFamilyIndex][pQueueInfo->queueIndex];
    }
    else
    {
      ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueue);

      {
        Chunk *chunk = NULL;

        {
          CACHE_THREAD_SERIALISER();

          SCOPED_SERIALISE_CHUNK(VulkanChunk::vkGetDeviceQueue2);
          Serialise_vkGetDeviceQueue2(ser, device, pQueueInfo, pQueue);

          chunk = scope.Get();
        }

        VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pQueue);
        RDCASSERT(record);

        record->queueFamilyIndex = pQueueInfo->queueFamilyIndex;

        VkResourceRecord *instrecord = GetRecord(m_Instance);

        // treat queues as pool members of the instance (ie. freed when the instance dies)
        {
          instrecord->LockChunks();
          instrecord->pooledChildren.push_back(record);
          instrecord->UnlockChunks();
        }

        record->AddChunk(chunk);
      }

      m_QueueFamilies[pQueueInfo->queueFamilyIndex][pQueueInfo->queueIndex] = *pQueue;

      if(pQueueInfo->queueFamilyIndex < m_ExternalQueues.size())
      {
        if(m_ExternalQueues[pQueueInfo->queueFamilyIndex].queue == VK_NULL_HANDLE)
          m_ExternalQueues[pQueueInfo->queueFamilyIndex].queue = *pQueue;
      }
      else
      {
        RDCERR("Unexpected queue family index %u", pQueueInfo->queueFamilyIndex);
      }

      if(pQueueInfo->queueFamilyIndex == m_QueueFamilyIdx)
      {
        m_Queue = *pQueue;

        // we can now submit any cmds that were queued (e.g. from creating debug
        // manager on vkCreateDevice)
        SubmitCmds();
      }
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, vkGetDeviceQueue, VkDevice device, uint32_t queueFamilyIndex,
                                uint32_t queueIndex, VkQueue *pQueue);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueSubmit, VkQueue queue, uint32_t submitCount,
                                const VkSubmitInfo *pSubmits, VkFence fence);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueBindSparse, VkQueue queue, uint32_t bindInfoCount,
                                const VkBindSparseInfo *pBindInfo, VkFence fence);

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueWaitIdle, VkQueue queue);

INSTANTIATE_FUNCTION_SERIALISED(void, vkQueueBeginDebugUtilsLabelEXT, VkQueue queue,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkQueueEndDebugUtilsLabelEXT, VkQueue queue);

INSTANTIATE_FUNCTION_SERIALISED(void, vkQueueInsertDebugUtilsLabelEXT, VkQueue queue,
                                const VkDebugUtilsLabelEXT *pLabelInfo);

INSTANTIATE_FUNCTION_SERIALISED(void, vkGetDeviceQueue2, VkDevice device,
                                const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue);
