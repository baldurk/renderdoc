/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

VkIndirectPatchData WrappedVulkan::FetchIndirectData(VkIndirectPatchType type,
                                                     VkCommandBuffer commandBuffer,
                                                     VkBuffer dataBuffer, VkDeviceSize dataOffset,
                                                     uint32_t count, uint32_t stride,
                                                     VkBuffer counterBuffer,
                                                     VkDeviceSize counterOffset)
{
  if(count == 0)
    return {};

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 0, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  };

  VkDeviceSize dataSize = 0;

  switch(type)
  {
    case VkIndirectPatchType::NoPatch: return {};
    case VkIndirectPatchType::DispatchIndirect: dataSize = sizeof(VkDispatchIndirectCommand); break;
    case VkIndirectPatchType::DrawIndirect:
    case VkIndirectPatchType::DrawIndirectCount:
      dataSize = sizeof(VkDrawIndirectCommand) + (count - 1) * stride;
      break;
    case VkIndirectPatchType::DrawIndexedIndirect:
    case VkIndirectPatchType::DrawIndexedIndirectCount:
      dataSize = sizeof(VkDrawIndexedIndirectCommand) + (count - 1) * stride;
      break;
    case VkIndirectPatchType::DrawIndirectByteCount: dataSize = 4; break;
  }

  bufInfo.size = AlignUp16(dataSize);

  if(counterBuffer != VK_NULL_HANDLE)
    bufInfo.size += 16;

  VkBuffer paramsbuf = VK_NULL_HANDLE;
  vkCreateBuffer(m_Device, &bufInfo, NULL, &paramsbuf);
  MemoryAllocation alloc =
      AllocateMemoryForResource(paramsbuf, MemoryScope::IndirectReadback, MemoryType::Readback);

  VkResult vkr = ObjDisp(m_Device)->BindBufferMemory(Unwrap(m_Device), Unwrap(paramsbuf),
                                                     Unwrap(alloc.mem), alloc.offs);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkBufferMemoryBarrier buf = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_ALL_WRITE_BITS,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(dataBuffer),
      dataOffset,
      dataSize,
  };

  if(type == VkIndirectPatchType::DrawIndirectByteCount)
    buf.srcAccessMask |= VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT;

  VkIndirectRecordData indirectcopy = {};

  indirectcopy.paramsBarrier = buf;

  VkBufferCopy copy = {dataOffset, 0, dataSize};

  indirectcopy.paramsCopy.src = dataBuffer;
  indirectcopy.paramsCopy.dst = paramsbuf;
  indirectcopy.paramsCopy.copy = copy;

  if(counterBuffer != VK_NULL_HANDLE)
  {
    buf.buffer = Unwrap(counterBuffer);
    buf.offset = counterOffset;
    buf.size = 4;

    indirectcopy.countBarrier = buf;

    copy.srcOffset = counterOffset;
    copy.dstOffset = bufInfo.size - 16;
    copy.size = 4;

    indirectcopy.countCopy.src = counterBuffer;
    indirectcopy.countCopy.dst = paramsbuf;
    indirectcopy.countCopy.copy = copy;
  }

  // if it's a dispatch we can do it immediately, otherwise we delay to the end of the renderpass
  if(type == VkIndirectPatchType::DispatchIndirect)
    ExecuteIndirectReadback(commandBuffer, indirectcopy);
  else
    m_BakedCmdBufferInfo[m_LastCmdBufferID].indirectCopies.push_back(indirectcopy);

  VkIndirectPatchData indirectPatch;
  indirectPatch.type = type;
  indirectPatch.alloc = alloc;
  indirectPatch.count = count;
  indirectPatch.stride = stride;
  indirectPatch.buf = paramsbuf;

  // secondary command buffers need to know that their event count should be shifted
  if(m_BakedCmdBufferInfo[m_LastCmdBufferID].level == VK_COMMAND_BUFFER_LEVEL_SECONDARY)
    indirectPatch.commandBuffer = m_LastCmdBufferID;

  return indirectPatch;
}

void WrappedVulkan::ExecuteIndirectReadback(VkCommandBuffer commandBuffer,
                                            const VkIndirectRecordData &indirectcopy)
{
  ObjDisp(commandBuffer)
      ->CmdPipelineBarrier(Unwrap(commandBuffer), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 1,
                           &indirectcopy.paramsBarrier, 0, NULL);

  ObjDisp(commandBuffer)
      ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(indirectcopy.paramsCopy.src),
                      Unwrap(indirectcopy.paramsCopy.dst), 1, &indirectcopy.paramsCopy.copy);

  if(indirectcopy.countCopy.src != VK_NULL_HANDLE)
  {
    ObjDisp(commandBuffer)
        ->CmdPipelineBarrier(Unwrap(commandBuffer), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 1,
                             &indirectcopy.countBarrier, 0, NULL);

    ObjDisp(commandBuffer)
        ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(indirectcopy.countCopy.src),
                        Unwrap(indirectcopy.countCopy.dst), 1, &indirectcopy.countCopy.copy);
  }
}

bool WrappedVulkan::IsDrawInRenderPass()
{
  BakedCmdBufferInfo &cmd = m_BakedCmdBufferInfo[m_LastCmdBufferID];

  if(cmd.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY && cmd.state.renderPass == ResourceId())
  {
    // for primary command buffers, we just check the per-command buffer tracked state
    return false;
  }
  else if(cmd.level == VK_COMMAND_BUFFER_LEVEL_SECONDARY &&
          (cmd.beginFlags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) == 0)
  {
    // secondary command buffers the VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT bit is
    // one-to-one with being a render pass. i.e. you must specify the bit if the execute comes from
    // inside a render pass, and you can't start a render pass in a secondary command buffer so
    // that's the only way to be inside.
    return false;
  }

  // assume a secondary buffer with RENDER_PASS_CONTINUE_BIT is in a render pass without checking
  // where it was actually executed since we won't know that yet.

  return true;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDraw(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                        uint32_t vertexCount, uint32_t instanceCount,
                                        uint32_t firstVertex, uint32_t firstInstance)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(vertexCount).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(firstVertex);
  SERIALISE_ELEMENT(firstInstance);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer);

        ObjDisp(commandBuffer)
            ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex, firstInstance);

        if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex,
                        firstInstance);
          m_ActionCallback->PostRedraw(eventId, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex, firstInstance);

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      {
        AddEvent();

        ActionDescription action;
        action.numIndices = vertexCount;
        action.numInstances = instanceCount;
        action.indexOffset = 0;
        action.vertexOffset = firstVertex;
        action.instanceOffset = firstInstance;

        action.flags |= ActionFlags::Drawcall | ActionFlags::Instanced;

        AddAction(action);
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                              uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdDraw(Unwrap(commandBuffer), vertexCount, instanceCount, firstVertex, firstInstance));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDraw);
    Serialise_vkCmdDraw(ser, commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndexed(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                               uint32_t indexCount, uint32_t instanceCount,
                                               uint32_t firstIndex, int32_t vertexOffset,
                                               uint32_t firstInstance)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(indexCount).Important();
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(firstIndex);
  SERIALISE_ELEMENT(vertexOffset);
  SERIALISE_ELEMENT(firstInstance);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer);

        ObjDisp(commandBuffer)
            ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex,
                             vertexOffset, firstInstance);

        if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex,
                               vertexOffset, firstInstance);
          m_ActionCallback->PostRedraw(eventId, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount, firstIndex,
                           vertexOffset, firstInstance);

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      {
        AddEvent();

        ActionDescription action;
        action.numIndices = indexCount;
        action.numInstances = instanceCount;
        action.indexOffset = firstIndex;
        action.baseVertex = vertexOffset;
        action.instanceOffset = firstInstance;

        action.flags |= ActionFlags::Drawcall | ActionFlags::Indexed | ActionFlags::Instanced;

        AddAction(action);
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount,
                                     uint32_t instanceCount, uint32_t firstIndex,
                                     int32_t vertexOffset, uint32_t firstInstance)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDrawIndexed(Unwrap(commandBuffer), indexCount, instanceCount,
                                           firstIndex, vertexOffset, firstInstance));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexed);
    Serialise_vkCmdDrawIndexed(ser, commandBuffer, indexCount, instanceCount, firstIndex,
                               vertexOffset, firstInstance);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndirect(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkBuffer buffer, VkDeviceSize offset,
                                                uint32_t count, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  bool multidraw = count > 1;

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // do execution (possibly partial)
    if(IsActiveReplaying(m_State))
    {
      if(!multidraw)
      {
        // for single draws, it's pretty simple

        // account for the fake indirect subcommand before checking if we're in re-record range
        if(count > 0)
          m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

        if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t eventId = HandlePreCallback(commandBuffer);

          ObjDisp(commandBuffer)
              ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

          if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
            m_ActionCallback->PostRedraw(eventId, commandBuffer);
          }
        }
      }
      else
      {
        if(InRerecordRange(m_LastCmdBufferID))
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t curEID = m_RootEventID;

          if(m_FirstEventID <= 1)
          {
            curEID = m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID;

            if(m_Partial[Primary].partialParent == m_LastCmdBufferID)
              curEID += m_Partial[Primary].baseEvent;
            else if(m_Partial[Secondary].partialParent == m_LastCmdBufferID)
              curEID += m_Partial[Secondary].baseEvent;
          }

          ActionUse use(m_CurChunkOffset, 0);
          auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

          if(it == m_ActionUses.end())
          {
            RDCERR("Unexpected action not found in uses vector, offset %llu", m_CurChunkOffset);
          }
          else
          {
            uint32_t baseEventID = it->eventId;

            // when we have a callback, submit every action individually to the callback
            if(m_ActionCallback && IsDrawInRenderPass())
            {
              VkMarkerRegion::Begin(
                  StringFormat::Fmt("Drawcall callback replay (drawCount=%u)", count), commandBuffer);

              // first copy off the buffer segment to our indirect action buffer
              VkBufferMemoryBarrier bufBarrier = {
                  VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                  NULL,
                  VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_QUEUE_FAMILY_IGNORED,
                  VK_QUEUE_FAMILY_IGNORED,
                  Unwrap(buffer),
                  offset,
                  (count > 0 ? stride * (count - 1) : 0) + sizeof(VkDrawIndirectCommand),
              };

              DoPipelineBarrier(commandBuffer, 1, &bufBarrier);
              VkBufferCopy region = {offset, 0, bufBarrier.size};
              ObjDisp(commandBuffer)
                  ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(buffer),
                                  Unwrap(m_IndirectBuffer.buf), 1, &region);

              // wait for the copy to finish
              bufBarrier.buffer = Unwrap(m_IndirectBuffer.buf);
              bufBarrier.offset = 0;
              DoPipelineBarrier(commandBuffer, 1, &bufBarrier);

              bufBarrier.size = sizeof(VkDrawIndirectCommand);

              for(uint32_t i = 0; i < count; i++)
              {
                uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Drawcall, i + 1);

                // action up to and including i. The previous draws will be nop'd out
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(m_IndirectBuffer.buf), 0, i + 1,
                                      stride);

                if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(m_IndirectBuffer.buf), 0,
                                        i + 1, stride);
                  m_ActionCallback->PostRedraw(eventId, commandBuffer);
                }

                // now that we're done, nop out this draw so that the next time around we only draw
                // the next draw.
                bufBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
                bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                DoPipelineBarrier(commandBuffer, 1, &bufBarrier);
                ObjDisp(commandBuffer)
                    ->CmdFillBuffer(Unwrap(commandBuffer), bufBarrier.buffer, bufBarrier.offset,
                                    bufBarrier.size, 0);
                bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
                DoPipelineBarrier(commandBuffer, 1, &bufBarrier);

                bufBarrier.offset += stride;
              }

              VkMarkerRegion::End(commandBuffer);
            }
            // To add the multidraw, we made an event N that is the 'parent' marker, then
            // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
            // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
            // the first sub-draw in that range.
            else if(m_LastEventID > baseEventID)
            {
              uint32_t drawidx = 0;

              if(m_FirstEventID <= 1)
              {
                // if we're replaying part-way into a multidraw, we can replay the first part
                // 'easily'
                // by just reducing the Count parameter to however many we want to replay. This only
                // works if we're replaying from the first multidraw to the nth (n less than Count)
                count = RDCMIN(count, m_LastEventID - baseEventID);
              }
              else
              {
                // otherwise we do the 'hard' case, draw only one multidraw
                // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
                // a single draw.
                //
                // We also need to draw the same number of draws so that DrawIndex is faithful. In
                // order to preserve the draw index we write a custom indirect buffer that has zeros
                // for the parameters of all previous draws.
                drawidx = (curEID - baseEventID - 1);

                offset += stride * drawidx;

                // ensure the custom buffer is large enough
                VkDeviceSize bufLength = sizeof(VkDrawIndirectCommand) * (drawidx + 1);

                RDCASSERT(bufLength <= m_IndirectBufferSize, bufLength, m_IndirectBufferSize);

                VkBufferMemoryBarrier bufBarrier = {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    NULL,
                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    Unwrap(m_IndirectBuffer.buf),
                    0,
                    m_IndirectBufferSize,
                };

                VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                      NULL,
                                                      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

                ObjDisp(m_IndirectCommandBuffer)
                    ->BeginCommandBuffer(Unwrap(m_IndirectCommandBuffer), &beginInfo);

                // wait for any previous indirect draws to complete before filling/transferring
                DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

                // initialise to 0 so all other draws don't draw anything
                ObjDisp(m_IndirectCommandBuffer)
                    ->CmdFillBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(m_IndirectBuffer.buf),
                                    0, m_IndirectBufferSize, 0);

                // wait for fill to complete before copy
                bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

                // copy over the actual parameter set into the right place
                VkBufferCopy region = {offset, bufLength - sizeof(VkDrawIndirectCommand),
                                       sizeof(VkDrawIndirectCommand)};
                ObjDisp(m_IndirectCommandBuffer)
                    ->CmdCopyBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(buffer),
                                    Unwrap(m_IndirectBuffer.buf), 1, &region);

                // finally wait for copy to complete before drawing from it
                bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

                DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

                ObjDisp(m_IndirectCommandBuffer)->EndCommandBuffer(Unwrap(m_IndirectCommandBuffer));

                // draw from our custom buffer
                m_IndirectDraw = true;
                buffer = m_IndirectBuffer.buf;
                offset = 0;
                count = drawidx + 1;
                stride = sizeof(VkDrawIndirectCommand);
              }

              if(IsDrawInRenderPass())
              {
                uint32_t eventId =
                    HandlePreCallback(commandBuffer, ActionFlags::Drawcall, drawidx + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

                if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
                  m_ActionCallback->PostRedraw(eventId, commandBuffer);
                }
              }
            }
          }
        }

        // multidraws skip the event ID past the whole thing
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID += count + 1;
      }
    }
    else
    {
      VkIndirectPatchData indirectPatch = FetchIndirectData(
          VkIndirectPatchType::DrawIndirect, commandBuffer, buffer, offset, count, stride);

      ObjDisp(commandBuffer)
          ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

      // add on the size we'll need for an indirect buffer in the worst case.
      // Note that we'll only ever be partially replaying one draw at a time, so we only need the
      // worst case.
      m_IndirectBufferSize =
          RDCMAX(m_IndirectBufferSize, sizeof(VkDrawIndirectCommand) + count * stride);

      rdcstr name = "vkCmdDrawIndirect";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      // for 'single' draws, don't do complex multi-draw just inline it
      if(count == 1)
      {
        ActionDescription action;

        AddEvent();

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command"_lit);
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(buffer));
          structuriser.Serialise("buffer"_lit, bufid);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("stride"_lit, stride);
          structuriser.Serialise("command"_lit, VkDrawIndirectCommand()).Important();
        }

        m_StructuredFile->chunks.insert(m_StructuredFile->chunks.size() - 1, fakeChunk);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

        AddEvent();

        action.customName = name;
        action.flags = ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indirect;

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.indirectPatch = indirectPatch;

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

        return true;
      }

      ActionDescription action;
      action.customName = name;
      action.flags = ActionFlags::MultiAction | ActionFlags::PushMarker;

      if(count == 0)
      {
        action.flags = ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indirect;
        action.customName += "(0)";
      }

      AddEvent();
      AddAction(action);

      VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

      actionNode.indirectPatch = indirectPatch;

      actionNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

      if(count > 0)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      for(uint32_t i = 0; i < count; i++)
      {
        ActionDescription multi;

        multi.customName = name;

        multi.flags |= ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indirect;

        // add a fake chunk for this individual indirect action
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command"_lit);
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(buffer));
          structuriser.Serialise("buffer"_lit, bufid);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("stride"_lit, stride);
          structuriser.Serialise("command"_lit, VkDrawIndirectCommand()).Important();
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddAction(multi);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      if(count > 0)
      {
        AddEvent();
        action.customName = name + " end";
        action.flags = ActionFlags::PopMarker;
        AddAction(action);
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                      VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirect);
    Serialise_vkCmdDrawIndirect(ser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    VkDeviceSize size = 0;
    if(count > 0)
    {
      size = (count - 1) * stride + sizeof(VkDrawIndirectCommand);
    }
    record->MarkBufferFrameReferenced(GetRecord(buffer), offset, size, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirect(SerialiserType &ser,
                                                       VkCommandBuffer commandBuffer,
                                                       VkBuffer buffer, VkDeviceSize offset,
                                                       uint32_t count, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(count).Important();
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  bool multidraw = count > 1;

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // do execution (possibly partial)
    if(IsActiveReplaying(m_State))
    {
      if(!multidraw)
      {
        // for single draws, it's pretty simple

        // account for the fake indirect subcommand before checking if we're in re-record range
        if(count > 0)
          m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

        if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t eventId = HandlePreCallback(commandBuffer);

          ObjDisp(commandBuffer)
              ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

          if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
          {
            ObjDisp(commandBuffer)
                ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                         stride);
            m_ActionCallback->PostRedraw(eventId, commandBuffer);
          }
        }
      }
      else
      {
        if(InRerecordRange(m_LastCmdBufferID))
        {
          commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

          uint32_t curEID = m_RootEventID;

          if(m_FirstEventID <= 1)
          {
            curEID = m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID;

            if(m_Partial[Primary].partialParent == m_LastCmdBufferID)
              curEID += m_Partial[Primary].baseEvent;
            else if(m_Partial[Secondary].partialParent == m_LastCmdBufferID)
              curEID += m_Partial[Secondary].baseEvent;
          }

          ActionUse use(m_CurChunkOffset, 0);
          auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

          if(it == m_ActionUses.end())
          {
            RDCERR("Unexpected action not found in uses vector, offset %llu", m_CurChunkOffset);
          }
          else
          {
            uint32_t baseEventID = it->eventId;

            // when we have a callback, submit every action individually to the callback
            if(m_ActionCallback && IsDrawInRenderPass())
            {
              for(uint32_t i = 0; i < count; i++)
              {
                uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Drawcall, i + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1,
                                             stride);

                if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1,
                                               stride);
                  m_ActionCallback->PostRedraw(eventId, commandBuffer);
                }

                offset += stride;
              }
            }
            // To add the multidraw, we made an event N that is the 'parent' marker, then
            // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
            // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
            // the first sub-draw in that range.
            else if(m_LastEventID > baseEventID)
            {
              uint32_t drawidx = 0;

              if(m_FirstEventID <= 1)
              {
                // if we're replaying part-way into a multidraw, we can replay the first part
                // 'easily'
                // by just reducing the Count parameter to however many we want to replay. This only
                // works if we're replaying from the first multidraw to the nth (n less than Count)
                count = RDCMIN(count, m_LastEventID - baseEventID);
              }
              else
              {
                // otherwise we do the 'hard' case, draw only one multidraw
                // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
                // a single draw.
                //
                // We also need to draw the same number of draws so that DrawIndex is faithful. In
                // order to preserve the draw index we write a custom indirect buffer that has zeros
                // for the parameters of all previous draws.
                drawidx = (curEID - baseEventID - 1);

                offset += stride * drawidx;

                // ensure the custom buffer is large enough
                VkDeviceSize bufLength = sizeof(VkDrawIndexedIndirectCommand) * (drawidx + 1);

                RDCASSERT(bufLength <= m_IndirectBufferSize, bufLength, m_IndirectBufferSize);

                VkBufferMemoryBarrier bufBarrier = {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    NULL,
                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    Unwrap(m_IndirectBuffer.buf),
                    0,
                    m_IndirectBufferSize,
                };

                VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                      NULL,
                                                      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

                ObjDisp(m_IndirectCommandBuffer)
                    ->BeginCommandBuffer(Unwrap(m_IndirectCommandBuffer), &beginInfo);

                // wait for any previous indirect draws to complete before filling/transferring
                DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

                // initialise to 0 so all other draws don't draw anything
                ObjDisp(m_IndirectCommandBuffer)
                    ->CmdFillBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(m_IndirectBuffer.buf),
                                    0, m_IndirectBufferSize, 0);

                // wait for fill to complete before copy
                bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

                // copy over the actual parameter set into the right place
                VkBufferCopy region = {offset, bufLength - sizeof(VkDrawIndexedIndirectCommand),
                                       sizeof(VkDrawIndexedIndirectCommand)};
                ObjDisp(m_IndirectCommandBuffer)
                    ->CmdCopyBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(buffer),
                                    Unwrap(m_IndirectBuffer.buf), 1, &region);

                // finally wait for copy to complete before drawing from it
                bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

                DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

                ObjDisp(m_IndirectCommandBuffer)->EndCommandBuffer(Unwrap(m_IndirectCommandBuffer));

                // draw from our custom buffer
                m_IndirectDraw = true;
                buffer = m_IndirectBuffer.buf;
                offset = 0;
                count = drawidx + 1;
                stride = sizeof(VkDrawIndexedIndirectCommand);
              }

              if(IsDrawInRenderPass())
              {
                uint32_t eventId =
                    HandlePreCallback(commandBuffer, ActionFlags::Drawcall, drawidx + 1);

                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                             stride);

                if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
                {
                  ObjDisp(commandBuffer)
                      ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                               stride);
                  m_ActionCallback->PostRedraw(eventId, commandBuffer);
                }
              }
            }
          }
        }

        // multidraws skip the event ID past the whole thing
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID += count + 1;
      }
    }
    else
    {
      VkIndirectPatchData indirectPatch = FetchIndirectData(
          VkIndirectPatchType::DrawIndexedIndirect, commandBuffer, buffer, offset, count, stride);

      ObjDisp(commandBuffer)
          ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

      // add on the size we'll need for an indirect buffer in the worst case.
      // Note that we'll only ever be partially replaying one draw at a time, so we only need the
      // worst case.
      m_IndirectBufferSize =
          RDCMAX(m_IndirectBufferSize, sizeof(VkDrawIndexedIndirectCommand) + count * stride);

      rdcstr name = "vkCmdDrawIndexedIndirect";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      // for 'single' draws, don't do complex multi-draw just inline it
      if(count == 1)
      {
        ActionDescription action;

        AddEvent();

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command"_lit);
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(buffer));
          structuriser.Serialise("buffer"_lit, bufid);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("stride"_lit, stride);
          structuriser.Serialise("command"_lit, VkDrawIndexedIndirectCommand()).Important();
        }

        m_StructuredFile->chunks.insert(m_StructuredFile->chunks.size() - 1, fakeChunk);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

        AddEvent();

        action.customName = name;
        action.flags = ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indexed |
                       ActionFlags::Indirect;

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.indirectPatch = indirectPatch;

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

        return true;
      }

      ActionDescription action;
      action.customName = name;
      action.flags = ActionFlags::MultiAction | ActionFlags::PushMarker;

      if(count == 0)
      {
        action.customName += "(0)";
        action.flags = ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indexed |
                       ActionFlags::Indirect;
      }

      AddEvent();
      AddAction(action);

      VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

      actionNode.indirectPatch = indirectPatch;

      actionNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

      if(count > 0)
        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      for(uint32_t i = 0; i < count; i++)
      {
        ActionDescription multi;

        multi.customName = name;

        multi.flags |= ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indexed |
                       ActionFlags::Indirect;

        // add a fake chunk for this individual indirect draw
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command"_lit);
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(buffer));
          structuriser.Serialise("buffer"_lit, bufid);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("stride"_lit, stride);
          structuriser.Serialise("command"_lit, VkDrawIndexedIndirectCommand()).Important();
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddAction(multi);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      if(count > 0)
      {
        AddEvent();
        action.customName = name + " end";
        action.flags = ActionFlags::PopMarker;
        AddAction(action);
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                             VkDeviceSize offset, uint32_t count, uint32_t stride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)
          ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexedIndirect);
    Serialise_vkCmdDrawIndexedIndirect(ser, commandBuffer, buffer, offset, count, stride);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    VkDeviceSize size = 0;
    if(count > 0)
    {
      size = (count - 1) * stride + sizeof(VkDrawIndexedIndirectCommand);
    }
    record->MarkBufferFrameReferenced(GetRecord(buffer), offset, size, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDispatch(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                            uint32_t x, uint32_t y, uint32_t z)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(x).Important();
  SERIALISE_ELEMENT(y).Important();
  SERIALISE_ELEMENT(z).Important();

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Dispatch);

        ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

        if(eventId && m_ActionCallback->PostDispatch(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);
          m_ActionCallback->PostRedispatch(eventId, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z);

      {
        AddEvent();

        ActionDescription action;
        action.dispatchDimension[0] = x;
        action.dispatchDimension[1] = y;
        action.dispatchDimension[2] = z;

        action.flags |= ActionFlags::Dispatch;

        AddAction(action);
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdDispatch(Unwrap(commandBuffer), x, y, z));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatch);
    Serialise_vkCmdDispatch(ser, commandBuffer, x, y, z);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDispatchIndirect(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                    VkDeviceSize offset)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Dispatch);

        ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

        if(eventId && m_ActionCallback->PostDispatch(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);
          m_ActionCallback->PostRedispatch(eventId, commandBuffer);
        }
      }
    }
    else
    {
      VkIndirectPatchData indirectPatch =
          FetchIndirectData(VkIndirectPatchType::DispatchIndirect, commandBuffer, buffer, offset, 1);

      ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset);

      {
        AddEvent();

        ActionDescription action;
        action.customName = "vkCmdDispatchIndirect(<?, ?, ?>)";
        action.dispatchDimension[0] = 0;
        action.dispatchDimension[1] = 0;
        action.dispatchDimension[2] = 0;

        action.flags |= ActionFlags::Dispatch | ActionFlags::Indirect;

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.indirectPatch = indirectPatch;

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                          VkDeviceSize offset)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdDispatchIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatchIndirect);
    Serialise_vkCmdDispatchIndirect(ser, commandBuffer, buffer, offset);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(buffer), offset, sizeof(VkDispatchIndirectCommand),
                                      eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBlitImage(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                             VkImage srcImage, VkImageLayout srcImageLayout,
                                             VkImage destImage, VkImageLayout destImageLayout,
                                             uint32_t regionCount, const VkImageBlit *pRegions,
                                             VkFilter filter)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(srcImage).Important();
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destImage).Important();
  SERIALISE_ELEMENT(destImageLayout);
  SERIALISE_ELEMENT(regionCount);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);
  SERIALISE_ELEMENT(filter);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Resolve);

        ObjDisp(commandBuffer)
            ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                           Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Resolve, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage),
                         destImageLayout, regionCount, pRegions, filter);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(srcImage));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(destImage));

        ActionDescription action;
        action.flags |= ActionFlags::Resolve;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();
        if(regionCount > 0)
        {
          action.copySourceSubresource = Subresource(pRegions[0].srcSubresource.mipLevel,
                                                     pRegions[0].srcSubresource.baseArrayLayer);
          action.copyDestinationSubresource = Subresource(
              pRegions[0].dstSubresource.mipLevel, pRegions[0].dstSubresource.baseArrayLayer);
        }
        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcImage == destImage)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::ResolveSrc)));
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destImage), EventUsage(actionNode.action.eventId, ResourceUsage::ResolveDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                   VkImageLayout srcImageLayout, VkImage destImage,
                                   VkImageLayout destImageLayout, uint32_t regionCount,
                                   const VkImageBlit *pRegions, VkFilter filter)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdBlitImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                         Unwrap(destImage), destImageLayout, regionCount, pRegions,
                                         filter));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBlitImage);
    Serialise_vkCmdBlitImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                             destImageLayout, regionCount, pRegions, filter);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < regionCount; i++)
    {
      const VkImageBlit &region = pRegions[i];

      ImageRange srcRange(region.srcSubresource);

      srcRange.offset = {RDCMIN(region.srcOffsets[0].x, region.srcOffsets[1].x),
                         RDCMIN(region.srcOffsets[0].y, region.srcOffsets[1].y),
                         RDCMIN(region.srcOffsets[0].z, region.srcOffsets[1].z)};
      srcRange.extent = {
          (uint32_t)(RDCMAX(region.srcOffsets[0].x, region.srcOffsets[1].x) - srcRange.offset.x),
          (uint32_t)(RDCMAX(region.srcOffsets[0].y, region.srcOffsets[1].y) - srcRange.offset.y),
          (uint32_t)(RDCMAX(region.srcOffsets[0].z, region.srcOffsets[1].z) - srcRange.offset.z)};

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = {RDCMIN(region.dstOffsets[0].x, region.dstOffsets[1].x),
                         RDCMIN(region.dstOffsets[0].y, region.dstOffsets[1].y),
                         RDCMIN(region.dstOffsets[0].z, region.dstOffsets[1].z)};
      dstRange.extent = {
          (uint32_t)(RDCMAX(region.dstOffsets[0].x, region.dstOffsets[1].x) - dstRange.offset.x),
          (uint32_t)(RDCMAX(region.dstOffsets[0].y, region.dstOffsets[1].y) - dstRange.offset.y),
          (uint32_t)(RDCMAX(region.dstOffsets[0].z, region.dstOffsets[1].z) - dstRange.offset.z)};

      record->MarkImageFrameReferenced(GetRecord(srcImage), srcRange, eFrameRef_Read);
      record->MarkImageFrameReferenced(GetRecord(destImage), dstRange, eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdResolveImage(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                VkImage srcImage, VkImageLayout srcImageLayout,
                                                VkImage destImage, VkImageLayout destImageLayout,
                                                uint32_t regionCount, const VkImageResolve *pRegions)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(srcImage).Important();
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destImage).Important();
  SERIALISE_ELEMENT(destImageLayout);
  SERIALISE_ELEMENT(regionCount);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Resolve);

        ObjDisp(commandBuffer)
            ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                              Unwrap(destImage), destImageLayout, regionCount, pRegions);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                Unwrap(destImage), destImageLayout, regionCount, pRegions);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Resolve, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                            Unwrap(destImage), destImageLayout, regionCount, pRegions);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(srcImage));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(destImage));

        ActionDescription action;
        action.flags |= ActionFlags::Resolve;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();
        if(regionCount > 0)
        {
          action.copySourceSubresource = Subresource(pRegions[0].srcSubresource.mipLevel,
                                                     pRegions[0].srcSubresource.baseArrayLayer);
          action.copyDestinationSubresource = Subresource(
              pRegions[0].dstSubresource.mipLevel, pRegions[0].dstSubresource.baseArrayLayer);
        }
        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcImage == destImage)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::ResolveSrc)));
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destImage), EventUsage(actionNode.action.eventId, ResourceUsage::ResolveDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                      VkImageLayout srcImageLayout, VkImage destImage,
                                      VkImageLayout destImageLayout, uint32_t regionCount,
                                      const VkImageResolve *pRegions)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdResolveImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                            Unwrap(destImage), destImageLayout, regionCount,
                                            pRegions));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdResolveImage);
    Serialise_vkCmdResolveImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                                destImageLayout, regionCount, pRegions);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < regionCount; i++)
    {
      const VkImageResolve &region = pRegions[i];

      ImageRange srcRange(region.srcSubresource);
      srcRange.offset = region.srcOffset;
      srcRange.extent = region.extent;

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = region.dstOffset;
      dstRange.extent = region.extent;

      record->MarkImageFrameReferenced(GetRecord(srcImage), srcRange, eFrameRef_Read);
      record->MarkImageFrameReferenced(GetRecord(destImage), dstRange, eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyImage(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                             VkImage srcImage, VkImageLayout srcImageLayout,
                                             VkImage destImage, VkImageLayout destImageLayout,
                                             uint32_t regionCount, const VkImageCopy *pRegions)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(srcImage).Important();
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destImage).Important();
  SERIALISE_ELEMENT(destImageLayout);
  SERIALISE_ELEMENT(regionCount);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                           Unwrap(destImage), destImageLayout, regionCount, pRegions);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                             Unwrap(destImage), destImageLayout, regionCount, pRegions);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage),
                         destImageLayout, regionCount, pRegions);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(srcImage));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(destImage));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();
        if(regionCount > 0)
        {
          action.copySourceSubresource = Subresource(pRegions[0].srcSubresource.mipLevel,
                                                     pRegions[0].srcSubresource.baseArrayLayer);
          action.copyDestinationSubresource = Subresource(
              pRegions[0].dstSubresource.mipLevel, pRegions[0].dstSubresource.baseArrayLayer);
        }

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcImage == destImage)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::Copy)));
        }
        else
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destImage), EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                   VkImageLayout srcImageLayout, VkImage destImage,
                                   VkImageLayout destImageLayout, uint32_t regionCount,
                                   const VkImageCopy *pRegions)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdCopyImage(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                         Unwrap(destImage), destImageLayout, regionCount, pRegions));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImage);
    Serialise_vkCmdCopyImage(ser, commandBuffer, srcImage, srcImageLayout, destImage,
                             destImageLayout, regionCount, pRegions);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < regionCount; i++)
    {
      const VkImageCopy &region = pRegions[i];

      ImageRange srcRange(region.srcSubresource);
      srcRange.offset = region.srcOffset;
      srcRange.extent = region.extent;

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = region.dstOffset;
      dstRange.extent = region.extent;

      record->MarkImageFrameReferenced(GetRecord(srcImage), srcRange, eFrameRef_Read);
      record->MarkImageFrameReferenced(GetRecord(destImage), dstRange, eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyBufferToImage(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage destImage,
    VkImageLayout destImageLayout, uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(srcBuffer).Important();
  SERIALISE_ELEMENT(destImage).Important();
  SERIALISE_ELEMENT(destImageLayout);
  SERIALISE_ELEMENT(regionCount);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                   destImageLayout, regionCount, pRegions);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                     destImageLayout, regionCount, pRegions);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destImage),
                                 destImageLayout, regionCount, pRegions);

      {
        AddEvent();

        ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(srcBuffer));
        ResourceId imgid = GetResourceManager()->GetOriginalID(GetResID(destImage));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = bufid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = imgid;
        action.copyDestinationSubresource = Subresource();
        if(regionCount > 0)
          action.copyDestinationSubresource = Subresource(
              pRegions[0].imageSubresource.mipLevel, pRegions[0].imageSubresource.baseArrayLayer);

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(srcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(destImage), EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                                           VkImage destImage, VkImageLayout destImageLayout,
                                           uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdCopyBufferToImage(Unwrap(commandBuffer), Unwrap(srcBuffer),
                                                 Unwrap(destImage), destImageLayout, regionCount,
                                                 pRegions));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBufferToImage);
    Serialise_vkCmdCopyBufferToImage(ser, commandBuffer, srcBuffer, destImage, destImageLayout,
                                     regionCount, pRegions);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkBufferImageCopyFrameReferenced(GetRecord(srcBuffer), GetRecord(destImage),
                                               regionCount, pRegions, eFrameRef_Read,
                                               eFrameRef_CompleteWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyImageToBuffer(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer,
                                                     VkImage srcImage, VkImageLayout srcImageLayout,
                                                     VkBuffer destBuffer, uint32_t regionCount,
                                                     const VkBufferImageCopy *pRegions)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(srcImage).Important();
  SERIALISE_ELEMENT(srcImageLayout);
  SERIALISE_ELEMENT(destBuffer).Important();
  SERIALISE_ELEMENT(regionCount);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                   Unwrap(destBuffer), regionCount, pRegions);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                     Unwrap(destBuffer), regionCount, pRegions);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage), srcImageLayout,
                                 Unwrap(destBuffer), regionCount, pRegions);

      {
        AddEvent();

        ResourceId imgid = GetResourceManager()->GetOriginalID(GetResID(srcImage));
        ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(destBuffer));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = imgid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = bufid;
        action.copyDestinationSubresource = Subresource();
        if(regionCount > 0)
          action.copySourceSubresource = Subresource(pRegions[0].imageSubresource.mipLevel,
                                                     pRegions[0].imageSubresource.baseArrayLayer);

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(srcImage), EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(destBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage,
                                           VkImageLayout srcImageLayout, VkBuffer destBuffer,
                                           uint32_t regionCount, const VkBufferImageCopy *pRegions)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdCopyImageToBuffer(Unwrap(commandBuffer), Unwrap(srcImage),
                                                 srcImageLayout, Unwrap(destBuffer), regionCount,
                                                 pRegions));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImageToBuffer);
    Serialise_vkCmdCopyImageToBuffer(ser, commandBuffer, srcImage, srcImageLayout, destBuffer,
                                     regionCount, pRegions);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkBufferImageCopyFrameReferenced(GetRecord(destBuffer), GetRecord(srcImage),
                                               regionCount, pRegions, eFrameRef_CompleteWrite,
                                               eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyBuffer(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              VkBuffer srcBuffer, VkBuffer destBuffer,
                                              uint32_t regionCount, const VkBufferCopy *pRegions)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(srcBuffer).Important();
  SERIALISE_ELEMENT(destBuffer).Important();
  SERIALISE_ELEMENT(regionCount);
  SERIALISE_ELEMENT_ARRAY(pRegions, regionCount);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)
            ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer),
                            regionCount, pRegions);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer),
                              regionCount, pRegions);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), regionCount,
                          pRegions);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(srcBuffer));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(destBuffer));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcBuffer == destBuffer)
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::Copy)));
        }
        else
        {
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(srcBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
          actionNode.resourceUsage.push_back(make_rdcpair(
              GetResID(destBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                                    VkBuffer destBuffer, uint32_t regionCount,
                                    const VkBufferCopy *pRegions)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(srcBuffer),
                                          Unwrap(destBuffer), regionCount, pRegions));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBuffer);
    Serialise_vkCmdCopyBuffer(ser, commandBuffer, srcBuffer, destBuffer, regionCount, pRegions);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    for(uint32_t i = 0; i < regionCount; i++)
    {
      record->MarkBufferFrameReferenced(GetRecord(srcBuffer), pRegions[i].srcOffset,
                                        pRegions[i].size, eFrameRef_Read);
      record->MarkBufferFrameReferenced(GetRecord(destBuffer), pRegions[i].dstOffset,
                                        pRegions[i].size, eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdFillBuffer(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                              VkBuffer destBuffer, VkDeviceSize destOffset,
                                              VkDeviceSize fillSize, uint32_t data)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(destBuffer).Important();
  SERIALISE_ELEMENT(destOffset);
  SERIALISE_ELEMENT(fillSize);
  SERIALISE_ELEMENT(data).Important();

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Clear);

        ObjDisp(commandBuffer)
            ->CmdFillBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset, fillSize, data);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Clear, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdFillBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset, fillSize, data);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Clear, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdFillBuffer(Unwrap(commandBuffer), Unwrap(destBuffer), destOffset, fillSize, data);

      {
        AddEvent();

        ResourceId id = GetResourceManager()->GetOriginalID(GetResID(destBuffer));

        ActionDescription action;
        action.flags = ActionFlags::Clear;
        action.copyDestination = id;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(destBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::Clear)));
      }
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

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(destBuffer), destOffset, fillSize,
                                      eFrameRef_CompleteWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdClearColorImage(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                   VkImage image, VkImageLayout imageLayout,
                                                   const VkClearColorValue *pColor,
                                                   uint32_t rangeCount,
                                                   const VkImageSubresourceRange *pRanges)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(image).Important();
  SERIALISE_ELEMENT(imageLayout);
  SERIALISE_ELEMENT_LOCAL(Color, *pColor).Important();
  SERIALISE_ELEMENT(rangeCount);
  SERIALISE_ELEMENT_ARRAY(pRanges, rangeCount);

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

        uint32_t eventId = HandlePreCallback(
            commandBuffer, ActionFlags(ActionFlags::Clear | ActionFlags::ClearColor));

        ObjDisp(commandBuffer)
            ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                                 rangeCount, pRanges);

        if(eventId &&
           m_ActionCallback->PostMisc(
               eventId, ActionFlags(ActionFlags::Clear | ActionFlags::ClearColor), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                                   rangeCount, pRanges);

          m_ActionCallback->PostRemisc(
              eventId, ActionFlags(ActionFlags::Clear | ActionFlags::ClearColor), commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout, &Color,
                               rangeCount, pRanges);

      {
        AddEvent();

        ActionDescription action;
        action.flags |= ActionFlags::Clear | ActionFlags::ClearColor;
        action.copyDestination = GetResourceManager()->GetOriginalID(GetResID(image));
        action.copyDestinationSubresource = Subresource();
        if(rangeCount > 0)
          action.copyDestinationSubresource =
              Subresource(pRanges[0].baseMipLevel, pRanges[0].baseArrayLayer);

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(image), EventUsage(actionNode.action.eventId, ResourceUsage::Clear)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image,
                                         VkImageLayout imageLayout, const VkClearColorValue *pColor,
                                         uint32_t rangeCount, const VkImageSubresourceRange *pRanges)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdClearColorImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                               pColor, rangeCount, pRanges));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearColorImage);
    Serialise_vkCmdClearColorImage(ser, commandBuffer, image, imageLayout, pColor, rangeCount,
                                   pRanges);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    VkResourceRecord *imageRecord = GetRecord(image);
    if(imageRecord->resInfo && imageRecord->resInfo->IsSparse())
      record->cmdInfo->sparse.insert(imageRecord->resInfo);

    for(uint32_t i = 0; i < rangeCount; i++)
    {
      record->MarkImageFrameReferenced(imageRecord, pRanges[i], eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdClearDepthStencilImage(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout,
    const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount,
    const VkImageSubresourceRange *pRanges)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(image).Important();
  SERIALISE_ELEMENT(imageLayout);
  SERIALISE_ELEMENT_LOCAL(DepthStencil, *pDepthStencil).Important();
  SERIALISE_ELEMENT(rangeCount);
  SERIALISE_ELEMENT_ARRAY(pRanges, rangeCount);

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

        uint32_t eventId = HandlePreCallback(
            commandBuffer, ActionFlags(ActionFlags::Clear | ActionFlags::ClearDepthStencil));

        ObjDisp(commandBuffer)
            ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                        &DepthStencil, rangeCount, pRanges);

        if(eventId &&
           m_ActionCallback->PostMisc(
               eventId, ActionFlags(ActionFlags::Clear | ActionFlags::ClearDepthStencil),
               commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                          &DepthStencil, rangeCount, pRanges);

          m_ActionCallback->PostRemisc(
              eventId, ActionFlags(ActionFlags::Clear | ActionFlags::ClearDepthStencil),
              commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image), imageLayout,
                                      &DepthStencil, rangeCount, pRanges);

      {
        AddEvent();

        ActionDescription action;
        action.flags |= ActionFlags::Clear | ActionFlags::ClearDepthStencil;
        action.copyDestination = GetResourceManager()->GetOriginalID(GetResID(image));
        action.copyDestinationSubresource = Subresource();
        if(rangeCount > 0)
          action.copyDestinationSubresource =
              Subresource(pRanges[0].baseMipLevel, pRanges[0].baseArrayLayer);

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(make_rdcpair(
            GetResID(image), EventUsage(actionNode.action.eventId, ResourceUsage::Clear)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image,
                                                VkImageLayout imageLayout,
                                                const VkClearDepthStencilValue *pDepthStencil,
                                                uint32_t rangeCount,
                                                const VkImageSubresourceRange *pRanges)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdClearDepthStencilImage(Unwrap(commandBuffer), Unwrap(image),
                                                      imageLayout, pDepthStencil, rangeCount,
                                                      pRanges));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearDepthStencilImage);
    Serialise_vkCmdClearDepthStencilImage(ser, commandBuffer, image, imageLayout, pDepthStencil,
                                          rangeCount, pRanges);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
    record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_PartialWrite);
    record->MarkResourceFrameReferenced(GetRecord(image)->baseResource, eFrameRef_Read);
    VkResourceRecord *imageRecord = GetRecord(image);
    if(imageRecord->resInfo && imageRecord->resInfo->IsSparse())
      record->cmdInfo->sparse.insert(imageRecord->resInfo);

    for(uint32_t i = 0; i < rangeCount; i++)
    {
      record->MarkImageFrameReferenced(imageRecord, pRanges[i], eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdClearAttachments(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer,
                                                    uint32_t attachmentCount,
                                                    const VkClearAttachment *pAttachments,
                                                    uint32_t rectCount, const VkClearRect *pRects)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(attachmentCount);
  SERIALISE_ELEMENT_ARRAY(pAttachments, attachmentCount).Important();
  SERIALISE_ELEMENT(rectCount);
  SERIALISE_ELEMENT_ARRAY(pRects, rectCount);

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags(ActionFlags::Clear));

        ObjDisp(commandBuffer)
            ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                  pRects);

        if(eventId &&
           m_ActionCallback->PostMisc(eventId, ActionFlags(ActionFlags::Clear), commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                    pRects);

          m_ActionCallback->PostRemisc(eventId, ActionFlags(ActionFlags::Clear), commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount, pAttachments, rectCount,
                                pRects);

      {
        AddEvent();

        ActionDescription action;
        action.flags |= ActionFlags::Clear;
        for(uint32_t a = 0; a < attachmentCount; a++)
        {
          if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
            action.flags |= ActionFlags::ClearColor;
          if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            action.flags |= ActionFlags::ClearDepthStencil;
        }

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();
        const VulkanRenderState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;

        if(state.renderPass != ResourceId() && state.GetFramebuffer() != ResourceId())
        {
          VulkanCreationInfo::RenderPass &rp = m_CreationInfo.m_RenderPass[state.renderPass];

          RDCASSERT(state.subpass < rp.subpasses.size());

          for(uint32_t a = 0; a < attachmentCount; a++)
          {
            uint32_t att = pAttachments[a].colorAttachment;

            if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
            {
              if(att < (uint32_t)rp.subpasses[state.subpass].colorAttachments.size())
              {
                att = rp.subpasses[state.subpass].colorAttachments[att];
                actionNode.resourceUsage.push_back(make_rdcpair(
                    m_CreationInfo.m_ImageView[state.GetFramebufferAttachments()[att]].image,
                    EventUsage(actionNode.action.eventId, ResourceUsage::Clear,
                               state.GetFramebufferAttachments()[att])));
              }
            }
            else if(pAttachments[a].aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            {
              if(rp.subpasses[state.subpass].depthstencilAttachment >= 0)
              {
                att = (uint32_t)rp.subpasses[state.subpass].depthstencilAttachment;
                actionNode.resourceUsage.push_back(make_rdcpair(
                    m_CreationInfo.m_ImageView[state.GetFramebufferAttachments()[att]].image,
                    EventUsage(actionNode.action.eventId, ResourceUsage::Clear,
                               state.GetFramebufferAttachments()[att])));
              }
            }
          }
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount,
                                          const VkClearAttachment *pAttachments, uint32_t rectCount,
                                          const VkClearRect *pRects)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdClearAttachments(Unwrap(commandBuffer), attachmentCount,
                                                pAttachments, rectCount, pRects));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdClearAttachments);
    Serialise_vkCmdClearAttachments(ser, commandBuffer, attachmentCount, pAttachments, rectCount,
                                    pRects);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    // image/attachments are referenced when the render pass is started and the framebuffer is
    // bound.
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDispatchBase(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                uint32_t baseGroupX, uint32_t baseGroupY,
                                                uint32_t baseGroupZ, uint32_t groupCountX,
                                                uint32_t groupCountY, uint32_t groupCountZ)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(baseGroupX);
  SERIALISE_ELEMENT(baseGroupY);
  SERIALISE_ELEMENT(baseGroupZ);
  SERIALISE_ELEMENT(groupCountX).Important();
  SERIALISE_ELEMENT(groupCountY).Important();
  SERIALISE_ELEMENT(groupCountZ).Important();

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

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Dispatch);

        ObjDisp(commandBuffer)
            ->CmdDispatchBase(Unwrap(commandBuffer), baseGroupX, baseGroupY, baseGroupZ,
                              groupCountX, groupCountY, groupCountZ);

        if(eventId && m_ActionCallback->PostDispatch(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDispatchBase(Unwrap(commandBuffer), baseGroupX, baseGroupY, baseGroupZ,
                                groupCountX, groupCountY, groupCountZ);
          m_ActionCallback->PostRedispatch(eventId, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)
          ->CmdDispatchBase(Unwrap(commandBuffer), baseGroupX, baseGroupY, baseGroupZ, groupCountX,
                            groupCountY, groupCountZ);

      {
        AddEvent();

        ActionDescription action;
        action.dispatchDimension[0] = groupCountX;
        action.dispatchDimension[1] = groupCountY;
        action.dispatchDimension[2] = groupCountZ;
        action.dispatchBase[0] = baseGroupX;
        action.dispatchBase[1] = baseGroupY;
        action.dispatchBase[2] = baseGroupZ;

        action.flags |= ActionFlags::Dispatch;

        AddAction(action);
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX,
                                      uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX,
                                      uint32_t groupCountY, uint32_t groupCountZ)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDispatchBase(Unwrap(commandBuffer), baseGroupX, baseGroupY,
                                            baseGroupZ, groupCountX, groupCountY, groupCountZ));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDispatchBase);
    Serialise_vkCmdDispatchBase(ser, commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX,
                                groupCountY, groupCountZ);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndirectCount(SerialiserType &ser,
                                                     VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                     VkDeviceSize offset, VkBuffer countBuffer,
                                                     VkDeviceSize countBufferOffset,
                                                     uint32_t maxDrawCount, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(countBuffer).Important();
  SERIALISE_ELEMENT(countBufferOffset);
  SERIALISE_ELEMENT(maxDrawCount).Important();
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // do execution (possibly partial)
    if(IsActiveReplaying(m_State))
    {
      // this count is wrong if we're not re-recording and fetching the actual count below, but it's
      // impossible without having a particular submission in mind because without a specific
      // instance we can't know what the actual count was (it could vary between submissions).
      // Fortunately when we're not in the re-recording command buffer the EID tracking isn't
      // needed.
      uint32_t count = maxDrawCount;

      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t curEID = m_RootEventID;

        if(m_FirstEventID <= 1)
        {
          curEID = m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID;

          if(m_Partial[Primary].partialParent == m_LastCmdBufferID)
            curEID += m_Partial[Primary].baseEvent;
          else if(m_Partial[Secondary].partialParent == m_LastCmdBufferID)
            curEID += m_Partial[Secondary].baseEvent;
        }

        ActionUse use(m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

        if(it == m_ActionUses.end() || GetAction(it->eventId) == NULL)
        {
          RDCERR("Unexpected action not found in uses vector, offset %llu", m_CurChunkOffset);
        }
        else
        {
          uint32_t baseEventID = it->eventId;

          // get the number of draws by looking at how many children the parent action has.
          const rdcarray<ActionDescription> &children = GetAction(it->eventId)->children;
          count = (uint32_t)children.size();

          // don't count the popmarker child
          if(!children.empty() && children.back().flags & ActionFlags::PopMarker)
            count--;

          // when we have a callback, submit every action individually to the callback
          if(m_ActionCallback && IsDrawInRenderPass())
          {
            for(uint32_t i = 0; i < count; i++)
            {
              uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Drawcall, i + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);

              if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, 1, stride);
                m_ActionCallback->PostRedraw(eventId, commandBuffer);
              }

              offset += stride;
            }
          }
          // To add the multidraw, we made an event N that is the 'parent' marker, then
          // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
          // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
          // the first sub-draw in that range.
          else if(m_LastEventID > baseEventID)
          {
            uint32_t drawidx = 0;

            if(m_FirstEventID <= 1)
            {
              // if we're replaying part-way into a multidraw, we can replay the first part
              // 'easily'
              // by just reducing the Count parameter to however many we want to replay. This only
              // works if we're replaying from the first multidraw to the nth (n less than Count)
              count = RDCMIN(count, m_LastEventID - baseEventID);
            }
            else
            {
              // otherwise we do the 'hard' case, draw only one multidraw
              // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
              // a single draw.
              //
              // We also need to draw the same number of draws so that DrawIndex is faithful. In
              // order to preserve the draw index we write a custom indirect buffer that has zeros
              // for the parameters of all previous draws.
              drawidx = (curEID - baseEventID - 1);

              offset += stride * drawidx;

              // ensure the custom buffer is large enough
              VkDeviceSize bufLength = sizeof(VkDrawIndirectCommand) * (drawidx + 1);

              RDCASSERT(bufLength <= m_IndirectBufferSize, bufLength, m_IndirectBufferSize);

              VkBufferMemoryBarrier bufBarrier = {
                  VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                  NULL,
                  VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_QUEUE_FAMILY_IGNORED,
                  VK_QUEUE_FAMILY_IGNORED,
                  Unwrap(m_IndirectBuffer.buf),
                  0,
                  m_IndirectBufferSize,
              };

              VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                                    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

              ObjDisp(m_IndirectCommandBuffer)
                  ->BeginCommandBuffer(Unwrap(m_IndirectCommandBuffer), &beginInfo);

              // wait for any previous indirect draws to complete before filling/transferring
              DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

              // initialise to 0 so all other draws don't draw anything
              ObjDisp(m_IndirectCommandBuffer)
                  ->CmdFillBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(m_IndirectBuffer.buf), 0,
                                  m_IndirectBufferSize, 0);

              // wait for fill to complete before copy
              bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
              bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

              DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

              // copy over the actual parameter set into the right place
              VkBufferCopy region = {offset, bufLength - sizeof(VkDrawIndirectCommand),
                                     sizeof(VkDrawIndirectCommand)};
              ObjDisp(m_IndirectCommandBuffer)
                  ->CmdCopyBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(buffer),
                                  Unwrap(m_IndirectBuffer.buf), 1, &region);

              // finally wait for copy to complete before drawing from it
              bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
              bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

              DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

              ObjDisp(m_IndirectCommandBuffer)->EndCommandBuffer(Unwrap(m_IndirectCommandBuffer));

              // draw from our custom buffer
              m_IndirectDraw = true;
              buffer = m_IndirectBuffer.buf;
              offset = 0;
              count = drawidx + 1;
              stride = sizeof(VkDrawIndirectCommand);
            }

            if(IsDrawInRenderPass())
            {
              uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Drawcall, drawidx + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);

              if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count, stride);
                m_ActionCallback->PostRedraw(eventId, commandBuffer);
              }
            }
          }
        }
      }

      // multidraws skip the event ID past the whole thing
      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID += count + 1;
    }
    else
    {
      VkIndirectPatchData indirectPatch =
          FetchIndirectData(VkIndirectPatchType::DrawIndirectCount, commandBuffer, buffer, offset,
                            maxDrawCount, stride, countBuffer, countBufferOffset);

      ObjDisp(commandBuffer)
          ->CmdDrawIndirectCount(Unwrap(commandBuffer), Unwrap(buffer), offset, Unwrap(countBuffer),
                                 countBufferOffset, maxDrawCount, stride);

      // add on the size we'll need for an indirect buffer in the worst case.
      // Note that we'll only ever be partially replaying one draw at a time, so we only need the
      // worst case.
      m_IndirectBufferSize =
          RDCMAX(m_IndirectBufferSize,
                 sizeof(VkDrawIndirectCommand) + (maxDrawCount > 0 ? maxDrawCount - 1 : 0) * stride);

      rdcstr name = "vkCmdDrawIndirectCount";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      ActionDescription action;
      action.customName = name;
      action.flags = ActionFlags::MultiAction | ActionFlags::PushMarker;

      if(maxDrawCount == 0)
        action.customName = name + "(0)";

      AddEvent();
      AddAction(action);

      VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

      actionNode.indirectPatch = indirectPatch;

      actionNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      // only allocate up to one indirect sub-command to avoid pessimistic allocation if
      // maxDrawCount is very high but the actual action count is low.
      for(uint32_t i = 0; i < RDCMIN(1U, maxDrawCount); i++)
      {
        ActionDescription multi;

        multi.customName = name;

        multi.flags |= ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indirect;

        // add a fake chunk for this individual indirect action
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command"_lit);
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(buffer));
          structuriser.Serialise("buffer"_lit, bufid);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("stride"_lit, stride);
          structuriser.Serialise("command"_lit, VkDrawIndirectCommand()).Important();
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddAction(multi);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      AddEvent();
      action.customName = name + " end";
      action.flags = ActionFlags::PopMarker;
      AddAction(action);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                           VkDeviceSize offset, VkBuffer countBuffer,
                                           VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                           uint32_t stride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDrawIndirectCount(Unwrap(commandBuffer), Unwrap(buffer), offset,
                                                 Unwrap(countBuffer), countBufferOffset,
                                                 maxDrawCount, stride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirectCount);
    Serialise_vkCmdDrawIndirectCount(ser, commandBuffer, buffer, offset, countBuffer,
                                     countBufferOffset, maxDrawCount, stride);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(buffer), offset,
                                      stride * (maxDrawCount - 1) + sizeof(VkDrawIndirectCommand),
                                      eFrameRef_Read);
    record->MarkBufferFrameReferenced(GetRecord(countBuffer), countBufferOffset, 4, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirectCount(
    SerialiserType &ser, VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
    VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(buffer).Important();
  SERIALISE_ELEMENT(offset);
  SERIALISE_ELEMENT(countBuffer).Important();
  SERIALISE_ELEMENT(countBufferOffset);
  SERIALISE_ELEMENT(maxDrawCount).Important();
  SERIALISE_ELEMENT(stride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // do execution (possibly partial)
    if(IsActiveReplaying(m_State))
    {
      // this count is wrong if we're not re-recording and fetching the actual count below, but it's
      // impossible without having a particular submission in mind because without a specific
      // instance we can't know what the actual count was (it could vary between submissions).
      // Fortunately when we're not in the re-recording command buffer the EID tracking isn't
      // needed.
      uint32_t count = maxDrawCount;

      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t curEID = m_RootEventID;

        if(m_FirstEventID <= 1)
        {
          curEID = m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID;

          if(m_Partial[Primary].partialParent == m_LastCmdBufferID)
            curEID += m_Partial[Primary].baseEvent;
          else if(m_Partial[Secondary].partialParent == m_LastCmdBufferID)
            curEID += m_Partial[Secondary].baseEvent;
        }

        ActionUse use(m_CurChunkOffset, 0);
        auto it = std::lower_bound(m_ActionUses.begin(), m_ActionUses.end(), use);

        if(it == m_ActionUses.end() || GetAction(it->eventId) == NULL)
        {
          RDCERR("Unexpected action not found in uses vector, offset %llu", m_CurChunkOffset);
        }
        else
        {
          uint32_t baseEventID = it->eventId;

          // get the number of draws by looking at how many children the parent action has.
          const rdcarray<ActionDescription> &children = GetAction(it->eventId)->children;
          count = (uint32_t)children.size();

          // don't count the popmarker child
          if(!children.empty() && children.back().flags & ActionFlags::PopMarker)
            count--;

          // when we have a callback, submit every action individually to the callback
          if(m_ActionCallback && IsDrawInRenderPass())
          {
            VkMarkerRegion::Begin(
                StringFormat::Fmt("Drawcall callback replay (drawCount=%u)", count), commandBuffer);

            // first copy off the buffer segment to our indirect draw buffer
            VkBufferMemoryBarrier bufBarrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                NULL,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                Unwrap(buffer),
                offset,
                (count > 0 ? stride * (count - 1) : 0) + sizeof(VkDrawIndirectCommand),
            };

            DoPipelineBarrier(commandBuffer, 1, &bufBarrier);
            VkBufferCopy region = {offset, 0, bufBarrier.size};
            ObjDisp(commandBuffer)
                ->CmdCopyBuffer(Unwrap(commandBuffer), Unwrap(buffer), Unwrap(m_IndirectBuffer.buf),
                                1, &region);

            // wait for the copy to finish
            bufBarrier.buffer = Unwrap(m_IndirectBuffer.buf);
            bufBarrier.offset = 0;
            DoPipelineBarrier(commandBuffer, 1, &bufBarrier);

            bufBarrier.size = sizeof(VkDrawIndexedIndirectCommand);

            for(uint32_t i = 0; i < count; i++)
            {
              uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Drawcall, i + 1);

              // action up to and including i. The previous draws will be nop'd out
              ObjDisp(commandBuffer)
                  ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(m_IndirectBuffer.buf), 0,
                                           i + 1, stride);

              if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(m_IndirectBuffer.buf), 0,
                                             i + 1, stride);
                m_ActionCallback->PostRedraw(eventId, commandBuffer);
              }

              // now that we're done, nop out this draw so that the next time around we only draw
              // the next draw.
              bufBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
              bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
              DoPipelineBarrier(commandBuffer, 1, &bufBarrier);
              ObjDisp(commandBuffer)
                  ->CmdFillBuffer(Unwrap(commandBuffer), bufBarrier.buffer, bufBarrier.offset,
                                  bufBarrier.size, 0);
              bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
              bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
              DoPipelineBarrier(commandBuffer, 1, &bufBarrier);

              bufBarrier.offset += stride;
            }

            VkMarkerRegion::End(commandBuffer);
          }
          // To add the multidraw, we made an event N that is the 'parent' marker, then
          // N+1, N+2, N+3, ... for each of the sub-draws. If the first sub-draw is selected
          // then we'll replay up to N but not N+1, so just do nothing - we DON'T want to draw
          // the first sub-draw in that range.
          else if(m_LastEventID > baseEventID)
          {
            uint32_t drawidx = 0;

            if(m_FirstEventID <= 1)
            {
              // if we're replaying part-way into a multidraw, we can replay the first part
              // 'easily'
              // by just reducing the Count parameter to however many we want to replay. This only
              // works if we're replaying from the first multidraw to the nth (n less than Count)
              count = RDCMIN(count, m_LastEventID - baseEventID);
            }
            else
            {
              // otherwise we do the 'hard' case, draw only one multidraw
              // note we'll never be asked to do e.g. 3rd-7th of a multidraw. Only ever 0th-nth or
              // a single draw.
              //
              // We also need to draw the same number of draws so that DrawIndex is faithful. In
              // order to preserve the draw index we write a custom indirect buffer that has zeros
              // for the parameters of all previous draws.
              drawidx = (curEID - baseEventID - 1);

              offset += stride * drawidx;

              // ensure the custom buffer is large enough
              VkDeviceSize bufLength = sizeof(VkDrawIndexedIndirectCommand) * (drawidx + 1);

              RDCASSERT(bufLength <= m_IndirectBufferSize, bufLength, m_IndirectBufferSize);

              VkBufferMemoryBarrier bufBarrier = {
                  VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                  NULL,
                  VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_QUEUE_FAMILY_IGNORED,
                  VK_QUEUE_FAMILY_IGNORED,
                  Unwrap(m_IndirectBuffer.buf),
                  0,
                  m_IndirectBufferSize,
              };

              VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                                    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

              ObjDisp(m_IndirectCommandBuffer)
                  ->BeginCommandBuffer(Unwrap(m_IndirectCommandBuffer), &beginInfo);

              // wait for any previous indirect draws to complete before filling/transferring
              DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

              // initialise to 0 so all other draws don't draw anything
              ObjDisp(m_IndirectCommandBuffer)
                  ->CmdFillBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(m_IndirectBuffer.buf), 0,
                                  m_IndirectBufferSize, 0);

              // wait for fill to complete before copy
              bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
              bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

              DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

              // copy over the actual parameter set into the right place
              VkBufferCopy region = {offset, bufLength - sizeof(VkDrawIndexedIndirectCommand),
                                     sizeof(VkDrawIndexedIndirectCommand)};
              ObjDisp(m_IndirectCommandBuffer)
                  ->CmdCopyBuffer(Unwrap(m_IndirectCommandBuffer), Unwrap(buffer),
                                  Unwrap(m_IndirectBuffer.buf), 1, &region);

              // finally wait for copy to complete before drawing from it
              bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
              bufBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

              DoPipelineBarrier(m_IndirectCommandBuffer, 1, &bufBarrier);

              ObjDisp(m_IndirectCommandBuffer)->EndCommandBuffer(Unwrap(m_IndirectCommandBuffer));

              // draw from our custom buffer
              m_IndirectDraw = true;
              buffer = m_IndirectBuffer.buf;
              offset = 0;
              count = drawidx + 1;
              stride = sizeof(VkDrawIndexedIndirectCommand);
            }

            if(IsDrawInRenderPass())
            {
              uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Drawcall, drawidx + 1);

              ObjDisp(commandBuffer)
                  ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                           stride);

              if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
              {
                ObjDisp(commandBuffer)
                    ->CmdDrawIndexedIndirect(Unwrap(commandBuffer), Unwrap(buffer), offset, count,
                                             stride);
                m_ActionCallback->PostRedraw(eventId, commandBuffer);
              }
            }
          }
        }
      }

      // multidraws skip the event ID past the whole thing
      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID += count + 1;
    }
    else
    {
      VkIndirectPatchData indirectPatch =
          FetchIndirectData(VkIndirectPatchType::DrawIndexedIndirectCount, commandBuffer, buffer,
                            offset, maxDrawCount, stride, countBuffer, countBufferOffset);

      ObjDisp(commandBuffer)
          ->CmdDrawIndexedIndirectCount(Unwrap(commandBuffer), Unwrap(buffer), offset,
                                        Unwrap(countBuffer), countBufferOffset, maxDrawCount, stride);

      // add on the size we'll need for an indirect buffer in the worst case.
      // Note that we'll only ever be partially replaying one draw at a time, so we only need the
      // worst case.
      m_IndirectBufferSize =
          RDCMAX(m_IndirectBufferSize, sizeof(VkDrawIndexedIndirectCommand) +
                                           (maxDrawCount > 0 ? maxDrawCount - 1 : 0) * stride);

      rdcstr name = "vkCmdDrawIndexedIndirectCount";

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      SDChunk *baseChunk = m_StructuredFile->chunks.back();

      ActionDescription action;
      action.customName = name;
      action.flags = ActionFlags::MultiAction | ActionFlags::PushMarker;

      if(maxDrawCount == 0)
        action.customName = name + "(0)";

      AddEvent();
      AddAction(action);

      VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

      actionNode.indirectPatch = indirectPatch;

      actionNode.resourceUsage.push_back(make_rdcpair(
          GetResID(buffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

      m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;

      // only allocate up to one indirect sub-command to avoid pessimistic allocation if
      // maxDrawCount is very high but the actual action count is low.
      for(uint32_t i = 0; i < RDCMIN(1U, maxDrawCount); i++)
      {
        ActionDescription multi;

        multi.customName = name;

        multi.flags |= ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indexed |
                       ActionFlags::Indirect;

        // add a fake chunk for this individual indirect action
        SDChunk *fakeChunk = new SDChunk("Indirect sub-command"_lit);
        fakeChunk->metadata = baseChunk->metadata;
        fakeChunk->metadata.chunkID = (uint32_t)VulkanChunk::vkCmdIndirectSubCommand;

        {
          StructuredSerialiser structuriser(fakeChunk, ser.GetChunkLookup());

          structuriser.Serialise<uint32_t>("drawIndex"_lit, 0U);
          ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(buffer));
          structuriser.Serialise("buffer"_lit, bufid);
          structuriser.Serialise("offset"_lit, offset);
          structuriser.Serialise("stride"_lit, stride);
          structuriser.Serialise("command"_lit, VkDrawIndexedIndirectCommand()).Important();
        }

        m_StructuredFile->chunks.push_back(fakeChunk);

        AddEvent();
        AddAction(multi);

        m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
      }

      AddEvent();
      action.customName = name + " end";
      action.flags = ActionFlags::PopMarker;
      AddAction(action);
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                  VkDeviceSize offset, VkBuffer countBuffer,
                                                  VkDeviceSize countBufferOffset,
                                                  uint32_t maxDrawCount, uint32_t stride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDrawIndexedIndirectCount(Unwrap(commandBuffer), Unwrap(buffer),
                                                        offset, Unwrap(countBuffer),
                                                        countBufferOffset, maxDrawCount, stride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndexedIndirectCount);
    Serialise_vkCmdDrawIndexedIndirectCount(ser, commandBuffer, buffer, offset, countBuffer,
                                            countBufferOffset, maxDrawCount, stride);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(buffer), offset,
                                      stride * (maxDrawCount - 1) + sizeof(VkDrawIndirectCommand),
                                      eFrameRef_Read);
    record->MarkBufferFrameReferenced(GetRecord(countBuffer), countBufferOffset, 4, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdDrawIndirectByteCountEXT(
    SerialiserType &ser, VkCommandBuffer commandBuffer, uint32_t instanceCount,
    uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset,
    uint32_t counterOffset, uint32_t vertexStride)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT(instanceCount).Important();
  SERIALISE_ELEMENT(firstInstance);
  SERIALISE_ELEMENT(counterBuffer).Important();
  SERIALISE_ELEMENT(counterBufferOffset);
  SERIALISE_ELEMENT(counterOffset);
  SERIALISE_ELEMENT(vertexStride);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    // do execution (possibly partial)
    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID) && IsDrawInRenderPass())
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer);

        ObjDisp(commandBuffer)
            ->CmdDrawIndirectByteCountEXT(Unwrap(commandBuffer), instanceCount, firstInstance,
                                          Unwrap(counterBuffer), counterBufferOffset, counterOffset,
                                          vertexStride);

        if(eventId && m_ActionCallback->PostDraw(eventId, commandBuffer))
        {
          ObjDisp(commandBuffer)
              ->CmdDrawIndirectByteCountEXT(Unwrap(commandBuffer), instanceCount, firstInstance,
                                            Unwrap(counterBuffer), counterBufferOffset,
                                            counterOffset, vertexStride);
          m_ActionCallback->PostRedraw(eventId, commandBuffer);
        }
      }
    }
    else
    {
      VkIndirectPatchData indirectPatch =
          FetchIndirectData(VkIndirectPatchType::DrawIndirectByteCount, commandBuffer,
                            counterBuffer, counterBufferOffset, 1, vertexStride);
      indirectPatch.vertexoffset = counterOffset;

      ObjDisp(commandBuffer)
          ->CmdDrawIndirectByteCountEXT(Unwrap(commandBuffer), instanceCount, firstInstance,
                                        Unwrap(counterBuffer), counterBufferOffset, counterOffset,
                                        vertexStride);

      if(!IsDrawInRenderPass())
      {
        AddDebugMessage(MessageCategory::Execution, MessageSeverity::High,
                        MessageSource::IncorrectAPIUse,
                        "Drawcall in happening outside of render pass, or in secondary command "
                        "buffer without RENDER_PASS_CONTINUE_BIT");
      }

      ActionDescription action;

      AddEvent();

      action.instanceOffset = firstInstance;
      action.numInstances = instanceCount;
      action.flags = ActionFlags::Drawcall | ActionFlags::Instanced | ActionFlags::Indirect;

      AddAction(action);

      VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

      actionNode.indirectPatch = indirectPatch;

      actionNode.resourceUsage.push_back(make_rdcpair(
          GetResID(counterBuffer), EventUsage(actionNode.action.eventId, ResourceUsage::Indirect)));

      return true;
    }
  }

  return true;
}

void WrappedVulkan::vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer,
                                                  uint32_t instanceCount, uint32_t firstInstance,
                                                  VkBuffer counterBuffer,
                                                  VkDeviceSize counterBufferOffset,
                                                  uint32_t counterOffset, uint32_t vertexStride)
{
  SCOPED_DBG_SINK();

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)
                          ->CmdDrawIndirectByteCountEXT(Unwrap(commandBuffer), instanceCount,
                                                        firstInstance, Unwrap(counterBuffer),
                                                        counterBufferOffset, counterOffset,
                                                        vertexStride));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdDrawIndirectByteCountEXT);
    Serialise_vkCmdDrawIndirectByteCountEXT(ser, commandBuffer, instanceCount, firstInstance,
                                            counterBuffer, counterBufferOffset, counterOffset,
                                            vertexStride);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    record->MarkBufferFrameReferenced(GetRecord(counterBuffer), counterBufferOffset, 4,
                                      eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyBuffer2KHR(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                  const VkCopyBufferInfo2KHR *pCopyBufferInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(CopyInfo, *pCopyBufferInfo).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyBufferInfo2KHR unwrappedInfo = CopyInfo;
    unwrappedInfo.srcBuffer = Unwrap(unwrappedInfo.srcBuffer);
    unwrappedInfo.dstBuffer = Unwrap(unwrappedInfo.dstBuffer);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkCopyBufferInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)->CmdCopyBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdCopyBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdCopyBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.srcBuffer));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.dstBuffer));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcid == dstid)
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(CopyInfo.srcBuffer),
                           EventUsage(actionNode.action.eventId, ResourceUsage::Copy)));
        }
        else
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(CopyInfo.srcBuffer),
                           EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(CopyInfo.dstBuffer),
                           EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer,
                                        const VkCopyBufferInfo2KHR *pCopyBufferInfo)
{
  SCOPED_DBG_SINK();

  VkCopyBufferInfo2KHR unwrappedInfo = *pCopyBufferInfo;
  unwrappedInfo.srcBuffer = Unwrap(unwrappedInfo.srcBuffer);
  unwrappedInfo.dstBuffer = Unwrap(unwrappedInfo.dstBuffer);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkCopyBufferInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdCopyBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBuffer2KHR);
    Serialise_vkCmdCopyBuffer2KHR(ser, commandBuffer, pCopyBufferInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < pCopyBufferInfo->regionCount; i++)
    {
      record->MarkBufferFrameReferenced(GetRecord(pCopyBufferInfo->srcBuffer),
                                        pCopyBufferInfo->pRegions[i].srcOffset,
                                        pCopyBufferInfo->pRegions[i].size, eFrameRef_Read);
      record->MarkBufferFrameReferenced(GetRecord(pCopyBufferInfo->dstBuffer),
                                        pCopyBufferInfo->pRegions[i].dstOffset,
                                        pCopyBufferInfo->pRegions[i].size, eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyImage2KHR(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                 const VkCopyImageInfo2KHR *pCopyImageInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(CopyInfo, *pCopyImageInfo).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyImageInfo2KHR unwrappedInfo = CopyInfo;
    unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
    unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkCopyImageInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)->CmdCopyImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdCopyImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdCopyImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.srcImage));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.dstImage));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcid == dstid)
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(CopyInfo.srcImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::Copy)));
        }
        else
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(CopyInfo.srcImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(CopyInfo.dstImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer,
                                       const VkCopyImageInfo2KHR *pCopyImageInfo)
{
  SCOPED_DBG_SINK();

  VkCopyImageInfo2KHR unwrappedInfo = *pCopyImageInfo;
  unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
  unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkCopyImageInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdCopyImage2KHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImage2KHR);
    Serialise_vkCmdCopyImage2KHR(ser, commandBuffer, pCopyImageInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < pCopyImageInfo->regionCount; i++)
    {
      const VkImageCopy2KHR &region = pCopyImageInfo->pRegions[i];

      ImageRange srcRange(region.srcSubresource);
      srcRange.offset = region.srcOffset;
      srcRange.extent = region.extent;

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = region.dstOffset;
      dstRange.extent = region.extent;

      record->MarkImageFrameReferenced(GetRecord(pCopyImageInfo->srcImage), srcRange, eFrameRef_Read);
      record->MarkImageFrameReferenced(GetRecord(pCopyImageInfo->dstImage), dstRange,
                                       eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyBufferToImage2KHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(CopyInfo, *pCopyBufferToImageInfo);

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyBufferToImageInfo2KHR unwrappedInfo = CopyInfo;
    unwrappedInfo.srcBuffer = Unwrap(unwrappedInfo.srcBuffer);
    unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkCopyBufferToImageInfo2KHR", tempMem,
                    (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)->CmdCopyBufferToImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdCopyBufferToImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdCopyBufferToImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

      {
        AddEvent();

        ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.srcBuffer));
        ResourceId imgid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.dstImage));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = bufid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = imgid;
        action.copyDestinationSubresource = Subresource();
        if(CopyInfo.regionCount > 0)
          action.copyDestinationSubresource =
              Subresource(CopyInfo.pRegions[0].imageSubresource.mipLevel,
                          CopyInfo.pRegions[0].imageSubresource.baseArrayLayer);

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(
            make_rdcpair(GetResID(CopyInfo.srcBuffer),
                         EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
        actionNode.resourceUsage.push_back(
            make_rdcpair(GetResID(CopyInfo.dstImage),
                         EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer,
                                               const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo)
{
  SCOPED_DBG_SINK();

  VkCopyBufferToImageInfo2KHR unwrappedInfo = *pCopyBufferToImageInfo;
  unwrappedInfo.srcBuffer = Unwrap(unwrappedInfo.srcBuffer);
  unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkCopyBufferToImageInfo2KHR", tempMem,
                  (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdCopyBufferToImage2KHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyBufferToImage2KHR);
    Serialise_vkCmdCopyBufferToImage2KHR(ser, commandBuffer, pCopyBufferToImageInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    // downcast the VkBufferImageCopy2KHR to VkBufferImageCopy for ease of use, as we don't need
    // anything in the next chains here

    // we're done with temp memory above so we can reuse here
    VkBufferImageCopy *pRegions =
        GetTempArray<VkBufferImageCopy>(pCopyBufferToImageInfo->regionCount);
    for(uint32_t i = 0; i < pCopyBufferToImageInfo->regionCount; i++)
    {
      pRegions[i].bufferOffset = pCopyBufferToImageInfo->pRegions[i].bufferOffset;
      pRegions[i].bufferRowLength = pCopyBufferToImageInfo->pRegions[i].bufferRowLength;
      pRegions[i].bufferImageHeight = pCopyBufferToImageInfo->pRegions[i].bufferImageHeight;
      pRegions[i].imageSubresource = pCopyBufferToImageInfo->pRegions[i].imageSubresource;
      pRegions[i].imageOffset = pCopyBufferToImageInfo->pRegions[i].imageOffset;
      pRegions[i].imageExtent = pCopyBufferToImageInfo->pRegions[i].imageExtent;
    }

    record->MarkBufferImageCopyFrameReferenced(
        GetRecord(pCopyBufferToImageInfo->srcBuffer), GetRecord(pCopyBufferToImageInfo->dstImage),
        pCopyBufferToImageInfo->regionCount, pRegions, eFrameRef_Read, eFrameRef_CompleteWrite);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdCopyImageToBuffer2KHR(
    SerialiserType &ser, VkCommandBuffer commandBuffer,
    const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(CopyInfo, *pCopyImageToBufferInfo).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkCopyImageToBufferInfo2KHR unwrappedInfo = CopyInfo;
    unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
    unwrappedInfo.dstBuffer = Unwrap(unwrappedInfo.dstBuffer);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkCopyImageToBufferInfo2KHR", tempMem,
                    (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Copy);

        ObjDisp(commandBuffer)->CmdCopyImageToBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Copy, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdCopyImageToBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Copy, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdCopyImageToBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo);

      {
        AddEvent();

        ResourceId imgid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.srcImage));
        ResourceId bufid = GetResourceManager()->GetOriginalID(GetResID(CopyInfo.dstBuffer));

        ActionDescription action;
        action.flags |= ActionFlags::Copy;

        action.copySource = imgid;
        action.copySourceSubresource = Subresource();
        if(CopyInfo.regionCount > 0)
          action.copySourceSubresource =
              Subresource(CopyInfo.pRegions[0].imageSubresource.mipLevel,
                          CopyInfo.pRegions[0].imageSubresource.baseArrayLayer);
        action.copyDestination = bufid;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        actionNode.resourceUsage.push_back(
            make_rdcpair(GetResID(CopyInfo.srcImage),
                         EventUsage(actionNode.action.eventId, ResourceUsage::CopySrc)));
        actionNode.resourceUsage.push_back(
            make_rdcpair(GetResID(CopyInfo.dstBuffer),
                         EventUsage(actionNode.action.eventId, ResourceUsage::CopyDst)));
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer,
                                               const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo)
{
  SCOPED_DBG_SINK();

  VkCopyImageToBufferInfo2KHR unwrappedInfo = *pCopyImageToBufferInfo;
  unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
  unwrappedInfo.dstBuffer = Unwrap(unwrappedInfo.dstBuffer);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkCopyImageToBufferInfo2KHR", tempMem,
                  (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdCopyImageToBuffer2KHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdCopyImageToBuffer2KHR);
    Serialise_vkCmdCopyImageToBuffer2KHR(ser, commandBuffer, pCopyImageToBufferInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    // downcast the VkBufferImageCopy2KHR to VkBufferImageCopy for ease of use, as we don't need
    // anything in the next chains here

    // we're done with temp memory above so we can reuse here
    VkBufferImageCopy *pRegions =
        GetTempArray<VkBufferImageCopy>(pCopyImageToBufferInfo->regionCount);
    for(uint32_t i = 0; i < pCopyImageToBufferInfo->regionCount; i++)
    {
      pRegions[i].bufferOffset = pCopyImageToBufferInfo->pRegions[i].bufferOffset;
      pRegions[i].bufferRowLength = pCopyImageToBufferInfo->pRegions[i].bufferRowLength;
      pRegions[i].bufferImageHeight = pCopyImageToBufferInfo->pRegions[i].bufferImageHeight;
      pRegions[i].imageSubresource = pCopyImageToBufferInfo->pRegions[i].imageSubresource;
      pRegions[i].imageOffset = pCopyImageToBufferInfo->pRegions[i].imageOffset;
      pRegions[i].imageExtent = pCopyImageToBufferInfo->pRegions[i].imageExtent;
    }

    record->MarkBufferImageCopyFrameReferenced(
        GetRecord(pCopyImageToBufferInfo->dstBuffer), GetRecord(pCopyImageToBufferInfo->srcImage),
        pCopyImageToBufferInfo->regionCount, pRegions, eFrameRef_CompleteWrite, eFrameRef_Read);
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdBlitImage2KHR(SerialiserType &ser, VkCommandBuffer commandBuffer,
                                                 const VkBlitImageInfo2KHR *pBlitImageInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(BlitInfo, *pBlitImageInfo).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkBlitImageInfo2KHR unwrappedInfo = BlitInfo;
    unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
    unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkBlitImageInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Resolve);

        ObjDisp(commandBuffer)->CmdBlitImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdBlitImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Resolve, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdBlitImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(BlitInfo.srcImage));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(BlitInfo.dstImage));

        ActionDescription action;
        action.flags |= ActionFlags::Resolve;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcid == dstid)
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(BlitInfo.srcImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(BlitInfo.srcImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::ResolveSrc)));
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(BlitInfo.dstImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::ResolveDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer,
                                       const VkBlitImageInfo2KHR *pBlitImageInfo)
{
  SCOPED_DBG_SINK();

  VkBlitImageInfo2KHR unwrappedInfo = *pBlitImageInfo;
  unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
  unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkBlitImageInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(ObjDisp(commandBuffer)->CmdBlitImage2KHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdBlitImage2KHR);
    Serialise_vkCmdBlitImage2KHR(ser, commandBuffer, pBlitImageInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < pBlitImageInfo->regionCount; i++)
    {
      const VkImageBlit2KHR &region = pBlitImageInfo->pRegions[i];

      ImageRange srcRange(region.srcSubresource);
      srcRange.offset = {RDCMIN(region.srcOffsets[0].x, region.srcOffsets[1].x),
                         RDCMIN(region.srcOffsets[0].y, region.srcOffsets[1].y),
                         RDCMIN(region.srcOffsets[0].z, region.srcOffsets[1].z)};
      srcRange.extent = {
          (uint32_t)(RDCMAX(region.srcOffsets[0].x, region.srcOffsets[1].x) - srcRange.offset.x),
          (uint32_t)(RDCMAX(region.srcOffsets[0].y, region.srcOffsets[1].y) - srcRange.offset.y),
          (uint32_t)(RDCMAX(region.srcOffsets[0].z, region.srcOffsets[1].z) - srcRange.offset.z)};

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = {RDCMIN(region.dstOffsets[0].x, region.dstOffsets[1].x),
                         RDCMIN(region.dstOffsets[0].y, region.dstOffsets[1].y),
                         RDCMIN(region.dstOffsets[0].z, region.dstOffsets[1].z)};
      dstRange.extent = {
          (uint32_t)(RDCMAX(region.dstOffsets[0].x, region.dstOffsets[1].x) - dstRange.offset.x),
          (uint32_t)(RDCMAX(region.dstOffsets[0].y, region.dstOffsets[1].y) - dstRange.offset.y),
          (uint32_t)(RDCMAX(region.dstOffsets[0].z, region.dstOffsets[1].z) - dstRange.offset.z)};

      record->MarkImageFrameReferenced(GetRecord(pBlitImageInfo->srcImage), srcRange, eFrameRef_Read);
      record->MarkImageFrameReferenced(GetRecord(pBlitImageInfo->dstImage), dstRange,
                                       eFrameRef_CompleteWrite);
    }
  }
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_vkCmdResolveImage2KHR(SerialiserType &ser,
                                                    VkCommandBuffer commandBuffer,
                                                    const VkResolveImageInfo2KHR *pResolveImageInfo)
{
  SERIALISE_ELEMENT(commandBuffer);
  SERIALISE_ELEMENT_LOCAL(ResolveInfo, *pResolveImageInfo).Important();

  Serialise_DebugMessages(ser);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkResolveImageInfo2KHR unwrappedInfo = ResolveInfo;
    unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
    unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

    byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

    UnwrapNextChain(m_State, "VkResolveImageInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

    m_LastCmdBufferID = GetResourceManager()->GetOriginalID(GetResID(commandBuffer));

    if(IsActiveReplaying(m_State))
    {
      if(InRerecordRange(m_LastCmdBufferID))
      {
        commandBuffer = RerecordCmdBuf(m_LastCmdBufferID);

        uint32_t eventId = HandlePreCallback(commandBuffer, ActionFlags::Resolve);

        ObjDisp(commandBuffer)->CmdResolveImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

        if(eventId && m_ActionCallback->PostMisc(eventId, ActionFlags::Resolve, commandBuffer))
        {
          ObjDisp(commandBuffer)->CmdResolveImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

          m_ActionCallback->PostRemisc(eventId, ActionFlags::Resolve, commandBuffer);
        }
      }
    }
    else
    {
      ObjDisp(commandBuffer)->CmdResolveImage2KHR(Unwrap(commandBuffer), &unwrappedInfo);

      {
        AddEvent();

        ResourceId srcid = GetResourceManager()->GetOriginalID(GetResID(ResolveInfo.srcImage));
        ResourceId dstid = GetResourceManager()->GetOriginalID(GetResID(ResolveInfo.dstImage));

        ActionDescription action;
        action.flags |= ActionFlags::Resolve;

        action.copySource = srcid;
        action.copySourceSubresource = Subresource();
        action.copyDestination = dstid;
        action.copyDestinationSubresource = Subresource();

        AddAction(action);

        VulkanActionTreeNode &actionNode = GetActionStack().back()->children.back();

        if(srcid == dstid)
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(ResolveInfo.srcImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::Resolve)));
        }
        else
        {
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(ResolveInfo.srcImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::ResolveSrc)));
          actionNode.resourceUsage.push_back(
              make_rdcpair(GetResID(ResolveInfo.dstImage),
                           EventUsage(actionNode.action.eventId, ResourceUsage::ResolveDst)));
        }
      }
    }
  }

  return true;
}

void WrappedVulkan::vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer,
                                          const VkResolveImageInfo2KHR *pResolveImageInfo)
{
  SCOPED_DBG_SINK();

  VkResolveImageInfo2KHR unwrappedInfo = *pResolveImageInfo;
  unwrappedInfo.srcImage = Unwrap(unwrappedInfo.srcImage);
  unwrappedInfo.dstImage = Unwrap(unwrappedInfo.dstImage);

  byte *tempMem = GetTempMemory(GetNextPatchSize(unwrappedInfo.pNext));

  UnwrapNextChain(m_State, "VkResolveImageInfo2KHR", tempMem, (VkBaseInStructure *)&unwrappedInfo);

  SERIALISE_TIME_CALL(
      ObjDisp(commandBuffer)->CmdResolveImage2KHR(Unwrap(commandBuffer), &unwrappedInfo));

  if(IsCaptureMode(m_State))
  {
    VkResourceRecord *record = GetRecord(commandBuffer);

    CACHE_THREAD_SERIALISER();

    ser.SetActionChunk();
    SCOPED_SERIALISE_CHUNK(VulkanChunk::vkCmdResolveImage2KHR);
    Serialise_vkCmdResolveImage2KHR(ser, commandBuffer, pResolveImageInfo);

    record->AddChunk(scope.Get(&record->cmdInfo->alloc));

    for(uint32_t i = 0; i < pResolveImageInfo->regionCount; i++)
    {
      const VkImageResolve2KHR &region = pResolveImageInfo->pRegions[i];

      ImageRange srcRange(region.srcSubresource);
      srcRange.offset = region.srcOffset;
      srcRange.extent = region.extent;

      ImageRange dstRange(region.dstSubresource);
      dstRange.offset = region.dstOffset;
      dstRange.extent = region.extent;

      record->MarkImageFrameReferenced(GetRecord(pResolveImageInfo->srcImage), srcRange,
                                       eFrameRef_Read);
      record->MarkImageFrameReferenced(GetRecord(pResolveImageInfo->dstImage), dstRange,
                                       eFrameRef_CompleteWrite);
    }
  }
}

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDraw, VkCommandBuffer commandBuffer, uint32_t vertexCount,
                                uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndexed, VkCommandBuffer commandBuffer,
                                uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                                int32_t vertexOffset, uint32_t firstInstance);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndirect, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                                uint32_t stride);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirect, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                                uint32_t stride);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDispatch, VkCommandBuffer commandBuffer, uint32_t x,
                                uint32_t y, uint32_t z);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDispatchIndirect, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                                const VkBufferCopy *pRegions);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyImage, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
                                VkImageLayout dstImageLayout, uint32_t regionCount,
                                const VkImageCopy *pRegions);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBlitImage, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
                                VkImageLayout dstImageLayout, uint32_t regionCount,
                                const VkImageBlit *pRegions, VkFilter filter);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyBufferToImage, VkCommandBuffer commandBuffer,
                                VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout,
                                uint32_t regionCount, const VkBufferImageCopy *pRegions);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyImageToBuffer, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer,
                                uint32_t regionCount, const VkBufferImageCopy *pRegions);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdFillBuffer, VkCommandBuffer commandBuffer,
                                VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize fillSize,
                                uint32_t data);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdClearColorImage, VkCommandBuffer commandBuffer,
                                VkImage image, VkImageLayout imageLayout,
                                const VkClearColorValue *pColor, uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdClearDepthStencilImage, VkCommandBuffer commandBuffer,
                                VkImage image, VkImageLayout imageLayout,
                                const VkClearDepthStencilValue *pDepthStencil, uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdClearAttachments, VkCommandBuffer commandBuffer,
                                uint32_t attachmentCount, const VkClearAttachment *pAttachments,
                                uint32_t rectCount, const VkClearRect *pRects);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdResolveImage, VkCommandBuffer commandBuffer,
                                VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
                                VkImageLayout dstImageLayout, uint32_t regionCount,
                                const VkImageResolve *pRegions);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDispatchBase, VkCommandBuffer commandBuffer,
                                uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ,
                                uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndirectCount, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                uint32_t stride);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirectCount, VkCommandBuffer commandBuffer,
                                VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer,
                                VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                uint32_t stride);

INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdDrawIndirectByteCountEXT, VkCommandBuffer commandBuffer,
                                uint32_t instanceCount, uint32_t firstInstance,
                                VkBuffer counterBuffer, VkDeviceSize counterBufferOffset,
                                uint32_t counterOffset, uint32_t vertexStride);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyBuffer2KHR, VkCommandBuffer commandBuffer,
                                const VkCopyBufferInfo2KHR *pCopyBufferInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyImage2KHR, VkCommandBuffer commandBuffer,
                                const VkCopyImageInfo2KHR *pCopyImageInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyBufferToImage2KHR, VkCommandBuffer commandBuffer,
                                const VkCopyBufferToImageInfo2KHR *pCopyBufferToImageInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdCopyImageToBuffer2KHR, VkCommandBuffer commandBuffer,
                                const VkCopyImageToBufferInfo2KHR *pCopyImageToBufferInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdBlitImage2KHR, VkCommandBuffer commandBuffer,
                                const VkBlitImageInfo2KHR *pBlitImageInfo);
INSTANTIATE_FUNCTION_SERIALISED(void, vkCmdResolveImage2KHR, VkCommandBuffer commandBuffer,
                                const VkResolveImageInfo2KHR *pResolveImageInfo);
