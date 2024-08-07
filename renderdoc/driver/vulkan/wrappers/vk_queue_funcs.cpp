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

#include <algorithm>
#include "../vk_core.h"
#include "../vk_debug.h"
#include "core/settings.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_VerboseCommandRecording);
RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkGetDeviceQueue(SerialiserType &ser, VkDevice device,
                                               uint32_t queueFamilyIndex, uint32_t queueIndex,
                                               VkQueue *pQueue)
{
  SERIALISE_ELEMENT(device);
  SERIALISE_ELEMENT(queueFamilyIndex).Important();
  SERIALISE_ELEMENT(queueIndex).Important();
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

void WrappedVulkan::DoSubmit(VkQueue queue, VkSubmitInfo2 submitInfo)
{
  // don't submit any semaphores
  submitInfo.waitSemaphoreInfoCount = 0;
  submitInfo.pWaitSemaphoreInfos = NULL;
  submitInfo.signalSemaphoreInfoCount = 0;
  submitInfo.pSignalSemaphoreInfos = NULL;

  if(GetExtensions(NULL).ext_KHR_synchronization2)
  {
    // if we have KHR_sync2 this is easy! unwrap, add our submit chain, and do it

    byte *tempMem = GetTempMemory(GetNextPatchSize(&submitInfo));
    VkSubmitInfo2 *unwrapped = UnwrapStructAndChain(m_State, tempMem, &submitInfo);
    AppendNextStruct(*unwrapped, m_SubmitChain);

    // don't submit the fence, since we have nothing to wait on it being signalled, and we
    // might not have it correctly in the unsignalled state.
    ObjDisp(queue)->QueueSubmit2(Unwrap(queue), 1, unwrapped, VK_NULL_HANDLE);
  }
  else
  {
    // otherwise we need to decompose into an original submit

    VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    rdcarray<VkCommandBuffer> commandBuffers;
    rdcarray<uint32_t> groupMasks;

    VkProtectedSubmitInfo prot = {VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO, NULL, VK_TRUE};
    VkDeviceGroupSubmitInfo group = {VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO};

    // we expect the pNext chain to be NULL, as there's nothing we should be replaying that we can
    // represent in this decomposed version
    RDCASSERTEQUAL((void *)submitInfo.pNext, (void *)NULL);

    if(submitInfo.flags & VK_SUBMIT_PROTECTED_BIT)
    {
      // we created the protected structure with the flag as TRUE since otherwise we just don't
      // chain it on at all
      AppendNextStruct(info, &prot);
    }

    commandBuffers.resize(submitInfo.commandBufferInfoCount);
    for(uint32_t i = 0; i < submitInfo.commandBufferInfoCount; i++)
    {
      commandBuffers[i] = submitInfo.pCommandBufferInfos[i].commandBuffer;

      if(submitInfo.pCommandBufferInfos[i].deviceMask != 0)
      {
        groupMasks.resize(submitInfo.commandBufferInfoCount);
        groupMasks[i] = submitInfo.pCommandBufferInfos[i].deviceMask;
      }
    }

    info.commandBufferCount = submitInfo.commandBufferInfoCount;
    info.pCommandBuffers = commandBuffers.data();

    if(!groupMasks.empty())
    {
      group.commandBufferCount = info.commandBufferCount;
      group.pCommandBufferDeviceMasks = groupMasks.data();
      // if we set up group masks, chain on the struct
      AppendNextStruct(info, &group);
    }

    byte *tempMem = GetTempMemory(GetNextPatchSize(&info));
    VkSubmitInfo *unwrapped = UnwrapStructAndChain(m_State, tempMem, &info);
    AppendNextStruct(*unwrapped, m_SubmitChain);

    // don't submit the fence, since we have nothing to wait on it being signalled, and we
    // might not have it correctly in the unsignalled state.
    ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, unwrapped, VK_NULL_HANDLE);
  }
}

WrappedVulkan::CommandBufferNode *WrappedVulkan::BuildSubmitTree(ResourceId cmdId, uint32_t curEvent,
                                                                 CommandBufferNode *rootNode)
{
  CommandBufferNode *cmdNode = new CommandBufferNode();
  cmdNode->cmdId = cmdId;
  cmdNode->beginEvent = curEvent;

  // setting the root node of the primary to itself simplifies building the tree here, as well as
  // building the partial stack during active replay.
  if(rootNode == NULL)
    rootNode = cmdNode;

  cmdNode->rootNode = rootNode;

  m_Partial.submitLookup[cmdId].push_back(cmdNode);

  const rdcarray<CommandBufferExecuteInfo> &executedCmds = m_CommandBufferExecutes[cmdId];

  for(const CommandBufferExecuteInfo &childExecuteInfo : executedCmds)
  {
    CommandBufferNode *rebaseChild = BuildSubmitTree(
        childExecuteInfo.cmdId, cmdNode->beginEvent + childExecuteInfo.relPos, rootNode);
    cmdNode->childCmdNodes.push_back(rebaseChild);
  }

  return cmdNode;
}

void WrappedVulkan::ReplayQueueSubmit(VkQueue queue, VkSubmitInfo2 submitInfo, rdcstr basename)
{
  if(IsLoading(m_State))
  {
    DoSubmit(queue, submitInfo);

    AddEvent();

    // we're adding multiple events, need to increment ourselves
    m_RootEventID++;

    if(submitInfo.commandBufferInfoCount == 0)
    {
      rdcstr name = StringFormat::Fmt("=> %s: No Command Buffers", basename.c_str());

      ActionDescription action;
      action.customName = name;
      action.flags |= ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary;
      AddEvent();

      m_RootEvents.back().chunkIndex = APIEvent::NoChunk;
      m_Events.back().chunkIndex = APIEvent::NoChunk;

      AddAction(action);
      m_RootEventID++;
    }

    for(uint32_t c = 0; c < submitInfo.commandBufferInfoCount; c++)
    {
      ResourceId cmd = GetResourceManager()->GetOriginalID(
          GetResID(submitInfo.pCommandBufferInfos[c].commandBuffer));

      BakedCmdBufferInfo &cmdBufInfo = m_BakedCmdBufferInfo[cmd];

      UpdateImageStates(m_BakedCmdBufferInfo[cmd].imageStates);

      rdcstr name = StringFormat::Fmt("=> %s[%u]: vkBeginCommandBuffer(%s)", basename.c_str(), c,
                                      ToStr(cmd).c_str());

      ActionDescription action;
      {
        // add a fake marker
        action.customName = name;
        action.flags |=
            ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary | ActionFlags::BeginPass;
        AddEvent();

        m_RootEvents.back().chunkIndex = cmdBufInfo.beginChunk;
        m_Events.back().chunkIndex = cmdBufInfo.beginChunk;

        AddAction(action);
        m_RootEventID++;
      }

      // insert the baked command buffer in-line into this list of notes, assigning new event
      // and drawIDs
      InsertActionsAndRefreshIDs(cmdBufInfo);

      // only primary command buffers can be submitted
      CommandBufferNode *rebaseNode = BuildSubmitTree(cmd, m_RootEventID);

      m_Partial.commandTree.push_back(rebaseNode);

      for(size_t i = 0; i < cmdBufInfo.debugMessages.size(); i++)
      {
        m_DebugMessages.push_back(cmdBufInfo.debugMessages[i]);
        m_DebugMessages.back().eventId += m_RootEventID;
      }

      m_RootEventID += cmdBufInfo.eventCount;
      m_RootActionID += cmdBufInfo.actionCount;

      {
        // pull in any remaining events on the command buffer that weren't added to an action
        uint32_t i = 0;
        for(APIEvent &apievent : cmdBufInfo.curEvents)
        {
          apievent.eventId = m_RootEventID - cmdBufInfo.curEvents.count() + i;

          m_RootEvents.push_back(apievent);
          m_Events.resize(apievent.eventId + 1);
          m_Events[apievent.eventId] = apievent;

          i++;
        }

        for(auto it = cmdBufInfo.resourceUsage.begin(); it != cmdBufInfo.resourceUsage.end(); ++it)
        {
          EventUsage u = it->second;
          u.eventId += m_RootEventID - cmdBufInfo.curEvents.count();
          m_ResourceUses[it->first].push_back(u);
          m_EventFlags[u.eventId] |= PipeRWUsageEventFlags(u.usage);
        }

        name = StringFormat::Fmt("=> %s[%u]: vkEndCommandBuffer(%s)", basename.c_str(), c,
                                 ToStr(cmd).c_str());
        action.customName = name;
        action.flags =
            ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary | ActionFlags::EndPass;
        AddEvent();

        m_RootEvents.back().chunkIndex = cmdBufInfo.endChunk;
        m_Events.back().chunkIndex = cmdBufInfo.endChunk;

        AddAction(action);
        m_RootEventID++;
      }
    }

    // account for the outer loop thinking we've added one event and incrementing,
    // since we've done all the handling ourselves this will be off by one.
    m_RootEventID--;
  }
  else
  {
    // account for the queue submit event
    m_RootEventID++;

    if(submitInfo.commandBufferInfoCount == 0)
    {
      // account for the "No Command Buffers" virtual label
      m_RootEventID++;
      m_RootActionID++;
    }

    uint32_t startEID = m_RootEventID;

    // advance m_CurEventID to match the events added when reading
    for(uint32_t c = 0; c < submitInfo.commandBufferInfoCount; c++)
    {
      ResourceId cmd = GetResourceManager()->GetOriginalID(
          GetResID(submitInfo.pCommandBufferInfos[c].commandBuffer));

      m_RootEventID += m_BakedCmdBufferInfo[cmd].eventCount;
      m_RootActionID += m_BakedCmdBufferInfo[cmd].actionCount;

      // 2 extra for the virtual labels around the command buffer
      {
        m_RootEventID += 2;
        m_RootActionID += 2;
      }
    }

    // same accounting for the outer loop as above
    m_RootEventID--;

    if(submitInfo.commandBufferInfoCount == 0)
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

      rdcarray<VkCommandBufferSubmitInfo> rerecordedCmds;

      for(uint32_t c = 0; c < submitInfo.commandBufferInfoCount; c++)
      {
        VkCommandBufferSubmitInfo info = submitInfo.pCommandBufferInfos[c];
        ResourceId cmdId = GetResourceManager()->GetOriginalID(GetResID(info.commandBuffer));

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
          info.commandBuffer = cmd;
          rerecordedCmds.push_back(info);

          UpdateImageStates(m_BakedCmdBufferInfo[cmdId].imageStates);
        }
        else
        {
#if ENABLED(VERBOSE_PARTIAL_REPLAY)
          RDCDEBUG("Queue not submitting %s", ToStr(cmdId).c_str());
#endif
        }

        eid += m_BakedCmdBufferInfo[cmdId].eventCount;

        // 1 extra to account for the virtual end command buffer label (begin is accounted for
        // above)
        eid++;
      }

      submitInfo.pCommandBufferInfos = rerecordedCmds.data();

      if(Vulkan_Debug_SingleSubmitFlushing())
      {
        submitInfo.commandBufferInfoCount = 1;
        for(size_t i = 0; i < rerecordedCmds.size(); i++)
        {
          DoSubmit(queue, submitInfo);
          submitInfo.pCommandBufferInfos++;

          FlushQ();
        }
      }
      else
      {
        submitInfo.commandBufferInfoCount = (uint32_t)rerecordedCmds.size();

        DoSubmit(queue, submitInfo);
      }
    }
  }

  if(Vulkan_Debug_SingleSubmitFlushing())
    FlushQ();
}

bool WrappedVulkan::PatchIndirectDraw(size_t drawIndex, uint32_t paramStride,
                                      VkIndirectPatchType type, ActionDescription &action,
                                      byte *&argptr, byte *argend)
{
  bool valid = false;

  action.drawIndex = (uint32_t)drawIndex;

  if(type == VkIndirectPatchType::MeshIndirectCount)
  {
    if(argptr && argptr + sizeof(VkDrawMeshTasksIndirectCommandEXT) <= argend)
    {
      VkDrawMeshTasksIndirectCommandEXT *arg = (VkDrawMeshTasksIndirectCommandEXT *)argptr;

      action.dispatchDimension[0] = arg->groupCountX;
      action.dispatchDimension[1] = arg->groupCountY;
      action.dispatchDimension[2] = arg->groupCountZ;

      valid = true;
    }
  }
  else if(type == VkIndirectPatchType::DrawIndirect || type == VkIndirectPatchType::DrawIndirectCount)
  {
    if(argptr && argptr + sizeof(VkDrawIndirectCommand) <= argend)
    {
      VkDrawIndirectCommand *arg = (VkDrawIndirectCommand *)argptr;

      action.numIndices = arg->vertexCount;
      action.numInstances = arg->instanceCount;
      action.vertexOffset = arg->firstVertex;
      action.instanceOffset = arg->firstInstance;

      valid = true;
    }
  }
  else if(type == VkIndirectPatchType::DrawIndirectByteCount)
  {
    if(argptr && argptr + 4 <= argend)
    {
      uint32_t *arg = (uint32_t *)argptr;

      action.numIndices = *arg;

      valid = true;
    }
  }
  else if(type == VkIndirectPatchType::DrawIndexedIndirect ||
          type == VkIndirectPatchType::DrawIndexedIndirectCount)
  {
    if(argptr && argptr + sizeof(VkDrawIndexedIndirectCommand) <= argend)
    {
      VkDrawIndexedIndirectCommand *arg = (VkDrawIndexedIndirectCommand *)argptr;

      action.numIndices = arg->indexCount;
      action.numInstances = arg->instanceCount;
      action.baseVertex = arg->vertexOffset;
      action.indexOffset = arg->firstIndex;
      action.instanceOffset = arg->firstInstance;

      valid = true;
    }
  }
  else
  {
    RDCERR("Unexpected indirect action type");
  }

  if(valid && !action.events.empty())
  {
    SDChunk *chunk = m_StructuredFile->chunks[action.events.back().chunkIndex];

    if(chunk->metadata.chunkID != (uint32_t)VulkanChunk::vkCmdIndirectSubCommand)
      chunk = m_StructuredFile->chunks[action.events.back().chunkIndex - 1];

    SDObject *drawIdx = chunk->FindChild("drawIndex");

    if(drawIdx)
      drawIdx->data.basic.u = drawIndex;

    SDObject *offset = chunk->FindChild("offset");

    if(offset)
      offset->data.basic.u += drawIndex * paramStride;

    SDObject *command = chunk->FindChild("command");

    // single action indirect draws don't have a command child since it can't be added without
    // breaking serialising the chunk.
    if(command)
    {
      // patch up structured data contents
      if(SDObject *sub = command->FindChild("vertexCount"))
        sub->data.basic.u = action.numIndices;
      if(SDObject *sub = command->FindChild("indexCount"))
        sub->data.basic.u = action.numIndices;
      if(SDObject *sub = command->FindChild("instanceCount"))
        sub->data.basic.u = action.numInstances;
      if(SDObject *sub = command->FindChild("firstVertex"))
        sub->data.basic.u = action.vertexOffset;
      if(SDObject *sub = command->FindChild("vertexOffset"))
        sub->data.basic.u = action.baseVertex;
      if(SDObject *sub = command->FindChild("firstIndex"))
        sub->data.basic.u = action.indexOffset;
      if(SDObject *sub = command->FindChild("firstInstance"))
        sub->data.basic.u = action.instanceOffset;
    }
  }

  return valid;
}

void WrappedVulkan::InsertActionsAndRefreshIDs(BakedCmdBufferInfo &cmdBufInfo)
{
  rdcarray<VulkanActionTreeNode> &cmdBufNodes = cmdBufInfo.action->children;

  // assign new action IDs
  for(size_t i = 0; i < cmdBufNodes.size(); i++)
  {
    VulkanActionTreeNode n = cmdBufNodes[i];
    n.action.eventId += m_RootEventID;
    n.action.actionId += m_RootActionID;

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

      n.action.customName =
          StringFormat::Fmt("vkCmdDispatchIndirect(<%u, %u, %u>)", args->x, args->y, args->z);
      n.action.dispatchDimension[0] = args->x;
      n.action.dispatchDimension[1] = args->y;
      n.action.dispatchDimension[2] = args->z;
    }
    else if(n.indirectPatch.type == VkIndirectPatchType::MeshIndirect)
    {
      VkDrawMeshTasksIndirectCommandEXT unknown = {0};
      bytebuf argbuf;
      GetDebugManager()->GetBufferData(GetResID(n.indirectPatch.buf), 0, 0, argbuf);
      VkDrawMeshTasksIndirectCommandEXT *args = (VkDrawMeshTasksIndirectCommandEXT *)&argbuf[0];

      if(argbuf.size() < sizeof(VkDrawMeshTasksIndirectCommandEXT))
      {
        RDCERR("Couldn't fetch arguments buffer for vkCmdDrawMeshTasksIndirectEXT");
        args = &unknown;
      }

      n.action.customName =
          StringFormat::Fmt("vkCmdDrawMeshTasksIndirectEXT(<%u, %u, %u>)", args->groupCountX,
                            args->groupCountY, args->groupCountZ);
      n.action.dispatchDimension[0] = args->groupCountX;
      n.action.dispatchDimension[1] = args->groupCountY;
      n.action.dispatchDimension[2] = args->groupCountZ;
    }
    else if(n.indirectPatch.type == VkIndirectPatchType::DrawIndirectByteCount ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndirect ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndexedIndirect ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndirectCount ||
            n.indirectPatch.type == VkIndirectPatchType::DrawIndexedIndirectCount ||
            n.indirectPatch.type == VkIndirectPatchType::MeshIndirectCount)
    {
      bool hasCount = (n.indirectPatch.type == VkIndirectPatchType::DrawIndirectCount ||
                       n.indirectPatch.type == VkIndirectPatchType::DrawIndexedIndirectCount ||
                       n.indirectPatch.type == VkIndirectPatchType::MeshIndirectCount);
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
          RDCERR("Couldn't get indirect action count");
        }

        if(indirectCount > n.indirectPatch.count)
        {
          RDCERR("Indirect count higher than maxCount, clamping");
          indirectCount = n.indirectPatch.count;
        }

        // this can be negative if indirectCount is 0
        int32_t eidShift = indirectCount - 1;

        // we reserved one event and action for the indirect count based action.
        // if we ended up with a different number eidShift will be non-zero, so we need to adjust
        // all subsequent EIDs and action IDs and either remove the subdraw we allocated (if no
        // draws
        // happened) or clone the subdraw to create more that we can then patch.
        if(eidShift != 0)
        {
          // i is the pushmarker, so i + 1 is the sub draws, and i + 2 is the pop marker.
          // adjust all EIDs and action IDs after that point
          for(size_t j = i + 2; j < cmdBufNodes.size(); j++)
          {
            cmdBufNodes[j].action.eventId += eidShift;
            cmdBufNodes[j].action.actionId += eidShift;

            for(APIEvent &ev : cmdBufNodes[j].action.events)
              ev.eventId += eidShift;

            for(rdcpair<ResourceId, EventUsage> &use : cmdBufNodes[j].resourceUsage)
              use.second.eventId += eidShift;
          }

          for(size_t j = 0; j < cmdBufInfo.debugMessages.size(); j++)
          {
            if(cmdBufInfo.debugMessages[j].eventId >= cmdBufNodes[i].action.eventId + 2)
              cmdBufInfo.debugMessages[j].eventId += eidShift;
          }

          cmdBufInfo.eventCount += eidShift;
          cmdBufInfo.actionCount += eidShift;

          // we also need to patch the original secondary command buffer here, if the indirect call
          // was on a secondary, so that vkCmdExecuteCommands knows accurately how many events are
          // in the command buffer.
          if(n.indirectPatch.commandBuffer != ResourceId())
          {
            m_BakedCmdBufferInfo[n.indirectPatch.commandBuffer].eventCount += eidShift;
            m_BakedCmdBufferInfo[n.indirectPatch.commandBuffer].actionCount += eidShift;
          }

          RDCASSERT(cmdBufNodes[i + 1].action.events.size() == 1);
          uint32_t chunkIndex = cmdBufNodes[i + 1].action.events[0].chunkIndex;

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
            m_StructuredFile->chunks.reserve(m_StructuredFile->chunks.size() + eidShift);
            for(int32_t e = 0; e < eidShift; e++)
              m_StructuredFile->chunks.push_back(chunk->Duplicate());

            // now copy the subdraw so we're not inserting into the array from itself
            VulkanActionTreeNode node = cmdBufNodes[i + 1];

            cmdBufNodes.resize(cmdBufNodes.size() + eidShift);
            for(size_t e = cmdBufNodes.size() - 1; e > i + 1 + eidShift; e--)
              cmdBufNodes[e] = std::move(cmdBufNodes[e - eidShift]);

            // then insert enough duplicates
            for(int32_t e = 0; e < eidShift; e++)
            {
              node.action.eventId++;
              node.action.actionId++;

              for(APIEvent &ev : node.action.events)
              {
                ev.eventId++;
                ev.chunkIndex = baseAddedChunk + e;
              }

              for(rdcpair<ResourceId, EventUsage> &use : node.resourceUsage)
                use.second.eventId++;

              cmdBufNodes[i + 2 + e] = node;
            }
          }
        }
      }

      // indirect count versions always have a multidraw marker regions, but static count of 1 would
      // be in-lined as a single action, so we patch in-place
      if(!hasCount && indirectCount == 1)
      {
        rdcstr name = GetStructuredFile()->chunks[n.action.events.back().chunkIndex]->name;

        bool valid =
            PatchIndirectDraw(0, n.indirectPatch.stride, n.indirectPatch.type, n.action, ptr, end);

        if(n.indirectPatch.type == VkIndirectPatchType::DrawIndirectByteCount)
        {
          if(n.action.numIndices > n.indirectPatch.vertexoffset)
            n.action.numIndices -= n.indirectPatch.vertexoffset;
          else
            n.action.numIndices = 0;

          n.action.numIndices /= n.indirectPatch.stride;
        }

        // if the actual action count was greater than 1, display this as an indirect count
        const char *countString = (n.indirectPatch.count > 1 ? "<1>" : "1");

        if(valid)
          n.action.customName = StringFormat::Fmt("%s(%s) => <%u, %u>", name.c_str(), countString,
                                                  n.action.numIndices, n.action.numInstances);
        else
          n.action.customName = StringFormat::Fmt("%s(%s) => <?, ?>", name.c_str(), countString);
      }
      else
      {
        // we should have N draws immediately following this one, check that that's the case
        RDCASSERT(i + indirectCount < cmdBufNodes.size(), i, indirectCount, n.indirectPatch.count,
                  cmdBufNodes.size());

        rdcstr name = GetStructuredFile()->chunks[n.action.events.back().chunkIndex]->name;

        // patch the count onto the root action name. The root is otherwise un-suffixed to allow
        // for collapsing non-multidraws and making everything generally simpler
        if(hasCount)
          n.action.customName = StringFormat::Fmt("%s(<%u>)", name.c_str(), indirectCount);
        else
          n.action.customName = StringFormat::Fmt("%s(%u)", name.c_str(), n.indirectPatch.count);

        for(size_t j = 0; j < (size_t)indirectCount && i + j + 1 < cmdBufNodes.size(); j++)
        {
          VulkanActionTreeNode &n2 = cmdBufNodes[i + j + 1];

          bool valid = PatchIndirectDraw(j, n.indirectPatch.stride, n.indirectPatch.type, n2.action,
                                         ptr, end);

          name = GetStructuredFile()->chunks[n2.action.events.back().chunkIndex]->name;

          if(n.indirectPatch.type == VkIndirectPatchType::MeshIndirectCount)
          {
            if(valid)
              n2.action.customName = StringFormat::Fmt(
                  "%s[%zu](<%u, %u, %u>)", name.c_str(), j, n2.action.dispatchDimension[0],
                  n2.action.dispatchDimension[1], n2.action.dispatchDimension[2]);
            else
              n2.action.customName = StringFormat::Fmt("%s[%zu](<?, ?>)", name.c_str(), j);
          }
          else
          {
            if(valid)
              n2.action.customName = StringFormat::Fmt("%s[%zu](<%u, %u>)", name.c_str(), j,
                                                       n2.action.numIndices, n2.action.numInstances);
            else
              n2.action.customName = StringFormat::Fmt("%s[%zu](<?, ?>)", name.c_str(), j);
          }

          if(ptr)
            ptr += n.indirectPatch.stride;
        }
      }
    }

    for(APIEvent &ev : n.action.events)
    {
      ev.eventId += m_RootEventID;
      m_Events.resize(ev.eventId + 1);
      m_Events[ev.eventId] = ev;
    }

    if(!n.action.events.empty())
    {
      ActionUse use(n.action.events.back().fileOffset, n.action.eventId);

      // insert in sorted location
      auto drawit = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);
      m_ActionUses.insert(drawit - m_ActionUses.begin(), use);
    }

    RDCASSERT(n.children.empty());

    for(auto it = n.resourceUsage.begin(); it != n.resourceUsage.end(); ++it)
    {
      EventUsage u = it->second;
      u.eventId += m_RootEventID;
      m_ResourceUses[it->first].push_back(u);
      m_EventFlags[u.eventId] |= PipeRWUsageEventFlags(u.usage);
    }

    GetActionStack().back()->children.push_back(n);

    // if this is a push marker too, step down the action stack
    if(cmdBufNodes[i].action.flags & ActionFlags::PushMarker)
      GetActionStack().push_back(&GetActionStack().back()->children.back());

    // similarly for a pop, but don't pop off the root
    if((cmdBufNodes[i].action.flags & ActionFlags::PopMarker) && GetActionStack().size() > 1)
      GetActionStack().pop_back();
  }
}

void WrappedVulkan::AddReferencesForSecondaries(VkResourceRecord *record,
                                                rdcarray<VkResourceRecord *> &cmdsWithReferences,
                                                std::unordered_set<ResourceId> &refdIDs)
{
  // cannot add references here until after we've done descriptor sets later, see comment
  // in CaptureQueueSubmit.
  // bakedSubcmds->AddResourceReferences(GetResourceManager());
  // GetResourceManager()->MergeReferencedMemory(bakedSubcmds->cmdInfo->memFrameRefs);
  // UpdateImageStates(bakedSubcmds->cmdInfo->imageStates);
  const rdcarray<VkResourceRecord *> &subcmds = record->bakedCommands->cmdInfo->subcmds;

  for(VkResourceRecord *subcmd : subcmds)
  {
    cmdsWithReferences.push_back(subcmd->bakedCommands);

    subcmd->bakedCommands->AddReferencedIDs(refdIDs);

    GetResourceManager()->MarkResourceFrameReferenced(subcmd->cmdInfo->allocRecord->GetResourceID(),
                                                      eFrameRef_Read);

    subcmd->bakedCommands->AddRef();

    AddReferencesForSecondaries(subcmd, cmdsWithReferences, refdIDs);
  }
}

void WrappedVulkan::AddRecordsForSecondaries(VkResourceRecord *record)
{
  const rdcarray<VkResourceRecord *> &subcmds = record->bakedCommands->cmdInfo->subcmds;

  for(VkResourceRecord *subcmd : subcmds)
  {
    m_CmdBufferRecords.push_back(subcmd->bakedCommands);
    AddRecordsForSecondaries(subcmd);
  }
}

void WrappedVulkan::UpdateImageStatesForSecondaries(VkResourceRecord *record,
                                                    rdcarray<VkResourceRecord *> &accelerationStructures)
{
  const rdcarray<VkResourceRecord *> &subcmds = record->bakedCommands->cmdInfo->subcmds;

  for(VkResourceRecord *subcmd : subcmds)
  {
    accelerationStructures.append(subcmd->bakedCommands->cmdInfo->accelerationStructures);

    subcmd->bakedCommands->AddResourceReferences(GetResourceManager());
    UpdateImageStates(subcmd->bakedCommands->cmdInfo->imageStates);
    UpdateImageStatesForSecondaries(subcmd, accelerationStructures);
  }
}

void WrappedVulkan::CaptureQueueSubmit(VkQueue queue,
                                       const rdcarray<VkCommandBuffer> &commandBuffers, VkFence fence)
{
  bool capframe = IsActiveCapturing(m_State);
  bool backframe = IsBackgroundCapturing(m_State);

  std::unordered_set<ResourceId> refdIDs;

  std::set<rdcpair<ResourceId, VkResourceRecord *>> capDescriptors;
  std::set<rdcpair<ResourceId, VkResourceRecord *>> descriptorSets;
  rdcarray<VkResourceRecord *> cmdsWithReferences;
  rdcarray<VkResourceRecord *> accelerationStructures;

  // pull in any copy sources, conservatively
  if(capframe)
  {
    SCOPED_LOCK(m_CapDescriptorsLock);
    capDescriptors.swap(m_CapDescriptors);
  }

  descriptorSets = capDescriptors;

  for(size_t i = 0; i < commandBuffers.size(); i++)
  {
    ResourceId cmd = GetResID(commandBuffers[i]);

    VkResourceRecord *record = GetRecord(commandBuffers[i]);

    if(Vulkan_Debug_VerboseCommandRecording())
    {
      RDCLOG("vkQueueSubmit() to queue %s, cmd %zu of %zu: %s baked to %s",
             ToStr(GetResID(queue)).c_str(), i, commandBuffers.size(),
             ToStr(record->GetResourceID()).c_str(),
             ToStr(record->bakedCommands->GetResourceID()).c_str());
    }

    if(capframe)
    {
      // add the bound descriptor sets
      for(auto it = record->bakedCommands->cmdInfo->boundDescSets.begin();
          it != record->bakedCommands->cmdInfo->boundDescSets.end(); ++it)
      {
        descriptorSets.insert(*it);
      }

      for(auto it = record->bakedCommands->cmdInfo->sparse.begin();
          it != record->bakedCommands->cmdInfo->sparse.end(); ++it)
        GetResourceManager()->MarkSparseMapReferenced(*it);

      // can't pull in frame refs here, we need to do these last, since they are disjoint from
      // descriptor references and we have to apply those first to be conservative.
      // For example say a buffer is referenced in a descriptor for read, then completely
      // overwritten in a later CmdFillBuffer. The fill will be listed in the normal references
      // here, the descriptor reference is lazy tracked and only added below when we handle those.
      // Since we don't know the ordering, we do it in the most conservative order - descriptor
      // references can never be CompleteWrite, they're either read-only or read-before-write for
      // storage descriptors.
      // record->bakedCommands->AddResourceReferences(GetResourceManager());
      // GetResourceManager()->MergeReferencedMemory(record->bakedCommands->cmdInfo->memFrameRefs);
      // UpdateImageStates(record->bakedCommands->cmdInfo->imageStates);
      cmdsWithReferences.push_back(record->bakedCommands);
      record->bakedCommands->AddReferencedIDs(refdIDs);

      // ref the parent command buffer's alloc record, this will pull in the cmd buffer pool
      GetResourceManager()->MarkResourceFrameReferenced(
          record->cmdInfo->allocRecord->GetResourceID(), eFrameRef_Read);

      // cannot add references here until after we've done descriptor sets later, see comment
      // above
      // bakedSubcmds->AddResourceReferences(GetResourceManager());
      // GetResourceManager()->MergeReferencedMemory(bakedSubcmds->cmdInfo->memFrameRefs);
      // UpdateImageStates(bakedSubcmds->cmdInfo->imageStates);
      AddReferencesForSecondaries(record, cmdsWithReferences, refdIDs);

      {
        SCOPED_LOCK(m_CmdBufferRecordsLock);
        m_CmdBufferRecords.push_back(record->bakedCommands);
        AddRecordsForSecondaries(record);
      }

      record->bakedCommands->AddRef();
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
    for(size_t i = 0; i < commandBuffers.size(); i++)
    {
      VkResourceRecord *record = GetRecord(commandBuffers[i]);

      accelerationStructures.append(record->bakedCommands->cmdInfo->accelerationStructures);

      record->bakedCommands->AddResourceReferences(GetResourceManager());
      UpdateImageStates(record->bakedCommands->cmdInfo->imageStates);
      UpdateImageStatesForSecondaries(record, accelerationStructures);
    }

    // every 20 submits clean background references, in case the application isn't presenting.
    if((Atomic::Inc64(&m_QueueCounter) % 20) == 0)
    {
      GetResourceManager()->CleanBackgroundFrameReferences();
    }
  }

  if(capframe)
  {
    VulkanResourceManager *rm = GetResourceManager();

    // for each descriptor set, mark it referenced as well as all resources currently bound to it
    for(auto it = descriptorSets.begin(); it != descriptorSets.end(); ++it)
    {
      rm->MarkResourceFrameReferenced(it->first, eFrameRef_Read);

      VkResourceRecord *setrecord = it->second;

      DescriptorBindRefs refs;

      DescSetLayout *layout = setrecord->descInfo->layout;

      for(size_t b = 0, num = layout->bindings.size(); b < num; b++)
      {
        const DescSetLayout::Binding &bind = layout->bindings[b];

        // skip empty bindings or inline uniform blocks
        if(bind.layoutDescType == VK_DESCRIPTOR_TYPE_MAX_ENUM ||
           bind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
          continue;

        uint32_t count = bind.descriptorCount;
        if(bind.variableSize)
          count = setrecord->descInfo->data.variableDescriptorCount;

        for(uint32_t a = 0; a < count; a++)
          setrecord->descInfo->data.binds[b][a].AccumulateBindRefs(refs, rm);
      }

      for(auto refit = refs.bindFrameRefs.begin(); refit != refs.bindFrameRefs.end(); ++refit)
      {
        refdIDs.insert(refit->first);
        GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second);
      }

      for(auto refit = refs.sparseRefs.begin(); refit != refs.sparseRefs.end(); ++refit)
      {
        GetResourceManager()->MarkSparseMapReferenced((*refit)->resInfo);
      }

      UpdateImageStates(refs.bindImageStates);
      GetResourceManager()->MergeReferencedMemory(refs.bindMemRefs);

      // for storage buffers we have to pessimise memory references because the order matters - if
      // the first recorded reference is a complete write then a later readbeforewrite won't
      // properly mark it as needing initial states preserved. So we do that here. Images are
      // handled separately
      for(auto refit = refs.storableRefs.begin(); refit != refs.storableRefs.end(); ++refit)
      {
        GetResourceManager()->FixupStorageBufferMemory(refs.storableRefs);
      }
    }

    // now we can insert frame references from command buffers, to have a conservative ordering vs.
    // descriptor references since we don't know which happened first
    for(auto it = cmdsWithReferences.begin(); it != cmdsWithReferences.end(); ++it)
    {
      (*it)->AddResourceReferences(GetResourceManager());
      GetResourceManager()->MergeReferencedMemory((*it)->cmdInfo->memFrameRefs);
    }

    for(auto it = cmdsWithReferences.begin(); it != cmdsWithReferences.end(); ++it)
    {
      UpdateImageStates((*it)->cmdInfo->imageStates);
    }

    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);

    if(fence != VK_NULL_HANDLE)
      GetResourceManager()->MarkResourceFrameReferenced(GetResID(fence), eFrameRef_Read);

    rdcarray<VkResourceRecord *> maps;

    // don't flush maps when there are no command buffers
    if(!commandBuffers.empty())
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
      if(state.mapCoherent && state.mappedPtr)
      {
        // only need to flush memory that could affect this submitted batch of work, or if there are
        // BDA buffers bound (as we can't track those!)
        if(!record->hasBDA && refdIDs.find(record->GetResourceID()) == refdIDs.end())
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
          RDCDEBUG("Reading back %s with GPU for comparison", ToStr(record->GetResourceID()).c_str());

          GetDebugManager()->InitReadbackBuffer(state.mapOffset + state.mapSize);

          // immediately issue a command buffer to copy back the data. We do that on this queue to
          // avoid complexity with synchronising with another queue, but the transfer queue if
          // available would be better for this purpose.
          VkCommandBuffer copycmd;

          const uint32_t queueFamilyIndex = GetRecord(queue)->queueFamilyIndex;

          if(m_QueueFamilyIdx == queueFamilyIndex)
            copycmd = GetNextCmd();
          else
            copycmd = GetExtQueueCmd(queueFamilyIndex);

          VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

          ObjDisp(copycmd)->BeginCommandBuffer(Unwrap(copycmd), &beginInfo);

          VkBufferCopy region = {state.mapOffset, state.mapOffset, state.mapSize};

          ObjDisp(copycmd)->CmdCopyBuffer(Unwrap(copycmd), Unwrap(state.wholeMemBuf),
                                          Unwrap(GetDebugManager()->GetReadbackBuffer()), 1, &region);

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

          if(m_QueueFamilyIdx == queueFamilyIndex)
          {
            VkSubmitInfo submit = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, UnwrapPtr(copycmd),
            };
            VkResult copyret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), 1, &submit, VK_NULL_HANDLE);
            RDCASSERTEQUAL(copyret, VK_SUCCESS);

            ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

            RemovePendingCommandBuffer(copycmd);
            AddFreeCommandBuffer(copycmd);
          }
          else
          {
            SubmitAndFlushExtQueue(queueFamilyIndex);
          }

          VkMappedMemoryRange range = {
              VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
              NULL,
              Unwrap(GetDebugManager()->GetReadbackMemory()),
              0,
              VK_WHOLE_SIZE,
          };

          VkResult copyret =
              ObjDisp(queue)->InvalidateMappedMemoryRanges(Unwrap(m_Device), 1, &range);
          RDCASSERTEQUAL(copyret, VK_SUCCESS);

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
                NULL,
                (VkDeviceMemory)(uint64_t)record->Resource,
                state.mapOffset + diffStart,
                diffEnd - diffStart,
            };
            InternalFlushMemoryRange(dev, range, true, capframe);
          }
        }
        else
        {
          RDCDEBUG("Persistent map flush not needed for %s", ToStr(record->GetResourceID()).c_str());
        }

        // restore this just in case
        state.cpuReadPtr = state.mappedPtr;
      }
    }
  }

  for(const rdcpair<ResourceId, VkResourceRecord *> &it : capDescriptors)
    it.second->Delete(GetResourceManager());
  capDescriptors.clear();

  for(VkResourceRecord *asRecord : accelerationStructures)
  {
    asRecord->accelerationStructureBuilt = true;
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueSubmit(SerialiserType &ser, VkQueue queue, uint32_t submitCount,
                                            const VkSubmitInfo *pSubmits, VkFence fence)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT(submitCount);
  SERIALISE_ELEMENT_ARRAY(pSubmits, submitCount).Important();
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

    // add an action use for this submission, to tally up with any debug messages that come from it
    if(IsLoading(m_State))
    {
      ActionUse use(m_CurChunkOffset, m_RootEventID);

      // insert in sorted location
      auto drawit = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);
      m_ActionUses.insert(drawit - m_ActionUses.begin(), use);
    }

    rdcarray<VkCommandBufferSubmitInfo> cmds;

    for(uint32_t sub = 0; sub < submitCount; sub++)
    {
      // make a fake VkSubmitInfo2. If KHR_synchronization2 isn't supported this may then decay
      // back down into separate structs but it keeps a lot of the processing the same in both paths
      // and it's easier to promote this then decay if necessary (knowing no unsupported features
      // will be used)
      VkSubmitInfo2 submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

      const VkProtectedSubmitInfo *prot = (const VkProtectedSubmitInfo *)FindNextStruct(
          &pSubmits[sub], VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO);
      const VkDeviceGroupSubmitInfo *group = (const VkDeviceGroupSubmitInfo *)FindNextStruct(
          &pSubmits[sub], VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO);

      cmds.resize(pSubmits[sub].commandBufferCount);
      for(uint32_t c = 0; c < pSubmits[sub].commandBufferCount; c++)
      {
        VkCommandBufferSubmitInfo &cmd = cmds[c];
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmd.commandBuffer = pSubmits[sub].pCommandBuffers[c];

        if(group && c < group->commandBufferCount)
          cmd.deviceMask = group->pCommandBufferDeviceMasks[c];
      }

      submitInfo.commandBufferInfoCount = (uint32_t)cmds.size();
      submitInfo.pCommandBufferInfos = cmds.data();

      if(prot && prot->protectedSubmit)
        submitInfo.flags |= VK_SUBMIT_PROTECTED_BIT;

      // don't replay any semaphores, this means we don't have to care about
      // VkD3D12FenceSubmitInfoKHR or VkTimelineSemaphoreSubmitInfo.
      // we unwrap VkProtectedSubmitInfo and VkDeviceGroupSubmitInfo above.
      // VkWin32KeyedMutexAcquireReleaseInfoKHR and VkWin32KeyedMutexAcquireReleaseInfoNV we
      // deliberately don't replay
      // VkPerformanceQuerySubmitInfoKHR we don't replay since we don't replay perf counter work

      rdcstr basename = StringFormat::Fmt("vkQueueSubmit(%u)", submitInfo.commandBufferInfoCount);

      ReplayQueueSubmit(queue, submitInfo, basename);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                      const VkSubmitInfo *pSubmits, VkFence fence)
{
  SCOPED_DBG_SINK();

  if(HasFatalError())
    return VK_ERROR_DEVICE_LOST;

  if(!m_MarkedActive)
  {
    m_MarkedActive = true;
    RenderDoc::Inst().AddActiveDriver(RDCDriver::Vulkan, false);
  }

  if(IsActiveCapturing(m_State))
  {
    // 15 is quite a lot of submissions.
    const int expectedMaxSubmissions = 15;

    RenderDoc::Inst().SetProgress(CaptureProgress::FrameCapture,
                                  FakeProgress(m_SubmitCounter, expectedMaxSubmissions));
    m_SubmitCounter++;
  }

  VkResult ret = VK_SUCCESS;
  bool present = false;
  bool beginCapture = false;
  bool endCapture = false;
  rdcarray<VkCommandBuffer> commandBuffers;

  for(uint32_t s = 0; s < submitCount; s++)
  {
    for(uint32_t i = 0; i < pSubmits[s].commandBufferCount; i++)
    {
      VkResourceRecord *record = GetRecord(pSubmits[s].pCommandBuffers[i]);
      present |= record->bakedCommands->cmdInfo->present;
      beginCapture |= record->bakedCommands->cmdInfo->beginCapture;
      endCapture |= record->bakedCommands->cmdInfo->endCapture;

      commandBuffers.push_back(pSubmits[s].pCommandBuffers[i]);
    }
  }

  if(beginCapture)
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    bool capframe = IsActiveCapturing(m_State);

    CaptureQueueSubmit(queue, commandBuffers, fence);

    size_t tempmemSize = sizeof(VkSubmitInfo) * submitCount;

    // because we pass the base struct this will calculate the patch size including it
    for(uint32_t i = 0; i < submitCount; i++)
      tempmemSize += GetNextPatchSize(&pSubmits[i]);

    byte *memory = GetTempMemory(tempmemSize);

    VkSubmitInfo *unwrappedSubmits = (VkSubmitInfo *)memory;
    memory += sizeof(VkSubmitInfo) * submitCount;

    for(uint32_t i = 0; i < submitCount; i++)
      unwrappedSubmits[i] = *UnwrapStructAndChain(m_State, memory, &pSubmits[i]);

    SERIALISE_TIME_CALL(ret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), submitCount,
                                                          unwrappedSubmits, Unwrap(fence)));

    if(capframe)
    {
      {
        CACHE_THREAD_SERIALISER();

        ser.SetActionChunk();
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
    RenderDoc::Inst().EndFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }

  if(present)
  {
    AdvanceFrame();
    Present(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueSubmit2(SerialiserType &ser, VkQueue queue, uint32_t submitCount,
                                             const VkSubmitInfo2 *pSubmits, VkFence fence)
{
  SERIALISE_ELEMENT(queue);
  SERIALISE_ELEMENT(submitCount);
  SERIALISE_ELEMENT_ARRAY(pSubmits, submitCount).Important();
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
      if(pSubmits[i].waitSemaphoreInfoCount > 0)
        doWait = true;

    if(doWait)
      ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));

    // add an action use for this submission, to tally up with any debug messages that come from it
    if(IsLoading(m_State))
    {
      ActionUse use(m_CurChunkOffset, m_RootEventID);

      // insert in sorted location
      auto drawit = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);
      m_ActionUses.insert(drawit - m_ActionUses.begin(), use);
    }

    for(uint32_t sub = 0; sub < submitCount; sub++)
    {
      rdcstr basename = StringFormat::Fmt("vkQueueSubmit2(%u)", pSubmits[sub].commandBufferInfoCount);

      ReplayQueueSubmit(queue, pSubmits[sub], basename);
    }
  }

  return true;
}

VkResult WrappedVulkan::vkQueueSubmit2(VkQueue queue, uint32_t submitCount,
                                       const VkSubmitInfo2 *pSubmits, VkFence fence)
{
  SCOPED_DBG_SINK();

  if(HasFatalError())
    return VK_ERROR_DEVICE_LOST;

  if(!m_MarkedActive)
  {
    m_MarkedActive = true;
    RenderDoc::Inst().AddActiveDriver(RDCDriver::Vulkan, false);
  }

  if(IsActiveCapturing(m_State))
  {
    // 15 is quite a lot of submissions.
    const int expectedMaxSubmissions = 15;

    RenderDoc::Inst().SetProgress(CaptureProgress::FrameCapture,
                                  FakeProgress(m_SubmitCounter, expectedMaxSubmissions));
    m_SubmitCounter++;
  }

  VkResult ret = VK_SUCCESS;
  bool present = false;
  bool beginCapture = false;
  bool endCapture = false;
  rdcarray<VkCommandBuffer> commandBuffers;

  for(uint32_t s = 0; s < submitCount; s++)
  {
    for(uint32_t i = 0; i < pSubmits[s].commandBufferInfoCount; i++)
    {
      VkResourceRecord *record = GetRecord(pSubmits[s].pCommandBufferInfos[i].commandBuffer);
      present |= record->bakedCommands->cmdInfo->present;
      beginCapture |= record->bakedCommands->cmdInfo->beginCapture;
      endCapture |= record->bakedCommands->cmdInfo->endCapture;

      commandBuffers.push_back(pSubmits[s].pCommandBufferInfos[i].commandBuffer);
    }
  }

  if(beginCapture)
  {
    RenderDoc::Inst().StartFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }

  {
    SCOPED_READLOCK(m_CapTransitionLock);

    bool capframe = IsActiveCapturing(m_State);

    CaptureQueueSubmit(queue, commandBuffers, fence);

    size_t tempmemSize = sizeof(VkSubmitInfo2) * submitCount;

    // because we pass the base struct this will calculate the patch size including it
    for(uint32_t i = 0; i < submitCount; i++)
      tempmemSize += GetNextPatchSize(&pSubmits[i]);

    byte *memory = GetTempMemory(tempmemSize);

    VkSubmitInfo2 *unwrappedSubmits = (VkSubmitInfo2 *)memory;
    memory += sizeof(VkSubmitInfo2) * submitCount;

    for(uint32_t i = 0; i < submitCount; i++)
      unwrappedSubmits[i] = *UnwrapStructAndChain(m_State, memory, &pSubmits[i]);

    SERIALISE_TIME_CALL(ret = ObjDisp(queue)->QueueSubmit2(Unwrap(queue), submitCount,
                                                           unwrappedSubmits, Unwrap(fence)));

    if(capframe)
    {
      {
        CACHE_THREAD_SERIALISER();

        ser.SetActionChunk();
        SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueSubmit2);
        Serialise_vkQueueSubmit2(ser, queue, submitCount, pSubmits, fence);

        m_FrameCaptureRecord->AddChunk(scope.Get());
      }

      for(uint32_t s = 0; s < submitCount; s++)
      {
        for(uint32_t sem = 0; sem < pSubmits[s].waitSemaphoreInfoCount; sem++)
          GetResourceManager()->MarkResourceFrameReferenced(
              GetResID(pSubmits[s].pWaitSemaphoreInfos[sem].semaphore), eFrameRef_Read);

        for(uint32_t sem = 0; sem < pSubmits[s].signalSemaphoreInfoCount; sem++)
          GetResourceManager()->MarkResourceFrameReferenced(
              GetResID(pSubmits[s].pSignalSemaphoreInfos[sem].semaphore), eFrameRef_Read);
      }
    }
  }

  if(endCapture)
  {
    RenderDoc::Inst().EndFrameCapture(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
  }

  if(present)
  {
    AdvanceFrame();
    Present(DeviceOwnedWindow(LayerDisp(m_Instance), NULL));
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
  SERIALISE_ELEMENT_ARRAY(pBindInfo, bindInfoCount).Important();
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
          if(IsLoading(m_State))
            m_SparseBindResources.insert(GetResID(buf[i].buffer));

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
          if(IsLoading(m_State))
            m_SparseBindResources.insert(GetResID(imopaque[i].image));

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
          if(IsLoading(m_State))
            m_SparseBindResources.insert(GetResID(im[i].image));

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
  if(HasFatalError())
    return VK_ERROR_DEVICE_LOST;

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
      ser.SetActionChunk();
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
    std::set<ResourceId> memories;
    for(uint32_t i = 0; i < bindInfoCount; i++)
    {
      for(uint32_t buf = 0; buf < pBindInfo[i].bufferBindCount; buf++)
      {
        const VkSparseBufferMemoryBindInfo &bind = pBindInfo[i].pBufferBinds[buf];
        GetRecord(bind.buffer)->resInfo->Update(bind.bindCount, bind.pBinds, memories);
      }

      for(uint32_t op = 0; op < pBindInfo[i].imageOpaqueBindCount; op++)
      {
        const VkSparseImageOpaqueMemoryBindInfo &bind = pBindInfo[i].pImageOpaqueBinds[op];
        GetRecord(bind.image)->resInfo->Update(bind.bindCount, bind.pBinds, memories);
      }

      for(uint32_t op = 0; op < pBindInfo[i].imageBindCount; op++)
      {
        const VkSparseImageMemoryBindInfo &bind = pBindInfo[i].pImageBinds[op];
        GetRecord(bind.image)->resInfo->Update(bind.bindCount, bind.pBinds, memories);
      }
    }

    for(ResourceId id : memories)
      GetResourceManager()->MarkDirtyResource(id);
  }

  return ret;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueWaitIdle(SerialiserType &ser, VkQueue queue)
{
  SERIALISE_ELEMENT(queue).Important();

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
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ObjDisp(queue)->QueueBeginDebugUtilsLabelEXT)
      ObjDisp(queue)->QueueBeginDebugUtilsLabelEXT(Unwrap(queue), &Label);

    if(IsLoading(m_State))
    {
      ActionDescription action;
      action.customName = Label.pLabelName ? Label.pLabelName : "";
      action.flags |= ActionFlags::PushMarker;

      action.markerColor.x = RDCCLAMP(Label.color[0], 0.0f, 1.0f);
      action.markerColor.y = RDCCLAMP(Label.color[1], 0.0f, 1.0f);
      action.markerColor.z = RDCCLAMP(Label.color[2], 0.0f, 1.0f);
      action.markerColor.w = RDCCLAMP(Label.color[3], 0.0f, 1.0f);

      AddEvent();
      AddAction(action);

      // now push the action stack
      GetActionStack().push_back(&GetActionStack().back()->children.back());
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
    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkQueueBeginDebugUtilsLabelEXT);
    Serialise_vkQueueBeginDebugUtilsLabelEXT(ser, queue, pLabelInfo);

    m_FrameCaptureRecord->AddChunk(scope.Get());
    GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkQueueEndDebugUtilsLabelEXT(SerialiserType &ser, VkQueue queue)
{
  SERIALISE_ELEMENT(queue).Unimportant();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ObjDisp(queue)->QueueEndDebugUtilsLabelEXT)
      ObjDisp(queue)->QueueEndDebugUtilsLabelEXT(Unwrap(queue));

    if(IsLoading(m_State))
    {
      ActionDescription action;
      action.flags = ActionFlags::PopMarker;

      AddEvent();
      AddAction(action);

      if(GetActionStack().size() > 1)
        GetActionStack().pop_back();
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
    ser.SetActionChunk();
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
  SERIALISE_ELEMENT_LOCAL(Label, *pLabelInfo).Important();

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    if(ObjDisp(queue)->QueueInsertDebugUtilsLabelEXT)
      ObjDisp(queue)->QueueInsertDebugUtilsLabelEXT(Unwrap(queue), &Label);

    if(IsLoading(m_State))
    {
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
    ser.SetActionChunk();
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
  SERIALISE_ELEMENT_LOCAL(QueueInfo, *pQueueInfo).Important();
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

INSTANTIATE_FUNCTION_SERIALISED(VkResult, vkQueueSubmit2, VkQueue queue, uint32_t submitCount,
                                const VkSubmitInfo2 *pSubmits, VkFence fence);
